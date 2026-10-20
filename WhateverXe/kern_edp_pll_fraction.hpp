// Tiger Lake 38.4 MHz Combo PLL fractional-divider workaround.
// Linux v6.12 intel_dpll_mgr.c: ehl_combo_pll_div_frac_wa_needed(),
// icl_calc_dpll_state(). This private trial admits only the captured DPLL0 value.
#ifndef kern_edp_pll_fraction_hpp
#define kern_edp_pll_fraction_hpp
#include <stdint.h>
#include <stddef.h>
namespace EdpPLLFrac {
inline bool donorMatches(const uint8_t *image,size_t size,size_t symbol) {
 const uint8_t head[]={0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x50,0x49,0x89,0xcf,0x49,0x89,0xd4};
 if(!image || symbol!=0x5063E || size<symbol+sizeof(head))return false;
 for(size_t i=0;i<sizeof(head);++i)if(image[symbol+i]!=head[i])return false;
 return true;
}
struct Scope {uintptr_t thread=0,access=0;uint32_t applied=0;};
inline bool begin(Scope &s,uintptr_t thread,uintptr_t access) {
 if(thread<=1 || !access)return false;
 uintptr_t expected=0;
 if(!__atomic_compare_exchange_n(&s.thread,&expected,uintptr_t(1),false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))return false;
 __atomic_store_n(&s.access,access,__ATOMIC_RELAXED);
 __atomic_store_n(&s.thread,thread,__ATOMIC_RELEASE);return true;
}
inline void end(Scope &s,uintptr_t thread) {
 if(__atomic_load_n(&s.thread,__ATOMIC_ACQUIRE)!=thread)return;
 __atomic_store_n(&s.access,uintptr_t(0),__ATOMIC_RELAXED);
 __atomic_store_n(&s.thread,uintptr_t(0),__ATOMIC_RELEASE);
}
inline uint32_t writeValue(Scope &s,uintptr_t thread,uintptr_t access,uint64_t reg,uint32_t value) {
 if(thread<=1 || __atomic_load_n(&s.thread,__ATOMIC_ACQUIRE)!=thread ||
    __atomic_load_n(&s.access,__ATOMIC_RELAXED)!=access || reg!=0x164284 || value!=0x01C001A5U)return value;
 __atomic_add_fetch(&s.applied,1U,__ATOMIC_RELAXED);return 0x00E001A5U;
}
}
#endif
