#include "kern_right_configured_lanes.hpp"
#include <cassert>
#include <cstdio>
int main(){using R=RightConfiguredLanes;
 assert(R::Register==0x16e8c0);assert(R::target(0x9a498086,1,5,3,true,true,true));
 assert(!R::target(0x9a408086,1,5,3,true,true,true));assert(!R::target(0x9a498086,0,5,3,true,true,true));
 for(unsigned ddi=0;ddi<9;ddi++)if(ddi!=5)assert(!R::target(0x9a498086,1,ddi,3,true,true,true));
 for(unsigned mode=0;mode<6;mode++)if(mode!=3)assert(!R::target(0x9a498086,1,5,mode,true,true,true));
 assert(!R::target(0x9a498086,1,5,3,false,true,true));assert(!R::target(0x9a498086,1,5,3,true,false,true));assert(!R::target(0x9a498086,1,5,3,true,true,false));
 for(unsigned n=0;n<16;n++)assert(R::result(0xabcdef00|n)==n);assert(R::result(UINT32_MAX)==0);
 for(unsigned n=0;n<16;n++) {
  const auto raw=0xabcde000U|n;
  assert(R::signalMask(raw,0x1234,0x1234)==((n==1 || n==3)?15:n));
  assert(R::signalMask(raw,0x1235,0x1234)==n);
  assert(R::signalMask(raw,0,0)==n);
 }
 assert(R::signalMask(UINT32_MAX,0x1234,0x1234)==0);
 puts("PASS: physical/revision/right-port/mode/ready/route/lock guards; FIA2 physical register; unpacked four-bit return and unavailable read rejected.");}
