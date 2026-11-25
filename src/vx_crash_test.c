/* VXCRASH v1.1 (.740; T739 on all three boards: run a-emu failed before anything crashed): with DirectDraw's acceleration off the child
 * asked for its depth buffer in video memory (DDERR_NOZBUFFERHW): the emulation runs use the software device's set-up (system memory),
 * as the second programs did; a second program whose front-buffer lock is refused (T739: 887601B3, DDERR_CANTLOCKSURFACE) logs what DCI
 * gives for the screen (the emulation reaches the primary through the display driver's DCI answer), for the driver's fix.
 * VXCRASH v1.0 (.736): crashes a full-screen Direct3D 7 program on demand and checks that the next program can still go full screen
 * (the crash write-up of .733: after GTA3 crashed, no full-screen program started until a reboot; SetCooperativeLevel answered
 * DDERR_HWNDALREADYSET and 3DMark D3DERR_DRIVERINTERNALERROR).
 *
 * Runs (each one a child program that is crashed, then the checks below):
 *   a      between frames, by an unhandled null write (as GTA3 died)
 *   b      mid-scene: between BeginScene and EndScene with draws queued
 *   c      inside a driver call: another thread faults while the main thread is in a DrawPrimitives2 (the callback lock held)
 *   d      as c, with the child made token 1 first (a process the driver's DLL never attached in: VX_D3D_TestSet 0x100)
 *   a-sw   as a, the child drawing with Direct3D's software RGB device: the driver's Direct3D callbacks never run, its DirectDraw ones do
 *   e      (v1.1) the child takes a 3D give-up on its own handle (as after a refused recovery) and dies: the next programs must have 3D
 *          again (the speed list's AD: the engine given back once the give-up's owner has gone)
 *   a-emu  as a, with DirectDraw's hardware acceleration off (HKLM\Software\Microsoft\DirectDraw EmulationOnly = 1, dxdiag's switch) for
 *          the run only: the driver's callbacks out entirely. The old value is put back after the run, and at the next start of
 *          VXCRASH if a run never finished (VXCRASH_OLD beside it holds the old value).
 * After each crash: the parent waits for the child to end (a wait over 2 s is logged; 60 s: it is ended), reads what the driver holds
 * (VXDDRAW.DLL's VX_D3D_CrashStats: the callback lock's holder, the held draws' process, the display start's owner, destroy and
 * exclusive-mode counts), then runs a second program 3 times (SetCooperativeLevel exclusive full screen, 640x480x16, a flip chain with
 * Z, a device, a frame drawn, flipped and read back) and a Direct3D 8 full-screen device once. Every step of every process is written to
 * VXCRASH.LOG as it happens (opened, written and closed: a stall leaves the last step), with its process ID and window.
 * Pass: every second program goes full screen and draws, with no step over 2 s; the Direct3D 8 device is created (hardware runs).
 * Command line: the runs to do (default all six); "dialog" keeps Windows' fault dialog (default off: SEM_NOGPFAULTERRORBOX, so the
 * child ends at once; with the dialog it ends when the dialog is closed, as GTA3 did). */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#define DIRECT3D_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#define VER "VXCRASH v1.1"
