/* VX855 Win9x DirectDraw HAL. Synchronous native video-to-video blits only. */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include "ddhal32/ddrawi.h"
#define VXD32
#include "ddhal32/vmdahal32.h"
#include "vx85598.h"
#include "vx855_2d.h"
#include "vx855_flip.h"
#include <stddef.h>
#include <string.h>
#include <stdarg.h>
#include "vx_d3d_hal.h"
int VX_D3D_forget_surface(DDRAWI_DDRAWSURFACE_LCL *);
static VMDAHAL_t *hal;
static HINSTANCE instance;
static HANDLE device=INVALID_HANDLE_VALUE;
static DWORD device_pid;
static volatile DWORD reopened,last_ioctl_error; /* v10.230: handles opened again after a failed call (VX_DD_Reopened) */
DWORD WINAPI VX_DD_Reopened(void) {return reopened;}
/* v10.385 (board, GTA3 v10.384: 500 frames of 75 ms and more, the longest 600-1150 ms, almost none of it inside Flip): the time between
 * flips by callback (TSC cycles / 256): 0 DrawPrimitives2, 1 Lock, 2 Unlock, 3 Blt, 4 surface create and destroy, 5 Flip; the rest of a
 * frame is outside the driver's callbacks. Frames of 75 ms and more are summed apart; the 8 longest are kept with their parts and their
 * Lock, Unlock and surface-create counts. TSC cycles per ms from GetTickCount over 3 s. VX_DD_QueryFrames (VXD3DMON) reads them. */
unsigned long VX_dd_tsc(void);
#pragma aux VX_dd_tsc = 0x0F 0x31 value [eax] modify exact [eax edx];
#define FT_CATS 6
static volatile DWORD ft_last,ft_last_tick,ft_cur[FT_CATS],ft_cnt[4],ft_frames,ft_long,ft_all[FT_CATS+1],ft_lsum[FT_CATS+1],ft_worst[8][FT_CATS+6],ft_cpms=1000000UL,ft_cal_t,ft_cal_c,ft_cal_ok,ft_dp2,ft_run_last,ft_run_all,ft_run_long;
static int ioctl(DWORD code,const void *in,DWORD n,void *out,DWORD z);
/* v10.389: the flipping thread's run time (ms, the VxD's _GetThreadExecTime through TEXDBG 0xBD 0x81): a long frame whose thread ran little was waiting on something else, not running */
static DWORD ft_run_ms(void) {DWORD in[2],o[8];in[0]=0xBD;in[1]=0x81;o[0]=0;o[2]=0;return ioctl(0x85500022UL,in,8,o,32)&&o[0]==0xBD?o[2]:0xFFFFFFFFUL;}
/* v10.432: this DLL's image (code, data, the core's packets and queue) locked in memory through the VxD while a Direct3D process holds it
 * (VX_IOCTL_LOCKMEM; the VxD releases a process's hold when its handle closes). Asked at each Direct3D context create; the last answer is
 * kept for VXD3DMON: pages locked, base, handles holding it, ACCEL_LOCK_HAL. */
DWORD vx_lock_result[4];static DWORD lock_pid[8]; /* the processes that asked (shared data; a reopened handle asks again only for them) */
static HANDLE dev_for_t(DWORD pid,int *temporary);
static int lock_wanted(DWORD pid) {int i;for(i=0;i<8;i++)if(lock_pid[i]==pid)return 1;return 0;}
/* v10.435: the request goes on this process's kept handle only (the VxD ties the hold to the handle): never on ioctl()'s temporary one
 * (all 8 slots in use; closed at once, which released the hold), and again on a handle that takes the slot's place (lock_again) */
static void lock_on(HANDLE h) {
 const BYTE *b=(const BYTE*)instance;DWORD pe,in[3],o[4],got=0;
 if(!b||h==INVALID_HANDLE_VALUE||*(const WORD*)b!=0x5A4DU)return;pe=*(const DWORD*)(b+0x3C);if(pe>4096||*(const DWORD*)(b+pe)!=0x4550UL)return;
 in[0]=1;in[1]=(DWORD)b;in[2]=*(const DWORD*)(b+pe+24+56); /* OptionalHeader.SizeOfImage */
 memset(o,0,sizeof(o));if(DeviceIoControl(h,0x85500028UL,in,12,o,16,&got,NULL)&&got==16){vx_lock_result[0]=o[0];vx_lock_result[1]=o[1];vx_lock_result[2]=o[2];vx_lock_result[3]=o[3];}
}
void VX_DD_lock_image(void) {
 int temp;HANDLE h=dev_for_t(GetCurrentProcessId(),&temp);
 if(temp){CloseHandle(h);return;}
 {DWORD me=GetCurrentProcessId();int i,f=-1;if(!lock_wanted(me)){for(i=0;i<8&&f<0;i++)if(!lock_pid[i])f=i;if(f<0)f=(int)(me&7);lock_pid[f]=me;}}
 lock_on(h);
}
DWORD VX_dd_cpms(void) {return ft_cpms;} /* v10.431: TSC cycles a millisecond (1000000 until calibrated) */
void VX_frame_add(DWORD cat,DWORD cycles) {if(cat<FT_CATS)ft_cur[cat]+=cycles>>8;if(!cat){ft_cnt[3]++;ft_dp2++;}} /* v10.386: DrawPrimitives2 calls counted */
/* v10.407 (board: up to 5 million Locks counted in a half-second frame): only the flipping process's calls are counted in the frame;
 * other processes' (VXDDRAW.DLL's data is shared) go to ft_other, reported apart */
static volatile DWORD ft_pid,ft_other[3];
static void ft_count(DWORD k) {if(k<3){if(!ft_pid||GetCurrentProcessId()==ft_pid)ft_cnt[k]++;else ft_other[k]++;}}
/* .511 (ACCEL_D3D_SAMPLE = 1, diagnostic, default 0): where a Direct3D game's frame goes, by sampling. A thread of the flipping process's own
 * suspends the thread that flips about a thousand times a second and keeps its EIP; every 600 flips C:\VXD3D.LOG gets the frames a second,
 * the share of each module (the game, D3DIM700.DLL, DDRAW.DLL, VXDDRAW.DLL, KERNEL32: the VxD calls; "(no module)": ring 0 or 16-bit code)
 * and VXDDRAW.DLL's busiest addresses (RVAs, 32 bytes a bucket, to be named from VXDDRAW.map). This DLL's data is shared by every process:
 * the sampler belongs to one process (dsm_pid); another takes it over only when the owner has stopped flipping for 10 seconds. */
extern DWORD vx_d3d_sample_on;
#define DSM_MAX 65536
static volatile DWORD dsm_pid,dsm_flips,dsm_tick,dsm_t0,dsm_n,dsm_stop,dsm_reports;static DWORD *dsm_eip;static HANDLE dsm_thread,dsm_target;
static void dsm_log(const char *fmt,...) {
 char line[300];DWORD w=0;int k;HANDLE f;va_list ap;
 va_start(ap,fmt);k=wvsprintfA(line,fmt,ap);va_end(ap);
 f=CreateFileA("C:\\VXD3D.LOG",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
 if(f==INVALID_HANDLE_VALUE)return;SetFilePointer(f,0,0,FILE_END);if(k>0)WriteFile(f,line,(DWORD)k,&w,0);CloseHandle(f);
}
static DWORD WINAPI dsm_main(LPVOID u) {
 CONTEXT cx;(void)u;
 while(!dsm_stop){
  Sleep(1);
  if(SuspendThread(dsm_target)==(DWORD)-1)break;
  memset(&cx,0,sizeof(cx));cx.ContextFlags=CONTEXT_CONTROL;
  if(GetThreadContext(dsm_target,&cx)&&dsm_n<DSM_MAX)dsm_eip[dsm_n++]=cx.Eip;
  ResumeThread(dsm_target);
 }
 return 0;
}
static void dsm_report(void) {
 static DWORD mb[24],mc[24];DWORD n=dsm_n,i,k,nm=0,other=0,base=(DWORD)instance,size=0,nb,*cnt,shown=0,inme=0,ms=GetTickCount()-dsm_t0;
 const BYTE *b=(const BYTE*)instance;
 if(!n)return;
 SuspendThread(dsm_thread);
 for(i=0;i<n;i++){MEMORY_BASIC_INFORMATION mi;DWORD a=0;
  if(VirtualQuery((LPCVOID)dsm_eip[i],&mi,sizeof(mi)))a=(DWORD)mi.AllocationBase;
  for(k=0;k<nm&&mb[k]!=a;k++);
  if(k==nm){if(nm<24){mb[nm]=a;mc[nm]=0;nm++;}else{other++;continue;}}
  mc[k]++;}
 dsm_reports++;
 dsm_log("D3D samples, report %lu (pid %08lX): %lu samples, %lu flips in %lu ms (%lu.%lu a second)\r\n",dsm_reports,GetCurrentProcessId(),n,dsm_flips,ms,ms?dsm_flips*1000/ms:0,ms?(dsm_flips*10000/ms)%10:0);
 for(k=0;k<nm;k++){char name[MAX_PATH];name[0]=0;if(mb[k]==base)lstrcpyA(name,"VXDDRAW.DLL (this driver: the HAL)");else if(!mb[k]||!GetModuleFileNameA((HMODULE)mb[k],name,sizeof(name)))lstrcpyA(name,"(no module)");
  else if(!lstrcmpiA(name+lstrlenA(name)-12,"KERNEL32.DLL"))lstrcatA(name," (with ring 0: the VxD calls, file and memory services)"); /* .512 */
  dsm_log("  %3lu.%lu%% %s (base %08lX)\r\n",mc[k]*100/n,(mc[k]*1000/n)%10,name,mb[k]);}
 if(other)dsm_log("  %3lu.%lu%% in more modules\r\n",other*100/n,(other*1000/n)%10);
 /* VXDDRAW.DLL's busiest 32-byte buckets (every one with 2 samples or more, up to 400) */
 if(*(const WORD*)b==0x5A4DU){DWORD pe=*(const DWORD*)(b+0x3C);if(pe<4096&&*(const DWORD*)(b+pe)==0x4550UL)size=*(const DWORD*)(b+pe+24+56);}
 nb=(size+31)/32;cnt=nb?(DWORD*)VirtualAlloc(0,nb*4,MEM_COMMIT,PAGE_READWRITE):0;
 if(cnt){
  for(i=0;i<n;i++){DWORD r=dsm_eip[i]-base;if(dsm_eip[i]>=base&&r<size){cnt[r>>5]++;inme++;}}
  for(k=0;k<400;k++){DWORD best=0,bi=0;for(i=0;i<nb;i++)if(cnt[i]>best){best=cnt[i];bi=i;}if(best<2)break;dsm_log("  VXDDRAW+%06lX %lu\r\n",bi<<5,best);shown+=best;cnt[bi]=0;}
  dsm_log("  (VXDDRAW: %lu samples, %lu in the lines above)\r\n",inme,shown);
  VirtualFree(cnt,0,MEM_RELEASE);}
 dsm_n=0;dsm_flips=0;dsm_t0=GetTickCount();ResumeThread(dsm_thread);
}
static void dsm_flip(void) {
 DWORD me=GetCurrentProcessId(),tick=GetTickCount(),id;
 if(!vx_d3d_sample_on)return;
 if(dsm_pid!=me){
  if(dsm_pid&&tick-dsm_tick<10000UL)return; /* another process samples */
  dsm_pid=me;dsm_thread=0;dsm_target=0;dsm_eip=0;dsm_n=0;dsm_flips=0;dsm_reports=0; /* (the old owner's buffer and handles were its own) */
 }
 dsm_tick=tick;
 if(!dsm_thread){
  if(!dsm_eip)dsm_eip=(DWORD*)VirtualAlloc(0,DSM_MAX*4,MEM_COMMIT,PAGE_READWRITE);if(!dsm_eip)return;
  if(!DuplicateHandle(GetCurrentProcess(),GetCurrentThread(),GetCurrentProcess(),&dsm_target,THREAD_SUSPEND_RESUME|THREAD_GET_CONTEXT|THREAD_QUERY_INFORMATION,FALSE,0)){dsm_log("D3D sampler: no thread handle\r\n");vx_d3d_sample_on=0;return;}
  dsm_stop=0;dsm_n=0;dsm_flips=0;dsm_t0=tick;dsm_thread=CreateThread(0,0,dsm_main,0,0,&id);
  if(dsm_thread)SetThreadPriority(dsm_thread,THREAD_PRIORITY_TIME_CRITICAL);else{CloseHandle(dsm_target);dsm_target=0;vx_d3d_sample_on=0;}
  dsm_log("D3D sampler (pid %08lX): %s\r\n",me,dsm_thread?"on":"not started");return;
 }
 if(++dsm_flips>=600)dsm_report();
}
static void ft_flip(void) {
 DWORD now=VX_dd_tsc(),tick=GetTickCount(),d,k,lo=0,d8,run=ft_run_ms(),rd;
 ft_pid=GetCurrentProcessId();
 dsm_flip(); /* .511 */
 rd=(run!=0xFFFFFFFFUL&&ft_run_last!=0xFFFFFFFFUL&&ft_last)?run-ft_run_last:0;ft_run_last=run;
 /* .533 (board, GTA3 on .532: VXD3DMON's HAL figures 24 times too big, "42 TSC cycles a microsecond"): a calibration outside 100 MHz to
  * 4 GHz is not taken; it starts again from this flip (the TSC's 32 bits and the tick were read where they do not belong together) */
 if(!ft_cal_ok){if(!ft_cal_t||tick-ft_cal_t>4000UL){ft_cal_t=tick;ft_cal_c=now;}else if(tick-ft_cal_t>=3000UL){DWORD c=(now-ft_cal_c)/(tick-ft_cal_t);
  if(c>=100000UL&&c<=4000000UL){ft_cpms=c;ft_cal_ok=1;}else{ft_cal_t=tick;ft_cal_c=now;}}}
 d=now-ft_last;
 if(!ft_last||tick-ft_last_tick>4000UL){ft_last=now;ft_last_tick=tick;for(k=0;k<FT_CATS;k++)ft_cur[k]=0;for(k=0;k<4;k++)ft_cnt[k]=0;return;} /* the first, or longer than the TSC's 32 bits hold */
 ft_last=now;ft_last_tick=tick;d8=d>>8;ft_frames++;ft_run_all+=rd;
 for(k=0;k<FT_CATS;k++)ft_all[k]+=ft_cur[k];ft_all[FT_CATS]+=d8;
 if(d>=75UL*ft_cpms){ft_long++;ft_run_long+=rd;for(k=0;k<FT_CATS;k++)ft_lsum[k]+=ft_cur[k];ft_lsum[FT_CATS]+=d8;}
 for(k=1;k<8;k++)if(ft_worst[k][0]<ft_worst[lo][0])lo=k;
 if(d8>ft_worst[lo][0]){ft_worst[lo][0]=d8;for(k=0;k<FT_CATS;k++)ft_worst[lo][1+k]=ft_cur[k];for(k=0;k<4;k++)ft_worst[lo][1+FT_CATS+k]=ft_cnt[k];ft_worst[lo][FT_CATS+5]=rd;}
 for(k=0;k<FT_CATS;k++)ft_cur[k]=0;for(k=0;k<4;k++)ft_cnt[k]=0;
}
/* out: [0] cycles per ms, [1] frames, [2] frames of 75 ms and more, [3-9] all frames' parts and their whole, [10-16] the long ones',
 * [17-96] the 8 longest (whole, 6 parts, Locks, Unlocks, creates); n | 0x80000000 clears after the copy */
/* v10.386: [17-104] 11 values a long frame (the DrawPrimitives2 calls last), [105] DrawPrimitives2 calls in all;
 * v10.389: 12 values a long frame (+ the thread's run ms), [17-112], [113] DrawPrimitives2 calls, [114] / [115] run ms in all / in long frames */
DWORD WINAPI VX_DD_QueryFrames(DWORD *out,DWORD n) {
 DWORD k,j,m=n&0x7FFFFFFFUL;
 if(!out||m<20+8*(FT_CATS+6))return 0;
 out[0]=ft_cpms;out[1]=ft_frames;out[2]=ft_long;
 for(k=0;k<=FT_CATS;k++){out[3+k]=ft_all[k];out[10+k]=ft_lsum[k];}
 for(k=0;k<8;k++)for(j=0;j<FT_CATS+6;j++)out[17+k*(FT_CATS+6)+j]=ft_worst[k][j];
 out[17+8*(FT_CATS+6)]=ft_dp2;out[18+8*(FT_CATS+6)]=ft_run_all;out[19+8*(FT_CATS+6)]=ft_run_long;
 if(m>=23+8*(FT_CATS+6))for(k=0;k<3;k++)out[20+8*(FT_CATS+6)+k]=ft_other[k]; /* v10.407 */
 if(n&0x80000000UL){for(k=0;k<3;k++)ft_other[k]=0;ft_frames=0;ft_long=0;ft_dp2=0;ft_run_all=0;ft_run_long=0;for(k=0;k<=FT_CATS;k++){ft_all[k]=0;ft_lsum[k]=0;}for(k=0;k<8;k++)for(j=0;j<FT_CATS+6;j++)ft_worst[k][j]=0;}
 return m>=23+8*(FT_CATS+6)?23+8*(FT_CATS+6):20+8*(FT_CATS+6);
}
/* v10.239: the frame buffer's linear address and size, for the OpenGL ICD (VXGL.DLL: a DirectDraw surface's offset = its lock address - base) */
DWORD WINAPI VX_DD_Vram(DWORD *linear,DWORD *size) {if(!hal||!linear||!size)return 0;*linear=hal->vramLinear;*size=hal->vramSize;return 1;}
/* v10.241: a surface's offset in video memory and pitch, from DirectDraw's own record of it (fpVidMem). A lock gives a different address on
 * Windows 98: DirectDraw hands out an alias of video memory (fpAliasedVidMem) so a lock can be held without the Win16 lock, and v10.240's
 * ICD, which took lock address - VX_DD_Vram's base, found its surfaces outside video memory (board: "back buffer not in usable video
 * memory") and drew everything with swrast. The argument is the application's IDirectDrawSurface interface (DDRAWI_DDRAWSURFACE_INT). */
DWORD WINAPI VX_DD_Surface(void *surface,DWORD *offset,DWORD *pitch) {
 LPDDRAWI_DDRAWSURFACE_INT i=(LPDDRAWI_DDRAWSURFACE_INT)surface;LPDDRAWI_DDRAWSURFACE_GBL g;
 if(!hal||!i||!offset||!pitch||IsBadReadPtr(i,sizeof(*i))||!i->lpLcl||IsBadReadPtr(i->lpLcl,sizeof(*i->lpLcl)))return 0;
 g=i->lpLcl->lpGbl;
 if(!g||IsBadReadPtr(g,sizeof(*g))||!(i->lpLcl->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY)||g->lPitch<=0)return 0;
 if(g->fpVidMem<hal->vramLinear||g->fpVidMem-hal->vramLinear>=hal->vramSize)return 0;
 /* v10.330 (review #155): the whole surface (pitch x height) inside video memory, not only its start; a FOURCC surface (DXT: lPitch is its
  * linear size; YUV) its lPitch bytes */
 {DWORD rows=((i->lpLcl->dwFlags&DDRAWISURF_HASPIXELFORMAT)&&(g->ddpfSurface.dwFlags&DDPF_FOURCC))?1UL:(DWORD)g->wHeight;
  if(!rows||rows>(hal->vramSize-(g->fpVidMem-hal->vramLinear))/(DWORD)g->lPitch)return 0;}
 *offset=g->fpVidMem-hal->vramLinear;*pitch=(DWORD)g->lPitch;return 1;
}
/* v10.139 (review F11): the DLL's data is shared by every process but a handle belongs to one: keep one handle per process (8 slots) instead of
 * forgetting the last process's handle each time another process calls in; each process closes its own at detach. */
#define DEV_SLOTS 8
static HANDLE dev_h[DEV_SLOTS]; static DWORD dev_pid[DEV_SLOTS];
static int dead_slot(void) {int i; for(i=0;i<DEV_SLOTS;i++) {DWORD code=0; HANDLE p=OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,dev_pid[i]); int dead=!p||!GetExitCodeProcess(p,&code)||code!=STILL_ACTIVE; if(p) CloseHandle(p); if(dead) return i;} return -1;}
static HANDLE dev_for_t(DWORD pid,int *temporary) {
 int i,free_i=-1;
 if(temporary) *temporary=0;
 for(i=0;i<DEV_SLOTS;i++) {if(dev_pid[i]==pid&&dev_h[i]&&dev_h[i]!=INVALID_HANDLE_VALUE) return dev_h[i]; if(free_i<0&&!dev_pid[i]) free_i=i;}
 {HANDLE h=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL); if(h==INVALID_HANDLE_VALUE) return h; if(free_i<0) free_i=dead_slot(); /* v10.140: full: reuse only a slot whose process has ended (its handle ended with it) */
  if(free_i<0) {if(temporary) *temporary=1; return h;} /* all 8 alive: not stored (v10.234: the caller closes it) */ dev_h[free_i]=h; dev_pid[free_i]=pid; return h;}
}
static HANDLE dev_for(DWORD pid) {return dev_for_t(pid,0);}
/* v10.322: at this process's attach, a slot under its ID is an ended process's (Windows 98 gives an ended process's ID again, v10.230): its
 * handle ended with that process; the slot is forgotten (not closed: the value is not a handle of this process) */
static void dev_attach(DWORD pid) {int i; for(i=0;i<DEV_SLOTS;i++) if(dev_pid[i]==pid) {dev_h[i]=NULL; dev_pid[i]=0;}}
static void dev_close(DWORD pid) {int i; for(i=0;i<DEV_SLOTS;i++) if(dev_pid[i]==pid) {if(dev_h[i]&&dev_h[i]!=INVALID_HANDLE_VALUE) CloseHandle(dev_h[i]); dev_h[i]=NULL; dev_pid[i]=0;}}
static DWORD pal_pid; /* v10.140: process that attached the active palette; only it puts the desktop palette back when it exits */
static DWORD flip_pid; /* process that moved the display start away from the desktop; it resets it on exit */
/* Also exported by the Win16 DRV; verification compares the binary constants. */
const DWORD VXDDABI[]={sizeof(VMDAHAL_t),offsetof(VMDAHAL_t,cb32),offsetof(VMDAHAL_t,modes),
 sizeof(DDHALINFO),sizeof(DDHALMODEINFO2_t),sizeof(DDPIXELFORMAT),sizeof(vx2d_request)};
/* v10.230: Windows 98 gives a new process the ID of one that has just ended (Tunnel run twice: the same ID), and a slot can outlive its
 * process; the second process then found the first one's handle under its own ID, every DeviceIoControl failed and each Lock and Blt
 * answered DDERR_SURFACELOST until reboot. A failed call is tried once more on a new handle (v10.319: which replaces the slot's only when
 * it works; see ioctl). */
/* v10.234: the handle comes from this process's slot on every call (the shared device/device_pid pair could be overwritten by another
 * process between two lines); the Direct3D HAL uses this too (VX_DD_call) instead of opening and closing a handle per call. */
