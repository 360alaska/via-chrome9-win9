/* VX2READ v1.0: can the CPU read the video memory faster, and is a cached mapping safe next to the 2D engine?
 * Part 1 measures CPU reads and writes of a DirectDraw video-memory surface with dword code and with 64-bit MMX moves.
 * Part 2 repeats that with the framebuffer MTRR range temporarily set to other cache types (the driver puts write-combining
 * back when VX2READ ends or its handle closes), and after each change checks whether the CPU and the 2D engine still see each
 * other's data without any cache flush: the engine fills a surface the CPU had just read, and the engine copies a surface the
 * CPU had just written.
 * While a cached type is active the desktop may show wrong pixels; they clear when write-combining is restored (move a window).
 * Writes only VX2READ.TXT. Esc aborts. */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <io.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "vx85598.h"
#include "vx855_2d.h"

static FILE *logfile;
static HANDLE dev=INVALID_HANDLE_VALUE;
static LARGE_INTEGER freq;
static int aborted;
static vx_status_t st;
static vx_cache_report cr;
static LPDIRECTDRAW dd;
static LPDIRECTDRAWSURFACE sa,sb;
static unsigned long bytes;
static unsigned char *sys1,*sys2;

void rep_movsd(void *d,const void *s,unsigned long n);
#pragma aux rep_movsd = "cld" "rep movsd" parm [edi] [esi] [ecx] modify exact [edi esi ecx];
void rep_stosd(void *d,unsigned long v,unsigned long n);
#pragma aux rep_stosd = "cld" "rep stosd" parm [edi] [eax] [ecx] modify exact [edi ecx];
/* copy ecx blocks of 32 bytes with four 64-bit MMX loads and stores per block (checked offline against an emulator) */
void mmx_copy(void *d,const void *s,unsigned long blocks);
#pragma aux mmx_copy = 0x0F 0x6F 0x06 0x0F 0x6F 0x4E 0x08 0x0F 0x6F 0x56 0x10 0x0F 0x6F 0x5E 0x18 \
 0x0F 0x7F 0x07 0x0F 0x7F 0x4F 0x08 0x0F 0x7F 0x57 0x10 0x0F 0x7F 0x5F 0x18 \
 0x83 0xC6 0x20 0x83 0xC7 0x20 0x49 0x75 0xD9 0x0F 0x77 parm [edi] [esi] [ecx] modify exact [edi esi ecx];

static void logl(const char *fmt,...) {
 va_list ap; va_start(ap,fmt); vfprintf(logfile,fmt,ap); va_end(ap); fflush(logfile); _commit(_fileno(logfile));
}
static double now(void) {
 LARGE_INTEGER t;
 if(freq.QuadPart&&QueryPerformanceCounter(&t)) return 1000.0*(double)t.QuadPart/(double)freq.QuadPart;
 return (double)GetTickCount();
}
static void pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {return DefWindowProc(w,m,a,b);}
static const char *tname(int t) {
 switch(t) {case 0:return "UC (uncached)";case 1:return "WC (write-combining)";case 4:return "WT (write-through)";case 5:return "WP";case 6:return "WB (write-back)";}
 return "unknown";
}
static int stats(vx2d_stats *s) {
 DWORD got=0; return DeviceIoControl(dev,VX_IOCTL_2D_STATS,NULL,0,s,sizeof(*s),&got,NULL)&&got==sizeof(*s)&&s->abi==VX2D_ABI;
}
/* MTRR type that covers the video memory right now (-1 unknown) */
static int fb_type(void) {
 DWORD got; unsigned i; int t=-1,found=0; unsigned long long addr;
 memset(&cr,0,sizeof(cr));
 if(!DeviceIoControl(dev,VX_IOCTL_CACHE,NULL,0,&cr,sizeof(cr),&got,NULL)||got!=sizeof(cr)||!cr.msr_ok) return -1;
 addr=cr.fb_phys;
 for(i=0;i<cr.var_count;i++) {
  unsigned long long b=((unsigned long long)cr.var[i*4+1]<<32)|cr.var[i*4], m=((unsigned long long)cr.var[i*4+3]<<32)|cr.var[i*4+2];
  int bt;
  if(!((m>>11)&1)) continue;
  if((addr&m&~0xFFFULL)!=(b&m&~0xFFFULL)) continue;
  bt=(int)(b&255);
  if(!found) {t=bt;found=1;} else if(bt==0||t==0) t=0; else if((bt==4&&t==6)||(bt==6&&t==4)) t=4; else if(bt!=t) t=-2;
 }
 if(!found) t=(int)(cr.deftype[0]&255);
 return t;
}
static DWORD set_type(DWORD type) {
 DWORD got=0,out=0xFFFFFFFFUL;
 if(!DeviceIoControl(dev,VX_IOCTL_CACHE_SET,&type,4,&out,4,&got,NULL)||got!=4) return 0xFFFFFFFEUL;
 return out;
}

