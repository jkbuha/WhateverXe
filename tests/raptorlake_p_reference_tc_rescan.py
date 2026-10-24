#!/usr/bin/env python3
"""Execute production TC3 rescan admission/action with faulted ownership and HPD."""
from pathlib import Path
import subprocess,tempfile,sys
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text()
code=s[s.index('bool nativeRescanPermitted(){'):s.index('void nativeRescanWorker(')]
resolver=s[s.index('bool wexRplReferenceNativeHotplugHooks('):s.index('bool wexRplReferenceAGDCObservationHooks(')]
harness=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
using IOReturn=uint32_t;
constexpr IOReturn kIOReturnSuccess=0,kIOReturnNotReady=1;
struct OSObject {int refs=0;void retain(){++refs;}void release(){--refs;}};
struct Loop {bool gated=true;bool inGate(){return gated;}} loop;
struct Owner:OSObject {Loop* work=&loop;Loop*getWorkLoop(){return work;}} owner;
Owner *framebufferOwner=&owner;
struct Target {void setProperty(const char*,void*,unsigned){}} targetObject;
Target *target=&targetObject;
uint32_t nativeRescanState[8]{1};
void *nativeRescanWork=(void*)1;
uint32_t livePortReady=1,livePortPermission=1;
struct Lease {uintptr_t owner=0;bool active=false;} livePortLease;
bool capturable=true,shutting=false,observer=true,selected=true;
bool captureAllowed(){return capturable;}
bool bound=true;bool nativeAGDCCallbackBound(){return bound;}
bool shutdown(){return shutting;}
bool(*shutdownQuery)()=shutdown;
alignas(16) unsigned char storage[0x700]{};
OSObject *port=reinterpret_cast<OSObject*>(storage);
unsigned mappedDDI=5,lookups=0;
OSObject* get(unsigned ddi){assert(ddi==5);++lookups;return port;}
uintptr_t getPortEntry=reinterpret_cast<uintptr_t>(get);
bool observerPort(void*p,uint32_t&ddi){ddi=mappedDDI;return observer&&p==port;}
bool livePortSelected(void*){return selected;}
struct Backend {uint32_t tc=0x35,hpd=1U<<18;unsigned reads=0;uint32_t readReg32(uint32_t reg){++reads;assert(reg==0x161508||reg==0x44470);return reg==0x161508?tc:hpd;}} backend;
unsigned calls=0,publishes=0;
void publishLivePort(){++publishes;}
void invoke(void*p,uint32_t ddi,uint32_t mode){assert(p==framebufferOwner&&ddi==5&&mode==3&&loop.inGate());++calls;assert(port->refs==1);uint32_t v=3;memcpy(storage+0x550,&v,4);v=1;memcpy(storage+0x554,&v,4);}
uintptr_t nativeHotplugEntry=reinterpret_cast<uintptr_t>(invoke);
'''+code+r'''
using mach_vm_address_t=uintptr_t;
bool liveOption=true,imageOK=true;uintptr_t fakeEntry=0;
bool checkKernelArgument(const char*){return liveOption;}
bool wexRplReferenceImage(void*,size_t,bool){return imageOK;}
struct KernelPatcher {uintptr_t solveSymbol(size_t,const char*,uintptr_t,size_t){return fakeEntry;}void clearError(){}};
'''+resolver+r'''
int main(){
 static unsigned char image[0xB0C9A]{};
 const uint8_t prefix[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC};
 const uint8_t lookup[]{0x44,0x89,0xE7,0xE8,0x96,0x5B,0xF7,0xFF,0x48,0x85,0xC0,0x0F};
 memcpy(image+0xAFE6C,prefix,sizeof(prefix));memcpy(image+0xAFF14,lookup,sizeof(lookup));
 KernelPatcher patcher;auto base=reinterpret_cast<uintptr_t>(image);fakeEntry=base+0xAFE6C;
 assert(wexRplReferenceNativeHotplugHooks(patcher,1,base,sizeof(image)));
 for(unsigned i=0;i<sizeof(prefix);i++){image[0xAFE6C+i]^=1;assert(!wexRplReferenceNativeHotplugHooks(patcher,1,base,sizeof(image)));image[0xAFE6C+i]^=1;}
 for(unsigned i=0;i<sizeof(lookup);i++){image[0xAFF14+i]^=1;assert(!wexRplReferenceNativeHotplugHooks(patcher,1,base,sizeof(image)));image[0xAFF14+i]^=1;}
 imageOK=false;assert(!wexRplReferenceNativeHotplugHooks(patcher,1,base,sizeof(image)));imageOK=true;
 assert(!wexRplReferenceNativeHotplugHooks(patcher,1,base,sizeof(image)-1));
 ++fakeEntry;assert(!wexRplReferenceNativeHotplugHooks(patcher,1,base,sizeof(image)));--fakeEntry;
 livePortReady=0;assert(!wexRplReferenceNativeHotplugHooks(patcher,1,base,sizeof(image)));livePortReady=1;
 nativeRescanWork=nullptr;assert(!wexRplReferenceNativeHotplugHooks(patcher,1,base,sizeof(image)));nativeRescanWork=(void*)1;
 liveOption=false;assert(wexRplReferenceNativeHotplugHooks(patcher,1,base,0));liveOption=true;
 nativeHotplugEntry=reinterpret_cast<uintptr_t>(invoke);
 assert(nativeRescanPermitted());
 for(unsigned i=0;i<8;i++){
  auto entry=nativeHotplugEntry;auto work=nativeRescanWork;auto shutdownFn=shutdownQuery;
  if(i==0)nativeHotplugEntry=0;if(i==1)nativeRescanWork=nullptr;if(i==2)capturable=false;
  if(i==3)shutdownQuery=nullptr;if(i==4)shutting=true;if(i==5)livePortReady=0;
  if(i==6)livePortPermission=0;if(i==7)livePortLease.active=true;
  assert(!nativeRescanPermitted());assert(nativeRescanAction(framebufferOwner,nullptr,nullptr,nullptr,nullptr)==kIOReturnNotReady);assert(!calls);
  nativeHotplugEntry=entry;nativeRescanWork=work;shutdownQuery=shutdownFn;capturable=true;shutting=false;livePortReady=livePortPermission=1;livePortLease.active=false;
 }
 livePortLease.owner=123;assert(!nativeRescanPermitted());livePortLease.owner=0;
 bound=false;assert(nativeRescanAction(framebufferOwner,0,0,0,0)==kIOReturnNotReady&&!calls);bound=true;
 Owner foreign;assert(nativeRescanAction(&foreign,0,0,0,0)==kIOReturnNotReady);
 loop.gated=false;assert(nativeRescanAction(framebufferOwner,0,0,0,0)==kIOReturnNotReady);loop.gated=true;
 owner.work=nullptr;assert(nativeRescanAction(framebufferOwner,0,0,0,0)==kIOReturnNotReady);owner.work=&loop;
 for(unsigned i=0;i<8;i++){
  auto fn=getPortEntry;
  if(i==0)getPortEntry=0;if(i==1)observer=false;if(i==2)mappedDDI=4;if(i==3)selected=false;
  if(i==4)backend.tc=~0U;if(i==5)backend.tc=4;if(i==6)backend.hpd=0;if(i==7)backend.hpd=(1U<<18)|4;
  assert(nativeRescanAction(framebufferOwner,0,0,0,0)==kIOReturnNotReady);assert(!calls);
  getPortEntry=fn;observer=selected=true;mappedDDI=5;backend.tc=0x35;backend.hpd=1U<<18;
 }
 backend.hpd=~0U;assert(nativeRescanAction(framebufferOwner,0,0,0,0)==kIOReturnNotReady);backend.hpd=1U<<18;
 assert(nativeRescanAction(framebufferOwner,0,0,0,0)==kIOReturnSuccess);
 assert(calls==1&&nativeRescanState[4]==1&&nativeRescanState[6]==3&&nativeRescanState[7]==1&&port->refs==0&&publishes==1);
 puts("PASS production native TC3 action: gate/owner/shutdown/permission/active-lease/port/HPD failures, exact DDI5 mode3 invocation and retained lifetime");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'test.cpp';p.write_text(harness);exe=Path(d)/'test'
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
control=(r/'WhateverXe/kern_raptorlake_p_control.cpp').read_text()
assert control.index('clientHasPrivilege')<control.index('if(rescan)return')
assert 'dict->getCount()!=1' in control
request=s[s.index('bool rescanTC3(){'):s.index('bool arm(){',s.index('bool rescanTC3(){'))]
assert '__sync_bool_compare_and_swap(&nativeRescanUsed,0,1)' in request
assert '__sync_bool_compare_and_swap(&nativeRescanBusy,0,1)' in request
assert 'thread_call_enter(nativeRescanWork)' in request
worker=s[s.index('void nativeRescanWorker('):s.index('void probeTCAux(){')]
assert 'loop->runAction(nativeRescanAction,owner)' in worker
assert worker.index('owner->retain()')<worker.index('runAction')<worker.index('owner->release()')
print('PASS root-only selector, once-per-boot request, and retained workloop dispatch')

if len(sys.argv)>1:
 import hashlib
 p=Path(sys.argv[1]);p=p/'Contents/MacOS/AppleIntelTGLGraphicsFramebuffer' if p.is_dir() else p
 raw=p.read_bytes()
 assert hashlib.sha256(raw).hexdigest()=='285ee7a9c3d6c9a9647013f44fb40f312b1cb42879974ef0542544cf049442b5'
 assert raw[0xAFE6C:0xAFE7C].hex()=='554889e54157415641554154534883ec'
 assert raw[0xAFF14:0xAFF20].hex()=='4489e7e8965bf7ff4885c00f'
 print('PASS exact private donor native-handler address, entry and port lookup')
