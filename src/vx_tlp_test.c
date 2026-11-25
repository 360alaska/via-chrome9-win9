/* VXTLP v1.51 (driver .737: 12c/12d, fog on 8 big triangles 2 to 80 deep, the software device's vertex fog against the driver's per-pixel
 * fog: the difference and both pictures, not judged). VXTLP v1.50 (driver .734: 10d/10e, untransformed vertices with three coordinate sets through the T&L, against the software device).
 * VXTLP v1.49 (driver .733: 12. depth vertex fog under a perspective projection, linear and EXP2, against the software device:
 * ACCEL_D3D_TFOG's fog from W). VXTLP v1.48 (driver .731: 11. WRAP0 the short way 0.75 to 0.0 against the long way (v1.47's case could not tell them apart); 10c
 * three distinct coordinate sets against the software device at 2%). VXTLP v1.47 (driver .730: 11. D3DRS_WRAP0 U and V, the short way across 1.0 against the long way). VXTLP v1.46 (driver .729: the header names the build). VXTLP v1.45 (driver .727: FILL, the texture fill rate with one and two units). VXTLP v1.44 (driver .726: 6W counts a frame not read back as wrong). VXTLP v1.43 (driver .724: 6P and 6Q judged against references drawn here; 6W q other than 1, folded or projected). VXTLP v1.42 (driver .723: 6W, the water against a reference drawn here, variants). VXTLP v1.41 (driver .721: 6Q the guard band on and off only, the floors judged). VXTLP v1.40 (driver .720: 6Q, the clipped projected floor and the water with strips and the guard band switched). VXTLP v1.39 (driver .719: 6P NA, a clipped blended floor with a strong vertex alpha gradient against the software device's). VXTLP v1.38 (driver .718: 6P, deep floors and a floor across the near plane, 2 huge triangles against a grid; 6N, a Dragothic-like water scene against the software device's, 24 frames; 6M in one round, .717's rhw switch gone; PROJ runs 6M and 6N). VXTLP v1.37 (driver .717: 6M, projected texture coordinates on large triangles against the software device's, the driver's rhw multiply off and on (see test_proj); VXTLP PROJ runs 6M alone). VXTLP v1.36 (driver .704: VP, every scene through a viewport at (40, 40), the guard band across a clip edge that is not 0). VXTLP v1.35 (driver .699: 6l writes its depth too, ZWRITEENABLE). VXTLP v1.34 (driver .696: 6l depth-tested, so its CULL_NONE frame hides the back faces; T682-T694 failed only because ZENABLE was off). VXTLP v1.33 (driver .682: 6l checked against its own frame with CULL_NONE; v1.32's 6k comparison was wrong, the views differ). VXTLP v1.32 (driver .680: 6l's picture against 6k's, the engine's back-face cull checked without the software device). VXTLP v1.31 (driver .677: 6l, 6k with back faces culled (D3DCULL_CCW) and the screen's sides crossing the outer tori: the driver's culling before lighting, .676, on a picture compared with the software device's; GRID 6L alone). VXTLP v1.30 (driver .668: 10, what the review of .667 found: a state block past 1,024 bytes of T&L state, 65 light indices with the first
 * switched on again, transformed vertices with three coordinate sets). VXTLP v1.29 (driver .663: 9, what the review of .661 found in the driver's T&L, each picture against the software device's: flat-shaded
 * triangles whose first vertex is clipped away, fog with flat shading, a texture matrix that reads a third coordinate, a cube map read by
 * three coordinates). VXTLP v1.28 (driver .658: a fault written to the log with its place, and the program ended, no dialog). VXTLP v1.27 (driver .657: the window has the arrow pointer, and its messages are read in the warm-up frames too). VXTLP v1.26 (driver .655, diagnosis: the board's 6k as a program's first scene gave NaN frame times and hung (and the software
 * device's 6e after it drew a garbage texture handle); .653's VXTLP (v1.23) never did; v1.24 brought the C library's sin and cos (and its
 * start-up's FPU checks) with the tori. Here: no library trigonometry (fsincos of our own, a series), and the x87 state checked after
 * every draw of the tori and the GTA3-like frame and after every frame: the first time a register is left in use or the control word
 * changes, where (the scene, the frame, the call) is written to the log; the state is then put back (FNINIT, the control word) so the run
 * goes on). VXTLP v1.25 (driver .655: shorter: GRID 10 s a device (HW or SW alone: 20 s, 15 s), 10 warm-up frames; outside GRID 5 warm-up and 5
 * timed frames (the picture checks); GRID leaves 6c out (the driver about 3 times the runtime's speed there for many builds). ATTACH: 10.
 * a second program started while a device draws: board, .654: VXD3DMON started after VXTLP's software device was drawing the GTA3-like
 * frame, one of its textures stopped resolving at once (a quarter of the primitives left out), and VXTLP hung after its last frame with the
 * machine. Here: the frame drawn, VXD3DMON.EXE 8 started beside it, the frame drawn on for 12 s, the picture compared, the device released;
 * the software device, then the hardware one). VXTLP v1.24 (driver .654: 6j and 6k, after 3DMark 2001's High Polygon Count: 30 untextured, unfogged tori of 5,000 triangles a
 * frame (150,000 triangles), 6j lit by one directional light, 6k by one directional and seven point lights with specular lighting; timed
 * on both devices and compared; GRID 6J and GRID 6K time one alone). VXTLP v1.23 (driver .653: 6i, the grid scene lit by four spot lights, two point lights and a directional one, stage 0 reading
 * reflection vectors through a texture matrix, stage 1 camera-space positions projected through another, every third grid's stage 1 reading
 * a set the vertex lacks: timed on both devices and compared (3DMark 2001's draws that .652 finished one vertex at a time); GRID 6I
 * times it alone). VXTLP v1.22 (driver .651: 6h, the grid scene lit by eight lights, four point and four directional, with specular lighting (power 20,
 * LOCALVIEWER), timed on both devices and compared: specular lighting in groups of four, 3DMark 2001's High Polygon Count). VXTLP v1.21 (driver .650: 6f and 6g, the grid scene with the second stage on for every grid and off for every grid (6d, on and
 * off between grids, was not the software device's picture on the second bench); each scene's pictures kept as VXTn_A/B.BMP when a
 * pixel differs; GRID 6E times the GTA3-like frame alone, for VXD3DMON's profile of each device). VXTLP v1.20 (driver .649: 9. depth surfaces made, bound and released while the device lives, ValidateDevice and a draw after
 * each: 3DMark 2001's "failed to validate material"). VXTLP v1.19 (driver .648: 6e. a GTA3-like frame, the same every time, timed on both devices and compared: an A/B that does not
 * depend on replaying a scene in the game). VXTLP v1.18 (driver .648: 6d. the grid scene with a second stage turned on and off between grids, against the software device's
 * picture: the record size changing between draws with the set-up kept). VXTLP v1.17 (driver .647: 6b. the grid scene lit by a point light and 6c. the same triangles as 720 small draws with a world matrix
 * before each, each timed on both devices and compared with the software device's picture: the point lights in groups of four and the
 * set-up kept between draws). VXTLP v1.16 (driver .646: 8., a spot light's cone, specular highlights, lights at indices 17 and 1000 and a third stage reading set 0,
 * each against the software device's picture: .645 and before lit none of them as Direct3D does). VXTLP v1.15 (driver .640: 7., untransformed points and lines and draws too large for the scratch buffer (a list of 9,000 triangles,
 * indices across 30,000 vertices), each against the software device's picture: .639 and before answered DD_OK and drew none of them).
 * VXTLP v1.14 (driver .553: GRID, HW and SW read from the arguments only; the CSVs beside the program). VXTLP v1.13 (.552: 30 warm-up frames, each
 * frame's time kept, median and 95th percentile by nearest rank, VXG_SW.CSV / VXG_HW.CSV; VXTLP GRID SW: the runtime's T&L with the driver's packing alone).
 * VXTLP v1.12 (driver .543: the same parts; 6. checks .543's clipping at the side planes (the grids off the screen's edges) against
 * the runtime's picture). VXTLP v1.11 (driver .542: VXTLP GRID HW: the driver's T&L alone, 60 s (for VXD3DMON's line samples). VXTLP v1.10 (driver .542: VXTLP GRID: part 6 only, each device 30 s, for VXD3DMON's sampler to profile the runtime's T&L path and
 * the driver's on the same scene). VXTLP v1.9 (driver .542: 6. again, the driver's T&L now SSE when the CPU has SSE and SSE2 (printed from CPUID: the driver checks the
 * same bits); the same scene, the same comparison and times). VXTLP v1.8 (driver .541: 6., a scene of the kind GTA3 draws: 30 lit, fogged, textured 24x24 grids a frame (1,152 triangles each, an
 * indexed list), some across the near plane and some off the screen, drawn by the runtime (software vertex processing) and by the driver's
 * T&L (hardware): the pictures compared (at most 1% of the lit pixels differing) and each device's time a frame (20 frames): the fused
 * path's speed on the board, the same scene every build). VXTLP v1.7 (driver .540: 5., what .540 fixed, on both devices: 5a. an indexed draw the HAL split wrongly (a triangle wider than an
 * indexed packet holds, then one with a bad vertex: .539 sent an empty packet, the VxD refused it and the device was lost, GTA3's freeze),
 * the lit square drawn after it; 5b. a lit square turned 90 degrees by the world matrix, lit by a light along -y (.539 turned the
 * normals the wrong way: black); 5c. two texture stages of untransformed vertices (.539 named the wrong vertex format: not drawn)).
 * VXTLP v1.6 (driver .538: 4c., a vertex buffer appended to under hardware vertex processing (locked again past the vertices already
 * drawn, NOOVERWRITE, as GTA3 does), pretransformed and untransformed: both halves drawn). VXTLP v1.5 (driver .537: 4b., vertex buffers created and released under hardware vertex processing, then the lit square drawn again,
 * from a vertex buffer and from memory: .536 lost the device when GTA3 created its vertex buffers, and every later draw failed). VXTLP v1.4 (driver .536: the setting read from the VxD (query 0xFFFFFE50, the answer's word 2 bit 21, as the HAL reads it), not from
 * QueryPhases 270: the HAL sets that word when DirectDraw first builds it after a restart, so a first program read 0 with the key on). VXTLP v1.3 (driver .536: ACCEL_D3D_TNL, the driver's own T&L (was .534's probe, ACCEL_D3D_TLPROBE): in 4. the lit square drawn on
 * the hardware-processing device too, against the software device's lit square (pixels differing, at most 1% of the lit ones); the counts
 * of the driver's T&L (QueryPhases 290-301); the driver's build printed). VXTLP v1.2 (driver .535: 3b., 1,089 textures: more texture handles than .534's 1,024 surface hints, one draw each). VXTLP v1.1 (driver .534): v1.0 called SetMaterial with a stage argument it does not take (this, D3DMATERIAL8 *), so Direct3D 8
 * read the material from address 0 and the program faulted in test 3. v1.0 (driver .534): the T&L probe (registry ACCEL_D3D_TNL, a feature under test, default 0; vx_d3d_core.c). With it on the HAL
 * offers hardware transform and lighting only so that a run can show which games choose it; nothing is transformed: the T&L commands are
 * read past and counted, and the draws of untransformed vertices are left out and counted. This tool checks both settings (it reads which
 * one the driver runs with from QueryPhases 270; set the key and restart Windows to change it):
 * 1. Direct3D 7: IDirect3D7::EnumDevices offers the T&L HAL device (IID_IDirect3DTnLHalDevice) with the probe on, not with it off.
 * 2. Direct3D 8: the HAL's caps show D3DDEVCAPS_HWTRANSFORMANDLIGHT and 8 lights with the probe on, not with it off.
 * 3. Direct3D 8, software vertex processing (both settings): an untransformed, lit square is drawn (the runtime transforms it, the probe
 *    leaves it alone) and nothing is counted; six shots of pretransformed quads, the same picture.
 * 4. Direct3D 8, hardware vertex processing: with the probe on the device is made; the lit square's draw arrives untransformed and is left
 *    out (nothing drawn, counted: calls, draws, SETTRANSFORM, SETMATERIAL, SETLIGHT with its data, CREATELIGHT); six shots of the
 *    pretransformed quads on that device, the same picture as on the software device. With the probe off the device is refused.
 * Results to the screen and VXTLP.LOG beside the program; VXTLP_A.BMP / VXTLP_B.BMP when two shots differ. Run it on the desktop at 16 or
 * 32 bpp; it opens a 256x256 window for a few seconds. VXD3DMON must not be running. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
/* v1.14 (.553): the words after the program's name. GRID, HW and SW were searched for in the whole command line, the program's path
 * included: a folder with those letters in its name chose the mode. */
static const char *args(void) {const char *c=GetCommandLineA();
 if(*c=='"'){c++;while(*c&&*c!='"')c++;if(*c)c++;}else while(*c&&*c!=' '&&*c!='\t')c++;
 return c;}
/* v1.14: the frame-time CSVs go beside the program, as the log does (they went to the current directory) */
static FILE *beside(const char *name) {char path[MAX_PATH+16],*q;
 if(!GetModuleFileNameA(0,path,MAX_PATH))return fopen(name,"w");
 q=strrchr(path,'\\');if(!q)return fopen(name,"w");
 strcpy(q+1,name);return fopen(path,"w");}
#include <ddraw.h>
#include <d3d.h>

typedef unsigned long U32;
#define VT(o,i) (((void**)(*(void***)(o)))[i])
typedef HRESULT (__stdcall *F0)(void*);
typedef HRESULT (__stdcall *F1)(void*,U32);
typedef HRESULT (__stdcall *F2)(void*,U32,U32);
typedef HRESULT (__stdcall *F2P)(void*,U32,void*);
typedef HRESULT (__stdcall *F3P)(void*,U32,U32,void*);
enum {D3D_REL=2,D3D_MODE=8,D3D_CAPS=13,D3D_CREATEDEV=15};
enum {DEV_SETVP=40,DEV_PRESENT=15,DEV_BACKBUF=16,DEV_BEGIN=34,DEV_END=35,DEV_CLEAR=36,DEV_SETXF=37,DEV_SETMAT=42,DEV_SETLIGHT=44,DEV_LIGHTON=46,DEV_SETRS=50,
      DEV_SETTEX=61,DEV_SETTSS=63,DEV_CREATEVB=23,DEV_DP=70,DEV_DPUP=72,DEV_DIPUP=73,DEV_SETVS=76,DEV_STREAM=83,DEV_CREATEIB=24,DEV_DIP=71,DEV_SETIB=85,DEV_CREATEZ=26,DEV_SETRT=31,DEV_GETRT=32,DEV_GETZ=33,DEV_VALIDATE=64};
enum {VB_LOCK=11,VB_UNLOCK=12}; /* v1.5: IDirect3DVertexBuffer8 */
enum {SURF_LOCK=9,SURF_UNLOCK=10};
typedef struct {U32 w,h,refresh,format;} MODE;
typedef struct {U32 bw,bh,bfmt,bcount,ms,swap;HWND wnd;U32 windowed,autoz,zfmt,flags,rate,interval;} PP;
typedef struct {long pitch;void *bits;} LR;
#define WIN 256
static const GUID iid_dd7={0x15E65EC0UL,0x3B9C,0x11D2,{0xB9,0x2F,0,0x60,0x97,0x97,0xEA,0x5B}};
static const GUID iid_d3d7={0xF5049E77UL,0x4861,0x11D2,{0xA4,0x07,0x00,0xA0,0xC9,0x06,0x29,0xA8}};
static const GUID iid_tnlhal={0xF5049E78UL,0x4861,0x11D2,{0xA4,0x07,0x00,0xA0,0xC9,0x06,0x29,0xA8}};

static FILE *logf;static int fails,probe;
static void say(const char *f,...) {static char b[2048];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);if(logf){fputs(b,logf);fflush(logf);}}
static void wrong(const char *s) {say("   WRONG: %s\n",s);fails++;}
static DWORD (WINAPI *hal_phases)(DWORD *,DWORD);
static DWORD qa[302];
static int q_read(DWORD *w) {return hal_phases&&hal_phases(w,302)==302&&w[0]==0x431;}

/* 1. Direct3D 7 */
static int tnl_seen,hal_seen;static U32 tnl_caps;
static HRESULT WINAPI dev_cb(LPSTR desc,LPSTR name,LPD3DDEVICEDESC7 d,LPVOID ctx) {
 (void)ctx;say("   device \"%s\" (%s): DevCaps %08lX%s\n",name?name:"",desc?desc:"",(U32)d->dwDevCaps,(d->dwDevCaps&0x10000UL)?" (HWTRANSFORMANDLIGHT)":"");
 if(!memcmp(&d->deviceGUID,&iid_tnlhal,sizeof(GUID))){tnl_seen=1;tnl_caps=d->dwDevCaps;}
 if(d->dwDevCaps&D3DDEVCAPS_HWRASTERIZATION)hal_seen=1;
 return D3DENUMRET_OK;
}
static void test_d3d7(void) {
 HMODULE ddl=LoadLibrary("ddraw.dll");HRESULT (WINAPI *create)(GUID *,void **,REFIID,IUnknown *);IDirectDraw7 *dd=0;IDirect3D7 *d3=0;
 say("1. Direct3D 7: the devices IDirect3D7::EnumDevices offers\n");
 create=ddl?(HRESULT (WINAPI *)(GUID *,void **,REFIID,IUnknown *))GetProcAddress(ddl,"DirectDrawCreateEx"):0;
 if(!create||create(0,(void**)&dd,&iid_dd7,0)||!dd){wrong("DirectDrawCreateEx failed");return;}
 if(IDirectDraw7_QueryInterface(dd,&iid_d3d7,(void**)&d3)||!d3){wrong("no IDirect3D7");IDirectDraw7_Release(dd);return;}
 IDirect3D7_EnumDevices(d3,dev_cb,0);
 say("   the T&L HAL device: %s (expected %s)\n",tnl_seen?"offered":"not offered",probe?"offered":"not offered");
 if(!hal_seen)wrong("no hardware device offered");
 if(tnl_seen!=probe)wrong(probe?"the probe is on but the T&L HAL device is not offered":"the T&L HAL device is offered with the probe off");
 if(tnl_seen&&!(tnl_caps&0x10000UL))wrong("the T&L HAL device without D3DDEVCAPS_HWTRANSFORMANDLIGHT");
 IDirect3D7_Release(d3);IDirectDraw7_Release(dd);
}

/* 2-4. Direct3D 8 */
static void *d3d,*dev;static U32 bbfmt;static HWND wnd;
typedef struct {float x,y,z,w;U32 c;float u,v;} TL; /* FVF 0x144 */
typedef struct {float x,y,z,nx,ny,nz;U32 c;} UV;  /* FVF 0x52: XYZ, NORMAL, DIFFUSE */
/* v1.36 (driver .704): VXTLP VP: every scene through a viewport at (40, 40), 176 x 176 (the whole target cleared first), so the guard
 * band's vertices lie on both sides of a clip edge that is not 0: .698-.703's VxD refused those between 0 and the edge (the packet left
 * out: the driver's picture misses triangles against the software device's) */
static int g_vp,g_proj; /* v1.37: PROJ, 6M alone */
static void viewport(U32 x,U32 y,U32 w,U32 h) {U32 v[6];float mz=0.0f,xz=1.0f;v[0]=x;v[1]=y;v[2]=w;v[3]=h;memcpy(&v[4],&mz,4);memcpy(&v[5],&xz,4);((HRESULT(__stdcall*)(void*,void*))VT(dev,DEV_SETVP))(dev,v);}
static HRESULT clear(void) {HRESULT r;
 if(g_vp)viewport(0,0,WIN,WIN);
 r=((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,3,0xFF000000UL,1.0f,0);
 if(g_vp)viewport(40,40,WIN-80,WIN-80);
 return r;}
static void rs(U32 s,U32 v) {((F2)VT(dev,DEV_SETRS))(dev,s,v);}
static void tss(U32 st,U32 s,U32 v) {((F3P)VT(dev,DEV_SETTSS))(dev,st,s,(void*)v);}
static unsigned short fpu_env(int w); /* v1.26: w 0 the control word, 8 the tag word (FNSTENV's 32-bit image; FLDENV puts it back) */
#pragma aux fpu_env = "sub esp,28" "fnstenv [esp]" "mov ax,[esp+ecx]" "fldenv [esp]" "add esp,28" parm [ecx] value [ax] modify exact [ax];
static void fpu_reset(unsigned short cw);
#pragma aux fpu_reset = "fninit" "push eax" "fldcw [esp]" "pop eax" parm [eax] modify exact [];
static unsigned short f_cw0;static int f_bad,f_frame;static const char *f_scene="",*f_where="start";
/* v1.28 (board, .657: VXTLP GRID SW 6E stopped in D3D8.DLL with an invalid page fault in its first frames): a fault is written to the log
 * (the code, the address, the module it is in and its offset there, the registers, the scene, frame and last checked place) and the
 * program ends with code 3, so a batch goes on without a dialog to close */
static LONG WINAPI crash_log(EXCEPTION_POINTERS *e) {
 const EXCEPTION_RECORD *r=e->ExceptionRecord;const CONTEXT *c=e->ContextRecord;DWORD a=(DWORD)r->ExceptionAddress;MEMORY_BASIC_INFORMATION mb;char mod[MAX_PATH];
 mod[0]=0;memset(&mb,0,sizeof(mb));if(VirtualQuery((void*)a,&mb,sizeof(mb))&&mb.AllocationBase)GetModuleFileNameA((HMODULE)mb.AllocationBase,mod,sizeof(mod));
 say("CRASH: exception %08lX at %08lX in %s (base %08lX, offset %08lX)%s%08lX; scene %s, frame %d, after %s\n",r->ExceptionCode,a,mod[0]?mod:"no module",(DWORD)mb.AllocationBase,a-(DWORD)mb.AllocationBase,
  r->ExceptionCode==0xC0000005UL?(r->ExceptionInformation[0]?"; writing ":"; reading "):"; ",r->ExceptionCode==0xC0000005UL?(DWORD)r->ExceptionInformation[1]:0UL,f_scene,f_frame,f_where);
 say("   EAX %08lX EBX %08lX ECX %08lX EDX %08lX ESI %08lX EDI %08lX EBP %08lX ESP %08lX\n",c->Eax,c->Ebx,c->Ecx,c->Edx,c->Esi,c->Edi,c->Ebp,c->Esp);
 {const DWORD *sp=(const DWORD*)c->Esp;int k;say("   stack:");for(k=0;k<24;k++){if(IsBadReadPtr(sp+k,4))break;say(" %08lX",sp[k]);}say("\n");}
 {HMODULE d8=GetModuleHandleA("d3d8.dll"),hal=GetModuleHandleA("VXDDRAW.DLL");say("   d3d8.dll at %08lX, VXDDRAW.DLL at %08lX\nFAILED: the program faulted\n",(DWORD)d8,(DWORD)hal);}
 if(logf)fclose(logf);logf=0;ExitProcess(3);return EXCEPTION_EXECUTE_HANDLER;
}
static void fchk(const char *where) {unsigned short tag=fpu_env(8),cw=fpu_env(0);f_where=where;
 if(tag!=0xFFFFU||(f_cw0&&cw!=f_cw0)){f_bad++;if(f_bad<=8)say("   x87: after %s (scene %s, frame %d): tag word %04X, control word %04X (was %04X); put back\n",where,f_scene,f_frame,tag,cw,f_cw0);
  fpu_reset(f_cw0?f_cw0:cw);}}
static void present(void) {((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);}
static int read_bb(U32 *out) {
 void *bb=0;LR lr;int ok=0;U32 x,y;
 if(((HRESULT(__stdcall*)(void*,U32,U32,void**))VT(dev,DEV_BACKBUF))(dev,0,0,&bb)||!bb)return 0;
 if(!((HRESULT(__stdcall*)(void*,LR*,void*,U32))VT(bb,SURF_LOCK))(bb,&lr,0,0x10)){
  for(y=0;y<WIN;y++){unsigned char *row=(unsigned char*)lr.bits+y*lr.pitch;
   for(x=0;x<WIN;x++){if(bbfmt==23){unsigned short p=((unsigned short*)row)[x];out[y*WIN+x]=((U32)(p>>11)<<19)|((U32)((p>>5)&63)<<10)|((U32)(p&31)<<3);}else out[y*WIN+x]=((U32*)row)[x]&0xFFFFFFUL;}}
  ((F0)VT(bb,SURF_UNLOCK))(bb);ok=1;}
 ((F0)VT(bb,D3D_REL))(bb);return ok;
}
static int differs(U32 a,U32 b) {int k;for(k=0;k<24;k+=8){int d=(int)((a>>k)&255)-(int)((b>>k)&255);if(d<-8||d>8)return 1;}return 0;}
static int lit(const U32 *p) {int n=0,k;for(k=0;k<WIN*WIN;k++)if(p[k])n++;return n;}
static int ndiff(const U32 *a,const U32 *b) {int n=0,k;for(k=0;k<WIN*WIN;k++)if(differs(a[k],b[k]))n++;return n;}
static U32 ref[WIN*WIN],cur[WIN*WIN],sw_ref[WIN*WIN];static int saved,have_sw;
static void bmp(const char *name,const U32 *p) {
 char path[MAX_PATH],*q;FILE *f;U32 y,x;unsigned char h[54];
 GetModuleFileName(0,path,sizeof(path));q=strrchr(path,'\\');if(q)strcpy(q+1,name);f=fopen(path,"wb");if(!f)return;
 memset(h,0,54);h[0]='B';h[1]='M';*(U32*)(h+2)=54+WIN*WIN*3;*(U32*)(h+10)=54;*(U32*)(h+14)=40;*(U32*)(h+18)=WIN;*(U32*)(h+22)=WIN;h[26]=1;h[28]=24;*(U32*)(h+34)=WIN*WIN*3;
 fwrite(h,1,54,f);for(y=WIN;y-->0;)for(x=0;x<WIN;x++){U32 c=p[y*WIN+x];unsigned char b3[3];b3[0]=(unsigned char)c;b3[1]=(unsigned char)(c>>8);b3[2]=(unsigned char)(c>>16);fwrite(b3,1,3,f);}
 fclose(f);
}
static void base_states(void) {rs(7,0);rs(14,0);rs(27,0);rs(15,0);rs(22,1);rs(8,3);rs(9,2);rs(29,0);rs(28,0);rs(168,15);
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F2P)VT(dev,DEV_SETTEX))(dev,1,0);tss(0,1,2);tss(0,2,0);tss(0,4,2);tss(0,5,0);tss(1,1,1);tss(1,4,1);}
static void draw_tl(void) { /* eight gradient quads, pretransformed */
 TL v[48];int k,j,n=0;static const float qx[6]={0,48,0,48,48,0},qy[6]={0,0,48,0,48,48};static const U32 c0[4]={0xFFFF2020UL,0xFF20FF20UL,0xFF2020FFUL,0xFFFFFF20UL};
 for(k=0;k<8;k++){float x=12.0f+(k&3)*60.0f,y=40.0f+(k>>2)*90.0f;for(j=0;j<6;j++){TL *t=&v[n++];t->x=x+qx[j];t->y=y+qy[j];t->z=0.5f;t->w=1.0f;t->c=c0[(k+j)&3];t->u=0;t->v=0;}}
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();rs(137,0);
 ((F1)VT(dev,DEV_SETVS))(dev,0x144);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,16,v,sizeof(TL));
 ((F0)VT(dev,DEV_END))(dev);
}
static void draw_lit_vb(void *vb);
static void draw_lit(void) {draw_lit_vb(0);}
/* v1.6: the lit square's state (identity world, view and projection, the material, a white directional light, lighting on) on its own */
static void draw_lit_state(void) {
 static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};float mat[17],lt[26];
 memset(mat,0,sizeof(mat));mat[0]=mat[1]=mat[2]=mat[3]=1.0f; /* diffuse */mat[4]=mat[5]=mat[6]=0.2f;mat[7]=1.0f; /* ambient */
 memset(lt,0,sizeof(lt));*(U32*)&lt[0]=3; /* D3DLIGHT8: Type DIRECTIONAL */lt[1]=lt[2]=lt[3]=lt[4]=1.0f; /* Diffuse */lt[18]=1.0f; /* Direction (floats 16-18) */lt[19]=1000.0f; /* Range */
 ((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
 ((HRESULT(__stdcall*)(void*,void*))VT(dev,DEV_SETMAT))(dev,mat); /* v1.1: SetMaterial(this, material) */((F2P)VT(dev,DEV_SETLIGHT))(dev,0,lt);((F2)VT(dev,DEV_LIGHTON))(dev,0,1);
 rs(137,1);rs(139,0xFF404040UL);rs(145,0);rs(147,0); /* lighting on, ambient, DIFFUSEMATERIALSOURCE and AMBIENT... from the material */
}
static void draw_lit_vb(void *vb) { /* v1.5: from a vertex buffer when vb */ /* one lit square, untransformed: identity world, view and projection, a white directional light */
 UV v[6];int j;static const float sx[6]={-0.6f,0.6f,-0.6f,0.6f,0.6f,-0.6f},sy[6]={0.6f,0.6f,-0.6f,0.6f,-0.6f,-0.6f};
 for(j=0;j<6;j++){v[j].x=sx[j];v[j].y=sy[j];v[j].z=0.5f;v[j].nx=0;v[j].ny=0;v[j].nz=-1.0f;v[j].c=0xFFFFFFFFUL;}
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();draw_lit_state();
 ((F1)VT(dev,DEV_SETVS))(dev,0x52);
 if(vb){((F3P)VT(dev,DEV_STREAM))(dev,0,(U32)vb,(void*)sizeof(UV));((HRESULT(__stdcall*)(void*,U32,U32,U32))VT(dev,DEV_DP))(dev,4,0,2);((F3P)VT(dev,DEV_STREAM))(dev,0,0,0);}
 else ((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,2,v,sizeof(UV));
 ((F0)VT(dev,DEV_END))(dev);
}
/* v1.5: a vertex buffer of the lit square's six vertices (the same as draw_lit's) */
static void *make_vb(U32 bytes,int fill) {
 void *vb=0;UV *p=0;int j;static const float sx[6]={-0.6f,0.6f,-0.6f,0.6f,0.6f,-0.6f},sy[6]={0.6f,0.6f,-0.6f,0.6f,-0.6f,-0.6f};
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,void**))VT(dev,DEV_CREATEVB))(dev,bytes,8,0x52,0,&vb)||!vb)return 0; /* WRITEONLY, D3DPOOL_DEFAULT */
 if(fill){if(((HRESULT(__stdcall*)(void*,U32,U32,void**,U32))VT(vb,VB_LOCK))(vb,0,0,(void**)&p,0)||!p){((F0)VT(vb,D3D_REL))(vb);return 0;}
  for(j=0;j<6;j++){p[j].x=sx[j];p[j].y=sy[j];p[j].z=0.5f;p[j].nx=0;p[j].ny=0;p[j].nz=-1.0f;p[j].c=0xFFFFFFFFUL;}((F0)VT(vb,VB_UNLOCK))(vb);}
 return vb;
}
/* v1.6: 4c. a dynamic vertex buffer of 12 vertices: the left half's 6 written (DISCARD) and drawn, then the right half's 6 written past
 * them (NOOVERWRITE) and drawn from vertex 6; the lit pixels counted in each half of the window */
