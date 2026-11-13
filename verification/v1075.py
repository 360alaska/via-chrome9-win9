#!/usr/bin/env python3
"""v10.75: lost-engine recovery, retry of a switched-off engine after a mode set, bind that survives a lost engine, HAL CPU paths fenced while lost."""
import struct
from engine import GPU,BIND,GDI_SEL,ARENA,MM,FB,pack
W=lambda v,i:struct.unpack('<I',bytes(v)[i*4:i*4+4])[0]
SS=24  # vx_status word index of set_successes (the model starts it at 1)
def v1075_checks():
 cases=[];out=ARENA+0x3000
 def ctl(g,v):
  g.call('VX2D_control',(v,out));return [W(g.u.mem_read(out,16),i) for i in range(4)]
 def newmode(g,n):
  a=g.addr('vx_status')+SS*4;g.u.mem_write(a,pack([n]))
 # --- engine lost, stays lost within the same mode set, recovers after the next one ---
 for bpp in (16,32):
  g=GPU(bpp);assert g.submit(g.request())==1;g.timeout=True;g.stuck=True
  assert g.submit(g.request())==3 and g.call('VX2D_lost')==1
  assert bytes(g.u.mem_read(ARENA+0x800,4))==struct.pack('<HH',0x8010,0)
  assert g.submit(g.request())==3 and g.call('VX2D_access',(1,))==3;cases.append(f'{bpp}bpp a lost engine stays lost within the same mode set')
  g.timeout=False;g.stuck=False;g.busy=False
  assert g.submit(g.request())==3,'no retry without a new mode set';cases.append(f'{bpp}bpp a working engine is still not retried until the mode is set again')
  newmode(g,2)
  assert g.submit(g.request())==1 and g.call('VX2D_lost')==0;cases.append(f'{bpp}bpp a lost engine is reset and used again after the next mode set')
  assert bytes(g.u.mem_read(ARENA+0x800,4))==struct.pack('<HH',0x8000,1);cases.append(f'{bpp}bpp recovery clears the GDI BUSY fence and enables GDI again')
  r=ctl(g,0xFFFFFE11);assert r[0]==1 and r[2]==1 and r[3]==0,r;cases.append(f'{bpp}bpp recovery counter, state and fence flag are reported')
 # --- still stuck after the mode set: one attempt only, still lost ---
 g=GPU(32);assert g.submit(g.request())==1;g.timeout=True;g.stuck=True;assert g.submit(g.request())==3
 newmode(g,2);assert g.submit(g.request())==3 and g.call('VX2D_lost')==1;r=ctl(g,0xFFFFFE11);assert r[0]==0 and r[3]==1,r
 assert bytes(g.u.mem_read(ARENA+0x800,4))==struct.pack('<HH',0x8010,0)
 cases.append('an engine that does not answer the reset stays lost and fenced')
 # --- recovery through the CPU access call (a Lock) ---
 g=GPU(32);assert g.submit(g.request())==1;g.timeout=True;g.stuck=True;assert g.submit(g.request())==3
 g.timeout=False;g.stuck=False;g.busy=False;newmode(g,2)
 assert g.call('VX2D_access',(1,))==1 and g.call('VX2D_lost')==0;g.call('VX2D_access',(2,));cases.append('a CPU access request after a mode set also brings a lost engine back')
 # --- bind with a lost engine succeeds and fences ---
 g=GPU(32);assert g.submit(g.request())==1;g.timeout=True;g.stuck=True;assert g.submit(g.request())==3
 g.u.mem_write(ARENA+0x800,struct.pack('<HH',0x8000,1))
 assert g.call('VX2D_bind',BIND)==1
 assert bytes(g.u.mem_read(ARENA+0x800,4))==struct.pack('<HH',0x8010,0);cases.append('Enable-time bind with a lost engine succeeds (no failed display driver) and fences the GDI words')
 g.timeout=False;g.stuck=False;g.busy=False
 g.u.mem_write(ARENA+0x800,struct.pack('<HH',0x8000,1))
 assert g.call('VX2D_bind',BIND)==1 and g.call('VX2D_lost')==0;assert bytes(g.u.mem_read(ARENA+0x800,4))==struct.pack('<HH',0x8000,1)
 cases.append('Enable-time bind resets a lost engine that answers and leaves GDI enabled')
 # --- v10.273: the DRV's words move in linear memory (same selector, new base): the VxD follows them, reads them and fences them there ---
 g=GPU(32);assert g.submit(g.request())==1
 new=ARENA+0x4000;g.u.mem_write(new+0x800,struct.pack('<HH',0x8000,1));g.u.mem_write(ARENA+0x800,struct.pack('<HH',0x8000,0));g.selbase[GDI_SEL]=new
 assert g.submit(g.request())==1;r=ctl(g,0xFFFFFE1C);assert r[0]==1 and r[3]==new+0x802,r
 cases.append('the enabled word is read at the selector\'s current base after the segment moved in linear memory (the old place reads 0)')
 g.u.mem_write(new+0x800,struct.pack('<HH',0x8000,0));assert g.submit(g.request())==0;g.u.mem_write(new+0x800,struct.pack('<HH',0x8000,1))
 g.timeout=True;g.stuck=True;assert g.submit(g.request())==3
 assert bytes(g.u.mem_read(new+0x800,4))==struct.pack('<HH',0x8010,0) and bytes(g.u.mem_read(ARENA+0x800,4))==struct.pack('<HH',0x8000,0)
 cases.append('a lost engine fences the GDI words at their current place, not the old one')
 g=GPU(32);g.selbase.pop(GDI_SEL);assert g.submit(g.request())==0;r=ctl(g,0xFFFFFE1C);assert r[2]>=1,r;cases.append('a selector that does not map: GDI requests are declined, nothing written')
 # --- v10.279 (review): the DRV's copy of the host mask (in the same segment as wEnabled) follows the segment too ---
 def xq(g,lin):
  head=struct.pack('<9I4I6Ii I4iI I3II',1264,0x10000,5,4,g.bpp,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,lin,0,0,0,0)
  g.u.mem_write(ARENA+0x2000,head+bytes(768)+bytes(372));return g.call('VX2D_xfer',(ARENA+0x2000,))
 g=GPU(32);xq(g,ARENA+0x900)
 new=ARENA+0x4000;g.u.mem_write(new+0x800,struct.pack('<HHHH',0x8000,1,0,0));g.u.mem_write(new+0x900,struct.pack('<H',0x55));g.u.mem_write(ARENA+0x900,struct.pack('<H',0x66));g.selbase[GDI_SEL]=new
 ctl(g,0xF7000007);assert struct.unpack('<H',bytes(g.u.mem_read(new+0x900,2)))[0]==7 and struct.unpack('<H',bytes(g.u.mem_read(ARENA+0x900,2)))[0]==0x66
 cases.append('a live ACCEL_HOST change is written to the DRV\'s mask at its segment\'s current place, the old place left alone')
 g=GPU(32);g.u.mem_write(0x6001000,struct.pack('<H',0x66));xq(g,0x6001000);ctl(g,0xF7000007);assert struct.unpack('<H',bytes(g.u.mem_read(0x6001000,2)))[0]==0x66
 g.call('VX2D_bind',(0,0));ctl(g,0xF7000005);assert struct.unpack('<H',bytes(g.u.mem_read(ARENA+0x900,2)))[0]!=5
 cases.append('a mask address outside the DRV\'s segment is not kept, and an unbind leaves nothing to write')
 # --- v10.279 (review): a 3D loss fences the GDI words where they are now, without a 2D call before it ---
 g=GPU(32);assert g.submit(g.request())==1
 new=ARENA+0x4000;g.u.mem_write(new+0x800,struct.pack('<HH',0x8000,1));g.u.mem_write(ARENA+0x800,struct.pack('<HH',0x8000,1));g.selbase[GDI_SEL]=new
 g.call('VX2D_device_lost')
 assert bytes(g.u.mem_read(new+0x800,4))==struct.pack('<HH',0x8010,0) and bytes(g.u.mem_read(ARENA+0x800,4))==struct.pack('<HH',0x8000,1)
 cases.append('the 3D side\'s device loss fences the GDI words at the segment\'s current place')
 # --- v10.279 (review): a DirectDraw blit declined early (under ACCEL_COPY_MIN) retires a 3D draw in flight before the HAL copies on the CPU ---
 g=GPU(32);seen=[];g.hook('VX3DN_busy',lambda:seen.append(1) or 0)
 g.u.mem_write(g.addr('vx2d_copy_min'),pack([100000]));assert g.submit(g.request(2,flags=16))==0 and seen
 n=len(seen);assert g.submit(g.request(2))==0 and len(seen)==n
 cases.append('a declined DirectDraw blit retires the 3D draw in flight (the CPU finishes it); a declined GDI request does not')
 # --- engine switched off after a timeout (state 2) is tried again after a mode set, not before ---
 g=GPU(32);assert g.submit(g.request())==1;g.timeout=True;assert g.submit(g.request(rop=0x5a))==2
 g.timeout=False;g.busy=False
 assert g.submit(g.request())==0;cases.append('after a timeout the engine stays switched off within the same mode set')
 newmode(g,2);assert g.submit(g.request())==1;r=ctl(g,0xFFFFFE11);assert r[1]==1 and r[2]==1,r;cases.append('a switched-off engine is initialised again after the next mode set')
 # --- HAL: CPU blits on video surfaces refuse while the engine is lost ---
 src=open(__file__.replace('verification/v1075.py','src/vx_ddraw.c') if 'verification/' in __file__ else '../src/vx_ddraw.c',encoding='latin-1').read()
 i=src.find('access(0)==VX2D_LOST');j=src.find('sw_blit(p,&w,0,count)');assert 0<i<j;cases.append('VXBlit checks for a lost engine before any CPU path that touches video memory')
 # --- v10.76: the engine at 8bpp (depth code 000, 1 byte per pixel; fills, ROPs, copies, keys and clipping were measured with VX2D8) ---
 def verify8(g,q,name):
  g.prefill();before=g.snapshot();expected=g.reference(q,before);n=len(g.commands);w0=len(g.writes)
  assert g.submit(q)==1,name
  assert g.snapshot()==expected,name
  assert len(g.commands)==n+1 and g.excluded==0,name
  assert (4,0) in g.writes[w0:],name # the depth register was written with code 000
 g=GPU(8)
 for rop in (0,255,0x55,0x5a,0xf0):
  for c in (0,0x1f,0xa5,0xff):verify8(g,g.request(rop=rop,fg=c),f'8bpp fill rop {rop:x} colour {c:x}')
 cases.append('8bpp fills with every supported ROP match the model and write depth code 000')
 for dx in (3,7,13):
  for dy in (2,6,11):verify8(g,g.request(2,sx=7,sy=6,dx=dx,dy=dy),f'8bpp overlap {dx},{dy}')
 cases.append('8bpp copies in all overlap directions match the model')
 for op in (1,2):verify8(g,g.request(op,flags=1,l=11,t=9,r=22,b=16),f'8bpp clipped {op}')
 key=(0x8345+3*1981+2*383)&255
 for kf in (2,4):verify8(g,g.request(2,flags=kf,key=key),f'8bpp key {kf}')
 cases.append('8bpp clipped fills and copies, source and destination colour key match the model')
 g=GPU(8);g.prefill()
 n=len(g.writes);assert g.submit(g.request(3))==0 and len(g.writes)==n
 cases.append('8bpp mono pattern operations are not sent to the engine (the colour pattern fill from video memory is: v10.77)')
 g=GPU(8);g.u.mem_write(g.addr('vx2d_f8'),pack([0]));n=len(g.writes);assert g.submit(g.request())==0 and len(g.writes)==n
 g.u.mem_write(g.addr('vx2d_f8'),pack([1]));assert g.submit(g.request())==1
 cases.append('ACCEL_8BPP=0 keeps 8bpp off the engine; 16 and 32bpp are not affected by it')
 for bpp in (16,32):
  g=GPU(bpp);g.u.mem_write(g.addr('vx2d_f8'),pack([0]));assert g.submit(g.request())==1
 g=GPU(8);q=g.request();q[5:9]=[0x10000,256,50,32];q[9:13]=q[5:9];g.prefill(0x10000,256,50,32);assert g.submit(q)==1
 q=g.request();q[7]=100;n=len(g.writes);assert g.submit(q)==0 and len(g.writes)==n # height 100 past the 64x48 desktop descriptor
 cases.append('8bpp offscreen fill works; a surface that does not fit its own descriptor is rejected')
 src=open(__file__.replace('verification/v1075.py','src/dibcall.c') if 'verification/' in __file__ else '../src/dibcall.c',encoding='latin-1').read()
 assert '(wBpp!=8&&wBpp!=16&&wBpp!=32)' in src and 'dp8BrushBits[k]!=c8' in src;cases.append('DRV sends 8bpp solid-brush fills and copies to the VxD')
 return cases
if __name__=='__main__':
 c=v1075_checks();print(len(c),'v10.75 checks passed')
