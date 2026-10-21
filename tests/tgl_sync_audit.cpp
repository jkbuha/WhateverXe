#include "kern_tgl_sync_audit.hpp"
#include <array>
#include <memory>
#include <cassert>
#include <cstdio>
using namespace TGLSyncAudit;
int main(){
 std::array<uint32_t,6> p{0x7a000004,0x00105000,0x41141000,0,0,0};auto t=decode(p.data(),6);
 assert(t.status==Decode::Write && t.space==Space::PPGTT && t.address==0x41141000 && t.data==0 && t.width==8 && t.operation==1);
 assert(decode(nullptr,6).status==Decode::Truncated);for(unsigned n=0;n<6;n++)assert(decode(p.data(),n).status==Decode::Truncated);
 unsigned cases=8;
 for(unsigned n=0;n<256;n++){auto q=p;q[0]=(q[0]&~255U)|n;assert(decode(q.data(),6).status==(n==4?Decode::Write:Decode::Malformed));++cases;}
 for(unsigned op=0;op<4;op++)for(unsigned global=0;global<2;global++){
  auto q=p;q[1]=(q[1]&~(3U<<14))|(op<<14)|(global<<24);auto d=decode(q.data(),6);
  assert(d.status==(op?Decode::Write:Decode::NoWrite));assert(d.operation==op && d.space==(global?Space::GGTT:Space::PPGTT));++cases;
 }
 auto q=p;q[1]=0x00101404;assert(decode(q.data(),6).status==Decode::NoWrite);++cases;
 for(unsigned bit:{6U,25U,31U}){auto q=p;q[1]|=1U<<bit;assert(decode(q.data(),6).status==Decode::Malformed);++cases;}
 for(unsigned bit:{21U,23U}){auto q=p;q[1]|=1U<<bit;assert(decode(q.data(),6).status==Decode::Unsupported);++cases;}
 for(unsigned bit:{8U,11U,12U,13U,14U,15U}){auto q=p;q[0]|=1U<<bit;assert(decode(q.data(),6).status==Decode::Malformed);++cases;}
 for(unsigned bit:{9U,10U}){auto q=p;q[0]|=1U<<bit;assert(decode(q.data(),6).status==Decode::Write);++cases;}
 for(unsigned bit=16;bit<32;bit++){auto q=p;q[0]^=1U<<bit;assert(decode(q.data(),6).status==Decode::NotPipeControl);++cases;}
 q=p;q[2]|=1;assert(decode(q.data(),6).status==Decode::Malformed);q=p;q[3]=0x10000;assert(decode(q.data(),6).status==Decode::Malformed);
 q=p;q[2]=0;q[3]=0;assert(decode(q.data(),6).status==Decode::Malformed);q=p;q[2]=0xfffffffc;q[3]=0x7fff;assert(decode(q.data(),6).status==Decode::Malformed);cases+=4;
 auto c=std::make_unique<TGLHangGraph::Capture>();const uint64_t root=0x12345000;
 assert(mapping(t,root,*c).status==Mapping::NotCaptured);++cases;
 auto fill=[&](unsigned slot,uint64_t gpu,uint64_t physical){auto &page=c->pages[slot];page={};page.q[0]=gpu;page.q[2]=4096;page.q[3]=page.q[5]=1;page.before.rootPhysical=page.after.rootPhysical=root;page.before.physical=page.after.physical=physical;page.before.pte=page.after.pte=physical|3;page.chainBefore.q[4]=page.chainAfter.q[4]=root;};
 c->q[1]=1;fill(0,0x41141000,0x81234000);auto good=*c;
 auto e=mapping(t,root,*c);assert(e.status==Mapping::CapturedWritable && e.physical==0x81234000 && e.pages==1);
 assert(alias(e,0)==Alias::Unknown && alias(e,e.physical)==Alias::Match && alias(e,e.physical+4096)==Alias::Mismatch);cases+=4;
 q=p;q[1]|=1U<<24;assert(mapping(decode(q.data(),6),root,*c).status==Mapping::WrongSpace);++cases;
 assert(mapping(t,root+4096,*c).status==Mapping::WrongRoot);++cases;
 for(unsigned field:{2U,3U,4U,5U,6U}){*c=good;c->pages[0].q[field]^=1;assert(mapping(t,root,*c).status==Mapping::Unstable);++cases;}
 *c=good;c->pages[0].before.pte=c->pages[0].after.pte=0x81234000;assert(mapping(t,root,*c).status==Mapping::NotPresent);++cases;
 *c=good;c->pages[0].before.pte=c->pages[0].after.pte=0x81234001;assert(mapping(t,root,*c).status==Mapping::ReadOnly);++cases;
 *c=good;c->pages[0].after.pte^=4096;assert(mapping(t,root,*c).status==Mapping::Unstable);++cases;
 *c=good;c->pages[0].chainAfter.q[6]=1;assert(mapping(t,root,*c).status==Mapping::Unstable);++cases;
 *c=good;c->pages[0].before.physical=c->pages[0].after.physical=0x91234000;assert(mapping(t,root,*c).status==Mapping::Malformed);++cases;
 *c=good;c->q[1]=2;c->pages[1]=c->pages[0];assert(mapping(t,root,*c).status==Mapping::Malformed);++cases;
 *c=good;q=p;q[2]=0x41141ffc;auto cross=decode(q.data(),6);assert(mapping(cross,root,*c).status==Mapping::NotCaptured);++cases;
 c->q[1]=2;fill(1,0x41142000,0x91234000);assert(mapping(cross,root,*c).status==Mapping::CapturedWritable && mapping(cross,root,*c).pages==2);assert(alias(mapping(cross,root,*c),0x81234ffc)==Alias::Unknown);cases+=2;
 c->q[1]=TGLHangGraph::MaxPages+1;assert(mapping(t,root,*c).status==Mapping::Malformed);++cases;
 printf("PASS %u synchronization decode/evidence cases: address spaces, no-write, reserved/index/LRI modes, malformed lengths/addresses, missing coverage, wrong roots, stale/mutated/readonly PTEs, cross-page writes and independent alias proof\n",cases);
}
