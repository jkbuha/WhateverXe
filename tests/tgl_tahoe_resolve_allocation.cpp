#include "kern_tgl_tahoe_resolve_allocation.hpp"
#include <vector>
#include <array>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstdio>
#include <sys/mman.h>
using namespace TGLTahoeResolveAllocation;
extern "C" __attribute__((naked)) uint64_t invokeAllocation(void*,void*){
 asm volatile("pushq %rbp\nmovq %rsp,%rbp\npushq %r12\npushq %r14\nmovq %rdi,%r12\nxorl %r14d,%r14d\ncallq *%rsi\npopq %r14\npopq %r12\npopq %rbp\nretq\n");
}
int main(int argc,char **argv){
 std::vector<uint8_t> image(ParamsSize+9);const uint64_t native=NativeSize;
 memcpy(image.data()+TGLTahoeResolveScratch::Allocation,&native,8);memcpy(image.data()+Constructor,ConstructorBytes,sizeof(ConstructorBytes));memcpy(image.data()+Allocate,AllocateBytes,sizeof(AllocateBytes));memcpy(image.data()+ParamsReference,ParamsBytes,sizeof(ParamsBytes));
 auto planNow=[&](){return plan(image.data(),image.size(),TGLTahoeResolveScratch::Allocation,Params,WithOptions);};
 assert(planNow().valid && planNow().params==0);
 memcpy(image.data()+ParamsSize,&native,8);assert(planNow().valid && planNow().params==NativeSize);
 assert(!plan(nullptr,image.size(),TGLTahoeResolveScratch::Allocation,Params,WithOptions).valid);
 for(size_t n=0;n<image.size();++n)assert(!plan(image.data(),n,TGLTahoeResolveScratch::Allocation,Params,WithOptions).valid);
 for(unsigned bad=0;bad<3;++bad)assert(!plan(image.data(),image.size(),TGLTahoeResolveScratch::Allocation+(bad==0),Params+(bad==1),WithOptions+(bad==2)).valid);
 for(uint64_t size:{1ULL,0x5000ULL,0x5101ULL,0x6000ULL,~0ULL}){memcpy(image.data()+ParamsSize,&size,8);assert(!planNow().valid);}memcpy(image.data()+ParamsSize,&native,8);
 struct Region{size_t at,len;};for(auto r:{Region{Constructor,sizeof(ConstructorBytes)},Region{Allocate,sizeof(AllocateBytes)},Region{ParamsReference,sizeof(ParamsBytes)},Region{TGLTahoeResolveScratch::Allocation,8}})
  for(unsigned i=0;i<r.len;++i){image[r.at+i]^=1;assert(!planNow().valid);image[r.at+i]^=1;}
 if(argc>1){std::ifstream f(argv[1],std::ios::binary);assert(f.good());std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});assert(plan(b.data(),b.size(),TGLTahoeResolveScratch::Allocation,Params,WithOptions).valid);}
 // Production transaction, including a writer that changes bytes but then
 // reports failure. Only the two admitted size words may change.
 unsigned transactions=0;
 for(uint64_t initial:{0ULL,uint64_t(NativeSize)})for(unsigned failAt=0;failAt<=2;++failAt)for(bool wroteBeforeFailure:{false,true}){
  memcpy(image.data()+ParamsSize,&initial,8);const auto before=image;const auto p=planNow();unsigned calls=0;
  auto writer=[&](size_t at,uint64_t old,uint64_t value){uint64_t actual=0;memcpy(&actual,image.data()+at,8);assert(actual==old);const bool fail=++calls==failAt;
   if(!fail || wroteBeforeFailure)memcpy(image.data()+at,&value,8);return !fail;
  };
  const auto ok=apply(image.data(),p,writer);assert(ok==(failAt==0));
  if(!ok)assert(image==before);
  else{auto expected=before;const uint64_t rounded=RoundedSize;memcpy(expected.data()+TGLTahoeResolveScratch::Allocation,&rounded,8);memcpy(expected.data()+ParamsSize,&rounded,8);assert(image==expected);}
  image=before;++transactions;
 }
 // Replay actual constructor copy and actual native allocation argument path.
 // Native allocation itself is replaced by a CPU-only requested-length echo.
 auto mem=static_cast<uint8_t*>(mmap(nullptr,ParamsSize+4096,7,MAP_ANON|MAP_PRIVATE,-1,0));assert(mem!=MAP_FAILED);
 memcpy(mem+Constructor,ConstructorBytes,sizeof(ConstructorBytes));mem[Constructor+sizeof(ConstructorBytes)]=0xc3;
 memcpy(mem+Allocate,AllocateBytes,sizeof(AllocateBytes));mem[Allocate+sizeof(AllocateBytes)]=0xc3;
 const uint8_t echo[]={0x48,0x89,0xF0,0xC3};memcpy(mem+WithOptions,echo,sizeof(echo));
 auto ctor=reinterpret_cast<void(*)()>(mem+Constructor);unsigned cases=0;
 for(bool ctorFirst:{false,true})for(bool patched:{false,true}){
  uint64_t value=NativeSize,zero=0;memcpy(mem+TGLTahoeResolveScratch::Allocation,&value,8);memcpy(mem+ParamsSize,&zero,8);
  if(ctorFirst)ctor();
  if(patched){value=RoundedSize;memcpy(mem+TGLTahoeResolveScratch::Allocation,&value,8);memcpy(mem+ParamsSize,&value,8);}
  if(!ctorFirst)ctor();
  assert(invokeAllocation(mem+Params,mem+Allocate)==value);++cases;
 }
 munmap(mem,ParamsSize+4096);
 assert(NativeSize/4096==5 && RoundedSize/4096==6 && 0x5000+8<=RoundedSize);
 printf("PASS exact extent guards/mutations/truncations, %u transactional success/failure cases and 4 actual native constructor/allocation-argument replays; no GPU/allocation calls\n",transactions);
}
