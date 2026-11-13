/* VX2SDIB v1.0: what DIBENG does with a 16-bit screen when StretchDIBits stretches a bitmap, measured before any hook is written (the driver is not changed; any v10.92 or later driver will do).
 * The desktop is switched to 640x480x16 (and put back). Everything is read back by copying the screen into a 565 DIB section.
 *   T1  colour conversion into the 565 screen: every one of the 32768 values of a 16-bit BI_RGB (5-5-5) source through StretchDIBits 1:1, StretchDIBits 2x and SetDIBitsToDevice (red and blue must keep their 5 bits;
 *       the 6th green bit is checked against: 0, replicated top bit, 1); a 16-bit BI_BITFIELDS 565 source (identity expected); 4096 colours of a 32-bit BI_RGB and a 24-bit source (truncation expected, rounding is the alternative).
 *   T2  geometry, with a 565 BI_BITFIELDS source of 21x13 pixels (an odd width, so every row has padding) and whole-number enlargement 2x or 3x: bottom-up and top-down bitmaps, whole source or a source rectangle,
 *       an odd destination position, a clip region, mirrored destinations; each result compared pixel for pixel with a model (source rows counted from the bottom for a bottom-up bitmap, from the top for a top-down one).
 *   T3  timing of the 16-bit cases that were slow in VX2STR: StretchDIBits 1:1, 2x and 1/2 into the 640x480 screen from a 555 source, a 565 BITFIELDS source, a 32-bit source and a 24-bit source.
 * Unattended: run it, read VX2SDIB.TXT (VX2SDIB.RES says RUNNING until it ends). Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {
 va_list a;
 if(!lf) return;
 va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));
}
static void finish(const char *res) {FILE *f=fopen("VX2SDIB.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
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
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc;
 memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc);
 return rc==DISP_CHANGE_SUCCESSFUL;
}

static HDC sdc,mdc; static HBITMAP dib,olddib; static WORD *tgt; /* 640x480 565 DIB section, top-down */
typedef struct {BITMAPINFOHEADER h; DWORD m[3];} bi565;
static void mkbi565(bi565 *b,int w,int h) {
 memset(b,0,sizeof(*b)); b->h.biSize=sizeof(BITMAPINFOHEADER); b->h.biWidth=w; b->h.biHeight=h; b->h.biPlanes=1; b->h.biBitCount=16; b->h.biCompression=BI_BITFIELDS;
 b->m[0]=0xF800; b->m[1]=0x07E0; b->m[2]=0x001F;
}
static int make_target(void) {
 bi565 b; void *bits=NULL;
 mkbi565(&b,640,-480);
 dib=CreateDIBSection(sdc,(BITMAPINFO*)&b,DIB_RGB_COLORS,&bits,NULL,0);
 if(!dib||!bits) return 0;
 mdc=CreateCompatibleDC(sdc); olddib=(HBITMAP)SelectObject(mdc,dib); tgt=(WORD*)bits;
 return 1;
}
static void shot(int x,int y,int w,int h) { /* the screen rectangle appears at the top left of tgt */
 BitBlt(mdc,0,0,w,h,sdc,x,y,SRCCOPY); GdiFlush();
}
static void clear_screen(void) {
 RECT rc; HBRUSH hb=CreateSolidBrush(RGB(0,0,0));
 rc.left=0; rc.top=0; rc.right=640; rc.bottom=480; FillRect(sdc,&rc,hb); DeleteObject(hb); GdiFlush();
}

