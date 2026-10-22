#ifndef KERN_RAPTORLAKE_P_PAGE_POOL_CORE_HPP
#define KERN_RAPTORLAKE_P_PAGE_POOL_CORE_HPP
#include <stdint.h>
#include <stddef.h>
#include <string.h>
namespace RaptorLakeP { namespace PagePool {
template<class T> T read(const void *p,unsigned off) {T v;memcpy(&v,static_cast<const uint8_t *>(p)+off,sizeof(v));return v;}
template<class T> void write(void *p,unsigned off,T v) {memcpy(static_cast<uint8_t *>(p)+off,&v,sizeof(v));}
struct Methods {
    void *(*allocate)(size_t);
    void (*deallocate)(void *,size_t);
    bool (*grow)(void *,uint64_t);
    void (*hashInit)(void *,uint64_t);
    uintptr_t hashVtable;
};
// Build the empty CPU containers only. Options requiring locks/workloops are
// deliberately unsupported. Publish ownership only after all allocations pass.
inline unsigned construct(void *page,void *owner,const Methods &m) {
    if(!page || !owner || read<void *>(page,0x18)!=owner || read<uint32_t>(page,0x60) ||
       read<uint8_t>(page,0x64) || read<void *>(page,0x28) || !m.allocate ||
       !m.deallocate || !m.grow || !m.hashInit || !m.hashVtable)return 1;
    void *queue=m.allocate(0x48);
    if(!queue)return 2;
    memset(queue,0,0x48);
    void *hash=nullptr;
    unsigned failure=0;
    do {
        if(!m.grow(queue,4) || read<uint64_t>(queue,0) || read<uint64_t>(queue,8)!=4 || !read<void *>(queue,0x10)){failure=3;break;}
        hash=m.allocate(0x18);
        if(!hash){failure=4;break;}
        memset(hash,0,0x18);
        m.hashInit(hash,16);
        if(read<uint64_t>(hash,0)!=16 || read<uint64_t>(hash,8)!=16 || !read<void *>(hash,0x10)){failure=5;break;}
        auto slots=static_cast<uint8_t *>(read<void *>(hash,0x10));
        for(unsigned i=0;i<16;++i){auto slot=slots+24*i;write(slot,0,uint64_t(0));write(slot,8,reinterpret_cast<uintptr_t>(slot));write(slot,16,uint64_t(0));}
        write(queue,0x18,m.hashVtable);write(queue,0x20,hash);write(queue,0x38,uint8_t(1));
        write(page,0x28,queue);return 0;
    }while(false);
    // Exact native grow/constructor retain zero fields on allocation failure.
    // No hash entries exist; free each buffer without invoking any GPU code.
    if(hash){auto slots=read<void *>(hash,0x10);if(slots)m.deallocate(slots,read<uint64_t>(hash,8)*24);m.deallocate(hash,0x18);}
    if(auto entries=read<void *>(queue,0x10))m.deallocate(entries,read<uint64_t>(queue,8)*8);
    m.deallocate(queue,0x48);return failure;
}
inline bool valid(const void *page,void *owner,uintptr_t vtable) {
    if(!page || read<void *>(page,0x18)!=owner || read<uint32_t>(page,0x60) || read<uint8_t>(page,0x64) ||
       read<uint64_t>(page,0x10) || read<uint64_t>(page,0x20) || read<uint64_t>(page,0x30) ||
       read<uintptr_t>(page,0x38)!=reinterpret_cast<uintptr_t>(page)+0x30 || read<uint64_t>(page,0x40) ||
       read<uint64_t>(page,0x48) || read<uint8_t>(page,0x50) || read<uint64_t>(page,0x68))return false;
    auto q=read<void *>(page,0x28);
    if(!q || read<uint64_t>(q,0) || read<uint64_t>(q,8)!=4 || !read<void *>(q,0x10) ||
       read<uintptr_t>(q,0x18)!=vtable || read<uint64_t>(q,0x28) || read<uint64_t>(q,0x30) || read<uint8_t>(q,0x38)!=1)return false;
    auto h=read<void *>(q,0x20);
    if(!h || read<uint64_t>(h,0)!=16 || read<uint64_t>(h,8)!=16 || !read<void *>(h,0x10))return false;
    auto slots=static_cast<const uint8_t *>(read<void *>(h,0x10));
    for(unsigned i=0;i<16;++i){auto s=slots+24*i;if(read<uint64_t>(s,0) || read<uintptr_t>(s,8)!=reinterpret_cast<uintptr_t>(s) || read<uint64_t>(s,16))return false;}
    return true;
}
// Replace only the unchecked-allocation tail, after native OSObject::init.
// Original prologue/field initialization and epilogue remain in the donor.
inline bool tailPatch(const uint8_t *before,uintptr_t helper,uint8_t (&after)[20]) {
    constexpr uint8_t prefix[]{0xBF,0x48,0,0,0,0xE8};
    constexpr uint8_t suffix[]{0x49,0x89,0xC6,0x45,0x31,0xFF,0x4C,0x89,0x38,0x4C};
    if(!helper || memcmp(before,prefix,6) || memcmp(before+10,suffix,10))return false;
    const uint8_t patch[]{0x48,0x89,0xDF,0x48,0xB8,0,0,0,0,0,0,0,0,0xFF,0xD0,0xE9,0xD5,0,0,0};
    memcpy(after,patch,20);memcpy(after+5,&helper,8);return true;
}
}}
#endif
