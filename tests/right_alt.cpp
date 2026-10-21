#include "kern_right_alt.hpp"
#include <cassert>
#include <initializer_list>
int main(){
 auto accept=[](uint32_t id=0x9A498086U,uint32_t ddi=5,bool cold=true,bool power=true,uint32_t first=0x1010,uint32_t live=0x103c,uint32_t ready=1,uint32_t pin=4,uint32_t buffer=0x80,uint32_t mode=0){return RightAlt::accept(id,ddi,cold,power,first,live,ready,pin,buffer,mode);};
 // Recorded working Linux and powered macOS bootstrap state.
 assert(accept());assert(accept(0x9A498086U,5,true,true,0x1010,0x1033));
 assert(!accept(0x9A408086U));assert(!accept(0x9A498086U,3));
 assert(!accept(0x9A498086U,5,false));assert(!accept(0x9A498086U,5,true,false));
 assert(!accept(0x9A498086U,5,true,true,0x1000));
 for(uint32_t live:{0xffffffffU,0x101cU,0x105cU,0x107cU,0x103fU,0x1030U})assert(!accept(0x9A498086U,5,true,true,0x1010,live));
 assert(!accept(0x9A498086U,5,true,true,0x1010,0x103c,0));
 assert(!accept(0x9A498086U,5,true,true,0x1010,0x103c,1,3));
 assert(!accept(0x9A498086U,5,true,true,0x1010,0x103c,1,4,0x80000000));
 assert(accept(0x9A498086U,5,true,true,0x1010,0x103c,1,4,0x80000000,3));
 assert(!accept(0x9A498086U,5,true,true,0x1010,0x103c,1,4,0x80,4));
 // Measured Linux lane selection is 3 even when physical assignment is C.
 assert(RightAlt::lanePhysical(0,2)==3);
 assert(RightAlt::lanePhysical(0xabcdeff0,2)==0xabcdeff3);
 assert(RightAlt::lanePhysical(0xabcdefff,1)==0xabcdeff1);
}
