/* D3D8PROF v1.1 (driver .534): v1.0 plus the vertex work a game asks of Direct3D 8 (at exit, "Vertex work"): its draws' primitives by vertex
 * kind (untransformed, pretransformed, vertex shader), for the untransformed ones what fixed-function transform and lighting they use (lights
 * by count and kind, specular, vertex fog, texture coordinate generation and transforms, vertex blending, user clip planes, clipping off,
 * normalizing), and the vertex ranges of its indexed draws against their triangles (software vertex processing transforms the whole range).
 * Built a second way (-DTLPROBE) as D3D8TLP, the caps probe: the caps a game reads show hardware transform and lighting (8 lights, no clip
 * planes, no vertex blending), the behaviour flags it then asks CreateDevice for are logged, and the device is made with software vertex
 * processing instead (hardware, mixed and pure-device taken out), so the game draws as before; its later SetRenderState of
 * D3DRS_SOFTWAREVERTEXPROCESSING is answered D3D_OK and not passed on; GetCreationParameters returns the flags it asked for. Log
 * C:\D3D8TLP.TXT. It answers "would this game choose hardware T&L?" without the driver offering it.
 *
 * v1.0: a d3d8.dll proxy for Windows 98 that times every IDirect3DDevice8 call a game makes.
 *
 * Put this d3d8.dll next to the game's EXE (Windows loads the one in the EXE's directory first). It loads the real one from the system
 * directory, hands the game an IDirect3D8 whose CreateDevice returns a wrapped device, and brackets each of the device's 97 methods with
 * rdtsc. Nothing else changes: every call goes to the real device with the same arguments, and the game sees the same results.
 *
 * Output: C:\D3D8PROF.TXT. One line a frame (at Present): the frame's wall time, the time inside d3d8 calls, the rest (the game's own
 * work), the draw calls and primitives, and the time in the draw, state and Present calls. At exit, totals and per-frame averages for
 * every method that was called, sorted by time. Cycles are converted with the TSC rate measured against QueryPerformanceCounter.
 *
 * Together with the HAL's own DrawPrimitives2 timing (VXD3DMON), the frame splits into: game, D3D8 runtime (call time minus the HAL's
 * time), driver. That is the split VXD3DMON cannot see from inside the driver.
 *
 * Built with clang --target=i686-pc-windows-msvc and lld-link, no C runtime (Windows 98 has no msvcrt we can count on). MIT, 2026. */
typedef unsigned long DWORD;typedef unsigned int UINT;typedef int BOOL;typedef void *HANDLE;typedef void *HMODULE;typedef long HRESULT;typedef long LONG;
typedef unsigned __int64 QWORD;typedef __int64 LONGLONG;
#define WINAPI __stdcall
#define NULL ((void*)0)
HMODULE WINAPI LoadLibraryA(const char *);void *WINAPI GetProcAddress(HMODULE,const char *);UINT WINAPI GetSystemDirectoryA(char *,UINT);
HANDLE WINAPI CreateFileA(const char *,DWORD,DWORD,void *,DWORD,DWORD,HANDLE);BOOL WINAPI WriteFile(HANDLE,const void *,DWORD,DWORD *,void *);
BOOL WINAPI CloseHandle(HANDLE);BOOL WINAPI QueryPerformanceCounter(LONGLONG *);BOOL WINAPI QueryPerformanceFrequency(LONGLONG *);
DWORD WINAPI GetTickCount(void);DWORD WINAPI GetCurrentThreadId(void);
#define GENERIC_WRITE 0x40000000UL
#define FILE_SHARE_READ 1
#define CREATE_ALWAYS 2
#define FILE_ATTRIBUTE_NORMAL 0x80
#define INVALID_HANDLE_VALUE ((HANDLE)-1)

/* no C runtime: the compiler may still call these */
int _fltused=1; /* the MSVC-style marker the compiler references for floating point; no CRT here */
void *memset(void *d,int c,unsigned n) {unsigned char *p=d;while(n--)*p++=(unsigned char)c;return d;}
void *memcpy(void *d,const void *s,unsigned n) {unsigned char *p=d;const unsigned char *q=s;while(n--)*p++=*q++;return d;}

static __inline QWORD rdtsc(void) {unsigned lo,hi;__asm__ __volatile__("rdtsc":"=a"(lo),"=d"(hi));return ((QWORD)hi<<32)|lo;}

