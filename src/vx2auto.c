/* VX2AUTO v2.7 (v2.7: a 1-bit bitmap brush with a transparent background is expected on the engine, and the first case of kinds 15 and 16 logs what DIBENG does with the 0 and 1 bits): the unattended version of VX2MODE (run it, walk away, read VX2AUTO.TXT or the one-line VX2AUTO.RES).
 * Nobody has to look at the screen or click anything:
 *  1. Every DirectDraw mode VX2MODE tests (640x480, 800x600, 1024x768 and the small game modes at 8, 16 and 32 bits per pixel): mode switch,
 *     driver status, blits and GDI drawing checked pixel by pixel, flip chain. At 8 bits per pixel the palette is proved by reading the DAC back
 *     (driver request VX_IOCTL_DAC, v10.71): DirectDraw palette attach, SetEntries, detach and re-attach.
 *  2. The desktop phase: 640x480, 800x600 and 1024x768 at the desktop depth, the other of 16/32, and 8 bits, switched the way Display Properties
 *     does it (ChangeDisplaySettings, no restart). In every mode: driver status, GetDeviceCaps, solid fills, a hatch pattern inverted twice,
 *     overlapping screen copies in both directions, the software cursor, and at 8 bits the system palette against the DAC.
 * Also (v10.73): video memory capacity (offscreen surfaces filled with distinct colours and read back, proves memory beyond 16 MB), the GDI binding of the driver
 * (a switched-off GDI acceleration is a failure), 2D engine operations during the GDI tests, and a scan line counter sanity scan (information only).
 * v1.4: brush fills (v10.77): in every desktop mode, DIB pattern brushes of 8 colours and of 2 colours, a dithered solid colour and a hatch are drawn with PatBlt
 * (PATCOPY and PATINVERT, four brush origins, odd positions and sizes of at least 16384 pixels so they reach the engine) on the screen and on a screen-format
 * memory bitmap (drawn by the DIB engine in software); the raw bits of both are compared, and the engine operations are counted.
 * v1.9: ExtTextOut with an opaque rectangle (16 cases: opaque and transparent mode, ETO_CLIPPED or not, a clip region or not, two rectangles) against the software reference (driver v10.83: the rectangle is an engine fill); brushes made from a 1-bit bitmap, opaque (expected to reach the engine at every depth, 8bpp since v10.83) and transparent (information only).
 * v1.8: (driver v10.82) the transparent hatches are expected to reach the engine and to equal the software reference, which paints the background colour whatever the mode.
 * v1.7: all six hatch styles are also drawn with a transparent background (v10.80: the PATCOPY half is expected to reach the engine at 16 and 32 bits per pixel).
 * v1.6: the brush fills (v10.79) now cover the six hatch styles opaque (background white, and yellow for the clipped rectangle) and DIAGCROSS transparent, with a clipped rectangle; the opaque hatches are expected to reach the engine, the transparent one stays in software.
 * v1.5: hatch capture (v10.78, 640x480 desktop modes only): the six hatch styles, opaque (two background colours, one brush reused) and transparent, three brush origins,
 * with distinct hatch, text, background and destination colours. For each fill the driver's capture (ExtEscape 8F72) returns what DIBENG handed over (flags, style,
 * mono and mask rows, brush pixels, draw mode), and the log shows the 8x8 tile the software reference drew. Nothing is accelerated yet; this is measurement.
 * The desktop mode is put back at the end. Every step is written to the log BEFORE it runs. Esc aborts between steps.
 * Command line: "list" = only list the modes; "nodesktop" = skip the desktop phase; "640x480x16 ..." = only those DirectDraw modes. */
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
#include "vx_hatch_diag.h"
static HANDLE dev=INVALID_HANDLE_VALUE;
static FILE *logfile;
static HWND win;
static LPDIRECTDRAW dd;
static DWORD depth,sw,sh,aborted,fails;
static LARGE_INTEGER freq;
typedef struct {DWORD w,h,b,hz;} modeinfo;
static modeinfo offered[256]; static int noffered;
typedef struct {DWORD w,h,b; int tried,setok,native,fails,flip;} result; /* flip: 0 not tried, 1 pass, 2 fail, 3 not offered */
static result results[96]; static int nresults;
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
static void wait_ms(DWORD ms) {DWORD end=GetTickCount()+ms; while(GetTickCount()<end&&!aborted) {pump(); Sleep(20);}}
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
static DWORD crtstart(void) {
 static vx_crt_report rep; DWORD got=0;
 memset(&rep,0,sizeof(rep));
 if(!DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,&rep,sizeof(rep),&got,NULL)||got!=sizeof(rep)) return 0xFFFFFFFFUL;
 return ((rep.cr[0x48]&3UL)<<24)|((rep.cr[0x34]&0xFFUL)<<16)|((rep.cr[0x0C]&0xFFUL)<<8)|(rep.cr[0x0D]&0xFFUL);
}
/* what the CRTC is programmed to right now: start address, offset (pitch) registers, and the driver's own idea of the display start */
static void scan(const char *where) {
 static vx_crt_report rep; DWORD got=0,r[VXF_REPLY_DWORDS];
 memset(&rep,0,sizeof(rep));
 if(!DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,&rep,sizeof(rep),&got,NULL)||got!=sizeof(rep)) {logline("  scanout (%s): CRT ioctl failed\n",where); return;}
 logline("  scanout (%s): CR0C %02lX CR0D %02lX CR34 %02lX CR48 %02lX -> start %lu; CR13 %02lX CR35 %02lX CR36 %02lX; CR07 %02lX CR09 %02lX",where,
  rep.cr[0x0C]&0xFFUL,rep.cr[0x0D]&0xFFUL,rep.cr[0x34]&0xFFUL,rep.cr[0x48]&0xFFUL,crtstart(),rep.cr[0x13]&0xFFUL,rep.cr[0x35]&0xFFUL,rep.cr[0x36]&0xFFUL,rep.cr[0x07]&0xFFUL,rep.cr[0x09]&0xFFUL);
 if(flipctl(VXF_INFO,0,0,r)) logline("; driver display offset %lu, flips %lu\n",r[VXF_R_DISPLAY],r[VXF_R_FLIPS]); else logline("\n");
}
/* ---- pixel helpers for 8, 16 and 32 bits per pixel ---- */
static DWORD maskv(void) {return depth==8?0xFFUL:depth==16?0xFFFFUL:0xFFFFFFUL;}
static DWORD pix(int x,int y,DWORD key) {
 if(((x+2*y)%5)==0) return key;
 if(depth==8) return (DWORD)((x*3+y*5)&255);
 if(depth==16) return (DWORD)((((x*5+y)&31)<<11)|(((x+y*3)&63)<<5)|((x*7+y*2)&31));
 return (DWORD)((((x*3+y)&255)<<16)|(((x+y*5)&255)<<8)|((x*7+y*2)&255));
}
static DWORD keycolor(void) {return depth==8?0x1CUL:depth==16?0x07E0UL:0x00FF00UL;}
static DWORD rgb(int r,int g,int b) {
 if(depth==8) return (DWORD)(((r>>5)<<5)|((g>>5)<<2)|(b>>6));
 return depth==16?(DWORD)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)):(DWORD)((r<<16)|(g<<8)|b);
}
static int lock_surf(LPDIRECTDRAWSURFACE s,DDSURFACEDESC *d) {
 memset(d,0,sizeof(*d)); d->dwSize=sizeof(*d);
 return SUCCEEDED(IDirectDrawSurface_Lock(s,NULL,d,DDLOCK_WAIT,NULL));
}
static DWORD getpx(const DDSURFACEDESC *d,int x,int y) {
 BYTE *p=(BYTE*)d->lpSurface+(DWORD)y*d->lPitch;
 return depth==8?(DWORD)p[x]:depth==16?(DWORD)((WORD*)p)[x]:(((DWORD*)p)[x]&0xFFFFFFUL);
}
static void putpx(const DDSURFACEDESC *d,int x,int y,DWORD v) {
 BYTE *p=(BYTE*)d->lpSurface+(DWORD)y*d->lPitch;
 if(depth==8) p[x]=(BYTE)v; else if(depth==16) ((WORD*)p)[x]=(WORD)v; else ((DWORD*)p)[x]=v;
}
static int readreg(LPDIRECTDRAWSURFACE s,int x0,int y0,int w,int h,DWORD *out) {
 DDSURFACEDESC d; int x,y;
 if(!lock_surf(s,&d)) return 0;
 for(y=0;y<h;y++) for(x=0;x<w;x++) out[y*w+x]=getpx(&d,x0+x,y0+y);
 IDirectDrawSurface_Unlock(s,d.lpSurface); return 1;
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
 for(y=0;y<h;y++) for(x=0;x<w;x++) putpx(&d,x,y,pix(x,y,key)&maskv());
 IDirectDrawSurface_Unlock(s,d.lpSurface);
}
static void colorfill(LPDIRECTDRAWSURFACE s,DWORD c,RECT *r) {
 DDBLTFX f; memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=c;
 IDirectDrawSurface_Blt(s,r,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f);
}
static LPDIRECTDRAWPALETTE pal; /* 3-3-2 palette so rgb() works at 8 bits per pixel */
static void setpal(LPDIRECTDRAWSURFACE prim) {
 PALETTEENTRY e[256]; int i; HRESULT hr;
 if(depth!=8) return;
 for(i=0;i<256;i++) {e[i].peRed=(BYTE)(((i>>5)&7)*255/7); e[i].peGreen=(BYTE)(((i>>2)&7)*255/7); e[i].peBlue=(BYTE)((i&3)*255/3); e[i].peFlags=0;}
 if(pal) {IDirectDrawPalette_Release(pal); pal=NULL;}
 hr=IDirectDraw_CreatePalette(dd,DDPCAPS_8BIT|DDPCAPS_ALLOW256,e,&pal,NULL);
 if(FAILED(hr)) {logline("  CreatePalette: %08lX\n",(DWORD)hr); fails++; pal=NULL; return;}
 hr=IDirectDrawSurface_SetPalette(prim,pal); if(FAILED(hr)) {logline("  SetPalette: %08lX\n",(DWORD)hr); fails++;}
}
/* ---- DirectDraw blit checks (same model as VX2FLIP) ---- */
static DWORD srcbuf[96*96];
static int blitcase(const char *name,LPDIRECTDRAWSURFACE src,int sw0,int sh0,LPDIRECTDRAWSURFACE dst,int dw0,int dh0,RECT sr,RECT dr,DWORD flags,DWORD fx,DWORD key) {
 DDSURFACEDESC d; DDBLTFX bf; HRESULT hr; int x,y,bad=0,sx,sy,dw=dr.right-dr.left,dh=dr.bottom-dr.top,w=sr.right-sr.left,h=sr.bottom-sr.top,fx0=-1,fy0=-1;
 static DWORD before[160*120];
 double t0,t1; DWORD m=maskv();
 if(!src||!dst) {logline("  %s: surfaces unavailable\n",name); fails++; return 0;}
 fillsurf(src,sw0,sh0,key);
 if(!lock_surf(dst,&d)) {logline("  %s: lock failed\n",name); fails++; return 0;}
 for(y=0;y<dh0;y++) for(x=0;x<dw0;x++) {DWORD v=(DWORD)(((x^y)&1)?0x4729:0x1234)&m; if(flags&DDBLT_KEYDESTOVERRIDE) v=(((x+y)&3)==0?key:0x2468)&m; putpx(&d,x,y,v); before[y*dw0+x]=v;}
 IDirectDrawSurface_Unlock(dst,d.lpSurface);
 for(y=0;y<sh0;y++) for(x=0;x<sw0;x++) srcbuf[y*sw0+x]=pix(x,y,key)&m;
 memset(&bf,0,sizeof(bf)); bf.dwSize=sizeof(bf); bf.dwDDFX=fx; bf.ddckSrcColorkey.dwColorSpaceLowValue=bf.ddckSrcColorkey.dwColorSpaceHighValue=key; bf.ddckDestColorkey=bf.ddckSrcColorkey;
 t0=now(); hr=IDirectDrawSurface_Blt(dst,&dr,src,&sr,DDBLT_WAIT|flags|(fx?DDBLT_DDFX:0),&bf); t1=now();
 if(FAILED(hr)) {logline("  %s: Blt failed %08lX\n",name,(DWORD)hr); fails++; return 0;}
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
  if((got&m)!=(want&m)) {if(!bad) {fx0=x; fy0=y;} bad++;}
 }
 IDirectDrawSurface_Unlock(dst,d.lpSurface);
 if(bad) logline("  %s: FAIL (%d mismatches of %d, first at %d,%d) %.2f ms\n",name,bad,dw0*dh0,fx0,fy0,t1-t0); else logline("  %s: PASS %.2f ms\n",name,t1-t0);
 if(bad) fails++;
 return !bad;
}
static void blits(LPDIRECTDRAWSURFACE prim) {
 LPDIRECTDRAWSURFACE v1,v2,s1,s2; RECT a,b; DWORD K=keycolor();
 logline("step: DirectDraw blits at %lu bits per pixel\n",depth);
 v1=mk(96,96,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY); v2=mk(160,120,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY);
 s1=mk(96,96,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY); s2=mk(160,120,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_SYSTEMMEMORY);
 if(!v1||!v2||!s1||!s2) {logline("  could not create the test surfaces\n"); fails++; goto out;}
 SetRect(&a,0,0,50,40); SetRect(&b,30,20,80,60);
 blitcase("video to video copy 50x40",v1,96,96,v2,160,120,a,b,0,0,K);
 blitcase("system to video copy 50x40",s1,96,96,v2,160,120,a,b,0,0,K);
 blitcase("system to video copy with source key",s1,96,96,v2,160,120,a,b,DDBLT_KEYSRCOVERRIDE,0,K);
 blitcase("video to system copy 50x40",v1,96,96,s2,160,120,a,b,0,0,K);
 SetRect(&a,0,0,32,24); SetRect(&b,10,7,106,79); blitcase("video to video stretch 32x24 -> 96x72",v1,96,96,v2,160,120,a,b,0,0,K);
 SetRect(&a,2,2,42,32); SetRect(&b,50,30,90,60); blitcase("mirror left-right 40x30",v1,96,96,v2,160,120,a,b,0,DDBLTFX_MIRRORLEFTRIGHT,K);
 blitcase("stretch + destination colour key",v1,96,96,v2,160,120,a,b,DDBLT_KEYDESTOVERRIDE,0,K);
 /* onto the screen itself */
 SetRect(&a,1,2,51,42); SetRect(&b,30,20,80,60);
 blitcase("system to PRIMARY copy 50x40",s1,96,96,prim,160,120,a,b,0,0,K);
 blitcase("video to PRIMARY copy 50x40",v1,96,96,prim,160,120,a,b,0,0,K);
 SetRect(&a,0,0,32,24); SetRect(&b,10,7,106,79); blitcase("video to PRIMARY stretch",v1,96,96,prim,160,120,a,b,0,0,K);
out:
 if(v1) IDirectDrawSurface_Release(v1); if(v2) IDirectDrawSurface_Release(v2); if(s1) IDirectDrawSurface_Release(s1); if(s2) IDirectDrawSurface_Release(s2);
}

