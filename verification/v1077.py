#!/usr/bin/env python3
"""v10.77: colour pattern fills from an 8x8 block in video memory (VxD block upload, offsets, ROPs, gates; DRV side is in frontend.py)."""
import random
from engine import GPU,FB,pack
BRUSH=0xFFF000 # the model's reserved 4 KB (vx2d_brush_off)
def v1077_checks():
 cases=[];rnd=random.Random(1077)
 for bpp in (8,16,32):
  g=GPU(bpp);mask=(1<<bpp)-1;px=bpp//8
  for rop in (0xf0,0x5a):
   for dx,dy in [(0,0),(3,5),(7,6),(13,2),(5,11),(8,8),(1,15),(15,1)]:
    q=g.request(4,dx=dx,dy=dy,rop=rop);vals=[rnd.getrandbits(32)&mask for i in range(64)];q[29:93]=vals
    g.prefill();before=g.snapshot();expected=g.reference(q,before);w0=len(g.writes);n=len(g.commands)
    assert g.submit(q)==1,(bpp,rop,dx,dy);assert g.snapshot()[:BRUSH]==expected[:BRUSH],(bpp,rop,dx,dy)
    assert len(g.commands)==n+1 and g.commands[-1]==((rop<<24)|1),hex(g.commands[-1])
    w=g.writes[w0:];ci=max(i for i,(a,v) in enumerate(w) if a==0);r20=[v for a,v in w[:ci] if a==0x20][-1] # the value the command ran with (the registers are restored afterwards)
    assert r20==((BRUSH>>3)|((dy&7)<<29)|((dx&7)<<26)),hex(r20)
    assert not [1 for a,v in w if 0x100<=a<0x200],'pattern RAM must not be written'
    blk=bytes(g.u.mem_read(FB+BRUSH,64*px));assert blk==b''.join(v.to_bytes(px,'little') for v in vals),'block bytes in video memory'
  cases.append(f'{bpp}bpp colour pattern fill: pixels match the screen-anchored model for 8 positions and ROPs F0 and 5A, block bytes in video memory, offsets (x&7, y&7) in register 20, pattern RAM untouched')
  q=g.request(4,flags=1,l=11,t=9,r=22,b=16,dx=3,dy=5,w=24,h=14);q[29:93]=[rnd.getrandbits(32)&mask for i in range(64)]
  g.prefill();before=g.snapshot();expected=g.reference(q,before);assert g.submit(q)==1 and g.snapshot()[:BRUSH]==expected[:BRUSH]
  cases.append(f'{bpp}bpp clipped colour pattern fill matches the model (offsets come from the unclipped rectangle corner)')
  for rop in (0,255,0x55,0xcc):
   q=g.request(4,rop=rop);n=len(g.writes);assert g.submit(q)==0 and len(g.writes)==n,rop
  cases.append(f'{bpp}bpp colour pattern fill with a ROP that does not use the pattern is rejected')
  g.u.mem_write(g.addr('vx2d_brush_off'),pack([0]));n=len(g.writes);assert g.submit(g.request(4))==0 and len(g.writes)==n
  g.u.mem_write(g.addr('vx2d_brush_off'),pack([BRUSH]));assert g.submit(g.request(4))==1
  cases.append(f'{bpp}bpp without a reserved block the colour fill is declined, with it accepted')
  g.u.mem_write(g.addr('vx2d_fcol'),pack([0]));n=len(g.writes);assert g.submit(g.request(4))==0 and len(g.writes)==n
  if bpp!=8:assert g.submit(g.request(3))==1
  g.u.mem_write(g.addr('vx2d_fcol'),pack([1]));assert g.submit(g.request(4))==1
  cases.append(f'{bpp}bpp ACCEL_COLORBRUSH=0 turns the colour fill off and leaves the mono fill alone (16 and 32bpp)')
 import os
 root=os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
 m=open(os.path.join(root,'src','vxd_main.c'),encoding='latin-1').read()
 assert 'fbhda->overlays_size += 4096UL;' in m and 'vx2d_brush_off = fbhda->vram_size_virt - fbhda->overlays_size;' in m
 j=m.find('fbhda->overlays_size += 4096UL;');assert 0<j;cases.append('the 4 KB block is reserved at the end of video memory, outside what DirectDraw is given (.530: no OVERLAY_n_SIZE blocks)')
 d=open(os.path.join(root,'src','dibcall.c'),encoding='latin-1').read()
 assert 'q.pattern[k]=b8->dp8BrushBits[k]' in d and 'b8->dp8BrushStyle==BS_HATCHED' in d and 'PATTERNMONO' in d and 'q.op=VX2D_COLOR;' in d and 'colour pattern fills keep hidden state' not in d
 cases.append('DRV: at 8bpp a non-solid, non-mono, non-hatch brush goes to the engine as a colour pattern fill; at 16 and 32bpp brushes of three or more colours do')
 a=open(os.path.join(root,'src','vx2auto.c'),encoding='latin-1').read()
 assert 'brush_tests(dc,w,h,b);' in a and 'PATINVERT' in a and 'GetBitmapBits' in a;cases.append('VX2AUTO runs the brush comparison in every desktop mode')
 return cases
if __name__=='__main__':
 c=v1077_checks();print(len(c),'v10.77 checks passed')