static void vb_append(U32 fvf,U32 stride,const char *what) {
 void *vb=0;unsigned char *p;int side,j,k,l[2]={0,0};
 static const float qx[6]={0,1,0,1,1,0},qy[6]={0,0,1,0,1,1};
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,void**))VT(dev,DEV_CREATEVB))(dev,12*stride,0x208,fvf,0,&vb)||!vb){wrong("CreateVertexBuffer (dynamic) failed");return;}
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();
 if(fvf==0x52)draw_lit_state();else rs(137,0);
 ((F1)VT(dev,DEV_SETVS))(dev,fvf);((F3P)VT(dev,DEV_STREAM))(dev,0,(U32)vb,(void*)stride);
 for(side=0;side<2;side++){
  p=0;if(((HRESULT(__stdcall*)(void*,U32,U32,void**,U32))VT(vb,VB_LOCK))(vb,side*6*stride,6*stride,(void**)&p,side?0x1000:0x2000)||!p){wrong("vertex buffer Lock failed");break;}
  for(j=0;j<6;j++){float *v=(float*)(p+j*stride);
   if(fvf==0x52){v[0]=(side?0.1f:-0.9f)+0.8f*qx[j];v[1]=0.6f-1.2f*qy[j];v[2]=0.5f;v[3]=0;v[4]=0;v[5]=-1.0f;*(U32*)&v[6]=0xFFFFFFFFUL;}
   else{v[0]=(side?140.0f:20.0f)+96.0f*qx[j];v[1]=60.0f+120.0f*qy[j];v[2]=0.5f;v[3]=1.0f;*(U32*)&v[4]=0xFFFFFFFFUL;v[5]=0;v[6]=0;}}
  ((F0)VT(vb,VB_UNLOCK))(vb);
  ((HRESULT(__stdcall*)(void*,U32,U32,U32))VT(dev,DEV_DP))(dev,4,side*6,2);}
 ((F3P)VT(dev,DEV_STREAM))(dev,0,0,0);((F0)VT(dev,DEV_END))(dev);
 if(read_bb(cur)){present();for(k=0;k<WIN*WIN;k++)if(cur[k])l[(k%WIN)>=WIN/2]++;}
 ((F0)VT(vb,D3D_REL))(vb);
 say("   %s: lit %d in the left half (vertices 0-5), %d in the right (6-11, written after the first draw)\n",what,l[0],l[1]);
 if(!l[0])wrong("the first half (vertices 0-5) was not drawn");
 if(!l[1])wrong("the appended half (vertices 6-11) was not drawn");
}
/* v1.7: 5. the .540 checks, on the current device (sw: the software-processing one) */
static U32 at(const U32 *p,int x,int y) {return p[y*WIN+x];}
static void test_540(int sw) {
 void *tx=0;LR lr;int k,l;
 static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},rz[16]={0,1,0,0,-1,0,0,0,0,0,1,0,0,0,0,1};
 say("5a. %s: an indexed draw with a triangle 501 vertices wide and a bad vertex (rhw -1), then the lit square\n",sw?"software":"hardware");
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,8,8,1,0,22,1,&tx)||!tx){wrong("CreateTexture failed");return;}
 if(!((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(tx,16))(tx,0,&lr,0,0)){for(k=0;k<8;k++)memset((char*)lr.bits+k*lr.pitch,0xC0,32);((F1)VT(tx,17))(tx,0);}
 {static TL v[600];static const unsigned short i1[3]={0,1,2},i2[3]={0,1,500};
  for(k=0;k<600;k++){v[k].x=20.0f+(k%60)*3.0f;v[k].y=20.0f+(k/60)*20.0f+(k%2)*10.0f;v[k].z=0.5f;v[k].w=1.0f;v[k].c=0xFFFFFFFFUL;v[k].u=(k%2)?1.0f:0.0f;v[k].v=(k%3)?1.0f:0.0f;}
  v[500].w=-1.0f;
  ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();rs(137,0);((F2P)VT(dev,DEV_SETTEX))(dev,0,tx);tss(0,1,4);tss(0,2,2);tss(0,3,0);
  ((F1)VT(dev,DEV_SETVS))(dev,0x144);
  ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DIPUP))(dev,4,0,600,1,i1,101,v,sizeof(TL));
  ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DIPUP))(dev,4,0,600,1,i2,101,v,sizeof(TL));
  ((F0)VT(dev,DEV_END))(dev);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);
  if(read_bb(cur))present();}
 ((F0)VT(tx,D3D_REL))(tx);
 draw_lit();l=read_bb(cur)?lit(cur):0;present();
 say("   the lit square after it: lit %d\n",l);
 if(!l)wrong("nothing drawn after the indexed draw (the device lost?)");
 say("5b. %s: the lit square turned +90 degrees about z (normals (1,0,0) to (0,1,0)), a light along -y\n",sw?"software":"hardware");
 {UV v[6];static const float sx[6]={-0.6f,0.6f,-0.6f,0.6f,0.6f,-0.6f},sy[6]={0.6f,0.6f,-0.6f,0.6f,-0.6f,-0.6f};float lt[26],mat[17];U32 c;
  for(k=0;k<6;k++){v[k].x=sx[k];v[k].y=sy[k];v[k].z=0.5f;v[k].nx=1.0f;v[k].ny=0;v[k].nz=0;v[k].c=0xFFFFFFFFUL;}
  ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();draw_lit_state();
  memset(mat,0,sizeof(mat));mat[0]=mat[1]=mat[2]=mat[3]=1.0f;mat[7]=1.0f;((HRESULT(__stdcall*)(void*,void*))VT(dev,DEV_SETMAT))(dev,mat);
  memset(lt,0,sizeof(lt));*(U32*)&lt[0]=3;lt[1]=lt[2]=lt[3]=lt[4]=1.0f;lt[17]=-1.0f;lt[19]=1000.0f;((F2P)VT(dev,DEV_SETLIGHT))(dev,0,lt);
  rs(139,0);((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)rz);
  ((F1)VT(dev,DEV_SETVS))(dev,0x52);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,2,v,sizeof(UV));
  ((F0)VT(dev,DEV_END))(dev);((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);
  c=read_bb(cur)?at(cur,WIN/2,WIN/2):0;present();
  say("   the centre pixel: %06lX (expected white: lit fully)\n",c&0xFFFFFFUL);
  if(((c>>16)&255)<200||((c>>8)&255)<200||(c&255)<200)wrong("the turned square is not lit (normals turned the wrong way?)");}
 say("5c. %s: two texture stages, untransformed vertices (XYZ, two coordinate sets)\n",sw?"software":"hardware");
 {typedef struct {float x,y,z,u0,v0,u1,v1;} T2;T2 v[6];static const float sx[6]={-0.6f,0.6f,-0.6f,0.6f,0.6f,-0.6f},sy[6]={0.6f,0.6f,-0.6f,0.6f,-0.6f,-0.6f};
  for(k=0;k<6;k++){v[k].x=sx[k];v[k].y=sy[k];v[k].z=0.5f;v[k].u0=v[k].v0=v[k].u1=v[k].v1=0;}
  ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
  rs(137,0);rs(60,0xFF80FF40UL); /* TEXTUREFACTOR */
  tss(0,1,2);tss(0,2,3);tss(0,4,2);tss(0,5,3);tss(1,1,2);tss(1,2,1);tss(1,4,2);tss(1,5,1);tss(1,11,1); /* SELECTARG1 TFACTOR, then CURRENT */
  ((F1)VT(dev,DEV_SETVS))(dev,0x202);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,2,v,sizeof(T2));
  ((F0)VT(dev,DEV_END))(dev);tss(1,1,1);tss(1,4,1);tss(1,11,1);
  l=read_bb(cur)?lit(cur):0;k=l?(int)(at(cur,WIN/2,WIN/2)&0xFFFFFFUL):0;present();
  say("   lit %d, the centre pixel %06X (expected 80FF40)\n",l,k);
  if(!l)wrong("the two-stage square was not drawn");}
}
/* v1.15 (.640): 7. untransformed points and lines, and draws too large for the driver's scratch buffer, on the current device. The
 * software device's pictures are kept; the hardware device's are compared with them. */
typedef struct {float x,y,z;U32 c;} PV; /* FVF 0x42 */
#define DPUP(t,n,v) ((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,t,n,v,sizeof(PV))
#define DIPUP(t,nv,n,ix,v) ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DIPUP))(dev,t,0,nv,n,ix,101,v,sizeof(PV))
static U32 p7_sw[21][WIN*WIN];static int p7_have[21]; /* v1.51: 19-20 for 12c-12d */ /* v1.29: 8-11 for 9; v1.30: 12-14 for 10; v1.49: 15-16 for 12; v1.50: 17-18 for 10d-10e */
static PV p7_v[30000];static unsigned short p7_ix[1800];
static void p7_begin(void) {static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();rs(137,0);
 ((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
 tss(0,1,2);tss(0,2,0);tss(0,4,2);tss(0,5,0);((F1)VT(dev,DEV_SETVS))(dev,0x42);}
static void p7_pv(PV *v,float x,float y,float z,U32 c) {v->x=x;v->y=y;v->z=z;v->c=c;}
static void p7_end(int no,int sw,const char *what,int pct) {
 int l;((F0)VT(dev,DEV_END))(dev);l=read_bb(cur)?lit(cur):0;present();
 if(sw){memcpy(p7_sw[no],cur,sizeof(cur));p7_have[no]=1;say("   %s: lit %d\n",what,l);if(!l)wrong("nothing drawn by the software device");return;}
 if(!l){say("   %s: lit 0\n",what);wrong("nothing drawn");return;}
 if(p7_have[no]){int d=ndiff(p7_sw[no],cur),ls=lit(p7_sw[no]);
  say("   %s: lit %d (software %d); %d pixels differ\n",what,l,ls,d);
  if(d*100>ls*pct||l*10<ls*9){char a[16],b[16];sprintf(a,no<10?"VXTL7%dA.BMP":"VXTL%dA.BMP",no);sprintf(b,no<10?"VXTL7%dB.BMP":"VXTL%dB.BMP",no);bmp(a,p7_sw[no]);bmp(b,cur);wrong("not the software device's picture (the A file software, the B file the driver's)");}}
 else say("   %s: lit %d (no software picture to compare)\n",what,l);
}
static void test_640(int sw) {
 int i,j,k,miss=0;
 say("7. %s: untransformed points and lines, large draws\n",sw?"software (the runtime's T&L)":"hardware (the driver's T&L)");
 /* 7a. 400 points, a 20 x 20 grid; 8 more outside the planes (not drawn) */
 for(k=0,j=0;j<20;j++)for(i=0;i<20;i++)p7_pv(&p7_v[k++],-0.855f+i*0.09f,0.855f-j*0.09f,0.5f,0xFF000000UL|((U32)(60+i*9)<<16)|((U32)(250-j*9)<<8)|0x80);
 p7_pv(&p7_v[k++],1.5f,0,0.5f,0xFFFFFFFFUL);p7_pv(&p7_v[k++],-1.5f,0,0.5f,0xFFFFFFFFUL);p7_pv(&p7_v[k++],0,1.5f,0.5f,0xFFFFFFFFUL);p7_pv(&p7_v[k++],0,-1.5f,0.5f,0xFFFFFFFFUL);
 p7_pv(&p7_v[k++],0.005f,0.005f,-0.5f,0xFFFFFFFFUL);p7_pv(&p7_v[k++],0.005f,0.005f,1.5f,0xFFFFFFFFUL);p7_pv(&p7_v[k++],2,2,0.5f,0xFFFFFFFFUL);p7_pv(&p7_v[k++],-2,-2,2,0xFFFFFFFFUL);
 p7_begin();DPUP(1,(U32)k,p7_v);((F0)VT(dev,DEV_END))(dev);
 if(read_bb(cur)){for(j=0;j<20;j++)for(i=0;i<20;i++){int x=(int)(128.0f+(-0.855f+i*0.09f)*128.0f),y=(int)(128.0f-(0.855f-j*0.09f)*128.0f),dx,dy,hit=0;
   for(dy=-1;dy<=1;dy++)for(dx=-1;dx<=1;dx++)if(cur[(y+dy)*WIN+x+dx])hit=1;if(!hit)miss++;}
  say("   7a. 400 points (and 8 outside the planes): lit %d, points not found %d\n",lit(cur),miss);
  if(miss)wrong("points not drawn");if(lit(cur)>1700)wrong("more lit than 400 points make");
  if(sw){memcpy(p7_sw[0],cur,sizeof(cur));p7_have[0]=1;}
  else if(p7_have[0]){int d=ndiff(p7_sw[0],cur);say("       against the software device: %d pixels differ\n",d);if(d>40){bmp("VXTL70A.BMP",p7_sw[0]);bmp("VXTL70B.BMP",cur);wrong("the points are not where the software device puts them");}}}
 else wrong("back buffer lock failed");
 present();
 /* 7b. lines: a list of 16 spokes, a strip of 24, an indexed list of 12, six leaving the planes */
 p7_begin();
 {static const float cs[16][2]={{1,0},{.92f,.38f},{.71f,.71f},{.38f,.92f},{0,1},{-.38f,.92f},{-.71f,.71f},{-.92f,.38f},{-1,0},{-.92f,-.38f},{-.71f,-.71f},{-.38f,-.92f},{0,-1},{.38f,-.92f},{.71f,-.71f},{.92f,-.38f}};
  for(k=0;k<16;k++){p7_pv(&p7_v[k*2],cs[k][0]*0.1f,cs[k][1]*0.1f,0.5f,0xFFFFFF00UL);p7_pv(&p7_v[k*2+1],cs[k][0]*0.45f,cs[k][1]*0.45f,0.5f,0xFFFF4040UL);}
  DPUP(2,16,p7_v);
  for(k=0;k<25;k++)p7_pv(&p7_v[k],-0.9f+k*0.075f,(k&1)?0.9f:0.7f,0.3f,0xFF40FF40UL);
  DPUP(3,24,p7_v);
  for(k=0;k<13;k++){p7_pv(&p7_v[k*2],-0.9f+k*0.15f,-0.6f,0.6f,0xFF4080FFUL);p7_pv(&p7_v[k*2+1],-0.9f+k*0.15f,-0.9f,0.6f,0xFFFFFFFFUL);}
  for(k=0;k<12;k++){p7_ix[k*2]=(unsigned short)(k*2);p7_ix[k*2+1]=(unsigned short)(k*2+3);}
  DIPUP(2,26,12,p7_ix,p7_v);
  p7_pv(&p7_v[0],0.5f,0.2f,0.5f,0xFFFFFFFFUL);p7_pv(&p7_v[1],1.7f,0.3f,0.5f,0xFFFFFFFFUL); /* right */
  p7_pv(&p7_v[2],-0.5f,0.2f,0.5f,0xFFFFFFFFUL);p7_pv(&p7_v[3],-1.7f,0.3f,0.5f,0xFFFFFFFFUL); /* left */
  p7_pv(&p7_v[4],0.6f,-0.3f,0.5f,0xFFFF80FFUL);p7_pv(&p7_v[5],0.7f,-1.8f,0.5f,0xFFFF80FFUL); /* bottom */
  p7_pv(&p7_v[6],-0.6f,0.5f,0.5f,0xFFFF80FFUL);p7_pv(&p7_v[7],-0.65f,1.8f,0.5f,0xFFFF80FFUL); /* top */
  p7_pv(&p7_v[8],-0.6f,-0.3f,0.5f,0xFF80FFFFUL);p7_pv(&p7_v[9],-0.2f,-0.5f,-0.5f,0xFF80FFFFUL); /* near */
  p7_pv(&p7_v[10],0.2f,-0.5f,1.5f,0xFF80FFFFUL);p7_pv(&p7_v[11],0.5f,-0.55f,0.5f,0xFF80FFFFUL); /* far */
  p7_pv(&p7_v[12],1.2f,0.9f,0.5f,0xFFFFFFFFUL);p7_pv(&p7_v[13],1.6f,-0.9f,0.5f,0xFFFFFFFFUL); /* outside: not drawn */
  DPUP(2,7,p7_v);}
 p7_end(1,sw,"7b. lines (list, strip, indexed, six leaving the planes)",10);
 /* 7c. one list of 9,000 triangles (27,000 vertices): 90 x 50 cells */
 p7_begin();
 for(k=0,j=0;j<50;j++)for(i=0;i<90;i++){float x0=-0.9f+i*0.02f,y0=0.5f-j*0.02f,x1=x0+0.02f,y1=y0-0.02f;U32 c=0xFF000000UL|((U32)(40+i*2)<<16)|((U32)(40+j*4)<<8)|(((i^j)&1)?0xFF:0x40);
  p7_pv(&p7_v[k++],x0,y0,0.5f,c);p7_pv(&p7_v[k++],x1,y0,0.5f,c);p7_pv(&p7_v[k++],x0,y1,0.5f,c);p7_pv(&p7_v[k++],x1,y0,0.5f,c);p7_pv(&p7_v[k++],x1,y1,0.5f,c);p7_pv(&p7_v[k++],x0,y1,0.5f,c);}
 DPUP(4,9000,p7_v);
 p7_end(2,sw,"7c. a list of 9,000 triangles in one draw",1);
 /* 7d. 600 indexed triangles whose vertices lie 100 apart in 30,000 */
 p7_begin();
 for(k=0;k<30000;k++)p7_pv(&p7_v[k],0,0,0.5f,0);
 for(k=0;k<300;k++){int cx=k%20,cy=k/20;float x0=-0.8f+cx*0.08f,y0=0.6f-cy*0.08f;PV *q=&p7_v[k*100];U32 c=0xFF000000UL|((U32)(50+cx*10)<<16)|((U32)(255-cy*12)<<8)|0x60;
  p7_pv(q,x0,y0,0.5f,c);p7_pv(q+1,x0+0.06f,y0,0.5f,c);p7_pv(q+2,x0,y0-0.06f,0.5f,c);p7_pv(q+3,x0+0.06f,y0-0.06f,0.5f,c);
  p7_ix[k*6]=(unsigned short)(k*100);p7_ix[k*6+1]=(unsigned short)(k*100+1);p7_ix[k*6+2]=(unsigned short)(k*100+2);p7_ix[k*6+3]=(unsigned short)(k*100+1);p7_ix[k*6+4]=(unsigned short)(k*100+3);p7_ix[k*6+5]=(unsigned short)(k*100+2);}
 DIPUP(4,30000,600,p7_ix,p7_v);
 p7_end(3,sw,"7d. 600 indexed triangles across 30,000 vertices",1);
}
/* v1.16 (.646): 8. what the review of .644 found, on the current device: a spot light's cone, specular highlights (local viewer), lights
 * at indices 17 and 1000, and a third texture stage reading set 0. Each picture is compared with the software device's (2% of the lit
 * pixels may differ: the cone's edge and the highlight's rounding). .645 and before: no cone, no highlight, the high-index lights dark,
 * the third stage on the second stage's coordinates. */
typedef struct {float x,y,z,nx,ny,nz;U32 c;} NV8;
typedef struct {float x,y,z,u0,v0,u1,v1;} T28;
static NV8 p8_v[25*25];static unsigned short p8_ix[24*24*6];
static void p8_light(U32 idx,U32 type,float r,float g,float b,const float *pos,const float *dir,float range,float theta,float phi,float a0,float a1) {
 float lt[26];memset(lt,0,sizeof(lt));*(U32*)&lt[0]=type;lt[1]=r;lt[2]=g;lt[3]=b;lt[4]=1.0f;lt[5]=lt[6]=lt[7]=lt[8]=1.0f;
 lt[13]=pos[0];lt[14]=pos[1];lt[15]=pos[2];lt[16]=dir[0];lt[17]=dir[1];lt[18]=dir[2];lt[19]=range;lt[20]=1.0f;lt[21]=a0;lt[22]=a1;lt[24]=theta;lt[25]=phi;
 ((F2P)VT(dev,DEV_SETLIGHT))(dev,idx,lt);((F2)VT(dev,DEV_LIGHTON))(dev,idx,1);}
static void p8_dome(void) { /* a 24x24 grid in clip space (identity matrices), the normals of a dome facing the viewer */
 int i,j,k=0;
 for(j=0;j<=24;j++)for(i=0;i<=24;i++){NV8 *v=&p8_v[j*25+i];float x=-0.8f+i*(1.6f/24),y=-0.8f+j*(1.6f/24),l;
  v->x=x;v->y=y;v->z=0.5f;v->nx=x;v->ny=y;v->nz=-1.0f;l=(float)sqrt(x*x+y*y+1.0f);v->nx/=l;v->ny/=l;v->nz/=l;v->c=0xFFFFFFFFUL;}
 for(j=0;j<24;j++)for(i=0;i<24;i++){unsigned short a=(unsigned short)(j*25+i);p8_ix[k++]=a;p8_ix[k++]=(unsigned short)(a+25);p8_ix[k++]=(unsigned short)(a+1);p8_ix[k++]=(unsigned short)(a+1);p8_ix[k++]=(unsigned short)(a+25);p8_ix[k++]=(unsigned short)(a+26);}}
static void p8_begin(int spec,int localview) {static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};float mat[17];
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();
 ((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
 memset(mat,0,sizeof(mat));mat[0]=mat[1]=mat[2]=0.8f;mat[3]=1.0f;mat[4]=mat[5]=mat[6]=0.3f;mat[7]=1.0f;mat[8]=mat[9]=mat[10]=1.0f;mat[11]=1.0f;mat[16]=20.0f;
 ((HRESULT(__stdcall*)(void*,void*))VT(dev,DEV_SETMAT))(dev,mat);
 rs(137,1);rs(139,0xFF101010UL);rs(141,0);rs(143,1);rs(29,spec);rs(142,localview);
 ((F2)VT(dev,DEV_LIGHTON))(dev,0,0);((F2)VT(dev,DEV_LIGHTON))(dev,17,0);((F2)VT(dev,DEV_LIGHTON))(dev,1000,0);
 ((F1)VT(dev,DEV_SETVS))(dev,0x52);}
static void p8_draw(void) {((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DIPUP))(dev,4,0,625,24*24*2,p8_ix,101,p8_v,sizeof(NV8));}
static void *p8_tex(U32 a,U32 b) {void *tx=0;LR lr;int x,y;
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,8,8,1,0,22,1,&tx)||!tx)return 0;
 if(!((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(tx,16))(tx,0,&lr,0,0)){for(y=0;y<8;y++){U32 *r=(U32*)((char*)lr.bits+y*lr.pitch);for(x=0;x<8;x++)r[x]=((x>>2)^(y>>2))&1?a:b;}((F1)VT(tx,17))(tx,0);}
 return tx;}
/* v1.29 (.663): 9. what the review of .661 found, through the driver's T&L, each picture against the software device's: 9a flat-shaded
 * triangles whose first vertex is clipped away (.661 and before: their colour interpolated at the cut), 9b fog with flat shading (the fog
 * flattened), 9c a texture matrix whose u is the vertex's third coordinate (read as 1), 9d a cube map read by three coordinates (r written
 * 1: every pixel from one face) */
typedef struct {float x,y,z;U32 c;float u,v,w;} PV3; /* FVF 0x10142: TEXCOORDSIZE3 */
static PV3 p9_v[96];
static U32 fbits9(float f){U32 u;memcpy(&u,&f,4);return u;}
static void p9_pv(PV3 *q,float x,float y,float z,U32 c,float u,float v,float w){q->x=x;q->y=y;q->z=z;q->c=c;q->u=u;q->v=v;q->w=w;}
static void test_663(int sw) {
 int k,j;void *tx,*cube=0;static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 static const float wu[16]={0,0,0,0, 0,1,0,0, 1,0,0,0, 0,0,0,1}; /* u' = the third coordinate, v' = v */
 say("9. %s: what the review of .661 found\n",sw?"software (the runtime's T&L)":"hardware (the driver's T&L)");
 /* 9a */
 p7_begin();rs(9,1);
 for(k=0;k<6;k++){float y=0.85f-k*0.3f;p7_pv(&p7_v[k*3],-1.5f,y,0.5f,0xFFFF4040UL);p7_pv(&p7_v[k*3+1],0.7f,y-0.05f,0.5f,0xFF40FF40UL);p7_pv(&p7_v[k*3+2],0.1f,y-0.27f,0.5f,0xFF4040FFUL);}
 DPUP(4,6,p7_v);rs(9,2);
 p7_end(8,sw,"9a. flat-shaded triangles, the first vertex clipped away",2);
 /* 9b */
 p7_begin();rs(9,1);rs(28,1);rs(34,0xFFC0C0C0UL);rs(140,3);rs(36,fbits9(0.1f));rs(37,fbits9(0.95f));
 for(k=0;k<6;k++){float y=0.85f-k*0.3f;p7_pv(&p7_v[k*3],-0.9f,y,0.1f,0xFF2040FFUL);p7_pv(&p7_v[k*3+1],0.9f,y-0.05f,0.9f,0xFF2040FFUL);p7_pv(&p7_v[k*3+2],-0.5f,y-0.27f,0.5f,0xFF2040FFUL);}
 DPUP(4,6,p7_v);rs(9,2);rs(28,0);
 p7_end(9,sw,"9b. fog with flat shading",2);
 /* 9c */
 tx=p8_tex(0xFFFF8000UL,0xFF0080FFUL);
 p7_begin();
 if(tx){((F2P)VT(dev,DEV_SETTEX))(dev,0,tx);tss(0,1,4);tss(0,2,2);tss(0,3,0);tss(0,24,2);((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)wu);((F1)VT(dev,DEV_SETVS))(dev,0x10142);
  for(k=0;k<4;k++){float x0=-0.9f+k*0.45f,x1=x0+0.4f;PV3 *q=p9_v+k*6;
   p9_pv(q,x0,0.8f,0.5f,0xFFFFFFFFUL,0,0,0);p9_pv(q+1,x1,0.8f,0.5f,0xFFFFFFFFUL,0,0,1.0f+k);p9_pv(q+2,x0,-0.8f,0.5f,0xFFFFFFFFUL,0,2,0);
   p9_pv(q+3,x1,0.8f,0.5f,0xFFFFFFFFUL,0,0,1.0f+k);p9_pv(q+4,x1,-0.8f,0.5f,0xFFFFFFFFUL,0,2,1.0f+k);p9_pv(q+5,x0,-0.8f,0.5f,0xFFFFFFFFUL,0,2,0);}
  ((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,8,p9_v,sizeof(PV3));
  tss(0,24,0);((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)id);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F1)VT(dev,DEV_SETVS))(dev,0x42);}
 p7_end(10,sw,"9c. a texture matrix reading the third coordinate",2);
 if(tx)((F0)VT(tx,D3D_REL))(tx);
 /* 9d */
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,void**))VT(dev,22))(dev,8,1,0,22,1,&cube)||!cube){say("   9d. a cube map: CreateCubeTexture refused, not run\n");return;}
 {static const U32 fc[6]={0xFFFF2020UL,0xFF20FF20UL,0xFF2020FFUL,0xFFFFFF20UL,0xFFFF20FFUL,0xFF20FFFFUL};LR lr;int x,y;
  for(j=0;j<6;j++)if(!((HRESULT(__stdcall*)(void*,U32,U32,LR*,void*,U32))VT(cube,16))(cube,(U32)j,0,&lr,0,0)){
   for(y=0;y<8;y++){U32 *r=(U32*)((char*)lr.bits+y*lr.pitch);for(x=0;x<8;x++)r[x]=fc[j];}((F2)VT(cube,17))(cube,(U32)j,0);}}
 p7_begin();((F2P)VT(dev,DEV_SETTEX))(dev,0,cube);tss(0,1,2);tss(0,2,2);tss(0,4,2);tss(0,5,2);((F1)VT(dev,DEV_SETVS))(dev,0x10142);
 for(k=0;k<4;k++){float x0=-0.9f+k*0.45f,x1=x0+0.4f,s=k&1?-1.0f:1.0f;PV3 *q=p9_v+k*6;
  p9_pv(q,x0,0.8f,0.5f,0xFFFFFFFFUL,-1,1,s*0.3f);p9_pv(q+1,x1,0.8f,0.5f,0xFFFFFFFFUL,1,1,s*(0.6f+k*0.2f));p9_pv(q+2,x0,-0.8f,0.5f,0xFFFFFFFFUL,-1,-1,s*0.625f);
  p9_pv(q+3,x1,0.8f,0.5f,0xFFFFFFFFUL,1,1,s*(0.6f+k*0.2f));p9_pv(q+4,x1,-0.8f,0.5f,0xFFFFFFFFUL,1,-1,s*1.5f);p9_pv(q+5,x0,-0.8f,0.5f,0xFFFFFFFFUL,-1,-1,s*0.625f);}
 ((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,8,p9_v,sizeof(PV3));
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F1)VT(dev,DEV_SETVS))(dev,0x42);
 p7_end(11,sw,"9d. a cube map read by three coordinates",2);
 ((F0)VT(cube,D3D_REL))(cube);
}
/* v1.30 (.668): 10. what the review of .667 found, each picture against the software device's: 10a a state block holding eleven matrices,
 * a material and ten lights (past the 1,024 bytes .663-.667 kept for a block's T&L commands: applying it left the state as it was),
 * applied after all of it was set otherwise; 10b 65 light indices set and switched on and off in turn, then the first switched on again
 * (.646-.667 had cleared its definition for the 65th: the dome dark); 10c already-transformed vertices with three coordinate sets on
 * three stages (.667 left them out: nothing drawn; .668 draws stage 2 on the second set, so only "drawn" is judged there) */
