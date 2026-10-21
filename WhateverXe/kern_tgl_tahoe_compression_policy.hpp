// Exact-donor comparison: keep the accelerator renderCompression policy off.
// Does not change IOSurface storage flags, resources, shaders or GPU packets.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace TGLTahoeCompressionPolicy {
constexpr size_t Start=0x23ECC,Dispatch=0x2B4F2,Zero=0x246AD,Startup=0x246C3;
constexpr size_t Immediate=0x246E0,Select=0x2B685,Setter=0x2B872,Selector=0x959D1;
constexpr uint8_t StartupBytes[]={0x45,0x88,0xBD,0x74,0x14,0,0,0x45,0x38,0xBD,0x8C,0x12,0,0,0x75,0x0E,0x41,0xF6,0x06,0x20,0x74,0x08,0x41,0xC6,0x85,0x74,0x14,0,0,1};
constexpr uint8_t ZeroBytes[]={0x45,0x31,0xFF};
constexpr uint8_t SelectBytes[]={0x48,0x8D,0x35,0x45,0xA3,0x06,0,0xBA,0,1,0,0,0x4C,0x89,0xE7,0xE8,0,0,0,0,0x85,0xC0,0x0F,0x84,0xD1,0x01,0,0};
constexpr uint8_t SetterBytes[]={0x41,0x8A,0x07,0x41,0x88,0x86,0x74,0x14,0,0,0xEB,0x16};
// MOV AL,0; NOP retains instruction extent and flags. The native store and
// success continuation stay intact; the caller's byte is never modified.
constexpr uint8_t SetterOff[]={0xB0,0,0x90};
inline bool kernel(uint64_t p,size_t n){return (p>>47)==0x1FFFF && p<=UINT64_MAX-n;}
template<class Read>bool resolvesTo(uint64_t target,uint64_t expected,Read read){
 if(!kernel(expected,1))return false;
 uint64_t seen[3]{};
 for(unsigned hop=0;hop<3;++hop){
  if(!kernel(target,6))return false;if(target==expected)return true;
  for(unsigned i=0;i<hop;++i)if(target==seen[i])return false;seen[hop]=target;
  uint8_t bytes[6]{};if(read(target,bytes,6)!=6)return false;
  int32_t delta=0;
  if(bytes[0]==0xFF && bytes[1]==0x25){
   memcpy(&delta,bytes+2,4);const auto slot=target+6+static_cast<int64_t>(delta);
   if(!kernel(slot,8) || read(slot,&target,8)!=8)return false;
  }else if(bytes[0]==0xE9){memcpy(&delta,bytes+1,4);target=target+5+static_cast<int64_t>(delta);}
  else return false;
 }
 return target==expected;
}
inline bool matches(const uint8_t *image,size_t size,size_t start,size_t dispatch,uint64_t liveBase=0,uint64_t compareTarget=0){
 if(!image || size<Selector+18)return false;
 int32_t displacement=0;memcpy(&displacement,image+Select+16,4);
 // This is an external BRANCH relocation to _strncmp in the exact donor.
 // Offline accepts its zero addend; live admission requires the resolved
 // admitted branch target, rather than ignoring the relocated operand.
 // Production separately validates direct/loader-stub jumps to _strncmp.
 const bool call=liveBase ? compareTarget && liveBase+Select+20+static_cast<int64_t>(displacement)==compareTarget : displacement==0 && !compareTarget;
 return image && size>=Selector+18 && start==Start && dispatch==Dispatch &&
  call &&
  !memcmp(image+Zero,ZeroBytes,sizeof(ZeroBytes)) &&
  !memcmp(image+Startup,StartupBytes,sizeof(StartupBytes)) &&
  !memcmp(image+Select,SelectBytes,16) && !memcmp(image+Select+20,SelectBytes+20,sizeof(SelectBytes)-20) &&
  !memcmp(image+Setter,SetterBytes,sizeof(SetterBytes)) &&
  !memcmp(image+Selector,"renderCompression",18);
}
// Bounded transaction, including a writer which mutates then reports failure.
template<class Write>bool apply(uint8_t *image,size_t size,size_t start,size_t dispatch,Write write,uint64_t liveBase=0,uint64_t compareTarget=0){
 if(!matches(image,size,start,dispatch,liveBase,compareTarget))return false;
 const uint8_t off=0;
 if(write(Immediate,StartupBytes+29,&off,1) &&
    write(Setter,SetterBytes,SetterOff,sizeof(SetterOff)) &&
    image[Immediate]==0 && !memcmp(image+Setter,SetterOff,sizeof(SetterOff)))return true;
 if(!memcmp(image+Setter,SetterOff,sizeof(SetterOff)))write(Setter,SetterOff,SetterBytes,sizeof(SetterOff));
 if(image[Immediate]==0)write(Immediate,&off,StartupBytes+29,1);
 return false;
}
}
