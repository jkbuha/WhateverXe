#!/usr/bin/env python3
"""Execute the production mode wrapper with owned, modeled native state; no GPU writes."""
from pathlib import Path
import re,subprocess,tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'WhateverXe/kern_xe.cpp').read_text();body=re.search(r'uint32_t XE::wrapTGLSetPortMode\(.*?\n\}',s,re.S).group()
with tempfile.TemporaryDirectory(prefix='right-hotplug-') as t:
 p=Path(t);(p/'test.cpp').write_text((r/'tests/right_hotplug_fixture.cpp').read_text().replace('// PRODUCTION_WRAPPER',body))
 subprocess.run(['clang++','-std=c++17','-O1','-g','-fsanitize=address,undefined','-fno-omit-frame-pointer','-I'+str(r/'WhateverXe'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
