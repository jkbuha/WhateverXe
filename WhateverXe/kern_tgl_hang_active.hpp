// Failure-only GGTT candidate at the donor's saved BB_ADDR. This is a CPU
// snapshot with paired PTE/physical checks, not proof of GPU execution or TLB
// contents. Never map memory, acquire wake, or write hardware here.
#pragma once
#include <stdint.h>
namespace TGLHangActive {
struct Capture {
 uint64_t q[16]; // schema,status,raw BB,page,physical,CPU,PTE before/after,
                 // copied,CPU physical before/after,reserved
 uint8_t bytes[4096];
};
template<class PTE,class Physical,class Read>
void collect(Capture &c,bool admitted,uint64_t raw,uint64_t directBase,
 uint64_t directEnd,PTE pte,Physical physical,Read read) {
 c.q[0]=1;c.q[1]=1;c.q[2]=raw;
 if(!admitted || !(raw&1) || raw>>32 || !(raw&~4095ULL))return;
 const uint64_t page=raw&~4095ULL;c.q[3]=page;
 c.q[6]=pte(page);c.q[1]=2;
 if(c.q[6]==UINT64_MAX || !(c.q[6]&1))return;
 const uint64_t pa=c.q[6]&0x7FFFFFF000ULL;c.q[4]=pa;
 if(!pa || directEnd<=directBase || pa>UINT64_MAX-directBase)return;
 const uint64_t cpu=directBase+pa;c.q[5]=cpu;c.q[1]=3;
 if(cpu<directBase || cpu>=directEnd || directEnd-cpu<4096 ||
    cpu>>47!=0x1FFFF || cpu>UINT64_MAX-4095)return;
 if(!physical(cpu,c.q[9]) || c.q[9]!=pa)return;
 c.q[8]=read(cpu,c.bytes,4096);c.q[7]=pte(page);c.q[1]=4;
 if(c.q[8]!=4096 || c.q[6]!=c.q[7] ||
    !physical(cpu,c.q[10]) || c.q[10]!=pa)return;
 c.q[1]=0;
}
}
