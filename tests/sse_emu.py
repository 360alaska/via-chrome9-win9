#!/usr/bin/env python3
"""v10.359: runs the SSE transform and clip kernels of the built VXGL.DLL (vxgl_sse.c, Watcom code as shipped) in an x86 emulator and checks
them against a single-precision reference with Mesa's operation order (bit for bit), against Mesa's own C functions from the same DLL
(the clip mask identical; values to a few units in the last place: the C code rounds at the x87's precision), with a program's MXCSR
(flush-to-zero, round toward zero) left as it was, for strides 0/12/16/32 and counts 0..n, and that vxgl_sse_install puts them in Mesa's tables."""
import struct,re,sys,random,pathlib
import numpy as np
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import *
ROOT=pathlib.Path(__file__).resolve().parent.parent
f32=np.float32
def u16(b,o):return struct.unpack_from('<H',b,o)[0]
def u32(b,o):return struct.unpack_from('<I',b,o)[0]
class DLL:
 def __init__(s):
  b=(ROOT/'driver'/'VXGL.DLL').read_bytes();h=u32(b,60);opt=h+24;s.base=u32(b,opt+28);u=s.u=Uc(UC_ARCH_X86,UC_MODE_32)
  u.mem_map(s.base,(u32(b,opt+56)+4095)&~4095);u.mem_write(s.base,b[:u32(b,opt+60)])
  for i in range(u16(b,h+6)):
   a=opt+u16(b,h+20)+i*40;v=u32(b,a+12);n=u32(b,a+16);off=u32(b,a+20);u.mem_write(s.base+v,b[off:off+n])
  s.sym={}
  for line in (ROOT/'build'/'VXGL.map').read_text().splitlines():
   m=re.match(r'([0-9a-fA-F]{8})[ +*]+(\S+)',line)
   if m:s.sym[m[2]]=int(m[1],16)
  u.mem_map(0,0x500000) # stack, return stub and data
  u.reg_write(UC_X86_REG_CR0,0x80000033&~0x80000000|0x22);u.reg_write(UC_X86_REG_CR4,0x600) # MP, NE; OSFXSR, OSXMMEXCPT
  u.reg_write(UC_X86_REG_FPCW,0x027F) # the x87 as a Windows thread has it (53-bit precision)
 def call(s,addr,args):
  sp=0x1F0000;s.u.mem_write(sp,struct.pack('<%dI'%(len(args)+1),0xF0000,*args));s.u.reg_write(UC_X86_REG_ESP,sp)
  s.u.mem_write(0xF0000,b'\xc3');s.u.emu_start(addr,0xF0000,count=5000000)
  assert s.u.reg_read(UC_X86_REG_EIP)==0xF0000 and s.u.reg_read(UC_X86_REG_ESP)==sp+4
  return s.u.reg_read(UC_X86_REG_EAX)
 def word(s,a):return u32(s.u.mem_read(a,4),0)
 def put(s,a,data):s.u.mem_write(a,data)
 def floats(s,a,n):return np.frombuffer(bytes(s.u.mem_read(a,4*n)),dtype='<f4').copy()
 def mxcsr(s):return s.u.reg_read(UC_X86_REG_MXCSR)
d=DLL()
FROM,TO,MAT,VIN,VOUT,MASK,ORM,ANDM=0x200000,0x280000,0x300000,0x301000,0x301100,0x302000,0x303000,0x303004
fails=0;checks=0
def bad(msg):
 global fails;fails+=1
 if fails<=20:print('FAIL',msg)
def vec(addr,start,count,stride,size,flags=0):d.put(addr,struct.pack('<7I',start,start,count,stride,size,flags,0))
def tab(name,i,j=None):return d.word(d.sym[name]+4*i) if j is None else d.word(d.word(d.sym[name]+4*i)+4*j)
def same(a,b):return (np.isnan(a)&np.isnan(b))|(a.view('<u4')==b.view('<u4'))
def ulps(a,b):
 ia=a.view('<i4').astype(np.int64);ib=b.view('<i4').astype(np.int64);ia=np.where(ia<0,-(ia&0x7FFFFFFF),ia);ib=np.where(ib<0,-(ib&0x7FFFFFFF),ib)
 m=np.isfinite(a)&np.isfinite(b);return int(np.max(np.abs(ia-ib)[m])) if m.any() else 0
