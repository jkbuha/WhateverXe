// Private diagnostic: bounded immutable samples, no MMIO, locks or allocation.
#ifndef kern_right_port_observer_hpp
#define kern_right_port_observer_hpp
#include <stdint.h>
struct RightPortObserver {
 static constexpr unsigned Kinds=12, Slots=4, Words=8, Capacity=Kinds*Slots;
 struct Slot {uint32_t ready {},q[Words] {};};
 Slot slots[Capacity] {};uint32_t counts[Kinds] {},sequence {};
 void record(uint32_t kind,uint32_t ddi,uint32_t mode,uint32_t a,uint32_t b,uint32_t c,uint32_t d) {
  if(ddi!=5 || kind>=Kinds)return;
  const auto n=__atomic_fetch_add(&counts[kind],1U,__ATOMIC_RELAXED);
  if(n>=Slots)return;
  auto &s=slots[kind*Slots+n];
  s.q[0]=__atomic_add_fetch(&sequence,1U,__ATOMIC_RELAXED);
  s.q[1]=kind;s.q[2]=ddi;s.q[3]=mode;s.q[4]=a;s.q[5]=b;s.q[6]=c;s.q[7]=d;
  __atomic_store_n(&s.ready,1U,__ATOMIC_RELEASE);
 }
 unsigned snapshot(uint32_t *out,unsigned capacityWords) const {
  unsigned n=0;
  for(const auto &s:slots) {
   if(!__atomic_load_n(&s.ready,__ATOMIC_ACQUIRE))continue;
   if(capacityWords-n<Words)break;
   for(unsigned j=0;j<Words;++j)out[n++]=s.q[j];
  }
  return n;
 }
};
#endif
