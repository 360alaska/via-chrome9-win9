/* VX2BENCH: independent GDI/DirectDraw timing and DirectDraw pixel checks.
 * Uses live equivalents of ACCEL_2D/ACCEL_COPY_ALL; restores exact prior flags.
 * No display mode change, primary flipping, or registry writes. */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <io.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"
#include "vx855_2d.h"
static HANDLE dev=INVALID_HANDLE_VALUE;
static FILE *logfile;
static HWND arena,drag;
static HDC dc;
static HBRUSH brush;
static HGDIOBJ oldbrush;
static DWORD saved_flags; static int saved,aborted;
static LARGE_INTEGER freq;
static LPDIRECTDRAW dd;
static LPDIRECTDRAWSURFACE srcsurf,dstsurf;
static DWORD depth;
static LPDIRECTDRAWCLIPPER clipper;
/* v1.7: cycle counter for the driver cost profile */
unsigned long tsc(void);
#pragma aux tsc = 0x0F 0x31 value [eax] modify exact [eax edx];
static RECT small_rect={0,0,8,8};
static void logline(const char *fmt,...) {
 va_list ap; va_start(ap,fmt); vfprintf(logfile,fmt,ap); va_end(ap); fflush(logfile); _commit(_fileno(logfile));
}
static int control(DWORD f,DWORD *o) {
 DWORD got=0; return DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&f,4,o,16,&got,NULL)&&got==16;
}
static int stats(vx2d_stats *s) {
 DWORD got=0; return DeviceIoControl(dev,VX_IOCTL_2D_STATS,NULL,0,s,sizeof(*s),&got,NULL)&&got==sizeof(*s)&&s->abi==VX2D_ABI;
}
static void pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {
  if(m.message==WM_QUIT) {aborted=1;continue;}
  TranslateMessage(&m); DispatchMessage(&m);
 }
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
}
static double now(void) {
 LARGE_INTEGER t;
 if(freq.QuadPart&&QueryPerformanceCounter(&t)) return 1000.0*(double)t.QuadPart/(double)freq.QuadPart;
 return (double)GetTickCount();
}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_CLOSE) {aborted=1;return 0;}
 if(m==WM_PAINT) {
  PAINTSTRUCT ps;HDC d=BeginPaint(w,&ps);FillRect(d,&ps.rcPaint,(HBRUSH)GetStockObject(w==drag?LTGRAY_BRUSH:BLACK_BRUSH));EndPaint(w,&ps);return 0;
 }
 if(m==WM_ERASEBKGND) return 1;
 return DefWindowProc(w,m,a,b);
}
static int workload(int kind,DWORD n) {
 DWORD i; DDBLTFX fx; HRESULT hr=DD_OK;
 memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwFillColor=depth==16?0x5B6D:0x5876B2;
 for(i=0;i<n&&!aborted;i++) {
  switch(kind) {
   case 0: if(!PatBlt(dc,8,8,288,192,PATCOPY)) return 0;break;
   case 1: if(!BitBlt(dc,168,8,128,192,dc,8,8,SRCCOPY)) return 0;break;
   case 2: if(!BitBlt(dc,9+(i&1),9,270,180,dc,8+(i&1),8,SRCCOPY)) return 0;break;
   case 3: if(!SetWindowPos(drag,NULL,20+(i&63),26+(i&31),112,72,SWP_NOZORDER|SWP_NOACTIVATE)) return 0; UpdateWindow(drag);break;
   case 4: hr=IDirectDrawSurface_Blt(dstsurf,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx);break;
   case 5: hr=IDirectDrawSurface_Blt(dstsurf,NULL,srcsurf,NULL,DDBLT_WAIT,NULL);break;
   case 6: hr=IDirectDrawSurface_Blt(dstsurf,NULL,srcsurf,NULL,DDBLT_WAIT|DDBLT_KEYSRC,NULL);break;
   case 7: hr=IDirectDrawSurface_Blt(dstsurf,NULL,srcsurf,NULL,DDBLT_WAIT|DDBLT_KEYDEST,NULL);break;
   case 8: hr=IDirectDrawSurface_Blt(dstsurf,&small_rect,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx);break;
   case 9: hr=IDirectDrawSurface_Blt(dstsurf,&small_rect,srcsurf,&small_rect,DDBLT_WAIT,NULL);break;
  }
  if(FAILED(hr)) {logline("  DirectDraw call failed: %08lX\n",(DWORD)hr);return 0;}
  if((i&31)==31) pump();
 }
 GdiFlush();return !aborted;
}
static double median(double *v) {
 double t;if(v[0]>v[1]) {t=v[0];v[0]=v[1];v[1]=t;}
 if(v[1]>v[2]) {t=v[1];v[1]=v[2];v[2]=t;}
 if(v[0]>v[1]) {t=v[0];v[0]=v[1];v[1]=t;}return v[1];
}
static void measure(int kind,const char *name) {
 static const DWORD settings[4]={2,0,1,5};/* defaults, size thresholds active */
 static const char *labels[4]={"OFF","CONSERVATIVE (default size thresholds)","ALL COPY DIRECTIONS (default size thresholds)","FORCED NATIVE (no thresholds, raw engine cost)"};
 double times[4][3],ms[4],start;DWORD o[4],n=32,ops[4]={0,0,0,0},fb[4]={0,0,0,0};int trial,j,index,valid[4]={1,1,1,1};
 vx2d_stats a,b;
 if(!control(2,o)) {aborted=1;return;}
 /* Determine one common iteration count from the software configuration. */
 while(!aborted) {
  start=now();if(!workload(kind,n)) return;
  if(now()-start>=150.0||n>=8192) break;n*=2;
 }
 logline("\n%s: %lu operations/trial, median of 3; milliseconds\n",name,n);
 for(trial=0;trial<3&&!aborted;trial++) for(j=0;j<4&&!aborted;j++) {
  index=(j+trial)%4; /* rotate order to reduce cache/order bias */
  if(!control(settings[index],o)||!workload(kind,4)||!stats(&a)) {valid[index]=0;continue;}
  start=now();if(!workload(kind,n)) {valid[index]=0;continue;} times[index][trial]=now()-start;
  if(!stats(&b)) {valid[index]=0;continue;}ops[index]+=b.ops-a.ops;fb[index]+=b.fallbacks-a.fallbacks;
 }
 for(index=0;index<4;index++) {
  if(!valid[index]||aborted) {logline("  %s: not measured\n",labels[index]);continue;}
  ms[index]=median(times[index]);logline("  %s: %.3f ms; native completed ops across 3 trials %lu; CPU fallbacks %lu",labels[index],ms[index],ops[index],fb[index]);
  if(index&&valid[0]&&ms[index]>0) logline("; %.2fx versus OFF",ms[0]/ms[index]);
  if(index&&!ops[index]) logline("; no native ops: size thresholds kept this on the CPU");
  logline("\n");
 }
}
/* v1.7: where does the ~0.4 ms per native operation go? The driver timestamps each phase of a completed native
 * operation with RDTSC; this reads the sums before and after a forced-native loop and prints microseconds per phase. */
