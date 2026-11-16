#!/usr/bin/env python3
""".599: the VX900 2D probe (VX2D_control 0xFFFFFE70 + k, the tool VX92D) in the compiled VxD against the 2D model, which decodes the
M1 layout: at 8, 16 and 32 bits, with the 2D engine in use (a VX855) and not in use (a VX900, as the driver runs there today):
the M1 fill draws exactly its rectangle; the older layout's fill does not; a keyed copy skips the key colour; the two whole-screen
inverts leave the screen as it was; every step leaves the whole frame buffer byte-identical and the engine's registers as found.
.601: with the device a VX900 the 2D engine is in use: a fill, a copy and a keyed copy go through it and match the reference; the
status probe (.602) draws nothing."""
import struct,sys,pathlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent.parent/'verification'))
import engine
from engine import GPU,pack,FB,ARENA
OUT=ARENA+0x3000
def step(g,k):
 g.u.mem_write(OUT,pack([0xEEEEEEEE]*4));g.call('VX2D_control',(0xFFFFFE70+k,OUT));r=list(struct.unpack('<4I',g.u.mem_read(OUT,16)))
 for c in (12,13,14):
  g.call('VX2D_control',(0xFFFFFE70+c,OUT));r+=list(struct.unpack('<4I',g.u.mem_read(OUT,16)))
 return r
