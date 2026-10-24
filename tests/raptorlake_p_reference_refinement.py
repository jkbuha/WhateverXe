"""Replay imported CCS repair and IOSurface copy routing without GPU writes."""
from pathlib import Path
import subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from prepare_rplp_reference import prepare
with tempfile.TemporaryDirectory(prefix='wex-refinement-') as td:
 p=Path(td);prepare(Path(sys.argv[1]),p,Path(sys.argv[2]));s=(p/'kern_gen11.cpp').read_text()
 common=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include "kern_raptorlake_p_blit_pitch.hpp"
using mach_vm_address_t=uintptr_t;using thread_t=void*;
thread_t thread=reinterpret_cast<void*>(0x123);thread_t current_thread(){return thread;}
#define FunctionCast(name,p) reinterpret_cast<decltype(&name)>(p)
#define IOLog(...) ((void)0)
template<class T>T& field(void*p,size_t o){return *reinterpret_cast<T*>(static_cast<uint8_t*>(p)+o);}
void *owner=nullptr;bool owned=true,requested=true;
bool wexRplReferenceRuntimeOwner(const void*p){return owned&&p==owner;}
bool wexRplReferenceBlitPitchRequested(){return requested;}
'''
 extras=s[s.index('static const uint32_t kXeRecEx[5]'):s.index('static bool xeRecResetGraphicsEngine(')]
 code=common+r'''
struct Callback{uint64_t length=0x200000;uint64_t mmioLength(){return length;}} cb;
struct NGreen{static Callback*callback;};Callback*NGreen::callback=&cb;
unsigned reads=0,writes=0,forwarded=0;bool repairedBeforeForward=false;uint8_t*live;
uint32_t xeRecRd(volatile uint8_t*m,uint32_t o){assert(o%4==0&&o+4<=cb.length);++reads;return *reinterpret_cast<volatile uint32_t*>(m+o);}
void xeRecWr(volatile uint8_t*m,uint32_t o,uint32_t v){assert(o%4==0&&o+4<=cb.length);++writes;
 auto*d=reinterpret_cast<volatile uint32_t*>(m+o);if(o==0x14800||o==0xce90){uint32_t mask=v>>16;*d=(*d&~mask)|(v&mask);}else *d=v;}
