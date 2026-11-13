/* VX2GAMMA v1.0: the hardware gamma ramp of driver v10.123 (CR33 bit 7, the DAC as the gamma table at 16 and 32 bpp). Through GDI (SetDeviceGammaRamp, what games and DirectDraw on Windows 98 use):
 * darker, brighter, red only, a fade to black and back, the ramp read back; through DirectDraw's gamma control in an exclusive 640x480x16 mode; at 640x480x8 the palette must stay as it is.
 * Each picture is judged by eye (Yes/No; in the DirectDraw part keys Y and N). The driver's counters (loads, CR33 read back) are logged. The ramp is put back at the end. Results in VX2GAMMA.TXT / .RES. */
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static HANDLE dev=INVALID_HANDLE_VALUE; static HWND win; static int aborted;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2GAMMA.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}}
static void wait_ms(int ms) {DWORD t=GetTickCount(); while((int)(GetTickCount()-t)<ms) {pump(); Sleep(5);}}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static void counters(const char *when) {DWORD c[4]; if(ctl(0xFFFFFE1AUL,c)) logline("   driver: %lu table loads, CR33 %02lX (bit 7 %s), gamma switch %lu, red[128] %04lX (%s)\n",c[0],c[1],(c[1]&0x80)?"on":"off",c[2],c[3],when);}
static int ask(const char *q) {int r; logline("   question: %s\n",q); r=MessageBoxA(win,q,"VX2GAMMA",MB_YESNO|MB_TOPMOST|MB_SETFOREGROUND); logline("   answer: %s\n",r==IDYES?"yes":"no"); return r==IDYES;}
static void paint(HDC dc,int w,int h) { /* a grey ramp of 32 steps, colour bars, a white and a black square */
 int i; RECT r; static const COLORREF bars[8]={RGB(255,255,255),RGB(255,255,0),RGB(0,255,255),RGB(0,255,0),RGB(255,0,255),RGB(255,0,0),RGB(0,0,255),RGB(0,0,0)};
 r.left=0; r.top=0; r.right=w; r.bottom=h; FillRect(dc,&r,(HBRUSH)GetStockObject(GRAY_BRUSH));
 for(i=0;i<32;i++) {HBRUSH b=CreateSolidBrush(RGB(i*255/31,i*255/31,i*255/31)); r.left=i*w/32; r.right=(i+1)*w/32; r.top=h/8; r.bottom=h*3/8; FillRect(dc,&r,b); DeleteObject(b);}
 for(i=0;i<8;i++) {HBRUSH b=CreateSolidBrush(bars[i]); r.left=i*w/8; r.right=(i+1)*w/8; r.top=h/2; r.bottom=h*3/4; FillRect(dc,&r,b); DeleteObject(b);}
}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_PAINT) {PAINTSTRUCT ps; HDC dc=BeginPaint(w,&ps); RECT r; GetClientRect(w,&r); paint(dc,r.right,r.bottom); EndPaint(w,&ps); return 0;}
 if(m==WM_ERASEBKGND) return 1;
 if(m==WM_CLOSE) {aborted=1; return 0;}
 return DefWindowProc(w,m,a,b);
}
static WORD ramp[3][256],saved[3][256];
static void make(double g,double kr,double kg,double kb,double scale) {int i; for(i=0;i<256;i++) {double v=pow(i/255.0,g)*scale; if(v>1) v=1; ramp[0][i]=(WORD)(v*kr*65535.0); ramp[1][i]=(WORD)(v*kg*65535.0); ramp[2][i]=(WORD)(v*kb*65535.0);}}
static int setramp(HDC dc,const char *name) {
 BOOL ok=SetDeviceGammaRamp(dc,ramp); WORD back[3][256]; BOOL got; int same;
 memset(back,0,sizeof(back)); got=GetDeviceGammaRamp(dc,back); same=got&&!memcmp(back,ramp,sizeof(ramp));
 logline("   SetDeviceGammaRamp(%s): %s; read back %s\n",name,ok?"ok":"FAILED",!got?"FAILED":same?"the same":"DIFFERENT");
 counters(name); return ok;
}
/* ---- DirectDraw gamma control, keys Y / N ---- */
static const GUID IIDGamma={0x69C11C3E,0xB46B,0x11D1,{0xAD,0x7A,0x00,0xC0,0x4F,0xC2,0x9B,0x4E}};
static int key_yn(void) {DWORD t=GetTickCount(); for(;;) {pump(); if(GetAsyncKeyState('Y')&0x8000) return 1; if(GetAsyncKeyState('N')&0x8000) return 0; if(GetTickCount()-t>60000) return 0; Sleep(20);}}
static int ddraw_part(void) {
 LPDIRECTDRAW dd=NULL; LPDIRECTDRAWSURFACE prim=NULL; LPDIRECTDRAWGAMMACONTROL gc=NULL; DDSURFACEDESC d; DDGAMMARAMP r,keep; HRESULT hr; HDC dc; int ok=0,i;
 logline("-- DirectDraw: exclusive 640x480x16, IDirectDrawGammaControl on the primary surface\n");
 hr=DirectDrawCreate(NULL,&dd,NULL); logline("   DirectDrawCreate: %08lX\n",(DWORD)hr); if(FAILED(hr)) return 0;
 hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN); logline("   SetCooperativeLevel: %08lX\n",(DWORD)hr); if(FAILED(hr)) goto done;
 hr=IDirectDraw_SetDisplayMode(dd,640,480,16); logline("   SetDisplayMode: %08lX\n",(DWORD)hr); if(FAILED(hr)) goto done;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE; hr=IDirectDraw_CreateSurface(dd,&d,&prim,NULL); logline("   primary: %08lX\n",(DWORD)hr); if(FAILED(hr)) goto done;
 hr=IDirectDrawSurface_QueryInterface(prim,&IIDGamma,(LPVOID*)&gc); logline("   QueryInterface(IDirectDrawGammaControl): %08lX\n",(DWORD)hr); if(FAILED(hr)||!gc) goto done;
 hr=IDirectDrawGammaControl_GetGammaRamp(gc,0,&keep); logline("   GetGammaRamp: %08lX\n",(DWORD)hr);
 if(SUCCEEDED(IDirectDrawSurface_GetDC(prim,&dc))) {paint(dc,640,480); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255)); TextOut(dc,20,440,"Red only now? press Y (yes) or N (no)",37); IDirectDrawSurface_ReleaseDC(prim,dc);}
 for(i=0;i<256;i++) {r.red[i]=(WORD)(i*257); r.green[i]=0; r.blue[i]=0;}
 hr=IDirectDrawGammaControl_SetGammaRamp(gc,0,&r); logline("   SetGammaRamp(red only): %08lX\n",(DWORD)hr); counters("DirectDraw red only");
 if(SUCCEEDED(hr)) {wait_ms(500); logline("   question (keys): only red shades on the screen?\n"); ok=key_yn(); logline("   answer: %s\n",ok?"yes":"no");}
 IDirectDrawGammaControl_SetGammaRamp(gc,0,&keep);