rnd=random.Random(359)
SPECIAL=[0.0,-0.0,1.0,-1.0,float('inf'),float('-inf'),float('nan'),1e-40,-1e-40,3e38,-3e38,1e-30]
def inputs(n,size,special):
 a=np.array([[rnd.uniform(-500,500) for _ in range(4)] for _ in range(n)],dtype=f32)
 if size==4:a[:,3]=np.array([rnd.choice([1.0,rnd.uniform(-2,2),rnd.uniform(0.01,50)]) for _ in range(n)],dtype=f32)
 if special:
  for i in range(n):
   if rnd.random()<0.3:a[i,rnd.randrange(size)]=f32(rnd.choice(SPECIAL))
 return a
def matrix(kind):
 if kind=='3d':m=np.array([rnd.uniform(-2,2) for _ in range(16)],dtype=f32);m[3]=m[7]=m[11]=0;m[15]=1;return m
 m=np.array([rnd.uniform(-3,3) for _ in range(16)],dtype=f32)
 if kind=='persp':m[3]=m[7]=0;m[11]=-1;m[15]=0
 return m
def ref_xf(a,m,size,kind):
 out=np.zeros((len(a),4),dtype=f32)
 for i,v in enumerate(a):
  for r in range(4):
   t=f32(m[r]*v[0])+f32(m[4+r]*v[1])
   if size>=3:t=f32(t+f32(m[8+r]*v[2]))
   t=f32(t+(f32(m[12+r]*v[3]) if size==4 else m[12+r]))
   out[i,r]=t
 return out
def lay(a,size,stride):
 buf=bytearray(max(stride,4*size)*len(a)+64)
 for i,v in enumerate(a):struct.pack_into('<%df'%size,buf,i*stride if stride else 0,*[float(x) for x in v[:size]])
 return bytes(buf)
