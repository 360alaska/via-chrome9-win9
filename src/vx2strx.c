/* VX2STRX v2.3: the DIB conversion fast path of driver v10.103 (StretchDIBits and DibToDevice into the 16bpp and 32bpp screen), compared with DIBENG pixel for pixel before it is switched on.
 * The desktop is switched to 640x480x16 and then 640x480x32 (and put back). Every case is drawn twice on the screen: first with the fast path off (live class mask 0, so DIBENG does it), then with one class on
 * (the VxD converts and draws the bitmap itself). The two screen images, read back through a DIB section of the screen's depth, and the two return values of the call must be identical, and the driver's counters must show
 * that the fast path really drew the case. Random sources (555, 565 BITFIELDS, 24-bit, 32-bit; bottom-up and top-down; random colours), StretchDIBits with factors 1..4 on each axis, SetDIBitsToDevice with source
 * rectangles and partial bitmaps (StartScan / cLines), origins that are negative or run off the screen, no clip / a clip rectangle / an elliptic clip region. Mirrored destinations, which the fast path leaves to DIBENG, are compared too.
 * Classes (ACCEL_STRETCH bits): 16bpp screen: 1 = 555, both factors >= 2; 2 = 24-bit, both >= 2; 4 = 32-bit, both >= 2; 8 = 24/32-bit with a factor 1 on one axis; 16 = 555 with a factor 1 on one axis; 32 = 24/32-bit 1:1; 64 = 555 1:1.
 * 32bpp screen: 128 = 555, both factors >= 2; 256 = 555 with a factor 1 on one axis; 512 / 1024 = the same for 565 BITFIELDS; 2048 / 4096 = the same for 24-bit; 8192 / 16384 / 32768 = 555 / 565 / 24-bit 1:1 (32-bit sources are fast in DIBENG already).
 * 8-bit (palette) sources, colour table in the header, 32bpp screen only: 524288 = both factors >= 2, 1048576 = a factor 1 on one axis, 2097152 = 1:1 (into the 16bpp screen DIBENG is as fast and rounds the colours its own way: left to it). A fifth of the 8-bit cases use DIB_PAL_COLORS (index colours): they must come out the same as DIBENG whatever the driver does with them.
 * Each class passes or fails alone. A class is also timed against DIBENG (representative 640x480 calls) and is only enabled when the fast path is not slower: the pixels must match AND the class must pay.
 * The unused top byte of a 32bpp pixel is ignored in the comparison (DIBENG leaves old data there). The classes that pass are written to HKLM\Software\VX85598 ACCEL_STRETCH (restart Windows to use them); a phase that could not run keeps its old bits. The live setting is put back.
 * At the end a probe records what the driver is called with for a top-down bitmap drawn with SetDIBitsToDevice (StartScan / cLines); it only logs. Needs driver v10.103 (VX85598.DRV and VX85598.VXD).
 * Unattended: read VX2STRX.TXT (VX2STRX.RES says RUNNING until it ends). Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"
#include "vx_hatch_diag.h"

#define PERCLASS 56
static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {
 va_list a;
 if(!lf) return;
 va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));
}
static void finish(const char *res) {FILE *f=fopen("VX2STRX.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
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
/* driver control: 0xFFFFFE13 reads out[0] conversions drawn, out[1] declined, out[2] pixels drawn, out[3] class mask; 0xFB000000|m sets the live class mask */
static int ctl(DWORD code,DWORD *out) {
 DWORD o[4],got=0; memset(o,0,sizeof(o));
 if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0;
 if(out) memcpy(out,o,16);
 return 1;
}
static int counters(DWORD *c) {return ctl(0xFFFFFE13UL,c);}
static void setmask(DWORD m) {ctl(0xFB000000UL|(m&0xFFFFFFUL),NULL);}
static int hesc(HDC dc,DWORD command,void *out) {
 vxh_command c; c.cb=sizeof(c); c.magic=VXH_MAGIC; c.command=command; c.reserved=0;
 return ExtEscape(dc,VXH_ESCAPE,sizeof(c),(LPCSTR)&c,out?(int)sizeof(vxh_reply):0,(LPSTR)out);
}

