/* VXGL.DLL: the ICD entry points (Drv*), pixel formats, the VxD and DirectDraw objects, the window buffers and the swap (v10.239).
 * Windows 98's opengl32.dll asks the display driver for the ICD's name (escape OPENGL_GETINFO: "VX855GL"), finds the DLL under
 * HKLM\Software\Microsoft\Windows\CurrentVersion\OpenGLdrivers, value VX855GL = VXGL.DLL, and calls the Drv* functions here.
 * .530: HKLM\Software\VX85598 holds only VXGL_LOG, VXGL_FRAMELOG, VXGL_SAMPLE and ACCEL_OPENGL_TSCPROF for this DLL (ACCEL_OPENGL and
 * ACCEL_OPENGL_HW were removed: the ICD always loads and always draws with the hardware). */
#include "vxgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include "glapi.h"
#include "context.h"

static const GUID iid_dd7={0x15e65ec0,0x3b9c,0x11d2,{0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b}}; /* IID_IDirectDraw7 (not in Watcom's dxguid.lib) */
HANDLE vxgl_vxd=INVALID_HANDLE_VALUE;
LPDIRECTDRAW7 vxgl_dd;
static LPDIRECTDRAWSURFACE7 primary;
static LPDIRECTDRAWCLIPPER clipper;
DWORD vxgl_vram_linear,vxgl_vram_size;
static DWORD (WINAPI *dd_surface)(void*,DWORD*,DWORD*); /* VXDDRAW.DLL VX_DD_Surface (v10.241) */
static DWORD (WINAPI *dd_splane)(void*,DWORD*,DWORD*);  /* VX_DD_StencilPlane (v10.245) */
static DWORD (WINAPI *dd_present)(DWORD); /* VX_DD_Present (v10.424): the swap's blits to the screen run asynchronously; 0 with an older VXDDRAW */
int vxgl_hw_enabled=1;
int vxgl_sse_ok=0;unsigned long vxgl_sse_cr4,vxgl_sse_edx; /* v10.359 */
static int icd_enabled=1,log_on;
static CRITICAL_SECTION lock,engine;
static vxgl_fb *fbs;
static int desktop_bpp; /* read again at each DescribePixelFormat/SetPixelFormat: a game may change the mode after loading the ICD */
void vxgl_screen_bpp(void);

/* ---------------------------------------------------------------- log (HKLM\Software\VX85598 VXGL_LOG = 1: C:\VXGL.LOG) */
/* v10.330 (review #42): a line's end-of-file seek and write as one: a critical section between this process's threads (logcs_ok once
 * DLL attach made it) and a lock of one byte far past the end between processes (a screen saver's preview and settings), a few tries,
 * the line written anyway after them */
static CRITICAL_SECTION logcs;static int logcs_ok;
extern int vxgl_ftrace; /* .591 (vxgl_mesa.c) */
void vxgl_log(const char *fmt,...) {
 static HANDLE f=INVALID_HANDLE_VALUE;char b[512];va_list a;DWORD n;int k,locked=0;
 if(!log_on)return;
 if(logcs_ok)EnterCriticalSection(&logcs);
 if(f==INVALID_HANDLE_VALUE){f=CreateFileA("C:\\VXGL.LOG",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(f==INVALID_HANDLE_VALUE){log_on=0;goto out;}}
 /* v10.259: every line with its process and thread, appended at the end each time (a screen saver's preview and its settings are two processes sharing the file) */
 k=_snprintf(b,40,"[%lX:%lX] ",(unsigned long)GetCurrentProcessId(),(unsigned long)GetCurrentThreadId());
 va_start(a,fmt);n=(DWORD)_vsnprintf(b+k,sizeof(b)-2-k,fmt,a);va_end(a);if(n>sizeof(b)-2-k)n=sizeof(b)-2-k;n+=k;
 for(k=0;k<20&&!(locked=LockFile(f,0x7FFFFFF0UL,0,1,0)!=0);k++)Sleep(k<4?0:1);
 /* v10.271: at most 4 MB (VXGL_LOG was left on through a session: 80 MB, the engine/software switches of every draw) */
 {DWORD at=SetFilePointer(f,0,0,FILE_END);if(at!=0xFFFFFFFFUL&&at>=(4UL<<20)){static int said;if(!said++){static const char full[]="(VXGL.LOG at 4 MB: no more lines this time)\r\n";WriteFile(f,full,sizeof(full)-1,&n,0);}}
  else WriteFile(f,b,n,&n,0);}
 if(locked)UnlockFile(f,0x7FFFFFF0UL,0,1,0);
out:
 if(logcs_ok)LeaveCriticalSection(&logcs);
}
static DWORD reg_dword(const char *name,DWORD def) {
 HKEY k;DWORD v=def,t,n=4;char e[16];
 /* v10.358: an environment variable of the same name (decimal, or hexadecimal after 0x) wins over the registry, for this process only:
  * test programs (VXGAME) compare settings without changing the machine's (as VXGL_HW, v10.310) */
 if(GetEnvironmentVariableA(name,e,sizeof(e))&&e[0]){char *end;unsigned long x=strtoul(e,&end,0);if(end!=e)return (DWORD)x;}
 if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Software\\VX85598",0,KEY_READ,&k)==ERROR_SUCCESS){if(RegQueryValueExA(k,name,0,&t,(BYTE*)&v,&n)!=ERROR_SUCCESS||t!=REG_DWORD)v=def;RegCloseKey(k);}
 return v;
}

/* ---------------------------------------------------------------- the VxD */
static int vxd_open(void) {
 if(vxgl_vxd==INVALID_HANDLE_VALUE)vxgl_vxd=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 return vxgl_vxd!=INVALID_HANDLE_VALUE;
}
#pragma aux tsc_low = 0x0F 0x31 value [eax] modify [edx];
unsigned long tsc_low(void);
#pragma aux tsc64 = 0x0F 0x31 value [edx eax] modify [];
unsigned __int64 tsc64(void);
/* v10.497 (board, VXFLAT v2.4: one 30 ms frame at the same frame every run): each VxD call timed (TSC, always), the longest kept with its code,
 * the busy retries and the waits of a millisecond counted (VXGL_StallStats; VXFLAT reads it after every frame) */
static unsigned long st_max,st_code,st_calls,st_busy,st_sleep;static double st_total;
static int vxd_call(DWORD code,const void *in,DWORD n,void *out,DWORD z) {
 DWORD got=0;unsigned long t0,d;BOOL r;
 if(!vxd_open())return 0;
 t0=tsc_low();r=DeviceIoControl(vxgl_vxd,code,(void*)in,n,out,z,&got,0);d=tsc_low()-t0;st_calls++;st_total+=(double)d;if(d>st_max){st_max=d;st_code=code;}
 if(r)return 1;
 CloseHandle(vxgl_vxd);vxgl_vxd=INVALID_HANDLE_VALUE; /* opened again on the next call */
 return 0;
}
int vxgl_vxd_info(vx3dn_info *i) {memset(i,0,sizeof(*i));return vxd_call(VX_IOCTL_3DN_INFO,0,0,i,sizeof(*i))&&i->ready;}
/* v10.296: VX_IOCTL_2D_ACCESS (vx855_2d.h) action 0 (a wait) with the depth surface's memory, its Coarse Z storage included */
int vxgl_z_written(vxgl_fb *fb) {return vxgl_z_written_at(&fb->sdepth);}
int vxgl_z_written_at(const vx3dn_surface *d) { /* v10.321: any depth surface (a framebuffer object's too) */
 DWORD in[3],rc=0;
 if(!d->format||!d->pitch)return 1; /* no depth surface the engine uses: nothing to tell */
 in[0]=0;in[1]=d->offset;in[2]=VX3DN_CZ_FORMAT(d->format)?VX3DN_CZ_END(d):d->offset+d->pitch*d->height;
 return vxd_call(0x85500017UL,in,12,&rc,4); /* v10.301 (review): a failed call is tried again (the flag stays set) */
}
int vxgl_vxd_query(uint32_t op,uint32_t owner,uint32_t *count) { /* v10.315; v10.316: a failure logged with its reason (the call's error, or the VxD's answer); v10.322: the owner */
 DWORD in[2],out[2];in[0]=op;in[1]=owner;out[0]=1;out[1]=0;*count=0;
 if(!vxd_call(VX_IOCTL_3DN_QUERY,in,8,out,8)){static int k;if(k++<8)vxgl_log("occlusion query %lu: the VxD call failed, error %lu\n",(unsigned long)op,(unsigned long)GetLastError());return 0;}
 if(out[0]){static int k;if(k++<8)vxgl_log("occlusion query %lu: the VxD answered %08lX%s\n",(unsigned long)op,(unsigned long)out[0],!op&&out[0]==VX3DN_BUSY?" (another context's query counts: this one in software)":"");return 0;}
 *count=out[1];return 1;}
/* v10.302: VX_IOCTL_2D_ACCESS action 0. v10.330 (review #19): 1 only when the VxD answered and did not answer VX2D_LOST (3: the engine
 * could not be reached); the callers then take their own fallback (glFinish locks the surfaces) or try again (rtt_done) */
int vxgl_engine_wait(void) {DWORD in=0,rc=0;int ok=vxd_call(0x85500017UL,&in,4,&rc,4)&&rc!=3;if(ok)vxgl_seq_idle();return ok;} /* v10.366: everything given is finished */
#define vxd_info vxgl_vxd_info
/* the core keeps its batch, template and clip buffers in statics and the engine is one: every use of them is under this recursive
 * lock (a second thread drawing with its own context waits; the swap's SetHWnd and Blt on the shared clipper too) */
void vxgl_engine_lock(void) {EnterCriticalSection(&engine);}
void vxgl_engine_unlock(void) {LeaveCriticalSection(&engine);}
/* v10.270: the cost of a submit, for VXGL.LOG (VXGL_LOG = 1): the time each takes here (QueryPerformanceCounter: the call into the VxD
 * and back), and at a context's end the VxD's own profile of its draw packets (TEXDBG 77: draws and their TSC cycles; AD: the cycles
 * in four parts: the entry checks, copy, validation and mapping; the sequencer open and clock check; the draw; the close) */
static double q_freq;static double sub_us[2],sub_max[2];static unsigned long sub_n[2],sub_retry;
static double qpc_us(void){LARGE_INTEGER c;if(q_freq<=0){LARGE_INTEGER f;q_freq=QueryPerformanceFrequency(&f)&&f.QuadPart?(double)f.QuadPart:1;}QueryPerformanceCounter(&c);return (double)c.QuadPart*1e6/q_freq;}
unsigned long vxgl_cycles(void) {return tsc_low();}
int vxgl_tsc_on;double vxgl_tsc[8];unsigned long vxgl_tsc_n[8]; /* .506 */
int vxgl_prof_on(void) {return log_on;}
static int texdbg(uint32_t code,uint32_t *out8) {uint32_t in[1];in[0]=code;memset(out8,0,32);return vxd_call(VX_IOCTL_3DN_TEXDBG,in,4,out8,32);}
double vxgl_prof_us[16];unsigned long vxgl_prof_n[16];double vxgl_prof_texels[2]; /* v10.271: VXGL's own parts (vxgl_mesa.c, vxgl_state.c, vxgl_tris.c): 0 state translations, 1 pipeline runs, 2 the core's primitives, 3 flushes */
/* v10.273: the 2D engine's state as the VxD sees it (VX_IOCTL_2D_STATS 0x85500016, VX_IOCTL_2D_CTRL 0x85500012 query FFFFFE10: the DRV's
 * binding), at each context's start and end: a desktop that draws slowly after an OpenGL program shows here which side stopped using the engine */
static void log_2d(const char *when) {
 uint32_t s[20],b[4],m[4],in=0xFFFFFE10UL;
 if(!log_on)return;
 memset(s,0,sizeof(s));memset(b,0,sizeof(b));
 if(!vxd_call(0x85500016UL,0,0,s,sizeof(s))){vxgl_log("2D engine %s: no answer\n",when);return;}
 (void)vxd_call(0x85500012UL,&in,4,b,16);
 memset(m,0,sizeof(m));in=0xFFFFFE1CUL;(void)vxd_call(0x85500012UL,&in,4,m,16); /* v10.273 */
 vxgl_log("2D engine %s: state %lu, ops %lu, fallbacks %lu, timeouts %lu, resets %lu, last result %08lX; the display driver: bound %lu, enabled %s, BUSY %s, moved in memory %lu times\n",
  when,(unsigned long)s[2],(unsigned long)s[4],(unsigned long)s[5],(unsigned long)s[14],(unsigned long)s[15],(unsigned long)s[19],(unsigned long)(b[0]&1),(b[0]&2)?"no":"yes",(b[0]&4)?"set":"clear",(unsigned long)m[0]);
}
/* v10.349: registry VXGL_SAMPLE = 1 (diagnostic, default 0; with VXGL_LOG): where the drawing thread's time goes, by sampling. A thread
 * of its own suspends the thread that made the context about a thousand times a second and keeps its EIP; at each slow 200-frame window
 * of prof_frames (over 30 ms a frame; at most four) VXGL.LOG gets the share of each module (VXGL.DLL, quake2.exe, ref_gl.dll, KERNEL32:
 * the VxD calls, ...) and VXGL.DLL's busiest addresses (RVAs, 32 bytes a bucket) to be named from the build's VXGL.map */
static HINSTANCE vxgl_inst;static int smp_on;static HANDLE smp_thread,smp_target;static volatile LONG smp_stop;
#define SMP_MAX 32768
static DWORD *smp_eip;static volatile unsigned long smp_n;static int smp_reports;
static DWORD WINAPI smp_main(LPVOID u) {
 CONTEXT cx;(void)u;
 while(!smp_stop){
  Sleep(1);
  if(SuspendThread(smp_target)==(DWORD)-1)break;
  memset(&cx,0,sizeof(cx));cx.ContextFlags=CONTEXT_CONTROL;
  if(GetThreadContext(smp_target,&cx)&&smp_n<SMP_MAX)smp_eip[smp_n++]=cx.Eip;
  ResumeThread(smp_target);
 }
 return 0;
}
static void smp_start(void) {
 DWORD id;
 if(!smp_on||smp_thread)return;
 if(!smp_eip)smp_eip=(DWORD*)malloc(SMP_MAX*sizeof(DWORD));if(!smp_eip)return;
 if(!DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&smp_target,THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,0)){vxgl_log("sampler: no thread handle\n");return;}
 smp_stop=0;smp_n=0;smp_reports=0;smp_thread=CreateThread(0,0,smp_main,0,0,&id);
 if(smp_thread)SetThreadPriority(smp_thread,THREAD_PRIORITY_TIME_CRITICAL);else{CloseHandle(smp_target);smp_target=0;}
 vxgl_log("sampler: %s\n",smp_thread?"on":"not started");
}
static void smp_end(void) {
 if(!smp_thread)return;
 smp_stop=1;WaitForSingleObject(smp_thread,2000);CloseHandle(smp_thread);smp_thread=0;CloseHandle(smp_target);smp_target=0;
}
static int smp_cmp(const void *a,const void *b) {DWORD x=*(const DWORD*)a,y=*(const DWORD*)b;return x<y?-1:x>y;}
typedef struct {DWORD rva;unsigned long n;} smp_run_t; /* v10.492 */
static int smp_run_cmp(const void *a,const void *b) {const smp_run_t *x=(const smp_run_t*)a,*y=(const smp_run_t*)b;return x->n<y->n?1:x->n>y->n?-1:(x->rva<y->rva?-1:x->rva>y->rva);}
static void smp_report(const char *what) {
 static DWORD mb[32];static unsigned long mc[32];unsigned long n=smp_n,i,k,nm=0,other=0;DWORD base=(DWORD)vxgl_inst;
 if(!smp_thread||!n)return;
 SuspendThread(smp_thread); /* the samples held still while they are read */
 for(i=0;i<n;i++){MEMORY_BASIC_INFORMATION mi;DWORD b=0;
  if(VirtualQuery((LPCVOID)smp_eip[i],&mi,sizeof(mi)))b=(DWORD)mi.AllocationBase;
  for(k=0;k<nm&&mb[k]!=b;k++);
  if(k==nm){if(nm<32){mb[nm]=b;mc[nm]=0;nm++;}else{other++;continue;}}
  mc[k]++;}
 vxgl_log("samples %s: %lu\n",what,n);
 for(k=0;k<nm;k++){char name[MAX_PATH];name[0]=0;if(!mb[k]||!GetModuleFileNameA((HMODULE)mb[k],name,sizeof(name)))strcpy(name,"(no module)");
  vxgl_log("  %5.1f%% %s (base %08lX)\n",100.0*mc[k]/n,name,(unsigned long)mb[k]);}
 if(other)vxgl_log("  %5.1f%% in more modules\n",100.0*other/n);
 /* VXGL.DLL's addresses: RVAs to 32 bytes, busiest first; v10.492: every one with 2 samples or more, up to 1500 (the busiest 120 left about
  * 45% of Lattice's samples unnamed): each as count << 32 | RVA, sorted */
 {static DWORD rv[SMP_MAX];static smp_run_t runs[SMP_MAX];unsigned long m=0,j,nr=0,shown=0;
  for(i=0;i<n;i++){MEMORY_BASIC_INFORMATION mi;if(VirtualQuery((LPCVOID)smp_eip[i],&mi,sizeof(mi))&&(DWORD)mi.AllocationBase==base)rv[m++]=(smp_eip[i]-base)&~31UL;}
  qsort(rv,m,sizeof(DWORD),smp_cmp);
  for(i=0;i<m;i=j){for(j=i;j<m&&rv[j]==rv[i];j++);runs[nr].rva=rv[i];runs[nr].n=j-i;nr++;}
  qsort(runs,nr,sizeof(runs[0]),smp_run_cmp);
  for(k=0;k<nr&&k<1500&&runs[k].n>=2;k++){vxgl_log("  VXGL+%06lX %lu\n",(unsigned long)runs[k].rva,runs[k].n);shown+=runs[k].n;}
  vxgl_log("  (VXGL: %lu samples, %lu in the lines above)\n",m,shown);}
 smp_n=0;ResumeThread(smp_thread);
}
static double pf_wall,pf_us[16],pf_sub,pf_tsc[8];static unsigned long pf_n[16]; /* v10.347: prof_frames' last report */
static double prof_wall0; /* v10.347: when the context's profile began (QueryPerformanceCounter microseconds) */
static unsigned __int64 pf_c0; /* v10.497: the TSC at the profile's start (its clock rate from the first report's elapsed time, no wait) */
static void prof_clear(void) {uint32_t o[8];if(!log_on)return;prof_wall0=qpc_us();pf_c0=tsc64();pf_wall=0;{int k;for(k=0;k<16;k++){pf_us[k]=0;pf_n[k]=0;}for(k=0;k<8;k++)pf_tsc[k]=vxgl_tsc[k];}pf_sub=0;memset(vx_d3d_vhist,0,sizeof(vx_d3d_vhist));memset(vx_d3d_cut,0,sizeof(vx_d3d_cut));log_2d("at the start");texdbg(0x77,o);texdbg(0xAD,o);texdbg(0xA4,o);texdbg(0xA9,o);memset(vxgl_prof_us,0,sizeof(vxgl_prof_us));memset(vxgl_prof_n,0,sizeof(vxgl_prof_n));vxgl_prof_texels[0]=vxgl_prof_texels[1]=0;memset(vxgl_ring_stat,0,sizeof(vxgl_ring_stat));sub_us[0]=sub_us[1]=sub_max[0]=sub_max[1]=0;sub_n[0]=sub_n[1]=0;sub_retry=0;vxgl_batch_calls=vxgl_batch_packets=0;memset(vxgl_batch_flushes,0,sizeof(vxgl_batch_flushes));texdbg(0xC1,o);texdbg(0xC3,o);texdbg(0xC9,o);}
/* v10.347: where a frame's time goes, for VXGL.LOG: every 200 swaps (the steady state: a timedemo, a level) and at the context's end, the
 * wall time against VXGL's own parts: pipeline runs (they hold the state translations, the core's primitives and the VxD's submits),
 * clears, the swap's blit; the rest is outside VXGL (the application, Mesa's front end: glVertex and the like, glTexImage's own copy) */
