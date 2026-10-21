#include "kern_tgl_hang_resolve.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <sys/mman.h>
// Exact native getter: never execute the constructor branch. This fixture
// checks null/no-create and existing-context return for both create flags.
int main(){
 const uint8_t code[]={0x55,0x48,0x89,0xE5,0x53,0x50,0x48,0x8B,0x87,0xA8,0x02,0,0,0x48,0x85,0xC0,0x75,0x14,0x40,0x84,0xF6,0x74,0x0F,0x48,0x89,0xFB,0xE8,0x92,0xFF,0xFF,0xFF,0x48,0x89,0x83,0xA8,0x02,0,0,0x48,0x83,0xC4,0x08,0x5B,0x5D,0xC3};
 assert(!memcmp(code,TGLHangResolve::ContextGetterBytes,sizeof(TGLHangResolve::ContextGetterBytes)));
 void *page=mmap(nullptr,4096,7,MAP_ANON|MAP_PRIVATE,-1,0);assert(page!=MAP_FAILED);memcpy(page,code,sizeof(code));
 auto get=reinterpret_cast<uint64_t(*)(void*,bool)>(page);std::array<uint8_t,0x300> task{};
 assert(get(task.data(),false)==0);unsigned cases=1;
 for(uint64_t context:{1ULL,0xffff800000010000ULL,0xffffffffffffffffULL})for(bool create:{false,true}){
  memcpy(task.data()+0x2a8,&context,8);auto before=task;assert(get(task.data(),create)==context && task==before);++cases;
 }
 munmap(page,4096);printf("PASS %u native context getter paths; no constructor, kernel or GPU calls\n",cases);
}
