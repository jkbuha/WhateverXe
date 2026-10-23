"""Exercise actual requestProbe admission without running a graphics driver."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('uint32_t guardedAcceleratorProbe('):s.index('bool earlyRequested(){')]
f=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
using mach_vm_address_t=uintptr_t;
constexpr uint32_t kIOReturnNotReady=0xE00002D8;
struct Meta {unsigned size=0x1C80;unsigned getClassSize(){return size;}};
struct IOService {uint8_t bytes[0x1C80]{};IOService *provider{};bool inactive{};Meta meta;
 IOService *getProvider(){return provider;}bool isInactive(){return inactive;}Meta *getMetaClass(){return &meta;}} gpu,pci,foreign,other;
IOService *target=&pci,*nativeOwner=&gpu;
bool panel=true,shutting=false;unsigned originals;
bool wexRplReferencePanel(){return panel;}
bool shutdown(){return shutting;}bool(*shutdownQuery)()=shutdown;
uint32_t state[9]{},acceleratorProbe[7]{1};
uint32_t original(IOService *,uint32_t options){assert(options==0xA5);++originals;return 0x123456;}
mach_vm_address_t acceleratorProbeOriginal=reinterpret_cast<uintptr_t>(original);
BODY
int main(){
 for(unsigned b=0;b<1024;++b){
  panel=b&1;gpu.provider=(b&2)?target:&foreign;nativeOwner=(b&4)?&gpu:&other;gpu.meta.size=(b&8)?0x1C80:0x1C58;
  state[1]=bool(b&16);state[4]=bool(b&32);state[7]=bool(b&64);gpu.inactive=b&128;shutting=b&256;
  uintptr_t gc=(b&512)?0xFFFF000000000100ULL:0;memcpy(gpu.bytes+0x1258,&gc,8);
  shutdownQuery=shutdown;originals=0;memset(acceleratorProbe,0,sizeof(acceleratorProbe));
  bool selected=panel&&gpu.provider==target;
  bool pass=!selected||(nativeOwner==&gpu&&gpu.meta.size==0x1C80&&state[1]&&state[4]&&!state[7]&&!gpu.inactive&&!shutting&&gc);
  auto got=guardedAcceleratorProbe(&gpu,0xA5);
  assert(got==(pass?0x123456:kIOReturnNotReady)&&originals==pass);
  assert(acceleratorProbe[2]==selected&&acceleratorProbe[4]==!pass);
 }
 panel=true;gpu.provider=target;nativeOwner=&gpu;gpu.meta.size=0x1C80;state[1]=state[4]=1;state[7]=0;gpu.inactive=false;shutting=false;
 uintptr_t gc=0;memcpy(gpu.bytes+0x1258,&gc,8);assert(guardedAcceleratorProbe(&gpu,0xA5)==kIOReturnNotReady&&acceleratorProbe[5]==4);
 gc=0x1234;memcpy(gpu.bytes+0x1258,&gc,8);shutdownQuery=nullptr;assert(guardedAcceleratorProbe(&gpu,0xA5)==kIOReturnNotReady&&acceleratorProbe[5]==3);
 acceleratorProbeOriginal=0;panel=false;assert(guardedAcceleratorProbe(&gpu,0xA5)==kIOReturnNotReady);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-probe-gate-') as d:
 p=Path(d);(p/'test.cpp').write_text(f);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
assert 'setProperty(' not in body and 'readReg32' not in body
print('PASS actual accelerator probe: 1024 owner/physical/arm/ABI/shutdown/collector cases; foreign provider/options/return preserved, missing collector rejects before native context allocation')
