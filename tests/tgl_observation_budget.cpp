#include "kern_tgl_observation_budget.hpp"
#include <thread>
#include <vector>
#include <atomic>
#include <cassert>
int main(){
 TGLObservationBudget::Budget b;assert(!b.allow(0));std::atomic<unsigned> allowed{0};std::vector<std::thread> threads;
 for(unsigned i=0;i<8;++i)threads.emplace_back([&]{for(unsigned j=0;j<20;++j)if(b.allow(1))++allowed;});
 for(auto &t:threads)t.join();assert(allowed==64);
 for(uint64_t i=2;i<=64;++i)assert(b.allow(i));assert(!b.allow(65));assert(!b.allow(1));
}
