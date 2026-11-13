/* VXGTUNE v1.3 (driver v10.510; v1.2 v10.509): the four WinTune GDI categories (line drawing, DIB blit, filled objects, text draw), timed in the current desktop mode
 * with the driver's GDI options switched live, and every new path checked pixel for pixel against the way .506 drew the same thing.
 *  Lines: the driver's own CPU lines (ACCEL_CPULINE) with long pen-copy segments on the engine (ACCEL_CPULINE_MAX shallow, ACCEL_CPULINE_MAXY steep), against the .506 path
 *   (engine line per segment from 32 pixels, DIBENG below); a length sweep (8 to 512 pixels) times the CPU and the engine per length and names each crossover.
 *  Filled objects and text: the cursor exclusion without VxD calls (ACCEL_GDIFAST); the engine counters show which way each case went.
 *  DIB blit: SetDIBitsToDevice / StretchDIBits 1:1 and 2x from 1, 4, 8, 16 (555 and 565), 24 and 32-bit sources, 64x64 and 256x256: DIBENG, the shipped classes and every class;
 *   a route the driver drew that is slower than DIBENG, or an off class that would win, is named (a case DIBENG drew in both columns is never flagged).
 * Nothing is written to the registry; the live values go back to the registry ones at the end. Unattended: read VXGTUNE.TXT (VXGTUNE.RES says RUNNING until it ends). Esc stops.
 * Move nothing while it runs: the mouse pointer is parked in the bottom right corner. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

#define VXGT_ESCAPE 0x8F74
static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VXGTUNE.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1; return !aborted;}
static double now(void) {LARGE_INTEGER c,f; if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;} return (double)GetTickCount();}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static void gdiopt(DWORD v) {ctl(0xE9000000UL|(v&7UL),NULL);}
static void linemax(DWORD x,DWORD y) {ctl(0xE8000000UL|(x&4095UL)|((y&4095UL)<<12),NULL);}
#define OPT_OLD 0UL
#define OPT_NEW 3UL
static DWORD seed; static DWORD rnd(void) {seed=seed*1103515245UL+12345UL; return (seed>>8)&0xFFFFFFUL;}

#define AW 600
#define AH 440
static HDC sdc,tdc; static HBITMAP tbm,told; static DWORD *tbits,*ref; static int bpp,scrw,scrh;
static int make_target(void) {
 BITMAPINFOHEADER b; void *p=NULL; memset(&b,0,sizeof(b)); b.biSize=sizeof(b); b.biWidth=AW; b.biHeight=-AH; b.biPlanes=1; b.biBitCount=32;
 tbm=CreateDIBSection(sdc,(BITMAPINFO*)&b,DIB_RGB_COLORS,&p,NULL,0); if(!tbm||!p) return 0;
 tdc=CreateCompatibleDC(sdc); told=(HBITMAP)SelectObject(tdc,tbm); tbits=(DWORD*)p; return 1;
}
static void shot(void) {GdiFlush(); BitBlt(tdc,0,0,AW,AH,sdc,0,0,SRCCOPY); GdiFlush();}
static void background(void) {
 int y,x; for(y=0;y<AH;y+=20) for(x=0;x<AW;x+=40) {RECT r; HBRUSH b=CreateSolidBrush(RGB((x*7+y*3)&255,(x*2+y*5+80)&255,(x*11+y)&255)); r.left=x; r.top=y; r.right=x+40; r.bottom=y+20; FillRect(sdc,&r,b); DeleteObject(b);}
 GdiFlush();
}
static int differ(int *fx,int *fy) {int x,y,d=0; for(y=0;y<AH;y++) for(x=0;x<AW;x++) if((ref[y*AW+x]^tbits[y*AW+x])&0xFFFFFFUL) {if(!d) {*fx=x; *fy=y;} d++;} return d;}
static void drvcnt(DWORD *c,int clear) {WORD in=(WORD)(clear?1:0); memset(c,0,64); ExtEscape(sdc,VXGT_ESCAPE,2,(LPCSTR)&in,64,(LPSTR)c);}

/* ---- the drawing of each case; the same seed draws the same thing ---- */
typedef void (*drawfn)(int n);
static HPEN pens[8],dotpen; static HBRUSH sbr[4],hbr; static HFONT fonts[4];
static void l_long(int n) {int i; seed=11; for(i=0;i<n;i++) {SelectObject(sdc,pens[i&7]); MoveToEx(sdc,(int)(rnd()%AW),(int)(rnd()%AH),NULL); LineTo(sdc,(int)(rnd()%AW),(int)(rnd()%AH));}}
static void l_short(int n) {int i; seed=12; for(i=0;i<n;i++) {int x=(int)(rnd()%(AW-40))+20,y=(int)(rnd()%(AH-40))+20; SelectObject(sdc,pens[i&7]); MoveToEx(sdc,x,y,NULL); LineTo(sdc,x+(int)(rnd()%33)-16,y+(int)(rnd()%33)-16);}}
static void l_hv(int n) {int i; seed=13; for(i=0;i<n;i++) {int x=(int)(rnd()%AW),y=(int)(rnd()%AH); SelectObject(sdc,pens[i&7]); MoveToEx(sdc,x,y,NULL); if(i&1) LineTo(sdc,x,(int)(rnd()%AH)); else LineTo(sdc,(int)(rnd()%AW),y);}}
static void l_poly(int n) {POINT p[64]; int i,k; seed=14; for(i=0;i<n;i++) {for(k=0;k<64;k++) {p[k].x=(int)(rnd()%AW); p[k].y=(int)(rnd()%AH);} SelectObject(sdc,pens[i&7]); Polyline(sdc,p,64);}}
static void l_xor(int n) {int i; seed=15; SetROP2(sdc,R2_XORPEN); for(i=0;i<n;i++) {SelectObject(sdc,pens[i&7]); MoveToEx(sdc,(int)(rnd()%AW),(int)(rnd()%AH),NULL); LineTo(sdc,(int)(rnd()%AW),(int)(rnd()%AH));} SetROP2(sdc,R2_COPYPEN);}
static void l_clip(int n) {int i; HRGN r=CreateRectRgn(100,80,500,360); seed=16; SelectClipRgn(sdc,r); for(i=0;i<n;i++) {SelectObject(sdc,pens[i&7]); MoveToEx(sdc,(int)(rnd()%AW),(int)(rnd()%AH),NULL); LineTo(sdc,(int)(rnd()%AW),(int)(rnd()%AH));} SelectClipRgn(sdc,NULL); DeleteObject(r);}
static void l_dot(int n) {int i; seed=17; SelectObject(sdc,dotpen); for(i=0;i<n;i++) {MoveToEx(sdc,(int)(rnd()%AW),(int)(rnd()%AH),NULL); LineTo(sdc,(int)(rnd()%AW),(int)(rnd()%AH));}}
static void f_rect_s(int n) {int i; seed=21; SelectObject(sdc,GetStockObject(NULL_PEN)); for(i=0;i<n;i++) {int x=(int)(rnd()%(AW-20)),y=(int)(rnd()%(AH-20)); SelectObject(sdc,sbr[i&3]); Rectangle(sdc,x,y,x+17,y+17);}}
static void f_rect_m(int n) {int i; seed=22; SelectObject(sdc,GetStockObject(NULL_PEN)); for(i=0;i<n;i++) {int x=(int)(rnd()%(AW-80)),y=(int)(rnd()%(AH-80)); SelectObject(sdc,sbr[i&3]); Rectangle(sdc,x,y,x+65,y+65);}}
static void f_rect_l(int n) {int i; seed=23; SelectObject(sdc,GetStockObject(NULL_PEN)); for(i=0;i<n;i++) {int x=(int)(rnd()%(AW-260)),y=(int)(rnd()%(AH-260)); SelectObject(sdc,sbr[i&3]); Rectangle(sdc,x,y,x+257,y+257);}}
static void f_hatch(int n) {int i; seed=24; SelectObject(sdc,GetStockObject(NULL_PEN)); SelectObject(sdc,hbr); SetBkColor(sdc,RGB(240,240,200)); for(i=0;i<n;i++) {int x=(int)(rnd()%(AW-100)),y=(int)(rnd()%(AH-100)); Rectangle(sdc,x,y,x+97,y+61);}}
static void f_ellipse(int n) {int i; seed=25; SelectObject(sdc,pens[0]); for(i=0;i<n;i++) {int x=(int)(rnd()%(AW-120)),y=(int)(rnd()%(AH-120)); SelectObject(sdc,sbr[i&3]); Ellipse(sdc,x,y,x+20+(int)(rnd()%100),y+20+(int)(rnd()%100));}}
static void f_poly(int n) {int i,k; POINT p[6]; seed=26; SelectObject(sdc,pens[1]); for(i=0;i<n;i++) {int x=(int)(rnd()%(AW-120)),y=(int)(rnd()%(AH-120)); for(k=0;k<6;k++) {p[k].x=x+(int)(rnd()%120); p[k].y=y+(int)(rnd()%120);} SelectObject(sdc,sbr[i&3]); Polygon(sdc,p,6);}}
static void f_round(int n) {int i; seed=27; SelectObject(sdc,pens[2]); for(i=0;i<n;i++) {int x=(int)(rnd()%(AW-120)),y=(int)(rnd()%(AH-90)); SelectObject(sdc,sbr[i&3]); RoundRect(sdc,x,y,x+110,y+80,24,24);}}
static const char *txt="The quick brown fox jumps over the lazy dog 0123456789";
static void t_opaque(int n) {int i; seed=31; SetBkMode(sdc,OPAQUE); for(i=0;i<n;i++) {SelectObject(sdc,fonts[i&3]); SetTextColor(sdc,RGB(rnd()&255,0,rnd()&255)); SetBkColor(sdc,RGB(255,255,rnd()&255)); TextOut(sdc,(int)(rnd()%200),(int)(rnd()%(AH-30)),txt,(int)strlen(txt));}}
static void t_trans(int n) {int i; seed=32; SetBkMode(sdc,TRANSPARENT); for(i=0;i<n;i++) {SelectObject(sdc,fonts[i&3]); SetTextColor(sdc,RGB(0,rnd()&255,rnd()&255)); TextOut(sdc,(int)(rnd()%200),(int)(rnd()%(AH-30)),txt,(int)strlen(txt));} SetBkMode(sdc,OPAQUE);}
static void t_ext(int n) {int i; RECT r; seed=33; for(i=0;i<n;i++) {r.left=(int)(rnd()%200); r.top=(int)(rnd()%(AH-30)); r.right=r.left+380; r.bottom=r.top+18; SelectObject(sdc,fonts[2]); ExtTextOut(sdc,r.left,r.top,ETO_OPAQUE|ETO_CLIPPED,&r,txt,(int)strlen(txt),NULL);}}

