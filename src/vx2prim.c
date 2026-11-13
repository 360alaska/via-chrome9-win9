/* VX2PRIM v1.0: what the driver still leaves to the DIB engine, measured before any hook is written. Needs VX85598.DRV and VX85598.VXD from v10.91 or later (the call capture).
 *   T1  survey: for each GDI primitive (Rectangle, FrameRect, FillRect, DrawFocusRect, RoundRect, Ellipse, Polygon, dashed and dotted lines, wide lines, Arc, Pie, SetPixel, GetPixel,
 *       FloodFill, TextOut with three fonts, DrawText, SetDIBitsToDevice, StretchDIBits) the driver's call capture (ExtEscape 8F72, command 4) lists which DDI calls reached it and with what
 *       arguments: Output (style, points, pen, draw mode), FastBorder, StrBlt, DibToDevice, StretchDIBits, ScanLR, Pixel (the first 20 stack words), brush fills and ExtTextOut.
 *   T2  what DIBENG draws for dashed and dotted pens (run lengths along horizontal, vertical, diagonal and shallow lines, opaque and transparent background, a Polyline against separate LineTo calls)
 *       and for wide pens (the pixels of a short line, widths 2 to 8), on a memory bitmap of the screen format.
 *   T3  bitmap upload speed: SetDIBitsToDevice and StretchDIBits (same depth, 24-bit source, 2x stretch), BitBlt from a memory bitmap to the screen, screen to screen (engine), memory to memory (CPU)
 *       and a full-screen PatBlt (engine), in ms per frame and MB/s of destination bytes, against the 542 MB/s the CPU writes to video memory (VX2READ, v10.69).
 * Depths 32, 16 and 8 bits per pixel at 640x480 (the desktop is switched and put back). Draws on the desktop for a few seconds. Unattended: run it, read VX2PRIM.TXT (VX2PRIM.RES says RUNNING until it ends).
 * Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"
#include "vx_hatch_diag.h"

static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static vxh_reply rep; static int bpp;
static void logline(const char *fmt,...) {
 va_list a;
 if(!lf) return;
 va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));
}
static void finish(const char *res) {FILE *f=fopen("VX2PRIM.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
 return !aborted;
}
static double now(void) {
 LARGE_INTEGER c,f;
 if(QueryPerformanceFrequency(&f)&&f.QuadPart) {QueryPerformanceCounter(&c); return (double)c.QuadPart*1000.0/(double)f.QuadPart;}
 return (double)GetTickCount();
}

/* ---- T1 ---- */
static int hesc(HDC dc,DWORD command,void *out) {
 vxh_command c; c.cb=sizeof(c); c.magic=VXH_MAGIC; c.command=command; c.reserved=0;
 return ExtEscape(dc,VXH_ESCAPE,sizeof(c),(LPCSTR)&c,out?(int)sizeof(vxh_reply):0,(LPSTR)out);
}
static const char *tagname(DWORD t) {
 switch(t) {
  case VXH_TAG_FBRD: return "FastBorder"; case VXH_TAG_STRB: return "StrBlt"; case VXH_TAG_DTDV: return "DibToDevice"; case VXH_TAG_SDIB: return "StretchDIBits";
  case VXH_TAG_SCLR: return "ScanLR"; case VXH_TAG_PIXL: return "Pixel"; case VXH_TAG_OUTP: return "Output"; case 0x54584554UL: return "ExtTextOut"; default: return "brush fill";
 }
}
static void show_rec(const vxh_record *t) {
 if(t->rop==VXH_TAG_OUTP) {
  logline("      Output (to DIBENG): style %lu, %lu points, first (%d,%d), pen style %lu flags %02lX bpp %lu colour %08lX mono %08lX, brush style %lX flags %02lX, Rop2 %lu, bkMode %lu, bk %08lX, text %08lX\n",
   t->h,t->w,(int)(short)t->x,(int)(short)t->y,t->flags&0xFFFF,(t->flags>>16)&0xFF,t->brush_bpp,t->raw_fg,t->raw_bg,t->hatch,t->mono[0],t->mask[0],t->bk_mode,t->bk_color,t->text_color);
  logline("        points:"); {int i; for(i=0;i<4&&(DWORD)i<t->w;i++) logline(" (%d,%d)",(int)(short)(t->bits[i]&0xFFFF),(int)(short)(t->bits[i]>>16)); logline("\n");}
 } else if(t->rop==VXH_TAG_FBRD||t->rop==VXH_TAG_STRB||t->rop==VXH_TAG_DTDV||t->rop==VXH_TAG_SDIB||t->rop==VXH_TAG_SCLR||t->rop==VXH_TAG_PIXL) {
  const uint32_t *w=&t->flags; int i;
  logline("      %s: 20 stack words above the return address (last Pascal parameter first):",tagname(t->rop));
  for(i=0;i<10;i++) logline(" %04lX %04lX",w[i]&0xFFFF,w[i]>>16);
  logline("\n");
 } else if(t->rop==0x54584554UL) {
  logline("      ExtTextOut: opaque rectangle (%lu,%lu)-(%lu,%lu), options %lX, engine answer %lX, conditions %lX\n",t->x,t->y,t->x+t->w,t->y+t->h,t->style,t->hatch,t->flags);
 } else {
  logline("      brush fill: ROP %08lX at (%lu,%lu) %lux%lu, brush style %lX flags %02lX\n",t->rop,t->x,t->y,t->w,t->h,t->style,t->flags);
 }
}
static void begin_cap(HDC dc) {hesc(dc,VXH_ARM_CALLS,NULL);}
static void end_cap(HDC dc,const char *what) {
 DWORD i;
 memset(&rep,0,sizeof(rep));
 if(hesc(dc,VXH_READ,&rep)<=0||rep.magic!=VXH_MAGIC) {logline("  %s: the capture escape is not answered (this DRV is older than v10.91)\n",what); return;}
 hesc(dc,VXH_STOP,NULL);
 logline("  %s: %lu DDI calls recorded%s\n",what,rep.count,rep.dropped?" (more were dropped)":"");
 for(i=0;i<rep.count&&i<VXH_CAP;i++) show_rec(&rep.records[i]);
}
static void clear_area(HDC dc) {
 HBRUSH b=CreateSolidBrush(RGB(0,0,0)); RECT r; r.left=0; r.top=0; r.right=400; r.bottom=300; FillRect(dc,&r,b); DeleteObject(b);
}
static void survey(HDC dc) {
 HPEN pen,old; HBRUSH br,oldb; POINT tri[3]; int i; RECT r; HFONT fnt,oldf; BITMAPINFO bi; static BYTE bits[64*64*4]; COLORREF ck;
 static const int dstyles[4]={PS_DASH,PS_DOT,PS_DASHDOT,PS_DASHDOTDOT}; static const char *dname[4]={"PS_DASH","PS_DOT","PS_DASHDOT","PS_DASHDOTDOT"};
 logline("-- T1 which DDI calls GDI makes, %d bits per pixel --\n",bpp);
 pen=CreatePen(PS_SOLID,1,RGB(255,255,0)); br=CreateSolidBrush(RGB(0,0,255));
 old=(HPEN)SelectObject(dc,pen); oldb=(HBRUSH)SelectObject(dc,br);
 clear_area(dc); begin_cap(dc); Rectangle(dc,20,20,120,90); end_cap(dc,"Rectangle (solid pen 1, solid brush)");
 SelectObject(dc,GetStockObject(NULL_BRUSH)); clear_area(dc); begin_cap(dc); Rectangle(dc,20,20,120,90); end_cap(dc,"Rectangle (solid pen 1, hollow brush)");
 SelectObject(dc,br); r.left=20; r.top=20; r.right=120; r.bottom=90;
 clear_area(dc); begin_cap(dc); FrameRect(dc,&r,br); end_cap(dc,"FrameRect (brush)");
 clear_area(dc); begin_cap(dc); FillRect(dc,&r,br); end_cap(dc,"FillRect");
 clear_area(dc); begin_cap(dc); DrawFocusRect(dc,&r); end_cap(dc,"DrawFocusRect");
 clear_area(dc); begin_cap(dc); RoundRect(dc,20,20,120,90,16,16); end_cap(dc,"RoundRect");
 clear_area(dc); begin_cap(dc); Ellipse(dc,20,20,120,90); end_cap(dc,"Ellipse");
 tri[0].x=20; tri[0].y=90; tri[1].x=70; tri[1].y=20; tri[2].x=120; tri[2].y=90;
 clear_area(dc); begin_cap(dc); Polygon(dc,tri,3); end_cap(dc,"Polygon (3 points, pen and brush)");
 clear_area(dc); begin_cap(dc); Arc(dc,20,20,120,90,120,55,20,55); end_cap(dc,"Arc");
 clear_area(dc); begin_cap(dc); Pie(dc,20,20,120,90,120,55,20,55); end_cap(dc,"Pie");
 for(i=0;i<4;i++) {
  char name[96]; HPEN dp=CreatePen(dstyles[i],1,RGB(255,255,255)); HPEN o2=(HPEN)SelectObject(dc,dp);
  clear_area(dc); SetBkMode(dc,OPAQUE); begin_cap(dc); MoveToEx(dc,10,10,NULL); LineTo(dc,150,60); sprintf(name,"%s line, opaque background",dname[i]); end_cap(dc,name);
  clear_area(dc); SetBkMode(dc,TRANSPARENT); begin_cap(dc); MoveToEx(dc,10,10,NULL); LineTo(dc,150,60); sprintf(name,"%s line, transparent background",dname[i]); end_cap(dc,name);
  SelectObject(dc,o2); DeleteObject(dp);
 }
 SetBkMode(dc,OPAQUE);
 {int wd[2]; wd[0]=3; wd[1]=8; for(i=0;i<2;i++) {char name[64]; HPEN wp=CreatePen(PS_SOLID,wd[i],RGB(255,255,255)); HPEN o2=(HPEN)SelectObject(dc,wp);
  clear_area(dc); begin_cap(dc); MoveToEx(dc,10,10,NULL); LineTo(dc,150,60); sprintf(name,"wide line, width %d",wd[i]); end_cap(dc,name); SelectObject(dc,o2); DeleteObject(wp);}}
 clear_area(dc); begin_cap(dc); SetPixel(dc,30,30,RGB(255,255,255)); end_cap(dc,"SetPixel");
 begin_cap(dc); ck=GetPixel(dc,30,30); end_cap(dc,"GetPixel");
 clear_area(dc); SelectObject(dc,pen); SelectObject(dc,GetStockObject(NULL_BRUSH)); Rectangle(dc,20,20,120,90); SelectObject(dc,br);
 begin_cap(dc); FloodFill(dc,50,50,RGB(255,255,0)); end_cap(dc,"FloodFill (border colour)");
 clear_area(dc); SetBkMode(dc,OPAQUE); SetTextColor(dc,RGB(255,255,255)); SetBkColor(dc,RGB(0,0,128));
 begin_cap(dc); TextOut(dc,20,20,"Hello, VX855 0123",17); end_cap(dc,"TextOut, default font");
 oldf=(HFONT)SelectObject(dc,GetStockObject(SYSTEM_FONT));
 begin_cap(dc); TextOut(dc,20,40,"Hello, VX855 0123",17); end_cap(dc,"TextOut, SYSTEM_FONT (raster)");
 fnt=CreateFont(-14,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,DEFAULT_PITCH,"Arial");
 SelectObject(dc,fnt); begin_cap(dc); TextOut(dc,20,60,"Hello, VX855 0123",17); end_cap(dc,"TextOut, Arial 14 (TrueType)");
 SelectObject(dc,oldf); DeleteObject(fnt);
 fnt=CreateFont(-12,0,0,0,FW_NORMAL,0,0,0,ANSI_CHARSET,OUT_RASTER_PRECIS,CLIP_DEFAULT_PRECIS,DEFAULT_QUALITY,VARIABLE_PITCH,"MS Sans Serif");
 oldf=(HFONT)SelectObject(dc,fnt); begin_cap(dc); TextOut(dc,20,80,"Hello, VX855 0123",17); end_cap(dc,"TextOut, MS Sans Serif 12 (raster)");
 r.left=20; r.top=100; r.right=200; r.bottom=130;
 begin_cap(dc); DrawText(dc,"DrawText 0123",13,&r,DT_LEFT|DT_TOP); end_cap(dc,"DrawText (opaque, raster)");
 SelectObject(dc,oldf); DeleteObject(fnt);
 memset(&bi,0,sizeof(bi)); bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); bi.bmiHeader.biWidth=64; bi.bmiHeader.biHeight=64; bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=24; bi.bmiHeader.biCompression=BI_RGB;
 memset(bits,0x80,sizeof(bits));
 begin_cap(dc); SetDIBitsToDevice(dc,200,20,64,64,0,0,0,64,bits,&bi,DIB_RGB_COLORS); end_cap(dc,"SetDIBitsToDevice 64x64, 24-bit source");
 begin_cap(dc); StretchDIBits(dc,200,100,64,64,0,0,64,64,bits,&bi,DIB_RGB_COLORS,SRCCOPY); end_cap(dc,"StretchDIBits 1:1, 24-bit source");
 begin_cap(dc); StretchDIBits(dc,280,100,128,128,0,0,64,64,bits,&bi,DIB_RGB_COLORS,SRCCOPY); end_cap(dc,"StretchDIBits 2x, 24-bit source");
 SelectObject(dc,old); SelectObject(dc,oldb); DeleteObject(pen); DeleteObject(br);
 clear_area(dc); (void)ck;
}

