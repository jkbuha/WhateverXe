// Private, one-shot DDI A enable/AUX correlation experiment.
// Native LightUpEDP scope qualifies physical identity and power ownership.
#ifndef kern_edp_enable_cycle_hpp
#define kern_edp_enable_cycle_hpp
#include "kern_edp_pll_fraction.hpp"
namespace EdpEnableCycle {
struct Report {uint64_t q[16] {};};
template<class Read,class Write,class Aux,class Delay>
bool probe(EdpPLLFrac::Scope &scope,uint32_t &once,uintptr_t thread,void *port,uint32_t original,
 Read read,Write write,Aux aux,Delay delay,Report &report) {
 auto pb=static_cast<uint8_t *>(port);auto config=pb?*reinterpret_cast<uint32_t **>(pb+0x548):nullptr;
 if(original || thread<=1 || !config || config[0]!=0 || config[2]!=0 || config[3]!=0 || pb[0x11]!=2 ||
    *reinterpret_cast<uint32_t *>(pb+0x550)!=1 || *reinterpret_cast<uint32_t *>(pb+0x124)!=2 ||
    *reinterpret_cast<uint32_t *>(pb+0x128)!=10 || *reinterpret_cast<uint32_t *>(pb+0x590)!=0x64000 ||
    __atomic_load_n(&scope.thread,__ATOMIC_ACQUIRE)!=thread ||
    __atomic_load_n(&scope.access,__ATOMIC_RELAXED)!=reinterpret_cast<uintptr_t>(*reinterpret_cast<void **>(pb+0x578)) ||
    __atomic_load_n(&once,__ATOMIC_RELAXED))return false;
 const auto before=read(0x64000);
 // Admit only the captured two-lane, enabled and non-idle DDI A state.
 if(before!=0x80000003U)return false;
 uint32_t expected=0;if(!__atomic_compare_exchange_n(&once,&expected,1U,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return false;
 auto &q=report.q;q[0]=1;q[1]=before;q[2]=0x64000;q[15]=0x4359434C;
 write(0x64000,before&~0x80000000U);
 uint32_t off=read(0x64000),offWait=0;
 while(off!=0xFFFFFFFFU && (off&0x80000080U)!=0x80U && offWait<1000){delay(10);offWait+=10;off=read(0x64000);}
 q[3]=off;q[4]=offWait;q[5]=0xE00002D8U;q[6]=0xFF;
 if(off!=0xFFFFFFFFU && (off&0x80000080U)==0x80U){uint8_t power=0xFF;q[5]=aux(&power);q[6]=power;}
 // Always restore the exact enabled request after our temporary disable,
 // including when the idle transition or the AUX read failed.
 write(0x64000,before);
 uint32_t on=read(0x64000),onWait=0;
 while(on!=0xFFFFFFFFU && (on&0x80000080U)!=0x80000000U && onWait<1000){delay(10);onWait+=10;on=read(0x64000);}
 q[7]=on;q[8]=onWait;q[9]=0xE00002D8U;q[10]=0xFF;
 if(on!=0xFFFFFFFFU && (on&0x80000080U)==0x80000000U){uint8_t power=0xFF;q[9]=aux(&power);q[10]=power;}
 q[11]=original;q[12]=1;return true;
}
}
#endif
