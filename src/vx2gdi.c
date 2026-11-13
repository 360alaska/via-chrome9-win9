/* VX2GDI v2.0 (v2.0: text cases and driver notes removed with the text path; a failing case is checked against a second DIBENG run): GDI drawing from system memory of driver v10.116 (ACCEL_HOST): memory bitmap -> screen BitBlt (1-bit SRCCOPY through the host window, ROPs that read the screen through the staging block),
 * text (DIBENG draws into a 1-bit buffer, the engine draws it), scan line spans with ROPs that read the screen. Every case is drawn twice at 640x480x16 and 640x480x32: with the new paths off
 * (live mask 0: DIBENG) and on (live mask 31), the screen is read back and compared pixel for pixel, and the driver's counters show whether the new path really drew it. Then each case is timed both ways
 * with the shipped size thresholds. Nothing is written to the registry. Unattended: read VX2GDI.TXT (VX2GDI.RES says RUNNING until it ends). Esc stops. */
#include <windows.h>
#include <stdint.h>
#include "vx_hatch_diag.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2GDI.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1; return !aborted;}
static double now(void) {LARGE_INTEGER c,f; if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;} return (double)GetTickCount();}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); {DWORD t=GetTickCount(); while(GetTickCount()-t<1000) {pump(); Sleep(20);}}
 return rc==DISP_CHANGE_SUCCESSFUL;
}
/* driver control: 0xFFFFFE18 counters (XF_MONO, XF_STAGE, XF_TEXT, spans), 0xFFFFFE19 (handed back, last reason, live mask, staging block), 0xF7000000|m live mask, 5 = all copy directions + no size thresholds, 0xFFFFFFFF registry values */
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static void setmask(DWORD m) {ctl(0xF7000000UL|(m&63UL),NULL);}

#define AW 600
#define AH 440
static HDC sdc,tdc; static HBITMAP tbm,told; static DWORD *tbits,*bufA;
static int make_target(void) {
 BITMAPINFOHEADER b; void *p=NULL; memset(&b,0,sizeof(b)); b.biSize=sizeof(b); b.biWidth=AW; b.biHeight=-AH; b.biPlanes=1; b.biBitCount=32;
 tbm=CreateDIBSection(sdc,(BITMAPINFO*)&b,DIB_RGB_COLORS,&p,NULL,0); if(!tbm||!p) return 0;
 tdc=CreateCompatibleDC(sdc); told=(HBITMAP)SelectObject(tdc,tbm); tbits=(DWORD*)p; return 1;
}
static void drop_target(void) {SelectObject(tdc,told); DeleteObject(tbm); DeleteDC(tdc);}
static void shot(void) {GdiFlush(); BitBlt(tdc,0,0,AW,AH,sdc,0,0,SRCCOPY); GdiFlush();}
static void background(void) { /* colourful and deterministic, so ROPs that read the screen have something to read */
 int y,x; for(y=0;y<AH;y+=8) for(x=0;x<AW;x+=40) {RECT r; HBRUSH b=CreateSolidBrush(RGB((x*7+y*3)&255,(x*2+y*5+80)&255,(x*11+y)&255)); r.left=x; r.top=y; r.right=x+40; r.bottom=y+8; FillRect(sdc,&r,b); DeleteObject(b);}
 GdiFlush();
}
static DWORD seed=20261021UL; static DWORD rnd(void) {seed=seed*1103515245UL+12345UL; return (seed>>8)&0xFFFFFFUL;}

