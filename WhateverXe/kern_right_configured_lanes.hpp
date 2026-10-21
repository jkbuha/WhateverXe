// Configured lanes for the measured modular FIA2 TC3 DP-alt transport.
#ifndef KERN_RIGHT_CONFIGURED_LANES_HPP
#define KERN_RIGHT_CONFIGURED_LANES_HPP
#include <stdint.h>
struct RightConfiguredLanes {
 static constexpr uint32_t Register=0x16e8c0;
 static bool target(uint32_t physical,uint32_t revision,uint32_t ddi,uint32_t mode,
  bool rightReady,bool routeReady,bool lockReady) {
  return physical==0x9a498086U && revision==1 && ddi==5 && mode==3 && rightReady && routeReady && lockReady;
 }
 // The native getter returns an unpacked nibble. Its old FIA1 extraction
 // shifted TC3 by8; FIA2 TC3 is index0, so no shift belongs in this return.
 // Diagnostic signal selector only: initialize both DKL banks as Linux does.
 // This is never written to FIA and is restricted to the exact native call site.
 static uint32_t signalMask(uint32_t raw,uintptr_t caller,uintptr_t expected) {
  const auto lanes=result(raw);
  return expected && caller==expected && (lanes==1 || lanes==3) ? 15U : lanes;
 }
 static uint32_t result(uint32_t raw){return raw==UINT32_MAX?0:raw&15U;}
};
#endif
