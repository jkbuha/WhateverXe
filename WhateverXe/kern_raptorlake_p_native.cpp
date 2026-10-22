#include "kern_raptorlake_p_native.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_raptorlake_p_native_memory.hpp"
#include "kern_raptorlake_p_control.hpp"
#include "kern_raptorlake_p_native_engines.hpp"
#include "kern_raptorlake_p_bringup.hpp"
#include "kern_xelp_device.hpp"
#include <Headers/kern_api.hpp>
#include <Headers/kern_util.hpp>
#include <IOKit/IOLib.h>
#include <IOKit/IOService.h>
#include <IOKit/IOCatalogue.h>
#include <kern/thread_call.h>
#include <kern/clock.h>
#include <libkern/c++/OSMetaClass.h>
#include <libkern/c++/OSSymbol.h>
#include <libkern/c++/OSNumber.h>
extern "C" const char version[];
namespace RaptorLakeP {namespace Native {
static const char *paths[]{"/Library/Extensions/AppleIntelTGLGraphics.kext/Contents/MacOS/AppleIntelTGLGraphics"};
static KernelPatcher::KextInfo graphics{"com.apple.driver.AppleIntelTGLGraphics",paths,1,{true},{},KernelPatcher::KextInfo::Unloaded};
static Record receipt;
static IOPCIDevice *gpu;
static thread_call_t call;
static mach_vm_address_t originalProbe,originalStart;
using Load=int32_t (*)(const char *,bool,bool,uint8_t,uint8_t,OSArray *);
using Add=bool (*)(IOCatalogue *,OSArray *,bool);
static Load load;static Add add;static IOCatalogue **catalogue;
static uint32_t firstProbe,remoteArmed;
static void set(Field field,uint64_t value){__atomic_store_n(&receipt.q[field],value,__ATOMIC_RELEASE);}
static uint64_t get(Field field){return __atomic_load_n(&receipt.q[field],__ATOMIC_ACQUIRE);}
static void publish(){if(gpu){Record snapshot;for(unsigned i=0;i<Fields;++i)snapshot.q[i]=__atomic_load_n(&receipt.q[i],__ATOMIC_ACQUIRE);gpu->setProperty("WhateverXeRPLPNative",&snapshot,sizeof(snapshot));}}
static void schedule(){uint64_t deadline=0;clock_interval_to_deadline(1,1000000000,&deadline);thread_call_enter_delayed(call,deadline);}
static bool physical(IOPCIDevice *pci){return pci && requested(XELP::deviceProfile(pci),XELP::kernelOS(version),
    checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
    checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs"));}
static IOService *probe(IOService *object,IOService *provider,SInt32 *score) {
    auto pci=OSDynamicCast(IOPCIDevice,provider);
    if(!get(PatchReady) || !physical(pci) || !object || object->getMetaClass()->getClassSize()<0x1400 ||
       !__sync_bool_compare_and_swap(&firstProbe,0,1))return nullptr;
    set(ProbeEntered,1);publish();
    auto result=reinterpret_cast<IOService *(*)(IOService *,IOService *,SInt32 *)>(originalProbe)(object,provider,score);
    set(ProbeReturned,1);
    uint32_t identity=0,revision=0,selector=0;
    memcpy(&identity,reinterpret_cast<uint8_t *>(object)+0x1118,4);
    memcpy(&revision,reinterpret_cast<uint8_t *>(object)+0x1110,4);
    memcpy(&selector,reinterpret_cast<uint8_t *>(object)+0x1120,4);
    set(Identity,identity);set(Revision,revision);set(Selector,selector);
    const bool matched=result==object && identity==0xA7A08086 && revision==4 && selector==2;
    set(ProbeMatched,matched);publish();return matched ? result : nullptr;
}
// Intentionally never invoke native start, configureDevice, memory manager or
// engine setup. A matched object may be freed through the ordinary IOKit path.
static bool start(IOService *object,IOService *provider) {
    if(physical(OSDynamicCast(IOPCIDevice,provider))) {
        set(StartBlocked,1);
        if(get(PatchReady) && get(ProbeMatched)) {
            auto pci=OSDynamicCast(IOPCIDevice,provider);NativeMemory::run(object,pci);
            if(NativeEngines::enabled())NativeEngines::run(pci,NativeMemory::passed());
        }
    }
    publish();return false;
}
bool requestRemoteArm() {
    if(!checkKernelArgument("-wex-rplp-manual-arm") || !get(Armed) || !get(PatchReady) ||
       get(Failure) || get(Published) || !physical(gpu) || !call ||
       !__sync_bool_compare_and_swap(&remoteArmed,0,1))return false;
    Control::update(true,true);schedule();return true;
}
static void worker(thread_call_param_t,thread_call_param_t) {
    if(!get(Armed) || !physical(gpu) || get(Failure))return;
    if(checkKernelArgument("-wex-rplp-manual-arm")) {
        if(!Control::create(gpu)){set(Failure,6);publish();return;}
        Control::update(get(PatchReady),__atomic_load_n(&remoteArmed,__ATOMIC_ACQUIRE)!=0);
        if(!__atomic_load_n(&remoteArmed,__ATOMIC_ACQUIRE))return; // no personality publication before root's signal
    }
    if(!get(LoadRequested)) {
        set(LoadRequested,1);publish();
        // XNU OSKextExcludeNone=0, OSKextExcludeAll=2. Request module start
        // with matching disabled, outside Lilu's patcher callback/lock.
        const auto result=load(graphics.id,true,true,0,2,nullptr);
        set(LoadResult,static_cast<uint32_t>(result));publish();
    }
    if(!get(PatchReady) || get(Published))return;
    const auto attempt=get(PublishAttempts)+1;set(PublishAttempts,attempt);
    auto name=OSSymbol::withCString("IntelAccelerator");
    const bool present=name && OSMetaClass::getMetaClassWithName(name);
    OSSafeReleaseNULL(name);
    if(!present) {if(attempt<20)schedule();else{set(Failure,4);publish();}return;}
    auto dict=OSDictionary::withCapacity(8);auto array=OSArray::withCapacity(1);
    const auto text=[&](const char *key,const char *value){auto str=OSString::withCString(value);const bool ok=dict && str && dict->setObject(key,str);OSSafeReleaseNULL(str);return ok;};
    auto priority=OSNumber::withNumber(1001,32);
    const bool valid=dict && array && priority &&
        text("CFBundleIdentifier",graphics.id) && text("IOClass","IntelAccelerator") &&
        text("IOProviderClass","IOPCIDevice") && text("IOMatchCategory","IOAccelerator") &&
        text("IOPCIPrimaryMatch","0xa7a08086") && text("IOPCIClassMatch","0x03000000&0xff000000") &&
        dict->setObject("IOProbeScore",priority) && array->setObject(dict);
    const bool published=valid && *catalogue && add(*catalogue,array,true);
    OSSafeReleaseNULL(priority);OSSafeReleaseNULL(array);OSSafeReleaseNULL(dict);
    set(Published,published);if(!published)set(Failure,5);publish();
}
static void processGraphics(void *,KernelPatcher &patcher,size_t index,mach_vm_address_t address,size_t size) {
    if(index!=graphics.loadIndex || !get(Armed) || !physical(gpu))return;
    const auto p=patcher.solveSymbol<mach_vm_address_t>(index,"__ZN16IntelAccelerator5probeEP9IOServicePi",address,size);
    const auto s=patcher.solveSymbol<mach_vm_address_t>(index,"__ZN16IntelAccelerator5startEP9IOService",address,size);
    patcher.clearError();
    bool ready=p==address+ProbeOffset && s==address+StartOffset && exactImage(reinterpret_cast<void *>(address),size);
    if(ready)ready=Bringup::prepare(patcher,gpu,address,size);
    if(ready) {
        KernelPatcher::RouteRequest stop{"__ZN16IntelAccelerator5startEP9IOService",start,originalStart};
        ready=patcher.routeMultiple(index,&stop,1,address,size);
        if(ready) {
            ready=NativeMemory::prepare(patcher,index,address,size);
        }
        if(ready && NativeEngines::enabled())ready=NativeEngines::prepare(patcher,index,address,size);
        if(ready) {
            KernelPatcher::RouteRequest observe{"__ZN16IntelAccelerator5probeEP9IOServicePi",probe,originalProbe};
            ready=patcher.routeMultiple(index,&observe,1,address,size);
        }
        if(ready) {
            KernelPatcher::LookupPatch sku{&graphics,SKUFind,SKUReplace,sizeof(SKUFind),1};
            patcher.applyLookupPatch(&sku,reinterpret_cast<uint8_t *>(address+SKUOffset),sizeof(SKUFind)+1);
            ready=patcher.getError()==KernelPatcher::Error::NoError &&
                !memcmp(reinterpret_cast<void *>(address+SKUOffset),SKUReplace,sizeof(SKUReplace));
        }
    }
    patcher.clearError();set(PatchReady,ready);if(!ready)set(Failure,3);publish();
    if(ready)schedule();
}
static void processKernel(void *,KernelPatcher &patcher) {
    load=reinterpret_cast<Load>(patcher.solveSymbol(KernelPatcher::KernelID,"__ZN6OSKext22loadKextWithIdentifierEPKcbbhhP7OSArray"));
    catalogue=reinterpret_cast<IOCatalogue **>(patcher.solveSymbol(KernelPatcher::KernelID,"_gIOCatalogue"));
    add=reinterpret_cast<Add>(patcher.solveSymbol(KernelPatcher::KernelID,"__ZN11IOCatalogue10addDriversEP7OSArrayb"));
    patcher.clearError();
}
void init() {
    if(!checkKernelArgument("-wex-rplp-native-probe"))return;
    const bool registered=lilu.onPatcherLoad(processKernel,nullptr)==LiluAPI::Error::NoError &&
        lilu.onKextLoad(&graphics,1,processGraphics,nullptr)==LiluAPI::Error::NoError;
    set(Registered,registered);
}
void arm(IOPCIDevice *pci,const XELP::Profile &profile,const Capture &gt,const GOPCapture &gop) {
    if(gpu || !get(Registered) || !requested(profile,XELP::kernelOS(version),
       checkKernelArgument("-wex-rplp-probe"),checkKernelArgument("-wex-rplp-native-probe"),
       checkKernelArgument("-wex-rplp-bcs"),checkKernelArgument("-wex-rplp-rcs")) ||
       gt.status!=Status::Complete || gt.topology.eus!=96 || gt.releasedMask!=3 || gt.releaseFailedMask ||
       gop.status!=GOPStatus::Complete || !physical(pci))return;
    gpu=pci;gpu->retain();
    if(!load || !catalogue || !add){set(Failure,1);publish();return;}
    call=thread_call_allocate(worker,nullptr);
    if(!call){set(Failure,2);publish();return;}
    set(Armed,1);publish();schedule();
}
}}
