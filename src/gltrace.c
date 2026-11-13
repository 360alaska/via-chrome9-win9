/* GLTRACE.DLL v1.0 (driver v10.342): records a program's OpenGL calls with every argument and every block they point to (vertices,
 * matrices, texture images), and passes each call on to the real OPENGL32.DLL. For Quake 2: copy GLTRACE.DLL next to quake2.exe and
 * start it with +set gl_driver gltrace (Quake then loads this DLL as its OpenGL and calls its wgl functions itself).
 * What is recorded: everything except the drawing itself (glBegin to glEnd) and glTexSubImage while Scroll Lock is off; everything while
 * it is on (a beep when it changes, taken at each SwapBuffers). Output: GLTRACE.BIN next to the DLL. src/glreplay.c plays it back.
 * File: "GLTRACE1", then records: u16 id, u16 0, u32 bytes, the arguments (4 bytes each, 8 for a double), then each pointer argument as
 * u32 size (0xFFFFFFFF: not recorded, 0xFFFFFFFE: NULL) and its bytes. Ids past the GL functions: swap, context made, made current,
 * context deleted, note. MIT, 2026. */
#include <windows.h>
#include <string.h>
#include <stdio.h>
typedef unsigned int GLenum;typedef unsigned char GLboolean;typedef unsigned int GLbitfield;typedef void GLvoid;typedef signed char GLbyte;typedef short GLshort;
typedef int GLint;typedef unsigned char GLubyte;typedef unsigned short GLushort;typedef unsigned int GLuint;typedef int GLsizei;typedef float GLfloat;typedef float GLclampf;
typedef double GLdouble;typedef double GLclampd;
static int gt_rec(int id);static void gt_i(unsigned long v);static void gt_f(float v);static void gt_d(double v);static void gt_blob(const void *p,unsigned long n);
static void gt_done(void);static void gt_gen(GLsizei n,GLuint *t);static unsigned long gt_img(long w,long h,unsigned long format,unsigned long type);
static unsigned long gt_pcount(unsigned long pname);static unsigned long gt_lists(long n,unsigned long type);static void gt_store(unsigned long pname,long v);
#define GT_PROXY
#include "gltrace_gen.h"
#define GT_SWAP (GT_NFUNCS+0)
#define GT_CREATE (GT_NFUNCS+1)
#define GT_CURRENT (GT_NFUNCS+2)
#define GT_DELETE (GT_NFUNCS+3)
#define GT_NOTE (GT_NFUNCS+4)