/* ---- drawing objects ---- */
static HBITMAP mono1,mono2,cbm,dib32,dibbu,dib555; static HDC mdc; static HFONT fonts[7]; static const char *fname[7]={"System","ANSI_VAR_FONT","MS Sans Serif 8","Arial 14 (TrueType)","Courier New 12 bold","Times New Roman 20 italic","Arial 9"};
static HBITMAP mkmono(int w,int h) {int n=((w+15)/16)*2*h,i; BYTE *b=(BYTE*)malloc(n); HBITMAP m; for(i=0;i<n;i++) b[i]=(BYTE)rnd(); m=CreateBitmap(w,h,1,1,b); free(b); return m;}
static HBITMAP mkdib(int w,int h,int bits,int topdown,int f565) {
 struct {BITMAPINFOHEADER h; DWORD m[3];} bi; void *p=NULL; HBITMAP r; DWORD n,i;
 memset(&bi,0,sizeof(bi)); bi.h.biSize=sizeof(bi.h); bi.h.biWidth=w; bi.h.biHeight=topdown?-h:h; bi.h.biPlanes=1; bi.h.biBitCount=(WORD)bits;
 if(f565) {bi.h.biCompression=BI_BITFIELDS; bi.m[0]=0xF800; bi.m[1]=0x07E0; bi.m[2]=0x001F;}
 r=CreateDIBSection(sdc,(BITMAPINFO*)&bi,DIB_RGB_COLORS,&p,NULL,0); if(!r||!p) return r;
 n=(DWORD)(((w*bits+31)/32)*4)*h; for(i=0;i<n;i++) ((BYTE*)p)[i]=(BYTE)rnd(); GdiFlush(); return r;
}
static void blit_from(HBITMAP b,int dx,int dy,int w,int h,int sx,int sy,DWORD rop) {HBITMAP o=(HBITMAP)SelectObject(mdc,b); BitBlt(sdc,dx,dy,w,h,mdc,sx,sy,rop); SelectObject(mdc,o);}
static const char *txt="The quick brown fox jumps over the lazy dog 0123456789 &%$#@!";

/* ---- cases: k = the variant (the same k draws the same thing) ---- */
typedef void (*drawfn)(void);
static void c_mono_copy(void) {SetTextColor(sdc,RGB(200,30,40)); SetBkColor(sdc,RGB(20,220,90)); blit_from(mono1,20,20,256,160,0,0,SRCCOPY); blit_from(mono1,300,30,77,50,13,7,SRCCOPY); blit_from(mono2,40,220,500,90,3,0,SRCCOPY); blit_from(mono2,590,400,40,30,0,0,SRCCOPY);}
static void c_mono_rops(void) {SetTextColor(sdc,RGB(0,0,0)); SetBkColor(sdc,RGB(255,255,255)); blit_from(mono1,20,20,256,160,0,0,SRCAND); blit_from(mono1,300,20,120,100,5,9,SRCPAINT); SetTextColor(sdc,RGB(10,120,250)); SetBkColor(sdc,RGB(250,250,0)); blit_from(mono2,40,220,500,90,0,0,SRCINVERT); blit_from(mono1,450,150,64,64,1,1,0x00220326UL /* DSna */);}
static void c_sprite(void) {SetTextColor(sdc,RGB(0,0,0)); SetBkColor(sdc,RGB(255,255,255)); blit_from(mono1,50,60,200,150,0,0,SRCAND); blit_from(cbm,50,60,200,150,0,0,SRCINVERT); blit_from(mono1,320,60,32,32,0,0,SRCAND); blit_from(cbm,320,60,32,32,0,0,SRCINVERT); blit_from(cbm,360,60,16,16,0,0,SRCPAINT);}
static void c_dib(void) {blit_from(dib32,20,20,300,200,7,3,SRCINVERT); blit_from(dibbu,340,20,200,200,0,11,SRCAND); blit_from(dibbu,20,240,250,150,9,0,SRCPAINT); blit_from(dib555,300,240,250,150,0,0,SRCINVERT); blit_from(dib32,560,400,60,60,0,0,0x00990066UL /* DSxn */);}
/* v2.3 (.595): brushes that are not one colour as the pattern of a ROP that also reads a bitmap and the screen: a brush made from an
 * 8x8 colour bitmap, with a brush origin, at odd positions and sizes; and a dithered colour (c_brushrop's own colour is dithered at 16 bpp) */
