/* VXNOSYNC v2.5 (driver .733): passes 6-7 on the VX900 judged as CPU flips (its VxD now reports ring flips off). VXNOSYNC v2.4 (driver .729): pass 9 draws 5 s before it measures (the monitor resyncs after the mode change), then measures 8 s, not 120 frames (the VX900 ran them in 2 s, gone before a monitor showed them).
 * VXNOSYNC v2.3 (driver .716): pass 9 says "kept off on this chip (as designed)" when the driver refuses the tear flip, not WRONG.
 * VXNOSYNC v2.2 (driver .698): no questions unless asked (ASK): the run needs no one at the machine; pass 9 is judged by its counts and
 * frame rates alone. VXNOSYNC v2.1 (driver .698): pass 9's 20 ms is the engine's work (20 blended full-screen quads), not the CPU held: v2.0 (T697, board B)
 * gave 49.4 fps with both settings, as the old flip's wait for the blank is in the ring and only costs when the engine is behind it.
 * VXNOSYNC v2.1 (driver .698): pass 9, the tear flip (ACCEL_FLIP_TEAR, set live through TEXDBG BC 44B0000 + value and put back after).
 * A no-vsync flip used to be shown at the next blank (.555), so a frame that missed a refresh waited for the next: 3DMark2000 locked to
 * 30, 15, 7.5 fps. .696 takes it from the next scan line (CR32 bit 0, real-time flipping). Pass 9 draws 120 frames of 20 ms each (white,
 * a red bar across the middle, v2.1: 20 blended quads, about 20 ms of the engine) with each setting: 0 the old flip (expect about half the
 * refresh), 1 the tear flip (expect about 50 fps), 2 also with IGA2 mirroring (only when it does). After each, a question: did the screen
 * stay white with the bar still? Thin tear lines are right; black flashes or a jumping, doubled bar are not. Passes 1-5 (no vsync) now
 * run with the tear flip too: they must stay white as before.
 * VXNOSYNC v1.9 (driver .695): the second display unit. Each pass says whether IGA2 shows the frame buffer (a mirrored monitor) and how
 * many ring flips moved it too (BC 85, .695: IGA2's swap words in the ring); with IGA2 mirroring, every ring flip must move it. With IGA2
 * alone (a laptop panel) the ring is not used (its wait is IGA1's blank): every flip by the CPU, none run out, is right.
 * VXNOSYNC v1.7 (driver .687): passes 6-7 judged by the ring-flip setting: with ACCEL_3D_RINGFLIP 0 every flip by the CPU is right.
 * VXNOSYNC v1.6 (driver .671): the board, .670: the engine and the CPU gave the same picture on the covered sizes (9,156 / 6,441 pixels
 * off one stretch of the whole window with both), and the HAL counted 4 Blts a covered Present, one clip rectangle each: the runtime
 * sends one Blt for each visible rectangle with its own whole-pixel source rectangle. So pass 8 and STRETCH judge the engine's picture
 * against the CPU's on the same Present (ACCEL_STRETCH3D set live), and hold only the uncovered sizes to the one-stretch expectation.
 * The offset sweep and ACCEL_STRETCH3D_BIAS are gone (offset 0 was exact). The HAL's CPU-blit count is now a running count.
 * VXNOSYNC v1.5 (driver .670): STRETCH adds part 4: .669 found every size exact without a covering window and 9,156 / 6,441 pixels off by
 * one with it. The covered sizes again with the engine and with the CPU's stretch (ACCEL_STRETCH3D set live through TEXDBG BC 44A0000 +
 * value, put back after), and the HAL's counts per Present (VX_D3D_QueryPhases 428, 431-433: the engine's stretches, those with a clip
 * list, their rectangles, the CPU's blits): one Blt with a clip list, or the runtime's own Blt for each rectangle.
 * VXNOSYNC v1.4 (driver .669): VXNOSYNC STRETCH runs only the stretch measure (STRETCH.BAT): .668's pass 8 found 13.6% of the pixels
 * of a 256 x 256 back buffer stretched by the engine to 258 x 259 reading another source pixel than the CPU's stretch. It reads each
 * pixel's source pixel from its colour, counts the differences by axis and by how far off, sweeps the driver's sample offset
 * (ACCEL_STRETCH3D_BIAS, set live through TEXDBG BC 4490000 + value) from -8 to +8 sixteenths of a pixel on each axis, picks the
 * offset with the fewest differences, and checks it on six sizes (two with a covering window, a clip list). Results to VXNOSYNC.LOG.
 * VXNOSYNC v1.3 (driver .667): two new parts. (a) Passes 6 and 7: flips after more than 100 ms of drawing queued in the ring (150
 * alpha-blended full-screen quads a frame, then the white quad), without and with vsync: on .666 the wait for such a flip ran out at
 * 100 ms with the engine still drawing and ring flips were off for the rest of the boot (3DMark 2001, then HarborBench 11-20% slower in
 * Direct3D). Right: every frame a ring flip, "ran out" 0, ring flips still on; "waits past 100 ms" above 0 shows the case was made.
 * (b) Pass 8: a windowed Present stretched by the driver (a 256 x 256 back buffer into a 258 x 259 window, part of it covered by a
 * second window, so the Blt has a clip list): .666 did it on the CPU (8.5 ms a Present); .667 on the 3D engine. Each source pixel has its
 * own colour; the window is read back and every uncovered pixel compared with the nearest-pixel source the CPU's stretch picks.
 * VXNOSYNC v1.2 (driver .558: v1.1's four passes did not hang the ring on .557, but none of them put v1.0's frame first; v1.2 runs it first and last, five passes). VXNOSYNC v1.1 (driver .557): four passes of 150 frames, to find what made v1.0 hang the command ring on .556 (its third flip never
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
static int counts(DWORD *c) {DWORD a[8],b[8],e[8];if(!texdbg2(0xBC,0x98,a)||!texdbg2(0xBC,0x99,b)||(a[0]&0xFF)!=0xBC)return 0;c[0]=a[2];c[1]=a[3];c[2]=a[6];c[3]=b[6];c[4]=b[7];
 c[5]=0xFFFFFFFFUL;c[6]=0;c[7]=0;if(texdbg2(0xBC,0x85,e)&&e[1]==0x85){c[5]=e[4];c[6]=e[5];c[7]=e[6];}return 1;} /* v1.9: c[5] IGA2 (bit 0 mirrors, bit 1 alone; FFFFFFFF an older VxD), c[6] ring flips that moved IGA2, c[7] late ring flips */
