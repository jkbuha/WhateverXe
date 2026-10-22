// SPDX-License-Identifier: BSD-2-Clause
// The late-loaded donor links only to this KPI-only provider. Hardware policy
// remains in WhateverXe; no Lilu/plugin symbols enter the auxiliary link graph.
#include <IOKit/IOService.h>
#include <libkern/c++/OSSymbol.h>
#include <libkern/c++/OSIterator.h>
#include <mach/kmod.h>
#include "kern_raptorlake_p_reference_abi.hpp"
namespace {
IOService *control;
const OSSymbol *selector;
bool invoke(RaptorLakeP::ReferenceABI::Request &r){
 return control && selector && control->callPlatformFunction(selector,false,&r,nullptr,nullptr,nullptr)==kIOReturnSuccess;
}
RaptorLakeP::ReferenceABI::Request request(unsigned operation,void *owner,volatile void *base=nullptr,uint64_t offset=0,uint64_t value=0){
 return {1,sizeof(RaptorLakeP::ReferenceABI::Request),operation,~0U,owner,base,offset,value};
}
}
extern "C" uint32_t read32(void *,unsigned long) asm("__ZN24AppleIntelBaseController14ReadRegister32Em");
extern "C" uint32_t read32(void *owner,unsigned long offset){auto r=request(1,owner,nullptr,offset);return invoke(r)?static_cast<uint32_t>(r.value):0;}
extern "C" void write32(void *,unsigned long,uint32_t) asm("__ZN24AppleIntelBaseController15WriteRegister32Emj");
extern "C" void write32(void *owner,unsigned long offset,uint32_t value){auto r=request(2,owner,nullptr,offset,value);invoke(r);}
extern "C" uint64_t read64(void *,volatile void *,unsigned long) asm("__ZN24AppleIntelBaseController14ReadRegister64EPVvm");
extern "C" uint64_t read64(void *owner,volatile void *base,unsigned long offset){auto r=request(3,owner,base,offset);return invoke(r)?r.value:0;}
extern "C" void write64(void *,volatile void *,unsigned long,uint64_t) asm("__ZN24AppleIntelBaseController15WriteRegister64EPVvmy");
extern "C" void write64(void *owner,volatile void *base,unsigned long offset,uint64_t value){auto r=request(4,owner,base,offset,value);invoke(r);}
extern "C" uint64_t now(void *) asm("__ZN24AppleIntelBaseController9getPMTNowEv");
extern "C" uint64_t now(void *owner){auto r=request(5,owner);return invoke(r)?r.value:0;}
extern "C" IOReturn dsb(void *) asm("__ZN24AppleIntelBaseController16hwSetupDSBMemoryEv");
extern "C" IOReturn dsb(void *owner){auto r=request(6,owner);return invoke(r)?static_cast<IOReturn>(r.status):kIOReturnUnsupported;}
extern "C" kern_return_t bridgeStart(kmod_info_t *,void *){
 if(control || selector)return KERN_FAILURE;
 auto matching=IOService::serviceMatching("WhateverXeRPLPControl");
 if(!matching)return KERN_FAILURE;
 auto matches=IOService::getMatchingServices(matching);matching->release();
 if(!matches)return KERN_FAILURE;
 auto candidate=OSDynamicCast(IOService,matches->getNextObject());
 if(candidate && !matches->getNextObject()){candidate->retain();control=candidate;}
 matches->release();
 if(!control)return KERN_FAILURE;
 selector=OSSymbol::withCStringNoCopy(RaptorLakeP::ReferenceABI::selector);
 if(!selector){control->release();control=nullptr;return KERN_FAILURE;}
 return KERN_SUCCESS;
}
extern "C" kern_return_t bridgeStop(kmod_info_t *,void *){return KERN_FAILURE;}
KMOD_EXPLICIT_DECL(org.b00t0x.WhateverXeRPLPBridge,"0.1.0",bridgeStart,bridgeStop)
extern "C" {
__attribute__((visibility("hidden"))) kmod_start_func_t *_realmain=bridgeStart;
__attribute__((visibility("hidden"))) kmod_stop_func_t *_antimain=bridgeStop;
__attribute__((visibility("hidden"))) int _kext_apple_cc=__APPLE_CC__;
}
