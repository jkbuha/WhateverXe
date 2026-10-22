"""Replay the exact manager init through pool/fence construction and stop.

Only external construction/mapping APIs are modeled. The real manager drives
its success branches, the page-pool stop, and the entire failure unwind.
"""
import ctypes as C,hashlib,mmap,platform,struct,sys
from pathlib import Path
assert platform.machine().lower() in ('x86_64','amd64')
data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() in ('6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64','bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0')
for phase in ('stop','pool-fail','fence-fail','gtt-refuse'):
 image=mmap.mmap(-1,0x200000,prot=7);image[:len(data)]=data;base=C.addressof(C.c_char.from_buffer(image));keep=[];calls=[];errors=[]
 assert data[0xe597:0xe5a4]==bytes.fromhex('488b05429c0b00ff9098000000')
 image[0xe597:0xe5a4]=bytes.fromhex('b001')+b'\x90'*11
 def jump(site,fn):
  keep.append(fn);image[site:site+14]=bytes.fromhex('ff2500000000')+struct.pack('<Q',C.cast(fn,C.c_void_p).value)
 def put(ptr,off,value):C.c_uint64.from_address(ptr+off).value=value
 def field(ptr,off):return C.c_uint64.from_address(ptr+off).value
 manager=C.create_string_buffer(0x118);owner=C.create_string_buffer(0x2000);shared=C.create_string_buffer(struct.pack('<II',0,0x18000000));stolen=(C.c_uint64*5)(1,0x712340000,1,0,0)
 vt=C.create_string_buffer(0x40);objects={name:C.create_string_buffer(16) for name in ['map','gtt','pool','fence']};names={C.addressof(v):k for k,v in objects.items()}
 @C.CFUNCTYPE(None,C.c_void_p)
 def release(ptr):calls.append('release-'+names[ptr])
 keep.append(release);struct.pack_into('<Q',vt,0x28,C.cast(release,C.c_void_p).value)
 for obj in objects.values():struct.pack_into('<Q',obj,0,C.addressof(vt))
 @C.CFUNCTYPE(C.c_bool,C.c_void_p)
 def device(m):
  calls.append('device');put(m,0x18,0x12340000);put(m,0x68,C.addressof(objects['map']));put(m,0x70,C.addressof(objects['map']));return True
 @C.CFUNCTYPE(C.c_bool,C.c_void_p)
 def segments(m):
  calls.append('segments')
  for off,value in [(0xa0,0),(0xa8,0x4000000),(0xb0,0x4000000),(0xb8,0xc000000)]:put(m,off,value)
  return True
 @C.CFUNCTYPE(C.c_bool,C.c_void_p)
 def dummy(m):calls.append('dummy');return True
 @C.CFUNCTYPE(C.c_bool,C.c_void_p)
 def gtt(m):
  calls.append('gtt');put(m,0x98,C.addressof(objects['gtt']));return phase!='gtt-refuse'
 @C.CFUNCTYPE(C.c_void_p,C.c_void_p,C.c_void_p)
 def pool(info,who):
  calls.append('pool')
  if who!=C.addressof(owner) or tuple((C.c_uint64*5).from_address(info))!=tuple(stolen):errors.append('wrong borrowed RAM info')
  return 0 if phase=='pool-fail' else C.addressof(objects['pool'])
 @C.CFUNCTYPE(C.c_void_p,C.c_void_p)
 def fence(who):
  calls.append('fence')
  if who!=C.addressof(owner):errors.append('wrong fence owner')
  return 0 if phase=='fence-fail' else C.addressof(objects['fence'])
 @C.CFUNCTYPE(C.c_bool,C.c_void_p)
 def stop(m):
  calls.append('page-pool-stop')
  if field(m,0x78)!=C.addressof(objects['pool']) or field(m,0x80)!=C.addressof(objects['fence']):errors.append('allocator construction order')
  return False
 @C.CFUNCTYPE(None,C.c_void_p)
 def cleanup(m):
  calls.append('dummy-cleanup')
  if any(field(m,o) for o in (0x18,0x68,0x70,0x78,0x80,0x98)):errors.append('backing released before native object unwind')
 for site,fn in [(0xe7b2,device),(0xebc2,segments),(0xec76,dummy),(0xecda,gtt),(0xc586,pool),(0x1379e,fence),(0xed9e,stop),(0xef1e,cleanup)]:
  assert data[site:site+4]==bytes.fromhex('554889e5');jump(site,fn)
 result=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_void_p,C.c_void_p,C.c_void_p)(base+0xe580)(manager,owner,shared,stolen)
 assert not result and not errors,(result,errors)
 expected=['device','segments','dummy','gtt']
 if phase!='gtt-refuse':expected+=['pool']
 if phase not in ('gtt-refuse','pool-fail'):expected+=['fence']
 if phase=='stop':expected+=['page-pool-stop']
 expected+=['release-map','release-map','release-gtt']
 if phase in ('stop','fence-fail'):expected+=['release-pool']
 if phase=='stop':expected+=['release-fence']
 expected+=['dummy-cleanup'];assert calls==expected,(phase,calls,expected)
 assert all(not field(C.addressof(manager),off) for off in (0x18,0x68,0x70,0x78,0x80,0x98,0xa0,0xa8,0xb0,0xb8,0xc0,0xc8,0xd0,0xd8))
 image.close()
print('PASS exact native memory factory: real pool/fence success branches, fixed pre-page-pool stop, complete original object unwind, early constructor failures and restored empty manager')
