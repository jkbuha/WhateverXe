"""Production child lifecycle: guard, maps, exhaustion, partial cleanup and reuse."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_subranges.cpp').read_text();body=s[s.index('namespace RaptorLakeP {namespace Subranges {'):]
fixture=r'''
#include "kern_raptorlake_p_page_pool_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <cassert>
#include <cstdlib>
#include <vector>
#include <memory>
using mach_vm_address_t=uintptr_t;using IOByteCount=uint64_t;
unsigned mode,mapReleased,releaseCalls,superReleases,allocAttempts,allocations,eventCalls;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
struct IOMapper{inline static void *gSystem=nullptr;};
bool checkKernelArgument(const char *s){if(!strcmp(s,"-wex-rplp-subranges"))return mode!=8;if(!strcmp(s,"-wex-rplp-native-factory"))return mode!=7;if(!strcmp(s,"-wex-rplp-cache-shadow"))return mode!=9;if(!strcmp(s,"-wex-rplp-bcs"))return mode==10;if(!strcmp(s,"-wex-rplp-rcs"))return false;return true;}
void *IOMalloc(unsigned n){assert(n==4096);return mode==24?nullptr:malloc(n);}void IOFree(void *p,unsigned n){assert(n==4096);free(p);}
struct Meta{unsigned size=0x118;unsigned getClassSize(){return size;}};
struct OSObject{virtual ~OSObject()=default;uint8_t bytes[0x2000]{};Meta meta;Meta *getMetaClass(){return &meta;}void release();};
struct IOService:OSObject{};
std::vector<uint32_t> backing(1024,0x5a5a5a5a),alternate(1024,0x5a5a5a5a);
struct IOMemoryMap{uintptr_t address;uint64_t length=4096;uint64_t getLength(){return length;}uintptr_t getVirtualAddress(){return address;}void release(){++mapReleased;delete this;}};
struct IOMemoryDescriptor:OSObject {
 bool isChild=false;uint64_t length=4096,physical=0x712340000ULL;IOMemoryDescriptor(){meta.size=0x90;}
 uint64_t getLength(){return length;}uint64_t getPhysicalSegment(unsigned off,IOByteCount *span,unsigned){assert(!off);*span=4096;return physical;}
 IOMemoryMap *map(){if(mode==30&&isChild)return nullptr;auto m=new IOMemoryMap;m->address=(uintptr_t)((isChild&&(mode==31||mode==32))?alternate.data():backing.data());if(mode==33&&isChild)m->length=4092;return m;}
};
IOMemoryDescriptor *mockCast(OSObject *p){return p?static_cast<IOMemoryDescriptor *>(p):nullptr;}
#define OSDynamicCast(T,p) mockCast(p)
constexpr unsigned kIOMemoryMapperNone=2;
struct IOPCIDevice:IOService{
 XELP::Profile profile=XELP::identify(0xa7a08086,4);std::vector<uint64_t> receipt;
 uint16_t configRead16(unsigned o){assert(o==0x50);return 0x2c1;}
 uint32_t configRead32(unsigned o){if(o==0)return mode==12?0:0xa7a08086;if(o==8)return mode==13?5:4;if(o==0xc0)return 0x6c800000;if(o==0xc4)return 0;assert(false);return 0;}
 void setProperty(const char *s,const void *p,unsigned n){assert(!strcmp(s,"WhateverXeRPLPSubranges"));receipt.assign((const uint64_t *)p,(const uint64_t *)p+n/8);}
};
namespace XELP{Profile deviceProfile(IOPCIDevice *p){return p->profile;}}
namespace RaptorLakeP{namespace CacheShadow{bool passed(){return mode!=11;}}namespace PageArray{bool integrated(){return mode!=14;}}}
void originalFreeStub(OSObject *);void superFreeStub(OSObject *);
struct KernelPatcher{
 struct RouteRequest{const char *s;uintptr_t f;uintptr_t &o;template<class F>RouteRequest(const char *n,F fn,uintptr_t &original):s(n),f(reinterpret_cast<uintptr_t>(fn)),o(original){}};
 template<class T>T solveSymbol(size_t,const char *s,uintptr_t b,size_t){unsigned off=strstr(s,"Pool8allocate")?0xc876:strstr(s,"10deallocate")?0xc91e:strstr(s,"12withSubRange")?0xfc90:strstr(s,"Descriptor4free")?0xfd58:strstr(s,"Manager14registerEvents")?0xf450:strstr(s,"PagePool14registerEvents")?0xba8c:strstr(s,"ZTV18")?0xcbff8:0xcc710;return T(b+off+(mode==1));}
 bool routeMultiple(size_t,RouteRequest *r,unsigned n,uintptr_t,size_t){assert(n==1);r->o=(uintptr_t)originalFreeStub;return mode!=6;}void clearError(){}
};
BODY
using namespace RaptorLakeP::Subranges;
OSObject *thePool;IOMemoryDescriptor *parent;std::vector<std::unique_ptr<IOMemoryDescriptor>> children;
namespace P=RaptorLakeP::PagePool;
void superFreeStub(OSObject *){++superReleases;}
void originalFreeStub(OSObject *obj){assert(obj==expectedFree&&P::read<void *>(obj,0x70)==thePool);++releaseCalls;
 if(mode==34)return;
 P::write(thePool,0x58,P::read<uint64_t>(thePool,0x58)+1);P::write(thePool,0x68,uint64_t(0));P::write(thePool,0x70,uintptr_t(0));P::write(thePool,0x78,(uintptr_t)thePool+0x70);
}
void OSObject::release(){descriptorFree(this);}
OSObject *allocateStub(OSObject *pool,uint64_t n) {
 assert(pool==thePool&&n==4096);++allocAttempts;
 if(P::read<uint64_t>(pool,0x68)){if(mode!=29)return nullptr;}
 if(mode==25)return nullptr;
 children.emplace_back(new IOMemoryDescriptor);auto d=children.back().get();d->isChild=true;
 if(mode==26){descriptorFree(d);return nullptr;}
 ++allocations;P::write(d,0x70,pool);P::write(d,0x78,uint32_t(2));P::write(d,0x88,(uintptr_t)pool+0x70);
 P::write(pool,0x50,P::read<uint64_t>(pool,0x50)+1);P::write(pool,0x68,uint64_t(4096));P::write(pool,0x70,d);P::write(pool,0x78,(uintptr_t)d+0x80);
 if(mode==27)d->physical+=4096;if(mode==28)d->meta.size=0x88;
 return d;
}
bool eventsStub(OSObject *m){++eventCalls;assert(record[ChildReleased]==2&&record[EmptyRounds]==2);if(mode==35)return false;if(mode==36){auto ps=P::read<OSObject **>(m,0x110);P::write(ps[3],0x40,uintptr_t(1));}return true;}
int main(int argc,char **argv) {
 assert(argc==2);mode=atoi(argv[1]);std::vector<uint8_t> image(0xd7000);uintptr_t b=(uintptr_t)image.data();
 for(auto o:{0xc876,0xc91e,0xfc90,0xfd58,0xf450,0xba8c})memcpy(image.data()+o,"\x55\x48\x89\xe5",4);
 std::vector<uint8_t> svt(0xa8);P::write(svt.data(),0xa0,(uintptr_t)superFreeStub);P::write(image.data(),0xc81a0,(uintptr_t)svt.data());P::write(image.data(),0xcc7b0,b+0xfd58);
 uint32_t count=mode==15?15:16;P::write(image.data(),0xc8240,&count);
 if(mode==2)image[0xc876]^=1;if(mode==3)image[0xcc7b0]^=1;if(mode==4)P::write(image.data(),0xc81a0,uintptr_t(0));if(mode==5)P::write(svt.data(),0xa0,uintptr_t(0));
 KernelPatcher patcher;const bool ready=prepare(patcher,7,b,image.size());
 if((mode>=1&&mode<=7)||mode==9){assert(!ready&&!record[PatchReady]);return 0;}assert(ready);
 OSObject manager,pool;IOService owner;IOPCIDevice pci;IOMemoryDescriptor desc;thePool=&pool;parent=&desc;
 std::vector<OSObject> pages(16);std::vector<OSObject *> array;for(auto &p:pages){p.meta.size=0x78;P::write(&p,0x18,&owner);array.push_back(&p);}
 P::write(&manager,0x10,&owner);P::write(&manager,0x78,&pool);P::write(&manager,0x110,array.data());
 pool.meta.size=0x80;poolVtable=P::read<uintptr_t>(&pool,0);descriptorVtable=P::read<uintptr_t>(&desc,0);
 P::write(&pool,0x10,&owner);P::write(&pool,0x18,&desc);P::write(&pool,0x30,uint64_t(0x712340000ULL));P::write(&pool,0x60,uint64_t(4096));P::write(&pool,0x78,(uintptr_t)&pool+0x70);
 allocate=(uintptr_t)allocateStub;registerEvents=(uintptr_t)eventsStub;
 if(mode==16)IOMapper::gSystem=(void *)1;if(mode==17)pool.meta.size=0x78;if(mode==18)desc.physical=0x6c800000;if(mode==19)P::write(&pool,0x78,uintptr_t(0));
 if(mode==20)P::write(&manager,0x110,uintptr_t(0));if(mode==21)P::write(&pages[9],0x64,uint8_t(1));if(mode==22)P::write(&pages[7],0x40,uintptr_t(1));if(mode==23)P::write(&manager,0x10,uintptr_t(0));
 if(mode==31)alternate[0]=0;
 bool passed=run(&manager,&owner,&pci,mode!=37);
 if(mode==8){assert(passed&&!record[Requested]);return 0;}
 assert(passed==(mode==0));assert(pci.receipt.size()==Fields);
 if(mode==0){assert(record[Created]==2&&record[DescriptorValid]==2&&record[ExhaustionRefused]==2&&record[AliasWords]==2048&&record[RestoredWords]==2048&&record[ChildReleased]==2&&record[EmptyRounds]==2&&record[PagesUnchanged]&&!held()&&!record[Failure]);}
 if(mode==26)assert(superReleases==1&&record[PartialFreed]==1&&!held());
 if(mode==27||mode==28||mode==29||mode==34)assert(held()&&record[Retained]);
 if(mode==30||mode==31||mode==32||mode==33)assert(releaseCalls==1&&!held()&&record[EmptyRounds]==1);
 for(auto word:backing)assert(word==0x5a5a5a5a);
 assert(!run(&manager,&owner,&pci,true));
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-subranges-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in range(38):subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS production subrange diagnostic: 38 identity/symbol/vtable/prerequisite/partial-free/descriptor/exhaustion/alias/restore/reuse/event/uncertain-ownership cases')
