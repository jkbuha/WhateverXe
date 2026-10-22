#include "kern_raptorlake_p_reference_shutdown.hpp"
#include <cassert>
#include <cstring>
#include <initializer_list>
using RaptorLakeP::ReferenceShutdown::getter;
int main(){uint8_t c[]{0x55,0x48,0x89,0xE5,0x8B,0x05,0,0,0,0,0x5D,0xC3};
 for(int32_t d:{int32_t(0),int32_t(12345),int32_t(-12345)}){
  memcpy(c+6,&d,4);auto target=uintptr_t(0x10000+10+d);
  assert(getter(c,12,0x10000,target));assert(!getter(c,12,0x10000,target+1));
  for(unsigned i=0;i<12;++i){c[i]^=1;assert(!getter(c,12,0x10000,target));c[i]^=1;}
 }
 assert(!getter(nullptr,12,1,1));assert(!getter(c,11,1,1));assert(!getter(c,12,UINTPTR_MAX-9,1));
 int32_t d=-100;memcpy(c+6,&d,4);assert(!getter(c,12,1,1));
 d=100;memcpy(c+6,&d,4);assert(!getter(c,12,UINTPTR_MAX-20,1));
}
