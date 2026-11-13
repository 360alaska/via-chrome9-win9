#!/usr/bin/env python3
"""Execute the built VxD flip module (display start calibration, flips, vertical blank) against a CRTC/BIOS model.
The model decides what the BIOS and the CRTC registers do; real VX855 behaviour is verified on hardware by VX2FLIP."""
import struct
from engine import GPU,ARENA,pack
from validate import u32
REQ,REP=ARENA+0x5200,ARENA+0x5300
(Q,F,W,R,I)=(0,1,2,3,4)
(OK,UNS,FAIL,TMO)=(0,1,2,3)
def geometry(g,w,h,bpp,pitch=None):
 px=bpp//8;pitch=pitch or w*px
 g.u.mem_write(g.hda+12,pack([w,h,bpp,pitch,0,pitch*h]))
 return pitch
def setup(bpp=32,w=1024,h=768,fast=False,**kw):
 g=GPU(bpp);geometry(g,w,h,bpp)
 for k,v in kw.items():setattr(g,k,v)
 return g
def ctl(g,cmd,arg=0,flags=0):
 g.u.mem_write(REQ,pack([16,cmd,arg,flags]));g.call('VX_flip_control',(REQ,REP))
 return list(struct.unpack('<16I',g.u.mem_read(REP,64)))
