// Offline guard/branch replay contract; no kext runtime integration.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace TGLIOSurfacePolicyAudit {
constexpr size_t Start=0x23ECC,Zero=0x246AD,Site=0x246C3,Immediate=0x246E0;
constexpr size_t SetterSite=0x2B872;
constexpr uint8_t SetterBytes[]={0x41,0x8A,0x07,0x41,0x88,0x86,0x74,0x14,0,0};
constexpr uint8_t ZeroBytes[]={0x45,0x31,0xFF};
constexpr uint8_t Native[]={0x45,0x88,0xBD,0x74,0x14,0,0,0x45,0x38,0xBD,0x8C,0x12,0,0,
 0x75,0x0E,0x41,0xF6,0x06,0x20,0x74,0x08,0x41,0xC6,0x85,0x74,0x14,0,0,1};
inline bool matches(const uint8_t *b,size_t n,size_t symbol){
 return b && n>=SetterSite+sizeof(SetterBytes) && symbol==Start && !memcmp(b+Zero,ZeroBytes,sizeof(ZeroBytes)) && !memcmp(b+Site,Native,sizeof(Native)) && !memcmp(b+SetterSite,SetterBytes,sizeof(SetterBytes));
}
}
