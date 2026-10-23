"""Apply attributed reference recovery changes with local admission safeguards."""
from pathlib import Path
import subprocess

def apply(out: Path):
 root=Path(__file__).resolve().parents[1]
 patch=root/'Reference/JulioNectar/813d2e6.patch'
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
 edit('SYSLOG("ngreen", "XE-GPUREC: sym rge=%d sfw=%d def=%d s5=%d cs5=%d lists=%d/%d(n=%llu/%llu) sig=%d%d%d%d%d route=%d",', 'IORegistryEntry::getRegistryRoot()->setProperty("WhateverXeRPLPResetRecoveryReady",gRecOn==1);\n\tSYSLOG("ngreen", "XE-GPUREC: sym rge=%d sfw=%d def=%d s5=%d cs5=%d lists=%d/%d(n=%llu/%llu) sig=%d%d%d%d%d route=%d",')
 p.write_text(s)