typedef struct {const char *name; drawfn fn; int n; int cat;} case_t; /* cat 0 lines, 1 fills, 2 text */
static const case_t cases[]={
 {"lines: random, long (MoveTo/LineTo, 8 pens)",l_long,600,0},{"lines: short (up to 16 pixels)",l_short,3000,0},{"lines: horizontal and vertical",l_hv,1500,0},
 {"lines: Polyline, 64 points",l_poly,40,0},{"lines: R2_XORPEN",l_xor,400,0},{"lines: clipped to a region",l_clip,600,0},{"lines: PS_DOT (engine / DIBENG as before)",l_dot,200,0},
 {"fill: Rectangle 16x16 solid",f_rect_s,3000,1},{"fill: Rectangle 64x64 solid",f_rect_m,800,1},{"fill: Rectangle 256x256 solid",f_rect_l,60,1},{"fill: hatched brush 96x60",f_hatch,300,1},
 {"fill: Ellipse",f_ellipse,300,1},{"fill: Polygon, 6 points",f_poly,300,1},{"fill: RoundRect",f_round,200,1},
 {"text: TextOut opaque, 4 fonts",t_opaque,400,2},{"text: TextOut transparent, 4 fonts",t_trans,400,2},{"text: ExtTextOut opaque rectangle, clipped",t_ext,400,2}};