static vxh_reply rep_;
static HDC sdc,mdc; static HBITMAP dib,olddib; static BYTE *tgt; static int tbits;
typedef struct {BITMAPINFOHEADER h; DWORD m[3];} bi565;
static int make_target(int bits) {
 bi565 b; void *p=NULL;
 memset(&b,0,sizeof(b)); b.h.biSize=sizeof(BITMAPINFOHEADER); b.h.biWidth=640; b.h.biHeight=-480; b.h.biPlanes=1; b.h.biBitCount=(WORD)bits;
 if(bits==16) {b.h.biCompression=BI_BITFIELDS; b.m[0]=0xF800; b.m[1]=0x07E0; b.m[2]=0x001F;}
 dib=CreateDIBSection(sdc,(BITMAPINFO*)&b,DIB_RGB_COLORS,&p,NULL,0);
 if(!dib||!p) return 0;
 mdc=CreateCompatibleDC(sdc); olddib=(HBITMAP)SelectObject(mdc,dib); tgt=(BYTE*)p; tbits=bits;
 return 1;
}
static void drop_target(void) {SelectObject(mdc,olddib); DeleteObject(dib); DeleteDC(mdc); dib=NULL; mdc=NULL;}
static DWORD px(int x,int y) {return tbits==16?(DWORD)((WORD*)tgt)[y*640+x]:(((DWORD*)tgt)[y*640+x]&0x00FFFFFFUL);}
static void shot(int x,int y,int w,int h) {BitBlt(mdc,0,0,w,h,sdc,x,y,SRCCOPY); GdiFlush();}
static void clear_screen(void) {
 RECT rc; HBRUSH hb=CreateSolidBrush(RGB(0,0,0));
 rc.left=0; rc.top=0; rc.right=640; rc.bottom=480; FillRect(sdc,&rc,hb); DeleteObject(hb); GdiFlush();
}
static DWORD seed=20261021UL;
static DWORD rnd(void) {seed=seed*1103515245UL+12345UL; return (seed>>8)&0xFFFFFFUL;}

/* ---- classes ---- */
typedef struct {int bit,dbpp,kind,shape;} cls_t; /* kind 0 555, 1 565, 2 24-bit, 3 24/32-bit (16bpp screen only); shape 0 both factors >= 2, 1 one factor 1, 2 1:1 */
static const cls_t classes[]={
 {1,16,0,0},{2,16,2,0},{4,16,4,0},{8,16,3,1},{16,16,0,1},{32,16,3,2},{64,16,0,2},
 {128,32,0,0},{256,32,0,1},{512,32,1,0},{1024,32,1,1},{2048,32,2,0},{4096,32,2,1},{8192,32,0,2},{16384,32,1,2},{32768,32,2,2},
 {524288,32,5,0},{1048576,32,5,1},{2097152,32,5,2}};
static const char *kname[6]={"555","565","24-bit","24/32-bit","32-bit","8-bit palette"};
static const char *shname[4]={"both factors >= 2","a factor 1 on one axis","1:1","1:1 or both factors >= 2"};
#define NCLS 19
typedef struct {int cases,eligible,engaged,missed,mism,retmis,declined_ok,declined_bad,modelok,vis,onmodel; DWORD ops,pixels; int shown;} stat_t;
static stat_t st[NCLS];

