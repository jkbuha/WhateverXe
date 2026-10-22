"""Execute the production captured-capability RAM replay boundary with mocks."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_caps_shadow.cpp').read_text()
body=s[s.index('namespace RaptorLakeP {namespace CapsShadow {'):]
fixture=r'''

#include "kern_raptorlake_p_cache_shadow_core.hpp"
#include "kern_raptorlake_p_page_pool_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <vector>
using mach_vm_address_t=uintptr_t;
struct IOService {};
unsigned mode,displayCalls,allocations,frees;uint32_t *mmio;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
struct IOMapper{inline static void *gSystem=nullptr;};
bool checkKernelArgument(const char *s){if(!strcmp(s,"-wex-rplp-caps-shadow"))return mode!=1;if(!strcmp(s,"-wex-rplp-manual-arm"))return mode!=3;if(!strcmp(s,"-wex-rplp-bcs"))return false;if(!strcmp(s,"-wex-rplp-rcs"))return mode==38;return true;}
void *IOMalloc(size_t n){if(mode==23)return nullptr;++allocations;return malloc(n);}
void IOFree(void *p,size_t n){assert(n==0x6000);++frees;free(p);}
void IODelay(unsigned n){assert(n==100);if(mode!=15){mmio[0x130044/4]=mmio[0xa188/4]&1;mmio[0xd84/4]=mmio[0xa278/4]&1;}}
struct IOMemoryMap{uint64_t length=0x1000000,physical=0x4011000000ULL;uintptr_t address;uint64_t getLength(){return length;}uint64_t getPhysicalAddress(){return physical;}uintptr_t getVirtualAddress(){return address;}};
struct IOPCIDevice{
 XELP::Profile profile=XELP::identify(0xa7a08086,4);std::vector<uint64_t> result,admit;
 uint16_t configRead16(unsigned o){if(o==4)return mode==7?0:2;assert(o==0x50);return 0x2c1;}
 uint32_t configRead32(unsigned o){if(o==0)return mode==5?0:0xa7a08086;if(o==8)return mode==6?5:4;if(o==0x10)return 0x11000004;if(o==0x14)return 0x40;if(o==0xc0)return 0x6c800000;if(o==0xc4)return 0;assert(false);return 0;}
 void setProperty(const char *k,const void *p,unsigned n){if(!strcmp(k,"WhateverXeRPLPCapsShadow"))result.assign((const uint64_t *)p,(const uint64_t *)p+n/8);else{assert(!strcmp(k,"WhateverXeRPLPCapsRegisters"));}}
};
namespace XELP{Profile deviceProfile(IOPCIDevice *p){return p->profile;}}
namespace RaptorLakeP {
 enum class DisplayStatus{Skipped,Complete,Unavailable,PowerChanged};struct DisplayCapture{DisplayStatus status=DisplayStatus::Complete;uint32_t data=123;};
 template<class IO>DisplayCapture captureDisplay(IO &,bool enabled){assert(enabled);++displayCalls;DisplayCapture x;if((mode==18&&displayCalls==1)||(mode==19&&displayCalls==2))x.status=DisplayStatus::PowerChanged;return x;}
 namespace GTTSlot{template<class IO>bool qualify(IO &,XELPMemory::Layout l){assert(l.valid);return mode!=13;}}
}

struct KernelPatcher {
 template<class T>T solveSymbol(size_t,const char *s,uintptr_t b,size_t){unsigned o=strstr(s,"10getGPUInfo")?0x2847e:strstr(s,"16initHardwareCaps")?0x2a666:strstr(s,"17initModeRegisters")?0x28e1a:0x14a5b8;return T(b+o+(mode==22));}
 void clearError(){}
};
BODY
using namespace RaptorLakeP::CapsShadow;
void info(void *owner){
 assert(scopeOwner==owner&&scopeBase==read<uintptr_t>(owner,0x1240));
 assert(consumeWake(mode==27?nullptr:scopeOwner,1,mode==28?4:5));
 if(mode==25)consumeWake(scopeOwner,1,5);
 write(owner,0x1124,uint32_t(mode==29?72:96));write(owner,0x1158,uint32_t(12));write(owner,0x115c,uint32_t(1));
 write(owner,0x1164,uint32_t(8));write(owner,0x1140,uint32_t(768));write(owner,0x1188,uint32_t(mode==36?0:19200000));
 *timestamp=mode==35?0:1920;
 if(mode==30)write(owner,0x1200,uint32_t(1));
 if(mode==31)reinterpret_cast<uint32_t *>(scopeBase)[0x7770/4]=1;
 if(mode==32)reinterpret_cast<uint32_t *>(scopeBase)[-1]^=1;
 if(mode==24)consumeWake(scopeOwner,0,4);else if(mode!=26)consumeWake(scopeOwner,0,5);
}
void modes(void *owner){
 auto ram=reinterpret_cast<uint32_t *>(read<uintptr_t>(owner,0x1240));
 if(mode!=37){ram[0x229c/4]=mode==33?0:0x80000;ram[0x2229c/4]=0x80000;ram[0x14800/4]=0x10000;}
 if(mode==34)write(owner,0x1124,uint32_t(1));
}
int main(int argc,char **argv){
 assert(argc==2);mode=atoi(argv[1]);std::vector<uint8_t> image(0x14b000);auto b=(uintptr_t)image.data();
 memcpy(image.data()+0x2847e,"\x55\x48\x89\xe5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x48\x83\xec\x18",17);
 memcpy(image.data()+0x28e1a,"\x55\x48\x89\xe5\x48\x8b\x87\x00\x13\x00\x00",11);
 if(mode==21)image[0x2847e]^=1;
 KernelPatcher p;bool ready=prepare(p,7,b,image.size());
 if(mode==3||mode==21||mode==22){assert(!ready);return 0;}assert(ready);
 nativeInfo=(uintptr_t)info;nativeMode=(uintptr_t)modes;if(!timestamp)timestamp=(volatile uint64_t *)(b+0x14a5b8);*timestamp=0x87654321;
 assert(!posix_memalign((void **)&mmio,4096,0x1000000));memset(mmio,0,0x1000000);
 mmio[0x9138/4]=1;mmio[0x913c/4]=0x3f;mmio[0x9118/4]=mode==14?1:0;mmio[0x9140/4]=0xe00fa;
 if(mode==16)mmio[0xa188/4]=mmio[0x130044/4]=1;
 if(mode==17)mmio[0x2030/4]=8;
 if(mode==20)mmio[0x145948/4]=UINT32_MAX;
 IOPCIDevice pci;IOMemoryMap map;map.address=(uintptr_t)mmio;
 if(mode==4)IOMapper::gSystem=(void *)1;if(mode==8)pci.profile=XELP::identify(0xa7a08086,5);
 if(mode==9)map.length=4096;if(mode==10)map.physical=0;if(mode==11)map.address=0;if(mode==12)++map.address;
 const size_t bytes=0x1002000;auto allocation=(uint32_t *)malloc(bytes);memset(allocation,0xa5,bytes);
 bool ok=run(&pci,&map,allocation,mode!=2);
 if(mode==1){assert(ok&&!record[Requested]&&pci.result.empty());free(mmio);free(allocation);return 0;}
 assert(ok==(mode==0));assert(pci.result.size()==Fields);
 assert(*timestamp==0x87654321&&!scopeOwner&&!scopeBase);
 assert(!mmio[0x229c/4]&&!mmio[0x2229c/4]&&!mmio[0x14800/4]);
 assert(allocations==frees);
 if(mode==0){assert(record[Replayed]&&record[ModeEntered]&&record[ModeWords]==3&&record[Freed]&&!record[Failure]&&record[Captured]==14&&record[Stable]&&record[LeaseReleased]==3&&record[GlobalRestored]);}
 if(mode>=24&&mode<=28)assert(!record[ModeEntered]&&record[WakeFailure]+(mode==26)>0);
 if(mode==30||mode==34)assert(record[OwnerMismatch]);
 if(mode==31||mode==33)assert(record[RAMMismatch]);
 if(mode==32)assert(record[Guards]);
 assert(!consumeWake(nullptr,1,5));assert(!run(&pci,&map,allocation,true));free(mmio);free(allocation);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-caps-shadow-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in range(39):subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS production caps replay: 39 identity/donor/mapping/lease/display/wake/output/footprint/cleanup cases; no hardware mode writes')
