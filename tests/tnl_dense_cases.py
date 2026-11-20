#!/usr/bin/env python3
"""tnl_dense_cases.py (.642): the driver's T&L taking a draw's vertex range in order (vx_d3d_tn_dense_on 1) against the gather by index
(0): the packets' vertices must be the same bytes, for meshes inside, across the planes and outside, lit, fogged and textured; and the
application's transformed vertices with a z of infinity: drawn with the z held to the range."""
import struct,sys,random
from pathlib import Path
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tests'))
from test_core import *
R=random.Random(642);fails=0
ID=struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
DN=(U*4).in_dll(lib,'vx_d3d_tn_dense');ON=U.in_dll(lib,'vx_d3d_tn_dense_on');ZI=U.in_dll(lib,'vx_d3d_zinf_prims')
f2u=lambda f:struct.unpack('<I',struct.pack('<f',f))[0]
def ctx(indexed):
 t=Contract('test_tnl');t.setUp()
 for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',indexed),('vx_d3d_ixpass',indexed),('vx_d3d_indexed',indexed)]:U.in_dll(lib,n).value=v
 return t
def stream(t):
 o=b''
 for p in t.calls:
  assert lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
  o+=bytes(p.vertices)[:p.vertex_count*C.sizeof(Vertex)]
 return o
def tris_set(t):
 """indexed packets: every triangle's three vertex records, as a sorted list (the scratch order differs between the two ways).
 .652 (a review of .651: this compared only each packet's form and vertex count, so wrong indices with the right sizes passed): each
 triangle by its vertices' words (8 or 10 a vertex), turned so its first vertex is the smallest (the winding kept); packets that are
 not indexed give their vertex words three at a time"""
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
 return sorted(o)
def mesh(gw,gh,ox,oy,sx,sy,z0,z1):
 vs=[];ix=[]
 for j in range(gh+1):
  for i in range(gw+1):vs.append((ox+sx*i/gw,oy+sy*j/gh,z0+(z1-z0)*j/gh))
 for j in range(gh):
  for i in range(gw):
   a=j*(gw+1)+i;ix+= [(a,a+gw+1,a+1),(a+1,a+gw+1,a+gw+2)]
 return vs,ix
def run(name,vs,ix,lit,pre_extra=b'',base=0,pad=0):
 global fails
 if lit:
  fvf=0x152;stride=36;vb=b''.join(struct.pack('<6fI2f',*v,R.uniform(-1,1),R.uniform(-1,1),-1.0,R.getrandbits(32),R.random(),R.random()) for v in vs)
  mat=struct.pack('<17f',1,1,1,1,.2,.2,.2,1,0,0,0,0,0,0,0,0,0);lt=struct.pack('<II',0,2)+struct.pack('<I',3)+struct.pack('<25f',1,1,1,1,0,0,0,0,.1,.1,.1,1,0,0,0,.2,.3,1.0,1000,0,1,0,0,0,0)+struct.pack('<II',0,0)
  st=cmd(33,1,mat)+cmd(34,2,lt)+rs((137,1),(139,0xFF202020),(28,1),(140,3),(36,f2u(.1)),(37,f2u(.9)),(35,0))
 else:
  fvf=0x42;stride=16;vb=b''.join(struct.pack('<3fI',*v,R.getrandbits(32)) for v in vs);st=rs((137,0),(28,0))
 vb=bytes(stride*pad)+vb
 pre=cmd(28,1,struct.pack('<4I',0,0,64,64))+cmd(36,3,struct.pack('<I',1)+ID+struct.pack('<I',2)+ID+struct.pack('<I',3)+ID)+rs((22,1),(136,1))+st+pre_extra
 draw=cmd(26,len(ix),struct.pack('<H',base)+b''.join(struct.pack('<3H',*(q+pad-base for q in x)) for x in ix))
 out=[]
 for on in (0,1):
  ON.value=on
  for k in range(4):DN[k]=0
  t=ctx(0);r=t.run_dp2(pre+draw+draw,vb,stride=stride,fvf=fvf);out.append((r,stream(t),list(DN)))
  t=ctx(1);r2=t.run_dp2(pre+draw+draw,vb,stride=stride,fvf=fvf);out[-1]+=(r2,tris_set(t))
 ON.value=1
 a,b=out
 ok=a[0]==(0,0)==b[0] and a[1]==b[1] and a[3]==(0,0)==b[3] and a[4]==b[4] and a[2][0]==0 and b[2][0] in (0,2) and (b[2][0]==2 or 'random' in name)
 print(('ok   ' if ok else 'FAIL ')+name,'vertex bytes',len(a[1]),len(b[1]),'in order',b[2],'indexed packets',len(a[4]),len(b[4]));fails+=not ok
