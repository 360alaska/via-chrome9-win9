#!/usr/bin/env python3
"""v10.71: the compiled VxD side of 8bpp: BIOS mode acceptance (VX_mode_layout), the DAC request (VX_dac) and the GDI palette shadow."""
import struct
from engine import GPU,ARENA,pack
W=lambda v,i:struct.unpack('<I',v[i*4:i*4+4])[0]
PHYS=0xE0000000
def info(attrs=0x99,pitch=640,x=640,y=480,bpp=8,model=4,phys=PHYS,lin=None,masks=(0,0,0,0,0,0)):
 b=bytearray(256);struct.pack_into('<H',b,0,attrs);struct.pack_into('<H',b,16,pitch);struct.pack_into('<HH',b,18,x,y);b[25]=bpp;b[27]=model
 b[31:37]=bytes(masks);struct.pack_into('<I',b,40,phys);struct.pack_into('<H',b,50,pitch if lin is None else lin);return bytes(b)
def eight_checks():
 cases=[];g=GPU(8);u=g.u
 s=list(struct.unpack('<31I',bytes(u.mem_read(g.addr('vx_status'),124))));s[10]=PHYS;u.mem_write(g.addr('vx_status'),pack(s))
 IB,OUT=ARENA+0x3000,ARENA+0x3200
 def layout(raw,version=0x200):
  u.mem_write(IB,raw);u.mem_write(OUT,pack([0xdeadbeef]));r=g.call('VX_mode_layout',(version,0x101,IB,OUT));return r,W(bytes(u.mem_read(OUT,4)),0)
 assert layout(info())==(1,640);assert layout(info(pitch=1024,x=1024,y=768))==(1,1024);assert layout(info(pitch=832,x=800,y=600))==(1,832)
 cases.append('8bpp packed-pixel modes are accepted with their BIOS pitch (VBE 2.0)')
 assert layout(info(pitch=640,lin=704),0x300)==(1,704);cases.append('8bpp uses the linear pitch under VBE 3.0')
 assert layout(info(model=6))[0]==0 and layout(info(model=3))[0]==0;cases.append('8bpp modes that are not memory model 4 are refused')
 assert layout(info(bpp=16,model=4,masks=(5,11,6,5,5,0)))[0]==0 and layout(info(bpp=16,model=6,pitch=1280,masks=(5,11,6,5,5,0)))==(1,1280);cases.append('16bpp still needs memory model 6 and the 5-6-5 layout')
 assert layout(info(pitch=600))[0]==0 and layout(info(attrs=0x19))[0]==0 and layout(info(phys=PHYS+0x1000))[0]==0 and layout(info(x=320,y=200,pitch=320))[0]==0 and layout(info(bpp=24,model=6,pitch=1920))[0]==0
 cases.append('8bpp modes with a short pitch, no linear frame buffer, another frame buffer address, a size below 640x480 or a 24bpp depth are refused')
 # DAC request
 req=lambda first,count,cols=(),cb=1040,abi=1:pack([cb,abi,first,count,*cols])+bytes(1040-16-4*len(cols))
 def dac(raw,bpp=8):
  g.dac=[];u.mem_write(ARENA+0x7000,raw);r=g.call('VX_dac',(ARENA+0x7000,));return r,list(g.dac)
 u.mem_write(g.addr('wram'),pack([ARENA+0x20000]))
 assert dac(req(0xFFFFFFFE,0))==(1,[]);cases.append('DAC probe answers yes at 8bpp and touches nothing')
 r,log=dac(req(5,2,[0xFF8040,0x102030]));assert r==1 and log==[(0x3c6,0xff),(0x3c8,5),(0x3c9,63),(0x3c9,32),(0x3c9,16),(0x3c9,4),(0x3c9,8),(0x3c9,12)],log
 cases.append('DAC write sets the PEL mask, the start index and 6-bit components in R, G, B order')
 for name,raw in (('first beyond 255',req(256,1,[0])),('count zero',req(3,0)),('count past the end',req(250,10,[0]*10)),('wrong size',req(0,1,[0],cb=1036)),('wrong ABI',req(0,1,[0],abi=2))):
  assert dac(raw)==(0,[]),name
 cases.append('DAC rejects an index past 255, an empty request, a range past the end, a wrong size and a wrong ABI before touching the hardware')
 for bpp in (16,32):
  h=GPU(bpp);h.u.mem_write(h.addr('wram'),pack([ARENA+0x20000]));h.u.mem_write(ARENA+0x7000,req(0,1,[0x112233]));h.dac=[]
  assert h.call('VX_dac',(ARENA+0x7000,))==0 and not h.dac
 cases.append('DAC request is refused at 16 and 32bpp')
 # the GDI shadow follows every palette write, whatever the mode; at the desktop depth it also reaches the hardware
 g.dac=[];g.call('FBHDA_palette_set',(7,0xA1B2C3));assert W(bytes(u.mem_read(ARENA+0x20000+28,4)),0)==0xA1B2C3
 cases.append('GDI palette writes keep the shadow copy')
 r,log=dac(req(0xFFFFFFFF,0));assert r==1 and log[:2]==[(0x3c6,0xff),(0x3c8,0)] and len(log)==770 and log[2+3*7:2+3*7+3]==[(0x3c9,(0xA1B2C3>>18)&63),(0x3c9,(0xA1B2C3>>10)&63),(0x3c9,(0xA1B2C3>>2)&63)]
 cases.append('DAC restore writes all 256 shadow colours back')

 # DAC read-back returns what was written (6-bit components at the shipped default)
 g.u.mem_write(ARENA+0x7000,req(0,3,[0x102030,0xFF8040,0x010203]));g.call('VX_dac',(ARENA+0x7000,))
 g.u.mem_write(ARENA+0x7000,pack([1040,1,0xFFFFFFFD,0])+bytes(1024));assert g.call('VX_dac_read',(ARENA+0x7000,))==1
 o=bytes(u.mem_read(ARENA+0x7000,1040));assert W(o,0)==1040 and W(o,1)==1 and W(o,2)==6 and W(o,3)==256 and W(o,4)==0x04080C and W(o,5)==0x3F2010 and W(o,6)==0 and W(o,7)==0
 cases.append('DAC read-back returns the written components and the DAC width')
 h=GPU(16);h.u.mem_write(h.addr('wram'),pack([ARENA+0x20000]));h.u.mem_write(ARENA+0x7000,bytes(1040));assert h.call('VX_dac_read',(ARENA+0x7000,))==0
 cases.append('DAC read-back is refused outside 8bpp')
 # primary-surface access hides the software cursor at 8bpp although the engine is not used there
 for bpp in (8,16,32):
  a=GPU(bpp);assert a.call('VX2D_access',(1,))==1 and a.excluded==1 and a.call('VX2D_access',(2,))==1 and a.excluded==0
 cases.append('VX2D_access excludes and restores the cursor at 8, 16 and 32bpp')
 return cases
if __name__=='__main__':
 c=eight_checks();print(len(c),'8bpp VxD checks passed')