static int ioctl(DWORD code,const void *in,DWORD n,void *out,DWORD z) {
 DWORD got=0,pid=GetCurrentProcessId();int temp;HANDLE h;BOOL ok;
 /* v10.58n: the DLL may sit in the shared arena (one copy for DDHELP and every application),
  * but file handles are per process. */
 BOOL called;
 h=dev_for_t(pid,&temp);
 called=h!=INVALID_HANDLE_VALUE&&DeviceIoControl(h,code,(void*)in,n,out,z,&got,NULL);ok=called&&got==z;
 if(temp) CloseHandle(h);
 if(ok) return 1;
 /* v10.367 (audit): the VxD ran the control and answered (a short reply): it is not sent again on a new handle (a ROP, a blit or a packet
  * would run twice); only a call DeviceIoControl refused is tried on a new handle */
 if(called){last_ioctl_error=0x20000UL|got;return 0;}
 /* v10.319 (audit H3): the call again on a new handle, the slot's kept until the new one works: when it does, the slot's was not this
  * process's (v10.230) and the new one takes its place; when it fails too, the call itself failed, the new handle is closed and the
  * slot's stays (it was dropped and a handle leaked at every failing call, a refused control code for one) */
 if(h==INVALID_HANDLE_VALUE||temp){last_ioctl_error=GetLastError()|(h==INVALID_HANDLE_VALUE?0x10000UL:0);return 0;}
 got=0;h=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(h==INVALID_HANDLE_VALUE){last_ioctl_error=GetLastError()|0x10000UL;return 0;}
 ok=DeviceIoControl(h,code,(void*)in,n,out,z,&got,NULL)&&got==z;
 if(!ok){last_ioctl_error=GetLastError();CloseHandle(h);return 0;}
 if(lock_wanted(pid)&&code!=0x85500028UL)lock_on(h); /* v10.435: the new handle holds the lock too before the old one's close drops its hold */
 /* v10.322 (review: five retries, six opens and no close): the slot's handle is this process's own (a slot left under this ID by an ended
  * process is forgotten when this process attaches, dev_attach), so it is closed as the new one takes its place */
 {int i;for(i=0;i<DEV_SLOTS;i++)if(dev_pid[i]==pid){HANDLE old=dev_h[i];dev_h[i]=h;if(old&&old!=INVALID_HANDLE_VALUE&&old!=h)CloseHandle(old);reopened++;return 1;}}
 CloseHandle(h);return 1; /* the slot went meanwhile: not kept */
}
int VX_DD_call(DWORD code,const void *in,DWORD n,void *out,DWORD z) {return ioctl(code,in,n,out,z);}
/* v10.229: access_why: the VxD's answer, or 80000000 | the DeviceIoControl error << 8 | 1 (handle missing: 2) when the call failed;
 * Lock and Unlock failures log it (C:\VXD3D.LOG, HAL event ring) in place of the surface address */
static DWORD access_why;
static DWORD access(DWORD action) {
 DWORD rc=VX2D_LOST;
 if(!ioctl(VX_IOCTL_2D_ACCESS,&action,4,&rc,4)) {access_why=0x80000000UL|((last_ioctl_error&0xFFFFUL)<<8)|((last_ioctl_error&0x10000UL)?2UL:1UL); return VX2D_LOST;}
 access_why=rc; return rc;
}
/* v10.295: the same wait, naming a depth surface's memory: the VxD stops trusting its Coarse Z records (vx_native3d_hw.c) */
static DWORD access_z(DWORD action,LPDDRAWI_DDRAWSURFACE_LCL s) {
 vx3dn_surface d;DWORD in[3],rc=VX2D_LOST;
 if(!s||!(s->ddsCaps.dwCaps&DDSCAPS_ZBUFFER)||!VX_D3D_surface(s,&d))return access(action);
 in[0]=action;in[1]=d.offset;in[2]=VX3DN_CZ_FORMAT(d.format)?VX3DN_CZ_END(&d):d.offset+d.pitch*d.height;
 if(!ioctl(VX_IOCTL_2D_ACCESS,in,12,&rc,4)) {access_why=0x80000000UL|((last_ioctl_error&0xFFFFUL)<<8)|((last_ioctl_error&0x10000UL)?2UL:1UL); return VX2D_LOST;}
 access_why=rc; return rc;
}
/* DirectDraw carries DriverInit's shared context in dwReserved3 (DX5+).
 * A callback may run in a process whose private DLL data was never initialized. */
static int vx_bind_hal(LPDDRAWI_DIRECTDRAW_GBL dd) {
 VMDAHAL_t *p;
 if(dd && dd->dwReserved3>=0x80000000UL) {
  p=(VMDAHAL_t*)dd->dwReserved3;
  if(p->dwSize!=sizeof(*p)) return 0;
  hal=p;
 }
 return hal&&hal->pFBHDA32;
}
static int format(const DDPIXELFORMAT *f,DWORD bpp) {
 if(bpp==8) return f->dwRGBBitCount==8&&f->dwFlags==(DDPF_RGB|DDPF_PALETTEINDEXED8)&&!f->dwRGBAlphaBitMask; /* v10.71 */
 return (bpp==16||bpp==32)&&f->dwRGBBitCount==bpp&&f->dwFlags==DDPF_RGB&&
  f->dwRBitMask==(bpp==16?0xF800UL:0xFF0000UL)&&
  f->dwGBitMask==(bpp==16?0x7E0UL:0xFF00UL)&&
  f->dwBBitMask==(bpp==16?0x1FUL:0xFFUL)&&!f->dwRGBAlphaBitMask;
}
extern uint32_t vx_d3d_feat;
extern void VX_D3D_trace2(DWORD cb,DWORD hr,DWORD why,DWORD a,DWORD b,DWORD c); /* v10.422 */
static int describe(LPDDRAWI_DDRAWSURFACE_LCL s,vx2d_surface *out) {
 LPDDRAWI_DDRAWSURFACE_GBL g; DWORD offset,limit,bytes;
 if(!s||!hal||!hal->pFBHDA32) return 0;
 g=s->lpGbl;
 if(!g||!(s->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY)||s->ddsCaps.dwCaps&(DDSCAPS_SYSTEMMEMORY|DDSCAPS_NONLOCALVIDMEM|DDSCAPS_OVERLAY|DDSCAPS_ZBUFFER)) return 0;
 /* v10.304 (review: a Blt from a video-memory texture to a plain surface was NOTHANDLED, DirectDraw's own code): a texture in the desktop's
  * pixel format (checked below) whose rows are a multiple of 16 bytes (the 2D engine's pitch unit; the sampler's rows are width x bytes,
  * so narrow textures stay with DirectDraw) is a 2D surface like any other (ACCEL_D3D_TEXBLT) */
 if((s->ddsCaps.dwCaps&DDSCAPS_TEXTURE)&&(!(vx_d3d_feat&VX_F_TEXBLT)||(g->lPitch&15))) return 0; /* v10.309 (review): DirectDraw sets no HASPIXELFORMAT on a surface in the
  * primary's own format, the textures this is for (v10.304 required it, so it never ran); other formats carry it and are refused below */
 if(s->dwFlags&DDRAWISURF_INVALID) return 0;
 if((s->dwFlags&DDRAWISURF_HASPIXELFORMAT)&&!format(&g->ddpfSurface,hal->pFBHDA32->bpp)) return 0;
 if(g->fpVidMem<hal->vramLinear||g->lPitch<=0) return 0;
 offset=g->fpVidMem-hal->vramLinear; limit=hal->vramSize;
 {DWORD pitch=(DWORD)g->lPitch,w=g->wWidth,h=g->wHeight;
  /* v10.422 (v10.423; board v10.422, WinTune GL -> D3D -> GL: every present of the second GL run refused by the VxD, check 0x14, and done on the CPU,
   * 32 ms a swap and banded): a primary made before another DirectDraw object changed the mode and changed it back keeps the size it
   * was given; the primary is the display, so its rows are the display's (the values the VxD checks a descriptor at offset 0 against).
   * The first disagreement is logged (cb 98: the record's pitch, width << 16 | height, the display's pitch, width << 16 | height). */
  if((s->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE)&&!offset&&hal->pFBHDA32->pitch&&hal->pFBHDA32->width&&hal->pFBHDA32->height&&
     (pitch!=hal->pFBHDA32->pitch||w!=hal->pFBHDA32->width||h!=hal->pFBHDA32->height)) {
   /* .552: trace2 takes six arguments. Put the old pitch in why so all
    * four geometry words fit its ABI (the old seven-argument call warned
    * and discarded the final word). */
   static int said;if(!said++)VX_D3D_trace2(98,0,pitch,w<<16|(h&0xFFFF),hal->pFBHDA32->pitch,hal->pFBHDA32->width<<16|(hal->pFBHDA32->height&0xFFFF));
   pitch=hal->pFBHDA32->pitch;w=hal->pFBHDA32->width;h=hal->pFBHDA32->height;
  }
 if(offset>=limit||!h||!w||pitch>0x3FF8UL||(pitch&15)||(offset&7)) return 0; /* v10.309 (review): 16-byte rows, as the VxD takes them (v10.277) */
 bytes=hal->pFBHDA32->bpp/8;
 if(w>pitch/bytes||h>(limit-offset)/pitch) return 0;
 out->offset=offset; out->pitch=pitch; out->width=w; out->height=h;}
 return 1;
}
static int rect(const RECT *r,const vx2d_surface *s) {
 return r->left>=0&&r->top>=0&&r->right>r->left&&r->bottom>r->top&&
  (DWORD)r->right<=s->width&&(DWORD)r->bottom<=s->height;
}
static DWORD result(DWORD rc,HRESULT *hr) {
 if(rc==VX2D_SKIP) return DDHAL_DRIVER_NOTHANDLED;
 *hr=rc==VX2D_DONE?DD_OK:DDERR_SURFACELOST;
 return DDHAL_DRIVER_HANDLED;
}
/* v10.58zb: CPU path for the same fill/copy request when the engine declines it (ACCEL_2D off, ACCEL_DDRAW off,
 * 3D busy, ...). A HAL that returns NOT HANDLED leaves DirectDraw to its own code, which cannot touch these video
 * surfaces (it failed with E_NOTIMPL), so the driver has to finish the blit itself. No CRT: plain loops.
 * v10.59: the same CPU path now also does what the engine cannot: stretching, mirroring and system-memory surfaces
 * (video <-> system, which DDCAPS_CANBLTSYSMEM promises). Stretch is point sampled at pixel centres. */
typedef struct {BYTE *p; DWORD pitch,w,h,off; int vram;} sview;
#include "vx_alpha.h"
typedef struct {sview d,s; DWORD op,flags,key,fg,bpp,alpha; RECT dr,sr; int mirx,miry,alpha_on;} swq;
/* v10.368 (audit N14): a system-memory surface is one block (DirectDraw's allocation): its first and last bytes are probed, not every page of it
 * (IsBadReadPtr / IsBadWritePtr touch each page: about 512 for a 1024 x 1024 16-bit sprite sheet, on every Blt) */
static int sys_ok(const BYTE *p,DWORD n,int write) {
 if(!p||!n) return 0;
 return write?!IsBadWritePtr((void*)p,1)&&!IsBadWritePtr((void*)(p+n-1),1):!IsBadReadPtr(p,1)&&!IsBadReadPtr(p+n-1,1);
}
static int view(LPDDRAWI_DDRAWSURFACE_LCL s,sview *v) {
 LPDDRAWI_DDRAWSURFACE_GBL g; vx2d_surface vs; DWORD caps,by;
 if(!s||!hal||!hal->pFBHDA32) return 0;
 g=s->lpGbl; if(!g) return 0;
 caps=s->ddsCaps.dwCaps;
 if(caps&DDSCAPS_VIDEOMEMORY) {
  if(!describe(s,&vs)) return 0;
  v->p=(BYTE*)(hal->vramLinear+vs.offset); v->pitch=vs.pitch; v->w=vs.width; v->h=vs.height; v->off=vs.offset; v->vram=1;
  return 1;
 }
 if(!(caps&DDSCAPS_SYSTEMMEMORY)||(caps&(DDSCAPS_OVERLAY|DDSCAPS_ZBUFFER|DDSCAPS_TEXTURE|DDSCAPS_NONLOCALVIDMEM))) return 0;
 if(s->dwFlags&DDRAWISURF_INVALID) return 0;
 by=hal->pFBHDA32->bpp/8;
 if((s->dwFlags&DDRAWISURF_HASPIXELFORMAT)&&!format(&g->ddpfSurface,hal->pFBHDA32->bpp)) return 0;
 if(!g->fpVidMem||g->lPitch<=0||!g->wWidth||!g->wHeight||(DWORD)g->lPitch>0x7FFF0UL||g->wWidth>(DWORD)g->lPitch/by||(DWORD)g->wHeight>0x40000000UL/(DWORD)g->lPitch) return 0; /* v10.139 (review F18): pitch x height stays far below 4 GB */
 v->p=(BYTE*)g->fpVidMem; v->pitch=(DWORD)g->lPitch; v->w=g->wWidth; v->h=g->wHeight; v->off=0; v->vram=0;
 return sys_ok(v->p,v->pitch*(v->h-1)+v->w*by,0);
}
/* v10.70: CPU reads of the video memory run at 30 MB/s with dword code and at 61 MB/s with 64-bit MMX loads (VX2READ, VIA C7, write-combining
 * mapping); a cached mapping is faster but not coherent with the 2D engine, so reads stay uncached and use MMX. MMX is used in a callee and the
 * x87 stack is empty across calls, so the application's floating-point state is not touched; the routine ends with EMMS. */
static void rep_movsd(void *d,const void *s,unsigned long n);
#pragma aux rep_movsd = "cld" "rep movsd" parm [edi] [esi] [ecx] modify exact [edi esi ecx];
static void mmx_copy(void *d,const void *s,unsigned long blocks);
#pragma aux mmx_copy = 0x0F 0x6F 0x06 0x0F 0x6F 0x4E 0x08 0x0F 0x6F 0x56 0x10 0x0F 0x6F 0x5E 0x18 \
 0x0F 0x7F 0x07 0x0F 0x7F 0x4F 0x08 0x0F 0x7F 0x57 0x10 0x0F 0x7F 0x5F 0x18 \
 0x83 0xC6 0x20 0x83 0xC7 0x20 0x49 0x75 0xD9 0x0F 0x77 parm [edi] [esi] [ecx] modify exact [edi esi ecx];
static unsigned long cpuid_present(void);
#pragma aux cpuid_present = 0x9C 0x58 0x89 0xC1 0x35 0x00 0x00 0x20 0x00 0x50 0x9D 0x9C 0x58 0x31 0xC8 0x25 0x00 0x00 0x20 0x00 0x51 0x9D value [eax] modify exact [eax ecx];
static void cpuid_leaf1(unsigned long *out);
#pragma aux cpuid_leaf1 = 0x53 0xB8 0x01 0x00 0x00 0x00 0x0F 0xA2 0x89 0x06 0x89 0x5E 0x04 0x89 0x4E 0x08 0x89 0x56 0x0C 0x5B parm [esi] modify exact [eax ecx edx];
static int has_mmx(void) {
 static int known,ok;
 if(!known) {unsigned long r[4]; known=1; if(cpuid_present()) {cpuid_leaf1(r); ok=(r[3]>>23)&1;}}
 return ok;
}
/* copy n bytes forward (the caller walks backwards itself when it must); src_vram selects MMX for the reads */
static void fast_copy(BYTE *d,const BYTE *s,DWORD n,int src_vram) {
 if(src_vram&&n>=32&&has_mmx()) {DWORD b=n>>5; mmx_copy(d,s,b); d+=b<<5; s+=b<<5; n&=31;}
 if(n>=4) {DWORD q=n>>2; rep_movsd(d,s,q); d+=q<<2; s+=q<<2; n&=3;}
 while(n--) *d++=*s++;
}
/* one pixel of 1, 2 or 4 bytes (8bpp indices, 16bpp, 32bpp) */
#define PXL(p,i) (by==4?((DWORD*)(p))[i]:by==2?(DWORD)((WORD*)(p))[i]:(DWORD)((BYTE*)(p))[i])
#define PXS(p,i,v) do{if(by==4)((DWORD*)(p))[i]=(v);else if(by==2)((WORD*)(p))[i]=(WORD)(v);else((BYTE*)(p))[i]=(BYTE)(v);}while(0)
/* destination rectangle [x0,x1) x [y0,y1) inside q->dr */
static void sw_rect(const swq *q,LONG x0,LONG y0,LONG x1,LONG y1) {
 DWORD by=q->bpp/8,mask=q->bpp==8?0xFFUL:q->bpp==16?0xFFFFUL:0xFFFFFFFFUL,kmask=q->bpp==32?0x00FFFFFFUL:mask,key=q->key&kmask,fg=q->fg&mask; /* v10.139 (review F07): at 32bpp the engine compares keys without the top byte; so does the CPU path now */
 LONG dw=q->dr.right-q->dr.left,dh=q->dr.bottom-q->dr.top,sw=q->sr.right-q->sr.left,sh=q->sr.bottom-q->sr.top;
 LONG w=x1-x0,h=y1-y0,r,c; BYTE *d,*sp; DWORD sv,dv;
 if(w<=0||h<=0) return;
 if(q->op==VX2D_FILL) {
  for(r=0;r<h;r++) {
   d=q->d.p+(DWORD)(y0+r)*q->d.pitch+(DWORD)x0*by;
   if(by==4) for(c=0;c<w;c++) ((DWORD*)d)[c]=fg; else if(by==2) for(c=0;c<w;c++) ((WORD*)d)[c]=(WORD)fg; else memset(d,(int)fg,(size_t)w);
  }
  return;
 }
 if(dw==sw&&dh==sh&&!q->mirx&&!q->miry) {
  /* 1:1 copy: walk backwards when source and destination are the same surface and the destination is later */
  LONG sx0=q->sr.left+(x0-q->dr.left),sy0=q->sr.top+(y0-q->dr.top),r0,r1,rs,c0,c1,cs; int back;
  back=q->s.p==q->d.p&&(sy0<y0||(sy0==y0&&sx0<x0));
  if(back) {r0=h-1; r1=-1; rs=-1; c0=w-1; c1=-1; cs=-1;} else {r0=0; r1=h; rs=1; c0=0; c1=w; cs=1;}
  for(r=r0;r!=r1;r+=rs) {
   d=q->d.p+(DWORD)(y0+r)*q->d.pitch+(DWORD)x0*by;
   sp=q->s.p+(DWORD)(sy0+r)*q->s.pitch+(DWORD)sx0*by;
   if(!q->alpha_on&&!(q->flags&(VX2D_SRC_KEY|VX2D_DST_KEY))&&!back) {
    fast_copy(d,sp,(DWORD)w*by,q->s.vram); /* v10.70 */
    continue;
   }
   for(c=c0;c!=c1;c+=cs) {
    sv=PXL(sp,c);
    if(q->flags&VX2D_SRC_KEY) if((sv&kmask)==key) continue;
    if(q->flags&VX2D_DST_KEY) {dv=PXL(d,c); if((dv&kmask)!=key) continue;}
    if(q->alpha_on)sv=vx_alpha_pixel(sv,PXL(d,c),q->alpha,q->bpp);
    PXS(d,c,sv);
   }
  }
  return;
 }
 {
  /* stretch and/or mirror: source index of destination pixel n is ((2n+1)*src)/(2*dst), counted from the far edge when mirrored.
   * The index advances without a division per pixel. */
  DWORD den_x=2UL*(DWORD)dw,den_y=2UL*(DWORD)dh,sqx=(2UL*(DWORD)sw)/den_x,srx=(2UL*(DWORD)sw)%den_x,sqy=(2UL*(DWORD)sh)/den_y,sry=(2UL*(DWORD)sh)%den_y;
  /* v10.285 (review): the starting numerators (2n+1)*src in 64 bits: a tall clipped stretch overflowed 32 bits (row 60001 of a 65535-row
   * source read as 27232); the per-pixel steps stay 32-bit (quotient and remainder below the denominators) */
  unsigned __int64 n64=(unsigned __int64)(2UL*(DWORD)(y0-q->dr.top)+1)*(DWORD)sh;
  DWORD yq=(DWORD)(n64/den_y),yr=(DWORD)(n64%den_y),xq,xr,sy,sx,xq0,xr0;
  n64=(unsigned __int64)(2UL*(DWORD)(x0-q->dr.left)+1)*(DWORD)sw; xq0=(DWORD)(n64/den_x); xr0=(DWORD)(n64%den_x); /* v10.367 (audit): the same for every row */
  for(r=0;r<h;r++) {
   sy=q->miry?(DWORD)sh-1-yq:yq;
   d=q->d.p+(DWORD)(y0+r)*q->d.pitch+(DWORD)x0*by;
   sp=q->s.p+(DWORD)(q->sr.top+(LONG)sy)*q->s.pitch+(DWORD)q->sr.left*by;
   xq=xq0; xr=xr0;
   for(c=0;c<w;c++) {
    sx=q->mirx?(DWORD)sw-1-xq:xq;
    sv=PXL(sp,sx);
    xq+=sqx; xr+=srx; if(xr>=den_x) {xr-=den_x; xq++;}
    if(q->flags&VX2D_SRC_KEY) if((sv&kmask)==key) continue;
    if(q->flags&VX2D_DST_KEY) {dv=PXL(d,c); if((dv&kmask)!=key) continue;}
    if(q->alpha_on)sv=vx_alpha_pixel(sv,PXL(d,c),q->alpha,q->bpp);
    PXS(d,c,sv);
   }
   yq+=sqy; yr+=sry; if(yr>=den_y) {yr-=den_y; yq++;}
  }
 }
}
/* v10.368 (audits: a GlobalAlloc and GlobalFree each stretch): the staging block is kept for the next blit when it is 1 MB or less
 * (the process's own memory, as before; a larger one is freed after the blit) */
static int overlap(const swq *q);
/* v10.465 (review): the DLL's data is shared by every process (fixlink -shared) and a GlobalAlloc block is the allocating process's
 * own memory: one kept block let another process's stretched Blt write at an address of the first one's. Kept per process (8 slots,
 * by process id); a process without a slot gets a block for the one blit. */
#define STAGE_SLOTS 8
static HGLOBAL stage_hs[STAGE_SLOTS]; static DWORD stage_ns[STAGE_SLOTS],stage_pid[STAGE_SLOTS];
/* *slot: the slot used, -1 when the block is this blit's alone (no slot free); nothing but the slot's own entries is shared */
static BYTE *stage_get(DWORD n,int *slot) {
 DWORD pid=GetCurrentProcessId();int i,f=-1;HGLOBAL h;
 for(i=0;i<STAGE_SLOTS;i++){if(stage_pid[i]==pid)break;if(f<0&&!stage_pid[i])f=i;}
 if(i==STAGE_SLOTS){
  if(f<0){HANDLE p;DWORD code;for(i=0;i<STAGE_SLOTS;i++){p=OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,stage_pid[i]);
    code=0;if(!p||!GetExitCodeProcess(p,&code)||code!=STILL_ACTIVE){if(p)CloseHandle(p);f=i;break;}CloseHandle(p);}} /* a slot of a process that ended (its memory went with it) */
  if(f<0){*slot=-1;return (BYTE*)GlobalAlloc(GMEM_FIXED,n);}
  i=f;stage_hs[i]=NULL;stage_ns[i]=0;stage_pid[i]=pid;}
 *slot=i;
 if(!(stage_hs[i]&&stage_ns[i]>=n)){if(stage_hs[i]){GlobalFree(stage_hs[i]);stage_hs[i]=NULL;stage_ns[i]=0;}
  if((h=GlobalAlloc(GMEM_FIXED,n))!=NULL){stage_hs[i]=h;stage_ns[i]=n;}}
 return (BYTE*)stage_hs[i];
}
static void stage_done(int slot,BYTE *buf) { /* the slot's block is kept when it is 1 MB or less */
 if(slot<0){if(buf)GlobalFree((HGLOBAL)buf);return;}
 if(stage_pid[slot]==GetCurrentProcessId()&&stage_hs[slot]&&stage_ns[slot]>0x100000UL){GlobalFree(stage_hs[slot]);stage_hs[slot]=NULL;stage_ns[slot]=0;}
}
/* 1 drawn; 0 nothing drawn: the source had to be staged (it overlaps the destination and is stretched, mirrored or copied in several
 * rectangles) and no memory could be had */
