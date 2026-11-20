#!/usr/bin/env python3
"""tnl_stage_sets.py (.646): the driver's T&L output coordinate sets by what the stages ask for. Before .646 stage 2 always got stage 1's
set (3DMark 2001 on the board: 453,331 draws counted 'state not done'), and two stages reading the same set made two sets.
usage: tnl_stage_sets.py [core.so]    (exit 1 on a failure)"""
import sys,os,struct,ctypes as C
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import test_core as T
if len(sys.argv)>1:T.lib=C.CDLL(os.path.abspath(sys.argv[1]))
lib=T.lib;U=T.U
class B(T.Contract):
 def runTest(self):pass
def m4(*m):return struct.pack('<16f',*m)
I4=m4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
TN=(U*12).in_dll(lib,'vx_d3d_tn')
def run(stages):
 vs=b''
 for i,(x,y) in enumerate([(-.5,-.5),(.5,-.5),(0,.5)]):vs+=struct.pack('<3f4f',x,y,.5,(i+1)/8.0,.25,(i+1)/16.0,.75)
 pre=T.cmd(28,1,struct.pack('<4I',0,0,64,64))+T.cmd(36,3,struct.pack('<I',1)+I4+struct.pack('<I',2)+I4+struct.pack('<I',3)+I4)+T.rs((137,0),(22,1),(28,0))
 draw=T.cmd(26,1,struct.pack('<H',0)+struct.pack('<3H',0,1,2))
 t=B();t.setUp()
 for n,v in [('vx_d3d_feat',T.ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',1),('vx_d3d_ixpass',1),('vx_d3d_indexed',1)]:U.in_dll(lib,n).value=v
 t._stages(stages);n11=TN[11]
 t.calls.clear();rc=t.run_dp2(pre+draw,vs,stride=28,fvf=0x202)
 after=[t.context.state.tss[k][11] for k in range(3)]
 uv=None
 for p in t.calls:
  w=list(p.pre_w);f=lambda i:round(struct.unpack('<f',struct.pack('<I',w[i]))[0],4)
  b0=p.bursts.b[0];sd=(8 if b0&0x04000000 else 10) if p.pre_on==2 else 10
  ix=list(struct.unpack('<%dH'%(b0&0xFFFF),bytes(p.vertices)[:(b0&0xFFFF)*2]))[:3] if p.pre_on==2 else [0,1,2] # an indexed packet: its first triangle's vertices
  uv=[tuple(f(v*sd+j) for j in range(6,sd)) for v in ix] # each vertex's coordinate words (u0 v0, then u1 v1 in the 10-word form)
  uv=(p.stages.units,list(p.stages.set),uv)
  break
 return rc,TN[11]-n11,after,uv
bad=0
S=lambda tci:(7,4,2,0,2,2,0,tci)
for name,st,want_extra,want_sets in [("stages 0 and 1 both read set 0",[S(0),S(0)],0,2),
 ("base, light map, detail (sets 0, 1, 0)",[S(0),S(1),S(0)],0,2),("sets 0, 1, 1",[S(0),S(1),S(1)],0,2),("three different sets: the third counted",[S(0),S(1),(7,4,2,0,2,2,0,0x10000)],1,2)]:
 rc,extra,after,uv=run(st)
 units,sets,words=uv
 want=list(range(len(st)))if len(st)<3 else[0,1,0 if st[2][7]==st[0][7] else 1] # stages 0 and 1 their own sets; stage 2 the one that matches it (else the second, counted)
 ok=rc==(0,0)and extra==want_extra and after==[x[7] for x in st]+[0,1,2][len(st):]and sets[:len(st)]==want # the packet's coordinate set by unit; the stages put back after the draw
 print(('ok  ' if ok else 'FAIL'),name,'rc',rc,'counted',extra,'stage TEXCOORDINDEX after the draw',after,'units',units,'sets',sets[:len(st)])
 bad+=not ok
print('PASS' if not bad else 'FAILED %d'%bad);sys.exit(1 if bad else 0)