for lit in (0,1):
 L=' lit, fog, texture' if lit else ''
 # .652 (a review of .651): wholly inside fans of 4-7 vertices, every group-tail length, after earlier draws' slots
 for vertices in (4,5,6,7):
  import math
  vs=[(0,0,.5)]+[(.7*math.cos(i*5.0/(vertices-1)),.7*math.sin(i*5.0/(vertices-1)),.5) for i in range(vertices-1)]
  run('inside, a group tail of %d'%(vertices%4)+L,vs,[(0,i,i+1) for i in range(1,vertices-1)],lit)
 run('24x24 mesh inside'+L,*mesh(24,24,-.9,-.9,1.8,1.8,.2,.8),lit)
 run('mesh across the right and bottom edges'+L,*mesh(20,20,0,-1.6,1.7,1.7,.3,.6),lit)
 run('mesh across the near plane'+L,*mesh(16,16,-.5,-.5,1,1,-.4,.6),lit)
 run('mesh across the far plane and a corner'+L,*mesh(16,16,.2,.2,1.5,1.5,.5,1.6),lit)
 run('mesh wholly outside (right)'+L,*mesh(10,10,1.2,-.5,1,1,.2,.8),lit)
 run('mesh wholly behind'+L,*mesh(10,10,-.5,-.5,1,1,-2,-.1),lit)
 run('mesh inside, base 300 and 500 vertices before it'+L,*mesh(12,12,-.8,-.8,1.6,1.6,.2,.8),lit,base=300,pad=500)
 vs,ix=mesh(12,12,-.8,-.8,1.6,1.6,.2,.8);vs[40]=(float('nan'),0,.5);run('mesh with one vertex not finite'+L,vs,ix,lit)
