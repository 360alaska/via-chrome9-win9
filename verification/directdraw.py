#!/usr/bin/env python3
"""Execute the built PE HAL callbacks, bridging IOCTL packets to the built VxD.
The DirectDraw runtime, Win98 loader, allocation and monitor are synthetic.
"""
import struct,re,json,random
from engine import GPU,FB,ARENA,pack
from flip import geometry,REQ,REP
from validate import ROOT,u16,u32
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE
from unicorn.x86_const import *
class HAL:
 def __init__(self,bpp=32,geo=None,ovl=0,**gpu_attrs):
  self.gpu=GPU(bpp);self.u=Uc(UC_ARCH_X86,UC_MODE_32);u=self.u
  if geo:geometry(self.gpu,geo[0],geo[1],bpp)
  if ovl:self.gpu.call('VX2D_control',(0xF8000000|ovl,ARENA+0x5100)) # v10.106: ACCEL_OVERLAY mask before DriverInit
  for k,v in gpu_attrs.items():setattr(self.gpu,k,v)
  b=(ROOT/'driver'/'VXDDRAW.DLL').read_bytes();h=u32(b,60);opt=h+24;self.base=u32(b,opt+28)
  self.sections=[];u.mem_map(self.base,(u32(b,opt+56)+4095)&~4095);u.mem_write(self.base,b[:u32(b,opt+60)])
  for i in range(u16(b,h+6)):
   a=opt+u16(b,h+20)+i*40;v=u32(b,a+12);n=u32(b,a+16);off=u32(b,a+20)
   self.sections.append((v,max(n,u32(b,a+8)),off));u.mem_write(self.base+v,b[off:off+n])
  u.mem_map(0x10000000,0x10000);u.mem_map(0,0x200000);u.mem_map(ARENA,0x100000);u.mem_map(FB,0x100000) # the CPU fallback writes video memory directly
  self.sym={}
  for line in (ROOT/'verification'/'VXDDRAW.map').read_text().splitlines():
   m=re.match(r'([0-9a-fA-F]{8})[ +*]+(\S+)',line)
   if m:self.sym[m[2]]=int(m[1],16)
  self.imports={};p=self.base+u32(b,opt+104);slot=0
  while self.word(p+12):
   dll=self.string(self.base+self.word(p+12));thunk=self.word(p) or self.word(p+16);iat=self.word(p+16);j=0
   while self.word(self.base+thunk+4*j):
    rva=self.word(self.base+thunk+4*j);name=self.string(self.base+rva+2);stub=0x10000000+16*slot;slot+=1
    self.imports[name]=stub;u.mem_write(self.base+iat+4*j,pack([stub]));u.mem_write(stub,b'\xc3')
    u.hook_add(UC_HOOK_CODE,self.import_call,(name,dll),stub,stub);j+=1
   p+=20
  # Shared Win16 context and HDA, at the ABI offsets verified separately.
  self.context=ARENA;self.hda=ARENA+0x7000 # the context is 27492 bytes (0x6B64)
  u.mem_write(self.context,bytes(27492));u.mem_write(self.context,pack([27492,FB,0x1000000]));u.mem_write(self.context+704,pack([self.hda]))
  u.mem_write(self.hda,bytes(self.gpu.u.mem_read(self.gpu.hda,320)))
  self.gpu.u.mem_write(self.gpu.addr('vx_status'),pack([0x38395856,124,0x10002]))
  self.call('_DllMain@12',(self.base,1,0));assert self.call('_DriverInit@4',(self.context,))==1
  self.dst=ARENA+0x8000;self.src=ARENA+0x8200;self.dg=ARENA+0x8400;self.sg=ARENA+0x8600;self.bd=ARENA+0x8800
  self.surface(self.dst,self.dg,0x10000,256,50,32);self.surface(self.src,self.sg,0x20000,384,45,40)
 SYNC=0x40000 # the HAL and the engine model are separate emulators: keep their copies of video memory coherent around each hand-over
 def to_gpu(self):self.gpu.u.mem_write(FB,bytes(self.u.mem_read(FB,self.SYNC)))
 def to_hal(self):self.u.mem_write(FB,bytes(self.gpu.u.mem_read(FB,self.SYNC)))
 def word(self,p):return u32(self.u.mem_read(p,4),0)
 def string(self,p):return bytes(self.u.mem_read(p,160)).split(b'\0')[0].decode('ascii')
 def arg(self,i):return self.word(self.u.reg_read(UC_X86_REG_ESP)+4+4*i)
 def import_call(self,u,a,z,data):
  name,dll=data;args=0;ret=1
  if name=='CreateFileA':
   args=7;path=self.string(self.arg(0))
   if path in ('C:\\VXDDRAW.LOG','C:\\VXD3D.LOG'):ret=0x77 # diagnostic logs, discarded (v10.191: the HAL event log)
   else:assert path=='\\\\.\\VX85598';ret=0x66
  elif name in ('WriteFile',):args=5
  elif name=='SetFilePointer':args=4;ret=0
  elif name=='GetTickCount':args=0;ret=1234
  elif name=='GetLastError':args=0;ret=31 # v10.479: after a failed VxD call (ERROR_GEN_FAILURE)
  elif name=='Sleep':args=1;ret=0 # v10.218: the HAL waits between busy retries
  elif name=='GetCurrentThreadId':args=0;ret=0x2000
  elif name=='GetFileSize':args=2;ret=0
  elif name=='SetEndOfFile':args=1;ret=1
  elif name=='GetFileAttributesA':args=1;ret=0xFFFFFFFF # no diagnostic marker file
  elif name=='GetCurrentProcessId':args=0;ret=0x1000
  elif name=='InterlockedExchange':
   args=2;p=self.arg(0);ret=self.word(p);u.mem_write(p,pack([self.arg(1)]))
  elif name=='GetModuleFileNameA':
   args=3;text=b'C:\\TEST\\BENCH.EXE\0';u.mem_write(self.arg(1),text);ret=len(text)-1
  elif name=='DeviceIoControl':
   args=8;code=self.arg(1);ip,n,op,z=self.arg(2),self.arg(3),self.arg(4),self.arg(5)
   if code in getattr(self,'fail_ioctl',()):ret=0;blob=None # v10.479: a VxD call made to fail
   elif code==0x85500001:blob=bytes(self.gpu.u.mem_read(self.gpu.addr('vx_status'),124))
   elif code==0x85500016:
    self.gpu.call('VX2D_stats',(ARENA+0x5000,));blob=bytes(self.gpu.u.mem_read(ARENA+0x5000,80))
   elif code==0x85500017:blob=pack([self.gpu.call('VX2D_access',(self.word(ip),))])
   elif code==0x85500012:
    v=self.word(ip);self.ctrl_log=getattr(self,'ctrl_log',[])+[v];self.gpu.call('VX2D_control',(v,ARENA+0x5100));blob=bytes(self.gpu.u.mem_read(ARENA+0x5100,16))
   elif code==0x85500015:
    assert n==372
    q=list(struct.unpack('<93I',u.mem_read(ip,n)));self.last=q;self.to_gpu();blob=pack([self.gpu.submit(q)]);self.to_hal()
   elif code==0x8550001B:
    assert n==1040 and z==4;self.gpu.u.mem_write(ARENA+0x7000,bytes(u.mem_read(ip,n)));blob=pack([self.gpu.call('VX_dac',(ARENA+0x7000,))])
   elif code==0x85500018:
    assert n==16 and z==64;self.gpu.u.mem_write(REQ,bytes(u.mem_read(ip,16)));self.gpu.call('VX_flip_control',(REQ,REP));blob=bytes(self.gpu.u.mem_read(REP,64))
   elif code==0x8550001C:
    assert n==80 and z==64;self.gpu.u.mem_write(REQ,bytes(u.mem_read(ip,n)));self.gpu.call('VX2D_ovl',(REQ,REP));blob=bytes(self.gpu.u.mem_read(REP,64))
   elif code==0x85500022: # v10.389: TEXDBG 0xBD 0x81 at a flip (the thread's run time): answered as the VxD does
    assert n==8 and z==32 and self.word(ip)==0xBD;blob=pack([0xBD,self.word(ip+4),0,0,0,0,0,0])
   elif code==0x85500028: # v10.432: VX_IOCTL_LOCKMEM (the VxD's own rules are checked in native3d.py): recorded, answered as locked
    assert n==12 and z==16;q=list(struct.unpack('<3I',u.mem_read(ip,12)));self.locks=getattr(self,'locks',[])+[q]
    pages=((q[1]+q[2]-1)>>12)-(q[1]>>12)+1;blob=pack([pages,q[1]&~4095,1,1])
   else:raise AssertionError(('unexpected IOCTL',hex(code)))
   if blob is not None:assert len(blob)==z;u.mem_write(op,blob);u.mem_write(self.arg(6),pack([z]))
  elif name=='EnumDisplaySettingsA':args=3;ret=0
  elif name in ('IsBadReadPtr','IsBadWritePtr'): # v10.474: 0 only when the range is mapped (the Blt signatures scan the stack for return addresses)
   args=2;ret=0
   try:
    if self.arg(1):u.mem_read(self.arg(0),self.arg(1))
   except Exception:ret=1
  elif name=='GlobalAlloc':args=2;ret=ARENA+0x80000;self.allocs=getattr(self,'allocs',0)+1
  elif name=='GlobalFree':args=1;ret=0
  elif name in ('CloseHandle','DisableThreadLibraryCalls','FlushFileBuffers'):args=1
  elif name=='wsprintfA':args=0;u.mem_write(self.arg(0),b'\0');ret=0 # cdecl: the caller pops; the event line is discarded
  else:raise AssertionError(('unexpected import executed',name,dll))
  sp=u.reg_read(UC_X86_REG_ESP);dest=self.word(sp);u.reg_write(UC_X86_REG_EAX,ret);u.reg_write(UC_X86_REG_ESP,sp+4+4*args);u.reg_write(UC_X86_REG_EIP,dest)
 def call(self,n,args=()):
  sp=0x1f0000;self.u.mem_write(sp,pack([0xf0000,*args]));self.u.reg_write(UC_X86_REG_ESP,sp)
  self.u.emu_start(self.sym[n],0xf0000,count=2000000)
  assert self.u.reg_read(UC_X86_REG_EIP)==0xf0000 and self.u.reg_read(UC_X86_REG_ESP)==sp+4+4*len(args),n
  return self.u.reg_read(UC_X86_REG_EAX)
 def surface(self,l,g,offset,pitch,w,h):
  self.u.mem_write(l,bytes(128));self.u.mem_write(l+4,pack([g]));self.u.mem_write(l+32,pack([0x4000])) # DDSCAPS_VIDEOMEMORY
  self.u.mem_write(g,bytes(72));self.u.mem_write(g+20,pack([FB+offset,pitch]));self.u.mem_write(g+28,struct.pack('<HH',h,w))
  self.u.mem_write(l+52,pack([0x1234,0x1234,0x1234,0x1234]))
 def blt(self,flags=0x1000000,key=0,clip=None,fill=False,rop=0x00cc0020):
  b=bytearray(204);struct.pack_into('<II4iI4iII',b,0,0,self.dst,7,6,26,19,0 if fill else self.src,2,3,21,16,flags,0)
  struct.pack_into('<I',b,52,100);struct.pack_into('<I',b,60,rop);struct.pack_into('<I',b,132,0x1234)
  struct.pack_into('<4I',b,136,key,key,key,key)
  if clip:
   at=ARENA+0x9000;self.u.mem_write(at,b''.join(struct.pack('<4i',*r) for r in clip));struct.pack_into('<I',b,160,1);struct.pack_into('<II',b,196,len(clip),at)
  self.u.mem_write(self.bd,bytes(b));n=len(self.gpu.commands);self.to_hal();rc=self.call('_VXBlit@4',(self.bd,));hr=self.word(self.bd+152);self.to_gpu()
  return rc,hr,len(self.gpu.commands)-n

