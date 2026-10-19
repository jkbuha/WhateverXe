#include "kern_tgl_c0_fresh.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <initializer_list>
static void put(uint8_t *e,unsigned o,uint32_t v){memcpy(e+o,&v,4);}
int main(){using namespace TGLC0Fresh;unsigned n=0;
 for(unsigned w:{1U,31U,32U,127U,128U,129U,511U,512U,513U,2020U,4095U,4096U})
 for(unsigned h:{1U,31U,32U,33U,511U,512U,513U,4095U,4096U}){
  uint8_t e[160]={},before[160],m[296]={};unsigned pitch=(w*4+511)&~511U,main=pitch*((h+31)&~31U);
  put(e,8,pitch);put(e,0x10,0xc0);put(e,0x14,3);put(e,0x3c,main);put(e,0x40,main/256);put(e,0x6c,w);put(e,0x70,h);put(e,0x74,1);put(e,0x78,1);memcpy(before,e,160);
  assert(nativeEligible(e)&&fresh(e));auto length=backing(pitch,h);assert(allocationLength(e)==length);
  uint64_t task=123,gpu=0x40000000;memcpy(m+0x90,&task,8);memcpy(m+0x98,&gpu,8);
  assert(mapForEntry(m,task,gpu,length,e));assert(!mapForEntry(m,task,gpu,length+4096,e));assert(!mapForEntry(m,task,gpu,length-4096,e));assert(!mapForEntry(m,task,gpu+1,length,e));assert(!mapForEntry(m,task+1,gpu,length,e));m[0x10]=64;assert(!mapForEntry(m,task,gpu,length,e));m[0x10]=0;
  for(unsigned off:{0U,8U,0x10U,0x14U,0x3cU,0x40U,0x44U,0x4cU,0x64U,0x68U,0x74U,0x78U,0x98U}){put(e,off,TGLC0Entry::word(e+off)+1);assert(!fresh(e));assert(!mapForEntry(m,task,gpu,length,e));memcpy(e,before,160);}
  e[0x4b]=8;assert(!fresh(e));memcpy(e,before,160);e[0x91]=1;assert(!fresh(e));memcpy(e,before,160);
  for(unsigned off:{0x6cU,0x70U})for(unsigned bad:{0U,4097U,0xffffffffU}){put(e,off,bad);assert(!nativeEligible(e));memcpy(e,before,160);}
  assert(disable(e,before));put(before,0x40,0);assert(!memcmp(e,before,160));++n;
 }
 assert(backing(2048,512)==1056768);printf("PASS: %u bounded native C0 geometries, exact allocation coupling, ownership flags and single-word mutation\n",n);
}
