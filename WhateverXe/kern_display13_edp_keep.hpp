// -wex-edp-keep: keep the firmware-trained Display13 eDP link.
// The donor answers the first panel mode set with a complete mode set: it
// stops the firmware's link and trains it again, and some panels never
// complete that training on display 13 (the IdeaPad Slim 5 14IAH8's Samsung
// ATNA40CT01 OLED fails channel equalization in every pattern and order).
// When the firmware's pipe already runs exactly the requested active image
// over a live eDP SST link in the same format, hwRegsNeedUpdate can answer
// "no update": the donor's own same-mode branch then only switches the plane
// and scaler to its surface ("Partial modeset"), as Linux fastboot does, and
// never stops the link.
// Pure comparison of the live pipe A registers with the donor's CRTCParams
// (AppleIntelFramebufferController::SetupTimings/SetupParams/enablePipe:
// +0x4 TRANS_DDI_FUNC_CTL, +0x10..+0x24 TRANS_HTOTAL/HBLANK/HSYNC/VTOTAL/
// VBLANK/VSYNC, +0x28 PIPESRC), plus the receiver-caps and EDID views that
// make the donor choose the firmware's usual eDP format (two lanes at HBR,
// 8 bits per component) in the first place.
#ifndef KERN_DISPLAY13_EDP_KEEP_HPP
#define KERN_DISPLAY13_EDP_KEEP_HPP
#include <stdint.h>
namespace Display13EDPKeep {
enum Reason : uint32_t {
 NotPanel=1U<<0,  // not the panel's SST port on pipe A, or foreign objects
 NotBoot=1U<<1,   // not the boot path WhateverXe adopted from the firmware
 Readout=1U<<2,   // pipe registers unreadable or power changed while reading
 NoSource=1U<<3,  // the firmware's eDP SST source is no longer running
 PipeOff=1U<<4,   // TRANSCONF not enabled and active
 Timing=1U<<5,    // requested active size differs from the pipe, or the
                  // firmware scales its own source (a scaled GOP mode)
 Format=1U<<6,    // lane count or color depth differs
 Sink=1U<<7,      // DPCD lane status not trained (recorded only)
 Blanking=1U<<8,  // totals or sync positions differ (recorded only: the pipe
                  // keeps the firmware's refresh of the same active image)
 Source=1U<<9,    // requested source size differs (recorded only: a scaled
                  // mode of the native timing, see compare)
};
constexpr uint32_t Blocking=~uint32_t(Sink|Blanking|Source);
// Registers in Live/record order: TRANS_DDI_FUNC_CTL, TRANSCONF, then the six
// timing words and PIPESRC.
constexpr uint32_t LiveRegisters[]={0x60400,0x70008,0x60000,0x60004,0x60008,0x6000C,0x60010,0x60014,0x6001C};
struct Live {uint32_t func,transconf,timing[6],source;};
struct Wanted {uint32_t func,timing[6],source;};
inline uint32_t field(const void *base,uint32_t offset) {
 uint32_t v;__builtin_memcpy(&v,static_cast<const uint8_t *>(base)+offset,4);return v;
}
inline Wanted wanted(const void *params) {
 Wanted w{};w.func=field(params,0x4);
 for(unsigned i=0;i<6;++i)w.timing[i]=field(params,0x10+4*i);
 w.source=field(params,0x28);return w;
}
// PIPESRC bits 31:16 and 15:0 hold the source width and height minus one; the
// source equals the active image when nothing scales it.
inline bool sourceIsActive(const Live &live) {
 return ((live.source>>16)&0x3FFFU)==(live.timing[0]&0x3FFFU) && (live.source&0x1FFFU)==(live.timing[3]&0x1FFFU);
}
// portLanes: the donor's AppleIntelPort +0x124 after setupClocks. The link
// rate is not compared: the same-mode branch never touches the DPLL, the DDI
// or the transcoder's M/N, so the pipe keeps the firmware's rate whatever the
// donor computed (the IdeaPad's firmware leaves LINK_BW_SET 0x14 next to
// LINK_RATE_SET 3; the donor picks HBR).
// own: the latest decision on this path kept the link, so the live pipe source
// and scaler are the donor's own. A source that differs with the same active
// timing is a scaled mode: hwSetMode's same-mode branch (Tahoe donor, after
// the hwRegsNeedUpdate call) compares the scaler parameters and, when they
// differ, runs AppleIntelScaler::programPipeScaler, which writes the pipe
// scaler, PIPESRC (the path's requested source) and re-arms the plane, with
// the transcoder and the link untouched. Before any kept decision a source that
// is not the active image is the firmware scaling a GOP mode: the donor's
// cached scaler parameters do not describe it, so that stays a complete mode set.
inline uint32_t compare(const Live &live,const Wanted &want,uint32_t portLanes,bool own=false) {
 uint32_t reasons=0;
 if((live.transconf&0xC0000000U)!=0xC0000000U)reasons|=PipeOff;
 // HTOTAL/VTOTAL bits 13:0 (12:0) hold the active size minus one.
 if((live.timing[0]&0x3FFFU)!=(want.timing[0]&0x3FFFU) || (live.timing[3]&0x1FFFU)!=(want.timing[3]&0x1FFFU))reasons|=Timing;
 if(live.source!=want.source)reasons|=own || sourceIsActive(live) ? Source : Timing;
 for(unsigned i=0;i<6;++i)if(live.timing[i]!=want.timing[i])reasons|=Blanking;
 const auto lanes=((live.func>>1)&7)+1;
 if(lanes!=portLanes || ((want.func>>1)&7)+1!=lanes || ((live.func^want.func)&0x00700000U))reasons|=Format;
 return reasons;
}
// DPCD 0x202..0x204: every active lane CR, EQ and symbol lock, aligned.
inline bool sinkTrained(const uint8_t *status,uint32_t lanes) {
 if(!status || !lanes || lanes>4)return false;
 for(uint32_t n=0;n<lanes;++n)if(((status[n/2]>>(4*(n&1)))&7)!=7)return false;
 return status[2]&1;
}
// The panel's receiver caps as the donor's getDPCDInfo and link setup read
// them, capped at HBR: MAX_LINK_RATE in the receiver caps (0x001) and the
// extended caps (0x2201), the eDP 1.4 SUPPORTED_LINK_RATES table (0x010, 200 kHz
// units, ascending, 0 ends it; getDPCDInfo prefers its highest entry over
// MAX_LINK_RATE) and the LINK_BW_SET the donor reads back and reuses (0x100).
// At HBR setupOptimalLaneCount gives a 1920x1200@60 8 bpc mode the firmware's
// two lanes (one at HBR2). Nothing is written to the sink.
inline void capLinkCaps(uint32_t address,uint8_t *data,uint32_t length) {
 if(!data || !length)return;
 const auto at=[&](uint32_t reg)->uint8_t *{return reg>=address && reg-address<length ? data+(reg-address) : nullptr;};
 const uint32_t rates[]={0x001U,0x2201U,0x100U};
 for(const auto reg:rates)if(auto b=at(reg); b && *b>0x0AU)*b=0x0A;
 for(uint32_t i=0;i<8;++i) {
  auto lo=at(0x10+2*i),hi=at(0x11+2*i);
  if(lo && hi && uint32_t(*lo|(*hi<<8))>13500U)*lo=*hi=0;
 }
}
// The panel EDID's DisplayPort color depth (EDID 1.4 video input definition,
// bits 6:4) reported as 8 bits per component when it says more, with the
// checksum fixed, so AGDC policy, mode validation and the mode set agree on the
// firmware's depth. Anything but a digital EDID 1.4 base block is unchanged.
inline bool edid8bpc(uint8_t *edid) {
 static const uint8_t header[]={0,255,255,255,255,255,255,0};
 if(!edid || __builtin_memcmp(edid,header,sizeof(header)) || edid[18]!=1 || edid[19]<4 ||
    !(edid[20]&0x80) || ((edid[20]>>4)&7)<=2)return false;
 edid[20]=uint8_t((edid[20]&0x8F)|0x20);
 uint32_t sum=0;for(unsigned i=0;i<127;++i)sum+=edid[i];
 edid[127]=uint8_t(-sum);
 return true;
}
}
#endif