static void say_two(const DWORD *c0,const DWORD *c1) {
 if(c1[5]==0xFFFFFFFFUL){say("; IGA2: not read (a VxD older than .695)");return;}
 say("; IGA2 %s (CR6A %02lX), ring flips that moved IGA2 %lu, ring flips that ran late %lu",(c1[5]&2)?"ALONE (panel): ring not used":(c1[5]&1)?"MIRRORS this frame buffer":"off or another picture",(c1[5]>>8)&0xFF,c1[6]-c0[6],c1[7]-c0[7]);} /* v1.7: c[4] the ring-flip setting (ACCEL_3D_RINGFLIP, .685) */
/* .667, BC 447: [2] ring-flip waits past 100 ms with the engine moving, [3] those that ran out, [4] ring flips given up for the boot */
static int counts2(DWORD *c) {DWORD a[8];if(!texdbg2(0xBC,0x447,a)||(a[1]&0xFFF)!=0x447)return 0;c[0]=a[2];c[1]=a[3];c[2]=a[4];return 1;}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
static void *d3d;static HWND wnd;static U32 deskfmt;static int stop;
#define TEAR_SETTLE_S 5 /* v2.4: seconds drawn before measuring (the monitor's resync) */
#define TEAR_MEASURE_S 8 /* v2.4: seconds measured and watched */
#define TEAR_QUADS 20 /* v2.1: alpha-blended full-screen quads a frame (pass 6's 150 take 136-190 ms on the boards: these about 18-25 ms) */
static int tear_get(DWORD *o) {return texdbg2(0xBC,0x44B0003UL,o)&&(o[0]&0xFF)==0xBC;}
static void tear_set(DWORD v) {DWORD o[8];texdbg2(0xBC,0x44B0000UL|(v&3),o);}
static int tear_run(DWORD set,double *fps,DWORD *ring,DWORD *cpu,DWORD *kept,DWORD *two) { /* v2.0: 0 = could not run */
 void *dev=0;PP pp;HRESULT hr;DWORD t0[8],t1[8],c0[8],c1[8];int k,j,done=0,ht,hc;LARGE_INTEGER f,a,b,s;TL q[4],r[4],h[4];
 tear_set(set);
 memset(&pp,0,sizeof(pp));pp.bw=640;pp.bh=480;pp.bfmt=(deskfmt==23)?23:22;pp.bcount=1;pp.swap=2;pp.wnd=wnd;pp.windowed=0;pp.autoz=0;pp.interval=0x80000000UL; /* IMMEDIATE: DDFLIP_NOVSYNC */
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);
 if(hr||!dev){pp.bfmt=pp.bfmt==23?22:23;hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);}
 if(hr||!dev){say("CreateDevice failed, %08lX\n",(U32)hr);return 0;}
 q[0].x=0;q[0].y=0;q[1].x=640;q[1].y=0;q[2].x=0;q[2].y=480;q[3].x=640;q[3].y=480;
 r[0].x=0;r[0].y=210;r[1].x=640;r[1].y=210;r[2].x=0;r[2].y=270;r[3].x=640;r[3].y=270;
 for(k=0;k<4;k++){q[k].z=r[k].z=0.5f;q[k].w=r[k].w=1.0f;q[k].c=0xFFFFFFFFUL;r[k].c=0xFFFF0000UL;h[k]=q[k];h[k].c=0x10808080UL;}
 /* v2.4 (.729): the monitor first: a full-screen mode change takes a monitor a few seconds to show a picture (the VX900's 120 frames were
  * over before it did), so the same frames are drawn for 5 s unmeasured, then measured for 8 s: the time, not a frame count, at any speed */
 QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);ht=hc=0;
 for(k=0;;k++){MSG m;int measuring;
  QueryPerformanceCounter(&s);
  measuring=f.QuadPart&&(s.QuadPart-a.QuadPart)>=f.QuadPart*TEAR_SETTLE_S;
  if(measuring&&!done&&!ht&&!hc){ht=tear_get(t0);hc=counts(c0);QueryPerformanceCounter(&a);a.QuadPart-=f.QuadPart*TEAR_SETTLE_S;}
  if(!f.QuadPart||(s.QuadPart-a.QuadPart)>=f.QuadPart*(TEAR_SETTLE_S+TEAR_MEASURE_S))break;
  while(PeekMessage(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}
  if(GetAsyncKeyState(VK_ESCAPE)&0x8000){stop=1;break;}
  ((F0)VT(dev,DEV_BEGIN))(dev);
  ((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,1,0xFF000000UL,1.0f,0);
  ((F2)VT(dev,DEV_SETRS))(dev,137,0);((F2)VT(dev,DEV_SETRS))(dev,7,0);((F2)VT(dev,DEV_SETRS))(dev,22,1);((F2)VT(dev,DEV_SETRS))(dev,27,0);
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F1)VT(dev,DEV_SETVS))(dev,0x44);
  ((F2)VT(dev,DEV_SETRS))(dev,27,1);((F2)VT(dev,DEV_SETRS))(dev,19,5);((F2)VT(dev,DEV_SETRS))(dev,20,6); /* v2.1: the frame's work on the engine, ~20 ms */
  for(j=0;j<TEAR_QUADS;j++)((FDPUP)VT(dev,DEV_DPUP))(dev,5,2,h,sizeof(TL));
  ((F2)VT(dev,DEV_SETRS))(dev,27,0);
  ((FDPUP)VT(dev,DEV_DPUP))(dev,5,2,q,sizeof(TL));((FDPUP)VT(dev,DEV_DPUP))(dev,5,2,r,sizeof(TL));
  ((F0)VT(dev,DEV_END))(dev);
  ((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);if(measuring)done++;}
 QueryPerformanceCounter(&b);a.QuadPart+=f.QuadPart*TEAR_SETTLE_S; /* the measured part only */
 if(ht)ht=tear_get(t1);if(hc)hc=counts(c1);
 ((F0)VT(dev,D3D_REL))(dev);
 *fps=done&&b.QuadPart>a.QuadPart?done*(double)f.QuadPart/(double)(b.QuadPart-a.QuadPart):0.0;
 *ring=ht?t1[3]-t0[3]:0;*cpu=ht?t1[4]-t0[4]:0;*kept=ht?t1[5]-t0[5]:0;*two=hc&&c1[5]!=0xFFFFFFFFUL?c1[5]:0;
 return done>0;
}
static int g_ask; /* v2.2: the eye questions only with ASK on the command line (VXNOSYNC ASK, VXNOSYNC TEAR ASK): ALL.BAT runs unattended */
static int ask(const char *q) {int a;if(!g_ask){say("   (not asked: run VXNOSYNC ASK to answer: %s)\n",q);return 1;}a=MessageBoxA(wnd,q,"VXNOSYNC (Y = yes, N = no)",MB_YESNO|MB_ICONQUESTION|MB_TOPMOST|MB_SETFOREGROUND);say("   QUESTION: %s -> %s\n",q,a==IDYES?"YES":"NO");return a==IDYES;}
static int tear_pass(int no) { /* v2.0 */
 DWORD o[8],keep,set,ring,cpu,kept,two,n;double fps,f0=0.0;int ok=1,eye,ran;
 if(!tear_get(o)){say("pass %d (the tear flip): the VxD does not answer BC 44B0003 (older than .696)\n",no);return 1;}
 keep=o[2];n=2;
 /* v2.3 (.716): a driver that keeps the tear flip off on this chip (its boot default, and a live set refused) is as designed, not WRONG */
 tear_set(1);if(tear_get(o)&&o[2]!=1){tear_set(keep);say("pass %d (the tear flip): the driver keeps it off on this chip (as designed): not tested\n",no);return 1;}
 tear_set(keep);
 for(set=0;set<=n&&!stop;set++){
  say("pass %d, ACCEL_FLIP_TEAR %lu: 5 s for the monitor, then 8 s of frames of about 20 ms of drawing without vsync, a red bar across the middle: ",no,set);
  ran=tear_run(set,&fps,&ring,&cpu,&kept,&two);
  if(!ran){ok=0;break;}
  if(set==0){f0=fps;if(!(two&1)||(two&2))n=1;} /* setting 2 differs from 1 only with IGA2 mirroring */
  say("%.1f fps; tear flips in the ring %lu, by the CPU %lu, kept as the one-frame flip %lu; IGA2 %s\n",fps,ring,cpu,kept,(two&2)?"alone (panel)":(two&1)?"mirrors":"off or another picture");
  eye=stop?1:ask(set==0?"Setting 0 (the old flip): did the screen stay white with the red bar still?":"Tear flip: did the screen stay white with the red bar still (thin tear lines are fine; black flashes or a jumping or doubled bar are not)?");
  if(set==0)continue;
  if(two&2){if(!(kept>=100&&ring+cpu==0))ok=0;say("   %s\n",kept>=100&&ring+cpu==0?"RIGHT: IGA2 alone (a panel): no flip tears, as built":"WRONG: IGA2 alone but flips tore");continue;}
  if(set==1&&(two&1)){if(!(kept>=100&&ring+cpu==0))ok=0;say("   %s\n",kept>=100&&ring+cpu==0?"RIGHT: IGA2 mirrors: setting 1 keeps the one-frame flip":"WRONG: IGA2 mirrors but setting 1 tore");continue;}
  {int all=ring+cpu>=100,fast=fps>=f0*1.15; /* v2.1: the engine's work, so the old flip waits for a blank in the ring each frame and the tear flip does not */if(!all||!fast||!eye)ok=0;
   say("   %s%s%s%s\n",all&&fast&&eye?"RIGHT: every flip tore, faster than setting 0, the picture right":"WRONG:",all?"":" flips did not tear",fast?"":" no faster than setting 0",eye?"":" the picture was wrong (see the answer)");}
 }
 tear_set(keep);say("   ACCEL_FLIP_TEAR put back to %lu\n",keep);
 return ok;
}
static void pass(int no,int vsync,int lock) {
 void *dev=0;PP pp;HRESULT hr;DWORD c0[8],c1[8];int have,k,done=0;LARGE_INTEGER f,a,b;TL q[4];
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
 if(have){say("; ring flips %lu, declined %lu, waits that ran out %lu, CPU flips after ring flips %lu",c1[0]-c0[0],c1[1]-c0[1],c1[2]-c0[2],c1[3]-c0[3]);say_two(c0,c1);
  say("%s\n",c1[2]!=c0[2]?"  <-- THE RING HUNG IN THIS PASS":(c1[5]!=0xFFFFFFFFUL&&(c1[5]&1)&&!(c1[5]&2)&&c1[6]-c0[6]!=c1[0]-c0[0])?"  <-- A RING FLIP LEFT IGA2 BEHIND":"");}
 else say("; the VxD's flip counts: not read\n");
}

