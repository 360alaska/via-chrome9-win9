#!/usr/bin/env python3
"""tnl_tfog_cases.py (.733, ACCEL_D3D_TFOG): the driver's T&L leaves depth vertex fog to the engine's fog unit from W when the projection's
clip W is eye Z exactly. Checked on tests/core.so: the packets' fog registers (01/70 mode, bit 4 clear = W), no fog worked out by the T&L
(vx_d3d_tn[9]), the vertex words the same as with the switch off apart from the specular word's alpha (the vertex fog), and every case the
switch must leave alone (range fog, a projection whose W is not eye Z, the switch off). Exit 1 on a failure."""
import struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_core import *
def tri_list(seq,base=0):return cmd(26,len(seq),struct.pack('<H',base)+b''.join(struct.pack('<3H',*x) for x in seq))
def verts(coords,col=0xffeeba98):return b''.join(struct.pack('<3fI2f',*xyz,col,.25,.75) for xyz in coords)
PERSP=(1,0,0,0,0,1,0,0,0,0,1,1,0,0,-.5,0)   # clip W = eye Z (fourth column 0 0 1 0)
PERSP2=(1,0,0,0,0,1,0,0,0,0,1,2,0,0,-.5,0)  # clip W = 2 eye Z: not taken
def run(tfog,mode=3,proj=PERSP,rfog=0):
 t=Contract('test_tnl');t.setUp()
 for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',1),('vx_d3d_ixpass',1),('vx_d3d_indexed',1),('vx_d3d_v8',1),('vx_d3d_tfog',tfog)]:U.in_dll(lib,n).value=v
 t._stages([(7,4,2,0,2,2,0,0)])
 d=(U*12).in_dll(lib,'vx_d3d_tn')
 for j in range(len(d)):d[j]=0
 n0=U.in_dll(lib,'vx_d3d_tfog_n').value
 v=verts([(-.75,-.75,2.),(.75,-.75,3.),(.25,.75,6.),(-.5,.5,9.),(.6,.4,4.)])
 st=cmd(36,1,pack(3)+struct.pack('<16f',*proj))+rs((137,0),(22,1),(28,1),(34,0x406080),(35,0),(140,mode),(36,bits(1.)),(37,bits(10.)),(38,bits(.2)),(48,rfog))
 r=t.run_dp2(st+tri_list([(0,1,2),(1,2,3),(2,3,4)]),v,stride=24,fvf=0x142);assert r==(0,0),r
 U.in_dll(lib,'vx_d3d_tfog').value=0
 words=[]
 for p in t.calls:
  assert lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0
  if p.pre_on==2:
   b0=p.bursts.b[0];s=8 if b0&0x04000000 else 10;n=b0&65535
   for k in struct.unpack_from(f'<{n}H',bytes(p.vertices)):words.append(tuple(p.pre_w[k*s:(k+1)*s]))
  else:
   for k in range(p.vertex_count):vx=p.vertices[k];words.append((vx.x,vx.y,vx.z,vx.rhw,vx.color,p.shader.spec[k]))
 return [(p.fog.enable,p.fog.r[0]) for p in t.calls],d[9],U.in_dll(lib,'vx_d3d_tfog_n').value-n0,words
bad=[]
def check(name,ok,got):
 print(('PASS ' if ok else 'FAIL ')+name+('' if ok else ' '+repr(got)));(None if ok else bad.append(name))
on=run(1);off=run(0)
check('linear depth fog, W = eye Z: the engine fogs from W (01/70 = 02, as XP), the T&L works out none',all(f==(1,0x02) for f in on[0]) and on[0] and on[1]==0 and on[2]>0,on[:3])
check('the switch off: vertex fog as before (01/70 = 30), worked out by the T&L',all(f==(1,0x30) for f in off[0]) and off[1]>0 and off[2]==0,off[:3])
strip=lambda ws:[w[:5]+tuple(x for x in w[6:]) for w in ws]
check('positions, rhw, diffuse and coordinates the same either way',strip(on[3])==strip(off[3]) and len(on[3])>0,(len(on[3]),len(off[3])))
check('with it on, the specular word carries no vertex fog (alpha 255)',all(w[5]>>24==0xFF for w in on[3]),[hex(w[5]) for w in on[3][:4]])
check('with it off, the vertex fog is in the specular alpha (not all 255)',any(w[5]>>24!=0xFF for w in off[3]),[hex(w[5]) for w in off[3][:4]])
e=run(1,mode=1);e2=run(1,mode=2)
check('EXP and EXP2 depth fog: 01/70 = 04 and 05',all(f==(1,0x04) for f in e[0]) and all(f==(1,0x05) for f in e2[0]) and e[1]==0 and e2[1]==0,(e[:2],e2[:2]))
r=run(1,rfog=1)
check('range fog stays vertex fog',all(f==(1,0x30) for f in r[0]) and r[1]>0 and r[2]==0,r[:3])
p2=run(1,proj=PERSP2)
check('a projection whose W is not eye Z stays vertex fog',all(f==(1,0x30) for f in p2[0]) and p2[1]>0 and p2[2]==0,p2[:3])
sys.exit(1 if bad else 0)
