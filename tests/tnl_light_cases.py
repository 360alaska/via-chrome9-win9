#!/usr/bin/env python3
"""tnl_light_cases.py (.645, review of .644): the driver's T&L lighting against Direct3D's fixed-function formulas, written out here in
double precision and independent of the driver: spot lights (the cone), specular lighting (SPECULARENABLE, the material's colour and power,
LOCALVIEWER), the colour sources, point-light range and attenuation. Before .645 a spot light was a point light and no specular colour was
written, while the HAL advertises POSITIONALLIGHTS and LOCALVIEWER.
usage: tnl_light_cases.py [core.so] [scenes] [seed]    (exit 1 when any vertex differs by more than 2 colour bytes)"""
import sys,os,struct,random,math,ctypes as C
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import test_core as T
if len(sys.argv)>1 and sys.argv[1]!='-':T.lib=C.CDLL(os.path.abspath(sys.argv[1]))
lib=T.lib;U=T.U
SCENES=int(sys.argv[2]) if len(sys.argv)>2 else 60
SEED=int(sys.argv[3]) if len(sys.argv)>3 else 645
LIDX=[0,1,2] # .646: the light indices; also run with sparse ones (any number is a valid index)
R=random.Random(SEED)
f2u=lambda f:struct.unpack('<I',struct.pack('<f',f))[0]
ON=lambda n:U.in_dll(lib,n)
class B(T.Contract):
 def runTest(self):pass
def m4(*m):return struct.pack('<16f',*m)
I4=m4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
def dot(a,b):return sum(x*y for x,y in zip(a,b))
def norm(a):
 l=math.sqrt(dot(a,a));return tuple(x/l for x in a) if l>0 else a
def unit():
 while 1:
  v=[R.uniform(-1,1) for _ in range(3)]
  if .2<math.sqrt(dot(v,v))<1:return norm(v)
def sat(x):return 0.0 if x<0 else 1.0 if x>1 else x
def rgb_bytes(c):return [int(sat(x)*255+.5) for x in c] # r, g, b
def build(kind):
 """a scene: view (rotation about y and a translation), vertices, lights, material, states, the expected colours"""
 ang=R.uniform(-.6,.6);ca,sa=math.cos(ang),math.sin(ang);tr=(R.uniform(-.1,.1),R.uniform(-.1,.1),R.uniform(-.05,.05))
 view=m4(ca,0,-sa,0, 0,1,0,0, sa,0,ca,0, tr[0],tr[1],tr[2],1)
 def toview(p):return (p[0]*ca+p[2]*sa+tr[0],p[1]+tr[1],-p[0]*sa+p[2]*ca+tr[2])
 def dirview(d):return (d[0]*ca+d[2]*sa,d[1],-d[0]*sa+d[2]*ca)
 nv=R.randint(12,30);verts=[]
 for i in range(nv):
  p=(R.uniform(-.4,.4),R.uniform(-.4,.4),R.uniform(.4,.75));n=unit() # inside the clip volume after the view: no vertex is made by clipping
  verts.append(dict(p=p,n=n,dif=R.getrandbits(32),spe=R.getrandbits(32),u=(i+1)/64.0,v=.5))
 mat=dict(dif=[R.random() for _ in range(3)]+[R.choice([1,.8])],amb=[R.random()*.6 for _ in range(3)]+[1],spe=[R.random() for _ in range(3)]+[1],emi=[R.random()*.2 for _ in range(3)]+[0],pw=R.choice([1.,4.,10.,25.,64.]))
 amb=R.getrandbits(32)&0xFFFFFF|0xFF000000
 nl=R.randint(1,3);lights=[]
 for i in range(nl):
  ty={'spot':2,'spec':R.choice([1,2,3]),'src':R.choice([1,2,3]),'point':1}[kind] if kind!='mix' else R.choice([1,2,3])
  th=R.uniform(.15,1.1);ph=th+R.uniform(.05,.9)
  lights.append(dict(type=ty,dif=[R.random() for _ in range(3)]+[1],spe=[R.random() for _ in range(3)]+[1],amb=[R.random()*.3 for _ in range(3)]+[1],
   pos=(R.uniform(-1.2,1.2),R.uniform(-1.2,1.2),R.uniform(-.5,1.5)),dir=unit(),rng=R.choice([1000.,3.,1.8]),fall=R.choice([1.,1.,2.,.5]),
   a0=R.choice([1.,.5]),a1=R.choice([0.,.3]),a2=R.choice([0.,.2]),theta=th,phi=ph))
 spec_on=kind in('spec','src','mix') ;local=R.choice([0,1]);cv=1 if kind=='src' else 0
 ssrc=R.choice([1,2]) if kind=='src' else 0
 return dict(view=view,toview=toview,dirview=dirview,verts=verts,mat=mat,amb=amb,lights=lights,spec_on=spec_on,local=local,cv=cv,ssrc=ssrc,kind=kind)