with np.errstate(all='ignore'):
 # Mesa's own C tables first (the DLL's one-time set-up), then the SSE kernels installed over them
 d.call(d.sym['_math_init_transformation'],[])
 cfun={(sz,t):tab('_mesa_transform_tab',sz,t) for sz in (2,3,4) for t in (0,6)};cclip=tab('_mesa_clip_tab',4);cclipnp=tab('_mesa_clip_np_tab',4)
 d.put(d.sym['vxgl_sse_active'],b'\0'*4);d.call(d.sym['vxgl_sse_install'],[])
 want={(2,0):'vxgl_sse_points2_general',(3,0):'vxgl_sse_points3_general',(4,0):'vxgl_sse_points4_general',(3,6):'vxgl_sse_points3_3d'}
 for k,n in want.items():
  checks+=1
  if tab('_mesa_transform_tab',k[0],k[1])!=d.sym[n]:bad('table %s'%(k,))
 checks+=2
 if tab('_mesa_clip_tab',4)!=d.sym['vxgl_sse_cliptest4']:bad('clip table')
 if tab('_mesa_clip_np_tab',4)!=d.sym['vxgl_sse_cliptest_np4']:bad('clip np table')
 if tab('_mesa_transform_tab',4,1)==d.sym['vxgl_sse_points4_general']:bad('identity entry replaced')
 worst={'xf':0,'proj':0}
 for trial in range(480): # .506: ACCEL_OPENGL_CLIPINT removed (movmskps only)
  size,kind=[(2,'gen'),(3,'gen'),(4,'gen'),(3,'3d'),(4,'persp'),(3,'persp')][trial%6]
  n=rnd.choice([0,1,2,3,7,31,64]);stride=rnd.choice([0,4*size,16,32]) if n else 16;special=trial%3==0
  a=inputs(max(n,1),size,special)[:n] if n else np.zeros((0,4),dtype=f32)
  if stride==0 and n:a[:]=a[0]
  m=matrix('3d' if kind=='3d' else kind);t=6 if kind=='3d' else 0
  d.put(MAT,m.tobytes());d.put(FROM,lay(a,size,stride));sentinel=struct.pack('<f',-7.25)*(4*max(n,1)+8)
  for which in ('sse','c'):
   d.put(TO,sentinel);vec(VIN,FROM,n,stride,size);vec(VOUT,TO,0,16,0)
   if which=='sse':d.u.reg_write(UC_X86_REG_MXCSR,0xFFC0 if trial&1 else 0x1F80);mx0=d.mxcsr()
   fn=d.sym[want[(size,t)]] if which=='sse' else cfun[(size,t)]
   d.call(fn,[VOUT,MAT,VIN]);got=d.floats(TO,4*n).reshape(n,4) if n else np.zeros((0,4),dtype=f32)
   osize,ocount=d.word(VOUT+16),d.word(VOUT+8);checks+=1
   if (osize,ocount)!=((3 if t==6 else 4),n):bad('%s size/count %d %d'%(which,osize,ocount))
   if which=='sse':
    checks+=1
    if d.mxcsr()!=mx0:bad('MXCSR %04X -> %04X'%(mx0,d.mxcsr()))
    r=ref_xf(a,m,size,kind);cols=3 if t==6 else 4;checks+=1
    if n and not same(got[:,:cols],r[:,:cols]).all():bad('transform %d %s stride %d n %d: %s vs %s'%(size,kind,stride,n,got[:2],r[:2]))
    if t==6 and n and not (got[:,3].view('<u4')==np.float32(-7.25).view('<u4')).all():bad('3d kernel wrote w')
    sse=got
   else:
    cols=3 if t==6 else 4
    if n:
     checks+=1
     if not (np.isnan(sse[:,:cols])==np.isnan(got[:,:cols])).all() and not special:bad('nan pattern vs C')
     if not special:
      for i in range(n):
       for r_ in range(cols):
        terms=[abs(float(m[r_])*float(a[i,0])),abs(float(m[4+r_])*float(a[i,1])),abs(float(m[8+r_])*float(a[i,2])) if size>=3 else 0.0,abs(float(m[12+r_])*(float(a[i,3]) if size==4 else 1.0))]
        big=sum(terms);u=float(np.spacing(f32(big))) if big else 1.0
        worst['xf']=max(worst['xf'],abs(float(sse[i,r_])-float(got[i,r_]))/u)
  # clip tests on transformed points (and on raw ones with specials)
  if size==4 or kind!='3d':
   c=ref_xf(a,m,size,kind) if n else np.zeros((0,4),dtype=f32)
   if special and n:
    for i in range(n):
     if rnd.random()<0.3:c[i,rnd.randrange(4)]=f32(rnd.choice(SPECIAL))
   cs=rnd.choice([16,32]);d.put(FROM,lay(c,4,cs))
   for np_ in (0,1):
    res={};om0,am0=rnd.choice([0,0x40]),rnd.choice([0x3F,0x7F])
    for which in ('sse','c'):
     d.put(TO,sentinel);d.put(MASK,b'\xEE'*(n+8));vec(VIN,FROM,n,cs,4);vec(VOUT,TO,0,16,0)
     d.put(ORM,bytes([om0]));d.put(ANDM,bytes([am0]))
     if which=='sse':d.u.reg_write(UC_X86_REG_MXCSR,0xDFC0 if trial&2 else 0x1F80);mx0=d.mxcsr()
     fn=(d.sym['vxgl_sse_cliptest_np4'] if np_ else d.sym['vxgl_sse_cliptest4']) if which=='sse' else (cclipnp if np_ else cclip)
     ret=d.call(fn,[VIN,VOUT,MASK,ORM,ANDM])
     res[which]=(ret,bytes(d.u.mem_read(MASK,n+1)),d.u.mem_read(ORM,1)[0],d.u.mem_read(ANDM,1)[0],d.floats(TO,4*n).reshape(n,4) if n else None,d.word(VOUT+16),d.word(VOUT+8))
     if which=='sse':
      checks+=1
      if d.mxcsr()!=mx0:bad('clip MXCSR')
    s_,c_=res['sse'],res['c'];checks+=4
    if s_[0]!=c_[0]:bad('return vector')
    if s_[1]!=c_[1]:bad('clip masks differ np=%d: %s vs %s'%(np_,s_[1].hex(),c_[1].hex()))
    if s_[2:4]!=c_[2:4]:bad('or/and masks %s vs %s'%(s_[2:4],c_[2:4]))
    if s_[5:]!=c_[5:]:bad('proj size/count')
    if not np_ and n:
     # bit for bit against the single-precision reference: 1/w by a divide, then each coordinate times it
     r=np.zeros((n,4),dtype=f32)
     for i in range(n):
      if s_[1][i]:r[i]=(0,0,0,1)
      else:oow=f32(f32(1)/c[i,3]);r[i]=(f32(c[i,0]*oow),f32(c[i,1]*oow),f32(c[i,2]*oow),oow)
     checks+=1
     if not same(s_[4],r).all():bad('projection vs reference: %s %s'%(s_[4][:2],r[:2]))
     if not special:worst['proj']=max(worst['proj'],ulps(s_[4],c_[4]))
