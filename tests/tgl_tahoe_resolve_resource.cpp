#include "kern_tgl_tahoe_resolve_resource.hpp"
#include <map>
#include <vector>
#include <memory>
#include <array>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstdio>
using namespace TGLTahoeResolveResource;
int main(int argc,char**argv){
 std::vector<uint8_t> image(ResourceClass+17);memcpy(image.data()+Entry,Head,sizeof(Head));memcpy(image.data()+0x10BCF,MapSize,sizeof(MapSize));memcpy(image.data()+0x126F5,MemorySize,sizeof(MemorySize));
 assert(matches(image.data(),image.size(),Entry,MapClass,MemoryClass,ResourceClass,TaskClass));
 for(size_t n=0;n<=ResourceClass+16;n++)assert(!matches(image.data(),n,Entry,MapClass,MemoryClass,ResourceClass,TaskClass));
 struct Guard{size_t off,len;};for(auto g:{Guard{Entry,sizeof(Head)},Guard{0x10BCF,sizeof(MapSize)},Guard{0x126F5,sizeof(MemorySize)}})
  for(size_t i=0;i<g.len;i++){image[g.off+i]^=1;assert(!matches(image.data(),image.size(),Entry,MapClass,MemoryClass,ResourceClass,TaskClass));image[g.off+i]^=1;}
 if(argc>1){std::ifstream f(argv[1],std::ios::binary);assert(f.good());std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});assert(matches(b.data(),b.size(),Entry,MapClass,MemoryClass,ResourceClass,TaskClass));puts("PASS donor resource method and exact map/memory class-size guards");}
 const uint64_t root=0xffffff8000100000ULL,r=root,p=r+0x1000,m=r+0x2000,mem=r+0x3000,v=r+0x4000,rect=r+0x5000,task=r+0x6000,fn=r+0x7000,thunk=r+0x8000;
 Classes cl{r+0x100000,r+0x110000,r+0x120000,r+0x130000};
 std::map<uint64_t,std::vector<uint8_t>> data{{r,std::vector<uint8_t>(512)},{p,std::vector<uint8_t>(160)},{m,std::vector<uint8_t>(0x128)},{mem,std::vector<uint8_t>(0x198)},{v,std::vector<uint8_t>(24)},{rect,std::vector<uint8_t>(16)},{task,std::vector<uint8_t>(8)},{cl.memory,std::vector<uint8_t>(0x128)},{fn,std::vector<uint8_t>(10)},{thunk,std::vector<uint8_t>(32)}};
 auto put=[&](uint64_t a,unsigned off,uint64_t x,unsigned n=8){memcpy(data[a].data()+off,&x,n);};
 put(r,0,cl.resource);put(r,0x40,m);put(task,0,cl.task);put(m,0,cl.map);put(m,0x18,mem);put(m,0x90,task);put(m,0x98,0x40160000);put(mem,0,cl.memory);put(mem,0x40,9375744);
 put(p,8,1,4);put(p,12,1920,4);put(p,16,1200,4);put(p,24,7680,4);put(p,32,0x40160000);put(p,64,0xd1,4);put(p,100,3,4);data[p][0x46]=3;
 put(v,0,1);put(v,8,1);put(v,16,rect);const uint32_t nativeRect[]={0,0,0x41f00000,0x42960000};memcpy(data[rect].data(),nativeRect,16);
 put(cl.memory,0x120,fn);memcpy(data[fn].data(),LengthGetter,10);const auto original=data;
 uint64_t fail=0;bool shortRead=false;unsigned reads=0;
 auto read=[&](uint64_t a,void*out,size_t n)->uint64_t{
  ++reads;if(a==fail && !shortRead)return 0;
  for(auto &block:data)if(a>=block.first && a-block.first<=block.second.size() && n<=block.second.size()-(a-block.first)){
   const auto copied=(a==fail && shortRead)?n-1:n;memcpy(out,block.second.data()+(a-block.first),copied);return copied;}
  return 0;
 };
 auto rec=std::make_unique<Receipt>();std::array<uint32_t,4> out{};unsigned cases=0;
 auto run=[&](bool expected){const auto before=data;reads=0;out.fill(0);bool changed=prepare(*rec,r,p,v,task,cl,out.data(),read);assert(changed==expected);assert(data==before && reads<=30);if(changed)assert(out[0]==0 && out[1]==0 && out[2]==0x41f00000 && out[3]==0x42980000);++cases;};
 run(true);assert(rec->q[19]==1 && rec->q[9]==3);
 // Actual native import-thunk form, followed without executing any function.
 put(cl.memory,0x120,thunk);data[thunk][0]=0xff;data[thunk][1]=0x25;put(thunk,2,2,4);put(thunk,8,fn);run(true);data=original;
 for(uint64_t addr:{r,p,m,mem,v,rect,task,cl.memory+0x120,fn,mem+0x40})for(bool partial:{false,true}){fail=addr;shortRead=partial;run(false);}fail=0;shortRead=false;
 for(unsigned off:{8U,12U,16U,20U,24U,64U,100U,0x46U,0x48U}){data=original;data[p][off]^=1;run(false);}data=original;
 for(uint64_t bytes:{0ULL,9338879ULL,UINT64_MAX}){put(mem,0x40,bytes);run(false);}data=original;
 for(uint64_t addr:{r,m,mem,task}){data[addr][0]^=1;run(false);data=original;}
 put(m,0x90,task+8);run(false);data=original;put(m,0x98,0x40170000);run(false);data=original;
 put(m,0x10,0x40,4);run(false);data=original;put(v,0,2);run(false);data=original;put(v,8,65);run(false);data=original;data[rect][12]^=1;run(false);data=original;
 data[fn][5]^=1;run(false);data=original;
 printf("PASS %u exact-backing rectangle cases: native length-getter ABI/thunk, bounded nofault/partial failures, classes/task/GPU/flags, shape/rectangle rejection and unchanged input objects\n",cases);
}
