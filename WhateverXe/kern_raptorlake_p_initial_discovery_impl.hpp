// One initial TC3 discovery through the already qualified native path.
// Included inside the reference backend's anonymous namespace. Hardware events
// and the existing bounded startup monitor request work; no new polling timer.
bool initialDiscoverySelected(){
 return checkKernelArgument("-wex-rplp-tcss-auto") && checkKernelArgument("-wex-rplp-tcss-live") &&
  checkKernelArgument("-wex-rplp-tcss-aux-probe") && checkKernelArgument("-wex-rplp-training-path") && observerSelected();
}
void requestInitialDiscovery(){
 if(!initialDiscoveryWork || !initialDiscoverySelected() || __atomic_load_n(&nativeRescanUsed,__ATOMIC_ACQUIRE))return;
 if(!__sync_bool_compare_and_swap(&initialDiscoveryBusy,0,1)){
  __atomic_store_n(&initialDiscoveryAgain,1,__ATOMIC_RELEASE);return;
 }
 __atomic_store_n(&initialDiscoveryAgain,0,__ATOMIC_RELEASE);
 __sync_fetch_and_add(&initialDiscoveryState[2],1);thread_call_enter(initialDiscoveryWork);
}
void initialDiscoveryWorker(thread_call_param_t,thread_call_param_t){
 ++initialDiscoveryState[3];uint32_t reason=1;
 if(initialDiscoverySelected() && captureAllowed() && shutdownQuery && !shutdownQuery() &&
    livePortReady && !nativeRescanUsed && !livePortLease.owner && !livePortLease.active && nativeAGDCCallbackBound()){
  reason=2;
  auto core=backend.readReg32(0x45404);
  auto panel=core!=~0U && (core&0xC03)==0xC03?backend.readReg32(0x60400):0;
  if(panel!=~0U && (panel&0x80000000U)){
   auto tc=backend.readReg32(0x161508),hpd=backend.readReg32(0x44470);
   reason=3;
   if(RaptorLakeP::ReferenceTCAux::connected(tc,hpd)){
    reason=4;
    // Never spend the one-shot preflight while the cable is absent.
    if(__sync_bool_compare_and_swap(&auxProbeAttempted,0,1)){++initialDiscoveryState[5];probeTCAux();}
    initialDiscoveryState[7]=__atomic_load_n(&livePortPermission,__ATOMIC_ACQUIRE);
    if(initialDiscoveryState[7]){
     reason=5;
     if(RaptorLakeP::Reference::rescanTC3()){++initialDiscoveryState[6];reason=0;}
    }
   }
  }
 }
 initialDiscoveryState[1]=initialDiscoveryWork?1:0;initialDiscoveryState[4]=reason;
 if(target)target->setProperty("WhateverXeRPLPInitialDiscovery",initialDiscoveryState,sizeof(initialDiscoveryState));
 __atomic_store_n(&initialDiscoveryBusy,0,__ATOMIC_RELEASE);
 // Preserve a hardware/capture notification that arrived during this worker.
 if(__atomic_exchange_n(&initialDiscoveryAgain,0,__ATOMIC_ACQ_REL))requestInitialDiscovery();
}
