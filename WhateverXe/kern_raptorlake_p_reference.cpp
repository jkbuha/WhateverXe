// Optional integration of the separately licensed RPL-P reference backend.
// BSD integration boundary; imported source retains its original license.
#include "kern_raptorlake_p_reference.hpp"
#include <Headers/kern_util.hpp>
#ifdef WEX_RPLP_REFERENCE_BACKEND
#include "kern_raptorlake_p.hpp"
#include "kern_raptorlake_p_reference_core.hpp"
#include "kern_raptorlake_p_reference_fb_core.hpp"
#include "kern_raptorlake_p_reference_snapshot.hpp"
#include "kern_raptorlake_p_reference_context.hpp"
#include "kern_backlight_profile.hpp"
#include "kern_raptorlake_p_reference_backlight.hpp"
#include "kern_raptorlake_p_reference_connectors.hpp"
#include "kern_raptorlake_p_reference_early.hpp"
#include "kern_raptorlake_p_reference_redraw.hpp"
#include "kern_raptorlake_p_reference_tc_aux.hpp"
#include <kern/thread.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include "kern_raptorlake_p_reference_shutdown.hpp"
#include <IOKit/IOLocks.h>
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
mach_vm_address_t wexRplReferenceContextFreeOriginal;
namespace {
NGreen backend;
IOPCIDevice *target;
OSDictionary *accelerator;
thread_call_t work;
thread_call_t monitor;
thread_call_t framebufferWork;
thread_call_t moduleWork;
thread_call_t captureWork;
thread_call_t earlyPublishWork;
uint32_t earlyState[8]{1}; // schema, hook ready, calls, blocked, foreign, unselected, wrong class, error
mach_vm_address_t firmwareProbeOriginal;
uint32_t captureBusy,captureSequence;
using ShutdownQuery=int (*)();
using TryRetain=bool (*)(const OSObject *,const void *);
ShutdownQuery shutdownQuery;TryRetain contextTryRetain;
IOSimpleLock *contextLock;
RaptorLakeP::ReferenceContext::Cache contextCache;
uint32_t contextState[8]{1}; // schema, stores, borrowed, dying, forgotten, released, shutdown skips, free calls
struct ContextIO {
 IOInterruptState interruptState{};
 void lock(){interruptState=IOSimpleLockLockDisableInterrupt(contextLock);}
 void unlock(){IOSimpleLockUnlockEnableInterrupt(contextLock,interruptState);}
 bool tryRetain(void *object){bool ok=contextTryRetain(static_cast<OSObject *>(object),nullptr);if(!ok)__sync_fetch_and_add(&contextState[3],1);return ok;}
};
uint32_t barrierState[8]{1}; // schema, attempts, native, refused, last reason, last result, failures, reserved
using ModuleLoader=int (*)(const char *,bool,bool,uint8_t,uint8_t,void *);
ModuleLoader moduleLoader;
uint32_t moduleState[8]{1}; // schema, requested, stage, Compat, FB, GPU, complete, failure
IOService *framebufferOwner,*firmwareOwner;
uint64_t handover[8]{1},displayPipe[8]{1},firmwareScan[8]{1};
uint64_t panelState[16]{1};
uint32_t portObserve[67]{1}; // schema, hook, error, then nine [calls,class,dispatch,vtable offset,mode,north,south] records
mach_vm_address_t portStateOriginal,portHPDOriginal,portAUXOriginal,portImageBase,portImageSize,compatPortEntry,pureVirtualEntry;
mach_vm_address_t auxReadEntry,getPortEntry;
uint32_t auxProbeAttempted,auxProbePermit;void *auxProbePort;thread_t auxProbeThread;
uint32_t portBindings[8]{1}; // schema, repaired slots, failure, original HAL/Diags dispatch, HPD holds, AUX holds, state holds
uint32_t portModes[37]{1}; // schema, then calls / register / status / mode per DDI
unsigned monitorSamples;

IOService *nativeOwner;
mach_vm_address_t acceleratorProbeOriginal;
uint32_t acceleratorProbe[7]{1}; // schema, hook, calls, allowed, refused, last reason, install error
uint64_t startup[16]{1}; // schema, stage, allocation, size, init, probe, matched, ID, rev, selector, attached, startEntered, startResult, registered, presented PCI ID, failure
uint32_t state[9]{1}; // version, physical, FB start blocked, GPU patched, armed, queued, published, failure, IOAF ready
const char *firmwarePaths[]{"/System/Library/Extensions/IONDRVSupport.kext/Contents/MacOS/IONDRVSupport"};
KernelPatcher::KextInfo firmwareInfo{"com.apple.iokit.IONDRVSupport",firmwarePaths,1,{true},{},KernelPatcher::KextInfo::Unloaded};
const char *fbPaths[]{"/Library/Extensions/AppleIntelTGLGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelTGLGraphicsFramebuffer"};
KernelPatcher::KextInfo fbInfo{"com.apple.driver.AppleIntelTGLGraphicsFramebuffer",fbPaths,1,{true},{},KernelPatcher::KextInfo::Unloaded};
const char *compatPaths[]{"/Library/Extensions/AppleIntelTGLCompat.kext/Contents/MacOS/AppleIntelTGLCompat"};
KernelPatcher::KextInfo compatInfo{"org.b00t0x.AppleIntelTGLCompat",compatPaths,1,{true},{},KernelPatcher::KextInfo::Unloaded};
const char *backlightPaths[]{"/System/Library/Extensions/AppleBacklight.kext/Contents/MacOS/AppleBacklight"};
KernelPatcher::KextInfo backlightInfo{"com.apple.driver.AppleBacklight",backlightPaths,1,{true},{},KernelPatcher::KextInfo::Unloaded};
const char *redrawPaths[]{"/System/Library/Extensions/IOGraphicsFamily.kext/Contents/MacOS/IOGraphicsFamily"};
KernelPatcher::KextInfo redrawInfo{"com.apple.iokit.IOGraphicsFamily",redrawPaths,1,{true},{},KernelPatcher::KextInfo::Unloaded};
mach_vm_address_t redrawOriginal;
uint8_t *redrawVerbose;
uint32_t redrawState[6]{1}; // schema, installed, calls, reset, foreign, install error
bool redrawRequested(){unsigned mode=0;return PE_parse_boot_argn("gfxrst",&mode,sizeof(mode)) && mode==1;}
void redrawFramebuffer(IOFramebuffer *fb){
 __sync_fetch_and_add(&redrawState[2],1);
 const bool physical=wexRplReferencePanel() && target && state[1] && !state[7];
 const bool owned=fb && fb->getProvider()==target;
 const bool head=owned && fb->getMetaClass()->getClassSize()==0x4B98;
 uint32_t index=~0U;
 if(head)memcpy(&index,reinterpret_cast<uint8_t *>(fb)+0x1DC,sizeof(index));
 const bool selected=RaptorLakeP::ReferenceRedraw::selected(redrawRequested(),physical,owned,head,index==0);
 if(selected && redrawVerbose){
  // Same reset/redraw request as WEG gfxrst=1. Never copy or clear the GOP
  // surface: its linear layout is not the native driver's tiled scanout.
  const uint8_t previous=*redrawVerbose;*redrawVerbose=1;
  reinterpret_cast<void (*)(IOFramebuffer *)>(redrawOriginal)(fb);
  *redrawVerbose=previous;__sync_fetch_and_add(&redrawState[3],1);
 }else{
  __sync_fetch_and_add(&redrawState[4],1);
  reinterpret_cast<void (*)(IOFramebuffer *)>(redrawOriginal)(fb);
 }
}
void redrawKext(void *,KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 if(i!=redrawInfo.loadIndex || !redrawRequested() || !wexRplReferencePanel() || !state[1] || state[7])return;
 const char *name="__ZN13IOFramebuffer6initFBEv";
 bool ok=RaptorLakeP::ReferenceRedraw::image(reinterpret_cast<void *>(a),n) && n>0x3F397;
 auto entry=ok?p.solveSymbol(i,name,a,n):0;p.clearError();
 auto verbose=ok?p.solveSymbol(i,"__ZL16gIOFBVerboseBoot",a,n):0;p.clearError();
 const uint8_t prefix[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xEC,0x58,0x01,0x00,0x00};
 ok=ok && entry==a+0x1B9D4 && verbose==a+0x3F397 && !memcmp(reinterpret_cast<void *>(entry),prefix,sizeof(prefix));
 if(ok){redrawVerbose=reinterpret_cast<uint8_t *>(verbose);KernelPatcher::RouteRequest route{name,redrawFramebuffer,redrawOriginal};ok=p.routeMultiple(i,&route,1,a,n);p.clearError();}
 redrawState[1]=ok;redrawState[5]=ok?0:1;
 SYSLOG("rplp","optional framebuffer redraw hook ready=%u",ok);
 if(target)target->setProperty("WhateverXeRPLPReferenceRedraw",redrawState,sizeof(redrawState));
}
mach_vm_address_t backlightOriginal;
mach_vm_address_t compatPortOriginal;
mach_vm_address_t scanoutMemoryOriginal;
uint32_t scanoutGate[7]{1}; // schema, hook, calls, forwarded, null refusals, last flags, install error
uint32_t displayPipeReadyMask,bootProbe[8]{1}; // schema, readiness checks, ready, requested, result, pipe mask, failure, mode-info status

uint32_t guardedScanoutMemory(void *pipe,void *mode,uint32_t flags){
 using Allocate=uint32_t (*)(void *,void *,uint32_t);
 const bool selected=wexRplReferencePanel() && target && state[1];
 if(selected){
  __sync_fetch_and_add(&scanoutGate[2],1);scanoutGate[5]=flags;
  // BA3AA1C0: the machine can call this with getDisplayPipe()==nullptr.
  // Refuse before forming output pointers at null+0x1298/null+0x12A0.
  // The native machine caller handles IOReturn and releases its busy lock.
  if(!pipe){__sync_fetch_and_add(&scanoutGate[4],1);return static_cast<uint32_t>(kIOReturnNotReady);}
  __sync_fetch_and_add(&scanoutGate[3],1);
 }
 return scanoutMemoryOriginal?reinterpret_cast<Allocate>(scanoutMemoryOriginal)(pipe,mode,flags):static_cast<uint32_t>(kIOReturnNotReady);
}

uint32_t guardedAcceleratorProbe(IOService *owner,uint32_t options){
 using Probe=uint32_t (*)(IOService *,uint32_t);
 if(!wexRplReferencePanel() || !target || !owner || owner->getProvider()!=target)
  return acceleratorProbeOriginal?reinterpret_cast<Probe>(acceleratorProbeOriginal)(owner,options):static_cast<uint32_t>(kIOReturnNotReady);
 __sync_fetch_and_add(&acceleratorProbe[2],1);
 unsigned reason=0;
 if(owner!=nativeOwner || owner->getMetaClass()->getClassSize()!=0x1C80)reason=1;
 else if(!state[1] || !state[4] || state[7] || owner->isInactive())reason=2;
 else if(!shutdownQuery || shutdownQuery())reason=3;
 else {
  // Exact BA3AA1C0 donor: IGHardwareContext::initWithOptions loads its
  // IGGarbageCollector from task->accelerator +0x1258. A pre-start probe may
  // reach this before that collector exists, then IGGCObject::init crashes.
  uintptr_t collector;memcpy(&collector,reinterpret_cast<uint8_t *>(owner)+0x1258,8);
  if(!collector)reason=4;
 }
 acceleratorProbe[5]=reason;
 if(reason){__sync_fetch_and_add(&acceleratorProbe[4],1);return static_cast<uint32_t>(kIOReturnNotReady);}
 __sync_fetch_and_add(&acceleratorProbe[3],1);
 return acceleratorProbeOriginal?reinterpret_cast<Probe>(acceleratorProbeOriginal)(owner,options):static_cast<uint32_t>(kIOReturnNotReady);
}

bool earlyRequested(){return checkKernelArgument("-wex-rplp-early-display");}
void earlyPublish(thread_call_param_t,thread_call_param_t){
 if(target)target->setProperty("WhateverXeRPLPEarlyDisplay",earlyState,sizeof(earlyState));
}
IOService *firmwareProbe(IOService *owner,IOService *provider,int32_t *score){
 __sync_fetch_and_add(&earlyState[2],1);
 const bool selected=earlyRequested();
 const bool physical=state[1] && !state[7] && target;
 const bool own=provider && provider==target;
 const bool firmware=owner && !strcmp(owner->getMetaClass()->getClassName(),"IONDRVFramebuffer");
 if(RaptorLakeP::ReferenceEarly::block(selected,earlyState[1],physical,own,firmware)){
  __sync_fetch_and_add(&earlyState[3],1);
  // Leave the firmware's running scanout untouched. Refuse only its fallback
  // driver binding so the first userspace framebuffer can be the native one.
  // Publish counters later, outside IOKit's probe/matching lock context.
  if(earlyPublishWork)thread_call_enter(earlyPublishWork);
  return nullptr;
 }
 __sync_fetch_and_add(&earlyState[!selected?5:!own?4:6],1);
 using Probe=IOService *(*)(IOService *,IOService *,int32_t *);
 return firmwareProbeOriginal ? reinterpret_cast<Probe>(firmwareProbeOriginal)(owner,provider,score) : nullptr;
}
void firmwareKext(void *,KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 if(i!=firmwareInfo.loadIndex || !earlyRequested() || !wexRplReferencePanel())return;
 if(!state[1] || state[7] || !earlyPublishWork){earlyState[7]=1;earlyPublish(nullptr,nullptr);return;}
 const char *name="__ZN17IONDRVFramebuffer5probeEP9IOServicePi";
 bool ok=RaptorLakeP::ReferenceEarly::image(reinterpret_cast<void *>(a),n);
 auto entry=ok?p.solveSymbol(i,name,a,n):0;p.clearError();
 const uint8_t prefix[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x53,0x50};
 ok=ok && entry>=a && n>=sizeof(prefix) && entry-a<=n-sizeof(prefix) && !memcmp(reinterpret_cast<void *>(entry),prefix,sizeof(prefix));
 if(ok){KernelPatcher::RouteRequest route{name,firmwareProbe,firmwareProbeOriginal};ok=p.routeMultiple(i,&route,1,a,n);p.clearError();}
 earlyState[1]=ok;earlyState[7]=ok?0:2;earlyPublish(nullptr,nullptr);
}
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
 const bool tcss=wexRplReferenceTCSSRequested();
 auto reg=tcss?RaptorLakeP::ReferenceFB::adlpTCSSRegister(ddi):(ddi<2?0:0x1638A0);
 auto status=reg?backend.readReg32(reg):0;
 auto mode=tcss?RaptorLakeP::ReferenceFB::adlpPortMode(ddi,status):RaptorLakeP::ReferenceFB::portMode(ddi,status);
 auto index=1+4*ddi;
 __sync_fetch_and_add(&portModes[index],1);portModes[index+1]=reg;portModes[index+2]=status;portModes[index+3]=mode;
 return panelState[13]=mode;
}

