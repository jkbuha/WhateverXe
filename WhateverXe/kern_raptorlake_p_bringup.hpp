#ifndef KERN_RAPTORLAKE_P_BRINGUP_HPP
#define KERN_RAPTORLAKE_P_BRINGUP_HPP
#include <Headers/kern_patcher.hpp>
class IOPCIDevice;
namespace RaptorLakeP {namespace Bringup {
bool prepare(KernelPatcher &,IOPCIDevice *,mach_vm_address_t,size_t);
}}
#endif
