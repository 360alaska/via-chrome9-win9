#!/usr/bin/env python3
"""v10.73: GDI binding diagnostic, scan line sampling (VxD and HAL, gated), register window read, and video memory beyond 16 MB."""
import struct
from engine import GPU,ARENA,MM,FB,pack
from flip import setup,ctl
from directdraw import HAL
W=lambda v,i:struct.unpack('<I',bytes(v)[i*4:i*4+4])[0]
SCAN=5
def v1073_checks():
 cases=[]
 # --- GDI binding diagnostic: 0xFFFFFE10 ---
 g=GPU(32);out=ARENA+0x3000;u=g.u
 q=lambda:(g.call('VX2D_control',(0xFFFFFE10,out)),[W(u.mem_read(out,16),i) for i in range(4)])[1]
 g.call('VX2D_control',(0xFFFFFFFE,out));flags0=g.word(out+12)
 r=q();assert r[0]==1 and r[1]==0 and r[3]==flags0,r;cases.append('GDI binding query: bound, enabled, not busy; the ordinary flags are untouched')
 u.mem_write(ARENA+0x802,struct.pack('<H',0));r=q();assert r[0]==3,r;cases.append('GDI binding query reports the DRV enabled word reading 0')
 u.mem_write(ARENA+0x800,struct.pack('<H',0x8010));r=q();assert r[0]==7,r;cases.append('GDI binding query reports the DRV BUSY flag')
 assert g.submit(g.request())==0;r=q();assert r[1]==1,r;assert g.submit(g.request(1,flags=16))==1;r=q();assert r[1]==1,r
 cases.append('a GDI request that reaches the VxD while the enabled word is 0 is counted; DirectDraw requests are not')
 g.call('VX2D_control',(0xFFFFFFFE,out));assert g.word(out+12)==flags0;cases.append('the query leaves the engine settings alone')
 # --- register window read ---
 g=GPU(32);g.u.mem_write(MM+0x1290,pack([(100<<16)|0x123]))
 assert g.call('VX2D_mmio_read',(0x1290,))==(100<<16)|0x123 and g.call('VX2D_mmio_read',(0x400000,))==0xFFFFFFFF and g.call('VX2D_mmio_read',(0x1291,))==0xFFFFFFFF
 cases.append('register window read returns the dword, refuses an offset past the window and a misaligned one')
 # --- VXF_SCAN in the VxD ---
 for bpp in (16,32):
  g=setup(bpp);raw=lambda v:g.u.mem_write(MM+0x1290,pack([v]))
  raw((100<<16)|0x123);r=ctl(g,SCAN);assert (r[0],r[1],r[2],r[3],r[4],r[5],r[6])==(0,1,100,(100<<16)|0x123,0,768,1),r;cases.append(f'{bpp}bpp scan sample reads line 100 from bits 31:16, gate off by default')
  g.u.mem_write(g.addr('vx_scan_enable'),pack([1]));r=ctl(g,SCAN);assert r[1]==1 and r[4]==1;cases.append(f'{bpp}bpp scan sample reports the SCANLINE gate')
  g.vbn=899;r=ctl(g,SCAN);assert r[1]==2 and r[2]==0;cases.append(f'{bpp}bpp a sample taken during the blank is reported as blank, not as a line')
  g.vbn=0;raw(900<<16);r=ctl(g,SCAN);assert r[1]==3 and r[6]==4,r;cases.append(f'{bpp}bpp a counter outside the picture is reported after four tries')
  g.vbn=0;raw(0xFFFFFFFF);r=ctl(g,SCAN);assert r[1]==0,r;cases.append(f'{bpp}bpp an unreadable window means the counter is unavailable')
 # --- HAL: GetScanLine only with the gate and a counter that answers ---
 SCANCB=27468
 for bpp in (16,32):
  h=HAL(bpp,geo=(1024,768));g=h.gpu
  assert h.word(h.context+SCANCB)==0;cases.append(f'{bpp}bpp GetScanLine is not offered while the registry gate is off')
  g.u.mem_write(g.addr('vx_scan_enable'),pack([1]));g.u.mem_write(MM+0x1290,pack([300<<16]));g.vbn=0
  assert h.call('_DriverInit@4',(h.context,))==1 and h.word(h.context+SCANCB)==h.sym['_VXGetScanLine@4'];cases.append(f'{bpp}bpp GetScanLine is offered when SCANLINE=1 and the counter answers')
  d=ARENA+0xb800;h.u.mem_write(d,pack([0,0xdead,0xdead,0]))
  assert h.call('_VXGetScanLine@4',(d,))==1 and h.word(d+4)==300 and h.word(d+8)==0;cases.append(f'{bpp}bpp GetScanLine returns the line')
  g.vbn=899;h.u.mem_write(d,pack([0,0xdead,0xdead,0]));assert h.call('_VXGetScanLine@4',(d,))==1 and h.word(d+8)==0x88760219;cases.append(f'{bpp}bpp GetScanLine during the blank returns VERTICALBLANKINPROGRESS')
  g.vbn=0;g.u.mem_write(MM+0x1290,pack([0xFFFFFFFF]));assert h.call('_DriverInit@4',(h.context,))==1 and h.word(h.context+SCANCB)==0;cases.append(f'{bpp}bpp an unreadable counter keeps GetScanLine out of the callback table')
 # --- video memory beyond 16 MB ---
 for limit,want in ((0x4000000,1),(0x1000000,0)):
  g=GPU(16);g.u.mem_map(0x3100000,0x1F00000) # FB offsets 17 MB .. 47 MB
  g.u.mem_write(g.hda+52,pack([limit]));g.u.mem_write(g.hda+60,pack([limit]))
  q=g.request(1,flags=16,fg=0x5A5A);q[5:9]=[0x1800000,256,50,32]
  assert g.submit(q)==want,(hex(limit),want)
  if want:assert bytes(g.u.mem_read(FB+0x1800000+6*256+7*2,2))==struct.pack('<H',0x5A5A)
 cases.append('a surface at 24 MB is accepted with 64 MB of video memory and refused with 16 MB')
 hdr=(open(__file__.replace('verification/v1073.py','src/vx85598.h') if 'verification/' in __file__ else '../src/vx85598.h',encoding='latin-1')).read()
 assert '#define VX_FB_LIMIT (64UL*1024UL*1024UL)' in hdr;cases.append('VX_FB_LIMIT is 64 MiB')
 dd=open(__file__.replace('verification/v1073.py','src/dddrv.c') if 'verification/' in __file__ else '../src/dddrv.c',encoding='latin-1').read()
 i=dd.rfind('dwCaps2 |= DDCAPS2_WIDESURFACES');assert i>dd.rfind('dwCaps2         =') and i>dd.rfind('FLIPNOVSYNC');cases.append('v10.74 DDCAPS2_WIDESURFACES is set after every other dwCaps2 assignment')
 return cases
if __name__=='__main__':
 c=v1073_checks();print(len(c),'v10.73 checks passed')