static void prof_frames(int end) {
 double now,mhz,t0,t1,fr,wall,pl,cl,sw,sb;unsigned long c0,c1;int k;static double mhz_;
 if(!log_on)return;
 /* v10.497 (board, VXFLAT v2.4: a 30 ms frame at the 200th swap, every run with VXGL.LOG on): the clock rate from the TSC and the counter since
  * prof_clear, not a 20 ms busy wait in the frame that reports first (the wait only when the profile began too recently to measure) */
 if(mhz_<=0){double dq=qpc_us()-prof_wall0;if(prof_wall0>0&&dq>100000)mhz_=(double)(__int64)(tsc64()-pf_c0)/dq;
  if(mhz_<=0){t0=qpc_us();c0=tsc_low();while((t1=qpc_us())-t0<20000);c1=tsc_low();mhz_=(double)(c1-c0)/(t1-t0);}}mhz=mhz_;
 now=qpc_us();if(pf_wall<=0)pf_wall=prof_wall0;
 fr=(double)(vxgl_prof_n[7]-pf_n[7]);wall=now-pf_wall;pl=(vxgl_prof_us[1]-pf_us[1])/mhz;cl=(vxgl_prof_us[6]-pf_us[6])/mhz;sw=(vxgl_prof_us[7]-pf_us[7])/mhz;sb=(sub_us[0]+sub_us[1]-pf_sub)/mhz;
 if(fr>0&&wall>0){
  vxgl_log("frames%s: %.0f in %.2f s (%.1f a second); a frame %.2f ms: pipeline runs %.2f (%.0f%%; the VxD's submits in them %.2f), clears %.2f, the swap %.2f, outside VXGL (the application, Mesa's front end) %.2f (%.0f%%); a frame has %.0f pipeline runs, %.0f primitive calls, %.1f state translations, %.1f texture uploads\n",
   end?" (to the end)":"",fr,wall/1e6,fr*1e6/wall,wall/fr/1000,pl/fr/1000,100*pl/wall,sb/fr/1000,cl/fr/1000,sw/fr/1000,(wall-pl-cl-sw)/fr/1000,100*(wall-pl-cl-sw)/wall,
   (vxgl_prof_n[1]-pf_n[1])/fr,(vxgl_prof_n[2]-pf_n[2])/fr,(vxgl_prof_n[0]-pf_n[0])/fr,(vxgl_prof_n[4]-pf_n[4])/fr);
  /* v10.363: the swap in its parts: settling the buffers (the queued packets sent to the VxD, each after the draw before it), the blit */
  vxgl_log("  the swap's parts a frame: settling the buffers (the queue sent) %.2f ms, the blit %.2f ms, the rest %.2f ms\n",(vxgl_prof_us[13]-pf_us[13])/mhz/fr/1000,
   (vxgl_prof_us[14]-pf_us[14])/mhz/fr/1000,(sw-(vxgl_prof_us[13]-pf_us[13])/mhz-(vxgl_prof_us[14]-pf_us[14])/mhz)/fr/1000);
  /* .511: with ACCEL_OPENGL_TSCPROF the ICD's parts too, as VXFLAT shows them (a real game's frame, not only the synthetic scenes) */
  if(vxgl_tsc_on)vxgl_log("  the ICD's parts a frame (ACCEL_OPENGL_TSCPROF), ms: pipeline runs %.2f; in them triangle-list runs %.2f, strips/polygons/fans %.2f, the SSE pass's transform and lighting %.2f, Mesa's vertex emit %.2f, packets to the VxD %.2f; swaps %.2f\n",
   (vxgl_tsc[0]-pf_tsc[0])/mhz/fr/1000,(vxgl_tsc[1]-pf_tsc[1])/mhz/fr/1000,(vxgl_tsc[2]-pf_tsc[2])/mhz/fr/1000,(vxgl_tsc[3]-pf_tsc[3])/mhz/fr/1000,
   (vxgl_tsc[4]-pf_tsc[4])/mhz/fr/1000,(vxgl_tsc[5]-pf_tsc[5])/mhz/fr/1000,(vxgl_tsc[6]-pf_tsc[6])/mhz/fr/1000);}
 if(smp_thread){if(fr>0&&wall/fr>30000&&smp_reports<4){smp_reports++;smp_report(end?"(to the end)":"(this window)");}else smp_n=0;} /* v10.349 */
 pf_wall=now;for(k=0;k<16;k++){pf_us[k]=vxgl_prof_us[k];pf_n[k]=vxgl_prof_n[k];}pf_sub=sub_us[0]+sub_us[1];for(k=0;k<8;k++)pf_tsc[k]=vxgl_tsc[k];
 if(end)pf_wall=0;
}
void vxgl_prof_swap(void) {if(log_on&&vxgl_prof_n[7]&&vxgl_prof_n[7]%200==0)prof_frames(0);} /* v10.347 */
static void prof_report(void) {
 uint32_t o[8],s[8];double mhz,t0,t1;unsigned long c0,c1;
 if(!log_on)return;
 log_2d("at the end");
 t0=qpc_us();c0=tsc_low();while((t1=qpc_us())-t0<20000);c1=tsc_low();mhz=(double)(c1-c0)/(t1-t0);
 vxgl_log("submits: %lu draw packets, %.1f microseconds each here (max %.0f); %lu clears, %.1f each; %lu busy retries\n",
  sub_n[0],sub_n[0]?sub_us[0]/sub_n[0]/mhz:0,sub_max[0]/mhz,sub_n[1],sub_n[1]?sub_us[1]/sub_n[1]/mhz:0,sub_retry);
 if(texdbg(0x77,o)&&o[0]==0x77&&o[1]) {
  vxgl_log("VxD: %lu draw packets, %.1f microseconds each in VX3DN_submit (CPU %.0f MHz); %lu engine writes, %lu status reads, %lu other reads a packet\n",
   (unsigned long)o[1],o[2]*256.0/mhz/o[1],mhz,(unsigned long)(o[3]/o[1]),(unsigned long)(o[4]/o[1]),(unsigned long)(o[5]/o[1]));
  if(texdbg(0xAD,s)&&s[0]==0xAC)vxgl_log("VxD: each draw packet's parts: checks, copy, validation, mapping %.1f; open and clock %.1f; the draw %.1f; the close %.1f microseconds\n",
   s[1]*256.0/mhz/o[1],s[2]*256.0/mhz/o[1],s[3]*256.0/mhz/o[1],s[4]*256.0/mhz/o[1]);
 }
 { /* v10.271: the command fetcher's counters (TEXDBG A4: packets fetched, words, packets through the registers, fetches not finished, the
    * most words; A9: draws sent without state (the same state as the one before), cycles building the words, waiting for the draw before,
    * in all) */
  uint32_t f[8],k[8];
  if(texdbg(0xA4,f)&&f[1])vxgl_log("VxD fetcher: %lu packets, %.0f command words each (most %lu), %lu through the registers, %lu not finished\n",
   (unsigned long)f[1],(double)f[2]/f[1],(unsigned long)f[5],(unsigned long)f[3],(unsigned long)f[4]);
  if(texdbg(0xA9,k)&&f[1])vxgl_log("VxD fetcher: %lu of them without state; a packet: building its words %.1f, waiting for the draw before %.1f, in all %.1f microseconds\n",
   (unsigned long)k[2],k[3]*256.0/mhz/f[1],k[4]*256.0/mhz/f[1],k[5]*256.0/mhz/f[1]); /* v10.272: A8's [1] is the switch, the counters [2..5] */
  if(f[1]&&texdbg(0xC9,k)&&k[0]==0xC8)vxgl_log("VxD building a packet's words (microseconds): the state-cache compare %.1f, the emitter %.1f (state set-up %.1f, texture set-up %.1f, program %.1f, primitive set-up and vertices %.1f), keeping the state %.1f\n",
   k[1]*256.0/mhz/f[1],k[2]*256.0/mhz/f[1],k[4]*256.0/mhz/f[1],k[5]*256.0/mhz/f[1],k[6]*256.0/mhz/f[1],k[7]*256.0/mhz/f[1],k[3]*256.0/mhz/f[1]); /* v10.286 */
 }
 {uint32_t d[8];static const char *tn[8]={"uncached","write-combining","?","?","write-through","write-protect","write-back","?"}; /* v10.283 */
  if(texdbg(0xC7,d)&&d[0]==0xC7&&d[1])vxgl_log("VxD command block at %08lX (physical %08lX): %s (the frame buffer's start: %s), page bits %03lX; 16 KB of CPU writes: %lu cycles (%.0f MB/s), to cached memory %lu cycles\n",
   (unsigned long)d[1],(unsigned long)d[2],d[3]<8?tn[d[3]]:"MTRRs not read",d[4]<8?tn[d[4]]:"MTRRs not read",(unsigned long)d[5],(unsigned long)d[6],d[6]?16384.0*mhz/d[6]:0.0,(unsigned long)d[7]);}
 {uint32_t d[8];if(texdbg(0xC3,d)&&d[0]==0xC2&&(d[2]||d[3]))vxgl_log("VxD state diff (mode %lu): %lu state writes left out, %lu sent (%.0f%% left out)\n",(unsigned long)d[1],(unsigned long)d[2],(unsigned long)d[3],100.0*d[2]/(d[2]+d[3]));} /* v10.276 */
 if(vxgl_batch_calls){uint32_t b[8];
  vxgl_log("queue: %lu packets in %lu calls (%.1f a call); sent for a full queue %lu, a packet going alone %lu, the rest %lu times\n",vxgl_batch_packets,vxgl_batch_calls,(double)vxgl_batch_packets/vxgl_batch_calls,
   vxgl_batch_flushes[0],vxgl_batch_flushes[1],vxgl_batch_flushes[2]);
  if(texdbg(0xC1,b)&&b[0]==0xC0)vxgl_log("VxD batches: %lu calls, %lu packets, %lu fetch starts (%.1f packets a start, most %lu)%s\n",(unsigned long)b[1],(unsigned long)b[2],(unsigned long)b[3],
   b[3]?(double)b[2]/b[3]:0.0,(unsigned long)b[4],b[5]?", XP's batch end between packets":"");}
 prof_frames(1); /* v10.347 */
 /* v10.363: the batches the core sent, by size and by what ended them */
 {uint32_t t=0,k;static const char *why[10]={"not named","a state change (new template)","another primitive kind","the vertex limit","the run limit","another flush of VXGL's","a state translation","a swap, glFlush or glFinish","a clear","a texture upload"};
  char b[600];int o=0;for(k=0;k<6;k++)t+=vx_d3d_vhist[k];
  if(t){vxgl_log("batches: %lu; vertices 1-16 %lu, 17-48 %lu, 49-96 %lu, 97-144 %lu, 145-191 %lu, 192 (full) %lu\n",(unsigned long)t,(unsigned long)vx_d3d_vhist[0],(unsigned long)vx_d3d_vhist[1],
    (unsigned long)vx_d3d_vhist[2],(unsigned long)vx_d3d_vhist[3],(unsigned long)vx_d3d_vhist[4],(unsigned long)vx_d3d_vhist[5]);
   for(k=0;k<10;k++)if(vx_d3d_cut[k])o+=sprintf(b+o,"%s%s %lu",o?", ":"",why[k],(unsigned long)vx_d3d_cut[k]);
   vxgl_log("batches ended by: %s\n",b);}}
 {DWORD q=0xFFFFFE36UL,o[4]={0,0,0,0}; /* v10.363: ACCEL_3D_ZFIX's count at the end */
  if(vxd_call(0x85500012UL,&q,4,o,16)&&o[2]==0x4F464946UL&&(o[3]&255))vxgl_log("ACCEL_3D_ZFIX %lu: sent by %lu draws (since the VxD loaded)\n",o[3]&255,o[3]>>8);}
 {static const char *nm[4]={"state translations","pipeline runs","the core's primitives","flushes"};int k;
  for(k=0;k<4;k++)if(vxgl_prof_n[k])vxgl_log("VXGL: %lu %s, %.1f microseconds each (%.0f ms in all)\n",vxgl_prof_n[k],nm[k],vxgl_prof_us[k]/mhz/vxgl_prof_n[k],vxgl_prof_us[k]/mhz/1000);
  /* v10.289: a translation's parts: the texture copies it made, its send of the batch drawn with the old state (the VxD's submit), the rest
   * VXGL's own (Mesa's state to the core's) */
  if(vxgl_prof_n[0]){double n=(double)vxgl_prof_n[0],up=vxgl_prof_us[4],sd=vxgl_prof_us[5],own=vxgl_prof_us[0]-up-sd;
   vxgl_log("VXGL: a state translation's parts (microseconds): texture uploads %.1f (%lu uploads, %.1f each), sending the batch %.1f (%lu sends), its own work %.1f\n",
    up/mhz/n,vxgl_prof_n[4],vxgl_prof_n[4]?up/mhz/vxgl_prof_n[4]:0.0,sd/mhz/n,vxgl_prof_n[5],own/mhz/n);}
  /* v10.303: an upload's parts, in all: the video memory made (CreateSurface), the batch sent before the copy, each level's Lock, the copy, the Unlock */
  if(vxgl_prof_n[4]){static const char *pn[5]={"made","batch sent","Lock","copy","Unlock"};char b[320];int o=0;
   for(k=8;k<13;k++)o+=sprintf(b+o,"%s%s %.1f ms (%lu)",k>8?", ":"",pn[k-8],vxgl_prof_us[k]/mhz/1000,vxgl_prof_n[k]);
   vxgl_log("VXGL: the texture uploads' parts in all: %s; %.0f texels read directly, %.0f by FetchTexelc (%.3f microseconds a texel copied)\n",b,vxgl_prof_texels[0],vxgl_prof_texels[1],
    vxgl_prof_texels[0]+vxgl_prof_texels[1]>0?vxgl_prof_us[11]/mhz/(vxgl_prof_texels[0]+vxgl_prof_texels[1]):0.0);
   vxgl_log("VXGL: texture ring (ACCEL_OPENGL_TEXRING %d, TEX16 %d): %lu uploads into a free copy, %lu after sending the queue, %lu into a new copy, %lu waited for the engine\n",
    vxgl_gl_texring,vxgl_gl_tex16,vxgl_ring_stat[0],vxgl_ring_stat[1],vxgl_ring_stat[2],vxgl_ring_stat[3]);}}
}
/* the core's submit service: busy means the engine is still on another client's work; try again for half a second (as the HAL) */
static void fb_settle_only(vxgl_fb *fb);
/* v10.275: the queue's send (vxgl_batch.c): n packets in one call; busy is tried again for half a second, as a single packet */
uint32_t vxgl_batch_send(const vx3dn_packet *p,uint32_t n,uint32_t *done) {
 uint32_t out[2],tries=0;DWORD start=0;
 *done=0;
 for(;;) {
  unsigned long t0=log_on?tsc_low():0;int ok;
  out[0]=VX3DN_LOST;out[1]=0;ok=vxd_call(VX_IOCTL_3DN_SUBMITN,p,n*sizeof(*p),out,8);
  if(log_on){double d=(double)(unsigned long)(tsc_low()-t0);sub_us[0]+=d;sub_n[0]+=out[1];if(d>sub_max[0])sub_max[0]=d;if(ok&&out[0]==VX3DN_BUSY)sub_retry++;}
  if(!ok)return VX3DN_LOST;
  *done=out[1];
  if(out[0]!=VX3DN_BUSY||out[1])return out[0];
  if(!tries)start=GetTickCount();
  if(GetTickCount()-start>500)return out[0];
  tries++;st_busy++;if(tries>=8)st_sleep++;Sleep(tries<8?0:1);
 }
}
/* v10.366: the part of a window's surface drawn since it was last shown (vxgl_fb dmg) */
static void fb_damage(vxgl_fb *fb,int k,long l,long t,long r,long b) {
 RECT *d=&fb->dmg[k];if(l<0)l=0;if(t<0)t=0;if(r>(long)fb->w)r=(long)fb->w;if(b>(long)fb->h)b=(long)fb->h;if(l>=r||t>=b)return;
 if(!fb->dmg_on[k]){d->left=l;d->top=t;d->right=r;d->bottom=b;fb->dmg_on[k]=1;return;}
 if(l<d->left)d->left=l;if(t<d->top)d->top=t;if(r>d->right)d->right=r;if(b>d->bottom)d->bottom=b;
}
static void fb_damage_all(vxgl_fb *fb) {fb_damage(fb,0,0,0,(long)fb->w,(long)fb->h);fb_damage(fb,1,0,0,(long)fb->w,(long)fb->h);}
static uint32_t vxgl_submit_t(void *ctx,const vx3dn_packet *p);
uint32_t vxgl_submit(void *ctx,const vx3dn_packet *p) {uint32_t r;VXT_BEGIN;r=vxgl_submit_t(ctx,p);VXT_END(5);return r;} /* .506 */
static int submit_lost(vxgl_ctx *c) {
 if(!c||c->batch_lost==vxgl_batch_lost)return 0;
 c->batch_lost=vxgl_batch_lost;
 return 1;
}
static uint32_t vxgl_submit_t(void *ctx,const vx3dn_packet *p) {
 uint32_t rc=VX3DN_LOST,tries=0;DWORD start=0;vxgl_ctx *c=(vxgl_ctx*)ctx;int queued;
 if(c)fb_settle_only(c->fb); /* v10.267: the engine draws into the surfaces swrast may still hold */
 if(c&&c->fb&&((p->op==VX3DN_DRAW&&(p->write_mask&15))||(p->clear_flags&1))){vxgl_fb *f=c->fb; /* v10.366: colour written */
  if(f->sback.format&&p->target.offset==f->sback.offset)fb_damage(f,0,(long)p->clip.left,(long)p->clip.top,(long)p->clip.right,(long)p->clip.bottom);
  else if(f->sfront.format&&p->target.offset==f->sfront.offset)fb_damage(f,1,(long)p->clip.left,(long)p->clip.top,(long)p->clip.right,(long)p->clip.bottom);}
 {static int n;if(n<24){n++;vxgl_log("packet %d: op %lu target %08lX pitch %lu %lux%lu format %lu, depth %08lX format %lu, clip %ld,%ld-%ld,%ld, %lu vertices, clear %04lX\n",n,
  (unsigned long)p->op,(unsigned long)p->target.offset,(unsigned long)p->target.pitch,(unsigned long)p->target.width,(unsigned long)p->target.height,(unsigned long)p->target.format,
  (unsigned long)p->depth.offset,(unsigned long)p->depth.format,(long)p->clip.left,(long)p->clip.top,(long)p->clip.right,(long)p->clip.bottom,(unsigned long)p->vertex_count,(unsigned long)p->clear_flags);}} /* v1.3: the desktop artifact */
 if(submit_lost(c))return VX3DN_LOST;
 queued=vxgl_batch_add(p); /* .573 (peer review): a full queue is sent here and the queue before a packet that goes alone below: a loss found by either is answered now (it waited for the context's next packet, and the lone packet was still sent) */
 if(!queued)vxgl_batch_flush_why(1); /* A standalone packet follows the queued ones. */
 if(submit_lost(c))return VX3DN_LOST;
 if(queued){if(c)c->stats[4]++;return VX3DN_OK;}
 for(;;) {
  unsigned long t0=log_on?tsc_low():0;int ok=vxd_call(VX_IOCTL_3DN_SUBMIT,p,sizeof(*p),&rc,4);
  if(log_on){double d=(double)(unsigned long)(tsc_low()-t0); /* cycles: converted in the report */int k=p->op==VX3DN_CLEAR;sub_us[k]+=d;sub_n[k]++;if(d>sub_max[k])sub_max[k]=d;if(ok&&rc==VX3DN_BUSY)sub_retry++;}
  if(!ok){vxgl_seq_alone(VX3DN_LOST);return VX3DN_LOST;}
  if(rc!=VX3DN_BUSY)break;
  if(!tries)start=GetTickCount();
  if(GetTickCount()-start>500)break;
  tries++;st_busy++;if(tries>=8)st_sleep++;Sleep(tries<8?0:1);
 }
 vxgl_seq_alone(rc); /* v10.366 */
 if(!rc&&c)c->stats[4]++;
 if(rc&&rc!=VX3DN_RECOVERED){static int n;if(n++<64)vxgl_log("submit: op %lu rc %lu\n",(unsigned long)p->op,(unsigned long)rc);}
 return rc;
}

