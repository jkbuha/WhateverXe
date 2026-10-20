// The captured two-lane panel starts TPS1 and lane state together.
#pragma once
#include "kern_edp_pll_fraction.hpp"
namespace EdpTrainingPacket {
inline unsigned prepare(EdpPLLFrac::Scope &scope,uintptr_t thread,uintptr_t access,bool qualified,
 uint32_t address,void *&data,uint32_t &length,uint8_t (&start)[3]) {
 if(!qualified || !data || thread<=1 || !access ||
  __atomic_load_n(&scope.thread,__ATOMIC_ACQUIRE)!=thread ||
  __atomic_load_n(&scope.access,__ATOMIC_RELAXED)!=access)return 0;
 if(address==0x102 && length==1 && *static_cast<uint8_t *>(data)==0x21) {
  start[0]=0x21;start[1]=start[2]=0;data=start;length=3;return 1;
 }
 if(address==0x103 && length==4){length=2;return 2;}
 return 0;
}
}
