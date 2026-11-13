/* VX2BARS v1.0: switches through 640x480, 800x600 and 1024x768 at 16 and 32 bits with the normal Windows mode change and draws the
 * same eight colour bars as VX2MODE using plain GDI (no DirectDraw). Each mode is held for 8 seconds so it can be screenshot.
 * Esc skips to the next mode, Q quits. Writes nothing. Compare with the VX2MODE pictures: if these are shifted too, DirectDraw is not the cause. */
#include <windows.h>
#include <stdio.h>
static const DWORD modes[6][3]={{640,480,16},{800,600,16},{1024,768,16},{640,480,32},{800,600,32},{1024,768,32}};
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_ERASEBKGND) return 1;
 return DefWindowProc(w,m,a,b);
}
static void paint(HWND win,int w,int h,int b) {
 static const COLORREF c[8]={RGB(255,255,255),RGB(255,255,0),RGB(0,255,255),RGB(0,255,0),RGB(255,0,255),RGB(255,0,0),RGB(0,0,255),RGB(0,0,0)};
 HDC dc=GetDC(win); RECT r; int i; HBRUSH br; char t[80];
 SetRect(&r,0,0,w,h); br=CreateSolidBrush(RGB(40,40,40)); FillRect(dc,&r,br); DeleteObject(br);
 for(i=0;i<8;i++) {SetRect(&r,w*i/8,h/4,w*(i+1)/8,h*3/4); br=CreateSolidBrush(c[i]); FillRect(dc,&r,br); DeleteObject(br);}
 br=CreateSolidBrush(c[0]);
 SetRect(&r,0,0,w,1); FillRect(dc,&r,br); SetRect(&r,0,h-1,w,h); FillRect(dc,&r,br);
 SetRect(&r,0,0,1,h); FillRect(dc,&r,br); SetRect(&r,w-1,0,w,h); FillRect(dc,&r,br); DeleteObject(br);
 sprintf(t," GDI %dx%dx%d (no DirectDraw) ",w,h,b); SetBkMode(dc,OPAQUE); SetBkColor(dc,RGB(0,0,0)); SetTextColor(dc,RGB(255,255,255)); TextOut(dc,12,12,t,(int)strlen(t));
 ReleaseDC(win,dc);
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show_) {
 WNDCLASS wc; HWND win; int i; DEVMODE dm; MSG m; int quit=0;
 (void)prev;(void)cmd;(void)show_;
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2BARS"; RegisterClass(&wc);
 for(i=0;i<6&&!quit;i++) {
  DWORD end; int w=(int)modes[i][0],h=(int)modes[i][1];
  memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=modes[i][0]; dm.dmPelsHeight=modes[i][1]; dm.dmBitsPerPel=modes[i][2]; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
  ChangeDisplaySettings(&dm,CDS_FULLSCREEN);
  Sleep(1500);
  win=CreateWindowEx(WS_EX_TOPMOST,wc.lpszClassName,"VX2BARS",WS_POPUP|WS_VISIBLE,0,0,w,h,NULL,NULL,hi,NULL);
  Sleep(300); paint(win,w,h,(int)modes[i][2]);
  end=GetTickCount()+8000UL;
  while(GetTickCount()<end&&!quit) {
   while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m);DispatchMessage(&m);}
   if(GetAsyncKeyState(VK_ESCAPE)&0x8000) break;
   if(GetAsyncKeyState('Q')&0x8000) quit=1;
   Sleep(50);
  }
  DestroyWindow(win);
 }
 ChangeDisplaySettings(NULL,0);
 MessageBox(NULL,"Done. Compare these pictures with the VX2MODE ones: were 800x600 and 1024x768 shifted here too?","VX2BARS",MB_OK);
 return 0;
}
