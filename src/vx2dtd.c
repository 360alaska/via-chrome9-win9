/* VX2DTD v1.0: what the next 2D fast paths need to know (the driver is not changed; v10.91 or later, because it uses the DDI call capture of VX2PRIM).
 *   Part 1, 640x480x16: SetDIBitsToDevice and 1:1 StretchDIBits, which reach the driver as DibToDevice (24-bit and 32-bit sources, 9 ms in VX2STR).
 *     T1  placement: the 21x13 24-bit source has colours that are all different after conversion; for bottom-up and top-down bitmaps, the whole source, a source rectangle (xSrc, ySrc, w, h),
 *         a partial bitmap (StartScan, cLines with the bits holding only those lines) and a clip region: where the first pixel lands, which source column and row it is, how many pixels and the bounding box,
 *         and the DDI arguments the driver receives (DibToDevice: X, Y, iScan, cScans as signed numbers, whether a clip rectangle, draw mode and translation table are given).
 *     T2  timing, ms per call: SetDIBitsToDevice and 1:1 StretchDIBits of 640x480, 320x240, 64x64 and 16x16 from 555, 565 BITFIELDS, 24-bit and 32-bit sources.
 *   Part 2, 640x480x32 (a 32bpp screen): T3 the colour conversion (all 32768 555 values, all 65536 565 values, 24-bit and 32-bit sources) through SetDIBitsToDevice and StretchDIBits 2x, against the candidate
 *     rules (shift, replicate the top bits, rounding); T4 timing of SetDIBitsToDevice, StretchDIBits 1:1, 2x and 1/2 from each source type.
 * The desktop is switched and put back. Unattended: run it, read VX2DTD.TXT (VX2DTD.RES says RUNNING until it ends). Esc stops. */
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
static void finish(const char *res) {FILE *f=fopen("VX2DTD.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
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
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc;
 memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc);
 return rc==DISP_CHANGE_SUCCESSFUL;
}
static HDC sdc,mdc; static HBITMAP dib,olddib; static BYTE *tgt; static int tbits;
typedef struct {BITMAPINFOHEADER h; DWORD m[3];} bi565;
static int make_target(int bits) {
 bi565 b; void *p=NULL;
 memset(&b,0,sizeof(b)); b.h.biSize=sizeof(BITMAPINFOHEADER); b.h.biWidth=640; b.h.biHeight=-480; b.h.biPlanes=1; b.h.biBitCount=(WORD)bits;
 if(bits==16) {b.h.biCompression=BI_BITFIELDS; b.m[0]=0xF800; b.m[1]=0x07E0; b.m[2]=0x001F;}
 dib=CreateDIBSection(sdc,(BITMAPINFO*)&b,DIB_RGB_COLORS,&p,NULL,0);
 if(!dib||!p) return 0;
 mdc=CreateCompatibleDC(sdc); olddib=(HBITMAP)SelectObject(mdc,dib); tgt=(BYTE*)p; tbits=bits;
 return 1;
}
static void drop_target(void) {SelectObject(mdc,olddib); DeleteObject(dib); DeleteDC(mdc); dib=NULL; mdc=NULL;}
static DWORD px(int x,int y) {return tbits==16?(DWORD)((WORD*)tgt)[y*640+x]:((DWORD*)tgt)[y*640+x]&0xFFFFFFUL;}
static void shot(int x,int y,int w,int h) {BitBlt(mdc,0,0,w,h,sdc,x,y,SRCCOPY); GdiFlush();}
static void clear_screen(void) {
 RECT rc; HBRUSH hb=CreateSolidBrush(RGB(0,0,0));
 rc.left=0; rc.top=0; rc.right=640; rc.bottom=480; FillRect(sdc,&rc,hb); DeleteObject(hb); GdiFlush();
}
static int hesc(DWORD command,void *out) {
 vxh_command c; c.cb=sizeof(c); c.magic=VXH_MAGIC; c.command=command; c.reserved=0;
 return ExtEscape(sdc,VXH_ESCAPE,sizeof(c),(LPCSTR)&c,out?(int)sizeof(vxh_reply):0,(LPSTR)out);
}

