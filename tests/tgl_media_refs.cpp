#include "kern_tgl_media_refs.hpp"
#include <fstream>
#include <iterator>
#include <vector>
#include <memory>
#include <cassert>
#include <cstdio>
using namespace XeMediaReferenceCandidate;
int main(){std::vector<uint32_t> root(42);root[0]=0x11000001;root[3]=0x7a000004;root[9]=0x69041301;
 root[10]=0x6101000e;root[14]=0x40005001;root[16]=0x40004001;root[20]=0x4069e001;
 root[26]=0x70000007;root[35]=0x70020002;root[37]=64;root[38]=0xc0;root[39]=0x18c00001;
 std::vector<uint8_t>b(root.size()*4);memcpy(b.data(),root.data(),b.size());unsigned controls=0;
 for(unsigned mode=0;mode<8;++mode){
  auto c=std::make_unique<Capture>();unsigned calls=0;
  auto read=[&](uint64_t gpu,uint8_t*out){++calls;memset(out,0x73,4096);
   if(gpu==0x40004000){uint64_t a=mode&2?0xffffffffffc0ULL:0x1000,second=0x2fc0;memcpy(out+0xc0,&a,8);memcpy(out+0xe0,&second,8);}
   if(mode&1){memset(out,0x88,4096);return false;}return true;
  };Collector<decltype(read)> x{*c,read};auto in=b;
  if(mode&4)in[0]=0xff;
  bool result=x.root(in.data(),in.size());assert(result==!(mode&4));
  if(mode&4){assert(calls==0&&c->refusedRoots==1);++controls;continue;}
  if(mode&1){assert(c->failedPages==2);for(unsigned i=0;i<c->pages;++i)for(auto v:c->page[i].bytes)assert(v==0);++controls;continue;}
  assert(c->acceptedRoots==1);assert(c->pages==((mode&2)?4U:5U));
  unsigned old=calls;assert(x.root(in.data(),in.size()));assert(calls==old); // same snapshot deduplicated
  assert(c->drops==((mode&2)?2U:0U));++controls;
 }
 auto c=std::make_unique<Capture>();unsigned calls=0;auto r=[&](uint64_t,uint8_t*p){++calls;memset(p,0,4096);return true;};Collector<decltype(r)>x{*c,r};
 for(unsigned i=0;i<MaxPages;++i)assert(x.page(0x1000ULL*(i+1),Descriptor));assert(calls==MaxPages);assert(!x.page(0x100000,Descriptor)&&c->drops==1);assert(!x.page(0,Descriptor));assert(!x.page(0x800000000000ULL,Descriptor));
 uint8_t out[64];assert(!x.copy(0x7fffffffffffULL,2,out,Descriptor));assert(calls==MaxPages);controls+=4;
 printf("PASS %u OWN reader/failure/dedup/overflow/page-limit controls; bounded synthetic reader\n",controls);
}