/* ---- the real DLL ---------------------------------------------------------------------------------------------------------------- */
static HMODULE real;
static void *(WINAPI *real_Create)(UINT);
static HRESULT (WINAPI *real_VPS)(DWORD *,void *,BOOL,char **);
static HRESULT (WINAPI *real_VVS)(DWORD *,DWORD *,void *,BOOL,char **);
static void (WINAPI *real_Mute)(DWORD);
static DWORD (WINAPI *real_SWInfo)(void);
static int load_real(void) {
 char path[300];UINT n;
 if(real)return 1;
 n=GetSystemDirectoryA(path,260);if(!n||n>260)return 0;
 memcpy(path+n,"\\d3d8.dll",10);
 real=LoadLibraryA(path);if(!real)return 0;
 real_Create=GetProcAddress(real,"Direct3DCreate8");real_VPS=GetProcAddress(real,"ValidatePixelShader");real_VVS=GetProcAddress(real,"ValidateVertexShader");
 real_Mute=GetProcAddress(real,"DebugSetMute");real_SWInfo=GetProcAddress(real,"D3D8GetSWInfo");
 return real_Create!=NULL;
}

/* ---- the log ---------------------------------------------------------------------------------------------------------------------- */
static HANDLE logh=INVALID_HANDLE_VALUE;static char logbuf[16384];static unsigned loglen;
static void log_flush(void) {DWORD w;if(logh!=INVALID_HANDLE_VALUE&&loglen)WriteFile(logh,logbuf,loglen,&w,NULL);loglen=0;}
static void log_str(const char *s) {while(*s){if(loglen>=sizeof(logbuf)-1)log_flush();logbuf[loglen++]=*s++;}}
static void log_ch(char c) {if(loglen>=sizeof(logbuf)-1)log_flush();logbuf[loglen++]=c;}
/* v / 10 and v % 10 for a 64-bit v by two 32-bit divides (no C runtime: the compiler's 64-bit division helper is not there) */
static QWORD div10(QWORD v,unsigned *rem) {
 unsigned hi=(unsigned)(v>>32),lo=(unsigned)v,qh,ql,r;
 __asm__("divl %4":"=a"(qh),"=d"(r):"a"(hi),"d"(0),"r"(10U));
 __asm__("divl %4":"=a"(ql),"=d"(r):"a"(lo),"d"(r),"r"(10U));
 *rem=r;return ((QWORD)qh<<32)|ql;
}
static void log_u(QWORD v) {char t[24];int i=0;unsigned r;if(!v){log_ch('0');return;}while(v){v=div10(v,&r);t[i++]=(char)('0'+r);}while(i)log_ch(t[--i]);}
/* a non-negative double under 4e9 with d decimals (d <= 3), rounded */
static void log_f(double f,int d) {unsigned long w,fr,p=1;int i;double x;
 if(f<0){log_ch('-');f=-f;}
 for(i=0;i<d;i++)p*=10;
 x=f*(double)p+0.5;w=(unsigned long)x;
 fr=w-(w/p)*p;w/=p;
 log_u(w);if(d){log_ch('.');{unsigned long q=p/10;while(q){log_ch((char)('0'+(fr/q)%10));q/=10;}}}}
static void log_pad(const char *s,int w) {int n=0;const char *p=s;while(*p++)n++;log_str(s);while(n++<w)log_ch(' ');}

/* ---- the device's methods ----------------------------------------------------------------------------------------------------------- */
#define NMETH 97
static const char *const meth_name[NMETH]={"QueryInterface","AddRef","Release","TestCooperativeLevel","GetAvailableTextureMem","ResourceManagerDiscardBytes","GetDirect3D","GetDeviceCaps",
 "GetDisplayMode","GetCreationParameters","SetCursorProperties","SetCursorPosition","ShowCursor","CreateAdditionalSwapChain","Reset","Present","GetBackBuffer","GetRasterStatus","SetGammaRamp",
 "GetGammaRamp","CreateTexture","CreateVolumeTexture","CreateCubeTexture","CreateVertexBuffer","CreateIndexBuffer","CreateRenderTarget","CreateDepthStencilSurface","CreateImageSurface","CopyRects",
 "UpdateTexture","GetFrontBuffer","SetRenderTarget","GetRenderTarget","GetDepthStencilSurface","BeginScene","EndScene","Clear","SetTransform","GetTransform","MultiplyTransform","SetViewport",
 "GetViewport","SetMaterial","GetMaterial","SetLight","GetLight","LightEnable","GetLightEnable","SetClipPlane","GetClipPlane","SetRenderState","GetRenderState","BeginStateBlock","EndStateBlock",
 "ApplyStateBlock","CaptureStateBlock","DeleteStateBlock","CreateStateBlock","SetClipStatus","GetClipStatus","GetTexture","SetTexture","GetTextureStageState","SetTextureStageState","ValidateDevice",
 "GetInfo","SetPaletteEntries","GetPaletteEntries","SetCurrentTexturePalette","GetCurrentTexturePalette","DrawPrimitive","DrawIndexedPrimitive","DrawPrimitiveUP","DrawIndexedPrimitiveUP",
 "ProcessVertices","CreateVertexShader","SetVertexShader","GetVertexShader","DeleteVertexShader","SetVertexShaderConstant","GetVertexShaderConstant","GetVertexShaderDeclaration",
 "GetVertexShaderFunction","SetStreamSource","GetStreamSource","SetIndices","GetIndices","CreatePixelShader","SetPixelShader","GetPixelShader","DeletePixelShader","SetPixelShaderConstant",
 "GetPixelShaderConstant","GetPixelShaderFunction","DrawRectPatch","DrawTriPatch","DeletePatch"};
