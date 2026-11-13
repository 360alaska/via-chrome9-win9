"""v10.144: the kept engine context (ACCEL_KEEP) against the per-operation open/save/restore/close, on the emulated engine.
Random fills, copies (clip, source key, destination key, fixed-pattern ROPs), mono patterns and lines at 8, 16 and 32 bpp on two engines that start from the
same 'foreign' register values: one with ACCEL_KEEP 0, one with 1. Every picture must match byte for byte after every operation; the kept engine opens once;
VX2D_sync, the live switch going off and VX2D_busy hand the context back (the 19 registers equal the foreign values again, the sequencer is locked again);
a mode generation change drops the old context without writing its registers back."""
import random,struct
from engine import GPU,MM,FB
SAVED=[4,8,12,16,20,24,28,32,36,40,44,48,64,68,72,76,80,88,92]
def pack(v):return struct.pack('<%dI'%len(v),*v)
def regs(g):return [struct.unpack('<I',g.u.mem_read(MM+r,4))[0] for r in SAVED]
def word(g,n):return struct.unpack('<I',g.u.mem_read(g.addr(n),4))[0]
def kq(g):
 out=0x1f8000;g.call('VX2D_control',(0xFFFFFE3F,out),strict=False);return struct.unpack('<4I',g.u.mem_read(out,16)) # opens, releases, open, switch
def foreign(g,seed):
 vals=[(seed*2654435761+i*40503)&0xFFFFFFFF for i in range(len(SAVED))]
 for r,v in zip(SAVED,vals):g.u.mem_write(MM+r,pack([v]))
 return vals
def random_request(g,rnd):
 bpp=g.bpp;mask=(1<<bpp)-1;op=rnd.choice([1,1,2,2,3,5]);w=rnd.randint(1,20);h=rnd.randint(1,14)
 dx=rnd.randint(0,63-w);dy=rnd.randint(0,47-h);kw=dict(dx=dx,dy=dy,w=w,h=h,fg=rnd.getrandbits(32)&mask,bg=rnd.getrandbits(32)&mask)
 flags=0
 if rnd.random()<0.3:
  l=rnd.randint(0,30);t=rnd.randint(0,20);flags|=1;kw.update(l=l,t=t,r=rnd.randint(l+1,64),b=rnd.randint(t+1,48))
 if op==1:kw['rop']=rnd.choice([0xF0,0x5A,0x00,0xFF,0x55])
 elif op==2:
  kw.update(sx=rnd.randint(0,63-w),sy=rnd.randint(0,47-h))
  k=rnd.random()
  if k<0.15:flags|=2;kw.update(rop=0xCC,key=rnd.getrandbits(32)&mask)
  elif k<0.3:flags|=4;kw.update(rop=0xCC,key=rnd.getrandbits(32)&mask)
  elif k<0.45:flags|=32;kw['rop']=rnd.choice([0xB8,0xE2,0x96])
  else:kw['rop']=rnd.choice([0xCC,0x66,0x88,0xEE,0xBB])
 elif op==3:kw['rop']=rnd.choice([0xF0,0x5A])
 else:
  x2=rnd.randint(0,63);y2=rnd.randint(0,47)
  if (x2,y2)==(dx,dy):x2=(x2+5)%64
  n=max(abs(x2-dx),abs(y2-dy));kw.update(sx=x2,sy=y2,w=n,h=1,rop=0xF0)
 q=g.request(op,flags=flags,**kw)
 if op==3:q[27]=rnd.getrandbits(32);q[28]=rnd.getrandbits(32)
 return q
