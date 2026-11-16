/* VXPANEL v1.1 (driver .599; v1.1: how often the driver finds the display in its blank, which must be a few per cent; the moving bar
 * also with one back buffer, where the program draws into the buffer that was on the screen a moment ago: a blank read the wrong way
 * round shows there as flicker; the sizes the panel must stretch, 800x600 and 1024x768, set and asked about).
 * v1.0 (driver .598): a display that is scanned by the chip's second display unit (IGA2), as a laptop's panel is.
 * .597 on such a machine passed every test that reads registers and video memory back, while the screen showed no pointer and one
 * flickering buffer under Direct3D: only the person at the screen can judge this, so the program asks. It
 * 1. reads how the outputs are routed (which unit is on, which the panel takes, the second unit's size, pitch and start address) and
 *    what the driver decided from it;
 * 2. samples the status bits a flip can wait on (MMIO 200 bits 1 and 3, 204 bits 1 and 2, CR61 bit 5) and says which ones toggle;
 * 3. shows a red square with the pointer's tip put on it, at the desktop's size and at 640x480 and 1024x768 when Windows offers them,
 *    and asks whether the pointer is there and on the square (the panel may stretch the smaller sizes);
 * 4. flips red, green and blue screens a second apart, then a moving bar every frame, and asks what was seen;
 * 5. lists the display modes Windows offers and the ones DirectDraw offers, and the ones DirectDraw leaves out.
 * Answer the questions with the keyboard (Y / N) if the pointer cannot be seen. Esc ends a running step. Log: VXPANEL.TXT. */
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"
#include "vx855_2d.h"
#include "vx855_flip.h"

static FILE *lf; static HANDLE dev=INVALID_HANDLE_VALUE; static HWND win; static HINSTANCE inst;
static int no_answers,unseen; static volatile int aborted;
static void say(const char *f,...) {static char b[2048];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);if(lf){fputs(b,lf);fflush(lf);}}
static int ctl(DWORD code,DWORD *o) {DWORD got=0; memset(o,0,16); return DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)&&got==16;}
static int crtdump(vx_crt_report *c) {DWORD got=0; memset(c,0,sizeof(*c)); return DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,c,sizeof(*c),&got,NULL)&&got==sizeof(*c);}
static int flipctl(DWORD cmd,DWORD arg,DWORD *r) {vxf_request q; DWORD got=0; q.cb=sizeof(q); q.cmd=cmd; q.arg=arg; q.flags=0; memset(r,0,4*VXF_REPLY_DWORDS); return DeviceIoControl(dev,VX_IOCTL_FLIP,&q,sizeof(q),r,4*VXF_REPLY_DWORDS,&got,NULL);}
static void pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {if(m.message==WM_KEYDOWN&&m.wParam==VK_ESCAPE) aborted=1; TranslateMessage(&m); DispatchMessage(&m);}}
static void wait_ms(DWORD ms) {DWORD t=GetTickCount(); while(GetTickCount()-t<ms&&!aborted) {pump(); Sleep(5);}}
static int ask(const char *what,const char *q) {
 int a=MessageBoxA(win,q,"VXPANEL (Y = yes, N = no)",MB_YESNO|MB_ICONQUESTION|MB_TOPMOST|MB_SETFOREGROUND);
 say("   QUESTION %s: %s -> %s\n",what,q,a==IDYES?"YES":"NO"); if(a!=IDYES) no_answers++; return a==IDYES;
}
static LRESULT CALLBACK wnd(HWND h,UINT m,WPARAM w,LPARAM l) {
 if(m==WM_KEYDOWN&&w==VK_ESCAPE) {aborted=1; return 0;}
 if(m==WM_SETCURSOR) {SetCursor(LoadCursor(NULL,IDC_ARROW)); return TRUE;}
 if(m==WM_ERASEBKGND) return 1;
 return DefWindowProc(h,m,w,l);
}

