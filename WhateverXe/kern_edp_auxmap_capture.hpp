// Read-only TGL AUX-map candidates, via already-owned software PPGTT copies.
// Entry layout: Mesa intel_aux_map.c, TGL/BSpec44930, 64KiB main granularity.
#ifndef kern_edp_auxmap_capture_hpp
#define kern_edp_auxmap_capture_hpp
#include <stdint.h>
#include <string.h>
namespace EdpAuxMap {
struct Capture {uint64_t q[16],rows[160][8];TGLHangRefs::Page pages[8];};
template<class PageRead>
void collect(Capture &out,const EdpCompression::Capture &source,PageRead read){
 auto q=out.q;q[0]=1;q[1]=1;q[14]=1; // namespace1: owning software PPGTT
 if(source.q[0]!=1 || source.q[1] || source.q[15]!=1 || source.q[10]!=sizeof(source.lrc) ||
  source.q[7]!=9375744 || source.q[8]!=9338880)return;
 q[3]=source.q[6];q[4]=source.q[8];q[5]=q[3]+q[4];
 if(!q[3] || q[3]>>47 || (q[3]&65535) || q[3]>0x7FFFFFFFFFFFULL-source.q[7])return;
 // Locate complete LRI packets; require both AUX-base registers in one
 // packet and consistency across duplicate candidates. No fixed LRC offset.
 for(unsigned o=0;o+4<=sizeof(source.lrc);o+=4){
  const uint32_t h=TGLHangRefs::word(source.lrc+o);
  if((h&0xFF800000)!=0x11000000)continue;
  const unsigned n=(h&255)+2;if(!(n&1) || n>(sizeof(source.lrc)-o)/4)continue;
  bool lo=false,hi=false;uint32_t l=0,u=0;
  for(unsigned j=1;j+1<n;j+=2){const auto reg=TGLHangRefs::word(source.lrc+o+j*4),v=TGLHangRefs::word(source.lrc+o+(j+1)*4);
   if(reg==0x4200){if(lo && l!=v)return;lo=true;l=v;}
   if(reg==0x4204){if(hi && u!=v)return;hi=true;u=v;}}
  if(lo && hi){const uint64_t base=(uint64_t(u)<<32)|l;
   if(q[2] && q[2]!=base)return;
   q[2]=base;q[12]=o;q[13]=h;}
 }
 if(!q[2] || q[2]>>47 || (q[2]&32767))return;
 auto entry=[&](uint64_t a,uint64_t &v)->bool{
  if(!a || a>>47 || (a&7) || (a&4095)>4088){q[1]=2;return false;}
  const uint64_t base=a&~4095ULL;TGLHangRefs::Page *page=nullptr;
  for(unsigned i=0;i<q[10];++i)if(out.pages[i].q[0]==base)page=&out.pages[i];
  if(!page){if(q[10]>=8){q[1]=3;return false;}page=&out.pages[q[10]++];page->q[0]=base;page->q[1]=TGLHangRefs::Table;read(*page);}
  if(page->q[2]!=4096 || !page->q[3]){q[1]=4;q[11]=page-out.pages;return false;}
  v=TGLHangRefs::pair(page->bytes+(a&4095));return true;
 };
 const unsigned count=static_cast<unsigned>((q[4]+65535)/65536);if(count>160)return;
 q[1]=0;
 for(unsigned i=0;i<count;++i){auto r=out.rows[i];r[0]=q[3]+uint64_t(i)*65536;q[6]=i+1;r[7]=q[5]+uint64_t(i)*256;
  r[1]=q[2]+((r[0]>>36)&4095)*8;if(!entry(r[1],r[2]))return;
  if(!(r[2]&1) || !(r[2]&0x0000FFFFFFFF8000ULL)){q[1]=5;return;}
  r[3]=(r[2]&0x0000FFFFFFFF8000ULL)+((r[0]>>24)&4095)*8;if(!entry(r[3],r[4]))return;
  if(!(r[4]&1) || !(r[4]&0x0000FFFFFFFFE000ULL)){q[1]=6;return;}
  r[5]=(r[4]&0x0000FFFFFFFFE000ULL)+((r[0]>>16)&255)*8;if(!entry(r[5],r[6]))return;
  if(r[6]&1){++q[7];if((r[6]&0x0000FFFFFFFFFF00ULL)==r[7])++q[8];
   if(((r[6]>>54)&7)==5 && ((r[6]>>52)&3)==1)++q[9];}
 }
}
}
#endif
