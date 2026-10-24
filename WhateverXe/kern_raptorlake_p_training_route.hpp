// Pipe-B routing for an owned TC3 training path. The caller holds native
// controller/pool exclusion, a retained path/FB/port, and a Display13 PipeB
// power lease. Never access this bank with only the TC PHY/AUX lease.
#pragma once
#include <stdint.h>
namespace RaptorLakeP {namespace TrainingRoute {
enum class Status:uint32_t {Ready,Denied,Unavailable,Busy,Foreign,Readback};
struct Candidate {
 bool owner{},controller{},access{},builtin{},joined{},connected{};
 uint32_t classSize{},index{},reserved{},assigned{};
};
inline bool candidate(const Candidate &c){
 return c.owner && c.controller && c.access && c.classSize==0x4B98 && c.index>0 && c.index<3 &&
  !c.builtin && !c.joined && !c.connected && !c.reserved && !c.assigned;
}
struct Route {
 bool captured{},active{},adopted{};
 uint32_t clock{},function{},lastClock{},lastFunction{};
 template<class IO> Status prepare(IO &io,unsigned lanes){
  if(adopted || !io.powerHeld() || (lanes!=1 && lanes!=2))return Status::Denied;
  const auto c=io.read(0x46144),f=io.read(0x61400),t=io.read(0x61540),b=io.read(0x64500),p=io.read(0x71008);
  if(c==~0U || f==~0U || t==~0U || b==~0U || p==~0U)return Status::Unavailable;
  if(((f|t|b)&0x80000000U) || (p&0xC0000000U) || !(b&0x80))return Status::Busy;
  if(((c&0xF0000000U) && (c&0xF0000000U)!=0x60000000U) ||
     ((f&0x78000000U) && (f&0x78000000U)!=0x30000000U))return Status::Foreign;
  if(!captured){clock=c;function=f;captured=true;}
  const auto wantedClock=(c&0x0FFFFFFFU)|0x60000000U;
  const auto wantedFunction=(f&~0x7F00000EU)|0x32000000U|((lanes-1)<<1);
  // Mark ownership before the first write so failed readback retains cleanup.
  active=true;lastClock=wantedClock;lastFunction=wantedFunction;
  io.write(0x46144,wantedClock);
  if(io.read(0x46144)!=wantedClock)return Status::Readback;
  io.write(0x61400,wantedFunction);
  return io.read(0x61400)==wantedFunction?Status::Ready:Status::Readback;
 }
 template<class IO> Status restore(IO &io){
  if(!active)return Status::Ready;
  if(adopted || !captured || !io.powerHeld())return Status::Denied;
  const auto c=io.read(0x46144),f=io.read(0x61400),t=io.read(0x61540),b=io.read(0x64500),p=io.read(0x71008);
  if(c==~0U || f==~0U || t==~0U || b==~0U || p==~0U)return Status::Unavailable;
  if(((f|t|b)&0x80000000U) || (p&0xC0000000U) || !(b&0x80))return Status::Busy;
  if(c!=clock && c!=lastClock)return Status::Foreign;
  if(f!=function && f!=lastFunction)return Status::Foreign;
  io.write(0x61400,function);if(io.read(0x61400)!=function)return Status::Readback;
  io.write(0x46144,clock);if(io.read(0x46144)!=clock)return Status::Readback;
  active=captured=false;return Status::Ready;
 }
};
}}
