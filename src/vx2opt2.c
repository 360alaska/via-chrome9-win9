/* VX2OPT2 v1.0: driver v10.133. Single engine operations (fills, copies, lines) three ways: "before" (ACCEL_OP_WAIT 0: 32 stable idle reads after each, ACCEL_DREAD_ANY 0: size
 * thresholds for every ROP), "one read" (OP_WAIT 1, DREAD 0) and "new" (OP_WAIT 1, DREAD 1: ROPs that read the screen go to the engine at any size). DIBENG (engine off) is the
 * pixel reference. At 640x480x8, x16 and x32: each case is drawn once per configuration and compared pixel for pixel with DIBENG, the engine operation counter shows how many
 * went to the engine, then each is timed (microseconds per call). Last, XOR shapes with the new default ACCEL_SPAN_WAIT 2 against 0. Nothing is written to the registry.
 * Unattended: VX2OPT2.TXT / .RES. Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2OPT2.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1; return !aborted;}
static double now(void) {LARGE_INTEGER c,f; if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;} return (double)GetTickCount();}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); {DWORD t=GetTickCount(); while(GetTickCount()-t<1000) {pump(); Sleep(20);}}
 return rc==DISP_CHANGE_SUCCESSFUL;
}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static DWORD engine_ops(void) {DWORD o[4]; ctl(0xFFFFFFFEUL,o); return o[0];} /* the engine's operation counter */

#define AW 600
#define AH 440
static HDC sdc,tdc; static HBITMAP tbm,told; static DWORD *tbits,*bufA; static HBRUSH bsolid,bhatch;
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
/* one call of a case; k varies the position. sz = the size of that case */
typedef void (*opfn)(int k,int sz);
static void o_patinv(int k,int sz) {HBRUSH o=(HBRUSH)SelectObject(sdc,bsolid); PatBlt(sdc,(k*37)%(AW-sz),(k*23)%(AH-sz),sz,sz,PATINVERT); SelectObject(sdc,o);}
static void o_hatchinv(int k,int sz) {HBRUSH o=(HBRUSH)SelectObject(sdc,bhatch); SetBkColor(sdc,RGB(0,0,0)); PatBlt(sdc,(k*37)%(AW-sz),(k*23)%(AH-sz),sz,sz,PATINVERT); SelectObject(sdc,o);}
static void o_dstinv(int k,int sz) {PatBlt(sdc,(k*41)%(AW-sz),(k*29)%(AH-sz),sz,sz,DSTINVERT);}
static void o_patcopy(int k,int sz) {HBRUSH o=(HBRUSH)SelectObject(sdc,bsolid); PatBlt(sdc,(k*37)%(AW-sz),(k*23)%(AH-sz),sz,sz,PATCOPY); SelectObject(sdc,o);}
static void o_copy(int k,int sz) {BitBlt(sdc,(k*31)%(AW-sz),(k*17)%(AH-sz),sz,sz,sdc,(k*13)%(AW-sz),(k*19)%(AH-sz),SRCCOPY);}
static void o_srcinv(int k,int sz) {BitBlt(sdc,(k*31)%(AW-sz),(k*17)%(AH-sz),sz,sz,sdc,(k*13)%(AW-sz),(k*19)%(AH-sz),SRCINVERT);}
static void o_line(int k,int sz) {HPEN p=(HPEN)SelectObject(sdc,GetStockObject(WHITE_PEN)); SetROP2(sdc,(k&1)?R2_XORPEN:R2_COPYPEN); MoveToEx(sdc,(k*37)%AW,(k*23)%AH,NULL); LineTo(sdc,(k*37+sz*3)%AW,(k*23+sz)%AH); SetROP2(sdc,R2_COPYPEN); SelectObject(sdc,p);}
static const struct {const char *name; opfn fn; int sz;} cases[]={
 {"PatBlt PATINVERT solid 16x16",o_patinv,16},{"PatBlt PATINVERT solid 32x32",o_patinv,32},{"PatBlt PATINVERT solid 64x64",o_patinv,64},{"PatBlt PATINVERT solid 96x96",o_patinv,96},{"PatBlt PATINVERT solid 160x160",o_patinv,160},
 {"PatBlt PATINVERT hatched 32x32",o_hatchinv,32},{"PatBlt PATINVERT hatched 96x96",o_hatchinv,96},
 {"PatBlt DSTINVERT 32x32",o_dstinv,32},{"PatBlt DSTINVERT 96x96",o_dstinv,96},
 {"PatBlt PATCOPY 32x32",o_patcopy,32},{"PatBlt PATCOPY 160x160",o_patcopy,160},{"PatBlt PATCOPY 300x300",o_patcopy,300},
 {"BitBlt screen SRCCOPY 32x32",o_copy,32},{"BitBlt screen SRCCOPY 200x200",o_copy,200},
 {"BitBlt screen SRCINVERT 32x32",o_srcinv,32},{"BitBlt screen SRCINVERT 96x96",o_srcinv,96},
 {"lines, thin pen, COPYPEN/XORPEN alternating",o_line,60}};
