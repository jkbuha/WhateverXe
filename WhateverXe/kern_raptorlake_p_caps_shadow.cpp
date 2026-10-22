// Read-only physical capability capture; native decoding/mode writes use RAM.
#include "kern_raptorlake_p_caps_shadow.hpp"
#include "kern_raptorlake_p_cache_shadow_core.hpp"
#include "kern_raptorlake_p_page_pool_core.hpp"
#include "kern_raptorlake_p_gtt_slot_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xelp_device.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOLib.h>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMapper.h>
#include <IOKit/pci/IOPCIDevice.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace CapsShadow {
using PagePool::read;using PagePool::write;
enum Field : unsigned {Version,PatchReady,Requested,Prerequisite,Captured,Stable,LeaseStatus,LeaseWrites,
    LeaseReleased,LeaseFailed,DisplayUnchanged,Allocated,Replayed,WakeAcquired,WakeReleased,WakeFailure,
    EUs,Subslices,Slices,DonorCacheBanks,DonorURBKiB,TimestampHz,EngineMask,GlobalBefore,GlobalDecoded,
    GlobalRestored,ModeEntered,ModeWords,RAMMismatch,OwnerMismatch,Guards,Freed,Failure,Fields};
static uint64_t record[Fields]{1};
static constexpr uint32_t offsets[]{0xD00,0x145948,0x145994,0x229C,0x7008,0x7000,0x7004,
    0x9138,0x913C,0x9134,0x911C,0x9120,0x9118,0x9140};
