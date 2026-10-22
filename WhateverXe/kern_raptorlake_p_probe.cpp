#include "kern_raptorlake_p_probe.hpp"
#include "kern_raptorlake_p.hpp"
#include "kern_raptorlake_p_display.hpp"
#include "kern_raptorlake_p_gop.hpp"
#include "kern_raptorlake_p_bcs.hpp"
#include "kern_raptorlake_p_rcs.hpp"
#include "kern_raptorlake_p_native.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOLib.h>
extern "C" const char version[];
namespace RaptorLakeP {
void probe(IOPCIDevice *pci,const XELP::Profile &profile) {
    static bool attempted=false;
    if(!pci || attempted || !eligible(profile,XELP::kernelOS(version),checkKernelArgument("-wex-rplp-probe")))return;
    attempted=true;
    // Revalidate physical config space before mapping, without enabling PCI
    // memory decoding or bus mastering. EFI must already provide a live BAR.
    const auto identity=pci->configRead32(0);
    const auto revision=pci->configRead8(8);
    const auto command=pci->configRead16(4),ggc=pci->configRead16(0x50);
    const auto low=pci->configRead32(0x10),high=pci->configRead32(0x14);
    const auto bsmLow=pci->configRead32(0xC0),bsmHigh=pci->configRead32(0xC4);
    uint32_t config[]{1,identity,revision,command,ggc,low,high,bsmLow,bsmHigh};
    pci->setProperty("WhateverXeRPLPPCI",config,sizeof(config));
    if(identity!=profile.identity || revision!=profile.revision || !(command&2) || command==UINT16_MAX ||
       low==UINT32_MAX || high==UINT32_MAX || (low&7)!=4)return;
    auto bar=pci->getDeviceMemoryWithRegister(0x10);
    auto aperture=pci->getDeviceMemoryWithRegister(0x18);
    IOByteCount span=0;
    const auto physical=bar ? bar->getPhysicalSegment(0,&span,kIOMemoryMapperNone) : 0;
    const auto base=(uint64_t(high)<<32)|(low&~0xFULL);
    uint32_t first=0,end=0;
    const auto layout=aperture ? XELPMemory::decode(ggc,bsmLow,bsmHigh,aperture->getLength()) : XELPMemory::Layout{};
    const bool valid=bar && XELPMemory::mmioPages(base,physical,bar->getLength(),span,first,end) && layout.valid;
    uint64_t memory[]{1,base,physical,bar ? bar->getLength() : 0,span,layout.base,layout.bytes,
        layout.gttBytes,layout.apertureBytes,valid};
    pci->setProperty("WhateverXeRPLPMemory",memory,sizeof(memory));
    if(!valid)return;
    auto map=pci->mapDeviceMemoryWithRegister(0x10,kIOMapInhibitCache);
    if(!map)return;
    if(map->getLength()!=bar->getLength() || !map->getVirtualAddress() || map->getPhysicalAddress()!=physical){map->release();return;}
    struct IO {
        volatile uint8_t *base;uint64_t length;
        bool readable(uint32_t offset)const{return !(offset&3) && length>=4 && offset<=length-4;}
        uint32_t read(uint32_t offset){return *reinterpret_cast<volatile uint32_t *>(base+offset);}
        void write(uint32_t offset,uint32_t value){*reinterpret_cast<volatile uint32_t *>(base+offset)=value;}
        uint64_t readPTE(uint32_t offset){return *reinterpret_cast<volatile uint64_t *>(base+offset);}
        void delay(){IODelay(100);}
    } io{reinterpret_cast<volatile uint8_t *>(map->getVirtualAddress()),map->getLength()};
    auto result=capture(io,true);
    pci->setProperty("WhateverXeRPLPGT",&result,sizeof(result));
    SYSLOG("wex","RPL-P diagnostic status %u, EU %u, released %u/failed %u",
        static_cast<unsigned>(result.status),result.topology.eus,result.releasedMask,result.releaseFailedMask);
    if(result.status==Status::Complete && result.releasedMask==3 && !result.releaseFailedMask) {
        auto display=captureDisplay(io,true);
        pci->setProperty("WhateverXeRPLPDisplay",&display,sizeof(display));
        auto gop=captureGOP(io,display,layout,true);
        pci->setProperty("WhateverXeRPLPGOP",&gop,sizeof(gop));
        const bool bcs=checkKernelArgument("-wex-rplp-bcs"),rcs=checkKernelArgument("-wex-rplp-rcs");
        if(bcs && !rcs)probeBCS(pci,map,display,gop,layout);
        if(rcs && !bcs)probeRCS(pci,map,display,gop,layout);
        if(!bcs && !rcs)Native::arm(pci,profile,result,gop);
    }
    map->release();
}
}
