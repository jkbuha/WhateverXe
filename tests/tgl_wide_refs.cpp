#include "kern_tgl_ppgtt.hpp"
#include "kern_tgl_hang_refs.hpp"
#include <cassert>
#include <cstring>
#include <memory>
#include <vector>
static void put(std::vector<uint8_t> &b,unsigned o,uint32_t v){memcpy(b.data()+o,&v,4);}
int main(){
 std::vector<uint8_t> raw(131072);
 put(raw,0,0x61010014);put(raw,16,0x200001);put(raw,24,0x300001);put(raw,40,0x400001);
 // Long packet walk exceeds the legacy 4096-step and 16KiB limits.
 put(raw,65528,0x782A0000);put(raw,65532,0x100);
 auto c=std::make_unique<TGLHangRefs::Capture>();unsigned reads=0;
 auto read=[&](TGLHangRefs::Page &p){++reads;p.q[4]=1;};
 TGLHangRefs::collect(*c,raw.data(),raw.size(),0x100000,0x110000,read);
 assert(c->q[8]==0 && c->q[5]==0x100000 && c->q[9]==0x200000 && c->q[12]==0x100);
 assert(reads>0 && reads<=TGLHangRefs::MaxPages && c->q[6]<=TGLHangRefs::MaxPages);
 *c={};reads=0;
 TGLHangRefs::collect(*c,raw.data()+49152,16384,0x10c000,0x10fffc,read);
 assert(c->q[8]==1 && reads==0); // original small receipt cannot see anchor
 for(uint64_t bb:{uint64_t(0xffffc),uint64_t(0x120000),uint64_t(0x110001)}){
  *c={};reads=0;TGLHangRefs::collect(*c,raw.data(),raw.size(),0x100000,bb,read);assert(c->q[8]==1 && reads==0);
 }
 *c={};TGLHangRefs::collect(*c,raw.data(),131073,0x100000,0x110000,read);assert(c->q[8]==1);
}
