#include "kern_edp_handoff.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>
int main() {
 EdpHandoff::Evidence e{};uint32_t software[8]={0x6C1,1,0,0,0,0,0,1};unsigned reads=0;
 auto good=[&](uint32_t reg)->uint32_t{++reads;return reg==0x45404?0x3F:reg==0x45454||reg==0x45444?3:reg;};
 assert(!EdpHandoff::capture(e,0,false,0,0,0,software,good));assert(!reads);
 assert(!EdpHandoff::capture(e,4,true,0,0,0,software,good));assert(!reads);
 assert(!EdpHandoff::capture(e,0,true,0,0,0,software,[](uint32_t){return 0U;}));
 assert(!EdpHandoff::capture(e,0,true,0,0,0,software,[](uint32_t){return UINT32_MAX;}));
 assert(EdpHandoff::capture(e,0,true,0,0,0,software,good));
 assert(e.slots[0].state==2 && e.slots[0].q[0]==2 && e.slots[0].q[159]==0x48414E44);
 assert(e.slots[0].q[6]==sizeof(EdpHandoff::regs)/sizeof(EdpHandoff::regs[0]));
 auto before=e;assert(!EdpHandoff::capture(e,0,true,0,0,0,software,good));assert(!memcmp(&before,&e,sizeof(e)));
 unsigned auxReads=0;
 auto auxOff=[&](uint32_t reg)->uint32_t {
  if(reg>=0x162300 && reg<0x162400)++auxReads;
  return reg==0x45404?0x3F:reg==0x45454?3:reg==0x45444?0:reg;
 };
 assert(EdpHandoff::capture(e,1,true,0,0,0,software,auxOff));assert(!auxReads);
 for(unsigned i=0;i<e.slots[1].q[6];++i) {
  auto reg=e.slots[1].q[16+2*i];if(reg>=0x162300 && reg<0x162400)assert(e.slots[1].q[17+2*i]==UINT32_MAX);
 }
 std::vector<std::thread> threads;
 for(unsigned i=0;i<8;++i)threads.emplace_back([&]{auto rd=[](uint32_t reg)->uint32_t{return reg==0x45404?0x3F:reg==0x45454||reg==0x45444?3:reg;};EdpHandoff::capture(e,2,true,0,0,0,software,rd);});
 for(auto &thread:threads)thread.join();assert(e.slots[2].state==2 && e.slots[2].q[159]==0x48414E44);
 puts("PASS powered panel snapshots: no unqualified/unpowered reads, AUX-lane ACK gating, immutable slots and concurrent completion");
}
