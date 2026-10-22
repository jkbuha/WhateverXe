#include "kern_raptorlake_p_rcs.hpp"
#include "kern_raptorlake_p_rcs_core.hpp"
#include "kern_xelp_device.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMapper.h>
#include <IOKit/pci/IOPCIDevice.h>
extern "C" const char version[];
namespace RaptorLakeP {
void probeRCS(IOPCIDevice *pci,IOMemoryMap *map,const DisplayCapture &display,const GOPCapture &gop,const XELPMemory::Layout &layout) {
    static bool attempted=false;
    static IOBufferMemoryDescriptor *pinned=nullptr;
    if(!pci || !map || attempted || pinned || !eligible(XELP::deviceProfile(pci),XELP::kernelOS(version),
        checkKernelArgument("-wex-rplp-probe") && checkKernelArgument("-wex-rplp-rcs")))return;
    attempted=true;
    RCS::GPUResult result;result.status=uint64_t(RCS::GPUStatus::BadMemory);
    IOBufferMemoryDescriptor *buffer=nullptr;IOMemoryMap *apertureMap=nullptr;
    uint64_t dma[RCS::GPUPages]{},previous[RCS::GPUPages]{};
    do {
        if(IOMapper::gSystem || map->getLength()!=0x1000000 || !map->getVirtualAddress() || gop.status!=GOPStatus::Complete ||
           !gop.candidate || gop.candidate>0x10000000-RCS::GPUBytes ||
           gop.verifiedPages!=layout.bytes/4096 || page_size!=4096 || (pci->configRead16(4)&6)!=6)break;
        auto bounded=layout;
        if(!XELPMemory::reservation(bounded,gop.reserved) || !bounded.range(gop.candidate,RCS::GPUBytes,true) ||
           gop.candidate<gop.gopEnd)break;
        auto aperture=pci->getDeviceMemoryWithRegister(0x18);
        if(!aperture || aperture->getLength()!=0x10000000)break;
        struct IO {
            volatile uint8_t *base;uint64_t length;volatile uint8_t *aperture;
            IOPCIDevice *pci;const DisplayCapture &original;const XELPMemory::Layout &layout;
            bool readable(uint32_t offset)const{return !(offset&3) && length>=4 && offset<=length-4;}
            uint32_t read(uint32_t offset){return *reinterpret_cast<volatile uint32_t *>(base+offset);}
            void write(uint32_t offset,uint32_t value){*reinterpret_cast<volatile uint32_t *>(base+offset)=value;}
            uint64_t readPTE(uint32_t offset){return *reinterpret_cast<volatile uint64_t *>(base+offset);}
            void writePTE(uint32_t offset,uint64_t value){*reinterpret_cast<volatile uint64_t *>(base+offset)=value;}
            uint32_t apertureRead(uint32_t offset){return *reinterpret_cast<volatile uint32_t *>(aperture+offset);}
            void fence(){__asm__ volatile("mfence" ::: "memory");}
            void flushMemory(void *memory,uint32_t bytes){
                for(uint32_t off=0;off<bytes;off+=64)__asm__ volatile("clflush (%0)" :: "r"(static_cast<uint8_t *>(memory)+off) : "memory");
                fence();
            }
            void delay(){IODelay(100);}
            bool displayUnchanged(){
                if(!eligible(XELP::deviceProfile(pci),XELP::OS::Tahoe,true) || (pci->configRead16(4)&6)!=6)return false;
                auto memory=XELPMemory::decode(pci->configRead16(0x50),pci->configRead32(0xC0),pci->configRead32(0xC4));
                if(!XELPMemory::samePhysical(memory,layout))return false;
                const auto now=captureDisplay(*this,true);
                return !memcmp(&now,&original,sizeof(now));
            }
        } io{reinterpret_cast<volatile uint8_t *>(map->getVirtualAddress()),map->getLength(),nullptr,pci,display,layout};
        if(!io.displayUnchanged())break;
        const auto slot=static_cast<uint32_t>(gop.candidate);
        bool same=true;
        for(unsigned p=0;p<RCS::GPUPages;++p) {
            previous[p]=io.readPTE(0x800000+(slot>>9)+8*p);
            if(previous[p]!=layout.pte(uint64_t(slot)+4096*p))same=false;
        }
        if(!same){result.status=uint64_t(RCS::GPUStatus::BusyGGTT);break;}
        buffer=IOBufferMemoryDescriptor::withOptions(kIODirectionInOut|kIOMemoryMapperNone,RCS::GPUBytes,4096);
        if(!buffer || !buffer->getBytesNoCopy() || buffer->getLength()!=RCS::GPUBytes)break;
        bool physical=true;
        for(unsigned p=0;p<RCS::GPUPages;++p) {
            IOByteCount span=0;dma[p]=buffer->getPhysicalSegment(p*4096,&span,kIOMemoryMapperNone);
            if(!dma[p] || (dma[p]&4095) || dma[p]>0x7FFFFFF000ULL || span<4096)physical=false;
            for(unsigned q=0;q<p;++q)if(dma[p]==dma[q])physical=false;
            // The test must not alias any page of firmware-owned stolen RAM.
            if(dma[p]>=layout.base && dma[p]<layout.base+layout.bytes)physical=false;
        }
        if(!physical)break;
        apertureMap=aperture->createMappingInTask(kernel_task,0,kIOMapAnywhere|kIOMapInhibitCache,slot,RCS::GPUBytes);
        if(!apertureMap || !apertureMap->getVirtualAddress() || apertureMap->getLength()!=RCS::GPUBytes)break;
        io.aperture=reinterpret_cast<volatile uint8_t *>(apertureMap->getVirtualAddress());
        result=RCS::testGPU(io,static_cast<uint8_t *>(buffer->getBytesNoCopy()),dma,previous,slot,true);
    } while(false);
    // Once the engine can see these pages they remain wired until reboot,
    // including timeout and successful completion. Never free a live context.
    if(result.retained)pinned=buffer;else OSSafeReleaseNULL(buffer);
    OSSafeReleaseNULL(apertureMap);
    pci->setProperty("WhateverXeRPLPRCS",&result,sizeof(result));
    SYSLOG("wex","RPL-P RCS status %llu stage %llu writes %llu token %llx retained %llu",
        result.status,result.stage,result.engineWrites,result.token,result.retained);
}
}
