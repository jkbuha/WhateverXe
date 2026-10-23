#include "kern_raptorlake_p_reference_fb_core.hpp"
#include <cassert>
#include <cstdint>
#include <initializer_list>
#include <cstring>
using namespace RaptorLakeP::ReferenceFB;
int main(){
 uint8_t image[80]{};auto put=[&](unsigned p,uint32_t v){memcpy(image+p,&v,4);};
 put(0,0xFEEDFACF);put(4,0x1000007);put(16,1);put(20,24);put(32,0x1B);put(36,24);
 const uint8_t uuid[]{0x7F,0xA0,0x03,0x6E,0x6A,0x61,0x31,0x02,0x98,0x89,0xA0,0x60,0x1F,0x82,0xE3,0xC6};memcpy(image+40,uuid,16);
 assert(compatImage(image,56));assert(!compatImage(nullptr,56));
 for(unsigned bytes=0;bytes<56;++bytes)assert(!compatImage(image,bytes));
 for(unsigned p:{0u,4u,16u,20u,32u,36u,40u,55u}){image[p]^=0x80;assert(!compatImage(image,56));image[p]^=0x80;}
 put(16,2);put(20,48);memcpy(image+56,image+32,24);assert(!compatImage(image,80));put(16,1);put(20,24);
 for(unsigned d=0;d<9;++d)assert(portRecord(0x1000,0x1020+24*d));
 assert(!portRecord(0,0x20));assert(!portRecord(UINT64_MAX-16,0));assert(!portRecord(0x1000,0x1018));assert(!portRecord(0x1000,0x1021));assert(!portRecord(0x1000,0x10F8));
 for(unsigned d=0;d<2;++d){assert(portMode(d,0)==1);assert(portMode(d,UINT32_MAX)==1);}
 for(unsigned d=2;d<=5;++d){auto bit=6+8*(d-2);assert(portMode(d,0)==0);assert(portMode(d,1U<<bit)==2);assert(portMode(d,1U<<(bit-1))==3);assert(portMode(d,(1U<<bit)|(1U<<(bit-1)))==2);assert(portMode(d,UINT32_MAX)==0);}
 for(unsigned d:{6u,8u,9u,UINT32_MAX})assert(portMode(d,0x7FFFFFFF)==0);

 for(unsigned d=0;d<10;++d)for(unsigned status=0;status<256;++status){
  auto reg=adlpTCSSRegister(d);assert(reg==(d>=3&&d<=6?0x161500+4*(d-3):0));
  assert(adlpPortMode(d,status)==(d<2?1:reg&&(status&4)&&(status&3)==1?3:0));
 }
 for(unsigned d=2;d<10;++d)assert(!adlpPortMode(d,UINT32_MAX));
 assert(adlpPortMode(5,0x35)==3&&adlpTCSSRegister(5)==0x161508);
 assert(!adlpPortMode(5,6)&&!adlpPortMode(5,7)); // unqualified TBT / ambiguous status

 for(unsigned width:{4u,8u}){
  assert(range(0,width,0x1000000));assert(range(0x1000000-width,width,0x1000000));
  assert(!range(0x1000000-width+1,width,0x1000000));assert(!range(UINT64_MAX,width,0x1000000));
  assert(!range(0,width,width-1));assert(!range(1,width,0x1000000));
 }
 for(unsigned width:{0u,1u,2u,3u,16u,UINT32_MAX})assert(!range(0,width,0x1000000));
 const uint64_t m=0xffffff8001000000ULL;
 assert(window(m+0x800000,0x7ffff8,8,m));assert(!window(m+0x800000,0x800000,8,m));
 assert(!window(m-8,0,8,m));assert(!window(m+4,0,8,m));assert(!window(m,UINT64_MAX,8,m));assert(!window(m,0,8,0));
 assert(dsb(0x4180000,0x12000,4096));assert(dsb(0x1000000,0x12000,4096));assert(dsb(0x100000000ULL-0x12000,0x12000,4096));
 for(uint64_t off:{uint64_t(0),uint64_t(0xFFF000),uint64_t(0x1000001),uint64_t(0x100000000ULL),UINT64_MAX})assert(!dsb(off,0x12000,4096));
 assert(!dsb(0x4180000,0x13000,4096));assert(!dsb(0x4180000,0x12000,16384));
}
