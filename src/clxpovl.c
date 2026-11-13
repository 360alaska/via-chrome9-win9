/* CLXPOVL v1.0 (driver v10.340): runs on the Windows XP install of the board (VIA's own driver). It asks XP's DirectDraw for the three
 * overlay features the Win98 driver lacks and records how VIA's driver programs the display unit for each, so they can be ported:
 *   1. the overlay with a destination colour key on a 32 bpp desktop (the reference: V1 as the Win98 driver already drives it);
 *   2. constant overlay alpha (DDOVER_ALPHASRCCONSTOVERRIDE / ALPHADESTCONSTOVERRIDE at 50%);
 *   3. a second overlay at the same time (V3);
 *   4. the destination colour key on an 8 bpp desktop (a palette index).
 * Each step: the HRESULT, the display unit's registers (MMIO 0x200-0x3FF and 0x1200-0x13FF read through CXPHYS.SYS, read-only) and what
 * changed from the step before, and what you see (a Yes / No / Cancel question). The 8 bpp step changes the mode for itself (temporary).
 * Needs Administrator and CXPHYS.SYS next to the EXE (as CLXP3D); inpout32.dll is used for the PCI scan when present. Output: CLXPOVL.TXT
 * next to the program. MIT, 2026. */
#define COBJMACROS
#define INITGUID
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

static FILE *lf;
static void out(const char *fmt,...) {char t[1200];va_list ap;va_start(ap,fmt);_vsnprintf(t,sizeof(t)-1,fmt,ap);va_end(ap);t[sizeof(t)-1]=0;fputs(t,stdout);if(lf){fputs(t,lf);fflush(lf);}}
static int ask(const char *q) {return MessageBoxA(NULL,q,"CLXPOVL",MB_YESNOCANCEL|MB_ICONQUESTION|MB_TOPMOST|MB_SETFOREGROUND);}

/* ---- the display unit's registers through CXPHYS.SYS (as CLXP3D v2.2+) ---- */
#define CX_IOCTL_READ 0x00226004UL
static HANDLE cxdev=INVALID_HANDLE_VALUE;static SC_HANDLE scm,svc;static DWORD cx_err,bar1;
typedef short (__stdcall *Inp32_t)(short);typedef void (__stdcall *Out32_t)(short,short);
typedef DWORD (__stdcall *DlRdL_t)(DWORD);typedef void (__stdcall *DlWrL_t)(DWORD,DWORD);
static DlRdL_t pRdL;static DlWrL_t pWrL;
static void cx_start(void) {
 char path[MAX_PATH],*e;
 GetModuleFileNameA(0,path,sizeof(path));e=strrchr(path,'\\');if(e)strcpy(e+1,"CXPHYS.SYS");
 if(GetFileAttributesA(path)==0xFFFFFFFFUL){cx_err=1;return;}
 scm=OpenSCManagerA(0,0,SC_MANAGER_ALL_ACCESS);if(!scm){cx_err=GetLastError();return;}
 svc=CreateServiceA(scm,"CXPHYS","CXPHYS",SERVICE_ALL_ACCESS,SERVICE_KERNEL_DRIVER,SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,path,0,0,0,0,0);
 if(!svc)svc=OpenServiceA(scm,"CXPHYS",SERVICE_ALL_ACCESS);
 if(!svc){cx_err=GetLastError();return;}
 if(!StartServiceA(svc,0,0)&&GetLastError()!=ERROR_SERVICE_ALREADY_RUNNING){cx_err=GetLastError();return;}
 cxdev=CreateFileA("\\\\.\\CXPHYS",GENERIC_READ,0,0,OPEN_EXISTING,0,0);if(cxdev==INVALID_HANDLE_VALUE)cx_err=GetLastError();
}
static void cx_stop(void) {
 SERVICE_STATUS st;
 if(cxdev!=INVALID_HANDLE_VALUE){CloseHandle(cxdev);cxdev=INVALID_HANDLE_VALUE;}
 if(svc){ControlService(svc,SERVICE_CONTROL_STOP,&st);DeleteService(svc);CloseServiceHandle(svc);svc=0;}
 if(scm){CloseServiceHandle(scm);scm=0;}
}
static int cx_read(DWORD phys,DWORD *o,DWORD len) {DWORD q[4],got;q[0]=phys;q[1]=0;q[2]=len;q[3]=0;
 return cxdev!=INVALID_HANDLE_VALUE&&DeviceIoControl(cxdev,CX_IOCTL_READ,q,16,o,len,&got,0)&&got==len;}
