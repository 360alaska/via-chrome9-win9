#!/usr/bin/env python3
"""Execute the built Win16 hooks with separate SS/DS and mocked DIB imports.
The genuine DIBENG rasterizer and Windows dispatcher are not executed.
"""
import struct,json
from harness16 import Frontend
from engine import GPU,pack,ARENA
from validate import ROOT,u16,u32
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
class Dummy:
 def call(self,n,args=()):assert n=='VESA_init_hw';return 1
class GDI(Frontend):
 def __init__(self,bpp=32):
  self.gpu=GPU(bpp);self.fallbacks=0;self.reply=None;self.last=None;self.sreply=1;self.sreqs=[]
  super().__init__(protected=True,detailed_imports=True,backend=Dummy())
  self.u.mem_write(0x30100,bytes(self.gpu.u.mem_read(self.gpu.hda,320)))
  def data(name,blob):self.u.mem_write(0x20000+self.sym[name][1],blob)
  data('_hda',struct.pack('<HH',0x100,self.sel['argument']))
  data('_lpDriverPDevice',struct.pack('<HH',0,self.sel['argument']))
  for n,v in [('_wEnabled',1),('_wBpp',bpp),('_wScreenX',64),('_wScreenY',48)]:data(n,struct.pack('<H',v))
  self.u.mem_write(0x3001c,struct.pack('<H',0x8000)) # VRAM flag
  self.u.mem_write(0x30200,bytes(334));self.u.mem_write(0x30200,struct.pack('<BBHIHI',1,20 if bpp==16 else 32,0,0x00682070 if bpp==16 else 0x00563412,0,0));self.u.mem_write(0x3024e,b''.join(struct.pack('<H',0x68EC) for i in range(64)) if bpp==16 else bytes(0))
  for (module,ordinal),at in self.imports.items():
   if module=='DIBENG':
    self.u.hook_add(UC_HOOK_CODE,self.colorinfo if ordinal==2 else self.dib,None,at,at)
 def interrupt(self,u,num,_):
  if num==0x31:
   op=u.reg_read(UC_X86_REG_AX);sel=u.reg_read(UC_X86_REG_BX)
   if op==6:
    base={self.sel['data']:0x20000,self.sel['stack']:0x50000,self.sel['argument']:0x30000}[sel]
    u.reg_write(UC_X86_REG_CX,base>>16);u.reg_write(UC_X86_REG_DX,base&65535)
   elif op==0:
    u.mem_write(0x80000+72,struct.pack('<HHBBBB',65535,0,0,0x92,0,0));u.reg_write(UC_X86_REG_GDTR,(0,0x80000,79,0));u.reg_write(UC_X86_REG_AX,72)
   elif op==0x501:
    u.reg_write(UC_X86_REG_BX,9);u.reg_write(UC_X86_REG_CX,0)
   elif op in (7,8):
    assert sel==72
    v=(u.reg_read(UC_X86_REG_CX)<<16)|u.reg_read(UC_X86_REG_DX)
    if op==7:u.mem_write(0x80000+72,struct.pack('<HHBBBB',65535,v&65535,(v>>16)&255,0x92,0,v>>24))
    else:u.mem_write(0x80000+72,struct.pack('<H',v))
   else:raise AssertionError(('DPMI',hex(op)))
   u.reg_write(UC_X86_REG_EFLAGS,u.reg_read(UC_X86_REG_EFLAGS)&~1);return
  return super().interrupt(u,num,_)
 def api(self,u,a,z,_):
  op=u.reg_read(UC_X86_REG_EDX)&65535
  if op==0x1133:
   self.binding=(u.reg_read(UC_X86_REG_ESI),u.reg_read(UC_X86_REG_EDI));u.reg_write(UC_X86_REG_ECX,1);return
  if op==0x1134:
   p=u.reg_read(UC_X86_REG_ESI);n=u.reg_read(UC_X86_REG_ECX);assert n==1136,n;self.sreqs=getattr(self,'sreqs',[])+[list(struct.unpack('<284I',u.mem_read(p,n)))]
   u.reg_write(UC_X86_REG_ECX,self.sreply);return
  if op!=0x1132:return super().api(u,a,z,_)
  p=u.reg_read(UC_X86_REG_ESI);n=u.reg_read(UC_X86_REG_ECX);assert n==372;self.nsub=getattr(self,'nsub',0)+1
  self.last=list(struct.unpack('<93I',u.mem_read(p,n)));self.reqs=getattr(self,'reqs',[])+[self.last]
  result=self.gpu.submit(self.last) if self.reply is None else self.reply
  u.reg_write(UC_X86_REG_ECX,result)
 def colorinfo(self,u,a,z,_):
  # DIB_ColorInfo(lpDestDev, dwColorin, lpPColor): mock stores the scaled RGB565 (floor(c*max/255)) at *lpPColor and returns the colour. Only the diagnostic trace calls it.
  sp=u.reg_read(UC_X86_REG_SP);ip,cs,po,ps,color=struct.unpack('<HHHHI',u.mem_read(0x50000+sp,12))
  r,g,b=color&255,(color>>8)&255,(color>>16)&255;ph=((r*31//255)<<11)|((g*63//255)<<5)|(b*31//255)
  u.mem_write(0x20000+po,struct.pack('<I',ph))
  u.reg_write(UC_X86_REG_AX,color&65535);u.reg_write(UC_X86_REG_DX,color>>16);u.reg_write(UC_X86_REG_SP,sp+4+12);u.reg_write(UC_X86_REG_CS,cs);u.reg_write(UC_X86_REG_IP,ip)
 def dib(self,u,a,z,_):
  self.fallbacks+=1;sp=u.reg_read(UC_X86_REG_SP);ip,cs=struct.unpack('<HH',u.mem_read(0x50000+sp,4))
  # Only BitBlt / StretchBlt are called in this test.
  argc=getattr(self,'dib_argc',None) or (36 if self.stretch else 32);self.dib_args=bytes(u.mem_read(0x50000+sp+4,argc))
  u.reg_write(UC_X86_REG_AX,1);u.reg_write(UC_X86_REG_SP,sp+4+argc);u.reg_write(UC_X86_REG_CS,cs);u.reg_write(UC_X86_REG_IP,ip)
 def exttextout(self,opts=0x100,rect=(10,8,40,30),clip=None):
  # ExtTextOut(dest, x, y, clip, string, count, font, drawmode, xform, widths, opaque, options): Pascal, last argument at the lowest address
  far=lambda off:struct.pack('<HH',off,self.sel['argument'])
  self.u.mem_write(0x30600,struct.pack('<4h',*rect))
  if clip:self.u.mem_write(0x30620,struct.pack('<4h',*clip))
  args=struct.pack('<H',opts)+far(0x600)+far(0)+far(0)+far(0x400)+far(0)+struct.pack('<h',0)+far(0)+(far(0x620) if clip else struct.pack('<HH',0,0))+struct.pack('<HH',0,0)+far(0)
  self.dib_argc=len(args)
  try:return self.call('ExtTextOut',args,incoming_ds=self.sel['stack'])
  finally:self.dib_argc=None
 def output(self,pts,style=18,pen=(0,1,0,0,0x123456),rop2=13,clip=None,bkmode=0,bkcolor=0):
  # Output(dest, style, count, points, pen, brush, drawmode, clip): Pascal, last argument at the lowest address. The draw mode sits at 0x400, the pen at 0x700, the points at 0x640.
  far=lambda off:struct.pack('<HH',off,self.sel['argument'])
  self.u.mem_write(0x30640,b''.join(struct.pack('<hh',*p) for p in pts));self.u.mem_write(0x30700,struct.pack('<HBBII',*pen))
  self.u.mem_write(0x30400,struct.pack('<hhI',rop2,bkmode,bkcolor))
  if clip:self.u.mem_write(0x30620,struct.pack('<4h',*clip))
  args=(far(0x620) if clip else struct.pack('<HH',0,0))+far(0x400)+far(0)+far(0x700)+far(0x640)+struct.pack('<HH',len(pts),style)+far(0)
  self.dib_argc=len(args);self.last=None;self.nsub=0;self.dib_args=None;self.fallbacks=0;self.reqs=[]
  try:return self.call('Output',args,incoming_ds=self.sel['stack'])
  finally:self.dib_argc=None
 def bitblt(self,rop=0x00f00021,source=True,stretch=False):
  self.stretch=stretch;far=lambda off:struct.pack('<HH',off,self.sel['argument'])
  # Pascal arguments are stacked right-to-left in memory, with far pointers
  # retaining their little-endian offset:selector representation.
  args=far(0x400)+far(0x200)+struct.pack('<I',rop)
  args+=struct.pack('<HH',13,19)
  args+=struct.pack('<HH',3,2)+far(0 if source else 0x500)
  if stretch:args+=struct.pack('<HH',13,19)
  args+=struct.pack('<HH',6,7)+far(0)
  if stretch:
   self.u.mem_write(0x30300,struct.pack('<4h',11,9,22,16));args=far(0x300)+args
  return self.call('StretchBlt' if stretch else 'BitBlt',args,incoming_ds=self.sel['stack'])
 def sdibits(self,bits=24,W=21,H=13,topdown=False,dest=(10,8,42,26),src=(0,0,21,13),clip=None,rop=0x00CC0020,fGet=0,compression=0,planes=1,usebits=True,useinfo=True,sdev=0,masks=(0xF800,0x07E0,0x001F),clrused=0,pal=None,trans=False,bitsoff=0x1100,sizeimage=0,hsize=40,tail=None):
  # StretchDIBits(dest, fGet, DestX, DestY, DestW, DestH, SrcX, SrcY, SrcW, SrcH, bits, info, translate, rop, brush, drawmode, clip): Pascal, last argument at the lowest address
  far=lambda off:struct.pack('<HH',off,self.sel['argument']);null=struct.pack('<HH',0,0)
  self.u.mem_write(0x31000,struct.pack('<IiiHHIIiiII',hsize,W,-H if topdown else H,planes,bits,compression,sizeimage,0,0,clrused,0)+struct.pack('<3I',*masks))
  if hsize>52:self.u.mem_write(0x31034,bytes(hsize-52)) # .532: a V4/V5 header's rest
  if tail is not None:self.u.mem_write(0x31000+hsize,struct.pack('<3I',*tail)) # .532: what follows the header
  if pal is not None:self.u.mem_write(0x31028,struct.pack('<%dI'%len(pal),*pal))
  if clip:self.u.mem_write(0x30300,struct.pack('<4h',*clip))
  args=(far(0x300) if clip else null)+far(0x400)+far(0x500)+struct.pack('<I',rop)+(far(0x600) if trans else null)+(far(0x1000) if useinfo else null)+(far(bitsoff) if usebits else null)
  args+=struct.pack('<4H',src[3],src[2],src[1],src[0])+struct.pack('<4H',dest[3]&65535,dest[2]&65535,dest[1]&65535,dest[0]&65535)+struct.pack('<H',fGet)+far(sdev)
  # (the arguments above are in memory order: lowest address first, so the last parameter comes first)
  self.dib_argc=len(args);self.fallbacks=0;self.sreqs=[];self.stretch=False
  try:return self.call('StretchDIBits',args,incoming_ds=self.sel['stack'])
  finally:self.dib_argc=None

 def dtd(self,bits=24,W=21,H=13,x=10,y=8,iscan=0,cscans=None,clip=(12,9,30,20),topdown=False,compression=0,masks=(0xF800,0x07E0,0x001F),usebits=True,useinfo=True):
  # DibToDevice(dest, X, Y, iScan, cScans, clip, drawmode, bits, info, translate): Pascal, last argument at the lowest address
  far=lambda off:struct.pack('<HH',off,self.sel['argument']);null=struct.pack('<HH',0,0)
  self.u.mem_write(0x31000,struct.pack('<IiiHHIIiiII',40,W,-H if topdown else H,1,bits,compression,0,0,0,0,0)+struct.pack('<3I',*masks))
  if clip:self.u.mem_write(0x30300,struct.pack('<4h',*clip))
  args=null+(far(0x1000) if useinfo else null)+(far(0x1100) if usebits else null)+far(0x400)+(far(0x300) if clip else null)+struct.pack('<4H',H if cscans is None else cscans,iscan,y&65535,x&65535)+far(0)
  self.dib_argc=len(args);self.fallbacks=0;self.sreqs=[];self.stretch=False
  try:return self.call('DibToDevice',args,incoming_ds=self.sel['stack'])
  finally:self.dib_argc=None

def checks():
 cases=[]
 for bpp in (16,32):
  f=GDI(bpp);f.gpu.prefill()
  if bpp==16:
   assert f.bitblt()==1 and f.last[23]==0x68EC and not f.fallbacks;cases.append('16bpp solid brush: dp32FgColor holds the raw COLORREF 00682070, the engine gets the converted pixel 68EC from BrushBits (v10.67f)')
   f.u.mem_write(0x3024e+2,struct.pack('<H',0x1111));before=f.fallbacks
   assert f.bitblt()==1 and f.fallbacks==before+1;cases.append('16bpp solid brush whose first two realized pixels disagree is declined (v10.67g sanity check)')
   f.u.mem_write(0x3024e+2,struct.pack('<H',0x68EC));f.u.mem_write(0x3024e+40,struct.pack('<H',0x1111));assert f.bitblt()==1 and f.last[23]==0x68EC and f.fallbacks==before+1;cases.append('16bpp solid brush: only the first realized pixels are trusted, later words are not read as a pattern');f.u.mem_write(0x3024e+40,struct.pack('<H',0x68EC));f.fallbacks=0
   f.u.mem_write(0x30200,bytes([0x80]));f.u.mem_write(0x3024e,b''.join(struct.pack('<H',0x1234) for i in range(64)))
   assert f.bitblt()==1 and f.last[2]==3 and f.last[23]==0x1234 and not f.fallbacks;cases.append('16bpp dithered brush whose 8x8 pixels are one colour goes to the engine with the physical pixel colour')
  else:
   assert f.bitblt()==1 and f.last[23]==0x123456 and not f.fallbacks;cases.append('32bpp realized solid colour / SS != DS')
  f.u.mem_write(0x30201,bytes([32 if bpp==16 else 16]));before=f.fallbacks
  assert f.bitblt()==1 and f.fallbacks==before+1;cases.append(f'{bpp}bpp brush realized at another depth takes DIB fallback (v10.67e: raw brush colours must never reach the engine)')
  f.u.mem_write(0x30201,bytes([20 if bpp==16 else 32]))
  assert f.bitblt(0x00cc0020)==1 and f.last[2]==2;cases.append(f'{bpp}bpp GDI copy packet')
  # v10.84: ROP3 wiring (VX2ROP: the engine does every ROP3 exactly)
  saved_flags=bytes(f.u.mem_read(0x30200,1));f.u.mem_write(0x30200,bytes([1]));solid=0x1234 if bpp==16 else 0x123456
  for rop,code in ((0x88,0x008800C6),(0xEE,0x00EE0086),(0x66,0x00660046),(0x33,0x00330008),(0x44,0x00440328),(0x11,0x001100A6)):
   f.last=None;before=f.fallbacks
   assert f.bitblt(code)==1 and f.fallbacks==before and f.last[2]==2 and f.last[25]==rop and not f.last[3]&32,(hex(code),f.last);cases.append(f'{bpp}bpp source ROP {rop:02x} from the screen goes to the engine as a plain copy')
  for rop,code in ((0xFB,0x00FB0A09),(0xC0,0x00C000CA),(0xBB,0x00BB0226)):
   f.last=None;before=f.fallbacks
   if rop==0xBB:assert f.bitblt(code)==1 and f.fallbacks==before and f.last[25]==0xBB and not f.last[3]&32;cases.append(f'{bpp}bpp MERGEPAINT (no pattern term) is a plain copy');continue
   assert f.bitblt(code)==1 and f.fallbacks==before and f.last[2]==2 and f.last[25]==rop and f.last[3]&32 and f.last[23]==solid,(hex(code),f.last[2:4],f.last[23:26]);cases.append(f'{bpp}bpp pattern+source ROP {rop:02x} with a solid brush is a copy with the fixed colour')
  f.last=None;before=f.fallbacks;f.u.mem_write(0x30200,bytes([0x80]));px=2 if bpp==16 else 4;keep=bytes(f.u.mem_read(0x3024e+px,px));f.u.mem_write(0x3024e+px,b'\x5a'*px) # v10.122: a patterned brush (one pixel differs): a uniform one counts as solid
  assert f.bitblt(0x00FB0A09)==1 and f.fallbacks==before+1 and f.last is None;cases.append(f'{bpp}bpp pattern+source ROP with a non-solid (patterned) brush takes DIB fallback');f.u.mem_write(0x30200,bytes([1]));f.u.mem_write(0x3024e+px,keep)
  f.last=None;before=f.fallbacks
  assert f.bitblt(0x008800C6,source=False)==1 and f.fallbacks==before+1 and f.last is None;cases.append(f'{bpp}bpp source ROP with a source that is not the screen takes DIB fallback')
  for rop,code in ((0xA0,0x00A000C9),(0x0A,0x000A0329),(0x5F,0x005F00E9),(0xFA,0x00FA0089),(0x0F,0x000F0001)):
   f.last=None;before=f.fallbacks
   assert f.bitblt(code)==1 and f.fallbacks==before and f.last[2]==1 and f.last[25]==rop and f.last[23]==solid,(hex(code),f.last[2],f.last[25]);cases.append(f'{bpp}bpp pattern ROP {rop:02x} with a solid brush is an engine fill')
  f.last=None;before=f.fallbacks
  assert f.bitblt(0x00AA0029)==1 and f.fallbacks==before and f.last is None;cases.append(f'{bpp}bpp D (ROP AA) draws nothing and takes no fallback')
  f.last=None;before=f.fallbacks
  assert f.bitblt(0x018800C6)==1 and f.fallbacks==before+1 and f.last is None;cases.append(f'{bpp}bpp ROP code with extra high bits takes DIB fallback')
  f.u.mem_write(0x30200,saved_flags)
  assert f.bitblt(stretch=True)==1 and f.last[3]&1 and f.last[19:23]==[11,9,22,16];cases.append(f'{bpp}bpp StretchBlt native clip')
  bits=bytearray()
  for i in range(64):
   c=(0x1234 if (i+(i//8))&1 else 0xabcd)
   bits+=struct.pack('<H' if bpp==16 else '<I',c)
  f.u.mem_write(0x30200,bytes([0]));f.u.mem_write(0x3024e,bytes(bits))
  assert f.bitblt()==1 and f.last[2]==3;cases.append(f'{bpp}bpp two-colour realized pattern -> mono registers')
  vals=[(0x10000+i*701)&(0xFFFF if bpp==16 else 0xFFFFFFFF) for i in range(64)]
  f.u.mem_write(0x3024e,b''.join(struct.pack('<H' if bpp==16 else '<I',v) for v in vals))
  before=f.fallbacks;assert f.bitblt()==1 and f.fallbacks==before and f.last[2]==4 and f.last[29:93]==vals;cases.append(f'{bpp}bpp multicolour realized pattern -> colour pattern fill from video memory, the 64 realized pixels travel in the request (v10.77)')
  f.u.mem_write(0x30202,struct.pack('<H',2));before=f.fallbacks
  assert f.bitblt()==1 and f.fallbacks==before+1;cases.append(f'{bpp}bpp hatched brush without a valid mask takes DIB fallback')
  f.u.mem_write(0x30400,struct.pack('<hhII',13,2,0x1234,0x5678));f.u.mem_write(0x30200,bytes([0x10]));before=f.fallbacks
  r_=f.bitblt();assert r_==1 and f.fallbacks==before and f.last[2]==4 and f.last[29:93]==vals,(r_,f.fallbacks,before,f.last[2],bpp);cases.append(f'{bpp}bpp hatched brush with a valid mask (flags 90 style) goes to the engine with its realized pixels (v10.79)')
  f.u.mem_write(0x30402,struct.pack('<h',1));before=f.fallbacks
  r_=f.bitblt();assert r_==1 and f.fallbacks==before and f.last[2]==4 and f.last[29:93]==vals,(r_,f.fallbacks,before,f.last[2]);cases.append(f'{bpp}bpp hatch in TRANSPARENT mode is drawn like the opaque one (DIBENG ignores the mode for hatches; v10.82)')
  f.u.mem_write(0x30402,struct.pack('<h',0));before=f.fallbacks
  r_=f.bitblt();assert r_==1 and f.fallbacks==before and f.last[2]==4;cases.append(f'{bpp}bpp hatch with an unset draw mode is drawn like the opaque one')
  f.u.mem_write(0x30402,struct.pack('<h',2));before=f.fallbacks
  assert f.bitblt(0x00A000C9)==1 and f.fallbacks==before+1;cases.append(f'{bpp}bpp hatch with a ROP other than PATCOPY/PATINVERT stays with DIBENG (v10.84)')
  assert f.bitblt(0x005A0049)==1 and f.fallbacks==before+1 and f.last[2]==4;cases.append(f'{bpp}bpp hatch with PATINVERT still goes to the engine')
  f.u.mem_write(0x30202,struct.pack('<H',1));before=f.fallbacks
  assert f.bitblt()==1 and f.fallbacks==before+1;cases.append(f'{bpp}bpp hollow brush takes DIB fallback')
  f.u.mem_write(0x30200,bytes([0]));f.u.mem_write(0x30202,struct.pack('<H',0))
  # Brush made from a 1-bit bitmap: pattern in dp32BrushMono (row byte x4, MSB leftmost), colours from the draw mode (0 bits text, 1 bits background).
  rows=[0x80,0x42,0x24,0x18,0xA5,0x81,0x7E,0x99]
  f.u.mem_write(0x30200,bytes([0x8c,20 if bpp==16 else 32])+struct.pack('<H',3));f.u.mem_write(0x3020e,b''.join(bytes([r])*4 for r in rows))
  f.u.mem_write(0x30400,struct.pack('<hhII',13,2,0x00d3431d if bpp==32 else 0x1234,0x004d17c7 if bpp==32 else 0x5678))
  want=[0,0]
  for y,r in enumerate(rows):
   rr=0
   for x in range(8):
    if (~r)&(0x80>>x)&255:rr|=1<<x
   want[y//4]|=rr<<((3-(y&3))*8)
  assert f.bitblt()==1 and f.last[2]==3 and f.last[27:29]==want and f.last[23]==(0x5678 if bpp==16 else 0x004d17c7) and f.last[24]==(0x1234 if bpp==16 else 0x00d3431d),(f.last[2],f.last[23:25],f.last[27:29],want)
  cases.append(f'{bpp}bpp brush from a 1-bit bitmap -> mono pattern from the mono rows, colours from the draw mode')
  f.u.mem_write(0x30402,struct.pack('<h',1));before=f.fallbacks;f.last=None
  assert f.bitblt()==1 and f.fallbacks==before and f.last[2]==3 and f.last[27:29]==want and f.last[23]==(0x5678 if bpp==16 else 0x004d17c7) and f.last[24]==(0x1234 if bpp==16 else 0x00d3431d);cases.append(f'{bpp}bpp 1-bit bitmap brush with transparent background -> the same mono pattern request as opaque (v10.89)')
  f.u.mem_write(0x30402,struct.pack('<h',2))
  f.u.mem_write(0x30200,bytes([1]));f.u.mem_write(0x30202,bytes(2));f.u.mem_write(0x3024e,b''.join(struct.pack('<H',0x68EC) for i in range(64)));f.fallbacks=0
  f.reply=0;assert f.bitblt()==1 and f.fallbacks==1;cases.append(f'{bpp}bpp preflight skip forwards to DIB')
  f.reply=2;assert f.bitblt(0x005a0049)==0 and f.fallbacks==1;cases.append(f'{bpp}bpp fired XOR failure never replayed')
  f.reply=3;assert f.bitblt()==0 and f.fallbacks==1;assert u16(f.u.mem_read(0x20000+f.sym['_wEnabled'][1],2),0)==0;cases.append(f'{bpp}bpp lost engine disables GDI device')
 # v10.83: 8bpp brush from a 1-bit bitmap, opaque: a colour block of the two draw-mode indices (0 bits text colour, 1 bits background colour)
 f=GDI(8);f.gpu.prefill()
 rows8=[0x80,0x42,0x24,0x18,0xA5,0x81,0x7E,0x99]
 f.u.mem_write(0x30200,bytes(334));f.u.mem_write(0x30200,bytes([0x8c,8])+struct.pack('<H',3));f.u.mem_write(0x3020e,b''.join(bytes([r])*4 for r in rows8))
 f.u.mem_write(0x30400,struct.pack('<hhII',13,2,0x1234,0x5678))
 want8=[0x34 if rows8[y]&(0x80>>x) else 0x78 for y in range(8) for x in range(8)] # 1 bits: background colour (low byte 34), 0 bits: text colour (low byte 78)
 assert f.bitblt()==1 and f.fallbacks==0 and f.last[2]==4 and f.last[29:93]==want8;cases.append('8bpp brush from a 1-bit bitmap (opaque) -> colour pattern block of the draw-mode indices (v10.83)')
 f.u.mem_write(0x30402,struct.pack('<h',1));before=f.fallbacks;f.last=None
 assert f.bitblt()==1 and f.fallbacks==before and f.last[2]==4 and f.last[29:93]==want8;cases.append('8bpp 1-bit bitmap brush with a transparent background -> the same colour block as opaque (v10.89)')
 # v10.84 at 8bpp: a solid brush (all 64 bytes the same) and the source ROPs
 f.u.mem_write(0x30200,bytes([1,8])+struct.pack('<H',0));f.u.mem_write(0x3020e+0x40,bytes([0x5C])*64)
 f.last=None;before=f.fallbacks
 assert f.bitblt(0x00A000C9)==1 and f.fallbacks==before and f.last[2]==1 and f.last[25]==0xA0 and f.last[23]==0x5C,(f.last and f.last[2:4],f.last and f.last[23:26]);cases.append('8bpp pattern ROP with a solid brush is an engine fill')
 f.last=None
 assert f.bitblt(0x00FB0A09)==1 and f.fallbacks==before and f.last[2]==2 and f.last[3]&32 and f.last[23]==0x5C and f.last[25]==0xFB;cases.append('8bpp pattern+source ROP with a solid brush is a copy with the fixed colour')
 f.last=None
 assert f.bitblt(0x008800C6)==1 and f.fallbacks==before and f.last[2]==2 and f.last[25]==0x88 and not f.last[3]&32;cases.append('8bpp source ROP is a plain copy')
 # v10.87: DIBENG fills only the first four bytes of a solid 8bpp brush (VX2AUTO v2.5 capture: F9 F9 F9 F9 then stale bytes)
 f.u.mem_write(0x3020e+0x40,bytes([0xF9])*4+bytes([0x34,0,0x6C,0])*15);f.last=None;before=f.fallbacks
 assert f.bitblt()==1 and f.fallbacks==before and f.last[2]==1 and f.last[23]==0xF9;cases.append('8bpp solid brush: the first four brush bytes are the colour, the stale rest is ignored (v10.87)')
 f.u.mem_write(0x3020e+0x40,bytes([0xF9,0xF9,0xF8,0xF9])+bytes([0xF9])*60);f.last=None;before=f.fallbacks
 assert f.bitblt()==1 and f.fallbacks==before+1 and f.last is None;cases.append('8bpp solid brush whose first four bytes disagree takes DIB fallback')
# v10.83: ExtTextOut with an opaque rectangle: the rectangle is a fill with the background colour, the text goes to DIBENG without the opaque flag and rectangle
 for bpp in (8,16,32):
  f=GDI(bpp);f.gpu.prefill();f.u.mem_write(0x30400,struct.pack('<hhII',13,2,0x00123456,0x00654321))
  want=0x56 if bpp==8 else 0x3456 if bpp==16 else 0x123456
  before=f.fallbacks;assert f.exttextout()==1 and f.last[2]==1 and f.last[23]==want and [f.last[13],f.last[14],f.last[17],f.last[18]]==[10,8,30,22] and f.fallbacks==before+1,(bpp,f.last[2],f.last[23:25],f.last[13:19])
  assert struct.unpack('<H',f.dib_args[0:2])[0]==0x100 and struct.unpack('<H',f.dib_args[2:4])[0]==f.sym['_vx_empty_rect'][1],f.dib_args[:8];assert bytes(f.u.mem_read(0x20000+f.sym['_vx_empty_rect'][1],8))==bytes(8);cases.append(f'{bpp}bpp ExtTextOut ETO_OPAQUE: the rectangle is filled with the draw-mode background colour by the engine, DIBENG gets the text with its options unchanged and an empty rectangle (v10.85)')
  f.last=None;before=f.fallbacks
  assert f.exttextout(clip=(20,0,64,48))==1 and [f.last[13],f.last[14],f.last[17],f.last[18]]==[20,8,20,22],f.last[13:19];cases.append(f'{bpp}bpp ExtTextOut opaque rectangle is cut by the clip rectangle')
  f.last=None;before=f.fallbacks
  assert f.exttextout(opts=2)==1 and f.last[2]==1 and struct.unpack('<H',f.dib_args[2:4])[0]==f.sym['_vx_empty_rect'][1];cases.append(f'{bpp}bpp ExtTextOut with the ETO_OPAQUE bit 0002 is handled like the 0100 form GDI really sends (v10.85)')
  f.last=None;before=f.fallbacks
  assert f.exttextout(opts=0)==1 and f.last is None and f.fallbacks==before+1 and struct.unpack('<H',f.dib_args[0:2])[0]==0;cases.append(f'{bpp}bpp ExtTextOut without ETO_OPAQUE is passed through unchanged')
  f.reply=0;f.last=None;before=f.fallbacks
  assert f.exttextout()==1 and f.fallbacks==before+1 and struct.unpack('<H',f.dib_args[0:2])[0]==0x100 and f.dib_args[2:6]!=bytes(4);cases.append(f'{bpp}bpp ExtTextOut opaque rectangle declined by the engine keeps the original options for DIBENG')
 f=GDI()
 # The near Watcom binding function takes its two far pointers in registers.
 # Exercise its real instructions via a tiny 16-bit caller with separate SS/DS.
 base=0x20000+f.sym['_wEnabled'][1]
 off=f.sym['VX_accel_bind_'][1]
 caller=b'\xb8'+struct.pack('<H',0x1c)+b'\xba'+struct.pack('<H',f.sel['argument'])+b'\xbb'+struct.pack('<H',f.sym['_wEnabled'][1])+b'\xb9'+struct.pack('<H',f.sel['data'])
 # C16 register arguments are DX:AX then CX:BX, matching the compiler ABI.
 rel=(off-(0xf200+len(caller)+3))&65535
 f.u.mem_write(0x1f200,caller+b'\xe8'+struct.pack('<H',rel)+b'\x90')
 f.u.reg_write(UC_X86_REG_SP,0xd000)
 f.u.emu_start(0xf200+(f.sel['code']<<4),0x1f200+len(caller)+3,count=100000)
 assert f.binding==((f.sel['argument']<<16)|0x1c,(f.sel['data']<<16)|f.sym['_wEnabled'][1]),f.binding # v10.273: selector:offset, the VxD maps the selector's current base at each call
 cases.append('Win16 fault binding passes the device and driver words as selector:offset (the VxD maps each selector itself)')
 ptr=lambda off:struct.pack('<HH',off,f.sel['argument'])
 f.u.mem_write(0x30800,struct.pack('<H',3075))
 assert f.call('Control',ptr(0x1000)+ptr(0x800)+struct.pack('<H',8)+ptr(0),incoming_ds=f.sel['stack'])==0x100
 cases.append('QUERYESCSUPPORT advertises the DirectDraw HAL version')
 f.u.mem_write(0x30800,pack([11,0,0,0x200,0]))
 assert f.call('Control',ptr(0x1000)+ptr(0x800)+struct.pack('<H',3075)+ptr(0),incoming_ds=f.sel['stack'])==1
 block=bytes(f.u.mem_read(0x31000,328));assert block[:12]==b'VXDDRAW.DLL\0' and block[260:271]==b'DriverInit\0' and u32(block,324)==0x90000
 assert u32(f.u.mem_read(0x90000,4),0)==27492
 cases.append('DCICOMMAND allocates shared context and names the shipped callback DLL')
# v10.90: Output (Polyline, LineTo). R2_COPYPEN, R2_BLACK, R2_NOTCOPYPEN with a solid pen go to the engine one segment at a time; anything else is passed to DIBENG untouched.
 for bpp in (8,16,32):
  f=GDI(bpp);mask=(1<<bpp)-1;col={8:0xA7,16:0xF81F,32:0x123456}[bpp]
  r=f.output([(3,4),(40,30)],pen=(0,1,bpp,0,col));assert r==1 and f.nsub==1 and f.fallbacks==0 and f.last[2]==5 and f.last[13:17]==[3,4,40,30] and f.last[17]==37 and f.last[18]==1 and f.last[23]==col and f.last[25]==0xF0 and f.last[3]==0,(bpp,f.last,r)
  cases.append(f'{bpp}bpp Output: a solid R2_COPYPEN line is one engine line with the pen colour, the end point and the major length')
  r=f.output([(3,4),(40,30)],pen=(0,1,bpp,0,col),rop2=4);assert r==1 and f.last[23]==(~col)&mask and f.fallbacks==0;cases.append(f'{bpp}bpp Output: R2_NOTCOPYPEN draws the inverted pen colour')
  r=f.output([(3,4),(40,30)],pen=(0,1,bpp,0,col),rop2=1);assert r==1 and f.last[23]==0 and f.fallbacks==0;cases.append(f'{bpp}bpp Output: R2_BLACK draws colour 0')
  for r2 in (2,3,5,6,7,8,9,10,11,12,14,15,16):
   r=f.output([(3,4),(40,30)],pen=(0,1,bpp,0,col),rop2=r2);assert r==1 and f.last is None and f.nsub==0 and f.fallbacks==1,(bpp,r2)
  cases.append(f'{bpp}bpp Output: every other raster operation (XOR, NOT, AND, OR, WHITE, ...) goes to DIBENG unchanged')
  r=f.output([(3,4),(40,30),(10,44)],pen=(0,1,bpp,0,col));assert r==1 and f.nsub==2 and f.fallbacks==0 and [f.last[13:17]]==[[40,30,10,44]];cases.append(f'{bpp}bpp Output: a three-point polyline is two engine segments')
  f.reply=0;r=f.output([(3,4),(40,30),(10,44)],pen=(0,1,bpp,0,col));assert r==1 and f.nsub==2 and f.fallbacks==1 and u16(f.dib_args,20)==3 and u16(f.dib_args,16)==0x640,(f.fallbacks,f.dib_args);f.reply=None;cases.append(f'{bpp}bpp Output: segments the engine declines wait and are drawn by DIBENG as one polyline from the same points')
  r=f.output([(3,4),(40,30)],pen=(2,1,bpp,0,col));assert r==1 and f.nsub==0 and f.fallbacks==1;cases.append(f'{bpp}bpp Output: a styled pen with a background mode that is neither opaque nor transparent stays with DIBENG')
  # v10.92: segments under 8 pixels are not offered to the VxD; consecutive ones that stay with DIBENG are one polyline
  r=f.output([(3,4),(40,30),(43,32)],pen=(0,1,bpp,0,col));assert r==1 and f.nsub==1 and f.fallbacks==1 and u16(f.dib_args,20)==2 and u16(f.dib_args,16)==0x644,(f.nsub,f.fallbacks);cases.append(f'{bpp}bpp Output: a 3-pixel closing segment is not offered to the engine and goes to DIBENG as a two-point polyline from the right point')
  r=f.output([(3,4),(10,6),(50,40)],pen=(0,1,bpp,0,col));assert r==1 and f.nsub==1 and f.fallbacks==1 and u16(f.dib_args,20)==2 and u16(f.dib_args,16)==0x640 and f.last[13:17]==[10,6,50,40],(f.nsub,f.fallbacks);cases.append(f'{bpp}bpp Output: a short leading segment waits and is drawn by DIBENG, the long one goes to the engine')
  r=f.output([(3,4),(6,6),(9,9)],pen=(0,1,bpp,0,col));assert r==1 and f.nsub==0 and f.fallbacks==1 and u16(f.dib_args,20)==3;cases.append(f'{bpp}bpp Output: a polyline of short segments (an Arc or Ellipse outline) is one DIBENG call and never visits the VxD')
  # .530: styled pens (PS_DASH .. PS_DASHDOTDOT) are DIBENG's: no request to the VxD
  for style in (1,2,3,4):
   r=f.output([(2,5),(38,5)],pen=(style,1,bpp,0,col),bkmode=2,bkcolor=0x40ABCDEF);assert r==1 and f.nsub==0 and f.fallbacks==1,(bpp,style)
  cases.append(f'{bpp}bpp Output: styled pens go to DIBENG whole')
  f.reply=None
  r=f.output([(3,4),(70,30)],pen=(0,1,bpp,0,col));assert r==1 and f.nsub==0 and f.fallbacks==1;cases.append(f'{bpp}bpp Output: an end point off the screen stays with DIBENG')
  r=f.output([(3,4),(3,4)],pen=(0,1,bpp,0,col));assert r==1 and f.nsub==0 and f.fallbacks==1;cases.append(f'{bpp}bpp Output: a zero-length segment stays with DIBENG')
  r=f.output([(3,4),(40,30)],style=4,pen=(0,1,bpp,0,col));assert r==1 and f.nsub==0 and f.fallbacks==1;cases.append(f'{bpp}bpp Output: other styles (scan lines) are passed through')
  r=f.output([(3,4),(40,30)],pen=(0,1,bpp,0,col),clip=(10,9,35,28));assert r==1 and f.nsub==1 and f.last[3]==1 and f.last[19:23]==[10,9,35,28],f.last;cases.append(f'{bpp}bpp Output: the clip rectangle goes to the engine as a clip')
  r=f.output([(3,4),(40,30)],pen=(0,1,bpp,0,col),clip=(10,9,10,28));assert r==1 and f.nsub==0 and f.fallbacks==1;cases.append(f'{bpp}bpp Output: an empty clip rectangle stays with DIBENG')
  f.reply=3;r=f.output([(3,4),(40,30)],pen=(0,1,bpp,0,col));assert r==0 and u16(f.u.mem_read(0x20000+f.sym['_wEnabled'][1],2),0)==0;f.reply=None;cases.append(f'{bpp}bpp Output: a lost engine disables the GDI device and the line is not replayed')
# v10.91: DIBCAP forwarders (FastBorder, Strblt, DibToDevice, StretchDIBits, ScanLR, Pixel) and the capture records. Off: a plain forward. On: the first 20 words above the return address are kept.
 f=GDI(32);far=lambda off:struct.pack('<HH',off,f.sel['argument']);wr=lambda name,v:f.u.mem_write(0x20000+f.sym[name][1],struct.pack('<H',v));rd=lambda name:u16(f.u.mem_read(0x20000+f.sym[name][1],2),0)
 args=bytes((i*7+3)&255 for i in range(40))
 for n,tag in (('FastBorder',1),('StrBlt',2),('DIBTODEVICE_FWD',3),('STRETCHDIBITS_FWD',4),('ScanLR',5),('Pixel',6)): # v10.99: StretchDIBits itself is now a hook; its capturing forwarder is StretchDIBits_fwd
  wr('_vx_rawcap_on',0);wr('_vx_rawcap_n',0);wr('_vx_rawcap_drop',0);f.fallbacks=0;f.dib_argc=len(args);f.dib_args=None
  assert f.call(n,args,incoming_ds=f.sel['stack'])==1 and f.fallbacks==1 and f.dib_args==args and rd('_vx_rawcap_n')==0,n
  wr('_vx_rawcap_on',1);f.fallbacks=0;f.dib_args=None
  assert f.call(n,args,incoming_ds=f.sel['stack'])==1 and f.fallbacks==1 and f.dib_args==args and rd('_vx_rawcap_n')==1,n
  slot=bytes(f.u.mem_read(0x20000+f.sym['_vx_rawcap_buf'][1],42));assert u16(slot,0)==tag and slot[2:42]==args,(n,slot)
  cases.append(f'{n} is forwarded to DIBENG with its arguments intact and, when capture is on, recorded as tag {tag} with the first 20 stack words; every register is restored')
 wr('_vx_rawcap_n',0);wr('_vx_rawcap_drop',0)
 for i in range(18):f.call('FastBorder',args,incoming_ds=f.sel['stack'])
 assert rd('_vx_rawcap_n')==16 and rd('_vx_rawcap_drop')==2;cases.append('the raw capture keeps 16 slots and counts the calls it drops')
 # through the escape: arm, an Output call that goes to DIBENG, a FastBorder call, read
 wr('_vx_rawcap_on',0);wr('_vx_rawcap_n',0);wr('_vx_rawcap_drop',0)
 ptr=lambda off:struct.pack('<HH',off,f.sel['argument'])
 def esc(cmd):
  f.u.mem_write(0x30800,struct.pack('<IIII',16,0x31485856,cmd,0));f.u.mem_write(0x31000,bytes(4400))
  return f.call('Control',ptr(0x1000)+ptr(0x800)+struct.pack('<H',0x8F72)+ptr(0),incoming_ds=f.sel['stack'])
 REC=120;HDR=24
 esc(4)
 f.output([(3,4),(40,30)],pen=(2,0x11,32,0x55,0xABCDEF),rop2=7);f.dib_argc=len(args);f.call('FastBorder',args,incoming_ds=f.sel['stack'])
 sz=esc(3);reply=bytes(f.u.mem_read(0x31000,HDR+32*REC));cnt=u32(reply,12)
 assert sz==HDR+32*REC and cnt==2,(sz,cnt)
 r0=struct.unpack('<30I',reply[HDR:HDR+REC]);r1=struct.unpack('<30I',reply[HDR+REC:HDR+2*REC])
 assert r0[1]==0x5054554F and r0[3:5]==(3,4) and r0[5]==2 and r0[6]==18 and r0[7]==(2|(0x11<<16)) and r0[8]==32 and r0[10]==0 and r0[17]==(4<<16|3) and r0[12]==0x55 and r0[11]==0xABCDEF and r0[15]==7 and r0[25]==1,r0
 assert r1[1]==0x44524246 and struct.pack('<10I',*r1[7:17])==args,(r1[1],)
 cases.append('the capture escape returns an Output call that went to DIBENG (style, point count, first point, pen fields, Rop2) and the raw FastBorder arguments')
 esc(2);f.call('FastBorder',args,incoming_ds=f.sel['stack']);assert rd('_vx_rawcap_n')==0;cases.append('the capture escape stop switches the raw capture off')

 # v10.99: the StretchDIBits hook. Request fields, the conditions that keep a call with DIBENG, the return value and a lost engine.
 f=GDI(16);rdw=lambda n:u16(f.u.mem_read(0x20000+f.sym[n][1],2),0);wrw=lambda n,v:f.u.mem_write(0x20000+f.sym[n][1],struct.pack('<H',v))
 SB=0x30000+0x1100
 def got(r,**kw):
  assert r==kw['sh'] and f.fallbacks==0 and len(f.sreqs)==1,(r,f.fallbacks,len(f.sreqs));return f.sreqs[0]
 W,H=21,13;stride24=((W*3+3)//4)*4
 r=f.sdibits(24,W,H,False,(10,8,42,26),(0,0,21,13));q=got(r,sh=13)
 assert q[8:12]==[10,8,2,2] and q[12:16]==[0,0,21,13] and q[16:20]==[10,8,52,34] and q[20]==SB and q[21]==stride24 and q[22:26]==[24,13,0,stride24*13],q
 assert q[4:8]==[0,f.gpu.pitch,64,48] and q[:4]==[1136,0x10000,0,16],q[:8]
 cases.append('StretchDIBits 2x 24-bit bottom-up: the engine request carries the destination origin, factors 2 and 2, the whole source rectangle, the visible rectangle, the DIB bits linear address, stride, depth, height and byte length; the call returns the number of source scan lines and DIBENG is not called')
 r=f.sdibits(32,W,H,True,(10,8,63,39),(0,0,21,13));q=got(r,sh=13);assert q[8:12]==[10,8,3,3] and q[22]==32 and q[21]==W*4 and q[24]==1 and q[16:20]==[10,8,64,47],q
 cases.append('StretchDIBits 3x 32-bit top-down: factor 3, stride W*4, top-down flag, visible rectangle clamped to the 64x48 screen')
 r=f.sdibits(16,W,H,False,(10,8,20,14),(3,2,10,7));q=got(r,sh=7);assert q[8:12]==[10,8,2,2] and q[12:16]==[3,H-2-7,10,7] and q[22]==16 and q[21]==((W*2+3)//4)*4,q
 cases.append('StretchDIBits 555 source rectangle 3,2 10x7 2x: the source rows are counted from the bottom (start row 13-2-7 = 4), whatever the direction of the DIB')
 r=f.sdibits(24,W,H,True,(10,8,20,14),(3,2,10,7));assert r==1 and f.fallbacks==1 and f.sreqs==[],(r,f.fallbacks,len(f.sreqs));f.fallbacks=0;cases.append('StretchDIBits top-down DIB with a source rectangle (or a partial bitmap: StartScan/cLines reach the DDI as such a rectangle): stays with DIBENG, only whole top-down bitmaps are taken')
 r=f.sdibits(24,W,H,False,(10,8,20,14),(3,2,10,7),clip=(15,10,22,16));q=got(r,sh=7);assert q[16:20]==[15,10,22,16],q;cases.append('StretchDIBits: the clip rectangle narrows the visible rectangle (right and bottom exclusive)')
 r=f.sdibits(24,W,H,False,(10,8,20,14),(3,2,10,7),clip=(0,0,500,500));q=got(r,sh=7);assert q[16:20]==[10,8,30,22],q;cases.append('StretchDIBits: a clip rectangle larger than the destination changes nothing')
 r=f.sdibits(24,W,H,False,(2,2,16,10),(0,0,8,5));q=got(r,sh=5);assert q[8:12]==[2,2,2,2],q;cases.append('StretchDIBits 2x on a small source')
 r=f.sdibits(24,W,H,False,(2,2,3,10),(0,0,1,5));q=got(r,sh=5);assert q[8:12]==[2,2,3,2],q;cases.append('StretchDIBits anisotropic 3x2: the factors go through unchanged for the VxD to class')
 pal40=[(i*0x010203+0x0A0B0C)&0xFFFFFF for i in range(40)]
 f16=f;f=GDI(32);f.sreply=1 # v10.104: palette sources go to the VxD only for the 32bpp screen
 r=f.sdibits(8,W,H,False,(10,8,42,26),(0,0,21,13),clrused=40,pal=pal40);q=got(r,sh=13)
 assert q[22]==8 and q[21]==((W+3)//4)*4 and q[26]==40 and q[27:67]==pal40 and q[67:283]==[0]*216 and q[25]==q[21]*13,q[20:30]
 cases.append('StretchDIBits 8-bit source: the request carries the colour table (biClrUsed 40 entries as 0x00RRGGBB dwords, the other 216 zero), pal_n 40, stride and byte length for 8 bits a pixel')
 pal256=[(i*0x010101)&0xFFFFFF for i in range(256)]
 f.u.mem_write(0x31028,struct.pack('<256I',*pal256))
 r=f.sdibits(8,W,H,False,(10,8,42,26),(0,0,21,13),clrused=0,pal=pal256,bitsoff=0x1800);q=got(r,sh=13);assert q[26]==256 and q[27:283]==pal256,q[26];cases.append('StretchDIBits 8-bit source with biClrUsed 0: all 256 colour table entries are passed')
 r=f.sdibits(8,W,H,True,(10,8,42,26),(0,0,21,13),clrused=40,pal=pal40);q=got(r,sh=13);assert q[24]==1 and q[26]==40,q;cases.append('StretchDIBits 8-bit top-down whole bitmap is taken')
 base8=dict(bits=8,W=W,H=H,dest=(10,8,42,26),src=(0,0,21,13),clrused=40,pal=pal40)
 for nm,kw in (('a translation table (index colours)',dict(trans=True)),('biClrUsed over 256',dict(clrused=300)),('a compression code that is neither RLE8 nor 0 (BITFIELDS)',dict(compression=3)),('RLE8 without biSizeImage',dict(compression=1)),('RLE4 compression on an 8-bit bitmap',dict(compression=2,sizeimage=100)),('RLE8 top-down',dict(compression=1,sizeimage=100,topdown=True))):
  d=dict(base8);d.update(kw);r=f.sdibits(**d);assert r==1 and f.fallbacks==1 and f.sreqs==[],(nm,r,f.fallbacks,len(f.sreqs));cases.append('StretchDIBits 8-bit source with '+nm+': stays with DIBENG')
 # v10.104: 1-bit, 4-bit and RLE sources (32bpp screen)
 pal16=[(i*0x111111)&0xFFFFFF for i in range(16)]
 r=f.sdibits(4,W,H,False,(10,8,42,26),(0,0,21,13),clrused=0,pal=pal16);q=got(r,sh=13);st4=((W*4+31)//32)*4
 assert q[22]==4 and q[21]==st4 and q[25]==st4*13 and q[26]==16 and q[27:43]==pal16 and q[43:283]==[0]*240 and q[2]==0 and q[283]==0,(q[20:27],q[283]);cases.append('StretchDIBits 4-bit source at 32bpp: src_bits 4, stride in bytes of 4 bits a pixel, 16 colour table entries (biClrUsed 0 = all), the rest zero')
 r=f.sdibits(4,W,H,False,(10,8,42,26),(0,0,21,13),clrused=9,pal=pal16[:9]);q=got(r,sh=13);assert q[26]==9 and q[27:36]==pal16[:9] and q[36:283]==[0]*247;cases.append('StretchDIBits 4-bit source with biClrUsed 9: nine table entries')
 pal2=[0x102030,0xE0D0C0]
 r=f.sdibits(1,W,H,False,(10,8,42,26),(0,0,21,13),clrused=0,pal=pal2);q=got(r,sh=13);st1=((W+31)//32)*4;assert q[22]==1 and q[21]==st1 and q[25]==st1*13 and q[26]==2 and q[27:29]==pal2 and q[29:283]==[0]*254,(q[20:27]);cases.append('StretchDIBits 1-bit source at 32bpp: src_bits 1, stride 4 bytes, two table entries')
 r=f.sdibits(1,W,H,True,(10,8,42,26),(0,0,21,13),clrused=2,pal=pal2);q=got(r,sh=13);assert q[24]==1 and q[26]==2;cases.append('StretchDIBits 1-bit top-down whole bitmap is taken')
 r=f.sdibits(4,W,H,True,(10,8,20,14),(3,2,10,7),clrused=16,pal=pal16);assert r==1 and f.fallbacks==1 and f.sreqs==[];f.fallbacks=0;cases.append('StretchDIBits 4-bit top-down with a source rectangle stays with DIBENG (as every top-down sub-rectangle)')
 r=f.sdibits(8,W,H,False,(10,8,42,26),(0,0,21,13),clrused=40,pal=pal40,compression=1,sizeimage=777);q=got(r,sh=13)
 assert q[2]==4 and q[22]==8 and q[21]==W and q[283]==W and q[25]==777 and q[23]==H and q[24]==0 and q[26]==40 and q[27:67]==pal40,(q[:3],q[20:27],q[283]);cases.append('StretchDIBits RLE8 source at 32bpp: flag 4, src_stride = src_width = the width, src_bytes = biSizeImage, the colour table of the header')
 r=f.sdibits(4,W,H,False,(10,8,42,26),(0,0,21,13),clrused=16,pal=pal16,compression=2,sizeimage=333);q=got(r,sh=13);assert q[2]==4 and q[22]==4 and q[21]==W and q[283]==W and q[25]==333 and q[26]==16;cases.append('StretchDIBits RLE4 source at 32bpp: flag 4, src_bits 4, width and stream length as for RLE8')
 for nm,kw in (('4-bit with biClrUsed 17',dict(bits=4,clrused=17,pal=pal16)),('1-bit with biClrUsed 3',dict(bits=1,clrused=3,pal=pal2)),('1-bit with a translation table',dict(bits=1,clrused=2,pal=pal2,trans=True)),('4-bit RLE8 mismatch',dict(bits=4,clrused=16,pal=pal16,compression=1,sizeimage=50)),('1-bit with any compression',dict(bits=1,clrused=2,pal=pal2,compression=1,sizeimage=50)),
  ('RLE8 bigger than the decode buffer (4096 x 100)',dict(bits=8,W=4096,H=100,clrused=40,pal=pal40,compression=1,sizeimage=900,dest=(10,8,8192,200),src=(0,0,4096,100))),('RLE8 with a stream over 16 MB',dict(compression=1,sizeimage=0x1000001,clrused=40,pal=pal40,bits=8))):
  d=dict(base8);d.update(kw);r=f.sdibits(**d);assert r==1 and f.fallbacks==1 and f.sreqs==[],(nm,r,f.fallbacks,len(f.sreqs));cases.append('StretchDIBits '+nm+': stays with DIBENG')
 f=f16
 for nm,kw in (('8-bit',dict(bits=8,clrused=40,pal=pal40)),('4-bit',dict(bits=4,clrused=16,pal=pal16)),('1-bit',dict(bits=1,clrused=2,pal=pal2)),('RLE8',dict(bits=8,clrused=40,pal=pal40,compression=1,sizeimage=500)),('RLE4',dict(bits=4,clrused=16,pal=pal16,compression=2,sizeimage=500))):
  d=dict(base8);d.update(kw);r=f.sdibits(**d);assert r==1 and f.fallbacks==1 and f.sreqs==[],(nm,r,f.fallbacks,len(f.sreqs));cases.append(f'StretchDIBits {nm} source at 16bpp: not sent to the VxD (DIBENG draws palette sources as fast)')
 # conditions that keep the call with DIBENG
 def declined(name,**kw):
  r=f.sdibits(**kw);assert r==1 and f.fallbacks==1 and f.sreqs==[],(name,r,f.fallbacks,len(f.sreqs));return name
 base=dict(bits=24,W=W,H=H,dest=(10,8,42,26),src=(0,0,21,13))
 names=[declined('fGet set (read the screen into a DIB)',**{**base,'fGet':1}),declined('ROP other than SRCCOPY',**{**base,'rop':0x00EE0086}),
  declined('destination not a multiple of the source (21 -> 40)',**{**base,'dest':(10,8,40,26)}),declined('destination height not a multiple',**{**base,'dest':(10,8,42,25)}),
  declined('negative destination width (mirror)',**{**base,'dest':(10,8,-42,26)}),declined('negative destination height (mirror)',**{**base,'dest':(10,8,42,-26)}),
  declined('BI_BITFIELDS / compressed source',**{**base,'compression':3}),declined('4-bit source',**{**base,'bits':4}),declined('two planes',**{**base,'planes':2}),declined('source rectangle past the right edge',**{**base,'src':(5,0,21,13)}),
  declined('source rectangle past the top',**{**base,'src':(0,3,21,13)}),declined('empty source width',**{**base,'src':(0,0,0,13),'dest':(10,8,0,26)}),declined('no bits',**{**base,'usebits':False}),declined('no info',**{**base,'useinfo':False}),
  declined('clip leaves nothing',**{**base,'clip':(60,0,70,20)}),declined('destination entirely off the right edge',**{**base,'dest':(64,8,42,26)}),declined('not the screen device',**{**base,'sdev':0x10})]
 cases.append('StretchDIBits stays with DIBENG when: '+', '.join(names))
 for flag,why in ((0x8000,'no VRAM flag'),(0x0010,'BUSY flag')):
  old=u16(f.u.mem_read(0x3001c,2),0);f.u.mem_write(0x3001c,struct.pack('<H',0 if why=='no VRAM flag' else old|0x0010))
  declined(why,**base);f.u.mem_write(0x3001c,struct.pack('<H',old))
 cases.append('StretchDIBits stays with DIBENG when the device is not in video memory or is flagged busy')
 wrw('_wEnabled',0);declined('GDI acceleration off',**base);wrw('_wEnabled',1);r=f.sdibits(**base);assert len(f.sreqs)==1;cases.append('StretchDIBits stays with DIBENG when acceleration is disabled (wEnabled 0), and works again when it is back')
 f.sreply=0;r=f.sdibits(**base);assert r==1 and f.fallbacks==1 and len(f.sreqs)==1;cases.append('StretchDIBits: when the VxD declines (class not enabled, size, 3D busy...) the call is forwarded to DIBENG with its arguments, return value from DIBENG')
 f.sreply=3;r=f.sdibits(**base);assert r==0 and f.fallbacks==0 and rdw('_wEnabled')==0 and u16(f.u.mem_read(0x3001c,2),0)&0x0010,(r,rdw('_wEnabled'));f.sreply=1;wrw('_wEnabled',1);f.u.mem_write(0x3001c,struct.pack('<H',0x8000))
 cases.append('StretchDIBits: a lost engine answer disables the GDI device, sets BUSY, and the call is not handed to DIBENG (v10.330, review #73)')

 # v10.100: 1:1 and negative origins in the StretchDIBits hook, DibToDevice, the 32bpp screen, 565 sources.
 f=GDI(16);f.sreply=1;stride24=((21*3+3)//4)*4
 r=f.sdibits(24,21,13,False,(10,8,21,13),(0,0,21,13));q=got(r,sh=13);assert q[8:12]==[10,8,1,1] and q[12:16]==[0,0,21,13] and q[16:20]==[10,8,31,21],q
 cases.append('StretchDIBits 1:1 (a top-down bitmap reaches the driver this way) is sent to the VxD with factors 1 and 1')
 r=f.sdibits(24,21,13,True,(10,8,10,7),(3,2,10,7));assert r==1 and f.fallbacks==1 and f.sreqs==[],(r,f.fallbacks);cases.append('StretchDIBits 1:1 top-down with a source rectangle stays with DIBENG')
 r=f.sdibits(24,21,13,False,(-4,8,42,26),(0,0,21,13));q=got(r,sh=13);assert q[8]==0xFFFFFFFC and q[9]==8 and q[16:20]==[0,8,38,34],q;cases.append('StretchDIBits with a negative destination x: the signed origin goes to the VxD and the visible rectangle starts at 0')
 r=f.sdibits(24,21,13,False,(10,-6,42,26),(0,0,21,13),clip=(0,0,500,500));q=got(r,sh=13);assert q[9]==0xFFFFFFFA and q[17]==0 and q[19]==20,q;cases.append('StretchDIBits with a negative destination y likewise')
 def dgot(r,cs):
  assert r==cs and f.fallbacks==0 and len(f.sreqs)==1,(r,f.fallbacks,len(f.sreqs));return f.sreqs[0]
 r=f.dtd(24,21,13,10,8,0,None,(12,9,30,20));q=dgot(r,13)
 assert q[:4]==[1136,0x10000,0,16] and q[8:12]==[10,8,1,1] and q[12:16]==[0,0,21,13] and q[16:20]==[12,9,30,20] and q[20]==0x30000+0x1100 and q[21]==stride24 and q[22:26]==[24,13,0,stride24*13],q
 cases.append('DibToDevice (SetDIBitsToDevice and bottom-up 1:1 StretchDIBits): the whole bitmap at X,Y with the clip rectangle as the visible rectangle goes to the VxD as a 1:1 request; the call returns cScans')
 r=f.dtd(24,21,13,0xFFFD,8,0,None,(0,9,30,20));q=dgot(r,13);assert q[8]==0xFFFFFFFD and q[16:20]==[0,9,18,20],q;cases.append('DibToDevice with a negative X (a source rectangle drawn at the left edge): signed origin, visible rectangle from the clip')
 r=f.dtd(24,21,13,10,8,0,None,None);q=dgot(r,13);assert q[16:20]==[10,8,31,21],q;cases.append('DibToDevice without a clip rectangle: the picture clamped to the screen')
 r=f.dtd(16,21,13,50,40,0,None,None);q=dgot(r,13);assert q[16:20]==[50,40,64,48],q;cases.append('DibToDevice running off the screen: clamped to 64x48')
 def dkept(name,**kw):
  r=f.dtd(**kw);assert r==1 and f.fallbacks==1 and f.sreqs==[],(name,r,f.fallbacks,len(f.sreqs));return name
 names=[dkept('iScan not 0',iscan=2),dkept('cScans not the height',cscans=5),dkept('top-down bitmap',topdown=True),dkept('BITFIELDS 24-bit',compression=3),dkept('4-bit',bits=4),dkept('no bits',usebits=False),dkept('no info',useinfo=False),dkept('565 BITFIELDS into a 16bpp screen',bits=16,compression=3),dkept('clip leaves nothing',clip=(40,0,50,9))]
 cases.append('DibToDevice stays with DIBENG when: '+', '.join(names))
 f.sreply=0;r=f.dtd();assert r==1 and f.fallbacks==1 and len(f.sreqs)==1;f.sreply=1;cases.append('DibToDevice: when the VxD declines the call is forwarded to DIBENG with its arguments and DIBENG answers')
 f.sreply=3;r=f.dtd();assert r==0 and f.fallbacks==0 and rdw('_wEnabled')==0 and u16(f.u.mem_read(0x3001c,2),0)&0x0010;f.sreply=1;wrw('_wEnabled',1);f.u.mem_write(0x3001c,struct.pack('<H',0x8000));cases.append('DibToDevice: a lost engine disables the GDI device, sets BUSY and the call is not handed to DIBENG (v10.330, review #74)')
 g=GDI(32);g.sreply=1
 r=g.sdibits(24,21,13,False,(10,8,42,26),(0,0,21,13));assert r==13 and len(g.sreqs)==1 and g.sreqs[0][3]==32 and g.sreqs[0][22]==24 and g.sreqs[0][2]==0 and g.fallbacks==0;cases.append('StretchDIBits at a 32bpp desktop: the request carries bpp 32')
 r=g.sdibits(16,21,13,False,(10,8,42,26),(0,0,21,13),compression=3);q=g.sreqs[0];assert r==13 and q[2]==2 and q[3]==32 and q[22]==16;cases.append('StretchDIBits at 32bpp with a 5-6-5 BITFIELDS source: flagged as 565')
 r=g.sdibits(16,21,13,False,(10,8,42,26),(0,0,21,13),compression=3,masks=(0x7C00,0x03E0,0x001F));assert r==1 and g.fallbacks==1 and g.sreqs==[];cases.append('StretchDIBits at 32bpp with BITFIELDS masks other than 5-6-5 stays with DIBENG')
 r=g.dtd(16,21,13,10,8,0,None,(12,9,30,20),compression=3);assert r==13 and g.sreqs[0][2]==2 and g.sreqs[0][3]==32;cases.append('DibToDevice at 32bpp with a 565 bitmap goes to the VxD flagged as 565')
 # .532 (review of .530, B4): a V4 (108) or V5 (124) header carries its masks inside it at 40: those decide, not the bytes after the header
 for hs in (108,124):
  r=g.sdibits(16,21,13,False,(10,8,42,26),(0,0,21,13),compression=3,hsize=hs,tail=(0x7C00,0x03E0,0x001F));q=g.sreqs[0];assert r==13 and q[2]==2 and g.fallbacks==0,(hs,r)
  r=g.sdibits(16,21,13,False,(10,8,42,26),(0,0,21,13),compression=3,hsize=hs,masks=(0x7C00,0x03E0,0x001F),tail=(0xF800,0x07E0,0x001F));assert r==1 and g.fallbacks==1 and g.sreqs==[],(hs,r)
 r=g.sdibits(16,21,13,False,(10,8,42,26),(0,0,21,13),compression=3,hsize=44);assert r==1 and g.fallbacks==1 and g.sreqs==[];cases.append('.532: StretchDIBits with a V4 or V5 BITFIELDS header reads the masks inside the header (5-6-5 there: the VxD; the bytes after it do not count); a header size between 40 and 52 stays with DIBENG')
 for nb in (1,4):
  r=g.dtd(nb,21,13,10,8,0,None,(12,9,30,20));q=g.sreqs[0];assert r==13 and q[22]==nb and q[26]==(2 if nb==1 else 16) and q[8:12]==[10,8,1,1] and q[21]==((21*nb+31)//32)*4,(nb,r,q[20:27]);cases.append(f'DibToDevice of a {nb}-bit bitmap at 32bpp goes to the VxD 1:1 with its colour table size')
 r=f.dtd(4,21,13,10,8,0,None,(12,9,30,20));assert r==1 and f.fallbacks==1 and f.sreqs==[];cases.append('DibToDevice of a 4-bit bitmap at 16bpp stays with DIBENG')
 return cases
if __name__=='__main__':
 c=checks();(ROOT/'verification'/'FRONTEND.json').write_text(json.dumps({'passed':len(c),'cases':c,'scope':__doc__},indent=2)+'\n');print(f'{len(c)} compiled-Win16 checks passed')
