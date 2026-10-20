#include "kern_edp_aux_lease.hpp"
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <vector>
int main() {
 // A firmware/other-client reference must survive our balanced get/put,
 // including native errors and a nested training call.
 for(unsigned inherited : {0U,1U,7U})for(bool admitted : {false,true})
  for(uint32_t error : {0U,1U,0xE00002D6U,0xE00002BCU}) {
   unsigned refs=inherited,gets=0,puts=0,calls=0;std::vector<unsigned> order;
   auto get=[&]{++gets;++refs;order.push_back(1);};
   auto put=[&]{assert(refs>inherited);++puts;--refs;order.push_back(3);};
   const auto result=EdpAuxLease::run(admitted,get,put,[&]{
    ++calls;order.push_back(2);assert(refs==inherited+unsigned(admitted));return error;
   });
   assert(result==error && calls==1 && refs==inherited);
   assert(gets==unsigned(admitted) && puts==gets);
   assert(order==(admitted?std::vector<unsigned>{1,2,3}:std::vector<unsigned>{2}));
  }
 unsigned refs=7,gets=0,puts=0;
 auto get=[&]{++gets;++refs;};auto put=[&]{assert(refs>7);++puts;--refs;};
 assert(EdpAuxLease::run(true,get,put,[&]{
  assert(refs==8);auto result=EdpAuxLease::run(true,get,put,[&]{assert(refs==9);return 0xE00002BCU;});
  assert(refs==8);return result;
 })==0xE00002BCU);
 assert(refs==7 && gets==2 && puts==2);
 std::puts("PASS training AUX lease: ordering, native results, inherited refs, bypass and balanced nesting");
}