def checks():
 cases=[]
 def case(s):cases.append(s)
 for bpp in (16,32):
  px=bpp//8;stride=1024*px*768
  # --- direct calibration, flips, reset ---
  g=setup(bpp);r=ctl(g,I);assert r[1]==0 and r[0]==OK
  r=ctl(g,Q);assert (r[0],r[1],r[2],r[3],r[7])==(OK,1,1,1,0),r # .530: FLIP_CAL's BIOS calibration removed: the known unit (2-byte steps)
  assert g.crt_start()==0 and not g.bios_log;case(f'{bpp}bpp calibration takes the known unit and makes no BIOS call')
  v=g.vbn;r=ctl(g,F,stride);assert r[0]==OK and r[4]==stride and g.crt_start()==stride>>1 and g.vbn>v;case(f'{bpp}bpp flip programs CR0C/0D/34 and waits for the blank')
  assert not g.bios_log;case(f'{bpp}bpp direct flips make no BIOS call')
  v=g.vbn;r=ctl(g,F,0,8);assert r[0]==OK and g.crt_start()==0 and g.vbn>v;case(f'{bpp}bpp .555: a NOVSYNC flip is the one-frame flip (the chip takes the address at the blank: the old front is guarded until then)')
  r=ctl(g,F,stride+4,0);assert r[0]==OK and g.crt_start()==(stride+4)>>1;case(f'{bpp}bpp dword aligned offsets are accepted')
  for bad in (stride+1,0x1000000,0xffffff00):
   before=g.crt_start();r=ctl(g,F,bad);assert r[0]==UNS and g.crt_start()==before,bad
  case(f'{bpp}bpp misaligned or out-of-memory flip targets are refused without touching the CRTC')
  r=ctl(g,F,stride,(3<<24));assert r[0]==OK;case(f'{bpp}bpp flip interval 3 waits for three blanks')
  r=ctl(g,R);assert r[0]==OK and r[4]==0 and g.crt_start()==0;case(f'{bpp}bpp reset returns to the desktop surface')
  assert g.seq[0x10]==0xa0 and g.cri==0x0a and g.sqi==0x17;case(f'{bpp}bpp ports and SR10 are restored')
  # --- v10.62 default: no BIOS calibration, known unit (2-byte steps verified on hardware) ---
  g=setup(bpp,fast=True);r=ctl(g,Q);assert (r[0],r[1],r[2],r[3],r[7])==(OK,1,1,1,0),r;assert not g.bios_log and g.crt_start()==0 and r[11]==0;case(f'{bpp}bpp default calibration makes no BIOS call and uses the verified 2-byte unit')
  r=ctl(g,F,stride);assert r[0]==OK and g.crt_start()==stride>>1 and not g.bios_log;case(f'{bpp}bpp default flip writes offset>>1 and still makes no BIOS call')
  r=ctl(g,F,stride+1);assert r[0]==UNS;case(f'{bpp}bpp default unit refuses odd byte offsets')
  r=ctl(g,R);assert r[0]==OK and g.crt_start()==0 and not g.bios_log;case(f'{bpp}bpp default reset returns to the desktop without the BIOS')
  # --- v10.63: CR34 fires the address, so it is written last, and zero is committed even when the registers already read 0 ---
  g=setup(bpp,fast=True);ctl(g,Q);g.crlog=[];r=ctl(g,F,stride);assert r[0]==OK and g.crlog[-4:]==[0x0d,0x0c,0x48,0x34],g.crlog;case(f'{bpp}bpp the start address is written CR0D, CR0C, CR48 and CR34 last')
  g=setup(bpp,fast=True);g.commit=49152;assert g.crt_start()==49152 and g.regs_start()==0;r=ctl(g,Q);assert r[0]==OK and g.crt_start()==0;case(f'{bpp}bpp calibration commits zero even when the registers already read 0')
  g=setup(bpp,fast=True);ctl(g,Q);g.commit=49152;r=ctl(g,R);assert r[0]==OK and g.crt_start()==0;case(f'{bpp}bpp reset commits zero even when no flip was recorded')
  # --- .577: the refresh rate for DirectDraw's monitor frequency: the rate the mode was set with, else measured on the blank and taken to a usual rate ---
  g=setup(bpp,fast=True);ctl(g,Q);hz=g.call('VX_flip_hz',());assert hz in (0,50,56,60,65,70,72,75,85,90,100,120),hz
  g.u.mem_write(g.addr('vesa_set_freq'),pack([75]));assert g.call('VX_flip_hz',())==75;g.u.mem_write(g.addr('vesa_set_freq'),pack([0]))
  g=setup(bpp,vb_enabled=False);ctl(g,Q);assert g.call('VX_flip_hz',())==0
  case(f'{bpp}bpp .577: the refresh rate: the rate the mode was set with; with none, the blank measured (or 0 with no blank signal)')
  # --- v10.384: FLIP_ASYNC (default 1): the flip returns once the address is written; the old front is waited for when drawn into ---
  g=setup(bpp,fast=True);ctl(g,Q);g.vbn=0
  r=ctl(g,F,stride);assert r[0]==OK and g.crt_start()==stride>>1 and g.vbn<50,(r,g.vbn) # no wait for the blank to begin
  old=g.word(g.addr('vx_flip_old_front'));assert old==0,hex(old)
  v=g.vbn;r=ctl(g,6,stride);assert r[8]==1 and g.vbn-v<50,(r,g.vbn-v) # the new front: no wait, still pending
  v=g.vbn;r=ctl(g,6,0);assert r[8]==0 and g.vbn-v>100 and g.word(g.addr('vx_flip_old_front'))==0xFFFFFFFF,(r,g.vbn-v) # the old front: waited (until the blank or a frame's time)
  case(f'{bpp}bpp asynchronous flip: returns without the blank wait; the old front waits for the blank, the new one does not')
  # After a flip away from a nonzero offset, distinguish old-front waits from status queries.
  a=setup(bpp);ctl(a,Q);ctl(a,F,stride);ctl(a,6,0);a.vbn=0;ctl(a,F,0)
  for cb,arg,expected,pending in ((12,stride,FAIL,0),(16,0xffffffff,OK,1),(16,0,OK,1),(16,stride,OK,0)):
   a.u.mem_write(REQ,pack([cb,6,arg,0]));a.call('VX_flip_control',(REQ,REQ))
   result=list(struct.unpack('<16I',a.u.mem_read(REQ,64)))
   assert (result[0],result[8])==(expected,pending),(cb,arg,result)
  assert a.word(a.addr('vx_flip_old_front'))==0xFFFFFFFF
  case(f'{bpp}bpp in-place VXF_FRONT preserves its request and rejects the wrong header size')
  r=ctl(g,F,0);assert r[0]==OK and g.word(g.addr('vx_flip_old_front'))==stride;v=g.vbn;r=ctl(g,F,stride);assert r[0]==OK and g.vbn>v and g.word(g.addr('vx_flip_old_front'))==0
  case(f'{bpp}bpp asynchronous flip: the next flip waits until the one before is taken')
  st=[g.word(g.addr('vx_frame_stat')+4*k) for k in range(16)];assert st[0]==3 and st[14]==3,st
  case(f'{bpp}bpp the frame counters count the flips and the ones that returned before the blank')
  g=setup(bpp,fast=True);g.u.mem_write(g.addr('vx_flip_async'),pack([0]));ctl(g,Q);v=g.vbn;r=ctl(g,F,stride);assert r[0]==OK and g.word(g.addr('vx_flip_old_front'))==0xFFFFFFFF and g.vbn%g.vb_period>=g.vb_period*9//10
  case(f'{bpp}bpp FLIP_ASYNC=0: the flip waits for the blank to begin, as before')
  # --- vertical blank service ---
  r=ctl(g,W,1);assert r[0]==OK;assert g.vbn%g.vb_period>=g.vb_period*9//10;r=ctl(g,W,0);assert r[8]==1
  ctl(g,W,2);assert g.vbn%g.vb_period<g.vb_period*9//10 and ctl(g,W,0)[8]==0;case(f'{bpp}bpp WaitForVerticalBlank begin/end/test semantics')
  assert 1<=r[12]<=200;case(f'{bpp}bpp frame time is measured')
  # --- no vertical blank ---
  g=setup(bpp,vb_enabled=False);r=ctl(g,Q);assert r[1]==1 and r[3]==2;v=g.vbn;assert ctl(g,F,stride)[0]==OK;assert ctl(g,W,1)[0]==UNS;case(f'{bpp}bpp without a blank signal flips still work and WaitForVerticalBlank is declined')
  # --- state handling ---
  g=setup(bpp);ctl(g,Q);ctl(g,F,stride);geometry(g,800,600,bpp);r=ctl(g,I);assert r[1]==1 # untouched until used
  r=ctl(g,Q);assert r[1]==1 and r[4]==0 and g.crt_start()==0;case(f'{bpp}bpp a mode change discards the calibration and the flipped state')
  g=setup(bpp);g.crt[0x0c]=0x10;r=ctl(g,Q);assert r[1]==1 and g.crt_start()==0;case(f'{bpp}bpp calibration first returns a shifted display to the top')
  g=setup(bpp);g.u.mem_write(g.hda+28,pack([4096]));r=ctl(g,Q);assert r[1]==3 and r[7]==2 and not g.bios_log;g.u.mem_write(g.hda+28,pack([0]));r=ctl(g,Q);assert r[1]==1;case(f'{bpp}bpp a busy desktop surface defers calibration without touching the BIOS')
  g=setup(bpp);geometry(g,1024,768,bpp,pitch=1024*px+64);r=ctl(g,Q);assert r[1]==3 and r[7]==1 and not g.bios_log;case(f'{bpp}bpp padded scanout pitch is refused (back buffers could not match)')
  g=setup(bpp);g.u.mem_write(g.hda+20,pack([24]));r=ctl(g,Q);assert r[1]==3 and r[7]==1;case('24bpp is refused')
  g=setup(bpp);g.u.mem_write(g.addr('vx_flip_enable'),pack([0]));r=ctl(g,Q);assert r[1]==3 and r[7]==7 and not g.bios_log and r[3]==1;case(f'{bpp}bpp FLIP=0 disables flipping without touching the BIOS (vertical blank is still reported)')
  g=setup(bpp);r=ctl(g,99);assert r[0]==UNS;g.u.mem_write(REQ,pack([12,0,0,0]));g.call('VX_flip_control',(REQ,REP));assert u32(g.u.mem_read(REP,4),0)==FAIL;case(f'{bpp}bpp bad command and bad request size')
 return cases
if __name__=='__main__':
 c=checks();print(len(c),'flip checks passed')