static int phases(DWORD *n,DWORD *sum) {
 DWORD k,o[4];
 for(k=0;k<8;k++) {if(!control(0xFFFFFE00UL|k,o)) return 0; sum[k]=o[1]; if(!k) *n=o[0];}
 return 1;
}
static double calibrate(void) {
 double t0=now(),t1; unsigned long c0,c1;
 c0=tsc(); do t1=now(); while(t1-t0<150.0); c1=tsc();
 return (double)(c1-c0)/((t1-t0)*1000.0); /* cycles per microsecond */
}
static void profile_case(int kind,const char *name,DWORD n,double cpu) {
 static const char *pn[8]={"validate/policy","unlock (e_begin)","pre-idle","save 19 regs","program regs","command + idle","restore 19 regs","relock/exit"};
 DWORD n0=0,n1=0,s0[8],s1[8],o[4],k; double start,wall,tot=0,us;
 if(aborted||!control(5,o)||!workload(kind,8)||!phases(&n0,s0)) return;
 start=now(); if(!workload(kind,n)) return; wall=now()-start;
 if(!phases(&n1,s1)||n1==n0) {logline("%s: no native operations were timed\n",name); return;}
 logline("%s: %lu ops, %lu timed native; %.1f us/op wall clock\n",name,n,n1-n0,wall*1000.0/n);
 for(k=0;k<8;k++) {us=(double)(s1[k]-s0[k])*16.0/cpu/(double)(n1-n0); tot+=us; logline("    %-18s %7.1f us\n",pn[k],us);}
 logline("    %-18s %7.1f us inside the driver; %.1f us outside it (ioctl, DirectDraw, this program)\n","total",tot,wall*1000.0/n-tot);
}
static void profile(void) {
 double cpu=calibrate();DWORD o[4];
 logline("\nNative operation cost profile (forced native, microseconds per operation; counter %.0f MHz)\n",cpu);
 /* v1.9 (v10.319): the VxD times phases only while its profile switch is on (0xF4000001; off by default from v10.132: every case read
  * "no native operations were timed"); off again at the end */
 if(!control(0xF4000001UL,o)||o[0]!=1) {logline("the VxD's phase profile could not be switched on: no profile\n");return;}
 profile_case(8,"DirectDraw video fill 8x8",1000,cpu);
 profile_case(4,"DirectDraw video fill 128x128",1000,cpu);
 profile_case(9,"DirectDraw video copy 8x8",1000,cpu);
 profile_case(5,"DirectDraw video copy 128x128",1000,cpu);
 control(0xF4000000UL,o);
}
static DWORD getpixelraw(const DDSURFACEDESC *s,int x,int y) {
 BYTE *p=(BYTE*)s->lpSurface+y*s->lPitch+x*(depth/8);
 return depth==16?*(WORD*)p:(*(DWORD*)p&0xFFFFFFUL);
}
static void putpixelraw(DDSURFACEDESC *s,int x,int y,DWORD v) {
 BYTE *p=(BYTE*)s->lpSurface+y*s->lPitch+x*(depth/8);
 if(depth==16) *(WORD*)p=(WORD)v;else *(DWORD*)p=v;
}
static int locksurface(LPDIRECTDRAWSURFACE surf,DDSURFACEDESC *s) {
 memset(s,0,sizeof(*s));s->dwSize=sizeof(*s);return SUCCEEDED(IDirectDrawSurface_Lock(surf,NULL,s,DDLOCK_WAIT,NULL));
}
static DWORD sourcepixel(int x,int y) {return ((x+y)&3)?(depth==16?0xB32D:0xB45A37):0;}
static int ddcheck(void) {
 DDSURFACEDESC s;DDCOLORKEY key;DWORD o[4],bad,total=0;int x,y,test;HRESULT hr;DDBLTFX fx;vx2d_stats a,b;
 key.dwColorSpaceLowValue=key.dwColorSpaceHighValue=0;
 logline("step: ddcheck start\n");IDirectDrawSurface_SetColorKey(srcsurf,DDCKEY_SRCBLT,&key);logline("step: src colorkey set\n");
 IDirectDrawSurface_SetColorKey(dstsurf,DDCKEY_DESTBLT,&key);
 memset(&a,0,sizeof(a));memset(&b,0,sizeof(b));
 /* v10.319 (audit T1): without the force switch or the counters the check proves nothing native: it says so and fails */
 if(!control(5,o)||!stats(&a)) {logline("DD native verification unavailable: the VxD's control or statistics call failed\n");return 0;} /* force native: the pixel checks must exercise the engine whatever the size thresholds say */
 for(test=0;test<5&&!aborted;test++) {
  logline("step: case %d lock source\n",test);if(!locksurface(srcsurf,&s)) {logline("lock source failed\n");return 0;}logline("step: source locked, ptr %08lX pitch %ld\n",(DWORD)s.lpSurface,(long)s.lPitch);
  for(y=0;y<128;y++) for(x=0;x<128;x++) putpixelraw(&s,x,y,sourcepixel(x,y)|(depth==32?(x&1?0xFF000000UL:0xA5000000UL):0));
  logline("step: source written\n");IDirectDrawSurface_Unlock(srcsurf,s.lpSurface);logline("step: lock destination\n");
  if(!locksurface(dstsurf,&s)) {logline("lock destination failed\n");return 0;}
  for(y=0;y<128;y++) for(x=0;x<128;x++) putpixelraw(&s,x,y,(((x^y)&1)?0:0x4729)|(depth==32?0x3C000000UL:0));
  IDirectDrawSurface_Unlock(dstsurf,s.lpSurface);
  memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwFillColor=0x37AD;
  if(test==4) {
   struct {RGNDATAHEADER h;RECT r[2];} region;
   memset(&region,0,sizeof(region));region.h.dwSize=sizeof(RGNDATAHEADER);region.h.iType=RDH_RECTANGLES;region.h.nCount=2;region.h.nRgnSize=sizeof(region.r);
   SetRect(&region.r[0],7,11,63,39);SetRect(&region.r[1],29,57,113,101);SetRect(&region.h.rcBound,7,11,113,101);
   if(FAILED(IDirectDraw_CreateClipper(dd,0,&clipper,NULL))||FAILED(IDirectDrawClipper_SetClipList(clipper,(RGNDATA*)&region,0))||FAILED(IDirectDrawSurface_SetClipper(dstsurf,clipper))) return 0;
  }
  logline("step: case %d blt\n",test);hr=IDirectDrawSurface_Blt(dstsurf,NULL,test?srcsurf:NULL,NULL,DDBLT_WAIT|(test==0?DDBLT_COLORFILL:(test==2?DDBLT_KEYSRC:(test==3?DDBLT_KEYDEST:0))),test?NULL:&fx);
  if(FAILED(hr)) {logline("DD pixel case %d failed HRESULT %08lX\n",test,(DWORD)hr);return 0;}
  if(!locksurface(dstsurf,&s)) return 0;
  bad=0;
  for(y=0;y<128;y++) for(x=0;x<128;x++) {
   DWORD old=((x^y)&1)?0:0x4729,source=sourcepixel(x,y),expected;
   expected=test==0?0x37AD:(test==1?source:(test==2?(source?source:old):(old==0?source:old)));
   if(test==4) expected=((x>=7&&x<63&&y>=11&&y<39)||(x>=29&&x<113&&y>=57&&y<101))?source:old;
   if(getpixelraw(&s,x,y)!=expected) bad++;
  }
  IDirectDrawSurface_Unlock(dstsurf,s.lpSurface);
  if(test==4) {IDirectDrawSurface_SetClipper(dstsurf,NULL);IDirectDrawClipper_Release(clipper);clipper=NULL;}
  logline("DD pixel case %d: %lu mismatches\n",test,bad);total+=bad;
 }
 if(!stats(&b)) {logline("DD native verification unavailable: the statistics call failed\n");return 0;}
 logline("DD correctness native ops: %lu; source-key %lu; destination-key %lu\n",b.ddraw-a.ddraw,b.src_keys-a.src_keys,b.dst_keys-a.dst_keys);
 if(aborted) return 0;
 if(b.ddraw==a.ddraw) {logline("DD verification: the engine did none of the operations (the pixels were not the engine's)\n");return 0;}
 if(b.src_keys==a.src_keys||b.dst_keys==a.dst_keys) {logline("DD verification: native coverage incomplete (a keyed case did not reach the engine)\n");return 0;}
 return !total;
}
static int initdd(HWND w) {
 HMODULE mod=LoadLibraryA("DDRAW.DLL");HRESULT hr;DDSURFACEDESC s;DDCAPS caps;
 typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*);CREATE create;
 if(!mod) return 0;
 create=(CREATE)GetProcAddress(mod,"DirectDrawCreate");if(!create) return 0;
 hr=create(NULL,&dd,NULL);logline("DirectDrawCreate hr=%08lX; bench pid %08lX\n",(DWORD)hr,GetCurrentProcessId());if(FAILED(hr)) return 0;
 if(FAILED(IDirectDraw_SetCooperativeLevel(dd,w,DDSCL_NORMAL))) return 0;
 memset(&caps,0,sizeof(caps));caps.dwSize=sizeof(caps);IDirectDraw_GetCaps(dd,&caps,NULL);
 {char sd[260],fp[300];FILE *t;
  GetSystemDirectoryA(sd,sizeof sd);sprintf(fp,"%s\\VXDDRAW.DLL",sd);t=fopen(fp,"rb");
  /* The DLL lives in the shared arena, so it never appears in this program's own module list: judge the HAL by its caps. */
  logline("\nVXDDRAW.DLL in system folder: %s; DirectDraw hardware HAL active: %s\n",t?"yes":"NO - copy it to the Windows System folder",(caps.dwCaps&0x02000000UL)?"NO (caps report no hardware)":"yes");if(t)fclose(t);}
 logline("\nDirectDraw hardware caps: %08lX; key caps %08lX; surface caps %08lX\n",caps.dwCaps,caps.dwCKeyCaps,caps.ddsCaps.dwCaps);
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;s.dwWidth=s.dwHeight=128;
 s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 logline("step: creating video source surface\n");hr=IDirectDraw_CreateSurface(dd,&s,&srcsurf,NULL);if(FAILED(hr)) {logline("Create video source: %08lX\n",(DWORD)hr);return 0;}logline("step: video source created\n");
 hr=IDirectDraw_CreateSurface(dd,&s,&dstsurf,NULL);if(FAILED(hr)) {logline("Create video destination: %08lX\n",(DWORD)hr);return 0;}logline("step: video destination created\n");
 return 1;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show) {
 WNDCLASS wc;vx_status_t s;vx2d_stats initial;DWORD got,o[4];int good=0;RECT r;char text[220];
 (void)prev;(void)cmd;(void)show;
 logfile=fopen("VX2BENCH.TXT","w");if(!logfile) return 1;
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||
  (s.bpp!=16&&s.bpp!=32)||!stats(&initial)||!control(VX2D_QUERY,o)) goto cleanup;
 saved_flags=o[3];saved=1;depth=s.bpp;
 logline("VX2BENCH v1.9 (built with driver v10.%d)\nDesktop %lux%lux%lu pitch %lu; initial live flags %lu\n",(int)DRV_VER_BUILD,s.width,s.height,s.bpp,s.video_pitch,saved_flags);
 logline("Driver state: stage %lu failure %lu vga_mode %lu | 2D state %lu (0 unused,1 ready,2 disabled,3 lost) flags %lu ops %lu fallbacks %lu rejected %lu timeouts %lu resets %lu last cmd %08lX status %08lX result %lu\n",
  s.stage,s.failure,s.vga_mode,initial.state,initial.flags,initial.ops,initial.fallbacks,initial.rejected,initial.timeouts,initial.resets,initial.last_command,initial.last_status,initial.last_result);
 logline("Flags OFF=ACCEL_2D 0; CONSERVATIVE=ACCEL_2D 1 ACCEL_COPY_ALL 0; ALL=both 1. Registry is not changed.\n");
 QueryPerformanceFrequency(&freq);
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wnd;wc.hInstance=hi;wc.lpszClassName="VX2BENCH";wc.hCursor=LoadCursor(NULL,IDC_ARROW);RegisterClass(&wc);
 arena=CreateWindowEx(WS_EX_TOPMOST,wc.lpszClassName,"VX2BENCH - Esc aborts",WS_POPUP|WS_VISIBLE,8,8,320,240,NULL,NULL,hi,NULL);
 if(!arena) goto cleanup;
 ShowWindow(arena,SW_SHOW);UpdateWindow(arena);dc=GetDC(arena);
 drag=CreateWindow(wc.lpszClassName,"drag",WS_CHILD|WS_VISIBLE,20,26,112,72,arena,NULL,hi,NULL);
 brush=CreateSolidBrush(RGB(38,173,91));oldbrush=SelectObject(dc,brush);
 GetClientRect(arena,&r);FillRect(dc,&r,brush);ShowWindow(drag,SW_HIDE);
 measure(0,"GDI solid fill 288x192");measure(1,"GDI disjoint copy 128x192");measure(2,"GDI overlapping copy 270x180");
 ShowWindow(drag,SW_SHOW);measure(3,"Actual child-window moves 112x72 (USER/GDI repaint overhead included)");ShowWindow(drag,SW_HIDE);
 if(!aborted&&initdd(arena)) {
  good=ddcheck();logline("DirectDraw pixel verification: %s\n",good?"PASS":"FAIL");
  if(good) {measure(4,"DirectDraw video fill 128x128");measure(5,"DirectDraw video copy 128x128");measure(6,"DirectDraw source-key sprite 128x128");measure(7,"DirectDraw destination-key copy 128x128");profile();}
 } else logline("DirectDraw video surfaces unavailable or run aborted.\n");
 good=good&&!aborted;