/* ---- T2 ---- */
#define PW RGB(255,255,255)
#define BKC RGB(255,0,0)
static HDC mdc; static HBITMAP mbm,mold;
static void rle(char *dst,const char *src,int n) {
 int i=0,len=0; dst[0]=0;
 while(i<n) {int j=i; char tmp[24]; while(j<n&&src[j]==src[i]) j++; sprintf(tmp,"%c%d ",src[i],j-i); strcpy(dst+len,tmp); len+=(int)strlen(tmp); i=j;}
}
static char cls(COLORREF c) {return c==PW?'P':c==BKC?'B':c==0?'D':'?';}
static void clear_m(void) {PatBlt(mdc,0,0,200,200,BLACKNESS);}
/* draw one line (or polyline) with a style and read it back along a path */
static void dashrun(const char *name,int ps,int bk,const POINT *pt,int np,int poly,int axis) {
 HPEN pen=CreatePen(ps,1,PW),old; char line[512],out[2048]; int i,k; int n;
 clear_m(); old=(HPEN)SelectObject(mdc,pen); SetBkMode(mdc,bk?OPAQUE:TRANSPARENT); SetBkColor(mdc,BKC);
 if(poly) Polyline(mdc,(POINT*)pt,np);
 else if(poly==0&&np==2) {MoveToEx(mdc,pt[0].x,pt[0].y,NULL); LineTo(mdc,pt[1].x,pt[1].y);}
 SelectObject(mdc,old); DeleteObject(pen);
 n=0;
 if(axis==0) {for(i=pt[0].x;i<pt[np-1].x&&n<500;i++) line[n++]=cls(GetPixel(mdc,i,pt[0].y));}                      /* left to right along y */
 else if(axis==1) {for(i=pt[0].x;i>pt[np-1].x&&n<500;i--) line[n++]=cls(GetPixel(mdc,i,pt[0].y));}              /* right to left */
 else if(axis==2) {for(i=pt[0].y;i<pt[np-1].y&&n<500;i++) line[n++]=cls(GetPixel(mdc,pt[0].x,i));}              /* downwards along x */
 else if(axis==3) {for(i=0;pt[0].x+i<pt[np-1].x&&n<500;i++) line[n++]=cls(GetPixel(mdc,pt[0].x+i,pt[0].y+i));}  /* diagonal */
 else {for(i=pt[0].x;i<pt[np-1].x&&n<500;i++) {char c='D'; for(k=-2;k<=2;k++) {char t=cls(GetPixel(mdc,i,pt[0].y+((i-pt[0].x)*(pt[np-1].y-pt[0].y))/(pt[np-1].x-pt[0].x)+k)); if(t=='P') c='P'; else if(t=='B'&&c!='P') c='B'; else if(t=='?'&&c=='D') c='?';} line[n++]=c;}}
 rle(out,line,n); logline("    %-34s %s\n",name,out);
}
static void widemap(int w,int x0,int y0,int x1,int y1) {
 HPEN pen=CreatePen(PS_SOLID,w,PW),old; int x,y,xa,ya,xb,yb;
 clear_m(); old=(HPEN)SelectObject(mdc,pen); MoveToEx(mdc,x0,y0,NULL); LineTo(mdc,x1,y1); SelectObject(mdc,old); DeleteObject(pen);
 xa=(x0<x1?x0:x1)-6; xb=(x0>x1?x0:x1)+6; ya=(y0<y1?y0:y1)-6; yb=(y0>y1?y0:y1)+6;
 logline("    width %d, line (%d,%d)-(%d,%d), window x %d..%d y %d..%d\n",w,x0,y0,x1,y1,xa,xb,ya,yb);
 for(y=ya;y<=yb;y++) {char row[64]; int n=0; for(x=xa;x<=xb&&n<60;x++) row[n++]=cls(GetPixel(mdc,x,y))=='P'?'#':'.'; row[n]=0; logline("      %s\n",row);}
}
static void dashes(HDC sdc) {
 static const int ps[4]={PS_DASH,PS_DOT,PS_DASHDOT,PS_DASHDOTDOT}; static const char *pn[4]={"PS_DASH","PS_DOT","PS_DASHDOT","PS_DASHDOTDOT"};
 int i,b; POINT h[2],hr[2],v[2],d[2],s[2],pl[4]; char nm[96]; static const int ws[5]={2,3,4,5,8};
 logline("-- T2 DIBENG dashed and wide pens on a memory bitmap, %d bits per pixel (P pen, B background colour, D destination, runs along the line) --\n",bpp);
 mdc=CreateCompatibleDC(NULL); {HDC sc=GetDC(NULL); mbm=CreateCompatibleBitmap(sc,200,200); ReleaseDC(NULL,sc);} mold=(HBITMAP)SelectObject(mdc,mbm);
 h[0].x=10; h[0].y=50; h[1].x=190; h[1].y=50; hr[0].x=190; hr[0].y=50; hr[1].x=10; hr[1].y=50; v[0].x=50; v[0].y=10; v[1].x=50; v[1].y=190;
 d[0].x=10; d[0].y=10; d[1].x=190; d[1].y=190; s[0].x=10; s[0].y=10; s[1].x=190; s[1].y=100;
 pl[0].x=10; pl[0].y=50; pl[1].x=70; pl[1].y=50; pl[2].x=130; pl[2].y=50; pl[3].x=190; pl[3].y=50;
 for(i=0;i<4&&pump();i++) for(b=1;b>=0;b--) {
  logline("  %s, %s background\n",pn[i],b?"OPAQUE":"TRANSPARENT");
  sprintf(nm,"horizontal left to right"); dashrun(nm,ps[i],b,h,2,0,0);
  sprintf(nm,"horizontal right to left"); dashrun(nm,ps[i],b,hr,2,0,1);
  sprintf(nm,"vertical downwards"); dashrun(nm,ps[i],b,v,2,0,2);
  sprintf(nm,"diagonal 45 degrees"); dashrun(nm,ps[i],b,d,2,0,3);
  sprintf(nm,"shallow (180:90)"); dashrun(nm,ps[i],b,s,2,0,4);
  sprintf(nm,"Polyline of 3 horizontal segments"); dashrun(nm,ps[i],b,pl,4,1,0);
  {HPEN pen=CreatePen(ps[i],1,PW),old; char line[512],out[2048]; int x,n=0;
   clear_m(); old=(HPEN)SelectObject(mdc,pen); SetBkMode(mdc,b?OPAQUE:TRANSPARENT); SetBkColor(mdc,BKC);
   MoveToEx(mdc,10,50,NULL); LineTo(mdc,70,50); LineTo(mdc,130,50); LineTo(mdc,190,50);
   SelectObject(mdc,old); DeleteObject(pen);
   for(x=10;x<190&&n<500;x++) line[n++]=cls(GetPixel(mdc,x,50));
   rle(out,line,n); logline("    %-34s %s\n","three LineTo calls in a row",out);}
 }
 logline("  wide pens (solid, PS_SOLID)\n");
 for(i=0;i<5&&pump();i++) {widemap(ws[i],30,30,50,30); widemap(ws[i],30,30,30,50); widemap(ws[i],30,30,42,42); widemap(ws[i],30,30,50,36);}
 SelectObject(mdc,mold); DeleteObject(mbm); DeleteDC(mdc);
}

