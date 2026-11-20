#!/usr/bin/env python3
"""tnl_pl_cases.py (.640): untransformed points and lines through the driver's T&L. Each case draws with untransformed vertices
(identity matrices, viewport 64x64) and again with the same primitives already transformed by this script; the packets' vertices
must be the same. Cases: every point and line command, lines cut at each plane, primitives wholly outside, a full buffer."""
import struct,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tests'))
from test_core import *
ID=struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
PRE=cmd(28,1,struct.pack('<4I',0,0,64,64))+cmd(36,3,struct.pack('<I',1)+ID+struct.pack('<I',2)+ID+struct.pack('<I',3)+ID)+rs((137,0),(28,0),(22,1),(136,1))
PL=(U*6).in_dll(lib,'vx_d3d_tn_pl');TN=(U*12).in_dll(lib,'vx_d3d_tn')
def ctx():
 t=Contract('test_tnl');t.setUp()
 for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',1),('vx_d3d_ixpass',1),('vx_d3d_indexed',1)]:U.in_dll(lib,n).value=v
 return t
def scr(p):x,y,z=p;return (32+x*32,32-y*32,z,1.0)
def clipseg(a,b):
 """the segment inside -1..1, -1..1, 0..1 (w = 1), or None"""
 t0,t1=0.0,1.0
 for ax,lo,hi in ((2,0.0,1.0),(0,-1.0,1.0),(1,-1.0,1.0)):
  for s,lim in ((1,lo),(-1,hi)):
   da=s*(a[ax]-lim);db=s*(b[ax]-lim)
   if da<0 and db<0:return None
   if da<0:t0=max(t0,da/(da-db))
   elif db<0:t1=min(t1,da/(da-db))
 if t0>t1:return None
 f=lambda t:tuple(a[k]+(b[k]-a[k])*t for k in range(3))
 return f(t0),f(t1)
def vbytes(p):
 """a packet's vertices (x y z rhw colour) as the arrays carry them; .654: from the 10-word form when the packet is prepacked (the driver's
 T&L draws without fog and texture take XP's diffuse program, T13, and with it the prepacked form)"""
 if p.pre_on==1:
  w=list(p.pre_w);return b''.join(struct.pack('<5I',*w[k*10:k*10+5])+bytes(C.sizeof(Vertex)-20) for k in range(p.vertex_count))
 assert p.pre_on==0
 return bytes(p.vertices)[:p.vertex_count*C.sizeof(Vertex)]
def verts_of(t):
 out=[]
 for p in t.calls:
  assert lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
  out.append((p.vertex_count,vbytes(p)))
 return out
def pos_of(t):
 out=[]
 for p in t.calls:
  assert lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
  b=vbytes(p)
  for k in range(p.vertex_count):out.append(struct.unpack_from('<4f',b,k*C.sizeof(Vertex)))
 return out
