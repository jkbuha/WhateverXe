#ifndef KERN_RAPTORLAKE_P_CAPS_SHADOW_HPP
#define KERN_RAPTORLAKE_P_CAPS_SHADOW_HPP
#include <Headers/kern_patcher.hpp>
class IOService;class IOPCIDevice;class IOMemoryMap;
namespace RaptorLakeP {namespace CapsShadow {
bool enabled();bool passed();
bool prepare(KernelPatcher &,size_t,mach_vm_address_t,size_t);
bool consumeWake(IOService *,uint32_t,uint32_t);
bool run(IOPCIDevice *,IOMemoryMap *,uint32_t *,bool);
}}
#endif
