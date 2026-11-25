/* VXHANG v1.4 (driver .785: it starts the VxD's sampler itself (E784B's run had 0 samples: only VXD3DMON started it), so the file shows where the stopped thread is executing; the sampler is stopped when this program ends). VXHANG v1.3 (driver .784B: E783's DX8H.BAT left no VXHANG.TXT: Direct3D 8's caps were read before the first write, and that call waited on DirectDraw, held by the stopped program. The caps are read by a thread of their own now, 20 seconds in; the file says when that call has not come back). VXHANG v1.2 (driver .772: the VxD sampler's last 64 samples (where each thread was executing, while VXD3DMON's sampler runs) and the modules of every program that has Direct3D 8 loaded, so an address names its module). VXHANG v1.1 (driver .771: Direct3D 8's caps for the HAL device read once at the start; words 636-643, the callback lock's holder and the DirectDraw callback entered). VXHANG v1.0 (driver .770): a watcher for a program that stops inside the driver. A console program that prints nothing once started (nothing here needs
 * the 16-bit side, which a stuck DirectDraw call holds): once a second it reads the HAL's status words (VX_D3D_QueryPhases, the copy of
 * VXDDRAW.DLL in the system folder: its data is shared with the game's) and every two seconds writes VXHANG.TXT whole: a line a second
 * of words 620-643 (where a DrawPrimitives2 call is: vx_d3d_core.c vx_d3d_bc), then every status word now and five seconds ago, and
 * four of the VxD's read-only answers. Runs for the seconds given (default 180), then ends. Read-only: nothing is changed in the driver.
 *   VXHANG [seconds] */
