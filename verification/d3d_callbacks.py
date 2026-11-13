#!/usr/bin/env python3
"""Execute the linked Win32 D3D callbacks with synthetic runtime/VxD services."""
import json,struct
from directdraw import HAL
from engine import ARENA,FB,pack
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from validate import ROOT,u32

class D3D(HAL):
 def __init__(self,bpp=32,vram_bytes=0x1000000,ovl=0):
  self.native=[];self.native_results=[];self.pid=0x101;self.generation=1;self.fb_bytes=vram_bytes
  if ovl:super().__init__(bpp,ovl=ovl)
  else:super().__init__(bpp)
  self.u.mem_write(self.context+8,pack([vram_bytes]))
  self.u.reg_write(UC_X86_REG_FPCW,0x037f)
  if 'vx_d3d_hold_on' in self.sym:self.u.mem_write(self.sym['vx_d3d_hold_on'],pack([0])) # .551: the HAL sets it at DriverInit; the cases that start DrawPrimitives2 themselves (emu_start) expect it off
  self.hold=0 # .550: the queue held across DrawPrimitives2 calls off for the cases written before it (each expects its call's packets sent)
 def call(self,n,args=()):
  if n=='_VX_D3D_DP2@4' and 'vx_d3d_hold_on' in self.sym:self.u.mem_write(self.sym['vx_d3d_hold_on'],pack([self.hold]))
  return super().call(n,args)
 def import_call(self,u,a,z,data):
  name,dll=data;argc=None;ret=1
  if name=='GetCurrentProcessId':argc=0;ret=self.pid
  elif name=='VirtualAlloc': # .517: the recorder's buffer
   argc=4;size=(self.arg(1)+0xFFFF)&~0xFFFF;base=0x40000000
   if getattr(self,'rec_mem',None)!=(base,size):u.mem_map(base,size);self.rec_mem=(base,size)
   ret=base
  elif name=='VirtualFree':argc=3;ret=1
  elif name=='CreateFileA' and self.string(self.arg(0))=='C:\\VXD3D.REC':argc=7;ret=0x88;self.rec_file=b''
  elif name=='WriteFile' and self.arg(0)==0x88:
   argc=5;self.rec_file+=bytes(u.mem_read(self.arg(1),self.arg(2)));u.mem_write(self.arg(3),pack([self.arg(2)]))
  elif name=='CloseHandle' and self.arg(0)==0x88:argc=1
  elif name=='InterlockedExchange':
   argc=2;p=self.arg(0);ret=self.word(p);u.mem_write(p,pack([self.arg(1)]))
  elif name=='DeviceIoControl' and self.arg(1) in (0x85500020,0x85500021,0x85500024):
   argc=8
   if self.arg(1)==0x85500024: # v10.276: n packets in one call; each taken until one's result is not 0
    n=self.arg(3)//32380;done=0;rc=0;self.submitn_calls=getattr(self,'submitn_calls',0)+1
    for k in range(n):
     self.native.append(bytes(u.mem_read(self.arg(2)+32380*k,32380)));rc=self.native_results.pop(0) if self.native_results else 0
     if rc:self.native.pop();break
     done+=1
    blob=pack([rc,done])
   elif self.arg(1)==0x85500020:
    h=self.gpu;self.info_calls=getattr(self,'info_calls',0)+1 # .522: the readiness questions asked
    blob=pack([64,0x10000,1,self.generation,self.fb_bytes,h.pitch*48,64,48,h.pitch,h.bpp,0,0,0,0,0,0])
   else:
    self.native.append(bytes(u.mem_read(self.arg(2),self.arg(3))));blob=pack([self.native_results.pop(0) if self.native_results else 0])
   if self.arg(1)!=0x85500020 and self.arg(5)==len(blob)+16:blob+=pack([0,0,0,0]) # v10.414: the submits' entry/exit stamps (0: not known)
   assert len(blob)==self.arg(5);u.mem_write(self.arg(4),blob);u.mem_write(self.arg(6),pack([len(blob)]))
  if argc is None:return super().import_call(u,a,z,data)
  sp=u.reg_read(UC_X86_REG_ESP);dest=self.word(sp);u.reg_write(UC_X86_REG_EAX,ret);u.reg_write(UC_X86_REG_ESP,sp+4+4*argc);u.reg_write(UC_X86_REG_EIP,dest)

def dx7(h):
 # The DirectX 7 runtime's Miscellaneous2Callbacks query, after which ContextCreate receives local objects.
 gi=ARENA+0xc000;out=ARENA+0xc100
 h.u.mem_write(gi,pack([44,0])+struct.pack('<IHH8B',0x406b2f00,0x3e5a,0x11d1,0xb6,0x40,0,0xaa,0,0xa1,0xf9,0x6a)+pack([24,out,0,0,0]))
 h.call('_VX_D3D_GetDriverInfo@4',(gi,))