static DWORD rop3c(DWORD r,DWORD p,DWORD s0,DWORD d0) {DWORD n=0; int i; for(i=0;i<8;i++) if((r>>i)&1) n|=((i&4)?p:~p)&((i&2)?s0:~s0)&((i&1)?d0:~d0); return n;}
static int ctrlq(DWORD in,DWORD *out);
/* v2.0: DirectDraw blits with DDBLT_ROP (driver v10.84: source ROPs between video surfaces and BLACKNESS/WHITENESS/DSTINVERT are engine operations; the rest stays with the runtime). The expected pixels are computed here. */
static void ddrop_tests(void) {
 static const DWORD code[10]={0x008800C6UL,0x00EE0086UL,0x00660046UL,0x00330008UL,0x00440328UL,0x001100A6UL,0x00BB0226UL,0x00000042UL,0x00FF0062UL,0x00550009UL};
 static const char *nm[10]={"SRCAND","SRCPAINT","SRCINVERT","NOTSRCCOPY","SRCERASE","NOTSRCERASE","MERGEPAINT","BLACKNESS","WHITENESS","DSTINVERT"};
 LPDIRECTDRAWSURFACE v1,v2; RECT a,bq; DWORD m=maskv(),K=keycolor(),o[4]; int i,x,y,f0=(int)fails;
 static DWORD before[160*120];
 logline("step: DirectDraw ROP blits at %lu bits per pixel\n",depth);
 v1=mk(96,96,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY); v2=mk(160,120,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY);
 if(!v1||!v2) {logline("  could not create the test surfaces\n"); fails++; goto out;}
 SetRect(&a,3,2,63,52); SetRect(&bq,30,20,90,70);
 for(i=0;i<10;i++) {
  DDSURFACEDESC d; DDBLTFX bf; HRESULT hr; int bad=0,fx0=-1,fy0=-1,usesrc=i<7; DWORD ops0=0,ops1=0;
  fillsurf(v1,96,96,K);
  if(!lock_surf(v2,&d)) {logline("  %s: lock failed\n",nm[i]); fails++; continue;}
  for(y=0;y<120;y++) for(x=0;x<160;x++) {DWORD v=(DWORD)(((x*3+y*5)^(x>>2))&0xFFFF)&m; if(depth==32) v=(DWORD)(((x*7+y)&255)<<16|((x+y*3)&255)<<8|((x^y)&255)); putpx(&d,x,y,v); before[y*160+x]=v&m;}
  IDirectDrawSurface_Unlock(v2,d.lpSurface);
  memset(&bf,0,sizeof(bf)); bf.dwSize=sizeof(bf); bf.dwROP=code[i];
  if(ctrlq(0xFFFFFFFEUL,o)) ops0=o[0];
  hr=IDirectDrawSurface_Blt(v2,&bq,usesrc?v1:NULL,usesrc?&a:NULL,DDBLT_WAIT|DDBLT_ROP,&bf);
  if(ctrlq(0xFFFFFFFEUL,o)) ops1=o[0];
  if(FAILED(hr)) {logline("  ROP %s (%08lX): the Blt returned %08lX%s\n",nm[i],code[i],(DWORD)hr,ops1>ops0?": FAIL (the engine had drawn)":" (information: the runtime refused it; the driver sent nothing to the engine)"); if(ops1>ops0) fails++; continue;}
  if(!lock_surf(v2,&d)) {fails++; continue;}
  for(y=0;y<120;y++) for(x=0;x<160;x++) {
   DWORD want=before[y*160+x],got=getpx(&d,x,y);
   if(x>=bq.left&&x<bq.right&&y>=bq.top&&y<bq.bottom) {DWORD s0=usesrc?(pix(a.left+x-bq.left,a.top+y-bq.top,K)&m):0; want=rop3c(code[i]>>16,0,s0,want)&m;}
   if((got&m)!=(want&m)) {if(!bad) {fx0=x; fy0=y;} bad++;}
  }
  IDirectDrawSurface_Unlock(v2,d.lpSurface);
  logline("  DirectDraw ROP %s (%08lX): %s, %s%s\n",nm[i],code[i],bad?"FAIL":"PASS",ops1>ops0?"reached the engine":"drawn without the engine",bad?" (mismatch, first at x,y below)":"");
  if(bad) {logline("    %d mismatches, first at %d,%d\n",bad,fx0,fy0); fails++;}
  if(ops1==ops0) logline("  WARN: DirectDraw ROP %s did not reach the engine (ACCEL_COPY/FILL=0 or below the size threshold; the 60x50 rectangle is 3000 pixels)\n",nm[i]);
 }
 logline("  DirectDraw ROP blits: %s\n",((int)fails==f0)?"PASS":"FAIL");
out:
 if(v1) IDirectDrawSurface_Release(v1); if(v2) IDirectDrawSurface_Release(v2);
}
/* ---- GDI checks on the primary surface's device context; the screen's own pixels are the reference ---- */
#define RW 96
#define RH 72
static DWORD reg0[RW*RH],reg1[RW*RH];
static int cmpreg(const char *name,const DWORD *got,const DWORD *want,int w,int h) {
 int x,y,bad=0,fx=-1,fy=-1;
 for(y=0;y<h;y++) for(x=0;x<w;x++) if((got[y*w+x]&maskv())!=(want[y*w+x]&maskv())) {if(!bad) {fx=x; fy=y;} bad++;}
 if(bad) {logline("  %s: FAIL (%d mismatches of %d, first at %d,%d: got %08lX wanted %08lX)\n",name,bad,w*h,fx,fy,got[fy*w+fx],want[fy*w+fx]); fails++;}
 else logline("  %s: PASS\n",name);
 return !bad;
}
static void gdi_copy(LPDIRECTDRAWSURFACE prim,const char *name,int sx,int sy,int dx,int dy,int w,int h,int ox,int oy) {
 /* region at (ox,oy) RWxRH holds a pattern; copy w x h from (sx,sy) to (dx,dy) (region-relative) with GDI; expect memmove semantics */
 HDC dc; int x,y; static DWORD want[RW*RH];
 if(!readreg(prim,ox,oy,RW,RH,reg0)) {logline("  %s: read failed\n",name); fails++; return;}
 memcpy(want,reg0,sizeof(want));
 for(y=0;y<h;y++) for(x=0;x<w;x++) want[(dy+y)*RW+dx+x]=reg0[(sy+y)*RW+sx+x];
 if(FAILED(IDirectDrawSurface_GetDC(prim,&dc))) {logline("  %s: GetDC failed\n",name); fails++; return;}
 if(!BitBlt(dc,ox+dx,oy+dy,w,h,dc,ox+sx,oy+sy,SRCCOPY)) logline("  %s: BitBlt returned FALSE\n",name);
 IDirectDrawSurface_ReleaseDC(prim,dc);
 if(!readreg(prim,ox,oy,RW,RH,reg1)) {logline("  %s: read failed\n",name); fails++; return;}
 cmpreg(name,reg1,want,RW,RH);
}
static void gdi_tests(LPDIRECTDRAWSURFACE prim) {
 HDC dc,mdc; HBITMAP bm,old,mono; HBRUSH br,obr; int x,y,i,tx,ty,bad; DWORD tile[64];
 const int OX=10,OY=150; /* test area below the DirectDraw blit area */
 logline("step: GDI tests on the primary surface device context\n");
 if(FAILED(IDirectDrawSurface_GetDC(prim,&dc))) {logline("  GetDC on the primary failed\n"); fails++; return;}
 mdc=CreateCompatibleDC(dc); bm=CreateCompatibleBitmap(dc,8,8); old=(HBITMAP)SelectObject(mdc,bm);
 for(y=0;y<8;y++) for(x=0;x<8;x++) SetPixel(mdc,x,y,RGB((x*37+y*11)&255,(x*13+y*71)&255,(x*97+y*29)&255));
 /* reference region: the tile repeated with BitBlt (memory to screen) */
 for(y=0;y<RH;y+=8) for(x=0;x<RW;x+=8) BitBlt(dc,OX+x,OY+y,8,8,mdc,0,0,SRCCOPY);
 IDirectDrawSurface_ReleaseDC(prim,dc);
 SelectObject(mdc,old); /* a brush is made from the bitmap while it is not selected anywhere */
 if(!readreg(prim,OX,OY,RW,RH,reg0)) {logline("  read failed\n"); fails++; goto out;}
 bad=0; for(y=0;y<RH;y++) for(x=0;x<RW;x++) if(reg0[y*RW+x]!=reg0[(y&7)*RW+(x&7)]) bad++;
 logline("  memory-bitmap to screen tiling: %s (%d pixels not periodic)\n",bad?"FAIL":"PASS",bad); if(bad) fails++;
 for(i=0;i<64;i++) tile[i]=reg0[(i>>3)*RW+(i&7)];
 /* copies in every direction, overlapping and not */
 gdi_copy(prim,"GDI screen copy, non-overlapping",0,0,48,0,40,40,OX,OY);
 gdi_copy(prim,"GDI screen copy, overlap right+down",0,0,6,5,70,60,OX,OY);
 gdi_copy(prim,"GDI screen copy, overlap left+up",6,5,0,0,70,60,OX,OY);
 gdi_copy(prim,"GDI screen copy, overlap right+up",0,10,6,0,70,60,OX,OY);
 gdi_copy(prim,"GDI screen copy, overlap left+down",6,0,0,10,70,60,OX,OY);
 gdi_copy(prim,"GDI screen copy, odd size 33x17 at odd offsets",3,5,41,9,33,17,OX,OY);
 /* pattern brush, aligned and with a shifted origin; the expected pixels come from the tile read back from the screen */
 for(i=0;i<2;i++) {
  int bx=OX+(i?3:0)+RW+8,by=OY+(i?5:0),w=64,h=48; DWORD *got=reg1; static DWORD want[RW*RH]; POINT po;
  if(FAILED(IDirectDrawSurface_GetDC(prim,&dc))) {logline("  GetDC failed\n"); fails++; break;}
  br=CreatePatternBrush(bm); UnrealizeObject(br); SetBrushOrgEx(dc,bx,by,&po); obr=(HBRUSH)SelectObject(dc,br); /* origin BEFORE selecting: Windows 9x fixes it at select time */
  PatBlt(dc,bx,by,w,h,PATCOPY);
  SelectObject(dc,obr); SetBrushOrgEx(dc,po.x,po.y,NULL); DeleteObject(br); IDirectDrawSurface_ReleaseDC(prim,dc);
  if(!readreg(prim,bx,by,w,h,got)) {logline("  read failed\n"); fails++; break;}
  for(y=0;y<h;y++) for(x=0;x<w;x++) want[y*w+x]=tile[(y&7)*8+(x&7)];
  cmpreg(i?"GDI pattern brush PatBlt, origin at the rectangle (shifted case)":"GDI pattern brush PatBlt, origin at the rectangle",got,want,w,h);
 }
 /* pattern brush with an origin that is NOT the rectangle corner */
 {
  int bx=OX+RW+8,by=OY+RH+4,w=64,h=48,ox=bx+3,oy=by+5; static DWORD want[RW*RH]; POINT po;
  if(SUCCEEDED(IDirectDrawSurface_GetDC(prim,&dc))) {
   br=CreatePatternBrush(bm); UnrealizeObject(br); SetBrushOrgEx(dc,ox,oy,&po); obr=(HBRUSH)SelectObject(dc,br);
   PatBlt(dc,bx,by,w,h,PATCOPY); SelectObject(dc,obr); SetBrushOrgEx(dc,po.x,po.y,NULL); DeleteObject(br); IDirectDrawSurface_ReleaseDC(prim,dc);
   if(readreg(prim,bx,by,w,h,reg1)) {
    for(y=0;y<h;y++) for(x=0;x<w;x++) {tx=((x-3)%8+8)%8; ty=((y-5)%8+8)%8; want[y*w+x]=tile[ty*8+tx];}
    cmpreg("GDI pattern brush PatBlt, origin 3,5 away from the rectangle",reg1,want,w,h);
   } else {logline("  read failed\n"); fails++;}
  }
 }
 /* monochrome bitmap expansion (text uses this): two colours only, bit 0 -> text colour, bit 1 -> background colour */
 {
  WORD bits[16]; int c0=0,c1=0; DWORD v0=0,v1=0,bx=OX+RW+8+70,by=OY; static DWORD got[16*16]; int ok=1;
  for(y=0;y<16;y++) bits[y]=(WORD)((y&1)?0xAAAA:0x5555);
  mono=CreateBitmap(16,16,1,1,bits);
  if(mono&&SUCCEEDED(IDirectDrawSurface_GetDC(prim,&dc))) {
   SetTextColor(dc,RGB(250,20,20)); SetBkColor(dc,RGB(20,20,250));
   SelectObject(mdc,mono); BitBlt(dc,(int)bx,by,16,16,mdc,0,0,SRCCOPY); SelectObject(mdc,old);
   IDirectDrawSurface_ReleaseDC(prim,dc);
   if(readreg(prim,(int)bx,by,16,16,got)) {
    /* use the stored byte order: row y is bits[y] little-endian in memory: byte0 = low byte */
    for(y=0;y<16;y++) for(x=0;x<16;x++) {
     BYTE byte=(BYTE)((x<8)?(bits[y]&0xFF):(bits[y]>>8)); int b=(byte>>(7-(x&7)))&1;
     if(b) {if(!c1++) v1=got[y*16+x]; else if(got[y*16+x]!=v1) ok=0;} else {if(!c0++) v0=got[y*16+x]; else if(got[y*16+x]!=v0) ok=0;}
    }
    if(v0==v1) ok=0;
    logline("  GDI monochrome bitmap expansion: %s (text colour pixels %08lX, background pixels %08lX)\n",ok?"PASS":"FAIL",v0,v1); if(!ok) fails++;
   } else {logline("  read failed\n"); fails++;}
  } else logline("  monochrome test skipped (no bitmap or no DC)\n");
  if(mono) DeleteObject(mono);
 }
out:
 SelectObject(mdc,old); DeleteObject(bm); DeleteDC(mdc);
}
/* the screen you look at: colour bars, a one-pixel border (shows wrong pitch or cropping), the mode name */
static void picture(LPDIRECTDRAWSURFACE prim,DWORD w,DWORD h,DWORD b) {
 RECT r; int i; DWORD c[8]; HDC dc; char t[80],t2[80];
 c[0]=rgb(255,255,255); c[1]=rgb(255,255,0); c[2]=rgb(0,255,255); c[3]=rgb(0,255,0); c[4]=rgb(255,0,255); c[5]=rgb(255,0,0); c[6]=rgb(0,0,255); c[7]=rgb(0,0,0);
 for(i=0;i<8;i++) {SetRect(&r,(int)(w*i/8),(int)(h/4),(int)(w*(i+1)/8),(int)(h*3/4)); colorfill(prim,c[i],&r);}
 SetRect(&r,0,0,(int)w,(int)h/4); colorfill(prim,rgb(40,40,40),&r);
 SetRect(&r,0,(int)(h*3/4),(int)w,(int)h); colorfill(prim,rgb(40,40,40),&r);
 SetRect(&r,0,0,(int)w,1); colorfill(prim,c[0],&r); SetRect(&r,0,(int)h-1,(int)w,(int)h); colorfill(prim,c[0],&r);
 SetRect(&r,0,0,1,(int)h); colorfill(prim,c[0],&r); SetRect(&r,(int)w-1,0,(int)w,(int)h); colorfill(prim,c[0],&r);
 if(SUCCEEDED(IDirectDrawSurface_GetDC(prim,&dc))) {
  sprintf(t," %lux%lux%lu ",w,h,b); sprintf(t2," display start register when drawn: %lu ",crtstart()); SetBkMode(dc,OPAQUE); SetBkColor(dc,RGB(0,0,0)); SetTextColor(dc,RGB(255,255,255)); TextOut(dc,12,12,t,(int)strlen(t)); TextOut(dc,12,32,t2,(int)strlen(t2));
  IDirectDrawSurface_ReleaseDC(prim,dc);
 }
}
/* ---- flip chain in this mode ---- */
static int flips(DWORD b,int *flipstate) {
 DDSURFACEDESC s; DDSCAPS caps; LPDIRECTDRAWSURFACE prim=NULL,back=NULL; HRESULT hr; DWORD r[VXF_REPLY_DWORDS],shift,i,bad=0,seen=0; BYTE *base0=NULL; int backs;
 DDCAPS c; DWORD colors[3];
 *flipstate=3;
 logline("step: flip chain in this mode\n");
 if(!flipctl(VXF_QUERY,0,0,r)) {logline("  flip ioctl failed\n"); fails++; *flipstate=2; return 0;}
 logline("  driver flip state %lu (1 direct CRTC, 2 BIOS per flip, 3 none) shift %lu reason %lu\n",r[VXF_R_STATE],r[VXF_R_SHIFT],r[VXF_R_REASON]);
 shift=r[VXF_R_SHIFT];
 memset(&c,0,sizeof(c)); c.dwSize=sizeof(c); IDirectDraw_GetCaps(dd,&c,NULL);
 logline("  DirectDraw caps %08lX surface caps %08lX: FLIP %s\n",c.dwCaps,c.ddsCaps.dwCaps,(c.ddsCaps.dwCaps&DDSCAPS_FLIP)?"offered":"not offered");
 if(r[VXF_R_RESULT]!=VXF_OK||!(c.ddsCaps.dwCaps&DDSCAPS_FLIP)) {logline("  flipping is not available in this mode (not a failure by itself)\n"); return 1;}
 for(backs=2;backs>=1;backs--) {
  memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; s.dwBackBufferCount=backs;
  s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
  hr=IDirectDraw_CreateSurface(dd,&s,&prim,NULL); logline("  CreateSurface(primary flip chain, %d back): %08lX\n",backs,(DWORD)hr);
  if(SUCCEEDED(hr)) break; prim=NULL;
 }
 if(!prim) {logline("  the flip chain could not be created\n"); fails++; *flipstate=2; return 0;}
 setpal(prim);
 memset(&caps,0,sizeof(caps)); caps.dwCaps=DDSCAPS_BACKBUFFER;
 hr=IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back); if(FAILED(hr)) {logline("  GetAttachedSurface: %08lX\n",(DWORD)hr); fails++; *flipstate=2; goto done;}
 {DDSURFACEDESC d; if(!lock_surf(prim,&d)) {logline("  primary lock failed\n"); fails++; *flipstate=2; goto done;} base0=(BYTE*)d.lpSurface; IDirectDrawSurface_Unlock(prim,d.lpSurface);}
 colors[0]=rgb(220,40,40); colors[1]=rgb(40,200,60); colors[2]=rgb(50,70,230);
 for(i=0;i<6&&!aborted;i++) {
  DWORD want,start,disp=0xFFFFFFFFUL; int ok; DDSURFACEDESC d;
  colorfill(back,colors[i%3],NULL);
  hr=IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT); if(FAILED(hr)) {logline("  flip %lu: Flip failed %08lX\n",i,(DWORD)hr); bad++; break;}
  if(lock_surf(prim,&d)) {disp=(DWORD)((BYTE*)d.lpSurface-base0); IDirectDrawSurface_Unlock(prim,d.lpSurface);}
  start=crtstart(); want=disp>>shift; ok=(r[VXF_R_STATE]==VXF_ST_DIRECT)?(start==want):1; if(!ok) bad++; else seen++;
  logline("  flip %lu: primary now at offset %lu; CRTC start %lu; expected %lu: %s\n",i,disp,start,want,ok?"ok":"MISMATCH");
  if(lock_surf(prim,&d)) {DWORD px=getpx(&d,10,10); IDirectDrawSurface_Unlock(prim,d.lpSurface); if((px&maskv())!=(colors[i%3]&maskv())) {logline("   primary pixel is %08lX, wanted %08lX\n",px,colors[i%3]); bad++;}}
  IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back);
  wait_ms(250);
 }
 logline("  flip verification: %s (%lu flips matched, %lu problems)\n",bad?"FAIL":"PASS",seen,bad); if(bad) fails++;
 *flipstate=bad?2:1;
 {HRESULT h2=IDirectDraw_FlipToGDISurface(dd); logline("  FlipToGDISurface: %08lX\n",(DWORD)h2);}
