#ifndef kern_tgl_c0_fresh_hpp
#define kern_tgl_c0_fresh_hpp
#include "kern_tgl_c0_entry.hpp"
#include <string.h>
namespace TGLC0Fresh {
inline uint64_t pair(const uint8_t *p){uint64_t v;memcpy(&v,p,8);return v;}
// Exact compressed C0 shapes observed in a failing boot, with matching
// uncompressed shapes in the clean comparator. No format/type conversion.
constexpr uint32_t SmallLayouts[][3]={{157,157,1024},
 {920,693,4096},
 {40,24,512},
 {30,24,512},
 {130,24,1024},
 {31,24,512},
 {245,245,1024},
 {34,24,512},
 {134,113,1024},
 {161,24,1024},
 {105,105,512},
 {242,285,1024},
 {152,24,1024},
 {1394,790,5632},
 {370,278,1536},
 {512,278,2048},
 {370,512,1536},
 {512,512,2048},
 {370,256,1536},
 {512,256,2048},
 {1394,738,5632},
 {109,109,512},
 {15,738,512},
 {1379,42,5632},
 {729,90,3072},
 {363,512,1536},
 {1387,512,5632},
 {1379,177,5632},
 {1379,320,5632},
 {309,296,1536},
 {309,174,1536},
 {47,47,512},
 {640,640,2560}};
// Native fillTextureData rounds pitch to512, height to32, and derives
// CCS bytes as main/256. Its allocation tail rounds aux+192 to4096.
inline uint64_t backing(uint32_t pitch,uint32_t height){
 const uint64_t main=uint64_t(pitch)*((uint64_t(height)+31)&~31ULL);
 return ((main+4095)&~4095ULL)+((main/256+192+4095)&~4095ULL);
}
inline bool nativeEligible(const uint8_t *e){
 if(!e || TGLC0Entry::word(e+0x10)!=0xC0 || TGLC0Entry::word(e+0x14)!=3 || TGLC0Entry::word(e+0x44))return false;
 const auto w=TGLC0Entry::word(e+0x6C),h=TGLC0Entry::word(e+0x70),pitch=TGLC0Entry::word(e+8);
 if(!w || !h || w>4096 || h>4096 || pitch!=((w*4U+511U)&~511U))return false;
 const uint64_t main=uint64_t(pitch)*((uint64_t(h)+31)&~31ULL);
 return TGLC0Entry::word(e+0x3C)==main && TGLC0Entry::word(e+0x40)==main/256;
}
inline bool smallEligible(const uint8_t *e){return nativeEligible(e) && !TGLC0Entry::eligible(e);}
inline bool candidate(const uint8_t *e){return TGLC0Entry::eligible(e)||nativeEligible(e);}
inline bool fresh(const uint8_t *e){
 if(!nativeEligible(e) || pair(e) || pair(e+0x98) || e[0x91] || (e[0x4B]&8) ||
    TGLC0Entry::word(e+0x64) || TGLC0Entry::word(e+0x68) ||
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
// Keep the bound coupled to a pristine exact observed layout.
inline uint64_t allocationLength(const uint8_t *e){
 if(!fresh(e))return 0;
 return ((uint64_t(TGLC0Entry::word(e+0x3C))+4095)&~4095ULL)+
        ((uint64_t(TGLC0Entry::word(e+0x40))+192+4095)&~4095ULL);
}
inline bool knownBacking(uint64_t length){
 if(length==9375744 || length==249856)return true;
 for(const auto &l:SmallLayouts)if(length==backing(l[2],l[1]))return true;
 return false;
}
inline bool map(const uint8_t *m,uint64_t task,uint64_t gpu,uint64_t length,uint64_t expected=9375744){
 return knownBacking(expected) && task && gpu && !(gpu&65535) && gpu<=0x7FFFFFFFFFFFULL-expected &&
  !(TGLC0Entry::word(m+0x10)&0x40) && pair(m+0x90)==task &&
  pair(m+0x98)==gpu && length==expected;
}
// Production bound is derived from the same pristine entry being modified;
// callers cannot supply an unrelated expected allocation length.
inline bool mapForEntry(const uint8_t *m,uint64_t task,uint64_t gpu,uint64_t length,const uint8_t *e){
 const auto expected=allocationLength(e);
 return expected && task && gpu && !(gpu&65535) && gpu<=0x7FFFFFFFFFFFULL-expected &&
  !(TGLC0Entry::word(m+0x10)&0x40) && pair(m+0x90)==task && pair(m+0x98)==gpu && length==expected;
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
