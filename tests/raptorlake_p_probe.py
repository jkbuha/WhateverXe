"""Exercise the production PCI/MMIO adapter with native IOKit mocks."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_probe.cpp').read_text()
body=s[s.index('namespace RaptorLakeP {'):]
body=body.replace('namespace RaptorLakeP {','namespace RaptorLakeP { void probeBCS(IOPCIDevice *,Map *,const DisplayCapture &,const GOPCapture &,const XELPMemory::Layout &){assert(false);} void probeRCS(IOPCIDevice *,Map *,const DisplayCapture &,const GOPCapture &,const XELPMemory::Layout &){assert(false);} namespace Native {void arm(IOPCIDevice *,const XELP::Profile &,const Capture &,const GOPCapture &){}}',1)
fixture=r'''
#include "kern_raptorlake_p.hpp"
#include "kern_raptorlake_p_display.hpp"
#include "kern_raptorlake_p_gop.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>
using IOByteCount=uint64_t;
constexpr unsigned kIOMemoryMapperNone=1,kIOMapInhibitCache=2;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
bool enabled=true;unsigned delays=0;
bool checkKernelArgument(const char *a){assert(!strcmp(a,"-wex-rplp-probe") || !strcmp(a,"-wex-rplp-bcs") || !strcmp(a,"-wex-rplp-rcs"));return !strcmp(a,"-wex-rplp-probe") && enabled;}
void IODelay(unsigned n){assert(n==100 && ++delays<=2000);}
#define SYSLOG(...)
struct Descriptor {
 uint64_t physical=0x4010000000ULL,length=0x1000000,span=0x1000000;
 uint64_t getPhysicalSegment(unsigned off,IOByteCount *n,unsigned options){assert(!off && options==kIOMemoryMapperNone);*n=span;return physical;}
 uint64_t getLength(){return length;}
};
struct Map {
 std::vector<uint32_t> memory=std::vector<uint32_t>(0x1000000/4);
 uint64_t physical=0x4010000000ULL,length=0x1000000;unsigned releases=0;bool nullAddress=false;
 uint64_t getLength(){return length;}uint64_t getPhysicalAddress(){return physical;}
 uintptr_t getVirtualAddress(){return nullAddress?0:reinterpret_cast<uintptr_t>(memory.data());}
 void release(){++releases;}
};
struct IOPCIDevice {
 Descriptor bar,aperture;Map map;unsigned maps=0,reads=0;bool noBar=false,noAperture=false,noMap=false;
 std::map<unsigned,uint32_t> config{{0,0xA7A08086},{8,4},{4,2},{0x50,0x340},{0x10,0x10000004},{0x14,0x40},{0xC0,0x80000000},{0xC4,0}};
 std::map<std::string,std::vector<uint8_t>> properties;
 IOPCIDevice(){aperture.length=0x10000000;map.memory[0xA188/4]=1;}
 uint32_t configRead32(unsigned off){++reads;return config[off];}
 uint16_t configRead16(unsigned off){return configRead32(off);}
 uint8_t configRead8(unsigned off){return configRead32(off);}
 void setProperty(const char *key,const void *data,unsigned size){auto p=static_cast<const uint8_t *>(data);properties[key]={p,p+size};}
 Descriptor *getDeviceMemoryWithRegister(unsigned reg){assert(reg==0x10 || reg==0x18);return reg==0x10?(noBar?nullptr:&bar):(noAperture?nullptr:&aperture);}
 Map *mapDeviceMemoryWithRegister(unsigned reg,unsigned options){assert(reg==0x10 && options==kIOMapInhibitCache);++maps;return noMap?nullptr:&map;}
};
BODY
int main(int argc,char **argv){assert(argc==2);unsigned test=atoi(argv[1]);IOPCIDevice pci;auto p=XELP::identify(0xA7A08086,4);
 switch(test){
 case 0:break;case 1:enabled=false;break;case 2:p=XELP::identify(0x9A498086,1);break;
 case 3:p.revision=0;break;case 4:p=XELP::identify(0xA7A08086,4,0x9A498086);break;
 case 5:version[22]='4';break;case 6:pci.config[0]=0x9A498086;break;
 case 7:pci.config[8]=0;break;case 8:pci.config[4]=0;break;case 9:pci.config[4]=0xffff;break;
 case 10:pci.config[0x10]=0x10000000;break;case 11:pci.config[0x14]=UINT32_MAX;break;
 case 12:pci.noBar=true;break;case 13:pci.noAperture=true;break;case 14:pci.bar.physical+=4096;break;
 case 15:pci.bar.length=4096;break;case 16:pci.bar.span=4096;break;case 17:pci.config[0x50]=0;break;
 case 18:pci.noMap=true;break;case 19:pci.map.length=4096;break;case 20:pci.map.physical+=4096;break;
 case 21:pci.map.nullAddress=true;break;case 22:pci.map.memory[0xA188/4]=0;break;
 }
 RaptorLakeP::probe(&pci,p);
 if(test>=1 && test<=5){assert(!pci.reads && !pci.maps && pci.properties.empty());return 0;}
 if(test>=6 && test<=17){assert(!pci.maps && !pci.properties.count("WhateverXeRPLPGT"));return 0;}
 assert(pci.maps==1);
 if(test==18){assert(!pci.map.releases && !pci.properties.count("WhateverXeRPLPGT"));return 0;}
 assert(pci.map.releases==1);
 if(test>=19 && test<=21){assert(!pci.properties.count("WhateverXeRPLPGT"));return 0;}
 RaptorLakeP::Capture capture;auto bytes=pci.properties.at("WhateverXeRPLPGT");assert(bytes.size()==sizeof(capture));memcpy(&capture,bytes.data(),bytes.size());
 assert(capture.status==(test==22?RaptorLakeP::Status::AckTimeout:RaptorLakeP::Status::ExistingOwner));
 assert(capture.writes==(test==22?4U:0U));if(test==22)assert(delays==1000 && capture.releasedMask==3);
 unsigned reads=pci.reads;RaptorLakeP::probe(&pci,p);assert(pci.reads==reads && pci.maps==1 && pci.map.releases==1);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-adapter-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for n in range(23):subprocess.run([str(p/'test'),str(n)],check=True)
print('PASS RPL-P production adapter: identity/OS gates, physical BAR mapping, cleanup and single attempt')