// The observer holds external ports disconnected: no Type-C ownership, AUX or
// link training is permitted until runtime dispatch has been qualified.
uint32_t observedPortState(void *port){
 if(!wexRplReferenceTCSSRequested() || !checkKernelArgument("-wex-rplp-tcss-observe"))
  return portStateOriginal?reinterpret_cast<uint32_t (*)(void *)>(portStateOriginal)(port):0;
 if(!wexRplReferenceFBAllowed(framebufferOwner) || !port)return 0;
 auto object=static_cast<OSObject *>(port);
 if(object->getMetaClass()->getClassSize()!=0x700)return 0;
 auto bytes=static_cast<uint8_t *>(port),controller=reinterpret_cast<uint8_t *>(framebufferOwner);
 uintptr_t access,expected,platform,config,table,dispatch;
 memcpy(&access,bytes+0x578,8);memcpy(&expected,controller+0xC40,8);
 memcpy(&platform,controller+0xD40,8);memcpy(&config,bytes+0x548,8);
 if(!access || access!=expected || !RaptorLakeP::ReferenceFB::portRecord(platform,config))return 0;
 uint32_t ddi;memcpy(&ddi,reinterpret_cast<void *>(config),4);
 if(ddi!=(config-platform-0x20)/24)return 0;
 // Internal Combo A retains its complete native path.
 if(ddi==0)return portStateOriginal?reinterpret_cast<uint32_t (*)(void *)>(portStateOriginal)(port):0;
 memcpy(&table,bytes,8);memcpy(&dispatch,reinterpret_cast<void *>(table+0x190),8);
 auto index=3+7*ddi;
 __sync_fetch_and_add(&portObserve[index],1);
 const char *name=object->getMetaClass()->getClassName();
 portObserve[index+1]=!strcmp(name,"AppleIntelPortHALDiags")?1:!strcmp(name,"AppleIntelPortHAL")?2:3;
 portObserve[index+2]=dispatch==compatPortEntry || dispatch==reinterpret_cast<uintptr_t>(compatPortMode)?1:dispatch==pureVirtualEntry?2:
  dispatch>=portImageBase && dispatch-portImageBase<portImageSize?3:4;
 portObserve[index+3]=table>=portImageBase && table-portImageBase<portImageSize?static_cast<uint32_t>(table-portImageBase):~0U;
 portObserve[index+4]=compatPortMode(port);
 portObserve[index+5]=backend.readReg32(0x44470);
 portObserve[index+6]=backend.readReg32(0xC4000);
 uint32_t disconnected=0;memcpy(bytes+0x554,&disconnected,4);
 __sync_fetch_and_add(&portBindings[7],1);
 return 0;
}


