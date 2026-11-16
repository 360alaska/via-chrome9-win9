/* VXHOLLOW v1.1 (driver .637: the static controls' background is compared with the stripes as they were read before the controls
 * were made; v1.0 expected GetNearestColor's one colour, and at 16 bits a pixel the orange stripe is a dithered brush of two: board,
 * .637, 16 bpp: 70 and 52 pixels reported, exactly half of the orange stripe's pixels in the area read, with every drawing case ok).
 * VXHOLLOW v1.0: drawing with the hollow (NULL) brush must change nothing.
 * InstallShield's wizard puts its title and subtitle over the banner bitmap as static controls whose WM_CTLCOLORSTATIC answer is the
 * hollow brush with a transparent text background; the banner must show through. On this driver the two controls came out as filled
 * boxes: black at 32 bits per pixel, grey at 16.
 * What it does, at the desktop's own depth, no mode change:
 *  1. Each drawing call with the hollow brush is made on the screen (a window) and on a memory bitmap of the screen's format (drawn by
 *     the DIB engine in software) over the same striped background; every pixel of the two is compared.
 *  2. A real static control over the stripes, its parent answering WM_CTLCOLORSTATIC as InstallShield does; the stripes must be there
 *     beside the text.
 * Writes VXHOLLOW.TXT next to the program. Run it at 16 and at 32 bits per pixel. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#define W 400
#define H 300
static FILE *lf; static int bad,cases;
static void say(const char *f,...) {va_list a; va_start(a,f); vfprintf(lf,f,a); va_end(a); fflush(lf);}
static const COLORREF stripe[6]={RGB(255,102,0),RGB(0,128,255),RGB(255,255,255),RGB(32,32,32),RGB(0,200,80),RGB(200,0,160)};
static void stripes(HDC dc) {
 int i; RECT r;
 for(i=0;i<W/8;i++) {HBRUSH b=CreateSolidBrush(stripe[i%6]); r.left=i*8; r.right=i*8+8; r.top=0; r.bottom=H; FillRect(dc,&r,b); DeleteObject(b);}
}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_ERASEBKGND) {stripes((HDC)a); return 1;}
 if(m==WM_CTLCOLORSTATIC) {SetBkMode((HDC)a,TRANSPARENT); SetTextColor((HDC)a,RGB(255,255,255)); return (LRESULT)GetStockObject(NULL_BRUSH);}
 return DefWindowProc(w,m,a,b);
}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} GdiFlush();}
typedef void (*opfn)(HDC,int,int,int,int);
static void op_fillrect(HDC d,int x,int y,int w,int h) {RECT r; r.left=x; r.top=y; r.right=x+w; r.bottom=y+h; FillRect(d,&r,(HBRUSH)GetStockObject(NULL_BRUSH));}
static void op_patcopy(HDC d,int x,int y,int w,int h) {PatBlt(d,x,y,w,h,PATCOPY);}
static void op_patinvert(HDC d,int x,int y,int w,int h) {PatBlt(d,x,y,w,h,PATINVERT);}
static void op_dpa(HDC d,int x,int y,int w,int h) {PatBlt(d,x,y,w,h,0x00A000C9UL);}
static void op_dpo(HDC d,int x,int y,int w,int h) {PatBlt(d,x,y,w,h,0x00FA0089UL);}
static void op_rect(HDC d,int x,int y,int w,int h) {HGDIOBJ p=SelectObject(d,GetStockObject(NULL_PEN)); Rectangle(d,x,y,x+w+1,y+h+1); SelectObject(d,p);}
static void op_ellipse(HDC d,int x,int y,int w,int h) {HGDIOBJ p=SelectObject(d,GetStockObject(NULL_PEN)); Ellipse(d,x,y,x+w+1,y+h+1); SelectObject(d,p);}
static void op_mergecopy(HDC d,int x,int y,int w,int h) {BitBlt(d,x,y,w,h,d,x+3,y+2,MERGECOPY);}
static void op_patpaint(HDC d,int x,int y,int w,int h) {BitBlt(d,x,y,w,h,d,x+3,y+2,PATPAINT);}
static void op_frame(HDC d,int x,int y,int w,int h) {RECT r; r.left=x; r.top=y; r.right=x+w; r.bottom=y+h; FrameRect(d,&r,(HBRUSH)GetStockObject(NULL_BRUSH));}
static const struct {const char *name; opfn f; int same;} ops[]={ /* same: the picture must be left as it was (not only equal to the software's) */
 {"FillRect, hollow brush",op_fillrect,1},{"PatBlt PATCOPY",op_patcopy,1},{"PatBlt PATINVERT",op_patinvert,0},{"PatBlt ROP A000C9 (DPa)",op_dpa,0},
 {"PatBlt ROP FA0089 (DPo)",op_dpo,0},{"Rectangle, no pen",op_rect,1},{"Ellipse, no pen",op_ellipse,1},{"BitBlt MERGECOPY from the screen",op_mergecopy,0},
 {"BitBlt PATPAINT from the screen",op_patpaint,0},{"FrameRect, hollow brush",op_frame,1}};
