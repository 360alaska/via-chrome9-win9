"""proj2_cases.py (.656): a projected texture set with two texture units (board, VXTLP 6i on .653-.655: stage 1's set projected
(TEXTURETRANSFORMFLAGS COUNT3 | PROJECTED) with two units drew undivided). Pretransformed vertices with a three-coordinate set: unit 1
projected on set 1 (q in r1, s3 bit 1, ulayout PROJ, unit 1's TAU word with the divide bits, FE/01 byte 1 S T R from set B), unit 0
projected on set 0 with a second unit (q in stages.q, s3 bit 0, layout PROJ, unit 0's projected word 307001A0); every packet validated
and executed; a plain two-unit draw keeps its words."""
import sys,struct,ctypes as C,unittest
sys.path.insert(0,__file__.rsplit('/',1)[0])
from test_core import *
fails=0
def bits(f):return struct.unpack('<I',struct.pack('<f',f))[0]
class T(Contract):
 def runTest(self):pass
def run(name,ttf0,ttf1,fvf,vb,stride,want):
 global fails
 t=T();t.setUp();t.calls.clear()
 for k in range(8):
  s=t.context.state.tss[k];s[0]=0;s[1]=1;s[4]=1
 for k,(ttf,tci) in enumerate(((ttf0,0),(ttf1,1))):
  s=t.context.state.tss[k];s[0]=5;s[1]=4;s[2]=2;s[3]=1 if k else 0;s[4]=2;s[5]=2;s[11]=tci;s[24]=ttf
 r=t.run_dp2(t.tri(),vb,stride=stride,fvf=fvf)
 ok=r==(0,0) and len(t.calls)==1
 if ok:
  p=t.calls[0];ok=lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
  if ok and p.stages.ulayout[0]: # .705 (a review of .703): unit 1's set out of range refused, its bit shift no longer reached (32 and up were undefined in C)
   sv=p.stages.set[1]
   for bad in (2,31,32,63,255,0xFFFFFFFF):
    p.stages.set[1]=bad
    if lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0:ok=False;print('   ',name,'set',bad,'taken')
   p.stages.set[1]=sv
  tr=[];refs=(READ(lambda _,r:0),WRITE(lambda _,r,v:tr.append((r,v))),READ(lambda _,r:0),WRITE(lambda _,r,v:None),TIME(lambda _:0))
  io=IO(None,*refs);last=U();ok=ok and lib.vx3dn_execute(C.byref(p),C.byref(io),C.byref(last))==0
  w=[v for a,v in tr if a==0x440]
  got=dict(s3=p.stages.s3,layout=p.stages.layout,ul0=p.stages.ulayout[0],units=p.stages.units,
           q=[p.stages.q[k] for k in range(3)],r1=[p.stages.r1[k] for k in range(3)],
           tau1=[x for x in w if x>>24==0x31],tau0=[x for x in w if x>>24==0x30],fe=[x for x in w if x>>24==1 and (x&0xFF0000)==0x010000])
  for k,v in want.items():
   if got[k]!=v and not (k in('tau0','tau1') and v[0] in got[k]):ok=False;print('   ',name,k,'got',[hex(x) for x in got[k]] if isinstance(got[k],list) else got[k],'want',v)
 U.in_dll(lib,'vx_d3d_qfold').value=1
 print(('ok   ' if ok else 'FAIL ')+name);fails+=not ok
