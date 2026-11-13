#!/usr/bin/env python3
"""tnl_544_cases.py: the driver's T&L cases .544's first cut got wrong (reproduced against .543), as a dump of the packets'
active vertex words and counters for a built tree: ZBIAS on the indexed path (the conversion), a base vertex of 65535 (the
index less base), linear fog's byte (the divide), the side-plane capacity fallback (the guard test before the packet). Run it
against two trees and diff: python3 tests/tnl_544_cases.py <tree> """
import hashlib,json,struct,sys
from pathlib import Path
root=Path(sys.argv[1]).resolve() if len(sys.argv)>1 else Path(__file__).resolve().parents[1]
sys.path.insert(0,str(root/'tests'))
from test_core import *
def tri_list(seq,base=0):return cmd(26,len(seq),struct.pack('<H',base)+b''.join(struct.pack('<3H',*x) for x in seq))
def verts(coords,col=0xffeeba98):return b''.join(struct.pack('<3fI2f',*xyz,col,.25,.75) for xyz in coords)
def sample(name,draws,data,pre=b'',repeats=1,stride=24,fvf=0x142):
 t=Contract('test_tnl');t.setUp()
 for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',1),('vx_d3d_ixpass',1),('vx_d3d_indexed',1),('vx_d3d_v8',1)]:U.in_dll(lib,n).value=v
 t._stages([(7,4,2,0,2,2,0,0)])
 d=(U*12).in_dll(lib,'vx_d3d_tn')
 for j in range(len(d)):d[j]=0
 for _ in range(repeats):
  r=t.run_dp2(rs((137,0),(28,0),(22,1))+pre+draws,data,stride=stride,fvf=fvf);assert r==(0,0),(name,r)
 h=hashlib.sha256();words=[]
 for p in t.calls:
  assert lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0,name
  if p.pre_on==2:
   b0=p.bursts.b[0];stride=8 if b0&0x04000000 else 10;n=b0&65535
   for v in struct.unpack_from(f'<{n}H',bytes(p.vertices)):w=p.pre_w[v*stride:(v+1)*stride];h.update(pack(*w));words.append([f'{x:08x}' for x in w])
  else:
   h.update(bytes(p.vertices)[:p.vertex_count*C.sizeof(Vertex)])
   if p.pre_on:h.update(pack(*p.pre_w[:p.vertex_count*10]))
 return name,[len(t.calls),h.hexdigest(),list(d),words[:6]]
v=verts([(-.75,-.75,.5),(.75,-.75,.5),(.25,.75,.5),(1.5,.25,.5),(-2,-.5,-.2),(.25,-.75,.2)])
side=[(0,1,3),(0,2,3)];small=tri_list(side+[(1,4,5)])
fog=rs((28,1),(140,3),(36,struct.unpack('<I',struct.pack('<f',0.1))[0]),(37,struct.unpack('<I',struct.pack('<f',0.9))[0]),(35,0))
cases=[sample('ZBIAS 16, a triangle at depth .5',tri_list([(0,1,2)]),v,rs((47,16))),
       sample('ZBIAS 3',tri_list([(0,1,2)]),v,rs((47,3))),
       sample('base 65535, indices 1 2 3',tri_list([(1,2,3)],65535),bytes(24*65535)+v),
       sample('base 65535, 3600 triangles',tri_list([(1,2,3),(0,1,2)]*1800,65535),bytes(24*65535)+v),
       sample('linear fog, the byte',tri_list([(0,1,2),(1,2,5)]),v,fog),
       sample('linear fog, range fog',tri_list([(0,1,2),(1,2,5)]),v,fog+rs((48,1))),
       sample('side capacity fallback',tri_list(side*1800),v),
       sample('side capacity fallback with near clips',tri_list((side+[(1,4,5)])*1100),v)]
# Each call uses new packet-local slots for a fresh copy of the same scratch
# indices. More than 127 resets also exercises the compact stamp's wrap.
wrap=verts([(-.75,-.75,.5),(.75,-.75,.5),(.75,.75,.5),(-.75,.75,.5),(0.,0.,.5)])
cases.append(sample('packet slot stamp wrap',tri_list([(0,1,4),(1,2,4),(2,3,4),(3,0,4)]),wrap,repeats=135))
# Eye Z=1, clip Z=0.5: division gives alpha 212 and reciprocal multiplication 213.
proj=(1,0,0,0,0,1,0,0,0,0,0,0,0,0,.5,1)
eye=verts([(-.75,-.75,1.),(.75,-.75,1.),(.25,.75,1.)])
linear=cmd(36,1,pack(3)+struct.pack('<16f',*proj))+rs((28,1),(35,0),(140,3),(36,bits(0.)),(37,bits(6.)))
cases.append(sample('linear fog division tie at 212',tri_list([(0,1,2)]),eye,linear))
# Three groups: the first calculates diffuse, the second can reuse it,
# and a changed normal in the third forces the full calculation again.
positions=[(-.6,-.5),(.6,-.5),(0.,.6)]
cache_vb=b''.join(struct.pack('<6fI2f',*positions[i%3],1+i/64,0,0,
    1 if i<9 else -1,0xffc0a080,.25,.75) for i in range(12))
cache_tris=tri_list([(i,i+1,i+2) for i in range(0,12,3)])
perspective=(1,0,0,0,0,1,0,0,0,0,1,1,0,0,-.5,0)
light=cmd(34,2,pack(0,2,3)+struct.pack('<25f',
    1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,-1,0,0,0,0,0,0,0)+pack(0,0))
mat=cmd(33,1,struct.pack('<17f',1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0))
setup=cmd(36,1,pack(3)+struct.pack('<16f',*perspective))+light+mat
setup+=rs((137,1),(28,1),(35,0),(140,3),(36,bits(0.)),(37,bits(2.)),(145,1))
cases.append(sample('grouped diffuse reuse then changed normal',cache_tris,cache_vb,setup+rs((141,0)),stride=36,fvf=0x152))
cases.append(sample('scalar diffuse reuse then changed normal',cache_tris,cache_vb,setup+rs((141,1)),stride=36,fvf=0x152))
# A draw that clips out completely must not change the visible draw that
# follows it, even when the two draws reference the same vertex buffer.
def world_x(x):
 return cmd(36,1,pack(1)+struct.pack('<16f',1,0,0,0,0,1,0,0,0,0,1,0,x,0,0,1))
cv_setup=setup+rs((141,1))
_,visible=sample('visible',cache_tris,cache_vb,cv_setup,stride=36,fvf=0x152)
_,after_reject=sample('fully rejected before visible',world_x(8.)+cache_tris+
    world_x(0.)+cache_tris,cache_vb,cv_setup,stride=36,fvf=0x152)
assert after_reject[:2]==visible[:2],(after_reject[:2],visible[:2])
print(json.dumps(dict(cases),sort_keys=True,indent=1))
