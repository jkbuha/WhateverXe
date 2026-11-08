// Hibernation-resume diagnostic: one read-only register snapshot of the
// display pipe A scanout and the RCS command streamer, taken after shared
// display resume. No writes; GT registers only under a caller-held render
// force-wake lease, otherwise left as UINT32_MAX.
#ifndef KERN_RESUME_SNAPSHOT_HPP
#define KERN_RESUME_SNAPSHOT_HPP
#include <stdint.h>
namespace ResumeSnapshot {
// Display engine (power wells, DBUF/MBUS, CDCLK, pipe/transcoder A, DDI A,
// primary plane 1A, cursor A, DE interrupts, panel power).
constexpr uint32_t display[] {
    0x45404,0x45008,0x4438C,0x46000,
    0x70008,0x60400,0x6001C,0x70040,0x64000,0x46140,
    0x70180,0x70188,0x7018C,0x70190,0x7019C,0x701A4,0x701AC,0x7027C,0x70240,
    0x70080,0x70084,0x7017C,
    0x44400,0x44408,0x4440C,0x44468,
    0xC7200,0xC7204
};
// RCS: tail, head, start, ctl, IPEIR, IPEHR, INSTDONE, ACTHD, ACTHD_UDW,
// MI_MODE, CCID, execlist status lo/hi, CSB pointers, execlist control;
// BCS head/tail/IPEHR; RC6 state; render forcewake ack.
constexpr uint32_t gt[] {
    0x2030,0x2034,0x2038,0x203C,0x2064,0x2068,0x206C,0x2074,0x205C,
    0x209C,0x2180,0x2234,0x2238,0x23A0,0x2550,
    0x22030,0x22034,0x22068,
    0xA094,0xD84
};
constexpr unsigned DisplayCount=sizeof(display)/sizeof(display[0]),GTCount=sizeof(gt)/sizeof(gt[0]);
constexpr unsigned Header=6,Words=Header+DisplayCount+GTCount;
// out: [schema, index, uptime_lo, uptime_hi, gtRead, count] then values.
template<class IO> void capture(IO &io,uint32_t (&out)[Words],uint32_t index,uint64_t uptime,bool gtLease) {
    out[0]=1;out[1]=index;out[2]=static_cast<uint32_t>(uptime);out[3]=static_cast<uint32_t>(uptime>>32);
    out[4]=gtLease;out[5]=Words;
    for(unsigned i=0;i<DisplayCount;++i)out[Header+i]=io.read(display[i]);
    for(unsigned i=0;i<GTCount;++i)out[Header+DisplayCount+i]=gtLease?io.read(gt[i]):UINT32_MAX;
}
}
#endif
