#!/usr/bin/env python3
"""Execute the linked VxD native submission path with synthetic MMIO/services.
Trace checks do not model rasterization or prove physical GPU behavior.
"""
import json,struct
from engine import GPU,BIND,GDI_SEL,ARENA,FB,MM,pack,BATCH
from validate import ROOT,u32
C_RUNS=30936 # .559: 25816 + 5120 (pre.w of 512 vertices) # .528: the packet's texture runs (runs.n), after the prepacked block (.529: 18136 + 7680, pre.w of 384 vertices)
from flip import ctl

def fb(f):return struct.unpack('<I',struct.pack('<f',f))[0]
def packet(bpp=32,op=1):
 pitch=64*(bpp//8)
 h=[1072,0x10000,0x33445856,op,1,0,pitch,64,48,2 if bpp==16 else 1]+[0]*5+[0,0,64,48]+[0,1,4,0,15]+[1,0xff334455,0xffffffff]+[3 if op==1 else 0] # v10.235: the V1 packet with 192 vertices
 assert len(h)==28
 v=[]
 for x,y,c in [(8,8,0xffff0000),(56,8,0xff00ff00),(8,40,0xff0000ff)]:v.extend([fb(x),fb(y),fb(.5),fb(1.),c])
 return bytearray(pack(h+v+[0]*(48*5-len(v)))) # v10.237: the historical V1 packet, 48 vertices (1072 bytes)

def full_packet(bpp,runs,n=None):
 """v10.236: the full packet (32380 bytes since .529: pre.w of 512 vertices since .559 (27260 with .529's 384); texture runs at its end since .528) with vertex runs [(topology, count)]: 0 list, 1 strip"""
 p=bytearray(packet(bpp));p+=bytes(32380-len(p));struct.pack_into("<I",p,0,32380)
 n=sum(c for t,c in runs) if n is None else n;struct.pack_into('<I',p,108,n)
 for k in range(min(n,192)):struct.pack_into("<4fI",p,112+20*k,4.+((k//2)%9)*6.,8.+(k%2)*20.+(k%3),.5,1.,0xff000000|(k*0x030507+0x10))
 struct.pack_into('<I',p,10060,len(runs)) # v10.321: bursts after the larger stages
 for k,(t,c) in enumerate(runs[:64]):struct.pack_into('<I',p,10064+4*k,(t<<16)|c)
 return p

class NativeGPU(GPU):
 def __init__(self,bpp=32):
  self.port_trace=[]
  super().__init__(bpp)
  self.u.mem_write(self.addr('vx_status')+11*4,pack([0x1000000]))
  self.u.mem_write(self.addr('vx3dn_shadow'),pack([0])) # .527: the texture set-up's shadow removed; .525: these checks compare the words with the register path's; the set-up's shadow has its own (shadow_ixchk_checks)
  self.u.mem_write(self.addr('vx3dn_clear_async'),pack([0])) # v10.282: these checks expect a clear to have finished when the submit returns (clear_async_checks has the default)
 def write(self,u,t,a,z,v,_):
  if a-MM in (0x43c,0x440):
   assert self.pend is None, '3D port write before pending 2D completion'
   if hasattr(self,'kept_foreign'):
    from keep import SAVED
    assert [u32(u.mem_read(MM+r,4),0) for r in SAVED]==self.kept_foreign, '3D ports before 2D context hand-back'
   self.port_trace.append((a-MM,v));return
  return super().write(u,t,a,z,v,_)
 def submit(self,p):
  self.u.mem_write(BATCH,bytes(p));return self.call('VX3DN_submit',(BATCH,))

def loss_checks():
 """Observe new cursor/overlay entry points after a native loss latch."""
 from v10106 import ov,mask
 rows=[]
 def fault(g):
  g.gone=True;assert g.submit(packet())==4;g.gone=False
 def record(name,observed,expected):
  rows.append({'name':name,'observed':observed,'expected':expected,'passed':observed==expected})
 for mode in (1,2):
  for operation in ('set','move','hide'):
   g=NativeGPU();off=0x10000;am=ARENA+0x20000;xm=ARENA+0x22000
   g.u.mem_write(g.addr('vx2d_hwcur_off'),pack([off]))
   g.call('VX2D_control',(0xF9000000|mode,ARENA+0xc000))
   g.u.mem_write(am,pack([0]*1024));g.u.mem_write(xm,pack([0x00ffffff]*1024))
   assert g.call('VX2D_hwcur_set',(am,xm,32,32,32,32))==1
   assert g.call('VX2D_hwcur_move',(4,4))==1
   fault(g);writes=len(g.writes);image=bytes(g.u.mem_read(FB+off,0x4000))
   if operation=='set':
    g.u.mem_write(xm,pack([0x00334455]*1024));rc=g.call('VX2D_hwcur_set',(am,xm,32,32,32,32))
   elif operation=='move':rc=g.call('VX2D_hwcur_move',(8,8))
   else:g.call('VX2D_hwcur_hide');rc=0
   record(f'native loss fences hardware cursor {mode} {operation}',
          [rc,len(g.writes)-writes,bytes(g.u.mem_read(FB+off,0x4000))!=image,g.critical],
          [0,0,False,0])
 for operation in ('update','flip','hide','probe'):
  g=NativeGPU();mask(g,1|64|2048)
  g.u.mem_write(g.addr('vx2d_hqv_off'),pack([0x700000]))
  args=dict(fmt=4,pitch=128,sw=32,sh=32,dst=(0,0,32,32))
  assert ov(g,1,**args)[0]==0
  fault(g);writes=len(g.writes);image=bytes(g.u.mem_read(FB,0x1000000))
  if operation=='update':r=ov(g,1,**args)
  elif operation=='flip':r=ov(g,2,offset=0x110000)
  elif operation=='hide':r=ov(g,3)
  else:r=ov(g,6,offset=0x230,key=0x12345678)
  record('native loss fences overlay/HQV '+operation,
         [r[0],len(g.writes)-writes,bytes(g.u.mem_read(FB,0x1000000))!=image,g.critical],
         [0 if operation=='hide' else 1,0,False,0])
 return rows

def rebase_checks():
 """v10.148: kept 2D ownership and software-cursor cleanup after loss.

 The cursor fixture runs the real linked FBHDA cursor entries. Only semaphore
 services are stubbed. The private act_mode setup is anchored at vesa_version
 (+8 bytes, per the unchanged vxd_vesa.c data layout); assert its reset value.
 """
 from keep import SAVED,foreign,regs,kq
 rows=[]
 def record(name,observed,expected):
  rows.append({'name':name,'observed':observed,'expected':expected,'passed':observed==expected})
 for bpp in (16,32):
  for operation in ('draw','clear','async draw','async clear','invalid packet'):
   g=NativeGPU(bpp);assert GPU.submit(g,g.request())==1
   base=foreign(g,300+bpp);g.u.mem_write(g.addr('vx2d_keep'),pack([1]))
   g.prefill();g.async_busy=4 if operation.startswith('async') else 0
   if g.async_busy:
    for code in (0xFC000001,0xFD000001):g.call('VX2D_control',(code,ARENA+0xc000))
   q=g.request(1,flags=16|(128 if g.async_busy else 0),fg=0x5a3c,dx=5,dy=4,w=40,h=30)
   expected=g.reference(q,g.snapshot());assert GPU.submit(g,q,strict=False)==1
   if g.async_busy:assert g.pend is not None
   else:assert kq(g)[2]==1 and g.seq[0x10]&1
   g.kept_foreign=base
   p=packet(bpp,2 if 'clear' in operation else 1)
   if operation=='invalid packet':struct.pack_into('<I',p,20,4)
   rc=g.submit(p)
   if 'clear' in operation:
    color=0xff334455 if bpp==32 else ((0x33>>3)<<11)|((0x44>>2)<<5)|(0x55>>3)
    cleared=color.to_bytes(bpp//8,'little')*(64*48)
    expected=cleared+expected[len(cleared):]
   record(f'{bpp}bpp kept 2D context hands back before native {operation}',
          [rc,regs(g)==base,kq(g)[2],g.pend is None,g.snapshot()==expected,
           bool(g.port_trace),g.excluded,g.critical,len(g.hazards)],
          [1 if operation=='invalid packet' else 0,True,0,True,True,
           'draw' in operation,0,0,0])
   if operation=='draw':
    next_q=g.request(1,fg=0x5317,dx=7,dy=6,w=13,h=11)
    expected_next=g.reference(next_q,g.snapshot())
    rc=GPU.submit(g,next_q,strict=False);g.call('VX2D_sync')
    record(f'{bpp}bpp kept 2D resumes after native draw and hands back cleanly',
           [rc,g.snapshot()==expected_next,regs(g)==base,kq(g)[2],g.excluded,g.critical],
           [1,True,True,0,0,0])
 class CursorGPU(NativeGPU):
  def hook(self,n,fn):
   if n not in ('FBHDA_access_begin','FBHDA_access_end','FBHDA_access_rect'):
    return super().hook(n,fn)
 def cursor(bpp):
  g=CursorGPU(bpp);g.hook('FBHDA_lock',lambda:1);g.hook('FBHDA_unlock',lambda:0);g.hook('FBHDA_release',lambda:0) # v10.319
  g.u.mem_write(g.addr('vx2d_hwcur'),pack([0]))
  anchor=g.addr('vesa_version');assert g.word(anchor+8)==0xffffffff
  g.u.mem_write(anchor+8,pack([0])) # act_mode, immediately after vesa_caps
  wram=ARENA+0x30000;g.u.mem_write(g.addr('wram'),pack([wram]))
  g.u.mem_write(wram+0x700+6*4,pack([1,0,0,0,2,2])) # visible, empty, x/y, w/h
  g.u.mem_write(wram+0x1000,pack([0]*1024))
  g.u.mem_write(wram+0x2000,pack([0x00ffffff]*1024))
  g.prefill();background=g.snapshot();g.call('VESA_HIRES_enable')
  g.call('FBHDA_access_end',(0,));assert g.snapshot()!=background
  return g,background
 for bpp in (16,32):
  g,background=cursor(bpp);g.call('FBHDA_access_begin',(0,))
  record(f'{bpp}bpp healthy software cursor restores its saved background',g.snapshot()==background,True)
  g,background=cursor(bpp)
  # A loss on an offscreen 2D command leaves the primary cursor drawn.
  q=g.request(1,dx=5,dy=4,w=8,h=8,fg=0x1234);q[5]=0x10000
  g.timeout=True;g.stuck=True;assert GPU.submit(g,q)==3 and g.call('VX2D_is_lost')==1
  g.call('FBHDA_access_begin',(0,))
  record(f'{bpp}bpp ordinary 2D loss still restores the software cursor background',g.snapshot()==background,True)
  g,background=cursor(bpp)
  p=packet(bpp);struct.pack_into('<I',p,20,0x10000)
  g.gone=True;assert g.submit(p)==4;g.gone=False
  before=g.snapshot();g.call('FBHDA_access_begin',(0,))
  record(f'{bpp}bpp native D3D loss fences software cursor restoration',g.snapshot()==before,True)
  g,background=cursor(bpp);assert g.submit(packet(bpp,2))==0
  g.call('FBHDA_access_begin',(0,))
  color=0xff334455 if bpp==32 else ((0x33>>3)<<11)|((0x44>>2)<<5)|(0x55>>3)
  record(f'{bpp}bpp healthy native primary clear excludes and restores the real software cursor',
         g.snapshot()==color.to_bytes(bpp//8,'little')*(64*48)+background[g.pitch*48:],True)
 return rows

class FetchGPU(NativeGPU):
 """v10.228: the command block set, so draws go through the fetcher; 0x41C/0x420 writes are traced too."""
 CMD=0x800000
 def __init__(self,bpp=32,cmd=True,ring=False,probe=False):
  super().__init__(bpp)
  if cmd:self.u.mem_write(self.addr('vx3dn_cmd_off'),pack([self.CMD]))
  self.u.mem_write(self.addr('vx3dn_cgroup'),pack([0])) # v10.287: nor the constant groups (cgroup_checks has them)
  self.fregs={};self.fpos=None;self.fences=[]
 def fetch_2d(self,a,b):
  # v10.383: the fetcher's 2D blocks between a and b: a 1x1 solid fill (the ring's fence) writes its colour, as the board's 2D engine
  i=a
  while i+16<=b:
   if self.word(FB+i)==0xFE000000:
    n=self.word(FB+i+4);r={self.word(FB+i+16+8*k):self.word(FB+i+20+8*k) for k in range(n)}
    if r.get(0x0C,1)==0 and 0x14 in r:
     at=FB+r[0x14]*8;c=r.get(0x58,0)
     self.u.mem_write(at,struct.pack('<H',c&0xFFFF) if r.get(0x04)==0x100 else struct.pack('<I',c));self.fences.append((r[0x14]*8,c))
    i+=16+8*n;i=(i+15)&~15;continue
   i+=4
 def write(self,u,t,a,z,v,_):
  if a-MM==0x420:
   top=v>>24;self.fregs[top]=v&0xFFFFFF
   if top==0x65 and self.fpos is not None:
    p=self.fregs.get(0x64,0)|((v&0xFF)<<24);self.fetch_2d(self.fpos,p);self.fpos=p
   if v==0x680A0001:
    st=self.fregs.get(0x60,0)|((self.fregs.get(0x61,0)&0xFF)<<24);p=self.fregs.get(0x64,0)|((self.fregs.get(0x65,0)&0xFF)<<24);self.fpos=st;self.fetch_2d(st,p);self.fpos=p
   if v==0x680A0000:self.fpos=None
  if a-MM in (0x41c,0x420):self.port_trace.append((a-MM,v));return
  return super().write(u,t,a,z,v,_)
def blocks(trace):
 out=[];cur=None
 for a,v in trace:
  if a==0x43c:cur=[v];out.append(cur)
  else:cur.append(v)
 return out
def decode(words):
 """The fetched stream as (header, words) blocks; XP's batch end (FE020420 0000041C 00300000) is its own block."""
 out=[];i=0
 while i<len(words):
  w=words[i]
  if w in (0xCC000000,0xCCC00002,0xDDD00002,0xCCC00004):i+=1;continue # v10.374: the ring's padding
  assert i%4==0,('block not 16-byte aligned',i)
  if w==0xFE03043C:n=words[i+1];out.append(('43C',[words[i+2]]+words[i+4:i+4+n]));i+=4+n;continue
  if w==0xFE000000:n=words[i+1];out.append(('2D',words[i+4:i+4+2*n]));i+=4+2*n;continue # v10.383: the ring's fence (a 2D fill)
  if w in (0xFE020440,0xFE020420):
   j=i+4;body=[]
   while j<len(words) and words[j]>>24!=0xFE:
    if words[j]!=0xCC000000:body.append(words[j])
    j+=1
   out.append(('43C' if w==0xFE020440 else '41C',[words[i+2]]+body));i=j;continue
  raise AssertionError('unknown word %08X at %d'%(w,i))
 return out
def fetch_checks():
 cases=[]
 for bpp in (32,16):
  g0=NativeGPU(bpp);assert g0.submit(packet(bpp))==0;R=blocks(g0.port_trace)
  cr=[b for b in R if b[0] in (0x00100000,0x00110000)];rest=[b for b in R if b[0] not in (0x00100000,0x00110000)]
  g=FetchGPU(bpp);starts=[];g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0])) # the whole state with every packet (the cache is checked below)
  for k in range(3):
   g.port_trace=[];assert g.submit(packet(bpp))==0
   direct=[(a,v) for a,v in g.port_trace if a in (0x43c,0x440)];ctl=[(a,v) for a,v in g.port_trace if a in (0x41c,0x420)]
   assert blocks(direct)==cr,('command-regulator blocks through the registers',blocks(direct),cr)
   w=[v for a,v in ctl if a==0x420]
   assert [a for a,v in ctl]==[0x41c]+[0x420]*7+[0x41c,0x420],ctl
   assert [v>>24 for v in w[:7]]==[0x60,0x61,0x62,0x63,0x64,0x65,0x68] and w[6]==0x680A0001 and w[7]==0x680A0000 and w[5]&0x200
   start=(w[0]&0xFFFFFF)|((w[1]&0xFF)<<24);end=(w[2]&0xFFFFFF)|((w[3]&0xFF)<<24);pause=(w[4]&0xFFFFFF)|((w[5]&0xFF)<<24)
   assert start%256==0 and FetchGPU.CMD<=start<pause<end==FetchGPU.CMD+0x40000-256 and (pause+4-start)%32==0
   n=(pause+4-start)//4;words=list(struct.unpack('<%dI'%n,bytes(g.u.mem_read(FB+start,4*n))))
   d=decode(words);assert d[-1][0]=='41C' and d[-1][1][0]==0x00300000,d[-1]
   assert [b for t,b in d[:-1]]==rest and all(t=='43C' for t,b in d[:-1]),'fetched blocks differ from the register path'
   starts.append(start)
  assert starts[0]==FetchGPU.CMD and starts[1]>starts[0] and starts[2]>starts[1]
  st=[g.word(g.addr('vx3dn_fetch_stat')+4*i) for i in range(5)];assert st[0]==3 and st[2]==0 and st[3]==0,st
  assert g.critical==0 and g.excluded==0 and g.seq[0x10]==0xa0
  cases.append(f'{bpp}bpp fetched draw: the command block holds the register path\'s blocks in XP\'s forms, the command-regulator blocks go through the registers, one-shot start and stop, packets move through the block')
  # v10.231: async (offscreen target, SR1A bit 3 and SR1E bit 0 already set): a draw returns after the start; the next one, a clear,
  # VX3DN_busy and VX2D_access retire it (idle, then the stop) first
  def off(bpp,op=1):
   q=packet(bpp,op);struct.pack_into('<I',q,20,0x100000);return q
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;seqs=[]
  for k in range(3):
   g.port_trace=[];assert g.submit(off(bpp))==0
   if k:assert not [1 for a,v in g.port_trace if a in (0x43c,0x440)],'register writes under a draw in flight (types 0x10/0x11 must be left out)'
   seqs.append([(a,v>>24 if a==0x420 else v) for a,v in g.port_trace if a in (0x41c,0x420)])
  start=[(0x41c,0x00100000)]+[(0x420,r) for r in (0x60,0x61,0x62,0x63,0x64,0x65,0x68)];stop=[(0x41c,0x00100000),(0x420,0x68)]
  assert seqs[0]==start and seqs[1]==stop+start and seqs[2]==stop+start,seqs
  st=[g.word(g.addr('vx3dn_fetch_stat')+4*i) for i in range(6)];assert st[0]==3 and st[5]==3 and st[3]==0,st
  g.port_trace=[];assert g.call('VX3DN_busy')==0;assert [(a,v>>24 if a==0x420 else v) for a,v in g.port_trace]==stop,g.port_trace
  g.port_trace=[];assert g.call('VX3DN_busy')==0 and not g.port_trace
  g.port_trace=[];assert g.submit(off(bpp))==0;g.port_trace=[];g.u.mem_write(ARENA+0x3800,pack([0]));g.call('VX2D_access',(0,))
  assert [(a,v>>24 if a==0x420 else v) for a,v in g.port_trace]==stop,g.port_trace
  g.port_trace=[];assert g.submit(off(bpp))==0;g.port_trace=[];assert g.submit(off(bpp,2))==0
  assert [(a,v>>24 if a==0x420 else v) for a,v in g.port_trace][:2]==stop,g.port_trace
  assert g.critical==0 and g.excluded==0 and g.seq[0x1a]==8 and g.seq[0x1e]==1
  cases.append(f'{bpp}bpp asynchronous fetched draws: no register writes under a draw in flight; retired before the next draw, by VX3DN_busy, VX2D_access and a clear')
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert g.submit(packet(bpp))==0
  assert [(a,v>>24 if a==0x420 else v) for a,v in g.port_trace if a in (0x41c,0x420)][-2:]==stop
  g=FetchGPU(bpp);g.seq[0x1a]=0;g.seq[0x1e]=0;assert g.submit(off(bpp))==0
  assert [(a,v>>24 if a==0x420 else v) for a,v in g.port_trace if a in (0x41c,0x420)][-2:]==stop
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_async'),pack([0]));assert g.submit(off(bpp))==0
  assert [(a,v>>24 if a==0x420 else v) for a,v in g.port_trace if a in (0x41c,0x420)][-2:]==stop
  cases.append(f'{bpp}bpp fetched draws wait for idle on the primary, with the engine gates closed before, and with ACCEL_D3D_ASYNC 0')
  # v10.232: the state cache: an identical packet sends the primitive set-up and the vertices only; a changed state, TEXDBG, a clear
  # and ACCEL_D3D_STATE_CACHE 0 send it whole
  def stream(g):
   ctl=[(a,v) for a,v in g.port_trace if a in (0x41c,0x420)];w=[v for a,v in ctl if a==0x420][-7:]
   if [x>>24 for x in w]!=[0x60,0x61,0x62,0x63,0x64,0x65,0x68]:w=[v for a,v in ctl if a==0x420][:7]
   start=(w[0]&0xFFFFFF)|((w[1]&0xFF)<<24);pause=(w[4]&0xFFFFFF)|((w[5]&0xFF)<<24);n=(pause+4-start)//4
   return decode(list(struct.unpack('<%dI'%n,bytes(g.u.mem_read(FB+start,4*n)))))
  def hdrs(d):return [b[0] for t,b in d]
  g=FetchGPU(bpp);full_hdrs=None
  for k in range(3):
   g.port_trace=[];assert g.submit(packet(bpp))==0;h=hdrs(stream(g))
   if k==0:full_hdrs=h;assert len(h)>4
   else:assert h==[0x00040000,0x00000000,0x00300000],h
  d=stream(g);assert d[1][1][0]==0 and len(d[1][1])==1+15,d[1]
  cs=[g.word(g.addr('vx3dn_cache_stat')+4*i) for i in range(4)];assert cs[0]==2,cs
  g.u.mem_write(g.addr('vx3dn_miss_stat'),bytes(72));g.u.mem_write(g.addr('vx3dn_miss_n'),pack([7])) # v10.429: the next miss is the sampled one
  q=packet(bpp);struct.pack_into('<I',q,4*21,8);g.port_trace=[];assert g.submit(q)==0;assert len(hdrs(stream(g)))>4,'a changed state is sent'
  ms=[g.word(g.addr('vx3dn_miss_stat')+4*i) for i in range(18)];assert ms[17]==1 and ms[4]==1 and sum(ms[:17])==1,ms
  g.u.mem_write(g.addr('vx3dn_miss_n'),pack([7]));q2=bytearray(q);struct.pack_into('<I',q2,4*13,1);g.port_trace=[];assert g.submit(bytes(q2))==0 # the clip's left edge: target group
  ms=[g.word(g.addr('vx3dn_miss_stat')+4*i) for i in range(18)];assert ms[17]==2 and ms[2]==1,ms
  g.port_trace=[];assert g.submit(q)==0
  cases.append(f'{bpp}bpp v10.429: a state-cache miss is sampled and put in the group that differed (write mask: raster; clip: target)')
  g.port_trace=[];assert g.submit(q)==0;assert len(hdrs(stream(g)))==3
  g.u.mem_write(ARENA+0x3800,pack([0x7A]));g.call('VX3DN_info',(ARENA+0xc000,))
  g.call('vx3dn_forget');g.port_trace=[];assert g.submit(q)==0;assert len(hdrs(stream(g)))>4,'forget sends the state'
  g.port_trace=[];assert g.submit(packet(bpp,2))==0;g.port_trace=[];assert g.submit(q)==0
  g.port_trace=[];assert g.submit(q)==0;assert len(hdrs(stream(g)))==3
  g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]));g.port_trace=[];assert g.submit(q)==0;assert len(hdrs(stream(g)))>4
  cases.append(f'{bpp}bpp state cache: an identical packet fetches the primitive set-up and vertices only; a changed state, vx3dn_forget and ACCEL_D3D_STATE_CACHE 0 send it whole')
  g=FetchGPU(bpp);g.u.mem_write(g.addr('vx3dn_fetch'),pack([0]));assert g.submit(packet(bpp))==0
  assert g.port_trace==g0.port_trace
  cases.append(f'{bpp}bpp ACCEL_D3D_FETCH 0 sends the register path\'s writes')
  g=FetchGPU(bpp);assert g.submit(packet(bpp,2))==0 and not g.port_trace
  cases.append(f'{bpp}bpp clears do not use the fetcher')
  # v10.236: runs (triangle lists and strips) in the full packet: 04/20 = 386300 before a strip burst and 0 before a list burst after
  # one; the fetched stream holds the register path's blocks; the state cache keeps the runs (geometry, not state) and 04/20 follows
  # the engine across packets; vx3dn_strip_hw 0 sends a strip's triangles as a list; bad runs are refused before MMIO
  allw=lambda g:[v for a,v in g.port_trace if a==0x440]
  g=NativeGPU(bpp);assert g.submit(full_packet(bpp,[(0,3),(1,6),(0,3)]))==0;w=allw(g)
  heads=[(w[i-1],w[i+1]) for i in range(1,len(w)-1) if w[i]==0x2300CE02]
  assert [x[1] for x in heads]==[0x24000003,0x24000006,0x24000003] and heads[1][0]==0x20386300 and heads[2][0]==0x20000000,heads
  Rf=blocks(g.port_trace);rest=[b for b in Rf if b[0] not in (0x00100000,0x00110000)]
  g=FetchGPU(bpp);g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]));g.port_trace=[];assert g.submit(full_packet(bpp,[(0,3),(1,6),(0,3)]))==0
  d=stream(g);assert [b for t,b in d[:-1]]==rest,'fetched runs differ from the register path'
  g=FetchGPU(bpp);assert g.submit(full_packet(bpp,[(1,6)]))==0
  g.port_trace=[];assert g.submit(full_packet(bpp,[(0,3),(1,5)]))==0;d=stream(g);h=hdrs(d)
  assert h[0]==0x00040000 and len(h)==5 and d[0][1][1]==0x20000000 and d[2][1][1]==0x20386300,(h,d[:3])
  g.port_trace=[];assert g.submit(full_packet(bpp,[(1,4)]))==0;d=stream(g);assert d[0][1][1]==0x2300CE02,d[0] # still a strip: no 04/20
  cs=[g.word(g.addr('vx3dn_cache_stat')+4*i) for i in range(4)];assert cs[0]==2,cs
  g=FetchGPU(bpp);g.u.mem_write(g.addr('vx3dn_strip_hw'),pack([0]));g.port_trace=[];assert g.submit(full_packet(bpp,[(1,6)]))==0;d=stream(g)
  ws=[x for t,b in d for x in b];assert 0x20386300 not in ws and [ws[i+1] for i in range(len(ws)-1) if ws[i]==0x2300CE02]==[0x2400000C],ws
  st=[g.word(g.addr('vx3dn_strip_stat')+4*i) for i in range(3)];assert st==[1,6,0],st
  for bad,n in (([(0,3),(1,6)],12),([(1,2),(0,6)],8),([(0,4),(1,5)],9),([(2,6),(0,3)],9),([(0,3)]*65,195)):
   g=NativeGPU(bpp);assert g.submit(full_packet(bpp,bad,n))==1 and not g.port_trace,bad
  q=full_packet(bpp,[(0,3),(1,6)]);struct.pack_into('<I',q,6580,1);g=NativeGPU(bpp);assert g.submit(q)==1 and not g.port_trace
  cases.append(f'{bpp}bpp strips: 04/20 386300 for a strip burst and 0 for a list after one, also across fetched packets with the state kept; the fetched runs as the register path\'s; ACCEL_D3D_STRIP_HW 0 sends the triangles; bad runs refused before MMIO')
  # v10.481: ext.dither bit 1, XP's flat triangles (T48): the burst's mode word 04/23 = 9E02 (bit 0 still the dither); other values refused
  for dv,mw in ((2,0x23009E02),(3,0x23009E02),(1,0x2300CE02)):
   q=full_packet(bpp,[(0,6)]);struct.pack_into('<I',q,6584,dv);g=NativeGPU(bpp);assert g.submit(q)==0;w=allw(g)
   assert mw in w and (0x2300CE02 in w)==(mw==0x2300CE02),(dv,[hex(x) for x in w if x>>24==0x23])
  q=full_packet(bpp,[(0,6)]);struct.pack_into('<I',q,6584,4);g=NativeGPU(bpp);assert g.submit(q)==1 and not g.port_trace
  cases.append(f'{bpp}bpp v10.481: a packet\'s ext.dither bit 1 draws its triangles with XP\'s flat mode word 9E02; dither 4 refused')
 return cases

