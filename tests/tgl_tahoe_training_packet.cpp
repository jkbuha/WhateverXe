#include "kern_edp_training_packet.hpp"
#include "kern_edp_framing.hpp"
#include <cassert>
#include <cstdio>
#include <initializer_list>
int main() {
 EdpPLLFrac::Scope s;assert(EdpPLLFrac::begin(s,42,0x1000));
 uint8_t original[4]={0x21,0xAA,0xBB,0xCC},start[3]{};void *data=original;uint32_t length=1;
 assert(EdpTrainingPacket::prepare(s,42,0x1000,true,0x102,data,length,start)==1);
 assert(data==start && length==3 && start[0]==0x21 && !start[1] && !start[2]);assert(original[1]==0xAA);
 data=original;length=4;assert(EdpTrainingPacket::prepare(s,42,0x1000,true,0x103,data,length,start)==2);
 assert(data==original && length==2 && original[3]==0xCC);
 for(unsigned invalid=0;invalid<6;++invalid) {
  data=invalid==0?nullptr:original;length=invalid==1?2:1;auto before=data;auto len=length;
  auto result=EdpTrainingPacket::prepare(s,invalid==2?43:42,invalid==3?0x1001:0x1000,invalid!=4,
   invalid==5?0x101:0x102,data,length,start);
  assert(!result && data==before && length==len);
 }
 original[0]=0;data=original;length=1;assert(!EdpTrainingPacket::prepare(s,42,0x1000,true,0x102,data,length,start));
 for(uint32_t before:{0x80000000U,0x80040000U,0x83000123U})for(uint8_t framing:{uint8_t(0),uint8_t(1)}) {
  auto got=EdpFraming::value(s,42,0x1000,0x60540,before,framing);
  assert(got==((before&~0x40000U)|(uint32_t(framing)<<18)));
 }
 for(uint32_t before:{0U,UINT32_MAX,0x88000000U})assert(EdpFraming::value(s,42,0x1000,0x60540,before,1)==before);
 assert(EdpFraming::value(s,43,0x1000,0x60540,0x80000000,1)==0x80000000);
 assert(EdpFraming::value(s,42,0x1001,0x60540,0x80000000,1)==0x80000000);
 assert(EdpFraming::value(s,42,0x1000,0x60541,0x80000000,1)==0x80000000);
 assert(EdpFraming::value(s,42,0x1000,0x60540,0x80000000,2)==0x80000000);
 EdpPLLFrac::end(s,42);data=original;length=4;
 assert(!EdpTrainingPacket::prepare(s,42,0x1000,true,0x103,data,length,start));assert(length==4);
 puts("PASS two-lane training packets: caller buffer preservation, exact address/length/pattern, scope isolation, SST enhanced framing bit preservation");
}