def checks():
 cases=[]
 h=D3D();u=h.u;create=ARENA+0xa000;dp=ARENA+0xa100;command=ARENA+0xa200;v=ARENA+0xa300
 # DirectDraw global's primary pixel format is supplied by the driver.
 pf=pack([32,0x40,0,32,0xff0000,0xff00,255,0]);u.mem_write(h.context+28,pf)
 h.surface(h.dst,h.dg,0,h.gpu.pitch,64,48)
 # v10.153: before DirectX 7 local objects reach ContextCreate the runtime has asked for Miscellaneous2Callbacks
 # (CreateSurfaceEx); without that answer ContextCreate receives IDirectDrawSurface interfaces (DDRAWI_DDRAWSURFACE_INT).
 legacy=ARENA+0xe000;u.mem_write(legacy,pack([ARENA+0xe100,h.dst,0,1]));u.mem_write(ARENA+0xe100,pack([0xbff71234,0xbff71240])) # lpVtbl -> QueryInterface, AddRef code addresses
 u.mem_write(create,pack([ARENA+0xb000,legacy,0,h.pid,0,0]))
 assert h.call('_VX_D3D_Create@4',(create,))==1 and h.word(create+20)==0
 context=h.word(create+16);assert context
 u.mem_write(create,pack([context,0]));h.call('_VX_D3D_Destroy@4',(create,));assert h.word(create+4)==0
 cases.append('linked ContextCreate takes an IDirectDrawSurface interface from a pre-DirectX 7 runtime call')
 gi0=ARENA+0xc000;out0=ARENA+0xc100
 u.mem_write(gi0,pack([44,0])+struct.pack('<IHH8B',0x406b2f00,0x3e5a,0x11d1,0xb6,0x40,0,0xaa,0,0xa1,0xf9,0x6a)+pack([24,out0,0,0,0]))
 assert h.call('_VX_D3D_GetDriverInfo@4',(gi0,))==1 and h.word(gi0+36)==0 and u32(bytes(u.mem_read(out0,24)),4)==14
 cases.append('linked Miscellaneous2Callbacks offers CreateSurfaceEx, GetDriverState and DestroyDDLocal')
 u.mem_write(create,pack([ARENA+0xb000,h.dst,0,h.pid,0,0]))
 assert h.call('_VX_D3D_Create@4',(create,))==1 and h.word(create+20)==0
 context=h.word(create+16);assert context
 cases.append('linked ContextCreate returns a nonzero owned context')
 # Empty streams have no vertices/FVF and still preserve x87 state.
 u.mem_write(h.src,bytes(128));u.mem_write(h.src+4,pack([h.sg]));u.mem_write(h.sg,bytes(72));u.mem_write(h.sg+20,pack([command,64]));u.mem_write(h.src+32,pack([0x800000]))
 fields=[context,1,0,h.src,0,0,0,0,0,0,0,0,0,0];u.mem_write(dp,pack(fields))
 cw=u.reg_read(UC_X86_REG_FPCW);assert h.call('_VX_D3D_DP2@4',(dp,))==1 and h.word(dp+48)==0
 assert u.reg_read(UC_X86_REG_FPCW)==cw
 cases.append('linked state-only DP2 accepts no VB/FVF and preserves x87 control word')
 # States + native triangle, with a nonzero command offset. dwVertexLength
 # is THREE VERTICES, not sixty bytes. Capture the actual private packet.
 stream=struct.pack('<BBHII',8,0,1,22,1)+struct.pack('<BBHH',18,0,1,0)
 u.mem_write(command,b'PAD!'+stream);u.mem_write(v,b''.join(struct.pack('<4fI',x,y,.5,1,c) for x,y,c in [(8,8,0xffff0000),(56,8,0xff00ff00),(8,40,0xff0000ff)]))
 fields=[context,1,0x44,h.src,4,len(stream),v,0,3,0,0,0,20,0];u.mem_write(dp,pack(fields))
 assert h.call('_VX_D3D_DP2@4',(dp,))==1 and h.word(dp+48)==0 and len(h.native)==1
 assert u32(h.native[0],0)==32380 and u32(h.native[0],4)==0x10000 and u32(h.native[0],108)==3
 cases.append('linked DP2 honors nonzero command offset, vertex count units and native packet ABI')
 # .550: the queue held across DrawPrimitives2 calls: two calls send nothing, then a Lock sends both packets in one VxD call
 h.hold=1;n0=len(h.native);s0=getattr(h,'submitn_calls',0);hs=h.sym['vx_d3d_hold_stat'];hw=lambda k:h.word(hs+4*k);st0=[hw(k) for k in range(6)]
 for _ in range(2):
  u.mem_write(dp,pack(fields));assert h.call('_VX_D3D_DP2@4',(dp,))==1 and h.word(dp+48)==0 and len(h.native)==n0
 assert hw(0)-st0[0]==2 and hw(1)-st0[1]==3 and hw(2)-st0[2]==1,[hw(k)-st0[k] for k in range(6)]
 lk=ARENA+0xf000;u.mem_write(lk,pack([0,h.dst])+bytes(40));h.call('_VXLock@4',(lk,))
 assert len(h.native)==n0+2 and getattr(h,'submitn_calls',0)==s0+1 and hw(4)-st0[4]==1 and hw(3)==st0[3],(len(h.native)-n0,[hw(k)-st0[k] for k in range(6)])
 assert h.native[n0][:32380-4]!=b'' and u32(h.native[n0+1],108)==3
 h.call('_VXLock@4',(lk,));assert len(h.native)==n0+2 and hw(4)-st0[4]==1 # nothing held: nothing sent
 # a held packet refused at the next call's send is dropped (its call answered DD_OK long ago); the next call's own packet still goes
 u.mem_write(dp,pack(fields));assert h.call('_VX_D3D_DP2@4',(dp,))==1 and len(h.native)==n0+2
 u.mem_write(dp,pack(fields));assert h.call('_VX_D3D_DP2@4',(dp,))==1 and h.word(dp+48)==0 and len(h.native)==n0+2
 h.native_results=[2];h.call('_VXLock@4',(lk,)) # the first held packet refused (UNSUPPORTED): dropped; the second still sent
 assert hw(3)-st0[3]==1 and len(h.native)==n0+3 and h.word(h.sym['vx_d3d_hold_stat']+4*0)-st0[0]==4,(len(h.native)-n0,[hw(k)-st0[k] for k in range(6)])
 h.hold=0
 cases.append('.550: draw packets held across DrawPrimitives2 calls go in one VxD call at the next Lock; a refused held packet is dropped, not retried')
 # DX3/5 entry points share the parser and native backend. These exercise
 # the compiled stdcall ABI rather than calling a host-side reimplementation.
 state=ARENA+0xd000;get=ARENA+0xd100;one=ARENA+0xd200;indexed=ARENA+0xd300
 u.mem_write(command+4,pack([9,1,22,1]))
 u.mem_write(state,pack([context,4,2,h.src,0]));h.call('_VX_D3D_RenderState@4',(state,))
 assert h.word(state+16)==0
 u.mem_write(get,pack([context,4,9,0,0]));h.call('_VX_D3D_GetState@4',(get,))
 assert h.word(get+12)==1 and h.word(get+16)==0
 cases.append('linked legacy RenderState and GetState exchange the applied flat-shade state')
 u.mem_write(state,pack([context,0,0,0,0]));h.call('_VX_D3D_RenderState@4',(state,));assert h.word(state+16)==0
 cases.append('empty legacy state callback requires no execute-buffer allocation')
 u.mem_write(v,b''.join(struct.pack('<4fII2f',x,y,.5,1,c,0,0,0) for x,y,c in [(8,8,0xffff0000),(56,8,0xff00ff00),(8,40,0xff0000ff)]))
 previous=len(h.native)
 tl=ARENA+0xe000;tlg=ARENA+0xe100;prim=ARENA+0xe200
 u.mem_write(tl,bytes(128));u.mem_write(tl+4,pack([tlg]));u.mem_write(tl+32,pack([0x800000]))
 u.mem_write(tlg,bytes(72));u.mem_write(tlg+20,pack([v,96]));u.mem_write(command+4,struct.pack('<4H',0,1,2,0))
 u.mem_write(prim,pack([context,4,0,h.src,0,tl])+struct.pack('<BBH',3,8,1)+pack([0]))
 h.call('_VX_D3D_RenderPrimitive@4',(prim,));assert h.word(prim+28)==0 and len(h.native)==previous+1
 cases.append('linked execute-buffer TRIANGLE instruction translates to the same GPU path')
 clear=ARENA+0xe300;u.mem_write(clear,pack([context,1,0xff123456,0,0,0,0]));h.call('_VX_D3D_Clear@4',(clear,))
 assert h.word(clear+24)==0 and u32(h.native[-1],12)==2 and u32(h.native[-1],100)==0xff123456
 cases.append('linked legacy Clear submits an explicit synchronous color clear')
 diag=ARENA+0xe400;u.mem_write(diag,pack([112])+bytes(108));h.call('_VX_D3D_QueryStats@4',(diag,))
 assert h.word(diag)==112 and h.word(diag+4)==0x2000B and h.word(diag+12)>=1 and h.word(diag+44)==len(h.native)
 cases.append('fixed-size read-only diagnostics report real callback and native-packet counts')
 # Unsupported token: exact offset is relative to command allocation.
 u.mem_write(command+4,struct.pack('<BBH',255,0,0));fields[5]=4;u.mem_write(dp,pack(fields))
 h.call('_VX_D3D_DP2@4',(dp,));assert h.word(dp+48)==0x88760bb8 and h.word(dp+52)==4
 cases.append('linked unknown token returns COMMAND_UNPARSED and allocation-relative error offset')
 # Same handle in another process must not access the first process state.
 h.pid=0x202;u.mem_write(dp,pack(fields));h.call('_VX_D3D_DP2@4',(dp,));assert h.word(dp+48)==0x200
 cases.append('linked context lookup rejects a foreign process')
 # .522: ACCEL_D3D_INFOCACHE: the readiness answer kept between calls; asked again after a refused submit and at a HAL rebuild
 sv_=lambda n,v:h.u.mem_write(h.sym['vx_d3d_'+n],pack([v]));gv_=lambda n:h.word(h.sym['vx_d3d_'+n]) # .530: the switches removed: the HAL's words directly
 assert gv_('infocache')==1,'.522: ACCEL_D3D_INFOCACHE on'
 h.pid=0x101;n0=h.info_calls;u.mem_write(dp,pack(fields));h.call('_VX_D3D_DP2@4',(dp,));u.mem_write(dp,pack(fields));h.call('_VX_D3D_DP2@4',(dp,))
 assert h.info_calls-n0<=1,('.522: two calls, at most one question',h.info_calls-n0)
 sv_('infocache',0);n0=h.info_calls;u.mem_write(dp,pack(fields));h.call('_VX_D3D_DP2@4',(dp,));u.mem_write(dp,pack(fields));h.call('_VX_D3D_DP2@4',(dp,))
 assert h.info_calls-n0==2,('.522: off, every call asks',h.info_calls-n0)
 cases.append('.522: ACCEL_D3D_INFOCACHE: with it on two DrawPrimitives2 calls ask the VxD for its readiness once, with it off each call asks')
 h.pid=0x101;h.generation=2;u.mem_write(dp,pack(fields));h.call('_VX_D3D_DP2@4',(dp,));assert h.word(dp+48)==0x887601c2
 sv_('infocache',1)
 cases.append('linked context is lost on a mode-generation change (.522: with the answer asked: the cache off)')
 u.mem_write(create,pack([context,0]));h.call('_VX_D3D_Destroy@4',(create,));assert h.word(create+4)==0
 cases.append('linked ContextDestroy releases the owned context')
 # Runtime's unknown-parser notification passes the function address
 # directly. If the driver dereferences that address, this test would read
 # an instruction word instead of retaining the callback pointer.
 gi=ARENA+0xc000;out=ARENA+0xc100;parser=0x1000f000
 guid=struct.pack('<IHH8B',0x2e04ffa0,0x98e4,0x11d1,0x8c,0xe1,0,0xa0,0xc9,6,0x29,0xa8)
 data=pack([44,0])+guid+pack([0,parser,0,0,0]);u.mem_write(gi,data)
 assert h.call('_VX_D3D_GetDriverInfo@4',(gi,))==1 and h.word(gi+36)==0
 cases.append('linked unknown-parser notification accepts the callback address without dereferencing its code')
 # Exact callback table bounds, reserved members and advertisement flags.
 guid=struct.pack('<IHH8B',0xddf41230,0xec0a,0x11d0,0xa9,0xb6,0,0xaa,0,0xc0,0x99,0x3e)
 u.mem_write(out,b'\xa5'*64);data=pack([44,0])+guid+pack([24,out,0,0,0]);u.mem_write(gi,data)
 h.call('_VX_D3D_GetDriverInfo@4',(gi,));table=bytes(u.mem_read(out,64))
 assert struct.unpack('<4I',table[:16])==(24,12,0,0) and table[24:]==b'\xa5'*40
 assert h.word(gi+32)==24
 cases.append('linked CALLBACKS3 has NULL obsolete/reserved entries and bounds its output copy')
 u.mem_write(out,b'\xa5'*64);u.mem_write(gi,pack([44,0])+guid+pack([8,out,0,0,0]));h.call('_VX_D3D_GetDriverInfo@4',(gi,))
 assert bytes(u.mem_read(out+8,56))==b'\xa5'*56 and h.word(gi+32)==8
 cases.append('linked capability query respects a truncated expected-size buffer')
 # A context created directly from a surface must be invalidated on destroy
 # even when CreateSurfaceEx never assigned a runtime surface-handle record.
 h=D3D();u=h.u;dx7(h);u.mem_write(h.context+28,pf);h.surface(h.dst,h.dg,0,h.gpu.pitch,64,48)
 u.mem_write(create,pack([ARENA+0xb000,h.dst,0,h.pid,0,0]));h.call('_VX_D3D_Create@4',(create,));context=h.word(create+16)
 u.mem_write(state,pack([0,h.dst,0,0]));assert h.call('_VXDestroySurface@4',(state,))==0
 u.mem_write(get,pack([context,4,9,0,0]));h.call('_VX_D3D_GetState@4',(get,))
 assert h.word(get+16)==0x887601c2
 cases.append('destroying an unmapped context surface invalidates the context before address reuse')
 return cases+audit_checks()+texture_checks()+memory_checks()+lock_checks()+alignment_checks()+audit319_checks()+phase_checks()+indexed_list_checks()+recorder_checks()

