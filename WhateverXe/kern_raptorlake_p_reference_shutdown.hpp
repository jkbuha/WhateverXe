// Verify the named kernel getter is a read-only load of the named shutdown flag.
#pragma once
#include <stdint.h>
#include <stddef.h>
namespace RaptorLakeP {namespace ReferenceShutdown {
inline bool getter(const uint8_t *code,size_t size,uintptr_t address,uintptr_t flag){
 if(!code || size<12 || !address || !flag || code[0]!=0x55 || code[1]!=0x48 || code[2]!=0x89 || code[3]!=0xE5 ||
  code[4]!=0x8B || code[5]!=0x05 || code[10]!=0x5D || code[11]!=0xC3 || address>UINTPTR_MAX-10)return false;
 uint32_t raw=uint32_t(code[6])|(uint32_t(code[7])<<8)|(uint32_t(code[8])<<16)|(uint32_t(code[9])<<24);
 auto next=address+10;
 if(raw&0x80000000U){auto distance=uint64_t(0x100000000ULL)-raw;return distance<=next && next-distance==flag;}
 return raw<=UINTPTR_MAX-next && next+raw==flag;
}
}}
