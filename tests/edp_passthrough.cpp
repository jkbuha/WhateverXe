#include "../WhateverXe/kern_edp_passthrough.hpp"
#include <array>
#include <cassert>
#include <utility>

int main() {
 std::array<uint8_t,160> entry {};
 std::array<uint8_t,296> map {};
 std::array<uint8_t,408> memory {};
 auto put=[](uint8_t *p,unsigned offset,uint64_t value,unsigned size){memcpy(p+offset,&value,size);};
 for(auto field: {std::pair<unsigned,uint32_t>{8,7680},{0x10,0xD1},{0x14,3},
  {0x3C,9338880},{0x40,36480},{0x4C,1},{0x6C,1920},{0x70,1200},{0x74,1},{0x78,1}})
  put(entry.data(),field.first,field.second,4);
 constexpr uint64_t task=0x10000,base=0x2000000;
 put(map.data(),0x90,task,8);put(map.data(),0x98,base,8);
 put(memory.data(),0x40,9375744,8);
 auto eligible=[&]{return EdpPassThrough::eligible(entry.data(),map.data(),memory.data(),task);};
 assert(eligible());
 for(unsigned offset: {8U,0x10U,0x14U,0x3CU,0x40U,0x44U,0x4CU,0x64U,
  0x68U,0x6CU,0x70U,0x74U,0x78U,0x91U}){
  entry[offset]^=1;assert(!eligible());entry[offset]^=1;
 }
 for(unsigned offset: {0x90U,0x98U}){map[offset]^=1;assert(!eligible());map[offset]^=1;}
 map[0x10]=0x40;assert(!eligible());map[0x10]=0;
 memory[0x40]^=1;assert(!eligible());memory[0x40]^=1;
 put(map.data(),0x98,0,8);assert(!eligible());
 put(map.data(),0x98,0x800000000000ULL,8);assert(!eligible());
 put(map.data(),0x98,base,8);assert(eligible());
 auto before=entry;
 assert(EdpPassThrough::complete(entry.data(),before.data()));
 assert(EdpPassThrough::word(entry.data()+0x4C)==0);
 assert(entry[0x91]==0);
 entry[0x4C]=1;assert(entry==before);
 entry[0x10]^=1;auto changed=entry;
 assert(!EdpPassThrough::complete(entry.data(),before.data()));assert(entry==changed);
}
