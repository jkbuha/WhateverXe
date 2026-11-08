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
}
