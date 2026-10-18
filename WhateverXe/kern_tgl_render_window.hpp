// Bounded CPU-only observation after a native RCS failure. No command edits.
#ifndef kern_tgl_render_window_hpp
#define kern_tgl_render_window_hpp
#include <stdint.h>
namespace TGLRenderWindow {
constexpr unsigned Pages=32, PageSize=4096;
template<class Page> struct Capture {
 // schema, BB, GPU base, page count, copied mask, stable mask, status;
 // q8..9 timestamps, q10..15 context/task/table/root/ring/LRC supplied by caller.
 uint64_t q[32];
 Page pages[Pages];
};
template<class Page,class Owned,class Read>
void collect(Capture<Page> &c,uint64_t bb,Owned owned,Read read) {
 c.q[0]=1;c.q[1]=bb;c.q[3]=Pages;c.q[6]=1;
 if((bb&3) || bb>=(1ULL<<47))return;
 const uint64_t page=bb&~4095ULL,back=(Pages-2)*4096ULL;
 c.q[2]=page>=back?page-back:0;
 if(c.q[2]>(1ULL<<47)-Pages*4096ULL)return;
 c.q[6]=2;if(!owned())return;
 for(unsigned i=0;i<Pages;++i){
  auto &p=c.pages[i];p.q[0]=c.q[2]+i*4096ULL;p.q[1]=1;
  read(p);
  if(p.q[2]==PageSize)c.q[4]|=1ULL<<i;
  if(p.q[2]==PageSize && p.q[3]==1 && !p.q[4])c.q[5]|=1ULL<<i;
 }
 c.q[6]=owned()?0:3;
}
}
#endif
