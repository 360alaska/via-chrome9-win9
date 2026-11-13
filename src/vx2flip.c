/* VX2FLIP v1.0: DirectDraw page flipping, vertical blank, stretch, mirror and system-memory blits. Writes VX2FLIP.TXT.
 * Part A needs no screen change: stretch/mirror/system-memory blits are checked pixel by pixel against a software model.
 * Part B uses a full-screen exclusive flip chain (the screen shows solid colours and a moving bar for about ten seconds)
 * and reads the CRTC start address back through the driver after every flip. Esc aborts. Add "mode" to the command line
 * to also call SetDisplayMode with the current mode before part B. The display start is always put back at the end. */
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
static DWORD depth,sw,sh,aborted,fails;
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
static int flipctl(DWORD cmd,DWORD arg,DWORD flags,DWORD *r) {
 vxf_request q; DWORD got=0,local[VXF_REPLY_DWORDS];
 if(!r) r=local;
 q.cb=sizeof(q); q.cmd=cmd; q.arg=arg; q.flags=flags;
 return DeviceIoControl(dev,VX_IOCTL_FLIP,&q,sizeof(q),r,4*VXF_REPLY_DWORDS,&got,NULL)&&got==4*VXF_REPLY_DWORDS;
}
static DWORD crtstart(void) { /* what the CRTC is really programmed to, read independently of the flip code */
 static vx_crt_report rep; DWORD got=0;
 memset(&rep,0,sizeof(rep));
 if(!DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,&rep,sizeof(rep),&got,NULL)||got!=sizeof(rep)) return 0xFFFFFFFFUL;
 return ((rep.cr[0x48]&3UL)<<24)|((rep.cr[0x34]&0xFFUL)<<16)|((rep.cr[0x0C]&0xFFUL)<<8)|(rep.cr[0x0D]&0xFFUL);
}
static void show(const char *what,const DWORD *r) {
 logline("%s: result %lu state %lu (0 untested,1 direct CRTC,2 BIOS per flip,3 none) shift %lu vblank %lu (1 seen,2 never) display %lu flips %lu BIOS calls %lu reason %lu\n",what,r[0],r[1],r[2],r[3],r[4],r[5],r[6],r[7]);
 logline("   calibration: test line %lu gave CRTC value %lu; registers changed by the BIOS call %lu; three frames %lu ms; timeouts %lu; last request %lu; last value %lu\n",r[9],r[10],r[11],r[12],r[13],r[14],r[15]);
}
/* ---- pattern helpers ---- */
static DWORD pix(int x,int y,DWORD key) {
 if(((x+2*y)%5)==0) return key;
 if(depth==16) return (DWORD)((((x*5+y)&31)<<11)|(((x+y*3)&63)<<5)|((x*7+y*2)&31));
 return (DWORD)((((x*3+y)&255)<<16)|(((x+y*5)&255)<<8)|((x*7+y*2)&255));
}
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
static LPDIRECTDRAWSURFACE mk(int w,int h,DWORD caps) {
 DDSURFACEDESC s; LPDIRECTDRAWSURFACE r=NULL; HRESULT hr;
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; s.dwWidth=w; s.dwHeight=h; s.ddsCaps.dwCaps=caps;
 hr=IDirectDraw_CreateSurface(dd,&s,&r,NULL);
 if(FAILED(hr)) {logline("  CreateSurface %dx%d caps %08lX failed: %08lX\n",w,h,caps,(DWORD)hr); return NULL;}
 return r;
}
static void fillsurf(LPDIRECTDRAWSURFACE s,int w,int h,DWORD key) {
 DDSURFACEDESC d; int x,y;
 if(!lock_surf(s,&d)) return;
 for(y=0;y<h;y++) for(x=0;x<w;x++) putpx(&d,x,y,pix(x,y,key)&(depth==16?0xFFFFUL:0xFFFFFFUL));
 IDirectDrawSurface_Unlock(s,d.lpSurface);
}
/* One blit case: src (sw0 x sh0 at 0,0) -> dst rectangle; compares every destination pixel with the model. */
static DWORD srcbuf[96*96];
static int blitcase(const char *name,LPDIRECTDRAWSURFACE src,int sw0,int sh0,LPDIRECTDRAWSURFACE dst,int dw0,int dh0,RECT sr,RECT dr,DWORD flags,DWORD fx,DWORD key) {
 DDSURFACEDESC d; DDBLTFX bf; HRESULT hr; int x,y,bad=0,sx,sy,dw=dr.right-dr.left,dh=dr.bottom-dr.top,w=sr.right-sr.left,h=sr.bottom-sr.top;
 static DWORD before[160*120];
 double t0,t1;
 DWORD m=depth==16?0xFFFFUL:0xFFFFFFUL;
 if(!src||!dst) {logline("%s: surfaces unavailable\n",name); fails++; return 0;}
 fillsurf(src,sw0,sh0,key);
 if(!lock_surf(dst,&d)) {logline("%s: lock failed\n",name); fails++; return 0;}
 for(y=0;y<dh0;y++) for(x=0;x<dw0;x++) {DWORD v=(DWORD)(((x^y)&1)?0x4729:0x1234)&m; if(flags&DDBLT_KEYDESTOVERRIDE) v=(((x+y)&3)==0?key:0x2468)&m; putpx(&d,x,y,v); before[y*dw0+x]=v;}
 IDirectDrawSurface_Unlock(dst,d.lpSurface);
 for(y=0;y<sh0;y++) for(x=0;x<sw0;x++) srcbuf[y*sw0+x]=pix(x,y,key)&m;
 memset(&bf,0,sizeof(bf)); bf.dwSize=sizeof(bf); bf.dwDDFX=fx; bf.ddckSrcColorkey.dwColorSpaceLowValue=bf.ddckSrcColorkey.dwColorSpaceHighValue=key; bf.ddckDestColorkey=bf.ddckSrcColorkey;
 t0=now(); hr=IDirectDrawSurface_Blt(dst,&dr,src,&sr,DDBLT_WAIT|flags|(fx?DDBLT_DDFX:0),&bf); t1=now();
 if(FAILED(hr)) {logline("%s: Blt failed %08lX\n",name,(DWORD)hr); fails++; return 0;}
 if(!lock_surf(dst,&d)) {fails++; return 0;}
 for(y=0;y<dh0;y++) for(x=0;x<dw0;x++) {
  DWORD want=before[y*dw0+x],got=getpx(&d,x,y);
  if(x>=dr.left&&x<dr.right&&y>=dr.top&&y<dr.bottom) {
   sx=((2*(x-dr.left)+1)*w)/(2*dw); sy=((2*(y-dr.top)+1)*h)/(2*dh);
   if(fx&DDBLTFX_MIRRORLEFTRIGHT) sx=w-1-sx; if(fx&DDBLTFX_MIRRORUPDOWN) sy=h-1-sy;
   {DWORD v=srcbuf[(sr.top+sy)*sw0+sr.left+sx]; int skip=0;
    if((flags&DDBLT_KEYSRCOVERRIDE)&&(v&m)==(key&m)) skip=1;
    if((flags&DDBLT_KEYDESTOVERRIDE)&&(want&m)!=(key&m)) skip=1;
    if(!skip) want=v;}
  }
  if((got&m)!=(want&m)) bad++;
 }
 IDirectDrawSurface_Unlock(dst,d.lpSurface);
 logline("%s: %s (%d mismatches of %d) %.2f ms\n",name,bad?"FAIL":"PASS",bad,dw0*dh0,t1-t0);
 if(bad) fails++;
 return !bad;
}
static void partA(void) {
 LPDIRECTDRAWSURFACE v1,v2,s1,s2; RECT a,b; DWORD K=depth==16?0x07E0:0x00FF00;
 logline("\nPART A: blit features against a software model (point sampling at pixel centres)\n");
 v1=mk(96,96,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY); v2=mk(160,120,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY);
 s1=mk(96,96,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY); s2=mk(160,120,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY);
 if(!v1||!v2||!s1||!s2) {logline("could not create the test surfaces\n"); fails++; goto out;}
 SetRect(&a,0,0,32,24); SetRect(&b,10,7,106,79); blitcase("video to video stretch 32x24 -> 96x72",v1,96,96,v2,160,120,a,b,0,0,K);
 SetRect(&a,0,0,64,48); SetRect(&b,5,3,25,18); blitcase("video to video shrink 64x48 -> 20x15",v1,96,96,v2,160,120,a,b,0,0,K);
 SetRect(&a,3,5,40,36); SetRect(&b,20,20,81,71); blitcase("video to video odd-ratio stretch 37x31 -> 61x51",v1,96,96,v2,160,120,a,b,0,0,K);
 SetRect(&a,2,2,42,32); SetRect(&b,50,30,90,60); blitcase("mirror left-right 40x30",v1,96,96,v2,160,120,a,b,0,DDBLTFX_MIRRORLEFTRIGHT,K);
 blitcase("mirror up-down 40x30",v1,96,96,v2,160,120,a,b,0,DDBLTFX_MIRRORUPDOWN,K);
 blitcase("mirror both 40x30",v1,96,96,v2,160,120,a,b,0,DDBLTFX_MIRRORLEFTRIGHT|DDBLTFX_MIRRORUPDOWN,K);
 SetRect(&b,50,30,110,75); blitcase("stretch + mirror + source colour key",v1,96,96,v2,160,120,a,b,DDBLT_KEYSRCOVERRIDE,DDBLTFX_MIRRORLEFTRIGHT,K);
 blitcase("stretch + destination colour key",v1,96,96,v2,160,120,a,b,DDBLT_KEYDESTOVERRIDE,0,K);
 SetRect(&a,0,0,50,40); SetRect(&b,30,20,80,60); blitcase("video to video same-size copy (engine)",v1,96,96,v2,160,120,a,b,0,0,K);
 SetRect(&a,1,2,51,42); SetRect(&b,30,20,80,60); blitcase("system to video copy 50x40",s1,96,96,v2,160,120,a,b,0,0,K);
 blitcase("system to video copy with source key",s1,96,96,v2,160,120,a,b,DDBLT_KEYSRCOVERRIDE,0,K);
 SetRect(&a,0,0,48,36); SetRect(&b,10,10,106,82); blitcase("system to video stretch 48x36 -> 96x72",s1,96,96,v2,160,120,a,b,0,0,K);
 SetRect(&a,1,2,51,42); SetRect(&b,30,20,80,60); blitcase("video to system copy 50x40",v1,96,96,s2,160,120,a,b,0,0,K);
 SetRect(&a,0,0,48,36); SetRect(&b,10,10,106,82); blitcase("video to system stretch 48x36 -> 96x72",v1,96,96,s2,160,120,a,b,0,0,K);
 SetRect(&a,0,0,96,96); SetRect(&b,0,0,160,120); blitcase("system to system stretch (DirectDraw's own code)",s1,96,96,s2,160,120,a,b,0,0,K);
 {/* timing: a larger stretch, the way a game scales a 320x200 frame */
  LPDIRECTDRAWSURFACE big=mk(320,200,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY),dst=mk(640,400,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY),vsrc=mk(320,200,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY);
  RECT r1,r2; double t0,t1; int i;
  if(big&&dst&&vsrc) {
   SetRect(&r1,0,0,320,200); SetRect(&r2,0,0,640,400);
   t0=now(); for(i=0;i<10;i++) IDirectDrawSurface_Blt(dst,&r2,big,&r1,DDBLT_WAIT,NULL); t1=now(); logline("timing: system 320x200 -> video 640x400 stretch: %.2f ms each\n",(t1-t0)/10);
   t0=now(); for(i=0;i<10;i++) IDirectDrawSurface_Blt(dst,&r2,vsrc,&r1,DDBLT_WAIT,NULL); t1=now(); logline("timing: video 320x200 -> video 640x400 stretch: %.2f ms each\n",(t1-t0)/10);
   t0=now(); for(i=0;i<10;i++) IDirectDrawSurface_Blt(dst,&r1,big,&r1,DDBLT_WAIT,NULL); t1=now(); logline("timing: system 320x200 -> video 1:1 copy: %.2f ms each\n",(t1-t0)/10);
  }
  if(big) IDirectDrawSurface_Release(big); if(dst) IDirectDrawSurface_Release(dst); if(vsrc) IDirectDrawSurface_Release(vsrc);
 }
out:
 if(v1) IDirectDrawSurface_Release(v1); if(v2) IDirectDrawSurface_Release(v2); if(s1) IDirectDrawSurface_Release(s1); if(s2) IDirectDrawSurface_Release(s2);
}
/* ---- part B ---- */
static void colorfill(LPDIRECTDRAWSURFACE s,DWORD c,RECT *r) {
 DDBLTFX f; memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=c;
 IDirectDrawSurface_Blt(s,r,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f);
}
static DWORD rgb(int r,int g,int b) {return depth==16?(DWORD)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)):(DWORD)((r<<16)|(g<<8)|b);}
static DWORD surfoffset(LPDIRECTDRAWSURFACE s,BYTE *base) { /* offset of a surface from the primary's original address */
 DDSURFACEDESC d; DWORD o=0xFFFFFFFFUL;
 if(lock_surf(s,&d)) {o=(DWORD)((BYTE*)d.lpSurface-base); IDirectDrawSurface_Unlock(s,d.lpSurface);}
 return o;
}
static void partB(int setmode) {
 DDSURFACEDESC s; DDSCAPS caps; LPDIRECTDRAWSURFACE prim=NULL,back=NULL; HRESULT hr; DWORD r[VXF_REPLY_DWORDS],shift,i,bad=0,seen=0; BYTE *base0=NULL;
 DWORD colors[3]; double t0,t1; int n,backs=2;
 logline("\nPART B: full-screen flip chain (screen changes colour; Esc aborts)\n");
 if(!flipctl(VXF_QUERY,0,0,r)) {logline("flip ioctl failed (VX85598.VXD older than v10.59?)\n"); fails++; return;}
 show("driver flip state",r); shift=r[VXF_R_SHIFT];
 if(r[VXF_R_RESULT]!=VXF_OK) {logline("RESULT: the driver cannot flip on this hardware (reason %lu). DirectDraw applications will use blits instead.\n",r[VXF_R_REASON]); return;}
 {DDCAPS c; memset(&c,0,sizeof(c)); c.dwSize=sizeof(c); IDirectDraw_GetCaps(dd,&c,NULL); logline("DirectDraw surface caps %08lX: FLIP %s, BACKBUFFER %s; caps %08lX\n",c.ddsCaps.dwCaps,(c.ddsCaps.dwCaps&DDSCAPS_FLIP)?"yes":"NO",(c.ddsCaps.dwCaps&DDSCAPS_BACKBUFFER)?"yes":"NO",c.dwCaps);}
 hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN); logline("SetCooperativeLevel(exclusive, fullscreen): %08lX\n",(DWORD)hr); if(FAILED(hr)) {fails++; return;}
 if(setmode) {logline("step: SetDisplayMode %lux%lux%lu\n",sw,sh,depth); hr=IDirectDraw_SetDisplayMode(dd,sw,sh,depth); logline("SetDisplayMode: %08lX\n",(DWORD)hr); if(FAILED(hr)) {fails++; goto done;}}
 for(;backs>=1;backs--) {
  memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; s.dwBackBufferCount=backs;
  s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
  logline("step: creating primary with %d back buffer(s)\n",backs);
  hr=IDirectDraw_CreateSurface(dd,&s,&prim,NULL); logline("CreateSurface(primary flip chain, %d back): %08lX\n",backs,(DWORD)hr);
  if(SUCCEEDED(hr)) break; prim=NULL;
 }
 if(!prim) {logline("RESULT: the flip chain could not be created\n"); fails++; goto done;}
 memset(&caps,0,sizeof(caps)); caps.dwCaps=DDSCAPS_BACKBUFFER;
 hr=IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back); logline("GetAttachedSurface(back buffer): %08lX\n",(DWORD)hr); if(FAILED(hr)) {fails++; goto done;}
 {DDSURFACEDESC d; if(!lock_surf(prim,&d)) {logline("primary lock failed\n"); fails++; goto done;} base0=(BYTE*)d.lpSurface; logline("primary at %08lX pitch %ld; display start register %lu\n",(DWORD)base0,(long)d.lPitch,crtstart()); IDirectDrawSurface_Unlock(prim,d.lpSurface);}
 logline("back buffer offset from primary: %lu bytes\n",surfoffset(back,base0));
 colors[0]=rgb(220,40,40); colors[1]=rgb(40,200,60); colors[2]=rgb(50,70,230);
 for(i=0;i<9&&!aborted;i++) {
  RECT mark; DWORD want,start,disp; int ok;
  colorfill(back,colors[i%3],NULL); SetRect(&mark,40,40,40+60*(int)(i%3+1),100); colorfill(back,rgb(255,255,255),&mark);
  hr=IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT);
  if(FAILED(hr)) {logline("flip %lu: Flip failed %08lX\n",i,(DWORD)hr); bad++; break;}
  disp=surfoffset(prim,base0); start=crtstart(); want=disp>>shift;
  ok=(r[VXF_R_STATE]==VXF_ST_DIRECT)?(start==want):1; if(!ok) bad++; else seen++;
  logline("flip %lu: primary now at offset %lu; CRTC start %lu; expected %lu: %s\n",i,disp,start,want,ok?"ok":"MISMATCH");
  {DDSURFACEDESC d; if(lock_surf(prim,&d)) {DWORD px=getpx(&d,10,10); IDirectDrawSurface_Unlock(prim,d.lpSurface); if((px&(depth==16?0xFFFFUL:0xFFFFFFUL))!=colors[i%3]) {logline("   primary pixel is %08lX, wanted %08lX\n",px,colors[i%3]); bad++;}}}
  IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back);
  {DWORD end=GetTickCount()+900; while(GetTickCount()<end&&!aborted) {pump(); Sleep(20);}}
 }
 logline("flip verification: %s (%lu flips matched, %lu problems)\n",bad?"FAIL":"PASS",seen,bad); if(bad) fails++;
 if(!aborted) {
  /* speed: draw a small rectangle and flip, the way a game does */
  RECT bar; int frame;
  t0=now(); for(n=0;n<120&&!aborted;n++) {IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back); SetRect(&bar,(n*7)%(int)sw,100,((n*7)%(int)sw)+40,300); colorfill(back,rgb(0,0,0),NULL); colorfill(back,rgb(255,255,0),&bar); IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT); if((n&15)==15) pump();} t1=now();
  logline("120 flips with DDFLIP_WAIT: %.1f ms each, %.1f flips per second\n",(t1-t0)/120,120000.0/(t1-t0));
  t0=now(); for(n=0;n<120&&!aborted;n++) {IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back); SetRect(&bar,(n*7)%(int)sw,100,((n*7)%(int)sw)+40,300); colorfill(back,rgb(0,0,0),NULL); colorfill(back,rgb(255,255,0),&bar); hr=IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT|DDFLIP_NOVSYNC); if(FAILED(hr)) {logline("DDFLIP_NOVSYNC: %08lX\n",(DWORD)hr); break;} if((n&15)==15) pump();} t1=now();
  logline("120 flips with DDFLIP_NOVSYNC: %.1f ms each, %.1f flips per second\n",(t1-t0)/120,120000.0/(t1-t0));
  t0=now(); for(n=0;n<60&&!aborted;n++) {IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back); colorfill(back,rgb(0,60,0),NULL); hr=IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT|DDFLIP_INTERVAL2); if(FAILED(hr)) {logline("DDFLIP_INTERVAL2: %08lX\n",(DWORD)hr); break;} if((n&15)==15) pump();} t1=now();
  logline("60 flips with DDFLIP_INTERVAL2: %.1f ms each, %.1f flips per second (about 30 expected)\n",(t1-t0)/60,60000.0/(t1-t0));
  /* animation to look at: a bar sweeping across at the refresh rate */
  t0=now(); frame=0; while(now()-t0<4000&&!aborted) {
   IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back); SetRect(&bar,(frame*9)%(int)(sw-60),(int)sh/3,(frame*9)%(int)(sw-60)+60,(int)sh*2/3);
   colorfill(back,rgb(0,0,60),NULL); colorfill(back,rgb(255,255,255),&bar); IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT); frame++; if((frame&7)==0) pump();}
  logline("sweeping bar: %d frames in 4 s (%.1f per second)\n",frame,frame*1000.0/(now()-t0));
 }
 {DWORD o,got=0; HRESULT h2=IDirectDraw_FlipToGDISurface(dd); logline("FlipToGDISurface: %08lX\n",(DWORD)h2); (void)o; (void)got;}