typedef void (*kernel)(unsigned char *vram,unsigned char *sys,unsigned long bytes);
static void k_wfill(unsigned char *v,unsigned char *s,unsigned long n) {(void)s; rep_stosd(v,0x5A5A5A5AUL,n>>2);}
static void k_wmovsd(unsigned char *v,unsigned char *s,unsigned long n) {rep_movsd(v,s,n>>2);}
static void k_wmmx(unsigned char *v,unsigned char *s,unsigned long n) {mmx_copy(v,s,n>>5);}
static void k_rmovsd(unsigned char *v,unsigned char *s,unsigned long n) {rep_movsd(s,v,n>>2);}
static void k_rdword(unsigned char *v,unsigned char *s,unsigned long n) {
 unsigned long *d=(unsigned long*)v,i,a=0; unsigned long *o=(unsigned long*)s;
 for(i=0;i+4<=(n>>2);i+=4) a+=d[i]+d[i+1]+d[i+2]+d[i+3];
 o[0]=a;
}
static void k_rmmx(unsigned char *v,unsigned char *s,unsigned long n) {mmx_copy(s,v,n>>5);}
static double run(const char *name,kernel f) {
 int rep; double sum=0,lo=1e30,hi=0; unsigned char *vram;
 DDSURFACEDESC d; HRESULT hr;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 hr=IDirectDrawSurface_Lock(sa,NULL,&d,DDLOCK_WAIT,NULL);
 if(FAILED(hr)||!d.lpSurface) {logl("  Lock failed %08lX\n",(DWORD)hr); return 0;}
 vram=(unsigned char*)d.lpSurface;
 for(rep=0;rep<2&&!aborted;rep++) {
  unsigned long n=0; double t0,t1,r; pump();
  f(vram,sys2,bytes);
  t0=now(); do {f(vram,sys2,bytes); n++; t1=now();} while(t1-t0<300.0&&n<100000UL);
  r=(double)bytes*n/1048576.0/((t1-t0)/1000.0); sum+=r; if(r<lo) lo=r; if(r>hi) hi=r;
 }
 IDirectDrawSurface_Unlock(sa,NULL);
 logl("  %-46s %8.1f MB/s  (runs %.1f to %.1f)\n",name,sum/2.0,lo,hi);
 return sum/2.0;
}

