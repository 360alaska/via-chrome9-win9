/* VX2WIDE v1.0: wide pens and filled shapes with ROP2s that only write (R2_COPYPEN, R2_NOTCOPYPEN, R2_BLACK, R2_WHITE), driver v10.130 (ACCEL_HOST bit 32: GDI hands
 * them over as OS_POLYSCANLINE rectangle bands, which now go to the engine like the ROPs that read the screen). Every case is drawn at 640x480x8, x16 and x32 with bit 32 off
 * (live mask 30: DIBENG) and on (live mask 62); the screen is read back and compared pixel for pixel, the driver's span counter shows whether the engine drew it. Then each
 * case is timed both ways with the shipped size thresholds, so bit 32 can be judged on speed. Nothing is written to the registry. Unattended: read VX2WIDE.TXT
 * (VX2WIDE.RES says RUNNING until it ends). Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2WIDE.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1; return !aborted;}
static double now(void) {LARGE_INTEGER c,f; if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;} return (double)GetTickCount();}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); {DWORD t=GetTickCount(); while(GetTickCount()-t<1000) {pump(); Sleep(20);}}
 return rc==DISP_CHANGE_SUCCESSFUL;
}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static void setmask(DWORD m) {ctl(0xF7000000UL|(m&63UL),NULL);}
#define M_OFF 30UL
#define M_ON 62UL

#define AW 600
#define AH 440
static HDC sdc,tdc; static HBITMAP tbm,told,pbm; static DWORD *tbits,*bufA;
static int make_target(void) {
 BITMAPINFOHEADER b; void *p=NULL; memset(&b,0,sizeof(b)); b.biSize=sizeof(b); b.biWidth=AW; b.biHeight=-AH; b.biPlanes=1; b.biBitCount=32;
 tbm=CreateDIBSection(sdc,(BITMAPINFO*)&b,DIB_RGB_COLORS,&p,NULL,0); if(!tbm||!p) return 0;
 tdc=CreateCompatibleDC(sdc); told=(HBITMAP)SelectObject(tdc,tbm); tbits=(DWORD*)p; return 1;
}
static void drop_target(void) {SelectObject(tdc,told); DeleteObject(tbm); DeleteDC(tdc);}
static void shot(void) {GdiFlush(); BitBlt(tdc,0,0,AW,AH,sdc,0,0,SRCCOPY); GdiFlush();}
static void background(void) {
 int y,x; for(y=0;y<AH;y+=8) for(x=0;x<AW;x+=40) {RECT r; HBRUSH b=CreateSolidBrush(RGB((x*7+y*3)&255,(x*2+y*5+80)&255,(x*11+y)&255)); r.left=x; r.top=y; r.right=x+40; r.bottom=y+8; FillRect(sdc,&r,b); DeleteObject(b);}
 GdiFlush();
}
static void wline(int w,COLORREF c,int x0,int y0,int x1,int y1) {HPEN p=CreatePen(PS_SOLID,w,c),o=(HPEN)SelectObject(sdc,p); MoveToEx(sdc,x0,y0,NULL); LineTo(sdc,x1,y1); SelectObject(sdc,o); DeleteObject(p);}

typedef void (*drawfn)(void);
static void c_lines(void) { /* widths 2..16, every direction, reversed ends, off the edge */
 static const int ws[7]={2,3,4,7,8,15,16}; int i;
 SetROP2(sdc,R2_COPYPEN);
 for(i=0;i<7;i++) {
  int y=20+i*30; COLORREF c=RGB(40*i,255-30*i,(90*i)&255);
  wline(ws[i],c,20,y,200,y); wline(ws[i],c,230+i*12,20,230+i*12,230); wline(ws[i],c,330,20+i*25,420,110+i*25); wline(ws[i],c,560,30+i*20,440,45+i*22);
  wline(ws[i],c,20+i*40,260,40+i*40,430); wline(ws[i],c,320+i*30,430,300+i*30,250); wline(ws[i],c,590,250+i*25,650,265+i*25); wline(ws[i],c,-20,400+i*5,60,430);
 }
 wline(5,RGB(255,255,0),500,300,500,300); wline(9,RGB(0,255,255),520,320,521,320);
}
static void c_polyline(void) { /* joins, acute angles, a reversal, a closed figure */
 POINT z[6]={{30,40},{250,60},{40,180},{260,200},{40,320},{250,340}},acute[3]={{320,40},{580,60},{330,90}},rev[3]={{320,150},{560,150},{400,150}},box[5]={{340,240},{560,260},{540,420},{360,400},{340,240}};
 HPEN p; SetROP2(sdc,R2_COPYPEN);
 p=CreatePen(PS_SOLID,9,RGB(250,80,20)); SelectObject(sdc,p); Polyline(sdc,z,6); SelectObject(sdc,GetStockObject(BLACK_PEN)); DeleteObject(p);
 p=CreatePen(PS_SOLID,6,RGB(30,90,250)); SelectObject(sdc,p); Polyline(sdc,acute,3); Polyline(sdc,rev,3); SelectObject(sdc,GetStockObject(BLACK_PEN)); DeleteObject(p);
 p=CreatePen(PS_SOLID,13,RGB(20,200,60)); SelectObject(sdc,p); Polyline(sdc,box,5); SelectObject(sdc,GetStockObject(BLACK_PEN)); DeleteObject(p);
}
static void c_shapes(void) { /* solid brushes, no outline, R2_COPYPEN */
 POINT star[5]={{150,30},{230,260},{40,110},{260,110},{70,260}}; HBRUSH s=CreateSolidBrush(RGB(120,200,40)),o; HPEN p=(HPEN)SelectObject(sdc,GetStockObject(NULL_PEN));
 SetROP2(sdc,R2_COPYPEN); SetPolyFillMode(sdc,WINDING); o=(HBRUSH)SelectObject(sdc,s); Polygon(sdc,star,5); Ellipse(sdc,300,30,580,220); RoundRect(sdc,30,280,280,420,40,40); Pie(sdc,320,240,560,430,560,330,320,330);
 SelectObject(sdc,o); SelectObject(sdc,p); DeleteObject(s);
}
static void c_outlined(void) { /* wide outline pen and a hatched or pattern brush together */
 HPEN p=CreatePen(PS_SOLID,7,RGB(200,20,120)),op=(HPEN)SelectObject(sdc,p); HBRUSH h=CreateHatchBrush(HS_DIAGCROSS,RGB(0,0,255)),pb=CreatePatternBrush(pbm),o;
 SetROP2(sdc,R2_COPYPEN); SetBkColor(sdc,RGB(255,255,200)); SetBkMode(sdc,OPAQUE);
 o=(HBRUSH)SelectObject(sdc,h); Ellipse(sdc,20,20,280,200); Rectangle(sdc,320,30,580,210);
 SelectObject(sdc,pb); Ellipse(sdc,30,240,290,420); RoundRect(sdc,320,240,580,420,60,60);
 SelectObject(sdc,o); SelectObject(sdc,op); DeleteObject(p); DeleteObject(h); DeleteObject(pb);
}
static void c_otherops(void) { /* the other ROP2s that only write */
 static const int r2[3]={R2_NOTCOPYPEN,R2_BLACK,R2_WHITE}; int i; HBRUSH s=CreateSolidBrush(RGB(60,160,220)),o=(HBRUSH)SelectObject(sdc,s); HPEN p=(HPEN)SelectObject(sdc,GetStockObject(NULL_PEN));
 for(i=0;i<3;i++) {SetROP2(sdc,r2[i]); Ellipse(sdc,20+i*190,20,190+i*190,200); SelectObject(sdc,p); wline(6+i*3,RGB(250,200,0),20+i*190,250,180+i*190,420); SelectObject(sdc,GetStockObject(NULL_PEN));}
 SetROP2(sdc,R2_COPYPEN); SelectObject(sdc,o); SelectObject(sdc,p); DeleteObject(s);
}
static void c_xor(void) { /* regression: the ROPs that read the screen (already on the engine since v10.118) */
 HPEN p=CreatePen(PS_SOLID,8,RGB(255,255,255)),op=(HPEN)SelectObject(sdc,p);
 SetROP2(sdc,R2_XORPEN); MoveToEx(sdc,20,20,NULL); LineTo(sdc,580,420); MoveToEx(sdc,580,20,NULL); LineTo(sdc,20,420); Ellipse(sdc,200,120,400,320);
 SetROP2(sdc,R2_COPYPEN); SelectObject(sdc,op); DeleteObject(p);
}
typedef struct {const char *name; drawfn fn; int engine;} case_t; /* engine: 1 = the band path should draw it with bit 32 on */
static const case_t cases[]={
 {"wide lines R2_COPYPEN (widths 2-16, all directions, off the edge, zero length)",c_lines,1},
 {"wide polylines R2_COPYPEN (joins, acute angle, reversal, closed)",c_polyline,1},
 {"filled shapes R2_COPYPEN, solid brush (Polygon, Ellipse, RoundRect, Pie)",c_shapes,1},
 {"wide outline pen + hatched and pattern brushes R2_COPYPEN",c_outlined,1},
 {"R2_NOTCOPYPEN / R2_BLACK / R2_WHITE shapes and wide lines",c_otherops,1},
 {"regression: R2_XORPEN wide lines and shape (bands since v10.118)",c_xor,0}};
