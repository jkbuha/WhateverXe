#include "kern_raptorlake_p_reference_snapshot.hpp"
#include <cassert>
#include <map>
#include <vector>
using namespace RaptorLakeP::ReferenceSnapshot;
struct IO {
 std::map<uint32_t,uint32_t> values;
 std::vector<uint32_t> reads;
 uint32_t unavailable=UINT32_MAX,powerChangeAt{},liveChangeAt{};
 bool readable(uint32_t reg){return reg!=unavailable && !(reg&3) && reg<0x1000000;}
 uint32_t read(uint32_t reg){
  reads.push_back(reg);
  if(reg==0x45404 && powerChangeAt && reads.size()>=powerChangeAt)return 0;
  if(reg==0x701AC && liveChangeAt && reads.size()>=liveChangeAt)return values[reg]+4096;
  return values[reg];
 }
};
static IO active(unsigned mask=1){
 IO io;
 for(unsigned p=0;p<4;++p)if(mask&(1U<<p)){
  io.values[0x45404]|=1U<<(2*(5+p));io.values[0x60400+0x1000*p]=0x80000000;
  io.values[0x70180+0x1000*p]=0x80000000;io.values[0x701AC+0x1000*p]=0x12345000;
 }
 return io;
}
int main(){
 static_assert(sizeof(Snapshot)==1568,"snapshot wire layout");
 {IO io;auto s=capture(io,19);assert(s.version==1&&s.sequence==19&&s.status==Complete&&s.count==2&&!s.activePipes);assert(io.reads==std::vector<uint32_t>({0x45404,0x45404}));}
 for(unsigned mask=1;mask<16;++mask){auto io=active(mask);auto s=capture(io,7);assert(s.status==Complete&&s.poweredPipes==mask&&s.activePipes==mask&&s.count<=192);for(auto r:io.reads)assert(!(r&3)&&r<0x1000000);}
 {auto io=active();io.values[0x60400]=0;auto s=capture(io,1);assert(s.status==Complete&&!s.activePipes&&s.count==4);}
 {auto io=active();io.unavailable=0x45404;auto s=capture(io,1);assert(s.status==Unavailable&&io.reads.empty());}
 {auto io=active();io.unavailable=0x70188;auto s=capture(io,1);assert(s.status==Unavailable);for(auto reg:io.reads)assert(reg!=0x70188);}
 {auto io=active();io.values[0x70188]=UINT32_MAX;auto s=capture(io,1);assert(s.status==Unavailable);}
 {auto io=active();io.powerChangeAt=2;auto s=capture(io,1);assert(s.status==PowerChanged&&io.reads.size()==2);}
 {auto io=active();io.liveChangeAt=27;auto s=capture(io,1);assert(s.status==BufferChanged);}
 {auto io=active();io.values[0x701AC]=0xFFFFE000;auto s=capture(io,1);assert(s.status==Complete);bool high=false;for(auto r:io.reads)if(r==0xFFFFFC)high=true;assert(high);}
 {auto io=active();io.values[0x701AC]=0xFFFFF000;auto s=capture(io,1);assert(s.status==Complete);for(auto r:io.reads)assert(r<0x800000);}
 {auto io=active();auto initial=capture(io,1);io.reads.clear();io.powerChangeAt=initial.count;auto s=capture(io,2);assert(s.status==PowerChanged);}
}
