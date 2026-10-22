#ifndef KERN_RAPTORLAKE_P_NATIVE_ENGINES_HPP
#define KERN_RAPTORLAKE_P_NATIVE_ENGINES_HPP
#include <Headers/kern_patcher.hpp>
class IOPCIDevice;
namespace RaptorLakeP {namespace NativeEngines {
bool enabled();bool prepare(KernelPatcher &,size_t,mach_vm_address_t,size_t);void run(IOPCIDevice *,bool);
}}
#endif
