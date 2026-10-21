#include "kern_tgl_tahoe_resolve_attributes.hpp"
#include <array>
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstdio>
extern "C" {uint32_t wxTahoeResolveAttributesEnabled=1,wxTahoeResolveAttributesCount=0;uintptr_t wxTahoeResolveAttributesResume=0;}
extern "C" __attribute__((naked,used)) void bridge(){asm volatile(WEX_TAHOE_RESOLVE_ATTRIBUTES_BRIDGE);}
extern "C" __attribute__((naked)) void finish(){asm volatile("retq\n");}
extern "C" __attribute__((naked)) void native(){asm volatile("movabsq $0x80000100784f0000,%rax\n movq %rax,0xb8(%r15)\n retq\n");}
struct Out{uint64_t rax,flags;};
extern "C" __attribute__((naked)) void run(void*,Out*,void*){
 asm volatile("pushq %rbp\n movq %rsp,%rbp\n pushq %r15\n pushq %rbx\n movq %rdi,%r15\n movq %rsi,%rbx\n xorl %eax,%eax\n stc\n callq *%rdx\n movq %rax,(%rbx)\n pushfq\n popq %rcx\n movq %rcx,8(%rbx)\n popq %rbx\n popq %r15\n popq %rbp\n retq\n");
}
int main(int argc,char**argv){
 using namespace TGLTahoeResolveAttributes;using TGLTahoeResolveScratch::Entry;using TGLTahoeResolveScratch::Head;
 wxTahoeResolveAttributesResume=reinterpret_cast<uintptr_t>(finish);
 for(bool enabled:{false,true})for(unsigned fill:{0U,0xA5U,0xFFU}){
  std::array<uint8_t,512> a,b;a.fill(fill);b=a;Out actual{},expected{};const auto count=wxTahoeResolveAttributesCount;wxTahoeResolveAttributesEnabled=enabled;
  run(a.data(),&actual,reinterpret_cast<void*>(bridge));run(b.data(),&expected,reinterpret_cast<void*>(native));
  if(enabled)b[0xbd]&=0xfe;
  assert(a==b && actual.rax==expected.rax && ((actual.flags^expected.flags)&0xcd5)==0);
  assert(wxTahoeResolveAttributesCount==count+enabled);
  uint64_t packet=0;memcpy(&packet,a.data()+0xb8,8);assert(packet==(enabled?0x80000000784f0000ULL:0x80000100784f0000ULL));
 }
 std::vector<uint8_t> image(SBESite+sizeof(SBE));memcpy(image.data()+Entry,Head,sizeof(Head));memcpy(image.data()+Site,Original,sizeof(Original));memcpy(image.data()+SBESite,SBE,sizeof(SBE));
 assert(matches(image.data(),image.size(),Entry));assert(!matches(nullptr,image.size(),Entry));assert(!matches(image.data(),image.size(),Entry+1));
 for(size_t n=0;n<image.size();n++)assert(!matches(image.data(),n,Entry));
 struct Guard{size_t off,len;};for(auto g:{Guard{Entry,sizeof(Head)},Guard{Site,sizeof(Original)},Guard{SBESite,sizeof(SBE)}})
  for(size_t i=0;i<g.len;i++){image[g.off+i]^=1;assert(!matches(image.data(),image.size(),Entry));image[g.off+i]^=1;}
 if(argc>1){std::ifstream f(argv[1],std::ios::binary);assert(f.good());std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});assert(matches(b.data(),b.size(),Entry));puts("PASS exact native PS_EXTRA store, zero-attribute SBE and resolve head");}
 puts("PASS six production attribute-bridge replays: only bit8 changes, native RAX/defined flags restored, disabled path unchanged, bounded memory and all guards");
}
