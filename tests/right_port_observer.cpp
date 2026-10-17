#include "kern_right_port_observer.hpp"
#include <cassert>
#include <thread>
#include <vector>
int main() {
 RightPortObserver rejected;
 rejected.record(0,3,3,1,2,3,4);rejected.record(RightPortObserver::Kinds,5,3,1,2,3,4);
 uint32_t out[RightPortObserver::Capacity*RightPortObserver::Words] {};
 assert(rejected.snapshot(out,sizeof(out)/4)==0);
 RightPortObserver capture;
 std::vector<std::thread> writers;
 for(unsigned k=0;k<RightPortObserver::Kinds;k++)writers.emplace_back([&,k]{
  for(unsigned n=0;n<1000;n++)capture.record(k,5,3,n,k^n,0x12345678,~n);
 });
 // Readers may run while writers reserve/fill slots; only ready records appear.
 for(unsigned n=0;n<1000;n++) {
  const auto words=capture.snapshot(out,sizeof(out)/4);
  assert(words%8==0);
  for(unsigned i=0;i<words;i+=8){
   assert(out[i+2]==5 && out[i+3]==3);
   assert(out[i+5]==(out[i+1]^out[i+4]));
   assert(out[i+6]==0x12345678 && out[i+7]==~out[i+4]);
  }
 }
 for(auto &t:writers)t.join();
 assert(capture.snapshot(out,sizeof(out)/4)==sizeof(out)/4);
 assert(capture.snapshot(out,7)==0);
 assert(capture.snapshot(out,8)==8);
 // Full slots remain immutable after later calls.
 const auto first=out[4];capture.record(0,5,4,9999,0,0,0);
 capture.snapshot(out,8);assert(out[4]==first);
}
