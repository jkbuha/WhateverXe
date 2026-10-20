#include "kern_tgl_tahoe_resolve_scratch.hpp"
#include <fstream>
#include <iterator>
#include <cstdint>
#include <cstring>
#include <cassert>
#include <cstdio>
#include <array>
extern "C" {uint32_t wxTahoeResolveScratchEnabled=1,wxTahoeResolveScratchFixes=0;uintptr_t wxTahoeResolveScratchResume=0;}
extern "C" __attribute__((naked,used)) void tglResolveScratchBridge() {asm volatile(WEX_TAHOE_RESOLVE_SCRATCH_BRIDGE);}

extern "C" __attribute__((naked)) void finish(){asm volatile("retq\n");}
extern "C" __attribute__((naked)) void native(){asm volatile("movl %r12d,%r13d\n shrl $2,%r13d\n shlq $2,%r13\n shrq $32,%r12\n movabsq $0x001014047a000004,%rax\n retq\n");}
struct Out {uint64_t high,low,header,flags;};
extern "C" __attribute__((naked)) void run(uint64_t,const uint8_t *,Out *,void *){
 asm volatile("pushq %rbp\n movq %rsp,%rbp\n pushq %r12\n pushq %r13\n pushq %r14\n pushq %rbx\n movq %rdi,%r12\n movq %rsi,%r14\n xorl %eax,%eax\n cld\n stc\n callq *%rcx\n movq %r12,(%rdx)\n movq %r13,8(%rdx)\n movq %rax,16(%rdx)\n pushfq\n popq %rcx\n movq %rcx,24(%rdx)\n popq %rbx\n popq %r14\n popq %r13\n popq %r12\n popq %rbp\n retq\n");
}
using State=std::array<uint8_t,64>;
void put(State &s,unsigned o,uint32_t v){memcpy(s.data()+o,&v,4);}
int main(int argc,char **argv){wxTahoeResolveScratchResume=reinterpret_cast<uintptr_t>(finish);State state{};
 put(state,0,0x2345f000);put(state,8,0x04af077f);put(state,12,0x1dff);put(state,16,2);put(state,24,5);unsigned cases=0;
 auto test=[&](State s,uint64_t gpu,bool enabled,bool qualifies){
  const auto before=s;const auto count=wxTahoeResolveScratchFixes;wxTahoeResolveScratchEnabled=enabled;
  Out actual{},expected{};run(gpu,s.data(),&actual,reinterpret_cast<void *>(tglResolveScratchBridge));
  run(gpu+(qualifies?0x5000:0),s.data(),&expected,reinterpret_cast<void *>(native));
  assert(actual.high==expected.high && actual.low==expected.low && actual.header==expected.header);
  assert(((actual.flags^expected.flags)&0x4c5)==0);assert(s==before);assert(wxTahoeResolveScratchFixes==count+(qualifies?1:0));++cases;
 };
 test(state,0x40b29000,true,true);test(state,0xffffc000,true,true);test(state,0x7fffffffa000,true,true);
 test(state,0x40b29000,false,false);test(state,0,true,false);test(state,0x40b29001,true,false);test(state,0x7fffffffb000,true,false);test(state,0xffff800000000000,true,false);
 for(unsigned off:{0U,8U,12U,16U,20U,24U}){auto s=state;s[off]^=1;test(s,0x40b29000,true,true);}
 // The address-only bridge must not dereference a surface at all.
 Out noSurface{};wxTahoeResolveScratchEnabled=1;run(0x40b29000,nullptr,&noSurface,reinterpret_cast<void *>(tglResolveScratchBridge));assert(noSurface.low==0x40b2e000);++cases;
 using namespace TGLTahoeResolveScratch;
 std::vector<uint8_t> image(Allocation+8,0);memcpy(image.data()+Entry,Head,sizeof(Head));
 memcpy(image.data()+Site,Original,sizeof(Original));memcpy(image.data()+Resume,Continuation,sizeof(Continuation));
 const uint64_t allocation=0x5100;memcpy(image.data()+Allocation,&allocation,8);
 assert(matches(image.data(),image.size(),Entry,Allocation));
 assert(!matches(nullptr,image.size(),Entry,Allocation));
 assert(!matches(image.data(),image.size(),Entry+1,Allocation));
 assert(!matches(image.data(),image.size(),Entry,Allocation+1));
 for(size_t n=0;n<image.size();++n)assert(!matches(image.data(),n,Entry,Allocation));
 struct Check {size_t offset,count;};
 for(auto check:{Check{Entry,sizeof(Head)},Check{Site,sizeof(Original)},Check{Resume,sizeof(Continuation)},Check{Allocation,8}})
  for(size_t i=0;i<check.count;++i){image[check.offset+i]^=1;assert(!matches(image.data(),image.size(),Entry,Allocation));image[check.offset+i]^=1;}
 if(argc>1){std::ifstream f(argv[1],std::ios::binary);assert(f.good());std::vector<uint8_t> donor((std::istreambuf_iterator<char>(f)),{});
  assert(matches(donor.data(),donor.size(),Entry,Allocation));puts("PASS supplied Tahoe donor entry, displaced block, continuation and scratch allocation constant");}
 printf("PASS: %u actual scratch-bridge replays; address relocation, 4GiB carry, zero/unaligned/noncanonical rejection, all surface shapes covered, disabled path, native registers/defined flags, unchanged descriptors and exact counter; ASan/UBSan harness.\n",cases);
}