def audit_checks():
 cases=[];h=D3D();u=h.u;dx7(h)
 pf=pack([32,0x40,0,32,0xff0000,0xff00,255,0]);u.mem_write(h.context+28,pf)
 h.surface(h.dst,h.dg,0,h.gpu.pitch,64,48)
 create=ARENA+0xa000;dp=ARENA+0xa100;command=ARENA+0xa200;get=ARENA+0xa300
 local=ARENA+0xb000;local2=ARENA+0xb100
 u.mem_write(create,pack([local,h.dst,0,h.pid,0,0]));h.call('_VX_D3D_Create@4',(create,));context=h.word(create+16)
 u.mem_write(h.src,bytes(128));u.mem_write(h.src+4,pack([h.sg]));u.mem_write(h.src+32,pack([0x800000]))
 u.mem_write(h.sg,bytes(72));u.mem_write(h.sg+20,pack([command,256]))
 def dp2(stream,vertex=0,offset=0,count=0,stride=0,flags=0):
  u.mem_write(command,stream)
  u.mem_write(dp,pack([context,flags,0x44,h.src,0,len(stream),vertex,offset,count,0,0,0,stride,0]))
  assert h.call('_VX_D3D_DP2@4',(dp,))==1
  return h.word(dp+48),h.word(dp+52)
 def alive(ctx=context):
  u.mem_write(get,pack([ctx,4,22,0,0]));h.call('_VX_D3D_GetState@4',(get,));return h.word(get+16)
 state=struct.pack('<BBHII',8,0,1,22,1);tri=struct.pack('<BBHH',18,0,1,0)
 assert dp2(state,0xdead0000,0xffffffff,0xffffffff,0xffffffff)==(0,0)
 assert alive()==0
 cases.append('state-only DP2 never reads a poisoned unused vertex union or validates its count/stride')
 assert dp2(state+tri,0,0,0,20,1)==(0x80070057,12)
 # v10.428: a render state the runtime recorded in its array (lpdwRStates) but never sent is taken from the array at the end of a call
 rsa=ARENA+0xe000
 def dp2r(stream,arr,flags=0):
  u.mem_write(command,stream);u.mem_write(rsa,pack(arr))
  u.mem_write(dp,pack([context,flags,0x44,h.src,0,len(stream),0,0,0,0,0,rsa,0,0]))
  assert h.call('_VX_D3D_DP2@4',(dp,))==1
  return h.word(dp+48)
 def rsv(i):
  u.mem_write(get,pack([context,4,i,0,0]));h.call('_VX_D3D_GetState@4',(get,));assert h.word(get+16)==0;return h.word(get+12)
 arr=[0]*256;arr[7]=1;arr[14]=1;arr[23]=4
 zw=lambda v:struct.pack('<BBHII',8,0,1,14,v)
 assert dp2r(zw(1),arr)==0 and rsv(14)==1 # the array kept in step with what was sent
 arr[14]=0;assert dp2r(zw(0),arr)==0 and rsv(14)==0
 arr[14]=1;assert dp2r(struct.pack('<BBHII',8,0,1,22,1),arr)==0 and rsv(14)==1,rsv(14) # ZWRITEENABLE 1 recorded, its command never sent: taken from the array
 cases.append('v10.428: ZWRITEENABLE recorded in the runtime\'s state array but never sent reaches the context at the end of the next call')
 arr2=list(arr);arr2[14]=0;assert dp2r(struct.pack('<BBHII',8,0,1,22,1),arr2)==0 and rsv(14)==0 # changed in the array again, unsent
 assert dp2r(struct.pack('<BBHII',8,0,1,22,1),arr2)==0 and rsv(14)==0 # unchanged array: nothing more
 arr3=list(arr2);arr3[14]=1;assert dp2r(zw(1)+zw(0),arr3)==0 and rsv(14)==1,rsv(14) # sent 1 then 0 but the array says 1 (changed): the array wins at the end
 st=list(arr3);assert dp2r(zw(0),st)==0 and rsv(14)==0 and dp2r(zw(0),st)==0 and rsv(14)==0 # an array the runtime stopped keeping (unchanged, disagreeing): never taken
 assert dp2r(zw(1),st)==0 and rsv(14)==1
 cases.append('v10.428: a value is taken only when the array changed since the last call; an array the runtime does not keep changes nothing')
 ex=[0]*256;ex[14]=0;assert dp2r(zw(1),ex,flags=2)==0 and rsv(14)==1;cases.append('v10.428: execute buffers (the HAL writes the array) are left alone')
 cases.append('lazy vertex failure identifies the first consuming command after applying preceding states')
 assert dp2(tri,0xfffffff0,32,3,20,1)==(0x80070057,0)
 cases.append('user vertex pointer arithmetic rejects 32-bit address wrap')
 vb=ARENA+0xc000;vg=vb+0x80
 u.mem_write(vb,bytes(128));u.mem_write(vb+4,pack([vg]));u.mem_write(vb+32,pack([0x800000]))
 u.mem_write(vg,bytes(72));u.mem_write(vg+20,pack([0xfffffff0,128]))
 assert dp2(tri,vb,32,3,20)==(0x80070057,0)
 cases.append('surface vertex pointer arithmetic rejects 32-bit address wrap')
 gi=ARENA+0xd000;out=gi+0x100
 def query(data1,data2,data3,data4,size=116):
  guid=struct.pack('<IHH8B',data1,data2,data3,*data4)
  u.mem_write(out,b'\xa5'*256);u.mem_write(gi,pack([44,0])+guid+pack([size,out,0,0,0]))
  h.call('_VX_D3D_GetDriverInfo@4',(gi,));assert h.word(gi+36)==0
  return bytes(u.mem_read(out,size))
 ext=query(0x7de41f80,0x9d93,0x11d0,[0x89,0xab,0,0xa0,0xc9,5,0x41,0x29])
 assert u32(ext,44)==16 and u32(ext,72)==2 and u32(ext,76)==0xFFFFFF and u32(ext,88)==0 # v10.206: two coordinate sets, + PM, PREMODULATE, the MODULATEINV ops, DOT3
 assert struct.unpack_from('<4I',ext,4)==(1,2048,1,2048) and u32(ext,36)==2048 and u32(ext,40)==2048 # v10.199: 2048
 assert struct.unpack_from('<HH',ext,80)==(3,3) and ext[96:]==bytes(20) # v10.206: three stages and textures
 cases.append('v10.169 extended caps: one texture stage and sampler, one coordinate set, 1..1024 textures, op caps DISABLE, SELECTARG1/2, MODULATE/2X/4X, ADD, ADDSIGNED, BLENDDIFFUSEALPHA, BLENDTEXTUREALPHA (v10.172), anisotropy one, no W range, zero reserved fields')
 misc=query(0x406b2f00,0x3e5a,0x11d1,[0xb6,0x40,0,0xaa,0,0xa1,0xf9,0x6a],24)
 h.sym['audit_create_ex']=u32(misc,12);h.sym['audit_destroy_local']=u32(misc,20)
 cb2=query(0x0ba584e1,0x70b6,0x11d0,[0x88,0x9d,0,0xaa,0,0xbb,0xb7,0x6a],28)
 h.sym['audit_setrt']=u32(cb2,8)
 # v10.164: the acceptance rules of the board's DDRAW.DLL 4.09.00.0904 (BAAB915F, BAABB6F2-BAABB772, BAABB95A, BAABB9EF): a function pointer needs its flag bit
 # and every flagged slot a function; DrawOnePrimitive/DrawOneIndexedPrimitive/DrawPrimitives all or none; CreateSurfaceEx needs DRAWPRIMITIVES2EX in
 # the global data's dwDevCaps; DRAWPRIMITIVES2 needs ValidateTextureStageState and DrawPrimitives2 in CALLBACKS3.
 cb3=query(0xddf41230,0xec0a,0x11d0,[0xa9,0xb6,0,0xaa,0,0xc0,0x99,0x3e],24)
 def slots_ok(t):
  n=u32(t,0)//4-2;fl=u32(t,4)
  for k in range(n):
   f=u32(t,8+4*k);bit=1<<k
   if f and not fl&bit:return False
   if fl&bit and not f:return False
  return u32(t,0)%4==0
 assert slots_ok(misc) and slots_ok(cb2) and slots_ok(cb3)
 assert (u32(cb2,16)!=0)==(u32(cb2,20)!=0)==(u32(cb2,24)!=0)
 gd=h.word(h.sym["VXDD3DABI"]+15*4);devcaps=h.word(gd+16)
 assert u32(misc,12)==0 or devcaps&0x8000,(hex(gd),hex(devcaps),hex(h.word(h.sym["VXDD3DABI"]+15*4)))
 assert not devcaps&0x2000 or (u32(cb3,16) and u32(cb3,20))
 cases.append('GetDriverInfo replies pass the DirectDraw 9.0c acceptance rules (flag bits, CALLBACKS2 draw trio, CreateSurfaceEx needs DRAWPRIMITIVES2EX, CALLBACKS3 for DRAWPRIMITIVES2)')
 abi=h.sym['VXDD3DABI'];handle_offset=h.word(abi+11*4);ex_offset=h.word(abi+12*4)
 assert h.word(abi+13*4)==12 and h.word(abi+14*4)==8
 validate=ARENA+0xe000
 def validation(ctx=context):
  u.mem_write(validate,pack([ctx,0,0,99,0]));h.call('_VX_D3D_Validate@4',(validate,))
  return h.word(validate+16),h.word(validate+12)
 assert validation()==(0,1) and validation(0xdead)==(0x200,0)
 u.mem_write(create,pack([0xdead,0]));h.call('_VX_D3D_Destroy@4',(create,));assert h.word(create+4)==0x200
 u.mem_write(create,pack([0xdead,h.dst,0,0]));h.call('audit_setrt',(create,));assert h.word(create+12)==0x200
 assert alive(0xdead)==0x200
 cases.append('invalid contexts return CONTEXT_BAD in ddrval across validate/destroy/set-target/legacy callbacks')
 # Bind a texture while testing invalid stage state. NULL + COLORARG1
 # TEXTURE terminates the cascade and legitimately ignores the unused op.
 assert dp2(struct.pack('<BBH',25,0,2)+struct.pack('<HHIHHI',0,0,1,0,1,22))[0]==0 # v10.206: BUMPENVMAP (DOTPRODUCT3 now draws)
 assert validation()==(0x88760000|2073,0)
 assert dp2(struct.pack('<BBH',25,0,2)+struct.pack('<HHIHHI',0,0,0,0,1,4))[0]==0
 assert validation()==(0,1)
 cases.append('ValidateTextureStageState returns the specific operation error and one pass only on success')
 ex=ARENA+0xe100
 def resource(i,handle,offset=None,z=False):
  l=ARENA+0x20000+i*0x300;g=l+0x80;m=l+0x100
  h.surface(l,g,(i+1)*0x10000 if offset is None else offset,256,64,48)
  u.mem_write(m,bytes(256));u.mem_write(l,pack([m]));u.mem_write(m+handle_offset,pack([handle]))
  if z:
   u.mem_write(l+28,pack([0x2000]));u.mem_write(l+32,pack([0x24000]))
   u.mem_write(g+40,pack([32,0x400,0,32,0,0xffffffff,0,0]))
  return l,g,m
 def register(l,loc=local):
  u.mem_write(ex,pack([0,loc,l,0]));h.call('audit_create_ex',(ex,));return h.word(ex+12)
 def target(handle,z=0):return dp2(struct.pack('<BBHII',41,0,1,handle,z))[0]
 surfaces=[resource(i,1000+i,0x10000+(i%8)*0x10000) for i in range(130)]
 for l,g,m in surfaces:assert register(l)==0
 assert target(1129)==0
 cases.append('per-DDLocal table grows beyond the old 128-surface global limit and resolves a sparse high handle')
 second=resource(130,1129,0xa0000);assert register(second[0],local2)==0
 # A second namespace reuses the same numeric handle without replacing the first.
 assert target(1129)==0
 clear=struct.pack('<BBHIIII4i',42,0,1,1,0xff123456,0x3f800000,0,0,0,64,48)
 assert dp2(clear)[0]==0 and u32(h.native[-1],20)==0x20000
 cases.append('equal numeric handles in different DDLocals retain distinct render-target descriptors')
 # Old direct target must no longer be considered active after DP2 rebinding.
 u.mem_write(ex,pack([0,h.dst,0,0]));h.call('_VXDestroySurface@4',(ex,));assert alive()==0
 cases.append('destroying the former context target after DP2 rebinding does not lose the active context')
 # Destruction in another DDLocal with an aliased offset must not lose this context.
 u.mem_write(ex,pack([0,second[0],0,0]));h.call('_VXDestroySurface@4',(ex,));assert alive()==0
 cases.append('surface destruction respects DDLocal identity rather than matching framebuffer offsets alone')
 current=surfaces[-1];u.mem_write(current[1]+20,pack([0]))
 assert register(current[0])==0 and alive()==0
 cases.append('video fpVidMem zero is not treated as a system-memory deletion notification')
 u.mem_write(current[1]+20,pack([FB+0xb0000]));assert register(current[0])==0 and alive()==0x887601c2
 cases.append('successful re-registration of a changed descriptor invalidates contexts using the old resource')
 # Fresh context for system-memory disassociation and attachment traversal.
 u.mem_write(create,pack([local,h.src,0,h.pid,0,0])) # h.src is a command buffer, use a real video surface.
 u.mem_write(create+4,pack([surfaces[0][0]]));h.call('_VX_D3D_Create@4',(create,));context=h.word(create+16);assert context
 assert target(1000)==0
 u.mem_write(surfaces[0][0]+32,pack([0x800]));u.mem_write(surfaces[0][1]+20,pack([0]))
 assert register(surfaces[0][0])==0 and alive(context)==0x887601c2
 assert target(1000)==0x887601c2
 cases.append('system-memory fpVidMem zero disassociates the old handle and invalidates its context')
 # Releasing one DDLocal must preserve another's metadata and surface bytes.
 assert register(second[0],local2)==0
 destroy=ARENA+0xe200;before=bytes(u.mem_read(second[0],128))
 u.mem_write(destroy,pack([0,local,0]));h.call('audit_destroy_local',(destroy,));assert h.word(destroy+8)==0
 assert bytes(u.mem_read(second[0],128))==before
 u.mem_write(create,pack([local2,second[0],0,h.pid,0,0]));h.call('_VX_D3D_Create@4',(create,));context=h.word(create+16)
 assert context and target(1129)==0
 cases.append('DestroyDDLocal releases only that local table/contexts and preserves DirectDraw surface storage')
 # Test color/depth attachment graph including a cycle, then role checks.
 color=resource(131,2001,0xc0000);depth=resource(132,2002,0xd0000,True)
 attach=ARENA+0xf000;u.mem_write(attach,pack([0,0,depth[0],0]));u.mem_write(color[0]+12,pack([attach]))
 u.mem_write(attach+16,pack([0,0,color[0],0]));u.mem_write(depth[0]+12,pack([attach+16]))
 assert register(color[0],local2)==0 and target(2001,2002)==0
 assert target(2002,2001)==0x80070057
 cases.append('CreateSurfaceEx visits supported cyclic attachments and DP2 rejects swapped color/depth roles')
 # Unsupported video textures fail while unknown system-memory formats are ignored.
 unsupported=resource(133,3001,0xe0000);u.mem_write(unsupported[0]+32,pack([0x5000]))
 assert register(unsupported[0],local2)==0x80004001
 u.mem_write(unsupported[0]+32,pack([0x1800]));assert register(unsupported[0],local2)==0
 cases.append('unsupported video resources fail registration while system-memory formats remain runtime-owned')
 # v10.169: a 64x64 X8R8G8B8 video texture (pitch 256) registers and a DP2 triangle with TEXTURE as COLORARG1 carries it to the native packet.
 tex=resource(134,4001,0xf0000);u.mem_write(tex[0]+32,pack([0x5000]));u.mem_write(tex[1]+28,struct.pack('<HH',64,64))
 assert register(tex[0],local2)==0
 tss=b''.join(struct.pack('<HHI',0,k,v) for k,v in [(0,4001),(1,2),(2,2),(4,2),(5,2),(13,3),(16,2)])
 stream=struct.pack('<BBH',25,0,7)+tss+struct.pack('<BBHH',18,0,1,0)
 vt=ARENA+0xf800;u.mem_write(vt,b''.join(struct.pack('<4fI2f',x,y,.5,1,0xffffffff,tu,tv) for x,y,tu,tv in [(8,8,0,0),(56,8,1,0),(8,40,0,1)]))
 u.mem_write(command,stream);n=len(h.native)
 u.mem_write(dp,pack([context,1,0x144,h.src,0,len(stream),vt,0,3,0,0,0,28,0]))
 assert h.call('_VX_D3D_DP2@4',(dp,))==1 and h.word(dp+48)==0 and len(h.native)==n+1
 # MAGFILTER is linear; MINFILTER retains its point default (packet mode 2).
 pk=h.native[-1];assert len(pk)==32380 and struct.unpack_from('<6I',pk,3952)==(1,0xf0000,256,64,64,5) and struct.unpack_from('<4I',pk,3976)==(2,1,2,1) and struct.unpack_from('<2I',pk,5528)==(1,0x01C00530) # v10.235: 192 vertices
 uv=lambda k:struct.unpack_from('<2f',pk,10456+40*k+24) if u32(pk,10452) else struct.unpack_from('<2f',pk,3992+8*k) # v10.441: the prepacked block (ACCEL_D3D_PREPACK, its default 1)
 assert u32(pk,10452)==1 and uv(1)==(1.0,0.0) and uv(2)==(0.0,1.0) and struct.unpack_from('<6I',pk,10456)[:5]==struct.unpack_from('<5I',struct.pack('<4fI',8,8,.5,1,0xffffffff))
 cases.append('a registered 64x64 video texture bound in stage 0 reaches the native packet with its address, format, filter, address modes and coordinates (v10.441: in the prepacked block: XP\'s 10 words a vertex)')
 # v10.185: an ALLOCONLOAD texture has no memory when CreateSurfaceEx sees it (IDirect3DTexture::Load allocates it later); it is described at
 # its first use, and D3DRENDERSTATE_TEXTUREHANDLE (render state 1) binds it as stage 0's texture.
 late=resource(135,4002,0xf8000);u.mem_write(late[0]+32,pack([0x5000]));u.mem_write(late[1]+28,struct.pack('<HH',64,64));u.mem_write(late[1]+20,pack([0]))
 assert register(late[0],local2)==0
 u.mem_write(late[1]+20,pack([FB+0xf8000]))
 stream=struct.pack('<BBHII',8,0,1,1,4002)+struct.pack('<BBHH',18,0,1,0)
 u.mem_write(command,stream);n=len(h.native)
 u.mem_write(dp,pack([context,1,0x144,h.src,0,len(stream),vt,0,3,0,0,0,28,0]))
 assert h.call('_VX_D3D_DP2@4',(dp,))==1 and h.word(dp+48)==0 and len(h.native)==n+1
 assert struct.unpack_from('<6I',h.native[-1],3952)==(1,0xf8000,256,64,64,5)
 cases.append('an ALLOCONLOAD texture registered before it has memory is described at first use and bound by TEXTUREHANDLE')
 # v10.463 ACCEL_D3D_TEXHINT: a remembered slot is used only while it still holds the handle: handle 4001 moved to another surface,
 # disassociated (its slot cleared) and registered again each draw from the surface registered last
 def tex4001(off):
  stream=struct.pack('<BBHII',8,0,1,1,4001)+struct.pack('<BBHH',18,0,1,0)
  u.mem_write(command,stream);n=len(h.native)
  u.mem_write(dp,pack([context,1,0x144,h.src,0,len(stream),vt,0,3,0,0,0,28,0]))
  assert h.call('_VX_D3D_DP2@4',(dp,))==1 and h.word(dp+48)==0
  return struct.unpack_from('<2I',h.native[-1],3952) if len(h.native)==n+1 else None
 assert h.word(h.sym['vx_d3d_texhint'])==1 and tex4001(0xf0000)==(1,0xf0000)
 moved=resource(137,4001,0xf4000);u.mem_write(moved[0]+32,pack([0x5000]));u.mem_write(moved[1]+28,struct.pack('<HH',64,64))
 assert register(moved[0],local2)==0 and tex4001(0xf4000)==(1,0xf4000)
 u.mem_write(moved[0]+32,pack([0x800]));u.mem_write(moved[1]+20,pack([0]));assert register(moved[0],local2)==0
 assert tex4001(0)!=(1,0xf4000)
 assert register(tex[0],local2)==0 and tex4001(0xf0000)==(1,0xf0000)
 cases.append('v10.463: ACCEL_D3D_TEXHINT follows a handle moved to another surface, cleared and registered again')
 # v10.466 (review): a surface destroyed while the callback lock is held is not refused (DirectDraw frees it anyway): queued, then
 # forgotten by the next callback before its own work (its handle no longer draws), and registered again it draws as before
 ow=h.sym['callback_owner'];tp,tv=h.sym['tok_pid'],h.sym['tok_val'];me=[h.word(tv+4*k) for k in range(64) if h.word(tp+4*k)==h.pid][0]
 dsd=ARENA+0x9800;u.mem_write(dsd,pack([0,tex[0],0xFFFFFFFF,0]))
 u.mem_write(ow,pack([me]));h.call('_VXDestroySurface@4',(dsd,));assert h.word(dsd+8)==0 and h.word(ow)==me,hex(h.word(dsd+8))
 assert h.word(h.sym['fq_queued'])==1 and any(h.word(h.sym['fq_ptr']+4*k)==tex[0] for k in range(32))
 u.mem_write(ow,pack([0]));assert tex4001(0)!=(1,0xf0000) and all(h.word(h.sym['fq_ptr']+4*k)==0 for k in range(32)) and h.word(ow)==0
 assert register(tex[0],local2)==0 and tex4001(0xf0000)==(1,0xf0000)
 cases.append('v10.466: DestroySurface with the callback lock held queues the surface; the next callback forgets it before drawing (its handle no longer draws)')
 # v10.470 (board, VXFETCH v2.1): a process that used the HAL without this DLL being attached in it (DirectDraw maps it in the shared arena),
 # then loads it (DllMain attach): its contexts and surface tables are its own, live: kept (they were reclaimed and every draw refused)
 tp,tv=h.sym['tok_pid'],h.sym['tok_val']
 for k in range(64):
  if h.word(tp+4*k)==h.pid:u.mem_write(tp+4*k,pack([0]));u.mem_write(tv+4*k,pack([0]))
 h.call('_DllMain@12',(h.base,1,0));assert tex4001(0xf0000)==(1,0xf0000),'kept'
 cases.append('v10.470: a DllMain attach in a process that already drew through the HAL keeps its contexts and surface tables')
 # Exhaust the bounded shared pool using independent DDLocals. Failed growth
 # must preserve earlier mappings; destroying a local returns its block.
 extra_locals=[ARENA+0x10000+i*0x100 for i in range(31)]
 for loc in extra_locals:assert register(second[0],loc)==0
 assert register(second[0],ARENA+0x14000)==0x8007000e
 assert target(1129)==0
 cases.append('shared table-pool exhaustion returns OUTOFMEMORY without corrupting existing handles')
 u.mem_write(destroy,pack([0,extra_locals[0],0]));h.call('audit_destroy_local',(destroy,));assert h.word(destroy+8)==0
 assert register(second[0],ARENA+0x14000)==0 and target(1129)==0
 cases.append('DestroyDDLocal makes its shared table block reusable after exhaustion')
 # v10.456 ACCEL_D3D_RTTEX: a 64x64 X8R8G8B8 render-target texture: drawn into as an X8R8G8B8 target, then sampled
 rtt=resource(136,5001,0xe8000);u.mem_write(rtt[0]+32,pack([0x7000]));u.mem_write(rtt[1]+28,struct.pack('<HH',64,64))
 assert register(rtt[0],local2)==0
 waits=lambda:h.word(h.sym['vx_d3d_rt_waits']);w0=waits()
 assert target(5001)==0 and waits()==w0
 assert dp2(clear)[0]==0 and struct.unpack_from('<5I',h.native[-1],20)==(0xe8000,256,64,64,1),struct.unpack_from('<5I',h.native[-1],20)
 assert target(1129)==0 and waits()==w0+1,'leaving a render-target texture waits for its draws'
 assert target(1129)==0 and waits()==w0+1,'leaving an ordinary target does not'
 stream=struct.pack('<BBH',25,0,7)+b''.join(struct.pack('<HHI',0,k,v) for k,v in [(0,5001),(1,2),(2,2),(4,2),(5,2),(13,3),(16,2)])+struct.pack('<BBHH',18,0,1,0)
 u.mem_write(command,stream);n=len(h.native);u.mem_write(dp,pack([context,1,0x144,h.src,0,len(stream),vt,0,3,0,0,0,28,0]))
 assert h.call('_VX_D3D_DP2@4',(dp,))==1 and h.word(dp+48)==0 and len(h.native)==n+1 and struct.unpack_from('<6I',h.native[-1],3952)==(1,0xe8000,256,64,64,5)
 cases.append('v10.456 ACCEL_D3D_RTTEX: DP2 SETRENDERTARGET draws into a render-target texture as an X8R8G8B8 target; leaving it waits for the engine once; the same surface is then sampled as a texture')
 # v10.479 (review): a change refused after its wait (a depth handle that does not resolve) leaves the texture the target and still marked:
 # the next change waits again (it skipped the wait before); the draw after the refusal still goes into the texture
 assert target(5001)==0;w1=waits()
 assert target(1129,0xBAD)!=0 and waits()==w1+1,'the refused change waited (the texture was the target)'
 assert dp2(clear)[0]==0 and struct.unpack_from('<I',h.native[-1],20)[0]==0xe8000,'the texture is still the target'
 assert target(1129)==0 and waits()==w1+2,'the next change waits for the draws made into the texture after the refusal'
 assert target(1129)==0 and waits()==w1+2
 cases.append('v10.479: DP2 SETRENDERTARGET refused after its wait (bad depth handle): the render-target texture stays the target and marked; the next change waits again')
 u.mem_write(h.sym['vx_d3d_rttex'],pack([0]));assert target(5001)==0x80070057;u.mem_write(h.sym['vx_d3d_rttex'],pack([1]))
 cases.append('v10.456: ACCEL_D3D_RTTEX 0: a texture is refused as a render target, as before')
 # .518 (audit A1): a recorded SETRENDERTARGET and clear, with ACCEL_D3D_RTTEX on and off: the strict replay takes the answers the HAL gave
 # (target with it on, resolve with it off) and makes the same packets
 for rtv in (1,0):
  u.mem_write(h.sym['vx_d3d_rttex'],pack([rtv]))
  assert h.call('_VX_D3D_Record@4',(1,))==1;h.call('VX_rec_flip',());n=len(h.native)
  assert target(1129)==0 and dp2(clear)[0]==0;h.call('VX_rec_flip',())
  st=h.call('_VX_D3D_Record@4',(0xFFFFFFFF,));assert st&255==3,hex(st)
  line=replay_strict(h.rec_file);assert line.startswith('1 frames, 2 DrawPrimitives2 calls: %d.0 packets'%(len(h.native)-n)),(rtv,line,len(h.native)-n)
 u.mem_write(h.sym['vx_d3d_rttex'],pack([1]))
 cases.append('.518: a recorded render-target change and clear replays strictly with ACCEL_D3D_RTTEX on and off (the target answers only when the HAL gave them)')
 # v10.477 ACCEL_D3D_RTNOWAIT: a Blt of the target (a video surface, not a texture) into the render-target texture is submitted to the VxD
 # (its ring or its 2D engine order it after the draws) without the HAL's wait first; with the switch 0 the wait comes first, as v10.456
 order=[];gc,gs=h.gpu.call,h.gpu.submit
 def call2(n,*a,**k):
  if n=='VX2D_access':order.append('wait')
  return gc(n,*a,**k)
 def submit2(q,*a,**k):order.append('submit');return gs(q,*a,**k)
 h.gpu.call,h.gpu.submit=call2,submit2
 def rtblt():
  bd=ARENA+0x9c00;b=bytearray(204);struct.pack_into('<II4iI4iII',b,0,0,rtt[0],0,0,64,48,h.dst,0,0,64,48,0x1020000,0)
  struct.pack_into('<I',b,52,100);struct.pack_into('<I',b,60,0xCC0020);u.mem_write(bd,bytes(b));del order[:]
  rc=h.call('_VXBlit@4',(bd,));return rc,h.word(bd+152),list(order)
 try:
  assert h.word(h.sym['vx_d3d_rtnowait'])==1,'ACCEL_D3D_RTNOWAIT on'
  rc,hr,o=rtblt();assert rc==1 and hr==0 and o[:1]==['submit'],(rc,hex(hr),o)
  h.u.mem_write(h.sym['vx_d3d_rtnowait'],pack([0]));rc,hr,o=rtblt();assert rc==1 and hr==0 and o[:2]==['wait','submit'],(rc,hex(hr),o)
 finally:h.gpu.call,h.gpu.submit=gc,gs;h.u.mem_write(h.sym['vx_d3d_rtnowait'],pack([1]))
 cases.append('v10.477 ACCEL_D3D_RTNOWAIT: a Blt of the target into a render-target texture goes to the VxD without the HAL waiting for the engine first (on by default); with it 0, the wait first, as v10.456')
 h.generation=2;assert validation(context)==(0x887601c2,0)
 cases.append('validation detects a changed mode generation and returns no successful pass count')
 # v10.470: an ID that already had a token (an ended process attached under it and never detached): its contexts are reclaimed on attach
 h.call('_DllMain@12',(h.base,1,0)) # this process's token exists now: a second attach is the stale case
 stream=struct.pack('<BBHII',8,0,1,1,4001)+struct.pack('<BBHH',18,0,1,0);u.mem_write(command,stream)
 u.mem_write(dp,pack([context,1,0x144,h.src,0,len(stream),vt,0,3,0,0,0,28,0]));h.call('_VX_D3D_DP2@4',(dp,));assert h.word(dp+48)!=0,'reclaimed'
 cases.append('v10.470: a DllMain attach under an ID that already had a token reclaims that ID\'s contexts (an ended process\'s)')
 return cases

