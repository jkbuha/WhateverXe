#include "raptorlake_p_native_fixture.hpp"
#include <cassert>
#include <cstdio>
using namespace RaptorLakeP::Native;
int main() {
 auto bytes=nativeImage();assert(exactImage(bytes.data(),bytes.size()));assert(!exactImage(nullptr,bytes.size()));
 for(auto size:{0U,31U,StartOffset+16})assert(!exactImage(bytes.data(),size));
 for(unsigned i:{0U,4U,16U,20U,32U,36U,40U,55U,ProbeOffset,ProbeOffset+16,StartOffset,StartOffset+16,SKUOffset,SKUOffset+7,0x23C25U,0x23C5CU,0x23C66U}){
  auto bad=bytes;bad[i]^=0xFF;assert(!exactImage(bad.data(),bad.size()));
 }
 auto p=XELP::identify(0xA7A08086,4);
 for(unsigned flags=0;flags<16;++flags)assert(requested(p,XELP::OS::Tahoe,flags&1,flags&2,flags&4,flags&8)==(flags==3));
 assert(!requested(p,XELP::OS::BigSur,true,true,false,false));
 for(unsigned rev=0;rev<256;++rev)assert(requested(XELP::identify(0xA7A08086,rev),XELP::OS::Tahoe,true,true,false,false)==(rev==4));
 for(auto device:XELP::DeviceTable)if(device.device!=0xA7A0)assert(!requested(XELP::identify((uint32_t(device.device)<<16)|0x8086,4),XELP::OS::Tahoe,true,true,false,false));
 assert(!requested(XELP::identify(0xA7A08086,4,0x9A498086),XELP::OS::Tahoe,true,true,false,false));
 assert(!p.admitted());
 puts("PASS native RPL-P boundary: exact image, architecture, UUID, opcode, physical identity, revision, OS and mutually exclusive probes");
}
