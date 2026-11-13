/* VX2LINE v1.0: the 2D engine's Bresenham line against the DIB engine's lines, pixel for pixel. Measures what a line hook in the driver needs to know:
 *   T1  8 octants x many slopes: the engine's line (command rop|2005, last pixel skipped) with the error term started at 2m-n (variant A) and at 2m-n-1 (variant B),
 *       against (a) a software Bresenham with the same recurrence, (b) the DIB engine's LineTo on a memory bitmap of the screen format. For lines where A and B differ (ties) the table says
 *       which one the DIB engine matches, per octant.
 *   T2  the 16 binary raster operations (R2 codes) on the engine, four (P,D) bit combinations each, against the software ROP3 (the line set from T1).
 *   T3  the same 16 R2 codes through the DIB engine's LineTo, against the software ROP3 of the pen colour (what R2 mapping the hook must use).
 *   T4  clipping: the engine's clip rectangle against the unclipped line cut by the rectangle, and the DIB engine's clipped line against its own unclipped line cut by the rectangle.
 *   T5  Polyline on the DIB engine against the same segments drawn one by one (shared vertices, the last point).
 * Depths 32, 16 and 8 bits per pixel (the desktop is switched to 640x480 at each, then put back). Everything is drawn in the 64x64 dword scratch surface of the lab and a 64x64 memory bitmap;
 * nothing on the desktop is touched. Needs VX85598.VXD from v10.74 or later. Unattended: run it, read VX2LINE.TXT (or the one-line VX2LINE.RES). Esc stops. */
#ifdef MOCK
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
typedef unsigned long DWORD; typedef unsigned char BYTE; typedef unsigned short WORD;
#else
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#endif
#include "vx85598.h"
#include "vx855_3d.h"

#define TGT ((DWORD)VX3D_TARGET)
#define NPX 64
#define ROWB 256UL

static int dpx; /* bytes per pixel under test */
static FILE *lf; static int aborted;
static vx3d_request rq; static vx3d_report rp;
static DWORD recover_flag;
static void logline(const char *fmt,...) {
 va_list a;
 if(!lf) return;
 va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf);
#ifndef MOCK
 _commit(_fileno(lf));
#endif
}
#ifdef MOCK
extern int engine_call(const vx3d_request *q,vx3d_report *r);
extern int dib_begin(int bits);
extern void dib_end(void);
extern void dib_fill(DWORD pixel);
extern void dib_draw(int npts,const int *xy,int r2,const int *clip);
extern void dib_read(unsigned char *img); /* NPX*NPX pixels, dpx bytes each, row pitch NPX*dpx */
#else
static HANDLE dev=INVALID_HANDLE_VALUE;
static int engine_call(const vx3d_request *q,vx3d_report *r) {
 DWORD got=0;
 memset(r,0,sizeof(*r));
 if(!DeviceIoControl(dev,VX_IOCTL_3D_TEST,(void*)q,sizeof(*q),r,sizeof(*r),&got,NULL)) {logline("  driver request failed: Windows error %lu\n",GetLastError()); return 0;}
 if(got!=sizeof(*r)||r->magic!=VX3D_MAGIC||r->cb!=sizeof(*r)||r->version!=VX3D_VERSION||r->build!=VX3D_BUILD||r->producer!=VX3D_PRODUCER) {logline("  the driver reply does not match this program (copy VX85598.VXD and VX2LINE.EXE from the same package)\n"); return 0;}
 return 1;
}
static HDC mdc; static HBITMAP mbm,mold; static HPEN mpen,mpold; static int mpitch;
static unsigned char dbuf[NPX*NPX*4];
static int dib_begin(int bits) {
 HDC sdc; BITMAP bm;
 sdc=GetDC(NULL); mdc=CreateCompatibleDC(sdc); mbm=CreateCompatibleBitmap(sdc,NPX,NPX); ReleaseDC(NULL,sdc);
 if(!mdc||!mbm) return 0;
 GetObject(mbm,sizeof(bm),&bm);
 if(bm.bmBitsPixel!=bits||bm.bmWidth!=NPX||bm.bmWidthBytes*NPX>(int)sizeof(dbuf)) {logline("memory bitmap is %d bits, %d bytes per row (expected %d bits)\n",bm.bmBitsPixel,bm.bmWidthBytes,bits); return 0;}
 mpitch=bm.bmWidthBytes;
 mold=(HBITMAP)SelectObject(mdc,mbm);
 mpen=CreatePen(PS_SOLID,1,RGB(255,255,255)); mpold=(HPEN)SelectObject(mdc,mpen);
 return 1;
}
static void dib_end(void) {
 if(mdc) {SelectObject(mdc,mpold); SelectObject(mdc,mold); DeleteObject(mpen); DeleteObject(mbm); DeleteDC(mdc);} mdc=NULL;
}
static void dib_fill(DWORD pixel) {
 int x,y,k;
 for(y=0;y<NPX;y++) for(x=0;x<NPX;x++) for(k=0;k<dpx;k++) dbuf[y*mpitch+x*dpx+k]=(unsigned char)(pixel>>(8*k));
 SetBitmapBits(mbm,(DWORD)(mpitch*NPX),dbuf);
}
static void dib_draw(int npts,const int *xy,int r2,const int *clip) {
 POINT pt[8]; int i,sv=0;
 SetROP2(mdc,r2);
 if(clip) {sv=SaveDC(mdc); IntersectClipRect(mdc,clip[0],clip[1],clip[2],clip[3]);}
 for(i=0;i<npts&&i<8;i++) {pt[i].x=xy[i*2]; pt[i].y=xy[i*2+1];}
 if(npts==2) {MoveToEx(mdc,pt[0].x,pt[0].y,NULL); LineTo(mdc,pt[1].x,pt[1].y);}
 else Polyline(mdc,pt,npts);
 if(clip) RestoreDC(mdc,sv);
}
static void dib_read(unsigned char *img) {
 int y;
 GetBitmapBits(mbm,(DWORD)(mpitch*NPX),dbuf);
 for(y=0;y<NPX;y++) memcpy(img+y*NPX*dpx,dbuf+y*mpitch,(size_t)(NPX*dpx));
}
#endif

