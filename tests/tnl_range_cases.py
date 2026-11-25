#!/usr/bin/env python3
"""tnl_range_cases.py (R1, the T&L reworked): the range path of the driver's T&L (vx_d3d_tn_range_on 1: a draw's vertex range through
tnq_pos, tnq_cull, tnq_lit and tnq_attr, its records into the packet by tn_ix_b's range stage) against the gather and the clipper (0),
on random scenes: meshes inside the viewport, across its edges inside the guard band, across the band, across the near plane and
wholly outside; perspective and orthographic projections, viewports off the origin; cull none / CW / CCW; unlit formats of 12 to 32
bytes, lit with 1-3 directional lights (the vertex colour as the material or not, NORMALIZENORMALS under a uniform and a non-uniform
world scale), fog; one to four draws a call with and without a world matrix between them; meshes larger than a packet; the vertex
buffer ending at the draw's last vertex; a texture bound or not (the packets that take the records' words as they are, or converted),
one or two coordinate sets, the guard band on and off. Every triangle sent must be the same: position and coordinate words exactly, colour bytes
within 1 (the fog factor of a range draw comes from the position, not from the clip array). Triangles wholly beyond one side of the
viewport are left out of the comparison: the range path always drops them, the gather only when an eighth of the draw's vertices are
past the viewport (.703), and they draw nothing. And giving the places back
(vx_d3d_tn_rewind_on) must change no word at all.
  python3 tests/tnl_range_cases.py [root] [scenes] [seed]"""
import struct,sys,random,math
from pathlib import Path
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 and not sys.argv[1].isdigit() else Path(__file__).resolve().parents[1]
nums=[int(a) for a in sys.argv[1:] if a.isdigit()]
SCENES=nums[0] if nums else 400;SEED=nums[1] if len(nums)>1 else 765
sys.path.insert(0,str(root/'tests'))
from test_core import *
R=random.Random(SEED);fails=0
import os
ONLY=int(os.environ['TNR_ONLY']) if 'TNR_ONLY' in os.environ else None # one scene of the run (the others made and not drawn)
RG=(U*9).in_dll(lib,'vx_d3d_tn_range');RON=U.in_dll(lib,'vx_d3d_tn_range_on');RWD=U.in_dll(lib,'vx_d3d_tn_rewind_on')
f2u=lambda f:struct.unpack('<I',struct.pack('<f',f))[0]
def ctx():
 t=Contract('test_tnl');t.setUp()
 for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',1),('vx_d3d_ixpass',1),('vx_d3d_indexed',1)]:U.in_dll(lib,n).value=v
 return t
u2f=lambda u:struct.unpack('<f',struct.pack('<I',u))[0]
def tris(t,vp=None):
 """every triangle sent, as its three vertices' words (8 or 10 each), turned so that the vertex with the smallest position and
 coordinate words comes first (the winding kept), sorted by those words"""
 o=[]
 for p in t.calls:
  assert lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
  if p.pre_on==2 and p.bursts.n:
   b0=p.bursts.b[0];sd=8 if b0&0x04000000 else 10;ni=b0&0xFFFF;w=list(p.pre_w)
   ix=struct.unpack('<%dH'%ni,bytes(p.vertices)[:ni*2])
   for k in range(0,ni-2,3):
    tri=[tuple(w[v*sd:(v+1)*sd]) for v in ix[k:k+3]];key=[v[:4]+v[6:] for v in tri];m=key.index(min(key))
    if vp:
     xs=[u2f(v[0]) for v in tri];ys=[u2f(v[1]) for v in tri]
     if max(xs)<vp[0] or min(xs)>vp[2] or max(ys)<vp[1] or min(ys)>vp[3]:continue
    o.append((tuple(key[m:]+key[:m]),tuple(tri[m:]+tri[:m])))
  else:
   b=bytes(p.vertices);z=C.sizeof(Vertex);vs=[b[k*z:(k+1)*z]+struct.pack('<I',p.shader.spec[k]) for k in range(p.vertex_count)]
   for k in range(0,len(vs)-2,3):
    tri=vs[k:k+3];m=tri.index(min(tri));o.append((tuple(tri[m:]+tri[:m]),None))
 return sorted(o,key=lambda x:x[0])
def near(a,b):
 """the same triangles: keys equal, colour and specular bytes within 1"""
 if len(a)!=len(b):return 'triangles %d against %d'%(len(a),len(b))
 for x,y in zip(a,b):
  if x[0]!=y[0]:return 'a position or coordinate word differs'
  if x[1] is None or y[1] is None:
   if x[1]!=y[1]:return 'packet kinds differ'
   continue
  for u,v in zip(x[1],y[1]):
   for wi in (4,5):
    for sh in (0,8,16,24):
     if abs(((u[wi]>>sh)&255)-((v[wi]>>sh)&255))>1:return 'a colour byte differs by more than 1 (%08X against %08X)'%(u[wi],v[wi])
 return None
