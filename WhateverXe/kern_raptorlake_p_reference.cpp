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
#include "kern_raptorlake_p_reference_tc_live.hpp"
#include "kern_display13_tc.hpp"
#include "kern_display13_power.hpp"
#include "kern_raptorlake_p_training_route.hpp"
#include "kern_raptorlake_p_reference_voltage.hpp"
#include "kern_raptorlake_p_reference_ddi.hpp"
#include <kern/thread.h>
#include <IOKit/graphics/IOFramebuffer.h>
#include "kern_raptorlake_p_reference_shutdown.hpp"
#include "kern_raptorlake_p_reference_nvram.hpp"
#include <IOKit/IOLocks.h>
#include <IOKit/IOWorkLoop.h>
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
thread_call_t captureWork,nativeRescanWork;
mach_vm_address_t nativeHotplugEntry;
uint32_t nativeRescanBusy,nativeRescanUsed,nativeRescanState[8]{1};
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

bool observerPort(void *,uint32_t &);
uint32_t nativeAuxBypassRefs(void *port){
 uint32_t ddi=~0U;
 if(!observerPort(port,ddi) || ddi!=5)return ~0U;
 OSObject *power=nullptr;memcpy(&power,reinterpret_cast<uint8_t *>(framebufferOwner)+0xC8,8);
 if(!power || power->getMetaClass()->getClassSize()!=0x80 ||
  strcmp(power->getMetaClass()->getClassName(),"AppleIntelPowerWell"))return ~0U;
 IOService *owner=nullptr;memcpy(&owner,reinterpret_cast<uint8_t *>(power)+0x10,8);
 uint8_t alwaysOn=0;memcpy(&alwaysOn,reinterpret_cast<uint8_t *>(power)+0x18,1);
 if(owner!=framebufferOwner || alwaysOn!=1)return ~0U;
 uint32_t refs;memcpy(&refs,reinterpret_cast<uint8_t *>(power)+0x68,4);return refs;
}

// Optional TC3 training remains closed until a privileged, successful AUX
// preflight. The private TGL mode routine must not write modular-FIA registers
// for an ADL-P/RPL-P Type-C PHY.
bool observerPort(void *,uint32_t &);
IORecursiveLock *livePortLock;
RaptorLakeP::ReferenceTCLive::Lease livePortLease;
mach_vm_address_t livePortOriginal,liveBaseMode,liveAuxEnable,liveAuxDisable;
uint32_t livePortReady,livePortPermission,livePortState[16]{1};
uint32_t livePhyTrace[16]{2,0,0,0,0,0,0,0,~0U,0,0,0};
// Captured under livePortLock. Retain the first programming sequence and the
// latest value for every allowed register; no extra hardware reads are added.
uint32_t livePhyRegisters[22][8]{},livePhyWrites[64][8]{},livePhyWriteCount;
mach_vm_address_t liveLaneMaskOriginal,liveSetLanesOriginal;
uint32_t liveLaneState[12]{1},liveAuxTrace[256][8]{},liveAuxTraceCount;
struct LivePortIO {
 void *port;
 uint32_t read(uint32_t reg){return backend.readReg32(reg);}
 void write(uint32_t reg,uint32_t value){backend.writeReg32(reg,value);}
 void pause(){IOSleep(1);}
 uint32_t baseMode(uint32_t mode){return reinterpret_cast<uint32_t (*)(void *,uint32_t)>(liveBaseMode)(port,mode);}
 void auxEnable(){reinterpret_cast<void (*)(void *)>(liveAuxEnable)(port);}
 void auxDisable(){reinterpret_cast<void (*)(void *)>(liveAuxDisable)(port);}
 uint32_t auxBypassRefs(){return nativeAuxBypassRefs(port);}
};
bool livePortSelected(void *port){
 if(!__atomic_load_n(&livePortReady,__ATOMIC_ACQUIRE) || !__atomic_load_n(&livePortPermission,__ATOMIC_ACQUIRE) || !livePortLock)return false;
 uint32_t ddi=~0U;return observerPort(port,ddi) && ddi==5;
}
void publishLivePort(){
 livePortState[1]=__atomic_load_n(&livePortReady,__ATOMIC_ACQUIRE);livePortState[2]=__atomic_load_n(&livePortPermission,__ATOMIC_ACQUIRE);
 livePortState[9]=livePortLease.parent;livePortState[10]=livePortLease.phy;
 livePortState[11]=livePortLease.aux;livePortState[12]=livePortLease.active;
 livePortState[13]=livePortLease.nativeResult;livePortState[14]=livePortLease.writes;
 if(target)target->setProperty("WhateverXeRPLPReferenceTCLive",livePortState,sizeof(livePortState));
 if(target){
  target->setProperty("WhateverXeRPLPReferenceTCLanes",liveLaneState,sizeof(liveLaneState));
  target->setProperty("WhateverXeRPLPReferenceTCAuxTrace",liveAuxTrace,liveAuxTraceCount*sizeof(liveAuxTrace[0]));
  target->setProperty("WhateverXeRPLPReferenceTCPhy",livePhyTrace,sizeof(livePhyTrace));
  target->setProperty("WhateverXeRPLPReferenceTCPhyRegisters",livePhyRegisters,sizeof(livePhyRegisters));
  target->setProperty("WhateverXeRPLPReferenceTCPhyWrites",livePhyWrites,livePhyWriteCount*sizeof(livePhyWrites[0]));
 }
}
uint32_t livePortMode(void *port,uint32_t mode){
 if(!livePortSelected(port))return livePortOriginal?reinterpret_cast<uint32_t (*)(void *,uint32_t)>(livePortOriginal)(port,mode):static_cast<uint32_t>(kIOReturnNotReady);
 if(mode!=0 && mode!=3)return static_cast<uint32_t>(kIOReturnUnsupported);
 IORecursiveLockLock(livePortLock);
 ++livePortState[3];LivePortIO io{port};
 using namespace RaptorLakeP::ReferenceTCLive;
 Status result;
 if(mode==3){++livePortState[4];result=livePortLease.acquire(io,reinterpret_cast<uintptr_t>(framebufferOwner),reinterpret_cast<uintptr_t>(port),true);}
 else {++livePortState[5];if(livePortLease.owner)result=livePortLease.release(io,reinterpret_cast<uintptr_t>(framebufferOwner),reinterpret_cast<uintptr_t>(port));else {livePortLease.nativeResult=io.baseMode(0);result=livePortLease.nativeResult?Status::BaseMode:Status::Ready;}}
 livePortState[8]=static_cast<uint32_t>(result);publishLivePort();
 IORecursiveLockUnlock(livePortLock);
 return result==Status::Ready?static_cast<uint32_t>(kIOReturnSuccess):static_cast<uint32_t>(kIOReturnNotReady);
}
uint32_t livePortAUX(void *port,uint32_t command,uint32_t *data,uint32_t bytes,bool retry){
 if(!livePortSelected(port))return static_cast<uint32_t>(kIOReturnNotReady);
 IORecursiveLockLock(livePortLock);LivePortIO io{port};
 const bool ready=livePortLease.witness(io,reinterpret_cast<uintptr_t>(framebufferOwner),reinterpret_cast<uintptr_t>(port));
 uint32_t result=static_cast<uint32_t>(kIOReturnNotReady),request=0;
 if(data && bytes)memcpy(&request,data,bytes<4?bytes:4);
 if(ready && portAUXOriginal){++livePortState[6];result=reinterpret_cast<uint32_t (*)(void *,uint32_t,uint32_t *,uint32_t,bool)>(portAUXOriginal)(port,command,data,bytes,retry);}
 else ++livePortState[7];
 if(liveAuxTraceCount<256){
  auto &row=liveAuxTrace[liveAuxTraceCount];row[0]=liveAuxTraceCount+1;row[1]=command;row[2]=bytes;row[3]=retry;
  row[4]=request;row[5]=0;if(data && bytes)memcpy(&row[5],data,bytes<4?bytes:4);
  row[6]=result;row[7]=ready;++liveAuxTraceCount;
 }
 IORecursiveLockUnlock(livePortLock);return result;
}