typedef struct {DWORD cmd,fg,init,w,h; int n; DWORD r[16][2];} op_t;
static void addreg(op_t *o,DWORD reg,DWORD v) {o->r[o->n][0]=reg; o->r[o->n][1]=v; o->n++;}
static int hung,nhang,eng_reset,eng_form;
static int run_op(DWORD mode,const op_t *o) {
 int i;
 memset(&rq,0,sizeof(rq));
 rq.cb=sizeof(rq); rq.version=VX3D_VERSION; rq.arm=VX3D_ARM; rq.reserved=21|(recover_flag?0x100UL:0UL); rq.probe=VX3D_SF_PROBE;
 rq.lab_cmd=o->cmd; rq.lab_mode=mode; rq.lab_w=o->w; rq.lab_h=o->h; rq.lab_fg=o->init; rq.lab_bg=0; rq.lab_flags=128UL|16UL|(eng_reset?32768UL:0UL); rq.lab_ndata=0; /* flag 128: the whole scratch surface holds lab_fg (the initial pixel), the line colour comes through registers */
 rq.lab_nregs=(DWORD)o->n; for(i=0;i<o->n;i++) {rq.lab_reg[i][0]=o->r[i][0]; rq.lab_reg[i][1]=o->r[i][1];}
 if(!engine_call(&rq,&rp)) return 0;
 recover_flag=(rp.fault_latched||rp.result==VX3D_DISABLED)?1:0;
 if(rp.sf_x_info[0][1]) {nhang++; logline("  HANG: the engine hung on this operation (%s)\n",rp.sf_x_info[0][2]?"reset worked":"reset FAILED, restart Windows"); if(!rp.sf_x_info[0][2]||nhang>=24) hung=1; return 0;}
 if(rp.result!=VX3D_PASS) {logline("  driver result %lu (not PASS)\n",(DWORD)rp.result); return 0;}
 return 1;
}
static int pump(void) {
#ifndef MOCK
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
#endif
 return !aborted&&!hung;
}
static DWORD M(void) {return dpx==1?0xFFUL:dpx==2?0xFFFFUL:0xFFFFFFFFUL;}
static DWORD rep(DWORD p) { /* a pixel value repeated to fill a dword */
 p&=M(); if(dpx==1) return p*0x01010101UL; if(dpx==2) return p|(p<<16); return p;
}
static DWORD bget(const unsigned int *w,DWORD byteoff) {return (w[byteoff>>2]>>((byteoff&3)*8))&0xFF;}
static DWORD eget(const unsigned int *w,int y,int x) {DWORD v=0; int k; for(k=dpx-1;k>=0;k--) v=(v<<8)|bget(w,(DWORD)y*ROWB+(DWORD)x*(DWORD)dpx+(DWORD)k); return v;}
static DWORD dget(const unsigned char *img,int y,int x) {DWORD v=0; int k; for(k=dpx-1;k>=0;k--) v=(v<<8)|img[(y*NPX+x)*dpx+k]; return v;}
static DWORD rop3(DWORD rop,DWORD P,DWORD S,DWORD D) {
 DWORD r=0; int i;
 for(i=0;i<8;i++) if((rop>>i)&1) r|=((i&4)?P:~P)&((i&2)?S:~S)&((i&1)?D:~D);
 return r&M();
}
static const BYTE r2rop[17]={0,0x00,0x05,0x0A,0x0F,0x50,0x55,0x5A,0x5F,0xA0,0xA5,0xAA,0xAF,0xF0,0xF5,0xFA,0xFF}; /* R2_BLACK (1) .. R2_WHITE (16) as ROP3 with P = pen, D = destination */

