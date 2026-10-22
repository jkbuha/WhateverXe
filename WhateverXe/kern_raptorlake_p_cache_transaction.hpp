#ifndef KERN_RAPTORLAKE_P_CACHE_TRANSACTION_HPP
#define KERN_RAPTORLAKE_P_CACHE_TRANSACTION_HPP
#include <stdint.h>
class IOPCIDevice;class IOMemoryMap;
namespace RaptorLakeP {namespace CacheTransaction {
bool enabled();
bool run(IOPCIDevice *,IOMemoryMap *,const uint8_t *,const uint32_t *,const uint32_t *,const uint32_t (*)[3],bool);
}}
#endif
