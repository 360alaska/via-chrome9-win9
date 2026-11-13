#!/usr/bin/env python3
""".579: the device profile (vx85598.c VX_check_hardware) in the compiled VxD objects, with a modelled PCI configuration space and video
ROM: the three devices, the BARs each uses (VX855 10h / 14h; VX800 10h and 14h, or 18h behind a 64-bit frame-buffer BAR; VX900 the
frame buffer at 18h, the registers at 14h), what is refused (another device, a ROM of another device, a BAR above 4 GB, an I/O BAR,
no register BAR), and that the engines are used on the VX800 and the VX855 and not on the VX900."""
import struct,sys,pathlib
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parent.parent/'verification'))
import engine
from engine import GPU,pack
ROMAT=0x180000
class PCI(GPU):
 def __init__(self,dev,cfg,romdev=None,second_vga=False):
  self.cfg=cfg;self.dev=dev;self.cf8=0;self.second=second_vga
  GPU.__init__(self,32)
  rom=bytearray(0x200);rom[0]=0x55;rom[1]=0xAA;rom[2]=1;rom[0x18]=0x40;rom[0x40:0x44]=b'PCIR';rom[0x44]=0x06;rom[0x45]=0x11
  rd=dev if romdev is None else romdev;rom[0x46]=rd&255;rom[0x47]=rd>>8;rom[0x54]=0
  rom[0x1FF]=(-sum(rom))&255;self.u.mem_write(ROMAT,bytes(rom))
  self.hook('__MapPhysToLinear',self.mapping2)
  self.u.mem_write(self.addr('vx_chip_id'),pack([0]))
 def mapping2(self):
  return ROMAT if self.arg(0)==0xC0000 else engine.MM
 def conf(self,a):
  bus=(a>>16)&255;d=(a>>11)&31;f=(a>>8)&7;off=a&0xFC
  if bus==0 and d==1 and f==0:
   base={0:0x1106|(self.dev<<16),4:0x00000007,8:0x03000000,0x0C:0}
   base.update(self.cfg);return base.get(off,0)
  if self.second and bus==0 and d==2 and f==0:return {0:0x12345678,4:1,8:0x03000000}.get(off,0)
  return 0xFFFFFFFF
 def port_in(self,u,p,z,_):
  if p==0xCF8:return self.cf8
  if p==0xCFC:return self.conf(self.cf8)
  return GPU.port_in(self,u,p,z,_)
 def port_out(self,u,p,z,v,_):
  if p==0xCF8:self.cf8=v;return
  return GPU.port_out(self,u,p,z,v,_)
def run(dev,cfg,**kw):
 g=PCI(dev,cfg,**kw);rc=g.call('VX_check_hardware',())
 st=struct.unpack('<10I',g.u.mem_read(g.addr('vx_status'),40))
 w=lambda n:g.word(g.addr(n))
 return rc,st[4],st[7],st[8],w('vx_chip_id'),w('vx_chip_fb_cfg'),w('vx_chip_mmio_cfg'),g.call('VX_chip_accel',()),g.call('VX_chip_video',())
def main():
 fails=0
 cases=[
  ('VX855: frame buffer 10h, registers 14h',0x5122,{0x10:0xD0000008,0x14:0xFB000000},{}, (1,0,0xD0000000,0xFB000000,0x5122,0x10,0x14,1,1)),
  ('VX800, 32-bit BARs: 10h and 14h',0x1122,{0x10:0xD0000008,0x14:0xFA000000},{}, (1,0,0xD0000000,0xFA000000,0x1122,0x10,0x14,1,1)),
  ('VX800, a 64-bit frame-buffer BAR: 10h (and 14h its upper half), registers 18h',0x1122,{0x10:0xD000000C,0x14:0,0x18:0xFA000004,0x1C:0},{}, (1,0,0xD0000000,0xFA000000,0x1122,0x10,0x18,1,1)),
  ('VX900: registers 14h, frame buffer 18h (64-bit), the engines not used',0x7122,{0x10:0xF8000000,0x14:0xFB000000,0x18:0xC000000C,0x1C:0},{}, (1,0,0xC0000000,0xFB000000,0x7122,0x18,0x14,0,0)),
  ('VX800: a frame buffer above 4 GB is refused',0x1122,{0x10:0xD000000C,0x14:1,0x18:0xFA000004},{}, (0,3)),
  ('VX855: an I/O BAR where the registers should be is refused',0x5122,{0x10:0xD0000008,0x14:0x0000E001},{}, (0,3)),
  ('VX900: no frame-buffer BAR assigned is refused',0x7122,{0x14:0xFB000000,0x18:0},{}, (0,3)),
  ('another VIA display device is not taken',0x3371,{0x10:0xD0000008,0x14:0xFB000000},{}, (0,1)),
  ('VX855: a video ROM of another device is refused',0x5122,{0x10:0xD0000008,0x14:0xFB000000},{'romdev':0x1122}, (0,4)),
  ('VX800: a VIA video ROM with another device number is taken',0x1122,{0x10:0xD0000008,0x14:0xFA000000},{'romdev':0x3157}, (1,0,0xD0000000,0xFA000000,0x1122,0x10,0x14,1,1)),
  ('two active display devices are refused',0x5122,{0x10:0xD0000008,0x14:0xFB000000},{'second_vga':True}, (0,2)),
 ]
 for name,dev,cfg,kw,want in cases:
  got=run(dev,cfg,**kw);ok=got[:len(want)]==want
  print('PASS' if ok else 'FAIL',name,'' if ok else (tuple(hex(x) for x in got),want))
  fails+=not ok
 print('chip_profile: %d cases, %d failures'%(len(cases),fails));sys.exit(1 if fails else 0)
if __name__=='__main__':main()
