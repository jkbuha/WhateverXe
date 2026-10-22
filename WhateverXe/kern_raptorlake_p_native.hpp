#ifndef KERN_RAPTORLAKE_P_NATIVE_HPP
#define KERN_RAPTORLAKE_P_NATIVE_HPP
#include "kern_raptorlake_p_gop.hpp"
class IOPCIDevice;
namespace RaptorLakeP {namespace Native {
void init();
bool requestRemoteArm();
void arm(IOPCIDevice *,const XELP::Profile &,const Capture &,const GOPCapture &);
}}
#endif
