#define COBJMACROS
#include <windows.h>
#include <ddraw.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "../src/vx_alpha.h"
#define DEPTH_BOUNDS 0x8890
#ifndef GL_TEXTURE_FILTER_CONTROL_EXT
#define GL_TEXTURE_FILTER_CONTROL_EXT 0x8500
#define GL_TEXTURE_LOD_BIAS_EXT 0x8501
#endif
#define MAX_ANISO 0x84FF
#define TEX_ANISO 0x84FE
#define SAMPLES_PASSED 0x8914
#define QUERY_RESULT 0x8866
#define FRAMEBUFFER 0x8D40
#define RENDERBUFFER 0x8D41
#define COLOR_ATTACHMENT0 0x8CE0
#define DEPTH_ATTACHMENT 0x8D00
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16 0x81A5
#endif
#define FRAMEBUFFER_COMPLETE 0x8CD5
static HWND mainw,logw,canvas;static HDC dc;static HGLRC rc;static FILE *out;static int pass,fail,skip;
static void say(const char *fmt,...){char b[1024];va_list a;va_start(a,fmt);vsprintf(b,fmt,a);va_end(a);
 if(out){fputs(b,out);fputs("\n",out);fflush(out);}strcat(b,"\r\n");SendMessageA(logw,EM_SETSEL,0x7FFFFFFF,0x7FFFFFFF);SendMessageA(logw,EM_REPLACESEL,0,(LPARAM)b);UpdateWindow(logw);}
static void check(const char *name,int ok){if(ok)pass++;else fail++;say("%s: %s",ok?"PASS":"FAIL",name);}
static void skipped(const char *name){skip++;say("SKIP: %s",name);}
static PROC proc(const char *n){PROC p=wglGetProcAddress(n);return (ULONG_PTR)p<=3||(LONG_PTR)p==-1?0:p;}
static void drain(void){while(glGetError()!=GL_NO_ERROR);}
static void reset(void){glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_STENCIL_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);
 if(strstr((const char*)glGetString(GL_EXTENSIONS),"GL_EXT_depth_bounds_test"))glDisable(DEPTH_BOUNDS);
 glColorMask(1,1,1,1);glDepthMask(1);glDepthFunc(GL_LESS);glEnable(GL_DEPTH_TEST);glViewport(0,0,256,128);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,256,0,128,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDrawBuffer(GL_BACK);glReadBuffer(GL_BACK);glClearColor(0,0,1,1);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);drain();}
static void quad(float l,float b,float r,float t,float z){glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex3f(l,b,z);glTexCoord2f(1,0);glVertex3f(r,b,z);glTexCoord2f(1,1);glVertex3f(r,t,z);glTexCoord2f(0,1);glVertex3f(l,t,z);glEnd();}
static int pixel(int x,int y,int r,int g,int b){GLubyte p[3];glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(x,y,1,1,GL_RGB,GL_UNSIGNED_BYTE,p);say("  pixel %d,%d = %u,%u,%u",x,y,p[0],p[1],p[2]);return abs((int)p[0]-r)<=8&&abs((int)p[1]-g)<=8&&abs((int)p[2]-b)<=8;}
typedef void (APIENTRY *Bounds)(GLclampd,GLclampd);
typedef BOOL (APIENTRY *SwapInt)(int);
typedef int (APIENTRY *GetSwapInt)(void);
typedef const char *(APIENTRY *WGLExt)(void);
typedef void (APIENTRY *GenObj)(GLsizei,GLuint*);
typedef void (APIENTRY *BindObj)(GLenum,GLuint);
typedef void (APIENTRY *DelObj)(GLsizei,const GLuint*);
typedef void (APIENTRY *RBStore)(GLenum,GLenum,GLsizei,GLsizei);
typedef void (APIENTRY *FBTex)(GLenum,GLenum,GLenum,GLuint,GLint);
typedef void (APIENTRY *FBRB)(GLenum,GLenum,GLenum,GLuint);
typedef GLenum (APIENTRY *FBStatus)(GLenum);
typedef void (APIENTRY *BeginQ)(GLenum,GLuint);
typedef void (APIENTRY *EndQ)(GLenum);
typedef void (APIENTRY *GetQ)(GLuint,GLenum,GLuint*);
#include "vx_bounds_checks.h"
static void bounds_test(void){Bounds bounds=(Bounds)proc("glDepthBoundsEXT");
 int advertised=strstr((const char*)glGetString(GL_EXTENSIONS),"GL_EXT_depth_bounds_test")!=0;
 say("  depth bounds advertised: %s; function resolved: %s",advertised?"YES":"NO",bounds?"YES":"NO");
 if(!advertised){skipped("depth bounds extension not advertised");return;}
 check("advertised depth bounds function resolves",bounds!=0);if(bounds)bounds_checks(bounds);}
