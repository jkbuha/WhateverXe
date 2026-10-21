// Existing native resource backing receipts and bounded rectangle correction.
#pragma once
#include "kern_edp_resolve_rectangle.hpp"
#include <stddef.h>
namespace TGLTahoeResolveResource {
constexpr size_t Entry=0x2C84C,MapClass=0xCCEF8,MemoryClass=0xCD6D8,ResourceClass=0xD93D0,TaskClass=0xCA5E0;
constexpr uint8_t Head[]={0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC,0x48};
constexpr uint8_t MapSize[]={0xB9,0x28,0x01,0,0},MemorySize[]={0xB9,0x98,0x01,0,0};
constexpr uint8_t LengthGetter[]={0x55,0x48,0x89,0xE5,0x48,0x8B,0x47,0x40,0x5D,0xC3};
inline uint64_t pair(const uint8_t*p){uint64_t n;memcpy(&n,p,8);return n;}
inline bool kernel(uint64_t p,size_t n){return (p>>47)==0x1FFFF && p<=UINT64_MAX-n;}
inline bool matches(const uint8_t *b,size_t n,size_t entry,size_t map,size_t memory,size_t resource,size_t task){
 return b && n>ResourceClass+16 && entry==Entry && map==MapClass && memory==MemoryClass && resource==ResourceClass && task==TaskClass &&
  !memcmp(b+Entry,Head,sizeof(Head)) && !memcmp(b+0x10BCF,MapSize,sizeof(MapSize)) && !memcmp(b+0x126F5,MemorySize,sizeof(MemorySize));
}
struct Classes {uint64_t resource,task,map,memory;};
struct Receipt {uint64_t q[24];uint8_t resource[512],params[160],map[0x128],memory[0x198];uint64_t vector[3];uint32_t rect[4];uint8_t getter[10];};
// Validate the actual bound family getter rather than assuming its address
// stayed fixed across host versions. At most one KC import thunk is accepted.
template<class Read>bool lengthABI(uint64_t vtable,uint64_t &target,uint8_t *bytes,Read read){
 target=0;if(!kernel(vtable,0x128) || read(vtable+0x120,&target,8)!=8 || !kernel(target,10))return false;
 uint8_t head[10]{};if(read(target,head,10)!=10)return false;
 if(head[0]==0xFF && head[1]==0x25){
  int32_t delta=0;memcpy(&delta,head+2,4);const auto slot=target+6+static_cast<int64_t>(delta);
  if(!kernel(slot,8) || read(slot,&target,8)!=8 || !kernel(target,10) || read(target,head,10)!=10)return false;
 }
 memcpy(bytes,head,10);return !memcmp(head,LengthGetter,10);
}
// All snapshots end before native execution. Caller provides a temporary
// vector/rectangle for the qualified case, leaving native caller data intact.
template<class Read>bool prepare(Receipt &o,uint64_t resource,uint64_t params,uint64_t rects,uint64_t task,
 const Classes &classes,uint32_t *out,Read read){
 memset(&o,0,sizeof(o));o.q[0]=2;o.q[3]=resource;o.q[4]=params;o.q[5]=rects;o.q[6]=task;
 auto copy=[&](uint64_t p,void*d,size_t n)->uint64_t{return kernel(p,n)?read(p,d,n):0;};
 o.q[8]=copy(params,o.params,sizeof(o.params));if(o.q[8]!=sizeof(o.params))return false;
 o.q[7]=copy(resource,o.resource,sizeof(o.resource));
 uint64_t vt=0;if(o.q[7]!=sizeof(o.resource) || pair(o.resource)!=classes.resource ||
  copy(task,&vt,8)!=8 || vt!=classes.task)return false;
 o.q[10]=pair(o.resource+0x40);
 if(copy(o.q[10],&vt,8)!=8 || vt!=classes.map)return false;o.q[11]=vt;
 o.q[12]=copy(o.q[10],o.map,sizeof(o.map));if(o.q[12]!=sizeof(o.map) || pair(o.map+0x90)!=task)return false;
 o.q[13]=pair(o.map+0x18);
 if(copy(o.q[13],&vt,8)!=8 || vt!=classes.memory)return false;o.q[14]=vt;
 o.q[15]=copy(o.q[13],o.memory,sizeof(o.memory));if(o.q[15]!=sizeof(o.memory))return false;
 const auto bytes=pair(o.memory+0x40),gpu=pair(o.params+32);
 if(bytes>0x1000000 || gpu>=0x800000000000ULL || bytes>0x800000000000ULL-gpu)return false;
 if(!lengthABI(vt,o.q[18],o.getter,copy))return false;o.q[19]=1;
 if(copy(rects,o.vector,sizeof(o.vector))!=sizeof(o.vector) || o.vector[0]!=1 ||
  copy(o.vector[2],o.rect,sizeof(o.rect))!=sizeof(o.rect))return false;
 if(!EdpResolveRectangle::prepare(o.q,o.params,o.map,o.memory,o.vector,o.rect,out))return false;
 // Revalidate the ownership chain and allocation length at the decision point.
 uint64_t later=0;
 if(copy(resource,&vt,8)!=8 || vt!=classes.resource || copy(resource+0x40,&later,8)!=8 || later!=o.q[10] ||
  copy(o.q[10],&vt,8)!=8 || vt!=classes.map || copy(o.q[10]+0x18,&later,8)!=8 || later!=o.q[13] ||
  copy(o.q[10]+0x90,&later,8)!=8 || later!=task || copy(o.q[10]+0x98,&later,8)!=8 || later!=pair(o.params+32) ||
  copy(o.q[13],&vt,8)!=8 || vt!=classes.memory || copy(o.q[13]+0x40,&later,8)!=8 || later!=pair(o.memory+0x40))return false;
 o.q[9]=3;return true;
}
}
