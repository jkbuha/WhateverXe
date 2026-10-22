#include "kern_raptorlake_p_ring_image_core.hpp"
#include <cassert>
#include <vector>
#include <memory>
#include <cstdio>
using namespace RaptorLakeP::RingImage;
unsigned variant;
void init(void *context){
    auto f=active;assert(context==f->data.context);
    auto image=reinterpret_cast<uint32_t *>(read<uintptr_t>(f->data.shared,0x38)+4096);
    auto gpu=query(variant==1?nullptr:f->data.maps[0]);
    if(variant==2)query(f->data.maps[0]);
    image[5]=0;image[7]=0;image[9]=uint32_t(gpu);image[11]=variant==3?0:1;
    if(variant==4)write(f->data.owner,0x1200,uint32_t(1));
}
void tail(void *,uint32_t t){auto image=reinterpret_cast<uint32_t *>(read<uintptr_t>(active->data.shared,0x38)+4096);image[7]=t;}
void mode(void *owner){assert(owner==active->data.owner);if(variant==5)write(owner,0x1124,uint32_t(1));}
void hws(void *owner){assert(owner==active->data.owner);query(active->data.maps[0]);if(variant!=6)query(active->data.maps[1]);}
int main(){
 auto frame=std::make_unique<Frame>();auto copy=std::make_unique<Data>();std::vector<uint8_t> image(0x10000,0xa5);
 Methods methods{init,tail,mode,hws};
 for(unsigned engine:{0U,2U})for(uint32_t gpu:{0xa03000U,0xa28000U,0x3e18000U})for(uint32_t end:{0U,32U,48U,4088U}) {
  variant=0;assert(build(*frame,*copy,image.data(),image.size(),gpu,end,engine,methods));
 }
 for(variant=1;variant<=4;++variant)assert(!build(*frame,*copy,image.data(),image.size(),0xa03000,32,2,methods));
 variant=0;assert(!build(*frame,*copy,nullptr,image.size(),0xa03000,32,2,methods));
 assert(!build(*frame,*copy,image.data(),4096,0xa03000,32,2,methods));
 assert(!build(*frame,*copy,image.data(),image.size(),0xa03001,32,2,methods));
 assert(!build(*frame,*copy,image.data(),image.size(),0xa03000,33,2,methods));
 assert(!build(*frame,*copy,image.data(),image.size(),0xa03000,4096,2,methods));
 assert(!build(*frame,*copy,image.data(),image.size(),0xa03000,32,1,methods));
 auto missing=methods;missing.init=nullptr;assert(!build(*frame,*copy,image.data(),image.size(),0xa03000,32,2,missing));
 for(variant=0;variant<=6;++variant){bool ok=setup(*frame,*copy,0x1000000,0xa00000,2,methods);assert(ok==(variant!=5&&variant!=6));}
 active=frame.get();assert(!build(*frame,*copy,image.data(),image.size(),0xa03000,32,2,methods));assert(!setup(*frame,*copy,0x1000000,0xa00000,2,methods));active=nullptr;
 assert(!setup(*frame,*copy,0x1000001,0xa00000,2,methods));assert(!setup(*frame,*copy,0x1000000,0xa00001,2,methods));
 puts("PASS native ring helper core: ring fields, pointer scope, malformed query/owner/size/tail, mode/HWS and reentrancy cases");
}
