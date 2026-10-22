"""Exact native GGTT constructor replay on RAM; no GPU or kernel execution."""
import ctypes as C,hashlib,mmap,platform,re,struct,sys
from pathlib import Path
assert platform.machine().lower() in ('x86_64','amd64')
data=Path(sys.argv[1]).read_bytes()
assert hashlib.sha256(data).hexdigest() in ('6d99a2b72f34648cbb68d651af53cb64a33e6298763e4f83a7c9f0be1406bb64','bd1a21f5e622aadabdab6441f6b5c38ed73fa3053c783fdf0fb7e4b309f273d0')
header=(Path(__file__).resolve().parents[1]/'WhateverXe/kern_raptorlake_p_gtt_shadow_core.hpp').read_text()
patches=[]
for off,n,before,after in re.findall(r'\{(0x[0-9A-F]+),(\d+),\{([^}]+)\},\{([^}]+)\}\}',header):
    parse=lambda v:bytes(int(x.strip(),0) for x in v.split(','))
    patches.append((int(off,0),int(n),parse(before),parse(after)))
assert len(patches)==4
limit=(0x1029c,10,bytes.fromhex("48be0000000001000000"),bytes.fromhex("48be0010a00000000000"))
assert data[limit[0]:limit[0]+10]==limit[2]
for patched,limited in ((False,False),(True,False),(True,True)):
    image=mmap.mmap(-1,0x200000,prot=7);image[:len(data)]=data
    base=C.addressof(C.c_char.from_buffer(image))
    # Model only superclass OSObject::init success; execute the real base init.
    assert data[0x13ffd:0x1400a]==bytes.fromhex('488b05dc410b00ff9098000000')
    image[0x13ffd:0x1400a]=bytes.fromhex('b001')+b'\x90'*11
    image[0x190000:0x190004]=bytes.fromhex('4889f8c3') # safeMetaCast identity
    image[0x190020:0x190026]=bytes.fromhex('b8c1020000c3') # read GGC
    for site,target in [(0x10200,0x190000),(0x1020d,0x190020)]:
        assert data[site:site+5]==b'\xe8\0\0\0\0'
        image[site+1:site+5]=struct.pack('<i',target-site-5)
    shift=C.c_uint32(12);page=C.c_uint64(4096);meta=C.c_void_p(0)
    for insn,end,pointer in [(0x101f6,0x101fd,C.addressof(meta)),(0x10230,0x10237,C.addressof(shift)),(0x102d7,0x102de,C.addressof(page))]:
        cell=end+struct.unpack_from('<i',data,insn+3)[0];struct.pack_into('<Q',image,cell,pointer)
    for off,n,before,after in patches:
        assert len(before)==len(after)==n and data[off:off+n]==before
        if patched:image[off:off+n]=after
    if limited:image[limit[0]:limit[0]+10]=limit[3]
    memory=mmap.mmap(-1,0x1002000,prot=3);memory[:]=b'\xa5'*len(memory)
    mmio=C.addressof(C.c_char.from_buffer(memory))+4096
    count=0x4000000//4096;stolen=0x6c800000;gtt=4096+0x800000
    prefix=b''.join(struct.pack('<Q',(stolen+i*4096)|1) for i in range(count))
    memory[gtt:gtt+len(prefix)]=prefix
    obj=C.create_string_buffer(0x80);owner=C.create_string_buffer(0x2000)
    struct.pack_into('<Q',owner,0x368,0x1234)
    region=(C.c_uint64*2)(0,0xa00000 if limited else 0x4000000);physical=0x7ef1234000
    fn=C.CFUNCTYPE(C.c_bool,C.c_void_p,C.c_void_p,C.c_void_p,C.c_void_p,C.c_uint64,C.c_uint)(base+0x101b4)
    assert fn(obj,owner,region,mmio,physical,0)
    expected=bytearray(prefix+b'\xa5'*(0x800000-len(prefix)))
    if limited:struct.pack_into('<Q',expected,8*0xa00,physical|1)
    else:
        if not patched:
            for i in range(count):struct.pack_into('<Q',expected,8*i,(stolen+i*4096)|3)
        expected[len(prefix):]=struct.pack('<Q',physical|1)*(0x100000-count)
    assert memory[gtt:gtt+0x800000]==expected
    assert memory[:gtt]==b'\xa5'*gtt and memory[-4096:]==b'\xa5'*4096
    assert struct.unpack_from('<Q',obj,0x20)[0]==mmio
    assert struct.unpack_from('<Q',obj,0x28)[0]==mmio+0x800000
    assert struct.unpack_from('<Q',obj,0x30)[0]==0x800000
    assert struct.unpack_from('<Q',obj,0x38)[0]==physical
    if limited:
        # Execute the real manager entry too. Only object allocation, virtual
        # options lookup and external PCI helpers are modeled; the native
        # manager passes its range to the real constructor on owned RAM.
        memory[gtt:gtt+0x800000]=prefix+b'\xA5'*(0x800000-len(prefix))
        manager=C.create_string_buffer(0x118);vt=C.create_string_buffer(0x130)
        calls=[];errors=[]
        @C.CFUNCTYPE(C.c_uint32,C.c_void_p,C.c_uint32)
        def options(this,kind):
            calls.append(('options',kind));return 0
        struct.pack_into('<Q',vt,0x128,C.cast(options,C.c_void_p).value)
        for off,value in [(0,C.addressof(vt)),(0x10,C.addressof(owner)),(0x18,mmio),(0xa8,0x4000000),(0xf0,physical)]:struct.pack_into('<Q',manager,off,value)
        @C.CFUNCTYPE(C.c_void_p,C.c_void_p,C.c_void_p,C.c_void_p,C.c_uint64,C.c_uint32)
        def construct(provider,ranges,backing,dma,flags):
            try:
                assert provider==C.addressof(owner) and backing==mmio and dma==physical and flags==0
                assert tuple((C.c_uint64*2).from_address(ranges))==(0,0xa00000)
                calls.append(('constructor',dma))
                return C.addressof(obj) if fn(obj,provider,ranges,backing,dma,flags) else 0
            except BaseException as e:errors.append(repr(e));return 0
        image[0x10140:0x1014e]=bytes.fromhex('ff2500000000')+struct.pack('<Q',C.cast(construct,C.c_void_p).value)
        for site,target in [(0xed30,0x190000),(0xed3d,0x190020)]:
            assert data[site:site+5]==b'\xe8\0\0\0\0'
            image[site+1:site+5]=struct.pack('<i',target-site-5)
        before=bytes(manager)
        struct.pack_into('<Q',manager,0xa8,0xa00000)
        try:assert C.CFUNCTYPE(C.c_bool,C.c_void_p)(base+0xecda)(manager)
        finally:struct.pack_into('<Q',manager,0xa8,0x4000000)
        assert not errors and calls==[('options',3),('constructor',physical)],(errors,calls)
        assert memory[gtt:gtt+0x800000]==expected
        assert struct.unpack_from('<Q',manager,0x98)[0]==C.addressof(obj)
        struct.pack_into('<Q',manager,0x98,0)
        assert bytes(manager)==before
    memory.close();image.close()
assert data[0xecf1:0xecf8]==bytes.fromhex('488db3a0000000') # manager passes its reserved range unchanged
print('PASS exact native GGTT RAM replay: negative-control reserved LM bit, protected firmware prefix, complete 39-bit dummy PTEs, exact one-page spare mapping, real manager range handoff/restoration, intact guard regions')
