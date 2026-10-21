// CPU-only audit of saved captures; never reads a live registry or device.
#include "kern_tgl_hang_detail.hpp"
#include "kern_tgl_sync_audit.hpp"
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
using namespace TGLSyncAudit;
const char *name(Decode d){switch(d){case Decode::Write:return "write";case Decode::NoWrite:return "no-write";case Decode::NotPipeControl:return "not-pipe-control";case Decode::Truncated:return "truncated";case Decode::Malformed:return "malformed";case Decode::Unsupported:return "unsupported";}return "invalid";}
const char *name(Mapping m){switch(m){case Mapping::CapturedWritable:return "captured-writable";case Mapping::NotCaptured:return "not-captured";case Mapping::WrongSpace:return "wrong-address-space";case Mapping::WrongRoot:return "wrong-root";case Mapping::Unstable:return "unstable";case Mapping::NotPresent:return "not-present";case Mapping::ReadOnly:return "read-only";case Mapping::Malformed:return "malformed";}return "invalid";}
void decoded(const Target &t){std::cout<<"\"decode\":\""<<name(t.status)<<"\",\"space\":\""<<(t.space==Space::PPGTT?"PPGTT":"GGTT")<<"\",\"address\":"<<t.address<<",\"operation\":"<<t.operation<<",\"width\":"<<t.width<<",\"flags\":"<<t.flags<<",\"raw_immediate\":"<<t.data;}
int main(int argc,char**argv){
 if(argc==2 && std::string(argv[1])=="--decode-stream"){
  uint32_t p[6];while(std::cin.read(reinterpret_cast<char*>(p),sizeof(p))){std::cout<<'{';decoded(decode(p,6));std::cout<<"}\n";}return std::cin.gcount()?3:0;
 }
 if(argc!=2)return 2;
 auto c=std::make_unique<TGLHangDetail::Capture>();std::ifstream f(argv[1],std::ios::binary|std::ios::ate);if(!f || f.tellg()!=static_cast<std::streamoff>(sizeof(*c)))return 3;
 f.seekg(0);f.read(reinterpret_cast<char*>(c.get()),sizeof(*c));if(!f || c->q[0]!=3 || c->graph.q[1]>TGLHangGraph::MaxPages || c->graph.q[2]>TGLHangGraph::MaxDraws)return 4;
 uint64_t acthd=TGLHangPending::u64(c->regs+0x68),root=c->before[TGLHangDetail::Root];
 std::cout<<"{\"schema\":"<<c->q[0]<<",\"capture_status\":"<<c->q[1]<<",\"context_id\":"<<c->before[TGLHangDetail::ContextID]<<",\"retired\":"<<c->before[TGLHangDetail::Retired]<<",\"root\":"<<root<<",\"acthd\":"<<acthd<<",\"IPEHR\":"<<TGLHangPending::u32(c->regs+0x7c)<<",\"INSTDONE\":"<<TGLHangPending::u32(c->regs+0x20)<<",\"graph_errors\":"<<c->graph.q[5]<<",\"ring_root_status\":"<<c->q[29]<<",\"draws\":[";
 for(unsigned i=0;i<c->graph.q[2];i++){if(i)std::cout<<',';auto &d=c->graph.draws[i];std::cout<<"{\"gpu\":"<<d.q[1]<<",\"known\":"<<d.q[2]<<",\"instruction_base_known\":"<<((d.q[2]&TGLHangGraph::InstructionBase)?"true":"false")<<",\"instruction_base\":"<<d.q[5]<<",\"dwords\":"<<d.q[18]<<",\"provenance\":"<<d.q[20]<<'}';}
 std::cout<<"],\"preceding_sync\":[";unsigned found=0;
 for(unsigned i=0;i<c->graph.q[1];i++){auto &p=c->graph.pages[i];if(acthd<p.q[0]+24 || acthd>=p.q[0]+4096 || p.q[2]!=4096)continue;
  const auto offset=static_cast<unsigned>(acthd-p.q[0]);if((offset&3) || offset>4092 || TGLHangPending::u32(p.bytes+offset)!=0x05000000)continue;
  uint32_t words[6];memcpy(words,p.bytes+offset-24,sizeof(words));auto t=decode(words,6);if(t.status!=Decode::Write)continue;
  if(found++)std::cout<<',';auto e=mapping(t,root,c->graph);
  std::cout<<"{\"pc_gpu\":"<<acthd-24<<",\"command_page_provenance\":"<<p.q[7]<<",\"command_page_stable\":"<<((p.q[3]==1 && !p.q[4] && p.q[5]==1 && !p.q[6])?"true":"false")<<',';decoded(t);
  std::cout<<",\"target_mapping\":\""<<name(e.status)<<"\",\"target_physical\":"<<e.physical<<",\"cpu_scratch_physical_captured\":false,\"alias_identity\":\"unknown\",\"raw\":[";for(unsigned j=0;j<6;j++){if(j)std::cout<<',';std::cout<<words[j];}std::cout<<"]}";
 }
 unsigned sba=0;for(unsigned off=0;off+88<=sizeof(c->lrc);off+=4)if(TGLHangPending::u32(c->lrc+off)==0x61010014)++sba;
 std::cout<<"],\"raw_lrc_sba_candidates\":"<<sba<<",\"sba_execution_proven\":false,\"render_success_proven\":false}\n";
}
