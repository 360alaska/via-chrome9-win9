/* VX2DKEY v1.4 (the HAL's record is off by default from v10.148: switched on through the VXDBGKEYON export for the run). v1.3 (v1.2: the runtime hands DDBLT_KEYDEST to the HAL as DDBLT_KEYDESTOVERRIDE with bltFX's destination key 0, while the surface LCL holds the key;
 * v1.3 adds an empty and a filled effects structure, a DESTOVERLAY key, the KEYSRC record, and the HAL's v10.147 record of bltFX.dwSize and the overlay keys). v1.2 (v1.1 looked the HAL up with GetModuleHandle only, which fails: LoadLibrary added). v1.1 (v1.0: the override key works on both paths, the surface key on neither, so the key value is wrong; v1.1 logs what the HAL was handed
 * (VXDDRAW.DLL v10.146 export VXDBGKEY) and GetColorKey). v1.0: DirectDraw destination colour key blits. VX2KEEP v1.0 found that a DDBLT_KEYDEST blit into the back buffer drew nothing at all (the key-coloured
 * stripes stayed, with ACCEL_KEEP 0 and 1, at 8, 16 and 32 bpp), while source-key, plain and fill blits were exact. This finds where it goes wrong.
 * Exclusive 640x480 at 8, 16 and 32 bpp. Destination: the back buffer of a flip chain, and a plain video-memory surface. Key: the surface's DDCKEY_DESTBLT
 * (DDBLT_KEYDEST) and the effects structure (DDBLT_KEYDESTOVERRIDE). Path: the engine (normal), and the HAL's CPU loop (engine switched off live).
 * Each case: the destination is filled with a background colour and key-coloured stripes, a 128x64 picture is blitted with the key, and every pixel is checked
 * (stripes must show the picture, the rest must keep the background). Logged: Blt's result, the driver counters (engine operations, declined, destination-key
 * commands, DirectDraw commands) and the pixel counts. VX2DKEY.TXT / .RES. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <io.h>
#include <ddraw.h>
#include "vx85598.h"
#include "vx855_2d.h"

static FILE *lf; static HANDLE dev=INVALID_HANDLE_VALUE; static HWND win;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2DKEY.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}}
static void wait_ms(int ms) {DWORD t=GetTickCount(); while((int)(GetTickCount()-t)<ms) {pump(); Sleep(5);}}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static int stats(vx2d_stats *s) {DWORD got=0; memset(s,0,sizeof(*s)); return DeviceIoControl(dev,VX_IOCTL_2D_STATS,NULL,0,s,sizeof(*s),&got,NULL)&&got==sizeof(*s);}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {if(m==WM_ERASEBKGND) return 1; return DefWindowProc(w,m,a,b);}
static void front(void) {HWND f=GetForegroundWindow(); DWORD ft=f?GetWindowThreadProcessId(f,NULL):0,me=GetCurrentThreadId();
 SetWindowPos(win,HWND_TOPMOST,0,0,640,480,SWP_SHOWWINDOW); if(ft&&ft!=me) AttachThreadInput(me,ft,TRUE); SetForegroundWindow(win); SetFocus(win); if(ft&&ft!=me) AttachThreadInput(me,ft,FALSE); wait_ms(300);}
static DWORD px(const DDSURFACEDESC *d,int by,int x,int y) {BYTE *r=(BYTE*)d->lpSurface+y*d->lPitch; return by==4?((DWORD*)r)[x]:by==2?(DWORD)((WORD*)r)[x]:(DWORD)r[x];}
static void put(const DDSURFACEDESC *d,int by,int x,int y,DWORD v) {BYTE *r=(BYTE*)d->lpSurface+y*d->lPitch; if(by==4) ((DWORD*)r)[x]=v; else if(by==2) ((WORD*)r)[x]=(WORD)v; else r[x]=(BYTE)v;}
static long bad_total;
static void dbg(LPDIRECTDRAWSURFACE dst) {
 static HMODULE m; DWORD *k; DDCOLORKEY ck; HRESULT hr;
 if(!m) m=GetModuleHandleA("VXDDRAW.DLL");
 if(!m) m=LoadLibraryA("VXDDRAW.DLL"); /* v1.2: DirectDraw maps the HAL in the shared arena without loading it into this process's module list */
 k=m?(DWORD*)GetProcAddress(m,"VXDBGKEY"):NULL;
 memset(&ck,0,sizeof(ck)); hr=IDirectDrawSurface_GetColorKey(dst,DDCKEY_DESTBLT,&ck);
 logline("      GetColorKey(DESTBLT) %08lX: %lX..%lX\n",(DWORD)hr,ck.dwColorSpaceLowValue,ck.dwColorSpaceHighValue);
 if(!k) {logline("      %s\n",m?"VXDDRAW.DLL has no VXDBGKEY (older than v10.146)":"VXDDRAW.DLL could not be loaded"); return;}
 logline("      the HAL saw (keyed blit %lu): flags %08lX; destination LCL %08lX flags %08lX, ddckCKDestBlt %lX..%lX, ddckCKSrcBlt %lX..%lX; bltFX dest key %lX..%lX, src key %lX..%lX; key used %lX; source LCL %08lX ddckCKSrcBlt %lX..%lX\n",
  k[0],k[1],k[12],k[2],k[3],k[4],k[5],k[6],k[7],k[8],k[9],k[10],k[11],k[13],k[14],k[15]);
 logline("      (v10.147 record) bltFX.dwSize %lu, dwDDFX %08lX; destination ddckCKDestOverlay %lX..%lX; source LCL flags %08lX, ddckCKSrcOverlay %lX\n",k[16],k[21],k[17],k[18],k[19],k[20]);
}
static void one_depth(int bpp) {
 LPDIRECTDRAW dd=NULL; LPDIRECTDRAWSURFACE pri=NULL,back=NULL,off=NULL,src=NULL; DDSURFACEDESC d; DDSCAPS caps; DDBLTFX fx; DDCOLORKEY ck; HRESULT hr; int by=bpp/8,x,y,dsel,kv,path;
 DWORD mask=bpp==32?0x00FFFFFFUL:bpp==16?0xFFFFUL:0xFFUL,K=0x2A,BG=bpp==8?0x37:bpp==16?0x3C6E:0x013C6E;
 logline("== 640x480x%d\n",bpp);
 if(FAILED(DirectDrawCreate(NULL,&dd,NULL))) {logline("   DirectDrawCreate failed\n"); bad_total++; return;}
 front();
 if(FAILED(hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))||FAILED(hr=IDirectDraw_SetDisplayMode(dd,640,480,bpp))) {logline("   exclusive mode %08lX\n",(DWORD)hr); bad_total++; goto out;}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; d.dwBackBufferCount=1; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
 if(FAILED(hr=IDirectDraw_CreateSurface(dd,&d,&pri,NULL))) {logline("   primary %08lX\n",(DWORD)hr); bad_total++; goto out;}
 caps.dwCaps=DDSCAPS_BACKBUFFER; IDirectDrawSurface_GetAttachedSurface(pri,&caps,&back);
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; d.dwWidth=320; d.dwHeight=200; d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 IDirectDraw_CreateSurface(dd,&d,&off,NULL);
 d.dwWidth=128; d.dwHeight=64; IDirectDraw_CreateSurface(dd,&d,&src,NULL);
 if(!back||!off||!src) {logline("   surfaces: back %p offscreen %p source %p\n",(void*)back,(void*)off,(void*)src); bad_total++; goto out;}
 {LPDIRECTDRAWSURFACE l[4];static const char *n[4]={"primary","back buffer","offscreen","source"};int i;l[0]=pri;l[1]=back;l[2]=off;l[3]=src; /* v1.5: where each surface lives */
  for(i=0;i<4;i++){DDSURFACEDESC q;memset(&q,0,sizeof(q));q.dwSize=sizeof(q);if(SUCCEEDED(IDirectDrawSurface_GetSurfaceDesc(l[i],&q)))logline("   %s: caps %08lX (%s) pitch %ld\n",n[i],q.ddsCaps.dwCaps,(q.ddsCaps.dwCaps&DDSCAPS_SYSTEMMEMORY)?"SYSTEM memory":"video memory",(long)q.lPitch);}}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 if(SUCCEEDED(IDirectDrawSurface_Lock(src,NULL,&d,DDLOCK_WAIT,NULL))) {for(y=0;y<64;y++) for(x=0;x<128;x++) {DWORD v=((DWORD)(x*2654435761UL+y*40503UL))&mask; if(v==K||v==BG) v^=8; put(&d,by,x,y,v);} IDirectDrawSurface_Unlock(src,d.lpSurface);}
 for(dsel=0;dsel<2;dsel++) for(kv=0;kv<5;kv++) for(path=0;path<(kv<2?2:1);path++) {
  static const char *kn[5]={"KEYDEST (surface key, no effects structure)","KEYDESTOVERRIDE","KEYDEST with an empty effects structure","KEYDEST with an effects structure holding key 55","KEYDEST with DESTBLT and DESTOVERLAY keys set"};
  LPDIRECTDRAWSURFACE dst=dsel?off:back; RECT dr,s; vx2d_stats s0,s1; DWORD o0[4],o1[4]; long stripe_ok=0,stripe_bad=0,rest_ok=0,rest_bad=0; DDSURFACEDESC sd;
  dr.left=40; dr.top=30; dr.right=168; dr.bottom=94;
  memset(&fx,0,sizeof(fx)); fx.dwSize=sizeof(fx); fx.dwFillColor=BG; IDirectDrawSurface_Blt(dst,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx);
  for(y=0;y<200;y+=8) {s.left=0; s.right=320; s.top=y; s.bottom=y+3; fx.dwFillColor=K; IDirectDrawSurface_Blt(dst,&s,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx);}
  ck.dwColorSpaceLowValue=ck.dwColorSpaceHighValue=K;
  if(kv!=1) {hr=IDirectDrawSurface_SetColorKey(dst,DDCKEY_DESTBLT,&ck); logline("   SetColorKey(DESTBLT) on the %s: %08lX\n",dsel?"offscreen surface":"back buffer",(DWORD)hr);}
  if(kv==4) {hr=IDirectDrawSurface_SetColorKey(dst,DDCKEY_DESTOVERLAY,&ck); logline("   SetColorKey(DESTOVERLAY): %08lX\n",(DWORD)hr);}
  memset(&fx,0,sizeof(fx)); fx.dwSize=sizeof(fx); if(kv==1) fx.ddckDestColorkey=ck; if(kv==3) fx.ddckDestColorkey.dwColorSpaceLowValue=fx.ddckDestColorkey.dwColorSpaceHighValue=0x55;
  if(path) ctl(3,NULL); /* engine off: the HAL's CPU loop */
  stats(&s0); ctl(0xFFFFFFFEUL,o0);
  hr=IDirectDrawSurface_Blt(dst,&dr,src,NULL,DDBLT_WAIT|(kv==1?DDBLT_KEYDESTOVERRIDE:DDBLT_KEYDEST),(kv==1||kv==2||kv==3)?&fx:NULL);
  while(IDirectDrawSurface_GetBltStatus(dst,DDGBS_ISBLTDONE)==DDERR_WASSTILLDRAWING) ;
  stats(&s1); ctl(0xFFFFFFFEUL,o1); ctl(1,NULL);
  memset(&sd,0,sizeof(sd)); sd.dwSize=sizeof(sd); memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
  if(SUCCEEDED(IDirectDrawSurface_Lock(dst,NULL,&sd,DDLOCK_WAIT,NULL))) {
   IDirectDrawSurface_Lock(src,NULL,&d,DDLOCK_WAIT,NULL);
   for(y=dr.top;y<dr.bottom;y++) for(x=dr.left;x<dr.right;x++) {DWORD v=px(&sd,by,x,y)&mask,sv=px(&d,by,x-dr.left,y-dr.top)&mask; if((y%8)<3) {if(v==sv) stripe_ok++; else stripe_bad++;} else {if(v==BG) rest_ok++; else rest_bad++;}}
   IDirectDrawSurface_Unlock(src,d.lpSurface); IDirectDrawSurface_Unlock(dst,sd.lpSurface);
  }
  logline("   %s, %s, %s: Blt %08lX; key pixels now the picture %ld / still key-coloured or other %ld; background kept %ld / changed %ld; engine ops +%lu, declined +%lu, destination-key commands +%lu, DirectDraw commands +%lu\n",
   dsel?"offscreen surface":"back buffer",kn[kv],path?"CPU loop":"engine",(DWORD)hr,stripe_ok,stripe_bad,rest_ok,rest_bad,o1[0]-o0[0],o1[1]-o0[1],s1.dst_keys-s0.dst_keys,s1.ddraw-s0.ddraw);
  dbg(dst);
  if(FAILED(hr)||stripe_bad||rest_bad) bad_total++;
  if(kv!=1) IDirectDrawSurface_SetColorKey(dst,DDCKEY_DESTBLT,NULL);
  if(kv==4) IDirectDrawSurface_SetColorKey(dst,DDCKEY_DESTOVERLAY,NULL);
 }
{ /* the source key, for comparison: what the HAL is handed for DDBLT_KEYSRC without an effects structure */
  RECT dr; dr.left=40; dr.top=30; dr.right=168; dr.bottom=94; ck.dwColorSpaceLowValue=ck.dwColorSpaceHighValue=K;
  hr=IDirectDrawSurface_SetColorKey(src,DDCKEY_SRCBLT,&ck); logline("   SetColorKey(SRCBLT) on the picture: %08lX\n",(DWORD)hr);
  hr=IDirectDrawSurface_Blt(off,&dr,src,NULL,DDBLT_WAIT|DDBLT_KEYSRC,NULL); logline("   KEYSRC blit (no effects structure): %08lX\n",(DWORD)hr); dbg(off);
  IDirectDrawSurface_SetColorKey(src,DDCKEY_SRCBLT,NULL);
 }