IOSimpleLock *livePhyLock;
mach_vm_address_t liveReadPhyOriginal,liveWritePhyOriginal;
uint32_t livePhyFault;
thread_t liveVoltageThread;void *liveVoltagePort;
struct LivePhyIO {
 void *port;
 bool powerHeld(){LivePortIO io{port};return !livePhyFault && livePortLease.witness(io,reinterpret_cast<uintptr_t>(framebufferOwner),reinterpret_cast<uintptr_t>(port));}
 bool readable(uint32_t reg){return !(reg&3) && reg<=0x1000000-4;}
 uint32_t read(uint32_t reg){return backend.readReg32(reg);}
 void write(uint32_t reg,uint32_t value){backend.writeReg32(reg,value);}
 IOInterruptState lock(){return IOSimpleLockLockDisableInterrupt(livePhyLock);}
 void unlock(IOInterruptState state){IOSimpleLockUnlockEnableInterrupt(livePhyLock,state);}
 void bankFault(){livePhyFault=1;++livePortState[15];}
};
// ADLP always uses modular FIA: TC3 belongs to FIA2, slot 0. Native TGL
// getters/setters instead use FIA1 slot 2. Keep the paired correction scoped
// to the physically owned TC3 lease; preserve the adjacent port's fields.
uint32_t livePortLaneMask(void *port){
 if(!livePortSelected(port))return liveLaneMaskOriginal?reinterpret_cast<uint32_t (*)(void *)>(liveLaneMaskOriginal)(port):0;
 IORecursiveLockLock(livePortLock);++liveLaneState[1];LivePortIO io{port};uint32_t mask=0;
 if(livePortLease.witness(io,reinterpret_cast<uintptr_t>(framebufferOwner),reinterpret_cast<uintptr_t>(port))){
  auto raw=io.read(0x16E8A0);liveLaneState[3]=raw;
  if(raw!=~0U){auto value=raw&15U;if(value==1 || value==2 || value==4 || value==8 || value==3 || value==12 || value==15)mask=value;}
 }
 liveLaneState[4]=mask;if(!mask)++liveLaneState[2];IORecursiveLockUnlock(livePortLock);return mask;
}
void livePortSetLanes(void *port,uint32_t lanes){
 if(!livePortSelected(port)){if(liveSetLanesOriginal)reinterpret_cast<void (*)(void *,uint32_t)>(liveSetLanesOriginal)(port,lanes);return;}
 IORecursiveLockLock(livePortLock);++liveLaneState[5];liveLaneState[6]=lanes;
 auto mask=livePortLaneMask(port);unsigned capacity=mask==15?4:(mask==3 || mask==12)?2:mask?1:0;
 LivePortIO io{port};auto buffer=capacity?io.read(0x64500):~0U;
 // DP-alt lane reversal is invalid in Linux's ADLP programming contract.
 if((lanes!=1 && lanes!=2 && lanes!=4) || lanes>capacity || buffer==~0U || (buffer&(1U<<16))){++liveLaneState[7];IORecursiveLockUnlock(livePortLock);return;}
 const auto irq=IOSimpleLockLockDisableInterrupt(livePhyLock);
 auto before=io.read(0x16E8C0);liveLaneState[8]=before;
 if(before!=~0U){
  auto desired=(before&~15U)|((1U<<lanes)-1);io.write(0x16E8C0,desired);++liveLaneState[10];
  auto after=io.read(0x16E8C0);liveLaneState[9]=after;if(after!=desired)++liveLaneState[11];
 }else ++liveLaneState[7];
 IOSimpleLockUnlockEnableInterrupt(livePhyLock,irq);IORecursiveLockUnlock(livePortLock);
}

