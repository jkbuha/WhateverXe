#include "kern_tgl_tahoe_resolve_alignment.hpp"
#include <array>
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstdio>
extern "C" {uint32_t wxTahoeResolveAlignmentEnabled=1,wxTahoeResolveAlignmentFixes=0;uintptr_t wxTahoeResolveAlignmentResume=0;}
extern "C" __attribute__((naked,used)) void bridge(){asm volatile(WEX_TAHOE_RESOLVE_ALIGNMENT_BRIDGE);}
extern "C" __attribute__((naked)) void finish(){asm volatile("retq\n");}
extern "C" __attribute__((naked)) void native(){asm volatile("movzbl 0x44(%r12),%eax\n andq %r10,%rax\n shlq $14,%rax\n movabsq $0x8000000037ff3000,%rcx\n retq\n");}
struct Out {uint64_t alignment,mask,flags;};
extern "C" __attribute__((naked)) void run(const uint8_t*,const uint8_t*,uint64_t,Out*,void*){
 asm volatile("pushq %rbp\n movq %rsp,%rbp\n pushq %r12\n pushq %r14\n pushq %rbx\n pushq %r15\n movq %rdi,%r12\n movq %rsi,%r14\n movq %rdx,%r10\n movq %rcx,%rbx\n stc\n callq *%r8\n movq %rax,(%rbx)\n movq %rcx,8(%rbx)\n pushfq\n popq %rdx\n movq %rdx,16(%rbx)\n popq %r15\n popq %rbx\n popq %r14\n popq %r12\n popq %rbp\n retq\n");
}
using Params=std::array<uint8_t,160>;using Surface=std::array<uint8_t,64>;
template<class A>void put(A &a,unsigned o,uint32_t v){memcpy(a.data()+o,&v,4);}
int main(int argc,char**argv){
 wxTahoeResolveAlignmentResume=reinterpret_cast<uintptr_t>(finish);
 Params p{};Surface s{};put(p,12,1920);put(p,16,1200);put(p,24,7680);p[0x44]=1;
 put(s,0,0x23457000);put(s,8,0x04af077f);put(s,12,0x1dff);put(s,16,2);
 unsigned cases=0;
 auto test=[&](Params a,Surface b,bool enabled,bool qualifies,uint64_t mask=3){
  const auto originalA=a;const auto originalB=b;auto expectedA=a;if(qualifies)expectedA[0x44]=3;
  const auto count=wxTahoeResolveAlignmentFixes;wxTahoeResolveAlignmentEnabled=enabled;
  Out actual{},expected{};run(a.data(),b.data(),mask,&actual,reinterpret_cast<void*>(bridge));
  run(expectedA.data(),b.data(),mask,&expected,reinterpret_cast<void*>(native));
  assert(actual.alignment==expected.alignment && actual.mask==expected.mask);
  assert(((actual.flags^expected.flags)&0xC5)==0);
  assert(a==originalA && b==originalB);assert(wxTahoeResolveAlignmentFixes==count+qualifies);++cases;
 };
 test(p,s,true,true);test(p,s,false,false);
 for(unsigned o:{12U,16U,24U}){auto a=p;a[o]^=1;test(a,s,true,false);}
 for(unsigned o:{0U,8U,12U,16U,20U}){auto b=s;b[o]^=1;test(p,b,true,false);}
 for(unsigned v:{0U,2U,3U,255U}){auto a=p;a[0x44]=v;test(a,s,true,false);}
 test(p,s,true,true,0);test(p,s,true,true,~0ULL);
 using namespace TGLTahoeResolveAlignment;
 std::vector<uint8_t> image(Resume);memcpy(image.data()+Entry,Head,sizeof(Head));memcpy(image.data()+Site,Original,sizeof(Original));
 assert(matches(image.data(),image.size(),Entry));assert(!matches(nullptr,image.size(),Entry));assert(!matches(image.data(),image.size(),Entry+1));
 for(size_t n=0;n<Resume;n++)assert(!matches(image.data(),n,Entry));
 for(auto check:{std::array<size_t,2>{Entry,sizeof(Head)},{Site,sizeof(Original)}})
  for(size_t i=0;i<check[1];i++){image[check[0]+i]^=1;assert(!matches(image.data(),image.size(),Entry));image[check[0]+i]^=1;}
 if(argc>1){std::ifstream f(argv[1],std::ios::binary);assert(f.good());std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});assert(matches(b.data(),b.size(),Entry));puts("PASS exact donor resolve entry and displaced alignment instructions");}
 printf("PASS %u production alignment-bridge replays: qualified correction, every shape guard, disabled path, native mask/defined flags and unchanged input memory\n",cases);
}
