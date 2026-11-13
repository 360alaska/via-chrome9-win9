/* VX2PROOF v1.0: closes the three proof gaps left after v10.65. Needs VX85598.VXD v10.65 (or any build with the 2D, CRT and flip ioctls).
 *   A  16bpp GDI engine path: switches to 800x600x16, runs the VX2SYNC comparison (engine drawing against a software model: fills, ROP fills,
 *      all copy directions, a mixed CPU/engine sequence, brushes, clipping) and checks that the engine really did the work (counters).
 *      If 16bpp fails, the same suite runs at 32bpp as a control, so a test-program fault shows up as a failure there too.
 *   B  DirectDraw soak: 40 back-to-back SetDisplayMode calls (640x480, 800x600, 1024x768 at 16 and 32 bits), each with a flip chain, a primary
 *      surface fill that is read back, and a check that the display start is 0 again afterwards. No restore between calls (the pattern that
 *      used to crash DDHELP). Command line "n=80" changes the count.
 *   C  Picture position at the high modes: 1152x864, 1280x720, 1360x768, 1366x768, 1440x900, 1600x900, 1600x1200, 1680x1050, 1920x1080 and 1920x1200
 *      at 32bpp (and 1280x1024 as the known-good reference). Each card shows a ruler, corner markers and a circle; you click OK or BAD.
 *      Sizes the monitor does not offer are listed as "not offered". The CRTC registers of each mode are logged.
 * Buttons on the start window run everything or one part. Esc stops a part. Log: VX2PROOF.TXT in the current folder (written line by line). */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <io.h>
#include "vx85598.h"
#include "vx855_2d.h"
#include "vx855_flip.h"
#include "vx_brush_diag.h"

#define AW 320
#define AH 240
#define AX 40
#define AY 40

static HANDLE dev=INVALID_HANDLE_VALUE; static FILE *lf;
static HWND hmain,hstat,hbtn[4]; static int busy,abort_req,opt_n=40;
static char stepname[80]; static char sum[3][260];
static void logl(const char *fmt,...) {
 if(!lf) return; {va_list a; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a);} fflush(lf); _commit(_fileno(lf));
}
static void step(const char *n) {strncpy(stepname,n,79); stepname[79]=0;}
static LONG WINAPI crashfilter(EXCEPTION_POINTERS *e) {
 MEMORY_BASIC_INFORMATION mbi; char mod[260]; DWORD a=(DWORD)e->ExceptionRecord->ExceptionAddress; mod[0]=0; memset(&mbi,0,sizeof(mbi));
 if(VirtualQuery((void*)a,&mbi,sizeof(mbi))&&mbi.AllocationBase) GetModuleFileNameA((HMODULE)mbi.AllocationBase,mod,sizeof(mod));
 logl("\nCRASH during \"%s\": exception %08lX at %08lX in %s (offset %08lX)\n",stepname,(DWORD)e->ExceptionRecord->ExceptionCode,a,mod[0]?mod:"(unknown)",a-(DWORD)mbi.AllocationBase);
 if(lf) {fclose(lf); lf=NULL;}
 return EXCEPTION_EXECUTE_HANDLER;
}
static void pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) abort_req=1;
}
static void wait_ms(DWORD ms) {DWORD e=GetTickCount()+ms; while(GetTickCount()<e) {pump(); Sleep(15);}}
static LRESULT CALLBACK twnd(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_ERASEBKGND) return 1;
 if(m==WM_SETCURSOR) {SetCursor(LoadCursor(NULL,IDC_ARROW)); return TRUE;}
 return DefWindowProc(w,m,a,b);
}
static HWND topwin(HINSTANCE hi,const char *cls) {
 HWND w=CreateWindowEx(WS_EX_TOPMOST,cls,"VX2PROOF",WS_POPUP|WS_VISIBLE,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,hi,NULL);
 ShowWindow(w,SW_SHOW); UpdateWindow(w); pump(); return w;
}
static int pr_setmode(DWORD w,DWORD h,DWORD b) {
 DEVMODE dm; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 return ChangeDisplaySettings(&dm,CDS_FULLSCREEN)==DISP_CHANGE_SUCCESSFUL;
}
static int offered(DWORD w,DWORD h,DWORD b) {
 DEVMODE dm; int i; for(i=0;;i++) {memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); if(!EnumDisplaySettings(NULL,i,&dm)) break; if(dm.dmPelsWidth==w&&dm.dmPelsHeight==h&&dm.dmBitsPerPel==b) return 1;}
 return 0;
}
static DWORD rgb(DWORD bpp,int r,int g,int b) {return bpp==16?(DWORD)(((r>>3)<<11)|((g>>2)<<5)|(b>>3)):(DWORD)((r<<16)|(g<<8)|b);}
static int status(vx_status_t *s) {DWORD got=0; memset(s,0,sizeof(*s)); return dev!=INVALID_HANDLE_VALUE&&DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,s,sizeof(*s),&got,NULL);}
/* display start and the driver's own view of it; returns 0 when the driver cannot be read */
static int start_info(DWORD *start,DWORD *disp,vx_crt_report *rep) {
 DWORD got=0,r[VXF_REPLY_DWORDS]; vxf_request q; static vx_crt_report local;
 if(!rep) rep=&local;
 *start=0xFFFFFFFFUL; *disp=0xFFFFFFFFUL;
 if(dev==INVALID_HANDLE_VALUE||!DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,rep,sizeof(*rep),&got,NULL)) return 0;
 *start=((DWORD)(rep->cr[0x48]&0x1F)<<24)|((DWORD)rep->cr[0x34]<<16)|((DWORD)rep->cr[0x0C]<<8)|rep->cr[0x0D];
 q.cb=sizeof(q); q.cmd=VXF_INFO; q.arg=0; q.flags=0; memset(r,0,sizeof(r));
 if(DeviceIoControl(dev,VX_IOCTL_FLIP,&q,sizeof(q),r,4*VXF_REPLY_DWORDS,&got,NULL)) *disp=r[VXF_R_DISPLAY];
 return 1;
}
static void memlog(int op) {MEMORYSTATUS m; m.dwLength=sizeof(m); GlobalMemoryStatus(&m); logl("  mem op %d: physical free %lu KB, page file free %lu KB\n",op,m.dwAvailPhys/1024,m.dwAvailPageFile/1024);}

