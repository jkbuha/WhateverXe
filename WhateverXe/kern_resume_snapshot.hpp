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
    0xC7200,0xC7204,
    // H8: plane colour/compression/key state, pipe colour pipeline (gamma
    // mode, CSC mode/coefficients/offsets, output CSC), pipe misc/bottom
    // colour/chicken, scaler, DP transport, timings, backlight, DE pipe IMR.
    0x701CC,0x701C0,0x701C4,0x701C8,0x701B4,0x701B8,0x70194,0x70198,0x701A0,
    0x4A480,0x49028,0x49010,0x49014,0x49018,0x4901C,0x49020,0x49024,
    0x49030,0x49034,0x49038,0x49040,0x49044,0x49048,
    0x49050,0x49054,0x49058,0x4905C,0x49060,0x49064,0x49068,0x4906C,0x49070,0x49074,0x49078,0x4907C,
    0x70030,0x70034,0x70038,0x70000,
    0x68180,0x68174,0x68170,0x60540,0x60000,0x6000C,0xC8250,0xC8254,0x44404,
    // H10: transcoder A PSR/PSR2 control, status, debug, IMR/IIR, event,
    // perf counter, selective-update/manual-tracking state and ALPM.
    0x60800,0x60810,0x60814,0x60818,0x60840,0x60844,0x60848,0x60860,
    0x60900,0x60910,0x60914,0x60918,0x6091C,0x60940,0x60950
    // H11: display-core state that native startup programs once and Linux
    // re-initialises on every resume: MBUS ABOX credits, BW buddy control and
    // page masks, all four DBUF slices, pipe A DBOX credits and arbitration,
    // DC state, DMC program/HTP/SSP, chicken and clock-gating, and the
    // remaining primary-plane watermark levels.
    ,0x45038,0x45048,0x4504C,0x45130,0x45134,0x45150,0x45154,
    0x44FE8,0x44300,0x44304,0x7003C,0x70028,0x45504,0x80000,0x8F004,0x8F074,
    0x46430,0x42080,0x46530,
    0x70244,0x70248,0x7024C,0x70250,0x70254,0x70258,0x7025C,0x70268
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
// GGTT entries behind the captured scanout/cursor/ring addresses. The GGTT
// starts 8 MiB into the 16 MiB BAR, one 64-bit entry per 4 KiB page. Several
// pages across the primary surface distinguish a restored mapping from one
// left pointing at a common scratch page after a hibernation cold boot.
constexpr uint32_t SurfaceOffsets[] {0,0x1000,0x100000,0x400000,0x800000,0x8C0000};
constexpr unsigned PTECount=sizeof(SurfaceOffsets)/sizeof(SurfaceOffsets[0])+2;
constexpr unsigned PTEWords=4+PTECount*3;
// out: [schema, surface, cursor, ring] then (gpu, pte_lo, pte_hi) rows.
template<class IO> void capturePTEs(IO &io,uint32_t (&out)[PTEWords],uint32_t surface,uint32_t cursor,uint32_t ring) {
    out[0]=1;out[1]=surface;out[2]=cursor;out[3]=ring;
    uint32_t gpu[PTECount];unsigned n=0;
    // An unprogrammed register (0) is not followed.
    for(auto o:SurfaceOffsets)gpu[n++]=surface?(surface&~4095U)+o:0;
    gpu[n++]=cursor&~4095U;gpu[n++]=ring&~4095U;
    for(unsigned i=0;i<PTECount;++i) {
        auto row=out+4+3*i;row[0]=gpu[i];
        const uint64_t entry=0x800000+(uint64_t(gpu[i])>>9);
        if(!gpu[i] || entry+8>0x1000000) {row[1]=row[2]=UINT32_MAX;continue;}
        row[1]=io.read(static_cast<uint32_t>(entry));row[2]=io.read(static_cast<uint32_t>(entry+4));
    }
}
}
#endif
