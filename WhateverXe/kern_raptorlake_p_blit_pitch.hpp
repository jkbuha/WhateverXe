#pragma once
#include <stdint.h>
namespace RaptorLakeP { namespace BlitPitch {
// Called only after the exact donor accepted the operation as a 2D copy.
// Preserve its small-copy threshold and its maximum linear row stride.
inline bool divert(bool owner,uint8_t bytesPerPixel,uint64_t pixels,uint32_t sourcePitch,uint32_t destinationPitch){
 if(!owner || !(bytesPerPixel==1 || bytesPerPixel==2 || bytesPerPixel==4 || bytesPerPixel==8 || bytesPerPixel==16))return false;
 if(!sourcePitch || !destinationPitch || sourcePitch>0x7FFE || destinationPitch>0x7FFE ||
    ((sourcePitch|destinationPitch)&3) || !((sourcePitch|destinationPitch)&63))return false;
 if(pixels>UINT64_MAX/bytesPerPixel)return false;
 return pixels*bytesPerPixel >= (bytesPerPixel==16 ? 0x51001ULL : 0x20001ULL);
}
}}