/* ---- cases ---- */
typedef struct {int api,bits,comp,topdown,W,H,sx,sy,sw,sh,kx,ky,dx,dy,clip,ss,cl,usage; RECT cr;} tc; /* api 0 StretchDIBits, 1 SetDIBitsToDevice; sy counted from the bottom; dx,dy: where the rectangle lands */
static BYTE *pic,*sbuf; static BITMAPINFO *sbi; static DWORD pal8[256]; /* the colour table of the 8-bit cases (0x00RRGGBB) */
#define SBI_SIZE (sizeof(BITMAPINFOHEADER)+1024+16)
static DWORD *bufA,*bufB;
static int stride_of(int W,int bits) {return ((W*bits+31)/32)*4;}
static void mkcase(tc *c,const cls_t *k) {
 int dw,dh,kk;
 c->bits=k->kind==5?8:k->kind==0||k->kind==1?16:k->kind==2?24:k->kind==3?((rnd()&1)?24:32):32; c->usage=(c->bits==8&&rnd()%5==0)?1:0;
 if(c->bits==8) {int i; for(i=0;i<256;i++) pal8[i]=rnd()&0xFFFFFFUL;} c->comp=k->kind==1?BI_BITFIELDS:BI_RGB;
 c->W=1+(int)(rnd()%40); c->H=1+(int)(rnd()%30);
 c->sw=1+(int)(rnd()%(DWORD)c->W); c->sh=1+(int)(rnd()%(DWORD)c->H); c->sx=(int)(rnd()%(DWORD)(c->W-c->sw+1)); c->sy=(int)(rnd()%(DWORD)(c->H-c->sh+1));
 c->topdown=(int)(rnd()&1);
 if(c->topdown&&rnd()%10<7) {c->sh=c->H; c->sy=0;} /* the driver takes a top-down bitmap only whole; the rest must stay with DIBENG */
 if(k->shape==2||(k->shape==3&&rnd()%4==0)) {c->kx=1; c->ky=1;}
 else if(k->shape==1) {kk=2+(int)(rnd()%3); if(rnd()&1) {c->kx=1; c->ky=kk;} else {c->kx=kk; c->ky=1;}}
 else {c->kx=2+(int)(rnd()%3); c->ky=2+(int)(rnd()%3);}
 c->api=(c->kx==1&&c->ky==1&&rnd()%10<6)?1:0;
 c->ss=0; c->cl=c->H;
 if(c->api==1&&!c->topdown&&rnd()%10<4) { /* a partial bitmap: only the scan lines ss..ss+cl-1 (counted from the bottom) are in the buffer; they cover the source rectangle */
  int lo=c->sy,hi=c->sy+c->sh; /* needed scans [lo,hi) */
  c->ss=lo-(int)(rnd()%(DWORD)(lo+1)); c->cl=hi+(int)(rnd()%(DWORD)(c->H-hi+1))-c->ss;
 }
 dw=c->sw*c->kx; dh=c->sh*c->ky;
 if(rnd()%100<70) {c->dx=(int)(rnd()%(DWORD)(640-dw+1)); c->dy=(int)(rnd()%(DWORD)(480-dh+1));}
 else {c->dx=(int)(rnd()%640UL); c->dy=(int)(rnd()%480UL);} /* may run off the right or bottom edge */
 if(rnd()%10==0) c->dx=640-dw/2-(int)(rnd()%3);
 if(rnd()%10==0) c->dy=480-dh/2-(int)(rnd()%3);
 if(rnd()%100<20) { /* an origin that is negative: part of the picture is left of or above the screen */
  if(rnd()&1) c->dx=-(int)(rnd()%(DWORD)dw); else c->dy=-(int)(rnd()%(DWORD)dh);
 }
 if(c->api==1) {if(c->dx<0) c->dx=0; if(c->dy<0) c->dy=0;} /* SetDIBitsToDevice takes a destination on the screen; the source rectangle gives the negative DDI origin by itself */
 { DWORD q=rnd()%10; c->clip=q<5?0:q<8?1:2; }
 { int x0=c->dx<0?0:c->dx,y0=c->dy<0?0:c->dy,l=x0+(int)(rnd()%(DWORD)(dw/2+1)),t=y0+(int)(rnd()%(DWORD)(dh/2+1));
   SetRect(&c->cr,l,t,l+1+(int)(rnd()%(DWORD)(dw+1)),t+1+(int)(rnd()%(DWORD)(dh+1))); }
 { long n=(long)stride_of(c->W,c->bits)*c->H,i; for(i=0;i<n;i++) pic[i]=(BYTE)(rnd()>>3); }
 /* the buffer handed to the call */
 if(c->ss==0&&c->cl==c->H) memcpy(sbuf,pic,(size_t)stride_of(c->W,c->bits)*c->H);
 else { /* bottom-up bitmap: memory line m is scan line m */
  int m,stride=stride_of(c->W,c->bits);
  memset(sbuf,0xC3,(size_t)stride*c->H);
  for(m=0;m<c->cl;m++) memcpy(sbuf+(long)m*stride,pic+(long)(c->ss+m)*stride,(size_t)stride);
 }
}
static int draw(const tc *c,int mirrorx,int mirrory) {
 HRGN rg=NULL; int r,dw=c->sw*c->kx,dh=c->sh*c->ky;
 memset(sbi,0,SBI_SIZE); sbi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER); sbi->bmiHeader.biWidth=c->W; sbi->bmiHeader.biHeight=c->topdown?-c->H:c->H;
 sbi->bmiHeader.biPlanes=1; sbi->bmiHeader.biBitCount=(WORD)c->bits; sbi->bmiHeader.biCompression=c->comp;
 if(c->comp==BI_BITFIELDS) {DWORD *m=(DWORD*)sbi->bmiColors; m[0]=0xF800; m[1]=0x07E0; m[2]=0x001F;}
 if(c->bits==8) {int i; if(c->usage) {WORD *w=(WORD*)sbi->bmiColors; for(i=0;i<256;i++) w[i]=(WORD)(i%20);} else {DWORD *d=(DWORD*)sbi->bmiColors; for(i=0;i<256;i++) d[i]=pal8[i];}}
 if(c->clip==1) {rg=CreateRectRgn(c->cr.left,c->cr.top,c->cr.right,c->cr.bottom); SelectClipRgn(sdc,rg);}
 else if(c->clip==2) {rg=CreateEllipticRgn(c->cr.left,c->cr.top,c->cr.right,c->cr.bottom); SelectClipRgn(sdc,rg);}
 if(c->api==1) r=SetDIBitsToDevice(sdc,c->dx,c->dy,(DWORD)c->sw,(DWORD)c->sh,c->sx,c->sy,(UINT)c->ss,(UINT)c->cl,sbuf,sbi,c->usage?DIB_PAL_COLORS:DIB_RGB_COLORS);
 else r=StretchDIBits(sdc,mirrorx?c->dx+dw-1:c->dx,mirrory?c->dy+dh-1:c->dy,mirrorx?-dw:dw,mirrory?-dh:dh,c->sx,c->sy,c->sw,c->sh,sbuf,sbi,c->usage?DIB_PAL_COLORS:DIB_RGB_COLORS,SRCCOPY);
 GdiFlush();
 if(rg) {SelectClipRgn(sdc,NULL); DeleteObject(rg);}
 return r;
}
/* the pixel DIBENG is expected to put at screen position X,Y (the model of the measurements; the comparison of the two screen images is what decides) */
static DWORD model(const tc *c,int dbpp,int X,int Y) {
 int i=X-c->dx,j=Y-c->dy,col=c->sx+i/c->kx,ty=(c->H-c->sy-c->sh)+j/c->ky,row=c->topdown?ty:(c->H-1-ty),one=(c->kx==1&&c->ky==1);
 const BYTE *p=pic+(long)row*stride_of(c->W,c->bits)+col*(c->bits/8);
 if(c->bits==8) {
  DWORD q; if(c->usage) return 0; /* index colours: only the comparison with DIBENG decides */
  q=pal8[p[0]]; return dbpp==16?(DWORD)((((q>>19)&31)<<11)|(((q>>10)&63)<<5)|((q>>3)&31)):(q&0xFFFFFFUL);
 }
 if(c->bits==16) {
  int v=p[0]|(p[1]<<8),r,g,b;
  if(dbpp==16) {r=(v>>10)&31; g=(v>>5)&31; b=v&31; return (DWORD)((r<<11)|((((g<<1)|(one?0:(g&1)))&63)<<5)|b);}
  if(c->comp==BI_BITFIELDS) {r=v>>11; g=(v>>5)&63; b=v&31; r=(r<<3)|((r&1)?7:0); g=(g<<2)|((g&1)?3:0); b=(b<<3)|((b&1)?7:0);}
  else {r=(v>>10)&31; g=(v>>5)&31; b=v&31; r=(r<<3)|((r&1)?7:0); g=(g<<3)|((g&1)?7:0); b=(b<<3)|((b&1)?7:0);}
  return ((DWORD)r<<16)|((DWORD)g<<8)|(DWORD)b;
 }
 if(dbpp==32) return ((DWORD)p[2]<<16)|((DWORD)p[1]<<8)|p[0];
 return (DWORD)(((p[2]>>3)<<11)|((p[1]>>2)<<5)|(p[0]>>3));
}
static const char *apin[2]={"StretchDIBits","SetDIBitsToDevice"};
static void desc(char *s,const tc *c) {
 sprintf(s,"%s %d-bit%s %dx%d topdown %d W %d H %d src %d,%d %dx%d scans %d+%d dest %d,%d clip %d",apin[c->api],c->bits,c->comp==BI_BITFIELDS?" 565":"",c->kx,c->ky,c->topdown,c->W,c->H,c->sx,c->sy,c->sw,c->sh,c->ss,c->cl,c->dx,c->dy,c->clip);
}
/* one case: DIBENG first (mask 0), then the fast path (mask = the class bit) */
static void runcase(const tc *c,int ci,int mirror) {
 const cls_t *k=&classes[ci]; stat_t *s=&st[ci]; DWORD c0[4],c1[4]; int x0,y0,x1,y1,w,h,rOff,rOn,x,y,bad=0,eligible=0,tdp=0,dw=c->sw*c->kx,dh=c->sh*c->ky; RECT vis; char d[200];
 x0=c->dx-8; y0=c->dy-8; x1=c->dx+dw+8; y1=c->dy+dh+8;
 if(x0<0) x0=0; if(y0<0) y0=0; if(x1>640) x1=640; if(y1>480) y1=480;
 if(x1<=x0||y1<=y0) return;
 w=x1-x0; h=y1-y0;
 SetRect(&vis,c->dx,c->dy,c->dx+dw,c->dy+dh);
 if(vis.left<0) vis.left=0; if(vis.top<0) vis.top=0; if(vis.right>640) vis.right=640; if(vis.bottom>480) vis.bottom=480;
 if(c->clip==1) {if(c->cr.left>vis.left) vis.left=c->cr.left; if(c->cr.top>vis.top) vis.top=c->cr.top; if(c->cr.right<vis.right) vis.right=c->cr.right; if(c->cr.bottom<vis.bottom) vis.bottom=c->cr.bottom;}
 tdp=c->topdown&&!(c->sy==0&&c->sh==c->H); /* a top-down sub-rectangle: left to DIBENG */
 eligible=!mirror&&!tdp&&!c->usage&&vis.right>vis.left&&vis.bottom>vis.top&&c->clip!=2;
 s->cases++;
 setmask(0); clear_screen(); rOff=draw(c,mirror&1,mirror>>1); shot(x0,y0,w,h);
 for(y=0;y<h;y++) for(x=0;x<w;x++) bufA[y*640+x]=px(x,y);
 setmask(mirror?0xFFFFFFUL:(DWORD)k->bit); counters(c0); clear_screen(); rOn=draw(c,mirror&1,mirror>>1); counters(c1); shot(x0,y0,w,h);
 for(y=0;y<h;y++) for(x=0;x<w;x++) bufB[y*640+x]=px(x,y);
 for(y=0;y<h;y++) for(x=0;x<w;x++) {
  if(bufA[y*640+x]!=bufB[y*640+x]) {
   bad++;
   if(s->shown<4) {
    s->shown++; desc(d,c);
    logline("      MISMATCH %s at %d,%d: DIBENG %08lX, fast path %08lX, model %08lX\n",d,x0+x,y0+y,(unsigned long)bufA[y*640+x],(unsigned long)bufB[y*640+x],(unsigned long)(eligible?model(c,k->dbpp,x0+x,y0+y):0));
   }
  }
 }
 if(eligible) {
  int a=0,on=0,n=0;
  for(y=vis.top;y<vis.bottom;y++) for(x=vis.left;x<vis.right;x++) {
   DWORD m=model(c,k->dbpp,x,y); int yy=y-y0,xx=x-x0; n++;
   if(bufA[yy*640+xx]==m) a++; if(bufB[yy*640+xx]==m) on++;
  }
  s->modelok+=a; s->onmodel+=on; s->vis+=n; s->eligible++;
  if(c1[0]>c0[0]) {s->engaged++; s->ops+=c1[0]-c0[0]; s->pixels+=c1[2]-c0[2];}
  else {
   s->missed++;
   if(s->missed<=4) {desc(d,c); logline("      NOT DRAWN by the fast path (declined %lu): %s\n",c1[1]-c0[1],d);}
  }
 } else if(mirror||tdp) {
  if(c1[0]>c0[0]) s->declined_bad++; else s->declined_ok++;
 }
 if(bad) s->mism++;
 if(rOff!=rOn) {
  s->retmis++;
  if(s->retmis<=4) {desc(d,c); logline("      RETURN VALUE differs: DIBENG %d, fast path %d (%s)\n",rOff,rOn,d);}
 }
}
static int run_class(int ci) {
 const cls_t *k=&classes[ci]; stat_t *x=&st[ci]; int n,ok;
 logline("-- class %d (%dbpp screen): %s source, %s --\n",k->bit,k->dbpp,kname[k->kind],shname[k->shape]);
 for(n=0;n<PERCLASS&&!aborted;n++) {tc c; mkcase(&c,k); runcase(&c,ci,0); pump();}
 if(k->shape!=2) for(n=0;n<6&&!aborted;n++) {tc c; mkcase(&c,k); c.api=0; c.ss=0; c.cl=c.H; memcpy(sbuf,pic,(size_t)stride_of(c.W,c.bits)*c.H); runcase(&c,ci,1+(n%3)); pump();} /* mirrored destinations stay with DIBENG */
 ok=x->cases>0&&!x->mism&&!x->retmis&&!x->missed&&x->engaged>0&&!x->declined_bad;
 logline("  class %d: %s: %d cases, %d eligible, drawn by the fast path %d (%lu conversions, %lu pixels), not drawn %d; images differing %d, return values differing %d; mirrored / top-down partial left to DIBENG %d, wrongly taken %d\n",
  k->bit,ok?"PASS":"FAIL",x->cases,x->eligible,x->engaged,x->ops,x->pixels,x->missed,x->mism,x->retmis,x->declined_ok,x->declined_bad);
 logline("           against the model of the measurements: DIBENG %d / %d pixels, fast path %d / %d\n",x->modelok,x->vis,x->onmodel,x->vis);
 return ok;
}
/* ---- timing ---- */
static double tm(int api,int bits,int comp,int sw,int sh,int dw,int dh,int topdown,DWORD mask) {
 BITMAPINFO *b=sbi; int i; double t0; long n,k;
 memset(b,0,SBI_SIZE); b->bmiHeader.biSize=sizeof(BITMAPINFOHEADER); b->bmiHeader.biWidth=sw; b->bmiHeader.biHeight=topdown?-sh:sh; b->bmiHeader.biPlanes=1; b->bmiHeader.biBitCount=(WORD)bits; b->bmiHeader.biCompression=comp;
 if(comp==BI_BITFIELDS) {DWORD *m=(DWORD*)b->bmiColors; m[0]=0xF800; m[1]=0x07E0; m[2]=0x001F;}
 if(bits==8) {DWORD *d=(DWORD*)b->bmiColors; for(k=0;k<256;k++) d[k]=(DWORD)k*0x010203UL;}
 n=(long)stride_of(sw,bits)*sh; for(k=0;k<n;k++) sbuf[k]=(BYTE)(k*7+(k>>9));
 setmask(mask);
#define CALL() (api==1?SetDIBitsToDevice(sdc,0,0,sw,sh,0,0,0,sh,sbuf,b,DIB_RGB_COLORS):StretchDIBits(sdc,0,0,dw,dh,0,0,sw,sh,sbuf,b,DIB_RGB_COLORS,SRCCOPY))
 CALL(); GdiFlush();
 t0=now(); for(i=0;i<10;i++) CALL(); GdiFlush();
 return (now()-t0)/10.0;
}
/* does the class pay? representative 640x480 calls of the class, DIBENG (mask 0) against the fast path (the class bit); the totals decide: enabled only when the fast path is not slower (5% noise allowed) */
static int gate(int ci) {
 const cls_t *k=&classes[ci]; int sb[2],sc[2],ns=0,i,j; double a=0,b=0;
 static const int geo[3][2][6]={ /* api, sw, sh, dw, dh, topdown */
  {{0,320,240,640,480,1},{0,214,160,642,480,1}},   /* both factors >= 2 */
  {{0,640,240,640,480,1},{0,213,480,639,480,1}},   /* a factor 1 on one axis */
  {{1,640,480,640,480,0},{0,640,480,640,480,1}}};  /* 1:1: SetDIBitsToDevice bottom-up, StretchDIBits top-down */
 if(k->kind==0) {sb[0]=16; sc[0]=BI_RGB; ns=1;}
 else if(k->kind==1) {sb[0]=16; sc[0]=BI_BITFIELDS; ns=1;}
 else if(k->kind==2) {sb[0]=24; sc[0]=BI_RGB; ns=1;}
 else if(k->kind==3) {sb[0]=24; sc[0]=BI_RGB; sb[1]=32; sc[1]=BI_RGB; ns=2;}
 else if(k->kind==5) {sb[0]=8; sc[0]=BI_RGB; ns=1;}
 else {sb[0]=32; sc[0]=BI_RGB; ns=1;}
 for(i=0;i<ns;i++) for(j=0;j<2;j++) {
  const int *g=geo[k->shape][j];
  a+=tm(g[0],sb[i],sc[i],g[1],g[2],g[3],g[4],g[5],0); b+=tm(g[0],sb[i],sc[i],g[1],g[2],g[3],g[4],g[5],(DWORD)k->bit);
 }
 setmask(0);
 logline("  class %d timing: DIBENG %.2f ms, fast path %.2f ms (sum of %d representative 640x480 calls): %s\n",k->bit,a,b,ns*2,b<=a*1.05?"pays, enabled":"NOT FASTER: left off");
 return b<=a*1.05;
}
static void timing(int dbpp,DWORD mask) {
 static const struct {const char *n; int bits,comp;} src[5]={{"555 BI_RGB",16,BI_RGB},{"565 BITFIELDS",16,BI_BITFIELDS},{"24-bit",24,BI_RGB},{"32-bit",32,BI_RGB},{"8-bit palette",8,BI_RGB}};
 int i,f;
 logline("-- timing into the 640x480x%d screen, ms per call, DIBENG / fast path (classes %lu on): SetDIBitsToDevice bottom-up 640x480, StretchDIBits 1:1 top-down, 2x, 3x --\n",dbpp,mask);
 for(i=0;i<5&&!aborted;i++) {
  double a[4],b[4];
  if(dbpp==32&&i==3) continue;
  if(dbpp==16&&i==1) continue;
  a[0]=tm(1,src[i].bits,src[i].comp,640,480,640,480,0,0); b[0]=tm(1,src[i].bits,src[i].comp,640,480,640,480,0,mask);
  a[1]=tm(0,src[i].bits,src[i].comp,640,480,640,480,1,0); b[1]=tm(0,src[i].bits,src[i].comp,640,480,640,480,1,mask);
  f=2; a[2]=tm(0,src[i].bits,src[i].comp,640/f,480/f,640,480,1,0); b[2]=tm(0,src[i].bits,src[i].comp,640/f,480/f,640,480,1,mask);
  f=3; a[3]=tm(0,src[i].bits,src[i].comp,214,160,642,480,1,0); b[3]=tm(0,src[i].bits,src[i].comp,214,160,642,480,1,mask);
  logline("    %-14s SetDIBits %6.2f / %6.2f   1:1 %6.2f / %6.2f   2x %6.2f / %6.2f   3x %6.2f / %6.2f\n",src[i].n,a[0],b[0],a[1],b[1],a[2],b[2],a[3],b[3]);
  pump();
 }
 setmask(0);
}
/* ---- probe: what the driver is called with for a top-down bitmap and SetDIBitsToDevice with StartScan / cLines (the fast path is off) ---- */
#define PW 21
#define PH 13
static void probe(void) {
 static const struct {const char *n; int ys,hs,ss,cl;} p[]={{"whole",0,PH,0,PH},{"rectangle ys 2 h 7",2,7,0,PH},{"partial: ys 4 h 5, StartScan 4, cLines 5",4,5,4,5},{"partial: ys 3 h 4, StartScan 2, cLines 8",3,4,2,8}};
 int i,pass;
 logline("-- probe: SetDIBitsToDevice of a top-down 24-bit bitmap %dx%d at 100,60 (16bpp screen), first with the fast path off (DIBENG), then with class 32 on; which source rows land where --\n",PW,PH);
 for(pass=0;pass<2;pass++) for(i=0;i<4&&!aborted;i++) {
  BITMAPINFOHEADER bh; int x,y,stride=stride_of(PW,24),line,found=0,fx=0,fy=0,n=0,r; vxh_record *rec=NULL; DWORD j; int minx=9999,miny=9999,maxx=-1,maxy=-1; DWORD bg;
  memset(sbuf,0,(size_t)stride*PH);
  for(line=0;line<p[i].cl;line++) { /* buffer line m holds the bitmap row ss+m counted from the top; its colour: row r, column x -> (r*PW+x) in 5-bit steps */
   int row=p[i].ss+line; for(x=0;x<PW;x++) {int idx=row*PW+x; BYTE *q=sbuf+(long)line*stride+x*3; q[0]=(BYTE)(((idx*7+3)&31)<<3); q[1]=(BYTE)(((idx>>5)&31)<<3); q[2]=(BYTE)((idx&31)<<3);}
  }
  memset(&bh,0,sizeof(bh)); bh.biSize=sizeof(bh); bh.biWidth=PW; bh.biHeight=-PH; bh.biPlanes=1; bh.biBitCount=24; bh.biCompression=BI_RGB;
  setmask(pass?32UL:0UL); {RECT rc; HBRUSH hb=CreateSolidBrush(RGB(8,16,24)); SetRect(&rc,0,0,640,480); FillRect(sdc,&rc,hb); DeleteObject(hb); GdiFlush();} hesc(sdc,VXH_ARM_CALLS,NULL); /* a non-black background: black pixels drawn from a buffer of zeros must show */
  r=SetDIBitsToDevice(sdc,100,60,PW,(DWORD)p[i].hs,0,p[i].ys,(UINT)p[i].ss,(UINT)p[i].cl,sbuf,(BITMAPINFO*)&bh,DIB_RGB_COLORS); GdiFlush();
  memset(&rep_,0,sizeof(rep_));
  if(hesc(sdc,VXH_READ,&rep_)>0&&rep_.magic==VXH_MAGIC) {
   for(j=0;j<rep_.count&&j<VXH_CAP;j++) if(rep_.records[j].rop==VXH_TAG_SDIB||rep_.records[j].rop==VXH_TAG_DTDV) {rec=&rep_.records[j]; break;}
  }
  hesc(sdc,VXH_STOP,NULL);
  shot(60,30,120,100);
  bg=px(119,99);
  for(y=0;y<100;y++) for(x=0;x<120;x++) if(px(x,y)!=bg) {n++; if(x<minx) minx=x; if(y<miny) miny=y; if(x>maxx) maxx=x; if(y>maxy) maxy=y; if(!found) {found=1; fx=x; fy=y;}}
  logline("    %s %-44s returned %d; ",pass?"fast path":"DIBENG   ",p[i].n,r);
  if(found) {
   DWORD o=px(fx,fy); int rr=(int)((o>>11)&31),gg=(int)((o>>6)&31),bb=(int)(o&31),idx=-1,t;
   for(t=0;t<PW*PH;t++) if((((t*7+3)&31)==bb)&&(((t>>5)&31)==gg)&&((t&31)==rr)) {idx=t; break;}
   logline("%d pixels, box %d,%d..%d,%d, first pixel = bitmap row %d column %d (top-down)\n",n,minx+60,miny+30,maxx+60,maxy+30,idx<0?-1:idx/PW,idx<0?-1:idx%PW);
  } else logline("nothing drawn\n");
  if(rec) {
   WORD w[20]; memcpy(w,&rec->flags,40);
   logline("      the driver was called as %s; first stack words (lowest address first, last parameter first): %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X\n",rec->rop==VXH_TAG_SDIB?"StretchDIBits":"DibToDevice",
    w[0],w[1],w[2],w[3],w[4],w[5],w[6],w[7],w[8],w[9],w[10],w[11],w[12],w[13],w[14],w[15],w[16],w[17],w[18],w[19]);
  } else logline("      no StretchDIBits or DibToDevice call was recorded (the driver's hook handled it or it went elsewhere)\n");
  pump();
 }
}
/* ---- probe: the arguments GDI hands the driver for an 8-bit bitmap (the fast path is off): the translation table pointer is the 13th parameter, the three words after the ROP dword ---- */
static void probe8(void) {
 int u;
 logline("-- probe: StretchDIBits of an 8-bit 21x13 bitmap, 2x at 100,60 (16bpp screen, fast path off), DIB_RGB_COLORS then DIB_PAL_COLORS; the stack words after 0020 00CC (the ROP) are the translation table pointer (0000 0000 = none), then the info and bits pointers --\n");
 for(u=0;u<2&&!aborted;u++) {
  int i; vxh_record *rec=NULL; DWORD j;
  memset(sbi,0,SBI_SIZE); sbi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER); sbi->bmiHeader.biWidth=21; sbi->bmiHeader.biHeight=13; sbi->bmiHeader.biPlanes=1; sbi->bmiHeader.biBitCount=8; sbi->bmiHeader.biCompression=BI_RGB;
  if(u) {WORD *w=(WORD*)sbi->bmiColors; for(i=0;i<256;i++) w[i]=(WORD)(i%20);} else {DWORD *d=(DWORD*)sbi->bmiColors; for(i=0;i<256;i++) d[i]=(DWORD)i*0x010203UL;}
  memset(sbuf,3,21UL*16UL);
  setmask(0); clear_screen(); hesc(sdc,VXH_ARM_CALLS,NULL);
  StretchDIBits(sdc,100,60,42,26,0,0,21,13,sbuf,sbi,u?DIB_PAL_COLORS:DIB_RGB_COLORS,SRCCOPY); GdiFlush();
  memset(&rep_,0,sizeof(rep_));
  if(hesc(sdc,VXH_READ,&rep_)>0&&rep_.magic==VXH_MAGIC) {for(j=0;j<rep_.count&&j<VXH_CAP;j++) if(rep_.records[j].rop==VXH_TAG_SDIB) {rec=&rep_.records[j]; break;}}
  hesc(sdc,VXH_STOP,NULL);
  if(rec) {
   WORD w[20]; memcpy(w,&rec->flags,40);
   logline("    %s: the driver was called as StretchDIBits; stack words: %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X %04X\n",u?"DIB_PAL_COLORS":"DIB_RGB_COLORS",w[0],w[1],w[2],w[3],w[4],w[5],w[6],w[7],w[8],w[9],w[10],w[11],w[12],w[13],w[14],w[15],w[16],w[17],w[18],w[19]);
  } else logline("    %s: no StretchDIBits call was recorded\n",u?"DIB_PAL_COLORS":"DIB_RGB_COLORS");
  pump();
 }
}
static int mode_ok(int bpp) {
 vx_status_t s; DWORD got=0,t=GetTickCount(); while(GetTickCount()-t<800) Sleep(20);
 memset(&s,0,sizeof(s));
 if(!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||s.width!=640||s.height!=480||(int)s.bpp!=bpp||s.stage!=6||s.failure||s.vga_mode) {logline("the driver is not in native 640x480x%d mode\n",bpp); return 0;}
 return 1;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 DWORD pass=0,tested=0,c[4],orig=0,final; int rc=0,have_orig,ci,phase; DEVMODEA origm; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2STRX.TXT","w"); logline("VX2STRX v2.3 / driver v10.103\n");
 finish("RUNNING: the run did not reach its end (VX2STRX.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 if(!counters(c)) {finish("FAIL: this driver has no conversion control (VX85598.VXD older than v10.99)"); return 1;}
 orig=c[3]; setmask(0xFFFFFFUL); counters(c);
 if(c[3]!=0xFFFFFFUL) {logline("the driver did not take the live class mask (read %lu): VX85598.VXD older than v10.103\n",c[3]); setmask(orig); finish("FAIL: VX85598.VXD older than v10.103"); return 1;}
 setmask(0);
 sbuf=(BYTE*)calloc(640UL*480UL*4UL+8192UL,1); pic=(BYTE*)calloc(640UL*480UL*4UL+8192UL,1); sbi=(BITMAPINFO*)calloc(SBI_SIZE,1); bufA=(DWORD*)calloc(640UL*480UL,4); bufB=(DWORD*)calloc(640UL*480UL,4);
 if(!sbuf||!pic||!sbi||!bufA||!bufB) {finish("FAIL: out of memory"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have_orig=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 logline("desktop was %lux%lux%lu; class mask at start %lu (from ACCEL_STRETCH in the registry)\n",have_orig?origm.dmPelsWidth:0,have_orig?origm.dmPelsHeight:0,have_orig?origm.dmBitsPerPel:0,orig);
 for(phase=0;phase<2&&!aborted;phase++) {
  int dbpp=phase?32:16; DWORD pmask=0,ppass=0;
  for(ci=0;ci<NCLS;ci++) if(classes[ci].dbpp==dbpp) pmask|=(DWORD)classes[ci].bit;
  if(!set_mode(640,480,dbpp)||!mode_ok(dbpp)) {logline("640x480x%d not available: this part is skipped and its classes keep their registry value\n",dbpp); if(!phase) rc=1; continue;}
  sdc=GetDC(NULL);
  if(!sdc||!make_target(dbpp)) {logline("no screen DC or no %d-bit DIB section: this part is skipped\n",dbpp); if(sdc) ReleaseDC(NULL,sdc); continue;}
  tested|=pmask;
  for(ci=0;ci<NCLS&&!aborted;ci++) if(classes[ci].dbpp==dbpp) {if(run_class(ci)&&gate(ci)) ppass|=(DWORD)classes[ci].bit;}
  logline("classes that passed (pixels equal and not slower) at %dbpp: %lu\n",dbpp,ppass);
  pass|=ppass;
  if(!aborted) {if(ppass) timing(dbpp,ppass); else logline("no class passed: timing skipped\n");}
  if(!aborted&&dbpp==16) {probe(); probe8();}
  drop_target(); ReleaseDC(NULL,sdc);
 }
 setmask(orig);
 if(have_orig) {ChangeDisplaySettingsA(&origm,0); logline("desktop put back\n");}
 final=pass|(orig&~tested);
 logline("classes that passed: %lu; ACCEL_STRETCH to write: %lu\n",pass,final);
 if(aborted) {finish("FAIL: aborted; nothing was changed"); logline("aborted\n");}
 else if(rc&&!tested) finish("FAIL: 640x480x16 not available; nothing was changed");
 else if(!pass) finish("FAIL: no class passed; ACCEL_STRETCH not written (see VX2STRX.TXT)");
 else {char m[160]; sprintf(m,"%s: classes that passed %lu (.533: nothing written, the driver reads no ACCEL_STRETCH)",pass==tested?"PASS":"PARTIAL",final); finish(m);}
 logline("Finished.\n"); if(lf) fclose(lf);
 CloseHandle(dev);
 return 0;
}
