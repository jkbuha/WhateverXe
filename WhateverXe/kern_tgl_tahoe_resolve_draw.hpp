// Exact native non-extended resolve draw length; retain zero padding.
#pragma once
#include "kern_tgl_tahoe_resolve_scratch.hpp"
namespace TGLTahoeResolveDraw {
constexpr size_t Site=0x8D420,TailSite=0x8D427,ZeroSite=0x8D2FC,SaveSite=0x8D40B;
constexpr uint8_t Original[] {0xC6,0x83,0x80,0x01,0,0,0x08};
constexpr uint8_t Replacement[] {0xC6,0x83,0x80,0x01,0,0,0x05};
constexpr uint8_t Tail[] {0x66,0xC7,0x83,0x81,0x01,0,0,0,0,0xC6,0x83,0x83,0x01,0,0,0x7B};
constexpr uint8_t Zero[] {0x45,0x31,0xFF};
constexpr uint8_t Save[] {0x44,0x89,0xBD,0xA8,0xFE,0xFF,0xFF,0x4C,0x89,0xBD,0xA0,0xFE,0xFF,0xFF,0x4C,0x89,0xBD,0x98,0xFE,0xFF,0xFF};
inline bool matches(const uint8_t *image,size_t size,size_t entry){
 using namespace TGLTahoeResolveScratch;
 return image && size>=TailSite+sizeof(Tail) && entry==Entry &&
  !memcmp(image+Entry,Head,sizeof(Head)) && !memcmp(image+Site,Original,sizeof(Original)) &&
  !memcmp(image+TailSite,Tail,sizeof(Tail)) && !memcmp(image+ZeroSite,Zero,sizeof(Zero)) &&
  !memcmp(image+SaveSite,Save,sizeof(Save));
}
}
