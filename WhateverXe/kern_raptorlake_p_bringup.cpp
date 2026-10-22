#include "kern_raptorlake_p_bringup.hpp"
#include "kern_raptorlake_p_bringup_core.hpp"
#include "kern_xelp_device.hpp"
#include <Headers/kern_util.hpp>
extern "C" const char version[];
namespace RaptorLakeP {namespace Bringup {
bool prepare(KernelPatcher &patcher,IOPCIDevice *pci,mach_vm_address_t address,size_t size) {
 if(!checkKernelArgument("-wex-rplp-donor-fixes"))return true;
 uint32_t receipt[]{1,0,0,0}; // version, physical/arming guard, qualified image, applied mask
 const bool guard=pci && eligible(XELP::deviceProfile(pci),XELP::kernelOS(version),
  checkKernelArgument("-wex-rplp-probe")) && checkKernelArgument("-wex-rplp-native-probe") &&
  checkKernelArgument("-wex-rplp-manual-arm") && checkKernelArgument("-wex-rplp-native-engines");
 receipt[1]=guard;
 auto image=reinterpret_cast<void *>(address);
 receipt[2]=guard && qualified(image,size);
 bool ok=false;
 if(receipt[2]) ok=apply(image,size,[&](uint32_t offset,const uint8_t *bytes,unsigned length){
  patcher.routeBlock(address+offset,bytes,length);
  const bool written=patcher.getError()==KernelPatcher::Error::NoError;
  patcher.clearError();return written;
 },receipt[3]);
 if(pci)pci->setProperty("WhateverXeRPLPDonorFixes",receipt,sizeof(receipt));
 return ok;
}
}}
