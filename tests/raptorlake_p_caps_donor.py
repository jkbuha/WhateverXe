"""Replay native capability decoding on read-only synthetic registers, CPU only.

The exact getGPUInfo/initHardwareCaps bodies run with forcewake and logging
stubbed. This verifies donor arithmetic, not hardware register accessibility,
cache sizing, clocks, media engines or runtime compatibility.
"""
import ctypes as C
import hashlib
import mmap
import platform
import struct
import sys
from pathlib import Path

assert platform.machine().lower() in ('x86_64', 'amd64')
data = Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() in (
    'bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0',
    '6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64')
assert data[0x2847e:0x2848f] == bytes.fromhex('554889e54157415641554154534883ec18')
assert data[0x2a666:0x2a670] == bytes.fromhex('554889e5415741565350')

image = mmap.mmap(-1, 0x200000, prot=mmap.PROT_READ | mmap.PROT_WRITE | mmap.PROT_EXEC)
image[:len(data)] = data
# Replace exactly the two explicit forcewake calls and imported log calls;
# preserve the native initHardwareCaps call and all arithmetic/branches.
for site in (0x2849c, 0x28521):
    assert data[site:site+1] == b'\xe8'
    target = site + 5 + struct.unpack_from('<i', data, site+1)[0]
    assert target == 0x28c2a
    image[site:site+5] = b'\x90' * 5
logs = (0x28536, 0x2854b, 0x28575, 0x2858c, 0x2863b, 0x2864f,
        0x28672, 0x28685, 0x286bf, 0x286d9, 0x2870a, 0x28727,
        0x287c0, 0x287d8, 0x2880a, 0x28822, 0x28833, 0x2883d,
        0x288a2, 0x288b3, 0x288e6, 0x2a7cc, 0x2a7dc)
for site in logs:
    assert data[site:site+5] == b'\xe8\0\0\0\0', hex(site)
    image[site:site+5] = b'\x90' * 5
# The tail log in getGPUInfo's failure branch is also an unresolved import.
assert data[0x28905:0x2890a] == b'\xe9\0\0\0\0'
image[0x28905:0x2890a] = b'\xc3' + b'\x90' * 4
base = C.addressof(C.c_char.from_buffer(image))
decode = C.CFUNCTYPE(None, C.c_void_p)(base + 0x2847e)
caps = C.CFUNCTYPE(C.c_bool, C.c_void_p)(base + 0x2a666)
libc = C.CDLL(None)
libc.mprotect.argtypes = [C.c_void_p, C.c_size_t, C.c_int]
libc.mprotect.restype = C.c_int
results = []
for dss, disabled, subslices, eus in ((0x3f, 0, 12, 96), (0x3f, 3, 12, 72)):
    registers = mmap.mmap(-1, 0x160000, prot=mmap.PROT_READ | mmap.PROT_WRITE)
    regbase = C.addressof(C.c_char.from_buffer(registers))
    for offset, value in ((0x9138, 1), (0x913c, dss), (0x9134, disabled),
                          (0x9140, 0xe00fa)):
        struct.pack_into('<I', registers, offset, value)
    before = hashlib.sha256(registers).digest()
    assert libc.mprotect(regbase, len(registers), mmap.PROT_READ) == 0
    obj = C.create_string_buffer(0x2000)
    struct.pack_into('<I', obj, 0x1120, 2)
    struct.pack_into('<Q', obj, 0x1240, regbase)
    struct.pack_into('<Q', obj, 0x1300, 0x3f)
    decode(obj)
    def field(offset): return struct.unpack_from('<I', obj, offset)[0]
    assert field(0x1124) == eus
    assert field(0x1158) == subslices and field(0x115c) == 1
    assert field(0x116c) == 8 - disabled.bit_count()
    assert field(0x1164) == 8  # Donor constant, not a measured RPL cache size.
    assert field(0x114c) == 6 and field(0x1128) == 12
    assert field(0x1144) == 0 and field(0x1140) == 768
    assert hashlib.sha256(registers).digest() == before
    results.append(dict(dss=dss, eu_disable=disabled, eus=eus, native_subslices=subslices))
    # No escaped buffer exports remain; undo read-only protection before close.
    assert libc.mprotect(regbase, len(registers), mmap.PROT_READ | mmap.PROT_WRITE) == 0
    registers.close()
for selector in (0, 3, 0xffffffff):
    obj = C.create_string_buffer(0x2000)
    struct.pack_into('<I', obj, 0x1120, selector)
    assert not caps(obj)
image.close()
print('PASS exact native GPU capability arithmetic on read-only synthetic registers:', results)
