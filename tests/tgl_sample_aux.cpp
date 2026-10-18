#include "../WhateverXe/kern_tgl_ppgtt.hpp"
#include "../WhateverXe/kern_tgl_hang_refs.hpp"
#include "../WhateverXe/kern_tgl_sample_aux.hpp"
#include <cassert>
#include <initializer_list>
using C=TGLSampleAux::Capture;
static_assert(sizeof(C)==26232);
int main(){
 for(unsigned mode=0;mode<8;++mode){C c{};auto q=c.q;q[2]=0x40150000;q[3]=0x40a38000;q[4]=q[5]=0x40008000;unsigned reads=0;
 auto read=[&](TGLHangRefs::Page &p){unsigned i=reads++;assert(i<3);p.q[2]=4096;p.q[3]=1;
  uint64_t address=i==0?0x40008000:i==1?0x40010200:0x400200a8;
  assert(p.q[0]==(address&~4095ULL));uint64_t v=i==0?0x40010001:i==1?0x40020001:0x2950000040a38001;
  if((mode==1&&i==0)||(mode==2&&i==1)||(mode==3&&i==2))v=0;
  if(mode==4&&i==0)p.q[2]=4095;if(mode==5&&i==1)p.q[3]=0;
  memcpy(p.bytes+(address&4095),&v,8);
 };
 if(mode==6)q[5]+=32768;if(mode==7)q[2]+=4;
 TGLSampleAux::collect(c,read);
 const unsigned status[]={0,5,6,7,3,3,2,1};const unsigned counts[]={3,1,2,3,1,2,0,0};
 assert(q[1]==status[mode]&&reads==counts[mode]&&q[22]==reads);
 if(!mode)assert(q[21]==0x2950000040a38001&&q[20]==0x400200a8);
 }
}