static DWORD blt_nh_line,blt_sw; /* v10.387: a slow Blt's path: the NOTHANDLED line, the CPU blits */
static int sw_blit_impl(LPDDHAL_BLTDATA p,const swq *q0,DWORD first,DWORD count);
static int sw_blit(LPDDHAL_BLTDATA p,const swq *q0,DWORD first,DWORD count) {blt_sw++;return sw_blit_impl(p,q0,first,count);}
static int sw_blit_impl(LPDDHAL_BLTDATA p,const swq *q0,DWORD first,DWORD count) {
 DWORD i; swq w=*q0; const swq *q=&w; int prim=(w.d.vram&&!w.d.off)||(w.op==VX2D_COPY&&w.s.vram&&!w.s.off),staged=0,must,sslot=-1;BYTE *sbuf=NULL;
 int scaled=w.sr.right-w.sr.left!=w.dr.right-w.dr.left||w.sr.bottom-w.sr.top!=w.dr.bottom-w.dr.top||w.mirx||w.miry;
 /* v10.368 (audit N6): a source that overlaps the destination is read whole first when the blit is not a plain 1:1 copy of one
  * rectangle (sw_rect walks those backwards): a stretch or mirror, or several clip rectangles, would read pixels already written */
 must=w.op==VX2D_COPY&&overlap(&w)&&(scaled||count-first>1);
 if(prim) (void)access(1);
 /* v10.59: video memory reads cost ~30 MB/s and a stretch reads each source pixel several times: read the source rectangle once
  * (sequentially) into system memory and stretch from there. */
 if(w.op==VX2D_COPY&&(must||(w.s.vram&&scaled))) {
  DWORD by=w.bpp/8,rw=(DWORD)(w.sr.right-w.sr.left),rh=(DWORD)(w.sr.bottom-w.sr.top),y;BYTE *buf;
  if(rw&&rh&&by&&rw<=0x800000UL/by/rh&&(buf=stage_get(rw*by*rh,&sslot))!=NULL) { /* v10.330 (review #52): the size checked by division (the product could wrap) */
   for(y=0;y<rh;y++) {
    BYTE *sp=w.s.p+(DWORD)(w.sr.top+(LONG)y)*w.s.pitch+(DWORD)w.sr.left*by;
    fast_copy(buf+y*rw*by,sp,rw*by,w.s.vram); /* v10.70: MMX reads of the video memory */
   }
   w.s.p=buf; w.s.pitch=rw*by; w.s.vram=0; w.sr.right-=w.sr.left; w.sr.left=0; w.sr.bottom-=w.sr.top; w.sr.top=0; staged=1; sbuf=buf;
  }
 }
 if(must&&!staged) {if(prim) (void)access(2); return 0;}
 for(i=first;i<count;i++) {
  if(p->IsClipped) sw_rect(q,p->prDestRects[i].left,p->prDestRects[i].top,p->prDestRects[i].right,p->prDestRects[i].bottom);
  else sw_rect(q,p->rDest.left,p->rDest.top,p->rDest.right,p->rDest.bottom);
 }
 if(staged) stage_done(sslot,sbuf);
 if(prim) (void)access(2);
 return 1;
}
#define nh(line) (blt_nh_line=(line),DDHAL_DRIVER_NOTHANDLED)
static int overlap(const swq *q) { /* source and destination rectangles of one surface intersect */
 return q->s.p==q->d.p&&q->sr.left<q->dr.right&&q->dr.left<q->sr.right&&q->sr.top<q->dr.bottom&&q->dr.top<q->sr.bottom;
}
static int rectv(const RECT *r,const sview *s) {
 return r->left>=0&&r->top>=0&&r->right>r->left&&r->bottom>r->top&&(DWORD)r->right<=s->w&&(DWORD)r->bottom<=s->h;
}
static int rop_us(DWORD r) {int i; for(i=0;i<8;i++) if(((r>>i)&1)!=((r>>(i^2))&1)) return 1; return 0;}
static int rop_up(DWORD r) {int i; for(i=0;i<8;i++) if(((r>>i)&1)!=((r>>(i^4))&1)) return 1; return 0;}
/* v10.146: what the last keyed blit was handed (VX2DKEY reads it through GetProcAddress): [0] count, [1] dwFlags, [2] destination LCL dwFlags, [3]/[4] its ddckCKDestBlt,
 * [5]/[6] its ddckCKSrcBlt, [7]/[8] bltFX.ddckDestColorkey, [9]/[10] bltFX.ddckSrcColorkey, [11] the key used, [12] destination LCL address, [13] source LCL address,
 * [14]/[15] the source LCL's ddckCKSrcBlt */
DWORD VXDBGKEY[24];
DWORD vx_heap_round=1; /* v10.179: registry ACCEL_HEAP_ALIGN through the VxD (vx_d3d_hal.c d3d_registry) */
/* v10.333 (VXPROBE ovkey, review #157): the overlay colour keys applications hand the HAL, kept only while VXOVKEYON is nonzero (VXPROBE
 * sets it through GetProcAddress; off by default). VXOVKEY[0] the count; entry k (k = count - 1 mod 16) at 1 + 6k: kind (1 UpdateOverlay
 * DDOVER_KEYDESTOVERRIDE, 2 UpdateOverlay DDOVER_KEYDEST (the surface's key), 3 SetColorKey DDCKEY_DESTOVERLAY, 4 UpdateOverlay source key),
 * the call's flags, the key's low and high values, the HAL's answer (ddRVal), the process id */
DWORD VXOVKEY[1+16*6],VXOVKEYON;
static void ovkey_note(DWORD kind,DWORD flags,const DDCOLORKEY *k,HRESULT hr) {
 DWORD *e;
 if(!VXOVKEYON||!k)return;
 e=VXOVKEY+1+(VXOVKEY[0]%16)*6;e[0]=kind;e[1]=flags;e[2]=k->dwColorSpaceLowValue;e[3]=k->dwColorSpaceHighValue;e[4]=(DWORD)hr;e[5]=GetCurrentProcessId();VXOVKEY[0]++;
}
DWORD VXDBGKEYON; /* v10.148: the record is kept only while this is nonzero (VX2DKEY sets it through GetProcAddress); off by default */ /* v10.147: [16] bltFX.dwSize, [17]/[18] destination LCL ddckCKDestOverlay, [19]/[20] source LCL dwFlags / ddckCKSrcOverlay low, [21] bltFX.dwDDFX */
/* v10.169: blits into a video-memory texture (DirectX 7 texture loads: system-memory copy -> video texture, colour fills). Textures are 32-bit
 * whatever the desktop depth, outside the 2D engine's surfaces, and DirectDraw's own code cannot reach video memory (E_NOTIMPL), so the CPU path
 * does them with 4-byte pixels. A source is a video texture or any 32-bit system-memory surface of the same pixel layout. */
static int tex_view(LPDDRAWI_DDRAWSURFACE_LCL s,sview *v,int dest,uint32_t *raw) {
 LPDDRAWI_DDRAWSURFACE_GBL g; vx3dn_surface d; uint32_t f,by; const DDPIXELFORMAT *pf;
 if(!s||!(g=s->lpGbl)||(s->dwFlags&DDRAWISURF_INVALID)) return 0;
 pf=(s->dwFlags&DDRAWISURF_HASPIXELFORMAT)?&g->ddpfSurface:&hal->ddpf;
 if(!VX_D3D_format(pf,&f)||!VX_D3D_texture_format(f)) return 0;
 *raw=f;by=VX_D3D_format_bytes(f);
 if(s->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY) {
  if(!(s->ddsCaps.dwCaps&DDSCAPS_TEXTURE)||!VX_D3D_surface(s,&d)) return 0;
  v->p=(BYTE*)(hal->vramLinear+d.offset); v->pitch=d.pitch; v->w=d.width; v->h=d.height; v->off=d.offset; v->vram=1; return 1;
 }
 if(dest||!(s->ddsCaps.dwCaps&DDSCAPS_SYSTEMMEMORY)) return 0;
 if(!g->fpVidMem||g->lPitch<=0||!g->wWidth||!g->wHeight||(DWORD)g->lPitch>0x7FFF0UL||g->wWidth>(DWORD)g->lPitch/by||(DWORD)g->wHeight>0x40000000UL/(DWORD)g->lPitch) return 0;
 v->p=(BYTE*)g->fpVidMem; v->pitch=(DWORD)g->lPitch; v->w=g->wWidth; v->h=g->wHeight; v->off=0; v->vram=0;
 return sys_ok(v->p,v->pitch*(v->h-1)+v->w*by,0);
}
/* v10.200: the sampler steps rows at least 32 bytes apart. A DXT surface is written as packed block rows (its lPitch is the linear size),
 * so block rows narrower than 32 bytes (DXT1 up to 8 wide, DXT3/5 up to 4 wide) are spread out to 32 bytes on Unlock and Blt and packed
 * again on Lock; dwReserved1 (the driver's) marks a spread surface. The block is allocated at the spread size. */
#define VX_SPREAD 0x58444E56UL
static int dxt_rows(LPDDRAWI_DDRAWSURFACE_LCL s,DWORD *packed,DWORD *padded,DWORD *rows) {
 LPDDRAWI_DDRAWSURFACE_GBL g;uint32_t f;
 if(!s||!(g=s->lpGbl)||!g->fpVidMem||!(s->ddsCaps.dwCaps&DDSCAPS_TEXTURE)||!(s->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY)||!(s->dwFlags&DDRAWISURF_HASPIXELFORMAT)||
    !VX_D3D_format(&g->ddpfSurface,&f)||!VX_RAW_IS_DXT(f))return 0;
 *packed=VX_D3D_texture_packed(f,g->wWidth);*padded=VX_D3D_texture_pitch(f,g->wWidth);*rows=VX_D3D_texture_rows(f,g->wHeight);
 return *packed<*padded&&*rows>1;
}
/* v10.310 (board, VXDX7 v2.79): the sampler finds a DXT chain's levels itself, each right after the one before at its row size (rows at
 * least 32 bytes) times its block rows, from level 0, whatever the level offsets sent (block 10030000): with every level in its own
 * 256-byte-aligned block, levels 2 and 3 of a 32x32 DXT1 chain were read from level 1's unused bytes (painted magenta and dark green there:
 * +280 and +2C0 from level 0, the 32-byte row rule; packed by linear sizes level 3 would have been at +2A0). So a DXT chain's top level is
 * given a block that holds every level that way (CreateSurface, ACCEL_D3D_DXTCHAIN), and each lower level, which keeps its own block as
 * the CPU's copy (packed, as written), is copied there spread to the sampler's rows on Unlock and Blt. The top level is spread in place,
 * as before. The level offsets the core sends stay those of the levels' own blocks (the sampler does not take them for DXT). */
#define VX_DXTCHAIN 0x4E484358UL /* the surface's dwReserved1 (local): the top level of a DXT chain whose block holds every level */
static DWORD dxt_level_bytes(uint32_t f,DWORD w,DWORD h) {if(!w)w=1;if(!h)h=1;return VX_D3D_texture_pitch(f,w)*VX_D3D_texture_rows(f,h);}
static LPDDRAWI_DDRAWSURFACE_LCL dxt_chain_top(LPDDRAWI_DDRAWSURFACE_LCL s,DWORD *level) {
 DWORD n=0;
 while(s&&(s->ddsCaps.dwCaps&DDSCAPS_MIPMAP)&&s->lpAttachListFrom&&s->lpAttachListFrom->lpAttached&&n<16){s=s->lpAttachListFrom->lpAttached;n++;}
 if(!s||s->dwReserved1!=VX_DXTCHAIN)return 0;
 *level=n;return s;
}
/* 1: s is a lower level of a marked chain (copied to where the sampler reads it, never spread in place); 0: anything else */
static int dxt_publish(LPDDRAWI_DDRAWSURFACE_LCL s) {
 LPDDRAWI_DDRAWSURFACE_LCL t;LPDDRAWI_DDRAWSURFACE_GBL g,tg;DWORD n=0,k,off=0,a,b,rows,r,w,h;uint32_t f;BYTE *d;const BYTE *src;
 if(!s||!(g=s->lpGbl)||!(t=dxt_chain_top(s,&n))||!n)return 0;
 tg=t->lpGbl;if(!tg||!tg->fpVidMem||!g->fpVidMem||!VX_D3D_format(&tg->ddpfSurface,&f)||!VX_RAW_IS_DXT(f))return 1;
 w=tg->wWidth>>n;h=tg->wHeight>>n;if(!w)w=1;if(!h)h=1;if(g->wWidth!=w||g->wHeight!=h)return 1;
 for(k=0;k<n;k++)off+=dxt_level_bytes(f,tg->wWidth>>k,tg->wHeight>>k);
 a=VX_D3D_texture_packed(f,w);b=VX_D3D_texture_pitch(f,w);rows=VX_D3D_texture_rows(f,h);
 d=(BYTE*)tg->fpVidMem+off;src=(const BYTE*)g->fpVidMem;
 for(r=0;r<rows;r++){memcpy(d+r*b,src+r*a,a);if(b>a)memset(d+r*b+a,0,b-a);}
 return 1;
}
static void dxt_spread(LPDDRAWI_DDRAWSURFACE_LCL s) {
 DWORD a,b,n,r;BYTE *m;
 if(dxt_publish(s))return; /* v10.310 */
 if(!dxt_rows(s,&a,&b,&n)||s->lpGbl->dwReserved1==VX_SPREAD)return;
 m=(BYTE*)s->lpGbl->fpVidMem;for(r=n;r-->1;){memmove(m+r*b,m+r*a,a);memset(m+r*b+a,0,b-a);}memset(m+a,0,b-a);
 s->lpGbl->dwReserved1=VX_SPREAD;
}
static void dxt_pack(LPDDRAWI_DDRAWSURFACE_LCL s) {
 DWORD a,b,n,r;BYTE *m;
 if(!dxt_rows(s,&a,&b,&n)||s->lpGbl->dwReserved1!=VX_SPREAD)return;
 m=(BYTE*)s->lpGbl->fpVidMem;for(r=1;r<n;r++)memmove(m+r*a,m+r*b,a);
 s->lpGbl->dwReserved1=0;
}
/* v10.245: a D24S8 depth buffer's stencil lives in its own plane after the Z words (vx_native3d.h VX3DN_SPLANE_*), as on XP; the engine
 * never looks at the Z words' low byte. A CPU lock still sees standard D24S8: Lock copies the plane into the low bytes, Unlock copies them
 * back (not after a read-only lock). A surface the OpenGL ICD reads and writes through the plane itself (VX_DD_StencilPlane) is left alone. */
#define VX_SPL_OWNED 0x4E4C5053UL /* the ICD handles the plane */
#define VX_SPL_RO 0x4F524C53UL    /* locked read-only: nothing to copy back */
static int splane(LPDDRAWI_DDRAWSURFACE_LCL s,vx3dn_surface *d) {
 return s&&s->lpGbl&&(s->ddsCaps.dwCaps&DDSCAPS_ZBUFFER)&&VX_D3D_surface(s,d)&&d->format==VX3DN_D24S8;
}
/* v10.246: the plane is tiled (vx_native3d.h VX3DN_SPLANE_AT); 4 pixels from a multiple of 4 are one dword of a tile row */
static void splane_merge(LPDDRAWI_DDRAWSURFACE_LCL s) {
 vx3dn_surface d;DWORD x,y,sp;const BYTE *pl;BYTE *z;
 if(!splane(s,&d)||s->lpGbl->dwReserved1==VX_SPL_OWNED)return;
 pl=(const BYTE*)(hal->vramLinear+VX3DN_SPLANE_OFF(&d));z=(BYTE*)(hal->vramLinear+d.offset);sp=VX3DN_SPLANE_PITCH(d.width);
 for(y=0;y<d.height;y++){BYTE *r=z+y*d.pitch;
  for(x=0;x<d.width;x+=4){DWORD v=*(const DWORD*)(pl+VX3DN_SPLANE_AT(sp,x,y)),k;for(k=0;k<4&&x+k<d.width;k++)r[(x+k)*4]=(BYTE)(v>>(8*k));}}
}
static void splane_split(LPDDRAWI_DDRAWSURFACE_LCL s) {
 vx3dn_surface d;DWORD x,y,sp;BYTE *pl;const BYTE *z;
 if(!splane(s,&d)||s->lpGbl->dwReserved1==VX_SPL_OWNED)return;
 pl=(BYTE*)(hal->vramLinear+VX3DN_SPLANE_OFF(&d));z=(const BYTE*)(hal->vramLinear+d.offset);sp=VX3DN_SPLANE_PITCH(d.width);
 for(y=0;y<d.height;y++){const DWORD *r=(const DWORD*)(z+y*d.pitch);
  for(x=0;x<d.width;x+=4){DWORD v=0,k;for(k=0;k<4&&x+k<d.width;k++)v|=(r[x+k]&255UL)<<(8*k);*(DWORD*)(pl+VX3DN_SPLANE_AT(sp,x,y))=v;}}
}
/* v10.245: for VXGL.DLL: the stencil plane's offset and pitch; from then on the HAL's Lock and Unlock leave the surface's plane to the
 * caller. The argument is the application's IDirectDrawSurface interface, as VX_DD_Surface. */
DWORD WINAPI VX_DD_StencilPlane(void *surface,DWORD *offset,DWORD *pitch) {
 LPDDRAWI_DDRAWSURFACE_INT i=(LPDDRAWI_DDRAWSURFACE_INT)surface;vx3dn_surface d;
 if(!hal||!i||!offset||!pitch||IsBadReadPtr(i,sizeof(*i))||!i->lpLcl||IsBadReadPtr(i->lpLcl,sizeof(*i->lpLcl))||
    !i->lpLcl->lpGbl||IsBadReadPtr(i->lpLcl->lpGbl,sizeof(*i->lpLcl->lpGbl))||!splane(i->lpLcl,&d))return 0;
 i->lpLcl->lpGbl->dwReserved1=VX_SPL_OWNED;*offset=VX3DN_SPLANE_OFF(&d);*pitch=VX3DN_SPLANE_PITCH(d.width);return 1;
}
/* v10.199: a DXT texture load: the whole surface, same format and size, copied as its linear bytes (DXT sub-rectangles, fills, keys and
 * effects are refused) */
static int dxt_blit(LPDDHAL_BLTDATA p) {
 LPDDRAWI_DDRAWSURFACE_LCL d=p->lpDDDestSurface,s=p->lpDDSrcSurface;LPDDRAWI_DDRAWSURFACE_GBL dg,sg;uint32_t df,sf;DWORD bytes;vx3dn_surface dv;
 const DDPIXELFORMAT *dp,*sp;
 if(!d||!(dg=d->lpGbl)||!(d->dwFlags&DDRAWISURF_HASPIXELFORMAT))return 0;
 dp=&dg->ddpfSurface;if(!VX_D3D_format(dp,&df)||!VX_RAW_IS_DXT(df))return 0;
 p->ddRVal=DDERR_UNSUPPORTED;
 if(!s||!(sg=s->lpGbl)||!(s->dwFlags&DDRAWISURF_HASPIXELFORMAT)||(p->dwFlags&~(DDBLT_WAIT|DDBLT_ASYNC|DDBLT_ROP))||((p->dwFlags&DDBLT_ROP)&&p->bltFX.dwROP!=SRCCOPY))return 1;
 sp=&sg->ddpfSurface;if(!VX_D3D_format(sp,&sf)||sf!=df||sg->wWidth!=dg->wWidth||sg->wHeight!=dg->wHeight||!VX_D3D_surface(d,&dv))return 1;
 if(p->rDest.left||p->rDest.top||(DWORD)p->rDest.right!=dg->wWidth||(DWORD)p->rDest.bottom!=dg->wHeight||
    p->rSrc.left||p->rSrc.top||(DWORD)p->rSrc.right!=sg->wWidth||(DWORD)p->rSrc.bottom!=sg->wHeight)return 1;
 /* v10.319 (audit H1): the clip list: none drawn (DD_OK, nothing touched), the whole surface (as unclipped), anything else refused (a DXT
  * copy is whole surfaces only); the whole surface was copied whatever the list held */
 if(p->IsClipped){if(!p->dwRectCnt){p->ddRVal=DD_OK;return 1;}
  if(p->dwRectCnt!=1||!p->prDestRects||p->prDestRects[0].left>0||p->prDestRects[0].top>0||p->prDestRects[0].right<(LONG)dg->wWidth||p->prDestRects[0].bottom<(LONG)dg->wHeight)return 1;}
 bytes=VX_D3D_texture_packed(df,dg->wWidth)*VX_D3D_texture_rows(df,dg->wHeight);
 /* v10.288 (review): a video-memory source may be spread itself (v10.200): its block rows are read at their spread pitch (as packed
  * bytes, only block row 0 was right) */
 {DWORD a=0,b=0,n=0,r;int spread=sg->dwReserved1==VX_SPREAD&&dxt_rows(s,&a,&b,&n);
  if(!sg->fpVidMem||IsBadReadPtr((void*)sg->fpVidMem,spread?n*b:bytes))return 1;
  if(access(0)==VX2D_LOST){p->ddRVal=DDERR_SURFACELOST;return 1;}
  if(s==d){p->ddRVal=DD_OK;return 1;}
  if(spread)for(r=0;r<n;r++)memcpy((BYTE*)(hal->vramLinear+dv.offset)+r*a,(const BYTE*)sg->fpVidMem+r*b,a);
  else memcpy((BYTE*)(hal->vramLinear+dv.offset),(const void*)sg->fpVidMem,bytes);}
 dg->dwReserved1=0;dxt_spread(d);
 p->ddRVal=DD_OK;return 1;
}
/* v10.367 (board, GTA3: VXD3D.LOG cb=20 hr=80004001, destination a texture, source caps 1000601C = the 3D back buffer, DDBLT_ROP SRCCOPY,
 * hundreds a minute): a copy from a render target or the back buffer (video memory, not a texture) into a texture of the same format, as
 * Direct3D 8's CopyRects asks (its formats must match). The source is described as the 2D engine sees the display's surfaces; the copy is
 * the engine's screen-to-screen copy into the texture's offset and pitch (the CPU copy when the engine declines). Same size, no key or
 * mirror; anything else stays unsupported. Board-unproven: the texture's rows are the sampler's pitch, as for every texture copy. */