static const int box[4][4]={{24,20,330,14},{60,50,318,13},{10,80,16,16},{40,110,300,150}}; /* the first two: the wizard's title and subtitle controls */
static void run_ops(HWND win,HDC sdc) {
 HDC mdc=CreateCompatibleDC(sdc),kdc=CreateCompatibleDC(sdc); HBITMAP mb=CreateCompatibleBitmap(sdc,W,H),kb=CreateCompatibleBitmap(sdc,W,H); int o,b,x,y;
 SelectObject(mdc,mb); SelectObject(kdc,kb); stripes(kdc);
 for(o=0;o<(int)(sizeof(ops)/sizeof(ops[0]));o++) for(b=0;b<4;b++) {
  int diff=0,changed=0,fx=-1,fy=-1; COLORREF fs=0,fm=0; HGDIOBJ os,om;
  say("%s, %dx%d at %d,%d: ",ops[o].name,box[b][2],box[b][3],box[b][0],box[b][1]); fflush(lf);
  stripes(sdc); stripes(mdc); GdiFlush();
  os=SelectObject(sdc,GetStockObject(NULL_BRUSH)); om=SelectObject(mdc,GetStockObject(NULL_BRUSH));
  ops[o].f(sdc,box[b][0],box[b][1],box[b][2],box[b][3]); ops[o].f(mdc,box[b][0],box[b][1],box[b][2],box[b][3]); GdiFlush();
  SelectObject(sdc,os); SelectObject(mdc,om);
  for(y=box[b][1]-2;y<box[b][1]+box[b][3]+2;y++) for(x=box[b][0]-2;x<box[b][0]+box[b][2]+2;x++) {
   COLORREF s=GetPixel(sdc,x,y),m=GetPixel(mdc,x,y);
   if(s!=m) {if(!diff) {fx=x; fy=y; fs=s; fm=m;} diff++;}
   if(s!=GetPixel(kdc,x,y)) changed++;
  }
  cases++;
  if(diff) {bad++; say("FAIL: %d pixels differ from the software's; first at %d,%d: screen %06lX, software %06lX; %d pixels of the screen changed\n",diff,fx,fy,(unsigned long)fs,(unsigned long)fm,changed);}
  else if(ops[o].same&&changed) {bad++; say("FAIL: equal to the software's, but %d pixels changed\n",changed);}
  else say("ok (%d pixels changed, as the software's)\n",changed);
 }
 DeleteDC(mdc); DeleteDC(kdc); DeleteObject(mb); DeleteObject(kb); (void)win;
}
static void run_static(HWND win,HDC sdc) {
 static const int ctl[2][4]={{24,20,330,14},{60,50,318,13}}; HWND c[2]; int k,x,y; static COLORREF was[2][16][60];
 stripes(sdc); GdiFlush();
 for(k=0;k<2;k++) for(y=0;y<ctl[k][3]&&y<16;y++) for(x=0;x<60;x++) was[k][y][x]=GetPixel(sdc,ctl[k][0]+ctl[k][2]-60+x,ctl[k][1]+y);
 c[0]=CreateWindowA("STATIC","Choose Destination Location",WS_CHILD|WS_VISIBLE|SS_LEFT,ctl[0][0],ctl[0][1],ctl[0][2],ctl[0][3],win,(HMENU)50,GetModuleHandle(NULL),NULL);
 c[1]=CreateWindowA("STATIC","Select folder where Setup will install files.",WS_CHILD|WS_VISIBLE|SS_LEFT,ctl[1][0],ctl[1][1],ctl[1][2],ctl[1][3],win,(HMENU)51,GetModuleHandle(NULL),NULL);
 for(k=0;k<2;k++) {SendMessage(c[k],WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0); InvalidateRect(c[k],NULL,TRUE); UpdateWindow(c[k]);}
 pump(); Sleep(300); pump();
 for(k=0;k<2;k++) {
  int wrong=0,white=0,fx=-1,fy=-1; COLORREF fc=0,fe=0;
  /* the last 60 pixels of the control: no text there, the stripes must show */
  for(y=ctl[k][1];y<ctl[k][1]+ctl[k][3];y++) for(x=ctl[k][0]+ctl[k][2]-60;x<ctl[k][0]+ctl[k][2];x++) {
   COLORREF s=GetPixel(sdc,x,y),e=was[k][y-ctl[k][1]][x-(ctl[k][0]+ctl[k][2]-60)];
   if(s!=e) {if(!wrong) {fx=x; fy=y; fc=s; fe=e;} wrong++;}
  }
  for(y=ctl[k][1];y<ctl[k][1]+ctl[k][3];y++) for(x=ctl[k][0];x<ctl[k][0]+120;x++) if(GetPixel(sdc,x,y)==GetNearestColor(sdc,RGB(255,255,255))&&(x/8)%6!=2) white++;
  cases++;
  say("static control %d (hollow brush, transparent, white text): ",50+k);
  if(wrong) {bad++; say("FAIL: %d pixels beside the text are not the background; first at %d,%d: %06lX, it was %06lX before the control; white text pixels %d\n",wrong,fx,fy,(unsigned long)fc,(unsigned long)fe,white);}
  else say("ok (the background shows; white text pixels %d%s)\n",white,white?"":": NO TEXT DRAWN");
  if(!wrong&&!white) bad++;
 }
 Sleep(1500); DestroyWindow(c[0]); DestroyWindow(c[1]);
}
int WINAPI WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR cmd,int show) {
 WNDCLASSA wc; HWND win; HDC sdc; RECT r; char path[MAX_PATH],*q; SYSTEMTIME t;
 (void)prev; (void)cmd; (void)show;
 GetModuleFileNameA(NULL,path,sizeof(path)); q=strrchr(path,'\\'); if(q) q[1]=0; else path[0]=0; strcat(path,"VXHOLLOW.TXT");
 lf=fopen(path,"a"); if(!lf) return 1;
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=inst; wc.lpszClassName="VXHOLLOW"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClassA(&wc);
 r.left=40; r.top=40; r.right=40+W; r.bottom=40+H;
 win=CreateWindowExA(WS_EX_TOPMOST,"VXHOLLOW","",WS_POPUP|WS_VISIBLE,r.left,r.top,W,H,NULL,NULL,inst,NULL);
 pump(); Sleep(300); pump();
 sdc=GetDC(win); GetLocalTime(&t);
 say("==== VXHOLLOW v1.1 %04u-%02u-%02u %02u:%02u:%02u ====\nscreen %dx%d, %d bits per pixel\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,
  GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),GetDeviceCaps(sdc,BITSPIXEL)*GetDeviceCaps(sdc,PLANES));
 run_ops(win,sdc); run_static(win,sdc);
 say("RESULT: %d of %d cases failed\n\n",bad,cases);
 ReleaseDC(win,sdc); DestroyWindow(win); fclose(lf);
 MessageBoxA(NULL,bad?"VXHOLLOW: failures. See VXHOLLOW.TXT.":"VXHOLLOW: all passed. See VXHOLLOW.TXT.","VXHOLLOW",MB_OK|MB_TOPMOST);
 return bad?1:0;
}
