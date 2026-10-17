// Bounded CPU-only observation of the exact native resolve initialization.
#ifndef kern_edp_resolve_init_capture_hpp
#define kern_edp_resolve_init_capture_hpp
#include <stdint.h>
#include <string.h>
namespace EdpResolveInit {
struct Capture {uint64_t q[16];uint8_t commands[0x4000];};
inline bool copy(Capture &out,bool qualified,const void *context,const void *pool,const uint8_t *start,const uint8_t *current,const uint8_t *limit,bool result){
 const auto a=reinterpret_cast<uintptr_t>(start),c=reinterpret_cast<uintptr_t>(current),l=reinterpret_cast<uintptr_t>(limit);
 if(!qualified || !context || !pool || !a || c<a || l<c)return false;
 const auto pending=c-a,n=pending<sizeof(out.commands)?pending:sizeof(out.commands);
 out.q[0]=1;out.q[1]=reinterpret_cast<uintptr_t>(context);out.q[2]=reinterpret_cast<uintptr_t>(pool);
 out.q[3]=a;out.q[4]=c;out.q[5]=l;out.q[6]=pending;out.q[7]=n;out.q[8]=result;
 if(n)memcpy(out.commands,start,n);out.q[15]=0x52534C56;return true;
}
}
#endif
