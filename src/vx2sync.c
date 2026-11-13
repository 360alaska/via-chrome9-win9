/* VX2SYNC v1.6: checks the 2D engine hook against a software model on the real desktop (32bpp).
 * Every operation is done twice: on the screen (through the driver) and on a memory bitmap (software), then compared.
 * Part 1 = driver as installed (engine does fills and the proven copy directions).
 * Part 2 = engine does every overlapping copy direction too (this is the default in v10.35).
 * Log: VX2SYNC.TXT next to this program. Needs VX85598.VXD v10.25 or newer. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vx85598.h"
#include "vx855_2d.h"

#define AW 320
#define AH 240
#define AX 40
#define AY 40

static HANDLE device=INVALID_HANDLE_VALUE, logfile=INVALID_HANDLE_VALUE;
static HWND hmain, hbtn, hstat, htest;
static char status_text[400];
static char lbuf[60000]; static unsigned lpos;
static unsigned long seed=12345UL;
static int abort_req, busy;
static HDC refdc, rddc, patdc;
static HBITMAP refbmp, rdbmp, patbmp;
static unsigned long *refbits, *rdbits, *patbits;
static int depth=32,saved_valid;
static unsigned long saved_flags;
static HGDIOBJ oldref,oldrd,oldpat;

static void log_flush(void) {DWORD w; if(logfile!=INVALID_HANDLE_VALUE && lpos) {WriteFile(logfile,lbuf,lpos,&w,NULL); lpos=0;}}
static void logf2(const char *fmt,...) {
 char t[900]; va_list ap; unsigned n;
 va_start(ap,fmt); n=_vsnprintf(t,sizeof(t)-1,fmt,ap); va_end(ap); if((int)n<0) n=sizeof(t)-1; t[n]=0;
 if(lpos+n>=sizeof(lbuf)) log_flush();
 memcpy(lbuf+lpos,t,n); lpos+=n;
}
static unsigned long rnd(void) {seed=seed*1103515245UL+12345UL; return (seed>>8)&0xFFFFFFUL;}
static void pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {
  if(m.message==WM_QUIT) {abort_req=1; PostQuitMessage((int)m.wParam); return;}
  TranslateMessage(&m); DispatchMessage(&m);
 }
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) abort_req=1;
}
static int ctrl(unsigned long flags,unsigned long *out) {
 DWORD got=0; unsigned long in=flags;
 return DeviceIoControl(device,VX_IOCTL_2D_CTRL,&in,4,out,16,&got,NULL)&&got==16;
}
static int query(unsigned long *out) {return ctrl(VX2D_QUERY,out);}
static void restore(void) {
 unsigned long out[4];
 if(saved_valid) {if(ctrl(saved_flags,out)&&out[3]==saved_flags) saved_valid=0;else logf2("ERROR: could not restore prior engine flags\r\n");}
}
static unsigned long pixel(unsigned long *p,unsigned i) {
 return depth==16?((unsigned short*)p)[i]:(p[i]&0xFFFFFFUL);
}
static void free_bitmaps(void) {
 if(refdc&&oldref) SelectObject(refdc,oldref);
 if(rddc&&oldrd) SelectObject(rddc,oldrd);
 if(patdc&&oldpat) SelectObject(patdc,oldpat);
 if(refdc) DeleteDC(refdc); if(rddc) DeleteDC(rddc); if(patdc) DeleteDC(patdc);
 if(refbmp) DeleteObject(refbmp); if(rdbmp) DeleteObject(rdbmp); if(patbmp) DeleteObject(patbmp);
 refdc=rddc=patdc=NULL; refbmp=rdbmp=patbmp=NULL; oldref=oldrd=oldpat=NULL;
}

static LRESULT CALLBACK tproc(HWND h,UINT m,WPARAM w,LPARAM l) {
 if(m==WM_ERASEBKGND) return 1;
 return DefWindowProc(h,m,w,l);
}

static int make_bitmaps(void) {
 struct {BITMAPINFOHEADER h; DWORD masks[3];} bi; HDC sdc;
 memset(&bi,0,sizeof(bi)); bi.h.biSize=sizeof(BITMAPINFOHEADER); bi.h.biWidth=AW; bi.h.biHeight=AH;
 bi.h.biPlanes=1; bi.h.biBitCount=depth; bi.h.biCompression=depth==16?BI_BITFIELDS:BI_RGB;
 bi.masks[0]=0xF800; bi.masks[1]=0x7E0; bi.masks[2]=31;
 sdc=GetDC(NULL);
 refdc=CreateCompatibleDC(sdc); rddc=CreateCompatibleDC(sdc); patdc=CreateCompatibleDC(sdc);
 refbmp=CreateDIBSection(sdc,(BITMAPINFO*)&bi,DIB_RGB_COLORS,(void**)&refbits,NULL,0);
 rdbmp=CreateDIBSection(sdc,(BITMAPINFO*)&bi,DIB_RGB_COLORS,(void**)&rdbits,NULL,0);
 patbmp=CreateDIBSection(sdc,(BITMAPINFO*)&bi,DIB_RGB_COLORS,(void**)&patbits,NULL,0);
 ReleaseDC(NULL,sdc);
 if(!refbmp||!rdbmp||!patbmp||!refdc||!rddc||!patdc) {free_bitmaps(); return 0;}
 oldref=SelectObject(refdc,refbmp); oldrd=SelectObject(rddc,rdbmp); oldpat=SelectObject(patdc,patbmp);
 return 1;
}
static void new_pattern(void) {
 unsigned i; GdiFlush();
 for(i=0;i<(unsigned)(AW*AH);i++) {
  unsigned long c=rnd()|(i&1?0x010101UL:0);
  if(depth==16) ((unsigned short*)patbits)[i]=(unsigned short)c;
  else patbits[i]=c;
 }
}
/* compare screen arena with the model; returns mismatches, logs the first */
static unsigned long compare(HDC sdc,const char *what) {
 unsigned long bad=0; unsigned i,first=0xFFFFFFFFUL;
 BitBlt(rddc,0,0,AW,AH,sdc,AX,AY,SRCCOPY);
 GdiFlush();
 for(i=0;i<(unsigned)(AW*AH);i++) if(pixel(rdbits,i)!=pixel(refbits,i)) {bad++; if(first==0xFFFFFFFFUL) first=i;}
 if(bad) logf2("    MISMATCH %s: %lu pixels differ; first at row %u col %u: screen %06lX model %06lX\r\n",what,bad,first/AW,first%AW,pixel(rdbits,first),pixel(refbits,first));
 return bad;
}
/* reset screen arena and model to the same random picture, using the CPU path */
static void fresh(HDC sdc) {
 new_pattern();
 BitBlt(refdc,0,0,AW,AH,patdc,0,0,SRCCOPY);
 BitBlt(sdc,AX,AY,AW,AH,patdc,0,0,SRCCOPY);
}
static void op_fillrop(HDC sdc,int x,int y,int w,int h,COLORREF c,DWORD rop) {
 HBRUSH b=CreateSolidBrush(c); HGDIOBJ o;
 o=SelectObject(sdc,b); PatBlt(sdc,AX+x,AY+y,w,h,rop); SelectObject(sdc,o);
 o=SelectObject(refdc,b); PatBlt(refdc,x,y,w,h,rop); SelectObject(refdc,o);
 DeleteObject(b);
}
static void op_fill(HDC sdc,int x,int y,int w,int h,COLORREF c) {op_fillrop(sdc,x,y,w,h,c,PATCOPY);}
static void op_copy(HDC sdc,int sx,int sy,int dx,int dy,int w,int h) {
 BitBlt(sdc,AX+dx,AY+dy,w,h,sdc,AX+sx,AY+sy,SRCCOPY);
 BitBlt(refdc,dx,dy,w,h,refdc,sx,sy,SRCCOPY);
}
static void op_cpu(HDC sdc,int x,int y,int w,int h) {
 int sx=(int)(rnd()%(AW-w)),sy=(int)(rnd()%(AH-h));
 BitBlt(sdc,AX+x,AY+y,w,h,patdc,sx,sy,SRCCOPY);
 BitBlt(refdc,x,y,w,h,patdc,sx,sy,SRCCOPY);
}