extern void VX_D3D_log(DWORD cb,DWORD hr,DWORD why,DWORD a,DWORD b,DWORD c);
DWORD vx_dd_rt_seen[4]; /* copies tried, last source and texture formats, copies taken */
static int rt_source(LPDDHAL_BLTDATA p,swq *w,uint32_t df) {
 LPDDRAWI_DDRAWSURFACE_LCL s=p->lpDDSrcSurface;uint32_t sf=0;
 if(!s||!s->lpGbl||!(s->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY)||(s->ddsCaps.dwCaps&(DDSCAPS_TEXTURE|DDSCAPS_ZBUFFER|DDSCAPS_OVERLAY|DDSCAPS_NONLOCALVIDMEM)))return 0;
 if(!VX_D3D_format((s->dwFlags&DDRAWISURF_HASPIXELFORMAT)?&s->lpGbl->ddpfSurface:&hal->ddpf,&sf))return 0;
 vx_dd_rt_seen[0]++;vx_dd_rt_seen[1]=sf;vx_dd_rt_seen[2]=df;
 if(sf!=df||(sf!=VX3DN_XRGB8888&&sf!=VX3DN_RGB565)||VX_D3D_format_bytes(sf)*8!=hal->pFBHDA32->bpp){static int n;if(n++<4)VX_D3D_log(31,0x80004001UL,0x01,sf,df,hal->pFBHDA32->bpp);return 0;} /* C:\VXD3D.LOG cb=31: the formats that did not match */
 if((p->dwFlags&~(DDBLT_WAIT|DDBLT_ASYNC|DDBLT_ROP))||((p->dwFlags&DDBLT_ROP)&&p->bltFX.dwROP!=SRCCOPY))return 0;
 if(!view(s,&w->s)||!w->s.vram||!rectv((RECT*)&p->rSrc,&w->s))return 0;
 if(p->rSrc.right-p->rSrc.left!=p->rDest.right-p->rDest.left||p->rSrc.bottom-p->rSrc.top!=p->rDest.bottom-p->rDest.top)return 0;
 w->sr=*(RECT*)&p->rSrc;w->op=VX2D_COPY;vx_dd_rt_seen[3]++;
 return 1;
}
extern uint32_t vx_d3d_rtnowait; /* v10.477 (vx_d3d_hal.c) */
static DWORD tex_blit(LPDDHAL_BLTDATA p) {
 swq w; DWORD i,count,fx; RECT c; uint32_t df=0,sf=0;int rt=0;
 if(dxt_blit(p))return DDHAL_DRIVER_HANDLED;
 /* v10.456: the engine's draws into a render-target texture finished before the CPU copies. v10.477 (ACCEL_D3D_RTNOWAIT, default 1; 0 =
  * as before; board, GTA3 v10.476: the back buffer copied into a render-target texture every gameplay frame, 2.94 ms in the Blt, nearly all
  * of it this wait though the copy goes into XP's ring after the draws): not here: every path below orders itself after the draws: the
  * ring's copy by the ring, the 2D engine's by the VxD's wait before it (VX3D_busy), each CPU copy by its own access() before sw_blit */
 if(!vx_d3d_rtnowait&&(VX_D3D_rt_texture(p->lpDDDestSurface)||VX_D3D_rt_texture(p->lpDDSrcSurface))&&access(0)==VX2D_LOST){p->ddRVal=DDERR_SURFACELOST;return DDHAL_DRIVER_HANDLED;}
 memset(&w,0,sizeof(w));
 if(!tex_view(p->lpDDDestSurface,&w.d,1,&df)||!rectv((RECT*)&p->rDest,&w.d)) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
 w.bpp=VX_D3D_format_bytes(df)*8; /* v10.172: 2- or 4-byte texels */
 w.dr=*(RECT*)&p->rDest;
 if(p->dwFlags&DDBLT_COLORFILL) {
  if(p->dwFlags&~(DDBLT_WAIT|DDBLT_ASYNC|DDBLT_COLORFILL|DDBLT_DDFX)) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
  w.op=VX2D_FILL; w.fg=p->bltFX.dwFillColor;
 } else {
  if((p->dwFlags&~(DDBLT_WAIT|DDBLT_ASYNC|DDBLT_ROP|DDBLT_KEYSRC|DDBLT_KEYSRCOVERRIDE|DDBLT_DDFX))||((p->dwFlags&DDBLT_ROP)&&p->bltFX.dwROP!=SRCCOPY)) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
  if(!tex_view(p->lpDDSrcSurface,&w.s,0,&sf)) {if(!rt_source(p,&w,df)) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;} rt=1;} /* v10.367 */
  else if(sf!=df||!rectv((RECT*)&p->rSrc,&w.s)) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
  if(!rt) {w.sr=*(RECT*)&p->rSrc; w.op=VX2D_COPY;}
  if(!rt&&(p->dwFlags&DDBLT_DDFX)) {
   fx=p->bltFX.dwDDFX;
   if(fx&~(DDBLTFX_ARITHSTRETCHY|DDBLTFX_MIRRORLEFTRIGHT|DDBLTFX_MIRRORUPDOWN|DDBLTFX_NOTEARING)) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
   w.mirx=(fx&DDBLTFX_MIRRORLEFTRIGHT)!=0; w.miry=(fx&DDBLTFX_MIRRORUPDOWN)!=0;
  }
  if(!rt&&(p->dwFlags&(DDBLT_KEYSRC|DDBLT_KEYSRCOVERRIDE))) {
   DDCOLORKEY *k=(p->dwFlags&DDBLT_KEYSRCOVERRIDE)?&p->bltFX.ddckSrcColorkey:&p->lpDDSrcSurface->ddckCKSrcBlt;
   if(k->dwColorSpaceLowValue!=k->dwColorSpaceHighValue) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
   w.flags|=VX2D_SRC_KEY; w.key=k->dwColorSpaceLowValue;
  }
  /* v10.368 (audit N6): an overlapping stretch or mirror is done from a staged copy of the source (sw_blit); it was refused */
 }
 count=p->IsClipped?p->dwRectCnt:1;
 if(count>4096||(p->IsClipped&&count&&!p->prDestRects)) {p->ddRVal=DDERR_INVALIDPARAMS; return DDHAL_DRIVER_HANDLED;}
 if(p->IsClipped) for(i=0;i<count;i++) {
  c=p->prDestRects[i];
  if(!rectv(&c,&w.d)||c.left<p->rDest.left||c.top<p->rDest.top||c.right>p->rDest.right||c.bottom>p->rDest.bottom) {p->ddRVal=DDERR_INVALIDPARAMS; return DDHAL_DRIVER_HANDLED;}
 }
 if(rt) { /* v10.367: the 2D engine's copy from the render target into the texture, a rectangle at a time; the CPU finishes what it declines */
  vx2d_request q;DWORD rc=VX2D_LOST;
  memset(&q,0,sizeof(q));q.cb=sizeof(q);q.abi=VX2D_ABI;q.flags=VX2D_DDRAW|VX2D_RT;q.bpp=w.bpp;q.op=VX2D_COPY;q.rop=0xCC; /* v10.453: VX2D_RT, the VxD may put it in its ring */
  q.dx=p->rDest.left;q.dy=p->rDest.top;q.width=p->rDest.right-p->rDest.left;q.height=p->rDest.bottom-p->rDest.top;
  q.dst.offset=w.d.off;q.dst.pitch=w.d.pitch;q.dst.width=w.d.w;q.dst.height=w.d.h;
  q.sx=p->rSrc.left;q.sy=p->rSrc.top;q.src.offset=w.s.off;q.src.pitch=w.s.pitch;q.src.width=w.s.w;q.src.height=w.s.h;
  for(i=0;i<count;i++) {
   if(p->IsClipped){c=p->prDestRects[i];q.flags|=VX2D_CLIP;q.left=c.left;q.top=c.top;q.right=c.right;q.bottom=c.bottom;}
   if(!ioctl(VX_IOCTL_2D_SUBMIT,&q,sizeof(q),&rc,4)){p->ddRVal=DDERR_SURFACELOST;return DDHAL_DRIVER_HANDLED;}
   if(rc==VX2D_SKIP)break;
   if(rc!=VX2D_DONE)return result(rc,&p->ddRVal);
  }
  if(i==count){p->ddRVal=DD_OK;return DDHAL_DRIVER_HANDLED;}
  if(access(0)==VX2D_LOST){p->ddRVal=DDERR_SURFACELOST;return DDHAL_DRIVER_HANDLED;}
  p->ddRVal=sw_blit(p,&w,i,count)?DD_OK:DDERR_OUTOFMEMORY;return DDHAL_DRIVER_HANDLED;
 }
 if(access(0)==VX2D_LOST) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;}
 p->ddRVal=!count||sw_blit(p,&w,0,count)?DD_OK:DDERR_OUTOFMEMORY; return DDHAL_DRIVER_HANDLED;
}

/* v10.195: DirectDraw 2D callbacks that fail (any error in ddRVal, DDERR_WASSTILLDRAWING included) go to the HAL event log and
 * C:\VXD3D.LOG: callback 20 Blt, 21 Flip, 22 Lock, 23 Unlock, 24 GetBltStatus, 25 GetFlipStatus, 26 CreateSurface, 27 CanCreateSurface,
 * 28 WaitForVerticalBlank, 29 SetExclusiveMode, 30 DestroySurface; why = the callback's own return; a = surface caps, b = flags, c = surface
 * or source caps / size (v10.229: Lock/Unlock: access_why). Per callback the first 8 failures and then every power-of-two count are written (a retry loop shows its count). */
extern void VX_D3D_log(DWORD cb,DWORD hr,DWORD why,DWORD a,DWORD b,DWORD c);
static DWORD fail2d[16];
extern void VX_D3D_trace(DWORD cb,DWORD hr,DWORD why,DWORD a,DWORD b,DWORD c); /* v10.420 (diagnostic, ACCEL_D3D_LOG): Lock 93, Unlock 94, Blt 95 */
extern void VX_D3D_trace2(DWORD cb,DWORD hr,DWORD why,DWORD a,DWORD b,DWORD c);extern DWORD VX_D3D_trace_on(void);
static DWORD tr_off(LPDDRAWI_DDRAWSURFACE_LCL s) {return s&&s->lpGbl&&hal&&s->lpGbl->fpVidMem>=hal->vramLinear?s->lpGbl->fpVidMem-hal->vramLinear:0xFFFFFFFFUL;}
static void note2d(DWORD cb,HRESULT hr,DWORD rc,DWORD a,DWORD b,DWORD c) {
 DWORD n;if(!FAILED(hr))return;n=++fail2d[cb&15];
 if(n<=8||!(n&(n-1)))VX_D3D_log(cb,(DWORD)hr,rc|(n<<8),a,b,c);
}
static DWORD VXBlit_impl(LPDDHAL_BLTDATA p);
static int front_wait(LPDDRAWI_DDRAWSURFACE_LCL s); /* v10.384; v10.479: 0 = the VxD did not answer */
/* v10.387: Blts of 10 ms and more: the 8 longest (time / 256, flags, destination caps, w << 16 | h, source caps, w << 16 | h, the answer
 * (rc << 31 | ddRVal & 0x7FFFFFFF), the NOTHANDLED line << 16 | CPU blits); VX_DD_QuerySlowBlt */
static volatile DWORD sb_worst[8][8],sb_count;
DWORD WINAPI VX_DD_QuerySlowBlt(DWORD *out,DWORD n) {
 DWORD k,j;if(!out||(n&0x7FFFFFFFUL)<65)return 0;
 out[0]=sb_count;for(k=0;k<8;k++)for(j=0;j<8;j++)out[1+k*8+j]=sb_worst[k][j];
 if(n&0x80000000UL){sb_count=0;for(k=0;k<8;k++)for(j=0;j<8;j++)sb_worst[k][j]=0;}
 return 65;
}
static void sb_note(LPDDHAL_BLTDATA p,DWORD d,DWORD rc) {
 DWORD k,lo=0,d8=d>>8;if(d<10UL*ft_cpms||!p)return;sb_count++;
 for(k=1;k<8;k++)if(sb_worst[k][0]<sb_worst[lo][0])lo=k;
 if(d8<=sb_worst[lo][0])return;
 sb_worst[lo][0]=d8;sb_worst[lo][1]=p->dwFlags;sb_worst[lo][2]=p->lpDDDestSurface?p->lpDDDestSurface->ddsCaps.dwCaps:0;
 sb_worst[lo][3]=((DWORD)(p->rDest.right-p->rDest.left)<<16)|((DWORD)(p->rDest.bottom-p->rDest.top)&0xFFFF);
 sb_worst[lo][4]=p->lpDDSrcSurface?p->lpDDSrcSurface->ddsCaps.dwCaps:0;
 sb_worst[lo][5]=p->lpDDSrcSurface?(((DWORD)(p->rSrc.right-p->rSrc.left)<<16)|((DWORD)(p->rSrc.bottom-p->rSrc.top)&0xFFFF)):0;
 sb_worst[lo][6]=(rc==DDHAL_DRIVER_HANDLED?0x80000000UL:0)|((DWORD)p->ddRVal&0x7FFFFFFFUL);sb_worst[lo][7]=(blt_nh_line<<16)|(blt_sw&0xFFFF);
}
/* v10.424: VXGL's swap marks its blits to the primary (VX_DD_Present(1) before, (0) after; this thread of this process only: the DLL's
 * data is shared); the Blt then arms the VxD (control 0xFFFFFE21) so the engine copy runs asynchronously, and disarms it (0xFFFFFE20)
 * when it returns. The arm and the disarm are a word in the VxD, not a wait (vx855_2d.c). */
static volatile DWORD pr_tid,pr_pid;static DWORD blt_armed;
DWORD WINAPI VX_DD_Present(DWORD on) {pr_tid=on?GetCurrentThreadId():0;pr_pid=on?GetCurrentProcessId():0;return 1;}
static void present_arm(DWORD on) {DWORD v=on?0xFFFFFE21UL:0xFFFFFE20UL,o[4];(void)ioctl(VX_IOCTL_2D_CTRL,&v,4,o,16);blt_armed=on;}
/* v10.422 (diagnostic, ACCEL_D3D_LOG): a Blt to the primary: cb 96 hr, why = NOTHANDLED line << 16 | CPU blits, destination offset,
 * source offset, dwFlags; for each one that went to the CPU or NOTHANDLED, cb 97: the 2D engine's last result (0x3NN in bits 8-19: the
 * check validate refused), rDest left << 16 | top, right << 16 | bottom, the clip rectangle count. The first 8 from each new source and
 * every one not done by the engines are logged. */
static void blt_log(LPDDHAL_BLTDATA p,DWORD rc) {
 static DWORD last_src=0xFFFFFFFEUL,same;DWORD so;vx2d_stats st;
 if(!p||!p->lpDDDestSurface||!(p->lpDDDestSurface->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE)||!VX_D3D_trace_on())return;
 so=tr_off(p->lpDDSrcSurface);if(so!=last_src){last_src=so;same=0;}
 if(same++>=8&&!blt_sw&&!blt_nh_line)return;
 VX_D3D_trace2(96,(DWORD)p->ddRVal,blt_nh_line<<16|(blt_sw&0xFFFF),tr_off(p->lpDDDestSurface),so,p->dwFlags);
 if(blt_sw||blt_nh_line){memset(&st,0,sizeof(st));if(!ioctl(VX_IOCTL_2D_STATS,NULL,0,&st,sizeof(st)))st.last_result=0xFFFFFFFFUL;
  VX_D3D_trace2(97,rc,st.last_result,(DWORD)p->rDest.left<<16|((DWORD)p->rDest.top&0xFFFF),(DWORD)p->rDest.right<<16|((DWORD)p->rDest.bottom&0xFFFF),p->IsClipped?p->dwRectCnt:0);}
}
/* v10.473: the Blts by kind and destination (board, GTA3 v10.472: Blt 2.3 ms a frame and about one retire of the 3D work in flight a frame,
 * 2.2 ms each): kind 0 colour fill, 1 depth fill, 2 copy, 3 other; destination 0 the front (primary), 1 a back buffer or render target
 * (BACKBUFFER, FLIP or 3DDEVICE), 2 a texture, 3 anything else; per class (kind * 4 + destination) the calls, their time (TSC / 256) and
 * the destination rectangles' pixels / 256; the copies by their source's class; those finished on the CPU (n >= 69: words 53-68).
 * VX_DD_QueryBlts reads, n | 0x80000000 clears after. */
static DWORD bk_n[16],bk_t[16],bk_px[16],bk_src[4],bk_sw[16]; /* bk_sw: of them finished on the CPU (sw_blit) */
static DWORD bk_class(DWORD c) {return (c&0x220UL)?0:(c&0x2014UL)?1:(c&0x1000UL)?2:3;} /* PRIMARYSURFACE|FRONTBUFFER; BACKBUFFER|FLIP|3DDEVICE; TEXTURE */
static void bk_note(LPDDHAL_BLTDATA p,DWORD cyc) {
 DWORD f,k,i;LONG w,h;
 if(!p||!p->lpDDDestSurface)return;
 f=p->dwFlags;k=(f&0x400UL)?0:(f&0x02000000UL)?1:p->lpDDSrcSurface?2:3; /* DDBLT_COLORFILL, DDBLT_DEPTHFILL */
 i=k*4+bk_class(p->lpDDDestSurface->ddsCaps.dwCaps);
 bk_n[i]++;bk_t[i]+=(cyc+128)>>8;w=p->rDest.right-p->rDest.left;h=p->rDest.bottom-p->rDest.top;if(w>0&&h>0&&w<65536&&h<65536)bk_px[i]+=((DWORD)w*(DWORD)h+128)>>8;
 if(k==2)bk_src[bk_class(p->lpDDSrcSurface->ddsCaps.dwCaps)]++;
 if(blt_sw)bk_sw[i]++;
}
DWORD WINAPI VX_DD_QueryBlts(DWORD *out,DWORD n) {
 DWORD k,m=n&0x7FFFFFFFUL;
 if(!out||m<53)return 0;
 out[0]=ft_cpms;for(k=0;k<16;k++){out[1+k]=bk_n[k];out[17+k]=bk_t[k];out[33+k]=bk_px[k];}for(k=0;k<4;k++)out[49+k]=bk_src[k];
 if(m>=69)for(k=0;k<16;k++)out[53+k]=bk_sw[k]; /* [53-68] the CPU blits by class */
 if(n&0x80000000UL){for(k=0;k<16;k++)bk_n[k]=bk_t[k]=bk_px[k]=bk_sw[k]=0;for(k=0;k<4;k++)bk_src[k]=0;}
 return m>=69?69:53;
}
/* v10.474: the Blts by signature (board, GTA3 v10.473: one copy a gameplay frame, 640 x 480, back buffer or render target to the same
 * class, 2.53 ms each, the two retires of the 3D work inside it): up to 8 kinds told apart by flags, both surfaces' caps, both rectangles'
 * sizes and whether source and destination are one surface; per kind the calls, their time (TSC / 256), those finished on the CPU, those
 * DirectDraw finished (NOTHANDLED), and at the first call the return addresses above it on the stack (a dword whose bytes before it are a
 * CALL: E8 rel32, FF 15 / FF 95 indirect, FF D0-D7 register; v10.475: up to 16, from the callback's frame up 4 KB, none in this DLL),
 * named by VXD3DMON with the game process's modules. VX_DD_QueryBltSigs: [0] kinds kept, [1] calls of kinds not kept, then 28 words a kind
 * (v10.475; 20 in v10.474): flags, caps, caps, sizes, sizes, one surface, calls, time, CPU, DirectDraw, 16 return addresses, offsets. */
typedef struct {DWORD flags,dcaps,scaps,dsz,ssz,same,n,t,cpu,nh,ret[16],dst,src;} bsig; /* v10.475: 16 return addresses (8 were all DDRAW's and KERNEL32's thunks in VXFETCH) */
static bsig bs[8];static DWORD bs_n,bs_over;
static int bs_is_call(DWORD v) {
 const BYTE *q=(const BYTE*)v;
 if(v<0x10000UL||IsBadReadPtr(q-6,6))return 0;
 return q[-5]==0xE8||(q[-6]==0xFF&&(q[-5]==0x15||q[-5]==0x95))||(q[-2]==0xFF&&q[-1]>=0xD0&&q[-1]<=0xD7)||(q[-3]==0xFF&&(q[-2]&0xF8)==0x50);
}
static void bs_note(LPDDHAL_BLTDATA p,DWORD cyc,DWORD rc,const DWORD *sp) {
 DWORD f,dc,sc,dsz,ssz,same,k;bsig *b=0;
 if(!p||!p->lpDDDestSurface)return;
 f=p->dwFlags;dc=p->lpDDDestSurface->ddsCaps.dwCaps;sc=p->lpDDSrcSurface?p->lpDDSrcSurface->ddsCaps.dwCaps:0;
 dsz=((DWORD)(p->rDest.right-p->rDest.left)<<16)|((DWORD)(p->rDest.bottom-p->rDest.top)&0xFFFFUL);
 ssz=p->lpDDSrcSurface?(((DWORD)(p->rSrc.right-p->rSrc.left)<<16)|((DWORD)(p->rSrc.bottom-p->rSrc.top)&0xFFFFUL)):0;
 same=p->lpDDSrcSurface&&p->lpDDSrcSurface->lpGbl==p->lpDDDestSurface->lpGbl;
 for(k=0;k<bs_n;k++)if(bs[k].flags==f&&bs[k].dcaps==dc&&bs[k].scaps==sc&&bs[k].dsz==dsz&&bs[k].ssz==ssz&&bs[k].same==same){b=&bs[k];break;}
 if(!b){DWORD m=0,j;
  if(bs_n>=8){bs_over++;return;}
  b=&bs[bs_n++];memset(b,0,sizeof(*b));b->flags=f;b->dcaps=dc;b->scaps=sc;b->dsz=dsz;b->ssz=ssz;b->same=same;
  b->dst=p->lpDDDestSurface->lpGbl?(DWORD)p->lpDDDestSurface->lpGbl->fpVidMem:0;b->src=p->lpDDSrcSurface&&p->lpDDSrcSurface->lpGbl?(DWORD)p->lpDDSrcSurface->lpGbl->fpVidMem:0;
  {DWORD lo=(DWORD)instance,hi=lo; /* v10.475: this DLL's own image left out (its frames are known), 4 KB of stack scanned */
   if(lo&&!IsBadReadPtr((void*)lo,0x40)){const BYTE *pe=(const BYTE*)lo+*(const DWORD*)((const BYTE*)lo+0x3C);if(!IsBadReadPtr(pe,0x58))hi=lo+*(const DWORD*)(pe+0x50);}
   for(j=0;j<1024&&m<16;j++){if(IsBadReadPtr(sp+j,4))break;if(sp[j]-lo>=hi-lo&&bs_is_call(sp[j]))b->ret[m++]=sp[j];}}}
 b->n++;b->t+=(cyc+128)>>8;if(blt_sw)b->cpu++;if(rc!=DDHAL_DRIVER_HANDLED)b->nh++;
}
DWORD WINAPI VX_DD_QueryBltSigs(DWORD *out,DWORD n) {
 DWORD k,j,m=n&0x7FFFFFFFUL;
 if(!out||m<2+8*28)return 0;
 out[0]=bs_n;out[1]=bs_over;
 for(k=0;k<8;k++){DWORD *o=out+2+k*28;const bsig *b=&bs[k];
  o[0]=b->flags;o[1]=b->dcaps;o[2]=b->scaps;o[3]=b->dsz;o[4]=b->ssz;o[5]=b->same;o[6]=b->n;o[7]=b->t;o[8]=b->cpu;o[9]=b->nh;for(j=0;j<16;j++)o[10+j]=b->ret[j];o[26]=b->dst;o[27]=b->src;}
 if(n&0x80000000UL){memset(bs,0,sizeof(bs));bs_n=0;bs_over=0;}
 return 2+8*28;
}
/* .550: the Direct3D HAL's held draw packets (vx_d3d_hal.c) sent first: they draw into what these callbacks read, write or move; a lock of
 * an execute buffer (a DrawPrimitives2 command or vertex buffer: the packets are copies) needs nothing */
