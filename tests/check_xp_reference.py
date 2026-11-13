#!/usr/bin/env python3
"""Bounded CPU-only checks of supplied XP code. No driver loading or hardware IO."""
from pathlib import Path
import hashlib
import json
import struct
import argparse

class PE:
    def __init__(self, path):
        self.path = Path(path)
        self.b = self.path.read_bytes()
        b = self.b
        assert b[:2] == b'MZ'
        self.pe = self.u32(60)
        assert b[self.pe:self.pe+4] == b'PE\0\0'
        self.opt = self.pe+24
        assert self.u16(self.opt) == 0x10b
        self.base = self.u32(self.opt+28)
        self.size = self.u32(self.opt+56)
        self.entry = self.u32(self.opt+16)
        self.sections = []
        for i in range(self.u16(self.pe+6)):
            p = self.opt+self.u16(self.pe+20)+40*i
            vs, va, size, raw = struct.unpack_from('<4I', b, p+8)
            self.sections.append(dict(name=b[p:p+8].rstrip(b'\0').decode(),
                                      va=self.base+va, virtual_size=vs, raw_size=size, offset=raw))
    def u16(self, at): return struct.unpack_from('<H', self.b, at)[0]
    def u32(self, at): return struct.unpack_from('<I', self.b, at)[0]
    def offset(self, va):
        if self.base <= va < self.base+self.u32(self.opt+60): return va-self.base
        for s in self.sections:
            if s['va'] <= va < s['va']+s['raw_size']: return s['offset']+va-s['va']
        raise ValueError(hex(va))
    def read(self, va, n):
        at=self.offset(va)
        assert 0 <= at <= len(self.b)-n
        return self.b[at:at+n]
    def string(self, rva):
        at=self.offset(self.base+rva)
        return self.b[at:self.b.index(0,at)].decode('ascii')
    def metadata(self):
        imports=[]
        rva=self.u32(self.opt+104)
        if rva:
            at=self.offset(self.base+rva)
            while any(self.b[at:at+20]):
                orig,_,_,name,first=struct.unpack_from('<5I',self.b,at)
                thunk=self.offset(self.base+(orig or first));names=[]
                while self.u32(thunk):
                    v=self.u32(thunk)
                    names.append('#'+str(v&65535) if v&0x80000000 else self.string(v+2))
                    thunk+=4
                imports.append({'dll':self.string(name),'names':names});at+=20
        export_rva=self.u32(self.opt+96);exports=[]
        if export_rva:
            at=self.offset(self.base+export_rva)
            _,_,_,_,_,ordinal,count,names,functions,table,ordtable=struct.unpack_from('<IIHH7I',self.b,at)
            for i in range(names):
                name=self.string(self.u32(self.offset(self.base+table)+4*i))
                idx=self.u16(self.offset(self.base+ordtable)+2*i)
                function=self.u32(self.offset(self.base+functions)+4*idx)
                exports.append({'name':name,'ordinal':ordinal+idx,'va':hex(self.base+function)})
        signature=struct.pack('<I',0xfeef04bd);v=self.b.find(signature)
        version=None
        if v>=0:
            ms,ls=struct.unpack_from('<II',self.b,v+8)
            version='.'.join(str(x) for x in [ms>>16,ms&65535,ls>>16,ls&65535])
        return {'filename':self.path.name,'sha256':hashlib.sha256(self.b).hexdigest(),
                'size':len(self.b),'base':hex(self.base),'entry_rva':hex(self.entry),
                'subsystem':self.u16(self.opt+68),'file_version':version,
                'sections':self.sections,'imports':imports,'exports':exports}

class XP:
    def __init__(self, image):
        from unicorn import Uc, UC_ARCH_X86, UC_MODE_32
        self.image=image
        u=self.u=Uc(UC_ARCH_X86,UC_MODE_32)
        u.mem_map(image.base,(image.size+4095)&~4095)
        u.mem_write(image.base,image.b[:image.u32(image.opt+60)])
        for s in image.sections:
            if s['raw_size']:u.mem_write(s['va'],image.b[s['offset']:s['offset']+s['raw_size']])
        u.mem_map(0x02000000,0x200000)
        u.mem_map(0x03000000,0x10000)
        from unicorn.x86_const import UC_X86_REG_ESP
        u.reg_write(UC_X86_REG_ESP,0x021f0000)
    def put(self, at, value):self.u.mem_write(at,struct.pack('<I',value))
    def get(self, at):return struct.unpack('<I',self.u.mem_read(at,4))[0]
    def run(self, start, end, registers):
        from unicorn.x86_const import UC_X86_REG_EIP
        for reg,value in registers.items():self.u.reg_write(reg,value)
        self.u.emu_start(start,end,count=100000)
        assert self.u.reg_read(UC_X86_REG_EIP)==end,hex(self.u.reg_read(UC_X86_REG_EIP))

