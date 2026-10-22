"""Execute only the exact signed PCI selector fragment; no MSR or driver calls."""
import ctypes as C,hashlib,mmap,platform,struct,sys
from pathlib import Path
assert platform.machine().lower() in ('x86_64','amd64')
data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() in ('bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0','6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64')
start,end=0x23C1F,0x23C6C
original=data[start:end];assert data[0x23C2F:0x23C37]==bytes.fromhex('81ff8680489a7425')
for patched in (False,True):
 code=bytearray(original)
 if patched:code[0x23C2F-start:0x23C37-start]=bytes.fromhex('81ff8680a0a77425')
 # All fragment exits converge on a normal userspace epilogue. Replace the
 # unknown-ID panic call with a jump to that exit; never execute kernel calls.
 for offset in (0x23C37,0x23C4F,0x23C67):
  rel=offset-start;code[rel:rel+5]=b'\xe9'+struct.pack('<i',len(code)-rel-5)
 program=bytes.fromhex('41564989f6')+code+bytes.fromhex('415ec3') # save/bind R14
 mem=mmap.mmap(-1,mmap.PAGESIZE,prot=mmap.PROT_READ|mmap.PROT_WRITE|mmap.PROT_EXEC);mem[:len(program)]=program
 fn=C.CFUNCTYPE(None,C.c_uint32,C.c_void_p)(C.addressof(C.c_char.from_buffer(mem)))
 for identity,expected in [(0xA7A08086,2 if patched else 0xA5A5A5A5),(0x9A408086,2),(0x9A488086,0xA5A5A5A5 if patched else 2),(0xDEAFBEEF,1),(0xFF208086,2),(0xA7A08087,0xA5A5A5A5),(0x46D18086,0xA5A5A5A5)]:
  obj=C.create_string_buffer(b'\xa5'*8192);fn(identity,C.addressof(obj));assert struct.unpack_from('<I',obj.raw,0x1120)[0]==expected
  changed=[i for i,v in enumerate(obj.raw[:8192]) if v!=0xA5]
  assert changed==([] if expected==0xA5A5A5A5 else list(range(0x1120,0x1124)))
 mem.close()
print('PASS actual native donor PCI selector: A7A0 selects branch2, signed split preserved, only selector field changes')
