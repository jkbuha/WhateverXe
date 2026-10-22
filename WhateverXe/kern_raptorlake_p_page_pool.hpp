#ifndef KERN_RAPTORLAKE_P_PAGE_POOL_HPP
#define KERN_RAPTORLAKE_P_PAGE_POOL_HPP
#include <Headers/kern_patcher.hpp>
class IOService;class IOPCIDevice;class OSObject;
namespace RaptorLakeP {namespace PagePool {
bool enabled();
bool passed();
OSObject *create(IOService *);
bool prepare(KernelPatcher &,size_t,mach_vm_address_t,size_t);
void run(IOService *,IOPCIDevice *,bool);
}}
#endif
