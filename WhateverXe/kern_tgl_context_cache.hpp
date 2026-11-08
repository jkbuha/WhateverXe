// Gen12 Wa_18022495364: invalidate instruction/state cache during RCS restore.
// Linux v6.12 intel_lrc.c uses a cacheline-sized INDIRECT_CTX at offset 0xD.
#ifndef kern_tgl_context_cache_hpp
#define kern_tgl_context_cache_hpp
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace TGLContextCache {
constexpr unsigned BatchOffset=4096, BatchBytes=64, RestoreOffset=0xD<<6;
inline void batch(void *page) {
 uint32_t words[16] {0x11000001,0x20D8,0x00400040};
 // INDIRECT_CTX is length-delimited, with NOOP padding, not MI_BATCH_BUFFER_END.
 memcpy(static_cast<uint8_t *>(page)+BatchOffset,words,sizeof(words));
}
inline bool install(void *image,size_t size,uint64_t gpu) {
 if(!image || size<0x1060 || !gpu || (gpu&4095) || gpu>0xFFFFE000ULL)return false;
 auto p=static_cast<uint8_t *>(image);uint32_t w[6];memcpy(w,p+0x1048,sizeof(w));
 const uint32_t pointer=static_cast<uint32_t>(gpu)+BatchOffset+1;
 if(w[0]!=0x21C0 || w[1]!=(gpu|5) || w[2]!=0x21C4 || w[4]!=0x21C8 ||
    (w[3] && w[3]!=pointer) || (w[5] && w[5]!=RestoreOffset))return false;
 memcpy(p+0x1054,&pointer,4);memcpy(p+0x105C,&RestoreOffset,4);return true;
}
// Linux v6.12 gen12_emit_cmd_buf_wa + gen12_emit_restore_scratch: reload the
// incoming context's own saved CMD_BUF_CCTL (state dword 0xB6) through GPR0,
// then reload GPR0 (state dword 0x74). The load addresses are per context, so
// the batch lives in that context's own PPHWSP page (unused above 0xC0).
constexpr unsigned SelfOffset=0xFC0, CCTLSlot=0x12D8, GPR0Slot=0x11D0, SelfBytes=64;
inline void selfBatch(uint32_t *w,uint32_t image) {
 const uint32_t words[16] {0x14C00002,0x2600,image+CCTLSlot+4,0, 0x15000001,0x2600,0x2084,
  0x14C00002,0x2600,image+GPR0Slot+4,0, 0x11000001,0x20D8,0x00400040, 0,0};
 memcpy(w,words,sizeof(words));
}
// A batch of this shape at SelfOffset, for any image (copied by a safe image).
inline bool selfShape(const uint32_t *w) {
 uint32_t ref[16];selfBatch(ref,w[2]-CCTLSlot-4);
 return !memcmp(w,ref,sizeof(ref)) && w[9]==w[2]-(CCTLSlot-GPR0Slot);
}
inline bool selfPointer(uint32_t v) {return (v&0xFFF)==SelfOffset+1;}
inline bool installSelf(void *image,size_t size,uint64_t image_gpu,uint64_t gpu) {
 if(!image || size<0x2000 || !image_gpu || (image_gpu&4095) || image_gpu>0x100000000ULL-size ||
    !gpu || (gpu&4095) || gpu>0xFFFFE000ULL)return false;
 auto p=static_cast<uint8_t *>(image);uint32_t w[6],slots[2],cur[16];
 memcpy(w,p+0x1048,sizeof(w));memcpy(slots,p+GPR0Slot,4);memcpy(slots+1,p+CCTLSlot,4);
 memcpy(cur,p+SelfOffset,sizeof(cur));
 const uint32_t shared=static_cast<uint32_t>(gpu)+BatchOffset+1;
 const uint32_t pointer=static_cast<uint32_t>(image_gpu)+SelfOffset+1;
 bool empty=true;for(auto v:cur)empty=empty && !v;
 if(w[0]!=0x21C0 || w[1]!=(gpu|5) || w[2]!=0x21C4 || w[4]!=0x21C8 ||
    (w[3] && w[3]!=shared && !selfPointer(w[3])) || (w[5] && w[5]!=RestoreOffset) ||
    slots[0]!=0x2600 || slots[1]!=0x2084 || (!empty && !selfShape(cur)))return false;
 uint32_t batch[16];selfBatch(batch,static_cast<uint32_t>(image_gpu));
 // Batch first, then the pointer that makes the restore fetch it.
 memcpy(p+SelfOffset,batch,sizeof(batch));
 memcpy(p+0x1054,&pointer,4);memcpy(p+0x105C,&RestoreOffset,4);return true;
}
// Never leave a pointer into another context's image: fall back to the shared
// (invalidate-only) batch, else clear INDIRECT_CTX. Returns 1 shared, 2 cleared.
inline unsigned dropSelf(void *image,size_t size,uint64_t gpu) {
 auto p=static_cast<uint8_t *>(image);uint32_t v;
 if(!p || size<0x1060)return 0;memcpy(&v,p+0x1054,4);
 if(!selfPointer(v))return 0;
 const uint32_t zero=0;memcpy(p+0x1054,&zero,4);
 if(install(image,size,gpu))return 1;
 memcpy(p+0x105C,&zero,4);return 2;
}
inline bool &cctl() {static bool c=false;return c;}
// Counts: installed, soft declines, closed refusals, self (CCTL) installs,
// reset reinstalls, reset fallbacks (shared or cleared). Function-local
// statics keep this header usable from C++14 fixtures without inline variables.
inline uint32_t *counts() {static uint32_t c[6] {};return c;}
// Soft mode: an unqualified image keeps its native INDIRECT_CTX (none) and the
// context is created normally; closed mode (TGL trial) fails construction.
inline bool &soft() {static bool s=false;return s;}
inline bool decline(bool constructed) {
 if(soft()) {__atomic_fetch_add(&counts()[1],1U,__ATOMIC_RELAXED);return constructed;}
 __atomic_fetch_add(&counts()[2],1U,__ATOMIC_RELAXED);return false;
}
}
#endif