#define NCASE (sizeof(cases)/sizeof(cases[0]))
/* ---- the length sweep: lines of exactly L pixels along the major axis, shallow (X-major) and steep (Y-major), pen copy ---- */
static int sw_len,sw_steep;
static void l_len(int n) {
 int i; seed=(DWORD)(50+sw_len+sw_steep);
 for(i=0;i<n;i++) {
  int L=sw_len,mn=(int)(rnd()%(DWORD)(L+1)),sx=(rnd()&1)?1:-1,sy=(rnd()&1)?1:-1,dx,dy,x0,y0;
  if(sw_steep&&L>AH-2) L=AH-2;
  dx=sw_steep?sx*(mn<L?mn:L)/2:sx*L; dy=sw_steep?sy*L:sy*mn/2;
  x0=dx<0?(int)(rnd()%(DWORD)(AW+dx))-dx:(int)(rnd()%(DWORD)(AW-dx)); y0=dy<0?(int)(rnd()%(DWORD)(AH+dy))-dy:(int)(rnd()%(DWORD)(AH-dy));
  SelectObject(sdc,pens[i&7]); MoveToEx(sdc,x0,y0,NULL); LineTo(sdc,x0+dx,y0+dy);
 }
}
static int crossover[2]={-1,-1}; static double cross_at[2]; static DWORD reg_lmax,reg_lmaxy;
static void sweep(void) {
 static const int lens[]={8,16,32,48,64,96,128,160,192,256,384,512}; int li,st,n; double tc[2][12],te[2][12],t5[2][12],s0;
 logline("\n-- line length sweep: us a line (pen copy, 8 pens, random direction; steep lines stop at %d); CPU = the driver's own loop, engine = one VxD call a line (thresholds off), .506 = engine from 32 pixels, DIBENG below --\n",AH-2);
 logline("  length |  shallow: CPU   engine   .506 |  steep: CPU   engine   .506\n");
 for(li=0;li<12&&pump();li++) {
  sw_len=lens[li]; n=sw_len<=64?2000:sw_len<=192?800:400;
  for(st=0;st<2;st++) {
   sw_steep=st;
   ctl(5,NULL); gdiopt(OPT_NEW); linemax(4095,4095); background(); pump(); s0=now(); l_len(n); GdiFlush(); tc[st][li]=(now()-s0)*1000.0/n;
   linemax(0,0); background(); pump(); s0=now(); l_len(n); GdiFlush(); te[st][li]=(now()-s0)*1000.0/n;
   ctl(0xFFFFFFFFUL,NULL); gdiopt(OPT_OLD); background(); pump(); s0=now(); l_len(n); GdiFlush(); t5[st][li]=(now()-s0)*1000.0/n;
  }
  ctl(0xFFFFFFFFUL,NULL);
  logline("  %6d | %10.2f %8.2f %6.2f | %10.2f %8.2f %6.2f\n",sw_len,tc[0][li],te[0][li],t5[0][li],tc[1][li],te[1][li],t5[1][li]);
 }
 for(st=0;st<2;st++) for(li=0;li<12;li++) if(te[st][li]<tc[st][li]) {int k,ok=1; for(k=li;k<12;k++) if(te[st][k]>=tc[st][k]) ok=0;
  if(ok) { /* v1.3: where the two lines cross between this length and the one before (the first length measured with the engine ahead is only an upper bound) */
   double x=lens[li];
   if(li>0) {double d0=te[st][li-1]-tc[st][li-1],d1=te[st][li]-tc[st][li]; if(d0>0.0&&d0-d1>0.0) x=lens[li-1]+(lens[li]-lens[li-1])*d0/(d0-d1);}
   cross_at[st]=x; crossover[st]=((int)(x+0.999)+15)/16*16; /* the driver keeps the lengths in sixteens */
   break;}}
 for(st=0;st<2;st++) {
  if(crossover[st]>0) logline("  %s: the engine and the CPU cross at about %.0f pixels (between the lengths measured): %s=%d fits this machine (shipped %lu)\n",st?"steep":"shallow",cross_at[st],st?"ACCEL_CPULINE_MAXY":"ACCEL_CPULINE_MAX",crossover[st],st?reg_lmaxy:reg_lmax);
  else logline("  %s: the CPU is as fast or faster at every length measured: %s=4095 (shipped %lu)\n",st?"steep":"shallow",st?"ACCEL_CPULINE_MAXY":"ACCEL_CPULINE_MAX",st?reg_lmaxy:reg_lmax);
 }
}
static const char *catname[4]={"Line drawing","Filled objects","Text draw","DIB blit"};
static double cat_old[4],cat_new[4];