static unsigned long suite(HDC sdc,int part,int *fails) {
 static const int offs[]={-33,-1,0,1,33};
 int a,b,k,total=0; unsigned long e0,e1,o[4];
 *fails=0;
 ctrl((part==2?1:0)|4,o);
 /* A: solid fills */
 query(o); e0=o[0];
 fresh(sdc);
 for(k=0;k<40 && !abort_req;k++) {
  int w=1+(int)(rnd()%150),h=1+(int)(rnd()%100),x=(int)(rnd()%(AW-w+1)),y=(int)(rnd()%(AH-h+1));
  op_fill(sdc,x,y,w,h,(COLORREF)rnd());
 }
 total+=(int)compare(sdc,"40 random fills")?1:0;  
 query(o); logf2("  fills: engine ops %lu\r\n",o[0]-e0);
 if(total) {(*fails)++; total=0; logf2("  FILL: FAIL\r\n");} else logf2("  FILL: pass\r\n");
 /* A2: the other fill operations */
 {
  static const DWORD ropv[4]={BLACKNESS,WHITENESS,DSTINVERT,PATINVERT};
  static const char *ropn[4]={"BLACKNESS","WHITENESS","DSTINVERT","PATINVERT"};
  int ri;
  for(ri=0;ri<4 && !abort_req;ri++) {
   unsigned long bad;
   fresh(sdc);
   query(o); e0=o[0];
   for(k=0;k<20 && !abort_req;k++) {
    int w=1+(int)(rnd()%150),h=1+(int)(rnd()%100),x=(int)(rnd()%(AW-w+1)),y=(int)(rnd()%(AH-h+1));
    op_fillrop(sdc,x,y,w,h,(COLORREF)rnd(),ropv[ri]);
   }
   query(o); e1=o[0];
   bad=compare(sdc,ropn[ri]);
   logf2("  %s: %s (engine ops %lu of 20)\r\n",ropn[ri],bad?"FAIL":"pass",e1-e0);
   if(bad) (*fails)++;
  }
 }
 /* B: copies, every direction and offset */
 for(a=0;a<5 && !abort_req;a++) for(b=0;b<5 && !abort_req;b++) {
  int dx=offs[a],dy=offs[b],w,h,sx,sy; unsigned long bad;
  if(!dx && !dy) continue;
  w=120; h=80; sx=100; sy=70;
  fresh(sdc);
  query(o); e0=o[0];
  op_copy(sdc,sx,sy,sx+dx,sy+dy,w,h);
  query(o); e1=o[0];
  bad=compare(sdc,"copy");
  logf2("  copy offset (%+d,%+d): %s (engine ops %lu)\r\n",dx,dy,bad?"FAIL":"pass",e1-e0);
  if(bad) (*fails)++;
  pump();
 }
 /* C: long random mixed sequence, CPU writes interleaved with engine ops */
 fresh(sdc);
 {
  int i,bad=0,failed_at=-1;
  query(o); e0=o[0];
  for(i=0;i<400 && !abort_req;i++) {
   int t=(int)(rnd()%3),w=1+(int)(rnd()%120),h=1+(int)(rnd()%90),x=(int)(rnd()%(AW-w+1)),y=(int)(rnd()%(AH-h+1));
   if(t==0) op_fill(sdc,x,y,w,h,(COLORREF)rnd());
   else if(t==1) op_cpu(sdc,x,y,w,h);
   else {
    int sx=(int)(rnd()%(AW-w+1)),sy=(int)(rnd()%(AH-h+1));
    if(rnd()&1) {sx=x+(int)(rnd()%9)-4; sy=y+(int)(rnd()%9)-4; if(sx<0)sx=0; if(sy<0)sy=0; if(sx>AW-w)sx=AW-w; if(sy>AH-h)sy=AH-h;}
    op_copy(sdc,sx,sy,x,y,w,h);
   }
   if(i%8==7) {unsigned long m=compare(sdc,"mixed"); if(m) {bad=1; failed_at=i; break;}}
   if(i%20==0) pump();
  }
  query(o);
  logf2("  mixed sequence: %s%s (engine ops %lu)\r\n",bad?"FAIL at op ":"pass",bad?"":"",o[0]-e0);
  if(bad) {logf2("    (failed within ops %d..%d)\r\n",failed_at-7,failed_at); (*fails)++;}
 }
 /* D: asymmetric mono/colour brushes, nonzero brush origin, and clips. */
 {
  int kind,j; HDC bdc=CreateCompatibleDC(sdc); HBITMAP bm=CreateCompatibleBitmap(sdc,8,8);
  HGDIOBJ old=SelectObject(bdc,bm);
  for(j=0;j<64;j++) SetPixel(bdc,j&7,j>>3,RGB((j*47)&255,(j*89)&255,(j*17)&255));
  SelectObject(bdc,old); DeleteDC(bdc);
  for(kind=0;kind<3&&!abort_req;kind++) {
   WORD mono[8]={0x80,0x42,0x24,0x18,0xA5,0x81,0x7E,0x99};
   HBITMAP mb=CreateBitmap(8,8,1,1,mono);
   HBRUSH brush=kind==0?CreatePatternBrush(mb):(kind==1?CreatePatternBrush(bm):CreateHatchBrush(HS_DIAGCROSS,RGB(17,221,63)));
   HGDIOBJ os,orr; POINT bs,br;
   fresh(sdc); query(o); e0=o[0];
   os=SelectObject(sdc,brush); orr=SelectObject(refdc,brush);
   SetBrushOrgEx(sdc,AX+3,AY+5,&bs); SetBrushOrgEx(refdc,3,5,&br);
   SetBkMode(sdc,OPAQUE); SetBkMode(refdc,OPAQUE);
   SetTextColor(sdc,RGB(199,23,77)); SetTextColor(refdc,RGB(199,23,77));
   SetBkColor(sdc,RGB(29,67,211)); SetBkColor(refdc,RGB(29,67,211));
   for(j=0;j<8;j++) {
    int x=13+j*19,y=11+j*13;
    PatBlt(sdc,AX+x,AY+y,93,47,j&1?PATINVERT:PATCOPY);
    PatBlt(refdc,x,y,93,47,j&1?PATINVERT:PATCOPY);
   }
   SelectObject(sdc,os); SelectObject(refdc,orr);
   SetBrushOrgEx(sdc,bs.x,bs.y,NULL); SetBrushOrgEx(refdc,br.x,br.y,NULL);
   if(compare(sdc,"brush origin/pattern")) (*fails)++;
   if(kind==0) { /* maps from the compare buffers (bottom-up): T = text colour, B = background colour, t/b = the same colours with red and blue swapped, . = other; screen (engine) beside model (software) */
    int pass2,yy,xx,ox[2],oy[2],bad=-1; char a[40],b2[40];
    ox[0]=13; oy[0]=11; ox[1]=0; oy[1]=0;
    for(yy=0;yy<AH&&bad<0;yy++) for(xx=0;xx<AW;xx++) if(pixel(rdbits,(AH-1-yy)*AW+xx)!=pixel(refbits,(AH-1-yy)*AW+xx)) {bad=1; ox[1]=xx-9; oy[1]=yy-6; break;}
    if(ox[1]<0) ox[1]=0; if(oy[1]<0) oy[1]=0; if(ox[1]>AW-19) ox[1]=AW-19; if(oy[1]>AH-13) oy[1]=AH-13;
    for(pass2=0;pass2<2;pass2++) {
     if(pass2==1&&bad<0) break;
     logf2("    pattern map %s (19x13 at %d,%d; brush origin set to %d,%d): screen | model\r\n",pass2?"around the first mismatch (top-down scan)":"first PatBlt corner",ox[pass2],oy[pass2],AX+3,AY+5);
     for(yy=0;yy<13;yy++) {
      for(xx=0;xx<19;xx++) {
       unsigned long cs=pixel(rdbits,(AH-1-(oy[pass2]+yy))*AW+ox[pass2]+xx),cm=pixel(refbits,(AH-1-(oy[pass2]+yy))*AW+ox[pass2]+xx);
       a[xx]=cs==0xC7174DUL?'T':(cs==0x1D43D3UL?'B':(cs==0x4D17C7UL?'t':(cs==0xD3431DUL?'b':'.'))); b2[xx]=cm==0xC7174DUL?'T':(cm==0x1D43D3UL?'B':(cm==0x4D17C7UL?'t':(cm==0xD3431DUL?'b':'.')));
      }
      a[19]=0; b2[19]=0; logf2("    %s | %s\r\n",a,b2);
     }
     logf2("    raw corner pixels screen %06lX %06lX %06lX | model %06lX %06lX %06lX\r\n",pixel(rdbits,(AH-1-oy[pass2])*AW+ox[pass2]),pixel(rdbits,(AH-1-oy[pass2])*AW+ox[pass2]+1),pixel(rdbits,(AH-1-oy[pass2])*AW+ox[pass2]+2),pixel(refbits,(AH-1-oy[pass2])*AW+ox[pass2]),pixel(refbits,(AH-1-oy[pass2])*AW+ox[pass2]+1),pixel(refbits,(AH-1-oy[pass2])*AW+ox[pass2]+2));
    }
   }
   query(o); logf2("  brush kind %d: engine ops %lu (zero means DIB fallback: kind 0 is a 1-bit bitmap brush and should use the engine; kinds 1 and 2 stay in software)\r\n",kind,o[0]-e0);
   DeleteObject(brush); DeleteObject(mb);
  }
  DeleteObject(bm);
 }
 {
  HRGN a=CreateRectRgn(AX+37,AY+29,AX+193,AY+137),b=CreateRectRgn(37,29,193,137);
  fresh(sdc); query(o); e0=o[0];
  SelectClipRgn(sdc,a); SelectClipRgn(refdc,b);
  op_fill(sdc,4,6,257,181,RGB(253,35,17));
  StretchBlt(sdc,AX+9,AY+8,211,151,sdc,AX+39,AY+41,211,151,SRCCOPY);
  StretchBlt(refdc,9,8,211,151,refdc,39,41,211,151,SRCCOPY);
  SelectClipRgn(sdc,NULL); SelectClipRgn(refdc,NULL);
  if(compare(sdc,"clipped fill/copy")) (*fails)++;
  query(o); logf2("  clipped GDI: engine ops %lu\r\n",o[0]-e0);
  DeleteObject(a); DeleteObject(b);
 }
 return 0;
}

