/* VX2SWAP v1.1 (v1.1: the sprite comparison draws both sprites over the same background): leaving and coming back, for driver v10.123. (1) Alt-Tab out of an exclusive DirectDraw game: a 640x480x16 exclusive mode with a flip chain and an offscreen surface is put in the background
 * (the window is minimised, which is what Alt-Tab does to it), the desktop is checked, the game is brought back, its surfaces must report lost, Restore, and blits, a flip and a Lock read-back must be right.
 * (2) A full-screen DOS box: COMMAND.COM is started and switched to full screen with Alt+Enter, then closed with EXIT (if the automation does not work, the test asks you to do it by hand).
 * After each: the GDI binding of the driver, the 2D engine state, a large fill through the engine, a sprite through the staging block against DIBENG, the hardware cursor, and the gamma table (a slightly
 * brighter ramp is set before leaving and must be in force again). Results in VX2SWAP.TXT / .RES. */
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static HANDLE dev=INVALID_HANDLE_VALUE; static HWND win; static HINSTANCE hinst; static int fails,checks;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2SWAP.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}}
static void wait_ms(int ms) {DWORD t=GetTickCount(); while((int)(GetTickCount()-t)<ms) {pump(); Sleep(5);}}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static void check(int ok,const char *what) {checks++; if(!ok) fails++; logline("   %-70s %s\n",what,ok?"ok":"FAIL");}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {if(m==WM_ERASEBKGND) return 1; return DefWindowProc(w,m,a,b);}
static WORD bright[3][256];
/* ---- the state of the driver on the desktop ---- */
static void desktop_state(const char *when,int expect_gamma) {
 DWORD o[4],c0[4],c1[4]; HDC dc=GetDC(NULL); RECT r; HBRUSH b; COLORREF px; int same=1,x,y;
 logline("-- desktop state %s\n",when); wait_ms(1500);
 ctl(0xFFFFFE10UL,o); check((o[0]&1)&&!(o[0]&2)&&!(o[0]&4),"GDI acceleration bound and enabled, DRV not busy");
 ctl(0xFFFFFE11UL,o); logline("      engine state %lu, recoveries %lu, retries %lu\n",o[2],o[0],o[1]); check(o[2]!=3,"2D engine not lost");
 ctl(0xFFFFFFFEUL,c0); b=CreateSolidBrush(RGB(30,160,90)); r.left=0; r.top=0; r.right=320; r.bottom=200; FillRect(dc,&r,b); DeleteObject(b); GdiFlush(); ctl(0xFFFFFFFEUL,c1);
 px=GetPixel(dc,160,100); check(px==RGB(30,160,90)||(GetDeviceCaps(dc,BITSPIXEL)==16&&(px&0xF8FCF8)==(RGB(30,160,90)&0xF8FCF8)),"a 320x200 fill is right");
 check(c1[0]>c0[0],"the fill went through the 2D engine");
 { /* a sprite through the staging block against DIBENG (live mask 0) */
  HDC m=CreateCompatibleDC(dc); HBITMAP mk=CreateBitmap(64,64,1,1,NULL),cb=CreateCompatibleBitmap(dc,64,64),o1; HDC t=CreateCompatibleDC(dc); HBITMAP tb=CreateCompatibleBitmap(dc,128,64),o2; DWORD s0[4],s1[4];
  o1=(HBITMAP)SelectObject(m,mk); PatBlt(m,0,0,64,64,WHITENESS); Ellipse(m,4,4,60,60); SelectObject(m,cb); for(y=0;y<64;y+=8) for(x=0;x<64;x+=8) {HBRUSH q=CreateSolidBrush(RGB(x*4,y*4,200)); RECT rr; rr.left=x; rr.top=y; rr.right=x+8; rr.bottom=y+8; FillRect(m,&rr,q); DeleteObject(q);}
  {RECT rr; HBRUSH q=CreateSolidBrush(RGB(70,110,150)); rr.left=400; rr.top=10; rr.right=544; rr.bottom=74; FillRect(dc,&rr,q); DeleteObject(q); GdiFlush();} /* v1.1: the same background under both sprites (SRCAND/SRCINVERT read it) */
  ctl(0xFFFFFE18UL,s0);
  for(x=0;x<2;x++) {ctl(0xF7000000UL|(x?0UL:22UL),NULL); SetTextColor(dc,0); SetBkColor(dc,RGB(255,255,255)); SelectObject(m,mk); BitBlt(dc,400+x*80,10,64,64,m,0,0,SRCAND); SelectObject(m,cb); BitBlt(dc,400+x*80,10,64,64,m,0,0,SRCINVERT);}
  ctl(0xFFFFFFFFUL,NULL); ctl(0xFFFFFE18UL,s1); GdiFlush();
  for(y=0;y<64&&same;y++) for(x=0;x<64;x++) if(GetPixel(dc,400+x,10+y)!=GetPixel(dc,480+x,10+y)) {same=0; break;}
  check(s1[1]>s0[1],"a sprite blit went through the staging block"); check(same,"that sprite equals DIBENG's");
  SelectObject(m,o1); DeleteObject(mk); DeleteObject(cb); DeleteDC(m); (void)t; (void)tb; (void)o2; DeleteDC(t); DeleteObject(tb);
 }
 { /* hardware cursor: mode, and moves reaching it */
  DWORD h0[4],h1[4],mv0[4],mv1[4]; POINT p; GetCursorPos(&p); ctl(0xFFFFFE15UL,h0); ctl(0xFFFFFE16UL,mv0); SetCursorPos(p.x+40,p.y+20); wait_ms(100); SetCursorPos(p.x,p.y); wait_ms(100); ctl(0xFFFFFE15UL,h1); ctl(0xFFFFFE16UL,mv1);
  logline("      hardware cursor: mode %lu, shown %lu, moves +%lu, shapes left to software %lu\n",h1[0],mv1[3],mv1[0]-mv0[0],h1[2]);
  if(h1[0]) {check(mv1[0]>mv0[0],"the hardware cursor follows the mouse"); check(mv1[3]==1,"the hardware cursor is shown");}
 }
 if(expect_gamma) {DWORD g[4]; ctl(0xFFFFFE1AUL,g); logline("      gamma: %lu loads, CR33 %02lX, red[128] %04lX\n",g[0],g[1],g[3]); check((g[1]&0x80)&&g[3]==bright[0][128],"the gamma table is in force again (CR33 bit 7, the ramp set before leaving)");}
 ReleaseDC(NULL,dc);
}
/* ---- 1: Alt-Tab out of an exclusive DirectDraw mode ---- */
static int fillsurf(LPDIRECTDRAWSURFACE s,WORD v) {DDBLTFX fx; memset(&fx,0,sizeof(fx)); fx.dwSize=sizeof(fx); fx.dwFillColor=v; return SUCCEEDED(IDirectDrawSurface_Blt(s,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&fx));}
static void alt_tab(void) {
 LPDIRECTDRAW dd=NULL; LPDIRECTDRAWSURFACE prim=NULL,back=NULL,off=NULL; DDSURFACEDESC d; DDSCAPS sc; HRESULT hr; int i;
 logline("== 1. Alt-Tab out of an exclusive DirectDraw mode ==\n");
 hr=DirectDrawCreate(NULL,&dd,NULL); if(FAILED(hr)) {logline("   DirectDrawCreate %08lX\n",(DWORD)hr); check(0,"DirectDraw"); return;}
 hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN); logline("   SetCooperativeLevel %08lX\n",(DWORD)hr);
 hr=IDirectDraw_SetDisplayMode(dd,640,480,16); logline("   SetDisplayMode(640x480x16) %08lX\n",(DWORD)hr); check(SUCCEEDED(hr),"exclusive 640x480x16");
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX; d.dwBackBufferCount=1;
 hr=IDirectDraw_CreateSurface(dd,&d,&prim,NULL); logline("   flip chain %08lX\n",(DWORD)hr);
 if(SUCCEEDED(hr)) {memset(&sc,0,sizeof(sc)); sc.dwCaps=DDSCAPS_BACKBUFFER; IDirectDrawSurface_GetAttachedSurface(prim,&sc,&back);}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY; d.dwWidth=128; d.dwHeight=128; IDirectDraw_CreateSurface(dd,&d,&off,NULL);
 if(!prim||!back||!off) {check(0,"surfaces created"); goto done;}
 fillsurf(off,0x07E0); fillsurf(back,0x001F); IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT); wait_ms(500);
 logline("   to the background (the window is minimised, as Alt-Tab does)\n");
 ShowWindow(win,SW_MINIMIZE); wait_ms(2500);
 desktop_state("while the game is in the background",1);
 logline("   back to the game\n");
 ShowWindow(win,SW_RESTORE); SetForegroundWindow(win); wait_ms(2500);
 hr=IDirectDrawSurface_IsLost(prim); logline("   primary IsLost %08lX\n",(DWORD)hr); check(hr==DDERR_SURFACELOST||hr==DD_OK,"the primary reports lost (or was restored by DirectDraw)");
 for(i=0;i<20&&FAILED(IDirectDrawSurface_Restore(prim));i++) wait_ms(200);
 hr=IDirectDrawSurface_Restore(prim); check(SUCCEEDED(hr),"Restore of the flip chain"); hr=IDirectDrawSurface_Restore(off); check(SUCCEEDED(hr),"Restore of the offscreen surface");
 fillsurf(off,0xF800); fillsurf(back,0x0000);
 hr=IDirectDrawSurface_BltFast(back,100,100,off,NULL,DDBLTFAST_WAIT|DDBLTFAST_NOCOLORKEY); check(SUCCEEDED(hr),"a blit after the return");
 hr=IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT); check(SUCCEEDED(hr),"a flip after the return");
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 if(SUCCEEDED(IDirectDrawSurface_Lock(prim,NULL,&d,DDLOCK_WAIT|DDLOCK_READONLY,NULL))) {
  WORD *p=(WORD*)d.lpSurface; WORD in=p[(150*d.lPitch)/2+150],out=p[(50*d.lPitch)/2+50]; IDirectDrawSurface_Unlock(prim,NULL);
  logline("   read back: inside the blit %04X (F800 expected), outside %04X (0000 expected)\n",in,out); check(in==0xF800&&out==0,"the shown picture is right after the return");
 } else check(0,"Lock of the primary after the return");
