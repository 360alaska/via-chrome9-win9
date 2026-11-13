/* VXSMPCAL v1.2 (driver .524: the RTC alone, the time-out clock removed). v1.1 (driver .521): every setting twice, on the sampler's two clocks: the RTC's interrupt (IRQ 8, 1024 a second, .521:
 * a hardware interrupt, taken wherever the processor is) and the VMM's time-outs (as before). PASS: on the RTC, every setting with
 * interrupts on within 3 points of the truth, counted and time-weighted; the time-outs reported, not judged. v1.0 (board): the ticks came every 4.5-5 ms (the desktop's timer; GTA3 runs it at 1 ms: 259,428
 * samples in 259.4 s), and the work alternated with a fixed period well under that: the samples aliased with it (0.2 ms chunks: 4.7%
 * counted for 29.9% true; 1 ms chunks from 1.7% to 41.9%), whatever the weighting; the 5 ms chunks, longer than the tick, read right (26%
 * counted, 30% time-weighted, every one). v1.1 asks for the 1 ms timer as the game has it (timeBeginPeriod(1)) and gives each chunk a
 * random length (0.5 to 1.5 times the setting's, ring 3 and the VxD apart: the same 70/30 on average), so no fixed period is left to
 * alias with; the tick rate reached is reported. The tick gaps by where they landed are what tell held ticks apart: a tick held in ring 3
 * and given at the VxD's entry comes after a longer gap than the ticks that land in ring 3.
 * v1.0 (driver .520): calibrates the VxD's sampling profiler (VX_IOCTL_SAMPLE) against a known split of time.
 *
 * The question (GTA3 on .519): the sampler put about 75% of the render thread's samples inside the VxD, while the VxD's own entry/exit
 * timing (TSC), the HAL's timing of its VxD calls and VX3DN_submit's per-packet time all put it at about 26% - a 3x over-count that held
 * even in menus. The suspect: a tick held back while the thread runs ring-3 code and delivered at its next ring-0 entry (our
 * DeviceIoControl), so it lands "in the VxD" though it stands for ring-3 time. .520's sampler also weights each sample by the time since
 * the tick before it; this tool checks both ways against the truth.
 *
 * The test: this thread alternates ring-3 work and a spin inside the VxD (VX_IOCTL_SAMPLE in[0] = 3: the VxD spins the given
 * microseconds and marks this thread as the render thread), 70% / 30% of the time, in chunks of 0.2, 1 and 5 ms of VxD time, for 2 s a
 * setting. The ring-3 work comes in four forms and the VxD's in three, because the deferral may depend on what the code does:
 *   ring 3: a plain integer loop; x87 arithmetic (the HAL's area and cull tests use doubles); SSE arithmetic with stores to a cached
 *   buffer (the HAL's vertex conversion and packet writes); SSE with stores to video memory (write-combined: a locked DirectDraw
 *   surface, DDLOCK_NOSYSLOCK);
 *   the VxD: a plain loop; MOVNTI copies and CLFLUSH over a buffer as the XP ring's copy and flush do (needs SSE2); the same with
 *   interrupts off (a tick due then can only land when they are back on, still in the VxD).
 * The truth: the VxD reports the TSC cycles it spun; ring-3 time is the rest of the setting's wall time (TSC). The sampler's answer:
 * the render thread's samples inside the VxD over all its samples, counted (the old way) and time-weighted (.520). Also the ticks' gaps
 * by where they landed: held ticks show as long gaps before ticks that land in the VxD.
 * PASS: for every setting with interrupts on, the time-weighted share within 3 points of the truth. The interrupts-off settings are
 * reported, not judged (no sampler can see inside them). Run it alone (not with VXD3DMON, which owns the sampler while it watches) on
 * the desktop; it takes about 80 s. Results to the screen and VXSMPCAL.LOG beside the program. */
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "vx_smp.h"

