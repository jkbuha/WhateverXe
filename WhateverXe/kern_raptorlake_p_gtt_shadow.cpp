// Native GGTT constructor diagnostic: RAM by default; an explicit bounded
// slot mode tests and restores one verified spare firmware mapping.
#include "kern_raptorlake_p_gtt_shadow.hpp"
#include "kern_raptorlake_p_gtt_shadow_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_raptorlake_p_gtt_slot_core.hpp"
#include "kern_raptorlake_p_gtt_aperture_core.hpp"
#include <IOKit/IOMapper.h>
#include <IOKit/IOLib.h>
#include "kern_xelp_device.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOService.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/pci/IOPCIDevice.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace GTTShadow {
enum Field : unsigned {Version,PatchReady,Attempted,Allocated,NativeReturned,NativeResult,ObjectValid,
    PrefixPages,PrefixChanged,TailPages,TailMismatches,GuardChanged,Released,Failure,
    PinRequested,PinHeldMask,PinPrepareMask,PinCheckedMask,PinStableMask,PinCompletedMask,PinReleasedMask,PinFailure,
    PinDataBefore,PinTableBefore,PinDataAfterNative,PinTableAfterNative,PinDataAfterComplete,PinTableAfterComplete,Fields};
static uint64_t record[Fields]{3};
static IOBufferMemoryDescriptor *buffer;
static IOMemoryMap *mapping;
static IOPCIDevice *gpu;
static IOBufferMemoryDescriptor *pins[2]{};
static uint64_t pinPhysical[2]{};
static bool prepared[2]{};
static GTTSlot::Result slot;
static GTTAperture::Result apertureRead;
static IOMemoryMap *apertureView,*dataView;
static bool apertureMode(){return checkKernelArgument("-wex-rplp-gtt-aperture");}
static bool prepareAperture(IOMemoryDescriptor *aperture) {
    IOByteCount span=0;
    const auto physical=aperture->getPhysicalSegment(0,&span,kIOMemoryMapperNone);
    if(!physical || span<aperture->getLength() || physical>UINT64_MAX-GTTSlot::Index*4096-4096){apertureRead.failure=1;return false;}
    dataView=pins[0]->map(kIOMapReadOnly);
    apertureView=aperture->createMappingInTask(kernel_task,0,kIOMapAnywhere|kIOMapInhibitCache|kIOMapReadOnly,GTTSlot::Index*4096,4096);
    if(!dataView || dataView->getLength()!=4096 || !dataView->getVirtualAddress() ||
       !apertureView || apertureView->getLength()!=4096 || !apertureView->getVirtualAddress() ||
       apertureView->getPhysicalAddress()!=physical+GTTSlot::Index*4096){apertureRead.failure=2;return false;}
    apertureRead.mapped=1;
    const auto cpu=reinterpret_cast<const uint32_t *>(dataView->getVirtualAddress());
    for(unsigned i=0;i<1024;++i)if(cpu[i]!=0xC01DBEAD){apertureRead.failure=3;return false;}
    // The native CPU initialization must be visible to the uncached aperture.
    for(unsigned off=0;off<4096;off+=64)asm volatile("clflush (%0)" :: "r"(reinterpret_cast<const uint8_t *>(cpu)+off) : "memory");
    asm volatile("mfence" ::: "memory");apertureRead.flushed=1;return true;
}
static bool slotMode(){return checkKernelArgument("-wex-rplp-gtt-slot");}
template<class T> static T field(void *object,unsigned off){T out;memcpy(&out,static_cast<uint8_t *>(object)+off,sizeof(out));return out;}
static void publish(){if(gpu){gpu->setProperty("WhateverXeRPLPGTTShadow",record,sizeof(record));
    if(slotMode())gpu->setProperty("WhateverXeRPLPGTTSlot",&slot,sizeof(slot));
    if(apertureMode())gpu->setProperty("WhateverXeRPLPGTTAperture",&apertureRead,sizeof(apertureRead));}}
