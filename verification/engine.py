#!/usr/bin/env python3
"""Execute the built LE VxD against synthetic OS services and a 2D recipe model.
These tests cannot establish Windows 98 boot or physical GPU correctness.
"""
import struct,json,random
from validate import LEImage,ROOT,u32
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_INSN,UC_HOOK_MEM_WRITE,UC_HOOK_MEM_READ
from unicorn.x86_const import *
IMAGE=LEImage()
FB,MM,ARENA=0x2000000,0x5000000,0x3000000
GDI_SEL=0x30 # v10.273: the DRV binds selector:offset; the model's selector 30h has base ARENA (self.selbase)
BIND=(GDI_SEL<<16|0x800,GDI_SEL<<16|0x802)
BATCH=0x6800000 # .559: a batch call's packets (32 of 32,380 bytes pass the arena's megabyte)
SRC=0x6000000 # v10.99: the DIB bits a StretchDIBits request points at
pack=lambda a:struct.pack('<%dI'%len(a),*a)
def uses(rop,k):return any(((rop>>i)&1)!=((rop>>(i^k))&1) for i in range(8))
def rop3(rop,p,s,d,mask):
 # ROP bit index = P<<2|S<<1|D (VX2ROP: the engine does every ROP3 exactly)
 n=0
 for i in range(8):
  if (rop>>i)&1:n|=(p if i&4 else ~p)&(s if i&2 else ~s)&(d if i&1 else ~d)
 return n&mask
