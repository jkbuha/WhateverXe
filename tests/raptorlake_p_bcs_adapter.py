"""Replay the actual allocation adapter; command execution is tested separately."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_bcs.cpp').read_text();body=s[s.index('namespace RaptorLakeP {'):]
body=body.replace('result=BCS::testGPU(', 'result=simulateCommand(')
fixture=r'''
#include "kern_raptorlake_p_bcs_core.hpp"
#include "kern_raptorlake_p_gop.hpp"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <map>
#include <string>
using IOByteCount=uint64_t;
constexpr unsigned kIOMemoryMapperNone=1,kIODirectionInOut=2,kIOMapAnywhere=4,kIOMapInhibitCache=8;
int kernel_task=1;unsigned page_size=4096,mode=0,allocations=0,freed=0,mapReleases=0,commands=0;
struct IOMapper {static void *gSystem;};void *IOMapper::gSystem=nullptr;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
bool checkKernelArgument(const char *arg){assert(!strcmp(arg,"-wex-rplp-probe") || !strcmp(arg,"-wex-rplp-bcs"));return !(mode==1 || (mode==2 && !strcmp(arg,"-wex-rplp-bcs")));}
void IODelay(unsigned){assert(false);}
#define SYSLOG(...)
#define OSSafeReleaseNULL(x) do{if(x){(x)->release();(x)=nullptr;}}while(0)
struct IOMemoryMap {
 std::vector<uint64_t> mem=std::vector<uint64_t>(0x1000000/8);uint64_t length=0x1000000;bool absent=false;
 uint64_t getLength(){return length;}uintptr_t getVirtualAddress(){return absent?0:reinterpret_cast<uintptr_t>(mem.data());}
 void release(){++mapReleases;}
 void set(uint32_t reg,uint32_t value){reinterpret_cast<uint32_t *>(mem.data())[reg/4]=value;}
};
struct Descriptor {
 uint64_t length=0x10000000;IOMemoryMap map;
 uint64_t getLength(){return length;}
 IOMemoryMap *createMappingInTask(int task,unsigned addr,unsigned options,uint64_t start,uint64_t bytes){
  assert(task==kernel_task && !addr && options==(kIOMapAnywhere|kIOMapInhibitCache) && start==0xA00000 && bytes==65536);
  map.length=mode==28?4096:bytes;map.absent=mode==27;return mode==26?nullptr:&map;
 }
};
struct IOBufferMemoryDescriptor {
 std::vector<uint64_t> mem=std::vector<uint64_t>(65536/8);
 static IOBufferMemoryDescriptor *withOptions(unsigned options,unsigned bytes,unsigned align){assert(options==(kIODirectionInOut|kIOMemoryMapperNone) && bytes==65536 && align==4096);++allocations;return mode==18?nullptr:new IOBufferMemoryDescriptor;}
 void *getBytesNoCopy(){return mode==19?nullptr:mem.data();}
 uint64_t getLength(){return mode==20?4096:65536;}
 uint64_t getPhysicalSegment(uint64_t offset,IOByteCount *span,unsigned options){assert(options==kIOMemoryMapperNone);*span=mode==22?4095:4096;return mode==21?1:mode==23?0x100000000ULL:mode==24?0x6C800000ULL:mode==25?0x8000000000ULL:0x100000000ULL+2*offset;}
 void release(){++freed;delete this;}
};
struct IOPCIDevice {
 Descriptor aperture;std::map<uint32_t,uint32_t> config{{0,0xA7A08086},{8,4},{4,7},{0x50,0x2C1},{0xC0,0x6C800000},{0xC4,0}};
 std::map<std::string,std::vector<uint8_t>> props;
 uint32_t configRead32(unsigned reg){return config[reg];}uint16_t configRead16(unsigned reg){return configRead32(reg);}uint8_t configRead8(unsigned reg){return configRead32(reg);}
 Descriptor *getDeviceMemoryWithRegister(unsigned reg){assert(reg==0x18);return mode==13?nullptr:&aperture;}
 void setProperty(const char *key,const void *data,unsigned length){auto bytes=static_cast<const uint8_t *>(data);props[key]={bytes,bytes+length};}
};
namespace XELP {Profile deviceProfile(IOPCIDevice *pci){return identify(pci->configRead32(0),pci->configRead8(8));}}
namespace RaptorLakeP {
// The core fixture interprets the real command packets. Here the command seam
// isolates production allocation, mapping, guard and lifetime behavior.
template<class IO> BCS::GPUResult simulateCommand(IO &io,uint8_t *memory,const uint64_t *dma,const uint64_t *previous,uint32_t slot,bool allowed){
 assert(allowed && memory && slot==0xA00000 && io.displayUnchanged());++commands;
 for(unsigned p=0;p<16;++p){assert(dma[p]==0x100000000ULL+8192*p);assert(previous[p]==((0x6C800000ULL+slot+4096*p)|1));}
 BCS::GPUResult result;result.retained=mode!=30;result.status=uint64_t(mode==30?BCS::GPUStatus::EngineBusy:BCS::GPUStatus::Complete);return result;
}}
BODY
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);IOPCIDevice pci;IOMemoryMap map;
 struct Reader {IOMemoryMap &map;bool readable(uint32_t){return true;}uint32_t read(uint32_t r){return reinterpret_cast<uint32_t *>(map.mem.data())[r/4];}} reader{map};
 map.set(0x45404,1U<<10);map.set(0x60400,0x8A010102);map.set(0x70008,0xC0000000);
 auto display=RaptorLakeP::captureDisplay(reader,true);
 auto layout=XELPMemory::decode(0x2C1,0x6C800000,0);
 RaptorLakeP::GOPCapture gop;gop.status=RaptorLakeP::GOPStatus::Complete;gop.candidate=0xA00000;gop.gopEnd=9216000;gop.verifiedPages=16384;gop.reserved=0x70600087;
 for(unsigned p=0;p<16;++p)map.mem[(0x800000+(0xA00000>>9))/8+p]=(0x6C800000ULL+0xA00000+4096*p)|1;
 switch(mode){case 3:pci.config[0]=0x9A498086;break;case 4:pci.config[8]=0;break;case 5:map.length=4096;break;
 case 6:map.absent=true;break;case 7:gop.status=RaptorLakeP::GOPStatus::MappingMismatch;break;
 case 8:gop.candidate=0xA00001;break;case 9:gop.verifiedPages=1;break;case 10:page_size=8192;break;
 case 11:pci.config[4]=2;break;case 12:gop.reserved=0;break;case 14:pci.aperture.length=4096;break;
 case 15:map.set(0x60400,0);break;case 16:pci.config[0x50]=0x3C1;break;case 17:map.mem[(0x800000+(0xA00000>>9))/8]=0;break;case 29:gop.candidate=0;break;case 31:IOMapper::gSystem=reinterpret_cast<void *>(1);break;}
 RaptorLakeP::probeBCS(&pci,&map,display,gop,layout);
 if(mode>=1 && mode<=4){assert(pci.props.empty() && !allocations && !commands);return 0;}
 auto data=pci.props.at("WhateverXeRPLPBCS");RaptorLakeP::BCS::GPUResult result;assert(data.size()==sizeof(result));memcpy(&result,data.data(),data.size());
 if(mode==0 || mode==30){assert(commands==1 && allocations==1 && mapReleases==1);assert(freed==(mode==30?1U:0U));assert(result.retained==(mode==0?1U:0U));}
 else{assert(!commands && !result.retained);if(mode>=19 && mode<=28)assert(allocations==1 && freed==1);else assert(!freed);}
 unsigned before=allocations;RaptorLakeP::probeBCS(&pci,&map,display,gop,layout);assert(allocations==before && commands<=1);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-bcs-owner-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for n in range(32):subprocess.run([str(p/'test'),str(n)],check=True)
print('PASS production RPL-P BCS owner: exact gates, allocation failures, physical pages, mapping and pinned lifetime')
