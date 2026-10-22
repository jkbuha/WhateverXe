"""Replay production native pool owner/cleanup with owned RAM and IOKit mocks."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_native_pools.cpp').read_text()
body=s[s.index('namespace RaptorLakeP {namespace NativePools {'):]
fixture=r'''
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <cassert>
#include <cstring>
#include <vector>
using mach_vm_address_t=uintptr_t;using IOByteCount=uint64_t;
constexpr unsigned kIODirectionInOut=3,kIOMemoryMapperNone=0x800;
#define OSDynamicCast(t,p) dynamic_cast<t *>(p)
int mode;unsigned created=0,poolCalls=0,fenceCalls=0,poolFrees=0,fenceFrees=0,bufferFrees=0;bool alive=false,poolLive=false,fenceLive=false;
struct IOMapper{inline static void *gSystem=nullptr;};
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
bool checkKernelArgument(const char *s){if(!strcmp(s,"-wex-rplp-native-pools"))return mode!=28;if(!strcmp(s,"-wex-rplp-bcs"))return mode==25;if(!strcmp(s,"-wex-rplp-rcs"))return false;if(!strcmp(s,"-wex-rplp-native-pages"))return mode!=26;if(!strcmp(s,"-wex-rplp-gtt-aperture"))return mode!=27;return true;}
struct OSMetaClass{unsigned size=0x2000;unsigned getClassSize(){return size;}};
struct OSObject{virtual ~OSObject()=default;uint8_t bytes[0x2000]{};OSMetaClass meta;OSMetaClass *getMetaClass(){return &meta;}virtual void release(){assert(alive);}};
struct IOService:OSObject{};
struct IOMemoryDescriptor:OSObject{virtual uint64_t getLength(){return mode==9?4095:4096;}virtual uint64_t getPhysicalSegment(unsigned,IOByteCount *span,unsigned opt){assert(opt==kIOMemoryMapperNone);*span=mode==11?0:4096;return mode==10?0x123001:0x123000;}};
struct IOBufferMemoryDescriptor:IOMemoryDescriptor{
 static IOBufferMemoryDescriptor *withOptions(unsigned options,unsigned bytes,unsigned alignment);
 uint64_t getLength(){return mode==5?4095:4096;}
 uint64_t getPhysicalSegment(unsigned,IOByteCount *span,unsigned opt){assert(opt==kIOMemoryMapperNone);*span=mode==20?0:4096;return mode==21?0x8000000000ULL:mode==22?0x6c800000:0x123000;}
 void release(){assert(alive&&!poolLive&&!fenceLive);alive=false;++bufferFrees;}
} page;
IOBufferMemoryDescriptor *IOBufferMemoryDescriptor::withOptions(unsigned options,unsigned bytes,unsigned alignment){assert(options==(kIODirectionInOut|kIOMemoryMapperNone)&&bytes==4096&&alignment==4096);++created;if(mode==4)return nullptr;alive=true;return &page;}
struct Pool:OSObject{void release(){assert(alive&&poolLive);poolLive=false;++poolFrees;}} pool;
struct Fence:OSObject{void release(){assert(alive&&fenceLive);fenceLive=false;++fenceFrees;}} fence;
IOMemoryDescriptor descriptor;OSObject allocator;
struct IOPCIDevice:IOService{
 XELP::Profile profile=XELP::identify(0xa7a08086,4);std::vector<uint8_t> receipt;
 uint16_t configRead16(unsigned off){assert(off==0x50);return 0x2c1;}
 uint32_t configRead32(unsigned off){assert(off==0xc0||off==0xc4);return off==0xc0?0x6c800001:0;}
 void setProperty(const char *name,const void *p,unsigned n){assert(!strcmp(name,"WhateverXeRPLPNativePools"));receipt.assign((const uint8_t *)p,(const uint8_t *)p+n);}
};
namespace XELP{Profile deviceProfile(IOPCIDevice *p){return p->profile;}}
struct KernelPatcher {
 template<class T>T solveSymbol(size_t,const char *name,uintptr_t base,size_t){unsigned off=strstr(name,"10withRanges")?0xc586:strstr(name,"11withOptions")?0x1379e:strstr(name,"18IGStolen")?0xc75e:0x13824;return T(base+off+(mode==1));}
 void clearError(){}
};
BODY
using namespace RaptorLakeP::NativePools;
template<class T>void put(void *p,unsigned off,T v){memcpy((uint8_t *)p+off,&v,sizeof(v));}
OSObject *makePool(const uint64_t *info,IOService *owner){
 assert(alive&&info[0]==1&&info[1]==0x123000&&info[2]==1&&!info[3]&&!info[4]);++poolCalls;
 if(mode==6)return nullptr;poolLive=true;
 if(mode==7)pool.meta.size=0x7f;
 put(&pool,0x10,mode==8?nullptr:owner);put(&pool,0x18,&descriptor);put(&pool,0x20,mode==12?nullptr:&allocator);
 for(unsigned i=0;i<5;++i)put(&pool,0x28+8*i,info[i]);
 if(mode==13)put(&pool,0x38,uint64_t(2));put(&pool,0x60,uint64_t(mode==15?0:4096));put(&pool,0x78,reinterpret_cast<uintptr_t>(&pool)+(mode==14?0:0x70));return &pool;
}
OSObject *makeFence(IOService *owner){assert(alive);++fenceCalls;if(mode==16)return nullptr;fenceLive=true;if(mode==17)fence.meta.size=0x47;put(&fence,0x10,mode==18?nullptr:owner);put(&fence,0x40,uint64_t(mode==29?0x1fffffff8ULL:mode==19?0:0xfffffff8));return &fence;}
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);
 std::vector<uint8_t> image(0x14000);for(unsigned off:{0xc586U,0x1379eU,0xc75eU,0x13824U})memcpy(image.data()+off,"\x55\x48\x89\xe5",4);if(mode==2)image[0xc586]^=1;
 KernelPatcher p;bool prepared=prepare(p,7,reinterpret_cast<uintptr_t>(image.data()),image.size());if(mode==1||mode==2)assert(!prepared);else assert(prepared);
 poolFactory=reinterpret_cast<uintptr_t>(makePool);fenceFactory=reinterpret_cast<uintptr_t>(makeFence);
 IOPCIDevice pci;IOService owner;if(mode==23)IOMapper::gSystem=(void *)1;if(mode==24)pci.profile=XELP::identify(0xa7a08086,5);
 run(&owner,&pci,mode!=3);
 if(mode==28){assert(!record[Requested]&&!created);return 0;}
 assert(record[Requested]);
 if(mode==1||mode==2||mode==3||(mode>=23&&mode<=27)){assert(record[Failure]==1&&!record[Attempted]&&!created);return 0;}
 assert(record[Attempted]&&created==1&&!alive);
 if(mode==0)assert(record[PoolValid]&&record[FenceValid]&&record[PoolReleased]&&record[FenceReleased]&&record[BufferReleased]&&!record[Failure]);
 else assert(record[Failure]);
 assert(poolFrees==record[PoolReturned]&&fenceFrees==record[FenceReturned]&&bufferFrees==(mode==4?0:1));
 run(&owner,&pci,true);assert(created==1);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-native-pools-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in range(30):subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS production native pools: exact method/identity/prerequisite guards, owned RAM, borrowed descriptors, partial constructors, pool/fence metadata and release-before-backing ordering')
