"""Read-only PE signature discovery. Does not change DLL configuration or game memory."""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path


PATTERN = "48 8B 05 ?? ?? ?? ?? 48 85 C0 74 05 48 8B 40 58 C3 C3"


def discover(path):
    data = Path(path).read_bytes()
    if data[:2] != b"MZ":
        raise ValueError("Not a PE executable")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("Invalid PE signature")
    machine, sections = struct.unpack_from("<HH", data, pe + 4)
    optional_size = struct.unpack_from("<H", data, pe + 20)[0]
    if machine != 0x8664 or struct.unpack_from("<H", data, pe + 24)[0] != 0x20B:
        raise ValueError("Expected x64 PE32+")
    image_size = struct.unpack_from("<I", data, pe + 24 + 56)[0]
    table = pe + 24 + optional_size
    regex = re.compile(b"".join(b"." if token == "??" else re.escape(bytes([int(token, 16)]))
                                for token in PATTERN.split()), re.DOTALL)
    matches = []
    for i in range(sections):
        off = table + i * 40
        name = data[off:off + 8].split(b"\0")[0].decode("ascii")
        virtual_size, rva, raw_size, raw = struct.unpack_from("<IIII", data, off + 8)
        flags = struct.unpack_from("<I", data, off + 36)[0]
        if not flags & 0x20000000:
            continue
        if raw + raw_size > len(data):
            raise ValueError("Section exceeds file")
        for match in regex.finditer(data[raw:raw + raw_size]):
            instruction = rva + match.start()
            displacement = struct.unpack_from("<i", data, raw + match.start() + 3)[0]
            target = instruction + 7 + displacement
            if not 0 <= target <= image_size - 8:
                raise ValueError("RIP target outside image")
            matches.append({"section": name, "instruction_rva": hex(instruction),
                            "target_rva": hex(target)})
    return {"executable": str(Path(path).resolve()), "sha256": hashlib.sha256(data).hexdigest(),
            "pattern": PATTERN, "image_size": image_size, "matches": matches,
            "status": "candidate_only", "inventory_layout_validated": False}


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("executable")
    parser.add_argument("--output")
    args = parser.parse_args()
    result = discover(args.executable)
    output = json.dumps(result, indent=2)
    if args.output:
        Path(args.output).write_text(output + "\n", encoding="utf-8")
    print(output)
    raise SystemExit(0 if len(result["matches"]) == 1 else 1)
