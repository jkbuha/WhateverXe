// -wex-edp-keep: the firmware eDP link is kept only when its pipe runs exactly
// the active image the donor requests over the same link format, and the
// IdeaPad firmware's measured eDP state is a valid AlderLakeN::dpSource. The
// receiver caps and EDID views make the donor request that format.
#include "kern_alderlake_n.hpp"
#include "kern_display13_edp_keep.hpp"
#include <map>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>
using namespace AlderLakeN;
struct IO {
    std::map<uint32_t,uint32_t> regs;
    bool readable(uint32_t reg) {return reg<0x1000000;}
    uint32_t read(uint32_t reg) {return regs[reg];}
    void write(uint32_t reg,uint32_t v) {regs[reg]=v;}
};
// AppleIntelFramebufferController::SetupTimings for one EDID detailed timing.
struct Mode {uint32_t ha,hb,hso,hsw,va,vb,vso,vsw;};
static void setupTimings(const Mode &t,uint32_t out[6]) {
    const uint32_t htotal=t.ha+t.hb,hstart=t.ha+t.hso,hend=hstart+t.hsw,vtotal=t.va+t.vb;
    out[0]=(((htotal-1)<<16)&0x3FFF0000U)|((t.ha-1)&0x3FFFU);
    out[1]=out[0];
    out[2]=(((hend-1)<<16)&0x3FFF0000U)|((hstart-1)&0x3FFFU);
    out[3]=(((vtotal-1)<<16)&0x1FFF0000U)|((t.va-1)&0x1FFFU);
    out[4]=out[3];
    const uint32_t vstart=(0x1FFFU+t.va+t.vso),vend=vstart+t.vsw;
    out[5]=((vend<<16)&0x1FFF0000U)|(vstart&0x1FFFU);
}
// Samsung ATNA40CT01 EDID DTD: 154.25 MHz, 1920+160 x 1200+36, sync 48/32, 8/8.
static const Mode panel {1920,160,48,32,1200,36,8,8};
static IO firmware() {
    uint32_t t[6];setupTimings(panel,t);
    IO io;
    io.regs={{0x45404,0xFC0F},{0x45454,3},{0x45444,1},{0x42000,0x8E380000},
        {0x60400,0x8A010102},{0x60540,0x80040300},{0x64000,0x80000002},{0x46140,0x10000000},
        {0x60030,0x7E5D159E},{0x60034,0x800000},{0x60040,0x7C1CD},{0x60044,0x80000},
        {0x60000,t[0]},{0x60004,t[1]},{0x60008,t[2]},{0x6000C,t[3]},{0x60010,t[4]},{0x60014,t[5]},
        {0x6001C,0x077F04AF},{0x70008,0xC0000000}};
    return io;
}
// The donor's CRTCParams for the same mode: +0x4 TRANS_DDI_FUNC_CTL (enable,
// DDI A, SST, positive hsync, x2), timings, PIPESRC.
static void params(uint8_t *p,const Mode &t,uint32_t lanes=2,uint32_t bpc=0) {
    memset(p,0,0x100);uint32_t timing[6];setupTimings(t,timing);
    const uint32_t func=0x8A010000U|(bpc<<20)|((lanes-1)<<1),source=((t.ha-1)<<16)|(t.va-1);
    memcpy(p+0x4,&func,4);memcpy(p+0x10,timing,sizeof(timing));memcpy(p+0x28,&source,4);
}
int main() {
    using namespace Display13EDPKeep;
    // The timing words match the register values the EDID DTD gives.
    {uint32_t t[6];setupTimings(panel,t);
     assert(t[0]==0x081F077F && t[2]==0x07CF07AF && t[3]==0x04D304AF && t[5]==0x04BF04B7);}
    auto io=firmware();auto boot=readDisplay(io,true);
    assert(boot.status==ReadoutStatus::Complete && boot.activePipes==1);
    assert(dpSource(boot,0,0,nullptr,true));
    uint32_t values[sizeof(LiveRegisters)/sizeof(LiveRegisters[0])];
    for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);++i)assert(value(boot,LiveRegisters[i],values[i]));
    const auto liveOf=[&](const uint32_t *v){Live l{};l.func=v[0];l.transconf=v[1];for(unsigned i=0;i<6;++i)l.timing[i]=v[2+i];l.source=v[8];return l;};
    const auto l=liveOf(values);
    uint8_t p[0x100];
    // Same mode, link and depth: kept.
    params(p,panel);assert(compare(l,wanted(p),2)==0);
    // Another refresh of the same image only records the blanking difference.
    {Mode t=panel;t.vb=60;params(p,t);const auto r=compare(l,wanted(p),2);assert(r==Blanking && !(r&Blocking));}
    {Mode t=panel;t.hso=40;params(p,t);assert(compare(l,wanted(p),2)==Blanking);}
    // A different active image is never kept.
    {Mode t=panel;t.ha=1680;t.hb=400;params(p,t);assert(compare(l,wanted(p),2)&Timing);}
    {Mode t=panel;t.va=1050;t.vb=186;params(p,t);assert(compare(l,wanted(p),2)&Timing);}
    // A scaled mode of the native timing (1600x1200 source) from an unscaled
    // pipe: the donor's scaler path reprograms the source, the link stays.
    {params(p,panel);uint32_t source=0x063F04AF;memcpy(p+0x28,&source,4);
     const auto r=compare(l,wanted(p),2);assert(r==Source && !(r&Blocking) && compare(l,wanted(p),2,true)==Source);}
    // The firmware's 1024x768 GOP mode scaled to the panel (OpenCore Resolution
    // below Max): the donor's cached scaler parameters do not describe it, so
    // the first decision refuses it.
    {params(p,panel);auto scaled=l;scaled.source=0x03FF02FF;assert(compare(scaled,wanted(p),2)==Timing);}
    // Once a decision on the path was kept, a scaled source is the donor's own:
    // back to native, or to another scaled mode, is again the scaler path.
    {params(p,panel);auto scaled=l;scaled.source=0x063F03E7;assert(compare(scaled,wanted(p),2,true)==Source);
     uint32_t source=0x068F0419;memcpy(p+0x28,&source,4);assert(compare(scaled,wanted(p),2,true)==Source);}
    // A different active size stays a complete mode set, own or not.
    {Mode t=panel;t.ha=1680;t.hb=400;params(p,t);assert(compare(l,wanted(p),2,true)&Timing);}
    // Lane count or depth the donor wants differ from the live link (the rate
    // is never compared: the same-mode branch does not touch the link).
    params(p,panel);
    assert(compare(l,wanted(p),1)==Format);
    params(p,panel,1);assert(compare(l,wanted(p),2)==Format);
    params(p,panel,2,1);assert(compare(l,wanted(p),2)==Format);
    // The pipe must be enabled and running.
    {params(p,panel);auto off=l;off.transconf=0x80000000;assert(compare(off,wanted(p),2)==PipeOff);}
    // A stopped or retrained source is not the firmware link any more.
    for(auto change:{std::make_pair(0x64000U,0x00000082U),std::make_pair(0x60540U,0x80040000U),
                     std::make_pair(0x60400U,0x0A010102U),std::make_pair(0x46140U,0U)}) {
        auto stopped=firmware();stopped.regs[change.first]=change.second;
        auto r=readDisplay(stopped,true);assert(!dpSource(r,0,0,nullptr,true));
    }
    // Every blocking reason blocks; the sink status and blanking never do.
    assert((Blocking&(NotPanel|NotBoot|Readout|NoSource|PipeOff|Timing|Format))==(NotPanel|NotBoot|Readout|NoSource|PipeOff|Timing|Format));
    assert(!(Blocking&(Sink|Blanking)));
    // DPCD lane status: CR, EQ and symbol lock on every active lane, aligned.
    {const uint8_t ok[6]={0x77,0x00,0x01,0,0,0},lane1[6]={0x07,0x00,0x01,0,0,0},open[6]={0x77,0x00,0x00,0,0,0};
     assert(sinkTrained(ok,2) && sinkTrained(lane1,1) && !sinkTrained(lane1,2) && !sinkTrained(open,2));
     const uint8_t four[6]={0x77,0x77,0x81,0,0,0};assert(sinkTrained(four,4) && !sinkTrained(four,0) && !sinkTrained(nullptr,2));}
    // Receiver caps as the IdeaPad's Samsung ATNA40CT01 reports them (DPCD
    // 0x000.., SUPPORTED_LINK_RATES up to 5.4 Gbps, firmware LINK_BW_SET 0x14):
    // capped at HBR in every view the donor reads, other bytes unchanged.
    {uint8_t caps[16]={0x14,0x14,0xc2,0xc1,0,0,1,0xc0,2,0,0,0,0x3f,0x09,0x80,0};const auto before=std::vector<uint8_t>(caps,caps+16);
     capLinkCaps(0,caps,16);assert(caps[1]==0x0A);for(unsigned i=0;i<16;++i)if(i!=1)assert(caps[i]==before[i]);
     uint8_t ext[4]={0x14,0x14,0xc2,0xc1};capLinkCaps(0x2200,ext,4);assert(ext[1]==0x0A && ext[0]==0x14);
     uint8_t rates[16]={0xa4,0x1f,0x30,0x2a,0x76,0x2f,0xbc,0x34,0x48,0x3f,0x60,0x54,0x78,0x69,0,0};
     capLinkCaps(0x10,rates,16);
     const uint8_t capped[16]={0xa4,0x1f,0x30,0x2a,0x76,0x2f,0xbc,0x34,0,0,0,0,0,0,0,0};assert(!memcmp(rates,capped,16));
     uint8_t bw[2]={0x14,0x82};capLinkCaps(0x100,bw,2);assert(bw[0]==0x0A && bw[1]==0x82);
     uint8_t hbr[2]={0x0A,0x82};capLinkCaps(0x100,hbr,2);assert(hbr[0]==0x0A);
     // A read that only partly covers a field caps what it covers.
     uint8_t low[1]={0x60};capLinkCaps(0x1A,low,1);assert(low[0]==0x60);
     uint8_t pair[2]={0x60,0x54};capLinkCaps(0x1A,pair,2);assert(!pair[0] && !pair[1]);
     capLinkCaps(0,nullptr,16);}
    // EDID 1.4 DisplayPort 10 bpc -> 8 bpc with a valid checksum; 8 bpc, 6 bpc,
    // analog or non-EDID data unchanged.
    {uint8_t edid[128]={0,255,255,255,255,255,255,0};edid[18]=1;edid[19]=4;edid[20]=0xB5;
     uint32_t sum=0;for(unsigned i=0;i<127;++i)sum+=edid[i];edid[127]=uint8_t(-sum);
     assert(edid8bpc(edid) && edid[20]==0xA5);sum=0;for(unsigned i=0;i<128;++i)sum+=edid[i];assert(!(sum&255));
     assert(!edid8bpc(edid) && edid[20]==0xA5);
     edid[20]=0x95;assert(!edid8bpc(edid) && edid[20]==0x95);
     edid[20]=0x35;assert(!edid8bpc(edid));
     edid[19]=3;edid[20]=0xB5;assert(!edid8bpc(edid));edid[19]=4;
     edid[0]=1;assert(!edid8bpc(edid));assert(!edid8bpc(nullptr));}
    printf("PASS Display13 eDP keep: firmware link kept only for the same active image and link format\n");
}
