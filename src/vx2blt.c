/* VX2BLT v1.0 (driver v10.321): where a program's frame lands on the screen, for the paths Quake II's software renderer uses. Needs VX85598.DRV v10.321 or later
 * for the blit capture (ExtEscape 8F72, command 5); without it the pixel checks still run.
 *   VX2BLT          unattended tests at the current desktop depth, then VX2BLT.TXT / VX2BLT.RES:
 *     T1  windowed: an 8-bit DIB section (its own 16-colour table) put into a window at a known place with BitBlt from a memory DC, as Quake II's windowed
 *         software renderer does; then the same with the window's DC taken before the window was moved (the DC Quake II keeps), from a CS_OWNDC window, and the
 *         same pixels with StretchDIBits and SetDIBitsToDevice. Each: the DDI calls the display driver received (destination, source, clip, who drew it) and
 *         a pixel check: in the window, at the screen's top left, or nowhere.
 *     T2  full screen: DirectDraw exclusive mode 640x480 at 8 bits (Quake II's full-screen software renderer): SetDisplayMode, then what DirectDraw, GDI and the
 *         primary surface say the mode is, a pattern written to the primary, and the mode put back.
 *   VX2BLT ARM      starts the blit capture and exits: run the program (Quake II), quit it, then
 *   VX2BLT READ     writes the captured calls (the first 32) to VX2BLT.TXT and stops the capture.
 * Esc stops. */
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx_hatch_diag.h"

static FILE *lf; static int aborted,fails,checks,in_win; static vxh_reply rep;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2BLT.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1; return !aborted;}
static void wait_ms(DWORD ms) {DWORD t=GetTickCount(); while(GetTickCount()-t<ms&&pump()) Sleep(10);}
static int hesc(DWORD command,void *out) {
 vxh_command c; HDC dc=GetDC(NULL); int r;
 c.cb=sizeof(c); c.magic=VXH_MAGIC; c.command=command; c.reserved=0;
 r=ExtEscape(dc,VXH_ESCAPE,sizeof(c),(LPCSTR)&c,out?(int)sizeof(vxh_reply):0,(LPSTR)out); ReleaseDC(NULL,dc); return r;
}
static const char *tname(DWORD t) {return t==VXH_TAG_BBLT?"BitBlt":t==VXH_TAG_SBLT?"StretchBlt":t==VXH_TAG_XDIB?"StretchDIBits":t==VXH_TAG_XDTD?"DibToDevice":"?";}
static const char *route(DWORD r) {return r==0xFFFF?"DIB engine (driver did not try)":r==0?"DIB engine (driver declined)":r==1?"driver (engine)":r==2?"driver: FAILED":r==3?"driver: LOST":"?";}
static void show_capture(const char *what) {
 DWORD i;
 memset(&rep,0,sizeof(rep));
 if(hesc(VXH_READ,&rep)<=0||rep.magic!=VXH_MAGIC) {logline("  %s: the capture escape is not answered (DRV older than v10.321)\n",what); return;}
 hesc(VXH_STOP,NULL);
 logline("  %s: %lu calls recorded%s\n",what,rep.count,rep.dropped?" (more were dropped)":"");
 for(i=0;i<rep.count&&i<VXH_CAP;i++) {const vxh_record *t=&rep.records[i];
  logline("    %s: destination (%ld,%ld) %lux%lu, source (%lu,%lu) of %ldx%ld, %lu bpp, row bytes %lu, flags %08lX, ROP %08lX; %s, DIB engine returned %ld\n",tname(t->rop),(long)t->x,(long)t->y,t->w,t->h,
   t->style&0xFFFF,t->style>>16,(long)t->bits[0],(long)t->bits[1],t->brush_bpp,t->bits[2],t->flags,t->raw_fg,route(t->hatch),(long)t->text_color);
  logline("      clip %s",t->dm_valid?"":"none");
  if(t->dm_valid) logline("(%d,%d)-(%d,%d)",(int)(short)(t->mask[0]&0xFFFF),(int)(short)(t->mask[0]>>16),(int)(short)(t->mask[1]&0xFFFF),(int)(short)(t->mask[1]>>16));
  logline("; screen %lux%lu at %lu bpp, pitch %lu; destination flags %04lX%s; source bits at %08lX; extra %08lX %08lX, colours used %lu\n",t->mono[0]&0xFFFF,t->mono[0]>>16,t->bpp,t->mono[1],t->bits[4],t->bits[6]?" (the screen)":" (NOT the screen)",t->bits[3],t->raw_bg,t->bits[7],t->bits[5]);}
}

