#!/usr/bin/env python3
"""review667_cases.py (.668, a review of .667): the host checks of the review's findings 1-3, each against the same state set directly.
 1a a state block holding one SETTRANSFORM of 15 world matrices (1,028 bytes recorded; .663-.667 kept 1,024 and lost it): applied
 1b a state block holding 20 SETTRANSFORM commands of one matrix each (past 1,024 bytes): applied
 2  state blocks in two contexts, more than eight live together (.663-.667 kept eight logs for every context together): the oldest applied
 3  65 light indices set, enabled and disabled in turn, then the first enabled again (.646-.667 cleared its definition for the 65th): lit
 3b the same through a state block that records the first light's enable
 4  transformed vertices with three coordinate sets, three stages on sets 0, 1, 2 (HarborBench's stages3; .667 left them out): drawn,
    unit 2 on the second set's slot
usage: review667_cases.py [core.so]    (exit 1 when any case fails)"""
import sys,os,struct,ctypes as C
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import test_core as T
if len(sys.argv)>1:T.lib=C.CDLL(os.path.abspath(sys.argv[1]))
A=type(sys)('audit663_helpers');A.__file__=os.path.join(os.path.dirname(os.path.abspath(__file__)),'audit663_cases.py') # its helpers without running its cases
_src=open(os.path.join(os.path.dirname(os.path.abspath(__file__)),'audit663_cases.py')).read();exec(_src[:_src.index('\ndef main():')],A.__dict__)
mk,m4,I4,PRE,outv,sset,VN,TRI,mat=A.mk,A.m4,A.I4,A.PRE,A.outv,A.sset,A.VN,A.TRI,A.mat
def wm(dx):return m4(1,0,0,0, 0,1,0,0, 0,0,1,0, dx,0,0,1)
def world(*ms):return T.cmd(36,len(ms),b''.join(struct.pack('<I',1)+m for m in ms))
ST=PRE+T.rs((137,1),(139,0xFFFFFFFF),(141,0),(147,0),(22,1),(28,0))+mat(1,1,1)
def xs(t):return [round(v[0],2) for v in outv(t)]
def direct(dx):
 t=mk();t.calls.clear();rc=t.run_dp2(ST+world(wm(dx))+TRI,VN,stride=24,fvf=0x12);return rc,xs(t)
def c1a():
 ms=[wm(0.01*k) for k in range(14)]+[wm(0.5)]
 t=mk();t.calls.clear();rc=t.run_dp2(ST+sset(0,7)+world(*ms)+sset(1,7)+world(I4)+sset(3,7)+TRI,VN,stride=24,fvf=0x12)
 want=direct(0.5);return rc==(0,0) and want[0]==(0,0) and xs(t)==want[1],(xs(t),want[1])
def c1b():
 s=ST+sset(0,8)+b''.join(world(wm(0.01*k)) for k in range(19))+world(wm(0.25))+sset(1,8)+world(I4)+sset(3,8)+TRI
 t=mk();t.calls.clear();rc=t.run_dp2(s,VN,stride=24,fvf=0x12)
 want=direct(0.25);return rc==(0,0) and xs(t)==want[1],(xs(t),want[1])
def c2():
 a=mk();b=mk();a.calls.clear();b.calls.clear()
 ra=a.run_dp2(ST+b''.join(sset(0,10+k)+world(wm(0.1*(k+1)))+sset(1,10+k) for k in range(5))+world(I4),VN,stride=24,fvf=0x12)
 rb=b.run_dp2(ST+b''.join(sset(0,20+k)+world(wm(-0.1*(k+1)))+sset(1,20+k) for k in range(5))+world(I4),VN,stride=24,fvf=0x12)
 a.calls.clear();ra2=a.run_dp2(sset(3,10)+TRI,VN,stride=24,fvf=0x12)
 want=direct(0.1);return ra==rb==ra2==(0,0) and xs(a)==want[1],(xs(a),want[1])
def light(i,r,g,b):return struct.pack('<2I',i,2)+struct.pack('<I',3)+struct.pack('<12f',r,g,b,1,0,0,0,0,0,0,0,0)+struct.pack('<6f',0,0,0,0,0,1)+struct.pack('<7f',0,0,1,0,0,0,0)
def en(i,on):return struct.pack('<2I',i,0 if on else 1)
LT=PRE+T.rs((137,1),(139,0),(141,0),(147,0),(22,1),(28,0))+T.cmd(33,1,struct.pack('<17f',1,1,1,1, 0,0,0,0, 0,0,0,0, 0,0,0,0, 0))
def defs():return b''.join(T.cmd(34,3,light(i,*((1,0,0) if i==0 else (0,0,1)))+en(i,1)+en(i,0)) for i in range(65))
def c3():
 t=mk();t.calls.clear();rc=t.run_dp2(LT+defs()+T.cmd(34,1,en(0,1))+TRI,VN,stride=24,fvf=0x12);d=['%08X'%v[3] for v in outv(t)]
 return rc==(0,0) and d==['FFFF0000']*3,d
def c3b():
 t=mk();t.calls.clear();rc=t.run_dp2(LT+defs()+sset(0,30)+T.cmd(34,1,en(0,1))+sset(1,30)+sset(3,30)+TRI,VN,stride=24,fvf=0x12);d=['%08X'%v[3] for v in outv(t)]
 return rc==(0,0) and d==['FFFF0000']*3,d
def c4():
 t=mk(0);t._stages([(5,4,2,0,2,2,0,0),(5,4,2,1,2,2,1,1),(5,4,2,1,2,2,1,2)])
 vs=b''.join(struct.pack('<4fI6f',x,y,.5,1,0xFFFFFFFF,u,v,u*2,v*2,1-u,v) for (x,y),(u,v) in zip([(8,8),(56,8),(8,56)],[(0,0),(1,0),(0,1)]))
 t.calls.clear();rc=t.run_dp2(T.rs((22,1),(28,0))+t.tri(),vs,stride=44,fvf=0x344)
 got=[(p.stages.units,list(p.stages.set[:3])) for p in t.calls];return rc==(0,0) and got==[(3,[0,1,1])],(rc,got)
def main():
 bad=0
 for name,f in [('1a',c1a),('1b',c1b),('2',c2),('3',c3),('3b',c3b),('4',c4)]:
  ok,info=f();print('review %s: %s %s'%(name,'PASS' if ok else 'FAIL',info));bad+=not ok
 return 1 if bad else 0
sys.exit(main())