/* ---- T3 ---- */
static BYTE *ubits; static BITMAPINFO *ubi;
static void mkinfo(int bits,int w,int h) {
 memset(ubi,0,sizeof(BITMAPINFOHEADER)+256*4); ubi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER); ubi->bmiHeader.biWidth=w; ubi->bmiHeader.biHeight=h; ubi->bmiHeader.biPlanes=1;
 ubi->bmiHeader.biBitCount=(WORD)bits; ubi->bmiHeader.biCompression=BI_RGB;
 if(bits==8) {int i; ubi->bmiHeader.biClrUsed=256; for(i=0;i<256;i++) {ubi->bmiColors[i].rgbRed=(BYTE)i; ubi->bmiColors[i].rgbGreen=(BYTE)i; ubi->bmiColors[i].rgbBlue=(BYTE)i;}}
}
typedef enum {U_SDI,U_SDI24,U_STR,U_STR2,U_BLT,U_S2S,U_M2M,U_PAT} utest;
static HDC udc,umdc,umdc2; static int ubpp;
static void doone(utest t) {
 switch(t) {
  case U_SDI: SetDIBitsToDevice(udc,0,0,640,480,0,0,0,480,ubits,ubi,DIB_RGB_COLORS); break;
  case U_SDI24: SetDIBitsToDevice(udc,0,0,640,480,0,0,0,480,ubits,ubi,DIB_RGB_COLORS); break;
  case U_STR: StretchDIBits(udc,0,0,640,480,0,0,640,480,ubits,ubi,DIB_RGB_COLORS,SRCCOPY); break;
  case U_STR2: StretchDIBits(udc,0,0,640,480,0,0,320,240,ubits,ubi,DIB_RGB_COLORS,SRCCOPY); break;
  case U_BLT: BitBlt(udc,0,0,640,480,umdc,0,0,SRCCOPY); break;
  case U_S2S: BitBlt(udc,320,0,320,480,udc,0,0,SRCCOPY); break;
  case U_M2M: BitBlt(umdc2,0,0,640,480,umdc,0,0,SRCCOPY); break;
  case U_PAT: PatBlt(udc,0,0,640,480,BLACKNESS); break;
 }
}
static void timeit(utest t,const char *name,double bytes) {
 double t0,t1,ms; int n=0;
 doone(t); /* warm up */
 t0=now(); do {doone(t); n++; t1=now();} while(t1-t0<500.0&&n<2000&&pump());
 ms=(t1-t0)/(double)n;
 logline("    %-52s %8.2f ms/frame %8.1f MB/s %5.1f%% of 542 MB/s\n",name,ms,bytes/ms/1000.0,bytes/ms/1000.0/542.0*100.0);
}
static void uploads(HDC sdc) {
 double full=640.0*480.0*(double)(ubpp/8); char nm[96]; HBITMAP bm,old,bm2,old2;
 logline("-- T3 bitmap upload speed, %d bits per pixel (640x480, destination %.0f KB per frame) --\n",bpp,full/1024.0);
 udc=sdc; ubpp=bpp;
 umdc=CreateCompatibleDC(sdc); bm=CreateCompatibleBitmap(sdc,640,480); old=(HBITMAP)SelectObject(umdc,bm);
 umdc2=CreateCompatibleDC(sdc); bm2=CreateCompatibleBitmap(sdc,640,480); old2=(HBITMAP)SelectObject(umdc2,bm2);
 PatBlt(umdc,0,0,640,480,WHITENESS);
 mkinfo(bpp==8?8:bpp==16?16:32,640,480); sprintf(nm,"SetDIBitsToDevice, %d-bit source (same depth)",bpp); timeit(U_SDI,nm,full);
 mkinfo(24,640,480); timeit(U_SDI24,"SetDIBitsToDevice, 24-bit source",full);
 mkinfo(bpp==8?8:bpp==16?16:32,640,480); sprintf(nm,"StretchDIBits 1:1, %d-bit source",bpp); timeit(U_STR,nm,full);
 mkinfo(24,640,480); timeit(U_STR,"StretchDIBits 1:1, 24-bit source",full);
 mkinfo(24,320,240); timeit(U_STR2,"StretchDIBits 2x, 24-bit source",full);
 timeit(U_BLT,"BitBlt memory bitmap to screen",full);
 timeit(U_S2S,"BitBlt screen to screen, 320x480 (engine)",full/2.0);
 timeit(U_M2M,"BitBlt memory bitmap to memory bitmap (CPU only)",full);
 timeit(U_PAT,"PatBlt BLACKNESS 640x480 (engine fill)",full);
 SelectObject(umdc2,old2); DeleteObject(bm2); DeleteDC(umdc2); SelectObject(umdc,old); DeleteObject(bm); DeleteDC(umdc);
}

