// Private-donor integration; included only inside the reference backend's
// anonymous namespace after TC3 ownership helpers. No user addresses or modes.
mach_vm_address_t trainingAllocateAnyOriginal,trainingAllocateFixedOriginal,trainingFreeOriginal;
mach_vm_address_t trainingResourcesOriginal,trainingPGGet,trainingPGPutOriginal,trainingEndOriginal,trainingPortWriteOriginal;
uint32_t trainingLeaseState[24]{1};
struct TrainingPathLease {
 void *path{},*port{};IOService *fb{};OSObject *power{};
 bool logicalPG{},adopted{},freed{},releasing{},busy{};
 thread_t adopting{};
 Display13Power::State powerState;
 Display13Power::Lease powerLease;
 RaptorLakeP::TrainingRoute::Route route;
} trainingLease;
struct TrainingPowerIO {
 bool readable(uint32_t r){return r==Display13Power::Driver || r==Display13Power::DDI || r==Display13Power::AUX || r==Display13Power::Fuses;}
 uint32_t read(uint32_t r){return backend.readReg32(r);}
 void write(uint32_t r,uint32_t v){backend.writeReg32(r,v);}
 void delayUs(unsigned n){IODelay(n);}
 bool powerHeld(){
  using namespace Display13Power;auto &s=trainingLease.powerState;LivePortIO portIO{trainingLease.port};
  return trainingLease.powerLease.owner==&s && !s.faulted && s.refs[static_cast<unsigned>(Domain::PipeB)] &&
   (read(Driver)&0x3000U)==0x3000U && livePortLease.witness(portIO,reinterpret_cast<uintptr_t>(framebufferOwner),reinterpret_cast<uintptr_t>(trainingLease.port));
 }
};
bool trainingWorkloop(){auto loop=framebufferOwner?framebufferOwner->getWorkLoop():nullptr;return loop&&loop->inGate();}
uint32_t trainingPGRefs(){uint32_t v=~0U;if(trainingLease.power)memcpy(&v,reinterpret_cast<uint8_t *>(trainingLease.power)+0x28,4);return v;}
bool trainingHardwareIdle(){
 if(!trainingLease.powerLease.owner)return false;
 TrainingPowerIO io;
 // Disconnection invalidates the AUX witness but not the separately held
 // PipeB lease. Cleanup reads only that powered bank and the owned DDI.
 auto core=io.read(0x45404);if(core==~0U || (core&0x3000)!=0x3000)return false;
 auto b=io.read(0x64500),p=io.read(0x71008),f=io.read(0x61400),t=io.read(0x61540);
 return b!=~0U&&p!=~0U&&f!=~0U&&t!=~0U && (b&0x80000080U)==0x80U && !(p&0xC0000000U) && !((f|t)&0x80000000U);
}
void trainingCleanupPower(){
 if(!trainingLease.power || !trainingLease.freed || trainingLease.logicalPG || trainingPGRefs() || !trainingHardwareIdle())return;
 TrainingPowerIO io;auto result=Display13Power::release(io,trainingLease.powerState,trainingLease.powerLease);
 trainingLeaseState[17]=static_cast<uint32_t>(result.status);
 if(result.status!=Display13Power::Status::Complete)return;
 trainingLease.fb->release();trainingLease.power->release();static_cast<OSObject *>(trainingLease.port)->release();
 trainingLease.fb=nullptr;trainingLease.power=nullptr;trainingLease.port=nullptr;trainingLease.path=nullptr;
 trainingLease.freed=false;trainingLease.adopted=false;trainingLease.route={};trainingLease.powerState={};++trainingLeaseState[18];
}
void trainingGetPG(void *power,uint32_t pg){
 IORecursiveLockLock(livePortLock);reinterpret_cast<void (*)(void *,uint32_t)>(trainingPGGet)(power,pg);IORecursiveLockUnlock(livePortLock);
}
void trainingPutPG(void *power,uint32_t pg){
 IORecursiveLockLock(livePortLock);reinterpret_cast<void (*)(void *,uint32_t)>(trainingPGPutOriginal)(power,pg);
 if(power==trainingLease.power && pg>=3)trainingCleanupPower();IORecursiveLockUnlock(livePortLock);
}
bool trainingRelease(){
 if(!trainingLease.path)return !trainingLease.power;
 if(trainingLease.adopted || trainingLease.busy || !trainingHardwareIdle())return false;
 TrainingPowerIO io;auto status=trainingLease.route.restore(io);trainingLeaseState[16]=static_cast<uint32_t>(status);
 if(status!=RaptorLakeP::TrainingRoute::Status::Ready)return false;
 trainingLease.releasing=true;reinterpret_cast<void (*)(void *)>(trainingFreeOriginal)(trainingLease.path);trainingLease.releasing=false;
 trainingLease.freed=true;
 if(trainingLease.logicalPG){trainingLease.logicalPG=false;trainingPutPG(trainingLease.power,3);}
 trainingCleanupPower();return !trainingLease.power;
}
uint32_t prepareTrainingPath(void *port){
 ++trainingLeaseState[2];
 if(!trainingLeaseState[1])return 0;
 if(!livePortSelected(port) || !trainingWorkloop())return 1;
 IORecursiveLockLock(livePortLock);
 auto finish=[&](uint32_t code){trainingLeaseState[3]=code;IORecursiveLockUnlock(livePortLock);return code;};
 if(trainingLease.busy || trainingLease.adopting || trainingLease.freed)return finish(2);
 if(trainingLease.path){if(trainingLease.port!=port)return finish(3);return finish(0);}
 // Existing ordinary native paths are never replaced or commandeered.
 if(reinterpret_cast<void *(*)(void *)>(pathLookupOriginal)(port))return finish(0);
 auto ctl=reinterpret_cast<uint8_t *>(framebufferOwner);uintptr_t access=0;OSObject *power=nullptr;uint32_t count=0;
 memcpy(&access,ctl+0xC40,8);memcpy(&power,ctl+0xC8,8);memcpy(&count,ctl+0xD5C,4);trainingLeaseState[4]=count;
 if(count<2 || count>3 || !access || !power || strcmp(power->getMetaClass()->getClassName(),"AppleIntelPowerWell") || power->getMetaClass()->getClassSize()!=0x80)return finish(4);
 uintptr_t portAccess=0,powerOwner=0;uint8_t always=0;memcpy(&portAccess,static_cast<uint8_t *>(port)+0x578,8);memcpy(&powerOwner,reinterpret_cast<uint8_t *>(power)+0x10,8);memcpy(&always,reinterpret_cast<uint8_t *>(power)+0x18,1);
 if(portAccess!=access || powerOwner!=reinterpret_cast<uintptr_t>(framebufferOwner) || always!=1)return finish(5);
 IOService *fb=nullptr;
 for(unsigned i=1;i<count;i++){
  IOService *candidate=nullptr;memcpy(&candidate,ctl+0xD60+8*i,8);if(!candidate)continue;
  auto meta=candidate->getMetaClass();if(strcmp(meta->getClassName(),"AppleIntelFramebufferDiags") || meta->getClassSize()!=0x4B98 || candidate->isInactive())continue;
  auto bytes=reinterpret_cast<uint8_t *>(candidate);uintptr_t controller=0,fbAccess=0;uint32_t index=0,reserved=0,assigned=0;
  memcpy(&controller,bytes+0x1D0,8);memcpy(&fbAccess,bytes+0x4A40,8);memcpy(&index,bytes+0x1DC,4);memcpy(&reserved,bytes+0x49E8,4);memcpy(&assigned,bytes+0x4A00,4);
  RaptorLakeP::TrainingRoute::Candidate c{true,controller==reinterpret_cast<uintptr_t>(framebufferOwner),fbAccess==access,bytes[0x1E9]!=0,bytes[0x44DE]!=0,bytes[0x1E7]!=0,meta->getClassSize(),index,reserved,assigned};
  if(RaptorLakeP::TrainingRoute::candidate(c)){fb=candidate;trainingLeaseState[5]=index;break;}
 }
 if(!fb)return finish(6);
 auto path=reinterpret_cast<void *(*)(uint32_t)>(trainingAllocateFixedOriginal)(1);if(!path)return finish(7);
 uint32_t pipe=~0U,mode=0;uintptr_t boundFB=0,boundPort=0;auto pb=static_cast<uint8_t *>(path);
 memcpy(&pipe,pb+0x3640,4);memcpy(&mode,pb+0x3644,4);memcpy(&boundFB,pb+0x32B0,8);memcpy(&boundPort,pb+0x3648,8);
 if(pipe!=1 || mode!=1 || boundPort){reinterpret_cast<void (*)(void *)>(trainingFreeOriginal)(path);return finish(8);}
 trainingLease.fb=fb;trainingLease.port=port;trainingLease.power=power;trainingLease.path=path;
 fb->retain();power->retain();static_cast<OSObject *>(port)->retain();
 TrainingPowerIO io;using namespace Display13Power;const auto domains=bit(Domain::PW1)|bit(Domain::PW2)|bit(Domain::PipeB);
 if(!initialize(io,trainingLease.powerState,domains)){trainingLease.freed=true;reinterpret_cast<void (*)(void *)>(trainingFreeOriginal)(path);fb->release();power->release();static_cast<OSObject *>(port)->release();trainingLease.path=nullptr;trainingLease.fb=nullptr;trainingLease.power=nullptr;trainingLease.port=nullptr;trainingLease.freed=false;return finish(9);}
 auto result=acquire(io,trainingLease.powerState,trainingLease.powerLease,domains);trainingLeaseState[6]=static_cast<uint32_t>(result.status);
 if(result.status!=Status::Complete){trainingLeaseState[7]=1;return finish(10);} // Retain any failed power cleanup for explicit recovery.
 if(!trainingHardwareIdle()){
  trainingLease.freed=true;reinterpret_cast<void (*)(void *)>(trainingFreeOriginal)(path);trainingLeaseState[7]=2;return finish(11);
 }
 reinterpret_cast<void (*)(void *,uint32_t)>(trainingPGGet)(power,3);trainingLease.logicalPG=true;
 memcpy(pb+0x32B0,&fb,8);memcpy(pb+0x3648,&port,8);trainingLeaseState[8]=pipe;trainingLeaseState[9]=1;
 return finish(0);
}
void *trainingAllocateAny(){
 IORecursiveLockLock(livePortLock);void *path;
 if(trainingLease.adopting==current_thread() && trainingLease.path && !trainingLease.adopted){path=trainingLease.path;trainingLease.adopted=true;++trainingLeaseState[12];}
 else path=reinterpret_cast<void *(*)()>(trainingAllocateAnyOriginal)();
 IORecursiveLockUnlock(livePortLock);return path;
}
void *trainingAllocateFixed(uint32_t pipe){
 IORecursiveLockLock(livePortLock);void *path;
 if(pipe==1 && trainingLease.adopting==current_thread() && trainingLease.path && !trainingLease.adopted){path=trainingLease.path;trainingLease.adopted=true;++trainingLeaseState[12];}
 else path=reinterpret_cast<void *(*)(uint32_t)>(trainingAllocateFixedOriginal)(pipe);
 IORecursiveLockUnlock(livePortLock);return path;
}
bool trainingResources(void *fb,uint32_t port,uint32_t stream){
 bool own=false;IORecursiveLockLock(livePortLock);
 if(trainingLease.path && port==5){
  if(stream || trainingLease.busy || trainingLease.adopting || trainingLease.adopted){IORecursiveLockUnlock(livePortLock);return false;}
  auto object=static_cast<IOService *>(fb);
  if(!object || object->isInactive() || strcmp(object->getMetaClass()->getClassName(),"AppleIntelFramebufferDiags") || object->getMetaClass()->getClassSize()!=0x4B98){IORecursiveLockUnlock(livePortLock);return false;}
  uintptr_t owner=0;memcpy(&owner,static_cast<uint8_t *>(fb)+0x1D0,8);
  if(owner!=reinterpret_cast<uintptr_t>(framebufferOwner)){IORecursiveLockUnlock(livePortLock);return false;}
  trainingLease.adopting=current_thread();own=true;++trainingLeaseState[11];
 }
 auto result=reinterpret_cast<bool (*)(void *,uint32_t,uint32_t)>(trainingResourcesOriginal)(fb,port,stream);
 if(own){
  trainingLease.adopting=nullptr;trainingLeaseState[13]=result;
  if(result && trainingLease.adopted){
   uintptr_t pathFB=0,pathPort=0;memcpy(&pathFB,static_cast<uint8_t *>(trainingLease.path)+0x32B0,8);memcpy(&pathPort,static_cast<uint8_t *>(trainingLease.path)+0x3648,8);
   if(pathFB==reinterpret_cast<uintptr_t>(fb) && pathPort==reinterpret_cast<uintptr_t>(trainingLease.port) && trainingPGRefs()>=2){
    trainingLease.route.adopted=true;if(trainingLease.logicalPG){trainingLease.logicalPG=false;trainingPutPG(trainingLease.power,3);}
   }else ++trainingLeaseState[22];
  }
 }
 IORecursiveLockUnlock(livePortLock);return result;
}
void trainingFree(void *path){
 IORecursiveLockLock(livePortLock);const bool own=path==trainingLease.path;
 reinterpret_cast<void (*)(void *)>(trainingFreeOriginal)(path);
 if(own && !trainingLease.releasing){trainingLease.freed=true;trainingLeaseState[14]++;trainingCleanupPower();}
 IORecursiveLockUnlock(livePortLock);
}
uint32_t trainingEnd(void *controller,void *request){
 auto result=reinterpret_cast<uint32_t (*)(void *,void *)>(trainingEndOriginal)(controller,request);
 uint32_t port=0,keep=~0U;if(request){memcpy(&port,request,4);memcpy(&keep,static_cast<uint8_t *>(request)+8,4);}
 if(controller==framebufferOwner && port==6 && !keep && !result){IORecursiveLockLock(livePortLock);trainingLeaseState[15]++;trainingRelease();IORecursiveLockUnlock(livePortLock);}
 return result;
}
void trainingPortWrite(void *port,uint32_t reg,uint32_t value){
 if(reg==0x64500 && livePortSelected(port)){
  IORecursiveLockLock(livePortLock);
  if(livePortLease.phy && livePortLease.object==reinterpret_cast<uintptr_t>(port)){
   auto before=backend.readReg32(reg);if(before==~0U || !(before&64)){++trainingLeaseState[20];IORecursiveLockUnlock(livePortLock);return;}
   value|=64;++trainingLeaseState[19];
  }
  reinterpret_cast<void (*)(void *,uint32_t,uint32_t)>(trainingPortWriteOriginal)(port,reg,value);
  IORecursiveLockUnlock(livePortLock);return;
 }
 reinterpret_cast<void (*)(void *,uint32_t,uint32_t)>(trainingPortWriteOriginal)(port,reg,value);
}
