#include "kern_tgl_hang_prestop.hpp"
#include <cassert>
#include <map>
#include <cstdio>
using namespace TGLHangPreStop;
int main() {
 constexpr uint64_t owner=0xffff800000001000ULL,scheduler=owner+0x2000,
  streamer=owner+0x4000,item=owner+0x6000,context=owner+0x8000,
  ring=owner+0xa000,stamps=owner+0xc000;
 const std::map<uint64_t,uint64_t> good{{scheduler+0x10,owner},{scheduler+0xca0,streamer},
  {streamer+0x10,owner},{streamer+0x30,item},{streamer+0x38,0},{streamer+0x40,0},{streamer+0x48,0},
  {item,context},{context+0x7c,6},{context+0x89,0xc0402d9319},{context+0xb0,ring},
  {context+0x6c,0},{context+0x50,owner},{ring+0x30,stamps},{ring+0x38,1},
  {ring+0x8a0,10},{ring+0x10,owner},{stamps+64,637}};
 unsigned cases=0;
 for(unsigned mode=0;mode<16;++mode) {
  auto m=good;unsigned probes=0,acks=0,ownerReads=0,allMMIO=0,gtAcks=0;
  if(mode==1)m[scheduler+0x10]=0;
  if(mode==7)m.erase(context+0x7c);
  auto read=[&](uint64_t a,void*out,unsigned n)->uint64_t {
   if(a==scheduler+0x10 && ++ownerReads==2 && mode==6)return 0;
   auto it=m.find(a);if(it==m.end())return 0;memcpy(out,&it->second,n);return n;
  };
  auto mmio=[&](uint32_t reg)->uint32_t {
   ++allMMIO;
   if(reg==0xd84){++acks;if(mode==2)return 0;if(mode==3)return UINT32_MAX;if(mode==4&&acks==3)return 0;if(mode==5&&acks==3)return UINT32_MAX;return 2;}
   if(reg==0x130044){++gtAcks;if(mode==12)return 0;if(mode==13)return UINT32_MAX;if(mode==14&&gtAcks==2)return 0;if(mode==15&&gtAcks==2)return UINT32_MAX;return 2;}
   if(reg==0x2084){++probes;if(mode==8&&probes==1)return UINT32_MAX;if(mode==9&&probes==2)return UINT32_MAX;return probes==1?0x12340446:mode==10?0x56780446:0x12340446;}
   return reg;
  };
  Capture c{};c.q[2]=100;CCTLCapture extra{};
  collect(c,owner,scheduler,read,mmio,mode!=11,&extra);
  assert(c.q[0]==2&&extra.q[0]==1&&extra.q[4]==100&&extra.q[5]==0);
  if(mode==1||mode==2||mode==3||mode==11||mode==12||mode==13)assert(probes==0&&extra.q[1]==1);
  else if(mode==4||mode==5||mode==14||mode==15)assert(probes==1&&extra.q[1]==2);
  else if(mode==6||mode==7)assert(probes==1&&extra.q[1]==3&&extra.q[6]==c.q[1]);
  else if(mode==8||mode==9)assert(probes==2&&extra.q[1]==4);
  else assert(probes==2&&extra.q[1]==0&&extra.q[2]==0x12340446&&extra.q[3]==(mode==10?0x56780446:0x12340446)&&allMMIO==75);
  ++cases;
 }
 // Explicitly show that enabling the witness changes no main capture bytes.
 for(unsigned gt=0;gt<2;++gt) {
  auto read=[&](uint64_t a,void*out,unsigned n)->uint64_t {auto it=good.find(a);if(it==good.end())return 0;memcpy(out,&it->second,n);return n;};
  auto mmio=[](uint32_t reg)->uint32_t {return reg==0xd84||reg==0x130044?2:reg;};
  Capture old{},observed{};CCTLCapture extra{};
  collect(old,owner,scheduler,read,mmio,gt);
  collect(observed,owner,scheduler,read,mmio,gt,&extra);
  assert(!memcmp(&old,&observed,sizeof old)&&extra.q[1]==(gt?0:1)&&extra.q[2]==(gt?0x2084:0)&&extra.q[3]==(gt?0x2084:0));
  ++cases;
 }
 static_assert(sizeof(Capture)==1152,"main ABI preserved");
 static_assert(sizeof(CCTLCapture)==64,"new pointer-free witness");
 printf("PASS %u actual preSTOP CCTL admitted/refused/lostwake/invalid/changing pair/mainABI byte equality controls; no writes/noGPU\n",cases);
}
