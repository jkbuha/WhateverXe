// Exercise real manager pool construction, then force the pre-page-pool unwind.
#include "kern_raptorlake_p_native_chain.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_raptorlake_p_page_pool.hpp"
#include "kern_raptorlake_p_page_array.hpp"
#include "kern_raptorlake_p_native_factory.hpp"
#include "kern_xelp_device.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOMapper.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace NativeChain {
enum Field : unsigned {Version,PatchReady,Requested,Attempted,Allocated,Physical,GTTAllowed,
    StopEntered,PoolValid,FenceValid,DescriptorBytes,DescriptorPhysical,FenceMask,
    CleanupSeen,CleanupCleared,FactoryReturned,BufferReleased,Retained,Failure,Fields};
static uint64_t record[Fields]{1};
static IOBufferMemoryDescriptor *buffer;
static IOService *owner;
static IOPCIDevice *gpu;
static mach_vm_address_t originalPagePool;
template<class T> static T field(void *p,unsigned off){T value;memcpy(&value,static_cast<uint8_t *>(p)+off,sizeof(value));return value;}
static void publish(){if(gpu)gpu->setProperty("WhateverXeRPLPNativeChain",record,sizeof(record));}
bool enabled(){return checkKernelArgument("-wex-rplp-native-chain");}
mach_vm_address_t pagePoolInitializer(){return record[FactoryReturned] && record[BufferReleased] && !record[Retained] && !record[Failure] ? originalPagePool : 0;}
static bool stop(OSObject *manager) {
    ++record[StopEntered];
    if(!owner || !buffer || !record[GTTAllowed] || !manager || manager->getMetaClass()->getClassSize()<0x118 ||
       field<void *>(manager,0x10)!=owner){record[Failure]=4;publish();return false;}
    auto pool=field<OSObject *>(manager,0x78),fence=field<OSObject *>(manager,0x80);
    if(pool && pool->getMetaClass()->getClassSize()>=0x80) {
        auto desc=OSDynamicCast(IOMemoryDescriptor,field<OSObject *>(pool,0x18));
        IOByteCount span=0;
        record[DescriptorBytes]=desc ? desc->getLength() : 0;
        record[DescriptorPhysical]=desc ? desc->getPhysicalSegment(0,&span,kIOMemoryMapperNone) : 0;
        record[PoolValid]=field<void *>(pool,0x10)==owner && field<void *>(pool,0x20) &&
            field<uint64_t>(pool,0x28)==1 && field<uint64_t>(pool,0x30)==record[Physical] &&
            field<uint64_t>(pool,0x38)==1 && !field<uint64_t>(pool,0x40) && !field<uint64_t>(pool,0x48) &&
            record[DescriptorBytes]==4096 && record[DescriptorPhysical]==record[Physical] && span>=4096 &&
            field<uint64_t>(pool,0x60)==4096 && !field<uint64_t>(pool,0x50) && !field<uint64_t>(pool,0x58) &&
            !field<uint64_t>(pool,0x68) && !field<void *>(pool,0x70) &&
            field<uintptr_t>(pool,0x78)==reinterpret_cast<uintptr_t>(pool)+0x70;
    }
    if(fence && fence->getMetaClass()->getClassSize()>=0x48) {
        record[FenceMask]=field<uint64_t>(fence,0x40);
        record[FenceValid]=field<void *>(fence,0x10)==owner && record[FenceMask]==0xFFFFFFF8U;
    }
    if(!record[PoolValid] || !record[FenceValid])record[Failure]=5;
    if(!record[Failure] && PageArray::integrated()) {
        PagePool::run(owner,gpu,true);
        if(!PagePool::passed() || !PageArray::populate(owner,gpu,manager,true,originalPagePool))record[Failure]=8;
        if(!record[Failure] && NativeFactory::enabled()) {
            const bool complete=NativeFactory::arm(manager,owner,gpu,true);publish();return complete;
        }
    }
    // Unless the measured full-factory trial is armed, stop before eDRAM probe.
    publish();return false;
}
bool prepare(KernelPatcher &patcher,size_t index,mach_vm_address_t address,size_t size) {
    if(!enabled())return true;
    const char *name="__ZN15IGMemoryManager12initPagePoolEv";
    constexpr uint8_t entry[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56};
    bool ready=size>0xEE69 && patcher.solveSymbol<mach_vm_address_t>(index,name,address,size)==address+0xED9E &&
        !memcmp(reinterpret_cast<void *>(address+0xED9E),entry,sizeof(entry));
    if(ready){KernelPatcher::RouteRequest gate{name,stop,originalPagePool};ready=patcher.routeMultiple(index,&gate,1,address,size);}
    patcher.clearError();record[PatchReady]=ready;return ready;
}
bool begin(IOService *object,IOPCIDevice *pci,uint64_t *info) {
    if(!enabled() || !object || !pci || !info || record[Requested])return false;
    owner=object;gpu=pci;record[Requested]=1;
    if(!record[PatchReady] || IOMapper::gSystem || !checkKernelArgument("-wex-rplp-native-pools") ||
       !checkKernelArgument("-wex-rplp-native-pages") || !checkKernelArgument("-wex-rplp-gtt-aperture") ||
       !checkKernelArgument("-wex-rplp-gtt-slot") ||
       !Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
        checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
        checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs"))) {
        record[Failure]=1;publish();owner=nullptr;return false;
    }
    record[Attempted]=1;
    buffer=IOBufferMemoryDescriptor::withOptions(kIODirectionInOut|kIOMemoryMapperNone,4096,4096);
    IOByteCount span=0;
    record[Physical]=buffer ? buffer->getPhysicalSegment(0,&span,kIOMemoryMapperNone) : 0;
    const auto layout=XELPMemory::decode(pci->configRead16(0x50),pci->configRead32(0xC0),pci->configRead32(0xC4));
    if(!buffer || buffer->getLength()!=4096 || !record[Physical] || (record[Physical]&4095) ||
       record[Physical]>=0x8000000000ULL || span<4096 || !layout.valid ||
       (record[Physical]>=layout.base && record[Physical]<layout.base+layout.bytes)) {
        if(buffer){buffer->release();buffer=nullptr;record[BufferReleased]=1;}
        record[Failure]=2;publish();owner=nullptr;return false;
    }
    const uint64_t ranges[]{1,record[Physical],1,0,0};memcpy(info,ranges,sizeof(ranges));
    record[Allocated]=1;publish();return true;
}
bool allowGTT(bool verified) {
    if(!enabled() || !owner || !buffer || !record[PatchReady] || record[Failure])return false;
    if(!verified){record[Failure]=3;publish();return false;}
    record[GTTAllowed]=1;publish();return true;
}
void observeCleanup(OSObject *manager) {
    if(!enabled() || !owner || !manager || field<void *>(manager,0x10)!=owner)return;
    ++record[CleanupSeen];
    record[CleanupCleared]=!field<void *>(manager,0x98) && !field<void *>(manager,0x78) && !field<void *>(manager,0x80);
    publish();
}
bool finish(bool returnedAndUnwound) {
    if(!enabled())return true;
    record[FactoryReturned]=returnedAndUnwound;
    if(buffer) {
        if(returnedAndUnwound && record[CleanupSeen] && record[CleanupCleared]) {
            buffer->release();buffer=nullptr;record[BufferReleased]=1;
        } else {record[Retained]=1;if(!record[Failure])record[Failure]=6;}
    }
    if((record[StopEntered]!=1 || !record[PoolValid] || !record[FenceValid] || !record[BufferReleased]) && !record[Failure])record[Failure]=7;
    owner=nullptr;publish();return !record[Failure];
}
}}
