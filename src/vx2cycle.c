/* VX2CYCLE v1.0: does the display fail after a number of mode changes? No input, no screenshots.
 * It cycles through six modes (640x480, 800x600, 1024x768 at 16 and 32 bpp) and logs "op N" before every mode change and "ok" after it.
 * Command line: a method plus optional settings.
 *   gdi      ChangeDisplaySettings only (no DirectDraw at all)
 *   dd       DirectDraw SetDisplayMode only (no surface)
 *   ddsurf   DirectDraw SetDisplayMode, then a primary surface filled with a colour and released
 *   ddchain  as ddsurf, plus a flip chain (primary with 2 back buffers, 6 flips, FlipToGDISurface) before the mode change
 *   n=40     number of mode changes (default 40)   hold=500  milliseconds to stay in each mode (default 500)   restore  restore the desktop mode after every change
 * Writes VX2CYCLE.TXT after every line (the last line shows where it stopped). */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <io.h>
static FILE *lf; static LPDIRECTDRAW dd;
static void logl(const char *fmt,...) {if(!lf) return; {va_list a; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a);} fflush(lf); _commit(_fileno(lf));}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {if(m==WM_ERASEBKGND) return 1; if(m==WM_SETCURSOR) {SetCursor(LoadCursor(NULL,IDC_ARROW)); return TRUE;} return DefWindowProc(w,m,a,b);}
static void wait_ms(DWORD ms) {DWORD e=GetTickCount()+ms; MSG m; while(GetTickCount()<e) {while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} Sleep(10);}}
static void memlog(int op) {MEMORYSTATUS m; m.dwLength=sizeof(m); GlobalMemoryStatus(&m); logl("  mem op %d: physical free %lu KB, page file free %lu KB\n",op,m.dwAvailPhys/1024,m.dwAvailPageFile/1024);}
static void flipcycle(void) {
 DDSURFACEDESC s; DDSCAPS caps; LPDIRECTDRAWSURFACE prim=NULL,back=NULL; int i;
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; s.dwBackBufferCount=2; s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
 if(FAILED(IDirectDraw_CreateSurface(dd,&s,&prim,NULL))) {logl("  chain: CreateSurface failed\n"); return;}
 memset(&caps,0,sizeof(caps)); caps.dwCaps=DDSCAPS_BACKBUFFER;
 if(FAILED(IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back))) {IDirectDrawSurface_Release(prim); return;}
 for(i=0;i<6;i++) {DDBLTFX f; memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=0x00DC2828UL>>(i&3); IDirectDrawSurface_Blt(back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f); if(FAILED(IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT))) break; IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back);}
 IDirectDraw_FlipToGDISurface(dd); IDirectDrawSurface_Release(prim);
}
static void surf(void) {
 DDSURFACEDESC d; LPDIRECTDRAWSURFACE ps=NULL; DDBLTFX f;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
 if(FAILED(IDirectDraw_CreateSurface(dd,&d,&ps,NULL))) {logl("  surface: CreateSurface failed\n"); return;}
 memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=0x00204080UL; IDirectDrawSurface_Blt(ps,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f); IDirectDrawSurface_Release(ps);
}
static LONG WINAPI crashfilter(EXCEPTION_POINTERS *e) {logl("\nCRASH: exception %08lX at %08lX\n",(DWORD)e->ExceptionRecord->ExceptionCode,(DWORD)e->ExceptionRecord->ExceptionAddress); if(lf) {fclose(lf); lf=NULL;} return EXCEPTION_EXECUTE_HANDLER;}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show_) {
 static const DWORD W[6]={640,800,1024,640,800,1024},H[6]={480,600,768,480,600,768},B[6]={16,16,16,32,32,32};
 int method=0,n=40,i,restore=0; DWORD hold=500; WNDCLASS wc; HWND win=NULL; HMODULE mod; typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*); CREATE create=NULL;
 (void)prev;(void)show_; if(!cmd) cmd="";
 if(strstr(cmd,"ddchain")) method=3; else if(strstr(cmd,"ddsurf")) method=2; else if(strstr(cmd,"gdi")) method=0; else if(strstr(cmd,"dd")) method=1; else {MessageBox(NULL,"Use: VX2CYCLE gdi | dd | ddsurf | ddchain  [n=40] [hold=500] [restore]","VX2CYCLE",MB_OK); return 1;}
 if(strstr(cmd,"n=")) n=atoi(strstr(cmd,"n=")+2); if(strstr(cmd,"hold=")) hold=(DWORD)atoi(strstr(cmd,"hold=")+5); if(strstr(cmd,"restore")) restore=1; if(n<1) n=1;
 SetUnhandledExceptionFilter(crashfilter); lf=fopen("VX2CYCLE.TXT","w");
 logl("VX2CYCLE v1.0 command line: %s\nmethod %s, %d mode changes, hold %lu ms, restore %d\n",cmd,method==0?"gdi":method==1?"dd":method==2?"ddsurf":"ddchain",n,hold,restore);
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.lpszClassName="VX2CYCLE"; RegisterClass(&wc);
 win=CreateWindowEx(WS_EX_TOPMOST,wc.lpszClassName,"VX2CYCLE",WS_POPUP|WS_VISIBLE,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,hi,NULL);
 if(method>=1) {
  mod=LoadLibraryA("DDRAW.DLL"); create=mod?(CREATE)GetProcAddress(mod,"DirectDrawCreate"):NULL;
  if(!create||FAILED(create(NULL,&dd,NULL))||FAILED(IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))) {logl("DirectDraw not available\n"); return 1;}
 }
 for(i=1;i<=n;i++) {
  int k=(i-1)%6; DWORD w=W[k],h=H[k],b=B[k];
  logl("op %d: %lux%lux%lu\n",i,w,h,b); memlog(i);
  if(method==3) flipcycle();
  if(method==0) {DEVMODE dm; LONG r; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL; r=ChangeDisplaySettings(&dm,CDS_FULLSCREEN); if(r!=DISP_CHANGE_SUCCESSFUL) logl("  ChangeDisplaySettings returned %ld\n",r);}
  else {HRESULT r=IDirectDraw_SetDisplayMode(dd,w,h,b); if(FAILED(r)) logl("  SetDisplayMode failed %08lX\n",(DWORD)r); else if(method>=2) surf();}
  logl("  ok\n"); wait_ms(hold);
  if(restore) {logl("  restore\n"); if(method==0) ChangeDisplaySettings(NULL,0); else IDirectDraw_RestoreDisplayMode(dd); logl("  restore ok\n"); wait_ms(hold);}
 }
 if(method>=1) {IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd);} else ChangeDisplaySettings(NULL,0);
 logl("\nFINISHED all %d mode changes\n",n); if(lf) fclose(lf);
 {char m[100]; sprintf(m,"Finished %d mode changes. Send VX2CYCLE.TXT.",n); MessageBox(NULL,m,"VX2CYCLE",MB_OK);} return 0;
}