#define M_RELEASE 2
#define M_PRESENT 15
#define M_CLEAR 36
#define M_SETRS 50
#define M_SETTEX 61
#define M_SETTSS 63
#define M_DP 70
#define M_DIP 71
#define M_DPUP 72
#define M_DIPUP 73

/* the wrapped objects: vtable pointer first (COM), then the real interface */
typedef struct {void **vtbl;void *real;} wrap_t;
static void *dev_vtbl[NMETH];      /* our thunks */
static void **dev_real_vtbl;       /* the real device's table (read once) */
static void *d3d_vtbl[16];
static wrap_t dev_wrap,d3d_wrap;
static int dev_wrapped;
#ifdef TLPROBE
#define PROF_NAME "D3D8TLP v1.1"
#define PROF_LOG "C:\\D3D8TLP.TXT"
#else
#define PROF_NAME "D3D8PROF v1.1"
#define PROF_LOG "C:\\D3D8PROF.TXT"
#endif
/* v1.1: the vertex work (prof_enter keeps the state the draws read: render states, lights, the vertex shader, texture stage states) */
#define M_SETLIGHT 44
#define M_LIGHTENABLE 46
#define M_APPLYSB 54
#define M_SETVS 76
static DWORD rs[256],tss[8][32],vs_cur,lt_on[8],lt_type[256],sb_applied;
static void rs_defaults(void) {int s;rs[137]=1;rs[136]=1;rs[142]=1;rs[141]=1;rs[145]=1;rs[146]=2;for(s=0;s<8;s++){tss[s][11]=(DWORD)s;tss[s][1]=s?1:4;}}
static void lt_set(DWORD i,DWORD on) {if(i<256){if(on)lt_on[i>>5]|=1UL<<(i&31);else lt_on[i>>5]&=~(1UL<<(i&31));}}
/* the counts: primitives [0] all draws, [1] untransformed, [2] pretransformed (XYZRHW), [3] a vertex shader; of the untransformed: [4] lit,
 * [5] specular, [6] vertex fog, [7] texture coordinates generated, [8] texture transforms, [9] vertex blending, [10] user clip planes, [11]
 * clipping off, [12] normalized normals, [13] lit with a point or spot light, [14]-[20] lit by 0, 1, 2, 3, 4, 5-8, 9+ lights; [21] the indexed
 * draws' vertex ranges (NumVertices), [22] their primitives, [23] their triangle-list primitives, [24] the ranges of those */
static QWORD vw[25],vw_draws[4];static DWORD fvf_seen[8],fvf_n[8];
static void vw_draw(DWORD n,DWORD *a) {
 DWORD prims=n==70?a[3]:n==71?a[5]:n==72?a[2]:a[4],k,lights=0,ptspot=0,i;int cls;
 vw[0]+=prims;
 cls=(vs_cur&1)?3:(vs_cur&0xE)==4?2:1;
 vw[cls]+=prims;vw_draws[cls]++;
 if(!(vs_cur&1))for(k=0;k<8;k++){if(fvf_n[k]&&fvf_seen[k]==vs_cur){fvf_n[k]++;break;}if(!fvf_n[k]){fvf_seen[k]=vs_cur;fvf_n[k]=1;break;}}
 if(n==71||n==73){vw[21]+=a[3];vw[22]+=prims;if(a[1]==4){vw[23]+=prims;vw[24]+=a[3];}} /* NumVertices / NumVertexIndices */
 if(cls!=1)return;
 for(i=0;i<256;i++)if(lt_on[i>>5]&(1UL<<(i&31))){lights++;if(lt_type[i]==1||lt_type[i]==2)ptspot=1;}
 if(rs[137]&&(vs_cur&0x10)){vw[4]+=prims;if(ptspot)vw[13]+=prims;vw[lights<=4?14+lights:lights<=8?19:20]+=prims;}
 if(rs[29])vw[5]+=prims;
 if(rs[28]&&rs[140]&&!rs[35])vw[6]+=prims;
 {int s,g=0,t=0;for(s=0;s<8;s++){if(tss[s][1]==1)break;if(tss[s][11]>>16)g=1;if(tss[s][24])t=1;}if(g)vw[7]+=prims;if(t)vw[8]+=prims;}
 if(rs[151])vw[9]+=prims;if(rs[152])vw[10]+=prims;if(!rs[136])vw[11]+=prims;if(rs[143])vw[12]+=prims;
}
#ifdef TLPROBE
static DWORD tlp_asked,tlp_caps_d3d,tlp_caps_dev,tlp_svp;
static void tlp_caps(unsigned char *c) {DWORD *d=(DWORD*)c;d[28/4]|=0x10000UL;d[156/4]=0x3BUL;d[160/4]=8;d[164/4]=0;d[168/4]=0;d[172/4]=0;} /* D3DCAPS8: DevCaps |= HWTRANSFORMANDLIGHT; VertexProcessingCaps TEXGEN MATERIALSOURCE7 DIRECTIONALLIGHTS POSITIONALLIGHTS LOCALVIEWER; MaxActiveLights 8; no clip planes or vertex blending */
static HRESULT WINAPI my_d3d_caps(wrap_t *w,UINT ad,DWORD type,unsigned char *caps) {
 void *r=w->real;HRESULT hr=((HRESULT (WINAPI*)(void*,UINT,DWORD,void*))(*(void***)r)[13])(r,ad,type,caps);tlp_caps_d3d++;if(hr>=0&&caps&&type==1)tlp_caps(caps);return hr;}
