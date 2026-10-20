#include "kern_tgl_tahoe_routing.hpp"
#include <cassert>
#include <cstdio>
#include <vector>
#include <fstream>
#include <iterator>
extern "C" __attribute__((naked,used)) void hpdResume() {asm volatile("movq %rdi, %rax\nretq");}
extern "C" __attribute__((naked,used)) void hpdNextTarget() {asm volatile("xorq %rax, %rax\nretq");}
static uint64_t counter;
extern "C" {
 uintptr_t wxTahoeHpdResume=reinterpret_cast<uintptr_t>(hpdResume);
 uintptr_t wxTahoeHpdNext=reinterpret_cast<uintptr_t>(hpdNextTarget);
 uintptr_t wxTahoeHpdCounter=reinterpret_cast<uintptr_t>(&counter);
}
extern "C" __attribute__((naked,used)) void hpdBridge() {asm volatile(WEX_TAHOE_HPD_BRIDGE);}
extern "C" __attribute__((naked,used)) uintptr_t invokeHPD(const uintptr_t *,uint64_t) {
 asm volatile("pushq %r13\nmovq %rdi, %rax\nmovq %rsi, %r13\ncallq _hpdBridge\npopq %r13\nretq");
}
int main(int argc,char **argv) {
 using namespace TGLTahoeRouting;
 std::vector<uint8_t> pristine(0xE7168,0);
 for(const auto &s:sites)memcpy(pristine.data()+s.offset-16,s.context,16+s.length);
 memcpy(pristine.data()+0x75E04,hpdHead,sizeof(hpdHead));
 memcpy(pristine.data()+0x75F59,hpdSite,sizeof(hpdSite));
 memcpy(pristine.data()+0x75FA6,hpdNext,sizeof(hpdNext));
 assert(matches(pristine.data(),pristine.size(),0x75E04));
 assert(!matches(nullptr,pristine.size(),0x75E04));
 assert(!matches(pristine.data(),pristine.size()-1,0x75E04));
 assert(!matches(pristine.data(),pristine.size(),0x75E05));
 auto changed=pristine;
 for(const auto &s:sites)for(unsigned j=0;j<16+s.length;++j) {
  changed[s.offset-16+j]^=1;assert(!matches(changed.data(),changed.size(),0x75E04));changed[s.offset-16+j]^=1;
 }
 struct Extra {size_t offset;size_t size;};
 for(auto s:{Extra{0x75E04,sizeof(hpdHead)},Extra{0x75F59,sizeof(hpdSite)},Extra{0x75FA6,sizeof(hpdNext)}})
  for(size_t j=0;j<s.size;++j) {changed[s.offset+j]^=1;assert(!matches(changed.data(),changed.size(),0x75E04));changed[s.offset+j]^=1;}
 for(size_t fail=0;fail<20;++fail) {
  auto image=pristine;size_t calls=0;
  bool ok=install(image.data(),image.size(),0x75E04,[&](size_t o,const uint8_t *b,size_t n) {
   memcpy(image.data()+o,b,n);return calls++!=fail;
  });
  assert(!ok && image==pristine);
 }
 auto image=pristine;
 assert(install(image.data(),image.size(),0x75E04,[&](size_t o,const uint8_t *b,size_t n){memcpy(image.data()+o,b,n);return true;}));
 for(const auto &s:sites)for(unsigned j=0;j<s.length;++j)assert(image[s.offset+j]==0x90);
 // Execute the actual routed HPD assembly with both real and absent paths.
 for(uintptr_t path:{uintptr_t(0),uintptr_t(0x1234)})for(uint64_t index:{uint64_t(0),uint64_t(1),UINT64_MAX}) {
  auto before=counter;assert(invokeHPD(&path,index)==path);
  assert(counter==before+(path && !index?1:0));
 }
 if(argc>1) {
  std::ifstream f(argv[1],std::ios::binary);assert(f.good());
  std::vector<uint8_t> donor((std::istreambuf_iterator<char>(f)),{});
  assert(matches(donor.data(),donor.size(),0x75E04));
  puts("PASS exact supplied donor: all 20 routing sites and HPD continuation");
 }
 puts("PASS routing: instruction mutations, bounds, all 20 partial-failure rollbacks, production HPD assembly");
}
