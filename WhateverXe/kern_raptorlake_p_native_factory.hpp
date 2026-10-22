#ifndef KERN_RAPTORLAKE_P_NATIVE_FACTORY_HPP
#define KERN_RAPTORLAKE_P_NATIVE_FACTORY_HPP
#include <Headers/kern_patcher.hpp>
class IOService;class IOPCIDevice;class OSObject;
namespace RaptorLakeP {namespace NativeFactory {
bool enabled();
bool prepare(KernelPatcher &,size_t,mach_vm_address_t,size_t);
bool arm(OSObject *,IOService *,IOPCIDevice *,bool);
bool consume(OSObject *);
bool finish(bool);
}}
#endif
