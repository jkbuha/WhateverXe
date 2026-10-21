#include "kern_tgl_tahoe_resolve_receipt.hpp"
#include <map>
#include <vector>
#include <memory>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstdio>
using namespace TGLTahoeResolveReceipt;
int main(int argc,char**argv){
 std::vector<uint8_t> image(MappedClass+17);using namespace TGLTahoeResolveScratch;
 memcpy(image.data()+Entry,Head,sizeof(Head));
 memcpy(image.data()+VirtualAccessor,VirtualBytes,sizeof(VirtualBytes));
 memcpy(image.data()+LengthStore,LengthBytes,sizeof(LengthBytes));assert(TGLTahoeResolveReceipt::matches(image.data(),image.size(),Entry,MappedClass));
 assert(!TGLTahoeResolveReceipt::matches(nullptr,image.size(),Entry,MappedClass));assert(!TGLTahoeResolveReceipt::matches(image.data(),image.size(),Entry+1,MappedClass));assert(!TGLTahoeResolveReceipt::matches(image.data(),image.size(),Entry,MappedClass+1));
 for(size_t n=0;n<=MappedClass+16;n++)assert(!TGLTahoeResolveReceipt::matches(image.data(),n,Entry,MappedClass));
 for(unsigned i=0;i<sizeof(Head);i++){image[Entry+i]^=1;assert(!TGLTahoeResolveReceipt::matches(image.data(),image.size(),Entry,MappedClass));image[Entry+i]^=1;}
 for(size_t off:{VirtualAccessor,LengthStore}){image[off]^=1;assert(!TGLTahoeResolveReceipt::matches(image.data(),image.size(),Entry,MappedClass));image[off]^=1;}
 const uint64_t base=0xffffff8000100000ULL,m=base,p=base+0x1000,v=base+0x2000,rect=base+0x3000,cpu=base+0x4000,vt=base+0x100000;
 std::map<uint64_t,std::vector<uint8_t>> data{{m,std::vector<uint8_t>(64)},{p,std::vector<uint8_t>(160)},{v,std::vector<uint8_t>(24)},{rect,std::vector<uint8_t>(64)},{cpu,std::vector<uint8_t>(ScratchSize,0xA5)}};
 auto put=[&](uint64_t at,unsigned off,uint64_t val){memcpy(data[at].data()+off,&val,8);};
 put(m,0,vt);put(m,0x20,ScratchSize);put(m,0x28,0x4113c000);put(m,0x38,cpu);
 put(v,0,1);put(v,8,4);put(v,16,rect);
 auto before=data;unsigned reads=0;uint64_t fail=0;bool partial=false;
 auto read=[&](uint64_t at,void *out,size_t n)->uint64_t{
  ++reads;if(at==fail && !partial)return 0;
  for(auto &x:data)if(at>=x.first && at-x.first<=x.second.size() && n<=x.second.size()-(at-x.first)){
   const auto copied=at==fail && partial?n-1:n;memcpy(out,x.second.data()+(at-x.first),copied);return copied;}
  return 0;
 };
 auto o=std::make_unique<Receipt>();
 auto run=[&](){reads=0;collect(*o,base+0x9000,m,p,v,true,2,true,vt,read);assert(data==before);assert(reads<=5);};
 run();assert(o->q[9]==160 && o->q[10]==64 && o->q[11]==24 && o->q[12]==16 && o->q[13]==ScratchSize);
 assert(o->q[1]==2 && o->q[6] && o->q[7] && o->q[14]==0x4113c000);assert(o->scratch[ScratchSize-1]==0xA5);
 for(uint64_t addr:{m,p,v,rect,cpu})for(bool truncated:{false,true}){fail=addr;partial=truncated;run();assert(o->q[9]!=160 || o->q[10]!=64 || o->q[11]!=24 || o->q[12]!=16 || o->q[13]!=ScratchSize);}fail=0;partial=false;
 for(uint64_t count:{0ULL,65ULL,UINT64_MAX}){put(v,0,count);before=data;run();assert(!o->q[12]);}put(v,0,1);
 for(uint64_t capacity:{0ULL,4097ULL}){put(v,8,capacity);before=data;run();assert(!o->q[12]);}put(v,8,4);
 put(v,0,64);put(v,8,64);before=data;run();assert(o->q[12]==64);put(v,0,1);put(v,8,4);
 for(uint64_t length:{0ULL,0x50ffULL,0x5101ULL,UINT64_MAX}){put(m,0x20,length);before=data;run();assert(!o->q[13]);}put(m,0x20,ScratchSize);
 put(m,0,vt+8);before=data;run();assert(!o->q[13]);put(m,0,vt);
 for(uint64_t ptr:{0ULL,0x1000ULL,UINT64_MAX-1}){put(m,0x38,ptr);before=data;run();assert(!o->q[13]);}
 if(argc>1){std::ifstream f(argv[1],std::ios::binary);assert(f.good());std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});assert(TGLTahoeResolveReceipt::matches(b.data(),b.size(),Entry,MappedClass));puts("PASS exact native resolve entry");}
 puts("PASS resolve receipts: bounded existing CPU reads, class/length/pointer gates, full/partial failures, vector bounds, phase/result, unchanged inputs");
}
