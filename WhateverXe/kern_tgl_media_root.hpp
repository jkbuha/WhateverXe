// Pure parser for the observed 16-DWORD legacy state format. Candidate addresses
// only: this is neither proof of active state nor permission to dereference them.
#pragma once
#include <stdint.h>
#include <string.h>
namespace XeMediaRootCandidate {
struct State {uint64_t surface,dynamic,instruction,descriptor;uint32_t descriptorBytes,terminalOffset,vfe[9];};
inline uint32_t word(const uint8_t*p){uint32_t v;memcpy(&v,p,4);return v;}
inline uint64_t pair(const uint8_t*p){uint64_t v;memcpy(&v,p,8);return v;}
inline bool add(uint64_t b,uint64_t o,uint64_t n,uint64_t &result){
 if(!b || b>>47 || !n || o>=0x800000000000ULL || b>0x7fffffffffffULL-o || n-1>0x7fffffffffffULL-(b+o))return false;
 result=b+o;return true;
}
inline bool parse(const uint8_t *bytes,unsigned size,State &out){
 out={};if(!bytes || size<12 || size>16384 || (size&3))return false;
 State s{};bool base=false,vfe=false,idd=false;
 for(unsigned off=0;off<size;){
  const uint32_t w=word(bytes+off);unsigned n=0;
  if(w==0x11000001)n=3; // observed root prefix; skipped, never interpreted as a reference
  else if((w&~0x200U)==0x7a000004)n=6;
  else if(w==0x69041301)n=1;
  else if(w==0x6101000e)n=16;
  else if(w==0x70000007)n=9;
  else if(w==0x70020002)n=4;
  else if(w==0x18c00001){
   if(size-off<12 || !base || !vfe || !idd)return false;
   s.terminalOffset=off;out=s;return true; // no address-space interpretation or traversal
  }else return false;
  if(n>(size-off)/4)return false;
  const auto p=bytes+off;
  if(w==0x6101000e){
   if(base || vfe || idd)return false;
   const uint64_t surface=pair(p+16),dynamic=pair(p+24),instruction=pair(p+40);
   if(!(surface&1) || !(dynamic&1) || !(instruction&1))return false;
   s.surface=surface&~4095ULL;s.dynamic=dynamic&~4095ULL;s.instruction=instruction&~4095ULL;
   uint64_t dummy;if(!add(s.surface,0,4096,dummy)||!add(s.dynamic,0,4096,dummy)||!add(s.instruction,0,4096,dummy))return false;
   base=true;
  }else if(w==0x70000007){
   if(!base || vfe || idd)return false;memcpy(s.vfe,p,36);vfe=true;
  }else if(w==0x70020002){
   if(!base || !vfe || idd || word(p+4))return false;
   const uint32_t len=word(p+8),offset=word(p+12);
   // 256 is a diagnostic capture cap, not a hardware maximum.
   if(!len || len>256 || (len&31) || (offset&31) || !add(s.dynamic,offset,len,s.descriptor))return false;
   s.descriptorBytes=len;idd=true;
  }
  off+=n*4;
 }
 return false;
}
}