/* ---- T1 ---- */
static BYTE *sbuf;
static void conv555(const char *what,int mode) { /* mode 0 StretchDIBits 1:1, 1 StretchDIBits 2x, 2 SetDIBitsToDevice, 3 StretchDIBits 1:1 into the memory DIB section */
 BITMAPINFOHEADER bh; int x,y,sc=(mode==1)?2:1; long cr=0,cb=0,ca=0,cbb=0,cc=0,other=0,total=0,ex=0; int mx=0,my=0; WORD gv=0;
 memset(&bh,0,sizeof(bh)); bh.biSize=sizeof(bh); bh.biWidth=256; bh.biHeight=-128; bh.biPlanes=1; bh.biBitCount=16; bh.biCompression=BI_RGB;
 for(y=0;y<128;y++) for(x=0;x<256;x++) ((WORD*)sbuf)[y*256+x]=(WORD)(y*256+x);
 if(mode==3) {
  PatBlt(mdc,0,0,640,480,BLACKNESS);
  StretchDIBits(mdc,0,0,256,128,0,0,256,128,sbuf,(BITMAPINFO*)&bh,DIB_RGB_COLORS,SRCCOPY); GdiFlush();
 } else {
  clear_screen();
  if(mode==2) SetDIBitsToDevice(sdc,0,0,256,128,0,0,0,128,sbuf,(BITMAPINFO*)&bh,DIB_RGB_COLORS);
  else StretchDIBits(sdc,0,0,256*sc,128*sc,0,0,256,128,sbuf,(BITMAPINFO*)&bh,DIB_RGB_COLORS,SRCCOPY);
  GdiFlush(); shot(0,0,256*sc,128*sc);
 }
 for(y=0;y<128;y++) for(x=0;x<256;x++) {
  WORD v=(WORD)(y*256+x),o=tgt[(y*sc)*640+x*sc]; int r5=(v>>10)&31,g5=(v>>5)&31,b5=v&31,r5o=o>>11,g6o=(o>>5)&63,b5o=o&31; int ok=0;
  total++;
  if(r5o==r5) cr++; if(b5o==b5) cb++;
  if(g6o==(g5<<1)) {ca++; ok=1;} if(g6o==((g5<<1)|(g5>>4))) {cbb++; ok=1;} if(g6o==((g5<<1)|1)) {cc++; ok=1;}
  if(!ok) other++;
  if((r5o!=r5||b5o!=b5||!ok)&&ex<3) {logline("      source %04X came out %04X\n",(unsigned)v,(unsigned)o); ex++;}
  (void)mx; (void)my; (void)gv;
 }
 logline("    %-44s red kept %ld/%ld, blue kept %ld/%ld; green 6th bit: 0 %ld, top bit %ld, 1 %ld, none of these %ld\n",what,cr,total,cb,total,ca,cbb,cc,other);
}
static void conv565(void) {
 bi565 b; int x,y; long same=0,total=0,ex=0;
 mkbi565(&b,256,-128);
 for(y=0;y<128;y++) for(x=0;x<256;x++) ((WORD*)sbuf)[y*256+x]=(WORD)(y*256+x);
 clear_screen();
 StretchDIBits(sdc,0,0,256,128,0,0,256,128,sbuf,(BITMAPINFO*)&b,DIB_RGB_COLORS,SRCCOPY); GdiFlush(); shot(0,0,256,128);
 for(y=0;y<128;y++) for(x=0;x<256;x++) {WORD v=(WORD)(y*256+x); total++; if(tgt[y*640+x]==v) same++; else if(ex<3) {logline("      source %04X came out %04X\n",(unsigned)v,(unsigned)tgt[y*640+x]); ex++;}}
 logline("    565 BITFIELDS source, StretchDIBits 1:1: %ld of %ld values unchanged\n",same,total);
}
static void conv32(int bits) { /* 32-bit and 24-bit BI_RGB: 64x64 colours */
 BITMAPINFOHEADER bh; int x,y,stride=(64*bits/8+3)&~3; long tr=0,ro=0,both=0,none=0,total=0,ex=0; DWORD seed=777;
 memset(&bh,0,sizeof(bh)); bh.biSize=sizeof(bh); bh.biWidth=64; bh.biHeight=-64; bh.biPlanes=1; bh.biBitCount=(WORD)bits; bh.biCompression=BI_RGB;
 for(y=0;y<64;y++) for(x=0;x<64;x++) {
  BYTE r,g,b; BYTE *p=sbuf+y*stride+x*(bits/8);
  seed=seed*1103515245UL+12345UL; r=(BYTE)(seed>>16); seed=seed*1103515245UL+12345UL; g=(BYTE)(seed>>16); seed=seed*1103515245UL+12345UL; b=(BYTE)(seed>>16);
  p[0]=b; p[1]=g; p[2]=r; if(bits==32) p[3]=0;
 }
 clear_screen();
 StretchDIBits(sdc,0,0,64,64,0,0,64,64,sbuf,(BITMAPINFO*)&bh,DIB_RGB_COLORS,SRCCOPY); GdiFlush(); shot(0,0,64,64);
 for(y=0;y<64;y++) for(x=0;x<64;x++) {
  BYTE *p=sbuf+y*stride+x*(bits/8); int r=p[2],g=p[1],b=p[0]; WORD o=tgt[y*640+x];
  WORD t=(WORD)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)),q=(WORD)((((r*31+127)/255)<<11)|(((g*63+127)/255)<<5)|((b*31+127)/255));
  total++; if(o==t) tr++; if(o==q) ro++; if(o==t&&o==q) both++; if(o!=t&&o!=q) {none++; if(ex<3) {logline("      source %02X%02X%02X came out %04X (truncated %04X, rounded %04X)\n",r,g,b,(unsigned)o,(unsigned)t,(unsigned)q); ex++;}}
 }
 logline("    %d-bit BI_RGB source, StretchDIBits 1:1: truncated %ld/%ld, rounded %ld/%ld (agree %ld), neither %ld\n",bits,tr,total,ro,total,both,none);
}
static void t1(void) {
 logline("-- T1 colour conversion into the 565 screen --\n");
 conv555("16-bit 555 source, StretchDIBits 1:1 to the screen",0);
 conv555("16-bit 555 source, StretchDIBits 2x to the screen",1);
 conv555("16-bit 555 source, SetDIBitsToDevice to the screen",2);
 conv555("16-bit 555 source, StretchDIBits 1:1 to a memory DIB",3);
 conv565();
 conv32(32); conv32(24);
}

