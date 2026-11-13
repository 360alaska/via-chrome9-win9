/* VX2MEM v1.0: why are CPU writes to the video memory slow, and is write-combining really on?
 * Part 1 asks the driver for the cache report (CPU, MTRR and PAT registers, page-table bits of the framebuffer mapping, the
 * answer of the Windows MTRR service) and works out which memory type the CPU should be using for the video memory.
 * Part 2 measures CPU writes and reads on a DirectDraw video-memory surface (the path a game uses with Lock) and on system memory.
 * Changes no display mode, draws nothing visible, writes only VX2MEM.TXT. Esc aborts. */
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

static FILE *logfile;
static HANDLE dev=INVALID_HANDLE_VALUE;
static LARGE_INTEGER freq;
static int aborted;

void rep_movsd(void *d,const void *s,unsigned long n);
#pragma aux rep_movsd = "cld" "rep movsd" parm [edi] [esi] [ecx] modify exact [edi esi ecx];
void rep_stosd(void *d,unsigned long v,unsigned long n);
#pragma aux rep_stosd = "cld" "rep stosd" parm [edi] [eax] [ecx] modify exact [edi ecx];

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

static const char *tname(unsigned t) {
 switch(t) {case 0:return "UC (uncached)";case 1:return "WC (write-combining)";case 4:return "WT (write-through)";case 5:return "WP (write-protect)";case 6:return "WB (write-back)";case 7:return "UC- (uncached, WC allowed by PAT)";}
 return "reserved";
}

