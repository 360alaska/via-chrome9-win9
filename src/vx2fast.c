/* VX2FAST v1.2 (part D logs the first failing call). v1.1 (4-bit stretches compared in the colour bits only: DIBENG's own top byte is a leftover, VX2TOP). v1.0: driver v10.142 (performance review: O14 tall fills and the DRV size pre-check, O02 hardware cursor position writes, O16 GetBltStatus without waiting,
 * O09 palette range for 1-bit and 4-bit DIBs, O01 software cursor, plus a cost profile of one small engine fill).
 * A. Tall narrow fills at 8, 16 and 32 bpp: PatBlt PATCOPY (solid and hatched), widths 1 to 32, heights 48 to 400, the engine against DIBENG. Every case is also drawn both ways
 *    and compared pixel for pixel. The smallest height from which the engine wins at every width, depth and brush is written to the registry as ACCEL_FILL_TALL (0 = none).
 * B. The DRV size pre-check (live 0xEC000000|v): small fills, opaque text rectangles and 20-pixel lines timed with it off and on; with it on the VxD must see
 *    (almost) none of them. The pictures are compared.
 * C. The hardware cursor: position register writes counted while 2000 engine fills run under a still cursor (should be 0), then while the cursor moves (must follow).
 * D. GetBltStatus: asynchronous DirectDraw fills and copies polled with DDGBS_ISBLTDONE until done; every pixel checked afterwards.
 * E. StretchDIBits 2x from 1-bit and 4-bit DIBs onto the 32bpp screen (the driver's CPU path) against DIBENG, pixel for pixel.
 * F. Profile: where the time of one 64x64 engine fill goes (VxD phases against the whole call).
 * G. 8bpp software cursor: judged by eye (Yes/No).
 * VX2FAST.TXT / .RES. Registry: ACCEL_FILL_TALL only. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <io.h>
#include <ddraw.h>
#include "vx85598.h"

unsigned long rdtsc_lo(void);
#pragma aux rdtsc_lo = 0x0F 0x31 value [eax] modify [edx];

typedef struct {BITMAPINFOHEADER h; RGBQUAD c[256];} BMI;
static FILE *lf; static HANDLE dev=INVALID_HANDLE_VALUE; static HWND win; static int scr_w=640,scr_h=480;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2FAST.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}}
static void wait_ms(int ms) {DWORD t=GetTickCount(); while((int)(GetTickCount()-t)<ms) {pump(); Sleep(5);}}
static double now(void) {LARGE_INTEGER c,f; if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;} return (double)GetTickCount();}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static DWORD fallbacks(void) {DWORD o[4]; ctl(0xFFFFFFFEUL,o); return o[1];}
static DWORD engine_ops(void) {DWORD o[4]; ctl(0xFFFFFFFEUL,o); return o[0];}
#define ENGINE_FORCED 5UL /* copy all directions | no size thresholds */
#define ENGINE_OFF 3UL    /* copy all directions | engine off: DIBENG */
#define ENGINE_NORMAL 1UL
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_PAINT) {PAINTSTRUCT ps; HDC dc=BeginPaint(w,&ps); FillRect(dc,&ps.rcPaint,(HBRUSH)GetStockObject(BLACK_BRUSH)); EndPaint(w,&ps); return 0;}
 if(m==WM_ERASEBKGND) return 1;
 return DefWindowProc(w,m,a,b);
}
static void front(void) {HWND f=GetForegroundWindow(); DWORD ft=f?GetWindowThreadProcessId(f,NULL):0,me=GetCurrentThreadId();
 SetWindowPos(win,HWND_TOPMOST,0,0,scr_w,scr_h,SWP_SHOWWINDOW); if(ft&&ft!=me) AttachThreadInput(me,ft,TRUE); SetForegroundWindow(win); if(ft&&ft!=me) AttachThreadInput(me,ft,FALSE); wait_ms(300);}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); wait_ms(1200);
 if(rc!=DISP_CHANGE_SUCCESSFUL) return 0; scr_w=(int)w; scr_h=(int)h; front(); return 1;
}
static int ask(const char *q) {int r; logline("   question: %s\n",q); r=MessageBoxA(NULL,q,"VX2FAST",MB_YESNO|MB_TOPMOST|MB_SETFOREGROUND); logline("   answer: %s\n",r==IDYES?"yes":"no"); return r==IDYES;}
/* read a w x h screen rectangle as 32-bit pixels */
static DWORD *grab(HDC dc,int x,int y,int w,int h) {
 BMI d; void *bits; HDC m=CreateCompatibleDC(dc); HBITMAP bm,o; DWORD *out=(DWORD*)malloc((size_t)w*h*4); int r;
 memset(&d,0,sizeof(d)); d.h.biSize=sizeof(BITMAPINFOHEADER); d.h.biWidth=w; d.h.biHeight=-h; d.h.biPlanes=1; d.h.biBitCount=32;
 bm=CreateDIBSection(dc,(BITMAPINFO*)&d,DIB_RGB_COLORS,&bits,NULL,0); if(!bm||!out) {if(out) memset(out,0xEE,(size_t)w*h*4); if(bm) DeleteObject(bm); DeleteDC(m); return out;}
 o=(HBITMAP)SelectObject(m,bm); BitBlt(m,0,0,w,h,dc,x,y,SRCCOPY); GdiFlush(); for(r=0;r<h;r++) memcpy(out+(size_t)r*w,(DWORD*)bits+(size_t)r*w,(size_t)w*4);
 SelectObject(m,o); DeleteObject(bm); DeleteDC(m); return out;
}
static DWORD diff_mask=0xFFFFFFFFUL; /* v1.1: 4-bit stretches against DIBENG compare the colour bits only (VX2TOP: DIBENG leaves a neighbour's blue byte in the top byte) */
static long diff(HDC dc,int ax,int ay,int bx,int by,int w,int h,const char *what) {
 DWORD *a=grab(dc,ax,ay,w,h),*b=grab(dc,bx,by,w,h); long n=0,i;
 if(!a||!b) {free(a); free(b); return -1;}
 for(i=0;i<(long)w*h;i++) if((a[i]^b[i])&diff_mask) {if(!n) logline("   %s: first difference at %ld,%ld: %08lX against %08lX\n",what,i%w,i/w,a[i],b[i]); n++;}
 free(a); free(b); return n;
}