static HMODULE real;static HANDLE out=INVALID_HANDLE_VALUE;static unsigned char *buf;static unsigned long used,start;static int on,in_begin,bad;
static unsigned long unpack_align=4,unpack_row=0,frames,unsup[GT_NFUNCS];
#define BUFSZ (12UL<<20)
static void flush(void) {DWORD w;if(out!=INVALID_HANDLE_VALUE&&used){WriteFile(out,buf,used,&w,0);}used=0;}
static void put(const void *p,unsigned long n) {if(bad)return;if(used+n>BUFSZ){bad=1;return;}memcpy(buf+used,p,n);used+=n;}
static void rec_start(int id) {unsigned short h[2];unsigned long z=0;if(used>(BUFSZ/2))flush();start=used;h[0]=(unsigned short)id;h[1]=0;put(h,4);put(&z,4);}
static void gt_done(void) {if(!bad)*(unsigned long*)(buf+start+4)=used-start-8;}
static int id_begin=-1,id_end=-1;
static int gt_rec(int id) {
 int inside;char c;
 if(id==id_begin){in_begin=1;inside=1;}else if(id==id_end){in_begin=0;inside=1;}else inside=in_begin;
 if(out==INVALID_HANDLE_VALUE||bad)return 0;
 c=gt_fns[id].cls;
 if(!on&&(inside||c=='S'||c=='Q'))return 0; /* Scroll Lock off: no drawing, no partial texture updates, no queries */
 if(c=='U')unsup[id]++;
 rec_start(id);return 1;
}
static void gt_store(unsigned long pname,long v) {if(pname==0x0CF5&&v>0)unpack_align=(unsigned long)v;else if(pname==0x0CF2&&v>=0)unpack_row=(unsigned long)v;}
static void gt_i(unsigned long v) {put(&v,4);}
static void gt_f(float v) {put(&v,4);}
static void gt_d(double v) {put(&v,8);}
static void gt_blob(const void *p,unsigned long n) {
 unsigned long m;
 if(n==0xFFFFFFFFUL){put(&n,4);return;}
 if(!p){m=0xFFFFFFFEUL;put(&m,4);return;}
 if(n>(4UL<<20)){m=0xFFFFFFFFUL;put(&m,4);return;}
 put(&n,4);put(p,n);
}
static void gt_gen(GLsizei n,GLuint *t) {(void)n;(void)t;}
static unsigned long comps(unsigned long f) {switch(f){case 0x1907:case 0x80E0:return 3;case 0x1908:case 0x80E1:return 4;case 0x190A:return 2;default:return 1;}}
static unsigned long gt_img(long w,long h,unsigned long format,unsigned long type) {
 unsigned long tb2=type==0x1402||type==0x1403?2:type==0x1404||type==0x1405||type==0x1406?4:1,row,al;long rl=unpack_row?(long)unpack_row:w;
 if(w<=0||h<=0)return 0;
 row=type==0x1A00?((unsigned long)rl+7)/8:(unsigned long)rl*comps(format)*tb2;al=(row+unpack_align-1)/unpack_align*unpack_align;
 return al*(unsigned long)(h-1)+((type==0x1A00)?((unsigned long)w+7)/8:(unsigned long)w*comps(format)*tb2);
}
static unsigned long gt_pcount(unsigned long p) {
 switch(p){case 0x2201:case 0x1004:case 0x0B66:case 0x1200:case 0x1201:case 0x1202:case 0x1203:case 0x1600:case 0x1602:case 0x0B53:case 0x2501:case 0x2502:return 4;
  case 0x1204:case 0x1603:case 0x8129:return 3;default:return 1;}
}
static unsigned long gt_lists(long n,unsigned long type) {return (unsigned long)n*(type==0x1402||type==0x1403?2:type>=0x1404&&type<=0x1406?4:type==0x1407?2:type==0x1408?3:type==0x1409?4:1);}
static void note(const char *s) {if(out==INVALID_HANDLE_VALUE)return;rec_start(GT_NOTE);put(s,(unsigned long)strlen(s)+1);gt_done();}
static void client(HDC dc,unsigned long *w,unsigned long *h) {RECT r;HWND wnd=WindowFromDC(dc);*w=*h=0;if(wnd&&GetClientRect(wnd,&r)){*w=(unsigned long)(r.right-r.left);*h=(unsigned long)(r.bottom-r.top);}}

