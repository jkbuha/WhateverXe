#include "kern_tgl_observation_budget.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
int main(){
 using namespace TGLObservationBudget;
 Budget b;assert(!b.allow(0,0));std::atomic<unsigned> allowed{0};std::vector<std::thread> threads;
 for(unsigned i=0;i<8;++i)threads.emplace_back([&]{for(unsigned j=0;j<20;++j)if(b.allow(1,0))++allowed;});
 for(auto &t:threads)t.join();assert(allowed==1);
 assert(!b.allow(1,IntervalNS-1));assert(!b.allow(1,0));
 for(uint64_t i=1;i<Limit;++i){assert(b.allow(1,i*IntervalNS));assert(!b.allow(1,i*IntervalNS));}
 assert(!b.allow(1,Limit*IntervalNS));assert(!b.allow(1,UINT64_MAX));
 for(uint64_t i=2;i<=Tasks;++i)assert(b.allow(i,0));assert(!b.allow(65,0));
}
