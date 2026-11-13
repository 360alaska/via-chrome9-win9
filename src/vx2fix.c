/* VX2FIX v1.0: driver v10.139 (fixes from the review of v10.103). 1. Colour cursors at 640x480x16 and x32 (the 16-bit cursor converter read every other pixel and packed
 * the channels wrong): a 32x32 cursor with red, green, blue, white and black stripes is shown; you judge it (Yes/No). 2. A fully transparent cursor (cursor_is_empty compared the
 * expanded mask with 0xFF): judged by eye. 3. DirectDraw CPU colour key at 32bpp (the CPU path compared the unused top byte): a system-memory source whose key-coloured pixels carry
 * FF in the top byte, blitted with a source key into video memory; the key pixels must be skipped. 4. The CR3 reload on every DRV call (live 0xEE000000|v): 3000 PatBlt PATINVERT
 * 32x32 (engine operations) timed with it on and off. Nothing is written to the registry. VX2FIX.TXT / .RES. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <io.h>
#include <ddraw.h>
#include "vx85598.h"

static FILE *lf; static HANDLE dev=INVALID_HANDLE_VALUE; static HWND win; static HCURSOR cur;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2FIX.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}}
static void wait_ms(int ms) {DWORD t=GetTickCount(); while((int)(GetTickCount()-t)<ms) {pump(); Sleep(5);}}
static double now(void) {LARGE_INTEGER c,f; if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;} return (double)GetTickCount();}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); wait_ms(1000); return rc==DISP_CHANGE_SUCCESSFUL;
}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_PAINT) {PAINTSTRUCT ps; HDC dc=BeginPaint(w,&ps); RECT r; GetClientRect(w,&r); r.right/=2; FillRect(dc,&r,(HBRUSH)GetStockObject(GRAY_BRUSH)); r.left=r.right; r.right*=2; FillRect(dc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH)); EndPaint(w,&ps); return 0;}
 if(m==WM_SETCURSOR&&cur) {SetCursor(cur); return TRUE;}
 if(m==WM_ERASEBKGND) return 1;
 return DefWindowProc(w,m,a,b);
}
static int ask(const char *q) {int r; logline("   question: %s\n",q); r=MessageBoxA(NULL,q,"VX2FIX",MB_YESNO|MB_TOPMOST|MB_SETFOREGROUND); logline("   answer: %s\n",r==IDYES?"yes":"no"); return r==IDYES;}
static HCURSOR make_colour_cursor(HDC dc,int empty) {
 ICONINFO ii; HBITMAP mask,col; HDC m=CreateCompatibleDC(dc); HBITMAP o; int x,y; static const COLORREF s[5]={RGB(255,0,0),RGB(0,255,0),RGB(0,0,255),RGB(255,255,255),RGB(0,0,0)}; BYTE mb[32*4*2]; HCURSOR c;
 memset(mb,empty?0xFF:0x00,sizeof(mb)); /* AND mask: 0 = opaque everywhere (or 1 = transparent everywhere); upper half AND, lower half XOR for a mono cursor is not used here */
 mask=CreateBitmap(32,32,1,1,mb); col=CreateCompatibleBitmap(dc,32,32); o=(HBITMAP)SelectObject(m,col);
 for(y=0;y<32;y++) for(x=0;x<32;x++) SetPixel(m,x,y,empty?RGB(0,0,0):s[(x/6)%5]);
 SelectObject(m,o); DeleteDC(m);
 memset(&ii,0,sizeof(ii)); ii.fIcon=FALSE; ii.xHotspot=0; ii.yHotspot=0; ii.hbmMask=mask; ii.hbmColor=col; c=(HCURSOR)CreateIconIndirect(&ii);
 DeleteObject(mask); DeleteObject(col); return c;
}
static void show_cursor(HCURSOR c) {int x; cur=c; SetClassLong(win,GCL_HCURSOR,(LONG)c); SetForegroundWindow(win); SetCursor(c); for(x=60;x<580;x+=40) {SetCursorPos(x,240); wait_ms(40);} SetCursorPos(300,240); wait_ms(300);}
static int ddkey(void) { /* 3: CPU source key at 32bpp with FF in the top byte */
 LPDIRECTDRAW dd=NULL; LPDIRECTDRAWSURFACE sys=NULL,vid=NULL; DDSURFACEDESC d; DDCOLORKEY ck; HRESULT hr; int ok=0,x,bad=0;
 hr=DirectDrawCreate(NULL,&dd,NULL); if(FAILED(hr)) {logline("   DirectDrawCreate %08lX\n",(DWORD)hr); return 0;}
 IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL);
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; d.dwWidth=64; d.dwHeight=4; d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY;
 hr=IDirectDraw_CreateSurface(dd,&d,&sys,NULL); logline("   system surface %08lX\n",(DWORD)hr); if(FAILED(hr)) goto out;
 d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY; hr=IDirectDraw_CreateSurface(dd,&d,&vid,NULL); logline("   video surface %08lX\n",(DWORD)hr); if(FAILED(hr)) goto out;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 if(SUCCEEDED(IDirectDrawSurface_Lock(vid,NULL,&d,DDLOCK_WAIT,NULL))) {for(x=0;x<64;x++) ((DWORD*)d.lpSurface)[x]=0x00102030UL; IDirectDrawSurface_Unlock(vid,d.lpSurface);}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 if(SUCCEEDED(IDirectDrawSurface_Lock(sys,NULL,&d,DDLOCK_WAIT,NULL))) {for(x=0;x<64;x++) ((DWORD*)d.lpSurface)[x]=(x&1)?0xFF001234UL:0x00ABCDEFUL; IDirectDrawSurface_Unlock(sys,d.lpSurface);}
 ck.dwColorSpaceLowValue=ck.dwColorSpaceHighValue=0x00001234UL; hr=IDirectDrawSurface_SetColorKey(sys,DDCKEY_SRCBLT,&ck); logline("   SetColorKey %08lX\n",(DWORD)hr);
 hr=IDirectDrawSurface_Blt(vid,NULL,sys,NULL,DDBLT_WAIT|DDBLT_KEYSRC,NULL); logline("   Blt system -> video with source key: %08lX\n",(DWORD)hr);
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 if(SUCCEEDED(hr)&&SUCCEEDED(IDirectDrawSurface_Lock(vid,NULL,&d,DDLOCK_WAIT,NULL))) {
  for(x=0;x<64;x++) {DWORD v=((DWORD*)d.lpSurface)[x]&0x00FFFFFFUL,want=(x&1)?0x00102030UL:0x00ABCDEFUL; if(v!=want) {if(!bad) logline("   pixel %d: %06lX, expected %06lX\n",x,v,want); bad++;}}
  IDirectDrawSurface_Unlock(vid,d.lpSurface); ok=!bad; logline("   %d of 64 pixels wrong (key pixels with FF in the top byte must be skipped)\n",bad);
 }
