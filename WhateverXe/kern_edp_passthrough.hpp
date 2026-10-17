#ifndef kern_edp_passthrough_hpp
#define kern_edp_passthrough_hpp
#include <stdint.h>
#include <string.h>
namespace EdpPassThrough {
inline uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
inline uint64_t pair(const uint8_t *p){uint64_t v;memcpy(&v,p,8);return v;}
inline bool eligible(const uint8_t *e,const uint8_t *map,const uint8_t *mem,uint64_t task){
 return word(e+8)==7680 && word(e+0x10)==0xD1 && word(e+0x14)==3 &&
 word(e+0x3C)==9338880 && word(e+0x40)==36480 && !word(e+0x44) &&
 word(e+0x4C)==1 && !word(e+0x64) && !word(e+0x68) &&
 word(e+0x6C)==1920 && word(e+0x70)==1200 && word(e+0x74)==1 &&
 word(e+0x78)==1 && e[0x91]==0 && !(word(map+0x10)&0x40) &&
 pair(map+0x90)==task && pair(map+0x98) && !(pair(map+0x98)&4095) &&
 pair(map+0x98)<=0x7FFFFFFFFFFFULL-9375744 && pair(mem+0x40)==9375744;
}
// Caller owns these native objects. Reject any changed snapshot before the
// same two completion bookkeeping writes used by native full resolve.
inline bool complete(uint8_t *entry,const uint8_t *before){
 if(memcmp(entry,before,160))return false;
 uint32_t dirty=word(entry+0x4C)&~3U,zero=0;
 memcpy(entry+0x4C,&dirty,4);memcpy(entry+0x64,&zero,4);return true;
}
}
#endif
