#!/usr/bin/env python3
"""Pin the opt-in IOGVAScaler withdrawal: only after a successful native
accelerator start, only that key, gated by -wex-no-gva-scaler."""
from pathlib import Path
s=(Path(__file__).resolve().parents[1]/'WhateverXe/kern_xe.cpp').read_text()
start=s[s.index('bool XE::wrapTGLAcceleratorStart'):]
start=start[:start.index('\n}\n')]
blk=start[start.index('-wex-no-gva-scaler')-40:]
blk=blk[:blk.index('\t}\n')]
assert 'if (result && checkKernelArgument("-wex-no-gva-scaler"))' in blk
assert blk.count('removeProperty(')==1 and 'removeProperty("IOGVAScaler")' in blk
assert start.index('orgTGLAcceleratorStart)(that, provider)') < start.index('-wex-no-gva-scaler')
for keep in ['IOGVACodec','IOGVAHEVCDecode','IOGVABGRAEnc']:assert keep not in blk
print('PASS: IOGVAScaler withdrawal is opt-in, post-start, single key')
