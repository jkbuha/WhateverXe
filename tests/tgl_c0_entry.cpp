#include "kern_tgl_c0_entry.hpp"
#include <cassert>
#include <cstring>
#include <initializer_list>
static void put(uint8_t *e,unsigned o,uint32_t v){for(unsigned i=0;i<4;++i)e[o+i]=v>>(i*8);}
int main(){
 uint8_t e[160] {};put(e,8,7680);put(e,0x10,0xC0);put(e,0x14,3);put(e,0x3C,9338880);put(e,0x40,36480);
 uint8_t saved[160];memcpy(saved,e,160);
 assert(!TGLC0Entry::eligible(nullptr));assert(TGLC0Entry::eligible(e));assert(!memcmp(e,saved,160));
 for(unsigned o : {8U,0x10U,0x14U,0x3CU,0x40U,0x44U}){memcpy(e,saved,160);e[o]^=1;assert(!TGLC0Entry::eligible(e));}
 memcpy(e,saved,160);put(e,0x6C,1920);put(e,0x70,1200);e[0x91]=3;assert(TGLC0Entry::eligible(e));
 // Include the observed compressed strip; native uncompressed entries remain excluded.
 put(e,0x3C,245760);put(e,0x40,960);put(e,0x70,24);assert(TGLC0Entry::eligible(e));
 memcpy(saved,e,160);
 for(unsigned o : {8U,0x10U,0x14U,0x3CU,0x40U,0x44U}){memcpy(e,saved,160);e[o]^=1;assert(!TGLC0Entry::eligible(e));}
 put(e,0x3C,0);put(e,0x40,0);assert(!TGLC0Entry::eligible(e));
}
