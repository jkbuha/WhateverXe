#include "kern_raptorlake_p_reference_core.hpp"
#include <cassert>
#include <initializer_list>
int main(){
 for(unsigned bits=0;bits<16;++bits){bool ref=bits&1,cap=bits&2,panel=bits&4,manual=bits&8;assert(RaptorLakeP::Reference::requested(ref,cap,panel,manual)==(manual&&(ref||(cap&&panel))));}
using namespace RaptorLakeP::Reference;
 auto p=XELP::identify(0xA7A08086,4);auto m=XELPMemory::decode(0x2C1,0x6C800000,0);
 assert(physical(p,XELP::OS::Tahoe,m));
 for(auto id:{0x9A498086u,0x46D18086u,0xA7A18086u,0xA7A08087u})assert(!physical(XELP::identify(id,4),XELP::OS::Tahoe,m));
 for(unsigned rev:{0u,3u,5u,255u})assert(!physical(XELP::identify(0xA7A08086,rev),XELP::OS::Tahoe,m));
 for(auto os:{XELP::OS::BigSur,XELP::OS::Sequoia,XELP::OS::Unknown})assert(!physical(p,os,m));
 auto changed=m;changed.bytes*=2;assert(!physical(p,XELP::OS::Tahoe,changed));changed=m;changed.gttBytes/=2;assert(!physical(p,XELP::OS::Tahoe,changed));
 changed=m;changed.apertureBytes/=2;assert(!physical(p,XELP::OS::Tahoe,changed));changed=m;changed.valid=false;assert(!physical(p,XELP::OS::Tahoe,changed));
 for(unsigned bits=0;bits<256;++bits){uint32_t s[9]{1};for(unsigned i=1;i<9;++i)s[i]=(bits>>(i-1))&1;
  assert(armable(s)==(bits==0x97));}
 uint32_t s[9]{1,1,1,1,0,1,0,0,1};assert(armable(s));s[0]=0;assert(!armable(s));s[0]=2;assert(!armable(s));
}
