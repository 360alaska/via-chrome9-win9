/* VX2OVL8 v1.0: the video overlay over an 8bpp desktop (driver v10.130: 0x220 bits 7:0 = the palette index). Exclusive DirectDraw 640x480x8 with its own palette:
 * background index 1, a LEFT box of index 2 and a RIGHT box of index 3 with exactly the same grey, a YUY2 colour-bar overlay over most of the screen.
 * 1 key 2 (override): bars only in the left box; 2 palette entry 2 turned red: the keyed area must not move; 3 stored key 3 (SetColorKey + DDOVER_KEYDEST): bars only in
 * the right box; 4 key 1: bars everywhere except the two boxes; 5 hide. Each is judged by eye with the Y / N keys (60 s each). Results in VX2OVL8.TXT / .RES. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <io.h>
#include <ddraw.h>

static FILE *lf; static HWND win; static LPDIRECTDRAW dd; static LPDIRECTDRAWSURFACE prim,ovl; static LPDIRECTDRAWPALETTE pal; static PALETTEENTRY pe[256];
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2OVL8.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}}
static void wait_ms(int ms) {DWORD t=GetTickCount(); while((int)(GetTickCount()-t)<ms) {pump(); Sleep(5);}}
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {if(m==WM_SETCURSOR) {SetCursor(NULL); return TRUE;} if(m==WM_ERASEBKGND) return 1; return DefWindowProc(w,m,a,b);}
static int key_yn(void) {DWORD t=GetTickCount(); while(GetAsyncKeyState('Y')&0x8000||GetAsyncKeyState('N')&0x8000) {pump(); Sleep(20);} for(;;) {pump(); if(GetAsyncKeyState('Y')&0x8000) return 1; if(GetAsyncKeyState('N')&0x8000) return 0; if(GetTickCount()-t>60000) return 0; Sleep(20);}}
static void text(const char *s) { /* the question on the top band (index 1 background, the overlay starts below it) */
 HDC dc; RECT r; HBRUSH b;
 if(FAILED(IDirectDrawSurface_GetDC(prim,&dc))) return;
 r.left=0; r.top=0; r.right=640; r.bottom=38; b=CreateSolidBrush(RGB(0,0,0)); FillRect(dc,&r,b); DeleteObject(b);
 SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(255,255,255)); TextOut(dc,8,4,s,(int)strlen(s)); TextOut(dc,8,20,"press Y (yes) or N (no)",23);
 IDirectDrawSurface_ReleaseDC(prim,dc);
}
static int ask(const char *q) {int r; text(q); logline("   question: %s\n",q); r=key_yn(); logline("   answer: %s\n",r?"yes":"no"); return r;}
static void paint_primary(void) { /* raw index bytes: no GDI colour matching or dithering */
 DDSURFACEDESC l; int y,x;
 memset(&l,0,sizeof(l)); l.dwSize=sizeof(l);
 if(FAILED(IDirectDrawSurface_Lock(prim,NULL,&l,DDLOCK_WAIT,NULL))) {logline("   primary Lock failed\n"); return;}
 for(y=0;y<480;y++) {BYTE *p=(BYTE*)l.lpSurface+(DWORD)y*(DWORD)l.lPitch; for(x=0;x<640;x++) {BYTE v=1; if(y>=100&&y<380) {if(x>=40&&x<300) v=2; else if(x>=340&&x<600) v=3;} p[x]=v;}}
 IDirectDrawSurface_Unlock(prim,l.lpSurface);
}
static const BYTE bars[8][3]={{235,128,128},{210,16,146},{170,166,16},{145,54,34},{106,202,222},{81,90,240},{41,240,110},{16,128,128}};
static int show(DWORD flags,DWORD key) {
 RECT s,d; DDOVERLAYFX fx; HRESULT hr;
 s.left=0; s.top=0; s.right=320; s.bottom=240; d.left=0; d.top=40; d.right=640; d.bottom=440;
 memset(&fx,0,sizeof(fx)); fx.dwSize=sizeof(fx); fx.dckDestColorkey.dwColorSpaceLowValue=fx.dckDestColorkey.dwColorSpaceHighValue=key;
 hr=IDirectDrawSurface_UpdateOverlay(ovl,&s,prim,&d,DDOVER_SHOW|flags,(flags&DDOVER_KEYDESTOVERRIDE)?&fx:NULL);
 logline("   UpdateOverlay flags %08lX key %lu: %08lX\n",flags,key,(DWORD)hr); wait_ms(300); return SUCCEEDED(hr);
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int sh) {
 WNDCLASSA wc; DDSURFACEDESC d; DDCAPS caps,hel; DDCOLORKEY ck; HRESULT hr; int i,pass=0,total=0,y,x; char res[200]; (void)hp; (void)cmd; (void)sh;
 lf=fopen("VX2OVL8.TXT","w"); logline("VX2OVL8 v1.0 / driver v10.130\n"); finish("RUNNING: the run did not reach its end (VX2OVL8.TXT ends where it stopped)");
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2OVL8"; RegisterClassA(&wc);
 win=CreateWindowExA(WS_EX_TOPMOST,"VX2OVL8","VX2OVL8",WS_POPUP|WS_VISIBLE,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,hi,NULL); SetForegroundWindow(win); wait_ms(300);
 hr=DirectDrawCreate(NULL,&dd,NULL); logline("DirectDrawCreate: %08lX\n",(DWORD)hr); if(FAILED(hr)) {finish("FAIL: DirectDraw"); return 1;}
 hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN); logline("SetCooperativeLevel: %08lX\n",(DWORD)hr); if(FAILED(hr)) {finish("FAIL: exclusive mode"); goto out;}
 hr=IDirectDraw_SetDisplayMode(dd,640,480,8); logline("SetDisplayMode(640x480x8): %08lX\n",(DWORD)hr); if(FAILED(hr)) {finish("FAIL: 640x480x8"); goto out;}
 memset(&caps,0,sizeof(caps)); caps.dwSize=sizeof(caps); memset(&hel,0,sizeof(hel)); hel.dwSize=sizeof(hel); IDirectDraw_GetCaps(dd,&caps,&hel);
 logline("caps at 8bpp: overlay %s, destination overlay key %s\n",(caps.dwCaps&DDCAPS_OVERLAY)?"yes":"NO",(caps.dwCKeyCaps&DDCKEYCAPS_DESTOVERLAY)?"yes":"NO");
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE; hr=IDirectDraw_CreateSurface(dd,&d,&prim,NULL); logline("primary: %08lX\n",(DWORD)hr); if(FAILED(hr)) {finish("FAIL: primary"); goto out;}
 memset(pe,0,sizeof(pe)); for(i=0;i<256;i++) {pe[i].peRed=(BYTE)i; pe[i].peGreen=(BYTE)i; pe[i].peBlue=(BYTE)i;}
 pe[0].peRed=pe[0].peGreen=pe[0].peBlue=0; pe[1].peRed=40; pe[1].peGreen=40; pe[1].peBlue=120; pe[2].peRed=pe[2].peGreen=pe[2].peBlue=128; pe[3]=pe[2]; pe[255].peRed=pe[255].peGreen=pe[255].peBlue=255;
 hr=IDirectDraw_CreatePalette(dd,DDPCAPS_8BIT|DDPCAPS_ALLOW256,pe,&pal,NULL); logline("CreatePalette: %08lX\n",(DWORD)hr); if(SUCCEEDED(hr)) IDirectDrawSurface_SetPalette(prim,pal);
 paint_primary();
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT; d.dwWidth=320; d.dwHeight=240; d.ddsCaps.dwCaps=DDSCAPS_OVERLAY|DDSCAPS_VIDEOMEMORY;
 d.ddpfPixelFormat.dwSize=sizeof(d.ddpfPixelFormat); d.ddpfPixelFormat.dwFlags=DDPF_FOURCC; d.ddpfPixelFormat.dwFourCC=0x32595559UL;
 hr=IDirectDraw_CreateSurface(dd,&d,&ovl,NULL); logline("YUY2 overlay surface 320x240: %08lX\n",(DWORD)hr); if(FAILED(hr)) {finish("FAIL: no YUY2 overlay surface at 8bpp"); goto out;}
 {DDSURFACEDESC l; memset(&l,0,sizeof(l)); l.dwSize=sizeof(l);
  if(SUCCEEDED(IDirectDrawSurface_Lock(ovl,NULL,&l,DDLOCK_WAIT,NULL))) {for(y=0;y<240;y++) {BYTE *p=(BYTE*)l.lpSurface+(DWORD)y*(DWORD)l.lPitch; for(x=0;x<320;x+=2) {int b=x*8/320; p[x*2]=bars[b][0]; p[x*2+1]=bars[b][1]; p[x*2+2]=bars[b][0]; p[x*2+3]=bars[b][2];}} IDirectDrawSurface_Unlock(ovl,l.lpSurface);}}
 logline("-- 1: key = index 2 (DDOVER_KEYDESTOVERRIDE)\n"); total++;
 if(show(DDOVER_KEYDESTOVERRIDE,2)&&ask("Colour bars ONLY inside the LEFT box (right box plain grey, blue around)?")) pass++;
 logline("-- 2: palette entry 2 turned red, the screen bytes unchanged\n"); total++;
 {PALETTEENTRY r; r.peRed=220; r.peGreen=0; r.peBlue=0; r.peFlags=0; hr=pal?IDirectDrawPalette_SetEntries(pal,0,2,1,&r):E_FAIL; logline("   SetEntries(2 = red): %08lX\n",(DWORD)hr);}
 wait_ms(300); if(ask("Still bars in the LEFT box only, in the same place (no red showing, right box grey)?")) pass++;
 if(pal) IDirectDrawPalette_SetEntries(pal,0,2,1,&pe[2]);
 logline("-- 3: stored key = index 3 (SetColorKey DDCKEY_DESTOVERLAY + DDOVER_KEYDEST)\n"); total++;
 ck.dwColorSpaceLowValue=ck.dwColorSpaceHighValue=3; hr=IDirectDrawSurface_SetColorKey(prim,DDCKEY_DESTOVERLAY,&ck); logline("   SetColorKey(3): %08lX\n",(DWORD)hr);
 if(SUCCEEDED(hr)&&show(DDOVER_KEYDEST,3)&&ask("Now bars ONLY inside the RIGHT box (left box plain grey)? The two boxes are the same grey.")) pass++;
 logline("-- 4: key = index 1 (the background)\n"); total++;
 if(show(DDOVER_KEYDESTOVERRIDE,1)&&ask("Bars everywhere EXCEPT inside the two grey boxes?")) pass++;
 logline("-- 5: hide\n"); total++;
 hr=IDirectDrawSurface_UpdateOverlay(ovl,NULL,prim,NULL,DDOVER_HIDE,NULL); logline("   UpdateOverlay(HIDE): %08lX\n",(DWORD)hr); wait_ms(300);
 if(ask("No colour bars anywhere now (two grey boxes on blue)?")) pass++;
 sprintf(res,"%s: %d of %d checks passed (see VX2OVL8.TXT)",pass==total?"PASS":"FAIL",pass,total); finish(res); logline("%s\n",res);
out:
 if(ovl) IDirectDrawSurface_Release(ovl);
 if(pal) IDirectDrawPalette_Release(pal);
 if(prim) IDirectDrawSurface_Release(prim);
 if(dd) {IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd);}
 DestroyWindow(win); if(lf) fclose(lf); return 0;
}
