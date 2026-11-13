import sys,struct,random,os
so=sys.argv[1];N=int(sys.argv[2])
""".543: the driver's T&L fuzzed under ASan/UBSan: random vertex formats, vertices (NaN, infinities, huge and tiny values among them), matrices, materials, lights, fog, stage coordinate flags and draw commands; every packet sent must pass the VxD's checks (vx3dn_validate) and no sanitizer report may appear"""
TC=os.path.join(os.path.dirname(os.path.abspath(__file__)),'test_core.py') # .544: the tree's own (a workspace path was hard-coded)
src=open(TC).read().replace("lib=C.CDLL(str(ROOT/'tests/core.so'))","lib=C.CDLL(%r)"%so).replace("if __name__=='__main__'","if False")
mod={'__name__':'tc','__file__':TC};exec(compile(src,'tc','exec'),mod)
C=mod['C'];U=mod['U'];cmd=mod['cmd'];rs=mod['rs'];ALLF=mod['ALLF'];lib=mod['lib']
t=mod['Contract']('test_tnl');t.setUp()
U.in_dll(lib,'vx_d3d_feat').value=ALLF;U.in_dll(lib,'vx_d3d_prepack').value=1;U.in_dll(lib,'vx_d3d_ixpass').value=1;U.in_dll(lib,'vx_d3d_indexed').value=1;U.in_dll(lib,'vx_d3d_tnl').value=1;U.in_dll(lib,'vx_d3d_texruns').value=1
R=random.Random(int(sys.argv[3]) if len(sys.argv)>3 else 1)
def F():
 r=R.random()
 if r<0.01:return float('nan')
 if r<0.02:return float('inf')*R.choice((1,-1))
 if r<0.05:return R.choice((0.0,-0.0,1e30,-1e30,1e-30,1e-40))
 return R.uniform(-3,3)
f2u=lambda f:struct.unpack('<I',struct.pack('<f',f))[0]
bad=0
for it in range(N):
 fvf=R.choice((0x002,0x012,0x042,0x052,0x142,0x152,0x112,0x252,0x202,0x0D2,0x10142))
 n=(fvf>>8)&15;sz=12+(12 if fvf&0x10 else 0)+(4 if fvf&0x40 else 0)+(4 if fvf&0x80 else 0)
 tsz=[ (2,3,4,1)[(fvf>>(16+2*k))&3]*4 for k in range(n)];sz+=sum(tsz)
 nv=R.randint(3,200);vb=b''
 for i in range(nv):
  vb+=struct.pack('<3f',F(),F(),F()*2+3)
  if fvf&0x10:vb+=struct.pack('<3f',F(),F(),F())
  if fvf&0x40:vb+=struct.pack('<I',R.getrandbits(32))
  if fvf&0x80:vb+=struct.pack('<I',R.getrandbits(32))
  for s in tsz:vb+=struct.pack('<%df'%(s//4),*[F() for _ in range(s//4)])
 m16=lambda:struct.pack('<16f',*[F() if R.random()<0.3 else (1.0 if k%5==0 else 0.0) for k in range(16)])
 P=struct.pack('<16f',1.5,0,0,0,0,2,0,0,0,0,1.005,1,0,0,-1.0,0) if R.random()<0.8 else m16()
 c=cmd(28,1,struct.pack('<4I',0,0,64,64))+cmd(36,3,struct.pack('<I',1)+m16()+struct.pack('<I',2)+m16()+struct.pack('<I',3)+P)
 if R.random()<0.3:c+=cmd(36,1,struct.pack('<I',16+R.randint(0,1))+m16())
 c+=cmd(33,1,struct.pack('<17f',*[F() for _ in range(17)]))
 for li in range(R.randint(0,3)):
  c+=cmd(34,2,struct.pack('<II',li,2)+struct.pack('<I',R.choice((1,2,3)))+struct.pack('<25f',*[F() for _ in range(25)])+struct.pack('<II',li,0))
 st=[(137,R.randint(0,1)),(139,R.getrandbits(32)),(141,R.randint(0,1)),(145,R.randint(0,2)),(147,R.randint(0,2)),(148,R.randint(0,2)),(143,R.randint(0,1)),
     (28,R.randint(0,1)),(35,0),(140,R.randint(0,3)),(36,f2u(F())),(37,f2u(F())),(38,f2u(F())),(48,R.randint(0,1)),(142,R.randint(0,1)),(22,R.randint(1,3))]
 c+=rs(*st)
 for k in range(3):
  tt=t.context.state.tss[k];tt[1]=R.choice((1,2,3,4));tt[11]=R.choice((0,1,0x10000,0x20000,0x30000,0x20001));tt[24]=R.choice((0,1,2,3,4,0x102,0x103,0x104,0x1FF,5));tt[0]=R.choice((0,7))
 nd=R.randint(1,4)
 for d in range(nd):
  op=R.choice((26,26,18,19,21,20,22,3))
  if op==26:
   tris=[(R.randrange(nv),R.randrange(nv),R.randrange(nv)) for _ in range(R.randint(1,300))];c+=t._itl(tris)
  elif op==3:
   tris=[(R.randrange(nv),R.randrange(nv),R.randrange(nv)) for _ in range(R.randint(1,50))];c+=cmd(3,len(tris),b''.join(struct.pack('<4H',*x,0) for x in tris))
  elif op in (18,19,21):
   k=R.randint(1,max(1,nv//3-1));c+=cmd(op,k,struct.pack('<H',0))
  else:
   k=R.randint(1,20);c+=cmd(op,k,struct.pack('<H',0)+b''.join(struct.pack('<H',R.randrange(nv)) for _ in range(k+2)))
 t.calls.clear();rc=t.run_dp2(c,vb,stride=sz,fvf=fvf)
 for p in t.calls:
  if lib.vx3dn_validate(C.byref(p),C.byref(t.info))!=0:bad+=1
print('iterations',N,'packets refused by the VxD check',bad,'T&L counts',list((U*12).in_dll(lib,'vx_d3d_tn')))
if bad:sys.exit(1)
