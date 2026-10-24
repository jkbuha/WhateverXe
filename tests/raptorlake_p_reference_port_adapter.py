"""Run the actual RPL-P compatibility export with mismatched-owner cases."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('uint32_t compatPortMode(void *port)'):s.index('uint32_t nativeAuxBypassRefs(')]
fixture=r'''#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include "kern_raptorlake_p_reference_fb_core.hpp"
using mach_vm_address_t=uintptr_t;
bool armed=true;unsigned size=0x700,reads,fallback;
struct Meta{unsigned getClassSize(){return size;}} meta;
struct OSObject{virtual ~OSObject()=default;Meta *getMetaClass(){return &meta;}};
struct Port:OSObject{uint8_t padding[0x700-8]{};} port;
uint8_t controller[0x1C48]{},platform[0x100]{};
uint32_t portModes[37]{1};bool tcss=false;bool wexRplReferenceTCSSRequested(){return tcss;}unsigned lastRegister;
void *framebufferOwner=controller;uint64_t panelState[16]{};uint32_t fia;
bool wexRplReferenceFBAllowed(void *p){return armed&&p==controller;}
struct Backend{uint32_t readReg32(unsigned r){assert(r==0x1638A0 || (r>=0x161500&&r<=0x16150C&&!(r&3)));lastRegister=r;++reads;return fia;}} backend;
uint32_t original(void *){++fallback;return 99;}mach_vm_address_t compatPortOriginal=reinterpret_cast<uintptr_t>(original);
BODY
void put(void *p,unsigned o,uintptr_t v){memcpy(static_cast<uint8_t *>(p)+o,&v,8);}
void setup(unsigned ddi){put(controller,0xC40,0x123);put(controller,0xD40,reinterpret_cast<uintptr_t>(platform));put(&port,0x578,0x123);put(&port,0x548,reinterpret_cast<uintptr_t>(platform+0x20+24*ddi));memcpy(platform+0x20+24*ddi,&ddi,4);}
int main(){
 assert(compatPortMode(nullptr)==99&&fallback==1);armed=false;assert(compatPortMode(&port)==99&&fallback==2&&!reads);armed=true;
 for(unsigned d=0;d<9;++d){setup(d);fia=0x40;auto expected=d<2?1:d==2?2:0;assert(compatPortMode(&port)==unsigned(expected));assert(panelState[12]==d&&panelState[13]==unsigned(expected));}
 auto denied=panelState[14];setup(0);size=0x6B0;assert(!compatPortMode(&port)&&panelState[14]==++denied);size=0x700;
 put(&port,0x578,0x124);assert(!compatPortMode(&port)&&panelState[14]==++denied);
 setup(0);put(&port,0x548,reinterpret_cast<uintptr_t>(platform+0x21));assert(!compatPortMode(&port)&&panelState[14]==++denied);
 setup(0);unsigned wrong=1;memcpy(platform+0x20,&wrong,4);assert(!compatPortMode(&port)&&panelState[14]==++denied);
 setup(0);auto before=reads;assert(compatPortMode(&port)==1&&reads==before);
 setup(3);fia=1U<<13;assert(compatPortMode(&port)==3&&reads==before+1);
 tcss=true;
 for(unsigned ddi=0;ddi<9;++ddi){
  setup(ddi);fia=0x35;unsigned oldReads=reads;
  auto mode=compatPortMode(&port);assert(mode==(ddi<2?1:ddi>=3&&ddi<=6?3:0));
  assert(reads==oldReads+(ddi>=3&&ddi<=6));
  if(ddi>=3&&ddi<=6)assert(lastRegister==0x161500+4*(ddi-3));
  auto i=1+4*ddi;assert(portModes[i]&&portModes[i+3]==mode);
 }
 setup(5);for(unsigned status:{0U,1U,2U,3U,4U,6U,7U,UINT32_MAX}){fia=status;assert(!compatPortMode(&port));}
 auto oldReads=reads;armed=false;assert(compatPortMode(&port)==99&&reads==oldReads);

}'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-reference-port-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print('PASS actual RPL-P port export: unarmed fallback, exact owner/ABI/record gates, Combo and FIA classes, read-only dispatch')
