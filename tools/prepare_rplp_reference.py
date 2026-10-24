"""Prepare the attributed optional RPL-P backend from a pinned external checkout.
Generated sources retain TSNPL notices and remain separate from BSD sources.
"""
from pathlib import Path
import hashlib,json,shutil,subprocess,plistlib,struct
PIN='d0c046a441fa467ac492770aeeefe9636b97c65e'
def prepare(source:Path,out:Path,reference_framebuffer:Path=None):
 source=source.resolve();out=out.resolve()
 assert subprocess.check_output(['git','-C',str(source),'rev-parse','HEAD'],text=True).strip()==PIN
 names=['DisplayMergeNub','DYLDPatches','Firmware','FirmwareADLP','HDMI','IntelDPLinkTraining','kern_gen11','kern_genx','kern_green','kern_patcherplus']
 src=source/'src/NootedGreen'; out.mkdir(parents=True,exist_ok=True)
 # Refuse modified reference inputs. Build dependencies/untracked logs do not matter.
 assert not subprocess.check_output(['git','-C',str(source),'diff','HEAD','--','src','LICENSE','NOTICE.md'],text=True).strip()
 for p in src.iterdir():
  if p.is_file():shutil.copy2(p,out/p.name)
 for p in [source/'LICENSE',source/'NOTICE.md',source/'src/LICENSE-NootedGreen']:shutil.copy2(p,out/p.name)
 from prepare_rplp_recovery import apply as apply_recovery
 apply_recovery(out)
 gate='#pragma once\nextern bool wexRplReferenceValidate();\nextern bool wexRplReferenceArmed();\nextern bool wexRplReferenceImage(const void *,unsigned long,bool);\nextern void wexRplReferenceLoaded(bool);\nextern void wexRplReferenceIOAF(bool);\nclass OSDictionary;\nextern bool wexRplReferenceQueue(OSDictionary *);\nextern void wexRplReferenceRuntime(unsigned *,unsigned);\nextern bool wexRplReferencePanel();\nextern bool wexRplReferenceFBAllowed(const void *);\nextern void wexRplReferenceFilteredWrite(unsigned long,unsigned);\n'
 gate+='extern bool wexRplReferenceRecoveryRequested();\nextern bool wexRplReferenceRecoveryExtendedRequested();\nextern bool wexRplReferenceBlitPitchRequested();\nextern bool wexRplReferenceForceWakeRequested();\nextern bool wexRplReferenceRuntimeOwner(const void *);\nextern bool wexRplReferenceForceWakeOwner(const void *);\n'
 gate+='class KernelPatcher;\nextern bool wexRplReferenceAcceleratorProbeHooks(KernelPatcher &,size_t,mach_vm_address_t,size_t);\nextern bool wexRplReferencePortHooks(KernelPatcher &,size_t,mach_vm_address_t,size_t);\nextern bool wexRplReferenceLivePortHooks(KernelPatcher &,size_t,mach_vm_address_t,size_t);\nextern bool wexRplReferenceNativeHotplugHooks(KernelPatcher &,size_t,mach_vm_address_t,size_t);\nextern bool wexRplReferenceAGDCObservationHooks(KernelPatcher &,size_t,mach_vm_address_t,size_t);\nextern bool wexRplReferenceNVRAMHooks(KernelPatcher &,size_t,mach_vm_address_t,size_t);\n'
 gate+='extern bool wexRplReferencePanelMode(const void *,const void *,const void *);\n'
 gate+='extern bool wexRplReferenceExternalPort(const void *);\nextern bool wexRplReferenceExternalNeedsMode(const void *,const void *,const void *);\n'
 gate+='extern bool wexRplReferenceTCSSSelected();\nextern bool wexRplReferenceTCSSRequested();\n'
 gate+='extern mach_vm_address_t wexRplReferenceContextFreeOriginal;\nextern void wexRplReferenceContextFree(void *);\nextern void wexRplReferenceCacheRCSContext(void *);\nextern void *wexRplReferenceAcquireRCSContext();\nextern void wexRplReferenceReleaseRCSContext(void *);\n'
 gate+='extern bool wexRplReferenceBarrierOwner(const void *);\nextern void wexRplReferenceBarrierResult(unsigned,unsigned);\n'
 gate+='extern bool wexRplReferenceAdoptFramebuffer(void *);\nextern void wexRplReferenceFramebufferStarted(void *,bool);\nextern mach_vm_address_t wexRplDisplayPipeInitOriginal;\nextern bool wexRplReferenceDisplayPipeInit(void *,void *,void *,void *,unsigned);\n'
 (out/'wex_reference_gate.hpp').write_text(gate)
 def edit(name,before,after):
  p=out/name;s=p.read_text();assert s.count(before)==1,(name,before,s.count(before));p.write_text(s.replace(before,after))
 edit('kern_green.cpp','#include "kern_green.hpp"','#include "kern_green.hpp"\n#include "wex_reference_gate.hpp"')
 edit('kern_gen11.cpp','#include "kern_gen11.hpp"','#include "kern_gen11.hpp"\n#include "wex_reference_gate.hpp"')
 edit('kern_green.cpp','void NGreen::processPatcher(KernelPatcher &patcher) {','void NGreen::processPatcher(KernelPatcher &patcher) {\n if(!wexRplReferenceValidate())return;')
 edit('kern_green.cpp','bool NGreen::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {','bool NGreen::processKext(KernelPatcher &patcher, size_t index, mach_vm_address_t address, size_t size) {\n if(!wexRplReferenceValidate())return false;')
 edit('kern_green.cpp','bool f2ok = p2.apply(patcher, address, size);','bool f2ok = p2.apply(patcher, address, size);\n wexRplReferenceIOAF(f2ok);')
 edit('kern_green.cpp','if (stolen_size<128) stolen_size=128;','// WhateverXe: retain measured GMS, never inflate reserved stolen memory.')
 # Signed installed bundles use com.apple identities. Keep all source notices.
 edit('kern_gen11.cpp','const char *bundleId = useTglNames ? "com.xxxxx.driver.AppleIntelTGLGraphics"','const char *bundleId = useTglNames ? "com.apple.driver.AppleIntelTGLGraphics"')
 edit('kern_gen11.cpp','bool ok = gIOCatalogue->addDrivers(array, true);','bool ok = wexRplReferenceQueue(dict);')
 for signature in ['bool Gen11::AppleIntelBaseControllerstart(AppleIntel::AppleIntelBaseController *that, IOService *param_1)','unsigned long Gen11::start(void *that,void  *param_1)']:
  edit('kern_gen11.cpp',signature+'\n{',signature+'\n{\n if(!wexRplReferenceArmed())return false;')
 fb='\t\tthis->tglFBLoaded = true;'
 edit('kern_gen11.cpp',fb,'\t\tif(!wexRplReferenceImage(reinterpret_cast<void *>(address),size,false))return false;\n'+fb)
 hw='\t\tthis->tglHWLoaded = true;'
 # It occurs in ICL path too; use full TGL branch context.
 ctx='} else if (kextG11HWT.loadIndex == index || kextG11HWTA.loadIndex == index) {\n'+hw
 edit('kern_gen11.cpp',ctx,ctx.replace(hw,'\t\tif(!wexRplReferenceImage(reinterpret_cast<void *>(address),size,true))return false;\n\t\tif(!wexRplReferenceAcceleratorProbeHooks(patcher,index,address,size))return false;\n'+hw))
 edit('kern_gen11.cpp','\t\treturn true;\n\t\t\n\t}else if (kextG11HW.loadIndex == index)', '\t\twexRplReferenceLoaded(false);\n\t\treturn true;\n\t\t\n\t}else if (kextG11HW.loadIndex == index)')
 marker='"RPL spoofed — slices=1 subslices=12(6DSS) maxEU/SS=8 totalEU=96 L3=8");'
 edit('kern_gen11.cpp',marker,marker+'\n\t\twexRplReferenceLoaded(true);')
 # Our signed framebuffer uses another private ABI. GPU-only mode registers
 # its own start blocker and does not run the imported display-hook path.
 # Force gfx-only registration independently of legacy -ngreen arguments.
 edit('kern_gen11.cpp','if (checkKernelArgument("-ngreentglfb") || checkKernelArgument("-ngreentglwithgfx")) {','if (wexRplReferencePanel()) {')
 edit('kern_gen11.cpp','} else if (checkKernelArgument("-ngreentglgfx")) {','} else if (true) {')
 # Imported display hooks are usable only with their matching debug donor ABI.
 # Use the exact private symbol source. The donor requires Apple's private KPI
 # namespace, so retain its Apple identity and qualify its UUID before routing.
 # The auxiliary collection must be built from the explicit donor path.
 if reference_framebuffer:
  edit('kern_gen11.cpp','"com.xxxxx.driver.AppleIntelTGLGraphicsFramebuffer", pathsTGLFB',
       '"com.apple.driver.AppleIntelTGLGraphicsFramebuffer", pathsTGLFB')
  edit('kern_gen11.cpp','"/Library/Extensions/AppleIntelTGLGraphicsFramebuffer.kext/Contents/MacOS/AppleIntelTGLGraphicsFramebuffer",',
       '"/Library/WhateverXeDevelopment/panel/WhateverXeRPLPFramebuffer.kext/Contents/MacOS/AppleIntelTGLGraphicsFramebuffer",')
  edit('kern_gen11.cpp',fb,'\t\tif(!wexRplReferenceNVRAMHooks(patcher,index,address,size))return false;\n\t\tif(!wexRplReferencePortHooks(patcher,index,address,size))return false;\n\t\tif(!wexRplReferenceLivePortHooks(patcher,index,address,size))return false;\n\t\tif(!wexRplReferenceNativeHotplugHooks(patcher,index,address,size))return false;\n\t\tif(!wexRplReferenceAGDCObservationHooks(patcher,index,address,size))return false;\n'+fb)
  edit('kern_gen11.cpp','\t\tlilu.onKextLoadForce(&kextG11FBTA);',
       '\t\t// Private debug donor only; never route the production framebuffer ABI.')
 p=out/'kern_gen11.cpp';p.write_text(p.read_text().replace('checkKernelArgument("-ngreentglwithgfx")','wexRplReferencePanel()'))
 # The reference pre-write repairs only the built-in head. External modes
 # must not replay that panel-specific TRANS_DDI_FUNC_CTL_A override.
 mode_before='\tif (!NGreen::callback->isRealTGL) {\n\t\tNGreen::callback->writeReg32(0x60400, 0x8A000106);  // TRANS_DDI_FUNC_CTL_A'
 mode_after='\tif (!NGreen::callback->isRealTGL && (!checkKernelArgument("-wex-rplp-training-path") || wexRplReferencePanelMode(that,param_1,param_2))) {\n\t\tNGreen::callback->writeReg32(0x60400, 0x8A000106);  // TRANS_DDI_FUNC_CTL_A'
 edit('kern_gen11.cpp',mode_before,mode_after)

 # These native Port methods return a bool and IOReturn respectively. The
 # pinned imported declarations discard both results. Preserve the private
 # ABI and bypass panel-only lane overrides for the qualified external port.
 for name in ['kern_gen11.hpp','kern_gen11.cpp']:
  for fn,result in [('computeLaneCount','uint8_t'),('setupOptimalLaneCount','uint32_t')]:
   before=('static void '+fn) if name.endswith('.hpp') else ('void Gen11::'+fn)
   after=('static '+result+' '+fn) if name.endswith('.hpp') else (result+' Gen11::'+fn)
   edit(name,before,after)
 start='uint8_t Gen11::computeLaneCount('
 end='IOReturn Gen11::wrapICLReadAUX('
 p=out/'kern_gen11.cpp';text=p.read_text();a=text.index(start);b=text.index(end,a);body=text[a:b]
 body=body.replace('if (!laneCount) return;','if (!laneCount) return 0;')
 for fn in ['computeLaneCount','setupOptimalLaneCount']:
  call='FunctionCast('+fn+', callback->o'+fn+')(that, timing, '
  assert body.count(call)==1
  body=body.replace(call,'auto nativeResult = '+call)
 body=body.replace('if (NGreen::callback->isRealTGL)\n\t\treturn;',
                   'if (NGreen::callback->isRealTGL || wexRplReferenceExternalPort(that))\n\t\treturn nativeResult;')
 for old,new in [('\t\t*laneCount = hwLanes;\n}', '\t\t*laneCount = hwLanes;\n\treturn nativeResult;\n}'),
                 ('\t\tcached = hwLanes;\n}', '\t\tcached = hwLanes;\n\treturn nativeResult;\n}')]:
  assert body.count(old)==1;body=body.replace(old,new)
 p.write_text(text[:a]+body+text[b:])
 # Suppressing pipe-A differences is valid only for the retained panel.
 edit('kern_gen11.cpp','if (!NGreen::callback->isRealTGL && param_3) {',
      'if (!NGreen::callback->isRealTGL && param_3 && (!checkKernelArgument("-wex-rplp-training-path") || wexRplReferencePanelMode(that,param_1,param_2))) {')
 # A newly adopted native path may be marked active before its hardware pipe
 # is enabled. Force the ordinary native full modeset for that exact lease.
 old='\treturn FunctionCast(hwRegsNeedUpdate, callback->ohwRegsNeedUpdate)(that, param_1, param_2, param_3, param_4, param_5);'
 new='\tauto nativeResult = FunctionCast(hwRegsNeedUpdate, callback->ohwRegsNeedUpdate)(that, param_1, param_2, param_3, param_4, param_5);\n\treturn wexRplReferenceExternalNeedsMode(that,param_1,param_2) ? 1 : nativeResult;'
 edit('kern_gen11.cpp',old,new)

 # Preserve the qualified panel/pipe/head counts. Only expose the physical
 # ADL-P port domain under the explicit exact-owner TCSS trial gate. The
 # imported three-port experiment otherwise hides every connected TC port.
 edit('kern_gen11.cpp','\t\tpinfo[1].connectors[3] = { 3, 3, 3, 0, ConnectorDummy, 0 };',
      '\t\tpinfo[1].connectors[3] = { 3, 3, 3, 0, ConnectorDummy, 0 };\n'
      '\t\tif(wexRplReferenceTCSSRequested()) {\n'
      '\t\t\tpinfo[1].fInfoPortCount=7;\n'
      '\t\t\tpinfo[1].connectors[3]={3,9,1,1,ConnectorDP,0x201};\n'
      '\t\t}\n')
 # The native HAL already merges south HPD and north TC/TBT HPD correctly.
 # The imported f19 experiment redirects its north read to TGL modular FIA,
 # hiding ADL-P TC hotplug. Preserve the exact original bytes for this trial,
 # while still checking the unique donor pattern. Keep f13's independent
 # early power-well probe bypass unchanged.
 p=out/'kern_gen11.cpp';text=p.read_text()
 needle='{activeKext, f19, r19, arrsize(f19),\t1},'
 lines=text.splitlines(keepends=True);matches=0
 for i,line in enumerate(lines):
  if line.lstrip().startswith(needle):
   lines[i]=line.replace(needle,'{activeKext, f19, wexRplReferenceTCSSSelected()?f19:r19, arrsize(f19),\t1},');matches+=1
 assert matches==2
 p.write_text(''.join(lines))
 # Keep PCI hardware identity throughout module preparation; on root arm the
 # presentation becomes 9A49. The known exact donor is guarded before this edit.
 edit('kern_gen11.cpp','\t\tNGreen::callback->setRMMIOIfNecessary();\n\t\tSYSLOG("ngreen", "V165: setRMMIO done, starting symbol resolve");',
      '\t\tNGreen::callback->setRMMIOIfNecessary();\n\t\tconst uint8_t skuOld[]{0x81,0xFF,0x86,0x80,0x48,0x9A,0x74,0x25};\n\t\tconst uint8_t skuNew[]{0x81,0xFF,0x86,0x80,0xA0,0xA7,0x74,0x25};\n\t\tif(size<=0x23C37 || memcmp(reinterpret_cast<void *>(address+0x23C2F),skuOld,8))return false;\n\t\tpatcher.routeBlock(address+0x23C2F,skuNew,8);\n\t\tif(patcher.getError()!=KernelPatcher::Error::NoError){patcher.clearError();return false;}\n\t\tSYSLOG("ngreen", "V165: setRMMIO done, starting symbol resolve");')
 # Preserve legacy startup behavior. Admit the original dependency barrier only
 # after native publication and after checking every context it dereferences.
 edit('kern_gen11.cpp','void *event, uint16_t count, const uint16_t *list) {',
      'void *event, uint16_t count, const uint16_t *list) {\n'+r"""
 // WEX native barrier candidate begin.
 if(wexRplReferenceBarrierOwner(accelerator)) {
  unsigned reason=0;
  void *task=nullptr;
  if(!queue || !cmdDesc || !event || (count && !list))reason=1;
  if(!reason && getMember<void *>(queue,0x5C0)!=accelerator)reason=2;
  if(!reason){
   auto last=getMember<int32_t>(queue,0x830);
   if(last < -1 || last > 15 || !getMember<void *>(cmdDesc,0x58))reason=3;
   else if(!(task=getMember<void *>(cmdDesc,8)))reason=4;
   else if(last>=0)for(int i=0;i<=last;++i)if(!getMember<void *>(queue,0x7B0+8*i)){reason=5;break;}
  }
  auto valid=[](void *ctx){return ctx && getMember<void *>(ctx,0xB8);};
  void *(*getters[])(void *,bool){getBlit2DContext,getBlit3DContext,getDepthResolveContext,getColorResolveContext};
  for(unsigned i=0;!reason && i<4;++i)if(!valid(getters[i](task,true)))reason=6+i;
  if(!reason){
   auto systemTask=getMember<void *>(accelerator,0x150);
   if(!systemTask)reason=10;
   for(unsigned i=0;!reason && i<2;++i)if(!valid(getters[i](systemTask,true)))reason=11+i;
  }
  if(!reason && !callback->obarrierSubmission)reason=13;
  if(!reason){
   auto result=FunctionCast(barrierSubmission,callback->obarrierSubmission)(queue,accelerator,cmdDesc,event,count,list);
   wexRplReferenceBarrierResult(0,result);return result;
  }
  wexRplReferenceBarrierResult(reason,0);
 }
 // WEX native barrier candidate end.
""")
 # The reference's raw cached context can outlive its owner (shutdown panic).
 # Invalidate before native base free; repair holds a temporary live reference.
 edit('kern_gen11.cpp','cb->lastRCSCtx = ctx;', 'wexRplReferenceCacheRCSContext(ctx);')
 edit('kern_gen11.cpp','void *ctx = cb->lastRCSCtx;',
      'struct WEXRCSLease { void *object; ~WEXRCSLease(){wexRplReferenceReleaseRCSContext(object);} };\n\tWEXRCSLease lease{wexRplReferenceAcquireRCSContext()};\n\tvoid *ctx = lease.object;')
 edit('kern_gen11.cpp','{"__ZN17IGHardwareContext11withOptionsEP11IGAccelTaskRK23IGHardwareContextParamsh",',
      '{"__ZN17IGHardwareContext4freeEv", wexRplReferenceContextFree, wexRplReferenceContextFreeOriginal},\n\t\t\t {"__ZN17IGHardwareContext11withOptionsEP11IGAccelTaskRK23IGHardwareContextParamsh",')
 # Context switching is routine; never flood release logs or imply a reset.
 edit('kern_gen11.cpp','SYSLOG("ngreen", "V194: getBlit3DContext ctx changed %p -> %p (restart recovery), updating cache",',
      'DBGLOG("ngreen", "getBlit3DContext: updating cached context %p -> %p",')
 # V509 must never invent a ring start on our machine. Invalid mapping refuses repair.
 edit('kern_gen11.cpp','if (!rcsStart) rcsStart = 0x402eb000;         // fallback from HANGCHECK','if (!rcsStart) { cb->writeReg32(GGTT_PTE_LO(0), saveLo); cb->writeReg32(GGTT_PTE_HI(0), saveHi); cb->writeReg32(0x101008, 1); return ret; }')
 # Bounded passive snapshots: no console logging or MMIO writes in interrupt paths.
 p=out/'kern_gen11.cpp'
 p.write_text(p.read_text()+r"""
void wexRplReferenceFilteredWrite(unsigned long reg,unsigned value){
 auto cb=NGreen::callback;if(!wexRplReferencePanel() || !wexRplReferenceArmed() || !cb)return;
 value=xeFixFilterWrite(reg,value);cb->writeReg32(reg,value);
}
void wexRplReferenceRuntime(unsigned *out,unsigned count){
 if(!out || count<256)return;
 bzero(out,256*sizeof(unsigned));out[0]=1;
 out[2]=__atomic_load_n(&gEvN,__ATOMIC_RELAXED);out[3]=gS5PushN;out[4]=gS5NotifyN;out[5]=gCs5SubmitN;
 out[6]=__atomic_load_n(&gCsbN,__ATOMIC_RELAXED);out[7]=gCsbSkipN;out[8]=gCsbResyncN;
 out[9]=gRbCount[0];out[10]=gRbLastStamp[0];out[11]=gS5PauseN;out[12]=gS5ResumeN;
 unsigned first=out[2]>32?out[2]-32:0,n=out[2]-first;out[13]=n;
 for(unsigned k=0;k<n;++k){auto &e=gEvLog[(first+k)%XE_EV_MAX];unsigned o=16+5*k;out[o]=first+k+1;out[o+1]=e.tag;out[o+2]=e.a;out[o+3]=e.b;out[o+4]=e.c;}
 auto cb=NGreen::callback;
 const unsigned regs[]={0x2094,0x2034,0x2030,0x2038,0x203c,0x2080,0x209c,0x229c,0x2234,0x23a0,0x2510,0x2514,0x2518,0x251c,0x2550,0x20a8,0x20ac,0x2064,0x22034,0x22030,0x2203c,0x22080,0x22234,0x22550,0x130044,0xD84,0xA278,0x20b0,0x402c,0x4094,0x4b0,0x190010};
 out[14]=cb!=nullptr;
 if(out[14])for(unsigned k=0;k<32;++k){out[176+2*k]=regs[k];out[177+2*k]=cb->readReg32(regs[k]);}
}
""")
 # Native matching owns provider attachment and firmware-framebuffer arbitration.
 edit('kern_gen11.cpp','bool Gen11::AppleIntelBaseControllerstart(AppleIntel::AppleIntelBaseController *that, IOService *param_1)\n{',
      'bool Gen11::AppleIntelBaseControllerstart(AppleIntel::AppleIntelBaseController *that, IOService *param_1)\n{\n if(!wexRplReferenceAdoptFramebuffer(that))return false;')
 edit('kern_gen11.cpp','\treturn ret;\n}\n\nuint32_t Gen11::wrapProbeCDClockFrequency',
      '\twexRplReferenceFramebufferStarted(that,ret);\n\treturn ret;\n}\n\nuint32_t Gen11::wrapProbeCDClockFrequency')
 edit('kern_gen11.cpp','RouteRequestPlus flipDiag[] = {',
      'RouteRequestPlus flipDiag[] = {\n {"__ZN18IGAccelDisplayPipe4initEP22IOGraphicsAccelerator2P27IOAccelLegacyDisplayMachineP13IOFramebufferj",wexRplReferenceDisplayPipeInit,wexRplDisplayPipeInitOriginal},')
 # Generated metadata is private build output, not redistributed Apple source.
 fb=Path('/Library/Extensions/AppleIntelTGLGraphicsFramebuffer.kext/Contents')
 info=plistlib.loads((fb/'Info.plist').read_bytes())
 personality=info['IOKitPersonalities']['AppleIntelFramebufferController']
 assert personality['CFBundleIdentifier']=='com.apple.driver.AppleIntelTGLGraphicsFramebuffer'
 assert personality['IOProbeScore']==80000
 personality['IOPCIPrimaryMatch']='0x9a498086'
 xml=plistlib.dumps(personality).decode();xml=xml[xml.index('<dict>'):xml.rindex('</dict>')+7]
 data=(fb/'MacOS/AppleIntelTGLGraphicsFramebuffer').read_bytes();pos=32;uuid=None
 assert hashlib.sha256(data).hexdigest()=='8470f7edc677352a19193c005b6dcdece2e3283262d5a8ec891bf9360d755c6e'
 for i in range(struct.unpack_from('<I',data,16)[0]):
  command,length=struct.unpack_from('<II',data,pos)
  if command==0x1b:uuid=data[pos+8:pos+24]
  pos+=length
 assert uuid
 header='// Generated from verified exact donors; private build output.\n'
 header+='constexpr bool referenceFramebufferEnabled='+('true' if reference_framebuffer else 'false')+';\n'
 header+='constexpr unsigned char framebufferDonorUUID[]={'+','.join(hex(x) for x in uuid)+'};\n'
 header+='constexpr char framebufferPersonalityXML[]=R"WEX('+xml+')WEX";\n'
 if reference_framebuffer:
  ref=reference_framebuffer.resolve();contents=ref/'Contents';raw=(contents/'MacOS/AppleIntelTGLGraphicsFramebuffer').read_bytes()
  assert hashlib.sha256(raw).hexdigest()=='285ee7a9c3d6c9a9647013f44fb40f312b1cb42879974ef0542544cf049442b5'
  assert raw[0xdd1fe:0xdd203]==bytes.fromhex('b9481c0000') # exact controller class size
  rp=plistlib.loads((contents/'Info.plist').read_bytes())['IOKitPersonalities']['AppleIntelFramebufferController']
  rp['CFBundleIdentifier']='com.apple.driver.AppleIntelTGLGraphicsFramebuffer';rp['IOPCIPrimaryMatch']='0x9a498086'
  assert rp['IOProbeScore']==80000 and rp['IOClass']=='AppleIntelFramebufferController'
  rx=plistlib.dumps(rp).decode();rx=rx[rx.index('<dict>'):rx.rindex('</dict>')+7]
  header+='constexpr unsigned char referenceFramebufferUUID[]={0xB9,0x6A,0x84,0x48,0x9D,0x17,0x3F,0x78,0xB7,0x6D,0x15,0xA0,0xB3,0x59,0xA9,0xA0};\n'
  header+='constexpr char referenceFramebufferPersonalityXML[]=R"WEX('+rx+')WEX";\n'
 else:
  header+='constexpr unsigned char referenceFramebufferUUID[16]{};\nconstexpr char referenceFramebufferPersonalityXML[]="";\n'

 gpu=Path('/Library/Extensions/AppleIntelTGLGraphics.kext/Contents')
 assert hashlib.sha256((gpu/'MacOS/AppleIntelTGLGraphics').read_bytes()).hexdigest()=='6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64'
 assert hashlib.sha256((gpu/'Info.plist').read_bytes()).hexdigest()=='b50f517410a73ee1df011cc600b8a8ff90087b775721581855957cf66e32097c'
 gp=plistlib.loads((gpu/'Info.plist').read_bytes())['IOKitPersonalities']['Gen7']
 assert gp['Development']['SingleChannelIndex']==0
 gp['IOAccelDisplayPipeCapabilities']={'DisplayPipeSupported':bool(reference_framebuffer),'TransactionsSupported':bool(reference_framebuffer)}
 xml=plistlib.dumps(gp).decode();xml=xml[xml.index('<dict>'):xml.rindex('</dict>')+7]
 header+='constexpr char acceleratorPersonalityXML[]=R"WEX('+xml+')WEX";\n'
 (out/'wex_reference_framebuffer.hpp').write_text(header)
 result={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in sorted(out.iterdir()) if p.is_file() and p.name!='SOURCE-MANIFEST.json'}
 (out/'SOURCE-MANIFEST.json').write_text(json.dumps(dict(upstream=PIN,recovery_commit='813d2e6adca277dd349ebe9674233953cce66342',recovery_patch_sha256=hashlib.sha256((Path(__file__).resolve().parents[1]/'Reference/JulioNectar/813d2e6.patch').read_bytes()).hexdigest(),hardening_sha256=hashlib.sha256((Path(__file__).parent/'prepare_rplp_recovery.py').read_bytes()).hexdigest(),files=result),indent=2)+'\n')
 return [out/(n+'.cpp') for n in names]
