"""Replay the production scanout entry against the observed null-pipe panic."""
from pathlib import Path
import subprocess,tempfile,hashlib
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text()
body=s[s.index('uint32_t guardedScanoutMemory('):s.index('uint32_t guardedAcceleratorProbe(')]
f=r'''
#include <cassert>
#include <cstdint>
using mach_vm_address_t=uintptr_t;
constexpr uint32_t kIOReturnNotReady=0xE00002D8;
bool panel=true;void *target=reinterpret_cast<void *>(1);uint32_t state[9]{},scanoutGate[7]{};
bool wexRplReferencePanel(){return panel;}
void *expectedPipe,*expectedMode;uint32_t expectedFlags,result;unsigned calls;
uint32_t original(void *p,void *m,uint32_t flags){assert(p==expectedPipe&&m==expectedMode&&flags==expectedFlags);++calls;return result;}
mach_vm_address_t scanoutMemoryOriginal=reinterpret_cast<uintptr_t>(original);
BODY
int main(){
 for(unsigned b=0;b<32;++b){
  panel=b&1;target=(b&2)?reinterpret_cast<void *>(1):nullptr;state[1]=bool(b&4);
  expectedPipe=(b&8)?reinterpret_cast<void *>(0x100):nullptr;
  expectedMode=(b&16)?reinterpret_cast<void *>(0x200):nullptr;
  for(uint32_t flags: {0U,1U,4U,0xFFFFFFFFU}){
   expectedFlags=flags;result=0xE00002BC;calls=0;
   bool refused=panel&&target&&state[1]&&!expectedPipe;
   auto got=guardedScanoutMemory(expectedPipe,expectedMode,flags);
   assert(got==(refused?kIOReturnNotReady:result));assert(calls==unsigned(!refused));
  }
 }
 panel=true;target=reinterpret_cast<void *>(1);state[1]=1;scanoutMemoryOriginal=0;
 assert(guardedScanoutMemory(nullptr,nullptr,0)==kIOReturnNotReady);
 assert(guardedScanoutMemory(reinterpret_cast<void *>(1),nullptr,0)==kIOReturnNotReady);
}
'''.replace('BODY',body).replace('#include <cstdint>','#include <cstdint>\n#include <initializer_list>')
with tempfile.TemporaryDirectory(prefix='rplp-scanout-gate-') as d:
 p=Path(d);(p/'test.cpp').write_text(f)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
for forbidden in ('setProperty(', 'IOSleep(', 'readReg32(', 'withOptions(', 'release('):assert forbidden not in body
# Pin the exact observed donor/call ABI, including the null-this dereference.
donor=Path('/Library/Extensions/AppleIntelTGLGraphics.kext/Contents/MacOS/AppleIntelTGLGraphics')
if donor.exists():
 data=donor.read_bytes();assert hashlib.sha256(data).hexdigest()=='6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64'
 assert data[0x7E5B2:0x7E5C3]==bytes.fromhex('554889e54157415641554154534883ec18')
 assert data[0x7F621:0x7F628]==bytes.fromhex('488bbf88000000')
print('PASS actual scanout guard:128 physical/panel/pipe/mode/flags cases; null pipe refused before donor; valid/foreign arguments and IOReturn preserved; exact panic bytes verified')
