#ifndef KERN_RAPTORLAKE_P_PAGE_ARRAY_HPP
#define KERN_RAPTORLAKE_P_PAGE_ARRAY_HPP
#include <Headers/kern_patcher.hpp>
class IOService;class IOPCIDevice;class OSObject;
namespace RaptorLakeP {namespace PageArray {
bool enabled();
bool integrated();
bool prepare(KernelPatcher &,size_t,mach_vm_address_t,size_t);
void run(IOService *,IOPCIDevice *,bool,mach_vm_address_t);
bool populate(IOService *,IOPCIDevice *,OSObject *,bool,mach_vm_address_t);
bool finishIntegrated(bool);
bool releaseOwned(OSObject *);
}}
#endif
