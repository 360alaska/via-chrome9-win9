/* VX2STR v1.0: stretching and bitmap uploads, measured before any hook is written (the driver is not changed; any v10.92 driver will do).
 *   T1  the pixel mapping DIBENG uses when it stretches (COLORONCOLOR): a source of up to 15 distinct colours is stretched horizontally and vertically by 1:1, 2x, 3x, 4x, 1/2, 3:2, 1.5x, 8:5, 2.29x, 1.6x, 15:11
 *       (and with a source and destination offset) through StretchDIBits to the screen (24-bit source), StretchBlt memory bitmap to screen and memory bitmap to memory bitmap; for every destination pixel the source pixel it
 *       took is read back and matched against five candidate rules (floor(i*sw/dw), the pixel-centre rule, round(i*(sw-1)/(dw-1)), ceil, floor+round). The raw map is printed for anything that does not match.
 *   T2  timing (640x480 destination, ms per frame): SetDIBitsToDevice and StretchDIBits 1:1, StretchDIBits 2x and 1/2 (same depth and 24-bit source), StretchBlt memory to screen 2x, memory to memory 2x (CPU only),
 *       and the cost of a plain C nearest-pixel 2x stretch into a buffer (and a 24-bit to 32 / 16 bit conversion) plus the SetDIBitsToDevice upload: what a driver fast path could reach.
 * Depths 32, 16 and 8 bits per pixel at 640x480 (the desktop is switched and put back). Unattended: run it, read VX2STR.TXT (VX2STR.RES says RUNNING until it ends). Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static int aborted,bpp; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {
 va_list a;
 if(!lf) return;
 va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));
}
static void finish(const char *res) {FILE *f=fopen("VX2STR.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
 return !aborted;
}
static double now(void) {
 LARGE_INTEGER c,f;
 if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;}
 return (double)GetTickCount();
}

/* ---- T1 ---- */
static const BYTE pal[15][3]={{128,0,0},{0,128,0},{128,128,0},{0,0,128},{128,0,128},{0,128,128},{192,192,192},{128,128,128},{255,0,0},{0,255,0},{255,255,0},{0,0,255},{255,0,255},{0,255,255},{255,255,255}};
static int decode(COLORREF c) { /* 0 = background (black), 1..15 = palette index + 1, -1 = other */
 int r=GetRValue(c),g=GetGValue(c),b=GetBValue(c),i,best=-1,bd=1<<30;
 if(r<24&&g<24&&b<24) return 0;
 for(i=0;i<15;i++) {int dr=r-pal[i][0],dg=g-pal[i][1],db=b-pal[i][2],d=dr*dr+dg*dg+db*db; if(d<bd) {bd=d; best=i;}}
 return bd<2500?best+1:-1;
}
static BITMAPINFO *tbi; static BYTE *tbits; /* 24-bit source, 16 x 16 */
static HDC sdc,mdcA,mdcB; static HBITMAP bmA,bmB,oldA,oldB;
#define MW 80
#define MH 80
static void src_pixels(int x0,int y0,int sw,int sh,int vertical) { /* the source rectangle (x0,y0,sw,sh) of a 32x32 top-down picture, stored as a bottom-up 24-bit DIB; colour index follows x (horizontal test) or y (vertical test) from the rectangle's origin */
 int x,y;
 memset(tbits,0,32UL*32UL*3UL);
 for(y=y0;y<y0+sh;y++) for(x=x0;x<x0+sw;x++) {
  int idx=vertical?(y-y0):(x-x0); BYTE *d=tbits+((31-y)*32+x)*3;
  if(idx<15) {d[0]=pal[idx][2]; d[1]=pal[idx][1]; d[2]=pal[idx][0];}
 }
}
static void fill_mem(HDC dc,int x0,int y0,int sw,int sh,int vertical) {
 int x,y; RECT rc; HBRUSH hb;
 rc.left=0; rc.top=0; rc.right=32; rc.bottom=32; hb=CreateSolidBrush(RGB(0,0,0)); FillRect(dc,&rc,hb); DeleteObject(hb);
 for(y=y0;y<y0+sh;y++) for(x=x0;x<x0+sw;x++) {
  int idx=vertical?(y-y0):(x-x0);
  if(idx<15) {hb=CreateSolidBrush(RGB(pal[idx][0],pal[idx][1],pal[idx][2])); rc.left=x; rc.top=y; rc.right=x+1; rc.bottom=y+1; FillRect(dc,&rc,hb); DeleteObject(hb);}
 }
}
static int candidate(int k,int i,int n,int dn) { /* source index for destination index i of dn, source length n */
 switch(k) {
  case 0: return (int)(((long)i*n)/dn);
  case 1: return (int)(((long)(2*i+1)*n)/(2L*dn));
  case 2: return dn>1?(int)(((long)i*(n-1)*2+(dn-1))/(2L*(dn-1))):0;
  case 3: return (int)(((long)i*n+dn-1)/dn);
  default: return (int)(((long)i*n*2+dn)/(2L*dn)); /* round(i*n/dn) */
 }
}
static const char *cname[5]={"floor(i*sw/dw)","centre floor((2i+1)*sw/2dw)","round(i*(sw-1)/(dw-1))","ceil(i*sw/dw)","round(i*sw/dw)"};
/* path 0: StretchDIBits to the screen, 1: StretchBlt memory to screen, 2: StretchBlt memory to memory. off = 0, or 3 = source offset along the stretched axis with destination offset 5 */
static void map_test(int path,int vertical,int n,int dn,int off) {
 static const char *pn[3]={"StretchDIBits -> screen","StretchBlt mem -> screen","StretchBlt mem -> mem"};
 HDC dst=path==2?mdcB:sdc; int i,obs[48],bad=0,over=0,k,m[5]={1,1,1,1,1};
 int ox=path==2?0:100,oy=path==2?0:100;
 int sw=vertical?4:n,sh=vertical?n:4,sx=vertical?0:off,sy=vertical?off:0;
 int dw=vertical?8:dn,dh=vertical?dn:8,dxp=ox+(vertical?20:4+(off?5:0)),dyp=oy+(vertical?4+(off?5:0):20);
 {RECT rc; HBRUSH hb=CreateSolidBrush(RGB(0,0,0)); rc.left=ox; rc.top=oy; rc.right=ox+MW; rc.bottom=oy+MH; FillRect(dst,&rc,hb); DeleteObject(hb);}
 SetStretchBltMode(dst,COLORONCOLOR);
 if(path==0) {
  src_pixels(sx,sy,sw,sh,vertical);
  StretchDIBits(dst,dxp,dyp,dw,dh,sx,32-(sy+sh),sw,sh,tbits,tbi,DIB_RGB_COLORS,SRCCOPY);
 } else {
  fill_mem(mdcA,sx,sy,sw,sh,vertical);
  StretchBlt(dst,dxp,dyp,dw,dh,mdcA,sx,sy,sw,sh,SRCCOPY);
 }
 /* obs[0..1] and obs[dn+2..dn+3] are the pixels either side of the destination: background if nothing overran */
 for(i=0;i<dn+4;i++) {
  int px=vertical?dxp+2:dxp-2+i,py=vertical?dyp-2+i:dyp+2;
  obs[i]=decode(GetPixel(dst,px,py));
 }
 if(obs[0]!=0||obs[1]!=0||obs[dn+2]!=0||obs[dn+3]!=0) over=1;
 for(i=0;i<dn;i++) {
  if(obs[i+2]<=0) bad=1;
  for(k=0;k<5;k++) if(obs[i+2]-1!=candidate(k,i,n,dn)) m[k]=0;
 }
 logline("    %-24s %s %2d -> %2d%s: %s",pn[path],vertical?"vertical  ":"horizontal",n,dn,off?" (offsets)":"",over?"OVERRUN ":"");
 if(bad||over||!(m[0]||m[1]||m[2]||m[3]||m[4])) {
  logline("%smap:",bad?"UNREADABLE ":"");
  for(i=0;i<dn+4;i++) logline(" %d",(i>=2&&i<dn+2)?obs[i]-1:obs[i]);
  logline("   (floor rule:"); for(i=0;i<dn;i++) logline(" %d",candidate(0,i,n,dn)); logline(")\n");
 } else {
  int first=1; for(k=0;k<5;k++) if(m[k]) {logline("%s%s",first?"matches ":" and ",cname[k]); first=0;}
  logline("\n");
 }
}
static void mapping(void) {
 static const int ns[11]={8,8,8,8,15,12,8,5,7,10,15},dns[11]={8,16,24,32,8,8,12,8,16,16,11};
 int i,p,v;
 logline("-- T1 stretch mapping, %d bits per pixel (COLORONCOLOR; n -> dn = source pixels -> destination pixels) --\n",bpp);
 for(p=0;p<3&&pump();p++) for(v=0;v<2;v++) {
  for(i=0;i<11&&pump();i++) map_test(p,v,ns[i],dns[i],0);
  map_test(p,v,8,16,3); map_test(p,v,7,16,3);
 }
}