/* .506 behaviour: CPU lines and fast access off */
static double timed(const case_t *c,DWORD opt) {double s; gdiopt(opt); background(); pump(); s=now(); c->fn(c->n); GdiFlush(); return now()-s;}
static int run_gdi_case(const case_t *c) {
 DWORD d0[16],d1[16],e0[4],e1[4]; double told_,tnew,tbr; int diff,fx=-1,fy=-1,ok=1;
 /* pixels: .506 way, then the new one */
 gdiopt(OPT_OLD); background(); c->fn(c->n<200?c->n:200); shot(); memcpy(ref,tbits,AW*AH*4);
 gdiopt(OPT_NEW); background(); drvcnt(d0,0); ctl(0xFFFFFFFEUL,e0); c->fn(c->n<200?c->n:200); GdiFlush(); drvcnt(d1,0); ctl(0xFFFFFFFEUL,e1); shot();
 diff=differ(&fx,&fy); if(diff) ok=0;
 /* times: old, new, new lines with the exclusion calls (fast access off) */
 told_=timed(c,OPT_OLD); tnew=timed(c,OPT_NEW); tbr=timed(c,OPT_NEW&~2UL);
 cat_old[c->cat]+=told_; cat_new[c->cat]+=tnew;
 logline("  %-44s %s %5d px | .506 %7.1f ms  new %7.1f ms (%5.2fx)  no GDIFAST %7.1f | CPU lines %lu segs %lu clipped %lu, to engine %lu | fast %lu, calls %lu+%lu, released %lu, engine ops %lu\n",
  c->name,diff?"DIFF":"ok  ",diff,told_,tnew,tnew>0?told_/tnew:0.0,tbr,d1[0]-d0[0],d1[1]-d0[1],d1[3]-d0[3],d1[12]-d0[12],d1[5]-d0[5],d1[4]-d0[4],d1[6]-d0[6],d1[7]-d0[7],e1[0]-e0[0]);
 if(diff) logline("     first difference at %d,%d: .506 %06lX, new %06lX\n",fx,fy,ref[fy*AW+fx]&0xFFFFFFUL,tbits[fy*AW+fx]&0xFFFFFFUL);
 return ok;
}

