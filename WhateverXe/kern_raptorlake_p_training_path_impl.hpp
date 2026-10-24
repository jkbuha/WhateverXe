// Private-donor integration; included only inside the reference backend's
// anonymous namespace after TC3 ownership helpers. No user addresses or modes.
mach_vm_address_t trainingAllocateAnyOriginal,trainingAllocateFixedOriginal,trainingFreeOriginal;
mach_vm_address_t trainingResourcesOriginal,trainingPGGet,trainingPGPutOriginal,trainingEndOriginal,trainingPortWriteOriginal;
uint32_t trainingLeaseState[32]{2};
mach_vm_address_t trainingConfiguredOriginal,trainingVoltageOriginal,trainingPatternOriginal;
uint32_t trainingSignal[20]{1},trainingLink[16]{1};
mach_vm_address_t trainingDDIGetOriginal,trainingDDIPutOriginal;
RaptorLakeP::ReferenceDDI::Request trainingDDIRequest;
uint32_t trainingDDIState[12]{1},trainingCleanupState[8]{1};
thread_call_t trainingCleanupWork;uint64_t trainingGeneration;

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
 auto core=io.read(0x45404);if(core==~0U || (core&0x300F)!=0x300F)return false;
 auto b=io.read(0x64500),p=io.read(0x71008),f=io.read(0x61400),t=io.read(0x61540);
 return b!=~0U&&p!=~0U&&f!=~0U&&t!=~0U && (b&0x80000080U)==0x80U && !(p&0xC0000000U) && !((f|t)&0x80000000U);
}

