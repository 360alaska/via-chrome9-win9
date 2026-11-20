#!/usr/bin/env python3
"""fuzz_tnl_diff.py (review of .644): the driver's T&L fast paths against its one-vertex reference path, on random scenes.

fast:      vx_d3d_tn_dense_on 1, vx_d3d_tn_soa_on 1 (the build's defaults)
reference: vx_d3d_tn_dense_on 0, vx_d3d_tn_soa_on 0 (the gather by index, every vertex finished by tn_attributes)

Each scene is a random mesh (grid, base vertex, padding before it, sparse or dense index use), a random vertex layout (normal,
diffuse, specular, one or two texture sets), random render state (lighting, colour sources, fog mode, cull) and a colour/normal
pattern (all the same, one odd vertex at a random place, all different). The triangles of the packets are compared as sorted sets:
positions and texture coordinates exactly, colour and fog bytes within 1.
usage: fuzz_tnl_diff.py [core.so] [scenes] [seed]"""
import sys,os,struct,random,ctypes as C
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import test_core as T
if len(sys.argv)>1 and sys.argv[1]!='-':T.lib=C.CDLL(os.path.abspath(sys.argv[1]))
lib=T.lib;U=T.U
SCENES=int(sys.argv[2]) if len(sys.argv)>2 else 300
SEED=int(sys.argv[3]) if len(sys.argv)>3 else 644
R=random.Random(SEED)
f2u=lambda f:struct.unpack('<I',struct.pack('<f',f))[0]
ON=lambda n:U.in_dll(lib,n)
class B(T.Contract):
 def runTest(self):pass
def m4(*m):return struct.pack('<16f',*m)
I4=m4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
LAYOUTS=[ # fvf, stride, has normal, has diffuse, has specular, tex sets
 (0x152,36,1,1,0,1),(0x1D2,40,1,1,1,1),(0x142,24,0,1,0,1),(0x252,44,1,1,0,2),(0x112,32,1,0,0,1),(0x142|0x80,28,0,1,1,1)]
def vert(lay,pos,n,dif,spe,uv):
 fvf,st,hn,hd,hs,ts=lay;o=struct.pack('<3f',*pos)
 if hn:o+=struct.pack('<3f',*n)
 if hd:o+=struct.pack('<I',dif)
 if hs:o+=struct.pack('<I',spe)
 for k in range(ts):o+=struct.pack('<2f',uv[0]+k*.1,uv[1])
 assert len(o)==st,(len(o),st,hex(fvf));return o
def unitn():
 while 1:
  v=[R.uniform(-1,1) for _ in range(3)];l=sum(x*x for x in v)**.5
  if l>.2:return tuple(x/l for x in v)
