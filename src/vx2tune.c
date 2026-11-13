/* VX2TUNE v1.1 (.533: the measured values reported, nothing written: the driver reads neither value). v1.0: driver v10.137 (no driver change needed). Two settings tuned on the board and written to HKLM\Software\VX85598 (restart Windows to use them):
 * 1. ACCEL_FILL_MIN (default 16384 = 128x128, set before ACCEL_OP_WAIT made every engine call about 8 us cheaper). PatBlt PATCOPY (the ROPs that read the screen skip the threshold
 *    since v10.133) with a solid, a hatched and an 8x8 colour pattern brush, squares 16 to 160 plus 256x8 and 8x256, at 640x480x8, x16 and x32: DIBENG (engine off) against the
 *    engine (thresholds off), microseconds per call. The new value is the smallest square area from which the engine wins for every brush at every depth (never below 1024).
 * 2. ACCEL_SPAN_WAIT 5 (no wait between band rectangles, colour patterns too) against 6 (one idle read, the default): 60 random XOR shape sets per brush and depth, each drawn
 *    with 6 and with 5 and compared pixel for pixel (the engine against itself, so DIBENG's 8bpp tip quirk does not count). 5 is written only if every set matched.
 * Unattended: VX2TUNE.TXT / .RES. Esc stops (nothing written then). */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2TUNE.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1; return !aborted;}
static double now(void) {LARGE_INTEGER c,f; if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;} return (double)GetTickCount();}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); {DWORD t=GetTickCount(); while(GetTickCount()-t<1000) {pump(); Sleep(20);}}
 return rc==DISP_CHANGE_SUCCESSFUL;
}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static DWORD engine_ops(void) {DWORD o[4]; ctl(0xFFFFFFFEUL,o); return o[0];}
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
static int compare(void) {int i,d=0; for(i=0;i<AW*AH;i++) if((bufA[i]^tbits[i])&0xFFFFFFUL) d++; return d;}
static void background(void) {
 int y,x; for(y=0;y<AH;y+=8) for(x=0;x<AW;x+=40) {RECT r; HBRUSH b=CreateSolidBrush(RGB((x*7+y*3)&255,(x*2+y*5+80)&255,(x*11+y)&255)); r.left=x; r.top=y; r.right=x+40; r.bottom=y+8; FillRect(sdc,&r,b); DeleteObject(b);}
 GdiFlush();
}
static DWORD seed; static int rnd(int n) {seed=seed*1103515245UL+12345UL; return (int)((seed>>8)%(DWORD)n);}

/* ---- 1: fill threshold ---- */
static const int sq[9]={16,24,32,48,64,80,96,128,160};
static double time_fill(HBRUSH br,int w,int h,int engine,DWORD *ops) {
 int k,n=w*h>=16384?100:300; double s; HBRUSH o=(HBRUSH)SelectObject(sdc,br); DWORD e0;
 SetBkColor(sdc,RGB(0,0,0)); ctl(engine?5UL:2UL,NULL); background(); e0=engine_ops(); s=now();
 for(k=0;k<n&&pump();k++) PatBlt(sdc,(k*37)%(AW-w),(k*23)%(AH-h),w,h,PATCOPY);
 GdiFlush(); s=(now()-s)*1000.0/n; *ops=engine_ops()-e0; ctl(0xFFFFFFFFUL,NULL); SelectObject(sdc,o); return s;
}
static int part1(HBRUSH *br,const char **bname,int bpp) { /* returns the smallest square side from which the engine wins for every brush, 0 = never up to 160 */
 int bi,i,side_all=0;
 for(bi=0;bi<3&&pump();bi++) {
  int win_from=0; char line[600]; int n=sprintf(line,"  %-20s",bname[bi]);
  for(i=8;i>=0;i--) {DWORD o1,o2; double td=time_fill(br[bi],sq[i],sq[i],0,&o1),te=time_fill(br[bi],sq[i],sq[i],1,&o2); (void)o1;
   if(te<td&&o2&&(win_from==sq[i+1>8?8:i+1]||i==8)) win_from=sq[i];
   n+=sprintf(line+n," %d:%.0f/%.0f%s",sq[i],td,te,o2?"":"(not engine)");}
  {DWORD o1,o2; double a=time_fill(br[bi],256,8,0,&o1),b=time_fill(br[bi],256,8,1,&o2),c=time_fill(br[bi],8,256,0,&o1),d=time_fill(br[bi],8,256,1,&o2); n+=sprintf(line+n," | 256x8:%.0f/%.0f 8x256:%.0f/%.0f",a,b,c,d);}
  logline("%s  -> engine wins from %d\n",line,win_from);
  if(!win_from) side_all=999; else if(win_from>side_all) side_all=win_from;
 }
 (void)bpp; return side_all;
}