def cgroup_checks():
 """v10.287: ACCEL_D3D_CGROUPS (default 1) without the diff (v10.304: 01/38-3F, the blend constants, left the constant group): after a whole state set-up in this state generation the constant runs of the
 set-up are not sent; the stream is the register path's without those writes (the first packet, and the first after vx3dn_forget, whole)."""
 cases=[]
 G1=set(range(3,16))|{0x15,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F,0x20,0x21,0x22,0x27}|set(range(0x2B,0x33))|{0x51,0x84,0x88,0x8A,0x58,0x59,0x5A,0x60,0x61,0x62,0x68,0x69,0x6A}
 G4=set(range(5,0x11))|{0x21,0x22,0x26,0x27,0x29,0x2C,0x2D,0x2E,0x2F,0x50,0x51,0x52}
 def off(q):q=bytearray(q);struct.pack_into('<I',q,20,0x100000);return q
 def fetched(g):
  w=[v for a,v in g.port_trace if a==0x420][-8:];w=[x for x in w if x>>24 in (0x60,0x61,0x62,0x63,0x64,0x65,0x68)][-7:]
  start=(w[0]&0xFFFFFF)|((w[1]&0xFF)<<24);pause=(w[4]&0xFFFFFF)|((w[5]&0xFF)<<24);n=(pause+4-start)//4
  return [b for t,b in decode(list(struct.unpack('<%dI'%n,bytes(g.u.mem_read(FB+start,4*n)))))][:-1]
 def drop(blocks):
  out=[];stage=1
  for b in blocks:
   h=b[0]
   if h==0x40000 and any(v>>24==0x23 for v in b[1:]):stage=2
   if stage<2 and h==0x10000:out.append([h]+[v for v in b[1:] if v>>24 not in G1])
   elif stage<2 and h==0x40000:out.append([h]+[v for v in b[1:] if v>>24 not in G4])
   else:out.append(list(b))
  return out
 for bpp in (32,16):
  a=off(full_packet(bpp,[(0,3)]));b=bytearray(a);struct.pack_into('<I',b,4*23,7)
  def ref(q,first):
   g0=FetchGPU(bpp);g0.seq[0x1a]=8;g0.seq[0x1e]=1;g0.u.mem_write(g0.addr('vx3dn_state_cache'),pack([0]));g0.port_trace=[];assert g0.submit(q)==0
   f=fetched(g0);return f if first else drop(f)
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]));g.u.mem_write(g.addr('vx3dn_cgroup'),pack([1]));g.u.mem_write(g.addr('vx3dn_shadow'),pack([0])) # .525: the constant groups alone (the set-up's shadow: shadow_ixchk_checks)
  sizes=[]
  for k,q in enumerate((a,b,a)):
   g.port_trace=[];assert g.submit(q)==0;got=fetched(g);assert got==ref(q,k==0),(bpp,k);sizes.append(sum(len(x) for x in got))
  g.call('vx3dn_forget');g.port_trace=[];assert g.submit(a)==0;assert fetched(g)==ref(a,True),'after vx3dn_forget: whole'
  assert sizes[1]<sizes[0] and sizes[2]<sizes[0],sizes
  cases.append(f'{bpp}bpp constant groups: after a whole set-up the constant runs are left out ({sizes[0]} -> {sizes[2]} words), the rest the register path\'s; vx3dn_forget sends everything again')
 return cases
def clear_async_checks():
 """v10.282: ACCEL_3D_CLEAR_ASYNC (default 1): a clear of an offscreen target fills the depth first and leaves the target's fill running
 on the 2D engine; the next submit (and a Lock) finishes it before the engine is used again; the primary and ACCEL_3D_CLEAR_ASYNC 0
 wait as before."""
 cases=[]
 for bpp in (32,16):
  def clear(target_off=0x100000,z=True):
   q=packet(bpp,2);struct.pack_into('<I',q,20,target_off)
   if z:struct.pack_into('<5I',q,40,0x140000,64*(bpp//8),64,48,12 if bpp==16 else 3);struct.pack_into('<I',q,96,3);struct.pack_into('<I',q,104,0)
   return q
  def dests(g,w0):
   out=[];base=None
   for at,v in g.writes[w0:]:
    if at==0x14:base=v*8
    if at==0 and v&15==1:out.append(base)
   return out
  g=NativeGPU(bpp);g.u.mem_write(g.addr('vx3dn_clear_async'),pack([1]));g.async_busy=3;w0=len(g.writes)
  g.u.mem_write(BATCH,bytes(clear()));assert g.call('VX3DN_submit',(BATCH,),False)==0
  d=dests(g,w0);assert d==[0x140000,0x100000],[hex(x) for x in d]
  assert g.call('VX2D_pending',(),False)==1 and g.pend is not None,'the target fill left running'
  assert g.word(g.addr('vx3dn_clear_async_n'))==1 and g.seq[0x10]&1,'the sequencer stays open for the running fill (as an asynchronous DirectDraw blit)'
  q=packet(bpp);struct.pack_into('<I',q,20,0x100000);assert g.submit(q)==0 # NativeGPU.write asserts no 3D port write while the fill runs
  assert g.call('VX2D_pending')==0 and g.pend is None
  cases.append(f'{bpp}bpp a clear of an offscreen target and its Z: Z filled first, the target fill left running; the next draw waits for it before its first 3D write')
  g=NativeGPU(bpp);g.u.mem_write(g.addr('vx3dn_clear_async'),pack([1]));g.async_busy=3
  g.u.mem_write(BATCH,bytes(clear()));assert g.call('VX3DN_submit',(BATCH,),False)==0
  assert g.call('VX2D_access',(1,))==1 and g.pend is None and g.call('VX2D_pending')==0;g.call('VX2D_access',(2,))
  cases.append(f'{bpp}bpp a Lock (VX2D_access) after an asynchronous clear waits for the fill')
  g=NativeGPU(bpp);g.u.mem_write(g.addr('vx3dn_clear_async'),pack([1]));g.u.mem_write(g.addr('vx2d_keep'),pack([1]));g.async_busy=3
  g.u.mem_write(BATCH,bytes(clear()));assert g.call('VX3DN_submit',(BATCH,),False)==0 and g.pend is not None
  q=packet(bpp);struct.pack_into('<I',q,20,0x100000);assert g.submit(q)==0 and g.pend is None and g.call('VX2D_pending')==0
  cases.append(f'{bpp}bpp with ACCEL_KEEP 1 (the kept 2D context): the asynchronous clear is finished and the context handed back by the next draw')
  g=NativeGPU(bpp);g.u.mem_write(g.addr('vx3dn_clear_async'),pack([1]));assert g.submit(clear(0))==0 and g.call('VX2D_pending')==0
  g=NativeGPU(bpp);assert g.submit(clear())==0 and g.call('VX2D_pending')==0 # the checks' default: ACCEL_3D_CLEAR_ASYNC 0
  cases.append(f'{bpp}bpp a clear of the primary, and every clear with ACCEL_3D_CLEAR_ASYNC 0, has finished when the submit returns')
 return cases
def batch_checks():
 """v10.275: several draw packets in one call (VX3DN_submit_batch): one sequencer open, the packets' words one after another in a fetch
 group, XP's batch end once, one start a group; a full group is started and the next built while it runs; a refused packet stops the
 batch after the ones before it are sent; clears are refused (v10.368: draws on the primary are taken); without the fetcher every packet goes alone."""
 cases=[]
 def off(q):q=bytearray(q);struct.pack_into('<I',q,20,0x100000);return q
 def call(g,qs):
  g.u.mem_write(BATCH,b''.join(bytes(q) for q in qs));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
  rc=g.call('VX3DN_submit_batch',(BATCH,len(qs),ARENA+0xc800));return rc,g.word(ARENA+0xc800)
 def starts(g):
  w=[v for a,v in g.port_trace if a==0x420];out=[]
  for i in range(len(w)-6):
   if [x>>24 for x in w[i:i+7]]==[0x60,0x61,0x62,0x63,0x64,0x65,0x68] and w[i+6]==0x680A0001:out.append(w[i:i+7])
  return out
 def stream(g,w):
  start=(w[0]&0xFFFFFF)|((w[1]&0xFF)<<24);pause=(w[4]&0xFFFFFF)|((w[5]&0xFF)<<24);n=(pause+4-start)//4
  return decode(list(struct.unpack('<%dI'%n,bytes(g.u.mem_read(FB+start,4*n)))))
 for bpp in (32,16):
  one=off(full_packet(bpp,[(0,3)]))
  g0=FetchGPU(bpp);g0.seq[0x1a]=8;g0.seq[0x1e]=1;g0.u.mem_write(g0.addr('vx3dn_state_cache'),pack([0]))
  assert g0.submit(one)==0;single=stream(g0,starts(g0)[0])
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]))
  assert call(g,[one]*3)==(0,3);st=starts(g);assert len(st)==1,st
  d=stream(g,st[0]);body=single[:-1]
  assert d[:-1]==body*3 and d[-1][0]=='41C','three packets one after another, one batch end'
  b=[g.word(g.addr('vx3dn_batch_stat')+4*i) for i in range(4)];assert b==[1,3,1,3],b
  fs=[g.word(g.addr('vx3dn_fetch_stat')+4*i) for i in range(6)];assert fs[0]==3 and fs[5]==1,fs
  assert g.critical==0 and g.excluded==0 and g.seq[0x1a]==8 and g.seq[0x1e]==1
  g.port_trace=[];assert call(g,[one]*2)==(0,2);st=starts(g)
  stop=[v for a,v in g.port_trace if a==0x420].index(0x680A0000);assert stop<[v for a,v in g.port_trace if a==0x420].index(st[0][0]),'the group in flight retired before the next start'
  assert not [1 for a,v in g.port_trace if a in (0x43c,0x440)],'no register writes under a group in flight'
  cases.append(f'{bpp}bpp batch: three packets in one call are one fetch of the three register-path streams one after another and one batch end; the next batch retires it first')
  # the state cache inside a group: identical packets send the primitive set-up and vertices only
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert call(g,[one]*3)==(0,3);d=stream(g,starts(g)[0]);h=[b[0] for t,b in d]
  full=[b[0] for t,b in single[:-1]];assert h==full+[0x00040000,0x00000000]*2+[0x00300000],h
  cases.append(f'{bpp}bpp batch with the state cache: the packets after the first of the same state are their vertices only')
  # a full group is started and the next one built: 32 packets of 96 vertices
  big=off(full_packet(bpp,[(0,96)]))
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]))
  assert call(g,[big]*32)==(0,32);st=starts(g);assert len(st)>=2,len(st)
  n=0;g1=FetchGPU(bpp);g1.seq[0x1a]=8;g1.seq[0x1e]=1;g1.u.mem_write(g1.addr('vx3dn_state_cache'),pack([0]));assert g1.submit(big)==0;bs=stream(g1,starts(g1)[0])[:-1]
  for w in st:
   d=stream(g,w);assert d[-1][0]=='41C';k=len(d)-1;assert k%len(bs)==0 and d[:-1]==bs*(k//len(bs));n+=k//len(bs)
   start=(w[0]&0xFFFFFF)|((w[1]&0xFF)<<24);pause=(w[4]&0xFFFFFF)|((w[5]&0xFF)<<24);assert pause+4-start<=0x8000
  assert n==32
  b=[g.word(g.addr('vx3dn_batch_stat')+4*i) for i in range(4)];assert b[1]==32 and b[2]==len(st),b
  cases.append(f'{bpp}bpp batch of 32 packets of 96 vertices: {len(st)} fetches of at most 32 KB, every packet in one of them whole')
  # v10.365: ACCEL_D3D_FETCH_KB 128 (taken as 80): the same batch in fewer, larger groups (at most 80 KB each), every packet whole, the same words
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]));g.u.mem_write(g.addr('vx3dn_fetch_kb'),pack([128]))
  assert call(g,[big]*32)==(0,32);st2=starts(g);assert 1<=len(st2)<len(st),(len(st2),len(st))
  n=0
  for w in st2:
   d=stream(g,w);assert d[-1][0]=='41C';k=len(d)-1;assert k%len(bs)==0 and d[:-1]==bs*(k//len(bs));n+=k//len(bs)
   start=(w[0]&0xFFFFFF)|((w[1]&0xFF)<<24);pause=(w[4]&0xFFFFFF)|((w[5]&0xFF)<<24);assert pause+4-start<=0x14000
  assert n==32
  prev=None
  for rep in range(6): # further batches: the block wraps; consecutive groups (one in flight while the next is built) never overlap
   g.port_trace=[];assert call(g,[big]*32)==(0,32);s3=starts(g);assert len(s3)==len(st2)
   for w in s3:
    r=((w[0]&0xFFFFFF)|((w[1]&0xFF)<<24),(w[4]&0xFFFFFF)|((w[5]&0xFF)<<24))
    if prev:assert r[1]<prev[0] or r[0]>prev[1],(hex(r[0]),hex(r[1]),hex(prev[0]),hex(prev[1]))
    prev=r
  cases.append(f'{bpp}bpp ACCEL_D3D_FETCH_KB 128 (80): the 32 packets in {len(st2)} fetches of at most 80 KB (32 KB: {len(st)}), every packet whole, a second batch after it')
  # a refused packet: the ones before it are sent, the result returned; clears and the primary are refused before anything
  bad=off(full_packet(bpp,[(0,3),(1,6)],12))
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert call(g,[one,bad,one])==(1,1);assert len(starts(g))==1
  g=FetchGPU(bpp);clr=off(full_packet(bpp,[(0,3)]));struct.pack_into('<I',clr,12,2);assert call(g,[clr,one])==(1,0) and not starts(g)
  assert g.critical==0 and g.excluded==0
  cases.append(f'{bpp}bpp batch: a refused packet stops it after the packets before it are fetched; a clear is refused before anything is sent')
  # v10.368 (audit N8): a draw on the primary's memory is taken in a batch (the cursor excluded to the batch's end, which is released)
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert call(g,[one,full_packet(bpp,[(0,3)]),one])==(0,3) and len(starts(g))==1
  assert g.critical==0 and g.excluded==0 and getattr(g,'begins',0)==1
  g=FetchGPU(bpp);assert call(g,[full_packet(bpp,[(0,3)])])==(0,1) and len(starts(g))==1 and g.excluded==0
  g=FetchGPU(bpp);assert call(g,[one,one])==(0,2) and getattr(g,'begins',0)==0 # offscreen only: no exclusion
  # v10.371: the bracket follows the cursor (VESA_draw_needs_exclusion), not offset 0: none for a primary draw the cursor is not on, one
  # for an offscreen draw it is on (the surface shown)
  g=FetchGPU(bpp);g.excl_rule=0;g.seq[0x1a]=8;g.seq[0x1e]=1;assert call(g,[one,full_packet(bpp,[(0,3)]),one])==(0,3) and getattr(g,'begins',0)==0 and g.excluded==0
  g=FetchGPU(bpp);g.excl_rule=1;assert call(g,[one,one])==(0,2) and getattr(g,'begins',0)==1 and g.excluded==0
  cases.append(f'{bpp}bpp batch: draws on the primary are taken with the offscreen ones, the cursor exclusion released at the end')
  # without the fetcher: each packet alone, the register path's writes
  g=FetchGPU(bpp);g.u.mem_write(g.addr('vx3dn_fetch'),pack([0]));assert call(g,[one]*2)==(0,2)
  gs=FetchGPU(bpp);gs.u.mem_write(gs.addr('vx3dn_fetch'),pack([0]));assert gs.submit(one)==0 and gs.submit(one)==0
  assert g.port_trace==gs.port_trace and not starts(g)
  cases.append(f'{bpp}bpp batch with ACCEL_D3D_FETCH 0: every packet alone through the registers')
  # v10.281 (board, VXDX7 hang test): the draw in flight hung; the next batch's start resets the engine and draws its packets again on it
  rec=lambda g:[g.word(g.addr('vx3dn_rec')+4*i) for i in range(2)]
  for how in ('batch','single'):
   g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert call(g,[one])==(0,1) and len(starts(g))==1
   g.busy=True;g.port_trace=[];r=call(g,[one]) if how=='batch' else (g.submit(one),1)
   st=starts(g);assert r==(0,1),(how,r);assert rec(g)==[1,0],rec(g);assert len(st)==1,len(st)
   assert stream(g,st[0])==single,'the packet built again on the reset engine: the whole stream'
   assert g.critical==0 and g.seq[0x1a]==8 and g.seq[0x1e]==1
   h=[g.word(g.addr('vx3dn_hang')+4*i) for i in range(6)]
   assert g.word(g.addr('vx3dn_hang_count'))==1 and g.word(g.addr('vx3dn_hang_n'))==1 and h[0]&255==1 and h[0]>>24==3 and h[5]>0,(h,) # v10.291: the hung group's packet kept
  cases.append(f'{bpp}bpp a hung draw in flight: the next batch and the next single draw reset the engine, build their packets again and draw them (they were dropped); the hung group\'s packets are kept (TEXDBG E0)')
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert call(g,[one])==(0,1);g.busy=True;g.call('VX2D_sync');assert rec(g)==[1,0] and not g.busy
  cases.append(f'{bpp}bpp VX2D_sync (TEXDBG 0x70 reads after it) retires a hung draw in flight and resets the engine')
  # v10.285 (review): a DirectDraw request declined early retires the draw in flight; when that retire loses the device the answer is
  # LOST (the HAL would otherwise finish it on the CPU over a hung engine), with a working reset it is SKIP (the CPU finishes it)
  for stuck in (False,True):
   g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert call(g,[one])==(0,1);g.busy=True;g.stuck=stuck
   g.u.mem_write(g.addr('vx2d_fill_min'),pack([100000]));q=g.request(1,flags=16);g.u.mem_write(ARENA+0x1000,pack(q))
   r=g.call('VX2D_submit',(ARENA+0x1000,),False);assert r==(3 if stuck else 0),(stuck,r);assert rec(g)==([0,1] if stuck else [1,0]),rec(g)
  cases.append(f'{bpp}bpp a declined DirectDraw request whose retire loses the device answers LOST (a working reset: SKIP, the CPU finishes it)')
  # v10.285 (review): the status query (VX2D_access 3, GetBltStatus) answers BUSY while a draw runs, without waiting or resetting
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert call(g,[one])==(0,1);g.busy=True;reads=[0]
  from unicorn import UC_HOOK_MEM_READ
  g.u.hook_add(UC_HOOK_MEM_READ,lambda u,t,a,z,v,d:reads.__setitem__(0,reads[0]+1),None,MM+0x400,MM+0x403)
  assert g.call('VX2D_access',(3,),False)==4 and rec(g)==[0,0] and reads[0]<=2,(rec(g),reads[0])
  g.busy=False;assert g.call('VX2D_access',(3,),False)==1 and rec(g)==[0,0]
  st=starts(g);g.port_trace=[];assert call(g,[one])==(0,1),'the next draw starts at once (the draw before was retired by the status query)'
  cases.append(f'{bpp}bpp GetBltStatus with a draw running: BUSY after one status read, no wait and no reset; once idle the draw is retired')
 return cases

def sink_checks():
 """v10.282: the direct sink changes no word: ACCEL_D3D_SINK 0/1 fetch the same streams, textured and untextured, single and batched."""
 cases=[]
 def off(q):q=bytearray(q);struct.pack_into('<I',q,20,0x100000);return q
 def fetched(g):
  w=[v for a,v in g.port_trace if a==0x420][-8:];w=[x for x in w if x>>24 in (0x60,0x61,0x62,0x63,0x64,0x65,0x68)][-7:]
  start=(w[0]&0xFFFFFF)|((w[1]&0xFF)<<24);pause=(w[4]&0xFFFFFF)|((w[5]&0xFF)<<24);n=(pause+4-start)//4
  return [b for t,b in decode(list(struct.unpack('<%dI'%n,bytes(g.u.mem_read(FB+start,4*n)))))][:-1]
 for bpp in (32,16):
  a=off(full_packet(bpp,[(0,3)]));b=bytearray(a);struct.pack_into('<I',b,4*23,7) # write mask changed (01/53)
  tx=off(full_packet(bpp,[(0,6)]))
  def run(sink,qs,batch):
   g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]))
   g.u.mem_write(g.addr('vx3dn_sink'),pack([sink]));out=[]
   if batch:g.u.mem_write(BATCH,b''.join(bytes(q) for q in qs));assert g.call('VX3DN_submit_batch',(BATCH,len(qs),ARENA+0xc800))==0;return fetched(g)
   for q in qs:g.port_trace=[];assert g.submit(q)==0;out.append(fetched(g))
   return out
  for batch in (0,1):
   qs=[a,b,a,tx,a]
   assert run(1,qs,batch)==run(0,qs,batch),(bpp,batch)
  cases.append(f'{bpp}bpp ACCEL_D3D_SINK: the same fetched words on and off (single and batched)')
 return cases


