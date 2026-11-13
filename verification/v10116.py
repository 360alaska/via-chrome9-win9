#!/usr/bin/env python3
"""v10.116: GDI drawing from system memory (VX2D_xfer): host-fed mono blits, staged ROP blits, text from the DRV's 1-bit buffer, scan line spans. The compiled VxD runs against the engine model."""
import struct,random
from engine import GPU,ARENA,FB,SRC,pack,rop3,uses
XQ=ARENA+0x2000;MASKW=ARENA+0x900;STAGE=0xC00000
def mk(g):
 for n,v in (('vx2d_host_min',0),('vx2d_mono_min',0),('vx2d_stage_off',STAGE),('vx2d_host',63),('vx2d_col8',1)):g.u.mem_write(g.addr(n),pack([v]))
 return g
def xq(g,op,flags=0,rop=0xCC,fg=0,bg=0,pfg=0,d=(0,0,1,1),src=(SRC,1,0,0,8,8,4),box=0,o=(0,0,0,0),soft=0,spans=(),tpl=None):
 head=struct.pack('<9I4I6Ii I4iI I3II',1264,0x10000,op,flags,g.bpp,rop,fg,bg,pfg,*d,src[0],src[1],src[2],src[3],src[4],src[5],src[6],box,*o,soft,MASKW,0,0,0,len(spans))
 sp=[0]*384
 for i,(y,a,b) in enumerate(spans):sp[i*3:i*3+3]=[y,a,b]
 t=tpl if tpl is not None else [0]*93
 g.u.mem_write(XQ,head+struct.pack('<384H',*sp)+pack(t));return g.call('VX2D_xfer',(XQ,))
