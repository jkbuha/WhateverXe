"""Replay actual generated forcewake/reset wrappers; verify exact donor signatures."""
from pathlib import Path
import subprocess,sys,tempfile,re,struct,hashlib
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from prepare_rplp_reference import prepare
with tempfile.TemporaryDirectory(prefix='wex-recovery-') as td:
 out=Path(td);prepare(Path(sys.argv[1]),out,Path(sys.argv[2]))
 s=(out/'kern_gen11.cpp').read_text()
 assert 'gRc6FwDrop = 0;' in s and 'gRc6FwDrop = 1;' not in s
 common=r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <vector>
#include <array>
#include <cstdio>
using mach_vm_address_t=uintptr_t;
using thread_t=void*;thread_t current_thread(){return reinterpret_cast<void*>(0x1234);}
bool OSCompareAndSwap(uint32_t a,uint32_t b,volatile uint32_t*p){return __sync_bool_compare_and_swap(p,a,b);}
#define FunctionCast(name,p) reinterpret_cast<decltype(&name)>(p)
#define IOLog(...) ((void)0)
template<class T>T& field(void*p,size_t o){return *reinterpret_cast<T*>(static_cast<uint8_t*>(p)+o);}
struct IOService{void setProperty(const char*,uint64_t,unsigned){}};
'''
 force=s[s.index('static inline uint64_t xeFwOff('):s.index('static inline bool xeEngIdle(')]
 code=common+r'''
using IOSimpleLock=int;bool locked=false;int lock;
void IOSimpleLockLock(IOSimpleLock*p){assert(p==&lock&&!locked);locked=true;}
void IOSimpleLockUnlock(IOSimpleLock*p){assert(p==&lock&&locked);locked=false;}
uintptr_t gFwSymAddr=0,gOrgSafeForceWake=0;int gRc6FwOn=1;void*gFwAccel;
uint32_t gFwActSeq,gFwUnderflow;uint64_t gFwLastGetCaller[3],gFwUnderflowCaller;
struct Call{bool set;uint32_t domain;};std::vector<Call> calls;
struct Gen11{static void xeForceWakePub(void*,bool set,uint32_t d,uint8_t ctx){assert(locked&&ctx==1);calls.push_back({set,d});}};
unsigned originals=0;bool enabled=true;
bool wexRplReferenceForceWakeOwner(const void*){return enabled;}
void xeEv(uint32_t,uint32_t,uint32_t,uint32_t){}
void original(void*,bool,uint32_t){++originals;}
'''+force+r'''
int main(){
 alignas(8)uint8_t a[0x1c80]{};IOSimpleLock*ptr=&lock;
 field<void*>(a,0x1468)=&ptr;field<uint64_t>(a,0x1190)=0x800;
 gOrgSafeForceWake=reinterpret_cast<uintptr_t>(original);
 auto*c=reinterpret_cast<int32_t*>(a+0x1290);
 xeSafeForceWake(a,true,7);assert(c[0]==1&&c[1]==1&&c[2]==1&&calls.size()==1&&calls.back().set&&calls.back().domain==7);
 xeSafeForceWake(a,true,7);assert(c[0]==2&&calls.size()==1);
 xeSafeForceWake(a,false,7);assert(c[0]==1&&calls.size()==1);
 xeSafeForceWake(a,false,7);assert(c[0]==0&&c[1]==0&&c[2]==0&&calls.size()==2&&!calls.back().set);
 xeSafeForceWake(a,false,7);assert(c[0]==0&&calls.size()==2&&gFwUnderflow==3);
 c[0]=-1;xeSafeForceWake(a,true,1);assert(c[0]==1&&calls.back().set&&calls.back().domain==1);
 xeSafeForceWake(a,false,1);assert(c[0]==0&&!calls.back().set);
 c[0]=INT32_MAX;auto n=calls.size();xeSafeForceWake(a,true,1);assert(c[0]==INT32_MAX&&calls.size()==n);
 for(unsigned i=0;i<4;++i){originals=0;enabled=i!=0;gRc6FwOn=i==1?0:1;field<uint64_t>(a,0x1190)=i==2?0:i==3?0x1800:0x800;xeSafeForceWake(a,true,7);assert(originals==1);}
 assert(!locked);
}
'''
 (out/'force.cpp').write_text(code)
 reset=s[s.index('static mach_vm_address_t gOrgRge'):s.index('// Route installer (called from processKext)')]
 code=common+r'''
