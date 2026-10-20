#include "kern_tgl_tahoe_compression.hpp"
#include <vector>
#include <map>
#include <array>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstdio>
using namespace TGLTahoeCompression;
int main(int argc,char **argv){
 std::vector<uint8_t> image(ResourceClass+17);memcpy(image.data()+Entry,Head,sizeof(Head));
 assert(matches(image.data(),image.size(),Entry,ResourceClass,TaskClass));
 assert(!matches(nullptr,image.size(),Entry,ResourceClass,TaskClass));
 assert(!matches(image.data(),image.size(),Entry+1,ResourceClass,TaskClass));
 assert(!matches(image.data(),image.size(),Entry,ResourceClass+1,TaskClass));
 assert(!matches(image.data(),image.size(),Entry,ResourceClass,TaskClass+1));
 for(size_t n=0;n<=ResourceClass+16;n++)assert(!matches(image.data(),n,Entry,ResourceClass,TaskClass));
 for(unsigned i=0;i<sizeof(Head);i++){image[Entry+i]^=1;assert(!matches(image.data(),image.size(),Entry,ResourceClass,TaskClass));image[Entry+i]^=1;}
 std::map<uint64_t,std::vector<uint8_t>> data;
 const uint64_t r=0xffffff8000100000ULL,e=r+0x1000,t=r+0x2000,m=r+0x3000,s=r+0x4000,mem=r+0x5000;
 const uint64_t rc=r+0x100000,tc=r+0x200000;
 auto block=[&](uint64_t p,size_t n){data[p].resize(n);};
 auto put=[&](uint64_t p,size_t off,uint64_t v){memcpy(data[p].data()+off,&v,8);};
 block(r,0x108);put(r,0,rc);data[r][0x14]=0x82;put(r,0x40,m);put(r,0xE0,s);
 block(e,160);data[e][0x10]=0xC0;block(t,8);put(t,0,tc);
 block(m,0x128);put(m,0,r+0x6000);put(m,0x18,mem);put(m,0x90,t);put(m,0x98,0x40000000);
 block(s,0x238);put(s,0,rc);put(s,0x80,mem);put(s,0x88,mem+0x100);put(s,0x230,0);
 block(mem,0x48);put(mem,0,rc+0x300);put(mem,0x40,0x80000);
 auto before=data;unsigned reads=0;uint64_t fail=0;
 auto read=[&](uint64_t p,void *out,size_t n)->size_t{
  ++reads;if(p==fail)return 0;
  for(auto &v:data)if(p>=v.first && p-v.first<=v.second.size() && n<=v.second.size()-(p-v.first)){
   memcpy(out,v.second.data()+(p-v.first),n);return n;}
  return 0;
 };
 Receipt o{};
 auto run=[&](uint64_t resource,uint64_t entry){reads=0;collect(o,resource,entry,0x40000000,r+0x7000,t,2,3,true,rc,tc,read);assert(data==before);assert(reads<=10);};
 run(r,e);assert(o.q[1]==127 && reads==10);assert(o.q[7]==2 && o.q[8]==3 && o.q[9]==1);
 assert(o.q[16]==s && o.q[20]==mem+0x100 && o.q[22]==0x80000);
 for(uint64_t bad:{0ULL,0x1000ULL,UINT64_MAX-3}){run(r,bad);assert(!o.q[1] && !reads);}
 for(uint64_t bad:{e,t,r,m,s,s+0x80,s+0x88,s+0x230,mem,mem+0x40}){fail=bad;run(r,e);assert(o.q[1]!=127);}fail=0;
 data[t][0]^=1;before=data;run(r,e);assert(o.q[1]==1);
 if(argc>1){std::ifstream f(argv[1],std::ios::binary);assert(f.good());std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});
  assert(matches(b.data(),b.size(),Entry,ResourceClass,TaskClass));puts("PASS exact native compression entry bytes");}
 puts("PASS compression receipt: bounded nofault reads, every read failure, invalid pointers, class rejection, native argument recording, memory preference and unchanged inputs");
}