enum {DEV_BEGINSB=52,DEV_ENDSB=53,DEV_APPLYSB=54,DEV_DELSB=56};
typedef struct {float x,y,z,rhw;U32 c;float u0,v0,u1,v1,u2,v2;} TV3; /* FVF 0x344 */
typedef struct {float x,y,z;U32 c;float u0,v0,u1,v1,u2,v2;} TU3; /* FVF 0x342: v1.50, untransformed */
static void p10_mat(float r,float g,float b) {float m[17];memset(m,0,sizeof(m));m[0]=r;m[1]=g;m[2]=b;m[3]=1;m[4]=m[5]=m[6]=0.2f;m[7]=1;((HRESULT(__stdcall*)(void*,void*))VT(dev,DEV_SETMAT))(dev,m);}
static void p10_dl(U32 idx,float r,float g,float b,float dx,float dy) {
 float lt[26];memset(lt,0,sizeof(lt));*(U32*)&lt[0]=3;lt[1]=r;lt[2]=g;lt[3]=b;lt[4]=1.0f;lt[16]=dx;lt[17]=dy;lt[18]=1.0f;((F2P)VT(dev,DEV_SETLIGHT))(dev,idx,lt);}
/* v1.47 (.730): 11. D3DRS_WRAP0 (cylindrical wrap, FE/0B): a quad whose u runs 0.875 -> 0.125 with WRAP0 U on must be the quad whose u runs
 * 0.875 -> 1.125 with it off (the short way across 1.0, address WRAP); the same quad with WRAP0 off goes the long way (0.875 -> 0.125), a
 * different picture, which shows the case can tell them apart. The same for V. Pretransformed, point sampled, an 8x8 checker. */
static void wrap_draw(float u0,float u1,float v0,float v1,U32 wrap,void *tx,U32 *out) {
 TL q[4];int k;
 for(k=0;k<4;k++){q[k].x=(k&1)?228.0f:28.0f;q[k].y=(k&2)?228.0f:28.0f;q[k].z=0.5f;q[k].w=1.0f;q[k].c=0xFFFFFFFFUL;q[k].u=(k&1)?u1:u0;q[k].v=(k&2)?v1:v0;}
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();rs(137,0);rs(7,0);rs(128,wrap);
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,tx);tss(0,1,2);tss(0,2,2);tss(0,4,2);tss(0,5,2);tss(0,13,1);tss(0,14,1);tss(0,16,1);tss(0,17,1);tss(0,18,0);tss(0,11,0);
 ((F1)VT(dev,DEV_SETVS))(dev,0x144);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,5,2,q,sizeof(TL));
 ((F0)VT(dev,DEV_END))(dev);rs(128,0);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);
 if(!read_bb(out))memset(out,0,WIN*WIN*4);
}
static void test_wrap(int sw) {
 static U32 r[WIN*WIN],w[WIN*WIN],l[WIN*WIN];void *tx=p8_tex(0xFFFF0000UL,0xFF0000FFUL);int pass;
 say("11. %s: D3DRS_WRAP0 (.730, FE/0B)\n",sw?"software device":"hardware device");
 if(!tx){wrong("11: no texture");return;}
 for(pass=0;pass<2;pass++){const int uv=pass==0;int nl,dw,dl;
  /* v1.48: 0.75 to 0.0 the short way (across 1.0) is 0.75 to 1.0, one cell of the checker; the long way crosses 0.5, two cells (v1.47's
   * 0.875 to 0.125 covered half of each cell both ways: no case could tell them apart) */
  if(uv){wrap_draw(0.75f,1.0f,0,1,0,tx,r);wrap_draw(0.75f,0.0f,0,1,1,tx,w);wrap_draw(0.75f,0.0f,0,1,0,tx,l);}
  else{wrap_draw(0,1,0.75f,1.0f,0,tx,r);wrap_draw(0,1,0.75f,0.0f,2,tx,w);wrap_draw(0,1,0.75f,0.0f,0,tx,l);}
  nl=lit(r);dw=ndiff(w,r);dl=ndiff(l,r);
  say("   11%c. %s across 1.0: lit %d; WRAP0 %s against the short way: %d pixels differ (%.1f%%); without it (the long way): %d differ%s\n",uv?'a':'b',uv?"u":"v",nl,uv?"U":"V",dw,nl?100.0*dw/nl:0.0,dl,dl<nl/10?" (the case cannot tell them apart)":"");
  if(nl<1000||dw*100>nl){wrong(uv?"11a: WRAP0 U not applied":"11b: WRAP0 V not applied");bmp(uv?"VXTW_UW.BMP":"VXTW_VW.BMP",w);bmp(uv?"VXTW_UR.BMP":"VXTW_VR.BMP",r);}}
 ((F0)VT(tx,D3D_REL))(tx);
}
/* v1.49 (.733, ACCEL_D3D_TFOG): 12. depth vertex fog under a perspective projection whose clip W is eye Z (fourth column 0 0 1 0): the
 * driver's T&L hands it to the engine's fog unit from W (01/70 bit 4 clear), per pixel; the software device works it out per vertex. A
 * finely cut floor from 2 to 80 deep, linear (5 to 50) and EXP2 (density 0.04), each against the software device's picture (2%): a
 * wrong start, end, density or W (Z fog, a scale) shows across the whole floor. */
static void tfog_floor(U32 mode) {
 static PV v[33*33];static unsigned short ix[32*32*6];int i,j,k=0;const float zn=1.0f,zf=100.0f,q=zf/(zf-zn);
 static float pr[16];static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 memset(pr,0,sizeof(pr));pr[0]=1.5f;pr[5]=1.5f;pr[10]=q;pr[11]=1.0f;pr[14]=-q*zn;
 for(j=0;j<=32;j++)for(i=0;i<=32;i++){PV *p=&v[j*33+i];float t=(float)j/32.0f;p->x=-30.0f+i*(60.0f/32);p->y=-1.5f;p->z=2.0f+t*t*78.0f;
  p->c=0xFF000000UL|((U32)(80+i*5)<<16)|((U32)(200-j*3)<<8)|(U32)(60+((i^j)&1)*60);}
 for(j=0;j<32;j++)for(i=0;i<32;i++){unsigned short a=(unsigned short)(j*33+i);ix[k++]=a;ix[k++]=(unsigned short)(a+33);ix[k++]=(unsigned short)(a+1);ix[k++]=(unsigned short)(a+1);ix[k++]=(unsigned short)(a+33);ix[k++]=(unsigned short)(a+34);}
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();rs(137,0);rs(22,1);
 ((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)pr);
 tss(0,1,2);tss(0,2,0);tss(0,4,2);tss(0,5,0);((F1)VT(dev,DEV_SETVS))(dev,0x42);
 rs(28,1);rs(34,0xFF8090A0UL);rs(35,0);rs(48,0);rs(140,mode);{float fs=5.0f,fe=50.0f,fd=0.04f;rs(36,*(U32*)&fs);rs(37,*(U32*)&fe);rs(38,*(U32*)&fd);}
 ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DIPUP))(dev,4,0,33*33,32*32*2,ix,101,v,sizeof(PV));
 rs(28,0);rs(140,0);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);}
/* v1.51 (.737, the fourth review of .736, #9): the same floor in 2 x 2 cells, so each triangle spans a wide depth range. The engine's fog
 * unit works fog out per pixel; Direct3D's vertex fog (the software device here) works it out at the corners and blends between them, so the
 * two differ on such triangles. Nothing is judged WRONG: the difference and both pictures (VXTLFxS.BMP the software device's vertex fog,
 * VXTLFxH.BMP the driver's per-pixel fog) are for deciding whether .733's fog from W stays */
static void tfog_big(U32 mode) {
 static PV v[9];static unsigned short ix[24];int i,j,k=0;const float zn=1.0f,zf=100.0f,q=zf/(zf-zn);
 static float pr[16];static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 memset(pr,0,sizeof(pr));pr[0]=1.5f;pr[5]=1.5f;pr[10]=q;pr[11]=1.0f;pr[14]=-q*zn;
 for(j=0;j<=2;j++)for(i=0;i<=2;i++){PV *p=&v[j*3+i];float t=(float)j/2.0f;p->x=-30.0f+i*30.0f;p->y=-1.5f;p->z=2.0f+t*t*78.0f;p->c=0xFF60C060UL;}
 for(j=0;j<2;j++)for(i=0;i<2;i++){unsigned short a=(unsigned short)(j*3+i);ix[k++]=a;ix[k++]=(unsigned short)(a+3);ix[k++]=(unsigned short)(a+1);ix[k++]=(unsigned short)(a+1);ix[k++]=(unsigned short)(a+3);ix[k++]=(unsigned short)(a+4);}
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();rs(137,0);rs(22,1);
 ((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)pr);
 tss(0,1,2);tss(0,2,0);tss(0,4,2);tss(0,5,0);((F1)VT(dev,DEV_SETVS))(dev,0x42);
 rs(28,1);rs(34,0xFF8090A0UL);rs(35,0);rs(48,0);rs(140,mode);{float fs=5.0f,fe=50.0f,fd=0.04f;rs(36,*(U32*)&fs);rs(37,*(U32*)&fe);rs(38,*(U32*)&fd);}
 ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DIPUP))(dev,4,0,9,8,ix,101,v,sizeof(PV));
 rs(28,0);rs(140,0);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);}
static void tfog_cmp(int no,int sw,const char *what,char tag) {
 int l,d,k,worst=0;char a[16],b[16];((F0)VT(dev,DEV_END))(dev);l=read_bb(cur)?lit(cur):0;present();
 if(sw){memcpy(p7_sw[no],cur,sizeof(cur));p7_have[no]=1;say("   %s: lit %d\n",what,l);return;}
 if(!p7_have[no]){say("   %s: lit %d (no software picture to compare)\n",what,l);return;}
 for(k=0;k<WIN*WIN;k++){int c;for(c=0;c<24;c+=8){int x=(int)((p7_sw[no][k]>>c)&255)-(int)((cur[k]>>c)&255);if(x<0)x=-x;if(x>worst)worst=x;}}
 d=ndiff(p7_sw[no],cur);
 say("   %s: lit %d (software %d); %d pixels differ (%.1f%%), the largest difference %d of 255 in a colour channel (not judged: see VXTLF%cS / VXTLF%cH.BMP)\n",what,l,lit(p7_sw[no]),d,lit(p7_sw[no])?100.0*d/lit(p7_sw[no]):0.0,worst,tag,tag);
 sprintf(a,"VXTLF%cS.BMP",tag);sprintf(b,"VXTLF%cH.BMP",tag);bmp(a,p7_sw[no]);bmp(b,cur);
}
/* v1.51 (.737, the fourth review of .736, #7): MAXMIPLEVEL on a DXT1 chain. A 64x64 DXT1 texture with all 7 levels, each one solid colour
 * (red, green, blue, yellow, cyan, magenta, white), drawn 1:1 (LOD 0) with point mip filtering and MAXMIPLEVEL 0 to 2: the picture must be
 * that level's colour. The sampler reads a DXT chain's levels packed in the top level's block; .736 based a level past 0 on the level's own
 * block (the CPU's copy), so the lower mips were read from the wrong bytes. */
static void test_maxmip(void) {
 static const unsigned short c565[7]={0xF800,0x07E0,0x001F,0xFFE0,0x07FF,0xF81F,0xFFFF};
 static const U32 c888[7]={0xFF0000UL,0x00FF00UL,0x0000FFUL,0xFFFF00UL,0x00FFFFUL,0xFF00FFUL,0xFFFFFFUL};
 void *tx=0;LR lr;int l,b,x,y;TL v[4];
 say("13. hardware: MAXMIPLEVEL 0 to 2 on a 64x64 DXT1 texture with 7 levels, each its own colour, drawn 1:1\n");
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,64,64,7,0,0x31545844UL,1,&tx)||!tx){say("   CreateTexture(DXT1, 7 levels) failed: skipped\n");return;}
 for(l=0;l<7;l++){int w=64>>l,bw=w<4?1:w/4;
  if(((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(tx,16))(tx,(U32)l,&lr,0,0)){wrong("13: LockRect of a DXT1 level failed");((F0)VT(tx,D3D_REL))(tx);return;}
  for(y=0;y<bw;y++)for(x=0;x<bw;x++){unsigned short *q=(unsigned short*)((char*)lr.bits+y*lr.pitch+x*8);q[0]=c565[l];q[1]=c565[l];q[2]=0;q[3]=0;}
  ((F1)VT(tx,17))(tx,(U32)l);}
 memset(v,0,sizeof(v));
 for(l=0;l<4;l++){v[l].x=40.0f+(l&1)*64.0f-0.5f;v[l].y=40.0f+(l>>1)*64.0f-0.5f;v[l].z=0.5f;v[l].w=1.0f;v[l].c=0xFFFFFFFFUL;v[l].u=(float)(l&1);v[l].v=(float)(l>>1);}
 for(b=0;b<3;b++){U32 got;int ok,k; /* 3 and up: the level's packed place (2688 bytes in for 64x64) is not 256-byte aligned: refused by design */
  ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();rs(137,0);((F2P)VT(dev,DEV_SETTEX))(dev,0,tx);
  tss(0,1,2);tss(0,2,2);tss(0,4,2);tss(0,5,2);tss(0,16,1);tss(0,17,1);tss(0,18,1);tss(0,20,(U32)b);
  ((F1)VT(dev,DEV_SETVS))(dev,0x144);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,5,2,v,sizeof(TL));
  tss(0,20,0);tss(0,18,0);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F0)VT(dev,DEV_END))(dev);
  got=read_bb(cur)?at(cur,72,72)&0xFFFFFFUL:0xFFFFFFFFUL;present();
  ok=got!=0xFFFFFFFFUL;for(k=0;k<24&&ok;k+=8){int d=(int)((got>>k)&255)-(int)((c888[b]>>k)&255);if(d<-16||d>16)ok=0;}
  say("   MAXMIPLEVEL %d: the centre %06lX (level %d's colour %06lX): %s\n",b,got,b,c888[b],ok?"right":"WRONG");
  if(!ok){char n[16];sprintf(n,"VXTLM%d.BMP",b);bmp(n,cur);wrong("13: MAXMIPLEVEL on a DXT1 chain sampled the wrong level's bytes");}}
 ((F0)VT(tx,D3D_REL))(tx);
}
static void test_tfog(int sw) {
 say("12. %s: depth vertex fog under a perspective projection (W = eye Z; .733 ACCEL_D3D_TFOG: the engine's fog unit from W)\n",sw?"software (the runtime's T&L)":"hardware (the driver's T&L)");
 tfog_floor(3);p7_end(15,sw,"12a. linear fog 5 to 50 on a floor 2 to 80 deep",2);
 tfog_floor(2);p7_end(16,sw,"12b. EXP2 fog, density 0.04, on the same floor",2);
 tfog_big(3);tfog_cmp(19,sw,"12c. linear fog on the floor in 8 big triangles (per vertex against per pixel)",'L');
 tfog_big(2);tfog_cmp(20,sw,"12d. EXP2 fog on the same 8 triangles",'E');
}
static void test_668(int sw) {
 static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1},sc[16]={0.8f,0,0,0,0,0.8f,0,0,0,0,1,0,0.1f,-0.1f,0,1};
 U32 h=0,k;void *t0,*t1,*t2;static TV3 v[6];
 say("10. %s: what the review of .667 found\n",sw?"software (the runtime's T&L)":"hardware (the driver's T&L)");
 /* 10a */
 p8_begin(0,0);p8_dome();
 ((F0)VT(dev,DEV_BEGINSB))(dev);
 ((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)sc);((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
 for(k=0;k<8;k++)((F2P)VT(dev,DEV_SETXF))(dev,16+k,(void*)id);
 p10_mat(1.0f,0.6f,0.2f);
 for(k=0;k<10;k++)p10_dl(k,k?0.1f:0.9f,k?0.1f:0.8f,k?0.9f:0.3f,0.3f-k*0.05f,-0.2f);
 ((F2)VT(dev,DEV_LIGHTON))(dev,0,1);for(k=1;k<10;k++)((F2)VT(dev,DEV_LIGHTON))(dev,k,0);
 ((HRESULT(__stdcall*)(void*,U32*))VT(dev,DEV_ENDSB))(dev,&h);
 ((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);p10_mat(1,1,1);for(k=0;k<10;k++){p10_dl(k,0.2f,0.9f,0.2f,0,0);((F2)VT(dev,DEV_LIGHTON))(dev,k,k==3);}
 if(h)((F1)VT(dev,DEV_APPLYSB))(dev,h);
 p8_draw();
 if(h)((F1)VT(dev,DEV_DELSB))(dev,h);
 for(k=0;k<10;k++)((F2)VT(dev,DEV_LIGHTON))(dev,k,0);rs(137,0);((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);
 p7_end(12,sw,h?"10a. a state block of eleven matrices, a material and ten lights, applied":"10a. (no state block made)",2);
 /* 10b */
 p8_begin(0,0);p8_dome();
 for(k=0;k<65;k++){p10_dl(100+k,k?0.1f:0.9f,k?0.1f:0.2f,k?0.9f:0.2f,0.2f,-0.3f);((F2)VT(dev,DEV_LIGHTON))(dev,100+k,1);((F2)VT(dev,DEV_LIGHTON))(dev,100+k,0);}
 ((F2)VT(dev,DEV_LIGHTON))(dev,100,1);
 p8_draw();
 ((F2)VT(dev,DEV_LIGHTON))(dev,100,0);rs(137,0);
 p7_end(13,sw,"10b. 65 light indices set in turn, the first switched on again",2);
 /* 10c */
 t0=p8_tex(0xFFFF8000UL,0xFF0080FFUL);t1=p8_tex(0xFFFFFFFFUL,0xFF808080UL);t2=p8_tex(0xFFFFFFFFUL,0xFF40C040UL); /* v1.48: set 2's layer green, its cells crossing set 1's */
 p7_begin();
 if(t0&&t1&&t2){static const float px[6]={64,192,64,192,192,64},py[6]={64,64,192,64,192,192},pu[6]={0,1,0,1,1,0},pv[6]={0,0,1,0,1,1};
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,t0);((F2P)VT(dev,DEV_SETTEX))(dev,1,t1);((F2P)VT(dev,DEV_SETTEX))(dev,2,t2);
  tss(0,1,4);tss(0,2,2);tss(0,3,0);tss(0,11,0);tss(1,1,4);tss(1,2,2);tss(1,3,1);tss(1,4,2);tss(1,5,1);tss(1,11,1);tss(2,1,4);tss(2,2,2);tss(2,3,1);tss(2,4,2);tss(2,5,1);tss(2,11,2);
  for(k=0;k<6;k++){v[k].x=px[k];v[k].y=py[k];v[k].z=0.5f;v[k].rhw=1;v[k].c=0xFFFFFFFFUL;v[k].u0=pu[k];v[k].v0=pv[k];v[k].u1=pu[k]*2;v[k].v1=pv[k]*2;v[k].u2=1-pu[k];v[k].v2=pv[k];}
  ((F1)VT(dev,DEV_SETVS))(dev,0x344);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,2,v,sizeof(TV3));
  tss(1,1,1);tss(1,4,1);tss(2,1,1);tss(2,4,1);tss(1,11,1);tss(2,11,2);
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F2P)VT(dev,DEV_SETTEX))(dev,1,0);((F2P)VT(dev,DEV_SETTEX))(dev,2,0);((F1)VT(dev,DEV_SETVS))(dev,0x42);}
 p7_end(14,sw,"10c. transformed vertices, three stages on three coordinate sets (.731: stage 2 on its own set, slot C)",2);

 /* 10d, 10e (v1.50, driver .734): the same three stages on three coordinate sets, untransformed vertices through the T&L (the driver's: a
  * third output set of its own, slot C), 10e with stage 2's coordinates through its own texture matrix (COUNT2, scaled and moved) */
 {static TU3 u[6];static const float sc2[16]={1.5f,0,0,0, 0,0.75f,0,0, 0.25f,0.1f,1,0, 0,0,0,1},id4[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};int e;
  for(e=0;e<2;e++){
   p7_begin();
   if(t0&&t1&&t2){static const float px[6]={-0.5f,0.5f,-0.5f,0.5f,0.5f,-0.5f},py[6]={0.5f,0.5f,-0.5f,0.5f,-0.5f,-0.5f},pu[6]={0,1,0,1,1,0},pv[6]={0,0,1,0,1,1};
    ((F2P)VT(dev,DEV_SETTEX))(dev,0,t0);((F2P)VT(dev,DEV_SETTEX))(dev,1,t1);((F2P)VT(dev,DEV_SETTEX))(dev,2,t2);
    tss(0,1,4);tss(0,2,2);tss(0,3,0);tss(0,11,0);tss(1,1,4);tss(1,2,2);tss(1,3,1);tss(1,4,2);tss(1,5,1);tss(1,11,1);tss(2,1,4);tss(2,2,2);tss(2,3,1);tss(2,4,2);tss(2,5,1);tss(2,11,2);
    if(e){((F2P)VT(dev,DEV_SETXF))(dev,18,(void*)sc2);tss(2,24,2);}
    for(k=0;k<6;k++){u[k].x=px[k];u[k].y=py[k];u[k].z=0.5f;u[k].c=0xFFFFFFFFUL;u[k].u0=pu[k];u[k].v0=pv[k];u[k].u1=pu[k]*2;u[k].v1=pv[k]*2;u[k].u2=1-pu[k];u[k].v2=pv[k]*1.5f;}
    ((F1)VT(dev,DEV_SETVS))(dev,0x342);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,2,u,sizeof(TU3));
    tss(2,24,0);((F2P)VT(dev,DEV_SETXF))(dev,18,(void*)id4);
    tss(1,1,1);tss(1,4,1);tss(2,1,1);tss(2,4,1);tss(1,11,1);tss(2,11,2);
    ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F2P)VT(dev,DEV_SETTEX))(dev,1,0);((F2P)VT(dev,DEV_SETTEX))(dev,2,0);((F1)VT(dev,DEV_SETVS))(dev,0x42);}
   p7_end(17+e,sw,e?"10e. untransformed, three stages on three sets, stage 2 through its texture matrix (COUNT2)":"10d. untransformed vertices, three stages on three coordinate sets (.734: the T&L's third output set)",2);}}
 if(t0)((F0)VT(t0,D3D_REL))(t0);if(t1)((F0)VT(t1,D3D_REL))(t1);if(t2)((F0)VT(t2,D3D_REL))(t2);
}
static void test_646(int sw) {
 static const float lpos[3]={0,0,-1.0f},ldir[3]={0,0,1.0f},dpos[3]={0,0,0},ddir[3]={0.5f,-0.3f,1.0f},ppos[3]={0.6f,0.6f,0.2f};
 say("8. %s: spot light, specular, lights at indices 17 and 1000, a third stage reading set 0\n",sw?"software (the runtime's T&L)":"hardware (the driver's T&L)");
 p8_dome();
 p8_begin(0,0);p8_light(0,2,1,1,1,lpos,ldir,100.0f,0.5f,0.9f,1.0f,0.0f);p8_draw();
 p7_end(4,sw,"8a. a spot light (theta 0.5, phi 0.9), its cone on a dome",2);
 p8_begin(1,1);p8_light(0,2,1,1,1,lpos,ldir,100.0f,0.9f,1.6f,1.0f,0.0f);p8_draw();
 p7_end(5,sw,"8b. the spot light with specular highlights (power 20, local viewer)",2);
 p8_begin(0,0);p8_light(17,3,0.9f,0.3f,0.2f,dpos,ddir,100.0f,0,0,1.0f,0.0f);p8_light(1000,1,0.1f,0.9f,0.3f,ppos,dpos,100.0f,0,0,0.5f,0.5f);p8_draw();
 ((F2)VT(dev,DEV_LIGHTON))(dev,17,0);((F2)VT(dev,DEV_LIGHTON))(dev,1000,0);
 p7_end(6,sw,"8c. a directional light at index 17 and a point light at index 1000",2);
 {void *ta=p8_tex(0xFFC02020UL,0xFF2020C0UL),*tb=p8_tex(0xFF808080UL,0xFF808080UL),*tc=p8_tex(0xFF00A000UL,0xFF000000UL);T28 q[6];int k;
  static const float qx[6]={-0.8f,0.8f,-0.8f,0.8f,0.8f,-0.8f},qy[6]={0.8f,0.8f,-0.8f,0.8f,-0.8f,-0.8f};
  if(!ta||!tb||!tc)wrong("CreateTexture failed (8d)");
  else{for(k=0;k<6;k++){q[k].x=qx[k];q[k].y=qy[k];q[k].z=0.5f;q[k].u0=(qx[k]+0.8f)/1.6f*2.0f;q[k].v0=(0.8f-qy[k])/1.6f*2.0f;q[k].u1=0.3f;q[k].v1=0.3f;}
   p7_begin();rs(137,0);((F1)VT(dev,DEV_SETVS))(dev,0x202);
   ((F2P)VT(dev,DEV_SETTEX))(dev,0,ta);((F2P)VT(dev,DEV_SETTEX))(dev,1,tb);((F2P)VT(dev,DEV_SETTEX))(dev,2,tc);
   tss(0,1,2);tss(0,2,2);tss(0,4,2);tss(0,5,2);tss(0,11,0);
   tss(1,1,4);tss(1,2,2);tss(1,3,1);tss(1,4,2);tss(1,5,1);tss(1,11,1);
   tss(2,1,7);tss(2,2,2);tss(2,3,1);tss(2,4,2);tss(2,5,1);tss(2,11,0);tss(3,1,1);tss(3,4,1);
   ((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,2,q,sizeof(T28));
   p7_end(7,sw,"8d. three stages: base on set 0, a grey layer on set 1, a detail texture on set 0",2);
   ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F2P)VT(dev,DEV_SETTEX))(dev,1,0);((F2P)VT(dev,DEV_SETTEX))(dev,2,0);
   tss(1,11,1);tss(2,11,2);tss(2,1,1);tss(2,4,1);tss(1,1,1);tss(1,4,1);}
  if(ta)((F0)VT(ta,D3D_REL))(ta);if(tb)((F0)VT(tb,D3D_REL))(tb);if(tc)((F0)VT(tc,D3D_REL))(tc);}
}
/* v1.8: 6. the grid scene (see the top); sw: the software-processing device's picture kept for the hardware one's comparison */
typedef struct {float x,y,z,nx,ny,nz;U32 c;float u,v;} GV; /* FVF 0x152 */
#define GN 24
static int g_secs; /* v1.10: GRID, the seconds each device runs (0: 20 frames) */
static GV g_vb[(GN+1)*(GN+1)];static unsigned short g_ix[GN*GN*6];
static U32 g_gt_tris;
static int g_mode; /* v1.19: 4 the GTA3-like frame (6e). v1.18 (.648): 3 the grid scene with a second stage turned on and off between grids (the records change size between draws:
 * .647's kept set-up drew such a draw with the frame of the draw before, 3DMark 2001's vertices with a NaN rhw); v1.17 (.647): 0 the grid scene; 1 the same lit by a point light; 2 the same triangles as 720 small draws (one row of a grid each, a world matrix before each: GTA3's kind) */