/* ---- A. tall fills ---- */
static double time_fill(HDC dc,int w,int h,HBRUSH br,DWORD mode) {
 double best=1e9; int run,i,reps=40; HBRUSH o=(HBRUSH)SelectObject(dc,br);
 ctl(mode,NULL);
 for(run=0;run<3;run++) {double s=now(); for(i=0;i<reps;i++) PatBlt(dc,8+((i*40)%(scr_w-w-16)),8+((i*13)%(scr_h-h-16)),w,h,PATCOPY); GdiFlush(); s=(now()-s)*1000.0/reps; if(s<best) best=s;}
 SelectObject(dc,o); ctl(ENGINE_NORMAL,NULL); return best;
}
static int partA(HDC dc,int bpp,DWORD fill_min,DWORD *tall_out,long *bad) {
 static const int ws[6]={1,2,4,8,16,32},hs[9]={48,64,96,128,160,192,256,320,400};
 HBRUSH br[2]; int b,hi,wi,tall=-1; char winrow[9];
 br[0]=CreateSolidBrush(RGB(40,170,90)); br[1]=CreateHatchBrush(HS_DIAGCROSS,RGB(230,60,20));
 for(hi=0;hi<9;hi++) winrow[hi]=1;
 for(b=0;b<2;b++) {
  logline("-- A %dbpp %s brush: us per PatBlt PATCOPY, DIBENG / engine (* = below ACCEL_FILL_MIN %lu)\n",bpp,b?"hatched":"solid",fill_min);
  for(hi=0;hi<9;hi++) {
   char line[400]; int len=0; int h=hs[hi]; if(h>scr_h-24) {winrow[hi]=0; continue;}
   len+=sprintf(line+len,"   h %3d:",h);
   for(wi=0;wi<6;wi++) {
    int w=ws[wi]; double td=time_fill(dc,w,h,br[b],ENGINE_OFF),te=time_fill(dc,w,h,br[b],ENGINE_FORCED); int below=(DWORD)(w*h)<fill_min;
    len+=sprintf(line+len," w%d %.0f/%.0f%s",w,td,te,below?"*":"");
    if(below&&te>td) winrow[hi]=0;
   }
   logline("%s\n",line);
  }
  /* correctness: engine at the left, DIBENG 256 pixels to the right (a multiple of 8: same brush phase) */
  {HBRUSH o=(HBRUSH)SelectObject(dc,br[b]); int k;
   for(k=0;k<3;k++) {int w=k==0?1:k==1?8:24,h=200; long n; char what[80];
    PatBlt(dc,0,0,scr_w,scr_h,BLACKNESS);
    ctl(ENGINE_FORCED,NULL); PatBlt(dc,40+k,24+k,w,h,PATCOPY); GdiFlush();
    ctl(ENGINE_OFF,NULL); PatBlt(dc,296+k,24+k,w,h,PATCOPY); GdiFlush(); ctl(ENGINE_NORMAL,NULL);
    sprintf(what,"%dbpp %s %dx%d engine/DIBENG",bpp,b?"hatched":"solid",w,h);
    n=diff(dc,32,16,288,16,48,220,what); logline("   %s: %ld differing pixels\n",what,n); if(n) (*bad)++;}
   SelectObject(dc,o);}
 }
 for(hi=8;hi>=0;hi--) {if(hs[hi]>scr_h-24) continue; if(!winrow[hi]) break; tall=hs[hi];}
 DeleteObject(br[0]); DeleteObject(br[1]);
 *tall_out=tall<0?0:(DWORD)tall;
 if(tall<0) logline("   %dbpp: no tested height from which the engine wins every narrow fill\n",bpp);
 else logline("   %dbpp: the engine wins every narrow fill from height %d up\n",bpp,tall);
 return tall;
}