static HRESULT WINAPI my_dev_caps(wrap_t *w,unsigned char *caps) {
 void *r=w->real;HRESULT hr=((HRESULT (WINAPI*)(void*,void*))(*(void***)r)[7])(r,caps);tlp_caps_dev++;if(hr>=0&&caps)tlp_caps(caps);return hr;}
static HRESULT WINAPI my_dev_params(wrap_t *w,DWORD *p) {
 void *r=w->real;HRESULT hr=((HRESULT (WINAPI*)(void*,void*))(*(void***)r)[9])(r,p);if(hr>=0&&p)p[3]=tlp_asked;return hr;}
void tlp_rs_ok(void); /* SetRenderState(D3DRS_SOFTWAREVERTEXPROCESSING) answered D3D_OK: this stub (stdcall, three arguments) */
__asm__(".text\n.globl _tlp_rs_ok\n_tlp_rs_ok:\n xor %eax,%eax\n ret $12\n");
#endif

/* counts: this frame and the whole run */
typedef struct {QWORD calls,cycles,prims;} acc_t;
static acc_t fr[NMETH],tot[NMETH];
static QWORD frame_t0,run_t0;static LONGLONG qpc0,qpf;static QWORD tsc0;static double tsc_mhz=1000.0;
static unsigned frames;static QWORD in_d3d_fr,in_d3d_tot,wall_tot;
/* the shadow stack of calls in flight (the return address is replaced by prof_epilogue) */
typedef struct {DWORD ret,n;QWORD t0;} shadow_t;
static shadow_t shadow[64];static int sp_top;static int overflow;

static void tsc_calibrate(void) {
 LONGLONG q;QWORD t=rdtsc();QueryPerformanceCounter(&q);
 if(qpf&&q>qpc0+qpf/4)tsc_mhz=(double)(LONGLONG)(t-tsc0)/((double)(q-qpc0)/(double)qpf)/1e6; /* after a quarter second: cycles / seconds */
}
static double ms(QWORD cyc) {return (double)(LONGLONG)cyc/(tsc_mhz*1000.0);}

static void frame_line(QWORD now) {
 QWORD wall=now-frame_t0;QWORD draws=fr[M_DP].calls+fr[M_DIP].calls+fr[M_DPUP].calls+fr[M_DIPUP].calls,prims=fr[M_DP].prims+fr[M_DIP].prims+fr[M_DPUP].prims+fr[M_DIPUP].prims;
 QWORD dcyc=fr[M_DP].cycles+fr[M_DIP].cycles+fr[M_DPUP].cycles+fr[M_DIPUP].cycles,scyc=fr[M_SETRS].cycles+fr[M_SETTEX].cycles+fr[M_SETTSS].cycles,scalls=fr[M_SETRS].calls+fr[M_SETTEX].calls+fr[M_SETTSS].calls;
 int i;
 frames++;wall_tot+=wall;in_d3d_tot+=in_d3d_fr;
 if(frames==1||(frames&63)==0)tsc_calibrate();
 log_str("F ");log_u(frames);log_str(" wall ");log_f(ms(wall),2);log_str(" d3d ");log_f(ms(in_d3d_fr),2);log_str(" game ");log_f(ms(wall>in_d3d_fr?wall-in_d3d_fr:0),2);
 log_str(" | draws ");log_u(draws);log_str(" prims ");log_u(prims);log_str(" drawms ");log_f(ms(dcyc),2);
 log_str(" | state ");log_u(scalls);log_str(" ms ");log_f(ms(scyc),2);log_str(" | present ");log_f(ms(fr[M_PRESENT].cycles),2);log_str(" clear ");log_f(ms(fr[M_CLEAR].cycles),2);
 log_str(" | other ");log_f(ms(in_d3d_fr-dcyc-scyc-fr[M_PRESENT].cycles-fr[M_CLEAR].cycles),2);log_ch('\n');
 for(i=0;i<NMETH;i++){tot[i].calls+=fr[i].calls;tot[i].cycles+=fr[i].cycles;tot[i].prims+=fr[i].prims;fr[i].calls=fr[i].cycles=fr[i].prims=0;}
 in_d3d_fr=0;frame_t0=now;
 if((frames&15)==0)log_flush();
}

