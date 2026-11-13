/* Win98 VX855 2D lab. Copyright (c) 2026, MIT.
 * Sends one operator-defined 2D engine operation to the VxD, shows the
 * before/after picture, classifies the result (copy, rotation, mirror...)
 * and logs everything to VX2DLAB.TXT. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include "vx85598.h"
#include "vx855_3d.h"

#define SCALE 3
#define IMG_X 8
#define IMG_Y 200
#define ID_RUN 201
#define ID_SWEEP 202
#define ID_SWEEP2 205
#define ID_REACH 206
#define ID_ROUTE 207
#define ID_DVI 208
#define ID_TUNE 209
#define ID_PRESET 203
#define ID_APPLY 204
#define ID_VERD0 220
#define ID_HOST 230
#define ID_INCR 231
#define NVERD 7
#define NPRESET 25

static HANDLE device=INVALID_HANDLE_VALUE,logfile=INVALID_HANDLE_VALUE;
static vx3d_report reply;
static vx3d_request request;
static DWORD before_img[4096],after_img[4096];
static int have_image;
static char status_text[2048]="Starting...";
static char dirpath[MAX_PATH];
static unsigned long recover_flag;
static int can_run;
static HWND hxf,hrt,hcpu,hroff,hcmd,hmode,hw,hh,hsx,hsy,hdx,hdy,hfg,hbg,hregs,hdata,hhost,hincr,hpreset,hmain;
static HFONT font;

static const char *verd_names[NVERD]={"Rot 90","Rot 180","Rot 270","Mirror","Plain copy","Nothing","Garbage"};
static const char *verd_text[NVERD]={"rotated 90 clockwise","rotated 180","rotated 270 clockwise (90 counter-clockwise)","mirrored","plain copy","nothing changed","garbage"};

typedef struct preset {const char *name; const char *cmd,*mode,*w,*h,*sx,*sy,*dx,*dy,*regs; unsigned int flags; const char *data;} preset;
static const preset presets[NPRESET]={
 {"Plain SRCCOPY (sanity)","CC000001","300","16","16","4","4","36","36","",0},
 {"Source colour key copy (key = F background)","CC000001","300","16","16","4","4","36","36","50=FF203060\r\n48=4000",0},
 {"Solid fill F0002001","F0002001","300","16","16","4","4","36","36","",0},
 {"CBU rotate window 90 (CPU writes)","CC000001","300","16","16","4","4","36","36","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n1E00=0000000D",12},
 {"CBU rotate window 180 (CPU writes)","CC000001","300","16","16","4","4","36","36","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n1E00=00000015",12},
 {"CBU rotate window 270 (CPU writes)","CC000001","300","16","16","4","4","36","36","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n1E00=0000001D",12},
 {"2D engine into rotate window 90 (SRCCOPY to scratch)","CC000001","300","16","16","4","4","0","0","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n08=00200140\r\n14=00040200\r\n10=00000000\r\n1E00=0000000D",20},
 {"2D engine into rotate window 180 (SRCCOPY to scratch)","CC000001","300","16","16","4","4","0","0","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n08=00200140\r\n14=00040200\r\n10=00000000\r\n1E00=00000015",20},
 {"2D engine into rotate window 270 (SRCCOPY to scratch)","CC000001","300","16","16","4","4","0","0","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n08=00200140\r\n14=00040200\r\n10=00000000\r\n1E00=0000001D",20},
 {"CPU READ through rotate window 90 (picture first)","CC000001","300","16","16","4","4","36","36","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n1E00=0000000D",44},
 {"2D engine READS rotate window 90 (scratch to desktop)","CC000001","300","16","16","4","4","36","36","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n08=01400020\r\n1C=00040200\r\n18=00040004\r\n1E00=0000000D",84},
 {"2D engine READS rotate window 180 (scratch to desktop)","CC000001","300","16","16","4","4","36","36","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n08=01400020\r\n1C=00040200\r\n18=00040004\r\n1E00=00000015",84},
 {"2D engine READS rotate window 270 (scratch to desktop)","CC000001","300","16","16","4","4","36","36","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n08=01400020\r\n1C=00040200\r\n18=00040004\r\n1E00=0000001D",84},
 {"GPU rotate 90 (cmd 9, mode 301, desktop)","CC000009","301","16","16","4","4","36","36","",0},
 {"GPU rotate 180 (cmd 9, mode 302, desktop)","CC000009","302","16","16","4","4","36","36","",0},
 {"GPU rotate 270 (cmd 9, mode 303, desktop)","CC000009","303","16","16","4","4","36","36","",0},
 {"GPU rotate 90 (cmd 9, mode 301) with CBU window, to scratch","CC000009","301","16","16","4","4","0","0","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n08=00200140\r\n14=00040200\r\n10=00000000\r\n1E00=0000000D",20},
 {"GPU rotate 180 (cmd 9, mode 302) with CBU window, to scratch","CC000009","302","16","16","4","4","0","0","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n08=00200140\r\n14=00040200\r\n10=00000000\r\n1E00=00000015",20},
 {"GPU rotate 270 (cmd 9, mode 303) with CBU window, to scratch","CC000009","303","16","16","4","4","0","0","1E04=00201000\r\n1E08=00205000\r\n1E0C=00010000\r\n1E10=00400040\r\n1E14=00400040\r\n08=00200140\r\n14=00040200\r\n10=00000000\r\n1E00=0000001D",20}
,
 {"XP 180 screen window ON + fill 8x8 at FLIPPED coords (expect square in window if engine writes are remapped)","F0002001","300","8","8","60","60","4","4","1E04=00000000\r\n1E08=0012C000\r\n1E0C=00001999\r\n1E10=02800280\r\n1E14=01E00280\r\n1E00=00000015\r\n10=009400B4",4},
 {"XP 180 screen window ON + fill 8x8 at NORMAL coords (square appears only if window does NOT remap)","F0002001","300","8","8","60","60","4","4","1E04=00000000\r\n1E08=0012C000\r\n1E0C=00001999\r\n1E10=02800280\r\n1E14=01E00280\r\n1E00=00000015",4},
 {"XP 180 window ON + plain screen copy 16x16 (src/dst given flipped; F should appear unrotated at 36,36 if both remapped)","CC000001","300","16","16","4","4","36","36","1E04=00000000\r\n1E08=0012C000\r\n1E0C=00001999\r\n1E10=02800280\r\n1E14=01E00280\r\n1E00=00000015\r\n10=006C008C\r\n18=008C00AC",4},
 {"XP 180 window ON + host colour rotate blit (CC00E049, mode 302) start corner FLIPPED","CC00E049","302","8","8","60","60","4","4","1E04=00000000\r\n1E08=0012C000\r\n1E0C=00001999\r\n1E10=02800280\r\n1E14=01E00280\r\n1E00=00000015\r\n40=00000000\r\n44=01DF027F\r\n10=009B00BB",5,"FFFF0000 FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FF00FF00 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF0000FF FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FFFFFF00"},
 {"XP 180 window ON + host colour rotate blit (CC00E049, mode 302) start corner NORMAL (bottom right of target)","CC00E049","302","8","8","60","60","11","11","1E04=00000000\r\n1E08=0012C000\r\n1E0C=00001999\r\n1E10=02800280\r\n1E14=01E00280\r\n1E00=00000015\r\n40=00000000\r\n44=01DF027F",5,"FFFF0000 FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FFFFFFFF FF00FF00 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FF0000FF FF303030 FF303030 FF303030 FF303030 FF303030 FF303030 FFFFFF00"},
 {"XP 180 window ON + host mono text rotate (CC02D54A, mode 302) start corner FLIPPED","CC02D54A","302","8","8","60","60","4","4","1E04=00000000\r\n1E08=0012C000\r\n1E0C=00001999\r\n1E10=02800280\r\n1E14=01E00280\r\n1E00=00000015\r\n40=00000000\r\n44=01DF027F\r\n10=009B00BB",5,"F88080FE 00808080"}
};

static char lbuf[60000]; static unsigned lbn; static int lbon;
static void logtext(const char *s) {
 DWORD n;
 if(lbon) {unsigned l=(unsigned)strlen(s); if(lbn+l<sizeof(lbuf)-1) {memcpy(lbuf+lbn,s,l); lbn+=l; lbuf[lbn]=0;} return;}
 if(logfile==INVALID_HANDLE_VALUE) return;
 WriteFile(logfile,s,(DWORD)strlen(s),&n,NULL); FlushFileBuffers(logfile);
}
static void lb_begin(void) {lbn=0; lbuf[0]=0; lbon=1;}
static void lb_end(void) {lbon=0; if(lbn) logtext(lbuf); lbn=0;}
static void logf2(const char *format,...) {
 char buf[1024]; va_list a;
 va_start(a,format); _vsnprintf(buf,sizeof(buf)-1,format,a); va_end(a); buf[sizeof(buf)-1]=0;
 logtext(buf);
}
static unsigned long gethex(HWND h) {char b[64]; GetWindowText(h,b,sizeof(b)); return strtoul(b,NULL,16);}
static unsigned long getdec(HWND h) {char b[64]; GetWindowText(h,b,sizeof(b)); return strtoul(b,NULL,10);}
static void sethex(HWND h,const char *s) {SetWindowText(h,s);}

static int parse_regs(void) {
 char b[1024],*p; unsigned int n=0;
 GetWindowText(hregs,b,sizeof(b)); p=b;
 while(*p && n<VX3D_LAB_REGS) {
  while(*p && !((*p>='0'&&*p<='9')||(*p>='A'&&*p<='F')||(*p>='a'&&*p<='f'))) p++;
  if(!*p) break;
  request.lab_reg[n][0]=strtoul(p,&p,16);
  while(*p==' '||*p=='=') p++;
  request.lab_reg[n][1]=strtoul(p,&p,16);
  n++;
 }
 request.lab_nregs=n; return (int)n;
}
static void parse_data(void) {
 char b[2048],*p; unsigned int n=0;
 GetWindowText(hdata,b,sizeof(b)); p=b;
 while(*p && n<VX3D_LAB_DATA) {
  char *e;
  while(*p && !((*p>='0'&&*p<='9')||(*p>='A'&&*p<='F')||(*p>='a'&&*p<='f'))) p++;
  if(!*p) break;
  request.lab_data[n++]=strtoul(p,&e,16); p=e;
 }
 request.lab_ndata=n;
}

/* ---- classification ---- */
static const char *tname[8]={"unrotated copy","rotated 90 clockwise","rotated 180","rotated 270 clockwise","mirrored left-right","mirrored top-bottom","transposed (mirror on the main diagonal)","anti-transposed (mirror on the other diagonal)"};
static DWORD blk[8][4096];
static void make_transforms(unsigned int w,unsigned int h,int sx,int sy,int *tw,int *th) {
 unsigned int x,y,t;
 for(y=0;y<h;y++) for(x=0;x<w;x++) {
  DWORD v=before_img[(sy+y)*64+(sx+x)];
  blk[0][y*w+x]=v;
  blk[1][x*h+(h-1-y)]=v;
  blk[2][(h-1-y)*w+(w-1-x)]=v;
  blk[3][(w-1-x)*h+y]=v;
  blk[4][y*w+(w-1-x)]=v;
  blk[5][(h-1-y)*w+x]=v;
  blk[6][x*h+y]=v;
  blk[7][(w-1-x)*h+(h-1-y)]=v;
 }
 for(t=0;t<8;t++) {tw[t]=(t==1||t==3||t==6||t==7)?(int)h:(int)w; th[t]=(t==1||t==3||t==6||t==7)?(int)w:(int)h;}
}
static void classify(char *out,size_t cap) {
 unsigned int w=(unsigned int)request.lab_w,h=(unsigned int)request.lab_h;
 int sx=(int)request.lab_sx,sy=(int)request.lab_sy,tw[8],th[8],t,px,py,x,y,found=0,bestt=-1,bestx=0,besty=0,best=0;
 char line[160]; out[0]=0;
 if(w<1||h<1||sx<0||sy<0||sx+(int)w>64||sy+(int)h>64||w>40||h>40) {strncpy(out,"(source block outside the 64x64 window; no automatic check)",cap-1); out[cap-1]=0; return;}
 make_transforms(w,h,sx,sy,tw,th);
 for(t=0;t<8;t++) for(py=0;py+th[t]<=64;py++) for(px=0;px+tw[t]<=64;px++) {
  int same=0,total=tw[t]*th[t],ok=1;
  if(t==0 && px==sx && py==sy) continue; /* the untouched source itself */
  for(y=0;y<th[t] && ok;y++) for(x=0;x<tw[t];x++) {
   if(after_img[(py+y)*64+px+x]==blk[t][y*tw[t]+x]) same++;
   else if(same+(total-(y*tw[t]+x))<best) {ok=0;break;}
  }
  if(same>best) {best=same;bestt=t;bestx=px;besty=py;}
  if(same==total) {
   sprintf(line,"EXACT: %s at (%d,%d)\r\n",tname[t],px,py);
   if(strlen(out)+strlen(line)<cap) strcat(out,line); found++;
  }
 }
 if(!found) {
  if(bestt>=0 && best*2>=(int)(w*h)) sprintf(line,"No exact match. Closest: %s at (%d,%d), %d of %u pixels equal.",tname[bestt],bestx,besty,best,w*h);
  else strcpy(line,"No orientation of the source block appears in the result.");
  strncat(out,line,cap-strlen(out)-1);
 }
}

/* ---- run one operation ---- */
static int call_lab(void) {
 DWORD got=0;
 request.cb=sizeof(request); request.version=VX3D_VERSION; request.arm=VX3D_ARM;
 request.reserved=21|(recover_flag?0x100UL:0UL); request.probe=VX3D_SF_PROBE;
 memset(&reply,0,sizeof(reply));
 if(!DeviceIoControl(device,VX_IOCTL_3D_TEST,&request,sizeof(request),&reply,sizeof(reply),&got,NULL)) {
  sprintf(status_text,"Driver request failed: Windows error %lu.",GetLastError()); return 0;
 }
 if(got!=sizeof(reply)||reply.magic!=VX3D_MAGIC||reply.cb!=sizeof(reply)||reply.version!=VX3D_VERSION||reply.build!=VX3D_BUILD||reply.producer!=VX3D_PRODUCER) {
  strcpy(status_text,"The driver reply does not match this program. Copy VX85598.VXD and VX2DLAB.EXE from the same package and restart Windows."); return 0;
 }
 recover_flag=(reply.fault_latched||reply.result==VX3D_DISABLED)?1:0;
 return 1;
}
static void collect(void) {
 memcpy(before_img,reply.sf_x_raw,sizeof(before_img));
 memcpy(after_img,reply.image,sizeof(after_img));
 have_image=1;
}
static void describe(char *out,size_t cap) {
 char cls[1024];
 unsigned long info3=reply.sf_x_info[0][3];
 classify(cls,sizeof(cls));
 _snprintf(out,cap-1,"cmd %08lX mode %03lX src (%lu,%lu) dst (%lu,%lu) size %lux%lu\r\nchanged %lu pixels; changed box x %lu..%lu y %lu..%lu\r\nengine %s%s\r\n%s",
  request.lab_cmd,request.lab_mode,request.lab_sx,request.lab_sy,request.lab_dx,request.lab_dy,request.lab_w,request.lab_h,
  (unsigned long)reply.sf_x_info[0][0],(info3>>24)&0xFF,(info3>>8)&0xFF,(info3>>16)&0xFF,info3&0xFF,
  reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?"HUNG then reset OK":"HUNG, reset FAILED: restart Windows"):"finished normally",
  reply.result==VX3D_PASS?"":" (driver result not PASS)",cls);
 out[cap-1]=0;
 if(request.lab_nregs) {
  unsigned int i,m=request.lab_nregs>VX3D_LAB_REGS?VX3D_LAB_REGS:request.lab_nregs; char rb[600]; rb[0]=0;
  strcat(out,"\r\nreg readback (wrote/read):");
  for(i=0;i<m && i<16;i++) if(request.lab_reg[i][0]>=0x1E00||request.lab_reg[i][0]<0x100) {sprintf(rb,"\r\n%04lX %08lX/%08lX",request.lab_reg[i][0],request.lab_reg[i][1],(unsigned long)reply.sf_x_cls[0][i]); if(strlen(out)+strlen(rb)<cap-1) strcat(out,rb);}
 }
}
static void read_fields(void) {
 request.lab_cmd=gethex(hcmd); request.lab_mode=gethex(hmode);
 request.lab_w=getdec(hw); request.lab_h=getdec(hh);
 request.lab_sx=getdec(hsx); request.lab_sy=getdec(hsy);
 request.lab_dx=getdec(hdx); request.lab_dy=getdec(hdy);
 request.lab_fg=gethex(hfg); request.lab_bg=gethex(hbg);
 request.lab_flags=(SendMessage(hhost,BM_GETCHECK,0,0)?1UL:0UL)|(SendMessage(hincr,BM_GETCHECK,0,0)?2UL:0UL)|(SendMessage(hroff,BM_GETCHECK,0,0)?4UL:0UL)|(SendMessage(hcpu,BM_GETCHECK,0,0)?8UL:0UL)|(SendMessage(hrt,BM_GETCHECK,0,0)?16UL:0UL)|(gethex(hxf)&0x60UL);
 parse_regs(); parse_data();
}
static void do_run(void) {
 char d[1500];
 memset(&request,0,sizeof(request)); read_fields();
 if(!can_run) return;
 if(!call_lab()) {logf2("RUN failed: %s\r\n",status_text); InvalidateRect(hmain,NULL,TRUE); return;}
 if(reply.result!=VX3D_PASS) {
  _snprintf(status_text,sizeof(status_text)-1,"Driver result %lu (not PASS). %s",reply.result,reply.fault_latched?"Engine latched a fault: restart Windows.":"");
  logf2("RUN result %lu fault %lu\r\n",reply.result,reply.fault_latched);
  if(reply.result==VX3D_TIMEOUT||reply.result==VX3D_DISABLED) can_run=0;
  InvalidateRect(hmain,NULL,TRUE); return;
 }
 collect(); describe(d,sizeof(d));
 strcpy(status_text,d);
 logf2("\r\nRUN\r\n%s\r\n",d);
 InvalidateRect(hmain,NULL,TRUE);
}
static void do_verdict(int i) {
 logf2("USER SAYS: %s  (for cmd %08lX mode %03lX)\r\n",verd_text[i],request.lab_cmd,request.lab_mode);
 strcat(status_text,"\r\nLogged: you said it looks ");
 strcat(status_text,verd_text[i]); strcat(status_text,".");
 InvalidateRect(hmain,NULL,TRUE);
}