static void grid_frame(void *tx) {
 static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};float pr[16],w[16],mat[17],lt[26];int k;U32 seed=541;
 const float zn=1.0f,zf=200.0f;
 memset(pr,0,sizeof(pr));pr[0]=1.5f;pr[5]=2.0f;pr[10]=zf/(zf-zn);pr[11]=1.0f;pr[14]=-zn*zf/(zf-zn);
 memset(mat,0,sizeof(mat));mat[0]=mat[1]=mat[2]=mat[3]=1.0f;mat[4]=mat[5]=mat[6]=0.3f;mat[7]=1.0f;
 memset(lt,0,sizeof(lt));*(U32*)&lt[0]=3;lt[1]=lt[2]=lt[3]=lt[4]=1.0f;lt[9]=lt[10]=lt[11]=0.2f;lt[12]=1.0f;lt[16]=0.3f;lt[17]=-1.0f;lt[18]=0.2f;lt[19]=1000.0f;
 if(g_mode==1){*(U32*)&lt[0]=1;lt[13]=0.0f;lt[14]=8.0f;lt[15]=30.0f;lt[19]=200.0f;lt[21]=0.5f;lt[22]=0.02f;lt[23]=0.0f;} /* a point light above the middle of the grids, attenuated */
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();
 ((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)pr);
 if(g_mode==7){mat[8]=mat[9]=mat[10]=0.8f;mat[11]=1.0f;mat[16]=20.0f;} /* v1.22: 6h, a specular material, power 20 */
 ((HRESULT(__stdcall*)(void*,void*))VT(dev,DEV_SETMAT))(dev,mat);((F2P)VT(dev,DEV_SETLIGHT))(dev,0,lt);((F2)VT(dev,DEV_LIGHTON))(dev,0,1);
 if(g_mode==7){int q;for(q=0;q<8;q++){float l2[26];memset(l2,0,sizeof(l2));*(U32*)&l2[0]=q<4?1:3;l2[1]=l2[2]=l2[3]=0.35f;l2[4]=1.0f;l2[5]=0.6f;l2[6]=0.5f+0.05f*q;l2[7]=0.4f;l2[8]=1.0f;
   l2[13]=-30.0f+20.0f*q;l2[14]=6.0f;l2[15]=10.0f+12.0f*q;l2[16]=0.2f*(q-4);l2[17]=-1.0f;l2[18]=0.3f;l2[19]=q<4?120.0f:1000.0f;l2[21]=q<4?0.6f:1.0f;l2[22]=q<4?0.01f:0.0f;
   ((F2P)VT(dev,DEV_SETLIGHT))(dev,(U32)q,l2);((F2)VT(dev,DEV_LIGHTON))(dev,(U32)q,1);}
  rs(29,1);rs(142,1);} /* v1.22: 6h, eight lights (four point, four directional) with specular lighting, LOCALVIEWER */
 if(g_mode==8){int q; /* v1.23: 6i, four spot lights (falloff 1 and 2) and two point lights besides the directional one */
  static const float t0[16]={0.5f,0,0,0, 0,-0.5f,0,0, 0,0,1.0f,0, 0.5f,0.5f,0,1.0f}; /* reflection (-1..1) to 0..1 */
  static const float t1[16]={0.75f,0,0,0, 0,-1.0f,0,0, 0.5f,0.5f,1.0f,0, 0,0,0,1.0f}; /* camera space to (0.75x + 0.5z, 0.5z - y, z): divided by z */
  for(q=0;q<6;q++){float l2[26];memset(l2,0,sizeof(l2));*(U32*)&l2[0]=q<4?2:1;l2[1]=0.8f;l2[2]=0.7f-0.1f*(q&1);l2[3]=0.5f+0.1f*q;l2[4]=1.0f;l2[9]=l2[10]=l2[11]=0.1f;l2[12]=1.0f;
   l2[13]=-20.0f+12.0f*q;l2[14]=6.0f;l2[15]=8.0f+10.0f*q;l2[16]=0.1f*(q-2);l2[17]=-1.0f;l2[18]=0.3f;l2[19]=q<4?150.0f:90.0f;l2[20]=(q&1)?2.0f:1.0f;l2[21]=1.0f;l2[22]=q<4?0.0f:0.02f;
   l2[24]=0.5f+0.1f*q;l2[25]=1.2f;((F2P)VT(dev,DEV_SETLIGHT))(dev,(U32)(q+1),l2);((F2)VT(dev,DEV_LIGHTON))(dev,(U32)(q+1),1);}
  ((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)t0);((F2P)VT(dev,DEV_SETXF))(dev,17,(void*)t1);}
 rs(137,1);rs(139,0xFF404040UL);rs(145,1);rs(147,0);rs(148,0);rs(22,1);
 rs(28,1);rs(34,0xFF6080A0UL);rs(35,0);rs(140,3);{float fs=10.0f,fe=150.0f;rs(36,*(U32*)&fs);rs(37,*(U32*)&fe);} /* linear vertex fog */
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,tx);tss(0,1,4);tss(0,2,2);tss(0,3,0);tss(0,4,2);tss(0,5,0);
 ((F1)VT(dev,DEV_SETVS))(dev,0x152);
 for(k=0;k<30;k++){float x,y,z;seed=seed*1103515245UL+12345UL;x=(float)((seed>>8)%600)/10.0f-30.0f;seed=seed*1103515245UL+12345UL;y=-2.0f-(float)((seed>>8)%60)/10.0f;
  seed=seed*1103515245UL+12345UL;z=(float)((seed>>8)%700)/10.0f-10.0f;
  memcpy(w,id,sizeof(w));w[12]=x;w[13]=y;w[14]=z;
  if(g_mode==3||g_mode==5||g_mode==6){const int on=g_mode==5?1:g_mode==6?0:(k&1); /* v1.21: 6f the second stage on for every grid, 6g off for every grid */
   ((F2P)VT(dev,DEV_SETTEX))(dev,1,on?tx:0);tss(1,1,on?4UL:1UL);tss(1,2,2);tss(1,3,1);tss(1,11,0);}
  if(g_mode==8){tss(0,11,0x30000UL);tss(0,24,2); /* v1.23: stage 0 the reflection vector (CAMERASPACEREFLECTIONVECTOR), count 2; stage 1 the camera-space
   * position (CAMERASPACEPOSITION), count 3 PROJECTED; every third grid's stage 1 set 1, which the vertex (one set) lacks */
   ((F2P)VT(dev,DEV_SETTEX))(dev,1,tx);tss(1,1,4);tss(1,2,2);tss(1,3,1);if(k%3==2){tss(1,11,1);tss(1,24,0);}else{tss(1,11,0x20000UL);tss(1,24,3|256);}}
  if(g_mode==2){int r;for(r=0;r<GN;r++){((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)w);
    ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DIPUP))(dev,4,0,(GN+1)*(GN+1),GN*2,g_ix+r*GN*6,101,g_vb,sizeof(GV));}}
  else{((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)w);
  ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DIPUP))(dev,4,0,(GN+1)*(GN+1),GN*GN*2,g_ix,101,g_vb,sizeof(GV));}}
 ((F0)VT(dev,DEV_END))(dev);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F2P)VT(dev,DEV_SETTEX))(dev,1,0);tss(1,1,1);rs(28,0);
 if(g_mode==7){int q;for(q=1;q<8;q++)((F2)VT(dev,DEV_LIGHTON))(dev,(U32)q,0);rs(29,0);rs(142,0);}
 if(g_mode==8){int q;for(q=1;q<7;q++)((F2)VT(dev,DEV_LIGHTON))(dev,(U32)q,0);tss(0,11,0);tss(0,24,0);tss(1,11,0);tss(1,24,0);((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,17,(void*)id);} /* v1.23 */
 rs(137,0);((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
}
/* v1.19 (.648): 6e. a GTA3-like frame, built from the board's GTA3 counts on .647 (VXD3DMON): about 650 indexed draws a frame of about
 * 100 triangles each from static vertex and index buffers (FVF 152: position, normal, colour, one set), a world matrix before two draws in
 * three, a material every 12 lit draws, the texture changed every second draw, linear vertex fog on every draw, 40% of the draws unlit
 * (prelit buildings: their vertex colours) and 60% lit by two directional lights with the diffuse colour from the vertices (Direct3D 8's
 * default), normals and colours varying by vertex, about a quarter of the objects off the screen and some across the near plane. The same
 * frame every time, so the two devices' times and pictures compare directly, whatever the weather in the game. */
#define GT_MESH 16
#define GT_DRAWS 640
#define GT_TEX 8
static void *gt_vb,*gt_ib,*gt_tx[GT_TEX];static U32 gt_v0[GT_MESH],gt_nv[GT_MESH],gt_i0[GT_MESH],gt_nt[GT_MESH];
static void gt_free(void) {int k;if(gt_vb)((F0)VT(gt_vb,D3D_REL))(gt_vb);if(gt_ib)((F0)VT(gt_ib,D3D_REL))(gt_ib);gt_vb=gt_ib=0;
 for(k=0;k<GT_TEX;k++)if(gt_tx[k]){((F0)VT(gt_tx[k],D3D_REL))(gt_tx[k]);gt_tx[k]=0;}}
static int gt_make(void) {
 static const float nt[8][3]={{0,1,0},{0.6f,0.8f,0},{-0.6f,0.8f,0},{0,0.8f,0.6f},{0,0.8f,-0.6f},{0.48f,0.6f,0.64f},{-0.8f,0.6f,0},{0,0,1}};
 U32 nv=0,ni=0,m,i,j,seed=1922;GV *v=0;unsigned short *ix=0;LR lr;int k;
 for(m=0;m<GT_MESH;m++){U32 cw=4+(m*3)%13,ch=2+(m*5)%7;gt_v0[m]=nv;gt_nv[m]=(cw+1)*(ch+1);gt_i0[m]=ni;gt_nt[m]=cw*ch*2;nv+=gt_nv[m];ni+=gt_nt[m]*3;}
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,void**))VT(dev,DEV_CREATEVB))(dev,nv*sizeof(GV),8,0x152,1,&gt_vb)||!gt_vb){wrong("CreateVertexBuffer (6e) failed");return 0;}
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,void**))VT(dev,DEV_CREATEIB))(dev,ni*2,8,101,1,&gt_ib)||!gt_ib){wrong("CreateIndexBuffer (6e) failed");gt_free();return 0;}
 if(((HRESULT(__stdcall*)(void*,U32,U32,void**,U32))VT(gt_vb,VB_LOCK))(gt_vb,0,0,(void**)&v,0)||!v){wrong("vertex buffer Lock (6e) failed");gt_free();return 0;}
 for(m=0;m<GT_MESH;m++){U32 cw=4+(m*3)%13,ch=2+(m*5)%7;
  for(j=0;j<=ch;j++)for(i=0;i<=cw;i++){GV *g=&v[gt_v0[m]+j*(cw+1)+i];const float *n;seed=seed*1103515245UL+12345UL;n=nt[(seed>>16)&7];
   g->x=(float)i/cw*2.0f-1.0f;g->z=(float)j/ch*2.0f-1.0f;g->y=(float)((seed>>8)%100)/400.0f+((i+j)&1?0.15f:0.0f);g->nx=n[0];g->ny=n[1];g->nz=n[2];
   g->c=0xFF000000UL|(((seed>>4)&0x7F)+0x60)<<16|(((seed>>11)&0x7F)+0x50)<<8|(((seed>>18)&0x7F)+0x40);g->u=(float)i/cw;g->v=(float)j/ch;}}
 ((F0)VT(gt_vb,VB_UNLOCK))(gt_vb);
 if(((HRESULT(__stdcall*)(void*,U32,U32,void**,U32))VT(gt_ib,VB_LOCK))(gt_ib,0,0,(void**)&ix,0)||!ix){wrong("index buffer Lock (6e) failed");gt_free();return 0;}
 for(m=0;m<GT_MESH;m++){U32 cw=4+(m*3)%13,ch=2+(m*5)%7,q=gt_i0[m];
  for(j=0;j<ch;j++)for(i=0;i<cw;i++){unsigned short a0=(unsigned short)(j*(cw+1)+i); /* indices from the mesh's first vertex (SetIndices' base) */
   ix[q++]=a0;ix[q++]=(unsigned short)(a0+cw+1);ix[q++]=(unsigned short)(a0+1);ix[q++]=(unsigned short)(a0+1);ix[q++]=(unsigned short)(a0+cw+1);ix[q++]=(unsigned short)(a0+cw+2);}}
 ((F0)VT(gt_ib,VB_UNLOCK))(gt_ib);
 for(k=0;k<GT_TEX;k++){static const U32 col[GT_TEX]={0xFFC0A080UL,0xFF90A0B0UL,0xFFB0B060UL,0xFF8080C0UL,0xFFA06050UL,0xFF60A060UL,0xFFD0D0D0UL,0xFF707070UL};
  if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,8,8,1,0,22,1,&gt_tx[k])||!gt_tx[k]){wrong("CreateTexture (6e) failed");gt_free();return 0;}
  if(!((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(gt_tx[k],16))(gt_tx[k],0,&lr,0,0)){int a,b;for(b=0;b<8;b++){U32 *r=(U32*)((char*)lr.bits+b*lr.pitch);for(a=0;a<8;a++)r[a]=((a^b^k)&1)?col[k]:(col[k]&0xFF7F7F7FUL);}((F1)VT(gt_tx[k],17))(gt_tx[k],0);}}
 return 1;
}
static void gt_frame(void) {
 static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};float pr[16],w[16],mat[17],lt[26];int d;U32 seed=650,tri=0;
 const float zn=1.0f,zf=200.0f;
 memset(pr,0,sizeof(pr));pr[0]=1.5f;pr[5]=2.0f;pr[10]=zf/(zf-zn);pr[11]=1.0f;pr[14]=-zn*zf/(zf-zn);
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();
 ((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)pr);
 memset(lt,0,sizeof(lt));*(U32*)&lt[0]=3;lt[1]=1.0f;lt[2]=0.95f;lt[3]=0.85f;lt[4]=1.0f;lt[9]=lt[10]=lt[11]=0.1f;lt[12]=1.0f;lt[16]=0.4f;lt[17]=-0.8f;lt[18]=0.45f;lt[19]=1000.0f;
 ((F2P)VT(dev,DEV_SETLIGHT))(dev,0,lt);((F2)VT(dev,DEV_LIGHTON))(dev,0,1);
 lt[1]=0.25f;lt[2]=0.3f;lt[3]=0.4f;lt[16]=-0.5f;lt[17]=-0.3f;lt[18]=-0.8f;((F2P)VT(dev,DEV_SETLIGHT))(dev,1,lt);((F2)VT(dev,DEV_LIGHTON))(dev,1,1);
 rs(139,0xFF303840UL);rs(145,1);rs(146,2);rs(147,0);rs(148,0);rs(22,1);
 rs(28,1);rs(34,0xFF8090A0UL);rs(35,0);rs(140,3);{float fs=20.0f,fe=160.0f;rs(36,*(U32*)&fs);rs(37,*(U32*)&fe);} /* linear vertex fog */
 tss(0,1,4);tss(0,2,2);tss(0,3,0);tss(0,4,2);tss(0,5,0);
 ((F1)VT(dev,DEV_SETVS))(dev,0x152);((F3P)VT(dev,DEV_STREAM))(dev,0,(U32)gt_vb,(void*)sizeof(GV));
 for(d=0;d<GT_DRAWS;d++){U32 m;const int litd=d>=GT_DRAWS*2/5; /* the prelit world first, then the lit objects */
  seed=seed*1103515245UL+12345UL;m=(seed>>16)%GT_MESH;
  if(d==0||d==GT_DRAWS*2/5)rs(137,litd);
  if(litd&&(d-GT_DRAWS*2/5)%12==0){memset(mat,0,sizeof(mat));seed=seed*1103515245UL+12345UL;mat[0]=mat[1]=mat[2]=mat[3]=1.0f;mat[4]=mat[5]=mat[6]=0.4f+(float)((seed>>16)%40)/100.0f;mat[7]=1.0f;
   ((HRESULT(__stdcall*)(void*,void*))VT(dev,DEV_SETMAT))(dev,mat);}
  if(!(d&1))((F2P)VT(dev,DEV_SETTEX))(dev,0,gt_tx[(d>>1)%GT_TEX]);
  if(d%3!=2){float x,y,z,sc,c,sn;seed=seed*1103515245UL+12345UL;z=(float)((seed>>8)%1500)/10.0f+0.5f;seed=seed*1103515245UL+12345UL;
   x=((float)((seed>>8)%2000)/1000.0f-1.0f)*z*0.95f;seed=seed*1103515245UL+12345UL;y=-1.5f-(float)((seed>>8)%50)/10.0f;sc=1.5f+(float)((seed>>4)%40)/10.0f;
   c=(float)((seed>>12)%100)/100.0f;sn=1.0f-c*0.5f;c=c*0.8f; /* a rough rotation about y, scaled */
   memcpy(w,id,sizeof(w));w[0]=sn*sc;w[2]=c*sc;w[5]=sc*0.6f;w[8]=-c*sc;w[10]=sn*sc;w[12]=x;w[13]=y;w[14]=z;((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)w);}
  ((HRESULT(__stdcall*)(void*,void*,U32))VT(dev,DEV_SETIB))(dev,gt_ib,gt_v0[m]);
  ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32))VT(dev,DEV_DIP))(dev,4,0,gt_nv[m],gt_i0[m],gt_nt[m]);fchk("a DrawIndexedPrimitive of the GTA3-like frame");tri+=gt_nt[m];}
 ((F0)VT(dev,DEV_END))(dev);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F3P)VT(dev,DEV_STREAM))(dev,0,0,0);((HRESULT(__stdcall*)(void*,void*,U32))VT(dev,DEV_SETIB))(dev,0,0);
 ((F2)VT(dev,DEV_LIGHTON))(dev,1,0);rs(28,0);rs(137,0);rs(146,0);((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
 g_gt_tris=tri;
}
/* .552: keep measurement I/O outside the timed run. Nearest-rank p50/p95
 * describe complete grid_frame + Present + message-pump wall intervals.
 * Present is not a GPU timer or a scanout timestamp. */
#define GRID_SAMPLES 65536UL
static double grid_samples[GRID_SAMPLES];
static int grid_cmp(const void *a,const void *b) {double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y?1:0;}
static U32 g_swm[12][WIN*WIN];static int g_havem[12];
/* v1.20 (.649): 9. depth surfaces swapped and released while the device lives (3DMark 2001 on .648, the second bench: between two
 * tests the context was marked lost for a destroyed depth surface, every ValidateDevice then failed and 3DMark stopped with "failed to
 * validate material"). Three rounds: a new depth surface made, SetRenderTarget(the back buffer, the new one), the previous released (its
 * destroy can reach the driver before the runtime's SETRENDERTARGET); ValidateDevice must answer D3D_OK and the lit square must draw. */
static void test_649(int sw) {
 void *rt=0,*z0=0,*prev=0,*z=0;int r,l=0,bad=0;U32 passes;HRESULT hv;
 say("9. %s: depth surfaces made, bound and released while the device lives, ValidateDevice and a draw after each\n",sw?"software":"hardware");
 if(((HRESULT(__stdcall*)(void*,void**))VT(dev,DEV_GETRT))(dev,&rt)||!rt){wrong("GetRenderTarget failed");return;}
 if(((HRESULT(__stdcall*)(void*,void**))VT(dev,DEV_GETZ))(dev,&z0)||!z0){wrong("GetDepthStencilSurface failed");((F0)VT(rt,D3D_REL))(rt);return;}
 for(r=0;r<3;r++){
  z=0;if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,void**))VT(dev,DEV_CREATEZ))(dev,WIN,WIN,80,0,&z)||!z){wrong("CreateDepthStencilSurface failed");bad++;break;}
  if(((HRESULT(__stdcall*)(void*,void*,void*))VT(dev,DEV_SETRT))(dev,rt,z)){wrong("SetRenderTarget failed");((F0)VT(z,D3D_REL))(z);bad++;break;}
  if(prev)((F0)VT(prev,D3D_REL))(prev);prev=z;
  passes=0;hv=((HRESULT(__stdcall*)(void*,U32*))VT(dev,DEV_VALIDATE))(dev,&passes);
  draw_lit();l=read_bb(cur)?lit(cur):0;present();
  say("   round %d: ValidateDevice %08lX (%lu passes); the lit square: lit %d\n",r+1,(unsigned long)hv,(unsigned long)passes,l);
  if(hv){wrong("ValidateDevice failed after the depth surface was swapped (3DMark: failed to validate material)");bad++;}
  if(!l){wrong("nothing drawn after the depth surface was swapped");bad++;}
 }
 ((HRESULT(__stdcall*)(void*,void*,void*))VT(dev,DEV_SETRT))(dev,rt,z0);if(prev)((F0)VT(prev,D3D_REL))(prev);
 ((F0)VT(z0,D3D_REL))(z0);((F0)VT(rt,D3D_REL))(rt);
 draw_lit();l=read_bb(cur)?lit(cur):0;present();say("   the first depth surface bound again: lit %d\n",l);if(!l)wrong("nothing drawn with the first depth surface bound again");
}
/* v1.24 (.654): 6j and 6k. 3DMark 2001's High Polygon Count on .653 (board): about 3 s a frame, given as N/A; its 1-light run's dragons
 * untextured and unfogged (no pixel program: every triangle through the HAL's per-triangle path), its 8-light run one directional and
 * seven point lights with specular lighting. Here: one torus of 50 x 50 quads (5,000 triangles, 2,601 vertices, FVF 152), drawn 30 times
 * a frame with its own world matrix, stage 0 off (no texture), no fog; 6j the directional light alone, 6k with seven point lights and a
 * specular material (power 20) */
