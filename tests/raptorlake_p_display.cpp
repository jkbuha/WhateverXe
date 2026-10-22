#include "kern_raptorlake_p_display.hpp"
#include "kern_xelp_geometry.hpp"
#include <cassert>
#include <map>
#include <vector>
#include <cstdio>
using namespace RaptorLakeP;
// Deliberately no write() method: the production core cannot mutate hardware.
struct IO {
    std::map<uint32_t,uint32_t> regs;
    std::vector<uint32_t> reads;uint32_t denied=0;unsigned powers=0,changeAt=0;
    bool readable(uint32_t reg){return reg!=denied;}
    uint32_t read(uint32_t reg){
        reads.push_back(reg);
        if(reg==0x45404){++powers;return changeAt && powers>=changeAt ? 0 : regs[reg];}
        unsigned pipe=reg>=0x70000?(reg-0x70000)/0x1000:(reg-0x60000)/0x1000;
        assert(pipe<4 && (regs[0x45404]&(1U<<(2*(5+pipe)))));
        return regs[reg];
    }
    void setup(unsigned mask,unsigned active){
        for(unsigned pipe=0;pipe<4;++pipe){
            if(mask&(1U<<pipe))regs[0x45404]|=1U<<(2*(5+pipe));
            unsigned off=0x1000*pipe;
            regs[0x60400+off]=(active&(1U<<pipe))?0x88000000:0;
            regs[0x70180+off]=0x82000000;regs[0x70188+off]=120;
            regs[0x70190+off]=(1199U<<16)|1919;
            regs[0x60000+off]=(2079U<<16)|1919;
            regs[0x6000C+off]=(1234U<<16)|1199;
        }
    }
};
int main(){
    {IO io;assert(captureDisplay(io,false).status==DisplayStatus::Skipped && io.reads.empty());}
    for(unsigned mask=0;mask<16;++mask)for(unsigned active=0;active<16;++active){
        IO io;io.setup(mask,active);auto c=captureDisplay(io,true);
        assert(c.status==DisplayStatus::Complete && c.poweredPipes==mask && c.activePipes==(mask&active));
        assert(c.count==2+__builtin_popcount(mask)+22*__builtin_popcount(mask&active));
        assert(c.count<=98 && c.powerBefore==c.powerAfter);
    }
    for(auto denied:{0x45404U,0x60400U,0x63400U,0x731ACU}){IO io;io.denied=denied;assert(captureDisplay(io,true).status==DisplayStatus::Unavailable && io.reads.empty());}
    {IO io;io.regs[0x45404]=UINT32_MAX;auto c=captureDisplay(io,true);assert(c.status==DisplayStatus::Unavailable && io.reads.size()==1);}
    {IO io;io.setup(1,1);io.regs[0x60400]=UINT32_MAX;auto c=captureDisplay(io,true);assert(c.status==DisplayStatus::Unavailable && c.count==2);}
    {IO io;io.setup(1,1);io.regs[0x70188]=UINT32_MAX;auto c=captureDisplay(io,true);assert(c.status==DisplayStatus::Unavailable && io.reads.back()==0x70188);}
    {IO io;io.setup(1,1);io.changeAt=2;auto c=captureDisplay(io,true);assert(c.status==DisplayStatus::PowerChanged && c.count==1 && !c.activePipes);}
    {IO io;io.setup(1,1);io.changeAt=3;auto c=captureDisplay(io,true);assert(c.status==DisplayStatus::PowerChanged && c.activePipes==1);}
    {IO io;io.setup(8,8);auto c=captureDisplay(io,true);assert(c.activePipes==8); // Pipe D is independently captured.
        auto g=XELPGeometry::linear(io.regs[0x73180],io.regs[0x73188],io.regs[0x73190],0,0);
        assert(g.valid && g.width==1920 && g.height==1200 && g.pitch==7680);
        assert(XELPGeometry::timing(g,io.regs[0x63000],io.regs[0x6300C]));
    }
    puts("PASS read-only four-pipe Display13 snapshot: power gating, inactive pipes, changing power and panel geometry");
}
