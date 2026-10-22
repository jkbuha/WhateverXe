// Native mapping and optional dummy pages; always stop before GGTT writes.
#include "kern_raptorlake_p_native_memory.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_raptorlake_p_gtt_shadow.hpp"
#include "kern_raptorlake_p_native_pools.hpp"
#include "kern_raptorlake_p_native_chain.hpp"
#include "kern_raptorlake_p_page_pool.hpp"
#include "kern_raptorlake_p_page_array.hpp"
#include "kern_raptorlake_p_native_factory.hpp"
#include "kern_xelp_device.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMapper.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace NativeMemory {
enum Field : unsigned {Version,PatchReady,Attempted,FactoryReturned,DummyBlocked,GTTBlocked,
    MapValid,ReservedStart,ReservedBytes,AllocStart,AllocBytes,VirtualStart,VirtualBytes,
    OtherStart,OtherBytes,ReleaseCount,Released,FactoryUnexpected,Failure,
    PagesRequested,DummyReturned,DummyResult,AllocationCalls,AllocationFailures,FailureOutputsCleared,
    DummyPhysical,TablePhysical,PageLength,TableLength,DummyReleaseCount,DummyReleased,
    ValidationMask,DataSpan,TableSpan,NativeDataPhysical,NativeTablePhysical,NativeOptions,
    DataFirst,TableFirst,DataMismatch,DataMismatchValue,TableMismatch,TableMismatchValue,Fields};
