#ifndef KERN_RAPTORLAKE_P_NATIVE_CHAIN_HPP
#define KERN_RAPTORLAKE_P_NATIVE_CHAIN_HPP
#include <Headers/kern_patcher.hpp>
class IOService;class IOPCIDevice;class OSObject;
namespace RaptorLakeP {namespace NativeChain {
bool enabled();
bool prepare(KernelPatcher &,size_t,mach_vm_address_t,size_t);
bool begin(IOService *,IOPCIDevice *,uint64_t *);
bool allowGTT(bool);
void observeCleanup(OSObject *);
bool finish(bool);
mach_vm_address_t pagePoolInitializer();
}}
#endif