done:
 if(back) IDirectDrawSurface_Release(back);
 if(prim) IDirectDrawSurface_Release(prim);
 {DWORD st=crtstart(); if(st!=0) {logline("  display start register %lu after leaving the chain (should be 0): FAIL\n",st); fails++; flipctl(VXF_RESET,0,0,NULL);}}
 (void)b;
 return 1;
}

/* ---- DAC read-back (driver request VX_IOCTL_DAC with first = 0xFFFFFFFD) ---- */
static int dac_read(DWORD *bits,DWORD *rgb) {
 static vx_dac_req q,o; DWORD got=0;
 memset(&q,0,sizeof(q)); q.cb=sizeof(q); q.abi=VX_DAC_ABI; q.first=0xFFFFFFFDUL; memset(&o,0,sizeof(o));
 if(!DeviceIoControl(dev,VX_IOCTL_DAC,&q,sizeof(q),&o,sizeof(o),&got,NULL)||got!=sizeof(o)||o.abi!=VX_DAC_ABI||o.count!=256) return 0;
 *bits=o.first; memcpy(rgb,o.rgb,256*sizeof(DWORD)); return 1;
}
static DWORD raw_of(DWORD c8,DWORD bits) {return bits==8?c8:(c8>>2);}
static DWORD pack_raw(PALETTEENTRY e,DWORD bits) {return (raw_of(e.peRed,bits)<<16)|(raw_of(e.peGreen,bits)<<8)|raw_of(e.peBlue,bits);}
static int dac_compare(const char *what,PALETTEENTRY *e,int from,int count,DWORD *rgb,DWORD bits) {
 int i,bad=0;
 for(i=from;i<from+count;i++) if(rgb[i]!=pack_raw(e[i],bits)) {if(bad<4) logline("    entry %d: DAC %06lX, expected %06lX\n",i,rgb[i],pack_raw(e[i],bits)); bad++;}
 logline("  %s: %s (%d of %d entries differ)\n",what,bad?"FAIL":"PASS",bad,count); if(bad) fails++;
 return bad;
}
/* DirectDraw palette at 8 bits per pixel, proved by reading the DAC (called with the 3-3-2 palette attached to the primary surface) */
static void dac_tests(LPDIRECTDRAWSURFACE prim) {
 DWORD bits=0,rgb[256]; PALETTEENTRY e[256],keep[5],n[5]; int i; HRESULT hr;
 if(!pal) return;
 logline("step: DAC read-back\n");
 if(!dac_read(&bits,rgb)) {logline("  DAC read-back: the driver refused (not 8bpp native, or not a VGA-compatible DAC): FAIL\n"); fails++; return;}
 logline("  DAC width: %lu bits per component\n",bits);
 for(i=0;i<256;i++) {e[i].peRed=(BYTE)(((i>>5)&7)*255/7); e[i].peGreen=(BYTE)(((i>>2)&7)*255/7); e[i].peBlue=(BYTE)((i&3)*255/3); e[i].peFlags=0;}
 dac_compare("DirectDraw palette attached to the primary surface: all 256 DAC entries",e,0,256,rgb,bits);
 for(i=0;i<5;i++) {keep[i]=e[10+i]; n[i].peRed=(BYTE)(10+i*40); n[i].peGreen=(BYTE)(200-i*10); n[i].peBlue=(BYTE)(i*50); n[i].peFlags=0; e[10+i]=n[i];}
 hr=IDirectDrawPalette_SetEntries(pal,0,10,5,n); logline("  SetEntries(10, 5 entries): %08lX\n",(DWORD)hr); if(FAILED(hr)) fails++;
 if(dac_read(&bits,rgb)) dac_compare("after SetEntries: entries 10..14 changed, the other 251 not",e,0,256,rgb,bits); else {logline("  DAC read-back failed: FAIL\n"); fails++;}
 hr=IDirectDrawSurface_SetPalette(prim,NULL); logline("  SetPalette(primary, NULL): %08lX\n",(DWORD)hr); if(FAILED(hr)) fails++;
 if(dac_read(&bits,rgb)) logline("  after detaching: DAC entry 0 %06lX, 1 %06lX, 255 %06lX, 10 %06lX (the GDI palette is back; not a pass/fail item)\n",rgb[0],rgb[1],rgb[255],rgb[10]);
 hr=IDirectDrawSurface_SetPalette(prim,pal); logline("  SetPalette(primary, palette) again: %08lX\n",(DWORD)hr); if(FAILED(hr)) fails++;
 if(dac_read(&bits,rgb)) dac_compare("after attaching again: the palette comes back with the changed entries",e,0,256,rgb,bits); else {logline("  DAC read-back failed: FAIL\n"); fails++;}
 for(i=0;i<5;i++) e[10+i]=keep[i];
 hr=IDirectDrawPalette_SetEntries(pal,0,10,5,keep); if(FAILED(hr)) fails++;
 if(dac_read(&bits,rgb)) dac_compare("after putting the original entries back: all 256",e,0,256,rgb,bits); else {logline("  DAC read-back failed: FAIL\n"); fails++;}
}

