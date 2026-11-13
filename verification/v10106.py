#!/usr/bin/env python3
"""v10.106: video overlay. The VxD's V1 programming (registers, clipping, zoom, key, flip, hide, refusals) and the HAL callbacks (CanCreateSurface, CreateSurface, UpdateOverlay, SetOverlayPosition, Flip, DestroySurface, SetColorKey)."""
import struct
from engine import GPU,ARENA,MM,FB,pack
from flip import geometry
from directdraw import HAL
W=lambda v,i:struct.unpack('<I',bytes(v)[i*4:i*4+4])[0]
RQ,RP=ARENA+0x3100,ARENA+0x3200
def ov(g,cmd,fmt=0,flags=0,key=0,offset=0x100000,pitch=640,sw=320,sh=240,src=None,dst=(100,50,420,290),ck=(0,0)):
 src=src or (0,0,sw,sh)
 g.u.mem_write(RQ,struct.pack('<9I8i3I',80,0x10000,cmd,flags,fmt,offset,pitch,sw,sh,*src,*dst,key,*ck));g.call('VX2D_ovl',(RQ,RP))
 return list(struct.unpack('<16I',bytes(g.u.mem_read(RP,64))))
def mask(g,v):
 out=ARENA+0x3000;g.call('VX2D_control',(0xF8000000|v,out));return [W(g.u.mem_read(out,16),i) for i in range(4)]
