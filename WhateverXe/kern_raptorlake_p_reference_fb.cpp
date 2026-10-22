// Optional exact-donor ABI exports, gated by physical owner and root arm.
// DSB field/register contract also documented by Steven Michaud's MIT-licensed
// HookCase. This bounded implementation uses WhateverXe's existing allocation policy.
#include "kern_raptorlake_p_reference_abi.hpp"
#ifdef WEX_RPLP_REFERENCE_BACKEND
#include "kern_raptorlake_p_reference_fb_core.hpp"
#include <Headers/kern_util.hpp>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include "wex_reference_gate.hpp"
extern void wexRplReferenceFBFailure(unsigned);
extern uint32_t wexRplReferenceFBRead(unsigned long);
namespace {
template<class T>T &field(void *owner,unsigned offset){return *reinterpret_cast<T *>(static_cast<uint8_t *>(owner)+offset);}
bool allowed(void *owner,unsigned long offset,unsigned width){if(!wexRplReferenceFBAllowed(owner) || !RaptorLakeP::ReferenceFB::range(offset,width,0x1000000)){wexRplReferenceFBFailure(20);return false;}return true;}
bool mapped(void *owner,volatile void *base,unsigned long offset){
 if(!wexRplReferenceFBAllowed(owner)){wexRplReferenceFBFailure(21);return false;}
 auto mmio=field<uintptr_t>(owner,0x9B0),ptr=reinterpret_cast<uintptr_t>(base);
 if((mmio>>47)!=0x1FFFF || !RaptorLakeP::ReferenceFB::window(ptr,offset,8,mmio)){wexRplReferenceFBFailure(22);return false;}return true;
}
}
extern "C" uint32_t rplFBRead(void *,unsigned long) asm("__ZN24AppleIntelBaseController14ReadRegister32Em");
extern "C" uint32_t rplFBRead(void *owner,unsigned long offset){return allowed(owner,offset,4)?wexRplReferenceFBRead(offset):0;}
extern "C" void rplFBWrite(void *,unsigned long,uint32_t) asm("__ZN24AppleIntelBaseController15WriteRegister32Emj");
extern "C" void rplFBWrite(void *owner,unsigned long offset,uint32_t value){if(allowed(owner,offset,4))wexRplReferenceFilteredWrite(offset,value);}
extern "C" uint64_t rplFBRead64(void *,volatile void *,unsigned long) asm("__ZN24AppleIntelBaseController14ReadRegister64EPVvm");
extern "C" uint64_t rplFBRead64(void *owner,volatile void *base,unsigned long offset){if(!mapped(owner,base,offset))return 0;return *reinterpret_cast<volatile uint64_t *>(reinterpret_cast<uintptr_t>(base)+offset);}
extern "C" void rplFBWrite64(void *,volatile void *,unsigned long,uint64_t) asm("__ZN24AppleIntelBaseController15WriteRegister64EPVvmy");
extern "C" void rplFBWrite64(void *owner,volatile void *base,unsigned long offset,uint64_t value){if(mapped(owner,base,offset))*reinterpret_cast<volatile uint64_t *>(reinterpret_cast<uintptr_t>(base)+offset)=value;}
extern "C" uint64_t rplFBTime(void *) asm("__ZN24AppleIntelBaseController9getPMTNowEv");
extern "C" uint64_t rplFBTime(void *){uint64_t ticks,ns;clock_get_uptime(&ticks);absolutetime_to_nanoseconds(ticks,&ns);return ns;}
extern "C" IOReturn rplFBDSB(void *) asm("__ZN24AppleIntelBaseController16hwSetupDSBMemoryEv");
extern "C" IOReturn rplFBDSB(void *owner){
 if(!wexRplReferenceFBAllowed(owner))return kIOReturnUnsupported;
 auto &stored=field<IOBufferMemoryDescriptor *>(owner,0xCD8);
 if(stored)return kIOReturnSuccess;
 auto bytes=field<uint32_t>(owner,0xDCC);auto offset=field<uint64_t>(owner,0xC50);
 auto mmio=field<uintptr_t>(owner,0x9B0);auto gtt=field<volatile uint8_t *>(owner,0xCA0);
 if(!RaptorLakeP::ReferenceFB::dsb(offset,bytes,page_size) || (mmio>>47)!=0x1FFFF || reinterpret_cast<uintptr_t>(gtt)!=mmio+0x800000){wexRplReferenceFBFailure(23);return kIOReturnBadArgument;}
 auto buffer=IOBufferMemoryDescriptor::withOptions(0x90,bytes,page_size);if(!buffer)return kIOReturnNoMemory;
 IOByteCount length=0;auto phys=buffer->getPhysicalSegment(0,&length,field<uint32_t>(owner,0xCF0));auto cpu=buffer->getBytesNoCopy();
 constexpr uint64_t mask=0x7FFFFFF000ULL;
 if(!cpu || length<bytes || !phys || (phys&4095) || ((phys+bytes-4096)&~mask)){buffer->release();wexRplReferenceFBFailure(24);return kIOReturnNoResources;}
 bzero(cpu,bytes);
 for(unsigned i=0;i<bytes;i+=4096)*reinterpret_cast<volatile uint64_t *>(gtt+((offset+i)>>9))=((phys+i)&mask)|7;
 __asm__ volatile("mfence" ::: "memory");stored=buffer; // pinned until reboot, including failed startup
 uint32_t chicken=1U<<23;if(field<uint8_t>(owner,0x1B5A))chicken|=(1U<<15)|(1U<<14)|(1U<<7)|(1U<<6);
 for(unsigned pipe=0;pipe<3;++pipe)for(unsigned dsb=0;dsb<3;++dsb)wexRplReferenceFilteredWrite(0x70BF0+pipe*0x1000+dsb*0x100,chicken);
 return kIOReturnSuccess;
}
bool wexRplReferenceABI(void *raw){
 using namespace RaptorLakeP::ReferenceABI;
 if(!raw)return false;
 auto &r=*static_cast<Request *>(raw);
 if(!valid(r) || !wexRplReferenceFBAllowed(r.owner))return false;
 r.status=0;
 switch(r.operation){
  case Read32:r.value=rplFBRead(r.owner,r.offset);break;
  case Write32:rplFBWrite(r.owner,r.offset,static_cast<uint32_t>(r.value));break;
  case Read64:r.value=rplFBRead64(r.owner,r.base,r.offset);break;
  case Write64:rplFBWrite64(r.owner,r.base,r.offset,r.value);break;
  case Time:r.value=rplFBTime(r.owner);break;
  case DSB:r.status=static_cast<uint32_t>(rplFBDSB(r.owner));break;
  default:return false;
 }
 return true;
}
#else
bool wexRplReferenceABI(void *){return false;}
#endif
