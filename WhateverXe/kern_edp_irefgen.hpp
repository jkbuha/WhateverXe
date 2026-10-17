// TGL PHY A is a reference-current master. Linux v6.12 intel_combo_phy.c
// phy_is_master()/icl_combo_phys_init(): COMP_DW8.IREFGEN must be set.
// Private trial: admit only this machine's captured calibration word.
#ifndef kern_edp_irefgen_hpp
#define kern_edp_irefgen_hpp
#include "kern_edp_pll_fraction.hpp"
namespace EdpIrefgen {
template<class Read,class Write>
bool apply(EdpPLLFrac::Scope &scope,uintptr_t thread,uintptr_t access,Read read,Write write,uint32_t (&q)[8]) {
 if(thread<=1 || !access || __atomic_load_n(&scope.thread,__ATOMIC_ACQUIRE)!=thread ||
    __atomic_load_n(&scope.access,__ATOMIC_RELAXED)!=access)return false;
 const auto before=read(0x162120);
 if(before!=0x000D0280U && before!=0x010D0280U)return false;
 const auto wanted=before|0x01000000U;
 if(wanted!=before)write(0x162120,wanted);
 const auto after=read(0x162120);
 q[0]=1;q[1]=before;q[2]=wanted;q[3]=after;q[4]=before!=wanted;q[5]=after==wanted;q[6]=0x49524546;
 return true;
}
}
#endif
