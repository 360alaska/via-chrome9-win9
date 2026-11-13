/* VXFLIPF v1.2 (driver .577): the refresh rate line now has an answer to show (the driver fills DirectDraw's monitor frequency).
 * VXFLIPF v1.1 (driver .576): what DirectDraw's flip flags do on this driver. v1.1: pairs of flips with nothing drawn between (v1.0's
 * fill before every flip waited for the flip before, as a write to the old front must: its Flip calls never met a pending flip), and the
 * driver's count of refused flips (.576: a DONOTWAIT flip with the flip before still pending answers "still drawing"). v1.0:
 * A peer review of .572 asked two things the source cannot answer: whether DDFLIP_DONOTWAIT reaches the driver's Flip callback and is
 * then made to wait there (the callback waits for the engine and for the flip before; it never answers "still drawing"), and what the
 * advertised DDFLIP_NOVSYNC costs (the driver shows such a flip at the next refresh: .555). This program measures both; it changes
 * nothing. A full-screen 640 x 480 x 16 flip chain with one back buffer; each case 40 flips, the back buffer filled with a colour before
 * each (a small blit: the frame is light, so a flip follows the one before well inside a refresh):
 *   WAIT; DONOTWAIT (one try each: the answers counted); DONOTWAIT again after a 20 ms pause (the flip before is shown by then);
 *   NOVSYNC; WAIT | INTERVAL2.
 * For each: the answers, the median and longest time of a Flip call, and what the driver's callback saw (calls, the flags, its last
 * answer: VX_D3D_QueryPhases words 356-361). A DONOTWAIT call that answers DD_OK after most of a refresh was made to wait by the driver.
 * Results to VXFLIPF.LOG beside the program. About ten seconds; the screen changes mode and shows colours. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ddraw.h>
static FILE *logf;
static const GUID my_IID_IDirectDraw7={0x15e65ec0,0x3b9c,0x11d2,{0xb9,0x2f,0x00,0x60,0x97,0x97,0xea,0x5b}};
static void say(const char *f,...) {static char b[1024];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);if(logf){fputs(b,logf);fflush(logf);}}
static double now(void) {LARGE_INTEGER c,f;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&c);return (double)c.QuadPart*1000.0/(double)f.QuadPart;}
static int cmpd(const void *a,const void *b) {double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y;}
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProcA(h,m,w,l);}
typedef DWORD (WINAPI *qp_fn)(DWORD*,DWORD);
#ifndef DDFLIP_DONOTWAIT
#define DDFLIP_DONOTWAIT 0x00000020L
#endif
#ifndef DDFLIP_NOVSYNC
#define DDFLIP_NOVSYNC 0x00000008L
#endif
#ifndef DDFLIP_INTERVAL2
#define DDFLIP_INTERVAL2 0x02000000L
#endif
#define N 40
static LPDIRECTDRAWSURFACE7 front,back;static qp_fn qp;static DWORD w0[363],w1[363];
static void run(const char *name,DWORD flags,DWORD pause) {
 double t[N],lng=0;int k,ok=0,still=0,other=0;HRESULT last=0;DDBLTFX fx;
 if(qp)qp(w0,363);
 for(k=0;k<N;k++){double a;HRESULT hr;
  memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwFillColor=(DWORD)(k*2113+flags);
  while(IDirectDrawSurface7_Blt(back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx)==DDERR_WASSTILLDRAWING);
  if(pause)Sleep(pause);
  a=now();hr=IDirectDrawSurface7_Flip(front,NULL,flags);t[k]=now()-a;if(t[k]>lng)lng=t[k];last=hr;
  if(hr==DD_OK)ok++;else if(hr==DDERR_WASSTILLDRAWING)still++;else other++;}
 if(qp)qp(w1,363);
 qsort(t,N,sizeof(double),cmpd);
 say("%-34s DD_OK %2d, still drawing %2d, other %2d (last %08lX) | a call: median %6.2f ms, longest %6.2f ms | the callback: %lu calls, DONOTWAIT in %lu, NOVSYNC in %lu, WAIT in %lu, last flags %08lX, last answer %08lX\n",
  name,ok,still,other,(unsigned long)last,t[N/2],lng,w1[356]-w0[356],w1[357]-w0[357],w1[358]-w0[358],w1[359]-w0[359],w1[360],w1[361]);
 Sleep(60);
}
/* v1.1: a flip, then at once another with the flags under test, nothing drawn between: the second meets the first still pending */
static void pair(const char *name,DWORD flags) {
 double t[N],lng=0;int k,ok=0,still=0,other=0;HRESULT last=0;DDBLTFX fx;
 if(qp)qp(w0,363);
 for(k=0;k<N;k++){double a;HRESULT hr;
  memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwFillColor=(DWORD)(k*977+flags);
  while(IDirectDrawSurface7_Blt(back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx)==DDERR_WASSTILLDRAWING);
  while(IDirectDrawSurface7_Flip(front,NULL,DDFLIP_WAIT)==DDERR_WASSTILLDRAWING);
  a=now();hr=IDirectDrawSurface7_Flip(front,NULL,flags);t[k]=now()-a;if(t[k]>lng)lng=t[k];last=hr;
  if(hr==DD_OK)ok++;else if(hr==DDERR_WASSTILLDRAWING)still++;else other++;
  Sleep(40);}
 if(qp)qp(w1,363);
 qsort(t,N,sizeof(double),cmpd);
 say("%-34s DD_OK %2d, still drawing %2d, other %2d (last %08lX) | the second call: median %6.2f ms, longest %6.2f ms | the callback: %lu calls, DONOTWAIT in %lu, refused %lu\n",
  name,ok,still,other,(unsigned long)last,t[N/2],lng,w1[356]-w0[356],w1[357]-w0[357],w1[362]-w0[362]);
}
int main(void) {
 WNDCLASSA wc;HWND win;LPDIRECTDRAW7 dd=0;DDSURFACEDESC2 s;DDSCAPS2 caps;HRESULT hr;HMODULE h,hd;char path[MAX_PATH],*q;DDCAPS dc;
 HRESULT (WINAPI *create)(GUID*,LPVOID*,REFIID,IUnknown*);
 GetModuleFileNameA(0,path,sizeof(path));q=strrchr(path,'\\');if(q)strcpy(q+1,"VXFLIPF.LOG");logf=fopen(path,"w");
 say("VXFLIPF v1.2 (driver .577): what DirectDraw's flip flags do on this driver\n");
 hd=LoadLibraryA("DDRAW.DLL");create=hd?(HRESULT (WINAPI*)(GUID*,LPVOID*,REFIID,IUnknown*))GetProcAddress(hd,"DirectDrawCreateEx"):0;
 if(!create||create(NULL,(LPVOID*)&dd,&my_IID_IDirectDraw7,NULL)!=DD_OK||!dd){say("no DirectDraw 7\nFAILED\n");return 1;}
 h=GetModuleHandleA("VXDDRAW.DLL");if(!h)h=LoadLibraryA("VXDDRAW.DLL");qp=h?(qp_fn)GetProcAddress(h,"VX_D3D_QueryPhases"):0;
 memset(w0,0,sizeof(w0));
 if(!qp||qp(w0,363)<363)say("WARNING: the driver's counters are not there (VXDDRAW.DLL before .576): the callback's columns read 0\n");
 memset(&dc,0,sizeof(dc));dc.dwSize=sizeof(dc);IDirectDraw7_GetCaps(dd,&dc,NULL);
 say("the driver's caps2 %08lX: FLIPNOVSYNC %s, FLIPINTERVAL %s\n",(unsigned long)dc.dwCaps2,(dc.dwCaps2&0x00400000UL)?"advertised":"not advertised",(dc.dwCaps2&0x00200000UL)?"advertised":"not advertised");
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="VXFLIPF";RegisterClassA(&wc);
 win=CreateWindowA("VXFLIPF","VXFLIPF",WS_POPUP|WS_VISIBLE,0,0,640,480,0,0,wc.hInstance,0);
 if((hr=IDirectDraw7_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))!=DD_OK||(hr=IDirectDraw7_SetDisplayMode(dd,640,480,16,0,0))!=DD_OK){say("no full-screen 640x480x16: %08lX\nFAILED\n",(unsigned long)hr);return 1;}
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT;s.dwBackBufferCount=1;s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
 if((hr=IDirectDraw7_CreateSurface(dd,&s,&front,0))!=DD_OK){say("no flip chain: %08lX\nFAILED\n",(unsigned long)hr);goto out;}
 memset(&caps,0,sizeof(caps));caps.dwCaps=DDSCAPS_BACKBUFFER;
 if((hr=IDirectDrawSurface7_GetAttachedSurface(front,&caps,&back))!=DD_OK){say("no back buffer: %08lX\nFAILED\n",(unsigned long)hr);goto out;}
 {DWORD f=0;IDirectDraw7_GetMonitorFrequency(dd,&f);say("GetMonitorFrequency: %lu Hz%s\n",(unsigned long)f,f?"":" (WRONG: the driver does not say; .577 fills it)");}
 run("warm-up (WAIT)",DDFLIP_WAIT,0);
 run("WAIT",DDFLIP_WAIT,0);
 run("DONOTWAIT, one try, at once",DDFLIP_DONOTWAIT,0);
 run("DONOTWAIT, one try, after 20 ms",DDFLIP_DONOTWAIT,20);
 run("WAIT | NOVSYNC",DDFLIP_WAIT|DDFLIP_NOVSYNC,0);
 run("WAIT | INTERVAL2",DDFLIP_WAIT|DDFLIP_INTERVAL2,0);
 say("a flip, then at once another, nothing drawn between (the second meets the first still pending):\n");
 pair("second WAIT",DDFLIP_WAIT);
 pair("second DONOTWAIT",DDFLIP_DONOTWAIT);
 pair("second WAIT | NOVSYNC",DDFLIP_WAIT|DDFLIP_NOVSYNC);
 say("done\n");
out:
 if(front)IDirectDrawSurface7_Release(front);
 IDirectDraw7_RestoreDisplayMode(dd);IDirectDraw7_SetCooperativeLevel(dd,win,DDSCL_NORMAL);IDirectDraw7_Release(dd);DestroyWindow(win);
 if(logf)fclose(logf);
 return 0;
}
