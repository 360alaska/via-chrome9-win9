/* VX2HOLD v2.0: guided shift test. For each mode and each variant it sets the mode through DirectDraw, draws the VX2MODE colour bars,
 * and waits for your verdict: press G (or click GOOD) when the picture is correct, B (or click BAD) when it is shifted/wrapped,
 * S skips, Esc quits. Variants reproduce what VX2MODE does one thing at a time:
 *   base  - mode set, surface, DirectDraw colour fills
 *   crt   - base + the driver register dump after the mode set, after the surface is created and after drawing
 *   flip  - base + the driver flip-info call at the same points
 *   lock  - base + lock/unlock of the surface before drawing
 *   all   - crt + flip + lock together (what VX2MODE does)
 *   late  - base, but nothing is drawn until 3 seconds after the mode set
 *   cpu   - base, but the bars are written by the CPU instead of DirectDraw fills
 * Variants 8 and 9 run a flip chain (as VX2MODE does at the end of each mode) before the mode set. Add "flips" to the command line to run only those two.
 * Command line: optional modes like "800x600x16 1024x768x32" (default those two). Writes VX2HOLD.TXT with every verdict. */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include <io.h>
#include "vx85598.h"
#include "vx855_flip.h"
#define F_CRT 1
#define F_FLIP 2
#define F_LOCK 4
#define F_LATE 8
#define F_CPU 16
#define F_FCHAIN 32
#define F_SAME 64
static LPDIRECTDRAW dd; static HANDLE dev=INVALID_HANDLE_VALUE; static int curflags;
static const struct {const char *name; int flags;} variants[]={{"base",0},{"crt",F_CRT},{"flip",F_FLIP},{"lock",F_LOCK},{"all (like VX2MODE)",F_CRT|F_FLIP|F_LOCK},{"late draw",F_LATE},{"CPU fills",F_CPU},{"after a flip chain at 640x480x16",F_FCHAIN},{"after a flip chain at the same mode",F_FCHAIN|F_SAME}};
#define NVAR 9
static volatile int lastkey,clicked; static volatile POINT clickpt; static FILE *lf;
static void logl(const char *fmt,const char *a,int n) {if(lf) {fprintf(lf,fmt,a,n); fflush(lf); _commit(_fileno(lf));}}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_ERASEBKGND) return 1;
 if(m==WM_KEYDOWN||m==WM_SYSKEYDOWN) {lastkey=(int)a; return 0;}
 if(m==WM_CHAR) {lastkey=(int)a; return 0;}
 if(m==WM_LBUTTONDOWN) {clickpt.x=(short)LOWORD(b); clickpt.y=(short)HIWORD(b); clicked=1; return 0;}
 return DefWindowProc(w,m,a,b);
}
static void poke(void) {
 static vx_crt_report rep; DWORD got=0,r[VXF_REPLY_DWORDS]; vxf_request q;
 if((curflags&F_CRT)&&dev!=INVALID_HANDLE_VALUE) DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,&rep,sizeof(rep),&got,NULL);
 if((curflags&F_FLIP)&&dev!=INVALID_HANDLE_VALUE) {q.cb=sizeof(q); q.cmd=VXF_INFO; q.arg=0; q.flags=0; DeviceIoControl(dev,VX_IOCTL_FLIP,&q,sizeof(q),r,4*VXF_REPLY_DWORDS,&got,NULL);}
}
static DWORD rgb(DWORD bpp,int r,int g,int b) {return bpp==16?(DWORD)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)):(DWORD)((r<<16)|(g<<8)|b);}
static void fill(LPDIRECTDRAWSURFACE s,DWORD bpp,DWORD c,int x0,int y0,int x1,int y1) {
 if(!(curflags&F_CPU)) {RECT r; DDBLTFX f; SetRect(&r,x0,y0,x1,y1); memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=c; IDirectDrawSurface_Blt(s,&r,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f); return;}
 {DDSURFACEDESC d; int x,y; memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
  if(FAILED(IDirectDrawSurface_Lock(s,NULL,&d,DDLOCK_WAIT,NULL))) return;
  for(y=y0;y<y1;y++) {BYTE *p=(BYTE*)d.lpSurface+(DWORD)y*d.lPitch; for(x=x0;x<x1;x++) {if(bpp==16) ((WORD*)p)[x]=(WORD)c; else ((DWORD*)p)[x]=c;}}
  IDirectDrawSurface_Unlock(s,NULL);}
}
static void label(LPDIRECTDRAWSURFACE s,const char *t,int x,int y,COLORREF fg,COLORREF bg) {
 HDC dc; if(SUCCEEDED(IDirectDrawSurface_GetDC(s,&dc))) {SetBkMode(dc,OPAQUE); SetBkColor(dc,bg); SetTextColor(dc,fg); TextOut(dc,x,y,t,(int)strlen(t)); IDirectDrawSurface_ReleaseDC(s,dc);}
}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m);DispatchMessage(&m);}}
static void waitkeys(void) {DWORD e=GetTickCount()+800UL; while(GetTickCount()<e) {pump(); Sleep(20);} lastkey=0; clicked=0;}
static void flipcycle(void) {
 DDSURFACEDESC s; DDSCAPS caps; LPDIRECTDRAWSURFACE prim=NULL,back=NULL; int i; DWORD cols[3];
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; s.dwBackBufferCount=2; s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
 if(FAILED(IDirectDraw_CreateSurface(dd,&s,&prim,NULL))) return;
 memset(&caps,0,sizeof(caps)); caps.dwCaps=DDSCAPS_BACKBUFFER;
 if(FAILED(IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back))) {IDirectDrawSurface_Release(prim); return;}
 cols[0]=0x00DC2828UL; cols[1]=0x0028C83CUL; cols[2]=0x003246E6UL; /* the exact colour does not matter here */
 for(i=0;i<6;i++) {
  DDBLTFX f; memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=cols[i%3]; IDirectDrawSurface_Blt(back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f);
  if(FAILED(IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT))) break;
  IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back);
 }
 IDirectDraw_FlipToGDISurface(dd);
 IDirectDrawSurface_Release(prim);
}
int main_run(HINSTANCE hi,LPSTR cmd) {
 WNDCLASS wc; HWND win; HMODULE mod; typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*); CREATE create;
 DWORD W[8],H[8],B[8]; int n=0,mi,vi,quit=0,total=0,done=0,first=0; const char *p; FILE *lf; static char res[64][100]; int nres=0;
 p=cmd?cmd:"";
 lf=fopen("VX2HOLD.TXT","w"); logl("VX2HOLD v2.1 (driver v10.61)%s\n","",0);
 while(*p) {DWORD w,h,b; int k; while(*p==' ') p++; if(sscanf(p,"%lux%lux%lu%n",&w,&h,&b,&k)==3&&n<8&&(b==16||b==32)) {W[n]=w;H[n]=h;B[n]=b;n++;p+=k;} else while(*p&&*p!=' ') p++;}
 if(!n) {W[0]=800;H[0]=600;B[0]=16;W[1]=1024;H[1]=768;B[1]=32;n=2;}
 total=n*NVAR;
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2HOLD"; RegisterClass(&wc);
 win=CreateWindowEx(WS_EX_TOPMOST,wc.lpszClassName,"VX2HOLD",WS_POPUP|WS_VISIBLE,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,hi,NULL);
 SetForegroundWindow(win); SetFocus(win);
 mod=LoadLibraryA("DDRAW.DLL"); create=mod?(CREATE)GetProcAddress(mod,"DirectDrawCreate"):NULL;
 if(!win||!create||FAILED(create(NULL,&dd,NULL))||FAILED(IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))) {MessageBox(NULL,"DirectDraw not available","VX2HOLD",MB_OK); return 1;}
 if(cmd&&strstr(cmd,"flips")) first=7;
 total=n*(NVAR-first);
 for(mi=0;mi<n&&!quit;mi++) for(vi=first;vi<NVAR&&!quit;vi++) {
  DWORD w=W[mi],h=H[mi],b=B[mi],end; LPDIRECTDRAWSURFACE ps=NULL; DDSURFACEDESC d; DWORD c[8]; int k,verdict=0; char t[120]; RECT good,bad;
  curflags=variants[vi].flags; lastkey=0; clicked=0;
  {char hdr[120]; sprintf(hdr,"START %lux%lux%lu %s",W[mi],H[mi],B[mi],variants[vi].name); logl("%s\n",hdr,0);}
  if(curflags&F_FCHAIN) {if(curflags&F_SAME) IDirectDraw_SetDisplayMode(dd,w,h,b); else IDirectDraw_SetDisplayMode(dd,640,480,16); logl("%s\n","  running a flip chain first",0); flipcycle(); logl("%s\n","  flip chain done",0);}
  if(FAILED(IDirectDraw_SetDisplayMode(dd,w,h,b))) {sprintf(res[nres++],"%lux%lux%lu %s: SetDisplayMode failed",w,h,b,variants[vi].name); continue;}
  poke();
  if(curflags&F_LATE) {end=GetTickCount()+3000UL; while(GetTickCount()<end) {pump(); Sleep(20);}}
  memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
  if(FAILED(IDirectDraw_CreateSurface(dd,&d,&ps,NULL))) {sprintf(res[nres++],"%lux%lux%lu %s: CreateSurface failed",w,h,b,variants[vi].name); continue;}
  if(curflags&F_LOCK) {DDSURFACEDESC l; memset(&l,0,sizeof(l)); l.dwSize=sizeof(l); if(SUCCEEDED(IDirectDrawSurface_Lock(ps,NULL,&l,DDLOCK_WAIT,NULL))) IDirectDrawSurface_Unlock(ps,NULL);}
  poke();
  c[0]=rgb(b,255,255,255); c[1]=rgb(b,255,255,0); c[2]=rgb(b,0,255,255); c[3]=rgb(b,0,255,0); c[4]=rgb(b,255,0,255); c[5]=rgb(b,255,0,0); c[6]=rgb(b,0,0,255); c[7]=rgb(b,0,0,0);
  for(k=0;k<8;k++) fill(ps,b,c[k],(int)(w*k/8),(int)(h/4),(int)(w*(k+1)/8),(int)(h*3/4));
  fill(ps,b,rgb(b,40,40,40),0,0,(int)w,(int)h/4); fill(ps,b,rgb(b,40,40,40),0,(int)(h*3/4),(int)w,(int)h);
  fill(ps,b,c[0],0,0,(int)w,1); fill(ps,b,c[0],0,(int)h-1,(int)w,(int)h); fill(ps,b,c[0],0,0,1,(int)h); fill(ps,b,c[0],(int)w-1,0,(int)w,(int)h);
  poke();
  SetRect(&good,20,70,(int)w/2-10,(int)h/4-10); SetRect(&bad,(int)w/2+10,70,(int)w-20,(int)h/4-10);
  fill(ps,b,rgb(b,0,140,0),good.left,good.top,good.right,good.bottom); fill(ps,b,rgb(b,170,0,0),bad.left,bad.top,bad.right,bad.bottom);
  sprintf(t," test %d of %d: %lux%lux%lu, variant: %s ",done+1,total,w,h,b,variants[vi].name); label(ps,t,12,12,RGB(255,255,255),RGB(0,0,0));
  label(ps," Is this picture correct?  G = GOOD   B = BAD   S = skip   Esc = quit ",12,36,RGB(255,255,255),RGB(0,0,0));
  label(ps," GOOD ",good.left+8,good.top+8,RGB(255,255,255),RGB(0,140,0)); label(ps," BAD ",bad.left+8,bad.top+8,RGB(255,255,255),RGB(170,0,0));
  logl("%s\n","  drawn, waiting for a verdict",0); end=GetTickCount()+40000UL; Sleep(700); lastkey=0; clicked=0; GetAsyncKeyState('G'); GetAsyncKeyState('B'); GetAsyncKeyState('S'); GetAsyncKeyState(VK_ESCAPE);
  while(GetTickCount()<end&&!verdict) {
   POINT pt;
   pump();
   if(lastkey=='g'||lastkey=='G') verdict=1; else if(lastkey=='b'||lastkey=='B') verdict=2; else if(lastkey=='s'||lastkey=='S') verdict=3; else if(lastkey==VK_ESCAPE||lastkey==27) {verdict=4; quit=1;}
   else if((GetAsyncKeyState('G')&0x8001)) verdict=1; else if((GetAsyncKeyState('B')&0x8001)) verdict=2; else if((GetAsyncKeyState('S')&0x8001)) verdict=3; else if(GetAsyncKeyState(VK_ESCAPE)&0x8001) {verdict=4; quit=1;}
   else if(clicked) {clicked=0; pt.x=clickpt.x; pt.y=clickpt.y; if(PtInRect(&good,pt)) verdict=1; else if(PtInRect(&bad,pt)) verdict=2;}
   else if((GetAsyncKeyState(VK_LBUTTON)&0x8000)&&GetCursorPos(&pt)) {if(PtInRect(&good,pt)) verdict=1; else if(PtInRect(&bad,pt)) verdict=2;}
   Sleep(30);
  }
  IDirectDrawSurface_Release(ps);
  sprintf(res[nres++],"%lux%lux%lu  %-20s %s",w,h,b,variants[vi].name,verdict==1?"GOOD":verdict==2?"BAD (shifted)":verdict==3?"skipped":verdict==4?"quit":"no answer (timed out after 40 s)");
  logl("RESULT %s\n",res[nres-1],0); done++; waitkeys();
 }
 IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd); DestroyWindow(win);
 if(lf) fclose(lf);
 {char msg[400]; int i,bads=0; msg[0]=0; for(i=0;i<nres;i++) if(strstr(res[i],"BAD")) bads++; sprintf(msg,"Finished: %d verdicts, %d bad. Results are in VX2HOLD.TXT - please send it.",nres,bads); MessageBox(NULL,msg,"VX2HOLD",MB_OK);}
 return 0;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show_) {(void)prev;(void)show_; return main_run(hi,cmd);}
