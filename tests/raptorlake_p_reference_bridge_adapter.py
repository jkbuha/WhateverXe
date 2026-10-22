"""Execute the production KPI-only bridge lifecycle and all exported wrappers."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXeRPLPBridge/WhateverXeRPLPBridge.cpp').read_text();body=s[s.index('namespace {'):s.index('KMOD_EXPLICIT_DECL')]
fixture=r'''#include <cassert>
#include <cstring>
#include <cstdlib>
#include "kern_raptorlake_p_reference_abi.hpp"
unsigned mode,platformCalls,releases;using kern_return_t=int;using IOReturn=int;struct kmod_info_t{};constexpr int KERN_FAILURE=5,KERN_SUCCESS=0,kIOReturnSuccess=0,kIOReturnUnsupported=-1;
struct Matching {void release(){++releases;}} matching;
struct OSSymbol {
 const char *s;static const OSSymbol *withCStringNoCopy(const char *s){static OSSymbol value{s};return mode==3?nullptr:&value;}
};
struct IOService;
#define OSDynamicCast(T,p) static_cast<T *>(p)
struct OSIterator{unsigned n=0;IOService *getNextObject();void release(){++releases;}} iterator;
struct IOService {
 void retain(){}
 void release(){++releases;}
 static Matching *serviceMatching(const char *s){assert(!strcmp(s,"WhateverXeRPLPControl"));return mode==1?nullptr:&matching;}
 static OSIterator *getMatchingServices(Matching *p){assert(p==&matching);return mode==2?nullptr:&iterator;}
 int callPlatformFunction(const OSSymbol *s,bool wait,void *p,void *p2,void *p3,void *p4){
 assert(!strcmp(s->s,RaptorLakeP::ReferenceABI::selector)&&!wait&&!p2&&!p3&&!p4);
 auto &r=*static_cast<RaptorLakeP::ReferenceABI::Request *>(p);assert(RaptorLakeP::ReferenceABI::valid(r)&&r.owner==(void *)1);++platformCalls;
 if(mode==4)return kIOReturnUnsupported;
 if(r.operation<=4)assert(r.offset==32);if(r.operation==3||r.operation==4)assert(r.base==(void *)2);
 if(r.operation==2)assert(r.value==0x87654321);if(r.operation==4)assert(r.value==0x123456789ABULL);
 r.value=r.operation==5?999:0x123456789ABULL;r.status=7;return kIOReturnSuccess;
 }
};
IOService *OSIterator::getNextObject(){static IOService svc;return n++==0||(mode==6&&n==2)?&svc:nullptr;}
BODY
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);void *owner=(void *)1;
 assert(read32(owner,32)==0&&read64(owner,(void *)2,32)==0&&now(owner)==0&&dsb(owner)==kIOReturnUnsupported);write32(owner,32,0x87654321);write64(owner,(void *)2,32,0x123456789ABULL);assert(!platformCalls);
 auto rc=bridgeStart(nullptr,nullptr);
 if((mode>=1&&mode<=3)||mode==6){assert(rc==KERN_FAILURE&&!control);assert(read32(owner,32)==0&&!platformCalls);return 0;}
 assert(rc==KERN_SUCCESS&&control&&selector&&releases==2);assert(bridgeStart(nullptr,nullptr)==KERN_FAILURE);assert(bridgeStop(nullptr,nullptr)==KERN_FAILURE);
 assert(read32(owner,32)==(mode==4?0:0x456789AB));write32(owner,32,0x87654321);
 assert(read64(owner,(void *)2,32)==(mode==4?0:0x123456789ABULL));write64(owner,(void *)2,32,0x123456789ABULL);
 assert(now(owner)==(mode==4?0:999));assert(dsb(owner)==(mode==4?kIOReturnUnsupported:7));assert(platformCalls==6);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-kpi-bridge-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in range(7):subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS actual KPI bridge: inert before bind, failed lookup/selector, six wrappers, refused dispatch, pinned lifetime')
