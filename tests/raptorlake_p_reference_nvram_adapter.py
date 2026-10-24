"""Exercise the production framebuffer NVRAM installer with a mock patcher."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text()
body=s[s.index('bool wexRplReferenceNVRAMHooks('):s.index('bool wexRplReferencePortHooks(')]
fixture=r'''
#include "kern_raptorlake_p_reference_nvram.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using mach_vm_address_t=uintptr_t;
using namespace RaptorLakeP::ReferenceNVRAM;
bool panel=true,known=true,tahoe=true;unsigned state[9]{1,1};const char *version="";
namespace XELP{enum class OS{Tahoe,Unknown};OS kernelOS(const char *){return tahoe?OS::Tahoe:OS::Unknown;}}
struct Target{bool ready=false;void setProperty(const char *s,bool v){assert(!strcmp(s,"WhateverXeRPLPNVRAMABIReady"));ready=v;}} object,*target=&object;
bool wexRplReferencePanel(){return panel;}bool wexRplReferenceImage(void *,size_t,bool){return known;}
#define SYSLOG(...) do{}while(0)
struct KernelPatcher{
 static constexpr unsigned KernelID=0;enum class Error{NoError,Failed};Error error=Error::NoError;
 std::vector<uint8_t> table=std::vector<uint8_t>(0xA20);
 bool badMethod=false,badTable=false,badWriter=false,badReserved=false,failWrite=false;
 unsigned writes=0;
 KernelPatcher(){uintptr_t writer=0x1000,reserved=0x2000;memcpy(table.data()+16+WriteSlot,&writer,8);memcpy(table.data()+16+OldSlot,&reserved,8);}
 uintptr_t solveSymbol(unsigned i,const char *name,uintptr_t a=0,size_t=0){
  if(i!=KernelID){assert(!strcmp(name,"__ZN24AppleIntelBaseController11hwSaveNVRAMEv"));return badMethod?0:a+Method;}
  if(!strcmp(name,"__ZTV18IODTPlatformExpert"))return badTable?0:reinterpret_cast<uintptr_t>(table.data());
  if(!strcmp(name,"__ZN18IODTPlatformExpert18writeNVRAMPropertyEP15IORegistryEntryPK8OSSymbolP6OSData"))return badWriter?0:0x1000;
  assert(!strcmp(name,"__ZN18IODTPlatformExpert28_RESERVEDIODTPlatformExpert1Ev"));return badReserved?0:0x2000;
 }
 void clearError(){error=Error::NoError;}Error getError(){return error;}
 void routeBlock(uintptr_t where,const uint8_t *bytes,size_t n){++writes;if(failWrite){error=Error::Failed;return;}memcpy(reinterpret_cast<void *>(where),bytes,n);}
};
BODY
int main(){
 std::vector<uint8_t> image(Call+6,0xCC);auto a=reinterpret_cast<uintptr_t>(image.data());
 auto setup=[&](){memcpy(image.data()+Call,Before,6);object.ready=false;panel=known=tahoe=true;state[1]=1;target=&object;};
 setup();KernelPatcher p;assert(wexRplReferenceNVRAMHooks(p,1,a,image.size()));assert(p.writes==1&&object.ready);
 assert(!memcmp(image.data()+Call,After,6));assert(image[Call-1]==0xCC);
 for(unsigned fault=0;fault<12;++fault){
  setup();KernelPatcher q;size_t n=image.size();
  if(fault==0)panel=false;if(fault==1)known=false;if(fault==2)tahoe=false;
  if(fault==3)state[1]=0;if(fault==4)target=nullptr;if(fault==5)n=Call+5;
  if(fault==6)q.badMethod=true;if(fault==7)q.badTable=true;if(fault==8)q.badWriter=true;
  if(fault==9)q.badReserved=true;if(fault==10)image[Call+2]^=1;
  if(fault==11){uintptr_t foreign=0x3000;memcpy(q.table.data()+16+WriteSlot,&foreign,8);}
  assert(!wexRplReferenceNVRAMHooks(q,1,a,n)&&q.writes==0&&!object.ready);
 }
 setup();KernelPatcher q;q.failWrite=true;assert(!wexRplReferenceNVRAMHooks(q,1,a,image.size()));assert(!object.ready&&!memcmp(image.data()+Call,Before,6));
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-nvram-abi-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS production NVRAM installer: exact call correction; foreign ABI/image/owner/truncation rejected; failed write refused')
