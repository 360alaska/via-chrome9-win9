/* VXAB v1.0: the same frames timed on any build of the driver, for an A/B between two builds on one machine (.551 against the current
 * one). It asks the driver nothing but its build number, so it runs on every build. Five scenes, each 8 frames to settle and 120
 * timed: the median and the 95th percentile of a frame, in ms.
 *  1. lists: a 40 x 40 grid as ten indexed lists, sixteen times a frame (51,200 triangles)
 *  2. strips: the grid as four indexed strips with stitches, sixteen times (53,504 triangles with the stitches)
 *  3. fans: 240 triangles across the near plane, clipped by Direct3D, sixteen times
 *  4. states: 384 quads, a state change before each (four textures, blend, alpha test and filter by turns)
 *  5. lists whose spans hold far-outside vertices no triangle uses, sixteen times
 * Run it on build A, copy build B's files and restart, run it again: VXAB.LOG is written beside the program with the build in its
 * name (VXAB551.LOG, VXAB566.LOG), so the two logs stand side by side. Direct3D 8, a 256x256 window, the desktop at 16 or 32 bpp,
 * nothing else running, VXD3DMON not running. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

typedef unsigned long U32;
#define VT(o,i) (((void**)(*(void***)(o)))[i])
typedef HRESULT (__stdcall *F0)(void*);
typedef HRESULT (__stdcall *F1)(void*,U32);
typedef HRESULT (__stdcall *F2)(void*,U32,U32);
typedef HRESULT (__stdcall *F2P)(void*,U32,void*);
typedef HRESULT (__stdcall *F3P)(void*,U32,U32,void*);
typedef HRESULT (__stdcall *FDPUP)(void*,U32,U32,const void*,U32);
typedef HRESULT (__stdcall *FDIPUP)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32);
enum {D3D_REL=2,D3D_MODE=8,D3D_CREATEDEV=15};
enum {DEV_PRESENT=15,DEV_BACKBUF=16,DEV_CREATETEX=20,DEV_BEGIN=34,DEV_END=35,DEV_CLEAR=36,DEV_SETRS=50,DEV_SETTEX=61,DEV_SETTSS=63,DEV_DPUP=72,DEV_DIPUP=73,DEV_SETVS=76};
enum {SURF_LOCK=9,SURF_UNLOCK=10,TEX_LOCK=16,TEX_UNLOCK=17};
typedef struct {U32 w,h,refresh,format;} MODE;
typedef struct {U32 bw,bh,bfmt,bcount,ms,swap;HWND wnd;U32 windowed,autoz,zfmt,flags,rate,interval;} PP;
typedef struct {long pitch;void *bits;} LR;
typedef struct {float x,y,z,w;U32 c;float u,v;} TL; /* FVF 0x144: XYZRHW, DIFFUSE, TEX1 */
typedef struct {float x,y,z;U32 c;float u,v;} UV;   /* FVF 0x142: XYZ, DIFFUSE, TEX1 (the runtime transforms and clips it) */
#define WIN 256
#define GN 40
#define NV ((GN+1)*(GN+1))
#define ROWS 10 /* rows a strip */
#define SIX (ROWS*(2*(GN+1)+2)-2) /* indices a strip: each row 2 * 41, two stitch indices between rows */
#define NF 240 /* clipped triangles */

