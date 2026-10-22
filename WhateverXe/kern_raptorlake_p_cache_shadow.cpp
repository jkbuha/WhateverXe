// The native cache initializer executes only against owned RAM. Physical cache
// registers are sampled under the existing bounded GT/render inspection lease.
#include "kern_raptorlake_p_cache_shadow.hpp"
#include "kern_raptorlake_p_cache_transaction.hpp"
#include "kern_raptorlake_p_caps_shadow.hpp"
#include "kern_raptorlake_p_cache_shadow_core.hpp"
#include "kern_raptorlake_p_page_pool_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xelp_device.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IOMapper.h>
#include <IOKit/pci/IOPCIDevice.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace CacheShadow {
using PagePool::read;using PagePool::write;
enum Field : unsigned {Version,PatchReady,Requested,Prerequisite,Allocated,NativeEntered,
    WakeAcquired,WakeReleased,WakeFailure,CPUCacheBytes,Changed,Unexpected,GuardErrors,
    FirstMismatch,PointersRestored,BufferReleased,LeaseStatus,LeaseWrites,LeaseReleased,
    LeaseReleaseFailed,SampleCount,Unstable,HardwareQualified,Failure,Fields};
static uint64_t record[Fields]{1};
static uint32_t samples[Count][3];
static IOPCIDevice *gpu;
static mach_vm_address_t nativeInit,originalWake,expectedVtable;
static const uint8_t *pat;
static const uint32_t *l3,*mocs;
static IOService *activeOwner;
static OSObject *activeManager;
static uintptr_t shadowBase;
static Wake wake;
static void publish() {
    if(!gpu)return;
    gpu->setProperty("WhateverXeRPLPCacheShadow",record,sizeof(record));
    gpu->setProperty("WhateverXeRPLPCacheRegisters",samples,sizeof(samples));
}
bool enabled(){return checkKernelArgument("-wex-rplp-cache-shadow");}
bool passed(){return record[NativeEntered] && record[PointersRestored] && record[BufferReleased] && record[HardwareQualified] && !record[Failure];}
static void forcewake(IOService *owner,uint32_t on,uint32_t domain) {
    if(CapsShadow::consumeWake(owner,on,domain))return;
    if(!activeManager) {
        reinterpret_cast<void (*)(IOService *,uint32_t,uint32_t)>(originalWake)(owner,on,domain);return;
    }
    // Suppress every hardware wake call during RAM replay, including malformed
    // calls. Record a refusal rather than touching legacy forcewake registers.
    wake.accept(owner==activeOwner,read<uintptr_t>(activeManager,0x18)==shadowBase &&
        read<uintptr_t>(activeOwner,0x1240)==shadowBase,on,domain);
}
static bool tableReference(mach_vm_address_t base,unsigned site,unsigned target) {
    const auto p=reinterpret_cast<const uint8_t *>(base+site);
    int32_t disp;memcpy(&disp,p+3,4);
    return p[0]==0x48 && p[1]==0x8D && p[2]==0x15 && int64_t(site)+7+disp==int64_t(target);
}
bool prepare(KernelPatcher &patcher,size_t index,mach_vm_address_t address,size_t size) {
    if(!enabled())return true;
    const char *names[]{"__ZN15IGMemoryManager9initCacheEv","__ZN16IntelAccelerator13SafeForceWakeEbj",
        "__ZN21IntelTGLMemoryManager14initGTL3LayoutEv","__ZN21IntelTGLMemoryManager20initMOCSTableEntriesEv",
        "__ZN21IntelTGLMemoryManager24updatePageAttributeTableEv"};
    const unsigned offsets[]{0xFE1A,0x28C2A,0x4BA56,0x4BA7C,0x4BE72};
    constexpr uint8_t initEntry[]{0x55,0x48,0x89,0xE5,0x41,0x56,0x53,0x49};
    constexpr uint8_t wakeEntry[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x50,0x85,0xD2};
    bool ready=size>0xD6360 && checkKernelArgument("-wex-rplp-native-factory");
    for(unsigned i=0;ready && i<5;++i)ready=patcher.solveSymbol<mach_vm_address_t>(index,names[i],address,size)==address+offsets[i];
    if(ready)ready=!memcmp(reinterpret_cast<void *>(address+0xFE1A),initEntry,sizeof(initEntry)) &&
        !memcmp(reinterpret_cast<void *>(address+0x28C2A),wakeEntry,sizeof(wakeEntry)) &&
        read<uintptr_t>(reinterpret_cast<void *>(address),0xD6340)==address+0x4BA7C &&
        read<uintptr_t>(reinterpret_cast<void *>(address),0xD6348)==address+0x4BA56 &&
        read<uintptr_t>(reinterpret_cast<void *>(address),0xD6358)==address+0x4BE72 &&
        tableReference(address,0xFF18,0xA00D0) && tableReference(address,0x4BA8D,0xBECD8) &&
        tableReference(address,0x4BE88,0xBECD0);
    if(ready) {
        KernelPatcher::RouteRequest route{names[1],forcewake,originalWake};
        ready=patcher.routeMultiple(index,&route,1,address,size);
    }
    patcher.clearError();record[PatchReady]=ready;
    if(ready) {
        nativeInit=address+0xFE1A;expectedVtable=address+0xD6208;
        pat=reinterpret_cast<const uint8_t *>(address+0xBECD0);
        l3=reinterpret_cast<const uint32_t *>(address+0xA00D0);
        mocs=reinterpret_cast<const uint32_t *>(address+0xBECD8);
    }
    return ready && (!CapsShadow::enabled() || CapsShadow::prepare(patcher,index,address,size));
}
struct MMIO {
    volatile uint32_t *base;
    bool readable(uint32_t o){return !(o&3) && o<Bytes;}
    uint32_t read(uint32_t o){return base[o/4];}
    void write(uint32_t o,uint32_t v){base[o/4]=v;}
    void delay(){IODelay(100);}
};
bool run(OSObject *manager,IOService *owner,IOPCIDevice *pci) {
    if(!enabled())return true;
    if(record[Requested])return false;
    gpu=pci;record[Requested]=1;record[FirstMismatch]=UINT64_MAX;
    record[Prerequisite]=record[PatchReady] && manager && owner && pci && !IOMapper::gSystem &&
        Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
            checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
            checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs")) &&
        pci->configRead32(0)==0xA7A08086 && (pci->configRead32(8)&255)==4 && (pci->configRead16(4)&2) &&
        manager->getMetaClass()->getClassSize()==0x118 && read<uintptr_t>(manager,0)==expectedVtable &&
        read<void *>(manager,0x10)==owner && read<void *>(manager,0x110);
    if(!record[Prerequisite]){record[Failure]=1;publish();return false;}
    const auto map=read<IOMemoryMap *>(manager,0x68);
    const auto physical=(uint64_t(pci->configRead32(0x14))<<32)|(pci->configRead32(0x10)&~0xFULL);
    if(!map || map->getLength()!=Bytes || map->getPhysicalAddress()!=physical ||
        !map->getVirtualAddress() || (map->getVirtualAddress()&4095) ||
        read<uintptr_t>(manager,0x18)!=map->getVirtualAddress()) {record[Failure]=2;publish();return false;}
    // Hardware data is independent of the RAM replay. A refused/unstable sample
    // remains visible without preventing safe CPU construction and cleanup.
    MMIO io{reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress())};
    const auto lease=capture(io,true,[&](const Capture &gt) {
        if(gt.topology.eus!=96 || gt.count!=16)return false;
        for(const auto &r:gt.registers)if((r.offset==0x2030 || r.offset==0x2034 || r.offset==0x2038 || r.offset==0x203C ||
            r.offset==0x22030 || r.offset==0x22034 || r.offset==0x22038 || r.offset==0x2203C) && r.value)return false;
        for(size_t i=0;i<Count;++i) {samples[i][0]=offset(i);samples[i][1]=io.read(offset(i));}
        for(size_t i=0;i<Count;++i) {samples[i][2]=io.read(offset(i));record[Unstable]+=samples[i][1]!=samples[i][2];}
        record[SampleCount]=Count;return true;
    });
    record[LeaseStatus]=static_cast<uint32_t>(lease.status);record[LeaseWrites]=lease.writes;
    record[LeaseReleased]=lease.releasedMask;record[LeaseReleaseFailed]=lease.releaseFailedMask;
    record[HardwareQualified]=lease.status==Status::Complete && lease.releasedMask==3 && !record[Unstable] && record[SampleCount]==Count;
    // Do not proceed after a failed hardware lease release.
    if(lease.releaseFailedMask){record[Failure]=3;publish();return false;}
    auto buffer=IOBufferMemoryDescriptor::withOptions(kIODirectionInOut|kIOMemoryMapperNone,Bytes+2*Guard,4096);
    auto mapping=buffer ? buffer->map() : nullptr;
    auto allocation=mapping && mapping->getLength()==Bytes+2*Guard ? reinterpret_cast<uint32_t *>(mapping->getVirtualAddress()) : nullptr;
    if(!allocation) {
        if(mapping)mapping->release();if(buffer)buffer->release();record[Failure]=4;publish();return false;
    }
    record[Allocated]=1;memset(allocation,0xA5,Bytes+2*Guard);
    shadowBase=reinterpret_cast<uintptr_t>(allocation)+Guard;
    const auto oldMMIO=read<uintptr_t>(manager,0x18),oldOwner=read<uintptr_t>(owner,0x1240);const auto oldCache=read<uint64_t>(manager,0x108);
    reinterpret_cast<uint32_t *>(shadowBase)[0x9008/4]=UINT32_MAX;
    reinterpret_cast<uint32_t *>(shadowBase)[0x900C/4]=UINT32_MAX;
    activeOwner=owner;activeManager=manager;wake={};
    write(manager,0x18,shadowBase);write(owner,0x1240,shadowBase);
    record[NativeEntered]=1;reinterpret_cast<void (*)(OSObject *)>(nativeInit)(manager);
    record[CPUCacheBytes]=read<uint64_t>(manager,0x108);
    // Restore every borrowed field before validation, release or publication.
    write(manager,0x18,oldMMIO);write(owner,0x1240,oldOwner);write(manager,0x108,oldCache);
    activeManager=nullptr;activeOwner=nullptr;shadowBase=0;
    record[PointersRestored]=read<uintptr_t>(manager,0x18)==oldMMIO && read<uintptr_t>(owner,0x1240)==oldOwner && read<uint64_t>(manager,0x108)==oldCache;
    record[WakeAcquired]=wake.acquired;record[WakeReleased]=wake.released;record[WakeFailure]=wake.failure;
    const auto check=verify(allocation,pat,l3,mocs);
    record[Changed]=check.changed;record[Unexpected]=check.unexpected;record[GuardErrors]=check.guards;record[FirstMismatch]=check.firstMismatch;
    const bool replayOK=record[PointersRestored] && record[CPUCacheBytes] && wake.acquired==1 && wake.released==1 &&
        !wake.failure && check.changed==Count && !check.unexpected && !check.guards && record[HardwareQualified];
    if(CacheTransaction::enabled() && !CacheTransaction::run(pci,map,pat,l3,mocs,samples,replayOK))record[Failure]=6;
    if(CapsShadow::enabled() && !CapsShadow::run(pci,map,allocation,replayOK && !record[Failure]))record[Failure]=7;
    mapping->release();buffer->release();record[BufferReleased]=1;
    if(!record[PointersRestored] || !record[CPUCacheBytes] || wake.acquired!=1 || wake.released!=1 || wake.failure ||
        check.changed!=Count || check.unexpected || check.guards)record[Failure]=5;
    publish();return !record[Failure];
}
}}