done:
 if(back) {IDirectDrawSurface_Release(back);} /* attached surfaces are released with the primary */
 if(prim) IDirectDrawSurface_Release(prim);
 IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL);
 {DWORD st=crtstart(); logline("after leaving: display start register %lu (should be 0): %s\n",st,st==0?"PASS":"FAIL"); if(st!=0) {fails++; flipctl(VXF_RESET,0,0,NULL); logline("display start reset by the driver; register now %lu\n",crtstart());}}
 flipctl(VXF_INFO,0,0,r); show("driver flip state at the end",r);
}
static void vblank(void) {
 DWORD r[VXF_REPLY_DWORDS]; int i; double t0,t1,acc=0,mn=1e9,mx=0; DWORD line=0; BOOL inv=0; HRESULT hr;
 logline("\nVERTICAL BLANK\n");
 if(!flipctl(VXF_INFO,0,0,r)) {logline("flip ioctl unavailable\n"); return;}
 show("driver flip state before any DirectDraw",r);
 hr=IDirectDraw_GetScanLine(dd,&line); logline("GetScanLine: %08lX line %lu (unsupported is expected)\n",(DWORD)hr,line);
 hr=IDirectDraw_GetVerticalBlankStatus(dd,&inv); logline("GetVerticalBlankStatus: %08lX in blank %d\n",(DWORD)hr,(int)inv);
 hr=IDirectDraw_WaitForVerticalBlank(dd,DDWAITVB_BLOCKBEGIN,NULL); logline("WaitForVerticalBlank(begin): %08lX\n",(DWORD)hr);
 if(FAILED(hr)) return;
 t0=now(); for(i=0;i<60;i++) {double a=now(); IDirectDraw_WaitForVerticalBlank(dd,DDWAITVB_BLOCKBEGIN,NULL); t1=now(); if(i) {acc+=t1-a; if(t1-a<mn) mn=t1-a; if(t1-a>mx) mx=t1-a;}}
 logline("60 WaitForVerticalBlank calls: average %.2f ms (%.1f Hz), shortest %.2f, longest %.2f\n",acc/59,59000.0/acc,mn,mx);
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show_) {
 WNDCLASS wc; vx_status_t s; DWORD got,r[VXF_REPLY_DWORDS]; HMODULE mod; HRESULT hr; char text[240]; int setmode=cmd&&strstr(cmd,"mode");
 typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*);
 (void)prev;(void)show_;
 logfile=fopen("VX2FLIP.TXT","w"); if(!logfile) return 1;
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||(s.bpp!=16&&s.bpp!=32)) {logline("The VX85598 driver is not active.\n"); goto cleanup;}
 depth=s.bpp; sw=s.width; sh=s.height; QueryPerformanceFrequency(&freq);
 logline("VX2FLIP v1.0 / driver v10.61\nDesktop %lux%lux%lu pitch %lu%s\n",sw,sh,depth,s.video_pitch,setmode?"; SetDisplayMode will be tried":"");
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2FLIP"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClass(&wc);
 win=CreateWindowEx(WS_EX_TOPMOST,wc.lpszClassName,"VX2FLIP - Esc aborts",WS_POPUP|WS_VISIBLE,0,0,(int)sw,(int)sh,NULL,NULL,hi,NULL);
 if(!win) goto cleanup;
 mod=LoadLibraryA("DDRAW.DLL"); if(!mod) goto cleanup;
 {CREATE create=(CREATE)GetProcAddress(mod,"DirectDrawCreate"); if(!create) goto cleanup; hr=create(NULL,&dd,NULL); logline("DirectDrawCreate: %08lX\n",(DWORD)hr); if(FAILED(hr)) {dd=NULL; goto cleanup;}}
 if(FAILED(IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL))) goto cleanup;
 {DDCAPS c; memset(&c,0,sizeof(c)); c.dwSize=sizeof(c); IDirectDraw_GetCaps(dd,&c,NULL); logline("DirectDraw hardware HAL active: %s; caps %08lX surface caps %08lX\n",(c.dwCaps&0x02000000UL)?"NO":"yes",c.dwCaps,c.ddsCaps.dwCaps);}
 vblank();
 if(!aborted) partA();
 if(!aborted) partB(setmode);
 logline("\nSUMMARY: %s (%lu failed checks)\n",fails?"FAILURES":"all checks passed",fails);
cleanup:
 if(dev!=INVALID_HANDLE_VALUE) {flipctl(VXF_RESET,0,0,r); flipctl(VXF_INFO,0,0,r); show("final driver flip state",r);}
 if(dd) IDirectDraw_Release(dd);
 if(win) DestroyWindow(win);
 if(dev!=INVALID_HANDLE_VALUE) CloseHandle(dev);
 logline("Finished%s.\n",aborted?" (aborted)":""); fclose(logfile);
 sprintf(text,"%s Read VX2FLIP.TXT. Did the screen change colour red, green, blue during part B, and did the sweeping bar move smoothly?",fails?"Some checks failed.":"Checks finished.");
 MessageBox(NULL,text,"VX2FLIP",MB_OK);
 return fails?1:0;
}
