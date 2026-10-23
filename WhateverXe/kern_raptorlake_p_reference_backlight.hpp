// Exact AppleBacklight 180.12 image guard; no hardware access.
#pragma once
#include <stdint.h>
namespace RaptorLakeP {namespace ReferenceBacklight {
inline bool image(const void *image,uint64_t bytes){
 if(!image || bytes<32)return false;
 const auto b=static_cast<const uint8_t *>(image);
 auto word=[&](uint64_t o){return uint32_t(b[o])|uint32_t(b[o+1])<<8|uint32_t(b[o+2])<<16|uint32_t(b[o+3])<<24;};
 if(word(0)!=0xFEEDFACF || word(4)!=0x1000007 || word(16)>128 || word(20)>bytes-32)return false;
 const uint8_t uuid[]{0xF9,0x9E,0xE2,0x3A,0xBD,0x38,0x3E,0x4E,0xA2,0x9A,0x16,0xA5,0x93,0x9E,0x2F,0xB3};
 uint64_t pos=32,end=32+word(20);bool found=false;
 for(unsigned i=0;i<word(16);++i){
  if(pos>end || end-pos<8)return false;
  auto kind=word(pos),len=word(pos+4);if(len<8 || len>end-pos)return false;
  if(kind==0x1B){if(found || len!=24)return false;for(unsigned k=0;k<16;++k)if(b[pos+8+k]!=uuid[k])return false;found=true;}
  pos+=len;
 }
 return found && pos==end;
}
struct PWM {uint32_t version{1},status{1},control{},period{},duty{};};
template<class IO> PWM capture(IO &io,bool active){
 PWM out;if(!active)return out;
 out.control=io.read(0xC8250);out.period=io.read(0xC8254);out.duty=io.read(0xC8258);
 out.status=(out.control==UINT32_MAX || out.period==UINT32_MAX || out.duty==UINT32_MAX)?2:0;return out;
}
}}
