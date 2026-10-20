#include "kern_edp_pll_fraction.hpp"
#include <cassert>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <vector>
int main(int argc,char **argv) {
 using namespace EdpPLLFrac;
 Scope s;const uintptr_t thread=42,access=0x1000;
 assert(!begin(s,0,access));assert(!begin(s,1,access));assert(!begin(s,thread,0));
 assert(writeValue(s,thread,access,0x164284,0x01C001A5)==0x01C001A5);
 assert(begin(s,thread,access));assert(!begin(s,thread,access));assert(!begin(s,43,access));
 assert(writeValue(s,43,access,0x164284,0x01C001A5)==0x01C001A5);
 assert(writeValue(s,thread,access+1,0x164284,0x01C001A5)==0x01C001A5);
 assert(writeValue(s,thread,access,0x164285,0x01C001A5)==0x01C001A5);
 assert(writeValue(s,thread,access,0x164284,0x01C001A4)==0x01C001A4);assert(s.applied==0);
 assert(writeValue(s,thread,access,0x164284,0x01C001A5)==0x00E001A5);assert(s.applied==1);
 end(s,43);assert(s.thread==thread);end(s,thread);assert(s.thread==0 && s.access==0);
 assert(writeValue(s,thread,access,0x164284,0x01C001A5)==0x01C001A5);assert(s.applied==1);
 if(argc>1) {
  std::ifstream f(argv[1],std::ios::binary);assert(f.good());std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});
  assert(donorMatches(b.data(),b.size(),0x5063E));assert(!donorMatches(nullptr,b.size(),0x5063E));
  assert(!donorMatches(b.data(),b.size(),0x5063F));assert(!donorMatches(b.data(),0x5063E + 19,0x5063E));
  for(size_t n=0;n<20;++n){b[0x5063E + n]^=1;assert(!donorMatches(b.data(),b.size(),0x5063E));b[0x5063E + n]^=1;}
  puts("PASS exact Tahoe LightUpEDP entry and all 20 instruction mutations");
 }
 puts("PASS PLL fraction: exact register/value, thread/owner isolation, reentrancy, borrowed scope and cleanup");
}
