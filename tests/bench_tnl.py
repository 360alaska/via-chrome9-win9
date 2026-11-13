#!/usr/bin/env python3
"""The driver's T&L on the host: VXTLP's GRID scene (30 lit, fogged, textured 24x24 grids, 34,560 triangles a frame, some across the
near plane, some off the screen) through vx_d3d_dp2, timed, and its triangles recorded for a comparison between two builds of core.so:
  python3 tests/bench_tnl.py [core.so] [frames] [dump.bin]     time the scene; with dump.bin, write the triangles (sorted) for a diff
  python3 tests/bench_tnl.py --cmp a.bin b.bin                 compare two dumps: positions and coordinates exact, colour bytes within 1
Uses tests/test_core.py's harness. (.544)"""
import sys,os,struct,time,ctypes as C,random
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
if len(sys.argv)>1 and sys.argv[1]=='--cmp':
 a=open(sys.argv[2],'rb').read();b=open(sys.argv[3],'rb').read()
 ta=[a[i:i+120] for i in range(0,len(a),120)];tb=[b[i:i+120] for i in range(0,len(b),120)]
 print('triangles',len(ta),len(tb))
 if len(ta)!=len(tb):print('COUNT DIFFERS');sys.exit(1)
 bad=0;worst=0
 for x,y in zip(ta,tb):
  wx=struct.unpack('<30I',x);wy=struct.unpack('<30I',y)
  for v in range(3):
   for k in range(10):
    if k in (4,5):
     for s in range(4):
      d=abs(((wx[v*10+k]>>(8*s))&255)-((wy[v*10+k]>>(8*s))&255))
      if d:worst=max(worst,d);bad+=d>1
    elif wx[v*10+k]!=wy[v*10+k]:bad+=1
 print('exact words differing or colour bytes more than 1 apart:',bad,'worst colour byte difference:',worst)
 sys.exit(1 if bad else 0)
import test_core as T
if len(sys.argv)>1 and sys.argv[1]!='-':T.lib=C.CDLL(os.path.abspath(sys.argv[1]))
lib=T.lib;U=T.U
class B(T.Contract):
 def runTest(self):pass
def main():
 frames=int(sys.argv[2]) if len(sys.argv)>2 else 20;dump=sys.argv[3] if len(sys.argv)>3 else None
 t=B();t.setUp()
 U.in_dll(lib,'vx_d3d_tnl').value=1;feat=U.in_dll(lib,'vx_d3d_feat');feat.value=T.ALLF
 for n in ('vx_d3d_prepack','vx_d3d_ixpass','vx_d3d_indexed'):U.in_dll(lib,n).value=1
 m4=lambda *m:struct.pack('<16f',*m);I=m4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1);zn,zf=1.0,200.0
 P=m4(1.5,0,0,0,0,2,0,0,0,0,zf/(zf-zn),1,0,0,-zn*zf/(zf-zn),0);N=24;vb=b'';tris=[]
 for j in range(N+1):
  for i in range(N+1):vb+=struct.pack('<6fI2f',i-N/2,0,j,0.3,1,0.1,0xff806040,i/N,j/N)
 for j in range(N):
  for i in range(N):a=j*(N+1)+i;tris+=[(a,a+N+1,a+1),(a+1,a+N+1,a+N+2)]
 light=T.cmd(34,2,struct.pack('<II',0,2)+struct.pack('<I',3)+struct.pack('<12f',1,1,1,1,0,0,0,0,.2,.2,.2,1)+struct.pack('<6f',0,0,0,.3,-1,.2)+struct.pack('<7f',1000,0,1,0,0,0,0)+struct.pack('<II',0,0))
 f2u=lambda f:struct.unpack('<I',struct.pack('<f',f))[0];rnd=random.Random(543);draws=b''
 for k in range(30):draws+=T.cmd(36,1,struct.pack('<I',1)+m4(1,0,0,0,0,1,0,0,0,0,1,0,rnd.uniform(-40,40),rnd.uniform(-8,-2),rnd.uniform(-10,80),1))+t._itl(tris)
 cmds=T.cmd(28,1,struct.pack('<4I',0,0,64,64))+T.cmd(36,2,struct.pack('<I',2)+I+struct.pack('<I',3)+P)+T.cmd(33,1,struct.pack('<17f',1,1,1,1,.3,.3,.3,1,0,0,0,0,0,0,0,0,0))+light
 cmds+=T.rs((137,1),(139,0xff404040),(145,1),(147,0),(148,0),(28,1),(35,0),(140,3),(36,f2u(10.)),(37,f2u(150.)),(22,1))+draws
 t._stages([(7,4,2,0,2,2,0,0)],spec=1)
 t.calls.clear();rc=t.run_dp2(cmds,vb,stride=36,fvf=0x152);assert rc==(0,0),rc;print('packets',len(t.calls))
 tn=(U*12).in_dll(lib,'vx_d3d_tn');print('one frame: draws %d triangles %d vertices transformed %d clipped %d dropped %d made %d left out %d lit %d'%(tn[0],tn[1],tn[2],tn[3],tn[4],tn[5],tn[7],tn[8]))
 if dump:
  out=[]
  for p in t.calls:
   b0=p.bursts.b[0];w=list(p.pre_w)
   if p.pre_on==2:
    ni=b0&0xFFFF;st=8 if b0&0x04000000 else 10;ix=(C.c_uint16*ni).from_buffer_copy(bytes(p.vertices)[:ni*2])
    for k in range(0,ni,3):out.append(tuple(sorted(tuple(w[i*st:i*st+8])+(0,0) for i in ix[k:k+3])))
   else:
    V=(lambda i:tuple(w[i*10:i*10+8])+(0,0)) if p.pre_on else (lambda i:(p.vertices[i].x,p.vertices[i].y,p.vertices[i].z,p.vertices[i].rhw,p.vertices[i].color,0,0,0,0,0))
    base=0
    for tp,cn in ([(b>>16,b&0xFFFF) for b in p.bursts.b[:p.bursts.n]] or [(0,p.vertex_count)]):
     out+=[tuple(sorted(V(base+k+j) for j in range(3))) for k in (range(0,cn,3) if tp==0 else range(cn-2))];base+=cn
  out.sort();open(dump,'wb').write(b''.join(struct.pack('<30I',*(x for v in tr for x in v)) for tr in out));print('dumped',len(out),'triangles to',dump)
 best=1e9
 for r in range(3):
  t0=time.perf_counter()
  for f in range(frames):t.calls.clear();t.run_dp2(cmds,vb,stride=36,fvf=0x152)
  best=min(best,(time.perf_counter()-t0)/frames)
 print('host: %.3f ms a frame (30 draws), %.1f us a draw, best of 3 x %d frames'%(best*1000,best*1e6/30,frames))
main()