fails=0
def check(name,command,pts,segs=None,points=None,expect=None):
 """command: the untransformed draw over pts; segs/points: the primitives it names (index tuples)"""
 global fails
 vb=b''.join(struct.pack('<3fI',*p,0xff204060+i) for i,p in enumerate(pts))
 for k in range(6):PL[k]=0
 t=ctx();r=t.run_dp2(PRE+command,vb,stride=16,fvf=0x42);got=pos_of(t);pl=list(PL)
 pre=[];col=[]
 if segs is not None:
  for a,b in segs:
   sg=clipseg(pts[a],pts[b])
   if sg:pre+=[scr(sg[0]),scr(sg[1])]
  c2=cmd(15,len(pre)//2,struct.pack('<H',0))
 else:
  for a in points:
   q=pts[a]
   if -1<=q[0]<=1 and -1<=q[1]<=1 and 0<=q[2]<=1:pre.append(scr(q))
  c2=cmd(1,1,struct.pack('<2H',len(pre),0))
 want=[]
 if pre:
  t2=ctx();r2=t2.run_dp2(PRE+c2,b''.join(struct.pack('<4fI',*q,0xffffffff) for q in pre),stride=20,fvf=0x44);assert r2==(0,0),r2;want=pos_of(t2)
 ok=r==(0,0) and len(got)==len(want) and all(abs(g[k]-w[k])<=0.002 for g,w in zip(got,want) for k in range(4))
 if expect is not None:ok=ok and all(pl[i]==v for i,v in expect.items())
 print(('ok   ' if ok else 'FAIL ')+name,'rc',r,'vertices',len(got),'expected',len(want),'counts',pl)
 if not ok:
  fails+=1
  for g,w in list(zip(got,want))[:6]:print('   got',g,'want',w)
P=[(-.5,-.5,.5),(.5,-.5,.5),(.5,.5,.25),(-.5,.5,.75),(0,0,.5),(.25,-.75,.1)]
H=lambda *a:struct.pack('<%dH'%len(a),*a)
check('POINTS one run of 6',cmd(1,1,H(6,0)),P,points=range(6),expect={0:1,1:6})
check('POINTS three runs',cmd(1,3,H(2,0,1,5,2,2)),P,points=[0,1,5,2,3],expect={1:5})
check('LINELIST 3 lines',cmd(15,3,H(0)),P,segs=[(0,1),(2,3),(4,5)],expect={2:1,3:3})
check('LINELIST from vertex 2',cmd(15,2,H(2)),P,segs=[(2,3),(4,5)])
check('LINESTRIP 5 lines',cmd(16,5,H(0)),P,segs=[(0,1),(1,2),(2,3),(3,4),(4,5)])
check('INDEXEDLINELIST',cmd(2,3,H(0,2,1,3,5,4)),P,segs=[(0,2),(1,3),(5,4)])
check('INDEXEDLINESTRIP base 1',cmd(17,3,H(1,0,2,4,1)),P,segs=[(1,3),(3,5),(5,2)])
check('INDEXEDLINELIST2 base 2',cmd(27,2,H(2,0,3,1,2)),P,segs=[(2,5),(3,4)])
Q=[(0,0,.5),(2,0,.5),(-3,.5,.5),(0,2.5,.5),(.5,-4,.5),(0,0,-1),(0,0,3),(3,3,.5),(-2,-2,2),(5,0,.5),(6,1,.5),(.2,.2,.2)]
check('a line cut at the right plane',cmd(27,1,H(0,0,1)),Q,segs=[(0,1)],expect={3:1,4:1})
check('lines cut at left, top, bottom',cmd(27,3,H(0,0,2,0,3,0,4)),Q,segs=[(0,2),(0,3),(0,4)],expect={4:3})
check('lines cut at near and far',cmd(27,2,H(0,11,5,11,6)),Q,segs=[(11,5),(11,6)],expect={4:2})
check('a line cut at two planes (both ends outside)',cmd(27,1,H(0,2,1)),Q,segs=[(2,1)],expect={3:1,4:1})
check('a line cut at a corner and the far plane',cmd(27,1,H(0,7,8)),Q,segs=[(7,8)])
check('a line wholly outside one plane',cmd(27,1,H(0,9,10)),Q,segs=[(9,10)],expect={3:0,5:1})
check('a line outside across two planes (no part inside)',cmd(27,1,H(0,1,3)),Q,segs=[(1,3)])
check('points inside and outside',cmd(1,1,H(12,0)),Q,points=range(12),expect={1:2,5:10})
check('points all outside',cmd(1,1,H(1,1)),Q,points=[1],expect={1:0,5:1})
import random;R=random.Random(640)
for it in range(300):
 n=R.randint(2,40);pts=[(R.uniform(-2,2),R.uniform(-2,2),R.uniform(-.6,1.6)) for _ in range(n)]
 k=R.randint(1,30);op=R.choice((1,2,15,16,17,27));name='random %d op %d'%(it,op)
 if op==1:
  runs=[];
  for _ in range(R.randint(1,4)):s=R.randrange(n);runs.append((R.randint(1,n-s),s))
  check(name,cmd(1,len(runs),b''.join(H(*x) for x in runs)),pts,points=[s+i for c,s in runs for i in range(c)]) if 0 else None
  c1=cmd(1,len(runs),b''.join(H(*x) for x in runs));prim=dict(points=[s+i for c,s in runs for i in range(c)])
 elif op==2:
  sg=[(R.randrange(n),R.randrange(n)) for _ in range(k)];c1=cmd(2,k,b''.join(H(*x) for x in sg));prim=dict(segs=sg)
 elif op==15:
  k=R.randint(1,n//2);s=R.randint(0,n-2*k);c1=cmd(15,k,H(s));prim=dict(segs=[(s+2*i,s+2*i+1) for i in range(k)])
 elif op==16:
  k=R.randint(1,n-1);s=R.randint(0,n-1-k);c1=cmd(16,k,H(s));prim=dict(segs=[(s+i,s+i+1) for i in range(k)])
 elif op==17:
  b=R.randrange(n);ix=[R.randrange(n-b) for _ in range(k+1)];c1=cmd(17,k,H(b,*ix));prim=dict(segs=[(b+ix[i],b+ix[i+1]) for i in range(k)])
 else:
  b=R.randrange(n);sg=[(R.randrange(n-b),R.randrange(n-b)) for _ in range(k)];c1=cmd(27,k,H(b)+b''.join(H(*x) for x in sg));prim=dict(segs=[(b+x,b+y) for x,y in sg])
 import io,contextlib
 f0=fails;buf=io.StringIO()
 with contextlib.redirect_stdout(buf):check(name,c1,pts,**prim)
 if fails!=f0:print(buf.getvalue(),end='')
print('random cases 300, failures so far',fails)
# a draw larger than the scratch buffer is answered by the per-primitive path or declined, never a fault; many draws in one call fill and flush it
t=ctx();n=9000;vb=b''.join(struct.pack('<3fI',(i%90)/50-.9,(i//90)/60-.8,.5,0xffffffff) for i in range(n))
r=t.run_dp2(PRE+b''.join(cmd(16,n-1,H(0)) for _ in range(4))+cmd(1,1,H(n,0)),vb,stride=16,fvf=0x42);g=pos_of(t)
ok=r==(0,0) and len(g)==4*(n-1)*2+n*6;print(('ok   ' if ok else 'FAIL ')+'four strips of 8999 lines and 9000 points in one call','rc',r,'vertices',len(g));fails+=not ok
# .640: a triangle draw too large for the scratch buffer, and indices across more vertices than it holds: the same vertices as the draw
# given in commands of 500 triangles (before: nothing drawn)
SP=(U*2).in_dll(lib,'vx_d3d_tn_split')
def plain():
 t=Contract('test_tnl');t.setUp()
 for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',0),('vx_d3d_ixpass',0),('vx_d3d_indexed',0)]:U.in_dll(lib,n).value=v
 return t
def tris_of(t):
 o=[]
 for p in t.calls:
  assert lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
  b=bytes(p.vertices);z=C.sizeof(Vertex)
  o.append(b[:p.vertex_count*z])
 return o
def big(name,nv,tl,op):
 global fails
 vb=b''.join(struct.pack('<3fI',R.uniform(-.95,.95),R.uniform(-.95,.95),R.uniform(.05,.95),R.getrandbits(32)) for i in range(nv))
 if op==26:one=cmd(26,len(tl),H(0)+b''.join(H(*x) for x in tl));many=b''.join(cmd(26,len(tl[i:i+500]),H(0)+b''.join(H(*x) for x in tl[i:i+500])) for i in range(0,len(tl),500))
 elif op==3:one=cmd(3,len(tl),b''.join(H(*x,0) for x in tl));many=b''.join(cmd(3,len(tl[i:i+500]),b''.join(H(*x,0) for x in tl[i:i+500])) for i in range(0,len(tl),500))
 elif op==18:one=cmd(18,len(tl),H(0));many=b''.join(cmd(18,min(500,len(tl)-i),H(i*3)) for i in range(0,len(tl),500))
 elif op==19:one=cmd(19,len(tl),H(0));many=b''.join(cmd(19,min(2340,len(tl)-i),H(i)) for i in range(0,len(tl),2340)) # strips stay strips in the packets: cut where the pieces are
 else:one=cmd(20,len(tl),H(0)+b''.join(H(x) for x in tl2));many=b''.join(cmd(20,min(2340,len(tl)-i),H(0)+b''.join(H(x) for x in tl2[i:i+min(2340,len(tl)-i)+2])) for i in range(0,len(tl),2340))
 SP[0]=SP[1]=0;d0=TN[7]
 t=plain();r=t.run_dp2(PRE+one,vb,stride=16,fvf=0x42);a=tris_of(t);sp=list(SP);dec=TN[7]-d0
 t=plain();r2=t.run_dp2(PRE+many,vb,stride=16,fvf=0x42);b=tris_of(t)
 ok=r==(0,0) and r2==(0,0) and b''.join(a)==b''.join(b) and len(a)>0 and dec==0
 print(('ok   ' if ok else 'FAIL ')+name,'rc',r,'packets',len(a),'in small commands',len(b),'vertex bytes',len(b''.join(a)),'pieces',sp,'declined',dec);fails+=not ok
for n in (3000,8192,8193,21000):
 big('TRIANGLELIST of %d'%n,n*3,[0]*n,18)
 big('INDEXEDTRIANGLELIST2 of %d over 300 vertices'%n,300,[(R.randrange(300),R.randrange(300),R.randrange(300)) for _ in range(n)],26)
 big('INDEXEDTRIANGLELIST of %d over 300 vertices'%n,300,[(R.randrange(300),R.randrange(300),R.randrange(300)) for _ in range(n)],3)
big('TRIANGLESTRIP of 9001',9003,[0]*9001,19)
tl2=[R.randrange(400) for _ in range(9003)];big('INDEXEDTRIANGLESTRIP of 9001',400,[0]*9001,20)
big('INDEXEDTRIANGLELIST2 of 800 across 40000 vertices',40000,[(R.randrange(40000),R.randrange(40000),R.randrange(40000)) for _ in range(800)],26)
big('INDEXEDTRIANGLELIST2 of 9000 across 40000 vertices',40000,[(R.randrange(40000),R.randrange(40000),R.randrange(40000)) for _ in range(9000)],26)
# .640: TRIANGLEFAN_IMM and LINELIST_IMM in a call of untransformed vertices (the vertices in the command), and XYZB1 vertices (a weight
# after the position, passed over): the same vertices as the plain draw
def same(name,c1,vb1,st1,fvf1,c2,vb2,st2,fvf2):
 global fails
 t=plain();r=t.run_dp2(PRE+c1,vb1,stride=st1,fvf=fvf1);a=b''.join(tris_of(t))
 t=plain();r2=t.run_dp2(PRE+c2,vb2,stride=st2,fvf=fvf2);b=b''.join(tris_of(t))
 ok=r==(0,0) and r2==(0,0) and a==b and len(a)>0;print(('ok   ' if ok else 'FAIL ')+name,'rc',r,r2,'vertex bytes',len(a),len(b));fails+=not ok
fan=[(0,0,.5),(.5,0,.5),(.5,.5,.5),(0,.7,.5),(-.5,.5,.5),(-1.5,0,.5)]
vf=b''.join(struct.pack('<3fI',*q,0xff8040c0) for q in fan);dummy=struct.pack('<3fI',9,9,9,0)*8
def imm(op,n,data):
 for pad in range(4):
  c=b'\0'*0
  h=cmd(op,n,b'')  # header only; the vertices follow, aligned to 4 from the command stream's start
  yield h
hdr=cmd(23,4,b'');assert len(hdr)==4
same('TRIANGLEFAN_IMM of 4 triangles',cmd(23,4,struct.pack('<I',0)+vf),dummy,16,0x42,cmd(21,4,H(0)),vf,16,0x42)
same('LINELIST_IMM of 3 lines',cmd(24,3,vf),dummy,16,0x42,cmd(15,3,H(0)),vf,16,0x42)
same('a draw after TRIANGLEFAN_IMM (the command length)',cmd(23,4,struct.pack('<I',0)+vf)+cmd(18,2,H(0)),vf,16,0x42,cmd(21,4,H(0))+cmd(18,2,H(0)),vf,16,0x42)
vb1=b''.join(struct.pack('<4fI',*q,.25,0xff8040c0) for q in fan)
same('XYZB1 triangles',cmd(18,2,H(0)),vb1,20,0x46,cmd(18,2,H(0)),vf,16,0x42)
same('XYZB3 lines',cmd(16,5,H(0)),b''.join(struct.pack('<6fI',*q,.25,.5,.1,0xff8040c0) for q in fan),28,0x4A,cmd(16,5,H(0)),vf,16,0x42)
print('FAILED' if fails else 'PASS');sys.exit(1 if fails else 0)