static void run_all(void) {
 WNDCLASS wc; int f1=0,f2=0; unsigned long o[4]; HDC sdc; DWORD got;
 vx_status_t s; vx2d_stats before,after; RECT r;
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=tproc; wc.hInstance=GetModuleHandle(NULL); wc.lpszClassName="VX2SYNCT"; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
 RegisterClass(&wc);
 busy=1; abort_req=0; seed=12345UL;
 logf2("\r\n==== VX2SYNC v1.6 ====\r\n");
 if(!DeviceIoControl(device,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)) {sprintf(status_text,"status failed"); busy=0; return;}
 logf2("desktop %lux%lux%lu, accelerated ops so far (draws) %lu\r\n",s.width,s.height,s.bpp,s.draws);
 if((s.bpp!=16&&s.bpp!=32) || s.width<AX+AW || s.height<AY+AH) {strcpy(status_text,"Needs a 16/32bpp desktop at 640x480 or larger."); logf2("not a 16/32bpp desktop; stop\r\n"); log_flush(); busy=0; return;}
 if(!DeviceIoControl(device,VX_IOCTL_2D_STATS,NULL,0,&before,sizeof(before),&got,NULL)||got!=sizeof(before)||before.abi!=VX2D_ABI) {strcpy(status_text,"Needs the v10.58 driver (2D ABI check failed)."); busy=0;return;}
 if(!query(o)) {strcpy(status_text,"Driver has no 2D control (need VXD v10.25+)."); logf2("no 2D control ioctl\r\n"); log_flush(); busy=0; return;}
 logf2("engine at start: state %lu flags %08lX ops %lu fallbacks %lu resets %lu timeouts %lu last result %08lX (high bytes = why it was skipped: 02xx desktop not eligible [low byte bits 1 gdi flags,2/4 gdi off,8 no vram,16 mode,32 a DirectDraw surface exists,64 fb address,128 mode mismatch], 04 feature switch off, 05 too small, 06 busy/disabled, 07 overlap, 08/09 engine start)\r\n",before.state,before.flags,before.ops,before.fallbacks,before.resets,before.timeouts,before.last_result);
 depth=(int)s.bpp; saved_flags=o[3]; saved_valid=1;
 if(!make_bitmaps()) {strcpy(status_text,"Cannot create bitmaps."); restore(); busy=0; return;}
 GetClientRect(GetDesktopWindow(),&r);
 htest=CreateWindowEx(WS_EX_TOPMOST,"VX2SYNCT","VX2SYNC test",WS_POPUP|WS_VISIBLE,0,0,r.right,r.bottom,NULL,NULL,wc.hInstance,NULL);
 ShowWindow(htest,SW_SHOW); UpdateWindow(htest); pump();
 sdc=GetDC(htest);
 {HBRUSH b=CreateSolidBrush(RGB(0,0,0)); RECT rr; GetClientRect(htest,&rr); FillRect(sdc,&rr,b); DeleteObject(b);}
 logf2("\r\nPART 1: conservative copy policy (fills + both-decrement overlaps)\r\n");
 suite(sdc,1,&f1);
 logf2("PART 1 result: %d failing tests\r\n",f1);
 if(!abort_req) {
  seed=12345UL;
  logf2("\r\nPART 2: engine does every overlap direction\r\n");
  suite(sdc,2,&f2);
  logf2("PART 2 result: %d failing tests\r\n",f2);
 }
 if(DeviceIoControl(device,VX_IOCTL_2D_STATS,NULL,0,&after,sizeof(after),&got,NULL)&&got==sizeof(after)) {
  logf2("engine at end: state %lu ops %lu fallbacks %lu resets %lu last result %08lX\r\n",after.state,after.ops,after.fallbacks,after.resets,after.last_result);
  logf2("Native completed: fills %lu, copies %lu, mono %lu, colour %lu, clipped %lu; timeouts %lu\r\n",
   after.fills-before.fills,after.copies-before.copies,after.mono-before.mono,after.color-before.color,after.clips-before.clips,after.timeouts-before.timeouts);
  if(after.fills==before.fills||after.copies==before.copies) logf2("NATIVE PATH NOT FULLY EXERCISED: zero pixel mismatches can also mean software fallback.\r\n");
 }
 restore();
 ReleaseDC(htest,sdc); DestroyWindow(htest); htest=NULL;
 free_bitmaps();
 logf2("\r\nDONE%s. Part 1 failures %d, part 2 failures %d. Exact prior live engine flags restored.\r\n",abort_req?" (aborted)":"",f1,f2);
 log_flush();
 sprintf(status_text,"Done. Part 1 failures %d, part 2 failures %d. Send me VX2SYNC.TXT.",f1,f2);
 busy=0;
}

