#!/usr/bin/env python3
""".604: the tool-driven triangle test of the 3D lab (VX3D_test, probe 1, lab_cmd 'RAW3'; the tool VX93D) in the compiled VxD: the
tool's words reach the 3D ports in order after the fixed steps, a wait marker writes nothing, a port outside the five allowed is
refused before anything is written, and the scratch arena is put back. The host has no model of the 3D engine's drawing: what a chip
draws is the board's answer."""
import struct,sys,pathlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent.parent/'verification'))
import engine
from engine import pack,FB,MM,ARENA
from native3d import NativeGPU
RQ,RP=ARENA+0x8000,ARENA+0x20000
REQ_WORDS=18+32+1024;ARENA3D=0x200000
class Lab(NativeGPU):
 def __init__(self,bpp=32):
  super().__init__(bpp)
  for n,fn in (('Get_Cur_VM_Handle',lambda:0x1234),('Get_Sys_VM_Handle',lambda:0x1234)):
   if n in self.image.symbols:self.hook(n,fn)
  self.hook('VESA_native_span',self.span)
  for k in (0x18,0x1c,0x20,0x40,0x44,0x48,0x4c,0x50,0x5c):self.reg.setdefault(k,0) # the lab's 2D fill check writes only the registers a fill needs
 def span(self):
  self.u.mem_write(self.arg(0),pack([0]));self.u.mem_write(self.arg(1),pack([64*48*self.px]));return 1
 def write(self,u,t,a,z,v,_):
  if a-MM in (0x41c,0x420,0x6c):self.port_trace.append((a-MM,v));return
  return super().write(u,t,a,z,v,_)
def run(g,pairs,flags=0,recover=0,ndata=None):
 w=[0]*REQ_WORDS;w[0]=REQ_WORDS*4;w[1]=0x00190000;w[2]=0x54534554;w[3]=0x100 if recover else 0;w[4]=1;w[5]=0x33574152;w[15]=flags;w[17]=len(pairs)*2 if ndata is None else ndata
 for i,(p,v) in enumerate(pairs):w[18+32+2*i]=p;w[18+32+2*i+1]=v
 g.u.mem_write(RQ,pack(w));g.u.mem_write(RP,bytes(0x12000));g.port_trace=[]
 rc=g.call('VX3D_test',(RQ,RP));r=struct.unpack('<24I',g.u.mem_read(RP,96))
 return rc,r
def main():
 fails=0;n=0
 def check(ok,name,extra=''):
  nonlocal fails,n;n+=1;fails+=not ok;print('PASS' if ok else 'FAIL',name,'' if ok else extra)
 words=[(0x43C,0x00040000),(0x440,0x00A80106),(0x440,0x010000C2),(0x43C,0x31030000),(0x440,0x0008014C),(0xFFFFFFFF,0),(0x43C,0),(0x440,0x41000000),(0x6C,0x00800000),(0x41C,0x00100000),(0x420,0x02000000)]
 g=Lab(32);g.u.mem_write(FB+ARENA3D,bytes((i*13+5)&255 for i in range(0x10000)));before=bytes(g.u.mem_read(FB,0x400000))
 rc,r=run(g,words)
 sent=[x for x in words if x[0]!=0xFFFFFFFF]
 check(rc==0 and r[3]==1,'the tool\'s words run to the end (result PASS)',(rc,r[:8]))
 check(g.port_trace[-len(sent)-2:-2]==sent,'they reach the ports in order, after the fixed steps, the wait marker writing nothing; then only the engine is switched off (01/01 = 0)',g.port_trace[-len(sent)-3:])
 check(g.port_trace[-2:]==[(0x43C,0x00010000),(0x440,0x01000000)],'the last two writes switch the 3D engine\'s output off',g.port_trace[-2:])
 check(len(g.port_trace)>len(sent)+20,'the fixed steps came first (command path, FE block, state set-up)',len(g.port_trace))
 check(bytes(g.u.mem_read(FB,0x400000))==before,'the scratch arena and the rest of video memory are as before')
 check(r[5]==4 and r[23]==0,'stage 4 (idle after the words), no fault latched',(r[5],r[23]))
 g=Lab(32);before=bytes(g.u.mem_read(FB,0x400000));rc,r=run(g,[(0x43C,0),(0x444,1),(0x440,2)])
 check(rc==0 and r[3]==12 and not g.port_trace and bytes(g.u.mem_read(FB,0x400000))==before,'a port outside 41C / 420 / 43C / 440 / 6C is refused (bad request) before anything is written',(rc,r[3],g.port_trace[:4]))
 rc,r2=run(g,words);check(rc==0 and r2[3]==1,'a refused request does not latch the fault: the next one runs',(rc,r2[3]))
 g=Lab(32);rc,r=run(g,[(0x440,1)]*8,ndata=1026)
 check(rc==0 and r[3]==12 and not g.port_trace,'more than 512 words are refused',(rc,r[3]))
 g=Lab(32);rc,r=run(g,[(0x440,1)]*8,ndata=15)
 check(rc==0 and r[3]==12 and not g.port_trace,'an odd word count is refused',(rc,r[3]))
 g=Lab(32);rc,r=run(g,words,flags=1|2|4|8|16)
 check(rc==0 and r[3]==1 and g.port_trace[:len(sent)]==sent,'with every fixed step skipped only the tool\'s words (and the switch-off) are written',g.port_trace[:4])
 print('vx900_tri: %d checks, %d failures'%(n,fails));sys.exit(1 if fails else 0)
if __name__=='__main__':main()
