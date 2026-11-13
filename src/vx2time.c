/* VX2TIME v1.0: dumps the full CRTC and sequencer registers after each mode switch, once through the normal Windows mode change
 * (ChangeDisplaySettings) and once through DirectDraw SetDisplayMode, so the horizontal timing of the same mode can be compared
 * between the two paths and with the desktop mode at start. Draws nothing. Writes VX2TIME.TXT. Esc aborts.
 * Part 3 repeats the sequence that shifts the picture: flip chain at 640x480x16, then a mode set. Options: "only3" runs just part 3, "dd" skips part 1, "gdi" runs only part 1. */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <io.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"
#include "vx855_flip.h"
static HANDLE dev=INVALID_HANDLE_VALUE; static FILE *lf; static int aborted;
static void logline(const char *f,...) {va_list ap; va_start(ap,f); vfprintf(lf,f,ap); va_end(ap); fflush(lf); _commit(_fileno(lf));}
static void wait_ms(DWORD ms) {DWORD e=GetTickCount()+ms; MSG m; while(GetTickCount()<e&&!aborted) {while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m);DispatchMessage(&m);} if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1; Sleep(20);}}

/* Watchdog: if the main thread makes no progress for 10 seconds (a display lock-up), this thread writes VX2HANG.TXT with the
 * step it is stuck in, the CRTC registers sampled several times (CR60/CR61 move while the CRTC is scanning), and the driver's own log. */
static volatile const char *stage="starting"; static volatile DWORD stage_tick; static volatile int finished;
static void progress(const char *s) {stage=s; stage_tick=GetTickCount();}
static void hang_write(HANDLE f,const char *fmt,...) {
 char b[256]; DWORD w; va_list ap; int n; va_start(ap,fmt); n=vsprintf(b,fmt,ap); va_end(ap); WriteFile(f,b,(DWORD)n,&w,NULL);
}
static DWORD WINAPI watchdog(LPVOID arg) {
 static vx_crt_report r; static char logbuf[32768]; DWORD got,k,i; HANDLE f; (void)arg;
 for(;;) {
  Sleep(500);
  if(finished) return 0;
  if(GetTickCount()-stage_tick>10000UL) break;
 }
 f=CreateFileA("VX2HANG.TXT",GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(f==INVALID_HANDLE_VALUE) return 1;
 hang_write(f,"VX2TIME watchdog: no progress for 10 s. Stuck in step: %s\r\n",(const char*)stage);
 for(k=0;k<4;k++) {
  memset(&r,0,sizeof(r)); got=0;
  if(DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,&r,sizeof(r),&got,NULL)&&got==sizeof(r)) {
   hang_write(f,"sample %lu: CR0C %02lX CR0D %02lX CR13 %02lX CR35 %02lX  CR60 %02lX CR61 %02lX CR9B %02lX\r\n",k,r.cr[0x0C]&0xFFUL,r.cr[0x0D]&0xFFUL,r.cr[0x13]&0xFFUL,r.cr[0x35]&0xFFUL,r.cr[0x60]&0xFFUL,r.cr[0x61]&0xFFUL,r.cr[0x9B]&0xFFUL);
   if(k==0) {hang_write(f,"CR00-FF:"); for(i=0;i<0x100;i++) {if(i%16==0) hang_write(f,"\r\n %02lX:",i); hang_write(f," %02lX",r.cr[i]&0xFFUL);} hang_write(f,"\r\nSR00-7F:"); for(i=0;i<0x80;i++) {if(i%16==0) hang_write(f,"\r\n %02lX:",i); hang_write(f," %02lX",r.sr[i]&0xFFUL);} hang_write(f,"\r\n");}
  } else hang_write(f,"sample %lu: CRT ioctl failed\r\n",k);
  FlushFileBuffers(f); Sleep(2000);
 }
 got=0; memset(logbuf,0,sizeof(logbuf));
 if(DeviceIoControl(dev,VX_IOCTL_LOG,NULL,0,logbuf,sizeof(logbuf)-1,&got,NULL)) {hang_write(f,"\r\n---- driver log (%lu bytes) ----\r\n",got); if(got>sizeof(logbuf)-1) got=sizeof(logbuf)-1; {DWORD w; WriteFile(f,logbuf,got,&w,NULL);}}
 else hang_write(f,"driver log ioctl failed\r\n");
 FlushFileBuffers(f); CloseHandle(f);
 return 0;
}
static void dump(const char *tag) {
 static vx_crt_report r; static char buf[8192]; DWORD got=0; int i,n=0;
 memset(&r,0,sizeof(r));
 if(!DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,&r,sizeof(r),&got,NULL)||got!=sizeof(r)) {logline("-- %s: CRT ioctl failed\n",tag); return;}
 n+=sprintf(buf+n,"-- %s: screen %dx%d\n  misc %02lX crtc_base %lX sr10_found %lX\n  CR00-FF:",tag,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),r.misc&0xFFUL,r.crtc_base,r.sr10_found);
 for(i=0;i<0x100;i++) {if(i%16==0) n+=sprintf(buf+n,"\n   %02X:",i); n+=sprintf(buf+n," %02lX",r.cr[i]&0xFFUL);}
 n+=sprintf(buf+n,"\n  SR00-FF:");
 for(i=0;i<0x100;i++) {if(i%16==0) n+=sprintf(buf+n,"\n   %02X:",i); n+=sprintf(buf+n," %02lX",r.sr[i]&0xFFUL);}
 n+=sprintf(buf+n,"\n");
 logline("%s",buf);
}

