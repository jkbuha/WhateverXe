// Full memory-factory CPU construction, with measured no-eDRAM handling.
// No ordinary driver start, GT cache setup, engines or display programming.
#include "kern_raptorlake_p_native_factory.hpp"
#include "kern_raptorlake_p_native_factory_core.hpp"
#include "kern_raptorlake_p_cache_shadow.hpp"
#include "kern_raptorlake_p_subranges.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_raptorlake_p_page_array.hpp"
#include "kern_raptorlake_p_page_pool_core.hpp"
#include "kern_xelp_device.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMapper.h>
#include <IOKit/pci/IOPCIDevice.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace NativeFactory {
using PagePool::read;using PagePool::write;
enum Field : unsigned {Version,PatchReady,Requested,Prerequisite,MapValid,CapabilityFirst,CapabilitySecond,
    Armed,EDRAMEntered,EDRAMCleared,FactoryReturned,ResultSeen,ResultAccepted,ClassBytes,
    ArrayReleased,ManagerReleased,Retained,Failure,Fields};
static uint64_t record[Fields]{1};
static mach_vm_address_t expectedVtable,originalEDRAM;
static OSObject *manager;
static IOService *owner;
static IOPCIDevice *gpu;
static volatile uint32_t *capability;
static void publish(){if(gpu)gpu->setProperty("WhateverXeRPLPNativeFactory",record,sizeof(record));}
bool enabled(){return checkKernelArgument("-wex-rplp-native-factory");}
static bool readCapability(Capability &out) {struct IO{volatile uint32_t *p;uint32_t read(){return *p;}} io{capability};return absent(io,out);}
static void detect(OSObject *object) {
    ++record[EDRAMEntered];
    if(!manager || object!=manager || !owner || !record[Armed] || read<void *>(object,0x10)!=owner) {
        record[Failure]=5;publish();return;
    }
    Capability cap{};
    if(!readCapability(cap) || cap.first!=record[CapabilityFirst])record[Failure]=6;
    // The gate was opened only for a measured disabled capability. Never call
    // the donor's legacy forcewake/registry probe, including on a changed read.
    write(object,0x20,uint16_t(0));record[EDRAMCleared]=1;publish();
}
bool prepare(KernelPatcher &patcher,size_t index,mach_vm_address_t address,size_t size) {
    if(!enabled())return true;
    const char *name="__ZN21IntelTGLMemoryManager11detectEDRAMEv";
    constexpr uint8_t entry[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56};
    bool ready=size>0xD6360 && patcher.solveSymbol<mach_vm_address_t>(index,name,address,size)==address+0x4BAAE &&
        patcher.solveSymbol<mach_vm_address_t>(index,"__ZTV21IntelTGLMemoryManager",address,size)==address+0xD61F8 &&
        !memcmp(reinterpret_cast<void *>(address+0x4BAAE),entry,8) &&
        read<uintptr_t>(reinterpret_cast<void *>(address),0xD6320)==address+0xE580 &&
        read<uintptr_t>(reinterpret_cast<void *>(address),0xD6350)==address+0x4BAAE;
    if(ready){KernelPatcher::RouteRequest route{name,detect,originalEDRAM};ready=patcher.routeMultiple(index,&route,1,address,size);}
    patcher.clearError();record[PatchReady]=ready;if(ready)expectedVtable=address+0xD6208;
    return ready && (!CacheShadow::enabled() || CacheShadow::prepare(patcher,index,address,size)) &&
        (!Subranges::enabled() || Subranges::prepare(patcher,index,address,size));
}
bool arm(OSObject *object,IOService *accelerator,IOPCIDevice *pci,bool prerequisite) {
    if(!enabled() || !object || !accelerator || !pci || record[Requested])return false;
    gpu=pci;record[Requested]=1;record[Prerequisite]=prerequisite;
    if(!record[PatchReady] || !prerequisite || IOMapper::gSystem || !PageArray::integrated() ||
       !checkKernelArgument("-wex-rplp-page-array") || !checkKernelArgument("-wex-rplp-page-pool") ||
       !checkKernelArgument("-wex-rplp-manager-pools") ||
       !Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
        checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
        checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs"))) {
        record[Failure]=1;publish();return false;
    }
    if(object->getMetaClass()->getClassSize()!=0x118 || read<uintptr_t>(object,0)!=expectedVtable ||
       read<void *>(object,0x10)!=accelerator || !read<void *>(object,0x110)) {record[Failure]=2;publish();return false;}
    auto map=read<IOMemoryMap *>(object,0x68);
    const uint64_t bar=(uint64_t(pci->configRead32(0x14))<<32)|(pci->configRead32(0x10)&~0xFULL);
    record[MapValid]=map && map->getLength()==0x1000000 && map->getPhysicalAddress()==bar &&
        map->getVirtualAddress() && map->getVirtualAddress()==read<uintptr_t>(object,0x18) &&
        !(map->getVirtualAddress()&3) && (pci->configRead16(4)&2) && pci->configRead32(0)==0xA7A08086 &&
        (pci->configRead32(8)&0xFF)==4;
    if(!record[MapValid]){record[Failure]=3;publish();return false;}
    capability=reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress()+0x120010);
    Capability cap{};const bool disabled=readCapability(cap);
    record[CapabilityFirst]=cap.first;record[CapabilitySecond]=cap.second;
    if(!disabled){record[Failure]=4;capability=nullptr;publish();return false;}
    manager=object;owner=accelerator;record[Armed]=1;publish();return true;
}
bool consume(OSObject *result) {
    if(!enabled() || !result || result!=manager || !record[Armed])return false;
    if(record[ResultSeen])return true; // retained ownership: never retry cleanup
    record[ResultSeen]=1;record[ClassBytes]=result->getMetaClass()->getClassSize();
    record[ResultAccepted]=record[ClassBytes]==0x118 && read<uintptr_t>(result,0)==expectedVtable &&
        read<void *>(result,0x10)==owner && record[EDRAMEntered]==1 && record[EDRAMCleared] &&
        !read<uint16_t>(result,0x20) && !record[Failure];
    if(!record[ResultAccepted] && !record[Failure])record[Failure]=7;
    if(record[ResultAccepted] && CacheShadow::enabled() && !CacheShadow::run(result,owner,gpu))record[Failure]=10;
    if(record[ResultAccepted] && Subranges::enabled()) {
        if(!Subranges::run(result,owner,gpu,!record[Failure]))record[Failure]=11;
        if(Subranges::held()){record[Retained]=1;publish();return true;}
    }
    // Native free does not release the array on the success path. Explicitly
    // release it first, while the manager remains alive and observer-verifiable.
    record[ArrayReleased]=PageArray::releaseOwned(result);
    if(!record[ArrayReleased]){record[Retained]=1;record[Failure]=8;publish();return true;}
    result->release();record[ManagerReleased]=1;
    manager=nullptr;owner=nullptr;capability=nullptr;publish();return true;
}
bool finish(bool returned) {
    if(!enabled())return true;
    record[FactoryReturned]=returned;
    if(!record[ResultAccepted] || !record[ArrayReleased] || !record[ManagerReleased] || !returned) {
        if(!record[Failure])record[Failure]=9;
    }
    publish();return !record[Failure];
}
}}
