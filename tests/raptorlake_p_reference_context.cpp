#include "kern_raptorlake_p_reference_context.hpp"
#include <atomic>
#include <cassert>
#include <mutex>
#include <thread>
#include <vector>
using RaptorLakeP::ReferenceContext::Cache;
struct Object{std::atomic<unsigned>refs{1};unsigned marker=0x12345678;};
Cache cache;std::mutex mutex;std::atomic<unsigned>destroyed{0};
struct IO{void lock(){mutex.lock();}void unlock(){mutex.unlock();}
 bool tryRetain(void*p){auto*o=static_cast<Object*>(p);auto n=o->refs.load();while(n&& !o->refs.compare_exchange_weak(n,n+1)){}return n!=0;}};
void release(Object*o){if(o->refs.fetch_sub(1)==1){IO io;cache.forget(io,o);++destroyed;delete o;}}
int main(){IO io;auto*a=new Object;auto*b=new Object;cache.store(io,a);cache.store(io,b);assert(!cache.forget(io,a));release(a);auto*borrow=static_cast<Object*>(cache.acquire(io));assert(borrow==b&&b->refs==2);release(borrow);release(b);assert(!cache.acquire(io));
 Object dying;dying.refs=0;cache.store(io,&dying);assert(!cache.acquire(io));assert(!cache.pointer);
 for(unsigned n=0;n<500;++n){auto*o=new Object;cache.store(io,o);std::atomic<bool>start{false};std::vector<std::thread>readers;
  for(unsigned i=0;i<4;++i)readers.emplace_back([&]{IO local;while(!start.load()){}for(unsigned j=0;j<100;++j){auto*p=static_cast<Object*>(cache.acquire(local));if(p){assert(p->marker==0x12345678);release(p);}}});
  start=true;release(o);for(auto&t:readers)t.join();assert(!cache.acquire(io));
 }assert(destroyed==502);
}
