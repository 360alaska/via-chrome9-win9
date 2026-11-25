#!/usr/bin/env python3
"""tnl_set3_cases.py (.734): the driver's T&L gives a third stage on a third coordinate set an output set of its own (records 48 bytes,
words 10-11), which the core sends as unit 2's own set (slot C: stages.set[2] = 2, S and T in stages.q / stages.r1). Checked on
tests/core.so: the slot map, unit 2's coordinates as the vertices' third set (read; through stage 2's texture matrix), the first two sets
unchanged, the VxD's check takes the packet; with vx_d3d_tn3 0, stage 2 on set 1 as before. Exit 1 on a failure."""
import struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_core import *
f=lambda w:struct.unpack('<f',struct.pack('<I',w))[0]
ID=(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
PTS=[(-.5,.5,.1,.2,.3,.4,.5,.6),(.5,.5,.7,.8,.9,.15,.25,.35),(-.5,-.5,.45,.55,.65,.75,.85,.95)]
def run(tn3=1,mat=None):
 t=Contract('test_tnl');t.setUp()
 for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_tn3',tn3)]:U.in_dll(lib,n).value=v
 T,CUR=2,1
 t._stages([(5,4,T,CUR,2,T,CUR,0),(5,4,T,CUR,2,CUR,T,1),(5,4,T,CUR,2,CUR,T,2)])
 vb=b''.join(struct.pack('<3fI6f',x,y,.5,0xFFFFFFFF,*uv) for x,y,*uv in PTS)
 pre=cmd(36,3,pack(1)+struct.pack('<16f',*ID)+pack(2)+struct.pack('<16f',*ID)+pack(3)+struct.pack('<16f',*ID))+rs((137,0),(22,1),(28,0))
 if mat:pre+=cmd(36,1,pack(18)+struct.pack('<16f',*mat));t.context.state.tss[2][24]=2
 n0=U.in_dll(lib,'vx_d3d_tn3_n').value
 r=t.run_dp2(pre+cmd(26,1,struct.pack('<H',0)+struct.pack('<3H',0,1,2)),vb,stride=40,fvf=0x342)
 U.in_dll(lib,'vx_d3d_tn3').value=1;t.context.state.tss[2][24]=0
 return t,r,U.in_dll(lib,'vx_d3d_tn3_n').value-n0
bad=0
def check(name,ok,got):
 global bad;print(('PASS ' if ok else 'FAIL ')+name+('' if ok else ' '+repr(got)));bad+=not ok
t,r,n=run()
p=t.calls[0] if t.calls else None
got=p and sorted((round(f(p.stages.q[k]),4),round(f(p.stages.r1[k]),4)) for k in range(3))
check('three stages on three sets: unit 2 on slot C with the third set (read)',r==(0,0) and p is not None and n>0 and p.stages.units==3 and list(p.stages.set[:3])==[0,1,2] and got==sorted((round(q[6],4),round(q[7],4)) for q in PTS),(r,n,p and list(p.stages.set[:3]),got))
g1=p and sorted((round(f(p.stages.uv1[k][0]),4),round(f(p.stages.uv1[k][1]),4)) for k in range(3));g0=p and sorted((round(f(p.tex.uv[k][0]),4),round(f(p.tex.uv[k][1]),4)) for k in range(3))
check('the first two sets unchanged',g0==sorted((round(q[2],4),round(q[3],4)) for q in PTS) and g1==sorted((round(q[4],4),round(q[5],4)) for q in PTS),(g0,g1))
check('the VxD\'s check takes it',p is not None and lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0,None)
M=(1.5,0,0,0,0,.75,0,0,.25,.1,1,0,0,0,0,1) # a 2D set is (u, v, 1, 0): the move in the third row
t,r,n=run(mat=M);p=t.calls[0] if t.calls else None
got=p and sorted((round(f(p.stages.q[k]),4),round(f(p.stages.r1[k]),4)) for k in range(3))
check('stage 2 through its texture matrix (COUNT2): u 1.5u + .25, v .75v + .1',r==(0,0) and p is not None and list(p.stages.set[:3])==[0,1,2] and got==sorted((round(1.5*q[6]+.25,4),round(.75*q[7]+.1,4)) for q in PTS),(r,got))
t,r,n=run(tn3=0);p=t.calls[0] if t.calls else None
check('vx_d3d_tn3 0: stage 2 on set 1 as before',r==(0,0) and p is not None and n==0 and list(p.stages.set[:3])==[0,1,1],(r,n,p and list(p.stages.set[:3])))
sys.exit(1 if bad else 0)
