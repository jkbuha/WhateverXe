// One bounded, reversible TC3 AUX diagnostic for the exact RPL-P backend.
// Register/sequence reference: Linux v6.12 intel_tc.c (adlp_tc_phy_connect),
// intel_display_power_map.c (XE_LPD) and intel_dp_aux_regs.h. No FIA writes.
#pragma once
#include <stdint.h>
namespace RaptorLakeP {namespace ReferenceTCAux {
enum Status:uint32_t {Complete,Denied,Preflight,DDIPower,Foreign,Ownership,AuxBusy,AuxPower,Disconnected,DPCD,Rollback};
struct Result {uint32_t version{1},status{Denied},writes{},nativeResult{~0U},before[4]{},after[4]{},dpcd[4]{};};
template<class IO> Result probe(IO &io,bool allowed){
 Result out;if(!allowed)return out;
 constexpr uint32_t request=1U<<11,state=1U<<10,owned=1U<<6,busy=1U<<31,tbt=1U<<11;
 bool ddiToken=false,auxToken=false,phyToken=false,controlToken=false;
 auto connected=[&](){auto v=io.read(0x161508),h=io.read(0x44470);return v!=~0U && (v&7)==5 && h!=~0U && (h&(1U<<18)) && !(h&(1U<<2));};
 auto write=[&](uint32_t r,uint32_t v){io.write(r,v);++out.writes;};
 auto wait=[&](uint32_t r,uint32_t mask,unsigned ms){for(unsigned i=0;i<=ms;++i){auto v=io.read(r);if(v==~0U)return false;if((v&mask)==mask)return true;if(i<ms)io.pause();}return false;};
 auto core=io.read(0x45404);
 if(core==~0U || (core&0xC03)!=0xC03 || !connected()){out.status=Preflight;return out;}
 out.before[0]=io.read(0x45454);out.before[1]=io.read(0x45444);
 if(out.before[0]==~0U || out.before[1]==~0U){out.status=Preflight;return out;}
 // Display's per-port power request is a separate token from PHY ownership.
 if(!(out.before[0]&request)){ddiToken=true;write(0x45454,out.before[0]|request);}
 out.status=DDIPower;
 if(wait(0x45454,state,50)){
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
