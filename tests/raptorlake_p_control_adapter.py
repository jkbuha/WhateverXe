"""Production control service privilege, command-shape and lifecycle boundary."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_control.cpp').read_text();body=s[s.index('class WhateverXeRPLPControl'):]
fixture=r'''
extern unsigned mode,requests;
namespace RaptorLakeP {namespace Reference {bool enabled(){return mode==14;}bool arm(){return false;}bool loadModules(){++requests;return mode==14;}bool captureState(){++requests;return mode==18;}}}
#include <cassert>
#include <cstring>
#include <map>
#include <string>
#include <cstdlib>
#include "kern_raptorlake_p_reference_abi.hpp"
bool wexRplReferenceABI(void *p){return p==(void *)0x1234;}
struct OSSymbol{const char *name;bool isEqualTo(const char *s)const{return !strcmp(name,s);}};
unsigned mode,freed,detached,registered,requests;
using IOReturn=int;constexpr int kIOReturnSuccess=0,kIOReturnUnsupported=1,kIOReturnNotPrivileged=2,kIOReturnNotReady=3;
#define OSDeclareDefaultStructors(x)
#define OSDefineMetaClassAndStructors(x,y)
#define OSDynamicCast(T,p) dynamic_cast<T *>(p)
struct OSObject{virtual ~OSObject()=default;void release(){++freed;delete this;}};
struct OSBoolean:OSObject{};OSBoolean value;OSBoolean *kOSBooleanTrue=&value;
struct OSDictionary:OSObject{std::map<std::string,OSObject *> v;unsigned getCount(){return v.size();}OSObject *getObject(const char *k){auto i=v.find(k);return i==v.end()?nullptr:i->second;}};
struct IOService:OSObject{
 std::map<std::string,unsigned> p;std::string name;
 virtual IOReturn callPlatformFunction(const OSSymbol *,bool,void *,void *,void *,void *){return kIOReturnUnsupported;}
 virtual IOReturn setProperties(OSObject *){return kIOReturnUnsupported;}
 bool init(){return mode!=1;}bool attach(IOService *){return mode!=2;}bool start(IOService *){return mode!=3;}void detach(IOService *){++detached;}
 void setName(const char *s){name=s;}void setProperty(const char *k,unsigned n,unsigned bits){assert(bits==32);p[k]=n;}void setProperty(const char *k,bool b){p[k]=b;}
 void registerService(){++registered;}
};
struct IOPCIDevice:IOService{};
const char *kIOClientPrivilegeAdministrator="root";void *current_task(){return (void *)1;}
struct IOUserClient{static int clientHasPrivilege(void *t,const char *s){assert(t==(void *)1&&!strcmp(s,"root"));return (mode==6||mode==13||mode==17)?99:0;}};
namespace RaptorLakeP{namespace Native{bool requestRemoteArm(){++requests;return mode!=8;}}}
BODY
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);IOPCIDevice pci;
 using namespace RaptorLakeP::Control;
 update(true,true);bool made=create(mode==4?nullptr:&pci);
 if(mode>=1&&mode<=4){assert(!made&&!service&&!registered);assert(freed==(mode==4?0U:1U));assert(detached==(mode==3?1U:0U));return 0;}
 assert(made&&registered==1&&service->p["ControlVersion"]==1&&!service->p["DiagnosticReady"]&&!service->p["DiagnosticArmed"]);
 assert(create(&pci)&&registered==1);update(true,false);assert(service->p["DiagnosticReady"]&&!service->p["DiagnosticArmed"]);
 OSSymbol pf{RaptorLakeP::ReferenceABI::selector};assert(service->callPlatformFunction(&pf,false,(void *)0x1234,nullptr,nullptr,nullptr)==kIOReturnSuccess);assert(service->callPlatformFunction(&pf,true,(void *)0x1234,nullptr,nullptr,nullptr)==kIOReturnUnsupported);assert(service->callPlatformFunction(&pf,false,(void *)0x1234,(void *)1,nullptr,nullptr)==kIOReturnUnsupported);assert(service->callPlatformFunction(&pf,false,nullptr,nullptr,nullptr,nullptr)==kIOReturnUnsupported);
 OSDictionary dict;dict.v["BeginDiagnostic"]=kOSBooleanTrue;OSBoolean bad;
 if(mode>=12){dict.v.clear();dict.v["LoadReferenceModules"]=mode==15?&bad:kOSBooleanTrue;}
 if(mode>=16){dict.v.clear();dict.v["CaptureReferenceState"]=mode==19?&bad:kOSBooleanTrue;if(mode==20)dict.v["Other"]=kOSBooleanTrue;}
 if(mode==5)dict.v["Other"]=kOSBooleanTrue;
 if(mode==7)dict.v["BeginDiagnostic"]=&bad;
 if(mode==9)dict.v.clear();
 int result=service->setProperties(mode==10?nullptr:mode==11?static_cast<OSObject *>(&bad):&dict);
 if(mode==5||mode==7||mode==9||mode==10||mode==11||mode==15||mode==19||mode==20){assert(result==kIOReturnUnsupported&&!requests);}
 else if(mode==6||mode==13||mode==17){assert(result==kIOReturnNotPrivileged&&!requests);}
 else if(mode==8||mode==12||mode==16){assert(result==kIOReturnNotReady&&requests==1);}
 else assert(result==kIOReturnSuccess&&requests==1);
 service->release();service=nullptr;assert(freed==1);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-control-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture);subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in range(21):subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS production root control: 21 lifecycle, metadata, repeated-create, exact-command, administrator and one-shot request cases')