out:
 if(vid) IDirectDrawSurface_Release(vid); if(sys) IDirectDrawSurface_Release(sys); if(dd) IDirectDraw_Release(dd); return ok;
}
static void cr3_time(HDC dc) {
 double t[2]; int m,i,n=3000; HBRUSH b=CreateSolidBrush(RGB(10,200,90)),o=(HBRUSH)SelectObject(dc,b); DWORD c0[4],c1[4];
 for(m=0;m<2;m++) {double s; ctl(m?0xEE000000UL:0xEE000001UL,NULL); ctl(0xFFFFFFFEUL,c0); s=now(); for(i=0;i<n;i++) PatBlt(dc,(i*13)%500,(i*7)%340,32,32,PATINVERT); GdiFlush(); t[m]=(now()-s)*1000.0/n; ctl(0xFFFFFFFEUL,c1);
  logline("  CR3 reload %s: %.2f us per PatBlt PATINVERT 32x32 (%lu engine operations)\n",m?"OFF":"on ",t[m],c1[0]-c0[0]);}
 ctl(0xEE000001UL,NULL); SelectObject(dc,o); DeleteObject(b);
 logline("  difference: %.2f us per call (%s)\n",t[0]-t[1],t[1]<t[0]?"faster without the reload":"no gain");
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int sh) {
 WNDCLASSA wc; DWORD c[4]; int pass=0,total=0,ph,have; DEVMODEA origm; HDC dc; char res[200]; static const int depths[2]={16,32}; (void)hp; (void)cmd; (void)sh;
 lf=fopen("VX2FIX.TXT","w"); logline("VX2FIX v1.0 / driver v10.139\n"); finish("RUNNING: the run did not reach its end (VX2FIX.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL); if(dev==INVALID_HANDLE_VALUE) {finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xEE000001UL,c)||c[0]!=1) {finish("FAIL: VX85598.VXD older than v10.139"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2FIX"; RegisterClassA(&wc);
 for(ph=0;ph<2;ph++) {
  HCURSOR cc,ce;
  if(!set_mode(640,480,depths[ph])) continue;
  win=CreateWindowExA(0,"VX2FIX","VX2FIX",WS_POPUP|WS_VISIBLE,0,0,640,480,NULL,NULL,hi,NULL); UpdateWindow(win); wait_ms(300);
  dc=GetDC(NULL); cc=make_colour_cursor(dc,0); ce=make_colour_cursor(dc,1);
  logline("-- 640x480x%d: colour cursor\n",depths[ph]); ctl(0xFFFFFE15UL,c); logline("   hardware cursor mode %lu\n",c[0]);
  show_cursor(cc); total++; if(ask("The cursor is a 32x32 square of vertical stripes: red, green, blue, white, black, repeating. Are the colours right and the stripes clean (not garbled, not doubled, no wrong colours)?")) pass++;
  logline("-- 640x480x%d: fully transparent cursor\n",depths[ph]);
  show_cursor(ce); total++; if(ask("Now the cursor should be completely invisible over the grey and white halves (no box, no dots). Is it invisible?")) pass++;
  show_cursor(LoadCursor(NULL,IDC_ARROW));
  if(depths[ph]==32) {logline("-- 640x480x32: DirectDraw CPU source key\n"); total++; if(ddkey()) pass++; logline("-- 640x480x32: CR3 reload timing\n"); cr3_time(dc);}
  DestroyCursor(cc); DestroyCursor(ce); ReleaseDC(NULL,dc); DestroyWindow(win); cur=NULL;
 }
 ctl(0xFFFFFFFFUL,NULL);
 if(have) ChangeDisplaySettingsA(&origm,0);
 sprintf(res,"%s: %d of %d checks passed (CR3 timing in VX2FIX.TXT)",pass==total?"PASS":"FAIL",pass,total); finish(res); logline("%s\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
