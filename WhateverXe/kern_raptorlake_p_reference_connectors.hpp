// Fixed read-only connector diagnostics for the exact private RPL-P donor.
#pragma once
#include <stdint.h>
namespace RaptorLakeP {namespace ReferenceConnectors {
struct Platform {
 uint32_t version{1},status{1},identity{},pch{},counts{},memory{},compression{},unified{};
 uint32_t connectors[9][6]{},flags{};
};
template<class View> Platform platform(View &view,bool available){
 Platform out;if(!available)return out;
 out.identity=view.read(0);out.pch=view.read(4);out.counts=view.read(0x10);
 out.memory=view.read(0x14);out.compression=view.read(0x18);out.unified=view.read(0x1C);
 // Retain the header on rejection, but never follow model/voltage pointers.
 if((out.identity&0xFFFF0000U)!=0x9A490000U || (out.counts&255)>1 ||
    ((out.counts>>8)&255)>4 || ((out.counts>>16)&255)>9 || ((out.counts>>24)&255)>3){out.status=2;return out;}
 for(unsigned port=0;port<9;++port)for(unsigned word=0;word<6;++word)
  out.connectors[port][word]=view.read(0x20+24*port+4*word);
 out.flags=view.read(0xF8);out.status=0;return out;
}
struct TypeC {
 uint32_t version{1},status{1},invalid{},count{},powerBefore{},powerAfter{};
 uint32_t values[15]{};
};
// Display-core HPD and TCSS status, then DDI buffer controls. No AUX request,
// ownership change, interrupt acknowledgement, or power-state write.
constexpr uint32_t registers[]{0x44470,0xC4000,0x1638A0,
 0x161500,0x161504,0x161508,0x16150C,
 0x64000,0x64100,0x64200,0x64300,0x64400,0x64500,0x64600,0x64700};
template<class IO> TypeC typeC(IO &io,bool active){
 TypeC out;if(!active)return out;
 out.powerBefore=io.read(0x45404);
 if(out.powerBefore==UINT32_MAX || !(out.powerBefore&0x15400)){out.status=2;return out;}
 for(unsigned i=0;i<15;++i){out.values[i]=io.read(registers[i]);++out.count;
  if(out.values[i]==UINT32_MAX)out.invalid|=1U<<i;
 }
 out.powerAfter=io.read(0x45404);
 out.status=out.powerAfter==UINT32_MAX?2:((out.powerBefore^out.powerAfter)&0x15400)?3:0;
 return out;
}
}}
