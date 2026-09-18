"""Bounded external READ-ONLY research capture; never certifies a layout or enables selling.

Uses the old layout as a hypothesis with a freshly discovered singleton anchor.
Requests PROCESS_VM_READ and PROCESS_QUERY_LIMITED_INFORMATION only.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
import struct
from discover_inventory_anchor import discover, PATTERN


def capture(pid, base, executable):
    anchor = discover(executable)
    if len(anchor["matches"]) != 1:
        raise ValueError("Need exactly one anchor match")
    kernel = c.WinDLL("kernel32", use_last_error=True)
    kernel.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]
    kernel.OpenProcess.restype = w.HANDLE
    kernel.CloseHandle.argtypes = [w.HANDLE]
    kernel.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
    kernel.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
    handle = kernel.OpenProcess(0x0010 | 0x1000, False, pid)
    if not handle:
        raise c.WinError(c.get_last_error())
    try:
        name, length = c.create_unicode_buffer(32768), w.DWORD(32768)
        if not kernel.QueryFullProcessImageNameW(handle, 0, name, c.byref(length)):
            raise c.WinError(c.get_last_error())
        if Path(name.value).resolve() != Path(executable).resolve():
            raise ValueError("Process executable does not match scanned file")

        def read(address, size):
            if address < 0x10000 or not 0 < size <= 0x20000:
                raise ValueError("Read outside bounded capture limits")
            buffer, actual = c.create_string_buffer(size), c.c_size_t()
            if not kernel.ReadProcessMemory(handle, address, buffer, size, c.byref(actual)) or actual.value != size:
                raise c.WinError(c.get_last_error())
            return buffer.raw

        def u64(address):
            return struct.unpack("<Q", read(address, 8))[0]

        match = anchor["matches"][0]
        instruction = base + int(match["instruction_rva"], 16)
        code = read(instruction, len(PATTERN.split()))
        if any(token != "??" and byte != int(token, 16) for token, byte in zip(PATTERN.split(), code)):
            raise ValueError("Live instruction does not match file signature")
        target = instruction + 7 + struct.unpack_from("<i", code, 3)[0]
        if target - base != int(match["target_rva"], 16):
            raise ValueError("Live RIP target differs from scanned file")
        manager = u64(target)
        player = u64(manager + 8)
        header = read(player + 0x400, 0x40)
        global_capacity = struct.unpack_from("<I", header, 0x10)[0]
        capacity = struct.unpack_from("<I", header, 0x14)[0]
        array = struct.unpack_from("<Q", header, 0x18)[0]
        count = struct.unpack_from("<I", header, 0x20)[0]
        if not 1 <= capacity <= global_capacity <= 4096 or count > capacity:
            raise ValueError(f"Implausible candidate container: count={count} capacity={capacity}")
        block = read(array, capacity * 0x18)
        if header != read(player + 0x400, 0x40) or block != read(array, capacity * 0x18):
            raise ValueError("Inventory changed during capture; retry while idle")
        rows = [dict(zip(("handle", "item_id", "quantity", "display_index", "unknown_10", "unknown_14"),
                         struct.unpack_from("<6I", block, i * 0x18)), index=i)
                for i in range(capacity)]
        controller = u64(player + 0x5D0)
        return {"status": "candidate_layout_only", "anchor": anchor, "pid": pid,
                "player": hex(player), "controller": hex(controller),
                "controller_header": read(controller, 0x90).hex(),
                "equipment_indices": list(struct.unpack("<22i", read(player + 0x2B8, 88))),
                "equipment_probe_words_354": list(struct.unpack("<22I", read(player + 0x354, 88))),
                "runes_candidate": struct.unpack("<I", read(player + 0x6C, 4))[0],
                "count": count, "capacity": capacity, "global_capacity": global_capacity,
                "header_u32": list(struct.unpack("<16I", header)), "rows": rows}
    finally:
        kernel.CloseHandle(handle)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--pid", type=int, required=True)
    parser.add_argument("--base", type=lambda s: int(s, 0), required=True)
    parser.add_argument("--executable", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    result = capture(args.pid, args.base, args.executable)
    Path(args.output).write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    populated = [r for r in result["rows"] if r["handle"] not in (0, 0xFFFFFFFF) and r["item_id"] != 0xFFFFFFFF and r["quantity"] > 0]
    print(json.dumps({k: v for k, v in result.items() if k not in ("rows", "anchor")}, indent=2))
    print(f"Populated candidate rows: {len(populated)}; output: {args.output}")
    print(json.dumps(populated[:8], indent=2))