def checks():
 cases=[]
 for bpp in (16,32):
  h=HAL(bpp);g=h.gpu
  assert h.word(h.context+616+8*4)==h.sym['_VXBlit@4'];cases.append(f'{bpp}bpp HAL initialization and callback pointer')
  lock=ARENA+0xa000
  h.u.mem_write(lock,pack([0,h.dst])+bytes(40));assert h.call('_VXLock@4',(lock,))==0 and g.excluded==0
  h.surface(h.dst,h.dg,0,g.pitch,64,48)
  assert h.call('_VXLock@4',(lock,))==0 and g.excluded==1
  assert h.call('_VXUnlock@4',(lock,))==0 and g.excluded==0
  cases.append(f'{bpp}bpp DirectDraw primary Lock/Unlock excludes and restores cursor')
  h.surface(h.dst,h.dg,0x10000,256,50,32)
  desc=ARENA+0xa100;can=ARENA+0xa200
  h.u.mem_write(desc,bytes(108));h.u.mem_write(desc,pack([108]));h.u.mem_write(desc+104,pack([0x4040]));h.u.mem_write(can,pack([0,desc,0,0,0]))
  assert h.call('_VXCanCreate@4',(can,))==1 and h.word(can+12)==0
  for cap in (0x1000,0x80,0x20000):
   h.u.mem_write(desc+104,pack([cap]));assert h.call('_VXCanCreate@4',(can,))==1 and h.word(can+12)!=0
  h.u.mem_write(desc+104,pack([0x800]));assert h.call('_VXCanCreate@4',(can,))==0 and h.word(can+12)==0
  cases.append(f'{bpp}bpp CanCreate rejects texture/overlay/z-buffer requests and leaves system-memory surfaces to the runtime')
  for cap in (0x10,8,0x18|0x200|0x4000,0x20|0x4000):
   h.u.mem_write(desc+104,pack([cap]));assert h.call('_VXCanCreate@4',(can,))==1 and h.word(can+12)==0,cap
  cases.append(f'{bpp}bpp CanCreate accepts flip chains when the VxD verified flipping')
  for name,flags,fill,key in [('fill',0x1000400,True,0),('copy',0x1000000,False,0),('source key',0x1008000,False,0x1234),('destination key',0x1002000,False,0x1234),('source override',0x1010000,False,0),('destination override',0x1004000,False,0)]:
   g.prefill(0x10000,256,50,32);g.prefill(0x20000,384,45,40);before=g.snapshot();rc,hr,n=h.blt(flags,key,fill=fill)
   assert (rc,hr,n)==(1,0,1),(name,rc,hex(hr),n)
   assert g.snapshot()==g.reference(h.last,before),name;cases.append(f'{bpp}bpp DirectDraw {name} -> VxD -> native engine')
  if bpp==32:
   # HCM Core/2D p79: alpha always passes key comparison.
   h.u.mem_write(h.sg+20,pack([FB+0x20000]));key=0x1234
   for y in range(40):
    g.u.mem_write(FB+0x20000+y*384,pack([(0xA5000000 if x&1 else 0xFF000000)|key for x in range(45)]))
   before=g.snapshot();assert h.blt(0x1008000,key)==(1,0,1);assert g.snapshot()==before
   cases.append('32bpp XRGB source key ignores differing unused alpha bytes')
  assert h.blt(clip=[(8,7,15,13),(16,14,24,18)])==(1,0,2);cases.append(f'{bpp}bpp DirectDraw clip list')
  # v10.84: DDBLT_ROP with a ROP other than SRCCOPY (VX2ROP: exact on the engine). Source ROPs between video surfaces are one engine copy, BLACKNESS/WHITENESS/DSTINVERT are engine fills,
  # anything that needs a pattern, a key, a clip list, or the CPU goes back to the runtime before a command is sent.
  for rop in (0x008800C6,0x00EE0086,0x00660046,0x00330008,0x00440328,0x001100A6,0x00BB0226):
   g.prefill(0x10000,256,50,32);g.prefill(0x20000,384,45,40);before=g.snapshot();rc,hr,n=h.blt(0x1020000,rop=rop)
   assert (rc,hr,n)==(1,0,1),(hex(rop),rc,hr,n);assert h.last[25]==(rop>>16)&255 and g.snapshot()==g.reference(h.last,before),hex(rop);cases.append(f'{bpp}bpp DirectDraw ROP {(rop>>16)&255:02x} source copy -> engine')
  for rop in (0x00000042,0x00FF0062,0x00550009):
   g.prefill(0x10000,256,50,32);before=g.snapshot();rc,hr,n=h.blt(0x1020000,fill=True,rop=rop)
   assert (rc,hr,n)==(1,0,1),(hex(rop),rc,hr,n);assert h.last[2]==1 and h.last[25]==(rop>>16)&255 and g.snapshot()==g.reference(h.last,before),hex(rop);cases.append(f'{bpp}bpp DirectDraw ROP {(rop>>16)&255:02x} without a source -> engine fill')
  assert h.blt(0x1020000,fill=True,rop=0x00AA0029)==(1,0,0);cases.append(f'{bpp}bpp DirectDraw ROP AA (destination unchanged) is handled with no command')
  for name,kw in (('pattern ROP',dict(rop=0x00FB0A09)),('pattern fill ROP',dict(rop=0x00F00021,fill=True)),('ROP with a clip list',dict(rop=0x008800C6,clip=[(8,7,15,13),(16,14,24,18)])),('ROP with a source key',dict(flags=0x1028000,key=0x1234,rop=0x008800C6)),('ROP with extra high bits',dict(rop=0x018800C6))):
   rc,hr,n=h.blt(kw.pop('flags',0x1020000),kw.pop('key',0),**kw);assert (rc,hr,n)==(0,0,0),(name,rc,hr,n);cases.append(f'{bpp}bpp DirectDraw {name} goes back to the runtime before any command')
  for flags in (0x1000000|0x2000000,0x1000000|0x100,0x1000000|0xa000):
   assert h.blt(flags)[0::2]==(0,0);cases.append(f'{bpp}bpp reject unsupported flags {flags:x} before command')
  # v10.58zb: when the engine declines a blit the HAL finishes it on the CPU (DirectDraw's own code cannot blit video surfaces).
  g.call('VX2D_control',(2,ARENA+0x5000))
  def cpu(name,flags,key=0,fill=False,clip=None):
   g.prefill(0x10000,256,50,32);g.prefill(0x20000,384,45,40);before=g.snapshot();rc,hr,n=h.blt(flags,key,clip=clip,fill=fill)
   assert (rc,hr,n)==(1,0,0),(name,rc,hex(hr),n)
   if clip:
    want=before
    for r in clip:
     q=list(h.last);q[3]|=1;q[19:23]=list(r);want=g.reference(q,want)
   else:want=g.reference(h.last,before)
   assert g.snapshot()==want,name;cases.append(f'{bpp}bpp CPU fallback {name} matches the engine model')
  for name,flags,fill,key in [('fill',0x1000400,True,0),('copy',0x1000000,False,0),('source key',0x1008000,False,0x1234),('destination key',0x1002000,False,0x1234),('source override',0x1010000,False,0),('destination override',0x1004000,False,0)]:
   cpu(name,flags,key,fill)
  cpu('clip list copy',0x1000000,clip=[(8,7,15,13),(16,14,24,18)])
  assert h.blt(0x1020000,rop=0x008800C6)==(0,0,0) and h.blt(0x1020000,fill=True,rop=0x00000042)==(0,0,0);cases.append(f'{bpp}bpp DirectDraw ROP declined by the engine goes back to the runtime, nothing drawn on the CPU')
  cpu('clip list fill',0x1000400,fill=True,clip=[(8,7,15,13),(16,14,24,18)])
  h.surface(h.src,h.sg,0x10000,256,50,32) # source and destination are one surface: the CPU copy must walk backwards
  cpu('overlapping self copy',0x1000000)
  h.surface(h.src,h.sg,0x20000,384,45,40)
  g.call('VX2D_control',(0,ARENA+0x5000));g.u.mem_write(g.addr('vx2d_fill_min'),pack([131072]));g.u.mem_write(g.addr('vx2d_copy_min'),pack([1024]))
  g.prefill(0x10000,256,50,32);g.prefill(0x20000,384,45,40);before=g.snapshot();rc,hr,n=h.blt(0x1000400,fill=True)
  assert (rc,hr,n)==(1,0,0) and g.snapshot()==g.reference(h.last,before);cases.append(f'{bpp}bpp small DirectDraw fill under the size threshold is finished on the CPU')
  g.u.mem_write(g.addr('vx2d_fill_min'),pack([0]));g.u.mem_write(g.addr('vx2d_copy_min'),pack([0]))
  g.call('VX2D_control',(1,ARENA+0x5000));g.timeout=True;rc,hr,n=h.blt();assert rc==1 and hr==0x887601c2 and n==1;cases.append(f'{bpp}bpp fired timeout returns SURFACELOST without replay')
 return cases+cpu_checks()+flip_checks()+palette_checks()+blt_class_checks()
