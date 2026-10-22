"""Validate actual generated personalities from pinned private donor inputs."""
from pathlib import Path
import plistlib,re,sys,tempfile
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
