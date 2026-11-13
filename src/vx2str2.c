/* VX2STR2 v1.0: 1-bit, 4-bit and RLE (RLE8, RLE4) DIB sources of driver v10.104 (StretchDIBits and DibToDevice into the 32bpp screen), compared with DIBENG pixel for pixel before they are switched on.
 * The desktop is switched to 640x480x32 (and put back). Every case is drawn twice on the screen: first with the new classes off (live class mask 2 = 0, so DIBENG does it), then with one class on (the VxD converts and
 * draws the bitmap itself). The two screen images, read back through a DIB section of the screen's depth, and the two return values of the call must be identical, and the driver's counters must show that the fast path
 * really drew the case. Random sources (raw 1-bit and 4-bit bitmaps with a random colour table, RLE8 and RLE4 streams made of encoded and absolute runs; bottom-up and top-down), StretchDIBits with factors 1..4 on each axis,
 * SetDIBitsToDevice with source rectangles and partial bitmaps, origins that are negative or run off the screen, no clip / a clip rectangle / an elliptic clip region. Mirrored destinations, top-down sub-rectangles and
 * RLE streams that leave pixels undefined (a short row, which DIBENG leaves alone) stay with DIBENG: they are drawn with every class on and the fast path must decline them. A fifth of the raw cases use DIB_PAL_COLORS
 * (index colours): they must come out the same as DIBENG whatever the driver does with them, and the log says how many the fast path drew.
 * Classes (ACCEL_STRETCH2 bits, 32bpp screen; kind * 3 + shape, shape 0 = both factors >= 2, 1 = a factor 1 on one axis, 2 = 1:1): 1-bit 1, 2, 4; 4-bit 8, 16, 32; RLE8 64, 128, 256; RLE4 512, 1024, 2048.
 * Each class passes or fails alone. A class is also timed against DIBENG (representative calls, RLE bitmaps 512x384 because the VxD decodes at most 262144 pixels) and is only enabled when the fast path is not slower.
 * At 16bpp these sources always stay with DIBENG (its palette path is as fast); that is checked, and DIBENG's time for each kind is logged so it can be seen whether any of them matters there.
 * The unused top byte of a 32bpp pixel is ignored in the comparison. The classes that pass are written to HKLM\Software\VX85598 ACCEL_STRETCH2 (restart Windows to use them); a phase that could not run keeps its old bits.
 * Needs driver v10.104 (VX85598.DRV and VX85598.VXD). Unattended: read VX2STR2.TXT (VX2STR2.RES says RUNNING until it ends). Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

#define PERCLASS 56
static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {
 va_list a;
 if(!lf) return;
 va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));
}
static void finish(const char *res) {FILE *f=fopen("VX2STR2.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
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
/* driver control: 0xFFFFFE13 reads out[0] conversions drawn, out[1] declined, out[2] pixels, out[3] class mask 1; 0xFFFFFE14 reads out[0] RLE bitmaps decoded, out[1] RLE streams refused by the decoder, out[2] RLE pixels, out[3] class mask 2;
 * 0xFA000000|m sets the live class mask 2, 0xFB000000|m the live class mask 1 */
static int ctl(DWORD code,DWORD *out) {
 DWORD o[4],got=0; memset(o,0,sizeof(o));
 if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0;
 if(out) memcpy(out,o,16);
 return 1;
}
static int counters(DWORD *c) {return ctl(0xFFFFFE13UL,c);}
static int counters2(DWORD *c) {return ctl(0xFFFFFE14UL,c);}
static void setmask1(DWORD m) {ctl(0xFB000000UL|(m&0xFFFFFFUL),NULL);}
static void setmask2(DWORD m) {ctl(0xFA000000UL|(m&0xFFFFFFUL),NULL);}

