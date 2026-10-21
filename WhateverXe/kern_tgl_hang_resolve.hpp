// First-failure CPU evidence only. Raw saved-state bases remain hypotheses.
#pragma once
#include "kern_tgl_hang_detail.hpp"
#include "kern_tgl_sync_audit.hpp"
#include "kern_tgl_tahoe_resolve_receipt.hpp"
namespace TGLHangResolve {
constexpr unsigned MaxPages=12;
struct Scratch {
 uint64_t q[12]; // schema,status,slot,mapped,GPU,CPU,size,class,phase,reserved
 uint64_t pages[6][4]; // CPU, physical before/after, valid mask
};
struct Capture {
 uint64_t q[24]; // schema,status,pages,drops,ACTHD,target,operation,SBA candidates,
 // candidate instruction base, policy before/after, scanned LRC bytes, policy
 // read mask, GPU root before/after, CPU root before/after, owner, reserved
 TGLHangGraph::Page pages[MaxPages];Scratch scratch[4];
};
static_assert(sizeof(Capture)<65536,"bounded separate failure property");
template<class Read,class Physical>
void scratch(Scratch &o,unsigned slot,const TGLTahoeResolveReceipt::Receipt &r,uint64_t mappedClass,Read read,Physical physical){
 memset(&o,0,sizeof(o));o.q[0]=1;o.q[1]=1;o.q[2]=slot;o.q[3]=r.q[3];o.q[8]=r.q[1];
 if(r.q[0]!=1 || r.q[10]!=64 || r.q[13]!=TGLTahoeResolveReceipt::ScratchSize ||
    r.q[17]!=mappedClass || !mappedClass || !TGLHangPending::kernelRange(r.q[3],64))return;
 uint8_t b[64]{},a[64]{};if(read(r.q[3],b,64)!=64)return;
 const unsigned fields[]={0,0x20,0x28,0x38};
 for(auto at:fields)if(TGLTahoeResolveReceipt::pair(b+at)!=TGLTahoeResolveReceipt::pair(r.mapped+at))return;
 auto cpu=TGLTahoeResolveReceipt::pair(b+0x38),gpu=TGLTahoeResolveReceipt::pair(b+0x28),size=TGLTahoeResolveReceipt::pair(b+0x20);
 if(size!=TGLTahoeResolveReceipt::ScratchSize || (cpu&4095) || (gpu&4095) || !gpu || gpu>>47 || size>(1ULL<<47)-gpu || !TGLHangPending::kernelRange(cpu,size))return;
 o.q[4]=gpu;o.q[5]=cpu;o.q[6]=size;o.q[7]=mappedClass;o.q[1]=2;
 for(unsigned i=0;i<6;++i){auto row=o.pages[i];row[0]=cpu+i*4096;
  if(physical(row[0],row[1]))row[3]|=1;
  if(physical(row[0],row[2]))row[3]|=2;
  if(row[3]==3 && row[1]==row[2])row[3]|=4;
 }
 if(read(r.q[3],a,64)!=64)return;
 for(auto at:fields)if(TGLTahoeResolveReceipt::pair(a+at)!=TGLTahoeResolveReceipt::pair(b+at))return;
 o.q[1]=0; // Consistent observations do not pin the object or prove lifetime.
}
template<class Read,class PageReader>
void collect(Capture &o,const TGLHangDetail::Capture &c,Read read,PageReader pageRead){
 memset(&o,0,sizeof(o));o.q[0]=1;o.q[1]=1;
 using namespace TGLHangDetail;
 if(c.q[0]!=Schema || c.q[1] || memcmp(c.before,c.after,sizeof(c.before)) ||
    !c.before[Root] || c.graph.q[1]>TGLHangGraph::MaxPages || c.graph.q[2]>TGLHangGraph::MaxDraws)return;
 const auto acthd=TGLHangPending::u64(c.regs+0x68);o.q[4]=acthd;
 o.q[13]=c.before[Root];o.q[17]=c.before[Owner];
 auto add=[&](uint64_t gpu,uint64_t reason){
  if(!gpu || gpu>>47)return;gpu&=~4095ULL;
  for(unsigned i=0;i<o.q[2];++i)if(o.pages[i].q[0]==gpu){o.pages[i].q[7]|=reason;return;}
  if(o.q[2]>=MaxPages){++o.q[3];return;}
  auto &p=o.pages[o.q[2]++];p.q[0]=gpu;p.q[7]=reason;pageRead(p);
 };
 for(unsigned i=0;i<c.graph.q[1];++i){const auto &p=c.graph.pages[i];
  if(p.q[0]>acthd || acthd-p.q[0]<24 || acthd-p.q[0]>4092 || (acthd&3) ||
     p.q[2]!=4096 || p.q[3]!=1 || p.q[4] || p.q[5]!=1 || p.q[6] ||
     p.before.rootPhysical!=c.before[Root] || p.after.rootPhysical!=c.before[Root])continue;
  const auto off=static_cast<unsigned>(acthd-p.q[0]);if(TGLHangPending::u32(p.bytes+off)!=0x05000000)continue;
  uint32_t words[6];memcpy(words,p.bytes+off-24,24);const auto t=TGLSyncAudit::decode(words,6);
  o.q[6]=t.operation;if(t.status!=TGLSyncAudit::Decode::Write || t.space!=TGLSyncAudit::Space::PPGTT)continue;
  o.q[5]=t.address;add(t.address,1);add(t.address+7,1);
 }
 // Only scan copied LRC pages. A matching packet is not executed-state proof.
 const auto size=c.before[LRCSize];
 if(size<=sizeof(c.lrc))for(unsigned off=0;off+88<=size;off+=4){
  const unsigned first=off/4096,last=(off+87)/4096;
  if(!(c.q[10]&(1ULL<<first)) || !(c.q[10]&(1ULL<<last)))continue;
  o.q[11]+=4;
  if(TGLHangPending::u32(c.lrc+off)!=0x61010014 || !(TGLHangPending::u32(c.lrc+off+40)&1))continue;
  const auto base=TGLHangPending::u64(c.lrc+off+40)&~4095ULL;
  if(!base || base>>47)continue;++o.q[7];o.q[8]=base;add(base,2);add(base+4096,2);
  for(unsigned i=0;i<c.graph.q[2];++i)for(unsigned j=0;j<4;++j){
   const auto &d=c.graph.draws[i];if(!(d.q[2]&(j?TGLHangGraph::PS:TGLHangGraph::VS)))continue;
   const auto gpu=TGLHangGraph::address(base,d.q[6+j]);if(!gpu || gpu>(1ULL<<47)-8192)continue;
   add(gpu,4);add(gpu+4096,4);add(gpu+8191,4);
  }
 }
 uint8_t policy=0;if(TGLHangPending::kernelRange(c.before[Owner],0x1475) && read(c.before[Owner]+0x1474,&policy,1)==1){o.q[9]=policy;o.q[12]|=1;}
 if(TGLHangPending::kernelRange(c.before[Owner],0x1475) && read(c.before[Owner]+0x1474,&policy,1)==1){o.q[10]=policy;o.q[12]|=2;}
 o.q[1]=0;
}
}