static HBITMAP patbm;
static void c_patrop(void) {
 HBRUSH b=CreatePatternBrush(patbm),d=CreateSolidBrush(RGB(101,67,33)),o; POINT po;
 SetBrushOrgEx(sdc,3,5,&po); o=(HBRUSH)SelectObject(sdc,b);
 blit_from(cbm,33,31,250,180,0,0,0x00B8074AUL /* PSDPxax */); blit_from(cbm,321,37,200,150,5,7,0x00E20746UL /* DSPDxax */); blit_from(cbm,601,401,9,9,1,1,0x00B8074AUL);
 SelectObject(sdc,d); SetBrushOrgEx(sdc,0,0,NULL);
 blit_from(cbm,40,250,255,170,1,3,0x00B8074AUL); blit_from(cbm,330,260,180,111,0,0,0x00E20746UL);
 SelectObject(sdc,o); SetBrushOrgEx(sdc,po.x,po.y,NULL); DeleteObject(b); DeleteObject(d);
}
static void c_brushrop(void) {HBRUSH b=CreateSolidBrush(RGB(90,30,200)),o=(HBRUSH)SelectObject(sdc,b); blit_from(cbm,30,30,250,180,0,0,0x00B8074AUL /* PSDPxax */); blit_from(dib32,320,30,250,180,0,0,0x00E20746UL /* DSPDxax */); SelectObject(sdc,o); DeleteObject(b);}
/* v10.120: text into 1-bit memory targets (a GDI bitmap and a DIB section): only to capture what GDI hands DIBENG; the screen is not touched */
static void c_spans(void) {
 POINT star[5]={{150,30},{230,260},{40,110},{260,110},{70,260}}; HBRUSH s=CreateSolidBrush(RGB(120,200,40)),h=CreateHatchBrush(HS_DIAGCROSS,RGB(250,0,250)),o; HPEN p=(HPEN)SelectObject(sdc,GetStockObject(NULL_PEN));
 SetPolyFillMode(sdc,WINDING); SetROP2(sdc,R2_XORPEN); o=(HBRUSH)SelectObject(sdc,s); Polygon(sdc,star,5); Ellipse(sdc,300,30,580,220); RoundRect(sdc,30,280,280,420,40,40);
 SetROP2(sdc,R2_NOT); Pie(sdc,320,240,560,430,560,330,320,330);
 SetROP2(sdc,R2_XORPEN); SelectObject(sdc,h); SetBkColor(sdc,RGB(0,0,0)); Ellipse(sdc,200,150,420,330);
 SetROP2(sdc,R2_MASKPEN); SelectObject(sdc,s); Ellipse(sdc,450,20,595,160);
 SetROP2(sdc,R2_COPYPEN); Ellipse(sdc,5,380,120,435);
 SelectObject(sdc,o); SelectObject(sdc,p); DeleteObject(s); DeleteObject(h);
}
typedef struct {const char *name; drawfn fn; int kind;} case_t; /* kind: counter index that should move (0 mono, 1 staged, 2 text, 3 spans) */
static const case_t cases[]={
 {"1-bit bitmap SRCCOPY (host window)",c_mono_copy,0},{"1-bit bitmap SRCAND/SRCPAINT/SRCINVERT/DSna (staging)",c_mono_rops,1},{"sprite: 1-bit mask SRCAND + compatible bitmap SRCINVERT/SRCPAINT",c_sprite,1},
 {"DIB sections of the screen depth (top-down, bottom-up) and 5-5-5, ROPs that read the screen",c_dib,1},{"brush ROPs PSDPxax / DSPDxax from memory bitmaps",c_brushrop,1},{"patterned and dithered brushes with PSDPxax / DSPDxax (.595)",c_patrop,1},

 {"filled shapes with R2_XORPEN / R2_NOT / R2_MASKPEN (solid and hatched brushes), one R2_COPYPEN",c_spans,3}};
#define NCASE (sizeof(cases)/sizeof(cases[0]))