static FILE *logf;static int fails;
static void say(const char *f,...) {static char b[2048];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);fflush(stdout);if(logf){fputs(b,logf);fflush(logf);}}
static void *dev,*tex;static U32 bbfmt;static HWND wnd;
static HRESULT clear(U32 color) {return ((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,1,color,1.0f,0);}
static void rs(U32 s,U32 v) {((F2)VT(dev,DEV_SETRS))(dev,s,v);}
static void tss(U32 s,U32 v) {((F3P)VT(dev,DEV_SETTSS))(dev,0,s,(void*)v);}
static void present(void) {((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);}
static TL gv[NV];static unsigned short si[GN/ROWS][SIX];static UV fv[NF*3];
#define LROWS 4 /* rows a list */
#define BADV(i) (((i)%(GN+1))%7==3&&(((i)/(GN+1))&1)) /* LROWS is even: a block's rows have the grid's parity */
static unsigned short li[LROWS*GN*6];
static TL bv[NV];static unsigned short bi[LROWS*GN*6];static U32 bn; /* scene 5: the grid with every seventh vertex far outside, the triangles that use none of them */
static void make_scenes(void) {
 U32 x,y,s,k;
 for(y=0;y<=GN;y++)for(x=0;x<=GN;x++){TL *v=&gv[y*(GN+1)+x];
  v->x=(float)x*(float)WIN/GN;v->y=(float)y*(float)WIN/GN;v->z=0.5f;v->w=1.0f;
  v->c=0xFF000000UL|((64+(x*191)/GN)<<16)|((64+(y*191)/GN)<<8)|(255-((x+y)*95)/GN);v->u=(float)x/4.0f;v->v=(float)y/4.0f;}
 for(s=0;s<GN/ROWS;s++){unsigned short *p=si[s];
  for(y=s*ROWS;y<(s+1)*ROWS;y++){
   if(y>s*ROWS){*p++=(unsigned short)(y*(GN+1)+GN);*p++=(unsigned short)(y*(GN+1));} /* the stitch: the row before's last index again and this row's first twice */
   for(x=0;x<=GN;x++){*p++=(unsigned short)(y*(GN+1)+x);*p++=(unsigned short)((y+1)*(GN+1)+x);}}}
 {unsigned short *p=li;for(y=0;y<LROWS;y++)for(x=0;x<GN;x++){unsigned short a=(unsigned short)(y*(GN+1)+x),b=(unsigned short)(a+1),c=(unsigned short)(a+GN+1),d=(unsigned short)(c+1);
   *p++=a;*p++=b;*p++=c;*p++=b;*p++=d;*p++=c;}}
 memcpy(bv,gv,sizeof(bv));for(k=0;k<NV;k++)if(BADV(k))bv[k].x=5000.0f; /* v1.3: by column and row parity, so a vertex is moved or not the same way in every block (v1.2 moved every seventh of the whole grid, each block met every residue and no triangle was left) */
 {unsigned short *p=bi;for(k=0;k<LROWS*GN*2;k++){const unsigned short *t=li+k*3;U32 a=t[0],b=t[1],c=t[2],r; /* the same index list for every block: a triangle stays when it uses no moved vertex in any block */
   (void)r;if(!BADV(a)&&!BADV(b)&&!BADV(c)){*p++=t[0];*p++=t[1];*p++=t[2];}}
  bn=(U32)(p-bi)/3;}
 /* the clipped triangles: a 20 x 12 field in clip space (x, y in -0.95 .. 0.95); one or two corners of each behind the near plane */
 for(k=0;k<NF;k++){UV *v=&fv[k*3];float cx=-0.9f+(float)(k%20)*0.095f,cy=-0.85f+(float)(k/20)*0.15f;U32 c=0xFF000000UL|((80+(k*7)%170)<<16)|((60+(k*13)%190)<<8)|(90+(k*29)%160);
  v[0].x=cx;v[0].y=cy;v[1].x=cx+0.085f;v[1].y=cy+0.02f;v[2].x=cx+0.03f;v[2].y=cy+0.13f;
  v[0].z=(k&1)?-0.3f:0.5f;v[1].z=(k%3)?0.5f:-0.2f;v[2].z=((k&1)||!(k%3))?0.6f:-0.4f; /* never all three behind, never none */
  for(s=0;s<3;s++){v[s].c=c;v[s].u=(v[s].x+1.0f)*4.0f;v[s].v=(v[s].y+1.0f)*4.0f;}}
}
static void *make_tex(U32 c0,U32 c1) {
 void *t=0;LR lr;U32 x,y;
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,DEV_CREATETEX))(dev,64,64,1,0,bbfmt==23?23:22,1,&t)||!t)return 0;
 if(((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(t,TEX_LOCK))(t,0,&lr,0,0)){((F0)VT(t,D3D_REL))(t);return 0;}
 for(y=0;y<64;y++)for(x=0;x<64;x++){U32 c=(((x>>3)^(y>>3))&1)?c0:c1;unsigned char *row=(unsigned char*)lr.bits+y*lr.pitch;
  if(bbfmt==23)((unsigned short*)row)[x]=(unsigned short)(((c>>19)&31)<<11|((c>>10)&63)<<5|((c>>3)&31));else ((U32*)row)[x]=c|0xFF000000UL;}
 ((F1)VT(t,TEX_UNLOCK))(t,0);return t;
}
static int read_bb(U32 *out) {
 void *bb=0;LR lr;int ok=0;U32 x,y;
 if(((HRESULT(__stdcall*)(void*,U32,U32,void**))VT(dev,DEV_BACKBUF))(dev,0,0,&bb)||!bb)return 0;
 if(!((HRESULT(__stdcall*)(void*,LR*,void*,U32))VT(bb,SURF_LOCK))(bb,&lr,0,0x10)){
  for(y=0;y<WIN;y++){unsigned char *row=(unsigned char*)lr.bits+y*lr.pitch;
   for(x=0;x<WIN;x++)out[y*WIN+x]=bbfmt==23?((unsigned short*)row)[x]:(((U32*)row)[x]&0xFFFFFFUL);}
  ((F0)VT(bb,SURF_UNLOCK))(bb);ok=1;}
 ((F0)VT(bb,D3D_REL))(bb);return ok;
}
static void states(U32 cull) {rs(137,0);rs(7,0);rs(14,0);rs(27,0);rs(22,cull);rs(52,0);rs(28,0);rs(26,0);
 tss(1,4);tss(2,2);tss(3,0);tss(4,2);tss(5,0);tss(16,1);tss(17,1);tss(18,0);tss(13,1);tss(14,1);} /* MODULATE texture x diffuse, point filters, wrap */
