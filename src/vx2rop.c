/* VX2ROP v1.0: which raster operations the 2D engine really does. The driver accelerates only PATCOPY, PATINVERT, DSTINVERT, BLACKNESS, WHITENESS and SRCCOPY;
 * GDI sprite code uses SRCAND, SRCPAINT and SRCINVERT, and fills use PATPAINT, MERGECOPY and the rest. This program runs all 256 ROP3 codes through the engine
 * at 32, 16 and 8 bits per pixel and compares every pixel with a software ROP3 (result = ROP3(P, S, D), bit by bit):
 *   T1  all 256 ROPs, command rop|2001 (fixed pattern colour in register 58, source on screen), source and destination rectangles apart
 *   T2  the 16 ROPs that do not use P, command rop|0001 (no fixed colour): the plain copy form
 *   T3  the 16 ROPs that do not use S, colour pattern from video memory (command rop|0001, register 20 = block address, offsets = screen phase)
 *   T4  the 16 ROPs that do not use S, mono pattern fill (command rop|0A01, registers 24 and 28 = pattern, 58 = 1 bits, 5C = 0 bits)
 *   T5  the 16 ROPs that do not use S, mono pattern fill with transparent background (command rop|10A01), 0 pixels must be left alone
 * Everything happens in the 64x64 dword scratch surface (video memory at the lab's target address), filled with pseudo-random data by the lab (lab_data, 4 KB):
 * rows 0..7 destination, rows 8..13 source, row 14 the 8x8 colour block. Nothing on the desktop is touched. Needs VX85598.VXD from v10.74 or later.
 * Unattended: run it, read VX2ROP.TXT (or the one-line VX2ROP.RES). The line before every operation is flushed to disk, so after a freeze the last line names the command.
 * Log: VX2ROP.TXT; result file: VX2ROP.RES (PASS when every operation ran, FAIL when the engine hung, ABORTED). Esc stops. The desktop is put back afterwards. */
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

#define TGT ((DWORD)VX3D_TARGET) /* the lab writes lab_data here (video memory byte address) */

static int dpx; /* bytes per pixel of the depth under test */
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
extern int engine_call(const vx3d_request *q,vx3d_report *r); /* the model engine, in the host test */
#else
static HANDLE dev=INVALID_HANDLE_VALUE;
static int engine_call(const vx3d_request *q,vx3d_report *r) {
 DWORD got=0;
 memset(r,0,sizeof(*r));
 if(!DeviceIoControl(dev,VX_IOCTL_3D_TEST,(void*)q,sizeof(*q),r,sizeof(*r),&got,NULL)) {logline("  driver request failed: Windows error %lu\n",GetLastError()); return 0;}
 if(got!=sizeof(*r)||r->magic!=VX3D_MAGIC||r->cb!=sizeof(*r)||r->version!=VX3D_VERSION||r->build!=VX3D_BUILD||r->producer!=VX3D_PRODUCER) {logline("  the driver reply does not match this program (copy VX85598.VXD and VX2ROP.EXE from the same package)\n"); return 0;}
 return 1;
}
#endif


