#include "kern_tgl_panel_power.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <fstream>
#include <iterator>
#include <sys/mman.h>
using TGLPanelPower::Lease;
struct IO {
 uint32_t ddi=0,pg[3]={1,0,0};std::vector<unsigned> order;
 bool badGet=false,badNative=false;
 uint32_t ddiRefs(){return ddi;}uint32_t pgRefs(unsigned i){return pg[i];}
 void getPG3(){order.push_back(1);for(auto &r:pg)++r;if(badGet)--pg[2];}
 void putPG3(){order.push_back(4);for(auto &r:pg){assert(r);--r;}}
 void get(){order.push_back(2);if(!badNative)++ddi;}
 void put(){order.push_back(3);if(!badNative && ddi)--ddi;}
};
template<class T>T &at(void *p,unsigned off){return *reinterpret_cast<T *>(static_cast<uint8_t *>(p)+off);}
static unsigned hwCalls;static void hardware(void *p,bool,uint32_t){
 // These are the real donor DDI transitions. Parent references must still
 // be held at both hardware enable and hardware disable boundaries.
 for(unsigned i=0;i<3;++i)assert(at<uint32_t>(p,0x20+4*i)>2);++hwCalls;
}
int main(int argc,char **argv) {
 for(unsigned inherited:{0U,1U,3U})for(unsigned count:{1U,2U,4U}) {
  Lease s;IO io;io.ddi=inherited;
  for(unsigned i=0;i<count;++i)s.enable(true,42,io,[&]{io.get();});
  assert(s.held==count && io.ddi==inherited+count && io.pg[2]==count);
  for(unsigned i=0;i<inherited;++i)s.disable(42,io,[&]{io.put();});
  assert(s.held==count && s.inheritedPuts==inherited && io.pg[2]==count);
  for(unsigned i=0;i<count;++i)s.disable(42,io,[&]{io.put();});
  assert(!s.held && !s.owner && !io.ddi && io.pg[0]==1 && !io.pg[1] && !io.pg[2]);
  assert(s.gets==count && s.puts==count && !s.errors && !s.fault);
  assert(io.order[0]==1 && io.order[1]==2 && io.order[io.order.size()-2]==3 && io.order.back()==4);
 }
 {Lease s;IO io;s.enable(false,42,io,[&]{io.get();});assert(!s.held && io.order==std::vector<unsigned>{2});}
 {Lease s;IO io;s.enable(true,42,io,[&]{io.get();});s.enable(true,43,io,[&]{io.get();});
  s.disable(43,io,[&]{io.put();});assert(s.held==1 && s.puts==0);s.disable(42,io,[&]{io.put();});assert(!s.held && s.puts==1);}
 {Lease s;IO io;io.pg[2]=UINT32_MAX;s.enable(true,42,io,[&]{io.get();});assert(!s.gets && s.errors==1);}
 {Lease s;IO io;io.badGet=true;s.enable(true,42,io,[&]{io.get();});assert(s.fault && s.errors==1 && !s.puts);}
 {Lease s;IO io;io.badNative=true;s.enable(true,42,io,[&]{io.get();});assert(!s.held && s.gets==s.puts && s.errors==1);}
 {Lease s;IO io;s.enable(true,42,io,[&]{io.get();});io.badNative=true;s.disable(42,io,[&]{io.put();});assert(s.fault && s.held==1 && !s.puts);}
 {Lease s;IO io;s.enable(true,42,io,[&]{io.get();});io.ddi=0;s.disable(42,io,[&]{io.put();});assert(s.fault && s.held==1 && !s.puts);}
 std::vector<uint8_t> image(TGLPanelPower::Get+sizeof(TGLPanelPower::GetHead));
 memcpy(image.data()+TGLPanelPower::Get,TGLPanelPower::GetHead,sizeof(TGLPanelPower::GetHead));
 memcpy(image.data()+TGLPanelPower::Put,TGLPanelPower::PutHead,sizeof(TGLPanelPower::PutHead));
 assert(TGLPanelPower::matches(image.data(),image.size(),TGLPanelPower::Get,TGLPanelPower::Put));
 for(size_t n=0;n<image.size();++n)assert(!TGLPanelPower::matches(image.data(),n,TGLPanelPower::Get,TGLPanelPower::Put));
 struct Guard {size_t off,n;};
 for(auto g:{Guard{TGLPanelPower::Get,sizeof(TGLPanelPower::GetHead)},Guard{TGLPanelPower::Put,sizeof(TGLPanelPower::PutHead)}})for(unsigned i=0;i<g.n;++i){
  image[g.off+i]^=1;assert(!TGLPanelPower::matches(image.data(),image.size(),TGLPanelPower::Get,TGLPanelPower::Put));image[g.off+i]^=1;}
 if(argc>1) {
  std::ifstream f(argv[1],std::ios::binary);std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});
  assert(TGLPanelPower::matches(b.data(),b.size(),TGLPanelPower::Get,TGLPanelPower::Put));
  auto code=static_cast<uint8_t *>(mmap(nullptr,0x400000,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANON,-1,0));assert(code!=MAP_FAILED);
  memcpy(code,b.data(),b.size());code[0x5C2AE]=0x48;code[0x5C2AF]=0xB8;
  uintptr_t h=reinterpret_cast<uintptr_t>(hardware);memcpy(code+0x5C2B0,&h,8);code[0x5C2B8]=0xFF;code[0x5C2B9]=0xE0;
  using PG=void(*)(void *,uint32_t);
  struct NativeIO {
   alignas(8) uint8_t p[0x100]{};PG getPG,putPG;
   uint32_t ddiRefs(){return at<uint32_t>(p,0x30);}uint32_t pgRefs(unsigned i){return at<uint32_t>(p,0x20+4*i);}
   void getPG3(){getPG(p,3);}void putPG3(){putPG(p,3);}
  } io{{},reinterpret_cast<PG>(code+0x5BEDC),reinterpret_cast<PG>(code+0x5B366)};
  auto get=reinterpret_cast<PG>(code+TGLPanelPower::Get),put=reinterpret_cast<PG>(code+TGLPanelPower::Put);
  for(unsigned inherited:{0U,1U,3U}) {
   memset(io.p,0,sizeof(io.p));for(unsigned i=0;i<3;++i)at<uint32_t>(io.p,0x20+4*i)=2;
   at<uint32_t>(io.p,0x30)=inherited;Lease s;
   for(unsigned i=0;i<3;++i)s.enable(true,reinterpret_cast<uintptr_t>(io.p),io,[&]{get(io.p,0);});
   assert(s.held==3 && io.ddiRefs()==inherited+3);
   for(unsigned i=0;i<inherited+3;++i)s.disable(reinterpret_cast<uintptr_t>(io.p),io,[&]{put(io.p,0);});
   assert(!s.held && !s.errors && !s.fault && !io.ddiRefs());
   for(unsigned i=0;i<3;++i)assert(io.pgRefs(i)==2);
  }
  assert(hwCalls>=2);munmap(code,0x400000);
  puts("PASS real Tahoe donor DDI0 and PG1..3 accounting; native hardware transitions mocked, inherited and owned references balanced");
 }
 puts("PASS panel power references: lifetime/order, inherited refs, unrelated owner, overflow, inconsistent native state, binary/truncation guards");
}
