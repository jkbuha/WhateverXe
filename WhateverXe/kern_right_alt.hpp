// Private bounded DP-alt trial. This policy never acquires power or touches MMIO.
#ifndef kern_right_alt_hpp
#define kern_right_alt_hpp
#include <stdint.h>
struct RightAlt {
 static bool accept(uint32_t physical,uint32_t ddi,bool cold,bool powered,
                    uint32_t fia1,uint32_t live,uint32_t ready,uint32_t pin,
                    uint32_t buffer,uint32_t mode) {
  if(physical!=0x9A498086U || ddi!=5 || !cold || !powered ||
     fia1==UINT32_MAX || !(fia1&0x10) || live==UINT32_MAX || ready==UINT32_MAX ||
     pin==UINT32_MAX || buffer==UINT32_MAX || (mode!=0 && mode!=3))return false;
  // Only the measured two-lane pin-D transport; neither TBT nor legacy.
  if((live&0x60)!=0x20 || !(ready&1) || (pin&15)!=4)return false;
  if((live&15)!=3 && (live&15)!=12)return false;
  // Software mode0 must not take over inherited enabled scanout.
  return mode==3 || !(buffer&0x80000000U);
 }
 static uint32_t lanePhysical(uint32_t old,uint32_t lanes){return (old&~15U)|(lanes==1?1U:3U);}
};
#endif
