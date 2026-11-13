/* VX2KEEP v1.0: driver v10.144, the kept engine context (ACCEL_KEEP, live 0xEB000000|v). With it the engine is opened and its 19 registers saved once,
 * and handed back only at mode sets, DOS boxes, flips, the 3D side, unbind and exit, instead of on every operation.
 * For 640x480 at 8, 16 and 32 bpp and 1024x768x32, twice (ACCEL_KEEP 0, then 1), the same thing happens:
 *   part 1: 700 random operations (solid, hatched, mono-bitmap and colour-pattern brushes with several ROPs; screen copies with plain and pattern ROPs; lines;
 *           opaque text rectangles; mono bitmaps and DIB sections with screen-reading ROPs (staged); polygons (bands); 1-bit DIB stretches; clip regions on and off;
 *           dotted pens), half with the size thresholds off (everything the engine can do goes to it), plus a fixed list of transitions (clipped -> unclipped,
 *           mono pattern -> plain fill, colour pattern -> mono -> solid, staged -> copy, line -> copy);  screen captured
 *   a DOS box;  part 2 (another 700);  screen captured
 *   a DirectDraw exclusive session (640x480 at the same depth): source-key, destination-key, unkeyed and fill blits, every pixel checked against the expected
 *           values; GetBltStatus polling; 10 flips
 *   part 3 (another 700; in the kept run the live switch goes off and on again in the middle);  screen captured
 * The captures of the two runs must match byte for byte (all 32 bits). Then, with no mode change in between: one operation with ACCEL_KEEP 0, the 19 registers read;
 * ACCEL_KEEP 1, 200 operations, handed back: the 19 registers must read the same. Then the speed: 32x32 and 64x64 engine fills and the whole part 1 list, 0 against 1,
 * and the profile of one 64x64 fill with the context kept. Nothing is written to the registry; the live switch goes back to the registry value. VX2KEEP.TXT / .RES. */
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
static FILE *lf; static HANDLE dev=INVALID_HANDLE_VALUE; static HWND win; static int scr_w=640,scr_h=480; static long problems;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2KEEP.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}}
static void wait_ms(int ms) {DWORD t=GetTickCount(); while((int)(GetTickCount()-t)<ms) {pump(); Sleep(5);}}
static double now(void) {LARGE_INTEGER c,f; if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;} return (double)GetTickCount();}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static void keep(int on) {ctl(on?0xEB000001UL:0xEB000000UL,NULL);}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_PAINT) {PAINTSTRUCT ps; HDC dc=BeginPaint(w,&ps); EndPaint(w,&ps); return 0;} /* the test paints everything itself */
 if(m==WM_ERASEBKGND) return 1;
 if(m==WM_SETCURSOR) {SetCursor(NULL); return TRUE;}
 return DefWindowProc(w,m,a,b);
}
static void front(void) {HWND f=GetForegroundWindow(); DWORD ft=f?GetWindowThreadProcessId(f,NULL):0,me=GetCurrentThreadId();
 SetWindowPos(win,HWND_TOPMOST,0,0,scr_w,scr_h,SWP_SHOWWINDOW); if(ft&&ft!=me) AttachThreadInput(me,ft,TRUE); SetForegroundWindow(win); SetFocus(win); if(ft&&ft!=me) AttachThreadInput(me,ft,FALSE); SetCursorPos(0,0); wait_ms(300);}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("   ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); wait_ms(1200);
 if(rc!=DISP_CHANGE_SUCCESSFUL) return 0; scr_w=(int)w; scr_h=(int)h; front(); return 1;
}
static DWORD *grab(HDC dc,int w,int h) {
 BMI d; void *bits; HDC m=CreateCompatibleDC(dc); HBITMAP bm,o; DWORD *out=(DWORD*)malloc((size_t)w*h*4); int r;
 memset(&d,0,sizeof(d)); d.h.biSize=sizeof(BITMAPINFOHEADER); d.h.biWidth=w; d.h.biHeight=-h; d.h.biPlanes=1; d.h.biBitCount=32;
 bm=CreateDIBSection(dc,(BITMAPINFO*)&d,DIB_RGB_COLORS,&bits,NULL,0); if(!bm||!out) {if(out) memset(out,0xEE,(size_t)w*h*4); if(bm) DeleteObject(bm); DeleteDC(m); return out;}
 o=(HBITMAP)SelectObject(m,bm); SelectClipRgn(dc,NULL); BitBlt(m,0,0,w,h,dc,0,0,SRCCOPY); GdiFlush(); for(r=0;r<h;r++) memcpy(out+(size_t)r*w,(DWORD*)bits+(size_t)r*w,(size_t)w*4);
 SelectObject(m,o); DeleteObject(bm); DeleteDC(m); return out;
}

