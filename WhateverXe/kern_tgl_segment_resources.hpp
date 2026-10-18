// Read-only, bounded receipts of exact native shared-resource metadata.
#ifndef kern_tgl_segment_resources_hpp
#define kern_tgl_segment_resources_hpp
#include <stdint.h>
#include <string.h>
namespace TGLSegmentResources {
constexpr unsigned Capacity=256;
struct Record {uint64_t q[16];uint8_t entry[160],map[296];};
// Exact donor IOAccelCommandQueue::getGPUTask@30064.
// Queue+5C0 is the accelerator, whose+150 is an internal task; it is not
// the client GPU namespace. Never substitute that task for a failed lookup.
template<class Read> uint64_t queueTask(uint64_t queue,uint64_t taskClass,Read read){
 uint64_t task=0,vt=0;
 return taskClass && read(queue+0x5B0,task) && read(task,vt) && vt==taskClass?task:0;
}
inline uint32_t word(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
inline uint64_t pair(const uint8_t *p){uint64_t v;memcpy(&v,p,8);return v;}
inline bool layout(const uint8_t *e){return word(e+0x10)==0xC0 && word(e+8)==7680 &&
 word(e+0x14)==3 && word(e+0x3C)==9338880 && word(e+0x40)==36480 &&
 word(e+0x44)==0 && word(e+0x6C)==1920 && word(e+0x70)==1200;}
template<class Read> bool collect(Record &r,uint64_t task,uint64_t resource,
 uint64_t taskClass,uint64_t resourceClass,uint64_t mapClass,uint64_t auxClass,Read read){
 auto q=r.q;q[0]=1;q[4]=task;q[5]=resource;
 auto get=[&](uint64_t a,uint64_t &v){return read(a,&v,8)==8;};uint64_t vt=0;
 if(!taskClass || !resourceClass || !mapClass || !auxClass ||
    !get(task,vt) || vt!=taskClass || !get(resource,vt) || vt!=resourceClass ||
    !get(resource+0x230,q[6]))return false;
 if(!q[6])q[6]=resource+0x180;
 if(read(q[6],r.entry,sizeof(r.entry))!=sizeof(r.entry) || !layout(r.entry) ||
    !get(resource+0x40,q[7]) || !get(q[7],vt) || vt!=mapClass ||
    read(q[7],r.map,sizeof(r.map))!=sizeof(r.map))return false;
 q[8]=pair(r.map+0x98);q[9]=pair(r.map+0x18);
 if(!q[8] || q[8]>>47 || (q[8]&65535) || (word(r.map+16)&0x40) ||
    !get(task+0x278,q[11]) || !get(q[11],vt) || vt!=auxClass)return false;
 get(q[9]+0x40,q[10]);get(q[11]+0x20,q[12]);get(q[11]+0x18,q[13]);
 uint64_t ptr=0,map=0,gpu=0,aux=0,owner=0;uint8_t after[160] {};
 const bool current=get(resource+0x230,ptr) && (ptr?ptr:resource+0x180)==q[6] &&
  get(resource+0x40,map) && map==q[7] && get(map+0x98,gpu) && gpu==q[8] &&
  get(task+0x278,aux) && aux==q[11] && get(aux+0x30,owner) && owner==task &&
  read(q[6],after,sizeof(after))==sizeof(after) && !memcmp(after,r.entry,sizeof(after));
 q[14]=current?0:1;return true;
}
}
#endif
