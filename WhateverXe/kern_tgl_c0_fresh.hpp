#ifndef kern_tgl_c0_fresh_hpp
#define kern_tgl_c0_fresh_hpp
#include "kern_tgl_c0_entry.hpp"
#include <string.h>
namespace TGLC0Fresh {
inline uint64_t pair(const uint8_t *p){uint64_t v;memcpy(&v,p,8);return v;}
inline bool fresh(const uint8_t *e){
 if(!TGLC0Entry::eligible(e) || pair(e) || pair(e+0x98) || e[0x91] || (e[0x4B]&8) ||
    TGLC0Entry::word(e+0x64) || TGLC0Entry::word(e+0x68) ||
    TGLC0Entry::word(e+0x6C)!=1920 ||
    !((TGLC0Entry::word(e+0x70)==1200 && TGLC0Entry::word(e+0x3C)==9338880) ||
      (TGLC0Entry::word(e+0x70)==24 && TGLC0Entry::word(e+0x3C)==245760)) ||
    TGLC0Entry::word(e+0x74)!=1 || TGLC0Entry::word(e+0x78)!=1)return false;
 for(unsigned o=0x4C;o<0x64;o+=4)if(TGLC0Entry::word(e+o))return false;
 return true;
}
// Native isIOSurface tests storage+0x1CB bit3 (default entry+0x4B).
// Shared IOSurface storage is excluded even when its local flags look fresh.
// Exact native getStorageResource cases that do not search or allocate.
inline uint64_t storage(uint64_t resource,const uint8_t *r){
 if(!(r[0x14]&15))return resource;
 return r[0x14]==0x82 ? pair(r+0xE0) : 0;
}
// Exact IOAccelResource2::map selection: optimized memory wins when present.
inline uint64_t selectedMemory(uint64_t main,uint64_t optimized){return optimized?optimized:main;}
// Keep the allocation bound coupled to one of the two exact native layouts.
inline uint64_t allocationLength(const uint8_t *e){
 if(!fresh(e))return 0;
 return TGLC0Entry::word(e+0x3C)==245760?249856:9375744;
}
inline bool map(const uint8_t *m,uint64_t task,uint64_t gpu,uint64_t length,uint64_t expected=9375744){
 return (expected==9375744 || expected==249856) && task && gpu && !(gpu&65535) && gpu<=0x7FFFFFFFFFFFULL-expected &&
  !(TGLC0Entry::word(m+0x10)&0x40) && pair(m+0x90)==task &&
  pair(m+0x98)==gpu && length==expected;
}
// Never reinterpret an already compressed surface. The only changed word is
// its unused CCS size, before first native compression enable or any dirty use.
// Native fillSurfaceStateAuxInfo then selects AUX_NONE; native CCS resolve sees
// both CCS sizes zero and returns without GPU commands.
inline bool disable(uint8_t *entry,const uint8_t *before){
 if(!fresh(before) || memcmp(entry,before,160))return false;
 const uint32_t zero=0;memcpy(entry+0x40,&zero,4);return true;
}
}
#endif