struct Callback{uint64_t mmioLength(){return 0x200000;}} callback;
struct NGreen{static Callback*callback;};Callback*NGreen::callback=&::callback;
bool owner=true;void*expectedAcc;unsigned originals=0,replays=0,holds=0,mode=0,ownIndex=0;
bool wexRplReferenceRuntimeOwner(const void*p){return owner&&p==expectedAcc;}
void xeEv(uint32_t,uint32_t,uint32_t,uint32_t){}
void sfw(void*p,bool set,uint32_t d){assert(p==expectedAcc&&d==7);if(set)++holds;else{assert(holds);--holds;}}
uint32_t getDef(void*,uint32_t r){return (r&0xfff)==0x29c?0x8000:0;}
void replay(void*,uint32_t type,void*ctx){assert(type==1-ownIndex&&!ctx);++replays;}
bool original(void*ring,void*){++originals;auto*mmio=field<uint8_t*>(ring,0x50);if(mode!=2){field<uint32_t>(mmio,ownIndex?0x2080:0x1a080)=0;field<uint32_t>(mmio,ownIndex?0x229c:0x1a29c)=0;}return mode!=3;}
'''+reset+r'''
int main(){for(mode=0;mode<16;++mode){
 alignas(8)uint8_t acc[0x1c80]{},ring[0x900]{},sched[0xd00]{},cs[0x40]{},desc[0x10]{},engine[0xb0]{};
 std::vector<uint64_t>memory(0x200000/8);auto*mmio=reinterpret_cast<uint8_t*>(memory.data());
 alignas(8)uint8_t data[2][0x24*4]{};uint64_t lists[2][3]{};
 uintptr_t vt[64]{};vt[0x190/8]=reinterpret_cast<uintptr_t>(replay);
 ownIndex=mode==1?1:0;unsigned sib=1-ownIndex;
 for(unsigned e=0;e<2;++e){lists[e][0]=lists[e][1]=2;lists[e][2]=reinterpret_cast<uintptr_t>(data[e]);
  field<uint32_t>(data[e],0)=e?0x1a080:0x2080;field<uint32_t>(data[e],0x24)=e?0x1a29c:0x229c;data[e][0x24+8]=1;
  field<uint32_t>(mmio,e?0x1a080:0x2080)=e?0x40005000:0x40004000;field<uint32_t>(mmio,e?0x1a29c:0x229c)=0x8000;
  gRecList[e]=reinterpret_cast<uintptr_t>(lists[e]);gRecGoodOk[e]=false;
 }
 field<void*>(ring,0x10)=acc;field<void*>(ring,0x50)=mmio;field<uint32_t>(ring,0x70)=2;ring[0x40]=ownIndex;
 field<uintptr_t>(ring,0x78)=gRecList[ownIndex];field<void*>(ring,0x8b8+8*ownIndex)=engine;
 if(mode==2)field<uint32_t>(engine,0xa4)=4;
 field<void*>(acc,0x1250)=sched;field<void*>(sched,0)=vt;field<void*>(sched,0xca0+8*sib)=cs;field<void*>(cs,0x28)=desc;desc[0]=sib;
 gOrgRge=reinterpret_cast<uintptr_t>(original);gRecSfw=reinterpret_cast<uintptr_t>(sfw);gRecGetDef=reinterpret_cast<uintptr_t>(getDef);gRecS5Proc=reinterpret_cast<uintptr_t>(replay);
 gRecOn=1;gRecExOn=1;gRecExBusy=0;gRecResets=gRecSibRestores=gRecSibReplays=gRecOwnFix=gRecSkipKick=0;originals=replays=holds=0;owner=mode!=4;expectedAcc=acc;
 if(mode==5)lists[sib][1]=1;
 if(mode==6)lists[sib][0]=65;
 if(mode==7)field<uint32_t>(data[sib],0x24)=0x200000;
 if(mode==8)field<uint32_t>(data[sib],0x24)=0x229d;
 if(mode==9)field<uint32_t>(ring,0x70)=6;
 if(mode==10)vt[0x190/8]=0;
 if(mode==11 || mode==12){field<uint32_t>(mmio,sib?0x1a080:0x2080)=0;if(mode==11){gRecGoodOk[sib]=true;gRecGood[sib][0]=0x40006000;gRecGood[sib][1]=0x8000;}}
 if(mode==13)gRecOn=0;if(mode==14)field<uint32_t>(mmio,sib?0x1a080:0x2080)=UINT32_MAX;if(mode==15)field<uint32_t>(mmio,ownIndex?0x1a080:0x2080)=0x123;
 auto result=xeRecResetGraphicsEngine(ring,nullptr);assert(originals==1&&result==(mode!=3)&&!holds&&!gRecExBusy&&!gRecPendAcc&&!gRecPendMmio&&!gRecPendThread);
 bool restores=mode==0||mode==1||mode==10||mode==11;
 assert(gRecSibRestores==(restores?1:0));assert(replays==(restores&&mode!=10?1:0));
 if(restores){assert(field<uint32_t>(mmio,sib?0x1a080:0x2080));assert(field<uint32_t>(mmio,sib?0x1a29c:0x229c)==0xffff8000);}
 if(mode==2)assert(gRecSkipKick==1);
}}
'''
 (out/'reset.cpp').write_text(code)
 for name in ['force','reset']:
  subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(out/(name+'.cpp')),'-o',str(out/name)],check=True)
  subprocess.run([str(out/name)],check=True)
 # Validate every imported signature against the exact signed donor; no kernel code execution.
 data=Path(sys.argv[3]).read_bytes();assert hashlib.sha256(data).hexdigest()=='6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64'
 syms={}
 for line in subprocess.check_output(['xcrun','nm','-n',sys.argv[3]],text=True).splitlines():
  cols=line.split()
  if len(cols)==3:
   try:syms[cols[2]]=int(cols[0],16)
   except ValueError:pass
 segs=[];pos=32
 for i in range(struct.unpack_from('<I',data,16)[0]):
  cmd,n=struct.unpack_from('<II',data,pos)
  if cmd==0x19:segs.append(struct.unpack_from('<QQQQ',data,pos+24))
  pos+=n
 def at(name,n):
  v=syms[name]
  for vm,vs,fo,fs in segs:
   if vm<=v and n<=fs and v-vm<=fs-n:return data[fo+v-vm:fo+v-vm+n]
  raise AssertionError(name)
 def pattern(name):
  m=re.search(r'static const uint8_t '+name+r'\[\] = \{(.*?)\};',s,re.S);assert m
  return bytes(int(x,16) for x in re.findall(r'0x[0-9a-fA-F]+',m[1]))
 sf='__ZN16IntelAccelerator13SafeForceWakeEbj';rg='__ZN20IGHardwareRingBuffer19resetGraphicsEngineEP17IGHardwareContext'
 for p in ['sCnt','sLck']:assert pattern(p) in at(sf,0x140)
 for p in ['sGdrst','sProc','sFlags']:assert pattern(p) in at(rg,0x320)[0x10:]
 assert at('__ZN12IGScheduler515processGPUResetE10IGHwCsTypeP17IGHardwareContext',4+len(pattern('sS5')))[4:]==pattern('sS5')
 assert pattern('sCs5') in at('__ZN26IGHardwareCommandStreamer515processGPUResetEv',0x40)
 for p in ['sRcu','sNp']:assert pattern(p) in at('__ZN16IntelAccelerator17initModeRegistersEv',0x120)
 print('PASS generated forcewake transitions/underflow/overflow/owner exclusions, 16 reset-domain cases, and seven exact donor signatures')
