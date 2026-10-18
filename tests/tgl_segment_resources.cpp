#include "kern_tgl_segment_resources.hpp"
#include <cassert>
#include <map>
#include <vector>
int main(){
 using namespace TGLSegmentResources;
 std::map<uint64_t,uint8_t> memory;
 auto put=[&](uint64_t a,uint64_t v,unsigned n=8){for(unsigned i=0;i<n;++i)memory[a+i]=v>>(i*8);};
 const uint64_t task=0x10000,res=0x20000,entry=0x30000,map=0x40000,sys=0x50000,aux=0x60000;
 put(task,1);put(res,2);put(res+0x230,entry);put(res+0x40,map);put(map,3);put(task+0x278,aux);put(aux,4);put(aux+0x30,task);
 for(unsigned i=0;i<160;++i)memory[entry+i]=0;
 for(unsigned i=8;i<296;++i)memory[map+i]=0;
 put(entry+8,7680,4);put(entry+0x10,0xc0,4);put(entry+0x14,3,4);put(entry+0x3c,9338880,4);put(entry+0x40,36480,4);put(entry+0x6c,1920,4);put(entry+0x70,1200,4);put(entry+0x91,3,1);
 put(map+0x98,0x40150000);put(map+0x18,sys);put(sys+0x40,9375744);put(aux+0x20,0);put(aux+0x18,0x70000);
 auto read=[&](uint64_t a,void *out,size_t n)->uint64_t{for(size_t i=0;i<n;++i)if(!memory.count(a+i))return 0;for(size_t i=0;i<n;++i)((uint8_t*)out)[i]=memory[a+i];return n;};
 put(0x90000+0x5b0,task);put(0x90000+0x5c0,0xa0000);put(0xa0000+0x150,0xb0000);put(0xb0000,1);
 auto read64=[&](uint64_t a,uint64_t &v){return read(a,&v,8)==8;};
 assert(queueTask(0x90000,1,read64)==task);
 put(0x90000+0x5b0,0);assert(queueTask(0x90000,1,read64)==0); // no internal-task fallback
 put(0x90000+0x5b0,task);put(task,9);assert(queueTask(0x90000,1,read64)==0);put(task,1);
 Record r{};auto go=[&]{r={};return collect(r,task,res,1,2,3,4,read);};assert(go());assert(r.q[14]==0 && r.q[8]==0x40150000 && r.q[10]==9375744 && r.entry[0x91]==3);
 put(aux+0x30,task+1);assert(go() && r.q[14]==1);put(aux+0x30,task);
 put(map+16,0x40,4);assert(!go());put(map+16,0,4);
 put(entry+0x10,0xd1,4);assert(!go());put(entry+0x10,0xc0,4);
 put(task,99);assert(!go());put(task,1);
 memory.erase(entry+100);assert(!go());
}
