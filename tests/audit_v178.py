#!/usr/bin/env python3
"""Focused v10.178 audit. Executes production C/PE/LE with synthetic services.

The probes check memory, state selection and emitted words, not GPU pixels.
Run after building the driver and tests/core.so. Requires Unicorn for x86.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path[:0] = [str(ROOT/'tests'), str(ROOT/'verification')]
import test_core as core
from d3d_callbacks import D3D, dx7
from native3d import NativeGPU
from engine import ARENA, FB, pack

ROWS = []
def check(name, actual, expected, group='fix'):
    ROWS.append(dict(name=name, actual=actual, expected=expected,
                     passed=actual == expected, group=group))

def fresh():
    t = core.Contract(); t.setUp(); return t

def linked():
    h = D3D(); dx7(h)
    h.u.mem_write(h.context+28, pack([32,0x40,0,32,0xff0000,0xff00,255,0]))
    h.surface(h.dst,h.dg,0x10000,256,64,48)
    q=ARENA+0xa000
    h.u.mem_write(q,pack([ARENA+0xb000,h.dst,0,h.pid,0,0]))
    h.call('_VX_D3D_Create@4',(q,))
    assert h.word(q+20)==0
    return h,h.word(q+16)

def can_create(h, caps, pf):
    d=ARENA+0x13000; q=d+0x100
    h.u.mem_write(d,bytes(108)); h.u.mem_write(d,pack([108,0x1006,8,8]))
    h.u.mem_write(d+72,pack(pf)); h.u.mem_write(d+104,pack([caps]))
    h.u.mem_write(q,pack([0,d,1,0,0])); h.call('_VXCanCreate@4',(q,))
    return h.word(q+12)==0

def format_checks():
    h,_=linked()
    z16=[32,0x400,0,16,0,0xffff,0,0]
    x555=[32,0x40,0,16,0x7c00,0x3e0,31,0]
    for name,caps,pf,want in [
        ('D16 cannot become a texture',0x5000,z16,False),
        ('X1R5G5B5 cannot become a Z buffer',0x24000,x555,False),
        ('D16 Z creation still succeeds',0x24000,z16,True),
        ('X1R5G5B5 texture creation still succeeds',0x5000,x555,True)]:
        check(name,can_create(h,caps,pf),want)

def clear_packet(t,fmt):
    p=core.Packet(); p.cb=C.sizeof(p); p.abi=0x10000; p.arm=0x33445856
    p.op=2;p.generation=7;p.target=t.target;p.depth=core.Surface(0x10000,256,64,64,fmt)
    p.clip=core.Rect(1,1,4,3);p.z_func=1;p.write_mask=15
    p.clear_flags=2;p.clear_depth=0x12345678
    return p

def clear_checks():
    t=fresh(); p=clear_packet(t,13); mem={}
    for y in range(64):
        for x in range(64):mem[0x10000+y*256+x*4]=0xaabbcc00|((x+3*y+7)&255)
    before=dict(mem);writes=[]
    def put(_,at,value):writes.append(at);mem[at]=value
    refs=(core.READ(lambda _,r:0),core.WRITE(lambda _,r,v:None),
          core.READ(lambda _,at:mem[at]),core.WRITE(put),core.TIME(lambda _:0))
    rc=core.lib.vx3dn_execute(C.byref(p),C.byref(core.IO(None,*refs)),C.byref(core.U()))
    check('CPU D24S8 Z clear preserves stencil',
          [rc,all(mem[a]&255==before[a]&255 for a in writes)], [0,True])
    check('CPU D24S8 clear writes requested depth',
          all(mem[a]&0xffffff00==0x12345600 for a in writes), True)
    check('CPU D24S8 clear stays inside rectangle',
          sorted(writes),sorted(0x10000+y*256+x*4 for y in range(1,3) for x in range(1,4)))
    for enabled in (0,1):
        g=NativeGPU(32);g.u.mem_write(g.addr('vx3dn_clear2d'),pack([enabled]))
        p.generation=1;p.target.height=48;p.depth.height=48
        p.clip=core.Rect(0,0,64,48);p.clear_depth=0xffffff00
        original=pack([0x1122336b]*(64*48));g.u.mem_write(FB+0x10000,original)
        rc=g.submit(bytes(p));data=bytes(g.u.mem_read(FB+0x10000,len(original)))
        check(f'linked D24S8 Z clear preserves stencil (2D enabled={enabled})',
              [rc,data==pack([0xffffff6b]*(64*48))], [0,True])
        # v10.179: with 2D clears enabled a partial D24S8 clear may run as two ROP fills (A0 then FA) instead of the CPU; the
        # preserved stencil above is the requirement, not which path did it.

def textured(t,mag=1,min_=1):
    s=t.context.state.tss[0]
    s[0]=5;s[1]=2;s[2]=2;s[4]=2;s[5]=2;s[16]=mag;s[17]=min_
    v=b''.join(struct.pack('<4fI2f',x,y,.5,1.,0x40c08020,u,v)
               for x,y,u,v in [(8,8,0.,0.),(56,8,1.,0.),(8,56,0.,1.)])
    rc=t.run_dp2(t.tri(),v,stride=28,fvf=0x144)[0]
    assert rc==0 and len(t.calls)==1
    return t.calls[0]

def filter_checks():
    for mag,min_ in [(1,1),(2,2),(2,1),(1,2)]:
        t=fresh();p=textured(t,mag,min_);trace=[]
        refs=(core.READ(lambda _,r:0),core.WRITE(lambda _,r,v:trace.append((r,v))),
              core.READ(lambda _,r:0),core.WRITE(lambda _,r,v:None),core.TIME(lambda _:0))
        core.lib.vx3dn_execute(C.byref(p),C.byref(core.IO(None,*refs)),C.byref(core.U()))
        bank=None;word=None
        for r,v in trace:
            if r==0x43c:bank=v
            elif bank==0x20020000 and v>>24==0x31:word=v&0xffffff
        want=0x10000|(0x2080 if mag==2 else 0)|(0x410 if min_==2 else 0)
        check(f'MAG={mag} MIN={min_} independent sampler fields',word,want)
        check(f'MAG={mag} MIN={min_} native packet validates',
              core.lib.vx3dn_validate(C.byref(p),C.byref(t.info)),0)

def target_format_checks():
    for fmt,pitch,want in [(1,256,0x980008),(2,128,0x890004)]:
        t=fresh();assert t.run_dp2(t.tri())[0]==0;p=t.calls[0]
        p.target=core.Surface(0x10000,pitch,64,64,fmt);trace=[]
        refs=(core.READ(lambda _,r:0),core.WRITE(lambda _,r,v:trace.append((r,v))),
              core.READ(lambda _,r:0),core.WRITE(lambda _,r,v:None),core.TIME(lambda _:0))
        core.lib.vx3dn_execute(C.byref(p),C.byref(core.IO(None,*refs)),C.byref(core.U()))
        bank=None;word=None
        for r,v in trace:
            if r==0x43c:bank=v
            elif bank==0x10000 and v>>24==0x52:word=v&0xffffff
        check('XRGB target uses no-alpha format' if fmt==1 else 'RGB565 target format stays intact',word,want)

def surface_checks():
    for kind in ('valid flip','invalid flag','below aperture','misaligned','past aperture'):
        h,ctx=linked()
        if kind=='valid flip':h.u.mem_write(h.dg+20,pack([FB+0x20000]))
        elif kind=='invalid flag':h.u.mem_write(h.dst+28,pack([0x10000000]))
        elif kind=='below aperture':h.u.mem_write(h.dg+20,pack([FB-256]))
        elif kind=='misaligned':h.u.mem_write(h.dg+20,pack([FB+0x20001]))
        else:h.u.mem_write(h.dg+20,pack([FB+0x1000000]))
        q=ARENA+0x14000
        h.u.mem_write(q,pack([ctx,1,0xff123456,0,0,0,0]))
        h.call('_VX_D3D_Clear@4',(q,))
        want=kind=='valid flip'
        check('object-bound target: '+kind,
              [h.word(q+24),len(h.native)], [0,1] if want else [core.LOST,0])
        if want:check('valid flip follows new memory',struct.unpack_from('<I',h.native[0],20)[0],0x20000)

def legacy_checks():
    # Offset wraps onto readable mapped RAM. IsBadReadPtr is stubbed to
    # readable by the existing harness; the missing arithmetic guard is real.
    h,ctx=linked();buf=ARENA+0x16000;g=buf+0x100;q=buf+0x200
    base=0xfff00000;payload=ARENA+0x16400
    h.surface(buf,g,0,64,8,8);h.u.mem_write(g+20,pack([base]))
    h.u.mem_write(payload,pack([22,2]))
    h.u.mem_write(q,pack([ctx,(payload-base)&0xffffffff,1,buf,0]))
    h.call('_VX_D3D_RenderState@4',(q,))
    check('legacy RenderState rejects wrapped base+offset',h.word(q+16),core.INVALID)
    get=q+0x80;h.u.mem_write(get,pack([ctx,4,22,0,0]));h.call('_VX_D3D_GetState@4',(get,))
    check('wrapped legacy state never mutates cull mode',h.word(get+12),3)
    for kind in ('triangle indices','TL vertices'):
        h,ctx=linked();eb=ARENA+0x18000;eg=eb+0x100;tb=eb+0x200;tg=eb+0x300
        q=eb+0x400;indices=eb+0x500;vertices=eb+0x600
        h.surface(eb,eg,0,64,8,8);h.surface(tb,tg,0,64,8,8)
        h.u.mem_write(indices,struct.pack('<4H',0,1,2,0))
        h.u.mem_write(vertices,b''.join(struct.pack('<4fII2f',x,y,.5,1.,0xffabcdef,0,0.,0.)
                                       for x,y in [(8,8),(56,8),(8,40)]))
        h.u.mem_write(eg+20,pack([base if kind=='triangle indices' else indices]))
        h.u.mem_write(tg+20,pack([base if kind=='TL vertices' else vertices]))
        offset=(indices-base)&0xffffffff if kind=='triangle indices' else 0
        tl_offset=(vertices-base)&0xffffffff if kind=='TL vertices' else 0
        h.u.mem_write(q,pack([ctx,offset,0,eb,tl_offset,tb])+struct.pack('<BBH',3,8,1)+pack([0]))
        h.call('_VX_D3D_RenderPrimitive@4',(q,))
        check('legacy rejects wrapped '+kind,[h.word(q+28),len(h.native)],[core.INVALID,0])

def null_texture_checks():
    for kind in ('factor argument','unused alpha state','unused next stage'):
        t=fresh();s=t.context.state.tss[0];s[0]=0;s[1]=4;s[2]=2
        if kind=='factor argument':s[3]=3
        elif kind=='unused alpha state':s[4]=2;s[5]=3
        else:t.context.state.tss[1][1]=4
        check('NULL COLORARG1 TEXTURE disables '+kind,
              core.lib.vx_d3d_texture_status(C.byref(t.context)),0)
        rc=t.run_dp2(t.tri())[0]
        check('NULL texture still draws diffuse: '+kind,
              [rc,len(t.calls),t.calls[0].tex.enable if t.calls else None],[0,1,0])

def unresolved_checks():
    # Exact captured programs have independently documented meanings in
    # vx_xp_programs.h / cookbook 16.3. Choosing these for another expression
    # demonstrates a state-selection bug; this is not a GPU pixel simulation.
    core.lib.vx_d3d_program.argtypes=[C.POINTER(core.Context),C.c_int,C.POINTER(core.U)]
    core.lib.vx_d3d_program.restype=C.c_int
    for name,op,a,b,aop,aa,ab,spec,bad_program in [
        ('MODULATE(T,T) wrongly selects captured MODULATE(T,D)',4,2,2,2,2,1,0,3),
        ('ADD(D,D) wrongly selects diffuse passthrough',7,0,0,2,0,1,0,-1),
        ('SELECT T with MODULATE alpha wrongly selects texture alpha',2,2,1,4,2,0,0,0),
        ('MODULATE RGB with diffuse alpha wrongly selects texture alpha',4,2,0,2,0,1,0,3),
        ('reversed BLENDTEXTUREALPHA wrongly selects diffuse passthrough',13,0,2,2,0,1,0,-1),
        ('SPECULARENABLE ignored with SELECT texture',2,2,1,2,2,1,1,0),
    ]:
        t=fresh();s=t.context.state.tss[0];s[0]=5;s[1]=op;s[2]=a;s[3]=b
        s[4]=aop;s[5]=aa;s[6]=ab;t.context.state.rs[29]=spec
        e=core.U();p=core.lib.vx_d3d_program(C.byref(t.context),1,C.byref(e))
        check(name,p==bad_program and e.value==0,False,group='unresolved')
        ROWS[-1].update(selected_program=p,error=e.value)
    t=fresh();t.context.depth=core.Surface(0x10000,256,64,64,13)
    stream=core.cmd(42,1,core.pack(6,0,core.bits(.5),0x6b)+core.pack(0,0,64,64))
    rc=t.run_dp2(stream)[0]
    check('advertised D24S8 accepts a combined Z+STENCIL clear',
          [rc,len(t.calls)],[0,1],group='unresolved')

def main():
    a=argparse.ArgumentParser();a.add_argument('--output',type=Path,required=True)
    a.add_argument('--fixes-only',action='store_true',help='omit the separately reported open combiner defects')
    args=a.parse_args()
    for fn in (format_checks,clear_checks,filter_checks,target_format_checks,surface_checks,legacy_checks,null_texture_checks):fn()
    if not args.fixes_only:unresolved_checks()
    names=['src/vx_d3d_core.c','src/vx_d3d_hal.c','src/vx_d3d_hal.h',
           'src/vx_d3d_legacy.inc','src/vx_native3d.h','src/vx_native3d_hw.c','src/vx_native3d_vxd.c']
    result=dict(scope=__doc__,checks=ROWS,
                hashes={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest() for n in names},
                binaries={n:hashlib.sha256((ROOT/n).read_bytes()).hexdigest()
                          for n in ['tests/core.so','driver/VXDDRAW.DLL','driver/VX85598.VXD']})
    args.output.write_text(json.dumps(result,indent=2)+'\n')
    for group in ('fix','unresolved'):
        rows=[r for r in ROWS if r['group']==group]
        print(f"{group}: {sum(r['passed'] for r in rows)}/{len(rows)} pass")
        for r in rows:
            if not r['passed']:print('FAIL',r['name'],r['actual'],'expected',r['expected'])
    return int(any(not r['passed'] for r in ROWS))

if __name__=='__main__':raise SystemExit(main())
