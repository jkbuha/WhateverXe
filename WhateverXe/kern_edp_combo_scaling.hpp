// Internal TGL Combo PHY trial. Linux v6.12 intel_ddi.c,
// icl_ddi_combo_vswing_program(): SCALING_MODE_SEL(0x2).
// Match only the captured donor field 6; preserve training enable, taps,
// termination and every unrelated bit. No hardware access in this helper.
#ifndef kern_edp_combo_scaling_hpp
#define kern_edp_combo_scaling_hpp
#include "kern_edp_pll_fraction.hpp"
namespace EdpComboScaling {
inline uint32_t writeValue(EdpPLLFrac::Scope &scope,uintptr_t thread,uintptr_t access,uint64_t reg,uint32_t value) {
 if(thread<=1 || __atomic_load_n(&scope.thread,__ATOMIC_ACQUIRE)!=thread ||
    __atomic_load_n(&scope.access,__ATOMIC_RELAXED)!=access || reg!=0x162694 ||
    (value&0x001C0000U)!=0x00180000U)return value;
 return (value&~0x001C0000U)|0x00080000U;
}
}
#endif