/* ---- T2 ---- */
#define SW 21
#define SH 13
static WORD src[SH][SW];
static WORD pv(int x,int y) {return (WORD)((((DWORD)(x+1)*1597UL+(DWORD)(y+1)*6571UL+12345UL)*40503UL)>>3);}
typedef struct {const char *name; int topdown,xs,ys,ws,hs,k,dx,dy,mx,my,clip;} cs_t;
static int caseno,casefails;
static void runcase(const cs_t *c) {
 bi565 b; int stride=(SW*2+3)&~3,x,y,s,wd=c->ws*c->k,hd=c->hs*c->k,bad=0,ex=0,place,bestbad=1<<30,bestplace=0; HRGN rg=NULL;
 int wx=c->dx-wd-6,wy=c->dy-hd-6,ww=2*wd+12,wh=2*hd+12; int alt=(c->mx||c->my)?2:1;
 if(wx<0) wx=0; if(wy<0) wy=0;
 for(s=0;s<SH;s++) {
  int row=c->topdown?s:(SH-1-s); WORD *p=(WORD*)(sbuf+(long)s*stride);
  for(x=0;x<SW;x++) p[x]=src[row][x];
  (void)p;
 }
 mkbi565(&b,SW,c->topdown?-SH:SH);
 clear_screen();
 if(c->clip) {rg=CreateRectRgn(c->dx+4,c->dy+3,c->dx+wd-5,c->dy+hd-4); SelectClipRgn(sdc,rg);}
 StretchDIBits(sdc,c->dx,c->dy,c->mx?-wd:wd,c->my?-hd:hd,c->xs,c->ys,c->ws,c->hs,sbuf,(BITMAPINFO*)&b,DIB_RGB_COLORS,SRCCOPY); GdiFlush();
 if(c->clip) {SelectClipRgn(sdc,NULL); DeleteObject(rg);}
 shot(wx,wy,ww,wh);
 for(place=0;place<alt;place++) { /* a mirrored destination may cover [dx-w+1,dx] (place 0) or [dx-w,dx-1] (place 1) */
  int px=c->mx?(c->dx-wd+(place==0?1:0)):c->dx,py=c->my?(c->dy-hd+(place==0?1:0)):c->dy; bad=0; ex=0;
  for(y=0;y<wh;y++) for(x=0;x<ww;x++) {
   int X=wx+x,Y=wy+y,i=X-px,j=Y-py; WORD want=0;
   if(i>=0&&i<wd&&j>=0&&j<hd) {
    int ii=c->mx?(wd-1-i):i,jj=c->my?(hd-1-j):j,col=c->xs+ii/c->k,ty=(c->topdown?c->ys:(SH-c->ys-c->hs))+jj/c->k;
    want=src[ty][col];
    if(c->clip&&!(X>=c->dx+4&&X<c->dx+wd-5&&Y>=c->dy+3&&Y<c->dy+hd-4)) want=0;
   }
   if(tgt[y*640+x]!=want) {bad++; if(place==0&&ex<3&&alt==1) {logline("      at %d,%d got %04X wanted %04X\n",X,Y,(unsigned)tgt[y*640+x],(unsigned)want); ex++;}}
  }
  if(bad<bestbad) {bestbad=bad; bestplace=place;}
 }
 caseno++;
 if(bestbad) casefails++;
 logline("    %-52s %s (%d of %d pixels differ%s)\n",c->name,bestbad?"FAIL":"PASS",bestbad,ww*wh,alt==2?(bestplace==0?"; mirror covers [x-w+1,x]":"; mirror covers [x-w,x-1]"):"");
}
static void t2(void) {
 static const cs_t cases[]={
  {"bottom-up, whole source, 2x at 100,60",0,0,0,SW,SH,2,100,60,0,0,0},
  {"top-down, whole source, 2x at 100,60",1,0,0,SW,SH,2,100,60,0,0,0},
  {"bottom-up, source rectangle 3,2 10x7, 2x",0,3,2,10,7,2,100,60,0,0,0},
  {"top-down, source rectangle 3,2 10x7, 2x",1,3,2,10,7,2,100,60,0,0,0},
  {"bottom-up, source rectangle 5,0 16x13, 3x at 101,63",0,5,0,16,13,3,101,63,0,0,0},
  {"bottom-up, source rectangle 0,6 21x7, 3x at 101,63",0,0,6,21,7,3,101,63,0,0,0},
  {"top-down, whole source, 3x at 101,63",1,0,0,SW,SH,3,101,63,0,0,0},
  {"1:1 (k=1), bottom-up, rectangle 2,3 15x8",0,2,3,15,8,1,100,60,0,0,0},
  {"bottom-up, whole source, 2x, clip region inside",0,0,0,SW,SH,2,100,60,0,0,1},
  {"top-down, rectangle 4,1 12x9, 3x, clip region",1,4,1,12,9,3,101,63,0,0,1},
  {"bottom-up, 2x, mirrored left-right",0,0,0,SW,SH,2,200,100,1,0,0},
  {"bottom-up, 2x, mirrored top-bottom",0,0,0,SW,SH,2,200,100,0,1,0},
  {"top-down, 2x, mirrored both ways",1,0,0,SW,SH,2,200,100,1,1,0},
 };
 int i,x,y;
 logline("-- T2 geometry (565 BITFIELDS source %dx%d, whole-number enlargement) --\n",SW,SH);
 for(y=0;y<SH;y++) for(x=0;x<SW;x++) src[y][x]=pv(x,y);
 for(i=0;i<(int)(sizeof(cases)/sizeof(cases[0]))&&!aborted;i++) {runcase(&cases[i]); pump();}
 logline("  T2: %d cases, %d failed\n",caseno,casefails);
}

