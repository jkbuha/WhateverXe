"""Replay the signed scheduler's real priority defaults and credit refill on RAM."""
import ctypes as C,hashlib,mmap,struct,sys
from pathlib import Path
b=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(b).hexdigest()=='6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64'
assert b[0x2726d:0x27272]==bytes.fromhex('ba02000000')
assert b[0x949d0:0x949e3]==b'SingleChannelIndex\0'
assert b[0x39187:0x39193]==bytes.fromhex('41f784249011000000800100')
assert b[0x39193:0x39199][:2]==b'\x0f\x85'
assert b[0x3934f:0x3935a]==bytes.fromhex('41c785cc05000004000000')
def fn(code,signature):
 m=mmap.mmap(-1,mmap.PAGESIZE,prot=7);m[:len(code)]=code
 return m,signature(C.addressof(C.c_char.from_buffer(m)))
# Save/bind the original R12/R13/RBX operands; the signed fragment otherwise runs unchanged.
prefix=bytes.fromhex('53415441554989fc4989f531db')
gate=b[0x39187:0x39193];defaults=b[0x3934f:0x393b8];suffix=bytes.fromhex('415d415c5bc3')
code=prefix+gate+b'\x0f\x85'+struct.pack('<i',len(defaults))+defaults+suffix
m,initialize=fn(code,C.CFUNCTYPE(None,C.c_void_p,C.c_void_p))
n,refill=fn(b[0x3864e:0x3867c],C.CFUNCTYPE(None,C.c_void_p))
for mode in range(4):
 accel=C.create_string_buffer(0x2000);sched=C.create_string_buffer(0x1000)
 struct.pack_into('<I',accel,0x1190,mode<<15);initialize(accel,sched)
 saved=[struct.unpack_from('<I',sched,0x5cc+i*0x180)[0] for i in range(4)]
 assert saved==([4,3,2,1] if mode==0 else [0]*4),(mode,saved)
 refill(sched)
 credits=[struct.unpack_from('<I',sched,0x5c8+i*0x180)[0] for i in range(4)]
 assert credits==saved
 # Missing property defaults to mode2: weighted evaluator needs credits but init skips them.
 assert (mode==1)==((mode<<15)&0x18000==0x8000)
 for i in range(4):struct.pack_into('<I',sched,0x5c8+i*0x180,0)
 refill(sched)
 assert [struct.unpack_from('<I',sched,0x5c8+i*0x180)[0] for i in range(4)]==saved
m.close();n.close()
print('PASS real signed priority initialization/refill: mode0 creates 4/3/2/1 credits; absent mode2 reproduces zero-credit weighted-evaluator loop')