/* ---- B. DRV size pre-check ---- */
static void partB(HDC dc,int bpp,long *bad) {
 int pass,i; double t[3][2]; DWORD f[3][2]; HBRUSH br=CreateSolidBrush(RGB(200,200,40)),o; HPEN pen=CreatePen(PS_SOLID,1,RGB(90,200,250)),op; RECT rc;
 for(pass=0;pass<2;pass++) {
  DWORD f0; double s;
  ctl(pass?0xEC000001UL:0xEC000000UL,NULL);
  o=(HBRUSH)SelectObject(dc,br);
  f0=fallbacks(); s=now(); for(i=0;i<3000;i++) PatBlt(dc,(i*17)%(scr_w-20),(i*11)%(scr_h-20),16,16,PATCOPY); GdiFlush(); t[0][pass]=(now()-s)*1000.0/3000; f[0][pass]=fallbacks()-f0;
  SelectObject(dc,o);
  SetBkColor(dc,RGB(20,20,120)); SetTextColor(dc,RGB(255,255,255));
  f0=fallbacks(); s=now(); for(i=0;i<2000;i++) {rc.left=(i*23)%(scr_w-100); rc.top=(i*7)%(scr_h-20); rc.right=rc.left+90; rc.bottom=rc.top+16; ExtTextOutA(dc,rc.left,rc.top,ETO_OPAQUE,&rc,"Menu item",9,NULL);} GdiFlush(); t[1][pass]=(now()-s)*1000.0/2000; f[1][pass]=fallbacks()-f0;
  op=(HPEN)SelectObject(dc,pen);
  f0=fallbacks(); s=now(); for(i=0;i<3000;i++) {int x=(i*29)%(scr_w-30),y=(i*13)%(scr_h-30); MoveToEx(dc,x,y,NULL); LineTo(dc,x+20,y+(i&7));} GdiFlush(); t[2][pass]=(now()-s)*1000.0/3000; f[2][pass]=fallbacks()-f0;
  SelectObject(dc,op);
 }
 logline("-- B %dbpp: us per call with the pre-check off / on (requests the VxD declined, off / on)\n",bpp);
 logline("   16x16 PatBlt: %.1f / %.1f us (%lu / %lu)\n",t[0][0],t[0][1],f[0][0],f[0][1]);
 logline("   ExtTextOut opaque 90x16: %.1f / %.1f us (%lu / %lu)\n",t[1][0],t[1][1],f[1][0],f[1][1]);
 logline("   20-pixel line: %.1f / %.1f us (%lu / %lu)\n",t[2][0],t[2][1],f[2][0],f[2][1]);
 if(f[0][1]>30||f[1][1]>20||f[2][1]>30) {logline("   the pre-check let too many through\n"); (*bad)++;}
 /* pictures: the same drawing with the check off (left) and on (right, 320 pixels further: brush phase kept) */
 {int k; long n; PatBlt(dc,0,0,scr_w,scr_h,BLACKNESS);
  for(k=0;k<2;k++) {int x0=k*320; ctl(k?0xEC000001UL:0xEC000000UL,NULL); o=(HBRUSH)SelectObject(dc,br);
   for(i=0;i<40;i++) PatBlt(dc,x0+(i*7)%260,(i*11)%200,16,16,PATCOPY);
   SelectObject(dc,o); rc.left=x0+10; rc.top=220; rc.right=x0+100; rc.bottom=236; ExtTextOutA(dc,rc.left,rc.top,ETO_OPAQUE,&rc,"Menu item",9,NULL);
   op=(HPEN)SelectObject(dc,pen); for(i=0;i<30;i++) {MoveToEx(dc,x0+10+i*8,250,NULL); LineTo(dc,x0+30+i*8,250+(i%9));} SelectObject(dc,op); GdiFlush();}
  ctl(0xEC000001UL,NULL);
  n=diff(dc,0,0,320,0,300,280,"pre-check off/on"); logline("   pictures with the check off and on: %ld differing pixels\n",n); if(n) (*bad)++;}
 DeleteObject(br); DeleteObject(pen);
}