static int run_case(int bpp,int ci,int *engaged) {
 DWORD c0[4],c1[4],k0[4],k1[4]; int x,y,diff=0,fx=-1,fy=-1; DWORD fa=0,fb=0,cm=0xFFFFFFUL; int bx0=AW,by0=AH,bx1=-1,by1=-1;
 background(); setmask(0); cases[ci].fn(); shot(); memcpy(bufA,tbits,AW*AH*4);
 background(); ctl(0xFFFFFE18UL,c0); ctl(0xFFFFFE19UL,k0); setmask(31); cases[ci].fn(); GdiFlush(); setmask(31); ctl(0xFFFFFE18UL,c1); ctl(0xFFFFFE19UL,k1); shot(); setmask(0);
 for(y=0;y<AH;y++) for(x=0;x<AW;x++) {DWORD a=bufA[y*AW+x]&cm,b=tbits[y*AW+x]&cm; if(a!=b) {if(!diff) {fx=x; fy=y; fa=a; fb=b;} diff++; if(x<bx0) bx0=x; if(x>bx1) bx1=x; if(y<by0) by0=y; if(y>by1) by1=y;}}
 *engaged=(int)(c1[cases[ci].kind]-c0[cases[ci].kind]);
 logline("  %-95s %s: %d differing pixels%s; new path drew %d (mono %lu, staged %lu, text %lu, spans %lu); handed back %lu (last reason %lu)\n",cases[ci].name,diff?"FAIL":"ok",diff,"",*engaged,c1[0]-c0[0],c1[1]-c0[1],c1[2]-c0[2],c1[3]-c0[3],k1[0]-k0[0],k1[1]);
 if(diff) {
  int d2=0; logline("     first difference at %d,%d: DIBENG %06lX, new path %06lX\n",fx,fy,fa,fb);
  background(); setmask(0); cases[ci].fn(); shot(); for(y=0;y<AH;y++) for(x=0;x<AW;x++) if((bufA[y*AW+x]&cm)!=(tbits[y*AW+x]&cm)) d2++;
  logline("     DIBENG drawn a second time: %d pixels differ from its first run%s\n",d2,d2?" (the drawing itself is not repeatable: not the new path)":"");
  /* v2.1 (.582; VX800 board: 58955 pixels differed once, more than the case draws, and nothing said where or whether it would again) */
  {int d3=0; logline("     the differing pixels lie in %d,%d to %d,%d (%d x %d)\n",bx0,by0,bx1,by1,bx1-bx0+1,by1-by0+1);
   background(); setmask(31); cases[ci].fn(); GdiFlush(); shot(); setmask(0); for(y=0;y<AH;y++) for(x=0;x<AW;x++) if((bufA[y*AW+x]&cm)!=(tbits[y*AW+x]&cm)) d3++;
   logline("     the new path drawn a second time: %d pixels differ from DIBENG%s\n",d3,d3?"":" (the first difference did not come back: something else was on the screen for a moment)");}
 }

 (void)bpp; return diff==0;
}
static void time_case(int ci) {
 double t[2]; int m,i,n=8;
 for(m=0;m<2;m++) {double s; background(); setmask(m?31:0); s=now(); for(i=0;i<n&&pump();i++) cases[ci].fn(); GdiFlush(); t[m]=(now()-s)/n;}
 setmask(0);
 logline("  %-95s DIBENG %8.2f ms, new path %8.2f ms (%s)\n",cases[ci].name,t[0],t[1],t[1]<t[0]?"faster":"slower");
}
/* A blinking caret of whatever window has the focus shows through the screen DC these cases draw on (VX855 and VX800 boards, .593 and
 * .594: 7 pixels at x = 6 with complementary colours, in a different case every run, DIBENG against itself too). The focus goes to a
 * window of this tool's own, which has no caret; Windows takes the old window's caret away with its focus. Returns what happened. */
/* (.595 on the VX800 board: 480 pixels in a 16x30 block of the untouched background differed once and not again: another window
 * painted a piece of itself between the two pictures.) The window now covers the whole screen, topmost, and never paints or erases:
 * the windows under it are clipped away, and the cases draw over it through the screen DC as before. */
