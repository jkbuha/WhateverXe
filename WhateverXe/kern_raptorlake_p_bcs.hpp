#ifndef KERN_RAPTORLAKE_P_BCS_HPP
#define KERN_RAPTORLAKE_P_BCS_HPP
#include "kern_raptorlake_p_gop.hpp"
class IOPCIDevice;
class IOMemoryMap;
namespace RaptorLakeP {
void probeBCS(IOPCIDevice *,IOMemoryMap *,const DisplayCapture &,const GOPCapture &,const XELPMemory::Layout &);
}
#endif