def checks(path):
    from unicorn import UC_HOOK_MEM_WRITE
    from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_EDI
    image=PE(path)
    assert hashlib.sha256(image.b).hexdigest()=='fec5211548bc2e1100efa8de55b89407717c14997d78e0d3eed333d62a1ca532', 'This probe is pinned to the supplied XP DLL'
    xp=XP(image);u=xp.u;ctx=0x02000000;obj=ctx+0x10000;chip=obj+0x1000;mmio=0x03000000
    xp.put(ctx+0x244,obj);xp.put(obj+0xc,chip);xp.put(chip+0xf4,mmio)
    trace=[]
    u.hook_add(UC_HOOK_MEM_WRITE,lambda u,a,p,size,value,data:trace.append((p-mmio,value)),begin=mmio,end=mmio+0x500)
    xp.run(0x110b27,0x110e77,{UC_X86_REG_EAX:ctx,UC_X86_REG_ECX:0x5122})
    assert trace[0]==(0x43c,0xfe0000) and len(trace)==17
    fe=[v for p,v in trace[1:]];assert all(p==0x440 for p,v in trace[1:])
    xp=XP(image);surface=0x02020000;shadow=0x02040000
    xp.put(surface+0x14c,256);xp.put(shadow+0x44,0x11000000)
    xp.run(0xd2da6,0xd2dcf,{UC_X86_REG_EAX:4,UC_X86_REG_ECX:0x12000000,
                           UC_X86_REG_EBP:surface,UC_X86_REG_ESI:shadow})
    d32=xp.get(shadow+0x48);assert d32==0x123c0000
    pitches=[]
    for pitch in (32,256,0x7fe0,0x8000):
        xp=XP(image);xp.put(shadow+0x148,0x52990000)
        xp.run(0xd290e,0xd2929,{UC_X86_REG_ECX:pitch,UC_X86_REG_EDI:0,UC_X86_REG_ESI:shadow})
        rt=xp.get(shadow+0x148)
        xp=XP(image);xp.put(surface+0x14c,pitch);xp.put(shadow+0x44,0x11000000)
        xp.run(0xd2da6,0xd2dcf,{UC_X86_REG_EAX:4,UC_X86_REG_ECX:0x12000000,
                               UC_X86_REG_EBP:surface,UC_X86_REG_ESI:shadow})
        depth=xp.get(shadow+0x44)
        pitches.append({'byte_pitch':pitch,'target_word':hex(rt),'depth_word':hex(depth)})
    compares=[]
    for function in range(1,9):
        xp=XP(image);xp.put(ctx+0x8c40,shadow);xp.put(ctx+0x8c74,obj)
        xp.put(ctx+0x1e94,function);xp.put(shadow+0x4c,0x13000000)
        # Stop before the dirty-state housekeeping; this is the actual mapping.
        xp.put(0x021f0004,ctx)
        xp.run(0xd3290,0xd32c3,{})
        word=xp.get(shadow+0x4c);assert word==0x13000000|((function-1)<<16)
        compares.append(word)
    xp=XP(image)
    xp.run(0xc1880,0xc194d,{UC_X86_REG_EAX:shadow,UC_X86_REG_EDI:shadow+0x20e0})
    defaults={'enable_01':hex(xp.get(shadow+4))}
    assert defaults['enable_01']=='0x1ce1540'
    # Replay the depth mask/window default block, including its split-byte writes.
    xp.run(0xc198f,0xc19d7,{UC_X86_REG_EAX:shadow,UC_X86_REG_EBX:0x800000})
    defaults.update({f'{i:02x}':hex(xp.get(shadow+4*i)) for i in [0x14,0x15,0x19,0x1a,0x1b]})
    xp=XP(image)
    for i in [0x34,0x35,0x36,0x37]:xp.put(shadow+4*i,i<<24)
    xp.run(0xd3979,0xd39c8,{UC_X86_REG_EAX:shadow})
    blend={f'{i:02x}':hex(xp.get(shadow+4*i)) for i in [0x34,0x35,0x36,0x37]}
    xp=XP(image)
    xp.run(0xc1880,0xc194d,{UC_X86_REG_EAX:shadow,UC_X86_REG_EDI:shadow+0x20e0})
    xp.run(0xc1c51,0xc1cf3,{UC_X86_REG_EAX:shadow,UC_X86_REG_EDI:shadow+0x20e0,UC_X86_REG_EBX:0x7f})
    vertex_defaults={f'{i:02x}':hex(xp.get(shadow+0x20e0+4*i)) for i in [0,0x2f,0x50,0x51,0x52]}
    formats={}
    for fmt in [0x1018,0x2030,0x2031]:
        xp=XP(image);xp.put(surface+0x3c,fmt);xp.put(0x021f0000,0x03000000);xp.put(0x021f0004,surface)
        xp.run(0xd75f0,0x03000000,{})
        formats[hex(fmt)]=hex(xp.u.reg_read(UC_X86_REG_EAX))
    return {'metadata':image.metadata(), 'fe_5122_trace':trace, 'fe_words':fe,
            'd32_format_word':hex(d32),'z_compare_words':[hex(x) for x in compares],
            'pitch_field_probes':pitches,'defaults':defaults,'blend_baseline':blend,'target_format_ids':formats,
            'vertex_defaults':vertex_defaults,
            'fragments':{'front_end':['0x110b27','0x110e77'],'d32':['0xd2da6','0xd2dcf'],
                         'target_pitch':['0xd290e','0xd2929'],'z_compare':['0xd3290','0xd32c3'],
                         'common_enable':['0xc1880','0xc194d'],'depth_defaults':['0xc198f','0xc19d7'],
                         'blend_baseline':['0xd3979','0xd39c8'],'format_function':'0xd75f0',
                         'vertex_defaults':['0xc1c51','0xc1cf3']},
            'scope':'Bounded XP x86 fragments with synthetic RAM; not XP runtime or physical GPU execution.'}