/* ---- part 1 ---- */
static unsigned long long mask36(const vx_cache_report *r,int i) {
 return ((unsigned long long)r->var[i*4+3]<<32)|r->var[i*4+2];
}
static unsigned long long base36(const vx_cache_report *r,int i) {
 return ((unsigned long long)r->var[i*4+1]<<32)|r->var[i*4+0];
}
static void report_cache(vx_cache_report *r,int *mtrr_type,int *pte_type) {
 char vend[13]; int i,def_enabled; unsigned long long addr; int found=0,t=-1;
 unsigned fam,mod,step;
 *mtrr_type=-1; *pte_type=-1;
 memcpy(vend,&r->vendor[0],4); memcpy(vend+4,&r->vendor[1],4); memcpy(vend+8,&r->vendor[2],4); vend[12]=0;
 fam=(r->sig>>8)&15; mod=(r->sig>>4)&15; step=r->sig&15;
 if(fam==15) fam+=(r->sig>>20)&255;
 if(fam==6||fam==15) mod|=((r->sig>>16)&15)<<4;
 logl("CPU: %s, CPUID %s, family %u model %u stepping %u (signature %08lX)\n",r->has_cpuid?vend:"(no CPUID)",r->has_cpuid?"yes":"no",fam,mod,step,r->sig);
 logl("  features EDX %08lX ECX %08lX: MSR %s, MTRR %s, PAT %s, MMX %s, SSE %s, SSE2 %s\n",r->feat_edx,r->feat_ecx,
  (r->feat_edx>>5)&1?"yes":"no",(r->feat_edx>>12)&1?"yes":"no",(r->feat_edx>>16)&1?"yes":"no",(r->feat_edx>>23)&1?"yes":"no",(r->feat_edx>>25)&1?"yes":"no",(r->feat_edx>>26)&1?"yes":"no");
 logl("\nWindows MTRR service (what the driver asked for the video memory):\n");
 logl("  driver option VRAM_WC (write-combining) = %lu\n",r->mtrr_conf);
 if(!r->mtrr_called) {
  logl("  the driver never called SetPhysicalCacheTypeRange. MTRR service version answered: %lu (0 = service not present). ",r->mtrr_ver);
  logl("%s\n",r->mtrr_ver?"The call was skipped for another reason (video memory below 1 MB, or already set).":"So no cache type was ever requested for the video memory.");
 } else {
  logl("  MTRR service version %lu; requested type 2 (framebuffer cached, i.e. write-combining) for %08lX, %lu bytes (%lu MB); service returned %08lX (%s)\n",
   r->mtrr_ver,r->mtrr_base,r->mtrr_size,r->mtrr_size>>20,r->mtrr_status,r->mtrr_status==0?"success":"FAILED");
  if(r->mtrr_size&(r->mtrr_size-1)) logl("  note: the size is not a power of two, which an MTRR range needs\n");
  if(r->mtrr_base&(r->mtrr_size?r->mtrr_size-1:0)) logl("  note: the base is not aligned to the size, which an MTRR range needs\n");
 }
 logl("\nCPU cache registers:\n");
 if(!r->msr_ok) {logl("  not read (no CPUID, or the CPU has no MSR/MTRR)\n");}
 else {
  logl("  MTRRCAP %08lX%08lX: %lu variable ranges, fixed ranges %s, WC type %s\n",r->mtrrcap[1],r->mtrrcap[0],r->mtrrcap[0]&0xFF,(r->mtrrcap[0]>>8)&1?"yes":"no",(r->mtrrcap[0]>>10)&1?"supported":"NOT supported");
  def_enabled=(r->deftype[0]>>11)&1;
  logl("  MTRR default type register %08lX: MTRRs %s, fixed %s, default type %s\n",r->deftype[0],def_enabled?"enabled":"DISABLED",(r->deftype[0]>>10)&1?"on":"off",tname(r->deftype[0]&255));
  for(i=0;i<(int)r->var_count;i++) {
   unsigned long long m=mask36(r,i),b=base36(r,i); int valid=(int)((m>>11)&1);
   unsigned long long size=0;
   if(valid) {unsigned long long mm=m&0xFFFFFFFFFULL&~0xFFFULL; size=(~mm+1)&0xFFFFFFFFFULL; if(!size) size=0x1000000000ULL;}
   logl("  range %d: base %09llX mask %09llX %s",i,b&~0xFFFULL,m&~0xFFFULL,valid?"valid":"unused");
   if(valid) logl(", %llu MB, type %s",size>>20,tname((unsigned)(b&255)));
   logl("\n");
  }
  if(r->feat_edx&(1UL<<16)) {
   logl("  PAT register %08lX%08lX: ",r->pat[1],r->pat[0]);
   for(i=0;i<8;i++) {unsigned t2=(unsigned)(((i<4?r->pat[0]:r->pat[1])>>((i&3)*8))&255); logl("%d=%s%s",i,tname(t2),i<7?"; ":"\n");}
  }
  addr=r->fb_phys;
  if(def_enabled) {
   for(i=0;i<(int)r->var_count;i++) {
    unsigned long long m=mask36(r,i),b=base36(r,i);
    if(!((m>>11)&1)) continue;
    if(((addr&m&~0xFFFULL))==(b&m&~0xFFFULL)) {
     int bt=(int)(b&255);
     if(!found) {t=bt;found=1;}
     else if(bt==0||t==0) t=0;
     else if((bt==4&&t==6)||(bt==6&&t==4)) t=4;
     else if(bt!=t) t=-2;
    }
   }
   if(!found) t=(int)(r->deftype[0]&255);
   *mtrr_type=t;
  }
  logl("  video memory at %08lX: ",r->fb_phys);
  if(!def_enabled) logl("MTRRs disabled, so the CPU treats it as uncached\n");
  else if(t==-2) logl("overlapping ranges with conflicting types\n");
  else logl("MTRR type %s%s\n",tname((unsigned)t),found?"":" (default, no range covers it)");
 }
 logl("\nFramebuffer mapping in the driver: physical %08lX, linear %08lX, %lu MB mapped\n",r->fb_phys,r->fb_linear,r->fb_map_bytes>>20);
 for(i=0;i<(int)r->pte_count;i++) {
  unsigned long e=r->pte[i]; unsigned idx=(unsigned)(((e>>7)&1)*4+((e>>4)&1)*2+((e>>3)&1)); unsigned pt;
  logl("  page %lu (offset %lu KB): entry %08lX: %s, PWT %lu PCD %lu PAT %lu",r->pte_page[i],r->pte_page[i]*4,e,(e&1)?"present":"NOT PRESENT",(e>>3)&1,(e>>4)&1,(e>>7)&1);
  if(e&1) {
   if(r->msr_ok&&(r->feat_edx&(1UL<<16))) {pt=(unsigned)(((idx<4?r->pat[0]:r->pat[1])>>((idx&3)*8))&255); logl(" -> page type %s",tname(pt)); if(i==0) *pte_type=(int)pt;}
   else {pt=(e>>4)&1?0:((e>>3)&1?4:6); logl(" -> page type %s (no PAT: PCD/PWT only)",tname(pt)); if(i==0) *pte_type=(int)pt;}
  }
  logl("\n");
 }
 if(*pte_type>=0) {
  int m=*mtrr_type,p=*pte_type,eff=-1;
  if(m==-2) eff=-1;
  else if(m==-1&&!r->msr_ok) eff=p; /* MTRRs not readable: page type only */
  else if(m==-1) eff=0; /* MTRRs disabled: uncached */
  else if(p==7) eff=(m==1)?1:0; /* UC- */
  else if(p==0||m==0) eff=0; else if(p==1||m==1) eff=1; else if(p==4||m==4) eff=4; else eff=6;
  if(eff<0) logl("\nExpected memory type: cannot tell (conflicting MTRR ranges)\n"); else logl("\nExpected memory type for CPU access to the video memory (page type combined with MTRR type; approximate): %s\n",tname((unsigned)eff));
 }
}