def back(g):return struct.unpack('<3I',bytes(g.u.mem_read(XQ+27*4,12))) # done, mask_out, result
def px(g,snap,x,y):at=y*g.pitch+x*g.px;return int.from_bytes(snap[at:at+g.px],'little')
def put(snap,g,x,y,v):at=y*g.pitch+x*g.px;snap[at:at+g.px]=(v&((1<<g.bpp)-1)).to_bytes(g.px,'little')
def bit(buf,stride,x,y):return (buf[y*stride+(x>>3)]>>(7-(x&7)))&1
def v10116_checks():
 cases=[];rnd=random.Random(116)
 def mono_src(W,H,stride):
  b=bytes(rnd.randrange(256) for _ in range(stride*H));return b
 for bpp in (16,32):
  m=(1<<bpp)-1
  # --- XF_MONO: SRCCOPY of a 1-bit bitmap through the host window ---
  for (sx,dx,dy,w,h) in ((3,5,7,37,9),(0,0,0,64,3),(5,40,2,20,6),(7,50,40,14,8),(1,1,1,1,1)):
   g=mk(GPU(bpp));g.prefill();W,H,st=80,20,12;b=mono_src(W,H,st);g.u.mem_write(SRC,b);before=bytearray(g.snapshot());fg,bg=0x5A3C9E71&m,0x0F1E2D3C&m
   assert xq(g,1,fg=fg,bg=bg,d=(dx,dy,w,h),src=(SRC,1,sx,2,W,H,st))==1,(bpp,sx,dx)
   for y in range(h):
    for x in range(w):put(before,g,dx+x,dy+y,fg if bit(b,st,sx+x,2+y) else bg)
   assert g.snapshot()==bytes(before) and g.excluded==0,(bpp,sx,dx,dy,w,h);cases.append(f'{bpp}bpp XF_MONO {w}x{h} at {dx},{dy} from column {sx}: 1 bits fg, 0 bits bg, nothing else touched (box padded to 32 pixels and clipped)')
  g=mk(GPU(bpp));g.prefill();g.u.mem_write(SRC,mono_src(80,20,12));before=g.snapshot()
  assert xq(g,1,rop=0x88,d=(0,0,8,8),src=(SRC,1,0,0,80,20,12))==0 and g.snapshot()==before;cases.append(f'{bpp}bpp XF_MONO with a ROP other than SRCCOPY is handed back untouched')
  g.u.mem_write(g.addr('vx2d_mono_min'),pack([4096]));assert xq(g,1,d=(0,0,8,8),src=(SRC,1,0,0,80,20,12))==0 and back(g)[2]==9 and g.snapshot()==before;cases.append(f'{bpp}bpp XF_MONO below ACCEL_MONO_MIN is handed back (reason 9)')
  # --- XF_STAGE: colour and 1-bit sources with ROPs that read the screen ---
  for rop in (0x88,0x66,0xEE,0x22,0xB8,0xE2,0x99):
   for neg in (0,1):
    g=mk(GPU(bpp));g.prefill();W,H=30,12;bpx=bpp//8;st=((W*bpx+3)//4)*4;img=bytes(rnd.randrange(256) for _ in range(st*H))
    base=SRC+(st*(H-1) if neg else 0);stride=-st if neg else st;g.u.mem_write(SRC,img);before=bytearray(g.snapshot());pf=0x33CC55AA&m
    fl=2 if uses(rop,4) else 0;dx,dy,sx,sy,w,h=9,5,4,3,21,8
    assert xq(g,2,flags=fl,rop=rop,pfg=pf,d=(dx,dy,w,h),src=(base,bpp,sx,sy,W,H,stride))==1,(bpp,rop,neg)
    for y in range(h):
     row=(H-1-(sy+y)) if neg else sy+y
     for x in range(w):
      at=row*st+(sx+x)*bpx;s=int.from_bytes(img[at:at+bpx],'little');put(before,g,dx+x,dy+y,rop3(rop,pf,s,px(g,before,dx+x,dy+y),m))
    assert g.snapshot()==bytes(before) and g.excluded==0,(bpp,rop,neg);cases.append(f'{bpp}bpp XF_STAGE colour rop {rop:02x}{" (rows going up in memory)" if neg else ""}: staging block then the engine ROP copy, every pixel as the ROP3 gives')
  # .595: XF_COLPAT: the pattern operand is the 8x8 block of brush pixels, anchored to the screen
  for rop in (0xB8,0xE2,0xC0,0xCA):
   for (dx,dy,w,h) in ((9,5,21,8),(0,0,17,19),(13,11,30,12)):
    if not uses(rop,1):continue
    g=mk(GPU(bpp));g.prefill();W,H=40,24;bpx=bpp//8;st=((W*bpx+3)//4)*4;img=bytes(rnd.randrange(256) for _ in range(st*H));g.u.mem_write(SRC,img);before=bytearray(g.snapshot())
    t=g.request(4,rop=0xF0);t[3]=0
    for k in range(64):t[29+k]=(k*0x01030507+0x10203)&m
    sx,sy=4,3
    assert xq(g,2,flags=64,rop=rop,d=(dx,dy,w,h),src=(SRC,bpp,sx,sy,W,H,st),tpl=t)==1,(bpp,rop,back(g))
    for y in range(h):
     for x in range(w):
      at=(sy+y)*st+(sx+x)*bpx;sv=int.from_bytes(img[at:at+bpx],'little');put(before,g,dx+x,dy+y,rop3(rop,t[29+((dy+y)&7)*8+((dx+x)&7)]&m,sv,px(g,before,dx+x,dy+y),m))
    assert g.snapshot()==bytes(before) and g.excluded==0,(bpp,rop,dx,dy);cases.append(f'{bpp}bpp XF_STAGE with XF_COLPAT rop {rop:02x} {w}x{h} at {dx},{dy}: pattern pixel (x&7, y&7) of the screen, source from the staging block, every pixel as the ROP3 gives')
  g=mk(GPU(bpp));g.prefill();before=g.snapshot();t=g.request(4,rop=0xF0);t[3]=0
  assert xq(g,2,flags=64|2,rop=0xB8,d=(0,0,8,8),src=(SRC,bpp,0,0,8,8,32),tpl=t)==0 and back(g)[2]==14 and g.snapshot()==before
  assert xq(g,2,flags=64,rop=0x88,d=(0,0,8,8),src=(SRC,bpp,0,0,8,8,32),tpl=t)==0 and back(g)[2]==14
  g.u.mem_write(g.addr('vx2d_brush_off'),pack([0]));assert xq(g,2,flags=64,rop=0xB8,d=(0,0,8,8),src=(SRC,bpp,0,0,8,8,32),tpl=t)==0 and back(g)[2]==18 and g.snapshot()==before
  cases.append(f'{bpp}bpp XF_COLPAT with the fixed colour too, with a ROP without a pattern, or without the brush block: handed back untouched (reasons 14, 14, 18)')
  g=mk(GPU(bpp));g.prefill();W,H,st=40,16,8;b=mono_src(W,H,st);g.u.mem_write(SRC,b);before=bytearray(g.snapshot());fg,bg=0x00FF00FF&m,0x12345678&m
  assert xq(g,2,rop=0x88,fg=fg,bg=bg,d=(3,4,33,11),src=(SRC,1,5,2,W,H,st))==1
  for y in range(11):
   for x in range(33):put(before,g,3+x,4+y,rop3(0x88,0,fg if bit(b,st,5+x,2+y) else bg,px(g,before,3+x,4+y),m))
  assert g.snapshot()==bytes(before);cases.append(f'{bpp}bpp XF_STAGE 1-bit source SRCAND: expanded to fg/bg in the staging block, then ANDed onto the screen')
  g=mk(GPU(bpp));g.prefill();before=g.snapshot()
  assert xq(g,2,rop=0xF0,d=(0,0,8,8),src=(SRC,bpp,0,0,8,8,32))==0 and g.snapshot()==before;cases.append(f'{bpp}bpp XF_STAGE refuses a ROP without a source')
  assert xq(g,2,rop=0xB8,d=(0,0,8,8),src=(SRC,bpp,0,0,8,8,32))==0 and back(g)[2]==14;cases.append(f'{bpp}bpp XF_STAGE refuses a pattern ROP without the fixed colour (reason 14)')
  g.u.mem_write(g.addr('vx2d_stage_off'),pack([0]));assert xq(g,2,rop=0x88,d=(0,0,8,8),src=(SRC,bpp,0,0,8,8,32))==0 and back(g)[2]==13;cases.append(f'{bpp}bpp XF_STAGE without a staging block is handed back (reason 13)')
 # --- XF_SPANS ---
 for bpp in (8,16,32):
  m=(1<<bpp)-1
  for op,rop in ((1,0x5A),(1,0x55),(1,0xA0),(3,0x5A),(4,0x5A),(4,0xFA)):
   if op==3 and bpp==8:continue
   g=mk(GPU(bpp));g.prefill();before=bytearray(g.snapshot())
   t=g.request(op,rop=rop,fg=0x6B5A4D3C&m,bg=0x1F2E3D4C&m);t[3]=0;t[27:29]=[0x81241842,0xE77E3C99]
   for k in range(64):t[29+k]=(k*0x01030507+0x10203)&m
   if op==4:g.u.mem_write(g.addr('vx2d_brush_off'),pack([0xFFF000]))
   spans=[(3,2,40),(3,45,60),(9,0,64),(17,7,8),(30,11,53)]
   assert xq(g,4,spans=spans,tpl=t)==1 and back(g)[0]==5,(bpp,op,rop,back(g))
   for (y,a,b) in spans:
    for x in range(a,b):
     if op==1:p=t[23]&m
     elif op==3:bits=t[27+(y&7)//4]>>((3-(y&3))*8);p=(t[23] if (bits>>(x&7))&1 else t[24])&m
     else:p=t[29+(y&7)*8+(x&7)]&m
     put(before,g,x,y,rop3(rop,p,0,px(g,before,x,y),m))
   assert g.snapshot()==bytes(before) and g.excluded==0,(bpp,op,rop);cases.append(f'{bpp}bpp XF_SPANS {("","fill","","mono pattern","colour pattern")[op]} rop {rop:02x}: five spans, each pixel as the ROP3 gives, patterns anchored to the screen')
  # v10.118: rectangles (OS_POLYSCANLINE scans): 4 words per entry
  for op,rop in ((1,0x5A),(3,0x5A),(4,0xA0),(1,0xF0),(4,0xF0),(1,0x0F)): # v10.130: write-only ROPs on rectangle bands (ACCEL_HOST bit 32)
   if op==3 and bpp==8:continue
   g=mk(GPU(bpp));g.prefill();before=bytearray(g.snapshot())
   t=g.request(op,rop=rop,fg=0x6B5A4D3C&m,bg=0x1F2E3D4C&m);t[3]=0;t[27:29]=[0x81241842,0xE77E3C99]
   for k in range(64):t[29+k]=(k*0x01030507+0x10203)&m
   rects=[(3,2,40,9),(13,0,64,1),(20,7,30,17)];flat=[v for r in rects for v in r];sp=[]
   head=struct.pack('<9I4I6Ii I4iI I3II',1264,0x10000,4,16,g.bpp,0,0,0,0,0,0,1,1,SRC,1,0,0,8,8,4,0,0,0,0,0,0,MASKW,0,0,0,len(rects))
   g.u.mem_write(XQ,head+struct.pack('<384H',*(flat+[0]*(384-len(flat))))+pack(t));assert g.call('VX2D_xfer',(XQ,))==1,(bpp,op)
   for (y0,a,b,hh) in rects:
    for y in range(y0,y0+hh):
     for x in range(a,b):
      if op==1:p=t[23]&m
      elif op==3:bits=t[27+(y&7)//4]>>((3-(y&3))*8);p=(t[23] if (bits>>(x&7))&1 else t[24])&m
      else:p=t[29+(y&7)*8+(x&7)]&m
      put(before,g,x,y,rop3(rop,p,0,px(g,before,x,y),m))
   assert g.snapshot()==bytes(before),(bpp,op);cases.append(f'{bpp}bpp XF_SPANS with XF_RECTS {("","fill","","mono pattern","colour pattern")[op]} rop {rop:02x}: rectangles of several lines, patterns anchored to the screen')
  g=mk(GPU(bpp));g.prefill();before=g.snapshot();t=g.request(1,rop=0xF0,fg=5);t[3]=0
  assert xq(g,4,spans=[(3,2,40)],tpl=t)==0 and g.snapshot()==before;cases.append(f'{bpp}bpp XF_SPANS refuses a ROP that does not read the screen (DIBENG writes as fast)')
  t=g.request(1,rop=0x5A,fg=5);t[3]=0
  assert xq(g,4,spans=[(3,2,40),(50,1,2)],tpl=t)==0 and g.snapshot()==before;cases.append(f'{bpp}bpp XF_SPANS: a span off the screen hands the whole call back untouched')
 # --- v10.135: the 8bpp colour-pattern guard ---
 g=mk(GPU(8));g.u.mem_write(g.addr('vx2d_col8'),pack([0]));g.prefill();before=g.snapshot();t=g.request(4,rop=0x5A,fg=5);t[3]=0
 for k in range(64):t[29+k]=k&0xFF
 head=struct.pack('<9I4I6Ii I4iI I3II',1264,0x10000,4,16,8,0,0,0,0,0,0,1,1,SRC,1,0,0,8,8,4,0,0,0,0,0,0,MASKW,0,0,0,1)
 g.u.mem_write(XQ,head+struct.pack('<384H',*([3,2,40,9]+[0]*380))+pack(t));assert g.call('VX2D_xfer',(XQ,))==0 and g.snapshot()==before;cases.append('v10.137: with ACCEL_COL8_BANDS 0 an 8bpp colour-pattern band batch is handed back untouched (1, the default, draws it: the XF_RECTS checks above)')
 # --- v10.132: ACCEL_SPAN_WAIT variants draw the same rectangles ---
 snaps=[]
 for sw in (0,1,2,3):
  g=mk(GPU(32));g.call('VX2D_control',(0xF2000000|sw,ARENA+0x3000));g.prefill();t=g.request(1,rop=0x5A,fg=0x123456);t[3]=0
  rects=[(3,2,40,9),(13,0,64,1),(20,7,30,17),(40,10,50,3)];flat=[v for r in rects for v in r]
  head=struct.pack('<9I4I6Ii I4iI I3II',1264,0x10000,4,16,32,0,0,0,0,0,0,1,1,SRC,1,0,0,8,8,4,0,0,0,0,0,0,MASKW,0,0,0,len(rects))
  g.u.mem_write(XQ,head+struct.pack('<384H',*(flat+[0]*(384-len(flat))))+pack(t));assert g.call('VX2D_xfer',(XQ,))==1,sw;snaps.append(g.snapshot())
 assert all(x==snaps[0] for x in snaps);cases.append('v10.132: ACCEL_SPAN_WAIT 0 to 3 (full idle, none, one read, four reads between the rectangles of a batch) draw the same pixels on the model')
 # --- live mask ---
 g=mk(GPU(32));out=ARENA+0x3000;g.u.mem_write(MASKW,pack([0]));xq(g,1,d=(0,0,1,1),src=(SRC,1,0,0,8,8,4))
 g.call('VX2D_control',(0xF7000000|6,out));assert struct.unpack('<4I',bytes(g.u.mem_read(out,16)))[2]==6 and struct.unpack('<H',bytes(g.u.mem_read(MASKW,2)))[0]==6
 g.prefill();before=g.snapshot();g.u.mem_write(SRC,bytes(64));assert xq(g,1,d=(0,0,8,8),src=(SRC,1,0,0,8,8,4))==0 and back(g)[2]==3 and back(g)[1]==6 and g.snapshot()==before
 g.call('VX2D_control',(0xFFFFFFFF,out));assert struct.unpack('<H',bytes(g.u.mem_read(MASKW,2)))[0]==30;cases.append('live mask 0xF7000000|m: switches the kinds off at once, is written to the DRV\'s copy, and VX2D_REGISTRY restores ACCEL_HOST (30 by default: mono SRCCOPY off, save-under on, write-only ROPs on bands off)')
 g=mk(GPU(32));g.call('VX2D_control',(0xF7000000|30,out));g.prefill();before=g.snapshot();t=g.request(1,rop=0xF0,fg=5);t[3]=0
 head=struct.pack('<9I4I6Ii I4iI I3II',1264,0x10000,4,16,32,0,0,0,0,0,0,1,1,SRC,1,0,0,8,8,4,0,0,0,0,0,0,MASKW,0,0,0,1)
 g.u.mem_write(XQ,head+struct.pack('<384H',*([3,2,40,9]+[0]*380))+pack(t));assert g.call('VX2D_xfer',(XQ,))==0 and g.snapshot()==before;cases.append('without ACCEL_HOST bit 32 a COPYPEN rectangle band is handed back to DIBENG')
 # --- v10.123: gamma table ---
 for bpp in (32,16,8):
  g=GPU(bpp);ramp=[(255-i)*257 for i in range(256)]+[i*257 for i in range(256)]+[(i//2)*257 for i in range(256)]
  g.u.mem_write(SRC,struct.pack('<768H',*ramp));g.crt[0x33]=0x80 if bpp==8 else 0x05;r=g.call('VX2D_gamma_load',(SRC,))
  if bpp==8:assert r==0 and not g.crt[0x33]&0x80;cases.append('8bpp: the gamma load clears CR33 bit 7 and leaves the DAC (the palette) alone');continue
  hw=g.palhw;assert r==1 and g.crt[0x33]==0x85 and all(hw[i]==[ramp[i]>>8,ramp[256+i]>>8,ramp[512+i]>>8] for i in range(256)),(r,hex(g.crt[0x33]))
  out=ARENA+0x3000;g.call('VX2D_control',(0xFFFFFE1A,out));c=struct.unpack('<4I',bytes(g.u.mem_read(out,16)));assert c[0]==1 and c[1]==0x85 and c[3]==ramp[128]
  g.call('VX2D_control',(0xF6000000,out));assert not g.crt[0x33]&0x80
  cases.append(f'{bpp}bpp gamma: the 3 x 256 ramp goes to the DAC (high bytes, red green blue per entry), CR33 bit 7 set, the other CR33 bits kept, live switch 0 clears the bit')
 # --- v10.124: SaveScreenBitmap (XF_SAVE) ---
 for bpp in (16,32):
  g=mk(GPU(bpp));g.u.mem_write(g.addr('vx2d_save_off'),pack([0xA00000]));g.u.mem_write(g.addr('vx2d_host'),pack([31]))
  def sv(flags,d):head=struct.pack('<9I4I6Ii I4iI I3II',1264,0x10000,5,flags,g.bpp,0,0,0,0,*d,0,0,0,0,0,0,0,0,0,0,0,0,0,MASKW,0,0,0,0);g.u.mem_write(XQ,head+bytes(768)+bytes(372));return g.call('VX2D_xfer',(XQ,))
  g.prefill();before=g.snapshot();assert sv(1,(5,7,33,20))==1 and g.excluded==0
  for y in range(48):g.u.mem_write(FB+y*g.pitch,bytes(g.pitch)) # the menu is drawn over it
  assert sv(2,(5,7,33,20))==1 and g.excluded==0;after=g.snapshot()
  for y in range(48):
   for x in range(64):
    inr=5<=x<38 and 7<=y<27;at=y*g.pitch+x*g.px
    assert after[at:at+g.px]==(before[at:at+g.px] if inr else bytes(g.px)),(bpp,x,y)
  cases.append(f'{bpp}bpp XF_SAVE: the saved rectangle comes back exactly (engine copies to the save block and back), nothing outside it changes')
  assert sv(2,(6,7,33,20))==0 and back(g)[2]==25;cases.append(f'{bpp}bpp XF_SAVE: a restore of a rectangle that was not saved is refused (reason 25: USER redraws)')
  out=ARENA+0x3000;sv(4,(0,0,0,0));assert sv(2,(5,7,33,20))==0;cases.append(f'{bpp}bpp XF_SAVE: after a discard nothing is restored')
  g.u.mem_write(g.addr('vx2d_host'),pack([22]));assert sv(1,(5,7,33,20))==0 and back(g)[2]==3;cases.append(f'{bpp}bpp XF_SAVE off without ACCEL_HOST bit 8 (USER does it its own way)')
 # --- .530: the inverting and colour-XOR variants (ACCEL_HWCURSOR_INV, the mono unit) removed ---
 AM,XM,OFF=ARENA+0x4000,ARENA+0x6000,0xFF0000
 g=GPU(32);g.u.mem_write(g.addr('vx2d_hwcur_off'),pack([OFF]));g.call('VX2D_control',(0xF9000001,ARENA+0x3000))
 a=[0xFFFFFFFF]*1024;x=[0]*1024;a[0]=0;x[0]=0x123456;x[1]=0xFFFFFF # pixel 0 opaque, pixel 1 inverting, the rest transparent
 g.u.mem_write(AM,pack(a));g.u.mem_write(XM,pack(x))
 assert g.call('VX2D_hwcur_set',(AM,XM,32,4,4,32))==0;cases.append('an inverting cursor pixel stays with the software cursor')
 x[1]=0;g.u.mem_write(XM,pack(x));assert g.call('VX2D_hwcur_set',(AM,XM,32,4,4,32))==1
 img=struct.unpack('<3I',bytes(g.u.mem_read(FB+OFF,12)));assert img==(0xFF123456,0,0) and g.reg[0x120C]==0xFFFFFF,[hex(v) for v in img]
 cases.append('an opaque and transparent shape: the opaque pixel its colour with alpha FF, transparent 0, the inverse colour register white')
 assert g.call('VX2D_hwcur_move',(7,7))==1 and (g.reg[0x2F0]&1)==1
 assert g.call('VX2D_hwcur_set',(AM,XM,32,33,4,32))==0 # refused (wider than 32)
 assert (g.reg[0x2F0]&1)==0,hex(g.reg[0x2F0]);cases.append('v10.251: a refused shape turns the shown cursor off')
 # v10.252: an engine reset may clear the unit; invalidate forgets the state, the next set + move programs it and enables it again
 assert g.call('VX2D_hwcur_set',(AM,XM,32,4,4,32))==1 and g.call('VX2D_hwcur_move',(7,7))==1 and (g.reg[0x2F0]&1)==1
 g.reg[0x2F0]=0;g.reg[0x2F4]=0;assert g.call('VX2D_hwcur_move',(8,8))==1 and (g.reg[0x2F0]&1)==0 # the old bug: moves "succeed", the unit stays off
 g.call('VX2D_hwcur_invalidate',());assert g.call('VX2D_hwcur_active',())==0
 assert g.call('VX2D_hwcur_set',(AM,XM,32,4,4,32))==1 and g.call('VX2D_hwcur_move',(9,9))==1 and (g.reg[0x2F0]&1)==1 and g.reg[0x2F4]==OFF
 cases.append('v10.252: after the unit loses its registers, invalidate + set + move programs the image address and enables it again')
 # --- v10.279 (review): a NULL request writes nothing and leaves the critical section balanced; strides outside +-65536 (INT32_MIN included) are refused ---
 g=mk(GPU(32));low=bytes(g.u.mem_read(0,0x200));assert g.call('VX2D_xfer',(0,))==0 and g.critical==0 and bytes(g.u.mem_read(0,0x200))==low
 cases.append('VX2D_xfer(NULL): handed back, nothing written, critical section balanced')
 for bpp in (16,32):
  g=mk(GPU(bpp));g.prefill();g.u.mem_write(SRC,bytes(range(240)));before=g.snapshot()
  for st in (-2**31,2**31-1,65537,-65537):assert xq(g,1,d=(0,0,8,8),src=(SRC,1,0,0,80,20,st))==0 and g.snapshot()==before and g.critical==0,(bpp,st)
  assert xq(g,1,d=(0,0,8,8),src=(SRC+12*19,1,0,0,80,20,-12))==1
  cases.append(f'{bpp}bpp XF_MONO strides INT32_MIN, INT32_MAX and +-65537 are refused untouched; a negative stride in range still draws')
 return cases+drv_checks()

# ---- the DRV side (16-bit hooks in the harness with mocked DIBENG): what the hooks put in the OP_VX_XFER packet ----
from frontend import GDI
class XGDI(GDI):
 def __init__(self,bpp=32):
  self.xreqs=[];self.xreply=1;super().__init__(bpp)
  self.u.mem_write(0x20000+self.sym['_vx_host_mask'][1],struct.pack('<H',31)) # every kind on (the shipped default is 22)
 def api(self,u,a,z,_):
  op=u.reg_read(UC_X86_REG_EDX)&65535
  if op!=0x1135:return super().api(u,a,z,_)
  p=u.reg_read(UC_X86_REG_ESI);n=u.reg_read(UC_X86_REG_ECX);assert n==1264,n
  raw=bytes(u.mem_read(p,n));self.xreqs.append(raw);u.mem_write(p+28*4,struct.pack('<I',31));u.reg_write(UC_X86_REG_ECX,self.xreply)
from unicorn.x86_const import UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_ECX
def X(raw):
 h=struct.unpack('<9I4I6IiI4iII3II',raw[:124]);sp=struct.unpack('<384H',raw[124:892]);t=struct.unpack('<93I',raw[892:1264])
 names='cb abi op flags bpp rop fg bg pfg dx dy w h src_lin src_bits src_x src_y src_w src_h src_stride box_lin ol ot orr ob soft mask_lin done mask_out result n'.split()
 d=dict(zip(names,h));d['sp']=sp;d['tpl']=t;return d
def drv_checks():
 cases=[]
 for bpp in (16,32):
  m=(1<<bpp)-1;f=XGDI(bpp);A=lambda off:0x30000+off
  # a 1-bit GDI bitmap at 0x500 (its bits at 0x900), the draw mode at 0x400
  f.u.mem_write(0x30500,struct.pack('<hhhHBBHHIHHHHH',0,40,20,6,1,1,0x900,f.sel['argument'],0,0,0,0,0,0))
  f.u.mem_write(0x30400,struct.pack('<hhII',13,2,0x00ABCDEF,0x00123456))
  f.fallbacks=0;assert f.bitblt(rop=0x00CC0020,source=False)==1 and f.fallbacks==0 and len(f.xreqs)==1
  x=X(f.xreqs[-1]);assert (x['op'],x['rop'],x['fg'],x['bg'],x['dx'],x['dy'],x['w'],x['h'])==(1,0xCC,0x00ABCDEF&m,0x00123456&m,7,6,19,13),x
  assert (x['src_lin'],x['src_bits'],x['src_x'],x['src_y'],x['src_w'],x['src_h'],x['src_stride'])==(A(0x900),1,2,3,40,20,6) and x['mask_lin']==0x20000+f.sym['_vx_host_mask'][1],x
  cases.append(f'{bpp}bpp DRV BitBlt SRCCOPY from a 1-bit GDI bitmap: XF_MONO with 1 bits = background colour, 0 bits = text colour, the bitmap\'s linear address, stride and rectangle')
  f.xreply=0;f.fallbacks=0;assert f.bitblt(rop=0x00CC0020,source=False)==1 and f.fallbacks==1;f.xreply=1;cases.append(f'{bpp}bpp DRV BitBlt: a mono blit the VxD hands back goes to DIBENG')
  assert f.bitblt(rop=0x008800C6,source=False)==1 and X(f.xreqs[-1])['op']==2 and X(f.xreqs[-1])['rop']==0x88;cases.append(f'{bpp}bpp DRV BitBlt SRCAND from a 1-bit bitmap: XF_STAGE')
  n=len(f.xreqs);f.fallbacks=0;assert f.bitblt(rop=0x00330008,source=False)==1 and len(f.xreqs)==n and f.fallbacks==1;cases.append(f'{bpp}bpp DRV BitBlt NOTSRCCOPY from a 1-bit bitmap (no screen read): DIBENG')
  # a DIB engine bitmap in the screen's format: deType RP, bits at selector:0x1000, delta -256 (rows going up)
  f.u.mem_write(0x30500,struct.pack('<HHHHBBIiIIHHH',0x5250,30,20,256,1,bpp,0,-256,0,0x1000,f.sel['argument'],0x40 if bpp==16 else 0,0x400))
  f.u.mem_write(0x3001c,struct.pack('<H',0x8000|(0x40 if bpp==16 else 0)))
  assert f.bitblt(rop=0x00660046,source=False)==1;x=X(f.xreqs[-1]);assert (x['op'],x['rop'],x['src_bits'],x['src_lin'],x['src_stride'],x['src_w'],x['src_h'])==(2,0x66,bpp,A(0x1000),-256,30,20),x
  cases.append(f'{bpp}bpp DRV BitBlt SRCINVERT from a DIB engine bitmap of the screen\'s depth: XF_STAGE at the bitmap\'s address with its (negative) row delta')
  n=len(f.xreqs);f.fallbacks=0;assert f.bitblt(rop=0x00CC0020,source=False)==1 and len(f.xreqs)==n and f.fallbacks==1;cases.append(f'{bpp}bpp DRV BitBlt SRCCOPY of a colour bitmap stays with DIBENG (it writes as fast as the CPU can)')
  f.u.mem_write(0x30500,struct.pack('<HHHHBBIiIIHHH',0x5250,30,20,256,1,bpp,0,-256,0,0x1000,f.sel['argument'],0x0 if bpp==16 else 0,0x400))
  if bpp==16:
   n=len(f.xreqs);assert f.bitblt(rop=0x00660046,source=False)==1 and len(f.xreqs)==n;cases.append('16bpp DRV BitBlt from a 5-5-5 bitmap onto the 5-6-5 screen stays with DIBENG')
  f.u.mem_write(0x3001c,struct.pack('<H',0x8000))
  # scan lines: XOR with the solid brush at 0x200
  def scan(pts,rop2,clip=None):
   far=lambda off:struct.pack('<HH',off,f.sel['argument'])
   f.u.mem_write(0x30640,b''.join(struct.pack('<hh',*p) for p in pts));f.u.mem_write(0x30400,struct.pack('<hhII',rop2,1,0,0))
   if clip:f.u.mem_write(0x30620,struct.pack('<4h',*clip))
   args=(far(0x620) if clip else struct.pack('<HH',0,0))+far(0x400)+far(0x200)+far(0x700)+far(0x640)+struct.pack('<HH',len(pts),4)+far(0)
   f.dib_argc=len(args);f.fallbacks=0
   try:return f.call('Output',args,incoming_ds=f.sel['stack'])
   finally:f.dib_argc=None
  n=len(f.xreqs);assert scan([(3,9),(2,20),(30,70),(40,41)],7,clip=(0,0,64,48))==1 and f.fallbacks==0 and len(f.xreqs)==n+1
  x=X(f.xreqs[-1]);assert x['op']==4 and x['n']==3 and list(x['sp'][:9])==[9,2,20,9,30,64,9,40,41] and x['tpl'][2]==1 and x['tpl'][25]==0x5A and x['tpl'][3]==0,(x['n'],x['sp'][:9],x['tpl'][:26])
  cases.append(f'{bpp}bpp DRV Output OS_SCANLINES with R2_XORPEN: one XF_SPANS call, the segments clipped to the screen, the brush as a PATINVERT fill template')
  n=len(f.xreqs);assert scan([(1,9),(2,20)],13)==1 and len(f.xreqs)==n and f.fallbacks==1;cases.append(f'{bpp}bpp DRV Output OS_SCANLINES with R2_COPYPEN stays with DIBENG')
 return cases
