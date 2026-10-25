// Pipe-B routing for an owned TC3 training path. The caller holds native
// controller/pool exclusion, a retained path/FB/port, and a Display13 PipeB
// power lease. Never access this bank with only the TC PHY/AUX lease.
#pragma once
#include <stdint.h>
namespace RaptorLakeP {namespace TrainingRoute {
// Display13 selects a PHY in TRANS_CLK_SEL: TC3 is PHY index7, hence8.
// The native port number5 remains correct in TRANS_DDI_FUNC_CTL (selection6).
// Linux v6.12 intel_ddi_enable_transcoder_clock distinguishes these namespaces.
inline bool bufferLinkRate(uint32_t value,unsigned rate,uint32_t &out){
 uint32_t code;
 switch(rate){case 6:code=0;break;case 10:code=1;break;case 20:code=2;break;case 30:code=3;break;default:return false;}
 out=(value&~0x00F00000U)|(code<<20);return true;
}
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
 template<class IO> Status prepare(IO &io,unsigned lanes,bool native=false){
  if(adopted!=native || !io.powerHeld() || (lanes!=1 && lanes!=2))return Status::Denied;
  const auto c=io.read(0x46144),f=io.read(0x61400),t=io.read(0x61540),b=io.read(0x64500),p=io.read(0x71008);
  if(c==~0U || f==~0U || t==~0U || b==~0U || p==~0U)return Status::Unavailable;
  if(((f|b)&0x80000000U) || (!native&&(t&0x80000000U)) || (p&0xC0000000U) || !(b&0x80))return Status::Busy;
  if(((c&0xF0000000U) && (c&0xF0000000U)!=0x80000000U) ||
     ((f&0x78000000U) && (f&0x78000000U)!=0x30000000U))return Status::Foreign;
  // End-of-training cleanup can restore the temporary route before native
  // adoption. Recapture only after ownership, power, idle and port-selection
  // checks above; a cleared temporary snapshot does not revoke native ownership.
  if(!captured){clock=c;function=f;captured=true;}
  const auto wantedClock=(c&0x0FFFFFFFU)|0x80000000U;
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