/* ---------------- part 1 ---------------- */
#define SW 21
#define SH 13
static BYTE *sbuf; static vxh_reply rep;
static int r5of(int idx) {return idx&31;}
static int g5of(int idx) {return (idx>>5)&31;}
static int b5of(int idx) {return (idx*7+3)&31;}
static short inv[32*32*32];
static int build_inv(void) {
 int i,n=SW*SH,bad=0;
 memset(inv,0,sizeof(inv));
 for(i=0;i<n;i++) {int k=(r5of(i)<<10)|(g5of(i)<<5)|b5of(i); if(inv[k]||(!r5of(i)&&!g5of(i)&&!b5of(i))) bad++; inv[k]=(short)(i+1);}
 return !bad;
}
static int decode(DWORD o) {return (int)inv[((o>>11)<<10)|(((o>>6)&31)<<5)|(o&31)]-1;}
/* fill sbuf with the 24-bit source (colour index y*21+x of the picture row y); only the lines first..first+count-1 (counted from the bottom for a bottom-up bitmap)
 * are stored, starting at the buffer's beginning (a partial bitmap); first=0,count=SH stores everything */
static void fill24(int topdown,int first,int count) {
 int x,y,stride=(SW*3+3)&~3,line;
 for(line=0;line<count;line++) {
  int stored=first+line; /* the line as the DIB counts it */
  y=topdown?stored:(SH-1-stored);
  for(x=0;x<SW;x++) {int idx=y*SW+x; BYTE *p=sbuf+(long)line*stride+x*3; p[0]=(BYTE)(b5of(idx)<<3); p[1]=(BYTE)(g5of(idx)<<3); p[2]=(BYTE)(r5of(idx)<<3);}
 }
}
typedef struct {const char *n; int api,topdown,xs,ys,ws,hs,start,lines,clip;} cs_t;
/* api 0 SetDIBitsToDevice, 1 StretchDIBits 1:1 */
static void ddi(vxh_record *rec) {
 WORD w[20]; short *s=(short*)w;
 if(!rec) {logline("      DDI: no DibToDevice call was recorded\n"); return;}
 memcpy(w,&rec->flags,40);
 logline("      DDI arguments: X %d Y %d iScan %d cScans %d, clip %s, draw mode %s, translate %s, bits %04X:%04X, info %04X:%04X\n",s[13],s[12],s[11],s[10],(w[8]||w[9])?"given":"none",(w[6]||w[7])?"given":"none",(w[0]||w[1])?"given":"none",w[5],w[4],w[3],w[2]);
}
static void runcase(const cs_t *c) {
 BITMAPINFOHEADER bh; vxh_record *rec=NULL; DWORD i; int x,y,found=0,fx=0,fy=0,n=0,minx=9999,miny=9999,maxx=-1,maxy=-1,d,r;
 HRGN rg=NULL;
 memset(&bh,0,sizeof(bh)); bh.biSize=sizeof(bh); bh.biWidth=SW; bh.biHeight=c->topdown?-SH:SH; bh.biPlanes=1; bh.biBitCount=24; bh.biCompression=BI_RGB;
 fill24(c->topdown,c->start,c->lines);
 clear_screen();
 if(c->clip) {rg=CreateRectRgn(104,63,114,69); SelectClipRgn(sdc,rg);}
 hesc(VXH_ARM_CALLS,NULL);
 if(c->api==0) r=SetDIBitsToDevice(sdc,100,60,c->ws,c->hs,c->xs,c->ys,c->start,c->lines,sbuf,(BITMAPINFO*)&bh,DIB_RGB_COLORS);
 else r=StretchDIBits(sdc,100,60,c->ws,c->hs,c->xs,c->ys,c->ws,c->hs,sbuf,(BITMAPINFO*)&bh,DIB_RGB_COLORS,SRCCOPY);
 GdiFlush();
 memset(&rep,0,sizeof(rep));
 if(hesc(VXH_READ,&rep)>0&&rep.magic==VXH_MAGIC) {for(i=0;i<rep.count&&i<VXH_CAP;i++) if(rep.records[i].rop==VXH_TAG_DTDV) {rec=&rep.records[i]; break;}}
 hesc(VXH_STOP,NULL);
 if(c->clip) {SelectClipRgn(sdc,NULL); DeleteObject(rg);}
 shot(60,30,120,100);
 for(y=0;y<100;y++) for(x=0;x<120;x++) {
  DWORD o=px(x,y);
  if(o) {
   d=decode(o); n++;
   if(x<minx) minx=x; if(y<miny) miny=y; if(x>maxx) maxx=x; if(y>maxy) maxy=y;
   if(!found) {found=1; fx=x; fy=y;}
  }
 }
 logline("    %-58s returned %d; ",c->n,r);
 if(found) {d=decode(px(fx,fy)); logline("%d pixels, box %d,%d..%d,%d, first pixel %d,%d = source column %d row %d (top-down index)\n",n,minx+60,miny+30,maxx+60,maxy+30,fx+60,fy+30,d<0?-1:d%SW,d<0?-1:d/SW);}
 else logline("nothing drawn\n");
 ddi(rec);
}
static void t1(void) {
 static const cs_t c[]={
  {"SetDIBitsToDevice bottom-up whole",0,0,0,0,SW,SH,0,SH,0},
  {"SetDIBitsToDevice top-down whole",0,1,0,0,SW,SH,0,SH,0},
  {"SetDIBitsToDevice bottom-up source rectangle 3,2 10x7",0,0,3,2,10,7,0,SH,0},
  {"SetDIBitsToDevice top-down source rectangle 3,2 10x7",0,1,3,2,10,7,0,SH,0},
  {"SetDIBitsToDevice bottom-up, partial bitmap start 4 lines 5",0,0,0,4,SW,5,4,5,0},
  {"SetDIBitsToDevice top-down, partial bitmap start 4 lines 5",0,1,0,4,SW,5,4,5,0},
  {"SetDIBitsToDevice bottom-up, rectangle 2,5 12x6 of partial 4/8",0,0,2,5,12,6,4,8,0},
  {"SetDIBitsToDevice bottom-up whole, clip region 104,63..114,69",0,0,0,0,SW,SH,0,SH,1},
  {"StretchDIBits 1:1 bottom-up whole",1,0,0,0,SW,SH,0,SH,0},
  {"StretchDIBits 1:1 top-down whole",1,1,0,0,SW,SH,0,SH,0},
  {"StretchDIBits 1:1 bottom-up rectangle 3,2 10x7",1,0,3,2,10,7,0,SH,0},
  {"StretchDIBits 1:1 top-down rectangle 3,2 10x7",1,1,3,2,10,7,0,SH,0},
  {"StretchDIBits 1:1 bottom-up whole, clip region",1,0,0,0,SW,SH,0,SH,1},
 };
 int i;
 logline("-- T1 placement and DDI arguments, 24-bit source %dx%d at 100,60 (16bpp screen) --\n",SW,SH);
 for(i=0;i<(int)(sizeof(c)/sizeof(c[0]))&&!aborted;i++) {runcase(&c[i]); pump();}
}
static BITMAPINFO *tbi;
static double tm(int api,int sw,int sh,int bits,int comp,int n) {
 double t0; int i,stride; DWORD *m; long k,tot;
 memset(tbi,0,sizeof(BITMAPINFOHEADER)+12); tbi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER); tbi->bmiHeader.biWidth=sw; tbi->bmiHeader.biHeight=-sh; tbi->bmiHeader.biPlanes=1; tbi->bmiHeader.biBitCount=(WORD)bits; tbi->bmiHeader.biCompression=comp;
 m=(DWORD*)tbi->bmiColors; if(comp==BI_BITFIELDS) {m[0]=0xF800; m[1]=0x07E0; m[2]=0x001F;}
 stride=(sw*bits/8+3)&~3; tot=(long)stride*sh; for(k=0;k<tot;k++) sbuf[k]=(BYTE)(k*7+(k>>9));
 if(api==0) SetDIBitsToDevice(sdc,0,0,sw,sh,0,0,0,sh,sbuf,tbi,DIB_RGB_COLORS); else StretchDIBits(sdc,0,0,sw,sh,0,0,sw,sh,sbuf,tbi,DIB_RGB_COLORS,SRCCOPY);
 GdiFlush(); t0=now();
 for(i=0;i<n;i++) {if(api==0) SetDIBitsToDevice(sdc,0,0,sw,sh,0,0,0,sh,sbuf,tbi,DIB_RGB_COLORS); else StretchDIBits(sdc,0,0,sw,sh,0,0,sw,sh,sbuf,tbi,DIB_RGB_COLORS,SRCCOPY);}
 GdiFlush(); return (now()-t0)/n;
}
static void t2(void) {
 static const struct {const char *n; int bits,comp;} src[4]={{"555 BI_RGB",16,BI_RGB},{"565 BITFIELDS",16,BI_BITFIELDS},{"24-bit",24,BI_RGB},{"32-bit",32,BI_RGB}};
 static const struct {int w,h,n;} sz[4]={{640,480,10},{320,240,20},{64,64,200},{16,16,500}};
 int i,j;
 logline("-- T2 timing, ms per call (16bpp screen), SetDIBitsToDevice / StretchDIBits 1:1 --\n");
 for(i=0;i<4&&!aborted;i++) {
  logline("    %-14s",src[i].n);
  for(j=0;j<4;j++) logline("  %dx%d: %.3f / %.3f",sz[j].w,sz[j].h,tm(0,sz[j].w,sz[j].h,src[i].bits,src[i].comp,sz[j].n),tm(1,sz[j].w,sz[j].h,src[i].bits,src[i].comp,sz[j].n));
  logline("\n"); pump();
 }
}

