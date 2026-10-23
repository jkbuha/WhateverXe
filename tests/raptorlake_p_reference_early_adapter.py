"""Run the actual firmware probe wrapper without IOKit or hardware."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text()
request=s[s.index('bool earlyRequested(){'):s.index('void earlyPublish(')]
body=s[s.index('IOService *firmwareProbe('):s.index('void firmwareKext(')]
fixture=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include "kern_raptorlake_p_reference_early.hpp"
using mach_vm_address_t=uintptr_t;
struct Meta{const char *name;const char *getClassName(){return name;}} firmwareMeta{"IONDRVFramebuffer"},otherMeta{"OtherFramebuffer"};
struct IOService{Meta *meta;Meta *getMetaClass(){return meta;}} gpu{&otherMeta},foreign{&otherMeta},owner{&firmwareMeta},result{&otherMeta};
IOService *target=&gpu;uint32_t state[9]{},earlyState[8]{1};bool requested,tcss;unsigned originals,queued;void *earlyPublishWork=reinterpret_cast<void *>(1);
bool checkKernelArgument(const char *s){if(!strcmp(s,"-wex-rplp-early-display"))return requested;if(!strcmp(s,"-wex-rplp-tcss"))return tcss;assert(false);return false;}
void thread_call_enter(void *p){assert(p==earlyPublishWork);++queued;}
IOService *original(IOService *,IOService *,int32_t *score){++originals;if(score)*score+=7;return &result;}
mach_vm_address_t firmwareProbeOriginal=reinterpret_cast<uintptr_t>(original);
REQUEST
BODY
int main(){
 for(unsigned b=0;b<256;++b){
  memset(earlyState,0,sizeof(earlyState));earlyState[0]=1;originals=queued=0;
  requested=b&1;tcss=b&2;earlyState[1]=bool(b&4);state[1]=bool(b&8);state[7]=bool(b&16);
  auto provider=(b&32)?&gpu:&foreign;owner.meta=(b&64)?&firmwareMeta:&otherMeta;auto object=(b&128)?&owner:nullptr;
  int32_t score=123;bool blocked=requested&&earlyState[1]&&state[1]&&!state[7]&&(provider==target)&&object&&owner.meta==&firmwareMeta;
  auto got=firmwareProbe(object,provider,&score);
  assert(got==(blocked?nullptr:&result));assert(originals==!blocked&&queued==blocked&&score==(blocked?123:130));assert(earlyState[2]==1&&earlyState[3]==blocked);
 }
 requested=false;tcss=false;firmwareProbeOriginal=0;assert(!firmwareProbe(&owner,&gpu,nullptr));
}
'''.replace('REQUEST',request).replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-early-probe-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
assert 'thread_call_enter(earlyPublishWork)' in body and 'setProperty(' not in body and 'IOLog(' not in body
print('PASS actual early probe: 256 ownership/image/flag/class cases, score/return preservation, no probe-lock allocation or property writes')
