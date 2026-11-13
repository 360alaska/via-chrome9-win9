/* VXNOSYNC v1.2 (driver .558: v1.1's four passes did not hang the ring on .557, but none of them put v1.0's frame first; v1.2 runs it first and last, five passes). VXNOSYNC v1.1 (driver .557): four passes of 150 frames, to find what made v1.0 hang the command ring on .556 (its third flip never
 * ran; the driver took the 100 ms way out and flipped by the CPU from then on; GTA3 with Frame Sync off flipped 1,630 times in the ring
 * without it): 1 no vsync, no lock; 2 vsync, no lock; 3 vsync, the back buffer locked after the clear; 4 no vsync, locked (v1.0's frame).
 * After each pass: the VxD's ring flips, declined flips, waits for a ring flip that ran out, CPU flips. The first pass whose "ran out"
 * grows is the one that hangs the ring; ring flips are off after it until a restart, so the passes after it say nothing.
 * VXNOSYNC v1.0 (driver .555): a flip without vsync (Direct3D 8's D3DPRESENT_INTERVAL_IMMEDIATE, DirectDraw's DDFLIP_NOVSYNC: GTA3's
 * "Frame Sync" off) must not show a frame while it is drawn.
 * Until .554 such a flip wrote the new start address and let the application go on at once. The chip takes a new address at the next
 * blank only, so the old front buffer stayed on the screen while the next frame was cleared and drawn into it: the picture flashed.
 * From .555 the flip is carried out as the one-frame flip (in the command ring, the old front guarded until the display has taken it).
 * This program runs full screen at 640x480 without vsync and draws 240 frames: each is cleared to BLACK, left black for 6 ms, then
 * covered with a WHITE rectangle and presented. A finished frame is all white, so:
 *    WATCH THE SCREEN: it must stay white. Black flashes or black bands are the fault.
 * It also reads the VxD's flip counts (TEXDBG BC 98, 99): on .555 the flips go into the ring ("ring flips" grows by the frames, "CPU
 * flips" does not), and prints the frame time. Results to VXNOSYNC.LOG beside the program. It runs on .554 too, where it flashes.
 * VXD3DMON must not be running. Esc ends it early. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef unsigned long U32;
#define VT(o,i) (((void**)(*(void***)(o)))[i])
typedef HRESULT (__stdcall *F0)(void*);
typedef HRESULT (__stdcall *F1)(void*,U32);
typedef HRESULT (__stdcall *F2)(void*,U32,U32);
typedef HRESULT (__stdcall *F2P)(void*,U32,void*);
typedef HRESULT (__stdcall *FDPUP)(void*,U32,U32,const void*,U32);
enum {D3D_REL=2,D3D_MODE=8,D3D_CREATEDEV=15};
enum {SURF_LOCK=9,SURF_UNLOCK=10,DEV_BACKBUF=16};
enum {DEV_PRESENT=15,DEV_BEGIN=34,DEV_END=35,DEV_CLEAR=36,DEV_SETRS=50,DEV_SETTEX=61,DEV_DPUP=72,DEV_SETVS=76};
typedef struct {U32 w,h,refresh,format;} MODE;
typedef struct {U32 bw,bh,bfmt,bcount,ms,swap;HWND wnd;U32 windowed,autoz,zfmt,flags,rate,interval;} PP;
typedef struct {float x,y,z,w;U32 c;} TL; /* FVF 0x44: XYZRHW, DIFFUSE */
typedef struct {long pitch;void *bits;} LR;
#define FRAMES 150

