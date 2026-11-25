/* VXVBL v1.1 (driver .776): the HAL's own vertex buffers are deleted (board, E775: with them a locked buffer held the 16-bit lock, and
 * kept it after the unlock); this must now always answer FREE. v1.0 (driver .773): does a locked vertex buffer hold the 16-bit system
 * lock? A Direct3D 8 device with hardware vertex processing, one static vertex buffer in the default pool, locked and left
 * locked while a second thread calls GetDC(0) / ReleaseDC (USER and GDI need the 16-bit lock). With the lock held by the first thread
 * the second cannot return until the buffer is unlocked: a program that waits for such a thread while its buffers are locked stops for
 * good (3DMark 2001 on E768-E772, hardware T&L: Direct3D 8 keeps a static buffer locked until the first draw that uses it).
 * Reports the time the second thread took with the buffer locked and after the unlock.
 * VXVBL.LOG. Nothing is changed in the driver. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
typedef unsigned long U32;
#define VT(o,i) (((void**)(*(void***)(o)))[i])
typedef struct {U32 w,h,refresh,format;} MODE;
typedef struct {U32 bw,bh,bfmt,bcount,ms,swap;HWND wnd;U32 windowed,autoz,zfmt,flags,rate,interval;} PP;
static FILE *logf;
static void say(const char *f,...) {static char b[1024];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);if(logf){fputs(b,logf);fflush(logf);}}
static HANDLE done;static volatile DWORD t_in,t_out;
static DWORD WINAPI other(LPVOID p) {HDC d;(void)p;t_in=GetTickCount();d=GetDC(0);if(d)ReleaseDC(0,d);t_out=GetTickCount();SetEvent(done);return 0;}
int main(void) {
 HMODULE d8;void *(__stdcall *create)(U32);void *d3d,*dev=0,*vb=0;PP pp;MODE m;WNDCLASS wc;HWND wnd;HRESULT hr;BYTE *bits=0;DWORD w,tid,t0;HANDLE th;
 int held=0;
 logf=fopen("VXVBL.LOG","w");
 say("VXVBL v1.1 (driver .776): a locked vertex buffer and the 16-bit system lock\n");
 d8=LoadLibraryA("d3d8.dll");create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 if(!create){say("d3d8.dll / Direct3DCreate8 not found\nFAIL\n");return 1;}
 d3d=create(220);if(!d3d){say("Direct3DCreate8 failed\nFAIL\n");return 1;}
 memset(&m,0,sizeof(m));((HRESULT(__stdcall*)(void*,U32,MODE*))VT(d3d,8))(d3d,0,&m);
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=DefWindowProc;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXVBL";wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClass(&wc);
 wnd=CreateWindow("VXVBL","VXVBL",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,264,284,0,0,wc.hInstance,0);
 memset(&pp,0,sizeof(pp));pp.bw=256;pp.bh=256;pp.bfmt=m.format;pp.bcount=1;pp.swap=3;pp.wnd=wnd;pp.windowed=1;pp.autoz=1;pp.zfmt=80;
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,15))(d3d,0,1,wnd,0x40,&pp,&dev);
 say("CreateDevice (hardware vertex processing): %08lX\n",(U32)hr);
 if(hr||!dev){say("FAIL: no device\n");return 1;}
 hr=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,void**))VT(dev,23))(dev,65536,8 /* WRITEONLY */,0x42 /* XYZ | DIFFUSE */,0 /* default pool */,&vb);
 say("CreateVertexBuffer (65536 bytes, static, default pool): %08lX\n",(U32)hr);
 if(hr||!vb){say("FAIL: no buffer\n");return 1;}
 done=CreateEvent(0,TRUE,FALSE,0);
 hr=((HRESULT(__stdcall*)(void*,U32,U32,BYTE**,U32))VT(vb,11))(vb,0,0,&bits,0);
 say("Lock: %08lX, memory at %08lX\n",(U32)hr,(U32)bits);
 if(hr||!bits){say("FAIL: no lock\n");return 1;}
 memset(bits,0,65536);
 t0=GetTickCount();th=CreateThread(0,0,other,0,0,&tid);
 w=WaitForSingleObject(done,3000);
 if(w==WAIT_OBJECT_0)say("with the buffer locked, a second thread's GetDC and ReleaseDC took %lu ms: the 16-bit lock is NOT held\n",t_out-t_in);
 else{held=1;say("with the buffer locked, a second thread's GetDC did not return in 3000 ms: the 16-bit lock IS held by the lock\n");}
 hr=((HRESULT(__stdcall*)(void*))VT(vb,12))(vb);
 say("Unlock: %08lX\n",(U32)hr);
 w=WaitForSingleObject(done,5000);
 if(w==WAIT_OBJECT_0)say("after the unlock the second thread had returned %lu ms after it started\n",t_out-t0);
 else say("after the unlock the second thread still did not return in 5000 ms\n");
 if(th)CloseHandle(th);
 ((HRESULT(__stdcall*)(void*))VT(vb,2))(vb);((HRESULT(__stdcall*)(void*))VT(dev,2))(dev);((HRESULT(__stdcall*)(void*))VT(d3d,2))(d3d);DestroyWindow(wnd);
 say(held?"RESULT: HELD (WRONG: report this; a program with a second thread can stop for good while such a buffer is locked)\n":"RESULT: FREE\n");
 if(logf)fclose(logf);
 return 0;
}
