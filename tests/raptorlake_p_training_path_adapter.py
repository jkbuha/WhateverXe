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
using mach_vm_address_t=uintptr_t;using thread_t=void*;
thread_t current_thread(){return (void*)7;}
struct Lock{unsigned depth=0;}lock,*livePortLock=&lock;
void IORecursiveLockLock(Lock*p){assert(p==&lock);++p->depth;}
void IORecursiveLockUnlock(Lock*p){assert(p==&lock&&p->depth);--p->depth;}
struct Meta {const char *name;unsigned size;const char*getClassName(){return name;}unsigned getClassSize(){return size;}};
struct Loop {bool gated=true;bool inGate(){return gated;}}loop;
struct OSObject {
 alignas(16) uint8_t memory[0x4B98]{};Meta meta{"AppleIntelFramebufferDiags",0x4B98};int refs=0;
 Meta*getMetaClass(){return &meta;}bool isInactive(){return false;}Loop*getWorkLoop(){return &loop;}
 void retain(){++refs;}void release(){assert(refs);--refs;}
};using IOService=OSObject;
OSObject controller,portObj,fbObj,powerObj;IOService*framebufferOwner=&controller;void*port=&portObj;
struct Backend {
 std::map<uint32_t,uint32_t> regs{{0x45404,0xC0F},{0x45454,3},{0x45444,0xC03},{0x42000,0x8E380000},{0x46144,0},{0x61400,0},{0x61540,0},{0x64500,0xC0},{0x71008,0},{0x16E8C0,3},{0x16E8A0,0x1C},{0x1010A0,0xAABBCCDD},{0x16AD00,0x1234}};unsigned writes=0;
 uint32_t readReg32(uint32_t r){assert(regs.count(r));return regs[r];}
 void writeReg32(uint32_t r,uint32_t v){assert(regs.count(r));++writes;if(r==0x45404){auto mask=0xA80AU;regs[r]=(v&~(mask>>1))|((v&mask)>>1);}else regs[r]=v;}
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
struct LiveLease{bool phy=true;uintptr_t object;bool witness(LivePortIO&,uintptr_t owner,uintptr_t p){return healthy&&owner==(uintptr_t)framebufferOwner&&p==(uintptr_t)port;}}livePortLease;
bool livePortSelected(void*p){return allowed&&p==port;}
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
uint32_t end(void*,void*){return 0;}
void portWrite(void*p,uint32_t reg,uint32_t v){assert(p==port);++originalWrites;backend.writeReg32(reg,v);}
bool trainingResourcesNative(void*,uint32_t,uint32_t);
uint32_t trainingConfiguredLanes(void*);
unsigned voltageCalls=0;RaptorLakeP::ReferenceVoltage::Drive lastDrive{};
uint32_t configuredNative(void*){return 0xC;}
void voltageNative(void*p,uint32_t,uint32_t,void*data){assert(p==port);++voltageCalls;assert(data&&trainingConfiguredLanes(p)==15);memcpy(&lastDrive,data,sizeof(lastDrive));}
uint32_t patternNative(void*,uint32_t p){backend.regs[0x61540]=0x80000000|(p<<8);return 0;}

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
 trainingLeaseState[1]=1;
 allowed=false;assert(prepareTrainingPath(port)==1&&!allocs&&!backend.writes);allowed=true;
 loop.gated=false;assert(prepareTrainingPath(port)==1&&!allocs);loop.gated=true;
 fbObj.memory[0x1E9]=1;assert(prepareTrainingPath(port)==6&&!allocs);fbObj.memory[0x1E9]=0;
 powerObj.memory[0x18]=0;assert(prepareTrainingPath(port)==5&&!allocs);powerObj.memory[0x18]=1;
 assert(!prepareTrainingPath(port)&&trainingLease.path==pathMem&&trainingLease.logicalPG&&trainingPGRefs()==1);
 assert(backend.regs[0x45404]==0x3C0F&&fbObj.refs==1&&powerObj.refs==1&&portObj.refs==1);
 assert(!prepareTrainingPath(port)&&allocs==1);TrainingPowerIO io;assert(io.powerHeld());
 assert(trainingLease.route.prepare(io,2)==RaptorLakeP::TrainingRoute::Status::Ready);
 assert(trainingConfiguredLanes(port)==12);trainingVoltage(port,0,1,nullptr);
 assert(voltageCalls==1&&lastDrive.swing==5&&lastDrive.emphasis==4&&lastDrive.preshoot==0&&trainingConfiguredLanes(port)==12&&!liveVoltageThread&&!liveVoltagePort);
 trainingVoltage(port,3,3,nullptr);assert(voltageCalls==1&&trainingSignal[11]==1);
 assert(!trainingPattern(pathMem,1)&&backend.regs[0x61540]==0x80040100&&trainingSignal[15]==1);
 backend.regs[0x61540]=0;
 trainingPortWrite(port,0x64500,0x80000080);assert(backend.regs[0x64500]==0x800000C0);
 auto wc=originalWrites;backend.regs[0x64500]&=~64U;trainingPortWrite(port,0x64500,0);assert(originalWrites==wc);backend.regs[0x64500]=0xC0;
 trainingLease.busy=true;assert(!trainingResources(&fbObj,5,0));trainingLease.busy=false;
 assert(!trainingResources(&fbObj,5,1));assert(!trainingResources(&fbObj,4,0));
 assert(trainingResources(&fbObj,5,0)&&trainingLease.adopted&&trainingLease.route.adopted&&!trainingLease.logicalPG&&trainingPGRefs()==1&&allocs==1);
 assert(!trainingRelease());trainingFree(pathMem);assert(trainingLease.power&&powerObj.refs==1);trainingPutPG(&powerObj,3);
 assert(!trainingLease.power&&!fbObj.refs&&!powerObj.refs&&!portObj.refs&&backend.regs[0x45404]==0xC0F);
 put32(fbObj.memory,0x49E8,0);trainingLease.freed=true;assert(!prepareTrainingPath(port)&&trainingLeaseState[23]==1);assert(trainingLease.route.prepare(io,1)==RaptorLakeP::TrainingRoute::Status::Ready);
 assert(trainingRelease()&&!trainingLease.power&&backend.regs[0x46144]==0x60000000&&backend.regs[0x61400]==0x32000002);
 assert(!fbObj.refs&&!powerObj.refs&&!portObj.refs&&!lock.depth);
}
'''.replace('BODY',body).replace('void*pathLookupOriginal;','uintptr_t pathLookupOriginal;').replace('pathLookupOriginal=(void*)lookup','pathLookupOriginal=(uintptr_t)lookup')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.cpp').write_text(f);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'t.cpp'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
print('PASS production training lease: owner/workloop/power/FB rejection, pipe B reservation, native adoption, balanced logical/physical power, ownership preservation and failed-training cleanup')
