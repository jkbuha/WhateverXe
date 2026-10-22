"""Run the actual generated barrier preflight and production owner gate on CPU."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'tools/prepare_rplp_reference.py').read_text();body=s[s.index(' // WEX native barrier candidate begin.'):s.index(' // WEX native barrier candidate end.')]
fixture=r'''#include <cassert>
#include <cstdint>
#include <cstring>
#include <array>
using Fn=uint8_t(*)(void*,void*,void*,void*,uint16_t,const uint16_t*);
#define FunctionCast(name,p) reinterpret_cast<Fn>(p)
template<class T>T&getMember(void*p,unsigned o){return *reinterpret_cast<T*>(static_cast<uint8_t*>(p)+o);}
struct Callback{uintptr_t obarrierSubmission;};Callback cb,*callback=&cb;
alignas(8)uint8_t q[0x840],acc[0x400],desc[0x60],task[0x200],sys[0x200],context[8][0xC0],fifo[8][0x180],segment[0x80],ev[0x40],descInner[0x40];
unsigned mode,originalCalls,records,lastReason,lastValue;bool enabled;
bool wexRplReferenceBarrierOwner(const void*p){return enabled&&p==acc;}
void wexRplReferenceBarrierResult(unsigned r,unsigned v){++records;lastReason=r;lastValue=v;}
void*getter(void*t,unsigned i){unsigned n=(t==sys?4:0)+i;if(mode==12&&n==1)return nullptr;return context[n];}
void*getBlit2DContext(void*t,bool force){assert(force);return getter(t,0);}
void*getBlit3DContext(void*t,bool force){assert(force);return getter(t,1);}
void*getDepthResolveContext(void*t,bool force){assert(force);return getter(t,2);}
void*getColorResolveContext(void*t,bool force){assert(force);return getter(t,3);}
uint8_t original(void*queue,void*owner,void*cmd,void*event,uint16_t count,const uint16_t*list){
 assert(queue==q&&owner==acc&&cmd==desc&&event==ev);assert(!count||list);++originalCalls;return mode==17?0:1;
}
uint8_t trial(void*queue,void*accelerator,void*cmdDesc,void*event,uint16_t count,const uint16_t*list){
BODY
 return 42; // legacy path marker: no native barrier was submitted
}
int main(){for(mode=0;mode<21;++mode){
 memset(q,0,sizeof(q));memset(acc,0,sizeof(acc));memset(desc,0,sizeof(desc));memset(context,0,sizeof(context));
 originalCalls=records=lastReason=lastValue=0;enabled=mode!=0;
 cb.obarrierSubmission=reinterpret_cast<uintptr_t>(original);
 getMember<void*>(q,0x5C0)=acc;getMember<int32_t>(q,0x830)=0;getMember<void*>(q,0x7B0)=segment;
 getMember<void*>(desc,8)=task;getMember<void*>(desc,0x58)=descInner;getMember<void*>(acc,0x150)=sys;
 for(unsigned i=0;i<8;++i)getMember<void*>(context[i],0xB8)=fifo[i];
 void*queue=q,*cmd=desc,*event=ev;uint16_t entry=1,count=1;const uint16_t*list=&entry;
 if(mode==2)queue=nullptr;if(mode==3)cmd=nullptr;if(mode==4)event=nullptr;if(mode==5)list=nullptr;
 if(mode==6)getMember<void*>(q,0x5C0)=sys;
 if(mode==7)getMember<int32_t>(q,0x830)=-2;if(mode==8)getMember<int32_t>(q,0x830)=16;
 if(mode==9)getMember<void*>(desc,0x58)=nullptr;if(mode==10)getMember<void*>(desc,8)=nullptr;
 if(mode==11)getMember<void*>(q,0x7B0)=nullptr;if(mode==13)getMember<void*>(context[3],0xB8)=nullptr;
 if(mode==14)getMember<void*>(acc,0x150)=nullptr;if(mode==15)getMember<void*>(context[5],0xB8)=nullptr;
 if(mode==16)cb.obarrierSubmission=0;if(mode==18)getMember<int32_t>(q,0x830)=-1;
 if(mode==19){count=0;list=nullptr;}
 if(mode==20){getMember<int32_t>(q,0x830)=15;for(unsigned i=0;i<16;i++)getMember<void*>(q,0x7B0+8*i)=segment;}
 auto result=trial(queue,acc,cmd,event,count,list);
 if(mode==0){assert(result==42&&!records&&!originalCalls);continue;}
 if(mode>=2&&mode<=16){const unsigned reasons[]={1,1,1,1,2,3,3,3,4,5,7,9,10,12,13};assert(result==42&&records==1&&!originalCalls&&lastReason==reasons[mode-2]);continue;}
 assert(originalCalls==1&&records==1&&!lastReason&&result==(mode==17?0:1)&&lastValue==result);
}}
'''.replace('BODY',body)
s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text();start=s.index('bool wexRplReferenceBarrierOwner(');end=s.index('bool wexRplReferenceQueue(',start)
owner=r'''#include <cassert>
#include <cstdint>
#include <cstring>
struct Owner{bool dead;bool isInactive(){return dead;}} obj,other;Owner*nativeOwner=&obj;
uint32_t state[9]{1},barrierState[8]{1};bool argument,panel;bool shuttingDown;int query(){return shuttingDown;} int(*shutdownQuery)()=query;
bool checkKernelArgument(const char*s){assert(!strcmp(s,"-wex-rplp-native-barriers"));return argument;}
bool wexRplReferencePanel(){return panel;}
BODY
int main(){for(unsigned bits=0;bits<256;++bits){
 argument=bits&1;panel=bits&2;state[1]=bits&4;state[4]=bits&8;state[6]=bits&16;state[7]=bits&32;obj.dead=bits&64;shuttingDown=bits&128;
 assert(wexRplReferenceBarrierOwner(&obj)==(bits==31));assert(!wexRplReferenceBarrierOwner(&other));
 }shutdownQuery=nullptr;assert(!wexRplReferenceBarrierOwner(&obj));shutdownQuery=query;shuttingDown=false;nativeOwner=nullptr;state[7]=0;obj.dead=false;assert(!wexRplReferenceBarrierOwner(&obj));
 wexRplReferenceBarrierResult(0,1);wexRplReferenceBarrierResult(6,0);wexRplReferenceBarrierResult(0,0);
 assert(barrierState[1]==3&&barrierState[2]==2&&barrierState[3]==1&&barrierState[6]==1&&barrierState[7]==6);
}
'''.replace('BODY',s[start:end])
with tempfile.TemporaryDirectory(prefix='rplp-barrier-') as d:
 for name,code in [('preflight',fixture),('owner',owner)]:
  p=Path(d);(p/(name+'.cpp')).write_text(code)
  subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/(name+'.cpp')),'-o',str(p/name)],check=True)
  subprocess.run([str(p/name)],check=True)
print('PASS production barrier preflight21 cases, owner gate256 cases, telemetry and original return propagation')