/* called by the thunk prologue: n the method, ret the game's return address, args the arguments (args[0] = this, our wrapper) */
DWORD prof_bracket,prof_fullret; /* read by the prologue: whether the call is timed; else the game's return address to put back */
DWORD WINAPI prof_enter(DWORD n,DWORD ret,DWORD *args) {
 wrap_t *w=(wrap_t*)args[0];shadow_t *s;
 args[0]=(DWORD)w->real; /* the real device gets the call */
 if(sp_top>=64){overflow++;prof_bracket=0;prof_fullret=ret;return (DWORD)dev_real_vtbl[n];} /* cannot bracket: forwarded with the game's own return address */
 prof_bracket=1;
 s=&shadow[sp_top++];s->ret=ret;s->n=n;
 if(n==M_DP)fr[n].prims+=args[3];else if(n==M_DIP)fr[n].prims+=args[5];else if(n==M_DPUP)fr[n].prims+=args[2];else if(n==M_DIPUP)fr[n].prims+=args[4];
 if(n>=M_DP&&n<=M_DIPUP)vw_draw(n,args); /* v1.1 */
 else if(n==M_SETRS){if(args[1]<256)rs[args[1]]=args[2];
#ifdef TLPROBE
  if(args[1]==153&&(tlp_asked&0x80UL)){tlp_svp++;s->t0=rdtsc();return (DWORD)tlp_rs_ok;} /* mixed asked, software made: answered here */
#endif
 }
 else if(n==M_SETTSS){if(args[1]<8&&args[2]<32)tss[args[1]][args[2]]=args[3];}
 else if(n==M_SETVS)vs_cur=args[1];
 else if(n==M_LIGHTENABLE)lt_set(args[1],args[2]);
 else if(n==M_SETLIGHT){if(args[1]<256&&args[2])lt_type[args[1]]=*(DWORD*)args[2];}
 else if(n==M_APPLYSB)sb_applied++;
 s->t0=rdtsc();
 return (DWORD)dev_real_vtbl[n];
}
/* called with the method's result after it returned: accounts the time, returns the game's return address */
DWORD WINAPI prof_leave(DWORD result) {
 QWORD now=rdtsc();shadow_t *s=&shadow[--sp_top];QWORD d=now-s->t0;DWORD n=s->n;
 (void)result;
 fr[n].calls++;fr[n].cycles+=d;in_d3d_fr+=d;
 if(n==M_PRESENT)frame_line(now);
 return s->ret;
}

/* The thunks. Each pushes its method number and jumps to the common prologue, which pops the number and the return address, calls
 * prof_enter (the arguments stay in place on the stack, this replaced by the real device), pushes prof_epilogue as the new return
 * address and jumps to the real method. The real method cleans its arguments (stdcall) and returns into prof_epilogue, which hands the
 * result to prof_leave and returns to the game's address with the result in eax. eax, ecx, edx are caller-saved under stdcall, so
 * using them here is safe; nothing else is touched. */
