#include "kern_raptorlake_p_reference_connectors.hpp"
#include <cassert>
#include <array>
#include <vector>
using namespace RaptorLakeP::ReferenceConnectors;
struct View {
 std::array<uint32_t,64> words{};std::vector<unsigned> reads;
 uint32_t read(unsigned offset){assert(offset%4==0 && offset<0x100 && offset!=8 && offset!=12);reads.push_back(offset);return words[offset/4];}
};
struct IO {
 std::vector<uint32_t> reads;unsigned powerReads{};bool changed{},absent{},invalid{};
 uint32_t read(uint32_t r){reads.push_back(r);if(r==0x45404){++powerReads;return absent?UINT32_MAX:changed&&powerReads>1?0:0x5400;}return invalid&&r==0x1638A0?UINT32_MAX:r;}
};
int main(){
 View v;assert(platform(v,false).status==1&&v.reads.empty());
 v.words[0]=0x9A490000;v.words[4]=0x03030301;
 for(unsigned i=8;i<64;++i)v.words[i]=i;
 auto p=platform(v,true);assert(p.status==0&&p.identity==0x9A490000&&p.counts==0x03030301&&p.flags==62);
 assert(p.connectors[8][5]==61&&v.reads.size()==61);
 for(unsigned counts:{0x03030302U,0x03030501U,0x030A0301U,0x04030301U}){
  v.reads.clear();v.words[4]=counts;assert(platform(v,true).status==2&&v.reads.size()==6);
 }
 v.words[4]=0x03030301;v.words[0]=0xA7A00000;v.reads.clear();assert(platform(v,true).status==2&&v.reads.size()==6);
 IO io;assert(typeC(io,false).status==1&&io.reads.empty());
 auto tc=typeC(io,true);assert(tc.status==0&&tc.count==15&&!tc.invalid&&io.reads.size()==17);
 for(unsigned i=0;i<15;++i)assert(io.reads[i+1]==registers[i]&&tc.values[i]==registers[i]);
 IO bad;bad.absent=true;assert(typeC(bad,true).status==2&&bad.reads.size()==1);
 IO changed;changed.changed=true;assert(typeC(changed,true).status==3);
 IO reserved;reserved.invalid=true;auto partial=typeC(reserved,true);assert(partial.status==0&&partial.invalid==(1U<<2)&&partial.count==15);
}
