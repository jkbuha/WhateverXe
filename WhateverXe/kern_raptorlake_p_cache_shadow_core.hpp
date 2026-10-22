// RAM-only native cache replay and passive register enumeration.
#ifndef KERN_RAPTORLAKE_P_CACHE_SHADOW_CORE_HPP
#define KERN_RAPTORLAKE_P_CACHE_SHADOW_CORE_HPP
#include <stdint.h>
#include <stddef.h>
namespace RaptorLakeP {namespace CacheShadow {
constexpr size_t Bytes=0x1000000,Guard=4096,Count=108;
constexpr uint32_t Sentinel=0xA5A5A5A5;
inline uint32_t offset(size_t i) {
    if(i<2)return i?0x900C:0x9008;
    if(i<10)return 0x4800+4*static_cast<uint32_t>(i-2);
    if(i<12)return i==10?0xB134:0xB138;
    if(i<44)return 0xB020+4*static_cast<uint32_t>(i-12);
    return 0x4000+4*static_cast<uint32_t>(i-44);
}
// 109 native stores, 108 distinct words: MOCS[9] overwrites 0x4024.
inline uint32_t expected(size_t i,const uint8_t *pat,const uint32_t *l3,const uint32_t *mocs) {
    if(i<2)return i?0xFF9FFFFF:0xFF3FFFFF;
    if(i<10)return pat[i-2];
    if(i<12)return i==10?0xD0000020:0;
    if(i<44)return l3[i-12];
    return mocs[i-44];
}
struct Check {uint64_t changed{},unexpected{},guards{},firstMismatch{UINT64_MAX};};
inline Check verify(const uint32_t *allocation,const uint8_t *pat,const uint32_t *l3,const uint32_t *mocs) {
    Check out;
    for(size_t i=0;i<Guard/4;++i)if(allocation[i]!=Sentinel || allocation[(Guard+Bytes)/4+i]!=Sentinel)++out.guards;
    const auto memory=allocation+Guard/4;
    for(size_t i=0;i<Bytes/4;++i) {
        uint32_t wanted=Sentinel;
        if(i==0x9008/4)wanted=0xFF3FFFFF;
        else if(i==0x900C/4)wanted=0xFF9FFFFF;
        else if(i>=0x4800/4 && i<0x4820/4)wanted=pat[i-0x4800/4];
        else if(i==0xB134/4)wanted=0xD0000020;
        else if(i==0xB138/4)wanted=0;
        else if(i>=0xB020/4 && i<0xB0A0/4)wanted=l3[i-0xB020/4];
        else if(i>=0x4000/4 && i<0x4100/4)wanted=mocs[i-0x4000/4];
        if(memory[i]!=Sentinel)++out.changed;
        if(memory[i]!=wanted){++out.unexpected;if(out.firstMismatch==UINT64_MAX)out.firstMismatch=i*4;}
    }
    return out;
}
struct Wake {
    uint64_t acquired{},released{},failure{};
    bool accept(bool owner,bool pointers,uint32_t on,uint32_t domain) {
        if(!owner || !pointers || domain!=5 || on>1 ||
            (on?(acquired || released):(!acquired || released))){failure=1;return false;}
        if(on)++acquired;else ++released;return true;
    }
};
}}
#endif
