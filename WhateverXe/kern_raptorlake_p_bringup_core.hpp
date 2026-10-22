// RPL-P donor adaptations. Hardware behavior identified in the JulioNectar
// XPS9320 investigation; implementation uses exact donor instruction windows.
// Reference: https://github.com/JulioNectar/Hackintosh-XPS13Plus-IrisXe-Tahoe
// No NootedGreen implementation code is incorporated here.
#ifndef KERN_RAPTORLAKE_P_BRINGUP_CORE_HPP
#define KERN_RAPTORLAKE_P_BRINGUP_CORE_HPP
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_tgl_rpcs.hpp"
namespace RaptorLakeP {namespace Bringup {
constexpr unsigned Count=5;
struct Edit {uint32_t offset;uint8_t length,changed;uint32_t before,after;};
// Operands rather than whole-function replacement. Keep every opcode, branch,
// mask bit and register destination outside these operands unchanged.
constexpr Edit edits[Count] {
 {0x7C313,11,7,0x80000000,TGLRPCS::FullSlice},
 {0x25189,10,6,0x40004000,0x40000000}, // masked clear, before reset-register snapshot
 {0x246D9,8,7,1,0},                  // startup renderCompression byte
 {0x2B872,10,0,0,0},                // runtime writer: zero AL, preserve its store
 {0x7287D,13,2,0xB0,0x98},          // Tahoe IOSurface plane count displacement
};
inline void expected(unsigned i,uint8_t (&b)[16],uint8_t (&n)[16]) {
 memset(b,0,16);memset(n,0,16);
 switch(i) {
 case 0:memcpy(b,TGLRPCS::Find,11);break;
 case 1:{const uint8_t v[]{0xC7,0x80,0xE0,0x20,0,0,0,0x40,0,0x40};memcpy(b,v,10);break;}
 case 2:{const uint8_t v[]{0x41,0xC6,0x85,0x74,0x14,0,0,1};memcpy(b,v,8);break;}
 case 3:{const uint8_t v[]{0x41,0x8A,7,0x41,0x88,0x86,0x74,0x14,0,0};memcpy(b,v,10);break;}
 case 4:{const uint8_t v[]{0x8B,0x83,0xB0,0,0,0,0x85,0xC0,0xBA,1,0,0,0};memcpy(b,v,13);break;}
 }
 memcpy(n,b,16);
 if(i==3){n[0]=0x30;n[1]=0xC0;n[2]=0x90;} // xor al,al; nop; original byte store
 else if(i==2)n[7]=0;
 else memcpy(n+edits[i].changed,&edits[i].after,4);
}
inline bool qualified(const void *image,size_t size) {
 if(!Native::exactImage(image,size))return false;
 const auto b=static_cast<const uint8_t *>(image);
 for(unsigned i=0;i<Count;++i){uint8_t old[16],next[16];expected(i,old,next);
  const auto &e=edits[i];if(e.offset>size || e.length>size-e.offset || memcmp(b+e.offset,old,e.length))return false;
 }
 // The runtime writer must still belong to the exact compression selector.
 return size>=0x959E3 && !memcmp(b+0x959D1,"renderCompression",18);
}
template<class Writer> bool apply(void *image,size_t size,Writer write,uint32_t &mask) {
 mask=0;if(!qualified(image,size))return false;
 auto b=static_cast<uint8_t *>(image);
 for(unsigned i=0;i<Count;++i){uint8_t old[16],next[16];expected(i,old,next);const auto &e=edits[i];
  if(!write(e.offset,next,e.length) || memcmp(b+e.offset,next,e.length))return false;
  mask|=1U<<i;
 }
 return mask==((1U<<Count)-1);
}
}}
#endif
