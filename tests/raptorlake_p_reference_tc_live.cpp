#include <cassert>
#include <cstdint>
#include <map>
#include "kern_raptorlake_p_reference_tc_live.hpp"
using namespace RaptorLakeP::ReferenceTCLive;
struct IO {
 std::map<uint32_t,uint32_t> regs{{0x46048,0},{0x45404,0xC03},{0x42000,1U<<25},{0x161508,0x35},{0x44470,0x40000},{0x45454,3},{0x45444,3},{0x64500,0x80},{0x64510,0x40000800}};
 unsigned reads{},writes{},pauses{},auxRefs{},enables{},disables{},mode{};uint32_t failedReg{},baseFailure{};bool fuse=true,ownership=true,disconnectOnAux=false,bypass=false,proof=true;unsigned baseCalls{};
 uint32_t read(uint32_t r){assert(regs.count(r));++reads;auto v=regs[r];if(r==0x42000&&!fuse)return 0;if(r==0x161508&&disconnectOnAux&&enables)return 4;
  if(r==0x45404&&failedReg!=r)v=(v&~4U)|((v&8)?4:0);
  if(r==0x45444&&failedReg!=r)v=(v&~0x400U)|((v&0x800)?0x400:0);return v;
 }
 void write(uint32_t r,uint32_t v){assert(r==0x45404||r==0x64500||r==0x64510||r==0x45444);++writes;
  if(r==0x64510){constexpr uint32_t w1c=(1U<<30)|(1U<<28)|(1U<<25);assert(!(v&w1c));regs[r]=(v&~w1c)|(regs[r]&w1c);return;}
  if(r==0x64500&&!ownership)v&=~64U;
  regs[r]=r==0x45404?v&~4U:v;
 }
 void pause(){++pauses;}
 uint32_t baseMode(unsigned v){++baseCalls;if(baseFailure)return baseFailure;mode=v;return 0;}
 void auxEnable(){assert(mode==3&&(read(0x45404)&12)==12&&(read(0x64500)&64));++enables;++auxRefs;if(!bypass)regs[0x45444]|=0x800;}
 void auxDisable(){assert(auxRefs);++disables;if(!--auxRefs&&!bypass)regs[0x45444]&=~0x800U;}
 uint32_t auxBypassRefs(){return bypass&&proof?auxRefs:~0U;}
};
int main(){

 {IO x;Lease p;assert(p.acquire(x,1,2,true)==Status::Ready);
  x.regs[0x161508]=4;x.regs[0x44470]=0;
  assert(!p.witness(x,1,2)&&p.teardownWitness(x,1,2));
  assert(!p.teardownWitness(x,3,2)&&!p.teardownWitness(x,1,3));
  x.regs[0x64500]|=0x80000000;assert(!p.teardownWitness(x,1,2));x.regs[0x64500]&=~0x80000000U;
  x.regs[0x46048]=0xCC000000;
  assert(p.release(x,1,2)==Status::Busy&&p.phy&&p.active&&x.auxRefs==1&&p.aux&&(x.regs[0x64500]&64));
  assert(p.teardownWitness(x,1,2));
  auto savedAux=x.regs[0x45444];x.regs[0x45444]=3;
  assert(p.teardownWitness(x,1,2)); // Direct PLL readout needs retained PW2/PHY, not AUX.
  x.regs[0x45444]=savedAux;
  x.regs[0x46048]=0x40000000;assert(p.release(x,1,2)==Status::Busy&&p.phy);
  x.regs[0x46048]=0;assert(p.release(x,1,2)==Status::Ready&&!p.owner&&!p.phy);
  assert(!p.teardownWitness(x,1,2));
 }
 IO io;Lease l;assert(l.acquire(io,1,2,false)==Status::Denied&&!io.reads&&!io.writes);
 assert(l.acquire(io,0,2,true)==Status::Denied&&!io.reads);
 io={};l={};assert(l.acquire(io,1,2,true)==Status::Ready&&l.active&&l.parent&&l.phy&&l.aux&&l.base&&io.auxRefs==1&&io.mode==3);assert(!(io.regs[0x64510]&0x800));assert(io.regs[0x64510]&(1U<<30));
 auto writes=io.writes;assert(l.acquire(io,1,2,true)==Status::Ready&&io.writes==writes&&io.enables==1);assert(l.acquire(io,3,2,true)==Status::Foreign&&io.writes==writes);assert(l.release(io,1,3)==Status::Foreign&&io.writes==writes);
 assert(l.release(io,1,2)==Status::Ready&&!l.owner&&!l.active&&!l.phy&&!l.parent&&!io.auxRefs&&io.mode==0&&io.regs[0x45404]==0xC03&&io.regs[0x64500]==0x80&&(io.regs[0x64510]&0x800));
 io={};l={};io.regs[0x45404]|=8;assert(l.acquire(io,1,2,true)==Status::Ready&&!l.parent);assert(l.release(io,1,2)==Status::Ready&&(io.regs[0x45404]&8));
 for(auto r:{0x45404U,0x161508U,0x44470U}){io={};l={};io.regs[r]=0;assert(l.acquire(io,1,2,true)==Status::Disconnected&&!io.writes&&!io.enables);}
 io={};l={};io.failedReg=0x45404;assert(l.acquire(io,1,2,true)==Status::ParentPower&&!l.owner&&!io.enables&&io.pauses==50&&io.regs[0x45404]==0xC03);
 io={};l={};io.fuse=false;assert(l.acquire(io,1,2,true)==Status::Fuse&&!l.owner&&!io.enables&&io.pauses==5);
 io={};l={};io.ownership=false;assert(l.acquire(io,1,2,true)==Status::Ownership&&!l.owner&&!io.enables);
 io={};l={};io.failedReg=0x45444;assert(l.acquire(io,1,2,true)==Status::AuxPower&&!l.owner&&io.enables==1&&io.disables==1&&io.pauses==500&&io.mode==0);
 io={};l={};io.disconnectOnAux=true;assert(l.acquire(io,1,2,true)==Status::Disconnected&&!l.owner&&!io.auxRefs&&io.mode==0);
 io={};l={};io.baseFailure=0xE0000001;assert(l.acquire(io,1,2,true)==Status::BaseMode&&!l.owner&&!io.enables);
 io={};l={};io.regs[0x64500]|=64;assert(l.acquire(io,1,2,true)==Status::Rollback&&l.parent&&(io.regs[0x45404]&8)&&!io.enables);
 io={};l={};io.regs[0x64500]|=1U<<31;assert(l.acquire(io,1,2,true)==Status::Rollback&&l.parent&&!io.enables);
 io={};l={};assert(l.acquire(io,1,2,true)==Status::Ready);io.regs[0x64500]|=1U<<31;writes=io.writes;assert(l.release(io,1,2)==Status::Busy&&io.writes==writes&&io.auxRefs==1&&l.parent);io.regs[0x64500]&=~(1U<<31);assert(l.release(io,1,2)==Status::Ready);
 io={};l={};assert(l.acquire(io,1,2,true)==Status::Ready);++io.auxRefs;assert(l.release(io,1,2)==Status::Busy&&io.auxRefs==1&&l.parent&&l.phy&&l.base);io.auxDisable();assert(l.release(io,1,2)==Status::Ready&&!l.owner);
 io={};l={};assert(l.acquire(io,1,2,true)==Status::Ready);io.regs[0x45404]|=1U<<13;assert(l.release(io,1,2)==Status::Busy&&l.parent&&!l.phy&&!l.base);io.regs[0x45404]&=~(1U<<13);assert(l.release(io,1,2)==Status::Ready&&!l.owner);
 io={};l={};io.bypass=true;assert(l.acquire(io,1,2,true)==Status::Ready&&l.auxRequest&&io.auxRefs==1&&(io.read(0x45444)&0xC00)==0xC00);assert(l.release(io,1,2)==Status::Ready&&!l.auxRequest&&!io.auxRefs&&!(io.read(0x45444)&0x800));
 io={};l={};io.bypass=true;assert(l.acquire(io,1,2,true)==Status::Ready);++io.auxRefs;assert(l.release(io,1,2)==Status::Busy&&l.auxRequest&&l.parent&&l.phy&&io.auxRefs==1&&(io.read(0x45444)&0x800));io.auxDisable();assert(l.release(io,1,2)==Status::Ready&&!l.owner&&!l.auxRequest);
 io={};l={};io.bypass=true;io.proof=false;assert(l.acquire(io,1,2,true)==Status::AuxPower&&!l.owner&&!l.auxRequest);
 io={};l={};io.bypass=true;assert(l.acquire(io,1,2,true)==Status::Ready);io.proof=false;assert(l.release(io,1,2)==Status::Busy&&l.auxRequest&&l.parent&&(io.read(0x45444)&0x800));io.proof=true;assert(l.release(io,1,2)==Status::Ready);
 io={};l={};io.bypass=true;io.failedReg=0x45444;assert(l.acquire(io,1,2,true)==Status::AuxPower&&!l.owner&&!l.auxRequest&&!io.auxRefs&&!(io.regs[0x45444]&0x800));
 io={};l={};io.bypass=true;io.auxRefs=1;assert(l.acquire(io,1,2,true)==Status::Rollback&&l.parent&&l.phy&&!l.auxRequest&&io.auxRefs==1);io.auxDisable();assert(l.release(io,1,2)==Status::Ready);
 for(unsigned bit=0;bit<32;bit++){if(bit==2||bit==3||(bit>=12&&bit<=17))continue;io={};l={};auto flag=1U<<bit;io.regs[0x45404]|=flag;assert(l.acquire(io,1,2,true)==Status::Ready);assert(l.release(io,1,2)==Status::Ready&&(io.regs[0x45404]&flag));}
}
