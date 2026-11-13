/* VXRTT v1.5 (.533: the frame copy without ACCEL_D3D_RTNOWAIT's off passes, its switch gone). VXRTT v1.4 (v10.479: the back buffer cleared before the sampled quad, so the check sees the copy, not the quads still under it; review) v1.3 (v10.478: the frame-copy test timed without a read-back, with and without CPU work, its Blts apart, retires by cause, the ring copies) v1.2 (v10.477: GTA3's per-frame copy into a render-target texture, with ACCEL_D3D_RTNOWAIT on and off; v1.1, v10.459: A8R8G8B8, alpha kept, 128-2048 sizes; formats Direct3D 8 does not offer on the desktop are not counted) v1.0 (v10.456): Direct3D 8 render-target textures on this driver (cookbook 24.8, P0) and what Direct3D 8 makes of the caps.
 *
 * 1. Direct3D 8's view: the adapter's display format, GetDeviceCaps (TextureCaps with D3DPTEXTURECAPS_MIPMAP 0x4000, the texture size
 *    limits) and CheckDeviceFormat for a render-target texture and a plain texture in X8R8G8B8, R5G6B5, A8R8G8B8, X1R5G5B5, A1R5G5B5.
 * 2. CreateTexture(64, 64, 1, D3DUSAGE_RENDERTARGET, fmt, D3DPOOL_DEFAULT) for X8R8G8B8 and R5G6B5 (and the refusals: A8R8G8B8, 48x48).
 * 3. For each texture made: draw into it (a blue clear, a red quad in the top-left quarter, a green one in the bottom-right), switch back to
 *    the back buffer, draw a 64x64 quad sampling it, read the back buffer through a lock and check the four quarters (corners, orientation,
 *    pitch); then 50 rounds of clear-into-the-texture-then-sample with a new colour each round (sampling straight after drawing).
 *
 * No Direct3D 8 headers are needed: the interfaces are called through their vtables (d3d8.h's order). Results go to the screen and to
 * VXRTT.LOG beside the program. Run it on the desktop (16 or 32 bpp); it opens a 256x256 window for a few seconds. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef unsigned long U32;
#define VT(o,i) (((void**)(*(void***)(o)))[i])
typedef HRESULT (__stdcall *F0)(void*);
typedef HRESULT (__stdcall *F1)(void*,U32);
typedef HRESULT (__stdcall *F1P)(void*,void*);
typedef HRESULT (__stdcall *F2)(void*,U32,U32);
typedef HRESULT (__stdcall *F2P)(void*,U32,void*);
typedef HRESULT (__stdcall *F3P)(void*,U32,U32,void*);
static FILE *logf;
static void say(const char *f,...) {static char b[2048];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a); /* v1.2: bounded (was vsprintf into 512 bytes on the stack) */fputs(b,stdout);if(logf){fputs(b,logf);fflush(logf);}}
static const char *fname(U32 f) {switch(f){case 21:return "A8R8G8B8";case 22:return "X8R8G8B8";case 23:return "R5G6B5";case 24:return "X1R5G5B5";case 25:return "A1R5G5B5";case 80:return "D16";}return "?";}

enum {D3D_REL=2,D3D_FMTCHECK=10,D3D_CAPS=13,D3D_CREATEDEV=15,D3D_MODE=8};
enum {DEV_PRESENT=15,DEV_BACKBUF=16,DEV_CREATETEX=20,DEV_SETRT=31,DEV_GETRT=32,DEV_GETZ=33,DEV_BEGIN=34,DEV_END=35,DEV_CLEAR=36,DEV_SETRS=50,
      DEV_SETTEX=61,DEV_SETTSS=63,DEV_DRAWUP=72,DEV_SETVS=76};
enum {TEX_SURF=15,SURF_LOCK=9,SURF_UNLOCK=10,DEV_COPYRECTS=28};
typedef struct {U32 w,h,refresh,format;} MODE;
typedef struct {U32 bw,bh,bfmt,bcount,ms,swap;HWND wnd;U32 windowed,autoz,zfmt,flags,rate,interval;} PP;
typedef struct {long pitch;void *bits;} LR;
typedef struct {float x,y,z,w;U32 c;} VC;
typedef struct {float x,y,z,w,u,v;} VT1;