static HDC sdc,mdc; static HBITMAP dib,olddib; static BYTE *tgt; static int tbits;
static int make_target(int bits) {
 BITMAPINFOHEADER b; void *p=NULL;
 memset(&b,0,sizeof(b)); b.biSize=sizeof(BITMAPINFOHEADER); b.biWidth=640; b.biHeight=-480; b.biPlanes=1; b.biBitCount=(WORD)bits;
 dib=CreateDIBSection(sdc,(BITMAPINFO*)&b,DIB_RGB_COLORS,&p,NULL,0);
 if(!dib||!p) return 0;
 mdc=CreateCompatibleDC(sdc); olddib=(HBITMAP)SelectObject(mdc,dib); tgt=(BYTE*)p; tbits=bits;
 return 1;
}
static void drop_target(void) {SelectObject(mdc,olddib); DeleteObject(dib); DeleteDC(mdc); dib=NULL; mdc=NULL;}
static DWORD px(int x,int y) {return ((DWORD*)tgt)[y*640+x]&0x00FFFFFFUL;}
static void shot(int x,int y,int w,int h) {BitBlt(mdc,0,0,w,h,sdc,x,y,SRCCOPY); GdiFlush();}
static void clear_screen(void) {
 RECT rc; HBRUSH hb=CreateSolidBrush(RGB(8,16,24));
 rc.left=0; rc.top=0; rc.right=640; rc.bottom=480; FillRect(sdc,&rc,hb); DeleteObject(hb); GdiFlush();
}
static DWORD seed=20261022UL;
static DWORD rnd(void) {seed=seed*1103515245UL+12345UL; return (seed>>8)&0xFFFFFFUL;}

/* ---- classes ---- */
typedef struct {int bit,kind,shape;} cls_t; /* kind 0 1-bit, 1 4-bit, 2 RLE8, 3 RLE4; shape 0 both factors >= 2, 1 one factor 1, 2 1:1 */
static const cls_t classes[]={{1,0,0},{2,0,1},{4,0,2},{8,1,0},{16,1,1},{32,1,2},{64,2,0},{128,2,1},{256,2,2},{512,3,0},{1024,3,1},{2048,3,2}};
static const int kbits[4]={1,4,8,4};
static const int kcomp[4]={BI_RGB,BI_RGB,BI_RLE8,BI_RLE4};
static const char *kname[4]={"1-bit","4-bit","RLE8","RLE4"};
static const char *shname[3]={"both factors >= 2","a factor 1 on one axis","1:1"};
#define NCLS 12
typedef struct {int cases,eligible,engaged,missed,mism,retmis,declined_ok,declined_bad,modelok,vis,onmodel,ucases,uengaged,refused; DWORD ops,pixels; int shown;} stat_t;
static stat_t st[NCLS];

