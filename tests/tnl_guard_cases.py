#!/usr/bin/env python3
"""tnl_guard_cases.py (.698): the driver's T&L with the guard band (vx_d3d_gband 1: the side planes clipped at twice the viewport, the
rest left to the engine's own screen-space clipping) against without it (the CPU clips the sides to the viewport), on fuzz_tnl_diff.py's
random scenes (64 x 64 viewport, meshes reaching well past every edge). Both outputs are rasterised here as the engine does (pixel (x, y)
covered at its corner, top-left rule, the clip window bounding the pixels, painted in order) and compared pixel by pixel: the same pixels
covered, and where both cover, z, the colour bytes and u alike (z within 1e-4, bytes within 2, u within 1e-3; every second scene
perspective with vertices behind the near plane (.699): edges within 2% of the pixels, z 1e-3, bytes 8, u 0.02: a clipped triangle's colour is
screen-linear between the CPU's new vertices). Every packet must pass
vx3dn_validate (with the guard bit) and every vertex lie inside the VxD's +-4096. .701: every fourth scene has a projected set
(u v q in the record), drawn in plain packets; .703: those carry the band too (VX3DN_EXT_GUARD) and are compared the same way (u left out).
usage: tnl_guard_cases.py [core.so] [scenes] [seed]"""
import sys,os,struct,ctypes as C
HERE=os.path.dirname(os.path.abspath(__file__))
src=open(os.path.join(HERE,'fuzz_tnl_diff.py')).read()
src=src[:src.rindex('sys.exit(main())')]
argv=sys.argv[:];sys.argv=[argv[0]]+argv[1:2]
ns={'__file__':os.path.join(HERE,'fuzz_tnl_diff.py'),'__name__':'fuzz_mod'};exec(compile(src,'fuzz_tnl_diff.py','exec'),ns);sys.argv=argv
T=ns['T'];lib=ns['lib'];U=ns['U'];ON=ns['ON'];B=ns['B'];build=ns['build']
SCENES=int(argv[2]) if len(argv)>2 else 200
SEED=int(argv[3]) if len(argv)>3 else 698
PJ=[0,0];M2=[0,0]
u2f=lambda u:struct.unpack('<f',struct.pack('<I',u&0xFFFFFFFF))[0]
def canon(p):
 # .701: a packet's bytes with the parts past its counts zeroed (the per-vertex arrays past vertex_count, the bursts past n, the
 # prepacked words past the vertices): those hold whatever an earlier packet left there
 q=type(p).from_buffer_copy(bytes(p));n=q.vertex_count
 for arr in (q.vertices,q.tex.uv,q.shader.spec,q.stages.uv1,q.stages.q,q.stages.r1):
  es=C.sizeof(arr)//len(arr);m=min(n,len(arr));C.memset(C.addressof(arr)+es*m,0,C.sizeof(arr)-es*m)
 m=min(q.bursts.n,64);C.memset(C.addressof(q.bursts.b)+4*m,0,4*(64-m))
 m=min(10*n,len(q.pre_w));C.memset(C.addressof(q.pre_w)+4*m,0,4*(len(q.pre_w)-m))
 return bytes(q)