bool observerPort(void *port,uint32_t &ddi){
 if(!wexRplReferenceFBAllowed(framebufferOwner) || !port)return false;
 auto object=static_cast<OSObject *>(port);
 if(object->getMetaClass()->getClassSize()!=0x700)return false;
 auto bytes=static_cast<uint8_t *>(port),controller=reinterpret_cast<uint8_t *>(framebufferOwner);
 uintptr_t access,expected,platform,config;
 memcpy(&access,bytes+0x578,8);memcpy(&expected,controller+0xC40,8);
 memcpy(&platform,controller+0xD40,8);memcpy(&config,bytes+0x548,8);
 if(!access || access!=expected || !RaptorLakeP::ReferenceFB::portRecord(platform,config))return false;
 memcpy(&ddi,reinterpret_cast<void *>(config),4);
 return ddi==(config-platform-0x20)/24;
}
bool observerSelected(){return wexRplReferenceTCSSRequested() && checkKernelArgument("-wex-rplp-tcss-observe");}
uint32_t observedPortHPD(void *port){
 if(observerSelected()){
  uint32_t ddi;if(!observerPort(port,ddi))return 0;
  if(ddi){__sync_fetch_and_add(&portBindings[5],1);return 0;}
 }
 return portHPDOriginal?reinterpret_cast<uint32_t (*)(void *)>(portHPDOriginal)(port):0;
}
uint32_t observedPortAUX(void *port,uint32_t command,uint32_t *data,uint32_t bytes,bool retry){
 if(observerSelected() && !(auxProbePermit && auxProbePort==port && auxProbeThread==current_thread())){
  uint32_t ddi;if(!observerPort(port,ddi))return static_cast<uint32_t>(kIOReturnNotReady);
  if(ddi){__sync_fetch_and_add(&portBindings[6],1);return static_cast<uint32_t>(kIOReturnNotReady);}
 }
 using Run=uint32_t (*)(void *,uint32_t,uint32_t *,uint32_t,bool);
 return portAUXOriginal?reinterpret_cast<Run>(portAUXOriginal)(port,command,data,bytes,retry):static_cast<uint32_t>(kIOReturnNotReady);
}

