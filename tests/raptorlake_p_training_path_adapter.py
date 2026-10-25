"""Replay the production private-donor reservation/adoption/power lifecycle."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
body=(r/'WhateverXe/kern_raptorlake_p_training_path_impl.hpp').read_text()
f=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <map>
#include "kern_display13_power.hpp"
#include "kern_raptorlake_p_training_route.hpp"
#include "kern_raptorlake_p_reference_voltage.hpp"
#include "kern_raptorlake_p_reference_ddi.hpp"
#include "kern_raptorlake_p_reference_tc_live.hpp"
using mach_vm_address_t=uintptr_t;using thread_t=void*;using thread_call_t=void*;using thread_call_param_t=void*;using IOReturn=uint32_t;
constexpr IOReturn kIOReturnNotReady=0xE00002D8,kIOReturnSuccess=0;constexpr unsigned kMillisecondScale=1;
void *pendingToken;void clock_interval_to_deadline(unsigned n,unsigned,uint64_t*d){assert(n==2000);*d=n;}
void thread_call_enter1_delayed(void *work,void*token,uint64_t){assert(work);pendingToken=token;}
thread_t current_thread(){return (void*)7;}
struct Lock{unsigned depth=0;}lock,*livePortLock=&lock;
void IORecursiveLockLock(Lock*p){assert(p==&lock);++p->depth;}
void IORecursiveLockUnlock(Lock*p){assert(p==&lock&&p->depth);--p->depth;}
struct Meta {const char *name;unsigned size;const char*getClassName(){return name;}unsigned getClassSize(){return size;}};
struct OSObject;
struct Loop {bool gated=true;bool inGate(){return gated;}void retain(){}void release(){}
 template<class F>uint32_t runAction(F f,OSObject*o,void*token){return f(o,token,nullptr,nullptr,nullptr);}}loop;
struct OSObject {
 alignas(16) uint8_t memory[0x4B98]{};Meta meta{"AppleIntelFramebufferDiags",0x4B98};int refs=0;
 Meta*getMetaClass(){return &meta;}bool isInactive(){return false;}Loop*getWorkLoop(){return &loop;}
 void retain(){++refs;}void release(){assert(refs);--refs;}
};using IOService=OSObject;
OSObject controller,portObj,fbObj,powerObj;IOService*framebufferOwner=&controller;void*port=&portObj;
struct Backend {
 std::map<uint32_t,uint32_t> regs{{0x4AC80,0x40000001},{0x45404,0xC0F},{0x45454,3},{0x45444,0xC03},{0x42000,0x8E380000},{0x46114,0x80000000},{0x46144,0},{0x61400,0},{0x61540,0},{0x64500,0xC0},{0x71008,0},{0x16E8C0,3},{0x16E8A0,0x1C},{0x1010A0,0xAABBCCDD},{0x16AD00,0x1234}};unsigned writes=0;
 uint32_t readReg32(uint32_t r){assert(regs.count(r));return regs[r];}
 void writeReg32(uint32_t r,uint32_t v){assert(regs.count(r));++writes;if(r==0x45454){regs[r]=(v&~0x400U)|((v&0x800)?0x400:0);}else if(r==0x45404){auto mask=0xA80AU;regs[r]=(v&~(mask>>1))|((v&mask)>>1);}else regs[r]=v;}
}backend;
void IODelay(unsigned){}
thread_t liveVoltageThread=nullptr;void *liveVoltagePort=nullptr;
struct Target{void setProperty(const char*,void*,unsigned){}}targetObject,*target=&targetObject;
struct LivePhyIO{
 void *port;bool held=false;bool powerHeld(){return true;}
 uint32_t read(uint32_t r){assert(held);return backend.readReg32(r);}void write(uint32_t r,uint32_t v){assert(held);backend.writeReg32(r,v);}
 unsigned lock(){assert(!held);held=true;return 1;}void unlock(unsigned n){assert(held&&n==1);held=false;}void bankFault(){}
};
struct LivePortIO{void*port;uint32_t read(uint32_t r){return backend.readReg32(r);}};bool allowed=true,healthy=true;
unsigned deferredReleases=0;uint32_t livePortState[16]{};
void publishLivePort(){assert(lock.depth);}
struct LiveLease{
 bool phy=true,active=true,parent=true;uintptr_t object,owner=0;
 bool witness(LivePortIO&,uintptr_t who,uintptr_t p){return healthy&&who==(uintptr_t)framebufferOwner&&p==(uintptr_t)port;}
 RaptorLakeP::ReferenceTCLive::Status release(LivePortIO&,uintptr_t who,uintptr_t p){
  assert(owner==who&&object==p&&!active&&portObj.refs&&lock.depth);
  assert(!(backend.regs[0x45404]&0x3000)); // PipeB must have drained first.
  ++deferredReleases;owner=0;parent=false;return RaptorLakeP::ReferenceTCLive::Status::Ready;
 }
}livePortLease;
bool livePortSelected(void*p){return allowed&&p==port;}
uint32_t livePortReady=1,livePortPermission=1,liveSinkMaxRate=20,liveSinkMaxLanes=2;
bool wexRplReferenceFBAllowed(void*p){return p==framebufferOwner;}
OSObject*portByDDI(uint32_t d){assert(d==5);return &portObj;}uintptr_t getPortEntry=(uintptr_t)portByDDI;
void*pathLookupOriginal; // Only the address is consumed by production.
alignas(16) uint8_t pathMem[0x3660]{};unsigned allocs=0,frees=0,originalWrites=0;
void put32(uint8_t*p,unsigned o,uint32_t v){memcpy(p+o,&v,4);}uint32_t get32(uint8_t*p,unsigned o){uint32_t v;memcpy(&v,p+o,4);return v;}
void putPtr(uint8_t*p,unsigned o,void*v){memcpy(p+o,&v,8);}
void* lookup(void*p){assert(p==port);void*bound;memcpy(&bound,pathMem+0x3648,8);return bound==p?pathMem:nullptr;}
void* fixed(uint32_t n){assert(n==1&&lock.depth);++allocs;if(get32(pathMem,0x3644))return nullptr;put32(pathMem,0x3640,1);put32(pathMem,0x3644,1);return pathMem;}
void* any(){return fixed(1);}
void freePath(void*p){assert(p==pathMem);++frees;memset(pathMem,0,sizeof(pathMem));put32(pathMem,0x3640,1);}
void getPG(void*p,uint32_t pg){assert(p==&powerObj&&pg==3);for(unsigned n=1;n<=pg;n++)put32(powerObj.memory,0x1c+4*n,get32(powerObj.memory,0x1c+4*n)+1);}
void putPG(void*p,uint32_t pg){assert(p==&powerObj&&pg==3);for(unsigned n=1;n<=pg;n++){auto count=get32(powerObj.memory,0x1c+4*n);assert(count);put32(powerObj.memory,0x1c+4*n,count-1);}}
void getDDI(void*p,uint32_t d){assert(p==&powerObj&&d==5);put32(powerObj.memory,0x44,get32(powerObj.memory,0x44)+1);}
void putDDI(void*p,uint32_t d){assert(p==&powerObj&&d==5);auto refs=get32(powerObj.memory,0x44);assert(refs);put32(powerObj.memory,0x44,refs-1);}
uint32_t end(void*,void*){return 0;}
void portWrite(void*p,uint32_t reg,uint32_t v){assert(p==port);++originalWrites;backend.writeReg32(reg,v);}
bool trainingResourcesNative(void*,uint32_t,uint32_t);
uint32_t trainingConfiguredLanes(void*);
unsigned voltageCalls=0;RaptorLakeP::ReferenceVoltage::Drive lastDrive{};
uint32_t configuredNative(void*){return 0xC;}
void voltageNative(void*p,uint32_t,uint32_t,void*data){assert(p==port);++voltageCalls;assert(data&&trainingConfiguredLanes(p)==15);memcpy(&lastDrive,data,sizeof(lastDrive));}
uint32_t patternNative(void*,uint32_t p){backend.regs[0x61540]=0x80000000|(p<<8);return 0;}

unsigned setupCalls=0,setConfigCalls=0,getLinkCalls=0;uint32_t setupResult=0;
uint32_t setupNative(void *p){assert(p==port);++setupCalls;
 if(portObj.memory[0x116]){put32(portObj.memory,0x128,portObj.memory[0x55A]);put32(portObj.memory,0x124,portObj.memory[0x563]);}
 return setupResult;
}
uint32_t setConfigNative(void *p,void *data){assert(p==port&&data);++setConfigCalls;memcpy(portObj.memory+0x558,data,28);portObj.memory[0x116]=1;return 0;}
uint32_t getLinkNative(void *p,void *data){assert(p==pathMem&&data);++getLinkCalls;memset(data,0xCC,28);return 0;}

BODY
bool trainingResourcesNative(void*fb,uint32_t portIndex,uint32_t stream){
 if(portIndex!=5||stream)return false;
 auto p=trainingAllocateFixed(1);if(!p)return false;
 putPtr(pathMem,0x32B0,fb);putPtr(pathMem,0x3648,port);put32(((OSObject*)fb)->memory,0x49E8,1);getPG(&powerObj,3);return true;
}
int main(){
 livePortLease.object=(uintptr_t)port;powerObj.meta={"AppleIntelPowerWell",0x80};
 putPtr(controller.memory,0xC8,&powerObj);putPtr(controller.memory,0xC40,(void*)0x1234);put32(controller.memory,0xD5C,2);putPtr(controller.memory,0xD68,&fbObj);
 putPtr(portObj.memory,0x578,(void*)0x1234);putPtr(powerObj.memory,0x10,&controller);powerObj.memory[0x18]=1;
 putPtr(fbObj.memory,0x1D0,&controller);putPtr(fbObj.memory,0x4A40,(void*)0x1234);put32(fbObj.memory,0x1DC,1);
 pathLookupOriginal=(void*)lookup;trainingAllocateFixedOriginal=(uintptr_t)fixed;trainingAllocateAnyOriginal=(uintptr_t)any;trainingFreeOriginal=(uintptr_t)freePath;
 trainingPGGet=(uintptr_t)getPG;trainingPGPutOriginal=(uintptr_t)putPG;trainingResourcesOriginal=(uintptr_t)trainingResourcesNative;trainingEndOriginal=(uintptr_t)end;trainingPortWriteOriginal=(uintptr_t)portWrite;
 trainingConfiguredOriginal=(uintptr_t)configuredNative;trainingVoltageOriginal=(uintptr_t)voltageNative;trainingPatternOriginal=(uintptr_t)patternNative;
 put32(portObj.memory,0x128,20);portObj.memory[0x11A]=1;
 trainingSetupOriginal=(uintptr_t)setupNative;trainingSetConfig=(uintptr_t)setConfigNative;trainingGetLinkOriginal=(uintptr_t)getLinkNative;
 trainingDDIGetOriginal=(uintptr_t)getDDI;trainingDDIPutOriginal=(uintptr_t)putDDI;trainingCleanupWork=(void*)0x55;
 trainingLeaseState[1]=1;
 trainingGetDDI(&powerObj,5);assert(backend.regs[0x45454]==0xC03&&powerObj.refs==1);
 allowed=false;assert(prepareTrainingPath(port)==1&&!allocs&&backend.writes==1);allowed=true;
 loop.gated=false;assert(prepareTrainingPath(port)==1&&!allocs);loop.gated=true;
 fbObj.memory[0x1E9]=1;assert(prepareTrainingPath(port)==6&&!allocs);fbObj.memory[0x1E9]=0;
 powerObj.memory[0x18]=0;assert(prepareTrainingPath(port)==5&&!allocs);powerObj.memory[0x18]=1;
 assert(!prepareTrainingPath(port)&&trainingLease.path==pathMem&&trainingLease.logicalPG&&trainingPGRefs()==1);
 assert(backend.regs[0x45404]==0x3C0F&&fbObj.refs==1&&powerObj.refs==2&&portObj.refs==1);
 assert(!prepareTrainingPath(port)&&allocs==1);TrainingPowerIO io;assert(io.powerHeld());
 assert(trainingLease.route.prepare(io,2)==RaptorLakeP::TrainingRoute::Status::Ready);
 assert(trainingConfiguredLanes(port)==12);trainingVoltage(port,0,1,nullptr);
 assert(voltageCalls==1&&lastDrive.swing==5&&lastDrive.emphasis==4&&lastDrive.preshoot==0&&trainingConfiguredLanes(port)==12&&!liveVoltageThread&&!liveVoltagePort);
 trainingVoltage(port,3,3,nullptr);assert(voltageCalls==1&&trainingSignal[11]==1);
 assert(!trainingPattern(pathMem,1)&&backend.regs[0x61540]==0x80040100&&trainingSignal[15]==1);
 backend.regs[0x61540]=0;
 trainingPortWrite(port,0x64500,0x80000080);assert(backend.regs[0x64500]==0x802000C0);
 assert(trainingLink[1]==1&&trainingLink[2]==20&&trainingLink[6]==0x80000000&&trainingLink[7]==0x80000000);
 put32(portObj.memory,0x128,0);auto badWrites=originalWrites;trainingPortWrite(port,0x64500,0x800000C0);assert(originalWrites==badWrites&&trainingLink[8]==1);put32(portObj.memory,0x128,20);
 auto wc=originalWrites;backend.regs[0x64500]&=~64U;trainingPortWrite(port,0x64500,0);assert(originalWrites==wc);backend.regs[0x64500]=0xC0;
 trainingLease.busy=true;assert(!trainingResources(&fbObj,5,0));trainingLease.busy=false;
 assert(!trainingResources(&fbObj,5,1));assert(!trainingResources(&fbObj,4,0));
 // Capture only successful, physically enabled, in-range electrical settings.
 portObj.memory[0xF]=10;portObj.memory[0x11]=2;portObj.memory[0x55A]=20;portObj.memory[0x563]=2;
 put32(portObj.memory,0x128,20);put32(portObj.memory,0x124,2);backend.regs[0x64500]=0x80203642;
 recordSuccessfulTraining(port);assert(trainingNegotiatedValid&&trainingNegotiatedConfig[2]==20&&trainingNegotiatedConfig[11]==2);
 backend.regs[0x64500]=0xC0;recordSuccessfulTraining(port);assert(trainingNegotiated[7]==2&&trainingNegotiatedValid);
 // AGDC ends its temporary training burst before native modeset adoption.
 assert(trainingLease.route.restore(io)==RaptorLakeP::TrainingRoute::Status::Ready&&!trainingLease.route.captured);
 assert(trainingResources(&fbObj,5,0)&&trainingLease.adopted&&trainingLease.route.adopted&&!trainingLease.logicalPG&&trainingPGRefs()==1&&allocs==1);
 backend.regs[0x46144]=0;backend.regs[0x61540]=0x80040300;
 assert(!prepareNativeTrainingPath(port)&&backend.regs[0x46144]==0x80000000&&trainingNegotiated[26]==0&&trainingNegotiated[27]==0x80000000);
 auto colorWrites=backend.writes;
 assert(trainingExternalGamma(0x4AC80,0x40000001)==0&&trainingColor[2]==0x40000001&&trainingColor[4]==0x40000001);
 assert(trainingExternalGamma(0x4A480,0x1234)==0x1234&&trainingExternalGamma(0x4B480,0x1234)==0x1234);
 healthy=false;assert(trainingExternalGamma(0x4AC80,0x40000001)==0x40000001);healthy=true;
 trainingLease.route.adopted=false;assert(trainingExternalGamma(0x4AC80,0x40000001)==0x40000001);trainingLease.route.adopted=true;
 trainingLeaseState[1]=0;assert(trainingExternalGamma(0x4AC80,0x40000001)==0x40000001);trainingLeaseState[1]=1;
 assert(backend.writes==colorWrites);
 auto nativeWrites=backend.writes;backend.regs[0x71008]=0xC0000000;
 assert(!prepareNativeTrainingPath(port)&&backend.writes==nativeWrites);backend.regs[0x71008]=0;
 backend.regs[0x61540]=0;
 portObj.memory[0x116]=0;put32(portObj.memory,0x128,10);put32(portObj.memory,0x124,4);
 assert(!trainingSetupPort(port)&&setConfigCalls==1&&setupCalls==1&&get32(portObj.memory,0x128)==20&&get32(portObj.memory,0x124)==2);
 portObj.memory[0x55A]=10;portObj.memory[0x563]=1;auto savedSetCalls=setConfigCalls;
 assert(!trainingSetupPort(port)&&setConfigCalls==savedSetCalls&&get32(portObj.memory,0x128)==10&&get32(portObj.memory,0x124)==1);
 portObj.memory[0x55A]=20;portObj.memory[0x563]=2;put32(portObj.memory,0x128,20);put32(portObj.memory,0x124,2);
 uint8_t reported[28];backend.regs[0x64500]=0x80203642;
 assert(!trainingGetLink(pathMem,reported)&&reported[2]==20&&reported[11]==2&&reported[16]==20&&reported[17]==2&&reported[3]==0xCC&&getLinkCalls==1);
 backend.regs[0x64500]=0xC0;assert(!trainingGetLink(pathMem,reported)&&reported[2]==0xCC&&trainingNegotiated[21]==1);
 setupResult=0xE00002BC;assert(trainingSetupPort(port)==setupResult);setupResult=0;
 assert(trainingExternalClock(&controller,&fbObj,pathMem,0x60000123)==0x80000123);
 assert(trainingExternalClock(&controller,&portObj,pathMem,0x60000123)==0x60000123);
 assert(trainingExternalClock(&controller,&fbObj,pathMem,0x10000123)==0x10000123);
 assert(trainingExternalNeedsMode(&controller,&fbObj,pathMem)&&trainingLink[11]==1);
 assert(!trainingExternalNeedsMode(&controller,&portObj,pathMem)&&!trainingExternalNeedsMode(&portObj,&fbObj,pathMem));
 backend.regs[0x71008]=0xC0000000;assert(!trainingExternalNeedsMode(&controller,&fbObj,pathMem));backend.regs[0x71008]=0;
 assert(!trainingRelease());trainingPutDDI(&powerObj,5);assert(backend.regs[0x45454]==3&&powerObj.refs==1);
 livePortLease.owner=(uintptr_t)framebufferOwner;livePortLease.active=false;
 trainingFree(pathMem);assert(trainingLease.power&&powerObj.refs==1&&!deferredReleases);trainingPutPG(&powerObj,3);
 assert(deferredReleases==1&&!livePortLease.owner&&trainingDeferredPort[1]==1&&!trainingDeferredPort[2]&&trainingDeferredPort[3]&&!trainingDeferredPort[4]);
 assert(!trainingLease.power&&!fbObj.refs&&!powerObj.refs&&!portObj.refs&&backend.regs[0x45404]==0xC0F);
 assert(!trainingNegotiatedValid);
 put32(fbObj.memory,0x49E8,0);trainingGetDDI(&powerObj,5);trainingLease.freed=true;assert(!prepareTrainingPath(port)&&trainingLeaseState[23]==1);assert(trainingLease.route.prepare(io,1)==RaptorLakeP::TrainingRoute::Status::Ready);
 assert(trainingRelease()&&!trainingLease.power&&backend.regs[0x46144]==0x80000000&&backend.regs[0x61400]==0x32000002);
 trainingPutDDI(&powerObj,5);assert(!fbObj.refs&&!powerObj.refs&&!portObj.refs&&!lock.depth);
 trainingGetDDI(&powerObj,5);assert(!prepareTrainingPath(port));assert(trainingLease.route.prepare(io,2)==RaptorLakeP::TrainingRoute::Status::Ready);
 uint8_t endRequest[12]{};put32(endRequest,0,6);
 assert(!trainingEnd(&controller,endRequest)&&trainingLease.path&&trainingLease.logicalPG);
 auto oldToken=pendingToken;assert(!prepareTrainingPath(port));trainingCleanupWorker(nullptr,oldToken);assert(trainingLease.path&&trainingCleanupState[3]==1);
 assert(trainingLease.route.prepare(io,1)==RaptorLakeP::TrainingRoute::Status::Ready);assert(!trainingEnd(&controller,endRequest));
 trainingPutDDI(&powerObj,5);trainingCleanupWorker(nullptr,pendingToken);
 assert(!trainingLease.path&&!trainingLease.power&&!fbObj.refs&&!powerObj.refs&&!portObj.refs&&!lock.depth);

}
'''.replace('BODY',body).replace('void*pathLookupOriginal;','uintptr_t pathLookupOriginal;').replace('pathLookupOriginal=(void*)lookup','pathLookupOriginal=(uintptr_t)lookup')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.cpp').write_text(f);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'t.cpp'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
print('PASS production training lease: owner/workloop/power/FB rejection, pipe B reservation, native adoption, balanced logical/physical power, ownership preservation and failed-training cleanup')
