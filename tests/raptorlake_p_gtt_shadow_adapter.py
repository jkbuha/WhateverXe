"""Replay the production RAM-backed GGTT owner and lifetime with IOKit mocks."""
import ast,re,subprocess,tempfile
from pathlib import Path
r=Path(__file__).resolve().parents[1]
tree=ast.parse((r/'tests/raptorlake_p_native_memory_adapter.py').read_text())
node=next(n for n in tree.body if isinstance(n,ast.Assign) and any(isinstance(t,ast.Name) and t.id=='fixture' for t in n.targets))
prefix=node.value.func.value.value.split('namespace RaptorLakeP{namespace GTTShadow{')[0]
prefix=prefix.replace('#include "kern_xelp_memory_layout.hpp"','#include "kern_xelp_memory_layout.hpp"\n#include "kern_raptorlake_p_gtt_shadow_core.hpp"\n#include "kern_raptorlake_p_gtt_slot_core.hpp"\n#include "kern_raptorlake_p_gtt_aperture_core.hpp"\nbool nativeObjectAlive=false;unsigned nativeCalls=0,buffersReleased=0;')
prefix=prefix.replace('void release(){assert(refs>0);--refs;}', 'void retain(){++refs;}void release(){assert(refs>0);--refs;}')
prefix=prefix.replace('bool pagesFlag=false,','bool pagesFlag=true,shadowFlag=true,')
prefix+='\nconstexpr unsigned kIODirectionInOut=3,kIOReturnSuccess=0,kIOPreparationIDAlwaysPrepared=2,kIOPreparationIDUnprepared=0;bool pinFlag=false;\n'
prefix=prefix.replace('if(!strcmp(s,"-wex-rplp-native-pages"))','if(!strcmp(s,"-wex-rplp-gtt-aperture"))return mode>=32;if(!strcmp(s,"-wex-rplp-gtt-slot"))return mode>=25;if(!strcmp(s,"-wex-rplp-pin-pages"))return mode>=19 && mode!=29;if(!strcmp(s,"-wex-rplp-gtt-shadow"))return shadowFlag;if(!strcmp(s,"-wex-rplp-native-pages"))')
prefix+='\nvoid IODelay(unsigned){}\n'
prefix=prefix.replace('return 0x2c1;','return mode==4?0:0x2c1;')
prefix=prefix.replace('"WhateverXeRPLPNativeMemory"','"WhateverXeRPLPGTTShadow"')
prefix=prefix.replace('uint64_t physical=0;','uint64_t physical=0;std::vector<uint8_t> ram;static IOBufferMemoryDescriptor *withCapacity(size_t n,unsigned){if(mode==5)return nullptr;auto p=new IOBufferMemoryDescriptor;p->ram.resize(n);return p;}void release(){assert(!nativeObjectAlive);++buffersReleased;OSObject::release();if(!refs&&!ram.empty())delete this;}')
prefix=prefix.replace('return mode==36&&physical==0x123000?4095:4096;','return ram.empty()?4096:ram.size();')
start=prefix.index('IOMemoryMap *map(unsigned options)');end=prefix.index('return &view;}',start)+len('return &view;}')
prefix=prefix[:start]+'''IOMemoryMap *map(unsigned options=0){assert(options==0);if(mode==6)return nullptr;++mapsCreated;view.refs=1;view.page=true;view.cpu=mode==7?0:reinterpret_cast<uintptr_t>(ram.data());view.length=mode==8?4096:ram.size();return &view;}'''+prefix[end:]
prefix=prefix.replace('struct KernelPatcher {','''struct KernelPatcher {
 struct KextInfo{size_t loadIndex=0;};struct LookupPatch{KextInfo *info;const uint8_t *find,*replace;size_t size,count;};
 enum class Error{NoError,Failure};Error error=Error::NoError;Error getError(){return error;}
 void applyLookupPatch(LookupPatch *p,uint8_t *where,size_t bytes){assert(p->info->loadIndex==7 && bytes==p->size+1 && p->count==1 && !memcmp(where,p->find,p->size));if(mode==3){error=Error::Failure;return;}memcpy(where,p->replace,p->size);}
''')
prefix=re.sub(r'unsigned off=strstr\(name,.*?;', 'unsigned off=strstr(name,"15initWithOptions")?0x101b4:0x10312;',prefix)
prefix=prefix.replace('mode==8?1:0','mode==1?1:0').replace('void clearError(){}','void clearError(){error=Error::NoError;}')
prefix=prefix.replace('uint64_t physical=0;', 'uint64_t physical=0;unsigned wires=1;uint64_t getPreparationID(){return wires?0x100000001:0;}int prepare(unsigned d){assert(d==3);if((mode==20&&physical==0x7ef1234000)||(mode==21&&physical==0x7ef1235000))return 1;++wires;return 0;}int complete(unsigned d){assert(d==3&&wires);if(mode==22&&physical==0x7ef1234000&&wires==1)return 1;--wires;return 0;}')
prefix=prefix.replace('uint16_t configRead16(unsigned off){assert(off==0x50);', 'uint8_t configRead8(unsigned){return 4;}uint16_t configRead16(unsigned off){assert(off==0x50);')
source=(r/'WhateverXe/kern_raptorlake_p_gtt_shadow.cpp').read_text()
body=source[source.index('namespace RaptorLakeP {namespace GTTShadow {'):]
# Hardware qualification is independently replayed by raptor_lake_p_gtt_slot.
# Substitute only that gate here; exercise the real owner, transaction and pins.
body=body.replace('GTTSlot::qualify(io,layout)','(mode!=28)')
body=body.replace('*reinterpret_cast<volatile uint64_t *>(base+off)=v;asm', 'if(mode!=26&&mode!=39)*reinterpret_cast<volatile uint64_t *>(base+off)=v;asm')
prefix+='\nstd::vector<uint8_t> hardware(0x1000000);using IOMemoryDescriptor=Aperture;constexpr unsigned kIOMapAnywhere=4,kIOMapInhibitCache=8;void *kernel_task=nullptr;\n'
prefix=prefix.replace('uint64_t getPhysicalAddress(){return mode==15?0:0x4011000000;}', 'uint64_t mappedPhysical=0;uint64_t getPhysicalAddress(){return mappedPhysical?mappedPhysical:mode==15?0:0x4011000000;}')
prefix=prefix.replace('*span=mode==34?0:4096;', '*span=4096;')
prefix=prefix.replace('assert(options==0);if(mode==6)return nullptr;', 'assert(options==0||options==kIOMapReadOnly);if(mode==6||(mode==34&&options==kIOMapReadOnly))return nullptr;')
prefix=prefix.replace('reinterpret_cast<uintptr_t>(ram.data());view.length=mode==8?4096:ram.size();', 'reinterpret_cast<uintptr_t>(ram.empty()?content:ram.data());view.length=mode==8?4096:ram.empty()?4096:ram.size();')
prefix=prefix.replace('struct Aperture{uint64_t getLength(){return 0x10000000;}};', '''struct Aperture{IOMemoryMap view;uint32_t alternate[1024]{};uint64_t getLength(){return 0x10000000;}
 uint64_t getPhysicalSegment(unsigned,IOByteCount *span,unsigned){*span=0x10000000;return 0x5000000000;}
 IOMemoryMap *createMappingInTask(void *,unsigned,unsigned,uint64_t off,unsigned size){assert(off==0xa00000&&size==4096);if(mode==33)return nullptr;++mapsCreated;view.page=true;view.refs=1;view.length=mode==36?4095:4096;view.cpu=reinterpret_cast<uintptr_t>(mode==38?alternate:reinterpret_cast<uint32_t *>(dataPage.content));view.mappedPhysical=mode==35?0x5000000000:0x5000a00000;return &view;}};''')

