#include "kern_raptorlake_p_reference_nvram.hpp"
#include <cassert>
#include <cstring>
using namespace RaptorLakeP::ReferenceNVRAM;
int main(){
 assert(accepts(Before,sizeof(Before),Method,0x100,0x100,0x200,0x200));
 assert(!accepts(nullptr,6,Method,0x100,0x100,0x200,0x200));
 for(size_t n=0;n<6;++n)assert(!accepts(Before,n,Method,0x100,0x100,0x200,0x200));
 assert(!accepts(Before,6,Method+1,0x100,0x100,0x200,0x200));
 uintptr_t bindings[]{0,0x100,0x200,0x300};
 for(auto old:bindings)for(auto reserved:bindings)for(auto next:bindings)for(auto writer:bindings)
  assert(accepts(Before,6,Method,old,reserved,next,writer)==
         (reserved && writer && reserved!=writer && old==reserved && next==writer));
 uint8_t changed[6];
 for(size_t j=0;j<6;++j){memcpy(changed,Before,6);changed[j]^=1;
  assert(!accepts(changed,6,Method,0x100,0x100,0x200,0x200));}
 assert(!accepts(After,6,Method,0x100,0x100,0x200,0x200));
 // The six-byte instruction changes only its displacement, preserving call
 // width, registers and return address used by native status/cleanup code.
 for(size_t j=0;j<6;++j)assert(j==2 || Before[j]==After[j]);
 assert(Call+sizeof(After)==Method+0x16A);
}