/* ---- the picture: 320x240, 16 colours in 40x30 blocks; colours exact at 5-6-5 */
#define PW 320
#define PH 240
static const BYTE pal16[16][3]={{0,0,0},{248,0,0},{0,252,0},{0,0,248},{248,252,0},{248,0,248},{0,252,248},{248,252,248},{128,0,0},{0,128,0},{0,0,128},{128,128,0},{128,0,128},{0,128,128},{192,192,192},{64,64,64}};
static int index_at(int x,int y) {return ((x/40)+(y/30)*3)&15;}
static int near_rgb(COLORREF c,int k) {int d=abs((int)GetRValue(c)-pal16[k][0])+abs((int)GetGValue(c)-pal16[k][1])+abs((int)GetBValue(c)-pal16[k][2]); return d<=24;}
static struct {BITMAPINFOHEADER h; RGBQUAD q[256];} bi;
static void make_info(int topdown) {
 int k; memset(&bi,0,sizeof(bi)); bi.h.biSize=sizeof(bi.h); bi.h.biWidth=PW; bi.h.biHeight=topdown?-PH:PH; bi.h.biPlanes=1; bi.h.biBitCount=8; bi.h.biClrUsed=256;
 for(k=0;k<16;k++) {bi.q[k].rgbRed=pal16[k][0]; bi.q[k].rgbGreen=pal16[k][1]; bi.q[k].rgbBlue=pal16[k][2];}
}
static void fill(BYTE *bits,int topdown) {int x,y; for(y=0;y<PH;y++) {BYTE *row=bits+(topdown?y:PH-1-y)*PW; for(x=0;x<PW;x++) row[x]=(BYTE)index_at(x,y);}}

