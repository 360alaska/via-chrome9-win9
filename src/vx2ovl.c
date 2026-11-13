/* VX2OVL v1.0: video overlay (driver v10.106).
 * The VX855's V1 video unit can show a YUY2 picture on top of the desktop, scaled up, with the desktop's own pixels deciding where it shows (colour key).
 * ACCEL_OVERLAY is 0 until this test has passed.
 * Mode 1 (ACCEL_OVERLAY is 0, which is the default): the program writes a colour bar picture into video memory, switches the driver's overlay on live and talks to
 * the V1 unit directly. It asks you after each step whether the picture looks right (Yes/No), trying the register set of the primary display and then the second
 * one's, and the two source width counts. With the first variant whose picture, enlargement and colour key are right it writes HKLM\Software\VX85598 ACCEL_OVERLAY.
 * Restart Windows, then run VX2OVL again:
 * Mode 2 (ACCEL_OVERLAY is set and DirectDraw reports the overlay): the same checks through DirectDraw (overlay surfaces, UpdateOverlay, colour key, Flip,
 * SetOverlayPosition). It writes nothing; the result tells whether the overlay may stay on.
 * Needs the desktop at 16 or 32 bits per pixel, 640x480 or larger. Esc stops. Writes VX2OVL.TXT and VX2OVL.RES. */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <io.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"
