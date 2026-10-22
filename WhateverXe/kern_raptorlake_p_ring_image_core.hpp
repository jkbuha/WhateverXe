// Execute pinned native helpers on scoped RAM objects, never admit a driver.
#ifndef KERN_RAPTORLAKE_P_RING_IMAGE_CORE_HPP
#define KERN_RAPTORLAKE_P_RING_IMAGE_CORE_HPP
#include "kern_raptorlake_p_page_pool_core.hpp"
namespace RaptorLakeP {namespace RingImage {
using PagePool::read;using PagePool::write;
struct Methods {void (*init)(void *);void (*tail)(void *,uint32_t);void (*mode)(void *);void (*hws)(void *);};
struct Data {
    alignas(16) uint8_t owner[0x2000],context[0xD8],shared[0x40],ring[0x88],mapped[2][0x38],maps[2][0x20],vtable[0x130];
};
struct Frame {Data data;uint64_t addresses[2]{},queries[2]{};uint64_t bad{};};
inline Frame *active;
inline uint64_t query(void *map) {
    if(!active)return 0;
    for(unsigned i=0;i<2;++i)if(map==active->data.maps[i]){++active->queries[i];return active->addresses[i];}
    ++active->bad;return active->addresses[0];
}
inline void seed(Frame &frame,uintptr_t mmio,uintptr_t cpu,uint32_t gpu,unsigned engine) {
    memset(&frame,0,sizeof(frame));auto &d=frame.data;
    write(d.owner,0x119C,uint32_t(4));write(d.owner,0x1240,mmio);write(d.owner,0x1300,uint64_t(1)<<engine);
    write(d.context,0x50,reinterpret_cast<uintptr_t>(d.owner));write(d.context,0x6C,uint8_t(engine));
    write(d.context,0x98,reinterpret_cast<uintptr_t>(d.shared));write(d.context,0xB0,reinterpret_cast<uintptr_t>(d.ring));
    write(d.shared,0x38,cpu);write(d.ring,0x80,reinterpret_cast<uintptr_t>(d.mapped[0]));
    write(d.vtable,0x128,reinterpret_cast<uintptr_t>(query));
    for(unsigned i=0;i<2;++i) {
        write(d.mapped[i],0x30,reinterpret_cast<uintptr_t>(d.maps[i]));
        write(d.maps[i],0,reinterpret_cast<uintptr_t>(d.vtable));frame.addresses[i]=gpu;
    }
    write(d.owner,0x1408+8*engine,reinterpret_cast<uintptr_t>(d.mapped[0]));
    write(d.owner,0x1438,reinterpret_cast<uintptr_t>(d.mapped[1]));
}
// The surrounding image remains a Gen12 initial-state template. These are the
// actual native ring address/control/head/tail methods, not native allocation.
inline bool build(Frame &frame,Data &saved,uint8_t *memory,size_t size,uint32_t ringGPU,
        uint32_t tail,unsigned engine,const Methods &m) {
    if(active || !memory || size<3*4096 || (engine!=0&&engine!=2) || !ringGPU || (ringGPU&4095) ||
       ringGPU>0xFFFFF000 || (tail&7) || tail>=4096 || !m.init || !m.tail)return false;
    seed(frame,0,reinterpret_cast<uintptr_t>(memory+4096),ringGPU,engine);memcpy(&saved,&frame.data,sizeof(saved));
    auto values=reinterpret_cast<uint32_t *>(memory+2*4096);
    constexpr unsigned indices[]{5,7,9,11};
    for(auto i:indices)values[i]=0xDEADBEEF;
    active=&frame;m.init(frame.data.context);m.tail(frame.data.context,tail);active=nullptr;
    return !frame.bad && frame.queries[0]==1 && !frame.queries[1] && !memcmp(&saved,&frame.data,sizeof(saved)) &&
        values[5]==0 && values[7]==tail && values[9]==ringGPU && values[11]==1;
}
inline bool setup(Frame &frame,Data &saved,uintptr_t mmio,uint32_t hwsGPU,unsigned engine,const Methods &m,uint32_t sharedGPU=0) {
    if(active || !mmio || (mmio&4095) || !hwsGPU || (hwsGPU&4095) || (engine!=0&&engine!=2) || !m.mode || !m.hws || (sharedGPU&4095))return false;
    seed(frame,mmio,0,hwsGPU,engine);if(sharedGPU)frame.addresses[1]=sharedGPU;memcpy(&saved,&frame.data,sizeof(saved));
    active=&frame;m.mode(frame.data.owner);m.hws(frame.data.owner);active=nullptr;
    return !frame.bad && frame.queries[0]==1 && frame.queries[1]==1 && !memcmp(&saved,&frame.data,sizeof(saved));
}
}}
#endif
