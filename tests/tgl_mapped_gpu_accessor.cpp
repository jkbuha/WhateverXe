#include "kern_tgl_hang_resolve.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <sys/mman.h>
using namespace TGLHangResolve;
int main(){
 constexpr uint64_t K=0xffff800000000000ULL,VT=K+0x1000,Getter=K+0x2000,Slot=K+0x3000;
 uint8_t getter[14];memcpy(getter,GPUGetterBytes,14);bool thunk=false,badSlot=false;
 auto read=[&](uint64_t at,void*p,unsigned n)->uint64_t{
  if(at==VT+0x128 && n==8){const auto target=Getter;memcpy(p,&target,8);return 8;}
  if(at==Getter && n==14){memcpy(p,getter,14);if(thunk){auto b=static_cast<uint8_t*>(p);b[0]=0xff;b[1]=0x25;int32_t delta=Slot-Getter-6;memcpy(b+2,&delta,4);}return 14;}
  if(at==Slot && n==8 && !badSlot){const auto target=Getter+0x100;memcpy(p,&target,8);return 8;}
  if(at==Getter+0x100 && n==14){memcpy(p,getter,14);return 14;}
  return 0;
 };
 uint64_t target=0;assert(gpuABI(VT,target,read) && target==Getter);unsigned cases=1;
 for(unsigned i=0;i<14;++i){getter[i]^=1;assert(!gpuABI(VT,target,read));getter[i]^=1;++cases;}
 thunk=true;assert(gpuABI(VT,target,read) && target==Getter+0x100);++cases;badSlot=true;assert(!gpuABI(VT,target,read));++cases;
 assert(!gpuABI(0,target,read));++cases;
 // Execute the complete donor accessor, including its vtable dispatch, and
 // the actual Family2 fast getter in private CPU memory. Never materialize.
 void *page=mmap(nullptr,4096,7,MAP_ANON|MAP_PRIVATE,-1,0);assert(page!=MAP_FAILED);
 memcpy(page,GPUAccessorBytes,sizeof(GPUAccessorBytes));memcpy(static_cast<uint8_t*>(page)+64,GPUGetterBytes,14);
 uint64_t vt[40]{};vt[0x128/8]=reinterpret_cast<uint64_t>(page)+64;
 std::array<uint8_t,0x128> map{};std::array<uint8_t,64> object{};
 const auto mapPointer=reinterpret_cast<uint64_t>(map.data()),vtable=reinterpret_cast<uint64_t>(vt);
 memcpy(map.data(),&vtable,8);memcpy(object.data()+0x30,&mapPointer,8);
 auto get=reinterpret_cast<uint64_t(*)(void*)>(page);
 for(unsigned flags=0;flags<256;++flags)if(!(flags&0x40)){
  map[0x10]=flags;
  for(uint64_t value:{0ULL,0x4113c000ULL,0x123456789000ULL,0x7ffffffff000ULL,0xffffffffffffffffULL}){
   memcpy(map.data()+0x98,&value,8);assert(get(object.data())==value);++cases;
  }
 }
 munmap(page,4096);printf("PASS %u native accessor/fast-getter and binding guard cases; no GPU calls\n",cases);
}
