#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include "kern_tgl_native_fia.hpp"
#include "kern_right_alt.hpp"
constexpr uint32_t kIOReturnNotReady=0xe00002d8;
struct IORecursiveLock{unsigned depth{};};
void IORecursiveLockLock(IORecursiveLock*p){assert(p);++p->depth;}
void IORecursiveLockUnlock(IORecursiveLock*p){assert(p&&p->depth);--p->depth;}
void *current_thread(){static char token;return &token;}
#define FunctionCast(name,address) reinterpret_cast<decltype(&name)>(address)
struct IORegistryEntry{static IORegistryEntry *getRegistryRoot(){static IORegistryEntry r;return &r;}template<class...A>void setProperty(A...){};};
void rightPortRecord(void*,unsigned,uint32_t,uint32_t=0,uint32_t=0,uint32_t=0){}
void tglAUXRecord(void*,unsigned,bool,uint32_t,uint32_t,uint32_t){}
void tglConnectionRecord(void*,unsigned,bool,uint32_t,uint32_t,void*){}
struct XE{
 uint32_t physical=0x9a498086,revision=1;bool ready=true,cold=true,powered=true;uint32_t live=0x103c,pin=4;
 uint32_t tglTCAutoRegistered{},tglModularProbed{},tglNativeFiaReads{},tglNativeFiaWrites{};
 bool tglModeResultReady=true,tglNativeFiaReady=true;IORecursiveLock lock;IORecursiveLock *tglModeTransitionLock=&lock;
 TGLNativeFiaContext tglNativeFiaContext;uintptr_t orgTGLSetPortMode{};uint32_t tglModeResultCorrections{},tglModeSerializedCalls{};
 bool tglRightAltEligible(uint32_t d)const{return physical==0x9a498086&&revision==1&&d==5&&ready;}
 bool tglTCModeAllowed(uint32_t d,uint32_t m)const{return tglRightAltEligible(d)?(m==0||m==3):(m==0||m==4);}
 bool tglRegisterLegacyHPD(uint32_t,uint32_t,uintptr_t){return false;}
 bool tglLegacyTCEligible(uint32_t d)const{return d==4;}
 bool tglDPMode(uint32_t d,uint32_t m)const{return m==4||(m==3&&tglRightAltEligible(d));}
 bool tglDPTransportEligible(uint32_t d)const{return tglLegacyTCEligible(d)||tglRightAltEligible(d);}
 static void tglRecordNativeModeBinding(void*,uint32_t,uint32_t){}
 static void tglRecordStartOrder(unsigned,void*,unsigned=9,uint64_t=0,uint64_t=0,uint64_t=0){}
 static uint32_t tglRightAltProbe(void*);
 static uint32_t wrapTGLSetPortMode(void*,uint32_t);
};
XE instance,*callbackXE=&instance;unsigned nativeCalls{},probeCalls{};
uint32_t XE::tglRightAltProbe(void *p){++probeCalls;assert(instance.lock.depth==1);auto mode=*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(p)+0x550);return RightAlt::accept(instance.physical,5,instance.cold,instance.powered,0x1010,instance.live,1,instance.pin,0x80,mode)?3:0;}
uint32_t native(void *p,uint32_t requested){++nativeCalls;assert(instance.lock.depth==1);auto d=(*reinterpret_cast<uint32_t**>(static_cast<uint8_t*>(p)+0x548))[0];auto &mode=*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(p)+0x550);
 if(requested==3){bool translated=instance.tglNativeFiaContext.matches(reinterpret_cast<uintptr_t>(current_thread()),p);mode=translated?3:0;}else mode=requested;
 return 0; // donor's false-success path when the coordinate scope is absent
}
// PRODUCTION_WRAPPER
int main(){alignas(8)uint8_t p[0x600]={};uint32_t config[8]={5};auto cfg=config;memcpy(p+0x548,&cfg,8);auto &mode=*reinterpret_cast<uint32_t*>(p+0x550);
 auto reset=[&](){instance=XE{};instance.tglModeTransitionLock=&instance.lock;instance.orgTGLSetPortMode=reinterpret_cast<uintptr_t>(native);mode=0;nativeCalls=probeCalls=0;};
 reset(); // Negative control: exact donor decision with no prior probe/scope.
 instance.lock.depth=1;native(p,3);instance.lock.depth=0;assert(mode==0);nativeCalls=0;
 assert(XE::wrapTGLSetPortMode(p,3)==0&&mode==3&&nativeCalls==1&&probeCalls==1);assert(instance.tglModularProbed==(1U<<5)&&instance.lock.depth==0&&!instance.tglNativeFiaContext.owner);
 // Reconnect retains qualification and does not claim any other DDI.
 assert(XE::wrapTGLSetPortMode(p,0)==0&&mode==0);assert(XE::wrapTGLSetPortMode(p,3)==0&&mode==3&&instance.tglModularProbed==(1U<<5));
 for(unsigned bad=0;bad<8;++bad){reset();switch(bad){case 0:instance.powered=false;break;case 1:instance.cold=false;break;case 2:instance.pin=3;break;case 3:instance.live=0;break;case 4:instance.tglModeTransitionLock=nullptr;break;case 5:instance.tglNativeFiaReady=false;break;case 6:instance.physical=0xa7a08086;break;case 7:instance.revision=4;break;}
 assert(XE::wrapTGLSetPortMode(p,3)==kIOReturnNotReady);assert(!nativeCalls&&!instance.tglModularProbed&&!instance.lock.depth&&mode==0);}
 reset();config[0]=4;assert(XE::wrapTGLSetPortMode(p,4)==0&&mode==4&&!probeCalls&&!instance.tglModularProbed);puts("PASS: production hotplug mode wrapper reproduces missing-probe failure, qualifies first insertion and reconnect, rejects bad power/pin/identity/lock, and preserves legacy mode4");
}