mach_vm_address_t fbOriginal;
bool framebufferStart(IOService *,IOService *){return false;}
void publish(){if(target){target->setProperty("WhateverXeRPLPReferenceRedraw",redrawState,sizeof(redrawState));target->setProperty("WhateverXeRPLPReferenceBootProbe",bootProbe,sizeof(bootProbe));target->setProperty("WhateverXeRPLPReferenceScanoutGate",scanoutGate,sizeof(scanoutGate));target->setProperty("WhateverXeRPLPReferenceProbeGate",acceleratorProbe,sizeof(acceleratorProbe));target->setProperty("WhateverXeRPLPReferenceContext",contextState,sizeof(contextState));target->setProperty("WhateverXeRPLPShutdownGuardReady",shutdownQuery && contextTryRetain && contextLock && wexRplReferenceContextFreeOriginal);target->setProperty("WhateverXeRPLPReferenceBarrier",barrierState,sizeof(barrierState));target->setProperty("WhateverXeRPLPReference",state,sizeof(state));target->setProperty("WhateverXeRPLPReferenceStartup",startup,sizeof(startup));target->setProperty("WhateverXeRPLPReferencePanel",panelState,sizeof(panelState));target->setProperty("WhateverXeRPLPReferenceModules",moduleState,sizeof(moduleState));target->setProperty("WhateverXeRPLPReferenceFirmware",firmwareScan,sizeof(firmwareScan));target->setProperty("WhateverXeRPLPReferenceHandover",handover,sizeof(handover));target->setProperty("WhateverXeRPLPReferenceDisplayPipe",displayPipe,sizeof(displayPipe));}}
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
void ready(){RaptorLakeP::Control::update(state[1]&&state[2]&&state[3]&&state[5]&&state[8]&&(!wexRplReferencePanel() || (panelState[10] && shutdownQuery && contextTryRetain && contextLock && wexRplReferenceContextFreeOriginal))&&!state[7],state[4]);publish();}
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

void probeTCAux(){
 using namespace RaptorLakeP::ReferenceTCAux;
 Result result;
 if(auxReadEntry && getPortEntry && portBindings[1]==2 && !portBindings[2] && captureAllowed() && observerSelected()){
  auto port=reinterpret_cast<OSObject *(*)(uint32_t)>(getPortEntry)(5);
  uint32_t ddi=0;
  if(observerPort(port,ddi) && ddi==5){
   port->retain();auxProbePort=port;auxProbeThread=current_thread();
   struct IO {
    uint32_t read(uint32_t reg){return backend.readReg32(reg);}
    void write(uint32_t reg,uint32_t value){backend.writeReg32(reg,value);}
    void pause(){IOSleep(1);}
    uint32_t readDPCD(uint8_t *data,unsigned length){
     using Read=uint32_t (*)(void *,uint32_t,void *,uint32_t);
     __sync_synchronize();auxProbePermit=1;
     auto result=reinterpret_cast<Read>(auxReadEntry)(auxProbePort,0,data,length);
     auxProbePermit=0;__sync_synchronize();return result;
    }
   } io;
   result=probe(io,true);
   auxProbePermit=0;auxProbeThread=nullptr;auxProbePort=nullptr;port->release();
  }
 }
 if(target)target->setProperty("WhateverXeRPLPReferenceTCAUX",&result,sizeof(result));
}