/* ================= stage A: 16bpp GDI engine path (the VX2SYNC suite) ================= */
static HDC refdc,rddc,patdc; static HBITMAP refbmp,rdbmp,patbmp; static unsigned long *refbits,*rdbits,*patbits;
static unsigned long n_solid,n_patinv,n_brush0,n_clip; /* engine ops seen per category (all parts) */
static HGDIOBJ oldref,oldrd,oldpat; static int depth=32; static unsigned long seed=12345UL;
static unsigned long rnd(void) {seed=seed*1103515245UL+12345UL; return (seed>>8)&0xFFFFFFUL;}
static int ctrl(unsigned long flags,unsigned long *out) {
 DWORD got=0; unsigned long in=flags;
 return DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&in,4,out,16,&got,NULL)&&got==16;
}
static int query(unsigned long *out) {return ctrl(VX2D_QUERY,out);}
static unsigned long pixel(unsigned long *p,unsigned i) {return depth==16?((unsigned short*)p)[i]:(p[i]&0xFFFFFFUL);}
/* Diagnostic build: only the EXE writes files. The DRV uses bounded memory.
 * Positive DIB height means row i/AW is a storage row, not logical screen Y. */
static unsigned long diag_before[AW*AH];
static int diag_before_valid,diag_dumped,diag_op=-1;
static unsigned long diag_color;
static vxb_reply diag_trace;
static int brush_trace(HDC dc,unsigned long command,void *out) {
 vxb_command c; c.cb=sizeof(c); c.magic=VXB_MAGIC; c.command=command; c.reserved=0;
 return ExtEscape(dc,VXB_ESCAPE,sizeof(c),(LPCSTR)&c,out?(int)sizeof(vxb_reply):0,(LPSTR)out);
}
static void brush_trace_log(HDC dc,COLORREF color,DWORD rop) {
 unsigned long k;
 int got;
 brush_trace(dc,VXB_STOP,NULL);
 memset(&diag_trace,0,sizeof(diag_trace));
 got=brush_trace(dc,VXB_READ,&diag_trace);
 if(got!=(int)sizeof(diag_trace)||
    diag_trace.cb!=sizeof(diag_trace)||diag_trace.magic!=VXB_MAGIC||
    diag_trace.abi!=1||diag_trace.count>VXB_CAP) {
  static int warned; if(!warned) {warned=1; logl("    (brush trace read failed: ExtEscape returned %d, expected %d; reply cb=%lu magic=%08lX abi=%lu count=%lu)\n",got,(int)sizeof(diag_trace),diag_trace.cb,diag_trace.magic,diag_trace.abi,diag_trace.count);}
  return;
 }
 logl("    BRUSH COLORREF=%08lX ROP=%08lX records=%lu dropped=%lu\n",(DWORD)color,rop,diag_trace.count,diag_trace.dropped);
 for(k=0;k<diag_trace.count;k++) {
  vxb_record *r=&diag_trace.records[k];
  logl("      rect %lu,%lu %lux%lu screen-bpp=%lu brush-bpp=%lu flags=%02lX style=%lu hatch=%lu rawFG=%08lX rawBG=%08lX deFlags=%04lX reason=%lu op=%lu qFG=%08lX qBG=%08lX result=%lu bits0=%08lX bits1=%08lX colorinfo=%08lX\n",
   r->x,r->y,r->w,r->h,r->bpp,r->brush_bpp,r->brush_flags,r->style,r->hatch,
   r->raw_fg,r->raw_bg,r->device_flags,r->reason,r->op,r->fg,r->bg,r->result,r->bits0,r->bits1,r->phys);
 }
}
static void free_bitmaps(void) {
 if(refdc&&oldref) SelectObject(refdc,oldref); if(rddc&&oldrd) SelectObject(rddc,oldrd); if(patdc&&oldpat) SelectObject(patdc,oldpat);
 if(refdc) DeleteDC(refdc); if(rddc) DeleteDC(rddc); if(patdc) DeleteDC(patdc);
 if(refbmp) DeleteObject(refbmp); if(rdbmp) DeleteObject(rdbmp); if(patbmp) DeleteObject(patbmp);
 refdc=rddc=patdc=NULL; refbmp=rdbmp=patbmp=NULL; oldref=oldrd=oldpat=NULL;
}
static int make_bitmaps(void) {
 struct {BITMAPINFOHEADER h; DWORD masks[3];} bi; HDC sdc;
 memset(&bi,0,sizeof(bi)); bi.h.biSize=sizeof(BITMAPINFOHEADER); bi.h.biWidth=AW; bi.h.biHeight=AH;
 bi.h.biPlanes=1; bi.h.biBitCount=(WORD)depth; bi.h.biCompression=depth==16?BI_BITFIELDS:BI_RGB;
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
  if(depth==16) ((unsigned short*)patbits)[i]=(unsigned short)c; else patbits[i]=c;
 }
}
static unsigned long compare(HDC sdc,const char *what) {
 unsigned long bad=0; unsigned i,first=0xFFFFFFFFUL;
 BitBlt(rddc,0,0,AW,AH,sdc,AX,AY,SRCCOPY); GdiFlush();
 for(i=0;i<(unsigned)(AW*AH);i++) if(pixel(rdbits,i)!=pixel(refbits,i)) {bad++; if(first==0xFFFFFFFFUL) first=i;}
 if(bad) logl("    MISMATCH %s: %lu pixels differ; first at row %u col %u: screen %06lX model %06lX\n",what,bad,first/AW,first%AW,pixel(rdbits,first),pixel(refbits,first));
 if(bad) logl("      logical first x=%u y=%u; screen x=%u y=%u\n",first%AW,AH-1-first/AW,AX+first%AW,AY+AH-1-first/AW);
 if(bad&&diag_before_valid&&!diag_dumped) {
  FILE *f=fopen("VX2BAD.CSV","w");
  if(f) {
   int xmin=AW,ymin=AH,xmax=-1,ymax=-1;
   fprintf(f,"op,colorref,storage_row,local_x,local_y,screen_x,screen_y,before,actual,expected,actual_xor_operand,expected_xor_operand,error_xor\n");
   for(i=0;i<(unsigned)(AW*AH);i++) if(pixel(rdbits,i)!=pixel(refbits,i)) {
    int x=i%AW,y=AH-1-i/AW;
    unsigned long b=pixel(diag_before,i),a=pixel(rdbits,i),e=pixel(refbits,i);
    if(x<xmin)xmin=x; if(x>xmax)xmax=x; if(y<ymin)ymin=y; if(y>ymax)ymax=y;
    fprintf(f,"%d,%08lX,%u,%d,%d,%d,%d,%08lX,%08lX,%08lX,%08lX,%08lX,%08lX\n",diag_op,diag_color,i/AW,x,y,AX+x,AY+y,b,a,e,a^b,e^b,a^e);
   }
   fclose(f); diag_dumped=1;
   logl("      VX2BAD.CSV written; mismatch bounds local x=%d..%d y=%d..%d\n",xmin,xmax,ymin,ymax);
  } else logl("      Could not create VX2BAD.CSV\n");
 }
 return bad;
}
static void fresh(HDC sdc) {new_pattern(); BitBlt(refdc,0,0,AW,AH,patdc,0,0,SRCCOPY); BitBlt(sdc,AX,AY,AW,AH,patdc,0,0,SRCCOPY);}
static void op_fillrop(HDC sdc,int x,int y,int w,int h,COLORREF c,DWORD rop) {
 HBRUSH b=CreateSolidBrush(c); HGDIOBJ o; int traced;
 o=SelectObject(sdc,b); traced=brush_trace(sdc,VXB_ARM,NULL)==1;
 PatBlt(sdc,AX+x,AY+y,w,h,rop); GdiFlush();
 if(traced) brush_trace_log(sdc,c,rop);
 else {static int warned; if(!warned) {warned=1; logl("    (brush trace escape 0x8F71 not accepted by the driver: this DRV does not have the diagnostic patch)\n");}}
 SelectObject(sdc,o);
 o=SelectObject(refdc,b); PatBlt(refdc,x,y,w,h,rop); SelectObject(refdc,o);
 DeleteObject(b);
}
static void op_fill(HDC sdc,int x,int y,int w,int h,COLORREF c) {op_fillrop(sdc,x,y,w,h,c,PATCOPY);}
static void op_copy(HDC sdc,int sx,int sy,int dx,int dy,int w,int h) {
 BitBlt(sdc,AX+dx,AY+dy,w,h,sdc,AX+sx,AY+sy,SRCCOPY); BitBlt(refdc,dx,dy,w,h,refdc,sx,sy,SRCCOPY);
}
static void op_cpu(HDC sdc,int x,int y,int w,int h) {
 int sx=(int)(rnd()%(AW-w)),sy=(int)(rnd()%(AH-h));
 BitBlt(sdc,AX+x,AY+y,w,h,patdc,sx,sy,SRCCOPY); BitBlt(refdc,x,y,w,h,patdc,sx,sy,SRCCOPY);
}
static int suite(HDC sdc,int part) {
 static const int offs[]={-33,-1,0,1,33};
 int a,b,k,fails=0; unsigned long e0,e1,o[4];
 ctrl((part==2?1:0)|4,o);
 fresh(sdc); query(o); e0=o[0];
 for(k=0;k<40&&!abort_req;k++) {int w=1+(int)(rnd()%150),h=1+(int)(rnd()%100),x=(int)(rnd()%(AW-w+1)),y=(int)(rnd()%(AH-h+1)); op_fill(sdc,x,y,w,h,(COLORREF)rnd());}
 {unsigned long bad=compare(sdc,"40 random fills"); query(o); logl("  fills: %s (engine ops %lu)\n",bad?"FAIL":"pass",o[0]-e0); n_solid+=o[0]-e0; if(bad) fails++;}
 {
  static const DWORD ropv[4]={BLACKNESS,WHITENESS,DSTINVERT,PATINVERT}; static const char *ropn[4]={"BLACKNESS","WHITENESS","DSTINVERT","PATINVERT"}; int ri;
  for(ri=0;ri<4&&!abort_req;ri++) {
   unsigned long bad; fresh(sdc); query(o); e0=o[0];
   for(k=0;k<20&&!abort_req;k++) {
    int w=1+(int)(rnd()%150),h=1+(int)(rnd()%100),x=(int)(rnd()%(AW-w+1)),y=(int)(rnd()%(AH-h+1));
    COLORREF c=(COLORREF)rnd(); unsigned long eb=0,ea=0;
    if(ri==3) {GdiFlush(); query(o); eb=o[0]; memcpy(diag_before,refbits,(size_t)(AW*AH)*(depth/8)); diag_before_valid=1; diag_op=k; diag_color=(unsigned long)c;}
    op_fillrop(sdc,x,y,w,h,c,ropv[ri]);
    if(ri==3) {
     char label[80]; unsigned long n; GdiFlush(); query(o); ea=o[0];
     sprintf(label,"PATINVERT op %d rect %d,%d %dx%d color %08lX",k,x,y,w,h,(DWORD)c);
     n=compare(sdc,label); logl("    %s: %lu mismatches after this op, engine ops +%lu (%s)\n",label,n,ea-eb,(ea-eb)?"NATIVE":"software");
     diag_before_valid=0;
    }
   }
   query(o); e1=o[0]; if(ri==3) n_patinv+=e1-e0; bad=compare(sdc,ropn[ri]); logl("  %s: %s (engine ops %lu of 20)\n",ropn[ri],bad?"FAIL":"pass",e1-e0); if(bad) fails++;
  }
 }
 for(a=0;a<5&&!abort_req;a++) for(b=0;b<5&&!abort_req;b++) {
  int dx=offs[a],dy=offs[b]; unsigned long bad;
  if(!dx&&!dy) continue;
  fresh(sdc); query(o); e0=o[0]; op_copy(sdc,100,70,100+dx,70+dy,120,80); query(o); e1=o[0];
  bad=compare(sdc,"copy"); logl("  copy offset (%+d,%+d): %s (engine ops %lu)\n",dx,dy,bad?"FAIL":"pass",e1-e0); if(bad) fails++;
  pump();
 }
 fresh(sdc);
 {
  int i,bad=0,failed_at=-1; query(o); e0=o[0];
  for(i=0;i<400&&!abort_req;i++) {
   int t=(int)(rnd()%3),w=1+(int)(rnd()%120),h=1+(int)(rnd()%90),x=(int)(rnd()%(AW-w+1)),y=(int)(rnd()%(AH-h+1));
   if(t==0) op_fill(sdc,x,y,w,h,(COLORREF)rnd());
   else if(t==1) op_cpu(sdc,x,y,w,h);
   else {
    int sx=(int)(rnd()%(AW-w+1)),sy=(int)(rnd()%(AH-h+1));
    if(rnd()&1) {sx=x+(int)(rnd()%9)-4; sy=y+(int)(rnd()%9)-4; if(sx<0)sx=0; if(sy<0)sy=0; if(sx>AW-w)sx=AW-w; if(sy>AH-h)sy=AH-h;}
    op_copy(sdc,sx,sy,x,y,w,h);
   }
   if(i%8==7) {if(compare(sdc,"mixed")) {bad=1; failed_at=i; break;}}
   if(i%20==0) pump();
  }
  query(o); logl("  mixed sequence: %s (engine ops %lu)\n",bad?"FAIL":"pass",o[0]-e0);
  if(bad) {logl("    (failed within ops %d..%d)\n",failed_at-7,failed_at); fails++;}
 }
 {
  int kind,j; HDC bdc=CreateCompatibleDC(sdc); HBITMAP bm=CreateCompatibleBitmap(sdc,8,8); HGDIOBJ old=SelectObject(bdc,bm);
  for(j=0;j<64;j++) SetPixel(bdc,j&7,j>>3,RGB((j*47)&255,(j*89)&255,(j*17)&255));
  SelectObject(bdc,old); DeleteDC(bdc);
  for(kind=0;kind<3&&!abort_req;kind++) {
   WORD mono[8]={0x80,0x42,0x24,0x18,0xA5,0x81,0x7E,0x99}; HBITMAP mb=CreateBitmap(8,8,1,1,mono);
   HBRUSH brush=kind==0?CreatePatternBrush(mb):(kind==1?CreatePatternBrush(bm):CreateHatchBrush(HS_DIAGCROSS,RGB(17,221,63)));
   HGDIOBJ os,orr; POINT bs,br;
   fresh(sdc); query(o); e0=o[0];
   os=SelectObject(sdc,brush); orr=SelectObject(refdc,brush);
   SetBrushOrgEx(sdc,AX+3,AY+5,&bs); SetBrushOrgEx(refdc,3,5,&br);
   SetBkMode(sdc,OPAQUE); SetBkMode(refdc,OPAQUE);
   SetTextColor(sdc,RGB(199,23,77)); SetTextColor(refdc,RGB(199,23,77)); SetBkColor(sdc,RGB(29,67,211)); SetBkColor(refdc,RGB(29,67,211));
   for(j=0;j<8;j++) {int x=13+j*19,y=11+j*13; PatBlt(sdc,AX+x,AY+y,93,47,j&1?PATINVERT:PATCOPY); PatBlt(refdc,x,y,93,47,j&1?PATINVERT:PATCOPY);}
   SelectObject(sdc,os); SelectObject(refdc,orr); SetBrushOrgEx(sdc,bs.x,bs.y,NULL); SetBrushOrgEx(refdc,br.x,br.y,NULL);
   {unsigned long bad=compare(sdc,"brush origin/pattern"); query(o); logl("  brush kind %d (0 = 1-bit bitmap, 1 = colour bitmap, 2 = hatch): %s (engine ops %lu)\n",kind,bad?"FAIL":"pass",o[0]-e0); if(kind==0) n_brush0+=o[0]-e0; if(bad) fails++;}
   DeleteObject(brush); DeleteObject(mb);
  }
  DeleteObject(bm);
 }
 {
  HRGN a2=CreateRectRgn(AX+37,AY+29,AX+193,AY+137),b2=CreateRectRgn(37,29,193,137);
  fresh(sdc); query(o); e0=o[0];
  SelectClipRgn(sdc,a2); SelectClipRgn(refdc,b2);
  op_fill(sdc,4,6,257,181,RGB(253,35,17));
  StretchBlt(sdc,AX+9,AY+8,211,151,sdc,AX+39,AY+41,211,151,SRCCOPY); StretchBlt(refdc,9,8,211,151,refdc,39,41,211,151,SRCCOPY);
  SelectClipRgn(sdc,NULL); SelectClipRgn(refdc,NULL);
  {unsigned long bad=compare(sdc,"clipped fill/copy"); query(o); logl("  clipped GDI: %s (engine ops %lu)\n",bad?"FAIL":"pass",o[0]-e0); n_clip+=o[0]-e0; if(bad) fails++;}
  DeleteObject(a2); DeleteObject(b2);
 }
 return fails;
}
/* one full run of the suite at the given depth; returns 1 for PASS. The text for the summary goes to out. */
/* v10.67e: ground truth for solid brushes at 16bpp: PATCOPY a 16x16 square, then read one physical pixel back (software path) */
static void solid_probe(HDC sdc) {
 static const COLORREF sys[]={0x00C0C0C0UL,0x00808000UL,0x00000000UL,0x00FFFFFFUL,0x00808080UL,0x00000080UL,0x00800000UL,0x00008000UL,0x00FFFF00UL,0x000000FFUL};
 unsigned long save=seed; int i; unsigned px;
 logl("SOLID BRUSH PROBE: PATCOPY of a 16x16 square, then the physical pixel read back\n");
 seed=777UL;
 for(i=0;i<22&&!abort_req;i++) {
  COLORREF c; unsigned r,g,b;
  if(i<10) c=sys[i]; else {unsigned long r5=rnd()&31,g6=rnd()&63,b5=rnd()&31; c=(COLORREF)((r5*8)|((g6*4)<<8)|((b5*8)<<16));}
  fresh(sdc);
  op_fillrop(sdc,100,100,16,16,c,PATCOPY);
  BitBlt(rddc,0,0,AW,AH,sdc,AX,AY,SRCCOPY); GdiFlush();
  px=(unsigned)pixel(rdbits,(unsigned)((AH-1-108)*AW+108));
  r=(unsigned)(c&255); g=(unsigned)((c>>8)&255); b=(unsigned)((c>>16)&255);
  logl("    PROBE COLORREF=%08lX screen pixel=%04X shift565=%04X scaled565=%04X\n",(DWORD)c,px,((r>>3)<<11)|((g>>2)<<5)|(b>>3),((r*31/255)<<11)|((g*63/255)<<5)|(b*31/255));
 }
 seed=save;
}
static int run_suite(HINSTANCE hi,int bpp,char *out) {
 vx_status_t s; vx2d_stats before,after; DWORD got; unsigned long o[4],saved; HDC sdc; HWND win; int f1=0,f2=0,ok;
 logl("\n--- GDI suite at 800x600x%d ---\n",bpp);
 step("set 800x600 mode");
 if(!pr_setmode(800,600,(DWORD)bpp)) {logl("could not set 800x600x%d\n",bpp); sprintf(out,"800x600x%d could not be set",bpp); return 0;}
 wait_ms(1000);
 if(!status(&s)||s.bpp!=(DWORD)bpp||s.width!=800) {logl("driver reports %lux%lux%lu after the mode change\n",s.width,s.height,s.bpp); sprintf(out,"driver reports %lux%lux%lu, not 800x600x%d",s.width,s.height,s.bpp,bpp); ChangeDisplaySettings(NULL,0); return 0;}
 if(!DeviceIoControl(dev,VX_IOCTL_2D_STATS,NULL,0,&before,sizeof(before),&got,NULL)||got!=sizeof(before)||before.abi!=VX2D_ABI) {sprintf(out,"2D stats ioctl/ABI failed"); ChangeDisplaySettings(NULL,0); return 0;}
 if(!query(o)) {sprintf(out,"no 2D control ioctl"); ChangeDisplaySettings(NULL,0); return 0;}
 logl("engine at start: state %lu flags %08lX ops %lu fallbacks %lu last result %08lX\n",before.state,before.flags,before.ops,before.fallbacks,before.last_result);
 saved=o[3]; depth=bpp; seed=12345UL; n_solid=n_patinv=n_brush0=n_clip=0;
 if(!make_bitmaps()) {sprintf(out,"cannot create bitmaps"); ctrl(saved,o); ChangeDisplaySettings(NULL,0); return 0;}
 win=topwin(hi,"VX2PROOFT"); sdc=GetDC(win);
 {HBRUSH b=CreateSolidBrush(RGB(0,0,0)); RECT rr; GetClientRect(win,&rr); FillRect(sdc,&rr,b); DeleteObject(b);}
 step("GDI suite part 1"); logl("PART 1: conservative copy policy\n"); f1=suite(sdc,1); logl("PART 1 result: %d failing tests\n",f1);
 if(!abort_req) {seed=12345UL; step("GDI suite part 2"); logl("PART 2: every overlap direction on the engine\n"); f2=suite(sdc,2); logl("PART 2 result: %d failing tests\n",f2);}
 if(bpp==16&&!abort_req) solid_probe(sdc);
 ok=DeviceIoControl(dev,VX_IOCTL_2D_STATS,NULL,0,&after,sizeof(after),&got,NULL)&&got==sizeof(after);
 if(ok) {
  logl("engine at end: state %lu ops %lu fallbacks %lu resets %lu timeouts %lu last result %08lX\n",after.state,after.ops,after.fallbacks,after.resets,after.timeouts,after.last_result);
  logl("native completed: fills %lu, copies %lu, mono %lu, colour %lu, clipped %lu, timeouts %lu\n",after.fills-before.fills,after.copies-before.copies,after.mono-before.mono,after.color-before.color,after.clips-before.clips,after.timeouts-before.timeouts);
 }
 ctrl(saved,o); ReleaseDC(win,sdc); DestroyWindow(win); free_bitmaps(); ChangeDisplaySettings(NULL,0); wait_ms(800);
 if(abort_req) {sprintf(out,"aborted"); return 0;}
 if(!ok) {sprintf(out,"could not read the engine counters"); return 0;}
 if(after.fills==before.fills||after.copies==before.copies) {sprintf(out,"%d+%d mismatching tests, but the engine did not run fills/copies (software fallback; zero mismatches would mean nothing)",f1,f2); logl("NATIVE PATH NOT EXERCISED\n"); return 0;}
 sprintf(out,"%d failing tests in part 1, %d in part 2; engine fills %lu copies %lu mono %lu clipped %lu timeouts %lu",f1,f2,after.fills-before.fills,after.copies-before.copies,after.mono-before.mono,after.clips-before.clips,after.timeouts-before.timeouts);
 {
  char nt[200]; nt[0]=0;
  if(!n_solid) strcat(nt," solid-colour fills,"); if(!n_patinv) strcat(nt," PATINVERT,"); if(!n_brush0) strcat(nt," 1-bit pattern brushes,");
  if(nt[0]) {logl("NOT ACCELERATED (results correct, but the engine did none of these):%s\n",nt); strcat(out,"; NOT ACCELERATED:"); strcat(out,nt); out[strlen(out)-1]=0;}
 }
 return f1==0&&f2==0&&after.timeouts==before.timeouts;
}
static void stageA(HINSTANCE hi) {
 char t16[200],t32[200]; int p16,p32=-1;
 logl("\n==== STAGE A: 16bpp GDI engine path ====\n");
 p16=run_suite(hi,16,t16); logl("A 16bpp: %s - %s\n",p16?"PASS":"FAIL",t16);
 if(!p16&&!abort_req) {p32=run_suite(hi,32,t32); logl("A 32bpp control: %s - %s\n",p32?"PASS":"FAIL",t32);}
 if(p16) sprintf(sum[0],"A  16bpp GDI engine path: PASS (%s)",t16);
 else if(p32==1) sprintf(sum[0],"A  16bpp GDI engine path: FAIL (%s). The 32bpp control passed, so the fault is specific to 16bpp.",t16);
 else if(p32==0) sprintf(sum[0],"A  16bpp GDI engine path: FAIL (%s). The 32bpp control failed too (%s): suspect the test or the setup.",t16,t32);
 else sprintf(sum[0],"A  16bpp GDI engine path: FAIL (%s)",t16);
}