/* ---------------------------------------------------------------- DirectDraw */
/* the primary: Restore fails (DDERR_WRONGMODE) once the display mode differs from the one it was made in, so it is made again then */
static int primary_ok(void) {
 DDSURFACEDESC2 sd;
 if(!vxgl_dd)return 0;
 if(primary&&IDirectDrawSurface7_IsLost(primary)!=DD_OK&&IDirectDrawSurface7_Restore(primary)!=DD_OK){vxgl_log("primary made again (mode change)\n");IDirectDrawSurface7_Release(primary);primary=0;}
 if(!primary) {
  memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS;sd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
  if(IDirectDraw7_CreateSurface(vxgl_dd,&sd,&primary,0)!=DD_OK){vxgl_log("primary failed\n");primary=0;return 0;}
  /* v10.321: a clipper that does not attach is logged; clip_ok attaches it again before each blit to the screen, or the frame goes through GDI */
  if(clipper){HRESULT r=IDirectDrawSurface7_SetClipper(primary,clipper);if(r!=DD_OK)vxgl_log("primary: SetClipper %08lX\n",(unsigned long)r);}
 }
 return 1;
}
/* v10.321: a blit to the primary only with the clipper on it and on this window: without it DirectDraw writes the rectangle over whatever
 * covers the window, or anywhere on the screen. 0: the frame goes through GDI instead (which clips to the window itself). */
static int clip_ok(HWND w) {
 LPDIRECTDRAWCLIPPER cur=0;HRESULT r;static int logged;
 if(!clipper||!primary){if(logged++<4)vxgl_log("swap: no clipper (%p): through GDI\n",(void*)clipper);return 0;}
 r=IDirectDrawSurface7_GetClipper(primary,&cur);if(cur)IDirectDrawClipper_Release(cur);
 if(r!=DD_OK||cur!=clipper){r=IDirectDrawSurface7_SetClipper(primary,clipper);if(r!=DD_OK){if(logged++<4)vxgl_log("swap: SetClipper %08lX: through GDI\n",(unsigned long)r);return 0;}}
 r=IDirectDrawClipper_SetHWnd(clipper,0,w);if(r!=DD_OK){if(logged++<4)vxgl_log("swap: SetHWnd %08lX: through GDI\n",(unsigned long)r);return 0;}
 return 1;
}
static int dd_init(void) {
 HMODULE m;HRESULT (WINAPI *create)(GUID*,LPVOID*,REFIID,IUnknown*);
 DWORD (WINAPI *vram)(DWORD*,DWORD*);
 if(vxgl_dd)return 1;
 m=LoadLibraryA("DDRAW.DLL");if(!m)return 0;
 create=(HRESULT (WINAPI *)(GUID*,LPVOID*,REFIID,IUnknown*))GetProcAddress(m,"DirectDrawCreateEx");
 if(!create||create(NULL,(LPVOID*)&vxgl_dd,&iid_dd7,NULL)!=DD_OK){vxgl_dd=0;vxgl_log("DirectDrawCreateEx failed\n");FreeLibrary(m);return 0;} /* v10.330 (review #23): the reference given back (a later call loads it again); kept for the process once DirectDraw is made */
 if(IDirectDraw7_SetCooperativeLevel(vxgl_dd,NULL,DDSCL_NORMAL)!=DD_OK){vxgl_log("SetCooperativeLevel failed\n");}
 if(IDirectDraw7_CreateClipper(vxgl_dd,0,&clipper,0)!=DD_OK)clipper=0;
 primary_ok();
 /* DirectDraw maps the HAL in the shared arena without putting it in this process's module list (board, v10.149): GetModuleHandle
  * finds nothing, LoadLibrary gives the same (already initialised) copy */
 m=GetModuleHandleA("VXDDRAW.DLL");if(!m)m=LoadLibraryA("VXDDRAW.DLL");vram=m?(DWORD (WINAPI *)(DWORD*,DWORD*))GetProcAddress(m,"VX_DD_Vram"):0;
 if(!vram||!vram(&vxgl_vram_linear,&vxgl_vram_size)){vxgl_log("VXDDRAW.DLL VX_DD_Vram missing: no hardware drawing\n");vxgl_vram_linear=0;}
 dd_surface=m?(DWORD (WINAPI *)(void*,DWORD*,DWORD*))GetProcAddress(m,"VX_DD_Surface"):0;
 dd_splane=m?(DWORD (WINAPI *)(void*,DWORD*,DWORD*))GetProcAddress(m,"VX_DD_StencilPlane"):0;
 dd_present=m?(DWORD (WINAPI *)(DWORD))GetProcAddress(m,"VX_DD_Present"):0;
 if(!dd_surface)vxgl_log("VXDDRAW.DLL has no VX_DD_Surface (older than v10.241): no hardware drawing\n");
 vxgl_log("DirectDraw ready: primary %p, video memory %08lX + %lu KB\n",(void*)primary,(unsigned long)vxgl_vram_linear,(unsigned long)(vxgl_vram_size>>10));
 {DWORD off=0,pitch=0;if(primary&&dd_surface&&dd_surface(primary,&off,&pitch))vxgl_log("primary: offset %08lX pitch %lu\n",(unsigned long)off,(unsigned long)pitch);}
 return 1;
}
unsigned long vxgl_texmem_freed; /* v10.287 */
static void dd_release_surface(LPDIRECTDRAWSURFACE7 *s) {if(*s){IDirectDrawSurface7_Release(*s);*s=0;vxgl_texmem_freed++;}}
/* a surface's size, pitch and CPU address (from a lock), and its video-memory offset (from the HAL: the lock's address is DirectDraw's
 * alias of video memory on Windows 98, not the frame buffer's own address; board, v10.240) */
static int surface_where(LPDIRECTDRAWSURFACE7 s,vx3dn_surface *d,GLubyte **cpu) {
 DDSURFACEDESC2 sd;HRESULT r;DWORD off=0,pitch=0;
 vxgl_batch_flush(); /* v10.275 */
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 r=IDirectDrawSurface7_Lock(s,NULL,&sd,DDLOCK_WAIT|DDLOCK_NOSYSLOCK,NULL);if(r!=DD_OK){vxgl_log("lock for the offset failed %08lX\n",(unsigned long)r);return 0;}
 IDirectDrawSurface7_Unlock(s,NULL);
 *cpu=(GLubyte*)sd.lpSurface;d->pitch=(uint32_t)sd.lPitch;d->width=sd.dwWidth;d->height=sd.dwHeight;
 if(!dd_surface||!dd_surface(s,&off,&pitch)){vxgl_log("surface %p (lock %p): the HAL has no video memory offset for it\n",(void*)s,sd.lpSurface);return 0;}
 if(pitch!=(DWORD)sd.lPitch){vxgl_log("surface %p: the HAL's pitch %lu, the lock's %ld\n",(void*)s,(unsigned long)pitch,(long)sd.lPitch);return 0;}
 d->offset=off;
 if(off&255){vxgl_log("surface %p: offset %08lX not on 256 bytes\n",(void*)s,(unsigned long)off);return 0;}
 vxgl_log("surface %p: offset %08lX pitch %lu (lock %p)\n",(void*)s,(unsigned long)off,(unsigned long)pitch,sd.lpSurface);
 return 1;
}
static void soft_front_free(vxgl_fb *fb);
static void fb_unlock_now(vxgl_fb *fb);
static void fb_free_surfaces(vxgl_fb *fb) {vxgl_batch_flush();if(fb->held||fb->hold)fb_unlock_now(fb);dd_release_surface(&fb->front);memset(&fb->sfront,0,sizeof(fb->sfront));soft_front_free(fb);fb->pfront=0;fb->fstate=0;dd_release_surface(&fb->depth);dd_release_surface(&fb->back);fb->w=fb->h=0;memset(&fb->sback,0,sizeof(fb->sback));memset(&fb->sdepth,0,sizeof(fb->sdepth));fb->pback=fb->pdepth=fb->pstencil=0;fb->soff=fb->spitch=0;}
static void fb_free_mem(vxgl_fb *fb) {if(fb->fmem)_mesa_free(fb->fmem);fb->fmem=0;fb->fmem_size=0;if(fb->cmem)_mesa_free(fb->cmem);if(fb->zmem)_mesa_free(fb->zmem);if(fb->smem)_mesa_free(fb->smem);fb->cmem=fb->zmem=fb->smem=0;fb->cmem_size=fb->zmem_size=fb->smem_size=0;}
/* system memory for a buffer swrast must write when its surface is missing or cannot be locked (kept, grown when needed) */
static GLubyte *fb_mem(GLubyte **m,DWORD *size,DWORD need) {
 if(!need)need=4;
 if(*size<need){if(*m)_mesa_free(*m);*m=(GLubyte*)_mesa_calloc(need);*size=*m?need:0;}
 return *m;
}
/* v10.261 (review): a double-buffered window's front buffer in system memory when its surface cannot be made or locked. Before, the span
 * functions then used the back buffer's memory: a front clear came out in the back. A DIB section of the back's pitch and format in a
 * memory DC, so GDI fills it from the window and shows it there (the surface path does the same with DirectDraw blits); if no DIB can be
 * made, plain memory (kept apart from the back, not shown). */
static void soft_front_free(vxgl_fb *fb) {
 if(fb->fdc){if(fb->fold)SelectObject((HDC)fb->fdc,(HGDIOBJ)fb->fold);DeleteDC((HDC)fb->fdc);}
 if(fb->fbmp)DeleteObject((HGDIOBJ)fb->fbmp);
 fb->fdc=fb->fbmp=fb->fold=0;fb->fbits=0;fb->fsoft=0;
}
static GLubyte *soft_front(vxgl_fb *fb) {
 struct {BITMAPINFOHEADER h;DWORD m[3];} bi;void *bits=0;DWORD bytes=fb->cb==16?2:4;
 if(fb->fbits)return fb->fbits;
 if(!fb->w||!fb->h||!fb->sback.pitch)return 0;
 if(!(fb->sback.pitch%bytes)&&!(fb->sback.pitch&3)) { /* a DIB's rows are dword aligned: its width in pixels gives the back's pitch */
  memset(&bi,0,sizeof(bi));bi.h.biSize=sizeof(bi.h);bi.h.biWidth=(LONG)(fb->sback.pitch/bytes);bi.h.biHeight=-(LONG)fb->h;bi.h.biPlanes=1;
  bi.h.biBitCount=(WORD)(bytes*8);bi.h.biCompression=BI_BITFIELDS;
  if(bytes==2){bi.m[0]=0xF800UL;bi.m[1]=0x07E0UL;bi.m[2]=0x001FUL;}else{bi.m[0]=0xFF0000UL;bi.m[1]=0xFF00UL;bi.m[2]=0xFFUL;}
  fb->fbmp=(void*)CreateDIBSection(0,(BITMAPINFO*)&bi,DIB_RGB_COLORS,&bits,0,0);
  if(fb->fbmp&&bits&&(fb->fdc=(void*)CreateCompatibleDC(0))!=0) {
   fb->fold=(void*)SelectObject((HDC)fb->fdc,(HGDIOBJ)fb->fbmp);fb->fbits=(GLubyte*)bits;
   vxgl_log("front buffer %ux%u in system memory (a DIB section)\n",fb->w,fb->h);return fb->fbits;
  }
  if(fb->fbmp)DeleteObject((HGDIOBJ)fb->fbmp);fb->fbmp=0;
 }
 fb->fbits=fb_mem(&fb->fmem,&fb->fmem_size,fb->sback.pitch*fb->h);
 vxgl_log("front buffer %ux%u in plain memory (not shown in the window)\n",fb->w,fb->h);
 return fb->fbits;
}
static void soft_front_fill(vxgl_fb *fb) {HDC w;if(!fb->fdc||!(w=GetDC(fb->hwnd)))return;BitBlt((HDC)fb->fdc,0,0,(int)fb->w,(int)fb->h,w,0,0,SRCCOPY);ReleaseDC(fb->hwnd,w);GdiFlush();}
static void soft_front_show(vxgl_fb *fb) {HDC w;if(!fb->fdc||!(w=GetDC(fb->hwnd)))return;GdiFlush();BitBlt(w,0,0,(int)fb->w,(int)fb->h,(HDC)fb->fdc,0,0,SRCCOPY);ReleaseDC(fb->hwnd,w);}
/* the engine may still be drawing into the surfaces (packets sent, not finished): a lock waits for it (the HAL's Lock retires 3D work),
 * so their video memory is not handed to a new surface while it is written */
static volatile int vxgl_gone; /* v10.260: set at DLL_PROCESS_DETACH: every Drv* entry returns at once */
static void fb_wait(vxgl_fb *fb) {
 DDSURFACEDESC2 sd;memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 if(!vxgl_gone)vxgl_hw_flush_all(); /* v10.287: the core's batch may still draw into these surfaces (a window found dead while its context's draws wait) */
 vxgl_fb_settle(fb);
 if(fb->back&&IDirectDrawSurface7_Lock(fb->back,NULL,&sd,DDLOCK_WAIT|DDLOCK_NOSYSLOCK,NULL)==DD_OK)IDirectDrawSurface7_Unlock(fb->back,NULL);
}
static int window_rects(vxgl_fb *fb,RECT *dst,RECT *src,int *screen);
/* v10.366 (board: 3D Pipes after Quake 2 showed Quake's menu through its opening dissolve, in the preview too, Quake not running): a new
 * colour surface held whatever video memory last had there. Pipes draws single-buffered and starts with scissored clears of small tiles,
 * a glFlush after each; every flush showed the whole surface (present_single), the tiles not yet cleared the old pixels. A single-buffered
 * window's surface (and a double-buffered window's front, vxgl_front_load) now starts as what the screen shows in the window, as drawing
 * into the window itself would; when that copy cannot be made (another desktop depth, the window not on the primary, a failed blit) it
 * starts black. A double-buffered window's back buffer starts black (its contents are undefined until drawn; the old frame was a leak). */
