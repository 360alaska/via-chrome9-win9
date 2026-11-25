#!/usr/bin/env python3
"""tnl_vbox_cases.py (.752): the driver's T&L dropping a dense draw before the gather from the cached box of its vertex range
(vx_d3d_vbox 1) against the path that reads and transforms every vertex first (0): the packets must carry the same triangles, word for
word, for meshes inside, across each plane, just inside and just outside each plane, behind the eye and with a coordinate that is not
finite, with and without the guard band, under perspective and parallel projections with a turned view. Mode 2 runs the box test and
the full path together: a box that says "outside" where the vertices do not ([7]) fails. The box's life is checked too: none before the
second draw of a range, none without a generation, dropped when the generation changes, and dropped (not the draw) when the range's
first, middle or last vertex no longer lies inside it.
usage: tnl_vbox_cases.py [root] [random scenes] [seed]"""
import struct,sys,random,math,ctypes as C
from pathlib import Path
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 and not sys.argv[1].isdigit() else Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tests'))
from test_core import *
nums=[a for a in sys.argv[1:] if a.isdigit()]
SCENES=int(nums[0]) if nums else 600
R=random.Random(int(nums[1]) if len(nums)>1 else 752);fails=0
g=lambda n:U.in_dll(lib,n)
VB=(U*8).in_dll(lib,'vx_d3d_tn_vbox');DN=(U*4).in_dll(lib,'vx_d3d_tn_dense');TN=(U*12).in_dll(lib,'vx_d3d_tn')
GEN=[1000];KEEP=[] # every scene's vertex memory is kept, so no two scenes share an address (a range made twice at one address waits longer for its box)
def newgen():GEN[0]+=1;return GEN[0]
def mul(a,b):return [sum(a[i*4+k]*b[k*4+j] for k in range(4)) for i in range(4) for j in range(4)]
def persp(fov,asp,zn,zf):
 h=1/math.tan(fov/2);w=h/asp;q=zf/(zf-zn);return [w,0,0,0, 0,h,0,0, 0,0,q,1, 0,0,-q*zn,0]
def ortho(w,h,zn,zf):return [2/w,0,0,0, 0,2/h,0,0, 0,0,1/(zf-zn),0, 0,0,-zn/(zf-zn),1]
def roty(a):c,s=math.cos(a),math.sin(a);return [c,0,-s,0, 0,1,0,0, s,0,c,0, 0,0,0,1]
def rotx(a):c,s=math.cos(a),math.sin(a);return [1,0,0,0, 0,c,s,0, 0,-s,c,0, 0,0,0,1]
def trans(x,y,z):return [1,0,0,0, 0,1,0,0, 0,0,1,0, x,y,z,1]
def scale(x,y,z):return [x,0,0,0, 0,y,0,0, 0,0,z,0, 0,0,0,1]
ID=[1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1]
def f32(x):return struct.unpack('<f',struct.pack('<f',x))[0]
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
 return sorted(o)
