#include "kern_tgl_tahoe_compression_policy.hpp"
#include <vector>
#include <array>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstdio>
#include <sys/mman.h>
using namespace TGLTahoeCompressionPolicy;
extern "C" __attribute__((naked)) unsigned invokeStartup(void*,void*,void*) {
 asm volatile("pushq %r13\npushq %r14\npushq %r15\nmovq %rdi,%r13\nmovq %rsi,%r14\nxorl %r15d,%r15d\ncallq *%rdx\nmovzbl 0x1474(%r13),%eax\npopq %r15\npopq %r14\npopq %r13\nretq\n");
}
extern "C" __attribute__((naked)) unsigned invokeSetter(void*,void*,void*) {
 asm volatile("pushq %rbp\nmovq %rsp,%rbp\npushq %r14\npushq %r15\nmovq %rdi,%r14\nmovq %rsi,%r15\ncallq *%rdx\nmovzbl 0x1474(%r14),%eax\npopq %r15\npopq %r14\npopq %rbp\nretq\n");
}
int main(int argc,char **argv){
 std::vector<uint8_t> image(Selector+18);
 auto put=[&](size_t at,const auto &bytes){memcpy(image.data()+at,bytes,sizeof(bytes));};
 put(Startup,StartupBytes);put(Zero,ZeroBytes);put(Select,SelectBytes);put(Setter,SetterBytes);memcpy(image.data()+Selector,"renderCompression",18);
 auto valid=[&](){return matches(image.data(),image.size(),Start,Dispatch);};assert(valid());
 assert(!matches(nullptr,image.size(),Start,Dispatch));
 for(size_t n=0;n<image.size();++n)assert(!matches(image.data(),n,Start,Dispatch));
 assert(!matches(image.data(),image.size(),Start+1,Dispatch));assert(!matches(image.data(),image.size(),Start,Dispatch+1));
 for(int32_t delta:{INT32_MIN,-4096,-1,0,1,4096,INT32_MAX}){
  const uint64_t base=0xFFFFFF7F80000000ULL,target=base+Select+20+static_cast<int64_t>(delta);
  memcpy(image.data()+Select+16,&delta,4);
  assert(matches(image.data(),image.size(),Start,Dispatch,base,target));
  assert(!matches(image.data(),image.size(),Start,Dispatch,base,target+1));
  assert(!matches(image.data(),image.size(),Start,Dispatch,base,0));
  if(delta)assert(!valid());
 }
 memset(image.data()+Select+16,0,4);
 struct Region{size_t at,len;};
 for(auto r:{Region{Startup,sizeof(StartupBytes)},Region{Zero,sizeof(ZeroBytes)},Region{Select,sizeof(SelectBytes)},Region{Setter,sizeof(SetterBytes)},Region{Selector,18}})
  for(size_t i=0;i<r.len;++i){image[r.at+i]^=1;assert(!valid());image[r.at+i]^=1;}
 if(argc>1){
  std::ifstream f(argv[1],std::ios::binary);assert(f.good());std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});const auto original=b;
  assert(apply(b.data(),b.size(),Start,Dispatch,[&](size_t at,const uint8_t *old,const uint8_t *value,size_t n){assert(!memcmp(b.data()+at,old,n));memcpy(b.data()+at,value,n);return true;}));
  auto expected=original;expected[Immediate]=0;memcpy(expected.data()+Setter,SetterOff,sizeof(SetterOff));assert(b==expected);
 }
 unsigned transactions=0;
 for(unsigned failAt=0;failAt<=2;++failAt)for(bool mutate:{false,true}){
  const auto before=image;unsigned calls=0;
  auto write=[&](size_t at,const uint8_t *old,const uint8_t *value,size_t n){assert(!memcmp(image.data()+at,old,n));bool fail=++calls==failAt;if(!fail || mutate)memcpy(image.data()+at,value,n);return !fail;};
  bool ok=apply(image.data(),image.size(),Start,Dispatch,write);assert(ok==(failAt==0));
  auto expected=before;if(ok){expected[Immediate]=0;memcpy(expected.data()+Setter,SetterOff,sizeof(SetterOff));}assert(image==expected);image=before;++transactions;
 }
 // Invalid input must never call the kernel writer.
 image[Immediate]^=1;bool called=false;assert(!apply(image.data(),image.size(),Start,Dispatch,[&](size_t,const uint8_t*,const uint8_t*,size_t){called=true;return true;}));assert(!called);image[Immediate]^=1;
 auto code=static_cast<uint8_t*>(mmap(nullptr,4096,7,MAP_ANON|MAP_PRIVATE,-1,0));assert(code!=MAP_FAILED);
 memcpy(code,StartupBytes,sizeof(StartupBytes));code[sizeof(StartupBytes)]=0xC3;
 memcpy(code+64,StartupBytes,sizeof(StartupBytes));code[64+29]=0;code[64+sizeof(StartupBytes)]=0xC3;
 memcpy(code+128,SetterBytes,10);code[138]=0xC3;
 memcpy(code+192,SetterBytes,10);memcpy(code+192,SetterOff,sizeof(SetterOff));code[202]=0xC3;
 unsigned startupCases=0,setterCases=0;
 for(unsigned condition=0;condition<256;++condition)for(unsigned feature=0;feature<256;++feature){
  std::array<uint8_t,0x1500>a,b;a.fill(0xA5);a[0x128C]=condition;b=a;uint8_t f=feature;
  assert(invokeStartup(a.data(),&f,code)==(!condition && (feature&32)?1U:0U));assert(invokeStartup(b.data(),&f,code+64)==0);assert(f==feature);
  a[0x1474]=b[0x1474]=0;assert(a==b);++startupCases;
 }
 for(unsigned initial=0;initial<256;++initial)for(unsigned value=0;value<256;++value){
  std::array<uint8_t,0x1500>a,b;a.fill(0xA5);a[0x1474]=initial;b=a;uint8_t input=value;
  assert(invokeSetter(a.data(),&input,code+128)==value);assert(invokeSetter(b.data(),&input,code+192)==0);assert(input==value);
  a[0x1474]=b[0x1474]=0;assert(a==b);++setterCases;
 }
 munmap(code,4096);
 printf("PASS exact guards, byte mutations/truncations, %u transactions; %u native startup and %u native setter CPU replays; only policy1474 differs, no GPU/system calls\n",transactions,startupCases,setterCases);
}
