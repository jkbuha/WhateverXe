#include <cassert>
#include <cstdint>
#include <map>
#include "kern_raptorlake_p_reference_tc_aux.hpp"
using namespace RaptorLakeP::ReferenceTCAux;
struct IO {
 std::map<uint32_t,uint32_t> regs{{0x45404,0xC03},{0x161508,0x35},{0x44470,0x40000},{0x45454,3},{0x45444,3},{0x64500,0x80},{0x64510,0x800}};
 unsigned reads{},writes{},pauses{},auxCalls{};uint32_t failPower{},result{};bool badCaps{},foreignScanout{},disconnect{};
 uint32_t read(uint32_t r){++reads;assert(regs.count(r));auto v=regs[r];if((r==0x45444||r==0x45454)&&r!=failPower){v=(v&~0x400U)|((v&0x800)?0x400:0);}if(disconnect&&writes>=2&&r==0x161508)return 4;return v;}
 void write(uint32_t r,uint32_t v){assert(regs.count(r));assert(r!=0x45404&&r!=0x161508&&r!=0x44470);regs[r]=(r==0x45444||r==0x45454)?v&~0x400U:v;++writes;}
 void pause(){++pauses;}
 uint32_t readDPCD(uint8_t *data,unsigned n){++auxCalls;assert(n==16&&(read(0x45454)&0xC00)==0xC00&&(read(0x45444)&0xC00)==0xC00&&(read(0x64500)&0x40)&&!(read(0x64510)&0x800));data[0]=badCaps?0:0x12;data[1]=0x14;data[2]=0x84;if(foreignScanout)regs[0x64500]|=1U<<31;return result;}
};
int main(){
 assert(connected(0x35,1U<<18)&&connected(0x3D,1U<<18));
 for(auto tc:{0U,4U,6U,7U,UINT32_MAX})assert(!connected(tc,1U<<18));
 for(auto hpd:{0U,1U<<2,(1U<<18)|(1U<<2),UINT32_MAX})assert(!connected(0x35,hpd));
 Result caps;caps.status=Complete;caps.nativeResult=0;caps.dpcd[0]=0x81C21412;
 uint32_t rate=0,lanes=0;assert(sinkLimits(caps,rate,lanes)&&rate==20&&lanes==2);
 for(auto fault:{0U,1U,2U,3U}){auto bad=caps;if(fault==0)bad.status=DPCD;if(fault==1)bad.nativeResult=1;if(fault==2)bad.dpcd[0]=0x81C20012;if(fault==3)bad.dpcd[0]=0x81C31412;
  rate=99;lanes=99;assert(!sinkLimits(bad,rate,lanes)&&rate==99&&lanes==99);
 }
 IO io;auto r=probe(io,false);assert(r.status==Denied&&!io.reads&&!io.writes);
 io={};r=probe(io,true);assert(r.status==Complete&&r.nativeResult==0&&r.dpcd[0]==0x841412&&io.auxCalls==1&&r.writes==8&&io.regs[0x45454]==3&&io.regs[0x45444]==3&&io.regs[0x64500]==0x80&&io.regs[0x64510]==0x800);
 for(unsigned bit=0;bit<32;++bit){if(bit==10||bit==11)continue;io={};auto extra=1U<<bit;io.regs[0x45454]|=extra;io.regs[0x45444]|=extra;r=probe(io,true);assert(r.status==Complete&&(r.after[0]&extra)&&(r.after[1]&extra));}
 io={};io.regs[0x45454]|=0x800;io.regs[0x45444]|=0x800;r=probe(io,true);assert(r.status==Complete&&(r.after[0]&0x800)&&(r.after[1]&0x800));
 for(auto reg:{0x45404U,0x161508U,0x44470U}){io={};io.regs[reg]=0;r=probe(io,true);assert(r.status==Preflight&&!io.writes&&!io.auxCalls);}
 for(auto reg:{0x45454U,0x45444U}){io={};io.regs[reg]=~0U;r=probe(io,true);assert(r.status==Preflight&&!io.writes);}
 io={};io.failPower=0x45454;r=probe(io,true);assert(r.status==DDIPower&&!io.auxCalls&&io.pauses==50&&io.regs[0x45454]==3);
 io={};io.failPower=0x45444;r=probe(io,true);assert(r.status==AuxPower&&!io.auxCalls&&io.pauses==500&&io.regs[0x45454]==3&&io.regs[0x64500]==0x80);
 for(auto value:{0x40U,0x80000000U,~0U}){io={};io.regs[0x64500]=value;r=probe(io,true);assert(r.status==Foreign&&!io.auxCalls&&io.writes==2);}
 io={};io.regs[0x64510]=1U<<31;r=probe(io,true);assert(r.status==AuxBusy&&!io.auxCalls&&io.regs[0x64500]==0x80);
 io={};io.disconnect=true;r=probe(io,true);assert(r.status==Ownership&&!io.auxCalls&&!(io.regs[0x64500]&0x40));
 io={};io.badCaps=true;r=probe(io,true);assert(r.status==DPCD&&io.auxCalls==1&&!(io.regs[0x64500]&0x40));
 io={};io.result=0xE00002D8;r=probe(io,true);assert(r.status==DPCD&&r.nativeResult==0xE00002D8&&!(io.regs[0x64500]&0x40));
 io={};io.foreignScanout=true;r=probe(io,true);assert(r.status==Rollback&&(io.regs[0x64500]&0x40)&&(io.regs[0x45454]&0x800)&&(io.regs[0x45444]&0x800));
}
