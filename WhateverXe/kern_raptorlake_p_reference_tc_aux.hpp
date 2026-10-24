// One bounded, reversible TC3 AUX diagnostic for the exact RPL-P backend.
// Register/sequence reference: Linux v6.12 intel_tc.c (adlp_tc_phy_connect),
// intel_display_power_map.c (XE_LPD) and intel_dp_aux_regs.h. No FIA writes.
#pragma once
#include <stdint.h>
namespace RaptorLakeP {namespace ReferenceTCAux {
enum Status:uint32_t {Complete,Denied,Preflight,DDIPower,Foreign,Ownership,AuxBusy,AuxPower,Disconnected,DPCD,Rollback};
struct Result {uint32_t version{1},status{Denied},writes{},nativeResult{~0U},before[4]{},after[4]{},dpcd[4]{};};
template<class IO> Result probe(IO &io,bool allowed,bool ddiLanes=false){
 Result out;if(!allowed)return out;
 constexpr uint32_t request=1U<<11,state=1U<<10,owned=1U<<6,busy=1U<<31,tbt=1U<<11;
 bool ddiToken=false,auxToken=false,phyToken=false,controlToken=false;
 auto connected=[&](){auto v=io.read(0x161508),h=io.read(0x44470);return v!=~0U && (v&7)==5 && h!=~0U && (h&(1U<<18)) && !(h&(1U<<2));};
 auto write=[&](uint32_t r,uint32_t v){io.write(r,v);++out.writes;};
 auto wait=[&](uint32_t r,uint32_t mask,unsigned ms){for(unsigned i=0;i<=ms;++i){auto v=io.read(r);if(v==~0U)return false;if((v&mask)==mask)return true;if(i<ms)io.pause();}return false;};
 auto core=io.read(0x45404);
 if(core==~0U || (core&0xC03)!=0xC03 || (ddiLanes && (core&12)!=12) || !connected()){out.status=Preflight;return out;}
 out.before[0]=io.read(0x45454);out.before[1]=io.read(0x45444);
 if(out.before[0]==~0U || out.before[1]==~0U){out.status=Preflight;return out;}
 // Legacy DDI-I/O and XE_LPD DDI-lanes are distinct domains. In the
 // latter case the caller must hold acknowledged PW2; never request a
 // TGL DDI-I/O well that the ADL-P PHY does not use for ownership.
 if(!ddiLanes && !(out.before[0]&request)){ddiToken=true;write(0x45454,out.before[0]|request);}
 out.status=DDIPower;
 if(ddiLanes || wait(0x45454,state,50)){
  out.before[2]=io.read(0x64500);out.status=Foreign;
  if(out.before[2]!=~0U && !(out.before[2]&(busy|owned))){
   phyToken=true;write(0x64500,out.before[2]|owned);out.status=Ownership;
   if((io.read(0x64500)&(busy|owned))==owned && connected()){
    out.before[3]=io.read(0x64510);out.status=AuxBusy;
    if(out.before[3]!=~0U && !(out.before[3]&busy)){
     if(out.before[3]&tbt){controlToken=true;write(0x64510,out.before[3]&~tbt);}
     auto power=io.read(0x45444);out.status=AuxPower;
     if(power!=~0U){
      if(!(power&request)){auxToken=true;write(0x45444,power|request);}
      if(wait(0x45444,state,500)){
       out.status=Disconnected;
       if(connected()){
        uint8_t data[16]{};out.nativeResult=io.readDPCD(data,16);
        for(unsigned i=0;i<16;++i)out.dpcd[i/4]|=uint32_t(data[i])<<(8*(i%4));
        auto lanes=data[2]&31;
        out.status=!out.nativeResult && data[0]>=0x10 && data[0]<=0x22 && (lanes==1 || lanes==2 || lanes==4)?Complete:DPCD;
       }
      }
     }
    }
   }
  }
 }
 // Release only tokens acquired here; preserve other request bits. If a
 // foreign transition unexpectedly starts scanout, retain its power/ownership
 // rather than cutting an active link out from underneath it.
 auto buffer=phyToken?io.read(0x64500):0;
 if(phyToken && (buffer==~0U || (buffer&busy))){out.status=Rollback;return out;}
 if(auxToken){auto v=io.read(0x45444);if(v==~0U){out.status=Rollback;return out;}write(0x45444,v&~request);}
 if(controlToken){auto v=io.read(0x64510);if(v==~0U || (v&busy)){out.status=Rollback;return out;}write(0x64510,(v&~tbt)|(out.before[3]&tbt));}
 if(phyToken)write(0x64500,buffer&~owned);
 if(phyToken){out.after[2]=io.read(0x64500);out.after[3]=io.read(0x64510);}
 if(ddiToken){auto v=io.read(0x45454);if(v==~0U){out.status=Rollback;return out;}write(0x45454,v&~request);}
 out.after[0]=io.read(0x45454);out.after[1]=io.read(0x45444);
 if((ddiToken&&(out.after[0]&request)) || (auxToken&&(out.after[1]&request)) || (phyToken&&(out.after[2]&owned)))out.status=Rollback;
 return out;
}
}}

