#ifndef KERN_TGL_LAYOUT_RECEIPTS_HPP
#define KERN_TGL_LAYOUT_RECEIPTS_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace TGLLayoutReceipts {
constexpr unsigned Capacity=256;
struct Record {uint64_t q[6];uint32_t surface[16];uint8_t entry[160];};
static_assert(sizeof(Record)==272,"layout receipt ABI");
// First immutable receipt for each descriptor layout. GPU base-address words
// 8/9 are excluded from the key; the full descriptor remains in the receipt.
// Concurrent first appearances may occupy more than one slot. No slot reuse.
struct Store {
 Record records[Capacity] {};
 uint64_t calls {},used {},duplicates {},dropped {},entryFailures {};
 bool full()const{return __atomic_load_n(&used,__ATOMIC_RELAXED)>=Capacity;}
 static bool plausible(const uint32_t *s) {
  if(!s)return false;
  const auto width=(s[2]&0x3fffU)+1,height=((s[2]>>16)&0x3fffU)+1;
  return width<=8192 && height<=8192;
 }
 template<class Read> bool record(const uint32_t *s,uint64_t resource,uint64_t entry,uint64_t caller,Read read) {
  if(!plausible(s))return false;
  __atomic_fetch_add(&calls,1ULL,__ATOMIC_RELAXED);
  const auto count=__atomic_load_n(&used,__ATOMIC_ACQUIRE);
  if(count>=Capacity){__atomic_fetch_add(&dropped,1ULL,__ATOMIC_RELAXED);return false;}
  for(unsigned i=0;i<count;i++) {
   const auto &r=records[i];
   if(__atomic_load_n(r.q,__ATOMIC_ACQUIRE) && !memcmp(r.surface,s,8*sizeof(uint32_t))) {
    __atomic_fetch_add(&duplicates,1ULL,__ATOMIC_RELAXED);return false;
   }
  }
  const auto slot=__atomic_fetch_add(&used,1ULL,__ATOMIC_RELAXED);
  if(slot>=Capacity){__atomic_fetch_add(&dropped,1ULL,__ATOMIC_RELAXED);return false;}
  auto &r=records[slot];r.q[1]=slot;r.q[2]=resource;r.q[3]=entry;r.q[4]=caller;
  memcpy(r.surface,s,sizeof(r.surface));
  r.q[5]=read(entry,r.entry,sizeof(r.entry))==sizeof(r.entry)?0:1;
  if(r.q[5])__atomic_fetch_add(&entryFailures,1ULL,__ATOMIC_RELAXED);
  __atomic_store_n(r.q,1ULL,__ATOMIC_RELEASE);return true;
 }
};
}
#endif
