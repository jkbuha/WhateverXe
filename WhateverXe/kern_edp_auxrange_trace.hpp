// Bounded immutable CPU receipts for exact native AUX map/unmap operations.
#ifndef kern_edp_auxrange_trace_hpp
#define kern_edp_auxrange_trace_hpp
#include <stdint.h>
#include <string.h>
namespace EdpAuxRanges {
constexpr unsigned MaxRecords=128;
struct Record {uint64_t q[16];};
struct Trace {
 uint32_t count {};Record records[MaxRecords] {};
 template<class Read,class Clock>
 uint32_t begin(Record &r,unsigned operation,uint64_t object,uint64_t range,uint64_t entry,
  uint64_t caller,uint64_t thread,uint64_t auxVtable,uint64_t mapVtable,Read read,Clock clock){
  const auto slot=__atomic_fetch_add(&count,1U,__ATOMIC_RELAXED);if(slot>=MaxRecords)return slot;
  auto q=r.q;q[0]=slot+1;q[1]=operation;q[2]=object;q[7]=entry;q[8]=caller;q[9]=thread;clock(q+10);
  uint64_t span[2] {};if(read(range,span,sizeof(span))==sizeof(span)){q[5]=span[0];q[6]=span[1];q[13]|=1;}
  uint8_t aux[0x50] {};uint64_t vt=0;
  if(read(object,&vt,8)==8 && auxVtable && vt==auxVtable && read(object,aux,sizeof(aux))==sizeof(aux)){
   q[13]|=2;memcpy(q+3,aux+0x30,8);memcpy(q+14,aux+0x18,8);uint64_t map=0;memcpy(&map,aux+0x10,8);
   uint8_t bytes[0x128] {};
   if(read(map,&vt,8)==8 && mapVtable && vt==mapVtable && read(map,bytes,sizeof(bytes))==sizeof(bytes)){
    q[13]|=4;uint32_t flags;memcpy(&flags,bytes+16,4);
    if(!(flags&0x40)){memcpy(q+4,bytes+0x98,8);q[13]|=8;}
   }
  }
  return slot;
 }
 template<class Clock>
 void finish(unsigned slot,Record &r,uint64_t result,Clock clock){
  if(slot>=MaxRecords)return;r.q[12]=result;clock(r.q+11);
  memcpy(records[slot].q,r.q,15*sizeof(uint64_t));
  __atomic_store_n(records[slot].q+15,1ULL,__ATOMIC_RELEASE);
 }
 const Record *published(unsigned i)const{
  return i<MaxRecords && __atomic_load_n(records[i].q+15,__ATOMIC_ACQUIRE)?records+i:nullptr;
 }
};
}
#endif