typedef struct {int x0,y0,x1,y1,oc;} ln_t; /* oc: the octant the line was generated for (a horizontal line has no sign to tell it by) */
static int octant(const ln_t *l) {return l->oc;}
/* software Bresenham, the engine's recurrence: e starts at 2m-n (-1 for variant B); e >= 0 steps the minor axis and adds 2(m-n), else adds 2m; n pixels (last one skipped) */
static void model_set(const ln_t *l,int var,unsigned char *s) {
 int dx=l->x1-l->x0,dy=l->y1-l->y0,adx=abs(dx),ady=abs(dy),ym=ady>adx,n=ym?ady:adx,m=ym?adx:ady,sx=dx<0?-1:1,sy=dy<0?-1:1,k,e,minor=0;
 memset(s,0,NPX*NPX);
 e=2*m-n-(var?1:0);
 for(k=0;k<n;k++) {
  int x,y;
  if(ym) {x=l->x0+sx*minor; y=l->y0+sy*k;} else {x=l->x0+sx*k; y=l->y0+sy*minor;}
  if(x>=0&&x<NPX&&y>=0&&y<NPX) s[y*NPX+x]=1;
  if(e>=0) {minor++; e+=2*(m-n);} else e+=2*m;
 }
}
static unsigned char eimg[NPX*NPX*4]; /* engine result, NPX x NPX pixels at dpx bytes */
static int espill;                    /* pixels changed outside the 64x64 pixel area */
static DWORD init_pixel;
static int eng_line(DWORD mode,const ln_t *l,int var,DWORD rop,DWORD P,DWORD init,const int *clip) {
 op_t o; int dx=l->x1-l->x0,dy=l->y1-l->y0,adx=abs(dx),ady=abs(dy),ym=ady>adx,n=ym?ady:adx,m=ym?adx:ady,x,y,k;
 if(!pump()) return 0;
 memset(&o,0,sizeof(o)); o.init=rep(init); o.fg=P; o.w=(DWORD)n; o.h=1;
 o.cmd=(rop<<24)|(eng_form==1?0x0005UL:0x2005UL)|(eng_form==2?0x400000UL:0UL)|0x100000UL|(dx<0?0x8000UL:0)|(dy<0?0x4000UL:0)|(ym?0x200000UL:0)|(clip?0x1000UL:0);
 addreg(&o,0x08,(32UL<<16)|32UL); addreg(&o,0x14,TGT>>3); addreg(&o,0x4C,P&M()); addreg(&o,0x58,P&M());
 addreg(&o,0x24,0xFFFFFFFFUL);
 addreg(&o,0x18,((DWORD)(2*m)<<16)|((DWORD)(2*(m-n))&0x3FFFUL));
 addreg(&o,0x2C,(DWORD)(2*m-n-(var?1:0))&0x3FFFUL);
 addreg(&o,0x0C,(DWORD)n&0xFFFUL);
 addreg(&o,0x10,((DWORD)l->y0<<16)|(DWORD)l->x0);
 if(clip) {addreg(&o,0x40,((DWORD)clip[1]<<16)|(DWORD)clip[0]); addreg(&o,0x44,((DWORD)(clip[3]-1)<<16)|(DWORD)(clip[2]-1));}
 logline("  engine line (%d,%d)-(%d,%d) variant %c rop %02lX%s cmd %08lX\n",l->x0,l->y0,l->x1,l->y1,var?'B':'A',rop,clip?" clipped":"",o.cmd);
 if(!run_op(mode,&o)) return 0;
 espill=0;
 for(y=0;y<NPX;y++) for(x=0;x<(int)(ROWB/(DWORD)dpx);x++) {
  DWORD g=eget((const unsigned int*)rp.image,y,x);
  if(x<NPX) for(k=0;k<dpx;k++) eimg[(y*NPX+x)*dpx+k]=(unsigned char)(g>>(8*k));
  else if(g!=(init&M())) espill++;
 }
 return 1;
}
static unsigned char dimg[NPX*NPX*4];
static int cnt_set(const unsigned char *s) {int i,c=0; for(i=0;i<NPX*NPX;i++) c+=s[i]; return c;}
static void set_from(const unsigned char *img,DWORD bg,unsigned char *s,int engine) {
 int x,y; (void)engine;
 for(y=0;y<NPX;y++) for(x=0;x<NPX;x++) s[y*NPX+x]=(dget(img,y,x)!=(bg&M()))?1:0;
}
static int diffs(const unsigned char *a,const unsigned char *b) {int i,c=0; for(i=0;i<NPX*NPX;i++) if(a[i]!=b[i]) c++; return c;}