/* ---- the operations ---- */
static DWORD rs;
static DWORD rnd(void) {rs=rs*1103515245UL+12345UL; return (rs>>8)&0xFFFFFFUL;}
static HBRUSH b_solid[4],b_hatch[3],b_mono,b_col; static HBITMAP mono_bm,dib_bm; static HDC mdc_mono,mdc_dib; static HPEN p_solid[3],p_dot; static BYTE one_bit[32*16]; static BMI bi1;
static void objects(HDC dc) {
 static WORD pat[8]={0x81,0x42,0x24,0x18,0x18,0x24,0x42,0x81}; static BYTE monobits[64*8]; struct {BITMAPINFOHEADER h; DWORD px[64];} cp; BMI d; void *bits; int i;
 b_solid[0]=CreateSolidBrush(RGB(200,30,30)); b_solid[1]=CreateSolidBrush(RGB(20,180,60)); b_solid[2]=CreateSolidBrush(RGB(40,60,220)); b_solid[3]=CreateSolidBrush(RGB(127,127,127));
 b_hatch[0]=CreateHatchBrush(HS_DIAGCROSS,RGB(250,200,0)); b_hatch[1]=CreateHatchBrush(HS_HORIZONTAL,RGB(0,200,250)); b_hatch[2]=CreateHatchBrush(HS_BDIAGONAL,RGB(250,0,250));
 {HBITMAP pb=CreateBitmap(8,8,1,1,pat); b_mono=CreatePatternBrush(pb); DeleteObject(pb);}
 memset(&cp,0,sizeof(cp)); cp.h.biSize=sizeof(BITMAPINFOHEADER); cp.h.biWidth=8; cp.h.biHeight=8; cp.h.biPlanes=1; cp.h.biBitCount=32;
 for(i=0;i<64;i++) cp.px[i]=((i*37)&255)|(((i*91)&255)<<8)|(((i*53)&255)<<16);
 b_col=CreateDIBPatternBrushPt(&cp,DIB_RGB_COLORS);
 for(i=0;i<(int)sizeof(monobits);i++) monobits[i]=(BYTE)(i*29+(i>>3));
 mono_bm=CreateBitmap(64,64,1,1,monobits); mdc_mono=CreateCompatibleDC(dc); SelectObject(mdc_mono,mono_bm);
 memset(&d,0,sizeof(d)); d.h.biSize=sizeof(BITMAPINFOHEADER); d.h.biWidth=64; d.h.biHeight=64; d.h.biPlanes=1; d.h.biBitCount=32;
 dib_bm=CreateDIBSection(dc,(BITMAPINFO*)&d,DIB_RGB_COLORS,&bits,NULL,0); for(i=0;i<64*64;i++) ((DWORD*)bits)[i]=(DWORD)(i*2654435761UL)&0xFFFFFFUL; GdiFlush();
 mdc_dib=CreateCompatibleDC(dc); SelectObject(mdc_dib,dib_bm);
 p_solid[0]=CreatePen(PS_SOLID,1,RGB(255,255,255)); p_solid[1]=CreatePen(PS_SOLID,1,RGB(255,128,0)); p_solid[2]=CreatePen(PS_SOLID,1,RGB(0,0,0)); p_dot=CreatePen(PS_DOT,1,RGB(0,255,0));
 for(i=0;i<(int)sizeof(one_bit);i++) one_bit[i]=(BYTE)(i*73+11);
 memset(&bi1,0,sizeof(bi1)); bi1.h.biSize=sizeof(BITMAPINFOHEADER); bi1.h.biWidth=32; bi1.h.biHeight=16; bi1.h.biPlanes=1; bi1.h.biBitCount=1; bi1.h.biClrUsed=2;
 bi1.c[0].rgbRed=10; bi1.c[0].rgbGreen=200; bi1.c[0].rgbBlue=90; bi1.c[1].rgbRed=240; bi1.c[1].rgbGreen=240; bi1.c[1].rgbBlue=20;
}
static void free_objects(void) {int i; for(i=0;i<4;i++) DeleteObject(b_solid[i]); for(i=0;i<3;i++) {DeleteObject(b_hatch[i]); DeleteObject(p_solid[i]);} DeleteObject(b_mono); DeleteObject(b_col); DeleteObject(p_dot); DeleteDC(mdc_mono); DeleteDC(mdc_dib); DeleteObject(mono_bm); DeleteObject(dib_bm);}
static void one_op(HDC dc,int k) {
 int w=1+(int)(rnd()%(k&1?220:60)),h=1+(int)(rnd()%(k&2?160:50)),x=(int)(rnd()%(DWORD)(scr_w-w)),y=(int)(rnd()%(DWORD)(scr_h-h)); HBRUSH ob; HPEN op; DWORD r=rnd();
 static const DWORD prop[6]={PATCOPY,PATINVERT,DSTINVERT,BLACKNESS,WHITENESS,0x00A000C9UL};
 switch(r%14) {
  case 0: ob=(HBRUSH)SelectObject(dc,b_solid[(r>>4)&3]); PatBlt(dc,x,y,w,h,prop[(r>>8)%6]); SelectObject(dc,ob); break;
  case 1: ob=(HBRUSH)SelectObject(dc,b_hatch[(r>>4)%3]); SetBkColor(dc,RGB(30,30,30)); PatBlt(dc,x,y,w,h,(r>>8)&1?PATINVERT:PATCOPY); SelectObject(dc,ob); break;
  case 2: ob=(HBRUSH)SelectObject(dc,b_mono); SetTextColor(dc,RGB(255,255,0)); SetBkColor(dc,RGB(0,0,128)); PatBlt(dc,x,y,w,h,(r>>8)&1?PATINVERT:PATCOPY); SelectObject(dc,ob); break;
  case 3: ob=(HBRUSH)SelectObject(dc,b_col); PatBlt(dc,x,y,w,h,(r>>8)&1?PATINVERT:PATCOPY); SelectObject(dc,ob); break;
  case 4: {int sx=(int)(rnd()%(DWORD)(scr_w-w)),sy=(int)(rnd()%(DWORD)(scr_h-h)); static const DWORD cr[5]={SRCCOPY,SRCINVERT,SRCAND,MERGEPAINT,NOTSRCCOPY}; BitBlt(dc,x,y,w,h,dc,sx,sy,cr[(r>>4)%5]); break;}
  case 5: {int sx=(int)(rnd()%(DWORD)(scr_w-w)),sy=(int)(rnd()%(DWORD)(scr_h-h)); ob=(HBRUSH)SelectObject(dc,b_solid[(r>>4)&3]); BitBlt(dc,x,y,w,h,dc,sx,sy,(r>>8)&1?0x00B8074AUL:0x00E20746UL); SelectObject(dc,ob); break;}
  case 6: op=(HPEN)SelectObject(dc,p_solid[(r>>4)%3]); SetROP2(dc,(r>>8)&1?R2_NOTCOPYPEN:R2_COPYPEN); MoveToEx(dc,x,y,NULL); LineTo(dc,x+(int)(rnd()%(DWORD)(scr_w-x)),(int)(rnd()%(DWORD)scr_h)); SetROP2(dc,R2_COPYPEN); SelectObject(dc,op); break;
  case 7: {RECT rc; rc.left=x; rc.top=y; rc.right=x+w; rc.bottom=y+(h>20?20:h); SetBkColor(dc,RGB((r>>4)&255,(r>>12)&255,90)); SetTextColor(dc,RGB(255,255,255)); ExtTextOutA(dc,x,y,ETO_OPAQUE,&rc,"keep",4,NULL); break;}
  case 8: {int mw=w>64?64:w,mh=h>64?64:h; SetTextColor(dc,RGB(0,0,0)); SetBkColor(dc,RGB(255,255,255)); BitBlt(dc,x,y,mw,mh,mdc_mono,0,0,(r>>4)&1?SRCAND:SRCPAINT); break;}
  case 9: {int mw=w>64?64:w,mh=h>64?64:h; BitBlt(dc,x,y,mw,mh,mdc_dib,0,0,(r>>4)&1?SRCINVERT:SRCAND); break;}
  case 10: {POINT pt[4]; ob=(HBRUSH)SelectObject(dc,(r>>4)&1?b_hatch[(r>>5)%3]:b_col); pt[0].x=x; pt[0].y=y; pt[1].x=x+w; pt[1].y=y+h/3; pt[2].x=x+w/2; pt[2].y=y+h; pt[3].x=x+w/4; pt[3].y=y+h/2; SetROP2(dc,(r>>8)&1?R2_XORPEN:R2_COPYPEN); Polygon(dc,pt,4); SetROP2(dc,R2_COPYPEN); SelectObject(dc,ob); break;}
  case 11: StretchDIBits(dc,x,y,64,32,0,0,32,16,one_bit,(BITMAPINFO*)&bi1,DIB_RGB_COLORS,SRCCOPY); break;
  case 12: if((r>>4)&1) {HRGN rg=CreateRectRgn(x,y,x+w+40,y+h+40); SelectClipRgn(dc,rg); DeleteObject(rg);} else SelectClipRgn(dc,NULL); break;
  default: op=(HPEN)SelectObject(dc,p_dot); MoveToEx(dc,x,y,NULL); LineTo(dc,x+w,y+h); SelectObject(dc,op); break;
 }
}
static void transitions(HDC dc) { /* fixed order: each command right after one that set different engine state */
 HRGN rg=CreateRectRgn(100,100,300,250); HBRUSH ob; HPEN op;
 SelectClipRgn(dc,rg); ob=(HBRUSH)SelectObject(dc,b_solid[0]); PatBlt(dc,80,80,260,200,PATCOPY); SelectClipRgn(dc,NULL); PatBlt(dc,120,90,90,120,PATINVERT); SelectObject(dc,ob); DeleteObject(rg);
 ob=(HBRUSH)SelectObject(dc,b_mono); PatBlt(dc,20,300,200,100,PATCOPY); SelectObject(dc,b_solid[1]); PatBlt(dc,60,320,200,60,PATCOPY);
 SelectObject(dc,b_col); PatBlt(dc,300,20,150,120,PATCOPY); SelectObject(dc,b_mono); PatBlt(dc,330,40,150,120,PATCOPY); SelectObject(dc,b_solid[2]); PatBlt(dc,360,60,150,120,PATCOPY); SelectObject(dc,ob);
 BitBlt(dc,400,300,64,64,mdc_dib,0,0,SRCINVERT); BitBlt(dc,420,320,120,100,dc,300,20,SRCCOPY);
 op=(HPEN)SelectObject(dc,p_solid[1]); MoveToEx(dc,10,10,NULL); LineTo(dc,600,400); SelectObject(dc,op); BitBlt(dc,10,200,200,100,dc,0,0,SRCCOPY);
 GdiFlush();
}
static void part(HDC dc,DWORD seed,int n,int toggle) {
 int k; rs=seed;
 for(k=0;k<n;k++) {
  if(k==n/2) ctl(5,NULL); /* the second half with the size thresholds off */
  if(toggle&&k==n/3) keep(0);
  if(toggle&&k==(2*n)/3) keep(1);
  one_op(dc,k);
  if((k&63)==63) pump();
 }
 ctl(1,NULL); SelectClipRgn(dc,NULL); GdiFlush();
}
static void dosbox(void) {
 STARTUPINFOA si; PROCESS_INFORMATION pi; char cmd[]="COMMAND.COM /C EXIT";
 memset(&si,0,sizeof(si)); si.cb=sizeof(si); si.dwFlags=STARTF_USESHOWWINDOW; si.wShowWindow=SW_SHOWMINNOACTIVE;
 if(CreateProcessA(NULL,cmd,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi)) {WaitForSingleObject(pi.hProcess,20000); CloseHandle(pi.hThread); CloseHandle(pi.hProcess);}
 wait_ms(800); front();
}
/* DirectDraw: keyed and plain blits with exact expected pixels; returns the number of wrong pixels / failed calls */
static long ddsession(int bpp,DWORD seed) {
 LPDIRECTDRAW dd=NULL; LPDIRECTDRAWSURFACE pri=NULL,back=NULL,src=NULL; DDSURFACEDESC d; DDSCAPS caps; DDBLTFX fx; DDCOLORKEY ck; HRESULT hr; int i,x,y,by=bpp/8; long bad=0;
 DWORD mask=bpp==32?0x00FFFFFFUL:bpp==16?0xFFFFUL:0xFFUL,K=0x15&mask,K2=0x2A&mask;
 static DWORD sv[128*64];
 if(FAILED(DirectDrawCreate(NULL,&dd,NULL))) return 1;
 front();
 if(FAILED(hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))||FAILED(hr=IDirectDraw_SetDisplayMode(dd,640,480,bpp))) {logline("   DirectDraw setup %08lX\n",(DWORD)hr); bad++; goto out;}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; d.dwBackBufferCount=1; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
 if(FAILED(hr=IDirectDraw_CreateSurface(dd,&d,&pri,NULL))) {logline("   primary %08lX\n",(DWORD)hr); bad++; goto out;}
 caps.dwCaps=DDSCAPS_BACKBUFFER; if(FAILED(hr=IDirectDrawSurface_GetAttachedSurface(pri,&caps,&back))) {bad++; goto out;}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; d.dwWidth=128; d.dwHeight=64; d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 if(FAILED(hr=IDirectDraw_CreateSurface(dd,&d,&src,NULL))) {logline("   offscreen %08lX\n",(DWORD)hr); bad++; goto out;}
 for(i=0;i<4;i++) {
  int step; DWORD fv=(seed*977+(DWORD)i*131)&mask; RECT dr; dr.left=40+i*30; dr.top=30+i*20; dr.right=dr.left+128; dr.bottom=dr.top+64;
  memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
  if(FAILED(IDirectDrawSurface_Lock(src,NULL,&d,DDLOCK_WAIT,NULL))) {bad++; break;}
  for(y=0;y<64;y++) {BYTE *r=(BYTE*)d.lpSurface+y*d.lPitch; for(x=0;x<128;x++) {DWORD v=((x*7+y*13+i)%5==0)?K:(((DWORD)(x*2654435761UL+y*40503UL+i))&mask); if(v==K&&(x*7+y*13+i)%5) v^=1; if(v==K2) v^=4; sv[y*128+x]=v; if(by==4) ((DWORD*)r)[x]=v; else if(by==2) ((WORD*)r)[x]=(WORD)v; else r[x]=(BYTE)v;}}
  IDirectDrawSurface_Unlock(src,d.lpSurface);
  for(step=0;step<3;step++) { /* 0 source key, 1 destination key, 2 plain: back filled with fv (and K2 stripes for step 1) first */
   long wrong=0;
   memset(&fx,0,sizeof(fx)); fx.dwSize=sizeof(fx); fx.dwFillColor=fv;
   if(FAILED(IDirectDrawSurface_Blt(back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx))) {bad++; continue;}
   if(step==1) {RECT s; for(y=0;y<480;y+=8) {s.left=0; s.right=640; s.top=y; s.bottom=y+3; fx.dwFillColor=K2; IDirectDrawSurface_Blt(back,&s,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx);}}
   if(step==0) {ck.dwColorSpaceLowValue=ck.dwColorSpaceHighValue=K; IDirectDrawSurface_SetColorKey(src,DDCKEY_SRCBLT,&ck); hr=IDirectDrawSurface_Blt(back,&dr,src,NULL,DDBLT_WAIT|DDBLT_KEYSRC,NULL);}
   else if(step==1) {ck.dwColorSpaceLowValue=ck.dwColorSpaceHighValue=K2; IDirectDrawSurface_SetColorKey(back,DDCKEY_DESTBLT,&ck); hr=IDirectDrawSurface_Blt(back,&dr,src,NULL,DDBLT_WAIT|DDBLT_KEYDEST,NULL);}
   else hr=IDirectDrawSurface_Blt(back,&dr,src,NULL,DDBLT_WAIT,NULL);
   if(FAILED(hr)) {logline("   blit step %d: %08lX\n",step,(DWORD)hr); bad++; continue;}
   while(IDirectDrawSurface_GetBltStatus(back,DDGBS_ISBLTDONE)==DDERR_WASSTILLDRAWING) ;
   memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
   if(FAILED(IDirectDrawSurface_Lock(back,NULL,&d,DDLOCK_WAIT,NULL))) {bad++; continue;}
   for(y=dr.top-2;y<dr.bottom+2;y++) {BYTE *r=(BYTE*)d.lpSurface+y*d.lPitch; for(x=dr.left-2;x<dr.right+2;x++) {
    DWORD v=(by==4?((DWORD*)r)[x]:by==2?(DWORD)((WORD*)r)[x]:(DWORD)r[x])&mask,bgv=(step==1&&(y%8)<3)?K2:fv,want=bgv;
    if(x>=dr.left&&x<dr.right&&y>=dr.top&&y<dr.bottom) {DWORD s=sv[(y-dr.top)*128+(x-dr.left)]; want=step==0?(s==K?bgv:s):step==1?(bgv==K2?s:bgv):s;}
    if(v!=want) {if(!wrong) logline("   round %d step %d pixel %d,%d: %06lX, expected %06lX\n",i,step,x,y,v,want); wrong++;}}}
   IDirectDrawSurface_Unlock(back,d.lpSurface); bad+=wrong;
   if(step==1) {ck.dwColorSpaceLowValue=ck.dwColorSpaceHighValue=0; IDirectDrawSurface_SetColorKey(back,DDCKEY_DESTBLT,NULL);}
  }
  IDirectDrawSurface_Flip(pri,NULL,DDFLIP_WAIT); IDirectDrawSurface_Flip(pri,NULL,DDFLIP_WAIT);
 }
 for(i=0;i<2;i++) IDirectDrawSurface_Flip(pri,NULL,DDFLIP_WAIT);
