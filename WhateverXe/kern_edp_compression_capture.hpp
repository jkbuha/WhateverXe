// Failure-only CPU copies from the already-qualified RCS ownership path.
#ifndef kern_edp_compression_capture_hpp
#define kern_edp_compression_capture_hpp
#include <stdint.h>
#include <string.h>
namespace EdpCompression {
struct Capture {
 uint64_t q[16];
 uint8_t lrc[0x1400],backing[1504];
 TGLHangRefs::Page aux[9];
};
template<class Record,class Read,class PageRead>
void collect(Capture &out,const TGLHangRefs::Capture &refs,const Record (&records)[4],
 uint64_t mapVtable,uint64_t memoryVtable,uint64_t lrc,uint64_t context,uint64_t root,
 uint64_t table,Read read,PageRead pageRead){
 static_assert(sizeof(Record)==1504,"Native backing snapshot layout");
 auto q=out.q;q[0]=1;q[1]=3;q[2]=lrc;q[3]=context;q[4]=root;q[5]=table;q[11]=UINT64_MAX;
 q[10]=read(lrc,out.lrc,sizeof(out.lrc));
 q[13]=TGLHangRefs::word(out.lrc+0x112C);
 if(q[10]==sizeof(out.lrc) && q[13]==0x11000003 &&
  TGLHangRefs::word(out.lrc+0x1130)==0x4204 && TGLHangRefs::word(out.lrc+0x1138)==0x4200)
  q[14]=(uint64_t(TGLHangRefs::word(out.lrc+0x1134))<<32)|TGLHangRefs::word(out.lrc+0x113C);
 if(refs.q[1] || refs.q[8] || !mapVtable || !memoryVtable)return;
 for(unsigned i=0;i<4;++i){
  const auto &r=records[i];const auto p=r.params;
  if(r.q[0]!=2 || r.q[7]!=512 || r.q[8]!=160 || r.q[12]!=0x128 || r.q[15]!=0x198 ||
   r.q[11]!=mapVtable || r.q[14]!=memoryVtable)continue;
  if(TGLHangRefs::word(p+12)!=1920 || TGLHangRefs::word(p+16)!=1200 ||
   TGLHangRefs::word(p+24)!=7680 || TGLHangRefs::word(p+64)!=0xD1 || TGLHangRefs::word(p+100)!=3)continue;
  const uint64_t base=TGLHangRefs::pair(p+32),length=TGLHangRefs::pair(r.memory+0x40);
  if(!base || base>>47 || (base&4095) || (TGLHangRefs::word(r.map+16)&0x40) ||
   TGLHangRefs::pair(r.map+0x98)!=base || length!=9375744 || base>0x7FFFFFFFFFFFULL-length)continue;
  bool pixels=false;
  for(unsigned j=0;j<refs.q[6] && j<TGLHangRefs::MaxPages;++j){const auto &page=refs.pages[j];
   if(page.q[0]==base && (page.q[1]&TGLHangRefs::Pixels) && page.q[2]==4096 && page.q[3])pixels=true;}
  if(!pixels)continue;
  q[6]=base;q[7]=length;q[8]=9338880;q[9]=9;q[11]=i;memcpy(out.backing,&r,sizeof(r));
  for(unsigned j=0;j<9;++j){auto &page=out.aux[j];page.q[0]=base+9338880+uint64_t(j)*4096;
   page.q[1]=TGLHangRefs::Constant;pageRead(page);if(page.q[3])++q[12];}
  q[1]=q[10]==sizeof(out.lrc) && q[12]==9 ? 0 : 4;
  return;
 }
}
}
#endif
