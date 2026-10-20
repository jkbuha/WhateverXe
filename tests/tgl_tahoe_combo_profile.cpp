#include "kern_edp_combo_profile.hpp"
#include "kern_edp_combo_scaling.hpp"
#include <cassert>
#include <map>
#include <vector>
#include <cstdio>
#include <cstring>
int main() {
 alignas(8) uint8_t port[0x600]{};uint32_t cfg[4]{};
 *reinterpret_cast<uint32_t **>(port+0x548)=cfg;port[0x11]=2;
 *reinterpret_cast<uint32_t *>(port+0x550)=1;
 *reinterpret_cast<uint32_t *>(port+0x124)=2;*reinterpret_cast<uint32_t *>(port+0x128)=10;
 *reinterpret_cast<uintptr_t *>(port+0x578)=0x1000;
 EdpPLLFrac::Scope scope;assert(EdpPLLFrac::begin(scope,42,0x1000));
 const uint32_t rows[10][5]={{0xA,0x32,0x3F,0,0},{0xA,0x4F,0x37,0,8},{0xC,0x71,0x2F,0,0x10},{6,0x7D,0x2B,0,0x14},
  {0xA,0x4C,0x3F,0,0},{0xC,0x73,0x34,0,0xB},{6,0x7F,0x2F,0,0x10},{0xC,0x6C,0x3C,0,3},{6,0x7F,0x35,0,0xA},{6,0x7F,0x3F,0,0}};
 const unsigned starts[]={0,4,7,9};
 for(unsigned swing=0;swing<4;++swing)for(unsigned emphasis=0;emphasis+swing<=3;++emphasis) {
  std::map<uint32_t,uint32_t> regs;std::vector<uint32_t> writes;
  for(unsigned lane=0;lane<4;++lane){regs[0x162890+0x100*lane]=0x12345678;regs[0x16289C+0x100*lane]=0x87654321;}
  regs[0x162894]=0x80080000;EdpComboProfile::Report report;unsigned calls=0;
  assert(EdpComboProfile::apply(scope,42,port,swing,emphasis,nullptr,
   [&](uint32_t reg){assert(regs.count(reg));return regs[reg];},
   [&](uint32_t reg,uint32_t value){writes.push_back(reg);regs[reg]=value;},
   [&](void *config)->uint32_t {auto v=static_cast<uint32_t *>(config);const auto &row=rows[starts[swing]+emphasis];
    assert(v[0]==0 && v[1]==0 && v[2]==row[0] && v[3]==((row[4]<<12)|row[2]) && v[4]==0x98 && v[5]==1 && v[6]==0);++calls;return 0;},report));
  assert(calls==1 && report.applied==1 && report.result==0 && report.level==starts[swing]+emphasis);
  assert(writes.size()==10 && writes.front()==0x162694 && writes.back()==0x162694);
  assert(regs[0x162694]==0x80080000);
  const auto &row=rows[report.level];
  for(unsigned lane=0;lane<4;++lane) {
   assert(regs[0x162890+0x100*lane]==((0x12345678&0x7FFC0000U)|((lane==1||lane==2)?0x80000000U:0)|(row[4]<<12)|row[2]));
   assert(regs[0x16289C+0x100*lane]==((0x87654321&~0x7F000000U)|(row[1]<<24)));
  }
 }
 // Every native error is preserved without follow-up writes.
 for(uint32_t error:{1U,0xE00002D6U}) {
  unsigned writes=0;EdpComboProfile::Report report;
  assert(EdpComboProfile::apply(scope,42,port,0,0,nullptr,[](uint32_t){return 0U;},
   [&](uint32_t,uint32_t){++writes;},[&](void *){return error;},report));assert(report.result==error && !writes && !report.applied);
 }
 for(unsigned bad=0;bad<8;++bad) {
  cfg[0]=bad==0?1:0;port[0x11]=bad==1?1:2;*reinterpret_cast<uintptr_t *>(port+0x578)=bad==7?0x1001:0x1000;unsigned reads=0,calls=0,writes=0;EdpComboProfile::Report report;
  bool ok=EdpComboProfile::apply(scope,bad==2?43:42,port,bad==3?4:0,bad==4?4:0,bad==5?port:nullptr,
   [&](uint32_t){++reads;return bad==6?UINT32_MAX:0U;},[&](uint32_t,uint32_t){++writes;},[&](void *){++calls;return 0U;},report);
  assert(!ok && !calls && !writes);
 }
 cfg[0]=0;port[0x11]=2;
 assert(EdpComboScaling::writeValue(scope,42,0x1000,0x162694,0x801ABCDE)==0x800ABCDE);
 assert(EdpComboScaling::writeValue(scope,43,0x1000,0x162694,0x801ABCDE)==0x801ABCDE);
 assert(EdpComboScaling::writeValue(scope,42,0x1001,0x162694,0x801ABCDE)==0x801ABCDE);
 assert(EdpComboScaling::writeValue(scope,42,0x1000,0x162695,0x801ABCDE)==0x801ABCDE);
 EdpPLLFrac::end(scope,42);assert(EdpComboScaling::writeValue(scope,42,0x1000,0x162694,0x801ABCDE)==0x801ABCDE);
 puts("PASS Combo PHY: all 10 profiles, preserved register bits, training latch ordering, native errors, invalid paths and scoped scaling");
}