/* ---- T2 ---- */
static BYTE *ubits,*ubuf; static BITMAPINFO *ubi;
static void mkinfo(int bits,int w,int h) {
 memset(ubi,0,sizeof(BITMAPINFOHEADER)+256*4); ubi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER); ubi->bmiHeader.biWidth=w; ubi->bmiHeader.biHeight=h; ubi->bmiHeader.biPlanes=1;
 ubi->bmiHeader.biBitCount=(WORD)bits; ubi->bmiHeader.biCompression=BI_RGB;
 if(bits==8) {int i; ubi->bmiHeader.biClrUsed=256; for(i=0;i<256;i++) {ubi->bmiColors[i].rgbRed=(BYTE)i; ubi->bmiColors[i].rgbGreen=(BYTE)i; ubi->bmiColors[i].rgbBlue=(BYTE)i;}}
}
static void ref2x(const BYTE *s,BYTE *d,int w,int h,int bytes) { /* plain C nearest-pixel 2x: every source pixel becomes a 2x2 block */
 int x,y;
 for(y=0;y<h;y++) {
  if(bytes==4) {const DWORD *sp=(const DWORD*)(s+(long)y*w*4); DWORD *d0=(DWORD*)(d+(long)(2*y)*w*8),*d1=(DWORD*)(d+(long)(2*y+1)*w*8); for(x=0;x<w;x++) {DWORD v=sp[x]; d0[2*x]=v; d0[2*x+1]=v; d1[2*x]=v; d1[2*x+1]=v;}}
  else if(bytes==2) {const WORD *sp=(const WORD*)(s+(long)y*w*2); WORD *d0=(WORD*)(d+(long)(2*y)*w*4),*d1=(WORD*)(d+(long)(2*y+1)*w*4); for(x=0;x<w;x++) {WORD v=sp[x]; d0[2*x]=v; d0[2*x+1]=v; d1[2*x]=v; d1[2*x+1]=v;}}
  else {const BYTE *sp=s+(long)y*w; BYTE *d0=d+(long)(2*y)*w*2,*d1=d+(long)(2*y+1)*w*2; for(x=0;x<w;x++) {BYTE v=sp[x]; d0[2*x]=v; d0[2*x+1]=v; d1[2*x]=v; d1[2*x+1]=v;}}
 }
}
static void conv24(const BYTE *s,BYTE *d,int n,int to16) { /* 24-bit BGR to 32-bit XRGB or 16-bit 555 */
 int i;
 if(!to16) {DWORD *dp=(DWORD*)d; for(i=0;i<n;i++) dp[i]=(DWORD)s[i*3]|((DWORD)s[i*3+1]<<8)|((DWORD)s[i*3+2]<<16);}
 else {WORD *dp=(WORD*)d; for(i=0;i<n;i++) dp[i]=(WORD)((s[i*3]>>3)|((s[i*3+1]>>3)<<5)|((s[i*3+2]>>3)<<10));}
}
typedef enum {Q_SDI,Q_SDI24,Q_S1,Q_S2,Q_S2_24,Q_SH,Q_SH24,Q_B2S,Q_M2M,Q_R2,Q_R2UP,Q_C24,Q_C24UP} qtest;
static int qbytes;
static void doone(qtest t) {
 switch(t) {
  case Q_SDI: mkinfo(bpp,640,480); SetDIBitsToDevice(sdc,0,0,640,480,0,0,0,480,ubits,ubi,DIB_RGB_COLORS); break;
  case Q_SDI24: mkinfo(24,640,480); SetDIBitsToDevice(sdc,0,0,640,480,0,0,0,480,ubits,ubi,DIB_RGB_COLORS); break;
  case Q_S1: mkinfo(bpp,640,480); StretchDIBits(sdc,0,0,640,480,0,0,640,480,ubits,ubi,DIB_RGB_COLORS,SRCCOPY); break;
  case Q_S2: mkinfo(bpp,320,240); StretchDIBits(sdc,0,0,640,480,0,0,320,240,ubits,ubi,DIB_RGB_COLORS,SRCCOPY); break;
  case Q_S2_24: mkinfo(24,320,240); StretchDIBits(sdc,0,0,640,480,0,0,320,240,ubits,ubi,DIB_RGB_COLORS,SRCCOPY); break;
  case Q_SH: mkinfo(bpp,640,480); StretchDIBits(sdc,0,0,320,240,0,0,640,480,ubits,ubi,DIB_RGB_COLORS,SRCCOPY); break;
  case Q_SH24: mkinfo(24,640,480); StretchDIBits(sdc,0,0,320,240,0,0,640,480,ubits,ubi,DIB_RGB_COLORS,SRCCOPY); break;
  case Q_B2S: StretchBlt(sdc,0,0,640,480,mdcA,0,0,320,240,SRCCOPY); break;
  case Q_M2M: StretchBlt(mdcB,0,0,640,480,mdcA,0,0,320,240,SRCCOPY); break;
  case Q_R2: ref2x(ubits,ubuf,320,240,qbytes); break;
  case Q_R2UP: ref2x(ubits,ubuf,320,240,qbytes); mkinfo(bpp,640,480); SetDIBitsToDevice(sdc,0,0,640,480,0,0,0,480,ubuf,ubi,DIB_RGB_COLORS); break;
  case Q_C24: conv24(ubits,ubuf,640*480,bpp==16); break;
  case Q_C24UP: conv24(ubits,ubuf,640*480,bpp==16); mkinfo(bpp,640,480); SetDIBitsToDevice(sdc,0,0,640,480,0,0,0,480,ubuf,ubi,DIB_RGB_COLORS); break;
 }
}
static void timeit(qtest t,const char *name) {
 double t0,t1; int n=0;
 doone(t);
 t0=now(); do {doone(t); n++; t1=now();} while(t1-t0<400.0&&n<2000&&pump());
 logline("    %-60s %8.2f ms/frame\n",name,(t1-t0)/(double)n);
}
static void timing(void) {
 char nm[96]; int c24ok=(bpp!=8);
 logline("-- T2 timing, %d bits per pixel (640x480 destination) --\n",bpp);
 qbytes=bpp/8;
 sprintf(nm,"SetDIBitsToDevice 640x480, %d-bit (reference)",bpp); timeit(Q_SDI,nm);
 timeit(Q_SDI24,"SetDIBitsToDevice 640x480, 24-bit source");
 sprintf(nm,"StretchDIBits 1:1, %d-bit",bpp); timeit(Q_S1,nm);
 sprintf(nm,"StretchDIBits 2x (320x240 -> 640x480), %d-bit",bpp); timeit(Q_S2,nm);
 timeit(Q_S2_24,"StretchDIBits 2x, 24-bit source");
 sprintf(nm,"StretchDIBits 1/2 (640x480 -> 320x240), %d-bit",bpp); timeit(Q_SH,nm);
 timeit(Q_SH24,"StretchDIBits 1/2, 24-bit source");
 timeit(Q_B2S,"StretchBlt memory bitmap 2x -> screen");
 timeit(Q_M2M,"StretchBlt memory bitmap 2x -> memory bitmap (CPU only)");
 sprintf(nm,"reference: plain C 2x stretch into a buffer, %d-bit (no upload)",bpp); timeit(Q_R2,nm);
 sprintf(nm,"reference: plain C 2x stretch + SetDIBitsToDevice, %d-bit",bpp); timeit(Q_R2UP,nm);
 if(c24ok) {
  timeit(Q_C24,"reference: plain C 24-bit -> screen format conversion, 640x480 (no upload)");
  timeit(Q_C24UP,"reference: plain C conversion + SetDIBitsToDevice");
 }
}