static uint32_t samples[14][3];
static mach_vm_address_t nativeInfo,nativeMode;
static volatile uint64_t *timestamp;
static IOService *scopeOwner;
static uintptr_t scopeBase;
static CacheShadow::Wake wake;
bool enabled(){return checkKernelArgument("-wex-rplp-caps-shadow");}
bool passed(){return record[Freed] && record[ModeWords]==3 && !record[Failure];}
bool consumeWake(IOService *owner,uint32_t on,uint32_t domain) {
    if(!scopeOwner)return false;
    // Even malformed calls are suppressed while the owned-RAM replay is active.
    wake.accept(owner==scopeOwner,read<uintptr_t>(scopeOwner,0x1240)==scopeBase,on,domain);return true;
}
bool prepare(KernelPatcher &patcher,size_t index,mach_vm_address_t address,size_t size) {
    if(!enabled())return true;
    constexpr uint8_t info[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC,0x18};
    constexpr uint8_t mode[]{0x55,0x48,0x89,0xE5,0x48,0x8B,0x87,0x00,0x13,0x00,0x00};
    bool ready=size>0x14A5C0 && checkKernelArgument("-wex-rplp-manual-arm") && checkKernelArgument("-wex-rplp-cache-transaction") &&
        patcher.solveSymbol<mach_vm_address_t>(index,"__ZN16IntelAccelerator10getGPUInfoEv",address,size)==address+0x2847E &&
        patcher.solveSymbol<mach_vm_address_t>(index,"__ZN16IntelAccelerator16initHardwareCapsEv",address,size)==address+0x2A666 &&
        patcher.solveSymbol<mach_vm_address_t>(index,"__ZN16IntelAccelerator17initModeRegistersEv",address,size)==address+0x28E1A &&
        patcher.solveSymbol<mach_vm_address_t>(index,"_g_timestampFreqPerDenominator",address,size)==address+0x14A5B8 &&
        !memcmp(reinterpret_cast<void *>(address+0x2847E),info,sizeof(info)) &&
        !memcmp(reinterpret_cast<void *>(address+0x28E1A),mode,sizeof(mode));
    patcher.clearError();record[PatchReady]=ready;
    if(ready){nativeInfo=address+0x2847E;nativeMode=address+0x28E1A;timestamp=reinterpret_cast<volatile uint64_t *>(address+0x14A5B8);}
    return ready;
}
struct MMIO {
    volatile uint32_t *base;
    bool readable(uint32_t o){return !(o&3) && o<CacheShadow::Bytes;}
    uint32_t read(uint32_t o){return base[o/4];}
    void write(uint32_t o,uint32_t v){base[o/4]=v;}
    uint64_t readPTE(uint32_t o){return *reinterpret_cast<volatile uint64_t *>(reinterpret_cast<volatile uint8_t *>(base)+o);}
    void delay(){IODelay(100);}
};
bool run(IOPCIDevice *pci,IOMemoryMap *map,uint32_t *allocation,bool prerequisite) {
    if(!enabled())return true;
    if(record[Requested])return false;
    record[Requested]=1;
    const auto publish=[&]{if(pci){pci->setProperty("WhateverXeRPLPCapsShadow",record,sizeof(record));pci->setProperty("WhateverXeRPLPCapsRegisters",samples,sizeof(samples));}};
    record[Prerequisite]=prerequisite && record[PatchReady] && pci && map && allocation && !IOMapper::gSystem &&
        checkKernelArgument("-wex-rplp-manual-arm") && checkKernelArgument("-wex-rplp-cache-transaction") &&
        Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
            checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
            checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs")) &&
        pci->configRead32(0)==0xA7A08086 && (pci->configRead32(8)&255)==4 && (pci->configRead16(4)&2);
    if(!record[Prerequisite]){record[Failure]=1;publish();return false;}
    const uint64_t physical=(uint64_t(pci->configRead32(0x14))<<32)|(pci->configRead32(0x10)&~0xFULL);
    if(map->getLength()!=CacheShadow::Bytes || map->getPhysicalAddress()!=physical ||
       !map->getVirtualAddress() || (map->getVirtualAddress()&4095)) {record[Failure]=2;publish();return false;}
    const auto begin=reinterpret_cast<uintptr_t>(allocation);
    constexpr size_t AllocationBytes=CacheShadow::Bytes+2*CacheShadow::Guard;
    if(begin>UINTPTR_MAX-AllocationBytes || map->getVirtualAddress()>UINTPTR_MAX-CacheShadow::Bytes ||
       (begin<map->getVirtualAddress()+CacheShadow::Bytes && begin+AllocationBytes>map->getVirtualAddress())) {
        record[Failure]=2;publish();return false;
    }
    MMIO io{reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress())};
    const auto layout=XELPMemory::decode(pci->configRead16(0x50),pci->configRead32(0xC0),pci->configRead32(0xC4),0x10000000);
    if(!GTTSlot::qualify(io,layout)){record[Failure]=3;publish();return false;}
    const auto before=captureDisplay(io,true);
    if(before.status!=DisplayStatus::Complete){record[Failure]=4;publish();return false;}
    const auto lease=capture(io,true,[&](const Capture &gt){
        if(gt.topology.eus!=96 || gt.count!=16 || gt.fuses[3]!=0)return false;
        for(const auto &r:gt.registers)if((r.offset==0x2030 || r.offset==0x2034 || r.offset==0x2038 || r.offset==0x203C ||
            r.offset==0x22030 || r.offset==0x22034 || r.offset==0x22038 || r.offset==0x2203C) && r.value)return false;
        for(unsigned i=0;i<14;++i){samples[i][0]=offsets[i];samples[i][1]=io.read(offsets[i]);}
        bool stable=true;
        for(unsigned i=0;i<14;++i){samples[i][2]=io.read(offsets[i]);stable&=samples[i][1]==samples[i][2] && samples[i][1]!=UINT32_MAX;}
        record[Captured]=14;record[Stable]=stable;return stable;
    });
    record[LeaseStatus]=static_cast<uint32_t>(lease.status);record[LeaseWrites]=lease.writes;
    record[LeaseReleased]=lease.releasedMask;record[LeaseFailed]=lease.releaseFailedMask;
    const auto after=captureDisplay(io,true);record[DisplayUnchanged]=!memcmp(&before,&after,sizeof(before));
    if(lease.status!=Status::Complete || lease.releasedMask!=3 || lease.releaseFailedMask || !record[Stable] ||
       !record[DisplayUnchanged] || samples[7][1]!=1 || samples[8][1]!=0x3F || samples[9][1] || samples[12][1] || samples[13][1]!=0xE00FA) {
        record[Failure]=5;publish();return false;
    }
    constexpr size_t OwnerBytes=0x2000,Guard=4096,Total=2*OwnerBytes+2*Guard;
    auto storage=static_cast<uint8_t *>(IOMalloc(Total));
    if(!storage){record[Failure]=6;publish();return false;}
    record[Allocated]=1;memset(storage,0xA5,Total);
    auto owner=storage+Guard,snapshot=owner+OwnerBytes;
    memset(owner,0,OwnerBytes);
    auto ram=allocation+CacheShadow::Guard/4;
    memset(ram,0,CacheShadow::Bytes);
    for(unsigned i=0;i<14;++i)ram[offsets[i]/4]=samples[i][1];
    write(owner,0x1120,uint32_t(2));write(owner,0x1240,reinterpret_cast<uintptr_t>(ram));write(owner,0x1300,uint64_t(5));
    memcpy(snapshot,owner,OwnerBytes);
    scopeOwner=reinterpret_cast<IOService *>(owner);scopeBase=reinterpret_cast<uintptr_t>(ram);wake={};
    record[GlobalBefore]=*timestamp;
    record[Replayed]=1;reinterpret_cast<void (*)(void *)>(nativeInfo)(owner);
    record[GlobalDecoded]=*timestamp;*timestamp=record[GlobalBefore];record[GlobalRestored]=*timestamp==record[GlobalBefore];
    scopeOwner=nullptr;scopeBase=0;
    record[WakeAcquired]=wake.acquired;record[WakeReleased]=wake.released;record[WakeFailure]=wake.failure;
    record[EUs]=read<uint32_t>(owner,0x1124);record[Subslices]=read<uint32_t>(owner,0x1158);record[Slices]=read<uint32_t>(owner,0x115C);
    record[DonorCacheBanks]=read<uint32_t>(owner,0x1164);record[DonorURBKiB]=read<uint32_t>(owner,0x1140);
    record[TimestampHz]=read<uint32_t>(owner,0x1188);record[EngineMask]=read<uint64_t>(owner,0x1300);
    for(size_t i=0;i<OwnerBytes;++i)if(!((i>=0x1124 && i<0x1198) || (i>=0xDD8 && i<0xDE0) || (i>=0x1300 && i<0x1308)) && owner[i]!=snapshot[i])++record[OwnerMismatch];
    if(!record[OwnerMismatch] && !wake.failure && wake.acquired==1 && wake.released==1 && record[EUs]==96 &&
       record[Subslices]==12 && record[Slices]==1 && record[EngineMask]==5 && record[GlobalRestored]) {
        memcpy(snapshot,owner,OwnerBytes);record[ModeEntered]=1;
        reinterpret_cast<void (*)(void *)>(nativeMode)(owner);
        record[OwnerMismatch]+=memcmp(snapshot,owner,OwnerBytes)!=0;
    }
    for(size_t i=0;i<CacheShadow::Bytes/4;++i) {
        uint32_t expected=0;const auto o=static_cast<uint32_t>(i*4);
        for(unsigned j=0;j<14;++j)if(o==offsets[j])expected=samples[j][1];
        if(record[ModeEntered]) {
            if(o==0x229C || o==0x2229C)expected=0x80000;
            if(o==0x14800)expected=0x10000;
        }
        if(ram[i]!=expected)++record[RAMMismatch];
    }
    record[ModeWords]=record[ModeEntered] && ram[0x229C/4]==0x80000 && ram[0x2229C/4]==0x80000 && ram[0x14800/4]==0x10000 ? 3 : 0;
    for(size_t i=0;i<Guard;++i)if(storage[i]!=0xA5 || storage[Total-Guard+i]!=0xA5)++record[Guards];
    for(size_t i=0;i<CacheShadow::Guard/4;++i)if(allocation[i]!=CacheShadow::Sentinel || allocation[(CacheShadow::Guard+CacheShadow::Bytes)/4+i]!=CacheShadow::Sentinel)++record[Guards];
    IOFree(storage,Total);record[Freed]=1;
    if(!record[ModeEntered] || record[ModeWords]!=3 || record[RAMMismatch] || record[OwnerMismatch] || record[Guards] ||
        record[DonorCacheBanks]!=8 || record[DonorURBKiB]!=768 || !record[TimestampHz] || !record[GlobalDecoded])record[Failure]=7;
    publish();return !record[Failure];
}
}}
