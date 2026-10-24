#include "kern_raptorlake_p_training_route.hpp"
#include <cassert>
#include <map>
#include <vector>
using namespace RaptorLakeP::TrainingRoute;
struct IO {
 bool powered=true;uint32_t reject=0;unsigned reads=0;
 std::map<uint32_t,uint32_t> regs{{0x46144,0x123},{0x61400,0x1100},{0x61540,0},{0x64500,0xC0},{0x71008,0}};
 std::vector<std::pair<uint32_t,uint32_t>> writes;
 bool powerHeld(){return powered;}
 uint32_t read(uint32_t r){++reads;assert(regs.count(r));return regs[r];}
 void write(uint32_t r,uint32_t v){assert(r==0x46144||r==0x61400);writes.emplace_back(r,v);if(r!=reject)regs[r]=v;}
};
int main(){
 // RPL-P/ADL-P link-rate field is independent of width, reversal and ownership.
 for(unsigned rate:{6U,10U,20U,30U}){
  const unsigned code=rate==6?0:rate==10?1:rate==20?2:3;
  for(uint32_t bits:{0U,0x80003642U,0xFFFFFFFFU}){
   uint32_t result=0;assert(bufferLinkRate(bits,rate,result));
   assert(((result>>20)&15)==code && (result&~0x00F00000U)==(bits&~0x00F00000U));
  }
 }
 for(unsigned rate:{0U,1U,12U,54U}){uint32_t result=0x1234;assert(!bufferLinkRate(0,rate,result)&&result==0x1234);}

 Candidate good{true,true,true,false,false,false,0x4B98,1,0,0};assert(candidate(good));
 for(unsigned fault=0;fault<11;fault++){
  auto c=good;switch(fault){case 0:c.owner=false;break;case 1:c.controller=false;break;case 2:c.access=false;break;case 3:c.builtin=true;break;case 4:c.joined=true;break;case 5:c.connected=true;break;case 6:c.classSize--;break;case 7:c.index=0;break;case 8:c.index=3;break;case 9:c.reserved=1;break;case 10:c.assigned=1;break;}assert(!candidate(c));
 }
 for(unsigned lanes:{1U,2U}){
  IO io;auto saved=io.regs;Route r;assert(r.prepare(io,lanes)==Status::Ready);
  assert(io.regs[0x46144]==0x80000123 && io.regs[0x61400]==(0x32001100|((lanes-1)<<1)));
  assert(r.prepare(io,lanes==1?2:1)==Status::Ready);assert(r.restore(io)==Status::Ready&&io.regs==saved&&!r.active&&!r.captured);
 }
 {IO io;Route r;assert(r.prepare(io,2,true)==Status::Denied&&io.writes.empty());
  assert(r.prepare(io,2)==Status::Ready);r.adopted=true;
  io.regs[0x46144]=0;io.regs[0x61540]=0x80040300;
  auto nw=io.writes.size();assert(r.prepare(io,2)==Status::Denied&&io.writes.size()==nw);
  assert(r.prepare(io,2,true)==Status::Ready&&io.regs[0x46144]==0x80000000);
  nw=io.writes.size();io.regs[0x64500]|=0x80000000;assert(r.prepare(io,2,true)==Status::Busy&&io.writes.size()==nw);
 }
 for(unsigned mode=0;mode<12;mode++){
  IO io;Route r;if(mode==0)io.powered=false;
  if(mode==1)io.regs[0x46144]=~0U;if(mode==2)io.regs[0x61400]=0x80000000;
  if(mode==3)io.regs[0x61540]=0x80000000;if(mode==4)io.regs[0x64500]=0x80000080;
  if(mode==5)io.regs[0x71008]=0x40000000;if(mode==6)io.regs[0x64500]=0x40;
  if(mode==7)io.regs[0x46144]=0x10000000;if(mode==8)io.regs[0x61400]=0x08000000;
  auto result=r.prepare(io,mode==9?0:mode==10?4:mode==11?3:2);
  assert(result!=Status::Ready&&io.writes.empty());if(!mode)assert(!io.reads);
 }
 for(uint32_t reject:{0x46144U,0x61400U}){
  IO io;auto saved=io.regs;Route r;io.reject=reject;assert(r.prepare(io,2)==Status::Readback&&r.active);
  io.reject=0;assert(r.restore(io)==Status::Ready&&io.regs==saved);
 }
 {IO io;Route r;assert(r.prepare(io,2)==Status::Ready);auto nw=io.writes.size();
  r.adopted=true;assert(r.restore(io)==Status::Denied&&io.writes.size()==nw);r.adopted=false;
  io.regs[0x64500]|=0x80000000;assert(r.restore(io)==Status::Busy&&io.writes.size()==nw);io.regs[0x64500]&=~0x80000000U;
  io.regs[0x46144]^=1;assert(r.restore(io)==Status::Foreign&&io.writes.size()==nw);io.regs[0x46144]^=1;
  assert(r.restore(io)==Status::Ready);
 }
}