#define TN 50
static void t_sincos(float a,float *sn,float *cs) { /* v1.26: no C library: a reduced to -pi..pi, then the series to x^13 */
 const float pi=3.14159265f;float x=a,x2,t;int k;
 while(x>pi)x-=2*pi;while(x<-pi)x+=2*pi;
 x2=x*x;t=x;*sn=x;for(k=1;k<=6;k++){t*=-x2/((2*k)*(2*k+1));*sn+=t;}
 t=1;*cs=1;for(k=1;k<=6;k++){t*=-x2/((2*k-1)*(2*k));*cs+=t;}
}
static GV t_vb[(TN+1)*(TN+1)];static unsigned short t_ix[TN*TN*6];static int t_made;
static void tor_make(void) {
 int i,j,k=0;if(t_made)return;
 for(j=0;j<=TN;j++)for(i=0;i<=TN;i++){GV *g=&t_vb[j*(TN+1)+i];float ca,sa,cb,sb;t_sincos(6.2831853f*i/TN,&sa,&ca);t_sincos(6.2831853f*j/TN,&sb,&cb);
  g->x=(1.5f+0.5f*cb)*ca;g->y=0.5f*sb;g->z=(1.5f+0.5f*cb)*sa;g->nx=cb*ca;g->ny=sb;g->nz=cb*sa;g->c=0xFFC0C0C0UL;g->u=(float)i/TN;g->v=(float)j/TN;}
 for(j=0;j<TN;j++)for(i=0;i<TN;i++){unsigned short a0=(unsigned short)(j*(TN+1)+i);t_ix[k++]=a0;t_ix[k++]=(unsigned short)(a0+TN+1);t_ix[k++]=(unsigned short)(a0+1);
  t_ix[k++]=(unsigned short)(a0+1);t_ix[k++]=(unsigned short)(a0+TN+1);t_ix[k++]=(unsigned short)(a0+TN+2);}
 t_made=1;
}
static int g_nocull; /* v1.33: 6l's CULL_NONE frame */
static void tor_frame(void) {
 static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};float pr[16],w[16],mat[17],lt[26];int k,q;
 const float zn=1.0f,zf=200.0f;
 fchk("before the tori's frame");tor_make();fchk("making the torus");
 memset(pr,0,sizeof(pr));pr[0]=1.5f;pr[5]=2.0f;pr[10]=zf/(zf-zn);pr[11]=1.0f;pr[14]=-zn*zf/(zf-zn);
 memset(mat,0,sizeof(mat));mat[0]=mat[1]=mat[2]=0.8f;mat[3]=1.0f;mat[4]=mat[5]=mat[6]=0.3f;mat[7]=1.0f;
 if(g_mode>=10){mat[8]=mat[9]=mat[10]=0.9f;mat[11]=1.0f;mat[16]=20.0f;} /* v1.31: 6l as 6k */
 memset(lt,0,sizeof(lt));*(U32*)&lt[0]=3;lt[1]=lt[2]=lt[3]=lt[4]=1.0f;lt[5]=lt[6]=lt[7]=lt[8]=1.0f;lt[9]=lt[10]=lt[11]=0.2f;lt[12]=1.0f;lt[16]=0.3f;lt[17]=-1.0f;lt[18]=0.4f;lt[19]=1000.0f;
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();
 ((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)pr);
 ((HRESULT(__stdcall*)(void*,void*))VT(dev,DEV_SETMAT))(dev,mat);((F2P)VT(dev,DEV_SETLIGHT))(dev,0,lt);((F2)VT(dev,DEV_LIGHTON))(dev,0,1);
 if(g_mode>=10){for(q=1;q<8;q++){float l2[26];memset(l2,0,sizeof(l2));*(U32*)&l2[0]=1;l2[1]=0.5f+0.06f*q;l2[2]=0.6f;l2[3]=0.9f-0.06f*q;l2[4]=1.0f;l2[5]=l2[6]=l2[7]=0.8f;l2[8]=1.0f;
   l2[13]=-12.0f+4.0f*q;l2[14]=3.0f;l2[15]=8.0f+3.0f*q;l2[19]=25.0f;l2[21]=1.0f;l2[22]=0.05f;
   ((F2P)VT(dev,DEV_SETLIGHT))(dev,(U32)q,l2);((F2)VT(dev,DEV_LIGHTON))(dev,(U32)q,1);}
  rs(29,1);rs(142,0);}
 rs(137,1);rs(139,0xFF303030UL);rs(145,0);rs(147,0);rs(148,0);rs(22,g_mode==11&&!g_nocull?3UL:1UL);rs(28,0);if(g_mode==11){rs(7,1);rs(14,1);} /* v1.35: and ZWRITEENABLE (base_states turns it off too: v1.34's test without writes changed nothing). v1.34: 6l depth-tested (base_states turns ZENABLE off; without it the CULL_NONE frame draws back faces over front ones, T682-T694: about 39% of the pixels on both devices) */ /* v1.31: 6l, back faces culled (D3DCULL_CCW: the tori are closed, about half their triangles face away) */
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);tss(0,1,1);tss(0,4,1); /* stage 0 off: the diffuse colour alone, as the High Polygon Count test's dragons */
 ((F1)VT(dev,DEV_SETVS))(dev,0x152);
 for(k=0;k<30;k++){float c,sn;t_sincos(0.4f*k,&sn,&c);
  memcpy(w,id,sizeof(w));w[0]=c;w[2]=-sn;w[4]=sn*0.3f;w[5]=0.95f;w[6]=c*0.3f;w[8]=sn;w[10]=c;
  w[12]=-12.0f+(float)(k%6)*4.8f;w[13]=-4.0f+(float)(k/6)*2.0f;w[14]=12.0f+(float)(k%5)*3.0f;
  ((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)w);
  ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DIPUP))(dev,4,0,(TN+1)*(TN+1),TN*TN*2,t_ix,101,t_vb,sizeof(GV));fchk("a torus's DrawIndexedPrimitiveUP");}
 ((F0)VT(dev,DEV_END))(dev);tss(0,1,4);tss(0,4,2);
 if(g_mode>=10){for(q=1;q<8;q++)((F2)VT(dev,DEV_LIGHTON))(dev,(U32)q,0);rs(29,0);}
 rs(22,1);
 rs(137,0);((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
}
/* v1.37 (driver .717): 6M. projected texture coordinates on large triangles, each case drawn projected and again in a form that gives
 * the same coordinates without the projected layout (both of VXTLP's devices end in the driver's rasterizer, so the software device is no
 * reference for the rasterizer): A camera-space position COUNT3 | PROJECTED q = 1 against the floor's plain coordinates; R the reflection
 * vector COUNT3 | PROJECTED q = 1 with LOCALVIEWER (3DMark 2001's water) against COUNT2; B q = the depth against pretransformed u/q, v/q
 * with one rhw; D stage 1 projected against COUNT2. A floor of two triangles from 1.2 to 40 deep, checkered; a case matches at 2% of the
 * reference's lit pixels or fewer. v1.38 (.718): one round, no switch (.717's rhw multiply, TEXDBG BC 44D000x, broke every case on the
 * board, 69-81% of the pixels, and is gone: the engine corrects projected coordinates for perspective itself). Pictures 6M<case><S|H>R.BMP
 * and 6M<case><S|H>.BMP when a case differs. VXTLP PROJ runs 6M and 6N alone. */
typedef struct {float x,y,z,nx,ny,nz;U32 c;float u,v;} FV; /* FVF 0x152 */
static void *pm_tex(U32 a,U32 b) {void *tx=0;LR lr;int x,y; /* 64x64, 8x8 squares */
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,64,64,1,0,22,1,&tx)||!tx)return 0;
 if(!((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(tx,16))(tx,0,&lr,0,0)){for(y=0;y<64;y++){U32 *r=(U32*)((char*)lr.bits+y*lr.pitch);for(x=0;x<64;x++)r[x]=((x>>3)^(y>>3))&1?a:b;}((F1)VT(tx,17))(tx,0);}
 return tx;}
static const float pm_fx[4]={-1.1f,1.1f,-35.0f,35.0f},pm_fz[4]={1.2f,1.2f,40.0f,40.0f};static const int pm_tri[6]={0,2,3,0,3,1};
static void pm_draw(int c,int ref,void *t0,void *t1) { /* c: 0 A, 1 R, 2 B, 3 D; ref: the reference form */
 static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};float pr[16],ma[16],mr[16],mb[16],m1[16];FV v[6];TL t[6];
 int k;const float zn=0.5f,zf=100.0f,Q=zf/(zf-zn);
 memset(pr,0,sizeof(pr));pr[0]=1;pr[5]=1;pr[10]=Q;pr[11]=1;pr[14]=-Q*zn; /* 90 degrees, near 0.5, far 100 */
 memset(ma,0,sizeof(ma));ma[0]=0.5f;ma[9]=0.5f;ma[14]=1; /* (x, y, z, 1) -> (x/2, z/2, 1) */
 memset(mr,0,sizeof(mr));mr[0]=2;mr[9]=2;mr[12]=0.5f;mr[13]=0.5f;mr[14]=1; /* R -> (2 Rx + 0.5, 2 Rz + 0.5, 1) */
 memset(mb,0,sizeof(mb));mb[0]=4;mb[5]=4;mb[10]=1; /* (4x, 4y, z): divided by the depth */
 memcpy(m1,ma,sizeof(m1));m1[12]=0.25f; /* stage 1: A's mapping moved half a check */
 for(k=0;k<6;k++){FV *q=&v[k];int i=pm_tri[k];q->x=pm_fx[i];q->y=-1.0f;q->z=pm_fz[i];q->nx=0;q->ny=1;q->nz=0;q->c=0xFFFFFFFFUL;q->u=pm_fx[i]*0.5f;q->v=pm_fz[i]*0.5f;}
 p7_begin();((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)pr);
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,t0);tss(0,1,2);tss(0,2,2);tss(0,16,2);tss(0,17,2);tss(0,18,0);
 if(c==0&&!ref){tss(0,11,0x20000UL);tss(0,24,3|256);((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)ma);}
 if(c==1){tss(0,11,0x30000UL);tss(0,24,ref?2:3|256);rs(142,1);((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)mr);}
 if(c==2&&!ref){tss(0,11,0x20000UL);tss(0,24,3|256);((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)mb);}
 if(c==3){((F2P)VT(dev,DEV_SETTEX))(dev,1,t1);tss(1,1,4);tss(1,2,2);tss(1,3,1);tss(1,16,2);tss(1,17,2);tss(1,18,0);tss(1,11,0x20000UL);tss(1,24,ref?2:3|256);((F2P)VT(dev,DEV_SETXF))(dev,17,(void*)m1);}
 if(c==2&&ref){U32 vx=0,vy=0,vw=WIN,vh=WIN; /* the screen position of each corner (this projection, the viewport), u v = 4x/z, 4y/z, one rhw */
  if(g_vp){vx=40;vy=40;vw=WIN-80;vh=WIN-80;}
  for(k=0;k<6;k++){TL *q=&t[k];int i=pm_tri[k];float x=pm_fx[i],y=-1.0f,z=pm_fz[i];q->x=(float)vx+(x/z+1.0f)*0.5f*(float)vw;q->y=(float)vy+(1.0f-y/z)*0.5f*(float)vh;q->z=(Q*z-Q*zn)/z;q->w=1.0f;q->c=0xFFFFFFFFUL;q->u=4.0f*x/z;q->v=4.0f*y/z;}
  ((F1)VT(dev,DEV_SETVS))(dev,0x144);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,2,t,sizeof(TL));}
 else{((F1)VT(dev,DEV_SETVS))(dev,0x152);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,2,v,sizeof(FV));}
 tss(0,11,0);tss(0,24,0);tss(1,11,1);tss(1,24,0);tss(1,1,1);rs(142,0);((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,17,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F2P)VT(dev,DEV_SETTEX))(dev,1,0);((F1)VT(dev,DEV_SETVS))(dev,0x42);
 ((F0)VT(dev,DEV_END))(dev);}
static void test_proj(int sw) {
 static const char *const nm[4]={"A (camera-space position, COUNT3 PROJECTED, q = 1)","R (reflection vector, COUNT3 PROJECTED, q = 1: the water)","B (camera-space position projected, q = the depth: 6i's form)","D (two stages, stage 1 projected, q = 1)"},lt[4]={'A','R','B','D'};
 static U32 rf[WIN*WIN];int lr,d,c,ok;char nmb[16];
 void *t0=pm_tex(0xFFF0C040UL,0xFF3050E0UL),*t1=pm_tex(0xFFFFFFFFUL,0xFF808080UL);
 say("6M. %s: projected texture coordinates on large triangles (a floor from 1.2 to 40 deep), each against the same coordinates drawn without the projected layout\n",sw?"software vertex processing (the runtime's s t q to the driver)":"hardware vertex processing (the driver's T&L)");
 if(!t0||!t1){wrong("6M: the checker textures were not made");goto out;}
 for(c=0;c<4;c++){pm_draw(c,1,t0,t1);ok=read_bb(rf);present();lr=ok?lit(rf):0;if(!lr){say("   %s: the reference lit 0\n",nm[c]);wrong("6M: the reference drew nothing");continue;}
  pm_draw(c,0,t0,t1);ok=read_bb(cur);present();d=ok?ndiff(rf,cur):-1;
  say("   %s: lit %d, %d pixels differ (%.1f%%)\n",nm[c],lr,d,d<0?0.0:100.0*d/lr);
  if(d<0||d*100>lr*2){sprintf(nmb,"6M%c%cR.BMP",lt[c],sw?'S':'H');bmp(nmb,rf);sprintf(nmb,"6M%c%c.BMP",lt[c],sw?'S':'H');if(d>=0)bmp(nmb,cur);wrong("6M: a projected case is not its reference's picture");}}
 out:if(t0)((F0)VT(t0,D3D_REL))(t0);if(t1)((F0)VT(t1,D3D_REL))(t1);
}
/* v1.38 (driver .718): 6P. deep floors: depth varying thousands of times across one triangle, the case two outside reviews named (VXTLP
 * 6M's floor varied 33 times; Dragothic's water near the camera far more), and a floor across the near plane and the screen's sides (the
 * third review: the clipper's new vertices). Each case drawn as 2 huge triangles and again as a 32x32 grid of the same plane (rows even in
 * screen height), the grid the reference; and, on the hardware device, against the software device's 2-triangle picture (the runtime clips).
 *   P2 plain coordinates (u = x/4, v = z/4) from 1.02 to 3,000 deep        P3 the same through COUNT3 | PROJECTED, q = 1 (the water's layout)
 *   PB P3 blended (SRCALPHA / INVSRCALPHA, alpha 0x80)                       PM P2 with a mip chain (a colour a level: the level picked shows)
 *   PQ P3 with the mip chain                                                 NC P3 across the near plane and every side, -3 to 60 deep
 *   NA (.719) NC blended with a strong vertex alpha and colour gradient and no texture: the clipper's new vertices' colour and alpha (an
 *      outside review: tn_lerp takes them at the clip-space fraction, the engine shades screen-linearly); judged only against the
 *      software device's (the runtime's own clipping), the grid differing by design
 * A case matches at 2% of its reference's lit pixels or fewer. Pictures 6P<case><S|H><G|T>.BMP (grid, two triangles) when one differs. */
typedef struct {float x,y,z;U32 c;float u,v;} DV; /* FVF 0x142 */
static DV dp_v[33*33];static unsigned short dp_ix[32*32*6];static U32 dp_sw[7][WIN*WIN];static int dp_have[7];
static void *dp_tex(int mips) {void *tx=0;int lv,x,y,n=mips?7:1;static const U32 lc[7]={0,0xFF20C020UL,0xFFC02020UL,0xFF2020C0UL,0xFFC0C020UL,0xFF20C0C0UL,0xFFC020C0UL}; /* 64x64; level k > 0 a flat colour of its own */
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,64,64,(U32)n,0,22,1,&tx)||!tx)return 0;
 for(lv=0;lv<n;lv++){LR lr;int w=64>>lv;if(((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(tx,16))(tx,(U32)lv,&lr,0,0))continue;
  for(y=0;y<w;y++){U32 *r=(U32*)((char*)lr.bits+y*lr.pitch);for(x=0;x<w;x++)r[x]=lv?lc[lv]:((x>>3)^(y>>3))&1?0xFFF0C040UL:0xFF3050E0UL;}((F1)VT(tx,17))(tx,(U32)lv);}
 return tx;}
static void dp_draw(int c,int grid,void *tx) {
 static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};float pr[16],ma[16];const float zn=1.0f,zf=5000.0f,Q=zf/(zf-zn);
 const int nc=c>=5,N=grid?32:1,proj=c==1||c==2||c==4||c==5||c==7||c==8;const float y0=nc?-0.3f:-1.0f,z0=c==7?1.02f:c==8?0.8f:nc?-3.0f:1.02f,z1=nc?60.0f:3000.0f,xs=nc?80.0f:0.95f,xn=c==8?0.05f:xs,xf=c==8?40.0f:xs;int i,j,k=0;
 /* .720: 7 NS across the sides only (1.02 to 60 deep), 8 NN across the near plane only (0.8 to 60, a trapezoid inside the sides), 9 NC with plain coordinates */
 memset(pr,0,sizeof(pr));pr[0]=1;pr[5]=1;pr[10]=Q;pr[11]=1;pr[14]=-Q*zn;
 memset(ma,0,sizeof(ma));ma[0]=0.25f;ma[9]=0.25f;ma[14]=1; /* (x, y, z, 1) -> (x/4, z/4, 1) */
 for(j=0;j<=N;j++)for(i=0;i<=N;i++){DV *q=&dp_v[j*(N+1)+i];float t=(float)j/N,s=(float)i/N,z=nc?z0+(z1-z0)*t:1.0f/(1.0f/z0+(1.0f/z1-1.0f/z0)*t),x=nc?(2*s-1)*(xn+(xf-xn)*t):(2*s-1)*xs*z;
  q->x=x;q->y=y0;q->z=z;q->c=c==2?0x80FFFFFFUL:0xFFFFFFFFUL;q->u=x*0.25f;q->v=z*0.25f;
  if(c==6){U32 a=(U32)(255.0f-215.0f*t),r=(U32)(255.0f*s),g=(U32)(255.0f*(1.0f-s));q->c=(a<<24)|(r<<16)|(g<<8)|0x40UL;}} /* .719: alpha 255 near to 40 far, red to green across */
 for(j=0;j<N;j++)for(i=0;i<N;i++){unsigned short a=(unsigned short)(j*(N+1)+i);dp_ix[k++]=a;dp_ix[k++]=(unsigned short)(a+N+1);dp_ix[k++]=(unsigned short)(a+N+2);dp_ix[k++]=a;dp_ix[k++]=(unsigned short)(a+N+2);dp_ix[k++]=(unsigned short)(a+1);}
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();rs(137,0);rs(22,1);
 ((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)pr);
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,tx);tss(0,1,4);tss(0,2,2);tss(0,3,0);tss(0,4,2);tss(0,5,0);tss(0,16,2);tss(0,17,2);tss(0,18,c==3||c==4?2:0);
 if(c==2||c==6){rs(27,1);rs(19,5);rs(20,6);}
 if(c==6){tss(0,1,2);tss(0,2,0);tss(0,4,2);tss(0,5,0);} /* .719: the diffuse colour and alpha alone (no texture read), blended over black */
 if(proj&&!grid){tss(0,11,0x20000UL);tss(0,24,3|256);((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)ma);} /* the grid, the reference, has the same values plain */
 ((F1)VT(dev,DEV_SETVS))(dev,0x142);
 ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DIPUP))(dev,4,0,(U32)((N+1)*(N+1)),(U32)(N*N*2),dp_ix,101,dp_v,sizeof(DV));
 tss(0,11,0);tss(0,24,0);tss(0,18,0);rs(27,0);((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F1)VT(dev,DEV_SETVS))(dev,0x42);
 ((F0)VT(dev,DEV_END))(dev);}
static void test_deep(int sw) {
 static const char *const nm[7]={"P2 plain coordinates, 1.02 to 3000 deep","P3 COUNT3 PROJECTED q = 1, 1.02 to 3000 deep","PB P3 blended, alpha 0x80","PM P2 with a mip chain","PQ P3 with a mip chain","NC P3 across the near plane and every side","NA NC blended, a strong vertex alpha and colour gradient"},*const ab[7]={"P2","P3","PB","PM","PQ","NC","NA"};
 static U32 g[WIN*WIN];void *t0=dp_tex(0),*t1=dp_tex(1);int c,lg,d,ds,ok;char n[16];
 say("6P. %s: deep floors (2 huge triangles against a 32x32 grid of the same plane)%s\n",sw?"software vertex processing":"hardware vertex processing (the driver's T&L)",sw?"":", and against the software device's 2 triangles");
 if(!t0||!t1){wrong("6P: the textures were not made");goto out;}
 for(c=0;c<7;c++){void *tx=c==3||c==4?t1:t0;
  dp_draw(c,1,tx);ok=read_bb(g);present();lg=ok?lit(g):0;
  dp_draw(c,0,tx);ok=ok&&read_bb(cur);present();d=ok?ndiff(g,cur):-1;
  if(sw){memcpy(dp_sw[c],cur,sizeof(cur));dp_have[c]=ok;}
  ds=!sw&&dp_have[c]?ndiff(dp_sw[c],cur):-1;
  say("   %-44s grid lit %6d; 2 triangles: %6d differ (%5.1f%%)",nm[c],lg,d,lg&&d>=0?100.0*d/lg:0.0);if(ds>=0)say("; against the software device's: %6d (%5.1f%%)",ds,lg?100.0*ds/lg:0.0);say("\n");
  if(!lg){wrong("6P: the grid drew nothing");continue;}
  /* v1.43 (driver .724): judged against the reference drawn here (wr_floor) where it has one (no mip chain, not NA): more than 3% of
   * its lit, not-aliased pixels off by more than 64 is wrong. T719-T723: the grid differed from 2 triangles by 3.5-5.5% on both devices
   * alike (aliasing where the far floor packs many texels in a pixel), so the grid only judges the mip cases, at 8%; NA differs from the
   * software device by design (its colours across a clip, .719) and is shown, not judged */
  {int rl=0,rb=-1,refd=c!=3&&c!=4&&c!=6&&!g_vp,bad;
   if(refd&&ok){static U32 rf[WIN*WIN];wr_floor(c==5?5:c,rf);rb=wr_gross(cur,rf,&rl);say("      against its own reference: %d of %d pixels off (%.1f%%)\n",rb,rl,rl?100.0*rb/rl:0.0);
    if(rl&&rb*100>rl*3){sprintf(n,"6P%s%cR.BMP",ab[c],sw?'S':'H');bmp(n,rf);}}
   bad=refd?(rb<0||rb*100>rl*3):(c==6?0:(d<0||d*100>lg*8));if(!refd&&c!=6&&ds>=0&&ds*100>lg*2)bad=1;if(refd&&ds>=0&&ds*100>lg*2)bad=1;
   if(bad){sprintf(n,"6P%s%cG.BMP",ab[c],sw?'S':'H');bmp(n,g);sprintf(n,"6P%s%cT.BMP",ab[c],sw?'S':'H');bmp(n,cur);
    wrong(refd&&(rb<0||rb*100>rl*3)?"6P: the 2 huge triangles are not the reference's picture":!refd&&(d<0||d*100>lg*8)?"6P: the 2 huge triangles are not the grid's picture":"6P: the driver's 2 triangles are not the software device's");}}}
 out:if(t0)((F0)VT(t0,D3D_REL))(t0);if(t1)((F0)VT(t1,D3D_REL))(t1);
}
/* v1.38 (driver .718): 6N. a water scene of 3DMark 2001's Dragothic kind, the driver's T&L against the runtime's (software vertex
 * processing: the runtime transforms, generates the coordinates and clips; both then go through the same rasterizer, which 6M checks).
 * Dragothic's water was large flat wrong-coloured triangles near the camera on the board while far water looked right. The scene: a
 * water plane of 32 large triangles in 8 draws (4 triangles each, tiles 120 units across), alpha-blended (SRCALPHA / INVSRCALPHA, the
 * vertex's diffuse alpha 0x57-0xB7), depth test LESSEQUAL, no depth write, stage 0 the camera-space reflection vector with LOCALVIEWER
 * through Dragothic's own texture matrix (COUNT3 | PROJECTED, q = 1), the normals slightly tilted per vertex as the recording's; an opaque
 * textured octahedron (the dragon) flying close in front of the camera, through the near plane and the screen's edges; the camera
 * coming down from 12 units to 0.25 over the water, pitching up and turning, over 24 frames: the water's triangles cross the near plane
 * and the sides more and more. Each frame compared with the software device's (pixels off by more than 8 in a channel); a frame matches at
 * 1% of its lit pixels or fewer. Pictures of the three worst frames: 6N<nn>S.BMP the software device's, 6N<nn>H.BMP the driver's. */
#define WN_FR 24
typedef struct {float x,y,z,nx,ny,nz;U32 c;float u,v;} WV;
static U32 wn_sw[WN_FR][WIN*WIN];static int wn_have;
static void *wn_tex(void) {void *tx=0;LR lr;int x,y; /* 256x256: a sky gradient (orange at the top to blue) crossed by a 32-texel checker */
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,256,256,1,0,22,1,&tx)||!tx)return 0;
 if(!((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(tx,16))(tx,0,&lr,0,0)){for(y=0;y<256;y++){U32 *r=(U32*)((char*)lr.bits+y*lr.pitch);for(x=0;x<256;x++){
   U32 R=240-y*3/4,G=140-y/4+(x>>3),B=40+y*3/4;if(((x>>5)^(y>>5))&1){R=R*3/4;G=G*3/4;B=B*3/4;}r[x]=0xFF000000UL|(R<<16)|(G<<8)|B;}}((F1)VT(tx,17))(tx,0);}
 return tx;}