def indexed_list_checks():
 """v10.472: the compiled indexed list loop (ix_list): a 12 x 12 grid's triangles in grid order and shuffled, two tokens a call, with
 ACCEL_D3D_INDEXED off and on: the same triangles (each a set of its three vertices' ten words; the off path makes strips of the grid),
 the indexed packets well formed (every vertex used, indices below the count), and culling (CCW) the same"""
 import random
 cases=[];G=12;NV=(G+1)*(G+1)
 vb=b''.join(struct.pack('<4fII2f',4+x*3.5-.5,4+y*3-.5,.5,1,0xFF000000|(x*20)<<8|y*20,0xFF000000,x/4,y/4) for y in range(G+1) for x in range(G+1))
 grid=[]
 for y in range(G):
  for x in range(G):a=y*(G+1)+x;grid+=[(a,a+G+1,a+1),(a+1,a+G+1,a+G+2)]
 shuf=list(grid);random.Random(472).shuffle(shuf)
 def tris_of(p):
  out=[];pre=u32(p,10452);w=lambda i:struct.unpack_from('<10I',p,10456+40*i)
  if pre==2:
   bw=u32(p,10064);ni=bw&0xFFFF;ix=struct.unpack_from('<%dH'%ni,p,112);vc=u32(p,108)
   assert u32(p,10060)==1 and (bw>>16)&0xFCFF==2 and ni%3==0 and set(ix)<=set(range(vc)),(ni,vc) # .515: the cull bits (24, 25); a pass-through span may hold unused vertices
   f=lambda i,k:struct.unpack('<f',struct.pack('<I',w(i)[k]))[0]
   def eng(a,b,c): # .515: the engine's cull as the bits ask (VX3DN_IX_CCW culls area < 0, else area > 0)
    if not bw&0x01000000:return True
    ar=(f(b,0)-f(a,0))*(f(c,1)-f(a,1))-(f(b,1)-f(a,1))*(f(c,0)-f(a,0))
    return not ((bw&0x02000000 and ar<0) or (not bw&0x02000000 and ar>0))
   return [tuple(sorted(w(ix[k+j]) for j in range(3))) for k in range(0,ni,3) if eng(*ix[k:k+3])]
  assert pre==1,pre;base=0
  for r in range(u32(p,10060)):
   b=u32(p,10064+4*r);t,c=b>>16,b&0xFFFF
   out+=[tuple(sorted(w(base+k+j) for j in range(3))) for k in (range(0,c,3) if t==0 else range(c-2))];base+=c
  return out
 def draw(order,on,cull):
  h=D3D();u=h.u;dx7(h)
  u.mem_write(h.context+28,pack([32,0x40,0,32,0xff0000,0xff00,255,0]));h.surface(h.dst,h.dg,0,h.gpu.pitch,64,48)
  create=ARENA+0xa000;dp=ARENA+0xa100;local=ARENA+0xb000;command=ARENA+0x40000;vbuf=ARENA+0x60000
  u.mem_write(create,pack([local,h.dst,0,h.pid,0,0]));assert h.call('_VX_D3D_Create@4',(create,))==1;context=h.word(create+16)
  u.mem_write(vbuf,vb);h.u.mem_write(h.sym['vx_d3d_indexed'],pack([on]))
  tok=struct.pack('<BBHH',26,0,len(order),0)+b''.join(struct.pack('<3H',*t) for t in order)
  stream=struct.pack('<BBH',8,0,2)+struct.pack('<IIII',22,cull,29,1)+tok+tok
  u.mem_write(h.src,bytes(128));u.mem_write(h.src+4,pack([h.sg]));u.mem_write(h.src+32,pack([0x800000]))
  u.mem_write(h.sg,bytes(72));u.mem_write(h.sg+20,pack([command,len(stream)+64]));u.mem_write(command,stream)
  u.mem_write(dp,pack([context,1,0x1C4,h.src,0,len(stream),vbuf,0,NV,0,0,0,32,0]))
  sp=0x1f0000;u.mem_write(sp,pack([0xf0000,dp]));u.reg_write(UC_X86_REG_ESP,sp)
  u.emu_start(h.sym['_VX_D3D_DP2@4'],0xf0000,count=50000000)
  assert u.reg_read(UC_X86_REG_EIP)==0xf0000 and h.word(dp+48)==0,hex(h.word(dp+48))
  pk=[p for p in h.native if u32(p,108)];return pk,sum((tris_of(p) for p in pk),[])
 hw_bits=set()
 for name,order in (('grid order',grid),('shuffled',shuf)):
  kept={}
  for cull in (1,2,3):
   pa,ta=draw(order,0,cull);pb,tb=draw(order,1,cull)
   assert sorted(ta)==sorted(tb),(name,cull,len(ta),len(tb));kept[cull]=len(tb)
   assert all(u32(p,10452)==2 for p in pb) and (len(pb)<=len(pa) or cull!=1),(name,[u32(p,10452) for p in pb]) # .515: with the engine culling, the culled triangles travel in the packets
   if cull!=1:hw_bits.update((u32(p,10064)>>24)&3 for p in pb)
  assert kept[1]==2*len(order) and sorted((kept[2],kept[3]))==[0,2*len(order)],(name,kept) # one winding: CW or CCW culls all of it
 assert hw_bits=={0},hw_bits # .529: the engine's cull removed: the HAL culls on the CPU, no bits
 cases.append('v10.472: the compiled indexed list loop draws the same triangles as the list and strip path (grid order and shuffled, cull none, CW and CCW), in well-formed indexed packets (.529: no cull bits, the HAL culls; IXPASS on as the VxD reports it)')
 return cases
