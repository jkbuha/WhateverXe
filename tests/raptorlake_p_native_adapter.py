"""Replay the production native boundary, patch ordering and deferred matching."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_native.cpp').read_text();body=s[s.index('namespace RaptorLakeP {namespace Native {'):]
fixture=r'''
#include "kern_raptorlake_p_native.hpp"
#include "raptorlake_p_native_fixture.hpp"
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#include <cstdlib>
using mach_vm_address_t=uintptr_t;using SInt32=int32_t;using thread_call_param_t=void *;
unsigned mode=0,loads=0,adds=0,scheduled=0,probeCalls=0,startCalls=0;
bool nativeFlag=true,probeFlag=true,bcsFlag=false,rcsFlag=false;
char version[]="Darwin Kernel Version 25.6.0: mock/RELEASE_X86_64";
bool checkKernelArgument(const char *s){if(!strcmp(s,"-wex-rplp-manual-arm"))return mode>=36;if(!strcmp(s,"-wex-rplp-probe"))return probeFlag;if(!strcmp(s,"-wex-rplp-native-probe"))return nativeFlag;if(!strcmp(s,"-wex-rplp-bcs"))return bcsFlag;if(!strcmp(s,"-wex-rplp-rcs"))return rcsFlag;assert(false);return false;}
struct OSObject {virtual ~OSObject()=default;unsigned refs=1;void retain(){++refs;}void release(){if(!--refs)delete this;}};
struct OSString:OSObject {std::string text;OSString(const char *s):text(s){}static OSString *withCString(const char *s){return mode==24?nullptr:new OSString(s);}};
struct OSSymbol:OSString {using OSString::OSString;static OSSymbol *withCString(const char *s){return new OSSymbol(s);}};
struct OSNumber:OSObject {uint64_t n;OSNumber(uint64_t v):n(v){}static OSNumber *withNumber(uint64_t v,unsigned bits){assert(bits==32);return mode==23?nullptr:new OSNumber(v);}};
struct OSDictionary:OSObject {std::map<std::string,OSObject *> values;~OSDictionary(){for(auto &p:values)p.second->release();}static OSDictionary *withCapacity(unsigned){return mode==21?nullptr:new OSDictionary;}bool setObject(const char *k,OSObject *v){assert(v);v->retain();values[k]=v;return true;}};
struct OSArray:OSObject {std::vector<OSObject *> values;~OSArray(){for(auto p:values)p->release();}static OSArray *withCapacity(unsigned){return mode==22?nullptr:new OSArray;}bool setObject(OSObject *v){assert(v);v->retain();values.push_back(v);return true;}};
struct OSMetaClass {unsigned getClassSize()const{return mode==33?0x1000:0x2000;}static const OSMetaClass *getMetaClassWithName(OSSymbol *s){assert(s->text=="IntelAccelerator");static OSMetaClass m;return mode==20?nullptr:&m;}};
struct IOService:OSObject {uint8_t object[0x2000]{};const OSMetaClass *getMetaClass(){static OSMetaClass m;return &m;}};
struct IOPCIDevice:IOService {XELP::Profile profile=XELP::identify(0xA7A08086,4);std::vector<uint8_t> property;void setProperty(const char *key,const void *data,unsigned length){assert(!strcmp(key,"WhateverXeRPLPNative"));auto p=static_cast<const uint8_t *>(data);property={p,p+length};}};
#define OSDynamicCast(type,p) dynamic_cast<type *>(p)
#define OSSafeReleaseNULL(x) do{if(x){(x)->release();x=nullptr;}}while(0)
namespace XELP {Profile deviceProfile(IOPCIDevice *pci){return pci->profile;}}
struct IOCatalogue{};IOCatalogue catalogueObject;IOCatalogue *cataloguePointer=&catalogueObject;
IOPCIDevice *device;
IOService *realProbe(IOService *object,IOService *,SInt32 *){++probeCalls;uint32_t id=0xA7A08086,rev=4,selector=mode==27?1:2;memcpy(reinterpret_cast<uint8_t *>(object)+0x1118,&id,4);memcpy(reinterpret_cast<uint8_t *>(object)+0x1110,&rev,4);memcpy(reinterpret_cast<uint8_t *>(object)+0x1120,&selector,4);return mode==26?nullptr:object;}
bool realStart(IOService *,IOService *){++startCalls;return true;}
uintptr_t routedProbe=0,routedStart=0;std::vector<std::string> routes;
int32_t mockLoad(const char *id,bool defer,bool delay,uint8_t start,uint8_t matching,OSArray *names){assert(!strcmp(id,"com.apple.driver.AppleIntelTGLGraphics") && defer && delay && start==0 && matching==2 && !names);++loads;return 0;}
bool mockAdd(IOCatalogue *c,OSArray *list,bool matching){
 assert(c==cataloguePointer && matching && list->values.size()==1);++adds;auto d=dynamic_cast<OSDictionary *>(list->values[0]);assert(d);
 for(auto pair:std::vector<std::pair<std::string,std::string>>{{"CFBundleIdentifier","com.apple.driver.AppleIntelTGLGraphics"},{"IOClass","IntelAccelerator"},{"IOProviderClass","IOPCIDevice"},{"IOPCIPrimaryMatch","0xa7a08086"},{"IOPCIClassMatch","0x03000000&0xff000000"},{"IOMatchCategory","IOAccelerator"}})assert(dynamic_cast<OSString *>(d->values.at(pair.first))->text==pair.second);
 assert(dynamic_cast<OSNumber *>(d->values.at("IOProbeScore"))->n==1001);
 if(mode==25)return false;
 IOService object;SInt32 score=1001;auto result=reinterpret_cast<IOService *(*)(IOService *,IOService *,SInt32 *)>(routedProbe)(&object,device,&score);
 if(result)assert(!reinterpret_cast<bool (*)(IOService *,IOService *)>(routedStart)(result,device));
 return true;
}
struct Thread{void (*fn)(void *,void *);};using thread_call_t=Thread *;Thread thread;
thread_call_t thread_call_allocate(void(*fn)(void *,void *),void *arg){assert(!arg);thread.fn=fn;return mode==13?nullptr:&thread;}
void clock_interval_to_deadline(unsigned i,unsigned scale,uint64_t *deadline){assert(i==1 && scale==1000000000);*deadline=1;}
void thread_call_enter_delayed(thread_call_t t,uint64_t deadline){assert(t==&thread && deadline==1);++scheduled;}
struct KernelPatcher {
 static constexpr unsigned KernelID=0;
 struct KextInfo {static constexpr unsigned Unloaded=0;const char *id;const char **paths;size_t pathNum;bool sys[6];bool user[2];size_t loadIndex;};
 struct RouteRequest {const char *symbol;uintptr_t replacement;uintptr_t &original;template<class F>RouteRequest(const char *s,F f,uintptr_t &o):symbol(s),replacement(reinterpret_cast<uintptr_t>(f)),original(o){}};
 struct LookupPatch {KextInfo *info;const uint8_t *find,*replace;size_t size,count;};
 enum class Error {NoError,Failure};Error error=Error::NoError;
 uintptr_t solveSymbol(size_t id,const char *name){assert(id==0);if(strstr(name,"loadKextWithIdentifier"))return mode==12?0:reinterpret_cast<uintptr_t>(mockLoad);if(!strcmp(name,"_gIOCatalogue"))return reinterpret_cast<uintptr_t>(&cataloguePointer);if(strstr(name,"addDrivers"))return reinterpret_cast<uintptr_t>(mockAdd);assert(false);return 0;}
 template<class T>T solveSymbol(size_t id,const char *symbol,uintptr_t address,size_t size){assert(id==7 && size>=0x30000);if(strstr(symbol,"5probe"))return T(address+RaptorLakeP::Native::ProbeOffset+(mode==15?1:0));return T(address+RaptorLakeP::Native::StartOffset+(mode==16?1:0));}
 bool routeMultiple(size_t id,RouteRequest *request,unsigned count,uintptr_t,size_t){assert(id==7 && count==1);bool isStart=strstr(request->symbol,"5start");routes.push_back(isStart?"start":"probe");if((mode==17&&isStart)||(mode==18&&!isStart))return false;if(isStart){routedStart=request->replacement;request->original=reinterpret_cast<uintptr_t>(realStart);}else{routedProbe=request->replacement;request->original=reinterpret_cast<uintptr_t>(realProbe);}return true;}
 void applyLookupPatch(LookupPatch *patch,uint8_t *where,size_t bytes){assert(routes==std::vector<std::string>({"start","probe"}) && bytes==9 && patch->count==1 && patch->size==8 && !memcmp(where,patch->find,8));if(mode==19){error=Error::Failure;return;}memcpy(where,patch->replace,8);}
 void clearError(){error=Error::NoError;}Error getError(){return error;}
};
struct LiluAPI {enum class Error {NoError,Failure};};
struct {LiluAPI::Error onPatcherLoad(void(*)(void *,KernelPatcher &),void *){return mode==10?LiluAPI::Error::Failure:LiluAPI::Error::NoError;}LiluAPI::Error onKextLoad(KernelPatcher::KextInfo *info,unsigned n,void(*)(void *,KernelPatcher &,size_t,uintptr_t,size_t),void *){assert(n==1);info->loadIndex=7;return mode==11?LiluAPI::Error::Failure:LiluAPI::Error::NoError;}}lilu;
namespace RaptorLakeP {namespace Control {bool create(IOPCIDevice *p){assert(p==device);return mode!=37;}void update(bool,bool){}}}
namespace RaptorLakeP {namespace Bringup {bool prepare(KernelPatcher &,IOPCIDevice *,uintptr_t,size_t){return true;}}}
namespace RaptorLakeP {namespace NativeMemory { bool prepare(KernelPatcher &,size_t,uintptr_t,size_t){return mode!=35;} void run(IOService *,IOPCIDevice *){} }}
namespace RaptorLakeP{namespace NativeEngines{bool enabled(){return false;}bool prepare(KernelPatcher &,size_t,uintptr_t,size_t){assert(false);return false;}void run(IOPCIDevice *,bool){assert(false);}}namespace NativeMemory{bool passed(){return true;}}}
BODY
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);using namespace RaptorLakeP;using namespace Native;IOPCIDevice pci;device=&pci;
 if(mode==1)nativeFlag=false;if(mode==2)probeFlag=false;if(mode==3)bcsFlag=true;if(mode==4)rcsFlag=true;if(mode==5)pci.profile=XELP::identify(0xA7A08086,0);if(mode==6)pci.profile=XELP::identify(0xA7A08086,4,0x9A498086);if(mode==34)version[22]='4';
 init();KernelPatcher patcher;processKernel(nullptr,patcher);
 Capture gt;gt.status=Status::Complete;gt.topology.eus=96;gt.releasedMask=3;
 GOPCapture gop;gop.status=GOPStatus::Complete;if(mode==7)gt.status=Status::Unavailable;if(mode==8)gt.topology.eus=64;if(mode==9)gop.status=GOPStatus::Incomplete;if(mode==30)gt.releaseFailedMask=1;if(mode==31)gt.releasedMask=1;
 arm(&pci,pci.profile,gt,gop);
 const bool skipped=(mode>=1&&mode<=11)||mode==30||mode==31||mode==34;
 if(skipped){assert(!get(Armed) && !loads && !adds && !probeCalls && !startCalls);return 0;}
 if(mode==12||mode==13){assert(get(Failure)==(mode==12?1:2) && !loads && !scheduled);return 0;}
 assert(get(Armed) && scheduled==1);if(mode==28)pci.profile=XELP::identify(0x9A498086,1);
 worker(nullptr,nullptr);if(mode==28){assert(!loads && !adds);return 0;}if(mode>=36){assert(!loads&&!adds);if(mode==37){assert(get(Failure)==6);return 0;}assert(!requestRemoteArm());}else assert(loads==1 && !adds);
 auto image=nativeImage();if(mode==14)image[40]^=1;if(mode==32)image[SKUOffset]^=1;
 processGraphics(nullptr,patcher,7,reinterpret_cast<uintptr_t>(image.data()),image.size());
 if((mode>=14&&mode<=19)||mode==32||mode==35){assert(get(Failure)==3 && !get(PatchReady) && !adds && !probeCalls && !startCalls);return 0;}
 if(mode>=36){
  if(mode==38)pci.profile=XELP::identify(0xA7A08086,5);
  if(mode==39)set(Failure,77);
  assert(requestRemoteArm()==(mode==36));
  if(mode!=36){assert(!loads&&!adds);return 0;}
  assert(!requestRemoteArm());worker(nullptr,nullptr);assert(loads==1&&adds==1&&get(StartBlocked)&&!startCalls);return 0;
 }
 assert(get(PatchReady) && routes==std::vector<std::string>({"start","probe"}));
 if(mode==20){for(unsigned n=0;n<20;++n)worker(nullptr,nullptr);assert(get(Failure)==4 && get(PublishAttempts)==20 && !adds);return 0;}
 worker(nullptr,nullptr);
 if(mode>=21&&mode<=25){assert(get(Failure)==5 && !get(Published) && !probeCalls && !startCalls);return 0;}
 assert(get(Published) && adds==1 && loads==1 && !startCalls);
 if(mode==26||mode==27||mode==33){assert(!get(ProbeMatched) && !get(StartBlocked));}
 else{assert(get(ProbeEntered) && get(ProbeReturned) && get(ProbeMatched) && get(StartBlocked) && probeCalls==1);}
 worker(nullptr,nullptr);assert(adds==1 && loads==1);IOService object;SInt32 score=0;assert(!probe(&object,&pci,&score));assert(!startCalls);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-native-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),'-I'+str(r/'tests'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for n in range(40):subprocess.run([str(p/'test'),str(n)],check=True)
print('PASS production native boundary: exact gates, start blocked first, patch failures, deferred class registration, matching and probe-only execution')
