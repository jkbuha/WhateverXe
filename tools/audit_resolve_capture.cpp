// Offline decoder for the separate first-failure property. No live access.
#include "kern_tgl_hang_resolve.hpp"
#include <fstream>
#include <memory>
#include <iostream>
int main(int argc,char **argv){
 if(argc!=2)return 2;auto c=std::make_unique<TGLHangResolve::Capture>();std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
 if(!f || f.tellg()!=static_cast<std::streamoff>(sizeof(*c)))return 3;f.seekg(0);f.read((char*)c.get(),sizeof(*c));
 if(!f || c->q[0]!=1 || c->q[2]>TGLHangResolve::MaxPages)return 4;
 std::cout<<"{\"status\":"<<c->q[1]<<",\"target\":"<<c->q[5]<<",\"SBA_candidates\":"<<c->q[7]<<",\"drops\":"<<c->q[3]<<",\"GPU_root\":"<<c->q[13]<<",\"root_rechecks_match\":"<<((c->q[13] && c->q[13]==c->q[14] && c->q[15] && c->q[15]==c->q[16])?"true":"false")<<",\"policy_read_mask\":"<<c->q[12]<<",\"policy_before\":"<<c->q[9]<<",\"policy_after\":"<<c->q[10]<<",\"pages\":[";
 auto graph=std::make_unique<TGLHangGraph::Capture>();graph->q[1]=c->q[2];
 for(unsigned i=0;i<c->q[2];++i){graph->pages[i]=c->pages[i];auto&p=c->pages[i];if(i)std::cout<<',';
  std::cout<<"{\"GPU\":"<<p.q[0]<<",\"reason_mask\":"<<p.q[7]<<",\"copy_status\":"<<p.q[4]<<",\"copied\":"<<p.q[2]<<",\"physical\":"<<p.before.physical<<",\"pte\":"<<p.before.pte<<",\"chain_mismatch\":"<<(p.chainBefore.q[6]|p.chainAfter.q[6])<<'}';
 }
 std::cout<<"],\"scratch_aliases\":[";
 TGLSyncAudit::Target t{TGLSyncAudit::Decode::Write,TGLSyncAudit::Space::PPGTT,c->q[5],0,0,1,8};auto e=TGLSyncAudit::mapping(t,c->q[13],*graph);
 for(unsigned i=0;i<4;++i){auto&s=c->scratch[i];if(i)std::cout<<',';const char *alias="unknown";
  if(!c->q[1] && c->q[13]==c->q[14] && c->q[15] && c->q[15]==c->q[16] && s.q[0]==1 && !s.q[1] && s.q[6]==0x5100 && t.address>=s.q[4] && t.address-s.q[4]<=s.q[6]-8){
   const auto offset=t.address-s.q[4];const auto index=offset/4096;
   if(index<6 && s.pages[index][3]==7 && s.pages[index][1]==s.pages[index][2]){
    const auto a=TGLSyncAudit::alias(e,s.pages[index][1]+(offset&4095));
    if(a==TGLSyncAudit::Alias::Match)alias="match";if(a==TGLSyncAudit::Alias::Mismatch)alias="mismatch";
   }
  }
  std::cout<<"{\"slot\":"<<i<<",\"schema\":"<<s.q[0]<<",\"status\":"<<s.q[1]<<",\"GPU_base\":"<<s.q[4]<<",\"CPU_base\":"<<s.q[5]<<",\"target_physical_identity\":\""<<alias<<"\"}";
 }
 std::cout<<"],\"executed_shader_base_proven\":false,\"render_success_proven\":false}\n";
}
