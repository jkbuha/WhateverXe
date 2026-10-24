// TC3's native logical DDI reference may bypass physical power when the
// reference donor sets fAlwaysOn. Own only a request acquired by this adapter.
#pragma once
#include <stdint.h>
namespace RaptorLakeP {namespace ReferenceDDI {
enum class Status:uint32_t {Ready,Denied,Unavailable,Busy,Timeout,Readback};
struct Request {
 uintptr_t owner{};bool owned{};
 template<class IO> Status get(IO &io,uintptr_t who){
  if(!who || !io.qualified() || !io.parentHeld() || !io.refs() || io.refs()==~0U)return Status::Denied;
  if(owner && owner!=who)return Status::Denied;
  auto value=io.read(0x45454);if(value==~0U)return Status::Unavailable;
  if(!(value&0x800)){
   if(!owner){owner=who;io.retainOwner();}owned=true;io.write(0x45454,value|0x800);
   if(!(io.read(0x45454)&0x800))return Status::Readback;
  }
  for(unsigned us=0;us<=1000;us++){
   value=io.read(0x45454);if(value==~0U)return Status::Unavailable;
   if((value&0xC00)==0xC00)return Status::Ready;
   if(!(value&0x800))return Status::Readback;
   if(us<1000)io.delayUs(1);
  }
  return Status::Timeout;
 }
 template<class IO> Status put(IO &io,uintptr_t who){
  if(!owned)return Status::Ready;
  if(owner!=who || !io.qualified() || !io.parentHeld())return Status::Denied;
  if(io.refs() || !io.idle())return Status::Busy;
  auto value=io.read(0x45454);if(value==~0U)return Status::Unavailable;
  io.write(0x45454,value&~0x800U);if(io.read(0x45454)&0x800)return Status::Readback;
  owned=false;owner=0;io.releaseOwner();return Status::Ready;
 }
};
}}