/* ---- video memory capacity: many offscreen surfaces, each filled with its own colour, then all read back (aliasing or wrap-around shows as a wrong pixel) ---- */
static int vram_done;
static DWORD rdpixel(const BYTE *p,DWORD bytes) {return bytes==1?(DWORD)p[0]:bytes==2?(DWORD)(p[0]|(p[1]<<8)):(DWORD)(p[0]|(p[1]<<8)|(p[2]<<16));}
static int ctrlq(DWORD in,DWORD *out);
static int wide_adv;
/* v1.2: a surface wider than the screen (needs DDCAPS2_WIDESURFACES): colour each half, read both back, then blit out of and into the right-hand part */
static void wide_test(void) {
 DWORD bytes=depth/8,W2=(((DWORD)sw*3UL/2UL)+7UL)&~7UL,A=rgb(200,30,30),B=rgb(30,60,220),C=rgb(20,200,40); int H=64,bad=0,k; LPDIRECTDRAWSURFACE wd=NULL,sm=NULL; RECT l,r,all; DDSURFACEDESC d;
 DWORD sx;
 if(W2*bytes>0x3FF8UL) W2=(0x3FF8UL/bytes)&~7UL;
 sx=(DWORD)sw;
 logline("step: surface wider than the screen (%lu wide on a %lu wide screen)\n",W2,sx);
 wd=mk((int)W2,H,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY);
 if(!wd) {if(wide_adv) {logline("  wide surface: FAIL (refused although wide surfaces are advertised)\n"); fails++;} else logline("  wide surface: refused, wide surfaces are not advertised (INFO)\n"); return;}
 l.left=0; l.top=0; l.right=(LONG)sx; l.bottom=H; r.left=(LONG)sx; r.top=0; r.right=(LONG)W2; r.bottom=H; all.left=0; all.top=0; all.right=(LONG)W2; all.bottom=H;
 colorfill(wd,A,&l); colorfill(wd,B,&r);
 if(lock_surf(wd,&d)) {
  int px[5][2]; DWORD ex[5]; px[0][0]=0; px[0][1]=0; ex[0]=A; px[1][0]=(int)sx-1; px[1][1]=H-1; ex[1]=A; px[2][0]=(int)sx; px[2][1]=0; ex[2]=B; px[3][0]=(int)sx+5; px[3][1]=10; ex[3]=B; px[4][0]=(int)W2-1; px[4][1]=H-1; ex[4]=B;
  for(k=0;k<5;k++) {DWORD g=getpx(&d,px[k][0],px[k][1])&maskv(); if(g!=(ex[k]&maskv())) {if(bad<4) logline("    wide pixel (%d,%d): %lX, expected %lX\n",px[k][0],px[k][1],g,ex[k]&maskv()); bad++;}}
  IDirectDrawSurface_Unlock(wd,d.lpSurface);
 } else {logline("  wide surface: Lock failed\n"); bad++;}
 logline("  wide surface fill and readback (both sides of the screen width): %s\n",bad?"FAIL":"PASS"); if(bad) {fails++; bad=0;}
 sm=mk((int)(W2-sx),H,DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY);
 if(sm) {
  HRESULT hr; DWORD g;
  hr=IDirectDrawSurface_Blt(sm,NULL,wd,&r,DDBLT_WAIT,NULL);
  if(FAILED(hr)) {logline("  blit out of the right-hand part: FAIL %08lX\n",(DWORD)hr); fails++;}
  else {
   if(lock_surf(sm,&d)) {g=getpx(&d,3,5)&maskv(); if(g!=(B&maskv())||(getpx(&d,(int)(W2-sx)-1,H-1)&maskv())!=(B&maskv())) bad++; IDirectDrawSurface_Unlock(sm,d.lpSurface);} else bad++;
   logline("  blit out of the right-hand part of the wide surface: %s\n",bad?"FAIL":"PASS"); if(bad) {fails++; bad=0;}
  }
  colorfill(sm,C,NULL);
  hr=IDirectDrawSurface_Blt(wd,&r,sm,NULL,DDBLT_WAIT,NULL);
  if(FAILED(hr)) {logline("  blit into the right-hand part: FAIL %08lX\n",(DWORD)hr); fails++;}
  else {
   if(lock_surf(wd,&d)) {
    if((getpx(&d,(int)sx+5,10)&maskv())!=(C&maskv())||(getpx(&d,(int)W2-1,H-1)&maskv())!=(C&maskv())||(getpx(&d,(int)sx-1,10)&maskv())!=(A&maskv())) bad++;
    IDirectDrawSurface_Unlock(wd,d.lpSurface);
   } else bad++;
   logline("  blit into the right-hand part of the wide surface (left part untouched): %s\n",bad?"FAIL":"PASS"); if(bad) {fails++; bad=0;}
  }
  IDirectDrawSurface_Release(sm);
 }
 IDirectDrawSurface_Release(wd);
}
static void vram_test(DWORD b) {
 enum {MAXS=160};
 static LPDIRECTDRAWSURFACE sf[MAXS]; static DWORD val[MAXS];
 DWORD i,n=0,bytes=b/8,bad=0,sz,wd; DDSURFACEDESC d; DDCAPS c; HRESULT hr; BYTE *first=NULL,*last=NULL;
 vram_done=1;
 logline("step: video memory capacity\n");
 memset(&c,0,sizeof(c)); c.dwSize=sizeof(c); IDirectDraw_GetCaps(dd,&c,NULL);
 logline("  DirectDraw reports video memory: total %lu KB, free %lu KB\n",c.dwVidMemTotal/1024,c.dwVidMemFree/1024);
 {
  /* v1.1b: the first 1024x256 attempt failed with 80070057 at 640x480x32; try narrower surfaces and log each result to see which sizes DirectDraw accepts */
  static const DWORD ws[5]={1024,640,512,256,128}; int k;
  wd=0;
  for(k=0;k<5;k++) {
   LPDIRECTDRAWSURFACE t=NULL;
   memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; d.dwWidth=ws[k]; d.dwHeight=256; d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
   hr=IDirectDraw_CreateSurface(dd,&d,&t,NULL); logline("  trial surface %lux256: CreateSurface %08lX\n",ws[k],(DWORD)hr);
   if(SUCCEEDED(hr)&&t) {IDirectDrawSurface_Release(t); if(!wd) wd=ws[k];}
  }
  if(!wd) {logline("  no trial size worked: FAIL\n"); fails++; return;}
 }
 sz=wd*256UL*bytes;
 for(i=0;i<MAXS&&!aborted;i++) {
  memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; d.dwWidth=wd; d.dwHeight=256; d.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
  hr=IDirectDraw_CreateSurface(dd,&d,&sf[i],NULL);
  if(FAILED(hr)||!sf[i]) {sf[i]=NULL; logline("  surface %lu: CreateSurface %08lX (video memory is full)\n",i+1,(DWORD)hr); break;}
  n++;
 }
 logline("  created %lu offscreen surfaces of %lu KB in video memory = %lu MB\n",n,sz/1024,n*sz/(1024UL*1024UL));
 for(i=0;i<n;i++) {val[i]=(((i+1)*0x0F0FUL+0x1357UL)&maskv()); if(!val[i]) val[i]=1; colorfill(sf[i],val[i],NULL);}
 for(i=0;i<n;i++) {
  DDSURFACEDESC l; int px[3][2]; int k; px[0][0]=0; px[0][1]=0; px[1][0]=(int)wd/2; px[1][1]=100; px[2][0]=(int)wd-1; px[2][1]=255;
  if(!lock_surf(sf[i],&l)) {logline("  surface %lu: Lock failed\n",i+1); bad++; continue;}
  if(!first) first=(BYTE*)l.lpSurface; last=(BYTE*)l.lpSurface;
  for(k=0;k<3;k++) {
   DWORD got=rdpixel((BYTE*)l.lpSurface+(DWORD)px[k][1]*(DWORD)l.lPitch+(DWORD)px[k][0]*bytes,bytes);
   if(got!=val[i]) {if(bad<4) logline("  surface %lu pixel (%d,%d): %lX, expected %lX\n",i+1,px[k][0],px[k][1],got,val[i]); bad++;}
  }
  IDirectDrawSurface_Unlock(sf[i],l.lpSurface);
 }
 if(n) logline("  first surface at %p, last at %p: they span about %ld MB of the frame buffer\n",(void*)first,(void*)last,(long)(((DWORD)last-(DWORD)first)/(1024UL*1024UL)));
 logline("  every surface kept its own colour: %s (%lu wrong pixels)\n",bad?"FAIL":"PASS",bad); if(bad) fails++;
 if(n*sz>=24UL*1024UL*1024UL) logline("  more than 16 MB of video memory is usable through DirectDraw: yes (%lu MB)\n",n*sz/(1024UL*1024UL));
 else logline("  INFO: only %lu MB of video memory was usable (an older driver, VIDMEM_MB set low, or a small BIOS frame buffer setting)\n",n*sz/(1024UL*1024UL));
 if(n>=2&&!aborted&&b!=8) {
  /* v1.2: native fills and copies on the highest surfaces, counted by the engine */
  DWORD o[4],ops0=0,ops1=0,ops2=0,e1=0,e2=0,hi=(DWORD)last-(DWORD)first; DDSURFACEDESC l; int bad2=0;
  const DWORD cA=val[0]^0x0A0A0AUL,cB=val[0]^0x505050UL;
  RECT whole; whole.left=0; whole.top=0; whole.right=(LONG)wd; whole.bottom=256;
  (void)whole;
  if(ctrlq(0xFFFFFFFEUL,o)) ops0=o[0];
  colorfill(sf[n-1],cA&maskv(),NULL);
  if(ctrlq(0xFFFFFFFEUL,o)) ops1=o[0];
  IDirectDrawSurface_Blt(sf[n-2],NULL,sf[n-1],NULL,DDBLT_WAIT,NULL);
  if(ctrlq(0xFFFFFFFEUL,o)) ops2=o[0];
  e1=ops1-ops0; e2=ops2-ops1;
  if(lock_surf(sf[n-2],&l)) {DWORD g=rdpixel((BYTE*)l.lpSurface+100UL*(DWORD)l.lPitch+(wd/2)*bytes,bytes); if(g!=(cA&maskv())) bad2++; IDirectDrawSurface_Unlock(sf[n-2],l.lpSurface);} else bad2++;
  colorfill(sf[0],cB&maskv(),NULL);
  IDirectDrawSurface_Blt(sf[n-1],NULL,sf[0],NULL,DDBLT_WAIT,NULL);
  if(lock_surf(sf[n-1],&l)) {DWORD g=rdpixel((BYTE*)l.lpSurface+255UL*(DWORD)l.lPitch+(wd-1)*bytes,bytes); if(g!=(cB&maskv())) bad2++; IDirectDrawSurface_Unlock(sf[n-1],l.lpSurface);} else bad2++;
  logline("  highest surfaces (about %lu MB above the first): fill reached the engine %lu time(s), copy between two high surfaces %lu time(s), low to high copy checked: %s%s\n",hi/(1024UL*1024UL),e1,e2,bad2?"FAIL":"PASS",(e1&&e2)?"":" WARN (no engine operation counted; ACCEL_2D may be 0)");
  if(bad2) fails++;
 }
 for(i=0;i<n;i++) IDirectDrawSurface_Release(sf[i]);
}
/* ---- one mode ---- */
static void testmode(DWORD w,DWORD h,DWORD b) {
 HRESULT hr; vx_status_t s; DWORD got=0; LPDIRECTDRAWSURFACE prim=NULL; DDSURFACEDESC d; result *R; int f0=fails;
 DDCAPS c;
 if(nresults>=96) return;
 R=&results[nresults++]; memset(R,0,sizeof(*R)); R->w=w; R->h=h; R->b=b; R->tried=1;
 logline("\n== MODE %lux%lux%lu ==\nstep: SetDisplayMode\n",w,h,b);
 hr=IDirectDraw_SetDisplayMode(dd,w,h,b); logline("SetDisplayMode: %08lX\n",(DWORD)hr);
 if(FAILED(hr)) {fails++; R->fails=fails-f0; return;}
 R->setok=1; depth=b; sw=w; sh=h;
 wait_ms(400);
 scan("right after SetDisplayMode");
 memset(&s,0,sizeof(s));
 if(DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)&&got==sizeof(s)&&s.magic==VX_MAGIC) {
  logline("driver reports %lux%lux%lu pitch %lu stage %lu failure %lu vga_mode %lu\n",(DWORD)s.width,(DWORD)s.height,(DWORD)s.bpp,(DWORD)s.video_pitch,(DWORD)s.stage,(DWORD)s.failure,(DWORD)s.vga_mode);
  R->native=(s.width==w&&s.height==h&&s.bpp==b&&s.stage==6&&!s.failure&&!s.vga_mode);
  logline("driver mode check: %s\n",R->native?"PASS (native mode, as requested)":"FAIL (driver is not in the requested native mode)"); if(!R->native) fails++;
 } else {logline("driver status ioctl failed\n"); fails++;}
 memset(&c,0,sizeof(c)); c.dwSize=sizeof(c); IDirectDraw_GetCaps(dd,&c,NULL);
 logline("DirectDraw hardware HAL active: %s; caps %08lX caps2 %08lX surface caps %08lX; wide surfaces %s\n",(c.dwCaps&0x02000000UL)?"NO (emulation only)":"yes",c.dwCaps,c.dwCaps2,c.ddsCaps.dwCaps,(c.dwCaps2&DDCAPS2_WIDESURFACES)?"advertised":"not advertised");
 wide_adv=(c.dwCaps2&DDCAPS2_WIDESURFACES)?1:0;
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
 hr=IDirectDraw_CreateSurface(dd,&d,&prim,NULL); logline("CreateSurface(primary): %08lX\n",(DWORD)hr);
 if(FAILED(hr)||!prim) {fails++; R->fails=fails-f0; return;}
 setpal(prim);
 if(b==8) dac_tests(prim);
 {DDSURFACEDESC l; if(lock_surf(prim,&l)) {logline("primary lock: pitch %ld, pixel format bits %lu\n",(long)l.lPitch,l.ddpfPixelFormat.dwRGBBitCount); IDirectDrawSurface_Unlock(prim,l.lpSurface);} else {logline("primary lock failed\n"); fails++;}}
 logline("step: draw the test picture\n");
 picture(prim,w,h,b); scan("picture drawn"); wait_ms(250);
 if(!aborted) blits(prim);
 if(!aborted) ddrop_tests();
 if(!aborted) wide_test();
 if(!aborted&&b!=8&&!vram_done) vram_test(b);
 if(!aborted&&h>=240&&w>=320) gdi_tests(prim); else if(!aborted) logline("step: GDI tests skipped (screen too small)\n");
 scan("after blits and GDI tests"); if(!aborted) wait_ms(300);
 IDirectDrawSurface_Release(prim); prim=NULL;
 if(!aborted) flips(b,&R->flip); else R->flip=0;
 scan("after the flip chain");
 R->fails=fails-f0;
 logline("MODE %lux%lux%lu result: %s\n",w,h,b,R->fails?"FAILURES":"all checks passed");
}

