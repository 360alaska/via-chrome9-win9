/* VXGL v5.6 (driver .593: the status at the first read after the command listed; every command now waited for from its start). VXGL v5.5 (driver .592: a 2D copy by the VxD after each of 200 swaps, its start timed). VXGL v5.4 (driver .591: the ICD's front-buffer steps of wrong and right tries from its log). VXGL v5.3 (driver .590: the 2D engine's counters for right and wrong front-clear tries). VXGL v5.2 (driver .590: the ICD logs for the run, its front-buffer lines copied to VXGL.TXT). VXGL v5.1 (driver .589: two more front-clear rows with the asynchronous copies switched off). VXGL v5.0 (driver .588: the front clear after a swap, three ways, 30 times each). VXGL v4.9 (driver .588: the front clear repeated 40 times after the checks). VXGL v4.8 (driver .533: the probes of the VxD's switch words removed, the words gone since .530). v4.7 (driver v10.430: the Coarse Z probe gone; v4.6 driver v10.357: WGL_ARB_pbuffer check, GL_CLAMP filtered linearly; v4.5 with v10.353: KIL as the alpha test, a fragment program's fog option, smooth points with the alpha test, MIRROR_CLAMP_TO_EDGE magnified and mipmapped; v4.4 with v10.321: antialiasing, fragment programs and shaders, NPOT repeat and mip chains, a cube map or 3D texture with a second unit, render to texture through framebuffer objects; v4.3 with v10.320: the 3D texture probe gone, settled by v10.319: 02/22 bit 15; v4.2 with v10.319: the engine-off run checked to be software; v4.1 with v10.318: the 3D texture probe; v4.0 with v10.317: NPOT, rectangle, border, FXT1, colour-table, depth-texture, LOD, DOT3_RGB, cube-mip and 3D-texture scenes, front-buffer drawing checked; v3.9 with v10.315: occlusion queries checked; v3.8 with v10.310: the engine-off run asked for through the environment, not the registry; v3.7 with v10.309: three units on for the run, quad by quad; v3.6 with v10.308: polygon stipple, the speed scene timed before scene 37; v3.5 with v10.307: three texture units, S3TC; v3.4 with v10.304: blend equations and constant colour, scaled combine, LERP, border colour, long and unit-1 mip chains; v3.3 with v10.303: GL_BLEND texture last-read experiments; v3.2 with v10.301: Coarse Z without the tag reset in every set-up; v3.1 with v10.300: BLEND env, q, textured lines, line stipple, sprites; v3.0 with v10.299: Coarse Z speed experiments; v2.9 with v10.298: Coarse Z with conservative merging, timed; v2.8 with v10.297: Coarse Z experiments; v2.7 with v10.296: a Coarse Z off probe; v2.6 with v10.295: logic ops, colour masks, wide lines and points, textured points; v2.5 with v10.251, v2.4 with v10.250, v2.3 and v2.2 with v10.249, v2.1 with v10.247, v2.0 with v10.245, v1.9 with v10.244, v1.8 with v10.243, v1.7 with v10.242, v1.3-v1.6 with v10.241, v1.2 with v10.241, v1.1 with v10.240, v1.0 with v10.239): the OpenGL ICD (VXGL.DLL). Each scene is drawn three times into a
 * 256x256 window and read back with glReadPixels: by the ICD with the engine, by Microsoft's software OpenGL (a generic pixel format),
 * and by the ICD with the engine off (HKLM\Software\VX85598 ACCEL_OPENGL_HW = 0 for that context, then put back as it was: Mesa's
 * software rasterizer, the ICD's own fallback). The engine is judged against the third: the same OpenGL rules but the engine's, as the
 * host test does (v1.2). Microsoft's renderer is a reference only: it splits quads on the other diagonal (OpenGL allows either), dithers
 * blended pixels at 16 bpp and rounds edges its own way. Log: VXGL.TXT; an engine scene more than 1% off writes VXGL_nn_HW.BMP and
 * VXGL_nn_SW.BMP (the ICD without the engine), one more than 5% off Microsoft's VXGL_nn_MS.BMP. "VXGL n" draws the speed scene n
 * frames (default 100). v1.3: four more stencil and depth scenes (who writes, who tests), a multipass GL_EQUAL scene (shown, not
 * judged), no dithering in the scenes, and the stencil and depth scenes again with the VxD's state cache off and with every engine path
 * synchronous (probes). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>

#define W 256
#define H 256
#define VXGL_NS 64 /* v4.0: the scenes the image arrays hold (v4.4: 64) */
static FILE *logf;
static unsigned char img[3][VXGL_NS][W*H*4];
static int have[3][VXGL_NS];
static double ms[3][VXGL_NS];static double sp_pre[3]; /* v3.6: the speed scene timed again before scene "three texture units" */
static int frames=100,failures,engine_on,icd_zbits=24;
/* v1.3: the stencil and depth scenes again with the VxD's engine paths switched (TEXDBG, as VXDISCO's tuning): v1.4: 0 nothing switched
 * (the control: the same scenes later in the run), 1 the state cache off, 2 everything synchronous through the registers (fetcher, async
 * and state cache off) */
#define VX_IOCTL_3DN_TEXDBG 0x85500022UL
/* v3.5 (v10.307): the polygon stipple probe (TEXDBG 0x48 + n, vx_native3d_hw.c stip_pre): a white full-window quad over black in each
 * experiment; the log shows the 32x32 cell at the window's top left and how many pixels repeat 32 to the right and below (v3.3's
 * last-read probe is gone: v10.303 settled it) */
static GLuint zbuf[2][W*H];static int zhave[2];
static const char *scene_name[32];
static int nscenes;

static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcA(h,m,w,l);}

#include "vx_gl_scenes.h"
typedef char ns_fits[NS<=VXGL_NS?1:-1]; /* the image arrays hold VXGL_NS scenes (v2.6: 28; v3.1: 34; v3.4: 40; v3.6: 44; v4.0: 52; the probe's only the stencil and depth scenes, below 24) */
/* v1.5: the top of the screen (rows 0..35, above the test windows at y = 40) is watched for writes that are not ours: the board showed a
 * dark band there from the first scenes with the engine on, none with it off. Read back through GDI after each step; each change is
 * logged once (where, how many pixels, the colour before and after) and becomes the new reference. */
