#!/usr/bin/env python3
"""tnl_retry_cases.py (.663, audit 03): a DrawPrimitives2 call whose queued packets are refused as busy is retried by the runtime from the
error offset; the driver's T&L state (material, transforms, lights, depth range) changed by commands after that offset must be put back
first, so the retry draws what the uninterrupted call draws. Cases: the material between two draws, a world matrix, a light, the depth
range; busy with nothing taken, and a full undo log (the queue sent before a T&L command).
usage: tnl_retry_cases.py [core.so]"""
import sys,os,struct,ctypes as C
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import test_core as T
if len(sys.argv)>1:T.lib=C.CDLL(os.path.abspath(sys.argv[1]))
lib=T.lib;U=T.U;ON=lambda n:U.in_dll(lib,n)
class B(T.Contract):
 def runTest(self):pass
def m4(*m):return struct.pack('<16f',*m)
I4=m4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
def mat(r,g,b):return T.cmd(33,1,struct.pack('<17f',0,0,0,1, r,g,b,1, 0,0,0,0, 0,0,0,0, 0))
def light(i,on,r,g,b):
 body=struct.pack('<I',3)+struct.pack('<4f',r,g,b,1)+struct.pack('<4f',0,0,0,0)+struct.pack('<4f',0,0,0,0)+struct.pack('<3f',0,0,0)+struct.pack('<3f',0,0,1)+struct.pack('<7f',1000,1,1,0,0,0,0)
 return T.cmd(34,2,struct.pack('<II',i,2)+body+struct.pack('<II',i,0 if on else 1))
def world(dx):return T.cmd(36,1,struct.pack('<I',1)+m4(1,0,0,0,0,1,0,0,0,0,1,0,dx,0,0,1))
def zrange(a,b):return T.cmd(32,1,struct.pack('<2f',a,b))
def tri(k):return T.cmd(26,1,struct.pack('<H',0)+struct.pack('<3H',3*k,3*k+1,3*k+2))
VS=b''.join(struct.pack('<6f',x,y,.5,0,0,-1) for k in range(30) for x,y in ((-.9+.06*k,-.5),(-.86+.06*k,-.5),(-.9+.06*k,-.45)))
def words(t):
 out=[]
 for p in t.calls:
  w=list(p.pre_w);b0=p.bursts.b[0]
  if p.pre_on==2:
   ni=b0&0xFFFF;sd=8 if b0&0x04000000 else 10;ix=(C.c_uint16*ni).from_buffer_copy(bytes(p.vertices)[:ni*2]);out+=[tuple(w[i*sd:i*sd+5]) for i in ix]
  else:out+=[tuple(w[i*10:i*10+5]) for i in range(p.vertex_count)]
 return out
def setup():
 t=B();t.setUp()
 for n,v in [('vx_d3d_feat',T.ALLF),('vx_d3d_tnl',1),('vx_d3d_prepack',1),('vx_d3d_ixpass',1),('vx_d3d_indexed',1)]:ON(n).value=v
 t._stages([(0,1,2,0,1,2,0,0)]);t.take=None;t.nres=0
 def submit_n(_,p,n,done):
  k=n if t.take is None else min(n,t.take.pop(0) if isinstance(t.take,list) else t.take)
  arr=C.cast(p,C.POINTER(T.Packet));t.calls.extend(T.Packet.from_buffer_copy(arr[i]) for i in range(k));done[0]=k
  return t.nres.pop(0) if isinstance(t.nres,list) else t.nres
 t.services.submit_n=T.SUBMITN(submit_n);t.sn=submit_n
 off=T.Surface(0x40000,256,64,64,1);lib.vx_d3d_init(C.byref(t.context),C.byref(off),None,7);t.context.state.rs[22]=1
 return t
PRE=T.cmd(28,1,struct.pack('<4I',0,0,64,64))+T.cmd(36,3,struct.pack('<I',1)+I4+struct.pack('<I',2)+I4+struct.pack('<I',3)+I4)+T.rs((137,1),(139,0xFFFFFFFF),(141,0),(147,0),(22,1),(28,0))
def check(name,body,fill=0):
 t=setup();s=PRE+body
 t.calls.clear();rc=t.run_dp2(s,VS,stride=24,fvf=0x12);assert rc==(0,0),(name,rc);want=words(t)
 t=setup();t.take=0;t.nres=3 # busy, nothing taken
 rc,err=t.run_dp2(s,VS,stride=24,fvf=0x12);rc&=0xFFFFFFFF
 if (rc,err)==(0,0):return 'no busy answer (nothing queued?)'
 assert rc==0x8876021C,(name,hex(rc))
 t.take=None;t.nres=0;t.calls.clear();rc2=t.run_dp2(s[err:],VS,stride=24,fvf=0x12);assert rc2==(0,0),(name,rc2)
 got=words(t)
 return None if got==want else 'retry drew %d words differing (of %d)'%(sum(1 for a,b in zip(got,want) if a!=b)+abs(len(got)-len(want)),len(want))
def main():
 cases=[('material',mat(1,0,0)+tri(0)+mat(0,0,1)+tri(1)),
  ('world matrix',mat(1,0,0)+tri(0)+world(.3)+tri(1)),
  ('light',mat(1,1,1)+light(0,1,1,0,0)+T.rs((139,0))+tri(0)+light(0,1,0,1,0)+tri(1)),
  ('light added',mat(1,1,1)+T.rs((139,0))+light(0,1,1,0,0)+tri(0)+light(5,1,0,0,1)+tri(1)),
  ('depth range',mat(1,0,0)+tri(0)+zrange(.2,.4)+tri(1)),
  ('full log',mat(1,0,0)+b''.join(tri(k%20)+world(.01*k)+mat(.02*k,0,1) for k in range(40)))]
 bad=0
 for name,body in cases:
  r=check(name,body)
  print('%-14s %s'%(name,r or 'the retry draws what one call draws'));bad+=r is not None
 if hasattr(lib,'vx_d3d_tnl_undo_stat'):u=(U*2).in_dll(lib,'vx_d3d_tnl_undo_stat');print('undo: %d entries put back, %d sends for a full log'%(u[0],u[1]))
 return 1 if bad else 0
sys.exit(main())
