// Native per-CPU page-pool array; optional real-manager construction/unwind.
#include "kern_raptorlake_p_page_array.hpp"
#include "kern_raptorlake_p_page_array_core.hpp"
#include "kern_raptorlake_p_page_pool.hpp"
#include "kern_raptorlake_p_page_pool_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xelp_device.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>
#include <IOKit/IOLib.h>
#include <IOKit/IOMapper.h>
#include <IOKit/pci/IOPCIDevice.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace PageArray {
using PagePool::read;using PagePool::write;
enum Field : unsigned {Version,PatchReady,Requested,Prerequisite,Attempted,CPUCount,ManagerAllocated,
    FactoryCalls,Created,FactoryFailures,InitReturned,InitResult,Validated,ReleaseCalled,
    ArrayCleared,ManagerReleased,Retained,Failure,Integrated,ManagerAccepted,UnwindSeen,FactoryReturned,Fields};
static uint64_t record[Fields]{2};
static uint32_t *cpuCount;
static mach_vm_address_t releaseArray,originalFactory;
static IOService *activeOwner;
static void *manager;
static IOPCIDevice *gpu;
static OSObject **ownedArray;
bool enabled(){return checkKernelArgument("-wex-rplp-page-array");}
bool integrated(){return checkKernelArgument("-wex-rplp-manager-pools");}
static void publishIntegrated(){if(gpu)gpu->setProperty("WhateverXeRPLPPageArray",record,sizeof(record));}
static void releaseObserved(void *object) {
    if(!record[Integrated] || object!=manager){reinterpret_cast<void (*)(void *)>(releaseArray)(object);return;}
    ++record[UnwindSeen];
    if(record[Retained]){publishIntegrated();return;}
    auto array=read<OSObject **>(object,0x110);
    if((!record[ReleaseCalled] && array!=ownedArray) ||
       (array && (*cpuCount!=record[CPUCount] || array!=ownedArray || record[Validated]!=record[CPUCount]))) {
        record[Retained]=1;record[Failure]=8;publishIntegrated();return;
    }
    record[ReleaseCalled]=1;reinterpret_cast<void (*)(void *)>(releaseArray)(object);
    record[ArrayCleared]=!read<void *>(object,0x110);
    if(!record[ArrayCleared]){record[Retained]=1;record[Failure]=6;}
    publishIntegrated();
}
static OSObject *factory(IOService *owner,uint32_t options) {
    if(!activeOwner || owner!=activeOwner || !manager || options || record[FactoryCalls]>=record[CPUCount])return nullptr;
    ++record[FactoryCalls];
    auto result=PagePool::create(owner);
    if(result)++record[Created];else ++record[FactoryFailures];
    return result;
}
bool prepare(KernelPatcher &patcher,size_t index,mach_vm_address_t address,size_t size) {
    if(!enabled())return true;
    const char *name="__ZN10IGPagePool11withOptionsEP16IntelAcceleratorj";
    constexpr uint8_t entry[]{0x55,0x48,0x89,0xE5};
    constexpr uint8_t countLoad[]{0x4C,0x8B,0x3D,0x8D,0x94,0x0B,0};
    bool ready=size>0xC8248 && patcher.solveSymbol<mach_vm_address_t>(index,name,address,size)==address+0xA860 &&
        patcher.solveSymbol<mach_vm_address_t>(index,"__ZN15IGMemoryManager15releasePagePoolEv",address,size)==address+0xF39E &&
        !memcmp(reinterpret_cast<void *>(address+0xA860),entry,4) && !memcmp(reinterpret_cast<void *>(address+0xF39E),entry,4) &&
        !memcmp(reinterpret_cast<void *>(address+0xEDAC),countLoad,7) &&
        !memcmp(reinterpret_cast<void *>(address+0xEE09),CleanupBefore,49);
    if(ready){
        cpuCount=read<uint32_t *>(reinterpret_cast<void *>(address),0xC8240);
        ready=cpuCount && !(reinterpret_cast<uintptr_t>(cpuCount)&3);
    }
    if(ready){
        KernelPatcher::KextInfo target{};target.loadIndex=index;
        KernelPatcher::LookupPatch patch{&target,CleanupBefore,CleanupAfter,49,1};
        auto where=reinterpret_cast<uint8_t *>(address+0xEE09);patcher.applyLookupPatch(&patch,where,50);
        ready=patcher.getError()==KernelPatcher::Error::NoError && !memcmp(where,CleanupAfter,49);
        if(ready){
            KernelPatcher::RouteRequest routes[]{{name,factory,originalFactory},
                {"__ZN15IGMemoryManager15releasePagePoolEv",releaseObserved,releaseArray}};
            ready=patcher.routeMultiple(index,routes,2,address,size);
        }
    }
    patcher.clearError();record[PatchReady]=ready;return ready;
}
void run(IOService *owner,IOPCIDevice *pci,bool prerequisite,mach_vm_address_t initializer) {
    if(integrated() || !enabled() || !owner || !pci || record[Requested])return;
    record[Requested]=1;record[Prerequisite]=prerequisite;
    const auto publish=[&]{pci->setProperty("WhateverXeRPLPPageArray",record,sizeof(record));};
    if(!record[PatchReady] || !prerequisite || !initializer || IOMapper::gSystem || !PagePool::passed() ||
       !checkKernelArgument("-wex-rplp-page-pool") || !checkKernelArgument("-wex-rplp-native-chain") ||
       !Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
        checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
        checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs"))) {
        record[Failure]=1;publish();return;
    }
    record[CPUCount]=*cpuCount;
    if(!record[CPUCount] || record[CPUCount]>64){record[Failure]=2;publish();return;}
    record[Attempted]=1;manager=IOMalloc(0x118);
    if(!manager){record[Failure]=3;publish();return;}
    memset(manager,0,0x118);write(manager,0x10,owner);record[ManagerAllocated]=1;
    activeOwner=owner;
    record[InitResult]=reinterpret_cast<bool (*)(void *)>(initializer)(manager);
    record[InitReturned]=1;activeOwner=nullptr;
    auto array=read<OSObject **>(manager,0x110);
    // The donor's allocator/releaser both use real_ncpus. Never free through a
    // changed count, or traverse an unexpected partially constructed array.
    if(*cpuCount!=record[CPUCount] || (!record[InitResult] && array)) {
        record[Retained]=1;record[Failure]=4;publish();return;
    }
    if(record[InitResult] && array && record[Created]==record[CPUCount] && !record[FactoryFailures]) {
        for(unsigned i=0;i<record[CPUCount];++i){
            if(!array[i])break;
            bool unique=true;for(unsigned j=0;j<i;++j)if(array[j]==array[i])unique=false;
            if(!unique)break;
            ++record[Validated];
        }
    }
    if(array){
        if(record[Validated]!=record[CPUCount]){record[Retained]=1;record[Failure]=5;publish();return;}
        record[ReleaseCalled]=1;reinterpret_cast<void (*)(void *)>(releaseArray)(manager);
    }
    record[ArrayCleared]=!read<void *>(manager,0x110);
    if(!record[ArrayCleared]){record[Retained]=1;record[Failure]=6;publish();return;}
    IOFree(manager,0x118);manager=nullptr;record[ManagerReleased]=1;
    if(!record[InitResult] || record[Validated]!=record[CPUCount])record[Failure]=7;
    publish();
}
bool populate(IOService *owner,IOPCIDevice *pci,OSObject *target,bool prerequisite,mach_vm_address_t initializer) {
    if(!integrated() || !owner || !pci || record[Requested])return false;
    gpu=pci;record[Integrated]=1;record[Requested]=1;record[Prerequisite]=prerequisite;
    if(!enabled() || !record[PatchReady] || !prerequisite || !initializer || IOMapper::gSystem || !PagePool::passed() ||
       !checkKernelArgument("-wex-rplp-page-pool") || !checkKernelArgument("-wex-rplp-native-chain") ||
       !Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
        checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
        checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs"))) {
        record[Failure]=1;publishIntegrated();return false;
    }
    if(!target || target->getMetaClass()->getClassSize()<0x118 || read<void *>(target,0x10)!=owner || read<void *>(target,0x110)) {
        record[Failure]=9;publishIntegrated();return false;
    }
    record[CPUCount]=*cpuCount;
    if(!record[CPUCount] || record[CPUCount]>64){record[Failure]=2;publishIntegrated();return false;}
    manager=target;record[ManagerAccepted]=1;record[Attempted]=1;activeOwner=owner;
    record[InitResult]=reinterpret_cast<bool (*)(void *)>(initializer)(manager);
    record[InitReturned]=1;activeOwner=nullptr;
    ownedArray=read<OSObject **>(manager,0x110);
    if(*cpuCount!=record[CPUCount] || (!record[InitResult] && ownedArray)) {
        record[Retained]=1;record[Failure]=4;publishIntegrated();return false;
    }
    if(record[InitResult] && ownedArray && record[Created]==record[CPUCount] && !record[FactoryFailures]) {
        for(unsigned i=0;i<record[CPUCount];++i){
            if(!ownedArray[i])break;
            bool unique=true;for(unsigned j=0;j<i;++j)if(ownedArray[j]==ownedArray[i])unique=false;
            if(!unique)break;
            ++record[Validated];
        }
    }
    if(!record[InitResult] || record[Validated]!=record[CPUCount])record[Failure]=7;
    // Leave successful arrays owned by the native manager. Its original failure
    // path invokes releasePagePool; observe that while the object is still alive.
    publishIntegrated();return !record[Failure];
}
bool finishIntegrated(bool returnedAndUnwound) {
    if(!integrated())return true;
    record[FactoryReturned]=returnedAndUnwound;
    record[ManagerReleased]=returnedAndUnwound && record[UnwindSeen] && record[ArrayCleared] && !record[Retained];
    if(!record[ManagerReleased] || !record[InitResult] || record[Validated]!=record[CPUCount]) {
        if(!record[Failure])record[Failure]=10;
        if(manager && !record[ManagerReleased])record[Retained]=1;
    }
    // The factory may already have freed the real manager. Never dereference it
    // here; ArrayCleared was recorded synchronously by releaseObserved.
    if(record[ManagerReleased]){manager=nullptr;ownedArray=nullptr;}
    publishIntegrated();return !record[Failure];
}
bool releaseOwned(OSObject *object) {
    if(!integrated() || !record[Integrated] || object!=manager || !record[InitResult] ||
       record[Validated]!=record[CPUCount] || record[Failure] || record[Retained])return false;
    releaseObserved(object);return record[ArrayCleared] && record[ReleaseCalled] && !record[Retained] && !record[Failure];
}
}}