void recordPhyAccess(uint32_t index,uint32_t raw,const Display13TC::Result &result,bool write,uint32_t requested){
 ++livePhyTrace[write?2:1];livePhyTrace[3]=index;livePhyTrace[4]=raw;
 livePhyTrace[5]=static_cast<uint32_t>(result.status);livePhyTrace[6]=result.before;livePhyTrace[7]=result.after;
 if(result.status!=Display13TC::Status::Complete && livePhyTrace[8]==~0U){
  livePhyTrace[8]=index;livePhyTrace[9]=raw;livePhyTrace[10]=static_cast<uint32_t>(result.status);
  livePhyTrace[12]=result.before;livePhyTrace[13]=result.after;livePhyTrace[14]=requested;livePhyTrace[15]=write;
 }
 if(index<0x34 || index>0x49)return;
 auto &row=livePhyRegisters[index-0x34];row[0]=index;row[1]=raw;++row[write?3:2];
 if(write)row[4]=requested;
 row[5]=result.before;row[6]=result.after;row[7]=static_cast<uint32_t>(result.status);
 if(write && livePhyWriteCount<64){
  auto &entry=livePhyWrites[livePhyWriteCount];entry[0]=livePhyWriteCount+1;entry[1]=index;entry[2]=raw;
  entry[3]=requested;entry[4]=result.before;entry[5]=result.after;entry[6]=static_cast<uint32_t>(result.status);entry[7]=result.writes;
  ++livePhyWriteCount;
 }
}
uint32_t livePortReadPhy(void *port,uint32_t index){
 if(!livePortSelected(port))return liveReadPhyOriginal?reinterpret_cast<uint32_t (*)(void *,uint32_t)>(liveReadPhyOriginal)(port,index):0;
 if(index<0x34 || index>0x49){__sync_fetch_and_add(&livePhyTrace[11],1);return 0;}
 IORecursiveLockLock(livePortLock);uint32_t raw=0,value=0;
 memcpy(&raw,static_cast<uint8_t *>(port)+0x580+4*index,4);
 Display13TC::Port coordinates;Display13TC::Port::resolve(5,coordinates);LivePhyIO io{port};
 const auto result=Display13TC::Dekel::access(io,coordinates,index,raw,false,value);
 recordPhyAccess(index,raw,result,false,0);
 if(result.status!=Display13TC::Status::Complete){value=0;++livePortState[7];}
 IORecursiveLockUnlock(livePortLock);return value;
}
void livePortWritePhy(void *port,uint32_t index,uint32_t value){
 if(!livePortSelected(port)){if(liveWritePhyOriginal)reinterpret_cast<void (*)(void *,uint32_t,uint32_t)>(liveWritePhyOriginal)(port,index,value);return;}
 if(index<0x34 || index>0x49){__sync_fetch_and_add(&livePhyTrace[11],1);return;}
 IORecursiveLockLock(livePortLock);uint32_t raw=0;
 memcpy(&raw,static_cast<uint8_t *>(port)+0x580+4*index,4);
 Display13TC::Port coordinates;Display13TC::Port::resolve(5,coordinates);LivePhyIO io{port};
 if(liveVoltageThread==current_thread() && liveVoltagePort==port && (index==0x44 || index==0x47))value=RaptorLakeP::ReferenceVoltage::dpControl(value);
 const auto requested=value;
 const auto result=Display13TC::Dekel::access(io,coordinates,index,raw,true,value);
 recordPhyAccess(index,raw,result,true,requested);
 if(result.status!=Display13TC::Status::Complete)++livePortState[7];
 IORecursiveLockUnlock(livePortLock);
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
 if(livePortSelected(port))return portStateOriginal?reinterpret_cast<uint32_t (*)(void *)>(portStateOriginal)(port):0;
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
  if(ddi && !livePortSelected(port)){__sync_fetch_and_add(&portBindings[5],1);return 0;}
 }
 return portHPDOriginal?reinterpret_cast<uint32_t (*)(void *)>(portHPDOriginal)(port):0;
}
uint32_t observedPortAUX(void *port,uint32_t command,uint32_t *data,uint32_t bytes,bool retry){
 if(observerSelected() && !(auxProbePermit && auxProbePort==port && auxProbeThread==current_thread())){
  uint32_t ddi;if(!observerPort(port,ddi))return static_cast<uint32_t>(kIOReturnNotReady);
  if(ddi){if(livePortSelected(port))return livePortAUX(port,command,data,bytes,retry);__sync_fetch_and_add(&portBindings[6],1);return static_cast<uint32_t>(kIOReturnNotReady);}
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

// Root-triggered TC3 rescan. The ordinary donor connectionProbe deliberately
// ignores external heads. Keep this native controller action on its workloop;
// no arbitrary connector, mode, function pointer or register is user supplied.
mach_vm_address_t agdcCallbackOriginal,pathLookupOriginal,portTrainingOriginal;
uint32_t trainingPathCalls,trainingPathFound,portTrainingCalls,portTrainingResult;
#include "kern_raptorlake_p_training_path_impl.hpp"
uint32_t agdcTraceReady,agdcTraceSequence,agdcTraceRows[32][8]{};
void recordAGDC(uint32_t kind,uint32_t event,unsigned long argument,uint32_t value,uint32_t result){
 auto seq=__sync_add_and_fetch(&agdcTraceSequence,1);
 uint32_t row[]{0,kind,event,argument<=9?static_cast<uint32_t>(argument):~0U,value,result,state[6],0};
 auto store=[&](uint32_t *to){__atomic_store_n(to,0U,__ATOMIC_RELEASE);memcpy(to+1,row+1,28);__atomic_store_n(to,seq,__ATOMIC_RELEASE);};
 store(agdcTraceRows[16+(seq%16)]);if(seq<=16)store(agdcTraceRows[seq-1]);
}
uint32_t observedAGDCCallback(void *owner,uint32_t event,unsigned long argument,uint32_t value){
 const bool own=owner==framebufferOwner && wexRplReferenceFBAllowed(framebufferOwner);
 if(own)recordAGDC(1,event,argument,value,~0U);
 auto result=reinterpret_cast<uint32_t (*)(void *,uint32_t,unsigned long,uint32_t)>(agdcCallbackOriginal)(owner,event,argument,value);
 if(own)recordAGDC(2,event,argument,value,result);
 return result;
}
void *observedPathLookup(void *port){
 auto path=reinterpret_cast<void *(*)(void *)>(pathLookupOriginal)(port);
 if(livePortSelected(port)){
  __sync_fetch_and_add(&trainingPathCalls,1);if(path)__sync_fetch_and_add(&trainingPathFound,1);
  recordAGDC(3,0,5,path?1:0,0);
 }
 return path;
}
uint32_t observedPortTraining(void *port,void *request){
 const bool own=livePortSelected(port);uint32_t portIndex=~0U,value=0;
 if(own){
  __sync_fetch_and_add(&portTrainingCalls,1);
  if(request){memcpy(&portIndex,request,4);auto bytes=static_cast<uint8_t *>(request);value=bytes[0x13]|(static_cast<uint32_t>(bytes[0x12])<<8);}
  recordAGDC(4,portIndex,5,value,~0U);
 }
 uint32_t prepare=0;
 if(own && trainingLeaseState[1]){
  prepare=prepareTrainingPath(port);
  if(!prepare && trainingLease.path && !trainingLease.adopted){
   IORecursiveLockLock(livePortLock);uint32_t lanes=0;memcpy(&lanes,static_cast<uint8_t *>(port)+0x124,4);
   TrainingPowerIO io;auto route=trainingLease.route.prepare(io,lanes);trainingLeaseState[10]=static_cast<uint32_t>(route);
   if(route!=RaptorLakeP::TrainingRoute::Status::Ready)prepare=20+static_cast<uint32_t>(route);else trainingLease.busy=true;
   IORecursiveLockUnlock(livePortLock);
  }
 }
 auto result=prepare?static_cast<uint32_t>(kIOReturnNotReady):reinterpret_cast<uint32_t (*)(void *,void *)>(portTrainingOriginal)(port,request);
 if(own && trainingLeaseState[1]){IORecursiveLockLock(livePortLock);trainingLease.busy=false;IORecursiveLockUnlock(livePortLock);}
 if(own){
  uint32_t status=~0U;if(request)memcpy(&status,static_cast<uint8_t *>(request)+0x38,4);
  portTrainingResult=result;recordAGDC(5,portIndex,5,status,result);
 }
 return result;
}

bool nativeAGDCCallbackBound(){
 if(!framebufferOwner || !wexRplReferenceFBAllowed(framebufferOwner))return false;
 uintptr_t function=0,context=0;memcpy(&function,reinterpret_cast<uint8_t *>(framebufferOwner)+0x1C00,8);
 memcpy(&context,reinterpret_cast<uint8_t *>(framebufferOwner)+0x1C08,8);
 return function && context;
}
void publishAGDCObservation(){
 if(!target || !captureAllowed())return;
 uint32_t meta[16]{2,agdcTraceReady,agdcTraceSequence,nativeAGDCCallbackBound(),~0U,~0U,~0U,~0U,~0U,0,0,0};
 auto port=getPortEntry?reinterpret_cast<OSObject *(*)(uint32_t)>(getPortEntry)(5):nullptr;uint32_t ddi=~0U;
 if(observerPort(port,ddi) && ddi==5){
  memcpy(&meta[4],reinterpret_cast<uint8_t *>(port)+0x120,4);
  memcpy(&meta[5],reinterpret_cast<uint8_t *>(port)+0x550,4);
  memcpy(&meta[6],reinterpret_cast<uint8_t *>(port)+0x554,4);
  uint8_t sinks=0;memcpy(&sinks,reinterpret_cast<uint8_t *>(port)+0xF8,1);meta[7]=sinks;
  meta[8]=nativeAuxBypassRefs(port);meta[9]=livePortLease.auxRequest;
  uint8_t maximum=0;memcpy(&maximum,reinterpret_cast<uint8_t *>(port)+0x11,1);meta[10]=maximum;
  memcpy(&meta[11],reinterpret_cast<uint8_t *>(port)+0x124,4);
 }
 meta[12]=trainingPathCalls;meta[13]=trainingPathFound;meta[14]=portTrainingCalls;meta[15]=portTrainingResult;
 uint32_t rows[32][8]{};
 for(unsigned i=0;i<32;++i){auto seq=__atomic_load_n(agdcTraceRows[i],__ATOMIC_ACQUIRE);memcpy(rows[i],agdcTraceRows[i],32);if(!seq || seq!=rows[i][0] || seq!=__atomic_load_n(agdcTraceRows[i],__ATOMIC_ACQUIRE))memset(rows[i],0,32);}
 target->setProperty("WhateverXeRPLPReferenceAGDCMeta",meta,sizeof(meta));
 target->setProperty("WhateverXeRPLPReferenceAGDCTrace",rows,sizeof(rows));
 publishTrainingLease();
}

bool nativeRescanPermitted(){
 return nativeHotplugEntry && nativeRescanWork && captureAllowed() && shutdownQuery && !shutdownQuery() &&
  __atomic_load_n(&livePortReady,__ATOMIC_ACQUIRE) && __atomic_load_n(&livePortPermission,__ATOMIC_ACQUIRE) &&
  !livePortLease.owner && !livePortLease.active;
}
void publishNativeRescan(){if(target)target->setProperty("WhateverXeRPLPReferenceTCRescan",nativeRescanState,sizeof(nativeRescanState));}
IOReturn nativeRescanAction(OSObject *owner,void *,void *,void *,void *){
 if(owner!=framebufferOwner || !nativeRescanPermitted() || !nativeAGDCCallbackBound())return kIOReturnNotReady;
 auto loop=framebufferOwner->getWorkLoop();
 if(!loop || !loop->inGate() || !getPortEntry)return kIOReturnNotReady;
 auto port=reinterpret_cast<OSObject *(*)(uint32_t)>(getPortEntry)(5);
 uint32_t ddi=~0U;
 if(!observerPort(port,ddi) || ddi!=5 || !livePortSelected(port))return kIOReturnNotReady;
 auto tc=backend.readReg32(0x161508),hpd=backend.readReg32(0x44470);
 if(tc==~0U || (tc&7)!=5 || hpd==~0U || !(hpd&(1U<<18)) || (hpd&(1U<<2)))return kIOReturnNotReady;
 memcpy(&nativeRescanState[5],reinterpret_cast<uint8_t *>(port)+0x550,4);
 port->retain();++nativeRescanState[4];
 reinterpret_cast<void (*)(void *,uint32_t,uint32_t)>(nativeHotplugEntry)(framebufferOwner,5,3);
 memcpy(&nativeRescanState[6],reinterpret_cast<uint8_t *>(port)+0x550,4);
 memcpy(&nativeRescanState[7],reinterpret_cast<uint8_t *>(port)+0x554,4);
 port->release();publishLivePort();return kIOReturnSuccess;
}
void nativeRescanWorker(thread_call_param_t,thread_call_param_t){
 IOReturn result=kIOReturnNotReady;
 if(nativeRescanPermitted()){
  auto owner=framebufferOwner;owner->retain();
  auto loop=owner->getWorkLoop();
  if(loop){loop->retain();result=loop->runAction(nativeRescanAction,owner);loop->release();}
  owner->release();
 }
 nativeRescanState[3]=result;publishNativeRescan();
 __sync_synchronize();nativeRescanBusy=0;
}

void probeTCAux(){
 using namespace RaptorLakeP::ReferenceTCAux;
 ParentResult result;
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
   result=probeWithParent(io,true);
   auxProbePermit=0;auxProbeThread=nullptr;auxProbePort=nullptr;port->release();
  }
 }
 if(target)target->setProperty("WhateverXeRPLPReferenceTCAUX",&result,sizeof(result));
 if(__atomic_load_n(&livePortReady,__ATOMIC_ACQUIRE) && result.aux.status==RaptorLakeP::ReferenceTCAux::Complete && result.aux.nativeResult==0){IORecursiveLockLock(livePortLock);__atomic_store_n(&livePortPermission,1,__ATOMIC_RELEASE);publishLivePort();IORecursiveLockUnlock(livePortLock);}
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
  if(livePortLock){IORecursiveLockLock(livePortLock);publishLivePort();IORecursiveLockUnlock(livePortLock);}
  publishAGDCObservation();
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

bool wexRplReferenceNVRAMHooks(KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 using namespace RaptorLakeP::ReferenceNVRAM;
 if(!wexRplReferencePanel() || !state[1] || !target || XELP::kernelOS(version)!=XELP::OS::Tahoe)return false;
 bool ok=wexRplReferenceImage(reinterpret_cast<void *>(a),n,false) && n>=Call+sizeof(Before);
 auto method=p.solveSymbol(i,"__ZN24AppleIntelBaseController11hwSaveNVRAMEv",a,n);p.clearError();
 auto table=p.solveSymbol(KernelPatcher::KernelID,"__ZTV18IODTPlatformExpert");p.clearError();
 auto writer=p.solveSymbol(KernelPatcher::KernelID,"__ZN18IODTPlatformExpert18writeNVRAMPropertyEP15IORegistryEntryPK8OSSymbolP6OSData");p.clearError();
 auto reserved=p.solveSymbol(KernelPatcher::KernelID,"__ZN18IODTPlatformExpert28_RESERVEDIODTPlatformExpert1Ev");p.clearError();
 uintptr_t oldBinding=0,newBinding=0;
 if(table && writer && reserved){
  memcpy(&oldBinding,reinterpret_cast<void *>(table+16+OldSlot),sizeof(oldBinding));
  memcpy(&newBinding,reinterpret_cast<void *>(table+16+WriteSlot),sizeof(newBinding));
 }
 ok=ok && method>=a && accepts(reinterpret_cast<const uint8_t *>(a+Call),sizeof(Before),method-a,
                             oldBinding,reserved,newBinding,writer);
 if(ok){
  // Preserve native NVRAM construction, write, status and cleanup. Only fix
  // the obsolete virtual dispatch slot that calls a reserved method on Tahoe.
  p.routeBlock(a+Call,After,sizeof(After));
  ok=p.getError()==KernelPatcher::Error::NoError && !memcmp(reinterpret_cast<void *>(a+Call),After,sizeof(After));
  p.clearError();
 }
 target->setProperty("WhateverXeRPLPNVRAMABIReady",ok);
 SYSLOG("rplp","reference framebuffer NVRAM ABI ready=%u",ok);
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

bool wexRplReferenceLivePortHooks(KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 if(!checkKernelArgument("-wex-rplp-tcss-live"))return true;
 bool ok=wexRplReferenceTCSSSelected() && checkKernelArgument("-wex-rplp-tcss-observe") &&
  checkKernelArgument("-wex-rplp-tcss-aux-probe") && wexRplReferenceImage(reinterpret_cast<void *>(a),n,false) &&
  portBindings[1]==2 && !portBindings[2] && n>=0x902FA;
 const char *names[]{"__ZN17AppleIntelPortHAL11setPortModeEN14AppleIntelPort8PortModeE",
  "__ZN14AppleIntelPort11setPortModeENS_8PortModeE","__ZN14AppleIntelPort18enablePowerWellAuxEv","__ZN14AppleIntelPort19disablePowerWellAuxEv",
  "__ZN17AppleIntelPortHAL15ReadDekelPhyRegEj","__ZN17AppleIntelPortHAL16WriteDekelPhyRegEjj",
  "__ZN17AppleIntelPortHAL26getDklPhyLaneConfigBitMaskEv","__ZN17AppleIntelPortHAL15setFIALaneCountEj"};
 const uint32_t offsets[]{0x8F344,0x262EA,0x2972C,0x29768,0x90234,0x902EA,0x8FAE0,0x8FDC4};
 const uint8_t prefixes[8][16]{
  {0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC},
  {0x55,0x48,0x89,0xE5,0x41,0x56,0x53,0x48,0x83,0xEC,0x20,0x48,0xFF,0x05,0x64,0x35},
  {0x55,0x48,0x89,0xE5,0x48,0xFF,0x05,0x51,0x0B,0x12,0x00,0x48,0xFF,0x05,0x52,0xFF},
  {0x55,0x48,0x89,0xE5,0x48,0xFF,0x05,0x1D,0x0B,0x12,0x00,0x48,0xFF,0x05,0x16,0xFF},
  {0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x53,0x50,0x48,0xFF,0x05,0xB3,0xD2,0x0C},
  {0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0xFF,0x05,0x0C,0xD2},
  {0x55,0x48,0x89,0xE5,0x41,0x56,0x53,0x48,0xFF,0x05,0xEA,0xD7,0x0C,0x00,0x48,0xFF},
  {0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0xFF,0x05,0xD2,0xD5}};
 mach_vm_address_t entries[8]{};
 for(unsigned x=0;ok&&x<8;x++){
  entries[x]=p.solveSymbol(i,names[x],a,n);p.clearError();
  ok=entries[x]==a+offsets[x] && n>=offsets[x]+16 && !memcmp(reinterpret_cast<void *>(entries[x]),prefixes[x],16);
 }
 if(ok){livePortLock=IORecursiveLockAlloc();livePhyLock=livePortLock?IOSimpleLockAlloc():nullptr;ok=livePortLock && livePhyLock;}
 if(ok){
  liveBaseMode=entries[1];liveAuxEnable=entries[2];liveAuxDisable=entries[3];
  KernelPatcher::RouteRequest routes[]{
   {names[0],livePortMode,livePortOriginal},{names[4],livePortReadPhy,liveReadPhyOriginal},
   {names[5],livePortWritePhy,liveWritePhyOriginal},
   {names[6],livePortLaneMask,liveLaneMaskOriginal},{names[7],livePortSetLanes,liveSetLanesOriginal}};
  ok=p.routeMultiple(i,routes,5,a,n);p.clearError();
 }
 __atomic_store_n(&livePortReady,ok,__ATOMIC_RELEASE);publishLivePort();
 SYSLOG("rplp","optional TC3 live transition ready=%u; awaiting privileged AUX preflight",ok);
 return ok;
}

bool wexRplReferenceNativeHotplugHooks(KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 if(!checkKernelArgument("-wex-rplp-tcss-live"))return true;
 const char *name="__ZN24AppleIntelBaseController20invokeHotplugHandlerEjN14AppleIntelPort8PortModeE";
 auto entry=p.solveSymbol(i,name,a,n);p.clearError();
 const uint8_t prefix[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC};
 const uint8_t lookup[]{0x44,0x89,0xE7,0xE8,0x96,0x5B,0xF7,0xFF,0x48,0x85,0xC0,0x0F};
 const bool ok=nativeRescanWork && __atomic_load_n(&livePortReady,__ATOMIC_ACQUIRE) &&
  wexRplReferenceImage(reinterpret_cast<void *>(a),n,false) && n>=0xB0C9A && entry==a+0xAFE6C &&
  !memcmp(reinterpret_cast<void *>(entry),prefix,sizeof(prefix)) &&
  !memcmp(reinterpret_cast<void *>(a+0xAFF14),lookup,sizeof(lookup));
 if(ok)nativeHotplugEntry=entry;
 nativeRescanState[1]=ok;publishNativeRescan();return ok;
}

bool wexRplReferenceTrainingPathHooks(KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 if(!checkKernelArgument("-wex-rplp-training-path"))return true;
 bool ok=livePortReady && wexRplReferenceImage(reinterpret_cast<void *>(a),n,false);
 const char *names[]{"__ZN21AppleIntelDisplayPath12allocatePathEv","__ZN21AppleIntelDisplayPath12allocatePathEj","__ZN21AppleIntelDisplayPath8freePathEv","__ZN21AppleIntelFramebuffer28allocateDisplayPathResourcesEjj","__ZN19AppleIntelPowerWell18disablePowerWellPGEj","__ZN24AppleIntelBaseController17doLinkTrainingEndEP23AGDCDPLinkTrainingEnd_t","__ZN14AppleIntelPort15writeRegister32Ejj","__ZN19AppleIntelPowerWell17enablePowerWellPGEj",
 "__ZN17AppleIntelPortHAL29getConfiguredDklPhyLaneConfigEv","__ZN17AppleIntelPortHAL29setVoltageSwingAndPreEmphasisEjjP15PortDriveConfig",
 "__ZN21AppleIntelDisplayPath22setLinkTrainingPatternE19LinkTrainingPattern","__ZN19AppleIntelPowerWell18enablePowerWellDDIEj","__ZN19AppleIntelPowerWell19disablePowerWellDDIEj"};
 const uint32_t offsets[]{0xe914c,0xe9302,0xe9f2c,0x9c150,0x89428,0xa250e,0x2f49e,0x8a5fa,0x8fc74,0x90588,0xee060,0x8a88a,0x8879e};
 const uint8_t prefixes[13][20]{{0x55,0x48,0x89,0xE5,0x53,0x48,0x83,0xEC,0x28,0x48,0xFF,0x05,0x74,0x50,0x08,0x00,0x48,0xFF,0x05,0xED},{0x55,0x48,0x89,0xE5,0x53,0x48,0x83,0xEC,0x28,0x48,0xFF,0x05,0x16,0x4F,0x08,0x00,0x89,0xFB,0x48,0xFF},{0x55,0x48,0x89,0xE5,0x41,0x56,0x53,0x48,0x83,0xEC,0x20,0x48,0xFF,0x05,0x52,0x45,0x08,0x00,0x49,0x89},{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xEC,0x30,0x48,0xFF,0x05,0xE2,0x37},{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xEC,0x30,0x48,0xFF,0x05,0xC2,0x28},{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x53,0x48,0x83,0xEC,0x38,0x48,0xFF,0x05,0xF6,0xE4,0x0B,0x00},{0x55,0x48,0x89,0xE5,0x48,0xFF,0x05,0xBF,0xBF,0x11,0x00,0x48,0x8B,0xBF,0x78,0x05,0x00,0x00,0x89,0xF6},{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x53,0x48,0x83,0xEC,0x38,0x48,0xFF,0x05,0x9A,0x1B,0x0D,0x00},{0x55,0x48,0x89,0xE5,0x41,0x56,0x53,0x48,0xFF,0x05,0xBE,0xD6,0x0C,0x00,0x48,0xFF,0x05,0xAF,0xD6,0x0C},{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC,0x58,0x48,0xFF,0x05},{0x55,0x48,0x89,0xE5,0x41,0x56,0x53,0x48,0x83,0xEC,0x30,0x48,0xFF,0x05,0x9E,0x0C,0x08,0x00,0x89,0xF3},{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC,0x38,0x48,0xFF,0x05},{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC,0x48,0x48,0xFF,0x05}};
 mach_vm_address_t entries[13]{};
 for(unsigned x=0;ok&&x<13;x++){entries[x]=p.solveSymbol(i,names[x],a,n);p.clearError();ok=entries[x]==a+offsets[x] && n>=offsets[x]+20 && !memcmp(reinterpret_cast<void *>(entries[x]),prefixes[x],20);}
 if(ok){trainingCleanupWork=thread_call_allocate(trainingCleanupWorker,nullptr);ok=trainingCleanupWork;}
 if(ok){
  KernelPatcher::RouteRequest routes[]{
   {names[0],trainingAllocateAny,trainingAllocateAnyOriginal},{names[1],trainingAllocateFixed,trainingAllocateFixedOriginal},
   {names[2],trainingFree,trainingFreeOriginal},{names[3],trainingResources,trainingResourcesOriginal},
   {names[4],trainingPutPG,trainingPGPutOriginal},{names[5],trainingEnd,trainingEndOriginal},
   {names[6],trainingPortWrite,trainingPortWriteOriginal},{names[7],trainingGetPG,trainingPGGet},
   {names[8],trainingConfiguredLanes,trainingConfiguredOriginal},{names[9],trainingVoltage,trainingVoltageOriginal},
   {names[10],trainingPattern,trainingPatternOriginal},{names[11],trainingGetDDI,trainingDDIGetOriginal},{names[12],trainingPutDDI,trainingDDIPutOriginal}};
  ok=p.routeMultiple(i,routes,13,a,n);p.clearError();
 }
 trainingLeaseState[1]=ok;return ok;
}

bool wexRplReferenceAGDCObservationHooks(KernelPatcher &p,size_t i,mach_vm_address_t a,size_t n){
 if(!checkKernelArgument("-wex-rplp-tcss-live"))return true;
 const char *names[]{"__ZN24AppleIntelBaseController12CallBackAGDCE31kAGDCRegisterLinkControlEvent_tmj",
  "__ZN21AppleIntelDisplayPath18getFirstPathByPortEP14AppleIntelPort","__ZN14AppleIntelPort12linkTrainingEP18AGDCDPPortConfig_t"};
 const uint32_t offsets[]{0x9F9CA,0xE9AAA,0x26C54};
 const uint8_t prefixes[3][20]{
  {0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xEC,0x20},
  {0x55,0x48,0x89,0xE5,0x41,0x56,0x53,0x48,0x83,0xEC,0x30,0x48,0xFF,0x05,0xE4,0x48,0x08,0x00,0x49},
  {0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xEC,0xB8,0x00,0x00,0x00}};
 const unsigned lengths[]{15,19,20};
 bool ok=wexRplReferenceImage(reinterpret_cast<void *>(a),n,false) && __atomic_load_n(&livePortReady,__ATOMIC_ACQUIRE);
 for(unsigned x=0;ok&&x<3;x++){
  auto entry=p.solveSymbol(i,names[x],a,n);p.clearError();
  ok=entry==a+offsets[x] && n>=offsets[x]+lengths[x] && !memcmp(reinterpret_cast<void *>(entry),prefixes[x],lengths[x]);
 }
 if(ok){
  KernelPatcher::RouteRequest routes[]{
   {names[0],observedAGDCCallback,agdcCallbackOriginal},{names[1],observedPathLookup,pathLookupOriginal},
   {names[2],observedPortTraining,portTrainingOriginal}};
  ok=p.routeMultiple(i,routes,3,a,n);p.clearError();
 }
 if(ok)ok=wexRplReferenceTrainingPathHooks(p,i,a,n);
 agdcTraceReady=ok;return ok;
}

bool wexRplReferenceExternalPort(const void *port){
 return checkKernelArgument("-wex-rplp-training-path") && livePortSelected(const_cast<void *>(port));
}
bool wexRplReferenceExternalNeedsMode(const void *owner,const void *head,const void *path){
 return checkKernelArgument("-wex-rplp-training-path") && trainingExternalNeedsMode(owner,head,path);
}

bool wexRplReferencePanelMode(const void *owner,const void *head,const void *path){
 if(!wexRplReferencePanel() || !state[1] || state[7] || !owner || owner!=framebufferOwner || !head || !path || !wexRplReferenceFBAllowed(owner))return false;
 auto fb=static_cast<const IOService *>(head);auto meta=fb->getMetaClass();
 if(strcmp(meta->getClassName(),"AppleIntelFramebufferDiags") || meta->getClassSize()!=0x4B98)return false;
 uintptr_t bound=0;uint32_t index=~0U,pipe=~0U;
 memcpy(&bound,static_cast<const uint8_t *>(head)+0x1D0,8);memcpy(&index,static_cast<const uint8_t *>(head)+0x1DC,4);
 memcpy(&pipe,static_cast<const uint8_t *>(path)+0x3640,4);
 return bound==reinterpret_cast<uintptr_t>(owner) && index==0 && pipe==0;
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
bool wexRplReferenceRecoveryExtendedRequested(){return wexRplReferenceRecoveryRequested() && checkKernelArgument("-wex-rplp-gpu-recovery-v2");}
bool wexRplReferenceBlitPitchRequested(){return wexRplReferencePanel() && state[1] && !state[7] && checkKernelArgument("-wex-rplp-blit-pitch");}
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
 if(wexRplReferencePanel() && checkKernelArgument("-wex-rplp-tcss-live"))nativeRescanWork=thread_call_allocate(nativeRescanWorker,nullptr);
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
bool rescanTC3(){
#ifdef WEX_RPLP_REFERENCE_BACKEND
 if(!nativeRescanPermitted() || !__sync_bool_compare_and_swap(&nativeRescanBusy,0,1))return false;
 if(!__sync_bool_compare_and_swap(&nativeRescanUsed,0,1)){nativeRescanBusy=0;return false;}
 ++nativeRescanState[2];publishNativeRescan();thread_call_enter(nativeRescanWork);return true;
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
