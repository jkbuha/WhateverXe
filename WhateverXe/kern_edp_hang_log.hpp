// Private bounded failure reporting from an already-owned CPU capture.
// No MMIO, allocation, address traversal, submission, or buffer mutation.
#ifndef kern_edp_hang_log_hpp
#define kern_edp_hang_log_hpp
#include <stdint.h>
#include <stddef.h>
#include <string.h>
struct EdpHangLog {
 template<class Capture,class Log> static void emit(const Capture &c,Log log) {
  log("WXEDP_HANG v1 matched=%u bb=%llx base=%llx total=%llu start=%llu copied=%llu stamp=%llu\n",
   unsigned(c.q[5]<64),static_cast<unsigned long long>(c.q[12]),static_cast<unsigned long long>(c.q[6]),
   static_cast<unsigned long long>(c.q[8]),static_cast<unsigned long long>(c.q[9]),
   static_cast<unsigned long long>(c.q[10]),static_cast<unsigned long long>(c.q[15]));
  if(c.q[5]>=64 || !c.q[10] || c.q[10]>sizeof(c.bytes) || c.q[10]>c.q[11] ||
     c.q[6]+c.q[9]<c.q[6])return;
  const uint64_t base=c.q[6]+c.q[9],bb=c.q[12]&~3ULL,have=c.q[10];
  uint64_t delta=bb>=base?bb-base:0;if(delta>have)delta=have;
  const uint64_t first=delta>1024?(delta-1024)&~3ULL:0;
  const uint64_t last=(delta+256<have?delta+256:have)&~3ULL;
  for(uint64_t offset=first;offset<last;offset+=32) {
   uint32_t words[8] {};const size_t count=last-offset<32?size_t(last-offset):32;
   memcpy(words,c.bytes+offset,count);
   log("WXEDP_BATCH +%llx n=%u %08x %08x %08x %08x %08x %08x %08x %08x\n",
    static_cast<unsigned long long>(c.q[9]+offset),unsigned(count/4),
    words[0],words[1],words[2],words[3],words[4],words[5],words[6],words[7]);
  }
  log("WXEDP_HANG_END\n");
 }
};
#endif
