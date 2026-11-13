/* VX2D8 v1.0: measures how the VX855 2D engine behaves at 8 bits per pixel, so that 8bpp acceleration can be written from facts.
 * Unattended: run it, read VX2D8.TXT (or the one-line VX2D8.RES). Needs VX85598.VXD from v10.74 or later (it uses the 2D lab request).
 *
 * The desktop is put at 640x480x32 (the lab window is a 64x64 dword block of that desktop at 448,320) and the engine is then asked, through the lab,
 * to draw into that window with its colour depth register set to a candidate value. The window is read back as bytes, so the depth the engine really
 * used shows as the number of bytes written per pixel.
 *   1  Depth sweep: a 16x4 fill with fg A5A5A5A5 for the depth codes 300 (32bpp, control), 100 (16bpp, control), 000 and 200. Bytes per row written / 16 = bytes per pixel.
 *   2  Suite at every depth that answered as expected (the two controls and the 1 byte per pixel code): fills (odd sizes, odd starts), ROP fills, copies in all
 *      overlap directions, source and destination colour key, clipping. Every operation is compared with a model on the whole window, so a write
 *      outside the rectangle shows up too. The controls (16 and 32 bits, proven on hardware before) check the program itself.
 * Log: VX2D8.TXT; result file: VX2D8.RES (PASS, FAIL or ABORTED). Esc stops. The desktop is put back afterwards. */
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

typedef struct {BYTE b[WINH][WINW];} win_t;
static win_t before,after,snap;
static int dpx; /* bytes per pixel of the depth under test */
static FILE *lf; static int aborted,total_fail,total_pass;
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
static DWORD cmask(void) {return dpx==4?0xFFFFFFUL:pmask();} /* the key compare ignores the top byte at 32 bits */
static DWORD px_get(const win_t *w,int x,int y) {DWORD v=0; int k; for(k=dpx-1;k>=0;k--) v=(v<<8)|w->b[y][x*dpx+k]; return v;}
static void px_put(win_t *w,int x,int y,DWORD v) {int k; for(k=0;k<dpx;k++) {w->b[y][x*dpx+k]=(BYTE)v; v>>=8;}}

#ifdef MOCK
extern int engine_call(const vx3d_request *q,vx3d_report *r); /* the model engine, in the host test */
#else
static HANDLE dev=INVALID_HANDLE_VALUE;
static int engine_call(const vx3d_request *q,vx3d_report *r) {
 DWORD got=0;
 memset(r,0,sizeof(*r));
 if(!DeviceIoControl(dev,VX_IOCTL_3D_TEST,(void*)q,sizeof(*q),r,sizeof(*r),&got,NULL)) {logline("  driver request failed: Windows error %lu\n",GetLastError()); return 0;}
 if(got!=sizeof(*r)||r->magic!=VX3D_MAGIC||r->cb!=sizeof(*r)||r->version!=VX3D_VERSION||r->build!=VX3D_BUILD||r->producer!=VX3D_PRODUCER) {logline("  the driver reply does not match this program (copy VX85598.VXD and VX2D8.EXE from the same package)\n"); return 0;}
 return 1;
}
#endif

