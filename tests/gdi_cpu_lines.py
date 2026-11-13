#!/usr/bin/env python3
"""v10.507: the display driver's own solid lines (ACCEL_CPULINE), run in the delivered VX85598.DRV under Unicorn (16-bit protected mode,
the frame buffer behind a big-limit selector), against the line rule of the engine path and DIBENG (VX2LINE): n = major length, m = minor,
e0 = 2m - n - (1 when Y increases), n pixels, e >= 0 takes the minor step and adds 2(m - n), else adds 2m; the end point is not drawn.
Checks every octant, 8/16/32 bpp, R2_COPYPEN / R2_NOTCOPYPEN / R2_BLACK / R2_XORPEN, clipped segments, polylines, the cursor handshake
(no VxD call while the cursor state says direct and not drawn; the exclusion calls otherwise; a put-off show released) and the fall-back
of what the path does not take (another ROP goes to DIBENG untouched)."""
import random,struct,sys,pathlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent.parent/'verification'))
from harness16 import Frontend
from unicorn.x86_const import *

FB=0x200000; FBSEL=0x48; W,H=320,200
OP_RECT,OP_END=0x1112,0x110D;SKIP=0;NOLIM=(63<<2)|(63<<8)
class Backend:
 def call(self,name,args=()): return 1

def line_model(x0,y0,x1,y1):
 ddx,ddy=x1-x0,y1-y0;adx,ady=abs(ddx),abs(ddy);ym=ady>adx
 n,m=(ady,adx) if ym else (adx,ady)
 e=2*m-n-(1 if ddy>0 else 0);sx=-1 if ddx<0 else 1;sy=-1 if ddy<0 else 1;x,y=x0,y0;out=[]
 for _ in range(n):
  out.append((x,y))
  if e>=0:
   if ym:x+=sx
   else:y+=sy
   e+=2*(m-n)
  else:e+=2*m
  if ym:y+=sy
  else:x+=sx
 return out

RIG=None
class F(Frontend):
 def api(self,u,address,size,ud):
  op=u.reg_read(UC_X86_REG_EDX)&0xffff
  if RIG is not None and op==0x1135: u.reg_write(UC_X86_REG_ECX,SKIP); return
  if RIG is not None and op==0x1132: RIG.ops.append(op); u.reg_write(UC_X86_REG_ECX,RIG.submit_rc); return
  if RIG is not None and op in (OP_RECT,OP_END,0x1136):
   RIG.ops.append(op)
   if op==OP_END and RIG.hda_lo()&4: RIG.set_cur(RIG.hda_lo()&~4)  # the VxD shows the cursor and clears the bit
   u.reg_write(UC_X86_REG_ECX,1);return
  return Frontend.api(self,u,address,size,ud)

SELBASE={8:0x10000,16:0x20000,24:0x50000,32:0x30000,0x48:FB}
def _intr(self,u,num,ud):
 if num==0x31 and u.reg_read(UC_X86_REG_AX)==6:
  b=SELBASE.get(u.reg_read(UC_X86_REG_BX),0);u.reg_write(UC_X86_REG_CX,b>>16);u.reg_write(UC_X86_REG_DX,b&0xffff)
  u.reg_write(UC_X86_REG_EFLAGS,u.reg_read(UC_X86_REG_EFLAGS)&~1);return
 return Frontend.interrupt(self,u,num,ud)
F.interrupt=_intr

