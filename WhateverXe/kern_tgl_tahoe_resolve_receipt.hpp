// Finite CPU-visible snapshots of an existing native resolve call.
#pragma once
#include "kern_tgl_tahoe_resolve_scratch.hpp"
namespace TGLTahoeResolveReceipt {
constexpr unsigned Capacity=4;
constexpr size_t MappedClass=0xCCCC0,ScratchSize=0x5100;
constexpr size_t VirtualAccessor=0x10B60,LengthStore=0x13C48;
constexpr uint8_t VirtualBytes[]={0x55,0x48,0x89,0xE5,0x48,0x8B,0x47,0x38,0x5D,0xC3};
constexpr uint8_t LengthBytes[]={0x48,0x89,0x43,0x20};
struct Receipt {
 uint64_t q[24];
 uint8_t params[160],mapped[64],vector[24],rects[64],scratch[ScratchSize];
};
inline uint64_t pair(const uint8_t *p){uint64_t v=0;memcpy(&v,p,8);return v;}
inline bool kernel(uint64_t p,size_t n){return (p>>47)==0x1FFFF && p<=UINT64_MAX-n;}
inline bool matches(const uint8_t *image,size_t size,size_t symbol,size_t mappedClass){
 using namespace TGLTahoeResolveScratch;
 return image && size>MappedClass+16 && symbol==Entry && mappedClass==MappedClass &&
  !memcmp(image+Entry,Head,sizeof(Head)) &&
  !memcmp(image+VirtualAccessor,VirtualBytes,sizeof(VirtualBytes)) &&
  !memcmp(image+LengthStore,LengthBytes,sizeof(LengthBytes));
}
// Observe only caller-owned CPU objects. Following the scratch CPU mapping
// requires the exact native class and allocation length; no map/virtual calls.
template<class Read>void collect(Receipt &o,uint64_t pool,uint64_t mapped,uint64_t params,
 uint64_t rects,bool mode,unsigned phase,bool result,uint64_t mappedClass,Read read){
 memset(&o,0,sizeof(o));o.q[0]=1;o.q[1]=phase;o.q[2]=pool;o.q[3]=mapped;
 o.q[4]=params;o.q[5]=rects;o.q[6]=mode;o.q[7]=result;
 auto copy=[&](uint64_t p,void *out,size_t n)->uint64_t{
  return kernel(p,n)?read(p,out,n):0;
 };
 o.q[9]=copy(params,o.params,sizeof(o.params));
 o.q[10]=copy(mapped,o.mapped,sizeof(o.mapped));
 o.q[11]=copy(rects,o.vector,sizeof(o.vector));
 if(o.q[11]==sizeof(o.vector)){
  const auto count=pair(o.vector),capacity=pair(o.vector+8),data=pair(o.vector+16);
  if(count && count<=64 && capacity>=count && capacity<=4096)
   o.q[12]=copy(data,o.rects,(count<4?count:4)*16);
 }
 if(o.q[10]!=sizeof(o.mapped))return;
 o.q[14]=pair(o.mapped+0x28);o.q[15]=pair(o.mapped+0x20);
 o.q[16]=pair(o.mapped+0x38);o.q[17]=pair(o.mapped);
 if(o.q[17]!=mappedClass || o.q[15]!=ScratchSize)return;
 o.q[13]=copy(o.q[16],o.scratch,sizeof(o.scratch));
}
}