typedef unsigned long U32;
static FILE *logf;
static void say(const char *f,...) {static char b[2048];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);if(logf){fputs(b,logf);fflush(logf);}}
static HANDLE vxd=INVALID_HANDLE_VALUE;
static vx_smp s0,s1;
U32 rdtsc(void);
#pragma aux rdtsc = 0x0F 0x31 value [eax] modify exact [eax edx];
U32 cpuid1_edx(void);
#pragma aux cpuid1_edx = 0xB8 0x01 0x00 0x00 0x00 0x53 0x0F 0xA2 0x5B value [edx] modify exact [eax ecx edx];
/* SSE: xmm0 = [eax]; xmm1 = [eax + 16]; xmm0 = xmm0 * xmm1 + xmm1; [edx] = xmm0; [edx + 16] = xmm1 (MOVUPS loads and stores) */
void sse_step(const void *src,void *dst);
#pragma aux sse_step = 0x0F 0x10 0x00 0x0F 0x10 0x48 0x10 0x0F 0x59 0xC1 0x0F 0x58 0xC1 0x0F 0x11 0x02 0x0F 0x11 0x4A 0x10 parm [eax] [edx] modify exact [];

static int smp_read(vx_smp *o) {DWORD in[3]={2,0,0},got=0;return DeviceIoControl(vxd,VX_IOCTL_SAMPLE,in,12,o,sizeof(*o),&got,0)&&got==sizeof(*o)&&o->version==VX_SMP_VERSION;}
static U32 vxd_spin(U32 us,U32 kind) {DWORD in[3],o[1]={0},got=0;in[0]=3;in[1]=us;in[2]=kind;DeviceIoControl(vxd,VX_IOCTL_SAMPLE,in,12,o,4,&got,0);return o[0];}

