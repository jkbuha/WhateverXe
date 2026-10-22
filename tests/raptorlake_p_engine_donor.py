"""Replay exact native engine-table helpers on owned RAM, never GPU registers.

The workaround generator is a TGL policy, not an RPL policy qualification.
Mapped-buffer GPU addresses are mocked; no allocation/submission is admitted.
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
for offset, prefix in ((0x7b6b6, '554889e50fb787bc000000'),
                       (0x28e1a, '554889e5488b8700130000'),
                       (0x28f34, '554889e541574156415453')):
    assert data[offset:offset+len(bytes.fromhex(prefix))].hex() == prefix
image = mmap.mmap(-1, 0x200000, prot=7)
image[:len(data)] = data
base = C.addressof(C.c_char.from_buffer(image))
wa = C.CFUNCTYPE(None, C.c_void_p, C.c_void_p, C.c_uint32, C.c_uint16, C.c_uint16)(base+0x7b6b6)
# Exact descriptor ABI: entry 1 is CCS (compute), not BCS.  Render+blitter mask is 5.
hws = (0x2080, 0x1a080, 0x22080, 0x1c0080, 0x1d0080, 0x1c8080)
mode = (0x229c, 0x1a29c, 0x2229c, 0x1c029c, 0x1d029c, 0x1c829c)
names = (b'RCS', b'CCS', b'BCS', b'VCS0', b'VCS2', b'VECS0')
for i in range(6):
    descriptor = 0xd1c60+0x79*i
    assert struct.unpack_from('<II', data, descriptor+0x35) == (hws[i], mode[i])
    pointer = struct.unpack_from('<Q', data, descriptor+5)[0]
    assert data[pointer:pointer+len(names[i])+1] == names[i]+b'\0'
# Generator ignores device ID and SKU: an equal numeric stepping does not
# establish that TGL workarounds apply to RPL. Verify its full write footprint.
wa_cases = 0
for identity in (0x9a498086, 0xa7a08086):
    for revision in (0, 3, 4, 0x8000):
        for seed in (0, 0x55555555555555, 0xaaaaaaaaaaaaaa):
            buf = C.create_string_buffer(0xc0+32)
            C.memset(buf, 0xa5, len(buf))
            struct.pack_into('<Q', buf, 0xb8, seed | (0xa5 << 56))
            sku = C.create_string_buffer(b'\xa5'*64)
            before = buf.raw
            sku_before = sku.raw
            wa(buf, sku, identity, revision, 0xffff)
            original = seed & 0xfe002000001bff
            stepping = ((~revision & 0xffff) >> 15)
            flags = (0x1fddfaddfec00 if revision < 4 else 0x1f8c5fec00)
            flags |= sum(stepping << bit for bit in (21, 25, 41, 28, 30))
            expected = bytearray(before)
            expected[0xb8:0xbf] = ((original | flags) & ((1 << 56)-1)).to_bytes(7, 'little')
            assert buf.raw == expected and sku.raw == sku_before
            wa_cases += 1

addresses = {}
seen = []
@C.CFUNCTYPE(C.c_uint64, C.c_void_p)
def gpu_address(pointer):
    seen.append(pointer)
    return addresses.get(pointer, 0)
# Mock only the IG mapped-buffer address accessor. Execute both native loops,
# descriptor lookups, feature branches and all RAM register stores unchanged.
image[0x13e7c:0x13e7c+14] = bytes.fromhex('ff2500000000') + struct.pack('<Q', C.cast(gpu_address, C.c_void_p).value)
init_mode = C.CFUNCTYPE(None, C.c_void_p)(base+0x28e1a)
init_hws = C.CFUNCTYPE(None, C.c_void_p)(base+0x28f34)
size = 0x200000
guard = 4096
ram = C.create_string_buffer(size+2*guard)
shadow = C.addressof(ram)+guard
owner = C.create_string_buffer(0x2000)
objects = [C.create_string_buffer(64) for _ in range(7)]
for i, obj in enumerate(objects):
    addresses[C.addressof(obj)] = 0x400000+i*0x1000
cases = 0
for mask in (0, 1, 4, 5, 0x3f):
    for features in (0, 1 << 33, 1 << 8, (1 << 8) | (1 << 33)):
        for extra in (False, True):
            C.memset(ram, 0xa5, len(ram))
            C.memset(owner, 0, len(owner))
            struct.pack_into('<Q', owner, 0x1240, shadow)
            struct.pack_into('<Q', owner, 0x1300, mask)
            struct.pack_into('<Q', owner, 0x1190, features)
            struct.pack_into('<I', owner, 0xfe6, (1 << 28) | (1 << 25) if extra else 0)
            for i, obj in enumerate(objects):
                struct.pack_into('<Q', owner, 0x1408+i*8, C.addressof(obj))
            before = owner.raw
            init_mode(owner)
            expected = bytearray(b'\xa5'*size)
            for i in range(6):
                if mask & (1 << i):
                    struct.pack_into('<I', expected, mode[i], 0x80000)
            struct.pack_into('<I', expected, 0x14800, (0xa5a5a5a5 & 0xfffefffe)+0x10000+bool(features & (1 << 33)))
            if extra:
                struct.pack_into('<I', expected, 0xce90, 0xa5a5a5a5 | 0x10001)
            if features & (1 << 8):
                for off, value in ((0x24d0, 0xe100), (0x24d4, 0x7010),
                    (0x24d8, 0xb134), (0x24dc, 0x7008), (0x24e0, 0x2580),
                    (0x24f0, 0x731c), (0x24f4, 0x2244), (0x1a4d0, 0xb134)):
                    struct.pack_into('<I', expected, off, value)
                if extra:
                    struct.pack_into('<II', expected, 0x24f8, 0x10002351, 0x10002341)
            assert C.string_at(shadow, size) == expected
            assert owner.raw == before
            seen.clear()
            init_hws(owner)
            active = [i for i in range(6) if mask & (1 << i)]
            for i in active:
                struct.pack_into('<I', expected, hws[i], 0x400000+i*0x1000)
            struct.pack_into('<I', expected, 0x18080, 0x406000)
            assert seen == [C.addressof(objects[i]) for i in active+[6]]
            assert C.string_at(shadow, size) == expected
            assert owner.raw == before
            assert ram.raw[:guard] == b'\xa5'*guard and ram.raw[-guard:] == b'\xa5'*guard
            cases += 1
image.close()
print(f'PASS exact native engine helpers: {wa_cases} workaround cases, {cases} mode/status cases, RCS/BCS mask 5, complete RAM footprint and guards; no GPU access or RPL workaround admission')