static void wn_cam(int f,float *vw) { /* the view matrix of frame f */
 float t=(float)f/(WN_FR-1),cy=12.0f*(1.0f-t)+0.25f,cz=-60.0f+70.0f*t,cx=6.0f*t,pit=0.45f*(1.0f-t)-0.03f,yaw=0.35f*t,sp,cp,sy,cy2,F[3],R[3],U[3],E[3];int k;
 t_sincos(pit,&sp,&cp);t_sincos(yaw,&sy,&cy2);
 F[0]=sy*cp;F[1]=-sp;F[2]=cy2*cp;R[0]=cy2;R[1]=0;R[2]=-sy;U[0]=F[1]*R[2]-F[2]*R[1];U[1]=F[2]*R[0]-F[0]*R[2];U[2]=F[0]*R[1]-F[1]*R[0];E[0]=cx;E[1]=cy;E[2]=cz;
 memset(vw,0,64);for(k=0;k<3;k++){vw[k*4]=R[k];vw[k*4+1]=U[k];vw[k*4+2]=F[k];}
 vw[12]=-(R[0]*E[0]+R[1]*E[1]+R[2]*E[2]);vw[13]=-(U[0]*E[0]+U[1]*E[1]+U[2]*E[2]);vw[14]=-(F[0]*E[0]+F[1]*E[1]+F[2]*E[2]);vw[15]=1;}
static U32 wn_ttf=3|256,wn_filt=2,wn_blend=1,wn_qm=0;static void *wn_cubetx; /* v1.43: the water reads this cube map when set (TTF 0: the reflection vector itself) */ /* v1.42: 6W's variants (TEXTURETRANSFORMFLAGS, MAG/MINFILTER, ALPHABLENDENABLE of the water) */
static void wn_frame(int f,void *tx) {
 static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 static const float tm1[16]={0.373904f,0.0305479f,0,0, 0.039355f,-0.368455f,0,0, 0.0391257f,0.0786848f,0,0, 0.5f,0.5f,1,1}; /* Dragothic's */
 static const float tmq[16]={0.373904f,0.0305479f,0.30f,0, 0.039355f,-0.368455f,0.20f,0, 0.0391257f,0.0786848f,-0.25f,0, 0.5f,0.5f,1,1}; /* v1.43: q = 1 + 0.3 Rx + 0.2 Ry - 0.25 Rz, 0.4 to 1.6 */
 const float *tm=wn_qm?tmq:tm1;
 float pr[16],vw[16],w[16];WV v[24];int k,i,j,d;const float zn=1.0f,zf=1000.0f,Q=zf/(zf-zn),t=(float)f/(WN_FR-1);
 memset(pr,0,sizeof(pr));pr[0]=1.3f;pr[5]=1.3f;pr[10]=Q;pr[11]=1;pr[14]=-Q*zn;wn_cam(f,vw);
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();rs(137,0);rs(22,1);
 ((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)vw);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)pr);((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,tx);tss(0,1,4);tss(0,2,2);tss(0,3,0);tss(0,4,2);tss(0,5,0);tss(0,16,wn_filt);tss(0,17,wn_filt);tss(0,18,0);
 { /* the dragon: an octahedron 2.4 across, its own plain coordinates, opaque, close in front of the camera */
  static const float o[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};static const int fc[8][3]={{0,2,4},{2,1,4},{1,3,4},{3,0,4},{2,0,5},{1,2,5},{3,1,5},{0,3,5}};
  float s1,c1,s2,c2,P[3];t_sincos(t*7.0f,&s1,&c1);t_sincos(t*5.0f,&s2,&c2);
  P[0]=s1*2.2f;P[1]=-0.4f+c2*0.9f;P[2]=1.8f+1.6f*s2; /* in camera space: crosses the near plane (1) and the sides */
  for(k=0;k<8;k++)for(j=0;j<3;j++){WV *q=&v[k*3+j];const float *a=o[fc[k][j]];float cs[3];int m;for(m=0;m<3;m++)cs[m]=P[m]+a[m]*1.2f;
   q->x=cs[0];q->y=cs[1];q->z=cs[2];q->nx=a[0];q->ny=a[1];q->nz=a[2];q->c=0xFFFFFFFFUL;q->u=0.5f+0.5f*a[0];q->v=0.5f+0.5f*a[1]+0.25f*a[2];}
  ((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id); /* given in camera space */
  ((F1)VT(dev,DEV_SETVS))(dev,0x152);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,8,v,sizeof(WV));
  ((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)vw);}
 /* the water */
 rs(27,wn_blend);rs(19,5);rs(20,6);rs(14,0);rs(23,4);rs(142,1);
 if(wn_cubetx)((F2P)VT(dev,DEV_SETTEX))(dev,0,wn_cubetx);
 tss(0,11,0x30000UL);tss(0,24,wn_cubetx?0:wn_ttf);((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)tm);
 for(d=0;d<8;d++){int n=0;for(k=0;k<2;k++){int tile=d*2+k,tx0=tile%4,tz0=tile/4;float x0=-240.0f+tx0*120.0f,z0=-120.0f+tz0*120.0f;float cx[4],cz[4];static const int qi[6]={0,2,3,0,3,1};
   cx[0]=x0;cz[0]=z0;cx[1]=x0+120;cz[1]=z0;cx[2]=x0;cz[2]=z0+120;cx[3]=x0+120;cz[3]=z0+120;
   for(i=0;i<6;i++){WV *q=&v[n++];float x=cx[qi[i]],z=cz[qi[i]],sx,cxs,sz,czs,nx,ny,nz,l;U32 a;
    t_sincos(x*0.013f,&sx,&cxs);t_sincos(z*0.011f,&sz,&czs);nx=-0.07f+0.03f*sx;ny=0.976f;nz=-0.20f+0.03f*czs;l=(float)sqrt(nx*nx+ny*ny+nz*nz);
    a=0x57UL+((U32)((int)(x+300)*7+(int)(z+300)*3)%0x61UL);
    q->x=x;q->y=0;q->z=z;q->nx=nx/l;q->ny=ny/l;q->nz=nz/l;q->c=(a<<24)|0xFFFFFFUL;q->u=0;q->v=0;}}
  ((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,4,v,sizeof(WV));}
 tss(0,11,0);tss(0,24,0);rs(142,0);rs(27,0);rs(14,1);((F2P)VT(dev,DEV_SETXF))(dev,16,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F1)VT(dev,DEV_SETVS))(dev,0x42);
 ((F0)VT(dev,DEV_END))(dev);}
static void test_water(int sw) {
 void *tx=wn_tex();int f,ok,d[WN_FR],l[WN_FR],bad=0,worst[3]={-1,-1,-1};char n[16];
 say("6N. %s: a Dragothic-like water scene, 24 frames, the camera coming down to the water with a flying object through the near plane\n",sw?"software vertex processing (the runtime's T&L and clipping: the reference)":"hardware vertex processing (the driver's T&L), each frame against the software device's");
 if(!tx){wrong("6N: the texture was not made");return;}
 for(f=0;f<WN_FR;f++){wn_frame(f,tx);ok=read_bb(cur);present();l[f]=ok?lit(cur):0;
  if(sw){if(ok)memcpy(wn_sw[f],cur,sizeof(cur));d[f]=0;continue;}
  d[f]=ok&&wn_have?ndiff(wn_sw[f],cur):-1;}
 if(sw){wn_have=1;say("   frames drawn, lit pixels:");for(f=0;f<WN_FR;f++)say(" %d",l[f]);say("\n");for(f=0;f<WN_FR;f++)if(!l[f])wrong("6N: a frame drew nothing");((F0)VT(tx,D3D_REL))(tx);return;}
 if(!wn_have){say("   no software pictures to compare\n");((F0)VT(tx,D3D_REL))(tx);return;}
 say("   frame: pixels differing (%% of the software device's lit pixels)\n  ");
 for(f=0;f<WN_FR;f++){int ls=lit(wn_sw[f]);say(" %2d:%6d (%4.1f%%)%s",f,d[f],ls?100.0*d[f]/ls:0.0,f%6==5?"\n  ":"");
  if(d[f]<0||d[f]*100>ls){bad++;}
  {int k,m=f;for(k=0;k<3;k++){if(worst[k]<0||d[m]>d[worst[k]]){int tmp=worst[k];worst[k]=m;m=tmp;if(m<0)break;}}}}
 say("\n");
 for(f=0;f<3;f++)if(worst[f]>=0&&d[worst[f]]>0){wn_frame(worst[f],tx);if(read_bb(cur)){sprintf(n,"6N%02dH.BMP",worst[f]);bmp(n,cur);sprintf(n,"6N%02dS.BMP",worst[f]);bmp(n,wn_sw[worst[f]]);}present();}
 say("   %d of %d frames differ by more than 1%%%s\n",bad,WN_FR,bad?" (pictures of the worst three: 6N<frame>S.BMP the software device's, 6N<frame>H.BMP the driver's)":"");
 if(bad)wrong("6N: the driver's water scene is not the software device's picture");
 ((F0)VT(tx,D3D_REL))(tx);
}
/* v1.42 (driver .723): 6W. the water scene (6N's) judged against a picture drawn by Direct3D's rules in this program (wr_frame: no
 * runtime, no driver: the transform, the reflection vector with LOCALVIEWER, the texture matrix, clipping in clip space with the
 * attributes interpolated there, perspective-correct coordinates divided by q at each pixel, the texture's bilinear or point WRAP read,
 * SRCALPHA blending over black and the dragon). T721 (VX855): both devices' 6N pictures had large triangles in blue and orange stripes
 * (the texture read near t = 1) where this reference is the water's smooth brown (t 0.3-0.44). A pixel is counted wrong when its blue
 * minus red is more than 32 from the reference's (the stripes' signature; the 565 back buffer and rounding stay well inside it); a frame
 * is wrong past 3% of its lit pixels. Variants: the driver as it is (.723: a projected set with q = 1 everywhere sent plain), the same
 * with that off (VX_D3D_TestSet 2: the projected layout as .722 sent it), the application's own two-coordinate form (TEXTURETRANSFORMFLAGS
 * COUNT2, no divide: the same coordinates, q = 1), and with the old layout: point filtering, and the water opaque. The old-layout rows
 * are shown, not judged: they say what the engine's projected layout trips on. */
#define WR_W WIN
/* v1.43 (driver .724): the reference renderer shared by 6W, 6P and 6Q (wr_tex_id: 0 6N's 256x256 sky texture, 1 6P's 64x64 checker;
 * wr_al marks the pixels where the texture is minified past a texel a pixel: aliasing there makes any two right rasterizers differ, so
 * the judges leave them out) */
static int wr_tex_id;static unsigned char wr_al[WR_W*WR_W];
static U32 wr_tex(int x,int y){U32 R,G,B;
 if(wr_tex_id==1){x&=63;y&=63;return ((x>>3)^(y>>3))&1?0xF0C040UL:0x3050E0UL;}
 R=240-y*3/4;G=140-y/4+(x>>3);B=40+y*3/4;if(((x>>5)^(y>>5))&1){R=R*3/4;G=G*3/4;B=B*3/4;}return (R<<16)|(G<<8)|B;}
static int wr_fl(double v){int i=(int)v;return (double)i>v?i-1:i;}
static void wr_sample(double u,double v,int pt,double *c){
 const int ts=wr_tex_id==1?64:256,tm=ts-1;double x=u*ts-0.5,y=v*ts-0.5;int x0,y0,k;
 if(pt){int xi=wr_fl(u*ts)&tm,yi=wr_fl(v*ts)&tm;U32 t=wr_tex(xi,yi);c[0]=(t>>16)&255;c[1]=(t>>8)&255;c[2]=t&255;return;}
 x0=wr_fl(x);y0=wr_fl(y);{double fx=x-x0,fy=y-y0;U32 a=wr_tex(x0&tm,y0&tm),b=wr_tex((x0+1)&tm,y0&tm),cc=wr_tex(x0&tm,(y0+1)&tm),d=wr_tex((x0+1)&tm,(y0+1)&tm);
  for(k=0;k<3;k++){int sh=16-8*k;c[k]=((a>>sh)&255)*(1-fx)*(1-fy)+((b>>sh)&255)*fx*(1-fy)+((cc>>sh)&255)*(1-fx)*fy+((d>>sh)&255)*fx*fy;}}}
/* v1.43: 6W's cube map: six 64x64 faces (+X -X +Y -Y +Z -Z), each its own colour with a gentle gradient, so a face chosen wrong is
 * far off and filtering is not; the reference picks the face and its s, t as Direct3D does and filters within the face (edges clamped) */
static U32 wr_cubetex(int f,int x,int y){static const U32 b[6][3]={{200,40,40},{40,180,40},{40,60,200},{200,190,40},{40,190,190},{190,40,190}};
 U32 r=b[f][0]+(U32)(x>>2),g=b[f][1]+(U32)(y>>2),bl=b[f][2]+(U32)((x+y)>>3);return (r<<16)|(g<<8)|bl;}
static void wr_cube(double rx,double ry,double rz,double *c){
 double ax=rx<0?-rx:rx,ay=ry<0?-ry:ry,az=rz<0?-rz:rz,sc,tc,ma,x,y;int f,x0,y0,k;
 if(ax>=ay&&ax>=az){f=rx>=0?0:1;ma=ax;sc=rx>=0?-rz:rz;tc=-ry;}
 else if(ay>=az){f=ry>=0?2:3;ma=ay;sc=rx;tc=ry>=0?rz:-rz;}
 else{f=rz>=0?4:5;ma=az;sc=rz>=0?rx:-rx;tc=-ry;}
 x=((sc/ma+1)/2)*64-0.5;y=((tc/ma+1)/2)*64-0.5;x0=wr_fl(x);y0=wr_fl(y);
 {double fx=x-x0,fy=y-y0;int xa=x0<0?0:x0>63?63:x0,xb=x0+1<0?0:x0+1>63?63:x0+1,ya=y0<0?0:y0>63?63:y0,yb=y0+1<0?0:y0+1>63?63:y0+1;
  U32 a=wr_cubetex(f,xa,ya),b=wr_cubetex(f,xb,ya),cc=wr_cubetex(f,xa,yb),d=wr_cubetex(f,xb,yb);
  for(k=0;k<3;k++){int sh=16-8*k;c[k]=((a>>sh)&255)*(1-fx)*(1-fy)+((b>>sh)&255)*fx*(1-fy)+((cc>>sh)&255)*(1-fx)*fy+((d>>sh)&255)*fx*fy;}}}
static int wr_cubeon;
typedef struct {double c[4],a[4];} WRV;
static double wr_pl(const WRV *v,int k){const double *c=v->c;switch(k){case 0:return c[2];case 1:return c[3]-c[2];case 2:return c[3]+c[0];case 3:return c[3]-c[0];case 4:return c[3]+c[1];default:return c[3]-c[1];}}
static int wr_clip(WRV *p,int n){WRV o[16];int k,i,m;
 for(k=0;k<6&&n;k++){m=0;for(i=0;i<n;i++){const WRV *a=&p[i],*b=&p[(i+1)%n];double da=wr_pl(a,k),db=wr_pl(b,k);
   if(da>=0&&m<16)o[m++]=*a;
   if((da>=0)!=(db>=0)&&m<16){double t=da/(da-db);int j;for(j=0;j<4;j++){o[m].c[j]=a->c[j]+(b->c[j]-a->c[j])*t;o[m].a[j]=a->a[j]+(b->a[j]-a->a[j])*t;}m++;}}
  n=m;for(i=0;i<n;i++)p[i]=o[i];}
 return n;}
static double wr_img[WR_W*WR_W][3];
static void wr_tri(const WRV *t0,const WRV *t1,const WRV *t2,int pt,int blend){
 double sx[3],sy[3],rw[3],ar;const WRV *t[3];int k,xa,xb,ya,yb,x,y;const int ts=wr_tex_id==1?64:256;t[0]=t0;t[1]=t1;t[2]=t2;
 for(k=0;k<3;k++){double w=t[k]->c[3];sx[k]=(t[k]->c[0]/w+1)*WR_W/2;sy[k]=(1-t[k]->c[1]/w)*WR_W/2;rw[k]=1/w;}
 ar=(sx[1]-sx[0])*(sy[2]-sy[0])-(sx[2]-sx[0])*(sy[1]-sy[0]);if(ar<1e-9&&ar>-1e-9)return;
 xa=wr_fl(sx[0]);xb=xa;ya=wr_fl(sy[0]);yb=ya;for(k=1;k<3;k++){int a=wr_fl(sx[k]),b=wr_fl(sy[k]);if(a<xa)xa=a;if(a>xb)xb=a;if(b<ya)ya=b;if(b>yb)yb=b;}
 xb++;yb++;if(xa<0)xa=0;if(ya<0)ya=0;if(xb>WR_W-1)xb=WR_W-1;if(yb>WR_W-1)yb=WR_W-1;
 for(y=ya;y<=yb;y++)for(x=xa;x<=xb;x++){double w0=((sx[1]-x)*(sy[2]-y)-(sx[2]-x)*(sy[1]-y))/ar,w1=((sx[2]-x)*(sy[0]-y)-(sx[0]-x)*(sy[2]-y))/ar,w2=1-w0-w1,c[3],*o=wr_img[y*WR_W+x],uv[3][2];int j;
  if(w0<0||w1<0||w2<0)continue;
  for(j=0;j<3;j++){double px=x+(j==1),py=y+(j==2),b0=((sx[1]-px)*(sy[2]-py)-(sx[2]-px)*(sy[1]-py))/ar,b1=((sx[2]-px)*(sy[0]-py)-(sx[0]-px)*(sy[2]-py))/ar,b2=1-b0-b1,r=b0*rw[0]+b1*rw[1]+b2*rw[2],
   s=(b0*t[0]->a[0]*rw[0]+b1*t[1]->a[0]*rw[1]+b2*t[2]->a[0]*rw[2])/r,tt=(b0*t[0]->a[1]*rw[0]+b1*t[1]->a[1]*rw[1]+b2*t[2]->a[1]*rw[2])/r,q=(b0*t[0]->a[2]*rw[0]+b1*t[1]->a[2]*rw[1]+b2*t[2]->a[2]*rw[2])/r;
   uv[j][0]=s/q;uv[j][1]=tt/q;}
  {double dx=(uv[1][0]-uv[0][0])*ts,dy=(uv[1][1]-uv[0][1])*ts,ex=(uv[2][0]-uv[0][0])*ts,ey=(uv[2][1]-uv[0][1])*ts;wr_al[y*WR_W+x]=(dx*dx+dy*dy>1.0||ex*ex+ey*ey>1.0);}
  if(wr_cubeon&&blend){double r=w0*rw[0]+w1*rw[1]+w2*rw[2];wr_cube((w0*t[0]->a[0]*rw[0]+w1*t[1]->a[0]*rw[1]+w2*t[2]->a[0]*rw[2])/r,(w0*t[0]->a[1]*rw[0]+w1*t[1]->a[1]*rw[1]+w2*t[2]->a[1]*rw[2])/r,(w0*t[0]->a[2]*rw[0]+w1*t[1]->a[2]*rw[1]+w2*t[2]->a[2]*rw[2])/r,c);}
  else wr_sample(uv[0][0],uv[0][1],pt,c);
  {double al=blend?(w0*t[0]->a[3]+w1*t[1]->a[3]+w2*t[2]->a[3])/255.0:1.0;for(k=0;k<3;k++)o[k]=c[k]*al+o[k]*(1-al);}
 }}
static void wr_out(U32 *out){int k,j;for(k=0;k<WR_W*WR_W;k++){U32 c=0;for(j=0;j<3;j++){double v=wr_img[k][j]+0.5;int iv=v<0?0:v>255?255:(int)v;c|=(U32)iv<<(16-8*j);}out[k]=c;}}
static void wr_frame(int f,int pt,int blend,U32 *out){
 static const float tm1[16]={0.373904f,0.0305479f,0,0, 0.039355f,-0.368455f,0,0, 0.0391257f,0.0786848f,0,0, 0.5f,0.5f,1,1},tmq[16]={0.373904f,0.0305479f,0.30f,0, 0.039355f,-0.368455f,0.20f,0, 0.0391257f,0.0786848f,-0.25f,0, 0.5f,0.5f,1,1};const float *tm=wn_qm?tmq:tm1;
 static const float o6[6][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}};static const int fc[8][3]={{0,2,4},{2,1,4},{1,3,4},{3,0,4},{2,0,5},{1,2,5},{3,1,5},{0,3,5}};
 static const int qi[6]={0,2,3,0,3,1};
 float vw[16];const double zn=1.0,zf=1000.0,Q=zf/(zf-zn),t=(double)f/(WN_FR-1);WRV p[16];int k,j,d,i,n,m;
 #define PRJ(cs,o) {o[0]=1.3*cs[0];o[1]=1.3*cs[1];o[2]=Q*cs[2]-Q*zn;o[3]=cs[2];}
 memset(wr_img,0,sizeof(wr_img));memset(wr_al,0,sizeof(wr_al));wr_tex_id=0;wn_cam(f,vw);
 {float s1,c1,s2,c2;double P[3];t_sincos((float)(t*7.0),&s1,&c1);t_sincos((float)(t*5.0),&s2,&c2);P[0]=s1*2.2;P[1]=-0.4+c2*0.9;P[2]=1.8+1.6*s2;
  for(k=0;k<8;k++){for(j=0;j<3;j++){const float *a=o6[fc[k][j]];double cs[3];int q;for(q=0;q<3;q++)cs[q]=P[q]+a[q]*1.2;PRJ(cs,p[j].c);p[j].a[0]=0.5+0.5*a[0];p[j].a[1]=0.5+0.5*a[1]+0.25*a[2];p[j].a[2]=1;p[j].a[3]=255;}
   n=wr_clip(p,3);for(i=1;i+1<n;i++)wr_tri(&p[0],&p[i],&p[i+1],pt,0);}}
 for(d=0;d<8;d++)for(k=0;k<2;k++){int tile=d*2+k,tx0=tile%4,tz0=tile/4;double x0=-240.0+tx0*120.0,z0=-120.0+tz0*120.0,cx[4],cz[4];WRV v6[6];
  cx[0]=x0;cz[0]=z0;cx[1]=x0+120;cz[1]=z0;cx[2]=x0;cz[2]=z0+120;cx[3]=x0+120;cz[3]=z0+120;
  for(i=0;i<6;i++){double x=cx[qi[i]],z=cz[qi[i]],nx,ny,nz,l,pc[3],nc[3],E[3],R[3],ne,el;float sx,cxs,sz,czs;U32 a;int q;
   t_sincos((float)(x*0.013),&sx,&cxs);t_sincos((float)(z*0.011),&sz,&czs);nx=-0.07+0.03*sx;ny=0.976;nz=-0.20+0.03*czs;l=sqrt(nx*nx+ny*ny+nz*nz);nx/=l;ny/=l;nz/=l;
   a=0x57UL+((U32)((int)(x+300)*7+(int)(z+300)*3)%0x61UL);
   for(q=0;q<3;q++){pc[q]=x*vw[q]+0*vw[4+q]+z*vw[8+q]+vw[12+q];nc[q]=nx*vw[q]+ny*vw[4+q]+nz*vw[8+q];}
   el=sqrt(pc[0]*pc[0]+pc[1]*pc[1]+pc[2]*pc[2]);for(q=0;q<3;q++)E[q]=-pc[q]/el;ne=E[0]*nc[0]+E[1]*nc[1]+E[2]*nc[2];for(q=0;q<3;q++)R[q]=2*ne*nc[q]-E[q];
   PRJ(pc,v6[i].c);for(q=0;q<3;q++)v6[i].a[q]=wr_cubeon?R[q]:R[0]*tm[q]+R[1]*tm[4+q]+R[2]*tm[8+q]+tm[12+q];v6[i].a[3]=(double)a;}
  for(i=0;i<6;i+=3){p[0]=v6[i];p[1]=v6[i+1];p[2]=v6[i+2];n=wr_clip(p,3);for(m=1;m+1<n;m++)wr_tri(&p[0],&p[m],&p[m+1],pt,blend);}}
 #undef PRJ
 wr_out(out);
}
/* v1.43: 6P / 6Q's floors (dp_draw's two-triangle form, case c) by Direct3D's rules: the checker, bilinear, WRAP, no mip chain */
static void wr_floor(int c,U32 *out){
 const int nc=c>=5;const double zn=1.0,zf=5000.0,Q=zf/(zf-zn),y0=nc?-0.3:-1.0,z0=c==7?1.02:c==8?0.8:nc?-3.0:1.02,z1=nc?60.0:3000.0,xs=nc?80.0:0.95,xn=c==8?0.05:xs,xf=c==8?40.0:xs;
 WRV v[4],p[16];int i,j,n,m;static const int tri[2][3]={{0,2,3},{0,3,1}};
 memset(wr_img,0,sizeof(wr_img));memset(wr_al,0,sizeof(wr_al));wr_tex_id=1;
 for(j=0;j<2;j++)for(i=0;i<2;i++){WRV *q=&v[j*2+i];double t=j,s2=i,z=nc?z0+(z1-z0)*t:1.0/(1.0/z0+(1.0/z1-1.0/z0)*t),x=nc?(2*s2-1)*(xn+(xf-xn)*t):(2*s2-1)*xs*z;
  q->c[0]=x;q->c[1]=y0;q->c[2]=Q*z-Q*zn;q->c[3]=z;q->a[0]=x*0.25;q->a[1]=z*0.25;q->a[2]=1;q->a[3]=c==2?128:255;}
 for(i=0;i<2;i++){for(j=0;j<3;j++)p[j]=v[tri[i][j]];n=wr_clip(p,3);for(m=1;m+1<n;m++)wr_tri(&p[0],&p[m],&p[m+1],0,c==2);}
 wr_out(out);}
/* pixels off by more than 64 in a channel where the reference is not aliased, and the lit pixels counted there */
static int wr_gross(const U32 *b,const U32 *r,int *lit_out){int n=0,l=0,k,j;for(k=0;k<WIN*WIN;k++){int bad=0;if(wr_al[k])continue;if(r[k])l++;
  for(j=0;j<24;j+=8){int d=(int)((b[k]>>j)&255)-(int)((r[k]>>j)&255);if(d>64||d<-64)bad=1;}n+=bad;}
 if(lit_out)*lit_out=l;return n;}
