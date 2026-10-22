"""Execute the production module worker without kernel or GPU calls."""
from pathlib import Path
import subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_raptorlake_p_reference.cpp').read_text()
start=s.index('void moduleWorker(');end=s.index('\nvoid ready()',start)
body=s[start:end]
fixture=r'''#include <cassert>
#include <cstdint>
#include <cstring>
using thread_call_param_t=void *;
unsigned mode,calls,publishes;uint32_t moduleState[8]{1},state[9]{1,1};void *target=(void *)1;
bool wexRplReferencePanel(){return mode!=1;}
void publish(){++publishes;}
int fake(const char *id,bool defer,bool delay,uint8_t start,uint8_t match,void *names){
 const char *ids[]{"org.b00t0x.AppleIntelTGLCompat","com.apple.driver.AppleIntelTGLGraphicsFramebuffer","com.apple.driver.AppleIntelTGLGraphics"};
 assert(calls<3&&!strcmp(id,ids[calls]));assert(!defer&&delay&&start==0&&match==2&&!names);
 ++calls;return mode==10+calls?91:0;
}
using ModuleLoader=int (*)(const char *,bool,bool,uint8_t,uint8_t,void *);ModuleLoader moduleLoader=fake;
BODY
int main(){for(mode=0;mode<15;++mode){
 calls=publishes=0;memset(moduleState,0,sizeof(moduleState));memset(state,0,sizeof(state));moduleState[0]=1;moduleState[1]=1;state[1]=1;target=(void *)1;moduleLoader=fake;
 if(mode==2)moduleLoader=nullptr;if(mode==3)target=nullptr;if(mode==4)state[1]=0;if(mode==5)state[4]=1;if(mode==6)state[7]=1;if(mode==7)moduleState[1]=0;
 moduleWorker(nullptr,nullptr);
 if(mode>=1&&mode<=7){assert(!calls&&!publishes);continue;}
 if(mode>=11&&mode<=13){assert(calls==mode-10&&moduleState[7]==calls&&!moduleState[6]);continue;}
 assert(calls==3&&moduleState[6]==1&&!moduleState[7]);
}}
'''.replace('BODY',body)
with tempfile.TemporaryDirectory(prefix='rplp-module-worker-') as d:
 p=Path(d);(p/'test.cpp').write_text(fixture)
 subprocess.run(['clang++','-std=c++17','-fsanitize=address,undefined',str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('PASS actual module worker: closed gates, exact providers, no defer, no matching and all failures')
