#include "kern_tgl_tahoe_timing.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <fstream>
#include <iterator>
extern "C" __attribute__((naked,used)) void timingResume() {asm volatile("retq");}
extern "C" __attribute__((naked,used)) void timingEmptyTarget() {asm volatile("movq $-1, %rax\nretq");}
extern "C" {
 uintptr_t wxTahoeTimingResume=reinterpret_cast<uintptr_t>(timingResume);
 uintptr_t wxTahoeTimingEmpty=reinterpret_cast<uintptr_t>(timingEmptyTarget);
}
extern "C" __attribute__((naked,used)) void timingBridge() {asm volatile(WEX_TAHOE_TIMING_BRIDGE);}
extern "C" __attribute__((naked,used)) uint64_t invokeTiming(const uint32_t *,uint64_t) {
 asm volatile("pushq %rbx\npushq %r12\nsubq $8, %rsp\nmovq %rdi, %r12\nmovq %rsi, %rbx\ncallq _timingBridge\naddq $8, %rsp\npopq %r12\npopq %rbx\nretq");
}
int main(int argc,char **argv) {
 using namespace TGLTahoeTiming;
 assert(requested(0x9A498086,1,false,true));
 assert(!requested(0x9A498086,0,false,true));
 assert(!requested(0x9A498086,2,false,true));
 assert(!requested(0x9A408086,1,false,true));
 assert(!requested(0x9A498086,1,true,true));
 assert(!requested(0x9A498086,1,false,false));
 std::vector<uint8_t> image(resume+3,0);
 struct Site {size_t offset;const uint8_t *bytes;size_t size;};
 const uint8_t tail[]={0x48,0x89,0xC1};
 Site sites[]={{entry,timingHead,sizeof(timingHead)},{path,timingPath,sizeof(timingPath)},
  {empty,timingEmpty,sizeof(timingEmpty)},{site,timingMath,sizeof(timingMath)},{resume,tail,sizeof(tail)}};
 for(auto &s:sites) memcpy(image.data()+s.offset,s.bytes,s.size);
 assert(matches(image.data(),image.size(),entry));
 assert(!matches(nullptr,image.size(),entry));
 assert(!matches(image.data(),image.size(),entry+1));
 for(size_t n=0;n<image.size();++n) assert(!matches(image.data(),n,entry));
 for(auto &s:sites) for(size_t i=0;i<s.size;++i) {
  image[s.offset+i]^=1;assert(!matches(image.data(),image.size(),entry));image[s.offset+i]^=1;
 }
 uint32_t state[0x168/4]{};
 // Execute the production assembly itself, including the panic's zero divisor.
 for(uint32_t num: {0U,1U,2U,60000U,UINT32_MAX})
  for(uint32_t den: {0U,1U,2U,1000U,UINT32_MAX})
   for(uint64_t clock: {uint64_t(0),uint64_t(1),uint64_t(148500000),UINT64_MAX}) {
    state[0x160/4]=num;state[0x164/4]=den;
    auto got=invokeTiming(state,clock);
    auto expected=(!num || !den || !clock) ? UINT64_MAX : (uint64_t(num)*clock)/den;
    assert(got==expected);
   }
 if(argc>1) {
  std::ifstream f(argv[1],std::ios::binary);assert(f.good());
  std::vector<uint8_t> donor((std::istreambuf_iterator<char>(f)),{});
  assert(matches(donor.data(),donor.size(),entry));
  puts("PASS exact supplied donor timing sites and continuation");
 }
 puts("PASS timing guard: physical/OS scope, all truncated lengths, instruction mutations, 100 production assembly cases");
}
