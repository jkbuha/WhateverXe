"""Production factory-continuation gate, owned backing and unwind lifetime."""
from pathlib import Path
import ast,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
tree=ast.parse((r/'tests/raptorlake_p_native_pools_adapter.py').read_text())
node=next(n for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='fixture' for t in n.targets))
prefix=node.value.func.value.value.split('BODY')[0]
prefix=prefix.replace('if(!strcmp(s,"-wex-rplp-native-pools"))return mode!=28;', 'if(!strcmp(s,"-wex-rplp-native-chain"))return mode!=24;if(!strcmp(s,"-wex-rplp-native-pools"))return mode!=25;')
prefix=prefix.replace('"WhateverXeRPLPNativePools"','"WhateverXeRPLPNativeChain"')
prefix=prefix[:prefix.index('struct KernelPatcher {')]+r'''
struct KernelPatcher{
 struct RouteRequest{const char *name;uintptr_t replacement;uintptr_t &original;template<class F>RouteRequest(const char *n,F f,uintptr_t &o):name(n),replacement(reinterpret_cast<uintptr_t>(f)),original(o){}};
 template<class T>T solveSymbol(size_t,const char *,uintptr_t b,size_t){return T(b+0xed9e + (mode==1));}
 bool routeMultiple(size_t,RouteRequest *,unsigned n,uintptr_t,size_t){assert(n==1);return mode!=3;}
 void clearError(){}
};
'''
prefix+=r'''
namespace RaptorLakeP{namespace PagePool{void run(IOService *,IOPCIDevice *,bool ok){assert(ok);}bool passed(){return mode!=32;}}namespace PageArray{bool integrated(){return mode>=30;}bool populate(IOService *,IOPCIDevice *,OSObject *,bool ok,uintptr_t p){assert(ok&&p==0x1234);return mode!=31;}}}
'''
prefix+=r'''
namespace RaptorLakeP{namespace NativeFactory{bool enabled(){return mode>=33;}bool arm(OSObject *,IOService *,IOPCIDevice *,bool ok){assert(ok);return mode!=34;}}}
'''
s=(r/'WhateverXe/kern_raptorlake_p_native_chain.cpp').read_text();body=s[s.index('namespace RaptorLakeP {namespace NativeChain {'):]
tail=r'''
using namespace RaptorLakeP::NativeChain;
template<class T>void put(void *p,unsigned off,T v){memcpy((uint8_t *)p+off,&v,sizeof(v));}
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);
 std::vector<uint8_t> image(0x14000);memcpy(image.data()+0xed9e,"\x55\x48\x89\xe5\x41\x57\x41\x56",8);if(mode==2)image[0xed9e]^=1;
 KernelPatcher p;bool ready=prepare(p,7,reinterpret_cast<uintptr_t>(image.data()),image.size());if(mode>=1&&mode<=3)assert(!ready);else assert(ready);originalPagePool=0x1234;
 IOPCIDevice pci;IOService object;OSObject manager;if(mode==23)IOMapper::gSystem=(void *)1;if(mode==26)pci.profile=XELP::identify(0xa7a08086,5);
 uint64_t info[5]{};bool started=begin(&object,&pci,info);
 if(mode==24){assert(!record[Requested]&&!started);return 0;}
 if((mode>=1&&mode<=5)||(mode>=20&&mode<=26)){assert(!started&&record[Failure]&&!alive);return 0;}
 assert(started&&alive&&info[0]==1&&info[1]==0x123000&&info[2]==1&&!info[3]&&!info[4]);
 const bool proceed=allowGTT(mode!=6);assert(proceed==(mode!=6));
 put(&manager,0x10,&object);put(&manager,0x78,mode==7?nullptr:static_cast<OSObject *>(&pool));put(&manager,0x80,mode==13?nullptr:static_cast<OSObject *>(&fence));
 put(&pool,0x10,mode==8?nullptr:&object);put(&pool,0x18,&descriptor);put(&pool,0x20,mode==12?nullptr:&allocator);
 for(unsigned i=0;i<5;++i)put(&pool,0x28+8*i,info[i]);put(&pool,0x60,uint64_t(4096));put(&pool,0x78,reinterpret_cast<uintptr_t>(&pool)+0x70);
 put(&fence,0x10,mode==15?nullptr:&object);put(&fence,0x40,uint64_t(mode==14?0x1fffffff8ULL:0xfffffff8));
 if(proceed&&mode!=18)assert(stop(&manager)==(mode==33));
 if(mode!=16){put(&manager,0x78,uintptr_t(0));put(&manager,0x80,uintptr_t(0));put(&manager,0x98,uintptr_t(0));}
 if(mode!=19)observeCleanup(&manager);
 assert(!pagePoolInitializer());originalPagePool=0x1234;bool success=finish(mode!=17);assert(pagePoolInitializer()==(success?0x1234U:0));
 if(mode==0||mode==30||mode==33||mode==34)assert(success&&record[PoolValid]&&record[FenceValid]&&record[StopEntered]==1&&record[CleanupCleared]&&record[BufferReleased]&&!record[Failure]);
 else assert(!success&&record[Failure]);
 if(mode==16||mode==17||mode==19)assert(record[Retained]&&alive&&!bufferFrees);else assert(!alive&&bufferFrees==1&&!record[Retained]);
 assert(!begin(&object,&pci,info));
}
'''
with tempfile.TemporaryDirectory(prefix='rplp-native-chain-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(prefix+body+tail)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for mode in list(range(27))+[30,31,32,33,34]:subprocess.run([str(p/'test'),str(mode)],check=True)
print('PASS native factory continuation: exact stop gate, owned backing, verified GGTT prerequisite, pool/fence validation, unwind proof and retained RAM on uncertain cleanup')
