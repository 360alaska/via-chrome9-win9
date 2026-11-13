/* VX2SAVE v1.0: SaveScreenBitmap of driver v10.124 (USER keeps the screen under a menu; the driver copies it to video memory and back with the engine). Popup menus of several sizes are opened and closed
 * (closed with Esc by a timer) over a fixed picture, with the save-under off (live ACCEL_HOST without bit 8: USER does it its own way) and on (bit 8); the screen under each menu is read back before and
 * after and must be the same, and the driver's counters must show saves and restores in the "on" run. Then both ways are timed. Nothing is written to the registry (ACCEL_HOST 30 turns it on for good).
 * Results in VX2SAVE.TXT / .RES. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static HANDLE dev=INVALID_HANDLE_VALUE; static HWND win; static HDC sdc,tdc; static HBITMAP tbm; static DWORD *tb,*ref; static int W,H;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2SAVE.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}}
static void wait_ms(int ms) {DWORD t=GetTickCount(); while((int)(GetTickCount()-t)<ms) {pump(); Sleep(5);}}
static double now(void) {LARGE_INTEGER c,f; if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;} return (double)GetTickCount();}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_PAINT) {PAINTSTRUCT ps; HDC dc=BeginPaint(w,&ps); int x,y; for(y=0;y<H;y+=12) for(x=0;x<W;x+=24) {RECT r; HBRUSH q=CreateSolidBrush(RGB((x*3+y)&255,(y*2+x/3)&255,(x+y*5)&255)); r.left=x; r.top=y; r.right=x+24; r.bottom=y+12; FillRect(dc,&r,q); DeleteObject(q);} EndPaint(w,&ps); return 0;}
 if(m==WM_ERASEBKGND) return 1;
 if(m==WM_TIMER) {KillTimer(w,1); keybd_event(VK_ESCAPE,0,0,0); keybd_event(VK_ESCAPE,0,KEYEVENTF_KEYUP,0); return 0;}
 return DefWindowProc(w,m,a,b);
}
static void shot(DWORD *to) {GdiFlush(); BitBlt(tdc,0,0,W,H,sdc,0,0,SRCCOPY); GdiFlush(); memcpy(to,tb,(size_t)W*H*4);}
static HMENU mk(int items) {HMENU m=CreatePopupMenu(); int i; char t[64]; for(i=0;i<items;i++) {sprintf(t,"Menu item number %d with some text",i+1); AppendMenuA(m,MF_STRING,100+i,t);} return m;}
static int run(HMENU m,int x,int y,DWORD mask,int *diff) {
 int i,d=0; shot(ref);
 ctl(0xF7000000UL|mask,NULL);
 SetForegroundWindow(win); SetTimer(win,1,150,NULL);
 TrackPopupMenu(m,TPM_LEFTALIGN|TPM_TOPALIGN,x,y,0,win,NULL);
 wait_ms(250); UpdateWindow(win); wait_ms(50);
 shot(tb); for(i=0;i<W*H;i++) if((ref[i]^tb[i])&0xFFFFFF) d++;
 *diff=d; return d==0;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 WNDCLASSA wc; BITMAPINFOHEADER bi; void *p=NULL; DWORD c0[4],c1[4],k[4]; int fails=0,tests=0,mode,s,d; HMENU menus[3]; static const int items[3]={4,12,28}; static const int at[3][2]={{40,40},{300,100},{500,60}};
 double t[2]; char res[200]; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2SAVE.TXT","w"); logline("VX2SAVE v1.0 / driver v10.124\n"); finish("RUNNING: the run did not reach its end (VX2SAVE.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL); if(dev==INVALID_HANDLE_VALUE) {finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xFFFFFE1BUL,k)) {finish("FAIL: no control interface"); return 1;} logline("save block at %08lX (0 = none: the driver cannot do it)\n",k[3]);
 ctl(0xFFFFFE19UL,k); logline("live ACCEL_HOST mask at the start %lu\n",k[2]);
 W=GetSystemMetrics(SM_CXSCREEN); H=GetSystemMetrics(SM_CYSCREEN); sdc=GetDC(NULL);
 logline("desktop %dx%d, %d bits per pixel; RASTERCAPS RC_SAVEBITMAP %s\n",W,H,GetDeviceCaps(sdc,BITSPIXEL),(GetDeviceCaps(sdc,RASTERCAPS)&RC_SAVEBITMAP)?"yes":"NO (the VX85598.DRV is older than v10.124)");
 memset(&bi,0,sizeof(bi)); bi.biSize=sizeof(bi); bi.biWidth=W; bi.biHeight=-H; bi.biPlanes=1; bi.biBitCount=32;
 tbm=CreateDIBSection(sdc,(BITMAPINFO*)&bi,DIB_RGB_COLORS,&p,NULL,0); tdc=CreateCompatibleDC(sdc); SelectObject(tdc,tbm); tb=(DWORD*)p; ref=(DWORD*)malloc((size_t)W*H*4);
 if(!tb||!ref) {finish("FAIL: out of memory"); return 1;}
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2SAVE"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClassA(&wc);
 win=CreateWindowExA(0,"VX2SAVE","VX2SAVE",WS_POPUP|WS_VISIBLE,0,0,W,H,NULL,NULL,hi,NULL); UpdateWindow(win); wait_ms(600);
 SetCursorPos(W-5,H-5);
 for(s=0;s<3;s++) menus[s]=mk(items[s]);
 for(mode=0;mode<2;mode++) {
  DWORD mask=mode?30UL:22UL;
  logline("-- save-under %s (live mask %lu)\n",mode?"ON":"off",mask);
  ctl(0xFFFFFE1BUL,c0);
  for(s=0;s<3;s++) {int ok=run(menus[s],at[s][0],at[s][1],mask,&d); tests++; if(!ok) fails++; logline("   menu of %2d items at %d,%d: %s, %d pixels differ after it closed\n",items[s],at[s][0],at[s][1],ok?"ok":"FAIL",d);}
  ctl(0xFFFFFE1BUL,c1); logline("   driver: %lu saves, %lu restores, %lu refused\n",c1[0]-c0[0],c1[1]-c0[1],c1[2]-c0[2]);
  if(mode) {tests++; if(c1[1]==c0[1]||c1[0]==c0[0]) {fails++; logline("   FAIL: no save or restore reached the driver with the save-under on\n");}}
 }
 for(mode=0;mode<2;mode++) {double st; int i; ctl(0xF7000000UL|(mode?30UL:22UL),NULL); st=now(); for(i=0;i<6;i++) {int dd; run(menus[2],at[2][0],at[2][1],mode?30UL:22UL,&dd);} t[mode]=(now()-st)/6-300.0;}
 logline("time per 28-item menu open and close, the 300 ms of waiting taken off: off %.1f ms, on %.1f ms\n",t[0],t[1]);
 ctl(0xFFFFFFFFUL,NULL);
 for(s=0;s<3;s++) DestroyMenu(menus[s]);
 DestroyWindow(win);
 sprintf(res,"%s: %d of %d checks passed (see VX2SAVE.TXT)",fails?"FAIL":"PASS",tests-fails,tests);
 finish(res); logline("%s\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
