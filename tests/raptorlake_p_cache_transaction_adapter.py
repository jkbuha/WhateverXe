"""Production hardware boundary backed by RAM: identity/lease/display/rollback."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_cache_transaction.cpp').read_text();body=s[s.index('namespace RaptorLakeP {namespace CacheTransaction {'):]
fixture=r'''
#include "kern_raptorlake_p_cache_transaction_core.hpp"
#include "kern_raptorlake_p_native_core.hpp"
#include "kern_xelp_memory_layout.hpp"
#include <cassert>
#include <cstring>
#include <cstdlib>
#include <vector>
unsigned mode,displayCalls;uint32_t *mmio;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
struct IOMapper{inline static void *gSystem=nullptr;};
bool checkKernelArgument(const char *s){if(!strcmp(s,"-wex-rplp-cache-transaction"))return mode!=1;if(!strcmp(s,"-wex-rplp-manual-arm"))return mode!=3;if(!strcmp(s,"-wex-rplp-bcs"))return false;if(!strcmp(s,"-wex-rplp-rcs"))return mode==25;return true;}
void IODelay(unsigned n){assert(n==100);if(mode!=15){mmio[0x130044/4]=mmio[0xa188/4]&1;mmio[0xd84/4]=mmio[0xa278/4]&1;}}
struct IOMemoryMap{uint64_t length=0x1000000,physical=0x4011000000ULL;uintptr_t address;uint64_t getLength(){return length;}uint64_t getPhysicalAddress(){return physical;}uintptr_t getVirtualAddress(){return address;}};
struct IOPCIDevice{
 XELP::Profile profile=XELP::identify(0xa7a08086,4);std::vector<uint64_t> result,admit;
 uint16_t configRead16(unsigned o){if(o==4)return mode==7?0:2;assert(o==0x50);return 0x2c1;}
 uint32_t configRead32(unsigned o){if(o==0)return mode==5?0:0xa7a08086;if(o==8)return mode==6?5:4;if(o==0x10)return 0x11000004;if(o==0x14)return 0x40;if(o==0xc0)return 0x6c800000;if(o==0xc4)return 0;assert(false);return 0;}
 void setProperty(const char *k,const void *p,unsigned n){if(!strcmp(k,"WhateverXeRPLPCacheTransaction"))result.assign((const uint64_t *)p,(const uint64_t *)p+n/8);else{assert(!strcmp(k,"WhateverXeRPLPCacheAdmission"));admit.assign((const uint64_t *)p,(const uint64_t *)p+n/8);}}
};
namespace XELP{Profile deviceProfile(IOPCIDevice *p){return p->profile;}}
namespace RaptorLakeP {
 enum class DisplayStatus{Skipped,Complete,Unavailable,PowerChanged};struct DisplayCapture{DisplayStatus status=DisplayStatus::Complete;uint32_t data=123;};
 template<class IO>DisplayCapture captureDisplay(IO &,bool enabled){assert(enabled);++displayCalls;DisplayCapture x;if((mode==18&&displayCalls==1)||(mode==19&&displayCalls==2)||(mode==20&&displayCalls==3))x.status=DisplayStatus::PowerChanged;return x;}
 namespace GTTSlot{template<class IO>bool qualify(IO &,XELPMemory::Layout l){assert(l.valid);return mode!=13;}}
}
BODY
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);using namespace RaptorLakeP;using namespace CacheTransaction;
 assert(!posix_memalign((void **)&mmio,4096,0x1000000));memset(mmio,0,0x1000000);
 mmio[0x9138/4]=1;mmio[0x913c/4]=0x3f;mmio[0x9118/4]=mode==14?1:0;
 if(mode==16)mmio[0xa188/4]=mmio[0x130044/4]=1;
 if(mode==17)mmio[0x2030/4]=8;
 uint32_t baseline[CacheShadow::Count][3]{};std::vector<uint32_t> before;
 for(unsigned i=0;i<CacheShadow::Count;++i){auto o=CacheShadow::offset(i);uint32_t v=o>=0x4800&&o<0x4820?3:o==0xb138?4:0x12340000+i;mmio[o/4]=v;baseline[i][0]=o;baseline[i][1]=baseline[i][2]=v;before.push_back(v);}
 uint8_t pat[8]{3,0,3,3,3,3,3,3};uint32_t l3[32],mocs[64];for(auto &v:l3)v=0x00100030;for(auto &v:mocs)v=0x51;
 IOPCIDevice pci;IOMemoryMap map;map.address=(uintptr_t)mmio;
 if(mode==4)IOMapper::gSystem=(void *)1;if(mode==8)pci.profile=XELP::identify(0xa7a08086,5);
 if(mode==9)map.length=4096;if(mode==10)map.physical=0;if(mode==11)map.address=0;if(mode==12)++map.address;
 if(mode==21)baseline[5][2]^=1;if(mode==22)pat[4]=255;
 if(mode==24)mmio[0x4800/4]=baseline[2][1]=baseline[2][2]=before[2]=0;
 bool ok=run(&pci,&map,pat,l3,mocs,mode==23?nullptr:baseline,mode!=2);
 if(mode==1){assert(ok&&!result.requested&&pci.result.empty());free(mmio);return 0;}
 assert(ok==(mode==0));assert(pci.admit.size()==9&&pci.result.size()==13);
 for(unsigned i=0;i<CacheShadow::Count;++i)assert(mmio[CacheShadow::offset(i)/4]==before[i]);
 if(mode==0){assert(result.writes==103&&result.verified==103&&result.restoreWrites==103&&result.restored==103&&!result.failure&&!admission[8]&&admission[5]==3&&admission[7]);}
 if(mode==20)assert(result.writes==103&&admission[8]==4);
 if(mode!=0&&mode!=20)assert(!result.writes);
 assert(!run(&pci,&map,pat,l3,mocs,baseline,true));free(mmio);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-cache-transaction-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture);subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in range(26):subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS production cache transaction:26 identity/prerequisite/BAR/GOP/forcewake/fuse/engine/display/plan/snapshot cases; every cache word restored')