def packets(sc,gb,persp=0,proj=0):
 pre,draw,vs,st,fvf,desc=sc
 ON('vx_d3d_tn_dense_on').value=1;ON('vx_d3d_tn_soa_on').value=1;ON('vx_d3d_tn_fastix').value=1
 ON('vx_d3d_gband').value=gb;ON('vx3dn_gband').value=gb
 t=B();t.setUp()
 for n,v in [('vx_d3d_feat',T.ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',1),('vx_d3d_ixpass',1),('vx_d3d_indexed',1)]:ON(n).value=v
 t._stages([(7,4,2,0,2,2,0,0)],spec=1)
 if proj:t.context.state.tss[0][24]=0x103 # .701: a projected set (u v q in the record): plain packets (.703: with VX3DN_EXT_GUARD)
 GB=(C.c_uint32*2).in_dll(lib,'vx_d3d_gbs');gb0=(GB[0],GB[1])
 if persp: # .699: a perspective projection and a view 1 unit back: w varies, some vertices behind the near plane (0.6)
  zn,zf=0.6,50.0;P=ns['m4'](1.2,0,0,0,0,1.6,0,0,0,0,zf/(zf-zn),1,0,0,-zn*zf/(zf-zn),0);V=ns['m4'](1,0,0,0,0,1,0,0,0,0,1,0,0,0,1.0,1)
  pre=pre+T.cmd(36,2,struct.pack('<I',2)+V+struct.pack('<I',3)+P)
 t.calls.clear();rc=t.run_dp2(pre+draw,vs,stride=st,fvf=fvf)
 tris=[];why=None;raw=[canon(p) for p in t.calls] # .701: the packets (a projected scene's must match: the used parts byte for byte)
 for p in t.calls:
  v=lib.vx3dn_validate(C.byref(p),C.byref(t.info))
  if v:why='validate %d'%v;break
  b0=p.bursts.b[0];w=list(p.pre_w)
  if p.pre_on!=2: # .703: a plain or prepacked triangle packet (the records indexed packets cannot take): VX3DN_EXT_GUARD carries the band
   if p.ext.prim!=0:why='not triangles';break
   if gb and not p.ext.dither&0x100:why='plain packet without VX3DN_EXT_GUARD';break
   if gb:
    ON('vx3dn_gband').value=0;r0=lib.vx3dn_validate(C.byref(p),C.byref(t.info));ON('vx3dn_gband').value=1
    if not r0:why='VX3DN_EXT_GUARD taken with ACCEL_D3D_GUARDBAND off';break
   n=p.vertex_count
   if p.pre_on:vx=[(u2f(w[i*10]),u2f(w[i*10+1]),u2f(w[i*10+2]),w[i*10+4],0.0,u2f(w[i*10+3])) for i in range(n)]
   else:vx=[(u2f(q.x),u2f(q.y),u2f(q.z),q.color,0.0,u2f(q.rhw)) for q in p.vertices[:n]] # (u left out: a projected set's u/q)
   if any(not(-4096<=a[0]<=4095 and -4096<=a[1]<=4095) for a in vx):why='vertex outside -4096 .. +4095';break
   bl=[p.bursts.b[k] for k in range(p.bursts.n)] or [n]
   at=0
   for b in bl:
    tp,cn=b>>16,b&0xFFFF
    if tp==0:tris+=[[vx[at+k],vx[at+k+1],vx[at+k+2]] for k in range(0,cn,3)]
    else:tris+=[[vx[at+k],vx[at+k+1],vx[at+k+2]] if not k&1 else [vx[at+k+1],vx[at+k],vx[at+k+2]] for k in range(cn-2)]
    at+=cn
   continue
  if gb and not b0&0x08000000:why='guard bit missing';break
  if gb:
   ON('vx3dn_gband').value=0;r0=lib.vx3dn_validate(C.byref(p),C.byref(t.info));ON('vx3dn_gband').value=1
   if not r0:why='the guard bit taken with ACCEL_D3D_GUARDBAND off';break
  ni=b0&0xFFFF;sd=8 if b0&0x04000000 else 10;ix=(C.c_uint16*ni).from_buffer_copy(bytes(p.vertices)[:ni*2])
  for k in range(0,ni,3):
   tri=[]
   for i in ix[k:k+3]:
    x,y,z=u2f(w[i*sd]),u2f(w[i*sd+1]),u2f(w[i*sd+2]);rhw=u2f(w[i*sd+3])
    if not(-4096<=x<=4095 and -4096<=y<=4095):why='vertex outside -4096 .. +4095';break
    tri.append((x,y,z,w[i*sd+4],u2f(w[i*sd+6]),rhw))
   tris.append(tri)
 if gb and why is None and (GB[1]-gb0[1] if proj else GB[0]-gb0[0])==0 and t.calls:why='band set-up count (.701): proj %d, with %d, without %d'%(proj,GB[0]-gb0[0],GB[1]-gb0[1])
 return rc,tris,why,(p.clip.left,p.clip.top,p.clip.right,p.clip.bottom) if t.calls else (0,0,64,64),raw
def raster(tris,clip):
 l,t_,r,b=clip;img={}
 for (a,bb,c) in tris:
  ax,ay=a[0],a[1];bx,by=bb[0],bb[1];cx,cy=c[0],c[1]
  area=(bx-ax)*(cy-ay)-(by-ay)*(cx-ax)
  if area==0:continue
  sgn=1 if area>0 else -1
  def edge(x0,y0,x1,y1,px,py):return sgn*((x1-x0)*(py-y0)-(y1-y0)*(px-x0))
  def tl(x0,y0,x1,y1): # top-left rule (for the orientation made positive)
   dx,dy=(x1-x0)*sgn,(y1-y0)*sgn
   return (dy==0 and dx>0) or dy<0
  E=[(ax,ay,bx,by),(bx,by,cx,cy),(cx,cy,ax,ay)];TL=[tl(*e) for e in E]
  x0=max(l,int(min(ax,bx,cx))-1);x1=min(r,int(max(ax,bx,cx))+2);y0=max(t_,int(min(ay,by,cy))-1);y1=min(b,int(max(ay,by,cy))+2)
  for py in range(y0,y1):
   for px in range(x0,x1):
    e=[edge(*E[k],px,py) for k in range(3)]
    if all(e[k]>0 or (e[k]==0 and TL[k]) for k in range(3)):
     l0=e[1]/(sgn*area);l1=e[2]/(sgn*area);l2=1-l0-l1
     z=a[2]*l0+bb[2]*l1+c[2]*l2;q=a[5]*l0+bb[5]*l1+c[5]*l2 or 1e-30
     u=(a[4]*a[5]*l0+bb[4]*bb[5]*l1+c[4]*c[5]*l2)/q # perspective-correct, as the engine
     col=[(a[3]>>s&255)*l0+(bb[3]>>s&255)*l1+(c[3]>>s&255)*l2 for s in (0,8,16,24)]
     img[(px,py)]=(z,col,u)
 return img
def main():
 fails=0;cmp_px=0;edge_px=0;scenes=0
 for s in range(SCENES):
  sc=build(SEED*100003+s)
  pj=1 if s%4==3 else 0;ra=packets(sc,0,s&1,pj);rb=packets(sc,1,s&1,pj)
  if pj:PJ[0]+=1;PJ.append(len(rb[4]))
  if ra[2] or rb[2] or ra[0]!=rb[0]:
   if ra[2]==rb[2]==None or ra[2]!=rb[2]:print('FAIL scene',s,'rc/why',ra[0],ra[2],rb[0],rb[2],sc[5]);fails+=1
   continue
  scenes+=1
  ia=raster(ra[1],ra[3]);ib=raster(rb[1],rb[3])
  if s%4==0: # .702: ACCEL_D3D_GUARDBAND 2 (the band without its viewport codes): the same pixels as with them, at most more triangles sent
   r2=packets(sc,2,s&1)
   if r2[2] or r2[0]!=rb[0] or raster(r2[1],r2[3])!=ib or len(r2[1])<len(rb[1]):print('FAIL scene',s,'mode 2 (no codes) differs',r2[0],r2[2],len(r2[1]),len(rb[1]));fails+=1
   M2[0]+=1;M2[1]+=len(r2[1])-len(rb[1])
  l,t_,r,b=ra[3];border=lambda k:k[0] in(l,r-1) or k[1] in(t_,b-1)
  only={k for k in set(ia)^set(ib) if not border(k)};bad=0 # .699: the viewport's own border left out: where the CPU's clipped edge lies
  # exactly on it, a tie decides the pixel (top-left rule) and this rasteriser's tie may not be the engine's; the guard band covers it
  for k in set(ia)&set(ib):
   (za,ca,ua),(zb,cb,ub)=ia[k],ib[k];cmp_px+=1
   tz,tc,tu=(1e-3,8,2e-2) if s&1 else (1e-4,2,1e-3) # .699: with perspective, the CPU's clipped vertices (clip-space interpolation, then a
   # screen-linear colour) differ from the whole triangle's by more than rounding: looser, as Gouraud colour across a clip does
   if abs(za-zb)>tz or any(abs(x-y)>tc for x,y in zip(ca,cb)) or abs(ua-ub)>tu:bad+=1
  edge_px+=len(only)
  if bad or len(only)>(max(16,len(ia)//50) if s&1 else max(2,len(ia)//200)): # .699: perspective edges a pixel apart along long clipped edges (up to 2%)
   fails+=1;print('FAIL scene',s,'pixels covered by one only',len(only),'of',len(ia),'differing',bad,sc[5])
   if fails>10:break
 print('scenes compared',scenes,'pixels compared',cmp_px,'pixels covered by one side only',edge_px,'failures',fails)
 print('.702: scenes drawn again without the viewport codes',M2[0],'triangles they sent that the codes drop',M2[1])
 print('.703: scenes with a projected set (plain packets with VX3DN_EXT_GUARD), compared as the rest',PJ[0],'packets',sum(PJ[2:]))
 return 1 if fails else 0
sys.exit(main())
