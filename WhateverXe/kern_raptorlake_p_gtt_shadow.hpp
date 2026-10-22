#ifndef KERN_RAPTORLAKE_P_GTT_SHADOW_HPP
#define KERN_RAPTORLAKE_P_GTT_SHADOW_HPP
#include <Headers/kern_patcher.hpp>
class IOService;
class IOPCIDevice;
class OSObject;
namespace RaptorLakeP {namespace GTTShadow {
bool enabled();
bool prepare(KernelPatcher &,size_t,mach_vm_address_t,size_t);
bool run(OSObject *,IOService *,IOPCIDevice *,mach_vm_address_t);
void finish();
bool aperturePassed();
bool slotPassed();
}}
#endif
