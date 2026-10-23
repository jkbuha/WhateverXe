#include "kern_raptorlake_p_reference_early.hpp"
#include <cassert>
#include <array>
#include <cstring>
using namespace RaptorLakeP::ReferenceEarly;
int main(){
 for(unsigned b=0;b<32;++b)assert(block(b&1,b&2,b&4,b&8,b&16)==(b==31));
 std::array<uint8_t,80> v{};auto put=[&](unsigned o,uint32_t n){memcpy(v.data()+o,&n,4);};
 put(0,0xFEEDFACF);put(4,0x1000007);put(16,1);put(20,24);put(32,0x1B);put(36,24);
 const uint8_t uuid[]{0x7C,0xD8,0xB8,0xF3,0xAC,0xFE,0x31,0x04,0x80,0x9B,0x93,0xF0,0xB5,0x73,0x6E,0x69};memcpy(v.data()+40,uuid,16);
 assert(image(v.data(),56));assert(!image(nullptr,80));
 for(unsigned n=0;n<56;++n)assert(!image(v.data(),n));
 for(unsigned i=40;i<56;++i){v[i]^=1;assert(!image(v.data(),80));v[i]^=1;}
 for(unsigned o:{0u,4u,16u,20u,32u,36u}){auto copy=v;put(o,UINT32_MAX);assert(!image(v.data(),80));v=copy;}
 auto copy=v;put(16,2);put(20,48);memcpy(v.data()+56,v.data()+32,24);assert(!image(v.data(),80));v=copy;
 put(36,0);assert(!image(v.data(),80));
}
