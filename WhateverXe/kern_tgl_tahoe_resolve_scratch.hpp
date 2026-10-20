// Guarded native colour-resolve synchronization slot relocation.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace TGLTahoeResolveScratch {
constexpr size_t Entry=0x8CB2A,Site=0x8CF32,Resume=0x8CF4B,Allocation=0xC26C0;
constexpr uint8_t Head[]={0x55,0x48,0x89,0xE5,0x41,0x57,0x41,0x56,0x41,0x55,0x41,0x54,0x53,0x48,0x81,0xEC,0x58,0x01,0,0};
constexpr uint8_t Original[]={0x45,0x89,0xE5,0x41,0xC1,0xED,0x02,0x49,0xC1,0xE5,0x02,0x49,0xC1,0xEC,0x20,0x48,0xB8,0x04,0,0,0x7A,0x04,0x14,0x10,0};
constexpr uint8_t Continuation[]={0x49,0x89,0x07,0x48,0x8B,0x05,0xEB,0xAA,0x03,0,0x4C,0x89};
inline bool matches(const uint8_t *image,size_t size,size_t symbol,size_t allocationSymbol) {
 if(!image || size<Allocation+8 || symbol!=Entry || allocationSymbol!=Allocation)return false;
 uint64_t bytes=0;memcpy(&bytes,image+Allocation,8);
 return bytes==0x5100 && !memcmp(image+Entry,Head,sizeof(Head)) &&
  !memcmp(image+Site,Original,sizeof(Original)) && !memcmp(image+Resume,Continuation,sizeof(Continuation));
}
}
#define WEX_TAHOE_RESOLVE_SCRATCH_BRIDGE \
"pushfq\n cmpl $1, _wxTahoeResolveScratchEnabled(%rip)\n jne 1f\n" \
"testq %r12, %r12\n je 1f\n testq $0xfff, %r12\n jne 1f\n" \
"movabsq $0x7fffffffafff, %rax\n cmpq %rax, %r12\n ja 1f\n" \
"lock incl _wxTahoeResolveScratchFixes(%rip)\n popfq\n" \
"leaq 0x5000(%r12), %r12\n jmp 2f\n" \
"1:\n popfq\n" \
"2:\n movl %r12d, %r13d\n shrl $2, %r13d\n shlq $2, %r13\n shrq $32, %r12\n" \
"movabsq $0x001014047a000004, %rax\n" \
"jmpq *_wxTahoeResolveScratchResume(%rip)\n"
