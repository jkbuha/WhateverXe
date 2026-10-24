"""Scope the imported panel register pre-write without relying on GPU state."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('bool wexRplReferencePanelMode('):s.index('bool wexRplReferenceValidate(){')]
f=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
uint32_t state[9]{0,1};bool selected=true,allowed=true;void*framebufferOwner=(void*)1;
bool wexRplReferencePanel(){return selected;}bool wexRplReferenceFBAllowed(const void*p){return allowed&&p==framebufferOwner;}
struct Meta{const char*name="AppleIntelFramebufferDiags";unsigned size=0x4B98;const char*getClassName()const{return name;}unsigned getClassSize()const{return size;}};
struct IOService{alignas(16)uint8_t bytes[0x4B98]{};Meta meta;const Meta*getMetaClass()const{return &meta;}}head;
uint8_t path[0x3660]{};
BODY
int main(){
 uintptr_t owner=1;memcpy(head.bytes+0x1D0,&owner,8);assert(wexRplReferencePanelMode((void*)1,&head,path));
 for(unsigned failure=0;failure<11;failure++){
  if(failure==0)selected=false;if(failure==1)allowed=false;if(failure==2)state[1]=0;if(failure==3)state[7]=1;
  if(failure==4)head.meta.size--;if(failure==5)head.meta.name="ForeignFramebuffer";
  uint32_t index=failure==6?1:0,pipe=failure==7?1:0;memcpy(head.bytes+0x1DC,&index,4);memcpy(path+0x3640,&pipe,4);
  owner=failure==8?2:1;memcpy(head.bytes+0x1D0,&owner,8);
  assert(!wexRplReferencePanelMode(failure==9?nullptr:(void*)1,failure==10?nullptr:&head,path));
  selected=allowed=true;state[1]=1;state[7]=0;head.meta.size=0x4B98;head.meta.name="AppleIntelFramebufferDiags";
 }
 assert(!wexRplReferencePanelMode((void*)1,&head,nullptr));
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.cpp').write_text(f);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'t.cpp'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
print('PASS production panel pre-write scope: physical owner, exact head ABI, controller binding, head0/pipe0 and unavailable/foreign rejection')