/* v1.3: (a) flips after more than 100 ms of drawing queued */
#define HEAVY_FRAMES 20
#define HEAVY_QUADS 150
static int heavy(int no,int vsync) {
 void *dev=0;PP pp;HRESULT hr;DWORD c0[8],c1[8],g0[3],g1[3];int have,have2,k,j,done=0,ok=1;LARGE_INTEGER f,a,b;TL q[4],w[4];
 say("pass %d: %s, %d alpha-blended full-screen quads a frame before the flip: ",no,vsync?"vsync":"no vsync",HEAVY_QUADS);
 memset(&pp,0,sizeof(pp));pp.bw=640;pp.bh=480;pp.bfmt=(deskfmt==23)?23:22;pp.bcount=1;pp.swap=2;pp.wnd=wnd;pp.windowed=0;pp.autoz=0;pp.interval=vsync?1UL:0x80000000UL;
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);
 if(hr||!dev){pp.bfmt=pp.bfmt==23?22:23;hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);}
 if(hr||!dev){say("CreateDevice failed, %08lX\n",(U32)hr);return 0;}
 q[0].x=0;q[0].y=0;q[1].x=640;q[1].y=0;q[2].x=0;q[2].y=480;q[3].x=640;q[3].y=480;
 for(k=0;k<4;k++){q[k].z=0.5f;q[k].w=1.0f;q[k].c=0x10808080UL;w[k]=q[k];w[k].c=0xFFFFFFFFUL;}
 have=counts(c0);have2=counts2(g0);
 QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);
 for(k=0;k<HEAVY_FRAMES;k++){MSG m;
  while(PeekMessage(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}
  if(GetAsyncKeyState(VK_ESCAPE)&0x8000){stop=1;break;}
  ((F0)VT(dev,DEV_BEGIN))(dev);
  ((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,1,0xFF000000UL,1.0f,0);
  ((F2)VT(dev,DEV_SETRS))(dev,137,0);((F2)VT(dev,DEV_SETRS))(dev,7,0);((F2)VT(dev,DEV_SETRS))(dev,22,1);
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F1)VT(dev,DEV_SETVS))(dev,0x44);
  ((F2)VT(dev,DEV_SETRS))(dev,27,1);((F2)VT(dev,DEV_SETRS))(dev,19,5);((F2)VT(dev,DEV_SETRS))(dev,20,6); /* ALPHABLENDENABLE, SRCALPHA, INVSRCALPHA */
  for(j=0;j<HEAVY_QUADS;j++)((FDPUP)VT(dev,DEV_DPUP))(dev,5,2,q,sizeof(TL));
  ((F2)VT(dev,DEV_SETRS))(dev,27,0);
  ((FDPUP)VT(dev,DEV_DPUP))(dev,5,2,w,sizeof(TL));
  ((F0)VT(dev,DEV_END))(dev);
  ((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);done++;}
 QueryPerformanceCounter(&b);
 if(have)have=counts(c1);if(have2)have2=counts2(g1);
 ((F0)VT(dev,D3D_REL))(dev);
 say("%d frames, %.1f ms each",done,done?(double)(b.QuadPart-a.QuadPart)*1000.0/(double)f.QuadPart/done:0.0);
 if(have){say("; ring flips %lu, declined %lu, waits that ran out %lu, CPU flips after ring flips %lu",c1[0]-c0[0],c1[1]-c0[1],c1[2]-c0[2],c1[3]-c0[3]);say_two(c0,c1);}
 if(have2)say("; waits past 100 ms with the engine moving %lu; ring flips %s",g1[0]-g0[0],g1[2]?"GIVEN UP for this boot":"still on");
 else say("; the .667 counts: not read (an older VxD)");
 if(have&&!c1[4]){ /* v1.7: ring flips off (ACCEL_3D_RINGFLIP 0): every flip by the CPU is right; none may have gone into the ring or run out */
  ok=c1[0]==c0[0]&&c1[2]==c0[2];say("\n   %s\n",ok?"RIGHT: ring flips off (ACCEL_3D_RINGFLIP 0, or the VX900: its flips are the CPU's), every flip by the CPU, none ran out":"WRONG: ring flips are off but a flip went into the ring or ran out");return ok;}
 if(have&&c1[5]!=0xFFFFFFFFUL&&(c1[5]&2)){ /* v1.9: IGA2 alone: the ring is not used */
  ok=c1[0]==c0[0]&&c1[2]==c0[2];say("\n   %s\n",ok?"RIGHT: IGA2 alone (a panel), every flip by the CPU, none ran out":"WRONG: IGA2 alone but a flip went into the ring or ran out");return ok;}
 if(have&&(c1[2]!=c0[2]||c1[0]-c0[0]<(DWORD)done))ok=0;
 if(have&&c1[5]!=0xFFFFFFFFUL&&(c1[5]&1)&&c1[6]-c0[6]!=c1[0]-c0[0])ok=0; /* v1.9: a mirrored IGA2 moves with every ring flip */
 if(have2&&g1[2])ok=0;
 say("\n   %s%s\n",!have?"the flip counts not read":ok?"RIGHT: every flip in the ring, none ran out":"WRONG: a flip wait ran out, flips left the ring, or a ring flip left IGA2 behind",have2&&ok&&g1[0]==g0[0]?" (no wait went past 100 ms: the engine was faster than this pass needs, so the case was not made)":"");
 return ok;
}
/* v1.3: (b) a windowed Present stretched by the driver, with a clip list */
static U32 pat(int x,int y) {return ((U32)(x&31)<<19)|((U32)(y&63)<<10)|((U32)((x>>5)&7)<<5)|((U32)((y>>6)&3)<<3);} /* survives 565: R5 G6 B5 */
static void pump(int n) {MSG m;int t;for(t=0;t<n;t++){while(PeekMessage(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}Sleep(10);}}
static DWORD (WINAPI *hal_q)(DWORD *,DWORD);
static void hal_open(void) {char dll[MAX_PATH];UINT n=GetSystemDirectoryA(dll,MAX_PATH-16);HMODULE m=0;if(n&&n<MAX_PATH-16){strcpy(dll+n,dll[n-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");m=LoadLibraryA(dll);}
 hal_q=m?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(m,"VX_D3D_QueryPhases"):0;}
static int hal_counts(DWORD *c) {static DWORD w[440];if(!hal_q||hal_q(w,440)<434)return 0;c[0]=w[431];c[1]=w[428];c[2]=w[432];c[3]=w[433];return 1;}
static void str_mode(DWORD m) {DWORD o[8];texdbg2(0xBC,0x44A0000UL|(m&3),o);}
static DWORD str_mode_now(void) {DWORD o[8];if(texdbg2(0xBC,0x446,o)&&(o[0]&0xFF)==0xBC)return o[2];return 1;}
/* v1.6: the device with the pattern in its 256 x 256 back buffer */
static void *str_dev(void) {
 void *dev=0,*bb=0;PP pp;HRESULT hr;LR lr;int x,y;
 SetWindowLong(wnd,GWL_STYLE,WS_POPUP|WS_VISIBLE);SetWindowPos(wnd,HWND_TOP,40,40,258,259,SWP_SHOWWINDOW);pump(10);
 memset(&pp,0,sizeof(pp));pp.bw=256;pp.bh=256;pp.bfmt=deskfmt==21?22:deskfmt;pp.bcount=1;pp.swap=1;pp.wnd=wnd;pp.windowed=1;pp.autoz=0;pp.flags=1; /* D3DSWAPEFFECT_DISCARD, LOCKABLE_BACKBUFFER */
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);
 if(hr||!dev){say("CreateDevice failed, %08lX\n",(U32)hr);return 0;}
 if(((HRESULT(__stdcall*)(void*,U32,U32,void**))VT(dev,DEV_BACKBUF))(dev,0,0,&bb)||!bb||((HRESULT(__stdcall*)(void*,LR*,void*,U32))VT(bb,SURF_LOCK))(bb,&lr,0,0)){say("the back buffer could not be locked\n");if(bb)((F0)VT(bb,D3D_REL))(bb);((F0)VT(dev,D3D_REL))(dev);return 0;}
 for(y=0;y<256;y++)for(x=0;x<256;x++){U32 c=pat(x,y);
  if(deskfmt==23)((unsigned short*)((char*)lr.bits+y*lr.pitch))[x]=(unsigned short)((((c>>19)&31)<<11)|(((c>>10)&63)<<5)|((c>>3)&31));
  else ((U32*)((char*)lr.bits+y*lr.pitch))[x]=c;}
 ((F0)VT(bb,SURF_UNLOCK))(bb);((F0)VT(bb,D3D_REL))(bb);
 return dev;
}
static void present(void *dev,int n) {int k;for(k=0;k<n;k++)((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);}
/* v1.6: the window at cw x ch (with a small window over it if covered), 3 Presents, its pixels into buf; wr the cover's place */
static U32 grab_e[512*512],grab_c[512*512];
static HWND str_cover(int cw,int ch) {HWND c;SetWindowPos(wnd,HWND_TOP,40,40,cw,ch,SWP_SHOWWINDOW);c=CreateWindow("VXNOSYNC","",WS_POPUP|WS_VISIBLE,40+cw/3,40+ch/3,cw/4,ch/5,wnd,0,GetModuleHandle(0),0);pump(8);return c;}
static int str_grab(void *dev,int cw,int ch,int covered,U32 *buf,RECT *wr) {
 HWND cover=0;HDC wdc,mdc;HBITMAP bm,old;BITMAPINFO bi;U32 *px=0;int ok=0;
 if(cw>512||ch>512)return 0;
 if(covered)cover=str_cover(cw,ch);else{SetWindowPos(wnd,HWND_TOP,40,40,cw,ch,SWP_SHOWWINDOW);pump(8);}
 present(dev,3);Sleep(60);
 wdc=GetDC(wnd);mdc=CreateCompatibleDC(wdc);memset(&bi,0,sizeof(bi));bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=cw;bi.bmiHeader.biHeight=-ch;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;
 bm=CreateDIBSection(wdc,&bi,DIB_RGB_COLORS,(void**)&px,0,0);
 if(bm&&px){old=(HBITMAP)SelectObject(mdc,bm);BitBlt(mdc,0,0,cw,ch,wdc,0,0,SRCCOPY);GdiFlush();memcpy(buf,px,(size_t)cw*ch*4);SelectObject(mdc,old);ok=1;}
 if(cover){GetWindowRect(cover,wr);OffsetRect(wr,-40,-40);}else SetRectEmpty(wr);
 if(bm)DeleteObject(bm);DeleteDC(mdc);ReleaseDC(wnd,wdc);
 if(cover)DestroyWindow(cover);
 return ok;
}
/* v1.6: the pixels outside the cover and its edge: ne those whose source pixel is not the one stretch of the whole window's (a clipped
 * Present comes as one Blt for each visible rectangle, each with its own whole-pixel source rectangle: the board, .670, 4 a Present, so
 * only the uncovered sizes are held to it), nc those that differ from the CPU's picture (every size) */
static void str_cmp(int cw,int ch,const RECT *wr,DWORD *ne,DWORD *nc,DWORD *nchk) {int x,y;
 *ne=*nc=*nchk=0;
 for(y=0;y<ch;y++)for(x=0;x<cw;x++){U32 c=grab_e[y*cw+x],r=(c>>16)&255,g=(c>>8)&255,b=c&255;int sx,sy;
  if(!IsRectEmpty(wr)&&x>=wr->left-1&&x<wr->right+1&&y>=wr->top-1&&y<wr->bottom+1)continue;
  sx=(int)((((b>>5)&7)<<5)|(r>>3));sy=(int)((((b>>3)&3)<<6)|(g>>2));(*nchk)++;
  if(sx!=(int)(((2L*x+1)*256L)/(2L*cw))||sy!=(int)(((2L*y+1)*256L)/(2L*ch)))(*ne)++;
  if((c^grab_c[y*cw+x])&0xF8FCF8UL)(*nc)++;}
}
/* v1.6: one size, the engine (ACCEL_STRETCH3D 1) and the CPU (0), with the HAL's counts for each; 1 if right */
static int str_size(void *dev,int cw,int ch,int covered,int detail) {
 DWORD c0[5],c1[5],c2[4],c3[4],ne,nc,nk;RECT wr,wr2;int h1,h2,ge,gc,ok;
 str_mode(1);h1=hal_counts(c0);ge=str_grab(dev,cw,ch,covered,grab_e,&wr);if(h1)h1=hal_counts(c1);
 str_mode(0);h2=hal_counts(c2);gc=str_grab(dev,cw,ch,covered,grab_c,&wr2);if(h2)h2=hal_counts(c3);
 if(!ge||!gc){say("   %3d x %3d%s: the window could not be read\n",cw,ch,covered?", covered":"");return 0;}
 str_cmp(cw,ch,&wr,&ne,&nc,&nk);ok=!nc&&(covered||!ne);
 say("   %3d x %3d%s: against the CPU's picture %lu of %lu pixels differ; against one stretch of the whole window %lu%s\n",cw,ch,covered?", covered":"         ",nc,nk,ne,covered?" (not held to it)":"");
 if(detail&&h1&&h2)say("      for 3 Presents: the engine: %lu stretches, %lu with a clip list, %lu rectangles, %lu CPU blits; the CPU: %lu stretches, %lu CPU blits\n",c1[0]-c0[0],c1[1]-c0[1],c1[2]-c0[2],c1[3]-c0[3],c3[0]-c2[0],c3[3]-c2[3]);
 return ok;
}
static int stretch_pass(int no) {
 void *dev;int ok;HWND cover;LARGE_INTEGER f,a,b;DWORD was=str_mode_now();
 say("pass %d: a windowed Present, 256 x 256 stretched into 258 x 259, part of the window covered (a clip list): ",no);
 if(deskfmt!=22&&deskfmt!=21&&deskfmt!=23){say("skipped: the desktop is not 32-bit or 16-bit 565 (format %lu)\n",deskfmt);return 1;}
 hal_open();dev=str_dev();if(!dev)return 0;
 str_mode(1);cover=str_cover(258,259);
 QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);present(dev,100);QueryPerformanceCounter(&b);
 Sleep(50);DestroyWindow(cover);
 say("%.2f ms a Present (.666: the CPU, about 8.5 ms; .667: the 3D engine, well under 1 ms)\n",(double)(b.QuadPart-a.QuadPart)*1000.0/(double)f.QuadPart/100.0);
 ok=str_size(dev,258,259,1,0);str_mode(was);
 ((F0)VT(dev,D3D_REL))(dev);
 say("   %s\n",ok?"RIGHT: the engine's picture is the CPU's":"WRONG: the engine's picture differs from the CPU's");
 SetWindowPos(wnd,HWND_TOP,0,0,640,480,SWP_SHOWWINDOW);
 return ok;
}
/* v1.6: STRETCH: six sizes, two covered, each drawn by the engine and by the CPU */
static int stretch_only(void) {
 static const int sz[6][3]={{258,259,0},{258,259,1},{320,240,0},{200,150,1},{512,512,0},{384,288,0}};
 void *dev;int x,ok=1;DWORD was=str_mode_now();
 say("a 256 x 256 back buffer stretched into a window (a windowed Present) by the engine and by the CPU (ACCEL_STRETCH3D set live, put back after), each pixel against the CPU's picture; the uncovered sizes also against one stretch of the whole window\n");
 if(deskfmt!=22&&deskfmt!=21&&deskfmt!=23){say("not run: the desktop is not 32-bit or 16-bit 565 (format %lu)\n",deskfmt);return 0;}
 hal_open();dev=str_dev();if(!dev)return 0;
 for(x=0;x<6&&!stop;x++){if(GetAsyncKeyState(VK_ESCAPE)&0x8000){stop=1;break;}if(!str_size(dev,sz[x][0],sz[x][1],sz[x][2],1))ok=0;}
 str_mode(was);
 ((F0)VT(dev,D3D_REL))(dev);
 say("%s\n",stop?"stopped (Esc)":ok?"RIGHT: every size the CPU's picture":"WRONG: see the sizes above (send this log)");
 return ok;
}
int main(int argc,char **argv) {
 HMODULE d8;void *(__stdcall *create)(U32);MODE mode;WNDCLASS wc;char path[MAX_PATH],*s;
 {int k;for(k=1;k<argc;k++)if(!lstrcmpi(argv[k],"ASK"))g_ask=1;} /* v2.2 */
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXNOSYNC.LOG");logf=fopen(path,"w");
 if(argc>1&&!lstrcmpi(argv[1],"STRETCH")){say("VXNOSYNC v2.2 (driver .698) STRETCH: ");}
 else if(argc>1&&!lstrcmpi(argv[1],"TEAR")){say("VXNOSYNC v2.5 (driver .733) TEAR: the tear flip alone (pass 9)\n");}
 else say("VXNOSYNC v2.5 (driver .733): flips with and without vsync, with and without a lock of the back buffer (passes 1-5: the screen must stay white); flips after more than 100 ms of drawing (passes 6-7); a stretched windowed Present with a clip list (pass 8); the tear flip (pass 9; its questions only with ASK).\n");
 vxd=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);
 d8=LoadLibrary("d3d8.dll");create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 if(!create){say("d3d8.dll / Direct3DCreate8 not found (DirectX 8 or later is needed)\n");return 1;}
 d3d=create(220);if(!d3d)d3d=create(120);
 if(!d3d){say("Direct3DCreate8 failed\n");return 1;}
 memset(&mode,0,sizeof(mode));((F2P)VT(d3d,D3D_MODE))(d3d,0,&mode);deskfmt=mode.format;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXNOSYNC";RegisterClass(&wc);
 wnd=CreateWindow("VXNOSYNC","VXNOSYNC",WS_POPUP|WS_VISIBLE,0,0,640,480,0,0,wc.hInstance,0);
 if(argc>1&&!lstrcmpi(argv[1],"STRETCH")){stretch_only();((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);if(logf)fclose(logf);return 0;}
 if(argc>1&&!lstrcmpi(argv[1],"TEAR")){int r=tear_pass(9);say("pass 9: %s\n",stop?"stopped (Esc)":r?"RIGHT":"WRONG (see above)");((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);if(logf)fclose(logf);return 0;} /* v2.0: VXNOSYNC TEAR, pass 9 alone (TEAR.BAT) */
 pass(1,0,1);if(!stop)pass(2,0,0);if(!stop)pass(3,1,0);if(!stop)pass(4,1,1);if(!stop)pass(5,0,1); /* v1.3 */{int r6=1,r7=1,r8=1,r9=1;if(!stop)r6=heavy(6,0);if(!stop)r7=heavy(7,1);if(!stop)r8=stretch_pass(8);if(!stop)r9=tear_pass(9);say("passes 6-9: %s\n",stop?"stopped (Esc)":r6&&r7&&r8&&r9?"RIGHT":"WRONG (see above)");} /* v1.2: v1.0's frame first, as in v1.0 (the first frames after the start), and once more at the end */
 ((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);
 say("Judge by eye: all white = right; black flashes or bands = a frame was shown while it was drawn. Send this log.\n");
 if(logf)fclose(logf);
 return 0;
}