class Rig:
 def __init__(self):
  global RIG
  fe=F(protected=True,backend=Backend(),connect=True);u=fe.u;self.fe=fe;self.u=u;self.ops=[];RIG=self
  gdt=bytes(u.mem_read(0x80000,72))+struct.pack('<HHBBBB',0xffff,FB&0xffff,(FB>>16)&255,0x92,0x8f,(FB>>24)&255)
  u.mem_write(0x80000,gdt);u.reg_write(UC_X86_REG_GDTR,(0,0x80000,len(gdt)-1,0))
  u.mem_map(FB,0x100000)
  self.sym=fe.sym;self.A=0x30000;self.S=32;self.submit_rc=SKIP
  u.mem_write(0x41000,b'\xb8\x01\x00\xca\x1c\x00')  # DIB_Output stub: mov ax,1 / retf 28
 def hda_lo(self): return struct.unpack('<H',bytes(self.u.mem_read(self.A+0x400+260,2)))[0]
 def set_cur(self,lo): self.u.mem_write(self.A+0x400+260,struct.pack('<HH',lo,0x4355))
 def dw(self,name,val,fmt='<H'): self.u.mem_write(0x20000+self.sym[name][1],struct.pack(fmt,val))
 def setup(self,bpp,feat=0x8003,cur=1):
  if feat in (0x8001,0x8002,0x8003,0x8007):feat|=NOLIM  # v10.509: no length limit unless the test sets one
  u=self.u;A=self.A;S=self.S;pitch=W*bpp//8+64
  self.bpp=bpp;self.pitch=pitch
  self.dw('_wEnabled',1);self.dw('_wBpp',bpp);self.dw('_wScreenX',W);self.dw('_wScreenY',H)
  u.mem_write(0x20000+self.sym['_hda'][1],struct.pack('<HH',0x400,S))
  u.mem_write(0x20000+self.sym['_lpDriverPDevice'][1],struct.pack('<HH',0x100,S))
  dev=struct.pack('<HHHHBBIIIIHHH',0x5250,W,H,pitch,1,bpp,0,pitch,0,0,FBSEL,0x8000,0x400)
  u.mem_write(A+0x100,dev)
  u.mem_write(A+0x400,bytes(280));u.mem_write(A+0x400+256,struct.pack('<HH',0,feat));self.set_cur(cur)
  random.seed(bpp*7+feat);self.bg=bytes(random.getrandbits(8) for _ in range(pitch*H));u.mem_write(FB,self.bg)
 def output(self,pts,rop,color,clip=None,style=0):
  u=self.u;A=self.A;S=self.S
  u.mem_write(A+0x200,struct.pack('<HBBII',style,0,self.bpp,0,color))
  u.mem_write(A+0x240,struct.pack('<hhII',rop,1,0,0))
  u.mem_write(A+0x300,b''.join(struct.pack('<hh',x,y) for x,y in pts))
  if clip:u.mem_write(A+0x2f0,struct.pack('<4h',*clip))
  fp=lambda o:struct.pack('<HH',o,S)
  args=(fp(0x2f0) if clip else b'\0\0\0\0')+fp(0x240)+b'\0\0\0\0'+fp(0x200)+fp(0x300)+struct.pack('<HH',len(pts),18)+fp(0x100)
  self.ops=[]
  return self.fe.call('Output',args,incoming_ds=self.fe.sel['stack'])
 def spans(self,style,data,rop,brush,clip=None):
  u=self.u;A=self.A;S=self.S
  u.mem_write(A+0x600,brush);u.mem_write(A+0x240,struct.pack('<hhII',rop,1,0,0));u.mem_write(A+0x800,data)
  if clip:u.mem_write(A+0x2f0,struct.pack('<4h',*clip))
  fp=lambda o:struct.pack('<HH',o,S)
  cnt=self.cnt
  args=(fp(0x2f0) if clip else b'\0\0\0\0')+fp(0x240)+fp(0x600)+b'\0\0\0\0'+fp(0x800)+struct.pack('<HH',cnt,style)+fp(0x100)
  self.ops=[]
  return self.fe.call('Output',args,incoming_ds=self.fe.sel['stack'])
 def fb(self): return bytes(self.u.mem_read(FB,self.pitch*H))
 def model(self,segs_pts,rop,color,clip=None):
  b=bytearray(self.bg);bp=self.bpp//8;mask=(1<<self.bpp)-1
  cl,ct,cr,cb=clip if clip else (0,0,W,H);cl=max(cl,0);ct=max(ct,0);cr=min(cr,W);cb=min(cb,H)
  c={13:color&mask,4:(~color)&mask,1:0,7:color&mask}[rop]
  for i in range(len(segs_pts)-1):
   for x,y in line_model(*segs_pts[i],*segs_pts[i+1]):
    if not(cl<=x<cr and ct<=y<cb):continue
    o=y*self.pitch+x*bp;v=int.from_bytes(b[o:o+bp],'little')
    v=(v^c) if rop==7 else c
    b[o:o+bp]=v.to_bytes(bp,'little')
  return bytes(b)

