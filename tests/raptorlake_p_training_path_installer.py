"""Validate every private-image entry before enabling training-path callbacks."""
from pathlib import Path
import subprocess,tempfile,sys,re,hashlib
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();body=s[s.index('bool wexRplReferenceTrainingPathHooks('):s.index('bool wexRplReferenceAGDCObservationHooks(')]
f=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
using mach_vm_address_t=uintptr_t;
bool selected=true,imageOK=true,routeOK=true;uint32_t livePortReady=1,trainingLeaseState[24]{};unsigned routes=0;
uintptr_t trainingAllocateAnyOriginal,trainingAllocateFixedOriginal,trainingFreeOriginal,trainingResourcesOriginal,trainingPGGet,trainingPGPutOriginal,trainingEndOriginal,trainingPortWriteOriginal,trainingConfiguredOriginal,trainingVoltageOriginal,trainingPatternOriginal;
void trainingConfiguredLanes(){}void trainingVoltage(){}void trainingPattern(){}void trainingGetPG(){}void trainingAllocateAny(){}void trainingAllocateFixed(){}void trainingFree(){}void trainingResources(){}void trainingPutPG(){}void trainingEnd(){}void trainingPortWrite(){}
bool checkKernelArgument(const char*n){assert(!strcmp(n,"-wex-rplp-training-path"));return selected;}
bool wexRplReferenceImage(void*,size_t,bool){return imageOK;}
const char* names[]{"__ZN21AppleIntelDisplayPath12allocatePathEv","__ZN21AppleIntelDisplayPath12allocatePathEj","__ZN21AppleIntelDisplayPath8freePathEv","__ZN21AppleIntelFramebuffer28allocateDisplayPathResourcesEjj","__ZN19AppleIntelPowerWell18disablePowerWellPGEj","__ZN24AppleIntelBaseController17doLinkTrainingEndEP23AGDCDPLinkTrainingEnd_t","__ZN14AppleIntelPort15writeRegister32Ejj","__ZN19AppleIntelPowerWell17enablePowerWellPGEj","__ZN17AppleIntelPortHAL29getConfiguredDklPhyLaneConfigEv","__ZN17AppleIntelPortHAL29setVoltageSwingAndPreEmphasisEjjP15PortDriveConfig","__ZN21AppleIntelDisplayPath22setLinkTrainingPatternE19LinkTrainingPattern"};
const uint32_t offsets[]{0xE914C,0xE9302,0xE9F2C,0x9C150,0x89428,0xA250E,0x2F49E,0x8A5FA,0x8FC74,0x90588,0xEE060};
const uint8_t prefixes[11][20]{PREFIXES};
struct KernelPatcher {
 struct RouteRequest{template<class T>RouteRequest(const char*,T,uintptr_t&o){o=0x1234;}};
 uintptr_t solveSymbol(unsigned,const char*n,uintptr_t a,size_t){for(unsigned x=0;x<11;x++)if(!strcmp(n,names[x]))return a+offsets[x];assert(false);return 0;}
 void clearError(){}bool routeMultiple(unsigned,RouteRequest*,unsigned n,uintptr_t,size_t){assert(n==11);++routes;return routeOK;}
};
BODY
int main(){
 std::vector<uint8_t> bytes(0xF0000);for(unsigned x=0;x<11;x++)memcpy(bytes.data()+offsets[x],prefixes[x],20);
 auto base=(uintptr_t)bytes.data();KernelPatcher p;assert(wexRplReferenceTrainingPathHooks(p,0,base,bytes.size())&&trainingLeaseState[1]&&routes==1);
 for(unsigned x=0;x<11;x++)for(unsigned b=0;b<20;b++){bytes[offsets[x]+b]^=1;assert(!wexRplReferenceTrainingPathHooks(p,0,base,bytes.size())&&!trainingLeaseState[1]&&routes==1);bytes[offsets[x]+b]^=1;}
 for(auto n:offsets)assert(!wexRplReferenceTrainingPathHooks(p,0,base,n+19)&&routes==1);
 livePortReady=0;assert(!wexRplReferenceTrainingPathHooks(p,0,base,bytes.size()));livePortReady=1;
 imageOK=false;assert(!wexRplReferenceTrainingPathHooks(p,0,base,bytes.size()));imageOK=true;
 routeOK=false;assert(!wexRplReferenceTrainingPathHooks(p,0,base,bytes.size())&&!trainingLeaseState[1]&&routes==2);
 selected=false;assert(wexRplReferenceTrainingPathHooks(p,0,base,0)&&routes==2);
}
'''.replace('BODY',body)
# Fixture entry bytes independently match the frozen donor below.
prefixes=re.search(r'const uint8_t prefixes\[11\]\[20\]\{(.*?)\};',body,re.S)[1]
f=f.replace('PREFIXES',prefixes)
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'t.cpp').write_text(f);subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'t.cpp'),'-o',str(p/'t')],check=True);subprocess.run([str(p/'t')],check=True)
if len(sys.argv)>1:
 p=Path(sys.argv[1]);p=p/'Contents/MacOS/AppleIntelTGLGraphicsFramebuffer' if p.is_dir() else p;b=p.read_bytes();assert hashlib.sha256(b).hexdigest()=='285ee7a9c3d6c9a9647013f44fb40f312b1cb42879974ef0542544cf049442b5'
 offsets=[int(x,16) for x in re.search(r'const uint32_t offsets\[\]\{(.*?)\};',body,re.S)[1].split(',')]
 rows=re.findall(r'\{([^{}]+)\}',prefixes)
 for offset,row in zip(offsets,rows):assert b[offset:offset+20]==bytes(int(x,16) for x in row.split(','))
print('PASS training-path installer: explicit opt-in, eleven exact donor entries, all byte mutations/truncations, physical-image gate and route failure')
