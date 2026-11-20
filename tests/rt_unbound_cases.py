#!/usr/bin/env python3
"""rt_unbound_cases.py (.649): a context whose target surface was destroyed while bound (3DMark 2001 on .648: the context was marked lost
and every ValidateDevice answered SURFACELOST). The HAL forgets the description (unbind_surface); the core must then draw nothing, clear
nothing and answer OK, and ValidateDevice must judge the states alone (vx_d3d_supported_unbound). A depth surface forgotten draws without
depth. usage: rt_unbound_cases.py [core.so]    (exit 1 on a failure)"""
import sys,os,struct,ctypes as C
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import test_core as T
if len(sys.argv)>1:T.lib=C.CDLL(os.path.abspath(sys.argv[1]))
lib=T.lib;U=T.U
class B(T.Contract):
 def runTest(self):pass
bad=0
def check(name,ok,*info):
 global bad
 print(('ok  ' if ok else 'FAIL'),name,*info);bad+=not ok
vs=b''.join(struct.pack('<4fI',10+i*20,10+i*10,.5,1,0xFFFFFFFF) for i in range(3))
clear=T.cmd(42,1,struct.pack('<IIfI',3,0xFF00FF00,1.0,0)+struct.pack('<4i',0,0,64,64))
tri=T.cmd(19,1,struct.pack('<H',0))
t=B();t.setUp()
for n,v in [('vx_d3d_feat',T.ALLF),('vx_d3d_tnl',0)]:U.in_dll(lib,n).value=v
t.calls.clear();rc=t.run_dp2(clear+tri,vs,stride=20,fvf=0x44);n0=len(t.calls)
check('bound: drawn',rc==(0,0) and n0>0 and lib.vx_d3d_supported(C.byref(t.context))==1,'rc',rc,'packets',n0)
z=bytes(t.context.depth)
C.memset(C.byref(t.context.depth),0,C.sizeof(t.context.depth))
t.calls.clear();rc=t.run_dp2(clear+tri,vs,stride=20,fvf=0x44)
check('the depth surface forgotten: drawn without depth',rc==(0,0) and len(t.calls)>0 and all(p.depth.format==0 for p in t.calls),'rc',rc,'packets',len(t.calls))
C.memset(C.byref(t.context.target),0,C.sizeof(t.context.target))
t.calls.clear();rc=t.run_dp2(clear+tri,vs,stride=20,fvf=0x44)
check('the target forgotten: nothing drawn or cleared, the call answers OK',rc==(0,0) and len(t.calls)==0,'rc',rc,'packets',len(t.calls))
check('the target forgotten: ValidateDevice judges the states alone',lib.vx_d3d_supported_unbound(C.byref(t.context))==1 and lib.vx_d3d_supported(C.byref(t.context))==0)
print('PASS' if not bad else 'FAILED %d'%bad);sys.exit(1 if bad else 0)