/* ================= stage B: DirectDraw soak ================= */
static LPDIRECTDRAW dd;
static void flipcycle(void) {
 DDSURFACEDESC s; DDSCAPS caps; LPDIRECTDRAWSURFACE prim=NULL,back=NULL; int i; DWORD cols[3];
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT; s.dwBackBufferCount=2; s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX;
 if(FAILED(IDirectDraw_CreateSurface(dd,&s,&prim,NULL))) {logl("  flip chain: CreateSurface failed\n"); return;}
 memset(&caps,0,sizeof(caps)); caps.dwCaps=DDSCAPS_BACKBUFFER;
 if(FAILED(IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back))) {IDirectDrawSurface_Release(prim); logl("  flip chain: no back buffer\n"); return;}
 cols[0]=0x00DC2828UL; cols[1]=0x0028C83CUL; cols[2]=0x003246E6UL;
 for(i=0;i<6;i++) {
  DDBLTFX f; memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=cols[i%3]; IDirectDrawSurface_Blt(back,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f);
  if(FAILED(IDirectDrawSurface_Flip(prim,NULL,DDFLIP_WAIT))) {logl("  flip %d failed\n",i); break;}
  IDirectDrawSurface_GetAttachedSurface(prim,&caps,&back);
 }
 IDirectDraw_FlipToGDISurface(dd); IDirectDrawSurface_Release(prim);
}
/* fills the primary with a known colour through the blitter and reads three pixels back through the CPU; returns the number of wrong pixels, -1 on an API failure */
static int fillcheck(DWORD w,DWORD h,DWORD b) {
 DDSURFACEDESC d; LPDIRECTDRAWSURFACE ps=NULL; DDBLTFX f; DWORD want=rgb(b,0x20,0x40,0x80),mask=b==16?0xFFFFUL:0xFFFFFFUL; int bad=0,i; DWORD px[3],py[3];
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d); d.dwFlags=DDSD_CAPS; d.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
 if(FAILED(IDirectDraw_CreateSurface(dd,&d,&ps,NULL))) return -1;
 memset(&f,0,sizeof(f)); f.dwSize=sizeof(f); f.dwFillColor=want;
 if(FAILED(IDirectDrawSurface_Blt(ps,NULL,NULL,NULL,DDBLT_COLORFILL|DDBLT_WAIT,&f))) {IDirectDrawSurface_Release(ps); return -1;}
 memset(&d,0,sizeof(d)); d.dwSize=sizeof(d);
 if(FAILED(IDirectDrawSurface_Lock(ps,NULL,&d,DDLOCK_WAIT,NULL))) {IDirectDrawSurface_Release(ps); return -1;}
 px[0]=2; py[0]=2; px[1]=w/2; py[1]=h/2; px[2]=w-3; py[2]=h-3;
 for(i=0;i<3;i++) {BYTE *p=(BYTE*)d.lpSurface+py[i]*(DWORD)d.lPitch+px[i]*(b/8); DWORD v=b==16?*(WORD*)p:*(DWORD*)p; if((v&mask)!=want) bad++;}
 IDirectDrawSurface_Unlock(ps,NULL); IDirectDrawSurface_Release(ps);
 return bad;
}
static void stageB(HINSTANCE hi) {
 static const DWORD W[6]={640,800,1024,640,800,1024},H[6]={480,600,768,480,600,768},B[6]={16,16,16,32,32,32};
 HWND win; HMODULE mod; typedef HRESULT (WINAPI *CREATE)(GUID*,LPDIRECTDRAW*,IUnknown*); CREATE create; int i,modefail=0,startbad=0,pixbad=0,apifail=0,done=0;
 logl("\n==== STAGE B: DirectDraw soak, %d back-to-back mode changes with flip chain, no restore ====\n",opt_n);
 win=topwin(hi,"VX2PROOFT"); mod=LoadLibraryA("DDRAW.DLL"); create=mod?(CREATE)GetProcAddress(mod,"DirectDrawCreate"):NULL;
 if(!create||FAILED(create(NULL,&dd,NULL))||FAILED(IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))) {logl("DirectDraw not available\n"); sprintf(sum[1],"B  DirectDraw soak: FAIL (DirectDraw not available)"); DestroyWindow(win); return;}
 for(i=1;i<=opt_n&&!abort_req;i++) {
  int k=(i-1)%6,pb; DWORD w=W[k],h=H[k],b=B[k],st,dp;
  logl("op %d: %lux%lux%lu\n",i,w,h,b); memlog(i);
  step("SetDisplayMode");
  if(FAILED(IDirectDraw_SetDisplayMode(dd,w,h,b))) {logl("  SetDisplayMode FAILED\n"); modefail++; continue;}
  step("flip chain"); flipcycle();
  step("fill and read back"); pb=fillcheck(w,h,b); if(pb<0) {apifail++; logl("  fill/lock API failure\n");} else if(pb) {pixbad+=pb; logl("  %d of 3 read-back pixels wrong\n",pb);}
  step("display start"); if(!start_info(&st,&dp,NULL)) {logl("  register dump failed\n"); apifail++;}
  else {logl("  display start %lu, driver display offset %lu\n",st,dp); if(st||dp) startbad++;}
  done++; logl("  ok\n"); wait_ms(300);
 }
 step("restore"); IDirectDraw_RestoreDisplayMode(dd); IDirectDraw_SetCooperativeLevel(dd,win,DDSCL_NORMAL); IDirectDraw_Release(dd); dd=NULL; DestroyWindow(win); wait_ms(800);
 logl("\nSTAGE B result: %d of %d mode changes completed; SetDisplayMode failures %d, API failures %d, wrong read-back pixels %d, display start not zero %d times\n",done,opt_n,modefail,apifail,pixbad,startbad);
 if(done==opt_n&&!modefail&&!apifail&&!pixbad&&!startbad) sprintf(sum[1],"B  DirectDraw soak: PASS (%d mode changes with flip chains and read-back, display start 0 every time)",done);
 else sprintf(sum[1],"B  DirectDraw soak: FAIL (%d of %d completed; mode failures %d, API failures %d, wrong pixels %d, start not zero %d)",done,opt_n,modefail,apifail,pixbad,startbad);
}