static void anisotropy_test(void){GLuint t;GLfloat max=0,a=0;GLubyte image[16]={255,255,255,255,0,0,0,255,255,0,0,255,0,255,0,255};
 if(!strstr((const char*)glGetString(GL_EXTENSIONS),"GL_EXT_texture_filter_anisotropic")){skipped("OpenGL anisotropy not advertised");return;}
 reset();glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,image);glGetFloatv(MAX_ANISO,&max);glTexParameterf(GL_TEXTURE_2D,TEX_ANISO,8);glGetTexParameterfv(GL_TEXTURE_2D,TEX_ANISO,&a);say("  maximum anisotropy %.1f; requested/read back %.1f",max,a);check("anisotropy advertised and accepted",max>=16&&a==8&&glGetError()==GL_NO_ERROR);glDeleteTextures(1,&t);}
static void query_test(void){GenObj gen=(GenObj)proc("glGenQueriesARB");BeginQ begin=(BeginQ)proc("glBeginQueryARB");EndQ end=(EndQ)proc("glEndQueryARB");GetQ get=(GetQ)proc("glGetQueryObjectuivARB");DelObj del=(DelObj)proc("glDeleteQueriesARB");GLuint q,n=999;
 if(!gen||!begin||!end||!get||!del){skipped("occlusion queries unavailable");return;}reset();gen(1,&q);begin(SAMPLES_PASSED,q);glClearDepth(.75);glClearStencil(0x31);glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);end(SAMPLES_PASSED);get(q,QUERY_RESULT,&n);say("  samples from clear alone: %lu",(unsigned long)n);check("internal clear excluded from query",n==0);
 begin(SAMPLES_PASSED,q);glColor3f(1,0,0);quad(16,16,112,112,0);end(SAMPLES_PASSED);get(q,QUERY_RESULT,&n);say("  samples from visible rectangle: %lu",(unsigned long)n);check("visible geometry counted after clear",n>0&&n!=0xFFFFFFFFUL);del(1,&q);check("query tests GL error",glGetError()==GL_NO_ERROR);}
static void fbo_test(void){GenObj genfb=(GenObj)proc("glGenFramebuffersEXT"),genrb=(GenObj)proc("glGenRenderbuffersEXT");BindObj bindfb=(BindObj)proc("glBindFramebufferEXT"),bindrb=(BindObj)proc("glBindRenderbufferEXT");RBStore storage=(RBStore)proc("glRenderbufferStorageEXT");FBTex attachtex=(FBTex)proc("glFramebufferTexture2DEXT");FBRB attachrb=(FBRB)proc("glFramebufferRenderbufferEXT");FBStatus status=(FBStatus)proc("glCheckFramebufferStatusEXT");DelObj delfb=(DelObj)proc("glDeleteFramebuffersEXT"),delrb=(DelObj)proc("glDeleteRenderbuffersEXT");GLuint f=0,d=0,t=0;int i;
 if(!genfb||!genrb||!bindfb||!bindrb||!storage||!attachtex||!attachrb||!status||!delfb||!delrb){skipped("FBO functions unavailable");return;}
 reset();glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,64,64,0,GL_RGB,GL_UNSIGNED_BYTE,0);
 genfb(1,&f);bindfb(FRAMEBUFFER,f);attachtex(FRAMEBUFFER,COLOR_ATTACHMENT0,GL_TEXTURE_2D,t,0);genrb(1,&d);bindrb(RENDERBUFFER,d);storage(RENDERBUFFER,GL_DEPTH_COMPONENT16,64,64);attachrb(FRAMEBUFFER,DEPTH_ATTACHMENT,RENDERBUFFER,d);
 if(status(FRAMEBUFFER)!=FRAMEBUFFER_COMPLETE){check("RGB+D16 FBO complete",0);goto done;}check("RGB+D16 FBO complete",1);glViewport(0,0,64,64);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,64,0,64,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 /* No readback between these frames: it would hide the original depth visibility failure. */
 for(i=0;i<32;i++){glClearColor(0,0,1,1);glClearDepth((i&1)?.75:.25);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glColor3f(1,0,0);quad(0,0,64,64,0);}
 check("FBO repeated asynchronous depth clears",pixel(32,32,255,0,0));check("FBO tests GL error",glGetError()==GL_NO_ERROR);