def mul(a,b):return [sum(a[i*4+k]*b[k*4+j] for k in range(4)) for i in range(4) for j in range(4)]
def rot(ax,ang):
 c,s=math.cos(ang),math.sin(ang);m=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1];i,j=[(1,2),(0,2),(0,1)][ax]
 m[i*4+i]=c;m[i*4+j]=s;m[j*4+i]=-s;m[j*4+j]=c;return m
def scene(n):
 persp=R.random()<.7;lit=R.random()<.5;cull=R.choice([1,1,2,3]);big=R.random()<.12
 kind=R.choice(['in','in','in','edge','edge','band','near','out','mix'])
 vx,vy,vw,vh=R.choice([(0,0,64,64),(0,0,64,64),(8,4,40,50),(3,0,61,32)])
 # the world: a rotation, a scale (uniform, or not: the eye-space kernels with NORMALIZENORMALS), a place in front of the eye
 sc=[R.uniform(.5,2)]*3 if R.random()<.7 else [R.uniform(.5,2) for _ in range(3)]
 W=mul([sc[0],0,0,0,0,sc[1],0,0,0,0,sc[2],0,0,0,0,1],mul(rot(0,R.uniform(-.6,.6)),mul(rot(1,R.uniform(-.6,.6)),rot(2,R.uniform(-3,3)))))
 if persp:
  zn,zf=.5,60.;Q=zf/(zf-zn);P=[1.5,0,0,0,0,1.5,0,0,0,0,Q,1,0,0,-Q*zn,0];zc=R.uniform(6,20)
 else:
  P=[.2,0,0,0,0,.2,0,0,0,0,.02,0,0,0,.3,1];zc=R.uniform(0,10)
 off={'in':(0,0,0),'edge':(R.choice([-1,1])*R.uniform(.6,1.2),R.uniform(-.4,.4),0),'band':(R.choice([-1,1])*R.uniform(1.6,2.6),R.choice([-1,1])*R.uniform(0,2.4),0),
  'near':(R.uniform(-.3,.3),R.uniform(-.3,.3),-1),'out':(R.choice([-1,1])*R.uniform(4,6),0,0),'mix':(R.uniform(-1.2,1.2),R.uniform(-1.2,1.2),0)}[kind]
 if persp:tx,ty,tz=off[0]*zc/1.5,off[1]*zc/1.5,(zc if off[2]==0 else R.uniform(-.5,1.5))
 else:tx,ty,tz=off[0]*5,off[1]*5,(zc if off[2]==0 else -20)
 W[12],W[13],W[14]=tx,ty,tz
 gw,gh=(R.randint(20,26),R.randint(20,26)) if big else (R.randint(1,9),R.randint(1,9));ext=R.uniform(.4,1.6)*(1.0 if persp else 2.5)
 vs=[];ix=[]
 for j in range(gh+1):
  for i in range(gw+1):vs.append((ext*(2*i/gw-1),ext*(2*j/gh-1),R.uniform(-.3,.3)*ext))
 for j in range(gh):
  for i in range(gw):
   a=j*(gw+1)+i;q=[(a,a+gw+1,a+1),(a+1,a+gw+1,a+gw+2)]
   if R.random()<.5:q=[(x[0],x[2],x[1]) for x in q]
   ix+=q
 if R.random()<.3:ix=[x for x in ix if R.random()<.7] or ix[:1] # some vertices no triangle uses
 if lit:
  fvf,stride=0x152,36;nl=R.randint(1,3);vcol=R.random()<.5;nn=R.random()<.5;fog=R.random()<.25
  vb=[struct.pack('<6fI2f',*v,*[R.uniform(-1,1) for _ in range(3)],R.getrandbits(32),R.random(),R.random()) for v in vs]
  mat=struct.pack('<17f',R.random(),R.random(),R.random(),R.random(),.2,.3,.1,1,0,0,0,0,R.uniform(0,.2),0,0,0,0)
  lt=b''.join(struct.pack('<II',k,2)+struct.pack('<I',3)+struct.pack('<25f',R.random(),R.random(),R.random(),1,0,0,0,0,.1,.05,.1,1,0,0,0,R.uniform(-1,1),R.uniform(-1,1),R.uniform(.2,1),1000,0,1,0,0,0,0)+struct.pack('<II',k,0) for k in range(nl))
  st=cmd(33,1,mat)+cmd(34,2*nl,lt)+rs((137,1),(139,0xFF101820),(141,1),(143,int(nn)),(145,int(vcol)),(147,0),(148,0),(28,int(fog)),(140,3 if fog else 0),(36,f2u(2.)),(37,f2u(30.)),(35,0))
 else:
  fvf,stride,pk=R.choice([(0x002,12,lambda v:struct.pack('<3f',*v)),(0x042,16,lambda v:struct.pack('<3fI',*v,R.getrandbits(32))),(0x142,24,lambda v:struct.pack('<3fI2f',*v,R.getrandbits(32),R.random(),R.random())),
   (0x1C2,28,lambda v:struct.pack('<3fII2f',*v,R.getrandbits(32),R.getrandbits(32),R.random(),R.random())),(0x242,32,lambda v:struct.pack('<3fI4f',*v,R.getrandbits(32),R.random(),R.random(),R.random(),R.random()))])
  vb=[pk(v) for v in vs];st=rs((137,0),(28,0),(140,0))
 tex=R.random()<.75;two=tex and fvf==0x242 and R.random()<.7;gb=R.random()<.7
 if tex:
  ent=[(0,0,5),(0,1,4),(0,2,2),(0,3,0),(0,11,0),(0,24,0)]+([(1,0,5),(1,1,4),(1,2,2),(1,3,1),(1,11,1),(1,24,0)] if two else [(1,1,1)]) # handle 5: the contract's 8x8 texture
  st+=cmd(25,len(ent),b''.join(struct.pack('<HHI',*e) for e in ent))
 pad=R.choice([0,0,3,17]);base=R.choice([0,0,pad]) if pad else 0
 vbuf=bytes(stride*pad)+b''.join(vb)
 pre=cmd(28,1,struct.pack('<4I',vx,vy,vx+vw,vy+vh))+cmd(36,3,struct.pack('<I',1)+struct.pack('<16f',*W)+struct.pack('<I',2)+struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)+struct.pack('<I',3)+struct.pack('<16f',*P))+rs((22,cull),(136,1))+st
 def draw(sub):return cmd(26,len(sub),struct.pack('<H',base)+b''.join(struct.pack('<3H',*(q+pad-base for q in x)) for x in sub))
 nd=R.choice([1,1,2,4]);body=b''
 for d in range(nd):
  sub=ix if d==0 else [x for x in ix if R.random()<.6] or ix[:1]
  if d and R.random()<.5:
   W2=list(W);W2[12]+=R.uniform(-1,1);body+=cmd(36,1,struct.pack('<I',1)+struct.pack('<16f',*W2))
  body+=draw(sub)
 out=[]
 if ONLY is not None and n!=ONLY:return None,''
 for ron,rwd in ((1,1),(0,1),(1,0)):
  RON.value=ron;RWD.value=rwd;t=ctx();U.in_dll(lib,'vx_d3d_gband').value=int(gb);U.in_dll(lib,'vx3dn_gband').value=int(gb);rc=t.run_dp2(pre+body,vbuf,stride=stride,fvf=fvf);out.append((rc,tris(t,(vx,vy,vx+vw,vy+vh)),tris(t)))
 RON.value=1;RWD.value=1;U.in_dll(lib,'vx_d3d_gband').value=0;U.in_dll(lib,'vx3dn_gband').value=0
 return out,'%s %s cull %d %s fvf %X draws %d verts %d%s%s%s'%(kind,'persp' if persp else 'ortho',cull,'lit' if lit else 'unlit',fvf,nd,len(vs),' texture' if tex else '',' two sets' if two else '',' band' if gb else '')
r0=list(RG)
for n in range(SCENES):
 abc,what=scene(n)
 if abc is None:continue
 a,b,c=abc;e=None
 if a[0]!=b[0] or a[0]!=c[0]:e='answers %r %r %r'%(a[0],b[0],c[0])
 elif a[2]!=c[2]:e='giving the places back changed the output'
 else:e=near(a[1],b[1])
 if e:
  fails+=1
  if fails<=12:print('FAIL scene %d (%s): %s'%(n,what,e))
d=[RG[k]-r0[k] for k in range(9)]
print('range draws tried %d: inside the viewport %d, inside the guard band %d, dropped beyond a side %d, left to the gather %d; triangles culled by tnq_cull %d; draws the packets took whole %d (%d vertices); places given back %d'%tuple(d))
if ONLY is None and not(d[1] and d[2] and d[3] and d[4] and d[5] and d[6] and d[8]):print('FAIL: a path was not exercised');fails+=1
print('scenes %d, failures %d'%(SCENES,fails))
print('PASS' if not fails else 'FAILED')
sys.exit(1 if fails else 0)