RecD3D=D3D # .518: the recorder's Win32 calls are D3D's own (a check switches recording on in any HAL)
def replay_strict(rec):
 """.518: the host replay of a recording with its recorded switches (exits 1 when it diverged); its summary line"""
 import subprocess,tempfile,os
 exe=ROOT/'tests'/'vx_replay'
 if not exe.exists():subprocess.run(['gcc','-std=c99','-O2','-Isrc','tests/vx_replay.c','src/vx_d3d_core.c','src/vx_native3d_hw.c','-o',str(exe)],cwd=ROOT,check=True)
 fd,path=tempfile.mkstemp(suffix='.REC');os.write(fd,rec);os.close(fd)
 try:r=subprocess.run([str(exe),path,'-q'],capture_output=True,text=True)
 finally:os.unlink(path)
 assert r.returncode==0,(r.returncode,r.stdout,r.stderr)
 return [l for l in r.stdout.splitlines() if 'DrawPrimitives2 calls' in l][0]
def recorder_checks():
 """.517: the DrawPrimitives2 recorder in the linked HAL: VX_D3D_Record(2) armed, a flip starts it, two frames of indexed grid draws
 (three calls), the second flip writes C:\\VXD3D.REC; the host replay (tests/vx_replay) of that file through the same core makes the same
 packets (count, triangles) as the HAL sent the VxD"""
 import subprocess,tempfile,os
 cases=[];G=10;NV=(G+1)*(G+1)
 vb=b''.join(struct.pack('<4fII2f',4+x*5.5-.5,4+y*4-.5,.5,1,0xFF000000|(x*20)<<8|y*20,0xFF000000,x/4,y/4) for y in range(G+1) for x in range(G+1))
 grid=[]
 for y in range(G):
  for x in range(G):a=y*(G+1)+x;grid+=[(a,a+G+1,a+1),(a+1,a+G+1,a+G+2)]
 h=RecD3D();u=h.u;dx7(h)
 u.mem_write(h.context+28,pack([32,0x40,0,32,0xff0000,0xff00,255,0]));h.surface(h.dst,h.dg,0,h.gpu.pitch,64,48)
 create=ARENA+0xa000;dp=ARENA+0xa100;local=ARENA+0xb000;command=ARENA+0x40000;vbuf=ARENA+0x60000
 u.mem_write(create,pack([local,h.dst,0,h.pid,0,0]));assert h.call('_VX_D3D_Create@4',(create,))==1;context=h.word(create+16)
 u.mem_write(vbuf,vb);h.u.mem_write(h.sym['vx_d3d_indexed'],pack([1]))
 def dp2(order,cull):
  tok=struct.pack('<BBHH',26,0,len(order),0)+b''.join(struct.pack('<3H',*t) for t in order)
  stream=struct.pack('<BBH',8,0,2)+struct.pack('<IIII',22,cull,29,1)+tok
  u.mem_write(h.src,bytes(128));u.mem_write(h.src+4,pack([h.sg]));u.mem_write(h.src+32,pack([0x800000]))
  u.mem_write(h.sg,bytes(72));u.mem_write(h.sg+20,pack([command,len(stream)+64]));u.mem_write(command,stream)
  u.mem_write(dp,pack([context,1,0x1C4,h.src,0,len(stream),vbuf,0,NV,0,0,0,32,0]))
  sp=0x1f0000;u.mem_write(sp,pack([0xf0000,dp]));u.reg_write(UC_X86_REG_ESP,sp)
  u.emu_start(h.sym['_VX_D3D_DP2@4'],0xf0000,count=50000000)
  assert u.reg_read(UC_X86_REG_EIP)==0xf0000 and h.word(dp+48)==0,hex(h.word(dp+48))
 assert h.call('_VX_D3D_Record@4',(2,))==1
 dp2(grid,1) # before the flip: not recorded
 h.call('VX_rec_flip',());n0=len(h.native)
 dp2(grid,3);dp2(grid[:60],1);h.call('VX_rec_flip',())
 st=h.call('_VX_D3D_Record@4',(0xFFFFFFFF,));assert st&255==2 and (st>>8)&255==1,hex(st)
 dp2(grid[40:],2);h.call('VX_rec_flip',())
 st=h.call('_VX_D3D_Record@4',(0xFFFFFFFF,));assert st&255==3 and (st>>8)&255==2,hex(st)
 rec=h.rec_file;assert rec[:4]==b'VXR1' and len(rec)>20000,len(rec)
 sent=h.native[n0:];tris=0
 for p in sent:
  if u32(p,10452)==2:tris+=(u32(p,10064)&0xFFFF)//3
  else:
   for r in range(u32(p,10060)):b=u32(p,10064+4*r);tris+=(b&0xFFFF)//3 if b>>16==0 else max((b&0xFFFF)-2,0)
 line=replay_strict(rec);out=line
 import re
 m=re.match(r'(\d+) frames, (\d+) DrawPrimitives2 calls: ([\d.]+) packets a frame \(([\d.]+) indexed\), (\d+) triangles a frame',line)
 assert m and int(m.group(1))==2 and int(m.group(2))==3,line
 assert round(float(m.group(3))*2)==len(sent) and int(m.group(5))*2 in (tris,tris+1,tris-1),(line,len(sent),tris)
 assert 'refused by the validation' in line and line.endswith('0 packets refused by the validation') and 'did not have' not in out,out
 # .518 (audit G3): a recording whose process detaches before its last flip is given up; the next request is taken
 assert h.call('_VX_D3D_Record@4',(5,))==1;h.call('VX_rec_flip',());dp2(grid,1)
 assert h.call('_VX_D3D_Record@4',(0xFFFFFFFF,))&255==2
 h.call('_DllMain@12',(h.base,0,0));assert h.call('_VX_D3D_Record@4',(0xFFFFFFFF,))&255==0,'given up at the detach'
 h.call('_DllMain@12',(h.base,1,0));assert h.call('_VX_D3D_Record@4',(1,))==1 and h.call('_VX_D3D_Record@4',(0,))==0
 # a cancel while it records: dropped at the recording process's next flip, nothing written
 h.rec_file=b'';assert h.call('_VX_D3D_Record@4',(5,))==1;h.call('VX_rec_flip',());dp2(grid,1)
 assert h.call('_VX_D3D_Record@4',(0,))==2;h.call('VX_rec_flip',());assert h.call('_VX_D3D_Record@4',(0xFFFFFFFF,))&255==0 and h.rec_file==b''
 cases.append('.517: the linked HAL records two frames of DrawPrimitives2 (armed by VX_D3D_Record, begun and counted at flips, written to C:\\VXD3D.REC); the host replay makes the same packets and triangles, every service answer found, every packet valid; .518: a detach or a cancel while it records gives it up, the next request is taken')
 return cases
