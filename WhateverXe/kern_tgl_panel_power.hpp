// Companion PG3 references for the native DDI-A reference lifetime. Caller
// preserves the donor's existing serialization of the power object. No locks,
// MMIO access or raw power requests are introduced by this bookkeeping.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace TGLPanelPower {
constexpr size_t Get=0x5BFA8,Put=0x5AA86;
constexpr uint8_t GetHead[]={0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x50,0x48,0xFF,0x05,0x23,0x5E,0x08,0,0x89,0xF0,0x8B,0x4C,0x87,0x30,0xFF,0xC1,0x89,0x4C,0x87,0x30};
constexpr uint8_t PutHead[]={0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC,0x18,0x48,0xFF,0x05,0x92,0x6E,0x08,0,0x89,0xF0,0x8B,0x4C,0x87,0x30,0x85,0xC9};
inline bool matches(const uint8_t *p,size_t n,size_t get,size_t put) {
 return p && n>=Get+sizeof(GetHead) && get==Get && put==Put &&
  !memcmp(p+Get,GetHead,sizeof(GetHead)) && !memcmp(p+Put,PutHead,sizeof(PutHead));
}
struct Lease {
 uintptr_t owner=0;uint32_t held=0,gets=0,puts=0,inheritedPuts=0,errors=0;
 bool fault=false;
 template<class IO,class Native>void enable(bool admitted,uintptr_t power,IO &io,Native native) {
  if(!admitted || !power || fault || (owner && owner!=power)){native();return;}
  const auto before=io.ddiRefs();uint32_t pg[3];
  if(before<held){++errors;fault=true;native();return;}
  if(before==UINT32_MAX || held==UINT32_MAX){++errors;native();return;}
  for(unsigned i=0;i<3;++i){pg[i]=io.pgRefs(i);if(pg[i]==UINT32_MAX){++errors;native();return;}}
  owner=power;io.getPG3();++gets;
  for(unsigned i=0;i<3;++i)if(io.pgRefs(i)!=pg[i]+1)fault=true;
  if(fault){++errors;native();return;} // Unknown ownership: do not guess a put.
  native();
  if(io.ddiRefs()==before+1)++held;
  else {++errors;io.putPG3();++puts;if(!held)owner=0;}
 }
 template<class IO,class Native>void disable(uintptr_t power,IO &io,Native native) {
  if(!power || power!=owner || !held || fault){native();return;}
  const auto before=io.ddiRefs();
  if(before<held){++errors;fault=true;native();return;}
  const bool release=before && before==held;
  native(); // DDI hardware must be disabled before dropping its parent wells.
  if(!release){if(before)++inheritedPuts;return;}
  if(io.ddiRefs()!=before-1){++errors;fault=true;return;}
  for(unsigned i=0;i<3;++i)if(!io.pgRefs(i)){++errors;fault=true;return;}
  io.putPG3();++puts;--held;if(!held)owner=0;
 }
};
}