static LRESULT CALLBACK cover_proc(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_ERASEBKGND) return 1;
 if(m==WM_PAINT) {ValidateRect(w,NULL); return 0;}
 return DefWindowProcA(w,m,a,b);
}
static HWND nocaret_w;
static int no_caret(void) {
 HWND fg=GetForegroundWindow(); DWORD me=GetCurrentThreadId(),other=fg?GetWindowThreadProcessId(fg,NULL):0; int att=0,ok; MSG m; DWORD t;
 {WNDCLASSA wc; memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=cover_proc; wc.hInstance=GetModuleHandleA(NULL); wc.lpszClassName="VXCOVER"; wc.hCursor=LoadCursorA(NULL,IDC_ARROW); RegisterClassA(&wc);}
 nocaret_w=CreateWindowExA(WS_EX_TOPMOST|WS_EX_TOOLWINDOW,"VXCOVER","",WS_POPUP,0,0,4096,4096,NULL,NULL,GetModuleHandleA(NULL),NULL);
 if(!nocaret_w) return 0;
 ShowWindow(nocaret_w,SW_SHOW);
 if(other&&other!=me) att=AttachThreadInput(me,other,TRUE)?1:0;
 SetForegroundWindow(nocaret_w); SetActiveWindow(nocaret_w); SetFocus(nocaret_w);
 if(att) AttachThreadInput(me,other,FALSE);
 for(t=GetTickCount();GetTickCount()-t<300;) {while(PeekMessageA(&m,NULL,0,0,PM_REMOVE)) DispatchMessageA(&m); Sleep(10);}
 ok=GetForegroundWindow()==nocaret_w;
 return ok?1:2;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 DWORD c[4]; int phase,ci,fails=0,untested=0,total=0,have; DEVMODEA origm; char res[400]; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2GDI.TXT","w"); logline("VX2GDI v2.6 / driver .597\n"); finish("RUNNING: the run did not reach its end (VX2GDI.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xFFFFFE19UL,c)) {finish("FAIL: no control interface"); return 1;}
 setmask(31); ctl(0xFFFFFE19UL,c); logline("live mask %lu (31 expected), staging block at %08lX\n",c[2],c[3]);
 if(c[2]!=31) {finish("FAIL: VX85598.VXD older than v10.116 (no ACCEL_HOST mask)"); return 1;}
 if(!c[3]) logline("WARNING: no staging block (video memory too small?): the staged cases cannot use the new path\n");
 {int nc=no_caret(); logline("the focus moved to this tool's own window (no caret on the screen): %s\n",nc==1?"yes":nc==2?"NO: another window kept the foreground; a blinking caret may show as 7 differing pixels":"NO: no window");}
 bufA=(DWORD*)calloc(AW*AH,4); if(!bufA) {finish("FAIL: out of memory"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 for(phase=0;phase<3&&!aborted;phase++) {
  int bpp=phase==2?8:phase?32:16; /* v2.3: 256 colours too, the two brush cases only (the other cases' paths are not offered there) */
  if(!set_mode(640,480,bpp)) {logline("640x480x%d not available: skipped\n",bpp); untested++; continue;}
  sdc=GetDC(NULL); if(!sdc||!make_target()) {logline("no screen DC / target: skipped\n"); untested++; continue;}
  mdc=CreateCompatibleDC(sdc);
  mono1=mkmono(256,160); mono2=mkmono(509,90);
  cbm=CreateCompatibleBitmap(sdc,256,200); {HBITMAP o=(HBITMAP)SelectObject(mdc,cbm); int y,x; for(y=0;y<200;y+=4) for(x=0;x<256;x+=4) {RECT r; HBRUSH b=CreateSolidBrush(RGB(rnd()&255,rnd()&255,rnd()&255)); r.left=x; r.top=y; r.right=x+4; r.bottom=y+4; FillRect(mdc,&r,b); DeleteObject(b);} SelectObject(mdc,o);}
  patbm=CreateCompatibleBitmap(sdc,8,8); {HBITMAP o=(HBITMAP)SelectObject(mdc,patbm); int y,x; for(y=0;y<8;y++) for(x=0;x<8;x++) SetPixel(mdc,x,y,RGB((x*36+y*5)&255,(y*33+x*9+40)&255,((x^y)*31+17)&255)); SelectObject(mdc,o);}
  dib32=mkdib(320,240,bpp,1,bpp==16); dibbu=mkdib(260,220,bpp,0,bpp==16); dib555=mkdib(260,160,16,1,0);
  fonts[0]=(HFONT)GetStockObject(SYSTEM_FONT); fonts[1]=(HFONT)GetStockObject(ANSI_VAR_FONT);
  fonts[2]=CreateFont(-11,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,0,0,0,0,"MS Sans Serif"); fonts[3]=CreateFont(-19,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,0,0,0,0,"Arial");
  fonts[4]=CreateFont(-16,0,0,0,FW_BOLD,0,0,0,ANSI_CHARSET,0,0,0,0,"Courier New"); fonts[5]=CreateFont(-27,0,0,0,FW_NORMAL,1,0,0,ANSI_CHARSET,0,0,0,0,"Times New Roman"); fonts[6]=CreateFont(-12,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,0,0,0,0,"Arial");
  logline("640x480x%d: pixels (new path against DIBENG, size thresholds off)\n",bpp);
  ctl(5,NULL); /* all copy directions, no size thresholds: every case reaches the new paths */
  for(ci=0;ci<(int)NCASE&&pump();ci++) {int e=0; if(bpp==8&&cases[ci].fn!=c_brushrop&&cases[ci].fn!=c_patrop) continue; total++; if(!run_case(bpp,ci,&e)) fails++; else if(!e&&cases[ci].kind!=3) untested++;}
  /* v2.4: which brush / bitmap / ROP combinations the staged path takes (.595 on the VX800: the brush case drew 1 of its 2 blits at 16 bpp) */
  {static const COLORREF bc[3]={RGB(90,30,200),RGB(101,67,33),RGB(255,0,0)}; static const char *bn[4]={"colour 90,30,200","colour 101,67,33","pure red","8x8 bitmap brush"};
   static const DWORD rp[2]={0x00B8074AUL,0x00E20746UL}; int bi,si,ri; DWORD a0[4],a1[4];
   logline("  staged or not, one blit each (250x180; + = the engine drew it, . = DIBENG): columns compatible bitmap PSDPxax, DSPDxax, DIB section PSDPxax, DSPDxax\n");
   for(bi=0;bi<4;bi++) {char ln[16]; int n=0; HBRUSH b=bi<3?CreateSolidBrush(bc[bi]):CreatePatternBrush(patbm),o=(HBRUSH)SelectObject(sdc,b);
    for(si=0;si<2;si++) for(ri=0;ri<2;ri++) {background(); setmask(31); ctl(0xFFFFFE18UL,a0); blit_from(si?dib32:cbm,30+si*290,30,250,180,0,0,rp[ri]); GdiFlush(); ctl(0xFFFFFE18UL,a1); ln[n++]=a1[1]!=a0[1]?'+':'.'; ln[n++]=' ';}
    ln[n]=0; SelectObject(sdc,o); DeleteObject(b); logline("     %-18s %s\n",bn[bi],ln);}
   /* v2.5/v2.6: the display driver's own account of one blit (capture escape): first its brush and 'MBSK' records (why its memory-blit
    * path left a pattern blit to DIBENG), then every call that put a bitmap on the screen (v2.6: armed after the background is drawn;
    * v2.5's 32 slots were filled by the background's fills) */
   for(bi=0;bi<4;bi+=3) for(si=0;si<2;si++) {static vxh_reply rep; vxh_command hc; int got,k,pass; 
    logline("     %s, %s, PSDPxax:\n",bn[bi],si?"DIB section":"compatible bitmap");
    for(pass=0;pass<2;pass++) {HBRUSH b=bi<3?CreateSolidBrush(bc[bi]):CreatePatternBrush(patbm),o;
     background(); setmask(31); o=(HBRUSH)SelectObject(sdc,b); GdiFlush();
     hc.cb=sizeof(hc); hc.magic=VXH_MAGIC; hc.reserved=0; hc.command=pass?VXH_ARM_BLITS:VXH_ARM; got=ExtEscape(sdc,VXH_ESCAPE,sizeof(hc),(LPCSTR)&hc,0,NULL);
     blit_from(si?dib32:cbm,30,30,250,180,0,0,rp[0]); GdiFlush();
     hc.command=VXH_STOP; ExtEscape(sdc,VXH_ESCAPE,sizeof(hc),(LPCSTR)&hc,0,NULL); memset(&rep,0,sizeof(rep)); hc.command=VXH_READ; ExtEscape(sdc,VXH_ESCAPE,sizeof(hc),(LPCSTR)&hc,sizeof(rep),(LPSTR)&rep);
     SelectObject(sdc,o); DeleteObject(b);
     logline("        %s: capture %s, %lu record(s), %lu dropped\n",pass?"calls that put a bitmap on the screen":"brush and pattern-blit records",got==1?"armed":"NOT accepted",rep.count,rep.dropped);
     for(k=0;k<(int)rep.count&&k<VXH_CAP&&k<12;k++) {vxh_record *q=&rep.records[k];
      if(pass) logline("           %c%c%c%c %ldx%ld at %ld,%ld, ROP %08lX, source type|flags %08lX %lu bpp %ldx%ld, the driver's own attempt %lu (FFFF none, 0 declined, 1 drawn), DIBENG returned %08lX\n",(char)q->rop,(char)(q->rop>>8),(char)(q->rop>>16),(char)(q->rop>>24),(long)q->w,(long)q->h,(long)q->x,(long)q->y,q->raw_fg,q->flags,q->brush_bpp,(long)q->bits[0],(long)q->bits[1],q->hatch,q->text_color);
      else if(q->rop==0x4B53424DUL) logline("           left to DIBENG at exit %lu: brush flags %04lX, depth byte %lu, style %lu; source type %04lX, flags %04lX; ROP %08lX; PATCOPY build answered %lu, kind %lu\n",q->style,q->flags,q->brush_bpp,q->hatch,q->raw_fg,q->raw_bg,q->bits[0],q->bits[1],q->bits[2]);
      else logline("           brush record: ROP %08lX, flags %04lX, depth byte %lu, style %lu, %lux%lu at %lu,%lu\n",q->rop,q->flags,q->brush_bpp,q->style,q->w,q->h,q->x,q->y);}}}
   setmask(0);}
  ctl(0xFFFFFFFFUL,NULL); setmask(0);
  logline("640x480x%d: times per case (shipped thresholds)\n",bpp);
  for(ci=0;ci<(int)NCASE&&pump();ci++) {if(bpp==8&&cases[ci].fn!=c_brushrop&&cases[ci].fn!=c_patrop) continue; time_case(ci);}
  for(ci=2;ci<7;ci++) DeleteObject(fonts[ci]);
  DeleteObject(mono1); DeleteObject(mono2); DeleteObject(cbm); DeleteObject(patbm); DeleteObject(dib32); DeleteObject(dibbu); DeleteObject(dib555); DeleteDC(mdc);
  drop_target(); ReleaseDC(NULL,sdc);
 }
 ctl(0xFFFFFFFFUL,NULL);
 if(have) {ChangeDisplaySettingsA(&origm,0); logline("desktop put back\n");}
 if(aborted) sprintf(res,"FAIL: aborted");
 else sprintf(res,"%s: %d cases, %d with differing pixels, %d where the new path drew nothing (see VX2GDI.TXT)",fails?"FAIL":untested?"PARTIAL":"PASS",total,fails,untested);
 finish(res); logline("%s\nFinished.\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