static void *dev;static U32 bbfmt;
static HRESULT clear(U32 flags,U32 color) {return ((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,flags,color,1.0f,0);}
static void rs(U32 s,U32 v) {((F2)VT(dev,DEV_SETRS))(dev,s,v);}
static void tss(U32 s,U32 v) {((F3P)VT(dev,DEV_SETTSS))(dev,0,s,(void*)v);}
static void quad_c(float x0,float y0,float x1,float y1,U32 c) {
 VC v[6];int k;float xs[6],ys[6];xs[0]=x0;ys[0]=y0;xs[1]=x1;ys[1]=y0;xs[2]=x0;ys[2]=y1;xs[3]=x1;ys[3]=y0;xs[4]=x1;ys[4]=y1;xs[5]=x0;ys[5]=y1;
 for(k=0;k<6;k++){v[k].x=xs[k]-0.5f;v[k].y=ys[k]-0.5f;v[k].z=0.5f;v[k].w=1.0f;v[k].c=c;}
 ((F1)VT(dev,DEV_SETVS))(dev,0x44);((HRESULT(__stdcall*)(void*,U32,U32,const void*,U32))VT(dev,DEV_DRAWUP))(dev,4,2,v,sizeof(VC));
}
static void quad_t(float x0,float y0,float s) {
 VT1 v[6];int k;static const float u[6]={0,1,0,1,1,0},t[6]={0,0,1,0,1,1};
 for(k=0;k<6;k++){v[k].x=x0+u[k]*s-0.5f;v[k].y=y0+t[k]*s-0.5f;v[k].z=0.5f;v[k].w=1.0f;v[k].u=u[k];v[k].v=t[k];}
 ((F1)VT(dev,DEV_SETVS))(dev,0x104);((HRESULT(__stdcall*)(void*,U32,U32,const void*,U32))VT(dev,DEV_DRAWUP))(dev,4,2,v,sizeof(VT1));
}
/* the back buffer's pixel at x, y as X8R8G8B8 (R5G6B5 widened); 0xFFFFFFFF when the lock fails */
static U32 bb_pixel(int x,int y) {
 void *bb=0;LR lr;U32 v=0xFFFFFFFFUL;
 if(((HRESULT(__stdcall*)(void*,U32,U32,void**))VT(dev,DEV_BACKBUF))(dev,0,0,&bb)||!bb)return v;
 if(!((HRESULT(__stdcall*)(void*,LR*,void*,U32))VT(bb,SURF_LOCK))(bb,&lr,0,0x10)) {
  unsigned char *row=(unsigned char*)lr.bits+y*lr.pitch;
  if(bbfmt==23){unsigned short p=((unsigned short*)row)[x];v=((U32)(p>>11)*255/31)<<16|((U32)((p>>5)&63)*255/63)<<8|((U32)(p&31)*255/31);}
  else v=((U32*)row)[x]&0xFFFFFFUL;
  ((F0)VT(bb,SURF_UNLOCK))(bb);
 }
 ((F0)VT(bb,D3D_REL))(bb);return v;
}
static int near_rgb(U32 a,U32 b) {int k;for(k=0;k<24;k+=8){int d=(int)((a>>k)&255)-(int)((b>>k)&255);if(d<-12||d>12)return 0;}return 1;}
static int fails;
static void expect(const char *what,U32 got,U32 want) {int ok=near_rgb(got,want);if(!ok)fails++;say("  %-34s %06lX (want %06lX) %s\n",what,got,want,ok?"ok":"WRONG");}

static void test_format(U32 fmt) {
 void *tex=0,*surf=0,*bb=0,*zs=0;HRESULT hr;int r,bad=0;
 hr=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,DEV_CREATETEX))(dev,64,64,1,1,fmt,0,&tex);
 say("CreateTexture 64x64 RENDERTARGET %-8s: %08lX\n",fname(fmt),(U32)hr);
 if(hr||!tex){fails++;return;}
 if(((F2P)VT(tex,TEX_SURF))(tex,0,&surf)||!surf){say("  GetSurfaceLevel failed\n");fails++;goto out;}
 ((F1P)VT(dev,DEV_GETRT))(dev,&bb);((F1P)VT(dev,DEV_GETZ))(dev,&zs);
 for(r=0;r<51;r++) {
  U32 cc=r?(0x10203UL*(U32)(r*5))|0x400000UL:0x0000FFUL;
  hr=((HRESULT(__stdcall*)(void*,void*,void*))VT(dev,DEV_SETRT))(dev,surf,0);
  if(hr){say("  SetRenderTarget(texture): %08lX\n",(U32)hr);fails++;break;}
  ((F0)VT(dev,DEV_BEGIN))(dev);rs(7,0);rs(22,1);rs(137,0);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);
  clear(1,0xFF000000UL|cc);
  if(!r){quad_c(0,0,32,32,0xFFFF0000UL);quad_c(32,32,64,64,0xFF00FF00UL);}
  ((F0)VT(dev,DEV_END))(dev);
  ((HRESULT(__stdcall*)(void*,void*,void*))VT(dev,DEV_SETRT))(dev,bb,zs);
  ((F0)VT(dev,DEV_BEGIN))(dev);clear(zs?3:1,0xFF000000UL);
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,tex);tss(1,2);tss(2,2);tss(4,2);tss(5,2);tss(16,1);tss(17,1);tss(18,0);
  quad_t(16,16,64);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);
  ((F0)VT(dev,DEV_END))(dev);
  if(!r){
   expect("top-left quarter (red)",bb_pixel(16+8,16+8),0xFF0000UL);
   expect("bottom-right quarter (green)",bb_pixel(16+48,16+48),0x00FF00UL);
   expect("top-right quarter (blue clear)",bb_pixel(16+48,16+8),0x0000FFUL);
   expect("bottom-left quarter (blue clear)",bb_pixel(16+8,16+48),0x0000FFUL);
   expect("outside the quad (black)",bb_pixel(100,100),0);
  } else if(!near_rgb(bb_pixel(16+48,16+8),cc)){bad++;if(bad<=3)say("  round %d: sampled %06lX, drawn %06lX\n",r,bb_pixel(16+48,16+8),cc);}
  ((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);
 }
 say("  50 rounds of draw-then-sample: %d wrong\n",bad);fails+=bad;
 if(fmt==21&&!bad) { /* v1.1: does the texture keep alpha? cleared to alpha 40, a quad drawn with vertex alpha 80 (no blending), sampled with
                     * the alpha replicated into the colour: 404040 where cleared, 808080 where drawn if the engine writes alpha (information) */
  U32 a,b;
  ((HRESULT(__stdcall*)(void*,void*,void*))VT(dev,DEV_SETRT))(dev,surf,0);((F0)VT(dev,DEV_BEGIN))(dev);rs(27,0);clear(1,0x40112233UL);quad_c(0,0,32,32,0x80FF0000UL);((F0)VT(dev,DEV_END))(dev);
  ((HRESULT(__stdcall*)(void*,void*,void*))VT(dev,DEV_SETRT))(dev,bb,zs);((F0)VT(dev,DEV_BEGIN))(dev);clear(zs?3:1,0xFF000000UL);
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,tex);tss(1,2);tss(2,0x22);tss(4,2);tss(5,2);quad_t(16,16,64);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);tss(2,2);((F0)VT(dev,DEV_END))(dev);
  a=bb_pixel(16+48,16+8);b=bb_pixel(16+8,16+8);
  say("  alpha kept (information): the clear's %06lX (want 404040) %s, a draw's %06lX (808080 if the engine writes alpha) %s\n",a,near_rgb(a,0x404040UL)?"ok":"no",b,near_rgb(b,0x808080UL)?"yes":"no");
  ((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);
 }
