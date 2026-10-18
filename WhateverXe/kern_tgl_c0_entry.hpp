#ifndef kern_tgl_c0_entry_hpp
#define kern_tgl_c0_entry_hpp
#include <stdint.h>
namespace TGLC0Entry {
constexpr unsigned Capacity=64;
inline uint32_t word(const uint8_t *p){return uint32_t(p[0]) | uint32_t(p[1])<<8 | uint32_t(p[2])<<16 | uint32_t(p[3])<<24;}
// Two observed full-width C0 layouts. Width/height may not be initialized here.
// This is an observation filter, never permission to modify or map a resource.
inline bool eligible(const uint8_t *e){
 return e && word(e+8)==7680 && word(e+0x10)==0xC0 && word(e+0x14)==3 &&
        word(e+0x44)==0 &&
        ((word(e+0x3C)==9338880 && word(e+0x40)==36480) ||
         (word(e+0x3C)==245760 && word(e+0x40)==960));
}
}
#endif
