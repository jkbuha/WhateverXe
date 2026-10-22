"""Exercise production page-pool patch guards, ownership and failure cleanup."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_page_pool.cpp').read_text()
body=s[s.index('namespace RaptorLakeP {namespace PagePool {'):]
fixture=r'''
#include "kern_raptorlake_p_page_pool_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include <cassert>
#include <cstdlib>
#include <map>
#include <vector>
using mach_vm_address_t=uintptr_t;
unsigned mode,allocs,frees,releases,initCalls,classCalls;
std::map<void *,size_t> heap;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
struct IOMapper{inline static void *gSystem=nullptr;};
bool checkKernelArgument(const char *s){
 const char *absent[]{"-wex-rplp-page-pool","-wex-rplp-native-chain","-wex-rplp-native-pools","-wex-rplp-native-pages","-wex-rplp-gtt-aperture","-wex-rplp-gtt-slot","-wex-rplp-probe","-wex-rplp-native-probe"};
 const unsigned modes[]{6,10,11,12,13,14,17,18};
 for(unsigned i=0;i<8;++i)if(!strcmp(s,absent[i]))return mode!=modes[i];
 if(!strcmp(s,"-wex-rplp-bcs"))return mode==15;if(!strcmp(s,"-wex-rplp-rcs"))return mode==16;assert(false);return false;
}
void *IOMalloc(size_t n){++allocs;if(mode>=22&&mode<=25&&allocs==mode-21)return nullptr;auto p=malloc(n);assert(p);memset(p,0xa5,n);heap[p]=n;return p;}
void IOFree(void *p,size_t n){assert(heap.count(p)&&heap[p]==n);heap.erase(p);free(p);++frees;}
struct OSObject;
struct OSMetaClass{unsigned size=0x78;unsigned getClassSize(){return size;}static OSObject *allocClassWithName(const char *);};
struct OSObject{virtual ~OSObject()=default;uint8_t bytes[0x78]{};OSMetaClass meta;OSMetaClass *getMetaClass(){return &meta;}void release();};
struct IOService:OSObject{};
struct IOPCIDevice:IOService{XELP::Profile profile=XELP::identify(0xa7a08086,4);std::vector<uint64_t> receipt;void setProperty(const char *s,const void *p,unsigned n){assert(!strcmp(s,"WhateverXeRPLPPagePool"));receipt.assign((const uint64_t *)p,(const uint64_t *)p+n/8);}};
namespace XELP{Profile deviceProfile(IOPCIDevice *p){return p->profile;}}
struct KernelPatcher{
 enum class Error{NoError,Failure};Error error=Error::NoError;
 struct KextInfo{size_t loadIndex;};struct LookupPatch{KextInfo *target;const uint8_t *find,*replace;size_t size,count;};
 template<class T>T solveSymbol(size_t,const char *s,uintptr_t b,size_t){unsigned off=strstr(s,"15initWithOptions")?0xa8ae:strstr(s,"4freeEv")?0xaa22:strstr(s,"4growEm")?0xbb94:strstr(s,"EC1Em")?0xbc0c:0xcbfc0;return T(b+off+(mode==1));}
 void applyLookupPatch(LookupPatch *p,uint8_t *where,size_t n){assert(p->target->loadIndex==7&&n==21&&p->size==20&&p->count==1&&!memcmp(where,p->find,20));if(mode==4)error=Error::Failure;if(mode!=5)memcpy(where,p->replace,20);}
 Error getError(){return error;}void clearError(){error=Error::NoError;}
};
BODY
using namespace RaptorLakeP::PagePool;
OSObject page;
OSObject *OSMetaClass::allocClassWithName(const char *s){assert(!strcmp(s,"IGPagePool"));++classCalls;if(mode==19)return nullptr;memset(reinterpret_cast<uint8_t *>(&page)+0x10,0,0x68);if(mode==20)page.meta.size=0x70;return &page;}
void OSObject::release(){++releases;auto q=read<void *>(this,0x28);if(q){auto h=read<void *>(q,0x20);IOFree(read<void *>(h,16),384);IOFree(h,24);IOFree(read<void *>(q,16),32);IOFree(q,72);}assert(heap.empty());}
bool mockGrow(void *q,uint64_t n){assert(n==4);auto p=IOMalloc(32);if(!p)return false;write(q,8,n);write(q,16,p);return true;}
void mockHash(void *h,uint64_t n){assert(n==16);auto p=IOMalloc(384);if(p){write(h,0,n);write(h,8,n);write(h,16,p);}}
bool mockInit(OSObject *p,IOService *o,uint32_t options){++initCalls;assert(p==&page&&!options);write(p,0x18,mode==29?nullptr:o);write(p,0x38,reinterpret_cast<uintptr_t>(p)+0x30);if(mode==21)return false;if(mode==30)write(p,0x60,uint32_t(1));bool result=safeTail(p);if(mode==26&&result)write(p,0x30,uint64_t(1));return result;}
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);
 std::vector<uint8_t> image(0xcc000);for(unsigned off:{0xa8aeU,0xaa22U,0xbb94U,0xbc0cU})memcpy(image.data()+off,"\x55\x48\x89\xe5",4);
 const uint8_t tail[]{0xbf,0x48,0,0,0,0xe8,0,0,0,0,0x49,0x89,0xc6,0x45,0x31,0xff,0x4c,0x89,0x38,0x4c};memcpy(image.data()+0xa92e,tail,20);
 const uint8_t end[]{0x48,0x83,0xc4,8,0x5b,0x41,0x5e,0x41,0x5f,0x5d,0xc3};memcpy(image.data()+0xaa17,end,11);
 if(mode==2)image[0xa92e]^=1;if(mode==3)image[0xaa17]^=1;
 KernelPatcher patcher;bool prepared=prepare(patcher,7,reinterpret_cast<uintptr_t>(image.data()),mode==31?0xa942:image.size());
 if((mode>=1&&mode<=5)||mode==31){assert(!prepared&&!record[PatchReady]);return 0;}
 assert(prepared);initializer=reinterpret_cast<uintptr_t>(mockInit);methods={allocate,deallocate,mockGrow,mockHash,0x123456};
 IOPCIDevice pci;IOService owner;if(mode==8)IOMapper::gSystem=(void *)1;if(mode==9)pci.profile=XELP::identify(0xa7a08086,5);
 run(mode==27?nullptr:&owner,mode==28?nullptr:&pci,mode!=7);
 if(mode==6||mode==27||mode==28){assert(!record[Requested]&&!classCalls);return 0;}
 assert(record[Requested]&&pci.receipt.size()==Fields);
 if(mode>=7&&mode<=18){assert(record[Failure]==1&&!record[Attempted]&&!classCalls);return 0;}
 assert(record[Attempted]&&classCalls==1);
 if(mode==19){assert(record[Failure]==2&&!releases);return 0;}
 if(mode==20){assert(record[Failure]==3&&releases==1&&!initCalls);return 0;}
 assert(initCalls==1&&record[InitReturned]&&record[ObjectReleased]&&releases==1&&heap.empty());
 if(mode==0){assert(record[ShapeValid]&&record[SlotsChecked]==16&&record[TailCalls]==1&&!record[Failure]);assert(allocs==4&&frees==4);}
 else assert(record[Failure]==4&&!record[ShapeValid]);
 unsigned previous=allocs;run(&owner,&pci,true);assert(classCalls==1&&allocs==previous);
 if(mode==0){
  assert(passed());uint64_t snapshot[Fields];memcpy(snapshot,record,sizeof(record));
  for(unsigned failure=0;failure<=4;++failure){mode=failure?failure+21:0;allocs=frees=0;auto created=create(&owner);assert(bool(created)==!failure);if(created)created->release();assert(heap.empty());assert(!memcmp(snapshot,record,sizeof(record)));}
  mode=0;previous=allocs;
 }
 assert(!safeTail(&page)&&allocs==previous); // patch is inert outside the scoped call
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-page-pool-adapter-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in range(32):subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS production page-pool adapter: 32 guard, patch, allocation, metadata, cleanup and scoped-callback cases')
