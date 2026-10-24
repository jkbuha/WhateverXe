"""Execute actual scoped AGDC callback, pointer binding proof and metadata copy."""
from pathlib import Path
import subprocess,tempfile,sys,hashlib
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('mach_vm_address_t agdcCallbackOriginal,'):s.index('bool nativeRescanPermitted(){')];body=body.replace('#include "kern_raptorlake_p_training_path_impl.hpp"','''struct TrainingPowerIO {};
namespace RaptorLakeP {namespace TrainingRoute {enum class Status {Ready};}}
struct TrainingLeaseMock {void *path{};bool adopted{},busy{};struct Route {RaptorLakeP::TrainingRoute::Status prepare(TrainingPowerIO&,unsigned){return RaptorLakeP::TrainingRoute::Status::Ready;}}route;}trainingLease;
uint32_t trainingLeaseState[24]{};void *livePortLock=nullptr;
void IORecursiveLockLock(void*){}void IORecursiveLockUnlock(void*){}uint32_t prepareTrainingPath(void*){return 0;}void publishTrainingLease(){}''');resolver=s[s.index('bool wexRplReferenceAGDCObservationHooks('):s.index('bool wexRplReferenceExternalPort(')]
f=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <vector>
using mach_vm_address_t=uintptr_t;constexpr uint32_t kIOReturnNotReady=0xE00002D8;
alignas(16) uint8_t controller[0x1C48]{},portBytes[0x700]{};
void*framebufferOwner=controller;bool ownerOK=true,capturable=true,portOK=true,flag=true,imageOK=true,routeOK=true;uint32_t ddi=5,state[9]{},livePortReady=1;
struct OSObject{};
uint32_t refs=1;struct Lease{bool auxRequest=true;}livePortLease;
OSObject*port=reinterpret_cast<OSObject*>(portBytes);
OSObject*getPort(unsigned n){assert(n==5);return port;}uintptr_t getPortEntry=reinterpret_cast<uintptr_t>(getPort);
bool observerPort(void*p,uint32_t&d){d=ddi;return portOK&&p==port;}
bool wexRplReferenceFBAllowed(void*o){return ownerOK&&o==framebufferOwner;}
bool captureAllowed(){return capturable;}
bool livePortSelected(void*p){return ownerOK&&portOK&&p==port&&ddi==5;}
uint32_t nativeAuxBypassRefs(void*p){assert(p==port);return refs;}
struct Target{std::vector<uint32_t> meta,rows;void setProperty(const char*n,void*p,unsigned bytes){if(!strcmp(n,"WhateverXeRPLPReferenceTrainingLease")){assert(bytes==128);return;}auto&out=!strcmp(n,"WhateverXeRPLPReferenceAGDCMeta")?meta:rows;out.assign((uint32_t*)p,(uint32_t*)p+bytes/4);}} targetObject,*target=&targetObject;
unsigned delegates=0;uint32_t returnValue=0;void*lastOwner;unsigned long lastArgument;
uint32_t delegate(void*o,uint32_t e,unsigned long a,uint32_t v){++delegates;lastOwner=o;lastArgument=a;assert(e==3&&v==7);return returnValue;}
void*expectedPath=(void*)0x1234;unsigned pathDelegates=0,trainingDelegates=0;
void *pathDelegate(void*p){assert(p==port);++pathDelegates;return expectedPath;}
uint32_t trainDelegate(void*p,void*q){assert(p==port&&q);++trainingDelegates;uint32_t v=0x8001;memcpy((uint8_t*)q+0x38,&v,4);return 0xE00002BC;}
BODY
bool checkKernelArgument(const char*){return flag;}bool wexRplReferenceImage(void*,size_t,bool){return imageOK;}
uintptr_t fakeEntry=0;unsigned routes=0;
struct KernelPatcher{
 struct RouteRequest{template<class F>RouteRequest(const char*,F,uintptr_t&){}};
 uintptr_t solveSymbol(unsigned,const char*n,uintptr_t,size_t){
  if(!strcmp(n,"__ZN21AppleIntelDisplayPath18getFirstPathByPortEP14AppleIntelPort"))return fakeEntry+0xE9AAA-0x9F9CA;
  if(!strcmp(n,"__ZN14AppleIntelPort12linkTrainingEP18AGDCDPPortConfig_t"))return fakeEntry+0x26C54-0x9F9CA;
  return fakeEntry;}void clearError(){}
 bool routeMultiple(unsigned,RouteRequest*,unsigned n,uintptr_t,size_t){assert(n==3);++routes;return routeOK;}
};
bool wexRplReferenceTrainingPathHooks(KernelPatcher&,unsigned,uintptr_t,size_t){return true;}
RESOLVER
int main(){
 agdcCallbackOriginal=reinterpret_cast<uintptr_t>(delegate);uintptr_t address=1;memcpy(controller+0x1C00,&address,8);memcpy(controller+0x1C08,&address,8);
 assert(nativeAGDCCallbackBound());address=0;memcpy(controller+0x1C00,&address,8);assert(!nativeAGDCCallbackBound());address=1;memcpy(controller+0x1C00,&address,8);
 ownerOK=false;assert(!nativeAGDCCallbackBound());ownerOK=true;
 auto controlBefore=std::vector<uint8_t>(controller,controller+sizeof(controller));
 for(uint32_t result:{0U,1U,0xE00002D8U}){returnValue=result;auto seq=agdcTraceSequence;assert(observedAGDCCallback(framebufferOwner,3,5,7)==result&&lastOwner==framebufferOwner&&lastArgument==5&&agdcTraceSequence==seq+2);assert(agdcTraceRows[16+((seq+2)%16)][5]==result);}
 auto seq=agdcTraceSequence;ownerOK=false;assert(observedAGDCCallback(framebufferOwner,3,5,7)==returnValue&&agdcTraceSequence==seq);ownerOK=true;assert(observedAGDCCallback((void*)2,3,5,7)==returnValue&&agdcTraceSequence==seq);
 assert(observedAGDCCallback(framebufferOwner,3,~0UL,7)==returnValue&&lastArgument==~0UL);assert(agdcTraceRows[16+(agdcTraceSequence%16)][3]==~0U);
 for(unsigned i=0;i<20;i++)recordAGDC(2,0,i,0,i);assert(agdcTraceRows[0][0]==1);assert(agdcTraceRows[15][0]==16);
 uint32_t value=5;memcpy(portBytes+0x120,&value,4);value=3;memcpy(portBytes+0x550,&value,4);value=2;memcpy(portBytes+0x554,&value,4);portBytes[0xF8]=1;
 publishAGDCObservation();assert(targetObject.meta.size()==16&&targetObject.rows.size()==256);auto&m=targetObject.meta;assert(m[3]==1&&m[4]==5&&m[5]==3&&m[6]==2&&m[7]==1&&m[8]==1&&m[9]==1);assert(!memcmp(controller,controlBefore.data(),sizeof(controller)));
 portOK=false;publishAGDCObservation();assert(targetObject.meta[4]==~0U&&targetObject.meta[7]==~0U);portOK=true;
 pathLookupOriginal=reinterpret_cast<uintptr_t>(pathDelegate);portTrainingOriginal=reinterpret_cast<uintptr_t>(trainDelegate);
 assert(observedPathLookup(port)==expectedPath&&trainingPathCalls==1&&trainingPathFound==1);
 expectedPath=nullptr;assert(!observedPathLookup(port)&&trainingPathCalls==2&&trainingPathFound==1);
 uint8_t req[0x50]{};value=6;memcpy(req,&value,4);req[0x13]=2;req[0x12]=10;
 assert(observedPortTraining(port,req)==0xE00002BC&&portTrainingCalls==1&&portTrainingResult==0xE00002BC);
 auto priorSeq=agdcTraceSequence;ownerOK=false;assert(!observedPathLookup(port));assert(observedPortTraining(port,req)==0xE00002BC&&portTrainingCalls==1&&agdcTraceSequence==priorSeq);ownerOK=true;
 assert(pathDelegates==3&&trainingDelegates==2);publishAGDCObservation();assert(targetObject.meta[12]==2&&targetObject.meta[13]==1&&targetObject.meta[14]==1&&targetObject.meta[15]==0xE00002BC);
 std::vector<uint8_t> image(0xF0000);uint8_t prefix[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0x83,0xEC,0x20};memcpy(image.data()+0x9F9CA,prefix,sizeof(prefix));uint8_t pathPrefix[]{0x55,0x48,0x89,0xe5,0x41,0x56,0x53,0x48,0x83,0xec,0x30,0x48,0xff,0x05,0xe4,0x48,0x08,0,0x49};
 uint8_t trainPrefix[]{0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xec,0xb8,0,0,0};
 memcpy(image.data()+0xE9AAA,pathPrefix,sizeof(pathPrefix));memcpy(image.data()+0x26C54,trainPrefix,sizeof(trainPrefix));
 auto base=reinterpret_cast<uintptr_t>(image.data());fakeEntry=base+0x9F9CA;KernelPatcher p;assert(wexRplReferenceAGDCObservationHooks(p,1,base,image.size())&&routes==1&&agdcTraceReady);
 for(unsigned i=0;i<sizeof(prefix);i++){image[0x9F9CA+i]^=1;assert(!wexRplReferenceAGDCObservationHooks(p,1,base,image.size()));image[0x9F9CA+i]^=1;}
 for(unsigned i=0;i<sizeof(pathPrefix);i++){image[0xE9AAA+i]^=1;assert(!wexRplReferenceAGDCObservationHooks(p,1,base,image.size()));image[0xE9AAA+i]^=1;}
 for(unsigned i=0;i<sizeof(trainPrefix);i++){image[0x26C54+i]^=1;assert(!wexRplReferenceAGDCObservationHooks(p,1,base,image.size()));image[0x26C54+i]^=1;}
 imageOK=false;assert(!wexRplReferenceAGDCObservationHooks(p,1,base,image.size()));imageOK=true;
 assert(!wexRplReferenceAGDCObservationHooks(p,1,base,0x9F9CA+sizeof(prefix)-1));routeOK=false;assert(!wexRplReferenceAGDCObservationHooks(p,1,base,image.size()));routeOK=true;
 livePortReady=0;assert(!wexRplReferenceAGDCObservationHooks(p,1,base,image.size()));livePortReady=1;flag=false;assert(wexRplReferenceAGDCObservationHooks(p,1,base,0));
 puts("PASS production AGDC observer: unchanged native ABI/results/arguments, scoped and redacted bounded trace, callback-binding proof, read-only sink/index/ref capture, exact entry/route failure gates");
}
'''.replace('BODY',body).replace('RESOLVER',resolver)
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.cpp').write_text(f);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'t.cpp'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
if len(sys.argv)>1:
 p=Path(sys.argv[1]);p=p/'Contents/MacOS/AppleIntelTGLGraphicsFramebuffer' if p.is_dir() else p;b=p.read_bytes();assert hashlib.sha256(b).hexdigest()=='285ee7a9c3d6c9a9647013f44fb40f312b1cb42879974ef0542544cf049442b5';assert b[0x9F9CA:0x9F9D9].hex()=='554889e5415741564154534883ec20';print('PASS actual private AGDC callback entry and complete position-independent prologue')