/* ---- C. hardware cursor position writes ---- */
static void partC(HDC dc,long *bad) {
 DWORD a[4],b[4],c[4],m[4]; int i; HBRUSH br=CreateSolidBrush(RGB(120,30,200)),o;
 ctl(0xFFFFFE15UL,m); logline("-- C hardware cursor mode %lu\n",m[0]);
 if(!m[0]) {logline("   no hardware cursor: part skipped\n"); DeleteObject(br); return;}
 SetCursorPos(scr_w/2,scr_h/2); wait_ms(300);
 ctl(0xFFFFFE16UL,a); o=(HBRUSH)SelectObject(dc,br); ctl(ENGINE_FORCED,NULL);
 for(i=0;i<2000;i++) PatBlt(dc,(i*31)%(scr_w-64),(i*17)%(scr_h-64),64,64,PATINVERT);
 GdiFlush(); ctl(ENGINE_NORMAL,NULL); SelectObject(dc,o); ctl(0xFFFFFE16UL,b);
 for(i=0;i<20;i++) {SetCursorPos(40+i*20,60+i*10); wait_ms(30);}
 ctl(0xFFFFFE16UL,c);
 logline("   position writes: %lu during 2000 engine fills with the cursor still, %lu while it moved to 20 places (shown %lu)\n",b[0]-a[0],c[0]-b[0],c[3]);
 if(b[0]-a[0]>2||c[0]-b[0]<10) (*bad)++;
 DeleteObject(br);
}

