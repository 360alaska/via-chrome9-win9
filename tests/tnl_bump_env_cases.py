#!/usr/bin/env python3
"""tnl_bump_env_cases.py (.737): 3DMark 2001's Environment Bump Mapping layout through the driver's T&L (board: the alien flat on the VX800
and VX855, the YouTube reference metallic with a turning reflection). Stage 0 the base and stage 1 the bump read set 1, stage 2 the
environment map reads set 0 through its texture matrix (COUNT2). Stages 0 and 1 share output set 0, stage 2 takes set 1 with stage 2's
matrix: the packet is the bump program's two-set form (stages.set 0, 0, 1; XP's, CLXP3D T153), set A the vertices' set 1, set B the
matrix applied to set 0; the VxD's check takes it. Before .737 the T&L made three sets and the program refused them (the alien left
out), and 737B drew stage 2 on stage 1's coordinates (the reflection frozen). Checked on tests/core.so; exit 1 on a failure."""
import struct,sys
from pathlib import Path
sys.path.insert(0,str(Path(__file__).resolve().parent))
from test_core import *
f=lambda w:struct.unpack('<f',struct.pack('<I',w))[0]
ID=(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
PTS=[(-.5,.5,.1,.2,.3,.4),(.5,.5,.7,.8,.9,.15),(-.5,-.5,.45,.55,.65,.75)] # x y, set 0 (u v), set 1 (u v)
M=(1.5,0,0,0,0,.75,0,0,.25,.1,1,0,0,0,0,1) # a 2D set is (u, v, 1, 0): the move in the third row
bad=0
def check(name,ok,got):
 global bad;print(('PASS ' if ok else 'FAIL ')+name+('' if ok else ' '+repr(got)));bad+=not ok
t=Contract('test_tnl');t.setUp()
for n,v in [('vx_d3d_feat',ALLF),('vx_d3d_tnl',1),('vx_d3d_tn3',1)]:U.in_dll(lib,n).value=v
T,D,CUR=2,0,1
t._stages([(5,4,T,D,2,T,D,1),(11,22,T,CUR,4,T,CUR,1),(7,4,T,CUR,4,T,CUR,0)])
s1=t.context.state.tss[1];s1[7]=bits(.5);s1[8]=0;s1[9]=0;s1[10]=bits(.5)
t.context.state.tss[2][24]=2
vb=b''.join(struct.pack('<3fI4f',x,y,.5,0xFFFFFFFF,*uv) for x,y,*uv in PTS)
pre=cmd(36,3,pack(1)+struct.pack('<16f',*ID)+pack(2)+struct.pack('<16f',*ID)+pack(3)+struct.pack('<16f',*ID))+rs((137,0),(22,1),(28,0))
pre+=cmd(36,1,pack(18)+struct.pack('<16f',*M))
n0=U.in_dll(lib,'vx_d3d_tn_shared01').value
r=t.run_dp2(pre+cmd(26,1,struct.pack('<H',0)+struct.pack('<3H',0,1,2)),vb,stride=32,fvf=0x242)
t.context.state.tss[2][24]=0
p=t.calls[0] if t.calls else None
check('drawn (not refused)',r==(0,0) and p is not None,r)
check('stages 0 and 1 on one output set, stage 2 on the other: sets 0, 0, 1 under the bump program',p is not None and p.stages.units==3 and p.stages.ps==1 and list(p.stages.set[:3])==[0,0,1],p and (p.stages.units,p.stages.ps,list(p.stages.set[:3])))
check('the T&L shared the set',U.in_dll(lib,'vx_d3d_tn_shared01').value>n0,U.in_dll(lib,'vx_d3d_tn_shared01').value-n0)
gA=p and sorted((round(f(p.tex.uv[k][0]),4),round(f(p.tex.uv[k][1]),4)) for k in range(3))
gB=p and sorted((round(f(p.stages.uv1[k][0]),4),round(f(p.stages.uv1[k][1]),4)) for k in range(3))
check('set A: the vertices\' set 1 (base and bump)',gA==sorted((round(q[4],4),round(q[5],4)) for q in PTS),gA)
check('set B: set 0 through stage 2\'s matrix (the environment map): u 1.5u + .25, v .75v + .1',gB==sorted((round(1.5*q[2]+.25,4),round(.75*q[3]+.1,4)) for q in PTS),gB)
check('the VxD\'s check takes it',p is not None and lib.vx3dn_validate(C.byref(p),C.byref(t.info))==0,None)
sys.exit(1 if bad else 0)
