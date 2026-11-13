/* VX2SDI2 v1.0: the rest of what a StretchDIBits fast path needs to know for 16-bit screens (the driver is not changed; v10.91 or later, because it uses the DDI call capture of VX2PRIM).
 * VX2SDIB showed: 565 BITFIELDS sources are already fast (1.45 ms for 2x); 555, 24-bit and 32-bit sources are slow (about 20 ms for 2x); 24/32-bit colours are truncated; the 555 green sixth bit is 0 at 1:1
 * but depends on the value when stretched 2x; a 565 source stretched 3x puts its source column boundaries one pixel late; for a top-down bitmap a source rectangle is counted from the bottom, like a bottom-up one.
 * This program uses 555, 24-bit and 32-bit sources of 21x13 pixels whose colours are all different after conversion, and for 2x, 3x and 4x enlargement:
 *   T1  the pixel mapping: for every destination column (in the first destination row) and row (in the first column) which source column/row was taken, compared with floor(i/k); the list when it differs.
 *   T2  the green sixth bit of the 555 source stretched k times, per 5-bit green value (0, 1 or mixed over the red/blue values) and whether a k x k block is one colour.
 *   T3  the source rectangle and the DDI arguments: bottom-up and top-down bitmaps, whole source and a rectangle 3,2 10x7 at 2x: which source row and column is at the top left of the destination, and the arguments the driver
 *       receives (source x, y, width, height, destination width, height as signed 16-bit numbers, the ROP, whether a clip rectangle and a translation table are given), for the same cases plus mirrored destinations.
 * 640x480x16 (the desktop is switched and put back). Unattended: run it, read VX2SDI2.TXT (VX2SDI2.RES says RUNNING until it ends). Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"
#include "vx_hatch_diag.h"

static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {
 va_list a;
 if(!lf) return;
 va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));
}
static void finish(const char *res) {FILE *f=fopen("VX2SDI2.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {
 MSG m;
 while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
 if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1;
 return !aborted;
}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc;
 memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc);
 return rc==DISP_CHANGE_SUCCESSFUL;
}
static HDC sdc,mdc; static HBITMAP dib,olddib; static WORD *tgt;
typedef struct {BITMAPINFOHEADER h; DWORD m[3];} bi565;
static int make_target(void) {
 bi565 b; void *bits=NULL;
 memset(&b,0,sizeof(b)); b.h.biSize=sizeof(BITMAPINFOHEADER); b.h.biWidth=640; b.h.biHeight=-480; b.h.biPlanes=1; b.h.biBitCount=16; b.h.biCompression=BI_BITFIELDS; b.m[0]=0xF800; b.m[1]=0x07E0; b.m[2]=0x001F;
 dib=CreateDIBSection(sdc,(BITMAPINFO*)&b,DIB_RGB_COLORS,&bits,NULL,0);
 if(!dib||!bits) return 0;
 mdc=CreateCompatibleDC(sdc); olddib=(HBITMAP)SelectObject(mdc,dib); tgt=(WORD*)bits;
 return 1;
}
static void shot(int x,int y,int w,int h) {BitBlt(mdc,0,0,w,h,sdc,x,y,SRCCOPY); GdiFlush();}
static void clear_screen(void) {
 RECT rc; HBRUSH hb=CreateSolidBrush(RGB(0,0,0));
 rc.left=0; rc.top=0; rc.right=640; rc.bottom=480; FillRect(sdc,&rc,hb); DeleteObject(hb); GdiFlush();
}
static int hesc(DWORD command,void *out) {
 vxh_command c; c.cb=sizeof(c); c.magic=VXH_MAGIC; c.command=command; c.reserved=0;
 return ExtEscape(sdc,VXH_ESCAPE,sizeof(c),(LPCSTR)&c,out?(int)sizeof(vxh_reply):0,(LPSTR)out);
}

#define SW 21
#define SH 13
static BYTE *sbuf; static vxh_reply rep;
static int r5of(int idx) {return idx&31;}
static int g5of(int idx) {return (idx>>5)&31;}
static int b5of(int idx) {return (idx*7+3)&31;}
static short inv[32*32*32]; /* (r5,g5,b5) -> idx + 1 */
static int build_inv(void) {
 int i,n=SW*SH,bad=0;
 memset(inv,0,sizeof(inv));
 for(i=0;i<n;i++) {int k=(r5of(i)<<10)|(g5of(i)<<5)|b5of(i); if(inv[k]||(!r5of(i)&&!g5of(i)&&!b5of(i))) bad++; inv[k]=(short)(i+1);}
 return !bad;
}
static int decode(WORD o) {return (int)inv[((o>>11)<<10)|(((o>>6)&31)<<5)|(o&31)]-1;} /* the source index, or -1 */
static const char *tname[3]={"555 BI_RGB","24-bit BI_RGB","32-bit BI_RGB"};
/* draw the 21x13 source with colour index y*21+x; returns the DDI capture record (or NULL) in *rec */
static void blit(int type,int topdown,int xs,int ys,int ws,int hs,int dx,int dy,int dw,int dh,vxh_record **rec) {
 BITMAPINFOHEADER bh; int x,y,stride,bytes=type==0?2:type==1?3:4; BYTE *p;
 stride=(SW*bytes+3)&~3;
 for(y=0;y<SH;y++) for(x=0;x<SW;x++) {
  int idx=y*SW+x,r=r5of(idx),g=g5of(idx),b=b5of(idx),row=topdown?y:(SH-1-y);
  p=sbuf+(long)row*stride+x*bytes;
  if(type==0) *(WORD*)p=(WORD)((r<<10)|(g<<5)|b);
  else {p[0]=(BYTE)(b<<3); p[1]=(BYTE)(g<<3); p[2]=(BYTE)(r<<3); if(type==2) p[3]=0;}
 }
 memset(&bh,0,sizeof(bh)); bh.biSize=sizeof(bh); bh.biWidth=SW; bh.biHeight=topdown?-SH:SH; bh.biPlanes=1; bh.biBitCount=(WORD)(bytes*8); bh.biCompression=BI_RGB;
 clear_screen();
 if(rec) {hesc(VXH_ARM_CALLS,NULL);}
 StretchDIBits(sdc,dx,dy,dw,dh,xs,ys,ws,hs,sbuf,(BITMAPINFO*)&bh,DIB_RGB_COLORS,SRCCOPY); GdiFlush();
 if(rec) {
  DWORD i; *rec=NULL; memset(&rep,0,sizeof(rep));
  if(hesc(VXH_READ,&rep)>0&&rep.magic==VXH_MAGIC) {for(i=0;i<rep.count&&i<VXH_CAP;i++) if(rep.records[i].rop==VXH_TAG_SDIB) {*rec=&rep.records[i]; break;}}
  hesc(VXH_STOP,NULL);
 }
}
static void t1(void) {
 int type,k,axis; static int col[200],row[200];
 logline("-- T1 which source column / row each destination pixel takes (21x13 source, whole, bottom-up, destination at 0,0) --\n");
 for(type=0;type<3;type++) for(k=2;k<=4;k++) {
  int i,n=SW*k,m=SH*k,okc=1,okr=1; vxh_record *rec;
  blit(type,0,0,0,SW,SH,0,0,n,m,&rec); shot(0,0,n,m);
  for(i=0;i<n;i++) {int d=decode(tgt[i]); col[i]=d<0?-1:d%SW; if(col[i]!=i/k) okc=0;}
  for(i=0;i<m;i++) {int d=decode(tgt[i*640]); row[i]=d<0?-1:d/SW; if(row[i]!=i/k) okr=0;}
  (void)axis;
  logline("    %-14s %dx: columns %s", tname[type],k,okc?"floor(i/k)":"differ:");
  if(!okc) for(i=0;i<3*k+3&&i<n;i++) logline(" %d",col[i]);
  logline("; rows %s",okr?"floor(j/k)":"differ:");
  if(!okr) for(i=0;i<3*k+3&&i<m;i++) logline(" %d",row[i]);
  logline("\n");
  pump();
 }
}
static void t2(void) {
 int k,g,r; static char cls[33];
 logline("-- T2 the green sixth bit of a 555 source, stretched k times (per 5-bit green value: 0, 1 or m = depends on red/blue) --\n");
 for(k=1;k<=4;k++) {
  BITMAPINFOHEADER bh; int x,y,w=32*k,h=32*k; long blockok=0,blocks=0; int cnt0[32],cnt1[32];
  for(y=0;y<32;y++) for(x=0;x<32;x++) ((WORD*)sbuf)[y*32+x]=(WORD)((y<<10)|(x<<5)|((x+y)&31)); /* x = green, y = red */
  memset(&bh,0,sizeof(bh)); bh.biSize=sizeof(bh); bh.biWidth=32; bh.biHeight=-32; bh.biPlanes=1; bh.biBitCount=16; bh.biCompression=BI_RGB;
  clear_screen();
  StretchDIBits(sdc,0,0,w,h,0,0,32,32,sbuf,(BITMAPINFO*)&bh,DIB_RGB_COLORS,SRCCOPY); GdiFlush(); shot(0,0,w,h);
  for(g=0;g<32;g++) {cnt0[g]=0; cnt1[g]=0;}
  for(y=0;y<32;y++) for(x=0;x<32;x++) {
   WORD o=tgt[(y*k)*640+x*k]; int i,j,same=1; if((o>>5)&1) cnt1[x]++; else cnt0[x]++;
   for(j=0;j<k;j++) for(i=0;i<k;i++) if(tgt[(y*k+j)*640+x*k+i]!=o) same=0;
   blocks++; if(same) blockok++;
  }
  for(g=0;g<32;g++) cls[g]=cnt1[g]==0?'0':cnt0[g]==0?'1':'m'; cls[32]=0;
  logline("    %dx: %s   (blocks of one colour: %ld of %ld)\n",k,cls,blockok,blocks);
  (void)r;
  pump();
 }
}
static void ddi(const char *name,vxh_record *rec) {
 WORD w[20]; short *s=(short*)w;
 if(!rec) {logline("      DDI arguments: no StretchDIBits call was recorded\n"); return;}
 memcpy(w,&rec->flags,40);
 logline("      DDI arguments (%s): source x %d y %d width %d height %d, destination width %d height %d, ROP %04X%04X, clip %s, translate %s, bits %04X:%04X, info %04X:%04X\n",name,s[17],s[16],s[15],s[14],s[19],s[18],w[7],w[6],(w[0]||w[1])?"given":"none",(w[8]||w[9])?"given":"none",w[13],w[12],w[11],w[10]);
}
static void t3(void) {
 static const struct {const char *n; int td,xs,ys,ws,hs,dw,dh;} c[]={
  {"bottom-up whole 2x",0,0,0,SW,SH,2*SW,2*SH},
  {"top-down whole 2x",1,0,0,SW,SH,2*SW,2*SH},
  {"bottom-up rectangle 3,2 10x7 2x",0,3,2,10,7,20,14},
  {"top-down rectangle 3,2 10x7 2x",1,3,2,10,7,20,14},
  {"bottom-up rectangle 3,2 10x7 mirrored left-right 2x",0,3,2,10,7,-20,14},
  {"bottom-up rectangle 3,2 10x7 mirrored top-bottom 2x",0,3,2,10,7,20,-14},
  {"top-down rectangle 3,2 10x7 mirrored top-bottom 2x",1,3,2,10,7,20,-14},
  {"bottom-up whole 1:1",0,0,0,SW,SH,SW,SH},
 };
 int i,type;
 logline("-- T3 source rectangle and the DDI arguments (destination at 100,60) --\n");
 for(type=1;type<2;type++) for(i=0;i<(int)(sizeof(c)/sizeof(c[0]));i++) {
  vxh_record *rec; int dw=c[i].dw,dh=c[i].dh,adw=dw<0?-dw:dw,adh=dh<0?-dh:dh,x,y,found=0,tx=0,ty=0;
  blit(type,c[i].td,c[i].xs,c[i].ys,c[i].ws,c[i].hs,100,60,dw,dh,&rec); shot(60,30,adw+80,adh+60);
  for(y=0;y<adh+60&&!found;y++) for(x=0;x<adw+80&&!found;x++) {int d=decode(tgt[y*640+x]); if(d>=0) {found=1; tx=x+60; ty=y+30; (void)d;}}
  if(found) {int d=decode(tgt[(ty-30)*640+(tx-60)]); logline("    %s (24-bit source): first pixel found at %d,%d is source column %d row %d (top-down index)\n",c[i].n,tx,ty,d%SW,d/SW);}
  else logline("    %s: nothing drawn\n",c[i].n);
  ddi(c[i].n,rec);
  pump();
 }
 logline("-- same calls with the 555 and 32-bit sources, DDI arguments only for the rectangle cases --\n");
 for(type=0;type<3;type+=2) for(i=2;i<4;i++) {
  vxh_record *rec;
  blit(type,c[i].td,c[i].xs,c[i].ys,c[i].ws,c[i].hs,100,60,c[i].dw,c[i].dh,&rec);
  logline("    %s, %s\n",c[i].n,tname[type]); ddi(c[i].n,rec);
 }
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 vx_status_t s; DWORD got=0,t; int rc=0,have_orig; DEVMODEA orig; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2SDI2.TXT","w"); logline("VX2SDI2 v1.0\n");
 finish("RUNNING: the run did not reach its end (VX2SDI2.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 sbuf=(BYTE*)calloc(640UL*480UL*4UL,1);
 if(!sbuf) {finish("FAIL: out of memory"); return 1;}
 if(!build_inv()) {logline("internal: the test colours are not all different\n"); finish("FAIL: internal"); return 1;}
 memset(&orig,0,sizeof(orig)); orig.dmSize=sizeof(orig); have_orig=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&orig)?1:0;
 logline("desktop was %lux%lux%lu\n",have_orig?orig.dmPelsWidth:0,have_orig?orig.dmPelsHeight:0,have_orig?orig.dmBitsPerPel:0);
 if(!set_mode(640,480,16)) {finish("FAIL: 640x480x16 not available"); rc=1;}
 else {
  t=GetTickCount(); while(GetTickCount()-t<800) Sleep(20);
  memset(&s,0,sizeof(s));
  if(!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||s.width!=640||s.height!=480||s.bpp!=16||s.stage!=6||s.failure||s.vga_mode) {
   logline("the driver is not in native 640x480x16 mode\n"); finish("FAIL: not native mode"); rc=1;
  } else {
   sdc=GetDC(NULL);
   if(!sdc||!make_target()) {logline("no screen DC or no 565 DIB section\n"); finish("FAIL: no DIB section"); rc=1;}
   else {
    t1(); if(pump()) t2(); if(pump()) t3();
    SelectObject(mdc,olddib); DeleteObject(dib); DeleteDC(mdc); ReleaseDC(NULL,sdc);
   }
  }
 }
 if(aborted) {finish("ABORTED"); logline("aborted\n");}
 else if(!rc) finish("DONE: see VX2SDI2.TXT");
 if(have_orig) {ChangeDisplaySettingsA(&orig,0); logline("desktop put back\n");}
 logline("Finished.\n"); if(lf) fclose(lf);
 return rc;
}
