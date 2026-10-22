// Native descriptor allocation, exhaustion, aliasing, reuse and pool registration.
// Only the real manager's already verified CPU-owned backing page is used.
#include "kern_raptorlake_p_subranges.hpp"
#include "kern_raptorlake_p_page_pool_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_raptorlake_p_page_array.hpp"
#include "kern_raptorlake_p_cache_shadow.hpp"
#include "kern_xelp_device.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMapper.h>
#include <IOKit/pci/IOPCIDevice.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace Subranges {
using PagePool::read;using PagePool::write;
enum Field:unsigned {Version,PatchReady,Requested,Prerequisite,PoolValid,Physical,
    AllocateCalls,Created,DescriptorValid,MapPairs,AliasWords,RestoredWords,
    ExhaustionCalls,ExhaustionRefused,ChildReleased,EmptyRounds,CPUCount,
    PagePoolsChecked,EventsEntered,EventsResult,PagesUnchanged,Retained,Failure,PartialFreed,FreeSeen,Fields};
static uint64_t record[Fields]{1};
static mach_vm_address_t allocate,registerEvents,poolVtable,descriptorVtable;
static const uint32_t *cpus;
static mach_vm_address_t originalFree,superFree;
static OSObject *activePool,*expectedFree;
static bool constructing,uncertain;
static OSObject *child,*extra;
static IOPCIDevice *gpu;
static void publish(){if(gpu)gpu->setProperty("WhateverXeRPLPSubranges",record,sizeof(record));}
bool enabled(){return checkKernelArgument("-wex-rplp-subranges");}
bool held(){return child || extra || uncertain;}
static void descriptorFree(OSObject *object) {
    if(!activePool){reinterpret_cast<void (*)(OSObject *)>(originalFree)(object);return;}
    if(constructing && object && object->getMetaClass()->getClassSize()==0x90 &&
        read<uintptr_t>(object,0)==descriptorVtable && !read<void *>(object,0x70)) {
        // Native withSubRange releases a partial object before it assigns pool.
        // Its free unconditionally dereferences pool. Superclass cleanup owns
        // only the partially initialized subdescriptor; range rollback remains
        // in native allocate, which has not yet linked or counted this object.
        ++record[PartialFreed];reinterpret_cast<void (*)(OSObject *)>(superFree)(object);return;
    }
    if(!constructing && object==expectedFree && read<void *>(object,0x70)==activePool) {
        ++record[FreeSeen];reinterpret_cast<void (*)(OSObject *)>(originalFree)(object);return;
    }
    // Ownership was not established. Do not guess a pool or repeat release.
    child=object;record[Failure]=13;record[Retained]=1;
}
bool prepare(KernelPatcher &patcher,size_t index,mach_vm_address_t base,size_t size) {
    if(!enabled())return true;
    const char *names[]{"__ZN18IGStolenMemoryPool8allocateEm","__ZN18IGStolenMemoryPool10deallocateEP24IGStolenMemoryDescriptorym",
        "__ZN24IGStolenMemoryDescriptor12withSubRangeEP18IGStolenMemoryPoolP18IOMemoryDescriptoryyj",
        "__ZN24IGStolenMemoryDescriptor4freeEv","__ZN15IGMemoryManager14registerEventsEv","__ZN10IGPagePool14registerEventsEv"};
    const unsigned offsets[]{0xC876,0xC91E,0xFC90,0xFD58,0xF450,0xBA8C};
    constexpr uint8_t entry[]{0x55,0x48,0x89,0xE5};
    bool ok=size>0xD6360 && checkKernelArgument("-wex-rplp-native-factory") && checkKernelArgument("-wex-rplp-cache-shadow");
    for(unsigned i=0;ok && i<6;++i)ok=patcher.solveSymbol<mach_vm_address_t>(index,names[i],base,size)==base+offsets[i] &&
        !memcmp(reinterpret_cast<void *>(base+offsets[i]),entry,4);
    if(ok)ok=patcher.solveSymbol<mach_vm_address_t>(index,"__ZTV18IGStolenMemoryPool",base,size)==base+0xCBFF8 &&
        patcher.solveSymbol<mach_vm_address_t>(index,"__ZTV24IGStolenMemoryDescriptor",base,size)==base+0xCC710 &&
        read<uintptr_t>(reinterpret_cast<void *>(base),0xCC7B0)==base+0xFD58;
    if(ok) {
        const auto superVtable=read<uintptr_t>(reinterpret_cast<void *>(base),0xC81A0);
        superFree=superVtable ? read<uintptr_t>(reinterpret_cast<void *>(superVtable),0xA0) : 0;
        ok=superFree!=0;
        if(ok){KernelPatcher::RouteRequest route{names[3],descriptorFree,originalFree};ok=patcher.routeMultiple(index,&route,1,base,size);}
    }
    patcher.clearError();record[PatchReady]=ok;
    if(ok){allocate=base+0xC876;registerEvents=base+0xF450;poolVtable=base+0xCC008;descriptorVtable=base+0xCC720;
        cpus=read<const uint32_t *>(reinterpret_cast<void *>(base),0xC8240);}
    return ok;
}
static bool empty(OSObject *pool,uint64_t rounds) {
    return read<uint64_t>(pool,0x50)==rounds && read<uint64_t>(pool,0x58)==rounds &&
        !read<uint64_t>(pool,0x68) && !read<void *>(pool,0x70) &&
        read<uintptr_t>(pool,0x78)==reinterpret_cast<uintptr_t>(pool)+0x70;
}
static bool linked(OSObject *pool,OSObject *object,uint64_t round) {
    return read<uint64_t>(pool,0x50)==round+1 && read<uint64_t>(pool,0x58)==round &&
        read<uint64_t>(pool,0x68)==4096 && read<void *>(pool,0x70)==object &&
        read<uintptr_t>(pool,0x78)==reinterpret_cast<uintptr_t>(object)+0x80 &&
        !read<void *>(object,0x80) && read<uintptr_t>(object,0x88)==reinterpret_cast<uintptr_t>(pool)+0x70;
}
bool run(OSObject *manager,IOService *owner,IOPCIDevice *pci,bool prerequisite) {
    if(!enabled())return true;
    if(record[Requested])return false;
    gpu=pci;record[Requested]=1;record[Prerequisite]=prerequisite;
    if(!prerequisite || !record[PatchReady] || !manager || !owner || !pci || IOMapper::gSystem ||
        !CacheShadow::passed() || !PageArray::integrated() || !cpus || *cpus!=16 ||
        !Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
            checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
            checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs")) ||
        pci->configRead32(0)!=0xA7A08086 || (pci->configRead32(8)&255)!=4 ||
        manager->getMetaClass()->getClassSize()!=0x118 || read<void *>(manager,0x10)!=owner) {
        record[Failure]=1;publish();return false;
    }
    record[CPUCount]=*cpus;
    auto pool=read<OSObject *>(manager,0x78);
    auto parent=pool ? OSDynamicCast(IOMemoryDescriptor,read<OSObject *>(pool,0x18)) : nullptr;
    IOByteCount span=0;record[Physical]=parent ? parent->getPhysicalSegment(0,&span,kIOMemoryMapperNone) : 0;
    const auto layout=XELPMemory::decode(pci->configRead16(0x50),pci->configRead32(0xC0),pci->configRead32(0xC4));
    record[PoolValid]=pool && pool->getMetaClass()->getClassSize()==0x80 && read<uintptr_t>(pool,0)==poolVtable &&
        read<void *>(pool,0x10)==owner && parent && parent->getLength()==4096 && span>=4096 &&
        record[Physical] && !(record[Physical]&4095) && record[Physical]<0x8000000000ULL &&
        layout.valid && !(record[Physical]>=layout.base && record[Physical]<layout.base+layout.bytes) &&
        read<uint64_t>(pool,0x30)==record[Physical] && read<uint64_t>(pool,0x60)==4096 && empty(pool,0);
    if(!record[PoolValid]){record[Failure]=2;publish();return false;}
    auto pages=read<OSObject **>(manager,0x110);
    if(!pages){record[Failure]=3;publish();return false;}
    for(unsigned i=0;i<16;++i) {
        auto p=pages[i];
        if(!p || p->getMetaClass()->getClassSize()!=0x78 || read<void *>(p,0x18)!=owner ||
            read<uint32_t>(p,0x60) || read<uint8_t>(p,0x64) || read<void *>(p,0x40) || read<void *>(p,0x48)) {
            record[Failure]=3;publish();return false;
        }
        ++record[PagePoolsChecked];
    }
    auto saved=static_cast<uint32_t *>(IOMalloc(4096));
    if(!saved){record[Failure]=4;publish();return false;}
    for(unsigned round=0;round<2 && !record[Failure];++round) {
        ++record[AllocateCalls];activePool=pool;constructing=true;
        auto allocated=reinterpret_cast<OSObject *(*)(OSObject *,uint64_t)>(allocate)(pool,4096);
        constructing=false;
        if(record[Failure]){if(!child)child=allocated;break;}
        child=allocated;
        if(!child){record[Failure]=5;break;}
        ++record[Created];
        auto desc=OSDynamicCast(IOMemoryDescriptor,child);
        IOByteCount childSpan=0;
        if(!desc || child->getMetaClass()->getClassSize()!=0x90 || read<uintptr_t>(child,0)!=descriptorVtable ||
            read<void *>(child,0x70)!=pool || read<uint64_t>(child,0x68) || read<uint32_t>(child,0x78)!=2 ||
            desc->getLength()!=4096 || desc->getPhysicalSegment(0,&childSpan,kIOMemoryMapperNone)!=record[Physical] ||
            childSpan<4096 || !linked(pool,child,round)){record[Failure]=6;break;}
        ++record[DescriptorValid];
        ++record[ExhaustionCalls];constructing=true;
        extra=reinterpret_cast<OSObject *(*)(OSObject *,uint64_t)>(allocate)(pool,4096);constructing=false;
        if(extra || !linked(pool,child,round)){record[Failure]=7;break;}
        ++record[ExhaustionRefused];
        auto pm=parent->map();auto cm=desc->map();
        const auto validMap=[](IOMemoryMap *m){return m && m->getLength()==4096 && m->getVirtualAddress() && !(m->getVirtualAddress()&3);};
        if(validMap(pm) && validMap(cm)) {
            ++record[MapPairs];
            auto a=reinterpret_cast<volatile uint32_t *>(pm->getVirtualAddress());
            auto b=reinterpret_cast<volatile uint32_t *>(cm->getVirtualAddress());
            bool same=true;
            for(unsigned i=0;i<1024;++i){saved[i]=a[i];if(b[i]!=saved[i])same=false;}
            if(same) {
                for(unsigned i=0;i<1024;++i)b[i]=0x93200000U^(round*0x10000U+i);
                for(unsigned i=0;i<1024;++i)if(a[i]==(0x93200000U^(round*0x10000U+i)))++record[AliasWords];else record[Failure]=9;
                // Restore both aliases before releasing either mapping or child.
                for(unsigned i=0;i<1024;++i){a[i]=saved[i];b[i]=saved[i];}
                for(unsigned i=0;i<1024;++i)if(a[i]==saved[i] && b[i]==saved[i])++record[RestoredWords];else record[Failure]=10;
            }else record[Failure]=8;
        }else record[Failure]=8;
        if(cm)cm->release();if(pm)pm->release();
        // Known descriptor/list ownership was proven before mapping; native free
        // returns its range and unlinks it. Unknown ownership above is retained.
        expectedFree=child;const auto previousFree=record[FreeSeen];child->release();expectedFree=nullptr;
        if(record[FreeSeen]!=previousFree+1){record[Failure]=14;uncertain=true;break;}
        child=nullptr;++record[ChildReleased];
        if(empty(pool,round+1))++record[EmptyRounds];else {record[Failure]=11;uncertain=true;}
    }
    IOFree(saved,4096);
    if(!record[Failure]) {
        record[EventsEntered]=1;record[EventsResult]=reinterpret_cast<bool (*)(OSObject *)>(registerEvents)(manager);
        bool unchanged=record[EventsResult] && cpus && *cpus==16 && read<OSObject **>(manager,0x110)==pages;
        for(unsigned i=0;unchanged && i<16;++i)unchanged=pages[i] && !read<uint8_t>(pages[i],0x64) &&
            !read<void *>(pages[i],0x40) && !read<void *>(pages[i],0x48) && read<void *>(pages[i],0x18)==owner;
        record[PagesUnchanged]=unchanged;if(!unchanged)record[Failure]=12;
    }
    if(!held())activePool=nullptr;
    record[Retained]=held();publish();return !record[Failure];
}
}}