#define NL 4000
static ln_t lines[NL]; static int nlines;
static void gen_lines(int stride) {
 static const int nl[]={1,2,3,4,5,6,7,8,9,10,12,13,16,19,21,25,28,34,40,47};
 int o,i,j,cnt=0; nlines=0;
 for(o=0;o<8;o++) for(i=0;i<(int)(sizeof(nl)/sizeof(nl[0]));i++) {
  int n=nl[i],ms[14],nm=0,k;
  if(n<=13) for(j=0;j<=n;j++) ms[nm++]=j;
  else {ms[nm++]=0; ms[nm++]=1; ms[nm++]=2; ms[nm++]=n/3; ms[nm++]=n/2-1; ms[nm++]=n/2; ms[nm++]=n/2+1; ms[nm++]=(2*n)/3; ms[nm++]=n-2; ms[nm++]=n-1; ms[nm++]=n;}
  for(k=0;k<nm;k++) {
   int m=ms[k],off,sx=(o&1)?-1:1,sy=(o&2)?-1:1,ym=(o&4)!=0,dx,dy; ln_t *l;
   if(ym&&m>=n) continue;
   cnt++; if(stride>1&&(cnt%stride)) continue;
   off=(cnt*7)%9; dx=ym?m:n; dy=ym?n:m; l=&lines[nlines++]; l->oc=o;
   l->x0=sx>0?3+off:56-off; l->y0=sy>0?3+((off*5)%9):56-((off*5)%9); l->x1=l->x0+sx*dx; l->y1=l->y0+sy*dy;
   if(nlines>=NL) return;
  }
 }
}
static DWORD ppix; /* the pen's pixel value as the DIB engine draws it */
static int nops;
static char resline[300];
static unsigned char sa[NPX*NPX],sb[NPX*NPX],se[NPX*NPX],sd[NPX*NPX],sd0[NPX*NPX];
static const int dib_pts_zero[1]={0};
static void dib_line_set(const ln_t *l,unsigned char *s,const int *clip) {
 int xy[4]; xy[0]=l->x0; xy[1]=l->y0; xy[2]=l->x1; xy[3]=l->y1;
 dib_fill(0); dib_draw(2,xy,13,clip); dib_read(dimg); set_from(dimg,0,s,0);
}
/* the minor-axis offset of each major step (0..n-1), as text; '-' when no pixel, '*' when more than one */
static void profile(const unsigned char *s,const ln_t *l,char *out,int cap) {
 int dx=l->x1-l->x0,dy=l->y1-l->y0,adx=abs(dx),ady=abs(dy),ym=ady>adx,n=ym?ady:adx,k,j,p=0,sx=dx<0?-1:1,sy=dy<0?-1:1;
 for(k=0;k<n&&p<cap-8;k++) {
  int cntp=0,mo=0,lim=(ym?adx:ady)+1;
  for(j=0;j<lim+1&&j<NPX;j++) {
   int x,y; if(ym) {x=l->x0+sx*j; y=l->y0+sy*k;} else {x=l->x0+sx*k; y=l->y0+sy*j;}
   if(x>=0&&x<NPX&&y>=0&&y<NPX&&s[y*NPX+x]) {if(!cntp) mo=j; cntp++;}
  }
  p+=sprintf(out+p,cntp==0?"-,":cntp>1?"*,":"%d,",mo);
 }
 out[p]=0;
}
static int oct_n[8],ea_m[8],eb_m[8],ea_d[8],eb_d[8],either[8],tie_n[8],tie_a[8],tie_b[8],tie_x[8],neither_logged[8];
static void oct_summary(int o) {
  const char *rule=tie_n[o]==0?"no ties":tie_a[o]==tie_n[o]?"A (step the minor axis on a tie)":tie_b[o]==tie_n[o]?"B (no step on a tie)":"MIXED or neither";
  logline("  octant %d (%s%s%s): %d lines; engine A=model A %d, engine B=model B %d; DIB=A %d, DIB=B %d, DIB=either %d; ties %d: DIB=A %d, DIB=B %d, neither %d -> DIB rule %s\n",
   o,(o&4)?"Y-major":"X-major",(o&1)?", X dec":"",(o&2)?", Y dec":"",oct_n[o],ea_m[o],eb_m[o],ea_d[o],eb_d[o],either[o],tie_n[o],tie_a[o],tie_b[o],tie_x[o],rule);
}
static int test_geometry(DWORD mode,int d) {
 int i,o;
 int tot=0,spillbad=0;
 gen_lines(8); /* v1.2: the 32bpp run of v1.1 settled the tie rules on all 1360 lines; this is the confirmation at each depth */
 eng_reset=1; /* T1 and T4 start every line from a reset engine and an all-zero surface (lab flag 32768) */
 memset(oct_n,0,sizeof(oct_n)); memset(ea_m,0,sizeof(ea_m)); memset(eb_m,0,sizeof(eb_m)); memset(ea_d,0,sizeof(ea_d)); memset(eb_d,0,sizeof(eb_d)); memset(either,0,sizeof(either));
 memset(tie_n,0,sizeof(tie_n)); memset(tie_a,0,sizeof(tie_a)); memset(tie_b,0,sizeof(tie_b)); memset(tie_x,0,sizeof(tie_x)); memset(neither_logged,0,sizeof(neither_logged));
 logline("-- T1 geometry, %d bytes per pixel, %d lines, pen pixel %lX --\n",d,nlines,ppix);
 for(i=0;i<nlines;i++) {
  const ln_t *l=&lines[i]; int ea,eb,tie,da,db,dx_a;
  if(!pump()) return 0;
  o=octant(l);
  dib_line_set(l,sd,NULL);
  if(!eng_line(mode,l,0,0xF0,ppix,0,NULL)) {if(hung||aborted) return 0; continue;}
  if(espill) spillbad++;
  set_from(eimg,0,se,1); nops++; model_set(l,0,sa); model_set(l,1,sb);
  ea=diffs(se,sa); tie=diffs(sa,sb)!=0;
  /* variant B from the engine only on tie lines (others are identical by construction of the recurrence, checked on the A run) */
  eb=0;
  if(tie) {
   if(!eng_line(mode,l,1,0xF0,ppix,0,NULL)) {if(hung||aborted) return 0; continue;}
   set_from(eimg,0,se,1); nops++; eb=diffs(se,sb);
  } else eb=ea;
  da=diffs(sd,sa); db=diffs(sd,sb); dx_a=da;
  oct_n[o]++; tot++;
  if(!ea) ea_m[o]++;
  if(!eb) eb_m[o]++;
  if(!da) ea_d[o]++;
  if(!db) eb_d[o]++;
  if(!da||!db) either[o]++;
  if(tie) {tie_n[o]++; if(!da&&db) tie_a[o]++; else if(!db&&da) tie_b[o]++; else if(da&&db) tie_x[o]++;}
  if(ea&&neither_logged[o]<3) {neither_logged[o]++; logline("  ENGINE != MODEL: octant %d line (%d,%d)-(%d,%d): %d pixels differ from the A model\n",o,l->x0,l->y0,l->x1,l->y1,ea);}
  if(da&&db&&neither_logged[o]<6) {
   neither_logged[o]++;
   if(neither_logged[o]<=2) {static char pa[400],pb[400],pd[400]; profile(sd,l,pd,400); profile(sa,l,pa,400); profile(sb,l,pb,400); logline("    minor offsets per step: DIB %s\n                            A   %s\n                            B   %s\n",pd,pa,pb);}
   logline("  DIB != A and != B: octant %d line (%d,%d)-(%d,%d): %d pixels differ from A, %d from B (DIB drew %d pixels, A %d)\n",o,l->x0,l->y0,l->x1,l->y1,da,db,cnt_set(sd),cnt_set(sa));}
  (void)dx_a;
  if(eb&&neither_logged[o]<6&&tie) {neither_logged[o]++; logline("  ENGINE B != MODEL B: octant %d line (%d,%d)-(%d,%d): %d pixels differ\n",o,l->x0,l->y0,l->x1,l->y1,eb);}
  if(i+1>=nlines||octant(&lines[i+1])!=o) oct_summary(o);
 }
 {int sm=0,sd2=0; for(o=0;o<8;o++) {sm+=ea_m[o]; sd2+=either[o];}
  logline("  T1 %d bytes per pixel: %d lines, engine equals its model on %d, DIB equals A or B on %d, %d engine runs changed pixels outside the area\n",d,tot,sm,sd2,spillbad);}
 return 1;
}
static const ln_t rop_line={5,6,45,23,0}; /* X-major, X and Y increasing, no tie */
/* v2.0: T2 (the ROP exploration of v1.2) is gone: the engine's line command draws a plain pen copy only (ROP 00, FF and 0F were ignored, the ROPs that read the destination hung it), so the driver maps R2_BLACK, R2_NOTCOPYPEN and R2_COPYPEN to the colour. T6 tests that mapping through the real driver. */
static int test_dib_rops(int d) {
 int r2,c,x,y,tot=0,good=0,xy[4]; DWORD X0=0x5A3CC3A5UL&M(); DWORD Xs[2]; int bad;
 Xs[0]=X0; Xs[1]=(~X0)&M();
 logline("-- T3 DIB engine raster operations on LineTo, %d bytes per pixel (pen pixel %lX) --\n",d,ppix);
 xy[0]=rop_line.x0; xy[1]=rop_line.y0; xy[2]=rop_line.x1; xy[3]=rop_line.y1;
 dib_line_set(&rop_line,sd0,NULL);
 for(r2=1;r2<=16;r2++) {
  int n=0,ok=0;
  for(c=0;c<2;c++) {
   dib_fill(Xs[c]); dib_draw(2,xy,r2,NULL); dib_read(dimg); n++; bad=0;
   for(y=0;y<NPX;y++) for(x=0;x<NPX;x++) {
    DWORD e=sd0[y*NPX+x]?rop3(r2rop[r2],ppix,0,Xs[c]):Xs[c],g=dget(dimg,y,x);
    if(g!=e) {if(!bad) logline("  DIB R2 %d dest %lX: first difference at %d,%d expected %lX got %lX\n",r2,Xs[c],x,y,e,g); bad++;}
   }
   if(!bad) ok++;
  }
  tot+=n; good+=ok;
  logline("  R2 %2d (ROP %02X): %d of %d runs match rop3(pen pixel, destination)\n",r2,r2rop[r2],ok,n);
 }
 logline("  T3 %d bytes per pixel: %d of %d runs match\n",d,good,tot);
 return 1;
}
static int test_clip(DWORD mode,int d) {
 static const int clips[5][4]={{10,10,40,40},{0,0,64,20},{20,0,30,64},{15,18,16,19},{5,30,60,31}};
 static const ln_t cl[8]={{4,4,58,40},{58,4,6,44},{4,58,58,20},{58,58,8,10},{4,4,40,58},{58,4,24,58},{4,58,44,6},{58,58,22,4}};
 int i,j,x,y,en=0,eok=0,dn=0,dok=0;
 eng_reset=1;
 logline("-- T4 clipping, %d bytes per pixel --\n",d);
 for(i=0;i<8;i++) for(j=0;j<5;j++) {
  const ln_t *l=&cl[i]; const int *c=clips[j]; int bad;
  model_set(l,0,sa);
  if(eng_line(mode,l,0,0xF0,ppix,0,c)) {
   nops++; en++; bad=0;
   set_from(eimg,0,se,1);
   for(y=0;y<NPX;y++) for(x=0;x<NPX;x++) {int in=x>=c[0]&&x<c[2]&&y>=c[1]&&y<c[3]; if(se[y*NPX+x]!=(in?sa[y*NPX+x]:0)) bad++;}
   if(!bad) eok++; else if(en-eok<=4) logline("  engine clip %d,%d,%d,%d on line (%d,%d)-(%d,%d): %d pixels differ from the cut model\n",c[0],c[1],c[2],c[3],l->x0,l->y0,l->x1,l->y1,bad);
  } else if(hung||aborted) return 0;
  dib_line_set(l,sd0,NULL); dib_line_set(l,sd,c); dn++; bad=0;
  for(y=0;y<NPX;y++) for(x=0;x<NPX;x++) {int in=x>=c[0]&&x<c[2]&&y>=c[1]&&y<c[3]; if(sd[y*NPX+x]!=(in?sd0[y*NPX+x]:0)) bad++;}
  if(!bad) dok++; else if(dn-dok<=4) logline("  DIB clip %d,%d,%d,%d on line (%d,%d)-(%d,%d): %d pixels differ from the unclipped line cut by the rectangle\n",c[0],c[1],c[2],c[3],l->x0,l->y0,l->x1,l->y1,bad);
 }
 logline("  T4 %d bytes per pixel: engine clip %d of %d match, DIB clip %d of %d match\n",d,eok,en,dok,dn);
 return 1;
}
static int test_poly(int d) {
 static const int pts[8]={5,5,40,12,30,50,8,40};
 int r2s[2]={13,7},k,s,x,y,same[2],lastset[2];
 logline("-- T5 Polyline on the DIB engine, %d bytes per pixel --\n",d);
 for(k=0;k<2;k++) {
  DWORD X0=0x5A3CC3A5UL&M(); int bad=0;
  static unsigned char pa[NPX*NPX*4];
  dib_fill(X0); dib_draw(4,pts,r2s[k],NULL); dib_read(dimg);
  memcpy(pa,dimg,(size_t)(NPX*NPX*dpx));
  dib_fill(X0);
  for(s=0;s<3;s++) dib_draw(2,pts+s*2,r2s[k],NULL);
  dib_read(dimg);
  for(y=0;y<NPX;y++) for(x=0;x<NPX;x++) {
   DWORD a=0,b=dget(dimg,y,x); int q; for(q=dpx-1;q>=0;q--) a=(a<<8)|pa[(y*NPX+x)*dpx+q];
   if(a!=b) {if(!bad) logline("  R2 %d: first difference at %d,%d: Polyline %lX, segments %lX\n",r2s[k],x,y,a,b); bad++;}
  }
  same[k]=!bad;
  {DWORD a=0,u=0; int q; for(q=dpx-1;q>=0;q--) a=(a<<8)|pa[(pts[7]*NPX+pts[6])*dpx+q]; u=X0; lastset[k]=(a!=u);}
  logline("  R2 %d: Polyline equals the three LineTo segments: %s; pixel at the last point %s\n",r2s[k],same[k]?"yes":"NO",lastset[k]?"changed (drawn)":"untouched (not drawn)");
 }
 return 1;
}
#ifndef MOCK
/* T6: the shipped hook. The same lines are drawn on the screen twice, 310 pixels apart on an identical background: region A with the engine forced on (ctrlq 5: all copy directions, ignore the
 * size thresholds), region B with the engine off (ctrlq 2), so B is DIBENG's drawing. Both are read back through one BitBlt and compared byte for byte. Raster operations the engine does not take (XOR, WHITE)
 * must not reach it; the others must (the engine operation counter says how many segments did). */
