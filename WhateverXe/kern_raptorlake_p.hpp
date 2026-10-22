// Raptor Lake-P inspection only. No driver admission or display programming.
// Register/stepping references: Linux v6.12 intel_uncore.c, intel_step.c,
// gt/intel_gt_regs.h and gt/intel_sseu.c. Capacity is measured, not inferred
// from a product name. L3 sizing and donor compatibility remain unqualified.
#ifndef KERN_RAPTORLAKE_P_HPP
#define KERN_RAPTORLAKE_P_HPP
#include "kern_xelp_platform.hpp"
#include "kern_xelp_os.hpp"
namespace RaptorLakeP {
inline bool eligible(const XELP::Profile &p,XELP::OS os,bool requested) {
    return requested && os==XELP::OS::Tahoe && p.identity==0xA7A08086 &&
        p.revision==4 && !p.overridden() &&
        XELP::same(p,XELP::identify(0xA7A08086,4)) && !p.admitted();
}
struct Resources {uint32_t slices {},dss {},nativeSubslices {},eus {};};
inline bool resources(uint32_t slice,uint32_t dss,uint32_t euDisable,Resources &out) {
    out={};
    if(slice==UINT32_MAX || dss==UINT32_MAX || euDisable==UINT32_MAX ||
       (slice&0xFF)!=1 || !dss || (dss&~0x3FU))return false;
    const auto count=static_cast<uint32_t>(__builtin_popcount(dss));
    const auto pairs=8-static_cast<uint32_t>(__builtin_popcount(euDisable&0xFF));
    if(!pairs)return false;
    out={1,count,2*count,2*count*pairs};return true;
}
enum class Status : uint32_t {Skipped,Unavailable,ExistingOwner,AckTimeout,TopologyUnknown,ReleaseFailed,Complete};
struct Register {uint32_t offset {},value {};};
struct Capture {
    uint32_t version {1};Status status {Status::Skipped};
    uint32_t writes {},requestedMask {},releasedMask {},releaseFailedMask {};
    uint32_t requestBefore[2] {},ackBefore[2] {},ackHeld[2] {},requestAfter[2] {},ackAfter[2] {};
    uint32_t fuses[5] {};Resources topology;
    uint32_t count {};Register registers[16] {};
};
// An isolated boot-time lease of kernel bit0. No request is taken from an
// existing owner; upper masked-write fields never overwrite other clients.
// Every acquired domain is released even on timeout or inaccessible hardware.
// Delay is 100us; four polls each have an explicit 50ms upper bound.
template<class IO,class Observer> Capture capture(IO &io,bool allowed,Observer observe) {
    Capture out;if(!allowed)return out;
    constexpr uint32_t request[]{0xA188,0xA278},ack[]{0x130044,0xD84};
    constexpr uint32_t fuses[]{0x9138,0x913C,0x9134,0x9118,0x9140};
    constexpr uint32_t engines[]{0x2030,0x2034,0x2038,0x203C,0x209C,0x2234,0x2238,0x229C,
        0x22030,0x22034,0x22038,0x2203C,0x2209C,0x22234,0x22238,0x2229C};
    out.status=Status::Unavailable;
    for(unsigned d=0;d<2;++d)if(!io.readable(request[d]) || !io.readable(ack[d]))return out;
    for(auto reg:fuses)if(!io.readable(reg))return out;
    for(auto reg:engines)if(!io.readable(reg))return out;
    for(unsigned d=0;d<2;++d) {
        out.requestBefore[d]=io.read(request[d]);out.ackBefore[d]=io.read(ack[d]);
        if(out.requestBefore[d]==UINT32_MAX || out.ackBefore[d]==UINT32_MAX)return out;
        if((out.requestBefore[d]|out.ackBefore[d])&1){out.status=Status::ExistingOwner;return out;}
    }
    const auto wait=[&](unsigned d,bool held,uint32_t &last) {
        for(unsigned n=0;n<500;++n) {
            last=io.read(ack[d]);if(last==UINT32_MAX)return false;
            if(bool(last&1)==held)return true;
            io.delay();
        }
        return false;
    };
    for(unsigned d=0;d<2;++d) {
        io.write(request[d],0x00010001);++out.writes;out.requestedMask|=1U<<d;
        io.read(request[d]);
    }
    out.status=Status::AckTimeout;bool held=true;
    for(unsigned d=0;d<2;++d)if(!wait(d,true,out.ackHeld[d]))held=false;
    if(out.ackHeld[0]==UINT32_MAX || out.ackHeld[1]==UINT32_MAX)out.status=Status::Unavailable;
    if(held) {
        out.status=Status::Complete;
        for(unsigned i=0;i<5;++i) {
            out.fuses[i]=io.read(fuses[i]);
            if(out.fuses[i]==UINT32_MAX)out.status=Status::Unavailable;
        }
        if(out.status==Status::Complete && !resources(out.fuses[0],out.fuses[1],out.fuses[2],out.topology))
            out.status=Status::TopologyUnknown;
        for(auto reg:engines) {
            const auto value=io.read(reg);out.registers[out.count++]={reg,value};
            if(value==UINT32_MAX)out.status=Status::Unavailable;
        }
    }
    // Optional inspection runs inside the same verified lease. No observer
    // runs on an inaccessible or unrecognized topology. Cleanup stays common.
    if(out.status==Status::Complete && !observe(out))out.status=Status::Unavailable;
    for(int d=1;d>=0;--d) {
        io.write(request[d],0x00010000);++out.writes;
        out.requestAfter[d]=io.read(request[d]);
        if(wait(static_cast<unsigned>(d),false,out.ackAfter[d]) &&
           (out.requestAfter[d]&0xFFFF)==(out.requestBefore[d]&0xFFFF))out.releasedMask|=1U<<d;
        else out.releaseFailedMask|=1U<<d;
    }
    if(out.releaseFailedMask)out.status=Status::ReleaseFailed;
    return out;
}
struct NoCaptureObserver {bool operator()(const Capture &)const{return true;}};
template<class IO> Capture capture(IO &io,bool allowed) {return capture(io,allowed,NoCaptureObserver{});}
}
#endif
