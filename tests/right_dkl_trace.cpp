#include "kern_right_dkl_trace.hpp"
#include <cassert>
#include <vector>
#include <thread>
#include <cstdio>
int main(){
 RightDKLTrace t;auto add=[&](uint32_t physical,uint32_t ddi,bool write,uint32_t index,uint32_t mode,uintptr_t thread=0x123456789abcdef0ULL){t.record(physical,ddi,write,index,0x16a2c0,1,mode,0x123,0x234,0x345,0x456,0x567,0x678,3,thread,0x1122334455667788ULL);};
 add(0x9a408086,5,true,0x42,3);add(0x9a498086,4,true,0x42,3);add(0x9a498086,5,false,0x42,3);add(0x9a498086,5,true,0x33,3);add(0x9a498086,5,true,0x4a,3);add(0x9a498086,5,true,0x42,4);assert(!t.count);
 add(0x9a498086,5,true,0x42,3);assert(t.count==1);
 std::vector<uint32_t> out(8+RightDKLTrace::Capacity*RightDKLTrace::Words,0xdeadbeef);
 assert(!t.snapshot(nullptr,out.size()));assert(!t.snapshot(out.data(),7));assert(!t.snapshot(out.data(),8));assert(t.snapshot(out.data(),out.size())==26);
 assert(out[0]==1 && out[1]==1 && out[2]==1 && out[3]==1024 && out[4]==72 && out[5]==1);
 uint32_t want[]={1,1,0x42,1,0x16a2c0,1,3,0x123,0x234,0x345,0x456,0x567,0x678,3,0x9abcdef0,0x12345678,0x55667788,0x11223344};
 for(unsigned i=0;i<18;i++)assert(out[8+i]==want[i]);
 t.entries[0][0]=0;assert(t.snapshot(out.data(),out.size())==26 && out[5]==0);for(unsigned i=0;i<18;i++)assert(!out[8+i]);t.entries[0][0]=1;
 for(unsigned i=1;i<1200;i++)add(0x9a498086,5,true,0x34,3);
 assert(t.count==1200 && t.snapshot(out.data(),out.size())==out.size() && out[2]==1024 && out[5]==1024);
 for(unsigned i=0;i<18;i++)assert(out[8+i]==want[i]);
 RightDKLTrace concurrent;std::thread workers[4];for(unsigned i=0;i<4;i++)workers[i]=std::thread([&,i]{for(unsigned j=0;j<200;j++)concurrent.record(0x9a498086,5,true,0x34,0x46038,0,3,j,j,0,j,0,0,0,i,123);});
 for(auto &w:workers)w.join();assert(concurrent.count==800);assert(concurrent.snapshot(out.data(),out.size())==8+800*18 && out[5]==800);for(unsigned i=0;i<800;i++)assert(out[8+i*18]==1 && out[9+i*18]==i+1);
 puts("PASS: scope rejection, exact values/thread/timestamp, bounded buffers/capacity, unpublished slots skipped, first record retained and800concurrent receipts published.");
}
