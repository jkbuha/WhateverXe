// ADL-P Dekel signal coefficients and lane wakeup. Register values follow
// Linux v6.12 intel_ddi_buf_trans.c / intel_ddi.c / intel_dkl_phy_regs.h.
// These are physical programming values, not a copy of the donor's TGL table.
#pragma once
#include "kern_display13_tc.hpp"
namespace RaptorLakeP {namespace ReferenceVoltage {
struct Drive {uint32_t swing,preshoot,emphasis;};
inline bool select(unsigned swing,unsigned emphasis,unsigned rate,Drive &out){
 if(swing>3 || emphasis>3 || swing+emphasis>3 || (rate!=6&&rate!=10&&rate!=20&&rate!=30))return false;
 constexpr Drive low[]{ {7,0,1},{5,0,6},{2,0,11},{0,0,23},{5,0,0},{2,0,8},{0,0,20},{2,0,0},{0,0,11},{0,0,0} };
 constexpr Drive high[]{{7,0,0},{5,0,4},{2,0,10},{0,0,24},{5,0,0},{2,0,6},{0,0,20},{2,0,0},{0,0,9},{0,0,0}};
 const unsigned index=4*swing-swing*(swing? swing-1:0)/2+emphasis;
 out=(rate>10?high:low)[index];return true;
}
// FIA2 stores logical enables; native TGL PHY routines consume a physical
// lane mask. Compact logical lane 0 maps to the first allocated physical lane.
inline uint32_t physicalMask(uint32_t configured,uint32_t allocated){
 if(configured!=1 && configured!=3 && configured!=15)return 0;
 if(allocated!=1 && allocated!=2 && allocated!=4 && allocated!=8 && allocated!=3 && allocated!=12 && allocated!=15)return 0;
 uint32_t out=0;unsigned logical=0;
 for(unsigned lane=0;lane<4;lane++)if(allocated&(1U<<lane)){if(configured&(1U<<logical))out|=1U<<lane;++logical;}
 return (configured>>logical)?0:out;
}
// Both Dekel lane groups are programmed by the ADL-P signal sequence, even
// when only two DP lanes are assigned. Preserve the other ports' HIP fields.
template<class IO> Display13TC::Status wake(IO &io,const Display13TC::Port &p){
 Display13TC::Port expected;
 if(!Display13TC::Port::resolve(5,expected) || p.ddi!=5 || p.phy!=expected.phy || p.hip!=expected.hip || p.shift!=expected.shift)return Display13TC::Status::Invalid;
 if(!io.powerHeld())return Display13TC::Status::NoPower;
 auto irq=io.lock();const auto saved=io.read(p.hip);auto status=Display13TC::Status::Complete;
 if(saved==~0U){io.unlock(irq);return Display13TC::Status::Unavailable;}
 for(unsigned group=0;group<2;group++){
  auto internal=0xD00U+0x1000U*group,selected=p.bank(saved,internal),reg=p.phyRegister(internal);
  io.write(p.hip,selected);if(io.read(p.hip)!=selected){status=Display13TC::Status::Readback;break;}
  if(io.read(reg)==~0U){status=Display13TC::Status::Unavailable;break;}
  io.write(reg,0);if(io.read(reg)){status=Display13TC::Status::Readback;break;}
 }
 io.write(p.hip,saved);if(io.read(p.hip)!=saved){io.bankFault();status=Display13TC::Status::Readback;}
 io.unlock(irq);return status;
}
inline uint32_t dpControl(uint32_t value){return value&~0x7CU;}
inline uint32_t framing(uint32_t value,bool enhanced){return (value&~(1U<<18))|(enhanced?1U<<18:0);}
}}
