"""Execute exact signed donor cache/PAT/L3/MOCS bodies on RAM, never hardware."""
import ctypes as C,hashlib,mmap,platform,struct,sys
from pathlib import Path
assert platform.machine().lower() in ('x86_64','amd64')
data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() in ('6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64','bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0')
assert data[0xfe1a:0xfe22].hex()=='554889e541565349'
assert data[0x28c2a:0x28c3a].hex()=='554889e54157415641554154535085d2'
for case in ('normal','wrong-wake-owner','wrong-wake-domain','wrong-mocs','stray-write'):
 image=mmap.mmap(-1,0x200000,prot=7);image[:len(data)]=data;base=C.addressof(C.c_char.from_buffer(image));keep=[];calls=[];errors=[]
 def jump(site,fn):
  keep.append(fn);image[site:site+14]=bytes.fromhex('ff2500000000')+struct.pack('<Q',C.cast(fn,C.c_void_p).value)
 manager=C.create_string_buffer(0x118);owner=C.create_string_buffer(0x2000);vt=C.create_string_buffer(0x160)
 size=0x1000000;guard=4096;ram=C.create_string_buffer(size+2*guard);C.memset(ram,0xa5,len(ram));shadow=C.addressof(ram)+guard
 struct.pack_into('<Q',manager,0,C.addressof(vt));struct.pack_into('<Q',manager,0x10,C.addressof(owner));struct.pack_into('<Q',manager,0x18,shadow);struct.pack_into('<Q',owner,0x1240,shadow)
 struct.pack_into('<Q',vt,0x140,base+0x4ba56);struct.pack_into('<Q',vt,0x150,base+0x4be72)
 for o in (0x9008,0x900c):C.c_uint32.from_address(shadow+o).value=0xffffffff
 @C.CFUNCTYPE(None,C.c_void_p,C.c_uint32,C.c_uint32)
 def wake(who,on,domain):
  if case=='wrong-wake-owner':who=0
  if case=='wrong-wake-domain':domain=4
  if who!=C.addressof(owner) or domain!=5:errors.append('wake-scope')
  calls.append(on)
 jump(0x28c2a,wake)
 @C.CFUNCTYPE(None,C.c_void_p)
 def mocs(m):
  assert m==C.addressof(manager)
  # The preceding 4024 store must exist before native MOCS overwrites it.
  assert C.c_uint32.from_address(shadow+0x4024).value==0xf0007fff
  C.CFUNCTYPE(None,C.c_void_p)(base+0x4ba7c)(m)
  if case=='wrong-mocs':C.c_uint32.from_address(shadow+0x4024).value=0xf0007fff
  if case=='stray-write':C.c_uint32.from_address(shadow+0x7770).value=0
 keep.append(mocs);struct.pack_into('<Q',vt,0x138,C.cast(mocs,C.c_void_p).value)
 C.CFUNCTYPE(None,C.c_void_p)(base+0xfe1a)(manager)
 assert calls==[1,0] and struct.unpack_from('<Q',manager,0x108)[0]>0
 expected=bytearray(b'\xa5'*size)
 for o,v in [(0x9008,0xff3fffff),(0x900c,0xff9fffff),(0xb134,0xd0000020),(0xb138,0)]:struct.pack_into('<I',expected,o,v)
 for i in range(8):struct.pack_into('<I',expected,0x4800+4*i,data[0xbecd0+i])
 expected[0xb020:0xb0a0]=data[0xa00d0:0xa0150];expected[0x4000:0x4100]=data[0xbecd8:0xbedd8]
 actual=C.string_at(shadow,size)
 assert (actual==expected)==(case not in ('wrong-mocs','stray-write'))
 assert bool(errors)==case.startswith('wrong-wake')
 assert ram.raw[:guard]==b'\xa5'*guard and ram.raw[-guard:]==b'\xa5'*guard
 assert struct.unpack_from('<Q',manager,0x18)[0]==shadow and struct.unpack_from('<Q',owner,0x1240)[0]==shadow
 del actual;image.close()
print('PASS exact native cache/PAT/L3/MOCS CPU replay: all 16MiB, guard pages, CPUID result, wake ABI/order, 4024 overwrite and negative controls; no GPU access')