#include <windows.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vx_smp.h"
#define NW 646
#define KEEP 240
static DWORD ring[KEEP][28],hist[6][NW],nline,nhist;
static DWORD caps8[76],caps_hr=0xFFFFFFFFUL;
/* Direct3D 8's own answer for adapter 0's HAL device (IDirect3D8::GetDeviceCaps, vtable slot 13): what a program sizes its buffers by */
static void read_caps(void) {
 HMODULE d=LoadLibraryA("d3d8.dll");void *(WINAPI *mk)(UINT);void **o;
 if(!d){caps_hr=0xFFFFFFFEUL;return;}
 mk=(void *(WINAPI *)(UINT))GetProcAddress(d,"Direct3DCreate8");if(!mk)return;
 o=(void **)mk(220);if(!o){caps_hr=0xFFFFFFFDUL;return;}
 {void **vt=*(void ***)o;
  caps_hr=((DWORD (WINAPI *)(void *,UINT,DWORD,void *))vt[13])(o,0,1,caps8);
  ((DWORD (WINAPI *)(void *))vt[2])(o);}
}
static char modtxt[24000];static DWORD modlen,modtaken;
/* every module (base, size, path) of every other process that has D3D8.DLL loaded; taken while the program is still running */
static void take_modules(void) {
 HANDLE ps=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);PROCESSENTRY32 pe;DWORD me=GetCurrentProcessId();
 if(ps==INVALID_HANDLE_VALUE)return;
 pe.dwSize=sizeof(pe);
 if(Process32First(ps,&pe))do{HANDLE ms;MODULEENTRY32 e;int has=0;DWORD at=modlen;
  if(pe.th32ProcessID==me)continue;
  ms=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,pe.th32ProcessID);if(ms==INVALID_HANDLE_VALUE)continue;
  e.dwSize=sizeof(e);
  if(Module32First(ms,&e))do{const char *n=e.szModule;if(!lstrcmpiA(n,"D3D8.DLL"))has=1;
   if(modlen+400<sizeof(modtxt))modlen+=(DWORD)sprintf(modtxt+modlen,"  %08lX %08lX %s\r\n",(DWORD)e.modBaseAddr,(DWORD)e.modBaseSize,e.szExePath);
   e.dwSize=sizeof(e);}while(Module32Next(ms,&e));
  CloseHandle(ms);
  if(!has)modlen=at;else modtaken++;
 }while(Process32Next(ps,&pe));
 CloseHandle(ps);
}
static void samples(HANDLE v,FILE *f) {
 DWORD k,n=0,e[64][2];memset(e,0,sizeof(e));
 for(k=0;k<32;k++){DWORD in[2],o[8],got=0;in[0]=0xBC;in[1]=0x45A0000UL+k;memset(o,0,sizeof(o));
  if(v==INVALID_HANDLE_VALUE||!DeviceIoControl(v,0x85500022UL,in,8,o,32,&got,0)||(o[0]&0xFF)!=0xBC){fprintf(f,"sampler's last samples: no answer\r\n");return;}
  n=o[2];e[k*2][0]=o[3];e[k*2][1]=o[4];e[k*2+1][0]=o[5];e[k*2+1][1]=o[6];}
 fprintf(f,"sampler's last 64 samples (%lu taken; newest last; category 0 render thread in the VxD waiting, 1 in the VxD, 2 HAL, 3 other 32-bit, 4 16-bit, 5 another thread 32-bit, 6 another 16-bit, 7 no render thread; R = the render thread; selector:address):\r\n",n);
 for(k=0;k<64;k++){const DWORD *x=e[(n+k)&63];fprintf(f,"%s%lu%s %04lX:%08lX",(k&7)?"  ":"\r\n  ",x[0]&0xFF,(x[0]&0x100)?"R":" ",x[0]>>16,x[1]);}
 fprintf(f,"\r\n");
}
static void texdbg(HANDLE v,DWORD a,DWORD b,int two,FILE *f) {
 DWORD in[2],o[8],got=0;int k;in[0]=a;in[1]=b;memset(o,0,sizeof(o));
 if(v==INVALID_HANDLE_VALUE||!DeviceIoControl(v,0x85500022UL,in,two?8:4,o,32,&got,0)){fprintf(f,"VxD %02lX %lX: no answer\r\n",a,b);return;}
 fprintf(f,"VxD %02lX %lX:",a,b);for(k=0;k<8;k++)fprintf(f," %08lX",o[k]);fprintf(f,"\r\n");
}
static int smp_on;
static volatile LONG caps_state; /* 0 not started, 1 in the call, 2 back */
static DWORD WINAPI caps_thread(LPVOID p) {(void)p;Sleep(20000);caps_state=1;read_caps();caps_state=2;return 0;}
int main(int argc,char **argv) {
 char dll[MAX_PATH];UINT n=GetSystemDirectoryA(dll,MAX_PATH-16);HMODULE m=0;DWORD (WINAPI *ph)(DWORD *,DWORD)=0;DWORD secs=180,t0,s;HANDLE v;
 static DWORD w[NW];
 if(argc>1&&atol(argv[1])>0)secs=(DWORD)atol(argv[1]);
 if(n&&n<MAX_PATH-16){strcpy(dll+n,dll[n-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");m=LoadLibraryA(dll);}
 if(m)ph=(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(m,"VX_D3D_QueryPhases");
 v=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);
 if(m&&v!=INVALID_HANDLE_VALUE){DWORD in[4],o[2],got=0,pe;const BYTE *b=(const BYTE*)m;pe=*(const DWORD*)(b+0x3C);
  in[0]=1;in[1]=(DWORD)b;in[2]=*(const DWORD*)(b+pe+24+56);in[3]=1;smp_on=DeviceIoControl(v,VX_IOCTL_SAMPLE,in,16,o,8,&got,0)&&got==8&&o[0]==VX_SMP_VERSION&&o[1];}
 {DWORD tid;HANDLE th=CreateThread(0,0,caps_thread,0,0,&tid);if(th)CloseHandle(th);}
 t0=GetTickCount();
 for(s=0;s<secs;s++){
  DWORD got=0,k;FILE *f;
  memset(w,0,sizeof(w));if(ph)got=ph(w,NW);
  {DWORD *r=ring[nline%KEEP];r[0]=GetTickCount()-t0;r[1]=got;for(k=0;k<26;k++)r[2+k]=w[620+k];nline++;}
  memcpy(hist[nhist%6],w,sizeof(w));nhist++;
  if(!modtaken&&s>=6&&!(s%4)&&w[621])take_modules(); /* once a program is drawing (DrawPrimitives2 calls counted), every 4 s until one with Direct3D 8 is found */
  if((s&1)||s+1==secs){
   f=fopen("VXHANG.TXT","wb");
   if(f){DWORD a=nline>KEEP?nline-KEEP:0,i;
    fprintf(f,"VXHANG v1.4 (driver .785): the sampler %s; %s; QueryPhases %s; %lu s of %lu\r\n",smp_on?"started":"NOT STARTED (IRQ 8 taken, or no answer)",m?dll:"VXDDRAW.DLL not loaded",ph?"found":"not found",s+1,secs);
    fprintf(f,"ms words | in-call calls place cmd-offset op|n<<8 cmds-this-call k0 split|more<<1 tn_draw range-tri call-start-tick last-answer failed err-offset submit tn_P | 636: lock-holder turned-away holder-line refused-line holder-thread since dd-callback(|100 returned) dd-entries last-lock-line lock-takes\r\n");
    for(i=a;i<nline;i++){const DWORD *r=ring[i%KEEP];fprintf(f,"%7lu %3lu |",r[0],r[1]);for(k=0;k<26;k++)fprintf(f," %lX",r[2+k]);fprintf(f,"\r\n");}
    fprintf(f,"tick now %08lX\r\n",GetTickCount());
    fprintf(f,"Direct3D 8's caps, asked for by this program 20 s in: %s\r\n",caps_state==0?"not asked yet":caps_state==1?"THE CALL HAS NOT COME BACK (Direct3D 8 / DirectDraw is held by another program)":"answered");fprintf(f,"Direct3D 8 GetDeviceCaps(HAL): result %08lX; the 76 words:",caps_hr);for(k=0;k<76;k++)fprintf(f,"%s%08lX",(k&7)?" ":"\r\n  ",caps8[k]);fprintf(f,"\r\n");
    samples(v,f);
    fprintf(f,"modules of the programs with Direct3D 8 loaded (base size path):\r\n");if(modlen)fwrite(modtxt,1,modlen,f);else fprintf(f,"  none taken yet\r\n");
    texdbg(v,0x75,0,0,f);texdbg(v,0xBC,0x44B,1,f);texdbg(v,0xBC,0x441,1,f);texdbg(v,0xB8,0,0,f);
    {const DWORD *now=hist[(nhist-1)%6],*old=nhist>=6?hist[nhist%6]:hist[0];
     fprintf(f,"every status word: index now five-seconds-ago (only the ones not 0 in either)\r\n");
     for(k=0;k<NW;k++)if(now[k]||old[k])fprintf(f,"%lu %lX %lX%s\r\n",k,now[k],old[k],now[k]!=old[k]?" *":"");}
    fclose(f);}
  }
  Sleep(1000);
 }
 if(smp_on){static vx_smp sm;DWORD in[3]={0,0,0},got=0;DeviceIoControl(v,VX_IOCTL_SAMPLE,in,12,&sm,sizeof(sm),&got,0);}
 return 0;
}