/* ---- DIB blits ---- */
typedef struct {BITMAPINFOHEADER h; DWORD m[256];} binfo;
static binfo bi; static BYTE *bits;
static int mkdib(int w,int h,int b,int f565) {
 DWORD n,i; memset(&bi,0,sizeof(bi)); bi.h.biSize=sizeof(bi.h); bi.h.biWidth=w; bi.h.biHeight=h; bi.h.biPlanes=1; bi.h.biBitCount=(WORD)b;
 if(f565) {bi.h.biCompression=BI_BITFIELDS; bi.m[0]=0xF800; bi.m[1]=0x07E0; bi.m[2]=0x001F;}
 if(b<=8) {for(i=0;i<(1UL<<b);i++) bi.m[i]=rnd(); bi.h.biClrUsed=0;}
 n=(DWORD)(((w*b+31)/32)*4)*h; free(bits); bits=(BYTE*)malloc(n); if(!bits) return 0;
 for(i=0;i<n;i++) bits[i]=(BYTE)(rnd()^(i*7)); return 1;
}
typedef struct {const char *name; int b,f565,op;} dib_t; /* op 0 SetDIBitsToDevice, 1 StretchDIBits 1:1, 2 StretchDIBits 2x */
static const dib_t dibs[]={{"1-bit",1,0,0},{"4-bit",4,0,0},{"8-bit",8,0,0},{"16-bit 555",16,0,0},{"16-bit 565",16,1,0},{"24-bit",24,0,0},{"32-bit",32,0,0},
 {"8-bit",8,0,1},{"24-bit",24,0,1},{"32-bit",32,0,1},{"8-bit",8,0,2},{"16-bit 555",16,0,2},{"24-bit",24,0,2},{"32-bit",32,0,2}};
