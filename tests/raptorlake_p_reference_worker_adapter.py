"""Execute the actual optional backend worker with lifecycle failure injection."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();worker=s[s.index('void worker(thread_call_param_t'):s.index('\nvoid kernel(')]
fixture=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <string>
#include <map>
using thread_call_param_t=void *;using SInt32=int;constexpr unsigned kIOServiceAsynchronous=1;
unsigned mode,requests,timingChecks;uint32_t displayPipeReadyMask,bootProbe[8]{1};
bool waitForNativeHead(){return mode!=23;}bool waitForNativeTimings(){++timingChecks;return mode!=20;}unsigned monitor,framebufferWork;bool wexRplReferencePanel(){return mode>=20;}void thread_call_enter(unsigned){}
constexpr unsigned kIOFBUserRequestProbe=1;
unsigned allocations,initCalls,probeCalls,attachCalls,startCalls,registered,published;
struct OSObject{virtual ~OSObject()=default;void release(){delete this;}};
struct OSNumber:OSObject{unsigned value;static OSNumber *withNumber(unsigned n,unsigned bits){assert(bits==32);if(mode==4)return nullptr;auto p=new OSNumber;p->value=n;return p;}};
struct OSDictionary:OSObject{
 std::map<std::string,unsigned> values;
 OSObject *getObject(const char *s){assert(!strcmp(s,"Development"));return this;}
 static OSDictionary *withDictionary(OSDictionary *d){return mode==3?nullptr:new OSDictionary(*d);}
 bool setObject(const char *s,OSNumber *n){if(mode==5)return false;values[s]=n->value;return true;}
 bool setObject(const char *s,OSDictionary *d){assert(!strcmp(s,"Development"));values=d->values;return mode!=5;}
};
struct Meta{unsigned getClassSize(){return mode==2?0x100:0x1C58;}};
struct IOService:OSObject{
 unsigned char storage[0x2200]{};Meta meta;
 bool setProperty(const char *key,const uint32_t *value,unsigned n){assert(n==4);assert((!strcmp(key,"device-id")&&*value==0x9A49)||(!strcmp(key,"AAPL,ig-platform-id")&&*value==0x9A490000));return mode!=18;}
 unsigned configRead32(unsigned reg){assert(reg==0);return mode==19?0xA7A08086:0x9A498086;}
 Meta *getMetaClass(){return &meta;}
 bool init(OSDictionary *d){++initCalls;assert(d->values["AddressMode"]==64&&d->values["SingleChannelIndex"]==0&&d->values["MultiForceWakeSelect"]==1&&d->values["GraphicsSchedulerSelect"]==4&&d->values["RenderCompressionMode"]==0&&d->values["AllowIOSurfaceRenderCompression"]==0);return mode!=6;}
 IOService *probe(IOService *p,SInt32 *score){++probeCalls;assert(p&&*score==1001);uint32_t id=mode==8?0xA7A08086:0x9A498086,rev=mode==9?5:4,selector=mode==10?1:2;memcpy((uint8_t *)this+0x1118,&id,4);memcpy((uint8_t *)this+0x1110,&rev,4);memcpy((uint8_t *)this+0x1120,&selector,4);return mode==7?nullptr:mode==11?p:this;}
 bool attach(IOService *p){assert(p);++attachCalls;return mode!=12;}
 bool start(IOService *p){assert(p);++startCalls;return mode!=13;}
 uint32_t requestProbe(unsigned flags){assert(flags==kIOFBUserRequestProbe&&startCalls==1&&!registered);++requests;displayPipeReadyMask=mode==22?6:mode==25?7:1;return mode==21?0xE00002D8:0;}
 void registerService(unsigned flags){assert(flags==1);if(mode>=20)assert(requests==1&&(displayPipeReadyMask&1));++registered;}
};
struct OSMetaClass{static OSObject *allocClassWithName(const char *s){assert(!strcmp(s,"IntelAccelerator"));++allocations;return mode==1?nullptr:new IOService;}};
#define OSDynamicCast(T,p) dynamic_cast<T *>(p)
#define OSSafeReleaseNULL(p) do{if(p){p->release();p=nullptr;}}while(0)
uint32_t state[9]{1,1,1,1,1,1,0,0,1};uint64_t startup[16]{1};
IOService targetObject,*target=&targetObject,*nativeOwner;OSDictionary dictionary,*accelerator=&dictionary;
void publish(){++published;}
BODY
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);dictionary.values["AddressMode"]=64;dictionary.values["SingleChannelIndex"]=2;if(mode==14)state[4]=0;if(mode==15)state[7]=1;if(mode==16)accelerator=nullptr;if(mode==17)target=nullptr;
 if(mode>=20)framebufferWork=1;
 worker(nullptr,nullptr);
 if(mode>=14&&mode<=17){assert(!allocations&&!startCalls&&!published);return 0;}
 assert(allocations==1);assert(startCalls==unsigned(mode==0||mode==13||(mode>=20&&mode!=23)));
 if(mode==0||mode==24||mode==25){assert(startup[1]==7&&startup[12]&&startup[13]&&!startup[15]&&registered==1&&state[6]==1);}
 else{unsigned failure=mode==1?1:mode==2?2:mode<=5?3:mode==6?4:mode<=11?5:mode==12?6:mode==13?7:mode==18?8:mode==19?9:mode==20?11:mode==23?10:12;assert(startup[15]==failure);assert(!registered&&!state[6]);}
 if(nativeOwner){worker(nullptr,nullptr);assert(allocations==1);delete nativeOwner;}
}
'''.replace('BODY',worker)
with tempfile.TemporaryDirectory(prefix='rplp-reference-worker-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for n in range(26):subprocess.run([str(p/'test'),str(n)],check=True)
print('PASS production direct startup: 26 metadata/lifecycle/error cases, panel timing and probe before publication, native probe fields, no startup after earlier failure, owner pinned through hardware failure')