/* what: 1 strips, 2 fans, 4 lists, 8 lists with unused vertices outside; reps times */
static void scene(int what,U32 cull,int reps) {
 int r;U32 s;
 ((F0)VT(dev,DEV_BEGIN))(dev);clear(0xFF200020UL);states(cull);((F2P)VT(dev,DEV_SETTEX))(dev,0,tex);
 for(r=0;r<reps;r++){
  if(what&8){rs(136,0);((F1)VT(dev,DEV_SETVS))(dev,0x144);
   for(s=0;s<GN/LROWS;s++)((FDIPUP)VT(dev,DEV_DIPUP))(dev,4,0,(LROWS+1)*(GN+1),bn,bi,101,bv+s*LROWS*(GN+1),sizeof(TL));}
  if(what&4){rs(136,0);((F1)VT(dev,DEV_SETVS))(dev,0x144);
   for(s=0;s<GN/LROWS;s++)((FDIPUP)VT(dev,DEV_DIPUP))(dev,4,0,(LROWS+1)*(GN+1),LROWS*GN*2,li,101,gv+s*LROWS*(GN+1),sizeof(TL));} /* D3DPT_TRIANGLELIST, the block's own vertices */
  if(what&1){rs(136,0);((F1)VT(dev,DEV_SETVS))(dev,0x144); /* pretransformed: no clipping */
   for(s=0;s<GN/ROWS;s++)((FDIPUP)VT(dev,DEV_DIPUP))(dev,5,0,NV,SIX-2,si[s],101,gv,sizeof(TL));} /* D3DPT_TRIANGLESTRIP, 16-bit indices */
  if(what&2){rs(136,1);((F1)VT(dev,DEV_SETVS))(dev,0x142);((FDPUP)VT(dev,DEV_DPUP))(dev,4,NF,fv,sizeof(UV));}}
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);
 ((F0)VT(dev,DEV_END))(dev);}
