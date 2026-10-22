"""Run production ring-field adapter with exact native bodies, CPU RAM only."""
import ctypes as C,hashlib,mmap,platform,struct,subprocess,sys,tempfile
from pathlib import Path
assert platform.machine().lower() in ('x86_64','amd64')
r=Path(__file__).resolve().parents[1];data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() in ('bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0','6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64')
image=mmap.mmap(-1,0x200000,prot=7);image[:len(data)]=data;base=C.addressof(C.c_char.from_buffer(image))
for offset in (0x7c46c,0x7c5b6,0x28e1a,0x28f34,0x13e7c,0x10b60):assert data[offset:offset+4]==bytes.fromhex('554889e5')
shift=C.c_uint8(12);struct.pack_into('<Q',image,0xc8230,C.addressof(shift))
cpp=r'''
#include "kern_raptorlake_p_ring_image_core.hpp"
#include "kern_raptorlake_p_bcs_core.hpp"
#include "kern_raptorlake_p_rcs_core.hpp"
#include <vector>
#include <memory>
using namespace RaptorLakeP;
extern "C" int fields(uint8_t *m,const uint64_t *d,uint32_t b,unsigned engine,uintptr_t init,uintptr_t tail) {
 const unsigned bytes=engine?BCS::GPUBytes:RCS::GPUBytes;
 if(!(engine?BCS::gpuImage(m,d,b):RCS::gpuImage(m,d,b)))return -1;
 std::vector<uint8_t> before(m,m+bytes);auto frame=std::make_unique<RingImage::Frame>();auto saved=std::make_unique<RingImage::Data>();
 RingImage::Methods methods{reinterpret_cast<void (*)(void *)>(init),reinterpret_cast<void (*)(void *,uint32_t)>(tail),nullptr,nullptr};
 bool ok=RingImage::build(*frame,*saved,m,bytes,b+(engine?3:24)*4096,engine?BCS::GPUTail:RCS::GPUTail,engine,methods);
 return ok && !memcmp(before.data(),m,bytes) && frame->queries[0]==1 && !frame->queries[1];
}
extern "C" int setup(uintptr_t mmio,uint32_t gpu,unsigned engine,uintptr_t mode,uintptr_t hws) {
 auto frame=std::make_unique<RingImage::Frame>();auto saved=std::make_unique<RingImage::Data>();
 RingImage::Methods methods{nullptr,nullptr,reinterpret_cast<void (*)(void *)>(mode),reinterpret_cast<void (*)(void *)>(hws)};
 return RingImage::setup(*frame,*saved,mmio,gpu,engine,methods,gpu+4096);
}
'''
with tempfile.TemporaryDirectory(prefix='rplp-ring-native-') as td:
 p=Path(td);(p/'test.cpp').write_text(cpp)
 subprocess.run(['clang++','-std=c++17','-O1','-dynamiclib','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test.dylib')],check=True)
 lib=C.CDLL(str(p/'test.dylib'));lib.fields.argtypes=[C.c_void_p,C.c_void_p,C.c_uint32,C.c_uint,C.c_void_p,C.c_void_p];lib.fields.restype=C.c_int
 lib.setup.argtypes=[C.c_size_t,C.c_uint32,C.c_uint,C.c_void_p,C.c_void_p];lib.setup.restype=C.c_int
 cases=0
 for engine,pages in ((0,32),(2,16)):
  for gpu in (0xa00000,0xa10000,0x3e00000):
   allocation=C.create_string_buffer(pages*4096+128);C.memset(allocation,0xa5,len(allocation));mem=C.addressof(allocation)+64
   dma=(C.c_uint64*pages)(*(0x100000000+8192*i for i in range(pages)))
   assert lib.fields(mem,dma,gpu,engine,base+0x7c46c,base+0x7c5b6)==1
   assert allocation.raw[:64]==b'\xa5'*64 and allocation.raw[-64:]==b'\xa5'*64
   shift.value=13
   assert lib.fields(mem,dma,gpu,engine,base+0x7c46c,base+0x7c5b6)==0
   shift.value=12
   mmio=mmap.mmap(-1,0x1000000,prot=3);mm=C.addressof(C.c_char.from_buffer(mmio))
   assert lib.setup(mm,gpu,engine,base+0x28e1a,base+0x28f34)==1
   expected=bytearray(len(mmio))
   for o,v in ((0x229c if engine==0 else 0x2229c,0x80000),(0x14800,0x10000),(0x2080 if engine==0 else 0x22080,gpu),(0x18080,gpu+4096)):
    struct.pack_into('<I',expected,o,v)
   assert mmio[:]==expected
   mmio.close();cases+=1
image.close()
print(f'PASS production ring adapter + exact native context/mode/HWS bodies: {cases} engine/address cases, full Gen12 template equality, page-shift refusal, guards, exact four-register RAM footprint; no hardware access')