class GPU:
 def busy3d(self): # .575: counted (the status query must not reach it: it starts the 3D state cache over)
  self.busy3d_calls=getattr(self,'busy3d_calls',0)+1;return int(self.lab)
 def __init__(self,bpp=32):
  self.image=IMAGE;self.u=Uc(UC_ARCH_X86,UC_MODE_32);u=self.u
  for at,n in [(0,0x200000),(FB,0x1000000),(MM,0x400000),(ARENA,0x100000),(SRC,0x200000),(BATCH,0x200000)]:u.mem_map(at,n)
  for at,o in zip(IMAGE.bases,IMAGE.objects):u.mem_map(at,len(o['data']));u.mem_write(at,bytes(o['data']))
  self.bpp=bpp;self.px=bpp//8;self.pitch=64*self.px;self.t=0;self.sqi=0x17;self.seq={0x10:0xa0,0x3c:16,0x4e:0};self.reg={}
  self.crt={};self.cri=0x0a;self.vbn=0;self.vb_enabled=True;self.bios_mode='normal';self.bios_shift=2;self.bios_max_y=10**9;self.bios_log=[];self.vb_period=1000
  self.commands=[];self.writes=[];self.host_cmd=None;self.host=[];self.host_need=0;self.excluded=0;self.critical=0;self.busy=False;self.timeout=False;self.stuck=False;self.gone=False;self.lab=False;self.async_busy=0;self.pend=None;self.busy_left=0;self.hazards=[]
  self.selbase={GDI_SEL:ARENA};self.hda=ARENA;u.mem_write(self.addr('hda'),pack([self.hda]));h=[0]*80
  h[0]=320;h[3:9]=[64,48,bpp,self.pitch,0,self.pitch*48];h[10]=FB;h[13]=0x1000000;h[15]=0x1000000
  u.mem_write(self.hda,pack(h));s=[0]*31;s[3]=6;s[8]=0xf0000000;s[12]=FB;s[18:21]=[64,48,bpp];s[22]=self.pitch;s[24]=1
  u.mem_write(self.addr('vx_status'),pack(s))
  if 'vx_chip_id' in self.image.symbols:u.mem_write(self.addr('vx_chip_id'),pack([getattr(self,'chip_id',0x5122)])) # .579: the device (the checks model the VX855 unless one sets chip_id first)
  self.u.mem_write(ARENA+0x800,struct.pack('<HH',0x8000,1))
  for name,fn in [('__MapPhysToLinear',self.mapping),('Get_System_Time',self.tick),('Get_System_Time_Address',self.st_address),('_Begin_Critical_Section',self.enter),('_End_Critical_Section',self.leave),('VX3D_busy',self.busy3d),('FBHDA_access_begin',self.begin),('VESA_draw_needs_exclusion',self.needs_excl),('FBHDA_access_end',self.end),('FBHDA_access_rect',self.begin),('vesa_bios',self.bios),('load_client_state',self.client_state),('__LinPageLock',self.pagelock),('__LinPageUnLock',self.pageunlock),('Get_Sys_VM_Handle',lambda:0xC1000000),('Get_Cur_Thread_Handle',lambda:0x1111),('Get_Thread_Exec_Time',lambda:0),('__SelectorMapFlat',self.selmap)]:self.hook(name,fn)
  self.pages=[];self.page_ok=True
  u.hook_add(UC_HOOK_INSN,self.port_in,None,1,0,UC_X86_INS_IN);u.hook_add(UC_HOOK_INSN,self.port_out,None,1,0,UC_X86_INS_OUT)
  u.hook_add(UC_HOOK_MEM_READ,self.read,None,MM+0x400,MM+0x403);u.hook_add(UC_HOOK_MEM_READ,self.read_fire,None,MM+0x298,MM+0x29b);u.hook_add(UC_HOOK_MEM_READ,self.read_hqv,None,MM+0x3D0,MM+0x3D3);self.fire_hold=0;self.fire_reads=0;u.hook_add(UC_HOOK_MEM_WRITE,self.write,None,MM,MM+0x3fffff)
  # The shipped defaults keep small operations on the CPU (v10.58c); these model tests exercise the engine recipes, so start with no size thresholds.
  for n in ('vx2d_fill_min','vx2d_copy_min','vx2d_force','vx2d_line_min'):u.mem_write(self.addr(n),pack([0]))
  u.mem_write(self.addr('vx2d_keep'),pack([0])) # v10.145: ACCEL_KEEP ships on; these recipe models check the per-call open/close (keep.py covers the kept context)
  u.mem_write(self.addr('vx2d_brush_off'),pack([0xFFF000])) # v10.77: 4 KB reserved at the end of the 16 MB the model has
  u.mem_write(self.addr('vx2d_fpat'),pack([1])) # ACCEL_PATTERN ships off by default (unverified on hardware); the recipe model covers mono/colour ops
  assert self.call('VX2D_bind',BIND)==1
 def addr(self,n):return self.image.symbols[n]
 def word(self,a):return u32(self.u.mem_read(a,4),0)
 def arg(self,i):return self.word(self.u.reg_read(UC_X86_REG_ESP)+4+4*i)
 def hook(self,n,fn):
  # v10.389: hooking a name again replaces its function (one hook an address: two would each return to the caller)
  if not hasattr(self,'_hooked'):self._hooked={}
  if n in self._hooked:self._hooked[n][0]=fn;return
  cell=[fn];self._hooked[n]=cell
  def run(u,a,z,_):
   v=cell[0]();sp=u.reg_read(UC_X86_REG_ESP);dest=self.word(sp)
   if v is not None:u.reg_write(UC_X86_REG_EAX,v)
   u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,dest)
  self.u.hook_add(UC_HOOK_CODE,run,None,self.addr(n),self.addr(n))
 def mapping(self):assert (self.arg(0),self.arg(1),self.arg(2))==(0xf0000000,0x400000,0);return MM
 def selmap(self):assert self.arg(0)==0xC1000000 and self.arg(2)==0;return self.selbase.get(self.arg(1),0xFFFFFFFF)
 def pagelock(self):self.pages.append(('lock',self.arg(0),self.arg(1),self.arg(2)));return 1 if self.page_ok else 0
 def pageunlock(self):self.pages.append(('unlock',self.arg(0),self.arg(1),self.arg(2)));return 1
 def tick(self):
  self.t+=1
  if getattr(self,'_stcell',None):self.u.mem_write(self._stcell,self.t.to_bytes(4,'little')) # v10.448: the VMM's time word (Get_System_Time_Address) as of the last read
  return self.t
 def st_address(self): # v10.448
  if not getattr(self,'_stcell',None):self._stcell=0x7FFF0000;self.u.mem_map(self._stcell,0x1000);self.u.mem_write(self._stcell,self.t.to_bytes(4,'little'))
  return self._stcell
 def enter(self):self.critical+=1;return 0
 def leave(self):self.critical-=1;assert self.critical>=0;return 0
 def needs_excl(self): # v10.371: the cursor model: excl_rule None = offset 0 (the old rule), else the answer given
  r=getattr(self,'excl_rule',None);return int(self.arg(0)==0) if r is None else int(r)
 def begin(self):self.excluded+=1;self.begins=getattr(self,'begins',0)+1;return 0
 def end(self):self.excluded-=1;assert self.excluded>=0;return 0
 def ext_open(self):return bool(self.seq.get(0x10,0)&1) # SR10 bit 0 opens the extended CRTC registers
 def port_in(self,u,p,z,_):
  assert z==1
  if p==0x3c4:return self.sqi
  if p==0x3c5:
   v=self.seq.get(self.sqi,0)
   if self.sqi==0x6f and v&0x80 and not getattr(self,'tlb_stuck',False):self.seq[0x6f]=v&0x7f # v10.426: the GTI cache flush (SR6F bit 7) clears itself, as on the board (VXD3DMON: "SR6F bit 7 cleared"); the request read once still set
   return v
  if p==0x3c9:
   m=self.__dict__.setdefault('palhw',[[0,0,0] for _ in range(256)]);v=m[getattr(self,'ri',0)][getattr(self,'rph',0)];self.rph=getattr(self,'rph',0)+1
   if self.rph==3:self.rph=0;self.ri=(getattr(self,'ri',0)+1)&255
   return v
  if p==0x3cc:return 0x67
  if p==0x3d4:return self.cri
  if p==0x3d5:
   if self.cri in (0x34,0x48,0x62) and not self.ext_open():return 0
   return self.crt.get(self.cri,0)
  if p==0x3da:
   self.vbn+=1;return 8 if self.vb_enabled and self.vbn%self.vb_period>=self.vb_period*9//10 else 0
  raise AssertionError(('unexpected port read',hex(p)))
 def port_out(self,u,p,z,v,_):
  assert z==1
  if p==0x3c4:self.sqi=v;return
  if p==0x3d4:self.cri=v;return
  if p==0x3d5:
   if self.cri in (0x34,0x48,0x62) and not self.ext_open():return
   self.crt[self.cri]=v;self.crlog=getattr(self,'crlog',[])+[self.cri]
   if self.cri==0x34:self.commit=self.regs_start() # CR34 is the fire register: the chip takes the whole address when it is written
   return
  if p in (0x3c6,0x3c7,0x3c8,0x3c9): # VGA DAC (8bpp palette): logged, and modelled with auto-incrementing indices
   self.dac=getattr(self,'dac',[])+[(p,v)];m=self.__dict__.setdefault('palhw',[[0,0,0] for _ in range(256)])
   if p==0x3c8:self.wi=v;self.wph=0
   elif p==0x3c7:self.ri=v;self.rph=0
   elif p==0x3c9:
    m[self.wi][self.wph]=v;self.wph+=1
    if self.wph==3:self.wph=0;self.wi=(self.wi+1)&255
   return
  assert p==0x3c5,hex(p);self.seq[self.sqi]=v
  if self.sqi==0x6f and v&0x80:self.tlb_flushes=getattr(self,'tlb_flushes',0)+1 # v10.426
  if self.sqi==0x4e and not(v&12) and not self.stuck:self.busy=False;self.pend=None
 def regs_start(self):return ((self.crt.get(0x48,0)&0x1f)<<24)|(self.crt.get(0x34,0)<<16)|(self.crt.get(0x0c,0)<<8)|self.crt.get(0x0d,0)
 def crt_start(self):return getattr(self,'commit',0) # the committed scan-out address, not the readable registers
 def bios(self):
  regs=self.arg(0);r=lambda o:u32(self.u.mem_read(regs+o,4),0);eax,ebx,ecx,edx=r(0x1c),r(0x10),r(0x18),r(0x14)
  self.bios_log.append((eax,ebx,ecx,edx));ok=eax==0x4f07 and ebx==0
  h=u32(self.u.mem_read(self.hda+16,4),0);pitch=u32(self.u.mem_read(self.hda+24,4),0);by=u32(self.u.mem_read(self.hda+20,4),0)//8
  m=self.bios_mode
  if m=='refuse' or edx>self.bios_max_y:ok=False
  if ok and m=='normal':
   v=(edx*pitch+ecx*by)>>self.bios_shift;self.crt[0x0d]=v&255;self.crt[0x0c]=(v>>8)&255;self.crt[0x34]=(v>>16)&255;self.crt[0x48]=(self.crt.get(0x48,0)&~3)|((v>>24)&3);self.commit=v
  elif ok and m=='other':self.crt[0x62]=((edx*pitch+ecx*by)>>16)&255 # a different layout (second controller): the CRTC we know does not move
  self.u.mem_write(regs+0x1c,pack([0x4f if ok else 0x14f]))
 def client_state(self):self.u.mem_write(self.arg(0),bytes(0x6c))
 def read(self,u,t,a,z,v,_):
  if self.pend is not None and getattr(self,'late_left',0)>0: # .592: the command regulator still holds the command: only its bit (4) shows
   self.late_left-=1;u.mem_write(MM+0x400,pack([getattr(self,'late_status',0x10)]));return
  if self.pend is not None:
   if self.busy_left<=0:c=self.pend;self.pend=None;self.run(c)
   else:self.busy_left-=1
  u.mem_write(MM+0x400,pack([0xffffffff if self.gone else (0x1ff7 if (self.busy or self.pend is not None or self.host_cmd is not None) else 0)]))
 def read_hqv(self,u,t,a,z,v,_): # v10.108: the HQV control register reads as idle, with no flip pending
  w=struct.unpack('<I',bytes(u.mem_read(MM+0x3D0,4)))[0];u.mem_write(MM+0x3D0,pack([(w|8)&~(0x10|0x8000)]))
 def read_fire(self,u,t,a,z,v,_): # v10.106: the video command is loaded at the next vertical blank: the fire bits clear once the register has been read fire_hold times
  self.fire_reads+=1
  if self.fire_hold>0:self.fire_hold-=1;return
  w=struct.unpack('<I',bytes(u.mem_read(MM+0x298,4)))[0]
  if w&0xC0000000:u.mem_write(MM+0x298,pack([w&~0xC0000000]))
 def write(self,u,t,a,z,v,_):
  at=a-MM
  if 0x200000<=at<0x400000: # v10.116: the 2D host window: data for the host-fed command that is waiting
   assert z==4 and self.host_cmd is not None,('host data without a host command',hex(at));self.host.append(v)
   if len(self.host)==self.host_need:c=self.host_cmd;self.host_cmd=None;self.host_run(c)
   return
  assert z==4 and (at<=0x5c or 0x100<=at<0x200 or at in (0x1CC,0x228,0x22C,0x28C,0x2F0,0x370,0x374,0x378,0x37C,0x380,0x384,0x388,0x38C,0x394,0x3B0,0x3B4,0x3B8,0x3CC,0x3D0,0x3D4,0x3D8,0x3E0,0x3E4,0x3E8,0x3EC,0x3F0,0x3F4,0x3F8,0x3FC,0x220,0x230,0x234,0x238,0x248,0x284,0x288,0x23C,0x240,0x244,0x24C,0x250,0x254,0x258,0x264,0x26C,0x298,0x208,0x20C,0x210,0x224,0x260,0x268,0x270,0x274,0x278,0x2D0,0x2D4,0x2D8,0x2DC,0x2E0,0x2E4,0x2E8,0x2EC,0x2F0,0x2F4,0x2F8,0x2FC,0x120C)),hex(at) # v10.105 (v10.128: the legacy mono cursor 0x2D0..0x2E0): the hardware cursor registers
  if self.pend is not None and at:self.hazards.append((at,v)) # a register written while a command runs would corrupt it
  self.writes.append((at,v));self.reg[at]=v
  if at:return
  self.commands.append(v)
  if self.timeout:self.busy=True;return
  if v&0x40: # v10.116: a host-fed command runs when its data has arrived (rows of 32-bit words)
   w=(self.reg[12]&0xffff)+1;h=(self.reg[12]>>16)+1
   if v==0xCC000041: # v10.507: the colour form (ACCEL_DIBHOST): rows of w pixels in dwords, the screen's format, no clip
    self.host_cmd=v;self.host=[];self.host_need=h*((w*self.px+3)//4);self.host_cmds=getattr(self,'host_cmds',0)+1;return
   assert v&0xF==1 and v&0x100 and (v>>17)&7==5 and v>>24==0xCC,hex(v) # mono source, dword rows, SRCCOPY: the recipes the driver uses
   self.host_cmd=v;self.host=[];self.host_need=h*((w+31)//32);self.host_cmds=getattr(self,'host_cmds',0)+1
   return
  if getattr(self,'late',0):self.pend=v;self.busy_left=2;self.late_left=self.late;return # .592
  if self.async_busy:self.pend=v;self.busy_left=self.async_busy;return # the effect appears when the status reads have found it busy async_busy times
  self.run(v)
 def run(self,v):
  u=self.u;r=self.reg;cmd=v
  if cmd&15==5:self.line(cmd);return
  assert cmd&15==1
  px=1 if r[4]==0 else 2 if r[4]==0x100 else 4;assert px==self.px
  dp=(r[8]>>16)*8;sp=(r[8]&0x7ff)*8;db=r[0x14]*8;sb=r[0x1c]*8
  w=(r[12]&0xffff)+1;h=(r[12]>>16)+1;dx=r[16]&0xffff;dy=r[16]>>16;sx=r.get(24,0)&0xffff;sy=r.get(24,0)>>16
  mask=(1<<(px*8))-1;compare_mask=0xFFFFFF if px==4 else mask;xi=-1 if cmd&0x8000 else 1;yi=-1 if cmd&0x4000 else 1
  def pixel(base,pitch,x,y):return int.from_bytes(u.mem_read(FB+base+pitch*y+x*px,px),'little')
  for yy in range(h):
   for xx in range(w):
    x,y=dx+xx*xi,dy+yy*yi
    if cmd&0x1000 and not((r[64]&65535)<=x<=(r[68]&65535) and (r[64]>>16)<=y<=(r[68]>>16)):continue
    d=pixel(db,dp,x,y);p=r[88]&mask;s=0
    rop=cmd>>24;us=uses(rop,2);up=uses(rop,4)
    if us:
     s=pixel(sb,sp,sx+xx*xi,sy+yy*yi)
     if rop==0xcc:
      if r[72]&0x4000 and (s&compare_mask)==(r[80]&compare_mask):continue
      if r[72]&0x8000 and (d&compare_mask)!=(r[76]&compare_mask):continue
     else:assert not(r[72]&0xC000) # keys stay with SRCCOPY
     if up and not cmd&0x2000: # .595 (not yet measured on a board): the colour block as the pattern of a ROP that also reads the source, phased as the colour pattern fill
      base=(r.get(0x20,0)&0x3ffffff)*8;xo=(r.get(0x20,0)>>26)&7;yo=(r.get(0x20,0)>>29)&7;assert base%16==0 and r.get(0x20,0)!=0 and cmd&0xA00==0
      col=(xx+xo)&7;row=(yy+yo)&7
      if px==1:row^=(((dx&7)-xo+col)//8)&1
      p=int.from_bytes(u.mem_read(FB+base+(row*8+col)*px,px),'little')
     else:assert bool(cmd&0x2000)==up and r.get(0x20,0)==0 # a pattern ROP with a source and a solid colour is the fixed colour form (VX2ROP T1), a pattern-free one the plain form (T2)
    elif cmd&0x200:
     row=yy&7;bits=(r[36 if row<4 else 40]>>((row&3)*8))&255 # measured: byte k of the 64 bits is row k of the rectangle, bit 7 is the leftmost pixel, X is anchored to the screen
     bit=(bits>>(7-(x&7)))&1
     if not bit and cmd&0x10000:continue
     p=(r[88] if bit else r[92])&mask
    elif cmd&0x800:p=r[0x100+4*((yy&7)*8+(xx&7))]&mask
    elif up and cmd&0xA00==0 and cmd&0x2000==0:
     # v10.77, measured with VX2COL: colour pattern from an 8x8 block in video memory, 16-byte aligned, row-major; it starts at the rectangle's corner, the offset bits add to
     # column and row; at 8bpp the row flips within its pair when floor(((x0&7)-xo+col)/8) is odd (zero whenever xo == x0&7, which is what the driver programs)
     base=(r.get(0x20,0)&0x3ffffff)*8;xo=(r.get(0x20,0)>>26)&7;yo=(r.get(0x20,0)>>29)&7;assert base%16==0 and r.get(0x20,0)!=0
     col=(xx+xo)&7;row=(yy+yo)&7
     if px==1:row^=(((dx&7)-xo+col)//8)&1
     p=int.from_bytes(u.mem_read(FB+base+(row*8+col)*px,px),'little')
    new=rop3(rop,p,s,d,mask)
    if px==4 and r[72]&0xC000:new=(new&0xFFFFFF)|(d&0xFF000000)
    u.mem_write(FB+db+y*dp+x*px,(new&mask).to_bytes(px,'little'))
 def host_run(self,cmd):
  if cmd==0xCC000041:
   r=self.reg;u=self.u;px=self.px;assert r[4]==(0x300 if px==4 else 0x100) and r.get(0x18,0)==0 and r[0x14]==0
   dp=(r[8]>>16)*8;w=(r[12]&0xffff)+1;h=(r[12]>>16)+1;dx=r[16]&0xffff;dy=r[16]>>16;n=(w*px+3)//4
   for yy in range(h):
    row=b''.join(struct.pack('<I',self.host[yy*n+j]) for j in range(n))
    u.mem_write(FB+(dy+yy)*dp+dx*px,row[:w*px])
   return
  # cookbook "host-fed mono bitmap": each row is (w+31)//32 words, the first 8 pixels in the low byte of the first word, bit 7 = leftmost; 1 bits colour 4C, 0 bits colour 50 (bit 10: left alone); bit 12 clip
  r=self.reg;u=self.u;px=1 if r[4]==0 else 2 if r[4]==0x100 else 4;assert px==self.px and px>1
  dp=(r[8]>>16)*8;db=r[0x14]*8;mask=(1<<(px*8))-1;w=(r[12]&0xffff)+1;h=(r[12]>>16)+1;dx=r[16]&0xffff;dy=r[16]>>16;n=(w+31)//32
  for yy in range(h):
   row=b''.join(struct.pack('<I',self.host[yy*n+j]) for j in range(n))
   for xx in range(w):
    x,y=dx+xx,dy+yy;bit=(row[xx>>3]>>(7-(xx&7)))&1
    if cmd&0x1000 and not((r[64]&65535)<=x<=(r[68]&65535) and (r[64]>>16)<=y<=(r[68]>>16)):continue
    if not bit and cmd&0x400:continue
    assert 0<=x<dp//px,('host blit outside the pitch',x)
    u.mem_write(FB+db+y*dp+x*px,((r[0x4c] if bit else r[0x50])&mask).to_bytes(px,'little'))
 def line(self,cmd):
  # v10.90, the hardware recipe of VX2LINE: n pixels (n+1 without bit 20) along the major axis, e >= 0 steps the minor axis and adds 2(m-n), else adds 2m; 14-bit signed error terms
  r=self.reg;u=self.u;px=1 if r[4]==0 else 2 if r[4]==0x100 else 4;assert px==self.px
  dc=getattr(self,'dash_conv',None);pat=bool(dc and (cmd&dc['paten'])==dc['paten'])
  assert cmd>>24==0xF0 and cmd&0x2000 and cmd&0x100000 and (pat or r[0x24]==0xFFFFFFFF),hex(cmd)
  sx14=lambda v:(v&0x3fff)-0x4000 if v&0x2000 else v&0x3fff
  dp=(r[8]>>16)*8;db=r[0x14]*8;mask=(1<<(px*8))-1
  n=r[12]&0xfff;x0=r[16]&0xffff;y0=r[16]>>16;e=sx14(r[0x2C]);hi=r[0x18]>>16;lo=sx14(r[0x18]);xi=-1 if cmd&0x8000 else 1;yi=-1 if cmd&0x4000 else 1;ym=bool(cmd&0x200000);minor=0
  self.line_pixels=getattr(self,'line_pixels',0)
  for k in range(n):
   x=x0+(minor*xi if ym else k*xi);y=y0+(k*yi if ym else minor*yi)
   if not(cmd&0x1000 and not((r[64]&65535)<=x<=(r[68]&65535) and (r[64]>>16)<=y<=(r[68]>>16))):
    pen=True
    if pat:
     L=dc['L'];j=(dc['rot']+k)%L;r24,r28=r.get(0x24,0),r.get(0x28,0)
     if L==32:b=(r24>>(31-j) if dc['msb'] else r24>>j)&1
     else:
      if not dc['msb']:b=(r24>>j)&1 if j<32 else (r28>>(j-32))&1
      else:
       t=63-j;b=(r28>>(t-32))&1 if t>=32 else (r24>>t)&1
     pen=bool(b^dc['inv'])
    if pen:u.mem_write(FB+db+y*dp+x*px,(r[0x4c]&mask).to_bytes(px,'little'));self.line_pixels+=1
    elif not(cmd&dc['trans']):u.mem_write(FB+db+y*dp+x*px,(r[dc['bgreg']]&mask).to_bytes(px,'little'))
   if e>=0:minor+=1;e+=lo
   else:e+=hi
 def call(self,n,args=(),strict=True,stdcall=False):
  u=self.u;sp=0x1f0000;u.mem_write(sp,pack([0xf0000,*args]));u.reg_write(UC_X86_REG_ESP,sp);u.reg_write(UC_X86_REG_EFLAGS,0x202)
  u.emu_start(self.addr(n),0xf0000,count=30000000)
  assert u.reg_read(UC_X86_REG_EIP)==0xf0000 and u.reg_read(UC_X86_REG_ESP)==sp+4+(4*len(args) if stdcall else 0),n # v10.476: a stdcall callee pops its arguments
  assert u.reg_read(UC_X86_REG_EFLAGS)&0x200 and self.critical==0 and self.cri==0x0a,n
  if strict:assert self.sqi==0x17 and self.seq[0x10]==0xa0,n
  return u.reg_read(UC_X86_REG_EAX)
 def submit(self,q,strict=True):self.u.mem_write(ARENA+0x1000,pack(q));return self.call('VX2D_submit',(ARENA+0x1000,),strict)
 def request(self,op=1,**kw):
  q=[0]*93;q[:5]=[372,0x10000,op,0,self.bpp];q[5:9]=[0,self.pitch,64,48];q[9:13]=q[5:9];q[13:19]=[7,6,2,3,19,13];q[23]=0x5a if self.px==1 else 0x1234 if self.px==2 else 0xff123456;q[24]=0x4321;q[25]=0xcc if op==2 else 0xf0
  fields={'flags':3,'dx':13,'dy':14,'sx':15,'sy':16,'w':17,'h':18,'l':19,'t':20,'r':21,'b':22,'fg':23,'bg':24,'rop':25,'key':26}
  for n,v in kw.items():q[fields[n]]=v
  return q
 def prefill(self,offset=0,pitch=None,w=64,h=48):
  pitch=pitch or self.pitch;mask=(1<<self.bpp)-1
  for y in range(h):
   self.u.mem_write(FB+offset+y*pitch,b''.join((((y*1981+x*383+0x8345)&mask).to_bytes(self.px,'little')) for x in range(w)))
 def snapshot(self):return bytes(self.u.mem_read(FB,0x40000))
 def reference(self,q,before):
  b=bytearray(before);op=q[2];flags=q[3];db,dp=q[5:7];sb,sp=q[9:11];dx,dy,sx,sy,w,h=q[13:19];mask=(1<<self.bpp)-1
  for y in range(h):
   for x in range(w):
    if flags&1 and not(q[19]<=dx+x<q[21] and q[20]<=dy+y<q[22]):continue
    at=db+(dy+y)*dp+(dx+x)*self.px;d=int.from_bytes(before[at:at+self.px],'little');p=q[23]&mask;s=0
    if op==2:
     if flags&32:p=q[23]&mask
     sa=sb+(sy+y)*sp+(sx+x)*self.px;s=int.from_bytes(before[sa:sa+self.px],'little')
     cm=0xFFFFFF if self.bpp==32 else mask
     if flags&2 and (s&cm)!=(q[26]&cm):pass
     elif flags&2:continue
     if flags&4 and (d&cm)!=(q[26]&cm):continue
    if op==3:
     X=dx+x;Y=dy+y;bits=q[27+(Y&7)//4]>>((3-(Y&3))*8);bit=(bits>>(X&7))&1 # the request is anchored to the screen on both axes
     if flags&8 and not bit:continue
     p=(q[23] if bit else q[24])&mask
    if op==4:p=q[29+((dy+y)&7)*8+((dx+x)&7)]&mask # v10.77: the brush is laid out for the screen, the engine is given offsets (dx&7, dy&7)
    new=rop3(q[25],p,s,d,mask)
    if self.bpp==32 and flags&6:new=(new&0xFFFFFF)|(d&0xFF000000)
    b[at:at+self.px]=(new&mask).to_bytes(self.px,'little')
  return bytes(b)

def checks():
 cases=[]
 def verify(g,q,name):
  g.prefill();before=g.snapshot();expected=g.reference(q,before);n=len(g.commands)
  assert g.submit(q)==1,name
  assert g.snapshot()==expected,name
  assert len(g.commands)==n+1 and g.excluded==0,name
  cases.append(name)
 for bpp in (16,32):
  g=GPU(bpp)
  for rop in (0,255,0x55,0x5a,0xf0):
   for c in (0,0x1f,0x7e0,0xf800,0xff987654):verify(g,g.request(rop=rop,fg=c),f'{bpp}bpp fill {rop:x} {c:x}')
  for dx in (3,7,13):
   for dy in (2,6,11):verify(g,g.request(2,sx=7,sy=6,dx=dx,dy=dy),f'{bpp}bpp overlap {dx},{dy}')
  # v10.84: every ROP3 on the engine: fills for the 16 ROPs without a source, copies (plain or fixed colour) for the 240 that use one, overlap included
  for rop in range(256):
   if not uses(rop,2):
    if rop!=0xAA:verify(g,g.request(rop=rop,fg=0x9A3C5E71 if bpp==32 else 0xA35C),f'{bpp}bpp fill rop {rop:02x}')
    if uses(rop,4):
     q=g.request(3);q[27:29]=[0x81241842,0xE77E3C99];q[25]=rop;verify(g,q,f'{bpp}bpp mono pattern rop {rop:02x}')
    continue
   fl=32 if uses(rop,4) else 0
   verify(g,g.request(2,flags=fl,sx=20,sy=6,dx=3,dy=2,fg=0x5A3CC3A5 if bpp==32 else 0x3CA5,bg=0x1F2E3D4C&((1<<bpp)-1),rop=rop),f'{bpp}bpp copy rop {rop:02x}')
   if rop%5==0:
    for dx,dy in ((7,6),(5,4),(9,8),(5,8),(9,4)):verify(g,g.request(2,flags=fl,sx=7,sy=6,dx=dx,dy=dy,fg=0x5A3CC3A5 if bpp==32 else 0x3CA5,rop=rop),f'{bpp}bpp overlapping copy rop {rop:02x} {dx},{dy}')
  for rop in (0x88,0xEE,0x66,0x33,0x44,0x11,0xBB,0xC0,0xFB,0xCA):
   q=g.request(2,flags=1+(32 if uses(rop,4) else 0),l=11,t=9,r=22,b=16,fg=0x3CA5,rop=rop);verify(g,q,f'{bpp}bpp clipped copy rop {rop:02x}')
  for bad,why in ((dict(op=1,rop=0x88),'fill with a source ROP'),(dict(op=2,rop=0xC0),'pattern ROP copy without the fixed colour'),(dict(op=2,rop=0x88,flags=2,key=5),'key with a non-SRCCOPY copy'),(dict(op=2,rop=0xC0,flags=32|2,key=5),'fixed colour with a key'),(dict(op=3,rop=0xCC),'mono pattern with a source ROP'),(dict(op=3,rop=0x55),'mono pattern that ignores the pattern')):
   q=g.request(bad['op'],flags=bad.get('flags',0),key=bad.get('key',0),rop=bad['rop']);n=len(g.writes);assert g.submit(q)==0 and len(g.writes)==n,why;cases.append(f'{bpp}bpp rejects {why}')
  for op in (1,2,3):
   q=g.request(op,flags=1,l=11,t=9,r=22,b=16);q[27:29]=[0x81241842,0xE77E3C99]
   verify(g,q,f'{bpp}bpp clipped op {op}')
  for key in (2,4):
   q=g.request(2,flags=key,key=0x8345+3*1981+2*383);verify(g,q,f'{bpp}bpp key {key}')
  for transparent in (0,8):
   q=g.request(3,flags=transparent);q[27:29]=[0x81241842,0xE77E3C99];verify(g,q,f'{bpp}bpp mono {transparent}')
  if bpp==32:
   q=g.request(4);q[29:93]=[0xff000000+i*3907 for i in range(64)];verify(g,q,'32bpp colour pattern from video memory')
  for op in (1,2):
   q=g.request(op,flags=16);q[5:9]=[0x10000,256,50,32];q[9:13]=[0x20000,384,45,40];verify(g,q,f'{bpp}bpp offscreen {op}')
  for op in (1,2): # v10.277 (VXKEY): the engine's pitch is in 16-byte steps; a pitch of 16n + 8 (destination or source) is declined, nothing written
   for dp,sp in ((264,384),(256,392)):
    q=g.request(op,flags=16);q[5:9]=[0x10000,dp,50,32];q[9:13]=[0x20000,sp,45,40];n=len(g.writes);assert g.submit(q)==0 and len(g.writes)==n,(op,dp,sp)
    if op==1:break
  cases.append(f'{bpp}bpp a pitch that is not a multiple of 16 bytes is declined (fill destination, copy destination and source)')
  for field,value in [(0,0),(1,0),(3,32),(4,8),(6,16384),(6,255),(5,1),(5,0xfffff8),(7,0),(17,0),(13,0xffffffff),(25,0xbb)]:
   q=g.request();q[field]=value;n=len(g.writes);assert g.submit(q)==0 and len(g.writes)==n,(field,value);cases.append(f'{bpp}bpp reject {field}={value:x}')
  out=ARENA+0x3000
  g.call('VX2D_control',(2,out));g.call('VX2D_control',(0xfffffffe,out));assert g.word(out+12)==2
  assert g.submit(g.request())==0;g.call('VX2D_control',(1,out));assert g.submit(g.request())==1;cases.append(f'{bpp}bpp control query preserves disabled setting')
  g.u.mem_write(g.hda+40,pack([ARENA+0x8000]));n=len(g.writes);assert g.submit(g.request())==0 and len(g.writes)==n;cases.append(f'{bpp}bpp shadow buffer rejected')
# v10.90: lines. DIBENG's rule as VX2LINE measured it (Y decreasing: a tie takes the minor step, Y increasing: it does not), end point not drawn, written independently of the engine recurrence.
 def dib_line(x0,y0,x1,y1):
  dx,dy=x1-x0,y1-y0;adx,ady=abs(dx),abs(dy);ym=ady>adx;n,m=(ady,adx) if ym else (adx,ady);sx=-1 if dx<0 else 1;sy=-1 if dy<0 else 1
  out=[];minor=0;e=2*m-n-(1 if dy>0 else 0)
  for k in range(n):
   out.append((x0+sx*minor,y0+sy*k) if ym else (x0+sx*k,y0+sy*minor))
   if e>=0:minor+=1;e-=2*(n-m)
   else:e+=2*m
  return out
 for bpp in (8,16,32):
  g=GPU(bpp);mask=(1<<bpp)-1
  if bpp==8:g.u.mem_write(g.addr('vx2d_f8'),pack([1]))
  lines=[(2,3,60,3),(60,5,2,5),(7,2,7,44),(9,44,9,2),(3,3,40,25),(40,25,3,3),(5,40,57,8),(57,8,5,40),(3,3,30,20),(30,3,3,20),(3,20,30,3),(30,20,3,3),(10,5,20,35),(20,5,10,35),(10,35,20,5),(20,35,10,5),(1,1,63,47),(63,47,1,1),(1,47,63,1),(63,1,1,47),(4,4,12,8),(12,4,4,8),(4,8,12,4),(12,8,4,4),(4,4,8,12),(8,4,4,12),(4,12,8,4),(8,12,4,4),(0,0,1,0),(0,0,0,1),(63,47,62,47),(0,47,63,47)]
  for ln in lines:
   x0,y0,x1,y1=ln;n=max(abs(x1-x0),abs(y1-y0));col=(0xff123456 if bpp==32 else 0xF81F if bpp==16 else 0xA7)&mask
   for clip in (None,(10,9,35,30)):
    g.prefill();before=g.snapshot();pitch=g.pitch;px=g.px
    q=g.request(5,flags=1 if clip else 0,dx=x0,dy=y0,sx=x1,sy=y1,w=n,h=1,fg=col,rop=0xf0)
    if clip:q[19:23]=list(clip)
    exp=bytearray(before)
    for (x,y) in dib_line(*ln):
     if clip and not(clip[0]<=x<clip[2] and clip[1]<=y<clip[3]):continue
     exp[y*pitch+x*px:y*pitch+x*px+px]=col.to_bytes(px,'little')
    c0=len(g.commands);assert g.submit(q)==1,(bpp,ln,clip);assert g.snapshot()==bytes(exp),(bpp,ln,clip);assert len(g.commands)==c0+1 and g.excluded==0
   cases.append(f'{bpp}bpp line {ln}: DIBENG tie rule, end point not drawn, with and without a clip rectangle')
  for why,kw in (('zero length',dict(sx=5,sy=6,w=0)),('rop other than F0',dict(rop=0x5a)),('end point off the surface',dict(sx=70,sy=6,w=70-7)),('width not the major length',dict(w=9)),('height not 1',dict(h=2)),('colour key flag',dict(flags=2)),('fixed colour flag',dict(flags=32))):
   q=g.request(5,dx=7,dy=6,sx=30,sy=20,w=23,h=1,fg=1,rop=0xf0)
   for k2,v2 in kw.items():q[{'flags':3,'dx':13,'dy':14,'sx':15,'sy':16,'w':17,'h':18,'fg':23,'rop':25}[k2]]=v2
   n=len(g.writes);assert g.submit(q)==0 and len(g.writes)==n,(bpp,why);cases.append(f'{bpp}bpp line rejected: {why}')
  # ACCEL_LINE_MIN: below the threshold the line stays with the CPU, above it runs native, force overrides
  g.u.mem_write(g.addr('vx2d_line_min'),pack([40]));q=g.request(5,dx=7,dy=6,sx=30,sy=20,w=23,h=1,fg=1,rop=0xf0);n=len(g.writes);assert g.submit(q)==0 and len(g.writes)==n
  q=g.request(5,dx=2,dy=6,sx=50,sy=20,w=48,h=1,fg=1,rop=0xf0);assert g.submit(q)==1;out=ARENA+0x3000;g.call('VX2D_control',(4,out));q=g.request(5,dx=7,dy=6,sx=30,sy=20,w=23,h=1,fg=1,rop=0xf0);assert g.submit(q)==1;cases.append(f'{bpp}bpp ACCEL_LINE_MIN keeps short lines on the CPU, long ones native, force overrides')
  g.u.mem_write(g.addr('vx2d_fline'),pack([0]));assert g.submit(q)==0;cases.append(f'{bpp}bpp ACCEL_LINES=0 turns the line op off')
  # .530: styled lines removed (ACCEL_DASH): a styled-line request is refused untouched
  q=g.request(5,flags=64,dx=7,dy=6,sx=30,sy=20,w=23,h=1,fg=1,bg=2,rop=0xf0);q[27]=0x0F0F0F0F;n0=len(g.writes);assert g.submit(q)==0 and len(g.writes)==n0
  q=g.request(5,flags=8,dx=7,dy=6,sx=30,sy=20,w=23,h=1,fg=1,rop=0xf0);n0=len(g.writes);assert g.submit(q)==0 and len(g.writes)==n0
  cases.append(f'{bpp}bpp styled and transparent line requests are refused (the DIB engine draws them)')
 for fault,expected in [('timeout',2),('stuck',3)]:
  g=GPU();assert g.submit(g.request())==1;g.timeout=True;g.stuck=fault=='stuck';g.prefill();before=g.snapshot()
  assert g.submit(g.request(rop=0x5a))==expected
  assert g.snapshot()==before
  assert g.submit(g.request())==(3 if expected==3 else 0)
  assert g.excluded==0 # v10.139 (review F05): the exclusion is released even when lost (the cursor stays fenced by the lost state instead, so it shows again after a recovery)
  if expected==3:assert bytes(g.u.mem_read(ARENA+0x800,4))==struct.pack('<HH',0x8010,0)
  cases.append(f'{fault}: fired XOR not replayed; future access fenced; the cursor exclusion is released (v10.139)')
 g=GPU();q=g.request();q[5:9]=[0x10000,256,50,32];g.timeout=True;g.stuck=True
 assert g.submit(q)==3 and bytes(g.u.mem_read(ARENA+0x800,4))==struct.pack('<HH',0x8010,0)
 assert g.call('VX2D_lost')==1 and g.call('VX2D_access',(1,))==3
 cases.append('unrecoverable offscreen fault fences DIB device and all primary locks')
 g=GPU();g.lab=True;n=len(g.writes);assert g.submit(g.request())==0 and len(g.writes)==n;cases.append('lab ownership prevents engine access')
 # v10.58zc: size thresholds apply to DirectDraw requests too; force and a zero threshold override them; phase counters.
 g=GPU();u=g.u;out=ARENA+0x3000
 u.mem_write(g.addr('vx2d_fill_min'),pack([131072]));u.mem_write(g.addr('vx2d_copy_min'),pack([1024]))
 for flags in (0,16):
  q=g.request(1,flags=flags);n=len(g.writes);assert g.submit(q)==0 and len(g.writes)==n,flags;cases.append(f'default threshold keeps small fill (flags {flags}) on the CPU')
 q=g.request(2,flags=16);q[17]=20;q[18]=20;n=len(g.writes);assert g.submit(q)==0 and len(g.writes)==n;cases.append('default threshold keeps a 20x20 DirectDraw copy on the CPU')
 q=g.request(2,flags=16);q[17]=40;q[18]=30;assert g.submit(q)==1;cases.append('DirectDraw copy above the copy threshold runs native')
 g.call('VX2D_control',(4,out));assert g.submit(g.request(1,flags=16))==1;cases.append('force bit overrides the thresholds for DirectDraw')
 g.call('VX2D_control',(0,out));assert g.submit(g.request(1,flags=16))==0;cases.append('clearing force restores the thresholds')
 u.mem_write(g.addr('vx2d_fill_min'),pack([0]));assert g.submit(g.request(1,flags=16))==1;cases.append('ACCEL_FILL_MIN 0 means always native')
 g=GPU();u=g.u;u.mem_write(g.addr('vx2d_fill_min'),pack([131072]));u.mem_write(g.addr('vx2d_copy_min'),pack([1024]))
 assert g.submit(g.request(1,rop=0x5A))==1 and g.submit(g.request(1,rop=0x55))==1 and g.submit(g.request(1,rop=0xF0))==0;cases.append('v10.133: a small fill whose ROP reads the screen (PATINVERT 5A, DSTINVERT 55) skips the size threshold; PATCOPY still waits for it')
 q=g.request(2,rop=0x66);q[17]=20;q[18]=20;assert g.submit(q)==1;q=g.request(2);q[17]=20;q[18]=20;assert g.submit(q)==0;cases.append('v10.133: a small copy with SRCINVERT skips the copy threshold, a small SRCCOPY does not')
 g.call('VX2D_control',(0xF0000000,out));assert g.submit(g.request(1,rop=0x5A))==0;g.call('VX2D_control',(0xF0000001,out));cases.append('v10.133: ACCEL_DREAD_ANY 0 puts every ROP back under the thresholds')
 for v in (0,1):
  g=GPU();g.call('VX2D_control',(0xF1000000|v,out));g.prefill();assert g.submit(g.request(1,rop=0x5A,w=20,h=10))==1
 cases.append('v10.133: ACCEL_OP_WAIT 0 and 1 both complete a fill')
 g=GPU();out=ARENA+0x3000;g.call('VX2D_control',(0xfffffffe,out));flags=g.word(out+12)
 g0=GPU();assert g0.submit(g0.request())==1;g0.call('VX2D_control',(0xfffffe00,out));assert g0.word(out)==0;cases.append('v10.132: the phase profile is off by default: an operation leaves no timing')
 g.call('VX2D_control',(0xF4000001,out));g.call('VX2D_control',(0xfffffffe,out));flags=g.word(out+12)
 for i in range(5):assert g.submit(g.request())==1
 assert g.submit(g.request(rop=0x12))==0
 for k in range(8):
  g.call('VX2D_control',(0xfffffe00+k,out));assert g.word(out)==5 and g.word(out+12)==flags and g.word(out+8)==1,k
 g.call('VX2D_control',(0xfffffe09,out));assert g.word(out+4)==0
 g.call('VX2D_control',(0xfffffffe,out));assert g.word(out+12)==flags and g.word(out)==5
 cases.append('phase profile counts completed native operations only and leaves the engine settings alone')

 # v10.95: asynchronous DirectDraw blits. The model engine stays busy for a few status reads after a command; the effect appears only when it has been found idle, and any register written meanwhile is a hazard.
 SAVED=[4,8,12,16,20,24,28,32,36,40,44,48,64,68,72,76,80,88,92]
 for bpp in (16,32):
  def actl(g,v):
   g.call('VX2D_control',(v,ARENA+0x3000),strict=False);return [g.word(ARENA+0x3000+4*i) for i in range(4)]
  def acnt(g):return actl(g,0xFFFFFE12)
  def warm(g):
   assert g.submit(g.request(1,flags=16,fg=0x1111&((1<<bpp)-1),dx=1,dy=1,w=3,h=3))==1 # a synchronous operation first: the 19 registers now hold known values
   g.prefill();return {r:g.reg.get(r) for r in SAVED}
  def start(g,q,busy=3):
   g.async_busy=busy;g.hazards=[];before=g.snapshot();exp=g.reference(q,before);c0=len(g.commands)
   rc=g.submit(q,strict=False);return rc,before,exp,c0
  def settled(g,exp,regs,name):
   assert g.pend is None and g.hazards==[],name
   assert g.snapshot()==exp,name
   assert g.excluded==0 and g.sqi==0x17 and g.seq[0x10]==0xa0 and g.critical==0,name
   assert {r:g.reg.get(r) for r in SAVED}==regs,name
  fill=lambda g,**kw:g.request(1,flags=16|128,fg=0x5A3C7E21&((1<<bpp)-1),dx=5,dy=4,w=40,h=30,**kw)
  cpy=lambda g,**kw:g.request(2,flags=16|128,sx=2,sy=2,dx=20,dy=10,w=30,h=25,**kw)
  def on(g):
   assert actl(g,0xFC000001)[0]>=0;st=actl(g,0xFD000001);assert acnt(g)[3]==3
  # a fill and a copy, primary surface and offscreen
  for name,mk,kw in (('fill',fill,{}),('copy',cpy,{}),('offscreen fill',fill,dict(rop=0xf0)),):
   g=GPU(bpp);regs=warm(g);on(g);q=mk(g)
   if name=='offscreen fill':q[5:9]=[0x10000,256,50,40];q[9:13]=q[5:9]
   regs=warm(g) if False else regs
   rc,before,exp,c0=start(g,q)
   assert rc==1 and g.pend is not None and len(g.commands)==c0+1,(name,rc,g.pend,len(g.commands),c0) # started, not finished
   assert g.snapshot()==before,name # nothing drawn yet: the call really returned early
   assert g.seq[0x10]!=0xa0 and (g.excluded>0 or q[5]!=0),name # the sequencer lock and the cursor exclusion are kept for the completion
   assert g.hazards==[],name
   assert g.call('VX2D_access',(0,),strict=False)==1
   settled(g,exp,regs,name);n=acnt(g);assert n[0]==1 and n[1]==1 and n[2]==0,(name,n)
   cases.append(f'{bpp}bpp async {name}: the call returns with the command running (nothing drawn, lock and cursor exclusion kept), VX2D_access finishes it: pixels as the reference, registers restored, lock and exclusion released, no register written meanwhile')
  # every wait point finishes it
  g=GPU(bpp)
  for name,fn in (('VX2D_access 0',lambda:g.call('VX2D_access',(0,),strict=False)),('VX2D_control',lambda:g.call('VX2D_control',(0xFFFFFFFE,ARENA+0x3000),strict=False)),('VX2D_lost',lambda:g.call('VX2D_lost',strict=False)),
                  ('VX2D_busy',lambda:g.call('VX2D_busy',strict=False)),('VX2D_sync',lambda:g.call('VX2D_sync',strict=False)),
                  ('VX_flip_control',lambda:(g.u.mem_write(ARENA+0x3400,pack([16,0,0,0])),g.call('VX_flip_control',(ARENA+0x3400,ARENA+0x3500),strict=False))),
                  ('a synchronous submit',lambda:g.submit(g.request(1,flags=16,fg=7,dx=50,dy=30,w=5,h=5),strict=False)),('a GDI request',lambda:g.submit(g.request(1,fg=7,dx=50,dy=30,w=5,h=5),strict=False))):
   g=GPU(bpp);regs=warm(g);on(g);q=fill(g);rc,before,exp,c0=start(g,q);assert rc==1 and g.pend is not None,name
   if name in ('a synchronous submit','a GDI request'):
    q2=g.request(1,flags=0 if name=='a GDI request' else 16,fg=7,dx=50,dy=30,w=5,h=5);exp=g.reference(q2,exp)
   fn();g.async_busy=0
   if name in ('a synchronous submit','a GDI request'):g.call('VX2D_access',(0,),strict=False) # a synchronous submit leaves nothing running
   assert g.pend is None and g.hazards==[] and g.snapshot()==exp and g.excluded==0 and g.sqi==0x17 and g.seq[0x10]==0xa0,name
   cases.append(f'{bpp}bpp async: {name} finishes the running command first')
  # two in a row: the second submit finishes the first
  g=GPU(bpp);regs=warm(g);on(g);q1=fill(g);q2=cpy(g);g.async_busy=3;g.hazards=[];before=g.snapshot();e1=g.reference(q1,before);e2=g.reference(q2,e1)
  assert g.submit(q1,strict=False)==1 and g.submit(q2,strict=False)==1 and g.hazards==[];assert g.call('VX2D_access',(0,),strict=False)==1
  settled(g,e2,regs,'back to back');n=acnt(g);assert n[0]==2 and n[1]==2,n;cases.append(f'{bpp}bpp async: two commands back to back run in order, no register hazard, each completion counted')
  # when the gate or the switch is off the same request is synchronous
  for name,pre in (('the exclusive-mode gate is off',lambda g:(actl(g,0xFC000001),)),('ACCEL_ASYNC is off',lambda g:(actl(g,0xFC000000),actl(g,0xFD000001))),('both off',lambda g:())):
   g=GPU(bpp);regs=warm(g);pre(g);g.async_busy=0;q=fill(g);rc,before,exp,c0=start(g,q,busy=0);assert rc==1 and g.pend is None and g.snapshot()==exp and g.sqi==0x17 and g.seq[0x10]==0xa0 and g.excluded==0,name;assert acnt(g)[0]==0,name
   cases.append(f'{bpp}bpp async flag ignored (synchronous submit) when {name}')
  g=GPU(bpp);regs=warm(g);on(g);assert acnt(g)[3]==3;g.submit(g.request(1,fg=7,dx=50,dy=30,w=5,h=5),strict=False);assert acnt(g)[3]==1;cases.append(f'{bpp}bpp a GDI request ends the exclusive-mode gate')
  g=GPU(bpp);regs=warm(g);on(g);actl(g,0xFD000000);assert acnt(g)[3]==1;actl(g,0xFD000001);assert acnt(g)[3]==3;actl(g,0xFC000000);assert acnt(g)[3]==2;actl(g,0xFD000000);assert acnt(g)[3]==0;actl(g,0xFD000001);assert acnt(g)[3]==2;actl(g,0xFC000001);assert acnt(g)[3]==3;cases.append(f'{bpp}bpp the HAL gate codes set and clear the gate whether async is on or off (entering exclusive mode before the switch is on still arms it); the switch does not touch the gate')
  g=GPU(bpp);regs=warm(g);on(g);actl(g,0xFFFFFFFF);assert acnt(g)[3]==0;cases.append(f'{bpp}bpp the registry reset puts async back to the registry value (off) and drops the gate')
  # v10.424: armed OpenGL presents (0xFFFFFE21 arm, 0xFFFFFE20 disarm: a word, no wait; 0xFFFFFE22/23 the switch; 0xFFFFFE24 counters)
  def pst(g):return actl(g,0xFFFFFE24)
  def armed(g,async_on=1):
   actl(g,0xFC000001 if async_on else 0xFC000000);assert acnt(g)[3]&2==0;actl(g,0xFFFFFE21)
  g=GPU(bpp);regs=warm(g);armed(g);q=cpy(g);rc,before,exp,c0=start(g,q)
  assert rc==1 and g.pend is not None and g.snapshot()==before and g.hazards==[],'armed present not asynchronous'
  actl(g,0xFFFFFE20);assert g.pend is not None,'the disarm waited for the copy' # a word: the copy is still running
  assert g.call('VX2D_access',(0,),strict=False)==1;settled(g,exp,regs,'armed present');n=pst(g);assert n[0]==1 and n[1]==0 and n[2]==1,n
  cases.append(f'{bpp}bpp armed present outside an exclusive mode: the copy returns running (nothing drawn yet), the disarm does not wait for it, VX2D_access finishes it as the reference; counted once')
  for name,pre,mk in (('a fill (only copies are presents)',lambda g:armed(g),fill),('the present switch off',lambda g:(armed(g),actl(g,0xFFFFFE22)),cpy),
                      ('ACCEL_ASYNC off',lambda g:armed(g,0),cpy),('not armed',lambda g:actl(g,0xFC000001),cpy),
                      ('armed, then a GDI request',lambda g:(armed(g),g.submit(g.request(1,fg=7,dx=50,dy=30,w=5,h=5),strict=False)),cpy),
                      ('armed, then VX2D_sync',lambda g:(armed(g),g.call('VX2D_sync',strict=False)),cpy)):
   g=GPU(bpp);regs=warm(g);pre(g);g.async_busy=0;q=mk(g);rc,before,exp,c0=start(g,q,busy=0)
   assert rc==1 and g.pend is None and g.snapshot()==exp and g.excluded==0 and g.seq[0x10]==0xa0,name;assert pst(g)[0]==0,name
   cases.append(f'{bpp}bpp present request synchronous when {name}')
  g=GPU(bpp);regs=warm(g);armed(g);g.submit(g.request(1,fg=7,dx=50,dy=30,w=5,h=5),strict=False);assert pst(g)[1]==0;g=GPU(bpp);armed(g);actl(g,0xFFFFFE22);assert pst(g)[2]==0;actl(g,0xFFFFFFFF);n=pst(g);assert n[1]==0 and n[2]==1,n
  cases.append(f'{bpp}bpp a GDI request disarms a present; the registry reset disarms it and puts the present switch back on (the default)')
  # v10.425: while an asynchronous command runs, bit 15 of the display driver's wEnabled word is set (DIBENG paths call VX2D_complete)
  def ew(g):return struct.unpack('<H',bytes(g.u.mem_read(ARENA+0x802,2)))[0]
  for fin_name,fin in (('VX2D_complete',lambda g:g.call('VX2D_complete',strict=False)),('VX2D_access',lambda g:g.call('VX2D_access',(0,),strict=False))):
   g=GPU(bpp);regs=warm(g);g.u.mem_write(ARENA+0x802,struct.pack('<H',1));armed(g);q=cpy(g);rc,before,exp,c0=start(g,q)
   assert rc==1 and g.pend is not None and ew(g)==0x8001,(fin_name,hex(ew(g)))
   fin(g);settled(g,exp,regs,fin_name);assert ew(g)==1,(fin_name,hex(ew(g)))
   cases.append(f'{bpp}bpp a running asynchronous present sets bit 15 of the display driver\'s wEnabled (1 -> 8001); {fin_name} finishes the copy as the reference and clears it (8001 -> 1)')
  g=GPU(bpp);regs=warm(g);g.u.mem_write(ARENA+0x802,struct.pack('<H',0));armed(g);q=cpy(g);rc,before,exp,c0=start(g,q)
  assert rc==1 and g.pend is not None and ew(g)==0,hex(ew(g));g.call('VX2D_complete',strict=False);settled(g,exp,regs,'fenced');assert ew(g)==0
  cases.append(f'{bpp}bpp a fenced display driver (wEnabled 0) is not marked by a running present')
  g=GPU(bpp);regs=warm(g);assert g.call('VX2D_complete',strict=False) is not None and g.pend is None;cases.append(f'{bpp}bpp VX2D_complete with nothing running changes nothing')
  # lines and styled lines are never asynchronous
  g=GPU(bpp);regs=warm(g);on(g);ql=g.request(5,flags=16|128,dx=7,dy=6,sx=30,sy=20,w=23,h=1,fg=0x1234,rop=0xf0);n0=len(g.writes);assert g.submit(ql,strict=False)==0 and len(g.writes)==n0 and g.pend is None;cases.append(f'{bpp}bpp a line request carrying the async flag is rejected untouched')
  # a hang during the completion: reset, engine off, lock and exclusion released
  g=GPU(bpp);regs=warm(g);on(g);q=fill(g);rc,before,exp,c0=start(g,q);assert rc==1 and g.pend is not None
  g.busy=True;g.pend=None;g.call('VX2D_sync',strict=False)
  assert g.sqi==0x17 and g.seq[0x10]==0xa0 and g.excluded==0 and g.busy==False;st=acnt(g);assert st[2]==0;cases.append(f'{bpp}bpp async: an engine that hangs before the completion is reset, and the lock and cursor exclusion are still released')


 # v10.99/v10.100: the DIB conversion fast path (StretchDIBits and DibToDevice sources into the 16bpp or 32bpp screen). VX2D_stretch converts and replicates on the CPU; the reference below is the measured DIBENG behaviour
 # (VX2SDIB, VX2SDI2, VX2DTD): source pixel floor(i/k) on both axes; 16bpp screen: 24/32 bit truncated to 565, a 555 source stretched takes the green low bit from the 5-bit value (g5<<1|g5&1) and 0 at 1:1;
 # 32bpp screen: a 5-bit value v becomes (v<<3)|7 when odd, a 6-bit one (v<<2)|3 when odd, 24-bit unchanged; the source rectangle is counted from the bottom row whatever the DIB's direction.
 import random as _r
 rnd=_r.Random(99)
 def sdib_ctl(g,v):
  g.call('VX2D_control',(v,ARENA+0x3000),strict=False);return [g.word(ARENA+0x3000+4*i) for i in range(4)]
 def sdib_cnt(g):return sdib_ctl(g,0xFFFFFE13)
 def sdib_src(g,bits,W,H,seed):
  bt=bits//8;stride=((W*bt+3)//4)*4;rr=_r.Random(seed);data=bytearray(rr.getrandbits(8) for _ in range(stride*H))
  g.u.mem_write(SRC+0x100,bytes(data));return stride,bytes(data)
 def sdib_req(g,bits,W,H,stride,topdown,dx,dy,kx,ky,sx,sy,sw,sh,clip,**kw):
  q=[1136,0x10000,0,g.bpp, 0,g.pitch,64,48, dx&0xFFFFFFFF,dy&0xFFFFFFFF,kx,ky, sx,sy,sw,sh, clip[0],clip[1],clip[2],clip[3], SRC+0x100,stride,bits,H,int(topdown),stride*H]
  pal=kw.pop('pal',None)
  if pal is None:pal=[_r.Random(bits*7+dx).getrandbits(24) for _ in range(256)] if bits==8 else [0]*256
  q+=[kw.pop('pal_n',256 if bits==8 else 0)]+list(pal)+[kw.pop('src_width',0)]
  names={'cb':0,'abi':1,'flags':2,'bpp':3,'src_lin':20,'src_stride':21,'src_bits':22,'src_height':23,'src_bytes':25,'pal_n':26,'src_width':283}
  for n,v in kw.items():q[names[n]]=v
  return q
 s32=lambda v:v-(1<<32) if v>>31 else v
 def pyclass(q):
  dbpp,flags,kx,ky,bits=q[3],q[2],q[10],q[11],q[22];mn=min(kx,ky);one=kx==1 and ky==1;iso=mn>=2 or one
  if dbpp==16:
   if bits==8:return 0
   if bits==16:return 0 if flags&2 else (64 if one else 1 if mn>=2 else 16)
   if bits==24:return 32 if one else 2 if mn>=2 else 8
   return 32 if one else 4 if mn>=2 else 8
  if bits==8:return 0 if flags else (2097152 if one else 524288 if mn>=2 else 1048576)
  if bits==16:return ((16384 if one else 512 if mn>=2 else 1024) if flags&2 else (8192 if one else 128 if mn>=2 else 256))
  if bits==24:return 32768 if one else 2048 if mn>=2 else 4096
  return 0
 def sdib_range(q):
  dx,dy=s32(q[8]),s32(q[9]);kx,ky,sx,sy=q[10:14];l,t,r,b=q[16:20];H,td,stride,bytes_=q[23],q[24],q[21],q[22]//8
  c0=(l-dx)//kx;c1=(r-1-dx)//kx;tlo=sy+(t-dy)//ky;thi=sy+(b-1-dy)//ky
  mlo,mhi=(tlo,thi) if td else (H-1-thi,H-1-tlo)
  return q[20]+mlo*stride+(sx+c0)*bytes_,q[20]+mhi*stride+(sx+c1+1)*bytes_
 def sdib_ref(g,before,q,data):
  b=bytearray(before);flags,dbpp=q[2],q[3];dpx=dbpp//8;dx,dy=s32(q[8]),s32(q[9]);kx,ky,sx,sy,sw,sh,l,t,r,bt=q[10:20];stride,bits,H,td=q[21],q[22],q[23],q[24];bytes_=bits//8;one=kx==1 and ky==1
  e5=lambda v:(v<<3)|(7 if v&1 else 0);e6=lambda v:(v<<2)|(3 if v&1 else 0)
  for y in range(t,bt):
   for x in range(l,r):
    c=(x-dx)//kx;ty=sy+(y-dy)//ky;row=ty if td else H-1-ty;o=row*stride+(sx+c)*bytes_
    if bits==8:
     pq=q[27+data[o]];out=((pq>>19&31)<<11|(pq>>10&63)<<5|(pq>>3&31)) if dbpp==16 else pq&0xFFFFFF
    elif bits==16:
     v=data[o]|data[o+1]<<8
     if flags&2:R,G,B=v>>11,(v>>5)&63,v&31;out=(e5(R)<<16)|(e6(G)<<8)|e5(B)
     else:
      R,G,B=(v>>10)&31,(v>>5)&31,v&31
      out=(R<<11)|(((G<<1)|(0 if (flags&1 or one) else G&1))<<5)|B if dbpp==16 else (e5(R)<<16)|(e5(G)<<8)|e5(B)
    else:
     B,G,R=data[o],data[o+1],data[o+2]
     out=(R>>3)<<11|(G>>2)<<5|(B>>3) if dbpp==16 else (R<<16)|(G<<8)|B
    at=(y*g.pitch)+x*dpx;b[at:at+dpx]=out.to_bytes(dpx,'little')
  return bytes(b)
 reads=[]
 def sdib_do(g,q):
  g.u.mem_write(ARENA+0x1800,pack(q));g.prefill();before=g.snapshot();n=len(g.commands);g.pages=[];reads.clear()
  h=g.u.hook_add(UC_HOOK_MEM_READ,lambda u,t,a,z,v,_:reads.append((a,z)),None,SRC,SRC+0x1FFFFF)
  try:rc=g.call('VX2D_stretch',(ARENA+0x1800,),strict=False)
  finally:g.u.hook_del(h)
  assert len(g.commands)==n and g.excluded==0 and g.critical==0,'stretch never starts the engine';return rc,before
 def sdib_pass(g,q,data,name):
  rc,before=sdib_do(g,q);assert rc==1,(name,rc)
  assert g.snapshot()==sdib_ref(g,before,q,data),name
  lo,hi=sdib_range(q);assert [p[0] for p in g.pages]==['lock','unlock'] and g.pages[0][1:]==g.pages[1][1:],(name,g.pages)
  assert g.pages[0][1]==lo>>12 and g.pages[0][2]==(((lo&0xFFF)+(hi-lo)+0xFFF)>>12),(name,g.pages,hex(lo),hex(hi))
  assert reads and min(a for a,z in reads)>=lo and max(a+z for a,z in reads)<=hi,(name,hex(min(a for a,z in reads)),hex(max(a+z for a,z in reads)),hex(lo),hex(hi)) # nothing read outside the locked range
 def sdib_skip(g,q,name):
  rc,before=sdib_do(g,q);assert rc==0,(name,rc);assert g.snapshot()==before and not reads,name
 def vis(g,dx,dy,w,h,rnd_clip=True):
  l,t,r,b=max(dx,0),max(dy,0),min(dx+w,64),min(dy+h,48)
  if r<=l or b<=t:return None
  if rnd_clip and rnd.random()<0.5:
   l2=rnd.randint(l,r-1);t2=rnd.randint(t,b-1);return (l2,t2,rnd.randint(l2+1,r),rnd.randint(t2+1,b))
  return (l,t,r,b)
 k0=0
 for dbpp in (16,32):
  g=GPU(dbpp)
  for bits,fl in ((16,0),(16,2),(24,0),(32,0),(8,0)):
   for kx,ky in ((1,1),(2,2),(3,3),(4,4),(2,3),(3,2),(1,2),(2,1),(1,4),(4,1),(1,3)):
    done=0
    for td in (False,True):
     for rep in range(2):
      W,H=rnd.randint(2,10),rnd.randint(2,8);sx=rnd.randint(0,W-1);sy=rnd.randint(0,H-1);sw=rnd.randint(1,W-sx);sh=rnd.randint(1,H-sy)
      while sw*kx>60 or sh*ky>44:sw=max(1,sw-1);sh=max(1,sh-1)
      stride,data=sdib_src(g,bits,W,H,k0);k0+=1
      dx=rnd.randint(-(sw*kx-1),63) if rep else rnd.randint(0,63-sw*kx);dy=rnd.randint(-(sh*ky-1),47) if rep else rnd.randint(0,47-sh*ky)
      v=vis(g,dx,dy,sw*kx,sh*ky)
      if v is None:continue
      q=sdib_req(g,bits,W,H,stride,td,dx,dy,kx,ky,sx,sy,sw,sh,v,flags=fl);sdib_ctl(g,0xFBFFFFFF)
      name=f'{dbpp}bpp {bits}-bit flags {fl} {kx}x{ky} td={td} origin {dx},{dy} visible {v}'
      if pyclass(q):sdib_pass(g,q,data,name)
      else:sdib_skip(g,q,name)
      done+=1
    assert done
   cases.append(f'{dbpp}bpp screen, {bits}-bit {"565" if fl else "555/RGB"} source, factors 1:1 and 1..4 on each axis, bottom-up and top-down, negative and positive origins, clipped: pixels equal the DIBENG reference (or declined and untouched when the class is not accelerated, e.g. 32-bit into 32bpp, 565 into 16bpp), the locked range is exactly the bytes read, engine untouched')
 # a 565 source with a 24-bit-only flag etc. is invalid
 # the class mask: each of the 19 bits allows exactly its class
 ex=[(16,16,0,2,2),(16,24,0,2,2),(16,32,0,2,2),(16,24,0,1,3),(16,16,0,1,3),(16,24,0,1,1),(16,16,0,1,1),(32,16,0,2,2),(32,16,0,1,2),(32,16,2,3,3),(32,16,2,3,1),(32,24,0,2,2),(32,24,0,4,1),(32,16,0,1,1),(32,16,2,1,1),(32,24,0,1,1),(32,8,0,2,2),(32,8,0,4,1),(32,8,0,1,1)]
 expect=[1,2,4,8,16,32,64,128,256,512,1024,2048,4096,8192,16384,32768,524288,1048576,2097152]
 for idx,(dbpp,bits,fl,kx,ky) in enumerate(ex):
  g=GPU(dbpp)
  for bit in expect:
   W,H=5,4;stride,data=sdib_src(g,bits,W,H,bit+idx);q=sdib_req(g,bits,W,H,stride,False,2,2,kx,ky,0,0,W,H,(2,2,2+W*kx,2+H*ky),flags=fl);assert pyclass(q)==expect[idx]
   sdib_ctl(g,0xFB000000|bit);c0=sdib_cnt(g)
   if bit==expect[idx]:sdib_pass(g,q,data,f'mask {bit} class {bit}')
   else:sdib_skip(g,q,f'mask {bit} must not take class {expect[idx]}')
   c1=sdib_cnt(g);assert c1[3]==bit and c1[0]-c0[0]==(bit==expect[idx]) and c1[1]-c0[1]==(bit!=expect[idx]),(bit,idx,c0,c1)
 cases.append('stretch class mask: bits 0..4 the 16bpp enlargements (555 both >= 2, 24-bit, 32-bit, 24/32 with one factor 1, 555 with one factor 1), bit 5 24/32-bit 1:1, bit 6 555 1:1, bits 7..12 the 32bpp screen enlargements (555, 565, 24-bit: both factors >= 2 / one factor 1), bits 13..15 the 32bpp 1:1 classes (555, 565, 24-bit): each bit allows exactly its class and nothing else; other classes counted as declined')
 # v10.507/v10.508: a 1:1 row onto the 32bpp screen is converted straight into the screen (8/16(555, 565)/24-bit, every width and alignment): the DIBENG reference pixels,
 # the direct-row counter moves; 32-bit 1:1 onto 32bpp stays with DIBENG
 def gdi(g,v):return sdib_ctl(g,v)
 g=GPU(32);sdib_ctl(g,0xFBFFFFFF);c0=gdi(g,0xFFFFFE26)
 for bits,fl in ((24,0),(16,0),(16,2),(8,0)):
  for rep_ in range(6):
   W,H=rnd.randint(1,40),rnd.randint(1,6);stride,data=sdib_src(g,bits,W,H,k0);k0+=1;dx=rnd.randint(0,63-W);dy=rnd.randint(0,47-H)
   q=sdib_req(g,bits,W,H,stride,bool(rep_&1),dx,dy,1,1,0,0,W,H,(dx,dy,dx+W,dy+H),flags=fl);sdib_pass(g,q,data,f'direct rows {bits}-bit fl {fl} W {W} at {dx}')
 c1=gdi(g,0xFFFFFE26);assert c1[2]>c0[2],(c0,c1)
 stride,data=sdib_src(g,32,5,4,k0);sdib_skip(g,sdib_req(g,32,5,4,stride,False,2,2,1,1,0,0,5,4,(2,2,7,6)),'32-bit 1:1 onto 32bpp: DIBENG')
 cases.append('1:1 DIB rows onto the 32bpp screen converted straight into the screen: the DIBENG pixels for 8/16(555, 565)/24-bit sources at every width and alignment; 32-bit 1:1 left to DIBENG')
 c=gdi(g,0xE9000000);assert c[0]==0;c=gdi(g,0xE8000000|300|(40<<12));assert c[1]==300|(40<<16),c
 hw=g.word(g.hda+256)>>16;assert hw==0x8000|((300>>4)<<2)|((40>>4)<<8),hex(hw)
 c=gdi(g,0xE8000000|4095|(2000<<12));hw=g.word(g.hda+256)>>16;assert hw==0x8000|(63<<2)|(63<<8),hex(hw)
 gdi(g,0xFFFFFFFF);c=gdi(g,0xFFFFFE26);assert c[0]==3 and c[1]==368|(112<<16),c;hw=g.word(g.hda+256)>>16;assert hw==0x8000|3|(23<<2)|(7<<8),hex(hw)
 cases.append('the GDI options: live 0xE9000000 | bits and 0xE8000000 | x | y << 12, published to the display driver (FBHDA rem3 bits 31:16 = 0x8000 | bits | x/16 << 2 | y/16 << 8, 63 = no limit); the registry reset puts back 3, 368 and 112')
 g=GPU(16);sdib_ctl(g,0xFB000000);stride,data=sdib_src(g,24,5,4,5);q0=sdib_req(g,24,5,4,stride,False,2,2,2,2,0,0,5,4,(2,2,12,10));sdib_skip(g,q0,'mask 0');assert sdib_cnt(g)[3]==0;cases.append('stretch: mask 0 (the shipped default) declines everything and draws nothing')
 # declines on malformed requests: nothing drawn, no engine access
 sdib_ctl(g,0xFBFFFFFF);W,H=5,4;stride,data=sdib_src(g,24,W,H,6);ok=lambda **kw:sdib_req(g,24,W,H,stride,False,2,2,2,2,0,0,W,H,(2,2,12,10),**kw)
 bad=[('wrong size',ok(cb=105)),('wrong ABI',ok(abi=0x20000)),('bpp 8 destination',ok(bpp=8)),('bpp 32 request on a 16bpp screen',ok(bpp=32)),('flags unknown',ok(flags=8)),('565 flag on a 24-bit source',ok(flags=2)),
  ('source bits 8',ok(src_bits=8)),('stride not multiple of 4',ok(src_stride=stride+1)),('stride too small',ok(src_stride=12)),('src_bytes mismatch',ok(src_bytes=stride*H-4)),('source below 4 MB',ok(src_lin=0x1000)),
  ('source row range beyond the bitmap',sdib_req(g,24,W,H,stride,False,2,2,2,2,0,2,W,H,(2,2,12,10))),('source column range beyond the row',sdib_req(g,24,W,H,stride,False,2,2,2,2,1,0,W,H,(2,2,12,10))),
  ('clip empty',sdib_req(g,24,W,H,stride,False,2,2,2,2,0,0,W,H,(5,5,5,9))),('clip outside the destination rectangle',sdib_req(g,24,W,H,stride,False,2,2,2,2,0,0,W,H,(1,2,12,10))),
  ('clip beyond the screen',sdib_req(g,24,W,H,stride,False,60,40,2,2,0,0,W,H,(60,40,70,48))),('clip with a negative left',sdib_req(g,24,W,H,stride,False,-2,2,2,2,0,0,W,H,(0xFFFFFFFE,2,6,10))),('destination over 4096',sdib_req(g,24,W,H,stride,False,0,0,1000,2,0,0,W,H,(0,0,10,8))),('zero factor',sdib_req(g,24,W,H,stride,False,2,2,0,2,0,0,W,H,(2,2,12,10)))]
 for name,q in bad:sdib_skip(g,q,name)
 st=sdib_cnt(g);assert st[1]>=len(bad),st
 q=ok();g.pages=[];g.page_ok=False;sdib_skip(g,q,'page lock fails');g.page_ok=True;assert [p[0] for p in g.pages]==['lock'],g.pages
 g.lab=True;sdib_skip(g,q,'3D busy');g.lab=False;sdib_pass(g,q,data,'recovers after the skips')
 g8=GPU(16);sdib_ctl(g8,0xFBFFFFFF);s8,d8=sdib_src(g8,8,W,H,9)
 for nm,kw in (('pal_n 0 on an 8-bit source',dict(pal_n=0)),('pal_n 257',dict(pal_n=257))):sdib_skip(g8,sdib_req(g8,8,W,H,s8,False,2,2,2,2,0,0,W,H,(2,2,12,10),**kw),nm)
 sdib_skip(g,sdib_req(g,24,W,H,stride,False,2,2,2,2,0,0,W,H,(2,2,12,10),pal_n=5),'palette entries on a 24-bit source')
 sdib_skip(g8,sdib_req(g8,8,W,H,s8,False,2,2,2,2,0,0,W,H,(2,2,12,10),flags=2),'565 flag on an 8-bit source')
 cases.append('stretch declines (nothing drawn, no engine access): bad size/ABI/depth/flags, unsupported source depth, bad stride/length/address, source or clip range errors, over 4096, zero factor, failed page lock (no unlock), 3D busy; then a good request still works')
 # a partial bitmap: the request claims the whole height but only the rows the clip shows exist (DibToDevice with StartScan/cLines); nothing outside the locked rows is read
 g=GPU(16);sdib_ctl(g,0xFBFFFFFF);W,H=6,9;stride,data=sdib_src(g,24,W,H,77)
 q=sdib_req(g,24,W,H,stride,False,3,10,1,1,0,0,W,H,(3,12,3+W,15));sdib_pass(g,q,data,'partial bitmap rows')
 lo,hi=sdib_range(q);assert hi-lo<stride*4,(lo,hi);cases.append('stretch of a partial bitmap (DibToDevice with a clip showing 3 of 9 rows): only those rows are locked and read')
 # the registry reset returns the mask to the configured default; a GDI stretch ends the DirectDraw gate and completes a running async command first
 sdib_ctl(g,0xFBFFFFFF);sdib_ctl(g,0xFFFFFFFF);assert sdib_cnt(g)[3]==0;cases.append('stretch: the registry reset puts the class mask back to ACCEL_STRETCH (default 0)')
 g=GPU(16)
 g.submit(g.request(1,flags=16,fg=0x1111,dx=1,dy=1,w=3,h=3));sdib_ctl(g,0xFB00001F);sdib_ctl(g,0xFC000001);sdib_ctl(g,0xFD000001)
 g.prefill();g.async_busy=3;fq=g.request(1,flags=16|128,fg=0x5A3C,dx=5,dy=4,w=40,h=30);b0=g.snapshot();e1=g.reference(fq,b0);assert g.submit(fq,strict=False)==1 and g.pend is not None
 stride,data=sdib_src(g,32,4,4,3);q=sdib_req(g,32,4,4,stride,False,10,8,2,2,0,0,4,4,(10,8,18,16));g.u.mem_write(ARENA+0x1800,pack(q));g.async_busy=0
 assert g.call('VX2D_stretch',(ARENA+0x1800,),strict=False)==1 and g.pend is None and g.hazards==[] and g.seq[0x10]==0xa0 and g.excluded==0
 assert g.snapshot()==sdib_ref(g,e1,q,data) and sdib_cnt(g)[0]==1;assert sdib_ctl(g,0xFFFFFE12)[3]&2==0 and sdib_ctl(g,0xFFFFFE12)[3]&1==1
 cases.append('stretch while a DirectDraw async blit is running: the blit completes first (registers restored, lock released), then the stretch draws over it; the exclusive-mode gate is dropped')

 # ---- v10.104: 1-bit, 4-bit and RLE sources into the 32bpp screen (class mask 2, ACCEL_STRETCH2) ----
 # reference decoder of the Windows RLE formats (written from the format description, independent of the VxD's decoder): returns the index bitmap, bottom row first, or None when pixels are left undefined
 def rle_ref(data,W,H,bits):
  bm=[[None]*W for _ in range(H)];x=y=0;i=0
  while i+1<len(data):
   a,b=data[i],data[i+1];i+=2
   if a:
    for k in range(a):
     if x<W and y<H:bm[y][x]=b if bits==8 else ((b>>4) if k%2==0 else b&15)
     x+=1
   elif b==0:x=0;y+=1
   elif b==1:break
   elif b==2:x+=data[i];y+=data[i+1];i+=2
   else:
    nb=b if bits==8 else (b+1)//2
    for k in range(b):
     v=data[i+k] if bits==8 else ((data[i+k//2]>>4) if k%2==0 else data[i+k//2]&15)
     if x<W and y<H:bm[y][x]=v
     x+=1
    i+=(nb+1)&~1
  return bm if all(v is not None for row in bm for v in row) else None
 # a random stream that covers every pixel once: encoded runs and absolute runs, an end of line after each row, an end of bitmap at the end
 def rle_gen(rr,W,H,bits,npal):
  out=bytearray()
  for y in range(H):
   x=0
   while x<W:
    left=W-x
    if left>=3 and rr.random()<0.4:
     n=rr.randint(3,left);vals=[rr.randrange(npal) for _ in range(n)]
     if bits==8:out+=bytes([0,n])+bytes(vals)+(b'\0' if n&1 else b'')
     else:
      nb=bytearray()
      for k in range(0,n,2):nb.append((vals[k]<<4)|(vals[k+1] if k+1<n else 0))
      out+=bytes([0,n])+nb+(b'\0' if len(nb)&1 else b'')
    else:
     n=rr.randint(1,min(left,255));v=rr.randrange(npal) if bits==8 else (rr.randrange(npal)<<4)|rr.randrange(npal)
     out+=bytes([n,v])
    x+=n
   out+=b'\0\0' if y<H-1 or rr.random()<0.5 else b''
  return bytes(out)+b'\0\1'
 def pk_idx(bits,data,stride):
  if bits==4:return lambda row,col:(data[row*stride+col//2]>>4) if col%2==0 else data[row*stride+col//2]&15
  return lambda row,col:(data[row*stride+col//8]>>(7-col%8))&1
 def sref2(q,before,pixel):
  b=bytearray(before);dx,dy=s32(q[8]),s32(q[9]);kx,ky,sx,sy,sw,sh,l,t,r,bt=q[10:20];H,td=q[23],q[24];pal=q[27:283]
  for y in range(t,bt):
   for x in range(l,r):
    c=(x-dx)//kx;ty=sy+(y-dy)//ky;row=ty if td else H-1-ty;out=pal[pixel(row,sx+c)]&0xFFFFFF
    at=y*256+x*4;b[at:at+4]=out.to_bytes(4,"little")
  return bytes(b)
 def rng2(q,rle):
  if rle:return q[20],q[20]+q[25]
  dx,dy=s32(q[8]),s32(q[9]);kx,ky,sx,sy=q[10:14];l,t,r,b=q[16:20];H,td,stride,bits=q[23],q[24],q[21],q[22]
  c0=(l-dx)//kx;c1=(r-1-dx)//kx;tlo=sy+(t-dy)//ky;thi=sy+(b-1-dy)//ky
  mlo,mhi=(tlo,thi) if td else (H-1-thi,H-1-tlo)
  return q[20]+mlo*stride+((sx+c0)*bits>>3),q[20]+mhi*stride+(((sx+c1+1)*bits+7)>>3)
 def pass2(g,q,pixel,rle,name):
  rc,before=sdib_do(g,q);assert rc==1,(name,rc)
  assert g.snapshot()==sref2(q,before,pixel),name
  lo,hi=rng2(q,rle);assert [p[0] for p in g.pages]==['lock','unlock'] and g.pages[0][1:]==g.pages[1][1:],(name,g.pages)
  assert g.pages[0][1]==lo>>12 and g.pages[0][2]==(((lo&0xFFF)+(hi-lo)+0xFFF)>>12),(name,g.pages,hex(lo),hex(hi))
  assert reads and min(a for a,z in reads)>=lo and max(a+z for a,z in reads)<=hi,(name,hex(lo),hex(hi))
 def cls2(kind,kx,ky):
  one=kx==1 and ky==1;mn=min(kx,ky);return 1<<(kind*3+(2 if one else 0 if mn>=2 else 1))
 done2=0;fused2=[0]
 g32=GPU(32);g16=GPU(16)
 for kind,(bits,rle) in enumerate(((1,0),(4,0),(8,1),(4,1))):
  for kx,ky in ((1,1),(2,2),(3,3),(4,4),(2,3),(3,2),(1,2),(2,1),(1,4),(4,1),(1,3)):
   for rep in range(4):
    W,H=rnd.randint(2,30),rnd.randint(2,8);sx=rnd.randint(0,W-1);sy=rnd.randint(0,H-1);sw=rnd.randint(1,W-sx);sh=rnd.randint(1,H-sy)
    while sw*kx>60 or sh*ky>44:sw=max(1,sw-1);sh=max(1,sh-1)
    npal=2 if bits==1 else 16 if bits==4 else 256;pal=[_r.Random(kind*977+rep*31+kx).getrandbits(32) for _ in range(256)]
    if rle:
     stream=rle_gen(rnd,W,H,bits,16 if bits==4 else 200);bm=rle_ref(stream,W,H,bits);assert bm is not None
     data=stream;pixel=lambda row,col,bm=bm:bm[row][col];stride=W
    else:
     stride=((W*bits+31)//32)*4;data=bytes(rnd.getrandbits(8) for _ in range(stride*H));pixel=pk_idx(bits,data,stride)
    g32.u.mem_write(SRC+0x100,data)
    td=(not rle) and rnd.random()<0.5
    dx=rnd.randint(-(sw*kx-1),63) if rep&1 else rnd.randint(0,63-sw*kx);dy=rnd.randint(-(sh*ky-1),47) if rep&1 else rnd.randint(0,47-sh*ky)
    v=vis(g32,dx,dy,sw*kx,sh*ky)
    if v is None:continue
    if td and not(sy==0 and sh==H):sy=0;sh=H  # a whole top-down bitmap
    kw=dict(flags=4 if rle else 0,src_bits=bits,pal=pal,pal_n=npal)
    if rle:kw.update(src_stride=W,src_width=W,src_bytes=len(data))
    else:kw.update(src_stride=stride,src_bytes=stride*H)
    q=sdib_req(g32,bits,W,H,stride,td,dx,dy,kx,ky,sx,sy,sw,sh,v,**kw);q[22]=bits;q[20]=SRC+0x100
    want=cls2(kind,kx,ky);name=f'{"RLE" if rle else "raw"}{bits} {W}x{H} {kx}x{ky} td={td} src {sx},{sy} {sw}x{sh} origin {dx},{dy} vis {v}'
    sdib_ctl(g32,0xFB000000);sdib_ctl(g32,0xFA000000|want);pass2(g32,q,pixel,rle,name);done2+=1
    if bits==1 and not rle:
     f0=sdib_ctl(g32,0xF3000001)[1];pass2(g32,q,pixel,rle,name+' fused');assert sdib_ctl(g32,0xF3000001)[1]>f0,name
     sdib_ctl(g32,0xF3000000);pass2(g32,q,pixel,rle,name+' unfused');f1=sdib_ctl(g32,0xF3000000)[1];sdib_ctl(g32,0xF3000001);fused2[0]+=1
    sdib_ctl(g32,0xFA000000|(0xFFFFFF&~want));sdib_skip(g32,q,name+' (other classes on)')
    sdib_ctl(g32,0xFA000000);sdib_skip(g32,q,name+' (mask 2 = 0)')
    sdib_ctl(g32,0xFB000000|0xFFFFFF);sdib_skip(g32,q,name+' (mask 1 alone does not take it)')
    q16=list(q);q16[3]=16;g16.u.mem_write(SRC+0x100,data);sdib_ctl(g16,0xFB000000|0xFFFFFF);sdib_ctl(g16,0xFA000000|0xFFFFFF);sdib_skip(g16,q16,name+' into the 16bpp screen')
 assert done2>=120,done2
 assert fused2[0]>=20,fused2;cases.append(f'v10.132: 1-bit sources into 32bpp through the fused nibble-table expansion and through the old index-byte line give the same pixels ({fused2[0]} cases, odd widths, every start bit, stretched and 1:1)')
 cases.append(f'{done2} stretches of 1-bit, 4-bit, RLE8 and RLE4 sources into the 32bpp screen (factors 1:1 and 1..4, bottom-up and top-down raw bitmaps, negative origins, clips, source columns that start inside a byte): pixels equal the palette reference, the locked range is exactly the bytes read (the whole stream for RLE), engine untouched; each class bit takes only its class, mask 1 and mask 0 take nothing, the 16bpp screen declines them all')
 # RLE streams that must stay with DIBENG: every one is a valid file for a Windows decoder, but leaves pixels undefined or breaks the format
 sdib_ctl(g32,0xFB000000);sdib_ctl(g32,0xFAFFFFFF);W,H=6,3
 base=rle_gen(random.Random(5),W,H,8,200)
 def rle_req(stream,W=W,H=H,bits=8,**kw):
  g32.u.mem_write(SRC+0x100,stream);pal=[0x010203*k for k in range(256)]
  return sdib_req(g32,bits,W,H,W,False,2,2,1,1,0,0,W,H,(2,2,2+W,2+H),flags=4,src_bits=bits,pal=pal,pal_n=256 if bits==8 else 16,src_width=W,src_stride=W,src_bytes=len(stream),**kw)
 c0=sdib_ctl(g32,0xFFFFFE14)
 rows=lambda *r:b''.join(bytes(x) for x in r)
 good=bytes([6,1,0,0,6,2,0,0,6,3,0,1])
 assert rle_ref(good,6,3,8) is not None
 pass2(g32,rle_req(good),(lambda bm:(lambda row,col:bm[row][col]))(rle_ref(good,6,3,8)),True,'hand-made RLE8')
 bad_rle=[('delta',bytes([6,1,0,0,3,2,0,0,2,0,1,0,1])),('delta skipping pixels',bytes([3,1,0,2,3,0,3,1,0,0,6,2,0,0,6,3,0,1])),('row ends short',bytes([5,1,0,0,6,2,0,0,6,3,0,1])),('row runs over',bytes([7,1,0,0,6,2,0,0,6,3,0,1])),
  ('a row missing',bytes([6,1,0,0,6,2,0,1])),('an extra row',bytes([6,1,0,0,6,2,0,0,6,3,0,0,6,4,0,1])),('no end of bitmap, one row short',bytes([6,1,0,0,6,2,0,0])),('end of line too early',bytes([6,1,0,0,0,0,6,2,0,0,6,3,0,1])),
  ('absolute run cut off by the end of the stream',bytes([6,1,0,0,6,2,0,0,0,6,3,3])),('absolute run over the row',bytes([6,1,0,0,6,2,0,0,0,7,1,2,3,4,5,6,7,0,0,1])),('one byte',b'\x05')]
 for name,st in bad_rle:
  rc,before=sdib_do(g32,rle_req(st));assert rc==0 and g32.snapshot()==before,('RLE8: '+name,rc);assert [p[0] for p in g32.pages]==['lock','unlock'],g32.pages # the stream was read and the pages released, nothing drawn
 c1=sdib_ctl(g32,0xFFFFFE14);assert c1[1]-c0[1]==len(bad_rle),(c0,c1)  # each was refused by the decoder, not by an earlier check
 q=rle_req(good);q[24]=1;sdib_skip(g32,q,'RLE bitmap marked top-down')
 q=rle_req(good);q[21]=W+1;sdib_skip(g32,q,'RLE stride differs from the width')
 q=rle_req(good);q[283]=0;sdib_skip(g32,q,'RLE without a width')
 q=rle_req(good,W=4096,H=65);sdib_skip(g32,q,'RLE bitmap larger than the decode buffer')
 q=rle_req(good);q[25]=0;sdib_skip(g32,q,'RLE with an empty stream')
 sdib_skip(g32,rle_req(b''),'RLE with an empty stream (src_bytes 0)')
 q=rle_req(good);q[22]=16;sdib_skip(g32,q,'RLE flag on a 16-bit source')
 q=rle_req(good);q[26]=0;sdib_skip(g32,q,'RLE with no palette')
 for bits,pn in ((1,0),(1,3),(4,0),(4,17)):
  stride=((W*bits+31)//32)*4;g32.u.mem_write(SRC+0x100,bytes(stride*H))
  sdib_skip(g32,sdib_req(g32,bits,W,H,stride,False,2,2,1,1,0,0,W,H,(2,2,2+W,2+H),src_bits=bits,pal_n=pn,src_stride=stride,src_bytes=stride*H),f'{bits}-bit with {pn} palette entries')
 for bits in (2,3):sdib_skip(g32,sdib_req(g32,bits,W,H,4,False,2,2,1,1,0,0,W,H,(2,2,2+W,2+H),src_bits=bits,pal_n=4,src_stride=4,src_bytes=4*H),f'{bits}-bit source')
 sdib_skip(g32,sdib_req(g32,1,W,H,4,False,2,2,1,1,0,0,W,H,(2,2,2+W,2+H),src_bits=1,pal_n=2,src_stride=4,src_bytes=4*H,flags=2),'565 flag on a 1-bit source')
 cases.append('RLE streams the VxD must leave to DIBENG, each refused by the decoder (counted): delta codes, rows that end short or run over, missing or extra rows, a missing end of bitmap, an early end of line, absolute runs cut off or over the row, empty and one-byte streams; also top-down RLE, a stride that is not the width, no width, a bitmap above the 262144-pixel decode buffer, an empty stream, the RLE flag on a 16-bit source, no palette, 1-bit and 4-bit with a palette of the wrong size, 2-bit and 3-bit sources, the 565 flag on a 1-bit source')
 g=GPU(32);sdib_ctl(g,0xFAFFFFFF);sdib_ctl(g,0xFFFFFFFF);assert sdib_ctl(g,0xFFFFFE14)[3]==0;cases.append('the registry reset puts class mask 2 back to ACCEL_STRETCH2 (default 0)')
 # .592 (VX800 board): with XP's ring paused the status shows only the command regulator's bit while the regulator still holds a 2D
 # command, and that reads as idle: a command must be waited for from its start
 for late in (1,20,200):
  for bpp in (16,32):
   g=GPU(bpp);assert g.submit(g.request(1,flags=16,fg=0x1111&((1<<bpp)-1),dx=1,dy=1,w=3,h=3))==1
   g.u.mem_write(g.addr('vx3dn_xr_cr_pause'),pack([1]));g.u.mem_write(MM+0x444,pack([0x80000000]))
   for flags in (0,16):
    q=g.request(1,flags=flags,fg=0x2222&((1<<bpp)-1),dx=5,dy=5,w=40,h=30);before=g.snapshot();exp=g.reference(q,before)
    g.late=late;rc=g.submit(q,strict=False);g.late=0
    assert rc==1 and g.pend is None and g.snapshot()==exp,(late,bpp,flags,rc,g.pend)
   r=[0]*4;g.call('VX2D_control',(0xFFFFFE46,ARENA+0x3000),strict=False);r=[g.word(ARENA+0x3000+4*i) for i in range(4)];assert r[0]>=2 and r[1]==0 and r[2]==1,r
 cases.append('.592: with the ring-paused idle rule in use, a fill the command regulator holds for 1, 20 or 200 status reads (status 0x10, 444 bit 31) is finished when the request returns, GDI and DirectDraw, 16 and 32 bpp; the waits are counted and the engine was seen busy in each')
 # .593 (VX800 board): the first status read after a command can be 00000000 with the engine not started (busy from the second read)
 for late in (1,2,3):
  for bpp in (16,32):
   g=GPU(bpp);assert g.submit(g.request(1,flags=16,fg=0x1111&((1<<bpp)-1),dx=1,dy=1,w=3,h=3))==1
   for flags in (0,16):
    q=g.request(1,flags=flags,fg=0x3333&((1<<bpp)-1),dx=5,dy=5,w=40,h=30);before=g.snapshot();exp=g.reference(q,before)
    g.late=late;g.late_status=0;rc=g.submit(q,strict=False);g.late=0
    assert rc==1 and g.pend is None and g.snapshot()==exp,(late,bpp,flags,rc,g.pend)
   g.call('VX2D_control',(0xFFFFFE47,ARENA+0x3000),strict=False);r=[g.word(ARENA+0x3000+4*i) for i in range(4)];assert r[0]>=2 and r[3]==0,r
 cases.append('.593: a fill whose first 1, 2 or 3 status reads are 00000000 before the engine shows busy is finished when the request returns (GDI and DirectDraw, 16 and 32 bpp, nothing pending, the ring-paused rule not in use)')
 return cases
if __name__=='__main__':
 c=checks();(ROOT/'verification'/'ENGINE.json').write_text(json.dumps({'passed':len(c),'cases':c,'scope':__doc__},indent=2)+'\n');print(f'{len(c)} compiled-VxD checks passed')
