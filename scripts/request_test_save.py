"""Request the game's own save operation on the exact validated 2.7.1.0 test build.

Writes ONLY GameMan's one-byte save-request flag, not inventory or save-file data.
Reference: TGA Save Request.cea; confirmed by current executable at RVA 0x67b349.
"""
import argparse
import ctypes as c
from ctypes import wintypes as w
import hashlib
from pathlib import Path
import struct

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--pid', type=int, required=True)
parser.add_argument('--base', type=lambda s: int(s, 0), required=True)
parser.add_argument('--executable', required=True)
args = parser.parse_args()
assert hashlib.sha256(Path(args.executable).read_bytes()).hexdigest() == '1a3547101327f65d0c76da2f9190ac0aa66871ea42bae2aecc61e11a8b597891'
k = c.WinDLL('kernel32', use_last_error=True)
k.OpenProcess.argtypes = [w.DWORD, w.BOOL, w.DWORD]; k.OpenProcess.restype = w.HANDLE
k.CloseHandle.argtypes = [w.HANDLE]
k.QueryFullProcessImageNameW.argtypes = [w.HANDLE, w.DWORD, w.LPWSTR, c.POINTER(w.DWORD)]
k.ReadProcessMemory.argtypes = [w.HANDLE, c.c_void_p, c.c_void_p, c.c_size_t, c.POINTER(c.c_size_t)]
k.WriteProcessMemory.argtypes = k.ReadProcessMemory.argtypes
h = k.OpenProcess(0x1000 | 0x10 | 0x20 | 0x08, False, args.pid)
if not h: raise c.WinError(c.get_last_error())
try:
    name, length = c.create_unicode_buffer(32768), w.DWORD(32768)
    assert k.QueryFullProcessImageNameW(h, 0, name, c.byref(length))
    assert Path(name.value).resolve() == Path(args.executable).resolve()
    def read(address, size):
        buf, actual = c.create_string_buffer(size), c.c_size_t()
        assert k.ReadProcessMemory(h, address, buf, size, c.byref(actual)) and actual.value == size
        return buf.raw
    def ptr(address): return struct.unpack('<Q', read(address, 8))[0]
    assert read(args.base + 0x67aba0, 18) == bytes.fromhex('48 8b 05 e1 2d 6f 03 80 b8 88 0d 00 00 0d 0f 94 c0 c3')
    game = ptr(args.base + 0x3d6d988)
    assert game > 0x10000
    slot = struct.unpack('<i', read(game + 0xac0, 4))[0]
    assert 0 <= slot < 10, f'Invalid active save slot {slot}'
    assert read(game + 0xb80, 1) == b'\0', 'Save is busy'
    assert read(game + 0xb72, 1) in (b'\0', b'\1')
    actual, flag = c.c_size_t(), c.c_ubyte(1)
    assert k.WriteProcessMemory(h, game + 0xb72, c.byref(flag), 1, c.byref(actual)) and actual.value == 1
    print(f'Requested game-owned save for slot {slot}; verify file write and reload independently.')
finally:
    k.CloseHandle(h)