typedef struct {DWORD cmd,fg,bg,w,h; int n; DWORD r[12][2]; int nd; unsigned int data[1024];} op_t;
static void addreg(op_t *o,DWORD reg,DWORD v) {o->r[o->n][0]=reg; o->r[o->n][1]=v; o->n++;}
static int hung,nhang;
static int run_op(DWORD mode,const op_t *o) {
 int i;
 memset(&rq,0,sizeof(rq));
 rq.cb=sizeof(rq); rq.version=VX3D_VERSION; rq.arm=VX3D_ARM; rq.reserved=21|(recover_flag?0x100UL:0UL); rq.probe=VX3D_SF_PROBE;
 rq.lab_cmd=o->cmd; rq.lab_mode=mode; rq.lab_w=o->w; rq.lab_h=o->h; rq.lab_fg=o->fg; rq.lab_bg=o->bg; rq.lab_flags=1024UL|16UL; rq.lab_ndata=(DWORD)o->nd;
 for(i=0;i<o->nd;i++) rq.lab_data[i]=o->data[i];
 rq.lab_nregs=(DWORD)o->n; for(i=0;i<o->n;i++) {rq.lab_reg[i][0]=o->r[i][0]; rq.lab_reg[i][1]=o->r[i][1];}
 if(!engine_call(&rq,&rp)) return 0;
 recover_flag=(rp.fault_latched||rp.result==VX3D_DISABLED)?1:0;
 if(rp.sf_x_info[0][1]) {nhang++; logline("  HANG: the engine hung on this operation (%s)\n",rp.sf_x_info[0][2]?"reset worked":"reset FAILED, restart Windows"); if(!rp.sf_x_info[0][2]||nhang>=4) hung=1; return 0;}
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

#define ROWB 256UL /* bytes per scratch row */
static unsigned int mem0[1024]; /* the data the lab wrote */
static unsigned long rs;
static unsigned long rnd(void) {rs^=rs<<13; rs&=0xFFFFFFFFUL; rs^=rs>>17; rs^=rs<<5; rs&=0xFFFFFFFFUL; return rs;}
static int tests;
static DWORD M(void) {return dpx==1?0xFFUL:dpx==2?0xFFFFUL:0xFFFFFFFFUL;}
static DWORD bget(const unsigned int *w,DWORD byteoff) { /* little-endian bytes of the dword array */
 return (w[byteoff>>2]>>((byteoff&3)*8))&0xFF;
}
static DWORD pget(const unsigned int *w,DWORD row,DWORD x) {DWORD v=0; int k; for(k=dpx-1;k>=0;k--) v=(v<<8)|bget(w,row*ROWB+x*(DWORD)dpx+(DWORD)k); return v;}
static DWORD rop3(DWORD rop,DWORD P,DWORD S,DWORD D) {
 DWORD r=0; int i;
 for(i=0;i<8;i++) if((rop>>i)&1) r|=((i&4)?P:~P)&((i&2)?S:~S)&((i&1)?D:~D);
 return r&M();
}
static int uses_s(DWORD rop) {int i; for(i=0;i<8;i++) if(((rop>>i)&1)!=((rop>>(i^2))&1)) return 1; return 0;}
static int uses_p(DWORD rop) {int i; for(i=0;i<8;i++) if(((rop>>i)&1)!=((rop>>(i^4))&1)) return 1; return 0;}
static void fill_data(op_t *o,int d) {
 int i; DWORD k,b; unsigned char *p;
 o->nd=1024; for(i=0;i<1024;i++) o->data[i]=(unsigned int)rnd();
 /* the colour block in row 14: 64 distinct-ish pixels */
 p=(unsigned char*)&o->data[14*64]; for(k=0;k<64;k++) for(b=0;(int)b<d;b++) p[k*(DWORD)d+b]=(unsigned char)(rnd()>>7);
 for(i=0;i<1024;i++) mem0[i]=o->data[i];
}
#define DX0 3UL
#define DY0 1UL
#define SX0 3UL
#define SY0 9UL
#define RH 5UL
#define PITCHREG ((32UL<<16)|32UL)
#define BASEREG (TGT>>3)
static int rect_w(void) {return (int)(ROWB/(DWORD)dpx)-8;}
/* one operation. kind: 1 fixed colour copy, 2 plain copy, 3 colour block, 4 mono, 5 mono transparent */
static int run_one(DWORD mode,int kind,DWORD rop,int *mism,int *spill,DWORD *efirst,DWORD *gfirst,int *xf,int *yf,DWORD *afirst) {
 op_t o; int x,y,w=rect_w(),bad=0,sp=0; DWORD P,S,D,e,g,pc=0x5A3CC3A5UL&M(),blkoff=14UL*ROWB; DWORD m0=0x6BA5D234UL,m1=0xE1B49A5CUL,pbg=0x1F2E3D4CUL&M();
 unsigned int *img;
 if(!pump()) return 0;
 memset(&o,0,sizeof(o)); fill_data(&o,dpx);
 o.w=(DWORD)w; o.h=RH; o.fg=pc; o.bg=pbg;
 addreg(&o,0x08,PITCHREG); addreg(&o,0x14,BASEREG); addreg(&o,0x1C,BASEREG);
 addreg(&o,0x10,(DY0<<16)|DX0); addreg(&o,0x18,(SY0<<16)|SX0);
 addreg(&o,0x58,pc); addreg(&o,0x5C,pbg); addreg(&o,0x4C,pc); addreg(&o,0x50,pbg);
 if(kind==1) o.cmd=(rop<<24)|0x2001UL;
 else if(kind==2) o.cmd=(rop<<24)|0x0001UL;
 else if(kind==3) {o.cmd=(rop<<24)|0x0001UL; addreg(&o,0x20,((TGT+blkoff)>>3)|((DY0&7UL)<<29)|((DX0&7UL)<<26));}
 else {o.cmd=(rop<<24)|0x0A01UL|(kind==5?0x10000UL:0UL); addreg(&o,0x24,m0); addreg(&o,0x28,m1); addreg(&o,0x20,0);}
 logline("  op kind %d rop %02lX cmd %08lX\n",kind,rop,o.cmd);
 if(!run_op(mode,&o)) return 0;
 img=(unsigned int*)rp.image;
 tests++;
 *mism=0; *spill=0; *efirst=0; *gfirst=0; *xf=-1; *yf=-1; *afirst=0;
 for(y=0;y<16;y++) for(x=0;x<(int)(ROWB/(DWORD)dpx);x++) {
  int inside=(x>=(int)DX0&&x<(int)DX0+w&&y>=(int)DY0&&y<(int)(DY0+RH));
  D=pget(mem0,(DWORD)y,(DWORD)x); g=pget(img,(DWORD)y,(DWORD)x);
  if(!inside) {if(g!=D) sp++; continue;}
  S=(kind==1||kind==2)?pget(mem0,SY0+(DWORD)(y-(int)DY0),SX0+(DWORD)(x-(int)DX0)):0;
  if(kind==3) {int kk=((y&7)*8+(x&7)); DWORD v=0; int b; for(b=dpx-1;b>=0;b--) v=(v<<8)|bget(mem0,blkoff+(DWORD)(kk*dpx+b)); P=v&M();}
  else if(kind==4||kind==5) {DWORD row=(DWORD)((y-(int)DY0)&7); DWORD bits=(row<4?m0:m1)>>((row&3)*8)&0xFF; int bit=(bits>>(7-(x&7)))&1; P=bit?pc:pbg;
   if(kind==5&&!bit) {e=D; goto cmp;}}
  else P=pc;
  e=rop3(rop,P,S,D);
 cmp:
  if((g&M())!=(e&M())) {if(!bad) {*efirst=e; *gfirst=g; *xf=x; *yf=y; *afirst=D;} bad++;}
 }
 *mism=bad; *spill=sp;
 return 1;
}
static const char *kn[6]={"","T1 fixed colour source+pattern","T2 plain copy","T3 colour block pattern","T4 mono pattern","T5 mono pattern, transparent"};
static int run_set(DWORD mode,int d,int kind) {
 DWORD rop; int n=0,ok=0,i,nfail=0; char list[1200]; int ll=0;
 dpx=d; list[0]=0;
 logline("-- %s, depth code %03lX (%d byte%s per pixel) --\n",kn[kind],mode,d,d==1?"":"s");
 for(i=0;i<256&&pump();i++) {
  int mism,spill,xf,yf; DWORD e,g,dd;
  rop=(DWORD)i;
  if(kind==2&&uses_p(rop)) continue;
  if(kind>=3&&uses_s(rop)) continue;
  if(!run_one(mode,kind,rop,&mism,&spill,&e,&g,&xf,&yf,&dd)) {if(hung) return 0; if(aborted) return 0; continue;}
  n++;
  if(!mism&&!spill) ok++;
  else {
   nfail++;
   if(ll<1100) ll+=sprintf(list+ll," %02lX",rop);
   if(nfail<=40) logline("  ROP %02lX: %d pixels differ, %d changed outside; first at (%d,%d): expected %lX got %lX (destination was %lX)\n",rop,mism,spill,xf,yf,e,g,dd);
  }
 }
 logline("  %s, %d byte%s per pixel: %d ROPs run, %d match the software ROP3%s%s\n",kn[kind],d,d==1?"":"s",n,ok,nfail?"; failing:":"",list);
 return 1;
}
static char resline[200];
static int run_all(void) {
 static const DWORD codes[3]={0x300,0x100,0x000}; static const int ds[3]={4,2,1};
 int k,kind; rs=0x2545F491UL;
 for(kind=1;kind<=5;kind++) for(k=0;k<3&&pump();k++) if(!run_set(codes[k],ds[k],kind)) break;
 if(aborted) return 2;
 if(hung) return 3;
 logline("RESULT: %d operations ran, %d hung the engine (each was recovered)\n",tests,nhang);
 sprintf(resline,"%s: %d operations ran, %d hangs (see VX2ROP.TXT for the ROPs that differ)",nhang?"PASS with hangs":"PASS",tests,nhang);
 return 0;
}
#ifndef MOCK
static void finish(const char *res) {FILE *f=fopen("VX2ROP.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
#endif
#ifdef MOCK
int main(void) {int rc; lf=stdout; rc=run_all(); printf("%s\nrc=%d\n",resline,rc); return rc;}
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
 lf=fopen("VX2ROP.TXT","w");
 logline("VX2ROP v1.0 / driver v10.82\n");
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
