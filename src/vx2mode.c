/* VX2MODE v1.1: display-mode sweep through DirectDraw SetDisplayMode at 8, 16 and 32 bits per pixel. Writes VX2MODE.TXT.
 * For every mode DirectDraw offers (from a list of sizes games use) it: switches to the mode, asks the driver what mode it is
 * really in, draws a test screen, checks DirectDraw blits and GDI drawing (copy in all directions, pattern brush, monochrome
 * expansion) pixel by pixel against the screen's own pixels, and tries a flip chain. Every step is written to the log BEFORE it
 * runs, so if the machine hangs the last line says where. Esc aborts between steps; the desktop mode is restored at the end.
 * Command line: "list" = only list the modes; "640x480x16 800x600x32 ..." = only those modes; nothing = the standard list. */
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
 logline("DirectDraw hardware HAL active: %s; caps %08lX surface caps %08lX\n",(c.dwCaps&0x02000000UL)?"NO (emulation only)":"yes",c.dwCaps,c.ddsCaps.dwCaps);
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
 hr=IDirectDraw_CreateSurface(dd,&d,&prim,NULL); logline("CreateSurface(primary): %08lX\n",(DWORD)hr);
 if(FAILED(hr)||!prim) {fails++; R->fails=fails-f0; return;}
 setpal(prim);
 {DDSURFACEDESC l; if(lock_surf(prim,&l)) {logline("primary lock: pitch %ld, pixel format bits %lu\n",(long)l.lPitch,l.ddpfPixelFormat.dwRGBBitCount); IDirectDrawSurface_Unlock(prim,l.lpSurface);} else {logline("primary lock failed\n"); fails++;}}
 logline("step: draw the test picture\n");
 picture(prim,w,h,b); scan("picture drawn"); wait_ms(3000); scan("after 3 seconds on the picture");
 if(!aborted) blits(prim);
 if(!aborted&&h>=240&&w>=320) gdi_tests(prim); else if(!aborted) logline("step: GDI tests skipped (screen too small)\n");
 scan("after blits and GDI tests"); if(!aborted) wait_ms(300);
 IDirectDrawSurface_Release(prim); prim=NULL;
 if(!aborted) flips(b,&R->flip); else R->flip=0;
 scan("after the flip chain");
 R->fails=fails-f0;
 logline("MODE %lux%lux%lu result: %s\n",w,h,b,R->fails?"FAILURES":"all checks passed");
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
 WNDCLASS wc; vx_status_t s; DWORD got,r[VXF_REPLY_DWORDS],desk_w,desk_h,desk_b; HMODULE mod; HRESULT hr; char text[300]; int i,k,listonly=cmd&&strstr(cmd,"list"),named=0;
 static const DWORD sizes[][2]={{640,480},{800,600},{1024,768},{320,200},{320,240},{400,300},{512,384},{640,400},{1280,720},{1280,1024}};
 DWORD order[3]; DWORD wantw[48],wanth[48],wantb[48]; int nwant=0;
 typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*);
 (void)prev;(void)show_;
 logfile=fopen("VX2MODE.TXT","w"); if(!logfile) return 1;
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC) {logline("The VX85598 driver is not active.\n"); goto cleanup;}
 desk_w=s.width; desk_h=s.height; desk_b=s.bpp; depth=s.bpp; sw=s.width; sh=s.height; QueryPerformanceFrequency(&freq);
 logline("VX2MODE v1.1 / driver v10.61\nDesktop %lux%lux%lu pitch %lu\n",desk_w,desk_h,desk_b,(DWORD)s.video_pitch);
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2MODE"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClass(&wc);
 win=CreateWindowEx(WS_EX_TOPMOST,wc.lpszClassName,"VX2MODE - Esc aborts",WS_POPUP|WS_VISIBLE,0,0,(int)sw,(int)sh,NULL,NULL,hi,NULL);
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
 if(win) DestroyWindow(win);
 if(dev!=INVALID_HANDLE_VALUE) CloseHandle(dev);
 logline("Finished%s.\n",aborted?" (aborted)":""); fclose(logfile);
 sprintf(text,"%s Read VX2MODE.TXT. Did every mode show eight colour bars with a thin white border and its name in the corner? Tell me which modes looked wrong.",fails?"Some checks failed.":"Checks finished.");
 MessageBox(NULL,text,"VX2MODE",MB_OK);
 return fails?1:0;
}