static void *tx[4];
static void quads(int reps) {
 int r,k;TL q[4];
 ((F0)VT(dev,DEV_BEGIN))(dev);clear(0xFF200020UL);
 rs(137,0);rs(7,0);rs(14,0);rs(22,1);rs(52,0);rs(28,0);rs(26,0);rs(136,0);
 tss(1,4);tss(2,2);tss(3,0);tss(4,2);tss(5,0);tss(13,1);tss(14,1);tss(18,0);
 ((F1)VT(dev,DEV_SETVS))(dev,0x144);
 for(r=0;r<reps;r++)for(k=0;k<64;k++){
  float x0=(float)(k%8)*32.0f,y0=(float)(k/8)*32.0f,x1=x0+32.0f,y1=y0+32.0f;U32 c=((120+(k*37)%130)<<16)|((120+(k*53)%130)<<8)|(120+(k*71)%130);int v;
  int t=(k+r)%4,bl=(k/4)&3,fl=(k/16)&1;
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,tx[t]);
  rs(27,bl==1||bl==2);if(bl==1){rs(19,5);rs(20,6);}else if(bl==2){rs(19,2);rs(20,2);}
  rs(15,bl==3);if(bl==3){rs(25,7);rs(24,0x40);}
  tss(16,fl?2:1);tss(17,fl?2:1);
  q[0].x=x0;q[0].y=y0;q[0].u=0;q[0].v=0;q[1].x=x1;q[1].y=y0;q[1].u=1;q[1].v=0;q[2].x=x0;q[2].y=y1;q[2].u=0;q[2].v=1;q[3].x=x1;q[3].y=y1;q[3].u=1;q[3].v=1;
  for(v=0;v<4;v++){q[v].z=0.5f;q[v].w=1.0f;q[v].c=c|0x90000000UL;}
  ((FDPUP)VT(dev,DEV_DPUP))(dev,5,2,q,sizeof(TL));}
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);rs(27,0);rs(15,0);tss(16,1);tss(17,1);
 ((F0)VT(dev,DEV_END))(dev);
}
static int cmpd(const void *a,const void *b) {double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y;}
#define FR 120
static double all_med[5];
static void timed(int no,const char *name,int what) {
 static double t[FR];int k;LARGE_INTEGER f,a,b;
 QueryPerformanceFrequency(&f);
 for(k=0;k<8;k++){if(what)scene(what,1,16);else quads(6);present();}
 for(k=0;k<FR;k++){QueryPerformanceCounter(&a);if(what)scene(what,1,16);else quads(6);present();QueryPerformanceCounter(&b);t[k]=(double)(b.QuadPart-a.QuadPart)*1000.0/(double)f.QuadPart;}
 qsort(t,FR,sizeof(double),cmpd);all_med[no-1]=t[FR/2];
 say("%d. %-58s median %7.2f ms, 95th percentile %7.2f ms a frame\n",no,name,t[FR/2],t[(FR*95)/100]);
}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
int main(void) {
 HMODULE d8;void *(__stdcall *create)(U32);void *d3d;MODE mode;PP pp;WNDCLASS wc;HRESULT hr;char path[MAX_PATH],*s;DWORD build=0;int k;
 static const U32 col[4][2]={{0xFFFFFFUL,0xB0B0B0UL},{0xFF4040UL,0x602020UL},{0x40FF40UL,0x206020UL},{0x4040FFUL,0x202060UL}};
 {HANDLE v=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);DWORD in=0x75,o[8],got=0;memset(o,0,sizeof(o));
  if(v!=INVALID_HANDLE_VALUE){if(DeviceIoControl(v,0x85500022UL,&in,4,o,32,&got,0))build=o[1];CloseHandle(v);}}
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)sprintf(s+1,"VXAB%lu.LOG",build%1000UL);logf=fopen(path,"w");
 say("VXAB v1.0: five scenes timed, for an A/B between two builds of the driver on one machine\n");
 if(build)say("the VxD's build: .%lu\n",build);else say("the VxD's build: not read (not this driver, or the VxD not open)\n");
 d8=LoadLibrary("d3d8.dll");create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 if(!create){say("d3d8.dll / Direct3DCreate8 not found (DirectX 8 or later is needed)\n");return 1;}
 d3d=create(220);if(!d3d)d3d=create(120);
 if(!d3d){say("Direct3DCreate8 failed\n");return 1;}
 memset(&mode,0,sizeof(mode));((F2P)VT(d3d,D3D_MODE))(d3d,0,&mode);bbfmt=mode.format;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXAB";wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClass(&wc);
 {RECT r;r.left=0;r.top=0;r.right=WIN;r.bottom=WIN;AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION,FALSE);
  wnd=CreateWindow("VXAB","VXAB",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,r.right-r.left,r.bottom-r.top,0,0,wc.hInstance,0);}
 SetWindowPos(wnd,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE);
 memset(&pp,0,sizeof(pp));pp.bw=WIN;pp.bh=WIN;pp.bfmt=mode.format;pp.bcount=1;pp.swap=3;pp.wnd=wnd;pp.windowed=1;pp.autoz=1;pp.zfmt=80;pp.flags=1;
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);
 say("display format %lu (%s); CreateDevice(HAL, %dx%d windowed): %08lX\n",mode.format,mode.format==23?"16 bpp":"32 bpp",WIN,WIN,(U32)hr);
 if(hr||!dev){((F0)VT(d3d,D3D_REL))(d3d);if(logf)fclose(logf);return 1;}
 if(bbfmt!=23&&bbfmt!=22&&bbfmt!=21){say("the desktop must be 16 bpp (R5G6B5) or 32 bpp\n");fails++;}
 else{
  make_scenes();tex=make_tex(0xFFFFFFUL,0xB0B0B0UL);for(k=0;k<4;k++)tx[k]=make_tex(col[k][0],col[k][1]);
  if(!tex||!tx[0]||!tx[1]||!tx[2]||!tx[3]){say("CreateTexture failed\n");fails++;}
  else{
   timed(1,"lists (51200 triangles a frame)",4);
   timed(2,"strips (53504 triangles a frame)",1);
   timed(3,"fans clipped by Direct3D (3840 triangles in a frame)",2);
   timed(4,"states (384 quads, a state change before each)",0);
   timed(5,"lists with unused far-outside vertices (39680 triangles)",8);
   say("medians: %.2f %.2f %.2f %.2f %.2f ms; their sum %.2f ms\n",all_med[0],all_med[1],all_med[2],all_med[3],all_med[4],all_med[0]+all_med[1]+all_med[2]+all_med[3]+all_med[4]);}}
 if(tex)((F0)VT(tex,D3D_REL))(tex);for(k=0;k<4;k++)if(tx[k])((F0)VT(tx[k],D3D_REL))(tx[k]);
 ((F0)VT(dev,D3D_REL))(dev);((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);
 say("%s\n",fails?"FAILED":"done");
 if(logf)fclose(logf);
 return fails?2:0;
}