/* ---- D. GetBltStatus ---- */
static void partD(long *bad) {
 LPDIRECTDRAW dd=NULL; LPDIRECTDRAWSURFACE pri=NULL,s1=NULL,s2=NULL; DDSURFACEDESC d; DDBLTFX fx; HRESULT hr; int i,x,y; long wrong=0,still=0,maxstill=0,fails=0; DWORD a0[4],a1[4];
 logline("-- D GetBltStatus (640x480x16, exclusive)\n");
 if(FAILED(DirectDrawCreate(NULL,&dd,NULL))) {logline("   DirectDrawCreate failed\n"); (*bad)++; return;}
 front();
 if(FAILED(hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))||FAILED(hr=IDirectDraw_SetDisplayMode(dd,640,480,16))) {logline("   exclusive mode %08lX\n",(DWORD)hr); (*bad)++; goto out;}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
 if(FAILED(hr=IDirectDraw_CreateSurface(dd,&d,&pri,NULL))) {logline("   primary %08lX\n",(DWORD)hr); (*bad)++; goto out;}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; d.dwWidth=640; d.dwHeight=400; d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 if(FAILED(hr=IDirectDraw_CreateSurface(dd,&d,&s1,NULL))||FAILED(hr=IDirectDraw_CreateSurface(dd,&d,&s2,NULL))) {logline("   offscreen %08lX\n",(DWORD)hr); (*bad)++; goto out;}
 {DDSURFACEDESC q;memset(&q,0,sizeof(q));q.dwSize=sizeof(q);if(SUCCEEDED(IDirectDrawSurface_GetSurfaceDesc(s1,&q)))logline("   offscreen surface: caps %08lX (%s) pitch %ld\n",q.ddsCaps.dwCaps,(q.ddsCaps.dwCaps&DDSCAPS_SYSTEMMEMORY)?"SYSTEM memory":"video memory",(long)q.lPitch);}
 ctl(0xFFFFFE12UL,a0);
 for(i=0;i<40;i++) {
  WORD v=(WORD)(0x1234+i*977); long n=0;
  memset(&fx,0,sizeof(fx)); fx.dwSize=sizeof(fx); fx.dwFillColor=v;
  hr=IDirectDrawSurface_Blt((i&1)?s2:s1,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx); if(FAILED(hr)) {if(!fails) logline("   round %d: colour fill Blt %08lX\n",i,(DWORD)hr); fails++; continue;}
  if(i&1) {hr=IDirectDrawSurface_Blt(s1,NULL,s2,NULL,DDBLT_WAIT,NULL); if(FAILED(hr)) {if(!fails) logline("   round %d: copy Blt %08lX\n",i,(DWORD)hr); fails++; continue;}}
  for(;;) {hr=IDirectDrawSurface_GetBltStatus(s1,DDGBS_ISBLTDONE); if(hr==DDERR_WASSTILLDRAWING) {n++; if(n>2000000L) break; continue;} break;}
  if(hr!=DD_OK) {logline("   round %d: GetBltStatus %08lX after %ld polls\n",i,(DWORD)hr,n); fails++;}
  still+=n; if(n>maxstill) maxstill=n;
  memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
  if(SUCCEEDED(IDirectDrawSurface_Lock(s1,NULL,&d,DDLOCK_WAIT,NULL))) {
   for(y=0;y<400;y++) {WORD *r=(WORD*)((BYTE*)d.lpSurface+y*d.lPitch); for(x=0;x<640;x++) if(r[x]!=v) {if(!wrong) logline("   round %d pixel %d,%d: %04X, expected %04X\n",i,x,y,r[x],v); wrong++;}}
   IDirectDrawSurface_Unlock(s1,d.lpSurface);
  } else {if(!fails) logline("   round %d: Lock failed\n",i); fails++;}
 }
 ctl(0xFFFFFE12UL,a1);
 logline("   40 rounds: %ld 'still drawing' answers (at most %ld in one round), %ld wrong pixels, %ld failed calls; asynchronous blits %lu, completions that waited %lu\n",still,maxstill,wrong,fails,a1[0]-a0[0],a1[1]-a0[1]);
 if(wrong||fails) (*bad)++;
out:
 if(s2) IDirectDrawSurface_Release(s2); if(s1) IDirectDrawSurface_Release(s1); if(pri) IDirectDrawSurface_Release(pri);
 if(dd) {IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd);}
 wait_ms(1500); front();
}

