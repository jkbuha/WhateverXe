#include "kern_tgl_tahoe_panel_clock.hpp"
#include <cassert>
#include <map>
#include <vector>
#include <cstdio>
struct IO {
 std::map<uint32_t,uint32_t> regs{{0x45404,0x3F},{0x45454,3},{0x46140,0},
  {0x60400,0x02000002},{0x64000,0},{0x60540,0}};
 std::vector<uint32_t> reads,writes;uint32_t fail=0;bool failed=false;
 uint32_t read(uint32_t r){reads.push_back(r);assert(regs.count(r));return regs[r];}
 void write(uint32_t r,uint32_t v) {writes.push_back(r);assert(r==0x46140 || r==0x60400);
  if(r==fail && !failed){failed=true;return;}regs[r]=v;}
};
int main() {
 using namespace TGLTahoePanelClock;
 Report report;uint32_t path=0;IO io;
 assert(apply(io,true,0,2,10,path,0x8A000002,report));
 assert(path==0x10000000 && report.status==5 && io.regs[0x60400]==0x0A000002);
 for(unsigned invalid=0;invalid<4;++invalid) {
  IO i;uint32_t p=0;
  assert(!apply(i,invalid!=0,invalid==1?1:0,invalid==2?4:2,invalid==3?20:10,p,0x8A000002,report));
  assert(i.reads.empty() && i.writes.empty() && p==0);
 }
 for(auto reg:{0x45404U,0x45454U})for(uint32_t value:{0U,UINT32_MAX}) {
  IO i;i.regs[reg]=value;uint32_t p=0;
  assert(!apply(i,true,0,2,10,p,0x8A000002,report));assert(i.writes.empty() && i.reads.size()==2 && p==0);
 }
 for(auto reg:{0x64000U,0x60540U}) {
  IO i;i.regs[reg]=0x80000000;uint32_t p=0;
  assert(!apply(i,true,0,2,10,p,0x8A000002,report));assert(i.writes.empty() && p==0);
 }
 for(auto reg:{0x46140U,0x60400U}) {
  IO i;i.regs[reg]=0x80000000;uint32_t p=0;
  assert(!apply(i,true,0,2,10,p,0x8A000002,report));assert(i.writes.empty() && p==0);
 }
 for(auto reg:{0x46140U,0x60400U}) {
  IO i;const auto before=i.regs;i.fail=reg;uint32_t p=0;
  assert(!apply(i,true,0,2,10,p,0x8A000002,report));assert(i.regs==before && p==0);
 }
 for(auto pair:{std::pair<uint32_t,uint32_t>{1,0x8A000002},{0,0x02000002}}) {
  IO i;auto p=pair.first;assert(!apply(i,true,0,2,10,p,pair.second,report));assert(i.writes.empty() && p==pair.first);
 }
 puts("PASS panel route: admitted DDI-A ordering, unpowered/active/foreign-path refusal, both write-failure rollbacks");
}
