#include "kern_tgl_hang_resolve.hpp"
#include <cassert>
#include <memory>
#include <set>
#include <cstdio>
using namespace TGLHangResolve;
constexpr uint64_t K=0xffff800000000000ULL,Root=0x12345000,Base=0x4113c000;
template<class T>void put(uint8_t *p,unsigned at,T v){memcpy(p+at,&v,sizeof(v));}
int main(){
 auto c=std::make_unique<TGLHangDetail::Capture>();auto out=std::make_unique<Capture>();
 c->q[0]=3;c->before[TGLHangDetail::Root]=Root;c->before[TGLHangDetail::Owner]=K;
 c->before[TGLHangDetail::LRCSize]=8192;memcpy(c->after,c->before,sizeof(c->before));c->q[10]=3;
 c->graph.q[1]=1;auto &p=c->graph.pages[0];p.q[0]=0x40007000;p.q[2]=4096;p.q[3]=p.q[5]=1;p.before.rootPhysical=p.after.rootPhysical=Root;
 put(c->regs,0x68,0x400072d4ULL);uint32_t pc[]={0x7a000004,0x00105000,0x41141000,0,0,0,0x05000000};memcpy(p.bytes+0x2bc,pc,sizeof(pc));
 put(c->lrc,0x1628,0x61010014U);put(c->lrc,0x1628+40,Base|1);
 c->graph.q[2]=1;c->graph.draws[0].q[2]=TGLHangGraph::VS|TGLHangGraph::PS;
 for(unsigned i=7;i<10;++i)c->graph.draws[0].q[i]=0x4d00;
 unsigned reads=0;auto read=[&](uint64_t at,void *v,unsigned n)->uint64_t{assert(at==K+0x1474 && n==1);*static_cast<uint8_t*>(v)=reads++&1;return 1;};
 std::set<uint64_t> pages;auto pageRead=[&](TGLHangGraph::Page &q){pages.insert(q.q[0]);};
 collect(*out,*c,read,pageRead);assert(!out->q[1] && out->q[2]==5 && out->q[5]==0x41141000 && out->q[6]==1 && out->q[7]==1 && out->q[8]==Base);
 assert(out->q[12]==3 && out->q[9]==0 && out->q[10]==1);assert((pages==std::set<uint64_t>{Base,Base+4096,Base+16384,Base+20480,Base+24576}));
 unsigned cases=1;
 for(unsigned mode=0;mode<9;++mode){auto d=std::make_unique<TGLHangDetail::Capture>(*c);
  switch(mode){case 0:d->q[0]=2;break;case 1:d->q[1]=1;break;case 2:d->after[0]^=1;break;case 3:d->before[TGLHangDetail::Root]=d->after[TGLHangDetail::Root]=0;break;
   case 4:d->graph.q[1]=97;break;case 5:d->graph.q[2]=65;break;case 6:d->q[10]=0;break;case 7:d->graph.pages[0].q[3]=0;break;case 8:put(d->graph.pages[0].bytes,0x2c0,0x01105000U);break;}
  pages.clear();collect(*out,*d,read,pageRead);
  if(mode<6)assert(out->q[1]==1 && pages.empty());
  if(mode==6)assert(!out->q[7] && !pages.count(Base));
  if(mode>=7)assert(!out->q[5]);++cases;
 }
 // No-write packets must not produce synchronization mapping requests.
 put(p.bytes,0x2c0,0x00101404U);collect(*out,*c,read,pageRead);assert(!out->q[5]);++cases;
 put(p.bytes,0x2c0,0x00105000U);
 auto r=std::make_unique<TGLTahoeResolveReceipt::Receipt>();r->q[0]=1;r->q[1]=2;r->q[3]=K+0x2000;r->q[10]=64;r->q[13]=TGLTahoeResolveReceipt::ScratchSize;r->q[17]=K+0x8000;
 put(r->mapped,0,r->q[17]);put(r->mapped,0x20,uint64_t(TGLTahoeResolveReceipt::ScratchSize));put(r->mapped,0x30,K+0x6000);put(r->mapped,0x38,K+0x4000);
 uint8_t live[64],map[0x128]{};memcpy(live,r->mapped,64);put(map,0,K+0x10000);put(map,0x98,Base);
 unsigned calls=0;bool mutate=false,missing=false;
 auto sr=[&](uint64_t at,void *v,unsigned n)->uint64_t{
  if(at==r->q[3] && n==64){memcpy(v,live,64);if(mutate && calls++)static_cast<uint8_t*>(v)[0x38]^=0x10;return 64;}
  if(at==K+0x6000 && n==sizeof(map)){memcpy(v,map,n);return n;}
  if(at==K+0x10000+0x128 && n==8){const auto getter=K+0x12000;memcpy(v,&getter,8);return 8;}
  if(at==K+0x12000 && n==14){memcpy(v,GPUGetterBytes,14);return 14;}
  assert(false);return 0;
 };
 auto physical=[&](uint64_t cpu,uint64_t &phys){phys=cpu-K+0x800000;return !missing;};Scratch s{};
 scratch(s,0,*r,r->q[17],K+0x10000,sr,physical);assert(!s.q[1] && s.q[4]==Base && s.q[5]==K+0x4000);
 for(unsigned i=0;i<6;++i)assert(s.pages[i][3]==7 && s.pages[i][1]==0x804000+i*4096);++cases;
 missing=true;scratch(s,0,*r,r->q[17],K+0x10000,sr,physical);assert(!s.q[1]);for(auto &row:s.pages)assert(!row[3]);++cases;missing=false;
 mutate=true;calls=0;scratch(s,0,*r,r->q[17],K+0x10000,sr,physical);assert(s.q[1]==2);++cases;mutate=false;
 for(unsigned field:{0U,0x20U,0x30U,0x38U}){live[field]^=1;scratch(s,0,*r,r->q[17],K+0x10000,sr,physical);assert(s.q[1]==1);live[field]^=1;++cases;}
 map[0x10]=0x40;scratch(s,0,*r,r->q[17],K+0x10000,sr,physical);assert(s.q[1]==1);map[0x10]=0;++cases;
 put(map,0x98,uint64_t(0));scratch(s,0,*r,r->q[17],K+0x10000,sr,physical);assert(s.q[1]==1);put(map,0x98,Base);++cases;
 r->q[13]=0;scratch(s,0,*r,r->q[17],K+0x10000,sr,physical);assert(s.q[1]==1);++cases;
 printf("PASS %u failure-only resolve evidence cases; no GPU/system access; capture bytes=%zu\n",cases,sizeof(Capture));
}
