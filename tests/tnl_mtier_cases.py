#!/usr/bin/env python3
"""tnl_mtier_cases.py (.756): the T&L's material-only set-up (vx_d3d_tn_matonly_on 1) against a full set-up after every material
change (0): the same triangles word for word. Lit meshes drawn many times in one context, each draw after a new material; between
some draws the world matrix, a light or the specular render state changes too, and some materials turn the specular colour to or from
black (the set-up must then be built in full). usage: tnl_mtier_cases.py [root] [scenes] [seed]"""
import struct,sys,random,math,ctypes as C
from pathlib import Path
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 and not sys.argv[1].isdigit() else Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tests'))
from test_core import *
nums=[a for a in sys.argv[1:] if a.isdigit()];SCENES=int(nums[0]) if nums else 120;SEED=int(nums[1]) if len(nums)>1 else 756
g=lambda n:U.in_dll(lib,n);MO=(U*2).in_dll(lib,'vx_d3d_tn_matonly');SU=(U*3).in_dll(lib,'vx_d3d_tn_setup')
f2u=lambda f:struct.unpack('<I',struct.pack('<f',f))[0]
ID=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
def tris_set(t):
 o=[]
 for p in t.calls:
  assert lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
  if p.pre_on==2 and p.bursts.n:
   b0=p.bursts.b[0];sd=8 if b0&0x04000000 else 10;ni=b0&0xFFFF;w=list(p.pre_w)
   ix=struct.unpack('<%dH'%ni,bytes(p.vertices)[:ni*2])
   for k in range(0,ni-2,3):
    tri=[tuple(w[v*sd:(v+1)*sd]) for v in ix[k:k+3]];m=tri.index(min(tri));o.append(tuple(tri[m:]+tri[:m]))
  else:
   b=bytes(p.vertices);z=C.sizeof(Vertex);vs=[b[k*z:(k+1)*z]+struct.pack('<I',p.shader.spec[k]) for k in range(p.vertex_count)]
   for k in range(0,len(vs)-2,3):
    tri=vs[k:k+3];m=tri.index(min(tri));o.append(tuple(tri[m:]+tri[:m]))
 return o # in drawing order: the two ways draw the same commands
def scene(seed):
 R=random.Random(seed);gw=R.randint(2,8)
 vs=[];ix=[]
 for j in range(gw+1):
  for i in range(gw+1):
   a,b=R.uniform(-1,1),R.uniform(-1,1);n=math.sqrt(a*a+b*b+1)
   vs.append(struct.pack('<6fI2f',-.8+1.6*i/gw,-.8+1.6*j/gw,R.uniform(.2,.8),a/n,b/n,-1/n,R.getrandbits(32),R.random(),R.random()))
 for j in range(gw):
  for i in range(gw):
   a=j*(gw+1)+i;ix+=[(a,a+gw+1,a+1),(a+1,a+gw+1,a+gw+2)]
 vb=b''.join(vs);draw=cmd(26,len(ix),struct.pack('<H',0)+b''.join(struct.pack('<3H',*x) for x in ix))
 def light(i,kind):
  c=[R.uniform(0,1) for _ in range(3)];s=[R.uniform(0,1) for _ in range(3)]
  d=struct.pack('<I',kind)+struct.pack('<25f',*c,1,*s,1,.1,.1,.1,1,R.uniform(-1,1),R.uniform(-1,1),R.uniform(-2,-.5),R.uniform(-.5,.5),R.uniform(-.5,.5),1.0,1000,0,1,R.uniform(0,.5),0,.5,1.0)
  return struct.pack('<II',i,2)+d+struct.pack('<II',i,0)
 def material():
  spec=R.choice(((0,0,0),(0,0,0),(R.random(),R.random(),R.random())))
  return cmd(33,1,struct.pack('<17f',R.random(),R.random(),R.random(),R.random(),R.random(),R.random(),R.random(),1,*spec,1,R.random()*.3,R.random()*.3,0,1,R.choice((0.0,4.0,20.0))))
 nl=R.randint(1,3)
 pre=cmd(28,1,struct.pack('<4I',0,0,64,64))+cmd(36,3,struct.pack('<I',1)+struct.pack('<16f',*ID)+struct.pack('<I',2)+struct.pack('<16f',*ID)+struct.pack('<I',3)+struct.pack('<16f',*ID))
 pre+=rs((22,1),(136,1),(137,1),(139,0xFF202020),(28,R.choice((0,1))),(140,3),(36,f2u(.1)),(37,f2u(.9)),(35,0),(29,R.choice((0,1))),(142,R.choice((0,1))),(141,R.choice((0,1))),(145,R.choice((0,1))),(147,R.choice((0,1))),(148,R.choice((0,0,1))),(146,R.choice((0,0,1)))) # .757: and material colours taken from the vertex (the kernels' own material constants)
 pre+=cmd(34,2*nl,b''.join(light(i,R.choice((1,3,3))) for i in range(nl)))+material()
 body=b''
 for d in range(R.randint(6,14)):
  e=R.random()
  if e<.75:body+=material()
  if R.random()<.5:
   m=ID[:];m[12]=R.uniform(-.3,.3);m[13]=R.uniform(-.3,.3);body+=cmd(36,1,struct.pack('<I',1)+struct.pack('<16f',*m))
  if e>.9:body+=cmd(34,2,light(0,3))
  if .8<e<.86:body+=rs((29,R.choice((0,1))))
  if R.random()<.15:body+=material() # two in a row
  body+=draw
 return pre+body,vb
fails=0;tier=0;fell=0
for it in range(SCENES):
 cmds,vb=scene(SEED*1000+it);out=[]
 for on in (0,1):
  for indexed in (0,1):
   g('vx_d3d_tn_matonly_on').value=on;MO[0]=MO[1]=0
   t=Contract('test_tnl');t.setUp()
   for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',indexed),('vx_d3d_ixpass',indexed),('vx_d3d_indexed',indexed)]:g(n).value=v
   r=t.run_dp2(cmds,vb,stride=36,fvf=0x152);out.append((r,tris_set(t),MO[0],MO[1]))
 ok=all(o[0]==(0,0) for o in out) and out[0][1]==out[2][1] and out[1][1]==out[3][1] and out[0][2]==0 and len(out[0][1])>0
 tier+=out[2][2];fell+=out[2][3]
 if not ok:print('FAIL scene',it,[o[0] for o in out],[len(o[1]) for o in out]);fails+=1
g('vx_d3d_tn_matonly_on').value=1
print('scenes',SCENES,'set-ups made again for a material only',tier,'materials that switched specular lighting',fell)
if tier<SCENES*2 or not fell:print('FAIL the material-only path or its fall to a full build was not reached');fails+=1
print('failures',fails);sys.exit(1 if fails else 0)