for it in range(120):
 gw=R.randint(2,30);gh=R.randint(2,30);vs,ix=mesh(gw,gh,R.uniform(-2,1),R.uniform(-2,1),R.uniform(.2,3),R.uniform(.2,3),R.uniform(-.5,1),R.uniform(0,1.5))
 R.shuffle(ix);ix=ix[:R.randint(len(ix)//2,len(ix))]
 import io,contextlib
 f0=fails;buf=io.StringIO()
 with contextlib.redirect_stdout(buf):run('random mesh %d'%it,vs,ix,it&1,base=R.choice((0,0,7)),pad=R.choice((7,7,60)))
 if fails!=f0:print(buf.getvalue(),end='')
print('random meshes 120, failures so far',fails)
# .643: a material colour from the vertices on the group path (four at a time) against the one-vertex code: the same triangles, colour
# bytes within 1; unlit without fog too
SO=U.in_dll(lib,'vx_d3d_tn_soa_on')
def words(t):
 o=[]
 for p in t.calls:
  assert lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
  b=bytes(p.vertices);z=C.sizeof(Vertex)
  for k in range(p.vertex_count):o.append(b[k*z:(k+1)*z]+struct.pack('<I',p.shader.spec[k])+struct.pack('<4I',p.tex.uv[k][0],p.tex.uv[k][1],p.stages.uv1[k][0],p.stages.uv1[k][1])) # .651: and the specular word (word 5; .642-.650 compared the diffuse alone); .653: and both coordinate sets
 return o
def vmat(name,srcs,nl,lit=1,fog=1,spec=0,same=0,point=0,sl=None):
 # sl (.651): specular lighting (SPECULARENABLE): (power, LOCALVIEWER, the specular source (rs 146))
 global fails
 vs,ix=mesh(14,14,-.9,-.9,1.9,1.9,.1,.9)
 fvf=0x152|(0x80 if spec else 0);vb=b''
 for v in vs:
  # same 1: one normal and one colour on every vertex (lit once); 2: runs of four alike (a lane keeps its products); 3: one colour, the normals differ
  kq=len(vb)//(36+(4 if spec else 0))
  if same==1:nrm=(.2,.3,-.9);col=(0xff8040c0,0x00406080)
  elif same==2:
   if kq%8<5:
    if kq%8==0:hold=((R.uniform(-1,1),R.uniform(-1,1),-.5),(R.getrandbits(32),R.getrandbits(32)))
    nrm,col=hold
   else:nrm=(R.uniform(-1,1),R.uniform(-1,1),-.5);col=(R.getrandbits(32),R.getrandbits(32))
  elif same==3:nrm=(R.uniform(-1,1),R.uniform(-1,1),-.5);col=(0xff8040c0,0x00406080)
  else:nrm=(R.uniform(-1,1),R.uniform(-1,1),R.uniform(-1,-.2));col=(R.getrandbits(32),R.getrandbits(32))
  vb+=struct.pack('<6fI',*v,*nrm,col[0])
  if spec:vb+=struct.pack('<I',col[1])
  vb+=struct.pack('<2f',R.random(),R.random())
 stride=36+(4 if spec else 0)
 mat=struct.pack('<17f',.9,.8,.7,.6,.3,.2,.4,1,*((.7,.6,.9,1) if sl else (0,0,0,0)),.05,.1,.02,0,sl[0] if sl else 0);lts=b''
 for i in range(nl):
  ty=1 if (point==1 or (point==2 and i%2==0)) else 2 if point==3 or (point==4 and i%3==0) else 1 if point==4 and i%3==1 else 3 # .653: point 3 spot lights (type 2), 4 spot, point and directional; .647: point lights (type 1): positions near the mesh, a range that cuts some vertices off, attenuation
  pos=(R.uniform(-1,1),R.uniform(-1,1),R.uniform(-.3,1.2)) if ty==1 else (0,0,0)
  dr=(R.uniform(-1,1),R.uniform(-1,1),1.0);fall=R.choice((1,.5)) if ty!=2 else R.choice((1,1,.5,2.5,0));cone=(0,0)
  if ty==2:pos=(R.uniform(-.6,.6),R.uniform(-.6,.6),R.uniform(-1,-.3));dr=(R.uniform(-.3,.3),R.uniform(-.3,.3),1.0);th=R.uniform(.2,1.2);cone=(th,th+R.choice((R.uniform(.1,.9),0)))
  lts+=cmd(34,2,struct.pack('<II',i,2)+struct.pack('<I',ty)+struct.pack('<25f',R.random(),R.random(),R.random(),1,*((R.random(),R.random(),R.random(),1) if sl else (0,0,0,0)),R.random()*.2,R.random()*.2,R.random()*.2,1,*pos,*dr,R.choice((1000,1.2,.8)) if ty!=2 else R.choice((1000,2.5)),fall if ty==2 else 0,R.choice((1,.5)),R.choice((0,.4)),R.choice((0,.3)),*cone)+struct.pack('<II',i,0))
 st=cmd(33,1,mat)+lts+rs((137,lit),(139,0xFF182430),(141,1),(145,srcs[0]),(147,srcs[1]),(148,srcs[2]),(143,1),(28,fog),(140,3 if fog else 0),(36,f2u(.1)),(37,f2u(.9)),(35,0),*(((29,1),(142,sl[1]),(146,sl[2])) if sl else ((29,0),)))
 pre=cmd(28,1,struct.pack('<4I',0,0,64,64))+cmd(36,3,struct.pack('<I',1)+ID+struct.pack('<I',2)+ID+struct.pack('<I',3)+ID)+rs((22,1),(136,1))+st
 draw=cmd(26,len(ix),struct.pack('<H',0)+b''.join(struct.pack('<3H',*x) for x in ix))
 out=[];ONE=U.in_dll(lib,'vx_d3d_tn_one');one=0
 for on in (0,1):
  SO.value=on;o0=ONE.value;t=ctx(0);r=t.run_dp2(pre+draw,vb,stride=stride,fvf=fvf);out.append((r,words(t)))
  if on:one=ONE.value-o0
 SO.value=1;a,b=out;worst=0;bad=len(a[1])!=len(b[1]) or not a[1]
 if not bad:
  for x,y in zip(a[1],b[1]):
   if x==y:continue
   wx=struct.unpack('<%dI'%(len(x)//4),x);wy=struct.unpack('<%dI'%(len(y)//4),y)
   for k,(p,q) in enumerate(zip(wx,wy)):
    if p==q:continue
    if k in (4,5):
     d=max(abs(((p>>(8*s))&255)-((q>>(8*s))&255)) for s in range(4));worst=max(worst,d);bad|=d>1
    else:bad=True
 ok=a[0]==(0,0)==b[0] and not bad and not ((point or sl) and lit and one)
 print(('ok   ' if ok else 'FAIL ')+name,'vertices',len(a[1]),len(b[1]),'worst colour byte difference',worst,'one at a time',one);fails+=not ok
for srcs in ((1,0,0),(0,1,0),(0,0,1),(1,1,0),(1,1,1),(2,0,0),(1,2,0),(0,0,2),(2,2,2),(0,0,0)):
 for nl in (0,1,3):
  vmat('material sources diffuse %d ambient %d emissive %d, %d directional lights'%(*srcs,nl),srcs,nl,spec=1 if 2 in srcs else 0)
for same in (1,2,3):
 for srcs in ((1,0,0),(0,0,1),(1,1,1),(2,1,0),(0,0,0)):
  for nl in (0,2):vmat('colours %s: sources %d %d %d, %d lights'%(('','all one, one normal','in runs','all one, normals differ')[same],*srcs,nl),srcs,nl,spec=1,same=same)
for point in (1,2):
 for srcs in ((0,0,0),(1,0,0),(1,1,1),(2,1,0)):
  for nl in (1,3):
   for same in (0,3):
    vmat('%s, sources %d %d %d, %d lights, %s'%(('point lights','point and directional')[point-1],*srcs,nl,('colours and normals vary','one colour, normals vary')[same==3]),srcs,nl,spec=1,point=point,same=same)
vmat('unlit, no fog (the kernel not called)',(0,0,0),0,lit=0,fog=0)
# .651: specular lighting in groups against the one-vertex code: powers 1, 0, 0.5, 12, 60; LOCALVIEWER on and off; the specular colour from
# the material or from the vertex (rs 146 = 2, the vertex's specular colour); directional, point and both; the vertices' colours varying
for pw,lv,ss,point,nl in ((12,1,0,0,2),(12,0,0,0,3),(1,1,0,1,2),(0,1,0,1,1),(.5,0,0,2,3),(60,1,0,2,4),(12,1,2,2,3),(30,0,2,1,2),(8,1,0,2,8)):
 vmat('specular lighting: power %g, LOCALVIEWER %d, source %d, %s, %d lights'%(pw,lv,ss,('directional','point','point and directional')[point],nl),(0,0,0) if ss==0 else (0,0,0),nl,spec=1,point=point,sl=(pw,lv,ss))
# .653: spot lights in groups (tnk_splight) against the one-vertex code: falloff 1, 0.5, 2.5 and 0, theta equal to phi or below it,
# with point and directional lights, specular on and off
for point,nl,sl in ((3,1,None),(3,2,None),(3,4,None),(4,3,None),(4,6,None),(3,2,(12,1,0)),(4,3,(1,0,0)),(4,5,(30,1,2)),(3,8,None)):
 for srcs in ((0,0,0),(1,1,1)):
  vmat('spot lights: %s, %d lights, sources %d %d %d%s'%(('spot','spot, point and directional')[point-3],nl,*srcs,', specular power %g'%sl[0] if sl else ''),srcs,nl,spec=1,point=point,sl=sl)
# .653: generated and transformed coordinate sets, and a set the vertex lacks, in the group path against the one-vertex code (words
# 6-9 exact): stage 0 (and stage 1) TEXCOORDINDEX camera-space normal (0x10000), position (0x20000), reflection (0x30000), set 1 of a
# one-set vertex; TEXTURETRANSFORMFLAGS 2 and 3 | PROJECTED (256) with a texture matrix
def texcase(name,tci0,ttf0,tci1=None,lit=1,lv=0,m=(12,12,-.9,-.9,1.8,1.8,.15,.85)):
 global fails
 vs,ix=mesh(*m);vb=b''
 for v in vs:vb+=struct.pack('<6fI2f',*v,R.uniform(-1,1),R.uniform(-1,1),-.6,R.getrandbits(32),R.random(),R.random())
 tm=struct.pack('<16f',.9,.1,.05,.02,-.1,.8,.03,.01,.2,.1,1.1,.05,.3,.2,.1,1)
 lt=cmd(34,2,struct.pack('<II',0,2)+struct.pack('<I',3)+struct.pack('<25f',1,1,1,1,0,0,0,0,.1,.1,.1,1,0,0,0,.2,.3,1.0,1000,0,1,0,0,0,0)+struct.pack('<II',0,0))
 st=cmd(33,1,struct.pack('<17f',.9,.8,.7,1,.3,.3,.3,1,0,0,0,0,0,0,0,0,0))+lt+rs((137,lit),(139,0xFF202020),(141,1),(145,1),(28,1),(140,3),(36,f2u(.1)),(37,f2u(.9)),(35,0),(142,lv),(143,1))
 ent=[(0,0,5),(0,1,4),(0,2,2),(0,3,0),(0,11,tci0),(0,24,ttf0)] # handle 5: the contract's 8x8 texture
 if tci1 is not None:ent+=[(1,0,5),(1,1,4),(1,2,2),(1,3,1),(1,11,tci1),(1,24,0)]
 else:ent+=[(1,1,1)]
 st+=cmd(25,len(ent),b''.join(struct.pack('<HHI',*e) for e in ent))
 pre=cmd(28,1,struct.pack('<4I',0,0,64,64))+cmd(36,4,struct.pack('<I',1)+ID+struct.pack('<I',2)+ID+struct.pack('<I',3)+ID+struct.pack('<I',16)+tm)+rs((22,1),(136,1))+st
 draw=cmd(26,len(ix),struct.pack('<H',0)+b''.join(struct.pack('<3H',*x) for x in ix))
 out=[];WHY=(U*10).in_dll(lib,'vx_d3d_tn_why');one=0 # one: draws the group path refused (vx_d3d_tn_why[8])
 for on in (0,1):
  SO.value=on;o0=WHY[8];t=ctx(0);r=t.run_dp2(pre+draw,vb,stride=36,fvf=0x152);out.append((r,words(t)))
  if on:one=WHY[8]-o0
 SO.value=1;a,b=out;worst=0;bad=len(a[1])!=len(b[1]) or not a[1]
 if not bad:
  for x,y in zip(a[1],b[1]):
   if x==y:continue
   wx=struct.unpack('<%dI'%(len(x)//4),x);wy=struct.unpack('<%dI'%(len(y)//4),y)
   for k,(p,q) in enumerate(zip(wx,wy)):
    if p==q:continue
    if k in (4,5):
     d=max(abs(((p>>(8*s))&255)-((q>>(8*s))&255)) for s in range(4));worst=max(worst,d);bad|=d>1
    else:bad=True
 nz=sum(1 for x in b[1] if x[-16:-8]!=bytes(8));nz1=sum(1 for x in b[1] if x[-8:]!=bytes(8))
 ok=a[0]==(0,0)==b[0] and not bad and one==0 and nz>len(b[1])//2 # .658: a set the vertex lacks reads set 0 (non-zero) too
 print(('ok   ' if ok else 'FAIL ')+name,'vertices',len(a[1]),len(b[1]),'worst colour byte difference',worst,'draws refused',one,'non-zero sets',nz,nz1);fails+=not ok
for tci,ttf,nm in ((0x10000,0,'camera-space normal'),(0x20000,0,'camera-space position'),(0x30000,0,'reflection'),(0x30000,0,'reflection, LOCALVIEWER'),(0,2,'set 0 transformed (count 2)'),
                   (0x20000,3|256,'position transformed, projected (count 3)'),(1,0,'set 1 of a one-set vertex')):
 texcase('coordinates: '+nm,tci,ttf,lv=1 if 'LOCALVIEWER' in nm else 0)
texcase('coordinates: stage 0 set 0, stage 1 reflection',0,0,tci1=0x30000)
texcase('coordinates: unlit, position transformed',0x20000,2,lit=0)
for nm,m in (('across the near plane',(14,14,-.5,-.5,1,1,-.4,.6)),('across the right edge and the far plane',(14,14,.2,-.6,1.5,1.2,.5,1.6))):
 texcase('coordinates: position transformed, projected, '+nm,0x20000,3|256,m=m)
 texcase('coordinates: reflection, '+nm,0x30000,0,tci1=0x10000,m=m)
vmat('unlit, no fog, a specular word',(0,0,0),0,lit=0,fog=0,spec=1)
vmat('unlit, fog',(0,0,0),0,lit=0,fog=1,spec=1)
# .647: the set-up kept between draws (vx_d3d_tn_keep_on 1) against it built every draw (0): one call of many draws, a world matrix before
# each (some draws with none, some with a light or material change between), meshes large enough to fill the scratch buffer (16,384
# places) several times: the packets' bytes must be the same. A draw refused for room and prepared again must still use its own world
# matrix (a fault of the first cut of .647: the retry kept the previous draw's matrices in the group kernels' frame).
KO=U.in_dll(lib,'vx_d3d_tn_keep_on');ST3=(U*3).in_dll(lib,'vx_d3d_tn_setup')
def keep_case(name,lit,cv,point,seed):
 global fails
 RR=random.Random(seed);vs,ix=mesh(30,30,-1.2,-1.2,2.4,2.4,.2,.9)
 vb=b''.join(struct.pack('<6fI2f',*v,RR.uniform(-1,1),RR.uniform(-1,1),-1.0,RR.getrandbits(32),RR.random(),RR.random()) for v in vs)
 mat=struct.pack('<17f',.9,.8,.7,1,.3,.3,.3,1,0,0,0,0,.05,.05,.05,0,0)
 lt=lambda i,ty:cmd(34,2,struct.pack('<II',i,2)+struct.pack('<I',ty)+struct.pack('<25f',RR.random(),RR.random(),RR.random(),1,0,0,0,0,.1,.1,.1,1,RR.uniform(-1,1),RR.uniform(-1,1),RR.uniform(0,1),.3,-1,.5,5,0,1,.2,0,0,0)+struct.pack('<II',i,0))
 pre=cmd(28,1,struct.pack('<4I',0,0,64,64))+cmd(36,2,struct.pack('<I',2)+ID+struct.pack('<I',3)+ID)+cmd(33,1,mat)+lt(0,3)+(lt(7,1) if point else b'')
 pre+=rs((22,1),(136,1),(137,lit),(139,0xFF202020),(141,cv),(145,1 if cv else 0),(28,1),(140,3),(36,f2u(.1)),(37,f2u(.9)),(35,0))
 draw=cmd(26,len(ix),struct.pack('<H',0)+b''.join(struct.pack('<3H',*x) for x in ix))
 body=b''
 for d in range(40):
  r=RR.random()
  if r<.8:body+=cmd(36,1,struct.pack('<I',1)+struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,RR.uniform(-.5,.5),RR.uniform(-.5,.5),RR.uniform(-.1,.1),1))
  elif r<.9:body+=lt(0,3)
  body+=draw
 out=[]
 for on in (0,1):
  KO.value=on;ST3[0]=ST3[1]=ST3[2]=0;t=ctx(1);rr=t.run_dp2(pre+body,vb,stride=36,fvf=0x152);out.append((rr,stream(t),list(ST3)))
 KO.value=1;a,b=out
 ok=a[0]==(0,0)==b[0] and a[1]==b[1] and b[2][1]>0
 print(('ok   ' if ok else 'FAIL ')+name,'bytes',len(a[1]),len(b[1]),'set-ups built, matrices again, kept',b[2]);fails+=not ok
# .648 (board, 3DMark 2001 on .647: 1,993,559 primitives left out for a non-finite vertex the driver's T&L made, none on .646): a draw
# whose record size differs from the buffer's (a second stage turned on or off) is prepared, refused for room (tn_draw 2) and prepared
# again after the flush: the second prepare found the set-up kept, and the group path used the frame of the draw before (the old record
# size). Draws here alternate stage 1 on and off with a world matrix between, in one call; keeping must give the same bytes and no
# primitive may be left out.
def stride_case(name,lit,point,seed):
 global fails
 RR=random.Random(seed);vs,ix=mesh(12,12,-1.2,-1.2,2.4,2.4,.2,.9)
 vb=b''.join(struct.pack('<6fI2f',*v,RR.uniform(-1,1),RR.uniform(-1,1),-1.0,RR.getrandbits(32),RR.random(),RR.random()) for v in vs)
 mat=struct.pack('<17f',.9,.8,.7,1,.3,.3,.3,1,0,0,0,0,.05,.05,.05,0,0)
 lt=lambda i,ty:cmd(34,2,struct.pack('<II',i,2)+struct.pack('<I',ty)+struct.pack('<25f',1,1,1,1,0,0,0,0,.1,.1,.1,1,0,.5,.5,.3,-1,.5,5,0,1,.2,0,0,0)+struct.pack('<II',i,0))
 pre=cmd(28,1,struct.pack('<4I',0,0,64,64))+cmd(36,2,struct.pack('<I',2)+ID+struct.pack('<I',3)+ID)+cmd(33,1,mat)+lt(0,3)+(lt(1,1) if point else b'')
 pre+=rs((22,1),(136,1),(137,lit),(139,0xFF202020),(28,1),(140,3),(36,f2u(.1)),(37,f2u(.9)),(35,0))
 pre+=cmd(25,2,struct.pack('<HHI',0,1,4)+struct.pack('<HHI',0,11,0))
 draw=cmd(26,len(ix),struct.pack('<H',0)+b''.join(struct.pack('<3H',*x) for x in ix))
 body=b''
 for d in range(60):
  body+=cmd(25,2,struct.pack('<HHI',1,1,4 if d&1 else 1)+struct.pack('<HHI',1,11,0))
  body+=cmd(36,1,struct.pack('<I',1)+struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,RR.uniform(-.3,.3),RR.uniform(-.3,.3),0,1))+draw
 out=[]
 for on in (0,0,1): # the first run left out: the call before it leaves the scratch buffer's order different (as on .645)
  KO.value=on;ST3[0]=ST3[1]=ST3[2]=0;BR[0]=BR[1]=0;t=ctx(1);rr=t.run_dp2(pre+body,vb,stride=36,fvf=0x152);out.append((rr,stream(t),list(ST3),BAD()))
 KO.value=1;a,b=out[1],out[2]
 ok=a[0]==(0,0)==b[0] and a[1]==b[1] and a[3]==b[3]==0
 print(('ok   ' if ok else 'FAIL ')+name,a[0],b[0],'bytes',len(a[1]),len(b[1]),'bad-vertex primitives',a[3],b[3],'set-ups',b[2]);fails+=not ok
BR=(U*18).in_dll(lib,'vx_d3d_bad_rec')
def BAD():return BR[0]+BR[1] # [0] the application's, [1] the driver's T&L
for lit in (0,1):
 for point in (0,1):
  if point and not lit:continue
  stride_case('kept set-up, the record size changing between draws: lit %d, a point light %d'%(lit,point),lit,point,648+lit*2+point)
for lit in (0,1):
 for cv in (0,1):
  for point in (0,1):
   if not lit and (cv or point):continue
   keep_case('kept set-up: lit %d, colour from the vertices %d, a point light %d'%(lit,cv,point),lit,cv,point,647+lit*4+cv*2+point)
# the application's transformed vertices with an infinite z
def zcase(name,z,want_drawn,want_z=None,x=48.0):
 global fails
 t=ctx(0);ZI.value=0
 vb=b''.join(struct.pack('<4fI',*q,0xffffffff) for q in ((16,48,.5,1),(x,48,z,1),(16,16,.5,1)))
 r=t.run_dp2(rs((22,1),(7,0))+cmd(18,1,struct.pack('<H',0)),vb,stride=20,fvf=0x44)
 zs=[struct.unpack_from('<4f',bytes(p.vertices),k*C.sizeof(Vertex))[2] for p in t.calls for k in range(p.vertex_count)]
 for p in t.calls:assert lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
 ok=r==(0,0) and bool(zs)==want_drawn and (want_z is None or want_z in zs) and ZI.value==(1 if want_drawn else 0)
 print(('ok   ' if ok else 'FAIL ')+name,'rc',r,'z values',zs,'counted',ZI.value);fails+=not ok
inf=float('inf')
zcase('z -infinity: drawn at z 0',-inf,True,0.0)
zcase('z +infinity: drawn at z 1',inf,True,1.0)
zcase('z NaN: left out',float('nan'),False)
zcase('x infinity with z -infinity: left out',-inf,False,x=inf)
print('FAILED' if fails else 'PASS');sys.exit(1 if fails else 0)
