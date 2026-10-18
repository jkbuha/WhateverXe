// Failure-only AUX-table observation. Never creates or changes a mapping.
#ifndef kern_tgl_sample_aux_hpp
#define kern_tgl_sample_aux_hpp
#include <stdint.h>
#include <string.h>
namespace TGLSampleAux {
struct Capture {
 uint64_t q[32];
 TGLHangRefs::Page pages[4]; // three GPU table pages, one native CPU root
 uint8_t object[80],map[296],lrc[8192];
};
template<class Read> void collect(Capture &c,Read read){
 auto q=c.q;q[0]=1;q[1]=1;
 if(!q[2] || q[2]>>47 || (q[2]&65535) || !q[3] || q[3]>>47 || (q[3]&255) ||
    !q[4] || q[4]>>47 || (q[4]&32767))return;
 q[1]=2;if(q[4]!=q[5])return;
 auto entry=[&](unsigned i,uint64_t address,uint64_t &value){
  auto &p=c.pages[i];p.q[0]=address&~4095ULL;p.q[1]=TGLHangRefs::Table;read(p);++q[22];
  if(p.q[2]!=4096 || p.q[3]!=1 || p.q[4])return false;
  memcpy(&value,p.bytes+(address&4095),8);return true;
 };
 q[16]=q[4]+((q[2]>>36)&4095)*8;q[1]=3;
 if(!entry(0,q[16],q[17]))return;
 q[1]=5;if(!(q[17]&1) || !(q[17]&0x0000FFFFFFFF8000ULL))return;
 q[18]=(q[17]&0x0000FFFFFFFF8000ULL)+((q[2]>>24)&4095)*8;q[1]=3;
 if(!entry(1,q[18],q[19]))return;
 q[1]=6;if(!(q[19]&1) || !(q[19]&0x0000FFFFFFFFE000ULL))return;
 q[20]=(q[19]&0x0000FFFFFFFFE000ULL)+((q[2]>>16)&255)*8;q[1]=3;
 if(!entry(2,q[20],q[21]))return;
 q[1]=(q[21]&1)?0:7;
}
}
#endif