static LRESULT CALLBACK wproc(HWND w,UINT m,WPARAM a,LPARAM b) {if(m==WM_ERASEBKGND) return 1; return DefWindowProc(w,m,a,b);}
static HWND make_window(const char *cls,int x,int y) {
 RECT r; HWND w; r.left=0; r.top=0; r.right=PW; r.bottom=PH; AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION,FALSE);
 w=CreateWindowEx(WS_EX_TOPMOST,cls,"VX2BLT",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,x,y,r.right-r.left,r.bottom-r.top,NULL,NULL,GetModuleHandle(NULL),NULL);
 if(w) {SetForegroundWindow(w); UpdateWindow(w); wait_ms(300);}
 return w;
}
/* the window's client area on the screen against the picture; the screen's top left against what it showed before */
static COLORREF tl_before[3]; static const POINT tl_pts[3]={{20,15},{60,45},{100,75}}; /* picture blocks 0, 1+3 = 4, 2+6 = 8 at the screen's top left if it lands there */
static void snap_tl(void) {HDC s=GetDC(NULL); int i; for(i=0;i<3;i++) tl_before[i]=GetPixel(s,tl_pts[i].x,tl_pts[i].y); ReleaseDC(NULL,s);}
static void verdict(const char *what,HWND w) {
 HDC s=GetDC(NULL); POINT o={0,0}; int i,in=0,tl=0,tlchg=0; static const POINT p[6]={{20,15},{140,45},{260,105},{60,135},{180,195},{300,225}};
 ClientToScreen(w,&o);
 for(i=0;i<6;i++) if(near_rgb(GetPixel(s,o.x+p[i].x,o.y+p[i].y),index_at(p[i].x,p[i].y))) in++;
 for(i=0;i<3;i++) {COLORREF c=GetPixel(s,tl_pts[i].x,tl_pts[i].y); if(c!=tl_before[i]) tlchg++; if(near_rgb(c,index_at(tl_pts[i].x,tl_pts[i].y))) tl++;}
 ReleaseDC(NULL,s); checks++;
 logline("  %s: client area at screen (%ld,%ld): %d of 6 picture points in the window; screen top left: %d of 3 points changed, %d of 3 show the picture -> %s\n",what,o.x,o.y,in,tlchg,tl,
  in==6?"IN THE WINDOW":tl>=2&&!(o.x<PW&&o.y<PH)?"AT THE SCREEN'S TOP LEFT":"NOT SHOWN (or elsewhere)");
 if(in!=6) fails++; else in_win++;
}
static void t1(void) {
 WNDCLASS wc; HWND w; HDC dc,mem; HBITMAP sec,old; BYTE *bits=NULL; int scr=0;
 HDC s=GetDC(NULL); int bpp=GetDeviceCaps(s,BITSPIXEL)*GetDeviceCaps(s,PLANES); ReleaseDC(NULL,s);
 logline("T1 windowed, desktop %dx%d at %d bpp\n",GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),bpp);
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wproc; wc.hInstance=GetModuleHandle(NULL); wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)GetStockObject(GRAY_BRUSH); wc.lpszClassName="VX2BLTW"; RegisterClass(&wc);
 wc.style=CS_OWNDC; wc.lpszClassName="VX2BLTO"; RegisterClass(&wc);
 make_info(0);
 /* a: DIB section, BitBlt from a memory DC, DC taken after the window is placed */
 w=make_window("VX2BLTW",200,150); if(!w) {logline("  CreateWindow failed %lu\n",GetLastError()); fails++; return;}
 dc=GetDC(w); sec=CreateDIBSection(dc,(BITMAPINFO*)&bi,DIB_RGB_COLORS,(void**)&bits,NULL,0);
 if(!sec||!bits) {logline("  CreateDIBSection failed %lu\n",GetLastError()); fails++; ReleaseDC(w,dc); DestroyWindow(w); return;}
 fill(bits,0); GdiFlush(); mem=CreateCompatibleDC(dc); old=(HBITMAP)SelectObject(mem,sec);
 snap_tl(); hesc(VXH_ARM_BLITS,NULL);
 if(!BitBlt(dc,0,0,PW,PH,mem,0,0,SRCCOPY)) logline("  BitBlt returned FALSE (%lu)\n",GetLastError());
 GdiFlush(); show_capture("a  8-bit DIB section, BitBlt to the window's DC"); verdict("a",w);
 /* b: the same DC after the window moved (Quake II keeps its window DC) */
 SetWindowPos(w,NULL,260,190,0,0,SWP_NOSIZE|SWP_NOZORDER); wait_ms(300);
 snap_tl(); hesc(VXH_ARM_BLITS,NULL);
 BitBlt(dc,0,0,PW,PH,mem,0,0,SRCCOPY); GdiFlush(); show_capture("b  the same DC after SetWindowPos to (260,190)"); verdict("b",w);
 /* c: StretchDIBits and SetDIBitsToDevice from the same bits */
 snap_tl(); hesc(VXH_ARM_BLITS,NULL);
 StretchDIBits(dc,0,0,PW,PH,0,0,PW,PH,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS,SRCCOPY); GdiFlush(); show_capture("c  StretchDIBits 1:1"); verdict("c",w);
 snap_tl(); hesc(VXH_ARM_BLITS,NULL);
 SetDIBitsToDevice(dc,0,0,PW,PH,0,0,0,PH,bits,(BITMAPINFO*)&bi,DIB_RGB_COLORS); GdiFlush(); show_capture("d  SetDIBitsToDevice"); verdict("d",w);
 SelectObject(mem,old); DeleteDC(mem); ReleaseDC(w,dc); DestroyWindow(w); wait_ms(200);
 /* e: a CS_OWNDC window */
 w=make_window("VX2BLTO",180,170);
 if(w) {dc=GetDC(w); mem=CreateCompatibleDC(dc); old=(HBITMAP)SelectObject(mem,sec); snap_tl(); hesc(VXH_ARM_BLITS,NULL);
  BitBlt(dc,0,0,PW,PH,mem,0,0,SRCCOPY); GdiFlush(); show_capture("e  CS_OWNDC window, BitBlt"); verdict("e",w);
  SelectObject(mem,old); DeleteDC(mem); ReleaseDC(w,dc); DestroyWindow(w);}
 DeleteObject(sec); (void)scr;
}
/* ---- T2: DirectDraw exclusive 640x480x8 */
typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*);
static void t2(void) {
 HMODULE mod=LoadLibrary("DDRAW.DLL"); LPDIRECTDRAW dd=NULL; LPDIRECTDRAWSURFACE prim=NULL; LPDIRECTDRAWPALETTE pal=NULL; HRESULT hr; HWND w; DDSURFACEDESC sd; PALETTEENTRY pe[256]; int k,x,y,ok=0;
 WNDCLASS wc;
 logline("T2 full screen: DirectDraw exclusive 640x480 at 8 bits\n");
 if(!mod) {logline("  no DDRAW.DLL\n"); fails++; return;}
 {CREATE create=(CREATE)GetProcAddress(mod,"DirectDrawCreate"); if(!create||FAILED(hr=create(NULL,&dd,NULL))) {logline("  DirectDrawCreate failed\n"); fails++; return;}}
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wproc; wc.hInstance=GetModuleHandle(NULL); wc.hbrBackground=(HBRUSH)GetStockObject(GRAY_BRUSH); wc.lpszClassName="VX2BLTF"; RegisterClass(&wc);
 w=CreateWindowEx(WS_EX_TOPMOST,"VX2BLTF","VX2BLT",WS_POPUP|WS_VISIBLE,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,GetModuleHandle(NULL),NULL);
 SetForegroundWindow(w); wait_ms(200);
 hr=IDirectDraw_SetCooperativeLevel(dd,w,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN|DDSCL_ALLOWMODEX); logline("  SetCooperativeLevel(exclusive, full screen, mode X): %08lX\n",(DWORD)hr);
 hr=IDirectDraw_SetDisplayMode(dd,640,480,8); logline("  SetDisplayMode(640, 480, 8): %08lX\n",(DWORD)hr);
 if(FAILED(hr)) {fails++; goto out;}
 wait_ms(500);
 memset(&sd,0,sizeof(sd)); sd.dwSize=sizeof(sd); hr=IDirectDraw_GetDisplayMode(dd,&sd);
 logline("  DirectDraw says the mode is %lux%lu at %lu bits, pitch %ld (%08lX)\n",sd.dwWidth,sd.dwHeight,sd.ddpfPixelFormat.dwRGBBitCount,sd.lPitch,(DWORD)hr);
 {HDC s=GetDC(NULL); logline("  GDI says %dx%d at %d bits\n",GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),GetDeviceCaps(s,BITSPIXEL)*GetDeviceCaps(s,PLANES)); ReleaseDC(NULL,s);}
 memset(&sd,0,sizeof(sd)); sd.dwSize=sizeof(sd); sd.dwFlags=DDSD_CAPS; sd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
 hr=IDirectDraw_CreateSurface(dd,&sd,&prim,NULL); logline("  CreateSurface(primary): %08lX\n",(DWORD)hr); if(FAILED(hr)) {fails++; goto out;}
 for(k=0;k<256;k++) {pe[k].peRed=k<16?pal16[k][0]:0; pe[k].peGreen=k<16?pal16[k][1]:0; pe[k].peBlue=k<16?pal16[k][2]:0; pe[k].peFlags=0;}
 hr=IDirectDraw_CreatePalette(dd,DDPCAPS_8BIT|DDPCAPS_ALLOW256,pe,&pal,NULL); logline("  CreatePalette: %08lX\n",(DWORD)hr);
 if(pal) {hr=IDirectDrawSurface_SetPalette(prim,pal); logline("  SetPalette: %08lX\n",(DWORD)hr);}
 memset(&sd,0,sizeof(sd)); sd.dwSize=sizeof(sd); hr=IDirectDrawSurface_Lock(prim,NULL,&sd,DDLOCK_WAIT,NULL);
 logline("  Lock(primary): %08lX: %lux%lu at %lu bits, pitch %ld, memory %p\n",(DWORD)hr,sd.dwWidth,sd.dwHeight,sd.ddpfPixelFormat.dwRGBBitCount,sd.lPitch,sd.lpSurface);
 if(SUCCEEDED(hr)&&sd.lpSurface) {
  for(y=0;y<480;y++) {BYTE *row=(BYTE*)sd.lpSurface+y*sd.lPitch; for(x=0;x<640;x++) row[x]=(BYTE)index_at(x/2,y/2);}
  IDirectDrawSurface_Unlock(prim,sd.lpSurface); ok=1;
  logline("  the pattern (16 colour blocks of 80x60, the same blocks as T1 at twice the size) is on the screen for 3 seconds: it should fill the whole screen\n");
  wait_ms(3000);
 }
