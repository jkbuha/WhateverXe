// Policy for the optional RPL-P backend; independent of presentation IDs.
#ifndef KERN_RAPTORLAKE_P_REFERENCE_CORE_HPP
#define KERN_RAPTORLAKE_P_REFERENCE_CORE_HPP
#include "kern_raptorlake_p.hpp"
#include "kern_xelp_memory_layout.hpp"
namespace RaptorLakeP {namespace Reference {
inline bool requested(bool reference,bool panelCapable,bool panel,bool manual){return manual && (reference || (panelCapable && panel));}
inline bool physical(const XELP::Profile &p,XELP::OS os,const XELPMemory::Layout &m){
 return eligible(p,os,true) && m.valid && m.bytes==0x4000000 && m.gttBytes==0x800000 && m.apertureBytes==0x10000000;
}
inline bool armable(const uint32_t (&s)[9]){
 return s[0]==1 && s[1]==1 && s[2]==1 && s[3]==1 && !s[4] && s[5]==1 && !s[6] && !s[7] && s[8]==1;
}
}}
#endif
