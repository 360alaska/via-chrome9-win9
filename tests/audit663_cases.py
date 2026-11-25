#!/usr/bin/env python3
"""audit663_cases.py (.663, a review of .661): the host checks of the findings fixed in .663, each against what Direct3D draws.
 09 a legacy TEXTUREADDRESS repeated after a stage's own ADDRESSU is applied again (the redundant-state shortcut kept the clamp)
 11 a clipped flat-shaded triangle through the driver's T&L keeps its first vertex's colour (the vertices made by clipping took theirs)
 12 flat shading keeps each vertex's fog factor (the specular alpha)
 14 a TRIANGLEFAN_IMM through the driver's T&L binds no vertex buffer (it carries its vertices)
 15 a wide point draw (more than TN_MAX vertices between its indices) reads index 65536 as 65536, not 0
 16 a state block's material, applied
 17 a state block's depth range: not applied while recording, applied with the block
usage: audit663_cases.py [core.so]    (exit 1 when any case fails)"""
import sys,os,struct,ctypes as C
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
import test_core as T
if len(sys.argv)>1:T.lib=C.CDLL(os.path.abspath(sys.argv[1]))
lib=T.lib;U=T.U;ON=lambda n:U.in_dll(lib,n)
class B(T.Contract):
 def runTest(self):pass
def m4(*m):return struct.pack('<16f',*m)
I4=m4(1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1)
PRE=T.cmd(28,1,struct.pack('<4I',0,0,64,64))+T.cmd(36,3,struct.pack('<I',1)+I4+struct.pack('<I',2)+I4+struct.pack('<I',3)+I4)
def mk(tnl=1):
 t=B();t.setUp()
 for n,v in [('vx_d3d_feat',T.ALLF),('vx_d3d_tnl',tnl),('vx_d3d_prepack',1),('vx_d3d_ixpass',1),('vx_d3d_indexed',1)]:ON(n).value=v
 t._stages([(0,1,2,0,1,2,0,0)]);return t
def outv(t):
 """every packet vertex as (x, y, z, diffuse, specular)"""
 r=[]
 for p in t.calls:
  w=list(p.pre_w);f=lambda x:struct.unpack('<f',struct.pack('<I',x))[0]
  if p.pre_on==2:
   b0=p.bursts.b[0];ni=b0&0xFFFF;sd=8 if b0&0x04000000 else 10;ix=(C.c_uint16*ni).from_buffer_copy(bytes(p.vertices)[:ni*2])
   r+=[(f(w[i*sd]),f(w[i*sd+1]),f(w[i*sd+2]),w[i*sd+4],w[i*sd+5]) for i in ix]
  elif p.pre_on==1:r+=[(f(w[i*10]),f(w[i*10+1]),f(w[i*10+2]),w[i*10+4],w[i*10+5]) for i in range(p.vertex_count)]
  else:r+=[(p.vertices[i].x,p.vertices[i].y,p.vertices[i].z,p.vertices[i].color,p.shader.spec[i]) for i in range(p.vertex_count)]
 return r
def mat(r,g,b):return T.cmd(33,1,struct.pack('<17f',0,0,0,1, r,g,b,1, 0,0,0,0, 0,0,0,0, 0))
def sset(op,h):return T.cmd(39,1,struct.pack('<3I',op,h,0))
VN=b''.join(struct.pack('<6f',x,y,.5,0,0,-1) for x,y in ((-.5,-.5),(.5,-.5),(-.5,.5)))
TRI=T.cmd(26,1,struct.pack('<H',0)+struct.pack('<3H',0,1,2))
def c09():
 t=mk(0);t._stages([(7,4,2,0,2,2,0,0)])
 t.calls.clear();rc=t.run_dp2(T.rs((3,1))+t.tri()+T.cmd(25,1,struct.pack('<HHI',0,13,3))+t.tri()+T.rs((3,1))+t.tri())
 a=[p.tex.address_u for p in t.calls];return rc==(0,0) and len(a)==3 and a[0]==a[2]!=a[1],a
def c11():
 t=mk();vs=b''.join(struct.pack('<3fI',x,y,.5,c) for (x,y),c in zip([(1.6,-.4),(-.5,-.5),(-.5,.5)],[0xFFFF0000,0xFF00FF00,0xFF0000FF]))
 t.calls.clear();rc=t.run_dp2(PRE+T.rs((137,0),(9,1),(22,1),(28,0))+TRI,vs,stride=16,fvf=0x42);d={v[3] for v in outv(t)}
 return rc==(0,0) and d=={0xFFFF0000},sorted('%08X'%x for x in d)
