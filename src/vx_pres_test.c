/* VXPRES v1.1 (driver v10.425; v1.1: the black square checked where it is drawn, rows 360-456 (v1.0 expected it 24 rows higher and
 * failed every frame for that alone); a second GDI marker drawn with SetPixelV, which goes to DIBENG's own pixel routine through the
 * driver's BeginAccess hook only). VXPRES v1.0 (driver v10.424): asynchronous OpenGL presents (ACCEL_ASYNC_PRESENT). A 640x480 OpenGL window (WinTune's pixel format:
 * double-buffered, 24-bit colour, 32-bit depth asked) drawn by the ICD, with the VxD's async presents switched off, then on (live,
 * 2D control 0xFFFFFE22 / 0xFFFFFE23; put back as it was at the end). Each pass:
 *  1 speed: 300 frames (clear + 2 triangles + SwapBuffers): frames a second, SwapBuffers' time, and the VxD's counters (presents that ran
 *    asynchronously, all asynchronous 2D copies), so the run shows whether presents were asynchronous at all;
 *  2 shown: 60 frames, each a different solid colour; after SwapBuffers the window is read back through GDI at once (counted, not judged:
 *    a read during the copy may see part of the old frame) and again after 30 ms (judged: every sampled pixel the frame's colour);
 *  3 order: frame k swapped, frame k+1 drawn into the back buffer at once and not swapped (glFinish), then the window read after 30 ms:
 *    it must still be frame k (fails when the next frame's clear overtook the copy still reading the back buffer);
 *  4 GDI: a 40x40 GDI fill (a marker) and an 8x8 block of SetPixelV pixels (a second marker) into the window right after SwapBuffers, the window read after 30 ms: the marker must be there and
 *    the rest the frame's colour (fails when the copy landed after the GDI fill and painted over it).
 * The window must stay uncovered and the pointer away from it while it runs. Writes VXPRES.TXT next to the program; exit code 0 = PASS. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <GL/gl.h>
#include "vx85598.h"

#define W 640
#define H 480
#define FRAMES 300
#define CHECKS 60
static FILE *lf;static HANDLE dev=INVALID_HANDLE_VALUE;static HWND win;static HDC wdc;static HGLRC rc;
static HDC mdc;static HBITMAP dib;static DWORD *bits;static LARGE_INTEGER qf;
static void out(const char *f,...) {char t[600];va_list a;va_start(a,f);_vsnprintf(t,sizeof(t)-1,f,a);va_end(a);t[sizeof(t)-1]=0;if(lf){fputs(t,lf);fflush(lf);}}
static double now_ms(void) {LARGE_INTEGER c;QueryPerformanceCounter(&c);return (double)c.QuadPart*1000.0/(double)qf.QuadPart;}
static int ctl(DWORD v,DWORD *o) {DWORD got=0;memset(o,0,16);return dev!=INVALID_HANDLE_VALUE&&DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&v,4,o,16,&got,NULL)&&got==16;}
static void pump(void) {MSG m;while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {if(m==WM_ERASEBKGND)return 1;return DefWindowProcA(h,m,w,l);}
/* colours every depth shows exactly: components 0 or 255; no white (the GDI marker) and no black (a cleared screen) */
static const DWORD pal[6]={0xFF0000,0x00FF00,0x0000FF,0xFFFF00,0x00FFFF,0xFF00FF};
#define MARK 0xFFFFFF
#define QUADC 0x000000 /* the triangles: black, bottom right */
static void draw(int k) {
 DWORD c=pal[k%6];
 glViewport(0,0,W,H);glClearColor(((c>>16)&255)/255.0f,((c>>8)&255)/255.0f,(c&255)/255.0f,1.0f);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glLoadIdentity();glEnable(GL_DEPTH_TEST);
 glColor3f(0,0,0);glBegin(GL_TRIANGLES); /* a black square at x 0.5..0.9, y -0.9..-0.5 (bottom right): screen x 480..608, y 360..456 */
 glVertex3f(0.5f,-0.9f,0);glVertex3f(0.9f,-0.9f,0);glVertex3f(0.9f,-0.5f,0);glVertex3f(0.5f,-0.9f,0);glVertex3f(0.9f,-0.5f,0);glVertex3f(0.5f,-0.5f,0);glEnd();
}
static void grab(void) {HDC d=GetDC(win);GdiFlush();BitBlt(mdc,0,0,W,H,d,0,0,SRCCOPY);GdiFlush();ReleaseDC(win,d);}
static int near_c(DWORD a,DWORD b) {int k;for(k=0;k<24;k+=8){int x=(int)((a>>k)&255),y=(int)((b>>k)&255);if(x-y>12||y-x>12)return 0;}return 1;}
/* sampled pixels (every 8th in both directions) not the expected colour; the black square and, with mark, the marker have their own */
static int mismatches(DWORD c,int mark,DWORD *first) {
 int x,y,n=0;*first=0xFFFFFFFFUL;
 for(y=4;y<H;y+=8)for(x=4;x<W;x+=8){DWORD p=bits[y*W+x]&0xFFFFFF,e=c;
  if(x>=484&&x<604&&y>=364&&y<452)e=QUADC;else if((x>=476&&x<612&&y>=356&&y<460))continue; /* the square's edges: either */
  if(mark&&x>=20&&x<60&&y>=20&&y<60)e=MARK;else if(mark&&x>=16&&x<64&&y>=16&&y<64)continue;
  if(mark&&x>=80&&x<88&&y>=16&&y<24)e=MARK;else if(mark&&x>=76&&x<92&&y>=12&&y<28)continue; /* v1.1: the SetPixelV block */
  if(!near_c(p,e)){if(!n)*first=(DWORD)x<<16|y;n++;}}
 return n;
}
static void mark_now(void) {HDC d=GetDC(win);HBRUSH b=CreateSolidBrush(RGB(255,255,255));RECT r={20,20,60,60};int x,y;FillRect(d,&r,b);
 for(y=16;y<24;y++)for(x=80;x<88;x++)SetPixelV(d,x,y,RGB(255,255,255)); /* v1.1: DIBENG's pixel routine (BeginAccess only) */
 GdiFlush();DeleteObject(b);ReleaseDC(win,d);}
