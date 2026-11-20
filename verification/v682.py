#!/usr/bin/env python3
""".683: the 2D engine's proof after a 3D engine reset (vx855_2d.c selftest): a good engine passes and GDI goes on drawing on it; an
engine whose copies come out wrong fails, the 2D engine is turned off (every request declined: GDI draws in software) until the next
mode set, where it is proved again; VX2D_proof mode 1 fails the next proof on purpose (VXREG GDITEST OFF)."""
import struct
from engine import GPU,ARENA,pack
W=lambda g,n,i=0:struct.unpack('<I',bytes(g.u.mem_read(g.addr(n)+4*i,4)))[0]
def v682_checks():
 cases=[];out=ARENA+0x3000
 def ver(g):return [W(g,'vx2d_verify',i) for i in range(6)]
 def fill(g,x=4,y=4):return g.submit(g.request(1,dx=x,dy=y,w=8,h=6))
 def newmode(g):s=W(g,'vx_status',24);g.u.mem_write(g.addr('vx_status')+24*4,pack([s+1]))
 for bpp in (16,32):
  g=GPU(bpp)
  assert fill(g)==1,'the first fill'
  g.call('VX2D_3d_reset',());assert W(g,'vx2d_verify_pending')==1
  assert fill(g,10,10)==1;v=ver(g);assert v[0]==1 and v[1]==1 and v[2]==0 and W(g,'vx2d_verify_pending')==0,v
  cases.append('%d bpp: a good engine passes its proof after a 3D reset and GDI goes on drawing on it'%bpp)
  good=g.run
  def bad(cmd,g=g,good=good):
   good(cmd)
   if cmd>>24==0xCC:g.u.mem_write(0x2000000+W(g,'vx2d_brush_off')+4*256,b'\x55'*8) # the copy lands wrong
  g.run=bad
  g.call('VX2D_3d_reset',());assert fill(g,12,12)==0,'declined after the failed proof'
  v=ver(g);assert v[2]==1 and v[3]==4,v
  assert fill(g,14,14)==0,'still declined in the same mode'
  g.call('VX2D_proof',(0,out));o=[struct.unpack('<I',bytes(g.u.mem_read(out+4*i,4)))[0] for i in range(2)];assert o[0]==2 and o[1]==2,o
  cases.append('%d bpp: a wrong copy fails the proof; the 2D engine is off (GDI in software) for the rest of the mode'%bpp)
  g.run=good;newmode(g)
  assert fill(g,16,16)==1,'proved again at the next mode set';v=ver(g);assert v[1]==2 and W(g,'vx2d_verify_pending')==0,v
  cases.append('%d bpp: the next mode set proves the engine again and GDI draws on it'%bpp)
  g.call('VX2D_proof',(0,out));o=[struct.unpack('<I',bytes(g.u.mem_read(out+4*i,4)))[0] for i in range(2)];assert o[0]==1 and o[1]==1,o
  g.call('VX2D_proof',(1,out));assert fill(g,18,18)==0;v=ver(g);assert v[3]==6 and v[5]==1,v
  cases.append('%d bpp: VX2D_proof: made now (passed), and the next one failed on purpose (VXREG GDITEST OFF) turns the engine off'%bpp)
 return cases
if __name__=='__main__':print('\n'.join(v682_checks()))
