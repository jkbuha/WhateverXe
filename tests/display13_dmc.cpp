#include "kern_display13_dmc.hpp"
#include <cassert>
#include <map>
#include <set>
#include <vector>
// Register model for the main DMC load. Program RAM can drop writes (an
// unpowered well) or alter one word; any register can be write-denied.
struct IO {
    std::map<uint32_t,uint32_t> r;std::set<uint32_t> deny;std::vector<uint32_t> order;
    bool dropRam=false;uint32_t corrupt=0;
    uint32_t read(uint32_t reg) {auto it=r.find(reg);return it==r.end()?0:it->second;}
    bool write(uint32_t reg,uint32_t v) {
        if(deny.count(reg))return false;
        order.push_back(reg);
        if(reg>=Display13DMC::Start && reg<=Display13DMC::RamEnd) {
            if(dropRam)return true;
            if(corrupt && reg==corrupt)v^=1;
        }
        r[reg]=v;return true;
    }
};
template<class F> Display13DMC::Result run(IO &io,bool force,F) {
    return Display13DMC::load([&](uint32_t g){return io.read(g);},[&](uint32_t g,uint32_t v){return io.write(g,v);},force);
}
static Display13DMC::Result load(IO &io,bool force=false) {return run(io,force,0);}

int main() {
    using namespace Display13DMC;
    const auto *p=mainProgram();
    assert(valid(p) && p->start==0x80000 && p->words==6274 && p->payload[0]==0x0C0A4040 && p->mmioCount==7);
    assert(p->mmio[0].reg==StackBase && p->mmio[0].value==0x00086FC0);
    assert(p->mmio[1].reg==EventControl && p->mmio[1].value==0xC003B400);
    // Only the main program is ever selected; the pipe programs live elsewhere.
    for(const auto &q:Display13DMCFirmware::programs)if(q.id)assert(!valid(&q));

    // Fresh load: handlers off first, then RAM, then MMIO list, then DC_STATE_DEBUG.
    IO io;io.r[DCStateEnable]=0;io.r[DCStateDebug]=0x10;io.r[ChickenDCPR2]=0x16;
    auto r=load(io);
    assert(r.status==Status::Complete && r.stage==8);
    assert(r.writes==2*Handlers+p->words+p->mmioCount+2);
    assert(io.order[0]==EventControl && io.order[1]==HandlerTable);
    assert(io.order[2*Handlers]==Start && io.order[2*Handlers+p->words]==p->mmio[0].reg);
    assert(io.order[io.order.size()-2]==DCStateDebug && io.r[DCStateDebug]==0x13);
    assert(io.order.back()==ChickenDCPR2 && io.r[ChickenDCPR2]==0x0F000016);
    assert(r.debugBefore==0x10 && r.debugAfter==0x13 && r.stackBase==0x00086FC0 && r.control0==0xC003B400);
    assert(loaded([&](uint32_t g){return io.read(g);},p));

    // Second call without force is a no-op; force reloads.
    io.order.clear();r=load(io);
    assert(r.status==Status::AlreadyLoaded && io.order.empty() && !r.writes);
    r=load(io,true);assert(r.status==Status::Complete && !io.order.empty());

    // Lost image (DC9, S4): first word gone -> reloaded.
    io.r[Start]=0;io.order.clear();r=load(io);
    assert(r.status==Status::Complete && io.r[Start]==p->payload[0]);
    // Debug masks cleared alone -> reload sets them again.
    io.r[DCStateDebug]=0;r=load(io);assert(r.status==Status::Complete && io.r[DCStateDebug]==3);
    // Workaround bits lost alone -> reload restores them, other bits kept.
    io.r[ChickenDCPR2]=0x16;r=load(io);assert(r.status==Status::Complete && io.r[ChickenDCPR2]==0x0F000016);
    IO deniedWa;deniedWa.deny.insert(ChickenDCPR2);
    r=load(deniedWa);assert(r.status==Status::Denied && r.stage==7);

    // Unpowered RAM: nothing reads back -> handlers left disabled, MMIO list never written.
    IO dead;dead.dropRam=true;
    r=load(dead);
    assert(r.status==Status::Verify && r.stage==5 && r.mismatchIndex==0 && r.expected==p->payload[0]);
    assert(!dead.r.count(StackBase) && dead.r[EventControl]==EventDisabled && dead.r[HandlerTable]==0);

    // One corrupted word in the middle -> refused at that index.
    IO bad;bad.corrupt=Start+4*100;
    r=load(bad);
    assert(r.status==Status::Verify && r.mismatchIndex==100 && r.observed==(p->payload[100]^1));
    for(uint32_t h=0;h<Handlers;++h)assert(bad.r[EventControl+4*h]==EventDisabled && bad.r[HandlerTable+4*h]==0);
    assert(!bad.r.count(StackBase));

    // A denied register stops the sequence where it is.
    IO denied;denied.deny.insert(Start+8);
    r=load(denied);assert(r.status==Status::Denied && r.stage==4 && !denied.r.count(StackBase));
    IO deniedDebug;deniedDebug.deny.insert(DCStateDebug);
    r=load(deniedDebug);assert(r.status==Status::Denied && r.stage==7);

    // Dead BAR reads.
    IO gone;gone.r[DCStateEnable]=UINT32_MAX;
    r=load(gone);assert(r.status==Status::Readback && r.stage==1 && gone.order.empty());

    // Disable-only path (engine stop) writes exactly the 16 handler registers.
    IO stop;
    assert(disableHandlers([&](uint32_t g,uint32_t v){return stop.write(g,v);})==2*Handlers && stop.order.size()==2*Handlers);
    // DC6 arming: only after a verified load, preserves other bits, refuses a held DC9.
    IO dc;dc.r[DCStateEnable]=0x10;
    r=Display13DMC::load([&](uint32_t g){return dc.read(g);},[&](uint32_t g,uint32_t v){return dc.write(g,v);},false,true);
    assert(r.status==Status::Complete && r.stage==9 && dc.r[DCStateEnable]==0x12 && dc.order.back()==DCStateEnable);
    dc.order.clear();
    r=Display13DMC::load([&](uint32_t g){return dc.read(g);},[&](uint32_t g,uint32_t v){return dc.write(g,v);},false,true);
    assert(r.status==Status::AlreadyLoaded && dc.order.empty());
    assert(clearDCStates([&](uint32_t g){return dc.read(g);},[&](uint32_t g,uint32_t v){return dc.write(g,v);}) && dc.r[DCStateEnable]==0x10);
    IO dc9;dc9.r[DCStateEnable]=8;
    r=Display13DMC::load([&](uint32_t g){return dc9.read(g);},[&](uint32_t g,uint32_t v){return dc9.write(g,v);},false,true);
    assert(r.status==Status::Denied && r.stage==9 && dc9.r[DCStateEnable]==8);
    IO dcDead;dcDead.dropRam=true;
    r=Display13DMC::load([&](uint32_t g){return dcDead.read(g);},[&](uint32_t g,uint32_t v){return dcDead.write(g,v);},false,true);
    assert(r.status==Status::Verify && !dcDead.r[DCStateEnable]);
    return 0;
}
