#ifndef kern_tgl_observation_budget_hpp
#define kern_tgl_observation_budget_hpp
#include <stdint.h>
namespace TGLObservationBudget {
constexpr unsigned Tasks=64,Limit=64;
constexpr uint64_t IntervalNS=250000000;
struct Slot {uint64_t task {},calls {},admitted {},nextNS {};};
struct Budget {
 Slot slots[Tasks] {};
 bool allow(uint64_t task,uint64_t now){
  if(!task)return false;
  for(auto &s:slots){
   auto owner=__atomic_load_n(&s.task,__ATOMIC_ACQUIRE);
   if(!owner){uint64_t empty=0;if(__atomic_compare_exchange_n(&s.task,&empty,task,false,__ATOMIC_ACQ_REL,__ATOMIC_ACQUIRE))owner=task;else owner=empty;}
   if(owner==task){
    __atomic_fetch_add(&s.calls,1ULL,__ATOMIC_RELAXED);
    if(__atomic_load_n(&s.admitted,__ATOMIC_RELAXED)>=Limit || now>UINT64_MAX-IntervalNS)return false;
    auto next=__atomic_load_n(&s.nextNS,__ATOMIC_RELAXED);
    if(now<next || !__atomic_compare_exchange_n(&s.nextNS,&next,now+IntervalNS,false,__ATOMIC_RELAXED,__ATOMIC_RELAXED))return false;
    return __atomic_fetch_add(&s.admitted,1ULL,__ATOMIC_RELAXED)<Limit;
   }
  }
  return false;
 }
};
}
#endif
