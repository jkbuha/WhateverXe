"""Production full-factory gate: measured capability, known object and cleanup."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_native_factory.cpp').read_text();body=s[s.index('namespace RaptorLakeP {namespace NativeFactory {'):]
fixture=r'''
#include "kern_raptorlake_p_native_factory_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_raptorlake_p_page_pool_core.hpp"
#include <cassert>
#include <cstdlib>
#include <vector>
using mach_vm_address_t=uintptr_t;
unsigned mode,releases,arrayCalls;bool arrayFreed=false;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
struct IOMapper{inline static void *gSystem=nullptr;};
bool checkKernelArgument(const char *s){
 if(!strcmp(s,"-wex-rplp-native-factory"))return mode!=6;if(!strcmp(s,"-wex-rplp-page-array"))return mode!=10;
 if(!strcmp(s,"-wex-rplp-page-pool"))return mode!=11;if(!strcmp(s,"-wex-rplp-manager-pools"))return mode!=12;
 if(!strcmp(s,"-wex-rplp-bcs"))return mode==13;if(!strcmp(s,"-wex-rplp-rcs"))return mode==14;return true;
}
struct Meta{unsigned size=0x118;unsigned getClassSize(){return size;}};
struct OSObject{virtual ~OSObject()=default;uint8_t bytes[0x118]{};Meta meta;Meta *getMetaClass(){return &meta;}void release(){assert(arrayFreed);++releases;}};
struct IOService:OSObject{};
std::vector<uint32_t> mmio(0x400000);
struct IOMemoryMap:OSObject{uint64_t getLength(){return mode==21?4096:0x1000000;}uint64_t getPhysicalAddress(){return mode==22?0:0x4011000000ULL;}uintptr_t getVirtualAddress(){return mode==23?0:reinterpret_cast<uintptr_t>(mmio.data())+(mode==25);}};
struct IOPCIDevice:IOService{
 XELP::Profile profile=XELP::identify(0xa7a08086,4);std::vector<uint64_t> receipt;
 uint16_t configRead16(unsigned o){assert(o==4);return mode==26?0:2;}
 uint32_t configRead32(unsigned o){if(o==0)return mode==27?0:0xa7a08086;if(o==8)return mode==28?5:4;if(o==0x10)return 0x11000004;if(o==0x14)return 0x40;assert(false);return 0;}
 void setProperty(const char *s,const void *p,unsigned n){assert(!strcmp(s,"WhateverXeRPLPNativeFactory"));receipt.assign((const uint64_t *)p,(const uint64_t *)p+n/8);}
};
namespace XELP{Profile deviceProfile(IOPCIDevice *p){return p->profile;}}
struct KernelPatcher;
namespace RaptorLakeP{namespace Subranges{bool enabled(){return mode>=45;}bool prepare(KernelPatcher &,size_t,uintptr_t,size_t){return mode!=48;}bool run(OSObject *,IOService *,IOPCIDevice *,bool prerequisite){assert(!arrayCalls&&!releases&&prerequisite);return mode!=46&&mode!=47;}bool held(){return mode==47;}}}
namespace RaptorLakeP{namespace CacheShadow{bool enabled(){return mode==43 || mode==44;}bool prepare(KernelPatcher &,size_t,uintptr_t,size_t);bool run(OSObject *,IOService *,IOPCIDevice *){assert(!arrayCalls&&!releases);return mode!=44;}}}
namespace RaptorLakeP{namespace PageArray{bool integrated(){return mode!=9;}bool releaseOwned(OSObject *p){++arrayCalls;if(mode==34)return false;arrayFreed=true;PagePool::write(p,0x110,uintptr_t(0));return true;}}}
struct KernelPatcher{
 struct RouteRequest{const char *s;uintptr_t f;uintptr_t &o;template<class F>RouteRequest(const char *n,F fn,uintptr_t &original):s(n),f(reinterpret_cast<uintptr_t>(fn)),o(original){}};
 template<class T>T solveSymbol(size_t,const char *s,uintptr_t b,size_t){return T(b+(strstr(s,"11detectEDRAM")?0x4baae:0xd61f8)+(mode==1));}
 bool routeMultiple(size_t,RouteRequest *,unsigned n,uintptr_t,size_t){assert(n==1);return mode!=5;}void clearError(){}
};
namespace RaptorLakeP{namespace CacheShadow{bool prepare(KernelPatcher &,size_t,uintptr_t,size_t){return true;}}}
BODY
using namespace RaptorLakeP::NativeFactory;
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);
 std::vector<uint8_t> image(0xd7000);uintptr_t b=reinterpret_cast<uintptr_t>(image.data());
 memcpy(image.data()+0x4baae,"\x55\x48\x89\xe5\x41\x57\x41\x56",8);write(image.data(),0xd6320,b+0xe580);write(image.data(),0xd6350,b+0x4baae);
 if(mode==2)image[0x4baae]^=1;if(mode==3)image[0xd6320]^=1;if(mode==4)image[0xd6350]^=1;
 KernelPatcher patcher;bool ready=prepare(patcher,7,b,mode==42?0x4baaf:image.size());
 if((mode>=1&&mode<=5)||mode==42||mode==48){assert(!ready&&(mode==48||!record[PatchReady]));return 0;}assert(ready);
 OSObject target,foreign;IOService accelerator;IOPCIDevice pci;IOMemoryMap map;
 expectedVtable=read<uintptr_t>(&target,0)+(mode==17);write(&target,0x10,mode==18?nullptr:&accelerator);write(&target,0x68,mode==20?nullptr:&map);
 write(&target,0x18,reinterpret_cast<uintptr_t>(mmio.data())+(mode==24?4:0));write(&target,0x110,uintptr_t(mode==19?0:0x1234));
 if(mode==16)target.meta.size=0x110;if(mode==8)IOMapper::gSystem=(void *)1;if(mode==15)pci.profile=XELP::identify(0xa7a08086,5);
 mmio[0x120010/4]=mode==29?1:mode==30?0xffffffffU:mode==38?0x102:0;
 bool armed=arm(mode==39?nullptr:&target,mode==40?nullptr:&accelerator,mode==41?nullptr:&pci,mode!=7);
 if(mode==6||mode==39||mode==40||mode==41){assert(!armed&&!record[Requested]);return 0;}
 assert(record[Requested]&&pci.receipt.size()==Fields);
 if(mode>=7&&mode<=15){assert(!armed&&record[Failure]==1&&!releases);return 0;}
 if(mode>=16&&mode<=19){assert(!armed&&record[Failure]==2&&!releases);return 0;}
 if(mode>=20&&mode<=28){assert(!armed&&record[Failure]==3&&!releases);return 0;}
 if(mode==29||mode==30){assert(!armed&&record[Failure]==4&&!releases);return 0;}
 assert(armed&&record[MapValid]&&record[Armed]&&!record[Failure]);
 if(mode==31)mmio[0x120010/4]=1;
 if(mode==32){write(&foreign,0x20,uint16_t(0xabcd));detect(&foreign);assert(read<uint16_t>(&foreign,0x20)==0xabcd);}
 if(mode!=35)detect(&target);
 if(mode==33)assert(!consume(&foreign)&&!releases&&!arrayCalls);
 assert(consume(&target));
 bool passed=finish(mode!=36);
 if(mode==0||mode==33||mode==37||mode==38||mode==43||mode==45){assert(passed&&record[ResultAccepted]&&record[ClassBytes]==0x118&&record[ArrayReleased]&&record[ManagerReleased]&&!record[Failure]);}
 else assert(!passed&&record[Failure]);
 if(mode==34||mode==47){assert(record[Retained]&&!record[ManagerReleased]&&!releases&&arrayCalls==(mode==47?0U:1U));}
 else assert(releases==1&&arrayCalls==1&&record[ArrayReleased]&&record[ManagerReleased]);
 assert(consume(&target)==((mode==34||mode==47)));assert(arrayCalls==(mode==47?0U:1U)&&releases==((mode==34||mode==47)?0U:1U));assert(!arm(&target,&accelerator,&pci,true));
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-native-factory-adapter-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in range(49):subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS production native factory: 49 exact-donor, identity, MMIO, capability, scoped detector, known-result, cleanup-order and retained-ownership cases')