/* ---- desktop phase: Display Properties-style mode and depth changes, GDI checked through the screen DC ---- */
static int near_rgb(COLORREF a,COLORREF b,int tol) {
 int dr=(int)(a&255)-(int)(b&255),dg=(int)((a>>8)&255)-(int)((b>>8)&255),db=(int)((a>>16)&255)-(int)((b>>16)&255);
 if(dr<0) dr=-dr; if(dg<0) dg=-dg; if(db<0) db=-db;
 return dr<=tol&&dg<=tol&&db<=tol;
}
static void grab(HDC dc,int x,int y,int w,int h,COLORREF *out) {int i,j; for(j=0;j<h;j++) for(i=0;i<w;i++) out[j*w+i]=GetPixel(dc,x+i,y+j);}
static COLORREF dsrc[96*64+64],dbefore[128*96],dafter[128*96];
static void desk_gdi(HDC dc,DWORD w,DWORD h,DWORD b) {
 /* only colours that exist exactly in the default 20-colour palette, so an 8bpp solid brush does not dither */
 static const COLORREF cols[8]={RGB(255,0,0),RGB(0,255,0),RGB(0,0,255),RGB(255,255,255),RGB(0,0,0),RGB(192,192,192),RGB(128,0,128),RGB(255,255,0)};
 int i,j,x,y,bad=0,tol=(b==16)?8:0; RECT r; HBRUSH br,old; COLORREF want,got; int f0=(int)fails;
 logline("  step: solid fills (GetNearestColor against GetPixel)\n");
 for(i=0;i<8;i++) {
  r.left=10+i*40; r.top=10; r.right=r.left+32; r.bottom=r.top+32; br=CreateSolidBrush(cols[i]); FillRect(dc,&r,br); DeleteObject(br);
  want=GetNearestColor(dc,cols[i]);
  for(j=0;j<4;j++) {got=GetPixel(dc,r.left+4+j*8,r.top+4+j*8); if(!near_rgb(got,want,tol)) {if(bad<4) logline("    fill %d: pixel %06lX, expected %06lX\n",i,(DWORD)got,(DWORD)want); bad++;}}
 }
 logline("  solid fills: %s (%d wrong pixels)\n",bad?"FAIL":"PASS",bad); if(bad) fails++;
 logline("  step: hatch pattern, PATINVERT twice\n");
 {
  int px=10,py=60,pw=64,ph=64,changed=0,back=0; HBRUSH hb=CreateHatchBrush(HS_DIAGCROSS,RGB(255,0,0));
  SetBkColor(dc,RGB(255,255,255)); r.left=px; r.top=py; r.right=px+pw; r.bottom=py+ph; FillRect(dc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));
  grab(dc,px,py,pw,ph,dbefore);
  old=(HBRUSH)SelectObject(dc,hb); PatBlt(dc,px,py,pw,ph,PATINVERT); grab(dc,px,py,pw,ph,dafter);
  for(i=0;i<pw*ph;i++) if(dafter[i]!=dbefore[i]) changed++;
  PatBlt(dc,px,py,pw,ph,PATINVERT); SelectObject(dc,old); DeleteObject(hb); grab(dc,px,py,pw,ph,dsrc);
  for(i=0;i<pw*ph;i++) if(dsrc[i]!=dbefore[i]) back++;
  logline("  PATINVERT: first pass changed %d pixels, second pass left %d pixels different from the start: %s\n",changed,back,(changed&&!back)?"PASS":"FAIL"); if(!changed||back) fails++;
 }
 for(j=0;j<2;j++) {
  int cw=96,ch=64,sx=200,sy=120,dx=j?-5:5,dy=j?-3:3,cells=0; COLORREF *dst=dafter;
  static const int cx=12,cy=8;
  logline("  step: overlapping screen copy, destination %s the source\n",j?"before":"after");
  for(y=0;y<ch;y+=cy) for(x=0;x<cw;x+=cx) {r.left=sx+x; r.top=sy+y; r.right=r.left+cx; r.bottom=r.top+cy; br=CreateSolidBrush(cols[(x/cx*3+y/cy)&7]); FillRect(dc,&r,br); DeleteObject(br);}
  grab(dc,sx-8,sy-8,cw+16,ch+16,dbefore);
  BitBlt(dc,sx+dx,sy+dy,cw,ch,dc,sx,sy,SRCCOPY);
  grab(dc,sx-8,sy-8,cw+16,ch+16,dst); bad=0;
  for(y=0;y<ch;y++) for(x=0;x<cw;x++) {
   COLORREF e=dbefore[(y+8)*(cw+16)+(x+8)],g=dst[(y+8+dy)*(cw+16)+(x+8+dx)];
   cells++; if(e!=g) {if(bad<3) logline("    copy pixel (%d,%d): %06lX, expected %06lX\n",x,y,(DWORD)g,(DWORD)e); bad++;}
  }
  logline("  screen copy: %s (%d of %d pixels wrong)\n",bad?"FAIL":"PASS",bad,cells); if(bad) fails++;
 }
 {
  /* v1.2: the screen read (GetPixel) cannot see a software cursor, so a pixel compare proves nothing either way. Check the cursor state instead and move it for a visual check. */
  int cxp=(int)w/2,cyp=(int)h/2,k,bad=0; POINT pt; HCURSOR hc,ar=LoadCursor(NULL,IDC_ARROW); int cnt;
  SetCursor(ar); SetCursorPos(cxp,cyp); wait_ms(100);
  GetCursorPos(&pt); hc=GetCursor(); cnt=ShowCursor(TRUE); ShowCursor(FALSE);
  if(abs(pt.x-cxp)>1||abs(pt.y-cyp)>1) bad++;
  if(hc!=ar) bad++;
  if(cnt<0) bad++;
  logline("  cursor state: position %ld,%ld (asked %d,%d), arrow shape %s, show count %d: %s\n",(long)pt.x,(long)pt.y,cxp,cyp,hc==ar?"set":"NOT set",cnt,bad?"FAIL":"PASS");
  if(bad) fails++;
  for(k=0;k<8;k++) {SetCursorPos(cxp-60+k*16,cyp-30+k*8); wait_ms(60);}
  SetCursorPos(cxp,cyp);
  logline("  cursor pixels: not verifiable from a screen read (INFO); the cursor was moved across the screen for a visual check\n");
 }
 (void)f0;
}

/* v1.4: brush fills against a software reference. Everything is drawn twice with the same brush, origin and ROP: on the screen DC (the engine may take it) and on a
 * memory bitmap of the screen's format (DIB engine, software). Both are then read back as raw bitmap bits and compared over the rectangle plus a margin. */
static struct {BITMAPINFOHEADER h; BYTE bits[192];} dibbrush;
static BYTE bga[1024*4*200],bgb[1024*4*200],bgo[1024*4*200];
static const BYTE bm_rows[8]={0xAA,0x55,0xF0,0x0F,0xC3,0x3C,0x81,0x7E}; /* the 1-bit bitmap brush's rows (kinds 15 and 16) */
static HBRUSH make_brush(int kind) {
 static const BYTE pal[8][3]={{0,0,255},{0,255,0},{255,0,0},{255,255,255},{0,0,0},{192,192,192},{128,0,128},{0,255,255}}; /* B,G,R of the colours in the default palette */
 int x,y;
 if(kind==2) return CreateSolidBrush(RGB(100,150,200));
 if(kind>=15) { /* v1.9: a brush made from a 1-bit bitmap: the pattern in the brush, the colours from the DC (0 bits text colour, 1 bits background) */
  static const BYTE rows[16]={0xAA,0,0x55,0,0xF0,0,0x0F,0,0xC3,0,0x3C,0,0x81,0,0x7E,0};
  HBITMAP mb=CreateBitmap(8,8,1,1,rows); HBRUSH hb; if(!mb) return NULL;
  hb=CreatePatternBrush(mb); DeleteObject(mb); return hb;
 }
 if(kind>=3) {static const int hst[6]={HS_HORIZONTAL,HS_VERTICAL,HS_FDIAGONAL,HS_BDIAGONAL,HS_CROSS,HS_DIAGCROSS}; return CreateHatchBrush(hst[(kind-3)%6],RGB(255,0,0));} /* kinds 3..8: the six styles opaque, kinds 9..14: the same with a transparent background */
 memset(&dibbrush,0,sizeof(dibbrush)); dibbrush.h.biSize=sizeof(BITMAPINFOHEADER); dibbrush.h.biWidth=8; dibbrush.h.biHeight=8; dibbrush.h.biPlanes=1; dibbrush.h.biBitCount=24; dibbrush.h.biCompression=BI_RGB;
 for(y=0;y<8;y++) for(x=0;x<8;x++) {
  int idx=(kind==0)?((x*3+y*5+(x>>1))&7):(((x+y)&1)?3:4); /* kind 0: 8 colours, kind 1: black and white */
  BYTE *p=&dibbrush.bits[y*24+x*3]; p[0]=pal[idx][0]; p[1]=pal[idx][1]; p[2]=pal[idx][2];
 }
 return CreateDIBPatternBrushPt(&dibbrush,DIB_RGB_COLORS);
}
/* v2.1: where do the two bitmaps differ? bounding box (in pixels), number of rows with a difference, and the first pairs (reference first, screen second). Limited to a few reports per mode. */
static int diag_budget,shown_pre_g;
static void diff_report(const char *tag,int x0,int y0,int ww,int hh,int bypp,int pitch) {
 int x,y,minx=100000,maxx=-1,miny=100000,maxy=-1,rows=0,shown=0,cnt=0;
 if(diag_budget<=0) return;
 diag_budget--;
 for(y=y0;y<y0+hh;y++) {
  int rowbad=0;
  for(x=x0;x<x0+ww;x++) {
   int k,d=0; DWORD va=0,vb=0;
   for(k=0;k<bypp;k++) {BYTE a=bga[(DWORD)y*pitch+x*bypp+k],c=bgb[(DWORD)y*pitch+x*bypp+k]; va|=(DWORD)a<<(8*k); vb|=(DWORD)c<<(8*k); if(a!=c) d=1;}
   if(!d) continue;
   rowbad=1; cnt++; if(x<minx) minx=x; if(x>maxx) maxx=x; if(y<miny) miny=y; if(y>maxy) maxy=y;
   if(shown<4) {logline("      %s: pixel %d,%d reference %lX screen %lX\n",tag,x,y,va,vb); shown++;}
  }
  rows+=rowbad;
 }
 logline("      %s: %d pixels differ, bounding box x %d..%d, y %d..%d, %d rows affected\n",tag,cnt,minx,maxx,miny,maxy,rows);
}
#define NKIND 17
static void brush_tests(HDC dc,DWORD w,DWORD h,DWORD b) {
 static const char *kname[NKIND]={"8-colour DIB pattern","2-colour DIB pattern","dithered solid colour","hatch HORIZONTAL (opaque)","hatch VERTICAL (opaque)","hatch FDIAGONAL (opaque)","hatch BDIAGONAL (opaque)","hatch CROSS (opaque)","hatch DIAGCROSS (opaque)","hatch HORIZONTAL (transparent)","hatch VERTICAL (transparent)","hatch FDIAGONAL (transparent)","hatch BDIAGONAL (transparent)","hatch CROSS (transparent)","hatch DIAGCROSS (transparent)","1-bit bitmap brush (opaque)","1-bit bitmap brush (transparent)"};
 static const int org[4][2]={{0,0},{3,5},{7,2},{5,7}};
 static const int rc[3][4]={{20,20,200,100},{23,17,201,99},{31,40,257,70}};
 HDC ma,mb; HBITMAP bma,bmb,oa,ob; int kind,oi,ri,ro,cases[NKIND]={0},hits[NKIND]={0},bad[NKIND]={0},total=0,bypp=(int)(b/8),f0=(int)fails;
 BITMAP bm;
 logline("  step: brush fills against a software reference (PatBlt on the screen and on a memory bitmap)\n");
 ma=CreateCompatibleDC(dc); mb=CreateCompatibleDC(dc); bma=CreateCompatibleBitmap(dc,(int)w,(int)h); bmb=CreateCompatibleBitmap(dc,(int)w,(int)h);
 if(!ma||!mb||!bma||!bmb) {logline("  brush tests: could not create the memory bitmaps: FAIL\n"); fails++; return;}
 oa=(HBITMAP)SelectObject(ma,bma); ob=(HBITMAP)SelectObject(mb,bmb);
 GetObject(bma,sizeof(bm),&bm);
 if(bm.bmBitsPixel!=b||(DWORD)bm.bmWidthBytes*176>sizeof(bga)) {logline("  brush tests: the memory bitmap is %d bits per pixel, %d bytes per row (expected %lu bits): FAIL\n",bm.bmBitsPixel,bm.bmWidthBytes,b); fails++;}
 else
 for(kind=0;kind<NKIND&&!aborted;kind++) for(oi=0;oi<4;oi++) for(ri=0;ri<3;ri++) for(ro=0;ro<2;ro++) {
  int rx=rc[ri][0],ry=rc[ri][1],rw=rc[ri][2],rh=rc[ri][3],x0=rx-8,y0=ry-8,ww=rw+16,hh=rh+16,y,nb=0,first=-1; DWORD rop=ro?PATINVERT:PATCOPY,o[4],ops0=0,ops1=0; HBRUSH br,o1,o2;
  SetBrushOrgEx(dc,org[oi][0],org[oi][1],NULL); SetBrushOrgEx(ma,org[oi][0],org[oi][1],NULL);
  {int bm_=((kind>=9&&kind<=14)||kind==16)?TRANSPARENT:OPAQUE; COLORREF bc=(kind>=3&&ri==1)?RGB(255,255,0):RGB(255,255,255); /* v1.6: the background colour changes with the rectangle, so a changed background must reach the engine */
  SetBkMode(dc,bm_); SetBkMode(ma,bm_); SetBkColor(dc,bc); SetBkColor(ma,bc); SetTextColor(dc,RGB(0,0,255)); SetTextColor(ma,RGB(0,0,255));}
  br=make_brush(kind); if(!br) {logline("    brush %d: could not be created\n",kind); fails++; continue;}
  UnrealizeObject(br); o1=(HBRUSH)SelectObject(dc,br); o2=(HBRUSH)SelectObject(ma,br);
  BitBlt(ma,x0,y0,ww,hh,dc,x0,y0,SRCCOPY);
  if(ctrlq(0xFFFFFFFEUL,o)) ops0=o[0];
  if(ri==1) {IntersectClipRect(dc,rx+5,ry+5,rx+rw-5,ry+rh-5); IntersectClipRect(ma,rx+5,ry+5,rx+rw-5,ry+rh-5);} /* v1.6: a clipped fill */
  PatBlt(dc,rx,ry,rw,rh,rop);
  if(ctrlq(0xFFFFFFFEUL,o)) ops1=o[0];
  if((kind==15||kind==16)&&oi==0&&ri==0&&ro==0) GetBitmapBits(bma,(DWORD)bm.bmWidthBytes*(DWORD)(ry+rh),bgo); /* v2.7: the destination before DIBENG draws, for the classification below */
  PatBlt(ma,rx,ry,rw,rh,rop);
  if(ri==1) {SelectClipRgn(dc,NULL); SelectClipRgn(ma,NULL);}
  BitBlt(mb,x0,y0,ww,hh,dc,x0,y0,SRCCOPY);
  GetBitmapBits(bma,(DWORD)bm.bmWidthBytes*(DWORD)(y0+hh),bga); GetBitmapBits(bmb,(DWORD)bm.bmWidthBytes*(DWORD)(y0+hh),bgb);
  if((kind==15||kind==16)&&oi==0&&ri==0&&ro==0) {
   /* v2.7: what DIBENG does with the 0 and 1 bits of a 1-bit bitmap brush (text colour blue, background white; the draw mode is OPAQUE for kind 15, TRANSPARENT for kind 16) */
   int xx,yy,k2; long ch[2],sm[2]; DWORD fv[2]; int have[2];
   ch[0]=ch[1]=sm[0]=sm[1]=0; fv[0]=fv[1]=0; have[0]=have[1]=0;
   for(yy=ry;yy<ry+rh;yy++) for(xx=rx;xx<rx+rw;xx++) {
    int bit=(bm_rows[(yy-org[oi][1])&7]>>(7-((xx-org[oi][0])&7)))&1; DWORD va=0,vo=0;
    for(k2=bypp-1;k2>=0;k2--) {va=(va<<8)|bga[(DWORD)yy*bm.bmWidthBytes+xx*bypp+k2]; vo=(vo<<8)|bgo[(DWORD)yy*bm.bmWidthBytes+xx*bypp+k2];}
    if(va!=vo) {ch[bit]++; if(!have[bit]) {have[bit]=1; fv[bit]=va;}} else sm[bit]++;
   }
   logline("  classification: %s on DIBENG: 0 bits: %ld pixels changed (first value %lX), %ld left alone; 1 bits: %ld changed (first value %lX), %ld left alone\n",kname[kind],ch[0],fv[0],sm[0],ch[1],fv[1],sm[1]);
  }
  for(y=y0;y<y0+hh;y++) {
   int k,from=x0*bypp,to=(x0+ww)*bypp;
   for(k=from;k<to;k++) if(bga[(DWORD)y*bm.bmWidthBytes+k]!=bgb[(DWORD)y*bm.bmWidthBytes+k]) {if(first<0) first=y*10000+k; nb++;}
  }
  SelectObject(dc,o1); SelectObject(ma,o2); DeleteObject(br);
  cases[kind]++; total++; if(ops1>ops0) hits[kind]++;
  if(nb) {
   bad[kind]++;
   if(bad[kind]<=2) logline("    %s, origin (%d,%d), rect %d,%d %dx%d, %s: %d bytes differ from the software reference (first at row %d byte %d)\n",kname[kind],org[oi][0],org[oi][1],rx,ry,rw,rh,ro?"PATINVERT":"PATCOPY",nb,first/10000,first%10000);
  }
 }
 SelectObject(ma,oa); SelectObject(mb,ob); DeleteObject(bma); DeleteObject(bmb); DeleteDC(ma); DeleteDC(mb);
 SetBrushOrgEx(dc,0,0,NULL);
 for(kind=0;kind<NKIND;kind++) {
  logline("  brush fills, %s: %d cases, %d reached the engine, %d differ from the software reference: %s\n",kname[kind],cases[kind],hits[kind],bad[kind],bad[kind]?"FAIL":"PASS");
  if(bad[kind]) fails++;
 }
 logline("  brush fills: %d cases, %s\n",total,((int)fails==f0)?"PASS":"FAIL");
 if(cases[0]&&!hits[0]) logline("  WARN: no 8-colour pattern fill reached the engine (ACCEL_COLORBRUSH=0, or the DRV declined the brush)\n");
 for(kind=3;kind<9;kind++) if(cases[kind]&&hits[kind]<cases[kind]) logline("  WARN: %s: %d of %d fills stayed in software (v10.79 expects the opaque hatches to reach the engine; ACCEL_COLORBRUSH=0 or the DRV declined the brush)\n",kname[kind],cases[kind]-hits[kind],cases[kind]);
 for(kind=15;kind<17;kind++) if(cases[kind]&&hits[kind]<cases[kind]) logline("  WARN: %s: %d of %d fills stayed in software (v10.89 expects them to reach the engine, transparent mode included)\n",kname[kind],cases[kind]-hits[kind],cases[kind]);
 for(kind=9;kind<15;kind++) if(cases[kind]&&hits[kind]<cases[kind]) logline("  WARN: %s: %d of %d fills stayed in software (v10.82 expects them to reach the engine like the opaque ones: DIBENG paints the background colour in TRANSPARENT mode too)\n",kname[kind],cases[kind]-hits[kind],cases[kind]);
}