def main():
 fails=0;n=0
 def check(ok,name,extra=''):
  nonlocal fails,n;n+=1;fails+=not ok;print('PASS' if ok else 'FAIL',name,'' if ok else extra)
 for bpp in (8,16,32):
  for used in (1,0):
   g=GPU(bpp);tag='%d bits, the engine %s'%(bpp,'in use (VX855)' if used else 'not in use (VX900)')
   if not used:
    g.u.mem_write(g.addr('vx2d_enable'),pack([0]));g.u.mem_write(g.addr('vx_chip_id'),pack([0x7122]))
   g.prefill();px=bpp//8
   # a recognisable screen: every byte from a counter, so a stray write anywhere shows
   g.u.mem_write(FB,bytes((i*7+3)&255 for i in range(64*48*px)))
   regs0=dict(g.reg);seq0=dict(g.seq)
   for k,name in ((0,'the M1 fill'),(3,'keyed copy 4000h'),(4,'keyed copy 6000h'),(5,'two inverts'),(1,'the older layout fill'),(0,'the M1 fill again')):
    before=g.snapshot();cmds=len(g.commands);r=step(g,k);after=g.snapshot()
    check(r[0]==0,'%s: %s ran'%(tag,name),hex(r[0]))
    check(before==after,'%s: %s leaves the frame buffer as it was'%(tag,name))
    check(g.excluded==0 and g.critical==0,'%s: %s leaves no exclusion or critical section'%(tag,name))
    if k==0:check(r[1]==r[11]==48*8 and r[2]==0 and r[12]==0 and r[8]==(8|4<<16) and r[9]==(55|11<<16),'%s: %s draws exactly its rectangle'%(tag,name),r)
    if k==1:check(r[1]!=r[11] and (r[8]&0xFFFF)<64 and (r[9]>>16)<16,'%s: %s does not draw the rectangle on an M1 engine (what it changes lies in the test area)'%(tag,name),r)
    if k in (3,4):check(r[1]==32 and r[5]==32 and r[2]==0 and r[4]==0 and r[6]==0,'%s: %s skips the key colour'%(tag,name),r)
    if k==5:check(r[8]==1 and len(g.commands)-cmds==2 and g.commands[-1]==0x55002001,'%s: %s put the screen back'%(tag,name),r)
    check(r[13]==(64|16<<16) and r[14]==(bpp|(64*px)<<8),'%s: %s reports the test area and the mode'%(tag,name),r)
   if not used:
    keys=(4,8,12,16,20,24,28,32,36,40,44,48,64,68,72,76,80,88,92)
    check(all(g.reg.get(a,0)==regs0.get(a,0) for a in keys) and g.reg.get(0x34,0)==regs0.get(0x34,0) and g.reg.get(0x38,0)==regs0.get(0x38,0),'%s: the engine\'s registers are as found'%tag,[(hex(a),hex(g.reg.get(a,0)),hex(regs0.get(a,0))) for a in keys if g.reg.get(a,0)!=regs0.get(a,0)])
    check(all(g.seq.get(i,0)==seq0.get(i,0) for i in (0x10,0x1A,0x1E,0x3C,0x4E)),'%s: the sequencer lock and the engine enables (SR1A, SR1E) are as found (.600)'%tag,{hex(i):(hex(g.seq.get(i,0)),hex(seq0.get(i,0))) for i in (0x10,0x1A,0x1E,0x3C,0x4E)})
  # .601: the VX900's 2D engine in use: the driver's own requests through the engine, against the reference
  class G9(GPU):chip_id=0x7122
  g=G9(bpp);g.call('VX2D_control',(0,OUT));tag='%d bits, VX900'%bpp
  check(g.word(g.addr('vx2d_enable'))==1,'%s: the 2D engine is on (.601)'%tag)
  for name,q in (('a fill',g.request(1,dx=5,dy=7,w=20,h=9)),('a copy',g.request(2,dx=30,dy=20,sx=3,sy=4,w=17,h=11)),('a source-keyed copy',g.request(2,flags=2,dx=30,dy=20,sx=3,sy=4,w=17,h=11,key=(4*1981+3*383+0x8345)&((1<<bpp)-1)))):
   g.prefill();before=g.snapshot();want=g.reference(q,before);cmds=len(g.commands);rc=g.submit(q)
   check(rc==1 and len(g.commands)==cmds+1 and g.snapshot()==want,'%s: %s is drawn by the engine as the reference draws it'%(tag,name),rc)
  # .602: the status probe (step 6): nothing drawn, the registers as found, the quick idle check's counts carried
  for used in (1,0):
   g=G9(bpp);tag2='%d bits, VX900, the engine %s'%(bpp,'on' if used else 'switched off live')
   if not used:g.u.mem_write(g.addr('vx2d_enable'),pack([0]))
   g.submit(g.request(1,dx=5,dy=7,w=20,h=9),strict=False);regs0=dict(g.reg);seq0=dict(g.seq);before=g.snapshot();cmds=len(g.commands);r=step(g,6)
   check(r[0]==0 and len(g.commands)==cmds and g.snapshot()==before,'%s: the status probe runs and draws nothing'%tag2,[hex(x) for x in r])
   check(all((r[1+k]&255)==0 and ((r[1+k]>>8)&255)==0 for k in range(5)),'%s: on the model (idle at once) no try is counted as not idle'%tag2,[hex(x) for x in r[1:6]])
   if not used:check(all(g.reg.get(a,0)==regs0.get(a,0) for a in (4,8,12,16,20,24,28,32,36,40,44,48,64,68,72,76,80,88,92)) and all(g.seq.get(i,0)==seq0.get(i,0) for i in (0x10,0x1A,0x1E)),'%s: the registers and the sequencer are as found'%tag2)
  # refusals
 g=GPU(32);g.u.mem_write(g.addr('vx2d_enable'),pack([0]));g.seq[0x4e]=0x10;r=step(g,0)
 check(r[0]==3 and (r[15]>>16)&255==0x10,'the 3D reset bits set in SR4E: the registers are not opened, SR4E is reported',[hex(x) for x in r])
 g=GPU(32);g.stuck=True;g.timeout=True;r=step(g,0)
 check(r[0] in (4,5),'an engine that stays busy is reported, not waited for for ever',[hex(x) for x in r])
 g=GPU(32);g.u.mem_write(OUT,pack([0]*4));g.call('VX2D_control',(0xFFFFFE72,OUT))
 check(struct.unpack('<I',g.u.mem_read(OUT,4))[0]==0xFFFFFFFF and len(g.commands)==0,'an unused probe number draws nothing')
 print('vx900_probe: %d checks, %d failures'%(n,fails));sys.exit(1 if fails else 0)
if __name__=='__main__':main()