namespace RaptorLakeP {namespace ReferenceTCAux {
// XE_LPD PW2 is the parent of USB-C DDI/AUX domains. PW_A does not need it.
// Reference: Linux v6.12 intel_display_power_map.c XELPD_PW_2_POWER_DOMAINS.
// Keep the qualified inner transaction separate; never leave its children
// without a parent if rollback detects a foreign transition.
struct ParentResult {
 Result aux;
 uint32_t coreBefore{},coreRequested{},coreAfter{},parentFlags{};
};
template<class IO> ParentResult probeWithParent(IO &io,bool allowed){
 ParentResult out;out.aux.version=2;
 if(!allowed)return out;
 constexpr uint32_t reg=0x45404,request=8,state=4,children=0x3F000;
 constexpr uint32_t parentPower=11,parentFuse=12;
 auto core=io.read(reg),tc=io.read(0x161508),hpd=io.read(0x44470);
 out.coreBefore=core;
 if(core==~0U || (core&0xC03)!=0xC03 || tc==~0U || (tc&7)!=5 || hpd==~0U || !(hpd&(1U<<18)) || (hpd&(1U<<2))){out.aux.status=Preflight;return out;}
 const bool token=!(core&request);out.parentFlags=token?1:2;
 auto write=[&](uint32_t r,uint32_t v){io.write(r,v);++out.aux.writes;};
 auto wait=[&](uint32_t r,uint32_t bits,unsigned ms){for(unsigned i=0;i<=ms;i++){auto v=io.read(r);if(v==~0U)return false;if((v&bits)==bits)return true;if(i<ms)io.pause();}return false;};
 // Reject an inconsistent pre-existing child state before taking a token.
 if(token && (core&children)){out.aux.status=Foreign;return out;}
 if(token)write(reg,core|request);
 out.coreRequested=io.read(reg);out.aux.status=parentPower;
 if(wait(reg,state,50)){
  out.aux.status=parentFuse;
  // SKL_FUSE_PG_DIST_STATUS(PG2) = bit25; this is distribution, not a fuse override.
  if(wait(0x42000,1U<<25,5)){
   auto inner=probe(io,true,true);const uint32_t parentWrites=out.aux.writes;
   out.aux=inner;out.aux.version=2;out.aux.writes+=parentWrites;
  }
 }
 if(token){
  auto now=io.read(reg),ddi=io.read(0x45454),aux=io.read(0x45444);
  // Requests for any non-A/B DDI/AUX child indicate another owner. Preserve
  // the parent in that case rather than cutting a foreign active link.
  constexpr uint32_t childRequests=0xAAAAAAA0U;
  if(out.aux.status==Rollback || now==~0U || ddi==~0U || aux==~0U || (now&children) || ((ddi|aux)&childRequests)){
   out.parentFlags|=4;out.aux.status=Rollback;out.coreAfter=now;return out;
  }
  write(reg,now&~request);
 }
 out.coreAfter=io.read(reg);
 if(out.coreAfter==~0U || (token&&(out.coreAfter&request))){out.aux.status=Rollback;out.parentFlags|=8;}
 return out;
}
}}