static volatile U32 sink;static float fbuf[64*1024/4+16];static double dsink;
static unsigned char *wc;static U32 wc_bytes;static int have_sse;
static void r3_work(U32 kind,U32 cyc) {
 U32 t=rdtsc(),k=0;
 switch(kind){
  case 0:while(rdtsc()-t<cyc){U32 j,x=sink;for(j=0;j<64;j++)x=x*1664525UL+1013904223UL;sink=x;}break;
  case 1:{double a=1.0001,b=0.9999,c=0;while(rdtsc()-t<cyc){U32 j;for(j=0;j<32;j++){c+=(a-b)*(b+c*0.5);if(c>1e6)c=0;}}dsink=c;}break;
  case 2:while(rdtsc()-t<cyc){U32 j;for(j=0;j<32;j++){sse_step(&fbuf[(k*8)&16383],&fbuf[((k*8)+4096)&16383]);k++;}}break;
  default:{U32 lim=wc_bytes/32;while(rdtsc()-t<cyc){U32 j;for(j=0;j<32;j++){sse_step(&fbuf[(k*8)&16383],wc+(k%lim)*32);k++;}}}break;}
}
static const char *r3n[4]={"plain loop","x87","SSE, cached stores","SSE, WC stores"},*vn[3]={"plain loop","MOVNTI + CLFLUSH","interrupts off"};
static int fails,ran;static U32 lcg=12345;
static U32 jit(U32 v) {lcg=lcg*1664525UL+1013904223UL;return v/2+(U32)(((double)(lcg>>8)/16777216.0)*v);} /* v1.1: 0.5 to 1.5 times v */
static double tick_ms;static int judge;
static void one(U32 r3,U32 vk,U32 chunk_us,U32 khz) {
 U32 vcyc=0,t0,t1,v=chunk_us,r3c=(U32)((double)chunk_us*7.0/3.0*khz/1000.0),k;double tr,inr,inv,cn,ct,tw,cnt;
 U32 a=VX_SMP_RWAIT,b=VX_SMP_RVXD;
 vxd_spin(50,0); /* this thread marked the render thread before the counts are read */
 if(!smp_read(&s0)){say("   the sampler could not be read\n");fails++;return;}
 t0=rdtsc();
 do{r3_work(r3,jit(r3c));vcyc+=vxd_spin(jit(v),vk);}while((rdtsc()-t0)<2000UL*khz); /* v1.1: random chunk lengths */
 t1=rdtsc();
 if(!smp_read(&s1)){say("   the sampler could not be read\n");fails++;return;}
 tr=(double)(U32)(t1-t0);inv=vcyc;inr=tr-inv;
 cn=0;cnt=0;ct=0;tw=0;
 for(k=0;k<5;k++){U32 dn=s1.n_all[k]-s0.n_all[k],dt=s1.tw_all[k]-s0.tw_all[k];cnt+=dn;tw+=dt;if(k==a||k==b){cn+=dn;ct+=dt;}}
 {double truth=100.0*inv/tr,counted=cnt?100.0*cn/cnt:0,weighted=tw?100.0*ct/tw:0;int ok=!judge||vk==2||(weighted>truth-3.0&&weighted<truth+3.0&&counted>truth-3.0&&counted<truth+3.0);
  U32 g0=0,g1=0;double m0,m1;for(k=0;k<8;k++){g0+=s1.gap_n[0][k]-s0.gap_n[0][k];g1+=s1.gap_n[1][k]-s0.gap_n[1][k];}
  m0=g0?(s1.gap_t[0]-s0.gap_t[0])*1024.0/khz/g0:0;m1=g1?(s1.gap_t[1]-s0.gap_t[1])*1024.0/khz/g1:0;
  say("   ring 3 %-19s VxD %-16s %4.1f ms chunks: in the VxD %5.1f%% true, %5.1f%% counted, %5.1f%% time-weighted; ticks in ring 3 %5lu (%.2f ms apart), in the VxD %5lu (%.2f ms apart) %s\n",
   r3n[r3],vn[vk],chunk_us/1000.0,truth,counted,weighted,g0,m0,g1,m1,vk==2||!judge?"(reported)":ok?"ok":"FAIL");
  if(judge){ran++;if(!ok)fails++;}
  {U32 n=0;for(k=0;k<VX_SMP_CATS;k++)n+=s1.n_all[k]-s0.n_all[k];if(n)tick_ms=tr/khz/n;}}
}
int main(void) {
 DWORD in[4],o[2],got=0;U32 khz,r3,vk,c,clk;static const U32 chunks[3]={200,1000,5000};
 LPDIRECTDRAW dd=0;LPDIRECTDRAWSURFACE sf=0;DDSURFACEDESC sd;HMODULE ddl;char path[MAX_PATH],*p;
 GetModuleFileNameA(0,path,sizeof(path));p=strrchr(path,'\\');if(p)strcpy(p+1,"VXSMPCAL.LOG");logf=fopen(path,"w");
 say("VXSMPCAL v1.2 (driver .524): the VxD sampler against a known split, 70%% ring 3 / 30%% in the VxD, chunks of random length\n");
 {HMODULE w=LoadLibraryA("WINMM.DLL");UINT (WINAPI *tbp)(UINT)=w?(UINT (WINAPI*)(UINT))GetProcAddress(w,"timeBeginPeriod"):0;say("the system timer at 1 ms (timeBeginPeriod, as the game): %s\n",tbp&&tbp(1)==0?"asked":"not available");}
 vxd=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);
 if(vxd==INVALID_HANDLE_VALUE){say("FAIL: VX85598.VXD not open\n");return 1;}
 _try{U32 d=cpuid1_edx();if(d&(1UL<<25)){sse_step(fbuf,fbuf+8);have_sse=1;}}_except(EXCEPTION_EXECUTE_HANDLER){have_sse=0;}
 say("SSE %s\n",have_sse?"available":"not available (its settings are left out)");
 ddl=LoadLibraryA("DDRAW.DLL");
 if(ddl&&have_sse){HRESULT (WINAPI *cr)(GUID*,LPDIRECTDRAW*,IUnknown*)=(HRESULT (WINAPI*)(GUID*,LPDIRECTDRAW*,IUnknown*))GetProcAddress(ddl,"DirectDrawCreate");
  if(cr&&SUCCEEDED(cr(0,&dd,0))&&SUCCEEDED(IDirectDraw_SetCooperativeLevel(dd,0,DDSCL_NORMAL))){
   memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;sd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;sd.dwWidth=256;sd.dwHeight=64;
   if(SUCCEEDED(IDirectDraw_CreateSurface(dd,&sd,&sf,0))){memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
    if(SUCCEEDED(IDirectDrawSurface_Lock(sf,0,&sd,DDLOCK_WAIT|DDLOCK_NOSYSLOCK|DDLOCK_WRITEONLY,0))&&sd.lpSurface&&sd.lPitch>0){wc=(unsigned char*)sd.lpSurface;wc_bytes=(U32)sd.lPitch*sd.dwHeight&~31UL;}}}}
 say("video memory for the write-combined stores: %s\n",wc?"locked":"not available (its settings are left out)");
 for(clk=1;clk==1;clk--){int rtc=0; /* v1.2 (.524): the RTC alone (the driver's time-out clock removed) */
  in[0]=1;in[1]=0;in[2]=0;in[3]=clk;
  if(!DeviceIoControl(vxd,VX_IOCTL_SAMPLE,in,16,o,8,&got,0)||got!=8||o[0]!=VX_SMP_VERSION||!o[1]){say("FAIL: the sampler did not start (a driver before .521, VXD3DMON running, or IRQ 8 / the RTC's periodic interrupt taken)\n");return 1;}
  if(!smp_read(&s0)){say("FAIL: the sampler could not be read\n");return 1;}
  rtc=s0.clk==1;khz=s0.tsc_khz?s0.tsc_khz:1000000UL;
  if(clk==1&&!rtc){say("the RTC's interrupt: not available (IRQ 8 taken, or its periodic interrupt in use); the time-outs judged instead\n");in[0]=0;DeviceIoControl(vxd,VX_IOCTL_SAMPLE,in,12,o,8,&got,0);judge=2;continue;}
  judge=clk==1||judge==2;
  say("\n%s (TSC %.1f MHz, measured by the VxD)%s:\n",clk==1?"THE SAMPLER ON THE RTC'S INTERRUPT (IRQ 8, 1024 a second)":"THE SAMPLER ON THE VMM'S TIME-OUTS",khz/1000.0,judge?"":", reported only");
  for(c=0;c<3;c++){say("VxD chunks of %.1f ms on average (0.5 to 1.5 times):\n",chunks[c]/1000.0);
   for(r3=0;r3<4;r3++){if(r3>=2&&!have_sse)continue;if(r3==3&&!wc)continue;
    for(vk=0;vk<3;vk++)one(r3,vk,chunks[c],khz);}}
  say("the sampler's ticks came every %.2f ms in the last setting\n",tick_ms);
  in[0]=0;DeviceIoControl(vxd,VX_IOCTL_SAMPLE,in,12,o,8,&got,0);}
 if(sf){IDirectDrawSurface_Unlock(sf,0);IDirectDrawSurface_Release(sf);}if(dd)IDirectDraw_Release(dd);
 {HMODULE w=GetModuleHandleA("WINMM.DLL");UINT (WINAPI *tep)(UINT)=w?(UINT (WINAPI*)(UINT))GetProcAddress(w,"timeEndPeriod"):0;if(tep)tep(1);}
 say("%s: %d settings, %d off by more than 3 points (interrupts-off settings not judged)\n",fails?"FAIL":"PASS",ran,fails);
 say("Judged: the RTC's clock (the time-outs when the RTC is not available), counted and time-weighted shares within 3 points of the true one. A held tick shows as a longer gap before the ticks that land on one side.\n");
 CloseHandle(vxd);if(logf)fclose(logf);return fails?1:0;
}