#include "vx855_2d.h"
static HANDLE dev=INVALID_HANDLE_VALUE;
static FILE *lf;
static HWND win,dlg;
static LPDIRECTDRAW dd;
static LPDIRECTDRAWSURFACE prim;
static DWORD depth,sw,sh,aborted,dlgres,keyrect;
#define PW 160
#define PH 120
#define KEY16 0xF81FUL
#define KEY32 0xFF00FFUL
typedef HRESULT (WINAPI *CREATE)(GUID FAR *,LPDIRECTDRAW FAR *,IUnknown FAR *);
static void logline(const char *fmt,...) {
 va_list ap; va_start(ap,fmt); vfprintf(lf,fmt,ap); va_end(ap); fflush(lf); _commit(_fileno(lf));
}
static void finish(const char *res) {
 FILE *f=fopen("VX2OVL.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}
 logline("%s\n",res);
}
static void pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
}
static void wait_ms(int ms) {DWORD t=GetTickCount(); while((int)(GetTickCount()-t)<ms&&!aborted) {pump(); Sleep(5);}}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_CLOSE) {aborted=1; return 0;}
 if(m==WM_PAINT) {
  PAINTSTRUCT ps; HDC dc=BeginPaint(w,&ps); RECT r; HBRUSH bg=CreateSolidBrush(RGB(0,40,90)),key=CreateSolidBrush(RGB(255,0,255));
  GetClientRect(w,&r); FillRect(dc,&r,bg);
  if(keyrect) {r.left=60; r.top=60; r.right=260; r.bottom=160; FillRect(dc,&r,key);}
  { /* reference colours drawn by the plain display path, to compare the overlay's colours with */
   static const COLORREF ref[8]={RGB(255,255,255),RGB(255,255,0),RGB(0,255,255),RGB(0,255,0),RGB(255,0,255),RGB(255,0,0),RGB(0,0,255),RGB(0,0,0)};
   int i; for(i=0;i<8;i++) {HBRUSH rb=CreateSolidBrush(ref[i]); r.left=20+i*40; r.right=r.left+40; r.top=300; r.bottom=360; FillRect(dc,&r,rb); DeleteObject(rb);}
   for(i=0;i<8;i++) {HBRUSH rb=CreateSolidBrush(RGB(i*36,i*36,i*36)); r.left=20+i*40; r.right=r.left+40; r.top=360; r.bottom=390; FillRect(dc,&r,rb); DeleteObject(rb);}
  }
  DeleteObject(bg); DeleteObject(key); EndPaint(w,&ps); return 0;
 }
 if(m==WM_ERASEBKGND) return 1;
 return DefWindowProc(w,m,a,b);
}
static LRESULT CALLBACK dwnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_COMMAND) {dlgres=LOWORD(a)==1?1:2; return 0;}
 if(m==WM_CLOSE) {aborted=1; return 0;}
 if(m==WM_PAINT) {
  PAINTSTRUCT ps; HDC dc=BeginPaint(w,&ps); RECT r; char t[600];
  GetClientRect(w,&r); FillRect(dc,&r,(HBRUSH)GetStockObject(LTGRAY_BRUSH));
  GetWindowTextA(w,t,sizeof(t)); r.left=10; r.top=6; r.right-=10; r.bottom-=40;
  SetBkMode(dc,TRANSPARENT); DrawTextA(dc,t,-1,&r,DT_WORDBREAK);
  EndPaint(w,&ps); return 0;
 }
 return DefWindowProc(w,m,a,b);
}
/* asks a Yes/No question in a small window at the bottom of the screen (the overlay stays above the desktop, clear of this window) */
static int ask(HINSTANCE hi,const char *q) {
 int x=(int)sw/2-290,y=(int)sh-150; HWND y1,n1;
 logline("   question: %s\n",q);
 dlgres=0;
 dlg=CreateWindowExA(WS_EX_TOPMOST,"VX2OVLD",q,WS_POPUP|WS_VISIBLE|WS_BORDER,x,y,580,140,win,NULL,hi,NULL);
 if(!dlg) {aborted=1; return 0;}
 y1=CreateWindowA("BUTTON","Yes",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,180,100,90,28,dlg,(HMENU)1,hi,NULL);
 n1=CreateWindowA("BUTTON","No",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,300,100,90,28,dlg,(HMENU)2,hi,NULL);
 (void)y1; (void)n1;
 while(!dlgres&&!aborted) {pump(); Sleep(10);}
 DestroyWindow(dlg); dlg=NULL; pump();
 logline("   answer: %s\n",aborted?"(stopped)":dlgres==1?"yes":"no");
 return dlgres==1&&!aborted;
}
static int ctl(DWORD code,DWORD *out) {
 DWORD o[4],got=0; memset(o,0,sizeof(o));
 if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0;
 if(out) memcpy(out,o,16);
 return 1;
}
static int ovl(vx_ovl_req *q,DWORD *r) {
 DWORD got=0; memset(r,0,4*VXO_REPLY_DWORDS);
 q->cb=sizeof(*q); q->abi=VXO_ABI;
 if(!DeviceIoControl(dev,VX_IOCTL_OVL,q,sizeof(*q),r,4*VXO_REPLY_DWORDS,&got,NULL)||got!=4*VXO_REPLY_DWORDS) return 0;
 return 1;
}
/* ---- pictures ---- */
static const BYTE bars[8][3]={{235,128,128},{210,16,146},{170,166,16},{145,54,34},{106,202,222},{81,90,240},{41,240,110},{16,128,128}};
static void yuy2(BYTE *base,DWORD pitch,int variant) {
 int x,y;
 for(y=0;y<PH;y++) {
  BYTE *p=base+(DWORD)y*pitch;
  for(x=0;x<PW;x+=2) {
   int b=x*8/PW; BYTE Y=0,U=128,V=128,Y1;
   if(variant) b=7-b;
   if(y<PH*3/4) {Y=bars[b][0]; U=bars[b][1]; V=bars[b][2];}
   else {Y=(BYTE)(16+x*219/PW); U=128; V=128;}
   Y1=Y;
   if(y<16&&x<16) {Y=Y1=235; U=V=128;} /* white marker at the top left */
   p[x*2]=Y; p[x*2+1]=U; p[x*2+2]=Y1; p[x*2+3]=V;
  }
 }
}
static void rgb565(BYTE *base,DWORD pitch,int variant) {
 int x,y; static const WORD c[8]={0xFFFF,0xFFE0,0x07FF,0x07E0,0xF81F,0xF800,0x001F,0x0000};
 for(y=0;y<PH;y++) {
  WORD *p=(WORD*)(base+(DWORD)y*pitch);
  for(x=0;x<PW;x++) {
   int b=x*8/PW; if(variant) b=7-b;
   p[x]=y<PH*3/4?c[b]:(WORD)(((x*31/PW)<<11)|((x*63/PW)<<5)|(x*31/PW));
   if(y<16&&x<16) p[x]=0xFFFF;
  }
 }
}
/* ---- raw mode: surfaces in video memory, the V1 unit driven through VX_IOCTL_OVL ---- */
typedef struct {LPDIRECTDRAWSURFACE s; DWORD offset,pitch;} mem;
static BYTE *primptr;
static int mkmem(mem *m,int variant,int fmt) {
 DDSURFACEDESC d; HRESULT hr; int w=(int)(PW*2/(depth/8));
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; d.dwWidth=w; d.dwHeight=PH; d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 hr=IDirectDraw_CreateSurface(dd,&d,&m->s,NULL);
 if(FAILED(hr)) {logline("  CreateSurface (picture memory) failed: %08lX\n",(DWORD)hr); return 0;}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 hr=IDirectDrawSurface_Lock(m->s,NULL,&d,DDLOCK_WAIT,NULL);
 if(FAILED(hr)) {logline("  Lock failed: %08lX\n",(DWORD)hr); return 0;}
 m->pitch=(DWORD)d.lPitch; m->offset=(DWORD)((BYTE*)d.lpSurface-primptr);
 if(fmt) rgb565((BYTE*)d.lpSurface,m->pitch,variant); else yuy2((BYTE*)d.lpSurface,m->pitch,variant);
 IDirectDrawSurface_Unlock(m->s,d.lpSurface);
 logline("  picture memory %d: video memory offset %08lX, pitch %lu\n",variant,m->offset,m->pitch);
 return 1;
}
static DWORD last_reason;
static int vxo(DWORD cmd,const mem *m,DWORD fmt,int sl,int st,int sr,int sb,int dl,int dt,int dr,int db,int key) {
 vx_ovl_req q; DWORD r[VXO_REPLY_DWORDS];
 memset(&q,0,sizeof(q)); q.cmd=cmd; q.fmt=fmt;
 if(m) {q.offset=m->offset; q.pitch=m->pitch; q.sw=PW; q.sh=PH;}
 q.sl=sl; q.st=st; q.sr=sr; q.sb=sb; q.dl=dl; q.dt=dt; q.dr=dr; q.db=db;
 if(key) {q.flags=VXO_KEY; q.key=depth==16?KEY16:KEY32;}
 if(!ovl(&q,r)) {logline("   the driver has no overlay request (VX85598.VXD older than v10.106)\n"); return 0;}
 last_reason=r[VXO_R_REASON];
 logline("   cmd %lu: result %lu reason %lu on %lu | V1 control %08lX compose %08lX start %08lX height/count %08lX\n",cmd,r[VXO_R_RESULT],r[VXO_R_REASON],r[VXO_R_ON],r[VXO_R_V1CTL],r[VXO_R_COMPOSE],r[VXO_R_START],r[VXO_R_SRCH]);
 return r[VXO_R_RESULT]!=1;
}
static void set_key_window(HINSTANCE hi,int on) {(void)hi; keyrect=on; InvalidateRect(win,NULL,TRUE); UpdateWindow(win); pump();}
/* returns a bit mask: 1 picture, 2 enlargement, 4 colour key, 8 flip, 16 move */
static DWORD raw_variant(HINSTANCE hi,DWORD m,mem *a,mem *b) {
 DWORD ok=0,c[4]; int i;
 ctl(0xF8000000UL|m,c);
 logline("-- variant: ACCEL_OVERLAY %lu (register set %lu, %s width count, key %s, flips %s)\n",m,m&3,(m&8)?"width":"width-1",(m&16)?"low bits replicated":"low bits zero",(m&32)?"plain address":"double buffer");
 if(c[0]!=m) {logline("   the driver did not take the live mask\n"); return 0;}
 set_key_window(hi,0);
 if(!vxo(VXO_UPDATE,a,VXO_FMT_YUY2,0,0,PW,PH,20,20,20+PW,20+PH,0)) {logline("   refused (reason %lu)\n",last_reason); return 0;}
 wait_ms(1200);
 if(!ask(hi,"A colour bar picture should be at the top left of the screen: 8 vertical bars (white, yellow, cyan, green, magenta, red, blue, black), a grey ramp underneath, and a small white square in its top left corner. Are the bars straight, in these colours and not shifted or torn? (Compare the colours with the reference row drawn lower on the screen.)")) {vxo(VXO_HIDE,NULL,0,0,0,0,0,0,0,0,0,0); return 0;}
 ok|=1;
 if(vxo(VXO_UPDATE,a,VXO_FMT_YUY2,0,0,PW,PH,20,20,20+2*PW,20+2*PH,0)) {
  wait_ms(1000);
  if(ask(hi,"The same picture is now twice as wide and twice as tall (320 x 240), with the same bars and a smooth look. Is it?")) ok|=2;
 }
 set_key_window(hi,1);
 if(vxo(VXO_UPDATE,a,VXO_FMT_YUY2,0,0,PW,PH,20,20,20+2*PW,20+2*PH,1)) {
  wait_ms(1000);
  if(ask(hi,"Now the picture should show only inside a 200 x 100 rectangle (inside the 320 x 240 area the dark blue background is visible around it). Is the bar picture visible only inside that rectangle, with dark blue elsewhere?")) ok|=4;
  else if(depth==16&&!(m&16)) {
   logline("   16 bpp: trying the key with replicated low bits\n"); ctl(0xF8000000UL|m|16,c);
   if(vxo(VXO_UPDATE,a,VXO_FMT_YUY2,0,0,PW,PH,20,20,20+2*PW,20+2*PH,1)) {wait_ms(800); if(ask(hi,"Again: the picture only inside the 200 x 100 rectangle, dark blue elsewhere?")) ok|=4|0x100;}
   if(!(ok&0x100)) ctl(0xF8000000UL|m,c);
  }
 }
 set_key_window(hi,0);
 if(ok&2) {
  vxo(VXO_UPDATE,a,VXO_FMT_YUY2,0,0,PW,PH,20,20,20+PW,20+PH,0); wait_ms(600);
  for(i=0;i<3&&!aborted;i++) {vxo(VXO_FLIP,b,VXO_FMT_YUY2,0,0,0,0,0,0,0,0,0); wait_ms(1500); vxo(VXO_FLIP,a,VXO_FMT_YUY2,0,0,0,0,0,0,0,0,0); wait_ms(1500);}
  if(ask(hi,"The picture has just changed three times between the colour bars and the same bars in reverse order (black first, white last). Did you see it change back and forth without flicker or damage?")) ok|=8;
  for(i=0;i<=10&&!aborted;i++) {vxo(VXO_MOVE,NULL,0,0,0,0,0,20+i*28,20+i*8,0,0,0); wait_ms(300);}
  if(ask(hi,"The picture has just moved in 10 steps from the top left to the right and down (each step 0.3 s) and now sits at the lower right. Did it move that way, without torn or leftover pieces?")) ok|=16;
 }
 vxo(VXO_HIDE,NULL,0,0,0,0,0,0,0,0,0,0); wait_ms(300);
 return ok;
}

