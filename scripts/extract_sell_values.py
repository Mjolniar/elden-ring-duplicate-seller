#!/usr/bin/env python3
"""Extract category-qualified Elden Ring sell values from regulation.bin.

This is an offline helper for dry-run reporting. It decrypts the regulation
with Soulstruct's ParamCrypt, reads only the target EquipParam entries, and
uses local ParamDef XML files to locate each row's sellValue field.
"""

from __future__ import annotations

import argparse
import importlib
import re
import struct
import sys
import tempfile
import types
import xml.etree.ElementTree as ET
from pathlib import Path


PARAM_TABLES = (
    ("EquipParamWeapon", "weapon"),
    ("EquipParamProtector", "armor"),
    ("EquipParamAccessory", "talisman"),
    ("EquipParamGoods", "consumable"),
    ("EquipParamGem", "ash_of_war"),
)

TYPE_BITS = {
    "u8": 8,
    "s8": 8,
    "dummy8": 8,
    "u16": 16,
    "s16": 16,
    "dummy16": 16,
    "u32": 32,
    "s32": 32,
    "f32": 32,
    "dummy32": 32,
    "u64": 64,
    "s64": 64,
    "f64": 64,
}


def flag(byte_value: int, bit: int) -> bool:
    return bool(byte_value & (1 << bit))


def install_eldenring_import_shim() -> None:
    """Avoid importing soulstruct.eldenring.__init__, which loads unrelated data."""
    import soulstruct

    if getattr(soulstruct, "__file__", None):
        soulstruct_root = Path(soulstruct.__file__).resolve().parent
    else:
        package_paths = list(getattr(soulstruct, "__path__", []))
        if not package_paths:
            raise RuntimeError("Could not resolve installed soulstruct package path")
        soulstruct_root = Path(package_paths[0]).resolve()
    eldenring_path = soulstruct_root / "eldenring"
    params_path = eldenring_path / "params"

    eldenring_pkg = types.ModuleType("soulstruct.eldenring")
    eldenring_pkg.__path__ = [str(eldenring_path)]
    sys.modules["soulstruct.eldenring"] = eldenring_pkg

    params_pkg = types.ModuleType("soulstruct.eldenring.params")
    params_pkg.__path__ = [str(params_path)]
    sys.modules["soulstruct.eldenring.params"] = params_pkg


def load_soulstruct_helpers():
    install_eldenring_import_shim()
    paramcrypt_module = importlib.import_module("soulstruct.base.params.ParamCrypt")
    containers_module = importlib.import_module("soulstruct.containers")
    return paramcrypt_module.ParamCrypt, containers_module.Binder


def field_definition_bits(definition: str) -> tuple[str, str, int, bool]:
    left = definition.split("=", 1)[0].strip()
    match = re.match(r"([A-Za-z0-9]+)\s+([^\s:[]+)(?:\[(\d+)\])?(?::(\d+))?", left)
    if not match:
        raise ValueError(f"Unsupported field definition: {definition}")

    field_type, field_name, array_count, bit_count = match.groups()
    if field_type not in TYPE_BITS:
        raise ValueError(f"Unsupported field type {field_type} in {definition}")

    if bit_count:
        return field_name, field_type, int(bit_count), True

    return field_name, field_type, TYPE_BITS[field_type] * int(array_count or "1"), False


def sell_value_offset(paramdef_xml: Path) -> int:
    bit_offset = 0
    active_bit_base = None
    active_bit_used = 0

    fields = ET.parse(paramdef_xml).getroot().find("Fields")
    if fields is None:
        raise ValueError(f"No Fields element in {paramdef_xml}")

    for field in fields.findall("Field"):
        field_name, field_type, bit_count, is_bitfield = field_definition_bits(field.attrib["Def"])

        if is_bitfield:
            base_bits = TYPE_BITS[field_type]
            if active_bit_base != base_bits or active_bit_used + bit_count > base_bits:
                if active_bit_base is not None:
                    bit_offset += active_bit_base - active_bit_used
                active_bit_base = base_bits
                active_bit_used = 0

            active_bit_used += bit_count
            if active_bit_used == active_bit_base:
                bit_offset += active_bit_base
                active_bit_base = None
                active_bit_used = 0
            continue

        if active_bit_base is not None:
            bit_offset += active_bit_base - active_bit_used
            active_bit_base = None
            active_bit_used = 0

        if field_name == "sellValue":
            if bit_offset % 8 != 0:
                raise ValueError(f"sellValue is not byte-aligned in {paramdef_xml}")
            return bit_offset // 8

        bit_offset += bit_count

    raise ValueError(f"No sellValue field in {paramdef_xml}")


