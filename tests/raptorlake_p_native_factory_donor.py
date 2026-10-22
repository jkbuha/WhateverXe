"""Replay full native factory/init/eDRAM-disabled/free on CPU-owned RAM.

Retain actual 2D24A/E580/ED9E/4BAAE/F39E/EE6A bodies, model external
construction/forcewake, verify explicit array cleanup and a negative leak case.
"""
import ctypes as C,hashlib,mmap,platform,struct,sys
from pathlib import Path
assert platform.machine().lower() in ('x86_64','amd64')
data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() in ('6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64','bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0')
for phase in ('success','missing-array-cleanup','array-alloc-fail','pool-0-fail','pool-7-fail','pool-15-fail'):
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
 def release(ptr):
  calls.append('release-'+names[ptr])
  if names[ptr].startswith('page-'):del pages[ptr]
 keep.append(release);struct.pack_into('<Q',vt,0x28,C.cast(release,C.c_void_p).value)
 for obj in objects.values():struct.pack_into('<Q',obj,0,C.addressof(vt))
 @C.CFUNCTYPE(C.c_bool,C.c_void_p)
 def device(m):
  calls.append('device');put(m,0x18,C.addressof(mmio));put(m,0x68,C.addressof(objects['map']));put(m,0x70,C.addressof(objects['map']));return True
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
  ok=C.CFUNCTYPE(C.c_bool,C.c_void_p)(base+0xed9e)(m)
  if ok!=(phase in ('success','missing-array-cleanup')):errors.append(('unexpected array result',ok))
  calls.append('page-array-constructed' if ok else 'page-array-failed')
  return ok
 @C.CFUNCTYPE(None,C.c_void_p)
 def cleanup(m):
  calls.append('dummy-cleanup')
  if phase=='success' and field(m,0x110):errors.append('native free before array cleanup')
 for site,fn in [(0xe7b2,device),(0xebc2,segments),(0xec76,dummy),(0xecda,gtt),(0xc586,pool),(0x1379e,fence),(0xef1e,cleanup)]:
  assert data[site:site+4]==bytes.fromhex('554889e5');jump(site,fn)
 # Keep ED9E intact: route its caller to a wrapper which invokes the real
 # initializer and always returns false to E580, before its cache virtual call.
 jump(0x1a0000,stop)
 assert data[0xe6c7:0xe6cc]==bytes.fromhex('e8d2060000')
 struct.pack_into('<i',image,0xe6c8,0x1a0000-0xe6cc)
 core=(Path(__file__).resolve().parents[1]/'WhateverXe/kern_raptorlake_p_page_array_core.hpp').read_text()
 def patchBytes(name):return bytes(int(t.strip(),0) for t in core.split(name+'[]{')[1].split('}')[0].split(','))
 assert data[0xee09:0xee3a]==patchBytes('CleanupBefore');image[0xee09:0xee3a]=patchBytes('CleanupAfter')
 ncpus=C.c_uint32(16);struct.pack_into('<Q',image,0xc8240,C.addressof(ncpus))
 pages={};factoryCalls=[0];backing=C.create_string_buffer(192);C.memset(C.addressof(backing),0xa5,192);arrayAddress=C.addressof(backing)+32
 @C.CFUNCTYPE(C.c_void_p,C.c_size_t)
 def allocate(n):
  calls.append('array-allocate')
  if n!=128:errors.append('array size')
  return 0 if phase=='array-alloc-fail' else arrayAddress
 @C.CFUNCTYPE(None,C.c_void_p,C.c_size_t)
 def deallocate(ptr,n):
  calls.append('array-free')
  if ptr!=arrayAddress or n!=128 or pages:errors.append('invalid array free')
  if backing.raw[:32]!=b'\xa5'*32 or backing.raw[-32:]!=b'\xa5'*32:errors.append('array redzone')
 @C.CFUNCTYPE(C.c_void_p,C.c_void_p,C.c_uint32)
 def pageFactory(who,options):
  i=factoryCalls[0];factoryCalls[0]+=1;calls.append('create-page-'+str(i))
  if who!=C.addressof(owner) or options:errors.append('wrong page factory arguments')
  if phase=='pool-'+str(i)+'-fail':return 0
  obj=C.create_string_buffer(0x78);struct.pack_into('<Q',obj,0,C.addressof(vt));ptr=C.addressof(obj);pages[ptr]=obj;names[ptr]='page-'+str(i);return ptr
 jump(0xa860,pageFactory)
 for site,fn in [(0xedba,allocate),(0xee49,deallocate),(0xf414,deallocate)]:
  assert data[site:site+5]==bytes.fromhex('e800000000')
  stub=0x190000+32*len(keep);jump(stub,fn);struct.pack_into('<i',image,site+1,stub-site-5)
 mmio=C.create_string_buffer(0x1000000)
 @C.CFUNCTYPE(None,C.c_void_p,C.c_uint32,C.c_uint32)
 def forcewake(who,on,domain):
  if who!=C.addressof(owner) or domain!=4:errors.append('native forcewake ABI')
  calls.append('native-fw-acquire' if on else 'native-fw-release')
 jump(0x28c2a,forcewake)
 @C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_void_p,C.c_uint64,C.c_uint32)
 def log(who,name,size,kind):
  calls.append('log-vram')
  if who!=C.addressof(owner) or C.string_at(name)!=b'VRAM,totalMB' or size!=384 or kind!=32:errors.append(('native property ABI',C.string_at(name),size,kind))
  return True
 @C.CFUNCTYPE(None,C.c_void_p)
 def superfree(m):calls.append('super-free')
 @C.CFUNCTYPE(None,C.c_void_p)
 def managerRelease(m):
  calls.append('manager-release');C.CFUNCTYPE(None,C.c_void_p)(base+0xee6a)(m)
 @C.CFUNCTYPE(C.c_void_p,C.c_void_p)
 def newManager(who):
  if who!=C.addressof(owner):errors.append('factory owner')
  calls.append('new-manager');return C.addressof(manager)
 keep.extend([log,superfree,managerRelease,newManager]);managerVT=C.create_string_buffer(0x160);ownerVT=C.create_string_buffer(0xb00);superVT=C.create_string_buffer(0xa8)
 struct.pack_into('<Q',managerVT,0x118,base+0xe580);struct.pack_into('<Q',managerVT,0x148,base+0x4baae);struct.pack_into('<Q',managerVT,0x28,C.cast(managerRelease,C.c_void_p).value);struct.pack_into('<Q',manager,0,C.addressof(managerVT))
 struct.pack_into('<Q',ownerVT,0x280,C.cast(log,C.c_void_p).value);struct.pack_into('<Q',ownerVT,0xaf0,C.cast(newManager,C.c_void_p).value);struct.pack_into('<Q',owner,0,C.addressof(ownerVT))
 C.memmove(C.addressof(owner)+0x13c0,shared,8);C.memmove(C.addressof(owner)+0x13d0,stolen,40)
 struct.pack_into('<Q',superVT,0xa0,C.cast(superfree,C.c_void_p).value);struct.pack_into('<Q',image,0xc81e0,C.addressof(superVT))
 result=C.CFUNCTYPE(C.c_void_p,C.c_void_p)(base+0x2d24a)(owner)
 success=phase in ('success','missing-array-cleanup')
 assert bool(result)==success and not errors,(result,errors,calls)
 if success:
  assert result==C.addressof(manager) and field(result,0x110)==arrayAddress and C.c_uint16.from_address(result+0x20).value==0
  if phase=='success':C.CFUNCTYPE(None,C.c_void_p)(base+0xf39e)(manager)
  managerRelease(result)
  if phase=='missing-array-cleanup':
   assert field(result,0x110)==arrayAddress and len(pages)==16
   calls.append('free-left-array-owned')
   C.CFUNCTYPE(None,C.c_void_p)(base+0xf39e)(manager)
 expected=['new-manager','device','segments','dummy','gtt','pool','fence','page-pool-stop','array-allocate']
 if success:expected+=['create-page-'+str(i) for i in range(16)]+['page-array-constructed','native-fw-acquire','native-fw-release','log-vram']
 elif phase=='array-alloc-fail':expected+=['page-array-failed']
 else:
  failed=int(phase.split('-')[1]);expected+=['create-page-'+str(i) for i in range(failed+1)]+['release-page-'+str(i) for i in reversed(range(failed))]+['array-free','page-array-failed']
 if success:
  if phase=='success':expected+=['release-page-'+str(i) for i in range(16)]+['array-free']
  expected+=['manager-release','dummy-cleanup','release-fence','release-pool','release-gtt','release-map','release-map','super-free']
  if phase=='missing-array-cleanup':expected+=['free-left-array-owned']+['release-page-'+str(i) for i in range(16)]+['array-free']
 else:expected+=['release-map','release-map','release-gtt','release-pool','release-fence','dummy-cleanup','manager-release','dummy-cleanup','super-free']
 assert calls==expected and not errors,(phase,calls,expected,errors)
 assert not pages and all(not field(C.addressof(manager),off) for off in (0x18,0x68,0x70,0x78,0x80,0x98,0xa0,0xa8,0xb0,0xb8,0xc0,0xc8,0xd0,0xd8,0x110))
 image.close()
print('PASS exact full native factory CPU replay: actual factory/init/eDRAM-disabled/array/free bodies, VRAM property ABI, explicit array-before-manager cleanup, negative leak control and allocation failure unwind; no GPU access')