/* ---- main ---- */
static DEVMODEA orig;
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc;
 memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc);
 return rc==DISP_CHANGE_SUCCESSFUL;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 static const int ds[3]={32,16,8};
 vx_status_t s; DWORD got=0,t; int k,rc=0,have_orig; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2STR.TXT","w"); logline("VX2STR v1.0\n");
 finish("RUNNING: the run did not reach its end (VX2STR.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 ubits=(BYTE*)calloc(640UL*480UL*4UL,1); ubuf=(BYTE*)calloc(640UL*480UL*4UL,1); ubi=(BITMAPINFO*)calloc(sizeof(BITMAPINFOHEADER)+256*4,1);
 tbi=(BITMAPINFO*)calloc(sizeof(BITMAPINFOHEADER)+256*4,1); tbits=(BYTE*)calloc(32UL*32UL*3UL,1);
 if(!ubits||!ubuf||!ubi||!tbi||!tbits) {finish("FAIL: out of memory"); return 1;}
 {unsigned long i; for(i=0;i<640UL*480UL*4UL;i++) ubits[i]=(BYTE)(i*7+(i>>9));}
 tbi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER); tbi->bmiHeader.biWidth=32; tbi->bmiHeader.biHeight=32; tbi->bmiHeader.biPlanes=1; tbi->bmiHeader.biBitCount=24; tbi->bmiHeader.biCompression=BI_RGB;
 memset(&orig,0,sizeof(orig)); orig.dmSize=sizeof(orig); have_orig=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&orig)?1:0;
 logline("desktop was %lux%lux%lu\n",have_orig?orig.dmPelsWidth:0,have_orig?orig.dmPelsHeight:0,have_orig?orig.dmBitsPerPel:0);
 for(k=0;k<3&&!rc&&!aborted;k++) {
  bpp=ds[k];
  if(!set_mode(640,480,(DWORD)bpp)) {logline("could not set 640x480x%d\n",bpp); finish("FAIL: mode not available"); rc=1; break;}
  t=GetTickCount(); while(GetTickCount()-t<800) Sleep(20);
  memset(&s,0,sizeof(s));
  if(!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||s.width!=640||s.height!=480||s.bpp!=(DWORD)bpp||s.stage!=6||s.failure||s.vga_mode) {
   logline("the driver is not in native 640x480x%d mode\n",bpp); finish("FAIL: not native mode"); rc=1; break;
  }
  sdc=GetDC(NULL); if(!sdc) {finish("FAIL: no screen DC"); rc=1; break;}
  mdcA=CreateCompatibleDC(sdc); bmA=CreateCompatibleBitmap(sdc,640,480); oldA=(HBITMAP)SelectObject(mdcA,bmA);
  mdcB=CreateCompatibleDC(sdc); bmB=CreateCompatibleBitmap(sdc,640,480); oldB=(HBITMAP)SelectObject(mdcB,bmB);
  PatBlt(mdcA,0,0,640,480,WHITENESS);
  mapping();
  if(pump()) timing();
  SelectObject(mdcB,oldB); DeleteObject(bmB); DeleteDC(mdcB); SelectObject(mdcA,oldA); DeleteObject(bmA); DeleteDC(mdcA);
  ReleaseDC(NULL,sdc);
 }
 if(aborted) {finish("ABORTED"); logline("aborted\n");}
 else if(!rc) finish("DONE: see VX2STR.TXT");
 if(have_orig) {ChangeDisplaySettingsA(&orig,0); logline("desktop put back\n");}
 logline("Finished.\n"); if(lf) fclose(lf);
 return rc;
}