static DWORD pci_rd(DWORD bus,DWORD dev,DWORD fn,DWORD reg) {pWrL(0xCF8,0x80000000UL|(bus<<16)|(dev<<11)|(fn<<8)|(reg&0xFC));return pRdL(0xCFC);}
/* BAR1 (the registers): the PCI scan when inpout32.dll is there, otherwise the PnP resource list (the 16 MB memory range) */
static void find_bar1(void) {
 HMODULE io=LoadLibraryA("inpout32.dll");DWORD bus,dev;
 if(io){pRdL=(DlRdL_t)GetProcAddress(io,"DlPortReadPortUlong");pWrL=(DlWrL_t)GetProcAddress(io,"DlPortWritePortUlong");}
 if(pRdL&&pWrL)for(bus=0;bus<4&&!bar1;bus++)for(dev=0;dev<32;dev++){DWORD id=pci_rd(bus,dev,0,0);if(id==0x51221106UL){bar1=pci_rd(bus,dev,0,0x14)&~15UL;break;}}
 if(!bar1){HKEY k,d,i;char name[256],devn[256],sub[300];DWORD a,b;
  if(!RegOpenKeyExA(HKEY_LOCAL_MACHINE,"SYSTEM\\CurrentControlSet\\Enum\\PCI",0,KEY_READ,&k)){
   for(a=0;!bar1&&!RegEnumKeyA(k,a,devn,sizeof(devn));a++){if(_strnicmp(devn,"VEN_1106&DEV_5122",17)||RegOpenKeyExA(k,devn,0,KEY_READ,&d))continue;
    for(b=0;!bar1&&!RegEnumKeyA(d,b,name,sizeof(name));b++){static BYTE buf[4096];DWORD type,size=sizeof(buf),cnt,k2;sprintf(sub,"%s\\Control",name);
     if(RegOpenKeyExA(d,sub,0,KEY_READ,&i))continue;
     if(!RegQueryValueExA(i,"AllocConfig",0,&type,buf,&size)&&size>=20){cnt=*(DWORD*)(buf+16);
      for(k2=0;k2<cnt&&20+k2*16+16<=size;k2++){BYTE *e=buf+20+k2*16;if(e[0]==3&&*(DWORD*)(e+12)==0x01000000UL){bar1=*(DWORD*)(e+4);break;}}}
     RegCloseKey(i);}
    RegCloseKey(d);}
   RegCloseKey(k);}}
 out("registers: CXPHYS %s, BAR1 %08lX (%s)\n",cxdev!=INVALID_HANDLE_VALUE?"running":cx_err==1?"CXPHYS.SYS missing next to the EXE":"not started",(unsigned long)bar1,pRdL?"PCI":"PnP list");
}
typedef struct {DWORD a[128],b[128];int ok;} dump_t; /* 0x200-0x3FF, 0x1200-0x13FF */
static int take(dump_t *d) {d->ok=bar1&&cx_read(bar1+0x200,d->a,512)&&cx_read(bar1+0x1200,d->b,512);return d->ok;}
static void diff(const char *what,const dump_t *x,const dump_t *y) {
 int k,n=0;
 if(!x->ok||!y->ok){out("   registers: not read\n");return;}
 out("   registers changed %s:",what);
 for(k=0;k<128;k++)if(x->a[k]!=y->a[k]){out("%s %03X %08lX->%08lX",n++%4?",":"\n     ",0x200+k*4,(unsigned long)x->a[k],(unsigned long)y->a[k]);}
 for(k=0;k<128;k++)if(x->b[k]!=y->b[k]){out("%s %04X %08lX->%08lX",n++%4?",":"\n     ",0x1200+k*4,(unsigned long)x->b[k],(unsigned long)y->b[k]);}
 out("%s\n",n?"":" none");
}
static void full(const char *what,const dump_t *d) {
 int k;if(!d->ok)return;
 out("   all registers %s:\n",what);
 for(k=0;k<128;k+=8)out("     %03X: %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX\n",0x200+k*4,d->a[k],d->a[k+1],d->a[k+2],d->a[k+3],d->a[k+4],d->a[k+5],d->a[k+6],d->a[k+7]);
 for(k=0;k<128;k+=8)out("     %04X: %08lX %08lX %08lX %08lX %08lX %08lX %08lX %08lX\n",0x1200+k*4,d->b[k],d->b[k+1],d->b[k+2],d->b[k+3],d->b[k+4],d->b[k+5],d->b[k+6],d->b[k+7]);
}

