"""Validate actual generated personalities from pinned private donor inputs."""
from pathlib import Path
import plistlib,re,sys,tempfile,subprocess
r=Path(__file__).resolve().parents[1];sys.path.insert(0,str(r/'tools'))
from prepare_rplp_reference import prepare
source=Path(sys.argv[1]);frame=Path(sys.argv[2])
with tempfile.TemporaryDirectory(prefix='rplp-reference-metadata-') as d:
 out=Path(d);prepare(source,out,frame)
 s=(out/'wex_reference_framebuffer.hpp').read_text()
 def personality(name):
  m=re.search(r'constexpr char '+name+r'\[\]=R"WEX\((.*?)\)WEX";',s,re.S);assert m,name
  return plistlib.loads(('<plist version="1.0">'+m[1]+'</plist>').encode())
 for name in ('framebufferPersonalityXML','referenceFramebufferPersonalityXML'):
  p=personality(name);assert p['IOProbeScore']==80000,(name,p.get('IOProbeScore'))
  assert p['IOClass']=='AppleIntelFramebufferController' and p['IOMatchCategory']=='IOFramebuffer'
  assert p['CFBundleIdentifier']=='com.apple.driver.AppleIntelTGLGraphicsFramebuffer'
 p=personality('acceleratorPersonalityXML');original=plistlib.loads(Path('/Library/Extensions/AppleIntelTGLGraphics.kext/Contents/Info.plist').read_bytes())['IOKitPersonalities']['Gen7']
 assert p['Development']==original['Development'] and p['Development']['SingleChannelIndex']==0
 assert p['IOAccelDisplayPipeCapabilities']=={'DisplayPipeSupported':True,'TransactionsSupported':True}
 native=(out/'kern_gen11.cpp').read_text()
 assert 'wexRplReferenceAdoptFramebuffer(that)' in native
 assert 'cb->lastRCSCtx' not in native
 assert 'wexRplReferenceCacheRCSContext(ctx)' in native
 assert 'WEXRCSLease lease{wexRplReferenceAcquireRCSContext()}' in native
 assert '~WEXRCSLease(){wexRplReferenceReleaseRCSContext(object);}' in native
 assert '{"__ZN17IGHardwareContext4freeEv", wexRplReferenceContextFree, wexRplReferenceContextFreeOriginal}' in native
print('PASS actual generated signed/reference metadata: original 80000 priority, native match category/class/identity, complete GPU defaults and display capabilities')

# Compile the actual generated platform hook in both flag states. This checks
# that admission of physical TC slots does not change pipe/head allocation,
# eDP parameters, framebuffer memory policy, or the remaining stock entries.
with tempfile.TemporaryDirectory(prefix='rplp-reference-tcss-table-') as d:
 out=Path(d);prepare(source,out,frame)
 s=(out/'kern_gen11.cpp').read_text();h=(out/'kern_gen11.hpp').read_text()
 body=s[s.index('uint64_t Gen11::getOSInformation('):s.index('int Gen11::handleLinkIntegrityCheck()')]
 types=h[h.index('struct PACKED VoltageConfig'):h.index('// ─── DMC')]
 reference=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text()
 gate=reference[reference.index('bool wexRplReferenceTCSSRequested(){'):reference.index('bool wexRplReferenceTCSSRequested(){')+reference[reference.index('bool wexRplReferenceTCSSRequested(){'):].index('\n}')+2]
 fixture=r"""
#include <cassert>
#include <cstdint>
#include <cstring>
#define PACKED __attribute__((packed))
TYPES
constexpr uint32_t ConnectorDummy=1,ConnectorLVDS=2,ConnectorDP=1024;
namespace AppleIntel {struct AppleIntelBaseController{};}
struct Gen11 {static uint64_t getOSInformation(AppleIntel::AppleIntelBaseController *);};
struct Backend{uintptr_t gPlatformInformationList,ogetOSInformation;} backend,*callback=&backend;
bool tcss=false,panel=true;unsigned calls;uint32_t state[9]{};
bool wexRplReferencePanel(){return panel;}
bool checkKernelArgument(const char *s){assert(!strcmp(s,"-wex-rplp-tcss"));return tcss;}
GATE
uint64_t original(AppleIntel::AppleIntelBaseController *){++calls;return 0x12345678ABCDEFULL;}
#define FunctionCast(name,address) reinterpret_cast<uint64_t(*)(AppleIntel::AppleIntelBaseController *)>(address)
#define SYSLOG(...) ((void)0)
BODY
int main(){
 state[1]=state[4]=1;
 static_assert(sizeof(PlatformInfo)==448);static_assert(sizeof(ConnectorEntry)==24);
 PlatformInfo table[2]{};memset(table,0x36,sizeof(table));table[1].fInfoPlatformID=0x9A490000;
 PlatformInfo initial=table[1];backend.gPlatformInformationList=reinterpret_cast<uintptr_t>(table);backend.ogetOSInformation=reinterpret_cast<uintptr_t>(original);
 assert(Gen11::getOSInformation(nullptr)==0x12345678ABCDEFULL&&calls==1);
 PlatformInfo baseline=table[1];assert(baseline.fPipeCount==3&&baseline.fInfoPortCount==3&&baseline.fInfoFramebufferCount==3);
 assert(baseline.connectors[0].type==ConnectorLVDS&&baseline.connectors[3].type==ConnectorDummy);
 table[1]=initial;tcss=true;
 assert(Gen11::getOSInformation(nullptr)==0x12345678ABCDEFULL&&calls==2);
 assert(table[1].fPipeCount==3&&table[1].fInfoFramebufferCount==3&&table[1].fInfoPortCount==7);
 auto c=table[1].connectors[3];assert(c.index==3&&c.busId==9&&c.pipe==1&&c.pad==1&&c.type==ConnectorDP&&c.flags==0x201);
 table[1].fInfoPortCount=baseline.fInfoPortCount;table[1].connectors[3]=baseline.connectors[3];
 assert(!memcmp(&table[1],&baseline,sizeof(baseline)));
 backend.gPlatformInformationList=0;assert(Gen11::getOSInformation(nullptr)==0x12345678ABCDEFULL&&calls==3);
 for(unsigned b=0;b<32;++b){panel=b&1;state[1]=bool(b&2);state[4]=bool(b&4);state[7]=bool(b&8);tcss=b&16;
  assert(wexRplReferenceTCSSRequested()==(panel&&state[1]&&state[4]&&!state[7]&&tcss));}
}
""".replace('TYPES',types).replace('GATE',gate).replace('BODY',body)
 (out/'table-test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(out/'table-test.cpp'),'-o',str(out/'table-test')],check=True)
 subprocess.run([str(out/'table-test')],check=True)
print('PASS actual generated TCSS platform hook: original return, flag-off baseline, seven ports, retained three heads/pipes and eDP/memory data')

raw=(frame/'Contents/MacOS/AppleIntelTGLGraphicsFramebuffer').read_bytes()
for offset,pattern in [(0x254B9,'89df48c1e703'),(0x254F0,'bf00070000'),(0x8E329,'4883f808'),(0x8F466,'83c0fd83f805')]:
 assert raw[offset:offset+len(pattern)//2]==bytes.fromhex(pattern),(hex(offset),pattern)
print('PASS exact private HAL: dynamic port allocation, 0x700 class, index<=8 switch and TC index minus three; actual owner/flag gate combinations')
