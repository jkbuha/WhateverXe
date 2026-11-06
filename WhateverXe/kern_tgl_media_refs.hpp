// Pure bounded candidate collector. Reader is injected; no OS or driver calls.
#pragma once
#include "kern_tgl_media_root.hpp"
namespace XeMediaReferenceCandidate {
constexpr unsigned MaxPages=16;
enum Kind:uint64_t {Descriptor=1,Instruction=2,Surface=4};
struct Page{uint64_t gpu,kind,copied,qualified;uint8_t bytes[4096];};
struct Capture{uint64_t schema,guard,pages,drops,refusedRoots,acceptedRoots,failedPages;Page page[MaxPages];};
static_assert(sizeof(Capture)<131072,"bounded immutable media reference property");
template<class Reader>struct Collector{
 Capture &capture;Reader &read;
 Page* page(uint64_t address,uint64_t kind){
  if(capture.pages>MaxPages || !address || address>>47){++capture.drops;return nullptr;}
  const uint64_t gpu=address&~4095ULL;
  for(unsigned i=0;i<capture.pages;++i)if(capture.page[i].gpu==gpu){capture.page[i].kind|=kind;return capture.page+i;}
  if(capture.pages>=MaxPages){++capture.drops;return nullptr;}
  auto &p=capture.page[capture.pages++];p={};p.gpu=gpu;p.kind=kind;
  // Reader must qualify mapping and complete ownership before returning true.
  p.qualified=read(gpu,p.bytes)?1:0;p.copied=p.qualified?4096:0;
  if(!p.qualified){memset(p.bytes,0,sizeof(p.bytes));++capture.failedPages;}
  return &p;
 }
 bool copy(uint64_t gpu,unsigned size,uint8_t*out,uint64_t kind){
  uint64_t end;if(!XeMediaRootCandidate::add(gpu,0,size,end))return false;
  for(unsigned off=0;off<size;){
   auto p=page(gpu+off,kind);if(!p || !p->qualified)return false;
   unsigned pos=(gpu+off)&4095,n=4096-pos;if(n>size-off)n=size-off;
   if(out)memcpy(out+off,p->bytes+pos,n);off+=n;
  }
  return true;
 }
 bool root(const uint8_t*bytes,unsigned size){
  capture.schema=1;XeMediaRootCandidate::State s{};
  if(!XeMediaRootCandidate::parse(bytes,size,s)){++capture.refusedRoots;return false;}
  ++capture.acceptedRoots;
  // Candidate surface-state page only; no surface-format/pixel dereference.
  page(s.surface,Surface);
  // First two entries only. Coverage is explicitly partial, never absence proof.
  const unsigned entries=s.descriptorBytes/32<2?s.descriptorBytes/32:2;
  for(unsigned i=0;i<entries;++i){
   uint8_t descriptor[32]{};if(!copy(s.descriptor+32*i,32,descriptor,Descriptor))continue;
   const uint64_t offset=XeMediaRootCandidate::pair(descriptor)&0x0000ffffffffffc0ULL;
   uint64_t gpu;if(!XeMediaRootCandidate::add(s.instruction,offset,512,gpu)){++capture.drops;continue;}
   copy(gpu,512,nullptr,Instruction);
  }
  return true;
 }
};
}