/* ---- the picture: a grey screen with a key square, overlays with colour bars ---- */
static const GUID iid_dd7={0x15E65EC0UL,0x3B9C,0x11D2,{0xB9,0x2F,0,0x60,0x97,0x97,0xEA,0x5B}};
static IDirectDraw7 *dd;static IDirectDrawSurface7 *pri,*ova,*ovb;static HWND win;
static LRESULT CALLBACK wproc(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_ERASEBKGND){RECT r;HBRUSH g=CreateSolidBrush(RGB(128,128,128)),k=CreateSolidBrush(RGB(255,0,255));GetClientRect(w,&r);FillRect((HDC)a,&r,g);
  SetRect(&r,60,60,180,180);FillRect((HDC)a,&r,k);SetRect(&r,260,60,380,180);FillRect((HDC)a,&r,k);DeleteObject(g);DeleteObject(k);return 1;}
 return DefWindowProc(w,m,a,b);
}
static void pump(DWORD ms) {DWORD t=GetTickCount();MSG m;while(GetTickCount()-t<ms){while(PeekMessage(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}Sleep(10);}}
static void make_window(void) {
 WNDCLASSA wc;memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wproc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="CLXPOVL";wc.hCursor=LoadCursor(NULL,IDC_ARROW);RegisterClassA(&wc);
 win=CreateWindowExA(WS_EX_TOPMOST,"CLXPOVL","CLXPOVL",WS_POPUP|WS_VISIBLE,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,wc.hInstance,NULL);pump(300);
}
static void fill_bars(IDirectDrawSurface7 *s,int shift) {
 static const DWORD yuy2[4]={0x5A515AF0UL,0x22913691UL,0xF0296E29UL,0x80108010UL}; /* red, green, blue, black (Y0 U Y1 V) */
 DDSURFACEDESC2 d;DWORD y,x;memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
 if(FAILED(IDirectDrawSurface7_Lock(s,NULL,&d,DDLOCK_WAIT|DDLOCK_WRITEONLY,NULL)))return;
 for(y=0;y<d.dwHeight;y++){DWORD *p=(DWORD*)((BYTE*)d.lpSurface+y*d.lPitch);for(x=0;x<d.dwWidth/2;x++)p[x]=yuy2[((x*2*4/d.dwWidth)+shift)&3];}
 IDirectDrawSurface7_Unlock(s,NULL);
}
static IDirectDrawSurface7 *make_overlay(const char *name,int shift) {
 DDSURFACEDESC2 s;IDirectDrawSurface7 *o=0;HRESULT hr;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;s.dwWidth=128;s.dwHeight=96;
 s.ddsCaps.dwCaps=DDSCAPS_OVERLAY|DDSCAPS_VIDEOMEMORY;s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);s.ddpfPixelFormat.dwFlags=DDPF_FOURCC;s.ddpfPixelFormat.dwFourCC=MAKEFOURCC('Y','U','Y','2');
 hr=IDirectDraw7_CreateSurface(dd,&s,&o,NULL);out("   CreateSurface(%s, YUY2 128x96 overlay) = %08lX\n",name,(unsigned long)hr);
 if(SUCCEEDED(hr))fill_bars(o,shift);
 return SUCCEEDED(hr)?o:0;
}
static int open_dd(void) {
 HRESULT (WINAPI *create)(GUID *,void **,REFIID,IUnknown *);HMODULE l=LoadLibraryA("DDRAW.DLL");DDSURFACEDESC2 s;HRESULT hr;
 create=l?(HRESULT (WINAPI *)(GUID *,void **,REFIID,IUnknown *))GetProcAddress(l,"DirectDrawCreateEx"):0;
 if(!create||FAILED(hr=create(NULL,(void**)&dd,&iid_dd7,NULL))){out("   DirectDrawCreateEx failed\n");return 0;}
 IDirectDraw7_SetCooperativeLevel(dd,NULL,DDSCL_NORMAL);
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS;s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
 if(FAILED(hr=IDirectDraw7_CreateSurface(dd,&s,&pri,NULL))){out("   primary %08lX\n",(unsigned long)hr);return 0;}
 return 1;
}
static void close_dd(void) {
 if(ova){IDirectDrawSurface7_UpdateOverlay(ova,NULL,pri,NULL,DDOVER_HIDE,NULL);IDirectDrawSurface7_Release(ova);ova=0;}
 if(ovb){IDirectDrawSurface7_UpdateOverlay(ovb,NULL,pri,NULL,DDOVER_HIDE,NULL);IDirectDrawSurface7_Release(ovb);ovb=0;}
 if(pri){IDirectDrawSurface7_Release(pri);pri=0;}
 if(dd){IDirectDraw7_Release(dd);dd=0;}
}
static void caps(void) {
 DDCAPS c,h;memset(&c,0,sizeof(c));c.dwSize=sizeof(c);memset(&h,0,sizeof(h));h.dwSize=sizeof(h);
 if(FAILED(IDirectDraw7_GetCaps(dd,&c,&h))){out("   GetCaps failed\n");return;}
 out("   caps: dwCaps %08lX (OVERLAY %d, ALPHA %d, COLORKEY %d) dwCaps2 %08lX dwCKeyCaps %08lX (DESTOVERLAY %d, DESTOVERLAYCLRSPACE %d, DESTOVERLAYONEACTIVE %d, DESTOVERLAYYUV %d)\n",
  c.dwCaps,(c.dwCaps&DDCAPS_OVERLAY)!=0,(c.dwCaps&DDCAPS_ALPHA)!=0,(c.dwCaps&DDCAPS_COLORKEY)!=0,c.dwCaps2,c.dwCKeyCaps,(c.dwCKeyCaps&DDCKEYCAPS_DESTOVERLAY)!=0,
  (c.dwCKeyCaps&DDCKEYCAPS_DESTOVERLAYCLRSPACE)!=0,(c.dwCKeyCaps&DDCKEYCAPS_DESTOVERLAYONEACTIVE)!=0,(c.dwCKeyCaps&DDCKEYCAPS_DESTOVERLAYYUV)!=0);
 out("   overlays: at most %lu visible, %lu now; dwFXAlphaCaps %08lX; constant overlay alpha bit depths %08lX, overlay alpha surface depths %08lX; dwFXCaps %08lX\n",
  c.dwMaxVisibleOverlays,c.dwCurrVisibleOverlays,c.dwFXAlphaCaps,c.dwAlphaOverlayConstBitDepths,c.dwAlphaOverlaySurfaceBitDepths,c.dwFXCaps);
}
static HRESULT show(IDirectDrawSurface7 *o,int x,DWORD flags,DDOVERLAYFX *fx) {
 RECT sr,dr;SetRect(&sr,0,0,128,96);SetRect(&dr,x,40,x+160,200);return IDirectDrawSurface7_UpdateOverlay(o,&sr,pri,&dr,DDOVER_SHOW|flags,fx);
}
static const char *ans(int a) {return a==IDYES?"yes":a==IDNO?"NO":"cancel";}