/* ---- sweep ---- */
static DWORD sheet[512*512];
static void save_sheet(const char *name) {
 BITMAPFILEHEADER bf; BITMAPINFOHEADER bi; HANDLE f; DWORD n; int y;
 memset(&bf,0,sizeof(bf)); memset(&bi,0,sizeof(bi));
 bf.bfType=0x4D42; bf.bfOffBits=sizeof(bf)+sizeof(bi); bf.bfSize=bf.bfOffBits+512*512*4;
 bi.biSize=sizeof(bi); bi.biWidth=512; bi.biHeight=512; bi.biPlanes=1; bi.biBitCount=32; bi.biCompression=BI_RGB;
 f=CreateFile(name,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(f==INVALID_HANDLE_VALUE) return;
 WriteFile(f,&bf,sizeof(bf),&n,NULL); WriteFile(f,&bi,sizeof(bi),&n,NULL);
 for(y=511;y>=0;y--) WriteFile(f,sheet+y*512,512*4,&n,NULL);
 CloseHandle(f);
}
static const char *host8_str[8]={"FFFF0000","FFFFFFFF","FFFFFFFF","FFFFFFFF","FFFFFFFF","FFFFFFFF","FFFFFFFF","FF00FF00"};
static DWORD hostimg[64],hostxf[8][64];
static void make_host8(void) {
 unsigned int x,y,t;
 for(y=0;y<8;y++) for(x=0;x<8;x++) {
  DWORD c=0xFF303030UL;
  if(y==0) c=(x==0)?0xFFFF0000UL:(x==7)?0xFF00FF00UL:0xFFFFFFFFUL;
  if(y==7 && x==0) c=0xFF0000FFUL;
  if(y==7 && x==7) c=0xFFFFFF00UL;
  hostimg[y*8+x]=c;
 }
 for(y=0;y<8;y++) for(x=0;x<8;x++) {
  DWORD v=hostimg[y*8+x];
  hostxf[0][y*8+x]=v; hostxf[1][x*8+(7-y)]=v; hostxf[2][(7-y)*8+(7-x)]=v; hostxf[3][(7-x)*8+y]=v;
  hostxf[4][y*8+(7-x)]=v; hostxf[5][(7-y)*8+x]=v; hostxf[6][x*8+y]=v; hostxf[7][(7-x)*8+(7-y)]=v;
 }
 (void)t; (void)host8_str;
}
static int find_host8(char *out,size_t cap) {
 int t,px,py,x,y,found=0; char line[120]; out[0]=0;
 for(t=0;t<8;t++) for(py=0;py<=56;py++) for(px=0;px<=56;px++) {
  int ok=1;
  for(y=0;y<8&&ok;y++) for(x=0;x<8;x++) if(after_img[(py+y)*64+px+x]!=hostxf[t][y*8+x]) {ok=0;break;}
  if(ok) {sprintf(line,"%s@(%d,%d) ",tname[t],px,py); if(strlen(out)+strlen(line)<cap) strcat(out,line); found++;}
 }
 if(!found) strncpy(out,"no whole 8x8 block found",cap-1);
 out[cap-1]=0; return found;
}
static void do_sweep(void) {
 static const unsigned long decb[4]={0x0000UL,0x8000UL,0x4000UL,0xC000UL};
 static const unsigned int ddx[4]={4,11,4,11},ddy[4]={4,4,11,11};
 static const char *decn[4]={"no dec","X dec","Y dec","X+Y dec"};
 static const unsigned long ctlv[4]={0,0x0D,0x15,0x1D};
 unsigned int rot,dec,wm,wmax,idx=0,i,exact=0; char cls[400],line[600],*dp;
 if(!can_run) return;
 make_host8();
 logf2("\r\nHOST SWEEP v10.01: cmd CC002049|dec, 8x8 host colour bitmap (first pixel red, top row white, corners red TL/green TR/blue BL/yellow BR), mode 300|angle. wm: 0 = CBU window off, 1 = window with 640x480 geometry (as XP), 2 = window with swapped geometry (90/270 only)\r\n");
 for(rot=0;rot<4 && can_run;rot++) {
  wmax=(rot==0)?1:(rot==2)?2:3;
  for(wm=0;wm<wmax && can_run;wm++) for(dec=0;dec<4 && can_run;dec++) {
   memset(&request,0,sizeof(request));
   request.lab_cmd=0xCC002049UL|decb[dec]; request.lab_mode=0x300UL|rot; request.lab_w=8; request.lab_h=8;
   request.lab_sx=60; request.lab_sy=60; request.lab_dx=ddx[dec]; request.lab_dy=ddy[dec];
   request.lab_fg=0xFFFF8000UL; request.lab_bg=0xFF004080UL; request.lab_flags=1|4;
   for(i=0;i<64;i++) request.lab_data[i]=hostimg[i]; request.lab_ndata=64;
   if(wm) {
    request.lab_reg[0][0]=0x1E04; request.lab_reg[0][1]=0;
    request.lab_reg[1][0]=0x1E08; request.lab_reg[1][1]=0x0012C000UL;
    request.lab_reg[2][0]=0x1E0C; request.lab_reg[2][1]=0x1999UL;
    request.lab_reg[3][0]=0x1E10; request.lab_reg[3][1]=(wm==2)?0x01E001E0UL:0x02800280UL;
    request.lab_reg[4][0]=0x1E14; request.lab_reg[4][1]=(wm==2)?0x028001E0UL:0x01E00280UL;
    request.lab_reg[5][0]=0x1E00; request.lab_reg[5][1]=ctlv[rot];
    request.lab_nregs=6;
   }
   sprintf(status_text,"Host sweep run %u ...",idx+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
   if(!call_lab()) {logf2("HSWEEP %u call failed: %s\r\n",idx,status_text); can_run=0; break;}
   if(reply.result!=VX3D_PASS) {logf2("HSWEEP %u result %lu fault %lu\r\n",idx,reply.result,reply.fault_latched); can_run=0; break;}
   collect(); if(find_host8(cls,sizeof(cls))) exact++;
   dp=cls; (void)dp;
   sprintf(line,"HSWEEP %2u: angle %u wm %u %-7s start(%u,%u): changed %lu%s; %s\r\n",idx,rot*90,wm,decn[dec],ddx[dec],ddy[dec],(unsigned long)reply.sf_x_info[0][0],reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"",cls);
   logtext(line);
   idx++;
  }
 }
 sprintf(status_text,"Host sweep done: %u runs, %u gave a whole 8x8 block. Send me VX2DLAB.TXT (grep HSWEEP).",idx,exact);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}

static void apply_preset(void) {
 int i=(int)SendMessage(hpreset,CB_GETCURSEL,0,0);
 if(i<0||i>=NPRESET) return;
 sethex(hcmd,presets[i].cmd); sethex(hmode,presets[i].mode); SetWindowText(hw,presets[i].w); SetWindowText(hh,presets[i].h);
 SetWindowText(hsx,presets[i].sx); SetWindowText(hsy,presets[i].sy); SetWindowText(hdx,presets[i].dx); SetWindowText(hdy,presets[i].dy);
 SetWindowText(hregs,presets[i].regs);
 {char xb[16]; sprintf(xb,"%X",presets[i].flags&0x60); SetWindowText(hxf,xb);} SendMessage(hrt,BM_SETCHECK,(presets[i].flags&16)?1:0,0); SendMessage(hcpu,BM_SETCHECK,(presets[i].flags&8)?1:0,0); SendMessage(hroff,BM_SETCHECK,(presets[i].flags&4)?1:0,0); SendMessage(hhost,BM_SETCHECK,(presets[i].flags&1)?1:0,0); SendMessage(hincr,BM_SETCHECK,(presets[i].flags&2)?1:0,0); if(presets[i].data) SetWindowText(hdata,presets[i].data);
}

static HWND mk(HWND w,const char *cls,const char *text,DWORD style,int x,int y,int cw,int ch,int id) {
 HWND c=CreateWindow(cls,text,WS_CHILD|WS_VISIBLE|style,x,y,cw,ch,w,(HMENU)id,GetModuleHandle(NULL),NULL);
 SendMessage(c,WM_SETFONT,(WPARAM)font,TRUE); return c;
}
static HWND field(HWND w,const char *lab,const char *init,int x,int y,int cw,int id) {
 mk(w,"STATIC",lab,0,x,y+3,34,16,0);
 return mk(w,"EDIT",init,WS_BORDER|ES_AUTOHSCROLL,x+34,y,cw,20,id);
}
static void draw_img(HDC dc,const DWORD *img,int x0,int y0) {
 BITMAPINFO bi; memset(&bi,0,sizeof(bi));
 bi.bmiHeader.biSize=sizeof(bi.bmiHeader); bi.bmiHeader.biWidth=64; bi.bmiHeader.biHeight=-64;
 bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=32; bi.bmiHeader.biCompression=BI_RGB;
 StretchDIBits(dc,x0,y0,64*SCALE,64*SCALE,0,0,64,64,img,&bi,DIB_RGB_COLORS,SRCCOPY);
}
static void open_device(void) {
 vx_status_t s; DWORD got=0;
 device=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(device==INVALID_HANDLE_VALUE) {sprintf(status_text,"Cannot open VX85598: Windows error %lu.",GetLastError()); return;}
 memset(&s,0,sizeof(s));
 if(!DeviceIoControl(device,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC||s.cb!=sizeof(s)||s.version!=VX_VERSION) {
  strcpy(status_text,"Installed driver has an incompatible status interface."); return;
 }
 memset(&reply,0,sizeof(reply));
 if(!DeviceIoControl(device,VX_IOCTL_3D_PROBE,NULL,0,&reply,sizeof(reply),&got,NULL)||reply.magic!=VX3D_MAGIC||reply.build!=VX3D_BUILD) {
  strcpy(status_text,"Driver build does not match this program. Copy both files from one package and restart Windows."); return;
 }
 if(reply.fault_latched) {strcpy(status_text,"The engine reports a latched fault. Restart Windows."); return;}
 can_run=1;
 sprintf(status_text,"Ready (desktop %lux%lux%lu). Pick a preset (it loads at once) and press RUN. The picture shows the 64x64 test window before and after.",s.width,s.height,s.bpp);
}
static DWORD nimg[1024],nxf[8][1024];
static int nsw,nsh,ntw[8],nth[8],nuniq;
static void make_hostn(int w,int h) {
 int x,y,t;
 nsw=w; nsh=h;
 for(y=0;y<h;y++) for(x=0;x<w;x++) {
  DWORD c=0xFF303030UL;
  if(y==0) c=0xFFFFFFFFUL;
  if(x==0 && y==h-1) c=0xFF0000FFUL;
  if(x==w-1 && y==h-1) c=0xFFFFFF00UL;
  if(x==0 && y==0) c=0xFFFF0000UL;
  if(x==w-1 && y==0) c=0xFF00FF00UL;
  if(x==2 && y==2) c=0xFFFF00FFUL; /* extra asymmetry marker */
  if(nuniq) c=0xFF800000UL|(DWORD)(y*w+x+1);
  nimg[y*w+x]=c;
  nxf[0][y*w+x]=c; nxf[1][x*h+(h-1-y)]=c; nxf[2][(h-1-y)*w+(w-1-x)]=c; nxf[3][(w-1-x)*h+y]=c;
  nxf[4][y*w+(w-1-x)]=c; nxf[5][(h-1-y)*w+x]=c; nxf[6][x*h+y]=c; nxf[7][(w-1-x)*h+(h-1-y)]=c;
 }
 for(t=0;t<8;t++) {ntw[t]=(t==1||t==3||t==6||t==7)?h:w; nth[t]=(t==1||t==3||t==6||t==7)?w:h;}
}
static int find_hostn(char *out,size_t cap) {
 int t,px,py,x,y,found=0; char line[120]; out[0]=0;
 for(t=0;t<8;t++) for(py=0;py+nth[t]<=64;py++) for(px=0;px+ntw[t]<=64;px++) {
  int ok=1;
  for(y=0;y<nth[t]&&ok;y++) for(x=0;x<ntw[t];x++) if(after_img[(py+y)*64+px+x]!=nxf[t][y*ntw[t]+x]) {ok=0;break;}
  if(ok) {sprintf(line,"%s@(%d,%d) ",tname[t],px,py); if(strlen(out)+strlen(line)<cap) strcat(out,line); found++;}
 }
 if(!found) strncpy(out,"no whole block found",cap-1);
 out[cap-1]=0; return found;
}
static void do_sweep2_unused(void) {
 static const unsigned long decb[4]={0x0000UL,0x8000UL,0x4000UL,0xC000UL};
 static const char *decn[4]={"no dec","X dec","Y dec","X+Y dec"};
 unsigned int rot,dec,hyp,idx=0,i,exact=0,bw,bh,sx0,sy0; char cls[500],line[700]; unsigned long ib;
 int W=10,H=6;
 if(!can_run) return;
 make_hostn(W,H);
 logf2("\r\nNONSQUARE SWEEP v10.01: cmd CC002049|dec, host colour bitmap %dx%d (TL red, TR green, BL blue, BR yellow, top row white, magenta marker at 2,2), no CBU window. hyp 0: destination box has the same size as the source (w x h); hyp 1: destination box is swapped (h x w). start = chosen corner of that box placed at (4,4)\r\n",W,H);
 for(rot=0;rot<4 && can_run;rot++) for(hyp=0;hyp<2 && can_run;hyp++) for(dec=0;dec<4 && can_run;dec++) {
  if(hyp==1 && (rot==0||rot==2)) continue;
  bw=(hyp==0)?(unsigned)W:(unsigned)H; bh=(hyp==0)?(unsigned)H:(unsigned)W;
  sx0=4+((decb[dec]&0x8000UL)?bw-1:0); sy0=4+((decb[dec]&0x4000UL)?bh-1:0);
  memset(&request,0,sizeof(request));
  request.lab_cmd=0xCC002049UL|decb[dec]; request.lab_mode=0x300UL|rot; request.lab_w=(unsigned)W; request.lab_h=(unsigned)H;
  request.lab_sx=60; request.lab_sy=60; request.lab_dx=sx0; request.lab_dy=sy0;
  request.lab_fg=0xFFFF8000UL; request.lab_bg=0xFF004080UL; request.lab_flags=1|4;
  for(i=0;i<(unsigned)(W*H);i++) request.lab_data[i]=nimg[i]; request.lab_ndata=(unsigned)(W*H);
  sprintf(status_text,"Non-square sweep run %u ...",idx+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("NSWEEP %u call failed: %s\r\n",idx,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("NSWEEP %u result %lu fault %lu\r\n",idx,reply.result,reply.fault_latched); can_run=0; break;}
  collect(); if(find_hostn(cls,sizeof(cls))) exact++;
  ib=reply.sf_x_info[0][3];
  sprintf(line,"NSWEEP %2u: angle %u hyp %u %-7s start(%u,%u): changed %lu box x %lu..%lu y %lu..%lu%s; %s\r\n",idx,rot*90,hyp,decn[dec],sx0,sy0,(unsigned long)reply.sf_x_info[0][0],(ib>>24)&0xFF,(ib>>8)&0xFF,(ib>>16)&0xFF,ib&0xFF,reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"",cls);
  logtext(line);
  idx++;
 }
 sprintf(status_text,"Non-square sweep done: %u runs, %u gave a whole block. Send me VX2DLAB.TXT (grep NSWEEP).",idx,exact);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}

static void do_sweep3_unused(void) {
 static const unsigned long decb[4]={0x0000UL,0x8000UL,0x4000UL,0xC000UL};
 static const char *decn[4]={"no dec","X dec","Y dec","X+Y dec"};
 static const int sz[5]={5,12,16,24,32};
 unsigned int si,dec,idx=0,i,exact=0,sx0,sy0,bw,bh; int x,y,k; char cls[500],line[700],row[120]; unsigned long ib;
 if(!can_run) return;
 logf2("\r\nSIZE SWEEP v10.01 part 1: square host blits, mode 301, cmd CC002049|dec, start = chosen corner of an s x s box at (4,4)\r\n");
 for(si=0;si<5 && can_run;si++) for(dec=0;dec<4 && can_run;dec++) {
  int s=sz[si];
  make_hostn(s,s);
  sx0=4+((decb[dec]&0x8000UL)?(unsigned)s-1:0); sy0=4+((decb[dec]&0x4000UL)?(unsigned)s-1:0);
  memset(&request,0,sizeof(request));
  request.lab_cmd=0xCC002049UL|decb[dec]; request.lab_mode=0x301UL; request.lab_w=(unsigned)s; request.lab_h=(unsigned)s;
  request.lab_sx=60; request.lab_sy=60; request.lab_dx=sx0; request.lab_dy=sy0;
  request.lab_fg=0xFFFF8000UL; request.lab_bg=0xFF004080UL; request.lab_flags=1|4;
  for(i=0;i<(unsigned)(s*s);i++) request.lab_data[i]=nimg[i]; request.lab_ndata=(unsigned)(s*s);
  sprintf(status_text,"Size sweep run %u ...",idx+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("SSWEEP %u call failed: %s\r\n",idx,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("SSWEEP %u result %lu fault %lu\r\n",idx,reply.result,reply.fault_latched); can_run=0; break;}
  collect(); if(find_hostn(cls,sizeof(cls))) exact++;
  ib=reply.sf_x_info[0][3];
  sprintf(line,"SSWEEP %2u: %dx%d %-7s start(%u,%u): changed %lu box x %lu..%lu y %lu..%lu%s; %s\r\n",idx,s,s,decn[dec],sx0,sy0,(unsigned long)reply.sf_x_info[0][0],(ib>>24)&0xFF,(ib>>8)&0xFF,(ib>>16)&0xFF,ib&0xFF,reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"",cls);
  logtext(line); idx++;
 }
 logf2("\r\nSIZE SWEEP part 2: non-square host blits, mode 301. Every source pixel has a unique number (row-major index, hex). Grid shows which source pixel landed at each window position x 0..19, y 0..19 ('..' = untouched)\r\n");
 for(si=0;si<2 && can_run;si++) for(dec=0;dec<4 && can_run;dec++) {
  int W=(si==0)?10:6,H=(si==0)?6:10;
  make_hostn(W,H);
  for(i=0;i<(unsigned)(W*H);i++) nimg[i]=0xFF800000UL|(i+1);
  bw=(unsigned)W; bh=(unsigned)H;
  sx0=4+((decb[dec]&0x8000UL)?bw-1:0); sy0=4+((decb[dec]&0x4000UL)?bh-1:0);
  memset(&request,0,sizeof(request));
  request.lab_cmd=0xCC002049UL|decb[dec]; request.lab_mode=0x301UL; request.lab_w=(unsigned)W; request.lab_h=(unsigned)H;
  request.lab_sx=60; request.lab_sy=60; request.lab_dx=sx0; request.lab_dy=sy0;
  request.lab_fg=0xFFFF8000UL; request.lab_bg=0xFF004080UL; request.lab_flags=1|4;
  for(i=0;i<(unsigned)(W*H);i++) request.lab_data[i]=nimg[i]; request.lab_ndata=(unsigned)(W*H);
  sprintf(status_text,"Size sweep map %u ...",idx+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("MSWEEP call failed: %s\r\n",status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("MSWEEP result %lu fault %lu\r\n",reply.result,reply.fault_latched); can_run=0; break;}
  collect();
  logf2("MSWEEP %ux%u source, %s, start (%u,%u)%s\r\n",bw,bh,decn[dec],sx0,sy0,reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"");
  for(y=0;y<20;y++) {
   row[0]=0;
   for(x=0;x<20;x++) {
    DWORD a=after_img[y*64+x]; char cell[8];
    if(a==before_img[y*64+x]) strcpy(cell,".. ");
    else if((a&0xFFFF0000UL)==0xFF800000UL && (int)(a&0xFFFF)>=1 && (int)(a&0xFFFF)<=W*H) sprintf(cell,"%02X ",(unsigned)((a&0xFFFF)-1));
    else strcpy(cell,"?? ");
    strcat(row,cell);
   }
   logf2("  y%02d %s\r\n",y,row);
  }
  (void)k; idx++;
 }
 sprintf(status_text,"Size sweep done: %u runs, %u squares gave a whole block. Send me VX2DLAB.TXT (grep SSWEEP / MSWEEP).",idx,exact);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}

static void do_verify(void) {
 static const int cs[9][2]={{10,6},{6,10},{30,20},{20,30},{31,17},{17,31},{1,10},{10,1},{3,7}};
 unsigned int ci,mode3,idx=0,i,pass=0,sx0,sy0; int sw,sh,want,w2,h2; char cls[400],line[700],name[16]; unsigned long ib,cmd;
 if(!can_run) return;
 nuniq=1;
 logf2("\r\nROTATION VERIFY v10.01: source image sw x sh (unique pixels), fed row by row through the host window. Engine size is the DESTINATION size (swapped for 90/270). 90 cw: mode 301, X dec, start = top-right of the destination box. 270 cw: mode 303, Y dec, start = bottom-left. 180: mode 302, X+Y dec, size unswapped, start = bottom-right. Box placed at (4,4)\r\n");
 for(ci=0;ci<9 && can_run;ci++) for(mode3=0;mode3<3 && can_run;mode3++) {
  sw=cs[ci][0]; sh=cs[ci][1];
  make_hostn(sw,sh);
  memset(&request,0,sizeof(request));
  if(mode3==0) {want=1; strcpy(name,"90 cw"); w2=sh; h2=sw; cmd=0xCC002049UL|0x8000UL; request.lab_mode=0x301UL; sx0=4+(unsigned)w2-1; sy0=4;}
  else if(mode3==1) {want=3; strcpy(name,"270 cw"); w2=sh; h2=sw; cmd=0xCC002049UL|0x4000UL; request.lab_mode=0x303UL; sx0=4; sy0=4+(unsigned)h2-1;}
  else {want=2; strcpy(name,"180"); w2=sw; h2=sh; cmd=0xCC002049UL|0xC000UL; request.lab_mode=0x302UL; sx0=4+(unsigned)w2-1; sy0=4+(unsigned)h2-1;}
  request.lab_cmd=cmd; request.lab_w=(unsigned)w2; request.lab_h=(unsigned)h2;
  request.lab_sx=60; request.lab_sy=60; request.lab_dx=sx0; request.lab_dy=sy0;
  request.lab_fg=0xFFFF8000UL; request.lab_bg=0xFF004080UL; request.lab_flags=1|4;
  for(i=0;i<(unsigned)(sw*sh);i++) request.lab_data[i]=nimg[i]; request.lab_ndata=(unsigned)(sw*sh);
  sprintf(status_text,"Rotation verify %u ...",idx+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("VERIFY %u call failed: %s\r\n",idx,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("VERIFY %u result %lu fault %lu\r\n",idx,reply.result,reply.fault_latched); can_run=0; break;}
  collect(); find_hostn(cls,sizeof(cls));
  { char want_s[40]; int ok; sprintf(want_s,"%s@(4,4)",tname[want]); ok=(strstr(cls,want_s)!=NULL); if(ok) pass++;
  ib=reply.sf_x_info[0][3];
  sprintf(line,"VERIFY %2u: %2dx%-2d %-6s engine size %dx%d start(%u,%u): %s; changed %lu box x %lu..%lu y %lu..%lu%s; found: %s\r\n",idx,sw,sh,name,w2,h2,sx0,sy0,ok?"PASS":"FAIL",(unsigned long)reply.sf_x_info[0][0],(ib>>24)&0xFF,(ib>>8)&0xFF,(ib>>16)&0xFF,ib&0xFF,reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"",cls);
  logtext(line); }
  idx++;
 }
 nuniq=0;
 sprintf(status_text,"Rotation verify done: %u of %u passed. Send me VX2DLAB.TXT (grep VERIFY).",pass,idx);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}

static unsigned short p16[64*128];
static unsigned long dk_pre(int x,int y) {
 unsigned long v;
 if(x>=4 && x<20 && y>=4 && y<20) {
  int gx=x-4,gy=y-4; v=0xFF203060UL;
  if((gx>=3&&gx<=5&&gy>=3&&gy<=13)||(gy>=3&&gy<=5&&gx>=3&&gx<=12)||(gy>=7&&gy<=9&&gx>=3&&gx<=9)) v=0xFFFFFFFFUL;
  if(gx<2&&gy<2) v=0xFFFF0000UL;
  if(gx>13&&gy<2) v=0xFF00FF00UL;
  if(gx<2&&gy>13) v=0xFF0000FFUL;
  if(gx>13&&gy>13) v=0xFFFFFF00UL;
  return v;
 }
 return 0xFF101820UL|(((unsigned long)(x*3+y*2)&0x1FUL)<<16)|(((unsigned long)(x+y*3)&0x1FUL)<<8);
}
static void do_dkey(void) {
 static const char *nm[4]={"fallback: dest-key COPY from a solid scratch surface","native: fixed-colour fill (F0002001) + dest key","native: mono register pattern all ones (F0000A01) + dest key","native: fixed fill, key colour also in 58 pattern fg"};
 unsigned int k,idx=0,good=0; int x,y; unsigned long key=0xFF203060UL,fill=0xFFFF8000UL,got,orig,want; unsigned int nfill,nwrong,nmiss,n;
 char line[400];
 if(!can_run) return;
 logf2("\r\nDEST KEY FILL v10.01: 16x16 box at (4,4) of the prefilled window; key colour FF203060 (the box background); fill colour FFFF8000. PASS = every key-coloured pixel in the box becomes the fill colour and every other pixel is untouched.\r\n");
 for(k=0;k<4 && can_run;k++) {
  memset(&request,0,sizeof(request));
  request.lab_mode=0x300UL; request.lab_w=16; request.lab_h=16; request.lab_dx=4; request.lab_dy=4;
  request.lab_sx=0; request.lab_sy=0; request.lab_fg=fill; request.lab_bg=0xFF004080UL; request.lab_flags=4;
  if(k==0) {
   request.lab_cmd=0xCC000001UL; request.lab_flags|=128;
   request.lab_reg[0][0]=0x08; request.lab_reg[0][1]=(320UL<<16)|32UL;
   request.lab_reg[1][0]=0x1C; request.lab_reg[1][1]=0x00040200UL;
   request.lab_reg[2][0]=0x18; request.lab_reg[2][1]=0x01400000UL; /* scratch coords 0,0 (hardware adds nothing): source row 320 would be wrong, so use plain 0 below */
   request.lab_reg[2][1]=0UL;
   request.lab_reg[3][0]=0x48; request.lab_reg[3][1]=0x8000UL;
   request.lab_reg[4][0]=0x4C; request.lab_reg[4][1]=key;
   request.lab_nregs=5;
  } else if(k==1) {
   request.lab_cmd=0xF0002001UL;
   request.lab_reg[0][0]=0x48; request.lab_reg[0][1]=0x8000UL;
   request.lab_reg[1][0]=0x4C; request.lab_reg[1][1]=key;
   request.lab_reg[2][0]=0x58; request.lab_reg[2][1]=fill;
   request.lab_nregs=3;
  } else if(k==2) {
   request.lab_cmd=0xF0000A01UL;
   request.lab_reg[0][0]=0x24; request.lab_reg[0][1]=0xFFFFFFFFUL;
   request.lab_reg[1][0]=0x28; request.lab_reg[1][1]=0xFFFFFFFFUL;
   request.lab_reg[2][0]=0x48; request.lab_reg[2][1]=0x8000UL;
   request.lab_reg[3][0]=0x4C; request.lab_reg[3][1]=key;
   request.lab_reg[4][0]=0x58; request.lab_reg[4][1]=fill;
   request.lab_reg[5][0]=0x5C; request.lab_reg[5][1]=fill;
   request.lab_nregs=6;
  } else {
   request.lab_cmd=0xF0002001UL;
   request.lab_reg[0][0]=0x48; request.lab_reg[0][1]=0x8000UL;
   request.lab_reg[1][0]=0x4C; request.lab_reg[1][1]=key;
   request.lab_reg[2][0]=0x58; request.lab_reg[2][1]=key;
   request.lab_reg[3][0]=0x5C; request.lab_reg[3][1]=fill;
   request.lab_nregs=4;
  }
  sprintf(status_text,"Dest key fill %u ...",k+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("DKEY %u call failed: %s\r\n",k,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("DKEY %u result %lu fault %lu\r\n",k,reply.result,reply.fault_latched); can_run=0; break;}
  collect();
  nfill=nwrong=nmiss=n=0;
  for(y=0;y<64;y++) for(x=0;x<64;x++) {
   got=reply.image[y*64+x]; orig=dk_pre(x,y);
   want=(x>=4&&x<20&&y>=4&&y<20&&orig==key)?fill:orig;
   if(want==fill && orig==key && x>=4&&x<20&&y>=4&&y<20) n++;
   if(got!=want) {if(got==fill) nwrong++; else if(orig==key) nmiss++; else nwrong++;} else if(want==fill && orig==key) nfill++;
  }
  { int ok=(!reply.sf_x_info[0][1] && nwrong==0 && nmiss==0 && nfill==n); if(ok) good++;
  sprintf(line,"DKEY %u: %s: %s; key pixels filled %u of %u, key pixels left unfilled %u, other pixels wrong %u%s\r\n",k,nm[k],ok?"PASS":"FAIL",nfill,n,nmiss,nwrong,reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"");
  logtext(line); }
  idx++;
 }
 sprintf(status_text,"Dest key test done: %u of %u passed. Send me VX2DLAB.TXT (grep DKEY).",good,idx);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}
static void do_dkey2(void) {
 static const unsigned long cm[9]={0xF0000A01UL,0xF0002A01UL,0xF0002801UL,0xF0012001UL,0xF0402001UL,0xAA002001UL,0xF0003001UL,0xCC002001UL,0xF0002001UL};
 static const char *nm[9]={"control: mono reg pattern (F0000A01), the passing form","bit13 + bit11 + bit9 (F0002A01)","bit13 + bit11 (F0002801)","bit13 + mono transparency bit16 (F0012001)","bit13 + pattern keep bit22 (F0402001)","bit13 with ROP AA (AA002001)","bit13 + clip enable (F0003001)","bit13 with ROP CC (CC002001)","bit13 plain (F0002001), the hanging form"};
 unsigned int k,idx=0,good=0; int x,y; unsigned long key=0xFF203060UL,fill=0xFFFF8000UL,got,orig,want; unsigned int nfill,nwrong,nmiss,n,nsame,nother,nbox; unsigned long c1=0,c2=0,c3=0; char line[500];
 if(!can_run) return;
 for(y=0;y<512*512;y++) sheet[y]=0xFF000000UL;
 logf2("\r\nDEST KEY FILL SWEEP v10.01: same box and key as v10.01. Each row changes one command bit around the hanging F0002001 form; hang-prone rows last.\r\n");
 for(k=0;k<9 && can_run;k++) {
  memset(&request,0,sizeof(request));
  request.lab_mode=0x300UL; request.lab_w=16; request.lab_h=16; request.lab_dx=4; request.lab_dy=4;
  request.lab_sx=0; request.lab_sy=0; request.lab_fg=fill; request.lab_bg=0xFF004080UL; request.lab_flags=4;
  request.lab_cmd=cm[k];
  request.lab_reg[0][0]=0x24; request.lab_reg[0][1]=0xFFFFFFFFUL;
  request.lab_reg[1][0]=0x28; request.lab_reg[1][1]=0xFFFFFFFFUL;
  request.lab_reg[2][0]=0x48; request.lab_reg[2][1]=0x8000UL;
  request.lab_reg[3][0]=0x4C; request.lab_reg[3][1]=key;
  request.lab_reg[4][0]=0x58; request.lab_reg[4][1]=fill;
  request.lab_reg[5][0]=0x5C; request.lab_reg[5][1]=fill;
  request.lab_nregs=6;
  if(k==6) {request.lab_reg[6][0]=0x40; request.lab_reg[6][1]=0UL; request.lab_reg[7][0]=0x44; request.lab_reg[7][1]=0x01DF027FUL; request.lab_nregs=8;}
  if(k==7) {request.lab_flags|=128; request.lab_reg[6][0]=0x08; request.lab_reg[6][1]=(320UL<<16)|32UL; request.lab_reg[7][0]=0x1C; request.lab_reg[7][1]=0x00040200UL; request.lab_nregs=8;}
  sprintf(status_text,"Dest key sweep %u ...",k+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("DK2 %u call failed: %s\r\n",k,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("DK2 %u result %lu fault %lu\r\n",k,reply.result,reply.fault_latched); can_run=0; break;}
  collect();
  {int tx=(int)(k%4)*128,ty=(int)(k/4)*128,px,py; for(py=0;py<128;py++) for(px=0;px<128;px++) sheet[(ty+py)*512+tx+px]=reply.image[(py/2)*64+px/2];}
  nfill=nwrong=nmiss=n=nsame=nother=nbox=0;
  for(y=0;y<64;y++) for(x=0;x<64;x++) {
   got=reply.image[y*64+x]; orig=dk_pre(x,y);
   want=(x>=4&&x<20&&y>=4&&y<20&&orig==key)?fill:orig;
   if(want==fill && orig==key) {
    n++;
    if(got==fill) nfill++;
    else if(got==orig) nsame++;
    else {nother++; if(!c1) c1=got; else if(!c2 && got!=c1) c2=got; else if(!c3 && got!=c1 && got!=c2) c3=got;}
   } else if(got!=want) {nwrong++; nbox++;}
  }
  nmiss=nsame+nother;
  { int ok=(!reply.sf_x_info[0][1] && nwrong==0 && nmiss==0 && nfill==n); if(ok) good++;
  sprintf(line,"DK2 %u: %s: %s; of %u key pixels: %u became fill, %u unchanged, %u changed to another colour (first values %08lX %08lX %08lX); non-key pixels changed %u%s\r\n",k,nm[k],ok?"PASS":"FAIL",n,nfill,nsame,nother,c1,c2,c3,nwrong,reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"");
  logtext(line); }
  idx++;
 }
 {int tx,ty,px,py; for(py=0;py<128;py++) for(px=0;px<128;px++) {int sx=px/2,sy=py/2; unsigned long o=dk_pre(sx,sy),w2=(sx>=4&&sx<20&&sy>=4&&sy<20&&o==key)?fill:o; sheet[(256+py)*512+128+px]=o; sheet[(256+py)*512+256+px]=w2;}}
 save_sheet("DKSWEEP.BMP");
 logtext("BMP DKSWEEP.BMP written: 4 tiles per row, 2x scale. Row 1 = variants 0-3, row 2 = variants 4-7, row 3 = variant 8, then the untouched prefill, then the expected result.\r\n");
 sprintf(status_text,"Dest key sweep done: %u of %u passed. Send me VX2DLAB.TXT (grep DK2).",good,idx);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}
static void do_monosrc(void) {
 static const unsigned long cm[8]={0xCC000101UL,0xCC020101UL,0xCC0A0101UL,0xCC000101UL,0xCC000101UL,0xCC000101UL,0xCC000101UL,0xCC000102UL};
 static const unsigned int pt[8]={1,1,1,2,4,8,0,1};
 static const char *nm[8]={"src mono (CC000101) pitch 1","+ alignment enable bit 17 (CC020101) pitch 1","+ dword alignment bits 19:17 (CC0A0101) pitch 1","CC000101 pitch 2","CC000101 pitch 4","CC000101 pitch 8","CC000101 pitch 0","text command 2 + mono (CC000102) pitch 1"};
 unsigned int k,idx=0,good=0,i; int x,y,r,bit; unsigned long fg=0xFFFF8000UL,bg=0xFF004080UL,got,orig;
 unsigned int msb,lsb,nfg,nbg,noth,chg; unsigned char b4[4]; char line[500];
 if(!can_run) return;
 for(i=0;i<512*512;i++) sheet[i]=0xFF000000UL;
 logf2("\r\nMONO SOURCE FROM SCREEN MEMORY v10.01: 32x8 mono bitmap written to the scratch surface (8 bytes per row, byte0 = pixels 0..7 ...), expanded to a 32x8 box at (4,4): 1 bits = orange FFFF8000, 0 bits = blue FF004080. Source base 1C = scratch >> 3, source pitch varies, source XY 0. msb/lsb = pixels matching the expected image if bit 7 / bit 0 of each byte is the leftmost pixel. Hang-prone rows last.\r\n");
 for(k=0;k<8 && can_run;k++) {
  memset(&request,0,sizeof(request));
  request.lab_mode=0x300UL; request.lab_cmd=cm[k]; request.lab_w=32; request.lab_h=8; request.lab_dx=4; request.lab_dy=4;
  request.lab_sx=0; request.lab_sy=0; request.lab_fg=fg; request.lab_bg=bg; request.lab_flags=4|1024;
  for(r=0;r<8;r++) {
   b4[0]=(unsigned char)(((0x80>>r)|0x01)&0xFF); b4[1]=0xAA; b4[2]=0x0F; b4[3]=(unsigned char)(r*0x11);
   request.lab_data[2*r]=(unsigned long)b4[0]|((unsigned long)b4[1]<<8)|((unsigned long)b4[2]<<16)|((unsigned long)b4[3]<<24);
   request.lab_data[2*r+1]=0;
  }
  request.lab_ndata=16;
  request.lab_reg[0][0]=0x08; request.lab_reg[0][1]=(320UL<<16)|(unsigned long)pt[k];
  request.lab_reg[1][0]=0x1C; request.lab_reg[1][1]=0x00040200UL;
  request.lab_reg[2][0]=0x18; request.lab_reg[2][1]=0UL;
  request.lab_nregs=3;
  sprintf(status_text,"Mono source %u ...",k+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("MSRC %u call failed: %s\r\n",k,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("MSRC %u result %lu fault %lu\r\n",k,reply.result,reply.fault_latched); can_run=0; break;}
  collect();
  {int tx=(int)(k%4)*128,ty=(int)(k/4)*128,px,py; for(py=0;py<128;py++) for(px=0;px<128;px++) sheet[(ty+py)*512+tx+px]=reply.image[(py/2)*64+px/2];}
  msb=lsb=nfg=nbg=noth=chg=0;
  for(y=0;y<8;y++) for(x=0;x<32;x++) {
   int bm,bl; unsigned char byv;
   r=y; byv=(x/8==0)?(unsigned char)(((0x80>>r)|0x01)&0xFF):(x/8==1)?0xAA:(x/8==2)?0x0F:(unsigned char)(r*0x11);
   bm=(byv>>(7-(x%8)))&1; bl=(byv>>(x%8))&1;
   got=reply.image[(4+y)*64+4+x]; orig=dk_pre(4+x,4+y);
   if(got!=orig) chg++;
   if(got==fg) nfg++; else if(got==bg) nbg++; else noth++;
   if(got==(bm?fg:bg)) msb++;
   if(got==(bl?fg:bg)) lsb++;
  }
  { int ok=(!reply.sf_x_info[0][1] && (msb==256 || lsb==256)); if(ok) good++;
  sprintf(line,"MSRC %u: %s: %s; box pixels changed %u of 256, orange %u, blue %u, other %u; match msb %u lsb %u of 256%s\r\n",k,nm[k],ok?"PASS":"FAIL",chg,nfg,nbg,noth,msb,lsb,reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"");
  logtext(line); }
  idx++;
 }
 {int px,py,xx,yy; unsigned long v; unsigned char byv;
  for(py=0;py<128;py++) for(px=0;px<128;px++) {
   xx=px/2-4; yy=py/2-4; v=dk_pre(px/2,py/2);
   if(xx>=0&&xx<32&&yy>=0&&yy<8) {
    byv=(xx/8==0)?(unsigned char)(((0x80>>yy)|0x01)&0xFF):(xx/8==1)?0xAA:(xx/8==2)?0x0F:(unsigned char)(yy*0x11);
    v=((byv>>(7-(xx%8)))&1)?fg:bg;
   }
   sheet[(256+py)*512+px]=v;
  }
 }
 save_sheet("MONOSRC.BMP");
 logtext("BMP MONOSRC.BMP written: 4 tiles per row, 2x scale. Row 1 = runs 0-3, row 2 = runs 4-7, row 3 first tile = expected result (MSB-first reading).\r\n");
 sprintf(status_text,"Mono source test done: %u of %u passed. Send me VX2DLAB.TXT and MONOSRC.BMP.",good,idx);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}
static void do_monosrc2(void) {
 static const unsigned long cm[6]={0xCC000101UL,0xCC001101UL,0xCC000501UL,0xCC020101UL,0xCC000102UL,0xCC001102UL};
 static const char *cn[6]={"CC000101","CC001101 +clip","CC000501 +src transp","CC020101 +align","CC000102 text","CC001102 text+clip"};
 static const unsigned int lc[3]={0,1,3};
 unsigned int ci,li,k=0,good=0,i; int x,y,r; unsigned long fg=0xFFFF8000UL,bg=0xFF004080UL,got,orig;
 unsigned int msb,lsb,chg; unsigned char b4[4]; char line[500];
 if(!can_run) return;
 for(i=0;i<512*512;i++) sheet[i]=0xFF000000UL;
 logf2("\r\nMONO SOURCE SWEEP 2 v10.01: same 32x8 bitmap and box as v10.01, source pitch 1, now varying the source location bits (reg 08 bits 13:12 = 00/01/11) and the command bits (clip, source transparency, alignment, text command). Order: location 00 first, then 01, then 11.\r\n");
 for(li=0;li<3 && can_run;li++) for(ci=0;ci<6 && can_run;ci++) {
  memset(&request,0,sizeof(request));
  request.lab_mode=0x300UL; request.lab_cmd=cm[ci]; request.lab_w=32; request.lab_h=8; request.lab_dx=4; request.lab_dy=4;
  request.lab_sx=0; request.lab_sy=0; request.lab_fg=fg; request.lab_bg=bg; request.lab_flags=4|1024;
  for(r=0;r<8;r++) {
   b4[0]=(unsigned char)(((0x80>>r)|0x01)&0xFF); b4[1]=0xAA; b4[2]=0x0F; b4[3]=(unsigned char)(r*0x11);
   request.lab_data[2*r]=(unsigned long)b4[0]|((unsigned long)b4[1]<<8)|((unsigned long)b4[2]<<16)|((unsigned long)b4[3]<<24);
   request.lab_data[2*r+1]=0;
  }
  request.lab_ndata=16;
  request.lab_reg[0][0]=0x08; request.lab_reg[0][1]=(320UL<<16)|((unsigned long)lc[li]<<12)|1UL;
  request.lab_reg[1][0]=0x1C; request.lab_reg[1][1]=0x00040200UL;
  request.lab_reg[2][0]=0x18; request.lab_reg[2][1]=0UL;
  request.lab_reg[3][0]=0x40; request.lab_reg[3][1]=0UL;
  request.lab_reg[4][0]=0x44; request.lab_reg[4][1]=0x01DF027FUL;
  request.lab_nregs=5;
  sprintf(status_text,"Mono source 2: %u ...",k+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("MS2 %u call failed: %s\r\n",k,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("MS2 %u result %lu fault %lu\r\n",k,reply.result,reply.fault_latched); can_run=0; break;}
  collect();
  {int tx=(int)(k%8)*64,ty=(int)(k/8)*64,px,py; for(py=0;py<64;py++) for(px=0;px<64;px++) sheet[(ty+py)*512+tx+px]=reply.image[py*64+px];}
  msb=lsb=chg=0;
  for(y=0;y<8;y++) for(x=0;x<32;x++) {
   int bm,bl; unsigned char byv;
   byv=(x/8==0)?(unsigned char)(((0x80>>y)|0x01)&0xFF):(x/8==1)?0xAA:(x/8==2)?0x0F:(unsigned char)(y*0x11);
   bm=(byv>>(7-(x%8)))&1; bl=(byv>>(x%8))&1;
   got=reply.image[(4+y)*64+4+x]; orig=dk_pre(4+x,4+y);
   if(got!=orig) chg++;
   if(got==(bm?fg:bg)) msb++;
   if(got==(bl?fg:bg)) lsb++;
  }
  { int ok=(!reply.sf_x_info[0][1] && (msb==256 || lsb==256)); if(ok) good++;
  sprintf(line,"MS2 %2u: loc %u %-20s: %s; changed %u of 256; match msb %u lsb %u of 256%s\r\n",k,lc[li],cn[ci],ok?"PASS":"FAIL",chg,msb,lsb,reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"");
  logtext(line); }
  k++;
 }
 save_sheet("MONOSRC2.BMP");
 logtext("BMP MONOSRC2.BMP written: 8 tiles per row at 1x, runs 0-17 in order (row 1 = 0-7, row 2 = 8-15, row 3 = 16-17).\r\n");
 sprintf(status_text,"Mono source sweep 2 done: %u of %u passed. Send me VX2DLAB.TXT and MONOSRC2.BMP.",good,k);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}

static unsigned long scr_pix(int x,int y) {
 if(x<64&&y<64) return 0xFFFF0000UL;
 if(x>=576&&y<64) return 0xFF00FF00UL;
 if(x<64&&y>=416) return 0xFF0000FFUL;
 if(x>=576&&y>=416) return 0xFFFFFF00UL;
 if(x>=320&&x<360&&y>=240&&y<280) return 0xFF00FFFFUL;
 if(x>=100&&x<120&&y>=100&&y<380) return 0xFFFFFFFFUL;
 if(x>=100&&x<300&&y>=100&&y<120) return 0xFFFFFFFFUL;
 if(x>=100&&x<240&&y>=230&&y<250) return 0xFFFFFFFFUL;
 if(x==0||y==0||x==639||y==479) return 0xFFFFFFFFUL;
 return 0xFF202020UL;
}
static void do_cbuscreen(void) {
 static const int pt[8][2]={{32,32},{607,32},{32,447},{607,447},{110,200},{200,110},{200,240},{330,250}};
 static const char *nm[3]={"180 (ctl 15, XP geometry 640x480)","90 (ctl 0D, swapped geometry guess)","270 (ctl 1D, swapped geometry guess)"};
 static const unsigned long ctl[3]={0x15UL,0x0DUL,0x1DUL};
 unsigned int v,k,s,i; char line[400];
 if(!can_run) return;
 logf2("\r\nCBU SCREEN HOLD v10.01: window programmed on the VISIBLE desktop (base 0, 640x480x32). Test picture painted by the CPU: red/green/blue/yellow 64x64 corner blocks (TL/TR/BL/BR), white F-shape at 100,100, cyan block at 320,240, white 1px border. The driver HOLDS the window ON for 8 seconds, then reads 8 sample pixels with the CPU, 2D-copies the desktop to a second surface and reads the same samples, disables the window, reads again, and restores the desktop. SET A = CPU reads while ON, SET B = reads of the 2D copy made while ON (copy B lives at 2.3 MB inside the 4 MB CPU-visible mapping; the backup lives at 4 MB where only the 2D engine can reach it), SET C = CPU reads after OFF, SET D = control (destination samples prefilled with sentinels DEAD0000+n, so a value like DEAD000n means the copy never wrote there): 2D copy to B with the window OFF, SET E = CPU reads of the desktop after the restore copy. id = pixels equal to the unrotated picture, r180 = equal to the picture rotated 180, mx/my = mirrored in X / Y.\r\n");
 for(v=0;v<3 && can_run;v++) {
  memset(&request,0,sizeof(request));
  request.lab_flags=4096UL; request.lab_bg=8000UL; request.lab_sx=0x400000UL; request.lab_sy=0x250000UL;
  request.lab_reg[0][0]=0x1E04; request.lab_reg[0][1]=0;
  request.lab_reg[1][0]=0x1E08; request.lab_reg[1][1]=0x0012C000UL;
  request.lab_reg[2][0]=0x1E0C; request.lab_reg[2][1]=(v==0)?0x1999UL:0x2222UL;
  request.lab_reg[3][0]=0x1E10; request.lab_reg[3][1]=(v==0)?0x02800280UL:0x01E001E0UL;
  request.lab_reg[4][0]=0x1E14; request.lab_reg[4][1]=(v==0)?0x01E00280UL:0x028001E0UL;
  request.lab_reg[5][0]=0x1E00; request.lab_reg[5][1]=ctl[v];
  request.lab_nregs=6;
  sprintf(status_text,"CBU screen %u of 3: window %s is going ON. LOOK AT THE REAL MONITOR for 8 seconds (the screen shows a test picture; note or photograph what it looks like). Windows will freeze for those 8 seconds.",v+1,nm[v]);
  InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  logf2("RUN %u: window %s\r\n",v+1,nm[v]);
  if(!call_lab()) {logf2("call failed: %s\r\n",status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("result %lu fault %lu\r\n",reply.result,reply.fault_latched); can_run=0; break;}
  logf2("  engine flags (1 = backup copy hung, 2 = copy B hung, 4 = restore hung, 8 = control copy hung): %lu\r\n",(unsigned long)reply.sf_x_info[0][1]);
  for(i=0;i<6;i++) logf2("  %04lX wrote %08lX read %08lX\r\n",request.lab_reg[i][0],request.lab_reg[i][1],(unsigned long)reply.sf_x_cls[0][i]);
  {unsigned int pk; static const unsigned long pa[6]={0x300000UL,0x400000UL,0x500000UL,0x600000UL,0x800000UL,0x1000000UL};
   logtext("  aperture probe (addr: wrote/read):");
   for(pk=0;pk<6;pk++) logf2(" %lX:%08lX/%08lX",pa[pk],0xA5000000UL|(pa[pk]>>8),(unsigned long)reply.image[40+pk]);
   logtext("\r\n");}
  for(s=0;s<5;s++) {
   unsigned int id=0,r180=0,mx=0,my=0; char *p=line;
   for(k=0;k<8;k++) {
    unsigned long g=reply.image[(s==3?32:s==4?48:s*8)+k]; int x=pt[k][0],y=pt[k][1];
    if(g==scr_pix(x,y)) id++;
    if(g==scr_pix(639-x,479-y)) r180++;
    if(g==scr_pix(639-x,y)) mx++;
    if(g==scr_pix(x,479-y)) my++;
   }
   p+=sprintf(p,"  SET %c:",s==3?'D':s==4?'E':'A'+s);
   for(k=0;k<8;k++) p+=sprintf(p," %08lX",(unsigned long)reply.image[(s==3?32:s==4?48:s*8)+k]);
   sprintf(p,"  id %u/8 r180 %u/8 mx %u/8 my %u/8",id,r180,mx,my);
   logtext(line); logtext("\r\n");
  }
 }
 sprintf(status_text,"CBU screen hold done. Send me VX2DLAB.TXT and tell me what the MONITOR showed during each 8 second hold (photo if you can).");
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}


static void cls_word(unsigned long w,char *c) {
 if((w>>24)==0x3C && (w&0xFFFFFFUL)<1024UL) sprintf(c,"VRAM k=%lu off %lu",w&0xFFFFFFUL,(w&0xFFFFFFUL)*4);
 else if((w>>24)==0x81 && (w&0xFFFFFFUL)<8UL) sprintf(c,"HOST j=%lu",w&0xFFFFFFUL);
 else if((w>>24)==0xAA && (w&0xFFFFFFUL)<8UL) sprintf(c,"STALE-OLD-HOST j=%lu",w&0xFFFFFFUL);
 else strcpy(c,"unmatched");
}

static void do_vramreach(void) {
 vx_status_t s; DWORD got=0; unsigned int k,j,ndst,nprobe; char line[300];
 if(!can_run) return;
 memset(&s,0,sizeof(s));
 if(!DeviceIoControl(device,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||got!=sizeof(s)||s.magic!=VX_MAGIC) {logf2("VRAM REACH: status ioctl failed\r\n"); return;}
 logf2("\r\nVRAM REACH v10.05: how much video memory can the CPU and the 2D engine really reach?\r\nPhase A (CPU): sentinel dwords at offsets from 4 MiB to the last page of the mapped window; the original contents are restored. Phase B (2D engine): copy a 32x8 block (32bpp, pitch 2560, source 0x250000) to a destination at each offset, then the CPU verifies it; if it did not arrive, the six wrap-around addresses dest&(2^b-1), b=22..27, are searched.\r\n");
 memset(&request,0,sizeof(request)); request.lab_flags=524288UL; request.lab_sx=s.fb_bytes;
 sprintf(status_text,"VRAM reach test running ..."); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
 if(!call_lab()) {logf2("VRAM REACH call failed: %s\r\n",status_text); can_run=0; return;}
 logf2("Sizes: VBE TotalMemory %lu bytes; host-bridge register 0xA1 says %lu bytes; mapping asked for %lu; mapped window (status fb_bytes) %lu bytes (%lu MiB); PCI BAR0 %08lX, fb_phys %08lX.\r\n",
  (unsigned long)reply.sf_x_info[5][0],(unsigned long)reply.sf_x_info[5][1],(unsigned long)reply.sf_x_info[5][2],(unsigned long)reply.sf_x_info[5][3],(unsigned long)reply.sf_x_info[5][3]>>20,s.bar0,s.fb_phys);
 if(reply.result!=VX3D_PASS) {logf2("VRAM REACH: driver result %lu (fault latched %lu)\r\n",(unsigned long)reply.result,(unsigned long)reply.fault_latched); if(reply.result==VX3D_TIMEOUT||reply.result==VX3D_DISABLED) can_run=0; }
 nprobe=(unsigned int)reply.image[0]; ndst=(unsigned int)reply.image[2];
 if(!nprobe) {logf2("Mapped window is not larger than 4 MiB: nothing to test.\r\n"); sprintf(status_text,"VRAM reach: window not widened."); InvalidateRect(hmain,NULL,TRUE); return;}
 logf2("PHASE A, CPU sentinels: %lu of %u offsets read back correctly.\r\n",(unsigned long)reply.image[1],nprobe);
 for(k=0;k<nprobe && k<16;k++) {
  sprintf(line,"  A%02u offset %08lX (%lu MiB): dword %08lX, next %08lX, expected %08lX/%08lX  %s\r\n",k,(unsigned long)reply.image[16+4*k],(unsigned long)reply.image[16+4*k]>>20,(unsigned long)reply.image[17+4*k],(unsigned long)reply.image[18+4*k],0xC0DE0000UL|k,~(0xC0DE0000UL|k),reply.image[19+4*k]?"OK":"FAIL");
  logtext(line);
 }
 logf2("PHASE B, 2D engine copy: %lu of %u destinations received the full block%s.\r\n",(unsigned long)reply.image[3],ndst,reply.image[4]?" (ENGINE HUNG, test stopped)":"");
 for(j=0;j<ndst && j<14;j++) {
  static const char *sn[4]={"full block","partial","not found","HUNG"};
  unsigned long st=reply.image[97+8*j];
  sprintf(line,"  B%02u dest %08lX (%lu MiB): %s, %lu of 256 dwords, first dword %08lX",j,(unsigned long)reply.image[96+8*j],(unsigned long)reply.image[96+8*j]>>20,st<4?sn[st]:"?",(unsigned long)reply.image[98+8*j],(unsigned long)reply.image[99+8*j]);
  logtext(line);
  if(reply.image[100+8*j]) {
   unsigned int bb; sprintf(line,"; copy found at wrap address %08lX (up to %lu dwords) for dest&(2^b-1), b =",(unsigned long)reply.image[102+8*j],(unsigned long)reply.image[101+8*j]); logtext(line);
   for(bb=22;bb<=27;bb++) if(reply.image[100+8*j]&(1UL<<bb)) {sprintf(line," %u",bb); logtext(line);}
  }
  logtext("\r\n");
 }
 sprintf(status_text,"VRAM reach done: CPU %lu/%u, engine %lu/%u. Send me VX2DLAB.TXT.",(unsigned long)reply.image[1],nprobe,(unsigned long)reply.image[3],ndst);
 logtext(status_text); logtext("\r\n"); InvalidateRect(hmain,NULL,TRUE);
}

static void route_edid(const unsigned char *e) {
 unsigned int i,sum=0,m; char name[16]; unsigned long pc;
 for(i=0;i<128;i++) sum+=e[i];
 if(e[0]!=0||e[1]!=0xFF||e[7]!=0) {logtext("    EDID header is not valid.\r\n"); return;}
 m=((unsigned)e[8]<<8)|e[9];
 logf2("    EDID: manufacturer %c%c%c, product %04X, serial %02X%02X%02X%02X, EDID %u.%u, checksum %s, extension blocks %u\r\n",
  '@'+((m>>10)&31),'@'+((m>>5)&31),'@'+(m&31),e[10]|(e[11]<<8),e[15],e[14],e[13],e[12],e[18],e[19],(sum&255)?"BAD":"ok",e[126]);
 logf2("    input: %s%s\r\n",(e[20]&0x80)?"DIGITAL":"ANALOG",(e[20]&0x80)?"":((e[20]&0x10)?" (blank-to-black)":""));
 pc=(unsigned long)(e[54]|(e[55]<<8))*10UL;
 if(pc) logf2("    first timing: %lu.%02lu MHz, %ux%u active\r\n",pc/1000UL,(pc%1000UL)/10UL,e[56]|((e[58]&0xF0)<<4),e[59]|((e[61]&0xF0)<<4));
 for(i=0;i<4;i++) {const unsigned char *d=e+54+18*i; if(d[0]==0&&d[1]==0&&d[3]==0xFC) {int k; for(k=0;k<13&&d[5+k]!=0x0A&&d[5+k]!=0;k++) name[k]=(char)d[5+k]; name[k]=0; logf2("    monitor name: %s\r\n",name);}}
}
static void do_routing(void) {
 static const char *bn[3]={"bus 1 (SR26, usually the VGA DDC)","bus 2 (SR31, DVI DDC / TMDS)","bus 3 (SR2C)"};
 vx_status_t s; DWORD got=0; unsigned int i,b,k; char line[200];
 if(!can_run) return;
 memset(&s,0,sizeof(s)); DeviceIoControl(device,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL);
 logf2("\r\nROUTING DUMP v10.13: read-only look at the display routing, DACs, and I2C buses. No video memory and no 3D engine are used.\r\n");
 memset(&request,0,sizeof(request)); request.lab_flags=1048576UL;
 sprintf(status_text,"Routing dump running ..."); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
 if(!call_lab()) {logf2("ROUTING call failed: %s\r\n",status_text); return;}
 lb_begin();
 if(reply.result!=VX3D_PASS||reply.image[0]!=1) {lbon=0; logf2("ROUTING: driver result %lu, format %lu\r\n",(unsigned long)reply.result,(unsigned long)reply.image[0]); return;}
 logf2("Memory: BIOS frame-buffer setting (host bridge reg) %lu MiB; VBE TotalMemory %lu MiB; mapped window %lu MiB; mapping asked %lu MiB.\r\n",
  (unsigned long)reply.image[11]>>20,(unsigned long)reply.image[12]>>20,(unsigned long)reply.image[10]>>20,(unsigned long)reply.image[13]>>20);
 logf2("Desktop (IGA1) now: %lux%lu, %lu bpp, pitch %lu; scanout span %08lX..%08lX; PCI id %08lX; misc output %02lX (CRTC at %03lX); SR10 found %02lX\r\n",
  (unsigned long)s.width,(unsigned long)reply.image[6],(unsigned long)reply.image[7],(unsigned long)reply.image[5],(unsigned long)reply.image[8],(unsigned long)reply.image[9],(unsigned long)reply.image[4],(unsigned long)reply.image[1],(unsigned long)reply.image[2],(unsigned long)reply.image[3]);
 {
  const uint32_t *I=reply.image; unsigned long sr44=I[16+0x44],sr45=I[16+0x45],sr46=I[16+0x46],sr4a=I[16+0x4A],sr4b=I[16+0x4B],sr4c=I[16+0x4C];
  unsigned long pdm=sr44|((sr45&3)<<8),pdr=(sr45>>2)&7,pdn=sr46&0x7F,sdm=sr4a|((sr4b&3)<<8),sdr=(sr4b>>2)&7,sdn=sr4c&0x7F;
  logf2("Routing state (decoded with the OpenChrome register map):\r\n");
  logf2("  analog DAC %s (CR47[2]=%lu); analog source IGA%lu (SR16[6]=%lu)\r\n",(I[272+0x47]&4)?"OFF":"on",(I[272+0x47]>>2)&1,((I[16+0x16]>>6)&1)+1,(I[16+0x16]>>6)&1);
  logf2("  DVP1 pad SR1E[5:4]=%lu (3 = on); DVP1 source IGA%lu (CR9B[4]=%lu); DVP1 sync polarity CR9B[6:5]=%lu (1 = negative: bit5 HSYNC, bit6 VSYNC); CR9B[3:0] adjust %lu\r\n",(I[16+0x1E]>>4)&3,((I[272+0x9B]>>4)&1)+1,(I[272+0x9B]>>4)&1,(I[272+0x9B]>>5)&3,I[272+0x9B]&15);
  logf2("  integrated TMDS power-down CRD2[3]=%lu; its source CR99[4]=%lu; CR97 %02lX; DVP1 drive SR65 %02lX\r\n",(I[272+0xD2]>>3)&1,(I[272+0x99]>>4)&1,(unsigned long)I[272+0x97],(unsigned long)I[16+0x65]);
  logf2("  IGA2: CR6A %02lX (enable bit7=%lu, bit6=%lu), CR6B %02lX (screen-off bit2=%lu), CR6C %02lX, CR79 scaling bit0=%lu\r\n",(unsigned long)I[272+0x6A],(I[272+0x6A]>>7)&1,(I[272+0x6A]>>6)&1,(unsigned long)I[272+0x6B],(I[272+0x6B]>>2)&1,(unsigned long)I[272+0x6C],I[272+0x79]&1);
  logf2("  gated clocks SR1B %02lX, PLL power SR2D %02lX\r\n",(unsigned long)I[16+0x1B],(unsigned long)I[16+0x2D]);
  if(pdn&&sdn) logf2("  dot clocks: primary PLL (SR44-46) %lu.%03lu MHz, secondary/IGA2 PLL (SR4A-4C) %lu.%03lu MHz\r\n",14318UL*pdm/(pdn<<pdr)/1000UL,14318UL*pdm/(pdn<<pdr)%1000UL,14318UL*sdm/(sdn<<sdr)/1000UL,14318UL*sdm/(sdn<<sdr)%1000UL);
  logf2("  strap view (SR5A[0]=1): SR12 %02lX, SR13 %02lX (SR13[7:6]=%lu)\r\n",(unsigned long)I[12+2],(unsigned long)I[15],(unsigned long)((I[15]>>6)&3));
 }
 logtext("SR00..FF:\r\n");
 for(i=0;i<256;i+=16) {sprintf(line,"  %02X:",i); logtext(line); for(k=0;k<16;k++) {sprintf(line," %02lX",(unsigned long)reply.image[16+i+k]&0xFF); logtext(line);} logtext("\r\n");}
 logtext("CR00..FF:\r\n");
 for(i=0;i<256;i+=16) {sprintf(line,"  %02X:",i); logtext(line); for(k=0;k<16;k++) {sprintf(line," %02lX",(unsigned long)reply.image[272+i+k]&0xFF); logtext(line);} logtext("\r\n");}
 logtext("PCI config of the VX855 graphics function (dwords 00..FC):\r\n");
 for(i=0;i<64;i+=8) {sprintf(line,"  %02X:",i*4); logtext(line); for(k=0;k<8;k++) {sprintf(line," %08lX",(unsigned long)reply.image[528+i+k]); logtext(line);} logtext("\r\n");}
 for(b=0;b<3;b++) {
  const uint32_t *o=reply.image+600+200*b; unsigned char e[128]; unsigned int n;
  logf2("I2C %s: lines idle at SCL=%lu SDA=%lu%s\r\n",bn[b],(unsigned long)o[0]&1,((unsigned long)o[0]>>1)&1,(o[0]==3)?"":"  (a line is stuck low: bus not usable)");
  if(o[0]!=3) continue;
  n=(unsigned)o[1]; logf2("  devices answering: %u:",n); for(k=0;k<n&&k<32;k++) logf2(" %02lX(w %02lX)",(unsigned long)o[2+k],(unsigned long)o[2+k]*2); logtext("\r\n");
  if(o[35]==0) {for(k=0;k<32;k++) {e[4*k]=(unsigned char)o[40+k]; e[4*k+1]=(unsigned char)(o[40+k]>>8); e[4*k+2]=(unsigned char)(o[40+k]>>16); e[4*k+3]=(unsigned char)(o[40+k]>>24);}
   logtext("  EDID read OK (128 bytes):\r\n"); route_edid(e);
   for(k=0;k<128;k+=16) {unsigned int j; sprintf(line,"      %02X:",k); logtext(line); for(j=0;j<16;j++) {sprintf(line," %02X",e[k+j]); logtext(line);} logtext("\r\n");}
  } else logf2("  EDID: %s\r\n",o[35]==1?"nothing at 0x50 (no monitor DDC on this bus)":"0x50 answered but the read failed");
  if(o[80]) {logf2("  VT1632 (7-bit addr 0x08, 8-bit 0x10) present, regs 00..0F:"); for(k=0;k<16;k++) logf2(" %02lX",(unsigned long)o[81+k]&0xFF); logf2("   [vendor %04lX device %04lX; expect 1106/3192; reg09 bit2 = monitor sense %lu]\r\n",(unsigned long)((o[81]&0xFF)|((o[82]&0xFF)<<8)),(unsigned long)((o[83]&0xFF)|((o[84]&0xFF)<<8)),((unsigned long)o[90]>>2)&1);}
  else logtext("  VT1632 (7-bit 0x08): no answer\r\n");
  if(o[100]) {logtext("  SiI164-type TMDS (addr 0x38) present, regs 00..07:"); for(k=0;k<8;k++) logf2(" %02lX",(unsigned long)o[101+k]&0xFF); logtext("\r\n");}
 }
 sprintf(status_text,"Routing dump done. Send me VX2DLAB.TXT."); logtext(status_text); logtext("\r\n"); lb_end(); InvalidateRect(hmain,NULL,TRUE);
}

static void pump_wait(unsigned ms) {
 MSG m; DWORD end=GetTickCount()+ms;
 while((long)(GetTickCount()-end)<0) {
  while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);}
  Sleep(50);
 }
}
static void do_dvitest(void) {
 static const char *vn[4]={"1: monitor native mode (from its EDID), FIFO per OpenChrome","2: monitor native mode with the BIOS FIFO values (for comparison)","3: monitor native mode, FIFO per OpenChrome, both sync polarities inverted","4: 1280x720 60 Hz (the monitor scales it), FIFO per OpenChrome"};
 static const unsigned long vt[4]={0xB7FUL,0x37FUL,0x3B7FUL,0xB7FUL};
 static const unsigned long vv[4]={5UL,5UL,5UL,1UL};
 unsigned int v,k; char line[300]; SYSTEMTIME t;
 if(!can_run) return;
 logf2("\r\nDVI TEST v10.17: the second display controller (IGA2) shows a colour-bar picture on the digital output (DVP1, VT1632 transmitter) while the VGA desktop stays on IGA1; 12 seconds per variant, everything restored after each. The mode is the monitor's own preferred timing, read from its EDID. The IGA2 display FIFO is set to the OpenChrome VX855 values (the BIOS values were smaller; variant 2 keeps them for comparison).\r\nPicture: 8 colour bars, grey ramp, 1-pixel black/white stripes (left) and solid grey (right) at the bottom, 1-pixel white border, corner squares red (top-left), green (top-right), blue (bottom-left), white (bottom-right).\r\n");
 for(v=1;v<=4;v++) {
  memset(&request,0,sizeof(request)); request.lab_flags=2097152UL; request.lab_sy=vv[v-1]; request.lab_dx=vt[v-1];
  sprintf(status_text,"DVI test variant %u: setting up ...",v); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("DVI TEST call failed: %s\r\n",status_text); can_run=0; return;}
  GetLocalTime(&t);
  if(reply.result!=VX3D_PASS||reply.image[0]!=1) {
   logf2("Variant %u NOT applied: result %lu status %lu (2 = memory too small, 3 = memory check failed, 5 = monitor EDID unreadable or mode unusable); usable memory %lu MiB\r\n",v,(unsigned long)reply.result,(unsigned long)reply.image[0],(unsigned long)reply.image[2]>>20);
   sprintf(status_text,"DVI test stopped: variant %u not applied (status %lu).",v,(unsigned long)reply.image[0]); InvalidateRect(hmain,NULL,TRUE); return;
  }
  logf2("Variant %s applied at %02u:%02u:%02u.%03u; picture at %08lX (usable memory %lu MiB)\r\n",vn[v-1],t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,(unsigned long)reply.image[1],(unsigned long)reply.image[2]>>20);
  if(reply.image[190]) logf2("  native mode from the monitor EDID: %lu.%02lu MHz, %lux%lu (htotal %lu, hsync %lu-%lu, vtotal %lu, vsync %lu-%lu), sync %s%s; PLL dm %lu dn %lu dr %lu = %lu.%03lu MHz\r\n",(unsigned long)reply.image[191]/1000UL,((unsigned long)reply.image[191]%1000UL)/10UL,(unsigned long)reply.image[192],(unsigned long)reply.image[196],(unsigned long)reply.image[195],(unsigned long)reply.image[193],(unsigned long)reply.image[194],(unsigned long)reply.image[199],(unsigned long)reply.image[197],(unsigned long)reply.image[198],(reply.image[200]&1)?"H-":"H+",(reply.image[200]&2)?" V-":" V+",(unsigned long)reply.image[201],(unsigned long)reply.image[202],(unsigned long)reply.image[203],(unsigned long)reply.image[204]/1000UL,(unsigned long)reply.image[204]%1000UL);
  logtext("  CR before:"); for(k=0;k<35;k++) {sprintf(line," %02lX",(unsigned long)reply.image[100+k]&0xFF); logtext(line);}
  logtext("\r\n  CR after: "); for(k=0;k<35;k++) {sprintf(line," %02lX",(unsigned long)reply.image[16+k]&0xFF); logtext(line);}
  if(reply.image[149]&1) {
   logf2("\r\n  VT1632 regs 00..0F before:"); for(k=0;k<16;k++) logf2(" %02lX",(unsigned long)reply.image[170+k]&0xFF);
   logf2("\r\n  VT1632 regs 00..0F after init (%s):",(reply.image[149]&2)?"writes acknowledged":"WRITES NOT ACKNOWLEDGED"); for(k=0;k<16;k++) logf2(" %02lX",(unsigned long)reply.image[150+k]&0xFF);
   logf2("\r\n  VT1632 monitor sense (reg 09 bit 2) before %lu, after %lu; reg 08 bit 0 (power) before %lu, after %lu",((unsigned long)reply.image[170+9]>>2)&1,((unsigned long)reply.image[150+9]>>2)&1,(unsigned long)reply.image[170+8]&1,(unsigned long)reply.image[150+8]&1);
  } else logf2("\r\n  VT1632 did NOT answer on I2C bus 2");
  logtext("\r\n  (CR list: 50-5F 62 63 64 65 66 67 6A 6B 6C 79 97 99 9B A3 D2 68 92 94 95)   SR before 16/1B/1E/2D/4A/4B/4C/65:"); for(k=0;k<8;k++) {sprintf(line," %02lX",(unsigned long)reply.image[140+k]&0xFF); logtext(line);}
  logtext("   after:");
  for(k=0;k<8;k++) {sprintf(line," %02lX",(unsigned long)reply.image[64+k]&0xFF); logtext(line);}
  logtext("\r\n");
  sprintf(status_text,"DVI TEST variant %u of 4 is ON for 12 s: look at the digital output (capture) NOW. The VGA desktop should stay normal.",v);
  InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain); MessageBeep(MB_ICONASTERISK);
  pump_wait(12000);
  memset(&request,0,sizeof(request)); request.lab_flags=4194304UL;
  if(!call_lab()) {logf2("DVI TEST restore call failed: %s\r\n",status_text); can_run=0; return;}
  GetLocalTime(&t);
  logf2("Variant %u restored at %02u:%02u:%02u.%03u (result %lu)\r\n",v,t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,(unsigned long)reply.result);
  sprintf(status_text,"Variant %u restored.",v); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  pump_wait(2500);
 }
 sprintf(status_text,"DVI test finished. Tell me which variant (if any) showed a picture on the digital output, and send VX2DLAB.TXT.");
 logtext(status_text); logtext("\r\n"); InvalidateRect(hmain,NULL,TRUE);
}

/* v10.16: live tuning of the digital output. Left/Right select a parameter, Up/Down change it (applied at once), G/B log "good"/"bad", Esc restores everything. */
static int tune_on,tune_sel;
static const char *tp_name[11]={"mode (1=720p 2=1080p 3=monitor native)","DVP1 adjust CR9B[3:0]","clock drive SR65[3:2]","data drive SR65[1:0]","VT1632 EDGE","VT1632 DSEL","VT1632 BSEL","OpenChrome FIFO","HSYNC inverted","VSYNC inverted","PLL dtz = 3"};
static int tp_val[11]={3,15,3,1,1,1,0,1,0,0,0};
static const int tp_max[11]={3,15,3,3,1,1,1,1,1,1,1};
static const int tp_min[11]={1,0,0,0,0,0,0,0,0,0,0};
static unsigned long tune_bits(void) {
 return (unsigned long)tp_val[1]|((unsigned long)tp_val[2]<<4)|((unsigned long)tp_val[3]<<6)|((unsigned long)tp_val[4]<<8)|((unsigned long)tp_val[5]<<9)|((unsigned long)tp_val[6]<<10)|((unsigned long)tp_val[7]<<11)|((unsigned long)tp_val[8]<<12)|((unsigned long)tp_val[9]<<13)|((unsigned long)tp_val[10]<<14);
}
static void tune_status(const char *note) {
 char b[1500]; int i; unsigned n=0;
 n+=sprintf(b+n,"DVI TUNE %s  (Left/Right = select, Up/Down = change, G = log GOOD, B = log BAD, Esc = restore and exit)\r\n",note);
 for(i=0;i<11;i++) n+=sprintf(b+n,"%s%s = %d\r\n",i==tune_sel?"> ":"  ",tp_name[i],tp_val[i]);
 strcpy(status_text,b); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
}
static void tune_apply(int full) {
 SYSTEMTIME t;
 memset(&request,0,sizeof(request)); request.lab_flags=2097152UL; request.lab_sy=(unsigned long)(tp_val[0]==3?5:tp_val[0]);
 request.lab_dx=tune_bits()|(full?0UL:(1UL<<15))|(1UL<<16);
 if(!call_lab()||reply.result!=VX3D_PASS||reply.image[0]!=1) {
  logf2("TUNE apply failed (call result %lu status %lu)\r\n",(unsigned long)reply.result,(unsigned long)reply.image[0]); tune_on=0; tune_status("STOPPED (apply failed)"); return;
 }
 GetLocalTime(&t);
 logf2("TUNE %02u:%02u:%02u.%03u %s: mode %d adj %d clk %d data %d edge %d dsel %d bsel %d fifo %d hneg %d vneg %d dtz3 %d\r\n",t.wHour,t.wMinute,t.wSecond,t.wMilliseconds,full?"full":"light",tp_val[0],tp_val[1],tp_val[2],tp_val[3],tp_val[4],tp_val[5],tp_val[6],tp_val[7],tp_val[8],tp_val[9],tp_val[10]);
}
static void tune_stop(void) {
 if(!tune_on) return;
 tune_on=0;
 memset(&request,0,sizeof(request)); request.lab_flags=4194304UL; (void)call_lab();
 logf2("TUNE ended and restored.\r\n"); tune_status("ENDED (registers restored)");
}
static void do_tune(void) {
 if(!can_run) return;
 if(tune_on) {tune_stop(); return;}
 logf2("\r\nDVI TUNE v10.17 started. Keys: Left/Right select, Up/Down change, G good, B bad, Esc end.\r\n");
 tune_on=1; tune_sel=0; tune_status("starting ...");
 tune_apply(1);
 if(tune_on) tune_status("running"); 
 SetFocus(hmain);
}
static void tune_key(WPARAM k) {
 int fullchg=0;
 if(!tune_on) return;
 if(k==VK_ESCAPE) {tune_stop(); return;}
 if(k==VK_LEFT) {tune_sel=(tune_sel+10)%11; tune_status("running"); return;}
 if(k==VK_RIGHT) {tune_sel=(tune_sel+1)%11; tune_status("running"); return;}
 if(k=='G'||k=='B') {logf2("TUNE MARK %s: mode %d adj %d clk %d data %d edge %d dsel %d bsel %d fifo %d hneg %d vneg %d dtz3 %d\r\n",k=='G'?"GOOD":"BAD",tp_val[0],tp_val[1],tp_val[2],tp_val[3],tp_val[4],tp_val[5],tp_val[6],tp_val[7],tp_val[8],tp_val[9],tp_val[10]); tune_status(k=='G'?"marked GOOD":"marked BAD"); return;}
 if(k==VK_UP||k==VK_DOWN) {
  int v=tp_val[tune_sel]+(k==VK_UP?1:-1);
  if(v>tp_max[tune_sel]) v=tp_min[tune_sel]; if(v<tp_min[tune_sel]) v=tp_max[tune_sel];
  tp_val[tune_sel]=v;
  fullchg=(tune_sel==0||tune_sel==10);
  tune_status("applying ...");
  tune_apply(fullchg);
  if(tune_on) tune_status("running");
 }
}
static void do_scan_log(const char *when) {
 char line[600]; unsigned int i;
 memset(&request,0,sizeof(request)); request.lab_flags=131072UL;
 if(!call_lab() || reply.result!=VX3D_PASS) {logf2("scan failed\r\n"); return;}
 sprintf(line,"SCAN %s: 81-tag words %lu",when,(unsigned long)reply.image[0]); logtext(line);
 for(i=0;i<24 && i<reply.image[0];i++) {sprintf(line," %lX",(unsigned long)reply.image[1+i]); logtext(line);}
 sprintf(line,"; AA-tag words %lu",(unsigned long)reply.image[32]); logtext(line);
 for(i=0;i<24 && i<reply.image[32];i++) {sprintf(line," %lX",(unsigned long)reply.image[33+i]); logtext(line);}
 logtext("\r\n");
}
static void do_monosrc4(void) {
 /* v10.01: unique-word decode. Source area dword k = 0x55000000|k, host stream word j = 0xAA000000|j. Each output row shows the 32 bits actually fetched. */
 static const unsigned long cmds[8]={0xCC000101UL,0xCC000101UL,0xCC0A0141UL,0xCC000101UL,0xCC000101UL,0xCC000101UL,0xCC000101UL,0xCC000101UL};
 static const unsigned long pit[8]={32,32,32,32,0,1,2,64};
 unsigned int k,x,y,i,px; unsigned long fg=0xFFFF8000UL,bg=0xFF004080UL,got; char line[500];
 if(!can_run) return;
 for(i=0;i<512*512;i++) sheet[i]=0xFF000000UL;
 logf2("\r\nMONO SOURCE SWEEP 11 v10.01: STEPPED HOST FEED. Same setup, tags and decode as the palindrome run (VRAM word k = 3C000000|k, host word j = 81000000|j), but the host stream is written ONE DWORD AT A TIME: status register is read after the command (before any data) and after every host dword, and feeding STOPS the moment the engine reports idle. This shows how many host dwords release each command (the control CC0A0141 should need all 8) and whether the screen-source command is released early with surplus words never sent. Rows of the output are decoded as before.\r\n");
 for(k=0;k<8 && can_run;k++) {
  memset(&request,0,sizeof(request));
  request.lab_mode=0x300UL; request.lab_cmd=cmds[k]; request.lab_w=32; request.lab_h=8; request.lab_dx=4; request.lab_dy=4;
  request.lab_sx=256; request.lab_sy=0; request.lab_fg=fg; request.lab_bg=bg; request.lab_flags=4|1|8192|32768UL|65536UL|262144UL;
  for(y=0;y<8;y++) {request.lab_data[2*y]=0x81000000UL|y; request.lab_data[2*y+1]=0;}
  request.lab_ndata=16;
  request.lab_reg[0][0]=0x08; request.lab_reg[0][1]=(320UL<<16)|pit[k];
  request.lab_reg[1][0]=0x1C; request.lab_reg[1][1]=0x00040200UL;
  request.lab_reg[2][0]=0x18; request.lab_reg[2][1]=0UL;
  request.lab_reg[3][0]=0x40; request.lab_reg[3][1]=0UL;
  request.lab_reg[4][0]=0x44; request.lab_reg[4][1]=0x01DF027FUL;
  request.lab_nregs=5;
  sprintf(status_text,"Mono source 8: %u ...",k+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("MS8 %u call failed: %s\r\n",k,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("MS8 %u result %lu fault %lu\r\n",k,reply.result,reply.fault_latched); can_run=0; break;}
  collect();
  {int tx=(int)(k%8)*64,ty=(int)(k/8)*64,qx,qy; for(qy=0;qy<64;qy++) for(qx=0;qx<64;qx++) sheet[(ty+qy)*512+tx+qx]=reply.image[qy*64+qx];}
  sprintf(line,"MS8 %u: %08lX pitch %lu%s",k,cmds[k],pit[k],reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG reset FAILED"):" finished");
  logtext(line);
  if(reply.sf_x_info[0][1]) {sprintf(line,"; st %08lX why %lu r0C %08lX r10 %08lX",(unsigned long)reply.sf_x_info[1][0],(unsigned long)reply.sf_x_info[1][1],(unsigned long)reply.sf_x_info[1][3],(unsigned long)reply.sf_x_info[1][4]); logtext(line);}
  {unsigned int st; sprintf(line,"   STEPPED FEED: status after command %08lX; after host dword 0..7:",(unsigned long)reply.sf_x_cls[1][8]); logtext(line); for(st=0;st<8 && st<reply.sf_x_info[2][0];st++) {sprintf(line," %08lX",(unsigned long)reply.sf_x_cls[1][st]); logtext(line);} sprintf(line,"; dwords fed %lu, idle reached %s\r\n",(unsigned long)reply.sf_x_info[2][0],reply.sf_x_info[2][1]?"YES (feeding stopped there)":"no"); logtext(line);}
  logtext("\r\n");
  if(!reply.sf_x_info[0][1]) for(y=0;y<8;y++) {
   unsigned char mb[4],lb[4]; unsigned int other=0;
   for(i=0;i<4;i++) {mb[i]=0; lb[i]=0;}
   for(x=0;x<32;x++) {
    int bit=0; got=reply.image[(4+y)*64+4+x];
    if(got==fg) bit=1; else if(got!=bg) other++;
    px=x%8; if(bit) {mb[x/8]|=(unsigned char)(0x80>>px); lb[x/8]|=(unsigned char)(1<<px);}
   }
   {unsigned long wm=(unsigned long)mb[0]|((unsigned long)mb[1]<<8)|((unsigned long)mb[2]<<16)|((unsigned long)mb[3]<<24);
    unsigned long wl=(unsigned long)lb[0]|((unsigned long)lb[1]<<8)|((unsigned long)lb[2]<<16)|((unsigned long)lb[3]<<24);
    char cm[60],cl[60];
    cls_word(wm,cm); cls_word(wl,cl);
    sprintf(line,"   row %u: msb %08lX %s | lsb %08lX %s%s\r\n",y,wm,cm,wl,cl,other?" (non fg/bg pixels present)":"");}
   logtext(line);
  }
 }
 save_sheet("MONOSRC9.BMP");
 logtext("BMP MONOSRC9.BMP written: runs 0-7 in order, one tile each.\r\n");
 sprintf(status_text,"Mono source sweep 8 done. Send me VX2DLAB.TXT.");
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}

static int tr_bit(const unsigned long *bm,int y,int x) {return (int)((((bm[y]>>((x/8)*8))&0xFFUL)>>(7-(x%8)))&1UL);}
/* destination box pixel (x,y) of hypothesis h: 0 none 1 r0 2 r180 3 r90cw 4 r270cw 5 transpose 6 anti-transpose */
static int tr_hyp(const unsigned long *bm,int h,int x,int y,int *bw,int *bh) {
 switch(h) {
  case 0: *bw=32;*bh=8; return tr_bit(bm,y,x);
  case 1: *bw=32;*bh=8; return tr_bit(bm,7-y,31-x);
  case 2: *bw=8;*bh=32; return tr_bit(bm,7-x,y);
  case 3: *bw=8;*bh=32; return tr_bit(bm,x,31-y);
  case 4: *bw=8;*bh=32; return tr_bit(bm,x,y);
  default: *bw=8;*bh=32; return tr_bit(bm,7-x,31-y);
 }
}
static void do_textrot(void) {
 /* v10.02: confirm run for rotated mono text (op A). First three rungs = unrotated control, opaque 180, XP exact 180; then 90/270 exploration (hang-prone, last). Every tile pixel is checked. */
 typedef struct {unsigned long cmd,mode,r10,r40,r44; unsigned w,h; int exp; const char *what;} trun;
 static const trun tr[9]={
  {0xCC02014AUL,0x300UL,0x014401C4UL,0,0,32,8,0,"CONTROL op A unrotated, mode 300"},
  {0xCC02C14AUL,0x302UL,0x014B01E3UL,0,0,32,8,1,"180 opaque, X+Y dec, start bottom-right"},
  {0xCC02D54AUL,0x302UL,0x014B01E3UL,0x014401C4UL,0x014B01E3UL,32,8,1,"180 XP exact (clip + transparency)"},
  {0xCC02814AUL,0x301UL,0x014401CBUL,0,0,8,32,2,"90 opaque, X dec, engine size 8x32, start top-right"},
  {0xCC02414AUL,0x303UL,0x016301C4UL,0,0,8,32,3,"270 opaque, Y dec, engine size 8x32, start bottom-left"},
  {0xCC02954AUL,0x301UL,0x014401CBUL,0x014401C4UL,0x016301CBUL,8,32,2,"90 clip + transparency, size 8x32"},
  {0xCC02554AUL,0x303UL,0x016301C4UL,0x014401C4UL,0x016301CBUL,8,32,3,"270 clip + transparency, size 8x32"},
  {0xCC02814AUL,0x301UL,0x014401E3UL,0,0,32,8,-1,"90 opaque, X dec, engine size 32x8, start top-right of 32x8"},
  {0xCC02414AUL,0x303UL,0x014B01C4UL,0,0,32,8,-1,"270 opaque, Y dec, engine size 32x8, start bottom-left of 32x8"}};
 static const char *hn[6]={"r0","r180","r90","r270","T","AT"};
 unsigned long bm[8]; unsigned int k,x,y,i,n=0,ones=0,fgt,chg,h,pass=0; unsigned long fg=0xFFFF8000UL,bg=0xFF004080UL,got; char line[600];
 unsigned int hm[6],stray,bgstray;
 if(!can_run) return;
 for(i=0;i<512*512;i++) sheet[i]=0xFF000000UL;
 for(y=0;y<8;y++) {unsigned char b0=(unsigned char)(((0x80>>y)|0x01)&0xFF); bm[y]=(unsigned long)b0|(0xAAUL<<8)|(0x0FUL<<16)|((unsigned long)(y*0x11)<<24);}
 for(y=0;y<8;y++) for(x=0;x<32;x++) ones+=(unsigned)tr_bit(bm,y,x);
 logf2("\r\nTEXT ROTATE CONFIRM v10.02: host-fed 32x8 mono bitmap (%u one-bits), fg FFFF8000, bg FF004080, box at (4,4). Rungs 0-2 = unrotated control, opaque 180 (CC02C14A), XP exact 180 (CC02D54A); run these right after a clean restart. Rungs 3-8 = 90/270 exploration. For each rung: every destination pixel is checked against the expected orientation (fg-ness, 256 pixels), plus stray fg/bg pixels anywhere in the 64x64 tile outside the destination box. hm = fg-ness matches (of 256) for each hypothesis r0 r180 r90cw r270cw transpose anti-transpose.\r\n",ones);
 for(k=0;k<9 && can_run;k++) {
  memset(&request,0,sizeof(request));
  request.lab_mode=tr[k].mode; request.lab_cmd=tr[k].cmd; request.lab_w=tr[k].w; request.lab_h=tr[k].h; request.lab_dx=4; request.lab_dy=4;
  request.lab_sx=0; request.lab_sy=0; request.lab_fg=fg; request.lab_bg=bg; request.lab_flags=4|1;
  for(y=0;y<8;y++) request.lab_data[y]=bm[y];
  request.lab_ndata=8;
  n=0;
  request.lab_reg[n][0]=0x08; request.lab_reg[n][1]=320UL<<16; n++;
  request.lab_reg[n][0]=0x10; request.lab_reg[n][1]=tr[k].r10; n++;
  if(tr[k].r40||tr[k].r44) {request.lab_reg[n][0]=0x40; request.lab_reg[n][1]=tr[k].r40; n++; request.lab_reg[n][0]=0x44; request.lab_reg[n][1]=tr[k].r44; n++;}
  request.lab_nregs=n;
  sprintf(status_text,"Text rotate confirm: %u of 9 ...",k+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("TR %u call failed: %s\r\n",k,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("TR %u result %lu fault %lu\r\n",k,reply.result,reply.fault_latched); can_run=0; break;}
  collect();
  {int tx=(int)(k%8)*64,ty=(int)(k/8)*64,qx,qy; for(qy=0;qy<64;qy++) for(qx=0;qx<64;qx++) sheet[(ty+qy)*512+tx+qx]=reply.image[qy*64+qx];}
  fgt=0; for(h=0;h<6;h++) hm[h]=0;
  for(y=0;y<64;y++) for(x=0;x<64;x++) if(reply.image[y*64+x]==fg) fgt++;
  for(h=0;h<6;h++) {int bw,bh,xx,yy; for(yy=0;yy<32;yy++) for(xx=0;xx<32;xx++) {int ex; tr_hyp(bm,(int)h,0,0,&bw,&bh); if(xx>=bw||yy>=bh) continue; ex=tr_hyp(bm,(int)h,xx,yy,&bw,&bh); if(((reply.image[(4+yy)*64+4+xx]==fg)?1:0)==ex) hm[h]++;}}
  stray=bgstray=0;
  if(tr[k].exp>=0) {
   int bw,bh; tr_hyp(bm,tr[k].exp,0,0,&bw,&bh);
   for(y=0;y<64;y++) for(x=0;x<64;x++) if(!((int)x>=4&&(int)x<4+bw&&(int)y>=4&&(int)y<4+bh)) {got=reply.image[y*64+x]; if(got==fg) stray++; else if(got==bg) bgstray++;}
  }
  chg=(unsigned int)reply.sf_x_info[0][0];
  {int ok=0; if(tr[k].exp>=0) {int bw,bh; tr_hyp(bm,tr[k].exp,0,0,&bw,&bh); ok=(!reply.sf_x_info[0][1] && hm[tr[k].exp]==(unsigned)(bw*bh) && stray==0 && bgstray==0);} if(ok) pass++;
  sprintf(line,"TR %u: %08lX mode %03lX size %ux%u start %08lX  %s: %s%s; changed %u; fg %u of %u; hm %s %u %s %u %s %u %s %u %s %u %s %u; stray fg %u bg %u",k,tr[k].cmd,tr[k].mode,tr[k].w,tr[k].h,tr[k].r10,tr[k].what,reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?"HUNG+reset":"HUNG reset FAILED"):"finished",tr[k].exp>=0?(ok?" PASS":" FAIL"):"",chg,fgt,ones,hn[0],hm[0],hn[1],hm[1],hn[2],hm[2],hn[3],hm[3],hn[4],hm[4],hn[5],hm[5],stray,bgstray);}
  logtext(line);
  if(reply.sf_x_info[0][1]) {sprintf(line,"; st %08lX why %lu r0C %08lX r10 %08lX",(unsigned long)reply.sf_x_info[1][0],(unsigned long)reply.sf_x_info[1][1],(unsigned long)reply.sf_x_info[1][3],(unsigned long)reply.sf_x_info[1][4]); logtext(line);}
  logtext("\r\n");
 }
 save_sheet("TEXTROT.BMP");
 logtext("BMP TEXTROT.BMP written: 8 tiles per row, rungs 0-8 in order.\r\n");
 sprintf(status_text,"Text rotate confirm done: %u of 7 rungs with a defined expectation passed. Send me VX2DLAB.TXT and TEXTROT.BMP.",pass);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}
static int ts_bit(int y,int x,int W) {
 int v=(((x*3+y*5+(x>>2)*(y+1))%7)<3);
 if(y==0&&x<2) v=1;
 if(y==1&&x==0) v=1;
 if(y==0&&x==W-1) v=1;
 if(y==7&&x>=W-2) v=0;
 return v;
}
static void ts_dims(int ang,int W,int *bw,int *bh) {if(ang==0||ang==180) {*bw=W;*bh=8;} else {*bw=8;*bh=W;}}
static int ts_dest(int ang,int W,int x,int y) {
 if(ang==0) return ts_bit(y,x,W);
 if(ang==180) return ts_bit(7-y,W-1-x,W);
 if(ang==90) return ts_bit(7-x,y,W);
 return ts_bit(x,W-1-y,W);
}
static void do_textsweep(void) {
 static const int widths[5]={5,13,24,32,33};
 static const int angs[4]={0,180,90,270};
 static const unsigned long decb[4]={0UL,0xC000UL,0x8000UL,0x4000UL};
 static const char *vn[4]={"opaque","transp+clip=box","opaque+cut clip","transp+cut clip"};
 unsigned int ph,ai,wi,v,tot=0,pass=0,hung=0,nrun=0,pp[3][4],pt[3][4],wp[3][5],wt[3][5],i,y,x;
 char line[700];
 if(!can_run) return;
 memset(pp,0,sizeof(pp)); memset(pt,0,sizeof(pt)); memset(wp,0,sizeof(wp)); memset(wt,0,sizeof(wt));
 logf2("\r\nTEXT SWEEP v10.05: host-fed mono text, op A, glyph widths 5 13 24 32 33 x height 8, angles 0 90 180 270, four draws each (opaque; transparent + clip = box; opaque + clip cutting through the glyph; transparent + cut clip). Commands and corner rules are the proven ones (mode 300|angle/90, decrement bits 90=X 180=X+Y 270=Y, start corner as the table, engine size = destination size). Phase 0: 32bpp, rows padded to dwords (alignment bits 101). Phase 1: 16bpp scratch target, alignment 101, widths 13 and 33 only. Phase 2: 32bpp, byte alignment (001) with rows packed byte by byte. Every pixel of the tile is compared with the exact expected colour (fg FFFF8000 / bg FF004080 at 32bpp; fg F81F / bg 07E0 at 16bpp; untouched pixels must keep the prefill). bad-in = wrong pixels inside the glyph box, bad-out = wrong pixels outside it.\r\n");
 for(ph=0;ph<3 && can_run;ph++) for(ai=0;ai<4 && can_run;ai++) for(wi=0;wi<5 && can_run;wi++) {
  int ang=angs[ai],W=widths[wi],bw,bh,bpp16=(ph==1),a101=(ph!=2);
  if(ph==1 && !(W==13||W==33)) continue;
  ts_dims(ang,W,&bw,&bh);
  for(v=0;v<4 && can_run;v++) {
   int trans=(v==1||v==3),clip=(v>=1),cut=(v>=2),sx,sy,cx0,cy0,cx1,cy1,ox=bpp16?0:448,oy=bpp16?0:320,TW=bpp16?128:64,px,py,bin=0,bout=0,fx=-1,fy=-1,drawn;
   unsigned long cmd,fg=bpp16?0xFFFFF81FUL:0xFFFF8000UL,bg=bpp16?0xFF0007E0UL:0xFF004080UL,gotv,expv,fgx=bpp16?0xF81FUL:0xFFFF8000UL,bgx=bpp16?0x07E0UL:0xFF004080UL,fv=0,fe=0;
   unsigned int nB=(unsigned)((W+7)/8),ndw,n=0,chg=0,expchg=0;
   unsigned char bytes[200];
   memset(bytes,0,sizeof(bytes));
   if(a101) {unsigned int dpr=(nB+3)/4; ndw=8*dpr; for(y=0;y<8;y++) for(x=0;x<(unsigned)W;x++) if(ts_bit((int)y,(int)x,W)) bytes[y*dpr*4+x/8]|=(unsigned char)(0x80>>(x%8));}
   else {ndw=(8*nB+3)/4; for(y=0;y<8;y++) for(x=0;x<(unsigned)W;x++) if(ts_bit((int)y,(int)x,W)) bytes[y*nB+x/8]|=(unsigned char)(0x80>>(x%8));}
   if(ang==0) {sx=4;sy=4;} else if(ang==180) {sx=4+bw-1;sy=4+bh-1;} else if(ang==90) {sx=4+bw-1;sy=4;} else {sx=4;sy=4+bh-1;}
   if(cut) {cx0=4+1+bw/6; cx1=4+bw-2-bw/6; cy0=4+1+bh/6; cy1=4+bh-2-bh/6;} else {cx0=4;cx1=4+bw-1;cy0=4;cy1=4+bh-1;}
   cmd=0xCC000000UL|(a101?0xA0000UL:0x20000UL)|0x140UL|decb[ai]|(clip?0x1000UL:0UL)|(trans?0x400UL:0UL)|0xAUL;
   memset(&request,0,sizeof(request));
   request.lab_mode=(bpp16?0x100UL:0x300UL)|(unsigned long)(ang/90); request.lab_cmd=cmd; request.lab_w=(unsigned)bw; request.lab_h=(unsigned)bh;
   request.lab_dx=4; request.lab_dy=4; request.lab_sx=0; request.lab_sy=0; request.lab_fg=fg; request.lab_bg=bg;
   request.lab_flags=bpp16?(1|4|16):(4|1);
   for(i=0;i<ndw && i<VX3D_LAB_DATA;i++) request.lab_data[i]=(unsigned long)bytes[4*i]|((unsigned long)bytes[4*i+1]<<8)|((unsigned long)bytes[4*i+2]<<16)|((unsigned long)bytes[4*i+3]<<24);
   request.lab_ndata=ndw;
   request.lab_reg[n][0]=0x08; request.lab_reg[n][1]=bpp16?0x00200000UL:(320UL<<16); n++;
   if(bpp16) {request.lab_reg[n][0]=0x14; request.lab_reg[n][1]=0x00040200UL; n++;}
   request.lab_reg[n][0]=0x10; request.lab_reg[n][1]=((unsigned long)(oy+sy)<<16)|(unsigned long)(ox+sx); n++;
   if(clip) {request.lab_reg[n][0]=0x40; request.lab_reg[n][1]=((unsigned long)(oy+cy0)<<16)|(unsigned long)(ox+cx0); n++; request.lab_reg[n][0]=0x44; request.lab_reg[n][1]=((unsigned long)(oy+cy1)<<16)|(unsigned long)(ox+cx1); n++;}
   request.lab_nregs=n;
   sprintf(status_text,"Text sweep: run %u ...",nrun+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
   if(!call_lab()) {logf2("TS %u call failed: %s\r\n",nrun,status_text); can_run=0; break;}
   if(reply.result!=VX3D_PASS) {logf2("TS %u result %lu fault %lu\r\n",nrun,reply.result,reply.fault_latched); can_run=0; break;}
   collect();
   for(py=0;py<64;py++) for(px=0;px<TW;px++) {
    int inbox=(px>=4&&px<4+bw&&py>=4&&py<4+bh);
    if(bpp16) {gotv=(reply.image[py*64+(px>>1)]>>(16*(px&1)))&0xFFFFUL; expv=(px&1)?0xFF00UL:0UL;}
    else {gotv=reply.image[py*64+px]; expv=dk_pre(px,py);}
    if(inbox) {
     int bit=ts_dest(ang,W,px-4,py-4); drawn=(px>=cx0&&px<=cx1&&py>=cy0&&py<=cy1)||!clip;
     if(drawn) {if(bit) expv=fgx; else if(!trans) expv=bgx;}
    }
    if(gotv!=expv) {chg++; if(inbox) bin++; else bout++; if(fx<0) {fx=px;fy=py;fv=gotv;fe=expv;}}
    {unsigned long pre0=bpp16?((px&1)?0xFF00UL:0UL):dk_pre(px,py); if(expv!=pre0) expchg++;}
   }
   (void)chg; (void)bgx;
   {int ok=(!reply.sf_x_info[0][1] && bin==0 && bout==0); tot++; pt[ph][ai]++; wt[ph][wi]++; if(ok) {pass++; pp[ph][ai]++; wp[ph][wi]++;} if(reply.sf_x_info[0][1]) hung++;
   sprintf(line,"TS %3u: %s a%s w%-2d %-3d %-16s cmd %08lX size %dx%d start %d,%d: %s%s; bad-in %d bad-out %d; expected-changed %u",nrun,bpp16?"16bpp":"32bpp",a101?"101":"001",W,ang,vn[v],cmd,bw,bh,sx,sy,ok?"PASS":"FAIL",reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"",bin,bout,expchg);
   logtext(line);
   if(!ok && fx>=0) {sprintf(line,"; first bad (%d,%d) got %08lX want %08lX",fx,fy,fv,fe); logtext(line);}
   logtext("\r\n");}
   nrun++;
  }
 }
 logf2("\r\nTEXT SWEEP SUMMARY v10.05: %u of %u runs passed, %u hung.\r\n",pass,tot,hung);
 for(ph=0;ph<3;ph++) {
  logf2("phase %u (%s, alignment %s): by angle 0/180/90/270 = %u/%u, %u/%u, %u/%u, %u/%u; by width 5/13/24/32/33 = %u/%u, %u/%u, %u/%u, %u/%u, %u/%u\r\n",ph,ph==1?"16bpp":"32bpp",ph==2?"001":"101",pp[ph][0],pt[ph][0],pp[ph][1],pt[ph][1],pp[ph][2],pt[ph][2],pp[ph][3],pt[ph][3],wp[ph][0],wt[ph][0],wp[ph][1],wt[ph][1],wp[ph][2],wt[ph][2],wp[ph][3],wt[ph][3],wp[ph][4],wt[ph][4]);
 }
 sprintf(status_text,"Text sweep done: %u of %u passed. Send me VX2DLAB.TXT.",pass,tot);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}
static void do_monosrc3(void) {
 static const unsigned long cm[6]={0xCC0A0141UL,0xCC0A0101UL,0xCC020101UL,0xCC000101UL,0xCC020102UL,0xCC0A0102UL};
 static const unsigned long strd[2]={256,256};
 static const unsigned long pit[2]={32,0};
 unsigned int ci,si,k=0,good=0,i; int x,y,r; unsigned long fg=0xFFFF8000UL,bg=0xFF004080UL,got,orig;
 unsigned int msb,lsb,chg; unsigned char b4[4]; char line[600];
 if(!can_run) return;
 for(i=0;i<512*512;i++) sheet[i]=0xFF000000UL;
 logf2("\r\nMONO SOURCE SWEEP 5 v10.01: HOST DATA FED TO SCREEN-SOURCE MONO COMMANDS. Hypothesis: the engine always takes mono source bits from the host window, so every earlier screen-source command (host bit 6 clear) sat waiting for data (the hang state: size untouched, no pixel written, wait reason 2). Each run issues the command, then immediately writes the 8 row dwords of the bitmap to the host window (0x200000). Run 0 / 6 = CC0A0141 (proven host-fed mono) as control. Then the same command with bit 6 CLEAR: CC0A0101, CC020101, CC000101, CC020102, CC0A0102. Source pitch 32 for runs 0-5, source pitch 0 for runs 6-11.\r\n");
 for(si=0;si<2 && can_run;si++) for(ci=0;ci<6 && can_run;ci++) {
  memset(&request,0,sizeof(request));
  request.lab_mode=0x300UL; request.lab_cmd=cm[ci]; request.lab_w=32; request.lab_h=8; request.lab_dx=4; request.lab_dy=4;
  request.lab_sx=(unsigned)strd[si]; request.lab_sy=0; request.lab_fg=fg; request.lab_bg=bg; request.lab_flags=4|2048|1|8192;
  for(r=0;r<8;r++) {
   b4[0]=(unsigned char)(((0x80>>r)|0x01)&0xFF); b4[1]=0xAA; b4[2]=0x0F; b4[3]=(unsigned char)(r*0x11);
   request.lab_data[2*r]=(unsigned long)b4[0]|((unsigned long)b4[1]<<8)|((unsigned long)b4[2]<<16)|((unsigned long)b4[3]<<24);
   request.lab_data[2*r+1]=0;
  }
  request.lab_ndata=16;
  request.lab_reg[0][0]=0x08; request.lab_reg[0][1]=(320UL<<16)|pit[si];
  request.lab_reg[1][0]=0x1C; request.lab_reg[1][1]=0x00040200UL;
  request.lab_reg[2][0]=0x18; request.lab_reg[2][1]=0UL;
  request.lab_reg[3][0]=0x40; request.lab_reg[3][1]=0UL;
  request.lab_reg[4][0]=0x44; request.lab_reg[4][1]=0x01DF027FUL;
  request.lab_nregs=5;
  sprintf(status_text,"Mono source 3: %u ...",k+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("MS3 %u call failed: %s\r\n",k,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("MS3 %u result %lu fault %lu\r\n",k,reply.result,reply.fault_latched); can_run=0; break;}
  collect();
  {int tx=(int)(k%8)*64,ty=(int)(k/8)*64,px,py; for(py=0;py<64;py++) for(px=0;px<64;px++) sheet[(ty+py)*512+tx+px]=reply.image[py*64+px];}
  msb=lsb=chg=0;
  for(y=0;y<8;y++) for(x=0;x<32;x++) {
   int bm,bl; unsigned char byv;
   byv=(x/8==0)?(unsigned char)(((0x80>>y)|0x01)&0xFF):(x/8==1)?0xAA:(x/8==2)?0x0F:(unsigned char)(y*0x11);
   bm=(byv>>(7-(x%8)))&1; bl=(byv>>(x%8))&1;
   got=reply.image[(4+y)*64+4+x]; orig=dk_pre(4+x,4+y);
   if(got!=orig) chg++;
   if(got==(bm?fg:bg)) msb++;
   if(got==(bl?fg:bg)) lsb++;
  }
  { int ok=(!reply.sf_x_info[0][1] && (msb==256 || lsb==256)); if(ok) good++;
  sprintf(line,"MS3 %2u: stride %lu pitch %lu %s: %s; changed %u of 256; match msb %u lsb %u of 256%s",k,strd[si],pit[si],ci==0?"CC0A0141":ci==1?"CC0A0101":ci==2?"CC020101":ci==3?"CC000101":ci==4?"CC020102":"CC0A0102",ok?"PASS":"FAIL",chg,msb,lsb,reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"");
  logtext(line);
  if(reply.sf_x_info[0][1]) {sprintf(line,"; st %08lX why %lu r0C %08lX r10 %08lX",(unsigned long)reply.sf_x_info[1][0],(unsigned long)reply.sf_x_info[1][1],(unsigned long)reply.sf_x_info[1][3],(unsigned long)reply.sf_x_info[1][4]); logtext(line);}
  logtext("\r\n"); }
  k++;
 }
 save_sheet("MONOSRC3.BMP");
 logtext("BMP MONOSRC3.BMP written: 8 tiles per row at 1x, runs 0-11 in order (2 pitches x 6 forms, form varies fastest).\r\n");
 sprintf(status_text,"Mono source sweep 3 done: %u of %u passed. Send me VX2DLAB.TXT and MONOSRC3.BMP.",good,k);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}
static void do_bench(void) {
 static const int sz[2]={16,32};
 static const char *nm[5]={"host blit, no rotate (op 1)","rotate 90","rotate 180","rotate 270","CPU rotate 90 (FB writes)"};
 unsigned int si,k,i,idx=0,reps=500; int w,h,dw,dh,dx,dy; unsigned long ms,bytes; char line[300];
 if(!can_run) return;
 logf2("\r\nTHROUGHPUT v10.01: 32bpp, %u repeats per case, square source w x w fed through the host window (w*w dwords per repeat), desktop box at (4,4). 'CPU' = rotate copy by CPU framebuffer writes through the same VxD accessor (hardware path writes the host window with no per-dword drain read, like the CPU path).\r\n",reps);
 for(si=0;si<2 && can_run;si++) for(k=0;k<5 && can_run;k++) {
  w=h=sz[si]; dw=w; dh=h; dx=4; dy=4;
  memset(&request,0,sizeof(request));
  request.lab_mode=0x300UL; request.lab_cmd=0xCC002041UL;
  if(k==1) {request.lab_cmd=0xCC00A049UL; request.lab_mode=0x301UL; dx=4+w-1;}
  if(k==2) {request.lab_cmd=0xCC00E049UL; request.lab_mode=0x302UL; dx=4+w-1; dy=4+h-1;}
  if(k==3) {request.lab_cmd=0xCC006049UL; request.lab_mode=0x303UL; dy=4+h-1;}
  if(k==4) {request.lab_mode=0x300UL;}
  request.lab_w=(unsigned)dw; request.lab_h=(unsigned)dh;
  request.lab_sx=60; request.lab_sy=60; request.lab_dx=(unsigned)dx; request.lab_dy=(unsigned)dy;
  request.lab_fg=0xFFFF8000UL; request.lab_bg=reps; request.lab_flags=256|(k==4?512:0);
  for(i=0;i<(unsigned)(w*h);i++) request.lab_data[i]=0xFF000000UL|(unsigned long)(i+1);
  request.lab_ndata=(unsigned)(w*h);
  sprintf(status_text,"Throughput %u ...",idx+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("BENCH %u call failed: %s\r\n",idx,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("BENCH %u result %lu fault %lu\r\n",idx,reply.result,reply.fault_latched); can_run=0; break;}
  collect(); ms=reply.sf_x_info[0][3]; bytes=(unsigned long)w*h*4UL*reps;
  sprintf(line,"BENCH %u: %2dx%-2d %-28s %5lu ms for %u reps = %lu us each, %lu KB/s, pixels drawn %lu%s\r\n",idx,w,h,nm[k],ms,reps,ms*1000UL/reps,ms?bytes/ms:0UL,(unsigned long)reply.sf_x_info[0][0],reply.sf_x_info[0][1]?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"");
  logtext(line);
  idx++;
 }
 sprintf(status_text,"Throughput done (%u runs). Send me VX2DLAB.TXT (grep BENCH).",idx);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}
static void do_verify16(void) {
 static const int cs[6][2]={{10,6},{6,10},{30,20},{20,30},{32,32},{31,17}};
 unsigned int ci,mode3,idx=0,i,pass=0,sx0,sy0; int sw,sh,want,w2,h2,x,y,t,px,py,found; char line[900],name[16],hits[400],tmp[100]; unsigned long ib,cmd;
 if(!can_run) return;
 nuniq=1;
 logf2("\r\n16 BPP ROTATION VERIFY v10.01: same as ROT VERIFY but 16 bits per pixel (mode bits 9:8 = 01), source rows padded to an even pixel count, packed two pixels per dword, source X = 0, destination = the 64x64 scratch target seen as 128 pixels x 64 rows of 16 bit. Box placed at (4,4)\r\n");
 for(ci=0;ci<6 && can_run;ci++) for(mode3=0;mode3<3 && can_run;mode3++) {
  sw=cs[ci][0]; sh=cs[ci][1];
  make_hostn(sw,sh);
  memset(&request,0,sizeof(request));
  if(mode3==0) {want=1; strcpy(name,"90 cw"); w2=sh; h2=sw; cmd=0xCC002049UL|0x8000UL; request.lab_mode=0x101UL; sx0=4+(unsigned)w2-1; sy0=4;}
  else if(mode3==1) {want=3; strcpy(name,"270 cw"); w2=sh; h2=sw; cmd=0xCC002049UL|0x4000UL; request.lab_mode=0x103UL; sx0=4; sy0=4+(unsigned)h2-1;}
  else {want=2; strcpy(name,"180"); w2=sw; h2=sh; cmd=0xCC002049UL|0xC000UL; request.lab_mode=0x102UL; sx0=4+(unsigned)w2-1; sy0=4+(unsigned)h2-1;}
  request.lab_cmd=cmd; request.lab_w=(unsigned)w2; request.lab_h=(unsigned)h2;
  request.lab_sx=60; request.lab_sy=60; request.lab_dx=4; request.lab_dy=4;
  request.lab_fg=0xFFFF8000UL; request.lab_bg=0xFF004080UL; request.lab_flags=1|4|16;
  request.lab_reg[0][0]=0x08; request.lab_reg[0][1]=0x00200000UL;
  request.lab_reg[1][0]=0x14; request.lab_reg[1][1]=0x00040200UL;
  request.lab_reg[2][0]=0x10; request.lab_reg[2][1]=((unsigned long)sy0<<16)|sx0;
  request.lab_reg[3][0]=0x18; request.lab_reg[3][1]=((unsigned long)380<<16);
  request.lab_nregs=4;
  { unsigned int spw=(unsigned)((sw+1)&~1),yy,xx,k=0,tot=spw*(unsigned)sh; static unsigned short pk[2200]; memset(pk,0,sizeof(pk)); for(yy=0;yy<(unsigned)sh;yy++) for(xx=0;xx<(unsigned)sw;xx++) pk[yy*spw+xx]=(unsigned short)(nimg[yy*(unsigned)sw+xx]&0xFFFFUL); for(k=0;k<tot/2+16 && k<VX3D_LAB_DATA;k++) request.lab_data[k]=(unsigned long)pk[2*k]|((unsigned long)pk[2*k+1]<<16); request.lab_ndata=k; }
  sprintf(status_text,"16 bpp verify %u ...",idx+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("V16 %u call failed: %s\r\n",idx,status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("V16 %u result %lu fault %lu\r\n",idx,reply.result,reply.fault_latched); can_run=0; break;}
  collect();
  for(y=0;y<64;y++) for(x=0;x<64;x++) {DWORD d=reply.image[y*64+x]; p16[y*128+2*x]=(unsigned short)(d&0xFFFFUL); p16[y*128+2*x+1]=(unsigned short)(d>>16);}
  hits[0]=0; found=0;
  for(t=0;t<8;t++) for(py=0;py+nth[t]<=64;py++) for(px=0;px+ntw[t]<=128;px++) {
   int ok=1;
   for(y=0;y<nth[t]&&ok;y++) for(x=0;x<ntw[t];x++) if(p16[(py+y)*128+px+x]!=(unsigned short)(nxf[t][y*ntw[t]+x]&0xFFFFUL)) {ok=0;break;}
   if(ok) {sprintf(tmp,"%s@(%d,%d) ",tname[t],px,py); if(strlen(hits)+strlen(tmp)<sizeof(hits)) strcat(hits,tmp); found++; if(t==want && px==4 && py==4) found+=1000;}
  }
  { int ok=(found>=1000); if(ok) pass++;
  ib=reply.sf_x_info[0][1];
  sprintf(line,"V16 %2u: %2dx%-2d %-6s engine size %dx%d start(%u,%u): %s%s; found: %s\r\n",idx,sw,sh,name,w2,h2,sx0,sy0,ok?"PASS":"FAIL",ib?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"",found?hits:"nothing matching");
  logtext(line); }
  idx++;
 }
 nuniq=0;
 sprintf(status_text,"16 bpp verify done: %u of %u passed. Send me VX2DLAB.TXT (grep V16).",pass,idx);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}

static void do_map16_unused(void) {
 static const unsigned long cm[8]={0x100,0x101,0x101,0x101,0x101,0x102,0x100,0x103};
 static const unsigned long cd[8]={0x0000,0x0000,0x8000,0x4000,0xC000,0xC000,0xC000,0x4000};
 static const unsigned int sx[8]={4,4,11,4,11,11,11,4},sy[8]={4,4,4,11,11,11,11,11};
 unsigned int ci,k,idx=0; int x,y; char row[160]; unsigned long ib;
 if(!can_run) return;
 logf2("\r\n16 BPP MAP v10.01: 8x8 source of 16 bit pixels numbered 01..40 (hex), packed two per dword in row order, cmd CC002049|dec, engine size 8x8. Grid = 16 bit pixel columns 0..19 of the scratch target, rows 0..19; '..' untouched, '?\?' other value\r\n");
 for(ci=0;ci<8 && can_run;ci++) {
  memset(&request,0,sizeof(request));
  request.lab_cmd=0xCC002049UL|cd[ci]; request.lab_mode=cm[ci]; request.lab_w=8; request.lab_h=8;
  request.lab_sx=60; request.lab_sy=60; request.lab_dx=4; request.lab_dy=4;
  request.lab_fg=0xFFFF8000UL; request.lab_bg=0xFF004080UL; request.lab_flags=1|4|16;
  request.lab_reg[0][0]=0x08; request.lab_reg[0][1]=0x00200000UL;
  request.lab_reg[1][0]=0x14; request.lab_reg[1][1]=0x00040200UL;
  request.lab_reg[2][0]=0x10; request.lab_reg[2][1]=((unsigned long)sy[ci]<<16)|sx[ci];
  request.lab_nregs=3;
  for(k=0;k<32;k++) request.lab_data[k]=(unsigned long)(2*k+1)|((unsigned long)(2*k+2)<<16);
  request.lab_ndata=32;
  sprintf(status_text,"16 bpp map %u ...",ci+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("M16 call failed: %s\r\n",status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("M16 result %lu fault %lu\r\n",reply.result,reply.fault_latched); can_run=0; break;}
  collect(); ib=reply.sf_x_info[0][1];
  logf2("M16 %u: mode %03lX cmd %08lX start (%u,%u)%s\r\n",ci,cm[ci],0xCC002049UL|cd[ci],sx[ci],sy[ci],ib?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):"");
  for(y=0;y<20;y++) {
   row[0]=0;
   for(x=0;x<20;x++) {
    DWORD d=reply.image[y*64+x/2]; unsigned int v=(x&1)?(unsigned)(d>>16):(unsigned)(d&0xFFFFUL); char cell[8];
    if(v==0 || v==0xFF00) strcpy(cell,".. "); else if(v<=0x40) sprintf(cell,"%02X ",v); else strcpy(cell,"?? ");
    strcat(row,cell);
   }
   logf2("  y%02d %s\r\n",y,row);
  }
  idx++;
 }
 sprintf(status_text,"16 bpp map done (%u runs). Send me VX2DLAB.TXT (grep M16).",idx);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}

static void do_align16_unused(void) {
 static const int ex[16][4]={{4,8,2,0},{0,8,2,0},{4,4,2,0},{4,12,2,0},{12,8,2,0},{1,5,2,0},{3,7,2,0},{4,8,2,1},{4,8,2,2},{4,8,2,3},{4,8,2,4},{4,8,2,5},{4,8,2,6},{4,8,2,7},{4,8,2,8},{4,8,2,12}};
 unsigned int ci,k,idx=0,nd; int x,y,x0,w,h,lines,sxl; char row[200]; unsigned long ib;
 if(!can_run) return;
 logf2("\r\n16 BPP ALIGN v10.01: plain 16bpp host blit (mode 100, cmd CC002049, no direction bits). Source pixels numbered 01.. in stream order (hex). Each run: dest start x0, width w, height h. Source X (reg 18 low bits) is set explicitly per run; data sent = generous. Grid: 16 bit pixel columns 0..31, rows 0..5\r\n");
 for(ci=0;ci<16 && can_run;ci++) {
  x0=ex[ci][0]; w=ex[ci][1]; h=ex[ci][2]; sxl=ex[ci][3];
  lines=((x0+w-1)/16)-(x0/16)+1; nd=(unsigned)(h*(lines+1)*8);
  memset(&request,0,sizeof(request));
  request.lab_cmd=0xCC002049UL; request.lab_mode=0x100UL; request.lab_w=(unsigned)w; request.lab_h=(unsigned)h;
  request.lab_sx=60; request.lab_sy=60; request.lab_dx=4; request.lab_dy=4;
  request.lab_fg=0xFFFF8000UL; request.lab_bg=0xFF004080UL; request.lab_flags=1|4|16;
  request.lab_reg[0][0]=0x08; request.lab_reg[0][1]=0x00200000UL;
  request.lab_reg[1][0]=0x14; request.lab_reg[1][1]=0x00040200UL;
  request.lab_reg[2][0]=0x10; request.lab_reg[2][1]=((unsigned long)1<<16)|(unsigned)x0;
  request.lab_reg[3][0]=0x18; request.lab_reg[3][1]=((unsigned long)380<<16)|(unsigned)sxl;
  request.lab_nregs=4;
  for(k=0;k<nd && k<VX3D_LAB_DATA;k++) request.lab_data[k]=(unsigned long)(2*k+1)|((unsigned long)(2*k+2)<<16);
  request.lab_ndata=nd;
  sprintf(status_text,"16 bpp align %u ...",ci+1); InvalidateRect(hmain,NULL,TRUE); UpdateWindow(hmain);
  if(!call_lab()) {logf2("A16 call failed: %s\r\n",status_text); can_run=0; break;}
  if(reply.result!=VX3D_PASS) {logf2("A16 result %lu fault %lu\r\n",reply.result,reply.fault_latched); can_run=0; break;}
  collect(); ib=reply.sf_x_info[0][1];
  logf2("A16 %u: x0=%d w=%d h=%d srcX=%d sent %u dwords%s\r\n",ci,x0,w,h,sxl,nd,ib?(reply.sf_x_info[0][2]?" HUNG+reset":" HUNG,NO RESET"):" finished");
  for(y=0;y<5;y++) {
   row[0]=0;
   for(x=0;x<32;x++) {
    DWORD d=reply.image[y*64+x/2]; unsigned int v=(x&1)?(unsigned)(d>>16):(unsigned)(d&0xFFFFUL); char cell[8];
    if(v==0 || v==0xFF00) strcpy(cell,".. "); else if(v<=0xFF) sprintf(cell,"%02X ",v); else strcpy(cell,"?? ");
    strcat(row,cell);
   }
   logf2("  y%d %s\r\n",y,row);
  }
  idx++;
 }
 sprintf(status_text,"16 bpp align done (%u runs). Send me VX2DLAB.TXT (grep A16).",idx);
 logtext(status_text); logtext("\r\n");
 InvalidateRect(hmain,NULL,TRUE);
}

static LRESULT CALLBACK proc(HWND w,UINT msg,WPARAM wp,LPARAM lp) {
 PAINTSTRUCT ps; HDC dc; RECT rc; int i;
 switch(msg) {
 case WM_CREATE:
  hmain=w;
  font=(HFONT)GetStockObject(DEFAULT_GUI_FONT);
  hcmd=field(w,"CMD","CC000001",8,8,76,300);   hmode=field(w,"MODE","300",132,8,40,301);
  hw=field(w,"W","16",216,8,36,302);           hh=field(w,"H","16",290,8,36,303);
  hsx=field(w,"SX","4",8,34,36,304);           hsy=field(w,"SY","4",92,34,36,305);
  hdx=field(w,"DX","36",176,34,36,306);        hdy=field(w,"DY","36",260,34,36,307);
  hfg=field(w,"FG","FFFF8000",8,60,76,308);    hbg=field(w,"BG","FF004080",132,60,76,309);
  hhost=mk(w,"BUTTON","host data",BS_AUTOCHECKBOX,400,8,90,18,ID_HOST);
  hincr=mk(w,"BUTTON","incr addr",BS_AUTOCHECKBOX,400,34,90,18,ID_INCR);
  hcpu=mk(w,"BUTTON","CPU window test",BS_AUTOCHECKBOX,500,8,130,18,232);
  hroff=mk(w,"BUTTON","rot off before read",BS_AUTOCHECKBOX,500,34,130,18,233);
  hrt=mk(w,"BUTTON","read scratch target",BS_AUTOCHECKBOX,500,60,130,18,234);
  hxf=field(w,"XF","0",270,60,50,235);
  mk(w,"STATIC","extra regs (OFF=VALUE hex per line, e.g. 08=01900000):",0,8,86,300,14,0);
  hregs=mk(w,"EDIT","",WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,8,100,300,44,310);
  mk(w,"STATIC","host data dwords (hex):",0,316,86,200,14,0);
  hdata=mk(w,"EDIT","",WS_BORDER|ES_MULTILINE|ES_AUTOVSCROLL|ES_WANTRETURN,316,100,300,44,311);
  hpreset=mk(w,"COMBOBOX","",WS_VSCROLL|CBS_DROPDOWNLIST,8,150,260,200,ID_PRESET);
  for(i=0;i<NPRESET;i++) SendMessage(hpreset,CB_ADDSTRING,0,(LPARAM)presets[i].name);
  SendMessage(hpreset,CB_SETCURSEL,0,0);
  mk(w,"BUTTON","Apply",BS_PUSHBUTTON,276,149,60,24,ID_APPLY);
  mk(w,"BUTTON","RUN",BS_PUSHBUTTON,344,149,70,24,ID_RUN);
  mk(w,"BUTTON","TEXT SWEEP",BS_PUSHBUTTON,424,149,100,24,ID_SWEEP);
  mk(w,"BUTTON","ROT VERIFY",BS_PUSHBUTTON,528,149,100,24,ID_SWEEP2);
  mk(w,"BUTTON","VRAM",BS_PUSHBUTTON,326,56,56,24,ID_REACH);
  mk(w,"BUTTON","ROUTING",BS_PUSHBUTTON,386,56,58,24,ID_ROUTE);
  mk(w,"BUTTON","DVI TEST",BS_PUSHBUTTON,448,56,50,24,ID_DVI);
  mk(w,"BUTTON","DVI TUNE",BS_PUSHBUTTON,520,81,100,18,ID_TUNE);
  mk(w,"STATIC","What do you see?",0,8,178,100,14,0);
  for(i=0;i<NVERD;i++) mk(w,"BUTTON",verd_names[i],BS_PUSHBUTTON,110+i*72,174,70,22,ID_VERD0+i);
  open_device();
  return 0;
 case WM_COMMAND:
  if(LOWORD(wp)==ID_PRESET && HIWORD(wp)==CBN_SELCHANGE) apply_preset();
  if(HIWORD(wp)==BN_CLICKED) {
   if(LOWORD(wp)==ID_RUN) do_run();
   else if(LOWORD(wp)==ID_SWEEP) do_textsweep();
   else if(LOWORD(wp)==ID_SWEEP2) do_verify();
   else if(LOWORD(wp)==ID_REACH) do_vramreach();
   else if(LOWORD(wp)==ID_ROUTE) do_routing();
   else if(LOWORD(wp)==ID_DVI) do_dvitest();
   else if(LOWORD(wp)==ID_TUNE) do_tune();
   else if(LOWORD(wp)==ID_APPLY) apply_preset();
   else if(LOWORD(wp)>=ID_VERD0 && LOWORD(wp)<ID_VERD0+NVERD) do_verdict(LOWORD(wp)-ID_VERD0);
  }
  return 0;
 case WM_PAINT:
  dc=BeginPaint(w,&ps);
  SelectObject(dc,font); SetBkMode(dc,TRANSPARENT);
  TextOut(dc,IMG_X,IMG_Y-2,"before",6); TextOut(dc,IMG_X+64*SCALE+10,IMG_Y-2,"after",5);
  if(have_image) {draw_img(dc,before_img,IMG_X,IMG_Y+12); draw_img(dc,after_img,IMG_X+64*SCALE+10,IMG_Y+12);}
  rc.left=IMG_X+2*64*SCALE+22; rc.top=IMG_Y+12; rc.right=632; rc.bottom=IMG_Y+12+64*SCALE+4;
  DrawText(dc,status_text,-1,&rc,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
  EndPaint(w,&ps); return 0;
 case WM_KEYDOWN: tune_key(wp); return 0;
 case WM_DESTROY: tune_stop(); PostQuitMessage(0); return 0;
 }
 return DefWindowProc(w,msg,wp,lp);
}
int WINAPI WinMain(HINSTANCE inst,HINSTANCE prev,LPSTR cmd,int show) {
 WNDCLASS wc; MSG m; HWND w; char name[MAX_PATH],*p;
 (void)prev;(void)cmd;
 GetModuleFileName(NULL,name,sizeof(name)); strcpy(dirpath,name); p=strrchr(name,'\\'); if(p) strcpy(p+1,"VX2DLAB.TXT"); else strcpy(name,"VX2DLAB.TXT");
 logfile=CreateFile(name,GENERIC_WRITE,FILE_SHARE_READ,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(logfile!=INVALID_HANDLE_VALUE) {SetFilePointer(logfile,0,NULL,FILE_END); logtext("\r\n==== VX2DLAB session (v10.05) ====\r\n");}
 memset(&wc,0,sizeof(wc));
 wc.lpfnWndProc=proc; wc.hInstance=inst; wc.hCursor=LoadCursor(NULL,IDC_ARROW);
 wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1); wc.lpszClassName="VX2DLab";
 RegisterClass(&wc);
 w=CreateWindow("VX2DLab","VX855 2D lab v10.17",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_VISIBLE,4,4,640,440,NULL,NULL,inst,NULL);
 if(!w) return 1;
 ShowWindow(w,show); UpdateWindow(w);
 while(GetMessage(&m,NULL,0,0)>0) {TranslateMessage(&m); DispatchMessage(&m);}
 return 0;
}
