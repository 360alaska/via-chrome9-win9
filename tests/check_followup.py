#!/usr/bin/env python3
"""Focused D3D regression probes of real portable C and linked callbacks.

Synthetic services do not rasterize 3D or establish Win98 runtime acceptance.
The output records observed/expected values so the same probes can capture
the uncorrected and corrected source without editing the test expectations.
"""
import argparse
import ctypes as C
import hashlib
import json
from pathlib import Path
import struct
import sys
ROOT=Path(__file__).resolve().parents[1]
sys.path[:0]=[str(ROOT/'tests'),str(ROOT/'verification')]
import test_core as core
from d3d_callbacks import D3D,dx7
from engine import ARENA,FB,pack

INVALID=0x80070057
LOST=0x887601c2
BUSY=0x8876021c

def checks():
 rows=[]
 def record(name,observed,expected):
  rows.append({'name':name,'observed':observed,'expected':expected,'passed':observed==expected})
 def portable():
  t=core.Contract();t.setUp();return t
 for flags in (1,9):
  t=portable();stream=core.cmd(42,1,core.pack(flags,0xff123456,0x7fc00000,0)+core.pack(0,0,64,64))
  rc,at=t.run_dp2(stream)
  record(f'color-only clear flags {flags} ignores unused NaN depth',
         [rc,len(t.calls),t.calls[0].clear_depth if t.calls else None],[0,1,0])
 t=portable();stream=core.cmd(42,1,core.pack(3,0xff123456,0x7fc00000,0)+core.pack(0,0,64,64))
 record('a depth clear without a depth surface clears the colour only (v10.372, as XP; the NaN depth is unused)',[t.run_dp2(stream)[0],len(t.calls),t.calls[0].clear_flags&7 if t.calls else None],[0,1,1])
 for result,expected in ((1,INVALID),(2,0x80004001),(3,BUSY),(4,LOST),(5,LOST)):
  t=portable();t.result=result;rc=t.run_dp2(t.tri())[0]
  record(f'first native result {result} maps its error and loss state',[rc,t.context.lost],[expected,int(result in (4,5))])
 for label,stream_fn in [('two triangles in one token',lambda t:t.tri(19,2)),
                         ('two consecutive tokens',lambda t:t.tri()+t.tri())]:
  t=portable()
  def send(_,p):
   t.calls.append(core.Packet.from_buffer_copy(p.contents));return 0 if len(t.calls)==1 else 3
  cb=core.SUBMIT(send);t.services.submit=cb
  rc=t.run_dp2(stream_fn(t))[0];after=t.run_dp2(t.tri())[0]
  record('busy after progress: '+label,[rc,t.context.lost,after,len(t.calls)],[LOST,1,LOST,2])
 for label,change in [('undersized depth',lambda t:setattr(t.depth,'width',32)),
                       ('overlapping buffers',lambda t:setattr(t.depth,'offset',0))]:
  t=portable();change(t);old=(t.context.target_handle,t.context.depth_handle)
  rc=t.run_dp2(core.cmd(41,1,core.pack(3,4)))[0]
  record('DP2 target binding rejects '+label,[rc,t.context.target_handle,t.context.depth_handle],[INVALID,*old])

 def linked():
  h=D3D();u=h.u;dx7(h)
  u.mem_write(h.context+28,pack([32,0x40,0,32,0xff0000,0xff00,255,0]))
  h.surface(h.dst,h.dg,0x10000,256,64,48)
  create=ARENA+0xa000;local=ARENA+0xb000
  def make(z=0):
   u.mem_write(create,pack([local,h.dst,z,h.pid,0,0]));h.call('_VX_D3D_Create@4',(create,))
   return h.word(create+16),h.word(create+20)
  ctx,hr=make();assert ctx and hr==0
  def query(d1,d2,d3,d4,size):
   q=ARENA+0xc000;out=q+0x100
   u.mem_write(q,pack([44,0])+struct.pack('<IHH8B',d1,d2,d3,*d4)+pack([size,out,0,0,0]))
   h.call('_VX_D3D_GetDriverInfo@4',(q,));assert h.word(q+36)==0
   return out
  out=query(0x406b2f00,0x3e5a,0x11d1,[0xb6,0x40,0,0xaa,0,0xa1,0xf9,0x6a],24)
  h.sym['audit_create_ex']=h.word(out+12)
  out=query(0x0ba584e1,0x70b6,0x11d0,[0x88,0x9d,0,0xaa,0,0xbb,0xb7,0x6a],28)
  h.sym['audit_setrt']=h.word(out+8)
  def alive():
   q=ARENA+0xd000;u.mem_write(q,pack([ctx,4,22,0,0]));h.call('_VX_D3D_GetState@4',(q,));return h.word(q+16)
  def register():
   m=ARENA+0xd100;u.mem_write(m,bytes(256));u.mem_write(h.dst,pack([m]))
   u.mem_write(m+h.word(h.sym['VXDD3DABI']+44),pack([700]))
   q=ARENA+0xd300;u.mem_write(q,pack([0,local,h.dst,0]));h.call('audit_create_ex',(q,));return h.word(q+12)
  def depth(width=64,offset=0x30000):
   l=ARENA+0xe000;g=l+0x80;h.surface(l,g,offset,256,width,48)
   u.mem_write(l+28,pack([0x2000]));u.mem_write(l+32,pack([0x24000]))
   u.mem_write(g+40,pack([32,0x400,0,32,0,0xffffffff,0,0]));return l
  def setrt(z=0):
   q=ARENA+0xe200;u.mem_write(q,pack([ctx,h.dst,z,0]));h.call('audit_setrt',(q,));return h.word(q+12)
  return h,make,alive,register,depth,setrt,ctx
 for changed in (False,True):
  h,make,alive,register,depth,setrt,ctx=linked()
  if changed:h.u.mem_write(h.dg+20,pack([FB+0x20000]))
  record('first registration '+('relocation loses the old direct context' if changed else 'of an unchanged descriptor preserves the context'),
         [register(),alive()],[0,LOST if changed else 0])
 for width,offset,label in [(32,0x30000,'undersized depth'),(64,0x10000,'overlap')]:
  h,make,alive,register,depth,setrt,ctx=linked();z=depth(width,offset)
  new,hr=make(z);record('ContextCreate rejects '+label,[new,hr],[0,INVALID])
  record('legacy SetRenderTarget rejects '+label,[setrt(z),alive()],[INVALID,0])
 h,make,alive,register,depth,setrt,ctx=linked();h.generation=2
 record('legacy target change rejects a stale mode generation',setrt(),LOST)
 for label,offset,limit in [('a primary alias',0x100,0x1000000),
                            ('native-reserved memory',0x14000,0x16000)]:
  h,make,alive,register,depth,setrt,ctx=linked()
  h.surface(h.dst,h.dg,offset,256,64,48);h.fb_bytes=limit
  new,hr=make();record('ContextCreate rejects '+label,[new,hr],[0,INVALID])
  record('legacy target change rejects '+label,[setrt(),alive()],[INVALID,0])
 # The old overlay-off setup used to clear DestroySurface after installing
 # the D3D hook. Check the actual callback slot offered to the Win16 bridge.
 h,make,alive,register,depth,setrt,ctx=linked()
 enabled=h.word(h.context+19160)!=0;destroy=h.word(h.context+616+3*4)
 record('surface-destruction hook is retained when D3D is advertised',
        bool(destroy) if enabled else True,True)
 # v10.218: a busy VxD (nothing drawn) is waited out by the HAL; only a busy through 601 attempts reaches the core
 for results,rects,expected,label in [([4],0,LOST,'native loss'),([3]*601,0,BUSY,'busy before progress'),
                                     ([0]+[3]*601,2,LOST,'busy after a first rectangle'),([3]*5,0,0,'a short busy, waited out'),
                                     ([0,3,3],2,0,'a short busy after a first rectangle, waited out')]:
  h,make,alive,register,depth,setrt,ctx=linked();h.native_results=list(results)
  clear=ARENA+0xf000;r=clear+0x100
  h.u.mem_write(r,pack([0,0,8,8,8,8,16,16]))
  h.u.mem_write(clear,pack([ctx,1,0xff123456,0,r if rects else 0,rects,0]))
  h.call('_VX_D3D_Clear@4',(clear,))
  record('legacy Clear contains '+label,[h.word(clear+24),alive()],[expected,LOST if expected==LOST else 0])
 return rows

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'verification/D3D_FOLLOWUP.json');a=p.parse_args()
 rows=checks();digest=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
 result={'scope':__doc__,'passed':sum(r['passed'] for r in rows),'total':len(rows),'checks':rows,
         'probe_sha256':digest(Path(__file__)),
         'production_source_sha256':{n:digest(ROOT/n) for n in ['src/vx_d3d_core.c','src/vx_d3d_core.h','src/vx_d3d_hal.c','src/vx_d3d_legacy.inc']},
         'tested_hal_sha256':digest(ROOT/'driver/VXDDRAW.DLL'),
         'tested_host_library_sha256':digest(ROOT/'tests/core.so')}
 a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps(result,indent=2)+'\n')
 print(f"{result['passed']}/{result['total']} follow-up D3D regression probes passed.")
 for row in rows:
  if not row['passed']:print(row['name']+': '+repr(row['observed'])+' expected '+repr(row['expected']))
 raise SystemExit(0 if result['passed']==result['total'] else 1)
