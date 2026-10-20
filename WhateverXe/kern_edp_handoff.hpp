// Bounded read-only handoff evidence. No register writes or registry calls here.
#ifndef kern_edp_handoff_hpp
#define kern_edp_handoff_hpp
#include <stdint.h>
namespace EdpHandoff {
static constexpr uint32_t regs[]={0x45404,0x45454,0x45440,0x45444,0x51004,0x64000,0x60540,0x60400,0x46140,0x70008,0x46010,0x164280,0x164284,0x164288,0xC7200,0xC7204,0xC7208,0xC720C,0xC8250,0xC8254,0xC8258,0x64c00,0x162014,0x162028,0x162030,0x162100,0x162104,0x16210c,0x162120,0x162124,0x162128,0x162304,0x162388,0x162390,0x162394,0x162398,0x16239c,0x1623a0,0x162888,0x162890,0x162894,0x16289c,0x1628a0,0x162988,0x162990,0x162994,0x16299c,0x1629a0,0x162a88,0x162a90,0x162a94,0x162a9c,0x162aa0,0x162b88,0x162b90,0x162b94,0x162b9c,0x162ba0};
struct Slot {uint32_t state;uint32_t q[160];};
struct Evidence {Slot slots[4];};
template<class Read> bool capture(Evidence &e,unsigned phase,bool qualified,uint32_t trigger,uint32_t value,uint64_t caller,const uint32_t (&software)[8],Read read){
 if(!qualified || phase>=4)return false;
 if(__atomic_load_n(&e.slots[phase].state,__ATOMIC_ACQUIRE)!=0)return false;
 // Existing display PW1 and DDI-A I/O power must already be acknowledged.
 // Never request a power well, issue AUX, or sample a PHY lane here.
 const auto pw=read(0x45404),io=read(0x45454);
 if(pw==0xFFFFFFFFU || io==0xFFFFFFFFU || (pw&0x3FU)!=0x3FU || (io&3U)!=3U)return false;
 auto &s=e.slots[phase];uint32_t empty=0;
 if(!__atomic_compare_exchange_n(&s.state,&empty,1,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))return false;
 auto q=s.q;q[0]=2;q[1]=phase;q[2]=trigger;q[3]=value;q[4]=uint32_t(caller);q[5]=uint32_t(caller>>32);q[6]=sizeof(regs)/sizeof(regs[0]);
 for(unsigned i=0;i<8;++i)q[8+i]=software[i];
 const auto aux=read(0x45444);
 for(unsigned i=0;i<q[6];++i){q[16+2*i]=regs[i];
  // AUX lane registers are only readable under their existing power ACK.
  const bool auxLane=regs[i]>=0x162300U && regs[i]<0x162400U;
  q[17+2*i]=auxLane && (aux==0xFFFFFFFFU || (aux&3U)!=3U)?0xFFFFFFFFU:read(regs[i]);}
 q[159]=0x48414E44;__atomic_store_n(&s.state,2U,__ATOMIC_RELEASE);return true;
}
}
#endif
