// Native probe boundary only. Normal runtime admission remains unchanged.
#ifndef KERN_RAPTORLAKE_P_NATIVE_CORE_HPP
#define KERN_RAPTORLAKE_P_NATIVE_CORE_HPP
#include "kern_raptorlake_p.hpp"
#include <string.h>
namespace RaptorLakeP { namespace Native {
constexpr uint32_t ProbeOffset=0x238E2,StartOffset=0x23ECC,SKUOffset=0x23C2F;
constexpr uint8_t SKUFind[]{0x81,0xFF,0x86,0x80,0x48,0x9A,0x74,0x25};
constexpr uint8_t SKUReplace[]{0x81,0xFF,0x86,0x80,0xA0,0xA7,0x74,0x25};
constexpr uint8_t ProbeEntry[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC,0x28};
constexpr uint8_t StartEntry[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xEC,0x18,0x01,0x00,0x00};
constexpr uint8_t UUID[]{0xBA,0x3A,0xA1,0xC0,0xFE,0x6B,0x33,0xB3,0x9D,0x85,0x73,0xF8,0x48,0x39,0x4E,0x3D};
inline bool requested(const XELP::Profile &p,XELP::OS os,bool probe,bool native,bool bcs,bool rcs) {
    return eligible(p,os,probe && native) && !bcs && !rcs;
}
inline uint32_t word(const uint8_t *p) {uint32_t v;memcpy(&v,p,4);return v;}
inline bool exactImage(const void *image,size_t size) {
    const auto b=static_cast<const uint8_t *>(image);
    if(!b || size<StartOffset+sizeof(StartEntry) || word(b)!=0xFEEDFACF || word(b+4)!=0x1000007)return false;
    const auto commands=word(b+16),bytes=word(b+20);
    if(!commands || commands>128 || bytes>size-32)return false;
    size_t pos=32;bool uuid=false;
    for(unsigned i=0;i<commands;++i) {
        if(pos>32+bytes || 32+bytes-pos<8)return false;
        const auto kind=word(b+pos),length=word(b+pos+4);
        if(length<8 || length>32+bytes-pos)return false;
        if(kind==0x1B) {
            if(uuid || length!=24 || memcmp(b+pos+8,UUID,16))return false;
            uuid=true;
        }
        pos+=length;
    }
    constexpr uint8_t split[]{0x7F,0x15},selector[]{0x41,0xC7,0x86,0x20,0x11,0,0,2,0,0,0};
    return uuid && pos==32+bytes && !memcmp(b+ProbeOffset,ProbeEntry,sizeof(ProbeEntry)) &&
        !memcmp(b+StartOffset,StartEntry,sizeof(StartEntry)) && !memcmp(b+SKUOffset,SKUFind,sizeof(SKUFind)) &&
        !memcmp(b+0x23C25,split,sizeof(split)) && !memcmp(b+0x23C5C,selector,sizeof(selector));
}
enum Field : unsigned {Version,Registered,Armed,LoadRequested,LoadResult,PatchReady,PublishAttempts,Published,
    ProbeEntered,ProbeReturned,ProbeMatched,StartBlocked,Identity,Revision,Selector,Failure,Fields};
struct Record {uint64_t q[Fields]{1};};
}}
#endif
