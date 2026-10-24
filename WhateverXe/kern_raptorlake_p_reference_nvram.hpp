#pragma once
#include <stddef.h>
#include <stdint.h>

namespace RaptorLakeP { namespace ReferenceNVRAM {
constexpr size_t Method = 0x77A5A, Call = 0x77BBE;
constexpr size_t OldSlot = 0x9F8, WriteSlot = 0x9E8;
constexpr uint8_t Before[]{0xFF,0x90,0xF8,0x09,0,0};
constexpr uint8_t After[]{0xFF,0x90,0xE8,0x09,0,0};
// The caller separately proves physical A7A0 rev4 and the exact private image.
// Never redirect an unknown method or a kernel whose vtable differs.
inline bool accepts(const uint8_t *bytes,size_t length,size_t method,
                    uintptr_t oldBinding,uintptr_t reserved,
                    uintptr_t newBinding,uintptr_t writer) {
 if(!bytes || length<sizeof(Before) || method!=Method || !reserved || !writer ||
    reserved==writer || oldBinding!=reserved || newBinding!=writer)return false;
 for(size_t i=0;i<sizeof(Before);++i)if(bytes[i]!=Before[i])return false;
 return true;
}
}}