def c12():
 t=mk(0);vs=b''.join(struct.pack('<4fII',x,y,.5,1,c,f<<24) for (x,y),c,f in zip([(8,8),(56,8),(8,56)],[0xFFFF0000,0xFF00FF00,0xFF0000FF],[32,128,224]))
 t.calls.clear();rc=t.run_dp2(T.rs((9,1),(28,1))+t.tri(),vs,stride=24,fvf=0xC4);f=[v[4]>>24 for v in outv(t)]
 return rc==(0,0) and f==[32,128,224],f
def c14():
 t=mk();calls=[]
 def bind(_,p):calls.append(1);return 0x887601F4
 cb=T.BIND(bind);t.services.bind_vertices=cb
 imm=struct.pack('<3fI',-.5,-.5,.5,0xFFFF0000)+struct.pack('<3fI',.5,-.5,.5,0xFF00FF00)+struct.pack('<3fI',-.5,.5,.5,0xFF0000FF)
 body=T.rs((137,0),(22,1),(28,0))+struct.pack('<BBHI',23,0,1,0)+imm
 t.calls.clear();rc=t.run_dp2(PRE+body,b'',count=0,stride=16,fvf=0x42);return rc==(0,0) and not calls and len(outv(t))==3,(rc,len(calls),len(outv(t)))
def c15():
 t=mk();N=65540;vs=b''.join(struct.pack('<3fI',0,0,.1+.8*i/N,0xFF000000|i) for i in range(N))
 t.calls.clear();rc=t.run_dp2(PRE+T.rs((137,0),(22,1),(28,0))+T.cmd(1,2,struct.pack('<HH',1,0)+struct.pack('<HH',2,65535)),vs,stride=16,fvf=0x42)
 got=sorted({v[3]&0xFFFFFF for v in outv(t)});return rc==(0,0) and got==[0,0xFFFF,0x10000],got
def c16():
 t=mk();st=PRE+T.rs((137,1),(139,0xFFFFFFFF),(141,0),(147,0),(22,1),(28,0))
 s=st+sset(0,5)+mat(0,0,1)+sset(1,5)+mat(1,0,0)+TRI+sset(3,5)+TRI
 t.calls.clear();rc=t.run_dp2(s,VN,stride=24,fvf=0x12);d=['%06X'%(v[3]&0xFFFFFF) for v in outv(t)]
 return rc==(0,0) and d==['FF0000']*3+['0000FF']*3,d
def c17():
 t=mk();st=PRE+T.rs((137,0),(22,1),(28,0))
 vs=b''.join(struct.pack('<3fI',x,y,.5,0xFFFFFFFF) for x,y in ((-.5,-.5),(.5,-.5),(-.5,.5)))
 s=st+sset(0,6)+T.cmd(32,1,struct.pack('<2f',.2,.4))+sset(1,6)+TRI+T.cmd(32,1,struct.pack('<2f',0,1))+TRI+sset(3,6)+TRI
 t.calls.clear();rc=t.run_dp2(s,vs,stride=16,fvf=0x42);z=[round(v[2],3) for v in outv(t)]
 return rc==(0,0) and z==[.5]*6+[.3]*3,z
def c06():
 t=mk();t._stages([(5,2,2,0,2,2,0,0)]);t.cubes[5]=[0x30000+k*0x100 for k in range(6)]
 vs=b''.join(struct.pack('<3fI3f',x,y,.5,0xFFFFFFFF,.1,.2,r) for (x,y),r in zip([(-.5,-.5),(.5,-.5),(-.5,.5)],[.625,.25,.75]))
 t.calls.clear();rc=t.run_dp2(PRE+T.rs((137,0),(22,1),(28,0))+TRI,vs,stride=28,fvf=0x10142)
 p=t.calls[0] if t.calls else None
 if not p:return False,rc
 f=lambda x:round(struct.unpack('<f',struct.pack('<I',x))[0],3)
 if p.pre_on:
  w=list(p.pre_w);b0=p.bursts.b[0];sd=8 if b0&0x04000000 else 10
  q=[f(p.stages.q[i]) for i in range(p.vertex_count)]
 else:q=[f(p.stages.q[i]) for i in range(p.vertex_count)]
 return rc==(0,0) and p.stages.layout==1 and sorted(set(q))==[.25,.625,.75],(rc,p.stages.layout,q)