out:
 if(surf)((F0)VT(surf,D3D_REL))(surf);if(bb)((F0)VT(bb,D3D_REL))(bb);if(zs)((F0)VT(zs,D3D_REL))(zs);if(tex)((F0)VT(tex,D3D_REL))(tex);
}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
/* v1.2 (driver v10.477; board, GTA3 v10.476: one 640 x 480 copy a gameplay frame from the back buffer into a render-target texture,
 * 2.94 ms in the Blt: v10.456's wait for every draw in flight before any Blt into a render-target texture, though the copy goes into XP's
 * ring after them): GTA3's per-frame copy, here: each frame 16 full-window quads in the frame's colour, CopyRects of the back buffer into a
 * 256x256 render-target texture, a 64x64 quad sampling it; with ACCEL_D3D_RTNOWAIT (VX_D3D_Switch 8) on and off in turn. Checked: the
 * sampled quad is this frame's colour (the copy after the draws, before the sampling, without the wait). Measured: ms a frame, the Blt's
 * ms (VX_DD_QueryBlts), the retires for a CPU access a frame (TEXDBG BC CA). */
static DWORD (WINAPI *hal_blts)(DWORD *,DWORD);static HANDLE vxd=INVALID_HANDLE_VALUE;
static int texdbg2(DWORD code,DWORD i,DWORD *o) {DWORD in[2],got=0;in[0]=code;in[1]=i;memset(o,0,32);return vxd!=INVALID_HANDLE_VALUE&&DeviceIoControl(vxd,0x85500022UL,in,8,o,32,&got,NULL);}
static void causes(U32 *n) {DWORD o[8];int k;for(k=0;k<6;k++)n[k]=0;if(texdbg2(0xBC,0xCA,o)&&(o[0]&0xFF)==0xBC&&o[1]==0xCA)for(k=0;k<6;k++)n[k]=o[2+k];}
static int ringcopies(U32 *n) {DWORD o[8];int k;for(k=0;k<5;k++)n[k]=0;if(!texdbg2(0xBC,0xB7,o)||(o[0]&0xFF)!=0xBC||o[1]!=0xB7)return 0;for(k=0;k<5;k++)n[k]=o[3+k];return 1;}
static double qms(LARGE_INTEGER a,LARGE_INTEGER b,LARGE_INTEGER f) {return (double)(b.QuadPart-a.QuadPart)*1000.0/(double)f.QuadPart;}
/* v1.3 (driver v10.478; board, VXRTT v1.2: with ACCEL_D3D_RTNOWAIT on one retire a frame fewer and the picture right, but the frame time
 * the same: v1.2 read a back-buffer pixel every frame, a retire of everything drawn, and its 16 quads keep the engine busy longer than the
 * CPU, so the wait only moved; its "copy's Blt" averaged every Blt of the pass, Present's copy to the window too). Each pass: 30 timed
 * frames without a read-back, then 3 checked frames with it. Two loads: no CPU work, and 6 ms of CPU work a frame after the copy (a game's
 * logic while the engine draws): only there does a wait the CPU no longer makes show in the frame time. Measured: ms a frame, the
 * CopyRects call, the HAL's Blt into the render-target texture (class copy to a 3D target) and Present's copy to the window apart, the
 * retires by cause, the VxD's ring copies (TEXDBG BC B7: in the ring, not taken, by the 2D engine after the wait for the 3D work). */