/* ---- 1. routing ---- */
#define C(n) ((unsigned long)c.cr[(n)]&0xFF)
#define S(n) ((unsigned long)c.sr[(n)]&0xFF)
static void routing(const char *when) {
 static vx_crt_report c; DWORD o[4]; unsigned long h1,v1,s1,ht2,h2,vt2,v2,p2,s2;
 if(!crtdump(&c)) {say("%s: the register dump failed\n",when); return;}
 h1=(C(0x01)+1)*8; v1=(C(0x12)|((C(0x07)&2)<<7)|((C(0x07)&0x40)<<3)|((C(0x35)&4)<<8))+1;
 s1=(((C(0x48)&0x1F)<<24)|(C(0x34)<<16)|(C(0x0C)<<8)|C(0x0D))<<1;
 ht2=(C(0x50)|((C(0x55)&0x0F)<<8))+1; h2=(C(0x51)|((C(0x55)&0x70)<<4))+1; vt2=(C(0x58)|((C(0x5D)&7)<<8))+1; v2=(C(0x59)|((C(0x5D)&0x38)<<5))+1;
 p2=(C(0x66)|((C(0x67)&3)<<8)|((C(0x71)&0x80)<<3))<<3; s2=(((C(0x62)>>1)&0x7F)|(C(0x63)<<7)|(C(0x64)<<15)|((C(0xA3)&7)<<23))<<3;
 say("%s:\n",when);
 say("   CR6A %02lX (bit 7: the second unit on), CR6B %02lX, CR36 %02lX (bits 5:4: the monitor output's power state, 0 = on), SR16 %02lX, SR1E %02lX, SR2A %02lX\n",C(0x6A),C(0x6B),C(0x36),S(0x16),S(0x1E),S(0x2A));
 say("   panel source: CR99 %02lX, CR97 %02lX (bit 4: the second unit); CR9B %02lX; CR79 %02lX (bit 0: the panel scaler on), CR7A %02lX, CR9F %02lX, CRA2 %02lX; CR91 %02lX, CRD2 %02lX, CRD4 %02lX\n",C(0x99),C(0x97),C(0x9B),C(0x79),C(0x7A),C(0x9F),C(0xA2),C(0x91),C(0xD2),C(0xD4));
 say("   first unit: %lu x %lu shown, start address %lu\n",h1,v1,s1);
 say("   second unit: %lu x %lu shown in %lu x %lu, pitch %lu bytes, depth bits %lu, start address %lu; CR61 %02lX, CR65 %02lX, CR6C %02lX\n",h2,v2,ht2,vt2,p2,C(0x67)>>6,s2,C(0x61),C(0x65),C(0x6C));
 if(ctl(0xFFFFFE48UL,o)) say("   the driver: CR6A %02lX CR36 %02lX CR97 %02lX CR99 %02lX, second unit's pitch %lu; second unit shows this frame buffer: %s; the picture seen is the second unit's alone: %s; pointer on both units: %s; start addresses written to the second unit %lu, flips that waited by time %lu\n",
  o[0]&0xFF,(o[0]>>8)&0xFF,(o[0]>>16)&0xFF,o[0]>>24,o[1],(o[2]&1)?"yes":"no",(o[2]&2)?"yes":"no",(o[2]&0x100)?"yes":"no",o[3]&0xFFFF,o[3]>>16);
 else say("   the driver does not answer control FFFFFE48 (older than .598)\n");
 if(ctl(0xFFFFFE15UL,o)) say("   hardware cursor: mode %lu, shown %lu times, shapes left to the software cursor %lu\n",o[0],o[1],o[2]);
}