/* ---- v10.108 extras: planar, RGB, shrink (HQV), source key, bob ---- */
static void fill_yuy2(BYTE *base,DWORD pitch,int w,int h) {
 int x,y;
 for(y=0;y<h;y++) {
  BYTE *p=base+(DWORD)y*pitch;
  for(x=0;x<w;x+=2) {
   int b=x*8/w; BYTE Y,U=128,V=128;
   if(y<h*3/4) {Y=bars[b][0]; U=bars[b][1]; V=bars[b][2];} else Y=(BYTE)(16+x*219/w);
   p[x*2]=Y; p[x*2+1]=U; p[x*2+2]=Y; p[x*2+3]=V;
  }
 }
}
static void fill_yuy2f(BYTE *base,DWORD pitch,int w,int h) { /* two interleaved fields: even lines the bars, odd lines the bars reversed */
 int x,y;
 for(y=0;y<h;y++) {
  BYTE *p=base+(DWORD)y*pitch;
  for(x=0;x<w;x+=2) {
   int b=x*8/w; BYTE Y,U=128,V=128;
   if(y&1) b=7-b;
   if(y<h*3/4) {Y=bars[b][0]; U=bars[b][1]; V=bars[b][2];} else Y=(BYTE)(16+x*219/w);
   p[x*2]=Y; p[x*2+1]=U; p[x*2+2]=Y; p[x*2+3]=V;
  }
 }
}
static void fill_yv12(BYTE *base,DWORD pitch,int w,int h) {
 int x,y; BYTE *vp=base+(DWORD)h*pitch,*up=vp+(DWORD)(h/2)*(pitch/2);
 for(y=0;y<h;y++) for(x=0;x<w;x++) {int b=x*8/w; base[(DWORD)y*pitch+x]=(y<h*3/4)?bars[b][0]:(BYTE)(16+x*219/w);}
 for(y=0;y<h/2;y++) for(x=0;x<w/2;x++) {
  int b=x*8/(w/2); BYTE U=128,V=128; if(y<(h/2)*3/4) {U=bars[b][1]; V=bars[b][2];}
  up[(DWORD)y*(pitch/2)+x]=U; vp[(DWORD)y*(pitch/2)+x]=V;
 }
}
static void fill_rgb32(BYTE *base,DWORD pitch,int w,int h) {
 int x,y; static const DWORD c[8]={0xFFFFFF,0xFFFF00,0x00FFFF,0x00FF00,0xFF00FF,0xFF0000,0x0000FF,0x000000};
 for(y=0;y<h;y++) {DWORD *p=(DWORD*)(base+(DWORD)y*pitch); for(x=0;x<w;x++) {int b=x*8/w; p[x]=y<h*3/4?c[b]:(DWORD)(((x*255/w)<<16)|((x*255/w)<<8)|(x*255/w));}}
}
static void fill_rgb565(BYTE *base,DWORD pitch,int w,int h) {
 int x,y; static const WORD c[8]={0xFFFF,0xFFE0,0x07FF,0x07E0,0xF81F,0xF800,0x001F,0x0000};
 for(y=0;y<h;y++) {WORD *p=(WORD*)(base+(DWORD)y*pitch); for(x=0;x<w;x++) {int b=x*8/w; p[x]=y<h*3/4?c[b]:(WORD)(((x*31/w)<<11)|((x*63/w)<<5)|(x*31/w));}}
}
/* a picture of bytes*rows bytes in video memory; kind 0 YUY2, 1 YV12, 2 RGB32, 3 RGB565 */
static int mkpic(mem *m,int kind,int w,int h) {
 DDSURFACEDESC d; HRESULT hr; DWORD bytes,rows=(DWORD)h,wp; int bpp=depth/8;
 bytes=(kind==2)?(DWORD)w*4:(kind==1)?(DWORD)w:(DWORD)w*2;
 if(kind==1) rows=(DWORD)h*3/2;
 wp=(bytes+bpp-1)/bpp; if(kind==1&&(wp*bpp)&31) wp=((wp*bpp+31)&~31UL)/bpp;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; d.dwWidth=wp; d.dwHeight=rows; d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 hr=IDirectDraw_CreateSurface(dd,&d,&m->s,NULL);
 if(FAILED(hr)) {logline("  CreateSurface (extra picture) failed: %08lX\n",(DWORD)hr); return 0;}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 if(FAILED(IDirectDrawSurface_Lock(m->s,NULL,&d,DDLOCK_WAIT,NULL))) return 0;
 m->pitch=(DWORD)d.lPitch; m->offset=(DWORD)((BYTE*)d.lpSurface-primptr);
 if(kind==0) fill_yuy2((BYTE*)d.lpSurface,m->pitch,w,h); else if(kind==4) fill_yuy2f((BYTE*)d.lpSurface,m->pitch,w,h); else if(kind==1) fill_yv12((BYTE*)d.lpSurface,m->pitch,w,h);
 else if(kind==2) fill_rgb32((BYTE*)d.lpSurface,m->pitch,w,h); else fill_rgb565((BYTE*)d.lpSurface,m->pitch,w,h);
 IDirectDrawSurface_Unlock(m->s,d.lpSurface);
 return 1;
}
static int vxo2(DWORD fmt,const mem *m,int sw,int sh,DWORD flags,DWORD ckv,int dl,int dt,int dr,int db) {
 vx_ovl_req q; DWORD r[VXO_REPLY_DWORDS];
 memset(&q,0,sizeof(q)); q.cmd=VXO_UPDATE; q.fmt=fmt; q.offset=m->offset; q.pitch=m->pitch; q.sw=sw; q.sh=sh;
 q.sl=0; q.st=0; q.sr=sw; q.sb=sh; q.dl=dl; q.dt=dt; q.dr=dr; q.db=db; q.flags=flags; q.ckl=ckv; q.ckh=ckv; if(flags&VXO_ALPHA) q.key=ckv<<24; if(flags&VXO_KEY) q.key=depth==16?KEY16:KEY32;
 if(!ovl(&q,r)) return 0;
 last_reason=r[VXO_R_RESULT]==1?r[VXO_R_REASON]:0;
 logline("   update fmt %lu %dx%d flags %lu: result %lu reason %lu hqv %lu | V1 control %08lX compose %08lX start %08lX 3D0 %08lX\n",fmt,sw,sh,flags,r[VXO_R_RESULT],r[VXO_R_REASON],r[VXO_R_HQV],r[VXO_R_V1CTL],r[VXO_R_COMPOSE],r[VXO_R_START],r[VXO_R_HQVCTL]);
 return r[VXO_R_RESULT]!=1;
}
static void one_extra(HINSTANCE hi,DWORD *bm,DWORD bit,const char *name,int kind,DWORD fmt,int sw,int sh,DWORD flags,DWORD ckv,int dw,int dh,const char *q) {
 mem p; DWORD o[4]; int ok=0;
 memset(&p,0,sizeof(p));
 logline("-- extra: %s (mask bit %lu)\n",name,bit);
 if(aborted) return;
 if(!mkpic(&p,kind,sw,sh)) {logline("   %s: no video memory for the picture\n",name); return;}
 ctl(0xF8000000UL|*bm|bit,o);
 if(vxo2(fmt,&p,sw,sh,flags,ckv,20,20,20+dw,20+dh)) {
  wait_ms(1200);
  {char qq[900]; sprintf(qq,"%s (Compare the colours with the reference row drawn lower on the screen.)",q); if(ask(hi,qq)) ok=1;}
  logline("   %s: %s\n",name,ok?"yes":"no");
 } else logline("   %s refused (reason %lu)\n",name,last_reason);
 vxo(VXO_HIDE,NULL,0,0,0,0,0,0,0,0,0,0);
 ctl(0xF8000000UL|*bm,o);
 if(ok) *bm|=bit;
 IDirectDrawSurface_Release(p.s);
}
static void bob_fields(HINSTANCE hi,DWORD *bm,const char *name,DWORD bit,int sw,int sh,int dw,int dh) {
 mem p; DWORD o[4]; int ok=0,i;
 memset(&p,0,sizeof(p));
 logline("-- extra: %s (mask bits %lu)\n",name,bit);
 if(aborted) return;
 if(!mkpic(&p,4,sw,sh)) {logline("   %s: no video memory for the picture\n",name); return;}
 ctl(0xF8000000UL|*bm|bit,o);
 if(vxo2(VXO_FMT_YUY2,&p,sw,sh,VXO_BOB|VXO_INTERLEAVED,0,20,20,20+dw,20+dh)) {
  wait_ms(1500);
  for(i=0;i<4&&!aborted;i++) {vxo(VXO_FLIP,&p,VXO_FMT_YUY2,0,0,sw,sh,0,0,0,0,0); wait_ms(1500);}
  {char qq[700]; sprintf(qq,"The picture holds two interleaved fields: the even lines are the bars (white first), the odd lines the reversed bars (black first). The overlay showed one field at a time, changing at each of 4 flips (1.5 s each): bars, reversed bars, bars, reversed bars, bars. Did it alternate like that, each time a clean full-height picture without combing or line doubling?"); if(ask(hi,qq)) ok=1;}
  logline("   %s: %s\n",name,ok?"yes":"no");
 } else logline("   %s refused (reason %lu)\n",name,last_reason);
 vxo(VXO_HIDE,NULL,0,0,0,0,0,0,0,0,0,0);
 ctl(0xF8000000UL|*bm,o);
 if(ok) *bm|=bit;
 IDirectDrawSurface_Release(p.s);
}
static void extras(HINSTANCE hi,DWORD *bm) {
 set_key_window(hi,0);
 one_extra(hi,bm,128,"YV12 planar",1,VXO_FMT_YV12,PW,PH,0,0,PW,PH,"YV12 (planar) source: the same colour bar picture (white, yellow, cyan, green, magenta, red, blue, black, grey ramp) at the top left, at the same size. Correct colours, nothing shifted?");
 one_extra(hi,bm,64,"shrink (HQV)",0,VXO_FMT_YUY2,2*PW,2*PH,0,0,PW,PH,"A 320 x 240 picture is now shown shrunk to 160 x 120 (HQV). Same bars, clean, not torn or smeared?");
 one_extra(hi,bm,64,"RGB565 (HQV)",3,VXO_FMT_RGB16,PW,PH,0,0,PW,PH,"RGB 5-6-5 source: the same colour bar picture. Do the bars have the right colours (white, yellow, cyan, green, magenta, red, blue, black)?");
 one_extra(hi,bm,64,"RGB32 (HQV)",2,VXO_FMT_RGB32,PW,PH,0,0,PW,PH,"32-bit RGB source: the same colour bar picture. Right colours again?");
 one_extra(hi,bm,256,"source colour key",0,VXO_FMT_YUY2,PW,PH,VXO_SRCKEY,0x913622UL,2*PW,2*PH,"Source colour key on the green bar: the picture is shown at 320 x 240 and the green bar should be see-through (desktop blue instead of green). Correct, the other bars unchanged?");
 one_extra(hi,bm,1024,"constant alpha",0,VXO_FMT_YUY2,PW,PH,VXO_ALPHA,0x80,2*PW,2*PH,"Constant alpha 50%: the 320 x 240 bar picture should look half transparent, with the dark blue desktop shining through it (the colours are darker and mixed with blue). Is it translucent like that?");
 bob_fields(hi,bm,"bob, interleaved fields (V1)",512,PW,PH,PW,PH);
 bob_fields(hi,bm,"bob, interleaved fields with shrink (HQV)",512|64,2*PW,2*PH,PW,PH);
 set_key_window(hi,0);
}