/* the wgl functions: forwarded; context and swap events recorded */
#define R(t,n,a) static t (WINAPI *r_##n) a;
R(HGLRC,wglCreateContext,(HDC)) R(BOOL,wglDeleteContext,(HGLRC)) R(HGLRC,wglGetCurrentContext,(void)) R(HDC,wglGetCurrentDC,(void)) R(PROC,wglGetProcAddress,(LPCSTR))
R(BOOL,wglMakeCurrent,(HDC,HGLRC)) R(BOOL,wglShareLists,(HGLRC,HGLRC)) R(BOOL,wglCopyContext,(HGLRC,HGLRC,UINT)) R(HGLRC,wglCreateLayerContext,(HDC,int))
R(BOOL,wglDescribeLayerPlane,(HDC,int,int,UINT,void*)) R(int,wglSetLayerPaletteEntries,(HDC,int,int,int,const COLORREF*)) R(int,wglGetLayerPaletteEntries,(HDC,int,int,int,COLORREF*))
R(BOOL,wglRealizeLayerPalette,(HDC,int,BOOL)) R(BOOL,wglSwapLayerBuffers,(HDC,UINT)) R(BOOL,wglUseFontBitmapsA,(HDC,DWORD,DWORD,DWORD)) R(BOOL,wglUseFontBitmapsW,(HDC,DWORD,DWORD,DWORD))
R(BOOL,wglUseFontOutlinesA,(HDC,DWORD,DWORD,DWORD,FLOAT,FLOAT,int,void*)) R(BOOL,wglUseFontOutlinesW,(HDC,DWORD,DWORD,DWORD,FLOAT,FLOAT,int,void*))
R(int,wglChoosePixelFormat,(HDC,const PIXELFORMATDESCRIPTOR*)) R(int,wglDescribePixelFormat,(HDC,int,UINT,PIXELFORMATDESCRIPTOR*)) R(int,wglGetPixelFormat,(HDC))
R(BOOL,wglSetPixelFormat,(HDC,int,const PIXELFORMATDESCRIPTOR*)) R(BOOL,wglSwapBuffers,(HDC))
HGLRC WINAPI gt_wglCreateContext(HDC dc) {HGLRC r=r_wglCreateContext(dc);unsigned long w,h;client(dc,&w,&h);if(out!=INVALID_HANDLE_VALUE){rec_start(GT_CREATE);gt_i((unsigned long)r);gt_i(w);gt_i(h);gt_done();}return r;}
BOOL WINAPI gt_wglDeleteContext(HGLRC c) {if(out!=INVALID_HANDLE_VALUE){rec_start(GT_DELETE);gt_i((unsigned long)c);gt_done();flush();}return r_wglDeleteContext(c);}
BOOL WINAPI gt_wglMakeCurrent(HDC dc,HGLRC c) {unsigned long w,h;client(dc,&w,&h);if(out!=INVALID_HANDLE_VALUE){rec_start(GT_CURRENT);gt_i((unsigned long)c);gt_i(w);gt_i(h);gt_done();}return r_wglMakeCurrent(dc,c);}
BOOL WINAPI gt_wglSwapBuffers(HDC dc) {
 unsigned long w,h;int now;BOOL r;
 client(dc,&w,&h);
 if(out!=INVALID_HANDLE_VALUE){rec_start(GT_SWAP);gt_i(frames++);gt_i((unsigned long)on);gt_i(w);gt_i(h);gt_done();}
 r=r_wglSwapBuffers(dc);
 now=(GetKeyState(VK_SCROLL)&1)!=0;if(now!=on){on=now;MessageBeep(now?MB_OK:MB_ICONHAND);flush();}
 if(used>(BUFSZ/4))flush();
 return r;
}
PROC WINAPI gt_wglGetProcAddress(LPCSTR n) {
 PROC p=r_wglGetProcAddress(n);int i;char t[160];
 if(!p||!n)return p;
 for(i=0;i<GT_NFUNCS;i++)if(gt_fns[i].ext&&!strcmp(gt_fns[i].name,n)){*gt_real[i]=(void*)p;return (PROC)gt_wrap[i];}
 _snprintf(t,sizeof(t)-1,"wglGetProcAddress(%s): not recorded",n);t[sizeof(t)-1]=0;note(t);
 return p;
}
HGLRC WINAPI gt_wglGetCurrentContext(void) {return r_wglGetCurrentContext();}
HDC WINAPI gt_wglGetCurrentDC(void) {return r_wglGetCurrentDC();}
BOOL WINAPI gt_wglShareLists(HGLRC a,HGLRC b) {return r_wglShareLists(a,b);}
BOOL WINAPI gt_wglCopyContext(HGLRC a,HGLRC b,UINT m) {return r_wglCopyContext(a,b,m);}
HGLRC WINAPI gt_wglCreateLayerContext(HDC d,int l) {return r_wglCreateLayerContext(d,l);}
BOOL WINAPI gt_wglDescribeLayerPlane(HDC d,int a,int b,UINT c,void *e) {return r_wglDescribeLayerPlane(d,a,b,c,e);}
int WINAPI gt_wglSetLayerPaletteEntries(HDC d,int a,int b,int c,const COLORREF *e) {return r_wglSetLayerPaletteEntries(d,a,b,c,e);}
int WINAPI gt_wglGetLayerPaletteEntries(HDC d,int a,int b,int c,COLORREF *e) {return r_wglGetLayerPaletteEntries(d,a,b,c,e);}
BOOL WINAPI gt_wglRealizeLayerPalette(HDC d,int a,BOOL b) {return r_wglRealizeLayerPalette(d,a,b);}
BOOL WINAPI gt_wglSwapLayerBuffers(HDC d,UINT p) {return r_wglSwapLayerBuffers(d,p);}
BOOL WINAPI gt_wglUseFontBitmapsA(HDC d,DWORD a,DWORD b,DWORD c) {return r_wglUseFontBitmapsA(d,a,b,c);}
BOOL WINAPI gt_wglUseFontBitmapsW(HDC d,DWORD a,DWORD b,DWORD c) {return r_wglUseFontBitmapsW(d,a,b,c);}
BOOL WINAPI gt_wglUseFontOutlinesA(HDC d,DWORD a,DWORD b,DWORD c,FLOAT e,FLOAT f,int g,void *h) {return r_wglUseFontOutlinesA(d,a,b,c,e,f,g,h);}
BOOL WINAPI gt_wglUseFontOutlinesW(HDC d,DWORD a,DWORD b,DWORD c,FLOAT e,FLOAT f,int g,void *h) {return r_wglUseFontOutlinesW(d,a,b,c,e,f,g,h);}
/* the pixel format through GDI (as a program using OPENGL32 does): GDI keeps the window's format and calls the real OPENGL32 itself */
int WINAPI gt_wglChoosePixelFormat(HDC d,const PIXELFORMATDESCRIPTOR *p) {return ChoosePixelFormat(d,p);}
int WINAPI gt_wglDescribePixelFormat(HDC d,int i,UINT n,PIXELFORMATDESCRIPTOR *p) {return DescribePixelFormat(d,i,n,p);}
int WINAPI gt_wglGetPixelFormat(HDC d) {return GetPixelFormat(d);}
BOOL WINAPI gt_wglSetPixelFormat(HDC d,int i,const PIXELFORMATDESCRIPTOR *p) {return SetPixelFormat(d,i,p);}