int VX_D3D_hold_sync(DWORD why);
#define HOLD_SYNC(why) VX_D3D_hold_sync(why)
DWORD WINAPI VXBlit(LPDDHAL_BLTDATA p) {DWORD t=VX_dd_tsc(),rc;if(HOLD_SYNC(1)){if(p)p->ddRVal=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;} /* .575 */blt_nh_line=0;blt_sw=0;rc=VXBlit_impl(p);if(blt_armed)present_arm(0);blt_log(p,rc);VX_frame_add(3,VX_dd_tsc()-t);bk_note(p,VX_dd_tsc()-t);bs_note(p,VX_dd_tsc()-t,rc,(const DWORD*)&t);if(p)VX_D3D_trace(95,(DWORD)p->ddRVal,rc,tr_off(p->lpDDDestSurface),tr_off(p->lpDDSrcSurface),p->dwFlags);sb_note(p,VX_dd_tsc()-t,rc);if(p)note2d(20,p->ddRVal,rc,p->lpDDDestSurface?p->lpDDDestSurface->ddsCaps.dwCaps:0,p->dwFlags,p->lpDDSrcSurface?p->lpDDSrcSurface->ddsCaps.dwCaps:0);return rc;}
static DWORD VXBlit_impl(LPDDHAL_BLTDATA p) {
 DWORD fxrop=0xCC; int ropfx=0;
 vx2d_request q; swq w; DWORD allowed,rc=VX2D_LOST,i,count,fx; RECT c; int engine;
 if(p) p->ddRVal=DD_OK; /* v10.58t: DDraw pre-sets WASSTILLDRAWING; NOTHANDLED needs DD_OK */
 if(!p||!vx_bind_hal(p->lpDD)) return nh(__LINE__);
 if(!front_wait(p->lpDDDestSurface)){p->ddRVal=DDERR_SURFACELOST;return DDHAL_DRIVER_HANDLED;} /* v10.384; v10.479 (review): not drawn into a surface that may still be shown */
 memset(&q,0,sizeof(q)); q.cb=sizeof(q); q.abi=VX2D_ABI; q.flags=VX2D_DDRAW|VX2D_ASYNC; /* v10.95: the VxD runs it asynchronously only inside an exclusive mode with ACCEL_ASYNC on */
 if(hal->d3dhal_global&&p->lpDDDestSurface&&(p->lpDDDestSurface->ddsCaps.dwCaps&(DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY))==(DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY)) return tex_blit(p); /* v10.169 */
 memset(&w,0,sizeof(w)); w.bpp=q.bpp=hal->pFBHDA32->bpp;
 if(!view(p->lpDDDestSurface,&w.d)||!rectv((RECT*)&p->rDest,&w.d)) return nh(__LINE__);
 if(!w.d.vram&&!sys_ok(w.d.p,w.d.pitch*(w.d.h-1)+w.d.w*(w.bpp/8),1)) return nh(__LINE__);
 allowed=DDBLT_WAIT|DDBLT_ASYNC|DDBLT_ROP|DDBLT_COLORFILL|DDBLT_KEYSRC|DDBLT_KEYDEST|DDBLT_KEYSRCOVERRIDE|DDBLT_KEYDESTOVERRIDE|DDBLT_DDFX|DDBLT_ALPHASRCCONSTOVERRIDE;
 if(p->dwFlags&~allowed) return nh(__LINE__);
 if(p->dwFlags&DDBLT_ALPHASRCCONSTOVERRIDE){DWORD bd=p->bltFX.dwAlphaSrcConstBitDepth,mx;
  if((w.bpp!=16&&w.bpp!=32)||(p->dwFlags&DDBLT_COLORFILL)||((p->dwFlags&DDBLT_ROP)&&p->bltFX.dwROP!=SRCCOPY))return nh(__LINE__);
  if(!bd||bd>32){p->ddRVal=DDERR_INVALIDPARAMS;return DDHAL_DRIVER_HANDLED;}
  mx=bd==32?0xFFFFFFFFUL:(1UL<<bd)-1;if(p->bltFX.dwAlphaSrcConst>mx){p->ddRVal=DDERR_INVALIDPARAMS;return DDHAL_DRIVER_HANDLED;}
  w.alpha=(DWORD)(((unsigned __int64)p->bltFX.dwAlphaSrcConst*255UL+mx/2)/mx);w.alpha_on=1;}
 w.dr=*(RECT*)&p->rDest;
 q.dx=p->rDest.left; q.dy=p->rDest.top; q.width=p->rDest.right-p->rDest.left; q.height=p->rDest.bottom-p->rDest.top;
 q.dst.offset=w.d.off; q.dst.pitch=w.d.pitch; q.dst.width=w.d.w; q.dst.height=w.d.h;
 /* v10.84 (VX2ROP, hardware: every ROP3 exact): DDBLT_ROP with a ROP other than SRCCOPY. The engine takes the ROPs that need no pattern: source ROPs
  * between video surfaces (below), and BLACKNESS, WHITENESS and DSTINVERT as fills. Everything else goes back to the runtime (NOTHANDLED, before anything is drawn). */
 if((p->dwFlags&DDBLT_ROP)&&!(p->dwFlags&DDBLT_COLORFILL)&&p->bltFX.dwROP!=SRCCOPY) {
  if((p->bltFX.dwROP>>24)!=0) return nh(__LINE__);
  fxrop=(p->bltFX.dwROP>>16)&0xFF;
  if(rop_up(fxrop)) return nh(__LINE__);
  if(p->dwFlags&(DDBLT_KEYSRC|DDBLT_KEYDEST|DDBLT_KEYSRCOVERRIDE|DDBLT_KEYDESTOVERRIDE|DDBLT_DDFX)) return nh(__LINE__);
  ropfx=1;
 }
 if(ropfx&&!rop_us(fxrop)) {
  if(!w.d.vram) return nh(__LINE__);
  if(fxrop==0xAA) {p->ddRVal=DD_OK; return DDHAL_DRIVER_HANDLED;} /* destination unchanged */
  q.op=VX2D_FILL; q.rop=(BYTE)fxrop; q.fg=0; w.op=VX2D_FILL;
  engine=1;
 } else
 if(p->dwFlags&DDBLT_COLORFILL) {
  if(p->dwFlags&~(DDBLT_WAIT|DDBLT_ASYNC|DDBLT_COLORFILL|DDBLT_DDFX)) return nh(__LINE__);
  q.op=VX2D_FILL; q.rop=0xF0; q.fg=p->bltFX.dwFillColor; w.op=VX2D_FILL; w.fg=q.fg;
  engine=w.d.vram;
 } else {
  if((p->dwFlags&DDBLT_ROP)&&p->bltFX.dwROP!=SRCCOPY&&!ropfx) return nh(__LINE__);
  if(!view(p->lpDDSrcSurface,&w.s)||!rectv((RECT*)&p->rSrc,&w.s)) return nh(__LINE__);
  if(!w.d.vram&&!w.s.vram) return nh(__LINE__); /* system to system: the runtime's own code handles it */
  w.sr=*(RECT*)&p->rSrc; w.op=VX2D_COPY;
  if(p->dwFlags&DDBLT_DDFX) {
   fx=p->bltFX.dwDDFX;
   if(fx&~(DDBLTFX_ARITHSTRETCHY|DDBLTFX_MIRRORLEFTRIGHT|DDBLTFX_MIRRORUPDOWN|DDBLTFX_NOTEARING)) return nh(__LINE__);
   w.mirx=(fx&DDBLTFX_MIRRORLEFTRIGHT)!=0; w.miry=(fx&DDBLTFX_MIRRORUPDOWN)!=0;
  }
  q.op=VX2D_COPY; q.rop=(BYTE)fxrop; q.sx=p->rSrc.left; q.sy=p->rSrc.top;
  q.src.offset=w.s.off; q.src.pitch=w.s.pitch; q.src.width=w.s.w; q.src.height=w.s.h;
  if((p->dwFlags&(DDBLT_KEYSRC|DDBLT_KEYSRCOVERRIDE))&&(p->dwFlags&(DDBLT_KEYDEST|DDBLT_KEYDESTOVERRIDE))) return nh(__LINE__);
  if(p->dwFlags&(DDBLT_KEYSRC|DDBLT_KEYSRCOVERRIDE)) {
   DDCOLORKEY *k=(p->dwFlags&DDBLT_KEYSRCOVERRIDE)?&p->bltFX.ddckSrcColorkey:&p->lpDDSrcSurface->ddckCKSrcBlt;
   if(k->dwColorSpaceLowValue!=k->dwColorSpaceHighValue) return nh(__LINE__);
   q.flags|=VX2D_SRC_KEY; q.key=k->dwColorSpaceLowValue; w.flags|=VX2D_SRC_KEY; w.key=q.key;
  }
  if(VXDBGKEYON&&(p->dwFlags&(DDBLT_KEYSRC|DDBLT_KEYSRCOVERRIDE|DDBLT_KEYDEST|DDBLT_KEYDESTOVERRIDE))) {
   LPDDRAWI_DDRAWSURFACE_LCL dl=p->lpDDDestSurface,sl=p->lpDDSrcSurface;
   VXDBGKEY[0]++; VXDBGKEY[1]=p->dwFlags; VXDBGKEY[2]=dl?dl->dwFlags:0; VXDBGKEY[3]=dl?dl->ddckCKDestBlt.dwColorSpaceLowValue:0; VXDBGKEY[4]=dl?dl->ddckCKDestBlt.dwColorSpaceHighValue:0;
   VXDBGKEY[5]=dl?dl->ddckCKSrcBlt.dwColorSpaceLowValue:0; VXDBGKEY[6]=dl?dl->ddckCKSrcBlt.dwColorSpaceHighValue:0;
   VXDBGKEY[7]=p->bltFX.ddckDestColorkey.dwColorSpaceLowValue; VXDBGKEY[8]=p->bltFX.ddckDestColorkey.dwColorSpaceHighValue;
   VXDBGKEY[9]=p->bltFX.ddckSrcColorkey.dwColorSpaceLowValue; VXDBGKEY[10]=p->bltFX.ddckSrcColorkey.dwColorSpaceHighValue;
   VXDBGKEY[12]=(DWORD)dl; VXDBGKEY[13]=(DWORD)sl; VXDBGKEY[14]=sl?sl->ddckCKSrcBlt.dwColorSpaceLowValue:0; VXDBGKEY[15]=sl?sl->ddckCKSrcBlt.dwColorSpaceHighValue:0;
   VXDBGKEY[16]=p->bltFX.dwSize; VXDBGKEY[17]=dl?dl->ddckCKDestOverlay.dwColorSpaceLowValue:0; VXDBGKEY[18]=dl?dl->ddckCKDestOverlay.dwColorSpaceHighValue:0; VXDBGKEY[19]=sl?sl->dwFlags:0; VXDBGKEY[20]=sl?sl->ddckCKSrcOverlay.dwColorSpaceLowValue:0; VXDBGKEY[21]=p->bltFX.dwDDFX;
  }
  if(p->dwFlags&(DDBLT_KEYDEST|DDBLT_KEYDESTOVERRIDE)) {
   DDCOLORKEY *k=(p->dwFlags&DDBLT_KEYDESTOVERRIDE)?&p->bltFX.ddckDestColorkey:&p->lpDDDestSurface->ddckCKDestBlt;
   /* v10.147 (VX2DKEY v1.2): the Windows 98 runtime hands DDBLT_KEYDEST over as DDBLT_KEYDESTOVERRIDE with bltFX's destination key left 0..0, while the
    * destination surface holds the key set by SetColorKey(DDCKEY_DESTBLT) (flag DDRAWISURF_HASCKEYDESTBLT). Then the surface's key is the one meant.
    * (Misread only when an application overrides with key 0 on a surface that also has a different destination key of its own.) */
   if((p->dwFlags&DDBLT_KEYDESTOVERRIDE)&&!k->dwColorSpaceLowValue&&!k->dwColorSpaceHighValue&&p->lpDDDestSurface&&(p->lpDDDestSurface->dwFlags&DDRAWISURF_HASCKEYDESTBLT)) k=&p->lpDDDestSurface->ddckCKDestBlt;
   if(VXDBGKEYON) VXDBGKEY[11]=k->dwColorSpaceLowValue;
   if(k->dwColorSpaceLowValue!=k->dwColorSpaceHighValue) return nh(__LINE__);
   q.flags|=VX2D_DST_KEY; q.key=k->dwColorSpaceLowValue; w.flags|=VX2D_DST_KEY; w.key=q.key;
  }
  /* the engine copies same-size rectangles between video surfaces; everything else is done on the CPU */
  engine=!w.alpha_on&&w.d.vram&&w.s.vram&&!w.mirx&&!w.miry&&w.sr.right-w.sr.left==w.dr.right-w.dr.left&&w.sr.bottom-w.sr.top==w.dr.bottom-w.dr.top;
  /* v10.368 (audit N6): an overlapping stretch or mirror goes to the CPU from a staged copy of the source (sw_blit); it was NOTHANDLED,
   * which for video surfaces is E_NOTIMPL for the application */
 }
if(ropfx&&((q.op==VX2D_COPY&&!engine)||(p->IsClipped&&p->dwRectCnt!=1))) return nh(__LINE__); /* v10.84: a source ROP needs the engine and one rectangle (nothing is drawn yet) */
 /* Validate the entire clip list before the first command: a later rejection
  * must not ask the runtime to replay a partially drawn operation. */
 count=p->IsClipped?p->dwRectCnt:1;
 if(count>4096||(p->IsClipped&&count&&!p->prDestRects)) return nh(__LINE__);
 if(p->IsClipped) for(i=0;i<count;i++) {
  c=p->prDestRects[i];
  if(!rectv(&c,&w.d)||c.left<p->rDest.left||c.top<p->rDest.top||c.right>p->rDest.right||c.bottom>p->rDest.bottom) return nh(__LINE__);
 }
 if(!count) {p->ddRVal=DD_OK; return DDHAL_DRIVER_HANDLED;}
 /* Native overlap direction and key validation must happen before a list.
  * Runtime clips of overlapping self copies are left to its software path. */
 /* v10.368 (audit N6): a clipped copy within one surface: the engine when source and destination rectangles are apart (the order of the
  * pieces does not matter then), otherwise the CPU from a staged copy of the source; both were NOTHANDLED */
 if(count>1&&q.op==VX2D_COPY&&overlap(&w)) engine=0;
 if(!engine) {
  /* v10.75: CPU paths (stretch, mirror, system memory) touch the frame buffer too: never while the engine is lost and may still be writing to it */
  if((w.d.vram||w.s.vram)&&access(0)==VX2D_LOST) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;}
  p->ddRVal=sw_blit(p,&w,0,count)?DD_OK:DDERR_OUTOFMEMORY; return DDHAL_DRIVER_HANDLED;
 }
 if(q.op==VX2D_COPY&&pr_tid&&pr_tid==GetCurrentThreadId()&&pr_pid==GetCurrentProcessId()&&(p->lpDDDestSurface->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE))present_arm(1); /* v10.424 */
 for(i=0;i<count;i++) {
  if(p->IsClipped) {c=p->prDestRects[i]; q.flags|=VX2D_CLIP; q.left=c.left; q.top=c.top; q.right=c.right; q.bottom=c.bottom;}
  if(!ioctl(VX_IOCTL_2D_SUBMIT,&q,sizeof(q),&rc,4)) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;}
   if(rc==VX2D_SKIP) {
   if(ropfx) return nh(__LINE__); /* v10.84: only a single rectangle, nothing drawn: the runtime does it */
   /* engine declined (disabled, busy, ...): finish this and any remaining rectangles on the CPU. v10.368 (audit N1): after the wait for
    * the engine, as the CPU path above (a 3D draw in flight could still be writing the destination) */
   if(access(0)==VX2D_LOST) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;}
   if(!sw_blit(p,&w,i,count)) {p->ddRVal=DDERR_OUTOFMEMORY; return DDHAL_DRIVER_HANDLED;} p->ddRVal=DD_OK; return DDHAL_DRIVER_HANDLED;
  }
  if(rc!=VX2D_DONE) return result(rc,&p->ddRVal);
 }
 p->ddRVal=DD_OK; return DDHAL_DRIVER_HANDLED;
}
static int flip_ok(void);
/* ---- v10.106: video overlay (VxD request VX_IOCTL_OVL). Offered only when the VxD's ACCEL_OVERLAY mask is set (DriverInit then sets cb32.UpdateOverlay). ---- */
#define VX_FOURCC_YUY2 0x32595559UL
static int ov_call(vx_ovl_req *q,DWORD *r) {
 q->cb=sizeof(*q); q->abi=VXO_ABI;
 return ioctl(VX_IOCTL_OVL,q,sizeof(*q),r,4*VXO_REPLY_DWORDS);
}
static int ov_mask(void) { /* live ACCEL_OVERLAY mask of the VxD, 0 when the overlay is off */
 vx_ovl_req q; DWORD r[VXO_REPLY_DWORDS];
 memset(&q,0,sizeof(q)); q.cmd=VXO_QUERY;
 if(!ov_call(&q,r)) return 0;
 return (int)r[VXO_R_MASK];
}
/* 0 unsupported, else 1 + VXO_FMT_* (v10.108: planar YUV with mask bit 7, RGB with mask bit 6) */
#define VX_FOURCC_YV12 0x32315659UL
#define VX_FOURCC_I420 0x30323449UL
#define VX_FOURCC_IYUV 0x56555949UL
static int ov_fmt(const DDPIXELFORMAT *f,int mask) {
 if(!f) return 0;
 if(f->dwFlags&DDPF_FOURCC) {
  if(f->dwFlags&(DDPF_RGB|DDPF_PALETTEINDEXED8)) return 0;
  if(f->dwFourCC==VX_FOURCC_YUY2) return 1+VXO_FMT_YUY2;
  if((mask&VXO_M_PLANAR)&&f->dwFourCC==VX_FOURCC_YV12) return 1+VXO_FMT_YV12;
  if((mask&VXO_M_PLANAR)&&(f->dwFourCC==VX_FOURCC_I420||f->dwFourCC==VX_FOURCC_IYUV)) return 1+VXO_FMT_I420;
  return 0;
 }
 if((mask&VXO_M_HQV)&&f->dwFlags==DDPF_RGB&&!f->dwRGBAlphaBitMask) {
  if(f->dwRGBBitCount==16&&f->dwRBitMask==0xF800UL&&f->dwGBitMask==0x7E0UL&&f->dwBBitMask==0x1FUL) return 1+VXO_FMT_RGB16;
  if(f->dwRGBBitCount==16&&f->dwRBitMask==0x7C00UL&&f->dwGBitMask==0x3E0UL&&f->dwBBitMask==0x1FUL) return 1+VXO_FMT_RGB15;
  if(f->dwRGBBitCount==32&&f->dwRBitMask==0xFF0000UL&&f->dwGBitMask==0xFF00UL&&f->dwBBitMask==0xFFUL) return 1+VXO_FMT_RGB32;
 }
 return 0;
}
/* pitch of an overlay surface row and total bytes of its memory for a format, 0 pitch = not possible */
static DWORD ov_pitch(int f,DWORD w) {
 DWORD p;
 if(f==1+VXO_FMT_YV12||f==1+VXO_FMT_I420) p=(w+31)&~31UL;
 else if(f==1+VXO_FMT_RGB32) p=((w*4)+31)&~31UL;
 else p=((w*2)+31)&~31UL;
 return p;
}
/* v10.114: a surface made in the desktop's own pixel format has no HASPIXELFORMAT flag (DirectDraw leaves ddpfSurface unset): its format is the desktop's. An RGB32 overlay on a 32 bit desktop is exactly that case. */
static int ov_sfmt(LPDDRAWI_DDRAWSURFACE_LCL s,LPDDRAWI_DDRAWSURFACE_GBL g,int mask) {
 DDPIXELFORMAT pf; DWORD bpp;
 if(s->dwFlags&DDRAWISURF_HASPIXELFORMAT) return ov_fmt(&g->ddpfSurface,mask);
 bpp=hal->pFBHDA32->bpp;
 memset(&pf,0,sizeof(pf)); pf.dwSize=sizeof(pf); pf.dwFlags=DDPF_RGB; pf.dwRGBBitCount=bpp;
 if(bpp==32) {pf.dwRBitMask=0xFF0000UL; pf.dwGBitMask=0xFF00UL; pf.dwBBitMask=0xFFUL;}
 else if(bpp==16) {pf.dwRBitMask=0xF800UL; pf.dwGBitMask=0x7E0UL; pf.dwBBitMask=0x1FUL;}
 else return 0;
 return ov_fmt(&pf,mask);
}
static DWORD VXCanCreate_impl(LPDDHAL_CANCREATESURFACEDATA p);
DWORD WINAPI VXCanCreate(LPDDHAL_CANCREATESURFACEDATA p) {DWORD rc=VXCanCreate_impl(p);if(p)note2d(27,p->ddRVal,rc,p->lpDDSurfaceDesc?p->lpDDSurfaceDesc->ddsCaps.dwCaps:0,p->lpDDSurfaceDesc?p->lpDDSurfaceDesc->dwFlags:0,p->lpDDSurfaceDesc?(p->lpDDSurfaceDesc->dwWidth<<16|(p->lpDDSurfaceDesc->dwHeight&0xFFFF)):0);return rc;}
static DWORD VXCanCreate_impl(LPDDHAL_CANCREATESURFACEDATA p) {
 DWORD cap;
 if(p) p->ddRVal=DD_OK; /* v10.58t: DDraw pre-sets WASSTILLDRAWING; NOTHANDLED needs DD_OK */
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 cap=p->lpDDSurfaceDesc->ddsCaps.dwCaps;
 /* v10.169: a texture in video memory: 32-bit A8R8G8B8 or X8R8G8B8, power-of-two sides up to 1024, one level (no mipmaps). A system-memory
  * texture (a managed texture's copy) is the runtime's own. */
 if(hal->d3dhal_global&&(cap&DDSCAPS_TEXTURE)&&!(cap&DDSCAPS_SYSTEMMEMORY)) {
  uint32_t f;const DDPIXELFORMAT *pf=(p->lpDDSurfaceDesc->dwFlags&DDSD_PIXELFORMAT)?&p->lpDDSurfaceDesc->ddpfPixelFormat:&hal->ddpf;
  p->ddRVal=DDERR_UNSUPPORTED;
  /* v10.199: mipmapped textures (a complex chain of DDSCAPS_MIPMAP levels), L8, DXT1/3/5, up to 2048 */
  /* v10.456: ACCEL_D3D_RTTEX: + DDSCAPS_3DDEVICE (a render-target texture): X8R8G8B8 or R5G6B5, one level, not a cube map */
  if(!(cap&(DDSCAPS_ZBUFFER|DDSCAPS_OVERLAY|DDSCAPS_NONLOCALVIDMEM|DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP))&&
     (!(cap&DDSCAPS_3DDEVICE)||(vx_d3d_rttex&&!(cap&(DDSCAPS_MIPMAP|DDSCAPS_COMPLEX))&&VX_D3D_format(pf,&f)&&(f==VX3DN_XRGB8888||f==VX3DN_RGB565||f==VX3DN_TEX_ARGB8888)))&&
     VX_D3D_format(pf,&f)&&VX_D3D_texture_ok(f,p->lpDDSurfaceDesc->dwWidth,p->lpDDSurfaceDesc->dwHeight)) p->ddRVal=DD_OK; /* v10.459: + A8R8G8B8 */
  else if(!VX_D3D_format(pf,&f)) p->ddRVal=DDERR_INVALIDPIXELFORMAT;
  /* v10.459: a render-target texture refused goes to the HAL event log with what it asked for (callback 32: caps; pixel format flags,
   * bits, red mask, alpha mask; w << 16 | h; mip count), the first 16 */
  if(p->ddRVal!=DD_OK&&(cap&DDSCAPS_3DDEVICE)){static int n;if(n++<16){VX_D3D_log(32,(DWORD)p->ddRVal,cap,pf->dwFlags|(pf->dwRGBBitCount<<24),pf->dwRBitMask,pf->dwRGBAlphaBitMask);
   VX_D3D_log(33,(DWORD)p->ddRVal,p->lpDDSurfaceDesc->dwFlags,(p->lpDDSurfaceDesc->dwWidth<<16)|(p->lpDDSurfaceDesc->dwHeight&0xFFFF),(p->lpDDSurfaceDesc->dwFlags&DDSD_MIPMAPCOUNT)?p->lpDDSurfaceDesc->dwMipMapCount:0,0);}}
  return DDHAL_DRIVER_HANDLED;
 }
 if(hal->d3dhal_global&&(cap&(DDSCAPS_3DDEVICE|DDSCAPS_ZBUFFER))) {
  uint32_t f;const DDPIXELFORMAT *pf=(p->lpDDSurfaceDesc->dwFlags&DDSD_PIXELFORMAT)?&p->lpDDSurfaceDesc->ddpfPixelFormat:&hal->ddpf;
  DWORD w=p->lpDDSurfaceDesc->dwWidth,h=p->lpDDSurfaceDesc->dwHeight;
  p->ddRVal=DDERR_UNSUPPORTED;
  /* v10.166: the primary (alone, or with its flip chain) as a render target: display size and format; a chain needs the display start to move.
   * Its surface description carries no width or height. Other targets stay single local surfaces. */
  if(cap&DDSCAPS_PRIMARYSURFACE) {w=hal->pFBHDA32->width;h=hal->pFBHDA32->height;}
  if((cap&(DDSCAPS_FLIP|DDSCAPS_COMPLEX))&&(!(cap&DDSCAPS_PRIMARYSURFACE)||(cap&DDSCAPS_ZBUFFER)||!flip_ok())) return DDHAL_DRIVER_HANDLED;
  if(!(cap&(DDSCAPS_OVERLAY|DDSCAPS_TEXTURE|DDSCAPS_SYSTEMMEMORY|DDSCAPS_NONLOCALVIDMEM))&&
     VX_D3D_format(pf,&f)&&((cap&DDSCAPS_ZBUFFER)?VX3DN_IS_DEPTH(f):(f==1||f==2))&&w&&w<=2048&&h&&h<=2048)p->ddRVal=DD_OK;
  return DDHAL_DRIVER_HANDLED;
 }
 /* v10.59: system-memory surfaces are the runtime's own business (their blits are handled in VXBlit) */
 if(cap&DDSCAPS_SYSTEMMEMORY) {p->ddRVal=DD_OK; return DDHAL_DRIVER_NOTHANDLED;}
 if((cap&DDSCAPS_OVERLAY)&&!(cap&(DDSCAPS_TEXTURE|DDSCAPS_ZBUFFER|DDSCAPS_NONLOCALVIDMEM|DDSCAPS_PRIMARYSURFACE))&&hal->cb32.UpdateOverlay) {
  /* v10.106: overlay surfaces (and their flip chains: the overlay flips by its own start address, not the display's) */
  int m=ov_mask(),f; DWORD w=p->lpDDSurfaceDesc->dwWidth,pitch;
  if(!(m&3)) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
  if(!(p->lpDDSurfaceDesc->dwFlags&DDSD_PIXELFORMAT)||!(f=ov_fmt(&p->lpDDSurfaceDesc->ddpfPixelFormat,m))) {p->ddRVal=DDERR_INVALIDPIXELFORMAT; return DDHAL_DRIVER_HANDLED;}
  pitch=ov_pitch(f,w);
  if(!w||(w&1)||w>2048||(DWORD)p->lpDDSurfaceDesc->dwHeight>2048||pitch>0x1FFFUL||((f==1+VXO_FMT_YV12||f==1+VXO_FMT_I420)&&((DWORD)p->lpDDSurfaceDesc->dwHeight&1))) {p->ddRVal=DDERR_INVALIDPARAMS; return DDHAL_DRIVER_HANDLED;}
  p->ddRVal=DD_OK; return DDHAL_DRIVER_HANDLED;
 }
 if(cap&(DDSCAPS_OVERLAY|DDSCAPS_TEXTURE|DDSCAPS_ZBUFFER|DDSCAPS_NONLOCALVIDMEM)) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
 /* v10.59: a flip chain (primary + back buffers) is allowed when the display start address can be moved */
 if(cap&(DDSCAPS_FLIP|DDSCAPS_COMPLEX)) if(!flip_ok()) {p->ddRVal=(cap&DDSCAPS_FLIP)?DDERR_NOFLIPHW:DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
 if(p->bIsDifferentPixelFormat&&!format(&p->lpDDSurfaceDesc->ddpfPixelFormat,hal->pFBHDA32->bpp)) {p->ddRVal=DDERR_INVALIDPIXELFORMAT; return DDHAL_DRIVER_HANDLED;}
 p->ddRVal=DD_OK; return DDHAL_DRIVER_HANDLED;
}
static DWORD VXCreate_impl(LPDDHAL_CREATESURFACEDATA p);
DWORD WINAPI VXCreate(LPDDHAL_CREATESURFACEDATA p) {DWORD t=VX_dd_tsc(),rc=VXCreate_impl(p);VX_frame_add(4,VX_dd_tsc()-t);ft_count(2);if(p)note2d(26,p->ddRVal,rc,(p->dwSCnt&&p->lplpSList&&p->lplpSList[0])?p->lplpSList[0]->ddsCaps.dwCaps:0,p->dwSCnt,(p->dwSCnt&&p->lplpSList&&p->lplpSList[0]&&p->lplpSList[0]->lpGbl)?((DWORD)p->lplpSList[0]->lpGbl->wWidth<<16|p->lplpSList[0]->lpGbl->wHeight):0);return rc;}
static DWORD VXCreate_impl(LPDDHAL_CREATESURFACEDATA p) {
 DWORD i,pitch;
 if(p) p->ddRVal=DD_OK; /* v10.58t: DDraw pre-sets WASSTILLDRAWING; NOTHANDLED needs DD_OK */
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 for(i=0;i<p->dwSCnt;i++) {
  LPDDRAWI_DDRAWSURFACE_GBL g=p->lplpSList[i]->lpGbl;
  if(hal->d3dhal_global&&(p->lplpSList[i]->ddsCaps.dwCaps&(DDSCAPS_3DDEVICE|DDSCAPS_ZBUFFER))&&!VX_D3D_rt_texture(p->lplpSList[i])) { /* v10.456: a render-target texture is checked as a texture */
   uint32_t f;const DDPIXELFORMAT *pf=(p->lplpSList[i]->dwFlags&DDRAWISURF_HASPIXELFORMAT)?&g->ddpfSurface:&hal->ddpf;
   if(!VX_D3D_format(pf,&f)||!g->wWidth||g->wWidth>2048||!g->wHeight||g->wHeight>2048){p->ddRVal=DDERR_INVALIDPIXELFORMAT;return DDHAL_DRIVER_HANDLED;}
   if(p->lplpSList[i]->ddsCaps.dwCaps&(DDSCAPS_OVERLAY|DDSCAPS_TEXTURE|DDSCAPS_SYSTEMMEMORY|DDSCAPS_NONLOCALVIDMEM)){p->ddRVal=DDERR_UNSUPPORTED;return DDHAL_DRIVER_HANDLED;}
   if((p->lplpSList[i]->ddsCaps.dwCaps&(DDSCAPS_FLIP|DDSCAPS_COMPLEX))&&((p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_ZBUFFER)||(!(p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE)&&((DWORD)g->wWidth!=hal->pFBHDA32->width||(DWORD)g->wHeight!=hal->pFBHDA32->height))||!flip_ok())){p->ddRVal=DDERR_UNSUPPORTED;return DDHAL_DRIVER_HANDLED;} /* v10.166: 3D flip chains are display-sized */
   if((p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_ZBUFFER)?!VX3DN_IS_DEPTH(f):(f!=1&&f!=2)){p->ddRVal=DDERR_INVALIDPIXELFORMAT;return DDHAL_DRIVER_HANDLED;}
   if((p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE)&&(hal->pFBHDA32->pitch&31)){p->ddRVal=DDERR_UNSUPPORTED;return DDHAL_DRIVER_HANDLED;}
  }
  if(hal->d3dhal_global&&(p->lplpSList[i]->ddsCaps.dwCaps&(DDSCAPS_TEXTURE|DDSCAPS_SYSTEMMEMORY))==DDSCAPS_TEXTURE) { /* v10.169 */
   uint32_t f;const DDPIXELFORMAT *pf=(p->lplpSList[i]->dwFlags&DDRAWISURF_HASPIXELFORMAT)?&g->ddpfSurface:&hal->ddpf;
   if(!VX_D3D_format(pf,&f)||!VX_D3D_texture_ok(f,g->wWidth,g->wHeight)||(p->lplpSList[i]->ddsCaps.dwCaps&(DDSCAPS_ZBUFFER|DDSCAPS_OVERLAY|DDSCAPS_NONLOCALVIDMEM|DDSCAPS_PRIMARYSURFACE))||
      ((p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_3DDEVICE)&&!VX_D3D_rt_texture(p->lplpSList[i]))){p->ddRVal=DDERR_UNSUPPORTED;return DDHAL_DRIVER_HANDLED;} /* v10.199: mip levels allowed; v10.456: a render-target texture */
   continue;
  }
  if(p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_OVERLAY) continue; /* v10.106 */
  /* v10.279 (review): a system-memory surface is not limited by the engine's pitch (the VxD declines it; the CPU copies it): it was
   * refused here with the desktop's bytes per pixel, not its own (a 5000-pixel 565 surface on a 32 bpp desktop counted 20000 bytes) */
  if(p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_SYSTEMMEMORY) continue;
  pitch=(((DWORD)g->wWidth*(hal->pFBHDA32->bpp/8))+15)&~15UL; /* v10.277: rows a multiple of 16 bytes (the 2D engine's pitch, VXKEY) */
  if(pitch>0x3FF0UL) {p->ddRVal=DDERR_INVALIDPARAMS; return DDHAL_DRIVER_HANDLED;}
 }
 for(i=0;i<p->dwSCnt;i++) {
  LPDDRAWI_DDRAWSURFACE_GBL g=p->lplpSList[i]->lpGbl;
  if(hal->d3dhal_global&&(p->lplpSList[i]->ddsCaps.dwCaps&(DDSCAPS_3DDEVICE|DDSCAPS_ZBUFFER))&&!VX_D3D_rt_texture(p->lplpSList[i])) { /* v10.456: a render-target texture gets the texture's rows (below) */
   uint32_t f;const DDPIXELFORMAT *pf=(p->lplpSList[i]->dwFlags&DDRAWISURF_HASPIXELFORMAT)?&g->ddpfSurface:&hal->ddpf;
   VX_D3D_format(pf,&f);
   if(p->lplpSList[i]->ddsCaps.dwCaps&(DDSCAPS_FLIP|DDSCAPS_BACKBUFFER)) {if(!(p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE))g->lPitch=hal->pFBHDA32->pitch;} /* v10.166: a 3D back buffer has the scanout pitch (the display start moves between whole screens) */
   else if(!(p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE))g->lPitch=((DWORD)g->wWidth*VX_D3D_format_bytes(f)+31)&~31UL;
   continue;
  }
  if(hal->d3dhal_global&&(p->lplpSList[i]->ddsCaps.dwCaps&(DDSCAPS_TEXTURE|DDSCAPS_SYSTEMMEMORY))==DDSCAPS_TEXTURE) { /* v10.169: the sampler takes rows of width*bytes per texel */
   uint32_t f=VX3DN_XRGB8888;const DDPIXELFORMAT *pf=(p->lplpSList[i]->dwFlags&DDRAWISURF_HASPIXELFORMAT)?&g->ddpfSurface:&hal->ddpf;VX_D3D_format(pf,&f);
   /* v10.199: a DXT surface reports its linear size in lPitch (dwLinearSize), as XP's driver does. v10.200: other rows at least 32 bytes */
   g->lPitch=VX_RAW_IS_DXT(f)?VX_D3D_texture_packed(f,g->wWidth)*VX_D3D_texture_rows(f,g->wHeight):VX_D3D_texture_pitch(f,g->wWidth);g->dwReserved1=0;continue;}
  if(p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_OVERLAY) { /* v10.106: QWORD-pair aligned rows; v10.108: planar surfaces (Y plane, then two quarter-size planes) ask DirectDraw for a block of the whole size */
   int f=ov_sfmt(p->lplpSList[i],g,ov_mask());
   g->lPitch=f?ov_pitch(f,(DWORD)g->wWidth):(((DWORD)g->wWidth*2)+31)&~31UL;
   if(f==1+VXO_FMT_YV12||f==1+VXO_FMT_I420) {g->fpVidMem=DDHAL_PLEASEALLOC_BLOCKSIZE; g->dwBlockSizeX=(DWORD)g->lPitch*(DWORD)g->wHeight*3UL/2UL; g->dwBlockSizeY=1;}
   continue;
  }
  if(!(p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE)) {
   /* back buffers must have the scanout pitch (the display start address moves between whole screens) */
   if((p->lplpSList[i]->ddsCaps.dwCaps&(DDSCAPS_FLIP|DDSCAPS_BACKBUFFER))&&(DWORD)g->wWidth==hal->pFBHDA32->width&&(DWORD)g->wHeight==hal->pFBHDA32->height) g->lPitch=hal->pFBHDA32->pitch;
   else if((p->lplpSList[i]->ddsCaps.dwCaps&DDSCAPS_SYSTEMMEMORY)&&(p->lplpSList[i]->dwFlags&DDRAWISURF_HASPIXELFORMAT)&&(g->ddpfSurface.dwFlags&DDPF_RGB)&&
           (g->ddpfSurface.dwRGBBitCount==16||g->ddpfSurface.dwRGBBitCount==32)) g->lPitch=(((DWORD)g->wWidth*(g->ddpfSurface.dwRGBBitCount/8))+15)&~15UL; /* v10.169: a 32-bit texture copy on a 16-bit desktop */
   /* v10.277 (VXKEY, board): the 2D engine takes a pitch in 16-byte steps (its pitch field's low bit is not used): a surface whose rows
    * were rounded to 8 bytes only (an odd width at 32 bpp, a width of 4n+1..4n+3 at 16 bpp) was filled and copied with the next lower
    * multiple of 16, so a keyed copy of the sample framework's text surfaces came out sheared (Tunnel's statistics line) and a fill left
    * the end of the last row. Rows are now a multiple of 16 bytes; the VxD declines any other pitch (DirectDraw's software does it). */
   else g->lPitch=(((DWORD)g->wWidth*(hal->pFBHDA32->bpp/8))+15)&~15UL;
  }
 }
 /* v10.179: every video-memory block a multiple of 256 bytes. DirectDraw's linear heap packs blocks end to end and does not apply
  * dwZBufferAlign / dwOffscreenAlign, so a surface whose size is not a multiple of 256 put the next one off the 3D engine's 256-byte
  * base alignment (VXDX7 --wintune, v10.178: a 292x223 X8R8G8B8 back buffer of 264032 bytes, then the Z buffer after it refused by
  * CreateSurfaceEx with DDERR_UNSUPPORTED). The heap starts on a 256-byte boundary (dddrv.c), so with every block rounded every block
  * starts on one. Overlays keep their own block size, system memory and the primary are not from the heap. */
 for(i=0;i<p->dwSCnt;i++) {
  LPDDRAWI_DDRAWSURFACE_GBL g=p->lplpSList[i]->lpGbl;DWORD c=p->lplpSList[i]->ddsCaps.dwCaps;
  /* v10.245: a D24S8 depth buffer's block holds its stencil plane too, whatever VX_HEAP_ROUND says */
  if((c&DDSCAPS_ZBUFFER)&&!(c&(DDSCAPS_SYSTEMMEMORY|DDSCAPS_NONLOCALVIDMEM))&&!g->fpVidMem&&g->lPitch>0) {
   uint32_t f;const DDPIXELFORMAT *pf=(p->lplpSList[i]->dwFlags&DDRAWISURF_HASPIXELFORMAT)?&g->ddpfSurface:&hal->ddpf;
   if(VX_D3D_format(pf,&f)&&f==VX3DN_D24S8){g->fpVidMem=DDHAL_PLEASEALLOC_BLOCKSIZE;g->dwBlockSizeX=VX3DN_SPLANE_BLOCK(g->lPitch,g->wWidth,g->wHeight);g->dwBlockSizeY=1;g->dwReserved1=0;continue;}
   if(VX_D3D_format(pf,&f)&&f==VX3DN_D16){g->fpVidMem=DDHAL_PLEASEALLOC_BLOCKSIZE;g->dwBlockSizeX=VX3DN_Z16_BLOCK(g->lPitch,g->wWidth,g->wHeight);g->dwBlockSizeY=1;g->dwReserved1=0;continue;} /* v10.295: and its Coarse Z storage */
  }
  if(!vx_heap_round||(c&(DDSCAPS_PRIMARYSURFACE|DDSCAPS_SYSTEMMEMORY|DDSCAPS_OVERLAY|DDSCAPS_NONLOCALVIDMEM))||g->fpVidMem||g->lPitch<=0) continue;
  if(c&(DDSCAPS_FLIP|DDSCAPS_BACKBUFFER)&&!(c&(DDSCAPS_3DDEVICE|DDSCAPS_ZBUFFER))) continue; /* a 2D flip chain is display-sized: pitch x height is already whole screens */
  {DWORD bytes=(DWORD)g->lPitch*(DWORD)g->wHeight;uint32_t f;const DDPIXELFORMAT *pf=(p->lplpSList[i]->dwFlags&DDRAWISURF_HASPIXELFORMAT)?&g->ddpfSurface:&hal->ddpf;
   if((c&DDSCAPS_TEXTURE)&&VX_D3D_format(pf,&f)&&VX_RAW_IS_DXT(f))bytes=VX_D3D_texture_pitch(f,g->wWidth)*VX_D3D_texture_rows(f,g->wHeight); /* v10.199: the linear size; v10.200: spread rows */
   p->lplpSList[i]->dwReserved1=0;
   /* v10.310: a DXT mip chain's top level (the list's first, each next one half the size): a block for every level, laid out as the
    * sampler reads them (dxt_publish) */
   if(!i&&p->dwSCnt>1&&(c&DDSCAPS_TEXTURE)&&(c&DDSCAPS_MIPMAP)&&(vx_d3d_feat&VX_F_DXTCHAIN)&&VX_D3D_format(pf,&f)&&VX_RAW_IS_DXT(f)) {
    DWORD j,all=0;int ok=1;
    for(j=0;j<p->dwSCnt&&ok;j++){LPDDRAWI_DDRAWSURFACE_LCL l=p->lplpSList[j];LPDDRAWI_DDRAWSURFACE_GBL lg=l->lpGbl;uint32_t lf;DWORD w=g->wWidth>>j,h=g->wHeight>>j;if(!w)w=1;if(!h)h=1;
     if(!(l->ddsCaps.dwCaps&DDSCAPS_MIPMAP)||!(l->ddsCaps.dwCaps&DDSCAPS_TEXTURE)||!(l->dwFlags&DDRAWISURF_HASPIXELFORMAT)||!VX_D3D_format(&lg->ddpfSurface,&lf)||lf!=f||lg->wWidth!=w||lg->wHeight!=h||j>=12)ok=0;
     else all+=dxt_level_bytes(f,w,h);}
    if(ok){bytes=all;p->lplpSList[0]->dwReserved1=VX_DXTCHAIN;}}
   g->fpVidMem=DDHAL_PLEASEALLOC_BLOCKSIZE;g->dwBlockSizeX=(bytes+255UL)&~255UL;g->dwBlockSizeY=1;}
 }
 return DDHAL_DRIVER_NOTHANDLED; /* DirectDraw allocates the advertised heap. */
}
/* v10.466 (review): a surface can be locked again before it is unlocked (rectangles, other threads): the layout changes for the CPU
 * (DXT block rows packed, D24S8's depth and stencil merged) are made at the first Lock and undone at the last Unlock, not at each: a
 * second Lock packed packed rows again and the first Unlock spread them under the second. Outstanding Locks by surface (64; with the
 * table full a surface converts at each Lock and Unlock as before). */
#define LK_SURF 64
static DWORD lks_gbl[LK_SURF],lks_pid[LK_SURF],lks_n[LK_SURF];
static int lks_find(DDRAWI_DDRAWSURFACE_LCL *s,int add) {
 DWORD g=(DWORD)s->lpGbl,pid=GetCurrentProcessId();int i,f=-1;
 for(i=0;i<LK_SURF;i++){if(lks_gbl[i]==g&&lks_pid[i]==pid&&lks_n[i])return i;if(f<0&&!lks_n[i])f=i;}
 if(!add||f<0)return -1;lks_gbl[f]=g;lks_pid[f]=pid;lks_n[f]=0;return f;
}
/* .518 (audit): only the surfaces whose layout a Lock changes are counted (DXT rows packed, a D24S8's planes merged): ordinary video
 * surfaces no longer fill the table, and one that needs the count and finds the table full is refused (SURFACEBUSY) before anything is
 * converted (a full table converted at each Lock and Unlock: the first Unlock spread the rows under a mapping still open) */
static int lks_needs(DDRAWI_DDRAWSURFACE_LCL *s) {DWORD a,b,n;if(!s||!s->lpGbl)return 0;if(dxt_rows(s,&a,&b,&n))return 1;return (s->ddsCaps.dwCaps&DDSCAPS_ZBUFFER)&&s->lpGbl->dwReserved1!=VX_SPL_OWNED;}
/* 1: this Lock is the surface's first (convert), 0: another is outstanding, -1: the table full (refuse the Lock) */
static int lks_lock(DDRAWI_DDRAWSURFACE_LCL *s) {int i;if(!lks_needs(s))return 1;i=lks_find(s,1);if(i<0)return -1;return ++lks_n[i]==1;}
/* 1: this Unlock is the surface's last (convert back), 0: another Lock is still outstanding */
static int lks_unlock(DDRAWI_DDRAWSURFACE_LCL *s) {int i;if(!s||!s->lpGbl)return 1;i=lks_find(s,0);if(i<0)return 1;if(--lks_n[i]){return 0;}lks_gbl[i]=0;lks_pid[i]=0;return 1;}
static DWORD VXLock_impl(LPDDHAL_LOCKDATA p);
DWORD WINAPI VXLock(LPDDHAL_LOCKDATA p) {DWORD rc,t=VX_dd_tsc();if((!p||!p->lpDDSurface||!(p->lpDDSurface->ddsCaps.dwCaps&DDSCAPS_EXECUTEBUFFER))&&HOLD_SYNC(0)){if(p)p->ddRVal=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;} /* .575 */access_why=0xFFFFFFFFUL;rc=VXLock_impl(p);VX_frame_add(1,VX_dd_tsc()-t);if(p)VX_D3D_trace(93,(DWORD)p->ddRVal,rc,tr_off(p->lpDDSurface),p->dwFlags,p->lpDDSurface?p->lpDDSurface->ddsCaps.dwCaps:0);ft_count(0);if(p)note2d(22,p->ddRVal,rc,p->lpDDSurface?p->lpDDSurface->ddsCaps.dwCaps:0,p->dwFlags,access_why);return rc;}
static DWORD VXLock_impl(LPDDHAL_LOCKDATA p) {
 vx2d_surface s; DWORD rc;
 if(p) p->ddRVal=DD_OK; /* v10.58t: DDraw pre-sets WASSTILLDRAWING; NOTHANDLED needs DD_OK */
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 if(!(p->dwFlags&DDLOCK_READONLY)&&!front_wait(p->lpDDSurface)){p->ddRVal=DDERR_SURFACELOST;return DDHAL_DRIVER_HANDLED;} /* v10.384; v10.479 (review): as Blt */
 if(!describe(p->lpDDSurface,&s)) {
  /* D32 and offscreen colour formats unlike the desktop are outside the
   * 2D descriptor helper. They still require the shared-engine loss fence
   * before the runtime can return a CPU mapping. */
  if(!p->lpDDSurface||!(p->lpDDSurface->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY))return DDHAL_DRIVER_NOTHANDLED;
  /* v10.283 (review H1): every video surface's Lock waits for the engine (an asynchronous clear or blit, a 3D draw in flight), not only 3D ones;
   * v10.295: a depth surface locked for writing loses its Coarse Z records */
  rc=(p->dwFlags&DDLOCK_READONLY)?access(0):access_z(0,p->lpDDSurface);
  if(!(p->lpDDSurface->ddsCaps.dwCaps&(DDSCAPS_3DDEVICE|DDSCAPS_ZBUFFER|DDSCAPS_TEXTURE))) {
   if(rc==VX2D_LOST) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;}
   return DDHAL_DRIVER_NOTHANDLED;
  }
 } else rc=access(s.offset?0:1);
 /* v10.58v: rc 0 is VX2D_SKIP (engine not in desktop mode; nothing is in flight, ops are synchronous). It used to be
  * reported as WASSTILLDRAWING, which made DirectDraw retry Lock forever. Only a lost engine blocks CPU access. */
 if(rc==VX2D_LOST) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;}
 {int first=lks_lock(p->lpDDSurface); /* v10.466 */
 if(first<0){p->ddRVal=DDERR_SURFACEBUSY;return DDHAL_DRIVER_HANDLED;} /* .518: no room to count it: refused before any conversion */
 if(first)dxt_pack(p->lpDDSurface); /* v10.200 */
 if(p->lpDDSurface&&(p->lpDDSurface->ddsCaps.dwCaps&DDSCAPS_ZBUFFER)&&p->lpDDSurface->lpGbl->dwReserved1!=VX_SPL_OWNED) { /* v10.245 */
  if(first){splane_merge(p->lpDDSurface);p->lpDDSurface->lpGbl->dwReserved1=(p->dwFlags&DDLOCK_READONLY)?VX_SPL_RO:0;}
  else if(!(p->dwFlags&DDLOCK_READONLY))p->lpDDSurface->lpGbl->dwReserved1=0;}} /* a writing Lock among them: split at the last Unlock */
 return DDHAL_DRIVER_NOTHANDLED; /* runtime computes pointer and rect offset */
}
static DWORD VXUnlock_impl(LPDDHAL_UNLOCKDATA p);
DWORD WINAPI VXUnlock(LPDDHAL_UNLOCKDATA p) {DWORD rc,t=VX_dd_tsc();access_why=0xFFFFFFFFUL;rc=VXUnlock_impl(p);VX_frame_add(2,VX_dd_tsc()-t);if(p)VX_D3D_trace(94,(DWORD)p->ddRVal,rc,tr_off(p->lpDDSurface),0,p->lpDDSurface?p->lpDDSurface->ddsCaps.dwCaps:0);ft_count(1);if(p)note2d(23,p->ddRVal,rc,p->lpDDSurface?p->lpDDSurface->ddsCaps.dwCaps:0,0,access_why);return rc;}
static DWORD VXUnlock_impl(LPDDHAL_UNLOCKDATA p) {
 vx2d_surface s;
 if(p) p->ddRVal=DD_OK; /* v10.58t: DDraw pre-sets WASSTILLDRAWING; NOTHANDLED needs DD_OK */
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 if(p->lpDDSurface&&(p->lpDDSurface->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY)&&
    (p->lpDDSurface->ddsCaps.dwCaps&(DDSCAPS_3DDEVICE|DDSCAPS_ZBUFFER|DDSCAPS_TEXTURE))&&access(3)==VX2D_LOST) { /* v10.368 (audit N11): the loss check
     * only (status, action 3): Lock already waited for the engine, and the CPU's writes are done; it waited for the engine again */
  (void)lks_unlock(p->lpDDSurface); /* .518 (audit): the Lock's count retired with the surface lost too (its entry stayed) */
  p->ddRVal=DDERR_SURFACELOST;return DDHAL_DRIVER_HANDLED;
 }
 if(describe(p->lpDDSurface,&s)&&!s.offset) (void)access(2);
 if(!lks_unlock(p->lpDDSurface))return DDHAL_DRIVER_NOTHANDLED; /* v10.466: another Lock outstanding: converted back at the last */
 dxt_spread(p->lpDDSurface); /* v10.200 */
 if(p->lpDDSurface&&(p->lpDDSurface->ddsCaps.dwCaps&DDSCAPS_ZBUFFER)&&p->lpDDSurface->lpGbl->dwReserved1!=VX_SPL_OWNED) { /* v10.245 */
  if(p->lpDDSurface->lpGbl->dwReserved1!=VX_SPL_RO)splane_split(p->lpDDSurface);p->lpDDSurface->lpGbl->dwReserved1=0;}
 return DDHAL_DRIVER_NOTHANDLED;
}
static DWORD VXGetBltStatus_impl(LPDDHAL_GETBLTSTATUSDATA p);
DWORD WINAPI VXGetBltStatus(LPDDHAL_GETBLTSTATUSDATA p) {DWORD rc=VXGetBltStatus_impl(p);if(p)note2d(24,p->ddRVal,rc,p->lpDDSurface?p->lpDDSurface->ddsCaps.dwCaps:0,p->dwFlags,(DWORD)p->lpDDSurface);return rc;}
static DWORD VXGetBltStatus_impl(LPDDHAL_GETBLTSTATUSDATA p) {
 DWORD rc;
 if(p) p->ddRVal=DD_OK; /* v10.58t: DDraw pre-sets WASSTILLDRAWING; NOTHANDLED needs DD_OK */
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 rc=access(3); /* v10.142 (review O16, known bug P02): status only; the VxD answers BUSY instead of waiting while an asynchronous blit runs (an older VxD answers SKIP = idle) */
 p->ddRVal=rc==VX2D_LOST?DDERR_SURFACELOST:(rc==VX2D_BUSY&&(p->dwFlags&DDGBS_ISBLTDONE))?DDERR_WASSTILLDRAWING:DD_OK; /* v10.58v: SKIP means idle, not busy; CANBLT: a new blit can always be issued (it waits for the running one itself) */
 return DDHAL_DRIVER_HANDLED;
}
static DWORD vx_color_key(LPDDHAL_SETCOLORKEYDATA p);
DWORD WINAPI VXColorKey(LPDDHAL_SETCOLORKEYDATA p) { /* v10.333: the key noted (VXOVKEYON) with the answer */
 DWORD r=vx_color_key(p);
 if(p&&(p->dwFlags&DDCKEY_DESTOVERLAY))ovkey_note(3,p->dwFlags,&p->ckNew,p->ddRVal);
 return r;
}
static DWORD vx_color_key(LPDDHAL_SETCOLORKEYDATA p) {
 if(p) p->ddRVal=DD_OK; /* v10.58t: DDraw pre-sets WASSTILLDRAWING; NOTHANDLED needs DD_OK */
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 if((p->dwFlags&~(DDCKEY_SRCBLT|DDCKEY_DESTBLT|DDCKEY_DESTOVERLAY))||((p->dwFlags&DDCKEY_DESTOVERLAY)&&!hal->cb32.UpdateOverlay)||p->ckNew.dwColorSpaceLowValue!=p->ckNew.dwColorSpaceHighValue) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
 p->ddRVal=DD_OK; return DDHAL_DRIVER_HANDLED;
}

