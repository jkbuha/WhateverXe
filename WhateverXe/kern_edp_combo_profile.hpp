// Private TGL Combo A/HBR two-lane trial. Table and RMW contract from Linux
// v6.12 intel_ddi_buf_trans.c (_tgl_combo_phy_trans_dp_hbr) and intel_ddi.c.
// Native PortDriveConfig layout is pinned by the existing donor symbol guard.
#ifndef kern_edp_combo_profile_hpp
#define kern_edp_combo_profile_hpp
#include "kern_edp_pll_fraction.hpp"
namespace EdpComboProfile {
struct Report {uint32_t level=0,result=0,applied=0;};
template<class Read,class Write,class Native>
bool apply(EdpPLLFrac::Scope &scope,uintptr_t thread,void *port,uint32_t swing,uint32_t emphasis,void *overrideConfig,
 Read read,Write write,Native native,Report &report) {
 auto pb=static_cast<uint8_t *>(port);
 auto config=pb?*reinterpret_cast<uint32_t **>(pb+0x548):nullptr;
 if(overrideConfig || !config || config[0]!=0 || config[2]!=0 || config[3]!=0 ||
    pb[0x11]!=2 || *reinterpret_cast<uint32_t *>(pb+0x550)!=1 ||
    *reinterpret_cast<uint32_t *>(pb+0x124)!=2 || *reinterpret_cast<uint32_t *>(pb+0x128)!=10 ||
    swing>3 || emphasis>3 || swing+emphasis>3 || thread<=1 ||
    __atomic_load_n(&scope.thread,__ATOMIC_ACQUIRE)!=thread ||
    __atomic_load_n(&scope.access,__ATOMIC_RELAXED)!=reinterpret_cast<uintptr_t>(*reinterpret_cast<void **>(pb+0x578)))return false;
 static const uint32_t rows[10][5]={
  {0xA,0x32,0x3F,0,0},{0xA,0x4F,0x37,0,8},{0xC,0x71,0x2F,0,0x10},{6,0x7D,0x2B,0,0x14},
  {0xA,0x4C,0x3F,0,0},{0xC,0x73,0x34,0,0xB},{6,0x7F,0x2F,0,0x10},
  {0xC,0x6C,0x3C,0,3},{6,0x7F,0x35,0,0xA},{6,0x7F,0x3F,0,0}};
 static const uint32_t starts[4]={0,4,7,9};
 const auto level=starts[swing]+emphasis;const auto &row=rows[level];
 // Preflight every new register read before changing the native call.
 uint32_t dw4[4],dw7[4];
 for(unsigned lane=0;lane<4;++lane){
  dw4[lane]=read(0x162890+0x100*lane);dw7[lane]=read(0x16289C+0x100*lane);
  if(dw4[lane]==0xFFFFFFFFU || dw7[lane]==0xFFFFFFFFU)return false;
 }
 uint32_t selected[7]={0,0,row[0],(row[4]<<12)|(row[3]<<6)|row[2],0x98,1,0};
 report.level=level;report.result=native(selected);
 if(report.result)return true; // Preserve the native error; no follow-up writes.
 const auto control=read(0x162894);
 if(control==0xFFFFFFFFU)return true;
 // Native enabled TX_TRAINING_EN. Clear it for the remaining TGL RMWs,
 // then re-enable to latch the complete profile before native enablePort.
 write(0x162694,control&~0x80000000U);
 for(unsigned lane=0;lane<4;++lane){
  const auto coeff=(row[4]<<12)|(row[3]<<6)|row[2];
  const auto loadgen=(lane==1 || lane==2)?0x80000000U:0;
  write(0x162890+0x100*lane,(dw4[lane]&0x7FFC0000U)|loadgen|coeff);
  write(0x16289C+0x100*lane,(dw7[lane]&~0x7F000000U)|(row[1]<<24));
 }
 write(0x162694,control|0x80000000U);report.applied=1;return true;
}
}
#endif
