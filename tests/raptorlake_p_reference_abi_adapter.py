"""Production kernel-only ABI dispatcher with no MMIO or kernel calls."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference_fb.cpp').read_text();body=s[s.index('bool wexRplReferenceABI(void *raw)'):s.index('\n#else')]
fixture=r'''#include <cassert>
#include "kern_raptorlake_p_reference_abi.hpp"
using namespace RaptorLakeP::ReferenceABI;
bool allowed=true;unsigned calls,last;uint64_t value,offset;volatile void *base;
bool wexRplReferenceFBAllowed(void *p){return allowed&&p==(void *)1;}
uint32_t rplFBRead(void *p,unsigned long o){assert(p==(void *)1);++calls;last=1;offset=o;return 0xABC;}
void rplFBWrite(void *p,unsigned long o,uint32_t v){assert(p==(void *)1);++calls;last=2;offset=o;value=v;}
uint64_t rplFBRead64(void *p,volatile void *b,unsigned long o){assert(p==(void *)1);++calls;last=3;base=b;offset=o;return 0x123456789ABULL;}
void rplFBWrite64(void *p,volatile void *b,unsigned long o,uint64_t v){assert(p==(void *)1);++calls;last=4;base=b;offset=o;value=v;}
uint64_t rplFBTime(void *p){assert(p==(void *)1);++calls;last=5;return 999;}
int rplFBDSB(void *p){assert(p==(void *)1);++calls;last=6;return -17;}
BODY
int main(){
 Request r{1,sizeof(Request),1,0,(void *)1,(void *)2,64,0};
 assert(!wexRplReferenceABI(nullptr)&&!calls);
 for(unsigned bad=0;bad<6;++bad){auto q=r;if(bad==0)q.version=0;if(bad==1)q.size--;if(bad==2)q.owner=nullptr;if(bad==3)q.owner=(void *)3;if(bad==4)q.operation=0;if(bad==5)q.operation=7;assert(!wexRplReferenceABI(&q)&&!calls);}
 allowed=false;assert(!wexRplReferenceABI(&r)&&!calls);allowed=true;
 for(unsigned op=1;op<=6;++op){r.operation=op;r.value=0x123456789ABULL;assert(wexRplReferenceABI(&r)&&last==op&&calls==op);
 if(op==1)assert(r.value==0xABC);if(op==2)assert(value==0x456789AB&&offset==64);if(op==3)assert(r.value==0x123456789ABULL&&base==(void *)2&&offset==64);if(op==4)assert(value==0x123456789ABULL&&base==(void *)2&&offset==64);if(op==5)assert(r.value==999);if(op==6)assert(r.status==static_cast<uint32_t>(-17));}
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-reference-abi-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True);subprocess.run([str(p/'test')],check=True)
print('PASS actual reference ABI dispatcher: closed owner gates, metadata rejection and six exact operations')
