/* VX2COL v1.0: measures the 2D engine's colour pattern fill from video memory (command F0000001, pattern address in register 20), at 8, 16 and 32 bits per pixel,
 * so that colour and hatch brushes can be drawn by the engine without the colour pattern RAM (which has a hidden row counter).
 * Unattended: run it, read VX2COL.TXT (or the one-line VX2COL.RES). Needs VX85598.VXD from v10.74 or later (it uses the 2D lab request).
 *
 * The desktop is put at 640x480x32 (the lab window is a 64x64 dword block of that desktop at 448,320). For each depth (32, 16, 8 bits per pixel) an
 * 8x8 block of distinct pixels is written into video memory (the lab's scratch area) and the engine is asked to fill rectangles from it.
 * Every result is fitted against all 8 block orientations (transpose and mirrors) and all 64 phases; the log gives the fit, whether the pattern is
 * anchored to the screen or to the rectangle, what the X/Y offset bits of register 20 do, whether the block address needs alignment, whether a block
 * rewritten by the CPU is seen by the next fill (cache), what ROP 5A and a clip rectangle do, and whether a monochrome fill after a colour fill is intact.
 * Log: VX2COL.TXT; result file: VX2COL.RES (PASS when every fill was explained by one layout rule, FAIL, or ABORTED). Esc stops. The desktop is put back afterwards. */
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

#define WINW 256
#define WINH 64
#define WIN_BASE ((320UL*2560UL+448UL*4UL)>>3) /* registers 0x14 and 0x1C count 8-byte units */
#define TGT ((DWORD)VX3D_TARGET) /* the lab writes lab_data here (video memory byte address) */

typedef struct {BYTE b[WINH][WINW];} win_t;
static win_t before,after;
static int dpx; /* bytes per pixel of the depth under test */
static FILE *lf; static int aborted;
static char resline[200];
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
static DWORD pmask(void) {return dpx==1?0xFFUL:dpx==2?0xFFFFUL:0xFFFFFFFFUL;}
static DWORD px_get(const win_t *w,int x,int y) {DWORD v=0; int k; for(k=dpx-1;k>=0;k--) v=(v<<8)|w->b[y][x*dpx+k]; return v;}

#ifdef MOCK
extern int engine_call(const vx3d_request *q,vx3d_report *r); /* the model engine, in the host test */
#else
static HANDLE dev=INVALID_HANDLE_VALUE;
static int engine_call(const vx3d_request *q,vx3d_report *r) {
 DWORD got=0;
 memset(r,0,sizeof(*r));
 if(!DeviceIoControl(dev,VX_IOCTL_3D_TEST,(void*)q,sizeof(*q),r,sizeof(*r),&got,NULL)) {logline("  driver request failed: Windows error %lu\n",GetLastError()); return 0;}
 if(got!=sizeof(*r)||r->magic!=VX3D_MAGIC||r->cb!=sizeof(*r)||r->version!=VX3D_VERSION||r->build!=VX3D_BUILD||r->producer!=VX3D_PRODUCER) {logline("  the driver reply does not match this program (copy VX85598.VXD and VX2COL.EXE from the same package)\n"); return 0;}
 return 1;
}
#endif


