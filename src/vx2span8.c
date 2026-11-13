/* VX2SPAN8 v1.0: driver v10.134. Why did XOR shapes at 8bpp differ by 19 pixels with ACCEL_SPAN_WAIT 2 (VX2OPT2)? XOR shapes (Polygon, Ellipse, RoundRect, Pie, no pen) drawn as
 * band rectangles with three brushes: the dithered green of VX2OPT2 (a colour pattern at 8bpp, a solid fill at 16/32), an exact colour (pure red: a solid fill everywhere) and an
 * 8x8 colour pattern brush (a colour pattern at every depth); under span waits 0 (full idle), 2 (the v10.134 default: one read, colour patterns keep the full wait), 6 (one read for
 * colour patterns too), 7 (four reads, colour patterns too) and 5 (no wait, colour patterns too). Each is drawn three times and compared with DIBENG (live mask 14: no bands);
 * DIBENG is also compared with itself. First differing pixel and count logged. 640x480x8, x16, x32. Nothing is written to the registry. VX2SPAN8.TXT / .RES. Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2SPAN8.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1; return !aborted;}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); {DWORD t=GetTickCount(); while(GetTickCount()-t<1000) {pump(); Sleep(20);}}
 return rc==DISP_CHANGE_SUCCESSFUL;
}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
#define AW 600
#define AH 440
static HDC sdc,tdc; static HBITMAP tbm,told; static DWORD *tbits,*bufA;
static int make_target(void) {
 BITMAPINFOHEADER b; void *p=NULL; memset(&b,0,sizeof(b)); b.biSize=sizeof(b); b.biWidth=AW; b.biHeight=-AH; b.biPlanes=1; b.biBitCount=32;
 tbm=CreateDIBSection(sdc,(BITMAPINFO*)&b,DIB_RGB_COLORS,&p,NULL,0); if(!tbm||!p) return 0;
 tdc=CreateCompatibleDC(sdc); told=(HBITMAP)SelectObject(tdc,tbm); tbits=(DWORD*)p; return 1;
}
static void drop_target(void) {SelectObject(tdc,told); DeleteObject(tbm); DeleteDC(tdc);}
static void shot(void) {GdiFlush(); BitBlt(tdc,0,0,AW,AH,sdc,0,0,SRCCOPY); GdiFlush();}
static int compare(int *fx,int *fy) {int i,d=0; *fx=*fy=-1; for(i=0;i<AW*AH;i++) if((bufA[i]^tbits[i])&0xFFFFFFUL) {if(!d) {*fx=i%AW; *fy=i/AW;} d++;} return d;}
static void background(void) {
 int y,x; for(y=0;y<AH;y+=8) for(x=0;x<AW;x+=40) {RECT r; HBRUSH b=CreateSolidBrush(RGB((x*7+y*3)&255,(x*2+y*5+80)&255,(x*11+y)&255)); r.left=x; r.top=y; r.right=x+40; r.bottom=y+8; FillRect(sdc,&r,b); DeleteObject(b);}
 GdiFlush();
}
static void shapes(HBRUSH br) {
 POINT star[5]={{150,30},{230,260},{40,110},{260,110},{70,260}}; HPEN p=(HPEN)SelectObject(sdc,GetStockObject(NULL_PEN)); HBRUSH o=(HBRUSH)SelectObject(sdc,br);
 SetROP2(sdc,R2_XORPEN); SetPolyFillMode(sdc,WINDING); Polygon(sdc,star,5); Ellipse(sdc,300,30,580,220); RoundRect(sdc,30,280,280,420,40,40); Pie(sdc,320,240,560,430,560,330,320,330);
 SetROP2(sdc,R2_COPYPEN); SelectObject(sdc,o); SelectObject(sdc,p); GdiFlush();
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 static const int depths[3]={8,16,32}; static const DWORD waits[5]={0,2,6,7,5};
 static const char *wname[5]={"0 full idle","2 default (colour full)","6 one read, colour too","7 four reads, colour too","5 no wait, colour too"};
 static const char *bname[3]={"dithered green","exact red","8x8 colour pattern"};
 DWORD c0[4],c1[4]; int ph,fails=0,have,bi,wi,rep,d,fx,fy; DEVMODEA origm; char res[300]; HBRUSH br[3]; HBITMAP pb; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2SPAN8.TXT","w"); logline("VX2SPAN8 v1.0 / driver v10.134\n"); finish("RUNNING: the run did not reach its end (VX2SPAN8.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xF2000006UL,c0)||c0[0]!=6) {finish("FAIL: VX85598.VXD older than v10.134"); return 1;}
 ctl(0xFFFFFFFFUL,NULL);
 bufA=(DWORD*)calloc(AW*AH,4); if(!bufA) {finish("FAIL: out of memory"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 for(ph=0;ph<3&&!aborted;ph++) {
  int bpp=depths[ph];
  if(!set_mode(640,480,bpp)) {logline("640x480x%d not available: skipped\n",bpp); continue;}
  sdc=GetDC(NULL); if(!sdc||!make_target()) {logline("no screen DC / target\n"); continue;}
  br[0]=CreateSolidBrush(RGB(120,200,40)); br[1]=CreateSolidBrush(RGB(255,0,0));
  {HDC m=CreateCompatibleDC(sdc); HBITMAP o; int y,x; pb=CreateCompatibleBitmap(sdc,8,8); o=(HBITMAP)SelectObject(m,pb);
   for(y=0;y<8;y++) for(x=0;x<8;x++) SetPixel(m,x,y,((x+y)&1)?RGB(255,255,0):((x^y)&2)?RGB(0,0,255):RGB(255,0,255)); SelectObject(m,o); DeleteDC(m); br[2]=CreatePatternBrush(pb);}
  logline("640x480x%d\n",bpp); ctl(5,NULL);
  for(bi=0;bi<3&&pump();bi++) {
   ctl(0xF700000EUL,NULL); background(); shapes(br[bi]); shot(); memcpy(bufA,tbits,AW*AH*4);
   background(); shapes(br[bi]); shot(); d=compare(&fx,&fy); logline("  %-20s DIBENG against itself: %d differing pixels\n",bname[bi],d);
   for(wi=0;wi<5&&pump();wi++) {
    int tot=0,first=-1,firsty=-1,n[3];
    ctl(0xF2000000UL|waits[wi],NULL); ctl(0xF700001EUL,NULL);
    for(rep=0;rep<3;rep++) {background(); ctl(0xFFFFFE18UL,c0); shapes(br[bi]); ctl(0xFFFFFE18UL,c1); shot(); n[rep]=compare(&fx,&fy); tot+=n[rep]; if(n[rep]&&first<0) {first=fx; firsty=fy;}}
    logline("  %-20s wait %-26s differing pixels %d / %d / %d%s, bands %lu%s\n",bname[bi],wname[wi],n[0],n[1],n[2],tot?" FAIL":"",c1[3]-c0[3],"");
    if(tot) {logline("     first at %d,%d\n",first,firsty); if(waits[wi]==2) fails++;}
    ctl(0xF700000EUL,NULL);
   }
  }
  ctl(0xFFFFFFFFUL,NULL);
  DeleteObject(br[0]); DeleteObject(br[1]); DeleteObject(br[2]); DeleteObject(pb); drop_target(); ReleaseDC(NULL,sdc);
 }
 ctl(0xFFFFFFFFUL,NULL);
 if(have) {ChangeDisplaySettingsA(&origm,0); logline("desktop put back\n");}
 sprintf(res,"%s: the v10.134 default (span wait 2) differed in %d brush/depth cases (see VX2SPAN8.TXT for every wait)",aborted?"FAIL (aborted)":fails?"FAIL":"PASS",fails);
 finish(res); logline("%s\nFinished.\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