done:bindfb(FRAMEBUFFER,0);bindrb(RENDERBUFFER,0);if(d)delrb(1,&d);if(f)delfb(1,&f);if(t)glDeleteTextures(1,&t);reset();}
static void long_mip_test(void){GLuint t;GLubyte *p;int n,l,i;reset();glDisable(GL_DEPTH_TEST);glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
 n=512;for(l=0;l<10;l++){p=(GLubyte*)malloc((size_t)n*n*3);if(!p){skipped("512 mip test allocation");glDeleteTextures(1,&t);return;}for(i=0;i<n*n;i++){p[i*3]=l==9?255:0;p[i*3+1]=l==9?0:255;p[i*3+2]=0;}glTexImage2D(GL_TEXTURE_2D,l,GL_RGB,n,n,0,GL_RGB,GL_UNSIGNED_BYTE,p);free(p);n>>=1;}
 glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glEnable(GL_TEXTURE_2D);/* Four pixels across, with explicit LOD bias: avoid a subpixel diagnostic. */glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT,GL_TEXTURE_LOD_BIAS_EXT,4);quad(64,64,68,68,0);say("  Known hardware limit: level 9 requires EXACT_GL.REG/software fallback.");check("512 texture reaches final 1x1 mip",pixel(65,65,255,0,0));check("long mip upload/draw GL error",glGetError()==GL_NO_ERROR);glTexEnvf(GL_TEXTURE_FILTER_CONTROL_EXT,GL_TEXTURE_LOD_BIAS_EXT,0);glPixelStorei(GL_UNPACK_ALIGNMENT,4);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&t);}
static void swap_test(void){SwapInt set=(SwapInt)proc("wglSwapIntervalEXT");GetSwapInt get=(GetSwapInt)proc("wglGetSwapIntervalEXT");WGLExt ext=(WGLExt)proc("wglGetExtensionsStringEXT");int old,i;DWORD t0,t1,t2;
 if(!set||!get||!ext){skipped("WGL swap control unavailable");return;}say("  WGL: %s",ext());old=get();check("swap interval zero",set(0)&&get()==0);check("negative swap interval refused",!set(-1)&&GetLastError()==ERROR_INVALID_DATA);set(0);t0=GetTickCount();for(i=0;i<8;i++)SwapBuffers(dc);t1=GetTickCount();set(1);for(i=0;i<8;i++)SwapBuffers(dc);t2=GetTickCount();say("  8 swaps: interval 0 %lu ms, interval 1 %lu ms",(unsigned long)(t1-t0),(unsigned long)(t2-t1));check("swap interval one",get()==1);set(old);}
static uint32_t pattern_pixel(unsigned x,unsigned y,uint32_t bpp){uint32_t r=(x*37+y*11)&255,g=(x*13+y*29)&255,b=(x*7+y*43)&255;
 return bpp==16?((r>>3)<<11)|((g>>2)<<5)|(b>>3):(r<<16)|(g<<8)|b;}