void captureWorker(thread_call_param_t,thread_call_param_t){
 if(captureAllowed()){
  struct IO {
   bool readable(uint32_t reg){return !(reg&3) && reg<=0x1000000-4;}
   uint32_t read(uint32_t reg){return backend.readReg32(reg);}
  } io;
  auto sample=RaptorLakeP::ReferenceSnapshot::capture(io,++captureSequence);
  target->setProperty("WhateverXeRPLPReferenceSnapshot",&sample,sizeof(sample));
  if(sample.status==RaptorLakeP::ReferenceSnapshot::Complete && (sample.activePipes&1) && checkKernelArgument("-wex-rplp-tcss-aux-probe") && observerSelected() && __sync_bool_compare_and_swap(&auxProbeAttempted,0,1))probeTCAux();
  // Controller lifetime, private image and physical owner are pinned by
  // captureAllowed(). D40 is the same immutable platform table used by the
  // qualified compatibility-port route; never export its kernel pointers.
  const uint8_t *platform=nullptr;
  memcpy(&platform,reinterpret_cast<const uint8_t *>(framebufferOwner)+0xD40,sizeof(platform));
  struct PlatformView {const uint8_t *base;uint32_t read(unsigned offset){uint32_t value;memcpy(&value,base+offset,4);return value;}} view{platform};
  auto connectors=RaptorLakeP::ReferenceConnectors::platform(view,platform && !(reinterpret_cast<uintptr_t>(platform)&3));
  target->setProperty("WhateverXeRPLPReferenceConnectors",&connectors,sizeof(connectors));
  auto tc=RaptorLakeP::ReferenceConnectors::typeC(io,sample.status==RaptorLakeP::ReferenceSnapshot::Complete && sample.activePipes);
  target->setProperty("WhateverXeRPLPReferenceTypeC",&tc,sizeof(tc));
  target->setProperty("WhateverXeRPLPReferencePortModes",portModes,sizeof(portModes));
  target->setProperty("WhateverXeRPLPReferencePortObserver",portObserve,sizeof(portObserve));
  target->setProperty("WhateverXeRPLPReferencePortBindings",portBindings,sizeof(portBindings));
  if(checkKernelArgument("-wex-rplp-backlight")){
   auto pwm=RaptorLakeP::ReferenceBacklight::capture(io,sample.status==RaptorLakeP::ReferenceSnapshot::Complete && (sample.activePipes&1));
   target->setProperty("WhateverXeRPLPBacklightPWM",&pwm,sizeof(pwm));
  }
  target->setProperty("WhateverXeRPLPReferenceBarrier",barrierState,sizeof(barrierState));
  target->setProperty("WhateverXeRPLPReferenceContext",contextState,sizeof(contextState));
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
bool waitForNativeTimings(){
 for(unsigned elapsed=0;elapsed<=15000;elapsed+=25){
  ++bootProbe[1];
  const bool controllerStarted=__atomic_load_n(&handover[7],__ATOMIC_ACQUIRE);
  IOService *head=nullptr;
  if(framebufferOwner && panelState[3]==0x1C48 && controllerStarted && panelState[8])
   memcpy(&head,reinterpret_cast<uint8_t *>(framebufferOwner)+0xD60,8);
  if(head && head->getProvider()==target && head->getMetaClass()->getClassSize()==0x4B98){
   uint32_t mode;uintptr_t timings;
   memcpy(&mode,reinterpret_cast<uint8_t *>(head)+0x4250,4);
   memcpy(&timings,reinterpret_cast<uint8_t *>(head)+0x4260,8);
   if(timings && mode>=0x10000 && mode<0xFFFF0000){
    IODisplayModeInformation information{};
    bootProbe[7]=static_cast<IOFramebuffer *>(head)->getInformationForDisplayMode(mode,&information);
    if(bootProbe[7]==kIOReturnSuccess && information.nominalWidth && information.nominalHeight){bootProbe[2]=1;return true;}
   }
  }
  if(elapsed<15000)IOSleep(25);
 }
 return false;
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
 if(wexRplReferencePanel()){
  // A framebuffer may probe us before start() has created the collector.
  // Retry only after native start and real mode timing are ready, before
  // publishing the accelerator to WindowServer. No fixed activation delay.
  if(!waitForNativeTimings()){bootProbe[6]=1;startup[15]=11;publish();return;}
  bootProbe[3]=1;
  bootProbe[4]=static_cast<uint32_t>(nativeOwner->requestProbe(kIOFBUserRequestProbe));
  bootProbe[5]=displayPipeReadyMask;
  if(bootProbe[4] || !(displayPipeReadyMask&1)){bootProbe[6]=2;startup[15]=12;publish();return;}
 }
 nativeOwner->registerService(kIOServiceAsynchronous);startup[13]=1;startup[1]=7;state[6]=1;publish();
 if(monitor)thread_call_enter(monitor);
}

void kernel(void *,KernelPatcher &p){
 if(!wexRplReferenceValidate())return;
 if(wexRplReferencePanel()){
  auto query=p.solveSymbol(KernelPatcher::KernelID,"_get_system_inshutdown");p.clearError();
  auto flag=p.solveSymbol(KernelPatcher::KernelID,"_system_inshutdown");p.clearError();
  if(query && flag && RaptorLakeP::ReferenceShutdown::getter(reinterpret_cast<const uint8_t *>(query),12,query,flag))shutdownQuery=reinterpret_cast<ShutdownQuery>(query);
  auto retain=p.solveSymbol(KernelPatcher::KernelID,"__ZNK8OSObject15taggedTryRetainEPKv");p.clearError();
  const uint8_t retainPrefix[]{0x55,0x48,0x89,0xE5};
  if(retain && !memcmp(reinterpret_cast<void *>(retain),retainPrefix,4))contextTryRetain=reinterpret_cast<TryRetain>(retain);
  if(!shutdownQuery || !contextTryRetain || !contextLock){state[7]=9;ready();return;}
  auto address=p.solveSymbol(KernelPatcher::KernelID,"__ZN6OSKext22loadKextWithIdentifierEPKcbbhhP7OSArray");p.clearError();
  const uint8_t prefix[]{0x55,0x48,0x89,0xE5};
  if(address && !memcmp(reinterpret_cast<void *>(address),prefix,sizeof(prefix)))moduleLoader=reinterpret_cast<ModuleLoader>(address);
 }
 XeDataAccess::processKernel(p);
}
bool backlightSetDisplay(IOService *panel,IOService *display) {
 if(!backlightOriginal)return false;
 // Add only the missing UID-19 profile for an actual internal 16-bit PWM
 // display. Keep Apple's/custom profiles and the native selection algorithm.
 // No string patch, product-ID list, ACPI path or model-specific table.
 auto provider=panel ? panel->getProvider() : nullptr;
 auto uid=provider ? OSDynamicCast(OSString,provider->getProperty("_UID")) : nullptr;
 auto product=display ? OSDynamicCast(OSNumber,display->getProperty("DisplayProductID")) : nullptr;
 bool owned=false;
 IOFramebuffer *framebuffer=nullptr;
 const bool enabled=wexRplReferencePanel() && wexRplReferenceArmed() && checkKernelArgument("-wex-rplp-backlight");
 for(auto entry=enabled ? display : nullptr;entry;entry=entry->getProvider()) {
  // IOGraphicsFamily is loaded later than this Lilu plugin. A direct
  // IOFramebuffer::metaClass import is not available in our dependency set.
  if(!framebuffer)framebuffer=static_cast<IOFramebuffer *>(entry->metaCast("IOFramebuffer"));
  if(entry==target){owned=true;break;}
 }
 // setDisplay precedes publication of IODisplayParameters. Query the same
 // three-word (current,min,max) attribute as AppleIntelPanelA itself.
 uintptr_t pwm[3] {};
 const bool internal=display && display->metaCast("AppleBacklightDisplay");
 const bool range=owned && internal && framebuffer &&
  framebuffer->getAttributeForConnection(0,'bklt',pwm)==kIOReturnSuccess;
 OSDictionary *heldProfiles=nullptr;
 if(range && uid && product && pwm[1]<=UINT32_MAX && pwm[2]<=UINT32_MAX &&
    XeBacklightProfile::eligible(uid->isEqualTo("19") ? 19 : 0,pwm[1],pwm[2],internal)) {
  auto original=OSDynamicCast(OSDictionary,panel->getProperty("ApplePanels"));
  if(original && !panel->getProperty("ApplePanel")) {
   char key[32];snprintf(key,sizeof(key),"F19T%04x",product->unsigned32BitValue());
   if(!original->getObject(key) && !original->getObject("F19")) {
    auto copy=OSDictionary::withDictionary(original);
    auto curve=OSData::withBytes(XeBacklightProfile::curve,sizeof(XeBacklightProfile::curve));
    if(copy && curve && copy->setObject(key,curve) && panel->setProperty("ApplePanels",copy)) {
     // Native removes ApplePanels before retaining its selected OSData in
     // ApplePanel. Keep the candidate dictionary alive across that handoff.
     heldProfiles=copy;
     panel->setProperty("WhateverXeRPLPBacklightProfile",key);
    }
    if(curve)curve->release();
    if(copy && !heldProfiles)copy->release();
   }
  }
 }
 uint32_t inputs[]={1,owned,uid && uid->isEqualTo("19"),range ? static_cast<uint32_t>(pwm[1]) : ~0U,
  range ? static_cast<uint32_t>(pwm[2]) : 0,0};
 const bool result=backlightOriginal ? FunctionCast(backlightSetDisplay,backlightOriginal)(panel,display) : false;
 if(heldProfiles)heldProfiles->release();
 if(panel) {
  inputs[5]=result;
  panel->setProperty("WhateverXeRPLPBacklightMatch",inputs,sizeof(inputs));
  if(target)target->setProperty("WhateverXeRPLPBacklightMatch",inputs,sizeof(inputs));
 }
 return result;
}
void backlightKext(void *,KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 if(i!=backlightInfo.loadIndex || !wexRplReferenceValidate() || !wexRplReferencePanel() || !checkKernelArgument("-wex-rplp-backlight"))return;
 const char *name="__ZN15AppleIntelPanel10setDisplayEP9IODisplay";
 bool ok=RaptorLakeP::ReferenceBacklight::image(reinterpret_cast<void *>(a),n);
 auto entry=ok ? p.solveSymbol(i,name,a,n) : 0;p.clearError();
 const uint8_t prefix[]{0x55,0x48,0x89,0xE5};
 ok=ok && entry>=a && n>=sizeof(prefix) && entry-a<=n-sizeof(prefix) && !memcmp(reinterpret_cast<void *>(entry),prefix,sizeof(prefix));
 if(ok){KernelPatcher::RouteRequest route{name,backlightSetDisplay,backlightOriginal};ok=p.routeMultiple(i,&route,1,a,n);p.clearError();}
 if(target)target->setProperty("WhateverXeRPLPBacklightProfileReady",ok);
}
void kext(void *,KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 if(!state[1])return;
 XeDataAccess::processKext(p,i,a,n);
 if(wexRplReferencePanel() && i==compatInfo.loadIndex){
  const char *name="__ZN17AppleIntelPortHAL13probePortModeEv";
  const uint8_t prefix[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xEC,0x10};
  bool ok=RaptorLakeP::ReferenceFB::compatImage(reinterpret_cast<void *>(a),n);
  auto entry=ok?p.solveSymbol(i,name,a,n):0;p.clearError();
  compatPortEntry=entry;
  ok=ok && entry>=a && n>=sizeof(prefix) && entry-a<=n-sizeof(prefix) && !memcmp(reinterpret_cast<void *>(entry),prefix,sizeof(prefix));
  if(ok){KernelPatcher::RouteRequest route{name,compatPortMode,compatPortOriginal};ok=p.routeMultiple(i,&route,1,a,n);p.clearError();}
  panelState[10]=ok;
  if(!ok)state[7]=7;
  ready();
 }
 if(i==fbInfo.loadIndex && !wexRplReferencePanel()){
  if(!wexRplReferenceImage(reinterpret_cast<void *>(a),n,false)){state[7]=3;ready();return;}
  const char *name="__ZN31AppleIntelFramebufferController5startEP9IOService";
  const auto entry=p.solveSymbol(i,name,a,n);p.clearError();
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
 __atomic_store_n(&handover[7],uint64_t(ok),__ATOMIC_RELEASE);if(!ok){panelState[15]=7;handover[6]=7;}publish();
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
 displayPipe[5]=ok;if(!ok)displayPipe[7]=2;
 if(ok && index<3)__sync_fetch_and_or(&displayPipeReadyMask,1U<<index);
 publish();return ok;
}
void wexRplReferenceFBFailure(unsigned code){__sync_bool_compare_and_swap(&panelState[15],0,code);}
uint32_t wexRplReferenceFBRead(unsigned long reg){return backend.readReg32(reg);}



bool wexRplReferenceAcceleratorProbeHooks(KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 if(!wexRplReferencePanel())return true;
 const char *name="__ZN16IntelAccelerator12requestProbeEj";
 bool ok=wexRplReferenceImage(reinterpret_cast<void *>(a),n,true);
 auto entry=ok?p.solveSymbol(i,name,a,n):0;p.clearError();
 const uint8_t prefix[]{0x55,0x48,0x89,0xE5,0x41,0x56,0x53,0x48,0x83,0xEC,0x10,0x49,0x89,0xFE};
 ok=ok && n>=0x26FEC+sizeof(prefix) && entry==a+0x26FEC && !memcmp(reinterpret_cast<void *>(entry),prefix,sizeof(prefix));
 if(ok){KernelPatcher::RouteRequest route{name,guardedAcceleratorProbe,acceleratorProbeOriginal};ok=p.routeMultiple(i,&route,1,a,n);p.clearError();}
 acceleratorProbe[1]=ok;acceleratorProbe[6]=ok?0:1;
 if(target)target->setProperty("WhateverXeRPLPReferenceProbeGate",acceleratorProbe,sizeof(acceleratorProbe));
 const char *allocate="__ZN18IGAccelDisplayPipe21allocateScanoutMemoryEP19IntelScaledModeDataj";
 auto allocationEntry=ok?p.solveSymbol(i,allocate,a,n):0;p.clearError();
 const uint8_t allocationPrefix[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC,0x18};
 ok=ok && n>=0x7E5B2+sizeof(allocationPrefix) && allocationEntry==a+0x7E5B2 && !memcmp(reinterpret_cast<void *>(allocationEntry),allocationPrefix,sizeof(allocationPrefix));
 if(ok){KernelPatcher::RouteRequest route{allocate,guardedScanoutMemory,scanoutMemoryOriginal};ok=p.routeMultiple(i,&route,1,a,n);p.clearError();}
 scanoutGate[1]=ok;scanoutGate[6]=ok?0:1;
 if(target)target->setProperty("WhateverXeRPLPReferenceScanoutGate",scanoutGate,sizeof(scanoutGate));
 return ok;
}

bool wexRplReferencePortHooks(KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 if(!wexRplReferenceTCSSSelected() || !checkKernelArgument("-wex-rplp-tcss-observe"))return true;
 bool ok=wexRplReferenceImage(reinterpret_cast<void *>(a),n,false) && panelState[10] && compatPortEntry;
 pureVirtualEntry=p.solveSymbol(KernelPatcher::KernelID,"___cxa_pure_virtual");p.clearError();
 const char *names[]{"__ZN14AppleIntelPort14probePortStateEv","__ZN17AppleIntelPortHAL11getHPDStateEv","__ZN17AppleIntelPortHAL13runAUXCommandEN14AppleIntelPort11AUX_CommandEPjjb"};
 const uint8_t shortPrefix[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x53,0x48,0x83,0xEC};
 const uint8_t auxPrefix[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC,0x78};
 const uint32_t offsets[]{0x2E90A,0x91836,0x91B64};
 for(unsigned x=0;ok && x<3;++x){
  auto entry=p.solveSymbol(i,names[x],a,n);p.clearError();
  auto prefix=x==2?auxPrefix:shortPrefix;size_t length=x==2?sizeof(auxPrefix):sizeof(shortPrefix);
  ok=entry==a+offsets[x] && n>=offsets[x]+length && !memcmp(reinterpret_cast<void *>(entry),prefix,length);
 }

 if(ok && checkKernelArgument("-wex-rplp-tcss-aux-probe")){
  auxReadEntry=p.solveSymbol(i,"__ZN14AppleIntelPort7readAUXEjPvj",a,n);p.clearError();
  getPortEntry=p.solveSymbol(i,"__ZN14AppleIntelPort12getPortByDDIEj",a,n);p.clearError();
  ok=n>0x28D9E + sizeof(auxPrefix) && auxReadEntry==a+0x28D9E && getPortEntry==a+0x25AB2 &&
   !memcmp(reinterpret_cast<void *>(auxReadEntry),auxPrefix,sizeof(auxPrefix)) &&
   !memcmp(reinterpret_cast<void *>(getPortEntry),auxPrefix,4);
 }
 const char *tables[]{"__ZTV17AppleIntelPortHAL","__ZTV22AppleIntelPortHALDiags"};
 const uint32_t tableOffsets[]{0x13CDA8,0x1415D8};
 mach_vm_address_t slots[2]{};
 // Exact private tables only. Refuse a foreign binding; never patch the kernel's
 // shared pure-virtual function or unrelated classes.
 for(unsigned x=0;ok && x<2;++x){
  auto table=p.solveSymbol(i,tables[x],a,n);p.clearError();
  ok=table==a+tableOffsets[x] && n>=tableOffsets[x]+16+0x190+8 && pureVirtualEntry;
  if(ok){
   slots[x]=table+16+0x190;mach_vm_address_t bound;memcpy(&bound,reinterpret_cast<void *>(slots[x]),8);
   portBindings[3+x]=bound==pureVirtualEntry?2:bound==compatPortEntry?1:3;
   ok=bound==pureVirtualEntry || bound==compatPortEntry;
  }
 }
 if(ok){
  portImageBase=a;portImageSize=n;
  KernelPatcher::RouteRequest routes[]{
   {names[0],observedPortState,portStateOriginal},
   {names[1],observedPortHPD,portHPDOriginal},
   {names[2],observedPortAUX,portAUXOriginal}};
  ok=p.routeMultiple(i,routes,3,a,n);p.clearError();
 }
 for(unsigned x=0;ok && x<2;++x){
  auto replacement=reinterpret_cast<mach_vm_address_t>(compatPortMode);
  p.routeBlock(slots[x],reinterpret_cast<const uint8_t *>(&replacement),sizeof(replacement));
  mach_vm_address_t after;memcpy(&after,reinterpret_cast<void *>(slots[x]),8);
  ok=p.getError()==KernelPatcher::Error::NoError && after==replacement;p.clearError();
  if(ok)++portBindings[1];
 }
 portObserve[1]=ok;portObserve[2]=ok?0:1;portBindings[2]=ok?0:1;
 if(target){target->setProperty("WhateverXeRPLPReferencePortObserver",portObserve,sizeof(portObserve));target->setProperty("WhateverXeRPLPReferencePortBindings",portBindings,sizeof(portBindings));}
 return ok;
}

bool wexRplReferenceValidate(){
 if(state[1])return !state[7];
 if(state[7] || !RaptorLakeP::Reference::enabled())return false;
 auto info=DeviceInfo::create();if(!info){SYSLOG("rplp","reference admission failed: DeviceInfo unavailable");state[7]=1;return false;}
 auto pci=OSDynamicCast(IOPCIDevice,info->videoBuiltin);
 const bool published=pci && WIOKit::awaitPublishing(pci);
 const auto profile=published?XELP::deviceProfile(pci):XELP::Profile{};
 bool valid=published && RaptorLakeP::eligible(profile,XELP::kernelOS(version),true);
 auto aperture=valid?pci->getDeviceMemoryWithRegister(0x18):nullptr;
 const uint16_t ggc=aperture?pci->configRead16(0x50):UINT16_MAX;
 const uint32_t bsmLow=aperture?pci->configRead32(0xC0):UINT32_MAX;
 const uint32_t bsmHigh=aperture?pci->configRead32(0xC4):UINT32_MAX;
 auto layout=aperture?XELPMemory::decode(ggc,bsmLow,bsmHigh,aperture->getLength()):XELPMemory::Layout{};
 valid=valid && RaptorLakeP::Reference::physical(profile,XELP::kernelOS(version),layout);
 // Admission is fail-closed. Preserve enough read-only proof to diagnose a
 // rejected boot even when no target/control service can be published.
 uint64_t admission[]{1,pci!=nullptr,published,profile.identity,profile.revision,
  ggc,bsmLow,bsmHigh,layout.valid,layout.bytes,layout.gttBytes,layout.apertureBytes,valid};
 if(auto registry=IORegistryEntry::getRegistryRoot())registry->setProperty("WhateverXeRPLPAdmission",admission,sizeof(admission));
 if(pci)pci->setProperty("WhateverXeRPLPAdmission",admission,sizeof(admission));
 SYSLOG("rplp","reference admission pci=%u published=%u id=%04X rev=%02X ggc=%04X bsm=%08X:%08X valid=%u stolen=%llX gtt=%llX aperture=%llX",
  pci!=nullptr,published,profile.identity,profile.revision,ggc,bsmHigh,bsmLow,valid,
  static_cast<unsigned long long>(layout.bytes),static_cast<unsigned long long>(layout.gttBytes),static_cast<unsigned long long>(layout.apertureBytes));
 if(valid){
  target=pci;target->retain();state[1]=1;
  // Record immutable physical proof BEFORE installing a registry/PCI presentation.
  uint32_t physical[]{0xA7A08086,4};target->setProperty("WhateverXeRPLPReferencePhysical",physical,sizeof(physical));
  if(!RaptorLakeP::Control::create(target)){state[7]=2;state[1]=0;}
 }else state[7]=1;
 DeviceInfo::deleter(info);publish();return state[1] && !state[7];
}
bool wexRplReferenceArmed(){return state[1] && state[4] && !state[7];}
bool wexRplReferenceTCSSSelected(){
 return wexRplReferencePanel() && state[1] && !state[7] && checkKernelArgument("-wex-rplp-tcss");
}
bool wexRplReferenceTCSSRequested(){
 return wexRplReferenceTCSSSelected() && state[4];
}
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
void wexRplReferenceCacheRCSContext(void *object){
 if(!object || !contextLock)return;
 ContextIO io;contextCache.store(io,object);__sync_fetch_and_add(&contextState[1],1);
}
void *wexRplReferenceAcquireRCSContext(){
 if(!contextLock || !contextTryRetain || !shutdownQuery)return nullptr;
 if(shutdownQuery()){__sync_fetch_and_add(&contextState[6],1);return nullptr;}
 ContextIO io;auto object=contextCache.acquire(io);
 if(object)__sync_fetch_and_add(&contextState[2],1);return object;
}
void wexRplReferenceReleaseRCSContext(void *object){
 if(!object)return;__sync_fetch_and_add(&contextState[5],1);static_cast<OSObject *>(object)->release();
}
void wexRplReferenceContextFree(void *object){
 if(contextLock){ContextIO io;if(contextCache.forget(io,object))__sync_fetch_and_add(&contextState[4],1);}
 __sync_fetch_and_add(&contextState[7],1);
 reinterpret_cast<void (*)(void *)>(wexRplReferenceContextFreeOriginal)(object);
}
bool wexRplReferenceRecoveryRequested(){return wexRplReferencePanel() && state[1] && !state[7] && checkKernelArgument("-wex-rplp-gpu-recovery");}
bool wexRplReferenceForceWakeRequested(){return wexRplReferencePanel() && state[1] && !state[7] && checkKernelArgument("-wex-rplp-forcewake-balance");}
bool wexRplReferenceForceWakeOwner(const void *owner){
 return wexRplReferenceForceWakeRequested() && state[4] && owner && owner==nativeOwner && shutdownQuery && !shutdownQuery();
}
bool wexRplReferenceRuntimeOwner(const void *owner){
 return wexRplReferencePanel() && state[1] && state[4] && state[6] && !state[7] && owner && owner==nativeOwner &&
  shutdownQuery && !shutdownQuery() && !nativeOwner->isInactive();
}
bool wexRplReferenceBarrierOwner(const void *owner){
 return checkKernelArgument("-wex-rplp-native-barriers") && wexRplReferencePanel() && state[1] && state[4] &&
  state[6] && !state[7] && shutdownQuery && !shutdownQuery() && nativeOwner && owner==nativeOwner && !nativeOwner->isInactive();
}
void wexRplReferenceBarrierResult(unsigned reason,unsigned result){
 __sync_fetch_and_add(&barrierState[1],1);
 __sync_fetch_and_add(&barrierState[reason?3:2],1);
 barrierState[4]=reason;barrierState[5]=result;
 if(reason)barrierState[7]=reason;
 if(!reason && !result)__sync_fetch_and_add(&barrierState[6],1);
}
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
 SYSLOG("rplp","reference backend initialization panel=%u early=%u",wexRplReferencePanel(),earlyRequested());
 if(wexRplReferencePanel())contextLock=IOSimpleLockAlloc();
 monitor=thread_call_allocate(monitorWorker,nullptr);
 if(wexRplReferencePanel())captureWork=thread_call_allocate(captureWorker,nullptr);
 if(wexRplReferencePanel())moduleWork=thread_call_allocate(moduleWorker,nullptr);
 if(wexRplReferencePanel())framebufferWork=thread_call_allocate(framebufferWorker,nullptr);
 if(wexRplReferencePanel() && earlyRequested())earlyPublishWork=thread_call_allocate(earlyPublish,nullptr);
 work=thread_call_allocate(worker,nullptr);if(!work || !monitor || (wexRplReferencePanel() && (!framebufferWork || !captureWork))){state[7]=4;return;}
 lilu.onPatcherLoadForce(kernel,nullptr);
 if(wexRplReferencePanel() && earlyRequested())lilu.onKextLoadForce(&firmwareInfo,1,firmwareKext,nullptr);
 if(wexRplReferencePanel() && checkKernelArgument("-wex-rplp-backlight"))lilu.onKextLoadForce(&backlightInfo,1,backlightKext,nullptr);
 if(wexRplReferencePanel() && redrawRequested())lilu.onKextLoadForce(&redrawInfo,1,redrawKext,nullptr);
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