/* v2.0: ROP3 codes against a software reference (driver v10.84: the engine does every ROP3 exactly, VX2ROP). Screen-to-screen source ROPs (no overlap, overlap both ways, clipped),
 * and the source-free pattern ROPs with solid, colour-pattern, hatch and 1-bit bitmap brushes. Same method as the brush tests: everything is drawn on the screen DC (the engine may take it)
 * and on a memory bitmap (DIBENG), both read back as raw bits. */
static void rop_noise(HDC d,int x0,int y0,int ww,int hh) {
 int i; HBRUSH hb,old; RECT r;
 for(i=0;i<70;i++) {
  hb=CreateSolidBrush(RGB((i*47)&255,(i*91+30)&255,(i*13+100)&255)); r.left=x0+(i*37)%(ww-20); r.top=y0+(i*23)%(hh-14); r.right=r.left+8+(i*11)%40; r.bottom=r.top+6+(i*7)%30;
  FillRect(d,&r,hb); DeleteObject(hb);
 }
 hb=CreateHatchBrush(HS_DIAGCROSS,RGB(255,255,0)); old=(HBRUSH)SelectObject(d,hb); SetBkColor(d,RGB(0,0,255)); PatBlt(d,x0+30,y0+20,ww/2,hh/3,PATCOPY); SelectObject(d,old); DeleteObject(hb);
}
static void rop_tests(HDC dc,DWORD w,DWORD h,DWORD b) {
 static const DWORD scode[9]={0x008800C6UL,0x00EE0086UL,0x00660046UL,0x00330008UL,0x00440328UL,0x001100A6UL,0x00BB0226UL,0x00C000CAUL,0x00FB0A09UL};
 static const char *sname[9]={"SRCAND","SRCPAINT","SRCINVERT","NOTSRCCOPY","SRCERASE","NOTSRCERASE","MERGEPAINT","MERGECOPY","PATPAINT"};
 static const DWORD pcode[15]={0x00A000C9UL,0x00FA0089UL,0x00A50065UL,0x00AF0229UL,0x00F50225UL,0x000500A9UL,0x000A0329UL,0x00500325UL,0x000F0001UL,0x005F00E9UL,0x00000042UL,0x00FF0062UL,0x00550009UL,0x005A0049UL,0x00F00021UL};
 static const char *pname[15]={"DPa","DPo","PDxn","DPno","PDno","DPon","DPna","PDna","Pn","DPan","BLACKNESS","WHITENESS","DSTINVERT","PATINVERT","PATCOPY"};
 static const int bk[5]={2,0,1,3,15}; static const char *bkn[5]={"solid","8-colour DIB","2-colour DIB","hatch","1-bit bitmap"};
 static const int geo[3][4]={{20,30,150,30},{40,60,70,80},{70,80,40,60}}; /* sx,sy,dx,dy: apart, overlap down-right, overlap up-left */
 HDC ma,mb,mn; HBITMAP bma,bmb,bmn,oa,ob,on; BITMAP bm; int bypp=(int)b/8,f0=(int)fails,i,g,cl,k,total=0;
 int X0=8,Y0=8,RWD=300,RHT=190,CW=120,CH=70; const int BXS=312;
 int sc[9]={0},sh[9]={0},sb[9]={0},pc[15][5],ph[15][5],pb[15][5];
 (void)w; (void)h;
 memset(pc,0,sizeof(pc)); memset(ph,0,sizeof(ph)); memset(pb,0,sizeof(pb));
 logline("  step: ROP3 codes against a software reference (screen-to-screen source ROPs, pattern ROPs)\n"); diag_budget=6; shown_pre_g=0;
 ma=CreateCompatibleDC(dc); mb=CreateCompatibleDC(dc); mn=CreateCompatibleDC(dc);
 bma=CreateCompatibleBitmap(dc,(int)w,(int)h); bmb=CreateCompatibleBitmap(dc,(int)w,(int)h); bmn=CreateCompatibleBitmap(dc,(int)w,(int)h);
 if(!ma||!mb||!mn||!bma||!bmb||!bmn) {logline("  rop tests: could not create the memory bitmaps: FAIL\n"); fails++; return;}
 oa=(HBITMAP)SelectObject(ma,bma); ob=(HBITMAP)SelectObject(mb,bmb); on=(HBITMAP)SelectObject(mn,bmn); GetObject(bma,sizeof(bm),&bm);
 if(bm.bmBitsPixel!=b||(DWORD)bm.bmWidthBytes*(DWORD)(Y0+RHT)>sizeof(bga)) {logline("  rop tests: the memory bitmap is %d bits per pixel (expected %lu): FAIL\n",bm.bmBitsPixel,b); fails++;}
 else {
  rop_noise(mn,X0,Y0,RWD,RHT);
  SetBkMode(dc,OPAQUE); SetBkMode(ma,OPAQUE); SetBkColor(dc,RGB(255,255,255)); SetBkColor(ma,RGB(255,255,255)); SetTextColor(dc,RGB(0,0,255)); SetTextColor(ma,RGB(0,0,255));
  for(i=0;i<9+15*5&&!aborted;i++) for(g=0;g<(i<9?3:1);g++) for(cl=0;cl<2;cl++) {
   int bi=i<9?0:(i-9)%5,pi=i<9?0:(i-9)/5; DWORD code=i<9?scode[i]:pcode[pi]; int kind=i<9?2:bk[bi];
   int sx=geo[g][0],sy=geo[g][1],dx=geo[g][2],dy=geo[g][3],cw=CW,ch=CH; int y,nb=0,first=-1; DWORD o[4],ops0=0,ops1=0; HBRUSH br,o1,o2;
   if(i>=9&&g>0) continue;
   if(i>=9) {dx=40+13*cl; dy=30+7*cl; cw=200; ch=100;} /* 20000 pixels: above the engine's fill threshold */
   if(i<9&&sy==30&&sx==20) {} /* apart */
   SetBrushOrgEx(dc,3,5,NULL);
   if(i<9||kind==2) br=CreateSolidBrush(RGB(255,0,0)); else br=make_brush(kind); /* a pure palette colour, so it is a true solid brush at 8bpp too */
   if(!br) {fails++; continue;}
   UnrealizeObject(br); o1=(HBRUSH)SelectObject(dc,br); o2=NULL;
   /* v2.3: no memory-bitmap reference any more (at 8bpp a screen-to-memory copy is not the identity, which made D-dependent ROPs look wrong even in software). Two identical copies of the noise sit side by side on the screen
    * (the second 312 pixels to the right, a multiple of 8 so brush phase is the same); the first gets the operation with the engine on, the second with the engine switched off (DIBENG software). Then both are read back and compared. */
   BitBlt(dc,X0,Y0,RWD,RHT,mn,X0,Y0,SRCCOPY); BitBlt(dc,X0+BXS,Y0,RWD,RHT,mn,X0,Y0,SRCCOPY);
   if(cl) IntersectClipRect(dc,dx+9,dy+7,dx+cw-11,dy+ch-6);
   if(ctrlq(0xFFFFFFFEUL,o)) ops0=o[0];
   if(i<9) BitBlt(dc,dx,dy,cw,ch,dc,sx,sy,code); else PatBlt(dc,dx,dy,cw,ch,code);
   if(ctrlq(0xFFFFFFFEUL,o)) ops1=o[0];
   if(cl) {SelectClipRgn(dc,NULL); IntersectClipRect(dc,dx+BXS+9,dy+7,dx+BXS+cw-11,dy+ch-6);}
   ctrlq(2,o); /* engine off */
   if(i<9) BitBlt(dc,dx+BXS,dy,cw,ch,dc,sx+BXS,sy,code); else PatBlt(dc,dx+BXS,dy,cw,ch,code);
   ctrlq(0xFFFFFFFFUL,o); /* back to the registry settings */
   if(cl) SelectClipRgn(dc,NULL);
   BitBlt(mb,X0,Y0,RWD+BXS,RHT,dc,X0,Y0,SRCCOPY);
   GetBitmapBits(bmb,(DWORD)bm.bmWidthBytes*(DWORD)(Y0+RHT),bgb);
   for(y=Y0;y<Y0+RHT;y++) memcpy(&bga[(DWORD)y*bm.bmWidthBytes+X0*bypp],&bgb[(DWORD)y*bm.bmWidthBytes+(X0+BXS)*bypp],(DWORD)RWD*bypp);
   for(y=Y0;y<Y0+RHT;y++) for(k=X0*bypp;k<(X0+RWD)*bypp;k++) if(bga[(DWORD)y*bm.bmWidthBytes+k]!=bgb[(DWORD)y*bm.bmWidthBytes+k]) {if(first<0) first=y*10000+k; nb++;}
   if(nb) diff_report(i<9?sname[i]:pname[pi],X0,Y0,RWD,RHT,bypp,bm.bmWidthBytes);
   SelectObject(dc,o1); (void)o2; DeleteObject(br);
   total++;
   if(i<9) {sc[i]++; if(ops1>ops0) sh[i]++; if(nb) sb[i]++;} else {pc[pi][bi]++; if(ops1>ops0) ph[pi][bi]++; if(nb) pb[pi][bi]++;}
   if(nb&&((i<9&&sb[i]<=2)||(i>=9&&pb[pi][bi]<=1))) logline("    %s%s%s, %s%s: %d bytes differ from the software reference (first at row %d byte %d)\n",i<9?sname[i]:pname[pi],i<9?"":", ",i<9?"":bkn[bi],i<9?(g==0?", apart":g==1?", overlap down-right":", overlap up-left"):"",cl?", clipped":"",nb,first/10000,first%10000);
  }
  SetBrushOrgEx(dc,0,0,NULL);
  for(i=0;i<9;i++) {logline("  ROP %s (%08lX), screen to screen: %d cases, %d reached the engine, %d differ from the software reference: %s\n",sname[i],scode[i],sc[i],sh[i],sb[i],sb[i]?"FAIL":"PASS"); if(sb[i]) fails++; if(i<7&&sc[i]&&sh[i]<sc[i]) logline("  WARN: %s: %d of %d stayed in software (v10.84 expects the engine; ACCEL_COPY=0 or below the size threshold)\n",sname[i],sc[i]-sh[i],sc[i]);}
  logline("  (MERGECOPY and PATPAINT use the solid brush as a fixed colour; the clipped and unclipped copies are counted together)\n");
  for(i=0;i<15;i++) for(k=0;k<5;k++) if(pc[i][k]) {
   int engine_expected=(k<3)||pcode[i]==0x005A0049UL||pcode[i]==0x00F00021UL||i>=10;
   logline("  ROP %s (%08lX) with %s brush: %d cases, %d reached the engine, %d differ from the software reference: %s\n",pname[i],pcode[i],bkn[k],pc[i][k],ph[i][k],pb[i][k],pb[i][k]?"FAIL":"PASS");
   if(pb[i][k]) fails++;
   if(engine_expected&&ph[i][k]<pc[i][k]) logline("  WARN: %s with %s brush: %d of %d stayed in software (v10.84 expects the engine)\n",pname[i],bkn[k],pc[i][k]-ph[i][k],pc[i][k]);
   if(!engine_expected) logline("  information: %s with %s brush: %d of %d reached the engine (v10.84 keeps hatches and bitmap brushes with these ROPs in software)\n",pname[i],bkn[k],ph[i][k],pc[i][k]);
  }
 }
 SelectObject(ma,oa); SelectObject(mb,ob); SelectObject(mn,on); DeleteObject(bma); DeleteObject(bmb); DeleteObject(bmn); DeleteDC(ma); DeleteDC(mb); DeleteDC(mn);
 logline("  ROP tests: %d cases, %s\n",total,((int)fails==f0)?"PASS":"FAIL");
}
static vxh_reply hrep; static int hesc(HDC dc,DWORD command,void *out);
/* v1.9: ExtTextOut with an opaque rectangle against a software reference (v10.83: the rectangle is filled by the engine, the text is drawn by DIBENG). Same method as the brush tests. */
static void text_tests(HDC dc,DWORD w,DWORD h,DWORD b) {
 static const int rc[2][4]={{20,20,220,120},{27,35,229,137}};
 static const char txt[]="Hello VX855 opaque";
 HDC ma,mb; HBITMAP bma,bmb,oa,ob; BITMAP bm; int bypp=(int)b/8,f0=(int)fails,cases=0,hits=0,bad=0,mode,opt,clipv,ri; (void)w; (void)h;
 logline("  step: ExtTextOut with an opaque rectangle against a software reference\n"); diag_budget=3;
 if(hesc(dc,VXH_ARM,NULL)!=1) logline("  (the driver did not accept the capture escape: no ExtTextOut diagnostic)\n");
 ma=CreateCompatibleDC(dc); mb=CreateCompatibleDC(dc); bma=CreateCompatibleBitmap(dc,(int)w,(int)h); bmb=CreateCompatibleBitmap(dc,(int)w,(int)h);
 if(!ma||!mb||!bma||!bmb) {logline("  text tests: could not create the memory bitmaps: FAIL\n"); fails++; return;}
 oa=(HBITMAP)SelectObject(ma,bma); ob=(HBITMAP)SelectObject(mb,bmb); GetObject(bma,sizeof(bm),&bm);
 if(bm.bmBitsPixel!=b||(DWORD)bm.bmWidthBytes*176>sizeof(bga)) {logline("  text tests: the memory bitmap is %d bits per pixel (expected %lu): FAIL\n",bm.bmBitsPixel,b); fails++;}
 else
 for(mode=0;mode<2;mode++) for(opt=0;opt<2;opt++) for(clipv=0;clipv<2;clipv++) for(ri=0;ri<2;ri++) {
  int rx=rc[ri][0],ry=rc[ri][1],rr=rc[ri][2],rb=rc[ri][3],x0=rx-8,y0=ry-8,ww=rr-rx+16,hh=rb-ry+16,y,nb=0; DWORD o[4],ops0=0,ops1=0; RECT rect; UINT eo=ETO_OPAQUE|(opt?ETO_CLIPPED:0);
  rect.left=rx; rect.top=ry; rect.right=rr; rect.bottom=rb;
  SetBkMode(dc,mode?TRANSPARENT:OPAQUE); SetBkMode(ma,mode?TRANSPARENT:OPAQUE);
  SetBkColor(dc,RGB(255,255,0)); SetBkColor(ma,RGB(255,255,0)); SetTextColor(dc,RGB(255,0,0)); SetTextColor(ma,RGB(255,0,0));
  BitBlt(ma,x0,y0,ww,hh,dc,x0,y0,SRCCOPY);
  if(clipv) {IntersectClipRect(dc,rx+9,ry+7,rr-11,rb-6); IntersectClipRect(ma,rx+9,ry+7,rr-11,rb-6);}
  if(ctrlq(0xFFFFFFFEUL,o)) ops0=o[0];
  ExtTextOut(dc,rx+10,ry+30,eo,&rect,txt,(UINT)strlen(txt),NULL);
  if(ctrlq(0xFFFFFFFEUL,o)) ops1=o[0];
  ExtTextOut(ma,rx+10,ry+30,eo,&rect,txt,(UINT)strlen(txt),NULL);
  if(clipv) {SelectClipRgn(dc,NULL); SelectClipRgn(ma,NULL);}
  BitBlt(mb,x0,y0,ww,hh,dc,x0,y0,SRCCOPY);
  GetBitmapBits(bma,(DWORD)bm.bmWidthBytes*(DWORD)(y0+hh),bga); GetBitmapBits(bmb,(DWORD)bm.bmWidthBytes*(DWORD)(y0+hh),bgb);
  for(y=y0;y<y0+hh;y++) {int k; for(k=x0*bypp;k<(x0+ww)*bypp;k++) if(bga[(DWORD)y*bm.bmWidthBytes+k]!=bgb[(DWORD)y*bm.bmWidthBytes+k]) nb++;}
  cases++; if(ops1>ops0) hits++;
  if(nb) diff_report(mode?"transparent":"opaque",x0,y0,ww,hh,bypp,bm.bmWidthBytes);
  if(nb) {bad++; if(bad<=3) logline("    %s, %s%s, rect %d,%d,%d,%d: %d bytes differ from the software reference\n",mode?"transparent":"opaque",opt?"ETO_CLIPPED, ":"",clipv?"clip region":"no clip region",rx,ry,rr,rb,nb);}
 }
 SelectObject(ma,oa); SelectObject(mb,ob); DeleteObject(bma); DeleteObject(bmb); DeleteDC(ma); DeleteDC(mb);
 {int got,k; hesc(dc,VXH_STOP,NULL); memset(&hrep,0,sizeof(hrep)); got=hesc(dc,VXH_READ,&hrep);
  if(got==(int)sizeof(hrep)&&hrep.magic==VXH_MAGIC) {
   int nt=0; for(k=0;k<(int)hrep.count&&k<VXH_CAP;k++) if(hrep.records[k].rop==0x54584554UL) nt++;
   logline("  ExtTextOut diagnostic: the driver's ExtTextOut was called %d time(s) for %d draws (%lu records dropped)\n",nt,cases,hrep.dropped);
   {int shown=0; for(k=0;k<(int)hrep.count&&k<VXH_CAP&&shown<8;k++) {const vxh_record *t=&hrep.records[k]; if(t->rop!=0x54584554UL||!(t->flags&0x10)) continue; shown++;
    logline("    call %d: options %lX, conditions %03lX (1 opaque flag, 2 rect, 4 draw mode, 8 enabled, 10 screen device, 20 video memory, 40 not busy, 80 depth ok, 100 non-empty), engine answer %lX (1 done, 0 skip, FFFF none), rect %ld,%ld %ldx%ld, clip %s, draw mode bk %06lX\n",k,t->style,t->flags,t->hatch,(long)t->x,(long)t->y,(long)t->w,(long)t->h,t->raw_fg?"yes":"no",t->bk_color);}}
  } else logline("  ExtTextOut diagnostic: no capture reply (escape returned %d)\n",got);}
 logline("  ExtTextOut opaque rectangle: %d cases, %d reached the engine, %d differ from the software reference: %s\n",cases,hits,bad,bad?"FAIL":"PASS");
 if(bad) fails++;
 if(cases&&hits<cases) logline("  information: %d of %d opaque rectangles stayed in software (the eight cut by a clip region are under the engine's fill threshold)\n",cases-hits,cases);
 (void)f0;
}
/* v1.5: hatch capture. */
static int hesc(HDC dc,DWORD command,void *out) {
 vxh_command c; c.cb=sizeof(c); c.magic=VXH_MAGIC; c.command=command; c.reserved=0;
 return ExtEscape(dc,VXH_ESCAPE,sizeof(c),(LPCSTR)&c,out?(int)sizeof(vxh_reply):0,(LPSTR)out);
}
static void hatch_capture(HDC dc,DWORD w,DWORD h) {
 static const int hs[6]={HS_HORIZONTAL,HS_VERTICAL,HS_FDIAGONAL,HS_BDIAGONAL,HS_CROSS,HS_DIAGCROSS};
 static const char *hn[6]={"HORIZONTAL","VERTICAL","FDIAGONAL","BDIAGONAL","CROSS","DIAGCROSS"};
 static const int org[3][2]={{0,0},{3,5},{7,2}};
 static const char *pn[3]={"opaque, background green","opaque, background yellow (same brush)","transparent"};
 const COLORREF HATCH=RGB(255,0,0),TEXTC=RGB(0,0,255),BKA=RGB(0,255,0),BKB=RGB(255,255,0),DEST=RGB(128,0,128);
 HDC ma; HBITMAP bma,oa; int si,oi,pi,x,y,cases=0,nocap=0;
 const int HX=19,HY=21,HW=80,HH=48;
 if(w!=640) return;
 logline("  step: hatch capture (what DIBENG hands the driver; the tile is what the software reference drew at screen 24..31,24..31)\n");
 ma=CreateCompatibleDC(dc); bma=CreateCompatibleBitmap(dc,(int)w,(int)h);
 if(!ma||!bma) {logline("  hatch capture: could not create the memory bitmap\n"); return;}
 oa=(HBITMAP)SelectObject(ma,bma);
 if(hesc(dc,VXH_ARM,NULL)!=1) {logline("  hatch capture: the driver did not accept the capture escape (an older DRV?)\n"); SelectObject(ma,oa); DeleteObject(bma); DeleteDC(ma); return;}
 hesc(dc,VXH_STOP,NULL);
 for(si=0;si<6&&!aborted;si++) for(oi=0;oi<3;oi++) {
  HBRUSH hb,o1,o2,pb; RECT r; r.left=HX; r.top=HY; r.right=HX+HW; r.bottom=HY+HH;
  SetBrushOrgEx(dc,org[oi][0],org[oi][1],NULL); SetBrushOrgEx(ma,org[oi][0],org[oi][1],NULL);
  hb=CreateHatchBrush(hs[si],HATCH); UnrealizeObject(hb); o1=(HBRUSH)SelectObject(dc,hb); o2=(HBRUSH)SelectObject(ma,hb);
  for(pi=0;pi<3;pi++) {
   int got; char tile[8][9]; int rows;
   SetTextColor(dc,TEXTC); SetTextColor(ma,TEXTC);
   SetBkMode(dc,pi==2?TRANSPARENT:OPAQUE); SetBkMode(ma,pi==2?TRANSPARENT:OPAQUE);
   SetBkColor(dc,pi==1?BKB:BKA); SetBkColor(ma,pi==1?BKB:BKA);
   pb=CreateSolidBrush(DEST); FillRect(dc,&r,pb); FillRect(ma,&r,pb); DeleteObject(pb);
   hesc(dc,VXH_ARM,NULL);
   PatBlt(dc,HX,HY,HW,HH,PATCOPY);
   hesc(dc,VXH_STOP,NULL);
   PatBlt(ma,HX,HY,HW,HH,PATCOPY);
   memset(&hrep,0,sizeof(hrep)); got=hesc(dc,VXH_READ,&hrep);
   for(y=0;y<8;y++) {
    for(x=0;x<8;x++) {
     COLORREF c=GetPixel(ma,24+x,24+y); char ch='?';
     if(near_rgb(c,HATCH,8)) ch='H'; else if(near_rgb(c,pi==1?BKB:BKA,8)) ch='B'; else if(near_rgb(c,DEST,8)) ch='D'; else if(near_rgb(c,TEXTC,8)) ch='T';
     tile[y][x]=ch;
    }
    tile[y][8]=0;
   }
   cases++;
   if(got!=(int)sizeof(hrep)||hrep.magic!=VXH_MAGIC||hrep.count==0) {nocap++; logline("  %s origin (%d,%d), %s: no record (escape returned %d, count %lu)\n",hn[si],org[oi][0],org[oi][1],pn[pi],got,got==(int)sizeof(hrep)?hrep.count:0UL);}
   else {
    const vxh_record *q=&hrep.records[0];
    logline("  %s origin (%d,%d), %s: %lu record(s); flags %02lX style %lu hatch %lu brush bpp %lu raw fg %06lX bg %06lX; mono %08lX %08lX mask %08lX %08lX; bits %lX %lX %lX %lX %lX %lX %lX %lX; draw mode %s bkMode %lu text %06lX bk %06lX\n",
     hn[si],org[oi][0],org[oi][1],pn[pi],hrep.count,q->flags,q->style,q->hatch,q->brush_bpp,q->raw_fg,q->raw_bg,q->mono[0],q->mono[1],q->mask[0],q->mask[1],
     q->bits[0],q->bits[1],q->bits[2],q->bits[3],q->bits[4],q->bits[5],q->bits[6],q->bits[7],q->dm_valid?"valid":"ABSENT",q->bk_mode,q->text_color,q->bk_color);
   }
   logline("    tile:"); for(rows=0;rows<8;rows++) logline(" %s",tile[rows]); logline("\n");
  }
  SelectObject(dc,o1); SelectObject(ma,o2); DeleteObject(hb);
 }
 SetBrushOrgEx(dc,0,0,NULL); SetBkMode(dc,OPAQUE); SetBkColor(dc,RGB(255,255,255));
 SelectObject(ma,oa); DeleteObject(bma); DeleteDC(ma);
 logline("  hatch capture: %d fills, %d without a record%s\n",cases,nocap,nocap?" (information: the hook is only reached when the fill goes through the driver's BitBlt)":"");
}
/* v2.5: what does DIBENG hand the driver for a plain solid brush at 8bpp (true solid PatBlt fills never reached the engine there)? */
static void solid_capture(HDC dc,DWORD b) {
 static const COLORREF col[4]={RGB(255,0,0),RGB(128,128,128),RGB(0,0,0),RGB(100,150,200)};
 static const char *cn[4]={"red 255,0,0","grey 128,128,128","black","dithered 100,150,200"};
 int i,k;
 if(b!=8) return;
 logline("  step: solid brush capture at 8bpp (what DIBENG hands the driver)\n");
 if(hesc(dc,VXH_ARM,NULL)!=1) {logline("  solid capture: the driver did not accept the capture escape\n"); return;}
 hesc(dc,VXH_STOP,NULL);
 for(i=0;i<4;i++) {
  HBRUSH hb=CreateSolidBrush(col[i]),o1; DWORD o[4],ops0=0,ops1=0; int got;
  o1=(HBRUSH)SelectObject(dc,hb); UnrealizeObject(hb);
  hesc(dc,VXH_ARM,NULL);
  if(ctrlq(0xFFFFFFFEUL,o)) ops0=o[0];
  PatBlt(dc,40,30,200,100,PATCOPY);
  if(ctrlq(0xFFFFFFFEUL,o)) ops1=o[0];
  hesc(dc,VXH_STOP,NULL);
  memset(&hrep,0,sizeof(hrep)); got=hesc(dc,VXH_READ,&hrep);
  if(got!=(int)sizeof(hrep)||hrep.magic!=VXH_MAGIC||hrep.count==0) logline("  solid %s: no record (escape returned %d); %s\n",cn[i],got,ops1>ops0?"reached the engine":"did not reach the engine");
  else {
   const vxh_record *q=&hrep.records[0];
   logline("  solid %s: %s; flags %02lX style %lu brush bpp %lu raw fg %08lX bg %08lX; bits",cn[i],ops1>ops0?"reached the engine":"did NOT reach the engine",q->flags,q->style,q->brush_bpp,q->raw_fg,q->raw_bg);
   for(k=0;k<8;k++) logline(" %02lX",q->bits[k]);
   logline("; %lu of 64 bytes equal the first\n",q->eq64);
  }
  SelectObject(dc,o1); DeleteObject(hb);
 }
}
static int desk_set(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc;
 memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0);
 logline("  ChangeDisplaySettings(%lux%lux%lu, no registry change): %ld (0 done, 1 restart needed, -2 mode not supported, -1 failed)\n",w,h,b,(long)rc);
 return rc==DISP_CHANGE_SUCCESSFUL;
}