/* ---- v10.71: 8bpp. DirectDraw hands the HAL its palette objects; the entries are kept in the shared context (any process may call the HAL) and are
 * loaded into the DAC when a palette is attached to the primary surface. At 16 and 32bpp every palette callback declines (NOTHANDLED, DD_OK) and
 * DirectDraw keeps its own software palette. ---- */
static int pal_slot(DWORD key) {
 int i; for(i=0;i<8;i++) if(hal->paletteSlots[i].key==key) return i;
 return -1;
}
static int pal_upload(int slot,DWORD first,DWORD count) {
 vx_dac_req q; DWORD i,ok=0;
 memset(&q,0,sizeof(q)); q.cb=sizeof(q); q.abi=VX_DAC_ABI; q.first=first; q.count=count;
 for(i=0;i<count;i++) q.rgb[i]=hal->paletteSlots[slot].rgb[first+i];
 return ioctl(VX_IOCTL_DAC,&q,sizeof(q),&ok,4)&&ok;
}
static void pal_desktop(void) { /* the GDI palette goes back into the DAC */
 vx_dac_req q; DWORD ok=0;
 if(!hal||!hal->pFBHDA32||hal->pFBHDA32->bpp!=8||!hal->activePalette) return;
 memset(&q,0,sizeof(q)); q.cb=sizeof(q); q.abi=VX_DAC_ABI; q.first=0xFFFFFFFFUL;
 if(ioctl(VX_IOCTL_DAC,&q,sizeof(q),&ok,4)&&ok) {hal->activePalette=0; pal_pid=0;} /* v10.330 (review #158): the palette counts as given back only when the VxD put the GDI palette back; otherwise the next exit, detach or detach of the palette tries again */
}
DWORD WINAPI VXCreatePalette(LPDDHAL_CREATEPALETTEDATA p) {
 int i; DWORD j;
 if(p) p->ddRVal=DD_OK;
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 if(hal->pFBHDA32->bpp!=8||!p->lpDDPalette||!p->lpColorTable||!(p->lpDDPalette->dwFlags&DDRAWIPAL_256)||(p->lpDDPalette->dwFlags&(DDRAWIPAL_STORED_8INDEX|DDRAWIPAL_ALPHA))) return DDHAL_DRIVER_NOTHANDLED;
 i=pal_slot((DWORD)p->lpDDPalette);
 if(i<0) for(i=0;i<8;i++) if(!hal->paletteSlots[i].key) break;
 if(i>=8) return DDHAL_DRIVER_NOTHANDLED; /* no free slot: DirectDraw keeps a software palette */
 for(j=0;j<256;j++) hal->paletteSlots[i].rgb[j]=((DWORD)p->lpColorTable[j].peRed<<16)|((DWORD)p->lpColorTable[j].peGreen<<8)|p->lpColorTable[j].peBlue;
 hal->paletteSlots[i].key=(DWORD)p->lpDDPalette;
 return DDHAL_DRIVER_HANDLED;
}
DWORD WINAPI VXDestroyPalette(LPDDHAL_DESTROYPALETTEDATA p) {
 int i;
 if(p) p->ddRVal=DD_OK;
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 i=pal_slot((DWORD)p->lpDDPalette);
 if(i<0) return DDHAL_DRIVER_NOTHANDLED;
 if(hal->activePalette==hal->paletteSlots[i].key) {pal_desktop(); if(hal->activePalette) hal->activePalette=1;} /* v10.330: a failed put-back stays pending (1: no palette object has that address), but the freed palette's address is not kept: a new palette there would count as attached */
 hal->paletteSlots[i].key=0;
 return DDHAL_DRIVER_HANDLED;
}
DWORD WINAPI VXSetEntries(LPDDHAL_SETENTRIESDATA p) {
 int i; DWORD j;
 (void)HOLD_SYNC(4);
 if(p) p->ddRVal=DD_OK;
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 i=pal_slot((DWORD)p->lpDDPalette);
 if(i<0) return DDHAL_DRIVER_NOTHANDLED;
 if(p->dwBase>256||p->dwNumEntries>256-p->dwBase||(!p->lpEntries&&p->dwNumEntries)) {p->ddRVal=DDERR_INVALIDPARAMS; return DDHAL_DRIVER_HANDLED;}
 for(j=0;j<p->dwNumEntries;j++) hal->paletteSlots[i].rgb[p->dwBase+j]=((DWORD)p->lpEntries[j].peRed<<16)|((DWORD)p->lpEntries[j].peGreen<<8)|p->lpEntries[j].peBlue;
 if(p->dwNumEntries&&hal->activePalette==hal->paletteSlots[i].key&&!pal_upload(i,p->dwBase,p->dwNumEntries)) p->ddRVal=DDERR_SURFACELOST;
 return DDHAL_DRIVER_HANDLED;
}
DWORD WINAPI VXSetPalette(LPDDHAL_SETPALETTEDATA p) {
 int i;
 if(p) p->ddRVal=DD_OK;
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 if(hal->pFBHDA32->bpp!=8||!p->lpDDSurface||!(p->lpDDSurface->ddsCaps.dwCaps&DDSCAPS_PRIMARYSURFACE)) return DDHAL_DRIVER_NOTHANDLED;
 if(!p->Attach||!p->lpDDPalette) {pal_desktop(); return DDHAL_DRIVER_HANDLED;}
 i=pal_slot((DWORD)p->lpDDPalette);
 if(i<0) return DDHAL_DRIVER_NOTHANDLED;
 if(!pal_upload(i,0,256)) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;}
 hal->activePalette=hal->paletteSlots[i].key; pal_pid=GetCurrentProcessId();
 return DDHAL_DRIVER_HANDLED;
}
/* ---- v10.59: page flipping and vertical blank. The VxD calibrates the display start address against the BIOS the first time
 * a flip chain is asked for; when that fails the HAL says "no flip hardware" and applications use blits instead. ---- */
