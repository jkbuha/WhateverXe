// Exact native resolve surface-alignment correction, scoped to one descriptor.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace TGLTahoeResolveAlignment {
constexpr size_t Entry=0x8CB2A,Site=0x8CE43,Resume=0x8CE5A;
constexpr uint8_t Head[]={0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xEC,0x58,0x01,0,0};
constexpr uint8_t Original[]={0x41,0x0F,0xB6,0x44,0x24,0x44,0x4C,0x21,0xD0,0x48,0xC1,0xE0,0x0E,0x48,0xB9,0x00,0x30,0xFF,0x37,0,0,0,0x80};
inline bool matches(const uint8_t *image,size_t size,size_t symbol){
 return image && size>=Resume && symbol==Entry && !memcmp(image+Entry,Head,sizeof(Head)) &&
  !memcmp(image+Site,Original,sizeof(Original));
}
}
// Shared with the CPU fixture. Replay the displaced native instructions;
// change only the alignment value for the exact level-zero descriptor shape.
#define WEX_TAHOE_RESOLVE_ALIGNMENT_BRIDGE \
"movzbl 0x44(%r12), %eax\n pushfq\n" \
"cmpl $1, _wxTahoeResolveAlignmentEnabled(%rip)\n jne 1f\n" \
"cmpl $1920, 0x0c(%r12)\n jne 1f\n" \
"cmpl $1200, 0x10(%r12)\n jne 1f\n" \
"cmpl $7680, 0x18(%r12)\n jne 1f\n" \
"cmpl $0x23457000, (%r14)\n jne 1f\n" \
"cmpl $0x04af077f, 8(%r14)\n jne 1f\n" \
"cmpl $0x1dff, 0x0c(%r14)\n jne 1f\n" \
"cmpl $2, 0x10(%r14)\n jne 1f\n" \
"cmpl $0, 0x14(%r14)\n jne 1f\n" \
"cmpb $1, 0x44(%r12)\n jne 1f\n" \
"movl $3, %eax\n lock incl _wxTahoeResolveAlignmentFixes(%rip)\n" \
"1:\n popfq\n andq %r10, %rax\n shlq $14, %rax\n" \
"movabsq $0x8000000037ff3000, %rcx\n" \
"jmpq *_wxTahoeResolveAlignmentResume(%rip)\n"