static int ctrlq(DWORD in,DWORD *out) {DWORD got=0; return DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&in,4,out,16,&got,NULL)&&got==16;}
/* scan line counter sanity scan: sample as fast as the driver call allows, check that it behaves like a line counter. Information only. */
static void scan_test(void) {
 static BYTE seen[4096]; DWORD r[VXF_REPLY_DWORDS],n,valid=0,blank=0,unav=0,oor=0,decr=0,last=0,minl=0xFFFFFFFFUL,maxl=0,height=0,gate=0,distinct=0,first[10],nf=0,i; int have=0,failed=0; double t0,t1,frames;
 logline("\nstep: scan line counter (register window offset 0x1290), information only\n");
 t0=now();
 for(n=0;n<2000&&!aborted;n++) {
  DWORD st,line;
  if(!flipctl(VXF_SCAN,0,0,r)||r[VXF_R_RESULT]!=VXF_OK) {failed=1; break;}
  height=r[VXF_S_HEIGHT]; gate=r[VXF_S_GATE]; st=r[VXF_S_STATUS];
  if(nf<10) first[nf++]=r[VXF_S_RAW];
  if(st==1) {
   valid++; line=r[VXF_S_LINE]; if(have&&line<last) decr++; last=line; have=1;
   if(line<minl) minl=line; if(line>maxl) maxl=line; if(line<4096&&!seen[line]) {seen[line]=1; distinct++;}
  } else if(st==2) blank++; else if(st==0) unav++; else oor++;
  if(now()-t0>600.0) break;
 }
 t1=now(); frames=(t1-t0)*60.0/1000.0;
 if(failed) {logline("  the scan request was refused by the driver (an older VxD?)\n"); return;}
 logline("  %lu samples in %.0f ms (about %.1f frames): valid %lu, in blank %lu, unavailable %lu, outside the picture %lu\n",n,t1-t0,frames,valid,blank,unav,oor);
 logline("  lines seen %lu..%lu of %lu, %lu distinct, %lu steps backwards (one per frame is expected)\n",valid?minl:0,maxl,height,distinct,decr);
 logline("  first raw register values:"); for(i=0;i<nf;i++) logline(" %08lX",first[i]); logline("\n");
 if(!valid&&unav==n) logline("  verdict: UNAVAILABLE (the register window did not read); GetScanLine stays off\n");
 else if(valid>=200&&maxl-minl>=height*3/4&&distinct>=height/4&&(double)decr<=frames+3.0) logline("  verdict: SANE, the counter behaves like a scan line; set SCANLINE=1 under HKLM\\Software\\VX85598 to offer GetScanLine (gate is %s)\n",gate?"ON":"off");
 else logline("  verdict: NOT SANE, the counter does not behave like a scan line; leave SCANLINE=0 (gate is %s)\n",gate?"ON":"off");
}
static void desktop_phase(DWORD desk_w,DWORD desk_h,DWORD desk_b) {
 static const DWORD sz[3][2]={{640,480},{800,600},{1024,768}};
 DWORD order[3],k,i,w,h,b,got=0; vx_status_t s; HDC dc; int n=0,total_fail=0;
 order[0]=desk_b; order[1]=desk_b==32?16:32; order[2]=8; if(desk_b==8) {order[0]=8; order[1]=16; order[2]=32;}
 logline("\n==== DESKTOP PHASE (the desktop is %lux%lux%lu) ====\n",desk_w,desk_h,desk_b);
 if(!aborted) scan_test();
 for(k=0;k<3&&!aborted;k++) for(i=0;i<3&&!aborted;i++) {
  int f0=(int)fails,ok,native=0; DWORD caps_bpp,caps_w,caps_h;
  w=sz[i][0]; h=sz[i][1]; b=order[k]; n++;
  logline("\n== DESKTOP %lux%lux%lu ==\nstep: switch\n",w,h,b);
  ok=desk_set(w,h,b);
  if(!ok) {fails++; logline("DESKTOP %lux%lux%lu result: FAIL (the mode change was refused)\n",w,h,b); continue;}
  wait_ms(1500);
  memset(&s,0,sizeof(s));
  if(DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)&&got==sizeof(s)&&s.magic==VX_MAGIC) {
   native=(s.width==w&&s.height==h&&s.bpp==b&&s.stage==6&&!s.failure&&!s.vga_mode);
   logline("  driver reports %lux%lux%lu stage %lu failure %lu vga_mode %lu: %s\n",(DWORD)s.width,(DWORD)s.height,(DWORD)s.bpp,(DWORD)s.stage,(DWORD)s.failure,(DWORD)s.vga_mode,native?"PASS":"FAIL"); if(!native) fails++;
  } else {logline("  driver status ioctl failed: FAIL\n"); fails++;}
  dc=GetDC(NULL);
  caps_bpp=(DWORD)GetDeviceCaps(dc,BITSPIXEL)*(DWORD)GetDeviceCaps(dc,PLANES); caps_w=(DWORD)GetDeviceCaps(dc,HORZRES); caps_h=(DWORD)GetDeviceCaps(dc,VERTRES);
  logline("  GetDeviceCaps: %lux%lux%lu: %s\n",caps_w,caps_h,caps_bpp,(caps_w==w&&caps_h==h&&caps_bpp==b)?"PASS":"FAIL"); if(!(caps_w==w&&caps_h==h&&caps_bpp==b)) fails++;
  if(!aborted) {
   DWORD o[4],ops0=0,ops1=0;
   if(ctrlq(0xFFFFFFFEUL,o)) ops0=o[0];
   desk_gdi(dc,w,h,b);
   if(!aborted) brush_tests(dc,w,h,b);
   if(!aborted) text_tests(dc,w,h,b);
   if(!aborted) rop_tests(dc,w,h,b);
   if(!aborted) hatch_capture(dc,w,h);
   if(!aborted) solid_capture(dc,b);
   if(ctrlq(0xFFFFFFFEUL,o)) ops1=o[0];
   logline("  2D engine operations during the GDI tests: %lu%s\n",ops1-ops0,(ops1==ops0)?" WARN (nothing reached the engine; ACCEL_2D may be 0 or the operations were below the size thresholds)":"");
   if(ctrlq(0xFFFFFE10UL,o)) {
    int off=(o[0]&1)&&(o[0]&6);
    logline("  GDI binding: bound %s, enabled word %s, DRV BUSY flag %s, requests that arrived while disabled %lu: %s\n",(o[0]&1)?"yes":"NO",(o[0]&2)?"0 (OFF)":"set",(o[0]&4)?"SET":"clear",o[1],off?"FAIL (GDI acceleration is switched off)":"PASS"); if(off) fails++;
   } else logline("  GDI binding query failed (an older VxD?)\n");
  }
  if(b==8&&!aborted) {
   PALETTEENTRY pe[256]; DWORD bits=0,rgb[256]; int j,bad=0; UINT cnt;
   logline("  step: system palette against the DAC\n");
   cnt=GetSystemPaletteEntries(dc,0,256,pe);
   if(cnt!=256) {logline("  GetSystemPaletteEntries returned %u entries: FAIL\n",cnt); fails++;}
   else if(!dac_read(&bits,rgb)) {logline("  DAC read-back refused: FAIL\n"); fails++;}
   else {
    for(j=0;j<256;j++) if(rgb[j]!=pack_raw(pe[j],bits)) {if(bad<4) logline("    entry %d: DAC %06lX, system palette %06lX\n",j,rgb[j],pack_raw(pe[j],bits)); bad++;}
    logline("  system palette against the DAC (%lu-bit): %s (%d of 256 entries differ)\n",bits,bad?"FAIL":"PASS",bad); if(bad) fails++;
   }
  }
  ReleaseDC(NULL,dc);
  total_fail+=(int)fails-f0;
  logline("DESKTOP %lux%lux%lu result: %s\n",w,h,b,((int)fails-f0)?"FAILURES":"all checks passed");
 }
 logline("\nputting the desktop back to %lux%lux%lu\n",desk_w,desk_h,desk_b);
 if(!desk_set(desk_w,desk_h,desk_b)) {logline("could not set the original mode: FAIL; asking Windows for the registry mode\n"); fails++; ChangeDisplaySettingsA(NULL,0);}
 wait_ms(1000);
 logline("DESKTOP PHASE: %d modes, %d failed checks\n",n,total_fail);
}
static HRESULT WINAPI enumcb(LPDDSURFACEDESC d,LPVOID ctx) {
 (void)ctx;
 if(noffered<256) {
  modeinfo *m=&offered[noffered++]; m->w=d->dwWidth; m->h=d->dwHeight; m->b=d->ddpfPixelFormat.dwRGBBitCount; m->hz=(d->dwFlags&DDSD_REFRESHRATE)?d->dwRefreshRate:0;
 }
 return DDENUMRET_OK;
}
static int is_offered(DWORD w,DWORD h,DWORD b) {int i; for(i=0;i<noffered;i++) if(offered[i].w==w&&offered[i].h==h&&offered[i].b==b) return 1; return 0;}
static int tried_already(DWORD w,DWORD h,DWORD b) {int i; for(i=0;i<nresults;i++) if(results[i].w==w&&results[i].h==h&&results[i].b==b) return 1; return 0;}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show_) {
 WNDCLASS wc; vx_status_t s; DWORD got,r[VXF_REPLY_DWORDS],desk_w,desk_h,desk_b; HMODULE mod; HRESULT hr; char text[300]; int i,k,listonly=cmd&&strstr(cmd,"list"),named=0,nodesktop=cmd&&strstr(cmd,"nodesktop"),ddran=0;
 static const DWORD sizes[][2]={{640,480},{800,600},{1024,768},{320,200},{320,240},{400,300},{512,384},{640,400},{1280,720},{1280,1024}};
 DWORD order[3]; DWORD wantw[48],wanth[48],wantb[48]; int nwant=0;
 typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*);
 (void)prev;(void)show_;
 logfile=fopen("VX2AUTO.TXT","w"); if(!logfile) return 1;
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC) {logline("The VX85598 driver is not active.\n"); goto cleanup;}
 desk_w=s.width; desk_h=s.height; desk_b=s.bpp; depth=s.bpp; sw=s.width; sh=s.height; QueryPerformanceFrequency(&freq);
 logline("VX2AUTO v2.7 / driver v10.89\nDesktop %lux%lux%lu pitch %lu\nvideo memory: %lu MB handed to DirectDraw, %lu MB mapped\n",desk_w,desk_h,desk_b,(DWORD)s.video_pitch,(DWORD)s.shadow_bytes/(1024UL*1024UL),(DWORD)s.fb_bytes/(1024UL*1024UL));
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2AUTO"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClass(&wc);
 win=CreateWindowEx(WS_EX_TOPMOST,wc.lpszClassName,"VX2AUTO - runs by itself, Esc aborts",WS_POPUP|WS_VISIBLE,0,0,(int)sw,(int)sh,NULL,NULL,hi,NULL);
 if(!win) goto cleanup;
 mod=LoadLibraryA("DDRAW.DLL"); if(!mod) goto cleanup;
 {CREATE create=(CREATE)GetProcAddress(mod,"DirectDrawCreate"); if(!create) goto cleanup; hr=create(NULL,&dd,NULL); logline("DirectDrawCreate: %08lX\n",(DWORD)hr); if(FAILED(hr)) {dd=NULL; goto cleanup;}}
 hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN); logline("SetCooperativeLevel(exclusive, fullscreen): %08lX\n",(DWORD)hr); if(FAILED(hr)) goto cleanup;
 hr=IDirectDraw_EnumDisplayModes(dd,0,NULL,NULL,enumcb); logline("EnumDisplayModes: %08lX, %d modes\n",(DWORD)hr,noffered);
 for(i=0;i<noffered;i++) logline("  offered: %lux%lux%lu refresh %lu\n",offered[i].w,offered[i].h,offered[i].b,offered[i].hz);
 if(listonly) goto summary;
 /* the modes to try: named on the command line, or the standard list in the order desktop depth, the other of 16/32, then 8 */
 if(cmd) {
  const char *p=cmd; DWORD w,h,b; int n;
  while(*p) {
   while(*p==' ') p++;
   if(sscanf(p,"%lux%lux%lu%n",&w,&h,&b,&n)==3&&nwant<48) {wantw[nwant]=w; wanth[nwant]=h; wantb[nwant]=b; nwant++; named=1; p+=n;}
   else {while(*p&&*p!=' ') p++;}
  }
 }
 if(!named) {
  order[0]=desk_b; order[1]=desk_b==32?16:32; order[2]=8; if(desk_b==8) {order[0]=8; order[1]=16; order[2]=32;}
  for(k=0;k<3;k++) for(i=0;i<(int)(sizeof(sizes)/sizeof(sizes[0]))&&nwant<48;i++) {wantw[nwant]=sizes[i][0]; wanth[nwant]=sizes[i][1]; wantb[nwant]=order[k]; nwant++;}
 }
 ddran=1;
 for(i=0;i<nwant&&!aborted;i++) {
  if(tried_already(wantw[i],wanth[i],wantb[i])) continue;
  if(!is_offered(wantw[i],wanth[i],wantb[i])) {logline("\n%lux%lux%lu: not offered by DirectDraw, skipped\n",wantw[i],wanth[i],wantb[i]); continue;}
  testmode(wantw[i],wanth[i],wantb[i]);
 }
 logline("\nreturning to the desktop mode\n");
 IDirectDraw_RestoreDisplayMode(dd);