def compare_source(root, reference):
    """Compare actual compiled portable emitter output with XP-derived words."""
    import ctypes as C
    import subprocess
    import tempfile
    U=C.c_uint32;I=C.c_int32;P=C.c_void_p
    class Surface(C.Structure):_fields_=[(n,U) for n in ('offset','pitch','width','height','format')]
    class Rect(C.Structure):_fields_=[(n,I) for n in ('left','top','right','bottom')]
    class Vertex(C.Structure):_fields_=[(n,U) for n in ('x','y','z','rhw','color')]
    class Packet(C.Structure):_fields_=[(n,U) for n in ('cb','abi','arm','op','generation')]+[('target',Surface),('depth',Surface),('clip',Rect)]+[(n,U) for n in ('z_enable','z_write','z_func','blend','write_mask','clear_flags','clear_color','clear_depth','vertex_count')]+[('vertices',Vertex*48)]
    class Info(C.Structure):_fields_=[(n,U) for n in ('cb','abi','ready','generation','fb_bytes','primary_bytes','width','height','pitch','bpp','submitted','triangles','clears','fault','last_status','last_result')]
    READ=C.CFUNCTYPE(U,P,U);WRITE=C.CFUNCTYPE(None,P,U,U);TIME=C.CFUNCTYPE(U,P)
    class IO(C.Structure):_fields_=[('context',P),('read',READ),('write',WRITE),('read_fb',READ),('write_fb',WRITE),('milliseconds',TIME)]
    q=Packet();q.cb=C.sizeof(q);q.abi=0x10000;q.arm=0x33445856;q.op=1;q.generation=1
    q.target=Surface(0,256,64,64,1);q.depth=Surface(0x10000,256,64,64,3);q.clip=Rect(0,0,64,64)
    q.z_enable=1;q.z_write=1;q.z_func=2;q.write_mask=15;q.vertex_count=3
    for i,(x,y) in enumerate([(8.,8.),(56.,8.),(8.,56.)]):
        q.vertices[i]=Vertex(*(struct.unpack('<I',struct.pack('<f',v))[0] for v in [x,y,.5,1.]),0xff112233)
    trace=[]
    read=READ(lambda ctx,at:0);write=WRITE(lambda ctx,at,value:trace.append((at,value)))
    tick=TIME(lambda ctx:0);fb_write=WRITE(lambda ctx,at,value:None)
    io=IO(None,read,write,read,fb_write,tick)
    with tempfile.TemporaryDirectory(prefix='vx855-xp-emitter-') as tmp:
        so=Path(tmp)/'core.so'
        subprocess.run(['gcc','-std=c99','-shared','-fPIC','-O2',str(root/'src/vx_native3d_hw.c'),'-o',str(so)],check=True)
        lib=C.CDLL(str(so));lib.vx3dn_execute.argtypes=[C.POINTER(Packet),C.POINTER(IO),C.POINTER(U)]
        assert lib.vx3dn_execute(C.byref(q),C.byref(io),C.byref(U()))==0
    banks={};fe=[];bank=0
    for at,value in trace:
        if at==0x43c:bank=value>>16
        elif at==0x440:
            if bank==0xfe:fe.append(value)
            if bank in [1,4]:banks[bank,value>>24]=value
    checks={
        'front_end_16_words':fe==reference['fe_words'],
        'd32_format_and_endpoint_clamps':banks[1,0x12]==int(reference['d32_format_word'],16),
        'z_compare_less':banks[1,0x13]==int(reference['z_compare_words'][1],16),
        'enabled_state':banks[1,1]==int(reference['defaults']['enable_01'],16)|12,
        'xrgb_target_format':((banks[1,0x52]>>16)&255)==int(reference['target_format_ids']['0x2031'],16),
    }
    for i in [0x14,0x15,0x19,0x1a,0x1b]:checks[f'depth_default_{i:02x}']=banks[1,i]==int(reference['defaults'][f'{i:02x}'],16)
    for i in [0x2f,0x50,0x51,0x52]:checks[f'vertex_default_{i:02x}']=banks[4,i]==int(reference['vertex_defaults'][f'{i:02x}'],16)
    for function in range(1,9):
        q.z_func=function;trace.clear()
        assert lib.vx3dn_execute(C.byref(q),C.byref(io),C.byref(U()))==0
        bank=0;word=None
        for at,value in trace:
            if at==0x43c:bank=value>>16
            elif at==0x440 and bank==1 and value>>24==0x13:word=value
        checks[f'z_compare_{function}']=word==int(reference['z_compare_words'][function-1],16)
    lib.vx3dn_validate.argtypes=[C.POINTER(Packet),C.POINTER(Info)]
    info=Info(C.sizeof(Info),0x10000,1,1,0x1000000,256*64,64,64,256,32)
    for pitch in [0x7fe0,0x8000]:
        q.z_enable=0;q.target=Surface(0x10000,pitch,64,64,1)
        checks[f'target_pitch_{pitch:#x}']=lib.vx3dn_validate(C.byref(q),C.byref(info))==(0 if pitch<=0x7fe0 else 1)
        q.z_enable=1;q.target=Surface(0,256,64,64,1);q.depth=Surface(0x10000,pitch,64,64,3)
        checks[f'depth_pitch_{pitch:#x}']=lib.vx3dn_validate(C.byref(q),C.byref(info))==(0 if pitch<=0x7fe0 else 1)
    return {'checks':checks,'passed':all(checks.values()),
            'source_sha256':hashlib.sha256((root/'src/vx_native3d_hw.c').read_bytes()).hexdigest(),
            'emitter_d32_word':hex(banks[1,0x12]),'xp_d32_word':reference['d32_format_word']}

