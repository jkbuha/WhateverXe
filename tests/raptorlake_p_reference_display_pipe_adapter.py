"""Execute the real display-pipe binding guard and argument forwarding."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('bool wexRplReferenceDisplayPipeInit('):s.index('void wexRplReferenceFBFailure(')]
fixture=r'''#include <cassert>
#include <cstdint>
#include <cstring>
using mach_vm_address_t=uintptr_t;
struct IOService;
struct Meta{unsigned size=0x4B98;const char *name="AppleIntelFramebufferDiags";unsigned getClassSize(){return size;}const char *getClassName(){return name;}};
struct IOService{uint8_t bytes[0x5000]{};Meta meta;IOService *provider=nullptr;Meta *getMetaClass(){return &meta;}IOService *getProvider(){return provider;}uint64_t getRegistryEntryID(){return 123;}} targetObject,controller,head,accel,firmware;
IOService *target=&targetObject,*framebufferOwner=&controller,*nativeOwner=&accel;
uint64_t panelState[16]{1},displayPipe[8]{1};uint32_t displayPipeReadyMask;bool panel=true,armed=true,result=true;unsigned calls,indexExpected;
bool wexRplReferencePanel(){return panel;}bool wexRplReferenceArmed(){return armed;}void publish(){}
bool original(void *p,void *g,void *m,void *f,uint32_t i){assert(p==(void *)1&&g==nativeOwner&&m==(void *)2&&f==&head&&i==indexExpected);++calls;return result;}
mach_vm_address_t wexRplDisplayPipeInitOriginal=reinterpret_cast<uintptr_t>(original);
BODY
void setHead(unsigned i){IOService *p=&head;memcpy(reinterpret_cast<uint8_t *>(&controller)+0xD60+8*i,&p,8);}
int main(){panelState[3]=0x1C48;head.provider=target;setHead(0);
 assert(wexRplReferenceDisplayPipeInit((void *)1,nativeOwner,(void *)2,&head,0)&&calls==1&&displayPipe[3]==2&&displayPipe[6]);
 firmware.meta.name="IONDRVFramebuffer";assert(!wexRplReferenceDisplayPipeInit((void *)1,nativeOwner,(void *)2,&firmware,0)&&calls==1&&displayPipe[3]==1&&displayPipe[7]==1);
 armed=false;assert(!wexRplReferenceDisplayPipeInit((void *)1,nativeOwner,(void *)2,&head,0)&&calls==1);armed=true;
 assert(!wexRplReferenceDisplayPipeInit((void *)1,&firmware,(void *)2,&head,0)&&calls==1);
 assert(!wexRplReferenceDisplayPipeInit((void *)1,nativeOwner,(void *)2,nullptr,0)&&calls==1);
 head.provider=&firmware;assert(!wexRplReferenceDisplayPipeInit((void *)1,nativeOwner,(void *)2,&head,0)&&calls==1);head.provider=target;
 head.meta.size=0x4B90;assert(!wexRplReferenceDisplayPipeInit((void *)1,nativeOwner,(void *)2,&head,0)&&calls==1);head.meta.size=0x4B98;
 assert(!wexRplReferenceDisplayPipeInit((void *)1,nativeOwner,(void *)2,&head,3)&&calls==1);
 setHead(2);indexExpected=2;assert(wexRplReferenceDisplayPipeInit((void *)1,nativeOwner,(void *)2,&head,2)&&calls==2);
 result=false;assert(!wexRplReferenceDisplayPipeInit((void *)1,nativeOwner,(void *)2,&head,2)&&calls==3&&displayPipe[7]==2);
 assert(displayPipeReadyMask==5);result=true;panel=false;assert(wexRplReferenceDisplayPipeInit((void *)1,nativeOwner,(void *)2,&head,2)&&calls==4);
}'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-reference-pipe-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print('PASS actual display-pipe binding: exact owner/head/index/ABI, firmware refusal before original, argument/result preservation and non-panel passthrough')