static FILE *logf;
static void say(const char *f,...) {static char b[1024];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);fflush(stdout);if(logf){fputs(b,logf);fflush(logf);}}
static HANDLE vxd=INVALID_HANDLE_VALUE;
static int texdbg2(DWORD code,DWORD i,DWORD *o) {DWORD in[2],got=0;in[0]=code;in[1]=i;memset(o,0,32);return vxd!=INVALID_HANDLE_VALUE&&DeviceIoControl(vxd,0x85500022UL,in,8,o,32,&got,NULL);}
/* BC 98: o[2] ring flips, o[3] declined, o[6] waits for a ring flip that ran out (100 ms); BC 99: o[6] CPU flips after ring flips */
static int counts(DWORD *c) {DWORD a[8],b[8];if(!texdbg2(0xBC,0x98,a)||!texdbg2(0xBC,0x99,b)||(a[0]&0xFF)!=0xBC)return 0;c[0]=a[2];c[1]=a[3];c[2]=a[6];c[3]=b[6];return 1;}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
static void *d3d;static HWND wnd;static U32 deskfmt;static int stop;
static void pass(int no,int vsync,int lock) {
 void *dev=0;PP pp;HRESULT hr;DWORD c0[4],c1[4];int have,k,done=0;LARGE_INTEGER f,a,b;TL q[4];
 say("pass %d: %s, %s: ",no,vsync?"vsync":"no vsync",lock?"the back buffer locked after the clear":"no lock"); /* said first: if the machine stops, the log names the pass */
 memset(&pp,0,sizeof(pp));pp.bw=640;pp.bh=480;pp.bfmt=(deskfmt==23)?23:22;pp.bcount=1;pp.swap=2;pp.wnd=wnd;pp.windowed=0;pp.autoz=0;pp.interval=vsync?1UL:0x80000000UL; /* D3DSWAPEFFECT_FLIP; D3DPRESENT_INTERVAL_ONE / IMMEDIATE */
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);
 if(hr||!dev){pp.bfmt=pp.bfmt==23?22:23;hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);}
 if(hr||!dev){say("CreateDevice failed, %08lX\n",(U32)hr);return;}
 q[0].x=0;q[0].y=0;q[1].x=640;q[1].y=0;q[2].x=0;q[2].y=480;q[3].x=640;q[3].y=480;
 for(k=0;k<4;k++){q[k].z=0.5f;q[k].w=1.0f;q[k].c=0xFFFFFFFFUL;}
 have=counts(c0);
 QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
 for(k=0;k<FRAMES;k++){MSG m;
  while(PeekMessage(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}
  if(GetAsyncKeyState(VK_ESCAPE)&0x8000){stop=1;break;}
  ((F0)VT(dev,DEV_BEGIN))(dev);
  ((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,1,0xFF000000UL,1.0f,0);
  ((F0)VT(dev,DEV_END))(dev);
  if(lock){void *bb=0;LR lr; /* a read lock of the back buffer: the runtime and the driver send what they hold, so the clear is in the buffer now */
   if(!((HRESULT(__stdcall*)(void*,U32,U32,void**))VT(dev,DEV_BACKBUF))(dev,0,0,&bb)&&bb){
    if(!((HRESULT(__stdcall*)(void*,LR*,void*,U32))VT(bb,SURF_LOCK))(bb,&lr,0,0x10))((F0)VT(bb,SURF_UNLOCK))(bb);
    ((F0)VT(bb,D3D_REL))(bb);}}
  Sleep(6);
  ((F0)VT(dev,DEV_BEGIN))(dev);
  ((F2)VT(dev,DEV_SETRS))(dev,137,0);((F2)VT(dev,DEV_SETRS))(dev,7,0);((F2)VT(dev,DEV_SETRS))(dev,22,1);((F2)VT(dev,DEV_SETRS))(dev,27,0);
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F1)VT(dev,DEV_SETVS))(dev,0x44);
  ((FDPUP)VT(dev,DEV_DPUP))(dev,5,2,q,sizeof(TL));
  ((F0)VT(dev,DEV_END))(dev);
  ((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);done++;}
 QueryPerformanceCounter(&b);
 if(have)have=counts(c1);
 ((F0)VT(dev,D3D_REL))(dev);
 say("%d frames, %.2f ms each",done,done?(double)(b.QuadPart-a.QuadPart)*1000.0/(double)f.QuadPart/done:0.0);
 if(have)say("; ring flips %lu, declined %lu, waits that ran out %lu, CPU flips after ring flips %lu%s\n",c1[0]-c0[0],c1[1]-c0[1],c1[2]-c0[2],c1[3]-c0[3],c1[2]!=c0[2]?"  <-- THE RING HUNG IN THIS PASS":"");
 else say("; the VxD's flip counts: not read\n");
}
int main(void) {
 HMODULE d8;void *(__stdcall *create)(U32);MODE mode;WNDCLASS wc;char path[MAX_PATH],*s;
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXNOSYNC.LOG");logf=fopen(path,"w");
 say("VXNOSYNC v1.2 (driver .558): flips with and without vsync, with and without a lock of the back buffer. The screen must stay white in every pass.\n");
 vxd=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);
 d8=LoadLibrary("d3d8.dll");create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 if(!create){say("d3d8.dll / Direct3DCreate8 not found (DirectX 8 or later is needed)\n");return 1;}
 d3d=create(220);if(!d3d)d3d=create(120);
 if(!d3d){say("Direct3DCreate8 failed\n");return 1;}
 memset(&mode,0,sizeof(mode));((F2P)VT(d3d,D3D_MODE))(d3d,0,&mode);deskfmt=mode.format;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXNOSYNC";RegisterClass(&wc);
 wnd=CreateWindow("VXNOSYNC","VXNOSYNC",WS_POPUP|WS_VISIBLE,0,0,640,480,0,0,wc.hInstance,0);
 pass(1,0,1);if(!stop)pass(2,0,0);if(!stop)pass(3,1,0);if(!stop)pass(4,1,1);if(!stop)pass(5,0,1); /* v1.2: v1.0's frame first, as in v1.0 (the first frames after the start), and once more at the end */
 ((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);
 say("Judge by eye: all white = right; black flashes or bands = a frame was shown while it was drawn. Send this log.\n");
 if(logf)fclose(logf);
 return 0;
}
