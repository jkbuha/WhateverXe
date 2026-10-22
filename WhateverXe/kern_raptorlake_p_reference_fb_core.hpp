// Bounds for the exact optional reference framebuffer donor; no hardware access.
#ifndef KERN_RPLP_REFERENCE_FB_CORE_HPP
#define KERN_RPLP_REFERENCE_FB_CORE_HPP
#include <stdint.h>
namespace RaptorLakeP {namespace ReferenceFB {
// Exact late-loaded compatibility image, before routing its port export.
inline bool compatImage(const void *image,uint64_t bytes){
 if(!image || bytes<32)return false;
 const auto b=static_cast<const uint8_t *>(image);
 auto word=[&](uint64_t o){return uint32_t(b[o])|uint32_t(b[o+1])<<8|uint32_t(b[o+2])<<16|uint32_t(b[o+3])<<24;};
 if(word(0)!=0xFEEDFACF || word(4)!=0x1000007 || word(16)>128 || word(20)>bytes-32)return false;
 const uint8_t uuid[]{0x7F,0xA0,0x03,0x6E,0x6A,0x61,0x31,0x02,0x98,0x89,0xA0,0x60,0x1F,0x82,0xE3,0xC6};
 uint64_t pos=32,end=32+word(20);bool found=false;
 for(unsigned i=0;i<word(16);++i){
  if(pos>end || end-pos<8)return false;
  auto kind=word(pos),len=word(pos+4);if(len<8 || len>end-pos)return false;
  if(kind==0x1B){if(found || len!=24)return false;for(unsigned k=0;k<16;++k)if(b[pos+8+k]!=uuid[k])return false;found=true;}
  pos+=len;
 }
 return found && pos==end;
}
inline bool portRecord(uint64_t platform,uint64_t record){return platform && platform<=UINT64_MAX-0xF8 && record>=platform+0x20 && record<platform+0xF8 && (record-platform-0x20)%24==0;}
// ADL-P/RPL-P fixed Combo DDI A/B and FIA TC1..4 classifications. Reads only;
// this does not acquire Type-C ownership or claim a connected monitor.
inline uint32_t portMode(uint32_t ddi,uint32_t fia){
 if(ddi<2)return 1;
 if(ddi>5 || fia==0xFFFFFFFF)return 0;
 auto bit=6+8*(ddi-2);
 return (fia&(1U<<bit))?2:(fia&(1U<<(bit-1)))?3:0;
}
inline bool range(uint64_t offset,unsigned width,uint64_t bytes){return (width==4 || width==8) && !(offset&(width-1)) && bytes>=width && offset<=bytes-width;}
inline bool window(uint64_t base,uint64_t offset,unsigned width,uint64_t mmio){return mmio && !(base&7) && base>=mmio && base-mmio<0x1000000 && range(offset,width,0x1000000-(base-mmio));}
inline bool dsb(uint64_t offset,uint64_t bytes,unsigned page){return page==4096 && bytes==0x12000 && !(offset&4095) && offset>=0x1000000 && offset<=0x100000000ULL-bytes;}
}}
#endif