static DWORD fillcolor(unsigned long v) {return st.bpp==16?(v&0xFFFFUL):v;}
static int engine_ran(vx2d_stats *a,vx2d_stats *b) {return b->ddraw!=a->ddraw||b->ops!=a->ops;}
static unsigned long count_dwords(LPDIRECTDRAWSURFACE s,unsigned long want,unsigned long *total) {
 DDSURFACEDESC d; unsigned long *p,i,n=0; unsigned long dw=bytes>>2;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 if(FAILED(IDirectDrawSurface_Lock(s,NULL,&d,DDLOCK_WAIT,NULL))||!d.lpSurface) {*total=0; return 0;}
 p=(unsigned long*)d.lpSurface; for(i=0;i<dw;i++) if(p[i]==want) n++;
 IDirectDrawSurface_Unlock(s,NULL); *total=dw; return n;
}
static void fill_cpu(LPDIRECTDRAWSURFACE s,unsigned long v) {
 DDSURFACEDESC d; memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 if(FAILED(IDirectDrawSurface_Lock(s,NULL,&d,DDLOCK_WAIT,NULL))||!d.lpSurface) return;
 rep_stosd(d.lpSurface,v,bytes>>2); IDirectDrawSurface_Unlock(s,NULL);
}
/* engine fill/copy coherency; returns via log */
static void coherency(void) {
 vx2d_stats a,b; DDBLTFX fx; HRESULT hr; unsigned long fresh,stale,total,n;
 /* 1: the CPU reads the surface (so its lines are cached if the type allows it), the engine overwrites it, the CPU reads again */
 fill_cpu(sa,0x11111111UL);
 n=count_dwords(sa,0x11111111UL,&total); (void)n;
 n=count_dwords(sa,0x11111111UL,&total);
 stats(&a); memset(&fx,0,sizeof(fx)); fx.dwSize=sizeof(fx); fx.dwFillColor=fillcolor(0x22222222UL);
 hr=IDirectDrawSurface_Blt(sa,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx); stats(&b);
 if(FAILED(hr)||!engine_ran(&a,&b)) {logl("  engine fill: NOT RUN ON THE ENGINE (hr %08lX, ops %lu -> %lu): test invalid\n",(DWORD)hr,a.ops,b.ops);}
 else {
  fresh=count_dwords(sa,st.bpp==16?0x22222222UL:0x22222222UL,&total); stale=count_dwords(sa,0x11111111UL,&total);
  logl("  engine fills what the CPU had just read: CPU then sees the new data in %lu of %lu dwords, the old data in %lu  => %s\n",fresh,total,stale,
   fresh==total?"COHERENT":(stale?"STALE (CPU cache not updated by the engine)":"MIXED"));
 }
 /* 2: the CPU writes a surface, the engine copies it, the CPU reads the copy */
 fill_cpu(sb,0x44444444UL); fill_cpu(sa,0x33333333UL);
 stats(&a); hr=IDirectDrawSurface_Blt(sb,NULL,sa,NULL,DDBLT_WAIT,NULL); stats(&b);
 if(FAILED(hr)||!engine_ran(&a,&b)) {logl("  engine copy: NOT RUN ON THE ENGINE (hr %08lX, ops %lu -> %lu): test invalid\n",(DWORD)hr,a.ops,b.ops);}
 else {
  fresh=count_dwords(sb,0x33333333UL,&total); stale=count_dwords(sb,0x44444444UL,&total);
  logl("  engine copies what the CPU had just written: copy holds the CPU data in %lu of %lu dwords, the older data in %lu  => %s\n",fresh,total,stale,
   fresh==total?"COHERENT":(stale?"STALE (the engine read memory the CPU cache had not written back)":"MIXED"));
 }
}