class Scene:
 """one vertex buffer kept at one address, drawn by several DrawPrimitives2 calls"""
 def __init__(self,vs,ix,world,view,proj,gb,indexed=1,pad=0,base=0):
  self.stride=16;self.fvf=0x42;self.n=len(vs);self.pad=pad
  vb=bytes(16*pad)+b''.join(struct.pack('<3fI',f32(v[0]),f32(v[1]),f32(v[2]),0xFF000000|(k*2654435761&0xFFFFFF)) for k,v in enumerate(vs))
  self.vbuf=C.create_string_buffer(vb,len(vb));self.len=len(vb);KEEP.append(self.vbuf)
  m=lambda a:struct.pack('<16f',*a)
  self.pre=cmd(28,1,struct.pack('<4I',0,0,64,64))+cmd(36,3,struct.pack('<I',1)+m(world)+struct.pack('<I',2)+m(view)+struct.pack('<I',3)+m(proj))+rs((22,1),(136,1),(137,0),(28,0))
  self.draw=cmd(26,len(ix),struct.pack('<H',base)+b''.join(struct.pack('<3H',*(q+pad-base for q in x)) for x in ix))
  g('vx_d3d_gband').value=gb;g('vx3dn_gband').value=gb
  t=Contract('test_tnl');t.setUp()
  for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',indexed),('vx_d3d_ixpass',indexed),('vx_d3d_indexed',indexed)]:g(n).value=v
  self.t=t;self.first=1
 def call(self,gen,draws=1):
  c=(self.pre if self.first else b'')+self.draw*draws;self.first=0
  cb=C.create_string_buffer(c,len(c))
  inp=Input(C.addressof(cb),C.addressof(self.vbuf),len(c),self.len,self.len//16,16,self.fvf,0,None,0)
  self.t.render_states=(U*256)();inp.render_states=C.addressof(self.t.render_states);inp.render_state_count=256
  g('vx_d3d_vbgen').value=gen
  err=U(0xdeadbeef);rc=lib.vx_d3d_dp2(C.byref(self.t.context),C.byref(inp),C.byref(self.t.services),C.byref(err))
  assert g('vx_d3d_vbgen').value==0 # a generation holds for one call
  return rc,err.value
 def poke(self,vertex,xyz):
  struct.pack_into('<3f',self.vbuf,(self.pad+vertex)*16,*xyz)
def zero():
 for k in range(8):VB[k]=0
 for k in range(4):DN[k]=0
def play(mk,mode,calls=5,gen=None):
 """the scene built anew (a new context, the same triangles), drawn `calls` times; the triangles of all packets and the counters"""
 g('vx_d3d_vbox').value=mode;zero();s=mk();gen=newgen() if gen is None else gen
 r=[s.call(gen) for _ in range(calls)]
 return r,tris_set(s.t),list(VB),list(DN)
def check(name,mk,expect=None,calls=5):
 """expect: 'out' the box must drop the draw from its third drawing on, 'in' it must never, None either"""
 global fails
 a=play(mk,0,calls);b=play(mk,1,calls);c=play(mk,2,calls)
 ok=all(x==(0,0) for x in a[0]+b[0]+c[0]) and a[1]==b[1]==c[1] and c[2][7]==0 and a[2]==[0]*8
 ok=ok and b[2][0]==calls and b[2][4]<=1 and b[3][0]==calls and a[3][0]==calls
 ok=ok and b[3][2]==a[3][2] # as many draws counted wholly outside either way
 if expect=='out':ok=ok and b[2][2]==calls-2 and b[2][3]==(calls-2)*mk.nv and c[2][2]==calls-2 and len(a[1])==0 and b[2][6]==0
 if expect=='in':ok=ok and b[2][2]==0 and c[2][2]==0 and b[2][1]==calls-2
 if not ok or expect is not None:print(('ok   ' if ok else 'FAIL ')+name,'triangles',len(a[1]),len(b[1]),'box',b[2],'mode 2',c[2])
 fails+=not ok
 return b[2],c[2]
def grid(gw,gh,ox,oy,sx,sy,z0,z1):
 vs=[];ix=[]
 for j in range(gh+1):
  for i in range(gw+1):vs.append((ox+sx*i/gw,oy+sy*j/gh,z0+(z1-z0)*j/gh))
 for j in range(gh):
  for i in range(gw):
   a=j*(gw+1)+i;ix+=[(a,a+gw+1,a+1),(a+1,a+gw+1,a+gw+2)]
 return vs,ix
def maker(vs,ix,world=ID,view=ID,proj=ID,gb=1,indexed=1,pad=0,base=0):
 f=lambda:Scene(vs,ix,world,view,proj,gb,indexed,pad,base);f.nv=max(max(t) for t in ix)-min(min(t) for t in ix)+1;return f
# 1. named cases: the identity transform (clip space is the mesh's own space: x, y in -1..1, z in 0..1, w 1), each plane, both band settings
for gb in (0,1):
 for indexed in (0,1):
  L=' (guard band %d, %s packets)'%(gb,'indexed' if indexed else 'plain')
  K=dict(gb=gb,indexed=indexed)
  check('inside'+L,maker(*grid(8,8,-.8,-.8,1.6,1.6,.2,.8),**K),'in')
  check('across the right edge'+L,maker(*grid(8,8,.5,-.5,1,1,.2,.8),**K),'in')
  check('across the near plane'+L,maker(*grid(8,8,-.5,-.5,1,1,-.4,.6),**K),'in')
  check('across the far plane'+L,maker(*grid(8,8,-.5,-.5,1,1,.5,1.6),**K),'in')
  check('outside right'+L,maker(*grid(8,8,1.2,-.5,1,1,.2,.8),**K),'out')
  check('outside left'+L,maker(*grid(8,8,-2.4,-.5,1,1,.2,.8),**K),'out')
  check('outside top'+L,maker(*grid(8,8,-.5,1.3,1,1,.2,.8),**K),'out')
  check('outside bottom'+L,maker(*grid(8,8,-.5,-2.5,1,1,.2,.8),**K),'out')
  check('behind (z < 0)'+L,maker(*grid(8,8,-.5,-.5,1,1,-2,-.1),**K),'out')
  check('beyond the far plane'+L,maker(*grid(8,8,-.5,-.5,1,1,1.1,3),**K),'out')
  check('outside right, 500 vertices before it and a base'+L,maker(*grid(8,8,1.2,-.5,1,1,.2,.8),pad=500,base=300,**K),'out')
  # a hair on either side of the right edge: outside by less than the room the test keeps is never dropped by the box, and the
  # triangles are the same either way
  for d in (1e-7,1e-6,3e-6,1e-5,1e-4,1e-3):
   check('right edge + %g'%d+L,maker(*grid(4,4,1+d,-.5,1,1,.2,.8),**K))
   check('right edge - %g'%d+L,maker(*grid(4,4,1-d,-.5,1,1,.2,.8),**K),'in')
  vs,ix=grid(8,8,1.2,-.5,1,1,.2,.8);vs[40]=(float('nan'),0,.5);b,c=check('outside right with one vertex not finite'+L,maker(vs,ix,**K))
  if b[4] or b[2]:print('FAIL a box for a range with a coordinate that is not finite',b);fails+=1
# 2. the box's life
def life():
 global fails
 vs,ix=grid(8,8,1.2,-.5,1,1,.2,.8);mk=maker(vs,ix);nv=mk.nv
 def expect(name,got,want):
  global fails
  ok=got==want;print(('ok   ' if ok else 'FAIL ')+name,got if ok else '%s, wanted %s'%(got,want));fails+=not ok
 g('vx_d3d_vbox').value=1
 zero();s=mk();[s.call(0) for _ in range(5)];expect('no generation: no box, nothing dropped by it',list(VB),[0]*8)
 zero();s=mk();a=newgen();s.call(a);expect('first draw: seen only',list(VB)[:5],[1,0,0,0,0]);s.call(a);expect('second draw: the box made',list(VB)[:5],[2,0,0,0,1])
 s.call(a);expect('third draw: dropped before the gather',list(VB)[:5],[3,1,1,nv,1])
 s.call(newgen());expect('a new generation: the box gone',list(VB)[:5],[4,1,1,nv,1])
 zero();s=mk();a=newgen();[s.call(a) for _ in range(3)];n0=len(tris_set(s.t))
 s.poke(0,(0.,0.,.5));s.poke(1,(.5,0.,.5));s.poke(9,(0.,.5,.5)) # the first triangle moved onto the screen without a new generation
 s.call(a);expect('first vertex rewritten outside a lock: the box dropped, the draw kept',(list(VB)[5],list(VB)[2],len(tris_set(s.t))>n0),(1,1,True))
 zero();s=mk();a=newgen();[s.call(a) for _ in range(3)];s.poke(nv-1,(0.,0.,.5));s.call(a);expect('last vertex rewritten outside a lock: the box dropped',(list(VB)[5],list(VB)[2]),(1,1))
 zero();s=mk();a=newgen();[s.call(a) for _ in range(3)];s.poke(nv>>1,(0.,0.,.5));s.call(a);expect('middle vertex rewritten outside a lock: the box dropped',(list(VB)[5],list(VB)[2]),(1,1))
 zero();s=mk();a=newgen();s.call(a,draws=6);expect('six draws in one call: four dropped',list(VB)[:5],[6,4,4,4*nv,1])
 # a range rebuilt twice waits for its fourth draw
 zero();s=mk()
 for k in range(3):a=newgen();s.call(a);s.call(a)
 expect('three generations, two draws each: the first two make a box, the third waits',list(VB)[4],2)
 s.call(a);s.call(a);expect('... and makes it on its fourth draw',list(VB)[4],3)
life()
# 3. random scenes: a perspective or parallel projection, a turned and moved view, a scaled and turned world, meshes placed around the
# frustum (many wholly outside, many across a plane, some touching one)
tot=[0]*8;tot2=[0]*8;touch=0
for it in range(SCENES):
 gb=it&1;indexed=(it>>1)&1
 if it%3:proj=persp(R.uniform(.5,2),R.uniform(.6,1.8),R.choice((.05,.5,1)),R.choice((20,100,1000)))
 else:proj=ortho(R.uniform(2,40),R.uniform(2,40),R.uniform(0,2),R.uniform(10,100))
 view=mul(mul(roty(R.uniform(-3.2,3.2)),rotx(R.uniform(-1,1))),trans(R.uniform(-5,5),R.uniform(-5,5),R.uniform(-5,25)))
 s=R.choice((.1,1,1,10));world=mul(mul(scale(s,s*R.uniform(.5,2),s),roty(R.uniform(-3.2,3.2))),trans(R.uniform(-30,30),R.uniform(-10,10),R.uniform(-30,30)))
 if it%7==0:world=mul(world,scale(R.choice((1e-4,1e4)),R.choice((1e-4,1e4)),1))
 gw=R.randint(1,12);gh=R.randint(1,12);e=R.choice((.2,2,20))/s
 vs,ix=grid(gw,gh,R.uniform(-e,e),R.uniform(-e,e),R.uniform(.05,2)*e,R.uniform(.05,2)*e,R.uniform(-e,e),R.uniform(-e,e))
 if it%5==0:vs=[(x+R.uniform(-.1,.1)*e,y+R.uniform(-.1,.1)*e,z+R.uniform(-.3,.3)*e) for x,y,z in vs]
 if it%11==0:R.shuffle(ix);ix=ix[:max(1,len(ix)//2)]
 if it%4==1:
  # touching: the projection moved sideways so the vertex nearest the right side lies a hair inside or outside it (the box test must
  # leave these to the full path unless they are outside by more than its room)
  M=mul(mul(world,view),proj);d=max((x*M[0]+y*M[4]+z*M[8]+M[12])-(x*M[3]+y*M[7]+z*M[11]+M[15]) for x,y,z in vs)
  sz=max(abs(x*M[0])+abs(y*M[4])+abs(z*M[8])+abs(M[12])+abs(x*M[3])+abs(y*M[7])+abs(z*M[11])+abs(M[15]) for x,y,z in vs)
  proj=proj[:];proj[12]+=-d+R.choice((-1,1))*R.choice((1e-8,1e-7,1e-6,3e-6,1e-5,3e-5,1e-4))*sz;touch+=1
 import io,contextlib
 buf=io.StringIO();f0=fails
 with contextlib.redirect_stdout(buf):b,c=check('random scene %d'%it,maker(vs,ix,world,view,proj,gb,indexed,pad=R.choice((0,3,40))),calls=4)
 if fails!=f0:print(buf.getvalue(),end='')
 tot=[x+y for x,y in zip(tot,b)];tot2=[x+y for x,y in zip(tot2,c)]
print('random scenes',SCENES,'(touching the right side:',touch,') draws with a generation',tot[0],'with a box',tot[1],'dropped before the gather',tot[2],'(vertices',tot[3],') found outside only after the gather',tot[6],'mode 2: box wrong',tot2[7])
if tot[2]<SCENES//8:print('FAIL too few random scenes dropped by the box for the comparison to mean anything');fails+=1
if tot2[7]:print('FAIL');fails+=1
g('vx_d3d_vbox').value=0;g('vx_d3d_gband').value=0;g('vx3dn_gband').value=0
print('failures',fails);sys.exit(1 if fails else 0)
