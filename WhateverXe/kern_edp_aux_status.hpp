// Bounded per-call CPU observation of existing native AUX control accesses.
#ifndef kern_edp_aux_status_hpp
#define kern_edp_aux_status_hpp
#include "kern_edp_pll_fraction.hpp"
namespace EdpAuxStatus {
struct Scope {EdpPLLFrac::Scope owner;uint32_t first=0,last=0,reads=0,write=0,writes=0;};
inline bool begin(Scope &s,uintptr_t thread,uintptr_t access) {
 if(!EdpPLLFrac::begin(s.owner,thread,access))return false;
 s.first=s.last=s.reads=s.write=s.writes=0;return true;
}
inline void sample(Scope &s,uintptr_t thread,uintptr_t access,uint64_t reg,uint32_t value,bool write) {
 if(thread<=1 || reg!=0x64010 || __atomic_load_n(&s.owner.thread,__ATOMIC_ACQUIRE)!=thread ||
    __atomic_load_n(&s.owner.access,__ATOMIC_RELAXED)!=access)return;
 if(write){s.write=value;++s.writes;}
 else {if(!s.reads)s.first=value;s.last=value;++s.reads;}
}
inline void end(Scope &s,uintptr_t thread,uint64_t *record) {
 if(__atomic_load_n(&s.owner.thread,__ATOMIC_ACQUIRE)!=thread)return;
 record[0]=s.first;record[1]=s.last;record[2]=s.reads;record[3]=s.write;record[4]=s.writes;record[5]=0x41555831;
 EdpPLLFrac::end(s.owner,thread);
}
}
#endif
