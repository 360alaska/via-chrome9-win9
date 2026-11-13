/* VX2TRN v1.0: measures which command bit makes the 2D engine's monochrome 8x8 pattern fill transparent (v10.80 used bit 16 from the notes, F0010A01, and the
 * engine painted the background colour anyway: VX2AUTO v1.7, 12 of 12 transparent PATCOPY hatch fills wrong, only the pattern pixels right).
 * Unattended: run it, read VX2TRN.TXT (or the one-line VX2TRN.RES). Needs VX85598.VXD from v10.74 or later (the 2D lab request).
 *
 * The desktop is put at 640x480x32 (the lab window is a 64x64 dword block of that desktop at 448,320). At 32, 16 and 8 bits per pixel a mono pattern fill of a
 * 24x16 rectangle at (3,5) is run with several command variants; every pixel of the rectangle is classified by its pattern bit (1 or 0) and by what it became:
 * the foreground register, the background register, left as it was, or something else. A variant is transparent when the 1 pixels get the foreground and the
 * 0 pixels are left alone. The variants: F0000A01 (opaque, the reference), F0010A01 (bit 16), F0000E01 (bit 10), F0010E01 (both), F0020A01 (bit 17), F0040A01 (bit 18).
 * Log: VX2TRN.TXT; result file: VX2TRN.RES (PASS: <command> when a variant is transparent at all three depths; FAIL otherwise; ABORTED). Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
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
static vx3d_request rq; static vx3d_report rp;
static DWORD recover_flag;

static void logline(const char *fmt,...) {
 va_list a;
 if(!lf) return;
 va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf);
_commit(_fileno(lf));
}


static HANDLE dev=INVALID_HANDLE_VALUE;
static int engine_call(const vx3d_request *q,vx3d_report *r) {
 DWORD got=0;
 memset(r,0,sizeof(*r));
 if(!DeviceIoControl(dev,VX_IOCTL_3D_TEST,(void*)q,sizeof(*q),r,sizeof(*r),&got,NULL)) {logline("  driver request failed: Windows error %lu\n",GetLastError()); return 0;}
 if(got!=sizeof(*r)||r->magic!=VX3D_MAGIC||r->cb!=sizeof(*r)||r->version!=VX3D_VERSION||r->build!=VX3D_BUILD||r->producer!=VX3D_PRODUCER) {logline("  the driver reply does not match this program (copy VX85598.VXD and VX2COL.EXE from the same package)\n"); return 0;}
 return 1;
}


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
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
 return !aborted&&!hung;
}

static const BYTE prow[8]={0x81,0x42,0x24,0x18,0xA5,0xF0,0x0F,0x99};
static FILE *lf2;
static int fails;
static DWORD pm2(int d) {return d==1?0xFFUL:d==2?0xFFFFUL:0xFFFFFFFFUL;}
static DWORD px2(const win_t *w,int d,int x,int y) {DWORD v=0; int k; for(k=d-1;k>=0;k--) v=(v<<8)|w->b[y][x*d+k]; return v;}
typedef struct {DWORD cmd; const char *name;} var_t;
static const var_t vars[6]={{0xF0000A01UL,"F0000A01 (opaque reference)"},{0xF0010A01UL,"F0010A01 (bit 16)"},{0xF0000E01UL,"F0000E01 (bit 10)"},{0xF0010E01UL,"F0010E01 (bits 16 and 10)"},{0xF0020A01UL,"F0020A01 (bit 17)"},{0xF0040A01UL,"F0040A01 (bit 18)"}};
static int okcount[6]; /* depths at which the variant was transparent */
static int run_variant(DWORD mode,int d,int vi) {
 op_t o; int x,y,cnt[2][4],x0=3,y0=5,w=24,h=16,k,wp=WINW/d; DWORD fg,bg,m0=0,m1=0;
 const char *cn[4]={"foreground","background","unchanged","other"};
 if(!pump()) return 0;
 fg=0x2468ACE0UL&pm2(d); bg=0x13579BDFUL&pm2(d);
 for(k=0;k<4;k++) {m0|=(DWORD)prow[k]<<(8*k); m1|=(DWORD)prow[k+4]<<(8*k);}
 memset(&o,0,sizeof(o)); o.cmd=vars[vi].cmd; o.fg=fg; o.bg=bg; o.w=(DWORD)w; o.h=(DWORD)h;
 addreg(&o,0x14,WIN_BASE); addreg(&o,0x1C,WIN_BASE); addreg(&o,0x10,((DWORD)y0<<16)|(DWORD)x0); addreg(&o,0x20,0); addreg(&o,0x24,m0); addreg(&o,0x28,m1);
 if(!run_op(mode,&o)) {logline("  %s: the operation did not run\n",vars[vi].name); return 0;}
 memset(cnt,0,sizeof(cnt));
 for(y=y0;y<y0+h;y++) for(x=x0;x<x0+w;x++) {
  int bit=(prow[(y-y0)&7]>>(7-(x&7)))&1; DWORD a=px2(&after,d,x,y),b=px2(&before,d,x,y);
  int c=(a==b)?2:(a==fg)?0:(a==bg)?1:3;
  cnt[bit][c]++;
 }
 {int spill=0; for(y=0;y<WINH;y++) for(x=0;x<wp;x++) if((x<x0||x>=x0+w||y<y0||y>=y0+h)&&px2(&after,d,x,y)!=px2(&before,d,x,y)) spill++;
  logline("  %s: pattern 1 pixels: %d foreground, %d background, %d unchanged, %d other; pattern 0 pixels: %d foreground, %d background, %d unchanged, %d other; %d changed outside\n",
   vars[vi].name,cnt[1][0],cnt[1][1],cnt[1][2],cnt[1][3],cnt[0][0],cnt[0][1],cnt[0][2],cnt[0][3],spill);
  if(cnt[1][0]&&!cnt[1][1]&&!cnt[1][3]&&cnt[0][2]&&!cnt[0][0]&&!cnt[0][1]&&!cnt[0][3]&&!spill) {logline("    -> TRANSPARENT: 1 pixels get the foreground, 0 pixels are left alone\n"); okcount[vi]++;}
  else if(cnt[1][0]&&!cnt[1][2]&&!cnt[1][3]&&cnt[0][1]&&!cnt[0][0]&&!cnt[0][3]&&!cnt[0][2]) logline("    -> opaque (1 -> foreground, 0 -> background)\n");
  else if(cnt[1][2]&&cnt[0][1]&&!cnt[1][0]&&!cnt[1][1]&&!cnt[0][0]&&!cnt[0][2]) logline("    -> inverted transparency (1 pixels left alone, 0 pixels get the background)\n");
  else if(cnt[1][2]&&cnt[0][0]&&!cnt[1][0]&&!cnt[1][1]&&!cnt[0][1]&&!cnt[0][2]) logline("    -> inverted transparency (1 pixels left alone, 0 pixels get the foreground)\n");
  else logline("    -> something else (see the counts)\n");
 }
 (void)cn;
 return 1;
}
static int run_all(char *res) {
 static const DWORD codes[3]={0x300,0x100,0x000}; static const int ds[3]={4,2,1}; int k,v,best=-1;
 for(k=0;k<3&&pump();k++) {
  logline("-- depth code %03lX (%d byte%s per pixel) --\n",codes[k],ds[k],ds[k]==1?"":"s");
  dpx=ds[k];
  for(v=0;v<6&&pump();v++) run_variant(codes[k],ds[k],v);
 }
 if(aborted) return 2;
 if(hung) return 3;
 for(v=1;v<6;v++) if(okcount[v]==3&&best<0) best=v;
 if(best>=0) {logline("RESULT: %s is transparent at 32, 16 and 8 bits per pixel\n",vars[best].name); sprintf(res,"PASS: %s transparent",vars[best].name); return 0;}
 for(v=1;v<6;v++) if(okcount[v]) logline("note: %s was transparent at %d of 3 depths\n",vars[v].name,okcount[v]);
 logline("RESULT: none of the variants gave a transparent pattern fill at all three depths\n"); sprintf(res,"FAIL: no transparent variant"); return 1;
}
static void finish(const char *res) {FILE *f=fopen("VX2TRN.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static DEVMODEA orig;
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc;
 memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc);
 return rc==DISP_CHANGE_SUCCESSFUL;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 vx_status_t s; DWORD got=0,t; int rc=1,have_orig; char res[200]="FAIL"; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2TRN.TXT","w"); (void)lf2; (void)fails;
 logline("VX2TRN v1.0 / driver v10.80\n");
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
 rc=run_all(res);
 if(rc==2) {finish("ABORTED"); logline("aborted\n");}
 else if(rc==3) {finish("FAIL: engine hung"); logline("the engine hung; restart Windows before the next test\n");}
 else finish(res);
 if(have_orig) {ChangeDisplaySettingsA(&orig,0); logline("desktop put back\n");}
 logline("Finished.\n");
 if(lf) fclose(lf);
 return rc;
}
