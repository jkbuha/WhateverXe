"""Exercise the production exact-owner fAlwaysOn AUX reference reader."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('uint32_t nativeAuxBypassRefs('):s.index('// Optional TC3 training remains closed')]
f=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdio>
struct Meta {unsigned size=0x80;const char *name="AppleIntelPowerWell";unsigned getClassSize(){return size;}const char*getClassName(){return name;}} meta;
struct OSObject{Meta *m=&meta;uint8_t pad[0x78]{};Meta*getMetaClass(){return m;}} power;
struct IOService{};
alignas(16) uint8_t controller[0x1C48]{};
IOService *framebufferOwner=reinterpret_cast<IOService*>(controller);
bool ownerOK=true;unsigned ddi=5;void *port=(void*)1;
bool observerPort(void*p,uint32_t&d){d=ddi;return ownerOK&&p==port;}
BODY
int main(){
 auto p=&power;memcpy(controller+0xC8,&p,8);memcpy(reinterpret_cast<uint8_t*>(&power)+0x10,&framebufferOwner,8);
 auto bytes=reinterpret_cast<uint8_t*>(&power);bytes[0x18]=1;
 for(uint32_t count:{0U,1U,2U,17U,0xFFFFFFFEU}){memcpy(bytes+0x68,&count,4);assert(nativeAuxBypassRefs(port)==count);}
 auto before=power;auto beforeController=controller[0xC8];
 for(unsigned i=0;i<9;i++){
  if(i==0)ownerOK=false;if(i==1)ddi=0;if(i==2)ddi=4;if(i==3)meta.size=0x78;if(i==4)meta.name="Foreign";
  if(i==5)bytes[0x18]=0;if(i==6)bytes[0x18]=2;if(i==7){void*q=nullptr;memcpy(controller+0xC8,&q,8);}if(i==8){IOService*q=nullptr;memcpy(bytes+0x10,&q,8);}
  assert(nativeAuxBypassRefs(port)==~0U);ownerOK=true;ddi=5;meta.size=0x80;meta.name="AppleIntelPowerWell";power=before;memcpy(controller+0xC8,&p,8);
 }
 assert(nativeAuxBypassRefs((void*)2)==~0U);assert(!memcmp(&power,&before,sizeof(power))&&controller[0xC8]==beforeController);
 puts("PASS actual AUX bypass reader: exact TC3/controller/class/extent/flag checks, all count bits, no metadata mutation");
}
'''.replace('BODY',body).replace('#include <cstdio>','#include <cstdio>\n#include <initializer_list>')
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.cpp').write_text(f);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'t.cpp'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