/* ---- E. 1-bit and 4-bit StretchDIBits 2x ---- */
static void partE(HDC dc,long *bad) {
 static BYTE bits[64*64]; BMI bi; int t,k,i; DWORD s0[4],s1[4],m1[4],m2[4]; long tot=0;
 ctl(0xFFFFFE13UL,m1); ctl(0xFFFFFE14UL,m2); /* the class masks: set to 0 for the DIBENG reference, then back */
 logline("-- E StretchDIBits 2x from 1-bit and 4-bit DIBs (32bpp): the driver against DIBENG\n");
 ctl(0xFFFFFE13UL,s0);
 for(t=0;t<2;t++) for(k=0;k<6;k++) {
  int bitsn=t?4:1,n=t?16:2; long d; char what[60];
  memset(&bi,0,sizeof(bi)); bi.h.biSize=sizeof(BITMAPINFOHEADER); bi.h.biWidth=60+k; bi.h.biHeight=40+k; bi.h.biPlanes=1; bi.h.biBitCount=(WORD)bitsn; bi.h.biClrUsed=(k&1)?0:(DWORD)n;
  for(i=0;i<256;i++) {DWORD r=(DWORD)rand()*7919UL+(DWORD)i*104729UL; bi.c[i].rgbRed=(BYTE)r; bi.c[i].rgbGreen=(BYTE)(r>>8); bi.c[i].rgbBlue=(BYTE)(r>>16); bi.c[i].rgbReserved=0;}
  for(i=0;i<(int)sizeof(bits);i++) bits[i]=(BYTE)rand();
  PatBlt(dc,0,0,scr_w,scr_h,BLACKNESS);
  ctl(ENGINE_NORMAL,NULL); StretchDIBits(dc,8,8,2*(60+k),2*(40+k),0,0,60+k,40+k,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY); GdiFlush();
  ctl(0xFB000000UL,NULL); ctl(0xFA000000UL,NULL); StretchDIBits(dc,328,8,2*(60+k),2*(40+k),0,0,60+k,40+k,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY); GdiFlush();
  ctl(0xFB000000UL|(m1[3]&0xFFFFFFUL),NULL); ctl(0xFA000000UL|(m2[3]&0xFFFFFFUL),NULL);
  sprintf(what,"%d-bit %dx%d",bitsn,60+k,40+k); diff_mask=bitsn==4?0x00FFFFFFUL:0xFFFFFFFFUL; d=diff(dc,8,8,328,8,2*(60+k),2*(40+k),what); diff_mask=0xFFFFFFFFUL; tot+=d; if(d) (*bad)++;
 }
 ctl(0xFFFFFE13UL,s1);
 logline("   12 images: %ld differing pixels; the driver drew %lu of them (%lu handed to DIBENG)\n",tot,s1[0]-s0[0],s1[1]-s0[1]);
}

/* ---- F. profile ---- */
static void partF(HDC dc) {
 static const char *nm[8]={"entry checks","engine open","idle check","save 19 registers + cursor","program","start + wait","restore 19 registers","close + cursor"};
 DWORD o[4],t0,t1; double q0,q1,mhz,sum=0,call; int i,n=2000; HBRUSH br=CreateSolidBrush(RGB(10,120,240)),ob;
 q0=now(); t0=rdtsc_lo(); wait_ms(200); t1=rdtsc_lo(); q1=now(); mhz=(double)(t1-t0)/((q1-q0)*1000.0);
 ob=(HBRUSH)SelectObject(dc,br); ctl(ENGINE_FORCED,NULL); ctl(0xF4000001UL,NULL);
 q0=now(); for(i=0;i<n;i++) PatBlt(dc,(i*31)%(scr_w-64),(i*17)%(scr_h-64),64,64,PATCOPY); GdiFlush(); call=(now()-q0)*1000.0/n;
 ctl(0xF4000000UL,NULL); ctl(ENGINE_NORMAL,NULL); SelectObject(dc,ob); DeleteObject(br);
 logline("-- F where one 64x64 engine fill's time goes (32bpp, CPU %.0f MHz): %.2f us per PatBlt in all\n",mhz,call);
 for(i=0;i<8;i++) {double us; ctl(0xFFFFFE00UL+i,o); us=o[0]?(double)o[1]*16.0/(double)o[0]/mhz:0; sum+=us; logline("   %-28s %6.2f us\n",nm[i],us);}
 logline("   inside the VxD %.2f us; outside it (GDI, DIBENG's checks, the DRV, the call into the VxD and back) %.2f us\n",sum,call-sum);
}


