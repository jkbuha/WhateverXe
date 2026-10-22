"""Private CPU replay of exact donor dummy allocation and native cleanup."""
import ctypes as C,hashlib,mmap,struct,json,sys,tempfile,subprocess,platform
from pathlib import Path
assert platform.machine().lower() in ('x86_64','amd64')
d=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(d).hexdigest() in ('6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64','bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0')
m=mmap.mmap(-1,0x200000,prot=7);m[:len(d)]=d;base=C.addressof(C.c_char.from_buffer(m));keep=[]
def redirect(site,fn):
 assert d[site:site+5]==b'\xe8\0\0\0\0'
 stub=0x180000+len(keep)*32;keep.append(fn)
 m[stub:stub+14]=bytes.fromhex('ff2500000000')+struct.pack('<Q',C.cast(fn,C.c_void_p).value)
 m[site+1:site+5]=struct.pack('<i',stub-site-5)
pageSize=C.c_uint64(4096);task=C.c_void_p(0)
for instruction,after,pointee in [(0xf248,0xf24f,C.addressof(pageSize)),(0xf2ac,0xf2b3,C.addressof(task))]:
 cell=after+struct.unpack_from('<i',d,instruction+3)[0];struct.pack_into('<Q',m,cell,pointee)
state={};errors=[]
def check(ok,message):
 if not ok:errors.append(message)
@C.CFUNCTYPE(None,C.c_void_p)
def bufferRelease(_):state['events'].append('buffer-release');state['refs']-=1
@C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint)
def prepare(_,direction):state['events'].append('prepare');return 1 if state['mode']=='prepare-failure' else 0
@C.CFUNCTYPE(C.c_int,C.c_void_p,C.c_uint)
def complete(_,direction):
 state['events'].append('complete')
 if state['refs']<=0:state['after_free']+=1
 return 0
@C.CFUNCTYPE(C.c_uint64,C.c_void_p,C.c_uint64,C.c_void_p,C.c_uint)
def physical(_,offset,length,options):return 0x12345000
@C.CFUNCTYPE(C.c_void_p,C.c_void_p)
def virtual(_):return C.addressof(page)
@C.CFUNCTYPE(C.c_uint64,C.c_void_p)
def length(_):return 4096
@C.CFUNCTYPE(None,C.c_void_p)
def mapRelease(_):state['events'].append('map-release')
bvt=C.create_string_buffer(0x220);mvt=C.create_string_buffer(0x150)
for off,cb in [(0x28,bufferRelease),(0x1f0,prepare),(0x1f8,complete),(0x138,physical)]:struct.pack_into('<Q',bvt,off,C.cast(cb,C.c_void_p).value)
for off,cb in [(0x28,mapRelease),(0x118,virtual),(0x128,length)]:struct.pack_into('<Q',mvt,off,C.cast(cb,C.c_void_p).value)
buffer=C.create_string_buffer(16);mapping=C.create_string_buffer(16);page=C.create_string_buffer(4096)
struct.pack_into('<Q',buffer,0,C.addressof(bvt));struct.pack_into('<Q',mapping,0,C.addressof(mvt))
@C.CFUNCTYPE(C.c_void_p,C.c_void_p,C.c_uint,C.c_uint64,C.c_uint64)
def allocate(task,options,size,alignment):
 check(options==0x323 and size==4096 and alignment==1,'allocation arguments')
 state['events'].append('allocate')
 if state['mode']=='allocation-failure':return None
 state['refs']=1;return C.addressof(buffer)
@C.CFUNCTYPE(C.c_void_p,C.c_void_p,C.c_void_p,C.c_uint64,C.c_uint,C.c_uint64,C.c_uint64)
def mapInTask(*args):
 state['events'].append('map')
 return None if state['mode']=='mapping-failure' else C.addressof(mapping)
redirect(0xf260,allocate);redirect(0xf2c3,mapInTask)
run=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_void_p,C.c_uint,C.c_uint)(base+0xf22e)
cleanup=C.CFUNCTYPE(None,C.c_void_p)(base+0xef1e)
source=(Path(__file__).resolve().parents[1]/'WhateverXe/kern_raptorlake_p_native_memory.cpp').read_text()
enum=source[source.index('enum Field'):source.index('static uint64_t record')]
body=source[source.index('static bool allocation('):source.index('static void dummyRelease(')]
fixture='#include <cstdint>\n#include <cstring>\n'+enum+'\nstatic uint64_t record[Fields];static void *owner,*pageManager;static uintptr_t originalAllocation;\nstatic bool pages(){return true;}\ntemplate<class T> T field(void *p,unsigned off){T value;memcpy(&value,static_cast<uint8_t *>(p)+off,sizeof(value));return value;}\n'+body+'\nextern "C" void setup(void *m,uintptr_t f){owner=pageManager=m;originalAllocation=f;}\nextern "C" bool run(void **p,uint64_t *a,uint32_t options,uint32_t kind){return allocation(p,a,options,kind);}\n'
tmp=tempfile.TemporaryDirectory(prefix='rplp-pages-native-');tp=Path(tmp.name);(tp/'wrapper.cpp').write_text(fixture)
subprocess.run(['clang++','-std=c++17','-dynamiclib','-O2',str(tp/'wrapper.cpp'),'-o',str(tp/'wrapper.dylib')],check=True)
wrapper=C.CDLL(str(tp/'wrapper.dylib'));wrapper.setup.argtypes=[C.c_void_p,C.c_uint64];wrapper.run.argtypes=[C.c_void_p,C.c_void_p,C.c_uint,C.c_uint];wrapper.run.restype=C.c_bool
raw=run
results=[]
for protected,mode,kind in [(False,'prepare-failure',0)]+[(True,m,k) for m,k in [('success',0),('success',4096),('allocation-failure',0),('prepare-failure',0),('mapping-failure',0)]]:
 state.clear();state.update(mode=mode,events=[],refs=0,after_free=0)
 manager=C.create_string_buffer(0x200);descriptor=C.c_void_p.from_buffer(manager,0xe8 if kind else 0xe0);address=C.c_uint64.from_buffer(manager,0xf8 if kind else 0xf0)
 wrapper.setup(C.addressof(manager),base+0xf22e)
 ok=(wrapper.run if protected else raw)(C.byref(descriptor),C.byref(address),0,kind)
 dangling=bool(descriptor.value and state['refs']==0)
 if ok:
  words=struct.unpack('<' + ('Q'*512 if kind else 'I'*1024),page.raw)
  assert set(words)=={0x12345001 if kind else 0xc01dbead}
 
 cleanup(manager)
 assert bool(ok)==(mode=='success')
 assert dangling==(mode=='prepare-failure' and not protected)
 assert state['after_free']==(1 if mode=='prepare-failure' and not protected else 0)
 assert not errors,errors
 results.append(dict(protected=protected,mode=mode,kind=kind,result=bool(ok),dangling_before_cleanup=dangling,cleanup_access_after_release=state['after_free'],events=list(state['events'])))
print('PASS exact native allocation and native cleanup with production failure-output wrapper:',json.dumps(results))