__asm__(
 ".text\n"
 ".globl _prof_common\n_prof_common:\n"
 " pop %ecx\n"                /* ecx = method number; [esp] = the game's return address */
 " pop %edx\n"                /* edx = return address; [esp] = this */
 " push %esp\n"               /* args */
 " push %edx\n"               /* ret */
 " push %ecx\n"               /* n */
 " call _prof_enter@12\n"     /* returns the real method; the shadow holds ret; [esp] = this again */
 " cmpl $0,_prof_bracket\n"
 " je 1f\n"
 " push $_prof_epilogue\n"
 " jmp *%eax\n"
 "1:\n"                       /* shadow full: forward with the game's own return address, no timing */
 " push _prof_fullret\n"
 " jmp *%eax\n"
 ".globl _prof_epilogue\n_prof_epilogue:\n"
 " push %eax\n"               /* the result */
 " push %eax\n"
 " call _prof_leave@4\n"      /* eax = the game's return address */
 " xchg %eax,(%esp)\n"        /* [esp] = return address, eax = result */
 " ret\n"
);
#define THUNK(n) __asm__(".text\n.globl _dev_thunk_" #n "\n_dev_thunk_" #n ":\n push $" #n "\n jmp _prof_common\n");
#define T10(a) THUNK(a##0) THUNK(a##1) THUNK(a##2) THUNK(a##3) THUNK(a##4) THUNK(a##5) THUNK(a##6) THUNK(a##7) THUNK(a##8) THUNK(a##9)
THUNK(0) THUNK(1) THUNK(2) THUNK(3) THUNK(4) THUNK(5) THUNK(6) THUNK(7) THUNK(8) THUNK(9)
T10(1) T10(2) T10(3) T10(4) T10(5) T10(6) T10(7) T10(8) THUNK(90) THUNK(91) THUNK(92) THUNK(93) THUNK(94) THUNK(95) THUNK(96)
#define DECL(n) void dev_thunk_##n(void);
#define D10(a) DECL(a##0) DECL(a##1) DECL(a##2) DECL(a##3) DECL(a##4) DECL(a##5) DECL(a##6) DECL(a##7) DECL(a##8) DECL(a##9)
DECL(0) DECL(1) DECL(2) DECL(3) DECL(4) DECL(5) DECL(6) DECL(7) DECL(8) DECL(9)
D10(1) D10(2) D10(3) D10(4) D10(5) D10(6) D10(7) D10(8) DECL(90) DECL(91) DECL(92) DECL(93) DECL(94) DECL(95) DECL(96)
#define ADDR(n) (void*)dev_thunk_##n,
#define A10(a) ADDR(a##0) ADDR(a##1) ADDR(a##2) ADDR(a##3) ADDR(a##4) ADDR(a##5) ADDR(a##6) ADDR(a##7) ADDR(a##8) ADDR(a##9)
static void *const dev_thunks[NMETH]={ADDR(0) ADDR(1) ADDR(2) ADDR(3) ADDR(4) ADDR(5) ADDR(6) ADDR(7) ADDR(8) ADDR(9)
 A10(1) A10(2) A10(3) A10(4) A10(5) A10(6) A10(7) A10(8) ADDR(90) ADDR(91) ADDR(92) ADDR(93) ADDR(94) ADDR(95) ADDR(96)};

/* IDirect3D8: every method forwarded to the real object (this replaced), CreateDevice wrapped so the device can be */
__asm__(
 ".text\n"
 ".globl _d3d_common\n_d3d_common:\n"
 " pop %ecx\n"                /* method number; [esp] = ret, [esp+4] = this (our wrapper) */
 " mov 4(%esp),%eax\n"
 " mov 4(%eax),%edx\n"        /* the real IDirect3D8 */
 " mov %edx,4(%esp)\n"
 " mov (%edx),%eax\n"         /* its vtable */
 " jmp *(%eax,%ecx,4)\n"
);
#define FTHUNK(n) __asm__(".text\n.globl _d3d_thunk_" #n "\n_d3d_thunk_" #n ":\n push $" #n "\n jmp _d3d_common\n"); void d3d_thunk_##n(void);
FTHUNK(0) FTHUNK(1) FTHUNK(2) FTHUNK(3) FTHUNK(4) FTHUNK(5) FTHUNK(6) FTHUNK(7) FTHUNK(8) FTHUNK(9) FTHUNK(10) FTHUNK(11) FTHUNK(12) FTHUNK(13) FTHUNK(14)

