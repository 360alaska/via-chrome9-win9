/* VXTIME v1.1 (driver .585): the driver's own display timing above 1024x768 on a device where it is still under test (the VX800: the
 * driver sets the BIOS's mode and then writes its own CRTC timing and dot clock; its PLL holds the multiplier and the divider less two,
 * unlike the VX855's, and that has not run on a board). Each size above 1024x768 that Windows offers at the desktop's depth is set
 * twice, about four seconds each: with the BIOS's timing, then with the driver's. The screen says which one is showing. For each the
 * refresh rate is measured on the vertical retrace and the PLL's registers are read back. At the end one question: was the picture
 * steady in every step marked DRIVER. The switch is put back as it was and the desktop's mode restored; nothing is written to the
 * registry. If the monitor loses the picture in a step, wait: the next step follows by itself. Log: VXTIME.LOG next to this program. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"
static FILE *lf;static HANDLE dev=INVALID_HANDLE_VALUE;static char label[160];static int fails;
static void say(const char *f,...) {static char b[1024];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);if(lf){fputs(b,lf);fflush(lf);}}
static int probe(DWORD sub,DWORD arg,DWORD *o) {DWORD in[2],got=0;in[0]=sub;in[1]=arg;memset(o,0,64*4);
 return dev!=INVALID_HANDLE_VALUE&&DeviceIoControl(dev,VX_IOCTL_PROBE,in,8,o,64*4,&got,NULL)&&got==64*4&&o[0]==VX_MAGIC;}
static void pump(DWORD ms) {DWORD e=GetTickCount()+ms;MSG m;do{while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}Sleep(10);}while((long)(e-GetTickCount())>0);}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {
 if(m==WM_PAINT){PAINTSTRUCT ps;HDC dc=BeginPaint(h,&ps);RECT r;HFONT f,o;HBRUSH b=CreateSolidBrush(RGB(0,0,96));HPEN p=CreatePen(PS_SOLID,1,RGB(255,255,255)),op;
  GetClientRect(h,&r);FillRect(dc,&r,b);DeleteObject(b);
  /* a white line on the outermost pixels: an edge that is cut off or shifted shows */
  op=(HPEN)SelectObject(dc,p);SelectObject(dc,GetStockObject(NULL_BRUSH));Rectangle(dc,0,0,r.right,r.bottom);SelectObject(dc,op);DeleteObject(p);
  f=CreateFontA(40,0,0,0,FW_BOLD,0,0,0,ANSI_CHARSET,0,0,0,0,"Arial");o=(HFONT)SelectObject(dc,f);SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));
  DrawTextA(dc,label,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);SelectObject(dc,o);DeleteObject(f);EndPaint(h,&ps);return 0;}
 return DefWindowProcA(h,m,w,l);
}
/* the rate from the VxD's count of retrace edges and the time-stamp counter (sub 2: edges, ticks first to last edge, ticks in all, ms in all) */
static double measured(void) {DWORD o[64];if(!probe(2,0,o)||o[2]<3||!o[3]||!o[5])return 0;return (double)(o[2]-1)*((double)o[4]/(double)o[5])*1000.0/(double)o[3];}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 DWORD o[64],was,bpp,sizes[8][2];int n=0,i,k,t,steps=0,driver_steps=0;DEVMODEA dm,cur;WNDCLASSA c;HWND w;(void)hp;(void)cmd;(void)show;
 lf=fopen("VXTIME.LOG","w");
 say("VXTIME v1.1 (driver .585): the driver's own timing above 1024x768 against the BIOS's\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(!probe(5,2,o)){say("FAILED: the VxD does not answer the timing request (older than .583, or not this driver)\n");if(lf)fclose(lf);return 2;}
 was=o[2];say("the timing switch now: %lu (0 the BIOS's timing, 1 the driver's); the PLL now: SR44 %02lX SR45 %02lX SR46 %02lX SR40 %02lX\n",was,o[6]&255,(o[6]>>8)&255,(o[6]>>16)&255,o[6]>>24);
 memset(&cur,0,sizeof(cur));cur.dmSize=sizeof(cur);EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&cur);bpp=cur.dmBitsPerPel;
 say("desktop %lux%lux%lu\n",cur.dmPelsWidth,cur.dmPelsHeight,bpp);
 for(i=0;n<8;i++){memset(&dm,0,sizeof(dm));dm.dmSize=sizeof(dm);if(!EnumDisplaySettingsA(NULL,i,&dm))break;
  if(dm.dmBitsPerPel!=bpp||(dm.dmPelsWidth<=1024&&dm.dmPelsHeight<=768))continue;
  for(k=0;k<n;k++)if(sizes[k][0]==dm.dmPelsWidth&&sizes[k][1]==dm.dmPelsHeight)break;
  if(k==n){sizes[n][0]=dm.dmPelsWidth;sizes[n][1]=dm.dmPelsHeight;n++;}}
 if(!n){DWORD s[64];probe(1,0,s);say("no size above 1024x768 is offered at %lu bits (EDID read: %s; without one they are hidden unless HKLM\\Software\\VX85598 NOEDIDHIGHRES = 1): nothing to test\nNOT RUN\n",bpp,s[32]?"yes":"no");if(lf)fclose(lf);return 1;}
 memset(&c,0,sizeof(c));c.lpfnWndProc=wp;c.hInstance=hi;c.lpszClassName="VXTIME";c.hCursor=LoadCursorA(0,IDC_ARROW);RegisterClassA(&c);
 for(k=0;k<n;k++)for(t=0;t<2;t++){
  LONG r;double hz;DWORD q[64];
  /* v1.1: back to the desktop's mode first. v1.0 set the same size twice running, and Windows does not set a mode it is already in: the
   * DRIVER step never reached the driver (board: "the last set used the BIOS's" on all eight) */
  ChangeDisplaySettingsA(NULL,0);pump(700);
  probe(5,(DWORD)t,q);
  memset(&dm,0,sizeof(dm));dm.dmSize=sizeof(dm);dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;dm.dmPelsWidth=sizes[k][0];dm.dmPelsHeight=sizes[k][1];dm.dmBitsPerPel=bpp;
  r=ChangeDisplaySettingsA(&dm,CDS_FULLSCREEN);
  if(r!=DISP_CHANGE_SUCCESSFUL){say("%lux%lu, %s timing: the mode was refused (%ld)\n",sizes[k][0],sizes[k][1],t?"DRIVER":"BIOS",r);continue;}
  sprintf(label,"%lu x %lu   %s timing   (step %d of %d)",sizes[k][0],sizes[k][1],t?"DRIVER":"BIOS",k*2+t+1,n*2);
  w=CreateWindowExA(WS_EX_TOPMOST,"VXTIME","VXTIME",WS_POPUP|WS_VISIBLE,0,0,(int)sizes[k][0],(int)sizes[k][1],0,0,hi,0);
  pump(2500);hz=measured();probe(5,2,q);pump(1500);
  say("%4lux%-4lu %-6s timing: the last set used %s; the driver's rate %lu Hz; measured %.2f Hz; PLL SR44 %02lX SR45 %02lX SR46 %02lX SR40 %02lX\n",sizes[k][0],sizes[k][1],t?"DRIVER":"BIOS",
   q[4]==1?"the driver's timing":q[4]==2?"the BIOS's (the driver's was REFUSED: no clock or timing for this size)":"the BIOS's",q[5],hz,q[6]&255,(q[6]>>8)&255,(q[6]>>16)&255,q[6]>>24);
  steps++;
  if(t){driver_steps++;
   if(q[4]!=1){say("   WRONG: the driver's timing was not used\n");fails++;}
   else if(hz<(double)q[5]-1.5||hz>(double)q[5]+1.5){say("   WRONG: the measured rate is not the one the driver set\n");fails++;}}
  if(w)DestroyWindow(w);
 }
 probe(5,was,o);ChangeDisplaySettingsA(NULL,0);pump(1500);
 say("the switch put back to %lu; the desktop's mode restored\n",o[3]);
 if(driver_steps){int a=MessageBoxA(NULL,"Was the picture steady and complete (white line visible on all four edges) in every step marked DRIVER?","VXTIME",MB_YESNO|MB_ICONQUESTION|MB_TOPMOST|MB_SETFOREGROUND);
  say("the picture in the DRIVER steps, as seen: %s\n",a==IDYES?"steady and complete":"NOT right");if(a!=IDYES)fails++;}
 say("%s: %d step(s) run, %d wrong\n",!driver_steps?"NOT RUN":fails?"FAILED":"PASSED",steps,fails);
 if(lf)fclose(lf);if(dev!=INVALID_HANDLE_VALUE)CloseHandle(dev);
 return fails?2:0;
}
