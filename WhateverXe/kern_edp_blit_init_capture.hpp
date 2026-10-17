// Copy only the exact donor's already-validated pending CPU initialization.
#ifndef kern_edp_blit_init_capture_hpp
#define kern_edp_blit_init_capture_hpp
#include <stdint.h>
#include <string.h>
namespace EdpBlitInit {
struct Capture {uint64_t q[16];uint8_t commands[0xA00];};
inline bool copy(Capture &out,bool qualified,const void *context,const void *pool,const uint8_t *start,const uint8_t *current,const uint8_t *limit){
 const auto a=reinterpret_cast<uintptr_t>(start),c=reinterpret_cast<uintptr_t>(current),l=reinterpret_cast<uintptr_t>(limit);
 if(!qualified || !context || !pool || !a || c<a || c-a!=0xA00 || l<c)return false;
 uint32_t end;memcpy(&end,start+0x860,4);if(end!=0x05000000U)return false;
 out.q[0]=1;out.q[1]=reinterpret_cast<uintptr_t>(context);out.q[2]=reinterpret_cast<uintptr_t>(pool);
 out.q[3]=a;out.q[4]=c;out.q[5]=l;out.q[6]=0xA00;
 memcpy(out.commands,start,sizeof(out.commands));out.q[15]=0x494E4954;return true;
}
}
#endif