summary:
 logline("\nSUMMARY\n");
 for(i=0;i<nresults;i++) logline("%4lux%-4lux%-2lu set %s, native %s, failed checks %d, flip %s\n",results[i].w,results[i].h,results[i].b,results[i].setok?"ok":"FAILED",results[i].native?"yes":"NO",results[i].fails,results[i].flip==1?"pass":results[i].flip==2?"FAIL":results[i].flip==3?"not offered":"not tried");
 logline("%s (%lu failed checks, %d modes tried)\n",fails?"FAILURES":"all checks passed",fails,nresults);
cleanup:
 if(pal) {IDirectDrawPalette_Release(pal); pal=NULL;}
 if(dev!=INVALID_HANDLE_VALUE) {flipctl(VXF_RESET,0,0,r);}
 if(dd) {IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd);}
 if(win) {DestroyWindow(win); win=NULL; wait_ms(300);}
 if(dev!=INVALID_HANDLE_VALUE&&ddran&&!nodesktop&&!aborted) desktop_phase(desk_w,desk_h,desk_b);
 if(dev!=INVALID_HANDLE_VALUE) CloseHandle(dev);
 logline("Finished%s.\n",aborted?" (aborted)":""); fclose(logfile);
 {FILE *rf=fopen("VX2AUTO.RES","w"); if(rf) {if(aborted) fprintf(rf,"ABORTED (%lu failed checks so far)\r\n",fails); else if(fails) fprintf(rf,"FAIL (%lu failed checks)\r\n",fails); else fprintf(rf,"PASS\r\n"); fclose(rf);}}
 (void)text;
 return fails?1:0;
}