out:
 if(src) IDirectDrawSurface_Release(src); if(back) IDirectDrawSurface_Release(back); if(pri) IDirectDrawSurface_Release(pri);
 if(dd) {IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd);}
 wait_ms(1500); front(); return bad;
}
/* one full run at the current mode: returns three captures */
static void run(int kept,DWORD w,DWORD h,DWORD bpp,DWORD *cap[3],long *ddbad) {
 HDC dc; DWORD o[4];
 keep(kept); logline("  run with ACCEL_KEEP %d\n",kept);
 if(!set_mode(w,h,bpp)) {cap[0]=cap[1]=cap[2]=NULL; return;}
 dc=GetDC(win); objects(dc); SelectClipRgn(dc,NULL); PatBlt(dc,0,0,scr_w,scr_h,BLACKNESS);
 part(dc,1000+bpp,700,0); transitions(dc); cap[0]=grab(dc,scr_w,scr_h); ReleaseDC(win,dc);
 dosbox(); dc=GetDC(win);
 part(dc,2000+bpp,700,0); transitions(dc); cap[1]=grab(dc,scr_w,scr_h); ReleaseDC(win,dc);
 *ddbad=ddsession((int)bpp,bpp*3+kept*0);
 dc=GetDC(win); SelectClipRgn(dc,NULL); PatBlt(dc,0,0,scr_w,scr_h,BLACKNESS);
 part(dc,3000+bpp,700,kept); transitions(dc); cap[2]=grab(dc,scr_w,scr_h);
 free_objects(); ReleaseDC(win,dc);
 ctl(0xFFFFFE3FUL,o); logline("   kept context opened %lu times, handed back %lu times so far (open now %lu)\n",o[0],o[1],o[2]);
}
static long cmp(const DWORD *a,const DWORD *b,long n,const char *what) {
 long i,d=0; if(!a||!b) {logline("   %s: missing capture\n",what); return 1;}
 for(i=0;i<n;i++) if(a[i]!=b[i]) {if(!d) logline("   %s: first difference at %ld,%ld: %08lX with 0, %08lX with 1\n",what,i%scr_w,i/scr_w,a[i],b[i]); d++;}
 logline("   %s: %ld differing pixels\n",what,d); return d;
}
static void regs(DWORD *r) {int k; DWORD o[4]; for(k=0;k<19;k++) {ctl(0xFFFFFE40UL+(DWORD)k,o); r[k]=o[0];}}
static DWORD reg_read(const char *name,DWORD def) {HKEY k; DWORD v=def,t,n=4; if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Software\\VX85598",0,KEY_READ,&k)==ERROR_SUCCESS) {if(RegQueryValueExA(k,name,NULL,&t,(BYTE*)&v,&n)!=ERROR_SUCCESS||t!=REG_DWORD) v=def; RegCloseKey(k);} return v;}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmdl,int sh) {
 WNDCLASSA wc; DWORD c[4],r0[19],r1[19]; int have,mi,k; DEVMODEA origm; char res[300]; HDC dc;
 static const DWORD modes[4][3]={{640,480,8},{640,480,16},{640,480,32},{1024,768,32}};
 (void)hp; (void)cmdl; (void)sh;
 lf=fopen("VX2KEEP.TXT","w"); logline("VX2KEEP v1.0 / driver v10.144 (registry ACCEL_KEEP %lu)\n",reg_read("ACCEL_KEEP",0)); finish("RUNNING: the run did not reach its end (VX2KEEP.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL); if(dev==INVALID_HANDLE_VALUE) {finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xEB000001UL,c)||c[0]!=1||!ctl(0xFFFFFE3FUL,c)||c[3]!=1) {finish("FAIL: VX85598.VXD older than v10.144"); return 1;}
 keep(0);
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2KEEP"; RegisterClassA(&wc);
 win=CreateWindowExA(WS_EX_TOPMOST,"VX2KEEP","VX2KEEP",WS_POPUP|WS_VISIBLE,0,0,scr_w,scr_h,NULL,NULL,hi,NULL); UpdateWindow(win); ShowCursor(FALSE);
 for(mi=0;mi<4;mi++) {
  DWORD *a[3],*b[3]; long da,db,n; int p;
  logline("== %lux%lux%lu\n",modes[mi][0],modes[mi][1],modes[mi][2]);
  run(0,modes[mi][0],modes[mi][1],modes[mi][2],a,&da);
  run(1,modes[mi][0],modes[mi][1],modes[mi][2],b,&db);
  n=(long)scr_w*scr_h;
  for(p=0;p<3;p++) {char what[80]; sprintf(what,"capture %d (%s)",p+1,p==0?"part 1":p==1?"after the DOS box":"after DirectDraw, live switch toggled in the kept run"); if(cmp(a[p],b[p],n,what)) problems++; free(a[p]); free(b[p]);}
  logline("   DirectDraw keyed/plain blits: %ld wrong with 0, %ld wrong with 1\n",da,db); if(da||db) problems++;
 }
 /* registers handed back */
 keep(0); dc=GetDC(win);
 {HBRUSH ob=(HBRUSH)SelectObject(dc,GetStockObject(GRAY_BRUSH)); ctl(5,NULL); PatBlt(dc,10,10,64,64,PATCOPY); GdiFlush(); regs(r0);
  keep(1); objects(dc); part(dc,4242,200,0); ctl(5,NULL); PatBlt(dc,10,10,64,64,PATINVERT); GdiFlush(); keep(0); regs(r1); free_objects(); ctl(1,NULL); SelectObject(dc,ob);}
 for(k=0;k<19;k++) if(r0[k]!=r1[k]) {logline("   register %d: %08lX before, %08lX after the kept context was handed back\n",k,r0[k],r1[k]); problems++;}
 logline("-- registers after the hand-back: %s\n",memcmp(r0,r1,sizeof(r0))?"DIFFERENT":"the same 19 values as before the kept run");
 /* speed */
 {double t[4][2]; int m,i; HBRUSH br=CreateSolidBrush(RGB(10,120,240)),ob=(HBRUSH)SelectObject(dc,br); DWORD q0,q1,o[4]; double mhz,q;
  for(m=0;m<2;m++) {double s;
   keep(m); ctl(5,NULL);
   s=now(); for(i=0;i<3000;i++) PatBlt(dc,(i*31)%(scr_w-64),(i*17)%(scr_h-64),32,32,PATCOPY); GdiFlush(); t[0][m]=(now()-s)*1000.0/3000;
   s=now(); for(i=0;i<3000;i++) PatBlt(dc,(i*31)%(scr_w-64),(i*17)%(scr_h-64),64,64,PATINVERT); GdiFlush(); t[1][m]=(now()-s)*1000.0/3000;
   s=now(); for(i=0;i<3000;i++) BitBlt(dc,(i*31)%(scr_w-80),(i*17)%(scr_h-80),48,48,dc,((i+5)*29)%(scr_w-80),((i+3)*13)%(scr_h-80),SRCCOPY); GdiFlush(); t[2][m]=(now()-s)*1000.0/3000;
   ctl(1,NULL); SelectObject(dc,ob); objects(dc); s=now(); part(dc,777,700,0); t[3][m]=(now()-s); free_objects(); SelectObject(dc,br);
  }
  logline("-- speed, ACCEL_KEEP 0 / 1 (%lux%lu, %lu bpp)\n",(DWORD)scr_w,(DWORD)scr_h,modes[3][2]);
  logline("   32x32 PatBlt PATCOPY: %.2f / %.2f us\n   64x64 PatBlt PATINVERT: %.2f / %.2f us\n   48x48 screen copy: %.2f / %.2f us\n   700 mixed operations: %.1f / %.1f ms\n",t[0][0],t[0][1],t[1][0],t[1][1],t[2][0],t[2][1],t[3][0],t[3][1]);
  {double qa,qb; qa=now(); q0=rdtsc_lo(); wait_ms(200); q1=rdtsc_lo(); qb=now(); mhz=(double)(q1-q0)/((qb-qa)*1000.0); q=qa;} (void)q;
  keep(1); ctl(5,NULL); ctl(0xF4000001UL,NULL); for(i=0;i<2000;i++) PatBlt(dc,(i*31)%(scr_w-64),(i*17)%(scr_h-64),64,64,PATCOPY); GdiFlush(); ctl(0xF4000000UL,NULL); ctl(1,NULL);
  {static const char *nm[8]={"entry checks","engine open","idle check","save 19 registers + cursor","program","start + wait","restore 19 registers","close + cursor"}; double sum=0;
   logline("-- profile of one 64x64 fill with the context kept (CPU %.0f MHz)\n",mhz);
   for(i=0;i<8;i++) {double us; ctl(0xFFFFFE00UL+(DWORD)i,o); us=o[0]?(double)o[1]*16.0/(double)o[0]/mhz:0; sum+=us; logline("   %-28s %6.2f us\n",nm[i],us);}
   logline("   inside the VxD %.2f us\n",sum);}
  SelectObject(dc,ob); DeleteObject(br);}
 ReleaseDC(win,dc);
 ctl(0xFFFFFFFFUL,NULL); ShowCursor(TRUE);
 if(have) ChangeDisplaySettingsA(&origm,0);
 sprintf(res,"%s: %ld problem%s (4 modes x 3 captures compared byte for byte, DirectDraw keyed blits, registers after the hand-back; speed in VX2KEEP.TXT)",problems?"FAIL":"PASS",problems,problems==1?"":"s");
 finish(res); logline("%s\n",res); DestroyWindow(win); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