class Surf:
 """A surface in the HAL emulator's memory: video (inside the mapped frame buffer) or system memory."""
 def __init__(self,h,l,g,kind,addr,pitch,w,hgt,px):
  self.h,self.kind,self.addr,self.pitch,self.w,self.hgt,self.px,self.l=h,kind,addr,pitch,w,hgt,px,l
  h.u.mem_write(l,bytes(128));h.u.mem_write(l+4,pack([g]));h.u.mem_write(l+32,pack([0x4000 if kind=='v' else 0x800]))
  h.u.mem_write(g,bytes(72));h.u.mem_write(g+20,pack([addr,pitch]));h.u.mem_write(g+28,struct.pack('<HH',hgt,w))
  h.u.mem_write(l+52,pack([0x1234,0x1234,0x1234,0x1234]))
 def fill(self,rnd,keys=(0x1234,)):
  m=(1<<8*self.px)-1;rows=bytearray()
  for y in range(self.hgt):
   row=b''.join(((rnd.choice(keys)&m) if rnd.random()<0.3 else rnd.getrandbits(8*self.px)&m).to_bytes(self.px,'little') for x in range(self.w))
   rows+=row+bytes(self.pitch-len(row))
  self.h.u.mem_write(self.addr,bytes(rows))
 def dump(self):return bytearray(self.h.u.mem_read(self.addr,self.pitch*self.hgt))
 def get(self,buf,x,y):o=y*self.pitch+x*self.px;return int.from_bytes(buf[o:o+self.px],'little')
 def put(self,buf,x,y,v):o=y*self.pitch+x*self.px;buf[o:o+self.px]=(v&((1<<8*self.px)-1)).to_bytes(self.px,'little')