#define SROWS 36
static HDC sdc;static HBITMAP sbm;static DWORD *sbits,*sbase,*snow;static int swide,sreports;
static void screen_grab(DWORD *dst) {HDC s=GetDC(0);BitBlt(sdc,0,0,swide,SROWS,s,0,0,SRCCOPY);ReleaseDC(0,s);GdiFlush();memcpy(dst,sbits,(size_t)swide*SROWS*4);}
static void screen_init(void) {
 BITMAPINFO bi;HDC s=GetDC(0);swide=GetDeviceCaps(s,HORZRES);sdc=CreateCompatibleDC(s);
 memset(&bi,0,sizeof(bi));bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=swide;bi.bmiHeader.biHeight=-SROWS;bi.bmiHeader.biPlanes=1;
 bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;sbm=CreateDIBSection(s,&bi,DIB_RGB_COLORS,(void**)&sbits,0,0);ReleaseDC(0,s);
 if(!sbm){fprintf(logf,"screen watch: no DIB section\n");return;}
 SelectObject(sdc,sbm);sbase=(DWORD*)malloc((size_t)swide*SROWS*4);snow=(DWORD*)malloc((size_t)swide*SROWS*4);
 if(!sbase||!snow){sbm=0;return;}
 screen_grab(sbase);
}
static void screen_check(const char *when,int k) {
 int i,n=0,first=-1,x0=99999,x1=-1,y0=99999,y1=-1;
 if(!sbm||sreports>=8)return;
 screen_grab(snow);
 for(i=0;i<swide*SROWS;i++)if(snow[i]!=sbase[i]){int x=i%swide,y=i/swide;if(first<0)first=i;n++;if(x<x0)x0=x;if(x>x1)x1=x;if(y<y0)y0=y;if(y>y1)y1=y;}
 if(!n)return;
 sreports++;
 fprintf(logf,"   SCREEN rows 0-%d changed %s",SROWS-1,when);if(k>=0)fprintf(logf," %d (%s)",k,scenes[k].name);
 fprintf(logf,": %d pixels in %d,%d-%d,%d; first at %d,%d: was %06lX, now %06lX\n",n,x0,y0,x1,y1,first%swide,first/swide,(unsigned long)(sbase[first]&0xFFFFFF),(unsigned long)(snow[first]&0xFFFFFF));
 fflush(logf);
 memcpy(sbase,snow,(size_t)swide*SROWS*4);
}
/* v1.4: the depth the engine stores against swrast's: a plane z = 0.6 - 0.8 x/256 - 0.6 y/256 (window depth (1 - z) / 2, 0.2 .. 0.9)
 * drawn with GL_ALWAYS and read back with glReadPixels(GL_DEPTH_COMPONENT); 0 by the ICD with the engine, 1 without it */
static void depth_measure(int slot,HDC dc) {
 reset();glEnable(GL_DEPTH_TEST);glDepthFunc(GL_ALWAYS);glColor3f(0.5f,0.5f,0.5f);
 glBegin(GL_TRIANGLES);glVertex3f(0,0,0.6f);glVertex3f(256,0,-0.2f);glVertex3f(256,256,-0.8f);glVertex3f(0,0,0.6f);glVertex3f(256,256,-0.8f);glVertex3f(0,256,0.0f);glEnd();
 glDepthFunc(GL_LESS);glFinish();glPixelStorei(GL_PACK_ALIGNMENT,4);glReadPixels(0,0,W,H,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,zbuf[slot]);
 zhave[slot]=glGetError()==GL_NO_ERROR;SwapBuffers(dc);
}
static void depth_report(int zbits) {
 long hist[2][11],i,k;double maxz=zbits>16?16777215.0:65535.0;int sh=zbits>16?8:16;
 static const int sx[5]={0,255,128,10,200},sy[5]={0,0,128,250,60};
 if(!zhave[0]||!zhave[1]){fprintf(logf,"== depth: not read back\n");return;}
 memset(hist,0,sizeof(hist));
 for(i=0;i<W*H;i++){int x=(int)(i%W),y=(int)(i/W);double z=0.6-0.8*(x+0.5)/256-0.6*(y+0.5)/256;long ideal=(long)((1-z)/2*maxz+0.5);
  for(k=0;k<2;k++){long d=(long)(zbuf[k][i]>>sh)-ideal;if(d<-5)d=-5;if(d>5)d=5;hist[k][d+5]++;}}
 fprintf(logf,"== depth stored for a plane, against the exact value rounded ((1 - z) / 2 x %.0f): pixels by difference -5 or less .. +5 or more\n",maxz);
 for(k=0;k<2;k++){fprintf(logf,"   %s:",k?"without the engine (swrast)":"the engine              ");for(i=0;i<11;i++)fprintf(logf," %6ld",hist[k][i]);fprintf(logf,"\n");}
 for(k=0;k<5;k++){int x=sx[k],y=sy[k];double z=0.6-0.8*(x+0.5)/256-0.6*(y+0.5)/256;
  fprintf(logf,"   pixel %3d,%3d: exact %.2f, engine %lu, swrast %lu\n",x,y,(1-z)/2*maxz,(unsigned long)(zbuf[0][y*W+x]>>sh),(unsigned long)(zbuf[1][y*W+x]>>sh));}
}

/* ------------------------------------------------------------------ renderers */
static int choose_format(HDC dc,int generic) {
 PIXELFORMATDESCRIPTOR p;int n=DescribePixelFormat(dc,1,sizeof(p),&p),i,best=0,score=-1;
 for(i=1;i<=n;i++) {
  int s;DescribePixelFormat(dc,i,sizeof(p),&p);
  if(!(p.dwFlags&PFD_SUPPORT_OPENGL)||!(p.dwFlags&PFD_DRAW_TO_WINDOW)||!(p.dwFlags&PFD_DOUBLEBUFFER)||p.iPixelType!=PFD_TYPE_RGBA)continue;
  if(((p.dwFlags&PFD_GENERIC_FORMAT)!=0)!=generic)continue;
  if(generic&&(p.dwFlags&PFD_GENERIC_ACCELERATED))continue;
  if(p.cDepthBits<16)continue;
  s=(p.cStencilBits>=8?100:0)+(p.cDepthBits>=24?10:0)+(p.cColorBits==GetDeviceCaps(dc,BITSPIXEL)?1000:0);
  if(s>score){score=s;best=i;}
 }
 if(best){DescribePixelFormat(dc,best,sizeof(p),&p);fprintf(logf,"   pixel format %d: flags %08lX, colour %d (%d%d%d), depth %d, stencil %d, accum %d\n",best,(unsigned long)p.dwFlags,p.cColorBits,p.cRedBits,p.cGreenBits,p.cBlueBits,p.cDepthBits,p.cStencilBits,p.cAccumBits);}
 return best;
}
/* v2.2 (v10.249): what the scenes cannot show, since the engine and the ICD's software path would agree on a wrong answer: the front
 * and back buffers kept apart, alpha 1.0 from a buffer without alpha bits, and front/back stencil set apart (glStencilFuncSeparate) */
/* v5.4: a line of this program's own in the ICD's log (appended as the ICD appends its lines), to tell the tries apart */
static void mark(const char *f,...) {char b[160];va_list ap;DWORD n;HANDLE h;va_start(ap,f);_vsnprintf(b,sizeof(b)-1,f,ap);b[sizeof(b)-1]=0;va_end(ap);
 h=CreateFileA("C:\\VXGL.LOG",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);if(h==INVALID_HANDLE_VALUE)return;
 SetFilePointer(h,0,0,FILE_END);WriteFile(h,b,(DWORD)strlen(b),&n,0);CloseHandle(h);}
