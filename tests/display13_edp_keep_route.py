#!/usr/bin/env python3
"""-wex-edp-keep through the production Display13 AUX wrappers and mode-set
decision: the panel's receiver caps as the donor reads them, and the decision
over a firmware-lit panel, with and without the switch. Optional exact donor
replay of the hooked hwRegsNeedUpdate and its hwSetMode call site."""
from pathlib import Path
import re,subprocess,sys,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'WhateverXe/kern_xe.cpp').read_text()
functions='\n'.join(re.search(re.escape(sig)+r'.*?\n\}',s,re.S).group() for sig in [
 'uint8_t *XE::display13SSTPort(', 'uint8_t *XE::display13PanelBootPort(', 'bool XE::display13HDMIPort(', 'bool XE::display13MainLinkAUX(',
 'uint32_t XE::wrapDisplay13EnablePort(', 'uint32_t XE::wrapDisplay13DisablePort(',
 'uint32_t XE::wrapDisplay13SignalLevels(', 'uint32_t XE::wrapDisplay13TrainingPattern(',
 'bool XE::display13AUXReady(', 'uint32_t XE::wrapDisplay13ReadAUX(', 'uint32_t XE::wrapDisplay13WriteAUX(',
 'bool XE::wrapDisplay13RegsNeedUpdate(',
 'bool XE::display13EDPBlankReady(', 'void XE::display13EDPBlankPublishReady(', 'bool XE::display13EDPBlankPower('])
binding=re.search(r'static TGLFBBinding::Readout adlnInspectPathBinding\(.*?\n\}',s,re.S).group()
keep=s[s.index('// -wex-edp-keep: keep the firmware-trained panel link (kern_display13_edp_keep,'):s.index('uint32_t XE::wrapDisplay13ReadAUX(')]
blank=s[s.index('// -wex-edp-blank (Display13EDPBlank), on top of'):s.index('bool XE::display13EDPBlankReady(')]
keep=(r/'tests/display13_edp_keep_stubs.hpp').read_text()+'\n'+keep+'\n'+blank
objects=(r/'tests/tgl_fb_binding.cpp').read_text().split('int main()')[0].replace('port[0x580]','port[0x700]')
base=(r/'tests/display13_sst_fixture.cpp').read_text().split('int main(')[0]
# The real context's boot readout and panel flag, read through the same
# unpowered-safe boot IO (absent registers read 0).
bootio='''struct Display13FBBootIO {
 Map *map;explicit Display13FBBootIO(Map *m):map(m){}
 bool readable(uint32_t r) const {return r<0x1000000;}
 uint32_t read(uint32_t r){auto i=map->regs.find(r);return i==map->regs.end()?0:i->second;}
};
'''
base=base.replace(' uint32_t base=0,drop=0,delay=0,idleAfter=0,idleReg=0;',' uint32_t base=0,drop=0,delay=0,idleAfter=0,idleReg=0;\n uintptr_t getVirtualAddress();size_t length=0x1000000;size_t getLength() {return length;}',1)
base=base.replace('// NATIVE_OBJECTS',objects).replace('// BINDING',binding).replace('// FUNCTIONS',bootio+keep+'\n'+functions)
base=base.replace('bool adlnWriterMappingBound()','void adlnFault(bool &fault,unsigned){fault=true;}\nbool adlnWriterMappingBound()')
assert base.count('constexpr unsigned kIOReturnNotReady=0xE00002D8;')==1
base=base.replace('constexpr unsigned kIOReturnNotReady=0xE00002D8;','constexpr unsigned kIOReturnNotReady=0xE00002D8,kIOReturnSuccess=0;')
base='#include "kern_alderlake_n.hpp"\n#include "kern_display13_edp_blank.hpp"\n#include <cstdio>\n'+base
assert base.count(' Map *map=nullptr;')==1
base=base.replace(' Map *map=nullptr;',' Map *map=nullptr;bool bootPanel=false;AlderLakeN::DisplayReadout displayScratch;')
header=(r/'WhateverXe/kern_xe.hpp').read_text()
extra=header[header.index(' bool display13FBAUXHooksReady'):header.index(' mach_vm_address_t orgDisplay13TrainingPattern')]
extra=extra.replace(' bool processDisplay13FramebufferAUX(KernelPatcher &,size_t,mach_vm_address_t,size_t);','').replace('mach_vm_address_t','uintptr_t')
assert ' mach_vm_address_t orgDisplay13RegsNeedUpdate {};' in header and ' static bool wrapDisplay13RegsNeedUpdate(void *controller,void *fb,void *path,void *params,const void *timing,void *scaler);' in header
extra+=' static bool display13EDPBlankPower(IOService *,uint32_t,uint32_t &);static bool display13EDPBlankReady();static void display13EDPBlankPublishReady();static bool display13EDPKeepActive();\n'
extra+=' bool fbOwned {true};bool ownedDisplay13Framebuffer(IOService *) const {return fbOwned;}\n'
extra+=' uintptr_t orgDisplay13RegsNeedUpdate {};\n static bool wrapDisplay13RegsNeedUpdate(void *controller,void *fb,void *path,void *params,const void *timing,void *scaler);\n'
base=base.replace('struct XE {','struct XE {\n'+extra)
code=base+(r/'tests/display13_edp_keep_route_fixture.cpp').read_text()
if len(sys.argv)>1:
 b=Path(sys.argv[1]).read_bytes()
 install=s[s.index('bool XE::processDisplay13FramebufferSST('):s.index('uint8_t *XE::display13SSTPort(')]
 entry=bytes(int(x,0) for x in re.search(r'regsEntry\[\]=\{([^}]+)\}',install).group(1).split(','))
 assert 'u==address+0x4F044' in install and b[0x4F044:0x4F044+len(entry)]==entry
 # hwSetMode: call hwRegsNeedUpdate, test al, je to the same-mode branch, whose
 # two "Partial modeset" paths and "Complete modeset" are the donor's strings.
 call=b[0x4FCA1:0x4FCAA]
 assert call[0]==0xE8 and 0x4FCA6+int.from_bytes(call[1:5],'little',signed=True)==0x4F044 and call[5:8]==bytes.fromhex('84c074')
 for text in [b'Partial modeset by updating plane',b'Partial modeset by updating scaler',b'Complete modeset']:assert text in b
 # Scaled modes: the same-mode branch's scaler path calls programPipeScaler,
 # which writes PIPESRC (0x6001C + pipe) from the path's requested source (+0x150).
 call=b[0x4FE1E:0x4FE23]
 assert call[0]==0xE8 and 0x4FE23+int.from_bytes(call[1:5],'little',signed=True)==0x65FD4
 assert b[0x66135:0x66143]==bytes.fromhex('418db61c000600418b9550010000')
 print('PASS exact donor hwRegsNeedUpdate entry and hwSetMode same-mode branch, scaler path writes PIPESRC')
with tempfile.TemporaryDirectory(prefix='display13-edp-keep-') as d:
 p=Path(d);(p/'test.cpp').write_text(code)
 subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for args in [[],['-wex-edp-keep'],['-wex-edp-keep','-wex-edp-blank'],['-wex-edp-blank']]:
  subprocess.run([str(p/'test')]+args,check=True)