static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l) {
 switch(m) {
 case WM_COMMAND:
  if(HIWORD(w)==BN_CLICKED && LOWORD(w)==100 && !busy) {
   EnableWindow(hbtn,FALSE); SetWindowText(hstat,"Running... do not touch the mouse. Esc stops."); UpdateWindow(h);
   run_all();
   SetWindowText(hstat,status_text); EnableWindow(hbtn,TRUE);
  }
  return 0;
 case WM_CLOSE: if(busy) {abort_req=1; return 0;} DestroyWindow(h); return 0;
 case WM_DESTROY: PostQuitMessage(0); return 0;
 }
 return DefWindowProc(h,m,w,l);
}

int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 WNDCLASS wc; MSG m; DWORD got; vx_status_t s;
 logfile=CreateFile("VX2SYNC.TXT",GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,0,NULL);
 if(logfile!=INVALID_HANDLE_VALUE) SetFilePointer(logfile,0,NULL,FILE_END);
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=proc; wc.hInstance=hi; wc.lpszClassName="VX2SYNC"; wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1); wc.hCursor=LoadCursor(NULL,IDC_ARROW);
 RegisterClass(&wc);
 hmain=CreateWindow("VX2SYNC","VX855 2D sync test (VX2SYNC v1.6)",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,20,20,520,200,NULL,NULL,hi,NULL);
 hbtn=CreateWindow("BUTTON","RUN TEST",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,20,20,140,32,hmain,(HMENU)100,hi,NULL);
 hstat=CreateWindow("STATIC","",WS_CHILD|WS_VISIBLE,20,70,470,80,hmain,NULL,hi,NULL);
 device=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(device==INVALID_HANDLE_VALUE) {strcpy(status_text,"Cannot open VX85598 driver."); EnableWindow(hbtn,FALSE);}
 else {
  memset(&s,0,sizeof(s));
  if(!DeviceIoControl(device,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||s.magic!=VX_MAGIC||s.cb!=sizeof(s)) {strcpy(status_text,"Incompatible driver status."); EnableWindow(hbtn,FALSE);}
  else sprintf(status_text,"Ready (desktop %lux%lux%lu). Click RUN TEST. It draws a black full-screen window for about a minute and compares engine drawing with software drawing.",s.width,s.height,s.bpp);
 }
 SetWindowText(hstat,status_text);
 ShowWindow(hmain,SW_SHOW); UpdateWindow(hmain);
 while(GetMessage(&m,NULL,0,0)>0) {TranslateMessage(&m); DispatchMessage(&m);}
 restore();
 log_flush();
 if(device!=INVALID_HANDLE_VALUE) CloseHandle(device);
 if(logfile!=INVALID_HANDLE_VALUE) CloseHandle(logfile);
 ExitProcess(0);
 return 0;
}