out:
 if(src) IDirectDrawSurface_Release(src); if(off) IDirectDrawSurface_Release(off); if(back) IDirectDrawSurface_Release(back); if(pri) IDirectDrawSurface_Release(pri);
 if(dd) {IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd);}
 wait_ms(1000);
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int sh) {
 WNDCLASSA wc; char res[160]; (void)hp; (void)cmd; (void)sh;
 lf=fopen("VX2DKEY.TXT","w"); logline("VX2DKEY v1.5\n"); finish("RUNNING: the run did not reach its end");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL); if(dev==INVALID_HANDLE_VALUE) {finish("FAIL: driver not found"); return 1;}
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2DKEY"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClassA(&wc);
 win=CreateWindowExA(WS_EX_TOPMOST,"VX2DKEY","VX2DKEY",WS_POPUP|WS_VISIBLE,0,0,640,480,NULL,NULL,hi,NULL);
 {char path[MAX_PATH]; WIN32_FIND_DATAA fd; HANDLE h; GetSystemDirectoryA(path,MAX_PATH); strcat(path,"\\VXDDRAW.DLL"); h=FindFirstFileA(path,&fd); if(h!=INVALID_HANDLE_VALUE) {logline("%s: %lu bytes\n",path,fd.nFileSizeLow); FindClose(h);}}
 {HMODULE m=GetModuleHandleA("VXDDRAW.DLL"); DWORD *on; if(!m) m=LoadLibraryA("VXDDRAW.DLL"); on=m?(DWORD*)GetProcAddress(m,"VXDBGKEYON"):NULL; if(on) *on=1; logline("HAL key record: %s\n",on?"switched on for this run":"no switch (VXDDRAW.DLL before v10.148: always on)");}
 one_depth(8); one_depth(16); one_depth(32);
 {HMODULE m=GetModuleHandleA("VXDDRAW.DLL"); DWORD *on=m?(DWORD*)GetProcAddress(m,"VXDBGKEYON"):NULL; if(on) *on=0;}
 ctl(0xFFFFFFFFUL,NULL);
 sprintf(res,"%s: %ld of 42 destination-key cases wrong (details in VX2DKEY.TXT)",bad_total?"FAIL":"PASS",bad_total);
 finish(res); logline("%s\n",res); DestroyWindow(win); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