static uint64_t record[Fields]{3};
static IOService *owner;
static IOPCIDevice *gpu;
static mach_vm_address_t factory,originalDummy,originalGTT,originalRelease;
static mach_vm_address_t originalAllocation,originalDummyRelease;
static OSObject *pageManager;
template<class T> static T field(void *object,unsigned offset){T value;memcpy(&value,static_cast<uint8_t *>(object)+offset,sizeof(value));return value;}
static void publish(){if(gpu)gpu->setProperty("WhateverXeRPLPNativeMemory",record,sizeof(record));}
bool enabled(){return checkKernelArgument("-wex-rplp-native-memory");}
static bool pages(){return checkKernelArgument("-wex-rplp-native-pages");}
// The donor releases its buffer on every allocation failure but its prepare
// failure branch leaves stale outputs. Clear them before native unwind.
static bool allocation(void **output,uint64_t *address,uint32_t options,uint32_t kind) {
    auto b=reinterpret_cast<uint8_t *>(pageManager);
    if(!owner || !b || !pages() || !output || !address || (kind!=0 && kind!=4096) ||
       output!=reinterpret_cast<void **>(b+(kind ? 0xE8 : 0xE0)) ||
       address!=reinterpret_cast<uint64_t *>(b+(kind ? 0xF8 : 0xF0)) ||
       options!=field<uint32_t>(pageManager,0x88))return false;
    ++record[AllocationCalls];
    const bool ok=reinterpret_cast<bool (*)(void **,uint64_t *,uint32_t,uint32_t)>(originalAllocation)(output,address,options,kind);
    if(!ok){++record[AllocationFailures];*output=nullptr;*address=0;++record[FailureOutputsCleared];}
    return ok;
}
static void dummyRelease(OSObject *manager) {
    reinterpret_cast<void (*)(OSObject *)>(originalDummyRelease)(manager);
    if(owner && manager && field<void *>(manager,0x10)==owner) {
        ++record[DummyReleaseCount];record[DummyReleased]=!field<void *>(manager,0xE0) &&
            !field<void *>(manager,0xE8) && !field<uint64_t>(manager,0xF0) && !field<uint64_t>(manager,0xF8);
        NativeChain::observeCleanup(manager);
        publish();
    }
}
// Mapping failures always stop. Optional allocations are CPU memory only;
// the independently routed GGTT gate never calls the original initializer.
static bool dummy(OSObject *manager) {
    ++record[DummyBlocked];
    if(!owner || !manager || manager->getMetaClass()->getClassSize()<0x118 || field<void *>(manager,0x10)!=owner) {
        record[Failure]=2;publish();return false;
    }
    auto map=field<IOMemoryMap *>(manager,0x68);
    const uint64_t base=(uint64_t(gpu->configRead32(0x14))<<32)|(gpu->configRead32(0x10)&~0xFULL);
    auto aperture=gpu->getDeviceMemoryWithRegister(0x18);
    const auto layout=aperture ? XELPMemory::decode(gpu->configRead16(0x50),gpu->configRead32(0xC0),gpu->configRead32(0xC4),aperture->getLength()) : XELPMemory::Layout{};
    record[MapValid]=map && map->getLength()==0x1000000 && map->getPhysicalAddress()==base &&
        map->getVirtualAddress() && field<uintptr_t>(manager,0x18)==map->getVirtualAddress();
    for(unsigned i=0;i<8;++i)record[ReservedStart+i]=field<uint64_t>(manager,0xA0+8*i);
    if(!record[MapValid] || !layout.valid || record[ReservedStart] || record[ReservedBytes]!=layout.bytes ||
       record[AllocStart]!=layout.bytes || record[AllocBytes]!=layout.apertureBytes-layout.bytes)record[Failure]=3;
    if(pages() && !record[Failure]) {
        pageManager=manager;
        const bool ok=reinterpret_cast<bool (*)(OSObject *)>(originalDummy)(manager);
        pageManager=nullptr;record[DummyReturned]=1;record[DummyResult]=ok;
        if(ok) {
            auto data=OSDynamicCast(IOBufferMemoryDescriptor,field<OSObject *>(manager,0xE0));
            auto table=OSDynamicCast(IOBufferMemoryDescriptor,field<OSObject *>(manager,0xE8));
            IOByteCount dataSpan=0,tableSpan=0;
            const uint64_t dp=data ? data->getPhysicalSegment(0,&dataSpan,kIOMemoryMapperNone) : 0;
            const uint64_t tp=table ? table->getPhysicalSegment(0,&tableSpan,kIOMemoryMapperNone) : 0;
            record[DummyPhysical]=dp;record[TablePhysical]=tp;
            record[PageLength]=data ? data->getLength() : 0;record[TableLength]=table ? table->getLength() : 0;
            record[DataSpan]=dataSpan;record[TableSpan]=tableSpan;
            record[NativeDataPhysical]=field<uint64_t>(manager,0xF0);record[NativeTablePhysical]=field<uint64_t>(manager,0xF8);
            record[NativeOptions]=field<uint32_t>(manager,0x88);
            // The donor allocates with no task and maps each buffer explicitly.
            // Such descriptors need not expose a getBytesNoCopy() pointer.
            auto dataMap=data && record[PageLength]==4096 ? data->map(kIOMapReadOnly) : nullptr;
            auto tableMap=table && record[TableLength]==4096 ? table->map(kIOMapReadOnly) : nullptr;
            auto d=dataMap && dataMap->getLength()==4096 ? reinterpret_cast<const uint32_t *>(dataMap->getVirtualAddress()) : nullptr;
            auto t=tableMap && tableMap->getLength()==4096 ? reinterpret_cast<const uint64_t *>(tableMap->getVirtualAddress()) : nullptr;
            uint64_t mask=0;
            if(data && table)mask|=1;
            if(dp && tp && dp!=tp && !((dp|tp)&4095) && dp<0x8000000000ULL && tp<0x8000000000ULL)mask|=2;
            if(dataSpan>=4096 && tableSpan>=4096)mask|=4;
            if(record[PageLength]==4096 && record[TableLength]==4096)mask|=8;
            if(record[NativeDataPhysical]==dp && record[NativeTablePhysical]==tp)mask|=16;
            if(d && t)mask|=32;
            record[DataMismatch]=UINT64_MAX;record[TableMismatch]=UINT64_MAX;
            if(d && record[PageLength]==4096) {
                record[DataFirst]=d[0];bool valid=true;
                for(unsigned i=0;i<1024;++i)if(d[i]!=0xC01DBEAD && valid) {
                    valid=false;record[DataMismatch]=i;record[DataMismatchValue]=d[i];
                }
                if(valid)mask|=64;
            }
            if(t && record[TableLength]==4096) {
                record[TableFirst]=t[0];bool valid=true;
                for(unsigned i=0;i<512;++i)if(t[i]!=(tp|1) && valid) {
                    valid=false;record[TableMismatch]=i;record[TableMismatchValue]=t[i];
                }
                if(valid)mask|=128;
            }
            record[ValidationMask]=mask;
            if(mask!=255)record[Failure]=10;
            if(tableMap)tableMap->release();
            if(dataMap)dataMap->release();
        }
        publish();return ok && !record[Failure];
    }
    publish();return false;
}
static bool gtt(OSObject *manager) {
    ++record[GTTBlocked];
    if(!pages() || !owner || !manager || field<void *>(manager,0x10)!=owner || !record[DummyResult])record[Failure]=4;
    if(!record[Failure] && GTTShadow::enabled() && !GTTShadow::run(manager,owner,gpu,originalGTT))record[Failure]=12;
    const bool continued=NativeChain::enabled() && NativeChain::allowGTT(!record[Failure] && GTTShadow::slotPassed());
    publish();return continued;
}
static void release(OSObject *manager) {
    reinterpret_cast<void (*)(OSObject *)>(originalRelease)(manager);
    if(owner && manager && field<void *>(manager,0x10)==owner) {
        ++record[ReleaseCount];record[Released]=!field<uintptr_t>(manager,0x18) &&
            !field<void *>(manager,0x68) && !field<void *>(manager,0x70);
        NativeChain::observeCleanup(manager);publish();
    }
}
bool prepare(KernelPatcher &patcher,size_t index,mach_vm_address_t address,size_t size) {
    if(!enabled())return true;
    // The caller has already verified the signed donor UUID/probe/start. Keep
    // all additional method entries and both factory vtable slots exact too.
    const char *names[]{"__ZN16IntelAccelerator19createMemoryManagerEv",
        "__ZN16IntelAccelerator16newMemoryManagerEv",
        "__ZN15IGMemoryManager14initDummyPagesEv",
        "__ZN15IGMemoryManager13initGlobalGTTEv",
        "__ZN15IGMemoryManager19releaseDeviceMemoryEv"};
    const unsigned offsets[]{0x2D24A,0x28A00,0xEC76,0xECDA,0xF05C};
    bool ready=size>0xD1B70;
    constexpr uint8_t entry[]{0x55,0x48,0x89,0xE5};
    for(unsigned i=0;ready && i<5;++i)ready=patcher.solveSymbol<mach_vm_address_t>(index,names[i],address,size)==address+offsets[i] &&
        !memcmp(reinterpret_cast<void *>(address+offsets[i]),entry,sizeof(entry));
    ready=ready && field<uintptr_t>(reinterpret_cast<void *>(address),0xD1B60)==address+0x28A00 &&
        field<uintptr_t>(reinterpret_cast<void *>(address),0xD1B68)==address+0x2D24A;
    patcher.clearError();
    if(ready && GTTShadow::enabled())ready=GTTShadow::prepare(patcher,index,address,size);
    if(ready && NativePools::enabled())ready=NativePools::prepare(patcher,index,address,size);
    if(ready && NativeChain::enabled())ready=NativeChain::prepare(patcher,index,address,size);
    if(ready && PagePool::enabled())ready=PagePool::prepare(patcher,index,address,size);
    if(ready && PageArray::enabled())ready=PageArray::prepare(patcher,index,address,size);
    if(ready && NativeFactory::enabled())ready=NativeFactory::prepare(patcher,index,address,size);
    if(ready) {
        KernelPatcher::RouteRequest gates[]{
            {names[2],dummy,originalDummy},{names[3],gtt,originalGTT},{names[4],release,originalRelease}};
        ready=patcher.routeMultiple(index,gates,3,address,size);
        if(ready && pages()) {
            const char *alloc="__ZZN15IGMemoryManager14initDummyPagesEvENK3$_5clEPP24IOBufferMemoryDescriptorPyjj";
            const char *cleanup="__ZN15IGMemoryManager17releaseDummyPagesEv";
            constexpr uint8_t allocEntry[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x50};
            constexpr uint8_t cleanupEntry[]{0x55,0x48,0x89,0xE5,0x41,0x56,0x53};
            ready=patcher.solveSymbol<mach_vm_address_t>(index,alloc,address,size)==address+0xF22E &&
                patcher.solveSymbol<mach_vm_address_t>(index,cleanup,address,size)==address+0xEF1E &&
                !memcmp(reinterpret_cast<void *>(address+0xF22E),allocEntry,sizeof(allocEntry)) &&
                !memcmp(reinterpret_cast<void *>(address+0xEF1E),cleanupEntry,sizeof(cleanupEntry));
            if(ready) {
                KernelPatcher::RouteRequest safety[]{{alloc,allocation,originalAllocation},{cleanup,dummyRelease,originalDummyRelease}};
                ready=patcher.routeMultiple(index,safety,2,address,size);
            }
        }
    }
    patcher.clearError();record[PatchReady]=ready;
    if(ready)factory=address+0x2D24A;
    return ready;
}
bool passed(){return record[Attempted] && record[FactoryReturned] && record[Released] && record[DummyReleased] && !record[Failure];}
void run(IOService *object,IOPCIDevice *pci) {
    if(!enabled() || !record[PatchReady] || record[Attempted] || !object || !pci ||
       object->getMetaClass()->getClassSize()<0x1C58 ||
       !Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
        checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
        checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs")))return;
    record[Attempted]=1;owner=object;gpu=pci;publish();
    record[PagesRequested]=pages();
    if(pages() && IOMapper::gSystem){record[Failure]=8;publish();owner=nullptr;return;}
    auto bytes=reinterpret_cast<uint8_t *>(object);
    // No superclass start is run. Supply only the provider and the same shared
    // memory input as the donor's standalone path; restore them after unwind.
    // Stolen-range contents are unused before our dummy-page gate.
    uint64_t chainInfo[5]{};
    if(NativeChain::enabled() && !NativeChain::begin(object,pci,chainInfo)){record[Failure]=13;publish();owner=nullptr;return;}
    uint8_t providerBefore[8],memoryBefore[8],stolenBefore[40];
    memcpy(providerBefore,bytes+0x368,8);memcpy(memoryBefore,bytes+0x13C0,8);memcpy(stolenBefore,bytes+0x13D0,40);
    memcpy(bytes+0x368,&pci,8);const uint32_t shared[]{0,0x18000000};memcpy(bytes+0x13C0,shared,8);memcpy(bytes+0x13D0,chainInfo,40);
    auto result=reinterpret_cast<OSObject *(*)(IOService *)>(factory)(object);
    record[FactoryReturned]=1;
    if(result && !(NativeFactory::enabled() && NativeFactory::consume(result))){record[FactoryUnexpected]=1;record[Failure]=5;result->release();}
    if(GTTShadow::enabled())GTTShadow::finish();
    if((!record[DummyBlocked] || !record[Released]) && !record[Failure])record[Failure]=6;
    if(pages() && !record[DummyReleased] && !record[Failure])record[Failure]=11;
    if(NativeFactory::enabled() && !NativeFactory::finish(record[FactoryReturned]) && !record[Failure])record[Failure]=16;
    if(PageArray::integrated() && !PageArray::finishIntegrated(record[FactoryReturned] && record[Released] && record[DummyReleased]) && !record[Failure])record[Failure]=15;
    if(NativeChain::enabled() && !NativeChain::finish(record[FactoryReturned] && record[Released] && record[DummyReleased]) && !record[Failure])record[Failure]=14;
    if(NativePools::enabled())NativePools::run(object,pci,!record[Failure] && GTTShadow::aperturePassed());
    if(PagePool::enabled())PagePool::run(object,pci,!record[Failure] && NativeChain::enabled() && NativePools::passed());
    if(PageArray::enabled())PageArray::run(object,pci,!record[Failure] && PagePool::passed(),NativeChain::pagePoolInitializer());
    memcpy(bytes+0x368,providerBefore,8);memcpy(bytes+0x13C0,memoryBefore,8);memcpy(bytes+0x13D0,stolenBefore,40);
    publish();owner=nullptr;
}
}}