done:
 if(off) IDirectDrawSurface_Release(off); if(prim) IDirectDrawSurface_Release(prim);
 if(dd) {IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd);}
 ShowWindow(win,SW_HIDE); wait_ms(1500);
 desktop_state("after the game ended",1);
}
/* ---- 2: a full-screen DOS box ---- */
static void key(BYTE vk,int up) {keybd_event(vk,(BYTE)MapVirtualKey(vk,0),up?KEYEVENTF_KEYUP:0,0);}
static void dos_box(void) {
 HWND t=NULL; int i;
 logline("== 2. A full-screen DOS box ==\n");
 WinExec("COMMAND.COM",SW_SHOWNORMAL);
 for(i=0;i<50&&!t;i++) {wait_ms(200); t=FindWindowA("tty",NULL);}
 if(t) {
  logline("   DOS box window found: Alt+Enter (full screen), 3 s, Alt+Enter, EXIT\n");
  SetForegroundWindow(t); wait_ms(800); key(VK_MENU,0); key(VK_RETURN,0); key(VK_RETURN,1); key(VK_MENU,1); wait_ms(3000);
  key(VK_MENU,0); key(VK_RETURN,0); key(VK_RETURN,1); key(VK_MENU,1); wait_ms(2000);
  {const char *e="EXIT"; for(i=0;e[i];i++) {key((BYTE)e[i],0); key((BYTE)e[i],1);} key(VK_RETURN,0); key(VK_RETURN,1);}
  wait_ms(2000);
 }
 if(!t||FindWindowA("tty",NULL)) MessageBoxA(NULL,"Please do this by hand now: switch the DOS box to full screen with Alt+Enter, wait a moment, then type EXIT and press Enter. Click OK when you are back on the desktop.","VX2SWAP",MB_OK|MB_TOPMOST|MB_SETFOREGROUND);
 checks++; if(MessageBoxA(NULL,"Did the DOS prompt show full screen (text mode, normal colours), and is the desktop back and normal?","VX2SWAP",MB_YESNO|MB_TOPMOST|MB_SETFOREGROUND)==IDYES) logline("   full-screen DOS box seen and the desktop back: yes\n"); else {fails++; logline("   full-screen DOS box seen and the desktop back: NO\n");}
 desktop_state("after the DOS box",1);
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 WNDCLASSA wc; HDC dc; WORD saved[3][256]; int i; char res[200]; (void)hp; (void)cmd; (void)show;
 hinst=hi; lf=fopen("VX2SWAP.TXT","w"); logline("VX2SWAP v1.1 / driver v10.123\n"); finish("RUNNING: the run did not reach its end (VX2SWAP.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL); if(dev==INVALID_HANDLE_VALUE) {finish("FAIL: driver not found"); return 1;}
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2SWAP"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH); RegisterClassA(&wc);
 win=CreateWindowExA(0,"VX2SWAP","VX2SWAP",WS_POPUP|WS_VISIBLE,0,0,640,480,NULL,NULL,hi,NULL);
 dc=GetDC(NULL); GetDeviceGammaRamp(dc,saved);
 for(i=0;i<256;i++) {double v=pow(i/255.0,0.85); bright[0][i]=bright[1][i]=bright[2][i]=(WORD)(v*65535.0);}
 logline("a slightly brighter gamma ramp (0.85) is set before leaving: %s\n",SetDeviceGammaRamp(dc,bright)?"ok":"FAILED");
 ShowWindow(win,SW_HIDE); desktop_state("at the start",1);
 ShowWindow(win,SW_SHOW); SetForegroundWindow(win); alt_tab();
 dos_box();
 SetDeviceGammaRamp(dc,saved); ReleaseDC(NULL,dc); DestroyWindow(win);
 sprintf(res,"%s: %d of %d checks passed (see VX2SWAP.TXT)",fails?"FAIL":"PASS",checks-fails,checks);
 finish(res); logline("%s\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