static int flip_call(DWORD cmd,DWORD arg,DWORD flags,DWORD *r) {
 vxf_request q; DWORD local[VXF_REPLY_DWORDS];
 if(!r) r=local;
 q.cb=sizeof(q); q.cmd=cmd; q.arg=arg; q.flags=flags;
 if(!ioctl(VX_IOCTL_FLIP,&q,sizeof(q),r,4*VXF_REPLY_DWORDS)) return 0;
 return 1;
}
/* v10.384: the flip returns before the display takes it (FLIP_ASYNC): a Lock or Blt into the surface it flipped away from waits in the
 * VxD until the display has moved on (VXF_FRONT); only asked while a flip may still be pending */
static DWORD hal_flip_pend;
/* v10.479 (review): returns 0 when a flip may be pending and the VxD did not answer (the wait not made): the caller fails with SURFACELOST
 * instead of writing into a surface the display may still show */
static int front_wait(LPDDRAWI_DDRAWSURFACE_LCL s) {
 DWORD r[VXF_REPLY_DWORDS];
 if(!hal_flip_pend||!s||!s->lpGbl||!hal||!(s->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY)||s->lpGbl->fpVidMem<hal->vramLinear) return 1;
 if(!flip_call(VXF_FRONT,s->lpGbl->fpVidMem-hal->vramLinear,0,r)) return 0;
 if(!r[VXF_R_IN_VB]) hal_flip_pend=0;
 return 1;
}
static int flip_ok(void) {
 DWORD r[VXF_REPLY_DWORDS];
 if(!flip_call(VXF_QUERY,0,0,r)) return 0;
 return r[VXF_R_RESULT]==VXF_OK&&r[VXF_R_STATE]==VXF_ST_DIRECT;
}
#define VX_DDOVER_BOBHARDWARE 0x01000000UL /* flags the runtime may pass with BOB; the V1 bob is hardware anyway */
#define VX_DDOVER_OVERRIDEBOBWEAVE 0x00400000UL
static int ov_surface(LPDDRAWI_DDRAWSURFACE_LCL s,vx_ovl_req *q);
static DWORD VXFlip_impl(LPDDHAL_FLIPDATA p);
static DWORD ov_src,ov_off; /* .532: defined with the overlay calls below */
void VX_rec_flip(void); /* .517 (vx_d3d_rec.inc) */
/* .575 (peer review of .572, finding 9; read by VXFLIPF through VX_D3D_QueryPhases words 356-361): what the runtime hands the Flip callback:
 * [0] calls, [1] with DDFLIP_DONOTWAIT (0x20), [2] with DDFLIP_NOVSYNC (0x08), [3] with DDFLIP_WAIT (0x01), [4] the flags of the last call,
 * [5] the last call's answer (ddRVal), [6] below */