def phase_checks():
 """v10.431: VX_D3D_QueryPhases: off until a clearing query; each DrawPrimitives2 call then counted once, its exclusive phases summed
 (the HAL around the core, the commands, the per-call VxD query, the profiler's own calls); a query with 0x40000000 turns it off."""
 cases=[];h=D3D();u=h.u;dx7(h)
 pf=pack([32,0x40,0,32,0xff0000,0xff00,255,0]);u.mem_write(h.context+28,pf)
 h.surface(h.dst,h.dg,0,h.gpu.pitch,64,48)
 create=ARENA+0xa000;dp=ARENA+0xa100;command=ARENA+0xa200;local=ARENA+0xb000;q=ARENA+0xe800
 u.mem_write(create,pack([local,h.dst,0,h.pid,0,0]));h.call('_VX_D3D_Create@4',(create,));context=h.word(create+16)
 u.mem_write(h.src,bytes(128));u.mem_write(h.src+4,pack([h.sg]));u.mem_write(h.src+32,pack([0x800000]))
 u.mem_write(h.sg,bytes(72));u.mem_write(h.sg+20,pack([command,256]))
 state=struct.pack('<BBHII',8,0,1,22,1)
 def dp2():
  u.mem_write(command,state);u.mem_write(dp,pack([context,0,0x44,h.src,0,len(state),0,0,0,0,0,0,0,0]))
  assert h.call('_VX_D3D_DP2@4',(dp,))==1 and h.word(dp+48)==0
 h.u.mem_write(h.sym['vx_d3d_infocache'],pack([0])) # .522: the per-call readiness question asked every call (ACCEL_D3D_INFOCACHE off), as these counts expect
 def query(n):
  assert h.call("_VX_D3D_QueryPhases@8",(q,n))==min(n&0x3FFFFFFF,220);return [h.word(q+4*i) for i in range(min(n&0x3FFFFFFF,220))]
 dp2();w=query(144);assert w[0]==0x431 and w[2]==0,w[:3]
 lk=getattr(h,'locks',[]);assert lk and all(x[0]==1 and x[2]>=0x10000 and not x[1]&0xFFF for x in lk),lk # v10.432: the create asked for the DLL's image
 w=query(148);assert w[144]==((lk[-1][1]+lk[-1][2]-1)>>12)-(lk[-1][1]>>12)+1 and w[145]==lk[-1][1] and w[146:148]==[1,1],w[144:148]
 query(144|0x80000000);dp2();dp2();w=query(144)
 assert w[2]==2 and sum(w[3:9])==2,w[:9]
 ph=lambda k:w[34+k*11:45+k*11]
 for k in (1,6,8,9):assert ph(k)[2]==2 and sum(ph(k)[4:10])==2,(k,ph(k)) # commands, the VxD query, the HAL, the profiler: in both calls
 assert ph(7)[2]==0,ph(7) # no busy waits
 g=query(172)[148:];assert g[21]==2 and g[0]==0,g # v10.440: the geometry counters on with the phases: two calls, no triangles
 query(2|0x40000000);dp2();w=query(172);assert w[2]==2 and w[148+21]==2,'off: not counted'
 assert '_VX_D3D_Switch@8' not in h.sym,'.533: VX_D3D_Switch removed'
 cases.append('.533: VX_D3D_Switch is gone from the HAL (no switch is left)')
 cases.append('v10.431: DrawPrimitives2 phases counted only after the clearing query, each call once with its exclusive phases; off again on request (v10.440: the geometry counters with them); v10.432: a context create asks for the DLL\'s whole image to be locked, the answer reported')
 return cases

