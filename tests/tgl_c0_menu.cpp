#include "kern_tgl_c0_fresh.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
static void put(uint8_t *e,unsigned off,uint32_t n){memcpy(e+off,&n,4);}
int main(){using namespace TGLC0Fresh;unsigned tested=0;
 for(const auto &l:SmallLayouts){uint8_t e[160]={},before[160],mapBytes[296]={};const auto main=l[2]*((l[1]+31)&~31U),ccs=main/256;
  put(e,8,l[2]);put(e,0x10,0xc0);put(e,0x14,3);put(e,0x3c,main);put(e,0x40,ccs);put(e,0x6c,l[0]);put(e,0x70,l[1]);put(e,0x74,1);put(e,0x78,1);memcpy(before,e,160);
  assert(smallEligible(e) && candidate(e) && fresh(e));const auto length=backing(l[2],l[1]);assert(allocationLength(e)==length && knownBacking(length));
  const uint64_t task=0x1234,gpu=0x45670000;memcpy(mapBytes+0x90,&task,8);memcpy(mapBytes+0x98,&gpu,8);
  assert(map(mapBytes,task,gpu,length,length));assert(!map(mapBytes,task,gpu+1,length,length));assert(!map(mapBytes,task+1,gpu,length,length));assert(!map(mapBytes,task,gpu,length+4096,length));mapBytes[0x10]=0x40;assert(!map(mapBytes,task,gpu,length,length));
  for(unsigned off:{8U,0x10U,0x14U,0x3cU,0x40U,0x6cU,0x70U,0x74U,0x78U}){put(e,off,0);assert(!fresh(e));memcpy(e,before,160);}
  for(unsigned off=0x4c;off<0x6c;off+=4){put(e,off,1);assert(!fresh(e));memcpy(e,before,160);}
  for(unsigned f=1;f<256;f++){e[0x91]=f;assert(!fresh(e));}memcpy(e,before,160);e[0x4b]=8;assert(!fresh(e));memcpy(e,before,160);e[0]=1;assert(!fresh(e));memcpy(e,before,160);e[0x98]=1;assert(!fresh(e));memcpy(e,before,160);
  e[0x80]=1;assert(!disable(e,before));memcpy(e,before,160);assert(disable(e,before));put(before,0x40,0);assert(!memcmp(e,before,160) && !fresh(e));++tested;
 }
 assert(!smallEligible(nullptr));assert(!knownBacking(4096));puts("PASS: exact observed small C0 layouts, computed backing bounds, dirty/shared/mip/state guards, stale descriptor rejection; only unused CCS-size word changes before first compression enable.");assert(tested>0);
}