def display_checks(path):
    from unicorn.x86_const import UC_X86_REG_EAX,UC_X86_REG_EBP
    image=PE(path)
    assert hashlib.sha256(image.b).hexdigest()=='ab4352c003301930ec9f8c3cdbe789f7ba0d2bf69ef20ebd61ac5fe424966859', 'Display probe is pinned to the supplied DLL'
    xp=XP(image);frame=0x02070000;device=0x02090000
    xp.put(frame-0xac,0x5122);xp.put(frame-4,device)
    xp.run(0x22dc3,0x22e52,{UC_X86_REG_EBP:frame})
    chip=xp.get(frame-0xb8);assert chip==13
    xp.run(0x22ed4,0x22ee5,{UC_X86_REG_EBP:frame})
    address=xp.u.reg_read(UC_X86_REG_EAX)
    name=bytes(xp.u.mem_read(address,32)).split(b'\0')[0].decode('ascii')
    assert name=='s3ginv.dll'
    return {'metadata':image.metadata(),'chip_5122_class':chip,'selected_backend':name,
            'selection_va':['0x22dc3','0x22e52'],'filename_va':['0x22ed4','0x22ee5']}

if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('--s3ginv',type=Path,required=True);p.add_argument('--display',type=Path);p.add_argument('--output',type=Path,required=True);p.add_argument('--source-root',type=Path);a=p.parse_args()
    result=checks(a.s3ginv)
    if a.display:result['display']=display_checks(a.display)
    if a.source_root:result['source_comparison']=compare_source(a.source_root.resolve(),result)
    a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n')
    print('XP 0x5122 front-end: 16 words; D32 clamp/format: '+result['d32_format_word']+'; depth compare functions: 8 matched.')
    if 'source_comparison' in result:
        print('Compiled emitter matches: '+str(result['source_comparison']['checks']))
        raise SystemExit(0 if result['source_comparison']['passed'] else 1)
