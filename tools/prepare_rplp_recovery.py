"""Apply attributed reference recovery changes with local admission safeguards."""
from pathlib import Path
import subprocess

def apply(out: Path):
 root=Path(__file__).resolve().parents[1]
 for name in ['813d2e6.patch','df7ab78.patch']:
  patch=root/'Reference/JulioNectar'/name
  subprocess.run(['/usr/bin/patch','-s','-f','-F','0','-p1','-d',str(out),'-i',str(patch)],check=True)
 p=out/'kern_gen11.cpp';s=p.read_text()
 def edit(a,b):
  nonlocal s
  assert s.count(a)==1,(a,s.count(a));s=s.replace(a,b)
 edit('if (!checkKernelArgument("-ngreennorc6fw")) {','if (wexRplReferenceForceWakeRequested()) {')
 edit('if (!NGreen::callback->isRealTGL) xeGpuRecInstall(patcher, index, address, size);','if (wexRplReferenceRecoveryRequested()) xeGpuRecInstall(patcher, index, address, size);')
 edit('static int gRc6FwDrop = 1;', 'static int gRc6FwDrop = 0;')
 edit('gRc6FwDrop = checkKernelArgument("-ngreenrc6fwnodrop") ? 0 : 1;', 'gRc6FwDrop = 0; // automatic idle-hold release remains unqualified')
 edit('if (sfw) {\n', 'if (sfw >= address && size >= 0x140 && sfw-address <= size-0x140) {\n')
 edit('if (gRc6FwOn != 1 || !dom || !a) {','if (gRc6FwOn != 1 || !dom || !a || !wexRplReferenceForceWakeOwner(that)) {')
 edit('if (set) {\n\t\t\tcnt[i] = c + 1;', 'if (set) {\n\t\t\tif (c == INT32_MAX) { cnt[i] = c; continue; }\n\t\t\tcnt[i] = c + 1;')
 edit('if (acc && NGreen::callback) {','if (acc && NGreen::callback && wexRplReferenceRuntimeOwner(acc)) {')
 edit('return m == 0 || m == 0xffffffff || (m & 0x200);','return m != 0xffffffff && (m == 0 || (m & 0x200));')
 edit('return d && n > 0 && n <= 64 && *reinterpret_cast<uint32_t *>(d)',
      'uint64_t capacity = *reinterpret_cast<uint64_t *>(v + 8);\n\treturn d && n > 0 && n <= 64 && capacity >= n && capacity <= 4096 && reinterpret_cast<uintptr_t>(d) <= UINTPTR_MAX-n*0x24 && *reinterpret_cast<uint32_t *>(d)')
 marker='static void xeRecWriteList('
 assert s.count(marker)==1
 s=s.replace(marker,'''static bool xeRecValidRegisters(uint8_t *data,uint64_t count,uint64_t bytes){
 if(bytes<4 || !data || !count || count>64)return false;
 for(uint64_t i=0;i<count;++i){auto reg=xeRecReg(data,i);if((reg&3) || reg>bytes-4)return false;}
 return true;
}

'''+marker)
 edit('if (!acc || !mmio || !(mask & 2) || own < 0 || type != (uint8_t)own) return orig(ring, ctx);',
      'if (!acc || !mmio || mask != 2 || own < 0 || type != (uint8_t)own || !wexRplReferenceRuntimeOwner(acc)) return orig(ring, ctx);')
 edit('if (!xeRecList(sib, sn, sd) || !xeRecList(own, on, od)) return orig(ring, ctx);',
      'if (!xeRecList(sib, sn, sd) || !xeRecList(own, on, od)) return orig(ring, ctx);\n\tif (!xeRecValidRegisters(sd,sn,NGreen::callback->mmioLength()) || !xeRecValidRegisters(od,on,NGreen::callback->mmioLength())) return orig(ring,ctx);')
 edit('for (uint64_t i = 0; i < on; i++) osnap[i] = xeRecRd(mmio, xeRecReg(od, i));',
      'for (uint64_t i = 0; i < on; i++) osnap[i] = xeRecRd(mmio, xeRecReg(od, i));\n\tif ((snap[0] && (snap[0]==UINT32_MAX || (snap[0]&0xFFF))) || (osnap[0] && (osnap[0]==UINT32_MAX || (osnap[0]&0xFFF)))) { sfw(acc,false,7);return orig(ring,ctx); }')
 edit('if (ret && gdrst && lost) {','if (ret && gdrst && lost && (snap[0] || gRecGoodOk[sib])) {')
 edit('auto has = [](mach_vm_address_t f, size_t len, const uint8_t *p, size_t n) {',
      'auto span = [address,size](mach_vm_address_t f,size_t n){return f>=address && n<=size && f-address<=size-n;};\n\tauto has = [&](mach_vm_address_t f, size_t len, const uint8_t *p, size_t n) {')
 edit('if (!f) return false;\n\t\tauto *b = reinterpret_cast<const uint8_t *>(f);',
      'if (!span(f,len)) return false;\n\t\tauto *b = reinterpret_cast<const uint8_t *>(f);')
 edit('if (gRecS5Proc) sig4 =', 'if (span(gRecS5Proc,4+sizeof(sS5))) sig4 =')
 edit('(cs5Proc && !memcmp(reinterpret_cast<const uint8_t *>(cs5Proc) + 0xe, sCs5, sizeof(sCs5)))',
      '(span(cs5Proc,0xe + sizeof(sCs5)) && !memcmp(reinterpret_cast<const uint8_t *>(cs5Proc) + 0xe, sCs5, sizeof(sCs5)))')
 edit('bool l0 = xeRecList(0, n0, d0), l1 = xeRecList(1, n1, d1);',
      'bool l0 = span(gRecList[0],24) && xeRecList(0,n0,d0), l1 = span(gRecList[1],24) && xeRecList(1,n1,d1);')
 edit('bool ok = rge && gRecSfw && gRecGetDef && gRecS5Proc && sig1 && sig2 && sig3 && sig4 && sig5;',
      'bool ok = rge && span(gRecSfw,1) && span(gRecGetDef,1) && gRecS5Proc && sig1 && sig2 && sig3 && sig4 && sig5;')
 edit('SYSLOG("ngreen", "XE-RC6FW: sym=%d sig=%d/%d route=%d idle=%u drop=%d",', 'IORegistryEntry::getRegistryRoot()->setProperty("WhateverXeRPLPForceWakeBalanceReady",gRc6FwOn==1);\n\t\t\t\tSYSLOG("ngreen", "XE-RC6FW: sym=%d sig=%d/%d route=%d idle=%u drop=%d",')
 edit('SYSLOG("ngreen", "XE-GPUREC: sym rge=%d sfw=%d def=%d s5=%d cs5=%d lists=%d/%d(n=%llu/%llu) sig=%d%d%d%d%d%d route=%d ex=%d",', 'IORegistryEntry::getRegistryRoot()->setProperty("WhateverXeRPLPResetRecoveryReady",gRecOn==1);\n\tIORegistryEntry::getRegistryRoot()->setProperty("WhateverXeRPLPResetRecoveryExtendedReady",gRecExOn==1);\n\tSYSLOG("ngreen", "XE-GPUREC: sym rge=%d sfw=%d def=%d s5=%d cs5=%d lists=%d/%d(n=%llu/%llu) sig=%d%d%d%d%d%d route=%d ex=%d",')
 # v2 and row-stride routing are separate, explicit exact-RPL trials. Keep
 # existing recovery/forcewake owner and BAR checks around the imported code.
 edit('#include "kern_gen11.hpp"', '#include "kern_gen11.hpp"\n#include "kern_raptorlake_p_blit_pitch.hpp"\n#include <kern/thread.h>')
 edit('if (okba && !checkKernelArgument("-ngreennoblitfix")) {', 'if (okba && wexRplReferenceBlitPitchRequested()) {')
 edit('if (gRecOn == 1 && sig6 && !checkKernelArgument("-ngreennogpurecex")) {', 'if (gRecOn == 1 && sig6 && wexRplReferenceRecoveryExtendedRequested()) {')
 edit('static volatile uint32_t gRecExFix = 0, gRecExLastRcu = 0xffffffffu;', 'static volatile uint32_t gRecExFix = 0, gRecExLastRcu = 0xffffffffu;\nstatic volatile uint32_t gRecExBusy=0;\nstatic thread_t gRecPendThread=nullptr;')
 edit('uint32_t v[5];\n\tfor (int i = 0; i < 5; i++) v[i] = xeRecRd(mmio, kXeRecEx[i]);', 'uint32_t v[5];\n\tgRecExSnapOk=false;\n\tif(!wexRplReferenceRuntimeOwner(acc) || !mmio || NGreen::callback->mmioLength()<0x1A4D4)return;\n\tfor (int i = 0; i < 5; i++){v[i] = xeRecRd(mmio, kXeRecEx[i]);if(v[i]==UINT32_MAX)return;}')
 edit('if (gRecExOn != 1 || !acc || !mmio) return 0;', 'if (gRecExOn != 1 || !acc || !mmio || !wexRplReferenceRuntimeOwner(acc) || NGreen::callback->mmioLength()<0x1A4D4) return 0;')
 edit('gRecExLastRcu = rcu;', 'gRecExLastRcu = rcu;\n\tif(rcu==UINT32_MAX)return 0;')
 edit('uint32_t cur = xeRecRd(mmio, kXeRecEx[i]);', 'uint32_t cur = xeRecRd(mmio, kXeRecEx[i]);\n\t\tif(cur==UINT32_MAX)continue;')
 edit('if (gRecPendAcc && gRecPendMmio) xeRecExFix(gRecPendAcc, gRecPendMmio);', 'auto *acc=gRecPendAcc;\n\tif(acc && gRecPendMmio && gRecPendThread==current_thread() && type<=1 && wexRplReferenceRuntimeOwner(acc) && *reinterpret_cast<void **>(static_cast<uint8_t *>(acc)+0x1250)==sched)xeRecExFix(acc,gRecPendMmio);')
 edit('xeRecExSnapshot(acc, mmio);\n\tgRecPendAcc = acc; gRecPendMmio = mmio;', 'const bool extended=gRecExOn==1 && OSCompareAndSwap(0,1,&gRecExBusy);\n\tif(extended){xeRecExSnapshot(acc,mmio);gRecPendThread=current_thread();gRecPendAcc=acc;gRecPendMmio=mmio;}')
 edit('gRecPendAcc = nullptr; gRecPendMmio = nullptr;\n\tuint32_t exDid = xeRecExFix(acc, mmio);', 'uint32_t exDid=0;\n\tif(extended){gRecPendAcc=nullptr;gRecPendMmio=nullptr;gRecPendThread=nullptr;exDid=xeRecExFix(acc,mmio);__sync_lock_release(&gRecExBusy);}')
 edit('auto *svc = OSDynamicCast(IOService, reinterpret_cast<OSObject *>(gBfAcc));', 'if(!wexRplReferenceRuntimeOwner(gBfAcc))return;\n\tauto *svc = OSDynamicCast(IOService, reinterpret_cast<OSObject *>(gBfAcc));')
 edit('gBfAcc = acc;   //', 'if(!wexRplReferenceRuntimeOwner(acc))return;\n\tgBfAcc = acc;   //')
 edit('const uint32_t bpp = p[0xa0];\n\tconst uint64_t bytes = px * bpp;', 'if(!p)return false;\n\tconst auto rd32=[p](size_t offset){uint32_t value;memcpy(&value,p+offset,4);return value;};\n\tconst uint32_t bpp = p[0xa0];\n\tif(!bpp || px>UINT64_MAX/bpp)return false;\n\tconst uint64_t bytes = px * bpp;')
 edit('*reinterpret_cast<const uint32_t *>(p) + *reinterpret_cast<const uint32_t *>(p + 0x2c)', 'rd32(0)+rd32(0x2c)')
 edit('*reinterpret_cast<const uint32_t *>(p + 0x48) + *reinterpret_cast<const uint32_t *>(p + 0x74)', 'rd32(0x48)+rd32(0x74)')
 edit('*reinterpret_cast<const uint32_t *>(p + 0x8), dp = *reinterpret_cast<const uint32_t *>(p + 0x50)', 'rd32(8), dp=rd32(0x50)')
 edit('if ((sp | dp) & 0x3f) {', 'if (RaptorLakeP::BlitPitch::divert(wexRplReferenceBlitPitchRequested() && wexRplReferenceRuntimeOwner(acc),bpp,px,sp,dp)) {')
 edit('if (!gBfTc) gBfTc = thread_call_allocate(xeBlitFixPublish, nullptr);', 'if(!wexRplReferenceBlitPitchRequested() || v2Size!=60 || size<0x2C264 || memcmp(reinterpret_cast<const void *>(address+0x2C228),v2,60))return false;\n\tif (!gBfTc) gBfTc = thread_call_allocate(xeBlitFixPublish, nullptr);')
 p.write_text(s)
