/* VXALT v1.0 (driver .582): what happens to the taskbar when a full-screen program is minimized and closed, as Alt-Tab out of a
 * full-screen OpenGL game does (VX800 board, Quake II demo: the taskbar was gone after Alt-Tab and stayed gone after the game quit; a
 * resolution change brought it back). Three programs are played in turn: a plain full-screen window (no OpenGL: the control), a
 * full-screen OpenGL window through the ICD, and the same with a mode change in and out (ChangeDisplaySettings, as the game does).
 * After each step the taskbar is examined: where it is, whether it is visible and on top at its own place, the work area, and whether
 * the Start button's pixels on the screen are the ones from before. Log: VXALT.LOG next to this program. Nothing is left changed. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <GL/gl.h>
static FILE *lf;static int fails;
static void say(const char *f,...) {static char b[1024];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);if(lf){fputs(b,lf);fflush(lf);}}
static void pump(DWORD ms) {DWORD e=GetTickCount()+ms;MSG m;do{while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}Sleep(10);}while((long)(e-GetTickCount())>0);}
typedef struct {int found,visible,topmost,ontop,sw,sh;RECT r,work;DWORD hash;int hw,hh;} tray_t;
/* the Start button's corner of the taskbar as the screen shows it (the clock's end changes by itself) */
static DWORD tray_hash(const RECT *r,int *pw,int *ph) {
 int w=r->right-r->left,h=r->bottom-r->top,x,y;DWORD hsh=2166136261UL;HDC s,m;HBITMAP b,o;BITMAPINFOHEADER bi;DWORD *px=0;
 if(w>48)w=48;if(h>22)h=22;*pw=w;*ph=h;if(w<=0||h<=0)return 0;
 memset(&bi,0,sizeof(bi));bi.biSize=sizeof(bi);bi.biWidth=w;bi.biHeight=-h;bi.biPlanes=1;bi.biBitCount=32;
 s=GetDC(0);m=CreateCompatibleDC(s);b=CreateDIBSection(s,(BITMAPINFO*)&bi,DIB_RGB_COLORS,(void**)&px,0,0);
 if(m&&b&&px){o=(HBITMAP)SelectObject(m,b);GdiFlush();BitBlt(m,0,0,w,h,s,r->left+2,r->top+2,SRCCOPY);GdiFlush();
  for(y=0;y<h;y++)for(x=0;x<w;x++){hsh^=px[y*w+x]&0xFFFFFFUL;hsh*=16777619UL;}SelectObject(m,o);}
 if(b)DeleteObject(b);if(m)DeleteDC(m);ReleaseDC(0,s);return hsh;
}
static void tray_get(tray_t *t) {
 HWND w=FindWindowA("Shell_TrayWnd",0);memset(t,0,sizeof(*t));
 t->sw=GetSystemMetrics(SM_CXSCREEN);t->sh=GetSystemMetrics(SM_CYSCREEN);SystemParametersInfoA(SPI_GETWORKAREA,0,&t->work,0);
 if(!w)return;
 t->found=1;GetWindowRect(w,&t->r);t->visible=IsWindowVisible(w)!=0;t->topmost=(GetWindowLongA(w,GWL_EXSTYLE)&WS_EX_TOPMOST)!=0;
 {POINT p;HWND at;p.x=(t->r.left+t->r.right)/2;p.y=(t->r.top+t->r.bottom)/2;at=WindowFromPoint(p);while(at&&at!=w&&GetParent(at))at=GetParent(at);t->ontop=at==w;}
 t->hash=tray_hash(&t->r,&t->hw,&t->hh);
}
static void tray_say(const char *what,const tray_t *t) {
 if(!t->found){say("   %-44s the taskbar window is not there\n",what);return;}
 say("   %-44s screen %dx%d, work area %ld,%ld-%ld,%ld; taskbar at %ld,%ld-%ld,%ld, %s, %s, %s at its own place; Start corner %08lX\n",what,t->sw,t->sh,
  t->work.left,t->work.top,t->work.right,t->work.bottom,t->r.left,t->r.top,t->r.right,t->r.bottom,t->visible?"visible":"HIDDEN",t->topmost?"topmost":"not topmost",t->ontop?"on top":"NOT on top",t->hash);
}
/* the taskbar must be as it was before the program started */
static void tray_check(const char *what,const tray_t *b) {
 tray_t t;int bad=0;tray_get(&t);tray_say(what,&t);
 if(!t.found||!b->found)return;
 if(!t.visible){say("      WRONG: the taskbar is hidden\n");bad=1;}
 if(memcmp(&t.r,&b->r,sizeof(RECT))){say("      WRONG: the taskbar is not where it was (%ld,%ld-%ld,%ld before)\n",b->r.left,b->r.top,b->r.right,b->r.bottom);bad=1;}
 if(t.sw!=b->sw||t.sh!=b->sh){say("      WRONG: the screen size is not the one from before (%dx%d)\n",b->sw,b->sh);bad=1;}
 if(memcmp(&t.work,&b->work,sizeof(RECT))){say("      WRONG: the work area is not the one from before (%ld,%ld-%ld,%ld)\n",b->work.left,b->work.top,b->work.right,b->work.bottom);bad=1;}
 if(!t.ontop){say("      WRONG: another window is on top at the taskbar's place\n");bad=1;}
 if(!bad&&t.hash!=b->hash){say("      WRONG: the taskbar is in place and on top, but the screen does not show it (Start corner %08lX before)\n",b->hash);bad=1;}
 if(bad)fails++;
}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {
 if(m==WM_ERASEBKGND)return 1;
 return DefWindowProcA(h,m,w,l);
}
/* kind 0: plain window; 1: OpenGL; 2: OpenGL with a mode change in and out */
static void play(int kind,const tray_t *before) {
 static const char *const nm[3]={"a plain full-screen window (no OpenGL)","a full-screen OpenGL window","a full-screen OpenGL window with a mode change"};
 HWND w;HDC dc=0;HGLRC rc=0;int i,sw,sh,changed=0;DEVMODEA dm;
 say("== %s\n",nm[kind]);
 if(kind==2){memset(&dm,0,sizeof(dm));dm.dmSize=sizeof(dm);dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT;
  if(before->sw==640){dm.dmPelsWidth=800;dm.dmPelsHeight=600;}else{dm.dmPelsWidth=640;dm.dmPelsHeight=480;}
  {LONG r=ChangeDisplaySettingsA(&dm,CDS_FULLSCREEN);say("   ChangeDisplaySettings(%lux%lu, full screen): %ld\n",dm.dmPelsWidth,dm.dmPelsHeight,r);if(r!=DISP_CHANGE_SUCCESSFUL){say("   (the mode change was refused: this part is left out)\n");return;}changed=1;}}
 sw=GetSystemMetrics(SM_CXSCREEN);sh=GetSystemMetrics(SM_CYSCREEN);
 w=CreateWindowExA(WS_EX_TOPMOST,"VXALT","VXALT",WS_POPUP|WS_VISIBLE|WS_CLIPCHILDREN|WS_CLIPSIBLINGS,0,0,sw,sh,0,0,GetModuleHandleA(0),0);
 if(!w){say("   WRONG: no window\n");fails++;if(changed)ChangeDisplaySettingsA(0,0);return;}
 SetForegroundWindow(w);SetFocus(w);pump(300);dc=GetDC(w);
 if(kind){PIXELFORMATDESCRIPTOR p;int f;memset(&p,0,sizeof(p));p.nSize=sizeof(p);p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=(BYTE)GetDeviceCaps(dc,BITSPIXEL);p.cDepthBits=16;
  f=ChoosePixelFormat(dc,&p);if(!f||!SetPixelFormat(dc,f,&p)||!(rc=wglCreateContext(dc))||!wglMakeCurrent(dc,rc)){say("   WRONG: no OpenGL context (pixel format %d, Windows error %lu)\n",f,GetLastError());fails++;}
  else say("   GL_RENDERER %s\n",(const char*)glGetString(GL_RENDERER));}
 for(i=0;i<40;i++){
  if(rc){glViewport(0,0,sw,sh);glClearColor((i&1)?0.2f:0.1f,0.3f,0.6f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);SwapBuffers(dc);}
  else{RECT r;HBRUSH b=CreateSolidBrush(RGB(30,80,(i&1)?160:150));r.left=r.top=0;r.right=sw;r.bottom=sh;FillRect(dc,&r,b);DeleteObject(b);}
  pump(20);}
 {tray_t t;tray_get(&t);tray_say("while it is in front (not judged):",&t);}
 /* out, as the game's window procedure does when it loses the foreground: minimized, the desktop's mode back */
 if(rc)wglMakeCurrent(0,0);
 ShowWindow(w,SW_MINIMIZE);if(changed){LONG r=ChangeDisplaySettingsA(0,0);say("   ChangeDisplaySettings(desktop): %ld\n",r);}
 pump(2000);tray_check("minimized (as after Alt-Tab):",before);
 /* back in, a few frames, then closed */
 if(changed){LONG r=ChangeDisplaySettingsA(&dm,CDS_FULLSCREEN);say("   ChangeDisplaySettings(%lux%lu, full screen) again: %ld\n",dm.dmPelsWidth,dm.dmPelsHeight,r);}
 ShowWindow(w,SW_RESTORE);SetForegroundWindow(w);pump(300);
 if(rc)wglMakeCurrent(dc,rc);
 for(i=0;i<10;i++){if(rc){glClear(GL_COLOR_BUFFER_BIT);SwapBuffers(dc);}pump(20);}
 /* and out again before it quits: the order that left the taskbar gone on the board (Alt-Tab, then quit) */
 if(rc)wglMakeCurrent(0,0);
 ShowWindow(w,SW_MINIMIZE);if(changed)ChangeDisplaySettingsA(0,0);pump(1000);
 if(rc){wglDeleteContext(rc);}
 ReleaseDC(w,dc);DestroyWindow(w);pump(2000);tray_check("closed while minimized (as quitting then):",before);
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 WNDCLASSA c;tray_t before;int k;(void)hp;(void)cmd;(void)show;
 lf=fopen("VXALT.LOG","w");
 say("VXALT v1.0 (driver .582): the taskbar after a full-screen program is minimized and closed\n");
 memset(&c,0,sizeof(c));c.lpfnWndProc=wp;c.hInstance=hi;c.lpszClassName="VXALT";c.hCursor=LoadCursorA(0,IDC_ARROW);c.style=CS_OWNDC;RegisterClassA(&c);
 pump(500);tray_get(&before);tray_say("before:",&before);
 if(!before.found){say("FAILED: no taskbar to examine (is Explorer the shell?)\n");if(lf)fclose(lf);return 2;}
 if(!before.visible||!before.ontop)say("   (the taskbar is not visible and on top at the start: close or move what covers it and run again; the results below compare against this state)\n");
 for(k=0;k<3;k++)play(k,&before);
 say("%s: %d step(s) left the taskbar wrong\n",fails?"FAILED":"PASSED",fails);
 if(lf)fclose(lf);
 return fails?2:0;
}