def keep_checks():
 cases=[]
 for bpp in (8,16,32):
  a=GPU(bpp);b=GPU(bpp);b.u.mem_write(b.addr('vx2d_keep'),pack([1]))
  for g in (a,b):g.prefill(w=64,h=48)
  base=foreign(a,bpp);foreign(b,bpp)
  rnd_a=random.Random(1000+bpp);rnd_b=random.Random(1000+bpp);n=0
  for i in range(160):
   qa=random_request(a,rnd_a);qb=random_request(b,rnd_b);assert qa==qb
   ra=a.submit(qa);rb=b.submit(qb,strict=False)
   assert ra==rb,(bpp,i,ra,rb)
   assert a.snapshot()==b.snapshot(),(bpp,i,'picture differs with the kept context',qa[:4])
   assert regs(a)==base,(bpp,i,'per-operation restore')
   n+=ra==1
  assert kq(b)[0]==1 and b.seq[0x10]&1,(bpp,'one open, sequencer left unlocked between calls')
  cases.append(f'{bpp}bpp: 160 random operations ({n} drawn by the engine: fills, copies with clip / keys / fixed-pattern ROPs, mono patterns, lines) identical byte for byte with ACCEL_KEEP 0 and 1; the kept engine opened once')
  b.call('VX2D_sync',(),strict=False)
  assert regs(b)==base and b.seq[0x10]==0xa0 and kq(b)[1]==1,(bpp,'sync hands back')
  cases.append(f'{bpp}bpp: VX2D_sync hands the context back: the 19 registers equal the foreign values again and the sequencer is locked again')
  assert a.submit(a.request(1,dx=1,dy=1,w=3,h=3))==1 and b.submit(b.request(1,dx=1,dy=1,w=3,h=3),strict=False)==1 and b.seq[0x10]&1 and kq(b)[0]==2
  out=0x1f8000;b.call('VX2D_control',(0xEB000000,out),strict=False)
  assert regs(b)==base and b.seq[0x10]==0xa0 and word(b,'vx2d_keep')==0,(bpp,'live switch off hands back')
  assert a.submit(a.request(1,dx=2,dy=2,w=3,h=3))==1 and b.submit(b.request(1,dx=2,dy=2,w=3,h=3))==1 and regs(b)==base # strict: switched off, every call relocks as before
  cases.append(f'{bpp}bpp: the live switch going off (0xEB000000) hands the context back; later operations open and close per call again')
  b.call('VX2D_control',(0xEB000001,out),strict=False)
  assert a.submit(a.request(1,dx=3,dy=3,w=3,h=3))==1 and b.submit(b.request(1,dx=3,dy=3,w=3,h=3),strict=False)==1 and b.seq[0x10]&1
  assert b.call('VX2D_busy',(),strict=False)==0 and regs(b)==base and b.seq[0x10]==0xa0,(bpp,'busy (3D) hands back')
  cases.append(f'{bpp}bpp: VX2D_busy (asked by the 3D side) hands the context back and reports idle')
  # a new mode generation while the context is open: dropped without writing its registers back
  assert a.submit(a.request(1,dx=4,dy=4,w=3,h=3))==1 and b.submit(b.request(1,dx=4,dy=4,w=3,h=3),strict=False)==1 and b.seq[0x10]&1
  for g in (a,b):
   s=struct.unpack('<31I',g.u.mem_read(g.addr('vx_status'),124));s=list(s);s[24]+=1;g.u.mem_write(g.addr('vx_status'),pack(s))
  stale=regs(b);a.writes=[];b.writes=[];rel=kq(b)[1]
  q=b.request(1,dx=5,dy=5,w=6,h=4);qa=a.request(1,dx=5,dy=5,w=6,h=4)
  assert a.submit(qa)==1 and b.submit(q,strict=False)==1
  assert a.snapshot()==b.snapshot(),(bpp,'picture after a mode generation change')
  wa=len([1 for r,v in a.writes if r in SAVED]);wb=len([1 for r,v in b.writes if r in SAVED])
  assert kq(b)[1]==rel+1 and wb==wa-19,(bpp,'old context dropped without write-back',wa,wb)
  b.call('VX2D_sync',(),strict=False);assert b.seq[0x10]==0xa0 and regs(b)==stale,(bpp,'the new context hands back what it found')
  cases.append(f'{bpp}bpp: a mode generation change with the context still open (no sync first) drops it without writing the old values back; the next operation draws the same picture; the new context hands back what it found')
  # the normal order: the mode set's VX2D_sync first, then the new generation
  for g in (a,b):foreign(g,bpp+7)
  base2=regs(a)
  assert a.submit(a.request(1,dx=6,dy=6,w=3,h=3))==1 and b.submit(b.request(1,dx=6,dy=6,w=3,h=3),strict=False)==1
  b.call('VX2D_sync',(),strict=False)
  for g in (a,b):
   s2=list(struct.unpack('<31I',g.u.mem_read(g.addr('vx_status'),124)));s2[24]+=1;g.u.mem_write(g.addr('vx_status'),pack(s2))
  assert regs(b)==base2 and a.submit(a.request(1,dx=7,dy=7,w=5,h=5))==1 and b.submit(b.request(1,dx=7,dy=7,w=5,h=5),strict=False)==1 and a.snapshot()==b.snapshot()
  b.call('VX2D_sync',(),strict=False);assert regs(b)==regs(a)==base2 and b.seq[0x10]==0xa0
  cases.append(f'{bpp}bpp: sync, mode generation change, draw, sync: pictures identical, the registers back to the foreign values, sequencer locked')
 return cases