def blt_class_checks():
 """v10.473: VX_DD_QueryBlts: a colour fill and a copy between two video-memory surfaces (neither front, back buffer nor texture) counted
 under their kind and destination, the copy's source counted, the destination's pixels / 256, and the counts cleared on request"""
 h=HAL(32);q=ARENA+0x9800
 def query(clear=False):
  assert h.call('_VX_DD_QueryBlts@8',(q,53|(0x80000000 if clear else 0)))==53;return [h.word(q+4*k) for k in range(53)]
 query(True);assert sum(query()[1:])==0
 h.blt(flags=0x1000400,fill=True);h.blt();b=query()
 assert b[1+0*4+3]==1 and b[1+2*4+3]==1 and sum(b[1:17])==2,b[1:17] # a colour fill and a copy, to another surface
 assert b[49:53]==[0,0,0,1] and b[33+0*4+3]==(19*13+128)>>8,(b[49:53],b[33:49])
 assert h.call('_VX_DD_QueryBlts@8',(q,69))==69 and sum(h.word(q+4*k) for k in range(53,69))<=2 # v10.474: the CPU blits by class, at most the two Blts
 g=ARENA+0x9a00;assert h.call('_VX_DD_QueryBltSigs@8',(g,226))==226;w=[h.word(g+4*k) for k in range(226)] # v10.474: the Blts by signature
 assert w[0]==2 and w[1]==0,w[:2];k0,k1=w[2:30],w[30:58]
 assert k0[0]&0x400 and k0[3]==(19<<16|13) and k0[6]==1 and k1[6]==1 and k1[3]==(19<<16|13) and k1[4]==(19<<16|13) and k1[5]==0,(k0,k1)
 h.blt();assert h.call('_VX_DD_QueryBltSigs@8',(g,226|0x80000000))==226 and h.word(g+30*4+6*4)==2 # the same copy again: one kind, two calls; then cleared
 assert h.call('_VX_DD_QueryBltSigs@8',(g,226))==226 and h.word(g)==0
 query(True);assert sum(query()[1:])==0
 return ['v10.473: the Blts counted by kind (colour fill, copy) and destination, the copies by their source, the pixels; cleared on request; v10.474: the CPU blits, the Blts by signature (flags, caps, sizes, one surface or two), a repeat counted under its kind']

def blt2(h,dst,src,dr,sr,flags=0,fx=0,key=0,clip=None,fill=None):
 b=bytearray(204);struct.pack_into('<II4iI4iII',b,0,0,dst.l,*dr,src.l if src else 0,*(sr or (0,0,0,0)),flags|0x1000000,0)
 struct.pack_into('<I',b,52,100);struct.pack_into('<I',b,56,fx);struct.pack_into('<I',b,60,0x00cc0020);struct.pack_into('<I',b,132,fill or 0)
 struct.pack_into('<4I',b,136,key,key,key,key)
 if clip:
  at=ARENA+0x9000;h.u.mem_write(at,b''.join(struct.pack('<4i',*r) for r in clip));struct.pack_into('<I',b,160,1);struct.pack_into('<II',b,196,len(clip),at)
 h.u.mem_write(h.bd,bytes(b));n=len(h.gpu.commands);h.to_gpu();rc=h.call('_VXBlit@4',(h.bd,));hr=h.word(h.bd+152);h.to_gpu()
 return rc,hr,len(h.gpu.commands)-n