static bool holdPages(OSObject *manager) {
    for(unsigned i=0;i<2;++i) {
        auto page=OSDynamicCast(IOBufferMemoryDescriptor,field<OSObject *>(manager,0xE0+8*i));
        const auto physical=field<uint64_t>(manager,0xF0+8*i);
        if(!page || page->getLength()!=4096 || !physical || (physical&4095) || physical>=0x8000000000ULL) {
            record[PinFailure]=1;return false;
        }
        page->retain();pins[i]=page;pinPhysical[i]=physical;record[PinHeldMask]|=1U<<i;
        if(page->prepare(kIODirectionInOut)!=kIOReturnSuccess){record[PinFailure]=1;return false;}
        prepared[i]=true;record[PinPrepareMask]|=1U<<i;
        record[PinDataBefore+i]=page->getPreparationID();
        if(record[PinDataBefore+i]<kIOPreparationIDAlwaysPrepared){record[PinFailure]=1;return false;}
    }
    return true;
}
bool enabled(){return (!apertureMode() || slotMode()) && (!slotMode() || checkKernelArgument("-wex-rplp-pin-pages")) && checkKernelArgument("-wex-rplp-gtt-shadow") &&
    checkKernelArgument("-wex-rplp-native-pages") && checkKernelArgument("-wex-rplp-native-memory");}
