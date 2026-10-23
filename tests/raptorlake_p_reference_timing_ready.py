"""Run production controller/timing/mode-info readiness without hardware or sleeps."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('bool waitForNativeTimings(){'):s.index('void worker(thread_call_param_t')]
f=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
constexpr unsigned kIOReturnSuccess=0;
struct IODisplayModeInformation{unsigned nominalWidth,nominalHeight;};
struct IOService;uint32_t result,width;unsigned infoCalls;
struct Meta{unsigned size=0x4B98;unsigned getClassSize(){return size;}};
struct IOService{uint8_t storage[0x5000]{};Meta meta;IOService *provider{};IOService *getProvider(){return provider;}Meta *getMetaClass(){return &meta;}
 unsigned getInformationForDisplayMode(unsigned mode,IODisplayModeInformation *i){assert(mode>=0x10000&&mode<0xFFFF0000);++infoCalls;i->nominalWidth=width;i->nominalHeight=1200;return result;}
} controller,head,pci,foreign;
using IOFramebuffer=IOService;
IOService *framebufferOwner=&controller,*target=&pci;
uint64_t panelState[16]{},handover[8]{};uint32_t bootProbe[8]{};unsigned sleeps,appear,completeAfter;
void setTiming(uint32_t mode,uintptr_t array){memcpy(reinterpret_cast<uint8_t *>(&head)+0x4250,&mode,4);memcpy(reinterpret_cast<uint8_t *>(&head)+0x4260,&array,8);}
void IOSleep(unsigned ms){assert(ms==25);++sleeps;if(appear&&sleeps==appear)setTiming(0x80001000,0x1234);if(completeAfter&&sleeps==completeAfter){panelState[8]=1;__atomic_store_n(&handover[7],uint64_t(1),__ATOMIC_RELEASE);}}
BODY
int main(){
 const uint32_t modes[]={0,0x1000,0x10000,0x80001000,0x80001012,0xFFFF0000,0xFFFFFF00,0xFFFFFFFB};
 for(unsigned mask=0;mask<1024;++mask)for(uint32_t mode:modes){
  framebufferOwner=(mask&1)?&controller:nullptr;panelState[3]=(mask&2)?0x1C48:0;
  IOService *h=(mask&4)?&head:nullptr;memcpy(reinterpret_cast<uint8_t *>(&controller)+0xD60,&h,8);
  head.provider=(mask&8)?target:&foreign;head.meta.size=(mask&16)?0x4B98:0x4B90;
  panelState[8]=bool(mask&64);handover[7]=bool(mask&128);result=(mask&256)?0:0xE00002C2;width=(mask&512)?1920:0;
  setTiming(mode,(mask&32)?0x1234:0);memset(bootProbe,0,sizeof(bootProbe));sleeps=appear=completeAfter=infoCalls=0;
  bool expected=mask==1023&&mode>=0x10000&&mode<0xFFFF0000;
  assert(waitForNativeTimings()==expected);assert(bootProbe[2]==expected);
  assert(sleeps==(expected?0U:600U)&&bootProbe[1]==sleeps+1);
  if(!(mask&64)||!(mask&128))assert(!infoCalls); // no virtual queries during native controller start
 }
 framebufferOwner=&controller;panelState[3]=0x1C48;IOService *h=&head;memcpy(reinterpret_cast<uint8_t *>(&controller)+0xD60,&h,8);
 head.provider=target;head.meta.size=0x4B98;result=0;width=1920;
 panelState[8]=handover[7]=1;setTiming(0xFFFFFFFB,0);memset(bootProbe,0,sizeof(bootProbe));sleeps=0;appear=3;
 assert(waitForNativeTimings()&&sleeps==3&&bootProbe[1]==4);
 panelState[8]=handover[7]=0;setTiming(0x80001000,0x1234);memset(bootProbe,0,sizeof(bootProbe));sleeps=appear=infoCalls=0;completeAfter=7;
 assert(waitForNativeTimings()&&sleeps==7&&infoCalls==1);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-timing-ready-') as d:
 p=Path(d);(p/'test.cpp').write_text(f);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print('PASS production readiness:8192 identity/controller/timing/mode-info cases; no mode queries during controller start; immediate and delayed readiness; bounded timeout')
