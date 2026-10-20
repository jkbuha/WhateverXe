// Pre-training DDI-A route, ported from the guarded panel backend.
#pragma once
#include <stdint.h>
namespace TGLTahoePanelClock {
struct Report {uint32_t version=1,status=0,power=0,ioPower=0,clockBefore=0,clockAfter=0,
 functionBefore=0,functionAfter=0,buffer=0,training=0,pathClockBefore=0,pathFunction=0;};
// The host proves physical identity, controller/map ownership, native mode,
// object layout and MMIO bounds before admission. No power requests here.
template<class IO> bool apply(IO &io,bool admitted,uint32_t pipe,uint32_t lanes,
 uint32_t rate,uint32_t &pathClock,uint32_t pathFunction,Report &r) {
 r=Report{};r.pathClockBefore=pathClock;r.pathFunction=pathFunction;
 if(!admitted || pipe!=0 || lanes!=2 || rate!=10) return false;
 r.power=io.read(0x45404);r.ioPower=io.read(0x45454);
 if(r.power==UINT32_MAX || r.ioPower==UINT32_MAX || (r.power&0x3F)!=0x3F || (r.ioPower&3)!=3) {r.status=1;return false;}
 r.clockBefore=io.read(0x46140);r.functionBefore=io.read(0x60400);
 r.buffer=io.read(0x64000);r.training=io.read(0x60540);
 if(r.buffer==UINT32_MAX || r.training==UINT32_MAX ||
  ((r.buffer|r.training)&0x80000000U) ||
  (r.clockBefore!=0 && r.clockBefore!=0x10000000U) ||
  (r.functionBefore!=0x02000002U && r.functionBefore!=0x0A000002U) ||
  (pathClock!=0 && pathClock!=0x10000000U) || pathFunction!=0x8A000002U) {r.status=2;return false;}
 io.write(0x46140,0x10000000U);r.clockAfter=io.read(0x46140);
 if(r.clockAfter!=0x10000000U) {io.write(0x46140,r.clockBefore);(void)io.read(0x46140);r.status=3;return false;}
 io.write(0x60400,0x0A000002U);r.functionAfter=io.read(0x60400);
 if(r.functionAfter!=0x0A000002U) {
  io.write(0x60400,r.functionBefore);(void)io.read(0x60400);
  io.write(0x46140,r.clockBefore);(void)io.read(0x46140);r.status=4;return false;
 }
 pathClock=0x10000000U;r.status=5;return true;
}
}