# v10.490: the pass's new kernels (vxgl_ssepass_ktest, the DLL's own code): snapw's grid four lanes at a time against the DLL's snapw
# (vx_d3d_snapw), both ways; UNCLAMPED_FLOAT_TO_UBYTE four lanes at a time against Mesa's macro, bit for bit
rng=random.Random(490)
vals=[]
for i in range(0,4000,3):
 for sg in (0,0x80000000):
  for f in (i/16.0,(i+0.5)/2048.0):
   b_=struct.unpack('<I',struct.pack('<f',f))[0]|sg;vals+=[b_,b_+1]+([b_-1] if b_&0x7FFFFFFF else [])
vals+=[struct.unpack('<I',struct.pack('<f',rng.uniform(-2100,2100)))[0] for _ in range(3000)]
vals+=[0,0x80000000,0x7F800000,0xFF800000,0x7FC00000,1,0x44FF0000,0x44FF0001,0x44FEFFFF,0xC4FF0000,0x49800000]
for up in (0,1):
 d.put(FROM,struct.pack('<%dI'%len(vals),*vals));d.call(d.sym['vxgl_ssepass_ktest'],[up,FROM,TO,len(vals),0])
 got=struct.unpack('<%dI'%len(vals),bytes(d.u.mem_read(TO,4*len(vals))));nb=0
 for v,g_ in zip(vals,got):
  w_=d.call(d.sym['vx_d3d_snapw'],[v,up]);checks+=1
  if w_!=g_:
   nb+=1
   if nb<4:bad('snap kernel %d: %08X gives %08X, snapw %08X'%(up,v,g_,w_))
vals=[struct.unpack('<I',struct.pack('<f',rng.uniform(-0.25,1.25)))[0] for _ in range(6000)]+[0x3F7F0000+k-200 for k in range(400)]+[0x3F800000+k-50 for k in range(100)]
vals+=[0,0x80000000,0x7F800000,0xFF800000,0x7FC00000,0xFFC00000,1,0x80000001]
d.put(FROM,struct.pack('<%dI'%len(vals),*vals));d.call(d.sym['vxgl_ssepass_ktest'],[2,FROM,TO,len(vals),0])
got=struct.unpack('<%dI'%len(vals),bytes(d.u.mem_read(TO,4*len(vals))));nb=0
for v,g_ in zip(vals,got):
 if v&0x80000000:r_=0
 elif v>=0x3F7F0000:r_=255
 else:r_=struct.unpack('<I',struct.pack('<f',f32(f32(struct.unpack('<f',struct.pack('<I',v))[0])*f32(255.0/256.0))+f32(32768.0)))[0]&255
 checks+=1
 if r_!=g_:
  nb+=1
  if nb<4:bad('byte kernel: %08X gives %d, Mesa %d'%(v,g_,r_))