DWORD vx_flip_seen[7]; /* .576: [6] flips that must not wait refused with the flip before still pending */
DWORD WINAPI VXFlip(LPDDHAL_FLIPDATA p) {DWORD t=VX_dd_tsc(),rc;if(HOLD_SYNC(2)){if(p)p->ddRVal=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;} /* .575 */if(p){vx_flip_seen[0]++;if(p->dwFlags&0x20UL)vx_flip_seen[1]++;if(p->dwFlags&0x08UL)vx_flip_seen[2]++;if(p->dwFlags&0x01UL)vx_flip_seen[3]++;vx_flip_seen[4]=p->dwFlags;}rc=VXFlip_impl(p);if(p)vx_flip_seen[5]=(DWORD)p->ddRVal;VX_rec_flip();VX_frame_add(5,VX_dd_tsc()-t);ft_flip();if(p)note2d(21,p->ddRVal,rc,p->lpSurfCurr?p->lpSurfCurr->ddsCaps.dwCaps:0,p->dwFlags,(DWORD)p->lpSurfCurr);return rc;}
static DWORD VXFlip_impl(LPDDHAL_FLIPDATA p) {
 vx2d_surface s; DWORD r[VXF_REPLY_DWORDS];
 if(p) p->ddRVal=DD_OK;
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 if(p->lpSurfTarg&&(p->lpSurfTarg->ddsCaps.dwCaps&DDSCAPS_OVERLAY)) { /* v10.106: overlay flip = the overlay's start address */
  vx_ovl_req q; DWORD o[VXO_REPLY_DWORDS];
  memset(&q,0,sizeof(q));
  if(!ov_surface(p->lpSurfTarg,&q)) {p->ddRVal=DDERR_NOTFLIPPABLE; return DDHAL_DRIVER_HANDLED;}
  q.cmd=VXO_FLIP;
  if(p->dwFlags&DDFLIP_ODD) q.flags|=VXO_FLD_ODD; else if(p->dwFlags&DDFLIP_EVEN) q.flags|=VXO_FLD_EVEN; /* v10.115: bob without a video port (DDCAPS2_CANFLIPODDEVEN) */
  if(!ov_call(&q,o)) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;}
  p->ddRVal=o[VXO_R_RESULT]==1?DDERR_GENERIC:DD_OK;
  /* .532 (review of .530, B5): the offset shown is now the target's (DestroySurface tests it); a refused flip hid the overlay */
  if(o[VXO_R_RESULT]==1){ov_src=0;ov_off=0xFFFFFFFFUL;}else if(ov_src)ov_off=q.offset;
  return DDHAL_DRIVER_HANDLED;
 }
 if(!describe(p->lpSurfTarg,&s)||s.pitch!=hal->pFBHDA32->pitch||s.width!=hal->pFBHDA32->width||s.height!=hal->pFBHDA32->height) {p->ddRVal=DDERR_NOTFLIPPABLE; return DDHAL_DRIVER_HANDLED;}
 /* .576 (peer review of .572, finding 9; board, VXFLIPF v1.0: the runtime hands DDFLIP_DONOTWAIT (0x20) to this callback and never DDFLIP_WAIT):
  * a flip that must not wait, asked while the display has not taken the flip before, answers DDERR_WASSTILLDRAWING and changes nothing (the
  * VxD waited for the flip before, up to a refresh); the same question GetFlipStatus asks */
 if((p->dwFlags&0x20UL)&&hal_flip_pend){DWORD q[VXF_REPLY_DWORDS];
  if(!flip_call(VXF_FRONT,0xFFFFFFFFUL,0,q)){p->ddRVal=DDERR_SURFACELOST;return DDHAL_DRIVER_HANDLED;}
  if(q[VXF_R_IN_VB]){vx_flip_seen[6]++;p->ddRVal=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}hal_flip_pend=0;}
 if(!flip_call(VXF_FLIP,s.offset,p->dwFlags,r)) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;}
 if(r[VXF_R_RESULT]==VXF_LOST) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;} /* v10.105 */
 if(r[VXF_R_RESULT]==VXF_OK||r[VXF_R_RESULT]==VXF_TIMEOUT) {
  flip_pid=s.offset?GetCurrentProcessId():0; p->ddRVal=DD_OK; hal_flip_pend=1; /* v10.384 */
 } else p->ddRVal=r[VXF_R_RESULT]==VXF_UNSUPPORTED?DDERR_NOTFLIPPABLE:DDERR_GENERIC;
 return DDHAL_DRIVER_HANDLED;
}
static DWORD VXGetFlipStatus_impl(LPDDHAL_GETFLIPSTATUSDATA p);
DWORD WINAPI VXGetFlipStatus(LPDDHAL_GETFLIPSTATUSDATA p) {DWORD rc=VXGetFlipStatus_impl(p);if(p)note2d(25,p->ddRVal,rc,p->lpDDSurface?p->lpDDSurface->ddsCaps.dwCaps:0,p->dwFlags,(DWORD)p->lpDDSurface);return rc;}
static DWORD VXGetFlipStatus_impl(LPDDHAL_GETFLIPSTATUSDATA p) {
 if(p) p->ddRVal=DD_OK; /* a flip is finished when Flip returns */
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 /* .518 (audit): CANFLIP too: a flip can finish at once only when the display has taken the last one (Flip with the last still pending
  * waits for it); asked for the flip chains' surfaces (not an overlay's, whose flips are their own) */
 if(hal_flip_pend&&(p->dwFlags&(DDGFS_ISFLIPDONE|DDGFS_CANFLIP))&&!(p->lpDDSurface&&(p->lpDDSurface->ddsCaps.dwCaps&DDSCAPS_OVERLAY))){DWORD r[VXF_REPLY_DWORDS]; /* v10.384: the display has not taken the last flip yet */
  if(!flip_call(VXF_FRONT,0xFFFFFFFFUL,0,r)){p->ddRVal=DDERR_SURFACELOST;return DDHAL_DRIVER_HANDLED;} /* v10.479 (review): the VxD did not answer; was DD_OK */
  if(r[VXF_R_IN_VB]){p->ddRVal=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}hal_flip_pend=0;}
 if(access(3)==VX2D_LOST) p->ddRVal=DDERR_SURFACELOST; /* v10.105: the engine is lost: a pending command may never finish. v10.368 (audit N10):
  * the status only (action 3): Flip returns when the flip is done, so there is nothing to wait for; it waited for every blit and draw in
  * flight (DirectDraw asks before Blt and Lock on a flip chain's surfaces). An engine still busy is not lost. */
 return DDHAL_DRIVER_HANDLED;
}
static DWORD ov_pid; /* process that showed the overlay: it hides it on exit */
static DWORD ov_src,ov_off=0xFFFFFFFFUL; /* v10.466: the surface shown and its offset (DestroySurface hides only that one) */
static int ov_surface(LPDDRAWI_DDRAWSURFACE_LCL s,vx_ovl_req *q) {
 LPDDRAWI_DDRAWSURFACE_GBL g; DWORD offset; int f;
 if(!s||!hal||!hal->pFBHDA32) return 0;
 g=s->lpGbl;
 if(!g||!(s->ddsCaps.dwCaps&DDSCAPS_OVERLAY)||!(s->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY)||(s->dwFlags&DDRAWISURF_INVALID)) return 0;
 f=ov_sfmt(s,g,ov_mask()); if(!f) return 0;
 if(g->fpVidMem<hal->vramLinear||g->lPitch<=0||!g->wWidth||!g->wHeight) return 0;
 offset=g->fpVidMem-hal->vramLinear;
 if(offset>=hal->vramSize) return 0;
 q->fmt=(DWORD)(f-1); q->offset=offset; q->pitch=(DWORD)g->lPitch; q->sw=g->wWidth; q->sh=g->wHeight;
 return 1;
}
static DWORD vx_update_overlay(LPDDHAL_UPDATEOVERLAYDATA p);
DWORD WINAPI VXUpdateOverlay(LPDDHAL_UPDATEOVERLAYDATA p) { /* v10.333: the keys noted (VXOVKEYON) with the answer */
 DWORD r=vx_update_overlay(p);
 if(p&&VXOVKEYON&&!(p->dwFlags&DDOVER_HIDE)) {
  if(p->dwFlags&DDOVER_KEYDESTOVERRIDE)ovkey_note(1,p->dwFlags,&p->overlayFX.dckDestColorkey,p->ddRVal);
  else if((p->dwFlags&DDOVER_KEYDEST)&&p->lpDDDestSurface)ovkey_note(2,p->dwFlags,&p->lpDDDestSurface->ddckCKDestOverlay,p->ddRVal);
  if(p->dwFlags&DDOVER_KEYSRCOVERRIDE)ovkey_note(4,p->dwFlags,&p->overlayFX.dckSrcColorkey,p->ddRVal);
  else if((p->dwFlags&DDOVER_KEYSRC)&&p->lpDDSrcSurface)ovkey_note(4,p->dwFlags,&p->lpDDSrcSurface->ddckCKSrcOverlay,p->ddRVal);
 }
 return r;
}
static DWORD vx_update_overlay(LPDDHAL_UPDATEOVERLAYDATA p) {
 vx_ovl_req q; DWORD r[VXO_REPLY_DWORDS]; const DDCOLORKEY *ck=NULL;
 if(p) p->ddRVal=DD_OK;
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 memset(&q,0,sizeof(q));
 if(p->dwFlags&DDOVER_HIDE) {
  q.cmd=VXO_HIDE; memset(r,0,sizeof(r)); p->ddRVal=ov_call(&q,r)&&r[VXO_R_RESULT]!=1?DD_OK:DDERR_GENERIC; if(p->ddRVal==DD_OK){ov_src=0;ov_off=0xFFFFFFFFUL;} return DDHAL_DRIVER_HANDLED; /* v10.330 (review #156): a failed call or a refusal is said */
 }
 /* no source key, alpha, automatic flip, bob/weave, effects (mirror, deinterlace, arithmetic stretch) */
 if(p->dwFlags&~(DDOVER_SHOW|DDOVER_KEYDEST|DDOVER_KEYDESTOVERRIDE|DDOVER_KEYSRC|DDOVER_KEYSRCOVERRIDE|DDOVER_BOB|DDOVER_INTERLEAVED|VX_DDOVER_BOBHARDWARE|VX_DDOVER_OVERRIDEBOBWEAVE|DDOVER_ADDDIRTYRECT|DDOVER_REFRESHDIRTYRECTS|DDOVER_REFRESHALL|DDOVER_DDFX|DDOVER_ALPHASRCCONSTOVERRIDE)) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
 if((p->dwFlags&DDOVER_DDFX)&&p->overlayFX.dwDDFX) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
 if(!ov_surface(p->lpDDSrcSurface,&q)) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
 if((p->dwFlags&DDOVER_KEYDESTOVERRIDE)) ck=&p->overlayFX.dckDestColorkey;
 else if(p->dwFlags&DDOVER_KEYDEST) {if(!p->lpDDDestSurface) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;} ck=&p->lpDDDestSurface->ddckCKDestOverlay;}
 if(ck) {q.flags|=VXO_KEY; q.key=ck->dwColorSpaceLowValue;}
 if((p->dwFlags&(DDOVER_KEYSRC|DDOVER_KEYSRCOVERRIDE))) { /* source key: a YUV pixel value; the YUY2 pixel is Y0 U Y1 V (low byte first) */
  const DDCOLORKEY *sk=(p->dwFlags&DDOVER_KEYSRCOVERRIDE)?&p->overlayFX.dckSrcColorkey:&p->lpDDSrcSurface->ddckCKSrcOverlay;
  DWORD lo=sk->dwColorSpaceLowValue,hi=sk->dwColorSpaceHighValue;
  if(q.fmt==VXO_FMT_YUY2) {lo=((lo&0xFFUL)<<16)|(lo&0xFF00UL)|((lo>>24)&0xFFUL); hi=((hi&0xFFUL)<<16)|(hi&0xFF00UL)|((hi>>24)&0xFFUL);}
  q.flags|=VXO_SRCKEY; q.ckl=lo&0xFFFFFFUL; q.ckh=hi&0xFFFFFFUL;
 }
 if(p->dwFlags&DDOVER_ALPHASRCCONSTOVERRIDE) { /* constant alpha of the overlay over the desktop (mask bit 10) */
  DWORD bd=p->overlayFX.dwAlphaSrcConstBitDepth,mx,av;
  if(!(ov_mask()&1024UL)) {p->ddRVal=DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
  if(bd<1||bd>8) bd=8;
  mx=(1UL<<bd)-1UL; av=p->overlayFX.dwAlphaSrcConst; if(av>mx) av=mx;
  q.flags|=VXO_ALPHA; q.key=(q.key&0xFFFFFFUL)|((av*255UL/mx)<<24);
 }
 if(p->dwFlags&DDOVER_BOB) {q.flags|=VXO_BOB; if(p->dwFlags&DDOVER_INTERLEAVED) q.flags|=VXO_INTERLEAVED;}
 q.cmd=VXO_UPDATE;
 q.sl=p->rSrc.left; q.st=p->rSrc.top; q.sr=p->rSrc.right; q.sb=p->rSrc.bottom;
 q.dl=p->rDest.left; q.dt=p->rDest.top; q.dr=p->rDest.right; q.db=p->rDest.bottom;
 if(!ov_call(&q,r)) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;}
 if(r[VXO_R_RESULT]==1) {p->ddRVal=(r[VXO_R_REASON]==8||r[VXO_R_REASON]==9)?DDERR_INVALIDRECT:DDERR_UNSUPPORTED; return DDHAL_DRIVER_HANDLED;}
 ov_pid=GetCurrentProcessId(); ov_src=(DWORD)p->lpDDSrcSurface; ov_off=q.offset; p->ddRVal=DD_OK; return DDHAL_DRIVER_HANDLED;
}
DWORD WINAPI VXSetOverlayPosition(LPDDHAL_SETOVERLAYPOSITIONDATA p) {
 vx_ovl_req q; DWORD r[VXO_REPLY_DWORDS];
 if(p) p->ddRVal=DD_OK;
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 memset(&q,0,sizeof(q)); q.cmd=VXO_MOVE; q.dl=p->lXPos; q.dt=p->lYPos;
 if(!ov_call(&q,r)) {p->ddRVal=DDERR_SURFACELOST; return DDHAL_DRIVER_HANDLED;}
 p->ddRVal=r[VXO_R_RESULT]==1?DDERR_UNSUPPORTED:DD_OK;
 return DDHAL_DRIVER_HANDLED;
}
/* an overlay surface that goes away must not stay on the screen */
static DWORD VXDestroySurface_impl(LPDDHAL_DESTROYSURFACEDATA p);
DWORD WINAPI VXDestroySurface(LPDDHAL_DESTROYSURFACEDATA p) {DWORD t=VX_dd_tsc(),rc;(void)HOLD_SYNC(3);rc=VXDestroySurface_impl(p);VX_frame_add(4,VX_dd_tsc()-t);if(p)note2d(30,p->ddRVal,rc,p->lpDDSurface?p->lpDDSurface->ddsCaps.dwCaps:0,0,(DWORD)p->lpDDSurface);return rc;}
static DWORD VXDestroySurface_impl(LPDDHAL_DESTROYSURFACEDATA p) {
 if(!p)return DDHAL_DRIVER_NOTHANDLED;
 p->ddRVal=DD_OK;
 if(!vx_bind_hal(p->lpDD))return DDHAL_DRIVER_NOTHANDLED;
 /* v10.279 (review): the memory goes back to DirectDraw's heap when this returns: a 3D draw still in flight on it (a render target, a
  * texture) is retired first, so nothing the engine does lands in memory that is no longer the surface's. v10.426 (review M02): retired
  * while the surface is still known to the HAL, then forgotten (it was forgotten first) */
 if(p->lpDDSurface&&!(p->lpDDSurface->ddsCaps.dwCaps&DDSCAPS_SYSTEMMEMORY)) (void)access_z(0,p->lpDDSurface); /* v10.295: a depth surface's Coarse Z records go with it */
 if(!VX_D3D_forget_surface(p->lpDDSurface)){p->ddRVal=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 if(p->lpDDSurface&&p->lpDDSurface->lpGbl){int i;for(i=0;i<LK_SURF;i++)if(lks_gbl[i]==(DWORD)p->lpDDSurface->lpGbl){lks_n[i]=0;lks_gbl[i]=0;lks_pid[i]=0;}} /* v10.466: its outstanding Locks go with it */
 if(p->lpDDSurface&&(p->lpDDSurface->ddsCaps.dwCaps&DDSCAPS_OVERLAY)) { /* v10.466 (review): only the overlay being shown (it hid
  * whatever overlay was up when any overlay surface went, another program's or a back buffer's) */
  vx_ovl_req q; DWORD r[VXO_REPLY_DWORDS];
  DWORD off=p->lpDDSurface->lpGbl&&p->lpDDSurface->lpGbl->fpVidMem>=hal->vramLinear?(DWORD)(p->lpDDSurface->lpGbl->fpVidMem-hal->vramLinear):0xFFFFFFFEUL;
  if((DWORD)p->lpDDSurface==ov_src||off==ov_off) {memset(&q,0,sizeof(q)); q.cmd=VXO_HIDE; (void)ov_call(&q,r); ov_src=0; ov_off=0xFFFFFFFFUL;}
 }
 return DDHAL_DRIVER_NOTHANDLED;
}
static DWORD VXWaitVB_impl(LPDDHAL_WAITFORVERTICALBLANKDATA p);
DWORD WINAPI VXWaitVB(LPDDHAL_WAITFORVERTICALBLANKDATA p) {DWORD rc=VXWaitVB_impl(p);if(p)note2d(28,p->ddRVal,rc,0,p->dwFlags,0);return rc;}
static DWORD VXWaitVB_impl(LPDDHAL_WAITFORVERTICALBLANKDATA p) {
 DWORD r[VXF_REPLY_DWORDS],arg;
 if(p) p->ddRVal=DD_OK;
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 if(p->dwFlags==DDWAITVB_I_TESTVB) arg=0; else if(p->dwFlags&DDWAITVB_BLOCKEND) arg=2; else if(p->dwFlags&DDWAITVB_BLOCKBEGIN) arg=1; else return DDHAL_DRIVER_NOTHANDLED;
 if(!flip_call(VXF_WAITVB,arg,0,r)||r[VXF_R_RESULT]==VXF_UNSUPPORTED) return DDHAL_DRIVER_NOTHANDLED;
 if(!arg) p->bIsInVB=r[VXF_R_IN_VB];
 p->ddRVal=DD_OK; return DDHAL_DRIVER_HANDLED;
}
/* v10.73: GetScanLine from the scan line counter; offered only when the registry gate SCANLINE=1 is set and the counter answers. */
DWORD WINAPI VXGetScanLine(LPDDHAL_GETSCANLINEDATA p) {
 DWORD r[VXF_REPLY_DWORDS],i;
 if(p) p->ddRVal=DD_OK;
 if(!p||!vx_bind_hal(p->lpDD)) return DDHAL_DRIVER_NOTHANDLED;
 for(i=0;i<3;i++) {
  if(!flip_call(VXF_SCAN,0,0,r)||r[VXF_R_RESULT]!=VXF_OK) return DDHAL_DRIVER_NOTHANDLED;
  if(r[VXF_S_STATUS]==1) {p->dwScanLine=r[VXF_S_LINE]; p->ddRVal=DD_OK; return DDHAL_DRIVER_HANDLED;}
  if(r[VXF_S_STATUS]==2||r[VXF_S_STATUS]==3) {p->dwScanLine=0; /* 3: the counter is past the last visible line, i.e. in the vertical blank interval (the status port bit covers only part of it) */ p->ddRVal=DDERR_VERTICALBLANKINPROGRESS; return DDHAL_DRIVER_HANDLED;}
 }
 return DDHAL_DRIVER_NOTHANDLED;
}
/* Leaving exclusive mode: show the desktop again whatever the application did. The runtime still carries on afterwards. */
static DWORD VXSetExclusive_impl(LPDDHAL_SETEXCLUSIVEMODEDATA p);
DWORD WINAPI VXSetExclusive(LPDDHAL_SETEXCLUSIVEMODEDATA p) {DWORD rc=VXSetExclusive_impl(p);if(p)note2d(29,p->ddRVal,rc,0,p->dwEnterExcl,0);return rc;}
static DWORD VXSetExclusive_impl(LPDDHAL_SETEXCLUSIVEMODEDATA p) {
 if(p) p->ddRVal=DD_OK;
 if(p) {DWORD v=0xFD000000UL|(p->dwEnterExcl?1UL:0UL),o[4]; (void)ioctl(VX_IOCTL_2D_CTRL,&v,4,o,16);} /* v10.95: the VxD may run blits asynchronously while a DirectDraw exclusive mode lasts */
 if(p&&!p->dwEnterExcl&&flip_pid) {(void)flip_call(VXF_RESET,0,0,NULL); flip_pid=0;}
 if(p&&!p->dwEnterExcl) pal_desktop(); /* v10.71 */
 return DDHAL_DRIVER_NOTHANDLED;
}
DWORD vx_init_seen[3];
DWORD WINAPI DriverInit(LPVOID ptr) {
 DWORD i; DEVMODEA dm; vx_status_t s; vx2d_stats st;
 /* .581 (VX800 board: DirectDraw ran without the HAL and no log said why): vx_init_seen[0] calls, [1] where the last one ended (1-7 the
  * check that refused it, 0x100 accepted), [2] the VxD's stage | failure << 8 | VGA mode << 16 | depth << 24 at that call; VXREG prints them */
 vx_init_seen[0]++; vx_init_seen[2]=0;
 if(!ptr) {vx_init_seen[1]=1; return 0;}
 hal=ptr;
 if(hal->dwSize!=sizeof(*hal)||!hal->pFBHDA32) {vx_init_seen[1]=2; return 0;}
 if(!ioctl(VX_IOCTL_STATUS,NULL,0,&s,sizeof(s))||s.magic!=VX_MAGIC) {vx_init_seen[1]=3; return 0;}
 vx_init_seen[2]=(s.stage&0xFFUL)|((s.failure&0xFFUL)<<8)|((s.vga_mode?1UL:0UL)<<16)|((s.bpp&0xFFUL)<<24);
 if(s.stage!=6||s.failure||s.vga_mode) {vx_init_seen[1]=4; return 0;}
 if(!ioctl(VX_IOCTL_2D_STATS,NULL,0,&st,sizeof(st))||st.abi!=VX2D_ABI) {vx_init_seen[1]=5; return 0;}
 if(s.bpp!=8&&s.bpp!=16&&s.bpp!=32) {vx_init_seen[1]=6; return 0;}
 if((DWORD)hal->pFBHDA32->vram_pm32!=s.fb_linear) {vx_init_seen[1]=7; return 0;}
 vx_init_seen[1]=0x100;
 hal->hInstance=(DWORD)instance;
 memset(&hal->cb32,0,sizeof(hal->cb32));
 hal->cb32.Blt=VXBlit; hal->cb32.CanCreateSurface=VXCanCreate; hal->cb32.CreateSurface=VXCreate;
 hal->cb32.Lock=VXLock; hal->cb32.Unlock=VXUnlock; hal->cb32.GetBltStatus=VXGetBltStatus; hal->cb32.SetColorKey=VXColorKey;
 hal->cb32.flags=DDHALINFO_ISPRIMARYDISPLAY|DDHALINFO_MODEXILLEGAL;
 hal->createPalette32=VXCreatePalette; hal->destroyPalette32=VXDestroyPalette; hal->setEntries32=VXSetEntries; hal->setPalette32=VXSetPalette; /* v10.71: set at every depth, each declines outside 8bpp (a game may switch to 8bpp later) */
 /* v10.59: flip chain and vertical blank, only when the VxD verified them (the DRV advertises DDSCAPS_FLIP from cb32.Flip) */
 {
  DWORD r[VXF_REPLY_DWORDS]; int ok; ok=flip_call(VXF_QUERY,0,0,r);
  if(ok&&r[VXF_R_RESULT]==VXF_OK&&r[VXF_R_STATE]==VXF_ST_DIRECT) {
   hal->cb32.Flip=VXFlip; hal->cb32.GetFlipStatus=VXGetFlipStatus; hal->cb32.SetExclusiveMode=VXSetExclusive;
  }
  if(ok&&r[VXF_R_VBLANK]==1) hal->cb32.WaitForVerticalBlank=VXWaitVB;
 }
 /* v10.106: overlay, only when ACCEL_OVERLAY is set in the VxD (the DRV then advertises the overlay capabilities from these pointers). Flip is needed for overlay chains even when the display cannot flip. */
 hal->cb32.UpdateOverlay=NULL; hal->cb32.SetOverlayPosition=NULL; hal->cb32.DestroySurface=NULL;
 if(ov_mask()&3) {
  hal->cb32.UpdateOverlay=VXUpdateOverlay; hal->cb32.SetOverlayPosition=VXSetOverlayPosition; hal->cb32.DestroySurface=VXDestroySurface;
  if(!hal->cb32.Flip) {hal->cb32.Flip=VXFlip; hal->cb32.GetFlipStatus=VXGetFlipStatus;}
 }
 hal->getScanLine32=NULL;
 {
  DWORD r[VXF_REPLY_DWORDS];
  if(flip_call(VXF_SCAN,0,0,r)&&r[VXF_R_RESULT]==VXF_OK&&r[VXF_S_GATE]&&r[VXF_S_STATUS]!=0) hal->getScanLine32=VXGetScanLine; /* v10.73 */
 }
 VX_D3D_init(hal);
 if(hal->d3dhal_global)hal->cb32.DestroySurface=VXDestroySurface;
 hal->modes_count=0;
 for(i=0;i<4096&&hal->modes_count<DISP_MODES_MAX;i++) {
  DDHALMODEINFO2_t *m;
  memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm);
  if(!EnumDisplaySettingsA(NULL,i,&dm)) break;
  if(dm.dmBitsPerPel!=8&&dm.dmBitsPerPel!=16&&dm.dmBitsPerPel!=32) continue;
  m=&hal->modes[hal->modes_count++]; memset(m,0,sizeof(*m));
  m->dwWidth=dm.dmPelsWidth; m->dwHeight=dm.dmPelsHeight; m->dwBPP=dm.dmBitsPerPel;
  m->lPitch=((m->dwWidth*(m->dwBPP/8))+7)&~7UL; m->wRefreshRate=0;
  if(m->dwBPP==8) m->wFlags=DDMODEINFO_PALETTIZED;
  else {m->dwRBitMask=m->dwBPP==16?0xF800:0xFF0000; m->dwGBitMask=m->dwBPP==16?0x7E0:0xFF00; m->dwBBitMask=m->dwBPP==16?31:255;}
 }
 /* Ensure the current mode is represented even with a sparse registry. */
 for(i=0;i<hal->modes_count;i++) if(hal->modes[i].dwWidth==s.width&&hal->modes[i].dwHeight==s.height&&hal->modes[i].dwBPP==s.bpp) break;
 if(i==hal->modes_count&&i<DISP_MODES_MAX) {
  DDHALMODEINFO2_t *m=&hal->modes[hal->modes_count++]; memset(m,0,sizeof(*m));
  m->dwWidth=s.width; m->dwHeight=s.height; m->dwBPP=s.bpp; m->lPitch=s.video_pitch;
  if(s.bpp==8) m->wFlags=DDMODEINFO_PALETTIZED;
  else {m->dwRBitMask=s.bpp==16?0xF800:0xFF0000; m->dwGBitMask=s.bpp==16?0x7E0:0xFF00; m->dwBBitMask=s.bpp==16?31:255;}
 }
 return 1;
}
BOOL WINAPI DllMain(HINSTANCE h,DWORD why,LPVOID reserved) {
 (void)reserved;
 if(why==DLL_PROCESS_ATTACH) {instance=h; DisableThreadLibraryCalls(h); dev_attach(GetCurrentProcessId()); VX_D3D_attach(1);} /* v10.319: this process's callback-lock token (vx_d3d_hal.c) */
 if(why==DLL_PROCESS_DETACH) {int i;VX_D3D_attach(0);for(i=0;i<8;i++)if(lock_pid[i]==GetCurrentProcessId())lock_pid[i]=0;} /* v10.435 */
 if(why==DLL_PROCESS_DETACH&&flip_pid==GetCurrentProcessId()) {(void)flip_call(VXF_RESET,0,0,NULL); flip_pid=0;}
 if(why==DLL_PROCESS_DETACH&&ov_pid&&ov_pid==GetCurrentProcessId()) {vx_ovl_req q; DWORD r[VXO_REPLY_DWORDS]; memset(&q,0,sizeof(q)); q.cmd=VXO_HIDE; (void)ov_call(&q,r); ov_pid=0;} /* v10.106 */
 if(why==DLL_PROCESS_DETACH) {DWORD me=GetCurrentProcessId(); int i,mine=0; for(i=0;i<DEV_SLOTS;i++) if(dev_pid[i]==me) mine=1; if(mine) {if(device_pid!=me) {device_pid=me; device=dev_for(me);} if(pal_pid==me) pal_desktop(); /* v10.140: not on every exit (DDHELP and other HAL callers have slots too) */ dev_close(me); device=INVALID_HANDLE_VALUE; device_pid=0;}} /* v10.139 (review F11) */
 return TRUE;
}
