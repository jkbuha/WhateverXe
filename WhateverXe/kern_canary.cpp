// Diagnostic memory canary (-wex-canary, 8192 MiB, rescanned every 20 s).
// Wires N MiB of ordinary kernel memory, fills every 64-bit word with a value
// derived from its own physical address, and rescans it every 10 s from a
// low-priority thread call. Any mismatch is a write by someone who does not own
// that memory (CPU or DMA). Each hit records physical address, expected and
// observed value and uptime, is logged, and is repaired so later hits stay
// distinguishable. Purely observational: no hardware or Apple object access.
#include <IOKit/IOLib.h>
#include <IOKit/IOBufferMemoryDescriptor.h>
#include <IOKit/IORegistryEntry.h>
#include <kern/clock.h>
#include <kern/thread_call.h>
#include <pexpert/pexpert.h>
#include <Headers/kern_util.hpp>

namespace {
constexpr uint32_t ChunkBytes=4U<<20,MaxChunks=2048,MaxHits=128,Buckets=20,PeriodSeconds=20;
struct Hit {uint64_t physical,expected,observed,ns;};
struct State {
    IOBufferMemoryDescriptor *chunk[MaxChunks] {};
    uint64_t *physical {};          // physical address of every 4 KiB page
    uint32_t chunks {},pages {};
    uint64_t scans {},hits {},lastScanNs {},firstHitNs {},minPA {~0ULL},maxPA {},above64G {};
    uint64_t bucket[Buckets] {};     // pages per 4 GiB physical bucket (last = >=76 GiB)
    uint64_t bitHits[64] {};         // hits per flipped bit position
    Hit hit[MaxHits] {};
    thread_call_t call {},scanCall {};
} canary;

inline uint64_t pattern(uint64_t pa) {
    uint64_t v=pa*0x9E3779B97F4A7C15ULL+0x5851F42D4C957F2DULL;
    v^=v>>29;
    // Alternate polarity every word so each bit is 1 in half the words: a
    // set-only or clear-only fault is caught at every position.
    return (pa>>3)&1 ? ~v : v;
}
uint64_t nowNs() {uint64_t t,ns;clock_get_uptime(&t);absolutetime_to_nanoseconds(t,&ns);return ns;}

void publish() {
    auto root=IORegistryEntry::getRegistryRoot();
    uint64_t summary[13+Buckets] {3,uint64_t(canary.pages)*4096,canary.pages,canary.scans,canary.hits,
        canary.lastScanNs,canary.firstHitNs,canary.minPA,canary.maxPA,canary.above64G,PeriodSeconds,canary.chunks,MaxHits};
    for(uint32_t i=0;i<Buckets;i++)summary[13+i]=canary.bucket[i];
    root->setProperty("WEXCanary",summary,sizeof(summary));
    root->setProperty("WEXCanaryBitHits",canary.bitHits,sizeof(canary.bitHits));
    const auto n=canary.hits<MaxHits ? canary.hits : MaxHits;
    if(n)root->setProperty("WEXCanaryHits",canary.hit,static_cast<unsigned>(n*sizeof(Hit)));
}

void scan(thread_call_param_t,thread_call_param_t) {
    const auto start=canary.hits;
    for(uint32_t c=0;c<canary.chunks;c++) {
        auto base=static_cast<volatile uint64_t *>(canary.chunk[c]->getBytesNoCopy());
        for(uint32_t p=0;p<ChunkBytes/4096;p++) {
            const auto pa=canary.physical[c*(ChunkBytes/4096)+p];
            auto w=base+p*512;
            for(uint32_t i=0;i<512;i++) {
                const auto expected=pattern(pa+8*i),observed=w[i];
                if(__builtin_expect(observed==expected,1))continue;
                const auto ns=nowNs();
                if(!canary.hits)canary.firstHitNs=ns;
                if(canary.hits<MaxHits)canary.hit[canary.hits]={pa+8*i,expected,observed,ns};
                auto diff=observed^expected;
                while(diff) {canary.bitHits[__builtin_ctzll(diff)]++;diff&=diff-1;}
                if(canary.hits<MaxHits)
                    IOLog("WEXCanary: hit %llu pa=0x%llx expected=0x%016llx observed=0x%016llx xor=0x%016llx t=%llu ms\n",
                        canary.hits,pa+8*i,expected,observed,expected^observed,ns/1000000);
                canary.hits++;
                w[i]=expected;
            }
        }
    }
    canary.scans++;canary.lastScanNs=nowNs();
    if(canary.hits!=start || canary.scans<=2 || !(canary.scans%30))publish();
    uint64_t deadline;clock_interval_to_deadline(PeriodSeconds,NSEC_PER_SEC,&deadline);
    thread_call_enter_delayed(canary.scanCall,deadline);
}

void setup(thread_call_param_t,thread_call_param_t) {
    const uint32_t mb=8192;
    auto want=(uint64_t(mb)<<20)/ChunkBytes;if(want>MaxChunks)want=MaxChunks;if(!want)want=1;
    canary.physical=static_cast<uint64_t *>(IOMalloc(size_t(want)*(ChunkBytes/4096)*sizeof(uint64_t)));
    if(!canary.physical) {IOLog("WEXCanary: page table allocation failed\n");return;}
    for(uint32_t c=0;c<want;c++) {
        auto b=IOBufferMemoryDescriptor::withOptions(kIODirectionInOut|kIOMemoryMapperNone,ChunkBytes,4096);
        if(!b || !b->getBytesNoCopy()) {OSSafeReleaseNULL(b);break;}
        auto base=static_cast<uint64_t *>(b->getBytesNoCopy());
        bool ok=true;
        for(uint32_t p=0;p<ChunkBytes/4096 && ok;p++) {
            IOByteCount span=0;const auto pa=b->getPhysicalSegment(uint64_t(p)*4096,&span,kIOMemoryMapperNone);
            if(!pa || (pa&4095) || span<4096) {ok=false;break;}
            canary.physical[c*(ChunkBytes/4096)+p]=pa;
            for(uint32_t i=0;i<512;i++)base[p*512+i]=pattern(pa+8*i);
        }
        if(!ok) {b->release();break;}
        canary.chunk[c]=b;canary.chunks++;
        for(uint32_t p=0;p<ChunkBytes/4096;p++) {
            const auto pa=canary.physical[c*(ChunkBytes/4096)+p];
            if(pa<canary.minPA)canary.minPA=pa;
            if(pa>canary.maxPA)canary.maxPA=pa;
            if(pa>=(1ULL<<36))canary.above64G++;
            auto k=pa>>32;canary.bucket[k<Buckets ? k : Buckets-1]++;
        }
    }
    canary.pages=canary.chunks*(ChunkBytes/4096);
    IOLog("WEXCanary: armed %u MiB in %u chunks, pa 0x%llx-0x%llx, %llu pages >=64GiB\n",
        canary.chunks*(ChunkBytes>>20),canary.chunks,canary.minPA,canary.maxPA,canary.above64G);
    publish();
    if(!canary.chunks)return;
    // The setup call stays allocated: freeing a call from its own handler is unsafe.
    canary.scanCall=thread_call_allocate_with_priority(scan,nullptr,THREAD_CALL_PRIORITY_LOW);
    uint64_t deadline;clock_interval_to_deadline(PeriodSeconds,NSEC_PER_SEC,&deadline);
    if(canary.scanCall)thread_call_enter_delayed(canary.scanCall,deadline);
}
} // namespace

void wexCanaryStart() {
    if(!checkKernelArgument("-wex-canary") || canary.call)return;
    // Arm 30 s after the plugin starts so boot-time allocation pressure is past.
    canary.call=thread_call_allocate_with_priority(setup,nullptr,THREAD_CALL_PRIORITY_LOW);
    uint64_t deadline;clock_interval_to_deadline(30,NSEC_PER_SEC,&deadline);
    if(canary.call)thread_call_enter_delayed(canary.call,deadline);
}
