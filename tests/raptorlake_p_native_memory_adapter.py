"""Exercise production native mapping stop/cleanup with deterministic IOKit mocks."""
from pathlib import Path
import subprocess
import tempfile
r = Path(__file__).resolve().parents[1]
s = (r/'WhateverXe/kern_raptorlake_p_native_memory.cpp').read_text()
body = s[s.index('namespace RaptorLakeP {namespace NativeMemory {'):]
fixture = r'''
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
using mach_vm_address_t=uintptr_t;using IOByteCount=uint64_t;constexpr unsigned kIOMemoryMapperNone=0x800,kIOMapReadOnly=0x1000;
unsigned mapsCreated=0,mapsReleased=0;
#define OSDynamicCast(t,p) dynamic_cast<t *>(p)
struct IOMapper{inline static void *gSystem=nullptr;};
unsigned mode,originalCalls=0,releaseCalls=0,factoryCalls=0;
bool pagesFlag=false,memoryFlag=true,probeFlag=true,nativeFlag=true,bcsFlag=false,rcsFlag=false;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
bool checkKernelArgument(const char *s){if(!strcmp(s,"-wex-rplp-native-pages"))return pagesFlag;if(!strcmp(s,"-wex-rplp-native-memory"))return memoryFlag;if(!strcmp(s,"-wex-rplp-probe"))return probeFlag;if(!strcmp(s,"-wex-rplp-native-probe"))return nativeFlag;if(!strcmp(s,"-wex-rplp-bcs"))return bcsFlag;if(!strcmp(s,"-wex-rplp-rcs"))return rcsFlag;assert(false);return false;}
struct OSMetaClass{unsigned size=0x2000;unsigned getClassSize()const{return size;}};
struct OSObject{virtual ~OSObject()=default;uint8_t bytes[0x2000]{};OSMetaClass meta;unsigned refs=1;const OSMetaClass *getMetaClass(){return &meta;}void release(){assert(refs>0);--refs;}};
struct IOService:OSObject{};
struct IOMemoryMap:OSObject{bool page=false;uintptr_t cpu=0;uint64_t length=4096;uint64_t getLength(){return page?length:(mode==14?0x1000:0x1000000);}uint64_t getPhysicalAddress(){return mode==15?0:0x4011000000;}uintptr_t getVirtualAddress(){return page?cpu:(mode==16?0:0x12340000);}void release(){++mapsReleased;OSObject::release();}};
struct IOBufferMemoryDescriptor:OSObject{alignas(8) uint8_t content[4096]{};uint64_t physical=0;uint64_t getPhysicalSegment(unsigned,IOByteCount *span,unsigned option){assert(option==kIOMemoryMapperNone);*span=mode==34?0:4096;return physical;}uint64_t getLength(){return mode==36&&physical==0x123000?4095:4096;}void *getBytesNoCopy(){return nullptr;}IOMemoryMap view;IOMemoryMap *map(unsigned options){assert(options==kIOMapReadOnly);if(mode==35&&physical==0x123000)return nullptr;++mapsCreated;view.refs=1;view.page=true;view.cpu=mode==40&&physical==0x123000?0:reinterpret_cast<uintptr_t>(content);view.length=mode==39&&physical==0x123000?4095:4096;return &view;}};
IOBufferMemoryDescriptor dataPage,tablePage;
bool allocateNative(void **,uint64_t *,uint32_t,uint32_t);bool dummyNative(OSObject *);void cleanupNative(OSObject *);
struct Aperture{uint64_t getLength(){return 0x10000000;}};
struct IOPCIDevice:IOService{XELP::Profile profile=XELP::identify(0xA7A08086,4);Aperture aperture;std::vector<uint8_t> property;uint32_t configRead32(unsigned off){if(off==0x14)return 0x40;if(off==0x10)return 0x11000004;if(off==0xC0)return 0x6c800001;if(off==0xC4)return 0;assert(false);return 0;}uint16_t configRead16(unsigned off){assert(off==0x50);return 0x2c1;}Aperture *getDeviceMemoryWithRegister(unsigned off){assert(off==0x18);return mode==17?nullptr:&aperture;}void setProperty(const char *key,const void *p,unsigned size){assert(!strcmp(key,"WhateverXeRPLPNativeMemory"));property.assign(static_cast<const uint8_t *>(p),static_cast<const uint8_t *>(p)+size);}};
namespace XELP{Profile deviceProfile(IOPCIDevice *pci){return pci->profile;}}
void originalRelease(OSObject *o){++releaseCalls;uintptr_t zero=0;for(auto off:{0x18,0x68,0x70})memcpy(reinterpret_cast<uint8_t *>(o)+off,&zero,8);}
bool original(OSObject *){++originalCalls;assert(false);return true;}
struct KernelPatcher {
 struct RouteRequest{const char *symbol;uintptr_t replacement;uintptr_t &original;template<class F>RouteRequest(const char *s,F f,uintptr_t &o):symbol(s),replacement(reinterpret_cast<uintptr_t>(f)),original(o){}};
 template<class T>T solveSymbol(size_t,const char *name,uintptr_t base,size_t){
  unsigned off=strstr(name,"ENK3")?0xf22e:strstr(name,"17releaseDummy")?0xef1e:strstr(name,"19createMemory")?0x2d24a:strstr(name,"16newMemory")?0x28a00:strstr(name,"14initDummy")?0xec76:strstr(name,"13initGlobal")?0xecda:0xf05c;
  return T(base+off+(mode==8?1:0));
 }
 void clearError(){}
 bool routeMultiple(size_t,RouteRequest *req,unsigned count,uintptr_t,size_t){assert(count==3 || count==2);for(unsigned i=0;i<count;++i)req[i].original=count==2?(i?reinterpret_cast<uintptr_t>(cleanupNative):reinterpret_cast<uintptr_t>(allocateNative)):(i==2?reinterpret_cast<uintptr_t>(originalRelease):i==0&&pagesFlag?reinterpret_cast<uintptr_t>(dummyNative):reinterpret_cast<uintptr_t>(original));return mode!=11 && !(mode==24 && count==2);}
};
namespace RaptorLakeP{namespace GTTShadow{
bool enabled(){return mode>=41;}bool prepare(KernelPatcher &,size_t,uintptr_t,size_t){return mode!=42;}
bool run(OSObject *,IOService *,IOPCIDevice *,uintptr_t){return mode!=43;}void finish();bool aperturePassed(){return mode!=46;}bool slotPassed(){return mode!=50;}
}}
namespace RaptorLakeP{namespace NativePools{bool enabled(){return mode>=44;}bool prepare(KernelPatcher &,size_t,uintptr_t,size_t){return mode!=45;}void run(IOService *,IOPCIDevice *,bool);bool passed(){return mode!=54;}}}
namespace RaptorLakeP{namespace NativeChain{bool enabled(){return mode>=47;}bool prepare(KernelPatcher &,size_t,uintptr_t,size_t){return mode!=48;}bool begin(IOService *,IOPCIDevice *,uint64_t *info){if(mode==49)return false;const uint64_t v[]{1,0x125000,1,0,0};memcpy(info,v,40);return true;}bool allowGTT(bool ok){assert(ok==(mode==47 || mode>=51));return ok;}uintptr_t pagePoolInitializer(){return 0x1234;}void observeCleanup(OSObject *){}bool finish(bool complete){assert(complete);return mode!=50&&mode!=53;}}}
namespace RaptorLakeP{namespace PagePool{bool enabled(){return mode>=51;}bool prepare(KernelPatcher &,size_t,uintptr_t,size_t){return mode!=52;}void run(IOService *,IOPCIDevice *,bool);bool passed(){return mode!=58;}}}
namespace RaptorLakeP{namespace PageArray{bool integrated(){return mode>=59;}bool finishIntegrated(bool complete){assert(complete);return mode!=60;}bool enabled(){return mode>=55;}bool prepare(KernelPatcher &,size_t,uintptr_t,size_t){return mode!=56;}void run(IOService *,IOPCIDevice *,bool,uintptr_t);}}
namespace RaptorLakeP{namespace NativeFactory{bool enabled(){return mode>=61;}bool prepare(KernelPatcher &,size_t,uintptr_t,size_t){return mode!=62;}bool consume(OSObject *o){if(mode==64)return false;o->release();return true;}bool finish(bool returned){assert(returned);return mode!=65;}}}
BODY
namespace RaptorLakeP{namespace GTTShadow{void finish(){assert(NativeMemory::record[NativeMemory::FactoryReturned] && NativeMemory::record[NativeMemory::GTTBlocked]);}}}
namespace RaptorLakeP{namespace NativePools{void run(IOService *object,IOPCIDevice *pci,bool prerequisite){assert(object&&pci&&NativeMemory::record[NativeMemory::FactoryReturned]&&NativeMemory::record[NativeMemory::Released]&&NativeMemory::record[NativeMemory::DummyReleased]);assert(prerequisite==(mode==44||mode==47||mode==51||mode==54||(mode>=55&&mode!=60&&mode!=64&&mode!=65)));}}}
namespace RaptorLakeP{namespace PagePool{void run(IOService *object,IOPCIDevice *pci,bool prerequisite){assert(object&&pci&&NativeMemory::record[NativeMemory::FactoryReturned]);assert(prerequisite==(mode==51||(mode>=55&&mode!=60&&mode!=64&&mode!=65)));}}}
namespace RaptorLakeP{namespace PageArray{void run(IOService *o,IOPCIDevice *p,bool prerequisite,uintptr_t init){assert(o&&p&&init==0x1234);assert(prerequisite==(mode!=58&&mode!=60&&mode!=64&&mode!=65));}}}
using namespace RaptorLakeP::NativeMemory;
template<class T>void put(void *o,unsigned off,T value){memcpy(static_cast<uint8_t *>(o)+off,&value,sizeof(value));}
bool allocateNative(void **out,uint64_t *address,uint32_t,uint32_t kind){
 auto b=kind?&tablePage:&dataPage;b->refs=1;b->physical=kind?0x124000:0x123000;if(mode==37&&!kind)b->physical=0x8000000000ULL;if(mode==38&&kind)b->physical=0x123000;
 *out=b;*address=b->physical;
 if(mode==27 && !kind){*out=nullptr;*address=0;return false;}
 if((mode==28&&!kind)||(mode==29&&kind)){b->release();return false;}
 if(mode==30&&kind){b->release();*out=nullptr;*address=0;return false;}
 if(kind){auto v=reinterpret_cast<uint64_t *>(b->content);for(unsigned i=0;i<512;++i)v[i]=b->physical|1;}
 else{auto v=reinterpret_cast<uint32_t *>(b->content);for(unsigned i=0;i<1024;++i)v[i]=0xc01dbead;}
 if(mode==31&&kind)b->content[0]^=1;if(mode==32&&!kind)b->content[0]^=1;if(mode==33&&!kind)*address+=4096;
 return true;
}
void cleanupNative(OSObject *m){for(auto off:{0xe0,0xe8}){auto b=field<OSObject *>(m,off);if(b)b->release();put(m,off,uintptr_t(0));}put(m,0xf0,uint64_t(0));put(m,0xf8,uint64_t(0));}
bool dummyNative(OSObject *m){
 auto b=reinterpret_cast<uint8_t *>(m);unsigned options=0;
 for(unsigned kind:{0U,4096U})if(!allocation(reinterpret_cast<void **>(b+(kind?0xe8:0xe0)),reinterpret_cast<uint64_t *>(b+(kind?0xf8:0xf0)),options,kind)){dummyRelease(m);return false;}
 return true;
}
OSObject *mockFactory(IOService *object){
 ++factoryCalls;
 if(mode==12)return nullptr;
 if(mode==47||mode==50||mode>=51)assert(field<uint64_t>(object,0x13d0)==1&&field<uint64_t>(object,0x13d8)==0x125000);
 assert(field<void *>(object,0x368)==gpu);
 assert(field<uint32_t>(object,0x13c0)==0 && field<uint32_t>(object,0x13c4)==0x18000000);
 OSObject manager;IOMemoryMap map;
 put(&manager,0x10,object);put(&manager,0x18,uintptr_t(0x12340000));put(&manager,0x68,&map);put(&manager,0x70,uintptr_t(1));
 const uint64_t ranges[]{0,0x4000000,0x4000000,0xc000000,0x40000000,0xbe000000,0x40000000,0xffffffffc0000000};
 memcpy(reinterpret_cast<uint8_t *>(&manager)+0xa0,ranges,sizeof(ranges));
 if(mode==13)put(&manager,0x10,uintptr_t(0));
 if(mode==18)put(&manager,0xa8,uint64_t(0));
 if(mode==19)put(&manager,0xb8,uint64_t(0));
 if(mode==20)manager.meta.size=0x100;
 if(dummy(&manager)){assert(pagesFlag);assert(gtt(&manager)==(mode==47||mode>=51));}
 if(mode==21)assert(!gtt(&manager));
 release(&manager);if(pagesFlag){dummyRelease(&manager);dummyRelease(&manager);}release(&manager);
 if(mode==22||mode==63||mode==64){static OSObject unexpected;return &unexpected;}
 return nullptr;
}
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);pagesFlag=mode>=23;if(mode==26)IOMapper::gSystem=reinterpret_cast<void *>(1);
 if(mode==1)memoryFlag=false;if(mode==2)probeFlag=false;if(mode==3)nativeFlag=false;if(mode==4)bcsFlag=true;if(mode==5)rcsFlag=true;
 std::vector<uint8_t> image(0xd2000);uintptr_t base=reinterpret_cast<uintptr_t>(image.data());
 const uint8_t head[]{0x55,0x48,0x89,0xe5};for(auto off:{0x2d24a,0x28a00,0xec76,0xecda,0xf05c})memcpy(image.data()+off,head,4);
 const uint8_t allocEntry[]{0x55,0x48,0x89,0xe5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x50},cleanupEntry[]{0x55,0x48,0x89,0xe5,0x41,0x56,0x53};memcpy(image.data()+0xf22e,allocEntry,sizeof(allocEntry));memcpy(image.data()+0xef1e,cleanupEntry,sizeof(cleanupEntry));if(mode==25)image[0xf22e]^=1;
 put(image.data(),0xd1b60,base+0x28a00);put(image.data(),0xd1b68,base+0x2d24a);
 if(mode==9)image[0xec76]^=1;if(mode==10)image[0xd1b60]^=1;
 KernelPatcher p;const bool prepared=prepare(p,7,base,image.size());
 if((mode>=8&&mode<=11)||mode==24||mode==25||mode==42||mode==45||mode==48||mode==52||mode==56||mode==62){assert(!prepared && !record[PatchReady]);return 0;}
 assert(prepared);factory=reinterpret_cast<uintptr_t>(mockFactory);
 IOService object;IOPCIDevice pci;
 if(mode==6)pci.profile=XELP::identify(0xA7A08086,0);if(mode==7)object.meta.size=0x1400;
 std::vector<uint8_t> before(reinterpret_cast<uint8_t *>(&object),reinterpret_cast<uint8_t *>(&object)+sizeof(object));
 run(&object,&pci);
 assert(!memcmp(before.data(),static_cast<void *>(&object),before.size()));assert(mapsCreated==mapsReleased);
 if(mode>=1&&mode<=7){assert(!record[Attempted] && !factoryCalls);return 0;}
 if(mode==26){assert(record[Attempted] && record[Failure]==8 && !factoryCalls);return 0;}
 if(mode==49){assert(record[Attempted]&&!record[FactoryReturned]&&!factoryCalls&&record[Failure]==13);return 0;}
 assert(record[Attempted] && record[FactoryReturned] && factoryCalls==1 && !originalCalls);
 if(mode>=32&&mode<=40){const uint64_t masks[]{191,239,251,159,151,253,253,159,159};assert(record[ValidationMask]==masks[mode-32]);}
 if(mode==31){assert(record[ValidationMask]==127 && record[TableMismatch]==0 && record[TableMismatchValue]!=record[TablePhysical]+1);}
 if(mode==23||mode==41||mode==44||mode==46||mode==47||mode==51||mode==54||(mode>=55&&mode!=60&&mode!=64&&mode!=65)){assert(record[ValidationMask]==255 && record[DataMismatch]==UINT64_MAX && record[TableMismatch]==UINT64_MAX);assert(record[DummyResult] && record[GTTBlocked]==1 && record[AllocationCalls]==2 && record[DummyReleased] && !record[Failure]);}
 else if(mode>=27&&mode<=30){assert(!record[DummyResult] && !record[GTTBlocked] && record[AllocationFailures]==1 && record[FailureOutputsCleared]==1 && record[DummyReleased] && !record[Failure]);}
 else if(mode==0)assert(record[MapValid] && record[DummyBlocked]==1 && record[Released] && record[ReleaseCount]==2 && !record[Failure]);
 else assert(record[Failure]);
 if(mode==12)assert(!releaseCalls && !record[DummyBlocked]);else assert(releaseCalls==2);
 run(&object,&pci);assert(factoryCalls==1);
}
'''.replace('BODY', body)
with tempfile.TemporaryDirectory(prefix='rplp-native-memory-') as tmp:
    p = Path(tmp)
    (p/'test.cpp').write_text(fixture)
    subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',
                    '-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
    for case in range(66):
        subprocess.run([str(p/'test'),str(case)],check=True)
print('PASS production native memory boundary: gates, map/segment capture, pre-GGTT stop, cleanup and restored accelerator fields')