typedef struct {DWORD cmd,fg,bg,w,h; int n; DWORD r[10][2];} op_t;
static void addreg(op_t *o,DWORD reg,DWORD v) {o->r[o->n][0]=reg; o->r[o->n][1]=v; o->n++;}
static int hung;
/* run one operation with the depth register set to `mode`; fills before/after (bytes of the 64x64 dword window) */
static int run_op(DWORD mode,const op_t *o) {
 int i;
 memset(&rq,0,sizeof(rq));
 rq.cb=sizeof(rq); rq.version=VX3D_VERSION; rq.arm=VX3D_ARM; rq.reserved=21|(recover_flag?0x100UL:0UL); rq.probe=VX3D_SF_PROBE;
 rq.lab_cmd=o->cmd; rq.lab_mode=mode; rq.lab_w=o->w; rq.lab_h=o->h; rq.lab_fg=o->fg; rq.lab_bg=o->bg; rq.lab_flags=0;
 rq.lab_nregs=(DWORD)o->n; for(i=0;i<o->n;i++) {rq.lab_reg[i][0]=o->r[i][0]; rq.lab_reg[i][1]=o->r[i][1];}
 if(!engine_call(&rq,&rp)) return 0;
 recover_flag=(rp.fault_latched||rp.result==VX3D_DISABLED)?1:0;
 memcpy(before.b,rp.sf_x_raw,sizeof(before.b)); memcpy(after.b,rp.image,sizeof(after.b));
 if(rp.sf_x_info[0][1]) {hung=1; logline("  the engine hung on this operation (%s)\n",rp.sf_x_info[0][2]?"reset worked":"reset FAILED, restart Windows"); return 0;}
 if(rp.result!=VX3D_PASS) {logline("  driver result %lu (not PASS)\n",(DWORD)rp.result); return 0;}
 return 1;
}
static void base_regs(op_t *o,int x,int y) {addreg(o,0x14,WIN_BASE); addreg(o,0x1C,WIN_BASE); addreg(o,0x10,((DWORD)y<<16)|(DWORD)x);}
/* ---- models ---- */
static DWORD rop_apply(DWORD rop,DWORD fg,DWORD d) {
 DWORD m=pmask();
 if(rop==0x00) return 0; if(rop==0xFF) return m; if(rop==0x55) return (~d)&m; if(rop==0x5A) return (fg^d)&m;
 return fg&m;
}
static void model_fill(win_t *m,int x,int y,int w,int h,DWORD rop,DWORD fg,int clip,int cl,int ct,int cr,int cb) {
 int i,j;
 for(j=0;j<h;j++) for(i=0;i<w;i++) {
  int px=x+i,py=y+j;
  if(px<0||py<0||px>=WINW/dpx||py>=WINH) continue;
  if(clip&&(px<cl||px>cr||py<ct||py>cb)) continue;
  px_put(m,px,py,rop_apply(rop,fg,px_get(m,px,py)));
 }
}
/* key: 0 none, 1 source key (skip where the source equals the key), 2 destination key (write only where the destination equals the key) */
static void model_copy(win_t *m,const win_t *src,int sx,int sy,int dx,int dy,int w,int h,int key,DWORD kv) {
 int i,j;
 for(j=0;j<h;j++) for(i=0;i<w;i++) {
  DWORD s=px_get(src,sx+i,sy+j),d=px_get(m,dx+i,dy+j);
  if(key==1&&((s&cmask())==(kv&cmask()))) continue;
  if(key==2&&((d&cmask())!=(kv&cmask()))) continue;
  px_put(m,dx+i,dy+j,s);
 }
}
static int compare(const char *name,const win_t *want) {
 int x,y,bad=0,first=1,pxn=WINW/dpx;
 for(y=0;y<WINH;y++) for(x=0;x<pxn;x++) {
  DWORD a=px_get(&after,x,y),b=px_get(want,x,y);
  if(a!=b) {if(bad<3) logline("    pixel (%d,%d): %lX, expected %lX\n",x,y,a,b); bad++; (void)first;}
 }
 logline("  %s: %s%s\n",name,bad?"FAIL":"PASS",bad?"":"");
 if(bad) {logline("    %d of %d pixels differ from the model\n",bad,pxn*WINH); total_fail++;} else total_pass++;
 return bad==0;
}
static int pump(void) {
#ifndef MOCK
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
#endif
 return !aborted&&!hung;
}
static void t_fill(DWORD mode,const char *name,int x,int y,int w,int h,DWORD rop,int clip,int cl,int ct,int cr,int cb) {
 op_t o; win_t want; DWORD fg=0xA5A5A5A5UL&pmask();
 if(!pump()) return;
 memset(&o,0,sizeof(o)); o.cmd=(rop<<24)|1|0x2000|(clip?0x1000UL:0); o.fg=fg; o.bg=fg; o.w=(DWORD)w; o.h=(DWORD)h;
 base_regs(&o,x,y);
 if(clip) {addreg(&o,0x40,((DWORD)ct<<16)|(DWORD)cl); addreg(&o,0x44,((DWORD)cb<<16)|(DWORD)cr);}
 if(!run_op(mode,&o)) {logline("  %s: FAIL (operation did not run)\n",name); total_fail++; return;}
 want=before; model_fill(&want,x,y,w,h,rop,fg,clip,cl,ct,cr,cb); compare(name,&want);
}
static void t_copy(DWORD mode,const char *name,int sx,int sy,int dx,int dy,int w,int h,int decx,int decy,int key,int keyx,int keyy,int keywin) {
 op_t o; win_t want; DWORD kv=0; int rsx=sx,rsy=sy,rdx=dx,rdy=dy;
 if(!pump()) return;
 /* the key value is read from the picture before the operation: from the source for a source key, from the destination for a destination key */
 memset(&o,0,sizeof(o)); o.cmd=(0xCCUL<<24)|1|(decx?0x8000UL:0)|(decy?0x4000UL:0); o.w=(DWORD)w; o.h=(DWORD)h;
 if(decx) {rdx+=w-1; rsx+=w-1;}
 if(decy) {rdy+=h-1; rsy+=h-1;}
 addreg(&o,0x14,WIN_BASE); addreg(&o,0x1C,WIN_BASE); addreg(&o,0x10,((DWORD)rdy<<16)|(DWORD)rdx); addreg(&o,0x18,((DWORD)rsy<<16)|(DWORD)rsx);
 if(key) {
  op_t probe; memset(&probe,0,sizeof(probe)); probe.cmd=(0xF0UL<<24)|1|0x2000; probe.w=1; probe.h=1; probe.fg=0; base_regs(&probe,WINW/dpx-1,WINH-1);
  /* a 1x1 fill into the last pixel of the window: harmless, and it returns the untouched picture */
  if(!run_op(mode,&probe)) {logline("  %s: FAIL (could not read the picture)\n",name); total_fail++; return;}
  kv=px_get(&before,keyx,keyy);
  if(key==1) {addreg(&o,0x50,kv); addreg(&o,0x48,0x4000);} else {addreg(&o,0x4C,kv); addreg(&o,0x48,0x8000);}
  (void)keywin;
 }
 if(!run_op(mode,&o)) {logline("  %s: FAIL (operation did not run)\n",name); total_fail++; return;}
 snap=before; want=before; model_copy(&want,&snap,sx,sy,dx,dy,w,h,key,kv); compare(name,&want);
}
/* depth under test: `mode` is the value written to register 4, d the bytes per pixel the engine should use */
static void suite(DWORD mode,int d) {
 dpx=d;
 logline("suite at depth code %03lX (%d byte%s per pixel)\n",mode,d,d==1?"":"s");
 t_fill(mode,"fill 16x4 at (8,2)",8,2,16,4,0xF0,0,0,0,0,0);
 t_fill(mode,"fill 1x1 at (5,5)",5,5,1,1,0xF0,0,0,0,0,0);
 t_fill(mode,"fill 3x7 at (3,9)",3,9,3,7,0xF0,0,0,0,0,0);
 t_fill(mode,"fill 13x2 at (1,20)",1,20,13,2,0xF0,0,0,0,0,0);
 t_fill(mode,"fill 31x3 at (7,24)",7,24,31,3,0xF0,0,0,0,0,0);
 t_fill(mode,"rop 00 (black) 9x5 at (10,30)",10,30,9,5,0x00,0,0,0,0,0);
 t_fill(mode,"rop FF (white) 9x5 at (10,30)",10,30,9,5,0xFF,0,0,0,0,0);
 t_fill(mode,"rop 55 (invert) 9x5 at (10,30)",10,30,9,5,0x55,0,0,0,0,0);
 t_fill(mode,"rop 5A (xor) 9x5 at (10,30)",10,30,9,5,0x5A,0,0,0,0,0);
 t_fill(mode,"clipped fill 20x10 at (10,10), clip x 14..19 y 12..15",10,10,20,10,0xF0,1,14,12,19,15);
 t_copy(mode,"copy 21x5 (2,6) -> (30,40)",2,6,30,40,21,5,0,0,0,0,0,0);
 t_copy(mode,"copy 33x5 (1,6) -> (27,50)",1,6,27,50,33,5,0,0,0,0,0,0);
 t_copy(mode,"overlapping copy right, 30x6 (4,10) -> (9,10)",4,10,9,10,30,6,1,0,0,0,0,0);
 t_copy(mode,"overlapping copy down, 30x6 (4,10) -> (4,13)",4,10,4,13,30,6,0,1,0,0,0,0);
 t_copy(mode,"overlapping copy right and down, 30x6 (4,10) -> (9,13)",4,10,9,13,30,6,1,1,0,0,0,0);
 t_copy(mode,"overlapping copy left, 30x6 (9,10) -> (4,10)",9,10,4,10,30,6,0,0,0,0,0,0);
 t_copy(mode,"overlapping copy up, 30x6 (4,13) -> (4,10)",4,13,4,10,30,6,0,0,0,0,0,0);
 t_copy(mode,"source colour key, 21x5 (2,6) -> (30,40), key from (4,8)",2,6,30,40,21,5,0,0,1,4,8,0);
 t_copy(mode,"destination colour key, 21x5 (2,6) -> (30,40), key from (32,42)",2,6,30,40,21,5,0,0,2,32,42,0);
}
/* depth sweep: bytes written per row for a 16 pixel wide fill */
static int sweep(DWORD mode,int *d_out) {
 op_t o; int y,x,rows=0,perrow=-1,same=1,first=-1,spill=0,a5ok=1,total; DWORD fg=0xA5A5A5A5UL;
 dpx=1; *d_out=0;
 if(!pump()) return 0;
 memset(&o,0,sizeof(o)); o.cmd=(0xF0UL<<24)|1|0x2000; o.fg=fg; o.bg=fg; o.w=16; o.h=4; base_regs(&o,8,2);
 if(!run_op(mode,&o)) {logline("code %03lX: the operation did not run\n",mode); return 0;}
 for(y=0;y<WINH;y++) {
  int n=0,f=-1;
  for(x=0;x<WINW;x++) if(after.b[y][x]!=before.b[y][x]) {n++; if(f<0) f=x; if(after.b[y][x]!=0xA5) a5ok=0;}
  if(y>=2&&y<6) {
   rows++; if(perrow<0) {perrow=n; first=f;} else if(perrow!=n||first!=f) same=0;
  } else if(n) spill+=n;
 }
 total=perrow;
 logline("code %03lX: rows written %d, bytes written per row %d, first byte %d, written bytes all A5: %s, changed bytes outside the 4 rows: %d\n",mode,rows,perrow,first,a5ok?"yes":"no",spill);
 if(rows==4&&same&&perrow>0&&perrow%16==0&&first==8*(perrow/16)&&a5ok&&!spill) {*d_out=perrow/16; logline("  -> %d byte%s per pixel\n",*d_out,*d_out==1?"":"s"); return 1;}
 if(perrow==0&&!spill) logline("  -> nothing was drawn in the window at this code\n");
 else {
  int shown=0;
  logline("  -> %s\n",perrow==0?"the fill landed elsewhere in the window (position or pitch differs at this code)":"not a clean rectangle at any whole number of bytes per pixel");
  for(y=0;y<WINH&&shown<12;y++) {
   int n=0,f=-1,l=-1;
   for(x=0;x<WINW;x++) if(after.b[y][x]!=before.b[y][x]) {n++; if(f<0) f=x; l=x;}
   if(n) {logline("     row %d: %d bytes changed, bytes %d..%d\n",y,n,f,l); shown++;}
  }
 }
 (void)total;
 return 1;
}
#ifndef MOCK
static void finish(const char *res) {
 FILE *f=fopen("VX2D8.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}
}
#endif
static int run_all(void) {
 static const DWORD codes[4]={0x300,0x100,0x000,0x200};
 static const int want_d[4]={4,2,1,0};
 int d[4],k,found8=-1;
 logline("depth sweep (control codes first)\n");
 for(k=0;k<4&&pump();k++) {
  if(!sweep(codes[k],&d[k])) d[k]=0;
 }
 if(aborted) return 2;
 if(hung) return 3;
 for(k=0;k<4;k++) if(codes[k]!=0x300&&codes[k]!=0x100&&d[k]==1&&found8<0) found8=k;
 if(d[0]!=4||d[1]!=2) logline("WARNING: the control codes did not give 4 and 2 bytes per pixel (%d and %d): the program or the engine state is not as expected, treat the 8bpp result with caution\n",d[0],d[1]);
 (void)want_d;
 for(k=0;k<4&&!aborted&&!hung;k++) if(d[k]&&(codes[k]==0x300||codes[k]==0x100||k==found8)) suite(codes[k],d[k]);
 if(aborted) return 2;
 if(hung) return 3;
 if(found8<0) {
  logline("RESULT: no depth code made the engine write 1 byte per pixel (codes tried: 000, 200)\n");
  sprintf(resline,"FAIL: no 1 byte per pixel depth code (000 gave %d, 200 gave %d bytes per pixel)",d[2],d[3]);
  return 1;
 }
 logline("RESULT: depth code %03lX gives 1 byte per pixel; %d checks passed, %d failed in all suites\n",codes[found8],total_pass,total_fail);
 sprintf(resline,"%s: 8bpp depth code %03lX, %d checks passed, %d failed",total_fail?"FAIL":"PASS",codes[found8],total_pass,total_fail);
 return total_fail?1:0;
}
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
 lf=fopen("VX2D8.TXT","w");
 logline("VX2D8 v1.0 / driver v10.75\n");
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