#define NCASE (sizeof(cases)/sizeof(cases[0]))

static int run_case(int ci,DWORD *engaged) {
 DWORD c0[4],c1[4],k0[4],k1[4],cm=0xFFFFFFUL,fa=0,fb=0; int x,y,diff=0,fx=-1,fy=-1;
 background(); setmask(M_OFF); cases[ci].fn(); shot(); memcpy(bufA,tbits,AW*AH*4);
 background(); ctl(0xFFFFFE18UL,c0); ctl(0xFFFFFE19UL,k0); setmask(M_ON); cases[ci].fn(); GdiFlush(); ctl(0xFFFFFE18UL,c1); ctl(0xFFFFFE19UL,k1); shot(); setmask(M_OFF);
 for(y=0;y<AH;y++) for(x=0;x<AW;x++) {DWORD a=bufA[y*AW+x]&cm,b=tbits[y*AW+x]&cm; if(a!=b) {if(!diff) {fx=x; fy=y; fa=a; fb=b;} diff++;}}
 *engaged=c1[3]-c0[3];
 logline("  %-82s %s: %d differing pixels; engine bands %lu; handed back %lu (last reason %lu)\n",cases[ci].name,diff?"FAIL":"ok",diff,*engaged,k1[0]-k0[0],k1[1]);
 if(diff) {
  int d2=0; logline("     first difference at %d,%d: DIBENG %06lX, engine %06lX\n",fx,fy,fa,fb);
  background(); setmask(M_OFF); cases[ci].fn(); shot(); for(y=0;y<AH;y++) for(x=0;x<AW;x++) if((bufA[y*AW+x]&cm)!=(tbits[y*AW+x]&cm)) d2++;
  logline("     DIBENG drawn a second time: %d pixels differ from its first run%s\n",d2,d2?" (the drawing itself is not repeatable)":"");
 }
 return diff==0;
}
static void time_case(int ci,double *sum) {
 double t[2]; int m,i,n=8;
 for(m=0;m<2;m++) {double s; background(); setmask(m?M_ON:M_OFF); s=now(); for(i=0;i<n&&pump();i++) cases[ci].fn(); GdiFlush(); t[m]=(now()-s)/n;}
 setmask(M_OFF); if(cases[ci].engine) {sum[0]+=t[0]; sum[1]+=t[1];}
 logline("  %-82s bit 32 off %8.2f ms, on %8.2f ms (%s)\n",cases[ci].name,t[0],t[1],t[1]<t[0]?"faster":"slower");
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 static const int depths[3]={8,16,32}; DWORD c[4],e; int phase,ci,fails=0,idle=0,total=0,have,slower=0; DEVMODEA origm; char res[400]; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2WIDE.TXT","w"); logline("VX2WIDE v1.0 / driver v10.130\n"); finish("RUNNING: the run did not reach its end (VX2WIDE.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 ctl(0xFFFFFE19UL,c); logline("live ACCEL_HOST mask at the start %lu (62 expected with v10.130)\n",c[2]);
 setmask(M_ON); ctl(0xFFFFFE19UL,c); if(c[2]!=M_ON) {finish("FAIL: VX85598.VXD older than v10.130 (mask bit 32 not kept)"); return 1;}
 bufA=(DWORD*)calloc(AW*AH,4); if(!bufA) {finish("FAIL: out of memory"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 for(phase=0;phase<3&&!aborted;phase++) {
  int bpp=depths[phase]; double sum[2]={0,0};
  static const WORD pat[8]={0xAA,0x55,0xF0,0x0F,0xCC,0x33,0x81,0x7E};
  if(!set_mode(640,480,bpp)) {logline("640x480x%d not available: skipped\n",bpp); continue;}
  sdc=GetDC(NULL); if(!sdc||!make_target()) {logline("no screen DC / target: skipped\n"); continue;}
  pbm=CreateBitmap(8,8,1,1,pat);
  logline("640x480x%d: pixels (engine against DIBENG, size thresholds off)\n",bpp);
  ctl(5,NULL);
  for(ci=0;ci<(int)NCASE&&pump();ci++) {total++; if(!run_case(ci,&e)) fails++; else if(!e) idle++;}
  ctl(0xFFFFFFFFUL,NULL); setmask(M_OFF);
  logline("640x480x%d: times per case (shipped thresholds)\n",bpp);
  for(ci=0;ci<(int)NCASE&&pump();ci++) time_case(ci,sum);
  logline("  write-only cases together: bit 32 off %.2f ms, on %.2f ms\n",sum[0],sum[1]); if(sum[1]>=sum[0]) slower++;
  DeleteObject(pbm); drop_target(); ReleaseDC(NULL,sdc);
 }
 ctl(0xFFFFFFFFUL,NULL);
 if(have) {ChangeDisplaySettingsA(&origm,0); logline("desktop put back\n");}
 if(aborted) sprintf(res,"FAIL: aborted");
 else sprintf(res,"%s: %d cases, %d with differing pixels, %d where the engine drew nothing; bit 32 slower at %d of 3 depths (see VX2WIDE.TXT)",fails?"FAIL":idle?"PARTIAL":"PASS",total,fails,idle,slower);
 finish(res); logline("%s\nFinished.\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