void original(void*,uint32_t,void*){++forwarded;repairedBeforeForward=(field<uint32_t>(live,0x14800)&1)!=0;}
'''+extras+r'''
int main(){
 alignas(8)uint8_t acc[0x1300]{},sched[16]{};std::vector<uint64_t> memory(0x200000/8);live=reinterpret_cast<uint8_t*>(memory.data());owner=acc;
 auto setup=[&](){for(auto&v:memory)v=0;owned=true;cb.length=0x200000;reads=writes=forwarded=0;gRecExOn=1;gRecExSnapOk=gRecExGoodOk=false;gRecExFix=0;field<uint64_t>(acc,0x1190)=1ULL<<33;field<void*>(acc,0x1250)=sched;
  uint32_t values[]{1,0x5678,0xb134,0xa5a5,0x5a5a};for(unsigned i=0;i<5;++i)field<uint32_t>(live,kXeRecEx[i])=values[i];};
 setup();xeRecExSnapshot(acc,live);assert(gRecExSnapOk&&gRecExGoodOk);
 for(auto o:kXeRecEx)field<uint32_t>(live,o)=0;
 assert(xeRecExFix(acc,live)==31&&writes==5);assert(field<uint32_t>(live,0x14800)==1&&field<uint32_t>(live,0x1a4d0)==0xb134);
 assert(xeRecExFix(acc,live)==0&&writes==5);
 setup();xeRecExSnapshot(acc,live);field<uint32_t>(live,0x14800)=0xa0000002;field<uint32_t>(live,0xce90)=0xbaad0000;assert(xeRecExFix(acc,live)==3);assert(field<uint32_t>(live,0x14800)==0xa0000003&&field<uint32_t>(live,0xce90)==0xbaad5678);
 setup();field<uint64_t>(acc,0x1190)=0;field<uint32_t>(live,0x14800)=0;xeRecExSnapshot(acc,live);for(auto o:kXeRecEx)field<uint32_t>(live,o)=0;assert(xeRecExFix(acc,live)==26);assert(!field<uint32_t>(live,0x1a4d0));
 setup();field<uint32_t>(live,0x14800)=UINT32_MAX;xeRecExSnapshot(acc,live);assert(!gRecExSnapOk&&!gRecExGoodOk);assert(xeRecExFix(acc,live)==0&&!writes);
 for(unsigned i=0;i<5;++i){setup();field<uint32_t>(live,kXeRecEx[i])=UINT32_MAX;xeRecExSnapshot(acc,live);assert(!gRecExSnapOk&&!gRecExGoodOk);}
 setup();owned=false;xeRecExSnapshot(acc,live);assert(xeRecExFix(acc,live)==0&&!reads&&!writes);
 setup();cb.length=0x1a4d3;xeRecExSnapshot(acc,live);assert(xeRecExFix(acc,live)==0&&!reads&&!writes);
 for(unsigned mode=0;mode<6;++mode){setup();xeRecExSnapshot(acc,live);field<uint32_t>(live,0x14800)=0;reads=writes=0;
  gRecPendAcc=mode==1?nullptr:acc;gRecPendMmio=live;gRecPendThread=mode==2?nullptr:current_thread();gOrgS5Proc=reinterpret_cast<uintptr_t>(original);if(mode==3)owned=false;
  xeRecProc(mode==4?nullptr:sched,mode==5?2:0,nullptr);assert(forwarded==1);assert((writes>0)==(mode==0));assert(repairedBeforeForward==(mode==0));}
}
'''
 (p/'extras.cpp').write_text(code)
 route=s[s.index('static void xeBlitFixNote('):s.index('// v2 = BLITALIGN')]
 code=common+r'''
uint32_t gBfPitch=0,gBfBase=0,gBf3D=0,gBfLast=0;void*volatile gBfAcc=nullptr;void*volatile gBfTc=reinterpret_cast<void*>(1);unsigned queued=0;
uint32_t OSIncrementAtomic(volatile uint32_t*p){return __sync_fetch_and_add(p,1);}
void thread_call_enter(void*p){assert(p==gBfTc);++queued;}
'''+route+r'''
int main(){
 alignas(8)uint8_t storage[0xb0+8]{},acc[8]{};owner=acc;
 for(unsigned offset=0;offset<8;++offset){auto*p=storage+offset;auto put=[&](size_t o,uint32_t v){memcpy(p+o,&v,4);};
  memset(storage,0,sizeof(storage));p[0xa0]=1;put(0,0x1000);put(0x48,0x2000);put(8,656);put(0x50,656);owned=requested=true;
  assert(xeBlitFixRoute(p,642ULL*512,acc));assert(gBfPitch>0);
  owned=false;auto n=queued;assert(!xeBlitFixRoute(p,642ULL*512,acc)&&queued==n);
  owned=true;requested=false;assert(!xeBlitFixRoute(p,642ULL*512,acc));requested=true;
  assert(xeBlitFixRoute(p,642ULL*128,acc));put(8,640);put(0x50,640);assert(!xeBlitFixRoute(p,642ULL*512,acc));
  put(0x2c,1);assert(xeBlitFixRoute(p,642ULL*512,acc));put(0x2c,0);
  p[0xa0]=16;assert(!xeBlitFixRoute(p,UINT64_MAX,acc));
 }
 assert(!xeBlitFixRoute(nullptr,100,acc));
}
'''
 (p/'pitch.cpp').write_text(code)
 for name in ['extras','pitch']:
  subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/(name+'.cpp')),'-o',str(p/name)],check=True)
  subprocess.run([str(p/name)],check=True)
 print('PASS production CCS masked restoration/idempotence/failed-read/owner/thread/scheduler cases; production copy routing, legacy fallback and eight descriptor alignments')
