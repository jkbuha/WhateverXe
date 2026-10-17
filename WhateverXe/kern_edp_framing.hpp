// Linux v6.12 intel_ddi_prepare_link_retrain: keep source enhanced framing
// consistent with the sink link configuration. CPU-only guarded value helper.
#ifndef kern_edp_framing_hpp
#define kern_edp_framing_hpp
#include "kern_edp_pll_fraction.hpp"
namespace EdpFraming {
inline uint32_t value(EdpPLLFrac::Scope &scope,uintptr_t thread,uintptr_t access,uint32_t reg,uint32_t before,uint8_t negotiated) {
 if(thread<=1 || !access || negotiated>1 || reg!=0x60540 ||
    __atomic_load_n(&scope.thread,__ATOMIC_ACQUIRE)!=thread ||
    __atomic_load_n(&scope.access,__ATOMIC_RELAXED)!=access ||
    before==0xFFFFFFFFU || (before&0x88000000U)!=0x80000000U)return before;
 return (before&~0x40000U)|(uint32_t(negotiated)<<18);
}
}
#endif
