#!/usr/bin/env python3
"""Execute the delivered NE mode-validation entry with distinct SS and DS.
Windows/VxD dispatch is mocked; Windows itself is not being booted.
"""
import json,re,struct
from validate import ROOT,u16,u32
from unicorn import Uc,UC_ARCH_X86,UC_MODE_16,UC_HOOK_INTR,UC_HOOK_CODE
from unicorn.x86_const import *

class Frontend:
 def __init__(self,registration=True,api_available=True,vdd_available=True,protected=False,detailed_imports=False,connect=True,backend=None):
  assert backend is not None, 'Pass a synthetic backend adapter'
  self.backend=backend;assert self.backend.call('VESA_init_hw')==1
  self.connected=True;self.calls=[];self.registration=registration;self.api_available=api_available
  self.protected=protected
  self.sel={'code':8,'data':16,'stack':24,'argument':32,'kernel':40,'dib':48,'api':56,'vdd':64} if protected else {'code':0x1000,'data':0x2000,'stack':0x5000,'argument':0x3000,'kernel':0x4000,'dib':0x4100,'api':0x6000,'vdd':0x6100}
  b=(ROOT/'driver'/'VX85598.DRV').read_bytes();h=u32(b,60)
  self.u=Uc(UC_ARCH_X86,UC_MODE_16);u=self.u;u.mem_map(0,0x100000)
  self.data=[];self.tables=[];self.entries={};self.imports={}
  ep=h+u16(b,h+4);ordinal=1
  while b[ep]:
   cnt,kind=b[ep:ep+2];ep+=2
   if kind==0:ordinal+=cnt;continue
   for _ in range(cnt):
    if kind==255:seg=b[ep+3];eo=u16(b,ep+4);ep+=6
    else:seg=kind;eo=u16(b,ep+1);ep+=3
    self.entries[ordinal]=(seg,eo);ordinal+=1
  for i in range(u16(b,h+0x1c)):
   off,n,flags,minimum=struct.unpack_from('<4H',b,h+u16(b,h+0x22)+i*8)
   off <<= u16(b,h+0x32);data=bytearray(65536);data[:n]=b[off:off+n]
   self.data.append(data);self.tables.append((off,n,flags))
  mod=[];p=h+u16(b,h+0x28);imp=h+u16(b,h+0x2a)
  for i in range(u16(b,h+0x1e)):
   at=imp+u16(b,p+2*i);mod.append(b[at+1:at+1+b[at]].decode())
  for data,(off,n,flags) in zip(self.data,self.tables):
   if not flags&0x100:continue
   count=u16(b,off+n)
   for k in range(count):
    typ,fl,source,a,target=struct.unpack_from('<BBHHH',b,off+n+2+k*8)
    if fl&3==1:
     module=mod[a-1]
     selector={'KERNEL':self.sel['kernel'],'DIBENG':self.sel['dib']}[module];offset=target*16 if detailed_imports else 0
     base={'KERNEL':0x40000,'DIBENG':0x41000}[module]
     self.imports[(module,target)]=base+offset
    else:
     assert fl&3==0,(typ,fl,a)
     if a==255:a,target=self.entries[target]
     assert a in (1,2),(typ,fl,a,target)
     selector=self.sel['code' if a==1 else 'data'];offset=target
    while source!=0xffff:
     nxt=u16(data,source)
     if typ==2:struct.pack_into('<H',data,source,selector)
     elif typ==3:struct.pack_into('<HH',data,source,offset,selector)
     elif typ==5:struct.pack_into('<H',data,source,offset)
     else:raise AssertionError(('NE relocation',typ))
     source=nxt
  u.mem_write(0x10000,bytes(self.data[0]));u.mem_write(0x20000,bytes(self.data[1]))
  # Numeric selector stand-ins use real-mode segment bases in this CPU-only
  # test. Instruction calling conventions and SS/DS separation are exercised;
  # Windows protected-mode selector allocation is not being simulated.
  if protected:
   gdt=bytearray(72)
   for index,base,access in [(1,0x10000,0x9a),(2,0x20000,0x92),(3,0x50000,0x92),(4,0x30000,0x92),(5,0x40000,0x9a),(6,0x41000,0x9a),(7,0x60000,0x9a),(8,0x61000,0x9a)]:
    struct.pack_into('<HHBBBB',gdt,8*index,0xffff,base&0xffff,(base>>16)&255,access,0,(base>>24)&255)
   u.mem_write(0x80000,bytes(gdt));u.reg_write(UC_X86_REG_GDTR,(0,0x80000,71,0));u.reg_write(UC_X86_REG_CR0,1)
  for reg,key in [(UC_X86_REG_CS,'code'),(UC_X86_REG_DS,'data'),(UC_X86_REG_SS,'stack'),(UC_X86_REG_ES,'argument')]:u.reg_write(reg,self.sel[key])
  if protected:
   # Unicorn's UC_MODE_16 reg_write on data segment registers retains real
   # segment bases. Load them with guest MOV instructions after enabling PE.
   bootstrap=b'\xb8\x10\x00\x8e\xd8\xb8\x20\x00\x8e\xc0\xb8\x18\x00\x8e\xd0'
   u.mem_write(0x1f100,bootstrap)
   u.emu_start(0xf100+(self.sel['code']<<4),0x1f100+len(bootstrap),count=9)
  u.reg_write(UC_X86_REG_EFLAGS,0x202)
  self.sym={}
  for line in (ROOT/'verification'/'VX85598_DRV.map').read_text().splitlines():
   m=re.match(r'(\d{4}):([0-9a-fA-F]{4})[ *+]*\s+(\S+)',line)
   if m:self.sym[m[3]]=(int(m[1]),int(m[2],16))
  # A mocked VDD registration endpoint; the mode validator does not use HDA.
  u.mem_write(0x20000+self.sym['_VDDEntryPoint'][1],struct.pack('<HH',0,self.sel['vdd'] if vdd_available else 0))
  u.mem_write(0x60000,b'\xcb');u.mem_write(0x61000,b'\xcb')
  u.hook_add(UC_HOOK_INTR,self.interrupt)
  u.hook_add(UC_HOOK_CODE,self.api,None,0x60000,0x60000)
  u.hook_add(UC_HOOK_CODE,self.vdd,None,0x61000,0x61000)
  self.connect_result=self.call('VXD_VM_connect_',far=False) if connect else None

 def interrupt(self,u,num,_):
  assert num==0x2f and u.reg_read(UC_X86_REG_AX)==0x1684,(num,hex(u.reg_read(UC_X86_REG_AX)),hex(u.reg_read(UC_X86_REG_CS)),hex(u.reg_read(UC_X86_REG_IP)),hex(u.reg_read(UC_X86_REG_SP)))
  assert u.reg_read(UC_X86_REG_BX)==0x4333
  u.reg_write(UC_X86_REG_ES,self.sel['api'] if self.api_available else 0);u.reg_write(UC_X86_REG_DI,0)
 def vdd(self,u,address,size,_):
  u.reg_write(UC_X86_REG_ESI,0x3000000 if self.registration else 0)
  u.reg_write(UC_X86_REG_EDX,0x30000000 if self.registration else 0)
  u.reg_write(UC_X86_REG_ECX,0x31000000 if self.registration else 0)
 def api(self,u,address,size,_):
  op=u.reg_read(UC_X86_REG_EDX)&0xffff;self.calls.append(op)
  if op==0x4000:result=1 if self.connected else 0
  elif op==0x4002:
   w=u.reg_read(UC_X86_REG_ESI);h=u.reg_read(UC_X86_REG_EDI);bpp=u.reg_read(UC_X86_REG_ECX)
   result=self.backend.call('VESA_validmode',(w,h,bpp))
  elif op==0x1137:result=getattr(self,'refresh_hz',60) # .577: OP_VX_HZ (the display driver fills DDHALINFO's dwMonitorFrequency)
  else:raise AssertionError(('unhandled frontend service',hex(op),hex(u.reg_read(UC_X86_REG_EAX)),hex(u.reg_read(UC_X86_REG_DS)),bytes(u.mem_read(0x21126,4)).hex(),bytes(u.mem_read(0x50000+u.reg_read(UC_X86_REG_SP),4)).hex()))
  u.reg_write(UC_X86_REG_ECX,result)
 def call(self,name,arguments=b'',far=True,incoming_ds=None):
  u=self.u;segment,off=self.sym[name];assert segment==1
  sp=0xe000;frame=struct.pack('<HH',0xf000,self.sel['code']) if far else struct.pack('<H',0xf000)
  u.mem_write(0x50000+sp,frame+arguments);u.reg_write(UC_X86_REG_SP,sp)
  if incoming_ds is not None:u.reg_write(UC_X86_REG_DS,incoming_ds)
  u.emu_start(off+(u.reg_read(UC_X86_REG_CS)<<4),0x1f000,count=1000000)
  assert u.reg_read(UC_X86_REG_IP)==0xf000
  assert u.reg_read(UC_X86_REG_SP)==sp+len(frame)+len(arguments),(name,hex(u.reg_read(UC_X86_REG_SP)))
  if incoming_ds is not None:assert u.reg_read(UC_X86_REG_DS)==incoming_ds
  return u.reg_read(UC_X86_REG_AX)
 def mode(self,w,h,bpp):
  self.u.mem_write(0x30000,struct.pack('<4H',8,bpp,w,h))
  return self.call('ValidateMode',struct.pack('<HH',0,self.sel['argument']),incoming_ds=self.sel['stack'])
