// Optional integration of the separately licensed RPL-P reference backend.
// BSD integration boundary; imported source retains its original license.
#include "kern_raptorlake_p_reference.hpp"
#include <Headers/kern_util.hpp>
#ifdef WEX_RPLP_REFERENCE_BACKEND
#include "kern_raptorlake_p.hpp"
#include "kern_raptorlake_p_reference_core.hpp"
#include "kern_raptorlake_p_reference_fb_core.hpp"
#include "kern_raptorlake_p_reference_snapshot.hpp"
#include "kern_xelp_device.hpp"
#include "kern_xelp_memory_layout.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xe_data_access.hpp"
#include "kern_raptorlake_p_control.hpp"
#include <Headers/kern_api.hpp>
#include <Headers/kern_devinfo.hpp>
#include <IOKit/IOCatalogue.h>
#include <libkern/c++/OSUnserialize.h>
#include <libkern/c++/OSMetaClass.h>
#include <kern/thread_call.h>
#include "kern_green.hpp"
#include "wex_reference_gate.hpp"
#include "wex_reference_framebuffer.hpp"
extern "C" const char version[];
mach_vm_address_t wexRplDisplayPipeInitOriginal;
namespace {
NGreen backend;
IOPCIDevice *target;
OSDictionary *accelerator;
thread_call_t work;
thread_call_t monitor;
thread_call_t framebufferWork;
thread_call_t moduleWork;
thread_call_t captureWork;
uint32_t captureBusy,captureSequence;
using ModuleLoader=int (*)(const char *,bool,bool,uint8_t,uint8_t,void *);
ModuleLoader moduleLoader;
uint32_t moduleState[8]{1}; // schema, requested, stage, Compat, FB, GPU, complete, failure
IOService *framebufferOwner,*firmwareOwner;
uint64_t handover[8]{1},displayPipe[8]{1},firmwareScan[8]{1};
uint64_t panelState[16]{1};
unsigned monitorSamples;

IOService *nativeOwner;
uint64_t startup[16]{1}; // schema, stage, allocation, size, init, probe, matched, ID, rev, selector, attached, startEntered, startResult, registered, presented PCI ID, failure
uint32_t state[9]{1}; // version, physical, FB start blocked, GPU patched, armed, queued, published, failure, IOAF ready
const char *fbPaths[]{"/Library/Extensions/AppleIntelTGLGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelTGLGraphicsFramebuffer"};
KernelPatcher::KextInfo fbInfo{"com.apple.driver.AppleIntelTGLGraphicsFramebuffer",fbPaths,1,{true},{},KernelPatcher::KextInfo::Unloaded};
const char *compatPaths[]{"/Library/Extensions/AppleIntelTGLCompat.kext/Contents/MacOS/AppleIntelTGLCompat"};
KernelPatcher::KextInfo compatInfo{"org.b00t0x.AppleIntelTGLCompat",compatPaths,1,{true},{},KernelPatcher::KextInfo::Unloaded};
mach_vm_address_t compatPortOriginal;
uint32_t compatPortMode(void *port){
 // This export is also shared with the TGL backend. Leave other owners on the
 // original path; the RPL-P binding never opens that backend's hardware gate.
 if(!wexRplReferenceFBAllowed(framebufferOwner) || !port)
  return compatPortOriginal?reinterpret_cast<uint32_t (*)(void *)>(compatPortOriginal)(port):0;
 ++panelState[11];
 auto object=static_cast<OSObject *>(port);
 if(object->getMetaClass()->getClassSize()!=0x700){++panelState[14];return 0;}
 auto bytes=static_cast<uint8_t *>(port),controller=reinterpret_cast<uint8_t *>(framebufferOwner);
 uintptr_t access,expected,platform,config;
 memcpy(&access,bytes+0x578,8);memcpy(&expected,controller+0xC40,8);
 memcpy(&platform,controller+0xD40,8);memcpy(&config,bytes+0x548,8);
 if(!access || access!=expected || !RaptorLakeP::ReferenceFB::portRecord(platform,config)){++panelState[14];return 0;}
 uint32_t ddi;memcpy(&ddi,reinterpret_cast<void *>(config),4);
 if(ddi!=(config-platform-0x20)/24){++panelState[14];return 0;}
 panelState[12]=ddi;
 return panelState[13]=RaptorLakeP::ReferenceFB::portMode(ddi,ddi<2?0:backend.readReg32(0x1638A0));
}
mach_vm_address_t fbOriginal;
bool framebufferStart(IOService *,IOService *){return false;}
void publish(){if(target){target->setProperty("WhateverXeRPLPReference",state,sizeof(state));target->setProperty("WhateverXeRPLPReferenceStartup",startup,sizeof(startup));target->setProperty("WhateverXeRPLPReferencePanel",panelState,sizeof(panelState));target->setProperty("WhateverXeRPLPReferenceModules",moduleState,sizeof(moduleState));target->setProperty("WhateverXeRPLPReferenceFirmware",firmwareScan,sizeof(firmwareScan));target->setProperty("WhateverXeRPLPReferenceHandover",handover,sizeof(handover));target->setProperty("WhateverXeRPLPReferenceDisplayPipe",displayPipe,sizeof(displayPipe));}}
void moduleWorker(thread_call_param_t,thread_call_param_t){
 if(!wexRplReferencePanel() || !moduleLoader || !target || !state[1] || state[4] || state[7] || !moduleState[1])return;
 const char *ids[]{"org.b00t0x.AppleIntelTGLCompat","com.apple.driver.AppleIntelTGLGraphicsFramebuffer","com.apple.driver.AppleIntelTGLGraphics"};
 for(unsigned i=0;i<3;++i){
  moduleState[2]=i+1;publish();
  // XNU OSKext::loadKextWithIdentifier: no deferred user-space load;
  // start the module, exclude ALL personality matching (OSKextLibPrivate.h).
  moduleState[3+i]=static_cast<uint32_t>(moduleLoader(ids[i],false,true,0,2,nullptr));publish();
  if(moduleState[3+i]){moduleState[7]=i+1;publish();return;}
 }
 moduleState[6]=1;publish();
}
void ready(){RaptorLakeP::Control::update(state[1]&&state[2]&&state[3]&&state[5]&&state[8]&&(!wexRplReferencePanel() || panelState[10])&&!state[7],state[4]);publish();}
void monitorWorker(thread_call_param_t,thread_call_param_t){
 if(!state[4] || !state[6] || state[7] || !target || !nativeOwner || monitorSamples>=120)return;
 uint32_t sample[256]{};wexRplReferenceRuntime(sample,256);
 sample[1]=++monitorSamples;
 target->setProperty("WhateverXeRPLPReferenceRuntime",sample,sizeof(sample));publish();
 uint64_t deadline;clock_interval_to_deadline(1000,kMillisecondScale,&deadline);
 if(monitorSamples<120)thread_call_enter_delayed(monitor,deadline);
}
bool captureAllowed(){
 return wexRplReferencePanel() && state[4] && state[6] && !state[7] && target && nativeOwner &&
  !nativeOwner->isInactive() && framebufferOwner && !framebufferOwner->isInactive() &&
  wexRplReferenceFBAllowed(framebufferOwner);
}
void captureWorker(thread_call_param_t,thread_call_param_t){
 if(captureAllowed()){
  struct IO {
   bool readable(uint32_t reg){return !(reg&3) && reg<=0x1000000-4;}
   uint32_t read(uint32_t reg){return backend.readReg32(reg);}
  } io;
  auto sample=RaptorLakeP::ReferenceSnapshot::capture(io,++captureSequence);
  target->setProperty("WhateverXeRPLPReferenceSnapshot",&sample,sizeof(sample));
 }
 __sync_synchronize();captureBusy=0;
}
bool nativeHeadReady(){
 handover[4]=!firmwareOwner || (firmwareOwner->isInactive() && firmwareOwner->getParentEntry(gIOServicePlane)!=target);
 if(!handover[4] || !framebufferOwner || panelState[3]!=0x1C48)return false;
 IOService *head=nullptr;memcpy(&head,reinterpret_cast<uint8_t *>(framebufferOwner)+0xD60,8);
 handover[3]=head && head->getMetaClass()->getClassSize()==0x4B98 && head->getProvider()==target && (head->getState()&kIOServiceRegisteredState);
 return handover[3];
}
bool waitForNativeHead(){
 for(unsigned elapsed=0;elapsed<45000;elapsed+=25){
  handover[5]=elapsed;
  if(handover[6])return false;
  if(nativeHeadReady()){publish();return true;}
  IOSleep(25);
 }
 handover[6]=8;publish();return false;
}
void framebufferWorker(thread_call_param_t,thread_call_param_t){
 if(!wexRplReferencePanel() || !wexRplReferenceArmed() || !target || !nativeOwner || handover[1])return;
 handover[1]=1;panelState[1]=1;publish();
 // Pin only this GPU's firmware instance. Late catalogue additions cannot
 // replace an already consumed match category, so retire that instance only
 // after the exact native personality has been admitted to the catalogue.
 firmwareScan[4]=target->getRegistryEntryID();
 auto it=target->getClientIterator();
 if(it){while(auto object=it->getNextObject()){
  ++firmwareScan[1];
  auto service=OSDynamicCast(IOService,object);
  if(service && !strcmp(service->getMetaClass()->getClassName(),"IONDRVFramebuffer") && service->getParentEntry(gIOServicePlane)==target && !service->isInactive()){
   ++firmwareScan[2];firmwareScan[3]=service->getRegistryEntryID();
   auto provider=service->getProvider();firmwareScan[5]=provider?provider->getRegistryEntryID():0;
   if(firmwareOwner){handover[6]=1;it->release();publish();return;}
   firmwareOwner=service;firmwareOwner->retain();firmwareScan[6]=1;
  }
 }it->release();}
 OSString *error=nullptr;auto metadata=OSUnserializeXML(referenceFramebufferPersonalityXML,&error);
 auto dict=OSDynamicCast(OSDictionary,metadata);OSSafeReleaseNULL(error);
 if(!dict){OSSafeReleaseNULL(metadata);handover[6]=2;publish();return;}
 auto score=OSDynamicCast(OSNumber,dict->getObject("IOProbeScore"));
 auto proof=target->getProperty("WhateverXeRPLPReferencePhysical");
 auto propertyMatch=OSDictionary::withCapacity(1);
 auto ids=OSString::withCString("0xA7A08086 0x9A498086");
 auto drivers=OSArray::withCapacity(1);
 bool ok=score && score->unsigned32BitValue()==80000 && proof && propertyMatch && ids && drivers;
 ok=ok && propertyMatch->setObject("WhateverXeRPLPReferencePhysical",proof) && dict->setObject("IOPropertyMatch",propertyMatch) && dict->setObject("IOPCIPrimaryMatch",ids) && drivers->setObject(dict);
 if(ok)ok=gIOCatalogue->addDrivers(drivers,false);
 OSSafeReleaseNULL(propertyMatch);OSSafeReleaseNULL(ids);OSSafeReleaseNULL(drivers);dict->release();
 handover[1]=ok?2:3;if(!ok){handover[6]=3;publish();return;}
 publish();
 if(firmwareOwner && !firmwareOwner->isInactive()){
  if(!firmwareOwner->terminate(kIOServiceRequired|kIOServiceSynchronous) || !firmwareOwner->isInactive()){
   handover[6]=4;publish();return;
  }
 }
 target->registerService(kIOServiceAsynchronous);publish();
}
void worker(thread_call_param_t,thread_call_param_t){
 if(!state[4] || !target || !accelerator || state[7] || nativeOwner)return;
 startup[1]=1;publish();
 // One native instance, retained through failures until reboot. No matching
 // publication can race another accelerator owner.
 nativeOwner=OSDynamicCast(IOService,OSMetaClass::allocClassWithName("IntelAccelerator"));
 startup[2]=nativeOwner!=nullptr;
 if(!nativeOwner){startup[15]=1;publish();return;}
 startup[3]=nativeOwner->getMetaClass()->getClassSize();
 if(startup[3]<0x1C58){startup[15]=2;publish();return;}
 startup[1]=2;publish();
 auto defaults=OSDynamicCast(OSDictionary,accelerator->getObject("Development"));
 auto dev=defaults?OSDictionary::withDictionary(defaults):nullptr;
 if(!dev){startup[15]=3;publish();return;}
 const char *keys[]{"MultiForceWakeSelect","GraphicsSchedulerSelect","RenderCompressionMode","AllowIOSurfaceRenderCompression","SingleChannelIndex"};
 const unsigned values[]{1,4,0,0,0};
 bool configured=true;
 for(unsigned i=0;i<5;++i){auto n=OSNumber::withNumber(values[i],32);configured=configured && n && dev->setObject(keys[i],n);OSSafeReleaseNULL(n);}
 configured=configured && accelerator->setObject("Development",dev);dev->release();
 if(!configured){startup[15]=3;publish();return;}
 startup[4]=nativeOwner->init(accelerator);publish();
 if(!startup[4]){startup[15]=4;publish();return;}
 // The physical identity was pinned before backend PCI hooks. Present the
 // reference driver's supported ID only after root arm; hardware stays A7A0.
 uint32_t presentation=0x9A49,platform=0x9A490000;
 if(!target->setProperty("device-id",&presentation,4) || !target->setProperty("AAPL,ig-platform-id",&platform,4)){startup[15]=8;publish();return;}
 startup[14]=target->configRead32(0);publish();
 if(startup[14]!=0x9A498086){startup[15]=9;publish();return;}
 startup[1]=3;publish();SInt32 score=1001;
 auto probed=nativeOwner->probe(target,&score);startup[5]=reinterpret_cast<uintptr_t>(probed);
 startup[6]=probed==nativeOwner;
 auto bytes=reinterpret_cast<uint8_t *>(nativeOwner);
 uint32_t id=0,rev=0,selector=0;memcpy(&id,bytes+0x1118,4);memcpy(&rev,bytes+0x1110,4);memcpy(&selector,bytes+0x1120,4);
 startup[7]=id;startup[8]=rev;startup[9]=selector;startup[1]=4;publish();
 if(!startup[6] || id!=0x9A498086 || rev!=4 || selector!=2){startup[15]=5;publish();return;}
 startup[10]=nativeOwner->attach(target);publish();
 if(!startup[10]){startup[15]=6;publish();return;}
 if(wexRplReferencePanel() && framebufferWork){
  thread_call_enter(framebufferWork);startup[1]=8;publish();
  if(!waitForNativeHead()){startup[15]=10;publish();return;}
 }
 startup[11]=1;startup[1]=5;publish();
 startup[12]=nativeOwner->start(target);startup[1]=6;publish();
 if(!startup[12]){startup[15]=7;publish();return;}
 nativeOwner->registerService(kIOServiceAsynchronous);startup[13]=1;startup[1]=7;state[6]=1;publish();
 if(monitor)thread_call_enter(monitor);
}

void kernel(void *,KernelPatcher &p){
 if(!wexRplReferenceValidate())return;
 if(wexRplReferencePanel()){
  auto address=p.solveSymbol(KernelPatcher::KernelID,"__ZN6OSKext22loadKextWithIdentifierEPKcbbhhP7OSArray");p.clearError();
  const uint8_t prefix[]{0x55,0x48,0x89,0xE5};
  if(address && !memcmp(reinterpret_cast<void *>(address),prefix,sizeof(prefix)))moduleLoader=reinterpret_cast<ModuleLoader>(address);
 }
 XeDataAccess::processKernel(p);
}
void kext(void *,KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 if(!state[1])return;
 XeDataAccess::processKext(p,i,a,n);
 if(wexRplReferencePanel() && i==compatInfo.loadIndex){
  const char *name="__ZN17AppleIntelPortHAL13probePortModeEv";
  const uint8_t prefix[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xEC,0x10};
  bool ok=RaptorLakeP::ReferenceFB::compatImage(reinterpret_cast<void *>(a),n);
  auto entry=ok?p.solveSymbol<mach_vm_address_t>(i,name,a,n):0;p.clearError();
  ok=ok && entry>=a && n>=sizeof(prefix) && entry-a<=n-sizeof(prefix) && !memcmp(reinterpret_cast<void *>(entry),prefix,sizeof(prefix));
  if(ok){KernelPatcher::RouteRequest route{name,compatPortMode,compatPortOriginal};ok=p.routeMultiple(i,&route,1,a,n);p.clearError();}
  panelState[10]=ok;
  if(!ok)state[7]=7;
  ready();
 }
 if(i==fbInfo.loadIndex && !wexRplReferencePanel()){
  if(!wexRplReferenceImage(reinterpret_cast<void *>(a),n,false)){state[7]=3;ready();return;}
  const char *name="__ZN31AppleIntelFramebufferController5startEP9IOService";
  const auto entry=p.solveSymbol<mach_vm_address_t>(i,name,a,n);p.clearError();
  const uint8_t prefix[]{0x55,0x48,0x89,0xE5};
  bool ok=entry>=a && entry-a<n-16 && !memcmp(reinterpret_cast<void *>(entry),prefix,4);
  if(ok){KernelPatcher::RouteRequest route{name,framebufferStart,fbOriginal};ok=p.routeMultiple(i,&route,1,a,n);p.clearError();}
  if(ok)state[2]=1;else state[7]=3;ready();
 }
}
}
bool wexRplReferencePanel(){return referenceFramebufferEnabled && RaptorLakeP::Reference::enabled() && checkKernelArgument("-wex-rplp-fb");}
bool wexRplReferenceFBAllowed(const void *owner){return wexRplReferencePanel() && wexRplReferenceArmed() && state[2] && state[3] && owner && owner==framebufferOwner && panelState[3]==0x1C48;}
bool wexRplReferenceAdoptFramebuffer(void *raw){
 if(!wexRplReferencePanel())return true;
 auto owner=static_cast<IOService *>(raw);
 if(!wexRplReferenceArmed() || !state[2] || !state[3] || !owner || owner->getMetaClass()->getClassSize()!=0x1C48 || owner->getProvider()!=target || (framebufferOwner && framebufferOwner!=owner))return false;
 if(!framebufferOwner){framebufferOwner=owner;owner->retain();}
 // init/probe/attach succeeded under IOKit's matching contract before start.
 panelState[2]=1;panelState[3]=0x1C48;panelState[4]=1;panelState[5]=1;panelState[6]=1;panelState[7]=1;panelState[1]=4;
 handover[2]=1;publish();return true;
}
void wexRplReferenceFramebufferStarted(void *raw,bool ok){
 if(!wexRplReferencePanel() || raw!=framebufferOwner)return;
 panelState[8]=ok;panelState[9]=ok && (framebufferOwner->getState()&kIOServiceRegisteredState);panelState[1]=ok?6:5;
 handover[7]=ok;if(!ok){panelState[15]=7;handover[6]=7;}publish();
}
bool wexRplReferenceDisplayPipeInit(void *pipe,void *gpu,void *machine,void *frame,uint32_t index){
 using Init=bool (*)(void *,void *,void *,void *,uint32_t);
 if(!wexRplReferencePanel())return wexRplDisplayPipeInitOriginal && reinterpret_cast<Init>(wexRplDisplayPipeInitOriginal)(pipe,gpu,machine,frame,index);
 ++displayPipe[1];displayPipe[4]=index;
 auto service=static_cast<IOService *>(frame);
 displayPipe[2]=service?service->getRegistryEntryID():0;
 const auto name=service?service->getMetaClass()->getClassName():"";
 displayPipe[3]=!strcmp(name,"IONDRVFramebuffer")?1:!strcmp(name,"AppleIntelFramebufferDiags")?2:!strcmp(name,"IOPCIDevice")?3:0;
 IOService *expected=nullptr;
 if(framebufferOwner && panelState[3]==0x1C48 && index<3)memcpy(&expected,reinterpret_cast<uint8_t *>(framebufferOwner)+0xD60+8*index,8);
 displayPipe[6]=wexRplReferenceArmed() && gpu==nativeOwner && service && service==expected && service->getProvider()==target && service->getMetaClass()->getClassSize()==0x4B98;
 // Do not construct a presentation pipe for the retired firmware framebuffer:
 // its unimplemented 'vald' call otherwise waits forever in the PCI provider.
 if(!displayPipe[6] || !wexRplDisplayPipeInitOriginal){displayPipe[7]=1;publish();return false;}
 auto ok=reinterpret_cast<Init>(wexRplDisplayPipeInitOriginal)(pipe,gpu,machine,frame,index);
 displayPipe[5]=ok;if(!ok)displayPipe[7]=2;publish();return ok;
}
void wexRplReferenceFBFailure(unsigned code){__sync_bool_compare_and_swap(&panelState[15],0,code);}
uint32_t wexRplReferenceFBRead(unsigned long reg){return backend.readReg32(reg);}
bool wexRplReferenceValidate(){
 if(state[1])return !state[7];
 if(state[7] || !RaptorLakeP::Reference::enabled())return false;
 auto info=DeviceInfo::create();if(!info){state[7]=1;return false;}
 auto pci=OSDynamicCast(IOPCIDevice,info->videoBuiltin);
 bool valid=pci && WIOKit::awaitPublishing(pci) && RaptorLakeP::eligible(XELP::deviceProfile(pci),XELP::kernelOS(version),true);
 auto aperture=valid?pci->getDeviceMemoryWithRegister(0x18):nullptr;
 auto layout=aperture?XELPMemory::decode(pci->configRead16(0x50),pci->configRead32(0xC0),pci->configRead32(0xC4),aperture->getLength()):XELPMemory::Layout{};
 valid=valid && RaptorLakeP::Reference::physical(XELP::deviceProfile(pci),XELP::kernelOS(version),layout);
 if(valid){
  target=pci;target->retain();state[1]=1;
  // Record immutable physical proof BEFORE installing a registry/PCI presentation.
  uint32_t physical[]{0xA7A08086,4};target->setProperty("WhateverXeRPLPReferencePhysical",physical,sizeof(physical));
  if(!RaptorLakeP::Control::create(target)){state[7]=2;state[1]=0;}
 }else state[7]=1;
 DeviceInfo::deleter(info);publish();return state[1] && !state[7];
}
bool wexRplReferenceArmed(){return state[1] && state[4] && !state[7];}
bool wexRplReferenceImage(const void *p,unsigned long n,bool gpu){
 if(!state[1] || !p || n<32)return false;
 if(gpu && !RaptorLakeP::Native::exactImage(p,n)){state[7]=3;ready();return false;}
 const auto b=static_cast<const uint8_t *>(p);
 auto word=[&](unsigned o){uint32_t v;memcpy(&v,b+o,4);return v;};
 if(word(0)!=0xFEEDFACF || word(4)!=0x1000007 || word(16)>128 || word(20)>n-32)return false;
 const uint8_t gpuUUID[]{0xBA,0x3A,0xA1,0xC0,0xFE,0x6B,0x33,0xB3,0x9D,0x85,0x73,0xF8,0x48,0x39,0x4E,0x3D};
 const auto expected=gpu?gpuUUID:(wexRplReferencePanel()?referenceFramebufferUUID:framebufferDonorUUID);
 size_t pos=32;bool found=false;
 for(unsigned i=0;i<word(16);++i){
  if(pos>32+word(20) || 32+word(20)-pos<8)return false;
  const auto kind=word(pos),len=word(pos+4);if(len<8 || len>32+word(20)-pos)return false;
  if(kind==0x1B){if(found || len!=24 || memcmp(b+pos+8,expected,16))return false;found=true;}
  pos+=len;
 }
 if(!found || pos!=32+word(20)){state[7]=3;ready();return false;}return true;
}
void wexRplReferenceIOAF(bool ok){state[8]=ok;if(!ok)state[7]=6;ready();}
void wexRplReferenceLoaded(bool gpu){state[gpu?3:2]=1;ready();}
bool wexRplReferenceQueue(OSDictionary *dict){
 if(!state[1] || !dict || accelerator)return false;
 // Direct allocation needs the complete signed personality defaults. In
 // particular SingleChannelIndex defaults to 2 when absent, which skips
 // priority-credit initialization while selecting the weighted evaluator.
 OSString *error=nullptr;
 auto metadata=OSUnserializeXML(acceleratorPersonalityXML,&error);
 accelerator=OSDynamicCast(OSDictionary,metadata);
 OSSafeReleaseNULL(error);
 if(!accelerator)OSSafeReleaseNULL(metadata);
 if(!accelerator)return false;
 auto match=OSString::withCString("0x9a498086");
 if(!match || !accelerator->setObject("IOPCIPrimaryMatch",match)){OSSafeReleaseNULL(match);OSSafeReleaseNULL(accelerator);return false;}
 match->release();state[5]=1;ready();return true; // publication occurs ONLY after root's arm
}
#endif
namespace RaptorLakeP {namespace Reference {
bool enabled(){
#ifdef WEX_RPLP_REFERENCE_BACKEND
 return requested(checkKernelArgument("-wex-rplp-reference-backend"),referenceFramebufferEnabled,checkKernelArgument("-wex-rplp-fb"),checkKernelArgument("-wex-rplp-manual-arm"));
#else
 return false;
#endif
}
void init(){
#ifdef WEX_RPLP_REFERENCE_BACKEND
 if(!enabled())return;
 monitor=thread_call_allocate(monitorWorker,nullptr);
 if(wexRplReferencePanel())captureWork=thread_call_allocate(captureWorker,nullptr);
 if(wexRplReferencePanel())moduleWork=thread_call_allocate(moduleWorker,nullptr);
 if(wexRplReferencePanel())framebufferWork=thread_call_allocate(framebufferWorker,nullptr);
 work=thread_call_allocate(worker,nullptr);if(!work || !monitor || (wexRplReferencePanel() && (!framebufferWork || !captureWork))){state[7]=4;return;}
 lilu.onPatcherLoadForce(kernel,nullptr);
 backend.init();
 XeDataAccess::init();if(!wexRplReferencePanel())lilu.onKextLoadForce(&fbInfo,1,kext,nullptr);
 if(wexRplReferencePanel())lilu.onKextLoadForce(&compatInfo,1,kext,nullptr);
 lilu.onKextLoadForce(nullptr,0,[](void *u,KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){if(i!=fbInfo.loadIndex && i!=compatInfo.loadIndex)kext(u,p,i,a,n);},nullptr);
#endif
}
bool loadModules(){
#ifdef WEX_RPLP_REFERENCE_BACKEND
 if(!wexRplReferencePanel() || !moduleWork || !moduleLoader || !state[1] || state[4] || state[7] ||
  !__sync_bool_compare_and_swap(&moduleState[1],0,1))return false;
 publish();thread_call_enter(moduleWork);return true;
#else
 return false;
#endif
}
bool captureState(){
#ifdef WEX_RPLP_REFERENCE_BACKEND
 if(!captureWork || !captureAllowed() || !__sync_bool_compare_and_swap(&captureBusy,0,1))return false;
 thread_call_enter(captureWork);return true;
#else
 return false;
#endif
}
bool arm(){
#ifdef WEX_RPLP_REFERENCE_BACKEND
 if(!enabled() || !armable(state) || !work ||
  !__sync_bool_compare_and_swap(&state[4],0,1))return false;
 ready();thread_call_enter(work);return true;
#else
 return false;
#endif
}
}}
