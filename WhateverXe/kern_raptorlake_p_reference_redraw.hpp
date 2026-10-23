// Exact IOGraphicsFamily 600 redraw contract; no VRAM or GPU register access.
#pragma once
#include <stdint.h>
namespace RaptorLakeP {namespace ReferenceRedraw {
inline bool selected(bool requested,bool physical,bool provider,bool head,bool internal){
 return requested && physical && provider && head && internal;
}
inline bool image(const void *image,uint64_t bytes){
 if(!image || bytes<32)return false;
 const auto b=static_cast<const uint8_t *>(image);
 auto word=[&](uint64_t o){return uint32_t(b[o])|uint32_t(b[o+1])<<8|uint32_t(b[o+2])<<16|uint32_t(b[o+3])<<24;};
 if(word(0)!=0xFEEDFACF || word(4)!=0x1000007 || word(16)>128 || word(20)>bytes-32)return false;
 const uint8_t uuid[]{0x74,0xAA,0x64,0x83,0x8A,0xB4,0x3A,0x94,0x81,0x43,0x5E,0x30,0x6C,0x16,0xD7,0xDA};
 uint64_t pos=32,end=32+word(20);bool found=false;
 for(unsigned i=0;i<word(16);++i){
  if(pos>end || end-pos<8)return false;
  auto kind=word(pos),len=word(pos+4);if(len<8 || len>end-pos)return false;
  if(kind==0x1B){if(found || len!=24)return false;for(unsigned k=0;k<16;++k)if(b[pos+8+k]!=uuid[k])return false;found=true;}
  pos+=len;
 }
 return found && pos==end;
}
}}