int main(void) {
 SYSTEMTIME t;char path[MAX_PATH],*e;DEVMODE cur;dump_t d0,d1,d2,d3,d4,d5;DDOVERLAYFX fx;HRESULT hr;DWORD key;int a;
 GetModuleFileNameA(NULL,path,sizeof(path));e=strrchr(path,'\\');if(e)strcpy(e+1,"CLXPOVL.TXT");else strcpy(path,"CLXPOVL.TXT");
 lf=fopen(path,"a");GetLocalTime(&t);
 memset(&cur,0,sizeof(cur));cur.dmSize=sizeof(cur);EnumDisplaySettings(NULL,ENUM_CURRENT_SETTINGS,&cur);
 out("\n==== CLXPOVL v1.0 %04u-%02u-%02u %02u:%02u:%02u; %lux%lux%lu ====\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,cur.dmPelsWidth,cur.dmPelsHeight,cur.dmBitsPerPel);
 if(cur.dmBitsPerPel!=32)out("   (the first three steps want a 32 bpp desktop; this one is %lu bpp)\n",cur.dmBitsPerPel);
 cx_start();find_bar1();
 make_window();
 if(!open_dd()){close_dd();cx_stop();if(lf)fclose(lf);return 1;}
 caps();take(&d0);full("before any overlay",&d0);
 /* 1: V1 with the destination key */
 out("== 1: one overlay, destination colour key (magenta) on the %lu bpp desktop\n",cur.dmBitsPerPel);
 ova=make_overlay("A",0);
 if(ova){memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);key=cur.dmBitsPerPel==16?0xF81FUL:0xFF00FFUL;fx.dckDestColorkey.dwColorSpaceLowValue=fx.dckDestColorkey.dwColorSpaceHighValue=key;
  hr=show(ova,40,DDOVER_KEYDESTOVERRIDE,&fx);out("   UpdateOverlay(A, KEYDESTOVERRIDE %06lX) = %08lX\n",(unsigned long)key,(unsigned long)hr);pump(500);take(&d1);diff("for overlay A",&d0,&d1);
  a=ask("Step 1: do you see coloured bars inside the LEFT magenta square only (grey around it)?");out("   you saw it inside the square only: %s\n",ans(a));
  /* 2: constant alpha */
  out("== 2: constant overlay alpha, 50%%\n");
  memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwAlphaSrcConstBitDepth=8;fx.dwAlphaSrcConst=128;
  hr=show(ova,40,DDOVER_ALPHASRCCONSTOVERRIDE,&fx);out("   UpdateOverlay(A, ALPHASRCCONSTOVERRIDE 128/255) = %08lX\n",(unsigned long)hr);pump(500);take(&d2);diff("for the source constant alpha",&d1,&d2);
  a=ask("Step 2a: is overlay A half transparent now (the grey and magenta behind it showing through)?");out("   you saw it half transparent: %s\n",ans(a));
  memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwAlphaDestConstBitDepth=8;fx.dwAlphaDestConst=128;
  hr=show(ova,40,DDOVER_ALPHADESTCONSTOVERRIDE,&fx);out("   UpdateOverlay(A, ALPHADESTCONSTOVERRIDE 128/255) = %08lX\n",(unsigned long)hr);pump(500);take(&d3);diff("for the destination constant alpha",&d2,&d3);
  a=ask("Step 2b: is overlay A half transparent now?");out("   you saw it half transparent: %s\n",ans(a));
  /* 3: a second overlay */
  out("== 3: a second overlay at the same time\n");
  hr=show(ova,40,0,NULL);out("   UpdateOverlay(A, plain) = %08lX\n",(unsigned long)hr);pump(300);take(&d3);
  ovb=make_overlay("B",2);
  if(ovb){memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dckDestColorkey.dwColorSpaceLowValue=fx.dckDestColorkey.dwColorSpaceHighValue=key;
   hr=show(ovb,240,DDOVER_KEYDESTOVERRIDE,&fx);out("   UpdateOverlay(B, KEYDESTOVERRIDE) = %08lX\n",(unsigned long)hr);pump(500);take(&d4);diff("for overlay B",&d3,&d4);full("with both overlays",&d4);
   a=ask("Step 3: do you see TWO overlays (A on the left, B with other bars inside the right magenta square)?");out("   you saw two overlays: %s\n",ans(a));}
 }
 close_dd();
 /* 4: an 8 bpp desktop */
 out("== 4: destination colour key on an 8 bpp desktop\n");
 {DEVMODE m=cur;LONG r;m.dmBitsPerPel=8;m.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;r=ChangeDisplaySettings(&m,CDS_FULLSCREEN);out("   ChangeDisplaySettings(%lux%lux8) = %ld\n",cur.dmPelsWidth,cur.dmPelsHeight,(long)r);
  if(r==DISP_CHANGE_SUCCESSFUL){pump(1500);InvalidateRect(win,NULL,TRUE);pump(500);
   if(open_dd()){DDSURFACEDESC2 s;DWORD y;BYTE idx=0;take(&d5);
    /* the key: the palette index the magenta square was drawn with, read from the screen */
    memset(&s,0,sizeof(s));s.dwSize=sizeof(s);if(SUCCEEDED(IDirectDrawSurface7_Lock(pri,NULL,&s,DDLOCK_WAIT|DDLOCK_READONLY,NULL))){idx=((BYTE*)s.lpSurface)[120*s.lPitch+120];IDirectDrawSurface7_Unlock(pri,NULL);}
    (void)y;out("   the magenta square reads palette index %u\n",idx);
    ova=make_overlay("A at 8 bpp",1);
    if(ova){memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dckDestColorkey.dwColorSpaceLowValue=fx.dckDestColorkey.dwColorSpaceHighValue=idx;
     hr=show(ova,40,DDOVER_KEYDESTOVERRIDE,&fx);out("   UpdateOverlay(A, KEYDESTOVERRIDE index %u) = %08lX\n",idx,(unsigned long)hr);pump(500);
     {dump_t d6;take(&d6);diff("for the 8 bpp keyed overlay",&d5,&d6);full("8 bpp, keyed overlay",&d6);}
     a=ask("Step 4 (8 bpp): do you see coloured bars inside the LEFT magenta square only?");out("   you saw it inside the square only: %s\n",ans(a));}
    close_dd();}
   r=ChangeDisplaySettings(NULL,0);out("   back to %lux%lux%lu: %ld\n",cur.dmPelsWidth,cur.dmPelsHeight,cur.dmBitsPerPel,(long)r);}
 }
 DestroyWindow(win);cx_stop();
 out("==== done ====\n");if(lf)fclose(lf);
 MessageBoxA(NULL,"CLXPOVL is done: CLXPOVL.TXT is next to the program.","CLXPOVL",MB_OK|MB_TOPMOST);
 return 0;
}