bool prepare(KernelPatcher &patcher,size_t index,mach_vm_address_t address,size_t size) {
    if(!enabled())return true;
    bool valid=exactSites(reinterpret_cast<void *>(address),size) &&
        patcher.solveSymbol<mach_vm_address_t>(index,"__ZN25IGHardwareGlobalPageTable15initWithOptionsEP16IntelAcceleratorRK14IGAddressRangePvyj",address,size)==address+0x101B4 &&
        patcher.solveSymbol<mach_vm_address_t>(index,"__ZN25IGHardwareGlobalPageTable4freeEv",address,size)==address+0x10312;
    valid=valid && (!slotMode() || (size>SlotLimit.offset+SlotLimit.size &&
        !memcmp(reinterpret_cast<void *>(address+SlotLimit.offset),SlotLimit.before,SlotLimit.size)));
    patcher.clearError();
    if(valid && slotMode()) {
        KernelPatcher::KextInfo target{};target.loadIndex=index;
        KernelPatcher::LookupPatch patch{&target,SlotLimit.before,SlotLimit.after,SlotLimit.size,1};
        auto where=reinterpret_cast<uint8_t *>(address+SlotLimit.offset);
        patcher.applyLookupPatch(&patch,where,SlotLimit.size+1);
        valid=patcher.getError()==KernelPatcher::Error::NoError && !memcmp(where,SlotLimit.after,SlotLimit.size);
    }
    if(valid)for(const auto &p:Patches) {
        KernelPatcher::KextInfo target{};target.loadIndex=index;
        KernelPatcher::LookupPatch patch{&target,p.before,p.after,p.size,1};
        auto where=reinterpret_cast<uint8_t *>(address+p.offset);
        patcher.applyLookupPatch(&patch,where,p.size+1);
        valid=patcher.getError()==KernelPatcher::Error::NoError && !memcmp(where,p.after,p.size);
        if(!valid)break;
    }
    patcher.clearError();record[PatchReady]=valid;return valid;
}
bool run(OSObject *manager,IOService *owner,IOPCIDevice *pci,mach_vm_address_t original) {
    if(!enabled() || !record[PatchReady] || record[Attempted] || !manager || !owner || !pci || !original ||
       !Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
        checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
        checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs")))return false;
    gpu=pci;record[Attempted]=1;
    auto aperture=pci->getDeviceMemoryWithRegister(0x18);
    const auto layout=aperture ? XELPMemory::decode(pci->configRead16(0x50),pci->configRead32(0xC0),pci->configRead32(0xC4),aperture->getLength()) : XELPMemory::Layout{};
    const auto realMMIO=field<uintptr_t>(manager,0x18);
    const auto physical=field<uint64_t>(manager,0xF0);
    auto realMap=field<IOMemoryMap *>(manager,0x68);
    if(!layout.valid || layout.bytes!=0x4000000 || layout.gttBytes!=0x800000 || layout.apertureBytes!=0x10000000 ||
       field<void *>(manager,0x10)!=owner || field<uint64_t>(manager,0xA0) || field<uint64_t>(manager,0xA8)!=layout.bytes ||
       field<void *>(manager,0x98) || !realMap || realMap->getLength()!=MMIOBytes ||
       !realMMIO || realMap->getVirtualAddress()!=realMMIO || !physical || (physical&4095) || physical>=0x8000000000ULL) {
        record[Failure]=1;publish();return false;
    }
    buffer=IOBufferMemoryDescriptor::withCapacity(BufferBytes,kIODirectionInOut);
    mapping=buffer && buffer->getLength()==BufferBytes ? buffer->map() : nullptr;
    auto bytes=mapping && mapping->getLength()==BufferBytes ? reinterpret_cast<uint8_t *>(mapping->getVirtualAddress()) : nullptr;
    if(!bytes || reinterpret_cast<uintptr_t>(bytes)>UINTPTR_MAX-BufferBytes) {record[Failure]=2;publish();return false;}
    const uintptr_t shadow=reinterpret_cast<uintptr_t>(bytes+GuardBytes);
    if(shadow==realMMIO){record[Failure]=3;publish();return false;}
    record[Allocated]=1;
    record[PinRequested]=checkKernelArgument("-wex-rplp-pin-pages");
    if(record[PinRequested] && !holdPages(manager)){record[Failure]=5;publish();return false;}
    if(slotMode()) {
        slot.requested=1;apertureRead.requested=apertureMode();slot.firmwarePTE=layout.pte(GTTSlot::Index*4096);
        const uint64_t pciBAR=(uint64_t(pci->configRead32(0x14))<<32)|(pci->configRead32(0x10)&~0xFULL);
        bool valid=!IOMapper::gSystem && pci->configRead32(0)==0xA7A08086 && pci->configRead8(8)==4 &&
            (pci->configRead16(4)&2) && realMap->getPhysicalAddress()==pciBAR &&
            record[PinHeldMask]==3 && record[PinPrepareMask]==3;
        for(unsigned i=0;valid && i<2;++i) {
            IOByteCount span=0;
            valid=pins[i] && pins[i]->getPreparationID()==record[PinDataBefore+i] &&
                pins[i]->getPhysicalSegment(0,&span,kIOMemoryMapperNone)==pinPhysical[i] && span>=4096 &&
                !(pinPhysical[i]>=layout.base && pinPhysical[i]<layout.base+layout.bytes);
        }
        if(valid && apertureRead.requested)valid=prepareAperture(aperture);
        struct IO {
            volatile uint8_t *base;
            bool readable(uint32_t off){return !(off&3) && off<=MMIOBytes-4;}
            uint32_t read(uint32_t off){return *reinterpret_cast<volatile uint32_t *>(base+off);}
            void write(uint32_t off,uint32_t v){*reinterpret_cast<volatile uint32_t *>(base+off)=v;}
            uint64_t readPTE(uint32_t off){return *reinterpret_cast<volatile uint64_t *>(base+off);}
            void restore(uint32_t off,uint64_t v){*reinterpret_cast<volatile uint64_t *>(base+off)=v;asm volatile("mfence" ::: "memory");}
            void delay(){IODelay(100);}
        } io{reinterpret_cast<volatile uint8_t *>(realMMIO)};
        slot.qualified=valid && GTTSlot::qualify(io,layout);
        if(slot.qualified)GTTSlot::transact(io,reinterpret_cast<uint64_t *>(shadow+GTTOffset),physical|1,[&] {
            // initGlobalGTT passes this range directly to the constructor.
            // Its temporary end and the verified machine-code bound select
            // exactly the spare 10MiB page; restore before native unwind.
            const uint64_t reservedBefore=field<uint64_t>(manager,0xA8),end=GTTSlot::Index*4096;
            memcpy(reinterpret_cast<uint8_t *>(manager)+0xA8,&end,sizeof(end));
            const bool ok=reinterpret_cast<bool (*)(OSObject *)>(original)(manager);
            memcpy(reinterpret_cast<uint8_t *>(manager)+0xA8,&reservedBefore,sizeof(reservedBefore));
            slot.rangeRestored=field<uint64_t>(manager,0xA8)==reservedBefore;
            record[NativeReturned]=1;record[NativeResult]=ok;return ok;
        },slot,[&] {
            // Repeat live ownership/GOP and physical PCI mapping checks after
            // the snapshot. Refusal here performs no PTE write, even restore.
            return GTTSlot::qualify(io,layout) && !IOMapper::gSystem && pci->configRead32(0)==0xA7A08086 &&
                pci->configRead8(8)==4 && (pci->configRead16(4)&2) &&
                ((uint64_t(pci->configRead32(0x14))<<32)|(pci->configRead32(0x10)&~0xFULL))==pciBAR &&
                pci->configRead16(0x50)==0x2C1 &&
                ((uint64_t(pci->configRead32(0xC4))<<32)|(pci->configRead32(0xC0)&~0xFFFULL))==layout.base;
        },[&] {
            if(!apertureRead.requested)return true;
            struct Read {
                const volatile uint32_t *words;
                uint32_t readWord(unsigned i){return words[i];}
            } reader{reinterpret_cast<const volatile uint32_t *>(apertureView->getVirtualAddress())};
            asm volatile("mfence" ::: "memory");
            return GTTAperture::verify(reader,apertureRead);
        });
        else slot.failure=5;
        auto object=field<OSObject *>(manager,0x98);
        record[ObjectValid]=slot.nativeResult && object && object->getMetaClass()->getClassSize()>=0x40 &&
            field<void *>(object,0x10)==owner && field<uint32_t>(object,0x18)==0 &&
            field<uintptr_t>(object,0x20)==realMMIO && field<uintptr_t>(object,0x28)==realMMIO+GTTOffset &&
            field<uint64_t>(object,0x30)==0x800000 && field<uint64_t>(object,0x38)==physical;
        record[PrefixPages]=layout.bytes/4096;record[TailPages]=1;
        record[TailMismatches]=slot.unexpected;
        const bool passed=!slot.failure && slot.restored && slot.rangeRestored && record[ObjectValid];
        if(!passed)record[Failure]=7;
        publish();return passed;
    }
    memset(bytes,0xA5,BufferBytes);
    auto ptes=reinterpret_cast<uint64_t *>(shadow+GTTOffset);
    const size_t count=layout.bytes/4096;record[PrefixPages]=count;record[TailPages]=Entries-count;
    for(size_t i=0;i<count;++i)ptes[i]=prefixPTE(layout.base,i);
    // Only this manager field supplies the constructor's backing. Native
    // metadata retains shadow RAM until the factory has completed its unwind.
    memcpy(reinterpret_cast<uint8_t *>(manager)+0x18,&shadow,sizeof(shadow));
    const bool ok=reinterpret_cast<bool (*)(OSObject *)>(original)(manager);
    memcpy(reinterpret_cast<uint8_t *>(manager)+0x18,&realMMIO,sizeof(realMMIO));
    record[NativeReturned]=1;record[NativeResult]=ok;
    auto object=field<OSObject *>(manager,0x98);
    record[ObjectValid]=ok && object && object->getMetaClass()->getClassSize()>=0x40 &&
        field<void *>(object,0x10)==owner && field<uint32_t>(object,0x18)==0 &&
        field<uintptr_t>(object,0x20)==shadow && field<uintptr_t>(object,0x28)==shadow+GTTOffset &&
        field<uint64_t>(object,0x30)==0x800000 && field<uint64_t>(object,0x38)==physical;
    for(size_t i=0;i<count;++i)if(ptes[i]!=prefixPTE(layout.base,i))++record[PrefixChanged];
    for(size_t i=count;i<Entries;++i)if(ptes[i]!=(physical|1))++record[TailMismatches];
    for(size_t i=0;i<GuardBytes+GTTOffset;++i)if(bytes[i]!=0xA5)++record[GuardChanged];
    for(size_t i=GuardBytes+MMIOBytes;i<BufferBytes;++i)if(bytes[i]!=0xA5)++record[GuardChanged];
    const bool passed=record[ObjectValid] && !record[PrefixChanged] && !record[TailMismatches] && !record[GuardChanged];
    if(!passed)record[Failure]=4;publish();return passed;
}
bool slotPassed(){return apertureRead.requested && apertureRead.checked==1024 && !apertureRead.failure &&
    slot.restored && slot.rangeRestored && !slot.failure && !slot.remaining && !slot.retainPins &&
    !record[Failure] && !record[PinFailure] && record[PinPrepareMask]==3 && !record[PinReleasedMask];}
bool aperturePassed(){return apertureRead.requested && apertureRead.checked==1024 && !apertureRead.failure &&
    apertureRead.released && slot.restored && !slot.failure && !slot.remaining && !slot.retainPins &&
    !record[Failure] && !record[PinFailure] && record[PinReleasedMask]==3;}
void finish() {
    // Called only after the native manager/factory has released its GGTT
    // object. No native object can retain a pointer into freed shadow RAM.
    // Unmap both CPU views before balancing the extra page wires.
    if(apertureView){apertureView->release();apertureView=nullptr;}
    if(dataView){dataView->release();dataView=nullptr;}
    if(apertureRead.requested)apertureRead.released=1;
    for(unsigned i=0;i<2;++i)if(pins[i]) {
        // A failed restoration or unexpected write may leave a GPU-visible
        // reference. Native unwind consumes only its original prepare count;
        // keep our extra wire and retain reachable until the next reboot.
        if(slot.retainPins)continue;
        if(prepared[i]) {
            const auto id=pins[i]->getPreparationID();
            record[PinDataAfterNative+i]=id;
            if(id<kIOPreparationIDAlwaysPrepared){record[PinFailure]=2;continue;}
            IOByteCount span=0;
            const auto physical=pins[i]->getPhysicalSegment(0,&span,kIOMemoryMapperNone);
            record[PinCheckedMask]|=1U<<i;
            if(id==record[PinDataBefore+i] && physical==pinPhysical[i] && span>=4096 && pins[i]->getLength()==4096)record[PinStableMask]|=1U<<i;
            else record[PinFailure]=2;
            if(pins[i]->complete(kIODirectionInOut)!=kIOReturnSuccess) {
                // A failed completion leaves the extra hold owned until reboot.
                record[PinFailure]=3;continue;
            }
            prepared[i]=false;record[PinCompletedMask]|=1U<<i;
            record[PinDataAfterComplete+i]=pins[i]->getPreparationID();
            if(record[PinDataAfterComplete+i]!=kIOPreparationIDUnprepared){record[PinFailure]=4;continue;}
        }
        pins[i]->release();pins[i]=nullptr;record[PinReleasedMask]|=1U<<i;
    }
    if(record[PinFailure] && !record[Failure])record[Failure]=6;
    if(mapping){mapping->release();mapping=nullptr;}
    if(buffer){buffer->release();buffer=nullptr;}
    if(record[Attempted]){record[Released]=1;publish();}
}
}}
