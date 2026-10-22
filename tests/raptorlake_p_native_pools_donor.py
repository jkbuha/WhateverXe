"""Execute exact native pool/fence initialization and release on CPU RAM.

Model external IOKit descriptor/allocator APIs and OSObject superclass methods;
retain real range copying, vector orchestration and page-count arithmetic.
"""
import ctypes as C,hashlib,mmap,platform,struct,sys
from pathlib import Path
assert platform.machine().lower() in ('x86_64','amd64')
data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() in ('6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64','bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0')
for fail in ('none','compound','allocator'):
 image=mmap.mmap(-1,0x200000,prot=7);image[:len(data)]=data;base=C.addressof(C.c_char.from_buffer(image));keep=[];calls=[];errors=[]
 def jump(site,fn):
  keep.append(fn);image[site:site+14]=bytes.fromhex('ff2500000000')+struct.pack('<Q',C.cast(fn,C.c_void_p).value)
 def call(site,fn):
  assert data[site:site+5]==b'\xe8\0\0\0\0',hex(site)
  stub=0x190000+32*len(keep);jump(stub,fn);image[site+1:site+5]=struct.pack('<i',stub-site-5)
 owner=C.create_string_buffer(0x2000);pool=C.create_string_buffer(0x80);fence=C.create_string_buffer(0x48)
 physical=0x712340000;info=(C.c_uint64*5)(1,physical,1,0,0);vector=(C.c_uint64*4)();page=C.c_uint64(4096)
 physicalDesc=C.create_string_buffer(16);compound=C.create_string_buffer(16);allocator=C.create_string_buffer(16)
 released=[];vt=C.create_string_buffer(0x1d0);supervt=C.create_string_buffer(0xa8)
 @C.CFUNCTYPE(None,C.c_void_p)
 def release(obj):released.append(obj)
 @C.CFUNCTYPE(C.c_bool,C.c_void_p)
 def superinit(obj):calls.append('super-init');return True
 @C.CFUNCTYPE(None,C.c_void_p)
 def superfree(obj):calls.append(('super-free',obj))
 @C.CFUNCTYPE(C.c_uint64,C.c_void_p)
 def length(obj):return 4096
 keep.extend([release,superinit,superfree,length])
 struct.pack_into('<Q',vt,0x28,C.cast(release,C.c_void_p).value);struct.pack_into('<Q',vt,0x1c8,C.cast(length,C.c_void_p).value)
 for obj in (physicalDesc,compound,allocator):struct.pack_into('<Q',obj,0,C.addressof(vt))
 struct.pack_into('<Q',supervt,0x98,C.cast(superinit,C.c_void_p).value);struct.pack_into('<Q',supervt,0xa0,C.cast(superfree,C.c_void_p).value)
 for insn,end in [(0xc5f1,0xc5f8),(0xc785,0xc78c),(0x137fd,0x13804),(0x13828,0x1382f)]:
  assert data[insn:insn+3]==bytes.fromhex('488b05')
  cell=end+struct.unpack_from('<i',data,insn+3)[0];struct.pack_into('<Q',image,cell,C.addressof(supervt))
 for insn,end in [(0xc7c5,0xc7cc),(0xc673,0xc67a)]:
  assert data[insn:insn+3]==bytes.fromhex('488b0d' if insn==0xc673 else '488b05')
  cell=end+struct.unpack_from('<i',data,insn+3)[0];struct.pack_into('<Q',image,cell,C.addressof(page))
 @C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_uint64)
 def grow(ptr,capacity):
  if capacity!=4:errors.append('unexpected vector growth')
  fields=(C.c_uint64*3).from_address(ptr);fields[1]=4;fields[2]=C.addressof(vector);return True
 jump(0xc9e8,grow)
 @C.CFUNCTYPE(C.c_void_p,C.c_uint64,C.c_uint64,C.c_uint32)
 def physicalRange(address,size,options):
  if (address,size,options)!=(physical,4096,3):errors.append(('bad physical range',address,size,options))
  calls.append('physical-descriptor');return C.addressof(physicalDesc)
 call(0xc7dc,physicalRange)
 @C.CFUNCTYPE(C.c_void_p,C.c_void_p,C.c_uint32,C.c_uint32,C.c_bool)
 def combine(ptr,count,options,reference):
  if count!=1 or options!=0x43 or reference or C.c_uint64.from_address(ptr).value!=C.addressof(physicalDesc):errors.append('bad compound descriptor')
  calls.append('compound-descriptor');return 0 if fail=='compound' else C.addressof(compound)
 call(0xc70c,combine)
 @C.CFUNCTYPE(None,C.c_void_p,C.c_uint64)
 def freeVector(ptr,size):
  if ptr!=C.addressof(vector) or size!=32:errors.append('bad vector free')
  calls.append('vector-free')
 call(0xc74a,freeVector)
 @C.CFUNCTYPE(C.c_void_p,C.c_uint64,C.c_uint64,C.c_uint64)
 def makeAllocator(start,size,alignment):
  if (start,size,alignment)!=(0,4096,4096):errors.append(('bad allocator',start,size,alignment))
  calls.append('allocator');return 0 if fail=='allocator' else C.addressof(allocator)
 jump(0x13018,makeAllocator)
 init=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_void_p,C.c_void_p)(base+0xc5de)
 # The donor reports true even on these descriptor/allocator failures. The
 # production diagnostic must validate every returned field before use.
 assert init(pool,info,owner)
 def field(off):return struct.unpack_from('<Q',pool,off)[0]
 assert field(0x10)==C.addressof(owner)
 assert [field(off) for off in (0x28,0x30,0x38,0x40,0x48)]==list(info)
 assert field(0x60)==4096 and field(0x70)==0 and field(0x78)==C.addressof(pool)+0x70
 assert field(0x18)==(C.addressof(compound) if fail=='none' else 0)
 assert field(0x20)==(C.addressof(allocator) if fail=='none' else 0)
 assert not errors,errors
 assert released==[C.addressof(physicalDesc)]+([C.addressof(compound)] if fail=='allocator' else [])
 C.CFUNCTYPE(None,C.c_void_p)(base+0xc75e)(pool)
 expected=[C.addressof(physicalDesc)]
 if fail!='compound':expected.append(C.addressof(compound))
 if fail=='none':expected.append(C.addressof(allocator))
 assert released==expected and calls[-1]==('super-free',C.addressof(pool)),(released,calls)
 assert C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_void_p)(base+0x137f0)(fence,owner)
 assert struct.unpack_from('<Q',fence,0x10)[0]==C.addressof(owner)
 assert struct.unpack_from('<Q',fence,0x40)[0]==0xfffffff8
 C.CFUNCTYPE(None,C.c_void_p)(base+0x13824)(fence)
 assert calls[-1]==('super-free',C.addressof(fence))
 image.close()
print('PASS exact native pool/fence CPU replay: one owned physical page, native range/vector arithmetic, compound descriptor ownership, partial-constructor false success and release paths; no GPU or DSM access')