static int fb_seed(vxgl_fb *fb,LPDIRECTDRAWSURFACE7 s,int from_window) {
 RECT dst,src;int scr;HRESULT r;DDBLTFX fx;
 if(!s)return 0;
 if(from_window&&fb->cb==desktop_bpp&&primary_ok()&&window_rects(fb,&dst,&src,&scr)&&scr){
  r=IDirectDrawSurface7_Blt(s,&src,primary,&dst,DDBLT_WAIT,NULL);if(r==DD_OK)return 1;
  {static int logged;if(logged++<4)vxgl_log("surface %ux%u: the window's pixels not copied %08lX: black\n",fb->w,fb->h,(unsigned long)r);}}
 memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwFillColor=0;
 r=IDirectDrawSurface7_Blt(s,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx);
 if(r!=DD_OK){ /* .532 (review of .530, B6): the CPU clears it then; only a surface that cannot even be locked is left as it was */
  DDSURFACEDESC2 sd;GLuint y,bytes=fb->cb==16?2:4;
  memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
  if(IDirectDrawSurface7_Lock(s,NULL,&sd,DDLOCK_WAIT|DDLOCK_NOSYSLOCK|DDLOCK_WRITEONLY,NULL)==DD_OK){
   if(sd.lpSurface&&sd.lPitch>0)for(y=0;y<sd.dwHeight;y++)memset((GLubyte*)sd.lpSurface+y*(DWORD)sd.lPitch,0,sd.dwWidth*bytes);
   IDirectDrawSurface7_Unlock(s,NULL);if(sd.lpSurface&&sd.lPitch>0)return 1;}
  {static int logged;if(logged++<4)vxgl_log("surface %ux%u: not cleared either %08lX\n",fb->w,fb->h,(unsigned long)r);return 0;}}
 return 1;
}
static int fb_make_surfaces(vxgl_fb *fb,GLuint w,GLuint h) {
 DDSURFACEDESC2 sd;int db,zb,sb,cb;
 fb_wait(fb);fb_free_surfaces(fb);
 if(!w)w=1;if(!h)h=1;
 if(!dd_init())return 0;
 vxgl_pf_info(fb->pf,&db,&zb,&sb,&cb);cb=fb->cb;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;sd.dwWidth=w;sd.dwHeight=h;
 sd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY|DDSCAPS_3DDEVICE;
 sd.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);sd.ddpfPixelFormat.dwFlags=DDPF_RGB;
 if(cb==16){sd.ddpfPixelFormat.dwRGBBitCount=16;sd.ddpfPixelFormat.dwRBitMask=0xF800;sd.ddpfPixelFormat.dwGBitMask=0x07E0;sd.ddpfPixelFormat.dwBBitMask=0x001F;}
 else {sd.ddpfPixelFormat.dwRGBBitCount=32;sd.ddpfPixelFormat.dwRBitMask=0xFF0000;sd.ddpfPixelFormat.dwGBitMask=0xFF00;sd.ddpfPixelFormat.dwBBitMask=0xFF;}
 if(IDirectDraw7_CreateSurface(vxgl_dd,&sd,&fb->back,0)!=DD_OK){vxgl_log("back buffer %ux%u failed\n",w,h);fb->back=0;return 0;}
 fb->sback.format=cb==16?VX3DN_RGB565:VX3DN_XRGB8888;
 if(!surface_where(fb->back,&fb->sback,&fb->pback)){vxgl_log("back buffer not in usable video memory\n");fb->sback.format=0;
  if(!fb->sback.pitch){dd_release_surface(&fb->back);return 0;}} /* not even lockable */
 /* v10.366: the window's pixels (single-buffered) or black, not old video memory (window_rects sizes by fb->w, fb->h); .573 (peer review): a buffer that could be neither filled nor locked is let go (it was used with its old video memory); the next validate makes it again */
 fb->w=w;fb->h=h;
 if(!fb_seed(fb,fb->back,!db)){fb_free_surfaces(fb);return 0;}
 fb->w=fb->h=0;fb->dmg_on[0]=fb->dmg_on[1]=0;
 if(zb) {
  GLuint zbytes=zb==16?2:4;
  memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;sd.dwWidth=w;sd.dwHeight=h;
  sd.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|DDSCAPS_VIDEOMEMORY;
  sd.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);
  if(zb==16){sd.ddpfPixelFormat.dwFlags=DDPF_ZBUFFER;sd.ddpfPixelFormat.dwZBufferBitDepth=16;sd.ddpfPixelFormat.dwZBitMask=0xFFFF;}
  else {sd.ddpfPixelFormat.dwFlags=DDPF_ZBUFFER|DDPF_STENCILBUFFER;sd.ddpfPixelFormat.dwZBufferBitDepth=32;sd.ddpfPixelFormat.dwStencilBitDepth=8;
   sd.ddpfPixelFormat.dwZBitMask=0xFFFFFF00UL;sd.ddpfPixelFormat.dwStencilBitMask=0xFF;}
  if(IDirectDraw7_CreateSurface(vxgl_dd,&sd,&fb->depth,0)!=DD_OK){vxgl_log("depth buffer %ux%u (%d) failed\n",w,h,zb);fb->depth=0;}
  else {fb->sdepth.format=zb==16?VX3DN_D16:VX3DN_D24S8;if(!surface_where(fb->depth,&fb->sdepth,&fb->pdepth)){vxgl_log("depth not in usable video memory\n");fb->sdepth.format=0;dd_release_surface(&fb->depth);}
   /* v10.245: the stencil plane (the engine's place for D24S8 stencil); swrast reads and writes it there too */
   else if(zb!=16&&(!dd_splane||!dd_splane(fb->depth,&fb->soff,&fb->spitch)||fb->soff<fb->sdepth.offset)){
    vxgl_log("depth buffer: the HAL has no stencil plane for it (VXDDRAW.DLL older than v10.245?)\n");fb->sdepth.format=0;fb->soff=fb->spitch=0;dd_release_surface(&fb->depth);}
   else if(zb!=16)vxgl_log("stencil plane %08lX pitch %lu\n",(unsigned long)fb->soff,(unsigned long)fb->spitch);}
  /* no depth surface the engine can use: the depth buffer is in system memory and swrast draws everything (vxgl_mesa.c core_targets);
   * Mesa has the depth renderbuffer either way */
  if(!fb->depth){memset(&fb->sdepth,0,sizeof(fb->sdepth));fb->sdepth.pitch=w*zbytes;fb->pdepth=fb_mem(&fb->zmem,&fb->zmem_size,w*zbytes*h);if(!fb->pdepth){fb_free_surfaces(fb);return 0;}
   fb->soff=0;fb->spitch=zb==16?0:VX3DN_SPLANE_PITCH(w);}
  if(fb->spitch&&!fb_mem(&fb->smem,&fb->smem_size,VX3DN_SPLANE_SIZE(w,h))){fb_free_surfaces(fb);return 0;} /* the plane's stand-in when a lock fails */
 }
 fb->w=w;fb->h=h;fb->made++; /* v10.288 */
 vxgl_log("buffers %ux%u: back %08lX pitch %lu, depth %08lX pitch %lu format %lu\n",w,h,(unsigned long)fb->sback.offset,(unsigned long)fb->sback.pitch,
  (unsigned long)fb->sdepth.offset,(unsigned long)fb->sdepth.pitch,(unsigned long)fb->sdepth.format);
 return fb->back!=0;
}
/* the window's client size; the surfaces are made again when it changed or DirectDraw lost them (a mode change) */
int vxgl_fb_validate(vxgl_ctx *c,vxgl_fb *fb) {
 RECT r;GLuint w,h;
 vxgl_fb_settle(fb);
 if(!GetClientRect(fb->hwnd,&r))return 0;
 w=r.right-r.left;h=r.bottom-r.top;if(!w)w=1;if(!h)h=1;
 if(fb->back&&IDirectDrawSurface7_IsLost(fb->back)!=DD_OK)fb->w=0;
 if(fb->depth&&IDirectDrawSurface7_IsLost(fb->depth)!=DD_OK)fb->w=0;
 if(fb->front&&IDirectDrawSurface7_IsLost(fb->front)!=DD_OK){dd_release_surface(&fb->front);memset(&fb->sfront,0,sizeof(fb->sfront));fb->fstate=0;} /* v10.249: made again when next used */
 if(vxgl_dd)primary_ok();
 if(w==fb->w&&h==fb->h&&fb->back)return 1;
 if(c)vxgl_hw_flush(c);
 return fb_make_surfaces(fb,w,h);
}
/* v10.268: a texture's copy for the engine: a DirectDraw texture surface in video memory (a mip chain for more than one level), as the
 * HAL makes Direct3D's; each level's offset from the HAL (VX_DD_Surface), which must be 256-byte aligned with the sampler's pitch */
/* v10.288 (review): a copy that could not be made gives back what it took without counting it as memory given back (vxgl_texmem_freed):
 * two textures that both failed so tried each other again at every state change */
/* v10.307: a level's pitch as the HAL reports it (a DXT surface: its linear size, packed block rows) and as the sampler reads it (rows of
 * at least 32 bytes; the HAL spreads narrow DXT block rows to 32 on Unlock, v10.200) */
static int texmem_dxt(uint32_t f) {return f>=VX3DN_TEX_DXT1&&f<=VX3DN_TEX_DXT5;}
static DWORD texmem_packed(uint32_t f,DWORD w) {return texmem_dxt(f)?(w<4?1:w/4)*(f==VX3DN_TEX_DXT1?8UL:16UL):w*(f==VX3DN_TEX_RGB565?2UL:4UL);} /* v10.366: R5G6B5 (ACCEL_OPENGL_TEX16) */
static DWORD texmem_rows(uint32_t f,DWORD h) {return texmem_dxt(f)?(h<4?1:h/4):h;}
static int texmem_pitch(const vxgl_texmem *m,GLuint l,DWORD hal,DWORD *sampler) {
 DWORD lw=m->w>>l,lh=m->h>>l,p;if(!lw)lw=1;if(!lh)lh=1;p=texmem_packed(m->format,lw);*sampler=p<32?32:p;
 return texmem_dxt(m->format)?hal==p*texmem_rows(m->format,lh):hal==*sampler;
}
static void alloc_undo(vxgl_texmem *m) {unsigned long f=vxgl_texmem_freed;vxgl_texmem_free(m);vxgl_texmem_freed=f;}
/* v10.317: a block copy (vxgl_texmem_layout: a cube map's face with its mip chain, a 3D texture's slices): one offscreen surface of the
 * desktop's format holding m->block bytes (rows of P = 256 x k bytes, at most 2048 of them), its levels at their offsets from its start */
static DWORD block_pitch(DWORD bytes) {DWORD k=(bytes+256UL*2048UL-1)/(256UL*2048UL);return 256UL*(k?k:1);}
static int block_where(vxgl_texmem *m) {
 DWORD off=0,pitch=0;GLuint l;
 if(!dd_surface((LPDIRECTDRAWSURFACE7)m->surf[0],&off,&pitch)||(off&255)||pitch!=block_pitch(m->block))return 0;
 for(l=0;l<m->levels;l++)m->off[l]=off+m->rel[l];
 return 1;
}
static int block_alloc(vxgl_texmem *m) {
 DDSURFACEDESC2 sd;HRESULT r;DWORD P=block_pitch(m->block),bpp=desktop_bpp==16?2:4;static int logged;
 if(desktop_bpp!=16&&desktop_bpp!=32)return 0;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;
 sd.dwWidth=P/bpp;sd.dwHeight=(m->block+P-1)/P;sd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 if((r=IDirectDraw7_CreateSurface(vxgl_dd,&sd,(LPDIRECTDRAWSURFACE7*)&m->surf[0],0))!=DD_OK){m->surf[0]=0;if(logged++<8)vxgl_log("texture block %lu bytes: CreateSurface %08lX\n",(unsigned long)m->block,(unsigned long)r);return 0;}
 if(!block_where(m)){if(logged++<8)vxgl_log("texture block %lu bytes: not where the engine can read it\n",(unsigned long)m->block);alloc_undo(m);return 0;}
 {static int n2;if(n2++<16)vxgl_log("texture %ux%u, %u levels, depth %u (block %lu bytes): video memory %08lX\n",m->w,m->h,m->levels,m->depth,(unsigned long)m->block,(unsigned long)m->off[0]);}
 return 1;
}
int vxgl_texmem_alloc(vxgl_texmem *m) {
 DDSURFACEDESC2 sd;LPDIRECTDRAWSURFACE7 n;DDSCAPS2 caps;GLuint l;DWORD off,pitch;HRESULT r;static int logged;
 if(!m->levels||m->levels>VXGL_TEXLEVELS||!dd_init()||!dd_surface)return 0;
 if(m->block)return block_alloc(m); /* v10.317 */
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT|(m->levels>1?DDSD_MIPMAPCOUNT:0);
 sd.dwWidth=m->w;sd.dwHeight=m->h;sd.dwMipMapCount=m->levels;
 sd.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY|(m->levels>1?DDSCAPS_MIPMAP|DDSCAPS_COMPLEX:0);
 sd.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);sd.ddpfPixelFormat.dwFlags=DDPF_RGB|(m->format==VX3DN_TEX_ARGB8888?DDPF_ALPHAPIXELS:0);
 sd.ddpfPixelFormat.dwRGBBitCount=32;sd.ddpfPixelFormat.dwRBitMask=0xFF0000;sd.ddpfPixelFormat.dwGBitMask=0xFF00;sd.ddpfPixelFormat.dwBBitMask=0xFF;
 sd.ddpfPixelFormat.dwRGBAlphaBitMask=m->format==VX3DN_TEX_ARGB8888?0xFF000000UL:0;
 if(m->format==VX3DN_TEX_RGB565){sd.ddpfPixelFormat.dwRGBBitCount=16;sd.ddpfPixelFormat.dwRBitMask=0xF800;sd.ddpfPixelFormat.dwGBitMask=0x7E0;sd.ddpfPixelFormat.dwBBitMask=0x1F;} /* v10.366: the HAL's R5G6B5 texture (vx_d3d_hal.c VX_D3D_format) */
 if(texmem_dxt(m->format)){memset(&sd.ddpfPixelFormat,0,sizeof(sd.ddpfPixelFormat));sd.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);sd.ddpfPixelFormat.dwFlags=DDPF_FOURCC;
  sd.ddpfPixelFormat.dwFourCC=m->format==VX3DN_TEX_DXT1?0x31545844UL:m->format==VX3DN_TEX_DXT3?0x33545844UL:0x35545844UL;} /* v10.307: DXT1 / DXT3 / DXT5 */
 if((r=IDirectDraw7_CreateSurface(vxgl_dd,&sd,(LPDIRECTDRAWSURFACE7*)&m->surf[0],0))!=DD_OK){m->surf[0]=0;if(logged++<8)vxgl_log("texture %ux%u, %u levels: CreateSurface %08lX\n",m->w,m->h,m->levels,(unsigned long)r);return 0;}
 for(l=1;l<m->levels;l++){memset(&caps,0,sizeof(caps));caps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_MIPMAP;
  if(IDirectDrawSurface7_GetAttachedSurface((LPDIRECTDRAWSURFACE7)m->surf[l-1],&caps,&n)!=DD_OK){if(logged++<8)vxgl_log("texture %ux%u: level %u not in the chain\n",m->w,m->h,l);alloc_undo(m);return 0;}
  m->surf[l]=n;}
 for(l=0;l<m->levels;l++){DWORD sp;
  if(!dd_surface((LPDIRECTDRAWSURFACE7)m->surf[l],&off,&pitch)||(off&255)||!texmem_pitch(m,l,pitch,&sp)){
   if(logged++<8)vxgl_log("texture %ux%u level %u: offset %08lX pitch %lu, not the sampler's\n",m->w,m->h,l,(unsigned long)off,(unsigned long)pitch);alloc_undo(m);return 0;}
  m->off[l]=off;m->pitch[l]=sp;}
 {static int n2;if(n2++<16)vxgl_log("texture %ux%u, %u levels, format %u: video memory %08lX\n",m->w,m->h,m->levels,m->format,(unsigned long)m->off[0]);}
 return 1;
}
void vxgl_texmem_free(vxgl_texmem *m) {
 GLuint l;
 if(m->surf[0])vxgl_texmem_freed++; /* v10.287: a failed copy is tried again */
 for(l=m->levels;l-->1;)if(m->surf[l])IDirectDrawSurface7_Release((LPDIRECTDRAWSURFACE7)m->surf[l]); /* GetAttachedSurface's references */
 if(m->surf[0])IDirectDrawSurface7_Release((LPDIRECTDRAWSURFACE7)m->surf[0]);
 memset(m,0,sizeof(*m));
}
GLubyte *vxgl_texmem_lock(vxgl_texmem *m,GLuint l,uint32_t *pitch) {
 DDSURFACEDESC2 sd;HRESULT r;void *s;
 if(l>=m->levels||!(s=m->block?m->surf[0]:m->surf[l]))return 0;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 if((r=IDirectDrawSurface7_Lock((LPDIRECTDRAWSURFACE7)s,NULL,&sd,DDLOCK_WAIT|DDLOCK_NOSYSLOCK|DDLOCK_WRITEONLY,NULL))!=DD_OK){
  static int logged;if(logged++<8)vxgl_log("texture level %u: lock %08lX\n",l,(unsigned long)r);return 0;}
 vxgl_seq_idle(); /* v10.366: the HAL's Lock waited for the engine (vx_ddraw.c VXLock) */
 if(m->block){*pitch=m->pitch[l];return (GLubyte*)sd.lpSurface+m->rel[l];} /* v10.317: the level in the block, the sampler's pitch */
 *pitch=(uint32_t)sd.lPitch;return (GLubyte*)sd.lpSurface;
}
/* v10.321: the same lock for reading too (a copy drawn into by the engine read back, a framebuffer object's renderbuffer read and written by swrast) */
GLubyte *vxgl_texmem_map(vxgl_texmem *m,GLuint l,uint32_t *pitch) {
 DDSURFACEDESC2 sd;HRESULT r;void *s;
 if(l>=m->levels||!(s=m->block?m->surf[0]:m->surf[l]))return 0;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 if((r=IDirectDrawSurface7_Lock((LPDIRECTDRAWSURFACE7)s,NULL,&sd,DDLOCK_WAIT|DDLOCK_NOSYSLOCK,NULL))!=DD_OK){
  static int logged;if(logged++<8)vxgl_log("video memory level %u: lock %08lX\n",l,(unsigned long)r);return 0;}
 if(m->block){*pitch=m->pitch[l];return (GLubyte*)sd.lpSurface+m->rel[l];}
 *pitch=(uint32_t)sd.lPitch;return (GLubyte*)sd.lpSurface;
}
/* v10.366: a level's memory without DirectDraw's Lock, which waits for the engine to finish everything (VXLock): the video memory's linear
 * address (VX_DD_Vram, the address the HAL's Lock returns from) at the level's offset, rows of the sampler's pitch (the HAL's for these
 * formats). For a copy no packet still in the engine or the queue reads (vxgl_tex.c's ring, fenced by vxgl_seq_done). Not a block, DXT
 * (the HAL packs and spreads its rows at Lock and Unlock) or lost copy: 0, the caller locks it */
GLubyte *vxgl_texmem_direct(vxgl_texmem *m,GLuint l,uint32_t *pitch) {
 uint32_t lh;
 if(!vxgl_vram_linear||m->block||texmem_dxt(m->format)||l>=m->levels||!m->surf[l]||!m->pitch[l])return 0;
 if(IDirectDrawSurface7_IsLost((LPDIRECTDRAWSURFACE7)m->surf[0])!=DD_OK)return 0;
 lh=m->h>>l;if(!lh)lh=1;
 if(m->off[l]>=vxgl_vram_size||lh>(vxgl_vram_size-m->off[l])/m->pitch[l])return 0;
 *pitch=m->pitch[l];return (GLubyte*)vxgl_vram_linear+m->off[l];
}
void vxgl_texmem_unlock(vxgl_texmem *m,GLuint l) {void *s=m->block?m->surf[0]:m->surf[l];if(l<m->levels&&s)IDirectDrawSurface7_Unlock((LPDIRECTDRAWSURFACE7)s,NULL);}
int vxgl_texmem_lost(vxgl_texmem *m) {
 LPDIRECTDRAWSURFACE7 s=(LPDIRECTDRAWSURFACE7)m->surf[0];
 return s&&IDirectDrawSurface7_IsLost(s)!=DD_OK;
}
/* v10.287 (review): Restore gives the chain memory again, not necessarily where it was (v10.268 kept the old offsets, which the engine
 * then sampled: other surfaces' memory after a mode change). Each level is checked restored and its offset and pitch read from the HAL
 * again, as vxgl_texmem_alloc takes them; anything wrong and the copy is freed (the caller makes a new one). The contents are not back:
 * the caller uploads again. */