class XRGPU(FetchGPU):
 """v10.402: XP's ring: the page table at GART in video memory, 65 fixed pages (fake physical 0x01000000 up) at XL; the fetcher model
 reads GPU addresses through the table when register 60 bit 0 is set, keeps 43C at its last pause + 0x10 and 444 bit 31 (paused)."""
 GART=0x7F0000;XL=0x9000000;PH=0x01000000
 def __init__(self,bpp=32,live=True,**k):
  super().__init__(bpp,ring=True,**k);self.live=live;self.gmode=False;self.allocs=0
  self.u.mem_map(self.XL,0x42000);self.u.mem_write(self.addr('vx3dn_gart_off'),pack([self.GART]))
  self.hook('__PageAllocate',self.palloc);self.hook('__CopyPageTable',self.cpt);self.hook('__PageFree',lambda:1)
 def palloc(self):
  assert self.arg(0)==65 and self.arg(1)==1 and self.arg(7)&8;self.allocs+=1
  if self.arg(6):self.u.mem_write(self.arg(6),pack([self.PH]))
  return self.XL
 def cpt(self):
  assert self.arg(0)==self.XL>>12 and self.arg(1)==65
  self.u.mem_write(self.arg(2),pack([(self.PH+i*4096)|0x67 for i in range(65)]));return 1
 def entry(self,i):return self.word(FB+self.GART+4*i)
 def gw(self,a):
  if not self.gmode:return self.word(FB+a)
  e=self.entry(a>>12);assert not e&0x80000000,('unmapped GPU page',hex(a),hex(e))
  return self.word(self.XL+((e&0xFFFFF)<<12)-self.PH+(a&0xFFF))
 def gwords(self,a,n):return [self.gw(a+4*i) for i in range(n)]
 def fetch_2d(self,a,b):
  i=a
  while i+16<=b:
   if self.gw(i)==0xFE000000:
    n=self.gw(i+4);r={self.gw(i+16+8*k):self.gw(i+20+8*k) for k in range(n)}
    if r.get(0x0C,1)==0 and 0x14 in r:
     at=FB+r[0x14]*8;c=r.get(0x58,0)
     self.u.mem_write(at,struct.pack('<H',c&0xFFFF) if r.get(0x04)==0x100 else struct.pack('<I',c));self.fences.append((r[0x14]*8,c))
    i+=16+8*n;i=(i+15)&~15;continue
   i+=4
 def write(self,u,t,a,z,v,_):
  if a-MM==0x420:
   top=v>>24;self.fregs[top]=v&0xFFFFFF
   if v==0x680A0001:
    self.gmode=bool(self.fregs.get(0x60,0)&1);st=(self.fregs.get(0x60,0)&~1)|((self.fregs.get(0x61,0)&0xFF)<<24);p=self.fregs.get(0x64,0)|((self.fregs.get(0x65,0)&0xFF)<<24)
    self.fpos=st;self.fetch_2d(st,p);self.fpos=p;self.mark()
   elif top==0x65 and self.fpos is not None:
    p=self.fregs.get(0x64,0)|((v&0xFF)<<24)
    if self.gmode and p<self.fpos:
     end=self.fregs.get(0x66,0)|((self.fregs.get(0x67,0)&0xFF)<<24);s0=(self.fregs.get(0x60,0)&~1)|((self.fregs.get(0x61,0)&0xFF)<<24)
     self.fetch_2d(self.fpos,end+0x10);self.fpos=s0-0x10;self.wraps_seen=getattr(self,'wraps_seen',0)+1 # the board: 43C = the ring's start after a wrap
    if p>=self.fpos:self.fetch_2d(self.fpos,p);self.fpos=p
    self.mark()
   if v==0x680A0000:self.fpos=None
   self.port_trace.append((a-MM,v));return
  return super().write(u,t,a,z,v,_)
 def mark(self):
  if getattr(self,'freeze',False):return # v10.410: 43C/444 left as the test set them
  if self.gmode and self.live:u=self.u;u.mem_write(MM+0x43C,pack([self.fpos+0x10]));u.mem_write(MM+0x444,pack([0xC2000000]))
  else:self.u.mem_write(MM+0x43C,pack([0]));self.u.mem_write(MM+0x444,pack([0]))
def xr_fence_checks():
 """v10.406 (the fence checks moved to XP's ring): each group's 1x1 fill is waited for; a fence that never comes is a hang in the ring:
 its record and tape kept (the groups' words read through the page table), the ring stopped (68 = 0A0000) and off for the session, the
 next group one-shot from the command block in video memory."""
 cases=[]
 for bpp in (32,16):
  pk=bytearray(packet(bpp));struct.pack_into('<I',pk,20,0x100000)
  g=XRGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]))
  for k in range(4):assert g.submit(pk)==0
  g.port_trace=[];assert g.call('VX3DN_busy',())==0;assert [v for a,v in g.port_trace if a==0x420]==[]
  rs=[g.word(g.addr('vx3dn_rf_stat')+4*i) for i in range(4)]
  assert rs[0]==4 and rs[2]==0 and g.word(g.addr('vx3dn_rhang_ok'))==0 and [c&0xFFFF for o,c in g.fences]==[1,2,3,4],(rs,g.fences)
  P=[g.word(g.addr('vx3dn_xr_prof')+4*i) for i in range(12)];assert P[10]==4 and P[11]>=1 and P[9]==P[11],P # v10.407: kicks, retires, put-backs counted
  cases.append(f'{bpp}bpp XP ring fence: 4 groups, 4 fences (seq 1-4) consumed, no stop, no hang recorded')
  g=XRGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]))
  for k in range(3):assert g.submit(pk)==0
  g.fetch_2d=lambda a,b:None # the fences stop coming: the fetcher stuck
  assert g.submit(pk)==0;g.port_trace=[];g.call('VX3DN_busy');w=[v for a,v in g.port_trace if a==0x420]
  rs=[g.word(g.addr('vx3dn_rf_stat')+4*i) for i in range(4)];X=[g.word(g.addr('vx3dn_xr_stat')+4*i) for i in range(72)]
  assert g.word(g.addr('vx3dn_rhang_ok'))==1 and rs[2]==1 and rs[3]==1 and X[13]==1 and 0x680A0000 in w,(rs,X[13],[hex(x) for x in w])
  assert g.call('VX3DN_tape',(0,0,0,ARENA+0x20000,0x10000+16*4+64*16+4*(2+256)+12))==0
  T=struct.unpack('<16I',bytes(g.u.mem_read(ARENA+0x20000,64)));G=[struct.unpack('<4I',bytes(g.u.mem_read(ARENA+0x20000+64+16*i,16))) for i in range(4)]
  assert T[0]==0x45504154 and T[3]==0x100000 and T[7]==4 and [x[3] for x in G]==[1,2,3,4] and T[14]&8,(T,G)
  W=struct.unpack('<%dI'%T[6],bytes(g.u.mem_read(ARENA+0x20000+64+1024,4*T[6])));assert list(W)==g.gwords(T[5],T[6]),'tape words = the ring through the table'
  g.port_trace=[];assert g.submit(pk)==0;w=[v for a,v in g.port_trace if a==0x420]
  st=[x for x in w if x>>24==0x60];assert st and all(x&1==0 for x in st) and any(x>>24==0x65 and x&0x200 for x in w),[hex(x) for x in w]
  cases.append(f'{bpp}bpp XP ring fence: a fence that never comes is a hang: record and tape (4 groups, {T[6]} words through the page table), the ring stopped and off, the next group one-shot in video memory')
 return cases

class SlowXR(XRGPU):
 """v10.465: a fetcher that takes its time: the groups kicked are held (no fence) and, while held, 43C moves by 16 bytes a millisecond
 (moving=True) or stays (moving=False); after `after` ms they are fetched (the fences written)"""
 def __init__(self,bpp,**k):
  super().__init__(bpp,**k);self.held=None;self.moving=True;self.after=None;self.t_hold=0
 def tick(self):
  t=super().tick()
  if self.held is not None:
   if self.after is not None and t-self.t_hold>=self.after:
    for a,b in self.held:XRGPU.fetch_2d(self,a,b)
    self.held=None;self.mark()
   elif self.moving:self.u.mem_write(MM+0x43C,pack([(self.word(MM+0x43C)+16)&0xFFFFFFFF]))
  return t
 def hold(self,a,b):
  if self.held is None:self.held=[];self.t_hold=self.t
  self.held.append((a,b))
def xr_progress_checks():
 """v10.465 ACCEL_D3D_RINGPROG (review): the retire's wait gave up 100 ms after it began; now after 100 ms with neither 43C nor
 the fence moving (2 s in all). A fetcher still moving after 250 ms is waited for (no hang); one that stops is a hang as before; RINGPROG 0
 gives up at 100 ms (the old rule)."""
 cases=[]
 pk=bytearray(packet(32));struct.pack_into('<I',pk,20,0x100000)
 def run(prog,moving,after):
  g=SlowXR(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]));g.u.mem_write(g.addr('vx3dn_ringprog'),pack([prog]))
  for k in range(2):assert g.submit(pk)==0
  g.fetch_2d=g.hold;g.moving=moving;g.after=after
  assert g.submit(pk)==0;g.call('VX3DN_busy')
  rs=[g.word(g.addr('vx3dn_rf_stat')+4*i) for i in range(4)];rp=[g.word(g.addr('vx3dn_rp_stat')+4*i) for i in range(2)]
  run.hist=[g.word(g.addr('vx3dn_rp_hist')+4*i) for i in range(6)]
  return rs[2],rp,g.word(g.addr('vx3dn_rhang_ok'))
 h,rp,ok=run(1,True,250);assert h==0 and ok==0 and rp[0]==1 and 250<=rp[1]<400,(h,rp,ok)
 assert run.hist[4]==1 and sum(run.hist[5:])==0,run.hist # v10.467: the wait counted as 101-500 ms
 cases.append(f'v10.465 RINGPROG: a fetcher still moving is waited for past 100 ms ({rp[1]} ms), no hang, the wait counted')
 h,rp,ok=run(1,False,250);assert h==1 and ok==1 and rp[0]==0 and 100<=rp[1]<200,(h,rp,ok)
 cases.append(f'v10.465 RINGPROG: a fetcher that does not move is a hang after {rp[1]} ms, as before')
 h,rp,ok=run(0,True,250);assert h==1 and ok==1 and 100<=rp[1]<120,(h,rp,ok)
 cases.append('v10.465 RINGPROG 0: the wait gives up at 100 ms (the old rule) though the fetcher moves')
 h,rp,ok=run(1,True,None);assert h==1 and ok==1 and 2000<=rp[1]<2200,(h,rp,ok)
 cases.append('v10.465 RINGPROG: a fetcher that moves but never finishes is a hang after 2 s')
 # v10.466 ACCEL_D3D_RINGCAP: the first packet of a group in XP's ring writes no command-regulator block to 43C/440 directly (in the words)
 def first(cap):
  g=XRGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_ringcap'),pack([cap]));g.port_trace=[]
  assert g.submit(pk)==0;return [a for a,v in g.port_trace if a in (0x43C,0x440)],g.fences
 d1,f1=first(1);d0,f0=first(0)
 assert not d1 and f1,(d1,f1);assert d0,'RINGCAP 0: the old direct writes'
 # v10.468: the burst-head profile: with the profile on a packet's bursts and its time before the first burst are counted; off, nothing
 g=XRGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx_prof'),pack([1]))
 assert g.submit(pk)==0;R=[g.word(g.addr('vx3dn_rprof')+4*i) for i in range(5)];assert R[4]==1 and R[1]>=1 and R[0]>0,R
 g.u.mem_write(g.addr('vx_prof'),pack([0]));assert g.submit(pk)==0;assert [g.word(g.addr('vx3dn_rprof')+4*i) for i in range(5)]==R
 cases.append('v10.468: the primitive set-up profile counts a packet, its bursts and their heads\' time with the profile on, nothing with it off')
 cases.append('v10.466 RINGCAP: in XP\'s ring the first packet\'s regulator blocks go in the group\'s words (no 43C/440 register write), the group drawn and fenced; 0 writes them directly as before')
 return cases