static HRESULT WINAPI my_CreateDevice(wrap_t *w,UINT adapter,DWORD type,void *hwnd,DWORD flags,void *pp,void **out) {
 void *real_d3d=w->real;void **vt=*(void***)real_d3d;void *dev=NULL;HRESULT hr;DWORD asked=flags;
#ifdef TLPROBE
 tlp_asked=flags;flags=(flags&~0xD0UL)|0x20UL; /* hardware (40), mixed (80) and pure device (10) out, software (20) in */
#endif
 hr=((HRESULT (WINAPI*)(void*,UINT,DWORD,void*,DWORD,void*,void**))vt[15])(real_d3d,adapter,type,hwnd,flags,pp,&dev);
 log_str("CreateDevice: behaviour flags asked ");log_u(asked);log_str(" (");log_str(asked&0x40?"hardware":asked&0x80?"mixed":asked&0x20?"software":"none");
 log_str(" vertex processing");if(asked&0x10)log_str(", pure device");log_str("), made with ");log_u(flags);log_str(", result ");log_u((QWORD)(DWORD)hr);
#ifdef TLPROBE
 log_str("; the game read the caps (showing hardware T&L) ");log_u(tlp_caps_d3d);log_str(" times before");
#endif
 log_ch('\n');log_flush();
 if(hr<0||!dev){if(out)*out=dev;return hr;}
 if(dev_wrapped){ /* a second device: handed out unwrapped (the counters are for one) */
  log_str("second CreateDevice: not wrapped\n");log_flush();if(out)*out=dev;return hr;}
 dev_real_vtbl=*(void***)dev;memcpy(dev_vtbl,dev_thunks,sizeof(dev_vtbl));rs_defaults();
#ifdef TLPROBE
 dev_vtbl[7]=(void*)my_dev_caps;dev_vtbl[9]=(void*)my_dev_params;
#endif
 dev_wrap.vtbl=dev_vtbl;dev_wrap.real=dev;dev_wrapped=1;
 QueryPerformanceFrequency(&qpf);QueryPerformanceCounter(&qpc0);tsc0=rdtsc();frame_t0=run_t0=tsc0;
 log_str("device created: flags ");log_u(flags);log_str(" (behaviour: 0x20 software vertex processing, 0x40 hardware, 0x80 mixed, 0x02 FPU preserve, 0x04 multithreaded)\n");
 log_str("F frame wall d3d game | draws prims drawms | state(SetRS+SetTexture+SetTSS) ms | present clear | other, all ms; draw time includes the runtime's vertex processing and the driver's DrawPrimitives2\n");
 log_flush();
 if(out)*out=&dev_wrap;
 return hr;
}

