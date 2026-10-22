"""Exact owned-RAM native capability/mode replay and timestamp write footprint.

Synthetic input values do not qualify physical clocks, cache layout or engines.
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
libc = C.CDLL(None)
libc.mprotect.argtypes = [C.c_void_p, C.c_size_t, C.c_int]
libc.mprotect.restype = C.c_int
info = C.CFUNCTYPE(None, C.c_void_p)(base+0x2847e)
modes = C.CFUNCTYPE(None, C.c_void_p)(base+0x28e1a)
registers = mmap.mmap(-1, 0x1000000, prot=3)
regbase = C.addressof(C.c_char.from_buffer(registers))
offsets = (0xd00, 0x145948, 0x145994, 0x229c, 0x7008, 0x7000, 0x7004,
           0x9138, 0x913c, 0x9134, 0x911c, 0x9120, 0x9118, 0x9140)
results = []
for crystal in range(4):
    for shift in range(4):
        owner = C.create_string_buffer(0x2000+128)
        C.memset(owner, 0xa5, len(owner))
        pointer = C.addressof(owner)+64
        C.memset(pointer, 0, 0x2000)
        struct.pack_into('<I', owner, 64+0x1120, 2)
        struct.pack_into('<Q', owner, 64+0x1240, regbase)
        struct.pack_into('<Q', owner, 64+0x1300, 5)
        before = C.string_at(pointer, 0x2000)
        registers[:] = b'\0'*len(registers)
        for off in offsets:
            struct.pack_into('<I', registers, off, off ^ 0x40000000)
        for off, value in ((0x9138, 1), (0x913c, 0x3f), (0x9134, 0),
                           (0x9118, 0), (0x9140, 0xe00fa),
                           (0xd00, (crystal << 3) | (shift << 1))):
            struct.pack_into('<I', registers, off, value)
        snapshot = registers[:]
        digest = hashlib.sha256(registers).digest()
        struct.pack_into('<Q', image, 0x14a5b8, 0x12345678)
        assert libc.mprotect(regbase, len(registers), mmap.PROT_READ) == 0
        info(pointer)
        assert hashlib.sha256(registers).digest() == digest
        frequency = (24000000, 19200000, 38400000, 25000000)[crystal] >> (3-shift)
        assert C.c_uint32.from_address(pointer+0x1188).value == frequency
        decoded = struct.unpack_from('<Q', image, 0x14a5b8)[0]
        assert decoded == ((frequency >> 5)*0xa7c5ac5) >> 39
        struct.pack_into('<Q', image, 0x14a5b8, 0x12345678)
        assert struct.unpack_from('<Q', image, 0x14a5b8)[0] == 0x12345678
        for off, value in ((0x1124, 96), (0x1158, 12), (0x115c, 1),
                           (0x1164, 8), (0x1140, 768)):
            assert C.c_uint32.from_address(pointer+off).value == value
        assert C.c_uint64.from_address(pointer+0x1300).value == 5
        after = C.string_at(pointer, 0x2000)
        for i, (old, new) in enumerate(zip(before, after)):
            assert old == new or 0x1124 <= i < 0x1198 or 0xdd8 <= i < 0xde0 or 0x1300 <= i < 0x1308
        assert libc.mprotect(regbase, len(registers), 3) == 0
        modes(pointer)
        expected = bytearray(snapshot)
        for off, value in ((0x229c, 0x80000), (0x2229c, 0x80000), (0x14800, 0x10000)):
            struct.pack_into('<I', expected, off, value)
        assert registers[:] == expected
        assert C.string_at(pointer, 0x2000) == after
        assert owner.raw[:64] == b'\xa5'*64 and owner.raw[-64:] == b'\xa5'*64
        results.append(frequency)
registers.close()
image.close()
print('PASS exact native caps/mode RAM replay: 16 crystal/shift combinations, 14-register input footprint, full 16MiB read-only/content validation, bounded owner writes, timestamp restoration and render/blitter mode stores; no hardware access')
