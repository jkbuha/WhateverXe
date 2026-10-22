"""Run the production cache diagnostic against RAM-backed PCI/MMIO fixtures."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_cache_shadow.cpp').read_text();body=s[s.index('namespace RaptorLakeP {namespace CacheShadow {'):]
fixture=r'''
#include "kern_raptorlake_p_cache_shadow_core.hpp"
#include "kern_raptorlake_p_page_pool_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include <cassert>
#include <cstdlib>
#include <vector>
#include <string>
using mach_vm_address_t=uintptr_t;
unsigned mode,originalCalls,bufferReleases,mapReleases;uint32_t *physicalMMIO;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
struct IOMapper{inline static void *gSystem=nullptr;};
bool checkKernelArgument(const char *s){if(!strcmp(s,"-wex-rplp-cache-shadow"))return mode!=9;if(!strcmp(s,"-wex-rplp-native-factory"))return mode!=7;if(!strcmp(s,"-wex-rplp-bcs"))return mode==36;if(!strcmp(s,"-wex-rplp-rcs"))return mode==37;return true;}
void IODelay(unsigned n){assert(n==100);if(mode!=38){physicalMMIO[0x130044/4]=physicalMMIO[0xa188/4]&1;physicalMMIO[0xd84/4]=physicalMMIO[0xa278/4]&1;}if(mode==39&&physicalMMIO[0xa188/4]==0x10000)physicalMMIO[0x130044/4]=1;}
struct Meta{unsigned size=0x118;unsigned getClassSize(){return size;}};
struct OSObject{virtual ~OSObject()=default;uint8_t bytes[0x2000]{};Meta meta;Meta *getMetaClass(){return &meta;}};
struct IOService:OSObject{};
OSObject *target;IOService *accelerator;
struct IOMemoryMap:OSObject{
 bool owned=false;uintptr_t virtualBase=0;uint64_t length=0x1000000,physical=0x4011000000ULL;
 uint64_t getLength(){return length;}uint64_t getPhysicalAddress(){return physical;}uintptr_t getVirtualAddress(){return virtualBase;}
 void release(){assert(owned);++mapReleases;delete this;}
};
constexpr unsigned kIODirectionInOut=1,kIOMemoryMapperNone=2;
struct IOBufferMemoryDescriptor {
 uint8_t *data;uint64_t length;
 static IOBufferMemoryDescriptor *withOptions(unsigned o,uint64_t n,unsigned a){assert(o==3&&a==4096);if(mode==23)return nullptr;auto b=new IOBufferMemoryDescriptor;b->length=n;b->data=(uint8_t *)malloc(n);return b;}
 IOMemoryMap *map(){if(mode==24)return nullptr;auto m=new IOMemoryMap;m->owned=true;m->virtualBase=(uintptr_t)data;m->length=length-(mode==25?4:0);return m;}
 void release(){++bufferReleases;free(data);delete this;}
};
struct IOPCIDevice:IOService{
 XELP::Profile profile=XELP::identify(0xa7a08086,4);std::vector<uint64_t> receipt;std::vector<uint32_t> registers;
 uint16_t configRead16(unsigned o){assert(o==4);return mode==13?0:2;}
 uint32_t configRead32(unsigned o){if(o==0)return mode==11?0:0xa7a08086;if(o==8)return mode==12?5:4;if(o==0x10)return 0x11000004;if(o==0x14)return 0x40;assert(false);return 0;}
 void setProperty(const char *s,const void *p,unsigned n){if(!strcmp(s,"WhateverXeRPLPCacheShadow"))receipt.assign((const uint64_t *)p,(const uint64_t *)p+n/8);else{assert(!strcmp(s,"WhateverXeRPLPCacheRegisters"));registers.assign((const uint32_t *)p,(const uint32_t *)p+n/4);}}
};
struct KernelPatcher;
namespace RaptorLakeP{namespace CapsShadow{bool enabled(){return false;}bool prepare(KernelPatcher &,size_t,uintptr_t,size_t);bool consumeWake(IOService *,uint32_t,uint32_t){return false;}bool run(IOPCIDevice *,IOMemoryMap *,uint32_t *,bool){assert(false);return false;}}}
namespace RaptorLakeP{namespace CacheTransaction{bool enabled(){return mode==43||mode==44;}bool run(IOPCIDevice *,IOMemoryMap *,const uint8_t *,const uint32_t *,const uint32_t *,const uint32_t (*)[3],bool prerequisite){assert(prerequisite&&!bufferReleases&&!mapReleases);return mode!=44;}}}
namespace XELP{Profile deviceProfile(IOPCIDevice *p){return p->profile;}}
void originalWakeStub(IOService *,uint32_t,uint32_t){++originalCalls;}
struct KernelPatcher{
 struct RouteRequest{const char *s;uintptr_t f;uintptr_t &o;template<class F>RouteRequest(const char *n,F fn,uintptr_t &original):s(n),f(reinterpret_cast<uintptr_t>(fn)),o(original){}};
 template<class T>T solveSymbol(size_t,const char *s,uintptr_t b,size_t){unsigned off=strstr(s,"9initCache")?0xfe1a:strstr(s,"13SafeForceWake")?0x28c2a:strstr(s,"14initGTL3Layout")?0x4ba56:strstr(s,"20initMOCS")?0x4ba7c:0x4be72;return T(b+off+(mode==1));}
 bool routeMultiple(size_t,RouteRequest *r,unsigned n,uintptr_t,size_t){assert(n==1);r->o=(uintptr_t)originalWakeStub;return mode!=6;}void clearError(){}
};
namespace RaptorLakeP{namespace CapsShadow{bool prepare(KernelPatcher &,size_t,uintptr_t,size_t){assert(false);return false;}}}
BODY
using namespace RaptorLakeP::CacheShadow;
void replay(OSObject *m) {
 assert(m==target&&read<uintptr_t>(m,0x18)==read<uintptr_t>(accelerator,0x1240));auto ram=(uint32_t *)read<uintptr_t>(m,0x18);
 forcewake(mode==30?nullptr:accelerator,1,mode==29?4:5);
 if(mode==26)forcewake(accelerator,1,5);
 for(unsigned i=0;i<Count;++i)ram[offset(i)/4]=expected(i,pat,l3,mocs);
 write(m,0x108,uint64_t(mode==31?0:0x1800000));
 if(mode==27)ram[0x7770/4]=0;
 if(mode==28)ram[-1]^=1;
 if(mode==32)ram[0x4024/4]=0xf0007fff;
 if(mode!=33)forcewake(accelerator,0,5);
}
int main(int argc,char **argv) {
 assert(argc==2);mode=atoi(argv[1]);std::vector<uint8_t> image(0xd7000);auto b=(uintptr_t)image.data();
 memcpy(image.data()+0xfe1a,"\x55\x48\x89\xe5\x41\x56\x53\x49",8);
 memcpy(image.data()+0x28c2a,"\x55\x48\x89\xe5\x41\x57\x41\x56\x41\x55\x41\x54\x53\x50\x85\xd2",16);
 write(image.data(),0xd6340,b+0x4ba7c);write(image.data(),0xd6348,b+0x4ba56);write(image.data(),0xd6358,b+0x4be72);
 unsigned sites[]{0xff18,0x4ba8d,0x4be88},tables[]{0xa00d0,0xbecd8,0xbecd0};
 for(unsigned i=0;i<3;++i){memcpy(image.data()+sites[i],"\x48\x8d\x15",3);write(image.data(),sites[i]+3,int32_t(tables[i]-sites[i]-7));}
 if(mode==2)image[0xfe1a]^=1;if(mode==3)image[0x28c2a]^=1;if(mode==4)image[0xd6340]^=1;if(mode==5)image[0xff1b]^=1;
 KernelPatcher p;auto ready=prepare(p,7,b,mode==8?0x100:image.size());
 if((mode>=1&&mode<=8)){assert(!ready&&!record[PatchReady]);return 0;}assert(ready);
 OSObject m;IOService owner;IOPCIDevice pci;IOMemoryMap map;target=&m;accelerator=&owner;
 assert(!posix_memalign((void **)&physicalMMIO,4096,Bytes));memset(physicalMMIO,0,Bytes);
 physicalMMIO[0x9138/4]=1;physicalMMIO[0x913c/4]=mode==40?0:0x3f;
 if(mode==41)physicalMMIO[0xa188/4]=physicalMMIO[0x130044/4]=1;
 if(mode==42)physicalMMIO[0x2030/4]=8;
 std::vector<uint32_t> before;for(size_t i=0;i<Count;++i){physicalMMIO[offset(i)/4]=0x12345600+i;before.push_back(physicalMMIO[offset(i)/4]);}
 map.virtualBase=(uintptr_t)physicalMMIO;write(&m,0x18,map.virtualBase);write(&m,0x10,&owner);write(&m,0x68,&map);write(&m,0x110,uintptr_t(0x1234));write(&m,0x108,uint64_t(0x87654321));write(&owner,0x1240,uintptr_t(0x11223344));
 expectedVtable=read<uintptr_t>(&m,0);nativeInit=(uintptr_t)replay;
 if(mode==10)IOMapper::gSystem=(void *)1;if(mode==14)m.meta.size=0x110;if(mode==15)++expectedVtable;if(mode==16)write(&m,0x10,uintptr_t(0));if(mode==17)write(&m,0x110,uintptr_t(0));
 if(mode==18)write(&m,0x68,uintptr_t(0));if(mode==19)map.length=4096;if(mode==20)map.physical=0;if(mode==21)map.virtualBase=0;if(mode==22)write(&m,0x18,uintptr_t(4));if(mode==34)pci.profile=XELP::identify(0xa7a08086,5);
 if(mode==35)version[0]='x';
 const auto oldMMIO=read<uintptr_t>(&m,0x18),oldOwner=read<uintptr_t>(&owner,0x1240);const auto oldCache=read<uint64_t>(&m,0x108);
 const bool result=run(&m,&owner,&pci);
 if(mode==9){assert(result&&!record[Requested]);free(physicalMMIO);return 0;}
 const bool expectPass=mode==0||mode==43||mode==38||mode==40||mode==41||mode==42;
 assert(result==expectPass&&pci.receipt.size()==Fields);
 assert(read<uintptr_t>(&m,0x18)==oldMMIO&&read<uintptr_t>(&owner,0x1240)==oldOwner&&read<uint64_t>(&m,0x108)==oldCache);
 for(unsigned i=0;i<Count;++i)assert(physicalMMIO[offset(i)/4]==before[i]);
 assert(!originalCalls);assert(!run(&m,&owner,&pci));
 if(record[NativeEntered]){assert(record[PointersRestored]&&record[BufferReleased]&&bufferReleases==1&&mapReleases==1);}
 if(mode==0){assert(record[HardwareQualified]&&record[SampleCount]==Count&&record[LeaseWrites]==4&&record[LeaseReleased]==3);for(unsigned i=0;i<Count;++i)assert(samples[i][0]==offset(i)&&samples[i][1]==before[i]&&samples[i][2]==before[i]);}
 if(mode==38||mode==40||mode==41||mode==42)assert(!record[HardwareQualified]&&!record[SampleCount]);
 if(mode==39)assert(record[Failure]==3&&!record[NativeEntered]&&record[LeaseReleaseFailed]);
 if(mode==27||mode==32)assert(record[Unexpected]==1);
 if(mode==28)assert(record[GuardErrors]==1);
 if(mode==26||mode==29||mode==30)assert(record[WakeFailure]);
 forcewake(&owner,1,5);assert(originalCalls==1);free(physicalMMIO);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-cache-shadow-adapter-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in range(45):subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS production cache diagnostic: 45 donor/identity/mapping/allocation/lease/idle/wake/content/restore/cleanup cases; hardware cache words unchanged')
