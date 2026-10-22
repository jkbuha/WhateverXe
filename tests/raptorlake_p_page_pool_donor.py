"""Replay guarded native page-pool init, empty purge and free on CPU RAM.

Use the production allocation-tail helper and actual native vector/hash bodies.
Model only external heap, OSObject superclass, atomic ID and time conversion.
"""
import ctypes as C,hashlib,mmap,platform,struct,subprocess,sys,tempfile
from pathlib import Path
assert platform.machine().lower() in ('x86_64','amd64')
r=Path(__file__).resolve().parents[1];data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() in ('6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64','bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0')
source=r'''
#include "kern_raptorlake_p_page_pool_core.hpp"
using namespace RaptorLakeP::PagePool;
extern "C" unsigned tail(void *p,void *o,const Methods *m){return construct(p,o,*m);}
extern "C" bool shape(void *p,void *o,uintptr_t vt){return valid(p,o,vt);}
extern "C" bool patch(const uint8_t *b,uintptr_t h,uint8_t *a){uint8_t out[20];if(!tailPatch(b,h,out))return false;memcpy(a,out,20);return true;}
'''
with tempfile.TemporaryDirectory(prefix='rplp-page-pool-donor-') as tmp:
 p=Path(tmp);(p/'core.cpp').write_text(source)
 subprocess.run(['clang++','-std=c++17','-O1','-shared','-fPIC','-I'+str(r/'WhateverXe'),str(p/'core.cpp'),'-o',str(p/'core.dylib')],check=True)
 lib=C.CDLL(str(p/'core.dylib'))
 class Methods(C.Structure):_fields_=[(n,C.c_void_p) for n in ('allocate','deallocate','grow','hashInit','hashVtable')]
 lib.tail.argtypes=[C.c_void_p,C.c_void_p,C.POINTER(Methods)];lib.tail.restype=C.c_uint
 lib.shape.argtypes=[C.c_void_p,C.c_void_p,C.c_size_t];lib.shape.restype=C.c_bool
 lib.patch.argtypes=[C.c_void_p,C.c_size_t,C.c_void_p];lib.patch.restype=C.c_bool
 for fail in range(8):
  image=mmap.mmap(-1,0x200000,prot=7);image[:len(data)]=data;base=C.addressof(C.c_char.from_buffer(image));keep=[];heap={};events=[];errors=[];counts=[0];tailResult=[]
  def jump(site,fn):
   keep.append(fn);image[site:site+14]=bytes.fromhex('ff2500000000')+struct.pack('<Q',C.cast(fn,C.c_void_p).value)
  def imported(site,fn):
   assert data[site] in (0xe8,0xe9) and data[site+1:site+5]==bytes(4),hex(site)
   stub=0x190000+32*len(keep);jump(stub,fn);struct.pack_into('<i',image,site+1,stub-site-5)
  @C.CFUNCTYPE(C.c_void_p,C.c_size_t)
  def allocate(n):
   counts[0]+=1;events.append(('alloc',n))
   if counts[0]==fail:return 0
   b=C.create_string_buffer(n);C.memset(C.addressof(b),0xa5,n);heap[C.addressof(b)]=(b,n);return C.addressof(b)
  @C.CFUNCTYPE(None,C.c_void_p,C.c_size_t)
  def deallocate(ptr,n):
   if ptr not in heap or heap[ptr][1]!=n:errors.append(('bad free',ptr,n));return
   events.append(('free',n));del heap[ptr]
  for site in (0xbbb2,0xbc3a,0xa933,0xa973):imported(site,allocate)
  for site in (0xbbee,0xbc92,0xaa63,0xaa70,0xbda1,0xbdb6):imported(site,deallocate)
  owner=C.create_string_buffer(0x2000);page=C.create_string_buffer(0x78);supervt=C.create_string_buffer(0xa8)
  @C.CFUNCTYPE(C.c_bool,C.c_void_p)
  def superinit(obj):events.append(('superinit',obj));return fail!=5
  @C.CFUNCTYPE(None,C.c_void_p)
  def superfree(obj):events.append(('superfree',obj))
  keep.extend([allocate,deallocate,superinit,superfree])
  struct.pack_into('<Q',supervt,0x98,C.cast(superinit,C.c_void_p).value);struct.pack_into('<Q',supervt,0xa0,C.cast(superfree,C.c_void_p).value)
  for site in (0xa916,0xab21):
   assert data[site:site+3]==bytes.fromhex('488b05')
   cell=site+7+struct.unpack_from('<i',data,site+3)[0];struct.pack_into('<Q',image,cell,C.addressof(supervt))
  @C.CFUNCTYPE(C.c_uint64,C.c_uint64,C.c_void_p)
  def atomic(amount,ptr):assert amount==1;return 123
  @C.CFUNCTYPE(None,C.c_uint64,C.c_void_p)
  def timestamp(ns,ptr):assert ns==1000000000;C.c_uint64.from_address(ptr).value=ns
  imported(0xa8f3,atomic);imported(0xa905,timestamp)
  # Also replay empty purge with native debug tracing enabled. Only the
  # external trace sink is modeled; no owner/GPU methods may be called.
  struct.pack_into('<I',image,0xab54+struct.unpack_from('<i',data,0xab50)[0],int(fail==7))
  debugMask=C.c_uint32(0)
  struct.pack_into('<Q',image,0xab7a+struct.unpack_from('<i',data,0xab76)[0],C.addressof(debugMask))
  @C.CFUNCTYPE(None,C.c_uint32,C.c_uint64,C.c_uint64,C.c_uint64,C.c_uint64)
  def trace(code,a,b,c,d):events.append(('trace',code))
  jump(0x56b86,trace)
  methods=Methods(C.cast(allocate,C.c_void_p).value,C.cast(deallocate,C.c_void_p).value,base+0xbb94,base+0xbc0c,base+0xcbfd0)
  @C.CFUNCTYPE(C.c_bool,C.c_void_p)
  def safeTail(obj):
   if obj!=C.addressof(page):errors.append('wrong object');return False
   v=lib.tail(obj,C.addressof(owner),C.byref(methods));tailResult.append(v);return v==0
  keep.append(safeTail);patch=C.create_string_buffer(20)
  assert lib.patch(C.c_char_p(data[0xa92e:0xa942]),C.cast(safeTail,C.c_void_p).value,patch)
  if fail not in (6,7):image[0xa92e:0xa942]=patch.raw
  result=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_void_p,C.c_uint32)(base+0xa8ae)(page,owner,0)
  assert result==(fail in (0,6,7)),(fail,result,events,errors)
  assert struct.unpack_from('<Q',page,0x18)[0]==C.addressof(owner)
  assert struct.unpack_from('<Q',page,0x70)[0]==123
  assert struct.unpack_from('<Q',page,0x58)[0]==1000000000
  if fail in (0,6,7):
   assert tailResult==([] if fail in (6,7) else [0]) and len(heap)==4 and lib.shape(page,owner,base+0xcbfd0)
  else:
   assert tailResult==([] if fail==5 else [fail+1])
   assert not heap and not struct.unpack_from('<Q',page,0x28)[0]
  C.CFUNCTYPE(None,C.c_void_p)(base+0xaa22)(page)
  assert not heap and not errors,(fail,heap,errors,events)
  assert events[-1]==('superfree',C.addressof(page))
  if fail==7:assert [v for k,v in events if k=='trace']==[0x85a0006d,0x85a0006e]
  if fail in (0,6,7):assert [v for k,v in events if k=='free']==[384,24,32,72],events
  image.close()
print('PASS guarded native page-pool CPU replay: unmodified-success control, real init/vector/hash/empty purge/free, all four heap failures, superclass failure, exact cleanup sizes and no GPU access')