/* ================= stage C: picture position at the high modes ================= */
static HWND cwin; static int cw,ch,cardmode; static RECT cbt[4]; static volatile int lastkey,clicked; static volatile POINT clickpt; static const char *clabel="";
static LRESULT CALLBACK cproc(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_ERASEBKGND) return 1;
 if(m==WM_SETCURSOR) {SetCursor(LoadCursor(NULL,IDC_ARROW)); return TRUE;}
 if(m==WM_KEYDOWN||m==WM_SYSKEYDOWN) {lastkey=(int)a; return 0;}
 if(m==WM_LBUTTONDOWN) {clickpt.x=(short)LOWORD(b); clickpt.y=(short)HIWORD(b); clicked=1; return 0;}
 return DefWindowProc(w,m,a,b);
}
static void box(HDC dc,int x0,int y0,int x1,int y1,COLORREF c) {RECT r; HBRUSH br=CreateSolidBrush(c); SetRect(&r,x0,y0,x1,y1); FillRect(dc,&r,br); DeleteObject(br);}
static void txt(HDC dc,const char *t,int x,int y,COLORREF fg,COLORREF bg) {SetBkMode(dc,OPAQUE); SetBkColor(dc,bg); SetTextColor(dc,fg); TextOut(dc,x,y,t,(int)strlen(t));}
static void drawcard(HDC dc,int W,int H,const char *what) {
 int k,m=40,x,y,rad; char t[120]; COLORREF c[8]; static const char *nm[4]={" OK "," BAD "," SKIP "," QUIT "}; COLORREF bc[4]; int bw=(W-160)/4,y0=H*2/3+20,y1=H-70;
 HBRUSH nb=(HBRUSH)GetStockObject(NULL_BRUSH),ob; HPEN pen=CreatePen(PS_SOLID,3,RGB(255,255,255)),op;
 c[0]=RGB(255,255,255); c[1]=RGB(255,255,0); c[2]=RGB(0,255,255); c[3]=RGB(0,255,0); c[4]=RGB(255,0,255); c[5]=RGB(255,0,0); c[6]=RGB(0,0,255); c[7]=RGB(0,0,0);
 box(dc,0,0,W,H,RGB(30,30,30));
 for(k=0;k<8;k++) box(dc,W*k/8,H/3,W*(k+1)/8,H*2/3-60,c[k]);
 box(dc,W/2-1,0,W/2+1,H,c[0]); box(dc,0,H/2-1,W,H/2+1,c[0]);
 box(dc,0,0,W,3,c[0]); box(dc,0,H-3,W,H,c[0]); box(dc,0,0,3,H,c[0]); box(dc,W-3,0,W,H,c[0]);
 box(dc,3,3,3+m,3+m,RGB(255,0,0)); box(dc,W-3-m,3,W-3,3+m,RGB(0,255,0)); box(dc,3,H-3-m,3+m,H-3,RGB(0,0,255)); box(dc,W-3-m,H-3-m,W-3,H-3,RGB(255,255,0));
 txt(dc,"TL",8,8,RGB(255,255,255),RGB(255,0,0)); txt(dc,"TR",W-36,8,RGB(0,0,0),RGB(0,255,0)); txt(dc,"BL",8,H-30,RGB(255,255,255),RGB(0,0,255)); txt(dc,"BR",W-36,H-30,RGB(0,0,0),RGB(255,255,0));
 for(x=100;x<W-50;x+=100) {box(dc,x,3,x+1,23,c[0]); sprintf(t,"%d",x); txt(dc,t,x+3,24,RGB(255,255,255),RGB(30,30,30)); box(dc,x,H-23,x+1,H-3,c[0]); txt(dc,t,x+3,H-44,RGB(255,255,255),RGB(30,30,30));}
 for(y=100;y<H-50;y+=100) {box(dc,3,y,23,y+1,c[0]); sprintf(t,"%d",y); txt(dc,t,26,y+3,RGB(255,255,255),RGB(30,30,30)); box(dc,W-23,y,W-3,y+1,c[0]); txt(dc,t,W-70,y+3,RGB(255,255,255),RGB(30,30,30));}
 rad=(W<H?W:H)/4; ob=(HBRUSH)SelectObject(dc,nb); op=(HPEN)SelectObject(dc,pen); Ellipse(dc,W/2-rad,H/2-rad,W/2+rad,H/2+rad); SelectObject(dc,ob); SelectObject(dc,op); DeleteObject(pen);
 sprintf(t," %s   %dx%d   (ruler numbers are pixels) ",what,W,H); txt(dc,t,60,H/6,RGB(255,255,255),RGB(0,0,0));
 txt(dc," OK = all four corner markers and the whole frame visible, circle round, nothing cut off or shifted ",60,H/6+24,RGB(255,255,255),RGB(0,0,0));
 bc[0]=RGB(0,140,0); bc[1]=RGB(170,0,0); bc[2]=RGB(90,90,90); bc[3]=RGB(40,40,120);
 for(k=0;k<4;k++) {SetRect(&cbt[k],80+k*bw+6,y0,80+(k+1)*bw-6,y1); box(dc,cbt[k].left,cbt[k].top,cbt[k].right,cbt[k].bottom,bc[k]); txt(dc,nm[k],cbt[k].left+10,cbt[k].top+10,RGB(255,255,255),RGB(0,0,0));}
}
/* 1 OK, 2 BAD, 3 SKIP, 4 QUIT, 0 nobody. Only fresh input counts (the first 1.5 s is thrown away, the button must first be seen released). */
static int ask(void) {
 DWORD t0=GetTickCount(),end=t0+300000UL; int v=0,i,pg,pb,ps,pe,pl; POINT pt;
 wait_ms(1500); lastkey=0; clicked=0;
 pg=(GetAsyncKeyState('G')&0x8000)!=0; pb=(GetAsyncKeyState('B')&0x8000)!=0; ps=(GetAsyncKeyState('S')&0x8000)!=0; pe=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0; pl=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
 abort_req=0;
 while(GetTickCount()<end&&!v) {
  int g,b,s,e,l; MSG m;
  while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
  if(lastkey=='G'||lastkey=='O') v=1; else if(lastkey=='B') v=2; else if(lastkey=='S') v=3; else if(lastkey==27) v=4;
  else if(clicked) {clicked=0; pt.x=clickpt.x; pt.y=clickpt.y; for(i=0;i<4;i++) if(PtInRect(&cbt[i],pt)) v=i+1;}
  if(!v) {
   g=(GetAsyncKeyState('G')&0x8000)!=0; b=(GetAsyncKeyState('B')&0x8000)!=0; s=(GetAsyncKeyState('S')&0x8000)!=0; e=(GetAsyncKeyState(VK_ESCAPE)&0x8000)!=0; l=(GetAsyncKeyState(VK_LBUTTON)&0x8000)!=0;
   if(g&&!pg) v=1; else if(b&&!pb) v=2; else if(s&&!ps) v=3; else if(e&&!pe) v=4;
   else if(l&&!pl&&GetCursorPos(&pt)) {for(i=0;i<4;i++) if(PtInRect(&cbt[i],pt)) v=i+1;}
   pg=g; pb=b; ps=s; pe=e; pl=l;
  }
  Sleep(30);
 }
 return v;
}
static void stageC(HINSTANCE hi) {
 static const DWORD W[11]={1280,1152,1280,1360,1366,1440,1600,1600,1680,1920,1920},H[11]={1024,864,720,768,768,900,900,1200,1050,1080,1200};
 int i,shown=0,okc=0,badc=0,skipped=0,quit=0,notoff=0,swbad=0; static char res[11][140];
 logl("\n==== STAGE C: picture position at the high modes (32bpp) ====\n");
 for(i=0;i<11&&!quit;i++) {
  DWORD w=W[i],h=H[i]; vx_status_t s; DWORD st,dp; static vx_crt_report rep; HDC dc; int uv,swok=1,k; char what[40];
  sprintf(what,"%s",i==0?"REFERENCE (known good)":"HIGH MODE");
  logl("\nMODE %lux%lux32 (%s)\n",w,h,what);
  if(!offered(w,h,32)) {logl("  not offered by the driver for this monitor\n"); sprintf(res[i],"%4lux%-4lu not offered",w,h); notoff++; continue;}
  step("set mode"); if(!pr_setmode(w,h,32)) {logl("  ChangeDisplaySettings failed\n"); sprintf(res[i],"%4lux%-4lu mode change FAILED",w,h); swbad++; ChangeDisplaySettings(NULL,0); wait_ms(800); continue;}
  wait_ms(1200); status(&s); logl("  driver reports %lux%lux%lu pitch %lu\n",s.width,s.height,s.bpp,s.video_pitch);
  if(s.width!=w||s.height!=h||s.bpp!=32) swok=0;
  if(start_info(&st,&dp,&rep)) {
   logl("  display start %lu, driver display offset %lu\n",st,dp); if(st||dp) swok=0;
   logl("  CR00-CR16:"); for(k=0;k<=0x16;k++) logl(" %02lX",rep.cr[k]); logl("\n  CR33 %02lX CR35 %02lX CR36 %02lX CR13 %02lX  SR1C %02lX SR1D %02lX SR44 %02lX SR45 %02lX SR46 %02lX  misc %02lX\n",rep.cr[0x33],rep.cr[0x35],rep.cr[0x36],rep.cr[0x13],rep.sr[0x1C],rep.sr[0x1D],rep.sr[0x44],rep.sr[0x45],rep.sr[0x46],rep.misc);
  } else {logl("  register dump failed\n"); swok=0;}
  cwin=topwin(hi,"VX2PROOFC"); cw=GetSystemMetrics(SM_CXSCREEN); ch=GetSystemMetrics(SM_CYSCREEN); dc=GetDC(cwin); drawcard(dc,cw,ch,what); ReleaseDC(cwin,dc);
  if((DWORD)cw!=w||(DWORD)ch!=h) {logl("  screen metrics %dx%d differ from the mode\n",cw,ch); swok=0;}
  step("waiting for your answer"); uv=ask(); logl("  your answer: %s\n",uv==1?"OK":uv==2?"BAD":uv==3?"skipped":uv==4?"quit":"no answer (5 min)");
  DestroyWindow(cwin); cwin=NULL; ChangeDisplaySettings(NULL,0); wait_ms(1000);
  shown++; logl("  software verdict: %s\n",swok?"PASS (mode as asked, start 0)":"FAIL");
  sprintf(res[i],"%4lux%-4lu software %s, you: %s",w,h,swok?"PASS":"FAIL",uv==1?"OK":uv==2?"BAD":uv==3?"skipped":uv==4?"quit":"no answer");
  if(uv==1) okc++; else if(uv==2) badc++; else skipped++;
  if(!swok) swbad++;
  if(uv==4) quit=1;
 }
 logl("\nSTAGE C table:\n"); for(i=0;i<11;i++) if(res[i][0]) logl("  %s\n",res[i]);
 sprintf(sum[2],"C  High-mode position: %d OK, %d BAD, %d skipped/unanswered, %d not offered, %d software failures (of %d shown)%s",okc,badc,skipped,notoff,swbad,shown,quit?"; quit early":"");
}