struct TrainingDDIIO {
 OSObject *power;
 bool qualified(){
  if(!power || !framebufferOwner || !wexRplReferenceFBAllowed(framebufferOwner) ||
   strcmp(power->getMetaClass()->getClassName(),"AppleIntelPowerWell") || power->getMetaClass()->getClassSize()!=0x80)return false;
  uintptr_t owner=0,bound=0;uint8_t always=0;memcpy(&owner,reinterpret_cast<uint8_t *>(power)+0x10,8);
  memcpy(&bound,reinterpret_cast<uint8_t *>(framebufferOwner)+0xC8,8);memcpy(&always,reinterpret_cast<uint8_t *>(power)+0x18,1);
  return owner==reinterpret_cast<uintptr_t>(framebufferOwner) && bound==reinterpret_cast<uintptr_t>(power) && always==1;
 }
 bool parentHeld(){auto c=backend.readReg32(0x45404);return c!=~0U && (c&15)==15;}
 uint32_t refs(){uint32_t v=~0U;memcpy(&v,reinterpret_cast<uint8_t *>(power)+0x44,4);return v;}
 bool idle(){auto b=backend.readReg32(0x64500);return b!=~0U && (b&0x80000080U)==0x80;}
 uint32_t read(uint32_t reg){return backend.readReg32(reg);}
 void write(uint32_t reg,uint32_t value){backend.writeReg32(reg,value);}
 void delayUs(unsigned us){IODelay(us);}
 void retainOwner(){power->retain();}void releaseOwner(){power->release();}
};
void trainingDDICleanup(){
 if(!trainingDDIRequest.owner)return;
 auto power=reinterpret_cast<OSObject *>(trainingDDIRequest.owner);TrainingDDIIO io{power};
 trainingDDIState[8]=io.refs();
 auto result=trainingDDIRequest.put(io,reinterpret_cast<uintptr_t>(power));trainingDDIState[9]=static_cast<uint32_t>(result);
 trainingDDIState[10]=trainingDDIRequest.owned;
}
void trainingGetDDI(void *object,uint32_t ddi){
 IORecursiveLockLock(livePortLock);reinterpret_cast<void (*)(void *,uint32_t)>(trainingDDIGetOriginal)(object,ddi);
 if(ddi==5 && livePortReady && livePortPermission){
  auto port=getPortEntry?reinterpret_cast<OSObject *(*)(uint32_t)>(getPortEntry)(5):nullptr;
  if(livePortSelected(port)){
   ++trainingDDIState[1];TrainingDDIIO io{static_cast<OSObject *>(object)};LivePortIO portIO{port};
   if(io.qualified() && livePortLease.witness(portIO,reinterpret_cast<uintptr_t>(framebufferOwner),reinterpret_cast<uintptr_t>(port))){
    trainingDDIState[3]=io.refs();trainingDDIState[4]=io.read(0x45454);
    auto result=trainingDDIRequest.get(io,reinterpret_cast<uintptr_t>(object));trainingDDIState[5]=static_cast<uint32_t>(result);
    trainingDDIState[6]=io.read(0x45454);trainingDDIState[10]=trainingDDIRequest.owned;
   }else ++trainingDDIState[11];
  }
 }
 IORecursiveLockUnlock(livePortLock);
}
void trainingPutDDI(void *object,uint32_t ddi){
 IORecursiveLockLock(livePortLock);reinterpret_cast<void (*)(void *,uint32_t)>(trainingDDIPutOriginal)(object,ddi);
 if(ddi==5 && trainingDDIRequest.owner==reinterpret_cast<uintptr_t>(object)){++trainingDDIState[2];trainingDDICleanup();}
 IORecursiveLockUnlock(livePortLock);
}
void trainingCleanupPower(){
 if(!trainingLease.power || !trainingLease.freed || trainingLease.logicalPG || trainingLease.busy || trainingLease.adopting || trainingPGRefs() || !trainingHardwareIdle())return;
 TrainingPowerIO io;auto result=Display13Power::release(io,trainingLease.powerState,trainingLease.powerLease);
 trainingLeaseState[17]=static_cast<uint32_t>(result.status);
 if(result.status!=Display13Power::Status::Complete)return;
 trainingLease.fb->release();trainingLease.power->release();static_cast<OSObject *>(trainingLease.port)->release();
 trainingLease.fb=nullptr;trainingLease.power=nullptr;trainingLease.port=nullptr;trainingLease.path=nullptr;
 trainingLease.freed=false;trainingLease.adopted=false;trainingLease.releasing=false;trainingLease.busy=false;trainingLease.adopting=nullptr;trainingLease.route={};trainingLease.powerState={};++trainingLeaseState[18];
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
 IORecursiveLockLock(livePortLock);++trainingGeneration;
 auto finish=[&](uint32_t code){trainingLeaseState[3]=code;IORecursiveLockUnlock(livePortLock);return code;};
 if(trainingLease.freed && !trainingLease.path && !trainingLease.port && !trainingLease.power && !trainingLease.powerLease.owner && !trainingLease.logicalPG && !trainingLease.busy && !trainingLease.adopting){trainingLease.freed=false;++trainingLeaseState[23];}
 if(trainingLease.busy || trainingLease.adopting || trainingLease.freed)return finish(2);
 auto ddiPower=backend.readReg32(0x45454);if(ddiPower==~0U || (ddiPower&0xC00)!=0xC00)return finish(12);
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
IOReturn trainingCleanupAction(OSObject *owner,void *token,void *,void *,void *){
 if(owner!=framebufferOwner || !trainingWorkloop())return kIOReturnNotReady;
 IORecursiveLockLock(livePortLock);++trainingCleanupState[1];
 if(reinterpret_cast<uintptr_t>(token)==trainingGeneration && !trainingLease.busy && !trainingLease.adopted){
  trainingCleanupState[4]=trainingRelease()?0:1;
 }else ++trainingCleanupState[3];
 IORecursiveLockUnlock(livePortLock);return kIOReturnSuccess;
}
void trainingCleanupWorker(thread_call_param_t,thread_call_param_t token){
 auto owner=framebufferOwner;if(!owner || owner->isInactive())return;owner->retain();
 auto loop=owner->getWorkLoop();if(loop){loop->retain();loop->runAction(trainingCleanupAction,owner,token);loop->release();}owner->release();
}
uint32_t trainingEnd(void *controller,void *request){
 auto result=reinterpret_cast<uint32_t (*)(void *,void *)>(trainingEndOriginal)(controller,request);
 uint32_t port=0,keep=~0U;if(request){memcpy(&port,request,4);memcpy(&keep,static_cast<uint8_t *>(request)+8,4);}
 if(controller==framebufferOwner && port==6 && !keep && !result){
  IORecursiveLockLock(livePortLock);trainingLeaseState[15]++;
  if(trainingCleanupWork && trainingLease.path && !trainingLease.adopted && !trainingLease.busy && trainingHardwareIdle()){
   TrainingPowerIO io;auto status=trainingLease.route.restore(io);trainingLeaseState[16]=static_cast<uint32_t>(status);
   if(status==RaptorLakeP::TrainingRoute::Status::Ready){
    auto generation=++trainingGeneration;uint64_t deadline;clock_interval_to_deadline(2000,kMillisecondScale,&deadline);
    thread_call_enter1_delayed(trainingCleanupWork,reinterpret_cast<thread_call_param_t>(static_cast<uintptr_t>(generation)),deadline);++trainingCleanupState[2];
   }
  }
  IORecursiveLockUnlock(livePortLock);
 }
 return result;
}

void trainingPortWrite(void *port,uint32_t reg,uint32_t value){
 if(reg==0x64500 && livePortSelected(port)){
  IORecursiveLockLock(livePortLock);
  if(livePortLease.phy && livePortLease.object==reinterpret_cast<uintptr_t>(port)){
   auto before=backend.readReg32(reg);if(before==~0U || !(before&64)){++trainingLeaseState[20];IORecursiveLockUnlock(livePortLock);return;}
   value|=64;++trainingLeaseState[19];
   if(value&0x80000000U){
    uint32_t rate=0;memcpy(&rate,static_cast<uint8_t *>(port)+0x128,4);
    ++trainingLink[1];trainingLink[2]=rate;trainingLink[3]=value;trainingLink[5]=before;
    if(!RaptorLakeP::TrainingRoute::bufferLinkRate(value,rate,value)){
     ++trainingLink[8];IORecursiveLockUnlock(livePortLock);return;
    }
    trainingLink[4]=value;trainingLink[6]=backend.readReg32(0x46114);trainingLink[7]=backend.readReg32(0x46144);
   }
  }
  reinterpret_cast<void (*)(void *,uint32_t,uint32_t)>(trainingPortWriteOriginal)(port,reg,value);
  trainingLink[9]=backend.readReg32(reg);
  trainingDDICleanup();IORecursiveLockUnlock(livePortLock);return;
 }
 reinterpret_cast<void (*)(void *,uint32_t,uint32_t)>(trainingPortWriteOriginal)(port,reg,value);
}

uint32_t trainingConfiguredLanes(void *port){
 if(!livePortSelected(port))return reinterpret_cast<uint32_t (*)(void *)>(trainingConfiguredOriginal)(port);
 IORecursiveLockLock(livePortLock);LivePortIO io{port};uint32_t mask=0;
 if(livePortLease.witness(io,reinterpret_cast<uintptr_t>(framebufferOwner),reinterpret_cast<uintptr_t>(port))){
  auto raw=io.read(0x16E8C0),assigned=io.read(0x16E8A0);trainingSignal[1]=raw;trainingSignal[18]=assigned;
  if(raw!=~0U && assigned!=~0U)mask=RaptorLakeP::ReferenceVoltage::physicalMask(raw&15U,assigned&15U);
  if(mask && liveVoltageThread==current_thread() && liveVoltagePort==port)mask=15;
 }
 trainingSignal[2]=mask;IORecursiveLockUnlock(livePortLock);return mask;
}
void trainingVoltage(void *port,uint32_t swing,uint32_t emphasis,void *custom){
 if(!livePortSelected(port) || custom){reinterpret_cast<void (*)(void *,uint32_t,uint32_t,void *)>(trainingVoltageOriginal)(port,swing,emphasis,custom);return;}
 IORecursiveLockLock(livePortLock);++trainingSignal[3];trainingSignal[4]=swing;trainingSignal[5]=emphasis;
 uint32_t rate=0;memcpy(&rate,static_cast<uint8_t *>(port)+0x128,4);trainingSignal[6]=rate;
 RaptorLakeP::ReferenceVoltage::Drive drive{};Display13TC::Port coordinates;Display13TC::Port::resolve(5,coordinates);LivePhyIO io{port};
 bool valid=RaptorLakeP::ReferenceVoltage::select(swing,emphasis,rate,drive);
 auto status=valid?RaptorLakeP::ReferenceVoltage::wake(io,coordinates):Display13TC::Status::Invalid;trainingSignal[7]=static_cast<uint32_t>(status);
 if(status==Display13TC::Status::Complete){
  auto priorThread=liveVoltageThread;auto priorPort=liveVoltagePort;liveVoltageThread=current_thread();liveVoltagePort=port;
  trainingSignal[8]=drive.swing;trainingSignal[9]=drive.preshoot;trainingSignal[10]=drive.emphasis;
  reinterpret_cast<void (*)(void *,uint32_t,uint32_t,void *)>(trainingVoltageOriginal)(port,swing,emphasis,&drive);
  liveVoltageThread=priorThread;liveVoltagePort=priorPort;
 }else ++trainingSignal[11];
 IORecursiveLockUnlock(livePortLock);
}
uint32_t trainingPattern(void *path,uint32_t pattern){
 auto result=reinterpret_cast<uint32_t (*)(void *,uint32_t)>(trainingPatternOriginal)(path,pattern);
 if(result)return result;
 IORecursiveLockLock(livePortLock);
 if(path!=trainingLease.path || !trainingLease.port){IORecursiveLockUnlock(livePortLock);return result;}
 TrainingPowerIO io;
 auto port=static_cast<uint8_t *>(trainingLease.port);uint8_t mst=0,enhanced=0;memcpy(&mst,port+0x117,1);memcpy(&enhanced,port+0x11A,1);
 if(pattern<=5 && !mst && enhanced<=1 && io.powerHeld()){
  auto before=io.read(0x61540);trainingSignal[12]=before;
  if(before!=~0U){auto wanted=RaptorLakeP::ReferenceVoltage::framing(before,enhanced);if(wanted!=before){io.write(0x61540,wanted);++trainingSignal[15];}
   auto after=io.read(0x61540);trainingSignal[13]=after;trainingSignal[14]=enhanced;if(after!=wanted)++trainingSignal[16];
  }
 }else ++trainingSignal[17];
 IORecursiveLockUnlock(livePortLock);return result;
}
uint32_t trainingExternalClock(const void *owner,const void *head,const void *path,uint32_t value){
 if(owner!=framebufferOwner || !head || !path || !livePortLock || (value&0xF0000000U)!=0x60000000U)return value;
 IORecursiveLockLock(livePortLock);
 if(trainingLease.adopted && path==trainingLease.path && head==trainingLease.fb && livePortSelected(trainingLease.port)){
  TrainingPowerIO io;
  if(io.powerHeld()){
   ++trainingLink[13];trainingLink[14]=value;value=(value&0x0FFFFFFFU)|0x80000000U;trainingLink[15]=value;
  }
 }
 IORecursiveLockUnlock(livePortLock);return value;
}
bool trainingExternalNeedsMode(const void *owner,const void *head,const void *path){
 if(owner!=framebufferOwner || !head || !path || !livePortLock)return false;
 IORecursiveLockLock(livePortLock);bool needs=false;
 if(trainingLease.adopted && path==trainingLease.path && head==trainingLease.fb && livePortSelected(trainingLease.port)){
  TrainingPowerIO io;
  if(io.powerHeld()){
   auto conf=io.read(0x71008);trainingLink[12]=conf;++trainingLink[10];
   needs=conf!=~0U && !(conf&0x80000000U);if(needs)++trainingLink[11];
  }
 }
 IORecursiveLockUnlock(livePortLock);return needs;
}
void publishTrainingLease(){
 IORecursiveLockLock(livePortLock);
 trainingLeaseState[24]=(trainingLease.path?1U:0U)|(trainingLease.port?2U:0U)|(trainingLease.power?4U:0U)|(trainingLease.logicalPG?8U:0U)|
  (trainingLease.adopted?16U:0U)|(trainingLease.freed?32U:0U)|(trainingLease.releasing?64U:0U)|(trainingLease.busy?128U:0U)|(trainingLease.adopting?256U:0U);
 trainingLeaseState[25]=trainingPGRefs();trainingLeaseState[26]=trainingLease.powerState.owned;trainingLeaseState[27]=trainingLease.powerState.borrowed;
 trainingLeaseState[28]=trainingLease.powerState.faulted;trainingLeaseState[29]=trainingLease.powerLease.owner?1:0;
 trainingLeaseState[30]=trainingLease.powerState.refs[static_cast<unsigned>(Display13Power::Domain::PipeB)];trainingLeaseState[31]=trainingLease.powerState.ready;
 target->setProperty("WhateverXeRPLPReferenceTrainingLease",trainingLeaseState,sizeof(trainingLeaseState));
 target->setProperty("WhateverXeRPLPReferenceSignal",trainingSignal,sizeof(trainingSignal));
 target->setProperty("WhateverXeRPLPReferenceLink",trainingLink,sizeof(trainingLink));
 target->setProperty("WhateverXeRPLPReferenceDDI",trainingDDIState,sizeof(trainingDDIState));
 target->setProperty("WhateverXeRPLPReferenceTrainingCleanup",trainingCleanupState,sizeof(trainingCleanupState));IORecursiveLockUnlock(livePortLock);
}