def texture_checks():
 """v10.169: video textures through the DirectDraw HAL: creation rules, row pitch, CPU loads by Blt."""
 cases=[];h=D3D();u=h.u;dx7(h)
 u.mem_write(h.context+28,pack([32,0x40,0,32,0xff0000,0xff00,255,0]))
 desc=ARENA+0xa100;can=ARENA+0xa200
 argb=pack([32,0x41,0,32,0xff0000,0xff00,255,0xff000000])
 def cancreate(w,hgt,caps,pf=argb):
  u.mem_write(desc,bytes(108));u.mem_write(desc,pack([108,0x1007,hgt,w]));u.mem_write(desc+72,pf);u.mem_write(desc+104,pack([caps]))
  u.mem_write(can,pack([0,desc,1,0x1234,0]));assert h.call('_VXCanCreate@4',(can,))==1;return h.word(can+12)
 assert cancreate(64,64,0x5000)==0 and cancreate(1,1024,0x5000)==0 and cancreate(1024,1024,0x5000)==0
 assert cancreate(64,48,0x5000)==0x80004001 and cancreate(4096,64,0x5000)==0x80004001 and cancreate(2048,64,0x5000)==0 and cancreate(64,64,0x405000|0x8)==0 # v10.199: 2048, mip chains
 assert cancreate(64,64,0x5000,pack([32,0x40,0,16,0xf800,0x7e0,0x1f,0]))==0 and cancreate(64,64,0x5000,pack([32,0x41,0,16,0xf00,0xf0,0xf,0xf000]))==0
 assert cancreate(64,64,0x5000,pack([32,0x41,0,16,0x7c00,0x3e0,0x1f,0x8000]))==0 and cancreate(64,64,0x5000,pack([32,0x40,0,16,0x7c00,0x3e0,0x1f,0]))==0
 assert cancreate(64,64,0x5000,pack([32,0x40,0,32,0xff,0xff00,0xff0000,0]))==0x88760091
 cases.append('CanCreate: video textures A8R8G8B8, X8R8G8B8 and (v10.172) R5G6B5, A1R5G5B5, A4R4G4B4, X1R5G5B5 with power-of-two sides up to 1024, no mipmaps, other formats refused')
 xrgb=pack([32,0x40,0,32,0xff0000,0xff00,255,0]);r565=pack([32,0x40,0,16,0xf800,0x7e0,0x1f,0])
 assert cancreate(64,64,0x7000,xrgb)==0 and cancreate(256,128,0x7000,r565)==0 and cancreate(4,4,0x7000,xrgb)==0
 assert cancreate(64,64,0x7000)==0 and cancreate(64,48,0x7000,xrgb)==0x80004001 and cancreate(64,64,0x407008,xrgb)==0x80004001 # v10.459: A8R8G8B8 taken
 assert cancreate(64,64,0x7000,pack([32,0x41,0,16,0x7c00,0x3e0,0x1f,0x8000]))==0x80004001,'A1R5G5B5 stays refused'
 rt=h.sym['vx_d3d_rttex'];u.mem_write(rt,pack([0]));assert cancreate(64,64,0x7000,xrgb)==0x80004001;u.mem_write(rt,pack([1]))
 cases.append('v10.456 ACCEL_D3D_RTTEX: CanCreate takes a render-target texture (TEXTURE | 3DDEVICE) in X8R8G8B8 or R5G6B5 (v10.459: and A8R8G8B8), power-of-two, one level; A1R5G5B5, other sizes, mip chains and RTTEX 0 refused')
 tl,tg=h.dst,h.dg;h.surface(tl,tg,0xC8000,0,64,64);u.mem_write(tl+32,pack([0x7000]));u.mem_write(tl+28,pack([0x2000]));u.mem_write(tg+40,xrgb)
 cs=ARENA+0xa300;lst=ARENA+0xa400;u.mem_write(lst,pack([tl]));u.mem_write(cs,pack([0,desc,lst,1,0x1234,0]))
 assert h.call('_VXCreate@4',(cs,))==0 and h.word(cs+16)==0 and h.word(tg+24)==256
 create=ARENA+0xa600;u.mem_write(create,pack([ARENA+0xb000,tl,0,h.pid,0,0]))
 assert h.call('_VX_D3D_Create@4',(create,))==1 and h.word(create+20)==0 and h.word(create+16)
 u.mem_write(create,pack([h.word(create+16),0]));h.call('_VX_D3D_Destroy@4',(create,))
 h.surface(tl,tg,0xC8000,0,64,64);u.mem_write(tl+32,pack([0x7000]));u.mem_write(tl+28,pack([0x2000]));u.mem_write(tg+40,r565)
 u.mem_write(cs,pack([0,desc,lst,1,0x1234,0]));assert h.call('_VXCreate@4',(cs,))==0 and h.word(cs+16)==0 and h.word(tg+24)==128
 h.surface(tl,tg,0xC8000,0,64,64);u.mem_write(tl+32,pack([0x7000]));u.mem_write(tl+28,pack([0x2000]));u.mem_write(tg+40,argb) # v10.459: A8R8G8B8
 u.mem_write(cs,pack([0,desc,lst,1,0x1234,0]));assert h.call('_VXCreate@4',(cs,))==0 and h.word(cs+16)==0 and h.word(tg+24)==256
 u.mem_write(create,pack([ARENA+0xb000,tl,0,h.pid,0,0]));assert h.call('_VX_D3D_Create@4',(create,))==1 and h.word(create+20)==0 and h.word(create+16)
 u.mem_write(create,pack([h.word(create+16),0]));h.call('_VX_D3D_Destroy@4',(create,))
 cases.append('v10.459: an A8R8G8B8 render-target texture: rows of 256 bytes at 64 wide, ContextCreate draws into it as the 32-bit target')
 cases.append('v10.456: CreateSurface gives a render-target texture the texture\'s rows (64 X8R8G8B8: 256 bytes; R5G6B5: 128), and ContextCreate draws into it')
 tl,tg=h.dst,h.dg;h.surface(tl,tg,0xC0000,0,8,8);u.mem_write(tl+32,pack([0x5000]));u.mem_write(tl+28,pack([0x2000]));u.mem_write(tg+40,argb)
 cs=ARENA+0xa300;lst=ARENA+0xa400;u.mem_write(lst,pack([tl]));u.mem_write(cs,pack([0,desc,lst,1,0x1234,0]))
 assert h.call('_VXCreate@4',(cs,))==0 and h.word(cs+16)==0 and h.word(tg+24)==32
 cases.append('CreateSurface gives a video texture rows of exactly width*4 bytes (the sampler takes no pitch)')
 # system-memory A8R8G8B8 copy -> video texture by Blt, then a colour fill of one quarter
 sl,sg=h.src,h.sg;sysmem=ARENA+0x30000;h.surface(sl,sg,0,64,8,8);u.mem_write(sl+32,pack([0x800]));u.mem_write(sl+28,pack([0x2000]));u.mem_write(sg+40,argb)
 u.mem_write(sg+20,pack([sysmem,32]));texels=[(0x80000000|(y*8+x)*0x010203) for y in range(8) for x in range(8)];u.mem_write(sysmem,pack(texels))
 b=bytearray(204);struct.pack_into('<II4iI4iII',b,0,0,tl,0,0,8,8,sl,0,0,8,8,0x1000000,0);struct.pack_into('<I',b,52,100);struct.pack_into('<I',b,60,0x00cc0020)
 u.mem_write(h.bd,bytes(b));assert h.call('_VXBlit@4',(h.bd,))==1 and h.word(h.bd+152)==0
 assert struct.unpack('<64I',u.mem_read(FB+0xC0000,256))==tuple(texels)
 struct.pack_into('<II4iI4iII',b,0,0,tl,4,4,8,8,0,0,0,0,0,0x400|0x1000000,0);struct.pack_into('<I',b,132,0x7f00ff00)
 u.mem_write(h.bd,bytes(b));assert h.call('_VXBlit@4',(h.bd,))==1 and h.word(h.bd+152)==0
 got=struct.unpack('<64I',u.mem_read(FB+0xC0000,256))
 assert all(got[y*8+x]==(0x7f00ff00 if x>=4 and y>=4 else texels[y*8+x]) for y in range(8) for x in range(8))
 cases.append('Blt loads a video texture from a 32-bit system-memory copy and colour-fills it with 4-byte pixels whatever the desktop depth')
 # v10.172: a 16-bit (R5G6B5) texture: rows of width*2, loaded by Blt with 2-byte texels
 rgb565=pack([32,0x40,0,16,0xf800,0x7e0,0x1f,0]);h.surface(tl,tg,0xC1000,0,8,8);u.mem_write(tl+32,pack([0x5000]));u.mem_write(tl+28,pack([0x2000]));u.mem_write(tg+40,rgb565)
 u.mem_write(cs,pack([0,desc,lst,1,0x1234,0]));assert h.call('_VXCreate@4',(cs,))==0 and h.word(cs+16)==0 and h.word(tg+24)==32 # v10.200: rows at least 32 bytes
 h.surface(sl,sg,0,16,8,8);u.mem_write(sl+32,pack([0x800]));u.mem_write(sl+28,pack([0x2000]));u.mem_write(sg+40,rgb565)
 t16=[(y*8+x)*0x0841&0xFFFF for y in range(8) for x in range(8)];u.mem_write(sg+20,pack([sysmem,16]));u.mem_write(sysmem,struct.pack('<64H',*t16))
 struct.pack_into('<II4iI4iII',b,0,0,tl,0,0,8,8,sl,0,0,8,8,0x1000000,0);u.mem_write(h.bd,bytes(b));assert h.call('_VXBlit@4',(h.bd,))==1 and h.word(h.bd+152)==0
 assert all(struct.unpack('<8H',u.mem_read(FB+0xC1000+y*32,16))==tuple(t16[y*8:y*8+8]) for y in range(8)) # v10.200: 32-byte rows
 cases.append('v10.172: an R5G6B5 texture loads by Blt with 2-byte texels (v10.200: rows of max(32, width*2) bytes)')
 # v10.279 (review): a wide system-memory surface in its own format is not held to the engine's pitch with the desktop's bytes per pixel
 wl,wg=h.src,h.sg;h.surface(wl,wg,0,0,5000,4);u.mem_write(wl+32,pack([0x800]));u.mem_write(wl+28,pack([0x2000]));u.mem_write(wg+40,rgb565)
 u.mem_write(lst,pack([wl]));u.mem_write(cs,pack([0,desc,lst,1,0x1234,0]))
 assert h.call('_VXCreate@4',(cs,))==0 and h.word(cs+16)==0 and h.word(wg+24)==10000,(h.word(cs+16),h.word(wg+24))
 cases.append('CreateSurface: a 5000-pixel R5G6B5 system-memory surface on a 32 bpp desktop is made (rows of 10000 bytes), not refused for 20000')
 # v10.279 (review): DestroySurface of a video surface retires a 3D draw still in flight before the memory goes back to the heap
 seen=[];h.gpu.hook('VX3DN_busy',lambda:seen.append(1) or 0);h.surface(tl,tg,0xC0000,0,8,8)
 ds=ARENA+0xa500;u.mem_write(ds,pack([0,tl,0,0]));h.call('_VXDestroySurface@4',(ds,));assert seen
 n=len(seen);u.mem_write(tl+32,pack([0x800]));h.call('_VXDestroySurface@4',(ds,));assert len(seen)==n
 cases.append('DestroySurface of a video surface waits for the 3D draw in flight (a system-memory one does not ask)')
 return cases

def memory_checks():
 cases=[];mib=1024*1024
 for limit,offset,expected in [(16*mib,24*mib,0x80070057),(64*mib,24*mib,0),
                               (64*mib,64*mib-0x3000,0),(64*mib,64*mib-0x2f00,0x80070057),
                               (128*mib,64*mib,0x80070057)]:
  h=D3D(vram_bytes=limit);u=h.u;dx7(h)
  u.mem_write(h.context+28,pack([32,0x40,0,32,0xff0000,0xff00,255,0]))
  h.surface(h.dst,h.dg,offset,256,64,48)
  create=ARENA+0xa000;u.mem_write(create,pack([ARENA+0xb000,h.dst,0,h.pid,0,0]))
  assert h.call('_VX_D3D_Create@4',(create,))==1 and h.word(create+20)==expected
  assert bool(h.word(create+16))==(expected==0)
  cases.append(f'v10.88 D3D target bounds: {limit//mib} MiB advertised, offset {offset:#x}, status {expected:#x}')
 for pitch,expected in [(0x7fe0,0),(0x8000,0x80070057)]:
  h=D3D();u=h.u;dx7(h)
  u.mem_write(h.context+28,pack([32,0x40,0,32,0xff0000,0xff00,255,0]))
  h.surface(h.dst,h.dg,0x10000,pitch,64,48)
  create=ARENA+0xa000;u.mem_write(create,pack([ARENA+0xb000,h.dst,0,h.pid,0,0]))
  assert h.call('_VX_D3D_Create@4',(create,))==1 and h.word(create+20)==expected
  cases.append(f'ContextCreate bounds ten-bit hardware pitch: {pitch:#x}, status {expected:#x}')
 return cases

def lock_checks():
 cases=[]
 for name,pf,caps in [('D32',[32,0x400,0,32,0,0xffffffff,0,0],0x24000),
                       ('RGB565 on a 32bpp desktop',[32,0x40,0,16,0xf800,0x7e0,31,0],0x6000)]:
  h=D3D();u=h.u;lock=ARENA+0xa000;unlock=ARENA+0xa100
  u.mem_write(h.context+28,pack([32,0x40,0,32,0xff0000,0xff00,255,0]))
  h.surface(h.dst,h.dg,0x10000,256,64,48)
  u.mem_write(h.dst+28,pack([0x2000]));u.mem_write(h.dst+32,pack([caps]));u.mem_write(h.dg+40,pack(pf))
  u.mem_write(lock,pack([0,h.dst])+bytes(36));u.mem_write(unlock,pack([0,h.dst,0,0]))
  assert h.call('_VXLock@4',(lock,))==0 and h.word(lock+32)==0
  assert h.call('_VXUnlock@4',(unlock,))==0 and h.word(unlock+8)==0
  cases.append(name+' Lock/Unlock leave healthy CPU mappings to the runtime')
  h.gpu.call('VX2D_device_lost')
  assert h.call('_VXLock@4',(lock,))==1 and h.word(lock+32)==0x887601c2
  cases.append(name+' Lock reports SURFACELOST after a native engine fault')
  assert h.call('_VXUnlock@4',(unlock,))==1 and h.word(unlock+8)==0x887601c2
  cases.append(name+' Unlock preserves the native engine fault fence')
 return cases

def alignment_checks():
 # Execute the linked Win16 bridge, supplying only its allocator/SetInfo
 # services. These are the actual VIDMEMINFO values offered to DirectDraw.
 from frontend import GDI
 cases=[]
 for bpp in (16,32):
  f=GDI(bpp);u=f.u;hal=0x90000
  f.call('DDHinstance_',far=False) # allocate the shared VMDAHAL
  assert u32(u.mem_read(hal,4),0)==27492
  u.mem_write(0x30700,struct.pack('<5I',0,(f.sel['argument']<<16)|0x720,0,0,0))
  u.mem_write(0x30720,struct.pack('<IHHII',16,0xf200,f.sel['code'],0,0))
  u.mem_write(0x1f200,b'\xb8\x01\x00\xca\x06\x00') # DDHAL_SetInfo returns TRUE, pops six bytes
  u.reg_write(UC_X86_REG_AX,0x700);u.reg_write(UC_X86_REG_DX,f.sel['argument'])
  assert f.call('DDNewCallbackFns_',far=False)==1
  for enabled in (False,True,False):
   u.mem_write(hal+19160,pack([0xa0100000 if enabled else 0]));u.reg_write(UC_X86_REG_AX,0)
   assert f.call('DDCreateDriverObject_',far=False)==1
   # VMDAHAL.ddHALInfo=60, DDHALINFO.vmiData=16, VIDMEMINFO alignment=52.
   align=struct.unpack('<4I',u.mem_read(hal+60+16+52,16));wanted=256 if enabled else 4
   assert align==(wanted,4,wanted,wanted),align
   # .577: DDHALINFO.dwMonitorFrequency (after vmiData's 80 bytes and ddCaps' 316: offset 412) from the VxD (OP_VX_HZ: the harness answers 60)
   assert struct.unpack('<I',u.mem_read(hal+60+412,4))[0]==60,[hex(x) for x in struct.unpack('<8I',u.mem_read(hal+60+396,32))]
   cases.append(f'{bpp}bpp Win16 bridge advertises {wanted}-byte target/texture/depth alignment with D3D '+('enabled' if enabled else 'disabled'))
 return cases

