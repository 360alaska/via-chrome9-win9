/* VX2CUR v1.0: hardware cursor (driver v10.105).
 * The VX855 can show a 64x64 ARGB cursor image on its own, without the driver drawing it into the frame buffer. Two register sets exist
 * (the primary display's and the second display's); the program tries them one after the other. For each one it switches the driver's
 * hardware cursor on, moves the Windows arrow across the screen (the arrow must be a clean arrow that follows the mouse position, its
 * tip on the red square), shows the I-beam (a shape the hardware cursor cannot draw: the driver must keep drawing it itself) and then
 * asks you whether you saw it. With a yes it writes HKLM\Software\VX85598 ACCEL_HWCURSOR=1 or 2 (restart Windows to use it).
 * It also times 300 mouse moves with and without the hardware cursor. Needs the desktop at 16 or 32 bits per pixel.
 * The live setting is put back to the registry value at the end. Writes VX2CUR.TXT and VX2CUR.RES. Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <io.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"
#include "vx855_2d.h"
static HANDLE dev=INVALID_HANDLE_VALUE;
static FILE *lf;
static HWND win;
static DWORD aborted;
static LARGE_INTEGER freq;
static int sw,sh;
#define MX 200
#define MY 150
static void logline(const char *fmt,...) {
 va_list ap; va_start(ap,fmt); vfprintf(lf,fmt,ap); va_end(ap); fflush(lf); _commit(_fileno(lf));
}
static void finish(const char *res) {FILE *f=fopen("VX2CUR.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static double now(void) {
 LARGE_INTEGER t;
 if(freq.QuadPart&&QueryPerformanceCounter(&t)) return 1000.0*(double)t.QuadPart/(double)freq.QuadPart;
 return (double)GetTickCount();
}
static int ctl(DWORD code,DWORD *out) {
 DWORD o[4],got=0; memset(o,0,sizeof(o));
 if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0;
 if(out) memcpy(out,o,16);
 return 1;
}
static void pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_CLOSE) {aborted=1; return 0;}
 if(m==WM_PAINT) {
  PAINTSTRUCT ps; HDC dc=BeginPaint(w,&ps); RECT r; HBRUSH bg=CreateSolidBrush(RGB(0,40,90)),red=CreateSolidBrush(RGB(255,0,0));
  GetClientRect(w,&r); FillRect(dc,&r,bg);
  r.left=MX; r.top=MY; r.right=MX+8; r.bottom=MY+8; FillRect(dc,&r,red);
  SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255));
  TextOutA(dc,24,24,"VX2CUR: the arrow tip must sit on the corner of the red square when it is placed there",88);
  DeleteObject(bg); DeleteObject(red); EndPaint(w,&ps); return 0;
 }
 if(m==WM_ERASEBKGND) return 1;
 return DefWindowProc(w,m,a,b);
}
static void wait_ms(int ms) {double t=now(); while(now()-t<ms&&!aborted) {pump(); Sleep(5);}}
static void put(int x,int y) {SetCursorPos(x,y); pump();}
/* one pass with a register set: returns 1 when the user saw what was expected */
static int try_mode(int mode) {
 DWORD c[4],m1[4],m2[4],i; int yes=0; char msg[512];
 ctl(0xF9000000UL|(DWORD)mode,c);
 logline("-- register set %d: live mode %lu, image block at video memory offset %08lX\n",mode,c[0],c[3]);
 if(c[0]!=(DWORD)mode) {logline("   the driver did not take the live mode\n"); return 0;}
 ctl(0xFFFFFE16UL,m1);
 SetCursor(LoadCursor(NULL,IDC_ARROW));
 put(MX,MY); wait_ms(1500);
 for(i=0;i<=100&&!aborted;i++) {put((int)((long)(sw-60)*i/100)+20,(int)((long)(sh-60)*i/100)+20); Sleep(30);}
 put(sw/2,sh/2); wait_ms(500);
 put(MX,MY); wait_ms(2000);
 SetCursor(LoadCursor(NULL,IDC_IBEAM)); put(sw/2,sh/2); wait_ms(300); put(sw/2+3,sh/2+3); wait_ms(1500);
 SetCursor(LoadCursor(NULL,IDC_ARROW)); put(MX,MY); wait_ms(1000);
 ctl(0xFFFFFE16UL,m2); ctl(0xFFFFFE15UL,c);
 logline("   hardware cursor: %lu moves, %lu shape loads, %lu hides, shown now %lu; shapes left to the software cursor %lu, switched on %lu times\n",m2[0]-m1[0],m2[1]-m1[1],m2[2]-m1[2],m2[3],c[2],c[1]);
 if(m2[1]==m1[1]||m2[0]==m1[0]) {logline("   the driver never used the hardware cursor (arrow shape or screen not eligible)\n"); return 0;}
 SetCursor(LoadCursor(NULL,IDC_ARROW)); put(MX,MY);
 sprintf(msg,"Register set %d.\n\nWhile this ran the arrow should have followed the mouse position as one clean arrow (no flicker, no trail, no second copy), with its tip on the red square corner when it stood still, and the I-beam should have been visible too.\n\nDid you see that? (Yes = OK, No = no cursor, wrong place or damaged.)",mode);
 yes=MessageBox(win,msg,"VX2CUR",MB_YESNO|MB_ICONQUESTION|MB_TOPMOST)==IDYES;
 logline("   you answered: %s\n",yes?"yes":"no");
 return yes;
}
static double moves(void) { /* ms for 300 moves with the arrow */
 int i; double t; SetCursor(LoadCursor(NULL,IDC_ARROW)); put(100,100); wait_ms(200);
 t=now(); for(i=0;i<300&&!aborted;i++) {SetCursorPos(100+i,100+i/2); pump();}
 return now()-t;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE pi,LPSTR cmd,int show) {
 DWORD c[4],orig; HDC dc; int bits,found=0,mode; WNDCLASSA wc; double t0,t1;
 (void)pi;(void)cmd;(void)show;
 lf=fopen("VX2CUR.TXT","w"); if(!lf) return 1;
 finish("RUNNING");
 QueryPerformanceFrequency(&freq);
 logline("VX2CUR v1.0 / driver v10.105\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xFFFFFE15UL,c)) {logline("the driver has no hardware cursor control\n"); finish("FAIL: VX85598.VXD older than v10.105"); return 1;}
 orig=c[0];
 logline("hardware cursor setting at start: %lu (ACCEL_HWCURSOR in the registry), image block at %08lX\n",orig,c[3]);
 if(!c[3]) {logline("no image block: the video memory is too small or the colour block is missing\n"); finish("FAIL: no video memory block for the cursor image"); return 1;}
 dc=GetDC(NULL); bits=GetDeviceCaps(dc,BITSPIXEL)*GetDeviceCaps(dc,PLANES); sw=GetDeviceCaps(dc,HORZRES); sh=GetDeviceCaps(dc,VERTRES); ReleaseDC(NULL,dc);
 logline("desktop %dx%dx%d\n",sw,sh,bits);
 if(bits!=16&&bits!=32) {logline("the hardware cursor is used at 16 and 32 bits per pixel only\n"); finish("FAIL: set the desktop to 16 or 32 bits per pixel"); return 1;}
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2CUR"; wc.hCursor=NULL; RegisterClassA(&wc);
 win=CreateWindowExA(WS_EX_TOPMOST,"VX2CUR","VX2CUR",WS_POPUP|WS_VISIBLE,0,0,sw,sh,NULL,NULL,hi,NULL);
 if(!win) {finish("FAIL: no window"); return 1;}
 pump(); UpdateWindow(win); wait_ms(500);
 ctl(0xF9000000UL,NULL); t0=moves();
 logline("300 mouse moves with the software cursor: %.1f ms\n",t0);
 for(mode=1;mode<=2&&!found&&!aborted;mode++) {
  if(try_mode(mode)) {
   found=mode; ctl(0xF9000000UL|(DWORD)mode,NULL); t1=moves();
   logline("300 mouse moves with the hardware cursor (register set %d): %.1f ms\n",mode,t1);
  } else {ctl(0xF9000000UL,NULL); pump(); wait_ms(300);}
 }
 ctl(0xFFFFFFFFUL,NULL); /* the registry values again */
 DestroyWindow(win);
 if(aborted) {logline("stopped\n"); finish("STOPPED: nothing written"); return 0;}
 if(found) {
  {char r[160]; logline("register set %d correct\n",found); sprintf(r,"PASS: register set %d gave a correct hardware cursor (.533: nothing written, the driver reads no ACCEL_HWCURSOR)",found); finish(r);}
 } else {logline("neither register set showed a correct cursor: nothing written\n"); finish("FAIL: no register set gave a correct hardware cursor (nothing written)");}
 return 0;
}
