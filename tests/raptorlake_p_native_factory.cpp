#include "kern_raptorlake_p_native_factory_core.hpp"
#include <cassert>
#include <initializer_list>
using namespace RaptorLakeP::NativeFactory;
struct IO {uint32_t a,b;unsigned reads=0;uint32_t read(){return reads++?b:a;}};
int main(){
    for(uint32_t v:{0U,2U,0x102U,0xfffffffeU}){IO io{v,v};Capability c;assert(absent(io,c)&&io.reads==2&&c.first==v&&c.second==v);}
    for(uint32_t v:{1U,3U,0xffffffffU}){IO io{v,v};Capability c;assert(!absent(io,c)&&io.reads==2);}
    for(uint32_t v:{1U,2U,0xffffffffU}){IO io{0,v};Capability c;assert(!absent(io,c)&&io.reads==2);}
}
