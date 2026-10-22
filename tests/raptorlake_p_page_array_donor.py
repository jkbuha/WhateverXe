"""Execute exact native per-CPU page-pool array construction and release.

Model the already-qualified empty-pool factory and external heap only. Guard
the array with redzones, test all partial-construction failures, and retain the
native success loops and releasePagePool body.
"""
import ctypes as C,hashlib,mmap,platform,struct,sys
from pathlib import Path
assert platform.machine().lower() in ('x86_64','amd64')
r=Path(__file__).resolve().parents[1];data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() in ('6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64','bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0')
source=(r/'WhateverXe/kern_raptorlake_p_page_array_core.hpp').read_text()
def array(name):return bytes(int(t.strip(),0) for t in source.split(name+'[]{')[1].split('}')[0].split(','))
before=array('CleanupBefore');after=array('CleanupAfter');assert len(before)==len(after)==49 and data[0xee09:0xee3a]==before
cases=0
for count in (1,2,16,64):
 for fail in (-2,-1,*range(count)):
  cases+=1;image=mmap.mmap(-1,0x200000,prot=7);image[:len(data)]=data;base=C.addressof(C.c_char.from_buffer(image));keep=[];live={};released=[];errors=[];calls=[0];allocations=[]
  def jump(site,fn):
   keep.append(fn);image[site:site+14]=bytes.fromhex('ff2500000000')+struct.pack('<Q',C.cast(fn,C.c_void_p).value)
  def imported(site,fn):
   assert data[site:site+5]==bytes.fromhex('e800000000'),hex(site)
   stub=0x190000+32*len(keep);jump(stub,fn);struct.pack_into('<i',image,site+1,stub-site-5)
  backing=C.create_string_buffer(count*8+64);C.memset(C.addressof(backing),0xa5,len(backing));arrayAddress=C.addressof(backing)+32
  @C.CFUNCTYPE(C.c_void_p,C.c_size_t)
  def allocate(n):
   if n!=count*8:errors.append(('bad allocation',n))
   allocations.append(n);return 0 if fail==-2 else arrayAddress
  @C.CFUNCTYPE(None,C.c_void_p,C.c_size_t)
  def deallocate(ptr,n):
   if ptr!=arrayAddress or n!=count*8 or live:errors.append(('bad free',ptr,n,len(live)))
   if backing.raw[:32]!=b'\xa5'*32 or backing.raw[-32:]!=b'\xa5'*32:errors.append('redzone changed')
   allocations.clear()
  @C.CFUNCTYPE(None,C.c_void_p)
  def release(obj):
   if obj not in live:errors.append(('duplicate/invalid release',obj));return
   released.append(live.pop(obj)[1])
  vt=C.create_string_buffer(0x30);struct.pack_into('<Q',vt,0x28,C.cast(release,C.c_void_p).value);keep.append(release)
  owner=C.create_string_buffer(0x2000);manager=C.create_string_buffer(0x118);struct.pack_into('<Q',manager,0x10,C.addressof(owner))
  @C.CFUNCTYPE(C.c_void_p,C.c_void_p,C.c_uint32)
  def factory(o,options):
   index=calls[0];calls[0]+=1
   if o!=C.addressof(owner) or options:errors.append('bad factory arguments')
   if index==fail:return 0
   obj=C.create_string_buffer(0x78);struct.pack_into('<Q',obj,0,C.addressof(vt));ptr=C.addressof(obj);live[ptr]=(obj,index);return ptr
  jump(0xa860,factory)
  for site,fn in ((0xedba,allocate),(0xee49,deallocate),(0xf414,deallocate)):imported(site,fn)
  ncpus=C.c_uint32(count);struct.pack_into('<Q',image,0xc8240,C.addressof(ncpus));image[0xee09:0xee3a]=after
  result=C.CFUNCTYPE(C.c_bool,C.c_void_p)(base+0xed9e)(manager)
  ptr=struct.unpack_from('<Q',manager,0x110)[0]
  if fail==-1:
   assert result and ptr==arrayAddress and calls[0]==count and len(live)==count,(count,fail,errors)
   C.CFUNCTYPE(None,C.c_void_p)(base+0xf39e)(manager)
   assert released==list(range(count)),released
  else:
   assert not result and not ptr and not live,(count,fail,result,ptr,live)
   assert released==list(reversed(range(max(0,fail)))),released
   assert calls[0]==(0 if fail==-2 else fail+1)
  assert not struct.unpack_from('<Q',manager,0x110)[0] and not live and not errors,(count,fail,errors)
  # Allocation failure returns null, so no corresponding free is expected.
  assert allocations==([count*8] if fail==-2 else [])
  image.close()
print(f'PASS exact native page-array CPU replay: {cases} cases, CPU counts 1/2/16/64, every failure position, reverse partial cleanup, normal release, untouched redzones and no GPU access')
