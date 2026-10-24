"""Replay production TC3 transition/AUX callbacks without hardware."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('// Optional TC3 training remains closed'):s.index('// The observer holds external ports disconnected:')]
f=r'''
#include <cassert>
#include <cstdint>
#include <map>
#include <cstring>
#include "kern_raptorlake_p_reference_tc_live.hpp"
#include "kern_display13_tc.hpp"
using mach_vm_address_t=uintptr_t;
constexpr uint32_t kIOReturnSuccess=0,kIOReturnNotReady=0xE00002D8,kIOReturnUnsupported=0xE00002C7;
using IOInterruptState=uintptr_t;struct IOSimpleLock {bool held;} spin;
IOInterruptState IOSimpleLockLockDisableInterrupt(IOSimpleLock *p){assert(p==&spin&&!p->held);p->held=true;return 0x99;}
void IOSimpleLockUnlockEnableInterrupt(IOSimpleLock *p,IOInterruptState state){assert(p==&spin&&p->held&&state==0x99);p->held=false;}
uint8_t portMemory[0x700]{};void *portObject=portMemory;
struct IORecursiveLock {unsigned depth{},maximum{};} lock;
void IORecursiveLockLock(IORecursiveLock *l){assert(l==&lock);++l->depth;if(l->depth>l->maximum)l->maximum=l->depth;}
void IORecursiveLockUnlock(IORecursiveLock *l){assert(l==&lock&&l->depth);--l->depth;}
void IOSleep(unsigned n){assert(n==1);}
struct Backend {
 std::map<uint32_t,uint32_t> regs{{0x45404,0xC03},{0x42000,1U<<25},{0x161508,0x35},{0x44470,0x40000},{0x45454,3},{0x45444,3},{0x64500,0x80},{0x64510,0x40000800},{0x16E8A0,0x00005A0C},{0x16E8C0,0xA5A5A5A0}};
 uint32_t readReg32(uint32_t r){assert(regs.count(r));auto v=regs[r];if(r==0x45404)v=(v&~4U)|((v&8)?4:0);if(r==0x45444)v=(v&~0x400U)|((v&0x800)?0x400:0);return v;}
 void writeReg32(uint32_t r,uint32_t v){assert(r==0x45444||r==0x45404||r==0x64500||r==0x64510||r==0x1010A0||r==0x46048||r==0x16E8C0||(r>=0x16A000&&r<0x16B000));if(r==0x64510){auto w1c=(1U<<30)|(1U<<28)|(1U<<25);assert(!(v&w1c));regs[r]=(v&~w1c)|(regs[r]&w1c);}else regs[r]=r==0x45404?v&~4U:v;}
} backend;
void *framebufferOwner=(void *)1;unsigned nativeModes,baseModes,enables,disables,auxCalls,auxRefs,mode;bool owner=true;unsigned ddi=5;
bool auxBypass=false;uint32_t nativeAuxBypassRefs(void*){return auxBypass?auxRefs:~0U;}
struct Target {unsigned publishes;void setProperty(const char *n,void *p,size_t s){assert(p&&((!strcmp(n,"WhateverXeRPLPReferenceTCLive")&&s==64)||(!strcmp(n,"WhateverXeRPLPReferenceTCLanes")&&s==48)||(!strcmp(n,"WhateverXeRPLPReferenceTCAuxTrace")&&s<=256*8*4)||(!strcmp(n,"WhateverXeRPLPReferenceTCPhy")&&s==64)||(!strcmp(n,"WhateverXeRPLPReferenceTCPhyRegisters")&&s==22*8*4)||(!strcmp(n,"WhateverXeRPLPReferenceTCPhyWrites")&&s<=64*8*4)));++publishes;}} targetObject,*target=&targetObject;
bool observerPort(void *p,uint32_t &d){d=ddi;return p==portObject&&owner;}
uint32_t originalMode(void *,uint32_t){++nativeModes;return 0x77;}
uint32_t baseMode(void *p,uint32_t m){assert(p==portObject);++baseModes;mode=m;return 0;}
void enableAux(void *p){assert(p==portObject&&mode==3);++enables;++auxRefs;if(!auxBypass)backend.regs[0x45444]|=0x800;}
void disableAux(void *p){assert(p==portObject&&auxRefs);++disables;if(!--auxRefs&&!auxBypass)backend.regs[0x45444]&=~0x800U;}
uint32_t livePortMode(void *,uint32_t);
uint32_t originalAux(void *p,uint32_t c,uint32_t *d,uint32_t b,bool retry){assert(p==portObject&&c==9&&b==4&&retry);++auxCalls;assert(livePortMode(p,3)==kIOReturnSuccess);*d=0x1234;return 0;}
mach_vm_address_t portAUXOriginal=reinterpret_cast<uintptr_t>(originalAux);
BODY
int main(){
 livePortOriginal=reinterpret_cast<uintptr_t>(originalMode);liveBaseMode=reinterpret_cast<uintptr_t>(baseMode);liveAuxEnable=reinterpret_cast<uintptr_t>(enableAux);liveAuxDisable=reinterpret_cast<uintptr_t>(disableAux);livePortLock=&lock;livePhyLock=&spin;
 assert(livePortMode(portObject,3)==0x77&&nativeModes==1&&!baseModes);
 livePortReady=1;assert(livePortMode(portObject,3)==0x77&&nativeModes==2&&!baseModes);
 livePortPermission=1;owner=false;assert(livePortMode(portObject,3)==0x77&&nativeModes==3&&!baseModes);owner=true;ddi=3;assert(livePortMode(portObject,3)==0x77&&nativeModes==4);ddi=5;
 uint32_t data=0x5678;assert(livePortAUX(portObject,9,&data,4,true)==kIOReturnNotReady&&!auxCalls&&data==0x5678);
 assert(livePortMode(portObject,2)==kIOReturnUnsupported&&!baseModes);
 assert(livePortMode(portObject,3)==kIOReturnSuccess&&livePortLease.active&&enables==1&&baseModes==1);
 assert(livePortLaneMask(portObject)==12);
 auto laneWrites=liveLaneState[10];
 livePortSetLanes(portObject,2);assert(backend.regs[0x16E8C0]==0xA5A5A5A3&&liveLaneState[10]==laneWrites+1);
 for(auto invalid:{0U,3U,4U,5U})livePortSetLanes(portObject,invalid);
 assert(liveLaneState[10]==laneWrites+1);
 backend.regs[0x64500]|=1U<<16;livePortSetLanes(portObject,1);assert(liveLaneState[10]==laneWrites+1);backend.regs[0x64500]&=~(1U<<16);
 for(auto mask:{1U,2U,4U,8U,3U,12U,15U}){backend.regs[0x16E8A0]=0x12340000|mask;assert(livePortLaneMask(portObject)==mask);}
 for(auto mask:{0U,5U,6U,7U,9U,10U,11U,13U,14U,~0U}){backend.regs[0x16E8A0]=mask;assert(!livePortLaneMask(portObject));livePortSetLanes(portObject,1);}
 assert(liveLaneState[10]==laneWrites+1);backend.regs[0x16E8A0]=0xC;
 const uint32_t table[]{0x20D4,0x20D8,0x2130,0x212C,0x46030,0x2200,0x2204,0x2208,0x168210,0x168214,0x168218,0x220C,0x2244,0x224C,0x2C0,0x2C4,0x2C8,0x12C0,0x12C4,0x12C8,0xA0,0x10A0};
 for(unsigned i=0;i<22;i++)memcpy(portMemory+0x580+4*(0x34+i),table+i,4);
 backend.regs[0x1010A0]=0x44332211;backend.regs[0x16A0D4]=0x12345678;backend.regs[0x46048]=0x80000000;
 assert(livePortReadPhy(portObject,0x34)==0x12345678&&backend.regs[0x1010A0]==0x44332211&&!spin.held);
 assert(livePortReadPhy(portObject,0x38)==0x80000000);livePortWritePhy(portObject,0x38,0x90000000);assert(backend.regs[0x46048]==0x90000000);
 livePortWritePhy(portObject,0x34,0x11223344);assert(backend.regs[0x16A0D4]==0x11223344&&backend.regs[0x1010A0]==0x44332211&&!spin.held);
 assert(livePortReadPhy(portObject,0x33)==0&&livePortReadPhy(portObject,0x4A)==0);
 uint32_t foreign=1;memcpy(portMemory+0x580+4*0x34,&foreign,4);assert(livePortReadPhy(portObject,0x34)==0);memcpy(portMemory+0x580+4*0x34,table,4);

 assert(!livePortAUX(portObject,9,&data,4,true)&&data==0x1234&&auxCalls==1&&enables==1&&lock.maximum==2&&!lock.depth);
 backend.regs[0x64500]&=~64U;assert(livePortAUX(portObject,9,&data,4,true)==kIOReturnNotReady&&auxCalls==1);backend.regs[0x64500]|=64;
 backend.regs[0x64500]|=1U<<31;assert(livePortMode(portObject,0)==kIOReturnNotReady&&livePortLease.aux&&!disables);backend.regs[0x64500]&=~(1U<<31);
 assert(livePortMode(portObject,0)==kIOReturnSuccess&&!livePortLease.owner&&disables==1&&baseModes==2&&mode==0&&backend.regs[0x45404]==0xC03&&!lock.depth);
 assert(livePortMode(portObject,0)==kIOReturnSuccess&&disables==1&&baseModes==3&&mode==0);
 auxBypass=true;assert(livePortMode(portObject,3)==kIOReturnSuccess&&livePortLease.auxRequest&&auxRefs==1);
 assert(livePortMode(portObject,0)==kIOReturnSuccess&&!livePortLease.owner&&!livePortLease.auxRequest&&!auxRefs&&!(backend.readReg32(0x45444)&0x800));
 assert(livePhyTrace[1]>0&&livePhyTrace[2]>0&&livePhyTrace[11]==2);
 assert(livePhyTrace[0]==2&&livePhyTrace[8]==0x34&&livePhyTrace[9]==1&&livePhyTrace[15]==0);
 const auto savedFailure=livePhyTrace[8];
 Display13TC::Result failure;failure.status=Display13TC::Status::Readback;failure.before=0x11;failure.after=0x22;failure.writes=1;
 for(unsigned i=0;i<100;i++)recordPhyAccess(0x38,0x46030,failure,true,0x33+i);
 assert(livePhyWriteCount==64&&livePhyWrites[0][1]==0x38&&livePhyWrites[0][3]==0x90000000);
 assert(livePhyRegisters[4][3]==101&&livePhyRegisters[4][4]==0x33+99&&livePhyRegisters[4][5]==0x11&&livePhyRegisters[4][6]==0x22);
 assert(livePhyTrace[8]==savedFailure); // First failure is immutable across retries.
 livePhyTrace[8]=~0U;recordPhyAccess(0x38,0x46030,failure,true,0x99);
 assert(livePhyTrace[12]==0x11&&livePhyTrace[13]==0x22&&livePhyTrace[14]==0x99&&livePhyTrace[15]==1);
 auto savedCalls=auxCalls;
 for(unsigned i=0;i<300;i++)assert(livePortAUX(portObject,9,&data,4,true)==kIOReturnNotReady);
 assert(liveAuxTraceCount==256&&auxCalls==savedCalls&&liveAuxTrace[0][6]==kIOReturnNotReady&&liveAuxTrace[1][5]==0x1234);
 publishLivePort();assert(targetObject.publishes>=4);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='wex-tc-live-') as d:
 p=Path(d);(p/'t.cpp').write_text(f);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'t.cpp'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
print('PASS production TC3 callbacks: flag/preflight/owner isolation, mode rejection, native AUX reference lifetime, recursive callbacks, lost ownership and active-buffer cleanup refusal')
