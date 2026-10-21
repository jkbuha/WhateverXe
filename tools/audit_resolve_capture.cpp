// Offline decoder for the separate first-failure property. No live access.
#include "kern_tgl_hang_resolve.hpp"
#include <fstream>
#include <memory>
#include <iostream>
int main(int argc,char **argv){
 if(argc!=2)return 2;auto c=std::make_unique<TGLHangResolve::Capture>();std::ifstream f(argv[1],std::ios::binary|std::ios::ate);
 const auto bytes=f?static_cast<std::streamoff>(f.tellg()):std::streamoff(-1);
 const auto legacy=static_cast<std::streamoff>(offsetof(TGLHangResolve::Capture,selected));
 if(bytes!=legacy && bytes!=static_cast<std::streamoff>(sizeof(*c)))return 3;f.seekg(0);f.read((char*)c.get(),bytes);
 if(!f || !((c->q[0]==1 && bytes==legacy) || (c->q[0]==2 && bytes==static_cast<std::streamoff>(sizeof(*c)))) || c->q[2]>TGLHangResolve::MaxPages)return 4;
 std::cout<<"{\"status\":"<<c->q[1]<<",\"target\":"<<c->q[5]<<",\"SBA_candidates\":"<<c->q[7]<<",\"drops\":"<<c->q[3]<<",\"GPU_root\":"<<c->q[13]<<",\"root_rechecks_match\":"<<((c->q[13] && c->q[13]==c->q[14] && c->q[15] && c->q[15]==c->q[16])?"true":"false")<<",\"policy_read_mask\":"<<c->q[12]<<",\"policy_before\":"<<c->q[9]<<",\"policy_after\":"<<c->q[10]<<",\"pages\":[";
 auto graph=std::make_unique<TGLHangGraph::Capture>();graph->q[1]=c->q[2];
 for(unsigned i=0;i<c->q[2];++i){graph->pages[i]=c->pages[i];auto&p=c->pages[i];if(i)std::cout<<',';
  std::cout<<"{\"GPU\":"<<p.q[0]<<",\"reason_mask\":"<<p.q[7]<<",\"copy_status\":"<<p.q[4]<<",\"copied\":"<<p.q[2]<<",\"physical\":"<<p.before.physical<<",\"pte\":"<<p.before.pte<<",\"chain_mismatch\":"<<(p.chainBefore.q[6]|p.chainAfter.q[6])<<'}';
 }
 std::cout<<"],\"selected_task\":"<<c->q[18]<<",\"resolve_context\":"<<c->q[19]<<",\"selected_mapped\":"<<c->q[20]<<",\"association_status\":"<<c->q[23]<<",\"scratch_aliases\":[";
 TGLSyncAudit::Target t{TGLSyncAudit::Decode::Write,TGLSyncAudit::Space::PPGTT,c->q[5],0,0,1,8};auto e=TGLSyncAudit::mapping(t,c->q[13],*graph);
 for(unsigned i=0;i<5;++i){auto&s=i<4?c->scratch[i]:c->selected;if(i)std::cout<<',';const char *alias="unknown";const bool ownerQualified=!c->q[1] && c->q[13] && c->q[13]==c->q[14] && c->q[15] && c->q[15]==c->q[16] && c->q[0]==2 && s.q[0]==2 && c->q[18] && s.q[8]==c->q[18] && (i<4 || (!c->q[23] && c->q[20]==s.q[3]));
  if(!c->q[1] && c->q[13]==c->q[14] && c->q[15] && c->q[15]==c->q[16] && (s.q[0]==1 || s.q[0]==2) && !s.q[1] && (s.q[6]==0x5100 || s.q[6]==0x6000) && t.address>=s.q[4] && t.address-s.q[4]<=s.q[6]-8){
   const auto offset=t.address-s.q[4];const auto index=offset/4096;
   if(index<6 && s.pages[index][0]==s.q[5]+index*4096 && s.pages[index][3]==7 && s.pages[index][1]==s.pages[index][2]){
    const auto a=TGLSyncAudit::alias(e,s.pages[index][1]+(offset&4095));
    if(a==TGLSyncAudit::Alias::Match)alias="match";if(a==TGLSyncAudit::Alias::Mismatch)alias="mismatch";
   }
  }
  std::cout<<"{\"slot\":"<<i<<",\"schema\":"<<s.q[0]<<",\"status\":"<<s.q[1]<<",\"GPU_base\":"<<s.q[4]<<",\"CPU_base\":"<<s.q[5]<<",\"map_task\":"<<(s.q[0]==2?s.q[8]:0)<<",\"address_space_qualified\":"<<(ownerQualified?"true":"false")<<",\"map\":"<<s.q[9]<<",\"getter\":"<<s.q[10]<<",\"map_flags\":"<<s.q[11]<<",\"target_physical_identity\":\""<<alias<<"\"}";
 }
 std::cout<<"],\"selected_page_identities\":[";
 const auto &selected=c->selected;
 for(unsigned i=0;i<6;++i){if(i)std::cout<<',';const char *identity="unknown";
  if(c->q[0]==2 && !c->q[1] && !c->q[23] && !selected.q[1] && selected.q[0]==2 && c->q[13]==c->q[14] && c->q[15] && c->q[15]==c->q[16]){
   auto probe=t;probe.address=selected.q[4]+i*4096;auto evidence=TGLSyncAudit::mapping(probe,c->q[13],*graph);const auto row=selected.pages[i];
   if(row[0]==selected.q[5]+i*4096 && row[3]==7 && row[1]==row[2]){
    auto result=TGLSyncAudit::alias(evidence,row[1]);if(result==TGLSyncAudit::Alias::Match)identity="match";if(result==TGLSyncAudit::Alias::Mismatch)identity="mismatch";
   }
  }
  std::cout<<"{\"page\":"<<i<<",\"physical_identity\":\""<<identity<<"\"}";
 }
 std::cout<<"],\"executed_shader_base_proven\":false,\"render_success_proven\":false}\n";
}
