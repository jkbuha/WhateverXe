// Exact native resolve scratch extent, rounded to complete GPU pages.
#pragma once
#include "kern_tgl_tahoe_resolve_scratch.hpp"
namespace TGLTahoeResolveAllocation {
constexpr size_t NativeSize=0x5100,RoundedSize=0x6000,Params=0x14B1C8,ParamsSize=Params+0x10;
constexpr size_t Constructor=0x7DACD,Allocate=0x7CF59,ParamsReference=0x81CC,WithOptions=0x10924;
constexpr uint8_t ConstructorBytes[]={0x48,0x8D,0x35,0xEC,0x4B,0x04,0,0x48,0x8B,0x36,0x48,0x89,0x35,0xFA,0xD6,0x0C,0};
constexpr uint8_t AllocateBytes[]={0x49,0x8B,0x74,0x24,0x10,0x48,0x85,0xF6,0x74,0x18,0x4C,0x89,0xF7,0x31,0xD2,0x31,0xC9,0xE8,0xB5,0x39,0xF9,0xFF};
constexpr uint8_t ParamsBytes[]={0x48,0x8D,0x15,0xF5,0x2F,0x14,0};
struct Plan {bool valid;uint64_t constant,params;};
inline Plan plan(const uint8_t *image,size_t size,size_t constantSymbol,size_t paramsSymbol,size_t allocatorSymbol){
 Plan p{};
 if(!image || size<ParamsSize+9 || constantSymbol!=TGLTahoeResolveScratch::Allocation || paramsSymbol!=Params || allocatorSymbol!=WithOptions)return p;
 memcpy(&p.constant,image+constantSymbol,8);memcpy(&p.params,image+ParamsSize,8);
 p.valid=p.constant==NativeSize && (p.params==0 || p.params==NativeSize) &&
  !memcmp(image+Constructor,ConstructorBytes,sizeof(ConstructorBytes)) &&
  !memcmp(image+Allocate,AllocateBytes,sizeof(AllocateBytes)) && !memcmp(image+ParamsReference,ParamsBytes,sizeof(ParamsBytes));
 return p;
}
// Patch is the existing kernel writer. Refuse changed input and roll back a
// reported failure even when the writer changed bytes before reporting it.
template<class Patch>bool apply(uint8_t *image,const Plan &p,Patch patch){
 if(!image || !p.valid)return false;
 auto word=[&](size_t at){uint64_t v;memcpy(&v,image+at,8);return v;};
 const auto constant=TGLTahoeResolveScratch::Allocation;
 if(word(constant)!=p.constant || word(ParamsSize)!=p.params)return false;
 if(patch(constant,p.constant,RoundedSize) && patch(ParamsSize,p.params,RoundedSize) &&
    word(constant)==RoundedSize && word(ParamsSize)==RoundedSize)return true;
 if(word(ParamsSize)==RoundedSize)patch(ParamsSize,RoundedSize,p.params);
 if(word(constant)==RoundedSize)patch(constant,RoundedSize,p.constant);
 return false;
}
static_assert((RoundedSize&4095)==0 && RoundedSize>=NativeSize,"cover the partial final page");
}