typedef void (APIENTRY *sfs_fn)(GLenum,GLenum,GLint,GLuint);
static void gl_checks(int which,HDC dc) {
 GLubyte px[4],st=0;int bad=0;PIXELFORMATDESCRIPTOR p;sfs_fn sfs=(sfs_fn)wglGetProcAddress("glStencilFuncSeparate");
 DescribePixelFormat(dc,GetPixelFormat(dc),sizeof(p),&p);
 reset();glPixelStorei(GL_PACK_ALIGNMENT,1);
 /* v2.4: the front buffer again with the generic renderer too (v2.2 faulted there: our driver's DCI primary surface, v10.250) */
 {
  glClearColor(1,0,0,1);glDrawBuffer(GL_BACK);glClear(GL_COLOR_BUFFER_BIT);
  glDrawBuffer(GL_FRONT);glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);glDrawBuffer(GL_BACK);glFinish();
  glReadBuffer(GL_BACK);glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  fprintf(logf,"   check: back after a front clear %02X%02X%02X (want red)",px[0],px[1],px[2]);if(px[0]<200||px[2]>50)bad++;
  glReadBuffer(GL_FRONT);glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  fprintf(logf,", front %02X%02X%02X (want blue)",px[0],px[1],px[2]);if(px[2]<200||px[0]>50)bad++;
  /* v4.0 (v10.317): a green triangle drawn into the front only (the engine draws it there from v10.317): green inside, blue outside, the
   * back still red under it */
  glDrawBuffer(GL_FRONT);glColor3f(0,1,0);glBegin(GL_TRIANGLES);glVertex2f(40,40);glVertex2f(200,50);glVertex2f(120,200);glEnd();glColor3f(1,1,1);
  glDrawBuffer(GL_BACK);glFinish();
  glReadBuffer(GL_FRONT);glReadPixels(120,100,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  fprintf(logf,", front triangle %02X%02X%02X (want green)",px[0],px[1],px[2]);if(px[1]<200||px[0]>50||px[2]>50)bad++;
  glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  fprintf(logf," beside it %02X%02X%02X (want blue)",px[0],px[1],px[2]);if(px[2]<200||px[0]>50||px[1]>50)bad++;
  glReadBuffer(GL_BACK);glReadPixels(120,100,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  fprintf(logf,", back under it %02X%02X%02X (want red)",px[0],px[1],px[2]);if(px[0]<200||px[1]>50||px[2]>50)bad++;
 }
 if(!p.cAlphaBits){glClearColor(0,0,0,0.25f);glClear(GL_COLOR_BUFFER_BIT);glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  fprintf(logf,", alpha %u (no alpha bits: want 255)",px[3]);if(px[3]!=255)bad++;}
 if(sfs&&p.cStencilBits){
  glClearStencil(0);glClear(GL_STENCIL_BUFFER_BIT);glEnable(GL_STENCIL_TEST);
  sfs(GL_FRONT,GL_ALWAYS,1,0xFF);sfs(GL_BACK,GL_ALWAYS,3,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
  glBegin(GL_TRIANGLES);glVertex2f(20,30);glVertex2f(60,200);glVertex2f(110,40);glEnd();
  glDisable(GL_STENCIL_TEST);glFinish();glReadPixels(63,90,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&st);
  sfs(GL_FRONT_AND_BACK,GL_ALWAYS,0,0xFF);
  fprintf(logf,", back-face stencil %u (want 3)",st);if(st!=3)bad++;}
 else fprintf(logf,", separate stencil not available");
 if(glGetError())bad++;
 fprintf(logf,": %s\n",bad?"FAIL":"pass");
 /* v5.0 (.588; VX800 board, v4.9: the front clear read wrong once or twice in 20 tries after a swap, never without one, and still wrong
  * 30 ms later; v4.9 logged only the first odd try, which was the screen's pixel): the front clear after a swap 30 times in each of
  * three ways: at once after SwapBuffers, after SwapBuffers and glFinish, after SwapBuffers and 20 ms. Each read at once and 30 ms
  * later; every wrong try is listed with what was read. (The screen's pixel through GDI reads black under this ICD: not judged.) */
 {int v,i;static const char *const vn[5]={"at once after the swap","after the swap and glFinish","after the swap and 20 ms","at once after the swap, the swap's copy not asynchronous (live switch FFFFFE22)","at once after the swap, no asynchronous copies at all (live switch FC000000)"};
  HANDLE vd=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);DWORD cv,co[4],cg;
  for(v=0;v<5;v++){int wrong=0,listed=0;GLubyte a[4],b2[4];
   /* v5.1: the last two with the driver's asynchronous copies switched off for the 30 tries, then on again */
   if(v>=3&&vd==INVALID_HANDLE_VALUE)continue;
   if(v==3){cv=0xFFFFFE22UL;DeviceIoControl(vd,0x85500012UL,&cv,4,co,16,&cg,NULL);}
   if(v==4){cv=0xFC000000UL;DeviceIoControl(vd,0x85500012UL,&cv,4,co,16,&cg,NULL);}
   fprintf(logf,"   front clear %s, 30 times:",vn[v]);
   for(i=0;i<30;i++){int bada,badb;
    glDrawBuffer(GL_BACK);glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);SwapBuffers(dc);
    if(v==1)glFinish();else if(v==2)Sleep(20);
    glDrawBuffer(GL_FRONT);glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);glDrawBuffer(GL_BACK);glFinish();
    glReadBuffer(GL_FRONT);glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,a);
    Sleep(30);glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,b2);glReadBuffer(GL_BACK);
    bada=a[2]<200||a[0]>50;badb=b2[2]<200||b2[0]>50;
    if(bada||badb){wrong++;if(listed++<6)fprintf(logf," try %d read %02X%02X%02X then %02X%02X%02X;",i,a[0],a[1],a[2],b2[0],b2[1],b2[2]);}}
   fprintf(logf," %d wrong\n",wrong);
   if(v==3){glFinish();cv=0xFFFFFE23UL;DeviceIoControl(vd,0x85500012UL,&cv,4,co,16,&cg,NULL);}
   if(v==4){glFinish();cv=0xFC000001UL;DeviceIoControl(vd,0x85500012UL,&cv,4,co,16,&cg,NULL);}}
  /* v5.3: what the VxD's 2D engine did in each try (its counters before and after: fills, copies, operations given back to the CPU,
   * DirectDraw requests, time-outs, and the last result), 60 tries at once after the swap; the distinct patterns of the right and of
   * the wrong tries, with how often each came. A wrong try that differs from the right ones names the step that went another way. */
  if(vd!=INVALID_HANDLE_VALUE){struct pat {DWORD d[6];int n;} good[8],badp[8];int ng=0,nb=0,wrong=0,k;DWORD s0[20],s1[20],g2;GLubyte a[4];
   memset(good,0,sizeof(good));memset(badp,0,sizeof(badp));
   for(i=0;i<60;i++){DWORD d[6];int isbad;struct pat *t;int *nt;
    glDrawBuffer(GL_BACK);glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glFinish();
    mark("#TRY %d (section %d)\r\n",i,which);
    memset(s0,0,sizeof(s0));DeviceIoControl(vd,0x85500016UL,NULL,0,s0,sizeof(s0),&g2,NULL);
    SwapBuffers(dc);
    glDrawBuffer(GL_FRONT);glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);glDrawBuffer(GL_BACK);glFinish();
    glReadBuffer(GL_FRONT);glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,a);glReadBuffer(GL_BACK);
    memset(s1,0,sizeof(s1));DeviceIoControl(vd,0x85500016UL,NULL,0,s1,sizeof(s1),&g2,NULL);
    /* vx2d_stats: 4 ops, 5 fallbacks, 6 fills, 7 copies, 13 ddraw, 14 timeouts, 19 last result */
    d[0]=s1[6]-s0[6];d[1]=s1[7]-s0[7];d[2]=s1[5]-s0[5];d[3]=s1[13]-s0[13];d[4]=s1[14]-s0[14];d[5]=s1[19];
    isbad=a[2]<200||a[0]>50;wrong+=isbad;t=isbad?badp:good;nt=isbad?&nb:&ng;
    mark("#END %d %s read %02X%02X%02X\r\n",i,isbad?"WRONG":"right",a[0],a[1],a[2]);
    for(k=0;k<*nt;k++)if(!memcmp(t[k].d,d,sizeof(d)))break;
    if(k==*nt&&*nt<8){memcpy(t[k].d,d,sizeof(d));(*nt)++;}
    if(k<8)t[k].n++;}
   fprintf(logf,"   front clear at once after the swap, 60 times with the 2D engine's counters: %d wrong\n",wrong);
   for(k=0;k<ng;k++)fprintf(logf,"      right, %2d times: fills %lu, copies %lu, given back to the CPU %lu, DirectDraw requests %lu, time-outs %lu, last result %08lX\n",good[k].n,good[k].d[0],good[k].d[1],good[k].d[2],good[k].d[3],good[k].d[4],good[k].d[5]);
   for(k=0;k<nb;k++)fprintf(logf,"      WRONG, %2d times: fills %lu, copies %lu, given back to the CPU %lu, DirectDraw requests %lu, time-outs %lu, last result %08lX\n",badp[k].n,badp[k].d[0],badp[k].d[1],badp[k].d[2],badp[k].d[3],badp[k].d[4],badp[k].d[5]);}
  /* v5.5: one 2D copy by the VxD (control FFFFFE45, inside its staging block) right after each of 200 swaps: how many status reads
   * until the 2D engine's busy bit shows, and whether the status passed the driver's "idle" test before that (a wait that ends
   * before the command has started) */
  if(vd!=INVALID_HANDLE_VALUE){int h[7]={0,0,0,0,0,0,0},early=0,notrun=0,listed=0;DWORD worst=0;DWORD sv[6];int sc[6],sn=0,so=0;
   fprintf(logf,"   a 2D copy right after each of 200 swaps:");
   for(i=0;i<200;i++){DWORD o4[4];
    glDrawBuffer(GL_BACK);glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);SwapBuffers(dc);
    cv=0xFFFFFE45UL;o4[0]=0xFFFFFFFEUL;o4[1]=o4[2]=o4[3]=0;DeviceIoControl(vd,0x85500012UL,&cv,4,o4,16,&cg,NULL);
    if(o4[0]==0xFFFFFFFEUL){notrun++;continue;}
    {int k;for(k=0;k<sn;k++)if(sv[k]==o4[3])break;if(k<sn)sc[k]++;else if(sn<6){sv[sn]=o4[3];sc[sn++]=1;}else so++;}
    {DWORD f=o4[0],pass=o4[2]>>16;int b=f==0?0:f<4?1:f<32?2:f<256?3:f<4096?4:f!=0xFFFFFFFFUL?5:6;h[b]++;if(f!=0xFFFFFFFFUL&&f>worst)worst=f;
     if(pass<f){early++;if(listed++<5)fprintf(logf," [swap %d: busy after %lu reads, the idle test passed at read %lu, status before %04lX, first after %08lX, busy for %lu]",i,f,pass,o4[2]&0xFFFF,o4[3],o4[1]);}}}
   fprintf(logf," reads before the engine shows busy: 0: %d, 1-3: %d, 4-31: %d, 32-255: %d, 256-4095: %d, more: %d, never: %d; most %lu; not run %d; the idle test passed before the engine started in %d\n",h[0],h[1],h[2],h[3],h[4],h[5],h[6],worst,notrun,early);
   {int k;fprintf(logf,"   the status at the first read after the command:");for(k=0;k<sn;k++)fprintf(logf," %08lX x %d;",sv[k],sc[k]);fprintf(logf," other values x %d\n",so);}
   cv=0xFFFFFE47UL;{DWORD o4[4]={0,0,0,0};DeviceIoControl(vd,0x85500012UL,&cv,4,o4,16,&cg,NULL);fprintf(logf,"   every command's wait (.593): %lu, of them the engine never showed busy in 4 reads: %lu, showed after the second read: %lu; time-outs %lu\n",o4[0],o4[1],o4[2],o4[3]);}
   cv=0xFFFFFE46UL;{DWORD o4[4]={0,0,0,0};DeviceIoControl(vd,0x85500012UL,&cv,4,o4,16,&cg,NULL);fprintf(logf,"   waits from the command's start (.592): %lu, of them the engine never showed busy: %lu; ring-paused idle rule in use: %lu; asynchronous commands completed: %lu\n",o4[0],o4[1],o4[2],o4[3]);}}
  if(vd!=INVALID_HANDLE_VALUE)CloseHandle(vd);
  glClearColor(0,0,0,1);}
 /* v3.9 (v10.315): occlusion queries, counted by the engine from v10.315: a 100x100 quad (10000 samples), then a 100x100 quad half behind
  * a nearer one (5000); the engine's edges may differ from swrast's by a line of pixels, so within 1%. Microsoft's 1.1 has no queries. */
 {typedef void (APIENTRY *gq_fn)(GLsizei,GLuint*);typedef void (APIENTRY *bq_fn)(GLenum,GLuint);typedef void (APIENTRY *eq_fn)(GLenum);typedef void (APIENTRY *rq_fn)(GLuint,GLenum,GLuint*);
  gq_fn gen=(gq_fn)wglGetProcAddress("glGenQueriesARB");bq_fn bq=(bq_fn)wglGetProcAddress("glBeginQueryARB");eq_fn eq=(eq_fn)wglGetProcAddress("glEndQueryARB");rq_fn rq=(rq_fn)wglGetProcAddress("glGetQueryObjectuivARB");
  if(gen&&bq&&eq&&rq){GLuint q=0,n1=0,n2=0;int qb=0;
   reset();glClearColor(0,0,0,1);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);gen(1,&q);
   bq(GL_SAMPLES_PASSED_ARB,q);glBegin(GL_QUADS);glVertex2f(20,20);glVertex2f(120,20);glVertex2f(120,120);glVertex2f(20,120);glEnd();eq(GL_SAMPLES_PASSED_ARB);rq(q,GL_QUERY_RESULT_ARB,&n1);
   glEnable(GL_DEPTH_TEST);glBegin(GL_QUADS);glVertex3f(20,20,0.5f);glVertex3f(70,20,0.5f);glVertex3f(70,120,0.5f);glVertex3f(20,120,0.5f);glEnd();
   bq(GL_SAMPLES_PASSED_ARB,q);glBegin(GL_QUADS);glVertex3f(20,20,-0.5f);glVertex3f(120,20,-0.5f);glVertex3f(120,120,-0.5f);glVertex3f(20,120,-0.5f);glEnd();eq(GL_SAMPLES_PASSED_ARB);rq(q,GL_QUERY_RESULT_ARB,&n2);
   glDisable(GL_DEPTH_TEST);
   if(n1<9900||n1>10100||n2<4950||n2>5050)qb=1;
   fprintf(logf,"   check: occlusion query %u samples for a 100x100 quad (want 10000), %u for it half behind another (want 5000): %s\n",n1,n2,qb?"FAIL":"pass");
   if(qb&&which!=1)failures++;}
 }
 /* v4.6 (v10.357): WGL_ARB_pixel_format and WGL_ARB_pbuffer: a format chosen for a 64x48 pbuffer with depth, a context on its DC, a clear
  * and a depth-tested triangle read back, then the window's context current again. Microsoft's 1.1 has neither. */
 if(which!=1){typedef BOOL (WINAPI *cpf_fn)(HDC,const int*,const FLOAT*,UINT,int*,UINT*);typedef HANDLE (WINAPI *cpb_fn)(HDC,int,int,int,const int*);
  typedef HDC (WINAPI *gpd_fn)(HANDLE);typedef int (WINAPI *rpd_fn)(HANDLE,HDC);typedef BOOL (WINAPI *dpb_fn)(HANDLE);typedef BOOL (WINAPI *qpb_fn)(HANDLE,int,int*);
  cpf_fn cpf=(cpf_fn)wglGetProcAddress("wglChoosePixelFormatARB");cpb_fn cpb=(cpb_fn)wglGetProcAddress("wglCreatePbufferARB");gpd_fn gpd=(gpd_fn)wglGetProcAddress("wglGetPbufferDCARB");
  rpd_fn rpd=(rpd_fn)wglGetProcAddress("wglReleasePbufferDCARB");dpb_fn dpb=(dpb_fn)wglGetProcAddress("wglDestroyPbufferARB");qpb_fn qpb=(qpb_fn)wglGetProcAddress("wglQueryPbufferARB");
  int pb_bad=0;
  if(!cpf||!cpb||!gpd||!rpd||!dpb||!qpb){fprintf(logf,"   check: pbuffer: the WGL_ARB_pbuffer / WGL_ARB_pixel_format functions are missing: FAIL\n");pb_bad=1;}
  else {static const int ia[]={0x202D,1,0x2010,1,0x2022,16,0x2014,15,0};int fmt=0;UINT nf=0;HANDLE pb=0;HDC pdc=0;HGLRC prc=0,orc=wglGetCurrentContext();HDC odc=wglGetCurrentDC();int pw=0,ph=0;GLubyte px[4]={0,0,0,0},bg[4]={0,0,0,0};
   if(!cpf(dc,ia,0,1,&fmt,&nf)||!nf){fprintf(logf,"   check: pbuffer: no pixel format for a pbuffer with depth: FAIL\n");pb_bad=1;}
   else if(!(pb=cpb(dc,fmt,64,48,0))||!(pdc=gpd(pb))){fprintf(logf,"   check: pbuffer: wglCreatePbufferARB / wglGetPbufferDCARB failed (%lu): FAIL\n",(unsigned long)GetLastError());pb_bad=1;}
   else {qpb(pb,0x2034,&pw);qpb(pb,0x2035,&ph);
    if(!(prc=wglCreateContext(pdc))||!wglMakeCurrent(pdc,prc)){fprintf(logf,"   check: pbuffer: no context on its DC (%lu): FAIL\n",(unsigned long)GetLastError());pb_bad=1;}
    else {glViewport(0,0,64,48);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,64,0,48,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
     glClearColor(1,0,0,1);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glColor3f(0,1,0);
     glBegin(GL_TRIANGLES);glVertex3f(4,4,0);glVertex3f(60,4,0);glVertex3f(32,44,0);glEnd();glDisable(GL_DEPTH_TEST);glFinish();
     glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(32,16,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);glReadPixels(1,46,1,1,GL_RGBA,GL_UNSIGNED_BYTE,bg);
     if(px[0]>16||px[1]<240||bg[0]<240||bg[1]>16||glGetError())pb_bad=1;
     fprintf(logf,"   check: pbuffer format %d, %dx%d: triangle %02X%02X%02X (want 00FF00), clear %02X%02X%02X (want FF0000): %s\n",fmt,pw,ph,px[0],px[1],px[2],bg[0],bg[1],bg[2],pb_bad?"FAIL":"pass");}
    wglMakeCurrent(odc,orc);if(prc)wglDeleteContext(prc);}
   if(pdc)rpd(pb,pdc);if(pb)dpb(pb);
   if(!wglGetCurrentContext())wglMakeCurrent(odc,orc);}
  if(pb_bad)failures++;}
 if(bad&&which!=1)failures++;
 SwapBuffers(dc);
}
/* which: 0 the ICD (engine), 1 Microsoft's generic OpenGL, 2 the ICD with ACCEL_OPENGL_HW = 0 */
static int run(int which) {
 WNDCLASSA wc;HWND w;HDC dc;HGLRC rc;int pf,k;RECT r;const char *name=which==1?"Microsoft software":which==2?"VX855 ICD, engine off (ACCEL_OPENGL_HW = 0)":"VX855 ICD";
 int generic=which==1;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="VXGL";wc.style=CS_OWNDC;RegisterClassA(&wc);
 r.left=0;r.top=0;r.right=W;r.bottom=H;AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
 w=CreateWindowA("VXGL",name,WS_OVERLAPPEDWINDOW|WS_VISIBLE,40+(which&1)*300,40,r.right-r.left,r.bottom-r.top,0,0,wc.hInstance,0);
 if(!w){fprintf(logf,"%s: no window\n",name);return 0;}
 dc=GetDC(w);fprintf(logf,"== %s\n",name);
 pf=choose_format(dc,generic);
 if(pf&&which==0){PIXELFORMATDESCRIPTOR p;DescribePixelFormat(dc,pf,sizeof(p),&p);icd_zbits=p.cDepthBits;}
 if(!pf){fprintf(logf,"   no %s pixel format\n",generic?"generic":"ICD");DestroyWindow(w);failures++;return 0;}
 {PIXELFORMATDESCRIPTOR p;DescribePixelFormat(dc,pf,sizeof(p),&p);if(!SetPixelFormat(dc,pf,&p)){fprintf(logf,"   SetPixelFormat failed %lu\n",(unsigned long)GetLastError());DestroyWindow(w);failures++;return 0;}}
 rc=wglCreateContext(dc);if(!rc){fprintf(logf,"   wglCreateContext failed %lu\n",(unsigned long)GetLastError());DestroyWindow(w);failures++;return 0;}
 if(!wglMakeCurrent(dc,rc)){fprintf(logf,"   wglMakeCurrent failed %lu\n",(unsigned long)GetLastError());wglDeleteContext(rc);DestroyWindow(w);failures++;return 0;}
 fprintf(logf,"   GL_VENDOR %s\n   GL_RENDERER %s\n   GL_VERSION %s\n",(const char*)glGetString(GL_VENDOR),(const char*)glGetString(GL_RENDERER),(const char*)glGetString(GL_VERSION));
 scene_point_param=(point_param_fn)wglGetProcAddress("glPointParameterfvARB");if(!scene_point_param)scene_point_param=(point_param_fn)wglGetProcAddress("glPointParameterfvEXT"); /* v3.1: the attenuated points (none on Microsoft's 1.1) */
 /* v3.4: the v10.304 scenes' entry points (Microsoft's 1.1 has none: those parts are left out there) */
 scene_blend_eq=(blend_eq_fn)wglGetProcAddress("glBlendEquation");if(!scene_blend_eq)scene_blend_eq=(blend_eq_fn)wglGetProcAddress("glBlendEquationEXT");
 scene_blend_col=(blend_col_fn)wglGetProcAddress("glBlendColor");if(!scene_blend_col)scene_blend_col=(blend_col_fn)wglGetProcAddress("glBlendColorEXT");
 scene_active_tex=(active_tex_fn)wglGetProcAddress("glActiveTextureARB");scene_mtc2=(mtc2_fn)wglGetProcAddress("glMultiTexCoord2fARB");scene_mtc3=(mtc3_fn)wglGetProcAddress("glMultiTexCoord3fARB");
 scene_ctex=(ctex_fn)wglGetProcAddress("glCompressedTexImage2DARB");if(!scene_ctex)scene_ctex=(ctex_fn)wglGetProcAddress("glCompressedTexImage2D"); /* v3.5 */
 scene_tex3d=(tex3d_fn)wglGetProcAddress("glTexImage3D");if(!scene_tex3d)scene_tex3d=(tex3d_fn)wglGetProcAddress("glTexImage3DEXT"); /* v4.0 (v10.317) */
 scene_ctab=(ctab_fn)wglGetProcAddress("glColorTableSGI");if(!scene_ctab)scene_ctab=(ctab_fn)wglGetProcAddress("glColorTable");
 scene_ati_gen=(ati_gen_fn)wglGetProcAddress("glGenFragmentShadersATI");scene_ati_bind=(ati_bind_fn)wglGetProcAddress("glBindFragmentShaderATI");scene_ati_begin=(ati_be_fn)wglGetProcAddress("glBeginFragmentShaderATI"); /* v4.4 */
 scene_ati_end=(ati_be_fn)wglGetProcAddress("glEndFragmentShaderATI");scene_ati_smap=(ati_smap_fn)wglGetProcAddress("glSampleMapATI");scene_ati_cop2=(ati_cop2_fn)wglGetProcAddress("glColorFragmentOp2ATI");
 scene_ati_cop3=(ati_cop3_fn)wglGetProcAddress("glColorFragmentOp3ATI");scene_ati_aop2=(ati_aop2_fn)wglGetProcAddress("glAlphaFragmentOp2ATI");scene_ati_const=(ati_const_fn)wglGetProcAddress("glSetFragmentShaderConstantATI");
 scene_glsl_create=(glsl_create_fn)wglGetProcAddress("glCreateShader");scene_glsl_prog=(glsl_prog_fn)wglGetProcAddress("glCreateProgram");scene_glsl_src=(glsl_src_fn)wglGetProcAddress("glShaderSource"); /* v4.4 */
 scene_glsl_compile=(glsl_one_fn)wglGetProcAddress("glCompileShader");scene_glsl_link=(glsl_one_fn)wglGetProcAddress("glLinkProgram");scene_glsl_use=(glsl_one_fn)wglGetProcAddress("glUseProgram");
 scene_glsl_attach=(glsl_attach_fn)wglGetProcAddress("glAttachShader");scene_glsl_loc=(glsl_loc_fn)wglGetProcAddress("glGetUniformLocation");scene_glsl_u1i=(glsl_u1i_fn)wglGetProcAddress("glUniform1i");scene_glsl_u4f=(glsl_u4f_fn)wglGetProcAddress("glUniform4f");
 scene_fbo_gen=(fbo_gen_fn)wglGetProcAddress("glGenFramebuffersEXT");scene_fbo_bind=(fbo_bind_fn)wglGetProcAddress("glBindFramebufferEXT");scene_fbo_tex2d=(fbo_tex2d_fn)wglGetProcAddress("glFramebufferTexture2DEXT"); /* v4.4 (v10.321) */
 scene_rb_gen=(fbo_gen_fn)wglGetProcAddress("glGenRenderbuffersEXT");scene_rb_bind=(fbo_bind_fn)wglGetProcAddress("glBindRenderbufferEXT");scene_rb_storage=(rb_storage_fn)wglGetProcAddress("glRenderbufferStorageEXT");
 scene_fbo_rb=(fbo_rb_fn)wglGetProcAddress("glFramebufferRenderbufferEXT");scene_fbo_status=(fbo_status_fn)wglGetProcAddress("glCheckFramebufferStatusEXT");scene_fbo_del=(fbo_del_fn)wglGetProcAddress("glDeleteFramebuffersEXT");
 scene_rb_del=(fbo_del_fn)wglGetProcAddress("glDeleteRenderbuffersEXT");scene_genmip=(genmip_fn)wglGetProcAddress("glGenerateMipmapEXT");
 scene_gen_prog=(gen_prog_fn)wglGetProcAddress("glGenProgramsARB");scene_bind_prog=(bind_prog_fn)wglGetProcAddress("glBindProgramARB");scene_prog_str=(prog_str_fn)wglGetProcAddress("glProgramStringARB");scene_fp=0; /* v3.6: sw_on */
 sw_on();sw_off(); /* v3.7: the program is made here, untimed (board, v3.6: scene 18 took 588 ms, Mesa checking its ARB grammar the first time a process parses a program) */
 {const char *e=(const char*)glGetString(GL_EXTENSIONS);scene_combine=e&&strstr(e,"GL_ARB_texture_env_combine")!=0;}
 if(which==0){const char *rn=(const char*)glGetString(GL_RENDERER);engine_on=rn&&!strstr(rn,"(software");if(!engine_on){fprintf(logf,"   the engine is not drawing: FAIL\n");failures++;}}
 if(which==2){const char *rn=(const char*)glGetString(GL_RENDERER);if(!rn||!strstr(rn,"(software")){fprintf(logf,"   the engine-off run is not in software (VXGL_HW = 0 not taken): FAIL\n");failures++;}} /* v10.319 (audit T2): the reference is checked */
 {const char *e=(const char*)glGetString(GL_EXTENSIONS);fprintf(logf,"   GL_EXTENSIONS: %u characters\n",e?(unsigned)strlen(e):0);}
 if(!generic)fprintf(logf,"   VXGL.DLL loaded: %s\n",GetModuleHandleA("VXGL.DLL")?"yes":"NO (opengl32 is not using the ICD)");
 screen_check("after the context was made current",-1);
 for(k=0;k<NS;k++) {
  DWORD t0;int f,n=k==NS-1?frames:1;MSG m;
  while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}
  if(!strcmp(scenes[k].name,"three texture units")){DWORD t1=GetTickCount();int f2; /* v3.6 */
   for(f2=0;f2<frames;f2++){scenes[NS-1].f();SwapBuffers(dc);}glFinish();sp_pre[which]=(double)(GetTickCount()-t1)/frames;}
  t0=GetTickCount();
  for(f=0;f<n;f++){scenes[k].f();if(f<n-1){SwapBuffers(dc);}}
  glFinish();ms[which][k]=(double)(GetTickCount()-t0)/n;
  glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,img[which][k]);
  have[which][k]=glGetError()==GL_NO_ERROR;
  SwapBuffers(dc);
  screen_check(which==0?"after the engine's scene":which==1?"after Microsoft's scene":"after the engine-off scene",k);
 }
 fprintf(logf,"   speed scene before \"three texture units\": %.1f ms a frame; at the end %.1f\n",sp_pre[which],ms[which][NS-1]); /* v3.6 */
 gl_checks(which,dc);
 screen_check("after the checks",-1);
 { /* v1.6: what the CPU sees in the stencil right after the software writes (slots: A 0, C 2, D 3, E 4) */
  static const int slot[4]={0,2,3,4};static const char *nm[4]={"A","C","D","E"};int q;
  fprintf(logf,"   stencil read back after the software write (pixels with 0 / 1 / 3 / other):");
  for(q=0;q<4;q++)fprintf(logf," %s %ld/%ld/%ld/%ld;",nm[q],st_count[slot[q]][0],st_count[slot[q]][1],st_count[slot[q]][2],st_count[slot[q]][3]);
  fprintf(logf,"\n   depth changed by the stencil scenes (pixels no longer 1.0): A %ld, B %ld, C %ld, D %ld, E %ld\n",st_zchg[0],st_zchg[1],st_zchg[2],st_zchg[3],st_zchg[4]);
 }
 /* v4.x (.533): the probes (the VxD's TEXDBG switch words A1-A7, AA/AB, B2/B3, 48-4C) removed: those words were removed in .530 */
 screen_check("after the probes",-1);
 if(which==0||which==2)depth_measure(which?1:0,dc);
 screen_check("after the depth measurement",-1);
 wglMakeCurrent(0,0);wglDeleteContext(rc);ReleaseDC(w,dc);DestroyWindow(w);
 return 1;
}
static void bmp(const char *name,const unsigned char *p) {
 FILE *f=fopen(name,"wb");int y,x;unsigned char h[54];
 if(!f)return;memset(h,0,54);h[0]='B';h[1]='M';*(DWORD*)(h+2)=54+W*H*3;*(DWORD*)(h+10)=54;*(DWORD*)(h+14)=40;*(LONG*)(h+18)=W;*(LONG*)(h+22)=H;*(WORD*)(h+26)=1;*(WORD*)(h+28)=24;
 fwrite(h,1,54,f);
 for(y=0;y<H;y++)for(x=0;x<W;x++){const unsigned char *q=p+(y*W+x)*4;fputc(q[2],f);fputc(q[1],f);fputc(q[0],f);}
 fclose(f);
}
/* ACCEL_OPENGL_HW for the third run: set to 0, put back afterwards (deleted when it was not there) */
/* v10.310: the engine-off run is asked for by VXGL_HW = 0 in this process's environment (VXGL.DLL reads it at CreateContext), not by
 * ACCEL_OPENGL_HW in the registry: a run stopped during its engine-off part (a hang, a reboot) left the key at 0, and each later run saved
 * that 0 and put it back (board, v10.309: every OpenGL program drew in software) */
