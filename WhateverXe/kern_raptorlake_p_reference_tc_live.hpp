#pragma once
#include <stdint.h>
namespace RaptorLakeP {namespace ReferenceTCLive {
enum class Status:uint32_t {Ready,Denied,Foreign,Disconnected,ParentPower,Fuse,Ownership,AuxBusy,BaseMode,AuxPower,Busy,Rollback};
// Caller serializes the exact controller/TC3 port and excludes concurrent AUX.
// Native AUX reference counting is retained; never clear its request directly.
struct Lease {
 uintptr_t owner{},object{};
 bool parent{},phy{},aux{},route{},base{},active{},auxRequest{};
 uint32_t savedRoute{},nativeResult{},writes{},last{};
 template<class IO> bool wait(IO &io,uint32_t reg,uint32_t bits,unsigned ms){
  for(unsigned i=0;i<=ms;i++){auto v=io.read(reg);if(v==~0U)return false;if((v&bits)==bits)return true;if(i<ms)io.pause();}return false;
 }
 template<class IO> void write(IO &io,uint32_t reg,uint32_t value){io.write(reg,value);++writes;}
 template<class IO> bool connected(IO &io){auto t=io.read(0x161508),h=io.read(0x44470);return t!=~0U&&(t&7)==5&&h!=~0U&&(h&(1U<<18))&&!(h&(1U<<2));}
 template<class IO> bool witness(IO &io,uintptr_t who,uintptr_t port){
  if(!active || owner!=who || object!=port)return false;
  auto core=io.read(0x45404);if(core==~0U || (core&12)!=12)return false;
  auto b=io.read(0x64500),a=io.read(0x45444);
  return b!=~0U && (b&64) && a!=~0U && (a&0xC00)==0xC00 && connected(io);
 }
 template<class IO> Status release(IO &io,uintptr_t who,uintptr_t port){
  if(!who||!port||owner!=who||object!=port)return Status::Foreign;
  auto core=io.read(0x45404);
  if(core==~0U || (core&12)!=12)return Status::Rollback;
  auto buffer=io.read(0x64500);
  if(buffer==~0U || (buffer&(1U<<31)))return Status::Busy;
  if((buffer&64)&&!phy)return Status::Foreign;
  if(aux){io.auxDisable();aux=false;}
  auto auxiliary=io.read(0x45444);
  auto bypassRefs=io.auxBypassRefs();
  // fAlwaysOn preserves logical AUX counts while bypassing native MMIO. A
  // request owned here is releasable only after every native reference drains.
  if(auxRequest){
   if(auxiliary==~0U || bypassRefs!=0)return Status::Busy;
   write(io,0x45444,auxiliary&~(1U<<11));auxRequest=false;
   auxiliary=io.read(0x45444);
  }else if(bypassRefs!=~0U && bypassRefs)return Status::Busy;
  if(auxiliary==~0U || (auxiliary&(1U<<11)))return Status::Busy;
  if(base){nativeResult=io.baseMode(0);if(nativeResult)return Status::BaseMode;base=false;}
  auto ctl=io.read(0x64510);
  if(ctl==~0U || (ctl&(1U<<31)))return Status::AuxBusy;
  if(route){
   constexpr uint32_t w1c=(1U<<30)|(1U<<28)|(1U<<25);
   write(io,0x64510,(ctl&~(w1c|(1U<<11)))|savedRoute);route=false;
  }
  if(phy){buffer=io.read(0x64500);if(buffer==~0U || (buffer&(1U<<31)))return Status::Busy;if(buffer&64)write(io,0x64500,buffer&~64U);if(io.read(0x64500)&64)return Status::Rollback;phy=false;}
  active=false;
  if(parent){
   core=io.read(0x45404);auto ddi=io.read(0x45454);auxiliary=io.read(0x45444);
   if(core==~0U || ddi==~0U || auxiliary==~0U || (core&0x3F000) || ((ddi|auxiliary)&0xAAAAAAA0U))return Status::Busy;
   write(io,0x45404,core&~8U);if(io.read(0x45404)&8)return Status::Rollback;parent=false;
  }
  owner=object=0;return Status::Ready;
 }
 template<class IO> Status acquire(IO &io,uintptr_t who,uintptr_t port,bool admitted){
  if(!admitted||!who||!port)return Status::Denied;
  if(owner && (owner!=who||object!=port))return Status::Foreign;
  if(active)return witness(io,who,port)?Status::Ready:Status::Rollback;
  if(owner){auto r=release(io,who,port);if(r!=Status::Ready)return r;}
  auto core=io.read(0x45404);if(core==~0U || (core&0xC03)!=0xC03 || !connected(io))return Status::Disconnected;
  if(!(core&8)&&(core&0x3F000))return Status::Foreign;
  owner=who;object=port;
  auto fail=[&](Status why){last=static_cast<uint32_t>(why);auto cleanup=release(io,who,port);return cleanup==Status::Ready?why:Status::Rollback;};
  if(!(core&8)){parent=true;write(io,0x45404,core|8);}
  // Parent failure has no powered port accesses or native callbacks.
  if(!wait(io,0x45404,4,50)){
   if(parent){auto v=io.read(0x45404);if(v==~0U)return Status::Rollback;write(io,0x45404,v&~8U);if(io.read(0x45404)&8)return Status::Rollback;parent=false;}
   owner=object=0;return Status::ParentPower;
  }
  if(!wait(io,0x42000,1U<<25,5))return fail(Status::Fuse);
  auto buffer=io.read(0x64500);
  if(buffer==~0U || (buffer&0x80000040U))return fail(Status::Foreign);
  phy=true;write(io,0x64500,buffer|64);
  if((io.read(0x64500)&0x80000040U)!=64)return fail(Status::Ownership);
  auto ctl=io.read(0x64510);
  if(ctl==~0U || (ctl&(1U<<31)))return fail(Status::AuxBusy);
  savedRoute=ctl&(1U<<11);
  if(savedRoute){route=true;write(io,0x64510,ctl&~((1U<<11)|(1U<<30)|(1U<<28)|(1U<<25)));}
  nativeResult=io.baseMode(3);if(nativeResult)return fail(Status::BaseMode);base=true;
  io.auxEnable();aux=true;
  auto auxiliary=io.read(0x45444);
  if(auxiliary==~0U)return fail(Status::AuxPower);
  if(!(auxiliary&(1U<<11))){
   auto bypassRefs=io.auxBypassRefs();
   if(bypassRefs==1){auxRequest=true;write(io,0x45444,auxiliary|(1U<<11));}
   else if(bypassRefs!=~0U)return fail(Status::Foreign);
  }
  if(!wait(io,0x45444,1U<<10,500))return fail(Status::AuxPower);
  if(!connected(io))return fail(Status::Disconnected);
  active=true;last=0;return Status::Ready;
 }
};
}}