/* ---- 2. status bits ---- */
static void bits(void) {
 DWORD o[4],n=0,t0,t1,set[4]={0,0,0,0},tr[4]={0,0,0,0},last[4]={0,0,0,0},k,r[VXF_REPLY_DWORDS]; static const char *name[4]={"MMIO 200 bit 1 (first unit's blank)","MMIO 200 bit 3","MMIO 204 bit 1 (address update pending)","MMIO 204 bit 2 (second unit's blank)"};
 static vx_crt_report c; DWORD cs=0,ct=0,cl=0,cn=0;
 say("status bits, sampled for half a second:\n");
 t0=GetTickCount();
 do {
  if(!ctl(0xFFFFFE80UL+0x20,o)) {say("   the MMIO read failed\n"); return;}
  {DWORD b[4]; b[0]=(o[0]>>1)&1; b[1]=(o[0]>>3)&1; b[2]=(o[1]>>1)&1; b[3]=(o[1]>>2)&1;
   for(k=0;k<4;k++) {set[k]+=b[k]; if(n&&b[k]!=last[k]) tr[k]++; last[k]=b[k];}}
  n++; t1=GetTickCount();
 } while(t1-t0<500);
 for(k=0;k<4;k++) say("   %s: set in %lu of %lu samples, %lu changes (%lu a second)%s\n",name[k],set[k],n,tr[k],tr[k]*1000/(t1-t0?t1-t0:1),tr[k]>=40&&tr[k]<=110?"  <- follows the refresh":tr[k]?"":"  <- does not move");
 t0=GetTickCount();
 do {if(!crtdump(&c)) break; {DWORD b=(c.cr[0x61]>>5)&1; cs+=b; if(cn&&b!=cl) ct++; cl=b;} cn++; t1=GetTickCount();} while(t1-t0<300);
 say("   CR61 bit 5 (second unit's flip flag): set in %lu of %lu samples, %lu changes\n",cs,cn,ct);
 {DWORD nb=0,nt=0,t2=GetTickCount(); do {if(!flipctl(VXF_WAITVB,0,r)||r[VXF_R_RESULT]!=VXF_OK) break; nb+=r[VXF_R_IN_VB]; nt++;} while(GetTickCount()-t2<300);
  if(nt) say("   the blank as the driver reads it: in the blank in %lu of %lu samples (%lu.%lu per cent; a few per cent is right)\n",nb,nt,nb*100/nt,(nb*1000/nt)%10); else say("   the driver gives no blank test\n");}
 if(flipctl(VXF_QUERY,0,r)) say("   the driver's flip state: result %lu, state %lu (1 = start address written directly), blank %lu (1 = usable, 2 = never seen), three frames %lu ms\n",r[VXF_R_RESULT],r[VXF_R_STATE],r[VXF_R_VBLANK],r[VXF_R_FRAME3MS]);
}