def build(seed):
 R.seed(seed)
 lay=R.choice(LAYOUTS);fvf,st,hn,hd,hs,ts=lay
 gw=R.choice([1,2,3,4,5,7,8,11,16,24]);gh=R.choice([1,2,3,5,8,12,24])
 sx=R.choice([.3,.8,1.6,1.9,3]);sy=R.choice([.3,.8,1.6,1.9,3]);ox=R.choice([-.95,-.8,-.5,-.2,0,.3,1.1,-2]) if sx<1.9 else R.choice([-.95,-.9,-1.2]);oy=R.choice([-.95,-.8,-.4,0,.5,1.2]) if sy<1.9 else R.choice([-.95,-.9])
 z0=R.choice([.1,.2,.4,.6,-.3,.9,1.3]);z1=z0+R.choice([0,0,.05,.3,-.5])
 pad=R.choice([0,0,3,40,513]);base=R.choice([0,0,7,300]) if pad+ (gw+1)*(gh+1) > 300 else 0
 pat=R.choice(['same','odd','odd8','odd_late','random','same_n'])
 nv=(gw+1)*(gh+1);nrm0=unitn();col0=R.getrandbits(32);spe0=R.getrandbits(32);odd=R.randrange(nv) if pat in('odd','odd_late') else (R.choice([7,8,9,11,12]) if pat=='odd8' else -1)
 if pat=='odd_late':odd=nv-1-R.randrange(min(nv,5))
 vs=b''
 for k in range(pad):vs+=vert(lay,(R.uniform(-3,3),R.uniform(-3,3),R.uniform(-1,2)),unitn(),R.getrandbits(32),R.getrandbits(32),(R.random(),R.random()))
 for j in range(gh+1):
  for i in range(gw+1):
   k=j*(gw+1)+i;n=nrm0;c=col0;s=spe0
   if pat=='random':n=unitn();c=R.getrandbits(32);s=R.getrandbits(32)
   elif pat=='same_n':c=R.getrandbits(32)
   elif k==odd:
    which=R.choice(['n','c','s','all']);
    if which in('n','all'):n=unitn()
    if which in('c','all'):c=R.getrandbits(32)
    if which in('s','all'):s=R.getrandbits(32)
   if R.random()<.01:n=(n[0]*3,n[1]*3,n[2]*3) # not unit length
   vs+=vert(lay,(ox+sx*i/gw,oy+sy*j/gh,z0+(z1-z0)*j/gh),n,c,s,(i/gw,j/gh))
 tris=[]
 for j in range(gh):
  for i in range(gw):
   a=j*(gw+1)+i;tris+=[(a,a+gw+1,a+1),(a+1,a+gw+1,a+gw+2)]
 if R.random()<.35:tris=[t for t in tris if R.random()<.4] or tris[:1] # sparse use of the range
 if R.random()<.2:R.shuffle(tris)
 lit=hn and R.random()<.8
 diff=R.choice([0,1,2]) if hd else 0
 amb=R.choice([0,1,2]);emi=R.choice([0,0,1,2]);spm=R.choice([0,1,2])
 fog=R.choice([0,0,1,2,3]);cull=R.choice([1,2,3])
 lightdir=unitn()
 mat=struct.pack('<17f',R.random(),R.random(),R.random(),1,R.random()*.3,R.random()*.3,R.random()*.3,1,R.random()*.2,R.random()*.2,R.random()*.2,0,0,0,0,0,0)
 lt=struct.pack('<II',0,2)+struct.pack('<I',3)+struct.pack('<25f',R.random(),R.random(),R.random(),1,0,0,0,0,R.random()*.3,R.random()*.3,R.random()*.3,1,0,0,0,*lightdir,1000,0,1,0,0,0,0)+struct.pack('<II',0,0)
 states=[(137,1 if lit else 0),(139,R.getrandbits(32)&0xFFFFFF|0xFF000000),(145,diff),(146,spm),(147,amb),(148,emi),(141,1 if (diff or amb or emi or spm) else 0),(22,cull),(29,0),(143,R.choice([0,1]))]
 pre=T.cmd(28,1,struct.pack('<4I',0,0,64,64))+T.cmd(36,3,struct.pack('<I',1)+I4+struct.pack('<I',2)+I4+struct.pack('<I',3)+I4)
 pre+=T.cmd(33,1,mat)+T.cmd(34,2,lt)+T.rs(*states)
 if fog:pre+=T.rs((28,1),(35,0),(140,fog),(36,f2u(R.choice([.1,.4,.8]))),(37,f2u(R.choice([1.5,3,9]))),(38,f2u(R.choice([.1,.5,2]))))
 mode=R.choice(['26','3','18'])
 if mode=='26':
  draw=T.cmd(26,len(tris),struct.pack('<H',base)+b''.join(struct.pack('<3H',*(q+pad-base if q+pad-base>=0 else 0 for q in t)) for t in tris))
 elif mode=='3':
  base=0;draw=T.cmd(3,len(tris),b''.join(struct.pack('<4H',*(q+pad for q in t),0) for t in tris))
 else:
  # non-indexed list: unroll the triangles' vertices into a new buffer
  uv=b'';allv=[vs[(pad+q)*st:(pad+q+1)*st] for t in tris for q in t];vs=b''.join(allv);pad=0;base=0
  draw=T.cmd(18,len(tris),struct.pack('<H',0))
 desc=dict(lay=hex(fvf),grid=(gw,gh),pad=pad,base=base,pat=pat,lit=lit,diff=diff,amb=amb,emi=emi,spm=spm,fog=fog,cull=cull,mode=mode,ntri=len(tris),odd=odd)
 return pre,draw,vs,st,fvf,desc
