// Bounded diagnostic transaction, with no engine submission or TLB consumer.
#ifndef KERN_RAPTORLAKE_P_GTT_SLOT_CORE_HPP
#define KERN_RAPTORLAKE_P_GTT_SLOT_CORE_HPP
#include "kern_raptorlake_p_gtt_shadow_core.hpp"
#include "kern_raptorlake_p_gop.hpp"
namespace RaptorLakeP {namespace GTTSlot {
constexpr size_t Index=0xA00;
struct Result {
    uint64_t version{2},requested{},qualified{},snapshot{},invoked{},nativeResult{},
        before{},expected{},observed{},unexpected{},restoreWritten{},restored{},
        after{},restorePolls{},remaining{},retainPins{},failure{},firmwarePTE{},slotOffset{Index*4096},rangeRestored{};
};
template<class IO> bool qualify(IO &io,XELPMemory::Layout layout) {
    if(!layout.valid || layout.bytes!=0x4000000 || layout.gttBytes!=0x800000 || layout.apertureBytes!=0x10000000)return false;
    const auto gt=capture(io,true);
    if(gt.status!=Status::Complete || gt.releasedMask!=3 || gt.releaseFailedMask || gt.topology.eus!=96 || gt.count!=16)return false;
    // No running ring, pending commands or HWS binding on either qualified engine.
    for(const auto &reg:gt.registers) {
        if(reg.offset==0x2030 || reg.offset==0x2034 || reg.offset==0x2038 || reg.offset==0x203C ||
           reg.offset==0x22030 || reg.offset==0x22034 || reg.offset==0x22038 || reg.offset==0x2203C)
            if(reg.value)return false;
    }
    const auto display=captureDisplay(io,true);
    const auto gop=captureGOP(io,display,layout,true);
    return gop.status==GOPStatus::Complete && gop.width==1920 && gop.height==1200 && !gop.surface &&
        gop.verifiedPages==layout.bytes/4096 && gop.gopEnd<=Index*4096 && gop.candidate==Index*4096 &&
        XELPMemory::reservation(layout,gop.reserved) && layout.range(Index*4096,4096,true);
}
// IO must provide bounded, aligned 64-bit PTE accesses and a 100us delay.
// Snapshot the entire table, invoke the exact one-slot constructor, and restore
// even if it returns failure. Any unaccounted mutation retains prepared pages.
struct Ready {bool operator()()const{return true;}};
template<class IO,class Invoke,class Guard=Ready,class Observe=Ready> void transact(IO &io,uint64_t *saved,uint64_t expected,Invoke invoke,Result &out,Guard guard={},Observe observe={}) {
    if(!saved || !out.qualified || !expected || (expected&4095)!=1 || expected>=0x8000000000ULL){out.failure=1;return;}
    out.expected=expected;
    for(size_t i=0;i<GTTShadow::Entries;++i) {
        const uint32_t offset=GTTShadow::GTTOffset+8*i;
        saved[i]=io.readPTE(offset);
        if(saved[i]!=io.readPTE(offset)){out.failure=2;return;}
    }
    out.before=saved[Index];
    if(!out.firmwarePTE || out.before!=out.firmwarePTE || out.before==expected){out.failure=3;return;}
    if(!guard()){out.failure=5;return;}
    // Recheck immediately before mutation, after the bounded full snapshot.
    if(io.readPTE(GTTShadow::GTTOffset+8*Index)!=out.before){out.failure=2;return;}
    out.snapshot=1;out.invoked=1;out.retainPins=1;
    out.nativeResult=invoke();
    out.observed=io.readPTE(GTTShadow::GTTOffset+8*Index);
    const bool observerOK=out.nativeResult && out.observed==expected && observe();
    for(size_t i=0;i<GTTShadow::Entries;++i)if(i!=Index && io.readPTE(GTTShadow::GTTOffset+8*i)!=saved[i])++out.unexpected;
    io.restore(GTTShadow::GTTOffset+8*Index,out.before);out.restoreWritten=1;
    for(unsigned n=0;n<500;++n) {
        ++out.restorePolls;out.after=io.readPTE(GTTShadow::GTTOffset+8*Index);
        if(out.after==out.before){out.restored=1;break;}
        io.delay();
    }
    for(size_t i=0;i<GTTShadow::Entries;++i)if(io.readPTE(GTTShadow::GTTOffset+8*i)!=saved[i])++out.remaining;
    out.retainPins=!out.restored || out.unexpected || out.remaining;
    if(!observerOK || out.unexpected || !out.restored || out.remaining)out.failure=4;
}
}}
#endif