static int wr_bad(const U32 *b,const U32 *r){int n=0,k;for(k=0;k<WIN*WIN;k++){int hb=(int)(b[k]&255)-(int)((b[k]>>16)&255),hr=(int)(r[k]&255)-(int)((r[k]>>16)&255),d=hb-hr;if(d>32||d<-32)n++;}return n;}
static U32 wr_ref[WIN*WIN];
static void test_wref(int sw) {
 static const int fr0=10,fr1=23;
 static const struct {const char *nm;int dev,q1,ttf,pt,bl,judge,qm,cube;} vr[]={
  {"as it is (.723: q = 1 sent plain)",0,1,0x103,0,1,1,0},{"the projected layout (.722)",0,0,0x103,0,1,0,0},{"the application's COUNT2 form",0,1,2,0,1,1,0},
  {"projected layout, point filter",0,0,0x103,1,1,0,0},{"projected layout, water opaque",0,0,0x103,0,0,0,0},
  {"q 0.4-1.6 folded into W (.724)",0,1,0x103,0,1,1,1},{"q 0.4-1.6, the projected layout",0,0,0x103,0,1,0,1},{"a cube map (the reflection vector)",0,1,0,0,1,1,0,1},
  {"as it is (.723: q = 1 sent plain)",1,1,0x103,0,1,1,0},{"the projected layout (.722)",1,0,0x103,0,1,0,0},
  {"q 0.4-1.6 folded into W (.724)",1,1,0x103,0,1,1,1},{"q 0.4-1.6, the projected layout",1,0,0x103,0,1,0,1},{"a cube map (the reflection vector)",1,1,0,0,1,1,0,1}};
 DWORD (WINAPI *ts)(DWORD,DWORD)=0;HMODULE h=GetModuleHandleA("VXDDRAW.DLL");void *tx=wn_tex(),*cubetx=0;int v,f,ok;char n[16];DWORD q0=1;
 say("6W. %s: the water scene against a reference drawn by Direct3D's rules in this program (pixels whose blue minus red is more than 32 off, %% of the lit ones), frames %d-%d\n",sw?"software vertex processing (the runtime's T&L, the driver's rasterizer)":"hardware vertex processing (the driver's T&L)",fr0,fr1);
 if(!tx){wrong("6W: the texture was not made");return;}
 if(!((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,void**))VT(dev,22))(dev,64,1,0,22,1,&cubetx)&&cubetx){int j,x,y;LR lr;
  for(j=0;j<6;j++)if(!((HRESULT(__stdcall*)(void*,U32,U32,LR*,void*,U32))VT(cubetx,16))(cubetx,(U32)j,0,&lr,0,0)){
   for(y=0;y<64;y++){U32 *r=(U32*)((char*)lr.bits+y*lr.pitch);for(x=0;x<64;x++)r[x]=0xFF000000UL|wr_cubetex(j,x,y);}((F2)VT(cubetx,17))(cubetx,(U32)j,0);}}
 else cubetx=0;
 ts=h?(DWORD (WINAPI *)(DWORD,DWORD))GetProcAddress(h,"VX_D3D_TestSet"):0;
 if(ts){q0=ts(2,0xFFFFFFFFUL);if(q0==0xFFFFFFFFUL)ts=0;}
 for(v=0;v<(int)(sizeof(vr)/sizeof(vr[0]));v++){int bad=0,worst=-1,wd=-1,d[WN_FR];
  if(vr[v].dev!=sw)continue;
  if(!vr[v].q1&&!ts){say("   %-36s not run (VX_D3D_TestSet 2: a driver before .723)\n",vr[v].nm);continue;}
  if(vr[v].qm&&vr[v].q1&&ts&&ts(3,0xFFFFFFFFUL)==0xFFFFFFFFUL){say("   %-36s not run (a driver before .724)\n",vr[v].nm);continue;}
  if(vr[v].cube&&!cubetx){say("   %-36s not run (CreateCubeTexture refused)\n",vr[v].nm);continue;}
  if(ts)ts(2,(DWORD)vr[v].q1);wn_ttf=vr[v].ttf;wn_filt=vr[v].pt?1:2;wn_blend=vr[v].bl;wn_qm=(U32)vr[v].qm;wn_cubetx=vr[v].cube?cubetx:0;wr_cubeon=vr[v].cube;
  say("   %-36s",vr[v].nm);
  for(f=fr0;f<=fr1;f++){int l;wr_frame(f,vr[v].pt,vr[v].bl,wr_ref);l=lit(wr_ref);wn_frame(f,tx);ok=read_bb(cur);present();
   d[f]=ok?wr_bad(cur,wr_ref):-1;say(" %2d:%4.1f%%",f,ok&&l?100.0*d[f]/l:-1.0);if(f==16)say("\n   %-36s","");
   if(!ok||!l||d[f]*100>l*3)bad++; /* v1.44 (driver .726, review): a frame not read back, or a reference that drew nothing, is wrong too */if(ok&&d[f]>wd){wd=d[f];worst=f;}}
  say("\n");
  if(worst>=0&&wd>0){wr_frame(worst,vr[v].pt,vr[v].bl,wr_ref);wn_frame(worst,tx);if(read_bb(cur)){sprintf(n,"6W%c%d%02d.BMP",sw?'S':'H',v,worst);bmp(n,cur);sprintf(n,"6WR%d%02d.BMP",v,worst);bmp(n,wr_ref);}present();}
  if(vr[v].judge&&bad){char m[96];sprintf(m,"6W: %s: %d frame(s) past 3%% wrong (%s)",vr[v].nm,bad,sw?"software":"hardware");wrong(m);}}
 if(ts)ts(2,q0);wn_ttf=3|256;wn_filt=2;wn_blend=1;wn_qm=0;wn_cubetx=0;wr_cubeon=0;if(cubetx)((F0)VT(cubetx,D3D_REL))(cubetx);
 ((F0)VT(tx,D3D_REL))(tx);
}
/* v1.40 (driver .720): 6Q. which part of the driver's path breaks a clipped projected floor (T719 on the VX855: 6P NC, P3 across the
 * near plane and every side, 35% of its pixels off against the software device's, far squares turned to stripes; 6N frames 14-22 the
 * same; host packets of that floor checked right vertex by vertex). Each case and four 6N frames drawn by the hardware device with the
 * T&L's guard band on and off (VXDDRAW.DLL VX_D3D_TestSet 1 in this process; off clips the sides to the viewport as the runtime does),
 * on / off / on / off / on; each picture against the software device's. T720 (VX855): NC 35.5% and NS 48.3% off with the band, 1.9% and
 * 0% without; NN 0%; strips or lists made no difference (.720's TEXDBG BC 44E switch, removed in .721). .721 leaves the band off for
 * records with a third coordinate: both settings must now match (a floor case past 3% is wrong). The 6N frames are shown, not judged:
 * reflection coordinates interpolated across triangles this large make the runtime's own picture differ from a reference. Cases: NC (P3 across near and sides), NS (P3 across the sides only), NN
 * (P3 across the near plane only), N2 (NC with plain coordinates: is it the projected layout?). Both switches put back as they were. */
static U32 cq_sw[4][WIN*WIN];static int cq_have;
static void test_clipab(int sw) {
 static const int cs[4]={5,7,8,9};static const char *const nm[4]={"NC (P3, near plane and sides)","NS (P3, sides only)","NN (P3, near plane only)","N2 (NC, plain coordinates)"};
 static const int fr[4]={14,16,19,22};static const int cf[5]={1,0,1,0,1};static const char *const cfn[5]={"band","no band","band","no band","band"};
 DWORD (WINAPI *ts)(DWORD,DWORD)=0;void *t0=dp_tex(0),*wt=wn_tex();DWORD b0=1;int c,k,r,ok,d[8][5],e[4][5],el[4];char n[16];HMODULE h=GetModuleHandleA("VXDDRAW.DLL");static U32 qr[4][WIN*WIN];static unsigned char qal[4][WIN*WIN];
 say("6Q. %s: a clipped projected floor and the water scene with the guard band switched\n",sw?"software vertex processing (the reference)":"hardware vertex processing (the driver's T&L)");
 if(!t0||!wt){wrong("6Q: the textures were not made");goto out;}
 if(sw){for(c=0;c<4;c++){dp_draw(cs[c],0,t0);ok=read_bb(cq_sw[c]);present();if(!ok)wrong("6Q: back buffer lock failed");}cq_have=1;say("   the software device's pictures kept\n");goto out;}
 ts=h?(DWORD (WINAPI *)(DWORD,DWORD))GetProcAddress(h,"VX_D3D_TestSet"):0;
 if(!ts){say("   VX_D3D_TestSet not there (a driver before .720): 6Q not run\n");goto out;}
 b0=ts(1,0xFFFFFFFFUL);
 for(c=0;c<4;c++){wr_floor(cs[c]==9?5:cs[c],qr[c]);memcpy(qal[c],wr_al,sizeof(wr_al));}
 for(r=0;r<5;r++){ts(1,(DWORD)cf[r]);
  for(c=0;c<4;c++){dp_draw(cs[c],0,t0);ok=read_bb(cur);present();d[c][r]=ok&&cq_have?ndiff(cq_sw[c],cur):-1;memcpy(wr_al,qal[c],sizeof(wr_al));e[c][r]=ok?wr_gross(cur,qr[c],&el[c]):-1;if(ok&&r<4&&d[c][r]>0){sprintf(n,"6Q%c%d.BMP",'A'+c,r);bmp(n,cur);}}
  for(k=0;k<4;k++){wn_frame(fr[k],wt);ok=read_bb(cur);present();d[4+k][r]=ok&&wn_have?ndiff(wn_sw[fr[k]],cur):-1;if(ok&&r<4&&d[4+k][r]>0){sprintf(n,"6QW%d%d.BMP",k,r);bmp(n,cur);}}}
 ts(1,b0);
 if(cq_have)for(c=0;c<4;c++){sprintf(n,"6Q%cS.BMP",'A'+c);bmp(n,cq_sw[c]);}
 say("   pixels differing from the software device's picture:\n   %-34s","");for(r=0;r<5;r++)say(" %12s",cfn[r]);say("\n");
 for(c=0;c<8;c++){int ls=c<4?lit(cq_sw[c]):lit(wn_sw[fr[c-4]]);char lb[40];if(c<4)sprintf(lb,"%s",nm[c]);else sprintf(lb,"6N frame %d",fr[c-4]);say("   %-34s",lb);
  for(r=0;r<5;r++){if(d[c][r]<0)say("        -    ");else say(" %6d %4.1f%%",d[c][r],ls?100.0*d[c][r]/ls:0.0);}say("\n");}
 {DWORD gb1;ts(1,1);gb1=ts(1,0xFFFFFFFFUL);ts(1,b0);say("   (the guard band was %lu before; put back)%s\n",b0,gb1?"":"; this chip's VxD does not take the band (VX800 / VX900): every column drawn without it");}
 say("   v1.43 (driver .724): the floors against their own reference (pixels off by more than 64 where it is not aliased, %% of its lit ones there; the judge, 3%%):\n");
 for(c=0;c<4;c++){say("   %-34s",nm[c]);for(r=0;r<5;r++){if(e[c][r]<0)say("        -    ");else say(" %6d %4.1f%%",e[c][r],el[c]?100.0*e[c][r]/el[c]:0.0);}say("\n");
  for(r=0;r<5;r++)if(e[c][r]<0||e[c][r]*100>el[c]*3){char m[96];sprintf(m,"6Q: %s is not its reference's picture (%s)",nm[c],cfn[r]);wrong(m);sprintf(n,"6QR%c.BMP",'A'+c);bmp(n,qr[c]);break;}}
 out:if(t0)((F0)VT(t0,D3D_REL))(t0);if(wt)((F0)VT(wt,D3D_REL))(wt);
}
static void test_grid(int sw) {
 void *tx=0;LR lr;int i,j,k,l;LARGE_INTEGER f,a,b,prev;FILE *csv;
 static const char *const gname[12]={"6. ","6b. a point light: ","6c. 720 draws of 48 triangles: ","6d. a second stage on and off between grids: ","6e. a GTA3-like frame: ","6f. a second stage on for every grid: ","6g. a second stage set off for every grid: ","6h. eight lights with specular lighting: ","6i. spot lights and generated coordinates: ","6j. 30 tori, one light: ","6k. 30 tori, eight lights with specular lighting: ","6l. 30 tori, eight lights with specular lighting, back faces culled: "},*const gcsv[12][2]={{"VXG_HW.CSV","VXG_SW.CSV"},{"VXP_HW.CSV","VXP_SW.CSV"},{"VXS_HW.CSV","VXS_SW.CSV"},{"VXD_HW.CSV","VXD_SW.CSV"},{"VXA_HW.CSV","VXA_SW.CSV"},{"VXF_HW.CSV","VXF_SW.CSV"},{"VXH_HW.CSV","VXH_SW.CSV"},{"VXL_HW.CSV","VXL_SW.CSV"},{"VXI_HW.CSV","VXI_SW.CSV"},{"VXJ_HW.CSV","VXJ_SW.CSV"},{"VXK_HW.CSV","VXK_SW.CSV"},{"VXC_HW.CSV","VXC_SW.CSV"}};
 if(g_mode==4)say("%s%s: 640 indexed draws from vertex and index buffers, 40%% prelit and 60%% lit by two lights, fog, 8 textures, a world matrix before two draws in three, a quarter off the screen\n",gname[g_mode],sw?"software (the runtime's T&L)":"hardware (the driver's T&L)");
 else if(g_mode>=9)say("%s%s: 30 untextured, unfogged tori of 5,000 triangles a frame (150,000 triangles), lit by %s\n",gname[g_mode],sw?"software (the runtime's T&L)":"hardware (the driver's T&L)",g_mode==9?"one directional light":g_mode==11?"one directional and seven point lights, specular (power 20), back faces culled":"one directional and seven point lights, specular (power 20)");
 else say("%s%s: 30 lit, fogged, textured 24x24 grids a frame (34,560 triangles), some across the near plane and off the screen\n",gname[g_mode],sw?"software (the runtime's T&L)":"hardware (the driver's T&L)");
 if(g_mode==4&&!gt_make())return;
 for(j=0;j<=GN;j++)for(i=0;i<=GN;i++){GV *g=&g_vb[j*(GN+1)+i];g->x=(float)i-GN/2;g->y=0;g->z=(float)j;g->nx=0.3f;g->ny=1.0f;g->nz=0.1f;g->c=0xFF806040UL;g->u=(float)i/GN;g->v=(float)j/GN;}
 for(k=0,j=0;j<GN;j++)for(i=0;i<GN;i++){unsigned short a0=(unsigned short)(j*(GN+1)+i);g_ix[k++]=a0;g_ix[k++]=(unsigned short)(a0+GN+1);g_ix[k++]=(unsigned short)(a0+1);g_ix[k++]=(unsigned short)(a0+1);g_ix[k++]=(unsigned short)(a0+GN+1);g_ix[k++]=(unsigned short)(a0+GN+2);}
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,8,8,1,0,22,1,&tx)||!tx){wrong("CreateTexture failed");return;}
 if(!((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(tx,16))(tx,0,&lr,0,0)){for(k=0;k<8;k++){U32 *r=(U32*)((char*)lr.bits+k*lr.pitch);for(i=0;i<8;i++)r[i]=((i^k)&1)?0xFFE0E0E0UL:0xFF909090UL;}((F1)VT(tx,17))(tx,0);}
 (g_mode==4?gt_frame():g_mode>=9?tor_frame():grid_frame(tx));l=read_bb(cur)?lit(cur):0;present();
 QueryPerformanceFrequency(&f);
 if(!f.QuadPart){wrong("no performance-counter frequency");((F0)VT(tx,D3D_REL))(tx);gt_free();return;}
 f_scene=gname[g_mode];for(i=0;i<(g_secs?10:5);i++){MSG wm;f_frame=-1-i;(g_mode==4?gt_frame():g_mode>=9?tor_frame():grid_frame(tx));fchk("the frame's draws");present();fchk("Present");while(PeekMessage(&wm,0,0,0,PM_REMOVE))DispatchMessage(&wm);} /* v1.27: the messages read in the warm-up too (the window answered none for up to 3 s: not responding) */ /* warmup, excluded from samples (v1.25: 10 in GRID, 5 outside; 30 before) */
 QueryPerformanceCounter(&a);prev=a;
 for(k=0;;k++){MSG msg;if(g_secs){QueryPerformanceCounter(&b);if(f.QuadPart&&(b.QuadPart-a.QuadPart)>=f.QuadPart*g_secs)break;}else if(k==5)break; /* v1.25: 5 outside GRID (20 before): the picture check */
  if((U32)k>=GRID_SAMPLES)break;
  f_frame=k;(g_mode==4?gt_frame():g_mode>=9?tor_frame():grid_frame(tx));fchk("the frame's draws");present();fchk("Present");while(PeekMessage(&msg,0,0,0,PM_REMOVE))DispatchMessage(&msg);
  QueryPerformanceCounter(&b);grid_samples[k]=(double)(b.QuadPart-prev.QuadPart)*1000.0/f.QuadPart;prev=b;}
 QueryPerformanceCounter(&b);
 ((F0)VT(tx,D3D_REL))(tx);gt_free();
 if(f_bad)say("   x87: %d checks found the state changed in this scene (the first 8 above)\n",f_bad);f_bad=0; /* v1.26 */
 if(g_mode==4)say("   %lu triangles a frame\n",(unsigned long)g_gt_tris);
 say("   lit %d; %d frames: %.2f ms a frame\n",l,k,f.QuadPart&&k?(double)(b.QuadPart-a.QuadPart)*1000.0/f.QuadPart/k:0.0);
 csv=beside(gcsv[g_mode][sw?1:0]);
 if(csv){fprintf(csv,"frame,ms\n");for(i=0;i<k;i++)fprintf(csv,"%d,%.6f\n",i+1,grid_samples[i]);fclose(csv);}
 else wrong("could not write the frame-time CSV");
 if(k){qsort(grid_samples,k,sizeof(grid_samples[0]),grid_cmp);
  say("   warmup %d; p50 %.3f ms; p95 %.3f ms; CSV %s (%s + driver packing)\n",g_secs?10:5,grid_samples[(k-1)/2],grid_samples[((U32)k*95+99)/100-1],gcsv[g_mode][sw?1:0],sw?"runtime T&L":"custom T&L");}
 if((U32)k==GRID_SAMPLES)say("   sample limit reached; run ended at 65536 frames\n");
 if(!l){wrong("the grids were not drawn");return;}
 if(sw){memcpy(g_swm[g_mode],cur,sizeof(cur));g_havem[g_mode]=1;}
 else if(g_havem[g_mode]){int d=ndiff(g_swm[g_mode],cur),ls=lit(g_swm[g_mode]);say("   against the software device's picture: %d pixels differ of %d lit\n",d,ls);
  if(d){char an[16],bn[16];sprintf(an,"VXT%d_A.BMP",g_mode);sprintf(bn,"VXT%d_B.BMP",g_mode);bmp(an,g_swm[g_mode]);bmp(bn,cur); /* v1.21: each scene's own pair, whenever a pixel differs */
   if(d*100>ls){say("   (%s software, %s the driver's)\n",an,bn);wrong("not the software device's picture");}}}
 /* v1.33 (T681: v1.32 compared 6l with 6k, but 6l's view and lights are not 6k's, so it differed on every run, with or without the
  * engine's cull): 6l's own frame drawn once more with CULL_NONE by the same device right after the timed frames; the tori are closed and
  * depth-tested, so culled correctly the two pictures are one (a wrong winding shows the insides, no cull or nothing drawn differ too) */
 if(g_mode==11){static U32 nc[WIN*WIN];int d,lk;g_nocull=1;tor_frame();g_nocull=0;fchk("the frame's draws");
  if(!read_bb(nc)){wrong("6l: the CULL_NONE frame could not be read");return;}
  present();d=ndiff(nc,cur);lk=lit(nc);
  say("   against its own frame with nothing culled (this run %s): %d pixels differ of %d lit\n",sw?"the software device's":"the driver's T&L",d,lk);
  if(!lit(cur))wrong("6l drew nothing (the engine's cull)");
  else if(d*200>lk){bmp(sw?"VXTLC_S.BMP":"VXTLC_H.BMP",cur);bmp("VXTLC_N.BMP",nc);wrong("6l is not its CULL_NONE picture (the engine's cull: VXTLC_*.BMP)");}}
}
static int make_device(U32 behaviour);
static U32 att_ref[WIN*WIN];
/* v1.45 (.727): FILL, the texture fill rate with one and with two units (3DMark 2001: the VX800 gave the same rate with two textures as
 * with one, the VX855 1.6 times and the VX900 1.8 times). 64 screen-sized quads a frame, pretransformed, no depth, no blend, bilinear,
 * each unit its own texture: 32x32 at one texel a pixel (wrapped: held in the texture cache), 256x256 at one texel a pixel, 1024x1024
 * shrunk four times (no mips: every pixel's texels far apart), each as R5G6B5, X8R8G8B8 and DXT1. If small textures double with two
 * units and large ones do not, memory feeds the units; if none does, the engine (or our set-up of its second unit) does not overlap them. */
typedef struct {float x,y,z,rhw,u0,v0,u1,v1;} FQ;
static void *fill_tex(U32 sz,U32 fmt,U32 seed) {void *tx=0;LR lr;U32 y,rows,bytes;
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,sz,sz,1,0,fmt,1,&tx)||!tx)return 0;
 rows=fmt==0x31545844UL?sz/4:sz;bytes=fmt==0x31545844UL?sz/4*8:fmt==23?sz*2:sz*4;
 if(!((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(tx,16))(tx,0,&lr,0,0)){
  for(y=0;y<rows;y++){unsigned char *r=(unsigned char*)lr.bits+y*lr.pitch;U32 x;for(x=0;x<bytes;x++){seed=seed*1664525UL+1013904223UL;r[x]=(unsigned char)(seed>>24);}}
  ((F1)VT(tx,17))(tx,0);}
 return tx;}
static double fill_run(int units,float span) {
 static FQ q[4];LARGE_INTEGER f,a,b;int fr,l,k;const float W=(float)WIN;
 for(k=0;k<4;k++){q[k].x=(k&1)?W:0.0f;q[k].y=(k&2)?W:0.0f;q[k].z=0.5f;q[k].rhw=1.0f;q[k].u0=q[k].u1=(k&1)?span:0.0f;q[k].v0=q[k].v1=(k&2)?span:0.0f;}
 ((F1)VT(dev,DEV_SETVS))(dev,0x204);
 for(fr=0;fr<12;fr++){
  if(fr==2){QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);}
  ((F0)VT(dev,DEV_BEGIN))(dev);
  for(l=0;l<64;l++)((HRESULT(__stdcall*)(void*,U32,U32,const void*,U32))VT(dev,DEV_DPUP))(dev,5,2,q,sizeof(FQ));
  ((F0)VT(dev,DEV_END))(dev);present();}
 {static U32 px[WIN*WIN];read_bb(px);} /* a Lock of the back buffer: the engine finished */
 QueryPerformanceCounter(&b);
 if(!f.QuadPart||b.QuadPart<=a.QuadPart)return 0;
 return (double)WIN*WIN*64.0*10.0*units/((double)(b.QuadPart-a.QuadPart)/(double)f.QuadPart)/1e6; /* texels a second, millions */
}
static void test_fill(void) {
 static const U32 fmts[3]={23,22,0x31545844UL};static const char *fn[3]={"R5G6B5","X8R8G8B8","DXT1"};
 static const U32 sizes[3]={32,256,1024};static const float spans[3]={8.0f,1.0f,1.0f};static const char *sn[3]={"32x32, a texel a pixel (cache)","256x256, a texel a pixel","1024x1024, shrunk 4x, no mips"};
 int si,fi;
 say("FILL. the texture fill rate, one unit and two (64 screen quads a frame, 10 frames; millions of texels a second; 3DMark 2001's multitexture question)\n");
 if(!make_device(0x20)){wrong("FILL: no device");return;}
 rs(7,0);rs(14,0);rs(27,0);rs(15,0);rs(22,1);rs(137,0);rs(28,0);rs(29,0);
 for(si=0;si<3;si++)for(fi=0;fi<3;fi++){
  void *t0=fill_tex(sizes[si],fmts[fi],1+si*3+fi),*t1=fill_tex(sizes[si],fmts[fi],100+si*3+fi);double r1,r2;int k;
  if(!t0||!t1){say("   %-32s %-9s not made (format not offered)\n",sn[si],fn[fi]);if(t0)((F0)VT(t0,D3D_REL))(t0);if(t1)((F0)VT(t1,D3D_REL))(t1);continue;}
  for(k=0;k<2;k++){tss(k,16,2);tss(k,17,2);tss(k,18,0);tss(k,13,1);tss(k,14,1);tss(k,11,k);}
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,t0);tss(0,1,2);tss(0,2,2);tss(0,4,2);tss(0,5,2);
  ((F2P)VT(dev,DEV_SETTEX))(dev,1,0);tss(1,1,1);tss(1,4,1);
  r1=fill_run(1,spans[si]);
  ((F2P)VT(dev,DEV_SETTEX))(dev,1,t1);tss(1,1,4);tss(1,2,2);tss(1,3,1);tss(1,4,3);tss(1,6,1);
  r2=fill_run(2,spans[si]);
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F2P)VT(dev,DEV_SETTEX))(dev,1,0);tss(1,1,1);tss(1,4,1);
  ((F0)VT(t0,D3D_REL))(t0);((F0)VT(t1,D3D_REL))(t1);
  say("   %-32s %-9s one unit %7.1f, two units %7.1f: %.2fx\n",sn[si],fn[fi],r1,r2,r1>0?r2/r1:0.0);}
 ((F0)VT(dev,D3D_REL))(dev);dev=0;
}
static void test_attach(int sw) {
 STARTUPINFOA si;PROCESS_INFORMATION pi;char cmd[64];DWORD t0;int k,n=0,d,l;MSG msg;
 say("10. a second program (VXD3DMON.EXE 8) started while the %s device draws the GTA3-like frame\n",sw?"software":"hardware");
 if(!make_device(sw?0x20:0x40)){wrong("no device");return;}
 g_mode=4;if(!gt_make()){((F0)VT(dev,D3D_REL))(dev);dev=0;return;}
 for(k=0;k<5;k++){gt_frame();present();}
 gt_frame();l=read_bb(att_ref)?lit(att_ref):0;present();say("   before: lit %d\n",l);
 memset(&si,0,sizeof(si));si.cb=sizeof(si);strcpy(cmd,"VXD3DMON.EXE 8");
 if(!CreateProcessA(0,cmd,0,0,FALSE,CREATE_NEW_CONSOLE,0,0,&si,&pi)){wrong("VXD3DMON.EXE could not be started (it must be in this folder)");gt_free();((F0)VT(dev,D3D_REL))(dev);dev=0;return;}
 say("   VXD3DMON started; drawing on for 12 s\n");
 t0=GetTickCount();while(GetTickCount()-t0<12000){gt_frame();present();n++;while(PeekMessage(&msg,0,0,0,PM_REMOVE))DispatchMessage(&msg);}
 d=WaitForSingleObject(pi.hProcess,20000)==WAIT_OBJECT_0;CloseHandle(pi.hProcess);CloseHandle(pi.hThread);
 say("   %d frames while it ran; VXD3DMON %s\n",n,d?"ended":"still running after 32 s");
 for(k=0;k<3;k++){gt_frame();present();}
 gt_frame();read_bb(cur);present();
 d=ndiff(att_ref,cur);say("   after: lit %d; %d pixels differ from before\n",lit(cur),d);
 if(d){char an[16],bn[16];sprintf(an,"VXTA%d_A.BMP",sw);sprintf(bn,"VXTA%d_B.BMP",sw);bmp(an,att_ref);bmp(bn,cur);say("   (%s before, %s after)\n",an,bn);wrong("the picture changed when the second program started");}
 say("   releasing the frame's buffers and textures\n");gt_free();
 say("   releasing the device\n");((F0)VT(dev,D3D_REL))(dev);dev=0;
 say("   released\n");
}
static int make_device(U32 behaviour) {
 PP pp;HRESULT hr;
 memset(&pp,0,sizeof(pp));pp.bw=WIN;pp.bh=WIN;pp.bfmt=bbfmt;pp.bcount=1;pp.swap=3;pp.wnd=wnd;pp.windowed=1;pp.autoz=1;pp.zfmt=80;pp.flags=1;dev=0;
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,behaviour,&pp,&dev);
 say("   CreateDevice(HAL, %s vertex processing): %08lX\n",behaviour==0x20?"software":"hardware",(U32)hr);
 if(hr||!dev){dev=0;return 0;}f_cw0=fpu_env(0);fchk("CreateDevice");return 1; /* v1.26: the control word Direct3D set */
}
static int six_tl(const char *what) {
 int k,d[6],bad=0;
 for(k=0;k<6;k++){draw_tl();if(!read_bb(k?cur:ref)){wrong("back buffer lock failed");return 0;}present();d[k]=k?ndiff(ref,cur):0;if(d[k]){bad++;if(!saved){bmp("VXTLP_A.BMP",ref);bmp("VXTLP_B.BMP",cur);saved=1;}}}
 say("   six shots of the pretransformed quads (%s), pixels differing from the first: %d %d %d %d %d; lit %d\n",what,d[1],d[2],d[3],d[4],d[5],lit(ref));
 if(!lit(ref))wrong("nothing drawn");
 if(bad)wrong("the picture is not steady (VXTLP_A/B.BMP)");
 return 1;
}
/* 3b. (v1.2, .535) many textures: a 33x33 grid of quads, each with its own 8x8 texture (1,089, so texture handles past 1,024), one draw a
 * quad; six shots, the same picture; the time a frame (20 frames), to compare drivers. */
