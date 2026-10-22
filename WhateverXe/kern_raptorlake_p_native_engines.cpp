// Grouped native ring-field/mode/HWS and dual-engine command qualification.
// The initial register list remains the verified Gen12 template. No shaders,
// native driver start, scheduler/IRQ ownership or display programming is enabled.
#include "kern_raptorlake_p_native_engines.hpp"
#include "kern_raptorlake_p_ring_image_core.hpp"
#include "kern_raptorlake_p_caps_shadow.hpp"
#include "kern_raptorlake_p_cache_shadow.hpp"
#include "kern_raptorlake_p_bcs_core.hpp"
#include "kern_raptorlake_p_rcs_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_raptorlake_p_gop.hpp"
#include "kern_xelp_device.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMapper.h>
#include <IOKit/pci/IOPCIDevice.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace NativeEngines {
enum Field:unsigned {Version,PatchReady,Requested,Prerequisite,Plan,Stage,PageShift,DisplayUnchanged,EngineMask,
    BCSSlot,RCSSlot,BCSAllocated,RCSAllocated,BCSImage,RCSImage,BCSInitQueries,RCSInitQueries,
    BCSSetup,RCSSetup,BCSHWSQueries,RCSHWSQueries,BCSStatus,RCSStatus,BCSToken,RCSToken,
    BCSPassed,RCSPassed,RetainedMask,RenderLeases,RenderReleased,BCSPowerHWS,RCSPowerHWS,Failure,Fields};
