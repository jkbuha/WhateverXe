#include "kern_tgl_layout_receipts.hpp"
#include <cassert>
#include <cstdio>
#include <thread>
#include <vector>
#include <atomic>
using namespace TGLLayoutReceipts;
int main(){
 Store a;uint32_t s[16]={};s[0]=0xc0U<<18;s[2]=(299U<<16)|255;s[3]=1023;unsigned reads=0;
 auto read=[&](uint64_t entry,void *out,size_t n)->size_t{assert(entry==0x2000 && n==160);++reads;memset(out,0x5a,n);return n;};
 assert(a.record(s,0x1000,0x2000,0x3000,read));assert(a.used==1 && a.records[0].q[0]==1 && !a.records[0].q[5]);
 s[8]=0xabcd;s[9]=0x1234;assert(!a.record(s,0x1001,0x2000,0x3001,read));assert(reads==1 && a.duplicates==1);
 s[3]++;assert(a.record(s,0x1001,0x2000,0x3001,read));s[0]=0x80U<<18;assert(a.record(s,0x1001,0x2000,0x3001,read));
 assert(!a.record(nullptr,0,0,0,read));s[2]=8192;assert(!a.record(s,0,0,0,read));s[2]=8192U<<16;assert(!a.record(s,0,0,0,read));s[2]=99;
 auto fail=[](uint64_t,void *,size_t)->size_t{return 0;};assert(a.record(s,1,2,3,fail));assert(a.entryFailures==1 && a.records[3].q[5]==1);
 Store full;for(unsigned i=0;i<Capacity;i++){s[0]=i;assert(full.record(s,0x1000,0x2000,0x3000,read));assert(full.records[i].q[1]==i);}
 s[0]=0xeeee;assert(full.full() && !full.record(s,0x1000,0x2000,0x3000,read));assert(full.used==Capacity && full.dropped==1);
 Store concurrent;std::atomic<unsigned> calls{0};std::vector<std::thread> threads;
 for(unsigned t=0;t<8;t++)threads.emplace_back([&,t]{for(unsigned i=0;i<20;i++){uint32_t words[16]={};words[0]=i;words[2]=(199U<<16)|255;words[3]=1023;auto r=[&](uint64_t,void *out,size_t n)->size_t{calls.fetch_add(1);memset(out,0x55,n);return n;};concurrent.record(words,t,0,3,r);}});
 for(auto &t:threads)t.join();assert(concurrent.used>=20 && concurrent.used<=160 && calls==concurrent.used);
 for(unsigned i=0;i<concurrent.used;i++){const auto &r=concurrent.records[i];assert(__atomic_load_n(r.q,__ATOMIC_ACQUIRE)==1 && !r.q[5]);assert(r.surface[0]<20 && r.surface[2]==((199U<<16)|255));for(auto v:r.entry)assert(v==0x55);}
 puts("PASS: menu-sized and arbitrary format/pitch layouts, GPU-address dedup, bounds/null, failed reads, capacity, immutable release publication and eight concurrent writers.");
}