out:
 if(pal) IDirectDrawPalette_Release(pal); if(prim) IDirectDrawSurface_Release(prim);
 hr=IDirectDraw_RestoreDisplayMode(dd); logline("  RestoreDisplayMode: %08lX\n",(DWORD)hr); IDirectDraw_SetCooperativeLevel(dd,w,DDSCL_NORMAL); IDirectDraw_Release(dd);
 DestroyWindow(w); wait_ms(500);
 {HDC s=GetDC(NULL); logline("  after: GDI says %dx%d at %d bits\n",GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),GetDeviceCaps(s,BITSPIXEL)*GetDeviceCaps(s,PLANES)); ReleaseDC(NULL,s);}
 (void)ok;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 char res[160]; (void)hi; (void)hp; (void)show;
 lf=fopen("VX2BLT.TXT","w"); if(!lf) return 1;
 logline("VX2BLT v1.0 (driver v10.321)\n");
 if(cmd&&!_strnicmp(cmd,"ARM",3)) {int r=hesc(VXH_ARM_BLITS,NULL); logline("blit capture %s: run the program, then VX2BLT READ\n",r>0?"started":"NOT started (DRV older than v10.321)"); finish(r>0?"ARMED":"FAIL"); fclose(lf); return 0;}
 if(cmd&&!_strnicmp(cmd,"READ",4)) {show_capture("what the display driver received since VX2BLT ARM"); finish("READ"); fclose(lf); return 0;}
 finish("RUNNING");
 t1(); if(!aborted) t2();
 sprintf(res,"%s: %d of %d window checks with the picture in the window; full screen: see VX2BLT.TXT",aborted?"ABORTED":fails?"FAIL":"PASS",in_win,checks);
 logline("\nRESULT %s\n",res); finish(res); fclose(lf); return 0;
}
