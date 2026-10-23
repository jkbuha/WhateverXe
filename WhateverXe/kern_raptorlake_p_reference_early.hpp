// Exact IONDRVSupport 600 guard and firmware-binding policy; no hardware I/O.
#pragma once
#include <stdint.h>
namespace RaptorLakeP {namespace ReferenceEarly {
inline bool block(bool requested,bool image,bool physical,bool provider,bool firmware){
 return requested && image && physical && provider && firmware;
}
inline bool image(const void *image,uint64_t bytes){
 if(!image || bytes<32)return false;
 const auto b=static_cast<const uint8_t *>(image);
 auto word=[&](uint64_t o){return uint32_t(b[o])|uint32_t(b[o+1])<<8|uint32_t(b[o+2])<<16|uint32_t(b[o+3])<<24;};
 if(word(0)!=0xFEEDFACF || word(4)!=0x1000007 || word(16)>128 || word(20)>bytes-32)return false;
 const uint8_t uuid[]{0x7C,0xD8,0xB8,0xF3,0xAC,0xFE,0x31,0x04,0x80,0x9B,0x93,0xF0,0xB5,0x73,0x6E,0x69};
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
