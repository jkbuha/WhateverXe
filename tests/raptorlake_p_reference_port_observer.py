"""Exercise the actual external-port hold before any native training."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text()
body=s[s.index('uint32_t observedPortState('):s.index('bool observerPort(void *port,uint32_t &ddi){')]
body+=s[s.index('bool observerPort(void *port,uint32_t &ddi){'):s.index('mach_vm_address_t fbOriginal;')]
fixture=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include "kern_raptorlake_p_reference_fb_core.hpp"
using mach_vm_address_t=uintptr_t;
bool tcss,observe,allowed;unsigned size=0x700,calls,reads,modes;const char *classname="AppleIntelPortHALDiags";
struct Meta {unsigned getClassSize(){return size;}const char *getClassName(){return classname;}} meta;
struct OSObject {uintptr_t table;Meta *getMetaClass(){return &meta;}};
struct Port:OSObject {uint8_t padding[0x700-8]{};} port;
uintptr_t vtable[64]{};uint8_t controller[0x1C48]{},platform[0x100]{};
void *framebufferOwner=controller;
uint32_t portObserve[67]{1},portBindings[8]{1};mach_vm_address_t portImageBase=0x10000,portImageSize=0x10000,compatPortEntry=0x30000,pureVirtualEntry=0x40000;
bool wexRplReferenceTCSSRequested(){return tcss;}
bool checkKernelArgument(const char *s){assert(!strcmp(s,"-wex-rplp-tcss-observe"));return observe;}
bool wexRplReferenceFBAllowed(void *p){return allowed&&p==controller;}
uint32_t compatPortMode(void *){++modes;return 3;}
bool livePortSelected(void *){return false;}
uint32_t livePortAUX(void *,uint32_t,uint32_t *,uint32_t,bool){assert(false);return 0;}
struct Backend {uint32_t readReg32(unsigned r){assert(r==0x44470||r==0xC4000);++reads;return r==0x44470?0x40000:0x10000;}} backend;
uint32_t original(void *){++calls;return 77;}mach_vm_address_t portStateOriginal=reinterpret_cast<uintptr_t>(original);
constexpr uint32_t kIOReturnNotReady=0xE00002D8;
void *auxProbePort;uintptr_t auxProbeThread;unsigned auxProbePermit;uintptr_t current_thread(){return 1;}
unsigned hpdCalls,auxCalls;uint32_t hpdOriginal(void *){++hpdCalls;return 1;}
uint32_t auxOriginal(void *,uint32_t command,uint32_t *data,uint32_t bytes,bool retry){++auxCalls;assert(command==9&&bytes==4&&retry);*data=0x1234;return 0;}
mach_vm_address_t portHPDOriginal=reinterpret_cast<uintptr_t>(hpdOriginal),portAUXOriginal=reinterpret_cast<uintptr_t>(auxOriginal);
unsigned initialRequests=0;void requestInitialDiscovery(){++initialRequests;}
BODY
void put(void *p,unsigned o,uintptr_t v){memcpy(static_cast<uint8_t *>(p)+o,&v,8);}
void setup(unsigned ddi){put(controller,0xC40,0x123);put(controller,0xD40,reinterpret_cast<uintptr_t>(platform));put(&port,0x578,0x123);put(&port,0x548,reinterpret_cast<uintptr_t>(platform+0x20+24*ddi));memcpy(platform+0x20+24*ddi,&ddi,4);port.table=reinterpret_cast<uintptr_t>(vtable);}
int main(){
 setup(0);assert(observedPortState(&port)==77);tcss=observe=allowed=true;
 assert(observedPortState(&port)==77&&calls==2&&!reads&&!modes);
 for(unsigned ddi=1;ddi<9;++ddi)for(unsigned dispatch=1;dispatch<=4;++dispatch){
  setup(ddi);vtable[0x190/8]=dispatch==1?compatPortEntry:dispatch==2?pureVirtualEntry:dispatch==3?portImageBase+10:1;
  auto prior=portObserve[3+7*ddi];assert(!observedPortState(&port));auto i=3+7*ddi;
  assert(portObserve[i]==prior+1&&portObserve[i+1]==1&&portObserve[i+2]==dispatch&&portObserve[i+4]==3&&portObserve[i+5]==0x40000&&portObserve[i+6]==0x10000);
 }
 assert(calls==2&&reads==64&&modes==32&&portBindings[7]==32);
 auto before=reads;setup(5);allowed=false;assert(!observedPortState(&port));allowed=true;size=0x6B0;assert(!observedPortState(&port));size=0x700;
 put(&port,0x578,0);assert(!observedPortState(&port));setup(5);put(&port,0x548,reinterpret_cast<uintptr_t>(platform+0x21));assert(!observedPortState(&port));
 setup(5);unsigned wrong=6;memcpy(platform+0x20+5*24,&wrong,4);assert(!observedPortState(&port));assert(reads==before&&calls==2);
 assert(!observedPortState(nullptr));
 observe=false;assert(observedPortState(&port)==77&&calls==3&&reads==before);
 setup(5);uint32_t data=0x5678;observe=true;
 assert(!observedPortHPD(&port)&&hpdCalls==0&&portBindings[5]==1);
 assert(observedPortAUX(&port,9,&data,4,true)==kIOReturnNotReady&&auxCalls==0&&data==0x5678&&portBindings[6]==1);
 setup(0);assert(observedPortHPD(&port)==1&&hpdCalls==1);
 assert(!observedPortAUX(&port,9,&data,4,true)&&auxCalls==1&&data==0x1234);
 allowed=false;assert(!observedPortHPD(&port)&&observedPortAUX(&port,9,&data,4,true)==kIOReturnNotReady&&auxCalls==1&&hpdCalls==1);
 observe=false;assert(observedPortHPD(&port)==1&&hpdCalls==2);assert(!observedPortAUX(&port,9,&data,4,true)&&auxCalls==2);
 observe=true;allowed=true;setup(5);auxProbePermit=1;auxProbePort=&port;auxProbeThread=2;
 assert(observedPortAUX(&port,9,&data,4,true)==kIOReturnNotReady&&auxCalls==2);
 auxProbeThread=1;auxProbePort=nullptr;assert(observedPortAUX(&port,9,&data,4,true)==kIOReturnNotReady&&auxCalls==2);
 auxProbePort=&port;assert(!observedPortAUX(&port,9,&data,4,true)&&auxCalls==3);
 assert(!observedPortHPD(&port));
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-port-observer-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
assert 'writeReg' not in body and 'setProperty' not in body
print('PASS actual port observer: native internal/flag-off returns preserved, all eight external ports held, dispatch classification, invalid owner/class/access/record rejected before MMIO')