int vxgl_texmem_restore(vxgl_texmem *m) {
 LPDIRECTDRAWSURFACE7 s=(LPDIRECTDRAWSURFACE7)m->surf[0];GLuint l;DWORD off=0,pitch=0;HRESULT r;static int logged;
 if(!s)return 0;
 if((r=IDirectDrawSurface7_Restore(s))!=DD_OK){if(logged++<8)vxgl_log("texture %ux%u: restore %08lX, made again\n",m->w,m->h,(unsigned long)r);vxgl_texmem_free(m);return 0;}
 if(m->block){if(!block_where(m)){if(logged++<8)vxgl_log("texture block restored: not usable, made again\n");vxgl_texmem_free(m);return 0;}return 1;} /* v10.317 */
 for(l=0;l<m->levels;l++){DWORD sp=0;
  s=(LPDIRECTDRAWSURFACE7)m->surf[l];
  if(!s||(IDirectDrawSurface7_IsLost(s)!=DD_OK&&IDirectDrawSurface7_Restore(s)!=DD_OK)||!dd_surface(s,&off,&pitch)||(off&255)||!texmem_pitch(m,l,pitch,&sp)){
   if(logged++<8)vxgl_log("texture %ux%u level %u restored: offset %08lX pitch %lu, made again\n",m->w,m->h,l,(unsigned long)off,(unsigned long)pitch);
   vxgl_texmem_free(m);return 0;}
  m->off[l]=off;m->pitch[l]=sp;}
 if(logged++<8)vxgl_log("texture %ux%u, %u levels restored: video memory %08lX\n",m->w,m->h,m->levels,(unsigned long)m->off[0]);
 return 1;
}
/* swrast's access. The lock waits for the engine (the HAL's Lock retires 3D work). DDLOCK_NOSYSLOCK: the Win16 lock is not held while
 * swrast draws (other programs' GDI would stop for that long). A surface that cannot be locked (lost: a mode change, or the engine
 * reset) is not written: swrast gets system memory instead and the window's size is forgotten, so the next validate makes the
 * surfaces again. */
static void fb_unlock_now(vxgl_fb *fb);
DWORD vxgl_lock_keep=1; /* v10.267: ACCEL_OPENGL_LOCKKEEP (default 1) */
int vxgl_fb_lock(vxgl_fb *fb) {
 DDSURFACEDESC2 sd;int ok=1;
 if(fb->locked)return fb->locked++,1;
 if(vxgl_batch_flush()&&fb->hold)fb_unlock_now(fb); /* v10.275: the queued packets drawn first; a kept lock taken before them would not wait for them */
 fb->locked++;
 if(fb->hold) { /* v10.267: the locks of swrast's last batch are still held: nothing else touched the surfaces since */
  fb->hold=0;
  /* v10.330 (review #47): every surface the kept lock holds checked (the back alone was) */
  if(!((fb->held&1)&&IDirectDrawSurface7_IsLost(fb->back)!=DD_OK)&&!((fb->held&2)&&fb->depth&&IDirectDrawSurface7_IsLost(fb->depth)!=DD_OK)&&
     !((fb->held&4)&&fb->front&&IDirectDrawSurface7_IsLost(fb->front)!=DD_OK))return fb->hold_ok;
  fb_unlock_now(fb); /* lost in the meantime (a mode change): lock again, which finds it */
 }
 fb->held=0;
 fb_damage_all(fb); /* v10.366: swrast may write anywhere */
 if(fb->back){memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
  if(IDirectDrawSurface7_Lock(fb->back,NULL,&sd,DDLOCK_WAIT|DDLOCK_NOSYSLOCK,NULL)==DD_OK){fb->pback=(GLubyte*)sd.lpSurface;fb->held|=1;}else ok=0;}
 if(!(fb->held&1))fb->pback=fb_mem(&fb->cmem,&fb->cmem_size,fb->sback.pitch*fb->h);
 if(fb->depth){memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
  if(IDirectDrawSurface7_Lock(fb->depth,NULL,&sd,DDLOCK_WAIT|DDLOCK_NOSYSLOCK,NULL)==DD_OK){fb->pdepth=(GLubyte*)sd.lpSurface;fb->held|=2;}else ok=0;}
 if(!(fb->held&2))fb->pdepth=fb_mem(&fb->zmem,&fb->zmem_size,fb->sdepth.pitch*fb->h);
 fb->pfront=0; /* v10.249: the front surface when there is one; v10.261: else the system-memory front (never the back's memory) */
 if(fb->front){memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
  if(IDirectDrawSurface7_Lock(fb->front,NULL,&sd,DDLOCK_WAIT|DDLOCK_NOSYSLOCK,NULL)==DD_OK&&sd.lPitch==(LONG)fb->sback.pitch){fb->pfront=(GLubyte*)sd.lpSurface;fb->held|=4;}
  else {static int logged;if(sd.lpSurface)IDirectDrawSurface7_Unlock(fb->front,NULL);
   if(logged++<4)vxgl_log("front buffer surface not locked (pitch %ld): system memory instead\n",(long)sd.lPitch);
   dd_release_surface(&fb->front);memset(&fb->sfront,0,sizeof(fb->sfront));fb->fsoft=1;if(soft_front(fb))soft_front_fill(fb);}}
 if(!fb->pfront&&fb->fsoft)fb->pfront=soft_front(fb);
 /* v10.245: the plane is in the same block as the Z words, so it is at the same distance from them in the lock's mapping */
 fb->pstencil=!fb->spitch?0:(fb->held&2)&&fb->soff?fb->pdepth+(fb->soff-fb->sdepth.offset):fb_mem(&fb->smem,&fb->smem_size,VX3DN_SPLANE_SIZE(fb->w,fb->h));
 /* v10.342 (review): no memory for a stand-in either: the width 0 makes every span (colour, depth, stencil, vxgl_mesa.c HW_CLIPLOOP) write
  * and read nothing, so swrast never reaches a missing buffer; the surfaces are made again at the next validate */
 if(!ok||!fb->pback||!fb->pdepth||(fb->spitch&&!fb->pstencil)||(fb->fsoft&&!fb->pfront)){static int logged;if(logged++<8)vxgl_log("lock failed (surfaces lost): swrast writes nothing until they are made again\n");fb->w=0;}
 fb->hold_ok=ok;
 return ok;
}
/* v10.267: swrast's batches each locked and unlocked the back and depth surfaces (and the HAL's Lock waits for the engine): four
 * DirectDraw calls and two VxD calls around every glBegin/glEnd that went to software. VXTEX on the board: 55 microseconds a bind and
 * a tiny triangle, 77 a small textured quad (Microsoft's software OpenGL 7 and 15). Now the locks are kept when a batch ends and
 * reused by the next one, and let go (vxgl_fb_settle) before anything else uses the surfaces: an engine packet (vxgl_submit), a blit
 * to or from them (the swap, the front buffer), a context switch, glFinish, and the surfaces being made again or freed. */
static void fb_unlock_now(vxgl_fb *fb) {
 if(fb->held&1)IDirectDrawSurface7_Unlock(fb->back,NULL);
 if(fb->held&2)IDirectDrawSurface7_Unlock(fb->depth,NULL);
 if(fb->held&4)IDirectDrawSurface7_Unlock(fb->front,NULL);
 fb->held=0;fb->hold=0;
}
void vxgl_fb_unlock(vxgl_fb *fb) {
 if(!fb->locked||--fb->locked)return;
 if(vxgl_lock_keep){fb->hold=1;return;}
 fb_unlock_now(fb);
}
static void fb_settle_only(vxgl_fb *fb) {if(fb&&!fb->locked&&fb->hold)fb_unlock_now(fb);}
void vxgl_fb_settle(vxgl_fb *fb) {vxgl_batch_flush();fb_settle_only(fb);} /* v10.275: and the queued packets sent (everything that settles is about to use the surfaces) */
/* Windows 98 gives a new window the handle of a destroyed one, so a window is known by its handle and a property set on it at
 * SetPixelFormat (a window's properties go with it) */
static const char fb_prop[]="VXGL.fb";
static int fb_alive(vxgl_fb *f) {return IsWindow(f->hwnd)&&GetPropA(f->hwnd,fb_prop)==(HANDLE)f;}
static vxgl_fb *fb_find(HWND w) {vxgl_fb *f;for(f=fbs;f;f=f->next)if(f->hwnd==w&&fb_alive(f))return f;return 0;}
static void fb_forget_dead(void) {
 vxgl_fb **pp=&fbs,*f;
 while((f=*pp)!=0){if(!fb_alive(f)){*pp=f->next;vxgl_fb_release(f);}else pp=&f->next;}
}
void vxgl_fb_destroy(vxgl_fb *f) {fb_wait(f);fb_free_surfaces(f);fb_free_mem(f);_mesa_free(f);}

/* ---------------------------------------------------------------- pixel formats */
/* index: 1 double z16, 2 double z24 s8, 3 double no z, 4 single z16, 5 single z24 s8 */
static const struct {int db,zb,sb;} pf_tab[5]={{1,16,0},{1,24,8},{1,0,0},{0,16,0},{0,24,8}};
#define PF_COUNT 5
static int pf_count(void) {return (desktop_bpp==16||desktop_bpp==32)?PF_COUNT:0;}
int vxgl_pf_info(int pf,int *db,int *zbits,int *sbits,int *cbits) {
 if(pf<1||pf>PF_COUNT)return 0;
 *db=pf_tab[pf-1].db;*zbits=pf_tab[pf-1].zb;*sbits=pf_tab[pf-1].sb;*cbits=desktop_bpp;return 1;
}
static void pf_describe(int pf,PIXELFORMATDESCRIPTOR *d) {
 int db,zb,sb,cb;vxgl_pf_info(pf,&db,&zb,&sb,&cb);
 memset(d,0,sizeof(*d));d->nSize=sizeof(*d);d->nVersion=1;
 d->dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|(db?PFD_DOUBLEBUFFER|PFD_SWAP_COPY:0);
 d->iPixelType=PFD_TYPE_RGBA;
 if(cb==16){d->cColorBits=16;d->cRedBits=5;d->cRedShift=11;d->cGreenBits=6;d->cGreenShift=5;d->cBlueBits=5;d->cBlueShift=0;}
 else {d->cColorBits=32;d->cRedBits=8;d->cRedShift=16;d->cGreenBits=8;d->cGreenShift=8;d->cBlueBits=8;d->cBlueShift=0;}
 d->cAccumBits=64;d->cAccumRedBits=16;d->cAccumGreenBits=16;d->cAccumBlueBits=16;d->cAccumAlphaBits=16; /* swrast's accumulation buffer */
 d->cDepthBits=(BYTE)zb;d->cStencilBits=(BYTE)sb;d->iLayerType=PFD_MAIN_PLANE;
}
static int window_pf(HWND w) {vxgl_fb *f=fb_find(w);return f?f->pf:0;}

/* ---------------------------------------------------------------- the dispatch table opengl32 calls */
typedef struct {DWORD size;PROC table[336];} ICDTABLE;
static ICDTABLE icd_table={336,{
#define ICD_ENTRY(func) (PROC)gl##func,
#include "icdlist.h"
#undef ICD_ENTRY
}};

/* ---------------------------------------------------------------- contexts */
#define MAX_CTX 32
static vxgl_ctx *ctx_tab[MAX_CTX];
static int ctx_slot(HGLRC h) {int i=(int)(DWORD)h-1;return i>=0&&i<MAX_CTX&&ctx_tab[i]?i:-1;}

BOOL APIENTRY DrvValidateVersion(DWORD v) {
 vx3dn_info i;(void)v;
 if(vxgl_gone)return FALSE;
 if(!icd_enabled){vxgl_log("DrvValidateVersion: ACCEL_OPENGL = 0\n");return FALSE;}
 if(!vxd_info(&i)){vxgl_log("DrvValidateVersion: the VxD is not ready\n");return FALSE;}
 /* v10.458 (board: VXGL.LOG showed VXGL 3.73 of v10.422 loaded from C:\VXGL.DLL in a screen saver's preview, every packet answered LOST
  * by the newer VxD): this ICD and the VxD must be one build; another build's ICD found first on the search path (the registry held
  * the bare name VXGL.DLL, searched in the program's and the current directory before the system directory) says no, and opengl32
  * draws with Microsoft's own renderer instead */
 {uint32_t o[8];if(!texdbg(0x75,o)||o[0]!=0x75||o[1]!=(uint32_t)DRV_VER_BUILD){char m[MAX_PATH];m[0]=0;GetModuleFileNameA(vxgl_inst,m,sizeof(m));
   vxgl_log("DrvValidateVersion: %s is driver v10.%d, the VxD is v10.%lu: not this ICD's VxD, refused\n",m,DRV_VER_BUILD,o[0]==0x75?(unsigned long)o[1]:0UL);return FALSE;}}
 /* .588 (VX800 board on DirectX 6.1: OpenGL programs could not start): this ICD keeps its buffers and textures as DirectDraw 7 surfaces.
  * On a DirectDraw without DirectDrawCreateEx it says no here, as for a wrong VxD, and opengl32 draws with Microsoft's own renderer: the
  * program runs, in software, until DirectX 7 or later is installed. Only the export is looked for; nothing is created at this point. */
 {HMODULE m=LoadLibraryA("DDRAW.DLL");int ok=m&&GetProcAddress(m,"DirectDrawCreateEx");if(m)FreeLibrary(m);
  if(!ok){vxgl_log("DrvValidateVersion: DDRAW.DLL has no DirectDrawCreateEx (DirectX older than 7): refused, opengl32 draws in software\n");return FALSE;}}
 return TRUE;
}
int APIENTRY DrvDescribePixelFormat(HDC hdc,int pf,UINT n,PIXELFORMATDESCRIPTOR *d) {
 if(vxgl_gone)return 0;
 (void)hdc;vxgl_screen_bpp();
 if(!d)return pf_count();
 if(pf<1||pf>pf_count()||n<sizeof(PIXELFORMATDESCRIPTOR)){SetLastError(ERROR_INVALID_PARAMETER);return 0;}
 pf_describe(pf,d);return pf_count();
}
/* lock order: the engine lock, then the list lock (MakeCurrent and the swap hold the engine and look windows up; creating and deleting a
 * context hold the list and flush the engine) */
static void big_lock(void) {vxgl_engine_lock();EnterCriticalSection(&lock);}
static void big_unlock(void) {LeaveCriticalSection(&lock);vxgl_engine_unlock();}
BOOL APIENTRY DrvSetPixelFormat(HDC hdc,int pf) {
 HWND w;vxgl_fb *f;
 if(vxgl_gone)return FALSE;
 w=WindowFromDC(hdc);
 vxgl_screen_bpp();
 if(!w||pf<1||pf>pf_count())return FALSE;
 big_lock();fb_forget_dead();
 f=fb_find(w);
 if(!f) {
  f=(vxgl_fb*)_mesa_calloc(sizeof(vxgl_fb));if(!f){big_unlock();return FALSE;}
  if(!SetPropA(w,fb_prop,(HANDLE)f)){_mesa_free(f);big_unlock();return FALSE;}
  f->hwnd=w;f->cb=desktop_bpp;f->next=fbs;fbs=f;
 }
 if(f->pf&&f->pf!=pf){big_unlock();return FALSE;} /* once per window, as GDI */
 f->pf=pf;big_unlock();
 vxgl_log("SetPixelFormat %d on %p\n",pf,(void*)w);
 return TRUE;
}
HGLRC APIENTRY DrvCreateContext(HDC hdc) {
 HWND w;int pf,i;vxgl_ctx *c;
 if(vxgl_gone)return 0;
 big_lock(); /* v10.330 (review #20): before the window list is read (another thread's call can free a dead window's entry under it) */
 w=WindowFromDC(hdc);pf=w?window_pf(w):0;
 if(!pf){big_unlock();vxgl_log("CreateContext: no pixel format set\n");SetLastError(ERROR_INVALID_PIXEL_FORMAT);return 0;}
 for(i=0;i<MAX_CTX&&ctx_tab[i];i++);
 if(i==MAX_CTX){big_unlock();return 0;}
 vxgl_hw_enabled=(1)!=0; /* per context (v10.241): VXGL.EXE draws with the engine off in the same process */
 {char e[4];if(GetEnvironmentVariableA("VXGL_HW",e,sizeof(e)))vxgl_hw_enabled=e[0]!='0';} /* v10.310: the test programs' engine-off run (process-local: nothing left behind when a run stops) */
 c=vxgl_create_context(pf,hdc,0);
 if(c)prof_clear(); /* v10.270 */
 if(c&&log_on)smp_start(); /* v10.349 */
 if(c)ctx_tab[i]=c;
 big_unlock();
 vxgl_log("CreateContext %d -> %p\n",i+1,(void*)c);
 return c?(HGLRC)(DWORD)(i+1):0;
}
HGLRC APIENTRY DrvCreateLayerContext(HDC hdc,int layer) {if(layer){SetLastError(ERROR_INVALID_PARAMETER);return 0;}return DrvCreateContext(hdc);}
BOOL APIENTRY DrvDeleteContext(HGLRC h) {
 int i;
 if(vxgl_gone)return TRUE; /* v10.260: process exit: the memory goes with the process */
 big_lock();i=ctx_slot(h);
 vxgl_log("DeleteContext %lu -> %p\n",(unsigned long)(DWORD)h,i<0?(void*)0:(void*)ctx_tab[i]);
 if(i<0){big_unlock();return FALSE;}
 /* v10.259: a context current in another thread is not deleted (wglDeleteContext documents it as an error): that thread would go on with freed memory */
 if(ctx_tab[i]->owner&&ctx_tab[i]->owner!=GetCurrentThreadId()){vxgl_log("DeleteContext: %p is current in thread %lX: refused\n",(void*)ctx_tab[i],(unsigned long)ctx_tab[i]->owner);big_unlock();SetLastError(ERROR_BUSY);return FALSE;}
 {int freed;prof_report();smp_end(); /* v10.270; v10.349 */
  freed=vxgl_destroy_context(ctx_tab[i]);
  ctx_tab[i]=0; /* the handle is retired either way: a broken context (v10.261: detached, quarantined, not freed) is never handed out again */
  fb_forget_dead();big_unlock(); /* a window destroyed before its context: freed now */
  if(!freed){SetLastError(ERROR_INVALID_HANDLE);return FALSE;}
  return TRUE;}
}
/* v10.259: the slot table under the lock in every entry that reads it (a context deleted by another thread meanwhile was used after it was freed) */
BOOL APIENTRY DrvCopyContext(HGLRC a,HGLRC b,UINT mask) {
 int i,j;BOOL r=FALSE;if(vxgl_gone)return FALSE;big_lock();i=ctx_slot(a);j=ctx_slot(b);if(i>=0&&j>=0){_mesa_copy_context(&ctx_tab[i]->gl,&ctx_tab[j]->gl,mask);r=TRUE;}big_unlock();return r;
}
BOOL APIENTRY DrvShareLists(HGLRC a,HGLRC b) {
 int i,j;BOOL r=FALSE;if(vxgl_gone)return FALSE;big_lock();i=ctx_slot(a);j=ctx_slot(b);if(i>=0&&j>=0)r=_mesa_share_state(&ctx_tab[j]->gl,&ctx_tab[i]->gl)?TRUE:FALSE;big_unlock();return r;
}
void *APIENTRY DrvSetContext(HDC hdc,HGLRC h,void *callback) {
 int i,ok;(void)callback;
 if(vxgl_gone)return 0;
 if(!hdc||!h){vxgl_make_current(0,0);return 0;}
 big_lock();i=ctx_slot(h);ok=i>=0&&vxgl_make_current(ctx_tab[i],hdc);big_unlock();
 return ok?&icd_table:0;
}
BOOL APIENTRY DrvReleaseContext(HGLRC h) {(void)h;if(!vxgl_gone)vxgl_make_current(0,0);return TRUE;}
BOOL APIENTRY DrvSwapBuffers(HDC hdc) {
 GET_CURRENT_CONTEXT(gl);BOOL r;
 if(vxgl_gone)return FALSE;
 vxgl_engine_lock();
 if(gl&&VXGL_CTX(gl)->owner!=GetCurrentThreadId())gl=0; /* v10.309 (review): the process-wide current context of another thread is not this caller's */
 if(gl)r=vxgl_swap(VXGL_CTX(gl),hdc)?TRUE:FALSE;
 else {vxgl_fb *f=vxgl_fb_for(hdc);vxgl_hw_flush_all();r=f&&vxgl_swap_blit(f)?TRUE:FALSE;} /* SwapBuffers needs no current context: what the window's back buffer holds; v10.288 (review): with the core's batch drawn first */
 if(!r){static int logged;if(logged++<8)vxgl_log("SwapBuffers(%p) failed: window %p, %s\n",(void*)hdc,(void*)WindowFromDC(hdc),vxgl_fb_for(hdc)?"its buffers not shown":"no pixel format of ours on it");}
 vxgl_engine_unlock();
 return r;
}
BOOL APIENTRY DrvSwapLayerBuffers(HDC hdc,UINT planes) {if(planes&WGL_SWAP_MAIN_PLANE)return DrvSwapBuffers(hdc);return TRUE;}
/* v10.354: WGL extensions are resolved by the ICD, separately from Mesa's GL dispatch. */
static vxgl_ctx *wgl_current(void) {GET_CURRENT_CONTEXT(g);vxgl_ctx *c=g?VXGL_CTX(g):0;
 return c&&c->owner==GetCurrentThreadId()?c:0;}