/* ---- probe mode (VX2OVL ALPHA): tries register combinations for the alpha blend, one question each ---- */
static DWORD regw(DWORD cmd,DWORD off,DWORD val,DWORD fl) {
 vx_ovl_req q; DWORD r[VXO_REPLY_DWORDS];
 memset(&q,0,sizeof(q)); q.cmd=cmd; q.offset=off; q.key=val; q.flags=fl;
 if(!ovl(&q,r)||r[VXO_R_RESULT]==1) {logline("   register %03lX: refused (reason %lu)\n",off,r[VXO_R_REASON]); return 0xDEADBEEFUL;}
 return r[VXO_R_UPDATES];
}
/* one probe: a plain V1 overlay (no key unless asked), then the listed 0x210 / window writes, V1 update fired after the last write */
static void probe_one(HINSTANCE hi,const char *name,int key,int shrink,const DWORD *w,int n) {
 mem p; int i,ok=0; DWORD rb,orig260=0,orig298=0,val;
 memset(&p,0,sizeof(p));
 logline("-- probe: %s\n",name);
 if(aborted) return;
 if(!mkpic(&p,0,shrink?2*PW:PW,shrink?2*PH:PH)) {logline("   no video memory\n"); return;}
 set_key_window(hi,key);
 if(!vxo2(VXO_FMT_YUY2,&p,shrink?2*PW:PW,shrink?2*PH:PH,key?VXO_KEY:0,0,20,20,20+(shrink?PW:2*PW),20+(shrink?PH:2*PH))) {logline("   refused (reason %lu)\n",last_reason); IDirectDrawSurface_Release(p.s); return;}
 orig260=regw(VXO_REGR,0x260,0,0); orig298=regw(VXO_REGR,0x298,0,0);
 logline("   before: 220=%08lX 264=%08lX 228=%08lX 210=%08lX 260=%08lX 298=%08lX\n",regw(VXO_REGR,0x220,0,0),regw(VXO_REGR,0x264,0,0),regw(VXO_REGR,0x228,0,0),regw(VXO_REGR,0x210,0,0),orig260,regw(VXO_REGR,0x298,0,0));
 for(i=0;i<n;i++) {
  val=w[2*i+1]; /* 0x298: the value is the bits to CLEAR in its original; 0x260: the bits to SET in its original */
  if(w[2*i]==0x298UL) val=orig298&~val; else if(w[2*i]==0x260UL) val=orig260|val;
  rb=regw(VXO_REGW,w[2*i],val,(i==n-1)?VXO_FIRE:0); logline("   write %03lX = %08lX%s, read back %08lX\n",w[2*i],val,(i==n-1)?" + fire":"",rb);
 }
 logline("   after: 210=%08lX 298=%08lX\n",regw(VXO_REGR,0x210,0,0),regw(VXO_REGR,0x298,0,0));
 wait_ms(1500);
 if(ask(hi,"Probe: is the picture now different from a normal fully opaque picture (see-through, darker, mixed with the blue desktop, or gone)?")) ok=1;
 logline("   %s: %s\n",name,ok?"YES, the picture changed":"no");
 regw(VXO_REGW,0x208,0,0); regw(VXO_REGW,0x20C,0,0); regw(VXO_REGW,0x224,0,0); regw(VXO_REGW,0x260,orig260,0); regw(VXO_REGW,0x298,orig298&~0xC0000000UL,0); regw(VXO_REGW,0x210,0xFF00,VXO_FIRE);
 vxo(VXO_HIDE,NULL,0,0,0,0,0,0,0,0,0,0);
 set_key_window(hi,0);
 IDirectDrawSurface_Release(p.s);
}
static void probe_mode(HINSTANCE hi) {
 DDSURFACEDESC d; HRESULT hr; DWORD o[4];
 DWORD W0=(20UL<<16)|20UL, W1=((20UL+2*PW-1)<<16)|(20UL+2*PH-1);
 DWORD h1[4],h2[8],h3[8],h4[10],h5[4],h6[4];
 /* 0x298 entries: bits to clear (0x100000 = bit 20, "V3 on top" -> V1 on top); 0x260 entries: bits to set (4 = HI blending enable) */
 h1[0]=0x210; h1[1]=0x00088000UL; h1[2]=0x298; h1[3]=0x00100000UL;
 h2[0]=0x210; h2[1]=0x00088000UL; h2[2]=0x208; h2[3]=W0; h2[4]=0x20C; h2[5]=W1; h2[6]=0x298; h2[7]=0x00100000UL;
 h3[0]=0x210; h3[1]=0x00088000UL; h3[2]=0x208; h3[3]=W0; h3[4]=0x20C; h3[5]=W1; h3[6]=0x260; h3[7]=4UL;
 h4[0]=0x210; h4[1]=0x00088000UL; h4[2]=0x208; h4[3]=W0; h4[4]=0x20C; h4[5]=W1; h4[6]=0x260; h4[7]=4UL; h4[8]=0x298; h4[9]=0x00100000UL;
 h5[0]=0x210; h5[1]=0x00088002UL; h5[2]=0x298; h5[3]=0x00100000UL;
 h6[0]=0x210; h6[1]=0x00088000UL; h6[2]=0x298; h6[3]=0x00100000UL;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 hr=IDirectDrawSurface_Lock(prim,NULL,&d,DDLOCK_WAIT,NULL);
 if(FAILED(hr)) {finish("FAIL: could not lock the primary surface"); return;}
 primptr=(BYTE*)d.lpSurface; IDirectDrawSurface_Unlock(prim,d.lpSurface);
 ctl(0xF8000000UL|961UL|2048UL,o);
 logline("probe mode: live mask %lu\n",o[0]);
 if(!(o[0]&2048UL)) {finish("FAIL: the driver does not have the probe commands (older VX85598.VXD)"); return;}
 probe_one(hi,"H1: 0x210=50% and 0x298 bit 20 cleared (V1 on top), fire",0,0,h1,2);
 probe_one(hi,"H2: H1 plus alpha window",0,0,h2,4);
 probe_one(hi,"H3: 0x210=50%, window, 0x260 bit 2 set (HI blending), fire",0,0,h3,4);
 probe_one(hi,"H4: H3 plus 0x298 bit 20 cleared",0,0,h4,5);
 probe_one(hi,"H5: graphics-stream select with 0x298 bit 20 cleared",0,0,h5,2);
 probe_one(hi,"H6: H1 with the colour key on",1,0,h6,2);
 ctl(0xFFFFFFFFUL,NULL);
 finish("PROBE done: nothing written to the registry; send VX2OVL.TXT");
}
static int forceraw,forceprobe;
static void raw_mode(HINSTANCE hi) {
 mem a,b,c; DWORD m,ok,best=0,bestm=0,v,orig[4];
 static const DWORD variants[4]={1,2,0,0}; DWORD bsc=0,sc;
 char res[300]; int i; HRESULT hr; DDSURFACEDESC d;
 memset(&a,0,sizeof(a)); memset(&b,0,sizeof(b)); memset(&c,0,sizeof(c));
 ctl(0xFFFFFE17UL,orig);
 logline("raw mode: live mask at start %lu (registry ACCEL_OVERLAY)\n",orig[0]);
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 hr=IDirectDrawSurface_Lock(prim,NULL,&d,DDLOCK_WAIT,NULL);
 if(FAILED(hr)) {finish("FAIL: could not lock the primary surface"); return;}
 primptr=(BYTE*)d.lpSurface; IDirectDrawSurface_Unlock(prim,d.lpSurface);
 if(!mkmem(&a,0,0)||!mkmem(&b,1,0)) {finish("FAIL: no video memory for the test pictures"); return;}
 for(i=0;i<2&&!aborted;i++) {
  m=variants[i]; ok=raw_variant(hi,m,&a,&b);
  logline("   variant %lu: picture %s, enlargement %s, colour key %s, flip %s, move %s\n",m,(ok&1)?"yes":"no",(ok&2)?"yes":"no",(ok&4)?"yes":"no",(ok&8)?"yes":"no",(ok&16)?"yes":"no");
  sc=0; if((ok&7)==7) sc=10+((ok&8)?4:0)+((ok&16)?2:0);
  if(sc>bsc) {bsc=sc; best=ok; bestm=m|((ok&0x100)?16:0);}
  if(sc==16) break;
 }
 if(best&&!aborted) extras(hi,&bestm);
 ctl(0xFFFFFFFFUL,NULL); /* registry values again (this also hides the overlay) */
 if(aborted) {finish("STOPPED: nothing written"); return;}
 if(!best) {finish("FAIL: no register set gave a correct picture, enlargement and colour key (nothing written)"); return;}
 v=bestm;
 sprintf(res,"%s: register set %lu (flip %s, move %s); .533: nothing written (the driver reads no ACCEL_OVERLAY); run VX2OVL again for the DirectDraw check",((best&24)==24)?"PASS":"PARTIAL",v,(best&8)?"ok":"FAILED",(best&16)?"ok":"FAILED");
 finish(res);
}
/* ---- DirectDraw mode ---- */
static LPDIRECTDRAWSURFACE ofront,oback;
static int ddshow(int dl,int dt,int dr,int db,int key) {
 RECT s,d; HRESULT hr; DWORD f=DDOVER_SHOW;
 s.left=0; s.top=0; s.right=PW; s.bottom=PH; d.left=dl; d.top=dt; d.right=dr; d.bottom=db;
 if(key) f|=DDOVER_KEYDEST;
 hr=IDirectDrawSurface_UpdateOverlay(ofront,&s,prim,&d,f,NULL);
 logline("   UpdateOverlay (%d,%d)-(%d,%d)%s: %08lX\n",dl,dt,dr,db,key?" with key":"",(DWORD)hr);
 return SUCCEEDED(hr);
}
/* ---- v10.112: the same extras through DirectDraw (HAL paths): kind 0 YUY2, 1 YV12, 2 RGB32, 3 RGB565, 4 YUY2 interleaved fields (bob, with a back buffer) ---- */
static int dd_extra(HINSTANCE hi,const char *name,int kind,int sw_,int sh_,int dw,int dh,DWORD oflags,const char *q) {
 DDSURFACEDESC d; HRESULT hr; LPDIRECTDRAWSURFACE f=NULL,b=NULL; DDOVERLAYFX fx; RECT s,r; int ok=0,i,back=(kind==4);
 logline("-- DirectDraw extra: %s\n",name);
 if(aborted) return 0;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT; d.dwWidth=(DWORD)sw_; d.dwHeight=(DWORD)sh_;
 d.ddsCaps.dwCaps=DDSCAPS_OVERLAY|DDSCAPS_VIDEOMEMORY;
 if(back) {d.dwFlags|=DDSD_BACKBUFFERCOUNT; d.dwBackBufferCount=1; d.ddsCaps.dwCaps|=DDSCAPS_FLIP|DDSCAPS_COMPLEX;}
 d.ddpfPixelFormat.dwSize=sizeof(d.ddpfPixelFormat);
 if(kind==0||kind==4) {d.ddpfPixelFormat.dwFlags=DDPF_FOURCC; d.ddpfPixelFormat.dwFourCC=0x32595559UL;}
 else if(kind==1) {d.ddpfPixelFormat.dwFlags=DDPF_FOURCC; d.ddpfPixelFormat.dwFourCC=0x32315659UL;}
 else if(kind==2) {d.ddpfPixelFormat.dwFlags=DDPF_RGB; d.ddpfPixelFormat.dwRGBBitCount=32; d.ddpfPixelFormat.dwRBitMask=0xFF0000UL; d.ddpfPixelFormat.dwGBitMask=0xFF00UL; d.ddpfPixelFormat.dwBBitMask=0xFFUL;}
 else {d.ddpfPixelFormat.dwFlags=DDPF_RGB; d.ddpfPixelFormat.dwRGBBitCount=16; d.ddpfPixelFormat.dwRBitMask=0xF800UL; d.ddpfPixelFormat.dwGBitMask=0x7E0UL; d.ddpfPixelFormat.dwBBitMask=0x1FUL;}
 hr=IDirectDraw_CreateSurface(dd,&d,&f,NULL);
 logline("   CreateSurface: %08lX\n",(DWORD)hr);
 if(FAILED(hr)||!f) return 0;
 if(back) {DDSCAPS sc; memset(&sc,0,sizeof(sc)); sc.dwCaps=DDSCAPS_BACKBUFFER; if(FAILED(IDirectDrawSurface_GetAttachedSurface(f,&sc,&b))) b=NULL;}
 for(i=0;i<2;i++) {
  LPDIRECTDRAWSURFACE t=i?b:f; DDSURFACEDESC l;
  if(!t) continue;
  memset(&l,0,sizeof(l)); l.dwSize=sizeof(l);
  if(FAILED(IDirectDrawSurface_Lock(t,NULL,&l,DDLOCK_WAIT,NULL))) {logline("   Lock failed\n"); goto done;}
  if(kind==0) fill_yuy2((BYTE*)l.lpSurface,(DWORD)l.lPitch,sw_,sh_); else if(kind==4) fill_yuy2f((BYTE*)l.lpSurface,(DWORD)l.lPitch,sw_,sh_);
  else if(kind==1) fill_yv12((BYTE*)l.lpSurface,(DWORD)l.lPitch,sw_,sh_); else if(kind==2) fill_rgb32((BYTE*)l.lpSurface,(DWORD)l.lPitch,sw_,sh_); else fill_rgb565((BYTE*)l.lpSurface,(DWORD)l.lPitch,sw_,sh_);
  IDirectDrawSurface_Unlock(t,l.lpSurface);
 }
 s.left=0; s.top=0; s.right=sw_; s.bottom=sh_; r.left=20; r.top=20; r.right=20+dw; r.bottom=20+dh;
 memset(&fx,0,sizeof(fx)); fx.dwSize=sizeof(fx);
 if(oflags&DDOVER_KEYSRCOVERRIDE) fx.dckSrcColorkey.dwColorSpaceLowValue=fx.dckSrcColorkey.dwColorSpaceHighValue=0x91UL|(0x36UL<<8)|(0x91UL<<16)|(0x22UL<<24); /* YUY2 pixel Y0 U Y1 V of the green bar */
 {
  static const DWORD bobf[3]={0x00200000UL|0x00800000UL,0x00200000UL|0x00800000UL|0x01000000UL,0x00200000UL}; /* BOB|INTERLEAVED, with BOBHARDWARE, BOB alone: the runtime decides which it takes */
  vx_ovl_req qq; DWORD rr[VXO_REPLY_DWORDS],ref0=0; int n=(kind==4)?3:1,t;
  memset(&qq,0,sizeof(qq)); qq.cmd=VXO_QUERY; if(ovl(&qq,rr)) ref0=rr[VXO_R_REFUSED];
  for(t=0;t<n;t++) {
   DWORD fl=(kind==4)?(DDOVER_SHOW|bobf[t]):(DDOVER_SHOW|oflags);
   hr=IDirectDrawSurface_UpdateOverlay(f,&s,prim,&r,fl,&fx);
   logline("   UpdateOverlay %dx%d -> %dx%d flags %08lX: %08lX\n",sw_,sh_,dw,dh,fl,(DWORD)hr);
   if(FAILED(hr)) {
    memset(&qq,0,sizeof(qq)); qq.cmd=VXO_QUERY;
    if(ovl(&qq,rr)) logline("   driver view: refusals %lu -> %lu, last refusal reason %lu, mask %lu (0 refusals = DirectDraw or the HAL said no before the VxD)\n",ref0,rr[VXO_R_REFUSED],rr[VXO_R_REASON],rr[VXO_R_MASK]);
   } else break;
  }
 }
 if(SUCCEEDED(hr)) {
  wait_ms(1500);
  if(kind==4) for(i=0;i<4&&!aborted;i++) { /* v1.3: odd, even, odd, even: the field the new buffer shows */
   DWORD ff=DDFLIP_WAIT|((i&1)?DDFLIP_EVEN:DDFLIP_ODD);
   hr=IDirectDrawSurface_Flip(f,NULL,ff);
   if(FAILED(hr)) {logline("   Flip %s: %08lX, retry without field flag\n",(i&1)?"EVEN":"ODD",(DWORD)hr); hr=IDirectDrawSurface_Flip(f,NULL,DDFLIP_WAIT);}
   logline("   Flip %s: %08lX\n",(i&1)?"EVEN":"ODD",(DWORD)hr); wait_ms(1500);}
  {char qq[900]; sprintf(qq,"%s (Compare the colours with the reference row drawn lower on the screen.)",q); if(ask(hi,qq)) ok=1;}
  logline("   %s: %s\n",name,ok?"yes":"no");
 }
 IDirectDrawSurface_UpdateOverlay(f,NULL,prim,NULL,DDOVER_HIDE,NULL);
done:
 if(b) IDirectDrawSurface_Release(b);
 if(f) IDirectDrawSurface_Release(f);
 wait_ms(300);
 return ok;
}
static DWORD dd_extras(HINSTANCE hi) { /* bit mask of what passed: 1 YV12, 2 shrink, 4 RGB565, 8 RGB32, 16 source key, 32 bob */
 DWORD ok=0;
 set_key_window(hi,0);
 if(dd_extra(hi,"YV12 planar",1,PW,PH,PW,PH,0,"YV12 (planar) overlay through DirectDraw: the colour bar picture at the top left, same size. Correct colours, nothing shifted?")) ok|=1;
 if(dd_extra(hi,"shrink",0,2*PW,2*PH,PW,PH,0,"A 320 x 240 YUY2 overlay surface is shown shrunk to 160 x 120. Same bars, clean, not torn or smeared?")) ok|=2;
 if(dd_extra(hi,"RGB565",3,PW,PH,PW,PH,0,"RGB 5-6-5 overlay surface: the colour bar picture. Right colours (white, yellow, cyan, green, magenta, red, blue, black)?")) ok|=4;
 if(dd_extra(hi,"RGB32",2,PW,PH,PW,PH,0,"32-bit RGB overlay surface: the colour bar picture. Right colours again?")) ok|=8;
 if(dd_extra(hi,"source colour key",0,PW,PH,2*PW,2*PH,DDOVER_KEYSRCOVERRIDE,"Source colour key on the green bar: the 320 x 240 picture is shown and the green bar should be see-through (desktop blue instead of green). Correct, the other bars unchanged?")) ok|=16;
 if(dd_extra(hi,"bob, interleaved fields",4,PW,PH,PW,PH,DDOVER_BOB|DDOVER_INTERLEAVED,"The surface holds two interleaved fields: even lines the bars (white first), odd lines the reversed bars (black first). The overlay showed one field at a time, changing at each of 4 flips: bars, reversed, bars, reversed, bars. Did it alternate like that, clean full-height pictures without combing?")) ok|=32;
 set_key_window(hi,0);
 return ok;
}
static void dd_mode(HINSTANCE hi) {
 DDSURFACEDESC d; HRESULT hr; DDCAPS caps; int pass=1,i; DDCOLORKEY ck; char res[300]; DWORD okm=0,xm=0; int ddx=1;
 memset(&caps,0,sizeof(caps)); caps.dwSize=sizeof(caps);
 hr=IDirectDraw_GetCaps(dd,&caps,NULL);
 logline("DirectDraw mode: GetCaps %08lX: overlay %s, FOURCC overlay %s, destination key %s, stretch %s, max visible overlays %lu\n",(DWORD)hr,(caps.dwCaps&DDCAPS_OVERLAY)?"yes":"no",(caps.dwCaps&DDCAPS_OVERLAYFOURCC)?"yes":"no",(caps.dwCKeyCaps&DDCKEYCAPS_DESTOVERLAY)?"yes":"no",(caps.dwCaps&DDCAPS_OVERLAYSTRETCH)?"yes":"no",caps.dwMaxVisibleOverlays);
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT|DDSD_BACKBUFFERCOUNT; d.dwWidth=PW; d.dwHeight=PH; d.dwBackBufferCount=1;
 d.ddsCaps.dwCaps=DDSCAPS_OVERLAY|DDSCAPS_VIDEOMEMORY|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
 d.ddpfPixelFormat.dwSize=sizeof(d.ddpfPixelFormat); d.ddpfPixelFormat.dwFlags=DDPF_FOURCC; d.ddpfPixelFormat.dwFourCC=0x32595559UL;
 hr=IDirectDraw_CreateSurface(dd,&d,&ofront,NULL);
 logline("CreateSurface (YUY2 overlay with one back buffer): %08lX\n",(DWORD)hr);
 if(FAILED(hr)) {finish("FAIL: DirectDraw refused the YUY2 overlay surface (see VX2OVL.TXT)"); return;}
 {DDSCAPS sc; memset(&sc,0,sizeof(sc)); sc.dwCaps=DDSCAPS_BACKBUFFER; hr=IDirectDrawSurface_GetAttachedSurface(ofront,&sc,&oback); logline("GetAttachedSurface(back buffer): %08lX\n",(DWORD)hr); if(FAILED(hr)) oback=NULL;}
 for(i=0;i<2;i++) {
  LPDIRECTDRAWSURFACE s=i?oback:ofront; DDSURFACEDESC l;
  if(!s) continue;
  memset(&l,0,sizeof(l)); l.dwSize=sizeof(l);
  hr=IDirectDrawSurface_Lock(s,NULL,&l,DDLOCK_WAIT,NULL);
  if(FAILED(hr)) {logline("Lock of the overlay surface failed: %08lX\n",(DWORD)hr); pass=0; continue;}
  logline("overlay surface %d: pitch %ld\n",i,l.lPitch);
  yuy2((BYTE*)l.lpSurface,(DWORD)l.lPitch,i); IDirectDrawSurface_Unlock(s,l.lpSurface);
 }
 ck.dwColorSpaceLowValue=ck.dwColorSpaceHighValue=depth==16?KEY16:KEY32;
 hr=IDirectDrawSurface_SetColorKey(prim,DDCKEY_DESTOVERLAY,&ck); logline("SetColorKey(primary, DDCKEY_DESTOVERLAY): %08lX\n",(DWORD)hr);
 if(FAILED(hr)) pass=0;
 set_key_window(hi,0);
 if(!ddshow(20,20,20+PW,20+PH,0)) pass=0;
 else {
  wait_ms(1200);
  if(ask(hi,"DirectDraw overlay: a colour bar picture should be at the top left of the screen (8 vertical bars, a grey ramp, a small white square in its corner). Is it there and clean?")) okm|=1; else pass=0;
 }
 if(!aborted&&ddshow(20,20,20+2*PW,20+2*PH,0)) {wait_ms(1000); if(ask(hi,"Now twice as wide and tall (320 x 240), same picture. Correct?")) okm|=2; else pass=0;} else pass=0;
 set_key_window(hi,1);
 if(!aborted&&ddshow(20,20,20+2*PW,20+2*PH,1)) {wait_ms(1000); if(ask(hi,"With the colour key: the picture should show only inside the 200 x 100 rectangle (dark blue around it). Correct?")) okm|=4; else pass=0;} else pass=0;
 /* v1.5 (.585; VX800 board: the key step failed with the low bits zero and replicated): the other conversions of a 5-6-5 key tried
  * live, and the key and compose registers logged for every try */
 if(!aborted&&!(okm&4)&&depth==16) {DWORD c2[4],m0,base,t; static const DWORD alt[3]={0,16UL,4096UL}; static const char *const an[3]={"low bits zero","low bits replicated","the pixel value itself"};
  ctl(0xFFFFFE17UL,c2); m0=c2[0]; base=m0&~(16UL|4096UL);
  for(t=0;t<3&&!aborted;t++) {DWORD mm=base|alt[t]; int shown,yes=0;
   ctl(0xF8000000UL|mm|2048UL,NULL); shown=ddshow(20,20,20+2*PW,20+2*PH,1); wait_ms(800);
   {static const DWORD rg[8]={0x220,0x264,0x298,0x230,0x210,0x228,0x22C,0x240}; int k2; logline("   ACCEL_OVERLAY %lu (key: %s)%s; registers:",mm,an[t],mm==m0?" [the driver's value]":"");
    for(k2=0;k2<8;k2++) {vx_ovl_req rq; DWORD rr[VXO_REPLY_DWORDS]; memset(&rq,0,sizeof(rq)); rq.cmd=VXO_REGR; rq.offset=rg[k2]; if(ovl(&rq,rr)) logline(" %03lX=%08lX",rg[k2],rr[VXO_R_UPDATES]); else logline(" %03lX=?",rg[k2]);} logline("\n");}
   if(mm!=m0&&shown) {char qq[300]; sprintf(qq,"Try %lu of 3 (key as %s): the picture should show only inside the 200 x 100 rectangle. Correct now?",t+1,an[t]); yes=ask(hi,qq); logline("   answer for ACCEL_OVERLAY %lu: %s\n",mm,yes?"YES: this device needs this value":"no");}
   if(yes) break;}
  ctl(0xF8000000UL|m0,NULL);}
 set_key_window(hi,0);
 if(!aborted&&oback&&ddshow(20,20,20+PW,20+PH,0)) {
  wait_ms(500);
  for(i=0;i<3&&!aborted;i++) {
   hr=IDirectDrawSurface_Flip(ofront,NULL,DDFLIP_WAIT); logline("   Flip: %08lX\n",(DWORD)hr); if(FAILED(hr)) pass=0;
   wait_ms(1500);
  }
  if(ask(hi,"The overlay was flipped three times (bars, reverse bars, bars, ...). Did the picture alternate between the two without flicker or damage? (After three flips it shows the reverse bars.)")) okm|=8; else pass=0;
  for(i=0;i<=10&&!aborted;i++) {hr=IDirectDrawSurface_SetOverlayPosition(ofront,20+i*28,20+i*8); if(FAILED(hr)) {logline("   SetOverlayPosition: %08lX\n",(DWORD)hr); pass=0; break;} wait_ms(300);}
  if(ask(hi,"The overlay was moved with SetOverlayPosition in 10 steps (0.3 s each) from the top left to the lower right. Did it move that way, cleanly?")) okm|=16; else pass=0;
 } else pass=0;
 hr=IDirectDrawSurface_UpdateOverlay(ofront,NULL,prim,NULL,DDOVER_HIDE,NULL); logline("UpdateOverlay(hide): %08lX\n",(DWORD)hr);
 if(FAILED(hr)) pass=0;
 wait_ms(300);
 if(ask(hi,"The overlay was hidden. Is the screen back to the plain dark blue desktop with nothing left over?")) okm|=32; else pass=0;
 if(oback) IDirectDrawSurface_Release(oback);
 if(ofront) IDirectDrawSurface_Release(ofront);
 oback=NULL; ofront=NULL;
 if(!aborted&&ddx) xm=dd_extras(hi);
 if(aborted) finish("STOPPED");
 else {sprintf(res,"%s: overlay through DirectDraw: picture %s, enlargement %s, key %s, flip %s, move %s, hide %s; extras: YV12 %s, shrink %s, RGB565 %s, RGB32 %s, source key %s, bob %s",pass&&okm==63&&xm==63?"PASS":"FAIL",(okm&1)?"ok":"BAD",(okm&2)?"ok":"BAD",(okm&4)?"ok":"BAD",(okm&8)?"ok":"BAD",(okm&16)?"ok":"BAD",(okm&32)?"ok":"BAD",(xm&1)?"ok":"BAD",(xm&2)?"ok":"BAD",(xm&4)?"ok":"BAD",(xm&8)?"ok":"BAD",(xm&16)?"ok":"BAD",(xm&32)?"ok":"BAD"); finish(res);}
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show_) {
 vx_status_t s; DWORD got=0,c[4]; WNDCLASSA wc; HMODULE mod; HRESULT hr; DDCAPS caps; int hasovl;
 (void)prev;(void)show_; forceraw=(cmd&&(strstr(cmd,"raw")||strstr(cmd,"RAW")))?1:0; forceprobe=(cmd&&(strstr(cmd,"alpha")||strstr(cmd,"ALPHA")))?1:0;
 lf=fopen("VX2OVL.TXT","w"); if(!lf) return 1;
 finish("RUNNING");
 logline("VX2OVL v1.5 (driver .585; DirectDraw extras, bob with DDFLIP_ODD/EVEN)\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC) {finish("FAIL: the VX85598 driver is not active"); return 1;}
 if(s.bpp!=16&&s.bpp!=32) {finish("FAIL: set the desktop to 16 or 32 bits per pixel first"); return 1;}
 if(!ctl(0xFFFFFE17UL,c)) {finish("FAIL: VX85598.VXD is older than v10.106 (no overlay control)"); return 1;}
 depth=s.bpp; sw=s.width; sh=s.height;
 logline("desktop %lux%lux%lu; ACCEL_OVERLAY (live) %lu\n",sw,sh,depth,c[0]);
 if(sw<640||sh<480) {finish("FAIL: the desktop must be 640x480 or larger"); return 1;}
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2OVL"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClassA(&wc);
 wc.lpfnWndProc=dwnd; wc.lpszClassName="VX2OVLD"; wc.hbrBackground=NULL; RegisterClassA(&wc);
 win=CreateWindowExA(WS_EX_TOPMOST,"VX2OVL","VX2OVL - Esc stops",WS_POPUP|WS_VISIBLE,0,0,(int)sw,(int)sh,NULL,NULL,hi,NULL);
 mod=LoadLibraryA("DDRAW.DLL"); if(!win||!mod) {finish("FAIL: no window or DDRAW.DLL"); return 1;}
 {CREATE create=(CREATE)GetProcAddress(mod,"DirectDrawCreate"); if(!create) {finish("FAIL: no DirectDrawCreate"); return 1;} hr=create(NULL,&dd,NULL); if(FAILED(hr)) {finish("FAIL: DirectDrawCreate failed"); return 1;}}
 hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); logline("SetCooperativeLevel(normal): %08lX\n",(DWORD)hr);
 {DDSURFACEDESC d; memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE; hr=IDirectDraw_CreateSurface(dd,&d,&prim,NULL); logline("CreateSurface(primary): %08lX\n",(DWORD)hr);}
 if(FAILED(hr)||!prim) {finish("FAIL: no primary surface"); return 1;}
 memset(&caps,0,sizeof(caps)); caps.dwSize=sizeof(caps); IDirectDraw_GetCaps(dd,&caps,NULL);
 hasovl=(caps.dwCaps&DDCAPS_OVERLAY)!=0;
 logline("DirectDraw caps: dwCaps %08lX dwCaps2 %08lX ckey %08lX fx %08lX surface caps %08lX max overlays %lu\n",(DWORD)caps.dwCaps,(DWORD)caps.dwCaps2,(DWORD)caps.dwCKeyCaps,(DWORD)caps.dwFXCaps,(DWORD)caps.ddsCaps.dwCaps,(DWORD)caps.dwMaxVisibleOverlays);
 {
  static const char *names[3]={"VX85598.DRV","VXDDRAW.DLL","VX85598.VXD"}; static const char *dirs[3]={"\\SYSTEM\\","\\SYSTEM\\","\\SYSTEM\\IOSUBSYS\\"};
  char dir[260],path[300]; int k; WIN32_FIND_DATAA fd; HANDLE fh;
  GetWindowsDirectoryA(dir,sizeof(dir));
  for(k=0;k<3;k++) {
   sprintf(path,"%s%s%s",dir,dirs[k],names[k]); fh=FindFirstFileA(path,&fd);
   if(fh==INVALID_HANDLE_VALUE) {sprintf(path,"%s\\SYSTEM\\VMM32\\%s",dir,names[k]); fh=FindFirstFileA(path,&fd);}
   if(fh==INVALID_HANDLE_VALUE) {sprintf(path,"%s\\SYSTEM\\%s",dir,names[k]); fh=FindFirstFileA(path,&fd);} /* v1.3: the drop folder copy puts the VxD in SYSTEM */
   if(fh!=INVALID_HANDLE_VALUE) {logline("file %s: %lu bytes\n",path,(DWORD)fd.nFileSizeLow); FindClose(fh);} else logline("file %s: not found\n",names[k]);
  }
 }
 pump(); UpdateWindow(win); wait_ms(300);
 if(forceprobe) probe_mode(hi);
 else if(hasovl&&!forceraw) dd_mode(hi);
 else {if(forceraw) logline("raw mode requested (VX2OVL RAW): testing the new overlay features\n"); else if(c[0]) logline("note: ACCEL_OVERLAY is set but DirectDraw reports no overlay (VX85598.DRV / VXDDRAW.DLL older than v10.106, or an 8 bit desktop); running the raw driver test instead\n"); raw_mode(hi);}
 IDirectDrawSurface_Release(prim); IDirectDraw_Release(dd); DestroyWindow(win);
 CloseHandle(dev); fclose(lf);
 return 0;
}
