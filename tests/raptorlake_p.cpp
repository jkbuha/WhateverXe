#include "kern_raptorlake_p.hpp"
#include <cassert>
#include <map>
#include <vector>
#include <cstdio>
using namespace RaptorLakeP;
struct IO {
    std::map<uint32_t,uint32_t> regs{{0xA188,0x20},{0xA278,0x40},{0x130044,0},{0xD84,0},
        {0x9138,1},{0x913C,0x3f},{0x9134,0},{0x9118,0x10000000},{0x9140,0xE00FC}};
    unsigned delays=0,reads=0,payload=0;int failHold=-1,failRelease=-1,inaccessible=-1;
    uint32_t unreadable=0;std::vector<uint32_t> writes;
    bool readable(uint32_t r){return r!=unreadable;}
    uint32_t read(uint32_t r){
        ++reads;
        if(!writes.empty() && ((inaccessible==0 && r==0x130044) || (inaccessible==1 && r==0xD84)))return UINT32_MAX;
        if(r!=0xA188 && r!=0xA278 && r!=0x130044 && r!=0xD84){
            assert((regs[0x130044]&1) && (regs[0xD84]&1));++payload;
        }
        return regs[r];
    }
    void write(uint32_t r,uint32_t value){
        assert(r==0xA188 || r==0xA278);assert(value==0x10001 || value==0x10000);writes.push_back(r);
        unsigned d=r==0xA188?0:1;auto a=d?0xD84:0x130044;
        regs[r]=(regs[r]&~1U)|(value&1);
        if(value&1){if(int(d)==inaccessible)regs[a]=UINT32_MAX;else if(int(d)!=failHold)regs[a]|=1;}
        else if(int(d)!=failRelease)regs[a]&=~1U;
    }
    void delay(){assert(++delays<=2000);}
};
int main(){
    auto p=XELP::identify(0xA7A08086,4);
    assert(eligible(p,XELP::OS::Tahoe,true));assert(!p.admitted());
    assert(!eligible(p,XELP::OS::Tahoe,false));assert(!eligible(p,XELP::OS::BigSur,true));
    for(unsigned rev=0;rev<256;++rev)assert(eligible(XELP::identify(0xA7A08086,rev),XELP::OS::Tahoe,true)==(rev==4));
    for(auto entry:XELP::DeviceTable)if(entry.device!=0xA7A0)assert(!eligible(XELP::identify((uint32_t(entry.device)<<16)|0x8086,4),XELP::OS::Tahoe,true));
    assert(!eligible(XELP::identify(0xA7A08086,4,0x9A498086),XELP::OS::Tahoe,true));
    p.gt=XELP::GT::TGLGT2;assert(!eligible(p,XELP::OS::Tahoe,true));
    Resources r;assert(resources(1,0x3f,0,r) && r.eus==96 && r.dss==6 && r.nativeSubslices==12);
    assert(resources(1,0x15,0xF0,r) && r.eus==24);
    assert(resources(0x10001,0x3f,0x100,r) && r.eus==96); // unrelated fuse fields are not EU bits
    for(auto d:{0U,0x40U,UINT32_MAX})assert(!resources(1,d,0,r));
    assert(!resources(2,0x3f,0,r));assert(!resources(1,0x3f,0xff,r));assert(!resources(1,0x3f,UINT32_MAX,r));
    {IO io;auto c=capture(io,false);assert(c.status==Status::Skipped && !io.reads && io.writes.empty());}
    {IO io;auto c=capture(io,true);assert(c.status==Status::Complete && c.topology.eus==96 && c.count==16 && c.writes==4 && c.releasedMask==3 && !c.releaseFailedMask);assert(io.regs[0xA188]==0x20 && io.regs[0xA278]==0x40);assert(io.writes==std::vector<uint32_t>({0xA188,0xA278,0xA278,0xA188}));}
    for(auto reg:{0xA188U,0xA278U,0x130044U,0xD84U}){
        {IO io;io.regs[reg]|=1;assert(capture(io,true).status==Status::ExistingOwner && io.writes.empty() && !io.payload);}
        {IO io;io.regs[reg]=UINT32_MAX;assert(capture(io,true).status==Status::Unavailable && io.writes.empty());}
    }
    for(auto reg:{0xA188U,0x130044U,0x9138U,0x2229CU}){IO io;io.unreadable=reg;assert(capture(io,true).status==Status::Unavailable && !io.reads && io.writes.empty());}
    for(int d=0;d<2;++d){
        {IO io;io.failHold=d;auto c=capture(io,true);assert(c.status==Status::AckTimeout && c.releasedMask==3 && c.writes==4 && io.delays==500 && !io.payload);}
        {IO io;io.failRelease=d;auto c=capture(io,true);assert(c.status==Status::ReleaseFailed && c.releaseFailedMask==(1U<<d) && io.delays==500 && c.writes==4);}
        {IO io;io.inaccessible=d;auto c=capture(io,true);assert(c.status==Status::ReleaseFailed && c.writes==4 && !io.payload);}
    }
    {IO io;io.failHold=0;io.failRelease=1;auto c=capture(io,true);assert(c.status==Status::ReleaseFailed && io.delays==1000 && c.releasedMask==1);}
    {IO io;io.regs[0x913C]=0x40;auto c=capture(io,true);assert(c.status==Status::TopologyUnknown && c.releasedMask==3 && !c.topology.eus);}
    for(auto reg:{0x9138U,0x913CU,0x9134U,0x9118U,0x9140U,0x2030U,0x2229CU}){IO io;io.regs[reg]=UINT32_MAX;auto c=capture(io,true);assert(c.status==Status::Unavailable && c.releasedMask==3 && c.writes==4);}
    puts("PASS RPL-P diagnostic gates, measured topology, forcewake ownership, bounded acquisition and cleanup failures");
}
