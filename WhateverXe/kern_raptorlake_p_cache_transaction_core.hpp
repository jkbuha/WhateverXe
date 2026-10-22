// Bounded cache-policy qualification. No engine submission or layout changes.
#ifndef KERN_RAPTORLAKE_P_CACHE_TRANSACTION_CORE_HPP
#define KERN_RAPTORLAKE_P_CACHE_TRANSACTION_CORE_HPP
#include "kern_raptorlake_p_cache_shadow_core.hpp"
namespace RaptorLakeP {namespace CacheTransaction {
constexpr size_t Count=103;
struct Result {uint64_t version{1},requested{},qualified{},snapshot{},writes{},verified{},writeMismatch{},
    unexpected{},restoreWrites{},restored{},remaining{},firstMismatch{UINT64_MAX},failure{};};
inline uint32_t offset(size_t i) {
    if(i<8)return 0x4800+4*static_cast<uint32_t>(i);
    if(i<40)return 0xB020+4*static_cast<uint32_t>(i-8);
    const auto m=static_cast<uint32_t>(i-40);return 0x4000+4*(m>=61?m+1:m);
}
inline uint32_t mask(size_t i){return offset(i)==0xB098?0xFFFF:UINT32_MAX;}
inline uint32_t target(size_t i,uint32_t before,const uint8_t *pat,const uint32_t *l3,const uint32_t *mocs) {
    const auto o=offset(i);uint32_t value;
    if(i<8)value=pat[i];else if(i<40)value=l3[i-8];else value=mocs[(o-0x4000)/4];
    return (before&~mask(i))|(value&mask(i));
}
inline bool validPlan(const uint8_t *pat,const uint32_t *l3,const uint32_t *mocs) {
    if(!pat||!l3||!mocs)return false;
    for(unsigned i=0;i<8;++i)if(pat[i]>3)return false;
    for(unsigned i=0;i<64;++i)if(mocs[i]&~0x7FU)return false;
    for(unsigned i=0;i<32;++i)for(unsigned half=0;half<2;++half){auto v=(l3[i]>>(16*half))&0xFFFF;if(v!=0x10&&v!=0x30)return false;}
    return true;
}
inline size_t indexOf(uint32_t o) {
    for(size_t i=0;i<CacheShadow::Count;++i)if(CacheShadow::offset(i)==o)return i;
    return CacheShadow::Count;
}
// Firmware scanout's special MOCS61 is preserved in both global and L3 tables.
// 9008/900C and B134/B138 layout registers are never written. PAT0 retains WB.
// IO writes only known policy registers; the caller holds GT/render forcewake.
template<class IO> void transact(IO &io,const uint32_t (*baseline)[3],const uint8_t *pat,
        const uint32_t *l3,const uint32_t *mocs,Result &out) {
    out.requested=1;
    if(!out.qualified || !baseline || !validPlan(pat,l3,mocs)){out.failure=1;return;}
    uint32_t saved[CacheShadow::Count]{};
    for(size_t i=0;i<CacheShadow::Count;++i) {
        auto o=CacheShadow::offset(i);saved[i]=io.read(o);
        if(baseline[i][0]!=o || baseline[i][1]!=baseline[i][2] || saved[i]!=baseline[i][1] ||
            saved[i]==UINT32_MAX || io.read(o)!=saved[i]){out.failure=2;out.firstMismatch=o;return;}
    }
    // Firmware identity GGTT pages use PAT0; don't alter their caching policy.
    if(saved[indexOf(0x4800)]!=pat[0] || pat[0]!=3){out.failure=2;out.firstMismatch=0x4800;return;}
    out.snapshot=1;
    for(size_t i=0;i<Count;++i) {
        const auto o=offset(i),before=saved[indexOf(o)],value=target(i,before,pat,l3,mocs);
        if(io.read(o)!=before){out.failure=3;out.firstMismatch=o;break;}
        io.write(o,value);++out.writes;
        if(io.read(o)!=value){++out.writeMismatch;out.firstMismatch=o;out.failure=4;break;}
        ++out.verified;
    }
    for(size_t j=0;j<CacheShadow::Count;++j) {
        const auto o=CacheShadow::offset(j);uint32_t wanted=saved[j];
        for(size_t i=0;i<out.writes;++i)if(offset(i)==o)wanted=target(i,saved[j],pat,l3,mocs);
        if(io.read(o)!=wanted)++out.unexpected;
    }
    // Restore every attempted write, including a failed posting read. Preserve
    // protected bits even if an unexpected external writer changed them.
    for(size_t n=out.writes;n;--n) {
        auto i=n-1;const auto o=offset(i),m=mask(i);
        io.write(o,(io.read(o)&~m)|(saved[indexOf(o)]&m));++out.restoreWrites;
        if(io.read(o)==saved[indexOf(o)])++out.restored;
    }
    for(size_t i=0;i<CacheShadow::Count;++i)if(io.read(CacheShadow::offset(i))!=saved[i])++out.remaining;
    if((out.unexpected || out.remaining || out.writes!=Count || out.restored!=out.writes) && !out.failure)out.failure=5;
}
}}
#endif
