#ifndef KERN_RAPTORLAKE_P_NATIVE_POOLS_HPP
#define KERN_RAPTORLAKE_P_NATIVE_POOLS_HPP
#include <Headers/kern_patcher.hpp>
class IOService;class IOPCIDevice;
namespace RaptorLakeP {namespace NativePools {
bool enabled();
bool passed();
bool prepare(KernelPatcher &,size_t,mach_vm_address_t,size_t);
void run(IOService *,IOPCIDevice *,bool);
}}
#endif
