#include "kern_raptorlake_p_blit_pitch.hpp"
#include <cassert>
using RaptorLakeP::BlitPitch::divert;
int main(){
 assert(divert(true,1,642ULL*512,656,656));
 assert(!divert(false,1,642ULL*512,656,656));
 assert(!divert(true,1,642ULL*128,656,656));
 assert(!divert(true,1,642ULL*512,640,640));
 assert(divert(true,4,165ULL*1024,672,672));
 assert(!divert(true,1,UINT64_MAX,0,656));
 for(unsigned b=0;b<256;++b){
  const bool known=b==1||b==2||b==4||b==8||b==16;
  assert(divert(true,b,1024ULL*1024,656,656)==known);
  if(b>1)assert(!divert(true,b,UINT64_MAX,656,656));
 }
 for(unsigned p=0;p<=65536;++p){
  const bool accepted=p>0&&p<=32766&&p%4==0&&p%64!=0;
  assert(divert(true,1,642ULL*512,p,640)==accepted);
  assert(divert(true,1,642ULL*512,640,p)==accepted);
 }
 assert(!divert(true,16,0x51000/16,656,656));
 assert(divert(true,16,0x51000/16+1,656,656));
}
