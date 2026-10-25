// Bounded, read-only Display 13 snapshot; no arbitrary register requests.
#pragma once
#include <stdint.h>
namespace RaptorLakeP {namespace ReferenceSnapshot {
enum Status : uint32_t {Complete,Unavailable,PowerChanged,BufferChanged};
struct Register {uint32_t offset,value;};
struct Snapshot {
 uint32_t version{1},sequence{},status{Complete},count{},powerBefore{},powerAfter{},poweredPipes{},activePipes{};
 Register registers[192]{};
};
template<class IO> Snapshot capture(IO &io,uint32_t sequence){
 Snapshot out;out.sequence=sequence;
 auto add=[&](uint32_t reg){
  if(out.count>=192 || !io.readable(reg)){out.status=Unavailable;return UINT32_MAX;}
  auto value=io.read(reg);out.registers[out.count++]={reg,value};
  if(value==UINT32_MAX)out.status=Unavailable;
  return value;
 };
 constexpr uint32_t power=0x45404,mask=0x15400;
 out.powerBefore=add(power);if(out.status!=Complete)return out;
 constexpr uint32_t local[]{0x70008,0x60000,0x60004,0x60008,0x6000C,0x60010,0x60014,0x6001C,
  0x70080,0x70280,0x70380,0x70480,0x70580,0x70680,0x70780,0x70180,0x70188,0x7018C,0x70190,0x7019C,0x701A4,0x701AC};
 for(unsigned pipe=0;pipe<4 && out.status==Complete;++pipe){
  if(!(out.powerBefore&(1U<<(2*(5+pipe)))))continue;
  auto now=add(power);if(out.status!=Complete)return out;
  if((now&mask)!=(out.powerBefore&mask)){out.status=PowerChanged;return out;}
  out.poweredPipes|=1U<<pipe;
  auto function=add(0x60400+0x1000*pipe);if(out.status!=Complete)return out;
  if(!(function&0x80000000U))continue;
  out.activePipes|=1U<<pipe;
  uint32_t control=0,live=0;
  for(auto reg:local){auto value=add(reg+0x1000*pipe);if(out.status!=Complete)return out;
   if(reg==0x70180)control=value;
   if(reg==0x701AC)live=value;
  }
  // Two adjacent GGTT entries for the currently latched scanout address.
  // A 32-bit surface address selects entries entirely inside this 16 MiB BAR.
  const auto surface=live&0xFFFFF000U;
  if((control&0x80000000U) && surface<=0xFFFFE000U){
   auto pte=0x800000U+(surface>>9);
   for(unsigned word=0;word<4;++word){add(pte+4*word);if(out.status!=Complete)return out;}
  }
  if(add(0x701AC+0x1000*pipe)!=live && out.status==Complete)out.status=BufferChanged;
 }
 if(out.status!=Complete)return out;
 if(out.activePipes){
  constexpr uint32_t globals[]{0x43200,0x43208,0x43224,0x45000,0x4208C,0x45504};
  for(auto reg:globals){
   add(reg);if(out.status!=Complete)return out;
  }
  for(unsigned fence=0;fence<32;++fence)for(unsigned word=0;word<2;++word){
   add(0x100000+8*fence+4*word);if(out.status!=Complete)return out;
  }
 }
 out.powerAfter=add(power);
 if(out.status==Complete && (out.powerAfter&mask)!=(out.powerBefore&mask))out.status=PowerChanged;
 return out;
}
// Separate property with the same bounded wire layout. Read only powered,
// active pipe banks; gamma/CSC offsets do not share the pipe's 0x1000 stride.
// Register definitions: Linux v6.12 intel_color_regs.h and i915_reg.h.
template<class IO> Snapshot captureColor(IO &io,uint32_t sequence){
 Snapshot out;out.sequence=sequence;
 auto add=[&](uint32_t reg){
  if(out.count>=192 || !io.readable(reg)){out.status=Unavailable;return UINT32_MAX;}
  auto value=io.read(reg);out.registers[out.count++]={reg,value};
  if(value==UINT32_MAX)out.status=Unavailable;
  return value;
 };
 constexpr uint32_t power=0x45404,mask=0x15400;
 out.powerBefore=add(power);if(out.status!=Complete)return out;
 for(unsigned pipe=0;pipe<4 && out.status==Complete;++pipe){
  if(!(out.powerBefore&(1U<<(2*(5+pipe)))))continue;
  auto now=add(power);if(out.status!=Complete)return out;
  if((now&mask)!=(out.powerBefore&mask)){out.status=PowerChanged;return out;}
  out.poweredPipes|=1U<<pipe;
  auto function=add(0x60400+0x1000*pipe);if(out.status!=Complete)return out;
  if(!(function&0x80000000U))continue;
  out.activePipes|=1U<<pipe;
  add(0x70030+0x1000*pipe);add(0x701CC+0x1000*pipe);
  add(0x4A480+0x800*pipe);add(0x49028+0x100*pipe);
  // Pipe CSC matrix and offsets, output CSC matrix, and sparse legacy LUT.
  constexpr uint32_t csc[]{0x49010,0x49014,0x49018,0x4901C,0x49020,0x49024,
   0x49030,0x49034,0x49038,0x49040,0x49044,0x49048,
   0x49050,0x49054,0x49058,0x4905C,0x49060,0x49064};
  for(auto reg:csc){if(out.status!=Complete)return out;add(reg+0x100*pipe);}
  constexpr unsigned entries[]{0,128,255};
  for(auto entry:entries){if(out.status!=Complete)return out;add(0x4A000+0x800*pipe+4*entry);}
 }
 if(out.status!=Complete)return out;
 out.powerAfter=add(power);
 if(out.status==Complete && (out.powerAfter&mask)!=(out.powerBefore&mask))out.status=PowerChanged;
 return out;
}
}}
