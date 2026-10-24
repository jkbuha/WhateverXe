#include <cassert>
#include <cstdint>
#include <map>
#include <vector>
#include "kern_raptorlake_p_reference_tc_aux.hpp"
using namespace RaptorLakeP::ReferenceTCAux;
struct IO {
 std::map<uint32_t,uint32_t> regs{{0x45404,0xC03},{0x42000,1U<<25},{0x161508,0x35},{0x44470,0x40000},{0x45454,3},{0x45444,3},{0x64500,0x80},{0x64510,0x800}};
 unsigned reads{},writes{},pauses{},auxCalls{};uint32_t failPower{},nativeResult{};bool fuse=true,foreign=false,badCaps=false,disconnect=false;std::vector<uint32_t> order;
 uint32_t read(uint32_t r){++reads;assert(regs.count(r));auto v=regs[r];if(r==0x161508&&disconnect&&writes)return 4;if(r==0x42000&&!fuse)return 0;
  if(r==0x45404&&r!=failPower)v=(v&~4U)|((v&8)?4:0);
  if((r==0x45444||r==0x45454)&&r!=failPower)v=(v&~0x400U)|((v&0x800)?0x400:0);return v;
 }
 void write(uint32_t r,uint32_t v){assert(r!=0x45454);assert(r==0x45404||r==0x45444||r==0x45454||r==0x64500||r==0x64510);
  if(r==0x45454&&(v&0x800))assert((read(0x45404)&12)==12);
  if(r==0x45404&&!(v&8)){assert(!(regs[0x45454]&0x800)&&!(regs[0x45444]&0x800)&&!(regs[0x64500]&0x40));}
  regs[r]=r==0x45404?v&~4U:(r==0x45444||r==0x45454)?v&~0x400U:v;++writes;order.push_back(r);
 }
 void pause(){++pauses;}
 uint32_t readDPCD(uint8_t *data,unsigned n){assert(n==16&&(read(0x45404)&12)==12&&read(0x45454)==3&&(read(0x45444)&0xC00)==0xC00);++auxCalls;data[0]=badCaps?0:0x12;data[1]=0x14;data[2]=0x84;if(foreign)regs[0x64500]|=1U<<31;return nativeResult;}
};
int main(){
 IO io;auto r=probeWithParent(io,false);assert(r.aux.status==Denied&&!io.reads&&!io.writes);
 io={};r=probeWithParent(io,true);assert(r.aux.status==Complete&&r.aux.version==2&&r.aux.writes==8&&io.auxCalls==1&&r.coreBefore==0xC03&&r.coreRequested==0xC0F&&r.coreAfter==0xC03&&r.parentFlags==1);assert(io.order.front()==0x45404&&io.order.back()==0x45404);
 io={};io.regs[0x45404]|=8;r=probeWithParent(io,true);assert(r.aux.status==Complete&&r.parentFlags==2&&(r.coreAfter&8)&&r.aux.writes==6);
 for(auto reg:{0x45404U,0x161508U,0x44470U}){io={};io.regs[reg]=0;r=probeWithParent(io,true);assert(r.aux.status==Preflight&&!io.writes&&!io.auxCalls);}
 io={};io.regs[0x45404]=~0U;r=probeWithParent(io,true);assert(r.aux.status==Preflight&&!io.writes);
 io={};io.failPower=0x45404;r=probeWithParent(io,true);assert(r.aux.status==11&&r.aux.writes==2&&io.pauses==50&&r.coreAfter==0xC03&&!io.auxCalls);
 io={};io.fuse=false;r=probeWithParent(io,true);assert(r.aux.status==12&&r.aux.writes==2&&io.pauses==5&&r.coreAfter==0xC03&&!io.auxCalls);
 io={};io.failPower=0x45454;r=probeWithParent(io,true);assert(r.aux.status==Complete&&r.coreAfter==0xC03&&io.auxCalls==1);
 io={};io.failPower=0x45444;r=probeWithParent(io,true);assert(r.aux.status==AuxPower&&r.coreAfter==0xC03&&!io.auxCalls);
 io={};io.disconnect=true;r=probeWithParent(io,true);assert(r.aux.status==Preflight&&r.coreAfter==0xC03&&!io.auxCalls&&r.aux.writes==2);
 io={};auto direct=probe(io,true,true);assert(direct.status==Preflight&&!io.writes);
 io={};io.nativeResult=0xE00002D8;r=probeWithParent(io,true);assert(r.aux.status==DPCD&&r.coreAfter==0xC03&&io.auxCalls==1);
 io={};io.badCaps=true;r=probeWithParent(io,true);assert(r.aux.status==DPCD&&r.coreAfter==0xC03);
 io={};io.foreign=true;r=probeWithParent(io,true);assert(r.aux.status==Rollback&&(r.parentFlags&4)&&(r.coreAfter&8)&&(io.regs[0x64500]&0x40));
 io={};io.regs[0x45404]|=1U<<13;r=probeWithParent(io,true);assert(r.aux.status==Foreign&&!io.writes);
 for(unsigned bit=0;bit<32;bit++){if(bit==2||bit==3||(bit>=12&&bit<=17))continue;io={};auto extra=1U<<bit;io.regs[0x45404]|=extra;r=probeWithParent(io,true);assert(r.aux.status==Complete&&(r.coreAfter&extra));}
}