static void test_frame_copy(void) {
 void *t=0,*ts=0,*bb=0;HRESULT hr;U32 fmt=bbfmt==23?23:22,pass,f,k;LARGE_INTEGER q0,q1,qf,c0,c1;
 static const U32 col[4]={0xFF2080E0UL,0xFFE04020UL,0xFF20C040UL,0xFFC0C020UL};
 static const char *cn[6]={"untagged","DirectDraw blit","GDI/2D","Lock/Unlock","flip","2D sync"};
 hr=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,DEV_CREATETEX))(dev,256,256,1,1,fmt,0,&t);
 if(hr||!t){say("frame copy test: CreateTexture 256x256 RENDERTARGET %s %08lX\n",fname(fmt),(U32)hr);fails++;return;}
 ((HRESULT(__stdcall*)(void*,U32,void**))VT(t,TEX_SURF))(t,0,&ts);
 QueryPerformanceFrequency(&qf);
 say("frame copy (v1.4, GTA3's per-frame copy): 16 full-window quads, CopyRects of the back buffer into a 256x256 render-target %s, a quad sampling it;\n"
     "   30 timed frames a pass without a read-back, then 3 frames checked by reading the back buffer\n",fname(fmt));
 for(pass=0;pass<4;pass++){U32 work=pass>=2?6:0, /* v1.5 (.533): ACCEL_D3D_RTNOWAIT always on since .524, its switch (VX_D3D_Switch 8) gone: no off passes */wrong=0,r0[6],r1[6],g0[5],g1[5],rg=0;double ms,cms=0,tb=0,pb=0;DWORD b[69];
  for(f=0;f<36;f++){U32 c=col[f&3],got;
   if(f==3){if(hal_blts)hal_blts(b,69|0x80000000UL);causes(r0);rg=ringcopies(g0);QueryPerformanceCounter(&q0);}
   if(f==33){QueryPerformanceCounter(&q1);causes(r1);ringcopies(g1);memset(b,0,sizeof(b));if(!hal_blts||hal_blts(b,69)<53)b[0]=0;}
   ((F0)VT(dev,DEV_BEGIN))(dev);clear(1,0xFF000000UL);rs(7,0);rs(27,0);rs(22,1);
   ((F2P)VT(dev,61))(dev,0,0);tss(1,2);tss(2,0);tss(4,2);tss(5,0);for(k=0;k<16;k++)quad_c(0,0,256,256,c);((F0)VT(dev,DEV_END))(dev); /* the diffuse colour (D3DTA_DIFFUSE) */
   ((HRESULT(__stdcall*)(void*,U32,U32,void**))VT(dev,DEV_BACKBUF))(dev,0,0,&bb);
   QueryPerformanceCounter(&c0);
   hr=((HRESULT(__stdcall*)(void*,void*,const RECT*,U32,void*,const POINT*))VT(dev,DEV_COPYRECTS))(dev,bb,0,0,ts,0);
   QueryPerformanceCounter(&c1);if(f>=3&&f<33)cms+=qms(c0,c1,qf);
   if(bb){((F0)VT(bb,D3D_REL))(bb);bb=0;}
   if(hr){say("   CopyRects %08lX\n",(U32)hr);fails++;break;}
   if(work){LARGE_INTEGER w0,w1;QueryPerformanceCounter(&w0);do QueryPerformanceCounter(&w1);while(qms(w0,w1,qf)<(double)work);}
   ((F0)VT(dev,DEV_BEGIN))(dev);clear(1,0xFF000000UL);((F2P)VT(dev,61))(dev,0,t);tss(1,2);tss(2,2);tss(4,2);tss(5,2);tss(16,1);tss(17,1);quad_t(192,192,64);((F2P)VT(dev,61))(dev,0,0);((F0)VT(dev,DEV_END))(dev);
   if(f>=33){got=bb_pixel(224,224);if(!near_rgb(got,c&0xFFFFFFUL))wrong++;got=bb_pixel(64,64);if(!near_rgb(got,0))wrong++;} /* v1.4: and the clear (outside the quad) black */
   ((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);}
  if(f<36)break;
  ms=qms(q0,q1,qf)/30.0;
  if(b[0]){if(b[1+9])tb=b[17+9]*256.0/b[0]/b[1+9];if(b[1+8])pb=b[17+8]*256.0/b[0]/b[1+8];}
  say("   %s: %.2f ms a frame; CopyRects %.3f ms (the HAL's Blt into the texture %.3f ms, %lu calls, %lu on the CPU); Present's copy %.3f ms\n",
   work?"6 ms CPU work":"no CPU work",ms,cms/30.0,tb,b[1+9],b[53+9],pb);
  {char line[256];int n=0;for(k=0;k<6;k++)if(r1[k]-r0[k])n+=sprintf(line+n,"%s%s %.2f",n?", ":"",cn[k],(r1[k]-r0[k])/30.0);if(!n)strcpy(line,"none");
   say("      retires a frame by cause: %s\n",line);}
  if(rg&&b[0])say("      the VxD's ring copies: in the ring %lu, not taken %lu; by the 2D engine after the wait for the 3D work %lu (declined %lu), that wait %.3f ms each\n",
   g1[0]-g0[0],g1[1]-g0[1],g1[2]-g0[2],g1[3]-g0[3],(g1[2]-g0[2]+g1[3]-g0[3])?(g1[4]-g0[4])*256.0/b[0]/(g1[2]-g0[2]+g1[3]-g0[3]):0.0);
  say("      checked frames: the sampled quad this frame's colour and the cleared rest black: %lu wrong of 6 %s\n",wrong,wrong?"WRONG":"ok");
  if(wrong)fails++;}
 if(ts)((F0)VT(ts,D3D_REL))(ts);((F0)VT(t,D3D_REL))(t);
}

