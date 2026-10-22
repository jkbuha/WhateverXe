#ifndef KERN_RAPTORLAKE_P_GTT_APERTURE_CORE_HPP
#define KERN_RAPTORLAKE_P_GTT_APERTURE_CORE_HPP
#include <stdint.h>
namespace RaptorLakeP {namespace GTTAperture {
struct Result {
    uint64_t version{1},requested{},mapped{},flushed{},checked{},first{},last{},
        mismatches{},firstMismatch{UINT64_MAX},mismatchValue{},released{},failure{};
};
// Read one page only, after the native PTE's exact readback. No aperture write
// or GPU command is issued. The caller restores the PTE even on mismatch.
template<class IO> bool verify(IO &io,Result &out) {
    if(!out.requested || !out.mapped || !out.flushed){out.failure=4;return false;}
    for(unsigned i=0;i<1024;++i) {
        const auto word=io.readWord(i);
        if(!i)out.first=word;out.last=word;++out.checked;
        if(word!=0xC01DBEAD) {
            if(!out.mismatches){out.firstMismatch=i;out.mismatchValue=word;}
            ++out.mismatches;
        }
    }
    if(out.mismatches)out.failure=5;
    return !out.failure;
}
}}
#endif
