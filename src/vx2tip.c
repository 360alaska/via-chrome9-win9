/* VX2TIP v1.0: driver v10.136. Who is right at the star's tip? (VX2COL8 v1.1: every single 8bpp colour-pattern fill matches DIBENG, but XOR shapes drawn as bands differ by 3 to 6 pixels
 * at 149,32 / 150,33 in every mode, also with the tile and all registers written again per rectangle.) At 640x480x8 with an 8x8 colour pattern brush and R2_XORPEN, three pictures of
 * the same star: P = DIBENG's own Polygon (engine off), E = the driver's bands (guard off), R = the rectangles of the star's region (GetRegionData of the same polygon) each drawn with
 * PatBlt PATINVERT by DIBENG (engine off). Every pixel where they disagree is listed with the three values, plus the region rectangles around the tip. Also the same star with an
 * exact red brush. Nothing is written to the registry. VX2TIP.TXT / .RES. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2TIP.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); {DWORD t=GetTickCount(); while(GetTickCount()-t<1000) {pump(); Sleep(20);}}
 return rc==DISP_CHANGE_SUCCESSFUL;
}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
#define AW 300
#define AH 280
static HDC sdc,tdc; static HBITMAP tbm,told; static DWORD *tbits,*imgP,*imgE,*imgR;
static int make_target(void) {
 BITMAPINFOHEADER b; void *p=NULL; memset(&b,0,sizeof(b)); b.biSize=sizeof(b); b.biWidth=AW; b.biHeight=-AH; b.biPlanes=1; b.biBitCount=32;
 tbm=CreateDIBSection(sdc,(BITMAPINFO*)&b,DIB_RGB_COLORS,&p,NULL,0); if(!tbm||!p) return 0;
 tdc=CreateCompatibleDC(sdc); told=(HBITMAP)SelectObject(tdc,tbm); tbits=(DWORD*)p; return 1;
}
static void shot(DWORD *to) {GdiFlush(); BitBlt(tdc,0,0,AW,AH,sdc,0,0,SRCCOPY); GdiFlush(); memcpy(to,tbits,AW*AH*4);}
static void background(void) {RECT r; HBRUSH b=CreateSolidBrush(RGB(0,0,0)); r.left=0; r.top=0; r.right=AW; r.bottom=AH; FillRect(sdc,&r,b); DeleteObject(b); GdiFlush();}
static POINT star[5]={{150,30},{230,260},{40,110},{260,110},{70,260}};
static int run(HBRUSH br,const char *name) {
 HPEN p; HBRUSH o; HRGN rg; DWORD n,i,k,cnt=0; RGNDATA *rd; RECT *rc; int x,y,listed=0;
 SetPolyFillMode(sdc,WINDING);
 /* P: DIBENG's Polygon */
 ctl(2,NULL); background(); p=(HPEN)SelectObject(sdc,GetStockObject(NULL_PEN)); o=(HBRUSH)SelectObject(sdc,br); SetROP2(sdc,R2_XORPEN); Polygon(sdc,star,5); SetROP2(sdc,R2_COPYPEN); shot(imgP);
 /* R: the region's rectangles, PatBlt PATINVERT by DIBENG */
 background(); rg=CreatePolygonRgn(star,5,WINDING); n=GetRegionData(rg,0,NULL); rd=(RGNDATA*)malloc(n); GetRegionData(rg,n,rd); rc=(RECT*)rd->Buffer; cnt=rd->rdh.nCount;
 for(i=0;i<cnt;i++) PatBlt(sdc,rc[i].left,rc[i].top,rc[i].right-rc[i].left,rc[i].bottom-rc[i].top,PATINVERT); shot(imgR); ctl(0xFFFFFFFFUL,NULL);
 /* E: the driver's bands, guard off */
 ctl(5,NULL); ctl(0xEF000000UL,NULL); background(); SetROP2(sdc,R2_XORPEN); Polygon(sdc,star,5); SetROP2(sdc,R2_COPYPEN); shot(imgE); ctl(0xFFFFFFFFUL,NULL);
 SelectObject(sdc,o); SelectObject(sdc,p);
 logline("-- %s: %lu region rectangles; the first ones (the tip):\n",name,cnt);
 for(i=0;i<cnt&&i<12;i++) logline("   [%lu] x %ld..%ld  y %ld..%ld\n",i,rc[i].left,rc[i].right,rc[i].top,rc[i].bottom);
 {int pe=0,pr=0,er=0; for(k=0;k<(DWORD)(AW*AH);k++) {DWORD a=imgP[k]&0xFFFFFF,b=imgE[k]&0xFFFFFF,c=imgR[k]&0xFFFFFF; if(a!=b) pe++; if(a!=c) pr++; if(b!=c) er++;}
  logline("   pixels: Polygon vs bands %d, Polygon vs region PatBlts %d, bands vs region PatBlts %d\n",pe,pr,er);}
 for(y=0;y<AH;y++) for(x=0;x<AW;x++) {DWORD a=imgP[y*AW+x]&0xFFFFFF,b=imgE[y*AW+x]&0xFFFFFF,c=imgR[y*AW+x]&0xFFFFFF; if((a!=b||a!=c)&&listed<40) {logline("   %3d,%3d: Polygon %06lX  bands %06lX  region PatBlt %06lX\n",x,y,a,b,c); listed++;}}
 DeleteObject(rg); free(rd); return listed;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 DWORD c[4]; int have; DEVMODEA origm; HBRUSH br[2]; HBITMAP pb; char res[200]; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2TIP.TXT","w"); logline("VX2TIP v1.0 / driver v10.136\n"); finish("RUNNING");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL); if(dev==INVALID_HANDLE_VALUE) {finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xEF000003UL,c)||c[0]!=3) {finish("FAIL: VX85598.VXD older than v10.136"); return 1;} ctl(0xFFFFFFFFUL,NULL);
 imgP=(DWORD*)calloc(AW*AH,4); imgE=(DWORD*)calloc(AW*AH,4); imgR=(DWORD*)calloc(AW*AH,4); if(!imgP||!imgE||!imgR) {finish("FAIL: memory"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 if(!set_mode(640,480,8)) {finish("FAIL: 640x480x8"); return 1;}
 sdc=GetDC(NULL); if(!sdc||!make_target()) {finish("FAIL: DC"); return 1;}
 {HDC m=CreateCompatibleDC(sdc); HBITMAP o; int y,x; pb=CreateCompatibleBitmap(sdc,8,8); o=(HBITMAP)SelectObject(m,pb);
  for(y=0;y<8;y++) for(x=0;x<8;x++) SetPixel(m,x,y,((x+y)&1)?RGB(255,255,0):((x^y)&2)?RGB(0,0,255):RGB(255,0,255)); SelectObject(m,o); DeleteDC(m); br[0]=CreatePatternBrush(pb);}
 br[1]=CreateSolidBrush(RGB(255,0,0));
 run(br[0],"8x8 colour pattern"); run(br[1],"exact red");
 DeleteObject(br[0]); DeleteObject(br[1]); DeleteObject(pb); SelectObject(tdc,told); DeleteObject(tbm); DeleteDC(tdc); ReleaseDC(NULL,sdc);
 ctl(0xFFFFFFFFUL,NULL); if(have) ChangeDisplaySettingsA(&origm,0);
 sprintf(res,"DONE (see VX2TIP.TXT)"); finish(res); logline("%s\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