static uint64_t record[Fields]{1};
static RingImage::Methods methods;
static IOBufferMemoryDescriptor *pinned[2]{};
static uint64_t pinnedDMA[2][32]{};
static IOPCIDevice *device;
static uint32_t sharedHWS;
static uint32_t preflight[2][14][3];
static void publish(){if(device){device->setProperty("WhateverXeRPLPNativeEngines",record,sizeof(record));device->setProperty("WhateverXeRPLPNativeEnginePreflight",preflight,sizeof(preflight));}}
bool enabled(){return checkKernelArgument("-wex-rplp-native-engines");}
bool prepare(KernelPatcher &patcher,size_t index,mach_vm_address_t address,size_t size) {
    if(!enabled())return true;
    const char *names[]{"__ZN17IGHardwareContext17initRingRegistersEv","__ZN17IGHardwareContext14updateRingTailEj",
        "__ZN16IntelAccelerator17initModeRegistersEv","__ZN16IntelAccelerator31initHardwareStatusPageRegistersEv",
        "__ZNK14IGMappedBuffer20getGPUVirtualAddressEv","__ZNK20IGSharedMappedBuffer17getVirtualAddressEv",
        "__ZN17IGHardwareContext25initRingGPUVirtualAddressEv","__ZN17IGHardwareContext15initRingControlEb",
        "__ZN17IGHardwareContext13resetRingHeadEv"};
    constexpr unsigned offsets[]{0x7C46C,0x7C5B6,0x28E1A,0x28F34,0x13E7C,0x10B60,0x7C4AC,0x7C4FE,0x7C574};
    constexpr uint8_t entry[]{0x55,0x48,0x89,0xE5};
    bool ready=size>0xD1F40 && checkKernelArgument("-wex-rplp-manual-arm") && checkKernelArgument("-wex-rplp-caps-shadow");
    for(unsigned i=0;ready&&i<9;++i)ready=patcher.solveSymbol<mach_vm_address_t>(index,names[i],address,size)==address+offsets[i] &&
        !memcmp(reinterpret_cast<void *>(address+offsets[i]),entry,sizeof(entry));
    if(ready) {
        const auto shift=RingImage::read<uintptr_t>(reinterpret_cast<void *>(address),0xC8230);
        record[PageShift]=shift ? *reinterpret_cast<const uint8_t *>(shift) : 0;
        ready=record[PageShift]==12;
    }
    patcher.clearError();record[PatchReady]=ready;
    if(ready)methods={reinterpret_cast<void (*)(void *)>(address+offsets[0]),reinterpret_cast<void (*)(void *,uint32_t)>(address+offsets[1]),
        reinterpret_cast<void (*)(void *)>(address+offsets[2]),reinterpret_cast<void (*)(void *)>(address+offsets[3])};
    return ready;
}
struct Storage {RingImage::Frame frame;RingImage::Data saved;};
static bool image(unsigned engine,uint8_t *memory,const uint64_t *dma,uint32_t gpu) {
    const bool render=engine==0;const unsigned n=render?RCS::GPUBytes:BCS::GPUBytes;
    if(!(render?RCS::gpuImage(memory,dma,gpu):BCS::gpuImage(memory,dma,gpu)))return false;
    auto storage=static_cast<Storage *>(IOMalloc(sizeof(Storage)));
    auto copy=static_cast<uint8_t *>(IOMalloc(n));
    if(!storage||!copy){if(storage)IOFree(storage,sizeof(Storage));if(copy)IOFree(copy,n);return false;}
    memset(storage,0,sizeof(Storage));memcpy(copy,memory,n);
    const bool ok=RingImage::build(storage->frame,storage->saved,memory,n,gpu+(render?24:3)*4096,
        render?RCS::GPUTail:BCS::GPUTail,engine,methods) && !memcmp(copy,memory,n);
    record[render?RCSImage:BCSImage]=ok;record[render?RCSInitQueries:BCSInitQueries]=storage->frame.queries[0];
    IOFree(copy,n);IOFree(storage,sizeof(Storage));publish();return ok;
}
struct Image {unsigned engine;bool operator()(uint8_t *m,const uint64_t *d,uint32_t b)const{return image(engine,m,d,b);}};
struct IO {
    volatile uint8_t *base;uint64_t length;volatile uint8_t *aperture;
    IOPCIDevice *pci;const DisplayCapture &original;const XELPMemory::Layout &layout;
    bool readable(uint32_t o)const{return !(o&3)&&length>=4&&o<=length-4;}
    uint32_t read(uint32_t o){return *reinterpret_cast<volatile uint32_t *>(base+o);}
    void write(uint32_t o,uint32_t v){*reinterpret_cast<volatile uint32_t *>(base+o)=v;}
    uint64_t readPTE(uint32_t o){return *reinterpret_cast<volatile uint64_t *>(base+o);}
    void writePTE(uint32_t o,uint64_t v){*reinterpret_cast<volatile uint64_t *>(base+o)=v;}
    uint32_t apertureRead(uint32_t o){return *reinterpret_cast<volatile uint32_t *>(aperture+o);}
    void fence(){asm volatile("mfence" ::: "memory");}
    void flushMemory(void *p,uint32_t n){for(uint32_t o=0;o<n;o+=64)asm volatile("clflush (%0)" :: "r"(static_cast<uint8_t *>(p)+o):"memory");fence();}
    void delay(){IODelay(100);}
    bool displayUnchanged(){
        if(!Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),true,true,false,false)||(pci->configRead16(4)&6)!=6)return false;
        const auto memory=XELPMemory::decode(pci->configRead16(0x50),pci->configRead32(0xC0),pci->configRead32(0xC4));
        if(!XELPMemory::samePhysical(memory,layout))return false;
        const auto now=captureDisplay(*this,true);return !memcmp(&now,&original,sizeof(now));
    }
};
struct Setup {
    unsigned engine;
    bool operator()(IO &io,uint32_t gpu)const {
        const bool render=engine==0;const uint32_t hws=render?0x2080:0x22080,mode=render?0x229C:0x2229C;
        const uint32_t powerHWS=gpu+(render?31:15)*4096; // POCS GHWSP: TGL PRM vol9, power-context image; separate owned page
        // Existing probes hold GT; render already holds the render domain too.
        // Blitter's native helpers also touch shared render-domain registers.
        auto before=io.read(0xA278);uint32_t ack=io.read(0xD84);bool ownRender=false;
        const auto wait=[&](bool held){for(unsigned n=0;n<500;++n){ack=io.read(0xD84);if(ack==UINT32_MAX)return false;if(bool(ack&1)==held)return true;io.delay();}return false;};
        if(before==UINT32_MAX||ack==UINT32_MAX)return false;
        if(render){if(!(before&1)||!(ack&1))return false;}
        else {
            if((before|ack)&1)return false;
            io.write(0xA278,0x10001);io.read(0xA278);ownRender=true;++record[RenderLeases];
        }
        bool ok=false;
        do {
            if(!render&&!wait(true))break;
            constexpr uint32_t registers[]{0xA188,0x130044,0xA278,0xD84,0x209C,0x229C,0x2080,
                0x2209C,0x2229C,0x22080,0x14800,0x18080,0xC000,0x190030};
            for(unsigned i=0;i<14;++i){auto row=preflight[render?1:0][i];row[0]=engine;row[1]=registers[i];row[2]=io.read(registers[i]);}
            publish();
            if(!io.displayUnchanged() || io.read(mode) || io.read(0x14800) || io.read(hws) ||
               io.read(0x18080)!=sharedHWS || io.read(0xC000)==UINT32_MAX || (io.read(0xC000)&0xFF00) || io.read(0x190030))break;
            auto storage=static_cast<Storage *>(IOMalloc(sizeof(Storage)));if(!storage)break;
            record[Stage]=render?5:3;publish();
            record[render?RCSPowerHWS:BCSPowerHWS]=powerHWS;
            ok=RingImage::setup(storage->frame,storage->saved,reinterpret_cast<uintptr_t>(io.base),gpu,engine,methods,powerHWS);
            record[render?RCSHWSQueries:BCSHWSQueries]=storage->frame.queries[0]+storage->frame.queries[1];
            // Masked mode writes must leave legacy defaults until the ordinary
            // command probe enables execlists. HWS bindings must use our page.
            ok=ok && !io.read(mode) && !io.read(0x14800) && io.read(hws)==gpu && io.read(0x18080)==powerHWS;
            if(io.read(0x18080)==powerHWS)sharedHWS=powerHWS;
            record[render?RCSSetup:BCSSetup]=ok;IOFree(storage,sizeof(Storage));
        } while(false);
        if(ownRender) {
            io.write(0xA278,0x10000);auto after=io.read(0xA278);
            if(wait(false)&&((after&0xFFFF)==(before&0xFFFF)))++record[RenderReleased];else ok=false;
        }
        publish();return ok;
    }
};
template<bool Render> static bool execute(IOPCIDevice *pci,IOMemoryMap *map,IOMemoryDescriptor *aperture,
        const DisplayCapture &display,const XELPMemory::Layout &layout,uint32_t slot) {
    constexpr unsigned engine=Render?0:2,index=Render?1:0;
    constexpr unsigned pages=Render?RCS::GPUPages:BCS::GPUPages,bytes=Render?RCS::GPUBytes:BCS::GPUBytes;
    IO io{reinterpret_cast<volatile uint8_t *>(map->getVirtualAddress()),map->getLength(),nullptr,pci,display,layout};
    uint64_t dma[pages]{},previous[pages]{};IOBufferMemoryDescriptor *buffer=nullptr;IOMemoryMap *view=nullptr;
    bool passed=false,retained=false;
    do {
        if(!io.displayUnchanged())break;
        bool valid=true;
        for(unsigned p=0;p<pages;++p){previous[p]=io.readPTE(0x800000+(slot>>9)+8*p);if(previous[p]!=layout.pte(uint64_t(slot)+4096*p))valid=false;}
        if(!valid)break;
        buffer=IOBufferMemoryDescriptor::withOptions(kIODirectionInOut|kIOMemoryMapperNone,bytes,4096);
        if(!buffer||buffer->getLength()!=bytes||!buffer->getBytesNoCopy())break;
        record[Render?RCSAllocated:BCSAllocated]=1;
        for(unsigned p=0;p<pages;++p) {
            IOByteCount span=0;dma[p]=buffer->getPhysicalSegment(p*4096,&span,kIOMemoryMapperNone);
            if(!dma[p]||(dma[p]&4095)||dma[p]>0x7FFFFFF000ULL||span<4096||(dma[p]>=layout.base&&dma[p]<layout.base+layout.bytes))valid=false;
            for(unsigned q=0;q<p;++q)if(dma[p]==dma[q])valid=false;
            for(unsigned q=0;q<(Render?BCS::GPUPages:0);++q)if(dma[p]==pinnedDMA[0][q])valid=false;
        }
        if(!valid)break;
        view=aperture->createMappingInTask(kernel_task,0,kIOMapAnywhere|kIOMapInhibitCache,slot,bytes);
        if(!view||view->getLength()!=bytes||!view->getVirtualAddress())break;
        io.aperture=reinterpret_cast<volatile uint8_t *>(view->getVirtualAddress());
        if constexpr(Render) {
            auto result=RCS::testGPU(io,static_cast<uint8_t *>(buffer->getBytesNoCopy()),dma,previous,slot,true,Image{engine},Setup{engine});
            pci->setProperty("WhateverXeRPLPRCS",&result,sizeof(result));retained=result.retained;
            record[RCSStatus]=result.status;record[RCSToken]=result.token;
            // IPEHR/DMA-FADDR are captured instruction/fetch metadata, not error flags.
            passed=result.status==uint64_t(RCS::GPUStatus::Complete)&&result.result==RCS::GPUValue&&result.token==RCS::GPUToken&&
                result.fwReleased&&result.renderReleased&&!result.errors&&!result.ipeir;
        } else {
            auto result=BCS::testGPU(io,static_cast<uint8_t *>(buffer->getBytesNoCopy()),dma,previous,slot,true,Image{engine},Setup{engine});
            pci->setProperty("WhateverXeRPLPBCS",&result,sizeof(result));retained=result.retained;
            record[BCSStatus]=result.status;record[BCSToken]=result.token;
            passed=result.status==uint64_t(BCS::GPUStatus::Complete)&&result.result==BCS::GPUValue&&result.token==BCS::GPUToken&&
                result.fwReleased&&!result.errors&&!result.ipeir;
        }
    } while(false);
    if(retained){pinned[index]=buffer;memcpy(pinnedDMA[index],dma,sizeof(dma));record[RetainedMask]|=1U<<index;}
    else if(buffer)buffer->release();
    if(view)view->release();record[Render?RCSPassed:BCSPassed]=passed;publish();return passed;
}
void run(IOPCIDevice *pci,bool prerequisite) {
    if(!enabled()||record[Requested])return;
    device=pci;record[Requested]=1;record[EngineMask]=5;
    record[Prerequisite]=prerequisite&&record[PatchReady]&&CapsShadow::passed()&&CacheShadow::passed()&&pci&&!IOMapper::gSystem&&page_size==4096&&
        checkKernelArgument("-wex-rplp-manual-arm")&&checkKernelArgument("-wex-rplp-caps-shadow")&&
        Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),checkKernelArgument("-wex-rplp-probe"),
            checkKernelArgument("-wex-rplp-native-probe"),checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs"))&&
        pci->configRead32(0)==0xA7A08086&&(pci->configRead32(8)&255)==4&&(pci->configRead16(4)&6)==6;
    if(!record[Prerequisite]){record[Failure]=1;publish();return;}
    auto aperture=pci->getDeviceMemoryWithRegister(0x18);auto map=pci->mapDeviceMemoryWithRegister(0x10,kIOMapInhibitCache);
    const auto layout=aperture?XELPMemory::decode(pci->configRead16(0x50),pci->configRead32(0xC0),pci->configRead32(0xC4),aperture->getLength()):XELPMemory::Layout{};
    do {
        const uint64_t physical=(uint64_t(pci->configRead32(0x14))<<32)|(pci->configRead32(0x10)&~0xFULL);
        if(!map||map->getLength()!=0x1000000||!map->getVirtualAddress()||(map->getVirtualAddress()&4095)||map->getPhysicalAddress()!=physical||
           !layout.valid||layout.bytes!=0x4000000||layout.gttBytes!=0x800000||layout.apertureBytes!=0x10000000){record[Failure]=2;break;}
        IO io{reinterpret_cast<volatile uint8_t *>(map->getVirtualAddress()),map->getLength(),nullptr,pci,DisplayCapture{},layout};
        const auto gt=capture(io,true);const auto display=captureDisplay(io,true);const auto gop=captureGOP(io,display,layout,true);
        if(gt.status!=Status::Complete||gt.releasedMask!=3||gt.releaseFailedMask||gt.topology.eus!=96||
           gop.status!=GOPStatus::Complete||gop.width!=1920||gop.height!=1200||gop.pitch!=7680||gop.surface||!gop.candidate||gop.verifiedPages!=layout.bytes/4096||gop.gopEnd>gop.candidate){record[Failure]=3;break;}
        auto bounded=layout;const uint64_t bcs=gop.candidate,rcs=bcs+BCS::GPUBytes;
        if(!XELPMemory::reservation(bounded,gop.reserved)||!bounded.range(bcs,BCS::GPUBytes,true)||
           !bounded.range(rcs,RCS::GPUBytes,true)||rcs>0x10000000-RCS::GPUBytes){record[Failure]=4;break;}
        record[BCSSlot]=bcs;record[RCSSlot]=rcs;record[Plan]=1;record[Stage]=1;publish();
        if(!execute<false>(pci,map,aperture,display,layout,uint32_t(bcs))){record[Failure]=5;break;}
        record[Stage]=4;publish();
        if(!execute<true>(pci,map,aperture,display,layout,uint32_t(rcs))){record[Failure]=6;break;}
        const auto after=captureDisplay(io,true);record[DisplayUnchanged]=!memcmp(&after,&display,sizeof(after));
        if(!record[DisplayUnchanged]||record[RenderLeases]!=record[RenderReleased]){record[Failure]=7;break;}
        record[Stage]=6;
    } while(false);
    if(map)map->release();publish();
}
}}
