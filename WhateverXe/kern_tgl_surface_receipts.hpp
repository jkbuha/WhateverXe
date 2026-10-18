#ifndef kern_tgl_surface_receipts_hpp
#define kern_tgl_surface_receipts_hpp
#include <stdint.h>
#include <string.h>
namespace TGLSurfaceReceipts {
constexpr unsigned Capacity=64,Attempts=256;
struct Record {uint64_t q[24];uint8_t entry[160],surface[64],map[296];};
static_assert(sizeof(Record)==712,"surface receipt layout");
inline uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
inline uint64_t pair(const uint8_t *p){uint64_t v;memcpy(&v,p,8);return v;}
inline unsigned kind(const uint8_t *s){const auto f=(word(s)>>18)&1023U,p=(word(s+12)&0x3FFFFU)+1;
 return f==0xC0&&p==7680?0:f==0x80&&p==15360?1:2;}
// Exact entry and descriptor supplied to the existing native aux-state helper.
// Never infer an entry by walking a resource's private default/keyed lists.
template<class Read> bool collect(Record &r,uint64_t resource,uint64_t entry,uint64_t surface,
 uint64_t resourceClass,uint64_t mapClass,uint64_t taskClass,uint64_t auxClass,Read read){
 auto q=r.q;q[0]=1;q[3]=resource;q[4]=entry;q[5]=surface;
 auto get=[&](uint64_t a,uint64_t &v){return read(a,&v,8)==8;};
 if(read(surface,r.surface,64)!=64 || kind(r.surface)>1 || read(entry,r.entry,160)!=160)return false;
 q[22]=1;if(!resourceClass || !get(resource,q[8]) || q[8]!=resourceClass)return true;
 if(read(resource+0x14,q+9,1)!=1){q[22]=6;return true;}
 q[10]=resource;
 if(q[9]==0x82 && !get(resource+0xE0,q[10])){q[22]=6;return true;}
 if(q[10] && get(q[10],q[11]) && q[11]==resourceClass)read(q[10]+0x14,q+12,1);
 q[22]=2;if(!mapClass || !get(resource+0x40,q[13]) || !get(q[13],q[14]) || q[14]!=mapClass)return true;
 if(read(q[13],r.map,296)!=296){q[22]=6;return true;}
 q[15]=pair(r.map+0x90);q[16]=pair(r.map+0x98);q[17]=pair(r.map+0x18);
 uint64_t vt=0;q[22]=3;if(!taskClass || !get(q[15],vt) || vt!=taskClass)return true;
 q[22]=4;if(!auxClass || !get(q[15]+0x278,q[18]) || !get(q[18],vt) || vt!=auxClass)return true;
 uint64_t owner=0,rootMap=0;q[22]=6;
 if(!get(q[18]+0x18,q[19]) || !get(q[18]+0x10,rootMap) || !get(rootMap,vt) || vt!=mapClass || !get(rootMap+0x98,q[20]) || !get(q[18]+0x20,q[21]) || !get(q[18]+0x30,owner))return true;
 // Retain partial/changed evidence with a status; never claim coherent ownership.
 uint64_t later=0;uint8_t after[160] {};q[22]=5;
 if(owner!=q[15] || !get(resource+0x40,later) || later!=q[13] ||
    !get(q[13]+0x90,later) || later!=q[15] || !get(q[13]+0x98,later) || later!=q[16] ||
    !get(q[15]+0x278,later) || later!=q[18] ||
    !get(q[18]+0x10,later) || later!=rootMap || !get(q[18]+0x18,later) || later!=q[19] ||
    !get(rootMap+0x98,later) || later!=q[20] ||
    read(entry,after,160)!=160 || memcmp(after,r.entry,160))return true;
 q[22]=0;return true;
}
}
#endif