done:
 if(gc) IDirectDrawGammaControl_Release(gc);
 if(prim) IDirectDrawSurface_Release(prim);
 if(dd) {IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd);}
 wait_ms(800); return ok;
}
static int set_mode(DWORD w,DWORD h,DWORD b) {DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL; rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); wait_ms(1000); return rc==DISP_CHANGE_SUCCESSFUL;}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 WNDCLASSA wc; HDC dc; int pass=0,total=0,i; DEVMODEA orig; int have; char res[300]; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2GAMMA.TXT","w"); logline("VX2GAMMA v1.0 / driver v10.123\n"); finish("RUNNING: the run did not reach its end (VX2GAMMA.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {finish("FAIL: driver not found"); return 1;}
 {DWORD c[4]; if(!ctl(0xFFFFFE1AUL,c)) {finish("FAIL: no control interface"); return 1;} if(c[2]!=1) logline("gamma switch reads %lu (ACCEL_GAMMA=0 or a VxD older than v10.123)\n",c[2]);}
 memset(&orig,0,sizeof(orig)); orig.dmSize=sizeof(orig); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&orig)?1:0;
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2GAMMA"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClassA(&wc);
 if(!set_mode(640,480,16)) logline("640x480x16 not available: the GDI part runs at the current mode\n");
 win=CreateWindowExA(WS_EX_TOPMOST,"VX2GAMMA","VX2GAMMA",WS_POPUP|WS_VISIBLE,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,hi,NULL);
 UpdateWindow(win); wait_ms(500);
 dc=GetDC(NULL);
 if(!GetDeviceGammaRamp(dc,saved)) {logline("GetDeviceGammaRamp FAILED: the driver does not offer gamma\n"); for(i=0;i<256;i++) saved[0][i]=saved[1][i]=saved[2][i]=(WORD)(i*257);}
 logline("ramp at start: red[0] %04X red[128] %04X red[255] %04X\n",saved[0][0],saved[0][128],saved[0][255]); counters("start");
 logline("-- GDI, 16 bpp\n");
 make(1.0,1,1,1,1); setramp(dc,"identity"); total++; if(ask("Identity ramp: does the picture look normal (grey steps from black to white, the colour bars as usual)?")) pass++;
 make(2.2,1,1,1,1); setramp(dc,"darker (gamma 2.2)"); total++; if(ask("Is the whole picture clearly DARKER now (dark grey steps nearly black)?")) pass++;
 make(0.45,1,1,1,1); setramp(dc,"brighter (gamma 0.45)"); total++; if(ask("Is the whole picture clearly BRIGHTER now (dark grey steps lifted)?")) pass++;
 make(1.0,1,0,0,1); setramp(dc,"red only"); total++; if(ask("Is the picture in shades of RED only (no green, no blue)?")) pass++;
 {int k; for(k=32;k>=0;k--) {make(1.0,1,1,1,k/32.0); SetDeviceGammaRamp(dc,ramp); wait_ms(25);} counters("faded to black"); wait_ms(400); for(k=0;k<=32;k++) {make(1.0,1,1,1,k/32.0); SetDeviceGammaRamp(dc,ramp); wait_ms(25);}}
 total++; if(ask("The screen faded to black and back again (about 1.5 seconds). Did it fade smoothly all the way to black?")) pass++;
 SetDeviceGammaRamp(dc,saved); counters("put back");
 ReleaseDC(NULL,dc);
 total++; if(ddraw_part()) pass++;
 if(set_mode(640,480,8)) {
  dc=GetDC(NULL); InvalidateRect(win,NULL,TRUE); UpdateWindow(win); wait_ms(300);
  logline("-- 8 bpp (the DAC is the palette: gamma must not change it)\n");
  make(1.0,1,0,0,1); {BOOL ok=SetDeviceGammaRamp(dc,ramp); logline("   SetDeviceGammaRamp(red only) at 8 bpp: %s\n",ok?"accepted":"refused");} counters("8 bpp");
  total++; if(ask("640x480 with 256 colours: does the picture still show its NORMAL colours (not red only)?")) pass++;
  SetDeviceGammaRamp(dc,saved); ReleaseDC(NULL,dc);
 }
 DestroyWindow(win);
 if(have) {ChangeDisplaySettingsA(&orig,0); logline("desktop put back\n");}
 sprintf(res,"%s: %d of %d checks passed (see VX2GAMMA.TXT)",pass==total?"PASS":"FAIL",pass,total);
 finish(res); logline("%s\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