static int hw_key(int set,DWORD *saved,int *existed) {(void)saved;(void)existed;if(set)return SetEnvironmentVariableA("VXGL_HW","0")!=0;SetEnvironmentVariableA("VXGL_HW",0);return 1;}
/* v10.310: a 0 left in the registry by an earlier version is reported (the engine is off for every OpenGL program until it is deleted) */
static void hw_key_note(FILE *l) {HKEY k;DWORD v=1,t=0,n=4;
 if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Software\\VX85598",0,KEY_READ,&k)!=ERROR_SUCCESS)return;
 if(RegQueryValueExA(k,"ACCEL_OPENGL_HW",0,&t,(BYTE*)&v,&n)==ERROR_SUCCESS&&t==REG_DWORD&&!v)fprintf(l,"ACCEL_OPENGL_HW = 0 in HKLM\\Software\\VX85598: the engine is off for every OpenGL program until it is deleted\n");
 RegCloseKey(k);}
static void compare(int a,int b,const char *title,double fail_pct,const char *bmp_a,const char *bmp_b) {
 int k;
 fprintf(logf,"== %s (a pixel differs when a channel is off by more than 16)\n",title);
 for(k=0;k<NS;k++) {
  int i,d=0,fx=-1,fy=-1,bad;
  if(!have[a][k]||!have[b][k]){fprintf(logf,"%2d %-38s not drawn by both\n",k,scenes[k].name);if(fail_pct>0)failures++;continue;}
  for(i=0;i<W*H;i++){int c,o=0;for(c=0;c<3;c++){int x=img[a][k][i*4+c],y=img[b][k][i*4+c];if(x-y>16||y-x>16)o=1;}if(o){if(!d){fx=i%W;fy=i/W;}d++;}}
  fprintf(logf,"%2d %-38s %7.1f ms against %7.1f ms; %5d pixels differ (%.2f%%)",k,scenes[k].name,ms[a][k],ms[b][k],d,100.0*d/(W*H));
  if(d){const unsigned char *p=img[a][k]+(fy*W+fx)*4,*q=img[b][k]+(fy*W+fx)*4;fprintf(logf,"; first at %d,%d: %02X%02X%02X against %02X%02X%02X",fx,fy,p[0],p[1],p[2],q[0],q[1],q[2]);}
  bad=100.0*d/(W*H)>(fail_pct>0?fail_pct:5.0);
  fprintf(logf,"%s\n",bad?(fail_pct>0&&!scenes[k].info?"  FAIL":fail_pct>0?"  (shown, not judged)":"  (over 5%)"):"");
  if(bad){char nm[32];if(fail_pct>0&&!scenes[k].info)failures++;
   if(bmp_a){sprintf(nm,"VXGL_%02d_%s.BMP",k,bmp_a);bmp(nm,img[a][k]);}
   if(bmp_b){sprintf(nm,"VXGL_%02d_%s.BMP",k,bmp_b);bmp(nm,img[b][k]);}}
 }
}
int main(int argc,char **argv) {
 char v[64];DWORD n=sizeof(v),t;HKEY key;
 SetEnvironmentVariableA("VXGL_LOG","1");DeleteFileA("C:\\VXGL.LOG");SetEnvironmentVariableA("VXGL_FTRACE","1"); /* v5.2: the ICD's log for this run, its front-buffer lines copied to the end of VXGL.TXT */
 SetEnvironmentVariableA("VXGL_UNITS3","1"); /* v3.7: the ICD draws three units on the engine in this run (ACCEL_OPENGL_UNITS3 defaults to 0 from v10.308) */
 if(argc>1)frames=atoi(argv[1]);if(frames<1)frames=1;
 logf=fopen("VXGL.TXT","w");if(!logf)return 1;
 setvbuf(logf,0,_IONBF,0); /* v2.5: every line on disk at once, so a crash leaves the log up to it */
 fprintf(logf,"VXGL v5.7 (driver .597): the OpenGL ICD with the engine, without it, and Microsoft's software OpenGL, %dx%d, %d speed frames\n",W,H,frames);
 v[0]=0;if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\Windows\\CurrentVersion\\OpenGLdrivers",0,KEY_READ,&key)==ERROR_SUCCESS){if(RegQueryValueExA(key,"Chrome9GL",0,&t,(BYTE*)v,&n)!=ERROR_SUCCESS&&(n=sizeof(v),RegQueryValueExA(key,"VX855GL",0,&t,(BYTE*)v,&n)!=ERROR_SUCCESS))strcpy(v,"(missing)");RegCloseKey(key);}else strcpy(v,"(no OpenGLdrivers key)");
 fprintf(logf,"OpenGLdrivers Chrome9GL (VX855GL before .580) = %s\n",v);
 {HDC d=GetDC(0);fprintf(logf,"desktop %dx%dx%d\n",GetDeviceCaps(d,HORZRES),GetDeviceCaps(d,VERTRES),GetDeviceCaps(d,BITSPIXEL));ReleaseDC(0,d);}
 hw_key_note(logf);
 screen_init();
 run(0);run(1);
 {DWORD saved=0;int existed=0;
  if(hw_key(1,&saved,&existed)){run(2);hw_key(0,&saved,&existed);}else{fprintf(logf,"ACCEL_OPENGL_HW could not be set: no engine-off run\n");failures++;}}
 compare(0,2,"the engine against the ICD without it (the same OpenGL rules): FAIL over 1%",1.0,"HW","SW");
 compare(0,1,"the ICD against Microsoft's software OpenGL (a reference: quad diagonals, dithering and edges differ)",0,0,"MS");
 {int k3=-1,k,q;for(k=0;k<NS;k++)if(!strcmp(scenes[k].name,"three texture units"))k3=k; /* v3.7: scene "three texture units" quad by quad */
  if(k3>=0&&have[0][k3]&&have[2][k3]){fprintf(logf,"== scene %d (three texture units, VXGL_UNITS3=1 for this run) quad by quad, the engine against the ICD without it\n",k3);
   for(q=0;q<7;q++){int x,y,d=0,cx=units3_quads[q].x+units3_quads[q].s/2,cy=units3_quads[q].y+units3_quads[q].s/2;const unsigned char *e=img[0][k3]+(cy*W+cx)*4,*o=img[2][k3]+(cy*W+cx)*4;
    for(y=units3_quads[q].y;y<units3_quads[q].y+units3_quads[q].s;y++)for(x=units3_quads[q].x;x<units3_quads[q].x+units3_quads[q].s;x++){int c,bad=0;for(c=0;c<3;c++){int a=img[0][k3][(y*W+x)*4+c],b=img[2][k3][(y*W+x)*4+c];if(a-b>16||b-a>16)bad=1;}d+=bad;}
    fprintf(logf,"   %-45s %5d of %d pixels differ; centre %02X%02X%02X against %02X%02X%02X\n",units3_quads[q].what,d,units3_quads[q].s*units3_quads[q].s,e[0],e[1],e[2],o[0],o[1],o[2]);}}}
 depth_report(icd_zbits);
 {FILE *gl=fopen("C:\\VXGL.LOG","r");char ln[600];int nl=0,tot=0;
  fprintf(logf,"== the ICD's log (C:\\VXGL.LOG): its lines about the front buffer\n");
  if(!gl)fprintf(logf,"   (no log file)\n");
  else{static char keep[48][400];int nk=0,in=0,pw=0,pr=0,sec=-1;
   while(fgets(ln,sizeof(ln),gl)){tot++;
    if(strstr(ln,"front buffer")||strstr(ln,"not copied")){if(nl++<60)fprintf(logf,"   %s",ln);}
    /* v5.4: the ICD's front-buffer steps of the first two wrong tries and the first right one of each section */
    if(!strncmp(ln,"#TRY ",5)){int s2=-1;char *q=strstr(ln,"(section ");if(q)s2=atoi(q+9);if(s2!=sec){sec=s2;pw=pr=0;}nk=0;in=1;if(nk<48)strncpy(keep[nk++],ln,399);}
    else if(!strncmp(ln,"#END ",5)){int w=strstr(ln,"WRONG")!=0;if(in&&((w&&pw<2)||(!w&&pr<1))){int k;if(w)pw++;else pr++;for(k=0;k<nk;k++)fprintf(logf,"   %s",keep[k]);fprintf(logf,"   %s",ln);}in=0;}
    else if(in&&strstr(ln,"ftrace:")&&nk<48){strncpy(keep[nk],ln,399);keep[nk][399]=0;nk++;}}
   fclose(gl);fprintf(logf,"   %d front-buffer line(s) of %d\n",nl,tot);}}
 fprintf(logf,"failures=%d\n",failures);fclose(logf);
 return failures;
}
