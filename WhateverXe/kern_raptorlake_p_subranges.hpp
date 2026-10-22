#ifndef KERN_RAPTORLAKE_P_SUBRANGES_HPP
#define KERN_RAPTORLAKE_P_SUBRANGES_HPP
#include <Headers/kern_patcher.hpp>
class IOService;class IOPCIDevice;class OSObject;
namespace RaptorLakeP {namespace Subranges {
bool enabled();bool held();
bool prepare(KernelPatcher &,size_t,mach_vm_address_t,size_t);
bool run(OSObject *,IOService *,IOPCIDevice *,bool);
}}
#endif