def ref_blt(dst,src,before,dr,sr,fx=0,key=0,mode=None,clip=None,fill=None,srcbuf=None):
 """Independent model: point sampling at pixel centres, mirrored from the far edge, clipped per rectangle."""
 exp=bytearray(before);px=dst.px;m=0xFF if px==1 else 0xFFFF if px==2 else 0xFFFFFF;key&=m
 rects=clip or [dr];dw,dh=dr[2]-dr[0],dr[3]-dr[1]
 for (x0,y0,x1,y1) in rects:
  for y in range(y0,y1):
   for x in range(x0,x1):
    if fill is not None:dst.put(exp,x,y,fill);continue
    sw,sh=sr[2]-sr[0],sr[3]-sr[1];sx=((2*(x-dr[0])+1)*sw)//(2*dw);sy=((2*(y-dr[1])+1)*sh)//(2*dh)
    if fx&2:sx=sw-1-sx
    if fx&4:sy=sh-1-sy
    v=src.get(srcbuf,sr[0]+sx,sr[1]+sy)
    if mode=='skey' and (v&m)==key:continue
    if mode=='dkey' and (dst.get(exp,x,y)&m)!=key:continue
    dst.put(exp,x,y,v)
 return exp
def cpu_checks():
 cases=[];rnd=random.Random(1998)
 for bpp in (8,16,32):
  px=bpp//8;h=HAL(bpp);g=h.gpu;SYS=ARENA+0x40000;E=1 # v10.76: the engine is used at 8bpp too (same-size copies and fills; stretch, mirror and system memory stay on the CPU)
  def mk(kind,i,w,hgt,pitch=None,off=None):
   pitch=pitch or ((w*px+7)&~7)+16
   if kind=='v':return Surf(h,ARENA+0xc000+0x100*i,ARENA+0xc080+0x100*i,'v',FB+(off if off is not None else 0x10000+0x8000*i),pitch,w,hgt,px)
   return Surf(h,ARENA+0xc000+0x100*i,ARENA+0xc080+0x100*i,'s',SYS+off,pitch,w,hgt,px)
  def run(name,dst,src,dr,sr,expect,**kw):
   dst.fill(rnd);
   if src and src is not dst:src.fill(rnd)
   before=dst.dump();sb=src.dump() if src else None
   mode=kw.pop('mode',None);flags=kw.pop('flags',0);fx=kw.pop('fx',0);key=kw.pop('key',0x1234);clip=kw.pop('clip',None);fill=kw.pop('fill',None)
   fl=flags|(0x800 if fx else 0)|(0x8000 if mode=='skey' else 0)|(0x2000 if mode=='dkey' else 0)|(0x400 if fill is not None else 0)
   keyarg=key if mode else 0
   rc,hr,n=blt2(h,dst,src,dr,sr,fl&~0x8000&~0x2000|(0x10000 if mode=='skey' else 0)|(0x4000 if mode=='dkey' else 0),fx,keyarg,clip,fill)
   assert (rc,hr)==(1,0),(name,rc,hex(hr));assert n==expect,(name,n)
   want=ref_blt(dst,src,before,dr,sr,fx,key,mode,clip,fill,sb);got=dst.dump()
   assert got==want,name
   cases.append(f'{bpp}bpp {name}')
  v1=mk('v',0,48,40);v2=mk('v',1,64,48);s1=mk('s',2,64,48,off=0);s2=mk('s',3,64,48,off=0x8000);v3=mk('v',4,64,48,off=0x40000)
  run('stretch 2x video to video on the CPU',v1,v2,(4,3,44,27),(2,2,22,14),0)
  run('shrink video to video on the CPU',v1,v2,(1,1,18,12),(0,0,40,30),0)
  run('odd-ratio stretch',v1,v2,(2,2,31,21),(5,4,18,11),0)
  run('stretch width only',v1,v2,(2,2,42,12),(5,4,25,14),0)
  run('stretch height only',v1,v2,(2,2,22,30),(5,4,25,14),0)
  run('mirror left-right',v1,v2,(3,3,22,16),(4,5,23,18),0,fx=2)
  run('mirror up-down',v1,v2,(3,3,22,16),(4,5,23,18),0,fx=4)
  run('mirror both',v1,v2,(3,3,22,16),(4,5,23,18),0,fx=6)
  run('stretch with mirror',v1,v2,(3,3,40,25),(4,5,20,15),0,fx=2)
  run('stretch with source colour key',v1,v2,(3,3,40,25),(4,5,20,15),0,mode='skey')
  run('stretch with destination colour key',v1,v2,(3,3,40,25),(4,5,20,15),0,mode='dkey')
  run('stretch with clip list',v1,v2,(3,3,40,25),(4,5,20,15),0,clip=[(5,4,12,9),(20,10,30,24),(3,3,6,6)])
  run('same-size copy with DDFX and no mirror stays on the engine' if E else 'same-size copy with DDFX and no mirror',v1,v2,(3,3,22,16),(4,5,23,18),E,fx=8)
  run('video to video same size copy uses the engine' if E else 'video to video same size copy',v1,v2,(3,3,22,16),(4,5,23,18),E)
  run('stretch inside one surface (no overlap)',v1,v1,(0,0,10,10),(20,20,25,25),0)
  run('system to video copy',v1,s1,(3,3,30,20),(2,4,29,21),0)
  run('system to video copy with source key',v1,s1,(3,3,30,20),(2,4,29,21),0,mode='skey')
  run('system to video stretch',v1,s1,(0,0,48,40),(0,0,24,20),0)
  run('system to video mirror',v1,s1,(3,3,30,20),(2,4,29,21),0,fx=6)
  run('video to system copy',s1,v2,(3,3,30,20),(2,4,29,21),0)
  run('video to system stretch',s1,v2,(0,0,50,30),(0,0,30,20),0)
  run('system fill',s1,None,(4,4,30,22),None,0,fill=0x5a5a5a5a&((1<<bpp)-1))
  run('video fill with the engine' if E else 'video fill',v1,None,(4,4,30,22),None,E,fill=0x123456&((1<<bpp)-1))
  s2.fill(rnd);s1.fill(rnd);before=s1.dump();rc,hr,n=blt2(h,s1,s2,(0,0,10,10),(0,0,10,10));assert (rc,n)==(0,0) and s1.dump()==before;cases.append(f'{bpp}bpp system to system is left to the runtime')
  for name,kw in (('rotation',dict(fx=0x100)),('z buffer',dict(fx=0x1000))):
   v1.fill(rnd);v2.fill(rnd);before=v1.dump();rc,hr,n=blt2(h,v1,v2,(0,0,10,10),(0,0,10,10),0x800,kw['fx']);assert (rc,n)==(0,0) and v1.dump()==before,name
  cases.append(f'{bpp}bpp unsupported effects are refused before anything is drawn')
  v1.fill(rnd);before=v1.dump();rc,hr,n=blt2(h,v1,v2,(40,30,60,50),(0,0,20,20));assert (rc,n)==(0,0) and v1.dump()==before;cases.append(f'{bpp}bpp destination outside the surface is refused')
  # v10.368 (audit N6): an overlapping stretch, mirror or clipped copy in one surface is done from a copy of the source taken first (it was
  # NOTHANDLED: E_NOTIMPL for a video surface); a clipped copy whose rectangles are apart stays on the engine
  run('overlapping stretch in one surface from a staged source',v1,v1,(0,0,20,20),(10,10,20,20),0)
  run('overlapping mirror in one surface from a staged source',v1,v1,(2,2,22,22),(6,6,26,26),0,fx=2)
  run('clipped overlapping copy in one surface from a staged source',v1,v1,(4,4,24,24),(0,0,20,20),0,clip=[(4,4,14,14),(14,14,24,24),(4,14,14,24)])
  run('clipped copy in one surface, rectangles apart, on the engine',v1,v1,(24,20,44,36),(0,0,20,16),3*E,clip=[(24,20,30,26),(34,28,44,36),(24,30,28,36)])
 # v10.285 (review): a tall clipped stretch: (2n+1)*src overflowed 32 bits (a source of 65535 rows, destination rows near 40000)
 h=HAL(32);SYS=ARENA+0x40000
 src=Surf(h,ARENA+0xc000,ARENA+0xc080,'s',SYS,4,1,65535,4);dst=Surf(h,ARENA+0xc100,ARENA+0xc180,'v',FB+0x10000,16,1,40000,4)
 h.u.mem_write(SYS,b''.join(struct.pack('<I',0xFF000000|y) for y in range(65535)));h.u.mem_write(FB+0x10000,bytes(16*40000))
 rc,hr,n=blt2(h,dst,src,(0,0,1,40000),(0,0,1,65535),clip=[(0,39990,1,40000)])
 assert rc==1 and hr==0,(rc,hex(hr))
 got=[struct.unpack('<I',bytes(h.u.mem_read(FB+0x10000+16*y,4)))[0]&0xFFFFFF for y in range(39990,40000)]
 want=[((2*y+1)*65535)//80000 for y in range(39990,40000)]
 assert got==want,(got,want)
 cases.append('a tall clipped stretch (65535 source rows to 40000, the last ten) reads the rows a 64-bit reference gives (it wrapped at 32 bits)')
 return cases

def flip_checks():
 cases=[]
 for bpp in (16,32):
  px=bpp//8;stride=1024*px*768;pitch=1024*px
  h=HAL(bpp,geo=(1024,768));g=h.gpu
  cb=lambda i:h.word(h.context+616+4*i)
  assert cb(4)==h.sym['_VXFlip@4'] and cb(12)==h.sym['_VXGetFlipStatus@4'] and cb(16)==h.sym['_VXWaitVB@4'] and cb(18)==h.sym['_VXSetExclusive@4']
  cases.append(f'{bpp}bpp DriverInit registers the flip callbacks after the VxD verified display-start flipping')
  front,back=ARENA+0xb000,ARENA+0xb100;fg,bg=ARENA+0xb200,ARENA+0xb300;h.surface(front,fg,0,pitch,1024,768);h.surface(back,bg,stride,pitch,1024,768)
  data=ARENA+0xb400
  def flip(flags=0,target=back):
   h.u.mem_write(data,pack([0,front,target,flags,0x1234,0,0,0]));rc=h.call('_VXFlip@4',(data,));return rc,h.word(data+16)
  v=g.vbn;assert flip()==(1,0) and g.crt_start()==stride>>1 and g.vbn>v;cases.append(f'{bpp}bpp Flip callback moves the display start to the target surface')
  v=g.vbn;assert flip(8,front)==(1,0) and g.crt_start()==0 and g.vbn>v;cases.append(f'{bpp}bpp .555: DDFLIP_NOVSYNC flips as the one-frame flip (the old front guarded until the blank)')
  assert flip()==(1,0)
  # a surface that is not a whole screen at the scanout pitch cannot be flipped to
  odd=ARENA+0xb500;og=ARENA+0xb600;h.surface(odd,og,stride,pitch//2,512,768);before=g.crt_start()
  rc,hr=flip(0,odd);assert rc==1 and hr==0x88760246 and g.crt_start()==before;cases.append(f'{bpp}bpp Flip refuses a target that is not a full scanout surface')
  gf=ARENA+0xb700;h.u.mem_write(gf,pack([0,front,0,0]));assert h.call('_VXGetFlipStatus@4',(gf,))==1 and h.word(gf+12)==0;cases.append(f'{bpp}bpp GetFlipStatus reports a finished flip')
  # .576 (peer review of .572, finding 9): a flip that must not wait (DDFLIP_DONOTWAIT, 0x20) with the flip before still pending answers
  # WASSTILLDRAWING and moves nothing; once the display has taken it the flip goes
  assert flip(0,front)==(1,0);before=g.crt_start();rc,hr=flip(0x20,back);assert (rc,hr)==(1,0x8876021C) and g.crt_start()==before,(rc,hex(hr),hex(g.crt_start()),hex(before))
  for _ in range(2000):
   h.u.mem_write(gf,pack([0,front,2,0]));assert h.call('_VXGetFlipStatus@4',(gf,))==1
   if h.word(gf+12)==0:break
  assert h.word(gf+12)==0;rc,hr=flip(0x20,back);assert (rc,hr)==(1,0) and g.crt_start()!=before,(rc,hex(hr),hex(g.crt_start()))
  assert flip(0,front)==(1,0)
  cases.append(f'{bpp}bpp .576: a DONOTWAIT flip with the flip before pending answers still drawing and moves nothing')
  # v10.479 (review): a flip possibly pending and the VxD's flip call failing: GetFlipStatus and a write Lock of the old front answer
  # SURFACELOST (were DD_OK, the Lock going on without the wait); once the VxD answers again they go on as before
  assert flip()==(1,0);h.fail_ioctl={0x85500018}
  h.u.mem_write(gf,pack([0,front,2,0]));assert h.call('_VXGetFlipStatus@4',(gf,))==1 and h.word(gf+12)==0x887601C2,hex(h.word(gf+12))
  lk=ARENA+0xbc00;h.u.mem_write(lk,pack([0,front,0,0,0,0,0,0,0,0,0]))
  assert h.call('_VXLock@4',(lk,))==1 and h.word(lk+32)==0x887601C2,hex(h.word(lk+32))
  h.fail_ioctl=();h.u.mem_write(gf,pack([0,front,2,0]));assert h.call('_VXGetFlipStatus@4',(gf,))==1 and h.word(gf+12) in (0,0x8876021C),hex(h.word(gf+12))
  cases.append(f'{bpp}bpp v10.479: with a flip pending and the VxD not answering, GetFlipStatus and a write Lock answer SURFACELOST instead of going on')
  wb=ARENA+0xb800
  for flags,name in ((1,'begin'),(4,'end')):
   h.u.mem_write(wb,pack([0,flags,0,0,0x1234,0]));assert h.call('_VXWaitVB@4',(wb,))==1 and h.word(wb+16)==0,name
  h.u.mem_write(wb,pack([0,0x80000006,7,0,0x1234,0]));assert h.call('_VXWaitVB@4',(wb,))==1 and h.word(wb+8) in (0,1);cases.append(f'{bpp}bpp WaitForVerticalBlank begin, end and test requests')
  h.u.mem_write(wb,pack([0,0x10,0,0,0x1234,0]));assert h.call('_VXWaitVB@4',(wb,))==0;cases.append(f'{bpp}bpp unknown vertical-blank request is left to the runtime')
  se=ARENA+0xb900;h.u.mem_write(se,pack([0,1,0,0x1234,0]));assert h.call('_VXSetExclusive@4',(se,))==0 and g.crt_start()==stride>>1
  h.u.mem_write(se,pack([0,0,0,0x1234,0]));assert h.call('_VXSetExclusive@4',(se,))==0 and g.crt_start()==0;cases.append(f'{bpp}bpp leaving exclusive mode puts the desktop back')
  cl=[v for v in getattr(h,'ctrl_log',[]) if v>>8==0xFD0000];assert cl[-2:]==[0xFD000001,0xFD000000],cl;cases.append(f'{bpp}bpp entering and leaving exclusive mode sends the async gate codes to the VxD (on, then off)')
  assert flip()==(1,0);assert h.call('_DllMain@12',(h.base,0,0))==1 and g.crt_start()==0;cases.append(f'{bpp}bpp process exit puts the desktop back')
  # without a blank signal the vertical-blank callback is not registered but flipping still is
  h=HAL(bpp,geo=(1024,768),vb_enabled=False);cb=lambda i:h.word(h.context+616+4*i);assert cb(4)!=0 and cb(16)==0;cases.append(f'{bpp}bpp no blank signal: WaitForVerticalBlank left to the runtime')
 return cases

PAL=19224 # offsets of the palette fields at the end of the shared context: createPalette32, destroyPalette32, setEntries32, setPalette32, 8 slots (key + 256 colours), activePalette
SLOT=PAL+16;ACTIVE=SLOT+8*1028
def palette_checks():
 """v10.71: 8bpp pixel format, DirectDraw palette callbacks and the DAC. Each callback declines outside 8bpp."""
 cases=[]
 for bpp in (8,16,32):
  h=HAL(bpp);g=h.gpu;g.u.mem_write(g.addr('wram'),pack([ARENA+0x20000]))
  for k,n in enumerate(('_VXCreatePalette@4','_VXDestroyPalette@4','_VXSetEntries@4','_VXSetPalette@4')):assert h.word(h.context+PAL+4*k)==h.sym[n],n
  cases.append(f'{bpp}bpp DriverInit registers the four palette callbacks at every depth')
  # surface formats: 8bpp accepts only palettized 8-bit surfaces, 16/32bpp reject them
  desc=ARENA+0xa100;can=ARENA+0xa200;h.u.mem_write(desc,bytes(108));h.u.mem_write(desc,pack([108]));h.u.mem_write(desc+104,pack([0x4040]))
  def fmt(flags,bits,r,gr,b):
   h.u.mem_write(desc+72,pack([32,flags,0,bits,r,gr,b,0]));h.u.mem_write(can,pack([0,desc,1,0,0]));assert h.call('_VXCanCreate@4',(can,))==1;return h.word(can+12)
  pal8=fmt(0x20|0x40,8,0,0,0)
  rgb16=fmt(0x40,16,0xF800,0x7E0,0x1F);rgb32=fmt(0x40,32,0xFF0000,0xFF00,0xFF)
  want={8:(0,1,1),16:(1,0,1),32:(1,1,0)}[bpp]
  assert tuple(int(x!=0) for x in (pal8,rgb16,rgb32))==want,(pal8,rgb16,rgb32)
  assert fmt(0x40,8,0xE0,0x1C,0x03)!=0 if bpp==8 else True # an RGB 3-3-2 surface is not the palettized format
  cases.append(f'{bpp}bpp CanCreate pixel formats: only the desktop depth is accepted')
  # palette objects
  P1,P2,P3=ARENA+0xd000,ARENA+0xd040,ARENA+0xd080;tab=ARENA+0xe000;ents=ARENA+0xf000
  def colours(seed):return [((seed+i)&255,(255-i)&255,(i*3+seed)&255) for i in range(256)]
  def put(addr,cols):h.u.mem_write(addr,b''.join(bytes([r,gc,b,0]) for r,gc,b in cols))
  def pobj(a,flags=1):h.u.mem_write(a,pack([1,flags,0,0x1000,0,0,0,0]))
  def rgbw(c):return (c[0]<<16)|(c[1]<<8)|c[2]
  def create(pal,cols,flags=1):
   pobj(pal,flags);put(tab,cols);d=ARENA+0xa300;h.u.mem_write(d,pack([0,pal,tab,0x1234,0,0]));rc=h.call('_VXCreatePalette@4',(d,));return rc,h.word(d+12)
  def dac():d=getattr(g,'dac',[]);g.dac=[];return d
  def expect_upload(log,first,cols,bits=6):
   exp=[(0x3c6,0xff),(0x3c8,first)]
   for r,gc,b in cols:exp+= [(0x3c9,r>>2),(0x3c9,gc>>2),(0x3c9,b>>2)] if bits==6 else [(0x3c9,r),(0x3c9,gc),(0x3c9,b)]
   return log==exp
  c1=colours(10);g.dac=[]
  rc,hr=create(P1,c1)
  if bpp!=8:
   assert (rc,hr)==(0,0) and h.word(h.context+SLOT)==0 and not dac();cases.append(f'{bpp}bpp CreatePalette is left to the runtime (software palette)');continue
  assert (rc,hr)==(1,0) and h.word(h.context+SLOT)==P1 and h.word(h.context+SLOT+4)==rgbw(c1[0]) and h.word(h.context+SLOT+4+4*255)==rgbw(c1[255]) and not dac()
  cases.append('8bpp CreatePalette keeps the 256 colours in the shared context and does not touch the DAC')
  assert create(P3,colours(1),flags=1|0x2000)==(0,0) and create(P3,colours(1),flags=0)==(0,0) and h.word(h.context+SLOT+1028)==0
  cases.append('8bpp CreatePalette declines alpha and non-256-colour palettes')
  # attach to the primary surface
  prim,pg,off,og=ARENA+0xb000,ARENA+0xb100,ARENA+0xb200,ARENA+0xb300;h.surface(prim,pg,0,g.pitch,64,48);h.u.mem_write(prim+32,pack([0x4200]));h.surface(off,og,0x10000,256,50,32)
  def setpal(surf,pal,attach):
   d=ARENA+0xa400;h.u.mem_write(d,pack([0,surf,pal,0x1234,0,attach]));rc=h.call('_VXSetPalette@4',(d,));return rc,h.word(d+12)
  def setent(pal,base,cnt,cols):
   put(ents,cols);d=ARENA+0xa500;h.u.mem_write(d,pack([0,pal,base,cnt,ents,0x1234,0]));rc=h.call('_VXSetEntries@4',(d,));return rc,h.word(d+20)
  assert setpal(off,P1,1)==(0,0) and not dac();cases.append('8bpp SetPalette on an offscreen surface is left to the runtime')
  assert setpal(prim,P1,1)==(1,0) and expect_upload(dac(),0,c1) and h.word(h.context+ACTIVE)==P1;cases.append('8bpp attaching a palette to the primary surface loads all 256 colours into the DAC')
  c2=colours(77)[:5];assert setent(P1,10,5,c2)==(1,0) and expect_upload(dac(),10,c2) and h.word(h.context+SLOT+4+4*12)==rgbw(c2[2]);cases.append('8bpp SetEntries on the attached palette writes only the changed DAC entries')
  rc,hr=create(P2,colours(200));assert (rc,hr)==(1,0)
  assert setent(P2,0,256,colours(5))==(1,0) and not dac() and h.word(h.context+SLOT+1028+4+4*3)==rgbw(colours(5)[3]);cases.append('8bpp SetEntries on a palette that is not attached only updates the cache')
  assert setent(P1,250,10,colours(1)[:10])[1]==0x80070057 and setent(P1,0,1,[]) [0]==1;dac();cases.append('8bpp SetEntries beyond entry 255 is refused')
  assert setent(P3,0,1,colours(1)[:1])==(0,0) and not dac();cases.append('8bpp SetEntries on an unknown palette is left to the runtime')
  # GDI palette goes back
  g.u.mem_write(ARENA+0x20000,b''.join(bytes([(i*5)&255,(i*7)&255,(i*11)&255,0]) for i in range(256))) # wram->palette in the VxD model (each entry is a 00RRGGBB DWORD)
  sh=[(g.u.mem_read(ARENA+0x20000+4*i,4)) for i in range(256)]
  assert setpal(prim,0,0)==(1,0);log=dac();assert log[:2]==[(0x3c6,0xff),(0x3c8,0)] and len(log)==2+768 and h.word(h.context+ACTIVE)==0
  exp=[]
  for i in range(256):
   v=int.from_bytes(bytes(sh[i]),'little')&0xFFFFFF;exp+=[(0x3c9,(v>>18)&63),(0x3c9,(v>>10)&63),(0x3c9,(v>>2)&63)]
  assert log[2:]==exp;cases.append('8bpp detaching the palette puts the GDI palette back from the shadow copy')
  assert setpal(prim,P1,1)==(1,0);dac();d=ARENA+0xa600;h.u.mem_write(d,pack([0,P1,0,0]));assert h.call('_VXDestroyPalette@4',(d,))==1 and h.word(h.context+SLOT)==0 and h.word(h.context+ACTIVE)==0 and len(dac())==2+768;cases.append('8bpp destroying the attached palette restores the GDI palette and frees the slot')
  assert setpal(prim,P2,1)==(1,0);dac();se=ARENA+0xb900;h.u.mem_write(se,pack([0,1,0,0x1234,0]));assert h.call('_VXSetExclusive@4',(se,))==0 and not dac()
  h.u.mem_write(se,pack([0,0,0,0x1234,0]));assert h.call('_VXSetExclusive@4',(se,))==0 and len(dac())==2+768 and h.word(h.context+ACTIVE)==0;cases.append('8bpp leaving exclusive mode puts the GDI palette back')
  assert setpal(prim,P2,1)==(1,0);dac();assert h.call('_DllMain@12',(h.base,0,0))==1 and len(dac())==2+768;cases.append('8bpp process exit puts the GDI palette back')
  # a ninth palette finds no slot and stays a software palette
  for i in range(8):h.u.mem_write(h.context+SLOT+i*1028,pack([0]))
  for i in range(8):r=create(ARENA+0xd100+0x40*i,colours(i));assert r==(1,0),(i,r,[hex(h.word(h.context+SLOT+k*1028)) for k in range(8)])
  assert create(ARENA+0xd400,colours(9))==(0,0);cases.append('8bpp a ninth palette is left to the runtime')
 return cases

if __name__=='__main__':
 c=checks();(ROOT/'verification'/'DIRECTDRAW.json').write_text(json.dumps({'passed':len(c),'cases':c,'scope':__doc__},indent=2)+'\n');print(f'{len(c)} compiled-DirectDraw checks passed')
