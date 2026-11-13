#!/usr/bin/env python3
"""v10.105: hardware cursor (register values, image conversion, which shapes stay with the software cursor) and the flip refused while the engine is lost."""
import struct
from engine import GPU,ARENA,MM,FB,pack
from flip import geometry,ctl as flip_ctl
W=lambda v,i:struct.unpack('<I',bytes(v)[i*4:i*4+4])[0]
AM,XM=ARENA+0x20000,ARENA+0x22000
OFF=0xFFC000
def v10105_checks():
 cases=[];out=ARENA+0x3000
 def ctl(g,v):
  g.call('VX2D_control',(v,out));return [W(g.u.mem_read(out,16),i) for i in range(4)]
 def shape(g,w,h,fn):
  a=[0xFFFFFFFF]*1024;x=[0]*1024
  for yy in range(h):
   for xx in range(w):
    aa,cc=fn(xx,yy);a[yy*32+xx]=aa;x[yy*32+xx]=cc
  g.u.mem_write(AM,pack(a));g.u.mem_write(XM,pack(x))
 def arrow(xx,yy):
  if xx<=yy and xx<8:return (0,0x00FFFFFF if xx>0 else 0)
  return (0xFFFFFFFF,0)
 def regs(g,n):return {k:v for k,v in g.reg.items() if k>=0x200}
 def fresh(bpp=32,mode=1):
  g=GPU(bpp);g.u.mem_write(g.addr('vx2d_hwcur_off'),pack([OFF]));ctl(g,0xF9000000|mode);return g
 def setshape(g,w=12,h=16,bpp=32,fn=arrow):
  shape(g,w,h,fn);return g.call('VX2D_hwcur_set',(AM,XM,32,w,h,bpp))
 # --- register set 1: start-up values, image, position, show, hide ---
 for bpp in (8,16,32): # v10.262: 8bpp too (the VxD hands colours, looked up in the palette)
  g=fresh(bpp,1);assert setshape(g,12,16,bpp)==1
  r=g.reg;assert (r[0x2EC],r[0x120C],r[0x2E4],r[0x2E8],r[0x2F0],r[0x2F4])==(0,0xFFFFFF,0xFFFFFF,0x0D000D0F,0x36000004,OFF),{hex(k):hex(v) for k,v in r.items() if k>=0x200}
  img=struct.unpack('<4096I',bytes(g.u.mem_read(FB+OFF,16384)))
  for yy in range(64):
   for xx in range(64):
    exp=0
    if xx<12 and yy<16:
     aa,cc=arrow(xx,yy);exp=(0xFF000000|cc) if aa==0 else 0
    assert img[yy*64+xx]==exp,(xx,yy,hex(img[yy*64+xx]),hex(exp))
  assert g.call('VX2D_hwcur_active')==0 and (g.reg[0x2F0]&1)==0;cases.append(f'{bpp}bpp hardware cursor set 1: start-up registers as OpenChrome writes them for the VX855, 64x64 ARGB image (opaque where AND is 0, transparent where AND is 1 and XOR 0, the rest of the 64x64 clear), cursor still off')
  assert g.call('VX2D_hwcur_move',(10,20))==1;r=g.reg;assert r[0x2F8]==(10<<16|20) and r[0x2FC]==0 and r[0x2F0]==0x36000005 and g.call('VX2D_hwcur_active')==1
  assert g.call('VX2D_hwcur_move',(300,200))==1;assert g.reg[0x2F8]==(300<<16|200);cases.append(f'{bpp}bpp hardware cursor set 1: the first move shows it, later moves only change the position')
  assert g.call('VX2D_hwcur_move',((-5)&0xFFFFFFFF,(-7)&0xFFFFFFFF))==1;assert g.reg[0x2F8]==0 and g.reg[0x2FC]==(4<<16|6),{hex(k):hex(v) for k,v in g.reg.items() if k>=0x2F8};cases.append(f'{bpp}bpp hardware cursor set 1: a negative position becomes 0 with an origin offset (even pixels, as OpenChrome)')
  g.call('VX2D_hwcur_hide');assert g.reg[0x2F0]==0x36000004 and g.call('VX2D_hwcur_active')==0
  st=ctl(g,0xFFFFFE16);assert (st[0],st[1],st[2],st[3])==(3,1,1,0),st;cases.append(f'{bpp}bpp hardware cursor: hide clears only the enable bit; the counters report moves, shape loads and hides')
 # --- register set 2 ---
 g=fresh(32,2);assert setshape(g)==1
 r=g.reg;assert (r[0x270],r[0x274],r[0x268],r[0x278],r[0x260],r[0x224])==(0,0xFFFFFF,0x000E0000,0x0E0F0000,0xB6000004,OFF)
 assert 0x2F0 not in r and 0x2F4 not in r
 g.call('VX2D_hwcur_move',(10,20));assert g.reg[0x208]==(10<<16|20) and g.reg[0x20C]==0 and g.reg[0x260]==0xB6000005
 g.call('VX2D_hwcur_hide');assert g.reg[0x260]==0xB6000004;cases.append('hardware cursor set 2 (the second display): its own registers, none of set 1 touched')
 # --- shapes the hardware cannot draw stay with the software cursor ---
 g=fresh(32,1);before=dict(g.reg)
 assert setshape(g,8,8,32,lambda x,y:(0xFFFFFFFF,0xFFFFFF if x==3 else 0))==0 and g.call('VX2D_hwcur_move',(5,5))==0 and g.reg==before;cases.append('a shape with an inverting pixel (AND 1, XOR not 0) is left to the software cursor: no register written, move refused')
 assert setshape(g,8,8,32,lambda x,y:(0x00FF00FF if x==3 else 0xFFFFFFFF,0))==0;cases.append('a mask that is neither all ones nor all zeros is left to the software cursor')
 assert setshape(g,33,8,32)==0;cases.append('a shape wider than 32 pixels is left to the software cursor')
 for bpp in (24,15):assert setshape(g,12,16,bpp)==0
 cases.append('15 and 24 bit screens are left to the software cursor')
 g.u.mem_write(g.addr('vx2d_hwcur_off'),pack([0]));assert setshape(g)==0;cases.append('no image block: software cursor')
 g.u.mem_write(g.addr('vx2d_hwcur_off'),pack([OFF+0x100]));assert setshape(g)==0;cases.append('an image block that is not 1 KB aligned is refused')
 g.u.mem_write(g.addr('vx2d_hwcur_off'),pack([0x1000000-0x2000]));assert setshape(g)==0;cases.append('an image block that does not fit in video memory is refused')
 g=fresh(32,1);g.u.mem_write(g.addr('vx_status')+12*4,pack([FB+0x1000]));assert setshape(g)==0;cases.append('with the shadow frame buffer (not the real video memory) the hardware cursor is not used')
 g=fresh(32,0);assert setshape(g)==0 and not any(k>=0x200 for k in g.reg);cases.append('hardware cursor off (ACCEL_HWCURSOR 0): nothing written')
 st=ctl(g,0xFFFFFE15);assert st[0]==0 and st[2]>=1 and st[3]==OFF,st
 # .530: the cursor check (ACCEL_CURSOR_CHECK, 0xFFFFFE1F, 70+i) and the inverting variants (ACCEL_HWCURSOR_INV) removed: 1D says ready and shown
 g=fresh(32,1);assert setshape(g)==1;g.call('VX2D_hwcur_move',(10,20));st=ctl(g,0xFFFFFE1D);assert st[0]&6==6 and (st[0]>>8)&3==1 and st[1]==0,st
 cases.append('0xFFFFFE1D: the shape ready and shown, the live mode')
 g=fresh(32,1);assert setshape(g,8,8,32,lambda x,y:(0xFFFFFFFF,0xFFFFFF) if x==3 else (0,0))==0;cases.append('a shape with inverting pixels is the software cursor\'s')
 g=fresh(32,1);assert setshape(g,8,8,32,lambda x,y:(0xFFFFFFFF,0x00FF00) if x==3 else (0,0))==0;cases.append('a shape with colour-XOR pixels is the software cursor\'s')
 g=fresh(32,1);setshape(g);g.call('VX2D_3d_reset',());g.call('VX2D_hwcur_invalidate',())
 assert ctl(g,0xFFFFFE1D)[0]&6==0;cases.append('after an invalidate the read back says not ready and not shown')
 # --- live control ---
 g=GPU(32);assert ctl(g,0xFFFFFE15)[0]==0
 assert ctl(g,0xF9000001)[0]==1 and ctl(g,0xF9000002)[0]==2 and ctl(g,0xF9000003)[0]==0 and ctl(g,0xF9000000)[0]==0;cases.append('live control: 1 and 2 pick the register set, anything else is off')
 g=GPU(32);g.u.mem_write(g.addr('vx2d_hwcur_cfg'),pack([2]));ctl(g,0xF9000001);ctl(g,0xFFFFFFFF);assert ctl(g,0xFFFFFE15)[0]==2;cases.append('the registry reset puts the live mode back to ACCEL_HWCURSOR')
 g=fresh(32,1);g.u.mem_write(g.addr('vx2d_hwcur_off'),pack([OFF]));setshape(g);g.call('VX2D_hwcur_move',(4,4));assert g.reg[0x2F0]&1
 ctl(g,0xF9000000);assert g.reg[0x2F0]&1==0 and g.call('VX2D_hwcur_move',(5,5))==0;cases.append('switching the mode off while the cursor is shown hides it and refuses further moves until a shape is loaded')
 # --- a flip while the engine is lost ---
 for bpp in (16,32):
  g=GPU(bpp);assert g.submit(g.request())==1;g.timeout=True;g.stuck=True;assert g.submit(g.request())==3
  geometry(g,1024,768,bpp)
  r=flip_ctl(g,1,0x4000);assert r[0]==4,r;cases.append(f'{bpp}bpp a flip while the engine is lost is refused (VXF_LOST) and the display address is not written')
  r=flip_ctl(g,3);assert r[0]==0;cases.append(f'{bpp}bpp returning to the desktop (reset) still works while the engine is lost')
 g=GPU(32);geometry(g,1024,768,32);r=flip_ctl(g,1,0x4000);assert r[0]==0,r;cases.append('control: the same flip on a healthy engine succeeds')
 return cases