/* ---- part 2 ---- */
static LPDIRECTDRAW dd;
static LPDIRECTDRAWSURFACE vs;
static double mbs(unsigned long bytes,double ms) {return ms>0.0?(double)bytes/1048576.0/(ms/1000.0):0.0;}
typedef void (*kernel)(unsigned char *vram,unsigned char *sys,unsigned long dwords,volatile unsigned long *sink);
static void k_wfill(unsigned char *v,unsigned char *s,unsigned long n,volatile unsigned long *k) {(void)s;(void)k; rep_stosd(v,0x5A5A5A5AUL,n);}
static void k_wcopy(unsigned char *v,unsigned char *s,unsigned long n,volatile unsigned long *k) {(void)k; rep_movsd(v,s,n);}
static void k_wloop(unsigned char *v,unsigned char *s,unsigned long n,volatile unsigned long *k) {
 unsigned long *d=(unsigned long*)v,*q=(unsigned long*)s,i; (void)k;
 for(i=0;i+8<=n;i+=8) {d[i]=q[i];d[i+1]=q[i+1];d[i+2]=q[i+2];d[i+3]=q[i+3];d[i+4]=q[i+4];d[i+5]=q[i+5];d[i+6]=q[i+6];d[i+7]=q[i+7];}
}
static void k_rcopy(unsigned char *v,unsigned char *s,unsigned long n,volatile unsigned long *k) {(void)k; rep_movsd(s,v,n);}
static void k_rloop(unsigned char *v,unsigned char *s,unsigned long n,volatile unsigned long *k) {
 unsigned long *d=(unsigned long*)v,i,a=0; (void)s;
 for(i=0;i+4<=n;i+=4) a+=d[i]+d[i+1]+d[i+2]+d[i+3];
 *k=a;
}
static void k_sfill(unsigned char *v,unsigned char *s,unsigned long n,volatile unsigned long *k) {(void)v;(void)k; rep_stosd(s,0x5A5A5A5AUL,n);}
static void k_scopy(unsigned char *v,unsigned char *s,unsigned long n,volatile unsigned long *k) {(void)k; rep_movsd(v,s,n);} /* v and s both system memory here */

