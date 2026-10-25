"""Execute the production automatic TC3 qualification worker and queue policy."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
body=(r/'WhateverXe/kern_raptorlake_p_initial_discovery_impl.hpp').read_text()
s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text()
# A cable-free diagnostic cannot spend the one-shot AUX transaction.
branch=s[s.index('if(sample.status==RaptorLakeP::ReferenceSnapshot::Complete'):s.index('// Controller lifetime, private image')]
assert branch.index('ReferenceTCAux::connected')<branch.index('__sync_bool_compare_and_swap(&auxProbeAttempted')
f=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include "kern_raptorlake_p_reference_tc_aux.hpp"
using thread_call_param_t=void*;
void *initialDiscoveryWork=(void*)1;
uint32_t initialDiscoveryBusy=0,initialDiscoveryAgain=0,initialDiscoveryState[8]{1};
uint32_t nativeRescanUsed=0,livePortReady=1,livePortPermission=0,auxProbeAttempted=0;
struct {uintptr_t owner=0;bool active=false;}livePortLease;
bool enabled=true,dependencies=true,observer=true,ready=true,shutting=false,bound=true,probeOK=true,rescanOK=true,reentrant=false;
bool checkKernelArgument(const char*s){return !strcmp(s,"-wex-rplp-tcss-auto")?enabled:dependencies;}
bool observerSelected(){return observer;}bool captureAllowed(){return ready;}
bool shutdown(){return shutting;}bool(*shutdownQuery)()=shutdown;
bool nativeAGDCCallbackBound(){return bound;}
struct Backend {
 uint32_t core=0xC03,panel=0x8A010102,tc=0x35,hpd=1U<<18;unsigned reads=0;
 uint32_t readReg32(uint32_t r){++reads;switch(r){case 0x45404:return core;case 0x60400:return panel;case 0x161508:return tc;case 0x44470:return hpd;default:assert(false);return ~0U;}}
}backend;
unsigned queued=0,probes=0,rescans=0;
void requestInitialDiscovery();
void thread_call_enter(void*p){assert(p==initialDiscoveryWork);++queued;}
void probeTCAux(){++probes;assert(RaptorLakeP::ReferenceTCAux::connected(backend.tc,backend.hpd));livePortPermission=probeOK;
 if(reentrant)requestInitialDiscovery();
}
namespace RaptorLakeP {namespace Reference {
bool rescanTC3(){
 assert(livePortPermission&&bound&&!livePortLease.owner&&!livePortLease.active);
 if(!rescanOK||nativeRescanUsed)return false;nativeRescanUsed=1;++rescans;return true;
}
}}
struct Target {unsigned publishes=0;void setProperty(const char*k,void*,unsigned size){assert(!strcmp(k,"WhateverXeRPLPInitialDiscovery")&&size==32);++publishes;}}object,*target=&object;
BODY
void run(){assert(initialDiscoveryBusy);initialDiscoveryWorker(nullptr,nullptr);assert(!initialDiscoveryBusy);}
void reset(){
 initialDiscoveryBusy=initialDiscoveryAgain=nativeRescanUsed=auxProbeAttempted=livePortPermission=0;
 memset(initialDiscoveryState,0,sizeof(initialDiscoveryState));initialDiscoveryState[0]=1;
 queued=probes=rescans=0;enabled=dependencies=observer=ready=bound=probeOK=rescanOK=true;
 shutting=reentrant=false;livePortReady=1;livePortLease={};backend={};initialDiscoveryWork=(void*)1;shutdownQuery=shutdown;
}
int main(){
 for(unsigned fault=0;fault<5;++fault){reset();if(fault==0)enabled=false;if(fault==1)dependencies=false;if(fault==2)observer=false;if(fault==3)initialDiscoveryWork=nullptr;if(fault==4)nativeRescanUsed=1;
  requestInitialDiscovery();assert(!queued&&!probes&&!initialDiscoveryBusy);
 }
 for(unsigned fault=0;fault<8;++fault){reset();if(fault==0)ready=false;if(fault==1)shutting=true;if(fault==2)bound=false;if(fault==3)livePortReady=0;if(fault==4)livePortLease.owner=1;if(fault==5)livePortLease.active=true;if(fault==6)shutdownQuery=nullptr;
  requestInitialDiscovery();if(fault==7)enabled=false;run();assert(!probes&&!rescans&&!auxProbeAttempted&&!backend.reads);
 }
 for(unsigned fault=0;fault<10;++fault){reset();
  if(fault==0)backend.core=~0U;if(fault==1)backend.core=0;if(fault==2)backend.panel=~0U;if(fault==3)backend.panel=0;
  if(fault==4)backend.tc=4;if(fault==5)backend.tc=~0U;if(fault==6)backend.tc=6;if(fault==7)backend.hpd=0;if(fault==8)backend.hpd=~0U;if(fault==9)backend.hpd|=4;
  requestInitialDiscovery();run();assert(!probes&&!rescans&&!auxProbeAttempted);
 }
 // Boot without a cable, bind AGDC, then first-insert after startup monitoring.
 reset();bound=false;requestInitialDiscovery();run();assert(initialDiscoveryState[4]==1);
 bound=true;backend.tc=4;backend.hpd=0;requestInitialDiscovery();run();assert(initialDiscoveryState[4]==3&&!auxProbeAttempted);
 backend.tc=0x35;backend.hpd=1U<<18;requestInitialDiscovery();run();assert(probes==1&&rescans==1&&nativeRescanUsed&&initialDiscoveryState[6]==1&&!initialDiscoveryState[4]);
 auto n=queued;requestInitialDiscovery();assert(queued==n&&!initialDiscoveryBusy);
 // Coalesce simultaneous hardware/startup/capture requests without re-probing.
 reset();requestInitialDiscovery();requestInitialDiscovery();reentrant=true;run();assert(queued==1&&probes==1&&rescans==1&&!initialDiscoveryAgain);
 // Failed preflight stays closed; it never turns into repeated AUX attempts.
 reset();probeOK=false;requestInitialDiscovery();run();assert(probes==1&&!rescans&&initialDiscoveryState[4]==4);
 requestInitialDiscovery();run();assert(probes==1&&!rescans);
 // A manual probe finishing later can notify the worker without a second probe.
 livePortPermission=1;requestInitialDiscovery();run();assert(probes==1&&rescans==1);
 reset();auxProbeAttempted=livePortPermission=1;requestInitialDiscovery();run();assert(!probes&&rescans==1);
 reset();rescanOK=false;requestInitialDiscovery();run();assert(probes==1&&!rescans&&initialDiscoveryState[4]==5);
 assert(!initialDiscoveryBusy);
}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='wex-initial-discovery-') as d:
 p=Path(d);(p/'t.cpp').write_text(f)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'t.cpp'),'-o',str(p/'t')],check=True)
 subprocess.run([str(p/'t')],check=True)
print('PASS production initial discovery: exact opt-in, native/AGDC readiness, absent cable preserves preflight, DP-alt isolation, one-shot probe/rescan and event coalescing')
