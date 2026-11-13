/* VX2OPT v1.0: the three optimisations of driver v10.132, each against what was there before. Unattended; results in VX2OPT.TXT (VX2OPT.RES says RUNNING until it ends). Esc stops.
 * A. ACCEL_SPAN_WAIT (live 0xF2000000|v): the wait between the rectangles of one OS_POLYSCANLINE batch. 0 = the full idle after each (before), 3 = four idle reads, 2 = one read,
 *    1 = none. Tried in that order (safest first) at 640x480x16 and x32 on XOR shapes (bands since v10.118) and COPYPEN shapes (ACCEL_HOST bit 32, live mask 62): pixels against
 *    DIBENG (live mask 14: no bands), the band counter, times. A variant that loses the engine is logged and the rest of that depth skips it.
 * B. ACCEL_STR_FUSE (live 0xF3000000|v): 1-bit DIBs through StretchDIBits into 640x480x32 at 1:1, 2x and 3x1, the 1-bit classes forced on: fused, unfused and DIBENG, pixels and times.
 * C. The phase profile (live 0xF4000001 on / 0xF4000000 off, off by default now): 3000 PatBlt fills of 96x96, times with the profile off and on.
 * Nothing is written to the registry; every live value is put back at the end. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2OPT.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1; return !aborted;}
static double now(void) {LARGE_INTEGER c,f; if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;} return (double)GetTickCount();}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); {DWORD t=GetTickCount(); while(GetTickCount()-t<1000) {pump(); Sleep(20);}}
 return rc==DISP_CHANGE_SUCCESSFUL;
}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static void setmask(DWORD m) {ctl(0xF7000000UL|(m&63UL),NULL);}

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

/* ---- A: shapes ---- */
typedef void (*drawfn)(void);
static void shapes(int rop2) {
 POINT star[5]={{150,30},{230,260},{40,110},{260,110},{70,260}}; HBRUSH s=CreateSolidBrush(RGB(120,200,40)),h=CreateHatchBrush(HS_DIAGCROSS,RGB(250,0,250)),o; HPEN p=(HPEN)SelectObject(sdc,GetStockObject(NULL_PEN)),w;
 SetROP2(sdc,rop2); SetPolyFillMode(sdc,WINDING); SetBkColor(sdc,RGB(0,0,0)); o=(HBRUSH)SelectObject(sdc,s);
 Polygon(sdc,star,5); Ellipse(sdc,300,30,580,220); RoundRect(sdc,30,280,280,420,40,40); Pie(sdc,320,240,560,430,560,330,320,330);
 SelectObject(sdc,h); Ellipse(sdc,200,150,420,330);
 w=CreatePen(PS_SOLID,9,RGB(30,90,250)); SelectObject(sdc,w); SelectObject(sdc,GetStockObject(NULL_BRUSH)); Ellipse(sdc,60,60,540,380); SelectObject(sdc,GetStockObject(NULL_PEN)); DeleteObject(w);
 SetROP2(sdc,R2_COPYPEN); SelectObject(sdc,o); SelectObject(sdc,p); DeleteObject(s); DeleteObject(h);
}
static void c_xor(void) {shapes(R2_XORPEN);}
static void c_copy(void) {shapes(R2_COPYPEN);}
static const struct {const char *name; drawfn fn;} acase[2]={{"XOR shapes (bands since v10.118)",c_xor},{"COPYPEN shapes (ACCEL_HOST bit 32)",c_copy}};
#define M_DIB 14UL
#define M_BANDS 62UL
static int part_a(int bpp,int *fails) {
 static const DWORD order[4]={0,3,2,1}; static const char *vname[4]={"0 full idle (before)","1 no wait","2 one idle read","3 four idle reads"};
 int vi,ci,dead=0,n=6; double tdib[2];
 for(ci=0;ci<2;ci++) {double s; int i; background(); setmask(M_DIB); s=now(); for(i=0;i<n&&pump();i++) acase[ci].fn(); GdiFlush(); tdib[ci]=(now()-s)/n;}
 logline("  DIBENG: %s %.2f ms, %s %.2f ms\n",acase[0].name,tdib[0],acase[1].name,tdib[1]);
 for(vi=0;vi<4&&pump();vi++) {
  DWORD v=order[vi];
  for(ci=0;ci<2&&!dead&&pump();ci++) {
   DWORD c0[4],c1[4],k[4],w[4]; int d,i; double s,te;
   background(); setmask(M_DIB); acase[ci].fn(); shot(); memcpy(bufA,tbits,AW*AH*4);
   ctl(0xF2000000UL|v,NULL); background(); ctl(0xFFFFFE18UL,c0); setmask(M_BANDS); acase[ci].fn(); GdiFlush(); ctl(0xFFFFFE18UL,c1); shot(); d=compare();
   ctl(0xF2000000UL|v,w); ctl(0xFFFFFE19UL,k);
   background(); s=now(); for(i=0;i<n&&pump();i++) acase[ci].fn(); GdiFlush(); te=(now()-s)/n; setmask(M_DIB);
   logline("  wait %-22s %-36s %s: %d differing pixels, %lu bands on the engine; %.2f ms (DIBENG %.2f ms, %s); handed back %lu, last reason %lu\n",vname[v],acase[ci].name,d?"FAIL":"ok",d,c1[3]-c0[3],te,tdib[ci],te<tdib[ci]?"faster":"slower",k[0],w[3]);
   if(d) (*fails)++;
   if(c1[3]==c0[3]) {logline("     the engine drew no band: lost or switched off? the remaining variants at this depth are skipped\n"); dead=1;}
  }
 }
 ctl(0xF2000000UL,NULL); setmask(30); return !dead;
}

