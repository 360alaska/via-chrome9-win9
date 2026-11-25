#!/usr/bin/env python3
"""tnl_osk_cases.py (.768B): the T&L's triangle loops with the record stride as a constant (vx_d3d_osk 1: tn_draw's and tn_ix_b's loops
for strides 40 and 32) against the stride read from memory (0, the loops as before .768B): the same triangles, vertex word for vertex word, in the same order. Lit
meshes with one and two coordinate sets (strides 32 and 40) and a third set's q (a stride the constant forms do not take), parts past
every side of the viewport (clipped, dropped, the guard band), all three cull modes, drawn plain and indexed; both constant forms must run.
usage: tnl_osk_cases.py [root] [scenes] [seed]"""
import struct,sys,random,math,ctypes as C
from pathlib import Path
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 and not sys.argv[1].isdigit() else Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tests'))
from test_core import *
nums=[a for a in sys.argv[1:] if a.isdigit()];SCENES=int(nums[0]) if nums else 160;SEED=int(nums[1]) if len(nums)>1 else 768
g=lambda n:U.in_dll(lib,n);N=(U*6).in_dll(lib,'vx_d3d_osk_n')
f2u=lambda f:struct.unpack('<I',struct.pack('<f',f))[0]
ID=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
def tris_set(t): # (tnl_opkey_cases.py's) the triangles' vertex words in drawing order: the packets' other words carry ids and generations
 o=[]
 for p in t.calls:
  assert lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
  if p.pre_on==2 and p.bursts.n:
   b0=p.bursts.b[0];sd=8 if b0&0x04000000 else 10;ni=b0&0xFFFF;w=list(p.pre_w)
   ix=struct.unpack('<%dH'%ni,bytes(p.vertices)[:ni*2])
   for k in range(0,ni-2,3):o.append(tuple(tuple(w[v*sd:(v+1)*sd]) for v in ix[k:k+3]))
  else:
   b=bytes(p.vertices);z=C.sizeof(Vertex);vs=[b[k*z:(k+1)*z]+struct.pack('<I',p.shader.spec[k]) for k in range(p.vertex_count)]
   for k in range(0,len(vs)-2,3):o.append(tuple(vs[k:k+3]))
 return o
def scene(seed):
 R=random.Random(seed);gw=R.randint(2,9);span=R.choice((1.6,2.6,4.0)) # .768B: past the sides too
 vs=[];ix=[]
 for j in range(gw+1):
  for i in range(gw+1):
   a,b=R.uniform(-1,1),R.uniform(-1,1);n=math.sqrt(a*a+b*b+1)
   vs.append(struct.pack('<6fI4f',-span/2+span*i/gw+R.uniform(-.05,.05),-span/2+span*j/gw+R.uniform(-.05,.05),R.uniform(-.1,1.1),a/n,b/n,-1/n,R.getrandbits(32),R.random(),R.random(),R.random(),R.random()))
 for j in range(gw):
  for i in range(gw):
   a=j*(gw+1)+i
   t1,t2=(a,a+gw+1,a+1),(a+1,a+gw+1,a+gw+2)
   if R.random()<.3:t1=t1[::-1] # back faces for the cull modes
   ix+=[t1,t2]
 vb=b''.join(vs);draw=cmd(26,len(ix),struct.pack('<H',0)+b''.join(struct.pack('<3H',*x) for x in ix))
 def light(i,kind):
  c=[R.uniform(0,1) for _ in range(3)];s=[R.uniform(0,1) for _ in range(3)]
  d=struct.pack('<I',kind)+struct.pack('<25f',*c,1,*s,1,.1,.1,.1,1,R.uniform(-1,1),R.uniform(-1,1),R.uniform(-2,-.5),R.uniform(-.5,.5),R.uniform(-.5,.5),1.0,1000,0,1,R.uniform(0,.5),0,.5,1.0)
  return struct.pack('<II',i,2)+d+struct.pack('<II',i,0)
 def material():
  return cmd(33,1,struct.pack('<17f',R.random(),R.random(),R.random(),R.random(),R.random(),R.random(),R.random(),1,0,0,0,1,R.random()*.3,R.random()*.3,0,1,4.0))
 nl=R.randint(1,3)
 pre=cmd(28,1,struct.pack('<4I',R.choice((0,8)),R.choice((0,8)),64,64))+cmd(36,3,struct.pack('<I',1)+struct.pack('<16f',*ID)+struct.pack('<I',2)+struct.pack('<16f',*ID)+struct.pack('<I',3)+struct.pack('<16f',*ID))
 pre+=rs((22,R.choice((1,2,3))),(136,1),(137,1),(139,0xFF202020),(28,R.choice((0,1))),(140,3),(36,f2u(.1)),(37,f2u(.9)),(35,0),(29,R.choice((0,1))))
 pre+=cmd(34,2*nl,b''.join(light(i,R.choice((1,3,3))) for i in range(nl)))+material()
 pre+=cmd(25,2,struct.pack('<HHI',0,1,4)+struct.pack('<HHI',0,11,0))
 body=b''
 for d in range(R.randint(4,10)):
  if R.random()<.5:body+=material()
  if R.random()<.5:
   m=ID[:];m[12]=R.uniform(-.6,.6);m[13]=R.uniform(-.6,.6);body+=cmd(36,1,struct.pack('<I',1)+struct.pack('<16f',*m))
  e=R.random() # the second stage on (two sets: stride 40), off (one: 32), or the third set's q (another stride)
  if e<.4:body+=cmd(25,3,struct.pack('<HHI',1,1,4)+struct.pack('<HHI',1,11,1)+struct.pack('<HHI',1,24,0))
  elif e<.8:body+=cmd(25,1,struct.pack('<HHI',1,1,1))
  else:body+=cmd(25,3,struct.pack('<HHI',1,1,4)+struct.pack('<HHI',1,11,1)+struct.pack('<HHI',1,24,0x103)) # TEXTURETRANSFORMFLAGS COUNT3 | PROJECTED
  if R.random()<.3:body+=rs((22,R.choice((1,2,3))))
  body+=draw
 return pre+body,vb
fails=0;tot=[0]*6;cases=0;ntri=0
for it in range(SCENES):
 cmds,vb=scene(SEED*1000+it)
 for indexed in (0,1):
  out=[]
  for on in (1,0):
   g('vx_d3d_osk').value=on
   t=Contract('test_tnl');t.setUp()
   for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',indexed),('vx_d3d_ixpass',indexed),('vx_d3d_indexed',indexed)]:g(n).value=v
   n0=[N[k] for k in range(6)]
   r=t.run_dp2(cmds,vb,stride=44,fvf=0x252)
   if on:
    for k in range(6):tot[k]+=N[k]-n0[k]
   out.append((r,tris_set(t)))
  cases+=1;ntri+=len(out[0][1])
  if out[0]!=out[1]:print('FAIL scene',it,'indexed',indexed,out[0][0],out[1][0],len(out[0][1]),len(out[1][1]));fails+=1
g('vx_d3d_osk').value=1
print('cases',cases,'triangles',ntri,'with OSK on: tn_draw by 40 / 32 / the stride',tot[0],tot[1],tot[2],'; tn_ix_b by 40 / 32 / the stride',tot[3],tot[4],tot[5])
if not (ntri>cases*20 and tot[0] and tot[1] and tot[2] and tot[3] and tot[4]):print('FAIL a loop form never ran');fails+=1
print('failures',fails);sys.exit(1 if fails else 0)