/* ---- 3. pointer ---- */
static void pointer_at(const char *what) {
 int w=GetSystemMetrics(SM_CXSCREEN),h=GetSystemMetrics(SM_CYSCREEN),x=w/4,y=h/4; HDC dc; RECT r; HBRUSH b; DWORD o[4]; char q[160];
 SetWindowPos(win,HWND_TOPMOST,0,0,w,h,SWP_SHOWWINDOW); SetForegroundWindow(win); pump();
 dc=GetDC(win); SetRect(&r,0,0,w,h); FillRect(dc,&r,(HBRUSH)GetStockObject(BLACK_BRUSH));
 b=CreateSolidBrush(RGB(255,0,0)); SetRect(&r,x-8,y-8,x+8,y+8); FillRect(dc,&r,b); DeleteObject(b);
 SetRect(&r,0,0,w,1); FillRect(dc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH)); SetRect(&r,0,h-1,w,h); FillRect(dc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));
 SetRect(&r,0,0,1,h); FillRect(dc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH)); SetRect(&r,w-1,0,w,h); FillRect(dc,&r,(HBRUSH)GetStockObject(WHITE_BRUSH));
 ReleaseDC(win,dc);
 SetCursorPos(x,y); SetCursor(LoadCursor(NULL,IDC_ARROW)); wait_ms(1500);
 say("pointer, %s (%d x %d): the tip put at %d,%d on a red square\n",what,w,h,x,y);
 if(ctl(0xFFFFFE48UL,o)) say("   the driver: pointer on both units: %s\n",(o[2]&0x100)?"yes":"no");
 if(ctl(0xFFFFFE16UL,o)) say("   hardware cursor: moves %lu, shapes %lu, hides %lu, on %lu\n",o[0],o[1],o[2],o[3]);
 _snprintf(q,sizeof(q)-1,"%s: is a white line visible on all four edges of the screen?",what); q[sizeof(q)-1]=0; (void)ask("picture",q);
 SetCursorPos(x,y); wait_ms(300);
 _snprintf(q,sizeof(q)-1,"%s: was the mouse pointer visible before this box appeared?",what); q[sizeof(q)-1]=0;
 if(ask("pointer",q)) {_snprintf(q,sizeof(q)-1,"%s: was its tip on the small red square (upper left quarter)?",what); q[sizeof(q)-1]=0; (void)ask("pointer place",q);} else unseen++;
}
static int offered(DWORD w,DWORD h,DWORD bpp) {
 DEVMODEA dm; DWORD i;
 for(i=0;i<4096;i++) {memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); if(!EnumDisplaySettingsA(NULL,i,&dm)) break; if(dm.dmPelsWidth==w&&dm.dmPelsHeight==h&&dm.dmBitsPerPel==bpp) return 1;}
 return 0;
}
static void pointer(void) {
 HDC s=GetDC(NULL); DWORD bpp=s?(DWORD)GetDeviceCaps(s,BITSPIXEL):16,k; static const DWORD sz[3][2]={{640,480},{800,600},{1024,768}};
 DWORD dw=(DWORD)GetSystemMetrics(SM_CXSCREEN),dh=(DWORD)GetSystemMetrics(SM_CYSCREEN);
 if(s) ReleaseDC(NULL,s);
 pointer_at("the desktop's size");
 for(k=0;k<3&&!aborted;k++) {
  DEVMODEA dm; LONG rc; char what[64];
  if(sz[k][0]==dw&&sz[k][1]==dh) continue;
  if(!offered(sz[k][0],sz[k][1],bpp)) {say("pointer, %lu x %lu x %lu: Windows does not offer this mode\n",sz[k][0],sz[k][1],bpp); continue;}
  memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=sz[k][0]; dm.dmPelsHeight=sz[k][1]; dm.dmBitsPerPel=bpp; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
  rc=ChangeDisplaySettingsA(&dm,CDS_FULLSCREEN); say("ChangeDisplaySettings(%lu x %lu x %lu): %ld\n",sz[k][0],sz[k][1],bpp,rc);
  if(rc!=DISP_CHANGE_SUCCESSFUL) continue;
  wait_ms(1500); _snprintf(what,sizeof(what)-1,"%lu x %lu",sz[k][0],sz[k][1]); what[sizeof(what)-1]=0;
  routing(what); pointer_at(what);
  ChangeDisplaySettingsA(NULL,0); wait_ms(1500);
 }
 ShowWindow(win,SW_HIDE); pump();
}