static void suite(const char *title,double *rr) {
 double w1,w2,w3,r1,r2,r3;
 logl("\n%s\n",title);
 w1=run("write, rep stosd (fill)",k_wfill); w2=run("write, rep movsd (copy from system)",k_wmovsd); w3=run("write, MMX 64-bit moves (copy from system)",k_wmmx);
 r1=run("read,  rep movsd (copy to system)",k_rmovsd); r2=run("read,  dword loads",k_rdword); r3=run("read,  MMX 64-bit moves (copy to system)",k_rmmx);
 (void)w1;(void)w2;(void)w3; rr[0]=r1; rr[1]=r2; rr[2]=r3;
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show) {
 WNDCLASS wc; HWND w; DWORD got; DDSURFACEDESC d; HRESULT hr; unsigned long rows; int base,i;
 static const struct {DWORD type; const char *name;} modes[]={{1,"cache type 1 (cached, write-back)"},{3,"cache type 3 (hardware coherent cached)"}};
 double rr[3],base_best=0,best;
 (void)prev;(void)cmd;(void)show;
 logfile=fopen("VX2READ.TXT","w"); if(!logfile) return 1;
 QueryPerformanceFrequency(&freq);
 logl("VX2READ v1.0 (driver v10.69 or later)\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&st,sizeof(st),&got,NULL)||got!=sizeof(st)||st.magic!=VX_MAGIC||(st.bpp!=16&&st.bpp!=32)) {
  logl("The driver does not answer or is not at 16/32 bpp.\n"); fclose(logfile); return 1;
 }
 logl("Desktop %lux%lux%lu\n",st.width,st.height,st.bpp);
 base=fb_type();
 if(base!=1&&base!=-1) logl("Warning: the video memory's MTRR type is %s, not write-combining; the first block is not the driver's normal setting.\n",tname(base));
 logl("Video memory MTRR type at start: %s\n",tname(base));
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2READ"; RegisterClass(&wc);
 w=CreateWindowEx(0,wc.lpszClassName,"VX2READ",WS_POPUP,0,0,1,1,NULL,NULL,hi,NULL);
 {typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*); HMODULE mod=LoadLibraryA("DDRAW.DLL"); CREATE create=mod?(CREATE)GetProcAddress(mod,"DirectDrawCreate"):NULL;
  if(!create) {logl("DDRAW.DLL not found\n"); goto done;}
  hr=create(NULL,&dd,NULL);}
 if(FAILED(hr)||FAILED(IDirectDraw_SetCooperativeLevel(dd,w,DDSCL_NORMAL))) {logl("DirectDraw start failed %08lX\n",(DWORD)hr); goto done;}
 rows=128;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; d.dwWidth=st.width; d.dwHeight=rows; d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 if(FAILED(hr=IDirectDraw_CreateSurface(dd,&d,&sa,NULL))||FAILED(hr=IDirectDraw_CreateSurface(dd,&d,&sb,NULL))) {logl("Could not create two video-memory surfaces: %08lX\n",(DWORD)hr); goto done;}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 if(FAILED(IDirectDrawSurface_Lock(sa,NULL,&d,DDLOCK_WAIT,NULL))||!d.lpSurface) {logl("Lock failed\n"); goto done;}
 bytes=((unsigned long)d.lPitch*rows)&~31UL; IDirectDrawSurface_Unlock(sa,NULL);
 logl("Test surfaces: two video-memory surfaces of %lu KB, address %08lX\n",bytes>>10,(unsigned long)d.lpSurface);
 sys1=(unsigned char*)VirtualAlloc(NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); sys2=(unsigned char*)VirtualAlloc(NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!sys1||!sys2) {logl("allocation failed\n"); goto done;}
 memset(sys1,0xA5,bytes); memset(sys2,0,bytes);
 {double r[3]; suite("== Block 1: driver setting (write-combining) ==",r); base_best=r[0]>r[1]?(r[0]>r[2]?r[0]:r[2]):(r[1]>r[2]?r[1]:r[2]); rr[0]=r[0];rr[1]=r[1];rr[2]=r[2];}
 logl("\nCoherency with the 2D engine at the driver setting:\n"); coherency();
 for(i=0;i<2&&!aborted;i++) {
  DWORD sr=set_type(modes[i].type); int t;
  logl("\n== Block %d: %s ==\n",i+2,modes[i].name);
  logl("  MTRR service status %08lX\n",sr);
  Sleep(200); t=fb_type(); logl("  video memory MTRR type now: %s\n",tname(t));
  if(sr!=0||t==1||t==-1) {logl("  the type did not change (or could not be read): block skipped\n"); set_type(2); continue;}
  { double r[3]; suite("  speed:",r); best=r[0]>r[1]?(r[0]>r[2]?r[0]:r[2]):(r[1]>r[2]?r[1]:r[2]); logl("  best read %.1f MB/s = %.1f times the write-combining best (%.1f)\n",best,base_best>0?best/base_best:0.0,base_best); }
  logl("\n  coherency with the 2D engine:\n"); coherency();
  sr=set_type(2); Sleep(200); logl("\n  write-combining restored: status %08lX, MTRR type now %s\n",sr,tname(fb_type()));
 }
 (void)rr;
done:
 set_type(2);
 logl("\nFinal video memory MTRR type: %s\n",tname(fb_type()));
 if(sa) IDirectDrawSurface_Release(sa); if(sb) IDirectDrawSurface_Release(sb); if(dd) IDirectDraw_Release(dd);
 logl("\nFinished.\n"); fclose(logfile);
 MessageBox(NULL,"VX2READ finished. Send me VX2READ.TXT. If the desktop looks odd, drag a window across it.","VX2READ",MB_OK);
 return 0;
}
