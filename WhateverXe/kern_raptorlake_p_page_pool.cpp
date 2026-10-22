// Standalone empty page-pool constructor preflight. Native manager gate stays shut.
#include "kern_raptorlake_p_page_pool.hpp"
#include "kern_raptorlake_p_page_pool_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xelp_device.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMapper.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/c++/OSMetaClass.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace PagePool {
enum Field : unsigned {Version,PatchReady,Requested,Prerequisite,Attempted,ClassAllocated,ClassBytes,
    InitReturned,InitResult,TailCalls,TailFailure,ShapeValid,SlotsChecked,ObjectReleased,Failure,Fields};
static uint64_t record[Fields]{1};
static IOService *activeOwner;
static OSObject *activePage;
static bool auxiliary;
static mach_vm_address_t initializer;
static Methods methods{};
bool enabled(){return checkKernelArgument("-wex-rplp-page-pool");}
bool passed(){return record[ShapeValid] && record[ObjectReleased] && !record[Failure];}
static void *allocate(size_t bytes){return IOMalloc(bytes);}
static void deallocate(void *p,size_t bytes){IOFree(p,bytes);}
static bool safeTail(void *page) {
    const unsigned failure=!activeOwner || page!=activePage ? 1 : construct(page,activeOwner,methods);
    if(!auxiliary){++record[TailCalls];record[TailFailure]=failure;}
    return !failure;
}
OSObject *create(IOService *owner) {
    if(!passed() || !owner || activeOwner || activePage)return nullptr;
    auto page=OSMetaClass::allocClassWithName("IGPagePool");
    if(!page)return nullptr;
    if(page->getMetaClass()->getClassSize()!=0x78){page->release();return nullptr;}
    activeOwner=owner;activePage=page;auxiliary=true;
    bool ok=reinterpret_cast<bool (*)(OSObject *,IOService *,uint32_t)>(initializer)(page,owner,0);
    activeOwner=nullptr;activePage=nullptr;auxiliary=false;
    if(!ok || !valid(page,owner,methods.hashVtable)){page->release();return nullptr;}
    return page;
}
bool prepare(KernelPatcher &patcher,size_t index,mach_vm_address_t address,size_t size) {
    if(!enabled())return true;
    const char *names[]{"__ZN10IGPagePool15initWithOptionsEP16IntelAcceleratorj","__ZN10IGPagePool4freeEv",
        "__ZN8IGVectorIPN10IGPagePool11PoolElementE25IGIOMallocAllocatorPolicyE4growEm",
        "__ZN8IGVectorIN11IGHashTableIPN10IGPagePool11PoolElementEmNS1_15PoolElementHashE25IGIOMallocAllocatorPolicyE4SlotES5_EC1Em",
        "__ZTV11IGHashTableIPN10IGPagePool11PoolElementEmNS0_15PoolElementHashE25IGIOMallocAllocatorPolicyE"};
    const unsigned offsets[]{0xA8AE,0xAA22,0xBB94,0xBC0C,0xCBFC0};
    constexpr uint8_t entry[]{0x55,0x48,0x89,0xE5};
    constexpr uint8_t epilogue[]{0x48,0x83,0xC4,0x08,0x5B,0x41,0x5E,0x41,0x5F,0x5D,0xC3};
    bool ready=size>0xCBFE8;
    for(unsigned i=0;ready && i<5;++i)ready=patcher.solveSymbol<mach_vm_address_t>(index,names[i],address,size)==address+offsets[i] &&
        (i==4 || !memcmp(reinterpret_cast<void *>(address+offsets[i]),entry,sizeof(entry)));
    uint8_t before[20]{},after[20]{};
    if(ready){memcpy(before,reinterpret_cast<void *>(address+0xA92E),20);ready=tailPatch(before,reinterpret_cast<uintptr_t>(safeTail),after) &&
        !memcmp(reinterpret_cast<void *>(address+0xAA17),epilogue,sizeof(epilogue));}
    if(ready){
        KernelPatcher::KextInfo target{};target.loadIndex=index;
        KernelPatcher::LookupPatch patch{&target,before,after,20,1};
        patcher.applyLookupPatch(&patch,reinterpret_cast<uint8_t *>(address+0xA92E),21);
        ready=patcher.getError()==KernelPatcher::Error::NoError && !memcmp(reinterpret_cast<void *>(address+0xA92E),after,20);
    }
    patcher.clearError();record[PatchReady]=ready;
    if(ready){initializer=address+0xA8AE;methods={allocate,deallocate,
        reinterpret_cast<bool (*)(void *,uint64_t)>(address+0xBB94),
        reinterpret_cast<void (*)(void *,uint64_t)>(address+0xBC0C),address+0xCBFD0};}
    return ready;
}
void run(IOService *owner,IOPCIDevice *pci,bool prerequisite) {
    if(!enabled() || !owner || !pci || record[Requested])return;
    record[Requested]=1;record[Prerequisite]=prerequisite;
    const auto publish=[&]{pci->setProperty("WhateverXeRPLPPagePool",record,sizeof(record));};
    if(!record[PatchReady] || !prerequisite || IOMapper::gSystem ||
       !checkKernelArgument("-wex-rplp-native-chain") || !checkKernelArgument("-wex-rplp-native-pools") ||
       !checkKernelArgument("-wex-rplp-native-pages") || !checkKernelArgument("-wex-rplp-gtt-aperture") ||
       !checkKernelArgument("-wex-rplp-gtt-slot") ||
       !Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
        checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
        checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs"))) {
        record[Failure]=1;publish();return;
    }
    record[Attempted]=1;
    // Avoid the donor's withOptions factory, which leaks its object on init failure.
    auto page=OSMetaClass::allocClassWithName("IGPagePool");
    record[ClassAllocated]=page!=nullptr;record[ClassBytes]=page ? page->getMetaClass()->getClassSize() : 0;
    if(!page){record[Failure]=2;publish();return;}
    if(record[ClassBytes]!=0x78){record[Failure]=3;page->release();record[ObjectReleased]=1;publish();return;}
    activeOwner=owner;activePage=page;
    record[InitResult]=reinterpret_cast<bool (*)(OSObject *,IOService *,uint32_t)>(initializer)(page,owner,0);
    record[InitReturned]=1;activeOwner=nullptr;activePage=nullptr;
    if(record[InitResult] && record[TailCalls]==1 && !record[TailFailure]) {
        record[ShapeValid]=valid(page,owner,methods.hashVtable);
        if(record[ShapeValid])record[SlotsChecked]=16;
    }
    if(!record[ShapeValid])record[Failure]=4;
    page->release();record[ObjectReleased]=1;publish();
}
}}
