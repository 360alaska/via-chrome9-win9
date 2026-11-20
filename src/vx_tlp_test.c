/* VXTLP v1.36 (driver .704: VP, every scene through a viewport at (40, 40), the guard band across a clip edge that is not 0). VXTLP v1.35 (driver .699: 6l writes its depth too, ZWRITEENABLE). VXTLP v1.34 (driver .696: 6l depth-tested, so its CULL_NONE frame hides the back faces; T682-T694 failed only because ZENABLE was off). VXTLP v1.33 (driver .682: 6l checked against its own frame with CULL_NONE; v1.32's 6k comparison was wrong, the views differ). VXTLP v1.32 (driver .680: 6l's picture against 6k's, the engine's back-face cull checked without the software device). VXTLP v1.31 (driver .677: 6l, 6k with back faces culled (D3DCULL_CCW) and the screen's sides crossing the outer tori: the driver's culling before lighting, .676, on a picture compared with the software device's; GRID 6L alone). VXTLP v1.30 (driver .668: 10, what the review of .667 found: a state block past 1,024 bytes of T&L state, 65 light indices with the first
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
static int g_vp;
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
static U32 p7_sw[15][WIN*WIN];static int p7_have[15]; /* v1.29: 8-11 for 9; v1.30: 12-14 for 10 */
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
static void p10_mat(float r,float g,float b) {float m[17];memset(m,0,sizeof(m));m[0]=r;m[1]=g;m[2]=b;m[3]=1;m[4]=m[5]=m[6]=0.2f;m[7]=1;((HRESULT(__stdcall*)(void*,void*))VT(dev,DEV_SETMAT))(dev,m);}
static void p10_dl(U32 idx,float r,float g,float b,float dx,float dy) {
 float lt[26];memset(lt,0,sizeof(lt));*(U32*)&lt[0]=3;lt[1]=r;lt[2]=g;lt[3]=b;lt[4]=1.0f;lt[16]=dx;lt[17]=dy;lt[18]=1.0f;((F2P)VT(dev,DEV_SETLIGHT))(dev,idx,lt);}
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
 t0=p8_tex(0xFFFF8000UL,0xFF0080FFUL);t1=p8_tex(0xFFFFFFFFUL,0xFF808080UL);t2=p8_tex(0xFFFFFFFFUL,0xFFC0C0C0UL);
 p7_begin();
 if(t0&&t1&&t2){static const float px[6]={64,192,64,192,192,64},py[6]={64,64,192,64,192,192},pu[6]={0,1,0,1,1,0},pv[6]={0,0,1,0,1,1};
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,t0);((F2P)VT(dev,DEV_SETTEX))(dev,1,t1);((F2P)VT(dev,DEV_SETTEX))(dev,2,t2);
  tss(0,1,4);tss(0,2,2);tss(0,3,0);tss(0,11,0);tss(1,1,4);tss(1,2,2);tss(1,3,1);tss(1,4,2);tss(1,5,1);tss(1,11,1);tss(2,1,4);tss(2,2,2);tss(2,3,1);tss(2,4,2);tss(2,5,1);tss(2,11,2);
  for(k=0;k<6;k++){v[k].x=px[k];v[k].y=py[k];v[k].z=0.5f;v[k].rhw=1;v[k].c=0xFFFFFFFFUL;v[k].u0=pu[k];v[k].v0=pv[k];v[k].u1=pu[k]*2;v[k].v1=pv[k]*2;v[k].u2=1-pu[k];v[k].v2=pv[k];}
  ((F1)VT(dev,DEV_SETVS))(dev,0x344);((HRESULT(__stdcall*)(void*,U32,U32,void*,U32))VT(dev,DEV_DPUP))(dev,4,2,v,sizeof(TV3));
  tss(1,1,1);tss(1,4,1);tss(2,1,1);tss(2,4,1);tss(1,11,1);tss(2,11,2);
  ((F2P)VT(dev,DEV_SETTEX))(dev,0,0);((F2P)VT(dev,DEV_SETTEX))(dev,1,0);((F2P)VT(dev,DEV_SETTEX))(dev,2,0);((F1)VT(dev,DEV_SETVS))(dev,0x42);}
 p7_end(14,sw,"10c. transformed vertices, three stages on three coordinate sets (drawn: stage 2 on the second set)",100);
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
 if(strstr(args(),"ATTACH")){test_attach(1);test_attach(0);g_mode=0;((F0)VT(d3d,D3D_REL))(d3d);d3d=0;return;} /* v1.25 */
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
  test_textures(); /* v1.2 */
  test_540(1); /* v1.7 */
  test_640(1); /* v1.15 */
  test_646(1); /* v1.16 */
  test_649(1); /* v1.20 */
  test_663(1); /* v1.29 */
  test_668(1); /* v1.30 */
  test_grid(1); /* v1.8 */
  g_mode=1;test_grid(1);g_mode=2;test_grid(1);g_mode=3;test_grid(1);g_mode=4;test_grid(1);g_mode=5;test_grid(1);g_mode=6;test_grid(1);g_mode=7;test_grid(1);g_mode=8;test_grid(1);g_mode=9;test_grid(1);g_mode=10;test_grid(1);g_mode=11;test_grid(1);g_mode=0; /* v1.17; v1.18 6d; v1.19 6e; v1.21 6f 6g; v1.22 6h */
  ((F0)VT(dev,D3D_REL))(dev);dev=0;}
 else wrong("no software-processing device");
 say("4. hardware vertex processing\n");
 if(!make_device(0x40)){if(probe)wrong("the probe is on but the hardware-processing device was refused");else say("   refused, as expected with the probe off\n");}
 else{
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
  test_grid(0); /* v1.8 */
  g_mode=1;test_grid(0);g_mode=2;test_grid(0);g_mode=3;test_grid(0);g_mode=4;test_grid(0);g_mode=5;test_grid(0);g_mode=6;test_grid(0);g_mode=7;test_grid(0);g_mode=8;test_grid(0);g_mode=9;test_grid(0);g_mode=10;test_grid(0);g_mode=11;test_grid(0);g_mode=0; /* v1.17; v1.18 6d; v1.19 6e; v1.21 6f 6g; v1.22 6h */
  if(six_tl("hardware vertex processing")&&have_sw){int d=ndiff(sw_ref,ref);say("   against the software device's picture: %d pixels differ\n",d);if(d){bmp("VXTLP_A.BMP",sw_ref);bmp("VXTLP_B.BMP",ref);wrong("not the software device's picture");}}
  ((F0)VT(dev,D3D_REL))(dev);dev=0;}
 ((F0)VT(d3d,D3D_REL))(d3d);d3d=0;
}
static void cpuid1(unsigned long *out); /* v1.9: CPUID leaf 1 into out[0..3] (the driver's bytes: its build refuses the mnemonic) */
#pragma aux cpuid1 = 0x53 0xB8 0x01 0x00 0x00 0x00 0x0F 0xA2 0x89 0x06 0x89 0x5E 0x04 0x89 0x4E 0x08 0x89 0x56 0x0C 0x5B parm [esi] modify exact [eax ecx edx];
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
int main(void) {
 char path[MAX_PATH],*s;WNDCLASS wc;
 SetUnhandledExceptionFilter(crash_log); /* v1.28 */
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXTLP.LOG");logf=fopen(path,"w");
 say("VXTLP v1.36 (driver .704; VP: a viewport at (40, 40)): the driver's T&L; 6b-6l timed scenes (6l: 6k with back faces culled) (a point light, small draws, a second stage on and off, a GTA3-like frame, the second stage always on, always off, eight lights with specular lighting, spot lights with generated coordinates, 30 tori with one light and with eight); 7. points, lines, large draws; 8. spot, specular, high light indices, a third stage's set; 9. depth surfaces swapped and released; 10. a state block past 1,024 bytes, 65 lights, three coordinate sets\n");
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
 if(strstr(args(),"GRID"))g_secs=strstr(args(),"HW")?20:strstr(args(),"SW")?15:10; /* v1.25: 10 s a device (20 and 15 alone); v1.11: HW, the driver's alone, 60 s */ /* v1.10; v1.11: HW, the driver's alone, 60 s; v1.17: each of the three scenes */
 if(!g_secs)test_d3d7();
 test_d3d8();
 DestroyWindow(wnd);
 say("%s: %d wrong\n",fails?"FAILED":"PASSED",fails);
 if(logf)fclose(logf);
 return fails?2:0;
}