static void stretch_test(LPDIRECTDRAWSURFACE7 src,LPDIRECTDRAWSURFACE7 dst,uint32_t bpp){DDSURFACEDESC2 sd;DDBLTFX fx;RECT sr={7,9,39,41},dr={0,0,63,31};HRESULT h;uint32_t x,y,sx,sy,m,bad,got,expect,mask=bpp==16?0xFFFF:0xFFFFFF;BYTE *row;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);h=IDirectDrawSurface7_Lock(src,0,&sd,DDLOCK_WAIT,0);if(h!=DD_OK){check("pattern source lock",0);return;}
 for(y=0;y<64;y++){row=(BYTE*)sd.lpSurface+y*sd.lPitch;for(x=0;x<64;x++){got=pattern_pixel(x,y,bpp);if(bpp==16)((WORD*)row)[x]=(WORD)got;else((DWORD*)row)[x]=got;}}
 IDirectDrawSurface7_Unlock(src,0);
 for(m=0;m<4;m++){memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwDDFX=(m&1?DDBLTFX_MIRRORLEFTRIGHT:0)|(m&2?DDBLTFX_MIRRORUPDOWN:0);h=IDirectDrawSurface7_Blt(dst,&dr,src,&sr,DDBLT_WAIT|DDBLT_DDFX,&fx);
  if(h!=DD_OK){say("  stretch/mirror %lu HRESULT %08lX",(unsigned long)m,(unsigned long)h);check("stretch/mirror request",0);continue;}
  memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);h=IDirectDrawSurface7_Lock(dst,0,&sd,DDLOCK_WAIT|DDLOCK_READONLY,0);if(h!=DD_OK){check("stretch/mirror result lock",0);continue;}bad=0;
  for(y=0;y<31;y++){row=(BYTE*)sd.lpSurface+y*sd.lPitch;for(x=0;x<63;x++){sx=((2*x+1)*32)/126;sy=((2*y+1)*32)/62;if(m&1)sx=31-sx;if(m&2)sy=31-sy;expect=pattern_pixel(sx+7,sy+9,bpp);got=bpp==16?((WORD*)row)[x]:((DWORD*)row)[x];if((got&mask)!=(expect&mask))bad++;}}
  IDirectDrawSurface7_Unlock(dst,0);say("  stretch/mirror %lu: %lu wrong pixels / 1953",(unsigned long)m,(unsigned long)bad);check("stretch/mirror pixels",bad==0);}}
static void feature_switches(void){HANDLE h=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);DWORD q=0xFFFFFE50UL,o[4],got=0;
 if(h==INVALID_HANDLE_VALUE)return;memset(o,0,sizeof(o));if(DeviceIoControl(h,0x85500012UL,&q,4,o,16,&got,0)&&got==16)say("Native stretch/mirror switch: %s (mask %08lX)",(o[3]&4194304UL)?"ON":"OFF",(unsigned long)o[3]);CloseHandle(h);}
