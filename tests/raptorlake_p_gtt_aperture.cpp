#include "kern_raptorlake_p_gtt_aperture_core.hpp"
#include <cassert>
#include <initializer_list>
#include <cstdio>
using namespace RaptorLakeP::GTTAperture;
struct IO {int bad=-1;unsigned reads{};uint32_t readWord(unsigned i){assert(i==reads&&i<1024);++reads;return int(i)==bad?0:0xC01DBEAD;}};
int main(){
 for(int bad:{-1,0,123,1023}){IO io;io.bad=bad;Result out;out.requested=out.mapped=out.flushed=1;
  assert(verify(io,out)==(bad<0));assert(io.reads==1024&&out.checked==1024);
  if(bad>=0)assert(out.mismatches==1&&out.firstMismatch==unsigned(bad)&&out.failure==5);
  else assert(!out.failure&&!out.mismatches&&out.firstMismatch==UINT64_MAX&&out.first==0xC01DBEAD&&out.last==0xC01DBEAD);
 }
 for(unsigned mode=0;mode<3;++mode){IO io;Result out;out.requested=out.mapped=out.flushed=1;if(mode==0)out.requested=0;if(mode==1)out.mapped=0;if(mode==2)out.flushed=0;assert(!verify(io,out)&&out.failure==4&&!io.reads);}
 puts("PASS one-page native aperture check: 1024 bounded reads, first/middle/last mismatches and refusal before mapping/flush");
}