BOOL WINAPI DllMain(HINSTANCE inst,DWORD why,LPVOID res) {
 (void)res;
 if(why==DLL_PROCESS_ATTACH){
  char p[MAX_PATH],*e;int i;
  GetSystemDirectoryA(p,sizeof(p));strcat(p,"\\OPENGL32.DLL");real=LoadLibraryA(p);if(!real)return FALSE;
  for(i=0;i<GT_NFUNCS;i++){if(!gt_fns[i].ext)*gt_real[i]=(void*)GetProcAddress(real,gt_fns[i].name);if(!strcmp(gt_fns[i].name,"glBegin"))id_begin=i;if(!strcmp(gt_fns[i].name,"glEnd"))id_end=i;}
#define G(n) *(FARPROC*)&r_##n=GetProcAddress(real,#n);
  G(wglCreateContext)G(wglDeleteContext)G(wglGetCurrentContext)G(wglGetCurrentDC)G(wglGetProcAddress)G(wglMakeCurrent)G(wglShareLists)G(wglCopyContext)G(wglCreateLayerContext)
  G(wglDescribeLayerPlane)G(wglSetLayerPaletteEntries)G(wglGetLayerPaletteEntries)G(wglRealizeLayerPalette)G(wglSwapLayerBuffers)G(wglUseFontBitmapsA)G(wglUseFontBitmapsW)
  G(wglUseFontOutlinesA)G(wglUseFontOutlinesW)G(wglChoosePixelFormat)G(wglDescribePixelFormat)G(wglGetPixelFormat)G(wglSetPixelFormat)G(wglSwapBuffers)
  buf=(unsigned char*)VirtualAlloc(0,BUFSZ,MEM_COMMIT,PAGE_READWRITE);
  GetModuleFileNameA(inst,p,sizeof(p));e=strrchr(p,'\\');if(e)strcpy(e+1,"GLTRACE.BIN");
  if(buf)out=CreateFileA(p,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
  if(out!=INVALID_HANDLE_VALUE){put("GLTRACE1",8);}
  on=(GetKeyState(VK_SCROLL)&1)!=0;
 } else if(why==DLL_PROCESS_DETACH){
  if(out!=INVALID_HANDLE_VALUE){int i;char t[200];
   for(i=0;i<GT_NFUNCS;i++)if(unsup[i]){_snprintf(t,sizeof(t)-1,"%s called %lu times: not replayed",gt_fns[i].name,unsup[i]);t[sizeof(t)-1]=0;note(t);}
   if(bad)note("a record did not fit: the trace stops there");
   flush();CloseHandle(out);out=INVALID_HANDLE_VALUE;}
 }
 return TRUE;
}