static void alpha_test(void){HMODULE m=LoadLibraryA("DDRAW.DLL");HRESULT(WINAPI *create)(GUID*,LPVOID*,REFIID,IUnknown*);static const GUID iid={0x15E65EC0,0x3B9C,0x11D2,{0xB9,0x2F,0,0x60,0x97,0x97,0xEA,0x5B}};LPDIRECTDRAW7 dd=0;LPDIRECTDRAWSURFACE7 s=0,d=0;DDSURFACEDESC2 sd,mode;DDCAPS caps;DDBLTFX fx;int alpha_cap=0;HRESULT hr;uint32_t bpp,red,blue,a,got,expected,mask;int i;static const uint32_t alphas[3]={0,128,255};
 if(!m){skipped("DDRAW.DLL unavailable");return;}create=(HRESULT(WINAPI*)(GUID*,LPVOID*,REFIID,IUnknown*))GetProcAddress(m,"DirectDrawCreateEx");if(!create||create(0,(LPVOID*)&dd,&iid,0)!=DD_OK){skipped("DirectDraw7 unavailable");goto done;}
 hr=IDirectDraw7_SetCooperativeLevel(dd,mainw,DDSCL_NORMAL);if(hr!=DD_OK){say("  SetCooperativeLevel HRESULT %08lX",(unsigned long)hr);check("DirectDraw cooperative level",0);goto done;}
 memset(&caps,0,sizeof(caps));caps.dwSize=sizeof(caps);hr=IDirectDraw7_GetCaps(dd,&caps,0);
 if(hr!=DD_OK){say("  GetCaps HRESULT %08lX",(unsigned long)hr);check("DirectDraw capabilities",0);goto done;}
 alpha_cap=(caps.dwCaps&DDCAPS_ALPHA)!=0&&caps.dwAlphaBltConstBitDepths!=0;
 say("  DD caps %08lX; constant-alpha depths %08lX",(unsigned long)caps.dwCaps,(unsigned long)caps.dwAlphaBltConstBitDepths);
 memset(&mode,0,sizeof(mode));mode.dwSize=sizeof(mode);if(IDirectDraw7_GetDisplayMode(dd,&mode)!=DD_OK)goto done;bpp=mode.ddpfPixelFormat.dwRGBBitCount;
 if(bpp!=16&&bpp!=32){skipped("alpha requires 16/32-bit desktop");goto done;}red=bpp==16?0xF800:0xFF0000;blue=bpp==16?31:255;mask=bpp==16?0xFFFF:0xFFFFFF;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;sd.dwWidth=sd.dwHeight=64;sd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;
 if(IDirectDraw7_CreateSurface(dd,&sd,&s,0)!=DD_OK||IDirectDraw7_CreateSurface(dd,&sd,&d,0)!=DD_OK){skipped("alpha video surfaces unavailable");goto done;}
 if(!alpha_cap){say("  Public DirectDraw Blt does not implement DDBLT_ALPHA flags; no support claimed.");skipped("DirectDraw constant alpha unavailable through public Blt API");goto stretch;}
 memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwFillColor=red;hr=IDirectDrawSurface7_Blt(s,0,0,0,DDBLT_WAIT|DDBLT_COLORFILL,&fx);if(hr!=DD_OK){say("  source fill HRESULT %08lX",(unsigned long)hr);check("alpha source fill",0);goto stretch;}
 for(i=0;i<3;i++){memset(&fx,0,sizeof(fx));fx.dwSize=sizeof(fx);fx.dwFillColor=blue;hr=IDirectDrawSurface7_Blt(d,0,0,0,DDBLT_WAIT|DDBLT_COLORFILL,&fx);if(hr!=DD_OK){say("  destination fill HRESULT %08lX",(unsigned long)hr);check("alpha destination fill",0);continue;}a=alphas[i];fx.dwAlphaSrcConstBitDepth=8;fx.dwAlphaSrcConst=a;hr=IDirectDrawSurface7_Blt(d,0,s,0,DDBLT_WAIT|DDBLT_ALPHASRCCONSTOVERRIDE,&fx);say("  alpha %lu: HRESULT %08lX",(unsigned long)a,(unsigned long)hr);
  if(hr!=DD_OK){check("constant source-alpha blit",0);continue;}memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);hr=IDirectDrawSurface7_Lock(d,0,&sd,DDLOCK_WAIT|DDLOCK_READONLY,0);if(hr!=DD_OK){check("alpha result lock",0);continue;}got=bpp==16?*(WORD*)sd.lpSurface:*(DWORD*)sd.lpSurface;IDirectDrawSurface7_Unlock(d,0);expected=vx_alpha_pixel(red,blue,a,bpp);say("  pixel %08lX, expected %08lX",(unsigned long)got,(unsigned long)expected);check("constant source-alpha pixel",(got&mask)==(expected&mask));}
