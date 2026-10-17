// Private diagnostic: do not replay PHY initialization inside a powered modeset.
// Intel TGL display PRM, Combo PHY Initialization Sequence: skip initialized
// PHYs; initialization otherwise belongs before DDI/AUX I/O power is enabled.
#ifndef kern_edp_phy_init_hpp
#define kern_edp_phy_init_hpp
#include "kern_edp_pll_fraction.hpp"
namespace EdpPHYInit {
template<class Read,class Write>
bool apply(EdpPLLFrac::Scope &scope,uintptr_t thread,uintptr_t access,Read read,Write,uint32_t (&q)[64]) {
 if(thread<=1 || !access || __atomic_load_n(&scope.thread,__ATOMIC_ACQUIRE)!=thread ||
    __atomic_load_n(&scope.access,__ATOMIC_RELAXED)!=access)return false;
 const auto comp=read(0x162100);if(comp==0xFFFFFFFFU)return false;
 q[0]=2;q[1]=comp;q[2]=0;q[3]=0;
 q[4]=(comp&0x80000000U)?1:2; // preserved initialized / no unsafe late initialization
 q[63]=0x50485949;return true;
}
}
#endif
