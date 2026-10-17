// Read-only snapshots inside an existing native AUX power reference.
#ifndef kern_edp_aux_phy_hpp
#define kern_edp_aux_phy_hpp
#include "kern_edp_pll_fraction.hpp"
namespace EdpAuxPHY {
template<class Read>
bool capture(EdpPLLFrac::Scope &scope,uint32_t &count,uintptr_t thread,uintptr_t access,uint32_t header,uint32_t size,Read read,uint64_t (&q)[64]) {
 if(thread<=1 || !access || header!=0x90060000U || size<0x164294 ||
    __atomic_load_n(&scope.thread,__ATOMIC_ACQUIRE)!=thread ||
    __atomic_load_n(&scope.access,__ATOMIC_RELAXED)!=access || __atomic_load_n(&count,__ATOMIC_RELAXED)>=16)return false;
 const auto main=read(0x45404),io=read(0x45454);
 if(main==0xFFFFFFFFU || io==0xFFFFFFFFU || (main&0x3FU)!=0x3FU || (io&3U)!=3U)return false;
 const auto sequence=__atomic_add_fetch(&count,1U,__ATOMIC_RELAXED);if(sequence>16)return false;
 const uint32_t regs[]={0x64000,0x164280,0x46010,0x164284,0x164288,
  0x162014,0x162028,0x162030,0x162100,0x162104,0x16210C,0x162120,0x162124,0x162128,
  0x162304,0x162388,0x162390,0x162394,0x162398,0x16239C,0x1623A0};
 q[0]=1;q[1]=sequence;q[2]=header;q[4]=sizeof(regs)/sizeof(regs[0]);q[5]=main;q[6]=io;
 for(unsigned i=0;i<q[4];++i){q[8+2*i]=regs[i];q[9+2*i]=read(regs[i]);}
 return true;
}
}
#endif
