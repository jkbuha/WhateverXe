#include "kern_edp_aux_auto.hpp"
#include <vector>
#include <cassert>
#include <fstream>
#include <iterator>
#include <cstdio>
int main(int argc,char **argv) {
 // Exact donor contexts are extracted from the production byte guards.
 std::vector<uint8_t> image(0x5D0C9,0);
 const uint8_t block0[]={0xf6,0xc3,0x2,0x74,0x3a,0x48,0xff,0x5,0x75,0x55,0x8,0x0,0x48,0xff,0x5,0xd6,0x4d,0x8,0x0,0x49,0x8b,0x3c,0x24,0xbe,0x30,0x20,0x16,0x0,0xe8,0xac,0x18,0x0,0x0,0x48,0xff,0x5,0xe9,0x50,0x8,0x0,0x83,0xc8,0x1,0x49,0x8b,0x3c,0x24,0x48,0x8b,0xf,0xbe,0x30,0x20,0x16,0x0,0x89,0xc2,0xff,0x91,0x30,0x1,0x0,0x0};
 const uint8_t block1[]={0xf6,0xc3,0x1,0x75,0x3a,0x48,0xff,0x5,0x2a,0x50,0x8,0x0,0x48,0xff,0x5,0xc3,0x47,0x8,0x0,0x49,0x8b,0x3c,0x24,0xbe,0x30,0x20,0x16,0x0,0xe8,0x99,0x12,0x0,0x0,0x48,0xff,0x5,0xd6,0x4a,0x8,0x0,0x83,0xe0,0xfe,0x49,0x8b,0x3c,0x24,0x48,0x8b,0xf,0xbe,0x30,0x20,0x16,0x0,0x89,0xc2,0xff,0x91,0x30,0x1,0x0,0x0};
 auto copy=[](uint8_t *d,const uint8_t *s,size_t n){for(size_t i=0;i<n;++i)d[i]=s[i];};
 copy(image.data()+0x5CA77,block0,sizeof(block0));copy(image.data()+0x5D08A,block1,sizeof(block1));
 auto pristine=image;
 for(unsigned fail=0;fail<3;++fail) {
  image=pristine;unsigned calls=0;
  bool ok=EdpAuxAuto::install(image.data(),image.size(),0x5CA12,true,[&](size_t off,const uint8_t *b,size_t n){copy(image.data()+off,b,n);return ++calls!=fail;});
  if(fail)assert(!ok && image==pristine);
  else {assert(ok && calls==2);assert(image[0x5CA7A]==0xEB && image[0x5D08D]==0xEB);}
 }
 for(unsigned mode=0;mode<3;++mode) {
  image=pristine;unsigned calls=0;
  bool ok=EdpAuxAuto::install(image.data(),mode==0?image.size()-1:image.size(),mode==1?0x5CA13:0x5CA12,mode!=2,[&](size_t,const uint8_t *,size_t){++calls;return true;});
  assert(!ok && !calls && image==pristine);
 }
 for(auto site:{std::pair<size_t,size_t>{0x5CA77,sizeof(block0)},{0x5D08A,sizeof(block1)}})for(size_t i=0;i<site.second;++i) {
  image=pristine;image[site.first+i]^=1;unsigned calls=0;
  assert(!EdpAuxAuto::install(image.data(),image.size(),0x5CA12,true,[&](size_t,const uint8_t *,size_t){++calls;return true;}));assert(!calls);
 }
 // A reported-success writer that does not change memory must also roll back.
 image=pristine;assert(!EdpAuxAuto::install(image.data(),image.size(),0x5CA12,true,[](size_t,const uint8_t *,size_t){return true;}));assert(image==pristine);
 if(argc>1) {
  std::ifstream f(argv[1],std::ios::binary);assert(f.good());
  std::vector<uint8_t> donor((std::istreambuf_iterator<char>(f)),{});auto before=donor;
  assert(EdpAuxAuto::install(donor.data(),donor.size(),0x5CA12,true,[&](size_t off,const uint8_t *b,size_t n){copy(donor.data()+off,b,n);return true;}));
  size_t differences=0;for(size_t i=0;i<donor.size();++i)if(donor[i]!=before[i]){assert(i==0x5CA7A || i==0x5D08D);++differences;}assert(differences==2);
  puts("PASS exact Tahoe donor: only the two DDI-A manual-toggle branch opcodes change");
 }
 puts("PASS AUX auto: all instruction mutations, scope/bounds/symbol refusal, partial and silent-write rollback");
}