/* ---- 2: no-wait soak ---- */
static void shapes(HBRUSH br,DWORD sd) {
 int i; HPEN p=(HPEN)SelectObject(sdc,GetStockObject(NULL_PEN)); HBRUSH o=(HBRUSH)SelectObject(sdc,br); seed=sd;
 SetROP2(sdc,R2_XORPEN); SetPolyFillMode(sdc,(sd&1)?WINDING:ALTERNATE); SetBkColor(sdc,RGB(0,0,0));
 for(i=0;i<6;i++) {
  int x=rnd(AW-60),y=rnd(AH-60),w=8+rnd(AW-x-8),h=8+rnd(AH-y-8),k=rnd(4);
  if(k==0) Ellipse(sdc,x,y,x+w,y+h); else if(k==1) RoundRect(sdc,x,y,x+w,y+h,1+rnd(40),1+rnd(40));
  else if(k==2) Pie(sdc,x,y,x+w,y+h,x+rnd(w),y,x,y+rnd(h));
  else {POINT pt[7]; int j; for(j=0;j<7;j++) {pt[j].x=x+rnd(w); pt[j].y=y+rnd(h);} Polygon(sdc,pt,7);}
 }
 SetROP2(sdc,R2_COPYPEN); SelectObject(sdc,o); SelectObject(sdc,p); GdiFlush();
}
static int part2(HBRUSH *br,const char **bname,int *bad_sets) {
 int bi,k,dsum=0; DWORD c0[4],c1[4];
 for(bi=0;bi<3&&pump();bi++) {
  int bad=0,first=-1; DWORD bands=0;
  for(k=0;k<60&&pump();k++) {
   DWORD sd=20261022UL+(DWORD)k*7919UL+(DWORD)bi*104729UL; int d;
   ctl(5,NULL); ctl(0xF2000006UL,NULL); background(); shapes(br[bi],sd); shot(); memcpy(bufA,tbits,AW*AH*4);
   ctl(0xF2000005UL,NULL); background(); ctl(0xFFFFFE18UL,c0); shapes(br[bi],sd); ctl(0xFFFFFE18UL,c1); shot(); d=compare(); bands+=c1[3]-c0[3];
   if(d) {bad++; dsum+=d; if(first<0) first=k;}
  }
  ctl(0xFFFFFFFFUL,NULL);
  logline("  %-20s 60 random XOR shape sets: %d differ between one read and no wait%s; %lu bands with no wait\n",bname[bi],bad,first>=0?" (FAIL)":"",bands);
  if(first>=0) logline("     first differing set: %d\n",first);
  *bad_sets+=bad;
 }
 return dsum;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 static const int depths[3]={8,16,32}; static const char *bname[3]={"solid","hatched","8x8 colour pattern"};
 DWORD c[4]; int ph,have,side=0,bad_sets=0,depths_run=0; DEVMODEA origm; char res[400]; HBRUSH br[3]; HBITMAP pb; DWORD fill_new=0; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2TUNE.TXT","w"); logline("VX2TUNE v1.0 / driver v10.137\n"); finish("RUNNING: the run did not reach its end (VX2TUNE.TXT ends where it stopped; nothing written)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xEF000001UL,c)||c[0]!=1) {finish("FAIL: VX85598.VXD older than v10.137"); return 1;}
 ctl(0xFFFFFFFFUL,NULL);
 bufA=(DWORD*)calloc(AW*AH,4); if(!bufA) {finish("FAIL: out of memory"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 for(ph=0;ph<3&&!aborted;ph++) {
  int bpp=depths[ph],s;
  if(!set_mode(640,480,bpp)) {logline("640x480x%d not available: skipped\n",bpp); continue;}
  sdc=GetDC(NULL); if(!sdc||!make_target()) {logline("no screen DC / target\n"); continue;}
  {HDC m=CreateCompatibleDC(sdc); HBITMAP o; int y,x; pb=CreateCompatibleBitmap(sdc,8,8); o=(HBITMAP)SelectObject(m,pb);
   for(y=0;y<8;y++) for(x=0;x<8;x++) SetPixel(m,x,y,((x+y)&1)?RGB(255,255,0):((x^y)&2)?RGB(0,0,255):RGB(255,0,255)); SelectObject(m,o); DeleteDC(m); br[2]=CreatePatternBrush(pb);}
  br[0]=CreateSolidBrush(bpp==8?RGB(255,0,0):RGB(120,200,40)); br[1]=CreateHatchBrush(HS_DIAGCROSS,RGB(255,255,0));
  logline("640x480x%d, 1. PatBlt PATCOPY, us per call DIBENG/engine by square side (largest first):\n",bpp);
  s=part1(br,bname,bpp); if(s>side) side=s;
  logline("640x480x%d, 2. no-wait soak\n",bpp); part2(br,bname,&bad_sets);
  depths_run++;
  DeleteObject(br[0]); DeleteObject(br[1]); DeleteObject(br[2]); DeleteObject(pb); drop_target(); ReleaseDC(NULL,sdc);
 }
 ctl(0xFFFFFFFFUL,NULL);
 if(have) {ChangeDisplaySettingsA(&origm,0); logline("desktop put back\n");}
 if(aborted||depths_run<3) {finish("FAIL: not every depth ran; nothing written"); logline("not every depth ran: nothing written\n"); if(lf) fclose(lf); return 0;}
 if(side&&side<999) {fill_new=(DWORD)side*(DWORD)side; if(fill_new<1024) fill_new=1024; if(fill_new>16384) fill_new=16384;}
 /* .533: nothing written (the driver reads neither ACCEL_FILL_MIN nor ACCEL_SPAN_WAIT): the measured values are reported */
 if(fill_new) logline("fill threshold measured: %lu (the engine wins from %dx%d for every brush and depth)\n",fill_new,side,side);
 else logline("fill threshold: none found (some brush never won up to 160x160)\n");
 if(!bad_sets) logline("span wait: no wait needed between band rectangles (every set matched)\n");
 else logline("span wait: %d sets differed: keep the driver's default\n",bad_sets);
 sprintf(res,"%s: fill threshold %s%lu, span wait %s (%d differing sets); .533: nothing written (see VX2TUNE.TXT)",bad_sets?"PARTIAL":"PASS",fill_new?"":"none found, ",fill_new?fill_new:16384UL,bad_sets?"keep the default (6)":"5",bad_sets);
 finish(res); logline("%s\nFinished.\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