static void flipinfo(const char *tag) {
 vxf_request q; DWORD r[VXF_REPLY_DWORDS],got=0; vx_status_t s; memset(r,0,sizeof(r)); q.cb=sizeof(q); q.cmd=VXF_INFO; q.arg=0; q.flags=0;
 if(DeviceIoControl(dev,VX_IOCTL_FLIP,&q,sizeof(q),r,4*VXF_REPLY_DWORDS,&got,NULL)) logline("  flip info (%s): state %lu shift %lu display offset %lu flips %lu bios calls %lu reason %lu last request %lu last start value %lu\n",tag,r[VXF_R_STATE],r[VXF_R_SHIFT],r[VXF_R_DISPLAY],r[VXF_R_FLIPS],r[VXF_R_BIOS],r[VXF_R_REASON],r[14],r[15]);
 memset(&s,0,sizeof(s)); if(DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)&&got==sizeof(s)) logline("  driver status (%s): %lux%lux%lu pitch %lu\n",tag,(DWORD)s.width,(DWORD)s.height,(DWORD)s.bpp,(DWORD)s.video_pitch);
}
static void flipcycle(LPDIRECTDRAW dd) {
 DDSURFACEDESC s; DDSCAPS caps; LPDIRECTDRAWSURFACE prim=NULL,back=NULL; int i; DWORD cols[3]; HRESULT hr;
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; s.dwBackBufferCount=2; s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
 progress("flip chain create"); hr=IDirectDraw_CreateSurface(dd,&s,&prim,NULL); logline("  flip chain CreateSurface: %08lX\n",(DWORD)hr); if(FAILED(hr)) return;
 memset(&caps,0,sizeof(caps)); caps.dwCaps=DDSCAPS_BACKBUFFER;
 if(FAILED(IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back))) {IDirectDrawSurface_Release(prim); return;}
 cols[0]=0x00DC2828UL; cols[1]=0x0028C83CUL; cols[2]=0x003246E6UL;
 for(i=0;i<6;i++) {
  DDBLTFX f; memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=cols[i%3]; IDirectDrawSurface_Blt(back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f);
  progress("flip chain flip"); if(FAILED(IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT))) break;
  IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back);
 }
 flipinfo("after the flips, before FlipToGDISurface");
 progress("FlipToGDISurface"); hr=IDirectDraw_FlipToGDISurface(dd); logline("  FlipToGDISurface: %08lX\n",(DWORD)hr);
 flipinfo("after FlipToGDISurface");
 progress("flip chain release"); IDirectDrawSurface_Release(prim);
 flipinfo("after releasing the chain");
}
static const DWORD modes[6][3]={{640,480,16},{800,600,16},{1024,768,16},{640,480,32},{800,600,32},{1024,768,32}};
static LRESULT CALLBACK wnd(HWND w,UINT m,WPARAM a,LPARAM b) {return DefWindowProc(w,m,a,b);}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show_) {
 int skip1,skip2,skip3,only3; char tag[64]; vx_status_t s; DWORD got; int i; WNDCLASS wc; HWND win; DEVMODE dm; LPDIRECTDRAW dd=NULL; HMODULE mod; HRESULT hr;
 typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*); CREATE create;
 (void)prev;(void)show_; skip1=cmd&&strstr(cmd,"dd"); skip2=cmd&&strstr(cmd,"gdi"); only3=cmd&&strstr(cmd,"only3"); skip3=skip2; if(only3) {skip1=1; skip2=1; skip3=0;}
 lf=fopen("VX2TIME.TXT","w"); if(!lf) return 1;
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC) {logline("driver not active\n"); fclose(lf); return 1;}
 logline("VX2TIME v1.0 / driver v10.61\ndesktop %lux%lux%lu\n",(DWORD)s.width,(DWORD)s.height,(DWORD)s.bpp);
 stage_tick=GetTickCount(); CreateThread(NULL,0,watchdog,NULL,0,&got); dump("desktop at start");
 logline("\nPART 1: Windows mode change (ChangeDisplaySettings)%s\n",skip1?" - skipped":"");
 for(i=0;i<6&&!aborted&&!skip1;i++) {
  LONG rc;
  memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=modes[i][0]; dm.dmPelsHeight=modes[i][1]; dm.dmBitsPerPel=modes[i][2]; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
  logline("\nstep: ChangeDisplaySettings %lux%lux%lu\n",modes[i][0],modes[i][1],modes[i][2]); sprintf(tag,"ChangeDisplaySettings %lux%lux%lu",modes[i][0],modes[i][1],modes[i][2]); progress(tag);
  rc=ChangeDisplaySettings(&dm,CDS_FULLSCREEN); logline("result %ld\n",(long)rc);
  wait_ms(2500); sprintf(tag,"GDI %lux%lux%lu",modes[i][0],modes[i][1],modes[i][2]); dump(tag);
 }
 if(!skip1) {ChangeDisplaySettings(NULL,0); wait_ms(2500); dump("desktop restored after part 1");}
 logline("\nPART 2: DirectDraw SetDisplayMode\n");
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VX2TIME"; RegisterClass(&wc);
 win=CreateWindowEx(WS_EX_TOPMOST,wc.lpszClassName,"VX2TIME",WS_POPUP|WS_VISIBLE,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,hi,NULL);
 mod=LoadLibraryA("DDRAW.DLL"); create=mod?(CREATE)GetProcAddress(mod,"DirectDrawCreate"):NULL;
  if(0) ;
 else if(win&&create&&SUCCEEDED(create(NULL,&dd,NULL))&&SUCCEEDED(IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))) {
  for(i=0;i<6&&!aborted&&!skip2;i++) {
   logline("\nstep: SetDisplayMode %lux%lux%lu\n",modes[i][0],modes[i][1],modes[i][2]); sprintf(tag,"SetDisplayMode %lux%lux%lu",modes[i][0],modes[i][1],modes[i][2]); progress(tag);
   hr=IDirectDraw_SetDisplayMode(dd,modes[i][0],modes[i][1],modes[i][2]); logline("result %08lX\n",(DWORD)hr);
   wait_ms(2500); sprintf(tag,"DDRAW %lux%lux%lu",modes[i][0],modes[i][1],modes[i][2]); dump(tag);
   {LPDIRECTDRAWSURFACE ps=NULL; DDSURFACEDESC d; memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
    progress("CreateSurface(primary)"); hr=IDirectDraw_CreateSurface(dd,&d,&ps,NULL); logline("CreateSurface(primary): %08lX\n",(DWORD)hr);
    if(SUCCEEDED(hr)) {
     progress("Lock primary"); memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); hr=IDirectDrawSurface_Lock(ps,NULL,&d,DDLOCK_WAIT,NULL);
     logline("lock: %08lX pitch %ld ptr %p\n",(DWORD)hr,(long)d.lPitch,d.lpSurface); if(SUCCEEDED(hr)) IDirectDrawSurface_Unlock(ps,NULL);
     wait_ms(1500); sprintf(tag,"DDRAW %lux%lux%lu with primary surface",modes[i][0],modes[i][1],modes[i][2]); dump(tag);
     progress("Release primary"); IDirectDrawSurface_Release(ps); progress("after release"); wait_ms(500);}}
  }
  if(!skip3&&!aborted) {
   static const DWORD tm[2][3]={{800,600,16},{1024,768,32}}; int k;
   logline("\nPART 3: flip chain at 640x480x16, then a mode set (the sequence that shifts the picture)\n");
   for(k=0;k<2&&!aborted;k++) {
    LPDIRECTDRAWSURFACE ps=NULL; DDSURFACEDESC d;
    logline("\nstep: SetDisplayMode 640x480x16 (mode A)\n"); progress("P3 SetDisplayMode A"); hr=IDirectDraw_SetDisplayMode(dd,640,480,16); logline("result %08lX\n",(DWORD)hr);
    wait_ms(2000); dump("P3 A 640x480x16 before its flip chain"); flipinfo("A before");
    logline("step: flip chain in mode A\n"); flipcycle(dd); wait_ms(1500); dump("P3 A after its flip chain");
    logline("\nstep: SetDisplayMode %lux%lux%lu (mode B, after the chain)\n",tm[k][0],tm[k][1],tm[k][2]); progress("P3 SetDisplayMode B");
    hr=IDirectDraw_SetDisplayMode(dd,tm[k][0],tm[k][1],tm[k][2]); logline("result %08lX\n",(DWORD)hr);
    wait_ms(2500); sprintf(tag,"P3 B %lux%lux%lu after the flip chain",tm[k][0],tm[k][1],tm[k][2]); dump(tag); flipinfo("B");
    memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
    progress("P3 CreateSurface"); hr=IDirectDraw_CreateSurface(dd,&d,&ps,NULL); logline("CreateSurface(primary): %08lX\n",(DWORD)hr);
    if(SUCCEEDED(hr)) {
     progress("P3 Lock"); memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); hr=IDirectDrawSurface_Lock(ps,NULL,&d,DDLOCK_WAIT,NULL);
     logline("lock: %08lX pitch %ld ptr %p\n",(DWORD)hr,(long)d.lPitch,d.lpSurface); if(SUCCEEDED(hr)) IDirectDrawSurface_Unlock(ps,NULL);
     wait_ms(1500); sprintf(tag,"P3 B %lux%lux%lu with primary surface",tm[k][0],tm[k][1],tm[k][2]); dump(tag); flipinfo("B with surface");
     IDirectDrawSurface_Release(ps); wait_ms(500);
    }
   }
  }
  progress("RestoreDisplayMode"); IDirectDraw_RestoreDisplayMode(dd); progress("SetCooperativeLevel normal"); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd);
  wait_ms(2500); dump("desktop restored after part 2");
 } else logline("DirectDraw not available\n");
 if(win) DestroyWindow(win);
 finished=1; Sleep(700); CloseHandle(dev); logline("Finished%s.\n",aborted?" (aborted)":""); fclose(lf);
 MessageBox(NULL,"Done. Send VX2TIME.TXT. Did any mode look shifted while this ran (it only changes modes, draws nothing)?","VX2TIME",MB_OK);
 return 0;
}
