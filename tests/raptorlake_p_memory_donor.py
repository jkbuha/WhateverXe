"""Replay exact native manager failure/unwind, with IOKit mapping modeled.

No GPU, kernel, driver construction or real memory mappings are executed.
The native init and release bodies must unwind before GGTT initialization.
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
assert data[0xe580:0xe58b] == bytes.fromhex('554889e541574156415453')
image = mmap.mmap(-1, 0x200000, prot=mmap.PROT_READ | mmap.PROT_WRITE | mmap.PROT_EXEC)
image[:len(data)] = data
assert data[0xe597:0xe5a4] == bytes.fromhex('488b05429c0b00ff9098000000')
# Model only superclass OSObject::init success, preserving native error branch.
image[0xe597:0xe5a4] = bytes.fromhex('b001') + b'\x90' * 11
base = C.addressof(C.c_char.from_buffer(image))
calls = []
keep = []
def jump(site, callback):
    keep.append(callback)
    image[site:site+14] = bytes.fromhex('ff2500000000') + struct.pack('<Q', C.cast(callback,C.c_void_p).value)
def put(ptr, off, value): C.c_uint64.from_address(ptr+off).value = value
@C.CFUNCTYPE(None, C.c_void_p)
def released(_): calls.append('release')
vtable = C.create_string_buffer(0x40)
struct.pack_into('<Q', vtable, 0x28, C.cast(released,C.c_void_p).value)
map_object = C.create_string_buffer(16)
struct.pack_into('<Q', map_object, 0, C.addressof(vtable))
@C.CFUNCTYPE(C.c_bool, C.c_void_p)
def device(manager):
    calls.append('device')
    put(manager,0x18,0x12340000)
    put(manager,0x68,C.addressof(map_object));put(manager,0x70,C.addressof(map_object))
    return True
@C.CFUNCTYPE(C.c_bool, C.c_void_p)
def segments(manager):
    calls.append('segments');put(manager,0xa8,0x4000000)
    put(manager,0xb0,0x4000000);put(manager,0xb8,0xc000000)
    return True
@C.CFUNCTYPE(C.c_bool, C.c_void_p)
def dummy(manager):
    calls.append('dummy-stop')
    assert C.c_uint64.from_address(manager+0xa8).value == 0x4000000
    return False
@C.CFUNCTYPE(C.c_bool, C.c_void_p)
def forbidden(_):
    calls.append('FORBIDDEN-GGTT')
    return False
for site, fn in ((0xe7b2,device),(0xebc2,segments),(0xec76,dummy),(0xecda,forbidden)):
    assert data[site:site+4] == bytes.fromhex('554889e5')
    jump(site,fn)
manager = C.create_string_buffer(0x200)
owner = C.create_string_buffer(0x2000)
shared = C.create_string_buffer(struct.pack('<II',0,0x18000000))
stolen = C.create_string_buffer(40)
fn = C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_void_p,C.c_void_p,C.c_void_p)(base+0xe580)
assert not fn(manager,owner,shared,stolen)
assert calls == ['device','segments','dummy-stop','release','release'], calls
for off in (0x18,0x68,0x70,0xa0,0xa8,0xb0,0xb8,0xc0,0xc8,0xd0,0xd8):
    assert struct.unpack_from('<Q',manager,off)[0] == 0, hex(off)
image.close()
print('PASS exact native memory init: dummy stop prevents GGTT, native unwind releases both mappings and clears ranges')