def c07():
 t=mk();t._stages([(5,2,2,0,2,2,0,0)]);t.context.state.tss[0][24]=2 # COUNT2: u' = r (the matrix puts the third input in u)
 M=m4(0,0,0,0, 0,1,0,0, 1,0,0,0, 0,0,0,1)
 vs=b''.join(struct.pack('<3fI3f',x,y,.5,0xFFFFFFFF,.1,.2,r) for (x,y),r in zip([(-.5,-.5),(.5,-.5),(-.5,.5)],[.25,.75,.5]))
 t.calls.clear();rc=t.run_dp2(PRE+T.cmd(36,1,struct.pack('<I',16)+M)+T.rs((137,0),(22,1),(28,0))+TRI,vs,stride=28,fvf=0x10142)
 u=sorted({round(v[6],3) for v in uvs(t)});return rc==(0,0) and u==[.25,.5,.75],(rc,u)
def c08():
 t=mk();t._stages([(5,2,2,0,2,2,0,0),(5,4,2,1,2,2,1,1),(5,4,2,1,2,2,1,0)]);t.context.state.tss[0][24]=2;t.context.state.tss[2][24]=2
 D=m4(2,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1)
 vs=b''.join(struct.pack('<3fI4f',x,y,.5,0xFFFFFFFF,.25,.5,.3,.4) for x,y in ((-.5,-.5),(.5,-.5),(-.5,.5)))
 t.calls.clear();rc=t.run_dp2(PRE+T.cmd(36,2,struct.pack('<I',16)+I4+struct.pack('<I',18)+D)+T.rs((137,0),(22,1),(28,0))+TRI,vs,stride=32,fvf=0x242)
 n=(U*12).in_dll(lib,'vx_d3d_tn')[11]
 # .734: stage 2 (set 0 through its own matrix: u x 2) gets the T&L's third output set (slot C): drawn, nothing counted as not done
 p=t.calls[0] if t.calls else None;f=lambda x:round(struct.unpack('<f',struct.pack('<I',x))[0],3)
 q=p and sorted({(f(p.stages.q[i]),f(p.stages.r1[i])) for i in range(p.vertex_count)})
 return rc==(0,0) and n==0 and p is not None and list(p.stages.set[:3])==[0,1,2] and q==[(.5,.5)],('not done',n,p and list(p.stages.set[:3]),q)
def uvs(t):
 r=[];f=lambda x:struct.unpack('<f',struct.pack('<I',x))[0]
 for p in t.calls:
  w=list(p.pre_w)
  if p.pre_on==2:
   b0=p.bursts.b[0];ni=b0&0xFFFF;sd=8 if b0&0x04000000 else 10;ix=(C.c_uint16*ni).from_buffer_copy(bytes(p.vertices)[:ni*2]);r+=[tuple(f(x) for x in w[i*sd:i*sd+8]) for i in ix]
  else:r+=[tuple(f(x) for x in w[i*10:i*10+8]) for i in range(p.vertex_count)]
 return r
def c13():
 def lines(t):return sum(p.vertex_count for p in t.calls)
 tri3=T.cmd(3,1,struct.pack('<4H',0,1,2,0x100)) # EDGEENABLE1 only
 t=mk(0);vs=b''.join(struct.pack('<4fI',x,y,.5,1,0xFFFFFFFF) for x,y in ((8,8),(56,8),(8,56)))
 t.calls.clear();rc0=t.run_dp2(T.rs((8,2),(22,1))+tri3,vs,stride=20,fvf=0x44);want=lines(t)
 t=mk();v2=b''.join(struct.pack('<3fI',x,y,.5,0xFFFFFFFF) for x,y in ((-.5,-.5),(.5,-.5),(-.5,.5)))
 t.calls.clear();rc1=t.run_dp2(PRE+T.rs((137,0),(8,2),(22,1),(28,0))+tri3,v2,stride=16,fvf=0x42);got=lines(t)
 t=mk();v3=b''.join(struct.pack('<3fI',x,y,.5,0xFFFFFFFF) for x,y in ((-.5,-.5),(1.5,-.5),(-.5,.5))) # clipped: edge 1 cut, still one line
 t.calls.clear();rc2=t.run_dp2(PRE+T.rs((137,0),(8,2),(22,1),(28,0))+tri3,v3,stride=16,fvf=0x42);gc=lines(t)
 return rc0==rc1==rc2==(0,0) and want==got==gc and want>0,('pretransformed',want,'T&L',got,'T&L clipped',gc)
def main():
 bad=0
 for name,f in [('06',c06),('07',c07),('08',c08),('09',c09),('13',c13),('11',c11),('12',c12),('14',c14),('15',c15),('16',c16),('17',c17)]:
  ok,info=f();print('audit %s: %s %s'%(name,'PASS' if ok else 'FAIL',info));bad+=not ok
 return 1 if bad else 0
sys.exit(main())
