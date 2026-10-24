"""Production live-transition installer exact-image and rejection replay."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('bool wexRplReferenceLivePortHooks('):s.index('bool wexRplReferenceNativeHotplugHooks(')]
f=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using mach_vm_address_t=uintptr_t;
bool flag=true,selected=true,observe=true,aux=true,image=true,allocation=true,spinAllocation=true;
uint32_t portBindings[8]{1,2,0},livePortReady;uintptr_t liveBaseMode,liveAuxEnable,liveAuxDisable,livePortOriginal,liveReadPhyOriginal,liveWritePhyOriginal;
unsigned allocations,publishes;struct IORecursiveLock{} lock,*livePortLock;
struct IOSimpleLock{} spin,*livePhyLock;
IOSimpleLock *IOSimpleLockAlloc(){return spinAllocation?&spin:nullptr;}
IORecursiveLock *IORecursiveLockAlloc(){++allocations;return allocation?&lock:nullptr;}
bool checkKernelArgument(const char *s){if(!strcmp(s,"-wex-rplp-tcss-live"))return flag;if(!strcmp(s,"-wex-rplp-tcss-observe"))return observe;assert(!strcmp(s,"-wex-rplp-tcss-aux-probe"));return aux;}
bool wexRplReferenceTCSSSelected(){return selected;}bool wexRplReferenceImage(void *,size_t,bool){return image;}
void publishLivePort(){++publishes;}uint32_t livePortMode(void *,uint32_t){return 0;}
uint32_t livePortReadPhy(void *,uint32_t){return 0;}
void livePortWritePhy(void *,uint32_t,uint32_t){}
#define SYSLOG(...) ((void)0)
struct KernelPatcher {
 bool badSymbol=false,failRoute=false;unsigned routes{};
 struct RouteRequest{template<class T>RouteRequest(const char *,T,uintptr_t &o){o=0x1234;}};
 uintptr_t solveSymbol(unsigned,const char *name,uintptr_t a,size_t){if(badSymbol)return 0;
  if(!strcmp(name,"__ZN17AppleIntelPortHAL11setPortModeEN14AppleIntelPort8PortModeE"))return a+0x8F344;
  if(!strcmp(name,"__ZN14AppleIntelPort11setPortModeENS_8PortModeE"))return a+0x262EA;
  if(!strcmp(name,"__ZN14AppleIntelPort18enablePowerWellAuxEv"))return a+0x2972C;
  if(!strcmp(name,"__ZN14AppleIntelPort19disablePowerWellAuxEv"))return a+0x29768;
  if(!strcmp(name,"__ZN17AppleIntelPortHAL15ReadDekelPhyRegEj"))return a+0x90234;
  if(!strcmp(name,"__ZN17AppleIntelPortHAL16WriteDekelPhyRegEjj"))return a+0x902EA;assert(false);return 0;}
 void clearError(){}bool routeMultiple(unsigned,RouteRequest *,size_t n,uintptr_t,size_t){assert(n==3);++routes;return !failRoute;}
};
BODY
int main(){
 std::vector<uint8_t> bytes(0x150000);auto a=reinterpret_cast<uintptr_t>(bytes.data());
 const uint32_t offsets[]{0x8F344,0x262EA,0x2972C,0x29768,0x90234,0x902EA};
 const uint8_t data[6][16]{{0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xec},{0x55,0x48,0x89,0xe5,0x41,0x56,0x53,0x48,0x83,0xec,0x20,0x48,0xff,0x05,0x64,0x35},{0x55,0x48,0x89,0xe5,0x48,0xff,0x05,0x51,0x0b,0x12,0,0x48,0xff,0x05,0x52,0xff},{0x55,0x48,0x89,0xe5,0x48,0xff,0x05,0x1d,0x0b,0x12,0,0x48,0xff,0x05,0x16,0xff},{0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x53,0x50,0x48,0xff,0x05,0xb3,0xd2,0x0c},{0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x54,0x53,0x48,0xff,0x05,0x0c,0xd2}};
 for(unsigned mode=0;mode<18;mode++){
  for(unsigned x=0;x<6;x++)memcpy(bytes.data()+offsets[x],data[x],16);
  flag=mode!=1;selected=mode!=2;observe=mode!=3;aux=mode!=4;image=mode!=5;allocation=mode!=6;spinAllocation=mode!=17;allocations=publishes=livePortReady=0;
  portBindings[1]=mode==7?1:2;portBindings[2]=mode==8?1:0;
  KernelPatcher p;p.badSymbol=mode==9;p.failRoute=mode==10;
  if(mode>=11&&mode<=16)bytes[offsets[mode-11]]^=1;
  bool result=wexRplReferenceLivePortHooks(p,1,a,bytes.size());
  assert(result==(mode==0||mode==1));
  assert(livePortReady==(mode==0));assert(p.routes==(mode==0||mode==10));
  if(mode!=0&&mode!=1&&mode!=6&&mode!=10&&mode!=17)assert(!allocations);
 }
 flag=selected=observe=aux=image=allocation=true,spinAllocation=true;portBindings[1]=2;portBindings[2]=0;KernelPatcher p;
 assert(!wexRplReferenceLivePortHooks(p,1,a,0x902F9)&&!p.routes);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='wex-tc-installer-') as d:
 p=Path(d);(p/'t.cpp').write_text(f);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'t.cpp'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
print('PASS production TC3 installer: opt-in, exact owner/image/symbol/bytes, prior bindings, allocation/route failure and truncated image')