/* ---- 4. flips ---- */
static void fill(LPDIRECTDRAWSURFACE s,RECT *r,DWORD c) {DDBLTFX f; memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=c; IDirectDrawSurface_Blt(s,r,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f);}
static void flips(LPDIRECTDRAW dd,DWORD bpp,DWORD backs) {
 DDSURFACEDESC s; DDSCAPS caps; LPDIRECTDRAWSURFACE prim=NULL,back=NULL; HRESULT hr; DWORD o0[4],o1[4],r[VXF_REPLY_DWORDS],i,t0,t1,col[3];
 int w=GetSystemMetrics(SM_CXSCREEN),h=GetSystemMetrics(SM_CYSCREEN);
 col[0]=bpp==16?0xF800UL:0xFF0000UL; col[1]=bpp==16?0x07E0UL:0x00FF00UL; col[2]=bpp==16?0x001FUL:0x0000FFUL;
 say("flips at %d x %d x %lu, %lu back buffer(s):\n",w,h,bpp,backs);
 SetWindowPos(win,HWND_TOPMOST,0,0,w,h,SWP_SHOWWINDOW); SetForegroundWindow(win); pump();
 hr=IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN); if(FAILED(hr)) {say("   SetCooperativeLevel: %08lX\n",(DWORD)hr); return;}
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX; s.dwBackBufferCount=backs;
 hr=IDirectDraw_CreateSurface(dd,&s,&prim,NULL); say("   CreateSurface(primary, %lu back buffer(s)): %08lX\n",backs,(DWORD)hr);
 if(FAILED(hr)) {prim=NULL; goto out;}
 memset(&caps,0,sizeof(caps)); caps.dwCaps=DDSCAPS_BACKBUFFER; hr=IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back); if(FAILED(hr)) {say("   GetAttachedSurface: %08lX\n",(DWORD)hr); back=NULL; goto out;}
 (void)ctl(0xFFFFFE48UL,o0);
 fill(prim,NULL,0);
 for(i=0;i<6&&!aborted&&backs==2;i++) {fill(back,NULL,col[i%3]); hr=IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT); if(FAILED(hr)) {say("   Flip %lu: %08lX\n",i,(DWORD)hr); break;} wait_ms(1000);}
 t0=GetTickCount();
 for(i=0;i<240&&!aborted;i++) {
  RECT b; fill(back,NULL,0); b.left=(LONG)((i*8)%(DWORD)(w-48)); b.right=b.left+48; b.top=0; b.bottom=h; fill(back,&b,bpp==16?0xFFFFUL:0xFFFFFFUL);
  hr=IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT); if(FAILED(hr)) {say("   Flip (bar) %lu: %08lX\n",i,(DWORD)hr); break;}
  pump();
 }
 t1=GetTickCount();
 say("   moving bar: %lu flips in %lu ms (%lu.%lu a second)\n",i,t1-t0,(t1-t0)?i*1000/(t1-t0):0,(t1-t0)?(i*10000/(t1-t0))%10:0);
 (void)ctl(0xFFFFFE48UL,o1);
 say("   the driver: start addresses written to the second unit %lu, flips that waited by time %lu\n",(o1[3]&0xFFFF)-(o0[3]&0xFFFF),(o1[3]>>16)-(o0[3]>>16));
 if(flipctl(VXF_INFO,0,r)) say("   the driver's flip state: state %lu, blank %lu, flips %lu, timeouts %lu, three frames %lu ms\n",r[VXF_R_STATE],r[VXF_R_VBLANK],r[VXF_R_FLIPS],r[VXF_R_TIMEOUTS],r[VXF_R_FRAME3MS]);
 routing("during the flip chain");
out:
 if(prim) {IDirectDraw_FlipToGDISurface(dd); IDirectDrawSurface_Release(prim);}
 IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); ShowWindow(win,SW_HIDE); pump(); wait_ms(500);
 if(prim) {
  if(backs==2) {(void)ask("colours","Did the whole screen show red, green and blue in turn (twice), each one steady for about a second?");
   (void)ask("bar, two back buffers","Then a white bar moved across a black screen: was it clean (no flicker, no black flashes, not torn in two)?");}
  else (void)ask("bar, one back buffer","A white bar moved across a black screen again: was it clean this time too (no flicker, no black flashes, not torn in two)?");
 }
}