/* Open Watcom declares IID_IDirectDraw7 but its dxguid.lib omits it (as VXDX7); DirectDrawCreateEx from ddraw.dll */
static const GUID iid_dd7={0x15E65EC0UL,0x3B9C,0x11D2,{0xB9,0x2F,0,0x60,0x97,0x97,0xEA,0x5B}};
static HRESULT ddcreate(LPDIRECTDRAW7 *dd) {
 HMODULE m=LoadLibrary("ddraw.dll");HRESULT (WINAPI *c)(GUID*,void**,REFIID,IUnknown*)=m?(HRESULT (WINAPI*)(GUID*,void**,REFIID,IUnknown*))GetProcAddress(m,"DirectDrawCreateEx"):0;
 return c?c(0,(void**)dd,&iid_dd7,0):DDERR_UNSUPPORTED;
}
static char logpath[MAX_PATH],exepath[MAX_PATH];
static HWND wnd;
static void say(const char *fmt,...) {
 char b[1024];va_list a;int n;FILE *f;SYSTEMTIME t;GetLocalTime(&t);
 n=sprintf(b,"%02d:%02d:%02d.%03d [%08lX] ",t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,GetCurrentProcessId());
 va_start(a,fmt);_vsnprintf(b+n,sizeof(b)-n-1,fmt,a);va_end(a);b[sizeof(b)-1]=0;
 f=fopen(logpath,"a");if(f){fputs(b,f);fflush(f);fclose(f);}
 fputs(b,stdout);fflush(stdout);
}
/* VXDDRAW.DLL's exports (the driver's DLL; loaded already in a DirectDraw process, else loaded here) */
typedef DWORD (WINAPI *FSTATS)(DWORD*,DWORD);typedef DWORD (WINAPI *FTSET)(DWORD,DWORD);
static FSTATS stats_fn;static FTSET tset_fn;
static void dll(void) {
 HMODULE m=GetModuleHandle("VXDDRAW.DLL");if(!m)m=LoadLibrary("VXDDRAW.DLL");
 if(m){stats_fn=(FSTATS)GetProcAddress(m,"VX_D3D_CrashStats");tset_fn=(FTSET)GetProcAddress(m,"VX_D3D_TestSet");}
}
static int stats(DWORD *w) {memset(w,0,22*4);return stats_fn&&stats_fn(w,22)==22;}
static void show_stats(const char *when) {
 DWORD w[22];
 if(!stats(w)){say("%s: VX_D3D_CrashStats not found (VXDDRAW.DLL of .736 or later needed)\n",when);return;}
 say("%s: lock holder token %lu (process %08lX, thread %08lX, held %lu ms); refusals %lu, takeovers %lu; held draws' process %08lX\n",
  when,w[0],w[1],w[2],w[3],w[4],w[5],w[6]);
 say("   destroy calls %lu / destroy-all %lu / DestroyDDLocal %lu; queued %lu, done from the queue %lu, queue full %lu; reclaimed %lu\n",
  w[7],w[8],w[9],w[10],w[11],w[12],w[13]);
 say("   live contexts %lu, surface tables %lu; exclusive enters %lu, leaves %lu, leftovers given back %lu (last enter %08lX, leave %08lX); display start owner %08lX\n",
  w[14],w[15],w[16],w[17],w[18],w[19],w[20],w[21]);
 if(w[0]&&w[3]>2000)say("   the lock has been held over 2 s\n");
}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM a,LPARAM b) {if(m==WM_SETCURSOR){SetCursor(0);return 1;}return DefWindowProc(h,m,a,b);}
static void make_window(const char *title) {
 WNDCLASS c;memset(&c,0,sizeof(c));c.lpfnWndProc=wp;c.hInstance=GetModuleHandle(0);c.lpszClassName="VXCRASH";c.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
 RegisterClass(&c);wnd=CreateWindowEx(WS_EX_TOPMOST,"VXCRASH",title,WS_POPUP|WS_VISIBLE,0,0,640,480,0,0,c.hInstance,0);
}
static void pump(void) {MSG m;while(PeekMessage(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}}
typedef struct {float x,y,z,rhw;DWORD c,s;float u,v;} TLV;
static void tri(TLV *v,float cx,float cy,float r,DWORD c) {
 static const float co[3]={0.0f,-0.8660254f,0.8660254f},si[3]={-1.0f,0.5f,0.5f};int k;
 for(k=0;k<3;k++){v[k].x=cx+r*co[k];v[k].y=cy+r*si[k];v[k].z=0.5f;v[k].rhw=1;v[k].c=c;v[k].s=0;v[k].u=v[k].v=0;}
}
/* the full-screen set-up both programs make. dev_rgb: Direct3D's software device. Returns 0 at the first failing step, logged with its
 * time; a step over 2 s is logged as a stall (*slow set). */
typedef struct {LPDIRECTDRAW7 dd;LPDIRECTDRAWSURFACE7 front,back,z;LPDIRECT3D7 d3d;LPDIRECT3DDEVICE7 dev;} FS;
static int slow;
static int step(const char *what,HRESULT hr,DWORD t0) {
 DWORD ms=GetTickCount()-t0;
 say("   %-52s %08lX%s (%lu ms)\n",what,(DWORD)hr,hr==DDERR_HWNDALREADYSET?" DDERR_HWNDALREADYSET":hr==DDERR_EXCLUSIVEMODEALREADYSET?" DDERR_EXCLUSIVEMODEALREADYSET":
  hr==DDERR_WASSTILLDRAWING?" DDERR_WASSTILLDRAWING":"",ms);
 if(ms>2000){slow=1;say("   ^ over 2 s\n");}
 return SUCCEEDED(hr);
}
static int fs_open(FS *f,int dev_rgb) {
 DDSURFACEDESC2 s;DDSCAPS2 caps;DWORD t;D3DVIEWPORT7 vp;memset(f,0,sizeof(*f));
 say("   window %08lX\n",(DWORD)wnd);
 t=GetTickCount();if(!step("DirectDrawCreateEx",ddcreate(&f->dd),t))return 0;
 t=GetTickCount();if(!step("SetCooperativeLevel(EXCLUSIVE, FULLSCREEN)",IDirectDraw7_SetCooperativeLevel(f->dd,wnd,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN),t))return 0;
 t=GetTickCount();if(!step("SetDisplayMode(640, 480, 16)",IDirectDraw7_SetDisplayMode(f->dd,640,480,16,0,0),t))return 0;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT;s.dwBackBufferCount=1;
 s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX|DDSCAPS_3DDEVICE; /* the software device draws into it as well */
 t=GetTickCount();if(!step("CreateSurface(primary + 1 back buffer)",IDirectDraw7_CreateSurface(f->dd,&s,&f->front,0),t))return 0;
 memset(&caps,0,sizeof(caps));caps.dwCaps=DDSCAPS_BACKBUFFER;
 t=GetTickCount();if(!step("GetAttachedSurface(back buffer)",IDirectDrawSurface7_GetAttachedSurface(f->front,&caps,&f->back),t))return 0;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;s.dwWidth=640;s.dwHeight=480;
 s.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|(dev_rgb?DDSCAPS_SYSTEMMEMORY:DDSCAPS_VIDEOMEMORY); /* the software device's Z in system memory */s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);
 s.ddpfPixelFormat.dwFlags=DDPF_ZBUFFER;s.ddpfPixelFormat.dwZBufferBitDepth=16;s.ddpfPixelFormat.dwZBitMask=0xFFFF;
 t=GetTickCount();if(!step("CreateSurface(Z 16, 640x480)",IDirectDraw7_CreateSurface(f->dd,&s,&f->z,0),t))return 0;
 t=GetTickCount();if(!step("AddAttachedSurface(Z to the back buffer)",IDirectDrawSurface7_AddAttachedSurface(f->back,f->z),t))return 0;
 t=GetTickCount();if(!step("QueryInterface(IDirect3D7)",IDirectDraw7_QueryInterface(f->dd,&IID_IDirect3D7,(void**)&f->d3d),t))return 0;
 t=GetTickCount();if(!step(dev_rgb?"CreateDevice(RGB, back buffer)":"CreateDevice(HAL, back buffer)",
  IDirect3D7_CreateDevice(f->d3d,dev_rgb?&IID_IDirect3DRGBDevice:&IID_IDirect3DHALDevice,f->back,&f->dev),t))return 0;
 memset(&vp,0,sizeof(vp));vp.dwWidth=640;vp.dwHeight=480;vp.dvMaxZ=1;IDirect3DDevice7_SetViewport(f->dev,&vp);
 IDirect3DDevice7_SetRenderState(f->dev,D3DRENDERSTATE_ZENABLE,TRUE);IDirect3DDevice7_SetRenderState(f->dev,D3DRENDERSTATE_LIGHTING,FALSE);
 IDirect3DDevice7_SetRenderState(f->dev,D3DRENDERSTATE_CULLMODE,D3DCULL_NONE);
 return 1;
}
static void fs_close(FS *f) {
 if(f->dev)IDirect3DDevice7_Release(f->dev);if(f->d3d)IDirect3D7_Release(f->d3d);if(f->z)IDirectDrawSurface7_Release(f->z);
 if(f->front)IDirectDrawSurface7_Release(f->front);
 if(f->dd){IDirectDraw7_RestoreDisplayMode(f->dd);IDirectDraw7_SetCooperativeLevel(f->dd,wnd,DDSCL_NORMAL);IDirectDraw7_Release(f->dd);}
}
static HRESULT frame(FS *f,DWORD bg,DWORD fg,int tris,int end) {
 static TLV v[3*1000];int k;HRESULT hr;
 for(k=0;k<tris;k++)tri(v+3*k,320.0f+(float)((k*37)%200-100),240.0f+(float)((k*53)%160-80),60.0f+(float)(k%40),fg);
 if(FAILED(hr=IDirect3DDevice7_Clear(f->dev,0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,bg,1.0f,0)))return hr;
 if(FAILED(hr=IDirect3DDevice7_BeginScene(f->dev)))return hr;
 for(k=0;k<tris;k+=100)if(FAILED(hr=IDirect3DDevice7_DrawPrimitive(f->dev,D3DPT_TRIANGLELIST,D3DFVF_TLVERTEX,v+3*k,3*(tris-k<100?tris-k:100),0)))break;
 if(!end)return hr;
 if(SUCCEEDED(hr))hr=IDirect3DDevice7_EndScene(f->dev);else IDirect3DDevice7_EndScene(f->dev);
 if(SUCCEEDED(hr))hr=IDirectDrawSurface7_Flip(f->front,0,DDFLIP_WAIT);
 return hr;
}
static volatile LONG crash_now;static volatile DWORD main_tid;
static void die(const char *how) {say("   crashing now: %s\n",how);*(volatile int*)0=0x0BADF00D;}
/* case c / d: a thread that faults the moment the main thread holds the driver's callback lock */
static DWORD WINAPI crasher(void *a) {
 DWORD w[22],end=GetTickCount()+10000,me=GetCurrentProcessId(),n=0;(void)a;
 SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_TIME_CRITICAL);
 while(!crash_now)Sleep(1);
 while(GetTickCount()<end){n++;if(stats(w)&&w[0]&&w[1]==me&&w[2]==main_tid)die("a second thread, while the main thread is inside a driver callback (the lock held)");}
 say("   the main thread was not caught inside a callback in 10 s (%lu looks)\n",n);die("a second thread (the lock not caught held)");
 return 0;
}
static int child(const char *cs) {
 FS f;int k,rgb=strstr(cs,"sw")!=0||strstr(cs,"emu")!=0;char c=cs[0];HRESULT hr; /* v1.1: emu too */
 make_window("VXCRASH child");dll();
 say("child, run %s: process %08lX, window %08lX\n",cs,GetCurrentProcessId(),(DWORD)wnd);
 if(c=='d'){DWORD n=tset_fn?tset_fn(0x100,0):0xFFFFFFFFUL;say("   made token 1 (VX_D3D_TestSet 0x100): %lu entries cleared%s\n",n,tset_fn?"":" (VX_D3D_TestSet not found)");}
 if(!fs_open(&f,rgb)){say("   the child could not go full screen: nothing to crash\n");fs_close(&f);return 3;}
 for(k=0;k<30;k++){pump();if(FAILED(hr=frame(&f,0xFF000040UL,0xFF00C000UL,200,1))){say("   frame %d: %08lX\n",k,(DWORD)hr);break;}}
 say("   30 frames drawn and flipped\n");
 if(c=='a'){frame(&f,0xFF000040UL,0xFF00C000UL,200,1);die("between frames (an unhandled null write)");}
 if(c=='b'){frame(&f,0xFF000040UL,0xFFC0C000UL,1000,0);die("mid-scene (after BeginScene, 1000 triangles queued, before EndScene)");}
 if(c=='e'){HANDLE h=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);DWORD in[2],o[8],got=0;in[0]=0xBC;in[1]=0x4500000UL;memset(o,0,sizeof(o)); /* v1.1 run e */
  if(h!=INVALID_HANDLE_VALUE)DeviceIoControl(h,0x85500022UL,in,8,o,32,&got,0);
  say("   took a 3D give-up on its own handle (TEXDBG BC 4500000): given up %lu\n",o[2]);
  die("with the engine given up (the speed list's AD: the next program must have 3D again)");}
 {DWORD tid;main_tid=GetCurrentThreadId();CreateThread(0,0,crasher,0,0,&tid);InterlockedExchange(&crash_now,1);
  for(;;){pump();frame(&f,0xFF400000UL,0xFF00C0C0UL,1000,1);}}
}
/* v1.1: what DCI gives for the screen (DCIMAN32: DCIOpenProvider, DCICreatePrimary, DCIBeginAccess), logged when the front lock is refused */
typedef struct {DWORD dwSize,dwDCICaps,dwCompression,dwMask[3],dwWidth,dwHeight;LONG lStride;DWORD dwBitCount,dwOffSurface;WORD wSelSurface,wReserved;
 DWORD dwReserved1,dwReserved2,dwReserved3;void *BeginAccess,*EndAccess,*DestroySurface;} dci_info;