def main():
 r=Rig();fails=0;cases=0
 rng=random.Random(507)
 for bpp in (8,16,32):
  for rop in (13,4,1,7):
   for trial in range(12):
    r.setup(bpp)
    n=rng.randint(2,6);pts=[]
    while len(pts)<n:
     p=(rng.randrange(W),rng.randrange(H))
     if pts and p==pts[-1]:continue
     pts.append(p)
    clip=None
    if trial%3==1:clip=(rng.randrange(0,W//2),rng.randrange(0,H//2),rng.randrange(W//2,W+20),rng.randrange(H//2,H+20))
    if trial%3==2:clip=(40,30,41+rng.randrange(200),31+rng.randrange(120))
    col=rng.getrandbits(32)
    try: rv=r.output(pts,rop,col,clip)
    except AssertionError as e: rv=-1; print('declined to the old path:',bpp,rop,pts,clip,str(e)[:60])
    cases+=1
    want=r.model(pts,rop,col,clip);got=r.fb()
    if rv!=1 or got!=want or r.ops:
     fails+=1;print('FAIL bpp',bpp,'rop',rop,pts,clip,'ret',rv,'ops',[hex(o) for o in r.ops],'diff bytes',sum(a!=b for a,b in zip(got,want)))
 # every octant and the short and axis cases, 32 bpp
 r.setup(32)
 for dx in range(-9,10):
  for dy in range(-9,10):
   if not dx and not dy:continue
   r.setup(32);pts=[(100,100),(100+dx*7,100+dy*5)];rv=r.output(pts,13,0x123456);cases+=1
   if r.fb()!=r.model(pts,13,0x123456):fails+=1;print('FAIL octant',dx,dy)
 # the cursor handshake: drawn cursor -> the exclusion calls; a put-off show -> released after the drawing
 r.setup(32,cur=3);rv=r.output([(10,10),(60,40)],13,0xFF);cases+=1
 if r.ops!=[OP_RECT,OP_END] or r.fb()!=r.model([(10,10),(60,40)],13,0xFF):fails+=1;print('FAIL drawn cursor: ops',[hex(o) for o in r.ops])
 r.setup(32,cur=5);rv=r.output([(10,10),(60,40)],13,0xFF);cases+=1
 if r.ops!=[OP_RECT,OP_END]:fails+=1;print('FAIL put-off show not released: ops',[hex(o) for o in r.ops])
 busy=struct.unpack('<H',bytes(r.u.mem_read(r.A+0x400+256,2)))[0]
 if busy:fails+=1;print('FAIL busy count left',busy)
 r.setup(32,feat=0x8001);rv=r.output([(10,10),(60,40)],13,0xFF);cases+=1
 if r.ops!=[OP_RECT,OP_END]:fails+=1;print('FAIL ACCEL_GDIFAST 0 should bracket: ops',[hex(o) for o in r.ops])
 # what the path does not take: ACCEL_CPULINE 0, R2_NOT, a styled pen -> nothing drawn here (DIBENG's stub)
 for feat,rop,style in ((0x8002,13,0),(0x8003,6,0),(0x8003,13,5)):
  r.setup(16,feat=feat);rv=r.output([(10,10),(200,40)],rop,0xFF,style=style);cases+=1
  if r.fb()!=r.bg:fails+=1;print('FAIL not taken',hex(feat),rop,style)
 # DIBENG's access hooks (ACCEL_GDIFAST): no VxD call while the cursor state allows it, the calls otherwise, paired, the busy count back to 0
 def acc(flags,begin):
  if begin:a=struct.pack('<HHHHHHH',flags,50,60,10,20,0x100,r.S);r.ops=[];r.fe.call('BEGINACCESS_VXD',a,incoming_ds=r.fe.sel['stack'])
  else:a=struct.pack('<HHH',flags,0x100,r.S);r.ops=[];r.fe.call('ENDACCESS_VXD',a,incoming_ds=r.fe.sel['stack'])
  return list(r.ops)
 r.u.mem_write(0x20000+r.sym['_mouse_vxd'][1],struct.pack('<H',1))
 for feat,cur,want in ((0x8003,1,[]),(0x8001,1,[OP_RECT]),(0x8003,3,[OP_RECT]),(0x8003,0,[OP_RECT])):
  r.setup(32,feat=feat,cur=cur)
  if cur==0:r.u.mem_write(r.A+0x400+260,b'\0\0\0\0')  # no 'CU' tag: an older VxD
  b=acc(8,1);mid=struct.unpack('<H',bytes(r.u.mem_read(r.A+0x400+256,2)))[0];e=acc(8,0);cases+=1
  wend=[] if not want else [OP_END]
  busy=struct.unpack('<H',bytes(r.u.mem_read(r.A+0x400+256,2)))[0]
  if b!=want or e!=wend or busy or mid!=(0 if want else 1):fails+=1;print('FAIL access',hex(feat),cur,[hex(o) for o in b],[hex(o) for o in e],'busy during',mid,'after',busy)
 r.setup(32);b=acc(8,1);r.set_cur(5);e=acc(8,0);cases+=1  # a show put off while DIBENG drew: released at the end
 if b or e!=[OP_RECT,OP_END]:fails+=1;print('FAIL put-off show at EndAccess',[hex(o) for o in b],[hex(o) for o in e])
 # .574: horizontal pen-copy segments as one string store: both directions, every length to 40 and some long, odd and even starts,
 # from the first column and row (a leftward one ending at the surface's first pixel), alone and inside polylines, against the model
 for bpp in (8,16,32):
  for rop in (13,4,1,7):
   hz=[[(x0,y),(x0+d*n,y)] for n in list(range(1,41))+[97,160,255] for x0,y,d in ((3,7,1),(4,8,1),(300,9,-1),(299,11,-1)) if 0<=x0+d*n<W]
   hz+=[[(40,0),(0,0)],[(0,0),(40,0)],[(W-1,H-1),(W-41,H-1)],[(5,5),(60,5),(60,40),(20,40),(20,41),(90,41),(88,41),(88,60),(10,60)],[(100,50),(50,50),(51,51),(120,51),(119,52),(30,52)]]
   for pts in hz:
    r.setup(bpp);col=rng.getrandbits(32);rv=r.output(pts,rop,col);cases+=1
    if rv!=1 or r.fb()!=r.model(pts,rop,col) or r.ops:fails+=1;print('FAIL horizontal',bpp,rop,pts,rv,r.ops)
 r.setup(32)
 begins=[acc(8,1) for _ in range(18)]
 ends=[acc(8,0) for _ in range(18)]
 busy=struct.unpack('<H',bytes(r.u.mem_read(r.A+0x400+256,2)))[0]
 cases+=1
 if begins!=[[]]*16+[[OP_RECT]]*2 or ends!=[[OP_END]]*2+[[]]*16 or busy:
  fails+=1;print('FAIL nested access pairing',begins,ends,'busy after',busy)
 # v10.508: long pen-copy segments (ACCEL_CPULINE_MAX) go to the engine; XOR and short ones stay here; an engine SKIP is drawn here
 for lmx,lmy in ((384,112),(64,160),(0,0)):
  for rc in (SKIP,1):
   r.setup(32,feat=0x8003|((lmx//16)<<2)|((lmy//16)<<8)|(0x4000 if not lmx and not lmy else 0));r.submit_rc=rc
   pts=[(10,10),(300,40),(310,50),(20,190),(25,185),(25,40),(200,60)]
   rv=r.output(pts,13,0x00ABCDEF);cases+=1
   longs=[]
   for i in range(len(pts)-1):
    adx,ady=abs(pts[i+1][0]-pts[i][0]),abs(pts[i+1][1]-pts[i][1])
    if (ady>adx and ady>=lmy) or (ady<=adx and adx>=lmx):longs.append(i)
   if rc==SKIP:want=r.model(pts,13,0x00ABCDEF)
   else:
    b=bytearray(r.bg)
    for i in range(len(pts)-1):
     if i in longs:continue
     for x,y in line_model(*pts[i],*pts[i+1]):o=y*r.pitch+x*4;b[o:o+4]=(0x00ABCDEF).to_bytes(4,'little')
    want=bytes(b)
   if r.ops.count(0x1132)!=len(longs) or r.fb()!=want:fails+=1;print('FAIL engine segments',lmx,lmy,'rc',rc,[hex(o) for o in r.ops],'longs',longs)
  r.setup(32,feat=0x8003|((lmx//16)<<2)|((lmy//16)<<8));r.submit_rc=1;pts=[(10,10),(300,40)];r.output(pts,7,0x123456);cases+=1
  if 0x1132 in r.ops or r.fb()!=r.model(pts,7,0x123456):fails+=1;print('FAIL XOR long must stay on the CPU')
 r.submit_rc=SKIP
 # v10.509: ACCEL_CPUSPAN is gone: scan lines go the old way (here DIBENG's stub), nothing drawn by the driver
 r.setup(16,feat=0x8007);data=struct.pack('<hh',2,50)+struct.pack('<hh',10,200);r.cnt=2;rv=r.spans(4,data,13,struct.pack('<BBHI',1,20,0,0)+bytes(406));cases+=1
 if r.fb()!=r.bg:fails+=1;print('FAIL scan lines drawn by the driver')
 # .510: clipped segments' visible runs worked out (not walked) and in-clip polylines in one call (vx_poly_run): many segments, every octant, thin and edge clips
 import math
 rng=random.Random(510)
 for bpp in (8,16,32):
  for rop in (13,7):
   for trial in range(40):
    r.setup(bpp)
    pts=[]
    while len(pts)<rng.randint(2,9):
     p=(rng.randrange(W),rng.randrange(H))
     if pts and p==pts[-1]:continue
     pts.append(p)
    k=trial%4
    if k==0:clip=(rng.randrange(W),rng.randrange(H),0,0);clip=(clip[0],clip[1],clip[0]+1+rng.randrange(W-clip[0]),clip[1]+1+rng.randrange(H-clip[1]))
    elif k==1:x=rng.randrange(W);clip=(x,0,x+1,H)          # one column
    elif k==2:y=rng.randrange(H);clip=(0,y,W,y+1)          # one row
    else:clip=(rng.randrange(-20,40),rng.randrange(-20,40),rng.randrange(W-40,W+20),rng.randrange(H-40,H+20))
    col=rng.getrandbits(32)
    rv=r.output(pts,rop,col,clip);cases+=1
    if r.fb()!=r.model(pts,rop,col,clip):fails+=1;print('FAIL clipped run',bpp,rop,pts,clip)
   for trial in range(6):  # an ellipse's outline: many short segments, inside the clip (one call)
    r.setup(bpp);cx,cy,ax,ay=rng.randrange(60,W-60),rng.randrange(50,H-50),rng.randrange(5,58),rng.randrange(5,48);pts=[]
    for i in range(97):
     p=(int(round(cx+ax*math.cos(i*2*math.pi/96))),int(round(cy+ay*math.sin(i*2*math.pi/96))))
     if not pts or p!=pts[-1]:pts.append(p)
    col=rng.getrandbits(32);clip=None if trial&1 else (0,0,W,H)
    rv=r.output(pts,rop,col,clip);cases+=1
    if r.fb()!=r.model(pts,rop,col,clip):fails+=1;print('FAIL outline',bpp,rop,len(pts))
 print('gdi_cpu_lines: %d cases, %d failures'%(cases,fails))
 return 1 if fails else 0

if __name__=='__main__':sys.exit(main())
