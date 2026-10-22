#include "kern_raptorlake_p_gtt_slot_core.hpp"
#include <cassert>
#include <vector>
#include <map>
#include <cstdio>
using namespace RaptorLakeP;
struct IO {
    std::vector<uint64_t> ptes=std::vector<uint64_t>(GTTShadow::Entries,0);
    std::map<uint32_t,uint32_t> regs;
    unsigned reads{},restoreWrites{},delays{},calls{};int mode{};bool invoked{};
    uint64_t original=0x6d200001;
    IO(){
        regs={{0x9138,1},{0x913c,63},{0x9134,0},{0x9118,0},{0x9140,0xe00fa},
            {0x45404,0xfc0f},{0x60400,0x8a010102},{0x70008,0xc0000000},
            {0x60000,0x081f077f},{0x6000c,0x04d204af},{0x6001c,0x077f04af},
            {0x70180,0x94000008},{0x70188,120},{0x70190,0x04af077f},
            {0x1082c0,0x70700001},{0x1082c4,0}};
        for(size_t i=0;i<0x4000;++i)ptes[i]=(0x6c800000+i*4096)|1;
        ptes[GTTSlot::Index]=original;
    }
    bool readable(uint32_t r){return r<0x1000000;}
    uint32_t read(uint32_t r){return regs[r];}
    void write(uint32_t r,uint32_t v){
        assert(r==0xa188 || r==0xa278);const auto ack=r==0xa188?0x130044:0xd84;
        regs[r]=v&0xffff;regs[ack]=v&1;
    }
    uint64_t readPTE(uint32_t off){
        assert(off>=0x800000 && off<0x1000000 && !(off&7));++reads;
        size_t i=(off-0x800000)/8;
        if(mode==1 && !invoked && i==100 && reads%2==0)return ptes[i]^2;
        if(mode==8 && restoreWrites && i==GTTSlot::Index && delays==4)ptes[i]=original;
        return ptes[i];
    }
    void restore(uint32_t off,uint64_t value){
        assert(off==0x805000 && value==original);++restoreWrites;
        if(mode!=7 && mode!=8)ptes[GTTSlot::Index]=value;
    }
    void delay(){++delays;}
};
int main(){
    const auto layout=XELPMemory::decode(0x2c1,0x6c800001,0,0x10000000);
    {IO io;assert(GTTSlot::qualify(io,layout));}
    for(unsigned r:{0x2030U,0x2034U,0x2038U,0x203cU,0x22030U,0x22034U,0x22038U,0x2203cU}){
        IO io;io.regs[r]=1;assert(!GTTSlot::qualify(io,layout));
    }
    for(unsigned r:{0x70080U,0x70280U,0x62400U}){
        IO io;io.regs[r]=0x80000027;assert(!GTTSlot::qualify(io,layout));
    }
    {IO io;io.ptes[123]^=2;assert(!GTTSlot::qualify(io,layout));}
    {IO io;io.regs[0xa188]=1;assert(!GTTSlot::qualify(io,layout));}
    for(int mode=0;mode<10;++mode){
        IO io;io.mode=mode;GTTSlot::Result out;out.qualified=1;out.firmwarePTE=io.original;
        if(mode==2)io.ptes[GTTSlot::Index]=UINT64_MAX;if(mode==9)io.ptes[GTTSlot::Index]=io.original^2;
        std::vector<uint64_t> saved(GTTShadow::Entries);
        GTTSlot::transact(io,saved.data(),0x7ef1234001,[&]{
            io.invoked=true;++io.calls;
            if(mode!=3)io.ptes[GTTSlot::Index]=mode==5?0x234001:0x7ef1234001;
            if(mode==6)io.ptes[GTTSlot::Index+1]^=1;
            return mode!=4;
        },out);
        if(mode==1 || mode==2 || mode==9){assert(out.failure && !out.invoked && !io.restoreWrites && !out.retainPins);continue;}
        assert(out.invoked && io.calls==1 && io.restoreWrites==1 && out.restoreWritten);
        if(mode==7){assert(out.failure && !out.restored && out.retainPins && out.remaining==1 && io.delays==500);}
        else if(mode==6){assert(out.failure && out.restored && out.unexpected==1 && out.remaining==1 && out.retainPins);}
        else {
            assert(out.restored && !out.remaining && !out.retainPins && io.ptes[GTTSlot::Index]==io.original);
            if(mode==0 || mode==8)assert(!out.failure);else assert(out.failure);
        }
        if(mode==8)assert(out.restorePolls==5 && io.delays==4);
    }
    {IO io;GTTSlot::Result out;GTTSlot::transact(io,nullptr,0x123001,[]{assert(false);return true;},out);assert(out.failure&&!out.invoked);}
    {IO io;GTTSlot::Result out;out.qualified=1;out.firmwarePTE=io.original;std::vector<uint64_t> saved(GTTShadow::Entries);
     GTTSlot::transact(io,saved.data(),0x7ef1234001,[]{assert(false);return true;},out,[]{return false;});
     assert(out.failure==5&&!out.invoked&&!io.restoreWrites&&!out.retainPins);}
    {IO io;GTTSlot::Result out;out.qualified=1;out.firmwarePTE=io.original;std::vector<uint64_t> saved(GTTShadow::Entries);unsigned reads=0;
     GTTSlot::transact(io,saved.data(),0x7ef1234001,[&]{io.ptes[GTTSlot::Index]=0x7ef1234001;return true;},out,[]{return true;},[&]{++reads;assert(io.ptes[GTTSlot::Index]==0x7ef1234001&&!io.restoreWrites);return false;});
     assert(reads==1&&out.failure==4&&out.restored&&!out.remaining&&!out.retainPins);}
    puts("PASS one-slot GGTT transaction: fresh GT/GOP guards, full-table snapshots, constructor failure, wrong PTE, unexpected adjacent write, bounded restore and pinned-buffer retention decisions");
}
