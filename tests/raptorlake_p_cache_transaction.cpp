#include "kern_raptorlake_p_cache_transaction_core.hpp"
#include <cassert>
#include <map>
#include <vector>
#include <cstdio>
using namespace RaptorLakeP;
struct IO {
 std::map<uint32_t,uint32_t> regs;std::vector<std::pair<uint32_t,uint32_t>> writes;
 unsigned mode,reads=0;uint32_t changed=0;
 uint32_t read(uint32_t o){++reads;if(mode==4&&writes.size()==2&&o==CacheTransaction::offset(2)){changed=o;regs[o]^=1;mode=0;}return regs.at(o);}
 void write(uint32_t o,uint32_t v){
  assert(o!=0x9008&&o!=0x900C&&o!=0xB134&&o!=0xB138&&o!=0x40F4);
  if(o==0xB098)assert((v>>16)==(regs[o]>>16));
  writes.push_back({o,v});if((mode==2&&writes.size()==3)||(mode==3&&writes.size()==104))return;regs[o]=v;
  if(mode==5&&writes.size()==3)regs[0xB138]^=4;
 }
};
int main(){
 uint8_t pat[8]{3,0,3,3,3,3,3,3};uint32_t l3[32],mocs[64];for(auto &v:l3)v=0x00100030;for(auto &v:mocs)v=0x51;
 for(unsigned mode=0;mode<10;++mode){IO io;io.mode=mode;uint32_t baseline[CacheShadow::Count][3]{};
  for(unsigned i=0;i<CacheShadow::Count;++i){auto o=CacheShadow::offset(i);uint32_t value=o==0x4800?3:0x12340000+i;io.regs[o]=value;baseline[i][0]=o;baseline[i][1]=baseline[i][2]=value;}
  auto before=io.regs;CacheTransaction::Result out;out.qualified=mode!=6;
  if(mode==1)io.regs[0x4804]^=1;if(mode==7)baseline[8][0]^=4;if(mode==8)pat[3]=4;if(mode==9)io.regs[0x4800]=baseline[2][1]=baseline[2][2]=0;
  CacheTransaction::transact(io,baseline,pat,l3,mocs,out);pat[3]=3;
  if(mode==0){assert(!out.failure&&out.writes==103&&out.verified==103&&out.restoreWrites==103&&out.restored==103&&!out.remaining&&io.regs==before);}
  else assert(out.failure);
  if(mode==1||mode==6||mode==7||mode==8||mode==9)assert(io.writes.empty());
  if(mode==2)assert(out.failure==4&&out.writes==3&&out.restoreWrites==3&&io.regs==before);
  if(mode==3)assert(out.remaining&&out.failure==5);
  if(mode==4)assert(out.failure==3&&out.writes==2&&out.restoreWrites==2&&io.regs[io.changed]!=before[io.changed]);
  if(mode==5)assert(out.unexpected&&out.remaining&&io.regs[0xB138]!=before[0xB138]);
 }
 puts("PASS cache policy transaction:103-word write/readback/restore, firmwareMOCS61/layout/PAT0 protection, changed snapshots, partial writes, restore failures and concurrent mutation");
}