class D3DFail(D3D):
 """v10.319 (audit H3): the access IOCTL failing on chosen calls; device handles opened and closed counted."""
 def __init__(self,*a,**k):
  self.fail=0;self.opened=0;self.closed=0;self.handles=[];self.closed_h=[];super().__init__(*a,**k)
 def import_call(self,u,a,z,data):
  name,dll=data
  if name=='CreateFileA' and self.string(self.arg(0))=='\\\\.\\VX85598': # v10.322: a new handle value each open, so a close names which one
   self.opened+=1;hv=0x1000+self.opened;self.handles.append(hv)
   sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,hv);u.reg_write(UC_X86_REG_ESP,sp+4+28);u.reg_write(UC_X86_REG_EIP,self.word(sp));return
  elif name=='CloseHandle' and self.arg(0) in self.handles:self.closed+=1;self.closed_h.append(self.arg(0))
  elif name=='GetLastError':
   sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,50);u.reg_write(UC_X86_REG_ESP,sp+4);u.reg_write(UC_X86_REG_EIP,self.word(sp));return
  elif name=='DeviceIoControl' and self.arg(1)==0x85500017 and self.fail:
   self.fail-=1;sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,0);u.reg_write(UC_X86_REG_ESP,sp+4+32);u.reg_write(UC_X86_REG_EIP,self.word(sp));return
  return super().import_call(u,a,z,data)

def audit319_checks():
 cases=[]
 # H3: a call that fails on the slot's handle and on a new one: the new one is closed, the slot's kept (it was dropped unclosed each time)
 h=D3DFail();u=h.u;lock=ARENA+0xa000;unlock=ARENA+0xa100
 u.mem_write(h.context+28,pack([32,0x40,0,32,0xff0000,0xff00,255,0]))
 h.surface(h.dst,h.dg,0x10000,256,64,48);u.mem_write(h.dst+28,pack([0x2000]));u.mem_write(h.dst+32,pack([0x6000]));u.mem_write(h.dg+40,pack([32,0x40,0,16,0xf800,0x7e0,31,0]))
 u.mem_write(lock,pack([0,h.dst])+bytes(36));u.mem_write(unlock,pack([0,h.dst,0,0]))
 assert h.call('_VXLock@4',(lock,))==0;assert h.call('_VXUnlock@4',(unlock,))==0
 o0,c0=h.opened,h.closed
 for k in range(5):
  h.fail=2;h.call('_VXLock@4',(lock,));assert h.word(lock+32)==0x887601c2,hex(h.word(lock+32))
 assert h.opened-o0==5 and h.closed-c0==5,(h.opened-o0,h.closed-c0)
 assert h.closed_h[-5:]==h.handles[-5:],'each fresh handle closed, the slot\'s kept'
 h.fail=0;assert h.call('_VXLock@4',(lock,))==0 and h.word(lock+32)==0;h.call('_VXUnlock@4',(unlock,))
 assert h.opened-o0==5,'the slot handle was kept: no new open'
 cases.append('v10.319 (audit H3): an IOCTL that fails on the slot handle and on a fresh one closes the fresh one and keeps the slot (no handle leaked a call)')
 slot=h.handles[0]
 for k in range(5): # v10.322 (review: five retries, six opens and no close): each successful retry closes the handle it replaces
  h.fail=1;assert h.call('_VXLock@4',(lock,))==0 and h.word(lock+32)==0;h.call('_VXUnlock@4',(unlock,))
  assert h.closed_h[-1]==slot,(hex(h.closed_h[-1]),hex(slot));slot=h.handles[-1]
 assert h.opened-o0==10 and h.closed-c0==10,(h.opened-o0,h.closed-c0)
 h.fail=0;n0=h.opened;assert h.call('_VXLock@4',(lock,))==0;h.call('_VXUnlock@4',(unlock,));assert h.opened==n0,'the last fresh handle is the slot\'s now'
 cases.append('v10.322 (review): a call that fails only on the slot handle: the fresh one replaces it and the replaced one is closed (5 retries: 5 opens, 5 closes)')
 # a slot left under this process's ID by an ended process (Windows 98 gives the ID again) is forgotten at attach, not closed
 h.call('_DllMain@12',(h.base,1,0));c1=h.closed;h.fail=0;assert h.call('_VXLock@4',(lock,))==0;h.call('_VXUnlock@4',(unlock,))
 assert h.closed==c1 and h.opened==n0+1,'after attach the stale slot is not closed; a new handle is opened'
 cases.append('v10.322: at a process attach a slot under its ID (an ended process\'s) is forgotten, its value not closed')
 # H2: the callback lock held by a process that has ended is taken over; held by a running one (or this process) it is not
 class D3DOwner(D3D):
  def import_call(self,u,a,z,data):
   name,dll=data
   if name in ('OpenProcess','GetExitCodeProcess'):
    sp=u.reg_read(UC_X86_REG_ESP);args=(3 if name=='OpenProcess' else 2)
    if name=='OpenProcess':ret=0x88 if self.arg(2) in self.alive else 0
    else:u.mem_write(self.arg(1),pack([259]));ret=1 # STILL_ACTIVE
    u.reg_write(UC_X86_REG_EAX,ret);u.reg_write(UC_X86_REG_ESP,sp+4+4*args);u.reg_write(UC_X86_REG_EIP,self.word(sp));return
   if name=='CloseHandle' and self.arg(0)==0x88:
    sp=u.reg_read(UC_X86_REG_ESP);u.reg_write(UC_X86_REG_EAX,1);u.reg_write(UC_X86_REG_ESP,sp+8);u.reg_write(UC_X86_REG_EIP,self.word(sp));return
   return super().import_call(u,a,z,data)
 h=D3DOwner();h.alive={0x777};u=h.u;ow,tp,tv=h.sym['callback_owner'],h.sym['tok_pid'],h.sym['tok_val']
 out=ARENA+0xb800
 def query():
  u.mem_write(out,bytes(4096));return h.call('_VX_D3D_QueryDraws@8',(out,1+40*32))
 assert query()>=1 and h.word(ow)==0
 mine=[h.word(tv+4*k) for k in range(64) if h.word(tp+4*k)==h.pid];assert len(mine)==1 and mine[0]>1,mine # this process's token from DllMain
 u.mem_write(tp+4*60,pack([0x777]));u.mem_write(tv+4*60,pack([0x5000])) # a running process's token
 u.mem_write(tp+4*61,pack([0x555]));u.mem_write(tv+4*61,pack([0x5001])) # an ended process's token
 for owner,taken in ((0x5000,False),(mine[0],False),(1,False),(0x5001,True),(0x5002,True)):
  u.mem_write(ow,pack([owner]));r=query();assert (r>=1)==taken,(hex(owner),r)
  if taken:assert h.word(ow)==0
 # a process that attaches again under the same ID (Windows 98 gives a restarted program the ended one's ID) gets a new token:
 # the lock its predecessor held is taken over
 u.mem_write(ow,pack([mine[0]]));h.call('_DllMain@12',(h.base,1,0))
 new=[h.word(tv+4*k) for k in range(64) if h.word(tp+4*k)==h.pid];assert len(new)==1 and new[0]!=mine[0]
 assert query()>=1 and h.word(ow)==0
 # a process detaching while it holds the lock lets it go
 u.mem_write(ow,pack([new[0]]));h.call('_DllMain@12',(h.base,0,0));assert h.word(ow)==0
 cases.append('v10.319 (audit H2): the HAL callback lock held by an ended process (or by this ID before it attached again) is taken over and released; held by a running process or this one it is not; a process detaching lets it go')
 # H1: a DXT1 texture Blt honours the clip list: none touches nothing, a partial list is refused untouched, the whole surface copies
 h=D3D();u=h.u;dx7(h);u.mem_write(h.context+28,pack([32,0x40,0,32,0xff0000,0xff00,255,0]))
 # Watcom's memmove (dxt_spread) saves ES with push/pop es, which Unicorn's flat model does not keep: done here, the caller pops the arguments
 def memmove(uc,addr,size,ud):
  sp=uc.reg_read(UC_X86_REG_ESP);ret,d,src,n=struct.unpack('<4I',uc.mem_read(sp,16));uc.mem_write(d,bytes(uc.mem_read(src,n)))
  uc.reg_write(UC_X86_REG_EAX,d);uc.reg_write(UC_X86_REG_ESP,sp+4);uc.reg_write(UC_X86_REG_EIP,ret)
 u.hook_add(UC_HOOK_CODE,memmove,None,h.sym['memmove'],h.sym['memmove'])
 dxt1=pack([32,4,0x31545844,0,0,0,0,0]);tl,tg,sl,sg=h.dst,h.dg,h.src,h.sg;sysmem=ARENA+0x30000;clip=ARENA+0x9000
 h.surface(tl,tg,0xC4000,32,8,8);u.mem_write(tl+32,pack([0x5000]));u.mem_write(tl+28,pack([0x2000]));u.mem_write(tg+40,dxt1)
 h.surface(sl,sg,0,32,8,8);u.mem_write(sl+32,pack([0x800]));u.mem_write(sl+28,pack([0x2000]));u.mem_write(sg+40,dxt1);u.mem_write(sg+20,pack([sysmem,32]))
 blocks=bytes(range(1,33));u.mem_write(sysmem,blocks)
 def blt(rects):
  u.mem_write(FB+0xC4000,b'\xEE'*64);b=bytearray(204);struct.pack_into('<II4iI4iII',b,0,0,tl,0,0,8,8,sl,0,0,8,8,0x1000000,0);struct.pack_into('<I',b,60,0x00cc0020)
  if rects is not None:
   u.mem_write(clip,b''.join(struct.pack('<4i',*r) for r in rects) or bytes(16));struct.pack_into('<I',b,160,1);struct.pack_into('<II',b,196,len(rects),clip)
  u.mem_write(h.bd,bytes(b));assert h.call('_VXBlit@4',(h.bd,))==1;return h.word(h.bd+152),bytes(u.mem_read(FB+0xC4000,64))
 spread=blocks[:16]+bytes(16)+blocks[16:]+bytes(16) # v10.200: block rows at the sampler's 32-byte pitch, the rest of each row 0
 assert blt(None)==(0,spread)
 assert blt([(0,0,8,8)])==(0,spread)
 assert blt([])==(0,b'\xEE'*64)
 hr,data=blt([(0,0,4,4)]);assert hr==0x80004001 and data==b'\xEE'*64,(hex(hr),data)
 hr,data=blt([(0,0,4,8),(4,0,8,8)]);assert hr==0x80004001 and data==b'\xEE'*64
 cases.append('v10.319 (audit H1): a DXT1 Blt with an empty clip list touches nothing; a partial list is refused with nothing written; the whole surface (clipped to itself or not) copies')
 return cases

if __name__=='__main__':
 cases=checks();(ROOT/'verification'/'D3D_CALLBACKS.json').write_text(json.dumps({'scope':__doc__,'passed':len(cases),'cases':cases},indent=2)+'\n');print(f'{len(cases)} linked Direct3D callback checks passed.')
