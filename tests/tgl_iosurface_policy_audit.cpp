#include "kern_tgl_iosurface_policy_audit.hpp"
#include <array>
#include <vector>
#include <fstream>
#include <iterator>
#include <cassert>
#include <cstdio>
#include <sys/mman.h>
using namespace TGLIOSurfacePolicyAudit;
int main(int argc,char**argv){
 std::vector<uint8_t> image(SetterSite+sizeof(SetterBytes));memcpy(image.data()+Site,Native,sizeof(Native));memcpy(image.data()+Zero,ZeroBytes,sizeof(ZeroBytes));memcpy(image.data()+SetterSite,SetterBytes,sizeof(SetterBytes));
 assert(matches(image.data(),image.size(),Start));assert(!matches(nullptr,image.size(),Start));assert(!matches(image.data(),image.size(),Start+1));
 for(size_t n=0;n<image.size();n++)assert(!matches(image.data(),n,Start));
 for(size_t off=0;off<sizeof(Native);off++){image[Site+off]^=1;assert(!matches(image.data(),image.size(),Start));image[Site+off]^=1;}
 for(size_t off=0;off<sizeof(ZeroBytes);off++){image[Zero+off]^=1;assert(!matches(image.data(),image.size(),Start));image[Zero+off]^=1;}
 for(size_t off=0;off<sizeof(SetterBytes);off++){image[SetterSite+off]^=1;assert(!matches(image.data(),image.size(),Start));image[SetterSite+off]^=1;}
 for(int arg=1;arg<argc;arg++){std::ifstream f(argv[arg],std::ios::binary);assert(f.good());std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});assert(matches(b.data(),b.size(),Start));}
 // Real donor block and one-immediate variant, executed in private user RAM.
 // R15 is zeroed as at the guarded native start path; RSI/caps -> native R14.
 const uint8_t prefix[]={0x41,0x55,0x41,0x56,0x41,0x57,0x49,0x89,0xFD,0x49,0x89,0xF6,0x45,0x31,0xFF};
 const uint8_t suffix[]={0x9C,0x59,0x48,0x89,0x0A,0x41,0x0F,0xB6,0x85,0x74,0x14,0,0,0x41,0x5F,0x41,0x5E,0x41,0x5D,0xC3};
 auto make=[&](bool off){std::vector<uint8_t>b(prefix,prefix+sizeof(prefix));b.insert(b.end(),Native,Native+sizeof(Native));if(off)b[sizeof(prefix)+29]=0;b.insert(b.end(),suffix,suffix+sizeof(suffix));return b;};
 auto a=make(false),b=make(true);unsigned diffs=0;for(unsigned i=0;i<a.size();i++)if(a[i]!=b[i]){assert(i==sizeof(prefix)+29 && a[i]==1 && b[i]==0);++diffs;}assert(diffs==1);
 void *page=mmap(nullptr,4096,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_ANON|MAP_PRIVATE,-1,0);assert(page!=MAP_FAILED);memcpy(page,a.data(),a.size());memcpy(static_cast<uint8_t*>(page)+512,b.data(),b.size());
 using Fn=unsigned(*)(uint8_t*,uint8_t*,uint64_t*);auto native=reinterpret_cast<Fn>(page),off=reinterpret_cast<Fn>(static_cast<uint8_t*>(page)+512);
 std::array<uint8_t,0x1500>x,y;unsigned cases=0;
 for(unsigned flag=0;flag<256;flag++)for(unsigned feature=0;feature<256;feature++){
  x.fill(0xA5);x[0x128c]=flag;y=x;uint8_t c=feature;uint64_t fa=0,fb=0;
  const auto n=native(x.data(),&c,&fa),p=off(y.data(),&c,&fb);assert(n==unsigned(flag==0 && (feature&32)) && p==0 && c==feature);assert(((fa^fb)&0xcd5)==0);
  x[0x1474]=y[0x1474]=0;assert(x==y);++cases;
 }
 // Replay the actual later setter's two memory instructions, not a model.
 const uint8_t setterPrefix[]={0x41,0x56,0x41,0x57,0x49,0x89,0xFE,0x49,0x89,0xF7};
 const uint8_t setterSuffix[]={0x41,0x5F,0x41,0x5E,0xC3};
 std::vector<uint8_t> setter(setterPrefix,setterPrefix+sizeof(setterPrefix));setter.insert(setter.end(),SetterBytes,SetterBytes+sizeof(SetterBytes));setter.insert(setter.end(),setterSuffix,setterSuffix+sizeof(setterSuffix));memcpy(static_cast<uint8_t*>(page)+1024,setter.data(),setter.size());
 auto set=reinterpret_cast<void(*)(uint8_t*,uint8_t*)>(static_cast<uint8_t*>(page)+1024);
 for(unsigned value=0;value<256;value++){y.fill(0xA5);uint8_t feature=32;uint64_t flags=0;assert(off(y.data(),&feature,&flags)==0);auto before=y;uint8_t requested=value;set(y.data(),&requested);assert(y[0x1474]==value && requested==value);before[0x1474]=value;assert(before==y);}
 munmap(page,4096);
 printf("PASS %u exhaustive native policy replays across all condition/capability bytes; one immediate differs, all other object/capability bytes and defined flags preserved. Runtime setter can override startup-only result; no GPU/system access.\n",cases);
}
