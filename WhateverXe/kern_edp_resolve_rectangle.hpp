// Scoped TGL colour-resolve rectangle trial. Integer IEEE754 bit copies only.
#ifndef kern_edp_resolve_rectangle_hpp
#define kern_edp_resolve_rectangle_hpp
#include <stdint.h>
#include <string.h>
namespace EdpResolveRectangle {
inline uint32_t word(const uint8_t *p,unsigned o){uint32_t v;memcpy(&v,p+o,4);return v;}
inline uint64_t pair(const uint8_t *p,unsigned o){uint64_t v;memcpy(&v,p+o,8);return v;}
inline bool prepare(const uint64_t *q,const uint8_t *params,const uint8_t *map,
 const uint8_t *memory,const uint64_t *vector,const uint32_t *rect,uint32_t *out){
 if(q[0]!=2 || q[7]!=512 || q[8]!=160 || q[12]!=0x128 || q[15]!=0x198)return false;
 if(word(params,8)!=1 || word(params,12)!=1920 || word(params,16)!=1200 ||
  word(params,20)!=0 || word(params,24)!=7680 || word(params,64)!=0xD1 ||
  word(params,100)!=3 || params[0x46]!=3 || params[0x48]!=0)return false;
 const auto gpu=pair(params,32);
 if(!gpu || gpu>>47 || (word(map,16)&0x40) || pair(map,0x98)!=gpu ||
  pair(memory,0x40)<9338880ULL)return false;
 if(vector[0]!=1 || !vector[1] || vector[1]>64 || (vector[2]>>47)!=0x1FFFF)return false;
 const uint32_t expected[]={0,0,0x41F00000,0x42960000};
 if(memcmp(rect,expected,sizeof(expected)))return false;
 memcpy(out,expected,sizeof(expected));out[3]=0x42980000; // 30x75 ->30x76
 return true;
}
}
#endif
