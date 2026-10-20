#include "kern_tgl_hang_active.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <initializer_list>
int main() {
 constexpr uint64_t base=0xFFFF800000000000ULL,pa=0x200000,gpu=0x40007000;
 for(unsigned fault=0;fault<12;++fault) {
  TGLHangActive::Capture c{};unsigned ptes=0,phys=0,reads=0;
  auto pte=[&](uint64_t at){assert(at==gpu);++ptes;
   return fault==3?UINT64_MAX:fault==4?0ULL:((fault==9 && ptes==2)?pa+4096:pa)|1ULL;};
  auto physical=[&](uint64_t cpu,uint64_t &value){assert(cpu==base+pa);++phys;
   value=(fault==7 || (fault==10 && phys==2))?pa+4096:pa;return fault!=6;};
  auto read=[&](uint64_t cpu,void *out,unsigned n)->uint64_t {
   assert(cpu==base+pa && n==4096);++reads;memset(out,0xA5,n);return fault==8?4095:4096;};
  const uint64_t raw=fault==1?gpu:fault==2?0x100000001ULL:gpu+0x2D5;
  TGLHangActive::collect(c,fault!=0,raw,base,fault==5?base+pa+4095:base+0x1000000,pte,physical,read);
  if(fault==11) {
   assert(c.q[1]==0 && c.q[3]==gpu && c.q[4]==pa && c.q[8]==4096);
   assert(ptes==2 && phys==2 && reads==1 && c.bytes[0]==0xA5 && c.bytes[4095]==0xA5);
  } else assert(c.q[1]!=0);
  if(fault<3)assert(!ptes && !phys && !reads);
  if(fault<8 && fault>2)assert(!reads);
 }
 // Address arithmetic and missing-page guards must reject without a CPU read.
 for(uint64_t raw : {0ULL,1ULL,0x100000001ULL,UINT64_MAX}) {
  TGLHangActive::Capture c{};unsigned reads=0;
  TGLHangActive::collect(c,true,raw,base,base+0x1000000,[](uint64_t){return 1ULL;},
   [](uint64_t,uint64_t &){return false;},[&](uint64_t,void *,unsigned){++reads;return 0ULL;});
  assert(c.q[1] && !reads);
 }
 std::puts("PASS failure-only active GGTT snapshot: admission, address/bounds, missing PTE, CPU mapping, read holes, mutation and paired stability");
}
