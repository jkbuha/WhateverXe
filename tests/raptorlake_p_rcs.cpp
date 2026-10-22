#include "kern_raptorlake_p_rcs_core.hpp"
#include <cassert>
#include <cstring>
#include <vector>
#include <map>
#include <cstdio>
using namespace RaptorLakeP::RCS;
struct IO {
    std::map<uint32_t,uint32_t> regs{{0x209c,0x200},{0x2234,1},{0xA188,0x20},{0x130044,0x40},{0xA278,0x40},{0xD84,0x80}};
    std::map<uint32_t,uint64_t> ptes;
    std::vector<uint32_t> writes;
    std::vector<uint8_t> memory=std::vector<uint8_t>(GPUBytes);
    uint64_t dma[GPUPages]{},previous[GPUPages]{};
    uint32_t slot,denied=0;unsigned reads=0,pteWrites=0,loads=0,delays=0,checks=0;
    bool execute=true,failHold=false,failRelease=false,failRenderHold=false,failRenderRelease=false,failPTE=false,failRestore=false,corrupt=false;
    unsigned changedAt=0;
    IO(uint32_t base):slot(base){for(unsigned p=0;p<GPUPages;++p){dma[p]=0x100000000ULL+8192*p;previous[p]=(0x6C800000ULL+slot+4096*p)|1;ptes[0x800000+(slot>>9)+8*p]=previous[p];}}
    bool readable(uint32_t r){return r!=denied;}
    bool displayUnchanged(){return !changedAt || ++checks<changedAt;}
    uint32_t read(uint32_t r){++reads;return regs[r];}
    uint64_t readPTE(uint32_t r){++reads;return ptes[r];}
    void writePTE(uint32_t r,uint64_t v){assert(r>=0x800000+(slot>>9) && r<0x800000+(slot>>9)+GPUPages*8);++pteWrites;if(failPTE && pteWrites==1)return;if(failRestore && pteWrites>GPUPages)return;ptes[r]=v;}
    void flushMemory(void *,uint32_t){}
    void fence(){}
    void delay(){assert(++delays<=22000);}
    uint32_t apertureRead(uint32_t off){assert(off<=GPUBytes-4);return *reinterpret_cast<uint32_t *>(memory.data()+off)^(corrupt?1U:0U);}
    void write(uint32_t r,uint32_t v){
        writes.push_back(r);
        if(r==0xA188 || r==0xA278){assert(v==0x10001 || v==0x10000);regs[r]=(regs[r]&~1U)|(v&1);auto ack=r==0xA188?0x130044:0xD84;bool holdFail=r==0xA188?failHold:failRenderHold,releaseFail=r==0xA188?failRelease:failRenderRelease;if((v&1)&&!holdFail)regs[ack]|=1;if(!(v&1)&&!releaseFail)regs[ack]&=~1U;return;}
        assert((regs[0x130044]&1) && (regs[0xD84]&1));
        assert((r>=0x2000 && r<0x2600)); // No display, render or reset writes.
        if(r==0x209c || r==0x229c){auto mask=v>>16;regs[r]=(regs[r]&~mask)|(v&mask);return;}
        regs[r]=v;
        if(r!=0x2550)return;
        ++loads;assert(v==1 && regs[0x2080]==slot);
        for(unsigned p=1;p<8;++p)assert(!regs[0x2510+8*p] && !regs[0x2514+8*p]);
        auto image=reinterpret_cast<uint32_t *>(memory.data()+2*4096);
        assert(image[1]==0x11081019 && image[33]==0x11081011);
        assert(image[52]==0x11081005 && image[65]==0x11080001 && image[66]==0x20C8 && image[67]==0x80041000);
        assert(image[81]==0x11081065 && image[185]==0x05000001 && image[23]==(0xD<<6));
        assert(image[180]==0x2068 && image[182]==0x2084);
        assert(image[9]==slot+24*4096 && image[7]==GPUTail);
        assert(image[0x31]==uint32_t(dma[25]>>32) && image[0x33]==uint32_t(dma[25]));
        if(!execute)return;
        auto ring=reinterpret_cast<uint32_t *>(memory.data()+24*4096);
        assert(ring[0]==0x10400002 && ring[4]==0x7A000004 && ring[5]==0x01104000);
        assert(ring[1]==slot+GPUResultOffset && ring[2]==0 && ring[6]==ring[1]+8 && !ring[7] && !ring[9] && !ring[10] && !ring[11]);
        *reinterpret_cast<uint32_t *>(memory.data()+ring[1]-slot)=ring[3];
        *reinterpret_cast<uint32_t *>(memory.data()+ring[6]-slot)=ring[8];
        regs[0x2030]=regs[0x2034]=GPUTail;regs[0x209c]=0x200;
        reinterpret_cast<uint32_t *>(memory.data())[0x2f]=1;
    }
    GPUResult run(bool allowed=true){return testGPU(*this,memory.data(),dma,previous,slot,allowed);}
};
int main(){
 {IO io(0xA00000);unsigned builds=0;auto reject=[&](uint8_t *,const uint64_t *,uint32_t){++builds;return false;};
  auto result=testGPU(io,io.memory.data(),io.dma,io.previous,io.slot,true,reject);
  assert(builds==1&&result.status==uint64_t(GPUStatus::BadMemory)&&io.writes.empty()&&!result.retained);}
 {IO io(0xA00000);unsigned setups=0;auto reject=[&](IO &,uint32_t base){assert(base==io.slot);++setups;return false;};
  auto result=testGPU(io,io.memory.data(),io.dma,io.previous,io.slot,true,DefaultImage{},reject);
  assert(setups==1&&result.status==uint64_t(GPUStatus::EngineBusy)&&result.stage==4&&result.retained&&!result.engineWrites&&!io.loads&&result.fwReleased);
  for(unsigned p=0;p<GPUPages;++p)assert(io.ptes[0x800000+(io.slot>>9)+8*p]==(io.dma[p]|1));}

 for(uint32_t base:{0xA00000U,0x1000000U,0x3E00000U}){
  IO io(base);auto c=io.run();assert(c.status==uint64_t(GPUStatus::Complete) && c.retained && c.fwReleased && c.renderReleased && c.slot==base);
  assert(c.result==GPUValue && c.token==GPUToken && c.head==48 && io.loads==1 && c.engineWrites==24);
  assert(io.regs[0xA188]==0x20 && io.regs[0x130044]==0x40 && io.regs[0xA278]==0x40 && io.regs[0xD84]==0x80);
 }
 {IO io(0xA00000);auto c=io.run(false);assert(!c.status && !io.reads && io.writes.empty());}
 for(uint32_t base:{0U,1U,0xA00001U,0x10000000U}){IO io(base);assert(io.run().status==uint64_t(GPUStatus::BadMemory) && !io.reads && io.writes.empty());}
 {IO io(0xA00000);io.dma[1]=io.dma[0];assert(io.run().status==uint64_t(GPUStatus::BadMemory) && !io.reads);}
 for(uint64_t dma:{0ULL,1ULL,0x8000000000ULL}){IO io(0xA00000);io.dma[3]=dma;assert(io.run().status==uint64_t(GPUStatus::BadMemory) && !io.reads);}
 {IO io(0xA00000);io.ptes[0x805000]=0;auto c=io.run();assert(c.status==uint64_t(GPUStatus::BusyGGTT) && io.writes.empty());}
 for(auto reg:{0xA188U,0x130044U,0xC000U,0x190030U,0x254cU,0x8050FCU}){IO io(0xA00000);io.denied=reg;auto c=io.run();assert(c.status==uint64_t(GPUStatus::BadMemory) && !io.reads && io.writes.empty());}
 for(auto reg:{0xA188U,0x130044U,0xA278U,0xD84U}){IO io(0xA00000);io.regs[reg]|=1;assert(io.run().status==uint64_t(GPUStatus::ForcewakeBusy) && io.writes.empty());}
 for(auto reg:{0x190030U,0x2030U,0x2034U,0x2038U,0x203cU,0x229cU,0x2548U}){IO io(0xA00000);io.regs[reg]=1;auto c=io.run();assert(c.status==uint64_t(GPUStatus::EngineBusy) && !c.engineWrites && !io.pteWrites && c.fwReleased);}
 {IO io(0xA00000);io.regs[0xC000]=0x100;auto c=io.run();assert(c.status==uint64_t(GPUStatus::EngineBusy) && !io.pteWrites);}
 {IO io(0xA00000);io.failHold=true;auto c=io.run();assert(c.status==uint64_t(GPUStatus::ForcewakeTimeout) && c.fwReleased && !c.engineWrites && io.delays==500);}
 {IO io(0xA00000);io.failRelease=true;auto c=io.run();assert(c.status==uint64_t(GPUStatus::ReleaseFailed) && c.retained && io.delays==500);}
 {IO io(0xA00000);io.execute=false;auto c=io.run();assert(c.status==uint64_t(GPUStatus::TimedOut) && c.retained && !c.result && !c.token && c.polls==20000 && io.delays==20000 && c.fwReleased);}
 for(unsigned change:{1U,2U}){IO io(0xA00000);io.changedAt=change;auto c=io.run();assert(!c.engineWrites && !c.retained && c.fwReleased);if(change==2)assert(c.pteWrites==64);}
 {IO io(0xA00000);io.failPTE=true;auto c=io.run();assert(c.pteErrors==1 && !c.engineWrites && !c.retained && c.pteWrites==64);}
 {IO io(0xA00000);io.corrupt=true;auto c=io.run();assert(c.apertureErrors && !c.engineWrites && !c.retained && c.pteWrites==64);}
 {IO io(0xA00000);io.corrupt=true;io.failRestore=true;auto c=io.run();assert(c.status==uint64_t(GPUStatus::RestoreFailed) && c.retained && !c.engineWrites);}
 {IO io(0xA00000);io.failRenderHold=true;auto c=io.run();assert(c.status==uint64_t(GPUStatus::ForcewakeTimeout) && c.fwReleased && c.renderReleased && !c.engineWrites && io.delays==500);}
 {IO io(0xA00000);io.failRenderRelease=true;auto c=io.run();assert(c.status==uint64_t(GPUStatus::ReleaseFailed) && c.retained && c.fwReleased && !c.renderReleased && io.delays==500);}
 puts("PASS dynamic RPL-P RCS packets, context, bounded timeout, ownership, unchanged display, restore and pinned lifetime");
}