def expect(sc,v):
 """Direct3D's fixed-function lighting of one vertex: diffuse (r g b a) and specular (r g b) in 0..1"""
 m=sc['mat'];e=sc['toview'](v['p']);n=norm(sc['dirview'](v['n']))
 ambg=[((sc['amb']>>(16-8*c))&255)/255.0 for c in range(3)]
 def col(w):return [((w>>16)&255)/255.0,((w>>8)&255)/255.0,(w&255)/255.0,((w>>24)&255)/255.0]
 md,ma,me,ms=m['dif'],m['amb'],m['emi'],m['spe']
 if sc['cv']: # COLORVERTEX with the material sources 0, 0, 0 for diffuse/ambient/emissive (rs 145 147 148 = 0) and the specular source
  c1,c2=col(v['dif']),col(v['spe'])
  if sc['ssrc']==1:ms=c1
  elif sc['ssrc']==2:ms=c2
 d=[me[c]+ambg[c]*ma[c] for c in range(3)];s=[0.0,0.0,0.0]
 V=norm((-e[0],-e[1],-e[2])) if sc['local'] else (0,0,-1)
 for l in sc['lights']:
  att=1.0;ldir=None
  if l['type']==3:
   L=norm(sc['dirview']((-l['dir'][0],-l['dir'][1],-l['dir'][2])))
  else:
   lp=sc['toview'](l['pos']);dv=(lp[0]-e[0],lp[1]-e[1],lp[2]-e[2]);dd=math.sqrt(dot(dv,dv))
   if dd>l['rng']:continue
   L=norm(dv);att=1.0/(l['a0']+l['a1']*dd+l['a2']*dd*dd)
   if l['type']==2:
    ax=norm(sc['dirview'](l['dir']));rho=-dot(L,ax);ct=math.cos(l['theta']/2);cp=math.cos(l['phi']/2)
    if rho<=cp:continue
    if rho<ct:att*=((rho-cp)/(ct-cp))**l['fall']
  for c in range(3):d[c]+=att*l['amb'][c]*ma[c]
  ndl=dot(n,L)
  if ndl<=0:continue
  for c in range(3):d[c]+=att*ndl*l['dif'][c]*md[c]
  if sc['spec_on']:
   H=(L[0]+V[0],L[1]+V[1],L[2]+V[2]);hl=math.sqrt(dot(H,H))
   if hl>0:
    ndh=dot(n,H)/hl
    if ndh>0:
     f=att*(ndh**m['pw'])
     for c in range(3):s[c]+=f*l['spe'][c]*ms[c]
 return d,s
