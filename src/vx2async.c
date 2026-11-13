/* VX2ASYNC v1.0: asynchronous DirectDraw blits (driver v10.95).
 * In full-screen exclusive mode the driver can start a DirectDraw blit or colour fill and return while the engine is still
 * drawing; the next Lock, GetBltStatus, Flip, blit or any other driver call waits for it first. This program checks that
 * every such wait point sees finished pixels, using random fill/copy sequences against a software model, and measures how
 * much CPU time the overlap gives back. It needs the desktop at 640x480 or larger in 16 or 32 bits per pixel. The screen shows
 * solid colours for a few seconds. If every check passes (and async blits were really used) it writes
 * HKLM\Software\VX85598 ACCEL_ASYNC=1 (restart Windows to use it); otherwise it changes nothing.
 * The live async setting is put back to what it was. Writes VX2ASYNC.TXT and VX2ASYNC.RES. */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <io.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"
#include "vx855_2d.h"
#include "vx855_flip.h"
static HANDLE dev=INVALID_HANDLE_VALUE;
static FILE *logfile;
static HWND win;
static LPDIRECTDRAW dd;
static DWORD depth,sw,sh,fails,aborted,mask;
static LARGE_INTEGER freq;
static void logline(const char *fmt,...) {
 va_list ap; va_start(ap,fmt); vfprintf(logfile,fmt,ap); va_end(ap); fflush(logfile); _commit(_fileno(logfile));
}
static double now(void) {
 LARGE_INTEGER t;
 if(freq.QuadPart&&QueryPerformanceCounter(&t)) return 1000.0*(double)t.QuadPart/(double)freq.QuadPart;
 return (double)GetTickCount();
}
static void pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_CLOSE) {aborted=1;return 0;}
 if(m==WM_ERASEBKGND) return 1;
 return DefWindowProc(w,m,a,b);
}
/* driver control: out[0]=async ops started, out[1]=waits that finished one, out[2]=one pending now, out[3]=bit0 async on, bit1 gate */
static int ctl(DWORD code,DWORD *out) {
 DWORD o[4],got=0; memset(o,0,sizeof(o));
 if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0;
 if(out) memcpy(out,o,16);
 return 1;
}
static int counters(DWORD *c) {return ctl(0xFFFFFE12UL,c);}
static void asyncset(int on) {
 ctl(0xFC000000UL|(on?1UL:0UL),NULL);
 if(on) ctl(0xFD000001UL,NULL);
}
static DWORD seed=12345;
static DWORD rnd(void) {seed=seed*1103515245UL+12345UL; return (seed>>8)&0xFFFFFFUL;}
static DWORD rcolor(void) {return ((rnd()<<8)^rnd())&mask;}
static int lock_surf(LPDIRECTDRAWSURFACE s,DDSURFACEDESC *d) {
 memset(d,0,sizeof(*d)); d->dwSize=sizeof(*d);
 return SUCCEEDED(IDirectDrawSurface_Lock(s,NULL,d,DDLOCK_WAIT,NULL));
}
static DWORD getpx(const DDSURFACEDESC *d,int x,int y) {
 BYTE *p=(BYTE*)d->lpSurface+(DWORD)y*d->lPitch;
 return depth==16?((WORD*)p)[x]:(((DWORD*)p)[x]&0xFFFFFFUL);
}
static void putpx(const DDSURFACEDESC *d,int x,int y,DWORD v) {
 BYTE *p=(BYTE*)d->lpSurface+(DWORD)y*d->lPitch;
 if(depth==16) ((WORD*)p)[x]=(WORD)v; else ((DWORD*)p)[x]=v;
}
static LPDIRECTDRAWSURFACE mk(int w,int h) {
 DDSURFACEDESC s; LPDIRECTDRAWSURFACE r=NULL; HRESULT hr;
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; s.dwWidth=w; s.dwHeight=h; s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 hr=IDirectDraw_CreateSurface(dd,&s,&r,NULL);
 if(FAILED(hr)) {logline("  CreateSurface %dx%d failed: %08lX\n",w,h,(DWORD)hr); return NULL;}
 return r;
}
#define N 256
static DWORD ma[N*N],mb[N*N],mp[N*N],tmp[N*N];
static void mfill(DWORD *m,RECT r,DWORD c) {int x,y; for(y=r.top;y<r.bottom;y++) for(x=r.left;x<r.right;x++) m[y*N+x]=c;}
static void mcopy(DWORD *dm,int dx,int dy,const DWORD *sm,int sx,int sy,int w,int h) {
 int x,y;
 for(y=0;y<h;y++) for(x=0;x<w;x++) tmp[y*w+x]=sm[(sy+y)*N+sx+x];
 for(y=0;y<h;y++) for(x=0;x<w;x++) dm[(dy+y)*N+dx+x]=tmp[y*w+x];
}
static void upload(LPDIRECTDRAWSURFACE s,const DWORD *m,int ox,int oy) {
 DDSURFACEDESC d; int x,y;
 if(!lock_surf(s,&d)) {fails++; return;}
 for(y=0;y<N;y++) for(x=0;x<N;x++) putpx(&d,ox+x,oy+y,m[y*N+x]);
 IDirectDrawSurface_Unlock(s,d.lpSurface);
}
static int compare(LPDIRECTDRAWSURFACE s,const DWORD *m,int ox,int oy,RECT r) {
 DDSURFACEDESC d; int x,y,bad=0;
 if(!lock_surf(s,&d)) return 1<<20;
 for(y=r.top;y<r.bottom;y++) for(x=r.left;x<r.right;x++) if(getpx(&d,ox+x,oy+y)!=m[y*N+x]) bad++;
 IDirectDrawSurface_Unlock(s,d.lpSurface);
 return bad;
}
static void randrect(RECT *r,int minw) {
 int w=minw+(int)(rnd()%(N-minw)),h=minw+(int)(rnd()%(N-minw)),x=(int)(rnd()%(N-w+1)),y=(int)(rnd()%(N-h+1));
 SetRect(r,x,y,x+w,y+h);
}
/* one random operation; returns the destination rectangle (in surface coordinates of the target) */
static int doop(LPDIRECTDRAWSURFACE A,LPDIRECTDRAWSURFACE B,RECT *dr,int *which,int withprimary,LPDIRECTDRAWSURFACE P,int px,int py) {
 DWORD k=rnd()%6; RECT s; DDBLTFX f; HRESULT hr; LPDIRECTDRAWSURFACE dst,src; DWORD *dm,*sm; int w,h,dx,dy,sx,sy,ox=0,oy=0;
 (void)withprimary;
 memset(&f,0,sizeof(f)); f.dwSize=sizeof(f);
 if(k<2) { /* colour fill of A, B or the primary window */
  DWORD t=rnd()%(P?3:2),c=rcolor();
  dst=t==0?A:t==1?B:P; dm=t==0?ma:t==1?mb:mp; *which=(int)t;
  if(t==2) {ox=px; oy=py;}
  randrect(&s,4); f.dwFillColor=c; *dr=s;
  { RECT rr=s; OffsetRect(&rr,ox,oy); hr=IDirectDrawSurface_Blt(dst,&rr,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f); }
  if(FAILED(hr)) {logline("  colour fill failed %08lX\n",(DWORD)hr); fails++; return 0;}
  mfill(dm,s,c); return 1;
 }
 { /* copy of a same-size rectangle between A, B and the primary window (same surface allowed: overlap) */
  DWORD ts=rnd()%(P?3:2),td=rnd()%(P?3:2);
  int sox=0,soy=0,dox=0,doy=0;
  src=ts==0?A:ts==1?B:P; sm=ts==0?ma:ts==1?mb:mp; if(ts==2) {sox=px; soy=py;}
  dst=td==0?A:td==1?B:P; dm=td==0?ma:td==1?mb:mp; if(td==2) {dox=px; doy=py;}
  *which=(int)td;
  w=8+(int)(rnd()%(N-8)); h=8+(int)(rnd()%(N-8)); sx=(int)(rnd()%(N-w+1)); sy=(int)(rnd()%(N-h+1)); dx=(int)(rnd()%(N-w+1)); dy=(int)(rnd()%(N-h+1));
  if(src==dst&&k==5) {dx=sx+(int)(rnd()%9)-4; dy=sy+(int)(rnd()%9)-4; if(dx<0) dx=0; if(dy<0) dy=0; if(dx+w>N) dx=N-w; if(dy+h>N) dy=N-h;}
  { RECT sr,rr; SetRect(&sr,sox+sx,soy+sy,sox+sx+w,soy+sy+h); SetRect(&rr,dox+dx,doy+dy,dox+dx+w,doy+dy+h);
    hr=IDirectDrawSurface_Blt(dst,&rr,src,&sr,DDBLT_WAIT,NULL); }
  if(FAILED(hr)) {logline("  copy %lu->%lu failed %08lX\n",ts,td,(DWORD)hr); fails++; return 0;}
  mcopy(dm,dx,dy,sm,sx,sy,w,h); SetRect(dr,dx,dy,dx+w,dy+h); return 1;
 }
}
static void checkpoint(const char *name,DWORD *c0,int bad) {
 DWORD c[4]; counters(c);
 logline("%s: %s (%d mismatching pixels); async ops %lu (+%lu), waits %lu (+%lu)\n",name,bad?"FAIL":"PASS",bad,c[0],c[0]-c0[0],c[1],c[1]-c0[1]);
 if(bad) fails++;
 memcpy(c0,c,16);
}
static void tests(LPDIRECTDRAWSURFACE A,LPDIRECTDRAWSURFACE B,LPDIRECTDRAWSURFACE P) {
 DWORD c0[4]; int i,bad,w;
 RECT all; HRESULT hr; LPDIRECTDRAWSURFACE which[3];
 SetRect(&all,0,0,N,N); which[0]=A; which[1]=B; which[2]=P;
 for(i=0;i<N*N;i++) {ma[i]=rcolor(); mb[i]=rcolor(); mp[i]=rcolor();}
 upload(A,ma,0,0); upload(B,mb,0,0);
 { /* the primary window: a 256x256 square in the middle of the screen */
   int px=((int)sw-N)/2,py=((int)sh-N)/2; (void)px; (void)py; }
 counters(c0);
 { /* T1: 60 random operations, then a single comparison */
  int px=((int)sw-N)/2,py=((int)sh-N)/2; RECT d; int wh;
  upload(P,mp,px,py);
  for(i=0;i<60&&!aborted;i++) doop(A,B,&d,&wh,1,P,px,py);
  bad=compare(A,ma,0,0,all)+compare(B,mb,0,0,all)+compare(P,mp,px,py,all);
  checkpoint("T1 60 random fills and copies (A, B, screen), one comparison at the end",c0,bad);
  /* T2: compare the destination right after each operation: Lock has to wait for the engine */
  bad=0;
  for(i=0;i<150&&!aborted;i++) {
   LPDIRECTDRAWSURFACE s; DWORD *m; int ox=0,oy=0;
   doop(A,B,&d,&wh,1,P,px,py);
   s=which[wh]; m=wh==0?ma:wh==1?mb:mp; if(wh==2) {ox=px; oy=py;}
   bad+=compare(s,m,ox,oy,d);
  }
  checkpoint("T2 150 operations, each destination locked and compared immediately",c0,bad);
  /* T3: queue many copies, ask GetBltStatus, then compare */
  bad=0; w=0;
  {
   int polls=0,still=0; double t0;
   for(i=0;i<12;i++) {
    DWORD c=rcolor(); DDBLTFX f; RECT r1,r2;
    memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=c;
    IDirectDrawSurface_Blt(A,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f); mfill(ma,all,c);
    SetRect(&r1,0,0,N,N); SetRect(&r2,0,0,N,N);
    IDirectDrawSurface_Blt(B,&r2,A,&r1,DDBLT_WAIT,NULL); mcopy(mb,0,0,ma,0,0,N,N);
   }
   t0=now();
   for(;;) {
    hr=IDirectDrawSurface_GetBltStatus(B,DDGBS_ISBLTDONE); polls++;
    if(hr==DD_OK) break;
    if(hr==DDERR_WASSTILLDRAWING) still++; else {logline("  GetBltStatus returned %08lX\n",(DWORD)hr); fails++; break;}
    if(now()-t0>2000) {logline("  GetBltStatus still not done after 2 s\n"); fails++; break;}
   }
   bad=compare(B,mb,0,0,all)+compare(A,ma,0,0,all);
   logline("  GetBltStatus: %d polls, %d answered 'still drawing'\n",polls,still);
   checkpoint("T3 24 queued operations, GetBltStatus until done, then compare",c0,bad);
  }
 }
}
static void flipchain(void) {
 DDSURFACEDESC s; DDSCAPS caps; LPDIRECTDRAWSURFACE prim=NULL,back=NULL; HRESULT hr; int i,bad=0; DWORD c0[4]; DWORD cols[3];
 logline("\nT4 flip chain: colour fill of the back buffer, Flip, lock the new front and test pixels\n");
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; s.dwBackBufferCount=1;
 s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
 hr=IDirectDraw_CreateSurface(dd,&s,&prim,NULL);
 if(FAILED(hr)) {logline("  no flip chain (%08lX): skipped\n",(DWORD)hr); return;}
 memset(&caps,0,sizeof(caps)); caps.dwCaps=DDSCAPS_BACKBUFFER;
 if(FAILED(IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back))) {logline("  no back buffer: skipped\n"); IDirectDrawSurface_Release(prim); return;}
 cols[0]=depth==16?0xF800:0xDC2828; cols[1]=depth==16?0x07E0:0x28C83C; cols[2]=depth==16?0x001F:0x3246E6;
 counters(c0);
 for(i=0;i<30&&!aborted;i++) {
  DDBLTFX f; DDSURFACEDESC d; int px[5],py[5],k; DWORD want=cols[i%3];
  px[0]=3; px[1]=(int)sw/2; px[2]=(int)sw-4; px[3]=17; px[4]=(int)sw/3; py[0]=3; py[1]=(int)sh/2; py[2]=(int)sh-4; py[3]=(int)sh-9; py[4]=(int)sh/5;
  memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=want;
  IDirectDrawSurface_Blt(back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f);
  hr=IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT);
  if(FAILED(hr)) {logline("  flip %d failed %08lX\n",i,(DWORD)hr); bad++; break;}
  if(!lock_surf(prim,&d)) {bad++; break;}
  for(k=0;k<5;k++) if(getpx(&d,px[k],py[k])!=want) bad++;
  IDirectDrawSurface_Unlock(prim,d.lpSurface);
  IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back);
  if((i&3)==3) pump();
 }
 checkpoint("T4 30 fill+Flip rounds",c0,bad);
 IDirectDraw_FlipToGDISurface(dd);
 if(back) IDirectDrawSurface_Release(back);
 IDirectDrawSurface_Release(prim);
}
static volatile DWORD sink;
static DWORD spin(DWORD n) {DWORD i,a=1; for(i=0;i<n;i++) a=a*1664525UL+1013904223UL; sink=a; return a;}
static double loop(LPDIRECTDRAWSURFACE C,LPDIRECTDRAWSURFACE D,DWORD cpuiters,int n) {
 RECT r; double t0; int i;
 SetRect(&r,0,0,640,400);
 t0=now();
 for(i=0;i<n;i++) {
  IDirectDrawSurface_Blt(D,&r,C,&r,DDBLT_WAIT,NULL);
  spin(cpuiters);
 }
 { DDSURFACEDESC d; if(lock_surf(D,&d)) IDirectDrawSurface_Unlock(D,d.lpSurface); } /* the last copy has to be finished too */
 return (now()-t0)/n;
}
static void timing(void) {
 LPDIRECTDRAWSURFACE C=mk(640,400),D=mk(640,400); DWORD it; double t0,t1,per,a[2][2]; DWORD c0[4],c1[4]; int r,m;
 logline("\nT5 CPU overlap: a 640x400 copy followed by about 1 ms of CPU work, 100 times\n");
 if(!C||!D) {logline("  surfaces unavailable: skipped\n"); if(C) IDirectDrawSurface_Release(C); if(D) IDirectDrawSurface_Release(D); return;}
 t0=now(); spin(2000000UL); t1=now(); per=(t1-t0)/2000000.0; it=(DWORD)(1.0/per); if(it<1000) it=1000;
 logline("  CPU work: %lu loop iterations = %.2f ms\n",it,(t1-t0)*it/2000000.0);
 asyncset(0); a[0][0]=loop(C,D,0,100); a[0][1]=loop(C,D,it,100);
 counters(c0); asyncset(1); a[1][0]=loop(C,D,0,100); a[1][1]=loop(C,D,it,100); counters(c1);
 for(m=0;m<2;m++) for(r=0;r<2;r++) (void)a[m][r];
 logline("  copy only, async off: %.3f ms per copy; async on: %.3f ms\n",a[0][0],a[1][0]);
 logline("  copy + 1 ms CPU work, async off: %.3f ms per round; async on: %.3f ms (%.0f%% of the off time)\n",a[0][1],a[1][1],100.0*a[1][1]/a[0][1]);
 logline("  async operations in the timed async runs: %lu\n",c1[0]-c0[0]);
 if(c1[0]==c0[0]) {logline("  FAIL: the timed runs used no asynchronous blits\n"); fails++;}
 IDirectDrawSurface_Release(C); IDirectDrawSurface_Release(D);
}
static void finish(const char *res) {
 FILE *f=fopen("VX2ASYNC.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}
 logline("%s\n",res);
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show_) {
 WNDCLASS wc; vx_status_t s; DWORD got,c[4],orig=0; HMODULE mod; HRESULT hr; LPDIRECTDRAWSURFACE A=NULL,B=NULL,P=NULL; int engaged=0;
 typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*);
 (void)prev;(void)show_;(void)cmd;
 logfile=fopen("VX2ASYNC.TXT","w"); if(!logfile) return 1;
 logline("VX2ASYNC v1.0 / driver v10.95\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC) {finish("FAIL: the VX85598 driver is not active"); return 1;}
 if(s.bpp!=16&&s.bpp!=32) {finish("FAIL: set the desktop to 16 or 32 bits per pixel first"); return 1;}
 if(!counters(c)) {finish("FAIL: this driver has no async control (VX85598.VXD older than v10.95)"); return 1;}
 depth=s.bpp; sw=s.width; sh=s.height; mask=depth==16?0xFFFFUL:0xFFFFFFUL; orig=c[3]&1; QueryPerformanceFrequency(&freq);
 logline("Desktop %lux%lux%lu; async was %s; registry ACCEL_ASYNC decides at start-up\n",sw,sh,depth,orig?"on":"off");
 if(sw<N||sh<N) {finish("FAIL: the desktop is too small"); return 1;}
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2ASYNC"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClass(&wc);
 win=CreateWindowEx(WS_EX_TOPMOST,wc.lpszClassName,"VX2ASYNC - Esc aborts",WS_POPUP|WS_VISIBLE,0,0,(int)sw,(int)sh,NULL,NULL,hi,NULL);
 mod=LoadLibraryA("DDRAW.DLL"); if(!win||!mod) {finish("FAIL: no window or DDRAW.DLL"); return 1;}
 {CREATE create=(CREATE)GetProcAddress(mod,"DirectDrawCreate"); if(!create) {finish("FAIL: no DirectDrawCreate"); return 1;} hr=create(NULL,&dd,NULL); if(FAILED(hr)) {finish("FAIL: DirectDrawCreate failed"); return 1;}}
 hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN); logline("SetCooperativeLevel(exclusive, fullscreen): %08lX\n",(DWORD)hr);
 if(FAILED(hr)) {finish("FAIL: exclusive mode refused"); return 1;}
 /* switch async on live; the HAL's exclusive-mode notice must have raised the gate */
 counters(c); logline("right after SetCooperativeLevel (async still off): flags %lu (bit1 = the HAL told the driver about exclusive mode)\n",c[3]);
 ctl(0xFC000001UL,NULL); counters(c);
 logline("after SetCooperativeLevel and live async on: flags %lu (bit0 async on, bit1 exclusive-mode gate)\n",c[3]);
 if(c[3]!=3) {logline("FAIL: the driver gate is not set by entering exclusive mode (HAL notice missing); setting it by hand to continue\n"); fails++; ctl(0xFD000001UL,NULL);}
 { DDSURFACEDESC d; memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
   hr=IDirectDraw_CreateSurface(dd,&d,&P,NULL); logline("CreateSurface(primary): %08lX\n",(DWORD)hr); if(FAILED(hr)) P=NULL; }
 A=mk(N,N); B=mk(N,N);
 if(!A||!B||!P) {logline("could not create the test surfaces\n"); fails++;}
 else {
  tests(A,B,P);
  counters(c); engaged=c[0]>0&&c[1]>0;
  logline("totals: %lu async blits started, %lu waits that found one still running\n",c[0],c[1]);
  if(!engaged) {logline("FAIL: no blit ran asynchronously; the switch or the gate did not work\n"); fails++;}
 }
 if(P) {IDirectDrawSurface_Release(P); P=NULL;}
 if(A) IDirectDrawSurface_Release(A);
 if(B) IDirectDrawSurface_Release(B);
 if(!aborted) flipchain();
 if(!aborted) timing();
 /* leaving exclusive mode: the HAL clears the gate; put the live switch back */
 IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL);
 ctl(0xFC000000UL|orig,NULL); counters(c); logline("after leaving exclusive mode: flags %lu (gate should be 0)\n",c[3]);
 if(c[3]&2) {logline("FAIL: the gate is still on after leaving exclusive mode\n"); fails++; ctl(0xFD000000UL,NULL);}
 if(dd) IDirectDraw_Release(dd);
 if(win) DestroyWindow(win);
 if(aborted) finish("FAIL: aborted; nothing was changed");
 else if(fails) finish("FAIL: see VX2ASYNC.TXT; nothing was changed");
 else finish("PASS (.533: nothing written: the driver reads no ACCEL_ASYNC; asynchronous blits are always on)");
 CloseHandle(dev); fclose(logfile);
 return fails?1:0;
}
