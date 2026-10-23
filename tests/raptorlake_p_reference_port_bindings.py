"""Run actual exact-image/table validation and pointer repairs with mocks."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('bool wexRplReferencePortHooks('):s.index('bool wexRplReferenceValidate(){')]
f=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using mach_vm_address_t=uintptr_t;
bool selected=true,observe=true,image=true,auxProbeFlag=false;uint64_t panelState[16]{};
uintptr_t auxReadEntry,getPortEntry;uintptr_t compatPortEntry=0x400000,pureVirtualEntry,portImageBase,portImageSize,portStateOriginal,portHPDOriginal,portAUXOriginal;
uint32_t portObserve[67]{1},portBindings[8]{1};
struct Target{void setProperty(const char *,void *,size_t){}} object,*target=&object;
bool wexRplReferenceTCSSSelected(){return selected;}
bool checkKernelArgument(const char *s){if(!strcmp(s,"-wex-rplp-tcss-aux-probe"))return auxProbeFlag;assert(!strcmp(s,"-wex-rplp-tcss-observe"));return observe;}
bool wexRplReferenceImage(void *,size_t,bool){return image;}
uint32_t compatPortMode(void *){return 3;}uint32_t observedPortState(void *){return 0;}uint32_t observedPortHPD(void *){return 0;}uint32_t observedPortAUX(void *,uint32_t,uint32_t *,uint32_t,bool){return 0;}
struct KernelPatcher{
 static constexpr unsigned KernelID=0;enum class Error{NoError,MemoryProtection};Error error=Error::NoError;unsigned writes=0,routes=0;bool badSymbol=false,failWrite=false;
 struct RouteRequest{template<class T>RouteRequest(const char *,T,uintptr_t &o){o=0x123;}};
 uintptr_t solveSymbol(unsigned i,const char *name,uintptr_t a=0,size_t=0){
 if(i==0){assert(!strcmp(name,"___cxa_pure_virtual"));return 0x500000;}
 if(badSymbol)return 0;
 if(!strcmp(name,"__ZN14AppleIntelPort7readAUXEjPvj"))return a+0x28D9E;
 if(!strcmp(name,"__ZN14AppleIntelPort12getPortByDDIEj"))return a+0x25AB2;
 if(!strcmp(name,"__ZN14AppleIntelPort14probePortStateEv"))return a+0x2E90A;
 if(!strcmp(name,"__ZN17AppleIntelPortHAL11getHPDStateEv"))return a+0x91836;
 if(!strcmp(name,"__ZN17AppleIntelPortHAL13runAUXCommandEN14AppleIntelPort11AUX_CommandEPjjb"))return a+0x91B64;
 if(!strcmp(name,"__ZTV17AppleIntelPortHAL"))return a+0x13CDA8;
 if(!strcmp(name,"__ZTV22AppleIntelPortHALDiags"))return a+0x1415D8;
 assert(false);return 0;
 }
 void clearError(){error=Error::NoError;}Error getError(){return error;}
 bool routeMultiple(unsigned,RouteRequest *,size_t count,uintptr_t,size_t){assert(count==3);++routes;return true;}
 void routeBlock(uintptr_t a,const uint8_t *b,size_t n){assert(n==8);++writes;if(failWrite)error=Error::MemoryProtection;else memcpy(reinterpret_cast<void *>(a),b,n);}
};
BODY
int main(){
 std::vector<uint8_t> bytes(0x150000);auto a=reinterpret_cast<uintptr_t>(bytes.data());
 auto setup=[&](){memset(bytes.data(),0,bytes.size());memset(portBindings,0,sizeof(portBindings));memset(portObserve,0,sizeof(portObserve));
 const uint8_t shortP[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x53,0x48,0x83,0xEC};
 const uint8_t auxP[]{0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x83,0xEC,0x78};
 memcpy(bytes.data()+0x2E90A,shortP,sizeof(shortP));memcpy(bytes.data()+0x91836,shortP,sizeof(shortP));memcpy(bytes.data()+0x91B64,auxP,sizeof(auxP));memcpy(bytes.data()+0x28D9E,auxP,sizeof(auxP));memcpy(bytes.data()+0x25AB2,auxP,4);
 uintptr_t pure=0x500000;memcpy(bytes.data()+0x13CF48,&pure,8);memcpy(bytes.data()+0x141778,&pure,8);panelState[10]=1;};
 setup();KernelPatcher p;assert(wexRplReferencePortHooks(p,1,a,bytes.size())&&p.writes==2&&p.routes==1&&portBindings[1]==2&&!portBindings[2]);
 uintptr_t got;memcpy(&got,bytes.data()+0x141778,8);assert(got==reinterpret_cast<uintptr_t>(compatPortMode));
 setup();p={};uintptr_t unknown=0x600000;memcpy(bytes.data()+0x141778,&unknown,8);assert(!wexRplReferencePortHooks(p,1,a,bytes.size())&&!p.writes&&!p.routes&&portBindings[2]);
 setup();p={};bytes[0x91836]=0;assert(!wexRplReferencePortHooks(p,1,a,bytes.size())&&!p.writes&&!p.routes);
 setup();p={};p.badSymbol=true;assert(!wexRplReferencePortHooks(p,1,a,bytes.size())&&!p.writes);
 setup();p={};p.failWrite=true;assert(!wexRplReferencePortHooks(p,1,a,bytes.size())&&p.writes==1&&portBindings[2]);
 setup();p={};panelState[10]=0;assert(!wexRplReferencePortHooks(p,1,a,bytes.size())&&!p.writes&&!p.routes);
 setup();p={};selected=false;assert(wexRplReferencePortHooks(p,1,a,bytes.size())&&!p.writes&&!p.routes);
 selected=true;observe=false;assert(wexRplReferencePortHooks(p,1,a,bytes.size())&&!p.writes&&!p.routes);
 observe=true;auxProbeFlag=true;setup();p={};assert(wexRplReferencePortHooks(p,1,a,bytes.size())&&p.writes==2&&auxReadEntry==a+0x28D9E&&getPortEntry==a+0x25AB2);
 setup();p={};bytes[0x28D9E]=0;assert(!wexRplReferencePortHooks(p,1,a,bytes.size())&&!p.writes&&!p.routes);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-port-bindings-') as d:
 p=Path(d);(p/'test.cpp').write_text(f);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print('PASS actual binding hook: exact three entries/two tables, pure-virtual replacement, unknown target/prefix/ABI/write failures reject, flag-off unchanged')
