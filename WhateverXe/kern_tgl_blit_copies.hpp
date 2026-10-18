#ifndef kern_tgl_blit_copies_hpp
#define kern_tgl_blit_copies_hpp
#include <stdint.h>
#include <string.h>
namespace TGLBlitCopies {
constexpr unsigned Capacity=64,Rectangles=8,ParamsBytes=0xD8;
struct Record {uint64_t q[16];uint8_t params[ParamsBytes],vector[24],rectangles[Rectangles*32];};
static_assert(sizeof(Record)==624,"copy receipt layout");
inline uint64_t pair(const uint8_t *p){uint64_t v;memcpy(&v,p,8);return v;}
inline uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
template<class Read> bool collect(Record &r,uint64_t owner,uint64_t params,uint64_t rects,uint64_t task,
 uint64_t ownerClass,uint64_t taskClass,Read read){
 uint64_t vt=0;
 if(!ownerClass || !taskClass || read(owner,&vt,8)!=8 || vt!=ownerClass ||
    read(task,&vt,8)!=8 || vt!=taskClass || read(params,r.params,ParamsBytes)!=ParamsBytes)return false;
 const auto src=word(r.params+8),dst=word(r.params+0x50);
 if(src!=7680 && src!=15360 && dst!=7680 && dst!=15360)return false;
 if(read(rects,r.vector,24)!=24)return false;
 const auto count=pair(r.vector),array=pair(r.vector+16);
 if(!count || count>pair(r.vector+8) || count>65536 || !array || array>UINT64_MAX-Rectangles*32)return false;
 r.q[0]=1;r.q[3]=owner;r.q[4]=params;r.q[5]=rects;r.q[6]=task;
 r.q[9]=ParamsBytes;r.q[10]=24;r.q[11]=count;
 const auto n=count<Rectangles?count:Rectangles;
 const auto bytes=read(array,r.rectangles,n*32);r.q[12]=bytes/32;
 // A rectangle-array read failure remains a partial CPU receipt, not evidence
 // that native execution used these coordinates or that they were absent.
 return true;
}
}
#endif