static int pass_run(int on,int *fails) {
 DWORD o[4],a0[4],b0[4],a1[4],b1[4],first;int k,bad,imm=0,shown=0,order=0,gdi=0;double t0,t1,sw=0,swmax=0;
 ctl(on?0xFFFFFE23UL:0xFFFFFE22UL,o);
 out("\r\n== async presents %s (switch %lu, ACCEL_ASYNC %lu) ==\r\n",on?"on":"off",o[2],o[3]);
 /* 1 speed */
 for(k=0;k<10;k++){draw(k);SwapBuffers(wdc);}glFinish();Sleep(50);
 ctl(0xFFFFFE24UL,a0);ctl(0xFFFFFE12UL,b0);
 t0=now_ms();
 for(k=0;k<FRAMES;k++){double s;draw(k);s=now_ms();SwapBuffers(wdc);s=now_ms()-s;sw+=s;if(s>swmax)swmax=s;if(!(k&31))pump();}
 glFinish();t1=now_ms();
 ctl(0xFFFFFE24UL,a1);ctl(0xFFFFFE12UL,b1);
 out("speed: %d frames in %.1f ms: %.1f a second; SwapBuffers %.3f ms on average (most %.3f)\r\n",FRAMES,t1-t0,FRAMES*1000.0/(t1-t0),sw/FRAMES,swmax);
 out("VxD: presents that ran asynchronously %lu, asynchronous 2D copies %lu, async waits %lu, exclusive-mode gate %s\r\n",
  a1[0]-a0[0],b1[0]-b0[0],b1[1]-b0[1],(b1[3]&2)?"OPEN":"closed");
 if(on&&a1[0]==a0[0])out("note: no present ran asynchronously (ACCEL_ASYNC or ACCEL_ASYNC_PRESENT off, or an older VXGL/VXDDRAW)\r\n");
 if(!on&&a1[0]!=a0[0]){out("FAIL: presents ran asynchronously with the switch off\r\n");(*fails)++;}
 /* 2 shown */
 for(k=0;k<CHECKS;k++){
  draw(k);SwapBuffers(wdc);grab();if(mismatches(pal[k%6],0,&first))imm++;
  Sleep(30);glFinish();grab();bad=mismatches(pal[k%6],0,&first);
  if(bad){shown++;if(shown<=4)out("  shown, frame %d: %d sampled pixels wrong, the first at %lu,%lu: %06lX\r\n",k,bad,first>>16,first&0xFFFF,bits[(first&0xFFFF)*W+(first>>16)]&0xFFFFFF);}
  pump();}
 out("shown: %d of %d frames wrong after 30 ms (must be 0); read at once: %d of %d not yet the whole new frame (information)\r\n",shown,CHECKS,imm,CHECKS);
 /* 3 order */
 for(k=0;k<CHECKS;k++){
  draw(k);SwapBuffers(wdc);draw(k+1);glFinish();Sleep(30);grab();bad=mismatches(pal[k%6],0,&first);
  if(bad){order++;if(order<=4)out("  order, frame %d: %d sampled pixels wrong, the first at %lu,%lu: %06lX\r\n",k,bad,first>>16,first&0xFFFF,bits[(first&0xFFFF)*W+(first>>16)]&0xFFFFFF);}
  SwapBuffers(wdc);glFinish();pump();}
 out("order: %d of %d frames showed the next frame's drawing (must be 0)\r\n",order,CHECKS);
 /* 4 GDI over the window */
 for(k=0;k<CHECKS;k++){
  draw(k);SwapBuffers(wdc);mark_now();Sleep(30);grab();bad=mismatches(pal[k%6],1,&first);
  if(bad){gdi++;if(gdi<=4)out("  GDI, frame %d: %d sampled pixels wrong, the first at %lu,%lu: %06lX\r\n",k,bad,first>>16,first&0xFFFF,bits[(first&0xFFFF)*W+(first>>16)]&0xFFFFFF);}
  pump();}
 out("GDI: %d of %d frames lost the marker or showed it painted over (must be 0)\r\n",gdi,CHECKS);
 *fails+=shown+order+gdi;
 return 1;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cl,int cs) {
 WNDCLASSA wc;PIXELFORMATDESCRIPTOR pfd;RECT r={0,0,W,H};BITMAPINFO bi;DWORD o[4],orig=1;int pf,fails=0;char path[MAX_PATH],*s;
 (void)hp;(void)cl;(void)cs;
 GetModuleFileNameA(0,path,sizeof(path));s=strrchr(path,'\\');strcpy(s?s+1:path,"VXPRES.TXT");lf=fopen(path,"a");
 QueryPerformanceFrequency(&qf);
 out("\r\nVXPRES 1.1: asynchronous OpenGL presents (ACCEL_ASYNC_PRESENT)\r\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE){out("the VxD is not there\r\n");MessageBoxA(0,"VX85598.VXD is not loaded.","VXPRES",MB_ICONSTOP);return 2;}
 if(!ctl(0xFFFFFE24UL,o)){out("the VxD did not answer 0xFFFFFE24 (a VxD older than v10.424)\r\n");MessageBoxA(0,"The VxD is older than v10.424.","VXPRES",MB_ICONSTOP);return 2;}
 orig=o[2];out("at the start: async presents %s, ACCEL_ASYNC %lu, presents run asynchronously so far %lu\r\n",orig?"on":"off",o[3],o[0]);
 ctl(0xFFFFFE12UL,o);out("exclusive-mode async gate at the start: %s\r\n",(o[3]&2)?"OPEN (left by an earlier exclusive-mode program)":"closed");
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=hi;wc.hCursor=LoadCursor(0,IDC_ARROW);wc.lpszClassName="VXPRES";RegisterClassA(&wc);
 AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
 win=CreateWindowA("VXPRES","VXPRES: keep this window uncovered",WS_OVERLAPPEDWINDOW|WS_CLIPCHILDREN|WS_CLIPSIBLINGS|WS_VISIBLE,40,40,r.right-r.left,r.bottom-r.top,0,0,hi,0);
 if(!win){out("no window\r\n");return 2;}
 SetForegroundWindow(win);pump();Sleep(200);pump();
 wdc=GetDC(win);
 memset(&pfd,0,sizeof(pfd));pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=24;pfd.cDepthBits=32;
 pf=ChoosePixelFormat(wdc,&pfd);if(!pf||!SetPixelFormat(wdc,pf,&pfd)){out("no pixel format\r\n");return 2;}
 DescribePixelFormat(wdc,pf,sizeof(pfd),&pfd);
 rc=wglCreateContext(wdc);if(!rc||!wglMakeCurrent(wdc,rc)){out("no OpenGL context\r\n");return 2;}
 out("pixel format %d: colour %d, depth %d, %s; renderer \"%s\", vendor \"%s\"\r\n",pf,pfd.cColorBits,pfd.cDepthBits,(pfd.dwFlags&PFD_GENERIC_FORMAT)?"GENERIC (Microsoft's, not the ICD)":"the ICD",
  (const char*)glGetString(GL_RENDERER),(const char*)glGetString(GL_VENDOR));
 if(pfd.dwFlags&PFD_GENERIC_FORMAT){out("FAIL: the ICD is not drawing this window\r\n");fails++;}
 memset(&bi,0,sizeof(bi));bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=W;bi.bmiHeader.biHeight=-H;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 mdc=CreateCompatibleDC(wdc);dib=CreateDIBSection(wdc,&bi,DIB_RGB_COLORS,(void**)&bits,0,0);if(!mdc||!dib||!bits){out("no DIB\r\n");return 2;}
 SelectObject(mdc,dib);
 pass_run(0,&fails);
 pass_run(1,&fails);
 ctl(orig?0xFFFFFE23UL:0xFFFFFE22UL,o);out("async presents put back %s\r\n",o[2]?"on":"off");
 out("%s: %d failure(s)\r\n",fails?"FAIL":"PASS",fails);
 wglMakeCurrent(0,0);wglDeleteContext(rc);ReleaseDC(win,wdc);DestroyWindow(win);DeleteDC(mdc);DeleteObject(dib);CloseHandle(dev);
 if(lf)fclose(lf);
 {char m[200];wsprintfA(m,"%s: %d failure(s).\nDetails in VXPRES.TXT.",fails?"FAIL":"PASS",fails);MessageBoxA(0,m,"VXPRES 1.1",fails?MB_ICONSTOP:MB_ICONINFORMATION);}
 return fails?1:0;
}