static BOOL APIENTRY vx_wgl_swap_interval(int n) {
 vxgl_ctx *c=wgl_current();
 if(!c||!c->fb){SetLastError(ERROR_DC_NOT_FOUND);return FALSE;}
 if(n<0){SetLastError(ERROR_INVALID_DATA);return FALSE;}
 if(n>4)n=4;
 vxgl_engine_lock();c->fb->swap_interval=n;c->fb->swap_interval_set=1;vxgl_engine_unlock();return TRUE;
}
int vxgl_swap_default=0; /* v10.358: ACCEL_OPENGL_SWAPINTERVAL, a window's interval until the application sets one; v10.362: default 0 (WinTune's OpenGL fill test 17 MPixels/s with 1, 106 with 0: a benchmark swapping often waited a blank each time; programs that want vsync ask, as Quake 2's gl_swapinterval) */
static int APIENTRY vx_wgl_get_swap_interval(void) {vxgl_ctx *c=wgl_current();int n=0;
 if(!c||!c->fb){SetLastError(ERROR_DC_NOT_FOUND);return 0;}
 vxgl_engine_lock();n=c->fb->swap_interval_set?c->fb->swap_interval:vxgl_swap_default;vxgl_engine_unlock();return n;
}
static const char *APIENTRY vx_wgl_extensions(void) {return "WGL_EXT_swap_control WGL_EXT_extensions_string WGL_ARB_extensions_string WGL_ARB_pixel_format WGL_ARB_pbuffer";}
static const char *APIENTRY vx_wgl_extensions_arb(HDC dc) {(void)dc;return vx_wgl_extensions();}

/* v10.357: WGL_ARB_pixel_format. The five formats of pf_tab, by attribute: minimum criteria for the bit counts (the format's at least the
 * one asked for), exact match for everything else; an attribute this ICD has no such thing for (stereo, alpha, aux buffers, multisampling,
 * overlays, colour index, GDI or bitmap drawing, palettes) matches only when 0 / FALSE is asked for */
