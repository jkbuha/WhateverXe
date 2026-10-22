// Native allocator construction on owned RAM. No DSM, GGTT or fence writes.
#include "kern_raptorlake_p_native_pools.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xelp_device.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <IOKit/IOMapper.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace NativePools {
enum Field : unsigned {Version,PatchReady,Requested,Prerequisite,Attempted,Allocated,Physical,
    PoolReturned,PoolValid,DescriptorBytes,DescriptorPhysical,DescriptorSpan,PoolBytes,
    FenceReturned,FenceMask,FenceValid,PoolReleased,FenceReleased,BufferReleased,Failure,Fields};
static uint64_t record[Fields]{1};
static mach_vm_address_t poolFactory,fenceFactory;
template<class T> static T field(void *object,unsigned off){T out;memcpy(&out,static_cast<uint8_t *>(object)+off,sizeof(out));return out;}
bool enabled(){return checkKernelArgument("-wex-rplp-native-pools");}
bool passed(){return record[Requested] && record[PoolValid] && record[FenceValid] && record[PoolReleased] && record[FenceReleased] && record[BufferReleased] && !record[Failure];}
bool prepare(KernelPatcher &patcher,size_t index,mach_vm_address_t address,size_t size) {
    if(!enabled())return true;
    const char *names[]{"__ZN18IGStolenMemoryPool10withRangesERK14_stolenMemInfoP16IntelAccelerator",
        "__ZN16IGFenceAllocator11withOptionsEP16IntelAccelerator",
        "__ZN18IGStolenMemoryPool4freeEv","__ZN16IGFenceAllocator4freeEv"};
    const unsigned offsets[]{0xC586,0x1379E,0xC75E,0x13824};
    constexpr uint8_t entry[]{0x55,0x48,0x89,0xE5};
    bool valid=size>0x13836;
    for(unsigned i=0;valid && i<4;++i)valid=patcher.solveSymbol<mach_vm_address_t>(index,names[i],address,size)==address+offsets[i] &&
        !memcmp(reinterpret_cast<void *>(address+offsets[i]),entry,sizeof(entry));
    patcher.clearError();record[PatchReady]=valid;
    if(valid){poolFactory=address+offsets[0];fenceFactory=address+offsets[1];}
    return valid;
}
void run(IOService *owner,IOPCIDevice *pci,bool prerequisite) {
    if(!enabled() || !owner || !pci || record[Requested])return;
    record[Requested]=1;record[Prerequisite]=prerequisite;
    const auto publish=[&]{pci->setProperty("WhateverXeRPLPNativePools",record,sizeof(record));};
    if(!record[PatchReady] || !prerequisite || IOMapper::gSystem ||
       !checkKernelArgument("-wex-rplp-native-pages") || !checkKernelArgument("-wex-rplp-gtt-slot") ||
       !checkKernelArgument("-wex-rplp-gtt-aperture") ||
       !Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
        checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
        checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs"))) {
        record[Failure]=1;publish();return;
    }
    record[Attempted]=1;
    auto buffer=IOBufferMemoryDescriptor::withOptions(kIODirectionInOut|kIOMemoryMapperNone,4096,4096);
    OSObject *pool=nullptr,*fence=nullptr;
    do {
        IOByteCount span=0;
        const auto physical=buffer ? buffer->getPhysicalSegment(0,&span,kIOMemoryMapperNone) : 0;
        record[Physical]=physical;
        const auto layout=XELPMemory::decode(pci->configRead16(0x50),pci->configRead32(0xC0),pci->configRead32(0xC4));
        if(!buffer || buffer->getLength()!=4096 || !physical || (physical&4095) || physical>=0x8000000000ULL || span<4096 ||
           !layout.valid || (physical>=layout.base && physical<layout.base+layout.bytes)){record[Failure]=2;break;}
        record[Allocated]=1;
        // _stolenMemInfo: count, then two (physical, page-count) pairs.
        // Native descriptors borrow our wired RAM page, never physical DSM.
        const uint64_t info[]{1,physical,1,0,0};
        pool=reinterpret_cast<OSObject *(*)(const uint64_t *,IOService *)>(poolFactory)(info,owner);
        record[PoolReturned]=pool!=nullptr;
        if(!pool || pool->getMetaClass()->getClassSize()<0x80){record[Failure]=3;break;}
        auto descriptor=OSDynamicCast(IOMemoryDescriptor,field<OSObject *>(pool,0x18));
        IOByteCount descriptorSpan=0;
        record[DescriptorBytes]=descriptor ? descriptor->getLength() : 0;
        record[DescriptorPhysical]=descriptor ? descriptor->getPhysicalSegment(0,&descriptorSpan,kIOMemoryMapperNone) : 0;
        record[DescriptorSpan]=descriptorSpan;record[PoolBytes]=field<uint64_t>(pool,0x60);
        record[PoolValid]=field<void *>(pool,0x10)==owner && descriptor && field<void *>(pool,0x20) &&
            field<uint64_t>(pool,0x28)==1 && field<uint64_t>(pool,0x30)==physical && field<uint64_t>(pool,0x38)==1 &&
            !field<uint64_t>(pool,0x40) && !field<uint64_t>(pool,0x48) && !field<uint64_t>(pool,0x50) &&
            !field<uint64_t>(pool,0x58) && record[PoolBytes]==4096 && !field<uint64_t>(pool,0x68) &&
            !field<void *>(pool,0x70) && field<uintptr_t>(pool,0x78)==reinterpret_cast<uintptr_t>(pool)+0x70 &&
            record[DescriptorBytes]==4096 && record[DescriptorPhysical]==physical && descriptorSpan>=4096;
        if(!record[PoolValid]){record[Failure]=4;break;}
        fence=reinterpret_cast<OSObject *(*)(IOService *)>(fenceFactory)(owner);
        record[FenceReturned]=fence!=nullptr;
        if(fence && fence->getMetaClass()->getClassSize()>=0x48) {
            record[FenceMask]=field<uint64_t>(fence,0x40);
            record[FenceValid]=field<void *>(fence,0x10)==owner && record[FenceMask]==0xFFFFFFF8U;
        }
        if(!record[FenceValid])record[Failure]=5;
    } while(false);
    // Native pool release disposes of its borrowed physical descriptors first.
    // Only then can their owned backing page be returned to the allocator.
    if(fence){fence->release();record[FenceReleased]=1;}
    if(pool){pool->release();record[PoolReleased]=1;}
    if(buffer){buffer->release();record[BufferReleased]=1;}
    publish();
}
}}
