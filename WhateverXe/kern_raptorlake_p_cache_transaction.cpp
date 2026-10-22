// Real policy-register qualification, preserving firmware scanout and restoring
// every attempted cache write before the native manager can be released.
#include "kern_raptorlake_p_cache_transaction.hpp"
#include "kern_raptorlake_p_cache_transaction_core.hpp"
#include "kern_raptorlake_p_gtt_slot_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xelp_device.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOMemoryDescriptor.h>
#include <IOKit/IOMapper.h>
#include <IOKit/pci/IOPCIDevice.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace CacheTransaction {
static Result result;
// version, prerequisite, GOP/GT qualification, leaseStatus, leaseWrites,
// leaseReleased, leaseFailed, displayUnchanged, failure.
static uint64_t admission[9]{1};
bool enabled(){return checkKernelArgument("-wex-rplp-cache-transaction");}
struct MMIO {
    volatile uint32_t *base;
    bool readable(uint32_t o){return !(o&3) && o<0x1000000;}
    uint32_t read(uint32_t o){return base[o/4];}
    void write(uint32_t o,uint32_t v){base[o/4]=v;}
    uint64_t readPTE(uint32_t o){return *reinterpret_cast<volatile uint64_t *>(reinterpret_cast<volatile uint8_t *>(base)+o);}
    void delay(){IODelay(100);}
};
bool run(IOPCIDevice *pci,IOMemoryMap *map,const uint8_t *pat,const uint32_t *l3,
         const uint32_t *mocs,const uint32_t (*baseline)[3],bool prerequisite) {
    if(!enabled())return true;
    if(result.requested)return false;
    result.requested=1;
    const auto publish=[&]{if(pci){pci->setProperty("WhateverXeRPLPCacheTransaction",&result,sizeof(result));pci->setProperty("WhateverXeRPLPCacheAdmission",admission,sizeof(admission));}};
    admission[1]=prerequisite && pci && map && !IOMapper::gSystem && checkKernelArgument("-wex-rplp-manual-arm") &&
        Native::requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
            checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
            checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs")) &&
        pci->configRead32(0)==0xA7A08086 && (pci->configRead32(8)&255)==4 && (pci->configRead16(4)&2);
    if(!admission[1]){admission[8]=1;publish();return false;}
    const auto physical=(uint64_t(pci->configRead32(0x14))<<32)|(pci->configRead32(0x10)&~0xFULL);
    const auto layout=XELPMemory::decode(pci->configRead16(0x50),pci->configRead32(0xC0),pci->configRead32(0xC4),0x10000000);
    if(map->getLength()!=0x1000000 || map->getPhysicalAddress()!=physical ||
        !map->getVirtualAddress() || (map->getVirtualAddress()&4095) || !validPlan(pat,l3,mocs)) {
        admission[8]=2;publish();return false;
    }
    MMIO io{reinterpret_cast<volatile uint32_t *>(map->getVirtualAddress())};
    admission[2]=GTTSlot::qualify(io,layout);
    if(!admission[2]){admission[8]=3;publish();return false;}
    const auto before=captureDisplay(io,true);
    if(before.status!=DisplayStatus::Complete){admission[8]=4;publish();return false;}
    const auto lease=capture(io,true,[&](const Capture &gt) {
        if(gt.topology.eus!=96 || gt.count!=16 || gt.fuses[3]!=0)return false; // exact recorded mirror-fuse value
        for(const auto &r:gt.registers)if((r.offset==0x2030 || r.offset==0x2034 || r.offset==0x2038 || r.offset==0x203C ||
            r.offset==0x22030 || r.offset==0x22034 || r.offset==0x22038 || r.offset==0x2203C) && r.value)return false;
        const auto recheck=captureDisplay(io,true);
        if(memcmp(&before,&recheck,sizeof(before)))return false;
        result.qualified=1;transact(io,baseline,pat,l3,mocs,result);return true;
    });
    admission[3]=static_cast<uint32_t>(lease.status);admission[4]=lease.writes;
    admission[5]=lease.releasedMask;admission[6]=lease.releaseFailedMask;
    const auto after=captureDisplay(io,true);
    admission[7]=before.status==DisplayStatus::Complete && !memcmp(&before,&after,sizeof(before));
    if(lease.status!=Status::Complete || lease.releasedMask!=3 || lease.releaseFailedMask ||
        !result.qualified || !admission[7])admission[8]=4;
    publish();return !result.failure && !admission[8];
}
}}