#define WGLA_NUMBER_PIXEL_FORMATS 0x2000
#define WGLA_DRAW_TO_WINDOW 0x2001
#define WGLA_DRAW_TO_BITMAP 0x2002
#define WGLA_ACCELERATION 0x2003
#define WGLA_NEED_PALETTE 0x2004
#define WGLA_NEED_SYSTEM_PALETTE 0x2005
#define WGLA_SWAP_LAYER_BUFFERS 0x2006
#define WGLA_SWAP_METHOD 0x2007
#define WGLA_NUMBER_OVERLAYS 0x2008
#define WGLA_NUMBER_UNDERLAYS 0x2009
#define WGLA_TRANSPARENT 0x200A
#define WGLA_SHARE_DEPTH 0x200C
#define WGLA_SHARE_STENCIL 0x200D
#define WGLA_SHARE_ACCUM 0x200E
#define WGLA_SUPPORT_GDI 0x200F
#define WGLA_SUPPORT_OPENGL 0x2010
#define WGLA_DOUBLE_BUFFER 0x2011
#define WGLA_STEREO 0x2012
#define WGLA_PIXEL_TYPE 0x2013
#define WGLA_COLOR_BITS 0x2014
#define WGLA_RED_BITS 0x2015
#define WGLA_RED_SHIFT 0x2016
#define WGLA_GREEN_BITS 0x2017
#define WGLA_GREEN_SHIFT 0x2018
#define WGLA_BLUE_BITS 0x2019
#define WGLA_BLUE_SHIFT 0x201A
#define WGLA_ALPHA_BITS 0x201B
#define WGLA_ALPHA_SHIFT 0x201C
#define WGLA_ACCUM_BITS 0x201D
#define WGLA_ACCUM_RED_BITS 0x201E
#define WGLA_ACCUM_GREEN_BITS 0x201F
#define WGLA_ACCUM_BLUE_BITS 0x2020
#define WGLA_ACCUM_ALPHA_BITS 0x2021
#define WGLA_DEPTH_BITS 0x2022
#define WGLA_STENCIL_BITS 0x2023
#define WGLA_AUX_BUFFERS 0x2024
#define WGLA_FULL_ACCELERATION 0x2027
#define WGLA_SWAP_COPY 0x2029
#define WGLA_SWAP_UNDEFINED 0x202A
#define WGLA_TYPE_RGBA 0x202B
#define WGLA_DRAW_TO_PBUFFER 0x202D
#define WGLA_MAX_PBUFFER_PIXELS 0x202E
#define WGLA_MAX_PBUFFER_WIDTH 0x202F
#define WGLA_MAX_PBUFFER_HEIGHT 0x2030
#define WGLA_PBUFFER_LARGEST 0x2033
#define WGLA_PBUFFER_WIDTH 0x2034
#define WGLA_PBUFFER_HEIGHT 0x2035
#define WGLA_PBUFFER_LOST 0x2036
#define WGLA_SAMPLE_BUFFERS 0x2041
#define WGLA_SAMPLES 0x2042
#define WGLA_ERROR_INVALID_PIXEL_TYPE 0x2043
#define VXGL_PBUFFER_MAX 2048
/* the value of attribute a for format pf (1-based); 0 with *ok = 0 for an attribute this ICD does not know */
static int pf_attrib(int pf,int a,int *ok) {
 PIXELFORMATDESCRIPTOR d;pf_describe(pf,&d);*ok=1;
 switch(a){
 case WGLA_NUMBER_PIXEL_FORMATS:return pf_count();
 case WGLA_DRAW_TO_WINDOW:case WGLA_SUPPORT_OPENGL:case WGLA_DRAW_TO_PBUFFER:return 1;
 case WGLA_DRAW_TO_BITMAP:case WGLA_NEED_PALETTE:case WGLA_NEED_SYSTEM_PALETTE:case WGLA_SWAP_LAYER_BUFFERS:case WGLA_NUMBER_OVERLAYS:case WGLA_NUMBER_UNDERLAYS:
 case WGLA_TRANSPARENT:case WGLA_SHARE_DEPTH:case WGLA_SHARE_STENCIL:case WGLA_SHARE_ACCUM:case WGLA_SUPPORT_GDI:case WGLA_STEREO:case WGLA_AUX_BUFFERS:
 case WGLA_ALPHA_BITS:case WGLA_ALPHA_SHIFT:case WGLA_SAMPLE_BUFFERS:case WGLA_SAMPLES:return 0;
 case WGLA_ACCELERATION:return WGLA_FULL_ACCELERATION;
 case WGLA_SWAP_METHOD:return (d.dwFlags&PFD_DOUBLEBUFFER)?WGLA_SWAP_COPY:WGLA_SWAP_UNDEFINED;
 case WGLA_DOUBLE_BUFFER:return (d.dwFlags&PFD_DOUBLEBUFFER)?1:0;
 case WGLA_PIXEL_TYPE:return WGLA_TYPE_RGBA;
 case WGLA_COLOR_BITS:return d.cColorBits==32?24:d.cColorBits; /* X8R8G8B8: 24 bits of colour, no alpha */
 case WGLA_RED_BITS:return d.cRedBits;case WGLA_RED_SHIFT:return d.cRedShift;
 case WGLA_GREEN_BITS:return d.cGreenBits;case WGLA_GREEN_SHIFT:return d.cGreenShift;
 case WGLA_BLUE_BITS:return d.cBlueBits;case WGLA_BLUE_SHIFT:return d.cBlueShift;
 case WGLA_ACCUM_BITS:return d.cAccumBits;case WGLA_ACCUM_RED_BITS:return d.cAccumRedBits;case WGLA_ACCUM_GREEN_BITS:return d.cAccumGreenBits;
 case WGLA_ACCUM_BLUE_BITS:return d.cAccumBlueBits;case WGLA_ACCUM_ALPHA_BITS:return d.cAccumAlphaBits;
 case WGLA_DEPTH_BITS:return d.cDepthBits;case WGLA_STENCIL_BITS:return d.cStencilBits;
 case WGLA_MAX_PBUFFER_WIDTH:case WGLA_MAX_PBUFFER_HEIGHT:return VXGL_PBUFFER_MAX;
 case WGLA_MAX_PBUFFER_PIXELS:return VXGL_PBUFFER_MAX*VXGL_PBUFFER_MAX;
 case 0x2070:case 0x2071:case 0x20A9:case 0x20B0:return 0; /* .575: attributes of extensions this driver does not have (BIND_TO_TEXTURE_RGB / RGBA, FRAMEBUFFER_SRGB_CAPABLE,
  * FLOAT_COMPONENTS): known, and no format has them (a request for one matches nothing; a request for none is met) */
 }
 *ok=0;return 0;
}
static int pf_minimum(int a) {return (a>=WGLA_COLOR_BITS&&a<=WGLA_AUX_BUFFERS&&a!=WGLA_RED_SHIFT&&a!=WGLA_GREEN_SHIFT&&a!=WGLA_BLUE_SHIFT&&a!=WGLA_ALPHA_SHIFT)||a==WGLA_SAMPLES||a==WGLA_SAMPLE_BUFFERS;}
static int pf_matches(int pf,const int *ia,const FLOAT *fa) {
 int k,ok,v;
 for(k=0;ia&&ia[k];k+=2){v=pf_attrib(pf,ia[k],&ok);if(!ok)continue; /* unknown: no constraint */
  if(ia[k]==WGLA_NUMBER_PIXEL_FORMATS||ia[k]==WGLA_MAX_PBUFFER_WIDTH||ia[k]==WGLA_MAX_PBUFFER_HEIGHT||ia[k]==WGLA_MAX_PBUFFER_PIXELS)continue;
  if(pf_minimum(ia[k])?v<ia[k+1]:(v!=ia[k+1]&&!(ia[k+1]&&v&&(ia[k]==WGLA_DRAW_TO_WINDOW||ia[k]==WGLA_SUPPORT_OPENGL||ia[k]==WGLA_DRAW_TO_PBUFFER||ia[k]==WGLA_DOUBLE_BUFFER))))return 0;}
 for(k=0;fa&&fa[k]!=0.0F;k+=2){int a=(int)fa[k];v=pf_attrib(pf,a,&ok);if(!ok)continue;
  if(pf_minimum(a)?(FLOAT)v<fa[k+1]:(FLOAT)v!=fa[k+1])return 0;}
 return 1;
}
static BOOL APIENTRY vx_wgl_choose_pf(HDC dc,const int *ia,const FLOAT *fa,UINT max,int *fmts,UINT *num) {
 int pf,n=pf_count();UINT got=0;(void)dc;
 if(!num){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 vxgl_screen_bpp();n=pf_count();
 /* .575 (peer review of .572, finding 13): an attribute that is not one is an error (WGL_ARB_pixel_format), not "no constraint": the
  * call answered TRUE with formats for a list it had not understood */
 {int k,ok;for(k=0;ia&&ia[k];k+=2){(void)pf_attrib(1,ia[k],&ok);if(!ok){SetLastError(ERROR_INVALID_DATA);return FALSE;}}
  for(k=0;fa&&fa[k]!=0.0F;k+=2){(void)pf_attrib(1,(int)fa[k],&ok);if(!ok){SetLastError(ERROR_INVALID_DATA);return FALSE;}}}
 for(pf=1;pf<=n;pf++)if(pf_matches(pf,ia,fa)){if(fmts&&got<max)fmts[got]=pf;got++;}
 *num=got<max?got:max;return TRUE;
}
static BOOL APIENTRY vx_wgl_get_pf_iv(HDC dc,int pf,int layer,UINT n,const int *a,int *v) {
 UINT k;int ok;(void)dc;vxgl_screen_bpp();
 if(layer){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 if(!a||!v){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 for(k=0;k<n;k++){if(a[k]!=WGLA_NUMBER_PIXEL_FORMATS&&(pf<1||pf>pf_count())){SetLastError(ERROR_INVALID_PIXEL_FORMAT);return FALSE;}
  v[k]=pf_attrib(a[k]==WGLA_NUMBER_PIXEL_FORMATS?1:pf,a[k],&ok);if(!ok){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}}
 return TRUE;
}
static BOOL APIENTRY vx_wgl_get_pf_fv(HDC dc,int pf,int layer,UINT n,const int *a,FLOAT *v) {
 UINT k;int t[64];
 if(!a||!v||n>64){SetLastError(ERROR_INVALID_PARAMETER);return FALSE;}
 if(!vx_wgl_get_pf_iv(dc,pf,layer,n,a,t))return FALSE;
 for(k=0;k<n;k++)v[k]=(FLOAT)t[k];return TRUE;
}

/* v10.357: WGL_ARB_pbuffer. A pbuffer is a hidden window of its size with the pixel format set through GDI (so opengl32 knows the format of
 * its DC, as it checks at wglMakeCurrent): the ICD gives it the same video-memory back buffer, depth and stencil as any window, and drawing
 * into it never reaches the screen (the window is never shown; its swap blits through a clipper with nothing visible). Its DC is the class's
 * own (CS_OWNDC) and stays valid until the pbuffer goes. The window may be larger than the screen (WM_GETMINMAXINFO). */
typedef struct {DWORD magic;HWND w;HDC dc;int pf,width,height;} vx_pbuffer;
#define VX_PB_MAGIC 0x50425856UL
static ATOM pb_class;
static LRESULT CALLBACK pb_proc(HWND w,UINT m,WPARAM wp,LPARAM lp) {
 if(m==WM_GETMINMAXINFO){MINMAXINFO *mm=(MINMAXINFO*)lp;mm->ptMaxSize.x=mm->ptMaxTrackSize.x=VXGL_PBUFFER_MAX+64;mm->ptMaxSize.y=mm->ptMaxTrackSize.y=VXGL_PBUFFER_MAX+64;return 0;}
 if(m==WM_ERASEBKGND)return 1;
 return DefWindowProcA(w,m,wp,lp);
}
static vx_pbuffer *pb_of(HANDLE h) {vx_pbuffer *p=(vx_pbuffer*)h;
 if(!p||IsBadReadPtr(p,sizeof(*p))||p->magic!=VX_PB_MAGIC){SetLastError(ERROR_INVALID_HANDLE);return 0;}return p;}
static HANDLE APIENTRY vx_wgl_create_pbuffer(HDC dc,int pf,int w,int h,const int *attr) {
 vx_pbuffer *p;PIXELFORMATDESCRIPTOR d;int k,largest=0;(void)dc;
 vxgl_screen_bpp();
 if(pf<1||pf>pf_count()){SetLastError(ERROR_INVALID_PIXEL_FORMAT);return 0;}
 for(k=0;attr&&attr[k];k+=2){if(attr[k]==WGLA_PBUFFER_LARGEST)largest=attr[k+1]!=0;else{SetLastError(ERROR_INVALID_DATA);return 0;}}
 if(w<=0||h<=0){SetLastError(ERROR_INVALID_DATA);return 0;}
 if(w>VXGL_PBUFFER_MAX||h>VXGL_PBUFFER_MAX){if(!largest){SetLastError(ERROR_NO_SYSTEM_RESOURCES);return 0;}if(w>VXGL_PBUFFER_MAX)w=VXGL_PBUFFER_MAX;if(h>VXGL_PBUFFER_MAX)h=VXGL_PBUFFER_MAX;}
 if(!pb_class){WNDCLASSA wc;memset(&wc,0,sizeof(wc));wc.style=CS_OWNDC;wc.lpfnWndProc=pb_proc;wc.hInstance=vxgl_inst;wc.lpszClassName="VXGLPbuffer";
  pb_class=RegisterClassA(&wc);if(!pb_class){SetLastError(ERROR_NO_SYSTEM_RESOURCES);return 0;}}
 p=(vx_pbuffer*)_mesa_calloc(sizeof(*p));if(!p){SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}
 p->w=CreateWindowExA(WS_EX_TOOLWINDOW,"VXGLPbuffer","",WS_POPUP|WS_DISABLED,-32000,-32000,w,h,NULL,NULL,vxgl_inst,NULL);
 if(!p->w){_mesa_free(p);SetLastError(ERROR_NO_SYSTEM_RESOURCES);return 0;}
 SetWindowPos(p->w,NULL,-32000,-32000,w,h,SWP_NOZORDER|SWP_NOACTIVATE|SWP_NOREDRAW); /* past any clamp at creation */
 {RECT r;if(GetClientRect(p->w,&r)){w=r.right-r.left;h=r.bottom-r.top;}}
 p->dc=GetDC(p->w);pf_describe(pf,&d);
 if(!p->dc||!SetPixelFormat(p->dc,pf,&d)){if(p->dc)ReleaseDC(p->w,p->dc);DestroyWindow(p->w);_mesa_free(p);SetLastError(ERROR_INVALID_PIXEL_FORMAT);return 0;}
 p->pf=pf;p->width=w;p->height=h;p->magic=VX_PB_MAGIC;
 vxgl_log("pbuffer %dx%d format %d: window %p\n",w,h,pf,(void*)p->w);
 return (HANDLE)p;
}
static HDC APIENTRY vx_wgl_get_pbuffer_dc(HANDLE h) {vx_pbuffer *p=pb_of(h);return p?p->dc:0;}
static int APIENTRY vx_wgl_release_pbuffer_dc(HANDLE h,HDC dc) {vx_pbuffer *p=pb_of(h);if(!p)return 0;if(dc!=p->dc){SetLastError(ERROR_DC_NOT_FOUND);return 0;}return 1;}
static BOOL APIENTRY vx_wgl_destroy_pbuffer(HANDLE h) {
 vx_pbuffer *p=pb_of(h);if(!p)return FALSE;
 p->magic=0;ReleaseDC(p->w,p->dc);DestroyWindow(p->w); /* the window's buffers go with it (fb_forget_dead, at the next context or format call) */
 big_lock();fb_forget_dead();big_unlock();
 _mesa_free(p);return TRUE;
}
static BOOL APIENTRY vx_wgl_query_pbuffer(HANDLE h,int a,int *v) {
 vx_pbuffer *p=pb_of(h);if(!p)return FALSE;if(!v){SetLastError(ERROR_INVALID_DATA);return FALSE;}
 if(a==WGLA_PBUFFER_WIDTH)*v=p->width;else if(a==WGLA_PBUFFER_HEIGHT)*v=p->height;else if(a==WGLA_PBUFFER_LOST)*v=0;
 else{SetLastError(ERROR_INVALID_DATA);return FALSE;}
 return TRUE;
}
PROC APIENTRY DrvGetProcAddress(LPCSTR name) {
 if(vxgl_gone||!name)return 0;
 if(!strcmp(name,"wglChoosePixelFormatARB"))return (PROC)vx_wgl_choose_pf; /* v10.357 */
 if(!strcmp(name,"wglGetPixelFormatAttribivARB"))return (PROC)vx_wgl_get_pf_iv;
 if(!strcmp(name,"wglGetPixelFormatAttribfvARB"))return (PROC)vx_wgl_get_pf_fv;
 if(!strcmp(name,"wglCreatePbufferARB"))return (PROC)vx_wgl_create_pbuffer;
 if(!strcmp(name,"wglGetPbufferDCARB"))return (PROC)vx_wgl_get_pbuffer_dc;
 if(!strcmp(name,"wglReleasePbufferDCARB"))return (PROC)vx_wgl_release_pbuffer_dc;
 if(!strcmp(name,"wglDestroyPbufferARB"))return (PROC)vx_wgl_destroy_pbuffer;
 if(!strcmp(name,"wglQueryPbufferARB"))return (PROC)vx_wgl_query_pbuffer;
 if(!strcmp(name,"wglSwapIntervalEXT"))return (PROC)vx_wgl_swap_interval;
 if(!strcmp(name,"wglGetSwapIntervalEXT"))return (PROC)vx_wgl_get_swap_interval;
 if(!strcmp(name,"wglGetExtensionsStringEXT"))return (PROC)vx_wgl_extensions;
 if(!strcmp(name,"wglGetExtensionsStringARB"))return (PROC)vx_wgl_extensions_arb;
 return (PROC)_glapi_get_proc_address(name);
}
BOOL APIENTRY DrvDescribeLayerPlane(HDC hdc,int pf,int layer,UINT n,LPLAYERPLANEDESCRIPTOR d) {(void)hdc;(void)pf;(void)layer;(void)n;(void)d;return FALSE;}
int APIENTRY DrvSetLayerPaletteEntries(HDC hdc,int layer,int start,int n,CONST COLORREF *p) {(void)hdc;(void)layer;(void)start;(void)n;(void)p;return 0;}
int APIENTRY DrvGetLayerPaletteEntries(HDC hdc,int layer,int start,int n,COLORREF *p) {(void)hdc;(void)layer;(void)start;(void)n;(void)p;return 0;}
BOOL APIENTRY DrvRealizeLayerPalette(HDC hdc,int layer,BOOL r) {(void)hdc;(void)layer;(void)r;return FALSE;}

/* ---------------------------------------------------------------- the swap: back buffer -> the window, through DirectDraw's blit */
/* the window's client rectangle on the screen and the part of a w x h buffer that fits it. v10.321: *screen = 0 when ClientToScreen fails:
 * dst is then the client rectangle only and must not be used on the primary (it would be the desktop's top left) */
static int window_rects(vxgl_fb *fb,RECT *dst,RECT *src,int *screen) {
 POINT o;
 *screen=0;
 if(!GetClientRect(fb->hwnd,dst))return 0;
 o.x=0;o.y=0;
 if(ClientToScreen(fb->hwnd,&o)){OffsetRect(dst,o.x,o.y);*screen=1;}
 else{static int logged;if(logged++<4)vxgl_log("window %p: ClientToScreen failed: not on the primary\n",(void*)fb->hwnd);}
 src->left=0;src->top=0;src->right=fb->w<(GLuint)(dst->right-dst->left)?fb->w:dst->right-dst->left;src->bottom=fb->h<(GLuint)(dst->bottom-dst->top)?fb->h:dst->bottom-dst->top;
 dst->right=dst->left+src->right;dst->bottom=dst->top+src->bottom;
 return src->right>0&&src->bottom>0;
}
/* v10.249: the front buffer of a double-buffered window: a surface like the back buffer, made the first time it is drawn or read, and
 * filled from the window (what the screen shows there) the first time after each swap */
/* .590 (VX800 board, VXGL v5.0 / v5.1: a front buffer cleared right after a swap read back as the swapped picture, one to seven tries in
 * 30, only while the swap's copy runs asynchronously): whatever draws into the front marks it loaded as well as drawn (fstate |= 3 in
 * vxgl_mesa.c and vxgl_tris.c). A load that did not happen before the draw (it failed, or was not tried because the surfaces were
 * locked) was otherwise done at the next use, over what had been drawn. The failures are logged (VXGL_LOG). */
int vxgl_front_load(vxgl_fb *fb) {
 DDSURFACEDESC2 sd;RECT dst,src;HRESULT r;int scr;
 if(vxgl_ftrace)vxgl_log("ftrace: load: fstate %d, front %p, soft %d, locked %d, hold %d, held %d\n",fb->fstate,(void*)fb->front,fb->fsoft,fb->locked,fb->hold,fb->held);
 vxgl_fb_settle(fb);
 if(fb->fstate&1)return fb->front||fb->fbits;
 if(!fb->front&&!fb->fsoft&&fb->back){
  memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
  if(IDirectDrawSurface7_GetSurfaceDesc(fb->back,&sd)!=DD_OK)return 0;
  sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;sd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY|DDSCAPS_3DDEVICE; /* as the back buffer: the same pitch */
  if(IDirectDraw7_CreateSurface(vxgl_dd,&sd,&fb->front,0)!=DD_OK){static int logged;if(logged++<4)vxgl_log("front buffer surface %ux%u failed: system memory instead\n",fb->w,fb->h);fb->front=0;}
  else {GLubyte *cpu=0;vxgl_log("front buffer %ux%u made\n",fb->w,fb->h);
   /* v10.317: where the engine can draw into it (ACCEL_OPENGL_FRONT): its video memory offset, the back buffer's pitch and format */
   memset(&fb->sfront,0,sizeof(fb->sfront));
   if(surface_where(fb->front,&fb->sfront,&cpu)&&fb->sfront.pitch==fb->sback.pitch&&fb->sfront.width==fb->w&&fb->sfront.height==fb->h)fb->sfront.format=fb->sback.format;
   else memset(&fb->sfront,0,sizeof(fb->sfront));}
 }
 if(!fb->front) { /* v10.261: no surface (or no back surface to model it on): the system-memory front, filled from the window */
  fb->fsoft=1;fb->fstate=(fb->fstate|1)&~2;
  if(!soft_front(fb))return 0;
  soft_front_fill(fb);return 1;
 }
 if(fb->cb==desktop_bpp&&primary_ok()&&window_rects(fb,&dst,&src,&scr)&&scr){
  r=IDirectDrawSurface7_Blt(fb->front,&src,primary,&dst,DDBLT_WAIT,NULL);
  if(vxgl_ftrace)vxgl_log("ftrace: load: copied from the window %ld,%ld-%ld,%ld: %08lX\n",(long)dst.left,(long)dst.top,(long)dst.right,(long)dst.bottom,(unsigned long)r);
  if(r!=DD_OK){static int logged;if(logged++<24)vxgl_log("front buffer: the window's pixels not copied %08lX\n",(unsigned long)r);if(!fb_seed(fb,fb->front,0)){static int l2;if(l2++<24)vxgl_log("front buffer: not loaded (the copy %08lX and the fill failed)\n",(unsigned long)r);return 0;}} /* v10.366: black, not old video memory; .532 (B6): a front that could not be seeded is not marked loaded */
 }
 else if(!fb_seed(fb,fb->front,0)){static int l3;if(l3++<24)vxgl_log("front buffer: not loaded (no copy possible and the fill failed)\n");return 0;} /* v10.366: no copy possible (another desktop depth, off the primary): black; .532 (B6): not marked loaded otherwise */
 fb->dmg_on[1]=0; /* v10.366: what the window shows (or black: then shown where drawn) */
 fb->fstate=(fb->fstate|1)&~2;
 return 1;
}
static int blit_to_window(vxgl_fb *fb,LPDIRECTDRAWSURFACE7 from,int *shown);
/* v10.287: a front buffer not shown (neither DirectDraw nor GDI took it) stays to be shown at the next glFlush / glFinish */
void vxgl_front_present(vxgl_fb *fb) {int shown=0;vxgl_fb_settle(fb);if(fb->fsoft){soft_front_show(fb);fb->fstate&=~2;}else if(fb->front&&blit_to_window(fb,fb->front,&shown)&&shown)fb->fstate&=~2;
 if(vxgl_ftrace)vxgl_log("ftrace: present: shown %d, fstate now %d\n",shown,fb->fstate);}
int vxgl_swap_blit(vxgl_fb *fb) {int shown,k,n=fb->swap_interval_set?fb->swap_interval:vxgl_swap_default;
 /* Interval zero keeps the unthrottled path. Single-buffer/front presentations do not wait. */
 if(n&&fb->pf>=1&&fb->pf<=PF_COUNT&&pf_tab[fb->pf-1].db&&!IsIconic(fb->hwnd)&&vxgl_dd){
  /* v10.360 (VXGAME: 29 fps at 60 Hz in every scene; Quake 2's timedemo 15): one wait for the start of a blank an interval. The end of
   * the next blank and then the start of the one after made every swap wait for the second blank after the frame (half the refresh, a
   * quarter when the frame took longer than one). The VxD's start-of-blank wait leaves a blank already running first. */
  if(vxgl_engine_wait())for(k=0;k<n;k++){
   if(IDirectDraw7_WaitForVerticalBlank(vxgl_dd,DDWAITVB_BLOCKBEGIN,NULL)!=DD_OK)break;}}
 return blit_to_window(fb,fb->back,&shown);}
/* v10.265: the picture through GDI when DirectDraw's blit cannot show it (a blit error, the window's format not the desktop's: GDI converts).
 * The surface is read through a lock; a lost surface shows nothing this time (vxgl_swap's resize makes it again). */
static DWORD gdi_error; /* v10.287: GetLastError after a failed StretchDIBits, for the log */
/* .532 (review of .530, B6): dmg, when not NULL, is the part to show (a single-buffered surface's or the front's damage): GDI copies
 * only it, as the DirectDraw blit does; the rows from its top as a DIB of its own height (no source y in a top-down DIB) */
static int gdi_present(vxgl_fb *fb,LPDIRECTDRAWSURFACE7 from,const RECT *src,const RECT *dmg) {
 DDSURFACEDESC2 sd;struct {BITMAPINFOHEADER h;DWORD m[3];} bi;HDC w;DWORD bytes=fb->cb==16?2:4;int ok=0;
 if(!from||fb->locked)return 0;
 vxgl_batch_flush(); /* v10.275 */
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 if(IDirectDrawSurface7_Lock(from,NULL,&sd,DDLOCK_WAIT|DDLOCK_NOSYSLOCK|DDLOCK_READONLY,NULL)!=DD_OK)return 0;
 if(sd.lpSurface&&sd.lPitch>0&&!(sd.lPitch&3)&&(w=GetDC(fb->hwnd))!=0) {
  memset(&bi,0,sizeof(bi));bi.h.biSize=sizeof(bi.h);bi.h.biWidth=(LONG)((DWORD)sd.lPitch/bytes);bi.h.biHeight=-(LONG)fb->h;bi.h.biPlanes=1;
  bi.h.biBitCount=(WORD)(bytes*8);bi.h.biCompression=BI_BITFIELDS;
  if(bytes==2){bi.m[0]=0xF800UL;bi.m[1]=0x07E0UL;bi.m[2]=0x001FUL;}else{bi.m[0]=0xFF0000UL;bi.m[1]=0xFF00UL;bi.m[2]=0xFFUL;}
  /* the whole image as the source rows (a top-down DIB), the visible part as the rectangle */
  /* v10.287 (review): the scan lines copied; 0 is a failure as much as GDI_ERROR (it was taken as shown) */
  {int n;
   if(dmg){int dw=(int)(dmg->right-dmg->left),dh=(int)(dmg->bottom-dmg->top);bi.h.biHeight=-(LONG)dh;
    n=StretchDIBits(w,(int)dmg->left,(int)dmg->top,dw,dh,(int)dmg->left,0,dw,dh,(GLubyte*)sd.lpSurface+(DWORD)dmg->top*(DWORD)sd.lPitch,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY);}
   else n=StretchDIBits(w,0,0,(int)src->right,(int)fb->h,0,0,(int)src->right,(int)fb->h,sd.lpSurface,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY);
   ok=n!=0&&n!=GDI_ERROR;if(!ok)gdi_error=GetLastError();}
  ReleaseDC(fb->hwnd,w);
 }
 IDirectDrawSurface7_Unlock(from,NULL);
 return ok;
}
/* v10.265: 1 the picture is in the window, or this frame could not be shown for a reason that passes (a lost surface: a mode change, the
 * engine reset; no primary for now; a minimised window), 0 only when there is no window or no buffer at all. A failed SwapBuffers
 * is fatal to games (Quake 2: "GLimp_EndFrame() - SwapBuffers() failed!"); a frame that is not shown is not. */
static int blit_to_window(vxgl_fb *fb,LPDIRECTDRAWSURFACE7 from,int *shown) {
 RECT dst,src;HRESULT r=DD_OK;int how=0,scr,part=-1;static int fails;
 *shown=0;
 {VXGL_PROF_BEGIN;vxgl_fb_settle(fb);VXGL_PROF_END(13);} /* v10.363: the swap in its parts (VXGL.LOG): 13 settling the buffers (the queue sent), 14 the blit */
 if(!IsWindow(fb->hwnd)){vxgl_log("swap: the window is gone\n");return 0;}
 if(!from){if(fails++<8)vxgl_log("swap: no back buffer surface (%ux%u)\n",fb->w,fb->h);return 0;}
 if(!window_rects(fb,&dst,&src,&scr)){*shown=1;return 1;} /* a minimised window: nothing to show */
 /* v10.366 (3D Pipes: its dissolve showed black, or old video memory, over the desktop GDI had put in the window): a single-buffered
  * window's surface and a front buffer show only what was drawn into them since they were last shown, as drawing into the window
  * itself would leave the rest; a swap shows the whole back buffer */
 {int k=from==fb->front?1:(from==fb->back&&!(fb->pf>=1&&fb->pf<=PF_COUNT&&pf_tab[fb->pf-1].db))?0:-1;
  if(k>=0){RECT d=fb->dmg[k];
   if(!fb->dmg_on[k]){*shown=1;return 1;} /* nothing new */
   if(d.right>src.right)d.right=src.right;if(d.bottom>src.bottom)d.bottom=src.bottom;
   if(d.left>=d.right||d.top>=d.bottom){fb->dmg_on[k]=0;*shown=1;return 1;} /* outside the window's visible part */
   dst.left+=d.left;dst.top+=d.top;dst.right=dst.left+(d.right-d.left);dst.bottom=dst.top+(d.bottom-d.top);src=d;}
  part=k;}
 if(fb->cb!=desktop_bpp)vxgl_screen_bpp();
 /* v10.321: DirectDraw only with the window's screen position and the clipper on the primary and this window (clip_ok); the primary
  * made again after a lost surface has its clipper checked again too */
 if(scr&&fb->cb==desktop_bpp&&primary_ok()&&clip_ok(fb->hwnd)) {
  /* v10.424: the blit runs asynchronously (VXDDRAW and the VxD, ACCEL_ASYNC_PRESENT): the next frame's first packet, a lock, or any
   * 2D call finishes it */
  if(dd_present)dd_present(1);
  {VXGL_PROF_BEGIN;r=IDirectDrawSurface7_Blt(primary,&dst,from,&src,DDBLT_WAIT,NULL);VXGL_PROF_END(14);}
  if(r==DDERR_SURFACELOST&&primary_ok()&&clip_ok(fb->hwnd))r=IDirectDrawSurface7_Blt(primary,&dst,from,&src,DDBLT_WAIT,NULL);
  if(dd_present)dd_present(0);
  if(r==DD_OK)how=1;
 }
 gdi_error=0;
 if(!how&&IDirectDrawSurface7_IsLost(from)==DD_OK&&part<0&&gdi_present(fb,from,&src,NULL))how=2;
 if(!how&&part>=0&&IDirectDrawSurface7_IsLost(from)==DD_OK){RECT whole;int s2;RECT d=src;if(window_rects(fb,&dst,&whole,&s2)&&gdi_present(fb,from,&whole,&d))how=2;} /* .532 (B6): GDI shows only the damage, as DirectDraw does (it showed the whole surface) */
 if(how&&part>=0)fb->dmg_on[part]=0;
 *shown=how!=0;
 if(fb->swap_logged<8||(!how&&fails<8)) { /* v10.422: per window (WinTune runs its GL test again in the same process) */
  if(fb->swap_logged<8)fb->swap_logged++;if(!how)fails++;
  vxgl_log("swap: window %p, %ld,%ld-%ld,%ld from %ld,%ld-%ld,%ld: %s (blit %08lX, GDI error %lu, desktop %d bpp, window %d bpp, back %s)\n",(void*)fb->hwnd,
   (long)dst.left,(long)dst.top,(long)dst.right,(long)dst.bottom,(long)src.left,(long)src.top,(long)src.right,(long)src.bottom,
   how==1?"DirectDraw":how==2?"GDI":"not shown",(unsigned long)r,(unsigned long)gdi_error,desktop_bpp,fb->cb,IDirectDrawSurface7_IsLost(from)==DD_OK?"there":"lost");
 }
 return 1;
}
vxgl_fb *vxgl_fb_for(HDC hdc) {
 HWND w=WindowFromDC(hdc);vxgl_fb *f;
 EnterCriticalSection(&lock);f=w?fb_find(w):0;LeaveCriticalSection(&lock);return f;
}
void vxgl_screen_bpp(void) {HDC d=GetDC(0);desktop_bpp=GetDeviceCaps(d,BITSPIXEL)*GetDeviceCaps(d,PLANES);ReleaseDC(0,d);}

extern void (*_mesa_out_hook)(const char *s);
static void mesa_out(const char *m) {static int n;if(n<64){n++;vxgl_log("%s%s\n",m,n==64?" (Mesa's messages: no more logged)":"");}} /* v10.260: Mesa's warnings and errors into VXGL.LOG, not stderr */
BOOL WINAPI DllMain(HINSTANCE h,DWORD why,LPVOID r) {
 vxgl_inst=h; /* v10.349: the sampler's base */
 if(why==DLL_PROCESS_ATTACH) {
  InitializeCriticalSection(&lock);InitializeCriticalSection(&engine);InitializeCriticalSection(&logcs);logcs_ok=1; /* v10.330 */
  /* .506: ACCEL_OPENGL_ALIGNHEAP removed */
  log_on=reg_dword("VXGL_LOG",0)!=0;vxgl_ftrace=reg_dword("VXGL_FTRACE",0)!=0; /* .591: the front buffer's steps in VXGL.LOG (logging) */if(vxgl_ftrace)log_on=1;vxgl_framelog=reg_dword("VXGL_FRAMELOG",0)!=0;smp_on=reg_dword("VXGL_SAMPLE",0)!=0; /* v10.349 */if(vxgl_framelog)log_on=1;icd_enabled=(1)!=0;vxgl_hw_enabled=(1)!=0;
  vxgl_lock_keep=(1); /* v10.267 */
  vxgl_batch_max=(32); /* v10.275: packets a VxD call (0/1 = one) */
  /* .530: ACCEL_OPENGL_SWAPINTERVAL fixed at 0 (vxgl_swap_default), ACCEL_OPENGL_FASTPOINTS (default 0) removed */
  vxgl_gl_combine=(1)!=0;vxgl_gl_cube=(1)!=0;vxgl_gl_projected=(1)!=0; /* v10.295 */
  vxgl_gl_logicop=(1)!=0;vxgl_gl_colormask=(1)!=0;vxgl_gl_wide=(1)!=0;vxgl_gl_strips=(1)!=0; /* v10.323 */vxgl_gl_flatstrips=(1)!=0; /* v10.480 */vxgl_gl_flatbatch=(1)!=0;{extern int _tnl_vx855_emit;_tnl_vx855_emit=(1)!=0;}vx_d3d_gl_hwflat=(1)!=0; /* v10.481; v10.482: 1 by default (VXFLAT v1.1 on the board: the template's pixels in every scene) */{DWORD h=(1); /* v10.483: 1 by default (VXFLAT v1.2 on the board: rule 1 the template's pixels) */vxgl_gl_hwflatstrip=h<=2?(int)h:0;} /* v10.482 */{extern int _mesa_vx855_fastint;_mesa_vx855_fastint=(1)!=0;} /* v10.483: lighting and fog without the float-to-int call */vxgl_gl_snap=(1)!=0; /* v10.324 */
  vxgl_gl_lines=(1)!=0;vxgl_gl_points=(1)!=0;vxgl_gl_blendenv=(1)!=0;vxgl_gl_qvertex=(1)!=0; /* v10.300 */
  vxgl_gl_longmip=(1)!=0; /* v10.357: default 0; v10.361: default 1 again (0 drew every 512+ mipmapped texture in software: VXGAME bigtex 117 fps against 968, WinTune) */vxgl_gl_unitmip=(1)!=0;vxgl_gl_border=(1)!=0;
  vxgl_gl_scale=(1)!=0;vxgl_gl_lerp=(1)!=0;vxgl_gl_blendop=(1)!=0; /* v10.304 */
  /* v10.317 */
  vxgl_gl_npot=(1)!=0;vxgl_gl_fxt1=(1)!=0;vxgl_gl_depthtex=(1)!=0;
  vxgl_gl_ctable=(1)!=0;vxgl_gl_dot3rgb=(1)!=0;vxgl_gl_cubemip=(1)!=0;

  {extern int vxgl_gl_zclear3d;DWORD z3=(1);vxgl_gl_zclear3d=z3>2?2:(int)z3;} /* v10.345; v10.346 default 1; v10.362: 1 the depth (and stencil) by the engine, the colour by the 2D fill; 2 the colour in the engine's quad too (v10.346-v10.361) */
  vxgl_gl_nocolor=(1)!=0; /* v10.346; v10.347 default 1 */
  /* v10.358: ACCEL_3D_GUARD as the VxD has it (its control word 0xFFFFFE32): the engine's fast path sends vertices at -0.5 only when the
   * VxD takes them (a packet it refused would be lost) */
  {DWORD q=0xFFFFFE32UL,o[4]={0,0,0,0};vx_d3d_feat&=~VX_F_GUARD;if(vxd_call(0x85500012UL,&q,4,o,16)&&o[1]==0x47524431UL&&o[0])vx_d3d_feat|=VX_F_GUARD;}
  /* v10.359: ACCEL_OPENGL_SSE (default 1): Mesa's vertex transform and clip test by SSE (vxgl_sse.c) when the processor has SSE and FXSR
   * (CPUID 1 EDX bits 25, 24) and the system saves the XMM registers (CR4 OSFXSR, bit 9: the VxD's control 0xFFFFFE34) */
  {DWORD q=0xFFFFFE34UL,o[4]={0,0,0,0};vxgl_sse_cr4=0;vxgl_sse_edx=0;
   if(vxd_call(0x85500012UL,&q,4,o,16)&&o[1]==0x31455353UL){vxgl_sse_cr4=o[0];vxgl_sse_edx=o[2];}
   vxgl_sse_ok=(1)!=0&&(vxgl_sse_edx&0x03000000UL)==0x03000000UL&&(vxgl_sse_cr4&0x200UL);}
  {extern int _mesa_vx855_rsqrt;_mesa_vx855_rsqrt=vxgl_sse_ok&&(1)!=0;} /* v10.484: the lighting's square roots and divisions by SSE */
  {extern int _tnl_vx855_clipstrip;_tnl_vx855_clipstrip=(1)!=0;} /* v10.485: a clipped strip split, its runs inside as strips */
  {extern int _mesa_vx855_lightfast;_mesa_vx855_lightfast=(1)!=0;} /* v10.485: one positional light in one straight loop */
  {extern int vxgl_ssepass_on;vxgl_ssepass_on=vxgl_sse_ok&&(1)!=0?1|((1)?4:0):0;}{extern int vxgl_ssepass_outside;vxgl_ssepass_outside=(1)!=0;}{extern int vxgl_ssepass_box,vxgl_ssepass_shine,vxgl_ssepass_pack;extern uint32_t vx_d3d_gl_cullrun; /* v10.490 */
   vxgl_ssepass_box=(1)!=0;vxgl_ssepass_shine=(1)!=0;vxgl_ssepass_pack=(1)!=0;
   vx_d3d_gl_cullrun=(1)!=0; /* v10.492: SSESNAP 0 by default (board: slower) */
   {extern int vxgl_ssepass_cache,vxgl_ssepass_mask,vxgl_gl_flatnocopy;vxgl_ssepass_cache=(1)!=0;vxgl_ssepass_mask=(1)!=0;
    vxgl_gl_flatnocopy=(1)!=0;}
   {extern int vxgl_ssepass_tight,_tnl_vx855_fastbind;extern uint32_t vx_d3d_gl_cullrun2;vxgl_ssepass_tight=(1)!=0; /* v10.493 */
    vx_d3d_gl_cullrun2=(1)!=0;_tnl_vx855_fastbind=(1)!=0;}
   {extern int vxgl_ssepass_fuse,vxgl_ssepass_accept,vxgl_ssepass_box2,vxgl_ssepass_partial,_vbo_vx855_listbind; /* v10.495 */
    vxgl_ssepass_fuse=(1)!=0;vxgl_ssepass_accept=(1)!=0;vxgl_ssepass_box2=(1)!=0;
    vxgl_ssepass_partial=(1)!=0;_vbo_vx855_listbind=(1)!=0;}
   {extern int vxgl_ssepass_immbox;vxgl_ssepass_immbox=(1)!=0;} /* v10.496 */
   {extern int vxgl_ssepass_snap2,vxgl_ssepass_outfast,vxgl_ssepass_early;vxgl_ssepass_snap2=(1)!=0; /* v10.497 */
    vxgl_ssepass_outfast=(1)!=0;vxgl_ssepass_early=(1)!=0;}} /* v10.492 */ /* v10.489: a draw whole outside not lit (as Mesa) */ /* v10.488: bits 2 unlit draws (0 by default: board, slower), 4 draws not from a list */ /* v10.487: a display list's draw in one SSE pass (vxgl_ssepass.c) */
  {extern int vxgl_gl_ubcolor;extern uint32_t vx_d3d_prepack,vx_d3d_indexed;DWORD q=0xFFFFFE50UL,o[4]={0,0,0,0};int area=0,pre=0; /* v10.501 */
   vxgl_gl_ubcolor=(1)!=0;{extern int _vbo_vx855_fastindex;_vbo_vx855_fastindex=(1)!=0;}
   vx_d3d_gl_gllist=(1)!=0;{DWORD gw=(2);extern uint32_t vx_d3d_gw_fixed;vx_d3d_gl_glwords=gw!=0;vx_d3d_gw_fixed=gw>=2;} /* .505: 2 the fixed layouts too */
   {extern int vxgl_gl_fan,vxgl_ssepass_range;vxgl_gl_fan=(1)!=0;vxgl_ssepass_range=(1)!=0;} /* .503 */
   vx_d3d_gl_plan=(1)!=0;vxgl_tsc_on=reg_dword("ACCEL_OPENGL_TSCPROF",0)!=0; /* .506; .530: kept for development (VXFLAT turns it on itself): DELETE WHEN DEVELOPMENT IS FINISHED */
   {extern int _vbo_vx855_minmax2;_vbo_vx855_minmax2=vxgl_sse_ok&&(vxgl_sse_edx&0x04000000UL)&&(1)!=0;} /* .506 */
   {extern int vxgl_ssepass_sse2,vxgl_ssepass_defer;vxgl_ssepass_sse2=vxgl_sse_ok&&(vxgl_sse_edx&0x04000000UL)&&(1)!=0;vxgl_ssepass_defer=(1)!=0;} /* .503: SSE2 by CPUID 1 EDX bit 26 */
   /* the packets in the Direct3D side's prepacked form (10 words a vertex) and, where the VxD has its vertex area, indexed */
   /* (the VxD's answer to the Direct3D HAL: o[3] bit 26 ACCEL_D3D_PREPACK as the VxD takes it, o[2] bit 7 its vertex area; an older VxD: neither) */
   if(vxd_call(0x85500012UL,&q,4,o,16)&&o[1]==0x56584433UL){if(o[2]&0x800UL)area=(o[2]>>7)&1;if(o[3]&0x80000000UL)pre=(o[3]>>26)&1;}
   vx_d3d_prepack=pre&&(1)!=0;vx_d3d_indexed=vx_d3d_prepack&&area;} /* .573 (.574: permanent, the switch VXGL_Indexed and VXGLIX removed; board, VXGLIX v1.1: Quake 2's scene -7.0%, Quake 3's -19.0%, a clipped grid -9.7%, the pictures the same): indexed packets where the VxD has its vertex area, every primitive kind by index (vx_d3d_core.c gl_ix_emit) */
  {extern int vxgl_gl_trilist,_tnl_vx855_cvacache,_vbo_vx855_fastvtx,vxgl_gl_storesnap,vxgl_gl_glapione; /* v10.500 */
   vxgl_gl_trilist=(1)!=0;_tnl_vx855_cvacache=(1)!=0;_vbo_vx855_fastvtx=(1)!=0;
   vxgl_gl_storesnap=(1)!=0;vxgl_gl_glapione=(1)!=0;}
  /* .506: ACCEL_OPENGL_CLIPINT removed */ /* v10.485: the clip test's compares in integer registers; v10.486: 0 by default (board: VXFLAT v1.5, 2-7% slower; Lattice: the clip test 7.7%) */
  vxgl_gl_smoothatest=(1)!=0;vxgl_gl_mirrorclamp=(1)!=0; /* v10.353 */
  vxgl_gl_front=(1)!=0;vxgl_gl_tex3d=(1)!=0;
  vxgl_gl_query=(1)!=0;vxgl_gl_lodclamp=(1)!=0; /* v10.315 */vxgl_gl_units3=(1)!=0; /* v10.313: default 1 again (unit 2 reads its set) */{char e[8];if(GetEnvironmentVariableA("VXGL_UNITS3",e,sizeof(e)))vxgl_gl_units3=e[0]=='1';} /* v10.309: VXGL turns it on for its own run */vxgl_gl_pstipple=(1)!=0;vxgl_gl_smooth=(1)!=0;vxgl_gl_fprog=(1)!=0;vxgl_gl_npotwrap=(1)!=0;vxgl_gl_unit3d=(1)!=0;vxgl_gl_rtt=(1)!=0; /* v10.321 */vxgl_gl_s3tc=(1)!=0; /* v10.307 */vxgl_gl_tex16=(1)!=0;{char e[8];if(GetEnvironmentVariableA("VXGL_TEX16",e,sizeof(e)))vxgl_gl_tex16=e[0]=='1';} /* (VXTEX turns it off for its own run) */{DWORD r=(3);vxgl_gl_texring=r>3?3:(int)r;} /* v10.366 */{DWORD in=0xBA,o[8];memset(o,0,sizeof(o));vxgl_seq_ring=vxd_call(0x85500022UL,&in,4,o,32)&&o[0]==0xBA&&o[1];} /* v10.378: the VxD's ring (TEXDBG 0xBA) */
  vxgl_screen_bpp();_mesa_out_hook=mesa_out;
  vxgl_log("VXGL %s (driver v10.%d) loaded in process %lu: desktop %d bpp, hardware %d\n",VXGL_VERSION,DRV_VER_BUILD,(unsigned long)GetCurrentProcessId(),desktop_bpp,vxgl_hw_enabled);
  vxgl_log("SSE transform %s (CPUID 1 EDX %08lX, CR4 %08lX)\n",vxgl_sse_ok?"on":"off",vxgl_sse_edx,vxgl_sse_cr4);
  {DWORD q=0xFFFFFE36UL,o[4]={0,0,0,0}; /* v10.363: the display FIFO's registers, ACCEL_3D_ZFIX */
   if(vxd_call(0x85500012UL,&q,4,o,16)&&o[2]==0x4F464946UL)vxgl_log("display FIFO: SR16 %02lX SR17 %02lX SR18 %02lX SR22 %02lX SR21 %02lX CR68 %02lX CR94 %02lX CR95 %02lX; ACCEL_3D_ZFIX %lu (sent by %lu draws so far)\n",
    o[0]&255,(o[0]>>8)&255,(o[0]>>16)&255,o[0]>>24,o[1]>>24,o[1]&255,(o[1]>>8)&255,(o[1]>>16)&255,o[3]&255,o[3]>>8);}
 } else if(why==DLL_THREAD_DETACH) {
  if(!vxgl_gone)vxgl_thread_gone(); /* v10.259 */
 } else if(why==DLL_PROCESS_DETACH&&r) { /* v10.260: process exit: the C runtime is shut down after this returns; opengl32's own clean-up may
                                           * still call Drv* (DeleteContext, ReleaseContext): they do nothing from now on (3D Pipes faulted in
                                           * KERNEL32 under fprintf, the runtime's stdio lock gone) */
  vxgl_log("process exit: VXGL entry points off\n");vxgl_gone=1;_mesa_out_hook=0;
 } else if(why==DLL_PROCESS_DETACH&&!r) { /* FreeLibrary; at process exit DirectDraw may be gone already */
  vxgl_fb *f;vxgl_gone=1;
  for(f=fbs;f;f=f->next)fb_free_surfaces(f);
  if(clipper)IDirectDrawClipper_Release(clipper);
  if(primary)IDirectDrawSurface7_Release(primary);
  if(vxgl_dd)IDirectDraw7_Release(vxgl_dd);
  if(vxgl_vxd!=INVALID_HANDLE_VALUE)CloseHandle(vxgl_vxd);
 }
 return TRUE;
}
/* v10.497: o[0] the longest VxD call since the last clear (microseconds), o[1] its control code, o[2] the calls, o[3] their microseconds,
 * o[4] busy answers tried again, o[5] of them after a 1 ms Sleep; clear: the counts start again; returns the TSC's MHz */
uint32_t __stdcall VXGL_StallStats(uint32_t *o,uint32_t clear) {
 static double mhz;
 if(mhz<=0){double t0=qpc_us(),t1;unsigned long c0=tsc_low(),c1;while((t1=qpc_us())-t0<20000);c1=tsc_low();mhz=(double)(c1-c0)/(t1-t0);if(mhz<=0)mhz=1;}
 if(o){o[0]=(uint32_t)((double)st_max/mhz);o[1]=(uint32_t)st_code;o[2]=(uint32_t)st_calls;o[3]=(uint32_t)(st_total/mhz);o[4]=(uint32_t)st_busy;o[5]=(uint32_t)st_sleep;}
 if(clear){st_max=st_code=st_calls=st_busy=st_sleep=0;st_total=0;}
 return (uint32_t)mhz;
}
