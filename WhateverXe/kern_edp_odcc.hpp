// TGL main-link TX clock initialization: Linux v6.12 intel_combo_phy.c
// icl_combo_phys_init()/icl_combo_phy_verify_state() and intel_combo_phy_regs.h.
// Set ODCC clock select and DIV2, preserving all other bits per lane.
#ifndef kern_edp_odcc_hpp
#define kern_edp_odcc_hpp
#include "kern_edp_pll_fraction.hpp"
namespace EdpODCC {
template<class Read,class Write>
bool apply(EdpPLLFrac::Scope &scope,uintptr_t thread,uintptr_t access,Read read,Write write,uint32_t (&q)[24]) {
 if(thread<=1 || !access || __atomic_load_n(&scope.thread,__ATOMIC_ACQUIRE)!=thread ||
    __atomic_load_n(&scope.access,__ATOMIC_RELAXED)!=access)return false;
 uint32_t before[4];
 for(unsigned lane=0;lane<4;++lane){before[lane]=read(0x1628A0+0x100*lane);if(before[lane]==0xFFFFFFFFU)return false;}
 q[0]=1;q[3]=0x4F444343;
 for(unsigned lane=0;lane<4;++lane){
  const auto reg=0x1628A0+0x100*lane;const auto wanted=(before[lane]&~0xE0000000U)|0xA0000000U;
  if(before[lane]!=wanted){write(reg,wanted);q[1]|=1U<<lane;}
  const auto after=read(reg);if(after==wanted)q[2]|=1U<<lane;
  q[4+4*lane]=reg;q[5+4*lane]=before[lane];q[6+4*lane]=wanted;q[7+4*lane]=after;
 }
 return true;
}
}
#endif