/* ---- B: 1-bit DIBs ---- */
static int part_b(int *fails) {
 static struct {BITMAPINFOHEADER h; RGBQUAD c[2];} bi; BYTE *bits; int W=301,H=211,stride,i,k,m; double t[3]; DWORD f0[4],f1[4];
 static const struct {int dw,dh; const char *name;} sz[3]={{301,211,"1:1"},{602,422,"2x"},{903,211,"3x1"}};
 stride=((W+31)/32)*4; bits=(BYTE*)malloc(stride*H); if(!bits) return 0;
 for(i=0;i<stride*H;i++) bits[i]=(BYTE)((i*37+(i>>7)*11)^(i>>3));
 memset(&bi,0,sizeof(bi)); bi.h.biSize=sizeof(bi.h); bi.h.biWidth=W; bi.h.biHeight=H; bi.h.biPlanes=1; bi.h.biBitCount=1; bi.h.biClrUsed=2;
 bi.c[0].rgbRed=200; bi.c[0].rgbGreen=40; bi.c[0].rgbBlue=30; bi.c[1].rgbRed=20; bi.c[1].rgbGreen=180; bi.c[1].rgbBlue=250;
 for(k=0;k<3&&pump();k++) {
  int dw=sz[k].dw>AW?AW:sz[k].dw,dh=sz[k].dh>AH?AH:sz[k].dh,sw=dw*W/sz[k].dw,sh=dh*H/sz[k].dh,d1,d2;
  /* DIBENG reference */
  ctl(0xFA000000UL,NULL); background(); StretchDIBits(sdc,0,0,dw,dh,0,H-sh,sw,sh,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY); shot(); memcpy(bufA,tbits,AW*AH*4);
  ctl(0xFA000007UL,NULL); ctl(0xF3000001UL,f0); background(); StretchDIBits(sdc,0,0,dw,dh,0,H-sh,sw,sh,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY); shot(); d1=compare(); ctl(0xF3000001UL,f1);
  ctl(0xF3000000UL,NULL); background(); StretchDIBits(sdc,0,0,dw,dh,0,H-sh,sw,sh,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY); shot(); d2=compare();
  for(m=0;m<3;m++) {
   double s; int r,n=20;
   ctl(m==0?0xFA000000UL:0xFA000007UL,NULL); ctl(m==2?0xF3000000UL:0xF3000001UL,NULL); background();
   s=now(); for(r=0;r<n&&pump();r++) StretchDIBits(sdc,0,0,dw,dh,0,H-sh,sw,sh,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY); GdiFlush(); t[m]=(now()-s)/n;
  }
  logline("  1-bit %3dx%3d -> %3dx%3d (%s): fused %s (%d differing pixels, %lu fused rows), unfused %s (%d); DIBENG %.2f ms, fused %.2f ms, unfused %.2f ms\n",sw,sh,dw,dh,sz[k].name,d1?"FAIL":"ok",d1,f1[1]-f0[1],d2?"FAIL":"ok",d2,t[0],t[1],t[2]);
  if(d1) (*fails)++; if(d2) (*fails)++;
 }
 ctl(0xF3000001UL,NULL); free(bits); return 1;
}

/* ---- C: the phase profile ---- */
static void part_c(void) {
 double t[2]; int m,i,n=3000; HBRUSH b=CreateSolidBrush(RGB(10,200,90)),o=(HBRUSH)SelectObject(sdc,b); DWORD p[4];
 for(m=0;m<2;m++) {double s; ctl(m?0xF4000001UL:0xF4000000UL,NULL); s=now(); for(i=0;i<n&&pump();i++) PatBlt(sdc,(i*13)%500,(i*7)%340,96,96,(i&1)?PATCOPY:PATINVERT); GdiFlush(); t[m]=(now()-s)*1000.0/n; if(m) ctl(0xFFFFFE00UL,p);}
 ctl(0xF4000000UL,NULL); SelectObject(sdc,o); DeleteObject(b);
 logline("  3000 PatBlt 96x96: profile off %.1f us each, on %.1f us each (%lu operations timed while on)\n",t[0],t[1],p[0]);
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 static const int depths[2]={16,32}; DWORD c[4]; int ph,fails=0,have; DEVMODEA origm; char res[300]; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2OPT.TXT","w"); logline("VX2OPT v1.0 / driver v10.132\n"); finish("RUNNING: the run did not reach its end (VX2OPT.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xF3000001UL,c)||c[0]!=1) {finish("FAIL: VX85598.VXD older than v10.132"); return 1;}
 bufA=(DWORD*)calloc(AW*AH,4); if(!bufA) {finish("FAIL: out of memory"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 for(ph=0;ph<2&&!aborted;ph++) {
  int bpp=depths[ph];
  if(!set_mode(640,480,bpp)) {logline("640x480x%d not available: skipped\n",bpp); continue;}
  sdc=GetDC(NULL); if(!sdc||!make_target()) {logline("no screen DC / target\n"); continue;}
  logline("A. 640x480x%d: wait between band rectangles\n",bpp); ctl(5,NULL); part_a(bpp,&fails); ctl(0xFFFFFFFFUL,NULL);
  if(bpp==32) {logline("B. 640x480x32: 1-bit DIBs, fused expansion\n"); part_b(&fails); ctl(0xFFFFFFFFUL,NULL);}
  logline("C. 640x480x%d: phase profile\n",bpp); part_c();
  drop_target(); ReleaseDC(NULL,sdc);
 }
 ctl(0xFFFFFFFFUL,NULL);
 if(have) {ChangeDisplaySettingsA(&origm,0); logline("desktop put back\n");}
 sprintf(res,"%s: %d cases with differing pixels (see VX2OPT.TXT for the times)",aborted?"FAIL (aborted)":fails?"FAIL":"PASS",fails);
 finish(res); logline("%s\nFinished.\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