pts=[(8,8,0.,0.,2.),(56,8,1.,0.,2.),(8,56,0.,1.,4.)]
# FVF: XYZRHW 4 | DIFFUSE 40 | SPECULAR 80 | TEX2 200, set 1 with three coordinates (TEXCOORDSIZE3(1) = 1 << 18)
vb=b''.join(struct.pack('<4fII2f3f',x,y,.5,1.,0xA0C08040,0xFF204060,u,v,v,u,q) for x,y,u,v,q in pts)
run('unit 1 projected on set 1',0,0x103,0x2C4|(1<<18),vb,44,dict(units=2,s3=2,ul0=2,layout=0,r1=[bits(2.),bits(2.),bits(4.)],tau1=[0x317111A1]))
vb0=b''.join(struct.pack('<4fII3f2f',x,y,.5,1.,0xA0C08040,0xFF204060,u,v,q,v,u) for x,y,u,v,q in pts)
run('unit 0 projected on set 0, a second unit',0x103,0,0x2C4|(1<<16),vb0,44,dict(units=2,s3=1,layout=2,q=[bits(2.),bits(2.),bits(4.)],tau0=[0x307001A0]))
vbp=b''.join(struct.pack('<4fII4f',x,y,.5,1.,0xA0C08040,0xFF204060,u,v,v,u) for x,y,u,v,q in pts)
run('two units, neither projected (as before)',0,0,0x2C4,vbp,40,dict(units=2,s3=0,ul0=0,layout=0,tau1=[0x311111B1]))
# the driver's T&L (untransformed vertices): a stage projected with COUNT3 keeps q in its record and TTF 103 for the HAL, so the packet
# carries the projected layout and q, the coordinates undivided (q = the eye-space z through the texture matrix's third column)
IDM=struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
TM=struct.pack('<16f',.75,0,0,0, 0,-1,0,0, .5,.5,1,0, 0,0,0,1)
TM4=struct.pack('<16f',.75,0,0,0, 0,-1,0,0, .5,.5,1,1, 0,0,0,0) # .726: COUNT4, the fourth component z (q), the third 1
def tnl(name,stage1,want,fold=0,ttf=0x103):
 global fails
 t=T();t.setUp();U.in_dll(lib,'vx_d3d_qfold').value=fold # .724: the projected layout itself (fold 0), or q folded into W
 for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',1),('vx_d3d_ixpass',1),('vx_d3d_indexed',1)]:U.in_dll(lib,n).value=v
 vb=b''.join(struct.pack('<6fI2f',x,y,z,0,0,-1,0xFFFFFFFF,.1,.2) for x,y,z in ((-.5,-.5,.4),(.5,-.5,.6),(-.5,.5,.8)))
 ent=[(0,0,5),(0,1,4),(0,2,2),(0,3,0)]
 if stage1:ent+=[(0,11,0),(0,24,0),(1,0,5),(1,1,4),(1,2,2),(1,3,1),(1,11,0x20000),(1,24,0x103)]
 else:ent+=[(0,11,0x20000),(0,24,ttf),(1,1,1)]
 pre=cmd(28,1,struct.pack('<4I',0,0,64,64))+cmd(36,4,struct.pack('<I',1)+IDM+struct.pack('<I',2)+IDM+struct.pack('<I',3)+IDM+struct.pack('<I',16 if not stage1 else 17)+(TM4 if ttf==0x104 else TM))
 pre+=rs((22,1),(136,1),(137,0),(28,0))+cmd(25,len(ent),b''.join(struct.pack('<HHI',*e) for e in ent))
 t.calls.clear();r=t.run_dp2(pre+cmd(18,1,struct.pack('<H',0)),vb,stride=36,fvf=0x152)
 ok=r==(0,0) and len(t.calls)==1
 if ok:
  p=t.calls[0];ok=lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
  qs=[struct.unpack('<f',struct.pack('<I',(p.stages.r1 if stage1 else p.stages.q)[k]))[0] for k in range(3)]
  us=[struct.unpack('<f',struct.pack('<I',(p.stages.uv1[k][0] if stage1 else p.tex.uv[k][0])))[0] for k in range(3)]
  got=dict(s3=p.stages.s3,layout=p.stages.layout,ul0=p.stages.ulayout[0],units=p.stages.units)
  for k,v in want.items():
   if got[k]!=v:ok=False;print('   ',name,k,'got',got[k],'want',v)
  # q is the vertex's eye z (.4 .6 .8); u = .75 x + .5 z undivided
  for k,(x,z) in enumerate(((-.5,.4),(.5,.6),(-.5,.8))):
   wq,wu=(1.0,(.75*x+.5*z)/z) if fold else (z,.75*x+.5*z) # .724: folded, u / q with q 1
   if abs(qs[k]-wq)>1e-5 or abs(us[k]-wu)>1e-5:ok=False;print('   ',name,'vertex',k,'q',qs[k],'u',us[k])
 print(('ok   ' if ok else 'FAIL ')+name);fails+=not ok
tnl("the driver's T&L: stage 1 projected (camera-space position, COUNT3 PROJECTED)",1,dict(units=2,s3=2,ul0=2,layout=0))
tnl("the driver's T&L: stage 0 projected, one stage",0,dict(layout=2))  # one unit: the packet's stages block is not used (units 0)
tnl("the driver's T&L: stage 0 projected, one stage, q folded into W (.724)",0,dict(layout=0),fold=1)
tnl("the driver's T&L: stage 0 COUNT4 PROJECTED, q kept (.726)",0,dict(layout=2),ttf=0x104)
tnl("the driver's T&L: stage 0 COUNT4 PROJECTED, q folded into W (.726)",0,dict(layout=0),fold=1,ttf=0x104)
print('FAILED' if fails else 'PASS');sys.exit(1 if fails else 0)
