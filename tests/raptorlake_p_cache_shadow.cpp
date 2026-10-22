#include "kern_raptorlake_p_cache_shadow_core.hpp"
#include "kern_raptorlake_p.hpp"
#include <cassert>
#include <vector>
#include <cstdio>
using namespace RaptorLakeP::CacheShadow;
int main() {
    uint8_t pat[8]{3,0,3,3,3,3,3,3};uint32_t l3[32],mocs[64];
    for(unsigned i=0;i<32;++i)l3[i]=0x100010+i;
    for(unsigned i=0;i<64;++i)mocs[i]=0x51+i;
    std::vector<uint32_t> ram((Bytes+2*Guard)/4,Sentinel);auto memory=ram.data()+Guard/4;
    for(size_t i=0;i<Count;++i){assert(!(offset(i)&3)&&offset(i)<Bytes);for(size_t j=0;j<i;++j)assert(offset(i)!=offset(j));memory[offset(i)/4]=expected(i,pat,l3,mocs);}
    auto check=verify(ram.data(),pat,l3,mocs);assert(check.changed==108&&!check.unexpected&&!check.guards&&check.firstMismatch==UINT64_MAX);
    memory[0x4024/4]=0xF0007FFF;check=verify(ram.data(),pat,l3,mocs);assert(check.unexpected==1&&check.firstMismatch==0x4024);
    memory[0x4024/4]=mocs[9];memory[0x7770/4]=0;ram[0]^=1;ram.back()^=1;
    check=verify(ram.data(),pat,l3,mocs);assert(check.changed==109&&check.unexpected==1&&check.firstMismatch==0x7770&&check.guards==2);
    Wake wake;assert(wake.accept(true,true,1,5)&&wake.accept(true,true,0,5)&&!wake.failure);
    for(unsigned mode=0;mode<7;++mode){Wake w;if(mode==4||mode==5)assert(w.accept(true,true,1,5));if(mode==5)assert(w.accept(true,true,0,5));
        assert(!w.accept(mode!=0,mode!=1,mode==3?2:mode==6?0:1,mode==2?4:5)&&w.failure);}
    puts("PASS cache RAM full-buffer, 108 unique final words/overwrite, unexpected write/redzones, forcewake ABI/order/refusal");
}