static void dci_probe(void) {
 HMODULE m=LoadLibraryA("DCIMAN32.DLL");HDC (WINAPI *op)(void);void (WINAPI *cl)(HDC);int (WINAPI *cr)(HDC,dci_info**);
 int (WINAPI *be)(dci_info*,int,int,int,int);void (WINAPI *en)(dci_info*);void (WINAPI *de)(dci_info*);HDC dc;dci_info *d=0;int rc;
 if(!m){say("   DCI: DCIMAN32.DLL not loaded\n");return;}
 op=(HDC (WINAPI*)(void))GetProcAddress(m,"DCIOpenProvider");cl=(void (WINAPI*)(HDC))GetProcAddress(m,"DCICloseProvider");
 cr=(int (WINAPI*)(HDC,dci_info**))GetProcAddress(m,"DCICreatePrimary");be=(int (WINAPI*)(dci_info*,int,int,int,int))GetProcAddress(m,"DCIBeginAccess");
 en=(void (WINAPI*)(dci_info*))GetProcAddress(m,"DCIEndAccess");de=(void (WINAPI*)(dci_info*))GetProcAddress(m,"DCIDestroy");
 if(!op||!cl||!cr||!be||!en||!de){say("   DCI: DCIMAN32.DLL lacks an export\n");FreeLibrary(m);return;}
 dc=op();say("   DCI: DCIOpenProvider %08lX\n",(DWORD)dc);if(!dc){FreeLibrary(m);return;}
 rc=cr(dc,&d);say("   DCI: DCICreatePrimary %d, surface %08lX\n",rc,(DWORD)d);
 {DWORD e[10];HDC sd=GetDC(0);memset(e,0,sizeof(e));
  if(ExtEscape(sd,0x8F73,0,0,sizeof(e),(LPSTR)e)>0)say("   DCI: the driver's record (escape 8F73): creates %lu, DIB engine %ld, form %lu, offset %08lX selector %04lX, written selector %04lX offset %08lX\n",
   e[0],(long)e[1],e[4],e[6],e[7],e[8],e[9]);ReleaseDC(0,sd);}
 if(rc>=0&&d&&!IsBadReadPtr(d,sizeof(*d))){int r2;
  say("   DCI: %lux%lu, %lu bpp, stride %ld, caps %08lX, offset %08lX, selector %04X\n",d->dwWidth,d->dwHeight,d->dwBitCount,d->lStride,d->dwDCICaps,d->dwOffSurface,d->wSelSurface);
  r2=be(d,0,0,(int)d->dwWidth,(int)d->dwHeight);say("   DCI: DCIBeginAccess (whole screen) %d\n",r2);if(r2>=0)en(d);de(d);}
 cl(dc);FreeLibrary(m);
}
/* the second program: full screen, a red triangle, flipped, the front buffer read back. Exit code 0 pass, 10 + the step failed, 9 slow. */
static int second(int n,int rgb) {
 FS f;int ok=0;HRESULT hr;DWORD t;
 make_window("VXCRASH second program");
 say("second program %d: process %08lX, window %08lX\n",n,GetCurrentProcessId(),(DWORD)wnd);
 if(fs_open(&f,rgb)) {
  TLV v[3];DDSURFACEDESC2 s;tri(v,320,240,200,0xFFFF0000UL);
  t=GetTickCount();hr=IDirect3DDevice7_Clear(f.dev,0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF0000FFUL,1.0f,0);
  if(SUCCEEDED(hr))hr=IDirect3DDevice7_BeginScene(f.dev);
  if(SUCCEEDED(hr)){hr=IDirect3DDevice7_DrawPrimitive(f.dev,D3DPT_TRIANGLELIST,D3DFVF_TLVERTEX,v,3,0);IDirect3DDevice7_EndScene(f.dev);}
  if(step("a frame (Clear, BeginScene, DrawPrimitive, EndScene)",hr,t)) {
   t=GetTickCount();if(step("Flip",IDirectDrawSurface7_Flip(f.front,0,DDFLIP_WAIT),t)) {
    memset(&s,0,sizeof(s));s.dwSize=sizeof(s);t=GetTickCount();
    if(step("Lock(front, read)",IDirectDrawSurface7_Lock(f.front,0,&s,DDLOCK_WAIT|DDLOCK_READONLY,0),t)) {
     WORD in=*(WORD*)((BYTE*)s.lpSurface+250*s.lPitch+320*2),out=*(WORD*)((BYTE*)s.lpSurface+8*s.lPitch+8*2);
     IDirectDrawSurface7_Unlock(f.front,0);
     ok=(in>>11)>=28&&((in>>5)&63)<4&&(in&31)<4&&(out&31)>=28&&(out>>11)<4;
     say("   read back: inside %04X (red F800), outside %04X (blue 001F): %s\n",in,out,ok?"right":"WRONG");}
    else dci_probe();}}
 }
 fs_close(&f);DestroyWindow(wnd);
 if(!ok){say("second program %d: FAILED\n",n);return 10;}
 if(slow){say("second program %d: drew, but a step took over 2 s\n",n);return 9;}
 say("second program %d: passed\n",n);return 0;
}
/* Direct3D 8: full-screen 640x480 R5G6B5 with D16, a clear and a Present (vtable indices as VXAB) */
#define VT(o,i) (((void**)(*(void***)(o)))[i])
typedef struct {DWORD bw,bh,bfmt,bcount,ms,swap;HWND wnd;DWORD windowed,autoz,zfmt,flags,rate,interval;} PP8;
static int d3d8(void) {
 HMODULE m;void *(WINAPI *create)(UINT);void *d,*dev=0;PP8 pp;HRESULT hr;DWORD t;
 make_window("VXCRASH Direct3D 8");
 say("Direct3D 8 program: process %08lX, window %08lX\n",GetCurrentProcessId(),(DWORD)wnd);
 m=LoadLibrary("d3d8.dll");create=m?(void*(WINAPI*)(UINT))GetProcAddress(m,"Direct3DCreate8"):0;
 if(!create){say("   d3d8.dll not found: skipped\n");DestroyWindow(wnd);return 0;}
 d=create(220);if(!d)d=create(120);if(!d){say("   Direct3DCreate8 failed\n");DestroyWindow(wnd);return 11;}
 memset(&pp,0,sizeof(pp));pp.bw=640;pp.bh=480;pp.bfmt=23;pp.bcount=1;pp.swap=1;pp.wnd=wnd;pp.windowed=0;pp.autoz=1;pp.zfmt=80;
 t=GetTickCount();hr=((HRESULT(WINAPI*)(void*,UINT,DWORD,HWND,DWORD,PP8*,void**))VT(d,15))(d,0,1,wnd,0x20,&pp,&dev);
 say("   CreateDevice(HAL, full screen 640x480 R5G6B5, D16) %08lX%s (%lu ms)\n",(DWORD)hr,hr==(HRESULT)0x88760827L?" D3DERR_DRIVERINTERNALERROR":"",GetTickCount()-t);
 if(SUCCEEDED(hr)&&dev){
  t=GetTickCount();hr=((HRESULT(WINAPI*)(void*,DWORD,const void*,DWORD,DWORD,float,DWORD))VT(dev,36))(dev,0,0,3,0xFF00FF00UL,1.0f,0);
  if(SUCCEEDED(hr))hr=((HRESULT(WINAPI*)(void*,const void*,const void*,HWND,const void*))VT(dev,15))(dev,0,0,0,0);
  say("   Clear and Present %08lX (%lu ms)\n",(DWORD)hr,GetTickCount()-t);
  ((ULONG(WINAPI*)(void*))VT(dev,2))(dev);}
 ((ULONG(WINAPI*)(void*))VT(d,2))(d);DestroyWindow(wnd);
 return SUCCEEDED(hr)?0:11;
}
/* EmulationOnly for run a-emu; VXCRASH_OLD keeps the value it had (0xFFFFFFFF: none) until it is put back */
static const char ddkey[]="Software\\Microsoft\\DirectDraw";
static void emu_restore(void) {
 HKEY k;DWORD old,n=4,ty;
 if(RegOpenKeyEx(HKEY_LOCAL_MACHINE,ddkey,0,KEY_ALL_ACCESS,&k)!=ERROR_SUCCESS)return;
 if(RegQueryValueEx(k,"VXCRASH_OLD",0,&ty,(BYTE*)&old,&n)==ERROR_SUCCESS){
  if(old==0xFFFFFFFFUL)RegDeleteValue(k,"EmulationOnly");else RegSetValueEx(k,"EmulationOnly",0,REG_DWORD,(BYTE*)&old,4);
  RegDeleteValue(k,"VXCRASH_OLD");say("EmulationOnly put back (%s)\n",old==0xFFFFFFFFUL?"none":old?"1":"0");}
 RegCloseKey(k);
}
static int emu_set(void) {
 HKEY k;DWORD old=0xFFFFFFFFUL,n=4,ty,one=1;
 if(RegCreateKeyEx(HKEY_LOCAL_MACHINE,ddkey,0,0,0,KEY_ALL_ACCESS,0,&k,0)!=ERROR_SUCCESS)return 0;
 if(RegQueryValueEx(k,"EmulationOnly",0,&ty,(BYTE*)&old,&n)!=ERROR_SUCCESS)old=0xFFFFFFFFUL;
 RegSetValueEx(k,"VXCRASH_OLD",0,REG_DWORD,(BYTE*)&old,4);RegSetValueEx(k,"EmulationOnly",0,REG_DWORD,(BYTE*)&one,4);RegCloseKey(k);return 1;
}
static DWORD spawn(const char *args,DWORD *pid,int dialog) {
 char cmd[MAX_PATH+64];STARTUPINFO si;PROCESS_INFORMATION pi;DWORD t0,code=0xFFFFFFFFUL,r,warned=0;
 sprintf(cmd,"\"%s\" %s%s",exepath,args,dialog?" dialog":"");memset(&si,0,sizeof(si));si.cb=sizeof(si);
 if(!CreateProcess(0,cmd,0,0,FALSE,0,0,0,&si,&pi)){say("CreateProcess(%s) failed: %lu\n",args,GetLastError());return 0xFFFFFFFEUL;}
 *pid=pi.dwProcessId;t0=GetTickCount();
 for(;;){r=WaitForSingleObject(pi.hProcess,500);if(r==WAIT_OBJECT_0)break;
  if(!warned&&GetTickCount()-t0>(dialog?30000UL:10000UL)){warned=1;say("   process %08lX (%s) has not ended after %lu s%s\n",pi.dwProcessId,args,(GetTickCount()-t0)/1000,dialog?" (close the fault dialog)":"");}
  if(GetTickCount()-t0>(dialog?300000UL:60000UL)){say("   process %08lX ended by VXCRASH after %lu s\n",pi.dwProcessId,(GetTickCount()-t0)/1000);TerminateProcess(pi.hProcess,99);WaitForSingleObject(pi.hProcess,5000);break;}}
 GetExitCodeProcess(pi.hProcess,&code);
 if(GetTickCount()-t0>2000&&args[0]!='/')say("   (it took %lu ms to end)\n",GetTickCount()-t0);
 CloseHandle(pi.hThread);CloseHandle(pi.hProcess);
 return code;
}
static int run(const char *cs,int dialog) {
 char a[64];DWORD pid,code,t0;int k,fails=0,rgb=strstr(cs,"sw")||strstr(cs,"emu");
 say("== run %s\n",cs);
 if(strstr(cs,"emu")){if(!emu_set()){say("   EmulationOnly could not be set: run skipped\n");return 1;}say("   EmulationOnly = 1 for this run (the second programs use the RGB device: no hardware device then)\n");}
 show_stats("before");
 sprintf(a,"/child %s",cs);t0=GetTickCount();code=spawn(a,&pid,dialog);
 say("the child %08lX ended: exit code %08lX%s after %lu ms\n",pid,code,code==0xC0000005UL?" (access violation, as intended)":"",GetTickCount()-t0);
 Sleep(500);show_stats("after the crash");
 for(k=1;k<=3;k++){sprintf(a,"/second %d%s",k,rgb?" sw":"");code=spawn(a,&pid,0);if(code){fails++;say("   second program %d (%08lX): exit code %lu\n",k,pid,code);}Sleep(300);}
 code=spawn("/d3d8",&pid,0);if(code&&!strstr(cs,"emu")){fails++;say("   the Direct3D 8 program failed\n");}
 {HANDLE h=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);DWORD in[2],o[8],got=0;in[0]=0xBC;in[1]=0x450;memset(o,0,sizeof(o)); /* v1.1: AD's counts */
  if(h!=INVALID_HANDLE_VALUE){DeviceIoControl(h,0x85500022UL,in,8,o,32,&got,0);CloseHandle(h);}
  say("   3D give-ups: owners gone %lu, given back %lu, resets that failed %lu, refused (limit) %lu; given up now %lu\n",o[2],o[3],o[4],o[5],o[6]);
  if(cs[0]=='e'&&(o[3]<1||o[6])){fails++;say("   run e: 3D was not given back after its owner died\n");}}
 show_stats("after the second programs");
 if(strstr(cs,"emu"))emu_restore();
 say("== run %s: %s\n\n",cs,fails?"FAILED":"passed");
 return fails;
}
int main(int argc,char **argv) {
 char *p;int k,dialog=0,fails=0,any=0;static const char *const all[]={"a","b","c","d","e","a-sw","a-emu"};
 GetModuleFileName(0,exepath,sizeof(exepath));strcpy(logpath,exepath);p=strrchr(logpath,'\\');if(p)strcpy(p+1,"VXCRASH.LOG");else strcpy(logpath,"VXCRASH.LOG");
 for(k=1;k<argc;k++)if(!stricmp(argv[k],"dialog"))dialog=1;
 if(argc>=3&&!strcmp(argv[1],"/child")){if(!dialog)SetErrorMode(SEM_NOGPFAULTERRORBOX|SEM_FAILCRITICALERRORS);return child(argv[2]);}
 if(argc>=2&&!strcmp(argv[1],"/second"))return second(argc>=3?atoi(argv[2]):0,argc>=4&&!strcmp(argv[3],"sw"));
 if(argc>=2&&!strcmp(argv[1],"/d3d8"))return d3d8();
 {FILE *f=fopen(logpath,"w");if(f)fclose(f);}
 say(VER": crashes a full-screen Direct3D 7 program and checks the next program can still go full screen\n");
 {HANDLE v=CreateFile("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);DWORD in=0x75,o[8],got=0;memset(o,0,sizeof(o));
  if(v!=INVALID_HANDLE_VALUE){if(DeviceIoControl(v,0x85500022UL,&in,4,o,32,&got,0))say("the VxD's build: .%lu\n",o[1]%1000UL);CloseHandle(v);}}
 dll();emu_restore();
 for(k=1;k<argc;k++){int j;for(j=0;j<6;j++)if(!stricmp(argv[k],all[j])){any=1;fails+=run(all[j],dialog)!=0;}}
 if(!any)for(k=0;k<6;k++)fails+=run(all[k],dialog)!=0;
 say("%s\n",fails?"VXCRASH: FAILED (see the runs above)":"VXCRASH: every run passed");
 return fails?2:0;
}