/* ---- exports --------------------------------------------------------------------------------------------------------------------- */
void *WINAPI Direct3DCreate8(UINT sdk) {
 void *d3d;
 if(!load_real())return NULL;
 d3d=real_Create(sdk);if(!d3d)return NULL;
 if(logh==INVALID_HANDLE_VALUE){logh=CreateFileA(PROF_LOG,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
  log_str(PROF_NAME ": Direct3DCreate8(");log_u(sdk);log_str(")\n");log_flush();}
 {int i;for(i=0;i<16;i++)d3d_vtbl[i]=0;
  d3d_vtbl[0]=(void*)d3d_thunk_0;d3d_vtbl[1]=(void*)d3d_thunk_1;d3d_vtbl[2]=(void*)d3d_thunk_2;d3d_vtbl[3]=(void*)d3d_thunk_3;d3d_vtbl[4]=(void*)d3d_thunk_4;
  d3d_vtbl[5]=(void*)d3d_thunk_5;d3d_vtbl[6]=(void*)d3d_thunk_6;d3d_vtbl[7]=(void*)d3d_thunk_7;d3d_vtbl[8]=(void*)d3d_thunk_8;d3d_vtbl[9]=(void*)d3d_thunk_9;
  d3d_vtbl[10]=(void*)d3d_thunk_10;d3d_vtbl[11]=(void*)d3d_thunk_11;d3d_vtbl[12]=(void*)d3d_thunk_12;d3d_vtbl[13]=(void*)d3d_thunk_13;d3d_vtbl[14]=(void*)d3d_thunk_14;
  d3d_vtbl[15]=(void*)my_CreateDevice;
#ifdef TLPROBE
  d3d_vtbl[13]=(void*)my_d3d_caps;
#endif
  }
 d3d_wrap.vtbl=d3d_vtbl;d3d_wrap.real=d3d;
 return &d3d_wrap;
}
HRESULT WINAPI ValidatePixelShader(DWORD *a,void *b,BOOL c,char **d) {if(!load_real()||!real_VPS)return (HRESULT)0x80004001L;return real_VPS(a,b,c,d);}
HRESULT WINAPI ValidateVertexShader(DWORD *a,DWORD *b,void *c,BOOL d,char **e) {if(!load_real()||!real_VVS)return (HRESULT)0x80004001L;return real_VVS(a,b,c,d,e);}
void WINAPI DebugSetMute(DWORD a) {if(load_real()&&real_Mute)real_Mute(a);}
DWORD WINAPI D3D8GetSWInfo(void) {if(load_real()&&real_SWInfo)return real_SWInfo();return 0;}

/* the summary at exit */
static void summary(void) {
 int i,j,order[NMETH],n=0;QWORD sum=0;
 if(logh==INVALID_HANDLE_VALUE)return;
 for(i=0;i<NMETH;i++){tot[i].calls+=fr[i].calls;tot[i].cycles+=fr[i].cycles;tot[i].prims+=fr[i].prims;if(tot[i].calls){order[n++]=i;sum+=tot[i].cycles;}}
 for(i=1;i<n;i++){int k=order[i];j=i;while(j>0&&tot[order[j-1]].cycles<tot[k].cycles){order[j]=order[j-1];j--;}order[j]=k;}
 log_str("\nsummary: ");log_u(frames);log_str(" frames, TSC ");log_f(tsc_mhz,1);log_str(" MHz; a frame on average: wall ");log_f(frames?ms(wall_tot)/frames:0,2);
 log_str(" ms, in d3d8 ");log_f(frames?ms(in_d3d_tot)/frames:0,2);log_str(" ms, game ");log_f(frames?ms(wall_tot>in_d3d_tot?wall_tot-in_d3d_tot:0)/frames:0,2);log_str(" ms");
 if(overflow){log_str("; ");log_u(overflow);log_str(" calls not timed (nesting over 64)");}
 log_str("\nmethod                        calls/frame   ms/frame   us/call   prims/frame   share\n");
 for(i=0;i<n;i++){acc_t *a=&tot[order[i]];
  log_pad(meth_name[order[i]],30);log_f(frames?(double)(LONGLONG)a->calls/frames:0,1);log_str("   ");log_f(frames?ms(a->cycles)/frames:0,3);log_str("   ");
  log_f(a->calls?ms(a->cycles)*1000.0/(double)(LONGLONG)a->calls:0,2);log_str("   ");log_f(frames?(double)(LONGLONG)a->prims/frames:0,1);log_str("   ");
  log_f(sum?100.0*(double)(LONGLONG)a->cycles/(double)(LONGLONG)sum:0,1);log_str("%\n");}
  { /* v1.1: the vertex work */
  static const char *const vn[14]={"","untransformed","pretransformed (XYZRHW)","a vertex shader","lit","specular","vertex fog","texture coordinates generated",
   "texture transforms","vertex blending","user clip planes","clipping off","normalized normals","lit with a point or spot light"};int i;QWORD a=vw[0]?vw[0]:1;
  log_str("\nVertex work (v1.1; the state at each draw as this proxy saw it set; ");log_u(sb_applied);log_str(" ApplyStateBlock calls, whose states it does not see): ");
  log_u(vw[0]);log_str(" primitives in all\n");
  for(i=1;i<4;i++){log_str("   ");log_pad(vn[i],34);log_f(100.0*(double)(LONGLONG)vw[i]/(double)(LONGLONG)a,1);log_str("% of the primitives (");log_u(vw_draws[i]);log_str(" draws)\n");}
  log_str("   of the untransformed primitives:\n");
  for(i=4;i<14;i++){QWORD u=vw[1]?vw[1]:1;log_str("      ");log_pad(vn[i],34);log_f(100.0*(double)(LONGLONG)vw[i]/(double)(LONGLONG)u,1);log_str("%\n");}
  log_str("      lit primitives by the lights enabled: 0 ");log_u(vw[14]);log_str(", 1 ");log_u(vw[15]);log_str(", 2 ");log_u(vw[16]);log_str(", 3 ");log_u(vw[17]);log_str(", 4 ");log_u(vw[18]);
  log_str(", 5-8 ");log_u(vw[19]);log_str(", 9+ ");log_u(vw[20]);log_ch('\n');
  log_str("   vertex formats (FVF in decimal: draws):");for(i=0;i<8;i++)if(fvf_n[i]){log_str(" ");log_u(fvf_seen[i]);log_str(": ");log_u(fvf_n[i]);log_str(";");}log_ch('\n');
  log_str("   indexed draws: ");log_u(vw[22]);log_str(" primitives over vertex ranges of ");log_u(vw[21]);log_str(" vertices in all; triangle lists ");log_u(vw[23]);log_str(" triangles over ");log_u(vw[24]);
  log_str(" vertices (");log_f(vw[23]?(double)(LONGLONG)vw[24]/(double)(LONGLONG)vw[23]:0,2);log_str(" a triangle: software vertex processing transforms every vertex of a draw's range)\n");
#ifdef TLPROBE
  log_str("caps probe: the caps read through IDirect3D8 ");log_u(tlp_caps_d3d);log_str(" times and the device ");log_u(tlp_caps_dev);log_str(" times (hardware T&L shown); behaviour flags asked ");log_u(tlp_asked);
  log_str("; D3DRS_SOFTWAREVERTEXPROCESSING set ");log_u(tlp_svp);log_str(" times (answered here)\n");
#endif
 }
 log_flush();CloseHandle(logh);logh=INVALID_HANDLE_VALUE;
}

BOOL WINAPI DllMain(HMODULE h,DWORD reason,void *reserved) {
 (void)h;(void)reserved;
 if(reason==0)summary(); /* DLL_PROCESS_DETACH */
 return 1;
}
