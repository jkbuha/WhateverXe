// Bounded read-only handoff evidence. No register writes or registry calls here.
#ifndef kern_edp_handoff_hpp
#define kern_edp_handoff_hpp
#include <stdint.h>
namespace EdpHandoff {
static constexpr uint32_t regs[]={0x45404,0x45454,0x45440,0x45444,0x51004,0x64000,0x60540,0x60400,0x46140,0x70008,0x46010,0x164280,0x164284,0x164288,0xC7200,0xC7204,0xC7208,0xC720C,0xC8250,0xC8254,0xC8258};
struct Slot {uint32_t state;uint32_t q[96];};
struct Evidence {Slot slots[4];};
template<class Read> bool capture(Evidence &e,unsigned phase,bool qualified,uint32_t trigger,uint32_t value,uint64_t caller,const uint32_t (&software)[8],Read read){
 if(!qualified || phase>=4)return false;
 if(__atomic_load_n(&e.slots[phase].state,__ATOMIC_ACQUIRE)!=0)return false;
 // Existing display PW1 and DDI-A I/O power must already be acknowledged.
 // Never request a power well, issue AUX, or sample a PHY lane here.
 const auto pw=read(0x45404),io=read(0x45454);
 if(pw==0xFFFFFFFFU || io==0xFFFFFFFFU || (pw&3U)!=3U || (io&3U)!=3U)return false;
 auto &s=e.slots[phase];uint32_t empty=0;
 if(!__atomic_compare_exchange_n(&s.state,&empty,1,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return false;
 auto q=s.q;q[0]=1;q[1]=phase;q[2]=trigger;q[3]=value;q[4]=uint32_t(caller);q[5]=uint32_t(caller>>32);q[6]=sizeof(regs)/sizeof(regs[0]);
 for(unsigned i=0;i<8;++i)q[8+i]=software[i];
 for(unsigned i=0;i<q[6];++i){q[16+2*i]=regs[i];q[17+2*i]=read(regs[i]);}
 q[95]=0x48414E44;__atomic_store_n(&s.state,2U,__ATOMIC_RELEASE);return true;
}
}
#endif
