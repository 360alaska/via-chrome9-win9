/* VX2TOP v1.0: what goes into the unused top byte of a 32bpp screen pixel when StretchDIBits draws a 1-bit, 4-bit or 8-bit DIB, the driver's CPU path against DIBENG.
 * VX2FAST v1.0 part E: 4-bit 2x images matched DIBENG in all colour bits but differed in the top byte (1-bit matched). Before changing the driver this finds DIBENG's rule:
 * a constant, the colour table's rgbReserved, or the byte that was already on the screen. Cases: 1-bit, 4-bit and 8-bit sources at 2x2, 3x1 and 1:1, single-colour images
 * with rgbReserved 0 and with rgbReserved 5A (the other entries 11), on a background whose top byte is C3 and one whose top byte is 00, plus a random image.
 * For each case: the pixel at 0,0 both ways (all 32 bits), how many pixels differ in the colour bits and in the top byte, and the top bytes seen. 640x480x32. VX2TOP.TXT / .RES. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <io.h>
#include "vx85598.h"

typedef struct {BITMAPINFOHEADER h; RGBQUAD c[256];} BMI;
static FILE *lf; static HANDLE dev=INVALID_HANDLE_VALUE; static HWND win;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2TOP.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}}
static void wait_ms(int ms) {DWORD t=GetTickCount(); while((int)(GetTickCount()-t)<ms) {pump(); Sleep(5);}}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_PAINT) {PAINTSTRUCT ps; HDC dc=BeginPaint(w,&ps); EndPaint(w,&ps); return 0;}
 if(m==WM_ERASEBKGND) return 1;
 return DefWindowProc(w,m,a,b);
}
static DWORD *grab(HDC dc,int x,int y,int w,int h) {
 BMI d; void *bits; HDC m=CreateCompatibleDC(dc); HBITMAP bm,o; DWORD *out=(DWORD*)malloc((size_t)w*h*4); int r;
 memset(&d,0,sizeof(d)); d.h.biSize=sizeof(BITMAPINFOHEADER); d.h.biWidth=w; d.h.biHeight=-h; d.h.biPlanes=1; d.h.biBitCount=32;
 bm=CreateDIBSection(dc,(BITMAPINFO*)&d,DIB_RGB_COLORS,&bits,NULL,0); if(!bm||!out) {if(out) memset(out,0xEE,(size_t)w*h*4); if(bm) DeleteObject(bm); DeleteDC(m); return out;}
 o=(HBITMAP)SelectObject(m,bm); BitBlt(m,0,0,w,h,dc,x,y,SRCCOPY); GdiFlush(); for(r=0;r<h;r++) memcpy(out+(size_t)r*w,(DWORD*)bits+(size_t)r*w,(size_t)w*4);
 SelectObject(m,o); DeleteObject(bm); DeleteDC(m); return out;
}
/* a background whose every pixel is 0xTT202020 (a 32-bit DIB copied 1:1) */
static void background(HDC dc,BYTE top) {
 static DWORD px[640*8]; BMI d; int y,i;
 for(i=0;i<640*8;i++) px[i]=((DWORD)top<<24)|0x202020UL;
 memset(&d,0,sizeof(d)); d.h.biSize=sizeof(BITMAPINFOHEADER); d.h.biWidth=640; d.h.biHeight=-8; d.h.biPlanes=1; d.h.biBitCount=32;
 for(y=0;y<480;y+=8) SetDIBitsToDevice(dc,0,y,640,8,0,0,0,8,px,(BITMAPINFO*)&d,DIB_RGB_COLORS);
 GdiFlush();
}
static void tops(const DWORD *p,long n,char *out) { /* up to 3 most common top bytes */
 long cnt[256]; int i,k; memset(cnt,0,sizeof(cnt)); for(i=0;i<n;i++) cnt[p[i]>>24]++;
 out[0]=0; for(k=0;k<3;k++) {int best=-1; for(i=0;i<256;i++) if(cnt[i]&&(best<0||cnt[i]>cnt[best])) best=i; if(best<0) break; sprintf(out+strlen(out),"%s%02X x%ld",k?", ":"",best,cnt[best]); cnt[best]=0;}
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int sh) {
 static BYTE bits[96*96]; WNDCLASSA wc; DEVMODEA dm,origm; HDC dc; BMI bi; DWORD m1[4],m2[4],m3[4]; int have,kind,shape,var,bg,i; long colour_bad=0,top_bad=0; char res[200];
 static const int kbits[3]={1,4,8},kidx[3]={1,5,77};
 (void)hp; (void)cmd; (void)sh;
 lf=fopen("VX2TOP.TXT","w"); logline("VX2TOP v1.0\n"); finish("RUNNING: the run did not reach its end");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL); if(dev==INVALID_HANDLE_VALUE) {finish("FAIL: driver not found"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=640; dm.dmPelsHeight=480; dm.dmBitsPerPel=32; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 if(ChangeDisplaySettingsA(&dm,0)!=DISP_CHANGE_SUCCESSFUL) {finish("FAIL: 640x480x32 refused"); return 1;}
 wait_ms(1200);
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2TOP"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClassA(&wc);
 win=CreateWindowExA(WS_EX_TOPMOST,"VX2TOP","VX2TOP",WS_POPUP|WS_VISIBLE,0,0,640,480,NULL,NULL,hi,NULL); UpdateWindow(win); SetCursorPos(639,479); wait_ms(400);
 dc=GetDC(win);
 ctl(0xFFFFFE13UL,m1); ctl(0xFFFFFE14UL,m2); logline("class masks: ACCEL_STRETCH %06lX, ACCEL_STRETCH2 %06lX\n",m1[3],m2[3]);
 for(kind=0;kind<3;kind++) for(shape=0;shape<3;shape++) for(var=0;var<3;var++) for(bg=0;bg<2;bg++) {
  int sw=40,shh=30,kx=shape==0?2:shape==1?3:1,ky=shape==0?2:1,dw=sw*kx,dh=shh*ky,n=1<<kbits[kind]; DWORD *a,*b; long cd=0,td=0,j; char ta[60],tb[60]; DWORD s0[4],s1[4];
  memset(&bi,0,sizeof(bi)); bi.h.biSize=sizeof(BITMAPINFOHEADER); bi.h.biWidth=sw; bi.h.biHeight=shh; bi.h.biPlanes=1; bi.h.biBitCount=(WORD)kbits[kind]; bi.h.biClrUsed=(DWORD)n;
  for(i=0;i<256;i++) {DWORD r=(DWORD)i*2654435761UL; bi.c[i].rgbRed=(BYTE)(r>>8); bi.c[i].rgbGreen=(BYTE)(r>>16); bi.c[i].rgbBlue=(BYTE)(r>>24); bi.c[i].rgbReserved=(BYTE)(var==1?0x11:0);}
  if(var==1) bi.c[kidx[kind]].rgbReserved=0x5A;
  if(var<2) { /* every pixel the one index */
   int stride=((sw*kbits[kind]+31)/32)*4,x,y;
   memset(bits,0,sizeof(bits));
   for(y=0;y<shh;y++) for(x=0;x<sw;x++) {
    if(kbits[kind]==8) bits[y*stride+x]=(BYTE)kidx[kind];
    else if(kbits[kind]==4) bits[y*stride+x/2]|=(BYTE)(kidx[kind]<<((x&1)?0:4));
    else bits[y*stride+x/8]|=(BYTE)(0x80>>(x&7));
   }
  } else for(i=0;i<(int)sizeof(bits);i++) bits[i]=(BYTE)rand();
  background(dc,(BYTE)(bg?0x00:0xC3));
  ctl(0xFFFFFE13UL,s0);
  StretchDIBits(dc,8,8,dw,dh,0,0,sw,shh,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY); GdiFlush();
  ctl(0xFFFFFE13UL,s1);
  ctl(0xFB000000UL,NULL); ctl(0xFA000000UL,NULL);
  StretchDIBits(dc,328,8,dw,dh,0,0,sw,shh,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY); GdiFlush();
  ctl(0xFB000000UL|(m1[3]&0xFFFFFFUL),m3); ctl(0xFA000000UL|(m2[3]&0xFFFFFFUL),NULL);
  a=grab(dc,8,8,dw,dh); b=grab(dc,328,8,dw,dh);
  if(a&&b) for(j=0;j<(long)dw*dh;j++) {if((a[j]^b[j])&0xFFFFFFUL) cd++; if((a[j]^b[j])>>24) td++;}
  tops(a,(long)dw*dh,ta); tops(b,(long)dw*dh,tb);
  logline("%d-bit %s %s, background top %02X, %s: pixel 0,0 driver %08lX DIBENG %08lX; differ: colour %ld, top byte %ld; tops driver [%s] DIBENG [%s]\n",
   kbits[kind],shape==0?"2x2":shape==1?"3x1":"1:1",var==0?"one colour, rgbReserved 0":var==1?"one colour, rgbReserved 5A (others 11)":"random image, rgbReserved 0",bg?0:0xC3,
   s1[0]>s0[0]?"the driver drew it":"DIBENG drew both",a?a[0]:0,b?b[0]:0,cd,td,ta,tb);
  if(s1[0]>s0[0]) {if(cd) colour_bad++; if(td) top_bad++;}
  free(a); free(b);
 }
 ReleaseDC(win,dc); ctl(0xFFFFFFFFUL,NULL);
 if(have) ChangeDisplaySettingsA(&origm,0);
 sprintf(res,"%s: cases the driver drew with colour differences %ld, with top-byte differences %ld (rules in VX2TOP.TXT)",(colour_bad||top_bad)?"DIFF":"SAME",colour_bad,top_bad);
 finish(res); logline("%s\n",res); DestroyWindow(win); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
