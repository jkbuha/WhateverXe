"""Exercise grouped production engine boundary with RAM and command emulation."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_native_engines.cpp').read_text();body=s[s.index('namespace RaptorLakeP {namespace NativeEngines {'):]
fixture=r'''
#include "kern_raptorlake_p_ring_image_core.hpp"
#include "kern_raptorlake_p_bcs_core.hpp"
#include "kern_raptorlake_p_rcs_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#define captureDisplay originalCaptureDisplay
#define captureGOP originalCaptureGOP
#include "kern_raptorlake_p_gop.hpp"
#undef captureDisplay
#undef captureGOP
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
using mach_vm_address_t=uintptr_t;using IOByteCount=uint64_t;
unsigned mode,displayCalls,bufferCount,mallocCalls;uint32_t *mmio;bool executed[2]{};
uint64_t page_size=4096;void *kernel_task=nullptr;constexpr unsigned kIODirectionInOut=1,kIOMemoryMapperNone=2,kIOMapAnywhere=1,kIOMapInhibitCache=2;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
struct IOMapper{inline static void *gSystem=nullptr;};
bool checkKernelArgument(const char *s){if(!strcmp(s,"-wex-rplp-native-engines"))return mode!=1;if(!strcmp(s,"-wex-rplp-manual-arm"))return mode!=3;if(!strcmp(s,"-wex-rplp-bcs"))return false;if(!strcmp(s,"-wex-rplp-rcs"))return mode==6;return true;}
void *IOMalloc(size_t n){++mallocCalls;if((mode==35&&mallocCalls==1)||(mode==36&&mallocCalls==2)||(mode==37&&mallocCalls==3))return nullptr;return malloc(n);}
void IOFree(void *p,size_t){free(p);}
struct IOMemoryMap{uintptr_t address;uint64_t length,physical;uint64_t getLength(){return length;}uintptr_t getVirtualAddress(){return address;}uint64_t getPhysicalAddress(){return physical;}void release(){delete this;}};
struct IOBufferMemoryDescriptor{
 uint8_t *data;uint64_t length,physical;unsigned index;
 inline static IOBufferMemoryDescriptor *last;
 static IOBufferMemoryDescriptor *withOptions(unsigned options,uint64_t n,unsigned alignment){assert(options==3&&alignment==4096);unsigned index=bufferCount++;if((mode==18&&!index)||(mode==19&&index))return nullptr;auto p=new IOBufferMemoryDescriptor;p->index=index;p->length=n;p->physical=0x100000000ULL+uint64_t(index)*0x1000000;assert(!posix_memalign((void **)&p->data,4096,n));memset(p->data,0,n);last=p;return p;}
 uint64_t getLength(){return length;}void *getBytesNoCopy(){return data;}
 uint64_t getPhysicalSegment(uint64_t o,IOByteCount *span,unsigned options){assert(options==2);*span=mode==23?2048:4096;auto value=physical+2*o;if(mode==20)value=0x6c800000+o;if(mode==21&&o==4096)value=physical;if(mode==24)value=0x8000000000ULL+o;if(mode==43&&index)value=0x100000000ULL;return value;}
 void release(){if(last==this)last=nullptr;free(data);delete this;}
};
struct IOMemoryDescriptor{
 uint64_t getLength(){return mode==10?0:0x10000000;}
 IOMemoryMap *createMappingInTask(void *,unsigned,unsigned flags,uint64_t,uint64_t n){assert(flags==3);if(mode==22)return nullptr;auto p=IOBufferMemoryDescriptor::last;return new IOMemoryMap{(uintptr_t)p->data,n,0x4000000000ULL};}
};
struct IOPCIDevice{
 XELP::Profile profile=XELP::identify(0xa7a08086,4);std::vector<uint64_t> result,bcs,rcs;IOMemoryDescriptor aperture;
 uint16_t configRead16(unsigned o){if(o==4)return mode==7?2:7;assert(o==0x50);return 0x2c1;}
 uint32_t configRead32(unsigned o){if(o==0)return mode==5?0:0xa7a08086;if(o==8)return mode==8?5:4;if(o==0x10)return 0x11000004;if(o==0x14)return 0x40;if(o==0xc0)return 0x6c800000;if(o==0xc4)return 0;assert(false);return 0;}
 IOMemoryDescriptor *getDeviceMemoryWithRegister(unsigned o){assert(o==0x18);return mode==9?nullptr:&aperture;}
 IOMemoryMap *mapDeviceMemoryWithRegister(unsigned o,unsigned flags){assert(o==0x10&&flags==2);if(mode==11)return nullptr;return new IOMemoryMap{(uintptr_t)mmio,mode==25?4096ULL:0x1000000ULL,mode==26?0:0x4011000000ULL};}
 void setProperty(const char *s,const void *p,unsigned n){if(!strcmp(s,"WhateverXeRPLPNativeEnginePreflight")){assert(n==336);return;}auto &target=!strcmp(s,"WhateverXeRPLPNativeEngines")?result:!strcmp(s,"WhateverXeRPLPBCS")?bcs:rcs;target.assign((const uint64_t *)p,(const uint64_t *)p+n/8);}
};
namespace XELP{Profile deviceProfile(IOPCIDevice *p){return p->profile;}}
namespace RaptorLakeP {
 namespace CapsShadow{bool passed(){return mode!=15;}}
 namespace CacheShadow{bool passed(){return mode!=16;}}
 template<class IO>DisplayCapture captureDisplay(IO &,bool on){assert(on);++displayCalls;DisplayCapture d;d.status=DisplayStatus::Complete;if(mode==33&&executed[1])d.powerBefore=1;return d;}
 template<class IO>GOPCapture captureGOP(IO &,DisplayCapture,XELPMemory::Layout,bool on){assert(on);GOPCapture g;g.status=mode==14?GOPStatus::Incomplete:GOPStatus::Complete;g.width=mode==17?1280:1920;g.height=1200;g.pitch=7680;g.candidate=0xa00000;g.reserved=0x70600087;g.gopEnd=0x8ca000;g.verifiedPages=16384;return g;}
}
void IODelay(unsigned n){
 assert(n==100);
 if(mode!=27){mmio[0x130044/4]=mmio[0xa188/4]&1;mmio[0xd84/4]=mmio[0xa278/4]&1;}
 auto p=IOBufferMemoryDescriptor::last;if(!p)return;
 for(unsigned index=0;index<2;++index){unsigned load=index?0x2550:0x22550;if(!mmio[load/4]||executed[index]||mode==32)continue;
  if(p->index!=index)continue;executed[index]=true;unsigned result=index?RaptorLakeP::RCS::GPUResultOffset:RaptorLakeP::BCS::GPUResultOffset;
  *reinterpret_cast<uint32_t *>(p->data+result)=index?RaptorLakeP::RCS::GPUValue:RaptorLakeP::BCS::GPUValue;
  *reinterpret_cast<uint32_t *>(p->data+result+8)=0xc001cafe;
  unsigned base=index?0x2000:0x22000;mmio[(base+0x68)/4]=index?0x7a000004:0x13004002;mmio[(base+0x78)/4]=index?0xa28030:0xa03020;if(mode==44)mmio[(base+0xb0)/4]=1;if(mode==45)mmio[(base+0x64)/4]=1;mmio[(base+0x30)/4]=mmio[(base+0x34)/4]=index?48:32;mmio[(base+0x9c)/4]=0x200;
 }
}
struct KernelPatcher{
 template<class T>T solveSymbol(size_t,const char *s,uintptr_t b,size_t){unsigned o=strstr(s,"17initRingRegisters")?0x7c46c:strstr(s,"14updateRingTail")?0x7c5b6:strstr(s,"17initModeRegisters")?0x28e1a:strstr(s,"31initHardwareStatus")?0x28f34:strstr(s,"20getGPUVirtual")?0x13e7c:strstr(s,"17getVirtualAddress")?0x10b60:strstr(s,"25initRingGPU")?0x7c4ac:strstr(s,"15initRingControl")?0x7c4fe:0x7c574;return T(b+o+(mode==30));}void clearError(){}
};
BODY
using namespace RaptorLakeP;using namespace NativeEngines;
void nativeInit(void *context){auto &f=*RingImage::active;assert(context==f.data.context);auto image=reinterpret_cast<uint32_t *>(RingImage::read<uintptr_t>(f.data.shared,0x38)+4096);image[5]=0;image[7]=0;image[9]=uint32_t(RingImage::query(f.data.maps[0]));image[11]=mode==38?0:1;}
void nativeTail(void *,uint32_t tail){auto &f=*RingImage::active;auto image=reinterpret_cast<uint32_t *>(RingImage::read<uintptr_t>(f.data.shared,0x38)+4096);image[7]=tail;}
void nativeMode(void *owner){auto mask=RingImage::read<uint64_t>(owner,0x1300);unsigned reg=mask==1?0x229c:0x2229c;mmio[reg/4]=mode==39?8:0;mmio[0x14800/4]=0;if(mode==42)RingImage::write(owner,0x1124,uint32_t(1));}
void nativeHWS(void *owner){auto &f=*RingImage::active;auto mask=RingImage::read<uint64_t>(owner,0x1300);unsigned reg=mask==1?0x2080:0x22080;mmio[reg/4]=uint32_t(RingImage::query(mode==41?nullptr:f.data.maps[0]));mmio[0x18080/4]=uint32_t(RingImage::query(f.data.maps[1]));if(mode==40)mmio[reg/4]=0;}
int main(int argc,char **argv){
 assert(argc==2);mode=atoi(argv[1]);std::vector<uint8_t> image(0xd2000);auto b=(uintptr_t)image.data();uint8_t shift=mode==31?13:12;
 for(unsigned o:{0x7c46cU,0x7c5b6U,0x28e1aU,0x28f34U,0x13e7cU,0x10b60U,0x7c4acU,0x7c4feU,0x7c574U})memcpy(image.data()+o,"\x55\x48\x89\xe5",4);
 RingImage::write(image.data(),0xc8230,reinterpret_cast<uintptr_t>(&shift));KernelPatcher patcher;bool ready=prepare(patcher,7,b,image.size());
 if(mode==3||mode==30||mode==31){assert(!ready);return 0;}assert(ready);methods={nativeInit,nativeTail,nativeMode,nativeHWS};
 assert(!posix_memalign((void **)&mmio,4096,0x1000000));memset(mmio,0,0x1000000);
 mmio[0x9138/4]=1;mmio[0x913c/4]=mode==12?0:0x3f;mmio[0x9140/4]=0xe00fa;mmio[0x209c/4]=mmio[0x2209c/4]=0x200;mmio[0x2234/4]=mmio[0x22234/4]=1;
 auto ptes=reinterpret_cast<uint64_t *>(reinterpret_cast<uint8_t *>(mmio)+0x800000);for(unsigned i=0;i<16384;++i)ptes[i]=(0x6c800000ULL+i*4096)|1;
 if(mode==28)mmio[0xc000/4]=0x100;if(mode==29)mmio[0x2030/4]=8;if(mode==34)mmio[0x18080/4]=0x400000;
 IOPCIDevice pci;if(mode==4)IOMapper::gSystem=(void *)1;if(mode==13)page_size=16384;
 run(&pci,mode!=2);
 if(mode==1){assert(!record[Requested]&&pci.result.empty());free(mmio);return 0;}
 assert(pci.result.size()==Fields);
 if(bool(record[Failure])!=(mode!=0)){fprintf(stderr,"mode=%u fields:",mode);for(auto v:record)fprintf(stderr," %llu",(unsigned long long)v);fprintf(stderr,"\n");}
 assert(bool(record[Failure])==(mode!=0));
 if(mode==0){assert(record[BCSPowerHWS]==0xa0f000&&record[RCSPowerHWS]==0xa2f000&&record[Stage]==6&&record[EngineMask]==5&&record[BCSSlot]==0xa00000&&record[RCSSlot]==0xa10000&&record[BCSImage]&&record[RCSImage]&&record[BCSSetup]&&record[RCSSetup]&&record[BCSPassed]&&record[RCSPassed]&&record[RetainedMask]==3&&record[RenderLeases]==1&&record[RenderReleased]==1&&record[BCSHWSQueries]==2&&record[RCSHWSQueries]==2&&pci.bcs.size()==45&&pci.rcs.size()==49);}
 if(mode==19||mode==29||mode==43)assert(record[BCSPassed]&&!record[RCSPassed]&&record[Failure]==6);
 if(mode>=35&&mode<=36)assert(!record[RetainedMask]&&!executed[0]);
 if(mode>=37&&mode<=42&&mode!=38)assert(record[RetainedMask]==1&&!executed[0]);
 if(mode==38)assert(!record[RetainedMask]&&!executed[0]);
 if(mode==33)assert(record[BCSPassed]&&record[RCSPassed]&&record[Failure]==7);
 auto requested=record[Requested];run(&pci,true);assert(record[Requested]==requested);
 // End-of-process mock cleanup only: hardware production keeps these pinned.
 for(auto &p:pinned)if(p){p->release();p=nullptr;}free(mmio);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-native-engines-') as td:
 p=Path(td);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in range(46):subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS production grouped native engines: 46 prerequisite/donor/layout/allocation/native-image/mode/HWS/timeout/ownership/retention cases, independent GGTT ranges and both command completions')
