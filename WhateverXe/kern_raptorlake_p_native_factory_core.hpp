#ifndef KERN_RAPTORLAKE_P_NATIVE_FACTORY_CORE_HPP
#define KERN_RAPTORLAKE_P_NATIVE_FACTORY_CORE_HPP
#include <stdint.h>
namespace RaptorLakeP {namespace NativeFactory {
struct Capability {uint32_t first{},second{};};
// Linux i915_edram.c: HSW_EDRAM_CAP bit0 enables eDRAM. Gen12's uncore
// range 0x40000..0x1BFFFF requires no forcewake (intel_uncore.c).
template<class IO> bool absent(IO &io,Capability &out) {
    out.first=io.read();out.second=io.read();
    return out.first!=UINT32_MAX && out.first==out.second && !(out.first&1);
}
}}
#endif
