"""Execute catalogue admission, native adoption and bounded handover waits."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('bool nativeHeadReady('):s.index('bool waitForNativeTimings(')];adopt=s[s.index('bool wexRplReferenceAdoptFramebuffer('):s.index('bool wexRplReferenceDisplayPipeInit(')]
fixture=r'''#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <map>
using thread_call_param_t=void *;constexpr unsigned kIOServiceRegisteredState=2,kIOServiceRequired=4,kIOServiceSynchronous=2,kIOServiceAsynchronous=1;
unsigned mode,published,submitted,sleeps,retains,terminated,rematched;
struct OSObject{virtual ~OSObject()=default;void release(){delete this;}void retain(){++retains;}};
struct OSString:OSObject{std::string value;static OSString *withCString(const char *s){assert(!strcmp(s,"0xA7A08086 0x9A498086"));if(mode==5)return nullptr;auto p=new OSString;p->value=s;return p;}};
struct OSNumber:OSObject{unsigned unsigned32BitValue(){return mode==4?1001:80000;}} score;
struct OSDictionary:OSObject{std::map<std::string,OSObject *>values;static OSDictionary *withCapacity(unsigned n){assert(n==1);return mode==6?nullptr:new OSDictionary;}OSObject *getObject(const char *s){assert(!strcmp(s,"IOProbeScore"));return &score;}bool setObject(const char *s,OSObject *p){assert(p);values[s]=p;return mode!=7;}};
struct OSArray:OSObject{OSDictionary *dict=nullptr;static OSArray *withCapacity(unsigned n){assert(n==1);return mode==8?nullptr:new OSArray;}bool setObject(OSDictionary *p){dict=p;return true;}};
const void *gIOServicePlane=(void *)1;struct Meta{const char *name="IOService";const char *getClassName(){return name;}unsigned size=0x1C48;unsigned getClassSize(){return size;}};
struct IOService;struct Iterator:OSObject{unsigned i=0;OSObject *getNextObject();};
OSObject proof;
struct IOService:OSObject{uint8_t bytes[0x5000]{};Meta meta;IOService *provider=nullptr;bool inactive=false,registered=true;bool terminate(unsigned flags){assert(flags==(kIOServiceRequired|kIOServiceSynchronous)&&submitted==1);++terminated;if(mode==21)return false;if(mode!=22){inactive=true;provider=nullptr;}return true;}void registerService(unsigned flags){assert(flags==kIOServiceAsynchronous);++rematched;}Meta *getMetaClass(){return &meta;}uint64_t getRegistryEntryID(){return 77;}IOService *getParentEntry(const void *plane){assert(plane==gIOServicePlane);return provider;}Iterator *getClientIterator(){return mode==1?nullptr:new Iterator;}IOService *getProvider(){return provider;}bool isInactive(){return inactive;}unsigned getState(){return registered?2:0;}OSObject *getProperty(const char *s){assert(!strcmp(s,"WhateverXeRPLPReferencePhysical"));return mode==10?nullptr:&proof;}static OSDictionary *serviceMatching(const char *s){assert(!strcmp(s,"IONDRVFramebuffer"));return mode==1?nullptr:new OSDictionary;}static Iterator *getMatchingServices(OSDictionary *){return mode==1?nullptr:new Iterator;}};
IOService targetObject,controller,head,firmware,other,*target=&targetObject,*nativeOwner=&other,*framebufferOwner,*firmwareOwner;
OSObject *Iterator::getNextObject(){return i++==0||(mode==2&&i==2)?&firmware:nullptr;}
struct Catalogue{bool addDrivers(OSArray *a,bool match){++submitted;assert(!match&&a&&a->dict&&!terminated);auto p=dynamic_cast<OSDictionary *>(a->dict->values["IOPropertyMatch"]);assert(p&&p->values["WhateverXeRPLPReferencePhysical"]==&proof);return mode!=9;}} catalogue,*gIOCatalogue=&catalogue;
OSObject *OSUnserializeXML(const char *,OSString **){if(mode==3)return nullptr;if(mode==13)return new OSObject;return new OSDictionary;}
#define OSDynamicCast(T,p) dynamic_cast<T *>(p)
#define OSSafeReleaseNULL(p) do{if(p){p->release();p=nullptr;}}while(0)
uint32_t state[9]{1,1,1,1};uint64_t panelState[16]{1},handover[8]{1},firmwareScan[8]{1};const char referenceFramebufferPersonalityXML[]="metadata";
bool wexRplReferencePanel(){return mode!=11;}bool wexRplReferenceArmed(){return mode!=12;}void publish(){++published;}void IOSleep(unsigned n){assert(n==25);++sleeps;}
BODY
ADOPT
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);firmware.provider=target;firmware.meta.name="IONDRVFramebuffer";head.provider=target;head.meta.size=0x4B98;
 if(mode<=13||mode>=21){framebufferWorker(nullptr,nullptr);
  if(mode==11||mode==12){assert(!published&&!submitted);return 0;}
  bool admitted=mode==0||mode==1||mode>=21;bool ok=mode==0||mode==1;assert((handover[1]==2)==admitted);assert(submitted==unsigned(admitted||mode==9));if(!ok)assert(handover[6]);assert(rematched==unsigned(ok));assert(terminated==unsigned(mode==0||mode>=21));
  auto calls=submitted;framebufferWorker(nullptr,nullptr);assert(submitted==calls);return 0;
 }
 controller.provider=target;
 if(mode==14){controller.meta.size=0x1C40;assert(!wexRplReferenceAdoptFramebuffer(&controller));return 0;}
 if(mode==15){framebufferOwner=&other;assert(!wexRplReferenceAdoptFramebuffer(&controller));return 0;}
 assert(wexRplReferenceAdoptFramebuffer(&controller)&&framebufferOwner==&controller&&panelState[1]==4&&handover[2]);
 assert(wexRplReferenceAdoptFramebuffer(&controller));
 IOService *h=&head;memcpy(reinterpret_cast<uint8_t *>(&controller)+0xD60,&h,8);firmwareOwner=&firmware;firmware.inactive=mode!=17;if(firmware.inactive)firmware.provider=nullptr;
 if(mode==18)head.registered=false;if(mode==19)head.provider=&other;
 if(mode==16){assert(waitForNativeHead()&&!sleeps);wexRplReferenceFramebufferStarted(&controller,true);assert(panelState[1]==6&&panelState[9]&&handover[7]);}
 else if(mode<=19){assert(!waitForNativeHead()&&sleeps==1800&&handover[6]==8);}
 else{wexRplReferenceFramebufferStarted(&controller,false);assert(panelState[15]==7&&handover[6]==7);assert(!waitForNativeHead()&&!sleeps);}
}'''.replace('BODY',body).replace('ADOPT',adopt)
with tempfile.TemporaryDirectory(prefix='rplp-reference-matching-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in list(range(10))+[10,11,12,13,14,15,16,17,18,19,20,21,22]:subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS actual native matching: 23 admission, metadata/score/identity, single-owner adoption, firmware arbitration, head readiness and bounded refusal cases')