#define NTX 1089
static void *txs[NTX];
static int make_textures(void) {
 int k;
 for(k=0;k<NTX;k++){LR lr;U32 x,y,c=0xFF000000UL|((U32)(k*37)&255)<<16|((U32)(k*91)&255)<<8|((U32)(k*53)&255)|0x101010UL;
  if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,8,8,1,0,22,1,&txs[k])||!txs[k]){say("   CreateTexture %d failed\n",k);return 0;}
  if(((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(txs[k],16))(txs[k],0,&lr,0,0)){say("   texture lock failed\n");return 0;}
  for(y=0;y<8;y++){U32 *row=(U32*)((unsigned char*)lr.bits+y*lr.pitch);for(x=0;x<8;x++)row[x]=((x^y)&4)?c:(c&0xFF7F7F7FUL);}
  ((F1)VT(txs[k],17))(txs[k],0);}
 return 1;
}
static void draw_textures(void) {
 int k,j;static const float qx[6]={0,1,0,1,1,0},qy[6]={0,0,1,0,1,1};TL v[6];
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();rs(137,0);tss(0,1,4);tss(0,2,2);tss(0,3,0);tss(0,4,2);tss(0,5,2);tss(0,16,1);tss(0,17,1);
 ((F1)VT(dev,DEV_SETVS))(dev,0x144);
 for(k=0;k<NTX;k++){float x=2.0f+(k%33)*7.7f,y=2.0f+(k/33)*7.7f;
  for(j=0;j<6;j++){v[j].x=x+qx[j]*7.0f;v[j].y=y+qy[j]*7.0f;v[j].z=0.5f;v[j].w=1.0f;v[j].c=0xFFFFFFFFUL;v[j].u=qx[j];v[j].v=qy[j];}
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,txs[k]);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,2,v,sizeof(TL));}
 ((F0)VT(dev,DEV_END))(dev);
}
static void test_textures(void) {
 int k,d[6],bad=0;LARGE_INTEGER f,a,b;
 say("3b. 1,089 textures, one draw each (software vertex processing)\n");
 if(!make_textures()){fails++;goto out;}
 for(k=0;k<6;k++){draw_textures();if(!read_bb(k?cur:ref)){wrong("back buffer lock failed");goto out;}present();d[k]=k?ndiff(ref,cur):0;if(d[k]){bad++;if(!saved){bmp("VXTLP_A.BMP",ref);bmp("VXTLP_B.BMP",cur);saved=1;}}}
 say("   six shots, pixels differing from the first: %d %d %d %d %d; lit %d\n",d[1],d[2],d[3],d[4],d[5],lit(ref));
 if(lit(ref)<40000)wrong("the grid not drawn");
 if(bad)wrong("the picture is not steady (VXTLP_A/B.BMP)");
 QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);for(k=0;k<20;k++){draw_textures();present();}QueryPerformanceCounter(&b);
 say("   20 frames: %.2f ms a frame (1,089 draws and texture changes each)\n",f.QuadPart?(double)(b.QuadPart-a.QuadPart)*1000.0/f.QuadPart/20:0.0);
out:
 for(k=0;k<NTX;k++)if(txs[k]){((F0)VT(txs[k],D3D_REL))(txs[k]);txs[k]=0;}
}
/* v1.52 (driver .752): VXTLP VBOX. The driver's T&L keeps a box of a vertex range and drops a draw whose box is off the screen before it
 * reads a vertex; the box is good only while the vertices are the ones it was measured on. This rewrites a mesh's vertices in every
 * way a program can, moving it off the screen and back with the matrices left alone, and looks at the picture after each move: a box
 * that outlived its vertices would leave the window empty when the mesh has come back. Six ways: a static vertex buffer locked with
 * no flags; a dynamic one locked with DISCARD; a dynamic one locked with NOOVERWRITE; a static one written through the pointer of an
 * earlier Lock, with no Lock at all; a managed one; and DrawPrimitiveUP (the runtime's own buffer). 128 triangles (384 vertices, a
 * list in order). Each state is drawn six times (the driver measures a range on its second draw and drops from the third), the
 * picture read after the first and the sixth. The driver's counters for each way are printed (QueryPhases 472-500); only the pictures
 * decide. Run it with VXREG VBUF OFF and ON and VXREG VBOX 0, 1, 2: the pictures must be right in every one. */
#define VBN 384
static void vbox_fill(PV *p,float ox) {int i,j,k=0;static const float qx[6]={0,1,0,1,1,0},qy[6]={0,0,1,0,1,1};
 for(j=0;j<8;j++)for(i=0;i<8;i++){int q;for(q=0;q<6;q++,k++)p7_pv(p+k,ox-0.4f+0.1f*((float)i+qx[q]),-0.4f+0.1f*((float)j+qy[q]),0.5f,0xFF00FF00UL);}}
static int vbox_frames(void *vb,const PV *up,int n) {int k,l=0;
 for(k=0;k<n;k++){p7_begin();
  if(vb){((F3P)VT(dev,DEV_STREAM))(dev,0,(U32)vb,(void*)sizeof(PV));((HRESULT(__stdcall*)(void*,U32,U32,U32))VT(dev,DEV_DP))(dev,4,0,VBN/3);((F3P)VT(dev,DEV_STREAM))(dev,0,0,0);}
  else DPUP(4,VBN/3,(void*)up);
  ((F0)VT(dev,DEV_END))(dev);l=read_bb(cur)?lit(cur):-1;present();}
 return l;}
static void test_vbox(void) {
 static const char *name[6]={"a static vertex buffer, Lock with no flags","a dynamic vertex buffer, Lock with DISCARD","a dynamic vertex buffer, Lock with NOOVERWRITE","a static vertex buffer written through a kept pointer, no Lock","a managed vertex buffer, Lock with no flags","DrawPrimitiveUP (no vertex buffer)"};
 static const U32 usage[6]={8,0x208,0x208,8,8,0},pool[6]={0,0,0,0,1,0},flags[6]={0,0x2000,0x1000,0,0,0};
 static PV up[VBN];static DWORD q0[501],q1[501];int m,c,bad0=fails;
 say("VBOX. vertices rewritten under a cached box (.752): the mesh moved off the screen and back six ways\n");
 if(hal_phases&&hal_phases(q0,501)==501)say("   the driver: cached boxes mode %lu (0 off, 1 on, 2 counted and checked only); the HAL's vertex buffers %s\n",q0[480],q0[481]?"on":"off");
 else{say("   QueryPhases with 501 words: not answered (a driver before .752)\n");q0[0]=0;}
 for(m=0;m<6;m++){void *vb=0;PV *p=0,*kept=0;int l0,l1,ok=1;char line[200];
  if(q0[0])hal_phases(q0,501);
  if(m<5){if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,void**))VT(dev,DEV_CREATEVB))(dev,VBN*sizeof(PV),usage[m],0x42,pool[m],&vb)||!vb){say("   %s: CreateVertexBuffer failed\n",name[m]);fails++;continue;}}
  for(c=0;c<7&&ok;c++){const float ox=(c&1)?0.0f:3.0f; /* off, on, off, on, off, on, off */
   if(m==5)vbox_fill(up,ox);
   else if(m==3&&kept)vbox_fill(kept,ox); /* no Lock: the pointer of the first one */
   else{p=0;if(((HRESULT(__stdcall*)(void*,U32,U32,void**,U32))VT(vb,VB_LOCK))(vb,0,0,(void**)&p,c?flags[m]:(m==1||m==2?0x2000:0))||!p){wrong("vertex buffer Lock failed");ok=0;break;}
    vbox_fill(p,ox);kept=p;((F0)VT(vb,VB_UNLOCK))(vb);}
   l0=vbox_frames(vb,up,1);l1=vbox_frames(vb,up,5);
   if((c&1)?(l0<=0||l1<=0):(l0!=0||l1!=0)){sprintf(line,"%s, move %d (%s the screen): lit %d after the first draw, %d after the sixth",name[m],c,(c&1)?"onto":"off",l0,l1);wrong(line);
    if(c&1){bmp("VXTLVBOX.BMP",cur);}ok=0;}}
  if(vb)((F0)VT(vb,D3D_REL))(vb);
  if(q0[0]&&hal_phases(q1,501)==501)say("   %s: %s; draws from a buffer with a generation %lu, with a box %lu, dropped before the gather %lu, boxes made %lu, dropped by the three-vertex check %lu, a box wrong (mode 2) %lu; buffers created %lu, locks seen %lu, unlocks %lu\n",
    name[m],ok?"the pictures right":"WRONG",q1[472]-q0[472],q1[473]-q0[473],q1[474]-q0[474],q1[476]-q0[476],q1[477]-q0[477],q1[479]-q0[479],q1[484]-q0[484],q1[487]-q0[487],q1[491]-q0[491]);
  else say("   %s: %s\n",name[m],ok?"the pictures right":"WRONG");
  if(q0[0]&&q1[479]!=q0[479])wrong("a cached box said a draw was off the screen and its vertices were not (mode 2's check)");}
 say("   VBOX: %s\n",fails==bad0?"RIGHT: every picture as the vertices were":"WRONG");
}
static void test_d3d8(void) {
 HMODULE d8=LoadLibrary("d3d8.dll");void *(__stdcall *create)(U32);MODE mode;unsigned char caps[512];DWORD qb[302];int k;static U32 sw_lit[WIN*WIN];int have_lit=0;
 create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 if(!create){say("2-4. d3d8.dll / Direct3DCreate8 not found (DirectX 8 or later is needed): not run\n");fails++;return;}
 d3d=create(220);if(!d3d)d3d=create(120);if(!d3d){wrong("Direct3DCreate8 failed");return;}
 memset(&mode,0,sizeof(mode));((F2P)VT(d3d,D3D_MODE))(d3d,0,&mode);bbfmt=mode.format;
 if(g_secs){ /* v1.10: GRID */
  for(g_mode=strstr(args(),"6E")?4:strstr(args(),"6I")?8:strstr(args(),"6J")?9:strstr(args(),"6K")?10:strstr(args(),"6L")?11:0;g_mode<12;g_mode=strstr(args(),"6E")||strstr(args(),"6I")||strstr(args(),"6J")||strstr(args(),"6K")||strstr(args(),"6L")?12:g_mode==1?4:g_mode==4?7:g_mode+1){ /* v1.25: 6c (2) left out */ /* v1.24: and 9, 10 (6j, 6k); GRID 6J / 6K alone */ /* v1.23: and 8 (6i); GRID 6I, 6i alone */ /* v1.22: 0, 1, 2, 4 and 7 (6h) */ /* v1.19: 0, 1, 2 and 4 (6e, the GTA3-like frame); not 3 (a correctness scene). v1.21: GRID 6E, 6e alone (for VXD3DMON's profile of each device) */ /* v1.17: the three scenes (the grids, lit by a point light, as 720 small draws), each on both devices */
  if(!strstr(args(),"HW")){if(make_device(0x20)){test_grid(1);((F0)VT(dev,D3D_REL))(dev);dev=0;}else wrong("no software-processing device");}
  if(!strstr(args(),"SW")){if(make_device(0x40)){test_grid(0);((F0)VT(dev,D3D_REL))(dev);dev=0;}else wrong("no hardware-processing device (ACCEL_D3D_TNL off?)");}}
  g_mode=0;
  ((F0)VT(d3d,D3D_REL))(d3d);d3d=0;return;}
 if(strstr(args(),"FILL")){test_fill();((F0)VT(d3d,D3D_REL))(d3d);d3d=0;return;} /* v1.45 */
 if(strstr(args(),"ATTACH")){test_attach(1);test_attach(0);g_mode=0;((F0)VT(d3d,D3D_REL))(d3d);d3d=0;return;} /* v1.25 */
 if(strstr(args(),"VBOX")){if(make_device(0x40)){test_vbox();((F0)VT(dev,D3D_REL))(dev);dev=0;}else wrong("no hardware-processing device (ACCEL_D3D_TNL off?)");((F0)VT(d3d,D3D_REL))(d3d);d3d=0;return;} /* v1.52 */
 say("2. Direct3D 8: the HAL's caps\n");
 memset(caps,0,sizeof(caps));
 if(((HRESULT(__stdcall*)(void*,U32,U32,void*))VT(d3d,D3D_CAPS))(d3d,0,1,caps)){wrong("GetDeviceCaps failed");}
 else{U32 dc=*(U32*)(caps+28),vp=*(U32*)(caps+156),ml=*(U32*)(caps+160);int hw=(dc&0x10000UL)!=0;
  say("   DevCaps %08lX: HWTRANSFORMANDLIGHT %s; VertexProcessingCaps %08lX, MaxActiveLights %lu (expected %s)\n",dc,hw?"yes":"no",vp,ml,probe?"yes, 8 lights":"no");
  if(hw!=probe)wrong(probe?"the probe is on but the caps show no hardware T&L":"hardware T&L in the caps with the probe off");
  if(probe&&ml!=8)wrong("not 8 lights");}
 say("3. software vertex processing (the probe must leave it alone)\n");
 if(make_device(0x20)){
  q_read(qa);draw_lit();if(!read_bb(cur)){wrong("back buffer lock failed");}else{present();q_read(qb);
   say("   the lit square, untransformed: lit %d (drawn: the runtime transforms it); the probe counted %lu calls, %lu draws\n",lit(cur),qb[271]-qa[271],qb[272]-qa[272]);
   if(!lit(cur))wrong("the lit square was not drawn");else{memcpy(sw_lit,cur,sizeof(cur));have_lit=1;}
   if(qb[271]!=qa[271]||qb[272]!=qa[272])wrong("the probe counted a software-processed draw");}
  if(six_tl("software vertex processing")){memcpy(sw_ref,ref,sizeof(ref));have_sw=1;}
  if(g_proj){test_proj(1);test_deep(1);test_water(1);test_wref(1);test_clipab(1);}else{ /* v1.37: VXTLP PROJ, 6M alone */
  test_textures(); /* v1.2 */
  test_540(1); /* v1.7 */
  test_640(1); /* v1.15 */
  test_646(1); /* v1.16 */
  test_649(1); /* v1.20 */
  test_663(1); /* v1.29 */
  test_668(1); /* v1.30 */
  test_wrap(1); /* v1.47 */
  test_tfog(1); /* v1.49 */
  test_proj(1); /* v1.37 */
  test_deep(1);test_water(1);test_wref(1);test_clipab(1); /* v1.38; v1.40 6Q; v1.42 6W */
  test_grid(1); /* v1.8 */
  g_mode=1;test_grid(1);g_mode=2;test_grid(1);g_mode=3;test_grid(1);g_mode=4;test_grid(1);g_mode=5;test_grid(1);g_mode=6;test_grid(1);g_mode=7;test_grid(1);g_mode=8;test_grid(1);g_mode=9;test_grid(1);g_mode=10;test_grid(1);g_mode=11;test_grid(1);g_mode=0; /* v1.17; v1.18 6d; v1.19 6e; v1.21 6f 6g; v1.22 6h */}
  ((F0)VT(dev,D3D_REL))(dev);dev=0;}
 else wrong("no software-processing device");
 say("4. hardware vertex processing\n");
 if(!make_device(0x40)){if(probe)wrong("the probe is on but the hardware-processing device was refused");else say("   refused, as expected with the probe off\n");}
 else{
  if(g_proj){test_proj(0);test_deep(0);test_water(0);test_wref(0);test_clipab(0);((F0)VT(dev,D3D_REL))(dev);dev=0;}else{ /* v1.37 */
  if(!probe)wrong("a hardware-processing device with the probe off");
  q_read(qa);draw_lit();if(!read_bb(cur))wrong("back buffer lock failed");else{present();q_read(qb);
   say("   the lit square, untransformed, transformed and lit by the driver: lit %d; counted: %lu calls with untransformed vertices, %lu draws (%lu primitives); "
    "SETTRANSFORM %lu, SETMATERIAL %lu, SETLIGHT %lu (%lu with data), CREATELIGHT %lu\n",lit(cur),qb[271]-qa[271],qb[272]-qa[272],qb[288]-qa[288],qb[276]-qa[276],qb[273]-qa[273],qb[274]-qa[274],qb[278]-qa[278],qb[275]-qa[275]);
   say("   the driver's T&L: %lu draws, %lu triangles, %lu vertices (%lu lit); %lu clipped, %lu dropped outside, %lu left out, %lu state not done\n",
    qb[290]-qa[290],qb[291]-qa[291],qb[292]-qa[292],qb[298]-qa[298],qb[293]-qa[293],qb[294]-qa[294],qb[297]-qa[297],qb[301]-qa[301]);
   for(k=0;k<4;k++)if(qb[280+k*2])say("   FVF %03lX: %lu calls\n",qb[279+k*2],qb[280+k*2]);
   if(!lit(cur))wrong("the lit square was not drawn");
   if(qb[290]==qa[290])wrong("the driver transformed no draw");
   if(have_lit){int d=ndiff(sw_lit,cur),l=lit(sw_lit);say("   against the software device's lit square: %d pixels differ of %d lit\n",d,l);
    if(d*100>l){bmp("VXTLP_A.BMP",sw_lit);bmp("VXTLP_B.BMP",cur);wrong("not the software device's lit square (VXTLP_A.BMP software, VXTLP_B.BMP the driver's)");}}
   if(qb[271]==qa[271]||qb[272]==qa[272])wrong("the untransformed draw was not counted");
   if(qb[276]-qa[276]<3)wrong("fewer than three SETTRANSFORM entries");
   if(qb[273]==qa[273])wrong("no SETMATERIAL");
   if(qb[278]==qa[278])wrong("no SETLIGHT with light data");}
  { /* 4b. (v1.5, .537) vertex buffers made and released (three rounds of 16, different sizes), the lit square drawn from one, then from memory */
   void *vbs[16];int r,j,n=0,l1=0,l2=0;
   say("4b. vertex buffers under hardware vertex processing\n");
   for(r=0;r<3;r++){for(j=0;j<16;j++){vbs[j]=make_vb((U32)(6+j*7+r*3)*sizeof(UV),j==5);if(vbs[j])n++;}
    if(r==2&&vbs[5]){draw_lit_vb(vbs[5]);if(read_bb(cur)){present();l1=lit(cur);}}
    for(j=0;j<16;j++)if(vbs[j])((F0)VT(vbs[j],D3D_REL))(vbs[j]);}
   draw_lit();if(read_bb(cur)){present();l2=lit(cur);}
   say("   %d vertex buffers made and released; the lit square from a vertex buffer: lit %d; then from memory: lit %d\n",n,l1,l2);
   if(n!=48)wrong("a vertex buffer was refused");
   if(!l1)wrong("nothing drawn from the vertex buffer");
   if(!l2)wrong("nothing drawn after the vertex buffers were released (the device lost?)");
   if(have_lit&&l1&&ndiff(sw_lit,cur)*100>lit(sw_lit))wrong("after the vertex buffers, not the software device's lit square");}
  say("4c. a vertex buffer appended to (hardware vertex processing)\n");vb_append(0x144,28,"pretransformed");vb_append(0x52,28,"untransformed, lit by the driver");
  test_540(0); /* v1.7 */
  test_640(0); /* v1.15 */
  test_646(0); /* v1.16 */
  test_649(0); /* v1.20 */
  test_663(0); /* v1.29 */
  test_668(0); /* v1.30 */
  test_wrap(0); /* v1.47 */
  test_tfog(0); /* v1.49 */
  test_maxmip(); /* v1.51 */
  test_proj(0); /* v1.37 */
  test_deep(0);test_water(0);test_wref(0);test_clipab(0); /* v1.38; v1.40 6Q; v1.42 6W */
  test_grid(0); /* v1.8 */
  g_mode=1;test_grid(0);g_mode=2;test_grid(0);g_mode=3;test_grid(0);g_mode=4;test_grid(0);g_mode=5;test_grid(0);g_mode=6;test_grid(0);g_mode=7;test_grid(0);g_mode=8;test_grid(0);g_mode=9;test_grid(0);g_mode=10;test_grid(0);g_mode=11;test_grid(0);g_mode=0; /* v1.17; v1.18 6d; v1.19 6e; v1.21 6f 6g; v1.22 6h */
  if(six_tl("hardware vertex processing")&&have_sw){int d=ndiff(sw_ref,ref);say("   against the software device's picture: %d pixels differ\n",d);if(d){bmp("VXTLP_A.BMP",sw_ref);bmp("VXTLP_B.BMP",ref);wrong("not the software device's picture");}}
  ((F0)VT(dev,D3D_REL))(dev);dev=0;}}
 ((F0)VT(d3d,D3D_REL))(d3d);d3d=0;
}
static void cpuid1(unsigned long *out); /* v1.9: CPUID leaf 1 into out[0..3] (the driver's bytes: its build refuses the mnemonic) */
#pragma aux cpuid1 = 0x53 0xB8 0x01 0x00 0x00 0x00 0x0F 0xA2 0x89 0x06 0x89 0x5E 0x04 0x89 0x4E 0x08 0x89 0x56 0x0C 0x5B parm [esi] modify exact [eax ecx edx];
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
int main(void) {
 char path[MAX_PATH],*s;WNDCLASS wc;
 SetUnhandledExceptionFilter(crash_log); /* v1.28 */
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXTLP.LOG");logf=fopen(path,"w");
 say("VXTLP v1.52 (driver .752; VBOX: vertices rewritten under a cached box; 12c/12d: fog on big triangles, per vertex against per pixel; 10d/10e: three sets through the T&L; 12: fog from W; 11: WRAP0 0.75 to 0.0; 10c: three sets; FILL: one and two units; 6P/6Q/6W against references drawn here; 6Q: guard band A/B; 6P NA: clipped vertex alpha; 6M: projected texture coordinates on large triangles; 6P: deep floors; 6N: a Dragothic-like water scene; PROJ: 6M, 6P and 6N alone; VP: a viewport at (40, 40)): the driver's T&L; 6b-6l timed scenes (6l: 6k with back faces culled) (a point light, small draws, a second stage on and off, a GTA3-like frame, the second stage always on, always off, eight lights with specular lighting, spot lights with generated coordinates, 30 tori with one light and with eight); 7. points, lines, large draws; 8. spot, specular, high light indices, a third stage's set; 9. depth surfaces swapped and released; 10. a state block past 1,024 bytes, 65 lights, three coordinate sets\n");
 {char dll[MAX_PATH];UINT n=GetSystemDirectoryA(dll,MAX_PATH-16);HMODULE h=0;
  if(n&&n<MAX_PATH-16){strcpy(dll+n,dll[n-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");h=LoadLibraryA(dll);}
  hal_phases=h?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(h,"VX_D3D_QueryPhases"):0;}
 if(!q_read(qa)){say("VX_D3D_QueryPhases with 302 words: not there (a driver before .536)\nFAILED\n");if(logf)fclose(logf);return 2;}
 {HANDLE v=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);DWORD code=0xFFFFFE50UL,o[4],got=0;memset(o,0,sizeof(o)); /* v1.4 */
  if(v!=INVALID_HANDLE_VALUE){if(!DeviceIoControl(v,0x85500012UL,&code,4,o,16,&got,0)||got!=16||o[1]!=0x56584433UL)o[2]=0;CloseHandle(v);}
  probe=(o[2]&0x800UL)&&((o[2]>>21)&1);}
 {HANDLE v=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);DWORD in=0x75,o[8],got=0;memset(o,0,sizeof(o));if(v!=INVALID_HANDLE_VALUE){DeviceIoControl(v,0x85500022UL,&in,4,o,32,&got,0);CloseHandle(v);}say("the driver: build .%lu\n",o[1]);}
 say("the driver runs with ACCEL_D3D_TNL %d (%s)\n",probe,probe?"hardware T&L offered and done by the driver, the default":"off");
 memset(&wc,0,sizeof(wc));wc.hCursor=LoadCursor(0,IDC_ARROW); /* v1.27: the pointer's shape over the window (none: Windows left the last one there) */wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXTLP";wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClass(&wc);
 wnd=CreateWindow("VXTLP","VXTLP",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,WIN+8,WIN+28,0,0,wc.hInstance,0);
 {unsigned long r[4]={0,0,0,0}; /* v1.9: the CPU's SSE and SSE2 (CPUID 1, EDX bits 25 and 26), as the driver's T&L checks them */
  cpuid1(r);
  say("the CPU: SSE %s, SSE2 %s: the driver's T&L uses %s\n",(r[3]>>25)&1?"yes":"no",(r[3]>>26)&1?"yes":"no",((r[3]>>25)&3)==3?"SSE":"the x87");}
 g_vp=strstr(args(),"VP")!=0; /* v1.36 */
 g_proj=strstr(args(),"PROJ")!=0; /* v1.37 */
 if(strstr(args(),"GRID"))g_secs=strstr(args(),"HW")?20:strstr(args(),"SW")?15:10; /* v1.25: 10 s a device (20 and 15 alone); v1.11: HW, the driver's alone, 60 s */ /* v1.10; v1.11: HW, the driver's alone, 60 s; v1.17: each of the three scenes */
 if(!g_secs&&!g_proj&&!strstr(args(),"VBOX"))test_d3d7();
 test_d3d8();
 DestroyWindow(wnd);
 say("%s: %d wrong\n",fails?"FAILED":"PASSED",fails);
 if(logf)fclose(logf);
 return fails?2:0;
}
