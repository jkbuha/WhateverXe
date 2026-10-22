"""Exercise production page-array scope, prerequisite and lifetime guards."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_page_array.cpp').read_text();body=s[s.index('namespace RaptorLakeP {namespace PageArray {'):]
fixture=r'''
#include "kern_raptorlake_p_page_array_core.hpp"
#include "kern_raptorlake_p_page_pool_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include <cassert>
#include <cstdlib>
#include <map>
#include <set>
#include <vector>
using mach_vm_address_t=uintptr_t;
bool integratedMode;unsigned mode,allocs,createIndex,objectReleases,nativeCalls,nativeReleaseCalls;uint32_t ncpus=16;
std::map<void *,size_t> heap;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
struct IOMapper{inline static void *gSystem=nullptr;};
bool checkKernelArgument(const char *s){
 if(!strcmp(s,"-wex-rplp-manager-pools"))return integratedMode;
 if(!strcmp(s,"-wex-rplp-page-array"))return mode!=9;if(!strcmp(s,"-wex-rplp-page-pool"))return mode!=13;
 if(!strcmp(s,"-wex-rplp-native-chain"))return mode!=14;if(!strcmp(s,"-wex-rplp-probe"))return mode!=32;
 if(!strcmp(s,"-wex-rplp-native-probe"))return mode!=33;
 if(!strcmp(s,"-wex-rplp-bcs"))return mode==16;if(!strcmp(s,"-wex-rplp-rcs"))return mode==17;assert(false);return false;
}
void *IOMalloc(size_t n){++allocs;if((mode==21&&!integratedMode&&allocs==1)||(mode==22&&allocs==(integratedMode?1U:2U)))return nullptr;auto p=malloc(n);assert(p);memset(p,0xa5,n);heap[p]=n;return p;}
void IOFree(void *p,size_t n){assert(heap.count(p)&&heap[p]==n);heap.erase(p);free(p);}
struct Meta{unsigned size=0x118;unsigned getClassSize(){return size;}};
struct OSObject{virtual ~OSObject()=default;uint8_t storage[0x118]{};Meta meta;Meta *getMetaClass(){return &meta;}void release();};
std::set<OSObject *> objects;
void OSObject::release(){assert(objects.erase(this));++objectReleases;delete this;}
struct IOService:OSObject{};
struct IOPCIDevice:IOService{XELP::Profile profile=XELP::identify(0xa7a08086,4);std::vector<uint64_t> receipt;void setProperty(const char *s,const void *p,unsigned n){assert(!strcmp(s,"WhateverXeRPLPPageArray"));receipt.assign((const uint64_t *)p,(const uint64_t *)p+n/8);}};
namespace XELP{Profile deviceProfile(IOPCIDevice *p){return p->profile;}}
namespace RaptorLakeP{namespace PagePool{bool passed(){return mode!=12;}OSObject *create(IOService *){unsigned i=createIndex++;if((mode==23&&i==0)||(mode==24&&i==7)||(mode==25&&i==15))return nullptr;auto p=new OSObject;objects.insert(p);return p;}}}
struct KernelPatcher{
 enum class Error{NoError,Failure};Error error=Error::NoError;
 struct KextInfo{size_t loadIndex;};struct LookupPatch{KextInfo *target;const uint8_t *find,*replace;size_t size,count;};
 struct RouteRequest{const char *s;uintptr_t f;uintptr_t &o;template<class F>RouteRequest(const char *n,F fn,uintptr_t &original):s(n),f(reinterpret_cast<uintptr_t>(fn)),o(original){}};
 template<class T>T solveSymbol(size_t,const char *s,uintptr_t b,size_t){return T(b+(strstr(s,"11withOptions")?0xa860:0xf39e)+(mode==1));}
 void applyLookupPatch(LookupPatch *p,uint8_t *where,size_t n){assert(p->target->loadIndex==7&&n==50&&p->size==49&&p->count==1&&!memcmp(where,p->find,49));if(mode==4)error=Error::Failure;if(mode!=5)memcpy(where,p->replace,49);}
 bool routeMultiple(size_t,RouteRequest *,unsigned n,uintptr_t,size_t){assert(n==2);return mode!=6;}
 Error getError(){return error;}void clearError(){error=Error::NoError;}
};
BODY
using namespace RaptorLakeP::PageArray;
bool mockInit(void *m){
 ++nativeCalls;assert(m==manager&&read<IOService *>(m,0x10)==activeOwner);
 auto a=static_cast<OSObject **>(IOMalloc(ncpus*8));write(m,0x110,a);if(!a)return false;
 memset(a,0,ncpus*8);
 for(unsigned i=0;i<ncpus;++i){
  a[i]=factory(activeOwner,mode==38?1:0);
  if(!a[i]){while(i)a[--i]->release();IOFree(a,ncpus*8);write(m,0x110,uintptr_t(0));return false;}
 }
 if(mode==26)return false;if(mode==27)++ncpus;if(mode==28)a[15]=a[0];if(mode==29)a[15]=nullptr;
 if(mode==36){for(unsigned i=0;i<16;++i)a[i]->release();IOFree(a,128);write(m,0x110,uintptr_t(0));}
 if(mode==37)--record[Created];
 return true;
}
void mockRelease(void *m){assert(!activeOwner);if(m!=manager){assert(!read<void *>(m,0x110));return;}++nativeReleaseCalls;if(mode==30||mode==51)return;auto a=read<OSObject **>(m,0x110);if(!a)return;for(unsigned i=0;i<ncpus;++i){assert(a[i]);a[i]->release();a[i]=nullptr;}IOFree(a,ncpus*8);write(m,0x110,uintptr_t(0));}
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);integratedMode=mode>=39;if(integratedMode)mode-=39;
 std::vector<uint8_t> image(0xc9000);for(unsigned off:{0xa860U,0xf39eU})memcpy(image.data()+off,"\x55\x48\x89\xe5",4);
 memcpy(image.data()+0xee09,CleanupBefore,49);memcpy(image.data()+0xedac,"\x4c\x8b\x3d\x8d\x94\x0b\x00",7);
 write(image.data(),0xc8240,mode==7?uintptr_t(0):reinterpret_cast<uintptr_t>(&ncpus)+(mode==8));
 if(mode==2)image[0xee09]^=1;if(mode==3)image[0xedac]^=1;
 KernelPatcher patcher;bool prepared=prepare(patcher,7,reinterpret_cast<uintptr_t>(image.data()),mode==31?0xf430:image.size());
 if((mode>=1&&mode<=8)||mode==31){assert(!prepared&&!record[PatchReady]);return 0;}assert(prepared);
 releaseArray=reinterpret_cast<uintptr_t>(mockRelease);
 IOPCIDevice pci;IOService owner;if(mode==11)IOMapper::gSystem=(void *)1;if(mode==15)pci.profile=XELP::identify(0xa7a08086,5);
 if(mode==19)ncpus=0;if(mode==20)ncpus=65;
 assert(!factory(&owner,0));
 if(integratedMode){
  OSObject target;write(&target,0x10,&owner);
  if(mode==40)target.meta.size=0x110;if(mode==41)write(&target,0x10,uintptr_t(0));if(mode==42)write(&target,0x110,uintptr_t(1));
  bool ok=populate(mode==34?nullptr:&owner,mode==35?nullptr:&pci,mode==39?nullptr:&target,mode!=10,mode==18?0:reinterpret_cast<uintptr_t>(mockInit));
  if(mode==34||mode==35){assert(!ok&&!record[Requested]&&!allocs);return 0;}
  assert(record[Integrated]&&record[Requested]&&!record[ManagerAllocated]);
  if((mode>=9&&mode<=18)||mode==32||mode==33){assert(!ok&&record[Failure]==1&&!record[Attempted]&&!allocs);return 0;}
  if(mode>=39&&mode<=42){assert(!ok&&record[Failure]==9&&!record[Attempted]&&!allocs);return 0;}
  if(mode==19||mode==20){assert(!ok&&record[Failure]==2&&!allocs);return 0;}
  assert(record[ManagerAccepted]&&record[InitReturned]&&nativeCalls==1&&!activeOwner);
  if(mode==47)++ncpus;if(mode==48)write(&target,0x110,uintptr_t(1));if(mode==49)write(&target,0x110,uintptr_t(0));
  if(mode==45){OSObject foreign;releaseObserved(&foreign);assert(!record[UnwindSeen]);}
  if(mode==52)assert(releaseOwned(&target));
  else {if(mode==53){OSObject foreign;assert(!releaseOwned(&foreign));}if(mode==54){record[Failure]=17;assert(!releaseOwned(&target));}if(mode!=43)releaseObserved(&target);}if(mode==50||mode==51)releaseObserved(&target);
  if(mode==51)assert(nativeReleaseCalls==1);
  if(mode==46)memset(reinterpret_cast<uint8_t *>(&target)+0x10,0,0x108); // finish must not read the freed manager
  bool finished=finishIntegrated(mode!=44);
  if(mode==0||mode==21||mode==45||mode==46||mode==50||mode==52||mode==53){assert(ok&&finished&&record[Validated]==16&&record[UnwindSeen]==(mode==50?2U:1U)&&record[ReleaseCalled]&&record[ArrayCleared]&&record[ManagerReleased]&&!record[Retained]&&!record[Failure]);assert(objects.empty()&&heap.empty()&&objectReleases==16);}
  else assert(!finished&&record[Failure]);
  if(mode==43||mode==44||mode==47||mode==48||mode==49||mode==51||(mode>=26&&mode<=30)||mode==37)assert(record[Retained]);
  unsigned prior=allocs;assert(!populate(&owner,&pci,&target,true,reinterpret_cast<uintptr_t>(mockInit)));assert(allocs==prior&&!factory(&owner,0));
  while(!objects.empty())(*objects.begin())->release();while(!heap.empty()){auto it=heap.begin();IOFree(it->first,it->second);}return 0;
 }
 run(mode==34?nullptr:&owner,mode==35?nullptr:&pci,mode!=10,mode==18?0:reinterpret_cast<uintptr_t>(mockInit));
 if(mode==9||mode==34||mode==35){assert(!record[Requested]&&!allocs);return 0;}
 assert(record[Requested]&&pci.receipt.size()==Fields);
 if((mode>=10&&mode<=18)||mode==32||mode==33){assert(record[Failure]==1&&!record[Attempted]&&!allocs);return 0;}
 if(mode==19||mode==20){assert(record[Failure]==2&&!allocs);return 0;}
 if(mode==21){assert(record[Failure]==3&&allocs==1&&!nativeCalls);return 0;}
 assert(record[Attempted]&&record[InitReturned]&&nativeCalls==1&&!activeOwner);
 if(mode==0){assert(record[FactoryCalls]==16&&record[Created]==16&&record[Validated]==16&&record[ReleaseCalled]&&record[ArrayCleared]&&record[ManagerReleased]&&!record[Retained]&&!record[Failure]);assert(objectReleases==16&&heap.empty()&&objects.empty());}
 else {assert(record[Failure]);if((mode>=26&&mode<=30)||mode==37)assert(record[Retained]&&manager&&!record[ManagerReleased]);else assert(record[ManagerReleased]&&heap.empty()&&objects.empty());}
 unsigned previous=allocs;run(&owner,&pci,true,reinterpret_cast<uintptr_t>(mockInit));assert(allocs==previous);assert(!factory(&owner,0));
 // Retention is intentional in production. Tear down known mock allocations
 // independently so sanitizer leak checking remains useful for every case.
 while(!objects.empty())(*objects.begin())->release();while(!heap.empty()){auto it=heap.begin();IOFree(it->first,it->second);}
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-page-array-adapter-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in range(94):subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS production page-array adapter: 94 private/integrated array cases: exact patches, guards, allocation, CPU count, ownership, native unwind, captured-before-free metadata and safe retention')
