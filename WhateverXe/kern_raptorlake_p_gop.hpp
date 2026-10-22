// Read-only firmware handoff qualification. A candidate range is diagnostic
// evidence, not permission to allocate or rewrite firmware GGTT entries.
#ifndef KERN_RAPTORLAKE_P_GOP_HPP
#define KERN_RAPTORLAKE_P_GOP_HPP
#include "kern_raptorlake_p_display.hpp"
#include "kern_xelp_geometry.hpp"
#include "kern_xelp_memory_layout.hpp"
namespace RaptorLakeP {
enum class GOPStatus : uint64_t {Skipped,Incomplete,NotPanel,OtherScanout,Geometry,Memory,Reservation,MappingMismatch,Complete};
struct GOPCapture {
    uint64_t version {1};GOPStatus status {GOPStatus::Skipped};
    uint64_t width {},height {},pitch {},surface {},gopEnd {},reserved {},usable {},candidate {},
        verifiedPages {},firstPTE {},lastPTE {},mismatchOffset {},mismatchValue {};
};
inline bool displayValue(const DisplayCapture &s,uint32_t offset,uint32_t &value) {
    if(s.count>98)return false;
    bool found=false;
    for(unsigned i=0;i<s.count;++i)if(s.registers[i].offset==offset) {
        if(found || s.registers[i].value==UINT32_MAX)return false;
        value=s.registers[i].value;found=true;
    }
    return found;
}
// No write() interface is required. At most 65536 aligned PTE reads (256MiB),
// always within the verified BAR. Fail closed on an unexpected reservation,
// secondary plane, cursor or non-identity firmware mapping.
template<class IO> GOPCapture captureGOP(IO &io,const DisplayCapture &display,XELPMemory::Layout memory,bool allowed) {
    GOPCapture out;if(!allowed)return out;
    out.status=GOPStatus::Incomplete;
    if(display.version!=2 || display.status!=DisplayStatus::Complete || display.count>98)return out;
    out.status=GOPStatus::NotPanel;
    uint32_t func=0,conf=0;
    if(display.activePipes!=1 || !(display.poweredPipes&1) ||
       !displayValue(display,0x60400,func) || !displayValue(display,0x70008,conf) ||
       !(conf&0x80000000) || !(func&0x80000000) || ((func>>27)&15)!=1 || ((func>>24)&7)!=2)return out;
    out.status=GOPStatus::OtherScanout;
    uint32_t cursor=0;
    if(!displayValue(display,0x70080,cursor) || (cursor&0x3F))return out;
    for(unsigned plane=2;plane<=7;++plane) {
        uint32_t control=0;
        if(!displayValue(display,0x70080+0x100*plane,control) || (control&0x80000000))return out;
    }
    uint32_t ctl=0,stride=0,size=0,position=0,offset=0,surface=0,live=0,ht=0,vt=0,source=0;
    out.status=GOPStatus::Geometry;
    if(!displayValue(display,0x70180,ctl) || !displayValue(display,0x70188,stride) ||
       !displayValue(display,0x70190,size) || !displayValue(display,0x7018C,position) ||
       !displayValue(display,0x701A4,offset) || !displayValue(display,0x7019C,surface) ||
       !displayValue(display,0x701AC,live) || !displayValue(display,0x60000,ht) ||
       !displayValue(display,0x6000C,vt) || !displayValue(display,0x6001C,source))return out;
    const auto geometry=XELPGeometry::linear(ctl,stride,size,position,offset);
    if(!XELPGeometry::timing(geometry,ht,vt) || (surface&4095) || surface!=live ||
       ((source>>16)&0x1FFF)+1!=geometry.width || (source&0x1FFF)+1!=geometry.height)return out;
    out.width=geometry.width;out.height=geometry.height;out.pitch=geometry.pitch;
    out.surface=surface;out.gopEnd=uint64_t(surface)+geometry.bytes;
    out.status=GOPStatus::Memory;
    if(!memory.valid || !memory.bytes || memory.bytes>0x10000000 ||
       !memory.range(surface,geometry.bytes) || !io.readable(0x1082C0) || !io.readable(0x1082C4))return out;
    out.status=GOPStatus::Reservation;
    const auto high=io.read(0x1082C4),low=io.read(0x1082C0),after=io.read(0x1082C4);
    if(high==UINT32_MAX || low==UINT32_MAX || high!=after)return out;
    out.reserved=(uint64_t(high)<<32)|low;
    if(!XELPMemory::reservation(memory,out.reserved) || !memory.range(surface,geometry.bytes,true))return out;
    out.usable=memory.usable();
    // A bounded candidate only. The future submission owner must independently
    // acquire/retain its memory and prove exclusivity before changing any PTE.
    out.candidate=(out.gopEnd+0x1FFFFF)&~0x1FFFFFULL;
    if(!memory.range(out.candidate,0x10000,true))return out;
    const auto pages=memory.bytes/4096;
    if(!io.readable(static_cast<uint32_t>(0x800000+8*pages-4)))return out;
    out.status=GOPStatus::MappingMismatch;
    for(uint32_t i=0;i<pages;++i) {
        const auto reg=0x800000+8*i;
        const auto pte=io.readPTE(reg);
        if(!i)out.firstPTE=pte;
        out.lastPTE=pte;
        if(pte!=memory.pte(uint64_t(i)*4096)){out.mismatchOffset=uint64_t(i)*4096;out.mismatchValue=pte;return out;}
        ++out.verifiedPages;
    }
    out.status=GOPStatus::Complete;return out;
}
}
#endif