stretch:if(s&&d)stretch_test(s,d,bpp);
done:if(d)IDirectDrawSurface7_Release(d);if(s)IDirectDrawSurface7_Release(s);if(dd)IDirectDraw7_Release(dd);FreeLibrary(m);}
static void run(void){SwapInt set;if(!rc){say("No OpenGL context. Use a 16/32-bit desktop and install the full package.");return;}out=fopen("VXFEAT.TXT","a");pass=fail=skip=0;say("VXFEAT 1.2 / v10.356 audit-test; results append to VXFEAT.TXT");say("Vendor: %s",glGetString(GL_VENDOR));say("Renderer: %s",glGetString(GL_RENDERER));say("OpenGL: %s",glGetString(GL_VERSION));set=(SwapInt)proc("wglSwapIntervalEXT");if(set)set(0);
 feature_switches();say("-- Depth bounds --");bounds_test();say("-- OpenGL anisotropy --");anisotropy_test();say("-- Query / clear exclusion --");query_test();say("-- FBO clear visibility --");fbo_test();say("-- Long mip chain (known native limit) --");long_mip_test();say("-- Swap interval --");swap_test();say("-- DirectDraw alpha --");alpha_test();reset();glDisable(GL_DEPTH_TEST);glColor3f(0,1,0);quad(16,16,112,112,0);SwapBuffers(dc);say("Finished: %d passed, %d failed, %d skipped",pass,fail,skip);if(out){fclose(out);out=0;}}
static LRESULT CALLBACK wndproc(HWND w,UINT msg,WPARAM wp,LPARAM lp){switch(msg){case WM_COMMAND:if(LOWORD(wp)==1){EnableWindow(GetDlgItem(w,1),FALSE);run();EnableWindow(GetDlgItem(w,1),TRUE);}return 0;case WM_CLOSE:DestroyWindow(w);return 0;case WM_DESTROY:if(rc){wglMakeCurrent(0,0);wglDeleteContext(rc);}if(dc)ReleaseDC(canvas,dc);PostQuitMessage(0);return 0;}return DefWindowProcA(w,msg,wp,lp);}
int WINAPI WinMain(HINSTANCE h,HINSTANCE prev,LPSTR cmd,int show){WNDCLASSA wc;MSG msg;PIXELFORMATDESCRIPTOR pfd;int pf;(void)prev;(void)cmd;memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wndproc;wc.hInstance=h;wc.hCursor=LoadCursor(0,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1);wc.lpszClassName="VXFEATMain";RegisterClassA(&wc);
 mainw=CreateWindowA(wc.lpszClassName,"VX855 v10.356 feature checks",WS_OVERLAPPEDWINDOW,60,40,780,620,0,0,h,0);wc.style=CS_OWNDC;wc.lpfnWndProc=DefWindowProcA;wc.lpszClassName="VXFEATGL";RegisterClassA(&wc);canvas=CreateWindowA(wc.lpszClassName,"",WS_CHILD|WS_VISIBLE,12,12,384,256,mainw,0,h,0);
 CreateWindowA("BUTTON","Run feature checks",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,420,20,220,34,mainw,(HMENU)1,h,0);CreateWindowA("STATIC","Results append to VXFEAT.TXT.\r\n\r\nThe long-mip check may fail in the normal profile; repeat with EXACT_GL.REG.",WS_CHILD|WS_VISIBLE,420,70,300,120,mainw,0,h,0);
 logw=CreateWindowA("EDIT","",WS_CHILD|WS_VISIBLE|WS_BORDER|WS_VSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL,12,280,740,285,mainw,0,h,0);SendMessageA(logw,EM_LIMITTEXT,500000,0);dc=GetDC(canvas);memset(&pfd,0,sizeof(pfd));pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=GetDeviceCaps(dc,BITSPIXEL);pfd.cDepthBits=24;pfd.cStencilBits=8;pf=ChoosePixelFormat(dc,&pfd);
 if(pf&&SetPixelFormat(dc,pf,&pfd)){rc=wglCreateContext(dc);if(rc&&!wglMakeCurrent(dc,rc)){wglDeleteContext(rc);rc=0;}}
 ShowWindow(mainw,show);UpdateWindow(mainw);say("Ready. Press Run feature checks. Keep the test window visible.");while(GetMessageA(&msg,0,0,0)>0){TranslateMessage(&msg);DispatchMessageA(&msg);}return 0;}
