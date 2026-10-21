// Exact native zero-attribute resolve PS_EXTRA consistency correction.
#pragma once
#include "kern_tgl_tahoe_resolve_scratch.hpp"
namespace TGLTahoeResolveAttributes {
constexpr size_t Site=0x8D083,Resume=0x8D094,SBESite=0x8D10B;
constexpr uint8_t Original[] {0x48,0xB8,0x00,0x00,0x4F,0x78,0x00,0x01,0x00,0x80,0x49,0x89,0x87,0xB8,0,0,0};
constexpr uint8_t SBE[] {0x48,0xB8,0x04,0x00,0x1F,0x78,0x20,0x08,0x20,0x30,0x49,0x89,0x87,0xFC,0,0,0};
inline bool matches(const uint8_t *image,size_t size,size_t entry){
 using namespace TGLTahoeResolveScratch;
 return image && size>=SBESite+sizeof(SBE) && entry==Entry &&
  !memcmp(image+Entry,Head,sizeof(Head)) && !memcmp(image+Site,Original,sizeof(Original)) &&
  !memcmp(image+SBESite,SBE,sizeof(SBE));
}
}
#define WEX_TAHOE_RESOLVE_ATTRIBUTES_BRIDGE \
"movabsq $0x80000100784f0000, %rax\n movq %rax, 0xb8(%r15)\n" \
"pushfq\n cmpl $1, _wxTahoeResolveAttributesEnabled(%rip)\n jne 1f\n" \
"andl $0xfffffeff, 0xbc(%r15)\n lock incl _wxTahoeResolveAttributesCount(%rip)\n" \
"1:\n popfq\n jmpq *_wxTahoeResolveAttributesResume(%rip)\n"
