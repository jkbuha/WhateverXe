#include "kern_tgl_context_cache.hpp"
#include <cassert>
#include <array>
int main() {
 std::array<uint8_t,81920> image{},before{};std::array<uint8_t,8192> page{};
 const uint64_t gpu=0x402b3000;const uint32_t fields[]={0x21C0,uint32_t(gpu|5),0x21C4,0,0x21C8,0};
 memcpy(image.data()+0x1048,fields,sizeof(fields));before=image;
 assert(TGLContextCache::install(image.data(),image.size(),gpu));
 for(size_t i=0;i<image.size();++i)if((i<0x1054||i>=0x1058)&&(i<0x105C||i>=0x1060))assert(image[i]==before[i]);
 auto installed=image;assert(TGLContextCache::install(image.data(),image.size(),gpu));assert(image==installed);
 for(unsigned field:{0U,1U,2U,3U,4U,5U}) {
  image=before;uint32_t bad=0x12345678;memcpy(image.data()+0x1048+field*4,&bad,4);auto copy=image;
  assert(!TGLContextCache::install(image.data(),image.size(),gpu));assert(image==copy);
 }
 for(uint64_t bad:{0ULL,0x402b3001ULL,0xFFFFF000ULL,0x100000000ULL})assert(!TGLContextCache::install(before.data(),before.size(),bad));
 assert(!TGLContextCache::install(before.data(),0x105F,gpu));
 page.fill(0xA5);TGLContextCache::batch(page.data());uint32_t words[16];memcpy(words,page.data()+4096,64);
 assert(words[0]==0x11000001 && words[1]==0x20D8 && words[2]==0x00400040);
 for(unsigned i=3;i<16;++i)assert(!words[i]);
 for(unsigned i=0;i<8192;++i)if(i<4096||i>=4160)assert(page[i]==0xA5);
 for(uint32_t old=0;old<65536;++old)assert(((old&~(words[2]>>16))|(words[2]&0xffff))==(old|0x40));
 // Closed (TGL trial) mode refuses construction; soft (ADL-N opt-in) mode keeps
 // the native constructor result. Each path counts separately.
 auto c=TGLContextCache::counts();assert(!TGLContextCache::soft() && !c[0] && !c[1] && !c[2]);
 assert(!TGLContextCache::decline(true) && !TGLContextCache::decline(false) && c[2]==2 && !c[1]);
 TGLContextCache::soft()=true;
 assert(TGLContextCache::decline(true) && !TGLContextCache::decline(false) && c[1]==2 && c[2]==2 && !c[0]);
 TGLContextCache::soft()=false;assert(!TGLContextCache::decline(true) && c[2]==3 && c[1]==2);
 // Per-context CMD_BUF_CCTL reload (Linux gen12_emit_cmd_buf_wa + restore_scratch).
 using namespace TGLContextCache;
 const uint64_t img=0x41200000;auto self=before;
 const uint32_t g0=0x2600,cc=0x2084,gv=0x35,cv=0xffff0446;
 memcpy(self.data()+GPR0Slot,&g0,4);memcpy(self.data()+GPR0Slot+4,&gv,4);
 memcpy(self.data()+CCTLSlot,&cc,4);memcpy(self.data()+CCTLSlot+4,&cv,4);
 auto clean=self;
 assert(installSelf(self.data(),self.size(),img,gpu));
 uint32_t b[16];memcpy(b,self.data()+SelfOffset,64);
 const uint32_t want[16] {0x14C00002,0x2600,uint32_t(img)+0x12DC,0,0x15000001,0x2600,0x2084,
  0x14C00002,0x2600,uint32_t(img)+0x11D4,0,0x11000001,0x20D8,0x00400040,0,0};
 assert(!memcmp(b,want,64) && selfShape(b));
 uint32_t v;memcpy(&v,self.data()+0x1054,4);assert(v==uint32_t(img)+0xFC1 && selfPointer(v));
 memcpy(&v,self.data()+0x105C,4);assert(v==0x340);
 for(size_t i=0;i<self.size();++i)
  if((i<SelfOffset||i>=SelfOffset+64)&&(i<0x1054||i>=0x1058)&&(i<0x105C||i>=0x1060))assert(self[i]==clean[i]);
 // Idempotent; a safe-image copy (another image's batch and pointer) is replaced.
 auto again=self;assert(installSelf(again.data(),again.size(),img,gpu) && again==self);
 auto moved=self;assert(installSelf(moved.data(),moved.size(),0x41300000,gpu));
 memcpy(b,moved.data()+SelfOffset,64);assert(b[2]==0x413012DC && b[9]==0x413011D4);
 memcpy(&v,moved.data()+0x1054,4);assert(v==0x41300FC1);
 // The shared pointer is upgraded.
 auto shared=clean;assert(install(shared.data(),shared.size(),gpu) && installSelf(shared.data(),shared.size(),img,gpu));
 // Refusals leave the image untouched.
 auto refuse=[&](std::array<uint8_t,81920> x,uint64_t i,uint64_t g){auto y=x;assert(!installSelf(x.data(),x.size(),i,g));assert(x==y);};
 auto bad=clean;uint32_t z=0x2604;memcpy(bad.data()+GPR0Slot,&z,4);refuse(bad,img,gpu);
 bad=clean;z=0x2088;memcpy(bad.data()+CCTLSlot,&z,4);refuse(bad,img,gpu);
 bad=clean;z=0x12345678;memcpy(bad.data()+SelfOffset+8,&z,4);refuse(bad,img,gpu);
 bad=clean;z=0x41200041;memcpy(bad.data()+0x1054,&z,4);refuse(bad,img,gpu);
 for(uint64_t i:{0ULL,0x41200800ULL,0x100000000ULL-0x10000})refuse(clean,i,gpu);
 refuse(clean,img,0);
 assert(!installSelf(clean.data(),0x1FFF,img,gpu));
 // dropSelf: a self pointer falls back to the shared batch; otherwise untouched.
 auto d=moved;assert(dropSelf(d.data(),d.size(),gpu)==1);memcpy(&v,d.data()+0x1054,4);assert(v==uint32_t(gpu)+4097);
 d=moved;uint32_t w1=0x402b6005;memcpy(d.data()+0x104C,&w1,4);assert(dropSelf(d.data(),d.size(),gpu)==2);
 memcpy(&v,d.data()+0x1054,4);assert(!v);memcpy(&v,d.data()+0x105C,4);assert(!v);
 d=shared;auto e=clean;assert(install(e.data(),e.size(),gpu));d=e;assert(!dropSelf(d.data(),d.size(),gpu) && d==e);
 assert(!cctl() && sizeof(counts()[0])==4);
}
