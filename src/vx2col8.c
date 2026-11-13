/* VX2COL8 v1.1 (v1.1: v1.0 showed every single fill right at heights 8 and 3, but its rows only started on even Y phases; the shapes still differed with the guard off): driver v10.136.
 * At 640x480x8, guard and size thresholds off: PatBlt with an 8x8 colour pattern, the dithered green and a hatched brush, PATINVERT at heights 1, 2 and 3 and PATCOPY at height 1, widths
 * 1 to 16 at every X phase and every Y phase, compared cell by cell with DIBENG (2-pixel margin); only differing cells are listed. Then XOR shapes (band batches) in the four modes of
 * ACCEL_COL8_GUARD: 0 guard off, 1 guard (DIBENG), 2 the pattern tile written again before every rectangle, 3 tile and register prologue again before every rectangle.
 * Nothing is written to the registry. VX2COL8.TXT / .RES. Esc stops. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <io.h>
#include "vx85598.h"

static FILE *lf; static int aborted; static HANDLE dev=INVALID_HANDLE_VALUE;
static void logline(const char *fmt,...) {va_list a; if(!lf) return; va_start(a,fmt); vfprintf(lf,fmt,a); va_end(a); fflush(lf); _commit(_fileno(lf));}
static void finish(const char *res) {FILE *f=fopen("VX2COL8.RES","w"); if(f) {fprintf(f,"%s\n",res); fclose(f);}}
static int pump(void) {MSG m; while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} if(GetAsyncKeyState(VK_ESCAPE)&0x8000) aborted=1; return !aborted;}
static int set_mode(DWORD w,DWORD h,DWORD b) {
 DEVMODEA dm; LONG rc; memset(&dm,0,sizeof(dm)); dm.dmSize=sizeof(dm); dm.dmPelsWidth=w; dm.dmPelsHeight=h; dm.dmBitsPerPel=b; dm.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 rc=ChangeDisplaySettingsA(&dm,0); logline("ChangeDisplaySettings(%lux%lux%lu): %ld\n",w,h,b,(long)rc); {DWORD t=GetTickCount(); while(GetTickCount()-t<1000) {pump(); Sleep(20);}}
 return rc==DISP_CHANGE_SUCCESSFUL;
}
static int ctl(DWORD code,DWORD *out) {DWORD o[4],got=0; memset(o,0,sizeof(o)); if(!DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)||got<16) return 0; if(out) memcpy(out,o,16); return 1;}
static DWORD engine_ops(void) {DWORD o[4]; ctl(0xFFFFFFFEUL,o); return o[0];}
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
static void background(void) {
 int y,x; for(y=0;y<AH;y+=8) for(x=0;x<AW;x+=40) {RECT r; HBRUSH b=CreateSolidBrush(RGB((x*7+y*3)&255,(x*2+y*5+80)&255,(x*11+y)&255)); r.left=x; r.top=y; r.right=x+40; r.bottom=y+8; FillRect(sdc,&r,b); DeleteObject(b);}
 GdiFlush();
}
static const int widths[10]={1,2,3,4,5,7,8,9,15,16};
static int cellx(int ph) {return ph*65+8;} /* x & 7 == phase */
static int celly(int row,int yph) {return row*24+8+yph;} /* y & 7 == yph */
static void grid(HBRUSH br,DWORD rop,int hh,int yph) {
 int r,ph; HBRUSH o=(HBRUSH)SelectObject(sdc,br); SetBkColor(sdc,RGB(0,0,0));
 for(r=0;r<10;r++) for(ph=0;ph<8;ph++) PatBlt(sdc,cellx(ph),celly(r,yph),widths[r],hh,rop);
 SelectObject(sdc,o); GdiFlush();
}
static int cell_diff(int r,int ph,int hh,int yph) {
 int x0=cellx(ph)-2,y0=celly(r,yph)-2,x,y,d=0;
 for(y=y0;y<y0+hh+4&&y<AH;y++) for(x=x0;x<x0+widths[r]+4&&x<AW;x++) if((bufA[y*AW+x]^tbits[y*AW+x])&0xFFFFFFUL) d++;
 return d;
}
static int table(const char *name,HBRUSH br,DWORD rop,int hh,int yph) {
 int r,ph,bad=0,n; DWORD e0,e1; char line[1200];
 ctl(2,NULL); background(); grid(br,rop,hh,yph); shot(); memcpy(bufA,tbits,AW*AH*4); ctl(0xFFFFFFFFUL,NULL); ctl(5,NULL); ctl(0xEF000000UL,NULL);
 background(); e0=engine_ops(); grid(br,rop,hh,yph); e1=engine_ops(); shot();
 n=sprintf(line,"  %-34s height %d, Y phase %d: %2lu/80 on the engine;",name,hh,yph,e1-e0);
 for(r=0;r<10;r++) for(ph=0;ph<8;ph++) {int d=cell_diff(r,ph,hh,yph); if(d) {bad++; if(n<1100) n+=sprintf(line+n," w%d/x%d:%d",widths[r],ph,d);}}
 logline("%s%s\n",line,bad?"":" all cells ok");
 ctl(0xFFFFFFFFUL,NULL); return bad;
}
static void shapes_draw(HBRUSH br) {
 POINT star[5]={{150,30},{230,260},{40,110},{260,110},{70,260}}; HPEN p=(HPEN)SelectObject(sdc,GetStockObject(NULL_PEN)); HBRUSH o=(HBRUSH)SelectObject(sdc,br);
 SetROP2(sdc,R2_XORPEN); SetPolyFillMode(sdc,WINDING); Polygon(sdc,star,5); Ellipse(sdc,300,30,580,220); RoundRect(sdc,30,280,280,420,40,40); SetROP2(sdc,R2_COPYPEN); SelectObject(sdc,o); SelectObject(sdc,p); GdiFlush();
}
static int shapes_modes(HBRUSH br,const char *name) {
 static const char *mname[4]={"0 guard off (as v10.134)","1 guard (DIBENG)","2 tile again per rectangle","3 tile + prologue per rectangle"}; int m,i,d,fx=-1,fy=-1,bad=0; DWORD c0[4],c1[4];
 ctl(0xF700000EUL,NULL); background(); shapes_draw(br); shot(); memcpy(bufA,tbits,AW*AH*4); ctl(0xFFFFFFFFUL,NULL);
 for(m=0;m<4;m++) {
  ctl(0xEF000000UL|(DWORD)m,NULL); background(); ctl(0xFFFFFE18UL,c0); shapes_draw(br); ctl(0xFFFFFE18UL,c1); shot();
  d=0; for(i=0;i<AW*AH;i++) if((bufA[i]^tbits[i])&0xFFFFFFUL) {if(!d) {fx=i%AW; fy=i/AW;} d++;}
  logline("  XOR shapes, %-24s mode %-32s %3d differing pixels, %lu bands%s\n",name,mname[m],d,c1[3]-c0[3],"");
  if(d) logline("     first at %d,%d\n",fx,fy);
  if(d&&m!=0) bad++;
 }
 ctl(0xFFFFFFFFUL,NULL); return bad;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 DWORD c[4]; int bad=0,gbad=0,have,bi,ri; DEVMODEA origm; char res[300]; HBRUSH br[3]; HBITMAP pb; static const char *bname[3]={"8x8 colour pattern","dithered green","hatched (diagonal cross)"};
 static const DWORD rops[2]={PATCOPY,PATINVERT}; static const char *rname[2]={"PATCOPY","PATINVERT"}; (void)hi; (void)hp; (void)cmd; (void)show;
 lf=fopen("VX2COL8.TXT","w"); logline("VX2COL8 v1.1 / driver v10.136\n"); finish("RUNNING: the run did not reach its end (VX2COL8.TXT ends where it stopped)");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {logline("cannot open VX85598: Windows error %lu\n",GetLastError()); finish("FAIL: driver not found"); return 1;}
 if(!ctl(0xEF000003UL,c)||c[0]!=3) {finish("FAIL: VX85598.VXD older than v10.136"); return 1;}
 ctl(0xFFFFFFFFUL,NULL);
 bufA=(DWORD*)calloc(AW*AH,4); if(!bufA) {finish("FAIL: out of memory"); return 1;}
 memset(&origm,0,sizeof(origm)); origm.dmSize=sizeof(origm); have=EnumDisplaySettingsA(NULL,ENUM_CURRENT_SETTINGS,&origm)?1:0;
 if(!set_mode(640,480,8)) {finish("FAIL: 640x480x8 not available"); return 1;}
 sdc=GetDC(NULL); if(!sdc||!make_target()) {finish("FAIL: no screen DC"); return 1;}
 {HDC m=CreateCompatibleDC(sdc); HBITMAP o; int y,x; pb=CreateCompatibleBitmap(sdc,8,8); o=(HBITMAP)SelectObject(m,pb);
  for(y=0;y<8;y++) for(x=0;x<8;x++) SetPixel(m,x,y,((x+y)&1)?RGB(255,255,0):((x^y)&2)?RGB(0,0,255):RGB(255,0,255)); SelectObject(m,o); DeleteDC(m); br[0]=CreatePatternBrush(pb);}
 br[1]=CreateSolidBrush(RGB(120,200,40)); br[2]=CreateHatchBrush(HS_DIAGCROSS,RGB(255,255,0));
 {int hh,yph; static const int hs[3]={1,2,3};
  for(bi=0;bi<3&&pump();bi++) for(ri=0;ri<2&&pump();ri++) for(hh=0;hh<3&&pump();hh++) for(yph=0;yph<8&&pump();yph++) {char n[80]; if(ri==0&&hs[hh]!=1) continue; sprintf(n,"%s %s",bname[bi],rname[ri]); bad+=table(n,br[bi],rops[ri],hs[hh],yph);}}
 for(bi=0;bi<3&&pump();bi++) gbad+=shapes_modes(br[bi],bname[bi]);
 DeleteObject(br[0]); DeleteObject(br[1]); DeleteObject(br[2]); DeleteObject(pb); drop_target(); ReleaseDC(NULL,sdc);
 ctl(0xFFFFFFFFUL,NULL);
 if(have) {ChangeDisplaySettingsA(&origm,0); logline("desktop put back\n");}
 sprintf(res,"DONE%s: %d single-fill cells differ (heights 1 to 3, every X and Y phase); %d shape cases differ in modes 1 to 3 (see VX2COL8.TXT)",aborted?" (aborted)":"",bad,gbad);
 finish(res); logline("%s\nFinished.\n",res); if(lf) fclose(lf); CloseHandle(dev); return 0;
}