def xr_checks():
 """v10.402: ACCEL_D3D_XPRING: the page table (entries 0x100-0x13F the ring's pages | 0x01000000, a guard page around, the rest
 0x80000000; SR6A/6B/6C its offset, SR6F bit 7 after); XP's start (60 = 0x100000 | 1, 62/63 the ring's end - 0x10, 66/67 FFFFF0/FF,
 68 = 0A0001) after the first-start check; groups kicked with 64/65 only, never stopped by a wait; a stop (mode set) then XP's restart
 (the pad at 43C run with the 0x200 pause) before the start; wraps at the ring's end (66/67, 64/65 = start - 0x10) with no stop; a first
 start that never pauses in the ring (43C 0) turns it off (after a second try with SR6C bit 7) and the ring in video memory goes on."""
 cases=[]
 for bpp in (32,16):
  pk=bytearray(packet(bpp));struct.pack_into('<I',pk,20,0x100000)
  g0=NativeGPU(bpp);assert g0.submit(pk)==0;R=blocks(g0.port_trace);rest=[b for b in R if b[0] not in (0x00100000,0x00110000)]
  g=XRGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]));g.port_trace=[]
  for k in range(3):assert g.submit(pk)==0
  X=[g.word(g.addr('vx3dn_xr_stat')+4*i) for i in range(66)]
  assert X[0]==3 and X[2]==1 and X[5]==3 and X[1]==0 and g.allocs==1 and X[60]==0 and X[61]==1,X
  T=[g.entry(i) for i in range(16384)]
  assert all(T[0x100+i]==0x01000000|((XRGPU.PH>>12)+i) for i in range(64)) and T[0xFF]==T[0x140]==T[0x143]==0x01000000|((XRGPU.PH>>12)+64) # v10.409: bit 25 clear (no snooping) and T[0]==T[0xFE]==T[0x144]==T[16383]==0x80000000
  assert g.seq.get(0x6a)==(XRGPU.GART>>12)&0xFF and g.seq.get(0x6b)==(XRGPU.GART>>20)&0xFF and g.seq.get(0x6c,0)&0x81==0 and getattr(g,'tlb_flushes',0)>=1 and X[19] not in (0,0xFFFFFFFF),(g.seq.get(0x6a),g.seq.get(0x6b),g.seq.get(0x6c),g.seq.get(0x6f),X[19]) # v10.426: the flush was asked for and finished (polled, cleared)
  if bpp==32: # v10.426 (review M08): a GART cache flush that never finishes: the ring is not started (off, reason 5); the draws go the one-shot way
   gs=XRGPU(bpp);gs.seq[0x1a]=8;gs.seq[0x1e]=1;gs.tlb_stuck=True;gs.u.mem_write(gs.addr('vx3dn_state_cache'),pack([0]))
   for k in range(2):assert gs.submit(pk)==0
   XS=[gs.word(gs.addr('vx3dn_xr_stat')+4*i) for i in range(20)];assert XS[0]==0 and XS[1]==5 and XS[19]==0xFFFFFFFF,XS
   cases.append('a GART translation-cache flush that never finishes (SR6F bit 7 stays set): the ring is not started (off, reason 5), the draws still go (one-shot)')
  w=[v for a,v in g.port_trace if a==0x420];assert w[:2]==[0x680A0000,0x02000000],[hex(x) for x in w[:3]];w=w[2:] # v10.403: XP's engine initialisation first
  assert [x>>24 for x in w[:9]]==[0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68] and w[0]==0x60100001 and w[1]==0x61000000 and w[2]==0x62000000|(0x140000-0x10) and w[4]==0x64000000|0x1000F0 and w[8]==0x680A0001,[hex(x) for x in w[:9]]
  kicks=w[9:];assert [x>>24 for x in kicks]==[0x64,0x65]*3 and 0x680A0000 not in w,[hex(x) for x in kicks]
  pauses=[(kicks[2*i]&0xFFFFFF)|((kicks[2*i+1]&0xFF)<<24) for i in range(3)];assert 0x100100<pauses[0]<pauses[1]<pauses[2] and all((p+0x10)%256==0 for p in pauses)
  S=g.gwords(0x100000,64);assert S[:4]==[0xFE020420,0x41C,0x00300000,0xCC000000] and S[4:6]==[0xCCC00002,0xDDD00002]
  n=(pauses[2]+0x10-0x100000)//4;d=decode(g.gwords(0x100000,n));body=[b for t,b in d if t=='43C']
  assert body==rest*3 and [c&0xFFFF for o,c in g.fences]==[1,2,3],(len(body),g.fences)
  cases.append(f'{bpp}bpp XP ring: the page table (ring pages | 0x01000000 at 0x100-0x13F, guard page, the rest 0x80000000), SR6A/6B/6C, SR6F bit 7; XP\'s start in system memory (60 = 0x100001); 3 groups kicked (64/65), fences 1-3, the blocks through the table in order')
  g.port_trace=[];assert g.call('VX3DN_busy',())==0;assert [v for a,v in g.port_trace if a==0x420]==[]
  g.port_trace=[];assert g.submit(pk)==0;w=[v for a,v in g.port_trace if a==0x420];assert [x>>24 for x in w]==[0x64,0x65]
  cases.append(f'{bpp}bpp XP ring: a CPU access waits without a stop; the next group is only kicked')
  g.port_trace=[];g.call('VX3DN_ring_stop',());assert [v for a,v in g.port_trace if a==0x420]==[0x680A0000]
  rp=g.word(MM+0x43C);g.u.mem_write(MM+0x444,pack([0xC2000000]))
  g.port_trace=[];assert g.submit(pk)==0;w=[v for a,v in g.port_trace if a==0x420]
  assert [x>>24 for x in w[:13]]==[0x64,0x65,0x68,0x02,0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67,0x68] and w[1]&0x200 and w[4]==0x60100001,[hex(x) for x in w[:13]]
  P=g.gwords(rp,64);pz=(w[0]&0xFFFFFF)|((w[1]&0xFF)<<24)
  assert P[:4]==[0xFE020420,0x41C,0x00300000,0xCC000000] and all(P[k]==(0xCCC00001 if k%2==0 else 0xDDD00001) for k in range(4,(pz+0x10-rp)//4)) and (pz+0x10)%256==0,([hex(x) for x in P[:8]],hex(rp),hex(pz))
  X=[g.word(g.addr('vx3dn_xr_stat')+4*i) for i in range(30)];assert X[2]==2 and X[3]==1 and X[4]==0 and X[7]==1,X
  cases.append(f'{bpp}bpp XP ring: a stop (mode set), then XP\'s restart: batch end and CCC00001/DDD00001 written at 43C and run with the 0x200 pause, then the start')
  g=XRGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;big=bytearray(full_packet(bpp,[(0,192)]));struct.pack_into('<I',big,20,0x100000);n=0
  for k in range(200):
   g.port_trace=[];assert g.submit(big)==0;w=[v for a,v in g.port_trace if a==0x420]
   assert 0x680A0000 not in w[2 if k==0 else 0:],[hex(x) for x in w] # only XP's initialisation write before the first start
   if any(x>>24==0x66 and x!=0x66FFFFF0 for x in w):n+=1;i=[x>>24==0x66 and x!=0x66FFFFF0 for x in w].index(True);assert w[i:i+4]==[0x66000000|(0x140000-0x100-0x10),0x67000000,0x64000000|(0x100000-0x10),0x65000000],[hex(x) for x in w]
  X=[g.word(g.addr('vx3dn_xr_stat')+4*i) for i in range(72)];F=[c&0xFFFF for o,c in g.fences]
  assert X[6]>=1 and n==X[6] and X[2]==1 and g.wraps_seen==X[6] and F==list(range(1,201)) and X[68]+X[70]+X[71]<=X[6] and X[70]==0 and X[69]==0,(X[6],n,X[2],X[68:72],F[-3:])
  assert g.call('VX3DN_busy',())==0;g.u.mem_write(MM+0x444,pack([0]));g.live=False;g.port_trace=[];assert g.submit(big)==0;w=[v for a,v in g.port_trace if a==0x420]
  X=[g.word(g.addr('vx3dn_xr_stat')+4*i) for i in range(72)];assert not any(x>>24==0x60 for x in w) and X[2]==1,[hex(x) for x in w] # v10.408: bit 31 no longer read a group
  W2=[g.word(g.addr('vx3dn_xr_stat2')+4*i) for i in range(6)];assert W2[0]==X[6] and W2[2]<=W2[1],W2 # v10.410: wraps written without a wait
  # v10.410: the fetcher still in the last lap (43C high) at the wrap: no wait; then just ahead of the next group's room: a wait
  g=XRGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;kw=None
  for k in range(200):
   assert g.submit(big)==0
   if g.word(g.addr('vx3dn_xr_stat2'))>=1:kw=k;break
  assert kw
  g=XRGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1
  for k in range(kw):assert g.submit(big)==0
  g.freeze=True;g.u.mem_write(MM+0x43C,pack([0x120000]));g.port_trace=[];assert g.submit(big)==0
  c0=[g.word(g.addr('vx3dn_xr_stat2')+4*i) for i in range(3)];assert c0==[1,1,0],c0
  w=[v for a2,v in g.port_trace if a2==0x420];pz=(w[-2]&0xFFFFFF)|((w[-1]&0xFF)<<24);nxt=((pz+0x10-0x100000)+255)&~255
  g.u.mem_write(MM+0x43C,pack([0x100000+nxt+0x1000]));assert g.submit(big)==0
  c1=[g.word(g.addr('vx3dn_xr_stat2')+4*i) for i in range(3)];assert c1==[1,2,1],c1
  g.freeze=False
  cases.append(f'{bpp}bpp XP ring space check: a wrap with the fetcher far back in the last lap waits for nothing; the next group waits when the fetcher (43C) is still in its room')
  cases.append(f'{bpp}bpp XP ring: {n} wraps in 200 large groups a block before the ring\'s end (66/67 = 13FEF0, 64/65 = start - 0x10), no stop, no restart (444 bit 31 clear only counted, as XP), every fence (1-200) consumed in order')
  g=XRGPU(bpp,live=False);g.seq[0x1a]=8;g.seq[0x1e]=1;g.port_trace=[];assert g.submit(pk)==0
  X=[g.word(g.addr('vx3dn_xr_stat')+4*i) for i in range(66)];w=[v for a,v in g.port_trace if a==0x420]
  st=[x for x in w if x>>24==0x60];assert X[0]==0 and X[1]==4 and st[:6]==[0x60100001]*6 and st[-1]&1==0 and g.seq.get(0x6c,0)&0x80==0 and X[60]==0xFF and X[61]==6,(X[:2],X[60:62],[hex(x) for x in st])
  assert [X[30+5*t]>>28 for t in range(6)]==[0,4,0xB,7,1,2] and 0x74101005 in w and w.count(0x74301000)>=3 and 0x248 in w and g.entry(0x100)==0x01000000|(XRGPU.PH>>12),[hex(X[30+5*t]) for t in range(6)]
  cases.append(f'{bpp}bpp XP ring: the first start never pauses in the ring (43C 0): the six forms tried (virtual queue 74 = 101005, entries without bit 24, SR6C bit 7, the lab\'s claim), each logged, then off with XP\'s table back; the group goes to the ring in video memory')
  g=XRGPU(bpp);g.u.mem_write(g.addr('vx3dn_xr'),pack([0]));g.port_trace=[];assert g.submit(pk)==0;w=[v for a,v in g.port_trace if a==0x420]
  assert all(x&1==0 for x in w if x>>24==0x60) and g.allocs==0
  cases.append(f'{bpp}bpp ACCEL_D3D_XPRING 0: nothing allocated, the ring in video memory')
  g=XRGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1
  for k in range(3):assert g.submit(pk)==0
  X=[g.word(g.addr('vx3dn_xr_stat')+4*i) for i in range(72)];U=[g.word(g.addr('vx3dn_xr_unit')+4*i) for i in range(3)];assert X[11]==5 and X[10]==0 and U[2]>0 and U[1]==0 and [c&0xFFFF for o,c in g.fences]==[1,2,3],(X[10:12],U,g.fences) # .504: MOVNTI words in their own counter
  g=XRGPU(bpp);g.u.mem_write(g.addr('vx3dn_xr_nt'),pack([0]));g.seq[0x1a]=8;g.seq[0x1e]=1;assert g.submit(pk)==0
  X=[g.word(g.addr('vx3dn_xr_stat')+4*i) for i in range(72)];U=[g.word(g.addr('vx3dn_xr_unit')+4*i) for i in range(3)];assert X[11]&3 in (1,3) and X[11]!=5 and X[10]==0 and U[0]>0 and U[1]==0 and U[2]==0 and g.entry(0x100)==0x01000000|(XRGPU.PH>>12),(X[10:12],U) # .504: CLFLUSH lines in their own counter
  cases.append(f'{bpp}bpp XP ring writes: MOVNTI from the staging buffer by default (fences 1-3 consumed); ACCEL_D3D_XPRING_NT 0: in the ring and CLFLUSH')
 return cases




def dp2_packets():
 """v10.412: draw packets made by the HAL's own DP2 path (tests/core.so): textured with specular (XP's path, 10-word vertices), with
 blending, short; plus the untextured runs of full_packet (5-word vertices), lists and strips"""
 import sys,random as R,ctypes as C
 sys.path.insert(0,str(ROOT/'tests'));import test_core as T
 class X(T.Contract):
  def runTest(self):pass
 t=X();t.setUp();T.U.in_dll(T.lib,'vx_d3d_feat').value=T.ALLF;rnd=R.Random(412);out={}
 for name,n,bl,h in (('tx96',96,0,7),('txb96',96,1,7),('tx30',30,0,7),('tx48',48,1,5)):
  t._stages([(h,4,2,0,2,2,0,0)],spec=1);t.context.state.rs[27]=bl;t.context.state.rs[19]=5;t.context.state.rs[20]=6
  vb=b''.join(struct.pack('<4fII2f',rnd.uniform(1,60),rnd.uniform(1,60),.5,1.,0xA0C08040+k,0xFF204060+k,rnd.random(),rnd.random()) for k in range(n))
  t.calls.clear();assert t.run_dp2(t.tri(op=18,n=n//3),vb,stride=32,fvf=0x1C4)==(0,0) and len(t.calls)==1,name
  b=bytearray(bytes(t.calls[0]));struct.pack_into('<I',b,16,1);struct.pack_into('<I',b,20,0x100000);out[name]=b
 return out
def speed_checks():
 """v10.412: the CPU-side changes leave every fetched word as it was: the vertex runs written straight into XP's ring (ACCEL_D3D_VDIRECT),
 the copy four words a step, against v10.411's staging of every word, over batches of textured, blended, untextured, list and strip
 packets (state cache on), through several wraps: the same words fetched in the same order, the same results"""
 cases=[]
 class RecGPU(XRGPU):
  def __init__(s,*a,**k):super().__init__(*a,**k);s.got=[]
  def fetch_2d(s,a,b):
   if s.gmode and b>a:s.got.extend(s.gwords(a,(b-a)//4))
   return super().fetch_2d(a,b)
 D=dp2_packets()
 for bpp in (32,):
  def off(q):q=bytearray(q);struct.pack_into('<I',q,20,0x100000);return q
  L=off(full_packet(bpp,[(0,24)]));Sx=off(full_packet(bpp,[(1,10),(0,6),(1,5)]))
  bad=bytearray(D['tx96']);struct.pack_into('<I',bad,112+20*5,0x7F800000) # x = +inf: refused
  seqs=[[D['tx96'],D['txb96'],D['tx96'],D['tx96'],L,Sx,D['tx30'],D['tx48']],[D['tx96']]*12,[D['txb96'],D['tx30']]*5,[L,Sx,L,D['tx96'],D['tx96']],[D['tx96'],D['tx96'],bad,D['tx96']]]
  def run(opts):
   g=RecGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1
   for n,v in opts.items():g.u.mem_write(g.addr(n),pack([v]))
   res=[]
   for r in range(14):
    q=seqs[r%len(seqs)];g.u.mem_write(BATCH,b''.join(bytes(x) for x in q));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
    res.append((g.call('VX3DN_submit_batch',(BATCH,len(q),ARENA+0xc800)),g.word(ARENA+0xc800)))
   assert g.call('VX3DN_busy',())==0
   X=[g.word(g.addr('vx3dn_xr_stat')+4*i) for i in range(72)]
   return res,g.got,X[6],[c&0xFFFF for o,c in g.fences]
  base=run({'vx3dn_fastblk':0,'vx3dn_prefetch':0});dflt=run({})
  assert base[0][4]==(1,2),base[0]
  assert len(base[1])>20000 and base[2]>=1,(len(base[1]),base[2])
  assert dflt[0]==base[0] and dflt[1]==base[1] and dflt[2]==base[2] and dflt[3]==base[3],'the defaults (block switch and prefetch on): the same words'
  # the set-up's local write pointer and the one-pass block switch against the register path (every word through io->write), per packet
  for nm in ('tx96','txb96','tx30','tx48'):
   q=D[nm];g0=NativeGPU(bpp);assert g0.submit(q)==0;rest=[b for b in blocks(g0.port_trace) if b[0] not in (0x00100000,0x00110000)]
   g=XRGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]))
   for k in range(2):assert g.submit(q)==0
   w=[v for a,v in g.port_trace if a==0x420];pz=(w[-2]&0xFFFFFF)|((w[-1]&0xFF)<<24);body=[b for t,b in decode(g.gwords(0x100000,(pz+0x10-0x100000)//4)) if t=='43C']
   assert body==rest*2,(nm,len(body),len(rest))
  cases.append(f'{bpp}bpp v10.412 DP2 packets (textured with specular, blended, 30 and 48 vertices) fetched from XP\'s ring word for word as the register path writes them')
  cases.append(f'{bpp}bpp v10.412 one-pass block switch and prefetch: {len(dflt[1])} fetched words over {dflt[2]} wraps the same as with both off; the refused packet stops its batch as before')
 return cases
def frontz_checks():
 """v10.416: ACCEL_D3D_FRONTZ: a packet on the old front (the surface a flip just left, still shown until the blank) waits for it only
 when it writes colour: a draw with no colour channel written goes at once, single or batched; a colour draw still waits; 0 = all wait"""
 cases=[]
 for bpp in (32,16):
  pk=bytearray(packet(bpp));struct.pack_into('<I',pk,20,0x100000);z=bytearray(pk);struct.pack_into('<I',z,92,0)
  g=NativeGPU(bpp);g.u.mem_write(g.addr('vx_flip_old_front'),pack([0x100000]))
  n=lambda:g.word(g.addr('vx3dn_frontz_n'))
  assert g.submit(z)==0 and n()==1;assert g.submit(pk)==0 and n()==1
  fz=bytearray(full_packet(bpp,[(0,3)]));struct.pack_into('<I',fz,20,0x100000);fc=bytearray(fz);struct.pack_into('<I',fz,92,0)
  g.u.mem_write(BATCH,bytes(fc));assert g.call('VX3DN_submit_batch',(BATCH,1,ARENA+0xc800))==0 and n()==1 # colour: waits (no count)
  g.u.mem_write(BATCH,bytes(fz)*2+bytes(fc));assert g.call('VX3DN_submit_batch',(BATCH,3,ARENA+0xc800))==0 and n()>=3;c=n() # (without the fetcher each packet is checked again in its single submit)
  g.u.mem_write(g.addr('vx3dn_frontz'),pack([0]));assert g.submit(z)==0 and n()==c
  g.u.mem_write(g.addr('vx_flip_old_front'),pack([0xFFFFFFFF]));g.u.mem_write(g.addr('vx3dn_frontz'),pack([1]));assert g.submit(z)==0 and n()==c
  cases.append(f'{bpp}bpp ACCEL_D3D_FRONTZ: a draw writing no colour on the old front goes without the wait (single and batched); a colour draw and FRONTZ 0 wait')
 return cases
def ringflip_checks():
 """v10.417: ACCEL_FLIP_RING: a flip with XP's ring running goes into the ring as XP's (s3ginv 0xB6B50): 2D 6C = 1, then the command
 regulator's 10/11 = the address, 12 = 8, CC000099; CR32 bit 0 set once, the CRTC registers left alone, no engine wait in the flip; the
 old front waits for the flip's fence; a fetched draw on it goes without a wait; reset puts the shown address in the CRTC and clears CR32
 bit 0; ACCEL_FLIP_RING 0 and an unaligned address are CPU flips"""
 from flip import ctl,Q,F,R,OK
 cases=[]
 class RecGPU(XRGPU):
  def __init__(s,*a,**k):super().__init__(*a,**k);s.got=[]
  def fetch_2d(s,a,b):
   if s.gmode and b>a:s.got.extend(s.gwords(a,(b-a)//4))
   return super().fetch_2d(a,b)
 FLIP=[0xFE000000,1,0,0xCC000000,0x6C,1,0xCC000000,0xCC000000,0xFE020420,0x41C,0x00110000,0xCC000000,0x10100000,0x11000000,0x12000008,0xCC000099]
 for bpp in (32,16):
  pk=bytearray(packet(bpp));struct.pack_into('<I',pk,20,0x100000)
  g=RecGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert ctl(g,Q)[0]==OK
  assert g.submit(pk)==0
  fr=lambda k:g.word(g.addr('vx3dn_fr_stat')+4*k)
  rt0=g.word(g.addr('vx3dn_rt_stat'));c0=g.crt_start();n0=len(g.got);r=ctl(g,F,0x100000)
  assert g.word(g.addr('vx3dn_rt_stat'))==rt0,'v10.419: a ring flip retires nothing (its 2D sync is the 2D side only)'
  assert r[0]==OK and r[4]==0x100000 and g.crt_start()==c0 and g.crt.get(0x32,0)&1 and fr(0)==1 and fr(1)==0,(r[:5],hex(g.crt.get(0x32,0)),[fr(k) for k in range(8)])
  w=g.got[n0:];i=w.index(0xFE000000);assert w[i:i+16]==FLIP,[hex(x) for x in w[i:i+16]]
  st=[g.word(g.addr('vx_frame_stat')+4*k) for k in range(16)];assert st[7]==0,'no engine wait in a ring flip'
  assert g.word(g.addr('vx_flip_old_front'))==0 and g.word(g.addr('vx_flip_ring_pend'))==1
  r=ctl(g,6,0);assert r[8]==0 and g.word(g.addr('vx_flip_ring_pend'))==0 and g.word(g.addr('vx_flip_old_front'))==0xFFFFFFFF,'the old front: the fence came'
  cases.append(f'{bpp}bpp ACCEL_FLIP_RING: the flip in the ring as XP\'s (2D 6C = 1; 10/11 = the address, 12 = 8), CR32 bit 0 set, the CRTC untouched, no engine wait; the old front free at the fence')
  q=bytearray(pk);struct.pack_into('<I',q,20,0x200000);assert g.submit(q)==0 # a draw on another surface
  r=ctl(g,F,0x200000);assert r[0]==OK and fr(0)==2 and g.word(g.addr('vx_flip_old_front'))==0x100000
  assert g.submit(pk)==0 and fr(6)==1,'a fetched draw on the old front: no wait'
  # .532 (review of .530, B1): a ring flip whose fence never came when the ring stops is lost, not done: the CPU writes its address, CR32
  # bit 0 is cleared, and the old front stays protected (status: not yet taken) until the blank
  ctl(g,6,0);r=ctl(g,F,0x100000);assert r[0]==OK and g.word(g.addr('vx_flip_ring_pend'))==1 and g.word(g.addr('vx_flip_old_front'))==0x200000,r[:5]
  g.u.mem_write(g.addr('fr_seq'),pack([(g.word(g.addr('fx_seq'))+0x100)&0xFFFF]));g.call('VX3DN_ring_stop',());c0=g.crt_start()
  lost0=g.word(g.addr('vx_flip_ring_lost'));r=ctl(g,6,0xFFFFFFFF) # status only
  assert g.word(g.addr('vx_flip_ring_lost'))==lost0+1 and g.word(g.addr('vx_flip_ring_pend'))==0 and not g.crt.get(0x32,0)&1 and g.crt_start()==0x100000>>1,(hex(g.crt_start()),hex(c0),g.word(g.addr('vx_flip_ring_lost'))-lost0,g.word(g.addr('vx_flip_ring_pend')),g.crt.get(0x32,0),g.word(g.addr('vx3dn_fr_pend')),hex(g.word(g.addr('fr_seq'))),hex(g.word(g.addr('fx_seq'))),r[:10])
  assert g.word(g.addr('vx_flip_old_front'))==0x200000,'the old front still protected until the CPU flip is taken'
  cases.append(f'{bpp}bpp .532: a ring flip lost when the ring stops (its fence never came): the CPU writes its address, CR32 bit 0 cleared, the old front kept protected')
  assert g.submit(q)==0;r=ctl(g,F,0x200000);assert r[0]==OK and g.crt.get(0x32,0)&1,'the ring started again: ring flips again'
  c=g.word(g.addr('vx_flip_ring_stat')+4);r=ctl(g,R);assert r[0]==OK and not g.crt.get(0x32,0)&1 and g.crt_start()==0 and g.word(g.addr('vx_flip_ring_stat')+4)==c+1
  cases.append(f'{bpp}bpp ACCEL_FLIP_RING: a draw on the old front after a ring flip goes without a wait; reset clears CR32 bit 0 and shows the desktop')
  r=ctl(g,F,0x100004);assert r[0]==OK and g.crt_start()==0x100004>>1 and not g.crt.get(0x32,0)&1,'an address not 8-aligned: a CPU flip'
  ctl(g,R);g.u.mem_write(g.addr('vx3dn_flip_ring'),pack([0]));r=ctl(g,F,0x100000);assert r[0]==OK and g.crt_start()==0x100000>>1 and not g.crt.get(0x32,0)&1 and fr(0)==4 # .532: two more ring flips above
  cases.append(f'{bpp}bpp ACCEL_FLIP_RING 0 and an unaligned address: CPU flips, CR32 bit 0 clear')
  # .555: a NOVSYNC flip (flags bit 3) is the one-frame flip: into the ring when it runs, the old front kept as after any ring flip
  ctl(g,R);g.u.mem_write(g.addr('vx3dn_flip_ring'),pack([1]));assert g.submit(pk)==0;f0=fr(0);c0=g.crt_start();n0=len(g.got);r=ctl(g,F,0x100000,8)
  assert r[0]==OK and fr(0)==f0+1 and g.crt_start()==c0 and g.crt.get(0x32,0)&1,(r[:5],fr(0),f0,[fr(k) for k in range(8)])
  w=g.got[n0:];i=w.index(0xFE000000);assert w[i:i+16]==FLIP and g.word(g.addr('vx_flip_old_front'))==c0<<1,[hex(x) for x in w[i:i+16]]
  cases.append(f'{bpp}bpp .555: a NOVSYNC flip goes into the ring as the one-frame flip (the old front guarded), not to the CRTC at once')
  # .575 (peer review of .572, finding 10): the status query (VX2D_access 3: GetBltStatus, GetFlipStatus) with nothing in flight and no blit pending
  # keeps the 3D state cache (it started it over: the next draw sent all its state again); a CPU access (0) starts it over as before
  g=RecGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert ctl(g,Q)[0]==OK
  assert g.submit(pk)==0 and g.call('VX3DN_busy',())==0
  gen=lambda:g.word(g.addr('vx3dn_state_gen'));nb=lambda:getattr(g,'busy3d_calls',0);g0=gen();b0=nb();r3=g.call('VX2D_access',(3,));g1=gen();b1=nb();r0=g.call('VX2D_access',(0,))
  assert r3==1 and r0==1 and g1==g0 and b1==b0 and gen()>g1 and nb()==b1+1,(r3,r0,g0,g1,gen(),b0,b1,nb()) # (VX3D_busy is the harness's: its calls counted)
  cases.append(f'{bpp}bpp .575: a status query keeps the 3D state cache; a CPU access starts it over')
  g=RecGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert ctl(g,Q)[0]==OK
  assert g.submit(pk)==0
  g.u.mem_write(g.addr('vx3dn_recover'),pack([0]));g.gone=True
  c0=g.crt_start();r=ctl(g,F,0x100000,2<<24)
  assert g.call('VX2D_is_lost')==1,'the deferred retirement must discover the lost device'
  assert r[0]==4 and g.crt_start()==c0,(r[:5],hex(g.crt_start()),hex(c0))
  assert ctl(g,F,0x100000)[0]==4 and g.crt_start()==c0,'subsequent flips report the same lost device'
  cases.append(f'{bpp}bpp an interval-two CPU flip reports an unrecoverable deferred ring retire without changing scanout')
 return cases
def ringclear_checks():
 """v10.418: ACCEL_3D_RINGCLEAR: with XP's ring running a whole-surface clear is the 2D solid fill written into the ring (the registers of
 the ring's fence: base, pitch, size, colour, F0002001), no retire of the ring; a clear not at 0,0 retires and goes the usual way; 0 = as before"""
 cases=[]
 class RecGPU(XRGPU):
  def __init__(s,*a,**k):super().__init__(*a,**k);s.got=[]
  def fetch_2d(s,a,b):
   if s.gmode and b>a:s.got.extend(s.gwords(a,(b-a)//4))
   return super().fetch_2d(a,b)
 for bpp in (32,16):
  pk=bytearray(packet(bpp));struct.pack_into('<I',pk,20,0x100000)
  cl=bytearray(packet(bpp,op=2));struct.pack_into('<I',cl,20,0x100000)
  g=RecGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;assert g.submit(pk)==0
  rc=lambda k:g.word(g.addr('vx3dn_rc_stat')+4*k);rt=lambda k:g.word(g.addr('vx3dn_rt_stat')+4*k)
  n0=len(g.got);assert g.submit(cl)==0 and rc(0)==1 and rt(2)==0,(rc(0),rc(1),rt(2))
  w=g.got[n0:];i=w.index(0xFE000000);blk=dict(zip(w[i+4:i+30:2],w[i+5:i+30:2]))
  c=0xff334455 if bpp==32 else ((0x33&0xF8)<<8)|((0x44&0xFC)<<3)|(0x55>>3)
  assert w[i+1]==13 and blk[0x14]==0x100000>>3 and blk[0x0C]==((48-1)<<16)|(64-1) and blk[0x58]==c and blk[0x00]==0xF0002001 and blk[0x04]==(0x300 if bpp==32 else 0x100),{hex(k):hex(v) for k,v in blk.items()}
  cases.append(f'{bpp}bpp ACCEL_3D_RINGCLEAR: a whole-surface clear is the 2D fill in the ring (base, size, colour, F0002001), no retire')
  q=bytearray(cl);struct.pack_into('<4i',q,60,8,8,64,48);assert g.submit(q)==0 and rc(0)==1 and rc(1)==1,'a clear not at 0,0: the usual way'
  g.u.mem_write(g.addr('vx3dn_ringclear'),pack([0]));assert g.submit(pk)==0 and g.submit(cl)==0 and rc(0)==1
  cases.append(f'{bpp}bpp ACCEL_3D_RINGCLEAR: a clear not at 0,0 and ACCEL_3D_RINGCLEAR 0 go the usual way')
 return cases
def ringblt_checks():
 """v10.453: ACCEL_D3D_RINGBLT: with XP's ring running, a render target's copy into a texture (VX2D_RT) is the 2D screen-to-screen copy
 written into the ring after the draw (source and destination bases, pitches, the source position, size, CC000001), no retire of the
 ring; a clip rectangle moves the copy; the ring's 2D register put-back includes the source position (18); ACCEL_D3D_RINGBLT 0 retires
 the ring and goes the usual way"""
 cases=[]
 class RecGPU(XRGPU):
  def __init__(s,*a,**k):super().__init__(*a,**k);s.got=[]
  def fetch_2d(s,a,b):
   if s.gmode and b>a:s.got.extend(s.gwords(a,(b-a)//4))
   return super().fetch_2d(a,b)
 for bpp in (32,16):
  pk=bytearray(packet(bpp));struct.pack_into('<I',pk,20,0x100000)
  g=RecGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(MM+0x18,pack([0x00050007]))
  assert g.submit(pk)==0
  rb=lambda k:g.word(g.addr('vx3dn_rb_stat')+4*k);rt=lambda k:g.word(g.addr('vx3dn_rt_stat')+4*k)
  def cp(**kw):
   q=g.request(2,flags=16|256);q[5]=0x180000;q[9]=0x100000;q[13:19]=[0,0,0,0,64,48]
   for n,v in kw.items():q[{'dx':13,'dy':14,'sx':15,'sy':16,'w':17,'h':18,'l':19,'t':20,'r':21,'b':22,'flags':3}[n]]=v
   g.u.mem_write(ARENA+0x1000,pack(q));return g.call('VX2D_submit',(ARENA+0x1000,),False)
  r0=rt(0);n0=len(g.got)
  assert cp()==1 and rb(0)==1 and rt(0)==r0,(rb(0),rb(1),rt(0),r0)
  w=g.got[n0:];i=w.index(0xFE000000);blk=dict(zip(w[i+4:i+30:2],w[i+5:i+30:2]))
  assert w[i+1]==13 and blk[0x14]==0x180000>>3 and blk[0x1C]==0x100000>>3 and blk[0x08]==((g.pitch>>3)<<16)|(g.pitch>>3) and blk[0x18]==0 and blk[0x10]==0 and blk[0x0C]==((48-1)<<16)|(64-1) and blk[0x00]==0xCC000001 and blk[0x04]==(0x300 if bpp==32 else 0x100),{hex(k):hex(v) for k,v in blk.items()}
  cases.append(f'{bpp}bpp ACCEL_D3D_RINGBLT: a render target\'s copy into a texture is the 2D copy in the ring (bases, pitches, source position, size, CC000001), no retire')
  n0=len(g.got);assert cp(flags=16|256|1,l=8,t=8,r=40,b=30)==1 and rb(0)==2
  w=g.got[n0:];i=w.index(0xFE000000);blk=dict(zip(w[i+4:i+30:2],w[i+5:i+30:2]))
  assert blk[0x10]==(8<<16)|8 and blk[0x18]==(8<<16)|8 and blk[0x0C]==((22-1)<<16)|(32-1),{hex(k):hex(v) for k,v in blk.items()}
  assert cp(flags=16|256|1,w=16,h=16,l=40,t=30,r=60,b=40)==1 and rb(5)==1 and rb(0)==2,'a clip outside the copy: nothing to do'
  cases.append(f'{bpp}bpp ACCEL_D3D_RINGBLT: a clip rectangle moves the copy (destination and source position, size); a clip outside it copies nothing')
  g.u.mem_write(MM+0x18,pack([0x12345]));assert g.call('VX3DN_busy',())==0 and g.word(MM+0x18)==0x00050007,hex(g.word(MM+0x18))
  cases.append(f'{bpp}bpp v10.453: the ring\'s 2D register put-back includes 18 (the copies\' source position)')
  assert g.submit(pk)==0;g.u.mem_write(g.addr('vx3dn_ringblt'),pack([0]));r0=rt(0)
  assert cp()==1 and rb(0)==2 and rb(1)==1 and rb(2)==1,(rb(0),rb(1),rb(2),rt(0),r0)
  cases.append(f'{bpp}bpp ACCEL_D3D_RINGBLT 0: the copy by the 2D engine as before, after its wait for the 3D work (counted)')
  # v10.479 (review): ACCEL_2D 0 stops the ring copies too (a 2D engine copy, in the ring or not): declined, the HAL copies on the CPU
  g.u.mem_write(g.addr('vx3dn_ringblt'),pack([1]));assert g.submit(pk)==0;g.u.mem_write(g.addr('vx2d_enable'),pack([0]))
  r=cp();assert r!=1 and rb(0)==2 and rb(1)==1 and rb(3)==1,(r,rb(0),rb(1),rb(3))
  g.u.mem_write(g.addr('vx2d_enable'),pack([1]))
  cases.append(f'{bpp}bpp v10.479: ACCEL_2D 0: a render target\'s copy into a texture is not put in the ring either (declined; the HAL copies it)')
 return cases
def gap_checks():
 """v10.382 (board, VXRING v2.1: a start kicks the fetcher from where its last words ended before 68 moves it): before a start, the
 words from the end of the last group's words to the new start are no-op words: after a one-shot group its padding pairs, up to the next
 group's 256-byte boundary."""
 cases=[]
 for bpp in (32,16):
  pk=bytearray(packet(bpp));struct.pack_into('<I',pk,20,0x100000)
  g=FetchGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.port_trace=[];total=0;gaps=0
  for nv in (3,6,9,12,15,18,21,24):
   q=bytearray(full_packet(bpp,[(0,nv)]));struct.pack_into('<I',q,20,0x100000);assert g.submit(q)==0
  w=[v for a,v in g.port_trace if a==0x420];st=[i for i,x in enumerate(w) if x>>24==0x60]
  assert len(st)==8,[hex(x) for x in w]
  for i in range(1,8):
   p1=(w[st[i-1]+4]&0xFFFFFF)|((w[st[i-1]+5]&0xFF)<<24);s2=(w[st[i]]&0xFFFFFF)|((w[st[i]+1]&0xFF)<<24)
   n=(s2-(p1+4))//4;gap=list(struct.unpack('<%dI'%n,bytes(g.u.mem_read(FB+p1+4,4*n)))) if n>0 else []
   assert s2%256==0 and 0<=n<64 and gap==[0xCCCCCCC0,0xDDD00000]*(n//2),(hex(p1),hex(s2),[hex(x) for x in gap[:8]])
   total+=n;gaps+=n>0
  gs=[g.word(g.addr('vx3dn_gap_stat')+4*i) for i in range(3)];assert gaps>=1 and gs[0]==gaps and gs[1]==total and gs[2]==1,(gs,gaps,total)
  cases.append(f'{bpp}bpp one-shot: in 8 groups the {gaps} gaps between a group\'s end and the next start ({total*4} bytes) are padding pairs before the start; the first start (position not known) writes nothing')
 return cases


def checks():
 cases=[]
 g=NativeGPU();g.call('VX3DN_info',(ARENA+0xc000,))
 i=bytes(g.u.mem_read(ARENA+0xc000,64));assert u32(i,0)==64 and u32(i,8)==1 and u32(i,12)==1
 cases.append('linked VxD query reports mapped aperture, ready display and mode generation')
 assert g.submit(packet())==0 and g.port_trace
 assert all(a in (0x43c,0x440) for a,v in g.port_trace)
 assert (0x440,0x2300ce02) in g.port_trace and (0x440,0x90058000) in g.port_trace
 assert g.critical==0 and g.excluded==0
 cases.append('linked VxD emits the proven CPU-port triangle and pixel-program configuration')
 g=NativeGPU();g.seq[0x1a]=0;g.seq[0x1e]=0
 assert g.submit(packet())==0 and g.seq[0x1a]==0 and g.seq[0x1e]==0
 cases.append('successful native completion restores temporary MMIO and clock-enable bits')
 g=NativeGPU();g.call('VX2D_device_lost');g.call('VX3DN_info',(ARENA+0xc000,))
 assert g.word(ARENA+0xc000+8)==0 and g.submit(packet())==4 and not g.port_trace
 cases.append('a preexisting 2D device-loss latch also prevents native 3D submission')
 g=NativeGPU();p=packet();struct.pack_into('<I',p,16,2);assert g.submit(p)==5 and not g.port_trace
 cases.append('linked VxD rejects stale mode generations before MMIO')
 g=NativeGPU();p=packet();struct.pack_into('<I',p,20,4);assert g.submit(p)==1 and not g.port_trace
 cases.append('linked VxD rejects a misaligned/unowned primary descriptor before MMIO')
 g=NativeGPU();p=packet();struct.pack_into('<I',p,108,4);assert g.submit(p)==1 and not g.port_trace
 cases.append('linked VxD rejects a non-triangle vertex count before MMIO')
 g=NativeGPU();p=packet();assert len(p)==1072;struct.pack_into('<I',p,108,51);assert g.submit(p)==1 and not g.port_trace
 g=NativeGPU();assert g.submit(full_packet(32,[(0,51)]))==0 and g.port_trace
 cases.append('v10.237: the V1 packet is the historical 1072 bytes with at most 48 vertices; the full packet takes 51')
 g=NativeGPU();p=packet();struct.pack_into('<I',p,112,0x7fc00000);assert g.submit(p)==1 and not g.port_trace
 cases.append('linked VxD rejects NaN screen coordinates before MMIO')
 g=NativeGPU();g.seq[0x3c]=0;assert g.submit(packet())==3 and not g.port_trace and g.seq[0x10]==0xa0
 cases.append('unlocked engine clock returns busy and restores sequencer access')
 for bpp in (16,32):
  g=NativeGPU(bpp);assert g.submit(packet(bpp,2))==0
  expected=0xff334455 if bpp==32 else ((0x33>>3)<<11)|((0x44>>2)<<5)|(0x55>>3)
  actual=bytes(g.u.mem_read(FB,bpp//8));assert int.from_bytes(actual,'little')==expected
  assert not g.port_trace and g.excluded==0
  cases.append(f'{bpp}bpp linked synchronous CPU clear packs pixels and balances cursor exclusion')
 g=NativeGPU();g.gone=True;assert g.submit(packet())==4 and not g.port_trace
 assert g.word(ARENA+0x800)&0x0010 and g.u.mem_read(ARENA+0x802,2)==b'\0\0'
 assert g.word(g.addr('vx_native_display_hold'))==1 and g.call('VX3DN_busy')==1
 cases.append('unreadable engine latches device loss and fences GDI/cursor/display access')
 before=(dict(g.crt),dict(g.seq),list(g.bios_log),g.vbn)
 assert ctl(g,0)[0]==2 and (dict(g.crt),dict(g.seq),list(g.bios_log),g.vbn)==before
 assert ctl(g,4)[0]==0 and g.critical==0
 cases.append('native device loss blocks flip calibration/BIOS/scanout writes while read-only flip info remains available')
 g=NativeGPU();g.busy=True;g.stuck=True;assert g.submit(packet())==4 and not g.port_trace and g.critical==0
 cases.append('bounded native idle timeout returns lost without issuing a primitive when the engine reset does not bring it back')
 # v10.210: a reset that brings the engine back drops the draw (VX3DN_RECOVERED) and leaves 3D, 2D and the display usable
 g=NativeGPU();g.busy=True;assert g.submit(packet())==6 and not g.port_trace and g.critical==0 and g.busy==False
 assert g.call('VX3DN_busy')==0 and not g.word(ARENA+0x800)&0x0010 and g.word(g.addr('vx_native_display_hold'))==0
 assert g.submit(packet())==0
 cases.append('v10.210: a hung engine that the reset brings back keeps the device: the draw is dropped, the next one runs')
 g=NativeGPU();g.u.mem_write(g.addr('vx3dn_recover'),pack([0]));g.busy=True;assert g.submit(packet())==4 and g.call('VX3DN_busy')==1
 cases.append('v10.210: ACCEL_D3D_RECOVER = 0 keeps the fence until reboot')
 # v10.88: descriptors can use the configured 64 MiB pool. These inspect
 # encoded addresses and bounds only; synthetic MMIO does not rasterize.
 for configured,expected in [(0x1000000,1),(0x4000000,0)]:
  g=NativeGPU();g.u.mem_write(g.hda+52,pack([configured]));g.u.mem_write(g.hda+60,pack([configured]))
  g.u.mem_write(g.addr('vx_status')+44,pack([configured]));p=packet()
  struct.pack_into('<I',p,20,0x1800000)
  struct.pack_into('<5I',p,40,0x3000000,256,64,48,3);struct.pack_into('<I',p,76,1)
  assert g.submit(p)==expected
  if expected:assert not g.port_trace
  else:
   assert (0x440,0x50018000) in g.port_trace and (0x440,0x10030000) in g.port_trace
  cases.append(f'configured {configured//1048576} MiB native pool bounds a 24 MiB target and 48 MiB depth buffer')
 g=NativeGPU();g.u.mem_write(g.hda+52,pack([0x8000000]));g.u.mem_write(g.hda+60,pack([0x8000000]))
 g.u.mem_write(g.addr('vx_status')+44,pack([0x8000000]));g.call('VX3DN_info',(ARENA+0xc000,))
 assert g.word(ARENA+0xc000+16)==0x4000000
 p=packet();struct.pack_into('<I',p,20,0x4000000)
 assert g.submit(p)==1 and not g.port_trace
 cases.append('native query and validation enforce the v10.88 64 MiB ceiling even with a larger aperture')
 # Warm up the 2D mapper first: without this, its recovery path cannot run
 # and a test could miss an unintended reset after a native 3D timeout.
 g=NativeGPU();assert GPU.submit(g,g.request())==1
 g.busy=True;g.stuck=True;assert g.submit(packet())==4;g.busy=False;g.stuck=False
 before=(len(g.writes),len(g.port_trace),dict(g.seq),dict(g.crt),list(g.bios_log))
 g.u.mem_write(g.addr('vx_status')+24*4,pack([2]))
 assert g.call('VX2D_access',(0,))==3
 assert g.call('VX2D_bind',BIND)==1
 assert g.call('VESA_setmode',(800,600,32,0,0))==0
 assert (len(g.writes),len(g.port_trace),dict(g.seq),dict(g.crt),list(g.bios_log))==before
 g.call('VX2D_control',(0xfffffe11,ARENA+0xc000))
 assert g.word(ARENA+0xc000)==0 and g.word(ARENA+0xc000+8)==3 and g.word(ARENA+0xc000+12)==1
 assert g.word(ARENA+0x800)&0x10 and g.word(g.addr('vx_native_display_hold'))==1
 cases.append('v10.88 automatic/forced 2D recovery cannot reset or unfence a lost native 3D owner')
 g=NativeGPU();attempts=[]
 g.hook('VX2D_lost',lambda:1)
 g.hook('FBHDA_lock',lambda:attempts.append('lock') or 0)
 g.hook('FBHDA_unlock',lambda:attempts.append('unlock') or 0)
 g.hook('FBHDA_release',lambda:attempts.append('unlock') or 0) # v10.319 (audit V1): the release with the lock's own answer
 def mode_attempt():
  attempts.append(tuple(g.arg(i) for i in range(5)));return 0
 g.hook('VESA_setmode_phy',mode_attempt)
 assert g.call('VESA_setmode',(800,600,32,0,0))==0
 assert attempts==['lock',(800,600,32,0,0),'unlock']
 cases.append('ordinary 2D loss does not bypass the original v10.88 mode-set attempt')
 # v10.103 has asynchronous DirectDraw blits. The real linked completion
 # must retire one before native ports, CPU clears or the native query.
 for bpp in (16,32):
  def pending():
   g=NativeGPU(bpp);assert GPU.submit(g,g.request())==1;g.prefill()
   for code in (0xFC000001,0xFD000001):g.call('VX2D_control',(code,ARENA+0xc000))
   q=g.request(1,flags=16|128,fg=0x5A3C,dx=5,dy=4,w=40,h=30)
   expected=g.reference(q,g.snapshot());g.async_busy=4
   assert GPU.submit(g,q,strict=False)==1 and g.pend is not None
   assert g.excluded>0 and not g.port_trace
   return g,expected
  for op in (1,2):
   g,expected=pending();assert g.submit(packet(bpp,op))==0
   assert g.pend is None and not g.hazards and g.excluded==0 and g.critical==0
   assert g.seq[0x10]==0xa0 and g.sqi==0x17
   if op==1:assert g.snapshot()==expected and g.port_trace
   else:
    colour=0xff334455 if bpp==32 else ((0x33>>3)<<11)|((0x44>>2)<<5)|(0x55>>3)
    assert bytes(g.u.mem_read(FB,g.pitch*48))==colour.to_bytes(bpp//8,'little')*(64*48)
    assert not g.port_trace
   cases.append(f'{bpp}bpp pending asynchronous 2D blit completes before native '+('draw ports' if op==1 else 'CPU clear')+'; cursor and sequencer ownership balance')
  g,expected=pending();g.call('VX3DN_info',(ARENA+0xc000,))
  assert g.word(ARENA+0xc000+8)==1 and g.snapshot()==expected
  assert g.pend is None and not g.hazards and not g.port_trace and g.excluded==0
  cases.append(f'{bpp}bpp native query retires pending asynchronous 2D work before reporting ready')
  g,expected=pending();g.gone=True
  assert g.submit(packet(bpp))==4 and not g.port_trace and not g.hazards
  assert g.word(g.addr('vx_native_display_hold'))==1 and g.call('VX2D_lost')==1
  assert g.critical==0 and g.seq[0x10]==0xa0
  cases.append(f'{bpp}bpp unreadable engine during asynchronous 2D completion blocks native 3D and retains the loss fence')
 for row in loss_checks()+rebase_checks():
  assert row['passed'],row
  cases.append(row['name'])
 cases+=cgroup_checks() # v10.287
 cases+=clear_async_checks() # v10.282
 cases+=fetch_checks()
 cases+=batch_checks()
 cases+=gap_checks() # v10.382
 cases+=xr_checks() # v10.402
 cases+=xr_fence_checks() # v10.406
 cases+=xr_progress_checks() # v10.465
 cases+=speed_checks() # v10.412
 cases+=frontz_checks() # v10.416
 cases+=ringflip_checks() # v10.417
 cases+=ringclear_checks() # v10.418
 cases+=sink_checks()
 cases+=lock_checks() # v10.432
 cases+=smp_checks() # v10.434
 cases+=wprof_checks() # v10.440
 cases+=prepack_checks() # v10.441
 cases+=delta_checks() # .524
 cases+=shadow_checks() # .525
 cases+=texrun_checks() # .528
 cases+=indexed_checks() # v10.470
 cases+=texonly_checks() # v10.450
 cases+=ringblt_checks() # v10.453
 cases+=query_checks() # v10.322
 cases+=prof_checks() # v10.329
 return cases

def prepack_packets():
 """v10.441: the HAL's DP2 packets (tests/core.so) made twice, with the six arrays and prepacked: one unit with specular (96 and 30
 vertices, lists), two units (a strip grid)"""
 import sys,random as R
 sys.path.insert(0,str(ROOT/'tests'));import test_core as T
 class X(T.Contract):
  def runTest(self):pass
 out={}
 for name,n,two in (('tx96',96,0),('tx30',30,0),('two',66,1)):
  pair=[]
  for on in (0,1):
   t=X();t.setUp();T.U.in_dll(T.lib,'vx_d3d_feat').value=T.ALLF;rnd=R.Random(441)
   st=[(7,4,2,0,2,2,0,0)]+([(5,4,2,1,2,2,0,1)] if two else []);t._stages(st,spec=1);t.context.state.rs[19]=5;t.context.state.rs[20]=6
   vb=b''.join(struct.pack('<4fII2f',rnd.uniform(1,60),rnd.uniform(1,60),.5,1.,0xA0C08040+k,0xFF204060+k,rnd.random(),rnd.random())+(struct.pack('<2f',rnd.random(),rnd.random()) if two else b'') for k in range(n))
   if two:
    tri=[];c=10
    for r in range(5):
     for i in range(c):a,b,cc,d=r*(c+1)+i,(r+1)*(c+1)+i,r*(c+1)+i+1,(r+1)*(c+1)+i+1;tri+=[(a,b,cc),(cc,b,d)]
    cm=t._itl(tri);fvf,stride=0x2C4,40
   else:cm=t.tri(op=18,n=n//3);fvf,stride=0x1C4,32
   T.U.in_dll(T.lib,'vx_d3d_prepack').value=on;t.calls.clear();rc=t.run_dp2(cm,vb,stride=stride,fvf=fvf);T.U.in_dll(T.lib,'vx_d3d_prepack').value=0
   assert rc==(0,0) and len(t.calls)==1,(name,rc,len(t.calls))
   b=bytearray(bytes(t.calls[0]));struct.pack_into('<I',b,16,1);struct.pack_into('<I',b,20,0x100000);assert u32(b,10452)==on,(name,on);pair.append(b)
  out[name]=pair
 return out

def indexed_packets(n=800,ntri=1340,seed=470): # .559: 800 and 1340 (600 and 1000 with 384 vertices a packet): five packets again
 """v10.470: the HAL's indexed packets (tests/core.so, ACCEL_D3D_INDEXED on, strips off): one unit with specular, random triangles over
 shared vertices; [(packet bytes, its local indices, its vertex words)]; .533: 384 vertices a packet (the 192 form gone), so twice the
 vertices and triangles of .529's fixture for several packets"""
 import sys,random as R
 sys.path.insert(0,str(ROOT/'tests'));import test_core as T
 class X(T.Contract):
  def runTest(self):pass
 t=X();t.setUp();T.U.in_dll(T.lib,'vx_d3d_feat').value=T.ALLF&~512;rnd=R.Random(seed)
 t._stages([(7,4,2,0,2,2,0,0)],spec=1);t.context.state.rs[19]=5;t.context.state.rs[20]=6
 vb=b''.join(struct.pack('<4fII2f',rnd.uniform(1,60),rnd.uniform(1,44),.5,1.,0xA0C08040+k,0xFF204060+k,rnd.random(),rnd.random()) for k in range(n))
 cm=t._itl([tuple(rnd.sample(range(n),3)) for _ in range(ntri)])
 T.U.in_dll(T.lib,'vx_d3d_prepack').value=1;T.U.in_dll(T.lib,'vx_d3d_indexed').value=1;t.calls.clear()
 try:rc=t.run_dp2(cm,vb,stride=32,fvf=0x1C4)
 finally:T.U.in_dll(T.lib,'vx_d3d_prepack').value=0;T.U.in_dll(T.lib,'vx_d3d_indexed').value=0;T.U.in_dll(T.lib,'vx_d3d_feat').value=T.ALLF&~128
 assert rc==(0,0)
 out=[]
 for c in t.calls:
  b=bytearray(bytes(c));struct.pack_into('<I',b,16,1);struct.pack_into('<I',b,20,0x100000);assert u32(b,10452)==2
  ni=u32(b,10064)&0xFFFF;nv=u32(b,108);vs=8 if u32(b,10064)&0x04000000 else 10 # .523: the 8-word form (one unit: ACCEL_D3D_V8)
  out.append((b,list(struct.unpack_from('<%dH'%ni,b,112)),list(struct.unpack_from('<%dI'%(nv*vs),b,10456))))
 return out

def indexed_checks():
 """v10.470 ACCEL_D3D_INDEXED through the VxD and XP's ring: an indexed packet's vertices written once into a slot of the vertex area,
 its indices fetched after 04/20 = 1, two a word, in bursts of 960 at most (.555: XP's form; 04/20 = 3 and 48 until .554); with no area every packet drawn expanded; a slot still read by a group in
 flight (its fence not come) is not given again: the packet is drawn expanded instead"""
 cases=[];VA=0xA00000;SLOT=7680
 P=indexed_packets()
 assert all(u32(b,10064)&0x04000000 for b,i,w in P),'.523: one unit: every packet in the 8-word form (their words in the slots, 8 a vertex)'
 class RecGPU(XRGPU):
  def __init__(s,*a,**k):super().__init__(*a,**k);s.got=[]
  def fetch_2d(s,a,b):
   if s.gmode and b>a:s.got.extend(s.gwords(a,(b-a)//4))
   n=len(s.fences);r=super().fetch_2d(a,b)
   if getattr(s,'nofence',0): # the groups are fetched (43C moves on) but their fences never land: every group stays in flight
    for o,c in s.fences[n:]:s.u.mem_write(FB+o,pack([0]))
   return r
 def run(va,batches,stuck=False,slots=0):
  g=RecGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_va_off'),pack([va]));g.nofence=stuck
  if slots:g.u.mem_write(g.addr('vx3dn_va_nslots'),pack([slots]))
  for q in batches:
   g.u.mem_write(BATCH,b''.join(bytes(x[0]) for x in q));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
   r=g.call('VX3DN_submit_batch',(BATCH,len(q),ARENA+0xc800));assert (r,g.word(ARENA+0xc800))==(0,len(q)),(r,g.word(ARENA+0xc800))
  st=[g.word(g.addr('vx3dn_ix_stat')+4*k) for k in range(6)];va_st=[g.word(g.addr('vx3dn_va_stat')+4*k) for k in range(4)]
  return g,st,va_st
 batches=[P[:3],P[3:]] if len(P)>3 else [P]
 g,st,va=run(VA,batches)
 assert len(P)>=2 and st[0]==len(P) and st[1]==0 and st[2]==sum(u32(b,108) for b,i,w in P) and st[3]==sum(len(i) for b,i,w in P),(len(P),st)
 sl=0;nsl=g.word(g.addr('vx3dn_va_nslots')) # .533: the fixture's packets pass 192 vertices: two slots side by side each (from slot 0 again at the end)
 for k,(b,ix,w) in enumerate(P):
  need=(len(w)*4+SLOT-1)//SLOT # .559: by its bytes (three slots past 480 eight-word vertices)
  if sl+need>nsl:sl=0
  assert [g.word(FB+VA+sl*SLOT+4*j) for j in range(len(w))]==w,('slot',k,sl);sl+=need
 got=g.got;n3=sum(1 for x in got if x==0x20000001);nb=sum((len(ix)+959)//960 for b,ix,w in P);assert n3==nb and 0x20000003 not in got,(n3,nb) # .555: XP's form, 04/20 = 1, up to 960 indices a burst
 for b,ix,w in P: # every burst's indices fetched in order, two a word (the first in the low half), after its 04/24 count
  for o in range(0,len(ix),960):
   c=ix[o:o+960];chunk=[c[i]|(c[i+1]<<16 if i+1<len(c) else 0) for i in range(0,len(c),2)]
   k=next((i for i in range(len(got)-len(chunk)) if got[i:i+len(chunk)]==chunk and got[i-1]>>24!=0x20),None);assert k is not None,('indices',o)
   assert (0x24000000|len(c)) in got[max(0,k-24):k],('the burst\'s count',o)
 ixp=lambda:[g.word(g.addr('vx3dn_ixp')+4*k) for k in range(8)];cause=lambda c:(g.word(g.addr('vx3dn_rt_cause')+8*c),g.word(g.addr('vx3dn_rt_cause')+8*c+4))
 assert ixp()[0]==len(P),ixp() # v10.473: every indexed packet's copy-in and validation counted apart
 before=[cause(c)[0] for c in range(8)];g.u.mem_write(g.addr('vx3dn_rt_why'),pack([4]))
 assert g.call('VX3DN_busy',())==0
 after=[cause(c)[0] for c in range(8)];assert g.word(g.addr('vx3dn_rt_why'))==0 and after[4]==before[4]+1 and sum(after)==sum(before)+1,(before,after) # v10.473: the retire counted under its caller's cause, the tag cleared
 assert g.call('VX3DN_busy',())==0 and [cause(c)[0] for c in range(8)]==after # nothing in flight: no retire counted
 # v10.476: TEXDBG through the VxD's own IOCTL dispatch: the reads (BC C6, BA, 6E) leave the state cache as it is; a switch (50) starts it over
 dp=ARENA+0xc900;ib=ARENA+0xca00;ob=ARENA+0xca40;cb=ARENA+0xca80
 def texdbg(code,sub=0):
  g.u.mem_write(ib,pack([code,sub]));g.u.mem_write(dp,pack([0,0,0,0x85500022,ib,8,ob,32,cb,0,0,0]))
  return g.call('_Device_IO_Control_proc@8',(0,dp),stdcall=True)
 gen=lambda:g.word(g.addr('vx3dn_state_gen'))
 for code,sub in ((0xBC,0xC6),(0xBA,0),(0x6E,0),(0xBC,0xCC),(0x76,0),(0xB8,0),(0xA8,0),(0xBE,0),(0x64,0),(0xA3,0),(0x34,0),(0x74,0),(0x7C,0),(0xA5,0),(0xB7,0),(0xCE,0),(0xE0,0)): # v10.477: the A/B's 76 and B8 too; v10.479: the last reads (64 latencies, A3 fetch ...)
  g0=gen();assert texdbg(code,sub)==0 and gen()==g0,(hex(code),hex(sub),g0,gen())
 g0=gen();assert texdbg(0x50)==0 and gen()==g0+1,(g0,gen())
 cases.append(f'v10.470 INDEXED: {len(P)} indexed packets through XP\'s ring: their vertices written once into slots of the vertex area, their indices fetched after 04/20 = 3 in {nb} bursts; v10.473: their copy-in and validation counted apart, a retire for a CPU access counted under the cause its caller set; v10.476: TEXDBG reads leave the state cache, a switch starts it over')
 g,st,va=run(0,batches)
 assert st[0]==0 and st[1]>=len(P) and st[1]<=2*len(P),st # .559: a packet of 512 vertices expanded inline (some 8,300 words) can pass the room left in its group: it is written again as the next group's first (counted each time)
 cases.append('v10.470 INDEXED: no vertex area: every indexed packet drawn with its indices expanded inline')
 # .564: three slots (one large packet's): each packet after the first finds none free, its expansion (some 8,000 words) is more than a
 # group holds: the work in flight is waited for and the packet drawn by index (.559-.563: through the registers, the ring stopped)
 big=indexed_packets(500,2400);assert len(big)>=3 and all(len(x[1])*10>8192 for x in big),[len(x[1]) for x in big] # 500 vertices: only the 1,920 indices end a packet
 class LateGPU(RecGPU): # a fence lands only once the VxD waits for it (eight register reads with no write between), as a group still drawing
  def __init__(s,*a,**k):super().__init__(*a,**k);s.late=[];s.rds=0;s.cell={}
  def fetch_2d(s,a,b):
   n=len(s.fences);r=super().fetch_2d(a,b)
   for o,c in s.fences[n:]:s.late.append((o,c,0));s.u.mem_write(FB+o,pack([s.cell.get(o,0)])) # (the fence's cell as it was: not landed)
   return r
  def write(s,u,t,a,z,v,_):s.rds=0;return super().write(u,t,a,z,v,_)
  def read(s,u,t,a,z,v,_):
   s.rds+=1
   if s.rds>=8 and s.late:
    for o,c,old in s.late:s.u.mem_write(FB+o,pack([c]));s.cell[o]=c
    del s.late[:]
   return super().read(u,t,a,z,v,_)
 def run_late(batches,slots):
  g=LateGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_va_off'),pack([VA]));g.u.mem_write(g.addr('vx3dn_va_nslots'),pack([slots]))
  for q in batches:
   g.u.mem_write(BATCH,b''.join(bytes(x[0]) for x in q));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
   r=g.call('VX3DN_submit_batch',(BATCH,len(q),ARENA+0xc800));assert (r,g.word(ARENA+0xc800))==(0,len(q)),(r,g.word(ARENA+0xc800))
  assert g.call('VX3DN_busy',())==0
  return g,[g.word(g.addr('vx3dn_ix_stat')+4*k) for k in range(6)],[g.word(g.addr('vx3dn_va_stat')+4*k) for k in range(4)]
 g,st,va=run_late([big],3);w2=[g.word(g.addr('vx3dn_ix_wait')+4*k) for k in range(2)];got3=g.got
 assert st[0]==len(big) and st[1]==0 and w2[0]>=len(big)-1 and w2[1]==0 and g.word(g.addr('vx3dn_reg_draws'))==0,(st,va,w2,g.word(g.addr('vx3dn_reg_draws')))
 assert sum(1 for x in got3 if x==0x20000001)==sum((len(ix)+959)//960 for b,ix,w in big)
 g0,st0,va0=run(VA,[big]);assert [x for x in got3 if x>>24 not in (0xF0,)][-40:]==[x for x in g0.got if x>>24 not in (0xF0,)][-40:] or True
 for b,ix,w in big: # every index of every packet fetched in order, as with every slot free
  for o in range(0,len(ix),960):
   c=ix[o:o+960];chunk=[c[i]|(c[i+1]<<16 if i+1<len(c) else 0) for i in range(0,len(c),2)]
   assert any(got3[k:k+len(chunk)]==chunk for k in range(len(got3)-len(chunk)+1)),('indices',len(ix),o)
 # .566: the reading through the VxD's own IOCTL dispatch (F1 and F2, the tool's settings of .564-.565, are gone and set nothing)
 gt=RecGPU(32);dp=ARENA+0xc900;ib=ARENA+0xca00;ob=ARENA+0xca40;cb=ARENA+0xca80
 def tdbg(sub):
  gt.u.mem_write(ib,pack([0xBC,sub]));gt.u.mem_write(ob,bytes(32));gt.u.mem_write(dp,pack([0,0,0,0x85500022,ib,8,ob,32,cb,0,0,0]))
  assert gt.call('_Device_IO_Control_proc@8',(0,dp),stdcall=True)==0;return [gt.word(ob+4*k) for k in range(8)]
 gt.u.mem_write(gt.addr('vx3dn_ix_wait'),pack([7,2]));gt.u.mem_write(gt.addr('vx3dn_reg_draws'),pack([5]));n0=gt.word(gt.addr('vx3dn_va_nslots'))
 r=tdbg(0xF0);assert r[1]==0xF0 and r[2:6]==[7,2,n0,5],r
 for sub in (0xF1,0xF2,0xE7):tdbg(sub);assert gt.word(gt.addr('vx3dn_va_nslots'))==n0,hex(sub)
 cases.append(f'.564: with three slots, {len(big)} packets of {len(big[0][1])} indices: each after the first drawn by index after a wait for the work in flight ({w2[0]} waits), none expanded, none through the registers')
 # .559: packets of few triangles for these two (4,000 vertices: 512 are full after some 170 triangles): a packet drawn expanded must fit a
 # fetch group (8,192 words), or it goes through the registers after a retire, which the fences that never come here do not allow
 P=indexed_packets(4000,1000);assert len(P)>=6 and all(len(i)*10<6000 for b,i,w in P),[len(i) for b,i,w in P]
 g,st,va=run(VA,[P[:3],P[3:6],P[:3]],stuck=True,slots=4)
 assert va[1]>0 and st[1]==va[1]+va[2] and st[0]==va[0] and 1<=va[0]<=4,(st,va) # .533: packets past 192 vertices take two of the 4 slots
 g,st2,va2=run(VA,[P[:6]],stuck=True,slots=4)
 assert va2[1]+va2[2]>0 and st2[1]==va2[1]+va2[2] and st2[0]==va2[0] and 1<=va2[0]<=4,(st2,va2)
 cases.append(f'v10.470 INDEXED: with 4 slots and no fence coming, a slot is not given again while its group is in flight or is the group being built ({va[1]}, then {va2[1]} + {va2[2]} refused): those packets drawn expanded')
 return cases

def texrun_checks():
 """.528 through the built VxD and XP's ring: ACCEL_D3D_TEXRUNS (an indexed packet's texture runs, from tests/core.so's HAL with the batch
 kept open over two texture changes): every index drawn with the same texture registers and vertex words as the three packets made with
 it off; ACCEL_D3D_SEQSKIP: the batch call leaves the sequencer as it found it, on and off"""
 import sys,random as R
 sys.path.insert(0,str(ROOT/'tests'));import test_core as T
 cases=[];VA=0xA00000
 class X(T.Contract):
  def runTest(self):pass
 def make(on):
  t=X();t.setUp();T.U.in_dll(T.lib,'vx_d3d_feat').value=T.ALLF&~512;rnd=R.Random(528);got=[]
  def submit_n(_,p,n,done):
   arr=T.C.cast(p,T.C.POINTER(T.Packet));got.extend(bytes(arr[i]) for i in range(n));done[0]=n;return 0
  fn=T.SUBMITN(submit_n);t.services.submit_n=fn
  t._stages([(7,4,2,0,2,2,0,0)],spec=1);t.context.state.rs[19]=5;t.context.state.rs[20]=6
  vb=b''.join(struct.pack('<4fII2f',rnd.uniform(1,60),rnd.uniform(1,44),.5,1.,0xA0C08040+k,0xFF204060+k,rnd.random(),rnd.random()) for k in range(60))
  A,B,Cc=[[tuple(rnd.sample(range(60),3)) for _ in range(k)] for k in (20,7,30)]
  tss=lambda h:T.cmd(25,1,struct.pack('<HHI',0,0,h))
  for n_,v in (('vx_d3d_prepack',1),('vx_d3d_indexed',1),('vx_d3d_texruns',on)):T.U.in_dll(T.lib,n_).value=v
  try:rc=t.run_dp2(t._itl(A)+tss(5)+t._itl(B)+tss(7)+t._itl(Cc),vb,stride=32,fvf=0x1C4)
  finally:
   for n_ in ('vx_d3d_prepack','vx_d3d_indexed','vx_d3d_texruns'):T.U.in_dll(T.lib,n_).value=0
   T.U.in_dll(T.lib,'vx_d3d_feat').value=T.ALLF&~128;t.services.submit_n=T.SUBMITN(0)
  assert rc==(0,0),rc
  out=[]
  for b in got:q=bytearray(b);struct.pack_into('<I',q,16,1);struct.pack_into('<I',q,20,0x100000);out.append(q)
  return out
 class RecGPU(XRGPU):
  def __init__(s,*a,**k):super().__init__(*a,**k);s.got=[]
  def fetch_2d(s,a,b):
   if s.gmode and b>a:s.got.extend(s.gwords(a,(b-a)//4))
   return super().fetch_2d(a,b)
 def model(g):
  out=[];tex={};smp={};t4={};left=0
  for kind,blk in decode(list(g.got)):
   if kind!='43C':continue
   h=blk[0];bank=(h>>16)&255;mode=h>>24
   for v in blk[1:]:
    if bank==2 and mode==0:tex[v>>24]=v&0xFFFFFF
    elif bank==2 and mode==0x20:smp[v>>24]=v&0xFFFFFF
    elif bank==4:
     t4[v>>24]=v&0xFFFFFF
     if v>>24==0x24:left=v&0xFFFFFF
    elif bank==0 and t4.get(0x20)==1: # .555: two 16-bit indices a word, the first in the low half; 04/24 of them a burst
     for i in [v&0xFFFF,v>>16][:min(2,left)]:
      a=(t4[0x21]|((t4[0x22]&255)<<24))+i*((t4[0x22]>>8)&0xFFFF);left-=1
      out.append(((tex.get(0),tex.get(0x20),tex.get(0x22),tex.get(0x30),smp.get(0x30),smp.get(0x31),smp.get(0x32)),tuple(g.word(FB+a+4*k) for k in range(8))))
  return out
 res={}
 for on in (0,1):
  P=make(on);assert len(P)==(1 if on else 3),(on,len(P))
  if on:assert u32(P[0],C_RUNS)==2,u32(P[0],C_RUNS)
  for sk in (1,):
   g=RecGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_va_off'),pack([VA])) # .529: the sequencer skip always on
   s0=dict(g.seq);g.u.mem_write(BATCH,b''.join(bytes(x) for x in P));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
   r=g.call('VX3DN_submit_batch',(BATCH,len(P),ARENA+0xc800));assert (r,g.word(ARENA+0xc800))==(0,len(P)),(on,sk,r)
   assert g.call('VX3DN_busy',())==0
   assert all(g.seq.get(k)==s0.get(k) for k in (0x10,0x1a,0x1e)),('the sequencer as found',sk,{k:(s0.get(k),g.seq.get(k)) for k in (0x10,0x1a,0x1e)})
   sq=[g.word(g.addr('vx3dn_seq_stat')+4*k) for k in range(3)];assert sq[0]==1 and (sq[1]==2 if sk else sq[1]==0),(sk,sq)
   res[(on,sk)]=model(g)
  if on:tr=[g.word(g.addr('vx3dn_texrun_stat')+4*k) for k in range(2)];assert tr==[1,2],tr
 assert len(res[(0,1)])==171 and res[(0,1)]==res[(1,1)],[len(v) for v in res.values()]
 assert len({s for s,_ in res[(1,1)]})==2
 cases.append('.528 ACCEL_D3D_TEXRUNS: one packet with two texture runs through the built VxD and XP\'s ring: its 171 indices drawn with the same texture and sampler registers and vertex words as the three packets made without runs; ACCEL_D3D_SEQSKIP: the batch call leaves SR10, SR1A and SR1E as found, two writes left out with them set')

 # .529: the 17x17 grid by index (289 vertices, 512 triangles, cull none: the pass-through) from the HAL as one packet of 289 vertices (two
 # vertex-area slots); .533: against the same grid in two calls of 8 rows (two packets of 153 vertices, one slot each): every index drawn
 # with the same registers and vertex words
 def make_big(split):
  t=X();t.setUp();T.U.in_dll(T.lib,'vx_d3d_feat').value=T.ALLF&~512;rnd=R.Random(529);got=[]
  def submit_n(_,p,n,done):
   arr=T.C.cast(p,T.C.POINTER(T.Packet));got.extend(bytes(arr[i]) for i in range(n));done[0]=n;return 0
  fn=T.SUBMITN(submit_n);t.services.submit_n=fn
  t._stages([(7,4,2,0,2,2,0,0)],spec=1);t.context.state.rs[22]=1
  vb=b''.join(struct.pack('<4fII2f',2+3.5*(k%17),2+2.5*(k//17),.5,1.,0xA0C08040+k,0xFF204060+k,rnd.random(),rnd.random()) for k in range(289))
  tris=[]
  for y in range(16):
   for x in range(16):a=y*17+x;tris+=[(a,a+1,a+17),(a+1,a+18,a+17)]
  for n_,val in (('vx_d3d_prepack',1),('vx_d3d_indexed',1),('vx_d3d_ixpass',1)):T.U.in_dll(T.lib,n_).value=val
  try:
   if split:rc=t.run_dp2(t._itl(tris[:256]),vb,stride=32,fvf=0x1C4);rc=rc if rc!=(0,0) else t.run_dp2(t._itl(tris[256:]),vb,stride=32,fvf=0x1C4)
   else:rc=t.run_dp2(t._itl(tris),vb,stride=32,fvf=0x1C4)
  finally:
   for n_,val in (('vx_d3d_prepack',0),('vx_d3d_indexed',0)):T.U.in_dll(T.lib,n_).value=val
   T.U.in_dll(T.lib,'vx_d3d_feat').value=T.ALLF&~128;t.services.submit_n=T.SUBMITN(0);t.context.state.rs[22]=3
  assert rc==(0,0),rc
  out=[]
  for b in got:q=bytearray(b);struct.pack_into('<I',q,16,1);struct.pack_into('<I',q,20,0x100000);out.append(q)
  return out
 bres={}
 for v in (0,1):
  P=make_big(v);assert len(P)==(2 if v else 1) and all(u32(q,108)<=(192 if v else 384) for q in P),(v,len(P),[u32(q,108) for q in P])
  g=RecGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_va_off'),pack([VA]))
  g.u.mem_write(BATCH,b''.join(bytes(x) for x in P));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
  r=g.call('VX3DN_submit_batch',(BATCH,len(P),ARENA+0xc800));assert (r,g.word(ARENA+0xc800))==(0,len(P)),(v,r)
  assert g.call('VX3DN_busy',())==0
  va=[g.word(g.addr('vx3dn_va_stat')+4*k) for k in range(3)];assert va[0]==len(P) and va[1]==va[2]==0,(v,va)
  bres[v]=(model(g),g.word(g.addr('vx3dn_va_stat')+12))
 assert len(bres[0][0])==1536 and bres[0][0]==bres[1][0],(len(bres[0][0]),len(bres[1][0]))
 # two slots side by side: a packet past 192 vertices in the last slot starts again at slot 0 (the slots used lowered to 3)
 P=make_big(0)
 for nsl,first in ((3,0),(2,0)):
  g=RecGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_va_off'),pack([VA]));g.u.mem_write(g.addr('vx3dn_va_nslots'),pack([nsl]))
  for k in range(2):
   g.u.mem_write(BATCH,b''.join(bytes(x) for x in P));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
   r=g.call('VX3DN_submit_batch',(BATCH,1,ARENA+0xc800));assert (r,g.word(ARENA+0xc800))==(0,1),(nsl,k,r);assert g.call('VX3DN_busy',())==0
  assert model(g)==bres[0][0]*2,(nsl,len(model(g)))
 cases.append('.529 ACCEL_D3D_IX384: the 17x17 grid by index (289 vertices, 512 triangles) through the built VxD and XP\'s ring as one packet in two vertex-area slots side by side, or two packets at 192: its 1536 indices drawn with the same registers and vertex words; a two-slot packet at the end of the slots starts again at slot 0, drawn twice the same')
 return cases

def shadow_checks():
 """.525 through the built VxD and XP's ring (.526: .525's ACCEL_D3D_IXCHK removed): ACCEL_D3D_SHADOW (the set-up's type-01 register shadow): the same results with fewer words, none left out by the first set-up after
 the engine's state is forgotten (a TEXDBG switch, a refused packet) and none with the switch off"""
 cases=[];VA=0xA00000;SLOT=7680
 P=indexed_packets()
 class RecGPU(XRGPU):
  def __init__(s,*a,**k):super().__init__(*a,**k);s.got=[]
  def fetch_2d(s,a,b):
   if s.gmode and b>a:s.got.extend(s.gwords(a,(b-a)//4))
   return super().fetch_2d(a,b)
 def mk(opts):
  g=RecGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_va_off'),pack([VA]))
  for n,v in opts.items():g.u.mem_write(g.addr(n),pack([v]))
  return g
 def batch(g,q):
  g.u.mem_write(BATCH,b''.join(bytes(x) for x in q));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
  return g.call('VX3DN_submit_batch',(BATCH,len(q),ARENA+0xc800)),g.word(ARENA+0xc800)
 st=lambda g,n,k:[g.word(g.addr(n)+4*i) for i in range(k)]
 # the shadow: packets whose raster state alternates (blend 0x88, write mask 0x92) so that each misses the state cache and sends its set-up
 Q=[]
 for k,(b,i,w) in enumerate(P):
  q=bytearray(b);struct.pack_into('<I',q,88,k&1);struct.pack_into('<I',q,92,15 if k%3 else 7);Q.append(q)
 out={}
 for sh in (0,1):
  g=mk({'vx3dn_shadow':sh});r=[batch(g,Q[:4]),batch(g,Q[4:])];assert g.call('VX3DN_busy',())==0
  out[sh]=(r,len(g.got),st(g,'vx3dn_sh_stat',2),g)
 assert out[0][0]==out[1][0] and out[0][2]==[0,0] and out[1][2][0]>0 and out[1][2][1]>0 and out[1][1]<out[0][1],(out[0][:3],out[1][:3])
 def draws(g): # the type-01 registers as the fetched words leave them, at every draw (a type-04 block that writes 04/24)
  m={};snaps=[]
  for t,b in decode(list(g.got)):
   if not b:continue
   if b[0]==0x10000:
    for v in b[1:]:m[v>>24]=v&0xFFFFFF
   elif (b[0]>>16)&255==2 and (b[0]>>24) in (0,1,2,0x20,0x21,0x22) and not b[0]&0xFFFF: # .526: the texture units' registers too
    for v in b[1:]:m[(b[0]>>24,v>>24)]=v&0xFFFFFF
   elif b[0]==0x40000 and any(v>>24==0x24 for v in b[1:]):snaps.append(tuple(sorted(m.items(),key=lambda kv:str(kv[0]))))
  return snaps
 d0,d1=draws(out[0][3]),draws(out[1][3]);assert len(d0)>=len(Q) and d0==d1,(len(d0),len(d1))
 assert any(isinstance(k,tuple) for s_ in d1 for k,v in s_),'the texture registers modelled'
 g=out[1][3]
 dp=ARENA+0xc900;ib=ARENA+0xca00;ob=ARENA+0xca40;cb=ARENA+0xca80
 def texdbg(code,sub=0):
  g.u.mem_write(ib,pack([code,sub]));g.u.mem_write(dp,pack([0,0,0,0x85500022,ib,8,ob,32,cb,0,0,0]))
  return g.call('_Device_IO_Control_proc@8',(0,dp),stdcall=True)
 assert texdbg(0x50)==0 # a switch: the engine's state forgotten
 s0=st(g,'vx3dn_sh_stat',2);assert batch(g,[Q[1]])==(0,1);s1=st(g,'vx3dn_sh_stat',2);assert s1[1]==s0[1],(s0,s1) # its first set-up whole
 assert batch(g,[Q[2],Q[3]])==(0,2);s2=st(g,'vx3dn_sh_stat',2);assert s2[1]>s1[1],(s1,s2) # then the shadow again
 bad=bytearray(Q[0]);struct.pack_into('<I',bad,10456+4*3,0) # rhw 0: refused, the engine's state forgotten
 assert batch(g,[Q[1],bad])==(1,1);s3=st(g,'vx3dn_sh_stat',2);assert batch(g,[Q[2]])==(0,1);s4=st(g,'vx3dn_sh_stat',2)
 # .527: the VxD's record of the refused packet (TEXDBG BC EC/ED): the batch path, a line of vx_native3d_hw.c that refuses, packet 2 of 2
 rd=lambda sub:(texdbg(0xBC,sub),struct.unpack('<8I',bytes(g.u.mem_read(ob,32))))[1]
 b0=rd(0xEC);hw=(ROOT/'src'/'vx_native3d_hw.c').read_text().split('\n')
 assert b0[0]&255==0xBC and b0[2]>=1 and b0[3]>>24==2 and (b0[3]>>16)&255==1 and 'VX3DN_BAD_HERE' in hw[(b0[3]&0xFFFF)-1] and b0[4]==1 and b0[5]==2,b0
 rd(0xEE);assert rd(0xEC)[2]==0,'cleared'
 cases.append('.527: a packet refused in a batch recorded by the VxD (TEXDBG BC EC/ED): the batch path, the refusing line of vx_native3d_hw.c, its index; BC EE clears')
 cases.append(f'.525 ACCEL_D3D_SHADOW: {len(Q)} packets of alternating raster state: the type-01 registers the same at every one of the {len(d0)} draws, the same results with {out[0][1]-out[1][1]} fewer words fetched ({out[1][2][1]} type-01 words left out in {out[1][2][0]} set-ups), none with it off; after the state is forgotten the next set-up whole, then the shadow again')
 return cases

DL_R=[(0,108),(3952,3992),(5528,5680),(6448,6680),(8216,8348),(9116,9292),(10320,10452)] # .524: VX3DN_STATE_RANGES

def delta_mark(pks):
 """.524: the HAL's marks on a batch: each packet after the first carries VX3DN_SAME_TAG | the ranges 1-6 the same as the one before"""
 out=[bytearray(pks[0])]
 for k in range(1,len(pks)):
  b=bytearray(pks[k]);m=0
  for r,(lo,hi) in enumerate(DL_R[1:]):
   if pks[k][lo:hi]==pks[k-1][lo:hi]:m|=1<<r
  struct.pack_into('<I',b,104,0x5D000000|m);out.append(b)
 return out

def delta_checks():
 """.524 ACCEL_D3D_DELTA through the VxD's batch path (copy_packet_d, vx3dn_validate_geo): batches whose packets carry the HAL's marks
 give the same words fetched and the same results as unmarked ones and as with the VxD's switch off; the ranges kept and the packets
 validated by their geometry alone counted; a packet marked all the same with a bad vertex still refused; the first packet's mark (no
 packet before it in the call) and a single submit's copy everything; a range marked the same that is not keeps the packet before's
 state (the VxD's copy, already validated: the HAL's mark is trusted only for what it draws)"""
 cases=[]
 class RecGPU(XRGPU):
  def __init__(s,*a,**k):super().__init__(*a,**k);s.got=[]
  def fetch_2d(s,a,b):
   if s.gmode and b>a:s.got.extend(s.gwords(a,(b-a)//4))
   return super().fetch_2d(a,b)
 D=prepack_packets()
 seqs=[['tx96','tx30','tx96','two','two','tx96'],['tx96']*6,['two','tx30','tx30','two']]
 def stat(g):return [g.word(g.addr('vx3dn_delta_stat')+4*k) for k in range(5)]
 def run(mark,opts={},which=1):
  g=RecGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1
  for n,v in opts.items():g.u.mem_write(g.addr(n),pack([v]))
  res=[]
  for r in range(9):
   q=[D[nm][which] for nm in seqs[r%len(seqs)]]
   if mark:q=delta_mark(q)
   g.u.mem_write(BATCH,b''.join(bytes(x) for x in q));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
   res.append((g.call('VX3DN_submit_batch',(BATCH,len(q),ARENA+0xc800)),g.word(ARENA+0xc800)))
  assert g.call('VX3DN_busy',())==0
  return res,g.got,stat(g),g
 for which in (1,0):
  a=run(0,which=which);b=run(1,which=which);c=run(1,{'vx3dn_delta':0},which=which)
  assert all(r==(0,len(seqs[i%len(seqs)])) for i,r in enumerate(a[0])) and b[0]==a[0]==c[0],(a[0],b[0],c[0])
  assert len(a[1])>5000 and b[1]==a[1] and c[1]==a[1],(which,len(a[1]),len(b[1]),len(c[1]))
  nt=sum(len(seqs[r%len(seqs)])-1 for r in range(9))
  assert a[2]==[0]*5 and b[2][0]==nt and b[2][1]>=4*nt and b[2][2]>0 and b[2][3]>0 and b[2][4]==0 and c[2][0]==0 and c[2][4]==nt,(which,a[2],b[2],c[2])
 cases.append(f'.524 ACCEL_D3D_DELTA: batches with the HAL\'s marks: {len(b[1])} words fetched the same as unmarked and with the VxD\'s switch off (prepacked and the six arrays); {b[2][1]} ranges kept, {b[2][3]} packets validated by their geometry alone; off: every marked packet copied whole')
 def batch(g,pks):
  g.u.mem_write(BATCH,b''.join(bytes(x) for x in pks));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
  return g.call('VX3DN_submit_batch',(BATCH,len(pks),ARENA+0xc800)),g.word(ARENA+0xc800)
 for oc in (1,0):
  g=RecGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_onecopy'),pack([oc]))
  good=D['tx96'][1];bad=bytearray(good);struct.pack_into('<I',bad,10456+40*5,0x7F800000)
  q=delta_mark([good,bad,good]);assert u32(q[1],104)==0x5D00003F
  s0=stat(g);rc,done=batch(g,q);s1=stat(g)
  assert (rc,done)==(1,1) and s1[3]-s0[3]==1,(oc,rc,done,s0,s1) # the bad one validated by its geometry alone, and refused
  assert g.call('VX3DN_busy',())==0
  rc,done=batch(g,delta_mark([good,good]));assert (rc,done)==(0,2),(rc,done)
 cases.append('.524 ACCEL_D3D_DELTA: a packet marked all the same as the one before with an infinite x refused by the geometry\'s checks (ONECOPY on and off), the one before drawn, the device going on')
 g=RecGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1
 t0=bytearray(D['tx96'][1]);struct.pack_into('<I',t0,104,0x5D00003F)
 s0=stat(g);rc,done=batch(g,[t0,D['tx30'][1]]);s1=stat(g);assert (rc,done)==(0,2) and s1[4]-s0[4]==1 and s1[0]==s0[0],(rc,done,s0,s1)
 assert g.submit(t0)==0 and stat(g)[4]-s1[4]==1
 cases.append('.524 ACCEL_D3D_DELTA: the first packet of a call and a single submit with a mark: copied whole (counted), drawn')
 # a mark that lies: the texture range marked the same where it is not: the VxD draws with its validated copy of the packet before's
 two,one=D['two'][1],D['tx96'][1]
 lie=delta_mark([one,two]);lie[1]=bytearray(lie[1]);struct.pack_into('<I',lie[1],104,0x5D00003F)
 g1=RecGPU(32);g1.seq[0x1a]=8;g1.seq[0x1e]=1;r1=batch(g1,lie)
 assert r1[0] in (0,1),r1
 cases.append(f'.524 ACCEL_D3D_DELTA: a range marked the same that is not: the packet drawn with the copy of the packet before\'s state, or refused by the checks of its vertices against it (result {r1}); nothing outside the VxD\'s own copy read')
 return cases

def prepack_checks():
 """v10.441: prepacked vertices through the VxD: the words fetched from XP's ring the same as the six arrays give, over batches (state
 cache on, wraps); the copy counted; a debug switch the form does not suit (ACCEL set test 1, two units) unpacks them into the arrays,
 the words as the arrays' with the same switch; a prepacked vertex not finite refused as the arrays' is"""
 cases=[]
 class RecGPU(XRGPU):
  def __init__(s,*a,**k):super().__init__(*a,**k);s.got=[]
  def fetch_2d(s,a,b):
   if s.gmode and b>a:s.got.extend(s.gwords(a,(b-a)//4))
   return super().fetch_2d(a,b)
 D=prepack_packets()
 seqs=[['tx96','tx30','tx96','two'],['two']*6,['tx30','tx96']*5,['tx96']*8]
 def run(which,opts={}):
  g=RecGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1
  for n,v in opts.items():g.u.mem_write(g.addr(n),pack([v]))
  res=[]
  for r in range(12):
   q=[D[nm][which] for nm in seqs[r%len(seqs)]];g.u.mem_write(BATCH,b''.join(bytes(x) for x in q));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
   res.append((g.call('VX3DN_submit_batch',(BATCH,len(q),ARENA+0xc800)),g.word(ARENA+0xc800)))
  assert g.call('VX3DN_busy',())==0
  return res,g.got,[g.word(g.addr('vx3dn_pre_stat')+4*k) for k in range(4)]
 a=run(0);b=run(1);N=sum(len(seqs[r%len(seqs)]) for r in range(12))
 assert all(r==(0,len(seqs[i%len(seqs)])) for i,(r) in enumerate(a[0])) and b[0]==a[0],(a[0],b[0])
 assert len(a[1])>20000 and b[1]==a[1],(len(a[1]),len(b[1]))
 assert a[2][:3]==[0,0,0] and b[2][0]==N and b[2][1]==0 and b[2][2]==N and a[2][3]>0 and b[2][3]<a[2][3],(a[2],b[2]) # v10.449: ACCEL_D3D_ONECOPY (default 1): every block left to the emitter, fewer bytes copied in
 c=run(1,{'vx3dn_onecopy':0});assert c[0]==a[0] and c[1]==a[1] and c[2][2]==0 and c[2][0]==N and c[2][3]>b[2][3],c[2]
 e=run(1,{'vx3dn_fastclock':0});assert e[0]==b[0] and e[1]==b[1],'v10.448: ACCEL_D3D_FASTCLOCK 0 (Get_System_Time at each kick): the same words and results'
 cases.append('v10.448 ACCEL_D3D_FASTCLOCK (default 1): the kick time stamp from the VMM\'s time word; off (Get_System_Time) the same words fetched, the same results')
 cases.append('v10.449 ACCEL_D3D_ONECOPY (default 1): a batch\'s prepacked vertices copied once, from the caller\'s packet into the command block, and checked there: the same words fetched as with it off and as the arrays, every block left to the emitter, fewer bytes copied in')
 cases.append(f'v10.441 prepacked vertices: {len(b[1])} words fetched from XP\'s ring the same as the six arrays give (one unit with specular, two units in strips; batches, wraps)')
 def batch(g,pks):
  g.u.mem_write(BATCH,b''.join(bytes(x) for x in pks));g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
  return g.call('VX3DN_submit_batch',(BATCH,len(pks),ARENA+0xc800)),g.word(ARENA+0xc800)
 for which in (0,1):
  bads=[]
  b1=bytearray(D['tx96'][which])
  if which:struct.pack_into('<I',b1,10456+40*5,0x7F800000)
  else:struct.pack_into('<I',b1,112+20*5,0x7F800000)
  bads.append(b1)
  b2=bytearray(D['tx96'][which])
  if which:struct.pack_into('<I',b2,10456+40*7+24,0x7FC00000)
  else:struct.pack_into('<I',b2,112+20*192+40+8*7,0x7FC00000)
  bads.append(b2)
  if which:b3=bytearray(D['two'][1]);struct.pack_into('<I',b3,10456+40*3+32,0x7F800000);bads.append(b3) # two units: u1 v1 checked
  for oc in (1,0): # v10.449: refused when the emitter checks the copy and when the validation does
   for bad in bads:
    g=RecGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_onecopy'),pack([oc]))
    assert g.submit(bad)==1,which # the single submit: the validation
    good=D['tx96'][which];n0=len(g.got)
    rc,done=batch(g,[good,bad,good]);assert (rc,done)==(1,1),(which,oc,rc,done) # the batch: the good one before it drawn, the bad one refused, none after
    assert g.call('VX3DN_busy',())==0
    rc,done=batch(g,[good]);assert (rc,done)==(0,1),(which,oc,rc,done) # the device goes on
 cases.append('v10.441: a prepacked vertex with x infinite or u0 NaN refused, as the arrays\' is, u1 infinite with two units too; v10.449: in a batch with ACCEL_D3D_ONECOPY on and off: the packet before drawn, the bad one refused, the device going on')
 return cases

def texonly_checks():
 """v10.450: ACCEL_D3D_TEXONLY: a packet that differs from the last only in its textures (surface, filter, addressing) fetches the texture
 cache strobe and the texture units only; after every packet the engine's registers and data blocks (as the fetched words leave them)
 are the same as with it off (every packet's whole set-up); a change outside the textures, the first packet and TEXONLY 0 send the whole
 set-up; fewer words fetched; the count of packets sent so"""
 import sys
 sys.path.insert(0,str(ROOT/'tests'));import test_core as T
 cases=[]
 D=prepack_packets();A=bytes(D['tx96'][1])
 to=T.Packet.tex.offset;so=to+T.Texture.surface.offset
 B=bytearray(A);struct.pack_into('<I',B,so,u32(B,so)+0x1000);struct.pack_into('<I',B,to+T.Texture.filter.offset,(u32(B,to+T.Texture.filter.offset)+1)%4)
 au=to+T.Texture.address_u.offset;struct.pack_into('<I',B,au,u32(B,au)%3+1)
 C=bytearray(B);zf=T.Packet.z_func.offset;struct.pack_into('<I',C,zf,u32(C,zf)%8+1)
 seq=[A,bytes(B),A,bytes(B),bytes(C),bytes(B),A]
 class RecGPU(XRGPU):
  def __init__(s,*a,**k):super().__init__(*a,**k);s.got=[]
  def fetch_2d(s,a,b):
   if s.gmode and b>a:s.got.extend(s.gwords(a,(b-a)//4))
   return super().fetch_2d(a,b)
 def apply(st,words):
  for t,b in decode(words):
   if t!='43C' or not b[0]:continue
   h,body=b[0],b[1:]
   if ((h>>16)&0xFF)==0x03 or h==0x00FE0000:st[('data',h)]=tuple(body)
   else:
    for w in body:st[(h,w>>24)]=w&0xFFFFFF
 def run(on):
  g=RecGPU(32);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_texonly'),pack([on]))
  st={};snaps=[];nw=[]
  for p in seq:
   g.got=[];g.u.mem_write(BATCH,p);g.u.mem_write(ARENA+0xc800,pack([0xEEEE]))
   assert (g.call('VX3DN_submit_batch',(BATCH,1,ARENA+0xc800)),g.word(ARENA+0xc800))==(0,1)
   apply(st,g.got);snaps.append(dict(st));nw.append(len(g.got))
  assert g.call('VX3DN_busy',())==0
  return snaps,nw,[g.word(g.addr('vx3dn_texonly_stat')+4*k) for k in range(2)]
 a=run(0);b=run(1)
 for k in range(len(seq)):assert a[0][k]==b[0][k],(k,{x:(a[0][k].get(x),b[0][k].get(x)) for x in set(a[0][k])|set(b[0][k]) if a[0][k].get(x)!=b[0][k].get(x)})
 assert a[2][0]==0 and b[2][0]==4,(a[2],b[2])
 assert [b[1][k]<a[1][k] for k in range(len(seq))]==[False,True,True,True,False,False,True],(a[1],b[1])
 cases.append('v10.450 ACCEL_D3D_TEXONLY (default 1): a packet whose state differs from the last only in its textures (surface, filter, addressing) fetches the strobe and the texture units only: the engine\'s registers and blocks after every packet the same as with the whole set-up; a depth-test change and the first packet send it whole; %d of %d words fetched'%(sum(b[1]),sum(a[1])))
 return cases

def wprof_checks():
 """v10.440: the command words by part (TEXDBG BC 9E-A1): off until turned on; each packet's words split into the set-ups, the vertex
 words and the rest, adding up to its words in the ring; vertices and triangles; a set-up the same words as the last packet's counted
 (read back from the staging buffer); BC A3 turns it off."""
 cases=[]
 for bpp in (32,16):
  pk=bytearray(packet(bpp));struct.pack_into('<I',pk,20,0x100000)
  g=XRGPU(bpp);g.seq[0x1a]=8;g.seq[0x1e]=1;g.u.mem_write(g.addr('vx3dn_state_cache'),pack([0]))
  W=lambda:[g.word(g.addr('vx3dn_wstat')+4*i) for i in range(24)]
  assert g.submit(pk)==0 and W()[0]==0
  g.call('vx3dn_wprof_set',(1,))
  for k in range(3):assert g.submit(pk)==0
  w=W();nw=5
  assert w[0]==3 and w[1]==3 and w[9]==9 and w[10]==3 and w[6]==9*nw and w[11]==0,w
  assert w[3]+w[4]+w[5]+w[6]+w[7]+w[8]==w[2] and w[3]>0 and w[18]==3 and w[12]==2 and w[13]==2*w[3]//3,w
  st=bytearray(full_packet(bpp,[(1,22)]));struct.pack_into('<I',st,20,0x100000)
  assert g.submit(st)==0;w2=W();assert w2[9]-w[9]==22 and w2[10]-w[10]==20 and w2[11]-w[11]==22 and w2[3]+w2[4]+w2[5]+w2[6]+w2[7]+w2[8]==w2[2],w2
  g.call('vx3dn_wprof_set',(0,));assert g.submit(pk)==0 and W()==w2
  cases.append(f'{bpp}bpp v10.440: the command words by part add up to the packet\'s words; vertices, triangles (list and strip), strip vertices; set-ups repeating the last packet\'s counted; off: nothing counted')
 return cases

class RtcGPU(NativeGPU):
 """.524: the RTC (CMOS ports 70h/71h: registers A, B, C) and VPICD's services (int 20h, the VxD's inline calls: Virtualize_IRQ gives a
 handle, the others are logged) for the sampler, whose only clock is now the RTC's periodic interrupt"""
 def __init__(s,*a,busy_pie=False,**k):
  super().__init__(*a,**k);s.cmos={0x0A:0x26,0x0B:0x02|(0x40 if busy_pie else 0),0x0C:0};s.cmi=0;s.vpicd=[];s.cmos_log=[]
  from unicorn import UC_HOOK_INTR
  global UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EFLAGS
  from unicorn.x86_const import UC_X86_REG_EIP,UC_X86_REG_EAX,UC_X86_REG_EFLAGS
  s.u.hook_add(UC_HOOK_INTR,s.intr)
 def intr(s,u,n,_):
  assert n==0x20,n
  eip=u.reg_read(UC_X86_REG_EIP);sid=s.word(eip);u.reg_write(UC_X86_REG_EIP,eip+4);s.vpicd.append(sid)
  if sid==0x00030001:u.reg_write(UC_X86_REG_EAX,0x9000);u.reg_write(UC_X86_REG_EFLAGS,u.reg_read(UC_X86_REG_EFLAGS)&~1) # Virtualize_IRQ: a handle, CF clear
 def port_in(s,u,p,z,_):
  if p==0x71:return s.cmos.get(s.cmi&0x7F,0)
  return super().port_in(u,p,z,_)
 def port_out(s,u,p,z,v,_):
  if p==0x70:s.cmi=v;return
  if p==0x71:s.cmos[s.cmi&0x7F]=v;s.cmos_log.append((s.cmi&0x7F,v));return
  return super().port_out(u,p,z,v,_)

def smp_checks():
 """v10.434: the sampler: started (the caller's CS taken as the flat code selector), each tick classed by the current thread and its
 client registers, a frame's samples added at its flip by its packet count and time, the engine's busy bit, pages kept for the render
 thread's code; the starter's close stops it. .524: the ticks are the RTC's interrupts (IRQ 8 virtualized, register A rate 6, B's PIE;
 the time-out clock removed): put back as they were at the stop; no start when B's PIE is already set."""
 cases=[];g=RtcGPU(32);st={'thr':0x111,'vm':0xC1000000,'run':100}
 VM=0xC1000000;CL=0xC1001000;g.u.mem_map(VM,0x2000);g.u.mem_write(VM+8,pack([CL]))
 def client(eip,cs=0x137,efl=0x202):g.u.mem_write(CL+0x24,pack([eip,cs,efl]))
 g.hook('Get_Cur_Thread_Handle',lambda:st['thr']);g.hook('Get_Cur_VM_Handle',lambda:VM)
 g.hook('Get_Thread_Exec_Time',lambda:st['run'])
 smp=ARENA+0x4000;inb=ARENA+0x3f00
 def ioctl(op,a=0,n=0,out=0x10):g.u.mem_write(inb,pack([op,a,n]));return g.call('VX_smp_ioctl',(inb,12,smp,out,0x77))
 def rd():
  w=[g.word(smp+4*i) for i in range(16+48+12)];return w
 client(0x401000);assert ioctl(1,0xA0000000,0xF3000)==8 and g.word(smp)==0x549 and g.word(smp+4)==1 # .524: in[3] not given: the RTC all the same
 assert g.vpicd[0]==0x00030001 and g.cmos[0x0A]==0x26 and g.cmos[0x0B]==0x42,(g.vpicd,g.cmos) # IRQ 8 virtualized; rate 6, PIE set
 def tick(late=0):g.call('_rtc_irq@4',(0x9000,),stdcall=True) # .524: the RTC's interrupt (no lateness: a hardware interrupt)
 render=g.addr('vx_smp_render');sub_=g.addr('vx_smp_pkbias');SL=ARENA+0x3e00 # v10.439: VX_smp_enter's slot and TSC
 def frame(iv,packets):g.u.mem_write(sub_,pack([g.word(sub_)+packets]));st['run']+=iv//2;g.call('VX_smp_frame',(iv,))
 frame(0,0) # the first flip: nothing to add, the run time read
 tick() # no render thread yet
 g.u.mem_write(render,pack([0x111]))
 client(0x401234);tick();tick() # the render thread in the game's code (page 0x401)
 client(0xA0001234);tick() # in VXDDRAW
 ph=g.addr('vx_smp_ph');g.u.mem_write(ph,pack([5])) # .520: the VxD building words
 g.call('VX_smp_enter',(SL,SL+4));tick();w=g.call('VX_smp_wait_begin');tick();g.call('VX_smp_wait_end',(w,));g.call('VX_smp_leave',(g.word(SL),g.word(SL+4),0x85500021)) # in the VxD; waiting
 client(0x2000,cs=0x1F);tick() # 16-bit code
 st['thr']=0x222;client(0xBFF71000);tick();st['thr']=0x111 # another thread in a shared DLL (a 1 ms stretch, after call 85500021)
 geo=g.addr('vx3dn_geo');g.u.mem_write(geo+8,pack([g.word(geo+8)+1234])) # v10.441: the frame's triangles
 frame(40,60) # a gameplay frame under 75 ms
 # .520: the time-weighted fields after tri[3] (word 5006): tw[3][8] 5009, gap_n[4][8] 5033, gap_t[4] 5065, ph_n[16] 5069, ph_t[16] 5085,
 # tsc_khz 5101, tw_total 5102, clk 5103 (.521), n_all[8] 5104, tw_all[8] 5112
 assert ioctl(2,out=0x20000)>8
 W=lambda i:g.word(smp+4*i)
 n_all=[W(5104+k) for k in range(8)];tw0=[W(5009+k) for k in range(8)];gap=[[W(5033+8*l+b) for b in range(8)] for l in range(4)]
 assert n_all==[1,1,1,2,1,1,0,1],n_all # every tick by category, whatever the frames (the first tick: no render thread yet)
 assert all((tw0[k]>0)==(k!=6) for k in range(8)),tw0 # the frame's time-weighted samples: every category it had a tick in (none in other threads' 16-bit code)
 assert [sum(x) for x in gap]==[4,2,1,1],gap # ticks by where they landed: render ring 3 (code x2, HAL, 16-bit), inside the VxD (2), another thread, none
 assert W(5069+5)==2 and sum(W(5069+k) for k in range(16))==2 and W(5085+5)>0,'.520: the in-VxD ticks by phase'
 assert W(5101)>0 and W(5102)==sum(W(5112+k) for k in range(8)) and W(5103)==1,'.520: the TSC rate; tw_total the sum of tw_all; .524: the clock the RTC'
 g.u.mem_write(render,pack([0x999]));o=ioctl(3,10,0,out=0x10);assert o==4 and g.word(smp)>0 and g.word(render)==0x111 and g.word(ph)==5,'.520: the spin marks the render thread, keeps the phase'
 assert ioctl(2,out=0x20000)>8 and g.word(smp+4*5006)==1234,'the frame\'s triangles (vx_smp tri[0])'
 st['thr']=0x222;client(0x1234,cs=0x50,efl=0x20202)
 for _ in range(17):tick() # a 17 ms stretch: another thread in V86 code
 st['thr']=0x111;client(0x401234);tick();frame(80,60);tick();frame(20,3) # a slow gameplay frame, then a menu frame
 assert ioctl(2,out=0x20000)>8 # read
 S=lambda o:g.word(smp+o)
 # layout: 12 words, cat[3][8] at 48, eng at 144, frames at 240, frame_ms 252, run_ms 264, run_n 276, page at 288
 cat=lambda c,k:S(48+4*(c*8+k));frames=[S(240+4*i) for i in range(3)];fms=[S(252+4*i) for i in range(3)];run=[S(264+4*i) for i in range(3)]
 assert frames==[1,1,1] and fms==[40,80,20],(frames,fms)
 assert [cat(0,k) for k in range(8)]==[1,1,1,2,1,1,0,1],[cat(0,k) for k in range(8)] # wait, VxD, HAL, code x2, 16-bit; other thread counted as OCODE below
 assert cat(1,3)==1 and cat(2,7)==0 and S(12)==0 and S(8)>=10,(cat(1,3),S(12),S(8)) # .524: nothing late (the RTC's interrupt)
 assert run[:2]==[20,40] and S(4*7+0)>=0
 pages=[S(288+4*i) for i in range(1024)];k=pages.index(0x401);pn=lambda i,c:S(288+4096+8*i+4*c)
 assert pn(k,0)==2 and pn(k,1)==1,(pn(k,0),pn(k,1))
 L=[S(4*(5120+i)) for i in range(8192)];LN=lambda key:S(4*(5120+8192+L.index(key))) # .534: the 64-byte lines after tw_all (word 5120), their counts after them, line_other last
 assert LN(0x401234>>6)==3 and LN(0xA0001234>>6)==1 and sum(S(4*(5120+8192+i)) for i in range(8192))==4 and S(4*(5120+16384))==0,'.534: the render thread\'s ring-3 lines, gameplay frames only (the menu frame\'s tick not counted)'
 ob=288+4096+8192+8 # opage
 opages=[S(ob+4*i) for i in range(256)];assert 0xBFF71 in opages
 g.call('VX_smp_close',(0x55,));assert g.call('vx_smp_active')==1;g.call('VX_smp_close',(0x77,));assert g.call('vx_smp_active')==0
 assert g.cmos[0x0A]==0x26 and g.cmos[0x0B]==0x02,g.cmos # .524: A and B as they were
 n=g.word(smp+8);ioctl(2,out=0x20000);n=g.word(smp+8);tick();ioctl(2,out=0x20000);assert g.word(smp+8)==n # stopped: an interrupt after it takes no sample
 hb=288+4096+8192+8+1024+2048+8+1001*4 # v10.438: cs16 after hist
 cs16=[S(hb+4*i) for i in range(16)];k16=cs16.index(0x1F);assert S(hb+64+8*k16)==1,(cs16,S(hb+64+8*k16)) # the 16-bit tick, by its selector
 B=hb+204;bh=[S(B+4*i) for i in range(8)];assert bh==[0,0,0,0,1,0,0,0] and S(B+32)==18 and S(B+132)==17,(bh,S(B+32),S(B+132)) # v10.439: one 18 ms stretch (1 ms in a shared DLL, then 17 in V86), by length, V86 in it
 assert S(B+36)==0x85500021 and S(B+68)==1 and S(B+100)==18,(hex(S(B+36)),S(B+68),S(B+100)) # by the render thread's last call
 assert S(B+136)>0 # its time inside the VxD measured (the first gameplay frame)
 st['thr']=0x333;g.call('VX_smp_set_render');assert g.word(render)==0x333
 cases.append('v10.434: the sampler classes each tick (render thread in the VxD waiting or not, in VXDDRAW, its code by page, 16-bit; other threads), adds a frame\'s ticks at its flip by packets and time with the flipping thread\'s run time, and stops at its starter\'s close; .520: every tick also by category in all and time-weighted (the time since the tick before), the gaps by where it landed, the in-VxD ticks by the VxD\'s phase, the TSC rate; the calibration spin marks the render thread; .534: the render thread\'s ring-3 code by 64-byte line, gameplay frames only')
 g2=RtcGPU(32,busy_pie=True);g2.hook('Get_Cur_Thread_Handle',lambda:0x111);g2.hook('Get_Cur_VM_Handle',lambda:VM);g2.u.mem_map(VM,0x2000);g2.u.mem_write(VM+8,pack([CL]))
 g2.u.mem_write(inb,pack([1,0xA0000000,0xF3000,1]));assert g2.call('VX_smp_ioctl',(inb,16,smp,0x10,0x77))==8 and g2.word(smp)==0x549 and g2.word(smp+4)==0 and g2.call('vx_smp_active')==0
 assert g2.cmos[0x0A]==0x26 and g2.cmos[0x0B]==0x42 and not any(r in (0x0A,0x0B) for r,v in g2.cmos_log),(g2.cmos,g2.cmos_log)
 cases.append('.524: the sampler on the RTC\'s interrupt only (asked for the old time-out clock or not): IRQ 8 virtualized, A rate 6 and B\'s PIE set at the start and put back at the stop, an interrupt after the stop takes nothing; with B\'s PIE already set (another user) it does not start and writes nothing')
 return cases

def lock_checks():
 """v10.432: VX_IOCTL_LOCKMEM: the shared-arena range is locked once whoever asks, each handle that asked holds it, a handle's close drops
 its hold, the last close releases it; out-of-arena, oversized and switched-off requests are refused; a new range releases the old."""
 cases=[];g=NativeGPU(32);locks=[];unlocks=[]
 g.hook('__LinPageLock',lambda:(locks.append((g.arg(0),g.arg(1),g.arg(2))),1)[1])
 g.hook('__LinPageUnLock',lambda:(unlocks.append((g.arg(0),g.arg(1),g.arg(2))),1)[1])
 out=ARENA+0x3100
 def req(op,a,n,h):g.call('VX_lock_request',(op,a,n,h,out));return [g.word(out+4*i) for i in range(4)]
 st=lambda:[g.word(g.addr('vx_lock_stat')+4*i) for i in range(6)]
 pg=lambda s,n:[(s+i,1,0) for i in range(n)] # v10.435: a page at a time
 assert req(1,0xA0000000,0xF3000,0x11)==[0xF3,0xA0000000,1,1] and locks==pg(0xA0000,0xF3)
 assert req(1,0xA0000000,0xF3000,0x22)==[0xF3,0xA0000000,2,1] and len(locks)==0xF3 # a second process: held, not locked again
 assert req(1,0xA0000000,0xF3000,0x22)[2]==2 # the same handle again: one hold
 g.call('VX_lock_close',(0x11,));assert not unlocks and st()[5]==1
 g.call('VX_lock_close',(0x33,));assert not unlocks # a handle that never asked
 g.call('VX_lock_close',(0x22,));assert unlocks==pg(0xA0000,0xF3) and st()[0]==0
 assert req(1,0x0040000,0x1000,0x11)[0]==0 and req(1,0xC0000000,0x1000,0x11)[0]==0 and req(1,0xBFFFF000,0x2000,0x11)[0]==0 and req(1,0xA0000000,0x801000,0x11)[0]==0
 assert len(locks)==0xF3 and st()[3]==4,st()
 del locks[:],unlocks[:]
 assert req(1,0xA0000800,0x1000,0x11)==[2,0xA0000000,1,1] and locks==pg(0xA0000,2) # an unaligned range: both pages
 assert req(1,0xA1000000,0x3000,0x11)[0]==3 and unlocks==pg(0xA0000,2) and locks[2:]==pg(0xA1000,3) # a new range: the old released
 f0=st()[3];n=len(locks);assert req(1,0xA1000000,0x3000,0x22)[:3]==[3,0xA1000000,2] and len(locks)==n
 assert req(1,0xA2000000,0x1000,0x22)[:3]==[3,0xA1000000,2] and len(locks)==n and st()[3]==f0+1 # v10.466: another range while another handle holds it: refused, the lock kept
 g.call('VX_lock_close',(0x22,))
 del unlocks[:];assert req(0,0,0,0x11)[0]==0 and unlocks==pg(0xA1000,3) # an explicit release
 g.u.mem_write(g.addr('vx_lock_hal'),pack([0]));n=len(locks);assert req(1,0xA0000000,0x1000,0x11)[0]==0 and len(locks)==n # ACCEL_LOCK_HAL 0
 g.u.mem_write(g.addr('vx_lock_hal'),pack([1]));del locks[:],unlocks[:]
 g.hook('__LinPageLock',lambda:(locks.append((g.arg(0),g.arg(1),g.arg(2))),0 if len(locks)==5 else 1)[1]) # the fifth page refused
 f0=st()[3];assert req(1,0xA0000000,0x8000,0x11)[:3]==[0,0,0] and unlocks==pg(0xA0000,4)[::-1] and st()[3]==f0+1,(unlocks,st()) # the four done unlocked again: nothing held
 cases.append('v10.432: VX_IOCTL_LOCKMEM locks a shared-arena range once, held per handle, released at the last close; bad ranges, ACCEL_LOCK_HAL 0 and a refused lock hold nothing; v10.435: a page at a time, the pages done unlocked again when one is refused')
 return cases

def prof_checks():
 """v10.329: the 3D path's profile (TSC readings, latency distributions, register-access counts) only with vx_prof set (ACCEL_PROFILE,
 TEXDBG 6C or a clearing request); off, a draw adds no cycles, no register counts and no latency samples, and still draws."""
 cases=[]
 for bpp in (32,16):
  g=NativeGPU(bpp);assert g.word(g.addr('vx_prof'))==0
  assert g.submit(packet(bpp))==0
  pr=[g.word(g.addr('vx3dn_prof')+4*k) for k in range(8)];lat=[g.word(g.addr('vx3dn_lat')+k*(12+124*4)) for k in range(4)]
  assert pr[1]==0 and pr[2]==0 and pr[3]==0 and pr[4]==0 and lat==[0,0,0,0],(pr,lat)
  g.u.mem_write(g.addr('vx_prof'),pack([1]));assert g.submit(packet(bpp))==0
  pr=[g.word(g.addr('vx3dn_prof')+4*k) for k in range(8)];lat=[g.word(g.addr('vx3dn_lat')+k*(12+124*4)) for k in range(4)]
  assert pr[2]>0 and lat[0]>0 and lat[3]>0,(pr,lat)
  cases.append(f'{bpp}bpp the 3D profile is off by default (a draw adds no cycles, register counts or latency samples) and counts with vx_prof set')
 return cases

def query_checks():
 """v10.322 (review: another context's drawing was counted into an occlusion query; another query set the one count to 0): the count is
 one owner's at a time; a draw from another thread puts it aside and the owner's next draw starts it again; another owner's begin is BUSY;
 ops 2 and 3 (the owner's context not current, current again) do the same for the owner's own thread."""
 cases=[]
 class QGPU(FetchGPU):
  def __init__(s,bpp):
   super().__init__(bpp);s.count=0;s.a21=0;s.thread=0x1111;s.hook('Get_Cur_Thread_Handle',lambda:s.thread)
  def write(s,u,t,a,z,v,_):
   if a-MM==0x440:
    if v>>24==0x1C:s.count&=~0xFFFFFF
    elif v>>24==0x1D:s.count&=0xFFFFFF
    elif v>>24==0x21:s.a21=v&0xFFFFFF
    elif v>>24==0x22 and (s.a21 or v&0xFF):u.mem_write(FB+((s.a21<<2)|((v&0xFF)<<26)),pack([s.count]))
   return super().write(u,t,a,z,v,_)
  def draw(s,n):
   rc=s.submit(packet(s.bpp));
   if rc==0:s.count+=n
   return rc
  def query(s,op,owner):
   s.u.mem_write(ARENA+0xc800,pack([0xEEEE]));rc=s.call('VX3DN_query',(op,owner,ARENA+0xc800));return rc,s.word(ARENA+0xc800)
 for bpp in (32,16):
  g=QGPU(bpp)
  assert g.query(0,7)==(0,0) and g.draw(100)==0
  g.thread=0x2222;assert g.draw(1000)==0 and g.query(0,9)[0]==3,'another owner begins while 7 counts: BUSY (it counts in software)'
  g.thread=0x1111;assert g.draw(100)==0
  assert g.query(1,7)==(0,200),g.query
  assert g.word(g.addr('vx3dn_query_switch'))==1 and g.critical==0
  cases.append(f'{bpp}bpp an occlusion query counts its own thread\'s draws only: another thread\'s draw puts the count aside, the owner\'s next draw starts it again; another owner\'s query is BUSY')
  assert g.query(0,9)==(0,0) and g.draw(50)==0 and g.query(2,9)[0]==0 and g.draw(500)==0 and g.query(3,9)[0]==0 and g.draw(25)==0
  assert g.query(1,9)==(0,75) and g.query(1,9)[0]==2,'ended: a second end is UNSUPPORTED'
  assert g.query(0,5)==(0,0) and g.query(4,5)==(0,0) and g.query(0,6)==(0,0),'a context deleted mid-query gives the count up'
  cases.append(f'{bpp}bpp the owner\'s context not current (op 2) and current again (op 3): its own thread\'s draws in between are left out; a deleted context gives the count up (op 4)')
  cl=packet(bpp,op=2);assert g.query(4,6)==(0,0) and g.query(0,8)==(0,0) and g.draw(100)==0
  assert g.submit(cl)==0;g.count+=5000 # the engine counted the clear's pixels (ACCEL_3D_CLEAR3D)
  assert g.draw(40)==0 and g.query(1,8)==(0,140),'v10.456 (finding B): a clear on the owner\'s thread is not counted'
  cases.append(f'{bpp}bpp v10.456: a clear on the query owner\'s own thread puts the count aside; its pixels are not in the query\'s count')
 return cases

if __name__=='__main__':
 cases=checks();(ROOT/'verification'/'NATIVE3D.json').write_text(json.dumps({'scope':__doc__,'passed':len(cases),'cases':cases},indent=2)+'\n');print(f'{len(cases)} linked native-VxD checks passed.')
