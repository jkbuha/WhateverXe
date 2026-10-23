#include "kern_raptorlake_p_reference_backlight.hpp"
#include <cassert>
#include <cstring>
#include <array>
#include <vector>
using RaptorLakeP::ReferenceBacklight::image;
struct IO{std::vector<uint32_t>reads;bool bad=false;uint32_t read(uint32_t r){reads.push_back(r);return bad?UINT32_MAX:r;}};
int main(){
 IO io;auto off=RaptorLakeP::ReferenceBacklight::capture(io,false);assert(off.status==1&&io.reads.empty());
 auto on=RaptorLakeP::ReferenceBacklight::capture(io,true);assert(on.status==0&&on.control==0xC8250&&on.period==0xC8254&&on.duty==0xC8258);assert((io.reads==std::vector<uint32_t>{0xC8250,0xC8254,0xC8258}));
 io.bad=true;assert(RaptorLakeP::ReferenceBacklight::capture(io,true).status==2);

 std::array<uint8_t,80>b{};auto put=[&](unsigned o,uint32_t v){memcpy(b.data()+o,&v,4);};
 put(0,0xFEEDFACF);put(4,0x1000007);put(16,1);put(20,24);put(32,0x1B);put(36,24);
 const uint8_t uuid[]{0xF9,0x9E,0xE2,0x3A,0xBD,0x38,0x3E,0x4E,0xA2,0x9A,0x16,0xA5,0x93,0x9E,0x2F,0xB3};memcpy(b.data()+40,uuid,16);
 assert(image(b.data(),56));assert(image(b.data(),b.size()));
 for(unsigned i=0;i<56;++i)assert(!image(b.data(),i));
 assert(!image(nullptr,80));for(unsigned i=40;i<56;++i){b[i]^=1;assert(!image(b.data(),80));b[i]^=1;}
 for(unsigned o:{0U,4U,16U,20U,32U,36U}){auto saved=b;put(o,0xFFFFFFFF);assert(!image(b.data(),80));b=saved;}
 auto saved=b;put(16,2);put(20,48);memcpy(b.data()+56,b.data()+32,24);assert(!image(b.data(),80));b=saved;
 put(36,0);assert(!image(b.data(),80));b=saved;put(16,0);assert(!image(b.data(),80));
}
