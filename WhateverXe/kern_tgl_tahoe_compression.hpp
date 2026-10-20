// Bounded CPU-only receipts before native compression enable. No mutations.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace TGLTahoeCompression {
constexpr size_t Entry=0x73774,ResourceClass=0xD93D0,TaskClass=0xCA5E0;
constexpr unsigned Capacity=16;
constexpr uint8_t Head[]={0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xEC,0x88,0,0,0};
inline bool matches(const uint8_t *image,size_t size,size_t entry,size_t resource,size_t task){
 return image && size>ResourceClass+16 && entry==Entry && resource==ResourceClass && task==TaskClass &&
  !memcmp(image+Entry,Head,sizeof(Head));
}
inline uint64_t pair(const uint8_t *p){uint64_t v=0;memcpy(&v,p,8);return v;}
inline bool kernel(uint64_t p,size_t n){return (p>>47)==0x1FFFF && p<=UINT64_MAX-n;}
struct Receipt {uint64_t q[24];uint8_t entry[160],resource[0x108],map[0x128];};
// The caller supplies native arguments; no entry search, native method calls,
// allocation, GPU submission or MMIO. Incomplete reads retain a failure mask.
template<class Read> void collect(Receipt &o,uint64_t resource,uint64_t entry,uint64_t gpu,
 uint64_t accel,uint64_t task,uint8_t level,uint8_t face,bool skip,
 uint64_t resourceClass,uint64_t taskClass,Read read){
 memset(&o,0,sizeof(o));o.q[0]=1;o.q[2]=resource;o.q[3]=entry;o.q[4]=gpu;
 o.q[5]=accel;o.q[6]=task;o.q[7]=level;o.q[8]=face;o.q[9]=skip;
 auto copy=[&](uint64_t p,void *out,size_t n){return kernel(p,n) && read(p,out,n)==n;};
 if(!copy(entry,o.entry,sizeof(o.entry)))return;o.q[1]|=1;
 uint64_t vt=0;
 if(!copy(task,&vt,8) || vt!=taskClass)return;o.q[1]|=2;
 if(!copy(resource,o.resource,sizeof(o.resource)) || pair(o.resource)!=resourceClass)return;o.q[1]|=4;
 o.q[10]=o.resource[0x14];o.q[11]=pair(o.resource+0x40);
 if(!copy(o.q[11],o.map,sizeof(o.map)))return;o.q[1]|=8;
 o.q[12]=pair(o.map);o.q[13]=pair(o.map+0x18);o.q[14]=pair(o.map+0x90);o.q[15]=pair(o.map+0x98);
 const auto type=o.resource[0x14];o.q[16]=!(type&15)?resource:(type==0x82?pair(o.resource+0xE0):0);
 if(!copy(o.q[16],&vt,8) || vt!=resourceClass)return;o.q[1]|=16;
 if(!copy(o.q[16]+0x80,o.q+17,8) || !copy(o.q[16]+0x88,o.q+18,8) ||
    !copy(o.q[16]+0x230,o.q+19,8))return;o.q[1]|=32;
 o.q[20]=o.q[18]?o.q[18]:o.q[17];
 if(!copy(o.q[13],o.q+21,8) || !copy(o.q[13]+0x40,o.q+22,8))return;o.q[1]|=64;
}
}