def run(sc,soa):
 vs=b''
 for v in sc['verts']:vs+=struct.pack('<6fIfI',*v['p'],*v['n'],0,0,0) if False else struct.pack('<6f2I2f',*v['p'],*v['n'],v['dif'],v['spe'],v['u'],v['v'])
 fvf=0x1D2;stride=44-4 # XYZ | NORMAL | DIFFUSE | SPECULAR | TEX1: 12+12+4+4+8 = 40
 assert len(vs)==40*len(sc['verts'])
 tris=[(i,(i+1)%len(sc['verts']),(i+2)%len(sc['verts'])) for i in range(0,len(sc['verts']),1)]
 m=sc['mat'];mat=struct.pack('<17f',*m['dif'],*m['amb'],*m['spe'],*m['emi'],m['pw'])
 pre=T.cmd(28,1,struct.pack('<4I',0,0,64,64))+T.cmd(36,3,struct.pack('<I',1)+I4+struct.pack('<I',2)+sc['view']+struct.pack('<I',3)+I4)+T.cmd(33,1,mat)
 for i,l in enumerate(sc['lights']):
  body=struct.pack('<I',l['type'])+struct.pack('<4f',*l['dif'])+struct.pack('<4f',*l['spe'])+struct.pack('<4f',*l['amb'])+struct.pack('<3f',*l['pos'])+struct.pack('<3f',*l['dir'])+struct.pack('<7f',l['rng'],l['fall'],l['a0'],l['a1'],l['a2'],l['theta'],l['phi'])
  li=LIDX[i]
  pre+=T.cmd(34,2,struct.pack('<II',li,2)+body+struct.pack('<II',li,0))
 st=[(137,1),(139,sc['amb']),(141,sc['cv']),(145,0),(146,sc['ssrc']),(147,0),(148,0),(22,1),(29,1 if sc['spec_on'] else 0),(142,sc['local']),(143,1),(28,0)]
 pre+=T.rs(*st)
 draw=T.cmd(26,len(tris),struct.pack('<H',0)+b''.join(struct.pack('<3H',*t) for t in tris))
 ON('vx_d3d_tn_soa_on').value=soa
 t=B();t.setUp()
 for n,v in [('vx_d3d_feat',T.ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',1),('vx_d3d_ixpass',1),('vx_d3d_indexed',1)]:ON(n).value=v
 t._stages([(7,4,2,0,2,2,0,0)],spec=1 if sc['spec_on'] else 0)
 t.calls.clear();rc=t.run_dp2(pre+draw,vs,stride=40,fvf=fvf)
 if rc!=(0,0):return rc,None
 got={}
 for p in t.calls:
  w=list(p.pre_w);b0=p.bursts.b[0]
  if p.pre_on==2:
   ni=b0&0xFFFF;sd=8 if b0&0x04000000 else 10;ix=(C.c_uint16*ni).from_buffer_copy(bytes(p.vertices)[:ni*2])
   for i in set(ix):
    uu=struct.unpack('<f',struct.pack('<I',w[i*sd+6]))[0]*64
    if abs(uu-round(uu))<1e-3:got[round(uu)]=(w[i*sd+4],w[i*sd+5])
  else:
   for i in range(p.vertex_count):got[round(struct.unpack('<f',struct.pack('<I',w[i*10+6]))[0]*64)]=(w[i*10+4],w[i*10+5])
 return rc,got
def main():
 global LIDX
 bad=0;checked=0;kinds=['spot','spec','src','mix','point'];per={}
 for s in range(SCENES):
  kind=kinds[s%len(kinds)];sc=build(kind);LIDX=[0,1,2] if s%2==0 else [17,1000,0x7FFFFFFF] # .646: odd scenes with indices past 15
  for soa in (1,0):
   rc,got=run(sc,soa)
   if got is None:print('FAIL scene',s,kind,'rc',rc);bad+=1;continue
   worst=0;nv=0
   for i,v in enumerate(sc['verts']):
    if i+1 not in got:continue
    d,sp=expect(sc,v);dw,sw=got[i+1]
    gd=[(dw>>16)&255,(dw>>8)&255,dw&255];gs=[(sw>>16)&255,(sw>>8)&255,sw&255]
    ed=rgb_bytes(d);es=rgb_bytes(sp) if sc['spec_on'] else [0,0,0]
    e=max([abs(a-b) for a,b in zip(gd,ed)]+[abs(a-b) for a,b in zip(gs,es)]);worst=max(worst,e);nv+=1
   checked+=nv;per[kind]=max(per.get(kind,0),worst)
   if worst>2:bad+=1;print('FAIL scene',s,kind,'soa',soa,'worst colour byte difference',worst,'over',nv,'vertices; lights',[l['type'] for l in sc['lights']],'local',sc['local'],'src',sc['ssrc'])
 ON('vx_d3d_tn_soa_on').value=1
 print('vertices checked',checked,'worst difference by kind',per,'failures',bad);return 1 if bad else 0
sys.exit(main())