def parse_param_rows(param_data: bytes) -> tuple[str, list[tuple[int, int]], str]:
    endian = ">" if param_data[0x2C] == 0xFF else "<"
    flags1 = param_data[0x2D]
    row_count = struct.unpack_from(endian + "H", param_data, 0x0A)[0]

    position = 0x0C
    if flag(flags1, 7):
        param_type_offset = struct.unpack_from(endian + "q", param_data, position + 4)[0]
        end = param_data.find(b"\0", param_type_offset)
        param_type = param_data[param_type_offset:end].decode("ascii", errors="replace")
        position += 32
    else:
        raw_param_type = param_data[position:position + 32]
        param_type = raw_param_type.split(b"\0", 1)[0].decode("ascii", errors="replace")
        position += 32

    position += 4
    if flag(flags1, 0) and flag(flags1, 1):
        position += 16
    elif flag(flags1, 2):
        position += 16

    rows: list[tuple[int, int]] = []
    long_offsets = flag(flags1, 2)
    for _ in range(row_count):
        if long_offsets:
            row_id, _unknown, data_offset, _name_offset = struct.unpack_from(endian + "iiqq", param_data, position)
            position += 24
        else:
            row_id, data_offset, _name_offset = struct.unpack_from(endian + "iII", param_data, position)
            position += 12
        rows.append((row_id, data_offset))

    return endian, rows, param_type


def decrypt_regulation(regulation_bin: Path) -> bytes:
    ParamCrypt, _Binder = load_soulstruct_helpers()
    with tempfile.TemporaryDirectory(prefix="erdup_paramcrypt_") as temp_dir:
        decrypted_path = Path(temp_dir) / "regulation.parambnd.dcx"
        ParamCrypt(regulation_bin, "decrypt", "er", decrypted_path)
        return decrypted_path.read_bytes()


def extract_values(regulation_bin: Path, param_defs_dir: Path) -> tuple[list[str], list[str]]:
    _ParamCrypt, Binder = load_soulstruct_helpers()
    binder = Binder.from_bytes(decrypt_regulation(regulation_bin))
    entries_by_name = {entry.name: entry for entry in binder.entries}

    rows_out: list[str] = []
    summary: list[str] = []
    for param_name, category in PARAM_TABLES:
        entry_name = f"{param_name}.param"
        if entry_name not in entries_by_name:
            raise ValueError(f"Missing {entry_name} in regulation binder")

        offset = sell_value_offset(param_defs_dir / f"{param_name}.xml")
        param_data = entries_by_name[entry_name].get_uncompressed_data()
        endian, rows, param_type = parse_param_rows(param_data)

        written = 0
        skipped_negative = 0
        for row_id, data_offset in rows:
            value_offset = data_offset + offset
            if value_offset + 4 > len(param_data):
                raise ValueError(f"{entry_name} row {row_id} sellValue is outside row data")
            sell_value = struct.unpack_from(endian + "i", param_data, value_offset)[0]
            if sell_value < 0:
                skipped_negative += 1
            value_token = 'unsellable' if sell_value < 0 else str(sell_value)
            rows_out.append(f"{category} {row_id} {value_token}")
            written += 1

        summary.append(
            f"{param_name}: param_type={param_type} category={category} rows={len(rows)} "
            f"written={written} marked_unsellable={skipped_negative} sell_value_offset={offset}"
        )

    rows_out.sort()
    return rows_out, summary


def run_self_test() -> None:
    sample = """<?xml version="1.0" encoding="utf-8"?>
<PARAMDEF XmlVersion="2">
  <Fields>
    <Field Def="u8 flagsA:1" />
    <Field Def="dummy8 flagsB:7" />
    <Field Def="dummy8 padding[3]" />
    <Field Def="s32 fixPrice" />
    <Field Def="s32 sellValue" />
  </Fields>
</PARAMDEF>
"""
    with tempfile.TemporaryDirectory(prefix="erdup_sell_value_selftest_") as temp_dir:
        path = Path(temp_dir) / "Sample.xml"
        path.write_text(sample, encoding="utf-8")
        offset = sell_value_offset(path)
    if offset != 8:
        raise AssertionError(f"Expected sample sellValue offset 8, got {offset}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--regulation-bin", type=Path, required=False)
    parser.add_argument("--param-defs-dir", type=Path, required=False)
    parser.add_argument("--output", type=Path, required=False)
    parser.add_argument("--self-test", action="store_true")
    args = parser.parse_args()

    if args.self_test:
        run_self_test()
        print("extract_sell_values.py self-test passed")
        return 0

    if args.regulation_bin is None or args.param_defs_dir is None or args.output is None:
        parser.error("--regulation-bin, --param-defs-dir, and --output are required unless --self-test is used")

    regulation_bin = args.regulation_bin.resolve()
    param_defs_dir = args.param_defs_dir.resolve()
    output = args.output.resolve()

    if not regulation_bin.is_file():
        raise FileNotFoundError(f"regulation.bin not found: {regulation_bin}")
    if not param_defs_dir.is_dir():
        raise FileNotFoundError(f"Param defs directory not found: {param_defs_dir}")

    rows, summary = extract_values(regulation_bin, param_defs_dir)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        "\n".join(
            [
                "# ERDuplicateSeller category-qualified sell values",
                f"# source_regulation={regulation_bin}",
                f"# param_defs_dir={param_defs_dir}",
                "# format: category item_id sell_value",
                *[f"# {line}" for line in summary],
                *rows,
                "",
            ]
        ),
        encoding="utf-8",
    )

    print(f"Wrote {len(rows)} sell-value rows to {output}")
    for line in summary:
        print(line)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
