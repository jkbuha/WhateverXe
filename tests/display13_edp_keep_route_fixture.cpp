// -wex-edp-keep through the production wrappers: the donor's receiver-caps
// reads (wrapDisplay13ReadAUX) and its mode-set decision
// (wrapDisplay13RegsNeedUpdate) over the panel path the IdeaPad Slim 5
// 14IAH8's firmware lit, with the real boot readout, AUX bridge and owner
// lock, mock MMIO and a sink with the panel's measured DPCD.
struct Sink {uint8_t dpcd[0x2300] {};bool trained=false;unsigned statusReads=0;} sink;
unsigned sinkRead(void *,uint32_t address,void *data,uint32_t length){
 assert(!locked);auto b=static_cast<uint8_t *>(data);
 if(address<=0x202 && address+length>0x202)++sink.statusReads;
 uint8_t status[6] {};if(sink.trained){status[0]=0x77;status[2]=0x01;}
 for(uint32_t i=0;i<length;++i){const auto a=address+i;b[i]=a>=0x202 && a<0x208 ? status[a-0x202] : sink.dpcd[a];}
 return 0;
}
// Neither the caps view nor the decision writes to the sink.
unsigned sinkWrite(void *,uint32_t,void *,uint32_t){assert(false);return 0;}
// Samsung ATNA40CT01 (WEXEDPDPCD and the Linux i915 DPCD dump): receiver caps,
// SUPPORTED_LINK_RATES up to 5.4 Gbps, eDP 1.4b, and the link settings the
// firmware leaves: LINK_BW_SET 0x14 next to LINK_RATE_SET 3, two lanes with
// enhanced framing.
void sinkReset(){
 sink=Sink{};const uint8_t caps[16]={0x14,0x14,0xc2,0xc1,0,0,1,0xc0,2,0,0,0,0x3f,0x09,0x80,0};
 memcpy(sink.dpcd,caps,16);memcpy(sink.dpcd+0x2200,caps,16);
 const uint8_t rates[16]={0xa4,0x1f,0x30,0x2a,0x76,0x2f,0xbc,0x34,0x48,0x3f,0x60,0x54,0x78,0x69,0,0};
 memcpy(sink.dpcd+0x10,rates,16);sink.dpcd[0x700]=0x05;sink.dpcd[0x701]=0xfd;sink.dpcd[0x702]=0xff;sink.dpcd[0x703]=0x02;
 sink.dpcd[0x100]=0x14;sink.dpcd[0x101]=0x82;sink.dpcd[0x115]=3;sink.dpcd[0x10A]=0x80;sink.dpcd[0x600]=1;
}
void edpSetup(Fixture &f,Map &map){
 setup(f,map,0,0,2,10,true);auto &c=display13FBPower;state.display13FBAUXHooksReady=true;
 state.display13AUXControllerSlot=reinterpret_cast<uintptr_t>(&c.controller);put(f.ctl,0xC30,c.lock);
 for(unsigned n=0;n<6;++n)put(f.port,0x594+4*n,0x64010U+4*n);
 state.orgDisplay13ReadAUX=reinterpret_cast<uintptr_t>(sinkRead);state.orgDisplay13WriteAUX=reinterpret_cast<uintptr_t>(sinkWrite);
 f.port[0x11A]=1;f.port[0x118]=0;put(f.port,0x550,1U);
 for(auto reg:{0x60544U,0x70008U,0x164280U,0x46010U,0x46014U,0x164284U,0x164288U,0x16428CU,0x164290U,0xC7200U,0xC7204U})map.regs[reg]=0;
 map.regs[0x64000]=0x80;sinkReset();
}
// getDPCDInfo and the link setup read the receiver caps, the extended caps,
// the rate table and the LINK_BW_SET they reuse. Under -wex-edp-keep every view
// is capped at HBR; otherwise the panel's values pass unchanged.
void capsScenarios(bool keep,bool hook=true){
 Fixture f(0,0,0);Map map;edpSetup(f,map);
 // The decision hook is installed (any non-null original); without it the caps stay native.
 state.orgDisplay13RegsNeedUpdate=hook ? 1 : 0;keep=keep && hook;
 uint8_t caps[16],ext[16],rates[16],bw[2];
 assert(!XE::wrapDisplay13ReadAUX(f.port,0,caps,16) && !XE::wrapDisplay13ReadAUX(f.port,0x2200,ext,16));
 assert(!XE::wrapDisplay13ReadAUX(f.port,0x10,rates,16) && !XE::wrapDisplay13ReadAUX(f.port,0x100,bw,2));
 assert(caps[1]==(keep?0x0A:0x14) && ext[1]==(keep?0x0A:0x14) && bw[0]==(keep?0x0A:0x14) && bw[1]==0x82);
 for(unsigned i=0;i<16;++i)if(i!=1)assert(caps[i]==sink.dpcd[i] && ext[i]==sink.dpcd[0x2200+i]);
 for(unsigned i=0;i<16;++i)assert(rates[i]==(keep && i>=8 ? 0 : sink.dpcd[0x10+i]));
 // Only the donor's view changes: the sink keeps its own values.
 assert(sink.dpcd[0x1]==0x14 && sink.dpcd[0x100]==0x14);
 assert(objectRefs.empty());
}
// The panel path the firmware lit: pipe A -> DDI A eDP SST x2, 8 bpc,
// 1920x1200 from the panel's EDID DTD (154.25 MHz, 1920+160 x 1200+36, sync
// 48/32, 8/8), and the donor's CRTCParams for the same mode.
struct KeepMode {uint32_t ha,hb,hso,hsw,va,vb,vso,vsw;};
const KeepMode keepPanel {1920,160,48,32,1200,36,8,8};
void keepTimings(const KeepMode &t,uint32_t out[6]) {
 const uint32_t htotal=t.ha+t.hb,hstart=t.ha+t.hso,hend=hstart+t.hsw,vtotal=t.va+t.vb;
 out[0]=out[1]=(((htotal-1)<<16)&0x3FFF0000U)|((t.ha-1)&0x3FFFU);
 out[2]=(((hend-1)<<16)&0x3FFF0000U)|((hstart-1)&0x3FFFU);
 out[3]=out[4]=(((vtotal-1)<<16)&0x1FFF0000U)|((t.va-1)&0x1FFFU);
 const uint32_t vstart=0x1FFFU+t.va+t.vso,vend=vstart+t.vsw;
 out[5]=((vend<<16)&0x1FFF0000U)|(vstart&0x1FFFU);
}
void firmwareLink(Map &map,const KeepMode &m) {
 uint32_t t[6];keepTimings(m,t);
 map.regs[0x45404]|=0x400; // PW_A state
 const std::pair<uint32_t,uint32_t> regs[]={{0x60400,0x8A010102},{0x60540,0x80040300},{0x64000,0x80000002},
  {0x46140,0x10000000},{0x60030,0x7E5D159E},{0x60034,0x800000},{0x60040,0x7C1CD},{0x60044,0x80000},
  {0x60000,t[0]},{0x60004,t[1]},{0x60008,t[2]},{0x6000C,t[3]},{0x60010,t[4]},{0x60014,t[5]},
  {0x6001C,0x077F04AF},{0x70008,0xC0000000}};
 for(const auto &r:regs)map.regs[r.first]=r.second;
}
void keepParams(uint8_t *p,const KeepMode &m,uint32_t lanes=2,uint32_t bpc=0) {
 memset(p,0,0x100);uint32_t timing[6];keepTimings(m,timing);
 const uint32_t func=0x8A010000U|(bpc<<20)|((lanes-1)<<1),source=((m.ha-1)<<16)|(m.va-1);
 memcpy(p+0x4,&func,4);memcpy(p+0x10,timing,sizeof(timing));memcpy(p+0x28,&source,4);
}
unsigned regsCalls=0;bool regsResult=true;
bool nativeRegs(void *,void *,void *,void *,const void *,void *){assert(!locked);++regsCalls;return regsResult;}
struct KeepResult {bool update=true,recorded=false;uint32_t reasons=0,kept=0,call=0,portLanes=0,portRate=0;unsigned statusReads=0;size_t writes=0;};
template<class Change> KeepResult keepCase(Change change) {
 Fixture f(0,0,0);Map map;edpSetup(f,map);auto &c=display13FBPower;
 state.orgDisplay13RegsNeedUpdate=reinterpret_cast<uintptr_t>(nativeRegs);regsResult=true;
 firmwareLink(map,keepPanel);sink.trained=true;
 // WhateverXe adopted this boot path from the firmware (N97FBBootHandoff).
 c.bootPanel=true;c.bridge.bootPort=0;c.bridge.bootPath=reinterpret_cast<uintptr_t>(f.path);c.bridge.bootAdopted=true;
 uint8_t params[0x100];keepParams(params,keepPanel);
 // No earlier decision on this path unless the case says so (a new fixture
 // may reuse the previous one's address).
 display13EDPKeepPath=0;
 change(f,map,params);
 auto &data=IORegistryEntry::getRegistryRoot()->data;data.erase("WEXEDPKeep");
 const size_t writes=map.writes.size();const unsigned reads=sink.statusReads,calls=regsCalls;
 KeepResult r;r.update=XE::wrapDisplay13RegsNeedUpdate(f.ctl,f.fb,f.path,params,nullptr,nullptr);
 assert(regsCalls==calls+1 && !locked);
 r.writes=map.writes.size()-writes;r.statusReads=sink.statusReads-reads;
 if(data.count("WEXEDPKeep")) {
  const auto &v=data["WEXEDPKeep"];assert(v.size()==sizeof(Display13EDPKeepRecord));
  Display13EDPKeepRecord record;memcpy(&record,v.data(),sizeof(record));
  r.recorded=true;r.reasons=record.reasons;r.kept=record.kept;r.call=record.call;r.portLanes=record.portLanes;r.portRate=record.portRate;
 }
 assert(objectRefs.empty());
 return r;
}
Display13EDPKeepRecord firstRecord() {
 auto &data=IORegistryEntry::getRegistryRoot()->data;
 assert(data.count("WEXEDPKeepFirst"));const auto &v=data["WEXEDPKeepFirst"];assert(v.size()==sizeof(Display13EDPKeepRecord));
 Display13EDPKeepRecord record;memcpy(&record,v.data(),sizeof(record));return record;
}
void keepScenarios() {
 namespace K=Display13EDPKeep;unsigned scenarios=0;
 const auto none=[](Fixture &,Map &,uint8_t *){};
 // 1. The firmware's link runs the requested mode: no complete mode set, no
 //    register write, one DPCD status read outside the owner lock. The donor's
 //    rate (HBR) and the firmware's LINK_BW_SET (0x14) differ, as on the
 //    IdeaPad, and do not matter: the link is not touched.
 {auto r=keepCase(none);assert(!r.update && r.recorded && !r.reasons && r.kept==1 && !r.writes && r.statusReads==1);
  assert(r.call==1 && r.portLanes==2 && r.portRate==0x0A);
  const auto f=firstRecord();assert(f.call==1 && f.kept==1 && !f.reasons && f.ddi==0 && !f.pipe);
  assert(testSyslog().size()==1 && strstr(testSyslog()[0].c_str(),"firmware link kept"));++scenarios;}
 // The panel's Combo port before setPortMode (native mode 0, the handoff's
 // terms in display13PanelBootPort) is the same panel; any other mode is not.
 {auto r=keepCase([](Fixture &f,Map &,uint8_t *){put(f.port,0x550,0U);});assert(!r.update && !r.reasons && r.kept==2);++scenarios;}
 {auto r=keepCase([](Fixture &f,Map &,uint8_t *){put(f.port,0x550,2U);});assert(r.update && r.reasons==K::NotPanel && !r.statusReads);++scenarios;}
 // 2. Recorded only: an untrained sink status, another refresh of the same image.
 {auto r=keepCase([](Fixture &,Map &,uint8_t *){sink.trained=false;});assert(!r.update && r.reasons==K::Sink && r.kept==3);++scenarios;}
 {auto r=keepCase([](Fixture &,Map &,uint8_t *p){KeepMode m=keepPanel;m.vb=60;keepParams(p,m);});
  assert(!r.update && r.reasons==K::Blanking);++scenarios;}
 // 3. Never kept: a stopped or retrained source, another image, a scaled GOP
 //    mode, another lane count or depth, a path WhateverXe did not adopt, a
 //    pipe that is off.
 {auto r=keepCase([](Fixture &,Map &map,uint8_t *){map.regs[0x64000]=0x82;});assert(r.update && (r.reasons&K::NoSource) && !r.writes);++scenarios;}
 {auto r=keepCase([](Fixture &,Map &map,uint8_t *){map.regs[0x60540]=0x80040000;});assert(r.update && (r.reasons&K::NoSource));++scenarios;}
 {auto r=keepCase([](Fixture &,Map &,uint8_t *p){KeepMode m=keepPanel;m.ha=1680;m.hb=400;keepParams(p,m);});assert(r.update && (r.reasons&K::Timing));++scenarios;}
 {auto r=keepCase([](Fixture &,Map &map,uint8_t *){map.regs[0x6001C]=0x03FF02FF;});assert(r.update && r.reasons==K::Timing);++scenarios;}
 // 6. A scaled mode of the native timing (WindowServer's 1600x1000 on the
 //    1920x1200 panel): kept, the donor's scaler path programs scaler and
 //    PIPESRC. After a kept decision the scaled source is the donor's own, so
 //    back to native and on to another scaled mode are kept too; a path whose
 //    latest decision was not kept still refuses a scaled source.
 {auto r=keepCase([](Fixture &,Map &,uint8_t *p){const uint32_t s=(1599U<<16)|999U;memcpy(p+0x28,&s,4);});
  assert(!r.update && r.reasons==K::Source && !r.writes);++scenarios;}
 {auto r=keepCase([](Fixture &f,Map &map,uint8_t *){map.regs[0x6001C]=(1599U<<16)|999U;display13EDPKeepPath=reinterpret_cast<uintptr_t>(f.path);});
  assert(!r.update && r.reasons==K::Source);++scenarios;}
 {auto r=keepCase([](Fixture &f,Map &map,uint8_t *p){map.regs[0x6001C]=(1599U<<16)|999U;display13EDPKeepPath=reinterpret_cast<uintptr_t>(f.path);
   const uint32_t s=(1679U<<16)|1049U;memcpy(p+0x28,&s,4);});
  assert(!r.update && r.reasons==K::Source);++scenarios;}
 {auto r=keepCase([](Fixture &f,Map &map,uint8_t *){map.regs[0x6001C]=(1599U<<16)|999U;display13EDPKeepPath=reinterpret_cast<uintptr_t>(f.path)+8;});
  assert(r.update && r.reasons==K::Timing);++scenarios;}
 {auto r=keepCase([](Fixture &,Map &,uint8_t *p){keepParams(p,keepPanel,1);});assert(r.update && r.reasons==K::Format);++scenarios;}
 {auto r=keepCase([](Fixture &,Map &,uint8_t *p){keepParams(p,keepPanel,2,1);});assert(r.update && r.reasons==K::Format);++scenarios;}
 {auto r=keepCase([](Fixture &f,Map &,uint8_t *){put(f.port,0x124,1U);});assert(r.update && r.reasons==K::Format);++scenarios;}
 {auto r=keepCase([](Fixture &,Map &,uint8_t *){display13FBPower.bridge.bootPath=1;});assert(r.update && r.reasons==K::NotBoot);++scenarios;}
 {auto r=keepCase([](Fixture &,Map &,uint8_t *){display13FBPower.bootPanel=false;});assert(r.update && r.reasons==K::NotBoot);++scenarios;}
 {auto r=keepCase([](Fixture &,Map &,uint8_t *){display13FBPower.bridge.bootAdopted=false;});assert(r.update && r.reasons==K::NotBoot);++scenarios;}
 {auto r=keepCase([](Fixture &,Map &map,uint8_t *){map.regs[0x70008]=0x80000000;});assert(r.update && (r.reasons&K::PipeOff));++scenarios;}
 // 4. Not the panel's pipe A: no readout, no AUX.
 {auto r=keepCase([](Fixture &f,Map &,uint8_t *){put(f.path,0x3640,1U);});assert(r.update && r.reasons==K::NotPanel && !r.statusReads);++scenarios;}
 // 5. The donor already found nothing to update: untouched, unrecorded.
 {auto r=keepCase([](Fixture &,Map &,uint8_t *){regsResult=false;});assert(!r.update && !r.recorded && !r.statusReads);++scenarios;}
 // The first decision stays published next to the last one, and only the
 // first kept link is logged.
 {const auto f=firstRecord();assert(f.call==1 && !f.reasons);assert(testSyslog().size()==1);}
 printf("PASS Display13 eDP keep: %u mode-set decisions over a firmware-lit panel through the production wrappers\n",scenarios);
}
// Without -wex-edp-keep the decision is the donor's, unrecorded.
void offScenario() {
 Fixture f(0,0,0);Map map;edpSetup(f,map);
 state.orgDisplay13RegsNeedUpdate=reinterpret_cast<uintptr_t>(nativeRegs);regsResult=true;firmwareLink(map,keepPanel);sink.trained=true;
 auto &c=display13FBPower;c.bootPanel=true;c.bridge.bootPort=0;c.bridge.bootPath=reinterpret_cast<uintptr_t>(f.path);c.bridge.bootAdopted=true;
 uint8_t params[0x100];keepParams(params,keepPanel);const unsigned reads=sink.statusReads;
 assert(XE::wrapDisplay13RegsNeedUpdate(f.ctl,f.fb,f.path,params,nullptr,nullptr) && sink.statusReads==reads);
 assert(!IORegistryEntry::getRegistryRoot()->data.count("WEXEDPKeep") && testSyslog().empty());
 printf("PASS Display13 eDP keep off: the donor's caps and mode-set decision unchanged\n");
}
int main(int argc,char **argv){
 for(int i=1;i<argc;++i)testKernelArguments().insert(argv[i]);
 const bool keep=testKernelArguments().count("-wex-edp-keep")!=0;
 capsScenarios(keep);
 // A donor build the decision hook does not match keeps nothing: its caps stay native.
 capsScenarios(keep,false);
 if(keep)keepScenarios();else offScenario();
 return 0;
}