#define NDIB (sizeof(dibs)/sizeof(dibs[0]))
static int dib_w,dib_n;
static void dib_draw(const dib_t *d,int n) {
 int i,x,y,w=dib_w; seed=41;
 for(i=0;i<n;i++) {
  x=(int)(rnd()%(AW-2*w>0?AW-2*w:1)); y=(int)(rnd()%(AH-2*w>0?AH-2*w:1)); if(d->op!=2) {x=(int)(rnd()%(AW-w)); y=(int)(rnd()%(AH-w));}
  if(d->op==0) SetDIBitsToDevice(sdc,x,y,w,w,0,0,0,w,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS);
  else if(d->op==1) StretchDIBits(sdc,x,y,w,w,0,0,w,w,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY);
  else StretchDIBits(sdc,x,y,2*w,2*w,0,0,w,w,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY);
 }
}
/* configurations: stretch class masks and GDI options */
typedef struct {const char *name; DWORD m1,m2,opt;} cfg_t;
static DWORD reg_m1,reg_m2;
static cfg_t cfgs[3];
static void set_cfg(const cfg_t *c) {ctl(0xFB000000UL|c->m1,NULL); ctl(0xFA000000UL|c->m2,NULL); gdiopt(c->opt);}
static double cfg_time[NDIB][2][3]; static int cfg_drv[3]; static int dib_fails,dib_slow,dib_could;
static void run_dib(int di,int si) {
 const dib_t *d=&dibs[di]; int k,diff,fx,fy; double s,t; DWORD a0[4],a1[4],h0[4],h1[4];
 dib_w=si?256:64; dib_n=si?(d->op==2?6:24):(d->op==2?100:300);
 seed=(DWORD)(1000+di*10+si); if(!mkdib(dib_w,dib_w,d->b,d->f565)) return;
 set_cfg(&cfgs[0]); background(); dib_draw(d,8); shot(); memcpy(ref,tbits,AW*AH*4);
 logline("  %-11s %-17s %3dx%-3d", d->name,d->op==0?"SetDIBitsToDevice":d->op==1?"StretchDIBits 1:1":"StretchDIBits 2x",dib_w,dib_w);
 for(k=0;k<3&&pump();k++) {
  set_cfg(&cfgs[k]); ctl(0xFFFFFE13UL,a0); ctl(0xFFFFFE26UL,h0); background(); dib_draw(d,8); GdiFlush(); ctl(0xFFFFFE13UL,a1); ctl(0xFFFFFE26UL,h1); shot();
  diff=k?differ(&fx,&fy):0;
  background(); s=now(); dib_draw(d,dib_n); GdiFlush(); t=(now()-s)/dib_n; cfg_time[di][si][k]=t;
  cfg_drv[k]=(a1[0]-a0[0])!=0;
  logline(" | %7.3f %s ",t,diff?"DIFF":cfg_drv[k]?"drv ":"dib ");
  if(diff) dib_fails++;
  if(diff) logline("[%d px, first %d,%d: %06lX vs %06lX] ",diff,fx,fy,ref[fy*AW+fx]&0xFFFFFFUL,tbits[fy*AW+fx]&0xFFFFFFUL);
 }
 {double D=cfg_time[di][si][0],S=cfg_time[di][si][1],A=cfg_time[di][si][2];
  if(cfg_drv[1]&&S>D*1.03) {dib_slow++; logline("  <- the shipped route is slower than DIBENG");}
  else if(cfg_drv[2]&&!cfg_drv[1]&&A<S*0.97&&A<D*0.97) {dib_could++; logline("  <- a class that is off would win");}}
 logline("\n");
}

/* A blinking caret of whatever window has the focus shows through the screen DC these cases draw on (VX855 and VX800 boards, .593 and
 * .594: 7 pixels at x = 6 with complementary colours, in a different case every run, DIBENG against itself too). The focus goes to a
 * window of this tool's own, which has no caret; Windows takes the old window's caret away with its focus. Returns what happened. */