prefix=prefix.replace('return page?cpu:(mode==16?0:0x12340000);', 'return page?cpu:(mode>=25?reinterpret_cast<uintptr_t>(hardware.data()):mode==16?0:0x12340000);')
prefix=prefix.replace('struct IOMemoryMap:OSObject', 'extern std::vector<uint8_t> hardware;struct IOMemoryMap:OSObject')
prefix=prefix.replace('if(off==0x14)return 0x40;', 'if(off==0)return 0xa7a08086;if(off==0x14)return 0x40;')
prefix=prefix.replace('assert(off==0x50);return mode==4?0:0x2c1;', 'if(off==4)return 7;assert(off==0x50);return mode==4?0:0x2c1;')
prefix=prefix.replace('assert(!strcmp(key,"WhateverXeRPLPGTTShadow"));', 'assert(!strcmp(key,"WhateverXeRPLPGTTShadow") || !strcmp(key,"WhateverXeRPLPGTTSlot") || !strcmp(key,"WhateverXeRPLPGTTAperture"));')

tail=r'''
using namespace RaptorLakeP::GTTShadow;namespace GTTSlot=RaptorLakeP::GTTSlot;
template<class T>void put(void *o,unsigned off,T value){memcpy(static_cast<uint8_t *>(o)+off,&value,sizeof(value));}
OSObject gttObject;
bool native(OSObject *manager){
 ++nativeCalls;if(mode>=25)assert(field<uint64_t>(manager,0xa8)==GTTSlot::Index*4096);auto shadow=field<uintptr_t>(manager,0x18);assert(shadow!=0x12340000 && buffer && mapping && mapsReleased==0 && buffersReleased==0);
 if(mode==9||mode==31)return false;
 nativeObjectAlive=true;auto ptes=reinterpret_cast<uint64_t *>(shadow+GTTOffset);auto physical=field<uint64_t>(manager,0xf0);
 for(size_t i=mode>=25?GTTSlot::Index:0x4000;i<(mode>=25?GTTSlot::Index+1:Entries);++i)ptes[i]=physical|1;
 put(&gttObject,0x10,field<void *>(manager,0x10));put(&gttObject,0x18,uint32_t(0));put(&gttObject,0x20,shadow);put(&gttObject,0x28,shadow+GTTOffset);put(&gttObject,0x30,uint64_t(0x800000));put(&gttObject,0x38,physical);put(manager,0x98,&gttObject);
 if(mode==27)ptes[GTTSlot::Index+1]^=2;if(mode==10)ptes[0]^=2;if(mode==11)ptes[0x4000]^=2;if(mode==12)reinterpret_cast<uint8_t *>(shadow)[0]^=1;if(mode==13)put(&gttObject,0x30,uint64_t(0));
 return true;
}
int main(int argc,char **argv){assert(argc==2);mode=atoi(argv[1]);pinFlag=mode>=19;if(mode==18)shadowFlag=false;if(mode==17)nativeFlag=false;
 std::vector<uint8_t> image(0x11000);for(auto p:Patches)memcpy(image.data()+p.offset,p.before,p.size);memcpy(image.data()+SlotLimit.offset,SlotLimit.before,SlotLimit.size);if(mode==30)image[SlotLimit.offset]^=1;if(mode==2)image[Patches[0].offset]^=1;
 KernelPatcher patcher;const bool ready=prepare(patcher,7,reinterpret_cast<uintptr_t>(image.data()),image.size());
 if((mode>=1&&mode<=3)||mode==30){assert(!ready && !record[PatchReady]);return 0;}assert(ready);
 IOService ownerObject;IOPCIDevice pci;OSObject manager;IOMemoryMap real;
 if(mode>=32){auto w=reinterpret_cast<uint32_t *>(dataPage.content);for(unsigned i=0;i<1024;++i)w[i]=0xc01dbead;if(mode==37)w[123]=0;}if(mode>=25){auto p=reinterpret_cast<uint64_t *>(hardware.data()+GTTOffset);for(size_t i=0;i<0x4000;++i)p[i]=prefixPTE(0x6c800000,i);}
 put(&manager,0x10,&ownerObject);put(&manager,0x18,mode>=25?reinterpret_cast<uintptr_t>(hardware.data()):uintptr_t(0x12340000));put(&manager,0x68,&real);put(&manager,0xa8,uint64_t(0x4000000));put(&manager,0xf0,uint64_t(0x7ef1234000));
 if(mode==14)pci.profile=XELP::identify(0x9a498086,1);if(mode==15)put(&manager,0x98,&gttObject);if(mode==16)put(&manager,0x18,uintptr_t(0));
 if(pinFlag){dataPage.physical=0x7ef1234000;tablePage.physical=0x7ef1235000;put(&manager,0xe0,&dataPage);put(&manager,0xe8,&tablePage);put(&manager,0xf8,tablePage.physical);}
 const auto before=field<uintptr_t>(&manager,0x18);
 const bool passed=run(&manager,&ownerObject,&pci,reinterpret_cast<uintptr_t>(native));
 assert(field<uint64_t>(&manager,0xa8)==0x4000000);assert(field<uintptr_t>(&manager,0x18)==before && !mapsReleased && !buffersReleased);
 if(mode==14||mode==17||mode==18||mode==29){assert(!passed && !record[Attempted] && !nativeCalls);return 0;}
 assert(record[Attempted]);if(mode==0||mode==19||mode==22||mode==23||mode==24||mode==25||mode==32)assert(passed && record[ObjectValid] && !record[Failure] && record[PrefixPages]==16384 && record[TailPages]==(mode>=25?1:1032192));
 else assert(!passed && record[Failure]);
 if(mode==10)assert(record[PrefixChanged]==1);if(mode==11)assert(record[TailMismatches]==1);if(mode==12)assert(record[GuardChanged]==1);
 nativeObjectAlive=false;put(&manager,0x98,uintptr_t(0));if(pinFlag){assert(!dataPage.complete(3)&&!tablePage.complete(3));dataPage.release();tablePage.release();if(mode==23)dataPage.physical+=4096;if(mode==24)dataPage.wires=0;}finish();if(mode>=25){if(mode==26||mode==27||mode==39){assert(slot.retainPins&&record[PinReleasedMask]==0&&dataPage.wires==1&&tablePage.wires==1);}else assert(record[PinReleasedMask]==3&&!dataPage.wires&&!tablePage.wires);if(mode==25)assert(!slot.failure&&slot.restored&&!slot.remaining&&slot.invoked&&slot.rangeRestored);if(mode==28)assert(!slot.qualified&&!slot.invoked);if(mode==26)assert(!slot.restored&&slot.restorePolls==500);if(mode==27)assert(slot.restored&&slot.unexpected==1);
 if(mode==32)assert(apertureRead.checked==1024&&!apertureRead.mismatches&&!apertureRead.failure&&apertureRead.released);
 if(mode>=33&&mode<=37)assert(apertureRead.failure&&apertureRead.released&&!slot.invoked);
 if(mode==38)assert(apertureRead.checked==1024&&apertureRead.mismatches==1024&&apertureRead.failure==5&&slot.restored&&!slot.retainPins);
 if(mode==39)assert(apertureRead.checked==1024&&!apertureRead.failure&&slot.retainPins&&!slot.restored);}if(mode==19)assert(record[PinHeldMask]==3&&record[PinPrepareMask]==3&&record[PinCheckedMask]==3&&record[PinStableMask]==3&&record[PinCompletedMask]==3&&record[PinReleasedMask]==3&&!record[PinFailure]&&!dataPage.wires&&!tablePage.wires);if(mode==20||mode==21)assert(record[PinFailure]==1&&!dataPage.wires&&!tablePage.wires);if(mode==22)assert(record[PinFailure]==3&&record[PinReleasedMask]==2&&pins[0]&&dataPage.wires==1);if(mode==24)assert(record[PinFailure]==2&&record[PinReleasedMask]==2&&pins[0]&&!dataPage.wires);if(mode==23)assert(record[PinFailure]==2&&record[PinStableMask]==2&&!dataPage.wires&&!tablePage.wires);assert(record[Released] && mapsCreated==mapsReleased && !buffer && !mapping);
 const auto calls=nativeCalls;assert(!run(&manager,&ownerObject,&pci,reinterpret_cast<uintptr_t>(native)) && nativeCalls==calls);
}
'''
with tempfile.TemporaryDirectory(prefix='rplp-gtt-shadow-') as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(prefix+body+tail)
 subprocess.run(['clang++','-std=c++17','-O1','-fsanitize=address,undefined','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 for case in range(40):subprocess.run([str(p/'test'),str(case)],check=True)
print('PASS production shadow GGTT owner: patch gates, RAM-only backing, firmware prefix, complete tail, guard pages, restored real pointer and delayed cleanup')