/* ---- main ---- */
static DEVMODEA orig;
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc;
 memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc);
 return rc==DISP_CHANGE_SUCCESSFUL;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 static const int ds[3]={32,16,8};
 vx_status_t s; DWORD got=0,t; int k,rc=0,have_orig; HDC sdc; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2PRIM.TXT","w"); logline("VX2PRIM v1.0 / driver v10.91\n");
 finish("RUNNING: the run did not reach its end (VX2PRIM.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 ubits=(BYTE*)calloc(640UL*480UL*4UL,1); ubi=(BITMAPINFO*)calloc(sizeof(BITMAPINFOHEADER)+256*4,1);
 if(!ubits||!ubi) {finish("FAIL: out of memory"); return 1;}
 memset(&orig,0,sizeof(orig)); orig.dmSize=sizeof(orig); have_orig=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&orig)?1:0;
 logline("desktop was %lux%lux%lu\n",have_orig?orig.dmPelsWidth:0,have_orig?orig.dmPelsHeight:0,have_orig?orig.dmBitsPerPel:0);
 for(k=0;k<3&&!rc&&!aborted;k++) {
  bpp=ds[k];
  if(!set_mode(640,480,(DWORD)bpp)) {logline("could not set 640x480x%d\n",bpp); finish("FAIL: mode not available"); rc=1; break;}
  t=GetTickCount(); while(GetTickCount()-t<800) Sleep(20);
  memset(&s,0,sizeof(s));
  if(!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||s.width!=640||s.height!=480||s.bpp!=(DWORD)bpp||s.stage!=6||s.failure||s.vga_mode) {
   logline("the driver is not in native 640x480x%d mode\n",bpp); finish("FAIL: not native mode"); rc=1; break;
  }
  sdc=GetDC(NULL);
  if(!sdc) {finish("FAIL: no screen DC"); rc=1; break;}
  survey(sdc);
  if(pump()) dashes(sdc);
  if(pump()) uploads(sdc);
  ReleaseDC(NULL,sdc);
 }
 if(aborted) {finish("ABORTED"); logline("aborted\n");}
 else if(!rc) finish("DONE: see VX2PRIM.TXT");
 if(have_orig) {ChangeDisplaySettingsA(&orig,0); logline("desktop put back\n");}
 logline("Finished.\n"); if(lf) fclose(lf);
 return rc;
}
