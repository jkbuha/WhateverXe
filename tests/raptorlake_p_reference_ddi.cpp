#include "kern_raptorlake_p_reference_ddi.hpp"
#include <cassert>
using namespace RaptorLakeP::ReferenceDDI;
struct IO {
 bool valid=true,parent=true,quiet=true,ack=true,drop=false;uint32_t native=1,reg=0x33;unsigned writes=0,retain=0,releases=0,elapsed=0;
 bool qualified(){return valid;}bool parentHeld(){return parent;}bool idle(){return quiet;}uint32_t refs(){return native;}
 uint32_t read(uint32_t r){assert(r==0x45454);return reg;}
 void write(uint32_t r,uint32_t v){assert(r==0x45454);++writes;if(!drop)reg=(v&~0x400U)|(ack&& (v&0x800)?0x400:0);}
 void delayUs(unsigned n){elapsed+=n;}void retainOwner(){++retain;}void releaseOwner(){++releases;}
};
int main(){
 {IO io;Request r;assert(r.get(io,1)==Status::Ready&&io.reg==0xC33&&r.owned&&r.owner==1&&io.retain==1);
  assert(r.get(io,1)==Status::Ready&&io.writes==1&&io.retain==1);assert(r.get(io,2)==Status::Denied&&io.writes==1);
  assert(r.put(io,1)==Status::Busy);io.native=0;io.quiet=false;assert(r.put(io,1)==Status::Busy&&r.owned);io.quiet=true;
  assert(r.put(io,1)==Status::Ready&&!r.owner&&!r.owned&&io.reg==0x33&&io.releases==1);
 }
 {IO io;io.reg|=0xC00;Request r;assert(r.get(io,1)==Status::Ready&&!r.owned&&!io.retain&&!io.writes);io.native=0;assert(r.put(io,1)==Status::Ready&&io.reg==0xC33&&!io.writes);}
 for(unsigned fault=0;fault<5;fault++){
  IO io;Request r;if(fault==0)io.valid=false;if(fault==1)io.parent=false;if(fault==2)io.native=0;if(fault==3)io.reg=~0U;if(fault==4)io.native=~0U;
  assert(r.get(io,1)!=Status::Ready&&!io.writes&&!io.retain);
 }
 {IO io;Request r;io.ack=false;assert(r.get(io,1)==Status::Timeout&&io.elapsed==1000&&r.owned&&io.retain==1);io.native=0;assert(r.put(io,1)==Status::Ready&&io.releases==1);}
 {IO io;Request r;io.drop=true;assert(r.get(io,1)==Status::Readback&&r.owned);io.drop=false;io.native=0;assert(r.put(io,1)==Status::Ready&&!r.owned);}
}
