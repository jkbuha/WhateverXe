"""Replay generated lane/modeset hooks against their actual native return ABI."""
from pathlib import Path
import sys,subprocess,tempfile
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from prepare_rplp_reference import prepare
with tempfile.TemporaryDirectory() as d:
 p=Path(d);out=p/'generated';prepare(Path(sys.argv[1]),out,Path(sys.argv[2]))
 s=(out/'kern_gen11.cpp').read_text()
 lanes=s[s.index('uint8_t Gen11::computeLaneCount('):s.index('IOReturn Gen11::wrapICLReadAUX(')]
 modes=s[s.index('uint8_t Gen11::hwRegsNeedUpdate\n'):s.index('int Gen11::wrapHwSetupMemory(')]
 h=(out/'kern_gen11.hpp').read_text();assert 'static uint8_t computeLaneCount' in h and 'static uint32_t setupOptimalLaneCount' in h
 test=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#define SYSLOG(...) ((void)0)
#define FunctionCast(name,address) reinterpret_cast<decltype(&Gen11::name)>(address)
struct IODetailedTimingInformationV2{};
namespace AppleIntel {
struct AppleIntelBaseController{alignas(16)uint8_t bytes[0x800]{};};
struct AppleIntelFramebuffer{};struct AppleIntelDisplayPath{};
struct CRTCParams{uint32_t TRANS_DDI_FUNC_CTL,TRANS_CONF;};struct SCALERPARAMS{};
}
using namespace AppleIntel;
struct CB {bool isRealTGL=false,dmcIsAdlp=true;unsigned reads=0;uint32_t readReg32(unsigned r){++reads;return r==0x64000?6:0xC0000000;}void adlpDcExit(const char*){}}cb;
namespace NGreen {CB *callback=&cb;}
bool external=true,panel=false,full=false,trial=true;
bool wexRplReferenceExternalPort(const void*){return external;}
bool wexRplReferencePanelMode(const void*,const void*,const void*){return panel;}
bool wexRplReferenceExternalNeedsMode(const void*,const void*,const void*){return full;}
bool checkKernelArgument(const char*){return trial;}
template<class T>T& getMember(AppleIntelBaseController*p,unsigned o){return *reinterpret_cast<T*>(reinterpret_cast<uint8_t*>(p)+o);}
uint8_t nativeOK=1;uint32_t nativeError=0,expectedBpp=24,expectedRate=20;unsigned laneCalls=0,optimalCalls=0;
uint8_t nativeCompute(AppleIntelBaseController*,const IODetailedTimingInformationV2*,unsigned bpp,unsigned rate,unsigned *lanes){assert(bpp==expectedBpp&&rate==expectedRate);++laneCalls;*lanes=2;return nativeOK;}
uint32_t nativeOptimal(AppleIntelBaseController*,const IODetailedTimingInformationV2*,unsigned bpp){assert(bpp==expectedBpp);++optimalCalls;return nativeError;}
uint8_t nativeMode(AppleIntelBaseController*,AppleIntelFramebuffer*,AppleIntelDisplayPath*,CRTCParams*,const IODetailedTimingInformationV2*,SCALERPARAMS*){return nativeOK;}
struct Gen11 {
 static uint8_t computeLaneCount(AppleIntelBaseController*,const IODetailedTimingInformationV2*,unsigned,unsigned,unsigned*);
 static uint32_t setupOptimalLaneCount(AppleIntelBaseController*,const IODetailedTimingInformationV2*,unsigned);
 static uint8_t hwRegsNeedUpdate(AppleIntelBaseController*,AppleIntelFramebuffer*,AppleIntelDisplayPath*,CRTCParams*,const IODetailedTimingInformationV2*,SCALERPARAMS*);
 static uintptr_t ocomputeLaneCount,osetupOptimalLaneCount,ohwRegsNeedUpdate;
 static Gen11 *callback;
};Gen11 instance,*Gen11::callback=&instance;
uintptr_t Gen11::ocomputeLaneCount=(uintptr_t)nativeCompute,Gen11::osetupOptimalLaneCount=(uintptr_t)nativeOptimal,Gen11::ohwRegsNeedUpdate=(uintptr_t)nativeMode;
LANES
MODES
int main(){
 AppleIntelBaseController port;IODetailedTimingInformationV2 timing;unsigned n=0;
 for(unsigned ok:{0U,1U}){nativeOK=ok;assert(Gen11::computeLaneCount(&port,&timing,24,20,&n)==ok&&n==2);}
 assert(!cb.reads);getMember<unsigned>(&port,0x148)=0xABCD;
 for(unsigned error:{0U,0xE00002C7U}){nativeError=error;assert(Gen11::setupOptimalLaneCount(&port,&timing,24)==error);assert(getMember<unsigned>(&port,0x148)==0xABCD);}
 assert(!cb.reads&&laneCalls==2&&optimalCalls==2);
 external=false;nativeOK=1;assert(Gen11::computeLaneCount(&port,&timing,24,20,&n)==1&&n==4);
 nativeOK=0;AppleIntelFramebuffer head;AppleIntelDisplayPath path;SCALERPARAMS scaler;CRTCParams params{0xB2030002,0x80000024};
 assert(!Gen11::hwRegsNeedUpdate(&port,&head,&path,&params,&timing,&scaler));assert(params.TRANS_CONF==0x80000024&&params.TRANS_DDI_FUNC_CTL==0xB2030002);
 full=true;assert(Gen11::hwRegsNeedUpdate(&port,&head,&path,&params,&timing,&scaler)==1);full=false;
 panel=true;assert(!Gen11::hwRegsNeedUpdate(&port,&head,&path,&params,&timing,&scaler));assert(params.TRANS_CONF==0xC0000000&&params.TRANS_DDI_FUNC_CTL==0xB2020002);
}
'''.replace('LANES',lanes).replace('MODES',modes).replace('#include <cstring>','#include <cstring>\n#include <initializer_list>')
 (p/'test.cpp').write_text(test)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS generated native lane ABI: bool/IOReturn preserved, external panel bypass, scoped pipe-A parameters, disabled external full modeset')