/* ---------------- part 2: 32bpp screen ---------------- */
static int ch5(int v,int rule) {return rule==0?(v<<3)|(v>>2):rule==1?(v<<3):(v*255+15)/31;}
static int ch6(int v,int rule) {return rule==0?(v<<2)|(v>>4):rule==1?(v<<2):(v*255+31)/63;}
static const char *rname[3]={"replicate top bits","shift only","rounding"};
/* source: 256x128 values y*256+x; mode 0 555 SetDIBitsToDevice, 1 555 StretchDIBits 2x, 2 565 SetDIBitsToDevice, 3 565 StretchDIBits 2x */
static void conv16(const char *what,int is565,int stretch) {
 bi565 b; int x,y,k=stretch?2:1,rr; long tot=0,cnt[3][3],other=0,ex=0; int ch,ru;
 memset(&b,0,sizeof(b)); b.h.biSize=sizeof(BITMAPINFOHEADER); b.h.biWidth=256; b.h.biHeight=-128; b.h.biPlanes=1; b.h.biBitCount=16;
 if(is565) {b.h.biCompression=BI_BITFIELDS; b.m[0]=0xF800; b.m[1]=0x07E0; b.m[2]=0x001F;} else b.h.biCompression=BI_RGB;
 for(y=0;y<128;y++) for(x=0;x<256;x++) ((WORD*)sbuf)[y*256+x]=(WORD)(y*256+x);
 clear_screen();
 if(stretch) StretchDIBits(sdc,0,0,256*k,128*k,0,0,256,128,sbuf,(BITMAPINFO*)&b,DIB_RGB_COLORS,SRCCOPY); else SetDIBitsToDevice(sdc,0,0,256,128,0,0,0,128,sbuf,(BITMAPINFO*)&b,DIB_RGB_COLORS);
 GdiFlush(); shot(0,0,256*k,128*k);
 for(ch=0;ch<3;ch++) for(ru=0;ru<3;ru++) cnt[ch][ru]=0;
 for(y=0;y<128;y++) for(x=0;x<256;x++) {
  WORD v=(WORD)(y*256+x); DWORD o=px(x*k,y*k); int rv,gv,bv,ro=(int)((o>>16)&255),go=(int)((o>>8)&255),bo=(int)(o&255),ok=1;
  if(is565) {rv=v>>11; gv=(v>>5)&63; bv=v&31;} else {rv=(v>>10)&31; gv=(v>>5)&31; bv=v&31;}
  tot++;
  for(rr=0;rr<3;rr++) {
   if(ro==ch5(rv,rr)) cnt[0][rr]++;
   if(go==(is565?ch6(gv,rr):ch5(gv,rr))) cnt[1][rr]++;
   if(bo==ch5(bv,rr)) cnt[2][rr]++;
  }
  if(ro!=ch5(rv,0)&&ro!=ch5(rv,1)&&ro!=ch5(rv,2)) ok=0;
  if(go!=(is565?ch6(gv,0):ch5(gv,0))&&go!=(is565?ch6(gv,1):ch5(gv,1))&&go!=(is565?ch6(gv,2):ch5(gv,2))) ok=0;
  if(bo!=ch5(bv,0)&&bo!=ch5(bv,1)&&bo!=ch5(bv,2)) ok=0;
  if(!ok) {other++; if(ex<3) {logline("      source %04X came out %06lX\n",(unsigned)v,(unsigned long)o); ex++;}}
 }
 logline("    %-40s of %ld values:",what,tot);
 for(ch=0;ch<3;ch++) {logline("  %s:",ch==0?"red":ch==1?"green":"blue"); for(ru=0;ru<3;ru++) logline(" %s %ld,",rname[ru],cnt[ch][ru]);}
 logline("  no rule fits %ld\n",other);
}
static void conv24(int bits,int stretch) {
 BITMAPINFOHEADER bh; int x,y,k=stretch?2:1,stride=(64*bits/8+3)&~3; long same=0,total=0,ex=0; DWORD seed=777;
 memset(&bh,0,sizeof(bh)); bh.biSize=sizeof(bh); bh.biWidth=64; bh.biHeight=-64; bh.biPlanes=1; bh.biBitCount=(WORD)bits; bh.biCompression=BI_RGB;
 for(y=0;y<64;y++) for(x=0;x<64;x++) {
  BYTE *p=sbuf+y*stride+x*(bits/8); BYTE r,g,b;
  seed=seed*1103515245UL+12345UL; r=(BYTE)(seed>>16); seed=seed*1103515245UL+12345UL; g=(BYTE)(seed>>16); seed=seed*1103515245UL+12345UL; b=(BYTE)(seed>>16);
  p[0]=b; p[1]=g; p[2]=r; if(bits==32) p[3]=0xA5;
 }
 clear_screen();
 if(stretch) StretchDIBits(sdc,0,0,64*k,64*k,0,0,64,64,sbuf,(BITMAPINFO*)&bh,DIB_RGB_COLORS,SRCCOPY); else SetDIBitsToDevice(sdc,0,0,64,64,0,0,0,64,sbuf,(BITMAPINFO*)&bh,DIB_RGB_COLORS);
 GdiFlush(); shot(0,0,64*k,64*k);
 for(y=0;y<64;y++) for(x=0;x<64;x++) {
  BYTE *p=sbuf+y*stride+x*(bits/8); DWORD want=((DWORD)p[2]<<16)|((DWORD)p[1]<<8)|p[0],o=px(x*k,y*k); total++;
  if(o==want) same++; else if(ex<3) {logline("      source %06lX came out %06lX\n",(unsigned long)want,(unsigned long)o); ex++;}
 }
 logline("    %d-bit BI_RGB source, %s: %ld of %ld colours unchanged (alpha byte of the source is 0xA5 for 32-bit; the top byte of the result is not examined)\n",bits,stretch?"StretchDIBits 2x":"SetDIBitsToDevice",same,total);
}
static void t3(void) {
 logline("-- T3 colour conversion into the 32bpp screen --\n");
 conv16("555 BI_RGB, SetDIBitsToDevice",0,0);
 conv16("555 BI_RGB, StretchDIBits 2x",0,1);
 conv16("565 BITFIELDS, SetDIBitsToDevice",1,0);
 conv16("565 BITFIELDS, StretchDIBits 2x",1,1);
 conv24(24,0); conv24(24,1); conv24(32,0); conv24(32,1);
}
static double tmst(int api,int sw,int sh,int bits,int comp,int dw,int dh) {
 double t0; int i,stride; DWORD *m; long k,tot;
 memset(tbi,0,sizeof(BITMAPINFOHEADER)+12); tbi->bmiHeader.biSize=sizeof(BITMAPINFOHEADER); tbi->bmiHeader.biWidth=sw; tbi->bmiHeader.biHeight=-sh; tbi->bmiHeader.biPlanes=1; tbi->bmiHeader.biBitCount=(WORD)bits; tbi->bmiHeader.biCompression=comp;
 m=(DWORD*)tbi->bmiColors; if(comp==BI_BITFIELDS) {m[0]=0xF800; m[1]=0x07E0; m[2]=0x001F;}
 stride=(sw*bits/8+3)&~3; tot=(long)stride*sh; for(k=0;k<tot;k++) sbuf[k]=(BYTE)(k*7+(k>>9));
 if(api==0) SetDIBitsToDevice(sdc,0,0,sw,sh,0,0,0,sh,sbuf,tbi,DIB_RGB_COLORS); else StretchDIBits(sdc,0,0,dw,dh,0,0,sw,sh,sbuf,tbi,DIB_RGB_COLORS,SRCCOPY);
 GdiFlush(); t0=now();
 for(i=0;i<10;i++) {if(api==0) SetDIBitsToDevice(sdc,0,0,sw,sh,0,0,0,sh,sbuf,tbi,DIB_RGB_COLORS); else StretchDIBits(sdc,0,0,dw,dh,0,0,sw,sh,sbuf,tbi,DIB_RGB_COLORS,SRCCOPY);}
 GdiFlush(); return (now()-t0)/10.0;
}
static void t4(void) {
 static const struct {const char *n; int bits,comp;} src[4]={{"555 BI_RGB",16,BI_RGB},{"565 BITFIELDS",16,BI_BITFIELDS},{"24-bit",24,BI_RGB},{"32-bit",32,BI_RGB}};
 int i;
 logline("-- T4 timing into the 640x480 32bpp screen, ms per call: SetDIBitsToDevice 640x480 / StretchDIBits 1:1 / 2x (from 320x240) / 3x (from 214x160) / 1/2 (from 640x480 to 320x240) --\n");
 for(i=0;i<4&&!aborted;i++) {
  logline("    %-14s %.2f / %.2f / %.2f / %.2f / %.2f\n",src[i].n,tmst(0,640,480,src[i].bits,src[i].comp,640,480),tmst(1,640,480,src[i].bits,src[i].comp,640,480),tmst(1,320,240,src[i].bits,src[i].comp,640,480),
   tmst(1,214,160,src[i].bits,src[i].comp,642,480),tmst(1,640,480,src[i].bits,src[i].comp,320,240));
  pump();
 }
}
static int mode_ok(int bpp) {
 vx_status_t s; DWORD got=0,t=GetTickCount(); while(GetTickCount()-t<800) Sleep(20);
 memset(&s,0,sizeof(s));
 if(!DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||s.width!=640||s.height!=480||(int)s.bpp!=bpp||s.stage!=6||s.failure||s.vga_mode) {logline("the driver is not in native 640x480x%d mode\n",bpp); return 0;}
 return 1;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 int rc=0,have_orig; DEVMODEA orig; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2DTD.TXT","w"); logline("VX2DTD v1.0\n");
 finish("RUNNING: the run did not reach its end (VX2DTD.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 sbuf=(BYTE*)calloc(640UL*480UL*4UL,1); tbi=(BITMAPINFO*)calloc(sizeof(BITMAPINFOHEADER)+16,1);
 if(!sbuf||!tbi) {finish("FAIL: out of memory"); return 1;}
 if(!build_inv()) {logline("internal: the test colours are not all different\n"); finish("FAIL: internal"); return 1;}
 memset(&orig,0,sizeof(orig)); orig.dmSize=sizeof(orig); have_orig=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&orig)?1:0;
 logline("desktop was %lux%lux%lu\n",have_orig?orig.dmPelsWidth:0,have_orig?orig.dmPelsHeight:0,have_orig?orig.dmBitsPerPel:0);
 if(!set_mode(640,480,16)||!mode_ok(16)) {finish("FAIL: 640x480x16 not available"); rc=1;}
 else {
  sdc=GetDC(NULL);
  if(!sdc||!make_target(16)) {logline("no screen DC or no 565 DIB section\n"); finish("FAIL: no DIB section"); rc=1;}
  else {t1(); if(pump()) t2(); drop_target(); ReleaseDC(NULL,sdc);}
 }
 if(!rc&&!aborted) {
  if(!set_mode(640,480,32)||!mode_ok(32)) logline("640x480x32 not available: part 2 skipped\n");
  else {
   sdc=GetDC(NULL);
   if(!sdc||!make_target(32)) logline("no 32-bit DIB section: part 2 skipped\n");
   else {t3(); if(pump()) t4(); drop_target(); ReleaseDC(NULL,sdc);}
  }
 }
 if(aborted) {finish("ABORTED"); logline("aborted\n");}
 else if(!rc) finish("DONE: see VX2DTD.TXT");
 if(have_orig) {ChangeDisplaySettingsA(&orig,0); logline("desktop put back\n");}
 logline("Finished.\n"); if(lf) fclose(lf);
 return rc;
}