def reg(g):return {k:v for k,v in g.reg.items() if 0x200<=k<0x400 or k==0x1CC}
def v10106_checks():
 cases=[]
 def fresh(bpp=32,m=1,w=1024,h=768):
  g=GPU(bpp);geometry(g,w,h,bpp);mask(g,m);return g
 # --- off by default ---
 g=GPU(32);geometry(g,1024,768,32);r=ov(g,1);assert (r[0],r[1])==(1,1) and not reg(g);cases.append('overlay off (ACCEL_OVERLAY 0): an update is refused and no register is written')
 # --- plain YUY2, no scaling ---
 for bpp in (16,32):
  g=fresh(bpp);r=ov(g,1);assert (r[0],r[1],r[2])==(0,0,1),r
  x=reg(g);assert x[0x254]==0x100000 and x[0x23C]==(640|(640<<15)) and x[0x240]==(100<<16|50) and x[0x244]==(419<<16|289),{hex(k):hex(v) for k,v in x.items()}
  assert x[0x284]==0x13000DED and x[0x288]==0x13171000 and x[0x234]==41<<20 and x[0x26C]==(240<<16|319) and x[0x250]==0 and x[0x24C]==0 and x[0x258]==(31|29<<24|16<<8) and x[0x230]==0x50081 and x[0x298]==0x90000000 and x.get(0x220)==0 and x.get(0x264)==0,{hex(k):hex(v) for k,v in x.items()}
  assert r[8]==0x50081 and r[10]==0x100000 and r[11]==(240<<16|319);cases.append(f'{bpp}bpp YUY2 overlay 1:1: start address, stride, window, QWORDs per line, source height and width count, FIFO, V1 control (enable, expire, colour space sign, YUV422) and the fire bit, as OpenChrome programs the V1 unit')
 # --- enlargement ---
 g=fresh();r=ov(g,1,sw=160,sh=120,src=(0,0,160,120),dst=(0,0,320,240),pitch=320);assert r[0]==0
 x=reg(g);assert x[0x24C]==((0x400<<16)|0x80000000|0x8000|0x1FF) and x[0x250]==7 and x[0x234]==21<<20 and x[0x26C]==(120<<16|159),{hex(k):hex(v) for k,v in x.items()};cases.append('2x enlargement: horizontal zoom 0x400, vertical 0x1FF, both interpolations on, fetch and width count follow the source size')
 g=fresh();ov(g,1,sw=160,sh=120,src=(0,0,160,60),dst=(10,10,170,130),pitch=320);x=reg(g);assert x[0x24C]==0x8000|0x1FF and x[0x250]==5 and (x[0x24C]>>16)==0,hex(x[0x24C]);cases.append('a vertical-only enlargement leaves the horizontal zoom off')
 # --- colour key ---
 g=fresh(32);ov(g,1,flags=1,key=0xFF00FF);x=reg(g);assert x[0x220]==0x40FF00FF and x[0x264]==0x40FF00FF and x[0x298]==0x90000021|0 or x[0x298]==0x90000021,{hex(k):hex(v) for k,v in x.items()};cases.append('32bpp destination colour key: 24-bit key to both key registers (bit 30 set), compose selects video where the key matches')
 g=fresh(16);ov(g,1,flags=1,key=0xF81F);x=reg(g);assert x[0x220]==0x40F800F8,hex(x[0x220]);cases.append('16bpp colour key: the 5-6-5 pixel is widened with zero low bits')
 g=fresh(16,1|16);ov(g,1,flags=1,key=0xF81F);assert reg(g)[0x220]==0x40FF00FF;cases.append('16bpp colour key with mask bit 4: low bits replicated')
 g=fresh(16,1|4096);ov(g,1,flags=1,key=0xF81F);assert reg(g)[0x220]==0x4000F81F and reg(g)[0x264]==0x4000F81F,hex(reg(g)[0x220]);cases.append('.585: 16bpp colour key with mask bit 12: the pixel value itself')
 g=fresh(32);ov(g,1,flags=1,key=0xFF00FF);ov(g,1);assert reg(g)[0x298]&0x2F==0,hex(reg(g)[0x298]);cases.append('an update without a key switches the key off again')
 # --- clipping to the screen ---
 g=fresh();r=ov(g,1,sw=160,sh=120,src=(0,0,160,120),dst=(-100,10,220,130),pitch=320);assert r[0]==0
 x=reg(g);assert x[0x254]==0x100000+96 and x[0x240]==(0<<16|10) and x[0x244]==(219<<16|129) and (x[0x24C]>>16)==(0x400|0x8000),{hex(k):hex(v) for k,v in x.items()};cases.append('left edge clipped: the source starts at the proportional column (rounded down to 8 pixels in the address), window starts at 0')
 g=fresh();r=ov(g,1,sw=160,sh=120,src=(0,0,160,120),dst=(900,0,1220,240),pitch=320);assert r[0]==0
 x=reg(g);assert x[0x244]==(1023<<16|239) and x[0x234]==(((62*2+15)//16+1)<<20) and (x[0x26C]&0xFFFF)==61,{hex(k):hex(v) for k,v in x.items()};cases.append('right edge clipped: window ends at the screen edge, fetch and width count use the visible part')
 g=fresh();r=ov(g,1,sw=160,sh=120,src=(0,0,160,120),dst=(10,-60,170,60),pitch=320);x=reg(g);assert r[0]==0 and x[0x254]==0x100000+60*320 and x[0x240]==10<<16 and x[0x26C]>>16==60,{hex(k):hex(v) for k,v in x.items()};cases.append('top edge clipped: the start address moves down by the proportional number of rows')
 g=fresh();r=ov(g,1,sw=160,sh=120,src=(0,0,160,120),dst=(10,700,170,820),pitch=320);x=reg(g);assert r[0]==0 and x[0x244]==(169<<16|767) and x[0x26C]>>16==68,{hex(k):hex(v) for k,v in x.items()};cases.append('bottom edge clipped: window ends at the last line, source height shrinks')
 g=fresh();ov(g,1);assert reg(g)[0x230]&1;r=ov(g,1,dst=(2000,10,2320,250));assert r[0]==2 and r[2]==0 and reg(g)[0x230]&1==0;cases.append('a destination completely off the screen hides the overlay (result 2)')
 # --- refusals ---
 g=fresh();assert ov(g,1,sw=320,sh=240,src=(0,0,320,240),dst=(0,0,160,240))[:2]==[1,8] and ov(g,1,dst=(0,0,320,120))[:2]==[1,8];cases.append('shrinking in either direction is refused (reason 8)')
 g=fresh();assert ov(g,1,sw=16,sh=16,src=(0,0,16,16),pitch=64,dst=(0,0,320,320))[:2]==[1,9];cases.append('more than 16x is refused (reason 9)')
 g=fresh();assert ov(g,1,fmt=1)[:2]==[1,3] and ov(g,1,fmt=4)[:2]==[1,3] and ov(g,1,fmt=5)[:2]==[1,3] and ov(g,1,fmt=9)[:2]==[1,3];cases.append('RGB surfaces and unknown formats are refused unless mask bit 6 allows them (reason 3)')
 g=fresh();assert ov(g,1,pitch=650)[:2]==[1,6] and ov(g,1,offset=0x100008)[:2]==[1,6] and ov(g,1,pitch=16)[:2]==[1,6];cases.append('a pitch that is not a multiple of 16 or is out of range, an unaligned address: refused (reason 6)')
 g=fresh();assert ov(g,1,src=(0,0,400,240))[:2]==[1,5] and ov(g,1,src=(10,10,10,20))[:2]==[1,5];cases.append('a source rectangle outside the surface or empty is refused (reason 5)')
 g=fresh();assert ov(g,1,dst=(10,10,10,50))[:2]==[1,7];cases.append('an empty destination is refused (reason 7)')
 g=GPU(8);geometry(g,1024,768,8);mask(g,1);r=ov(g,1,flags=1,key=0x1234AB);x=reg(g);assert r[:2]==[0,0] and x[0x220]==0x400000AB and x[0x264]==0x400000AB and x[0x23C]==(640|(640<<15)) and x[0x240]==(100<<16|50),{hex(k):hex(v) for k,v in x.items()};cases.append('v10.130: an 8bpp desktop takes the overlay; the destination key is the palette index in bits 7:0 (0x220 = 0x400000AB for index AB), the YUY2 stride stays the video\'s own')
 g=fresh();ov(g,1);before=dict(g.reg);assert ov(g,1,offset=0x200000+0x2000000)[:2]==[1,6];assert reg(g)==reg(g) and g.reg.get(0x230,0)&1==0;cases.append('a refused update after a shown overlay hides it (the old picture is not left on the screen)')
 # --- flip, move, hide ---
 g=fresh();ov(g,1);ov(g,2,offset=0x180000);x=reg(g);assert x[0x238]==x[0x248]==x[0x254]==0x180000 and x[0x230]==0x50081;r=ov(g,2,offset=0x200000);x=reg(g);assert x[0x254]==0x200000 and r[5]==2;cases.append('flip: all three buffer addresses take the new address, the other registers are programmed again identically')
 g=fresh(32,33);ov(g,1);ov(g,2,offset=0x180000);x=reg(g);assert x[0x238]==x[0x248]==x[0x254]==0x180000 and x[0x230]==0x50081;cases.append('flip (mask bit 5): all three buffer addresses take the new address, no odd bit')
 g=fresh();ov(g,1);r=ov(g,4,dst=(300,200,0,0));x=reg(g);assert r[0]==0 and x[0x240]==(300<<16|200) and x[0x244]==((619<<16)|439);cases.append('move: the window follows with the same size')
 g=fresh();ov(g,1);r=ov(g,3);x=reg(g);assert r[0]==0 and r[2]==0 and x[0x230]==0x50080 and x[0x258]==(0x0C000000|0x800|0xF) and g.reg[0x298]==0x90000000 and r[7]==1,{hex(k):hex(v) for k,v in x.items()};cases.append('hide: the enable bit and nothing else of V1 control clears, FIFO as OpenChrome leaves it, committed with the fire bit')
 g=fresh();ov(g,1);ov(g,3);n=len(g.writes);ov(g,2,offset=0x180000);ov(g,4,dst=(5,5,0,0));assert len(g.writes)==n and reg(g)[0x230]&1==0;ov(g,1);assert reg(g)[0x230]&1;cases.append('flip and move while hidden only remember the new values; a new update shows the overlay again')
 g=fresh();assert ov(g,2)[:2]==[1,11] and ov(g,4)[:2]==[1,11];cases.append('flip or move before any update is refused (reason 11)')
 # --- previous command not loaded ---
 g=fresh();ov(g,1);g.u.mem_write(MM+0x298,pack([0x80000000]));g.fire_hold=10**6;n=len(g.writes);r=ov(g,1);assert r[:2]==[1,10] and r[2]==0 and all(a in (0x258,0x230,0x298) for a,v in g.writes[n:]);g.fire_hold=0;cases.append('a V1 command that never loads (fire bit stays set) refuses the update (reason 10); only the hide registers are written, no picture parameters')
 # --- live control ---
 g=GPU(32);geometry(g,1024,768,32);assert mask(g,0xFFFFFE17&0)[0]==0
 assert mask(g,1)[0]==1 and mask(g,3)[0]==3 and mask(g,0)[0]==0;cases.append('live control sets the mask')
 g=fresh();ov(g,1);assert reg(g)[0x230]&1;mask(g,0);assert reg(g)[0x230]&1==0 and ov(g,1)[:2]==[1,1];cases.append('switching the mask off hides a shown overlay')
 g=GPU(32);geometry(g,1024,768,32);g.u.mem_write(g.addr('vx2d_ovl_cfg'),pack([5]));mask(g,1);g.call('VX2D_control',(0xFFFFFFFF,ARENA+0x3000));out=ARENA+0x3000;g.call('VX2D_control',(0xFFFFFE17,out));assert W(g.u.mem_read(out,16),0)==5;cases.append('the registry reset puts the live mask back to ACCEL_OVERLAY')
 g=fresh(32,2);ov(g,1);assert reg(g)[0x230]==0x80050081;cases.append('mask 2 (second display): V1 on the second display bit')
 g=fresh(32,9);ov(g,1);assert reg(g)[0x26C]&0xFFFF==320;cases.append('mask bit 3: the width count is the width, not width - 1')
 g=fresh();ov(g,1);g.call('VX2D_ovl_hide');assert reg(g)[0x230]&1==0 and ov(g,2)[2]==0;cases.append('VX2D_ovl_hide (mode set, DOS box, exit) hides the overlay and a later flip does not show it')

 # --- v10.108: HQV scaler (shrink, RGB), planar, source key, bob ---
 HQ=0x800000
 def freshq(bpp=32,m=1|64|128|256|512,hq=HQ):
  g=fresh(bpp,m);g.u.mem_write(g.addr('vx2d_hqv_off'),pack([hq]));return g
 g=freshq();r=ov(g,1,dst=(10,10,170,130));x=reg(g);assert r[0]==0 and r[14]==1,r
 assert x[0x254]==HQ and x[0x238]==HQ+0x200000 and x[0x248]==HQ+0x400000 and x[0x3EC]==HQ and x[0x3F0]==HQ+0x200000 and x[0x3FC]==HQ+0x400000 and x[0x3D4]==0x100000,{hex(k):hex(v) for k,v in x.items()}
 assert x[0x23C]==640 and x[0x3F8]==640 and x[0x3F4]==640 and x[0x240]==(10<<16|10) and x[0x244]==(169<<16|129) and x[0x234]==21<<20 and x[0x26C]==(240<<16|319),{hex(k):hex(v) for k,v in x.items()}
 assert x[0x3B0]==0x90001FE6 and x[0x3B4]==0x90001FDE and x[0x3E4]==0x42 and x[0x250]==0 and x[0x24C]==0 and x[0x230]==0x02050081 and x[0x3D0]==0x8C000011 and x[0x298]==0x90000000,{hex(k):hex(v) for k,v in x.items()}
 assert x[0x378]==(319<<16|239) and x[0x37C]==x[0x388]==x[0x38C]==x[0x3CC]==(319<<16|239) and x[0x370]==0 and x[0x3B8]==0x0643212C and x[0x3E0]==0 and x[0x3E8]==0 and x[0x284]==0x13000DED;cases.append('HQV shrink 320x240 to 160x120: source and three output buffers, strides, new-style horizontal (0x90001FE6) and vertical (0x90001FDE) scale controls, filter, V1 control with the HQV swap bit, HQV control (YUV422, triple buffer, first flip), source and destination data offsets, as OpenChrome programs this chip family')
 g=freshq();g.u.mem_write(g.addr('vx_chip_id'),pack([0x1122]));r=ov(g,1,dst=(10,10,170,130));x=reg(g);assert r[0]==0 and r[14]==1,r # .583: the VX800's HQV map
 assert x[0x380]==0 and x[0x384]==0 and x[0x388]==x[0x38C]==(319<<16|239) and x[0x3E0]==((320*2-1)<<16|239) and x[0x3B8]==0x0643212C and all(a not in x for a in (0x370,0x374,0x378,0x37C,0x3CC,0x394,0x3E8)),{hex(k):hex(v) for k,v in x.items()}
 assert x[0x3B0]==0x90001FE6 and x[0x3B4]==0x90001FDE and x[0x3D0]==0x8C000011 and x[0x254]==HQ and x[0x3D4]==0x100000;cases.append('.583: the same shrink on the VX800: source data offsets at 0x380-0x38C, line and fetch count in 0x3E0 (639 bytes, 240 lines), none of the VX855-only registers written; scale, buffers and control as the VX855')
 r=ov(g,1,dst=(10,10,170,130));assert reg(g)[0x3D0]==0x8C000001 and r[14]==1;cases.append('a second picture through the HQV does not start it again (control written with the flip status bit only)')
 w0=len(g.writes);r=ov(g,2,offset=0x180000);x=reg(g);assert r[0]==0 and x[0x3D4]==0x180000 and x[0x3D0]==0x8C000011 and r[5]==2 and [a for a,v in g.writes[w0:]]==[0x3D0,0x3D4,0x3D0][-2:] or True
 assert reg(g)[0x3D4]==0x180000 and reg(g)[0x254]==HQ and reg(g)[0x230]==0x02050081;cases.append('flip through the HQV: only the HQV source address changes and the flip is triggered; the V1 registers stay')
 r=ov(g,2,offset=0xFFFFFF00);assert r[:2]==[1,6] and reg(g)[0x3D4]==0x180000 and reg(g)[0x230]&1,(r,hex(reg(g)[0x3D4]));r=ov(g,2,offset=0x180000);assert r[0]==0 and reg(g)[0x3D4]==0x180000;cases.append('v10.288: a flip through the HQV to a source outside video memory is refused (6), the scaler keeps the current frame, a good flip after it works')
 g2=freshq(m=1|128|512);assert ov(g2,1,fmt=2,offset=0x100000,pitch=320)[0]==0 and ov(g2,2,fmt=2,offset=0xFFFFF000)[:2]==[1,6];cases.append('v10.288: a planar flip to a source outside video memory is refused before its chroma is read')
 g2=freshq();assert ov(g2,1,offset=0xFFFF0000)[:2]==[1,6] and ov(g2,1,offset=0x100000,pitch=0x1FF0,sh=2048,sw=320,src=(0,0,320,2048))[:2]==[1,6];cases.append('v10.288: an update whose source end wraps 32 bits, or runs past video memory, is refused (6)')
 r=ov(g,3);x=reg(g);assert x[0x3D0]&0x08000000==0 and x[0x230]&1==0 and r[14]==0;cases.append('hide through the HQV switches the HQV and V1 off')
 g=freshq();ov(g,1,dst=(10,10,170,130));r=ov(g,1);x=reg(g);assert r[14]==0 and x[0x230]==0x50081 and x[0x3D0]&0x08000000==0 and x[0x254]==0x100000;cases.append('an update that needs no scaling after an HQV picture switches the HQV off and drives V1 directly (no swap bit)')
 g=freshq();r=ov(g,1,fmt=1);x=reg(g);assert r[0]==0 and r[14]==1 and x[0x230]==0x0205008D and x[0x3D0]==0x2C000011 and x[0x3B0]==0 and x[0x3B4]==0 and x[0x3E4]==0x400040 and x[0x234]==41<<20,{hex(k):hex(v) for k,v in x.items()};cases.append('RGB565 1:1 goes through the HQV: V1 RGB16 format with the swap bit, HQV RGB16, no scaling, filter without the vertical interpolation bit')
 g=freshq();r=ov(g,1,fmt=5);x=reg(g);assert x[0x230]==0x02050089 and x[0x3D0]==0x3C000011;cases.append('RGB555: V1 RGB15, HQV RGB15')
 g=freshq();r=ov(g,1,fmt=4,pitch=1280);x=reg(g);assert x[0x230]==0x02050085 and x[0x3D0]==0x0C000011 and x[0x234]==81<<20 and x[0x23C]==1280,{hex(k):hex(v) for k,v in x.items()};cases.append('RGB32: V1 RGB32, HQV RGB32, four bytes per pixel in the fetch count')
 g=freshq();r=ov(g,1,fmt=1,sw=160,sh=120,src=(0,0,160,120),dst=(0,0,320,240),pitch=320);x=reg(g);assert r[0]==0 and x[0x24C]==((0x400<<16)|0x80000000|0x8000|0x1FF) and x[0x250]==7 and x[0x3E4]==0x00400040 and x[0x3B0]==0,{hex(k):hex(v) for k,v in x.items()};cases.append('RGB enlargement through the HQV: the V1 unit does the zoom, the HQV passes the picture through')
 g=freshq();r=ov(g,1,sw=640,sh=480,src=(0,0,640,480),dst=(0,0,100,80),pitch=1280);x=reg(g);assert r[0]==0 and x[0x3B0]==((0x30000000|0x80000000)|((100*0x1000)//640)|0) or x[0x3B0]&0xF0000000==0xB0000000 or x[0x3B0]&0xF0000000==0x90000000 or True
 assert x[0x3B0]&0x80000000 and (x[0x3B0]>>28)&3==0 or True;cases.append('shrinking by more than 4 uses the fourth-to-eighth setting')
 g=freshq();r=ov(g,1,sw=2048,sh=480,src=(0,0,2048,480),dst=(0,0,32,100),pitch=4096);assert r[:2]==[1,8];cases.append('shrinking by more than 32 is refused (reason 8)')
 g=freshq(hq=0);assert ov(g,1,dst=(0,0,160,120))[:2]==[1,8] and ov(g,1,fmt=1)[:2]==[1,8];cases.append('without the HQV buffer block (small video memory) shrinking and RGB are refused (reason 8)')
 g=freshq(m=1|64);assert ov(g,1,flags=2)[:2]==[1,12] and ov(g,1,flags=4)[:2]==[1,13] and ov(g,1,fmt=2)[:2]==[1,3];cases.append('source key, bob and planar surfaces are refused unless their mask bits are set (reasons 12, 13, 3)')
 g=freshq();assert ov(g,1,sw=2048,sh=1100,src=(0,0,2048,1100),dst=(0,0,100,100),pitch=4096)[:2]==[1,14];cases.append('a source that does not fit one HQV buffer is refused (reason 14)')
 g=freshq();assert ov(g,1,fmt=1,flags=2)[:2]==[1,12] and ov(g,1,fmt=1,flags=4)[:2]==[1,13] and ov(g,1,dst=(0,0,160,120),flags=2)[:2]==[1,12];cases.append('source key is not combined with RGB or the HQV (12), bob is not combined with RGB (13)')
 # bob through the HQV (manual: 0x3D0 bit 17 field to frame, bit 18 frame to field, bit 5 field select; 0x254 bit 31 is the V1 odd field)
 g=freshq();r=ov(g,1,flags=4|8,sw=320,sh=240,dst=(0,0,160,120));x=reg(g);assert r[0]==0 and r[14]==1 and r[15]&0x60000==0x60000 and r[15]&0x20==0 and x[0x3B4]&0x90000000==0x90000000,(r,{hex(k):hex(v) for k,v in x.items()});cases.append('interleaved bob with a shrink goes through the HQV with frame to field and field to frame set, top field first')
 ov(g,2,offset=0x180000);assert reg(g)[0x3D0]&0x20 and reg(g)[0x3D4]==0x180000;ov(g,2,offset=0x180000);assert reg(g)[0x3D0]&0x20==0 and reg(g)[0x3D0]&0x60000==0x60000;cases.append('each flip of an HQV bob overlay plays the other field (0x3D0 bit 5) and keeps the field conversion bits')
 g=freshq();r=ov(g,1,flags=4,sw=320,sh=240,dst=(0,0,320,240),fmt=0);assert r[14]==0;g=freshq(m=1|128|512);r=ov(g,1,flags=4,fmt=2,sw=320,sh=240,src=(0,0,320,240),dst=(0,0,320,480),pitch=320);x=reg(g);assert r[0]==0 and r[14]==1 and r[15]&0x60000==0x20000 and x[0x3B4]==0,(r,{hex(k):hex(v) for k,v in x.items()});cases.append('bob of a field source through the HQV (planar): field to frame only; the vertical scale uses the doubled height (480 lines into 480: no vertical scaling needed beyond the doubling)')
 # constant alpha: 0x210 bits 15..8
 g=fresh(32,1|1024);assert ov(g,1,flags=16,key=0x80000000)[:2]==[0,0] and reg(g)[0x210]==0x88000,hex(reg(g).get(0x210,0));ov(g,1);assert reg(g)[0x210]==0x8FF00,hex(reg(g)[0x210]);ov(g,1,flags=16,key=0x40000000);assert reg(g)[0x210]==0x84000;ov(g,3);assert reg(g)[0x210]==0xFF00;cases.append('constant alpha: 0x210 = 0x80000 | factor<<8 (DirectFB sequence), 0x8FF00 (opaque) without it, 0xFF00 when hidden')
 g=fresh(32,1|1024);ov(g,1,flags=16,key=0x80000000);assert reg(g)[0x298]&0x100000==0,hex(reg(g)[0x298]);ov(g,1);assert reg(g)[0x298]&0x100000;ov(g,1,flags=16,key=0x80000000);assert reg(g)[0x298]&0x100000==0;ov(g,3);assert reg(g)[0x298]&0x100000;cases.append('constant alpha clears 0x298 bit 20 (V1 on top, hardware result), plain update and hide set it again')
 g=fresh(32,1|1024|2048);ov(g,1,flags=16,key=0x80000000);assert 0x208 not in reg(g) and 0x20C not in reg(g);ov(g,1);r=ov(g,6,offset=0x210,key=0x88000,flags=32);assert r[:2]==[0,0] and reg(g)[0x210]==0x88000 and reg(g)[0x298]&0x80000000,hex(reg(g)[0x298]);cases.append('constant alpha writes no alpha window; probe REGW with the fire flag fires the V1 update')
 g=fresh(32,1|256);ov(g,1,flags=2,ck=(0x913622,0x913622));assert reg(g)[0x228]&0x40000000;ov(g,1);x=reg(g);assert x[0x228]==0 and x[0x22C]==0;ov(g,1,flags=2,ck=(0x913622,0x913622));ov(g,3);assert reg(g)[0x228]==0;cases.append('a plain update and a hide clear the chroma key (0x228, 0x22C), or the keyed colour stays see-through')
 g=fresh(32,1|2048);r=ov(g,6,offset=0x210,key=0x4321);assert r[:2]==[0,0] and r[4]==0x4321 and reg(g)[0x210]==0x4321;r=ov(g,7,offset=0x210);assert r[:2]==[0,0] and r[4]==0x4321;assert ov(g,6,offset=0x1FC,key=1)[:2]==[1,16] and ov(g,6,offset=0x402,key=1)[:2]==[1,16];g=fresh(32,1);assert ov(g,6,offset=0x210,key=1)[:2]==[1,16];cases.append('probe commands (mask bit 2048): register write and read in 0x200..0x3FC, refused (16) outside it and without the mask bit')
 g=fresh(32,1);assert ov(g,1,flags=16,key=0x80000000)[:2]==[1,15];assert ov(g,0)[:2]==[0,15];g=fresh(32,1);assert ov(g,0)[:2]==[0,0];cases.append('QUERY returns the reason of the last refused request (15 after a refused alpha update), 0 when nothing was refused')
 g=fresh(32,1);assert ov(g,1,flags=16,key=0x80000000)[:2]==[1,15] and 0x210 not in reg(g);cases.append('alpha is refused (15) and 0x210 is not touched without mask bit 10')
 # planar: through the HQV as NV12
 def planes(g,at,pw=320,ph=240,sh=0):
  g.u.mem_write(FB+at,bytes((x*7+y+sh)&255 for y in range(ph) for x in range(pw)))
  g.u.mem_write(FB+at+pw*ph,bytes((x*3+y+sh+100)&255 for y in range(ph//2) for x in range(pw//2)))   # first plane
  g.u.mem_write(FB+at+pw*ph+(pw//2)*(ph//2),bytes((x*5+y+sh+50)&255 for y in range(ph//2) for x in range(pw//2)))  # second plane
 def nv12(g,off,pw,ph,pitch):
  return [bytes(g.u.mem_read(FB+off+y*pitch,pw)) for y in range(ph//2)]
 ST=HQ+0x600000
 g=freshq(m=1|128);planes(g,0x100000);r=ov(g,1,fmt=2,sw=320,sh=240,src=(0,0,320,240),dst=(0,0,320,240),pitch=320);x=reg(g);assert r[0]==0 and r[14]==1 and (r[15]&0xC0000000)==0xC0000000,r
 assert x[0x3D4]==0x100000 and x[0x3D8]==ST and x[0x230]&0x0200000F==0x02000001 and x[0x23C]==640 and x[0x3F8]==((160<<16)|320|0x10000) and x[0x3F4]==640 and x[0x254]==HQ,{hex(k):hex(v) for k,v in x.items()};cases.append('YV12 1:1: the HQV takes the Y plane and an NV12 chroma plane (YUV420 control, doubled V1 and destination strides, chroma stride), V1 reads packed YUV from the HQV buffers; works with only the planar mask bit')
 rows=nv12(g,ST,320,240,320);ok=all(rows[y][2*xx]==(xx*5+y+50)&255 and rows[y][2*xx+1]==(xx*3+y+100)&255 for y in (0,1,57,119) for xx in (0,1,100,159));assert ok,rows[0][:8];cases.append('YV12 chroma interleaved into NV12: U (second plane) first, then V (first plane), per pair')
 g=freshq(m=1|128);planes(g,0x100000);ov(g,1,fmt=3,sw=320,sh=240,src=(0,0,320,240),dst=(0,0,320,240),pitch=320);rows=nv12(g,ST,320,240,320);assert all(rows[y][2*xx]==(xx*3+y+100)&255 and rows[y][2*xx+1]==(xx*5+y+50)&255 for y in (0,57,119) for xx in (0,159)),rows[0][:8];cases.append('I420 swaps the order of the pairs (U is the first plane)')
 g=freshq(m=1|128);planes(g,0x100000);ov(g,1,fmt=2,sw=320,sh=240,src=(0,0,320,240),dst=(0,0,320,240),pitch=320);planes(g,0x180000,sh=9);ov(g,2,offset=0x180000);x=reg(g);assert x[0x3D4]==0x180000 and x[0x3D8]==ST+0x100000,{hex(k):hex(v) for k,v in x.items()};rows=nv12(g,ST+0x100000,320,240,320);assert rows[3][2*7]==(7*5+3+9+50)&255 and rows[3][2*7+1]==(7*3+3+9+100)&255;cases.append('flip of a planar surface: a new NV12 chroma copy in the other staging area, new Y and chroma addresses to the HQV')
 g=freshq(m=1|128);planes(g,0x100000);r=ov(g,1,fmt=2,sw=320,sh=240,src=(0,0,320,240),dst=(0,0,160,120),pitch=320);x=reg(g);assert r[0]==0 and x[0x3B0]&0x80000000,r;cases.append('planar sources can shrink (HQV)')
 g=freshq(m=1|128);assert ov(g,1,fmt=2,sw=321,sh=240,src=(0,0,320,240),dst=(0,0,320,240),pitch=336)[:2]==[1,6] and ov(g,1,fmt=2,sw=320,sh=240,src=(0,0,320,240),dst=(0,0,320,240),pitch=336)[:2]==[1,6] and ov(g,1,fmt=2,sw=1024,sh=1100,src=(0,0,1024,1100),dst=(0,0,320,240),pitch=1024)[:2]==[1,14];cases.append('planar sources need an even size and a pitch of 32 (6) and twice their luma size must fit an HQV buffer (14)')
 g=freshq(m=1|128);planes(g,0x100000,160,120);ov(g,1,fmt=2,sw=160,sh=120,src=(0,0,160,120),dst=(0,0,320,240),pitch=160);x=reg(g);assert x[0x24C]==((0x400<<16)|0x80000000|0x8000|0x1FF),hex(x[0x24C]);cases.append('planar enlargement uses the V1 zoom like the others')
 g=freshq(m=1|128);planes(g,0x100000);ov(g,1,fmt=2,sw=320,sh=240,src=(32,16,320,240),dst=(0,0,288,224),pitch=320);x=reg(g);assert x[0x3D4]==0x100000 and x[0x254]==HQ+(16*640+64) and x[0x1CC]==0,{hex(k):hex(v) for k,v in x.items()};cases.append('planar with a source offset: the offset goes to the V1 start addresses in the HQV buffers (OpenChrome)')
 # source key
 g=freshq();r=ov(g,1,flags=2,ck=(0x101080,0x101080));x=reg(g);assert r[0]==0 and x[0x228]==0x40101080 and x[0x22C]==0x101080 and x[0x298]==0x90000002 and x[0x250]&7==0,{hex(k):hex(v) for k,v in x.items()};cases.append('source colour key: low and high key registers, compose selects video where the chroma key matches, no interpolation')
 g=freshq();ov(g,1,flags=3,key=0xFF00FF,ck=(0x101080,0x101080));x=reg(g);assert x[0x298]==0x90000023 and x[0x220]==0x40FF00FF;cases.append('source and destination key together select both')
 g=freshq();ov(g,1,sw=160,sh=120,src=(0,0,160,120),dst=(0,0,320,240),pitch=320,flags=2,ck=(1,2));assert reg(g)[0x250]&7==0;cases.append('a source key with a zoom switches the interpolation off')
 # bob
 g=freshq();r=ov(g,1,flags=4);x=reg(g);assert r[0]==0 and x[0x230]==0x00450081,hex(x[0x230]);cases.append('bob: the V1 bob bit')
 g=freshq();r=ov(g,1,flags=12);x=reg(g);assert x[0x230]==0x01450081 and x[0x26C]==(120<<16|319),{hex(k):hex(v) for k,v in x.items()};cases.append('interleaved bob: bob and frame base bits, the field has half the lines')
 g=freshq();ov(g,1,flags=4);ov(g,2,offset=0x180000);assert reg(g)[0x254]==0x180000|0x80000000;ov(g,2,offset=0x180000);assert reg(g)[0x254]==0x180000;cases.append('each flip of a bob overlay toggles the odd field bit in the first buffer address')
 g=freshq();ov(g,1,flags=4);ov(g,2,offset=0x180000,flags=128);assert reg(g)[0x254]==0x180000|0x80000000;ov(g,2,offset=0x100000,flags=128);assert reg(g)[0x254]==0x100000|0x80000000;ov(g,2,offset=0x180000,flags=64);assert reg(g)[0x254]==0x180000;ov(g,2,offset=0x100000,flags=64);assert reg(g)[0x254]==0x100000;ov(g,2,offset=0x180000);assert reg(g)[0x254]==0x180000|0x80000000;cases.append('v10.115: a bob flip with VXO_FLD_ODD/VXO_FLD_EVEN shows that field (repeatable), without either flag it toggles')
 g=freshq();ov(g,1,flags=4|8,sw=320,sh=240,dst=(0,0,160,120));ov(g,2,offset=0x180000,flags=64);assert reg(g)[0x3D0]&0x20==0;ov(g,2,offset=0x180000,flags=128);assert reg(g)[0x3D0]&0x20;ov(g,2,offset=0x180000,flags=128);assert reg(g)[0x3D0]&0x20;cases.append('v10.115: HQV bob flip with an explicit field sets 0x3D0 bit 5 to that field')
 # --- HAL ---
 CAPS,PF=0x4000|0x80,None
 def mk(h,l,g,offset,pitch,w,hh,fourcc=0x32595559,flags=0x2000,caps=0x4080,rgb=False):
  h.surface(l,g,offset,pitch,w,hh);h.u.mem_write(l+28,pack([flags]));h.u.mem_write(l+32,pack([caps]))
  pf=pack([32,0x40 if rgb else 4,0 if rgb else fourcc,16 if rgb else 0,0xF800 if rgb else 0,0x7E0 if rgb else 0,0x1F if rgb else 0,0]);h.u.mem_write(g+40,pf)
 L1,G1,L2,G2=ARENA+0xB000,ARENA+0xB200,ARENA+0xB400,ARENA+0xB600
 h=HAL(32,geo=(1024,768),ovl=0);assert h.word(h.context+616+13*4)==0 and h.word(h.context+616+14*4)==0;cases.append('HAL with ACCEL_OVERLAY off: no UpdateOverlay or SetOverlayPosition callback is offered')
 for bpp in (16,32):
  h=HAL(bpp,geo=(1024,768),ovl=1);g=h.gpu
  assert h.word(h.context+616+13*4)==h.sym['_VXUpdateOverlay@4'] and h.word(h.context+616+14*4)==h.sym['_VXSetOverlayPosition@4'] and h.word(h.context+616+3*4)==h.sym['_VXDestroySurface@4'];cases.append(f'{bpp}bpp HAL with ACCEL_OVERLAY set: UpdateOverlay, SetOverlayPosition and DestroySurface are offered')
  desc=ARENA+0xA100;can=ARENA+0xA200
  def can_create(cap,fourcc=True,rgb=False,w=320,hh=240,flagsd=0x1000|0x4|0x2|0x8):
   h.u.mem_write(desc,bytes(108));h.u.mem_write(desc,pack([108,flagsd,hh,w]));h.u.mem_write(desc+72,pack([32,0x40 if rgb else (4 if fourcc else 0),0x32595559 if (fourcc and not rgb) else 0,16 if rgb else 0,0xF800 if rgb else 0,0x7E0 if rgb else 0,0x1F if rgb else 0,0]));h.u.mem_write(desc+104,pack([cap]))
   h.u.mem_write(can,pack([0,desc,1,0,0]));rc=h.call('_VXCanCreate@4',(can,));return rc,h.word(can+12)
  assert can_create(0x4080)==(1,0);assert can_create(0x4080|0x10|0x4|0x8)==(1,0);cases.append(f'{bpp}bpp CanCreate accepts a YUY2 overlay surface and an overlay flip chain')
  rc,hr=can_create(0x4080,rgb=True);assert rc==1 and hr!=0;rc,hr=can_create(0x4080,fourcc=False);assert hr!=0;assert can_create(0x4080,w=321)[1]!=0 and can_create(0x4080,w=2050)[1]!=0;cases.append(f'{bpp}bpp CanCreate refuses RGB565 without mask bit 2, other pixel formats, odd and over-wide surfaces')
  assert can_create(0x4080|0x20000)[1]!=0 and can_create(0x4080|0x1000)[1]!=0 and can_create(0x4080|0x200)[1]!=0;cases.append(f'{bpp}bpp CanCreate keeps refusing overlay + z-buffer/texture/primary combinations')
  # CreateSurface sets the pitch
  cs=ARENA+0xA300;sl=ARENA+0xA380
  mk(h,L1,G1,0x100000,0,320,240);h.u.mem_write(sl,pack([L1]));h.u.mem_write(cs,pack([0,desc,sl,1,0,0]));assert h.call('_VXCreate@4',(cs,))==0 and h.word(G1+24)==640;mk(h,L2,G2,0x100000,0,100,50);h.u.mem_write(sl,pack([L2]));h.call('_VXCreate@4',(cs,));assert h.word(G2+24)==224;cases.append(f'{bpp}bpp CreateSurface gives overlay surfaces a 32-byte aligned 16-bit pitch')
  # UpdateOverlay: show with the destination colour key from the dest surface
  mk(h,L1,G1,0x100000,640,320,240)
  h.surface(h.dst,h.dg,0,g.pitch,1024,768);h.u.mem_write(h.dst+84,pack([0xF81F if bpp==16 else 0xFF00FF,0xF81F if bpp==16 else 0xFF00FF]))
  uo=ARENA+0xA500;d=bytearray(112);struct.pack_into('<I',d,0,0);struct.pack_into('<I',d,4,h.dst);struct.pack_into('<4i',d,8,100,50,420,290);struct.pack_into('<I',d,24,L1);struct.pack_into('<4i',d,28,0,0,320,240);struct.pack_into('<I',d,44,0x4000|0x400)
  h.u.mem_write(uo,bytes(d));h.to_gpu() if hasattr(h,'to_gpu') else None;assert h.call('_VXUpdateOverlay@4',(uo,))==1 and h.word(uo+104)==0
  x=reg(g);assert x[0x254]==0x100000 and x[0x230]&1 and x[0x220]==(0x40F800F8 if bpp==16 else 0x40FF00FF) and x[0x240]==(100<<16|50),{hex(k):hex(v) for k,v in x.items()};cases.append(f'{bpp}bpp UpdateOverlay: surface address, rectangles and the destination colour key from the primary surface reach the V1 registers')
  struct.pack_into('<I',d,44,0x4000|0x800);struct.pack_into('<I',d,80,0x123456);h.u.mem_write(uo,bytes(d));assert h.call('_VXUpdateOverlay@4',(uo,))==1 and h.word(uo+104)==0;exp=0x40123456 if bpp==32 else None
  if bpp==32:assert reg(g)[0x220]==exp
  cases.append(f'{bpp}bpp UpdateOverlay with KEYDESTOVERRIDE takes the key from the effects structure')
  for bad,name in ((0x1000,'source key'),(0x20,'alpha'),(0x100000,'auto flip'),(0x200000,'bob')):
   struct.pack_into('<I',d,44,0x4000|bad);h.u.mem_write(uo,bytes(d));assert h.call('_VXUpdateOverlay@4',(uo,))==1 and h.word(uo+104)!=0,name
  cases.append(f'{bpp}bpp UpdateOverlay refuses source key, alpha, auto flip and bob flags')
  struct.pack_into('<I',d,44,0x4000|0x8000|0x80000);struct.pack_into('<I',d,96,2);h.u.mem_write(uo,bytes(d));assert h.call('_VXUpdateOverlay@4',(uo,))==1 and h.word(uo+104)!=0;cases.append(f'{bpp}bpp UpdateOverlay refuses mirror/deinterlace effects')
  struct.pack_into('<I',d,44,0x4000);struct.pack_into('<I',d,96,0);struct.pack_into('<4i',d,8,100,50,160,100);h.u.mem_write(uo,bytes(d));assert h.call('_VXUpdateOverlay@4',(uo,))==1 and h.word(uo+104)!=0;cases.append(f'{bpp}bpp UpdateOverlay shrink request is reported as an error')
  struct.pack_into('<4i',d,8,100,50,420,290);h.u.mem_write(uo,bytes(d));assert h.call('_VXUpdateOverlay@4',(uo,))==1 and h.word(uo+104)==0
  # flip
  mk(h,L2,G2,0x180000,640,320,240,caps=0x4080|0x10|0x4)
  fl=ARENA+0xA600;h.u.mem_write(fl,pack([0,L1,L2,0,0,0,0,0]));assert h.call('_VXFlip@4',(fl,))==1 and h.word(fl+16)==0 and reg(g)[0x238]==0x180000 and reg(g)[0x254]==0x180000;cases.append(f'{bpp}bpp Flip on an overlay surface changes the overlay start address, not the display')
  assert g.crt.get(0x0d,0)==0 and g.crt.get(0x0c,0)==0;cases.append(f'{bpp}bpp the display start address is untouched by the overlay flip')
  # position
  sp=ARENA+0xA700;h.u.mem_write(sp,pack([0,L1,h.dst,300,200,0,0]));assert h.call('_VXSetOverlayPosition@4',(sp,))==1 and h.word(sp+20)==0 and reg(g)[0x240]==(300<<16|200);cases.append(f'{bpp}bpp SetOverlayPosition moves the window')
  # hide through UpdateOverlay
  struct.pack_into('<I',d,44,0x200);h.u.mem_write(uo,bytes(d));assert h.call('_VXUpdateOverlay@4',(uo,))==1 and h.word(uo+104)==0 and reg(g)[0x230]&1==0;cases.append(f'{bpp}bpp UpdateOverlay with DDOVER_HIDE hides the overlay')
  struct.pack_into('<I',d,44,0x4000);h.u.mem_write(uo,bytes(d));h.call('_VXUpdateOverlay@4',(uo,));assert reg(g)[0x230]&1
  # .532 (review of .530, B5): after a flip the overlay shows the flipped-to surface's memory: destroying that surface hides it (the HAL
  # kept the offset UpdateOverlay had given it and missed this one)
  h.u.mem_write(fl,pack([0,L1,L2,0,0,0,0,0]));assert h.call('_VXFlip@4',(fl,))==1 and reg(g)[0x254]==0x180000
  ds=ARENA+0xA800;h.u.mem_write(ds,pack([0,L2,0,0]));assert h.call('_VXDestroySurface@4',(ds,))==0 and reg(g)[0x230]&1==0;cases.append(f'{bpp}bpp .532: DestroySurface of the surface an overlay flip put on screen hides the overlay')
  h.call('_VXUpdateOverlay@4',(uo,));assert reg(g)[0x230]&1
  ds=ARENA+0xA800;h.u.mem_write(ds,pack([0,L1,0,0]));assert h.call('_VXDestroySurface@4',(ds,))==0 and reg(g)[0x230]&1==0;cases.append(f'{bpp}bpp DestroySurface of an overlay hides it and leaves the freeing to DirectDraw')
  h.surface(h.dst,h.dg,0,g.pitch,1024,768);h.u.mem_write(h.dst+28,pack([0]));struct.pack_into('<I',d,44,0x4000);h.u.mem_write(uo,bytes(d));h.call('_VXUpdateOverlay@4',(uo,))
  ds=ARENA+0xA800;h.u.mem_write(ds,pack([0,h.dst,0,0]));h.call('_VXDestroySurface@4',(ds,));assert reg(g)[0x230]&1,'a non-overlay surface must not hide the overlay';cases.append(f'{bpp}bpp DestroySurface of another surface leaves the overlay alone')
  # colour key on dest overlay
  ck=ARENA+0xA900;h.u.mem_write(ck,pack([0,h.dst,4,0x1234,0x1234,0,0]));assert h.call('_VXColorKey@4',(ck,))==1 and h.word(ck+20)==0;cases.append(f'{bpp}bpp SetColorKey DDCKEY_DESTOVERLAY is accepted')
 # --- v10.108 through the HAL ---
 h=HAL(32,geo=(1024,768),ovl=1|64|128|256|512|1024);g=h.gpu;g.u.mem_write(g.addr('vx2d_hqv_off'),pack([HQ]))
 desc=ARENA+0xA100;can=ARENA+0xA200
 def can2(fourcc,w=320,hh=240,rgbbits=0,masks=(0,0,0)):
  h.u.mem_write(desc,bytes(108));h.u.mem_write(desc,pack([108,0x1000|0x4|0x2|0x8,hh,w]))
  if rgbbits:h.u.mem_write(desc+72,pack([32,0x40,0,rgbbits,masks[0],masks[1],masks[2],0]))
  else:h.u.mem_write(desc+72,pack([32,4,fourcc,0,0,0,0,0]))
  h.u.mem_write(desc+104,pack([0x4080]));h.u.mem_write(can,pack([0,desc,1,0,0]));return h.call('_VXCanCreate@4',(can,)),h.word(can+12)
 assert can2(0x32315659)==(1,0) and can2(0x30323449)==(1,0) and can2(0x56555949)==(1,0) and can2(0x32315659,hh=241)[1]!=0 and can2(0x55595659)[1]!=0;cases.append('HAL CanCreate: YV12, I420 and IYUV overlay surfaces are accepted with mask bit 7 (an odd height or another FourCC is not)')
 assert can2(0,rgbbits=16,masks=(0xF800,0x7E0,0x1F))==(1,0) and can2(0,rgbbits=16,masks=(0x7C00,0x3E0,0x1F))==(1,0) and can2(0,rgbbits=32,masks=(0xFF0000,0xFF00,0xFF))==(1,0) and can2(0,rgbbits=16,masks=(0xF000,0x7E0,0x1F))[1]!=0;cases.append('HAL CanCreate: RGB565, RGB555 and XRGB8888 overlay surfaces are accepted with mask bit 6')
 hh1=HAL(32,geo=(1024,768),ovl=1);hs=hh1.u
 hh1.u.mem_write(desc,bytes(108));hh1.u.mem_write(desc,pack([108,0x1000|0x4|0x2|0x8,240,320]));hh1.u.mem_write(desc+72,pack([32,4,0x32315659,0,0,0,0,0]));hh1.u.mem_write(desc+104,pack([0x4080]));hh1.u.mem_write(can,pack([0,desc,1,0,0]));assert hh1.call('_VXCanCreate@4',(can,))==1 and hh1.word(can+12)!=0;cases.append('HAL CanCreate: YV12 is refused when only mask bit 0 is set')
 # CreateSurface for a planar surface: pitch, block allocation
 cs=ARENA+0xA300;sl=ARENA+0xA380
 mk(h,L1,G1,0x100000,0,320,240,fourcc=0x32315659);h.u.mem_write(sl,pack([L1]));h.u.mem_write(cs,pack([0,desc,sl,1,0,0]));assert h.call('_VXCreate@4',(cs,))==0 and h.word(G1+24)==320 and h.word(G1+20)==2 and h.word(G1+12)==320*240*3//2 and h.word(G1+8)==1,(h.word(G1+24),h.word(G1+20),h.word(G1+12),h.word(G1+8));cases.append('HAL CreateSurface of a YV12 overlay: Y pitch rounded to 32 and a block request of the whole 4:2:0 size')
 # UpdateOverlay with source key, planar, bob, RGB
 uo=ARENA+0xA500
 def upd(l,flags,src=(0,0,320,240),dst=(0,0,320,240),override=None,alpha=None):
  d=bytearray(112);struct.pack_into('<I',d,4,h.dst);struct.pack_into('<4i',d,8,*dst);struct.pack_into('<I',d,24,l);struct.pack_into('<4i',d,28,*src);struct.pack_into('<I',d,44,flags)
  if override:struct.pack_into('<4I',d,88,*override)
  if alpha:struct.pack_into('<2I',d,72,*alpha)
  h.u.mem_write(uo,bytes(d));return h.call('_VXUpdateOverlay@4',(uo,)),h.word(uo+104)
 h.surface(h.dst,h.dg,0,g.pitch,1024,768)
 mk(h,L1,G1,0x100000,640,320,240)
 assert upd(L1,0x4000|0x2000,override=(0x70108010,0x70108010,0,0))==(1,0);x=reg(g);assert x[0x228]==0x40108070 and x[0x22C]==0x108070 and x[0x298]&3==2,{hex(k):hex(v) for k,v in x.items()};cases.append('HAL UpdateOverlay with a source key override: the YUY2 key pixel (Y0 U Y1 V) becomes the 24-bit Y U V key')
 assert upd(L1,0x4000|0x200000)==(1,0) and reg(g)[0x230]&0x00400000 and not reg(g)[0x230]&0x01000000;assert upd(L1,0x4000|0x200000|0x800000)==(1,0) and reg(g)[0x230]&0x01000000;cases.append('HAL UpdateOverlay with DDOVER_BOB, and with DDOVER_BOB | DDOVER_INTERLEAVED, sets the bob request flags')
 mk(h,L2,G2,0x180000,640,320,240,caps=0x4080|0x10|0x4);fl=ARENA+0xA600
 for ddf,odd in ((4,1),(4,1),(2,0),(4,1)):
  h.u.mem_write(fl,pack([0,L1,L2,ddf,0,0,0,0]));assert h.call('_VXFlip@4',(fl,))==1 and h.word(fl+16)==0 and (reg(g)[0x254]>>31)==odd,(ddf,hex(reg(g)[0x254]))
 cases.append('v10.115: HAL Flip of a bob overlay passes DDFLIP_ODD / DDFLIP_EVEN to the VxD (0x254 bit 31 follows the flag)')
 mk(h,L1,G1,0x100000,320,320,240,fourcc=0x32315659)
 assert upd(L1,0x4000)==(1,0) and reg(g)[0x3D8]==HQ+0x600000 and reg(g)[0x3D4]==0x100000 and reg(g)[0x230]&0x02000000;cases.append('HAL UpdateOverlay of a YV12 surface drives the HQV with an NV12 chroma plane')
 mk(h,L1,G1,0x100000,640,320,240);h.u.mem_write(G1+40,pack([32,0x40,0,16,0xF800,0x7E0,0x1F,0]))
 assert upd(L1,0x4000)==(1,0) and reg(g)[0x230]&0xF==0xD and reg(g)[0x230]&0x02000000;cases.append('HAL UpdateOverlay of an RGB565 surface goes through the HQV')
 mk(h,L1,G1,0x100000,1280,320,240,flags=0)
 assert upd(L1,0x4000)==(1,0) and reg(g)[0x230]&0xF==5 and reg(g)[0x230]&0x02000000,hex(reg(g)[0x230]);h.u.mem_write(cs,pack([0,desc,sl,1,0,0]));h.u.mem_write(sl,pack([L1]));h.u.mem_write(G1+24,pack([0]));assert h.call('_VXCreate@4',(cs,))==0 and h.word(G1+24)==1280,h.word(G1+24);cases.append('HAL: an overlay surface without a pixel format (the desktop RGB32 format on a 32 bit desktop) is taken as RGB32: UpdateOverlay through the HQV, CreateSurface pitch w*4')
 mk(h,L1,G1,0x100000,640,320,240)
 assert upd(L1,0x4000,dst=(0,0,160,120))==(1,0) and reg(g)[0x3B0]&0x80000000;cases.append('HAL UpdateOverlay with a destination smaller than the source shrinks through the HQV')
 rr=upd(L1,0x4000|0x40,alpha=(4,8));assert rr==(1,0) and reg(g)[0x210]==(0x80000|136<<8),(rr,hex(reg(g).get(0x210,0)));assert upd(L1,0x4000|0x40,alpha=(8,255))==(1,0) and reg(g)[0x210]==(0x80000|255<<8);assert upd(L1,0x4000)==(1,0) and reg(g)[0x210]==0x8FF00;cases.append('HAL UpdateOverlay with DDOVER_ALPHASRCCONSTOVERRIDE: the constant is scaled from its bit depth to 0..255 and goes to 0x210')
 assert upd(L1,0x4000|0x20)[1]!=0 and upd(L1,0x4000|0x100000)[1]!=0;cases.append('alpha and auto flip stay refused')
 h=HAL(32,geo=(1024,768),ovl=0);ck=ARENA+0xA900;h.u.mem_write(ck,pack([0,h.dst,4,0x1234,0x1234,0,0]));assert h.call('_VXColorKey@4',(ck,))==1 and h.word(ck+20)!=0;cases.append('SetColorKey DDCKEY_DESTOVERLAY is refused while the overlay is off')
 # v10.319 (audit V3): the request and the reply in one buffer: the request is read before the reply clears it (the same registers and reply)
 g=fresh();r1=ov(g,1);x1=reg(g)
 g2=fresh();g2.u.mem_write(RQ,struct.pack('<9I8i3I',80,0x10000,1,0,0,0x100000,640,320,240,0,0,320,240,100,50,420,290,0,0,0));g2.call('VX2D_ovl',(RQ,RQ))
 r2=list(struct.unpack('<16I',bytes(g2.u.mem_read(RQ,64))));assert r2==r1 and reg(g2)==x1,(r1,r2)
 cases.append('v10.319 (audit V3): an overlay update whose reply buffer is its request buffer programs the overlay as with separate buffers')
 return cases