def tris_of(t):
 out=[]
 for p in t.calls:
  b0=p.bursts.b[0];w=list(p.pre_w)
  if p.pre_on==2:
   ni=b0&0xFFFF;sd=8 if b0&0x04000000 else 10;ix=(C.c_uint16*ni).from_buffer_copy(bytes(p.vertices)[:ni*2])
   for k in range(0,ni,3):
    out.append(tuple(sorted(tuple(w[i*sd:i*sd+8]) for i in ix[k:k+3])))
  else:
   V=(lambda i:tuple(w[i*10:i*10+8])) if p.pre_on else (lambda i:(p.vertices[i].x,p.vertices[i].y,p.vertices[i].z,p.vertices[i].rhw,p.vertices[i].color,0,0,0))
   base=0
   for tp,cn in ([(b>>16,b&0xFFFF) for b in p.bursts.b[:p.bursts.n]] or [(0,p.vertex_count)]):
    out+=[tuple(sorted(V(base+k+j) for j in range(3))) for k in (range(0,cn,3) if tp==0 else range(cn-2))];base+=cn
 return out
def run(sc,fast,indexed=1):
 pre,draw,vs,st,fvf,desc=sc
 ON('vx_d3d_tn_dense_on').value=1 if fast else 0;ON('vx_d3d_tn_soa_on').value=1 if fast else 0
 ON('vx_d3d_tn_fastix').value=1 if fast else 0 # .680: tn_ix's block path on the fast side only (.682: the engine's cull removed)
 t=B();t.setUp()
 for n,v in [('vx_d3d_feat',T.ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',indexed),('vx_d3d_ixpass',indexed),('vx_d3d_indexed',indexed)]:ON(n).value=v
 t._stages([(7,4,2,0,2,2,0,0)],spec=1)
 t.calls.clear();rc=t.run_dp2(pre+draw+draw,vs,stride=st,fvf=fvf)
 for p in t.calls:
  v=lib.vx3dn_validate(C.byref(p),C.byref(t.info))
  if v:return rc,None,'validate %d'%v
 return rc,tris_of(t),None
def same_tri(a,b):
 if len(a)!=len(b):return False
 for va,vb in zip(a,b):
  for k in range(8):
   if k in(4,5):
    for s in range(4):
     if abs(((va[k]>>(8*s))&255)-((vb[k]>>(8*s))&255))>1:return False
   elif va[k]!=vb[k]:return False
 return True
TOT=[0]
def main():
 fails=0;stats={}
 for s in range(SCENES):
  seed=SEED*100003+s;sc=build(seed);d=sc[5]
  for indexed in (1,0):
   ra=run(sc,0,indexed);rb=run(sc,1,indexed)
   ok=ra[0]==rb[0] and ra[2]==rb[2]
   if ok and ra[1] is not None:
    a=sorted(ra[1]);b=sorted(rb[1]);TOT[0]+=len(a)
    if len(a)!=len(b):ok=False;why='triangle count %d vs %d'%(len(a),len(b))
    else:
     bad=sum(not same_tri(x,y) for x,y in zip(a,b))
     if bad:ok=False;why='%d of %d triangles differ'%(bad,len(a))
   elif ok:why='';
   else:why='rc/validate %s %s vs %s %s'%(ra[0],ra[2],rb[0],rb[2])
   key=(d['pat'],d['lit']);stats[key]=stats.get(key,0)+1
   if not ok:
    fails+=1;print('FAIL scene',s,'seed',seed,'indexed' if indexed else 'expanded',d,why if not ok else '');
    if fails>12:break
  if fails>12:break
 ON('vx_d3d_tn_dense_on').value=1;ON('vx_d3d_tn_soa_on').value=1
 dn=(U*4).in_dll(lib,'vx_d3d_tn_dense');tn=(U*12).in_dll(lib,'vx_d3d_tn')
 print('coverage: T&L draws %d triangles %d vertices %d; dense draws %d (wholly inside %d, wholly outside %d); compared triangles %d'%(tn[0],tn[1],tn[2],dn[0],dn[1],dn[2],TOT[0]))
 fx=(U*4).in_dll(lib,'vx_d3d_tn_fastix_stat')
 print('.680: tn_ix block runs %d (%d triangles, %d vertices), spans left to the loop %d'%(fx[0],fx[1],fx[2],fx[3]))
 print('scenes',SCENES,'failures',fails);return 1 if fails else 0
sys.exit(main())
