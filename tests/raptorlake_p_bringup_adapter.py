"""Validate the production donor edits, failure boundaries and exact donor replay."""
from pathlib import Path
import subprocess,tempfile,sys
r=Path(__file__).resolve().parents[1]
fixture=r'''
#include "kern_raptorlake_p_bringup_core.hpp"
#include "raptorlake_p_native_fixture.hpp"
#include <cassert>
#include <fstream>
#include <iterator>
using namespace RaptorLakeP::Bringup;
int main(int argc,char **argv){
 auto b=nativeImage();b.resize(0xA0000);
 for(unsigned i=0;i<Count;++i){uint8_t old[16],n[16];expected(i,old,n);memcpy(b.data()+edits[i].offset,old,edits[i].length);}
 memcpy(b.data()+0x959D1,"renderCompression",18);
 if(argc==2){std::ifstream f(argv[1],std::ios::binary);assert(f);b.assign(std::istreambuf_iterator<char>(f),{});}
 assert(qualified(b.data(),b.size()));
 const auto original=b;unsigned writes=0;uint32_t mask=0;
 auto writer=[&](uint32_t o,const uint8_t *p,unsigned n){++writes;memcpy(b.data()+o,p,n);return true;};
 // One mismatch at ANY checked instruction must prevent EVERY write.
 for(unsigned i=0;i<Count;++i)for(unsigned n=0;n<edits[i].length;++n){
  b=original;b[edits[i].offset+n]^=1;writes=0;mask=99;
  assert(!apply(b.data(),b.size(),writer,mask));assert(!writes&&!mask);
 }
 for(auto offset:{0u,4u,0x959D1u}){b=original;b[offset]^=1;writes=0;assert(!apply(b.data(),b.size(),writer,mask));assert(!writes);}
 b=original;writes=0;assert(!apply(b.data(),100,writer,mask));assert(!writes);
 // Refuse short writes and stops after a writer error; never claim readiness.
 for(unsigned fail=0;fail<Count;++fail){b=original;writes=0;
  auto broken=[&](uint32_t o,const uint8_t *p,unsigned n){unsigned at=writes++;if(at==fail)return false;memcpy(b.data()+o,p,n);return true;};
  assert(!apply(b.data(),b.size(),broken,mask));assert(writes==fail+1 && mask==((1u<<fail)-1));
 }
 b=original;writes=0;assert(!apply(b.data(),b.size(),[](uint32_t,const uint8_t *,unsigned){return true;},mask));assert(!mask);
 b=original;writes=0;assert(apply(b.data(),b.size(),writer,mask));assert(writes==5&&mask==31);
 // Compare independently specified outcomes, including the compression setter's store.
 assert(RaptorLakeP::Native::word(b.data()+0x7C31A)==0x80041000);
 assert(RaptorLakeP::Native::word(b.data()+0x2518F)==0x40000000);
 assert(b[0x246E0]==0 && b[0x2B872]==0x30&&b[0x2B873]==0xC0&&b[0x2B874]==0x90);
 assert(RaptorLakeP::Native::word(b.data()+0x7287F)==0x98);
 for(size_t i=0;i<b.size();++i){bool in=false;for(auto e:edits)in|=i>=e.offset&&i<e.offset+e.length;if(!in)assert(b[i]==original[i]);}
 // Reapplying to an already altered image is refused before the first write.
 writes=0;assert(!apply(b.data(),b.size(),writer,mask));assert(!writes);
}
'''
with tempfile.TemporaryDirectory(prefix='rplp-bringup-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),'-I'+str(r/'tests'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')]+sys.argv[1:],check=True)
print('PASS production donor adaptation: exact image, all-window preflight, short/write-failure refusal, complete operands and unchanged bytes')
