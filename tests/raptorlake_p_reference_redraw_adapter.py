"""Exercise production redraw routing across selected and foreign framebuffer calls."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text()
body=s[s.index('void redrawFramebuffer('):s.index('void redrawKext(')]
f=r'''
#include "kern_raptorlake_p_reference_redraw.hpp"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <initializer_list>
struct Meta{unsigned size=0;unsigned getClassSize(){return size;}};
struct IOFramebuffer{uint8_t data[0x200]{};void *provider{};Meta meta;void *getProvider(){return provider;}Meta *getMetaClass(){return &meta;}};
uint32_t redrawState[6]{},state[9]{};uint8_t value,*redrawVerbose;uintptr_t redrawOriginal;void *target;bool panel,requested;unsigned calls;uint8_t observed;
bool wexRplReferencePanel(){return panel;}bool redrawRequested(){return requested;}
void original(IOFramebuffer *){++calls;observed=value;}
BODY
int main(){
 IOFramebuffer fb;int pci,foreign;target=&pci;redrawOriginal=reinterpret_cast<uintptr_t>(&original);
 for(unsigned bits=0;bits<512;++bits)for(uint8_t initial:{uint8_t(0),uint8_t(1),uint8_t(3)}){
  panel=bits&1;requested=bits&2;state[1]=bits&4;state[7]=bits&8;
  fb.provider=(bits&16)?target:&foreign;fb.meta.size=(bits&32)?0x4B98:0x500;
  uint32_t index=(bits&64)?0:1;memcpy(reinterpret_cast<uint8_t *>(&fb)+0x1DC,&index,4);
  redrawVerbose=(bits&128)?&value:nullptr;value=initial;calls=0;memset(redrawState,0,sizeof(redrawState));
  redrawFramebuffer((bits&256)?&fb:nullptr);
  bool yes=panel&&requested&&state[1]&&!state[7]&&(bits&16)&&(bits&32)&&(bits&64)&&(bits&128)&&(bits&256);
  assert(calls==1&&value==initial&&observed==(yes?1:initial));assert(redrawState[2]==1&&redrawState[3]==yes&&redrawState[4]==!yes);
 }
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='redraw-') as d:
 p=Path(d);(p/'test.cpp').write_text(f)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS 1536 production wrapper cases: one original call; foreign/null/no-argument unchanged; verbose byte restored')