tab=np.array([(i/256.0)**7.3 for i in range(257)],dtype=f32);TAB=0x380000;d.put(TAB,tab.tobytes())
vals=[struct.unpack('<I',struct.pack('<f',rng.uniform(0,1.02)))[0] for _ in range(5000)]+[0x3F800000,0x3F7FFFFF,0x3B800000,1,0x437F0000//256]
d.put(FROM,struct.pack('<%dI'%len(vals),*vals));d.call(d.sym['vxgl_ssepass_ktest'],[3,FROM,TO,len(vals),TAB])
got=struct.unpack('<%dI'%len(vals),bytes(d.u.mem_read(TO,4*len(vals))));nb=0
for v,g_ in zip(vals,got):
 x=f32(struct.unpack('<f',struct.pack('<I',v))[0]);f=f32(x*f32(255));checks+=1
 if f>=f32(255):r_=0xFFFFFFFF
 else:k_=int(np.floor(f));r_=struct.unpack('<I',struct.pack('<f',f32(tab[k_]+f32(f32(f-f32(k_))*f32(tab[k_+1]-tab[k_])))))[0]
 if r_!=g_:
  nb+=1
  if nb<4:bad('specular kernel: %08X gives %08X, reference %08X'%(v,g_,r_))
# v10.495: the fused kernels (ACCEL_OPENGL_SSEFUSE) against the kernels each replaces, bit for bit, on the DLL's own code (ktest 4): ordinary
# values, sizes over the whole range, and NaNs, infinities, zeros and denormals mixed in
def fw(x):return struct.unpack('<I',struct.pack('<f',x))[0]
specials=[0,0x80000000,0x7F800000,0xFF800000,0x7FC00000,0xFFC00000,0x7F800001,1,0x80000001,0x007FFFFF,0x3F800000,0xBF800000]
for kind in range(4):
 if kind==0:vals=[fw(rng.uniform(-2,2)) for _ in range(16000)]
 elif kind==1:vals=[fw(rng.choice((-1,1))*10**rng.uniform(-30,30)) for _ in range(16000)]
 elif kind==2:vals=[rng.choice(specials) if rng.random()<0.08 else fw(rng.uniform(-3,3)) for _ in range(16000)]
 else:vals=[rng.getrandbits(32) for _ in range(16000)]
 d.put(FROM,struct.pack('<%dI'%len(vals),*vals));d.call(d.sym['vxgl_ssepass_ktest'],[4,FROM,TO,len(vals),0])
 got=struct.unpack('<14I',bytes(d.u.mem_read(TO,56)));checks+=13*got[13]
 if got[13]!=len(vals)//64:bad('fused kernels: %d trials'%got[13])
 for name,e_ in zip(('vk_front','vk_front_in','vk_xfn_norm','vk_lpos','vk_ldiff','vk_lacc','vk_laccs','vk_lhalf_sub','vk_lhalf_add','vk_xfbits','vk_snap2d','vk_snap2u','vk_box3'),got[:13]):
  if e_:bad('fused kernel %s (values %d): %d of %d trials differ from the kernels it replaces'%(name,kind,e_,got[9]))
# v10.500: the store's grid words (ACCEL_OPENGL_STORESNAP, vxgl_ssepass_snapstore_to on the DLL's own code) against the DLL's snapw for every
# vertex's x and y, both ways (y up with the window's flip), a 24-byte store, counts not a multiple of four
xs_=[fw(rng.uniform(-2100,2100)) for _ in range(1500)]+[fw(i/16.0+0.03125*(i&1)) for i in range(0,600)]+specials+[rng.getrandbits(32) for _ in range(400)]
ys_=[fw(rng.uniform(-300,2100)) for _ in range(len(xs_)-len(specials))]+specials[::-1]
ys_=ys_[:len(xs_)]
for up in (0,1):
 for n in (len(xs_),len(xs_)-3):
  st=b''.join(struct.pack('<6I',xs_[i],ys_[i],0x3F000000,0x3F800000,0xFF808080,0) for i in range(n))
  d.put(FROM,st);d.put(TO,b'\xAA'*(8*(n+4)));d.call(d.sym['vxgl_ssepass_snapstore_to'],[TO,FROM,24,n,up])
  got=struct.unpack('<%dI'%(2*n),bytes(d.u.mem_read(TO,8*n)));nb=0
  for i in range(n):
   wx=d.call(d.sym['vx_d3d_snapw'],[xs_[i],0]);wy=d.call(d.sym['vx_d3d_snapw'],[ys_[i],up]);checks+=2
   if got[2*i]!=wx or got[2*i+1]!=wy:
    nb+=1
    if nb<4:bad('store grid words %d: vertex %d (%08X %08X) gives %08X %08X, snapw %08X %08X'%(up,i,xs_[i],ys_[i],got[2*i],got[2*i+1],wx,wy))
# the VxD's control 0xFFFFFE34 (VXGL.DLL's check that the system saves the XMM registers): CR4 as the processor has it, 'SSE1', CPUID 1
sys.path.insert(0,str(ROOT/'verification'))
from engine import GPU,ARENA
for cr4 in (0x6F0,0x0F0):
 g=GPU(32);g.u.reg_write(UC_X86_REG_CR4,cr4);out=ARENA+0x3000;g.u.mem_write(out,b'\xAA'*16);g.call('VX2D_control',(0xFFFFFE34,out))
 o=struct.unpack('<4I',bytes(g.u.mem_read(out,16)));checks+=1
 if o[1]!=0x31455353 or (o[0]&0x200)!=(cr4&0x200) or o[2]==0xAAAAAAAA or o[3]==0xAAAAAAAA:bad('VxD control 0xFFFFFE34: %s'%[hex(x) for x in o])
print('SSE kernels: %d checks, %d failures; largest difference from Mesa\'s C (x87) code: transform %.2f units in the last place of the sum of the products\' sizes, divide %d units in the last place'%(checks,fails,worst['xf'],worst['proj']))
if worst['xf']>3 or worst['proj']>2:print('FAIL difference from the C code too large');fails+=1
sys.exit(1 if fails else 0)
