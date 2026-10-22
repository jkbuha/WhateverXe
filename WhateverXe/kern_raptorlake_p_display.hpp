// Passive Display 13 GOP snapshot. Linux v6.12 i915_reg.h defines
// XELPD_PW_CTL_IDX_PW_A..D as 5..8. No TGL three-pipe assumption and no
// register writes, power requests, AUX transactions or plane changes.
#ifndef KERN_RAPTORLAKE_P_DISPLAY_HPP
#define KERN_RAPTORLAKE_P_DISPLAY_HPP
#include "kern_raptorlake_p.hpp"
namespace RaptorLakeP {
enum class DisplayStatus : uint32_t {Skipped,Complete,Unavailable,PowerChanged};
struct DisplayCapture {
    uint32_t version {2};DisplayStatus status {DisplayStatus::Skipped};
    uint32_t count {},poweredPipes {},activePipes {},powerBefore {},powerAfter {};
    Register registers[98] {};
};
template<class IO> DisplayCapture captureDisplay(IO &io,bool allowed) {
    DisplayCapture out;if(!allowed)return out;
    constexpr uint32_t powerReg=0x45404,powerMask=0x15400;
    constexpr uint32_t local[]{0x70008,0x60000,0x60004,0x60008,0x6000C,0x60010,0x60014,
        0x6001C,0x70080,0x70280,0x70380,0x70480,0x70580,0x70680,0x70780,0x70180,0x70188,0x7018C,0x70190,0x7019C,0x701A4,0x701AC};
    out.status=DisplayStatus::Unavailable;
    if(!io.readable(powerReg))return out;
    for(unsigned pipe=0;pipe<4;++pipe) {
        if(!io.readable(0x60400+0x1000*pipe))return out;
        for(auto reg:local)if(!io.readable(reg+0x1000*pipe))return out;
    }
    const auto add=[&](uint32_t reg) {
        const auto value=io.read(reg);out.registers[out.count++]={reg,value};
        if(value==UINT32_MAX)out.status=DisplayStatus::Unavailable;
        return value;
    };
    out.status=DisplayStatus::Complete;out.powerBefore=add(powerReg);
    if(out.status!=DisplayStatus::Complete)return out;
    for(unsigned pipe=0;pipe<4;++pipe) {
        const uint32_t bit=1U<<(2*(5+pipe));
        if(!(out.powerBefore&bit))continue;
        const auto now=io.read(powerReg);
        if(now==UINT32_MAX){out.status=DisplayStatus::Unavailable;return out;}
        if((now&powerMask)!=(out.powerBefore&powerMask)){out.status=DisplayStatus::PowerChanged;return out;}
        out.poweredPipes|=1U<<pipe;
        const auto function=add(0x60400+0x1000*pipe);
        if(out.status!=DisplayStatus::Complete)return out;
        if(!(function&0x80000000))continue;
        out.activePipes|=1U<<pipe;
        for(auto reg:local) {
            add(reg+0x1000*pipe);
            if(out.status!=DisplayStatus::Complete)return out;
        }
    }
    out.powerAfter=add(powerReg);
    if(out.status==DisplayStatus::Complete && (out.powerAfter&powerMask)!=(out.powerBefore&powerMask))
        out.status=DisplayStatus::PowerChanged;
    return out;
}
}
#endif
