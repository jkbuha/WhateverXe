#include "kern_tgl_c0_fresh.hpp"
#include <cassert>
#include <cstring>
static void put(uint8_t *p,unsigned o,uint64_t v,unsigned n=8){memcpy(p+o,&v,n);}
int main(){
 assert(TGLC0Fresh::selectedMemory(0x1000,0x2000)==0x2000);assert(TGLC0Fresh::selectedMemory(0x1000,0)==0x1000);assert(!TGLC0Fresh::selectedMemory(0,0));
 uint8_t e[160] {},before[160],r[0x108] {},m[0x128] {};
 put(e,8,7680,4);put(e,0x10,0xC0,4);put(e,0x14,3,4);put(e,0x3C,9338880,4);put(e,0x40,36480,4);put(e,0x6C,1920,4);put(e,0x70,1200,4);put(e,0x74,1,4);put(e,0x78,1,4);memcpy(before,e,160);
 assert(TGLC0Fresh::fresh(e));e[0x4B]=8;assert(!TGLC0Fresh::fresh(e));memcpy(e,before,160);
 for(unsigned f=1;f<256;++f){e[0x91]=f;assert(!TGLC0Fresh::fresh(e));}memcpy(e,before,160);
 for(unsigned o=0x4C;o<0x64;o+=4){e[o]=1;assert(!TGLC0Fresh::fresh(e));e[o]=0;}
 e[0x64]=1;assert(!TGLC0Fresh::disable(e,before));memcpy(e,before,160);assert(TGLC0Fresh::disable(e,before));put(before,0x40,0,4);assert(!memcmp(e,before,160));assert(!TGLC0Fresh::fresh(e));
 assert(TGLC0Fresh::storage(0x1000,r)==0x1000);r[0x14]=0x82;put(r,0xE0,0x2000);assert(TGLC0Fresh::storage(0x1000,r)==0x2000);r[0x14]=1;assert(!TGLC0Fresh::storage(0x1000,r));
 put(m,0x90,0x1234);put(m,0x98,0x45710000);assert(TGLC0Fresh::map(m,0x1234,0x45710000,9375744));assert(!TGLC0Fresh::map(m,0x1235,0x45710000,9375744));assert(!TGLC0Fresh::map(m,0x1234,0x45710001,9375744));assert(!TGLC0Fresh::map(m,0x1234,0x45710000,9379840));m[0x10]=0x40;assert(!TGLC0Fresh::map(m,0x1234,0x45710000,9375744));
}