int main(void) {
 HMODULE d8;void *(__stdcall *create)(U32);void *d3d;MODE mode;static U32 caps[128];PP pp;HWND wnd;WNDCLASS wc;HRESULT hr;U32 k;
 char path[MAX_PATH],*s;static const U32 fm[5]={22,23,21,24,25};
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXRTT.LOG");logf=fopen(path,"w");
 say("VXRTT v1.5 (driver .533): Direct3D 8 render-target textures\n");
 {char dll[MAX_PATH];UINT n=GetSystemDirectoryA(dll,MAX_PATH-16);HMODULE h=0; /* v1.2: the HAL by its full path, before the device (VXFETCH's way) */
  if(n&&n<MAX_PATH-16){strcpy(dll+n,dll[n-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");h=LoadLibraryA(dll);}
  hal_blts=h?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(h,"VX_DD_QueryBlts"):0;
  vxd=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);}
 d8=LoadLibrary("d3d8.dll");create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 if(!create){say("d3d8.dll / Direct3DCreate8 not found (DirectX 8 or later is needed)\n");return 1;}
 d3d=create(220);if(!d3d)d3d=create(120);
 if(!d3d){say("Direct3DCreate8 failed\n");return 1;}
 memset(&mode,0,sizeof(mode));hr=((F2P)VT(d3d,D3D_MODE))(d3d,0,&mode);
 say("display %lux%lu format %lu (%s): %08lX\n",mode.w,mode.h,mode.format,fname(mode.format),(U32)hr);
 hr=((F3P)VT(d3d,D3D_CAPS))(d3d,0,1,caps);
 say("GetDeviceCaps(HAL): %08lX  TextureCaps %08lX (MIPMAP 0x4000 %s, POW2 %s, CUBEMAP %s, MIPCUBEMAP %s)  max texture %lux%lu\n",(U32)hr,caps[15],
  (caps[15]&0x4000)?"yes":"no",(caps[15]&2)?"yes":"no",(caps[15]&0x800)?"yes":"no",(caps[15]&0x10000)?"yes":"no",caps[22],caps[23]);
 say("  DevCaps %08lX  Caps2 %08lX  TextureFilterCaps %08lX\n",caps[7],caps[3],caps[16]);
 for(k=0;k<5;k++) {
  HRESULT a=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32))VT(d3d,D3D_FMTCHECK))(d3d,0,1,mode.format,1,3,fm[k]);
  HRESULT b=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32))VT(d3d,D3D_FMTCHECK))(d3d,0,1,mode.format,0,3,fm[k]);
  HRESULT c=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32))VT(d3d,D3D_FMTCHECK))(d3d,0,1,mode.format,1,1,fm[k]);
  say("CheckDeviceFormat %-8s: render-target texture %08lX  texture %08lX  render-target surface %08lX\n",fname(fm[k]),(U32)a,(U32)b,(U32)c);
 }
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXRTT";wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClass(&wc);
 wnd=CreateWindow("VXRTT","VXRTT",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,264,284,0,0,wc.hInstance,0);
 memset(&pp,0,sizeof(pp));pp.bw=256;pp.bh=256;pp.bfmt=mode.format;pp.bcount=1;pp.swap=3;pp.wnd=wnd;pp.windowed=1;pp.autoz=1;pp.zfmt=80;pp.flags=1;
 bbfmt=mode.format;
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);
 say("CreateDevice(HAL, 256x256 windowed, D16): %08lX\n",(U32)hr);
 if(hr||!dev){((F0)VT(d3d,D3D_REL))(d3d);if(logf)fclose(logf);return 1;}
 {static const U32 tf[3]={22,21,23};for(k=0;k<3;k++){ /* v1.1: only what Direct3D 8 offers on this desktop (it refuses the others itself) */
  HRESULT a=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32))VT(d3d,D3D_FMTCHECK))(d3d,0,1,mode.format,1,3,tf[k]);
  if(a)say("%s render-target texture: not offered by Direct3D 8 on this desktop (%08lX), not tested\n",fname(tf[k]),(U32)a);else test_format(tf[k]);}}
 {static const U32 tf[2]={22,21};U32 j,sz;for(j=0;j<2;j++)for(sz=128;sz<=2048;sz*=2){void *t=0; /* v1.1: the sizes a benchmark asks for */
   hr=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,DEV_CREATETEX))(dev,sz,sz,1,1,tf[j],0,&t);
   say("CreateTexture %lux%lu RENDERTARGET %-8s: %08lX %s\n",sz,sz,fname(tf[j]),(U32)hr,hr?"WRONG":"ok");if(hr&&hr!=0x8876017CUL)fails++;if(t)((F0)VT(t,D3D_REL))(t);}}
 {void *t=0;hr=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,DEV_CREATETEX))(dev,48,48,1,1,22,0,&t);
  say("CreateTexture 48x48 RENDERTARGET X8R8G8B8: %08lX (not a power of two: refused, as expected)\n",(U32)hr);if(t)((F0)VT(t,D3D_REL))(t);}
 test_frame_copy(); /* v1.2; v1.3 */
 Sleep(1500);
 ((F0)VT(dev,D3D_REL))(dev);((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);
 say("%s: %d wrong\n",fails?"FAILED":"PASSED",fails);
 if(logf)fclose(logf);
 return fails?2:0;
}
