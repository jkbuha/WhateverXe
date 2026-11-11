#ifndef KERN_DISPLAY13_DMC_HPP
#define KERN_DISPLAY13_DMC_HPP
#include <stdint.h>
#include "kern_display13_dmc_fw.hpp"
// Display 13 (ADL-P/RPL-P) main DMC firmware load, following Linux i915
// intel_dmc_load_program() for display version 13: disable the event handlers,
// write the program to its RAM, apply the firmware's MMIO list, then set the
// DC_STATE_DEBUG core/memory-up masks. Only the main program is loaded. Linux
// keeps every pipe DMC event disabled on this generation, and its pipe-C/D
// program RAM sits in pipe wells macOS may have powered off.
// The display engine and PW1 must be powered; the caller checks that. DC
// state policy (DC_STATE_EN) is left to the native driver.
namespace Display13DMC {
constexpr uint32_t Start=0x80000,RamEnd=0x86FFF,RegsStart=0x8F000,RegsEnd=0x8FFFF;
constexpr uint32_t EventControl=0x8F034,HandlerTable=0x8F004,Handlers=8,StackBase=0x8F074;
constexpr uint32_t DCStateEnable=0x45504,DCStateDebug=0x45520,DebugMasks=3;
// Wa_14011508470 (GEN11_CHICKEN_DCPR_2): CLEAR_MEMSTAT_DIS, SEND_RESP_IMM,
// MASK_LPMODE, MASK_MAXLATENCY_MEMUP_CLR. Linux sets these right after the
// DMC load on every display 12/13 core init.
constexpr uint32_t ChickenDCPR2=0x46434,DCPR2Bits=0xFU<<24;
// DC_STATE_EN[1:0]: 1 = up to DC5, 2 = up to DC6; bit 3 = DC9 (native driver).
constexpr uint32_t DCStateMask=3,DCStateUpToDC6=2;
// DMC_EVT_CTL type EDGE_0_1 (3<<16), event id FALSE (1<<8): never fires.
constexpr uint32_t EventDisabled=(3U<<16)|(1U<<8);
enum class Status:uint32_t {NotRun=0,Complete=1,AlreadyLoaded=2,Firmware=3,Readback=4,Verify=5,Denied=6};
struct Result {
    Status status {Status::NotRun};uint32_t stage {},writes {},mismatchIndex {},expected {},observed {};
    uint32_t dcStateEnable {},debugBefore {},debugAfter {},stackBase {},handler0 {},control0 {};
};
inline const Display13DMCFirmware::Program *mainProgram() {
    for(const auto &p:Display13DMCFirmware::programs)if(p.id==0)return &p;
    return nullptr;
}
// The firmware itself: program in its RAM range, MMIO list inside the main
// register block, non-empty payload. Checked again at run time so a bad
// regenerated header is refused rather than written.
inline bool valid(const Display13DMCFirmware::Program *p) {
    if(!p || p->id || !p->words || !p->payload || !p->mmioCount || !p->mmio || p->start!=Start ||
       p->start+p->words*4-1>RamEnd)return false;
    for(uint32_t i=0;i<p->mmioCount;++i)if(p->mmio[i].reg<RegsStart || p->mmio[i].reg>RegsEnd || (p->mmio[i].reg&3))return false;
    return true;
}
// Loaded and started: program's first and last words plus every MMIO pair.
template<class Read> bool loaded(Read read,const Display13DMCFirmware::Program *p) {
    if(!valid(p) || read(p->start)!=p->payload[0] || read(p->start+4*(p->words-1))!=p->payload[p->words-1])return false;
    for(uint32_t i=0;i<p->mmioCount;++i)if(read(p->mmio[i].reg)!=p->mmio[i].value)return false;
    return true;
}
template<class Read,class Write> bool clearDCStates(Read read,Write write) {
    const auto en=read(DCStateEnable);
    if(en==UINT32_MAX)return false;
    return !(en&DCStateMask) || write(DCStateEnable,en&~DCStateMask);
}
template<class Write> uint32_t disableHandlers(Write write) {
    uint32_t writes=0;
    for(uint32_t h=0;h<Handlers;++h) {
        if(!write(EventControl+4*h,EventDisabled) || !write(HandlerTable+4*h,0))return writes;
        writes+=2;
    }
    return writes;
}
// write(reg,value) returns false when the register is not permitted; the load
// stops there. A program that does not read back exactly is left with every
// handler disabled, so a partial image never runs.
// allowDC6: after a verified load, arm DC_STATE_EN "up to DC6" as Linux does
// once its DC-off well is released. The DMC still refuses DC5/DC6 while any
// pipe scans out without PSR; the caller must clear it (clearDCStates) before
// the pipes and engine go down.
template<class Read,class Write> Result load(Read read,Write write,bool force,bool allowDC6=false,const Display13DMCFirmware::Program *p=mainProgram()) {
    Result r;
    r.stage=1;r.dcStateEnable=read(DCStateEnable);r.debugBefore=read(DCStateDebug);
    if(r.dcStateEnable==UINT32_MAX || r.debugBefore==UINT32_MAX){r.status=Status::Readback;return r;}
    if(!valid(p)){r.status=Status::Firmware;return r;}
    r.stage=2;
    if(!force && loaded(read,p) && (r.debugBefore&DebugMasks)==DebugMasks && (read(ChickenDCPR2)&DCPR2Bits)==DCPR2Bits &&
       (!allowDC6 || (r.dcStateEnable&(DCStateMask|8))==DCStateUpToDC6)) {
        r.status=Status::AlreadyLoaded;r.debugAfter=r.debugBefore;
        r.stackBase=read(StackBase);r.handler0=read(HandlerTable);r.control0=read(EventControl);return r;
    }
    r.stage=3;
    const auto off=disableHandlers(write);r.writes+=off;
    if(off!=2*Handlers){r.status=Status::Denied;return r;}
    r.stage=4;
    for(uint32_t i=0;i<p->words;++i) {
        if(!write(p->start+4*i,p->payload[i])){r.status=Status::Denied;return r;}
        ++r.writes;
    }
    r.stage=5;
    for(uint32_t i=0;i<p->words;++i) {
        const auto v=read(p->start+4*i);
        if(v!=p->payload[i]) {
            r.mismatchIndex=i;r.expected=p->payload[i];r.observed=v;
            r.writes+=disableHandlers(write);
            r.status=v==UINT32_MAX ? Status::Readback : Status::Verify;return r;
        }
    }
    r.stage=6;
    for(uint32_t i=0;i<p->mmioCount;++i) {
        if(!write(p->mmio[i].reg,p->mmio[i].value)){r.status=Status::Denied;return r;}
        ++r.writes;
    }
    r.stage=7;
    if((r.debugBefore&DebugMasks)!=DebugMasks) {
        if(!write(DCStateDebug,r.debugBefore|DebugMasks)){r.status=Status::Denied;return r;}
        ++r.writes;
    }
    const auto dcpr=read(ChickenDCPR2);
    if(dcpr==UINT32_MAX){r.status=Status::Readback;return r;}
    if((dcpr&DCPR2Bits)!=DCPR2Bits) {
        if(!write(ChickenDCPR2,dcpr|DCPR2Bits)){r.status=Status::Denied;return r;}
        ++r.writes;
    }
    r.debugAfter=read(DCStateDebug);r.stackBase=read(StackBase);
    r.handler0=read(HandlerTable);r.control0=read(EventControl);
    r.stage=8;
    r.status=loaded(read,p) && (r.debugAfter&DebugMasks)==DebugMasks && (read(ChickenDCPR2)&DCPR2Bits)==DCPR2Bits ? Status::Complete : Status::Verify;
    if(r.status!=Status::Complete || !allowDC6)return r;
    r.stage=9;
    const auto en=read(DCStateEnable);
    if(en==UINT32_MAX){r.status=Status::Readback;return r;}
    // Never combine with a DC9 request the native driver still holds.
    if(en&8){r.status=Status::Denied;return r;}
    if((en&DCStateMask)!=DCStateUpToDC6) {
        if(!write(DCStateEnable,(en&~DCStateMask)|DCStateUpToDC6)){r.status=Status::Denied;return r;}
        ++r.writes;
    }
    if((read(DCStateEnable)&DCStateMask)!=DCStateUpToDC6)r.status=Status::Verify;
    return r;
}
}
#endif
