#include "kern_tgl_media_root.hpp"
#include <fstream>
#include <iterator>
#include <vector>
#include <cassert>
#include <cstdio>
using namespace XeMediaRootCandidate;
int main(){std::vector<uint32_t> root(42);root[0]=0x11000001;root[3]=0x7a000004;root[9]=0x69041301;
 root[10]=0x6101000e;root[14]=0x40005001;root[16]=0x40004001;root[20]=0x4069e001;
 root[26]=0x70000007;root[35]=0x70020002;root[37]=64;root[38]=0xc0;root[39]=0x18c00001;
 std::vector<uint8_t>b(root.size()*4);memcpy(b.data(),root.data(),b.size());State s;assert(parse(b.data(),b.size(),s));assert(s.surface==0x40005000&&s.dynamic==0x40004000&&s.instruction==0x4069e000&&s.descriptor==0x400040c0&&s.descriptorBytes==64&&s.terminalOffset==156);
 unsigned refused=0;for(unsigned n=0;n<168;++n){assert(!parse(b.data(),n,s));++refused;}
 for(unsigned offset:{0x38U,0x40U,0x50U}){auto v=b;v[offset]&=0xfe;assert(!parse(v.data(),v.size(),s));++refused;}
 for(uint32_t len:{0U,1U,31U,257U,0xffffffffU}){auto v=b;memcpy(v.data()+0x94,&len,4);assert(!parse(v.data(),v.size(),s));++refused;}
 for(uint32_t off:{1U,31U,0xffffffffU}){auto v=b;memcpy(v.data()+0x98,&off,4);assert(!parse(v.data(),v.size(),s));++refused;}
 auto v=b;v[0]=0xff;assert(!parse(v.data(),v.size(),s));++refused;
 assert(!parse(nullptr,216,s));++refused;uint64_t target;
 assert(!add(0,0,32,target));assert(!add(0x800000000000ULL,0,32,target));assert(!add(0x7ffffffff000ULL,4096,32,target));assert(!add(0x7ffffffff000ULL,0,4097,target));
 assert(add(0x7ffffffff000ULL,0,4096,target));assert(add(0x40004000,0xc0,64,target)&&target==0x400040c0);
 printf("PASS synthetic root + %u parser refusals + 6 address bounds; synthetic root/no reader calls\n",refused);
}
