"""Exact native child allocation/free, range rollback and page registration on RAM."""
import ctypes as C,hashlib,mmap,platform,struct,sys
from pathlib import Path
assert platform.machine().lower() in ('x86_64','amd64')
data=Path(sys.argv[1]).read_bytes();assert hashlib.sha256(data).hexdigest() in ('bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0','6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64')
# Exact donor vptr is CC720, free slot+90 atCC7B0. The imported
# superclass vtable pointer is the symbol base: native tail uses base+A0.
assert struct.unpack_from('<Q',data,0xcc7b0)[0]==0xfd58
for phase in ('success','range-fail','class-fail','subinit-fail'):
 image=mmap.mmap(-1,0x200000,prot=7);image[:len(data)]=data;base=C.addressof(C.c_char.from_buffer(image));keep=[];errors=[];calls=[];live={};reserved=[False];partial=[0]
 def jump(site,fn):keep.append(fn);image[site:site+14]=bytes.fromhex('ff2500000000')+struct.pack('<Q',C.cast(fn,C.c_void_p).value)
 def put(obj,off,value):C.c_uint64.from_address(C.addressof(obj)+off).value=value
 def read(obj,off):return C.c_uint64.from_address(C.addressof(obj)+off).value
 owner=C.create_string_buffer(0x2000);pool=C.create_string_buffer(0x80);parent=C.create_string_buffer(16);allocator=C.create_string_buffer(16);avt=C.create_string_buffer(0x168);cvt=C.create_string_buffer(0x1d0);meta=C.create_string_buffer(0x90);supervt=C.create_string_buffer(0x260)
 put(pool,0x18,C.addressof(parent));put(pool,0x20,C.addressof(allocator));put(pool,0x60,4096);put(pool,0x78,C.addressof(pool)+0x70);put(allocator,0,C.addressof(avt))
 @C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_uint64,C.c_void_p,C.c_uint64)
 def allocate(who,size,output,alignment):
  assert who==C.addressof(allocator) and size==4096 and alignment==0
  if reserved[0] or phase=='range-fail':return False
  reserved[0]=True;C.c_uint64.from_address(output).value=0;calls.append('reserve');return True
 @C.CFUNCTYPE(None,C.c_void_p,C.c_uint64,C.c_uint64)
 def deallocate(who,offset,size):
  assert who==C.addressof(allocator) and offset==0 and size==4096 and reserved[0]
  reserved[0]=False;calls.append('return-range')
 @C.CFUNCTYPE(C.c_void_p,C.c_void_p)
 def make(who):
  calls.append('make')
  if phase=='class-fail':return 0
  obj=C.create_string_buffer(0x90);put(obj,0,C.addressof(cvt));live[C.addressof(obj)]=obj;return C.addressof(obj)
 @C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_void_p,C.c_uint64,C.c_uint64,C.c_uint32)
 def init(obj,desc,offset,size,options):
  assert desc==C.addressof(parent) and offset==0 and size==4096 and options==0x43
  calls.append('init');return phase!='subinit-fail'
 @C.CFUNCTYPE(C.c_uint64,C.c_void_p)
 def length(obj):return 4096
 @C.CFUNCTYPE(None,C.c_void_p)
 def superfree(obj):assert obj in live;calls.append('superfree');del live[obj]
 @C.CFUNCTYPE(None,C.c_void_p)
 def release(obj):
  # Model the new narrowly scoped production free guard for the donor bug.
  if not C.c_uint64.from_address(obj+0x70).value:
   partial[0]+=1;superfree(obj)
  else:C.CFUNCTYPE(None,C.c_void_p)(base+0xfd58)(obj)
 keep.extend([allocate,deallocate,make,init,length,superfree,release])
 put(avt,0x150,C.cast(allocate,C.c_void_p).value);put(avt,0x160,C.cast(deallocate,C.c_void_p).value);put(cvt,0x28,C.cast(release,C.c_void_p).value);put(cvt,0x1c8,C.cast(length,C.c_void_p).value)
 put(meta,0x88,C.cast(make,C.c_void_p).value);struct.pack_into('<Q',image,0x14a7c8,C.addressof(meta));put(supervt,0x250,C.cast(init,C.c_void_p).value);put(supervt,0xa0,C.cast(superfree,C.c_void_p).value);struct.pack_into('<Q',image,0xc81a0,C.addressof(supervt))
 invoke=C.CFUNCTYPE(C.c_void_p,C.c_void_p,C.c_uint64)(base+0xc876)
 for round in range(2 if phase=='success' else 1):
  obj=invoke(pool,4096)
  if phase!='success':
   assert not obj and not live and not reserved[0] and read(pool,0x50)==0 and read(pool,0x68)==0
   assert partial[0]==(phase=='subinit-fail');break
  assert obj in live and read(pool,0x50)==round+1 and read(pool,0x58)==round and read(pool,0x68)==4096 and read(pool,0x70)==obj and read(pool,0x78)==obj+0x80
  assert C.c_uint64.from_address(obj+0x70).value==C.addressof(pool) and C.c_uint32.from_address(obj+0x78).value==2
  assert not invoke(pool,4096) and len(live)==1
  release(obj)
  assert not live and not reserved[0] and read(pool,0x58)==round+1 and read(pool,0x68)==0 and read(pool,0x70)==0 and read(pool,0x78)==C.addressof(pool)+0x70
 manager=C.create_string_buffer(0x118);pages=[C.create_string_buffer(0x78) for _ in range(16)];array=(C.c_uint64*16)(*[C.addressof(p) for p in pages]);count=C.c_uint32(16)
 put(manager,0x110,C.addressof(array));struct.pack_into('<Q',image,0xc8240,C.addressof(count))
 before=[p.raw for p in pages]
 assert C.CFUNCTYPE(C.c_bool,C.c_void_p)(base+0xf450)(manager)
 assert before==[p.raw for p in pages]
 image.close()
print('PASS exact native allocate/withSubRange/free/deallocate bodies, exhaustion/reuse, failure range rollback, partial-free guard and 16 real non-custom event bodies; no hardware')