#define NCASE (sizeof(cases)/sizeof(cases[0]))
static const struct {const char *name; DWORD wait,dread;} cfg[3]={{"before",0,0},{"one read",1,0},{"new",1,1}};
static void setcfg(int c) {ctl(0xF1000000UL|cfg[c].wait,NULL); ctl(0xF0000000UL|cfg[c].dread,NULL);}
static void seq(int ci,int n) {int k; for(k=0;k<n&&pump();k++) cases[ci].fn(k,cases[ci].sz);}
static int run_depth(int *fails) {
 int ci,c; DWORD e0;
 for(ci=0;ci<(int)NCASE&&pump();ci++) {
  double t[4]; int d[3]; DWORD ops[3]; int n=cases[ci].sz>=200?60:cases[ci].sz>=96?200:400;
  ctl(2,NULL); background(); seq(ci,40); shot(); memcpy(bufA,tbits,AW*AH*4); ctl(0xFFFFFFFFUL,NULL); /* DIBENG reference (engine off) */
  for(c=0;c<3;c++) {setcfg(c); background(); e0=engine_ops(); seq(ci,40); GdiFlush(); ops[c]=engine_ops()-e0; shot(); d[c]=compare(); if(d[c]) (*fails)++;}
  {double s; int k; ctl(2,NULL); background(); s=now(); seq(ci,n); GdiFlush(); t[0]=(now()-s)*1000.0/n; ctl(0xFFFFFFFFUL,NULL);
   for(c=0;c<3;c++) {setcfg(c); background(); s=now(); for(k=0;k<n&&pump();k++) cases[ci].fn(k,cases[ci].sz); GdiFlush(); t[c+1]=(now()-s)*1000.0/n;}}
  logline("  %-44s DIBENG %7.1f | before %7.1f (%2lu/40 engine, %s) | one read %7.1f (%2lu/40, %s) | new %7.1f (%2lu/40, %s) us\n",cases[ci].name,t[0],
   t[1],ops[0],d[0]?"PIXELS DIFFER":"ok",t[2],ops[1],d[1]?"PIXELS DIFFER":"ok",t[3],ops[2],d[2]?"PIXELS DIFFER":"ok");
  for(c=0;c<3;c++) if(d[c]) logline("     %s: %d differing pixels\n",cfg[c].name,d[c]);
 }
 ctl(0xFFFFFFFFUL,NULL); return 1;
}
static void shapes_check(int *fails) {
 POINT star[5]={{150,30},{230,260},{40,110},{260,110},{70,260}}; HBRUSH s=CreateSolidBrush(RGB(120,200,40)),o; HPEN p; double t[2]; int w,i,d; DWORD x0[4],x1[4];
 for(w=0;w<2;w++) {
  double st; ctl(0xF2000000UL|(w?2UL:0UL),NULL);
  if(w) {ctl(2,NULL); background(); p=(HPEN)SelectObject(sdc,GetStockObject(NULL_PEN)); SetROP2(sdc,R2_XORPEN); o=(HBRUSH)SelectObject(sdc,s); Polygon(sdc,star,5); Ellipse(sdc,300,30,580,220); RoundRect(sdc,30,280,280,420,40,40); SelectObject(sdc,o); SelectObject(sdc,p); shot(); memcpy(bufA,tbits,AW*AH*4); ctl(0xFFFFFFFFUL,NULL); ctl(0xF2000002UL,NULL);}
  background(); ctl(0xFFFFFE18UL,x0); p=(HPEN)SelectObject(sdc,GetStockObject(NULL_PEN)); SetROP2(sdc,R2_XORPEN); o=(HBRUSH)SelectObject(sdc,s);
  st=now(); for(i=0;i<6&&pump();i++) {Polygon(sdc,star,5); Ellipse(sdc,300,30,580,220); RoundRect(sdc,30,280,280,420,40,40);} GdiFlush(); t[w]=(now()-st)/6;
  SelectObject(sdc,o); SelectObject(sdc,p); SetROP2(sdc,R2_COPYPEN); ctl(0xFFFFFE18UL,x1);
  if(w) {background(); p=(HPEN)SelectObject(sdc,GetStockObject(NULL_PEN)); SetROP2(sdc,R2_XORPEN); o=(HBRUSH)SelectObject(sdc,s); Polygon(sdc,star,5); Ellipse(sdc,300,30,580,220); RoundRect(sdc,30,280,280,420,40,40); SelectObject(sdc,o); SelectObject(sdc,p); SetROP2(sdc,R2_COPYPEN); shot(); d=compare(); if(d) (*fails)++;
   logline("  XOR shapes: ACCEL_SPAN_WAIT 0 %.2f ms, 2 %.2f ms; %lu bands; pixels with 2 against DIBENG: %s (%d)\n",t[0],t[1],x1[3]-x0[3],d?"DIFFER":"ok",d);}
 }
 ctl(0xFFFFFFFFUL,NULL); DeleteObject(s);
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 static const int depths[3]={8,16,32}; DWORD c[4]; int ph,fails=0,have; DEVMODEA origm; char res[300]; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2OPT2.TXT","w"); logline("VX2OPT2 v1.0 / driver v10.133\n"); finish("RUNNING: the run did not reach its end (VX2OPT2.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 ctl(0xFFFFFFFFUL,NULL); if(!ctl(0xF0000001UL,c)||c[0]!=1) {finish("FAIL: VX85598.VXD older than v10.133"); return 1;}
 ctl(0xF2000000UL,c); logline("registry values: span wait reply %lu (the value just set); defaults are ACCEL_OP_WAIT 1, ACCEL_DREAD_ANY 1, ACCEL_SPAN_WAIT 2\n",c[0]); ctl(0xFFFFFFFFUL,NULL);
 bufA=(DWORD*)calloc(AW*AH,4); if(!bufA) {finish("FAIL: out of memory"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 for(ph=0;ph<3&&!aborted;ph++) {
  int bpp=depths[ph];
  if(!set_mode(640,480,bpp)) {logline("640x480x%d not available: skipped\n",bpp); continue;}
  sdc=GetDC(NULL); if(!sdc||!make_target()) {logline("no screen DC / target\n"); continue;}
  bsolid=CreateSolidBrush(RGB(90,30,200)); bhatch=CreateHatchBrush(HS_DIAGCROSS,RGB(250,250,0));
  logline("640x480x%d: microseconds per call; engine = how many of 40 calls went to the engine\n",bpp); run_depth(&fails);
  shapes_check(&fails);
  DeleteObject(bsolid); DeleteObject(bhatch); drop_target(); ReleaseDC(NULL,sdc);
 }
 ctl(0xFFFFFFFFUL,NULL);
 if(have) {ChangeDisplaySettingsA(&origm,0); logline("desktop put back\n");}
 sprintf(res,"%s: %d cases with differing pixels (see VX2OPT2.TXT for the times)",aborted?"FAIL (aborted)":fails?"FAIL":"PASS",fails);
 finish(res); logline("%s\nFinished.\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
