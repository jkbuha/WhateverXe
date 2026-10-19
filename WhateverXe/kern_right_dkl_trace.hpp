// Bounded memory-only receipt of existing native DKL writes. No MMIO here.
#ifndef KERN_RIGHT_DKL_TRACE_HPP
#define KERN_RIGHT_DKL_TRACE_HPP
#include <stdint.h>
#include <stddef.h>
struct RightDKLTrace {
 static constexpr uint32_t Capacity=1024,Words=18;
 alignas(8) uint32_t count=0;
 uint32_t entries[Capacity][Words] {};
 void record(uint32_t physical,uint32_t ddi,bool write,uint32_t index,uint32_t offset,
  uint32_t bank,uint32_t mode,uint32_t requested,uint32_t effective,uint32_t before,
  uint32_t after,uint32_t suspendBefore,uint32_t suspendAfter,uint32_t flags,uintptr_t thread,uint64_t ns) {
  if(physical!=0x9a498086U || ddi!=5 || !write || mode!=3 || index<0x34 || index>0x49)return;
  auto slot=__sync_fetch_and_add(&count,1U);if(slot>=Capacity)return;
  auto q=entries[slot];q[1]=slot+1;q[2]=index;q[3]=write;q[4]=offset;q[5]=bank;q[6]=mode;
  q[7]=requested;q[8]=effective;q[9]=before;q[10]=after;q[11]=suspendBefore;q[12]=suspendAfter;
  q[13]=flags;q[14]=static_cast<uint32_t>(thread);q[15]=static_cast<uint32_t>(uint64_t(thread)>>32);
  q[16]=static_cast<uint32_t>(ns);q[17]=static_cast<uint32_t>(ns>>32);
  __atomic_store_n(q,1U,__ATOMIC_RELEASE);
 }
 size_t snapshot(uint32_t *out,size_t words) const {
  if(!out || words<8)return 0;
  const auto total=__atomic_load_n(&count,__ATOMIC_ACQUIRE);const auto used=total<Capacity?total:Capacity;
  if(words<8+size_t(used)*Words)return 0;
  out[0]=1;out[1]=total;out[2]=used;out[3]=Capacity;out[4]=Words*4;out[5]=0;out[6]=out[7]=0;
  for(uint32_t i=0;i<used;i++) {
   auto dst=out+8+i*Words;
   if(__atomic_load_n(entries[i],__ATOMIC_ACQUIRE)) {
    for(uint32_t j=0;j<Words;j++)dst[j]=entries[i][j];++out[5];
   }else for(uint32_t j=0;j<Words;j++)dst[j]=0;
  }
  return 8+size_t(used)*Words;
 }
};
#endif
