#ifndef KERN_RAPTORLAKE_P_RCS_HPP
#define KERN_RAPTORLAKE_P_RCS_HPP
#include "kern_raptorlake_p_gop.hpp"
class IOPCIDevice;
class IOMemoryMap;
namespace RaptorLakeP {
void probeRCS(IOPCIDevice *,IOMemoryMap *,const DisplayCapture &,const GOPCapture &,const XELPMemory::Layout &);
}
#endif