int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int sh) {
 WNDCLASSA wc; DWORD c[4],fill_min,tall_all=0,t; int ph,have,anytall=1; long bad=0; DEVMODEA origm; HDC dc; char res[300]; static const int depths[3]={8,16,32};
 (void)hp; (void)cmd; (void)sh;
 lf=fopen("VX2FAST.TXT","w"); logline("VX2FAST v1.2 / driver v10.142+\n"); finish("RUNNING: the run did not reach its end (VX2FAST.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL); if(dev==INVALID_HANDLE_VALUE) {finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xED000080UL,c)||c[0]!=0x80) {finish("FAIL: VX85598.VXD older than v10.142"); return 1;}
 fill_min=2304UL; logline("fill threshold %lu pixels (the driver's fixed value; .533: no registry value is read or written)\n",fill_min);
 ctl(0xED000000UL,NULL); /* the tall rule off while measuring (the engine is forced or off anyway) */
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2FAST"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClassA(&wc);
 win=CreateWindowExA(WS_EX_TOPMOST,"VX2FAST","VX2FAST",WS_POPUP|WS_VISIBLE,0,0,scr_w,scr_h,NULL,NULL,hi,NULL); UpdateWindow(win);
 for(ph=0;ph<3;ph++) {
  if(!set_mode(640,480,(DWORD)depths[ph])) {anytall=0; continue;}
  dc=GetDC(win);
  {int tl=partA(dc,depths[ph],fill_min,&t,&bad); if(tl<0) anytall=0; else if(t>tall_all) tall_all=t;}
  if(depths[ph]!=8) partB(dc,depths[ph],&bad);
  if(depths[ph]==32) {partC(dc,&bad); partE(dc,&bad); partF(dc);}
  ReleaseDC(win,dc);
  if(depths[ph]==16) partD(&bad);
 }
 if(!anytall) tall_all=0;
 logline("tall-fill height measured: %lu (%s)\n",tall_all,tall_all?"fills at least that many rows high go to the engine below ACCEL_FILL_MIN":"no height where the engine always wins: the tall rule is off");
 /* G. 8bpp software cursor */
 if(set_mode(640,480,8)) {
  HDC d=GetDC(win); int i; logline("-- G 8bpp software cursor\n");
  for(i=0;i<16;i++) {HBRUSH b=CreateSolidBrush(RGB((i*53)&255,(i*97)&255,(i*151)&255)); RECT r; r.left=i*40; r.top=0; r.right=r.left+40; r.bottom=480; FillRect(d,&r,b); DeleteObject(b);}
  ReleaseDC(win,d);
  for(i=0;i<60;i++) {SetCursorPos(20+i*10,100+(i%7)*40); wait_ms(25);}
  SetCursorPos(320,240);
  if(!ask("640x480 with 256 colours: the mouse pointer is drawn by the driver here. Move it around over the coloured stripes. Does the arrow look normal (black outline, white inside), with no trails, boxes or wrong colours left behind?")) bad++;
 }
 ctl(0xFFFFFFFFUL,NULL); ctl(0xED000000UL|(tall_all&0xFFFFUL),NULL); /* the registry values, with the new tall height live at once */
 if(have) ChangeDisplaySettingsA(&origm,0);
 sprintf(res,"%s: %ld problem%s; ACCEL_FILL_TALL = %lu (details in VX2FAST.TXT)",bad?"FAIL":"PASS",bad,bad==1?"":"s",tall_all);
 finish(res); logline("%s\n",res); DestroyWindow(win); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
