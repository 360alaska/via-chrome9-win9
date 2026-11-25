#!/usr/bin/env python3
"""tnl_split_cases.py (.765): non-indexed T&L draws large enough to be taken in pieces, past index 65535 (outside audit of .757, item 2):
a TRIANGLELIST of 22,000 triangles (66,000 vertices), a TRIANGLESTRIP and a TRIANGLEFAN of 65,535 triangles, each drawn whole and
against the same triangles drawn as smaller commands over the matching slice of the vertex buffer: the same triangles word for word,
in the same order (strips and fans in indexed packets). usage: tnl_split_cases.py [root]"""
import struct,sys,math,ctypes as C
from pathlib import Path
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tests'))
from test_core import *
g=lambda n:U.in_dll(lib,n)
ID=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
def tris_set(t):
 o=[]
 for p in t.calls:
  if p.pre_on==2 and p.bursts.n:
   b0=p.bursts.b[0];sd=8 if b0&0x04000000 else 10;ni=b0&0xFFFF;w=list(p.pre_w)
   ix=struct.unpack('<%dH'%ni,bytes(p.vertices)[:ni*2])
   for k in range(0,ni-2,3):
    tri=[tuple(w[v*sd:(v+1)*sd]) for v in ix[k:k+3]];m=tri.index(min(tri));o.append(tuple(tri[m:]+tri[:m]))
  else:
   b=bytes(p.vertices);z=C.sizeof(Vertex);vs=[b[k*z:(k+1)*z]+struct.pack('<I',p.shader.spec[k]) for k in range(p.vertex_count)]
   for k in range(0,len(vs)-2,3):
    tri=vs[k:k+3];m=tri.index(min(tri));o.append(tuple(tri[m:]+tri[:m]))
 return o
def vtx(x,y,k):return struct.pack('<6fI2f',x,y,.5,0,0,-1,0xFF000000|(k*2654435761&0xFFFFFF),(k%97)/97.,(k%89)/89.)
pre=cmd(28,1,struct.pack("<4I",0,0,64,64))+cmd(36,3,struct.pack('<I',1)+struct.pack('<16f',*ID)+struct.pack('<I',2)+struct.pack('<16f',*ID)+struct.pack('<I',3)+struct.pack('<16f',*ID))
pre+=rs((22,1),(136,1),(137,0),(28,0),(29,0))
def run(cmds,vb,indexed):
 t=Contract('test_tnl');t.setUp()
 for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',indexed),('vx_d3d_ixpass',indexed),('vx_d3d_indexed',indexed)]:g(n).value=v
 r=t.run_dp2(pre+cmds,vb,stride=36,fvf=0x152);return r,tris_set(t)
fails=0
def check(name,whole,parts,modes=(0,1)):
 global fails
 for indexed in modes:
  r,a=run(whole[0],whole[1],indexed);b=[]
  for c,v in parts:
   r2,x=run(c,v,indexed);b+=x
   if r2!=(0,0):print('FAIL',name,'part',r2);fails+=1
  ok=r==(0,0) and a==b and len(a)>0
  print(name,'indexed' if indexed else 'plain',r,len(a),len(b),'same' if a==b else 'DIFFERENT')
  if not ok:fails+=1
# a list of 22,000 triangles: the whole against 20,000 + 2,000 over the slices
N=22000;L=[vtx(-.9+1.8*((k//3)%150)/150+.004*(k%3),-.9+1.8*((k//3)//150)/150+.004*((k%3)==2),k) for k in range(N*3)];vb=b''.join(L)
check('list 22000',(cmd(18,N,struct.pack('<H',0)),vb),[(cmd(18,20000,struct.pack('<H',0)),b''.join(L[:60000])),(cmd(18,2000,struct.pack('<H',0)),b''.join(L[60000:]))])
# a strip of 65,535 triangles: the whole against two strips (the second's first triangle is triangle 40,000, even: the same winding)
N=65535;S=[vtx(-.9+1.8*((k//2)%200)/200,-.9+1.8*(((k//2)//200)%180)/180+.003*(k&1),k) for k in range(N+2)];vb=b''.join(S)
check('strip 65535',(cmd(19,N,struct.pack('<H',0)),vb),[(cmd(19,40000,struct.pack('<H',0)),b''.join(S[:40002])),(cmd(19,N-40000,struct.pack('<H',0)),b''.join(S[40000:]))],(1,)) # plain packets carry strips and fans as strip bursts (not read back here): indexed only
# a fan of 65,535 triangles: the whole against a fan of 40,000 and one of the rest over [centre] + the rest of the rim
N=65535;F=[vtx(0,0,0)]+[vtx(.8*math.cos(k*.0001),.8*math.sin(k*.0001),k) for k in range(1,N+2)];vb=b''.join(F)
check('fan 65535',(cmd(21,N,struct.pack('<H',0)),vb),[(cmd(21,40000,struct.pack('<H',0)),b''.join(F[:40002])),(cmd(21,N-40000,struct.pack('<H',0)),F[0]+b''.join(F[40001:]))],(1,))
print('failures',fails);sys.exit(1 if fails else 0)