/* ---- 5. modes ---- */
#define MAXM 256
static DWORD wm[MAXM][3],dm_[MAXM][3]; static int wn,dn;
static HRESULT WINAPI modecb(LPDDSURFACEDESC d,LPVOID ctx) {
 (void)ctx; if(dn<MAXM) {dm_[dn][0]=d->dwWidth; dm_[dn][1]=d->dwHeight; dm_[dn][2]=d->ddpfPixelFormat.dwRGBBitCount; dn++;} return DDENUMRET_OK;
}
static void modes(LPDIRECTDRAW dd) {
 DEVMODEA dm; DWORD i; int k,j,miss=0; HRESULT hr;
 for(i=0;i<4096&&wn<MAXM;i++) {
  memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); if(!EnumDisplaySettingsA(NULL,i,&dm)) break;
  for(k=0;k<wn;k++) if(wm[k][0]==dm.dmPelsWidth&&wm[k][1]==dm.dmPelsHeight&&wm[k][2]==dm.dmBitsPerPel) break;
  if(k==wn) {wm[wn][0]=dm.dmPelsWidth; wm[wn][1]=dm.dmPelsHeight; wm[wn][2]=dm.dmBitsPerPel; wn++;}
 }
 say("modes Windows offers (EnumDisplaySettings, %lu entries, %d different), with ChangeDisplaySettings(test)'s answer (0 = can be set):\n",i,wn);
 for(k=0;k<wn;k++) {
  LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=wm[k][0]; dm.dmPelsHeight=wm[k][1]; dm.dmBitsPerPel=wm[k][2]; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
  rc=ChangeDisplaySettingsA(&dm,CDS_TEST); say("   %lu x %lu x %lu: %ld\n",wm[k][0],wm[k][1],wm[k][2],rc);
 }
 hr=IDirectDraw_EnumDisplayModes(dd,0,NULL,NULL,modecb);
 say("modes DirectDraw offers (EnumDisplayModes %08lX, %d):\n",(DWORD)hr,dn);
 for(k=0;k<dn;k++) say("   %lu x %lu x %lu\n",dm_[k][0],dm_[k][1],dm_[k][2]);
 say("modes Windows offers at 8, 16 or 32 bits that DirectDraw leaves out:\n");
 for(k=0;k<wn;k++) {
  if(wm[k][2]!=8&&wm[k][2]!=16&&wm[k][2]!=32) continue;
  for(j=0;j<dn;j++) if(dm_[j][0]==wm[k][0]&&dm_[j][1]==wm[k][1]&&dm_[j][2]==wm[k][2]) break;
  if(j==dn) {say("   %lu x %lu x %lu\n",wm[k][0],wm[k][1],wm[k][2]); miss++;}
 }
 if(!miss) say("   none\n");
 {DDCAPS c; memset(&c,0,sizeof(c)); c.dwSize=sizeof(c); if(SUCCEEDED(IDirectDraw_GetCaps(dd,&c,NULL))) say("DirectDraw: hardware %s; video memory %lu KB in all, %lu KB free; monitor frequency ",(c.dwCaps&0x02000000UL)?"NO":"yes",c.dwVidMemTotal>>10,c.dwVidMemFree>>10);
  {DWORD hz=0; if(SUCCEEDED(IDirectDraw_GetMonitorFrequency(dd,&hz))) say("%lu Hz\n",hz); else say("not given\n");}}
}