typedef struct {DWORD cmd,fg,bg,w,h; int n; DWORD r[12][2]; int nd; unsigned int data[1024];} op_t;
static void addreg(op_t *o,DWORD reg,DWORD v) {o->r[o->n][0]=reg; o->r[o->n][1]=v; o->n++;}
static int hung;
static int run_op(DWORD mode,const op_t *o) {
 int i;
 memset(&rq,0,sizeof(rq));
 rq.cb=sizeof(rq); rq.version=VX3D_VERSION; rq.arm=VX3D_ARM; rq.reserved=21|(recover_flag?0x100UL:0UL); rq.probe=VX3D_SF_PROBE;
 rq.lab_cmd=o->cmd; rq.lab_mode=mode; rq.lab_w=o->w; rq.lab_h=o->h; rq.lab_fg=o->fg; rq.lab_bg=o->bg; rq.lab_flags=o->nd?1024UL:0UL; rq.lab_ndata=(DWORD)o->nd;
 for(i=0;i<o->nd;i++) rq.lab_data[i]=o->data[i];
 rq.lab_nregs=(DWORD)o->n; for(i=0;i<o->n;i++) {rq.lab_reg[i][0]=o->r[i][0]; rq.lab_reg[i][1]=o->r[i][1];}
 if(!engine_call(&rq,&rp)) return 0;
 recover_flag=(rp.fault_latched||rp.result==VX3D_DISABLED)?1:0;
 memcpy(before.b,rp.sf_x_raw,sizeof(before.b)); memcpy(after.b,rp.image,sizeof(after.b));
 if(rp.sf_x_info[0][1]) {hung=1; logline("  the engine hung on this operation (%s)\n",rp.sf_x_info[0][2]?"reset worked":"reset FAILED, restart Windows"); return 0;}
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
/* ---- the block and the layout model ---- */
typedef struct {int t,mx,my,px,py;} lay_t;
static DWORD blk[64];
static void make_block(int d,DWORD seed) {
 int k;
 for(k=0;k<64;k++) {
  DWORD kk=(DWORD)k+seed*7UL;
  if(d==1) blk[k]=(0x10UL+kk*3UL)&0xFFUL;
  else if(d==2) blk[k]=(0x0123UL+kk*0x0405UL)&0xFFFFUL;
  else blk[k]=0xFF000000UL|((0x102030UL+kk*0x030507UL)&0xFFFFFFUL);
 }
 /* distinct values within the block (seed shifts the whole set) */
}
static int lay_idx(const lay_t *l,int x,int y) {
 int u=(x+l->px)&7,v=(y+l->py)&7,s;
 if(l->mx) u=7-u; if(l->my) v=7-v;
 if(l->t) {s=u; u=v; v=s;}
 return v*8+u;
}
/* pack the block as bytes at dword index j of the data array; junk before it */
static void pack_block(op_t *o,int d,int j) {
 int i,k,b; BYTE *p;
 o->nd=j+16*d; for(i=0;i<o->nd;i++) o->data[i]=0xC3C3C3C3U^(unsigned int)i;
 p=(BYTE*)&o->data[j];
 for(k=0;k<64;k++) for(b=0;b<d;b++) p[k*d+b]=(BYTE)(blk[k]>>(8*b));
}
#define WP (WINW/dpx)
static int in_rect(int x,int y,int x0,int y0,int w,int h,int useclip,int cl,int ct,int cr,int cb) {
 if(x<x0||y<y0||x>=x0+w||y>=y0+h) return 0;
 if(useclip&&(x<cl||x>cr||y<ct||y>cb)) return 0;
 return 1;
}
static DWORD expect(int xorflag,int x,int y,const lay_t *l) {
 DWORD p=blk[lay_idx(l,x,y)];
 return xorflag?((px_get(&before,x,y)^p)&pmask()):p;
}
/* count layouts (of 512) that explain every pixel of the rectangle; first one returned */
static int fit(int x0,int y0,int w,int h,int useclip,int cl,int ct,int cr,int cb,int xorflag,lay_t *first) {
 int t,mx,my,px,py,x,y,n=0;
 for(t=0;t<2;t++) for(mx=0;mx<2;mx++) for(my=0;my<2;my++) for(px=0;px<8;px++) for(py=0;py<8;py++) {
  lay_t l; int ok=1; l.t=t; l.mx=mx; l.my=my; l.px=px; l.py=py;
  for(y=y0;y<y0+h&&ok;y++) for(x=x0;x<x0+w;x++) {
   if(!in_rect(x,y,x0,y0,w,h,useclip,cl,ct,cr,cb)) continue;
   if(px_get(&after,x,y)!=expect(xorflag,x,y,&l)) {ok=0; break;}
  }
  if(ok) {if(!n) *first=l; n++;}
 }
 return n;
}
static int spill(int x0,int y0,int w,int h,int useclip,int cl,int ct,int cr,int cb) {
 int x,y,n=0;
 for(y=0;y<WINH;y++) for(x=0;x<WP;x++) if(!in_rect(x,y,x0,y0,w,h,useclip,cl,ct,cr,cb)&&px_get(&after,x,y)!=px_get(&before,x,y)) n++;
 return n;
}
typedef struct {int ok; lay_t l; int x0,y0,xo,yo,kind;} res_t;
static res_t res[64]; static int nres;
static int fails; /* fills not explained by any layout, or with spill */
static void show_tile(int x0,int y0) {
 int x,y;
 logline("    observed 8x8 at the rectangle's corner (hex):\n");
 for(y=0;y<8;y++) {logline("     "); for(x=0;x<8;x++) logline(" %lX",px_get(&after,x0+x,y0+y)); logline("\n");}
 logline("    block in memory order (hex), row by row:\n");
 for(y=0;y<8;y++) {logline("     "); for(x=0;x<8;x++) logline(" %lX",blk[y*8+x]); logline("\n");}
}
/* one colour pattern fill. kind: 0 plain, 1 offsets, 2 alignment, 3 rop 5A, 4 clip, 5 sequence */
static int pat_fill(DWORD mode,int kind,const char *name,int x0,int y0,int w,int h,int xo,int yo,int j,DWORD rop,int useclip,int cl,int ct,int cr,int cb,lay_t *out) {
 op_t o; int n,sp; lay_t l;
 if(!pump()) return 0;
 memset(&o,0,sizeof(o)); pack_block(&o,dpx,j);
 o.cmd=(rop<<24)|1|(useclip?0x1000UL:0); o.w=(DWORD)w; o.h=(DWORD)h; o.fg=0x13579BDFUL&pmask(); o.bg=0;
 addreg(&o,0x14,WIN_BASE); addreg(&o,0x1C,WIN_BASE); addreg(&o,0x10,((DWORD)y0<<16)|(DWORD)x0);
 addreg(&o,0x20,((TGT+4UL*(DWORD)j)>>3)|((DWORD)yo<<29)|((DWORD)xo<<26));
 if(useclip) {addreg(&o,0x40,((DWORD)ct<<16)|(DWORD)cl); addreg(&o,0x44,((DWORD)cb<<16)|(DWORD)cr);}
 if(!run_op(mode,&o)) {logline("  %s: FAIL (operation did not run)\n",name); fails++; return 0;}
 n=fit(x0,y0,w,h,useclip,cl,ct,cr,cb,rop==0x5A,&l);
 sp=spill(x0,y0,w,h,useclip,cl,ct,cr,cb);
 if(n) {
  logline("  %s: fit transpose %d mirrorX %d mirrorY %d phase (%d,%d) [%d layout%s fit]%s\n",name,l.t,l.mx,l.my,l.px,l.py,n,n==1?"":"s",sp?", SPILL":"");
  if(sp) logline("    %d pixels changed outside the rectangle\n",sp);
  if(nres<64) {res[nres].ok=1; res[nres].l=l; res[nres].x0=x0; res[nres].y0=y0; res[nres].xo=xo; res[nres].yo=yo; res[nres].kind=kind; nres++;}
  if(sp) fails++;
  if(out) *out=l;
  return 1;
 }
 logline("  %s: no layout explains the result\n",name); fails++;
 show_tile(x0,y0);
 if(nres<64) {res[nres].ok=0; res[nres].kind=kind; res[nres].x0=x0; res[nres].y0=y0; res[nres].xo=xo; res[nres].yo=yo; nres++;}
 return 0;
}
static int same_xform(const lay_t *a,const lay_t *b) {return a->t==b->t&&a->mx==b->mx&&a->my==b->my;}
/* the core tests at one depth */
static lay_t base;
static void core(DWORD mode,int d) {
 lay_t l; dpx=d; nres=0;
 logline("-- depth code %03lX (%d byte%s per pixel), block %d bytes --\n",mode,d,d==1?"":"s",64*d);
 make_block(d,0);
 pat_fill(mode,0,"A  24x16 at (0,0), offsets 0,0",0,0,24,16,0,0,0,0xF0,0,0,0,0,0,&base);
 pat_fill(mode,0,"B1 13x11 at (3,5), offsets 0,0",3,5,13,11,0,0,0,0xF0,0,0,0,0,0,&l);
 pat_fill(mode,0,"B2 17x9 at (6,20), offsets 0,0",6,20,17,9,0,0,0,0xF0,0,0,0,0,0,&l);
 pat_fill(mode,0,"B3 8x8 at (16,32), offsets 0,0",16,32,8,8,0,0,0,0xF0,0,0,0,0,0,&l);
 pat_fill(mode,1,"C1 24x16 at (0,0), x offset 3",0,0,24,16,3,0,0,0xF0,0,0,0,0,0,&l);
 pat_fill(mode,1,"C2 24x16 at (0,0), y offset 5",0,0,24,16,0,5,0,0xF0,0,0,0,0,0,&l);
 pat_fill(mode,1,"C3 24x16 at (0,0), offsets 6,2",0,0,24,16,6,2,0,0xF0,0,0,0,0,0,&l);
 pat_fill(mode,1,"C4 13x11 at (3,5), offsets 6,2",3,5,13,11,6,2,0,0xF0,0,0,0,0,0,&l);
 {int js[6]={2,4,8,16,32,64},k; char nm[80];
  for(k=0;k<6;k++) {sprintf(nm,"D%d block at +%d bytes (24x16 at (0,0))",k+1,js[k]*4); pat_fill(mode,2,nm,0,0,24,16,0,0,js[k],0xF0,0,0,0,0,0,&l);}
 }
}
/* after a sequence fill that no layout explains: does the PREVIOUS block explain it (the engine kept a stale pattern)? */
static void stale_check(int d,DWORD prev_seed,int x0,int y0,int w,int h) {
 lay_t l; int n;
 make_block(d,prev_seed); n=fit(x0,y0,w,h,0,0,0,0,0,0,&l);
 if(n) logline("    the result matches the PREVIOUS block (seed %lu): the engine used a stale pattern, the block rewritten by the CPU was not seen\n",(unsigned long)prev_seed);
}
/* the extra tests at one depth: rop 5A, clip, a rewritten block, a monochrome fill after a colour fill */
static void extra(DWORD mode,int d) {
 lay_t l,l2; dpx=d;
 logline("-- extra tests, depth code %03lX (%d byte%s per pixel) --\n",mode,d,d==1?"":"s");
 make_block(d,0);
 pat_fill(mode,4,"E1 clip: 24x16 at (0,0), clip x 5..17 y 3..11",0,0,24,16,0,0,0,0xF0,1,5,3,17,11,&l);
 /* sequence: a different block at the same address; the next fill must show the new block (no stale pattern cache) */
 make_block(d,1); if(!pat_fill(mode,5,"F1 new block (seed 1) at the same address",0,0,24,16,0,0,0,0xF0,0,0,0,0,0,&l)) stale_check(d,0,0,0,24,16);
 make_block(d,2); if(!pat_fill(mode,5,"F2 new block (seed 2) at the same address, elsewhere",9,14,24,16,0,0,0,0xF0,0,0,0,0,0,&l2)) {stale_check(d,1,9,14,24,16);}
 /* monochrome after colour: all-ones pattern must give a solid foreground */
 if(pump()) {
  op_t o; int x,y,bad=0,pxn=WP; DWORD fg=0x2468ACE0UL&pmask();
  memset(&o,0,sizeof(o)); o.cmd=0xF0000A01UL; o.fg=fg; o.bg=0; o.w=20; o.h=10;
  addreg(&o,0x14,WIN_BASE); addreg(&o,0x1C,WIN_BASE); addreg(&o,0x10,(4UL<<16)|2UL); addreg(&o,0x24,0xFFFFFFFFUL); addreg(&o,0x28,0xFFFFFFFFUL);
  if(!run_op(mode,&o)) {logline("  G  monochrome fill after colour fills: FAIL (operation did not run)\n"); fails++;}
  else {
   for(y=0;y<WINH;y++) for(x=0;x<pxn;x++) {
    int inside=(x>=2&&x<22&&y>=4&&y<14);
    DWORD want=inside?fg:px_get(&before,x,y);
    if(px_get(&after,x,y)!=want) bad++;
   }
   logline("  G  monochrome all-ones fill after colour fills: %s%s\n",bad?"FAIL":"PASS",bad?" (pixels differ)":"");
   if(bad) fails++;
  }
 }
 /* rop 5A last: it may be refused by the engine */
 make_block(d,0);
 pat_fill(mode,3,"H1 rop 5A (xor), 24x16 at (0,0)",0,0,24,16,0,0,0,0x5A,0,0,0,0,0,&l);
}
static void summary(DWORD mode,int d,int idx0,int idx1) {
 int i; lay_t b; int have=0,allsame=1;
 (void)mode;
 for(i=idx0;i<idx1;i++) if(res[i].ok&&res[i].kind==0&&res[i].x0==0&&res[i].y0==0) {b=res[i].l; have=1; break;}
 if(!have) {logline("summary %d byte%s per pixel: no base layout (test A did not fit)\n",d,d==1?"":"s"); return;}
 for(i=idx0;i<idx1;i++) if(res[i].ok&&!same_xform(&res[i].l,&b)) allsame=0;
 logline("summary %d byte%s per pixel: base layout transpose %d mirrorX %d mirrorY %d phase (%d,%d); orientation %s in all fitted fills\n",d,d==1?"":"s",b.t,b.mx,b.my,b.px,b.py,allsame?"the same":"NOT the same");
 for(i=idx0;i<idx1;i++) if(res[i].ok&&res[i].kind==0&&(res[i].x0||res[i].y0)) {
  int sx=(res[i].l.px==b.px&&res[i].l.py==b.py), rx=(((res[i].l.px+res[i].x0)&7)==b.px&&((res[i].l.py+res[i].y0)&7)==b.py);
  logline("  rectangle at (%d,%d): pattern is %s\n",res[i].x0,res[i].y0,sx?"anchored to the screen":rx?"anchored to the rectangle corner":"neither screen- nor rectangle-anchored (see fits)");
 }
 for(i=idx0;i<idx1;i++) if(res[i].ok&&res[i].kind==1) {
  int dx=(res[i].l.px-b.px)&7,dy=(res[i].l.py-b.py)&7,xo=res[i].xo,yo=res[i].yo;
  /* the phase of a rectangle at (x0,y0) only differs from the base by the offsets when anchored to the screen */
  logline("  offsets (%d,%d) at (%d,%d): phase shift (%d,%d): %s\n",xo,yo,res[i].x0,res[i].y0,dx,dy,
   (dx==(xo&7)&&dy==(yo&7))?"phase = base + offset":(dx==((8-xo)&7)&&dy==((8-yo)&7))?"phase = base - offset":"other relation");
 }
}
static int run_all(void) {
 static const DWORD codes[3]={0x300,0x100,0x000};
 static const int ds[3]={4,2,1};
 int k;
 for(k=0;k<3&&pump();k++) {core(codes[k],ds[k]); summary(codes[k],ds[k],0,nres);} /* res[] restarts at every depth */
 for(k=0;k<3&&pump();k++) extra(codes[k],ds[k]);
 if(aborted) return 2;
 if(hung) return 3;
 if(fails) {logline("RESULT: %d fills were not explained by one layout rule (or spilled) - see the lines above\n",fails); sprintf(resline,"FAIL: %d fills not explained",fails); return 1;}
 logline("RESULT: every colour pattern fill from video memory matched one layout rule at 32, 16 and 8 bits per pixel\n");
 sprintf(resline,"PASS: colour pattern from video memory works at 32, 16 and 8 bpp");
 return 0;
}
#ifndef MOCK
static void finish(const char *res) {
 FILE *f=fopen("VX2COL.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}
}
#endif
#ifdef MOCK
int main(void) {
 int rc; lf=stdout; rc=run_all(); printf("%s\nrc=%d\n",resline,rc); return rc;
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
 vx_status_t s; DWORD got=0,t; int rc=1,have_orig; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2COL.TXT","w");
 logline("VX2COL v1.0 / driver v10.76\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 memset(&orig,0,sizeof(orig)); orig.dmSize=sizeof(orig); have_orig=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&orig)?1:0;
 logline("desktop was %lux%lux%lu\n",have_orig?orig.dmPelsWidth:0,have_orig?orig.dmPelsHeight:0,have_orig?orig.dmBitsPerPel:0);
 if(!set_mode(640,480,32)) {logline("could not set 640x480x32\n"); finish("FAIL: 640x480x32 not available"); return 1;}
 t=GetTickCount(); while(GetTickCount()-t<800) Sleep(20);
 memset(&s,0,sizeof(s));
 if(!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||s.width!=640||s.height!=480||s.bpp!=32||s.stage!=6||s.failure||s.vga_mode) {
  logline("the driver is not in native 640x480x32 mode\n"); finish("FAIL: not native 640x480x32"); if(have_orig) ChangeDisplaySettingsA(&orig,0); return 1;
 }
 rc=run_all();
 if(rc==2) {finish("ABORTED"); logline("aborted\n");}
 else if(rc==3) {finish("FAIL: engine hung"); logline("the engine hung; restart Windows before the next test\n");}
 else finish(resline);
 if(have_orig) {ChangeDisplaySettingsA(&orig,0); logline("desktop put back\n");}
 logline("Finished.\n");
 if(lf) fclose(lf);
 return rc;
}
#endif
