#include "kern_display13_edp_blank.hpp"
#include <assert.h>
#include <map>
#include <stdio.h>
#include <vector>
using namespace Display13EDPBlank;
struct IO {
    std::map<uint32_t,uint32_t> r;
    std::vector<uint32_t> writes;
    uint32_t read(uint32_t reg) {assert(r.count(reg));return r[reg];}
    void write(uint32_t reg,uint32_t v) {assert(r.count(reg));r[reg]=v;writes.push_back(reg);}
};
static IO live() {
    IO io;
    io.r[PlaneCtl]=0x84001008U; io.r[PlaneSurf]=0x00200000U;
    io.r[CurCtl]=0x04000027U; io.r[CurBase]=0x01230000U;
    return io;
}
int main() {
    // Decisions (eligible, owner, current, requested, blanked).
    assert(decide(false,false,2,1,false)==Action::Native);
    assert(decide(true,false,2,1,false)==Action::Blank);
    // Off is a system sleep (S3/hibernation): always the donor's, blanked or not.
    assert(decide(true,false,2,0,false)==Action::Native);
    assert(decide(true,true,1,0,true)==Action::Native);
    assert(decide(true,false,1,0,false)==Action::Native);
    // The wake after a native Off is the donor's own wake too.
    assert(decide(true,false,0,2,false)==Action::Native);
    assert(decide(true,true,1,2,true)==Action::Unblank);
    assert(decide(true,true,0,2,true)==Action::Unblank);
    assert(decide(true,false,2,2,false)==Action::Keep);
    assert(decide(true,false,1,2,false)==Action::Native);      // native doze stays native
    assert(decide(true,false,2,3,false)==Action::Native);      // unknown state
    assert(decide(false,true,1,2,true)==Action::Unblank);      // the panel's own wake never leaves it dark
    assert(decide(false,true,1,1,true)==Action::Native);
    // Another framebuffer never touches the blanked panel, whatever it asks.
    for(uint32_t current=0;current<=3;++current)
        for(uint32_t requested=0;requested<=3;++requested) {
            assert(decide(false,false,current,requested,true)==Action::Native);
            assert(decide(true,false,current,requested,true)==Action::Native);
        }

    // Blank clears only the enables and latches each plane once.
    auto io=live();State s;
    blank(io,s);
    assert(s.blanked && s.blanks==1);
    assert(io.r[PlaneCtl]==0x04001008U && io.r[PlaneSurf]==0x00200000U);
    assert(io.r[CurCtl]==0x04000000U && io.r[CurBase]==0x01230000U);
    assert((io.writes==std::vector<uint32_t>{CurCtl,CurBase,PlaneCtl,PlaneSurf}));

    // Unblank restores the enables, latching after each control write.
    io.writes.clear();
    unblank(io,s);
    assert(!s.blanked && s.unblanks==1);
    assert(io.r[PlaneCtl]==0x84001008U && io.r[CurCtl]==0x04000027U);
    assert((io.writes==std::vector<uint32_t>{PlaneCtl,PlaneSurf,CurCtl,CurBase}));

    // A donor reprogramming while dark is kept; only the enable comes back.
    io=live();State t;blank(io,t);
    io.r[PlaneCtl]=0x04003008U; io.r[PlaneSurf]=0x00400000U;
    unblank(io,t);
    assert(io.r[PlaneCtl]==0x84003008U && io.r[PlaneSurf]==0x00400000U);

    // A cursor the donor already re-enabled is not rewritten.
    io=live();State u;blank(io,u);
    io.r[CurCtl]=0x04000022U;io.writes.clear();
    unblank(io,u);
    assert((io.writes==std::vector<uint32_t>{PlaneCtl,PlaneSurf}));
    assert(io.r[CurCtl]==0x04000022U);

    // Unblank and forget release the owner; forget writes nothing.
    io=live();State w;blank(io,w);w.fb=0x1234;unblank(io,w);
    assert(!w.blanked && !w.fb && w.unblanks==1 && !w.forgets);
    blank(io,w);w.fb=0x1234;io.writes.clear();forget(w);
    assert(!w.blanked && !w.fb && w.forgets==1 && w.unblanks==1 && io.writes.empty());
    assert(io.r[PlaneCtl]==0x04001008U && io.r[CurCtl]==0x04000000U);

    // A plane that was already off stays off.
    io=live();io.r[PlaneCtl]=0x04001008U;State v;blank(io,v);unblank(io,v);
    assert(io.r[PlaneCtl]==0x04001008U);
    puts("display13_edp_blank: ok");
    return 0;
}
