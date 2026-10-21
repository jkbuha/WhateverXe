#include "kern_tgl_tahoe_resolve_normal.hpp"
#include <array>
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstdio>
extern "C" {
 uint32_t wxTahoeResolveNormalEnabled=1,wxTahoeResolveNormalCopies=0,wxTahoeResolveNormalSelects=0;
 uintptr_t wxTahoeResolveNormalSource=0,wxTahoeResolveNormalCopyResume=0,wxTahoeResolveNormalSelectResume=0;
}
extern "C" void *wxTahoeResolveNormalCopy(void *last,const void *source,size_t bytes){return TGLTahoeResolveNormal::copy(last,source,bytes,wxTahoeResolveNormalEnabled,wxTahoeResolveNormalCopies);}
extern "C" __attribute__((naked,used)) void copyBridge(){asm volatile(WEX_TAHOE_RESOLVE_NORMAL_COPY_BRIDGE);}
extern "C" __attribute__((naked,used)) void selectBridge(){asm volatile(WEX_TAHOE_RESOLVE_NORMAL_SELECT_BRIDGE);}
extern "C" __attribute__((naked)) void copyFinish(){asm volatile("movq %rax,(%r12)\n movq %rbx,8(%r12)\n popq %r12\n popq %rbx\n popq %rbp\n retq\n");}
// Jump into the real call-site bridge at the original aligned RSP, then
// resume through a balanced caller frame. This exercises its C memcpy call.
extern "C" __attribute__((naked)) void runCopy(void*,void*){asm volatile("pushq %rbp\n movq %rsp,%rbp\n pushq %rbx\n pushq %r12\n movq %rdi,%rbx\n movq %rsi,%r12\n jmp _copyBridge\n");}
extern "C" __attribute__((naked)) void finish(){asm volatile("retq\n");}
extern "C" __attribute__((naked)) void native(){asm volatile("movq %r9,0xac(%r15)\n movq %r9,0xa4(%r15)\n movl %r9d,0xb4(%r15)\n retq\n");}
struct Out{uint64_t rax,r9,flags;};
extern "C" __attribute__((naked)) void run(void*,void*,void*,uint64_t,Out*,void*){
 asm volatile("pushq %rbp\n movq %rsp,%rbp\n pushq %r15\n pushq %r14\n pushq %rbx\n pushq %r12\n movq %rdi,%r15\n movq %rdx,%r14\n movq %rsi,%rdx\n movq %r9,%r12\n movq %rcx,%r9\n movq %r8,%rbx\n xorl %eax,%eax\n stc\n callq *%r12\n movq %rax,(%rbx)\n movq %r9,8(%rbx)\n pushfq\n popq %rcx\n movq %rcx,16(%rbx)\n popq %r12\n popq %rbx\n popq %r14\n popq %r15\n popq %rbp\n retq\n");
}
using Buffer=std::array<uint8_t,512>;using Params=std::array<uint8_t,160>;using Surface=std::array<uint8_t,64>;
template<class A>void put(A &a,unsigned off,uint32_t v){memcpy(a.data()+off,&v,4);}
int main(int argc,char**argv){
 using namespace TGLTahoeResolveNormal;using TGLTahoeResolveScratch::Entry;using TGLTahoeResolveScratch::Head;using TGLTahoeResolveScratch::Allocation;
 std::vector<uint8_t> image(Source+0x380);memcpy(image.data()+Entry,Head,sizeof(Head));memcpy(image.data()+CopySite,CopyPrefix,sizeof(CopyPrefix));memcpy(image.data()+SelectSite,SelectOriginal,sizeof(SelectOriginal));const uint64_t allocation=0x5100;memcpy(image.data()+Allocation,&allocation,8);
 assert(matches(image.data(),image.size(),Init,Entry,Allocation));assert(!matches(nullptr,image.size(),Init,Entry,Allocation));
 for(auto bad:{0U,1U,2U})assert(!matches(image.data(),image.size(),Init+(bad==0),Entry+(bad==1),Allocation+(bad==2)));
 for(size_t n=0;n<image.size();n++)assert(!matches(image.data(),n,Init,Entry,Allocation));
 struct Guard{size_t off,len;};for(auto g:{Guard{Entry,sizeof(Head)},Guard{CopySite,sizeof(CopyPrefix)},Guard{SelectSite,sizeof(SelectOriginal)},Guard{Allocation,8}})
  for(size_t i=0;i<g.len;i++){image[g.off+i]^=1;assert(!matches(image.data(),image.size(),Init,Entry,Allocation));image[g.off+i]^=1;}
 if(argc>1){std::ifstream f(argv[1],std::ios::binary);assert(f.good());std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});assert(matches(b.data(),b.size(),Init,Entry,Allocation));puts("PASS exact donor scratch extent, native last-copy and selector tail guards");}
 std::array<uint8_t,0x380> source;for(unsigned i=0;i<source.size();i++)source[i]=i&255;
 wxTahoeResolveNormalSource=reinterpret_cast<uintptr_t>(source.data());wxTahoeResolveNormalCopyResume=reinterpret_cast<uintptr_t>(copyFinish);
 for(bool enabled:{false,true}){
  std::vector<uint8_t> actual(0x5180,0xa5),expected=actual;uint64_t out[2]{};wxTahoeResolveNormalEnabled=enabled;const auto count=wxTahoeResolveNormalCopies;
  runCopy(actual.data(),out);memcpy(expected.data()+0x4700,source.data(),source.size());if(enabled)memcpy(expected.data()+CodeOffset,Code,sizeof(Code));
  assert(actual==expected && out[0]==reinterpret_cast<uintptr_t>(actual.data()+0x4700) && out[1]==out[0]);assert(wxTahoeResolveNormalCopies==count+enabled);
 }
 // Sizes other than the guarded native last copy keep memcpy semantics.
 std::array<uint8_t,0xA00> extra{},expected{};const auto count=wxTahoeResolveNormalCopies;
 copy(extra.data(),source.data(),32,true,wxTahoeResolveNormalCopies);memcpy(expected.data(),source.data(),32);assert(extra==expected && wxTahoeResolveNormalCopies==count);
 wxTahoeResolveNormalSelectResume=reinterpret_cast<uintptr_t>(finish);
 Buffer b{};Params p{};Surface s{};put(b,0xa0,0x1f8000c2);put(p,12,1920);put(p,16,1200);put(p,24,7680);put(p,64,0xd1);put(s,0,0x2345f000);put(s,16,2);
 unsigned cases=0;
 auto test=[&](Buffer buf,Params pa,Surface surf,bool enabled,bool copied,bool qualify,uint64_t value=0){
  const auto oldP=pa;const auto oldS=surf;auto expected=buf;wxTahoeResolveNormalEnabled=enabled;wxTahoeResolveNormalCopies=copied;const auto count=wxTahoeResolveNormalSelects;Out actual{},nativeOut{};
  run(buf.data(),pa.data(),surf.data(),value,&actual,reinterpret_cast<void*>(selectBridge));run(expected.data(),pa.data(),surf.data(),value,&nativeOut,reinterpret_cast<void*>(native));
  if(qualify){const uint64_t offset=CodeOffset;for(unsigned o:{0x8cU,0xa8U,0xb0U})memcpy(expected.data()+o,&offset,8);}
  assert(buf==expected && pa==oldP && surf==oldS);assert(actual.rax==nativeOut.rax && actual.r9==nativeOut.r9 && ((actual.flags^nativeOut.flags)&0xcd5)==0);assert(wxTahoeResolveNormalSelects==count+qualify);++cases;
 };
 test(b,p,s,true,true,true);test(b,p,s,false,true,false,0x12345678);test(b,p,s,true,false,false);
 for(unsigned o:{12U,16U,24U,64U}){auto v=p;v[o]^=1;test(b,v,s,true,true,false);}
 for(unsigned o:{0U,16U,20U}){auto v=s;v[o]^=1;test(b,p,v,true,true,false);}
 for(unsigned v:{0x1f800002U,0x1f800082U,0x1f800102U}){auto buf=b;put(buf,0xa0,v);test(buf,p,s,true,true,false);}
 printf("PASS real aligned copy bridge: native bytes/return/preserved RBX result, helper at4D00 and canaries; %u production selector replays, every shape guard, copied/disabled paths, native stores/registers/flags and unchanged inputs\n",cases);
}
