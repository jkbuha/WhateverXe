#include "kern_resume_snapshot.hpp"
#include <cassert>
#include <set>
struct IO {
    std::set<uint32_t> seen;
    uint32_t read(uint32_t r){seen.insert(r);return r^0x5A5A0000U;}
};
int main() {
    using namespace ResumeSnapshot;
    static_assert(Words==Header+DisplayCount+GTCount,"layout");
    uint32_t out[Words];
    // Without a render lease no GT register is touched and the GT half is marked unread.
    IO a;capture(a,out,1,0x123456789ULL,false);
    assert(out[0]==1 && out[1]==1 && out[2]==0x23456789U && out[3]==1 && !out[4] && out[5]==Words);
    for(unsigned i=0;i<DisplayCount;++i)assert(out[Header+i]==(display[i]^0x5A5A0000U));
    for(unsigned i=0;i<GTCount;++i){assert(out[Header+DisplayCount+i]==UINT32_MAX);assert(!a.seen.count(gt[i]));}
    assert(a.seen.size()==DisplayCount);
    // With the lease, every listed register is read once, in order.
    IO b;capture(b,out,0,7,true);
    assert(out[4]==1 && b.seen.size()==DisplayCount+GTCount);
    for(unsigned i=0;i<GTCount;++i)assert(out[Header+DisplayCount+i]==(gt[i]^0x5A5A0000U));
    // The display and GT lists are disjoint and dword aligned.
    for(auto d:display){assert(!(d&3));for(auto g:gt)assert(d!=g);}
    for(auto g:gt)assert(!(g&3) && g<0x40000);
    for(auto d:display)assert(d>=0x40000);
    // GGTT rows: entry address = 8 MiB + (gpu >> 9), two dwords per entry.
    uint32_t p[PTEWords];IO c;capturePTEs(c,p,0x46E00123,0x04040000,0x402E4000);
    assert(p[0]==1 && p[1]==0x46E00123 && p[4]==0x46E00000 && p[5]==((0x800000+(0x46E00000U>>9))^0x5A5A0000U));
    assert(p[6]==((0x800000+(0x46E00000U>>9)+4)^0x5A5A0000U));
    assert(p[4+3*6]==0x04040000 && p[4+3*7]==0x402E4000 && p[4+3*2]==0x46E00000+0x100000);
    for(unsigned i=0;i<PTECount;++i){const auto e=0x800000+(p[4+3*i]>>9);assert(c.seen.count(e) && c.seen.count(e+4));}
    assert(c.seen.size()==2*PTECount);
    // Unprogrammed addresses are not followed; nothing outside the GGTT is read.
    IO z;capturePTEs(z,p,0,0,0);assert(z.seen.empty());
    for(unsigned i=0;i<PTECount;++i)assert(p[5+3*i]==UINT32_MAX && p[6+3*i]==UINT32_MAX);
    IO hi;capturePTEs(hi,p,0xFFFFF000U,0,0);for(auto r:hi.seen)assert(r>=0x800000 && r<0x1000000);
}