static double run(const char *name,kernel f,unsigned char *vram,unsigned char *sys,unsigned long bytes,double *worst,double *best) {
 int rep; double sum=0; volatile unsigned long sink=0; unsigned long dw=bytes>>2;
 *worst=1e30; *best=0;
 for(rep=0;rep<3&&!aborted;rep++) {
  unsigned long n=0; double t0,t1,ms;
  pump();
  f(vram,sys,dw,&sink); /* warm-up */
  t0=now(); do {f(vram,sys,dw,&sink); n++; t1=now();} while(t1-t0<400.0&&n<100000UL);
  ms=t1-t0; {double r=mbs(bytes*n,ms); sum+=r; if(r<*worst) *worst=r; if(r>*best) *best=r;}
 }
 logl("  %-52s %8.1f MB/s  (runs %.1f to %.1f)\n",name,sum/3.0,*worst,*best);
 return sum/3.0;
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show) {
 WNDCLASS wc; HWND w; vx_status_t s; vx_cache_report cr; DWORD got; int mt,pt;
 DDSURFACEDESC d; HRESULT hr; unsigned long bytes,rows; unsigned char *sys1,*sys2; double need;
 (void)prev;(void)cmd;(void)show;
 logfile=fopen("VX2MEM.TXT","w"); if(!logfile) return 1;
 QueryPerformanceFrequency(&freq);
 logl("VX2MEM v1.0 (driver v10.68 or later)\n\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC) {
  logl("The driver does not answer (VX85598.VXD not loaded).\n"); MessageBox(NULL,"VX2MEM: the driver does not answer.","VX2MEM",MB_OK); fclose(logfile); return 1;
 }
 logl("Desktop %lux%lux%lu pitch %lu\n\n",s.width,s.height,s.bpp,s.video_pitch);
 memset(&cr,0,sizeof(cr));
 if(!DeviceIoControl(dev,VX_IOCTL_CACHE,NULL,0,&cr,sizeof(cr),&got,NULL)||got!=sizeof(cr)||cr.magic!=VX_MAGIC||cr.abi!=VX_CACHE_ABI) {
  logl("The driver has no cache report (needs VX85598.VXD v10.68 or later; Windows error %lu)\n",GetLastError());
  memset(&cr,0,sizeof(cr)); mt=pt=-1;
 } else report_cache(&cr,&mt,&pt);
 logl("\nCPU access speed:\n");
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2MEM"; RegisterClass(&wc);
 w=CreateWindowEx(0,wc.lpszClassName,"VX2MEM",WS_POPUP,0,0,1,1,NULL,NULL,hi,NULL);
 {typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*); HMODULE mod=LoadLibraryA("DDRAW.DLL"); CREATE create=mod?(CREATE)GetProcAddress(mod,"DirectDrawCreate"):NULL;
  if(!create) {logl("DDRAW.DLL not found\n"); goto done;}
  hr=create(NULL,&dd,NULL);}
 if(FAILED(hr)) {logl("DirectDrawCreate failed: %08lX\n",(DWORD)hr); goto done;}
 hr=IDirectDraw_SetCooperativeLevel(dd,w,DDSCL_NORMAL);
 if(FAILED(hr)) {logl("SetCooperativeLevel failed: %08lX\n",(DWORD)hr); goto done;}
 rows=(1024UL*1024UL)/s.video_pitch; if(rows<64) rows=64; if(rows>s.height) rows=s.height;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; d.dwWidth=s.width; d.dwHeight=rows;
 d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 hr=IDirectDraw_CreateSurface(dd,&d,&vs,NULL);
 if(FAILED(hr)) {logl("Could not create a video-memory surface: %08lX\n",(DWORD)hr); goto done;}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 hr=IDirectDrawSurface_Lock(vs,NULL,&d,DDLOCK_WAIT,NULL);
 if(FAILED(hr)||!d.lpSurface) {logl("Lock failed: %08lX\n",(DWORD)hr); goto done;}
 bytes=(unsigned long)d.lPitch*rows; bytes&=~31UL;
 logl("Test surface: video memory, %lux%lu, pitch %ld, %lu KB, address %08lX",s.width,rows,(long)d.lPitch,bytes>>10,(unsigned long)d.lpSurface);
 if(cr.fb_linear) {
  if((unsigned long)d.lpSurface>=cr.fb_linear&&(unsigned long)d.lpSurface<cr.fb_linear+cr.fb_map_bytes) logl(" (inside the driver's framebuffer mapping, so the report above applies)\n");
  else logl(" (OUTSIDE the driver's framebuffer mapping at %08lX: the cache report above may not describe it)\n",cr.fb_linear);
 } else logl("\n");
 sys1=(unsigned char*)VirtualAlloc(NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE); sys2=(unsigned char*)VirtualAlloc(NULL,bytes,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
 if(!sys1||!sys2) {logl("could not allocate system memory\n"); goto done;}
 memset(sys1,0xA5,bytes); memset(sys2,0x00,bytes);
 {
  double wf,wc2,wl,rc,rl,sf,sc,b1,b2;
  wf =run("write, video memory, rep stosd (fill)",k_wfill,(unsigned char*)d.lpSurface,sys1,bytes,&b1,&b2);
  wc2=run("write, video memory, rep movsd (copy from system)",k_wcopy,(unsigned char*)d.lpSurface,sys1,bytes,&b1,&b2);
  wl =run("write, video memory, 8 dword stores per loop",k_wloop,(unsigned char*)d.lpSurface,sys1,bytes,&b1,&b2);
  rc =run("read,  video memory, rep movsd (copy to system)",k_rcopy,(unsigned char*)d.lpSurface,sys2,bytes,&b1,&b2);
  rl =run("read,  video memory, dword loads",k_rloop,(unsigned char*)d.lpSurface,sys2,bytes,&b1,&b2);
  sf =run("system memory, rep stosd (fill)",k_sfill,sys2,sys2,bytes,&b1,&b2);
  sc =run("system memory, rep movsd (copy)",k_scopy,sys1,sys2,bytes,&b1,&b2);
  need=(double)s.width*(double)s.height*(double)(s.bpp/8)*60.0/1048576.0;
  logl("\nA full-screen redraw at 60 frames per second in this mode needs %.0f MB/s of writes to the video memory.\n",need);
  logl("Best video-memory write measured: %.1f MB/s (%s).\n",wf>wc2?(wf>wl?wf:wl):(wc2>wl?wc2:wl),
   (wf>wc2?(wf>wl?wf:wl):(wc2>wl?wc2:wl))>=need*1.5?"enough for 60 fps":"NOT enough for 60 fps");
  logl("Video-memory write speed is %.1f%% of system-memory copy speed; video-memory read speed is %.1f%% of it.\n",100.0*(wc2>wf?wc2:wf)/(sc>0?sc:1),100.0*(rc>rl?rc:rl)/(sc>0?sc:1));
  (void)sf;
 }
 IDirectDrawSurface_Unlock(vs,NULL);
done:
 if(vs) IDirectDrawSurface_Release(vs);
 if(dd) IDirectDraw_Release(dd);
 logl("\nFinished.\n"); fclose(logfile);
 MessageBox(NULL,"VX2MEM finished. Send me VX2MEM.TXT.","VX2MEM",MB_OK);
 return 0;
}