/* the VxD's own mode table: what the video BIOS lists and what the driver made of each entry */
static void biosmodes(void) {
 static vx_edid_report e; DWORD in=0,got=0,i;
 if(!DeviceIoControl(dev,VX_IOCTL_EDID,&in,4,&e,sizeof(e),&got,NULL)||got!=sizeof(e)||e.magic!=VX_MAGIC) {say("the driver's mode report failed\n"); return;}
 say("the driver's mode table: EDID %s (bus %lu), %lu base modes, %lu in all, extra modes %s, the video BIOS lists %lu\n",e.edid_valid?"read":"NOT read",e.edid_bus,e.base_modes,e.total_modes,e.extras_on?"on":"off",e.bios_count);
 if(e.edid_valid) {say("   EDID:"); for(i=0;i<128;i++) say("%s%02lX",(i&31)?" ":"\n      ",e.edid[i>>2]>>((i&3)*8)&0xFF); say("\n");}
 say("   BIOS modes (verdict: 0 base, 1 taken, 2 query failed, 3 not linear direct colour, 4 depth, 5 size, 6 layout, 7 already there, 8 monitor does not list it, 9 no EDID and above 1024x768, 10 table full):\n");
 for(i=0;i<e.bios_count&&i<64;i++) say("   %04lX: %lu x %lu x %lu, memory model %lu, attributes %04lX, verdict %lu\n",e.bios[i*3]&0xFFFF,e.bios[i*3+1]&0xFFFF,e.bios[i*3+1]>>16,e.bios[i*3+2]&0xFF,(e.bios[i*3+2]>>8)&0xFF,e.bios[i*3+2]>>16,e.bios[i*3]>>16);
}
typedef HRESULT (WINAPI *CREATE)(GUID *,LPDIRECTDRAW *,IUnknown *);
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show) {
 WNDCLASS wc; DWORD o[8],got=0,in=0x75; HMODULE mod; LPDIRECTDRAW dd=NULL; HDC s; DWORD bpp=16; HRESULT hr; SYSTEMTIME t;
 (void)prev; (void)cmd; (void)show; inst=hi;
 lf=fopen("VXPANEL.TXT","a"); GetLocalTime(&t);
 say("\n==== VXPANEL v1.1 (driver .599 and later) %04u-%02u-%02u %02u:%02u:%02u ====\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
 s=GetDC(NULL); if(s) {bpp=(DWORD)GetDeviceCaps(s,BITSPIXEL); ReleaseDC(NULL,s);}
 say("Windows: %d x %d, %lu bits\n",GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),bpp);
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {say("The driver is not loaded.\n"); if(lf) fclose(lf); MessageBoxA(NULL,"The Chrome9 driver is not loaded.","VXPANEL",MB_OK); return 1;}
 memset(o,0,sizeof(o));
 if(DeviceIoControl(dev,0x85500022UL,&in,4,o,32,&got,NULL)&&got==32&&o[0]==0x75) say("the VxD's build: .%lu; the device 1106:%04lX\n",o[1],o[2]);
 if(bpp!=16&&bpp!=32) {say("The desktop must be at 16 or 32 bits.\n"); MessageBoxA(NULL,"Set the desktop to 16 or 32 bit colour first.","VXPANEL",MB_OK); CloseHandle(dev); if(lf) fclose(lf); return 1;}
 MessageBoxA(NULL,"VXPANEL asks what you see on the screen.\n\nAnswer each box with the keyboard: Y = yes, N = no.\nEsc ends a running step.\nIt changes the screen size a few times; nothing is written to the registry.","VXPANEL",MB_OK|MB_TOPMOST|MB_SETFOREGROUND);
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=wnd; wc.hInstance=hi; wc.lpszClassName="VXPANEL"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClass(&wc);
 win=CreateWindowEx(WS_EX_TOPMOST,wc.lpszClassName,"VXPANEL",WS_POPUP,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,hi,NULL);
 if(!win) {say("no window\n"); CloseHandle(dev); if(lf) fclose(lf); return 1;}
 routing("at the desktop");
 biosmodes();
 bits();
 if(!aborted) pointer();
 mod=LoadLibraryA("DDRAW.DLL");
 if(mod) {CREATE create=(CREATE)GetProcAddress(mod,"DirectDrawCreate"); if(create) {hr=create(NULL,&dd,NULL); if(FAILED(hr)) {say("DirectDrawCreate: %08lX\n",(DWORD)hr); dd=NULL;}}}
 if(dd) {
  IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL);
  modes(dd);
  if(!aborted) flips(dd,bpp,2);
  if(!aborted) flips(dd,bpp,1);
  IDirectDraw_Release(dd);
 } else say("DirectDraw is not available\n");
 routing("at the end");
 say("SUMMARY: %d question(s) answered NO, the pointer not seen in %d of the sizes tried%s\n",no_answers,unseen,aborted?"; ended early with Esc":"");
 say("Done. Send VXPANEL.TXT.\n");
 DestroyWindow(win); CloseHandle(dev); if(lf) fclose(lf);
 MessageBoxA(NULL,"Done. Please send VXPANEL.TXT (next to the program).","VXPANEL",MB_OK|MB_TOPMOST|MB_SETFOREGROUND);
 return 0;
}
