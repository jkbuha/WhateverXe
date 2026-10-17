// Private TGL PHY A initialization replay, following Linux v6.12
// intel_combo_phy.c: icl_combo_phys_init/icl_set_procmon_ref_values.
// Admit only the captured 0.85V dot0 process/voltage reference table.
#ifndef kern_edp_phy_init_hpp
#define kern_edp_phy_init_hpp
#include "kern_edp_pll_fraction.hpp"
namespace EdpPHYInit {
template<class Read,class Write>
bool apply(EdpPLLFrac::Scope &scope,uintptr_t thread,uintptr_t access,Read read,Write write,uint32_t (&q)[64]) {
 if(thread<=1 || !access || __atomic_load_n(&scope.thread,__ATOMIC_ACQUIRE)!=thread ||
    __atomic_load_n(&scope.access,__ATOMIC_RELAXED)!=access)return false;
 const auto process=read(0x16210C);
 if(process==0xFFFFFFFFU || (process&0x1F000000U)!=0)return false;
 const uint32_t reads[]={0x64C00,0x1628A0,0x162804,0x162104,0x162124,0x162128,0x162120,0x162100,0x162014};
 const uint32_t writes[]={0x64C00,0x1626A0,0x162604,0x162104,0x162124,0x162128,0x162120,0x162100,0x162014};
 uint32_t before[9];for(unsigned i=0;i<9;++i){before[i]=read(reads[i]);if(before[i]==0xFFFFFFFFU)return false;}
 const uint32_t wanted[]={before[0]&~0x00800000U,(before[1]&~0xE0000000U)|0xA0000000U,
  before[2]&~0x00300000U,before[3]&~0x00FF00FFU,0x62AB67BB,0x51914F96,
  before[6]|0x01000000U,before[7]|0x80000000U,before[8]|0x10U};
 q[0]=1;q[1]=process;q[3]=9;
 // Re-emit the complete native Linux initialization order, including the
 // calibration/reference words and COMP_INIT after the clock/IREF setup.
 for(unsigned i=0;i<9;++i){
  write(writes[i],wanted[i]);const auto after=read(reads[i]);if(after==wanted[i])q[2]|=1U<<i;
  q[4+6*i]=reads[i];q[5+6*i]=writes[i];q[6+6*i]=before[i];q[7+6*i]=wanted[i];q[8+6*i]=after;
 }
 q[63]=0x50485949;return true;
}
}
#endif