/* ================= start window ================= */
static void runstages(HINSTANCE hi,int mask) {
 int i; char m[900];
 busy=1; abort_req=0; for(i=0;i<3;i++) sum[i][0]=0;
 ShowWindow(hmain,SW_HIDE); pump();
 if(mask&1) stageA(hi);
 if(mask&2&&!abort_req) stageB(hi);
 if(mask&4&&!abort_req) stageC(hi);
 ChangeDisplaySettings(NULL,0);
 ShowWindow(hmain,SW_SHOW);
 logl("\nSUMMARY\n"); m[0]=0;
 for(i=0;i<3;i++) if(sum[i][0]) {logl("%s\n",sum[i]); strcat(m,sum[i]); strcat(m,"\n\n");}
 if(abort_req) {logl("(stopped with Esc)\n"); strcat(m,"(stopped with Esc)\n");}
 strcat(m,"Send VX2PROOF.TXT.");
 MessageBox(hmain,m,"VX2PROOF",MB_OK);
 SetWindowText(hstat,"Done. Results are in VX2PROOF.TXT. Click a button to run again (the log restarts).");
 busy=0;
}
static HINSTANCE ghi;
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l) {
 switch(m) {
 case WM_COMMAND:
  if(HIWORD(w)==BN_CLICKED&&!busy&&LOWORD(w)>=100&&LOWORD(w)<=103) {
   int id=LOWORD(w)-100,mask=id==0?7:(1<<(id-1));
   if(lf) fclose(lf); lf=fopen("VX2PROOF.TXT","w"); logl("VX2PROOF v1.0 (expects VX85598.VXD v10.65)\n");
   runstages(ghi,mask);
  }
  return 0;
 case WM_CLOSE: if(busy) return 0; DestroyWindow(h); return 0;
 case WM_DESTROY: PostQuitMessage(0); return 0;
 }
 return DefWindowProc(h,m,w,l);
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE prev,LPSTR cmd,int show_) {
 WNDCLASS wc; MSG m; vx_status_t s; char t[300]; static const char *bn[4]={"RUN ALL (A, B, C)","A: 16bpp GDI test","B: DirectDraw soak","C: high-mode position"};
 int i; (void)prev;(void)show_; ghi=hi;
 if(cmd&&strstr(cmd,"n=")) opt_n=atoi(strstr(cmd,"n=")+2); if(opt_n<1) opt_n=1;
 SetUnhandledExceptionFilter(crashfilter);
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=proc; wc.hInstance=hi; wc.lpszClassName="VX2PROOF"; wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1); wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClass(&wc);
 wc.lpfnWndProc=twnd; wc.lpszClassName="VX2PROOFT"; wc.hbrBackground=NULL; RegisterClass(&wc);
 wc.lpfnWndProc=cproc; wc.lpszClassName="VX2PROOFC"; RegisterClass(&wc);
 hmain=CreateWindow("VX2PROOF","VX855 proof run (VX2PROOF v1.0)",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,20,20,560,260,NULL,NULL,hi,NULL);
 for(i=0;i<4;i++) hbtn[i]=CreateWindow("BUTTON",bn[i],WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,20+(i%2)*260,20+(i/2)*44,240,36,hmain,(HMENU)(100+i),hi,NULL);
 hstat=CreateWindow("STATIC","",WS_CHILD|WS_VISIBLE,20,120,520,90,hmain,NULL,hi,NULL);
 if(!status(&s)||s.magic!=VX_MAGIC) {strcpy(t,"Cannot read the VX85598 driver."); for(i=0;i<4;i++) EnableWindow(hbtn[i],FALSE);}
 else sprintf(t,"Desktop %lux%lux%lu. A takes about 2 minutes, B about %d s, C needs you to click OK/BAD per card. Esc stops a part. Screen modes change during the run.",s.width,s.height,s.bpp,opt_n*2);
 SetWindowText(hstat,t);
 ShowWindow(hmain,SW_SHOW); UpdateWindow(hmain);
 while(GetMessage(&m,NULL,0,0)>0) {TranslateMessage(&m); DispatchMessage(&m);}
 if(lf) fclose(lf);
 if(dev!=INVALID_HANDLE_VALUE) CloseHandle(dev);
 return 0;
}
