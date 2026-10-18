#include "kern_tgl_surface_receipts.hpp"
#include <map>
#include <cassert>
int main(){using namespace TGLSurfaceReceipts;
 std::map<uint64_t,uint8_t> m;const uint64_t resource=0x1000,entry=0x4000,surface=0x5000,map=0x6000,task=0x7000,aux=0x8000,storage=0x9000,rootMap=0xa000;
 auto put=[&](uint64_t a,uint64_t v,unsigned n=8){for(unsigned i=0;i<n;++i)m[a+i]=v>>(8*i);};
 for(unsigned i=0;i<160;++i)m[entry+i]=0;for(unsigned i=0;i<64;++i)m[surface+i]=0;for(unsigned i=0;i<296;++i)m[map+i]=0;
 put(resource,1);put(resource+0x14,0x82,1);put(resource+0xe0,storage);put(storage,1);put(storage+0x14,0xc0,1);put(resource+0x40,map);put(map,2);put(map+0x90,task);put(map+0x98,0x40154000);put(map+0x18,0xb000);put(task,3);put(task+0x278,aux);put(aux,4);put(aux+0x10,rootMap);put(rootMap,2);put(rootMap+0x98,0x40008000);put(aux+0x18,0xc000);put(aux+0x20,0);put(aux+0x30,task);
 put(surface,0xc0U<<18,4);put(surface+12,7679,4);put(entry+0x10,0xc0,4);put(entry+0x91,3,1);
 unsigned reads=0;auto read=[&](uint64_t a,void*out,size_t n)->uint64_t{++reads;for(size_t i=0;i<n;++i)if(!m.count(a+i))return 0;for(size_t i=0;i<n;++i)((uint8_t*)out)[i]=m[a+i];return n;};
 Record r{};auto go=[&]{r={};auto before=m;const bool result=collect(r,resource,entry,surface,1,2,3,4,read);assert(m==before);return result;};
 assert(go()&&r.q[22]==0&&r.q[4]==entry&&r.q[10]==storage&&r.q[12]==0xc0&&r.q[16]==0x40154000&&r.q[20]==0x40008000&&r.entry[0x91]==3);
 put(surface,0x80U<<18,4);put(surface+12,15359,4);put(entry+0x10,0x80,4);assert(go()&&r.q[22]==0&&kind(r.surface)==1);
 put(surface+12,7679,4);assert(!go());put(surface+12,15359,4);
 put(resource,99);assert(go()&&r.q[22]==1);put(resource,1);
 put(map,99);assert(go()&&r.q[22]==2);put(map,2);
 put(task,99);assert(go()&&r.q[22]==3);put(task,3);
 put(aux,99);assert(go()&&r.q[22]==4);put(aux,4);
 put(rootMap,99);assert(go()&&r.q[22]==6);put(rootMap,2);
 put(aux+0x30,task+1);assert(go()&&r.q[22]==5);put(aux+0x30,task);
 put(aux+0x20,7);assert(go()&&r.q[21]==7&&r.q[22]==0);
 assert(entry!=resource+0x180&&entry!=storage+0x180);
 m.erase(entry+100);assert(!go());assert(reads>0);
}
