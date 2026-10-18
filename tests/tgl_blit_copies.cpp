#include "kern_tgl_blit_copies.hpp"
#include <cassert>
#include <cstring>
struct Memory {
 uint8_t owner[8] {},task[8] {},params[216] {},vec[24] {},rects[256] {};
 bool rectanglesReadable=true;
 Memory(){uint64_t o=0x1111,t=0x2222,count=2,capacity=8,array=0x5000;uint32_t pitch=7680;memcpy(owner,&o,8);memcpy(task,&t,8);memcpy(params+8,&pitch,4);memcpy(vec,&count,8);memcpy(vec+8,&capacity,8);memcpy(vec+16,&array,8);for(unsigned i=0;i<256;++i)rects[i]=i;}
 size_t read(uint64_t a,void *out,size_t n){const uint8_t *p=nullptr;size_t z=0;
  switch(a){case 0x1000:p=owner;z=8;break;case 0x2000:p=task;z=8;break;case 0x3000:p=params;z=216;break;case 0x4000:p=vec;z=24;break;case 0x5000:if(rectanglesReadable){p=rects;z=256;}break;}
  if(!p||n>z)return 0;memcpy(out,p,n);return n;
 }
};
static bool collect(TGLBlitCopies::Record &r,Memory &m){return TGLBlitCopies::collect(r,0x1000,0x3000,0x4000,0x2000,0x1111,0x2222,[&](uint64_t a,void *p,size_t n){return m.read(a,p,n);});}
int main(){Memory m;auto original=m;TGLBlitCopies::Record r {};assert(collect(r,m));assert(r.q[9]==216&&r.q[11]==2&&r.q[12]==2&&!memcmp(r.params,m.params,216)&&!memcmp(r.rectangles,m.rects,64));assert(!memcmp(&m,&original,sizeof(m)));
 m.rectanglesReadable=false;r={};assert(collect(r,m)&&r.q[12]==0);m=original;
 uint64_t n=9;memcpy(m.vec,&n,8);r={};assert(!collect(r,m));uint64_t cap=16;memcpy(m.vec+8,&cap,8);assert(collect(r,m)&&r.q[12]==8);n=65537;memcpy(m.vec,&n,8);assert(!collect(r,m));m=original;
 uint64_t invalid=UINT64_MAX-100;memcpy(m.vec+16,&invalid,8);assert(!collect(r,m));m=original;
 m.owner[0]^=1;assert(!collect(r,m));m=original;m.task[0]^=1;assert(!collect(r,m));m=original;
 uint32_t small=512;memcpy(m.params+8,&small,4);assert(!collect(r,m));uint32_t target=15360;memcpy(m.params+0x50,&target,4);assert(collect(r,m));
}