static int fails6,nsets6;
static int ctrlq(DWORD in,DWORD *out) {DWORD got=0,tmp[4]; if(!out) out=tmp; return DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&in,4,out,16,&got,NULL)&&got==16;}
static unsigned long rs6=0x6D2B79F5UL;
static int rnd6(int n) {rs6^=rs6<<13; rs6&=0xFFFFFFFFUL; rs6^=rs6>>17; rs6^=rs6<<5; rs6&=0xFFFFFFFFUL; return (int)(rs6%(unsigned long)n);}
static BYTE tb[640*480*4];
#define RA_X 10
#define RB_X 322 /* v2.2: 312 pixels from region A = a multiple of 8, so the 8x8 dither pattern of a non-pure colour brush is in the same phase in both regions (v2.0 used 310 and every 8bpp set differed in the background) */
#define RY 10
#define RW 300
#define RH 300
static void t6_draw(HDC sdc,int ox,const POINT *pt,int nseg,int poly,int useclip,HPEN pen,int r2) {
 HPEN old; int i,sv=SaveDC(sdc);
 SetROP2(sdc,r2); old=(HPEN)SelectObject(sdc,pen);
 if(useclip) IntersectClipRect(sdc,ox+50,RY+50,ox+250,RY+250);
 if(poly) {POINT q[8]; for(i=0;i<=nseg&&i<8;i++) {q[i].x=ox+pt[i].x; q[i].y=RY+pt[i].y;} Polyline(sdc,q,nseg+1);}
 else for(i=0;i<nseg;i++) {MoveToEx(sdc,ox+pt[i*2].x,RY+pt[i*2].y,NULL); LineTo(sdc,ox+pt[i*2+1].x,RY+pt[i*2+1].y);}
 SelectObject(sdc,old); RestoreDC(sdc,sv);
}
static void t6_set(HDC sdc,HDC mdc,HBITMAP mbm,int bypp,COLORREF col,int r2,int useclip,int poly) {
 static const COLORREF bgc[3]={RGB(40,90,160),RGB(200,200,50),RGB(120,30,140)};
 POINT pt[64]; int nseg=poly?5:24,i,k,x,y,bad=0,fx=-1,fy=-1,eng; DWORD o[4],ops0=0,ops1=0; HPEN pen; HBRUSH hb; RECT rc; BITMAP bm; DWORD va=0,vb=0; int expect_engine=(r2==13||r2==4||r2==1);
 nsets6++;
 ctrlq(2,NULL); /* engine off for the background, so both regions get the same pixels */
 for(i=0;i<3;i++) {
  hb=CreateSolidBrush(bgc[i]);
  rc.left=RA_X; rc.right=RA_X+RW; rc.top=RY+i*100; rc.bottom=RY+i*100+100; FillRect(sdc,&rc,hb);
  rc.left=RB_X; rc.right=RB_X+RW; FillRect(sdc,&rc,hb);
  DeleteObject(hb);
 }
 if(poly) {for(i=0;i<=nseg;i++) {pt[i].x=20+rnd6(260); pt[i].y=20+rnd6(260);} for(i=1;i<=nseg;i++) if(pt[i].x==pt[i-1].x&&pt[i].y==pt[i-1].y) pt[i].x^=1;}
 else for(i=0;i<nseg;i++) {
  int dx,dy;
  do {pt[i*2].x=rnd6(RW); pt[i*2].y=rnd6(RH); pt[i*2+1].x=rnd6(RW); pt[i*2+1].y=rnd6(RH); dx=abs(pt[i*2+1].x-pt[i*2].x); dy=abs(pt[i*2+1].y-pt[i*2].y);} while((dx>dy?dx:dy)<40);
 }
 pen=CreatePen(PS_SOLID,1,col);
 ctrlq(5,o); ops0=o[0];
 t6_draw(sdc,RA_X,pt,nseg,poly,useclip,pen,r2);
 ctrlq(0xFFFFFFFEUL,o); ops1=o[0];
 ctrlq(2,NULL);
 t6_draw(sdc,RB_X,pt,nseg,poly,useclip,pen,r2);
 ctrlq(0xFFFFFFFFUL,NULL);
 DeleteObject(pen);
 BitBlt(mdc,0,0,640,480,sdc,0,0,SRCCOPY);
 GetObject(mbm,sizeof(bm),&bm);
 GetBitmapBits(mbm,(DWORD)bm.bmWidthBytes*480UL,tb);
 for(y=0;y<RH;y++) for(x=0;x<RW;x++) {
  int d=0;
  for(k=0;k<bypp;k++) if(tb[(DWORD)(RY+y)*bm.bmWidthBytes+(DWORD)(RA_X+x)*bypp+k]!=tb[(DWORD)(RY+y)*bm.bmWidthBytes+(DWORD)(RB_X+x)*bypp+k]) d=1;
  if(d) {
   if(!bad) {fx=x; fy=y; va=0; vb=0; for(k=bypp-1;k>=0;k--) {va=(va<<8)|tb[(DWORD)(RY+y)*bm.bmWidthBytes+(DWORD)(RA_X+x)*bypp+k]; vb=(vb<<8)|tb[(DWORD)(RY+y)*bm.bmWidthBytes+(DWORD)(RB_X+x)*bypp+k];}}
   bad++;
  }
 }
 eng=(int)(ops1-ops0);
 {int bad_eng=expect_engine?(eng<1):(eng!=0);
  logline("  T6 pen RGB(%d,%d,%d) R2 %d%s%s: %d segments, %d on the engine, %d bytes differ from DIBENG%s%s: %s\n",(int)GetRValue(col),(int)GetGValue(col),(int)GetBValue(col),r2,useclip?" clipped":"",poly?" polyline":"",nseg,eng,bad,
   bad?" (first at ":"",bad?"":"",(bad||bad_eng)?"FAIL":"PASS");
  if(bad) logline("      first difference at %d,%d: hook %lX, DIBENG %lX\n",fx,fy,va,vb);
  if(bad_eng) logline("      %s\n",expect_engine?"no segment reached the engine (the hook declined: pen, clip or colour rule)":"a raster operation the engine does not take reached it");
  if(bad||bad_eng) fails6++;}
}
static void test_hook(int d) {
 static const COLORREF cols[5]={RGB(255,0,0),RGB(0,128,255),RGB(255,255,255),RGB(0,0,0),RGB(10,200,60)};
 static const int r2s[5]={13,4,1,7,16};
 HDC sdc,mdc; HBITMAP mbm,mold; int c,r,f6=fails6;
 logline("-- T6 the line hook through GDI against DIBENG, %d bytes per pixel --\n",d);
 if(!ctrlq(0xFFFFFFFEUL,NULL)) {logline("  the driver control call is not available: T6 skipped\n"); return;}
 sdc=GetDC(NULL); mdc=CreateCompatibleDC(sdc); mbm=CreateCompatibleBitmap(sdc,640,480);
 if(!sdc||!mdc||!mbm) {logline("  T6: could not create the bitmap\n"); fails6++; return;}
 mold=(HBITMAP)SelectObject(mdc,mbm);
 for(r=0;r<5&&pump();r++) for(c=0;c<5;c++) t6_set(sdc,mdc,mbm,d,cols[c],r2s[r],0,0);
 t6_set(sdc,mdc,mbm,d,cols[0],13,1,0); t6_set(sdc,mdc,mbm,d,cols[1],13,1,0);
 t6_set(sdc,mdc,mbm,d,cols[4],13,0,1); t6_set(sdc,mdc,mbm,d,cols[1],4,0,1); t6_set(sdc,mdc,mbm,d,cols[2],13,1,1);
 SelectObject(mdc,mold); DeleteObject(mbm); DeleteDC(mdc); ReleaseDC(NULL,sdc);
 ctrlq(0xFFFFFFFFUL,NULL);
 logline("  T6 %d bytes per pixel: %d sets, %s\n",d,nsets6,fails6==f6?"all match DIBENG":"FAILURES");
}
#endif
static int run_depth(DWORD mode,int d) {
 int xy[4],x,y,np=0; DWORD v; (void)mode;
 dpx=d;
 xy[0]=5; xy[1]=5; xy[2]=50; xy[3]=5;
 dib_fill(0); dib_draw(2,xy,13,NULL); dib_read(dimg);
 ppix=0; for(y=0;y<NPX;y++) for(x=0;x<NPX;x++) {v=dget(dimg,y,x); if(v&&!ppix) ppix=v; if(v) np++;}
 logline("DIB engine: horizontal pen line sets %d pixels, pen pixel value %lX (%d byte%s per pixel)\n",np,ppix,d,d==1?"":"s");
 if(!ppix) {logline("the DIB engine drew nothing: stop\n"); return 0;}
 if(!test_geometry(mode,d)) return 0;
 if(!test_dib_rops(d)) return 0;
 if(!test_clip(mode,d)) return 0;
 if(!test_poly(d)) return 0;
return 1;
}
#ifndef MOCK
static void finish(const char *res) {FILE *f=fopen("VX2LINE.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
#endif
#ifdef MOCK
int main(void) {
 static const DWORD codes[3]={0x300,0x100,0x000}; static const int ds[3]={4,2,1}; int k,rc=0;
 lf=stdout;
 for(k=0;k<3&&!rc;k++) {if(!dib_begin(ds[k]*8)) {printf("dib_begin failed\n"); return 1;} dpx=ds[k]; if(!run_depth(codes[k],ds[k])) rc=1; dib_end();}
 sprintf(resline,"%s: %d engine lines ran, %d hangs",nhang?"PASS with hangs":"PASS",nops,nhang);
 printf("%s\nrc=%d\n",resline,rc); return rc;
}
#else
static DEVMODEA orig;
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc;
 memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc);
 return rc==DISP_CHANGE_SUCCESSFUL;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 static const DWORD codes[3]={0x300,0x100,0x000}; static const int ds[3]={4,2,1};
 vx_status_t s; DWORD got=0,t; int rc=0,have_orig,k,pass; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2LINE.TXT","w");
 logline("VX2LINE v2.2 / driver v10.90\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 memset(&orig,0,sizeof(orig)); orig.dmSize=sizeof(orig); have_orig=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&orig)?1:0;
 logline("desktop was %lux%lux%lu\n",have_orig?orig.dmPelsWidth:0,have_orig?orig.dmPelsHeight:0,have_orig?orig.dmBitsPerPel:0);
 finish("RUNNING: the run did not reach its end (VX2LINE.TXT ends where it stopped)");
 /* v2.1: pass 0 runs T6 (the shipped hook, through GDI) at every depth first; pass 1 runs the lab tests T1, T3, T4, T5 (engine registers driven directly). The v2.0 run stopped inside the lab T4 at 16bpp, before T6 of that depth. */
 for(pass=0;pass<2&&!rc&&!aborted&&!hung;pass++) {
 logline(pass?"==== lab tests (engine driven directly) ====\n":"==== T6: the shipped hook through GDI ====\n");
 for(k=0;k<3&&!rc&&!aborted&&!hung;k++) {
  if(!set_mode(640,480,(DWORD)ds[k]*8)) {logline("could not set 640x480x%d\n",ds[k]*8); finish("FAIL: mode not available"); rc=1; break;}
  t=GetTickCount(); while(GetTickCount()-t<800) Sleep(20);
  memset(&s,0,sizeof(s));
  if(!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||s.width!=640||s.height!=480||s.bpp!=(DWORD)(ds[k]*8)||s.stage!=6||s.failure||s.vga_mode) {
   logline("the driver is not in native 640x480x%d mode\n",ds[k]*8); finish("FAIL: not native mode"); rc=1; break;
  }
  if(!pass) {test_hook(ds[k]); continue;}
  if(!dib_begin(ds[k]*8)) {logline("could not create the memory bitmap\n"); finish("FAIL: memory bitmap"); rc=1; break;}
  if(!run_depth(codes[k],ds[k])&&!aborted&&!hung) rc=1;
  dib_end();
 }
 }
 if(aborted) {finish("ABORTED"); logline("aborted\n");}
 else if(hung) {finish("FAIL: engine hung"); logline("the engine hung; restart Windows before the next test\n");}
 else if(!rc) {sprintf(resline,"%s: T6 %d sets, %d differ from DIBENG; %d lab engine lines ran, %d hangs",fails6?"FAIL":"PASS",nsets6,fails6,nops,nhang); finish(resline);}
 if(have_orig) {ChangeDisplaySettingsA(&orig,0); logline("desktop put back\n");}
 logline("Finished.\n");
 if(lf) fclose(lf);
 return rc;
}
#endif
