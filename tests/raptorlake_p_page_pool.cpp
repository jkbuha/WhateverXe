#include "kern_raptorlake_p_page_pool_core.hpp"
#include <cassert>
#include <cstdlib>
#include <map>
using namespace RaptorLakeP::PagePool;
static std::map<void *,size_t> heap;
static unsigned call,failAt;
static void *alloc(size_t n){if(++call==failAt)return nullptr;auto p=malloc(n);assert(p);memset(p,0xa5,n);heap[p]=n;return p;}
static void dealloc(void *p,size_t n){assert(heap.count(p)&&heap[p]==n);heap.erase(p);free(p);}
static bool grow(void *q,uint64_t n){auto p=alloc(n*8);if(!p)return false;write(q,8,n);write(q,16,p);return true;}
static void hashInit(void *h,uint64_t n){memset(h,0,24);auto p=alloc(n*24);if(p){write(h,0,n);write(h,8,n);write(h,16,p);}}
static void initialized(uint8_t *p,void *owner){memset(p,0,0x78);write(p,0x18,owner);write(p,0x38,reinterpret_cast<uintptr_t>(p)+0x30);}
int main(){
    int owner;uint8_t page[0x78];Methods m{alloc,dealloc,grow,hashInit,0x123456};
    for(failAt=0;failAt<=4;++failAt){
        call=0;initialized(page,&owner);unsigned result=construct(page,&owner,m);
        if(failAt){assert(result==failAt+1 && !read<void *>(page,0x28) && heap.empty());continue;}
        assert(!result&&call==4&&heap.size()==4&&valid(page,&owner,m.hashVtable));
        auto q=read<void *>(page,0x28),h=read<void *>(q,0x20),s=read<void *>(h,16);
        write(s,8,uintptr_t(0));assert(!valid(page,&owner,m.hashVtable));write(s,8,reinterpret_cast<uintptr_t>(s));
        write(q,0x30,uint64_t(1));assert(!valid(page,&owner,m.hashVtable));write(q,0x30,uint64_t(0));
        write(page,0x30,uint64_t(1));assert(!valid(page,&owner,m.hashVtable));write(page,0x30,uint64_t(0));
        assert(valid(page,&owner,m.hashVtable));
        dealloc(s,384);dealloc(h,24);dealloc(read<void *>(q,16),32);dealloc(q,72);assert(heap.empty());
    }
    failAt=0;
    for(unsigned off:{0x18U,0x28U,0x60U,0x64U}){initialized(page,&owner);write(page,off,uint64_t(1));call=0;assert(construct(page,&owner,m)==1&&!call);}
    assert(construct(nullptr,&owner,m)==1);
    uint8_t before[]{0xbf,0x48,0,0,0,0xe8,0x12,0x34,0x56,0x78,0x49,0x89,0xc6,0x45,0x31,0xff,0x4c,0x89,0x38,0x4c},after[20]{};
    assert(tailPatch(before,0x123456789abcdef,after));
    assert(read<uint64_t>(after,5)==0x123456789abcdefULL && after[13]==0xff && after[14]==0xd0);
    assert(0xa92e + 20 + read<int32_t>(after,16)==0xaa17);
    for(unsigned i=0;i<20;++i){before[i]^=1;bool allowed=i>=6&&i<=9;assert(tailPatch(before,1,after)==allowed);before[i]^=1;}
    assert(!tailPatch(before,0,after));
}
