#include "kern_tgl_tahoe_resolve_draw.hpp"
#include <array>
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstdio>
#include <sys/mman.h>
using namespace TGLTahoeResolveDraw;
int main(int argc,char**argv){
 using TGLTahoeResolveScratch::Entry;using TGLTahoeResolveScratch::Head;
 std::vector<uint8_t> image(TailSite+sizeof(Tail));
 memcpy(image.data()+Entry,Head,sizeof(Head));memcpy(image.data()+Site,Original,sizeof(Original));
 memcpy(image.data()+TailSite,Tail,sizeof(Tail));memcpy(image.data()+ZeroSite,Zero,sizeof(Zero));memcpy(image.data()+SaveSite,Save,sizeof(Save));
 auto match=[&](const uint8_t *b,size_t n,size_t e){return TGLTahoeResolveDraw::matches(b,n,e);};
 assert(match(image.data(),image.size(),Entry));assert(!match(nullptr,image.size(),Entry));assert(!match(image.data(),image.size(),Entry+1));
 for(size_t n=0;n<image.size();n++)assert(!match(image.data(),n,Entry));
 struct Guard {size_t off,len;};
 for(auto c:{Guard{Entry,sizeof(Head)},Guard{Site,sizeof(Original)},Guard{TailSite,sizeof(Tail)},Guard{ZeroSite,sizeof(Zero)},Guard{SaveSite,sizeof(Save)}})
  for(size_t i=0;i<c.len;i++){image[c.off+i]^=1;assert(!match(image.data(),image.size(),Entry));image[c.off+i]^=1;}
 if(argc>1){std::ifstream f(argv[1],std::ios::binary);assert(f.good());std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)),{});assert(match(b.data(),b.size(),Entry));puts("PASS actual donor resolve draw, header tail and zero-field guards");}
 // Execute exactly the guarded native header stores in isolated user memory.
 std::vector<uint8_t> code={0x53,0x48,0x89,0xfb};code.insert(code.end(),Original,Original+sizeof(Original));code.insert(code.end(),Tail,Tail+sizeof(Tail));code.push_back(0x5b);code.push_back(0xc3);
 auto mem=mmap(nullptr,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_ANON|MAP_PRIVATE,-1,0);assert(mem!=MAP_FAILED);
 auto execute=[&](bool patched,std::array<uint8_t,512> &out){
  auto bytes=code;if(patched)memcpy(bytes.data()+4,Replacement,sizeof(Replacement));memcpy(mem,bytes.data(),bytes.size());reinterpret_cast<void(*)(void*)>(mem)(out.data());
 };
 for(unsigned pattern:{0U,0xA5U,0xFFU}){
  std::array<uint8_t,512> a,b;a.fill(pattern);b=a;execute(false,a);execute(true,b);
  unsigned differences=0;for(unsigned i=0;i<a.size();i++)if(a[i]!=b[i]){assert(i==0x180 && a[i]==8 && b[i]==5);++differences;}assert(differences==1);
  uint32_t old=0,now=0;memcpy(&old,a.data()+0x180,4);memcpy(&now,b.data()+0x180,4);
  assert(old==0x7b000008 && now==0x7b000005 && !(now&(1<<11)) && (now&255)+2==7);
 }
 munmap(mem,4096);
 // Actual failure's ten-DWORD draw: correction leaves all following offsets
 // intact; the last three zero DWORDs are legal standalone MI_NOOP packets.
 const uint32_t old[]={0x7b000008,0,3,0,1,0,0,0,0,0,0x7a000004,0x00105000};
 auto fixed=std::array<uint32_t,12>{};memcpy(fixed.data(),old,sizeof(old));fixed[0]=0x7b000005;
 assert(!memcmp(fixed.data()+1,old+1,sizeof(old)-4));for(unsigned i=7;i<10;i++)assert(fixed[i]==0);assert(fixed[10]==0x7a000004);
 puts("PASS isolated native emitter replays, exactly one changed byte, seven-DWORD non-extended packet, three zero NOOPs, unchanged following flush and all exact guards");
}
