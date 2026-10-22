#ifndef RPLP_NATIVE_FIXTURE_HPP
#define RPLP_NATIVE_FIXTURE_HPP
#include "kern_raptorlake_p_native_core.hpp"
#include <vector>
inline std::vector<uint8_t> nativeImage() {
    using namespace RaptorLakeP::Native;
    std::vector<uint8_t> bytes(0x30000);
    const auto word=[&](unsigned offset,uint32_t value){memcpy(bytes.data()+offset,&value,4);};
    word(0,0xFEEDFACF);word(4,0x1000007);word(16,1);word(20,24);word(32,0x1B);word(36,24);
    memcpy(bytes.data()+40,UUID,16);memcpy(bytes.data()+ProbeOffset,ProbeEntry,sizeof(ProbeEntry));
    memcpy(bytes.data()+StartOffset,StartEntry,sizeof(StartEntry));memcpy(bytes.data()+SKUOffset,SKUFind,sizeof(SKUFind));
    bytes[0x23C25]=0x7F;bytes[0x23C26]=0x15;
    const uint8_t selected[]{0x41,0xC7,0x86,0x20,0x11,0,0,2,0,0,0};memcpy(bytes.data()+0x23C5C,selected,11);
    return bytes;
}
#endif
