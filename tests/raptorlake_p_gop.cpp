#include "kern_raptorlake_p_gop.hpp"
#include <cassert>
#include <map>
#include <cstdio>
using namespace RaptorLakeP;
struct IO {
    unsigned reads=0,pteReads=0;uint32_t denied=0;bool changed=false;unsigned highs=0;
    uint64_t reserved=0x6E700001,base=0x6A800000,mismatch=UINT64_MAX;
    bool readable(uint32_t reg){return reg!=denied;}
    uint32_t read(uint32_t reg){++reads;assert(reg==0x1082C0 || reg==0x1082C4);if(reg==0x1082C4){++highs;return changed && highs==2?1:uint32_t(reserved>>32);}return uint32_t(reserved);}
    uint64_t readPTE(uint32_t reg){assert(!(reg&7) && reg>=0x800000 && reg<0x820000);++pteReads;uint64_t offset=uint64_t((reg-0x800000)/8)*4096;return offset==mismatch?0:(base+offset)|1;}
};
static DisplayCapture panel(){
 DisplayCapture d;d.status=DisplayStatus::Complete;d.poweredPipes=7;d.activePipes=1;
 const Register regs[]{{0x60400,0x8A010102},{0x70008,0xC0000000},{0x60000,0x081F077F},{0x6000C,0x04D204AF},
 {0x6001C,0x077F04AF},{0x70180,0x94000008},{0x70188,120},{0x70190,0x04AF077F},
 {0x7018C,0},{0x701A4,0},{0x7019C,0},{0x701AC,0},{0x70080,0},{0x70280,0},{0x70380,0},{0x70480,0},{0x70580,0},{0x70680,0},{0x70780,0}};
 for(auto reg:regs)d.registers[d.count++]=reg;return d;
}
static void set(DisplayCapture &d,uint32_t reg,uint32_t v){for(unsigned i=0;i<d.count;++i)if(d.registers[i].offset==reg){d.registers[i].value=v;return;}assert(false);}
int main(){
 auto d=panel();auto m=XELPMemory::decode(0x2C1,0x6A800001,0);
 assert(m.valid && m.bytes==0x4000000);
 {IO io;auto c=captureGOP(io,d,m,false);assert(c.status==GOPStatus::Skipped && !io.reads && !io.pteReads);}
 {IO io;auto c=captureGOP(io,d,m,true);assert(c.status==GOPStatus::Complete && c.verifiedPages==16384 && io.pteReads==16384);assert(c.width==1920 && c.height==1200 && c.pitch==7680 && c.gopEnd==9216000 && c.candidate==0xA00000 && c.usable==0x3F00000);}
 {IO io;auto bad=d;bad.version=1;assert(captureGOP(io,bad,m,true).status==GOPStatus::Incomplete && !io.reads);}
 {IO io;auto bad=d;bad.count=99;assert(captureGOP(io,bad,m,true).status==GOPStatus::Incomplete && !io.reads);}
 for(unsigned active:{0U,2U,3U,8U}){IO io;auto bad=d;bad.activePipes=active;assert(captureGOP(io,bad,m,true).status==GOPStatus::NotPanel && !io.reads);}
 {IO io;auto bad=d;set(bad,0x60400,0x90000000);assert(captureGOP(io,bad,m,true).status==GOPStatus::NotPanel && !io.reads);}
 for(unsigned plane=2;plane<=7;++plane){IO io;auto bad=d;set(bad,0x70080+0x100*plane,0x80000000);assert(captureGOP(io,bad,m,true).status==GOPStatus::OtherScanout && !io.reads);}
 {IO io;auto bad=d;set(bad,0x70080,0x27);assert(captureGOP(io,bad,m,true).status==GOPStatus::OtherScanout && !io.reads);}
 for(auto reg:{0x70188U,0x70190U,0x60000U,0x6000CU,0x6001CU,0x70180U}){IO io;auto bad=d;set(bad,reg,0);assert(captureGOP(io,bad,m,true).status==GOPStatus::Geometry && !io.reads);}
 {IO io;auto bad=d;set(bad,0x701AC,4096);assert(captureGOP(io,bad,m,true).status==GOPStatus::Geometry && !io.reads);}
 {IO io;auto bad=d;bad.registers[bad.count++]=bad.registers[0];assert(captureGOP(io,bad,m,true).status==GOPStatus::NotPanel && !io.reads);}
 {IO io;auto bad=m;bad.valid=false;assert(captureGOP(io,d,bad,true).status==GOPStatus::Memory && !io.reads);}
 {IO io;auto bad=m;bad.bytes=0x10001000;assert(captureGOP(io,d,bad,true).status==GOPStatus::Memory && !io.reads);}
 for(auto reg:{0x1082C0U,0x1082C4U}){IO io;io.denied=reg;assert(captureGOP(io,d,m,true).status==GOPStatus::Memory && !io.reads);}
 {IO io;io.changed=true;assert(captureGOP(io,d,m,true).status==GOPStatus::Reservation && !io.pteReads);}
 for(uint64_t reserved:{0ULL,0x6A000001ULL,0x6A900001ULL,UINT64_MAX}){IO io;io.reserved=reserved;assert(captureGOP(io,d,m,true).status==GOPStatus::Reservation && !io.pteReads);}
 for(uint64_t offset:{0ULL,0x8C9000ULL,0xA00000ULL,0x3FFF000ULL}){IO io;io.mismatch=offset;auto c=captureGOP(io,d,m,true);assert(c.status==GOPStatus::MappingMismatch && c.mismatchOffset==offset && c.verifiedPages==offset/4096 && io.pteReads==offset/4096+1);}
 {IO io;io.denied=0x81FFFC;assert(captureGOP(io,d,m,true).status==GOPStatus::Reservation && !io.pteReads);}
 puts("PASS passive RPL-P GOP qualification: WUXGA geometry, secondary scanouts, 64MiB bounds, reservations and full firmware GGTT prefix");
}