/* ---- cases ---- */
typedef struct {int api,bits,comp,kind,topdown,W,H,sx,sy,sw,sh,kx,ky,dx,dy,clip,ss,cl,usage,cover,slen; RECT cr;} tc; /* api 0 StretchDIBits, 1 SetDIBitsToDevice; sy counted from the bottom; dx,dy: where the rectangle lands */
static BYTE *pic,*sbuf,*dec,*defd; static BITMAPINFO *sbi; static DWORD palx[256]; /* the colour table of the case (0x00RRGGBB) */
#define SBI_SIZE (sizeof(BITMAPINFOHEADER)+1024+16)
static DWORD *bufA,*bufB;
static int stride_of(int W,int bits) {return ((W*bits+31)/32)*4;}
/* a random RLE stream for a W x H bitmap: encoded runs and absolute runs, an end of line after each row, an end of bitmap at the end. partial: one row is cut short (its end of line comes early), which leaves pixels undefined */
static int gen_rle(BYTE *o,int W,int H,int bits,int partial) {
 int n=0,y,x,left,cnt,k,np=bits==8?256:16,cutrow=partial?(int)(rnd()%(DWORD)H):-1,limit;
 for(y=0;y<H;y++) {
  x=0; limit=(y==cutrow&&W>1)?1+(int)(rnd()%(DWORD)(W-1)):W;
  while(x<limit) {
   left=limit-x;
   if(left>=3&&rnd()%10<4) { /* an absolute run of cnt >= 3 pixels */
    cnt=3+(int)(rnd()%(DWORD)(left-2)); if(cnt>255) cnt=255;
    o[n++]=0; o[n++]=(BYTE)cnt;
    if(bits==8) {for(k=0;k<cnt;k++) o[n++]=(BYTE)(rnd()%256); if(cnt&1) o[n++]=0;}
    else {int nb=(cnt+1)/2; for(k=0;k<nb;k++) o[n++]=(BYTE)(((rnd()%16)<<4)|(rnd()%16)); if(nb&1) o[n++]=0;}
   } else {
    cnt=1+(int)(rnd()%(DWORD)(left<255?left:255));
    o[n++]=(BYTE)cnt; o[n++]=bits==8?(BYTE)(rnd()%(DWORD)np):(BYTE)(((rnd()%16)<<4)|(rnd()%16));
   }
   x+=cnt;
  }
  o[n++]=0; o[n++]=(BYTE)(y==H-1?1:0);
 }
 return n;
}
/* the reference decoder of the test (written from the format description): dec[row*W+col] = the colour index, row 0 = the bottom line. Returns 1 when every pixel was defined */
static int rle_dec(const BYTE *s,int n,int W,int H,int bits) {
 int i=0,x=0,y=0,k,a,b,def=0; BYTE *d=dec;
 memset(dec,0,(size_t)W*H); memset(defd,0,(size_t)W*H);
 while(i+1<n) {
  a=s[i]; b=s[i+1]; i+=2;
  if(a) {for(k=0;k<a;k++,x++) if(x<W&&y<H) {if(!defd[y*W+x]) def++; defd[y*W+x]=1; d[y*W+x]=(BYTE)(bits==8?b:(k&1?b&15:b>>4));}}
  else if(b==0) {x=0; y++;}
  else if(b==1) break;
  else if(b==2) {x+=s[i]; y+=s[i+1]; i+=2;}
  else {
   int nb=bits==8?b:(b+1)/2;
   for(k=0;k<b;k++,x++) if(x<W&&y<H) {int v=bits==8?s[i+k]:(k&1?s[i+k/2]&15:s[i+k/2]>>4); if(!defd[y*W+x]) def++; defd[y*W+x]=1; d[y*W+x]=(BYTE)v;}
   i+=(nb+1)&~1;
  }
 }
 return def==W*H;
}
static void mkcase(tc *c,const cls_t *k) {
 int dw,dh,kk,rle=k->kind>=2,i;
 c->kind=k->kind; c->bits=kbits[k->kind]; c->comp=kcomp[k->kind]; c->usage=(!rle&&rnd()%5==0)?1:0; c->cover=1; c->slen=0;
 for(i=0;i<256;i++) palx[i]=rnd()&0xFFFFFFUL;
 c->W=1+(int)(rnd()%40); c->H=1+(int)(rnd()%30);
 c->sw=1+(int)(rnd()%(DWORD)c->W); c->sh=1+(int)(rnd()%(DWORD)c->H); c->sx=(int)(rnd()%(DWORD)(c->W-c->sw+1)); c->sy=(int)(rnd()%(DWORD)(c->H-c->sh+1));
 c->topdown=rle?0:(int)(rnd()&1);
 if(c->topdown&&rnd()%10<7) {c->sh=c->H; c->sy=0;} /* the driver takes a top-down bitmap only whole; the rest must stay with DIBENG */
 if(k->shape==2) {c->kx=1; c->ky=1;}
 else if(k->shape==1) {kk=2+(int)(rnd()%3); if(rnd()&1) {c->kx=1; c->ky=kk;} else {c->kx=kk; c->ky=1;}}
 else {c->kx=2+(int)(rnd()%3); c->ky=2+(int)(rnd()%3);}
 c->api=(c->kx==1&&c->ky==1&&rnd()%10<6)?1:0;
 c->ss=0; c->cl=c->H;
 if(c->api==1&&!c->topdown&&!rle&&rnd()%10<4) { /* a partial bitmap: only the scan lines ss..ss+cl-1 (counted from the bottom) are in the buffer; they cover the source rectangle */
  int lo=c->sy,hi=c->sy+c->sh;
  c->ss=lo-(int)(rnd()%(DWORD)(lo+1)); c->cl=hi+(int)(rnd()%(DWORD)(c->H-hi+1))-c->ss;
 }
 dw=c->sw*c->kx; dh=c->sh*c->ky;
 if(rnd()%100<70) {c->dx=(int)(rnd()%(DWORD)(640-dw+1)); c->dy=(int)(rnd()%(DWORD)(480-dh+1));}
 else {c->dx=(int)(rnd()%640UL); c->dy=(int)(rnd()%480UL);}
 if(rnd()%10==0) c->dx=640-dw/2-(int)(rnd()%3);
 if(rnd()%10==0) c->dy=480-dh/2-(int)(rnd()%3);
 if(rnd()%100<20) {if(rnd()&1) c->dx=-(int)(rnd()%(DWORD)dw); else c->dy=-(int)(rnd()%(DWORD)dh);}
 if(c->api==1) {if(c->dx<0) c->dx=0; if(c->dy<0) c->dy=0;}
 { DWORD q=rnd()%10; c->clip=q<5?0:q<8?1:2; }
 { int x0=c->dx<0?0:c->dx,y0=c->dy<0?0:c->dy,l=x0+(int)(rnd()%(DWORD)(dw/2+1)),t=y0+(int)(rnd()%(DWORD)(dh/2+1));
   SetRect(&c->cr,l,t,l+1+(int)(rnd()%(DWORD)(dw+1)),t+1+(int)(rnd()%(DWORD)(dh+1))); }
 if(rle) { /* the stream is the picture; a quarter of the RLE cases leave pixels undefined */
  c->slen=gen_rle(pic,c->W,c->H,c->bits,rnd()%4==0&&c->H*c->W>1);
  c->cover=rle_dec(pic,c->slen,c->W,c->H,c->bits);
  memcpy(sbuf,pic,(size_t)c->slen);
 } else {
  long n=(long)stride_of(c->W,c->bits)*c->H,j; for(j=0;j<n;j++) pic[j]=(BYTE)(rnd()>>3);
  if(c->ss==0&&c->cl==c->H) memcpy(sbuf,pic,(size_t)n);
  else { /* bottom-up bitmap: memory line m is scan line m */
   int m,stride=stride_of(c->W,c->bits);
   memset(sbuf,0xC3,(size_t)stride*c->H);
   for(m=0;m<c->cl;m++) memcpy(sbuf+(long)m*stride,pic+(long)(c->ss+m)*stride,(size_t)stride);
  }
 }
}
static void fill_info(const tc *c) {
 int i,ne=c->bits==8?256:(1<<c->bits);
 memset(sbi,0,SBI_SIZE); sbi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER); sbi->bmiHeader.biWidth=c->W; sbi->bmiHeader.biHeight=c->topdown?-c->H:c->H;
 sbi->bmiHeader.biPlanes=1; sbi->bmiHeader.biBitCount=(WORD)c->bits; sbi->bmiHeader.biCompression=c->comp; sbi->bmiHeader.biSizeImage=(DWORD)c->slen;
 if(c->usage) {WORD *w=(WORD*)sbi->bmiColors; for(i=0;i<ne;i++) w[i]=(WORD)(i%20);} else {DWORD *d=(DWORD*)sbi->bmiColors; for(i=0;i<ne;i++) d[i]=palx[i];}
}
static int draw(const tc *c,int mirrorx,int mirrory) {
 HRGN rg=NULL; int r,dw=c->sw*c->kx,dh=c->sh*c->ky;
 fill_info(c);
 if(c->clip==1) {rg=CreateRectRgn(c->cr.left,c->cr.top,c->cr.right,c->cr.bottom); SelectClipRgn(sdc,rg);}
 else if(c->clip==2) {rg=CreateEllipticRgn(c->cr.left,c->cr.top,c->cr.right,c->cr.bottom); SelectClipRgn(sdc,rg);}
 if(c->api==1) r=SetDIBitsToDevice(sdc,c->dx,c->dy,(DWORD)c->sw,(DWORD)c->sh,c->sx,c->sy,(UINT)c->ss,(UINT)c->cl,sbuf,sbi,c->usage?DIB_PAL_COLORS:DIB_RGB_COLORS);
 else r=StretchDIBits(sdc,mirrorx?c->dx+dw-1:c->dx,mirrory?c->dy+dh-1:c->dy,mirrorx?-dw:dw,mirrory?-dh:dh,c->sx,c->sy,c->sw,c->sh,sbuf,sbi,c->usage?DIB_PAL_COLORS:DIB_RGB_COLORS,SRCCOPY);
 GdiFlush();
 if(rg) {SelectClipRgn(sdc,NULL); DeleteObject(rg);}
 return r;
}
static int idxat(const tc *c,int row,int col) {
 if(c->comp!=BI_RGB) return dec[row*c->W+col];
 if(c->bits==4) {BYTE b=pic[(long)row*stride_of(c->W,4)+col/2]; return (col&1)?(b&15):(b>>4);}
 return (pic[(long)row*stride_of(c->W,1)+col/8]>>(7-col%8))&1;
}
/* the pixel DIBENG is expected to put at screen position X,Y: the palette entry of the source pixel (the comparison of the two screen images is what decides) */
static DWORD model(const tc *c,int X,int Y) {
 int i=X-c->dx,j=Y-c->dy,col=c->sx+i/c->kx,ty=(c->H-c->sy-c->sh)+j/c->ky,row=c->topdown?ty:(c->H-1-ty);
 return palx[idxat(c,row,col)]&0xFFFFFFUL;
}
static const char *apin[2]={"StretchDIBits","SetDIBitsToDevice"};
static void desc(char *s,const tc *c) {
 sprintf(s,"%s %s%s %dx%d topdown %d W %d H %d src %d,%d %dx%d scans %d+%d dest %d,%d clip %d%s",apin[c->api],kname[c->kind],c->usage?" (index colours)":"",c->kx,c->ky,c->topdown,c->W,c->H,c->sx,c->sy,c->sw,c->sh,c->ss,c->cl,c->dx,c->dy,c->clip,c->cover?"":" (stream leaves pixels undefined)");
}
/* one case: DIBENG first (mask 2 = 0), then the fast path (mask 2 = the class bit; every class for the cases that must stay with DIBENG) */
static void runcase(const tc *c,int ci,int mirror) {
 const cls_t *k=&classes[ci]; stat_t *s=&st[ci]; DWORD c0[4],c1[4],r0[4],r1[4]; int x0,y0,x1,y1,w,h,rOff,rOn,x,y,bad=0,eligible=0,tdp=0,mustdecline,dw=c->sw*c->kx,dh=c->sh*c->ky; RECT vis; char d[220];
 x0=c->dx-8; y0=c->dy-8; x1=c->dx+dw+8; y1=c->dy+dh+8;
 if(x0<0) x0=0; if(y0<0) y0=0; if(x1>640) x1=640; if(y1>480) y1=480;
 if(x1<=x0||y1<=y0) return;
 w=x1-x0; h=y1-y0;
 SetRect(&vis,c->dx,c->dy,c->dx+dw,c->dy+dh);
 if(vis.left<0) vis.left=0; if(vis.top<0) vis.top=0; if(vis.right>640) vis.right=640; if(vis.bottom>480) vis.bottom=480;
 if(c->clip==1) {if(c->cr.left>vis.left) vis.left=c->cr.left; if(c->cr.top>vis.top) vis.top=c->cr.top; if(c->cr.right<vis.right) vis.right=c->cr.right; if(c->cr.bottom<vis.bottom) vis.bottom=c->cr.bottom;}
 tdp=c->topdown&&!(c->sy==0&&c->sh==c->H);
 mustdecline=mirror||tdp||!c->cover;
 eligible=!mustdecline&&!c->usage&&vis.right>vis.left&&vis.bottom>vis.top&&c->clip!=2;
 s->cases++;
 setmask2(0); clear_screen(); rOff=draw(c,mirror&1,mirror>>1); shot(x0,y0,w,h);
 for(y=0;y<h;y++) for(x=0;x<w;x++) bufA[y*640+x]=px(x,y);
 setmask2(mustdecline?0xFFFFFFUL:(DWORD)k->bit); counters(c0); counters2(r0); clear_screen(); rOn=draw(c,mirror&1,mirror>>1); counters(c1); counters2(r1); shot(x0,y0,w,h);
 for(y=0;y<h;y++) for(x=0;x<w;x++) bufB[y*640+x]=px(x,y);
 for(y=0;y<h;y++) for(x=0;x<w;x++) {
  if(bufA[y*640+x]!=bufB[y*640+x]) {
   bad++;
   if(s->shown<4) {
    s->shown++; desc(d,c);
    logline("      MISMATCH %s at %d,%d: DIBENG %08lX, fast path %08lX, model %08lX\n",d,x0+x,y0+y,(unsigned long)bufA[y*640+x],(unsigned long)bufB[y*640+x],(unsigned long)(eligible?model(c,x0+x,y0+y):0));
   }
  }
 }
 if(c->usage) {s->ucases++; if(c1[0]>c0[0]) s->uengaged++;}
 if(eligible) {
  int a=0,on=0,n=0;
  for(y=vis.top;y<vis.bottom;y++) for(x=vis.left;x<vis.right;x++) {
   DWORD m=model(c,x,y); int yy=y-y0,xx=x-x0; n++;
   if(bufA[yy*640+xx]==m) a++; if(bufB[yy*640+xx]==m) on++;
  }
  s->modelok+=a; s->onmodel+=on; s->vis+=n; s->eligible++;
  if(c1[0]>c0[0]) {s->engaged++; s->ops+=c1[0]-c0[0]; s->pixels+=c1[2]-c0[2];}
  else {
   s->missed++;
   if(s->missed<=4) {desc(d,c); logline("      NOT DRAWN by the fast path (declined %lu, RLE refused %lu): %s\n",c1[1]-c0[1],r1[1]-r0[1],d);}
  }
 } else if(mustdecline) {
  if(c1[0]>c0[0]) {s->declined_bad++; if(s->declined_bad<=4) {desc(d,c); logline("      WRONGLY TAKEN by the fast path: %s\n",d);}} else s->declined_ok++;
  if(!c->cover&&!mirror&&!tdp&&r1[1]>r0[1]) s->refused++;
 }
 if(bad) s->mism++;
 if(rOff!=rOn) {
  s->retmis++;
  if(s->retmis<=4) {desc(d,c); logline("      RETURN VALUE differs: DIBENG %d, fast path %d (%s)\n",rOff,rOn,d);}
 }
}
static int run_class(int ci) {
 const cls_t *k=&classes[ci]; stat_t *x=&st[ci]; int n,ok;
 logline("-- class %d (32bpp screen): %s source, %s --\n",k->bit,kname[k->kind],shname[k->shape]);
 for(n=0;n<PERCLASS&&!aborted;n++) {tc c; mkcase(&c,k); runcase(&c,ci,0); pump();}
 if(k->shape!=2) for(n=0;n<6&&!aborted;n++) {
  tc c; mkcase(&c,k); c.api=0; c.ss=0; c.cl=c.H;
  if(c.comp==BI_RGB) memcpy(sbuf,pic,(size_t)stride_of(c.W,c.bits)*c.H);
  runcase(&c,ci,1+(n%3)); pump(); /* mirrored destinations stay with DIBENG */
 }
 ok=x->cases>0&&!x->mism&&!x->retmis&&!x->missed&&x->engaged>0&&!x->declined_bad;
 logline("  class %d: %s: %d cases, %d eligible, drawn by the fast path %d (%lu conversions, %lu pixels), not drawn %d; images differing %d, return values differing %d; left to DIBENG on purpose %d, wrongly taken %d",
  k->bit,ok?"PASS":"FAIL",x->cases,x->eligible,x->engaged,x->ops,x->pixels,x->missed,x->mism,x->retmis,x->declined_ok,x->declined_bad);
 if(k->kind>=2) logline("; streams the decoder refused %d",x->refused);
 logline("\n");
 logline("           against the model (palette entry of the source pixel): DIBENG %d / %d pixels, fast path %d / %d\n",x->modelok,x->vis,x->onmodel,x->vis);
 if(x->ucases) logline("           index colours (DIB_PAL_COLORS): %d cases, %d drawn by the fast path, images equal to DIBENG in every case that passed\n",x->ucases,x->uengaged);
 return ok;
}
/* ---- timing ---- */
static double tm(int api,int kind,int sw,int sh,int dw,int dh,DWORD mask) {
 BITMAPINFO *b=sbi; int i,bits=kbits[kind],slen=0,rle=kind>=2; double t0; long n,k;
 if(rle) slen=gen_rle(sbuf,sw,sh,bits,0);
 else {n=(long)stride_of(sw,bits)*sh; for(k=0;k<n;k++) sbuf[k]=(BYTE)(k*7+(k>>9));}
 memset(b,0,SBI_SIZE); b->bmiHeader.biSize=sizeof(BITMAPINFOHEADER); b->bmiHeader.biWidth=sw; b->bmiHeader.biHeight=sh; b->bmiHeader.biPlanes=1; b->bmiHeader.biBitCount=(WORD)bits; b->bmiHeader.biCompression=kcomp[kind]; b->bmiHeader.biSizeImage=(DWORD)slen;
 {DWORD *d=(DWORD*)b->bmiColors; int ne=bits==8?256:(1<<bits); for(k=0;k<ne;k++) d[k]=(DWORD)k*0x010203UL+0x102030UL;}
 setmask2(mask);
#define CALL() (api==1?SetDIBitsToDevice(sdc,0,0,sw,sh,0,0,0,sh,sbuf,b,DIB_RGB_COLORS):StretchDIBits(sdc,0,0,dw,dh,0,0,sw,sh,sbuf,b,DIB_RGB_COLORS,SRCCOPY))
 CALL(); GdiFlush();
 t0=now(); for(i=0;i<10;i++) CALL(); GdiFlush();
 return (now()-t0)/10.0;
}
/* representative calls per shape: api, sw, sh, dw, dh; raw bitmaps 640x480, RLE 512x384 (the decode buffer holds 262144 pixels) */
static const int geoRaw[3][2][5]={{{0,320,240,640,480},{0,214,160,642,480}},{{0,640,240,640,480},{0,213,480,639,480}},{{1,640,480,640,480},{0,640,480,640,480}}};
static const int geoRle[3][2][5]={{{0,256,192,512,384},{0,171,128,513,384}},{{0,512,192,512,384},{0,171,384,513,384}},{{1,512,384,512,384},{0,512,384,512,384}}};
/* does the class pay? DIBENG (mask 2 = 0) against the fast path (the class bit): the totals of the representative calls decide; enabled only when the fast path is not slower (5% noise allowed) */
static int gate(int ci) {
 const cls_t *k=&classes[ci]; int j; double a=0,b=0;
 for(j=0;j<2;j++) {
  const int *g=k->kind>=2?geoRle[k->shape][j]:geoRaw[k->shape][j];
  a+=tm(g[0],k->kind,g[1],g[2],g[3],g[4],0); b+=tm(g[0],k->kind,g[1],g[2],g[3],g[4],(DWORD)k->bit);
 }
 setmask2(0);
 logline("  class %d timing: DIBENG %.2f ms, fast path %.2f ms (sum of 2 representative calls): %s\n",k->bit,a,b,b<=a*1.05?"pays, enabled":"NOT FASTER: left off");
 return b<=a*1.05;
}
static void timing(int dbpp,DWORD mask) {
 int kind;
 logline("-- timing into the 640x480x%d screen, ms per call, DIBENG / fast path (classes %lu on): 1:1 SetDIBitsToDevice, 2x, 3x (RLE: 512x384 pictures) --\n",dbpp,mask);
 for(kind=0;kind<4&&!aborted;kind++) {
  double a[3],b[3]; int rle=kind>=2,W=rle?512:640,H=rle?384:480;
  a[0]=tm(1,kind,W,H,W,H,0); b[0]=dbpp==32?tm(1,kind,W,H,W,H,mask):a[0];
  a[1]=tm(0,kind,W/2,H/2,W,H,0); b[1]=dbpp==32?tm(0,kind,W/2,H/2,W,H,mask):a[1];
  a[2]=tm(0,kind,W/3,H/3,(W/3)*3,(H/3)*3,0); b[2]=dbpp==32?tm(0,kind,W/3,H/3,(W/3)*3,(H/3)*3,mask):a[2];
  if(dbpp==32) logline("    %-6s 1:1 %6.2f / %6.2f   2x %6.2f / %6.2f   3x %6.2f / %6.2f\n",kname[kind],a[0],b[0],a[1],b[1],a[2],b[2]);
  else logline("    %-6s 1:1 %6.2f   2x %6.2f   3x %6.2f   (DIBENG only: these sources are not taken at 16bpp)\n",kname[kind],a[0],a[1],a[2]);
  pump();
 }
 setmask2(0);
}
/* 16bpp: every new class on, the fast path must decline everything */
static int check16(void) {
 int kind,n,drawn=0,total=0; DWORD c0[4],c1[4];
 logline("-- 16bpp screen: 1-bit, 4-bit and RLE sources with every class on must stay with DIBENG --\n");
 for(kind=0;kind<4&&!aborted;kind++) for(n=0;n<12&&!aborted;n++) {
  cls_t k; tc c; k.bit=0; k.kind=kind; k.shape=n%3; mkcase(&c,&k); c.clip=0;
  setmask2(0xFFFFFFUL); counters(c0); draw(&c,0,0); counters(c1); total++; if(c1[0]>c0[0]) {drawn++; logline("      TAKEN at 16bpp: %s %dx%d\n",kname[kind],c.kx,c.ky);}
  pump();
 }
 setmask2(0);
 logline("  %d cases, %d drawn by the fast path (must be 0): %s\n",total,drawn,drawn?"FAIL":"PASS");
 return !drawn;
}
static int mode_ok(int bpp) {
 vx_status_t s; DWORD got=0,t=GetTickCount(); while(GetTickCount()-t<800) Sleep(20);
 memset(&s,0,sizeof(s));
 if(!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||s.width!=640||s.height!=480||(int)s.bpp!=bpp||s.stage!=6||s.failure||s.vga_mode) {logline("the driver is not in native 640x480x%d mode\n",bpp); return 0;}
 return 1;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 DWORD pass=0,tested=0,c[4],orig1=0,orig2=0,final; int rc=0,have_orig,ci,phase,ok16=1; DEVMODEA origm; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2STR2.TXT","w"); logline("VX2STR2 v1.0 / driver v10.104\n");
 finish("RUNNING: the run did not reach its end (VX2STR2.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 if(!counters(c)) {finish("FAIL: this driver has no conversion control (VX85598.VXD older than v10.99)"); return 1;}
 orig1=c[3]; counters2(c); orig2=c[3];
 setmask2(0xFFFFFFUL); counters2(c);
 if(c[3]!=0xFFFFFFUL) {logline("the driver did not take the live class mask 2 (read %lu): VX85598.VXD older than v10.104\n",c[3]); setmask2(orig2); finish("FAIL: VX85598.VXD older than v10.104"); return 1;}
 setmask2(0);
 sbuf=(BYTE*)calloc(640UL*480UL*4UL+8192UL,1); pic=(BYTE*)calloc(640UL*480UL*4UL+8192UL,1); dec=(BYTE*)calloc(64UL*64UL,1); defd=(BYTE*)calloc(64UL*64UL,1); sbi=(BITMAPINFO*)calloc(SBI_SIZE,1); bufA=(DWORD*)calloc(640UL*480UL,4); bufB=(DWORD*)calloc(640UL*480UL,4);
 if(!sbuf||!pic||!dec||!defd||!sbi||!bufA||!bufB) {finish("FAIL: out of memory"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have_orig=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 logline("desktop was %lux%lux%lu; class mask 2 at start %lu (from ACCEL_STRETCH2 in the registry), class mask 1 %lu\n",have_orig?origm.dmPelsWidth:0,have_orig?origm.dmPelsHeight:0,have_orig?origm.dmBitsPerPel:0,orig2,orig1);
 for(phase=0;phase<2&&!aborted;phase++) {
  int dbpp=phase?32:16; DWORD pmask=0,ppass=0;
  for(ci=0;ci<NCLS;ci++) pmask|=(DWORD)classes[ci].bit;
  if(!set_mode(640,480,dbpp)||!mode_ok(dbpp)) {logline("640x480x%d not available: this part is skipped%s\n",dbpp,phase?" and the classes keep their registry value":""); if(phase) rc=1; continue;}
  sdc=GetDC(NULL);
  if(!sdc||!make_target(dbpp)) {logline("no screen DC or no %d-bit DIB section: this part is skipped\n",dbpp); if(sdc) ReleaseDC(NULL,sdc); if(phase) rc=1; continue;}
  setmask1(0);
  if(!phase) {ok16=check16(); if(!aborted) timing(16,0);}
  else {
   tested=pmask;
   for(ci=0;ci<NCLS&&!aborted;ci++) {if(run_class(ci)&&gate(ci)) ppass|=(DWORD)classes[ci].bit;}
   logline("classes that passed (pixels equal and not slower) at 32bpp: %lu\n",ppass);
   pass=ppass;
   if(!aborted) {if(ppass) timing(32,ppass); else logline("no class passed: timing skipped\n");}
  }
  drop_target(); ReleaseDC(NULL,sdc);
 }
 setmask2(orig2); setmask1(orig1);
 if(have_orig) {ChangeDisplaySettingsA(&origm,0); logline("desktop put back\n");}
 final=tested?pass:orig2;
 logline("classes that passed: %lu; ACCEL_STRETCH2 to write: %lu\n",pass,final);
 if(aborted) {finish("FAIL: aborted; nothing was changed"); logline("aborted\n");}
 else if(!tested) finish("FAIL: 640x480x32 not available; nothing was changed");
 else if(!ok16) finish("FAIL: the fast path drew a 1-bit, 4-bit or RLE source at 16bpp; ACCEL_STRETCH2 not written (see VX2STR2.TXT)");
 else if(!pass) finish("FAIL: no class passed; ACCEL_STRETCH2 not written (see VX2STR2.TXT)");
 else {char m[160]; sprintf(m,"%s: classes that passed %lu (.533: nothing written, the driver reads no ACCEL_STRETCH2)",pass==tested?"PASS":"PARTIAL",final); finish(m);}
 logline("Finished.\n"); if(lf) fclose(lf);
 CloseHandle(dev);
 return 0;
}