cleanup:
 if(clipper) {IDirectDrawSurface_SetClipper(dstsurf,NULL);IDirectDrawClipper_Release(clipper);}
 if(dstsurf) IDirectDrawSurface_Release(dstsurf);if(srcsurf) IDirectDrawSurface_Release(srcsurf);if(dd) IDirectDraw_Release(dd);
 if(dc&&oldbrush) SelectObject(dc,oldbrush);if(brush) DeleteObject(brush);if(dc) ReleaseDC(arena,dc);if(arena) DestroyWindow(arena);
 if(saved) {if(control(saved_flags,o)&&o[3]==saved_flags) logline("Exact prior live engine flags restored: %lu\n",saved_flags);else logline("ERROR: could not restore live engine flags\n");}
 if(dev!=INVALID_HANDLE_VALUE) CloseHandle(dev);
 logline("Finished%s. Native counts are global and include other desktop activity; timings are not GPU-only.\n",aborted?" (aborted)":"");fclose(logfile);
 sprintf(text,"%s Read VX2BENCH.TXT for timings, native counts, and DirectDraw pixel checks.",good?"Benchmark finished.":"2D tests finished. DirectDraw part unavailable.");MessageBox(NULL,text,"VX2BENCH",MB_OK);
 return good?0:1;
}
