// Offline interpretation of bounded captured synchronization evidence.
// Shared with the failure observer. Unknown capture coverage is not a fault.
#pragma once
#include "kern_tgl_hang_graph.hpp"
namespace TGLSyncAudit {
enum class Decode {Write,NoWrite,NotPipeControl,Truncated,Malformed,Unsupported};
enum class Space {PPGTT,GGTT};
struct Target {Decode status;Space space;uint64_t address,data;uint32_t flags,operation,width;};
inline Target decode(const uint32_t *p,size_t n){
 Target t{Decode::Truncated,Space::PPGTT,0,0,0,0,0};if(!p || !n)return t;
 if((p[0]&0xFFFF0000U)!=0x7A000000U){t.status=Decode::NotPipeControl;return t;}
 if((p[0]&255)!=4 || (p[0]&0xFFFFU&~0x6FFU)){t.status=Decode::Malformed;return t;}
 if(n<6)return t;t.flags=p[1];t.operation=(p[1]>>14)&3;t.space=(p[1]&(1U<<24))?Space::GGTT:Space::PPGTT;
 if(p[1]&((1U<<6)|(1U<<25)|(1U<<31))){t.status=Decode::Malformed;return t;}
 if(p[1]&((1U<<21)|(1U<<23))){t.status=Decode::Unsupported;return t;}
 if(!t.operation){t.status=Decode::NoWrite;return t;}
 if((p[2]&3) || (p[3]&0xFFFF0000U)){t.status=Decode::Malformed;return t;}
 t.address=uint64_t(p[2])|(uint64_t(p[3])<<32);t.data=uint64_t(p[4])|(uint64_t(p[5])<<32);
 if(!t.address || t.address>>47 || t.address>0x800000000000ULL-8){t.status=Decode::Malformed;return t;}
 t.width=8;t.status=Decode::Write;return t;
}
enum class Mapping {CapturedWritable,NotCaptured,WrongSpace,WrongRoot,Unstable,NotPresent,ReadOnly,Malformed};
struct Evidence {Mapping status;uint64_t physical;unsigned pages;};
inline Evidence mapping(const Target &t,uint64_t root,const TGLHangGraph::Capture &c){
 Evidence out{Mapping::Malformed,0,0};if(t.status!=Decode::Write || t.width!=8 || !root || (root&4095) || c.q[1]>TGLHangGraph::MaxPages)return out;
 if(t.space!=Space::PPGTT){out.status=Mapping::WrongSpace;return out;}
 for(uint64_t page=t.address&~4095ULL;page<=((t.address+7)&~4095ULL);page+=4096){
  const TGLHangGraph::Page *found=nullptr;
  for(unsigned i=0;i<c.q[1];i++)if(c.pages[i].q[0]==page){if(found)return out;found=c.pages+i;}
  if(!found){out.status=Mapping::NotCaptured;return out;}const auto &p=*found;
  if(p.before.rootPhysical!=root || p.after.rootPhysical!=root || p.chainBefore.q[4]!=root || p.chainAfter.q[4]!=root){out.status=Mapping::WrongRoot;return out;}
  if(p.q[2]!=4096 || p.q[3]!=1 || p.q[4] || p.q[5]!=1 || p.q[6] || p.before.status || p.after.status ||
   p.chainBefore.q[1] || p.chainAfter.q[1] || p.chainBefore.q[6] || p.chainAfter.q[6] || memcmp(&p.before,&p.after,sizeof(p.before))){out.status=Mapping::Unstable;return out;}
  if(!(p.before.pte&1)){out.status=Mapping::NotPresent;return out;}
  if(!(p.before.pte&2)){out.status=Mapping::ReadOnly;return out;}
  if(p.before.physical!=(p.before.pte&0x7FFFFFF000ULL)){out.status=Mapping::Malformed;return out;}
  if(!out.pages)out.physical=p.before.physical+(t.address&4095);++out.pages;
 }
 out.status=Mapping::CapturedWritable;return out;
}
enum class Alias {Unknown,Match,Mismatch};
inline Alias alias(const Evidence &gpu,uint64_t independentlyCapturedCPUPhysical){
 if(gpu.status!=Mapping::CapturedWritable || gpu.pages!=1 || !independentlyCapturedCPUPhysical)return Alias::Unknown;
 return gpu.physical==independentlyCapturedCPUPhysical?Alias::Match:Alias::Mismatch;
}
}