/* (.595 on the VX800 board: 480 pixels in a 16x30 block of the untouched background differed once and not again: another window
 * painted a piece of itself between the two pictures.) The window now covers the whole screen, topmost, and never paints or erases:
 * the windows under it are clipped away, and the cases draw over it through the screen DC as before. */
static LRESULT CALLBACK cover_proc(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_ERASEBKGND) return 1;
 if(m==WM_PAINT) {ValidateRect(w,NULL); return 0;}
 return DefWindowProcA(w,m,a,b);
}
static HWND nocaret_w;
static int no_caret(void) {
 HWND fg=GetForegroundWindow(); DWORD me=GetCurrentThreadId(),other=fg?GetWindowThreadProcessId(fg,NULL):0; int att=0,ok; MSG m; DWORD t;
 {WNDCLASSA wc; memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=cover_proc; wc.hInstance=GetModuleHandleA(NULL); wc.lpszClassName="VXCOVER"; wc.hCursor=LoadCursorA(NULL,IDC_ARROW); RegisterClassA(&wc);}
 nocaret_w=CreateWindowExA(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,"VXCOVER","",WS_POPUP,0,0,4096,4096,NULL,NULL,GetModuleHandleA(NULL),NULL);
 if(!nocaret_w) return 0;
 ShowWindow(nocaret_w,SW_SHOW);
 if(other&&other!=me) att=AttachThreadInput(me,other,TRUE)?1:0;
 SetForegroundWindow(nocaret_w); SetActiveWindow(nocaret_w); SetFocus(nocaret_w);
 if(att) AttachThreadInput(me,other,FALSE);
 for(t=GetTickCount();GetTickCount()-t<300;) {while(PeekMessageA(&m,NULL,0,0,PM_REMOVE)) DispatchMessageA(&m); Sleep(10);}
 ok=GetForegroundWindow()==nocaret_w;
 return ok?1:2;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 DWORD c[4],dc[16]; int ci,fails=0,di,si,k; char res[300]; double best; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VXGTUNE.TXT","w"); logline("VXGTUNE v1.5 / driver .597\n"); finish("RUNNING: the run did not reach its end (VXGTUNE.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xFFFFFE26UL,c)) {finish("FAIL: no control interface"); return 1;}
 reg_lmax=c[1]&0xFFFFUL; reg_lmaxy=c[1]>>16; logline("GDI options (registry): %lu = CPULINE %lu, GDIFAST %lu; CPULINE_MAX %lu, CPULINE_MAXY %lu; software cursor shows put off so far %lu\n",c[0],c[0]&1,(c[0]>>1)&1,reg_lmax,reg_lmaxy,c[3]);
 if(c[0]>3||!(c[1]>>16)) {finish("FAIL: VX85598.VXD older than v10.509"); return 1;}
 {int nc=no_caret(); logline("the focus moved to this tool's own window (no caret on the screen): %s\n",nc==1?"yes":nc==2?"NO: another window kept the foreground; a blinking caret may show as 7 differing pixels":"NO: no window");}
 ctl(0xFFFFFE13UL,c); reg_m1=c[3]; ctl(0xFFFFFE14UL,c); reg_m2=c[3];
 sdc=GetDC(NULL); if(!sdc||!make_target()) {finish("FAIL: no screen DC"); return 1;}
 bpp=GetDeviceCaps(sdc,BITSPIXEL)*GetDeviceCaps(sdc,PLANES); scrw=GetSystemMetrics(SM_CXSCREEN); scrh=GetSystemMetrics(SM_CYSCREEN);
 logline("desktop %dx%dx%d; ACCEL_STRETCH %lu, ACCEL_STRETCH2 %lu\n",scrw,scrh,bpp,reg_m1,reg_m2);
 drvcnt(dc,1); if(!ExtEscape(sdc,VXGT_ESCAPE,0,NULL,64,(LPSTR)dc)) logline("WARNING: the display driver has no VXGT escape (VX85598.DRV older than v10.509): its counters read 0\n");
 ref=(DWORD*)calloc(AW*AH,4); if(!ref) {finish("FAIL: out of memory"); return 1;}
 SetCursorPos(scrw-1,scrh-1);
 for(k=0;k<8;k++) pens[k]=CreatePen(PS_SOLID,1,RGB((k*97)&255,(k*53+40)&255,(k*151+90)&255));
 dotpen=CreatePen(PS_DOT,1,RGB(200,20,20));
 for(k=0;k<4;k++) sbr[k]=CreateSolidBrush(RGB((k*80+30)&255,(k*40+120)&255,(200-k*50)&255));
 hbr=CreateHatchBrush(HS_DIAGCROSS,RGB(20,60,200));
 fonts[0]=(HFONT)GetStockObject(SYSTEM_FONT); fonts[1]=CreateFont(-11,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,0,0,0,0,"MS Sans Serif");
 fonts[2]=CreateFont(-16,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,0,0,0,0,"Arial"); fonts[3]=CreateFont(-24,0,0,0,FW_BOLD,1,0,0,ANSI_CHARSET,0,0,0,0,"Times New Roman");
 logline("\n-- lines, fills, text: .506 way (CPULINE 0, GDIFAST 0) against v10.510 (both 1, CPULINE_MAX %lu, CPULINE_MAXY %lu), pixels compared, then timed --\n",reg_lmax,reg_lmaxy);
 for(ci=0;ci<(int)NCASE&&pump();ci++) if(!run_gdi_case(&cases[ci])) fails++;
 sweep();
 logline("\n-- DIB blits: ms per call --\n   columns: DIBENG (classes off) | shipped classes | all classes\n   'dib' = DIBENG drew it, 'drv' = the VxD did, DIFF = pixels differ from DIBENG\n");
 cfgs[0].name="DIBENG"; cfgs[0].m1=0; cfgs[0].m2=0; cfgs[0].opt=OPT_NEW;
 cfgs[1].name="shipped"; cfgs[1].m1=reg_m1; cfgs[1].m2=reg_m2; cfgs[1].opt=OPT_NEW;
 cfgs[2].name="all classes"; cfgs[2].m1=0xFFFFFF; cfgs[2].m2=0xFFFFFF; cfgs[2].opt=OPT_NEW;
 if(bpp!=16&&bpp!=32) logline("   (the VxD's DIB paths take 16 and 32 bpp screens only: everything below is DIBENG)\n");
 for(di=0;di<(int)NDIB&&pump();di++) for(si=0;si<2&&pump();si++) run_dib(di,si);
 /* the WinTune-like totals: DIB = the 1:1 and 2x cases at 256x256, shipped against the best route */
 for(di=0;di<(int)NDIB;di++) {
  cat_old[3]+=cfg_time[di][1][0]*10; cat_new[3]+=cfg_time[di][1][1]*10;
 }
 logline("\n-- per category (sum of the cases above; DIB: 10 calls of each 256x256 case, DIBENG alone against the shipped classes) --\n");
 for(k=0;k<4;k++) logline("  %-15s %s %9.1f ms   v10.510 %9.1f ms   %5.2fx\n",catname[k],k==3?"DIBENG":".506  ",cat_old[k],cat_new[k],cat_new[k]>0?cat_old[k]/cat_new[k]:0.0);
 ctl(0xFFFFFFFFUL,NULL); ctl(0xFFFFFE26UL,c);
 logline("live values put back to the registry ones (GDI options %lu); software cursor shows put off %lu\n",c[0],c[3]);
 for(k=0;k<8;k++) DeleteObject(pens[k]); DeleteObject(dotpen); for(k=0;k<4;k++) DeleteObject(sbr[k]); DeleteObject(hbr); for(k=1;k<4;k++) DeleteObject(fonts[k]);
 SelectObject(tdc,told); DeleteObject(tbm); DeleteDC(tdc); ReleaseDC(NULL,sdc);
 if(aborted) sprintf(res,"FAIL: aborted");
 else sprintf(res,"%s: %d of %d line/fill/text cases differ from the .506 way, %d DIB routes differ from DIBENG; %d shipped DIB routes slower than DIBENG, %d off that would win; line crossovers %d shallow, %d steep (see VXGTUNE.TXT)",(fails||dib_fails)?"FAIL":"PASS",fails,(int)NCASE,dib_fails,dib_slow,dib_could,crossover[0],crossover[1]);
 finish(res); logline("%s\nFinished.\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