/* ---- T3 ---- */
static BYTE *tb; static BITMAPINFO *tbi;
static double tm(int sw,int sh,int bits,int comp,int dw,int dh) {
 double t0; int i,stride; DWORD *m;
 memset(tbi,0,sizeof(BITMAPINFOHEADER)+12); tbi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER); tbi->bmiHeader.biWidth=sw; tbi->bmiHeader.biHeight=-sh; tbi->bmiHeader.biPlanes=1; tbi->bmiHeader.biBitCount=(WORD)bits; tbi->bmiHeader.biCompression=comp;
 m=(DWORD*)tbi->bmiColors; if(comp==BI_BITFIELDS) {m[0]=0xF800; m[1]=0x07E0; m[2]=0x001F;}
 stride=(sw*bits/8+3)&~3;
 { long n=(long)stride*sh,k; for(k=0;k<n;k++) tb[k]=(BYTE)(k*7+(k>>9)); }
 StretchDIBits(sdc,0,0,dw,dh,0,0,sw,sh,tb,tbi,DIB_RGB_COLORS,SRCCOPY); GdiFlush();
 t0=now(); for(i=0;i<10;i++) StretchDIBits(sdc,0,0,dw,dh,0,0,sw,sh,tb,tbi,DIB_RGB_COLORS,SRCCOPY); GdiFlush();
 return (now()-t0)/10.0;
}
static void t3(void) {
 logline("-- T3 timing, ms per frame (640x480 destination, 16-bit screen) --\n");
 logline("    555 BI_RGB source:      1:1 %.2f   2x %.2f   1/2 %.2f\n",tm(640,480,16,BI_RGB,640,480),tm(320,240,16,BI_RGB,640,480),tm(640,480,16,BI_RGB,320,240));
 logline("    565 BITFIELDS source:   1:1 %.2f   2x %.2f   1/2 %.2f\n",tm(640,480,16,BI_BITFIELDS,640,480),tm(320,240,16,BI_BITFIELDS,640,480),tm(640,480,16,BI_BITFIELDS,320,240));
 logline("    32-bit BI_RGB source:   1:1 %.2f   2x %.2f   1/2 %.2f\n",tm(640,480,32,BI_RGB,640,480),tm(320,240,32,BI_RGB,640,480),tm(640,480,32,BI_RGB,320,240));
 logline("    24-bit BI_RGB source:   1:1 %.2f   2x %.2f   1/2 %.2f\n",tm(640,480,24,BI_RGB,640,480),tm(320,240,24,BI_RGB,640,480),tm(640,480,24,BI_RGB,320,240));
 logline("    2x with 3x and 4x factors, 565 BITFIELDS: 3x (214x160) %.2f   4x (160x120) %.2f\n",tm(214,160,16,BI_BITFIELDS,642,480),tm(160,120,16,BI_BITFIELDS,640,480));
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 vx_status_t s; DWORD got=0,t; int rc=0,have_orig; DEVMODEA orig; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2SDIB.TXT","w"); logline("VX2SDIB v1.0\n");
 finish("RUNNING: the run did not reach its end (VX2SDIB.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 sbuf=(BYTE*)calloc(640UL*480UL*4UL,1); tb=(BYTE*)calloc(640UL*480UL*4UL,1); tbi=(BITMAPINFO*)calloc(sizeof(BITMAPINFOHEADER)+16,1);
 if(!sbuf||!tb||!tbi) {finish("FAIL: out of memory"); return 1;}
 memset(&orig,0,sizeof(orig)); orig.dmSize=sizeof(orig); have_orig=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&orig)?1:0;
 logline("desktop was %lux%lux%lu\n",have_orig?orig.dmPelsWidth:0,have_orig?orig.dmPelsHeight:0,have_orig?orig.dmBitsPerPel:0);
 if(!set_mode(640,480,16)) {finish("FAIL: 640x480x16 not available"); rc=1;}
 else {
  t=GetTickCount(); while(GetTickCount()-t<800) Sleep(20);
  memset(&s,0,sizeof(s));
  if(!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||s.width!=640||s.height!=480||s.bpp!=16||s.stage!=6||s.failure||s.vga_mode) {
   logline("the driver is not in native 640x480x16 mode\n"); finish("FAIL: not native mode"); rc=1;
  } else {
   sdc=GetDC(NULL);
   if(!sdc||!make_target()) {logline("no screen DC or no 565 DIB section\n"); finish("FAIL: no DIB section"); rc=1;}
   else {
    t1(); if(pump()) t2(); if(pump()) t3();
    SelectObject(mdc,olddib); DeleteObject(dib); DeleteDC(mdc); ReleaseDC(NULL,sdc);
   }
  }
 }
 if(aborted) {finish("ABORTED"); logline("aborted\n");}
 else if(!rc) finish("DONE: see VX2SDIB.TXT");
 if(have_orig) {ChangeDisplaySettingsA(&orig,0); logline("desktop put back\n");}
 logline("Finished.\n"); if(lf) fclose(lf);
 return rc;
}
