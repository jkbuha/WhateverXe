#include "kern_raptorlake_p_reference_voltage.hpp"
#include <cassert>
#include <map>
#include <vector>
using namespace RaptorLakeP::ReferenceVoltage;
struct IO {
 bool power=true,held=false;uint32_t reject=0;unsigned faults=0;
 std::map<uint32_t,uint32_t> regs{{0x1010A0,0xABCD1234},{0x16AD00,0x1234}};
 std::vector<std::pair<uint32_t,uint32_t>> writes;
 bool powerHeld(){return power;}unsigned lock(){assert(!held);held=true;return 9;}void unlock(unsigned n){assert(held&&n==9);held=false;}
 uint32_t read(uint32_t r){assert(held&&regs.count(r));return regs[r];}void write(uint32_t r,uint32_t v){assert(held);writes.emplace_back(r,v);if(r!=reject)regs[r]=v;}
 void bankFault(){++faults;}
};
int main(){
 assert(physicalMask(1,12)==4&&physicalMask(3,12)==12&&physicalMask(15,12)==0);
 assert(physicalMask(3,15)==3&&physicalMask(15,15)==15&&physicalMask(1,8)==8);
 assert(!physicalMask(2,12)&&!physicalMask(3,5)&&!physicalMask(3,0));
 Drive d{};assert(select(0,0,20,d)&&d.swing==7&&d.preshoot==0&&d.emphasis==0);
 assert(select(0,1,20,d)&&d.swing==5&&d.emphasis==4);assert(select(1,1,10,d)&&d.swing==2&&d.emphasis==8);
 assert(select(2,1,20,d)&&d.swing==0&&d.emphasis==9);assert(select(3,0,6,d)&&d.swing==0&&d.emphasis==0);
 for(unsigned s=0;s<6;s++)for(unsigned e=0;e<6;e++)for(unsigned rate:{6U,10U,20U,30U,0U,12U}){
  auto before=d;bool valid=s<=3&&e<=3&&s+e<=3&&(rate==6||rate==10||rate==20||rate==30);
  assert(select(s,e,rate,d)==valid);if(!valid)assert(d.swing==before.swing&&d.emphasis==before.emphasis);
 }
 Display13TC::Port p;assert(Display13TC::Port::resolve(5,p));IO io;assert(wake(io,p)==Display13TC::Status::Complete);
 assert(io.regs[0x1010A0]==0xABCD1234&&!io.held&&io.regs[0x16AD00]==0&&io.writes.size()==5);
 for(auto w:io.writes)if(w.first==p.hip)assert(((w.second^0xABCD1234)&~(15U<<16))==0);
 io.power=false;auto count=io.writes.size();assert(wake(io,p)==Display13TC::Status::NoPower&&io.writes.size()==count);
 io.power=true;io.regs[0x16AD00]=~0U;assert(wake(io,p)==Display13TC::Status::Unavailable&&io.regs[p.hip]==0xABCD1234&&!io.held);
 io.regs[0x16AD00]=7;io.reject=0x16AD00;assert(wake(io,p)==Display13TC::Status::Readback&&io.regs[p.hip]==0xABCD1234&&!io.held);
 assert(dpControl(0xABCDEF7F)==0xABCDEF03);assert(framing(0x80000200,true)==0x80040200&&framing(0x80040200,false)==0x80000200);
}
