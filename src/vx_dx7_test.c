/* DirectX 7 runtime -> VXDDRAW HAL -> VxD -> GPU test. MIT, 2026.
 * No software/reference device is selected. Default (no argument) is --texture; --query only queries.
 * --draw uses runtime-allocated offscreen color/depth surfaces in the
 * current display mode, checks pixels and native counters, then previews. */
#define COBJMACROS
#define INITGUID
#define DIRECTDRAW_VERSION 0x0700
#define DIRECT3D_VERSION 0x0700
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vx_curchk.h"
#include <math.h>
#include <io.h>
#include "vx_native3d.h"
#include "vx_d3d_diag.h"
#include "ddrawi.h"
typedef struct tl_vertex {float x,y,z,rhw;DWORD color,spec;float u,v;} tl_vertex;
/* Open Watcom declares IID_IDirectDraw7 but its dxguid.lib omits it. */
static const GUID iid_dd7={0x15E65EC0UL,0x3B9C,0x11D2,{0xB9,0x2F,0,0x60,0x97,0x97,0xEA,0x5B}};
static FILE *logfile;
static IDirectDraw7 *dd;
static IDirect3D7 *d3d;
static IDirect3DDevice7 *dev;
static IDirectDrawSurface7 *rt,*depth,*primary;
static IDirectDrawClipper *clipper;
static HMODULE ddlib,halmod;
static HANDLE endpoint=INVALID_HANDLE_VALUE;
static DWORD (WINAPI *stats)(vx_d3d_diag *);
static DWORD failures,devices;
static HWND window;
static int note(const char *s,HRESULT hr) {
 fprintf(logfile,"%s = %08lX\n",s,(unsigned long)hr);fflush(logfile);
 if(FAILED(hr)){failures++;return 0;}return 1;
}
static int native_info(vx3dn_info *p) {
 DWORD got=0;memset(p,0,sizeof(*p));
 return endpoint!=INVALID_HANDLE_VALUE&&DeviceIoControl(endpoint,VX_IOCTL_3DN_INFO,0,0,p,sizeof(*p),&got,0)&&got==sizeof(*p)&&p->abi==VX3DN_ABI;
}
static void diagnostics(void) {
 vx_d3d_diag s;vx3dn_info n;
 memset(&s,0,sizeof(s));s.cb=sizeof(s);
 if(stats&&stats(&s))fprintf(logfile,"HAL M2 enabled=%lu creates=%lu DP2=%lu legacy=%lu RS=%lu packets=%lu last callback (last failing one, if any)=%lu hr=%08lX opcode=%lu offset=%lu\n",
  (unsigned long)s.enabled,(unsigned long)s.creates,(unsigned long)s.dp2,(unsigned long)s.legacy_draws,(unsigned long)s.render_states,
  (unsigned long)s.native_packets,(unsigned long)s.last_callback,(unsigned long)s.last_hr,(unsigned long)s.last_opcode,(unsigned long)s.last_offset);
 if(stats&&s.cb==sizeof(s))fprintf(logfile,"HAL primitives left out (state not drawable): %lu, last reason %03lX\n",(unsigned long)s.dropped,(unsigned long)s.drop_why);
 if(stats&&s.cb==sizeof(s)&&s.dropped)fprintf(logfile,"   at the last one: texture error %08lX, TEXTUREHANDLE %lu, stage 0 texture %lu, COLOROP %lu ARG1 %lX ARG2 %lX, ALPHAOP %lu, stage 1 COLOROP %lu\n",
  (unsigned long)s.drop_state[0],(unsigned long)s.drop_state[1],(unsigned long)s.drop_state[2],(unsigned long)s.drop_state[3],(unsigned long)s.drop_state[4],(unsigned long)s.drop_state[5],(unsigned long)s.drop_state[6],(unsigned long)s.drop_state[7]);
 if(halmod) { /* v2.17: the HAL's event log (v10.185) */
  static const char *cbn[13]={"?","ContextCreate","DrawPrimitives2","?","?","ValidateTextureStageState","CreateSurfaceEx","packet refused","refused packet offsets","refused packet vertex","refused packet state","fog","packet waited (VxD busy)"};
  DWORD (WINAPI *ev)(DWORD *,DWORD)=(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(halmod,"VX_D3D_QueryEvents");DWORD o[1+16*6],k,c;
  static DWORD shown;
  if(ev&&(c=ev(o,sizeof(o)/sizeof(o[0])))!=0&&o[0]!=shown) {
   DWORD m=(c-1)/6,first=o[0]-m;
   fprintf(logfile,"HAL events %lu in all:\n",(unsigned long)o[0]);
   for(k=0;k<m;k++)if(first+k>=shown)fprintf(logfile,"   #%lu %s result %08lX reason %lX: %08lX %08lX %08lX\n",(unsigned long)(first+k+1),cbn[o[1+k*6]<13?o[1+k*6]:0],
    (unsigned long)o[2+k*6],(unsigned long)o[3+k*6],(unsigned long)o[4+k*6],(unsigned long)o[5+k*6],(unsigned long)o[6+k*6]);
   shown=o[0];
  }
 }
 if(native_info(&n))fprintf(logfile,"VxD ready=%lu generation=%lu mode=%lux%lux%lu pitch=%lu triangles=%lu CPU-clears=%lu fault=%lu last=%lu status=%08lX\n",
  (unsigned long)n.ready,(unsigned long)n.generation,(unsigned long)n.width,(unsigned long)n.height,(unsigned long)n.bpp,(unsigned long)n.pitch,
  (unsigned long)n.triangles,(unsigned long)n.clears,(unsigned long)n.fault,(unsigned long)n.last_result,(unsigned long)n.last_status);
 fflush(logfile);
}
/* v1.2: what the DirectDraw runtime holds for the display driver (Win9x: the interface points at DDRAWI_DIRECTDRAW_INT). */
static void gbl_dump(const char *who,void *iface) {
 LPDDRAWI_DIRECTDRAW_INT i=(LPDDRAWI_DIRECTDRAW_INT)iface;LPDDRAWI_DIRECTDRAW_GBL g;
 if(!i||IsBadReadPtr(i,sizeof(*i))||!i->lpLcl||IsBadReadPtr(i->lpLcl,sizeof(*i->lpLcl))){fprintf(logfile,"%s: runtime object unreadable\n",who);return;}
 g=i->lpLcl->lpGbl;
 if(!g||IsBadReadPtr(g,sizeof(*g))){fprintf(logfile,"%s: global object unreadable\n",who);return;}
 fprintf(logfile,"%s: global %08lX flags %08lX (no hardware %s, display driver %s) caps %08lX caps2 %08lX surface caps %08lX D3D global %08lX callbacks %08lX/%08lX/%08lX extended %08lX HAL instance %08lX local flags %08lX\n",who,
  (unsigned long)g,(unsigned long)g->dwFlags,(g->dwFlags&DDRAWI_NOHARDWARE)?"YES":"no",(g->dwFlags&DDRAWI_DISPLAYDRV)?"yes":"no",(unsigned long)g->ddCaps.dwCaps,(unsigned long)g->ddCaps.dwCaps2,(unsigned long)g->ddCaps.ddsCaps.dwCaps,
  (unsigned long)g->lpD3DGlobalDriverData,(unsigned long)g->lpD3DHALCallbacks,(unsigned long)g->lpD3DHALCallbacks2,(unsigned long)g->lpD3DHALCallbacks3,(unsigned long)g->lpD3DExtendedCaps,(unsigned long)g->hInstance,(unsigned long)i->lpLcl->dwLocalFlags);
}
static const char *guid_name(DWORD d) {
 switch(d){case 0xDDF41230UL:return "D3DCallbacks3";case 0x0BA584E1UL:return "D3DCallbacks2";case 0x7DE41F80UL:return "D3DExtendedCaps";
 case 0x93869880UL:return "ZPixelFormats";case 0x406B2F00UL:return "Miscellaneous2Callbacks";case 0x2E04FFA0UL:return "D3DParseUnknownCommandCallback";}
 return "";
}
static void trace_report(void);
/* v1.6: DirectDraw 9.0c drops the HAL when IsBadCodePtr says a function in a GetDriverInfo reply is bad (DDRAW.DLL 4.09.00.0904 at BAAB915F).
 * Probe the HAL's code (its first code page, 0xA0001000) the same way from this process. */
static void code_probe(const char *when) {
 FARPROC f=(FARPROC)0xA0001000UL;
 fprintf(logfile,"IsBadCodePtr(A0001000) %s: %s; IsBadReadPtr: %s; GetModuleHandle(VXDDRAW.DLL) %08lX\n",when,IsBadCodePtr(f)?"BAD":"ok",IsBadReadPtr((const void*)f,4)?"BAD":"ok",(unsigned long)GetModuleHandleA("VXDDRAW.DLL"));
 fflush(logfile);
}
static void runtime_report(void) {
 HRESULT (WINAPI *create1)(GUID *,LPDIRECTDRAW *,IUnknown *);LPDIRECTDRAW dd1=0;DDCAPS hw,hel;HRESULT hr;
 create1=(HRESULT (WINAPI *)(GUID *,LPDIRECTDRAW *,IUnknown *))GetProcAddress(ddlib,"DirectDrawCreate");
 if(create1){
  hr=create1(0,&dd1,0);fprintf(logfile,"DirectDrawCreate (DirectX 1 interface, as the 2D tests use) = %08lX\n",(unsigned long)hr);
  if(SUCCEEDED(hr)){
   memset(&hw,0,sizeof(hw));hw.dwSize=sizeof(hw);memset(&hel,0,sizeof(hel));hel.dwSize=sizeof(hel);
   if(SUCCEEDED(IDirectDraw_GetCaps(dd1,&hw,&hel)))fprintf(logfile,"   its driver caps: dwCaps %08lX (no hardware %s, 3D %s) dwCaps2 %08lX surface caps %08lX\n",(unsigned long)hw.dwCaps,(hw.dwCaps&DDCAPS_NOHARDWARE)?"YES":"no",(hw.dwCaps&DDCAPS_3D)?"yes":"no",(unsigned long)hw.dwCaps2,(unsigned long)hw.ddsCaps.dwCaps);
   gbl_dump("   DirectDrawCreate object",dd1);
  }
 }
 gbl_dump("DirectDrawCreateEx object",dd);
 if(dd1)IDirectDraw_Release(dd1);
 trace_report();
}
/* v2.40 --draws: the distinct draw states the HAL sent since the last --draws (any process: run the application first), then cleared */
/* v2.82 (v10.321): the HAL keeps the records only from the first --draws on (or with ACCEL_D3D_LOG = 1): an empty answer says so */
static void draws_report(void) {
 DWORD (WINAPI *q)(DWORD *,DWORD)=0;static DWORD o[1+32*40];DWORD n,i,k;
 if(halmod)q=(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(halmod,"VX_D3D_QueryDraws");
 if(!q){fprintf(logfile,"HAL draw records unavailable (VXDDRAW.DLL older than v10.217)\n");return;}
 n=q(o,0x80000000UL|(sizeof(o)/sizeof(o[0])));
 if(!n){fprintf(logfile,"HAL draw records: no answer\n");return;}
 fprintf(logfile,"== draw states the HAL sent: %lu\n",(unsigned long)o[0]);
 if(!o[0])fprintf(logfile,"   (driver v10.321 on: records are kept from the first --draws, or always with ACCEL_D3D_LOG = 1; run the application, then --draws again)\n");
 for(i=0;i*40+1<n;i++){const DWORD *r=o+1+i*40;
  fprintf(logfile,"#%lu pid %08lX: %lu packets, %lu triangles; op %lu prim %lu target %lu depth %lu\n",(unsigned long)i,(unsigned long)r[3],(unsigned long)r[1],(unsigned long)r[2],(unsigned long)r[4],(unsigned long)r[5],(unsigned long)r[6],(unsigned long)r[7]);
  fprintf(logfile,"   tex %lu format %lu size %lux%lu filter/addr %06lX; z %03lX blend %06lX alpha %04lX mask/dither/stencil %03lX fog %04lX\n",(unsigned long)r[8],(unsigned long)r[9],(unsigned long)(r[10]>>16),(unsigned long)(r[10]&0xFFFF),(unsigned long)r[11],
   (unsigned long)r[12],(unsigned long)r[13],(unsigned long)r[14],(unsigned long)r[15],(unsigned long)r[16]);
  fprintf(logfile,"   program %lu instructions, kflags %lX, units/layout/ps %06lX:",(unsigned long)r[17],(unsigned long)r[18],(unsigned long)r[19]);
  for(k=0;k<8;k++)fprintf(logfile," %08lX",(unsigned long)r[32+k]);
  fprintf(logfile,"\n   vertex 0: colour %08lX specular %08lX uv %08lX %08lX xyzw %08lX %08lX %08lX %08lX; diffuse min %08lX max %08lX, specular max %08lX\n",(unsigned long)r[20],(unsigned long)r[21],(unsigned long)r[22],(unsigned long)r[23],
   (unsigned long)r[24],(unsigned long)r[25],(unsigned long)r[26],(unsigned long)r[27],(unsigned long)r[28],(unsigned long)r[29],(unsigned long)r[30]);}
 fflush(logfile);
}
static void trace_report(void) {
 DWORD (WINAPI *trace)(DWORD *,DWORD)=0;DWORD t[28+32*4],n,i;
 if(halmod)trace=(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(halmod,"VX_D3D_QueryTrace");
 if(!trace){fprintf(logfile,"HAL trace export unavailable (VXDDRAW.DLL older than v10.150)\n");return;}
 memset(t,0,sizeof(t));n=trace(t,sizeof(t)/sizeof(t[0]));
 if(n<28||t[0]!=0x00020000UL){fprintf(logfile,"HAL trace empty or older layout\n");return;}
 fprintf(logfile,"HAL: DriverInit D3D setups %lu, ACCEL_D3D %lu; DRV built the HAL info %lu time(s), last DDHAL_SetInfo %s with D3D %s, dwCaps %08lX surface caps %08lX; GetDriverInfo calls %lu\n",
  (unsigned long)t[1],(unsigned long)t[2],(unsigned long)t[4],(t[5]&1)?"accepted":"REFUSED",(t[5]&0x100)?"advertised":"not advertised",(unsigned long)t[6],(unsigned long)t[7],(unsigned long)t[3]);
 if(t[8])fprintf(logfile,"   last positive GetDriverInfo reply %08lX %s: %08lX %08lX %08lX %08lX %08lX %08lX %08lX\n",(unsigned long)t[8],guid_name(t[8]),(unsigned long)t[9],(unsigned long)t[10],(unsigned long)t[11],(unsigned long)t[12],(unsigned long)t[13],(unsigned long)t[14],(unsigned long)t[15]);
 if(t[16]==0x10)fprintf(logfile,"   RenderPrimitive draw refused %08lX: %lu triangles, highest vertex %lu; first triangle %08lX %08lX; first vertex %08lX %08lX %08lX %08lX %08lX %08lX\n",(unsigned long)t[17],(unsigned long)t[18],(unsigned long)t[19],
  (unsigned long)t[20],(unsigned long)t[21],(unsigned long)t[22],(unsigned long)t[23],(unsigned long)t[24],(unsigned long)t[25],(unsigned long)t[26],(unsigned long)t[27]);
 else if(t[16])fprintf(logfile,"   refused RenderPrimitive %s buffer %08lX: words %08lX %08lX %08lX %08lX; [word0] %08lX %08lX; [word1] %08lX %08lX %08lX %08lX\n",t[16]==1?"execute":"TL",(unsigned long)t[17],
  (unsigned long)t[18],(unsigned long)t[19],(unsigned long)t[20],(unsigned long)t[21],(unsigned long)t[22],(unsigned long)t[23],(unsigned long)t[24],(unsigned long)t[25],(unsigned long)t[26],(unsigned long)t[27]);
 for(i=28;i+4<=n;i+=4)fprintf(logfile,"   GetDriverInfo %08lX %s: expected %lu, returned %lu, result %08lX\n",(unsigned long)t[i],guid_name(t[i]),(unsigned long)t[i+1],(unsigned long)t[i+2],(unsigned long)t[i+3]);
 fflush(logfile);
}
static HRESULT CALLBACK enumerate(LPSTR description,LPSTR name,LPD3DDEVICEDESC7 caps,LPVOID unused) {
 (void)unused;devices++;
 fprintf(logfile,"Device %s / %s: devcaps=%08lX render-depths=%08lX Z-depths=%08lX textures=%u\n",name,description,
  (unsigned long)caps->dwDevCaps,(unsigned long)caps->dwDeviceRenderBitDepth,(unsigned long)caps->dwDeviceZBufferBitDepth,(unsigned)caps->wMaxSimultaneousTextures);
 return D3DENUMRET_OK;
}
static void triangle(tl_vertex *v,float z,DWORD a,DWORD b,DWORD c) {
 memset(v,0,sizeof(tl_vertex)*3);
 v[0].x=32;v[0].y=32;v[1].x=224;v[1].y=32;v[2].x=32;v[2].y=160;
 v[0].z=v[1].z=v[2].z=z;v[0].rhw=v[1].rhw=v[2].rhw=1;
 v[0].color=a;v[1].color=b;v[2].color=c;
}
static int draw(tl_vertex *v) {
 HRESULT hr;
 if(!note("BeginScene",IDirect3DDevice7_BeginScene(dev)))return 0;
 hr=IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLELIST,D3DFVF_TLVERTEX,v,3,0);
 if(!note("DrawPrimitive(TLVERTEX)",hr)){IDirect3DDevice7_EndScene(dev);return 0;}
 return note("EndScene",IDirect3DDevice7_EndScene(dev));
}
static DWORD rgb(const DDSURFACEDESC2 *s,DWORD x,DWORD y) {
 BYTE *p=(BYTE*)s->lpSurface+y*s->lPitch;DWORD q;
 if(s->ddpfPixelFormat.dwRGBBitCount==32){memcpy(&q,p+x*4,4);return q&0xFFFFFFUL;}
 {WORD w;DWORD r,g,b;memcpy(&w,p+x*2,2);r=(w>>11)&31;g=(w>>5)&63;b=w&31;
  return (((r<<3)|(r>>2))<<16)|(((g<<2)|(g>>4))<<8)|(b<<3)|(b>>2);}
}
static int samples(DWORD *a,DWORD *b,DWORD *c) {
 DDSURFACEDESC2 s;memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
 if(!note("Lock(readback)",IDirectDrawSurface7_Lock(rt,0,&s,DDLOCK_WAIT|DDLOCK_READONLY,0)))return 0;
 *a=rgb(&s,40,40);*b=rgb(&s,80,72);*c=rgb(&s,240,176);
 fprintf(logfile,"RGB samples (40,40)=%06lX (80,72)=%06lX outside=%06lX\n",(unsigned long)*a,(unsigned long)*b,(unsigned long)*c);
 return note("Unlock",IDirectDrawSurface7_Unlock(rt,0));
}
static void check(const char *s,int ok) {
 fprintf(logfile,"%s: %s\n",s,ok?"PASS":"FAIL");fflush(logfile);if(!ok)failures++;
}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_TIMER||(m==WM_KEYDOWN&&a==VK_ESCAPE)){DestroyWindow(w);return 0;}
 if(m==WM_DESTROY){PostQuitMessage(0);return 0;}
 return DefWindowProc(w,m,a,b);
}
/* v2.6: the depth formats the HAL offers (v10.177): 0 none, 1 D32, 2 Z16, 3 D24S8 */
static const char *zname[4]={"none","D32","Z16","D24S8"};
static int make_depth(int z) {
 DDSURFACEDESC2 s;char line[64];
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;
 s.dwWidth=256;s.dwHeight=192;s.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|DDSCAPS_VIDEOMEMORY;
 s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);s.ddpfPixelFormat.dwFlags=DDPF_ZBUFFER;
 if(z==1){s.ddpfPixelFormat.dwZBufferBitDepth=32;s.ddpfPixelFormat.dwZBitMask=0xFFFFFFFFUL;}
 if(z==2){s.ddpfPixelFormat.dwZBufferBitDepth=16;s.ddpfPixelFormat.dwZBitMask=0xFFFFUL;}
 if(z==3){s.ddpfPixelFormat.dwFlags|=DDPF_STENCILBUFFER;s.ddpfPixelFormat.dwZBufferBitDepth=32;s.ddpfPixelFormat.dwStencilBitDepth=8;
  s.ddpfPixelFormat.dwZBitMask=0xFFFFFF00UL;s.ddpfPixelFormat.dwStencilBitMask=0xFFUL;}
 sprintf(line,"CreateSurface(%s depth)",zname[z]);if(!note(line,IDirectDraw7_CreateSurface(dd,&s,&depth,0)))return 0;
 sprintf(line,"Attach %s",zname[z]);return note(line,IDirectDrawSurface7_AddAttachedSurface(rt,depth));
}
static int setup_z(int z) {
 DDSURFACEDESC2 s;D3DVIEWPORT7 vp;D3DDEVICEDESC7 caps;
 if(!rt){
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;
 s.dwWidth=256;s.dwHeight=192;s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE|DDSCAPS_VIDEOMEMORY;
 if(!note("CreateSurface(color 256x192 video)",IDirectDraw7_CreateSurface(dd,&s,&rt,0)))return 0;
 }
 if(z&&!make_depth(z))return 0;
 if(!note("CreateDevice(IID_IDirect3DHALDevice)",IDirect3D7_CreateDevice(d3d,&IID_IDirect3DHALDevice,rt,&dev)))return 0;
 memset(&caps,0,sizeof(caps));if(!note("GetCaps",IDirect3DDevice7_GetCaps(dev,&caps)))return 0;
 if(!(caps.dwDevCaps&D3DDEVCAPS_HWRASTERIZATION)){check("hardware device required",0);return 0;}
 memset(&vp,0,sizeof(vp));vp.dwWidth=256;vp.dwHeight=192;vp.dvMaxZ=1;
 if(!note("SetViewport",IDirect3DDevice7_SetViewport(dev,&vp)))return 0;
#define RS(k,v) if(!note(#k,IDirect3DDevice7_SetRenderState(dev,k,v)))return 0
 RS(D3DRENDERSTATE_ZENABLE,FALSE);RS(D3DRENDERSTATE_LIGHTING,FALSE);RS(D3DRENDERSTATE_CULLMODE,D3DCULL_NONE);
 RS(D3DRENDERSTATE_SPECULARENABLE,FALSE);RS(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);RS(D3DRENDERSTATE_FOGENABLE,FALSE);
#undef RS
#define TS(k,v) if(!note(#k,IDirect3DDevice7_SetTextureStageState(dev,0,k,v)))return 0
 TS(D3DTSS_COLOROP,D3DTOP_SELECTARG1);TS(D3DTSS_COLORARG1,D3DTA_DIFFUSE);
 TS(D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);TS(D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);
#undef TS
 return 1;
}
static int setup(int with_depth) {return setup_z(with_depth?1:0);}
static void drop_device(void) {
 if(dev){IDirect3DDevice7_Release(dev);dev=0;}
 if(depth){IDirectDrawSurface7_DeleteAttachedSurface(rt,0,depth);IDirectDrawSurface7_Release(depth);depth=0;}
}
static int run_tests(int with_depth) {
 tl_vertex v[3];DWORD a,b,c;vx3dn_info before,after;int have;
 have=native_info(&before);
 if(!note("Flat shade",IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_SHADEMODE,D3DSHADE_FLAT))||
    !note("Clear color",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF101820UL,1,0)))return 0;
 triangle(v,.5F,0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL);
 if(!draw(v)||!samples(&a,&b,&c))return 0;
 check("flat provoking vertex and outside coverage",a==0xFF0000UL&&b==0xFF0000UL&&((c>>16)&255)<32&&c!=a);
 if(!note("Gouraud shade",IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_SHADEMODE,D3DSHADE_GOURAUD))||
    !note("Clear color",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF101820UL,1,0))||!draw(v)||!samples(&a,&b,&c))return 0;
 check("Gouraud interpolation",((a>>16)&255)>((a>>8)&255)&&((b>>16)&255)>16&&((b>>8)&255)>16&&(b&255)>16&&a!=b);
 if(with_depth) {
 if(!note("ZENABLE",IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_ZENABLE,TRUE))||
    !note("ZWRITEENABLE",IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_ZWRITEENABLE,TRUE))||
    !note("ZFUNC LESS",IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_ZFUNC,D3DCMP_LESS))||
    !note("Clear color+Z",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF101820UL,1,0)))return 0;
 triangle(v,.25F,0xFFFF0000UL,0xFFFF0000UL,0xFFFF0000UL);if(!draw(v))return 0;
 triangle(v,.75F,0xFF0000FFUL,0xFF0000FFUL,0xFF0000FFUL);if(!draw(v)||!samples(&a,&b,&c))return 0;
 check("D32 near triangle rejects later far triangle",a==0xFF0000UL&&b==0xFF0000UL);
 }
 check("native GPU triangle counter increased",have&&native_info(&after)&&after.triangles-before.triangles>=(with_depth?4UL:2UL)&&!after.fault);
 return 1;
}
static void preview(void) {
 DDSURFACEDESC2 s;RECT dr,sr;POINT p;MSG msg;
 if(!note("Z off for preview",IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_ZENABLE,FALSE)))return;
 {tl_vertex v[3];triangle(v,.5F,0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL);
  if(!note("Preview clear",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF101820UL,1,0))||!draw(v))return;}
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS;s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
 if(!note("Create primary for preview",IDirectDraw7_CreateSurface(dd,&s,&primary,0)))return;
 if(!note("CreateClipper",IDirectDraw7_CreateClipper(dd,0,&clipper,0))||
    !note("Clipper HWND",IDirectDrawClipper_SetHWnd(clipper,0,window))||
    !note("SetClipper",IDirectDrawSurface7_SetClipper(primary,clipper)))return;
 p.x=0;p.y=0;ClientToScreen(window,&p);SetRect(&dr,p.x,p.y,p.x+256,p.y+192);SetRect(&sr,0,0,256,192);
 if(!note("Present via DirectDraw Blt",IDirectDrawSurface7_Blt(primary,&dr,rt,&sr,DDBLT_WAIT,0)))return;
 SetTimer(window,1,3000,0);while(GetMessage(&msg,0,0,0)>0){TranslateMessage(&msg);DispatchMessage(&msg);}
}
/* v1.8 (--flip): a full-screen 3D flip chain. Each check frame is rendered into the back buffer in its own colours, flipped, and the
 * front buffer read back: the frame must be what is now shown (a driver that kept drawing at the old back-buffer address would draw into
 * the screen and show the previous frame). Then 240 animated frames, timed. */
static void big_triangle(tl_vertex *v,float cx,float cy,float r,float a,DWORD c0,DWORD c1,DWORD c2) {
 int k;memset(v,0,sizeof(tl_vertex)*3);
 for(k=0;k<3;k++){float t=a+k*2.0943951f;v[k].x=cx+r*(float)cos(t);v[k].y=cy+r*(float)sin(t);v[k].z=0.5f;v[k].rhw=1;}
 v[0].color=c0;v[1].color=c1;v[2].color=c2;
}
static DWORD near_rgb(DWORD a,DWORD b) {int k;for(k=0;k<24;k+=8){int d=(int)((a>>k)&255)-(int)((b>>k)&255);if(d<-8||d>8)return 0;}return 1;}
static void flip_test(int bpp) {
 DDSURFACEDESC2 s;DDSCAPS2 caps;LPDIRECTDRAWSURFACE7 front=0,back=0,z=0;LPDIRECT3DDEVICE7 d=0;D3DVIEWPORT7 vp;tl_vertex v[3];HRESULT hr;int k,ok=1;DWORD t0,t1;
 static const DWORD fg[4]={0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL,0xFFFFFF00UL},bg[4]={0xFF202020UL,0xFF000060UL,0xFF600000UL,0xFF006000UL};
 fprintf(logfile,"== flip chain 640x480x%d\n",bpp);fflush(logfile);
 if(!note("   SetCooperativeLevel(exclusive, full screen)",IDirectDraw7_SetCooperativeLevel(dd,window,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))||
    !note("   SetDisplayMode",IDirectDraw7_SetDisplayMode(dd,640,480,bpp,0,0)))goto out;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT;s.dwBackBufferCount=1;
 s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX|DDSCAPS_3DDEVICE;
 if(!note("   CreateSurface(primary + 1 back buffer, 3DDEVICE)",IDirectDraw7_CreateSurface(dd,&s,&front,0)))goto out;
 memset(&caps,0,sizeof(caps));caps.dwCaps=DDSCAPS_BACKBUFFER;
 if(!note("   GetAttachedSurface(back buffer)",IDirectDrawSurface7_GetAttachedSurface(front,&caps,&back)))goto out;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;s.dwWidth=640;s.dwHeight=480;
 s.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|DDSCAPS_VIDEOMEMORY;s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);s.ddpfPixelFormat.dwFlags=DDPF_ZBUFFER;
 s.ddpfPixelFormat.dwZBufferBitDepth=32;s.ddpfPixelFormat.dwZBitMask=0xFFFFFFFFUL;
 if(!note("   CreateSurface(D32 640x480)",IDirectDraw7_CreateSurface(dd,&s,&z,0))||!note("   Attach D32 to the back buffer",IDirectDrawSurface7_AddAttachedSurface(back,z)))goto out;
 if(!note("   CreateDevice(IID_IDirect3DHALDevice, back buffer)",IDirect3D7_CreateDevice(d3d,&IID_IDirect3DHALDevice,back,&d)))goto out;
 memset(&vp,0,sizeof(vp));vp.dwWidth=640;vp.dwHeight=480;vp.dvMaxZ=1;
 note("   SetViewport",IDirect3DDevice7_SetViewport(d,&vp));
 IDirect3DDevice7_SetRenderState(d,D3DRENDERSTATE_ZENABLE,TRUE);IDirect3DDevice7_SetRenderState(d,D3DRENDERSTATE_LIGHTING,FALSE);
 IDirect3DDevice7_SetRenderState(d,D3DRENDERSTATE_CULLMODE,D3DCULL_NONE);IDirect3DDevice7_SetRenderState(d,D3DRENDERSTATE_SHADEMODE,D3DSHADE_FLAT);
 for(k=0;k<4;k++) {
  DWORD in,outp;
  big_triangle(v,320,240,200,-1.5707963f,fg[k],fg[k],fg[k]);
  hr=IDirect3DDevice7_Clear(d,0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,bg[k],1.0f,0);if(FAILED(hr)){note("   Clear",hr);ok=0;break;}
  if(FAILED(hr=IDirect3DDevice7_BeginScene(d))||FAILED(hr=IDirect3DDevice7_DrawPrimitive(d,D3DPT_TRIANGLELIST,D3DFVF_TLVERTEX,v,3,0))||FAILED(hr=IDirect3DDevice7_EndScene(d))){note("   frame",hr);ok=0;break;}
  if(FAILED(hr=IDirectDrawSurface7_Flip(front,0,DDFLIP_WAIT))){note("   Flip",hr);ok=0;break;}
  memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
  if(FAILED(hr=IDirectDrawSurface7_Lock(front,0,&s,DDLOCK_WAIT|DDLOCK_READONLY,0))){note("   Lock(front)",hr);ok=0;break;}
  in=rgb(&s,320,250);outp=rgb(&s,8,8);IDirectDrawSurface7_Unlock(front,0);
  fprintf(logfile,"   frame %d after Flip: front inside %06lX (drawn %06lX), outside %06lX (cleared %06lX)\n",k,(unsigned long)in,(unsigned long)(fg[k]&0xFFFFFF),(unsigned long)outp,(unsigned long)(bg[k]&0xFFFFFF));
  if(!near_rgb(in,fg[k])||!near_rgb(outp,bg[k]))ok=0;
 }
 check("   each flipped frame is the one just rendered",ok);
 IDirect3DDevice7_SetRenderState(d,D3DRENDERSTATE_SHADEMODE,D3DSHADE_GOURAUD);
 {DWORD tc=0,td=0,tf=0,a,b;
 t0=GetTickCount();
 for(k=0;k<240;k++) {
  big_triangle(v,320,240,200,k*0.05f,0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL);
  a=GetTickCount();hr=IDirect3DDevice7_Clear(d,0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,0xFF101828UL,1.0f,0);b=GetTickCount();tc+=b-a;
  if(FAILED(hr)||FAILED(hr=IDirect3DDevice7_BeginScene(d))||FAILED(hr=IDirect3DDevice7_DrawPrimitive(d,D3DPT_TRIANGLELIST,D3DFVF_TLVERTEX,v,3,0))||FAILED(hr=IDirect3DDevice7_EndScene(d))){fprintf(logfile,"   animation frame %d: %08lX\n",k,(unsigned long)hr);failures++;break;}
  a=GetTickCount();td+=a-b;
  hr=IDirectDrawSurface7_Flip(front,0,DDFLIP_WAIT);tf+=GetTickCount()-a;
  if(FAILED(hr)){fprintf(logfile,"   animation flip %d: %08lX\n",k,(unsigned long)hr);failures++;break;}
 }
 t1=GetTickCount();
 fprintf(logfile,"   animation: %d frames in %lu ms (%.1f frames/s, flips wait for vertical blank); Clear %lu ms, draw %lu ms, Flip %lu ms in all\n",k,(unsigned long)(t1-t0),t1>t0?k*1000.0/(t1-t0):0.0,(unsigned long)tc,(unsigned long)td,(unsigned long)tf);}
out:
 if(d)IDirect3DDevice7_Release(d);if(z){if(back)IDirectDrawSurface7_DeleteAttachedSurface(back,0,z);IDirectDrawSurface7_Release(z);}
 if(back)IDirectDrawSurface7_Release(back);if(front)IDirectDrawSurface7_Release(front);
 IDirectDraw7_RestoreDisplayMode(dd);IDirectDraw7_SetCooperativeLevel(dd,window,DDSCL_NORMAL);
 if(!ok)failures++;
 fflush(logfile);
}
/* v2.0 (--texture): textures on the HAL. An 8x8 A8R8G8B8 texture in four quadrants (red, green, blue, white) is mapped onto a 128x128 quad at
 * (32,32) of the 256x192 target and read back: point sampling, texture-coordinate wrap/mirror/clamp beyond 1, bilinear filtering across a
 * quadrant edge, a texture loaded by Blt from a system-memory copy, a 256x256 texture, X8R8G8B8, and texture alpha into the blender. */
static const DWORD quad_colour[4]={0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL,0xFFFFFFFFUL};
static DWORD quad_texel(DWORD w,DWORD h,DWORD x,DWORD y) {return quad_colour[(x>=w/2?1:0)+(y>=h/2?2:0)];}
static HRESULT CALLBACK texfmt(LPDDPIXELFORMAT f,LPVOID unused) {
 (void)unused;fprintf(logfile,"   texture format: flags %08lX bits %lu masks %08lX %08lX %08lX alpha %08lX\n",(unsigned long)f->dwFlags,(unsigned long)f->dwRGBBitCount,
  (unsigned long)f->dwRBitMask,(unsigned long)f->dwGBitMask,(unsigned long)f->dwBBitMask,(unsigned long)f->dwRGBAlphaBitMask);
 return D3DENUMRET_OK;
}
static LPDIRECTDRAWSURFACE7 make_texture(DWORD w,DWORD h,int alpha,DWORD where,DWORD fixed_alpha,const char *what) {
 DDSURFACEDESC2 s;LPDIRECTDRAWSURFACE7 t=0;DWORD x,y;char line[160];
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;s.dwWidth=w;s.dwHeight=h;
 s.ddsCaps.dwCaps=DDSCAPS_TEXTURE|where;s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);s.ddpfPixelFormat.dwFlags=DDPF_RGB|(alpha?DDPF_ALPHAPIXELS:0);
 s.ddpfPixelFormat.dwRGBBitCount=32;s.ddpfPixelFormat.dwRBitMask=0xFF0000UL;s.ddpfPixelFormat.dwGBitMask=0xFF00UL;s.ddpfPixelFormat.dwBBitMask=0xFFUL;
 s.ddpfPixelFormat.dwRGBAlphaBitMask=alpha?0xFF000000UL:0;
 sprintf(line,"   CreateSurface(%s %lux%lu %s, %s)",alpha?"A8R8G8B8":"X8R8G8B8",(unsigned long)w,(unsigned long)h,where==DDSCAPS_SYSTEMMEMORY?"system memory":"video memory",what);
 if(!note(line,IDirectDraw7_CreateSurface(dd,&s,&t,0)))return 0;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
 if(!note("   Lock(texture)",IDirectDrawSurface7_Lock(t,0,&s,DDLOCK_WAIT|DDLOCK_WRITEONLY,0))){IDirectDrawSurface7_Release(t);return 0;}
 if(s.lPitch<(LONG)(w*4)){fprintf(logfile,"   texture pitch %ld below width*4\n",(long)s.lPitch);failures++;}
 for(y=0;y<h;y++)for(x=0;x<w;x++){DWORD c=quad_texel(w,h,x,y);if(fixed_alpha)c=(c&0xFFFFFFUL)|fixed_alpha;((DWORD*)((BYTE*)s.lpSurface+y*s.lPitch))[x]=c;}
 note("   Unlock(texture)",IDirectDrawSurface7_Unlock(t,0));
 return t;
}
static DWORD quad_diffuse=0xFFFFFFFFUL,quad_spec=0;
static int tex_quad(float u0,float u1);
static int read_points(const int *xy,DWORD *out,int n);
/* v2.3 (v10.174) and v2.4 (v10.175): the wrap experiment. The VxD makes the mode's register writes the
 * last ones on each textured draw (VX_IOCTL_3DN_TEXDBG). Each mode draws the WRAP gradient quad (u, v 0..2) and four 16x16 patches whose
 * vertices all carry one (u,v): (1.25,.25), (.25,1.25), (-.25,.25), (1.25,1.25), so interpolation plays no part. Logged as R/G/B/W. */
static char letter(DWORD c){return c==0xFF0000UL?'R':c==0x00FF00UL?'G':c==0x0000FFUL?'B':c==0xFFFFFFUL?'W':'?';}
static int patches(void) {
 static const float uv[4][2]={{1.25f,.25f},{.25f,1.25f},{-.25f,.25f},{1.25f,1.25f}};tl_vertex v[24];int k,j;
 memset(v,0,sizeof(v));
 for(k=0;k<4;k++){float x0=176,y0=(float)(24+k*40),x1=192,y1=y0+16;float px[6],py[6];px[0]=x0;px[1]=x1;px[2]=x0;px[3]=x1;px[4]=x1;px[5]=x0;py[0]=y0;py[1]=y0;py[2]=y1;py[3]=y0;py[4]=y1;py[5]=y1;
  for(j=0;j<6;j++){tl_vertex *q=&v[k*6+j];q->x=px[j];q->y=py[j];q->z=.5F;q->rhw=1;q->color=0xFFFFFFFFUL;q->u=uv[k][0];q->v=uv[k][1];}}
 if(!note("   BeginScene",IDirect3DDevice7_BeginScene(dev)))return 0;
 k=note("   DrawPrimitive(constant-UV patches)",IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLELIST,D3DFVF_TLVERTEX,v,24,0));
 return note("   EndScene",IDirect3DDevice7_EndScene(dev))&&k;
}
static void rs_set(D3DRENDERSTATETYPE k,DWORD v){IDirect3DDevice7_SetRenderState(dev,k,v);}
/* v2.2: a 16-bit texture in the four quadrant colours (full alpha) */
static DWORD pack16(DWORD c,DWORD r,DWORD g,DWORD b,DWORD a) {
 DWORD m[4],v[4],k,out=0;m[0]=a;m[1]=r;m[2]=g;m[3]=b;v[0]=0xFF;v[1]=(c>>16)&255;v[2]=(c>>8)&255;v[3]=c&255;
 for(k=0;k<4;k++)if(m[k]){DWORD sh=0,bits=0,mm=m[k];while(!(mm&1)){mm>>=1;sh++;}while(mm&1){mm>>=1;bits++;}out|=((v[k]>>(8-bits))<<sh)&m[k];}
 return out;
}
static LPDIRECTDRAWSURFACE7 make_texture16(DWORD w,DWORD h,DWORD flags,DWORD r,DWORD g,DWORD b,DWORD a,const char *name) {
 DDSURFACEDESC2 s;LPDIRECTDRAWSURFACE7 t=0;DWORD x,y;char line[96];
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;s.dwWidth=w;s.dwHeight=h;
 s.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);s.ddpfPixelFormat.dwFlags=flags;s.ddpfPixelFormat.dwRGBBitCount=16;
 s.ddpfPixelFormat.dwRBitMask=r;s.ddpfPixelFormat.dwGBitMask=g;s.ddpfPixelFormat.dwBBitMask=b;s.ddpfPixelFormat.dwRGBAlphaBitMask=a;
 sprintf(line,"   CreateSurface(%s %lux%lu video memory)",name,(unsigned long)w,(unsigned long)h);
 if(!note(line,IDirectDraw7_CreateSurface(dd,&s,&t,0)))return 0;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
 if(!note("   Lock(texture)",IDirectDrawSurface7_Lock(t,0,&s,DDLOCK_WAIT|DDLOCK_WRITEONLY,0))){IDirectDrawSurface7_Release(t);return 0;}
 for(y=0;y<h;y++)for(x=0;x<w;x++)((WORD*)((BYTE*)s.lpSurface+y*s.lPitch))[x]=(WORD)pack16(quad_texel(w,h,x,y),r,g,b,a);
 note("   Unlock(texture)",IDirectDrawSurface7_Unlock(t,0));
 return t;
}
static int tex_quad(float u0,float u1) {
 tl_vertex v[6];HRESULT hr;int k;
 memset(v,0,sizeof(v));
 v[0].x=32;v[0].y=32;v[0].u=u0;v[0].v=u0; v[1].x=160;v[1].y=32;v[1].u=u1;v[1].v=u0; v[2].x=32;v[2].y=160;v[2].u=u0;v[2].v=u1;
 v[3]=v[1];v[4].x=160;v[4].y=160;v[4].u=u1;v[4].v=u1;v[5]=v[2];
 for(k=0;k<6;k++){v[k].z=.5F;v[k].rhw=1;v[k].color=quad_diffuse;v[k].spec=quad_spec;}
 if(!note("   Clear",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF000060UL,1,0))||!note("   BeginScene",IDirect3DDevice7_BeginScene(dev)))return 0;
 hr=IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLELIST,D3DFVF_TLVERTEX,v,6,0);
 if(!note("   DrawPrimitive(TLVERTEX, textured quad)",hr)){IDirect3DDevice7_EndScene(dev);return 0;}
 return note("   EndScene",IDirect3DDevice7_EndScene(dev));
}
static int read_points(const int *xy,DWORD *out,int n) {
 DDSURFACEDESC2 s;int k;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
 if(!note("   Lock(readback)",IDirectDrawSurface7_Lock(rt,0,&s,DDLOCK_WAIT|DDLOCK_READONLY,0)))return 0;
 for(k=0;k<n;k++)out[k]=rgb(&s,(DWORD)xy[k*2],(DWORD)xy[k*2+1]);
 IDirectDrawSurface7_Unlock(rt,0);
 fprintf(logfile,"   samples:");for(k=0;k<n;k++)fprintf(logfile," (%d,%d)=%06lX",xy[k*2],xy[k*2+1],(unsigned long)out[k]);fprintf(logfile,"\n");
 return 1;
}
static int expect(const int *xy,const DWORD *want,int n,const char *what) {
 DWORD got[8];int k,ok=1;
 if(!read_points(xy,got,n)){check(what,0);return 0;}
 for(k=0;k<n;k++)if(!near_rgb(got[k],want[k]&0xFFFFFFUL))ok=0;
 check(what,ok);return ok;
}
static int ts(DWORD k,DWORD v) {return SUCCEEDED(IDirect3DDevice7_SetTextureStageState(dev,0,(D3DTEXTURESTAGESTATETYPE)k,v));}
/* v2.25 (v10.199): a non-square texture, 2048 textures, L8, DXT1/3/5 written by Lock and loaded by Blt, mipmaps with LOD bias, anisotropy */
static LPDIRECTDRAWSURFACE7 make_fmt(DWORD w,DWORD h,const DDPIXELFORMAT *pf,DWORD where,DWORD levels,const char *name) {
 DDSURFACEDESC2 s;LPDIRECTDRAWSURFACE7 t=0;char line[128];
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;s.dwWidth=w;s.dwHeight=h;s.ddpfPixelFormat=*pf;
 s.ddsCaps.dwCaps=DDSCAPS_TEXTURE|where;
 if(levels>1){s.dwFlags|=DDSD_MIPMAPCOUNT;s.dwMipMapCount=levels;s.ddsCaps.dwCaps|=DDSCAPS_MIPMAP|DDSCAPS_COMPLEX;}
 sprintf(line,"   CreateSurface(%s %lux%lu, %s)",name,(unsigned long)w,(unsigned long)h,where==DDSCAPS_SYSTEMMEMORY?"system memory":"video memory");
 if(!note(line,IDirectDraw7_CreateSurface(dd,&s,&t,0)))return 0;
 return t;
}
/* fill one surface (a level): ARGB per quadrant through fn, in the surface's own format */
static DWORD lumq[4]={0x40,0x80,0xC0,0xFF};
static int fill_level(LPDIRECTDRAWSURFACE7 t,DWORD solid,int kind) { /* kind 0 ARGB8888, 1 L8, 2 DXT1, 3 DXT3, 4 DXT5, 5 R5G6B5 */
 DDSURFACEDESC2 s;DWORD x,y,w,h;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
 if(!note("   Lock(texture)",IDirectDrawSurface7_Lock(t,0,&s,DDLOCK_WAIT|DDLOCK_WRITEONLY,0)))return 0;
 w=s.dwWidth;h=s.dwHeight;
 if(kind>=2&&kind<=4) {
  DWORD bs=kind==2?8:16,bw=w<4?1:w/4,bh=h<4?1:h/4,bx,by;
  for(by=0;by<bh;by++)for(bx=0;bx<bw;bx++){BYTE *b=(BYTE*)s.lpSurface+(by*bw+bx)*bs;DWORD c=solid?solid:quad_texel(w,h,bx*4,by*4);WORD c565=(WORD)(((c>>8)&0xF800)|((c>>5)&0x7E0)|((c>>3)&31));
   if(bs==16){memset(b,kind==3?0xFF:0,8);if(kind==4){b[0]=0xFF;b[1]=0xFF;}b+=8;}
   b[0]=(BYTE)c565;b[1]=(BYTE)(c565>>8);b[2]=b[3]=0;b[4]=b[5]=b[6]=b[7]=0;}
 } else for(y=0;y<h;y++)for(x=0;x<w;x++){DWORD c=solid?solid:quad_texel(w,h,x,y);BYTE *r=(BYTE*)s.lpSurface+y*s.lPitch;
   if(kind==1)r[x]=(BYTE)lumq[(x>=w/2?1:0)+(y>=h/2?2:0)];
   else if(kind==5)((WORD*)r)[x]=(WORD)(((c>>8)&0xF800)|((c>>5)&0x7E0)|((c>>3)&31));
   else ((DWORD*)r)[x]=c;}
 note("   Unlock(texture)",IDirectDrawSurface7_Unlock(t,0));return 1;
}
static void v199_textures(const int *pts,const DWORD *want) {
 static DDPIXELFORMAT argb,rgb16,l8,dxt[3];LPDIRECTDRAWSURFACE7 t;DWORD got[8];int k;char line[128];
 memset(&argb,0,sizeof(argb));argb.dwSize=sizeof(argb);argb.dwFlags=DDPF_RGB|DDPF_ALPHAPIXELS;argb.dwRGBBitCount=32;argb.dwRBitMask=0xFF0000UL;argb.dwGBitMask=0xFF00UL;argb.dwBBitMask=0xFFUL;argb.dwRGBAlphaBitMask=0xFF000000UL;
 rgb16=argb;rgb16.dwFlags=DDPF_RGB;rgb16.dwRGBBitCount=16;rgb16.dwRBitMask=0xF800;rgb16.dwGBitMask=0x7E0;rgb16.dwBBitMask=0x1F;rgb16.dwRGBAlphaBitMask=0;
 memset(&l8,0,sizeof(l8));l8.dwSize=sizeof(l8);l8.dwFlags=DDPF_LUMINANCE;l8.dwRGBBitCount=8;l8.dwRBitMask=0xFF;
 for(k=0;k<3;k++){memset(&dxt[k],0,sizeof(dxt[k]));dxt[k].dwSize=sizeof(dxt[k]);dxt[k].dwFlags=DDPF_FOURCC;dxt[k].dwFourCC=k==0?0x31545844UL:k==1?0x33545844UL:0x35545844UL;}
 fprintf(logfile,"== v10.199 textures: shapes, formats, mipmaps, anisotropy\n");
 ts(D3DTSS_COLOROP,D3DTOP_SELECTARG1);ts(D3DTSS_COLORARG1,D3DTA_TEXTURE);ts(D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);ts(D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
 ts(D3DTSS_MAGFILTER,D3DTFG_POINT);ts(D3DTSS_MINFILTER,D3DTFN_POINT);ts(D3DTSS_MIPFILTER,D3DTFP_NONE);ts(D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);ts(D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);
 /* shapes */
 {static const DWORD wh[4][2]={{64,8},{8,64},{2048,16},{16,2048}};
  for(k=0;k<4;k++)if((t=make_fmt(wh[k][0],wh[k][1],&argb,DDSCAPS_VIDEOMEMORY,1,"A8R8G8B8"))!=0){
   if(fill_level(t,0,0)&&note("   SetTexture",IDirect3DDevice7_SetTexture(dev,0,t))&&tex_quad(0,1)){sprintf(line,"   %lux%lu texture, four quadrants",(unsigned long)wh[k][0],(unsigned long)wh[k][1]);expect(pts,want,5,line);}
   IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(t);}}
 if((t=make_fmt(2048,2048,&rgb16,DDSCAPS_VIDEOMEMORY,1,"R5G6B5"))!=0){
  if(fill_level(t,0,5)&&note("   SetTexture",IDirect3DDevice7_SetTexture(dev,0,t))&&tex_quad(0,1))expect(pts,want,5,"   2048x2048 R5G6B5 texture, four quadrants");
  IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(t);}
 else fprintf(logfile,"   (2048x2048 R5G6B5 did not fit in video memory: not counted)\n");
 /* L8 */
 if((t=make_fmt(8,8,&l8,DDSCAPS_VIDEOMEMORY,1,"L8"))!=0){static const DWORD wl[5]={0x404040UL,0x808080UL,0xC0C0C0UL,0xFFFFFFUL,0x000060UL};
  if(fill_level(t,0,1)&&note("   SetTexture",IDirect3DDevice7_SetTexture(dev,0,t))&&tex_quad(0,1))expect(pts,wl,5,"   L8 texture: 40, 80, C0, FF grey");
  IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(t);}
 else check("   L8 texture",0);
 /* DXT, by Lock, then DXT1 loaded by Blt from system memory */
 for(k=0;k<3;k++){static const char *nm[3]={"DXT1","DXT3","DXT5"};
  if((t=make_fmt(8,8,&dxt[k],DDSCAPS_VIDEOMEMORY,1,nm[k]))!=0){
   if(fill_level(t,0,2+k)&&note("   SetTexture",IDirect3DDevice7_SetTexture(dev,0,t))&&tex_quad(0,1)){sprintf(line,"   %s texture written by Lock, four quadrants",nm[k]);expect(pts,want,5,line);}
   IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(t);}
  else {sprintf(line,"   %s texture",nm[k]);check(line,0);}}
 {LPDIRECTDRAWSURFACE7 sys=make_fmt(8,8,&dxt[0],DDSCAPS_SYSTEMMEMORY,1,"DXT1"),v=make_fmt(8,8,&dxt[0],DDSCAPS_VIDEOMEMORY,1,"DXT1");
  if(sys&&v&&fill_level(sys,0,2)&&note("   Blt(DXT1 system copy -> video texture)",IDirectDrawSurface7_Blt(v,0,sys,0,DDBLT_WAIT,0))&&
     note("   SetTexture",IDirect3DDevice7_SetTexture(dev,0,v))&&tex_quad(0,1))expect(pts,want,5,"   DXT1 texture loaded by Blt from system memory");
  else check("   DXT1 texture loaded by Blt from system memory",0);
  IDirect3DDevice7_SetTexture(dev,0,0);if(v)IDirectDrawSurface7_Release(v);if(sys)IDirectDrawSurface7_Release(sys);}
 /* mipmaps: 32x32, 6 levels, one colour a level; the quad maps 512 texels onto 128 pixels (4x: level 2) */
 if((t=make_fmt(32,32,&argb,DDSCAPS_VIDEOMEMORY,6,"A8R8G8B8 mip chain"))!=0){
  static const DWORD lc[6]={0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL,0xFFFFFF00UL,0xFF00FFFFUL,0xFFFF00FFUL};LPDIRECTDRAWSURFACE7 l=t,n;int lv,ok=1;static const int pm[]={64,64,128,128};
  IDirectDrawSurface7_AddRef(l);
  for(lv=0;lv<6&&l;lv++){DDSCAPS2 c;ok&=fill_level(l,lc[lv],0);memset(&c,0,sizeof(c));c.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_MIPMAP;
   n=0;if(lv<5&&FAILED(IDirectDrawSurface7_GetAttachedSurface(l,&c,&n))){ok=0;n=0;}IDirectDrawSurface7_Release(l);l=n;}
  if(l)IDirectDrawSurface7_Release(l);
  check("   mip chain created and filled (6 levels)",ok);
  /* v2.80 (v10.310; a DXT1 chain's lower levels are read after level 0 at their row sizes, not at the offsets sent): the same for A8R8G8B8.
   * Levels 3 (4x4, rows 32 bytes: 128) and 4 (2x2: 64) each sit in their own 256-byte block; their unused bytes are painted (level 3's from
   * byte 128 orange, level 4's from 64 white), then levels 4 (cyan) and 5 (magenta) drawn: orange if the sampler reads them after level 3
   * at its row size, their own colours if it takes the offsets sent. */
  {DWORD (WINAPI *sf)(void*,DWORD*,DWORD*)=halmod?(DWORD (WINAPI *)(void*,DWORD*,DWORD*))GetProcAddress(halmod,"VX_DD_Surface"):0;LPDIRECTDRAWSURFACE7 lv2=t,nx;int q;DWORD ao[6];DDSURFACEDESC2 d;
   memset(ao,0,sizeof(ao));IDirectDrawSurface7_AddRef(lv2);
   for(q=0;q<6&&lv2;q++){DWORD pit=0;DDSCAPS2 c2;if(sf)sf(lv2,&ao[q],&pit);fprintf(logfile,"   A8R8G8B8 level %d: offset %08lX, pitch %lu\n",q,(unsigned long)ao[q],(unsigned long)pit);
    memset(&c2,0,sizeof(c2));c2.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_MIPMAP;nx=0;if(q<5&&FAILED(IDirectDrawSurface7_GetAttachedSurface(lv2,&c2,&nx)))nx=0;IDirectDrawSurface7_Release(lv2);lv2=nx;}
   if(lv2)IDirectDrawSurface7_Release(lv2);
   memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
   if(ok&&ao[0]&&ao[3]&&ao[4]&&ao[4]>=ao[3]+256&&note("   Lock(A8R8G8B8 level 0, to paint the unused bytes)",IDirectDrawSurface7_Lock(t,0,&d,DDLOCK_WAIT,0))){DWORD x;
    for(x=ao[3]+128;x<ao[3]+256;x+=4)*(DWORD*)((BYTE*)d.lpSurface+(long)(x-ao[0]))=0xFFFF8000UL;
    for(x=ao[4]+64;x<ao[4]+256;x+=4)*(DWORD*)((BYTE*)d.lpSurface+(long)(x-ao[0]))=0xFFFFFFFFUL;
    IDirectDrawSurface7_Unlock(t,0);
    fprintf(logfile,"   painted %08lX-%08lX orange, %08lX-%08lX white\n",(unsigned long)(ao[3]+128),(unsigned long)(ao[3]+256),(unsigned long)(ao[4]+64),(unsigned long)(ao[4]+256));
    if(note("   SetTexture(mip chain, painted)",IDirect3DDevice7_SetTexture(dev,0,t))){static const int pm2[]={64,64,128,128};
     ts(D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);ts(D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);ts(D3DTSS_MIPFILTER,D3DTFP_POINT);
     if(tex_quad(0,64)&&read_points(pm2,got,2))fprintf(logfile,"   painted A8R8G8B8 chain, level 4 drawn (minified 16x): %06lX, %06lX (own colour 00FFFF)\n",(unsigned long)got[0],(unsigned long)got[1]);
     if(tex_quad(0,128)&&read_points(pm2,got,2))fprintf(logfile,"   painted A8R8G8B8 chain, level 5 drawn (minified 32x): %06lX, %06lX (own colour FF00FF)\n",(unsigned long)got[0],(unsigned long)got[1]);
     ts(D3DTSS_MIPFILTER,D3DTFP_NONE);ts(D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);ts(D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);IDirect3DDevice7_SetTexture(dev,0,0);}}}
  if(ok&&note("   SetTexture(mip chain)",IDirect3DDevice7_SetTexture(dev,0,t))){
   ts(D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);ts(D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);
   if(tex_quad(0,16)&&read_points(pm,got,2))check("   MIPFILTER NONE, minified 4x: level 0 (red)",near_rgb(got[0],0xFF0000UL)&&near_rgb(got[1],0xFF0000UL));
   ts(D3DTSS_MIPFILTER,D3DTFP_POINT);
   if(tex_quad(0,16)&&read_points(pm,got,2))check("   MIPFILTER POINT, minified 4x: level 2 (blue)",near_rgb(got[0],0x0000FFUL)&&near_rgb(got[1],0x0000FFUL));
   if(tex_quad(0,4)&&read_points(pm,got,2))check("   MIPFILTER POINT, 1:1: level 0 (red)",near_rgb(got[0],0xFF0000UL)&&near_rgb(got[1],0xFF0000UL));
   {float b=1.0f;ts(D3DTSS_MIPMAPLODBIAS,*(DWORD*)&b);}
   if(tex_quad(0,16)&&read_points(pm,got,2))check("   MIPFILTER POINT, LOD bias +1, minified 4x: level 3 (yellow)",near_rgb(got[0],0xFFFF00UL)&&near_rgb(got[1],0xFFFF00UL));
   ts(D3DTSS_MIPMAPLODBIAS,0);ts(D3DTSS_MIPFILTER,D3DTFP_LINEAR);
   if(tex_quad(0,16)&&read_points(pm,got,2)){sprintf(line,"   MIPFILTER LINEAR, minified 4x: level 2 or a blend next to it (got %06lX)",(unsigned long)got[0]);check(line,(got[0]&0xFF)>=0x60);}
   ts(D3DTSS_MIPFILTER,D3DTFP_NONE);ts(D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);ts(D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);}
  IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(t);}
 else check("   mip chain created",0);
 /* v2.77 (v10.308; board, v10.307: VXGL's DXT1 chain drew its 8x8 level wrong, the one level drawn whose block rows are spread to 32 bytes):
  * a DXT1 chain 32x32 to 1x1, one colour a level, drawn at levels 0 to 3 (32x32 and 16x16 rows are 64 and 32 bytes; 8x8 16, spread) */
 if((t=make_fmt(32,32,&dxt[0],DDSCAPS_VIDEOMEMORY,6,"DXT1 mip chain"))!=0){
  static const DWORD lc[6]={0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL,0xFFFFFF00UL,0xFF00FFFFUL,0xFFFF00FFUL};LPDIRECTDRAWSURFACE7 l=t,n;int lv,ok=1;static DWORD loff[6];static const int pm[]={64,64,128,128};
  static const struct {float rep;DWORD want;const char *name;} dl[4]={{4,0xFF0000UL,"1:1: level 0 (red)"},{8,0x00FF00UL,"minified 2x: level 1 (green, 16x16)"},
   {16,0x0000FFUL,"minified 4x: level 2 (blue, 8x8, rows spread to 32 bytes)"},{32,0xFFFF00UL,"minified 8x: level 3 (yellow, 4x4)"}};
  IDirectDrawSurface7_AddRef(l);
  for(lv=0;lv<6&&l;lv++){DDSCAPS2 c;ok&=fill_level(l,lc[lv],2);memset(&c,0,sizeof(c));c.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_MIPMAP;
   n=0;if(lv<5&&FAILED(IDirectDrawSurface7_GetAttachedSurface(l,&c,&n))){ok=0;n=0;}IDirectDrawSurface7_Release(l);l=n;}
  if(l)IDirectDrawSurface7_Release(l);
  check("   DXT1 mip chain created and filled (6 levels)",ok);
  /* v2.78 (v10.309; board, v10.308: levels 0 and 1 drew, 2 (8x8) and 3 (4x4, one block, nothing spread) black): each level's video-memory
   * offset and the HAL's pitch (VX_DD_Surface), the offset's low byte (the sampler takes offsets in 256-byte units), and the first 8 bytes
   * a Lock gives back */
  memset(loff,0,sizeof(loff));
  {DWORD (WINAPI *sf)(void*,DWORD*,DWORD*)=halmod?(DWORD (WINAPI *)(void*,DWORD*,DWORD*))GetProcAddress(halmod,"VX_DD_Surface"):0;LPDIRECTDRAWSURFACE7 lv2=t,nx;int q;
   IDirectDrawSurface7_AddRef(lv2);
   for(q=0;q<6&&lv2;q++){DWORD off=0,pit=0;DDSURFACEDESC2 d;DDSCAPS2 c2;BYTE b[8];int z;
    memset(b,0,8);if(sf)sf(lv2,&off,&pit);loff[q]=off;memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
    if(SUCCEEDED(IDirectDrawSurface7_Lock(lv2,0,&d,DDLOCK_WAIT|DDLOCK_READONLY,0))){memcpy(b,d.lpSurface,8);IDirectDrawSurface7_Unlock(lv2,0);}
    fprintf(logfile,"   DXT1 level %d: %lux%lu, offset %08lX (low byte %02lX), pitch %lu, lPitch %ld, flags %08lX, bytes",q,(unsigned long)d.dwWidth,(unsigned long)d.dwHeight,(unsigned long)off,(unsigned long)(off&255),(unsigned long)pit,(long)d.lPitch,(unsigned long)d.ddpfPixelFormat.dwFlags);
    for(z=0;z<8;z++)fprintf(logfile," %02X",b[z]);fprintf(logfile,"\n");
    memset(&c2,0,sizeof(c2));c2.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_MIPMAP;nx=0;if(q<5&&FAILED(IDirectDrawSurface7_GetAttachedSurface(lv2,&c2,&nx)))nx=0;IDirectDrawSurface7_Release(lv2);lv2=nx;}
   if(lv2)IDirectDrawSurface7_Release(lv2);}
  if(ok&&note("   SetTexture(DXT1 mip chain)",IDirect3DDevice7_SetTexture(dev,0,t))){int q;
   ts(D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);ts(D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);ts(D3DTSS_MIPFILTER,D3DTFP_POINT);
   for(q=0;q<4;q++)if(tex_quad(0,dl[q].rep)&&read_points(pm,got,2)){sprintf(line,"   DXT1 MIPFILTER POINT, %s (got %06lX)",dl[q].name,(unsigned long)got[0]);check(line,near_rgb(got[0],dl[q].want)&&near_rgb(got[1],dl[q].want));}
   /* v2.79 (v10.310; board, v10.309: the levels at +0, +200, +300, +400, +500, +600 each in its own 256-byte block): painting the bytes the
    * levels leave unused showed the sampler reading level 2 at +280 and level 3 at +2C0, after level 1 at the 32-byte row size. v2.80
    * (v10.310: the top level's block holds every level where the sampler reads them; the lower levels' own blocks are the CPU's copies):
    * the copies' unused bytes are painted, each inside its own 256-byte block: level 1's from byte 128 (magenta 128-191, dark green
    * 192-255), level 2's from 64 (white), level 3's from 8 (orange). Levels 2 and 3 drawn again: their own colours if the sampler reads
    * the top level's block from level 0 on; magenta and dark green if it takes level 1 from the offset sent and the rest after it. */
   if(loff[0]&&loff[1]&&loff[2]&&loff[3]){DDSURFACEDESC2 d;
    static const struct {int lv;DWORD a,b;WORD c;const char *what;} rg[]={{1,128,192,0xF81F,"magenta: level 1's block, bytes 128-191"},{1,192,256,0x0400,"dark green: level 1's block, bytes 192-255"},
     {2,64,256,0xFFFF,"white: level 2's block from byte 64"},{3,8,256,0xFC00,"orange: level 3's block from byte 8"}};
    memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
    if(note("   Lock(DXT1 level 0, to paint the unused bytes)",IDirectDrawSurface7_Lock(t,0,&d,DDLOCK_WAIT,0))){int r;
     for(r=0;r<(int)(sizeof(rg)/sizeof(rg[0]));r++){DWORD a=loff[rg[r].lv]+rg[r].a,e=loff[rg[r].lv]+rg[r].b,x;
      for(x=a;x+8<=e;x+=8){BYTE *m=(BYTE*)d.lpSurface+(long)(x-loff[0]);m[0]=m[2]=(BYTE)rg[r].c;m[1]=m[3]=(BYTE)(rg[r].c>>8);m[4]=m[5]=m[6]=m[7]=0;}
      fprintf(logfile,"   painted %08lX-%08lX %s\n",(unsigned long)a,(unsigned long)e,rg[r].what);}
     IDirectDrawSurface7_Unlock(t,0);
     ts(D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);ts(D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);ts(D3DTSS_MIPFILTER,D3DTFP_POINT);
     for(q=2;q<4;q++)if(tex_quad(0,dl[q].rep)&&read_points(pm,got,2))fprintf(logfile,"   painted chain, level %d drawn: %06lX at (64,64), %06lX at (128,128) (own colour %06lX)\n",q,(unsigned long)got[0],(unsigned long)got[1],(unsigned long)dl[q].want);
     ts(D3DTSS_MIPFILTER,D3DTFP_NONE);ts(D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);ts(D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);}}
   ts(D3DTSS_MIPFILTER,D3DTFP_NONE);ts(D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);ts(D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);}
  IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(t);}
 else check("   DXT1 mip chain created",0);
 /* v2.26 (v10.200): the sampler's row stride, measured. An L8 texture w x 8 is filled, past its own rows, with bytes that encode their
  * offset (offset / 4) over its 256-byte-rounded block; texel (0, r) then reads the byte at r x stride. Then textures with rows under 32
  * bytes in each format. */
 {static const DWORD pw[5]={4,8,16,32,64};int q;
  for(q=0;q<5;q++)if((t=make_fmt(pw[q],8,&l8,DDSCAPS_VIDEOMEMORY,1,"L8 stride probe"))!=0){DDSURFACEDESC2 d;
   memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
   if(note("   Lock(texture)",IDirectDrawSurface7_Lock(t,0,&d,DDLOCK_WAIT|DDLOCK_WRITEONLY,0))){DWORD o,span=d.lPitch*8;if(span<256)span=256;
    for(o=0;o<span;o++)((BYTE*)d.lpSurface)[o]=(BYTE)(o/4);note("   Unlock(texture)",IDirectDrawSurface7_Unlock(t,0));
    if(note("   SetTexture",IDirect3DDevice7_SetTexture(dev,0,t))&&tex_quad(0,1)){int xy[6];DWORD g[3];
     xy[0]=32+(int)(64/pw[q]);xy[1]=32+8+16;xy[2]=xy[0];xy[3]=32+8+32;xy[4]=xy[0];xy[5]=32+8+16*7;
     if(read_points(xy,g,3))fprintf(logfile,"   L8 %lux8 (lPitch %ld): rows 1, 2, 7 read from offsets %lu, %lu, %lu: stride %lu (rows of %lu bytes; 32-byte minimum predicts %lu)\n",
      (unsigned long)pw[q],(long)d.lPitch,(unsigned long)((g[0]>>16)&255)*4,(unsigned long)((g[1]>>16)&255)*4,(unsigned long)((g[2]>>16)&255)*4,
      (unsigned long)((g[0]>>16)&255)*4,(unsigned long)pw[q],(unsigned long)(pw[q]<32?32:pw[q]));}}
   IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(t);}}
 {static const struct {DWORD w,h;int kind;const char *name;} sm[]={{8,8,1,"L8 8x8"},{4,4,1,"L8 4x4"},{8,8,5,"R5G6B5 8x8"},{4,4,0,"A8R8G8B8 4x4"},{2,2,0,"A8R8G8B8 2x2"},
   {8,8,2,"DXT1 8x8"},{16,16,2,"DXT1 16x16"},{32,32,2,"DXT1 32x32"},{4,4,3,"DXT3 4x4"},{8,8,3,"DXT3 8x8"},{4,4,4,"DXT5 4x4"}};
  static const DWORD wl[5]={0x404040UL,0x808080UL,0xC0C0C0UL,0xFFFFFFUL,0x000060UL};int q;
  for(q=0;q<(int)(sizeof(sm)/sizeof(sm[0]));q++){const DDPIXELFORMAT *f=sm[q].kind==1?&l8:sm[q].kind==5?&rgb16:sm[q].kind>=2&&sm[q].kind<=4?&dxt[sm[q].kind-2]:&argb;
   const DWORD *wn=sm[q].kind==1?wl:want;DWORD w4[5];
   if(sm[q].w<4||(sm[q].kind>=2&&sm[q].kind<=4&&sm[q].w==4)){w4[0]=w4[1]=w4[2]=w4[3]=sm[q].kind>=2&&sm[q].kind<=4?want[0]:0;w4[4]=0x000060UL;}
   if((t=make_fmt(sm[q].w,sm[q].h,f,DDSCAPS_VIDEOMEMORY,1,sm[q].name))!=0){
    if(fill_level(t,(sm[q].kind>=2&&sm[q].kind<=4&&sm[q].w==4)?0xFFFF0000UL:0,sm[q].kind)&&note("   SetTexture",IDirect3DDevice7_SetTexture(dev,0,t))&&tex_quad(0,1)){
     sprintf(line,"   %s texture (rows under 32 bytes)%s",sm[q].name,(sm[q].kind>=2&&sm[q].kind<=4&&sm[q].w==4)?", one red block":"");
     if(sm[q].kind>=2&&sm[q].kind<=4&&sm[q].w==4)expect(pts,w4,5,line);
     else if(sm[q].w<4){static const int p2x2[]={64,64,128,64,64,128,128,128,200,170};expect(p2x2,wn,5,line);}
     else expect(pts,wn,5,line);}
    IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(t);}
   else {sprintf(line,"   %s texture",sm[q].name);check(line,0);}}}
 /* anisotropy at 1:1: the quadrant colours (filtering changes only the edges) */
 if((t=make_fmt(8,8,&argb,DDSCAPS_VIDEOMEMORY,1,"A8R8G8B8"))!=0){
  if(fill_level(t,0,0)&&note("   SetTexture",IDirect3DDevice7_SetTexture(dev,0,t))&&ts(D3DTSS_MINFILTER,D3DTFN_ANISOTROPIC)&&ts(D3DTSS_MAXANISOTROPY,4)&&tex_quad(0,1))
   expect(pts,want,5,"   MINFILTER ANISOTROPIC, MAXANISOTROPY 4, four quadrants");
  ts(D3DTSS_MINFILTER,D3DTFN_POINT);ts(D3DTSS_MAXANISOTROPY,1);IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(t);}
 fflush(logfile);
}
/* v2.32 (v10.206): two and three texture stages and the v2.6 ops (CLXP3D T116-T124, T138-T145, T149), on solid 4x4 A8R8G8B8 textures
 * (and the quadrant texture for the coordinate-set check). The quad's second coordinate set is (0.75, 0.25) everywhere. Read at (64,64). */
typedef struct tl2_vertex {float x,y,z,rhw;DWORD color,spec;float u,v,u1,v1;} tl2_vertex;
static LPDIRECTDRAWSURFACE7 solid_tex(DWORD argb) {
 DDSURFACEDESC2 s;LPDIRECTDRAWSURFACE7 t=0;DWORD x,y;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;s.dwWidth=4;s.dwHeight=4;
 s.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);s.ddpfPixelFormat.dwFlags=DDPF_RGB|DDPF_ALPHAPIXELS;
 s.ddpfPixelFormat.dwRGBBitCount=32;s.ddpfPixelFormat.dwRBitMask=0xFF0000UL;s.ddpfPixelFormat.dwGBitMask=0xFF00UL;s.ddpfPixelFormat.dwBBitMask=0xFFUL;s.ddpfPixelFormat.dwRGBAlphaBitMask=0xFF000000UL;
 if(FAILED(IDirectDraw7_CreateSurface(dd,&s,&t,0)))return 0;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
 if(FAILED(IDirectDrawSurface7_Lock(t,0,&s,DDLOCK_WAIT|DDLOCK_WRITEONLY,0))){IDirectDrawSurface7_Release(t);return 0;}
 for(y=0;y<4;y++)for(x=0;x<4;x++)((DWORD*)((BYTE*)s.lpSurface+y*s.lPitch))[x]=argb;
 IDirectDrawSurface7_Unlock(t,0);return t;
}
static int ts2(DWORD st,DWORD k,DWORD v) {return SUCCEEDED(IDirect3DDevice7_SetTextureStageState(dev,st,(D3DTEXTURESTAGESTATETYPE)k,v));}
/* v2.34: the second set is (q2u, q2v) everywhere, or the first set again (q2same); specular q2spec */
static float q2u=.75F,q2v=.25F;static int q2same;static DWORD q2spec;
static int quad2(DWORD diffuse) {
 tl2_vertex v[6];HRESULT hr;int k;
 memset(v,0,sizeof(v));
 v[0].x=32;v[0].y=32;v[0].u=0;v[0].v=0; v[1].x=160;v[1].y=32;v[1].u=1;v[1].v=0; v[2].x=32;v[2].y=160;v[2].u=0;v[2].v=1;
 v[3]=v[1];v[4].x=160;v[4].y=160;v[4].u=1;v[4].v=1;v[5]=v[2];
 for(k=0;k<6;k++){v[k].z=.5F;v[k].rhw=1;v[k].color=diffuse;v[k].spec=q2spec;v[k].u1=q2same?v[k].u:q2u;v[k].v1=q2same?v[k].v:q2v;}
 if(!note("   Clear",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF000060UL,1,0))||!note("   BeginScene",IDirect3DDevice7_BeginScene(dev)))return 0;
 hr=IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLELIST,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX2,v,6,0);
 if(!note("   DrawPrimitive(two coordinate sets)",hr)){IDirect3DDevice7_EndScene(dev);return 0;}
 return note("   EndScene",IDirect3DDevice7_EndScene(dev));
}
static void stage_tests(LPDIRECTDRAWSURFACE7 quads) {
 #define T_ D3DTA_TEXTURE
 #define D_ D3DTA_DIFFUSE
 #define C_ D3DTA_CURRENT
 static const struct {DWORD t[3];DWORD op[3][6];DWORD tci[3],diffuse,blend,want;const char *name;} sc[]={
  {{0xFFC08040UL,0xFF808080UL,0},{{D3DTOP_MODULATE,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,0,0},0xFFFFFFFFUL,0,0x604020UL,"light map: C08040 x diffuse, x texture 1 808080 = 604020 (T116)"},
  {{0xFF400000UL,0xFF002040UL,0},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_ADD,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,0,0},0xFFFFFFFFUL,0,0x402040UL,"400000 then ADD texture 1 002040 = 402040 (T117)"},
  {{0xFF406080UL,0xFF808080UL,0},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE2X,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,0,0},0xFFFFFFFFUL,0,0x406080UL,"406080 then MODULATE2X texture 1 808080 = 406080 (T118)"},
  {{0xFFFF0000UL,0xFF0000FFUL,0},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,D_,T_},{D3DTOP_BLENDCURRENTALPHA,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,0,0},0x80FFFFFFUL,0,0x7F0080UL,"red, alpha = diffuse 80, then BLENDCURRENTALPHA texture 1 blue = 7F0080 (T119)"},
  {{0xFFFF0000UL,0x80FFFFFFUL,0},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_SELECTARG2,T_,C_,D3DTOP_MODULATE,T_,C_}},{0,0,0},0xFFFFFFFFUL,1,0x800030UL,"red, then alpha MODULATE texture 1 (alpha 80), blended = 800030 (T120)"},
  {{0xFFFF8040UL,0,0},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,C_,D_,D3DTOP_SELECTARG1,C_,D_}},{0,0,0},0xFF808080UL,0,0x804020UL,"FF8040 then MODULATE CURRENT x diffuse 808080 (no texture 1) = 804020 (T123)"},
  {{0xFFC0C0C0UL,0xFF404040UL,0},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_SUBTRACT,C_,T_,D3DTOP_SELECTARG2,T_,C_}},{0,0,0},0xFFFFFFFFUL,0,0x808080UL,"C0C0C0 then CURRENT SUBTRACT texture 1 404040 = 808080 (T124)"},
  {{0xFFFFFFFFUL,1,0},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,1,0},0xFFFFFFFFUL,0,0x00FF00UL,"texture 1 on coordinate set 1 (0.75, 0.25): green quadrant (T116)"},
  {{0xFFFFFFFFUL,1,0},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,0,0},0xFFFFFFFFUL,0,0xFF0000UL,"texture 1 on coordinate set 0 (0.25, 0.25): red quadrant (T121)"},
  {{0xFF808080UL,0xFF808080UL,0xFF204060UL},{{D3DTOP_MODULATE,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_},{D3DTOP_ADD,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,0,0},0xFFFF8040UL,0,0x606070UL,"three stages: 808080 x FF8040, x 808080, + 204060 = 606070 (T149)"},
  /* v2.77 (v10.308; board, v10.307: VXGL's three units drew black with unit 1 on coordinate set 1): T149 with its own sets (0, 1, 0), and
   * (0, 1, 1); then the program VXGL makes (stage 0 SELECTARG1, stages 1 and 2 MODULATE CURRENT) on sets (0, 0, 0) and (0, 1, 0); then
   * which set stage 2 reads (the quadrant texture: set 0 (0.25, 0.25) red, set 1 (0.75, 0.25) green) */
  {{0xFF808080UL,0xFF808080UL,0xFF204060UL},{{D3DTOP_MODULATE,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_},{D3DTOP_ADD,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,1,0},0xFFFF8040UL,0,0x606070UL,"three stages on sets 0, 1, 0 (T149's own) = 606070"},
  {{0xFF808080UL,0xFF808080UL,0xFF204060UL},{{D3DTOP_MODULATE,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_},{D3DTOP_ADD,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,1,1},0xFFFF8040UL,0,0x606070UL,"three stages on sets 0, 1, 1 = 606070"},
  {{0xFF808080UL,0xFF808080UL,0xFF808080UL},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_},{D3DTOP_MODULATE,T_,C_,D3DTOP_MODULATE,T_,C_}},{0,0,0},0xFFFFFFFFUL,0,0x202020UL,"VXGL's three-unit program on sets 0, 0, 0: 808080 x 808080 x 808080 = 202020"},
  {{0xFF808080UL,0xFF808080UL,0xFF808080UL},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_},{D3DTOP_MODULATE,T_,C_,D3DTOP_MODULATE,T_,C_}},{0,1,0},0xFFFFFFFFUL,0,0x202020UL,"VXGL's three-unit program on sets 0, 1, 0 = 202020"},
  {{0xFFFFFFFFUL,0xFFFFFFFFUL,1},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,1,0},0xFFFFFFFFUL,0,0xFF0000UL,"stage 2 on set 0 with stage 1 on set 1: red quadrant"},
  {{0xFFFFFFFFUL,0xFFFFFFFFUL,1},{{D3DTOP_SELECTARG1,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,1,1},0xFFFFFFFFUL,0,0x00FF00UL,"stage 2 on set 1 with stage 1 on set 1: green quadrant"},
  {{0xFFFF8080UL,0,0},{{D3DTOP_DOTPRODUCT3,T_,D_,D3DTOP_SELECTARG1,T_,D_}},{0,0,0},0xFFFF8080UL,0,0xFFFFFFUL,"DOTPRODUCT3 (1,0,0).(1,0,0) = FFFFFF (T142)"},
  {{0xFFFF8080UL,0,0},{{D3DTOP_DOTPRODUCT3,T_,D_,D3DTOP_SELECTARG1,T_,D_}},{0,0,0},0xFF80FF80UL,0,0x000000UL,"DOTPRODUCT3 (1,0,0).(0,1,0) = 000000"},
  {{0xFFFF8080UL,0xFF808080UL,0},{{D3DTOP_DOTPRODUCT3,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_MODULATE,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,0,0},0xFFFF8080UL,0,0x808080UL,"DOTPRODUCT3, then MODULATE texture 1 808080 = 808080 (T145)"},
  {{0x80400000UL,0,0},{{D3DTOP_BLENDTEXTUREALPHAPM,T_,D_,D3DTOP_SELECTARG1,T_,D_}},{0,0,0},0xFF0000FFUL,0,0x40007FUL,"BLENDTEXTUREALPHAPM 400000 (alpha 80) + blue x (1 - 0.5) = 40007F (T138)"},
  {{0x80400000UL,0,0},{{D3DTOP_MODULATEINVALPHA_ADDCOLOR,T_,D_,D3DTOP_SELECTARG1,T_,D_}},{0,0,0},0xFF0000FFUL,0,0x40007FUL,"MODULATEINVALPHA_ADDCOLOR 400000 (alpha 80), blue = 40007F (T139)"},
  {{0x80C00000UL,0,0},{{D3DTOP_MODULATEINVCOLOR_ADDALPHA,T_,D_,D3DTOP_SELECTARG1,T_,D_}},{0,0,0},0xFF404040UL,0,0x8FBFBFUL,"MODULATEINVCOLOR_ADDALPHA C00000 (alpha 80), 404040 = 8FBFBF (T140)"},
  {{0xFFFF8000UL,0xFF808080UL,0},{{D3DTOP_PREMODULATE,T_,D_,D3DTOP_SELECTARG1,T_,D_},{D3DTOP_SELECTARG2,T_,C_,D3DTOP_SELECTARG2,T_,C_}},{0,0,0},0xFFFFFFFFUL,0,0x804000UL,"PREMODULATE FF8000 with texture 1 808080 = 804000 (T144)"}};
 static const int p5[]={64,64};
 D3DDEVICEDESC7 caps;int k,st;DWORD got[1];char line[160];
 memset(&caps,0,sizeof(caps));IDirect3DDevice7_GetCaps(dev,&caps);
 fprintf(logfile,"   stages: %u blend stages, %u textures, op caps %08lX, FVF caps %08lX\n",(unsigned)caps.wMaxTextureBlendStages,(unsigned)caps.wMaxSimultaneousTextures,(unsigned long)caps.dwTextureOpCaps,(unsigned long)caps.dwFVFCaps);
 if(caps.wMaxSimultaneousTextures<3){check("   three textures offered",0);return;}
 for(k=0;k<(int)(sizeof(sc)/sizeof(sc[0]));k++){LPDIRECTDRAWSURFACE7 tx[3]={0,0,0};DWORD passes=0;HRESULT hr;
  sprintf(line,"   %s",sc[k].name);
  for(st=0;st<3;st++){
   if(sc[k].t[st]==1)tx[st]=quads;else if(sc[k].t[st])tx[st]=solid_tex(sc[k].t[st]);
   IDirect3DDevice7_SetTexture(dev,st,tx[st]);
   ts2(st,D3DTSS_COLOROP,sc[k].op[st][0]?sc[k].op[st][0]:D3DTOP_DISABLE);ts2(st,D3DTSS_COLORARG1,sc[k].op[st][1]);ts2(st,D3DTSS_COLORARG2,sc[k].op[st][2]);
   ts2(st,D3DTSS_ALPHAOP,sc[k].op[st][0]?sc[k].op[st][3]:D3DTOP_DISABLE);ts2(st,D3DTSS_ALPHAARG1,sc[k].op[st][4]);ts2(st,D3DTSS_ALPHAARG2,sc[k].op[st][5]);
   ts2(st,D3DTSS_TEXCOORDINDEX,sc[k].tci[st]);ts2(st,D3DTSS_MAGFILTER,D3DTFG_POINT);ts2(st,D3DTSS_MINFILTER,D3DTFN_POINT);
   ts2(st,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);ts2(st,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);}
  ts2(3,D3DTSS_COLOROP,D3DTOP_DISABLE);ts2(3,D3DTSS_ALPHAOP,D3DTOP_DISABLE);
  hr=IDirect3DDevice7_ValidateDevice(dev,&passes);fprintf(logfile,"   ValidateDevice = %08lX\n",(unsigned long)hr);
  if(sc[k].blend){rs_set(D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);rs_set(D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA);rs_set(D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);}
  if(quad2(sc[k].diffuse)&&read_points(p5,got,1))check(line,near_rgb(got[0],sc[k].want));else check(line,0);
  if(sc[k].tci[1]==1) { /* v2.33 (v10.207): what ring 0 sent for this draw (TEXDBG 0x54), then the same draw with the two sets swapped (0x51) */
   DWORD in=0x54,o[8],gt=0;
   if(DeviceIoControl(endpoint,0x85500022UL,&in,4,o,32,&gt,0)&&gt==32&&(o[0]&0xF0)==0x50)
    fprintf(logfile,"   ring 0: FE/01 %06lX, units %lu, unit 1 set %lu, vertex 0 uv0 %08lX %08lX uv1 %08lX %08lX\n",(unsigned long)o[1],(unsigned long)o[2],(unsigned long)o[3],
     (unsigned long)o[4],(unsigned long)o[5],(unsigned long)o[6],(unsigned long)o[7]);
   else fprintf(logfile,"   ring 0 readback: the VxD does not take it\n");
  }
  rs_set(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);
  for(st=0;st<3;st++){IDirect3DDevice7_SetTexture(dev,st,0);if(tx[st]&&tx[st]!=quads)IDirectDrawSurface7_Release(tx[st]);
   if(st){ts2(st,D3DTSS_COLOROP,D3DTOP_DISABLE);ts2(st,D3DTSS_ALPHAOP,D3DTOP_DISABLE);}ts2(st,D3DTSS_TEXCOORDINDEX,st);}}
 ts2(0,D3DTSS_COLOROP,D3DTOP_SELECTARG1);ts2(0,D3DTSS_COLORARG1,D3DTA_TEXTURE);ts2(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);ts2(0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE);
 IDirect3DDevice7_SetTexture(dev,0,quads);
 /* v2.33: one stage on coordinate set 1 (the core picks the set; one unit) */
 ts2(0,D3DTSS_TEXCOORDINDEX,1);
 if(quad2(0xFFFFFFFFUL)&&read_points(p5,got,1))check("   one stage, TEXCOORDINDEX 1 (0.75, 0.25): green quadrant",near_rgb(got[0],0x00FF00UL));else check("   one stage, TEXCOORDINDEX 1",0);
 ts2(0,D3DTSS_TEXCOORDINDEX,0);
 #undef T_
 #undef D_
 #undef C_
}
/* v2.34 (v10.208): coordinate-set experiment, colour key with specular / two stages (T73, T125), bump mapping (T146-T148), projected
 * texture (T151). */
static void set_stage(DWORD st,DWORD op,DWORD a1,DWORD a2,DWORD aop,DWORD b1,DWORD b2,DWORD tci) {
 ts2(st,D3DTSS_COLOROP,op);ts2(st,D3DTSS_COLORARG1,a1);ts2(st,D3DTSS_COLORARG2,a2);ts2(st,D3DTSS_ALPHAOP,aop);ts2(st,D3DTSS_ALPHAARG1,b1);ts2(st,D3DTSS_ALPHAARG2,b2);
 ts2(st,D3DTSS_TEXCOORDINDEX,tci);ts2(st,D3DTSS_MAGFILTER,D3DTFG_POINT);ts2(st,D3DTSS_MINFILTER,D3DTFN_POINT);ts2(st,D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP);ts2(st,D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP);
}
static void stages_off(void) {DWORD st;for(st=0;st<3;st++){IDirect3DDevice7_SetTexture(dev,st,0);if(st){ts2(st,D3DTSS_COLOROP,D3DTOP_DISABLE);ts2(st,D3DTSS_ALPHAOP,D3DTOP_DISABLE);}ts2(st,D3DTSS_TEXCOORDINDEX,st);ts2(st,D3DTSS_TEXTURETRANSFORMFLAGS,0);}
 set_stage(0,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,0);q2u=.75F;q2v=.25F;q2same=0;q2spec=0;
 rs_set(D3DRENDERSTATE_COLORKEYENABLE,FALSE);rs_set(D3DRENDERSTATE_SPECULARENABLE,FALSE);}
static LPDIRECTDRAWSURFACE7 halves_tex(DWORD flags,DWORD bits,DWORD m0,DWORD m1,DWORD m2,DWORD left,DWORD right,const char *name) {
 DDSURFACEDESC2 d;LPDIRECTDRAWSURFACE7 t=0;DWORD x,y;char line[96];
 memset(&d,0,sizeof(d));d.dwSize=sizeof(d);d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;d.dwWidth=8;d.dwHeight=8;d.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;
 d.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);d.ddpfPixelFormat.dwFlags=flags;d.ddpfPixelFormat.dwRGBBitCount=bits;d.ddpfPixelFormat.dwRBitMask=m0;d.ddpfPixelFormat.dwGBitMask=m1;d.ddpfPixelFormat.dwBBitMask=m2;
 d.ddpfPixelFormat.dwRGBAlphaBitMask=(flags&DDPF_ALPHAPIXELS)?0xFF000000UL:0;
 sprintf(line,"   CreateSurface(%s 8x8)",name);
 if(!note(line,IDirectDraw7_CreateSurface(dd,&d,&t,0)))return 0;
 memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
 if(!note("   Lock(texture)",IDirectDrawSurface7_Lock(t,0,&d,DDLOCK_WAIT|DDLOCK_WRITEONLY,0))){IDirectDrawSurface7_Release(t);return 0;}
 for(y=0;y<8;y++)for(x=0;x<8;x++){DWORD c=x<4?left:right;if(bits==16)((WORD*)((BYTE*)d.lpSurface+y*d.lPitch))[x]=(WORD)c;else ((DWORD*)((BYTE*)d.lpSurface+y*d.lPitch))[x]=c;}
 IDirectDrawSurface7_Unlock(t,0);return t;
}
static int q3_draw(void) { /* (u, v, q): q 1 at the left, 2 at the right, u 0 -> 2: divided, the quad shows the four quadrants once */
 typedef struct {float x,y,z,rhw;DWORD c,s;float u,v,q;} v3;v3 v[6];HRESULT hr;int k;
 static const float xy[6][5]={{32,32,0,0,1},{160,32,2,0,2},{32,160,0,1,1},{160,32,2,0,2},{160,160,2,2,2},{32,160,0,1,1}};
 for(k=0;k<6;k++){v[k].x=xy[k][0];v[k].y=xy[k][1];v[k].z=.5F;v[k].rhw=1;v[k].c=0xFFFFFFFFUL;v[k].s=0;v[k].u=xy[k][2];v[k].v=xy[k][3];v[k].q=xy[k][4];}
 if(!note("   Clear",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF000060UL,1,0))||!note("   BeginScene",IDirect3DDevice7_BeginScene(dev)))return 0;
 hr=IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLELIST,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX1|D3DFVF_TEXCOORDSIZE3(0),v,6,0);
 if(!note("   DrawPrimitive(s, t, q)",hr)){IDirect3DDevice7_EndScene(dev);return 0;}
 return note("   EndScene",IDirect3DDevice7_EndScene(dev));
}
/* v2.81 (v10.312; board, v10.311: stage 2 on set 0 read set 1, on set 1 read set 0; VXGL's unit-2 quads showed a diagonal seam):
 * what unit 2 reads. Stages 0 and 1 white (stage 1 on set 1), stage 2 the quadrant texture on set 0, then on set 1; the VxD's probe n
 * (TEXDBG 0x58 + n) puts a marker (0.25, 0.75: blue) in the third set, with strips on and off. Samples (64, 64) and (128, 128): set 0 there
 * is (0.25, 0.25) red and (0.75, 0.75) white, set 1 (0.75, 0.25) green at both, the marker blue at both. */
static void v208_tests(LPDIRECTDRAWSURFACE7 quads) {
 static const int pp[]={64,128,128,64},pk[]={64,64,128,64},pb[]={40,40,64,64,128,64,64,128,128,128},pq[]={96,48,96,112};
 DWORD got[5],in,o[8],gt;int m;char line[256];LPDIRECTDRAWSURFACE7 w=solid_tex(0xFFFFFFFFUL),g8=solid_tex(0xFF808080UL);
 fprintf(logfile,"== v10.208: coordinate set 1, colour key with a program, bump mapping, projected texture\n");
 /* 1. texture 1's coordinates: set A at (64,128) = (0.25, 0.75) blue, at (128,64) = (0.75, 0.25) green; set B = (0.75, 0.75) white everywhere;
  * coordinates of zero read red. v10.209: 04/28 bits 11:10 (set 1 S, T present) */
 if(w){IDirect3DDevice7_SetTexture(dev,0,w);IDirect3DDevice7_SetTexture(dev,1,quads);
  set_stage(0,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,0);
  set_stage(1,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_CURRENT,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_CURRENT,1);q2u=.75F;q2v=.75F;
  if(quad2(0xFFFFFFFFUL)&&read_points(pp,got,2)){sprintf(line,"   texture 1 on coordinate set 1 (0.75, 0.75): white at (64,128) and (128,64) (got %06lX %06lX; FF0000 = zero, 0000FF 00FF00 = set 0)",
    (unsigned long)got[0],(unsigned long)got[1]);check(line,near_rgb(got[0],0xFFFFFFUL)&&near_rgb(got[1],0xFFFFFFUL));}else check("   texture 1 on coordinate set 1",0);
  stages_off();}
 /* 2. colour key with specular (T73) and with a second stage (T125): K (2182C6) keyed on the left, N (C68221) drawn on the right */
 {LPDIRECTDRAWSURFACE7 kt=halves_tex(DDPF_RGB|DDPF_ALPHAPIXELS,32,0xFF0000UL,0xFF00UL,0xFFUL,0xFF2182C6UL,0xFFC68221UL,"A8R8G8B8 key halves");DDCOLORKEY key;
  key.dwColorSpaceLowValue=key.dwColorSpaceHighValue=0xFF2182C6UL;
  if(kt&&note("   SetColorKey(DDCKEY_SRCBLT)",IDirectDrawSurface7_SetColorKey(kt,DDCKEY_SRCBLT,&key))) {
   IDirect3DDevice7_SetTexture(dev,0,kt);rs_set(D3DRENDERSTATE_COLORKEYENABLE,TRUE);
   set_stage(0,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,0);rs_set(D3DRENDERSTATE_SPECULARENABLE,TRUE);q2spec=0xFF004000UL;
   if(quad2(0xFF808080UL)&&read_points(pk,got,2))check("   colour key + MODULATE 808080 + specular 004000: K keyed, N = 638110 (T73)",near_rgb(got[0],0x000060UL)&&near_rgb(got[1],0x638110UL));else check("   colour key + specular",0);
   rs_set(D3DRENDERSTATE_SPECULARENABLE,FALSE);q2spec=0;
   if(g8){IDirect3DDevice7_SetTexture(dev,1,g8);set_stage(0,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,0);
    set_stage(1,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_CURRENT,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_CURRENT,0);
    if(quad2(0xFFFFFFFFUL)&&read_points(pk,got,2))check("   colour key on stage 0, MODULATE texture 1 808080: K keyed, N = 634110 (T125)",near_rgb(got[0],0x000060UL)&&near_rgb(got[1],0x634110UL));else check("   colour key + two stages",0);}
  } else check("   colour key texture",0);
  stages_off();if(kt)IDirectDrawSurface7_Release(kt);}
 /* 3. bump mapping (T146-T148): du, dv = +0.25, -0.25 on the left, -0.25, +0.25 on the right, matrix 0.5 0 0 0.5, texture 1 = the quadrants
  * (WRAP), luminance scale 1 offset 0.25 with L = 0.5; XP's pixels: blue, red, green, blue, white (x 0.75 with luminance) */
 {static const struct {DWORD flags,bits,m0,m1,m2,left,right,lum;const char *name;} bf[3]={
   {DDPF_BUMPDUDV,16,0xFFUL,0xFF00UL,0,0xE020UL,0x20E0UL,0,"V8U8"},
   {DDPF_BUMPDUDV|DDPF_BUMPLUMINANCE,16,0x1FUL,0x3E0UL,0xFC00UL,(32UL<<10)|(0x1CUL<<5)|4UL,(32UL<<10)|(4UL<<5)|0x1CUL,1,"L6V5U5"},
   {DDPF_BUMPDUDV|DDPF_BUMPLUMINANCE,32,0xFFUL,0xFF00UL,0xFF0000UL,0x80E020UL,0x8020E0UL,1,"X8L8V8U8"}};
  static const DWORD wv[5]={0x0000FFUL,0xFF0000UL,0x00FF00UL,0x0000FFUL,0xFFFFFFUL};int b,tci;
  for(b=0;b<3;b++)for(tci=0;tci<2;tci++){LPDIRECTDRAWSURFACE7 bt=halves_tex(bf[b].flags,bf[b].bits,bf[b].m0,bf[b].m1,bf[b].m2,bf[b].left,bf[b].right,bf[b].name);int k,ok=1;
   if(!bt){check(bf[b].name,0);continue;}
   IDirect3DDevice7_SetTexture(dev,0,bt);IDirect3DDevice7_SetTexture(dev,1,quads);
   set_stage(0,bf[b].lum?D3DTOP_BUMPENVMAPLUMINANCE:D3DTOP_BUMPENVMAP,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_SELECTARG1,D3DTA_DIFFUSE,D3DTA_TEXTURE,0);
   ts2(0,D3DTSS_BUMPENVMAT00,0x3F000000UL);ts2(0,D3DTSS_BUMPENVMAT01,0);ts2(0,D3DTSS_BUMPENVMAT10,0);ts2(0,D3DTSS_BUMPENVMAT11,0x3F000000UL);
   ts2(0,D3DTSS_BUMPENVLSCALE,0x3F800000UL);ts2(0,D3DTSS_BUMPENVLOFFSET,0x3E800000UL);
   set_stage(1,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_CURRENT,D3DTOP_SELECTARG2,D3DTA_TEXTURE,D3DTA_CURRENT,(DWORD)tci);ts2(1,D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);ts2(1,D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);
   q2same=1;
   if(quad2(0xFFFFFFFFUL)&&read_points(pb,got,5)){for(k=0;k<5;k++){DWORD want=wv[k];if(bf[b].lum)want=((want&0xFF0000UL)?0xBF0000UL:0)|((want&0xFF00UL)?0xBF00UL:0)|((want&0xFFUL)?0xBFUL:0);if(!near_rgb(got[k],want))ok=0;}
    sprintf(line,"   bump %s, texture 1 on coordinate set %d: %s (T146-T148)",bf[b].name,tci,bf[b].lum?"BF-scaled blue, red, green, blue, white":"blue, red, green, blue, white");check(line,ok);}
   else check(bf[b].name,0);
   stages_off();IDirectDrawSurface7_Release(bt);}}
 /* 4. projected texture (T151): COUNT3 | PROJECTED, WRAP; divided, (96,48) is green and (96,112) white; undivided they read red and blue */
 IDirect3DDevice7_SetTexture(dev,0,quads);set_stage(0,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,0);
 ts2(0,D3DTSS_ADDRESSU,D3DTADDRESS_WRAP);ts2(0,D3DTSS_ADDRESSV,D3DTADDRESS_WRAP);ts2(0,D3DTSS_TEXTURETRANSFORMFLAGS,D3DTTFF_COUNT3|D3DTTFF_PROJECTED);
 if(q3_draw()&&read_points(pq,got,2))check("   projected texture: (96,48) green, (96,112) white (T151)",near_rgb(got[0],0x00FF00UL)&&near_rgb(got[1],0xFFFFFFUL));else check("   projected texture",0);
 stages_off();IDirect3DDevice7_SetTexture(dev,0,quads);
 /* 5. the hang recovery: hang_test, after every other texture test (v2.72) */
 if(w)IDirectDrawSurface7_Release(w);if(g8)IDirectDrawSurface7_Release(g8);
}
/* v2.37 (v10.214): cube maps (T150), batching, one engine burst per packet */
static LPDIRECTDRAWSURFACE7 cube_tex(const DWORD *col) {
 DDSURFACEDESC2 s;LPDIRECTDRAWSURFACE7 t=0;DWORD x,y,k;HRESULT hr;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;s.dwWidth=8;s.dwHeight=8;
 s.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY|DDSCAPS_COMPLEX;s.ddsCaps.dwCaps2=DDSCAPS2_CUBEMAP|DDSCAPS2_CUBEMAP_ALLFACES;
 s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);s.ddpfPixelFormat.dwFlags=DDPF_RGB|DDPF_ALPHAPIXELS;s.ddpfPixelFormat.dwRGBBitCount=32;
 s.ddpfPixelFormat.dwRBitMask=0xFF0000UL;s.ddpfPixelFormat.dwGBitMask=0xFF00UL;s.ddpfPixelFormat.dwBBitMask=0xFFUL;s.ddpfPixelFormat.dwRGBAlphaBitMask=0xFF000000UL;
 if(!note("   CreateSurface(cube map 8x8 A8R8G8B8, all faces)",IDirectDraw7_CreateSurface(dd,&s,&t,0)))return 0;
 for(k=0;k<6;k++){LPDIRECTDRAWSURFACE7 f=t;DDSCAPS2 c;
  if(k){memset(&c,0,sizeof(c));c.dwCaps=DDSCAPS_TEXTURE;c.dwCaps2=DDSCAPS2_CUBEMAP|(DDSCAPS2_CUBEMAP_POSITIVEX<<k);
   hr=IDirectDrawSurface7_GetAttachedSurface(t,&c,&f);if(!note("   GetAttachedSurface(face)",hr)){IDirectDrawSurface7_Release(t);return 0;}}
  memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
  if(FAILED(IDirectDrawSurface7_Lock(f,0,&s,DDLOCK_WAIT|DDLOCK_WRITEONLY,0))){note("   Lock(face)",E_FAIL);if(k)IDirectDrawSurface7_Release(f);IDirectDrawSurface7_Release(t);return 0;}
  for(y=0;y<8;y++)for(x=0;x<8;x++)((DWORD*)((BYTE*)s.lpSurface+y*s.lPitch))[x]=col[k];
  IDirectDrawSurface7_Unlock(f,0);if(k)IDirectDrawSurface7_Release(f);}
 return t;
}
/* six 32x32 quads, quad k with the constant direction of face k (major axis 1, the others 0.1) */
static int cube_draw(void) {
 typedef struct {float x,y,z,rhw;DWORD c,s;float u,v,q;} v3;static v3 v[36];HRESULT hr;int k,j;
 static const float dir[6][3]={{1,.1F,.1F},{-1,.1F,.1F},{.1F,1,.1F},{.1F,-1,.1F},{.1F,.1F,1},{.1F,.1F,-1}};
 static const int cx[6]={0,0,32,0,0,32},cy[6]={0,0,0,32,32,32};
 for(k=0;k<6;k++){float x0=(float)(16+(k%3)*80),y0=(float)(16+(k/3)*80);
  for(j=0;j<6;j++){v3 *p=&v[k*6+j];int c=(j==1||j==3||j==4)?1:0,r=(j==2||j==4||j==5)?1:0;(void)cx;(void)cy;
   p->x=x0+(float)(c*32);p->y=y0+(float)(r*32);p->z=.5F;p->rhw=1;p->c=0xFFFFFFFFUL;p->s=0;p->u=dir[k][0];p->v=dir[k][1];p->q=dir[k][2];}}
 if(!note("   Clear",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF000060UL,1,0))||!note("   BeginScene",IDirect3DDevice7_BeginScene(dev)))return 0;
 hr=IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLELIST,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR|D3DFVF_TEX1|D3DFVF_TEXCOORDSIZE3(0),v,36,0);
 if(!note("   DrawPrimitive(cube directions)",hr)){IDirect3DDevice7_EndScene(dev);return 0;}
 return note("   EndScene",IDirect3DDevice7_EndScene(dev));
}
/* 40 small flat triangles in one DrawPrimitive, colour by index */
static DWORD batch_col(int k){return 0xFF000000UL|((DWORD)(40+k*5)<<16)|((DWORD)(200-k*4)<<8)|(DWORD)(k*6);}
static int batch_draw(void) {
 typedef struct {float x,y,z,rhw;DWORD c,s;} vt;static vt v[120];HRESULT hr;int k,j;
 for(k=0;k<40;k++){float x0=(float)(8+(k%10)*24),y0=(float)(8+(k/10)*24);
  for(j=0;j<3;j++){vt *p=&v[k*3+j];p->x=x0+(j==1?20.F:0);p->y=y0+(j==2?20.F:0);p->z=.5F;p->rhw=1;p->c=batch_col(k);p->s=0;}}
 if(!note("   Clear",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF000060UL,1,0))||!note("   BeginScene",IDirect3DDevice7_BeginScene(dev)))return 0;
 hr=IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLELIST,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR,v,120,0);
 if(!note("   DrawPrimitive(40 triangles)",hr)){IDirect3DDevice7_EndScene(dev);return 0;}
 return note("   EndScene",IDirect3DDevice7_EndScene(dev));
}
static int burst(DWORD mode){DWORD in=mode,o[8],gt=0;return endpoint!=INVALID_HANDLE_VALUE&&DeviceIoControl(endpoint,0x85500022UL,&in,4,o,32,&gt,0)&&gt==32&&o[0]==(mode==0x74?o[0]:mode);}
static void v214_tests(LPDIRECTDRAWSURFACE7 quads) {
 static const DWORD fc[6]={0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL,0xFFFFFF00UL,0xFFFF00FFUL,0xFF00FFFFUL};
 static const char *fn[6]={"+X red","-X green","+Y blue","-Y yellow","+Z magenta","-Z cyan"};
 int pc[12],pt[8],k,m,ok;DWORD got[6];char line[256];LPDIRECTDRAWSURFACE7 cb;vx_d3d_diag d0,d1;D3DDEVICEDESC7 caps;
 fprintf(logfile,"== v10.214: cube maps, batching, one burst per packet\n");
 for(k=0;k<6;k++){pc[k*2]=32+(k%3)*80;pc[k*2+1]=32+(k/3)*80;}
 for(k=0;k<4;k++){pt[k*2]=8+(k*3%10)*24+5;pt[k*2+1]=8+(k*3/10)*24+5;}
 memset(&caps,0,sizeof(caps));IDirect3DDevice7_GetCaps(dev,&caps);
 fprintf(logfile,"   texture caps %08lX: CUBEMAP %s\n",(unsigned long)caps.dpcTriCaps.dwTextureCaps,(caps.dpcTriCaps.dwTextureCaps&D3DPTEXTURECAPS_CUBEMAP)?"yes":"no");
 for(m=0;m<2;m++) {
  if(!burst(m?0x72:0x73)){fprintf(logfile,"   burst: the VxD does not take TEXDBG 0x72/0x73\n");break;}fprintf(logfile,m?"   -- one engine burst per packet (TEXDBG 0x72) --\n":"   -- one burst per triangle (TEXDBG 0x73) --\n");
  /* 1. cube map: each quad reads the face its direction points at */
  stages_off();cb=cube_tex(fc);
  if(cb){IDirect3DDevice7_SetTexture(dev,0,cb);set_stage(0,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,0);
   if(cube_draw()&&read_points(pc,got,6)){ok=1;line[0]=0;for(k=0;k<6;k++){char b[32];sprintf(b," %06lX",(unsigned long)got[k]);strcat(line,b);if(!near_rgb(got[k],fc[k]&0xFFFFFFUL))ok=0;}
    fprintf(logfile,"   cube faces read:%s\n",line);sprintf(line,"   cube map: six directions read %s, %s, %s, %s, %s, %s (T150)",fn[0],fn[1],fn[2],fn[3],fn[4],fn[5]);check(line,ok);}
   else check("   cube map",0);
   stages_off();IDirectDrawSurface7_Release(cb);}
  else check("   cube map created",0);
  /* 2. batching: 40 triangles in one DrawPrimitive reach ring 0 in fewer packets, each pixel its triangle's colour */
  memset(&d0,0,sizeof(d0));d0.cb=sizeof(d0);memset(&d1,0,sizeof(d1));d1.cb=sizeof(d1);
  IDirect3DDevice7_SetTexture(dev,0,0);ts2(0,D3DTSS_COLOROP,D3DTOP_SELECTARG2);ts2(0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);ts2(0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG2);ts2(0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
  if(stats)stats(&d0);
  if(batch_draw()&&read_points(pt,got,4)){unsigned long np=0,nb=0;if(stats&&stats(&d1)){np=d1.native_packets-d0.native_packets;nb=d1.batched-d0.batched;}
   ok=1;for(k=0;k<4;k++)if(!near_rgb(got[k],batch_col(k*3)&0xFFFFFFUL))ok=0;
   fprintf(logfile,"   40 triangles: %lu packets to ring 0 (the clear is one), %lu triangles appended to a batch; pixels %06lX %06lX %06lX %06lX\n",np,nb,(unsigned long)got[0],(unsigned long)got[1],(unsigned long)got[2],(unsigned long)got[3]);
   check("   batching: 40 triangles drawn right in at most 3 packets (+ the clear)",ok&&np>=2&&np<=4);}
  else check("   batching",0);
  stages_off();IDirect3DDevice7_SetTexture(dev,0,quads);
 }
 burst(0x72); /* v2.39: back to the driver's default (on since v10.216) */
}
/* v2.72 (v10.281): the v10.210 hang recovery, now after every other texture test (the cube maps and batching ran after it and saw the
 * engine it reset); --nohang leaves it out. */
static int no_hang;
/* v2.73 (v10.290; board, v10.289 at 16 bpp: the draw after the recovery read the clear colour and the VxD's last status was busy): the
 * recoveries after that draw are read too (a second one means it hung as well), and the test runs again with the VxD's constant groups
 * off (TEXDBG CB), its asynchronous clears off (CD), and both, reported and not counted; the settings are put back after. */
/* v2.74 (v10.291; board, v10.290: the draw after the recovery hung in every configuration): the packets of the last group that did not
 * finish (TEXDBG 0xE0 + n), and the test again with each form of the engine reset (TEXDBG 0xF0 + n) instead of the VxD's switches */
static void texture_tests(void) {
 static const int pts[]={64,64,128,64,64,128,128,128,200,170};
 static const DWORD want[5]={0xFF0000UL,0x00FF00UL,0x0000FFUL,0xFFFFFFUL,0x000060UL};
 LPDIRECTDRAWSURFACE7 t=0,sys=0,big=0,x8=0,half=0;DWORD got[8],before;vx3dn_info info;D3DDEVICEDESC7 caps;DDSURFACEDESC2 s;HRESULT hr;
 fprintf(logfile,"== textures\n");
 memset(&caps,0,sizeof(caps));IDirect3DDevice7_GetCaps(dev,&caps);
 fprintf(logfile,"   caps: texture %08lX filter %08lX address %08lX, max %lux%lu, op caps %08lX, stages %u, simultaneous %u\n",(unsigned long)caps.dpcTriCaps.dwTextureCaps,
  (unsigned long)caps.dpcTriCaps.dwTextureFilterCaps,(unsigned long)caps.dpcTriCaps.dwTextureAddressCaps,(unsigned long)caps.dwMaxTextureWidth,(unsigned long)caps.dwMaxTextureHeight,
  (unsigned long)caps.dwTextureOpCaps,(unsigned)caps.wMaxTextureBlendStages,(unsigned)caps.wMaxSimultaneousTextures);
 note("   EnumTextureFormats",IDirect3DDevice7_EnumTextureFormats(dev,texfmt,0));
 before=native_info(&info)?info.triangles:0;
 if(!(t=make_texture(8,8,1,DDSCAPS_VIDEOMEMORY,0,"written by Lock")))goto out;
 if(!note("   SetTexture",IDirect3DDevice7_SetTexture(dev,0,t)))goto out;
 if(!ts(D3DTSS_COLOROP,D3DTOP_SELECTARG1)||!ts(D3DTSS_COLORARG1,D3DTA_TEXTURE)||!ts(D3DTSS_ALPHAOP,D3DTOP_SELECTARG1)||!ts(D3DTSS_ALPHAARG1,D3DTA_TEXTURE)||
    !ts(D3DTSS_MAGFILTER,D3DTFG_POINT)||!ts(D3DTSS_MINFILTER,D3DTFN_POINT)||!ts(D3DTSS_ADDRESSU,D3DTADDRESS_CLAMP)||!ts(D3DTSS_ADDRESSV,D3DTADDRESS_CLAMP)){check("   texture stage states",0);goto out;}
 {DWORD passes=0;hr=IDirect3DDevice7_ValidateDevice(dev,&passes);fprintf(logfile,"   ValidateDevice = %08lX, passes %lu\n",(unsigned long)hr,(unsigned long)passes);}
 if(tex_quad(0,1))expect(pts,want,5,"   point-sampled 8x8 texture, four quadrants");
 /* coordinates 0..2: x=100 is u=1.06, x=144 is u=1.75 */
 {static const int p2[]={48,48,100,48,144,48};
  DWORD wrap[3],mirror[3],clamp[3];wrap[0]=mirror[0]=clamp[0]=quad_colour[0];wrap[1]=quad_colour[0];wrap[2]=quad_colour[1];
  mirror[1]=quad_colour[1];mirror[2]=quad_colour[0];clamp[1]=clamp[2]=quad_colour[1];
  /* v2.5: ADDRESSU/ADDRESSV, not D3DTSS_ADDRESS. The DirectX 7 runtime drops a D3DTSS_ADDRESS write equal to its own copy of ADDRESS (still
   * WRAP, the default, after this program set ADDRESSU/V to CLAMP), so ADDRESS = WRAP never reached the driver (board, v10.175 record trace). */
  if(ts(D3DTSS_ADDRESSU,D3DTADDRESS_WRAP)&&ts(D3DTSS_ADDRESSV,D3DTADDRESS_WRAP)&&tex_quad(0,2))expect(p2,wrap,3,"   address WRAP beyond 1 (ADDRESSU/V)");
  if(ts(D3DTSS_ADDRESS,D3DTADDRESS_MIRROR)&&tex_quad(0,2))expect(p2,mirror,3,"   address MIRROR beyond 1 (D3DTSS_ADDRESS)");
  if(ts(D3DTSS_ADDRESS,D3DTADDRESS_CLAMP)&&tex_quad(0,2))expect(p2,clamp,3,"   address CLAMP beyond 1 (D3DTSS_ADDRESS)");
  if(ts(D3DTSS_ADDRESS,D3DTADDRESS_WRAP)&&tex_quad(0,2))expect(p2,wrap,3,"   address WRAP beyond 1 (D3DTSS_ADDRESS, changed from CLAMP)");}
 /* bilinear: x=96 lies on the red/green edge of the top half */
 {static const int p3[]={96,64,48,48};
  if(ts(D3DTSS_MAGFILTER,D3DTFG_LINEAR)&&ts(D3DTSS_MINFILTER,D3DTFN_LINEAR)&&tex_quad(0,1)&&read_points(p3,got,2))
   check("   bilinear filter blends across the quadrant edge",((got[0]>>16)&255)>=40&&((got[0]>>16)&255)<=215&&((got[0]>>8)&255)>=40&&((got[0]>>8)&255)<=215&&near_rgb(got[1],0xFF0000UL));
  ts(D3DTSS_MAGFILTER,D3DTFG_POINT);ts(D3DTSS_MINFILTER,D3DTFN_POINT);}
 /* texture load: system-memory copy -> video texture by Blt (how DirectX 7 texture management and most loaders do it) */
 if((sys=make_texture(64,64,1,DDSCAPS_SYSTEMMEMORY,0,"source copy"))!=0) {
  memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;s.dwWidth=64;s.dwHeight=64;
  s.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);s.ddpfPixelFormat.dwFlags=DDPF_RGB|DDPF_ALPHAPIXELS;
  s.ddpfPixelFormat.dwRGBBitCount=32;s.ddpfPixelFormat.dwRBitMask=0xFF0000UL;s.ddpfPixelFormat.dwGBitMask=0xFF00UL;s.ddpfPixelFormat.dwBBitMask=0xFFUL;s.ddpfPixelFormat.dwRGBAlphaBitMask=0xFF000000UL;
  if(note("   CreateSurface(A8R8G8B8 64x64 video memory, loaded by Blt)",IDirectDraw7_CreateSurface(dd,&s,&big,0))&&
     note("   Blt(system copy -> video texture)",IDirectDrawSurface7_Blt(big,0,sys,0,DDBLT_WAIT,0))&&
     note("   SetTexture(64x64)",IDirect3DDevice7_SetTexture(dev,0,big))&&ts(D3DTSS_ADDRESS,D3DTADDRESS_CLAMP)&&tex_quad(0,1))
   expect(pts,want,5,"   64x64 texture loaded by Blt from system memory");
  if(big){IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(big);big=0;}
 }
 if((big=make_texture(256,256,1,DDSCAPS_VIDEOMEMORY,0,"large"))!=0&&note("   SetTexture(256x256)",IDirect3DDevice7_SetTexture(dev,0,big))&&tex_quad(0,1))
  expect(pts,want,5,"   256x256 texture");
 if((x8=make_texture(16,16,0,DDSCAPS_VIDEOMEMORY,0,"no alpha"))!=0&&note("   SetTexture(X8R8G8B8)",IDirect3DDevice7_SetTexture(dev,0,x8))&&tex_quad(0,1))
  expect(pts,want,5,"   X8R8G8B8 texture");
 /* texture alpha 0x80 blended SRCALPHA/INVSRCALPHA over the dark blue clear: about half colour plus half 000060 */
 if((half=make_texture(8,8,1,DDSCAPS_VIDEOMEMORY,0x80000000UL,"alpha 0x80"))!=0&&note("   SetTexture(alpha)",IDirect3DDevice7_SetTexture(dev,0,half))) {
  static const int p4[]={64,64,128,128};
  IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA);
  IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);
  if(tex_quad(0,1)&&read_points(p4,got,2))
   check("   texture alpha into the blender (red at half over blue, white at half)",((got[0]>>16)&255)>=100&&((got[0]>>16)&255)<=160&&(got[0]&255)>=32&&(got[0]&255)<=80&&((got[1]>>8)&255)>=100&&((got[1]>>8)&255)<=160);
  IDirect3DDevice7_SetRenderState(dev,D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);
 }
 /* v2.2 (v10.172): XP's blend programs. Red texel (point sampled, quadrant 0) against a grey diffuse. */
 if(t&&note("   SetTexture(8x8)",IDirect3DDevice7_SetTexture(dev,0,t))) {
  static const int p5[]={64,64};
  static const struct {DWORD op,diffuse,want;const char *name;} bl[]={
   {D3DTOP_MODULATE,0xFF808080UL,0x800000UL,"MODULATE red x 808080 = 800000"},
   {D3DTOP_MODULATE2X,0xFF606060UL,0xC00000UL,"MODULATE2X red x 606060 = C00000"},
   {D3DTOP_MODULATE4X,0xFF303030UL,0xC00000UL,"MODULATE4X red x 303030 = C00000"},
   {D3DTOP_ADD,0xFF404040UL,0xFF4040UL,"ADD red + 404040 = FF4040"},
   {D3DTOP_ADDSIGNED,0xFFA0A0A0UL,0xFF2020UL,"ADDSIGNED red + A0A0A0 - 0.5 = FF2020"}};
  int k;
  ts(D3DTSS_ADDRESS,D3DTADDRESS_CLAMP);ts(D3DTSS_COLORARG1,D3DTA_TEXTURE);ts(D3DTSS_COLORARG2,D3DTA_DIFFUSE);
  for(k=0;k<5;k++){char line[96];quad_diffuse=bl[k].diffuse;
   if(ts(D3DTSS_COLOROP,bl[k].op)&&tex_quad(0,1)&&read_points(p5,got,1)){sprintf(line,"   %s",bl[k].name);check(line,near_rgb(got[0],bl[k].want));}}
  quad_diffuse=0xFF808080UL;rs_set(D3DRENDERSTATE_TEXTUREFACTOR,0xFF40C080UL);
  if(ts(D3DTSS_COLOROP,D3DTOP_MODULATE)&&ts(D3DTSS_COLORARG2,D3DTA_TFACTOR)&&tex_quad(0,1)&&read_points(p5,got,1))check("   MODULATE red x TFACTOR 40C080 = 400000",near_rgb(got[0],0x400000UL));
  ts(D3DTSS_COLORARG2,D3DTA_DIFFUSE);quad_diffuse=0xFFFFFFFFUL;ts(D3DTSS_COLOROP,D3DTOP_SELECTARG1);
  /* v2.24 (v10.198): programs built the way XP's driver builds them: the remaining ops, argument modifiers, specular on other programs,
   * colour and alpha set independently (alpha shown by SRCALPHA blending over the clear colour 000060). Red texel, opaque. */
  {static const struct {DWORD op,a1,a2,aop,b1,b2,diffuse,spec,tf,blend,want;const char *name;} xo[]={
    {D3DTOP_SUBTRACT,D3DTA_TEXTURE,D3DTA_DIFFUSE,0,0,0,0xFF404040UL,0,0,0,0xBF0000UL,"SUBTRACT red - 404040 = BF0000"},
    {D3DTOP_ADDSIGNED2X,D3DTA_TEXTURE,D3DTA_DIFFUSE,0,0,0,0xFFA0A0A0UL,0,0,0,0xFF4040UL,"ADDSIGNED2X red, A0A0A0 = FF4040"},
    {D3DTOP_ADDSMOOTH,D3DTA_TEXTURE,D3DTA_DIFFUSE,0,0,0,0xFFA0A0A0UL,0,0,0,0xFFA0A0UL,"ADDSMOOTH red, A0A0A0 = FFA0A0"},
    {D3DTOP_BLENDFACTORALPHA,D3DTA_TEXTURE,D3DTA_DIFFUSE,0,0,0,0xFF404040UL,0,0x80000000UL,0,0xA02020UL,"BLENDFACTORALPHA (TFACTOR alpha 80) red, 404040 = A02020"},
    {D3DTOP_BLENDCURRENTALPHA,D3DTA_TEXTURE,D3DTA_DIFFUSE,0,0,0,0x80404040UL,0,0,0,0xA02020UL,"BLENDCURRENTALPHA (diffuse alpha 80) red, 404040 = A02020"},
    {D3DTOP_MODULATEALPHA_ADDCOLOR,D3DTA_TEXTURE,D3DTA_DIFFUSE,0,0,0,0xFF404040UL,0,0,0,0xFF4040UL,"MODULATEALPHA_ADDCOLOR red + 1 x 404040 = FF4040"},
    {D3DTOP_MODULATECOLOR_ADDALPHA,D3DTA_TEXTURE,D3DTA_DIFFUSE,0,0,0,0xFF404040UL,0,0,0,0xFFFFFFUL,"MODULATECOLOR_ADDALPHA red x 404040 + 1 = FFFFFF"},
    {D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE|D3DTA_COMPLEMENT,0,0,0,0xFF404040UL,0,0,0,0xBF0000UL,"MODULATE red x (404040 | COMPLEMENT) = BF0000"},
    {D3DTOP_SELECTARG1,D3DTA_TEXTURE|D3DTA_COMPLEMENT,D3DTA_DIFFUSE,0,0,0,0xFFFFFFFFUL,0,0,0,0x00FFFFUL,"SELECTARG1 red | COMPLEMENT = 00FFFF"},
    {D3DTOP_SELECTARG1,D3DTA_DIFFUSE|D3DTA_ALPHAREPLICATE,D3DTA_TEXTURE,0,0,0,0x80FF0000UL,0,0,0,0x808080UL,"SELECTARG1 diffuse (alpha 80) | ALPHAREPLICATE = 808080"},
    {D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_TFACTOR|D3DTA_COMPLEMENT,0,0,0,0xFFFFFFFFUL,0,0xFF404040UL,0,0xBF0000UL,"MODULATE red x (TFACTOR 404040 | COMPLEMENT) = BF0000"},
    {D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_SPECULAR,0,0,0,0xFFFFFFFFUL,0xFF808080UL,0,0,0x800000UL,"MODULATE red x SPECULAR 808080 = 800000"},
    {D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE,0,0,0,0xFF808080UL,0xFF004000UL,0,2,0x804000UL,"MODULATE red x 808080 + specular 004000 = 804000"},
    {D3DTOP_ADD,D3DTA_TEXTURE,D3DTA_DIFFUSE,0,0,0,0xFF000040UL,0xFF004000UL,0,2,0xFF4040UL,"ADD red + 000040 + specular 004000 = FF4040"},
    {D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_SELECTARG1,D3DTA_DIFFUSE,D3DTA_TEXTURE,0x80FFFFFFUL,0,0,1,0x800030UL,"colour MODULATE, alpha = diffuse 80, blended = 800030"},
    {D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE,0x80FFFFFFUL,0,0,1,0x800030UL,"colour = texture, alpha MODULATE (FF x 80), blended = 800030"},
    {D3DTOP_ADD,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_SELECTARG1,D3DTA_TFACTOR,D3DTA_TEXTURE,0xFF000040UL,0,0x40FFFFFFUL,1,0x400058UL,"colour ADD red + 000040, alpha = TFACTOR 40, blended = 400058"},
    {D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_SELECTARG1,D3DTA_TEXTURE|D3DTA_COMPLEMENT,D3DTA_DIFFUSE,0xFFFFFFFFUL,0,0,1,0x000060UL,"alpha = texture | COMPLEMENT (0), blended = 000060"},
    /* v2.76 (v10.306): alpha forms no earlier case ran, each one feature of v10.305's GL_BLEND-on-INTENSITY program that hung the engine:
     * the engine's interpolation (w3 op 11) under the alpha mask with TFACTOR in A, as the ICD's LERP now builds it; TFACTOR in B */
    {D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_BLENDDIFFUSEALPHA,D3DTA_TFACTOR,D3DTA_TEXTURE,0x40FFFFFFUL,0,0x00FFFFFFUL,1,0xBF0018UL,"alpha BLENDDIFFUSEALPHA (TFACTOR alpha 0, texture; diffuse alpha 40) = BF, blended = BF0018"},
    {D3DTOP_SELECTARG1,D3DTA_TEXTURE,D3DTA_DIFFUSE,D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_TFACTOR,0xFFFFFFFFUL,0,0xBFFFFFFFUL,1,0xBF0018UL,"alpha MODULATE texture x TFACTOR alpha BF = BF, blended = BF0018"}};
   int k;
   for(k=0;k<(int)(sizeof(xo)/sizeof(xo[0]));k++){char line[128];
    quad_diffuse=xo[k].diffuse;quad_spec=xo[k].spec;rs_set(D3DRENDERSTATE_TEXTUREFACTOR,xo[k].tf);rs_set(D3DRENDERSTATE_SPECULARENABLE,xo[k].blend==2);
    ts(D3DTSS_COLOROP,xo[k].op);ts(D3DTSS_COLORARG1,xo[k].a1);ts(D3DTSS_COLORARG2,xo[k].a2);
    ts(D3DTSS_ALPHAOP,xo[k].aop?xo[k].aop:D3DTOP_SELECTARG1);ts(D3DTSS_ALPHAARG1,xo[k].aop?xo[k].b1:D3DTA_TEXTURE);ts(D3DTSS_ALPHAARG2,xo[k].aop?xo[k].b2:D3DTA_DIFFUSE);
    if(xo[k].blend==1){rs_set(D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);rs_set(D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA);rs_set(D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);}
    sprintf(line,"   %s",xo[k].name);
    if(tex_quad(0,1)&&read_points(p5,got,1))check(line,near_rgb(got[0],xo[k].want));else check(line,0);
    rs_set(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);}
   /* v2.76: two stages, stage 0 colour = texture: an alpha instruction reading the temporary r0 in C (stage 1 alpha ADD(TFACTOR, CURRENT),
    * stage 0's alpha = diffuse 40), and XP's T104 complement form under the alpha mask written to r0 (stage 0 alpha MODULATE(texture,
    * diffuse | COMPLEMENT), read by stage 1 alpha MODULATE(CURRENT, diffuse)) */
   {static const struct {DWORD aop0,a01,a02,aop1,a11,a12,tf,want;const char *name;} two[]={
     {D3DTOP_SELECTARG1,D3DTA_DIFFUSE,D3DTA_TEXTURE,D3DTOP_ADD,D3DTA_TFACTOR,D3DTA_CURRENT,0x20FFFFFFUL,0x60003CUL,"stage 1 alpha ADD (TFACTOR alpha 20 + CURRENT alpha 40) = 60, blended = 60003C"},
     {D3DTOP_MODULATE,D3DTA_TEXTURE,D3DTA_DIFFUSE|D3DTA_COMPLEMENT,D3DTOP_MODULATE,D3DTA_CURRENT,D3DTA_DIFFUSE,0xFFFFFFFFUL,0x30004EUL,"stage 1 alpha MODULATE (CURRENT = texture x (1 - diffuse 40), diffuse 40) = 30, blended = 30004E"}};
    int j;quad_diffuse=0x40FFFFFFUL;quad_spec=0;rs_set(D3DRENDERSTATE_SPECULARENABLE,FALSE);
    rs_set(D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);rs_set(D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA);rs_set(D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);
    ts(D3DTSS_COLOROP,D3DTOP_SELECTARG1);ts(D3DTSS_COLORARG1,D3DTA_TEXTURE);
    IDirect3DDevice7_SetTextureStageState(dev,1,D3DTSS_COLOROP,D3DTOP_SELECTARG1);IDirect3DDevice7_SetTextureStageState(dev,1,D3DTSS_COLORARG1,D3DTA_CURRENT);
    for(j=0;j<2;j++){char line[128];rs_set(D3DRENDERSTATE_TEXTUREFACTOR,two[j].tf);
     ts(D3DTSS_ALPHAOP,two[j].aop0);ts(D3DTSS_ALPHAARG1,two[j].a01);ts(D3DTSS_ALPHAARG2,two[j].a02);
     IDirect3DDevice7_SetTextureStageState(dev,1,D3DTSS_ALPHAOP,two[j].aop1);IDirect3DDevice7_SetTextureStageState(dev,1,D3DTSS_ALPHAARG1,two[j].a11);
     IDirect3DDevice7_SetTextureStageState(dev,1,D3DTSS_ALPHAARG2,two[j].a12);
     sprintf(line,"   %s",two[j].name);
     if(tex_quad(0,1)&&read_points(p5,got,1))check(line,near_rgb(got[0],two[j].want));else check(line,0);}
    rs_set(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);rs_set(D3DRENDERSTATE_TEXTUREFACTOR,0xFFFFFFFFUL);
    IDirect3DDevice7_SetTextureStageState(dev,1,D3DTSS_COLOROP,D3DTOP_DISABLE);IDirect3DDevice7_SetTextureStageState(dev,1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);}
   quad_diffuse=0xFFFFFFFFUL;quad_spec=0;rs_set(D3DRENDERSTATE_SPECULARENABLE,FALSE);
   ts(D3DTSS_COLOROP,D3DTOP_SELECTARG1);ts(D3DTSS_COLORARG1,D3DTA_TEXTURE);ts(D3DTSS_COLORARG2,D3DTA_DIFFUSE);
   ts(D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);ts(D3DTSS_ALPHAARG1,D3DTA_TEXTURE);ts(D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);}
 }
 /* v2.2: 16-bit texture formats, the four quadrants each */
 {static const struct {DWORD flags,r,g,b,a;const char *name;} f16[]={
   {DDPF_RGB,0xF800,0x7E0,0x1F,0,"R5G6B5"},{DDPF_RGB|DDPF_ALPHAPIXELS,0x7C00,0x3E0,0x1F,0x8000,"A1R5G5B5"},
   {DDPF_RGB|DDPF_ALPHAPIXELS,0xF00,0xF0,0xF,0xF000,"A4R4G4B4"},{DDPF_RGB,0x7C00,0x3E0,0x1F,0,"X1R5G5B5"}};
  int k;
  for(k=0;k<4;k++){LPDIRECTDRAWSURFACE7 t16=make_texture16(16,16,f16[k].flags,f16[k].r,f16[k].g,f16[k].b,f16[k].a,f16[k].name);char line[64];
   if(!t16){check(f16[k].name,0);continue;}
   if(note("   SetTexture(16-bit)",IDirect3DDevice7_SetTexture(dev,0,t16))&&tex_quad(0,1)){sprintf(line,"   texture format %s",f16[k].name);expect(pts,want,5,line);}
   IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(t16);}
 }
 /* v2.22 (v10.197): texture colour key (SetColorKey DDCKEY_SRCBLT + COLORKEYENABLE, SELECTARG1 texture). Left half of the texture is the key
  * colour K, right half its channel-reversed twin N: keyed pixels show the clear colour 000060, so K keyed and N drawn settles the key's
  * channel order (N keyed instead = reversed). The key is given with alpha FF, then with alpha 00, then on an R5G6B5 texture. */
 {static const int pk[]={64,64,128,64};
  static const struct {DWORD k,n,key;int bits16;const char *name;} ck[]={
   {0xFF2182C6UL,0xFFC68221UL,0xFF2182C6UL,0,"A8R8G8B8, key 2182C6 with alpha FF"},
   {0xFF2182C6UL,0xFFC68221UL,0x002182C6UL,0,"A8R8G8B8, key 2182C6 with alpha 00"},
   {0xFF2182C6UL,0xFFC68221UL,0x2418UL,1,"R5G6B5, key 2418 (2182C6)"}};
  int k;
  for(k=0;k<3;k++){LPDIRECTDRAWSURFACE7 kt=0;DDSURFACEDESC2 d;DDCOLORKEY key;DWORD x,y,w2[2];char line[128];
   memset(&d,0,sizeof(d));d.dwSize=sizeof(d);d.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;d.dwWidth=8;d.dwHeight=8;d.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_VIDEOMEMORY;
   d.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);d.ddpfPixelFormat.dwFlags=DDPF_RGB|(ck[k].bits16?0:DDPF_ALPHAPIXELS);d.ddpfPixelFormat.dwRGBBitCount=ck[k].bits16?16:32;
   d.ddpfPixelFormat.dwRBitMask=ck[k].bits16?0xF800UL:0xFF0000UL;d.ddpfPixelFormat.dwGBitMask=ck[k].bits16?0x7E0UL:0xFF00UL;d.ddpfPixelFormat.dwBBitMask=ck[k].bits16?0x1FUL:0xFFUL;
   d.ddpfPixelFormat.dwRGBAlphaBitMask=ck[k].bits16?0:0xFF000000UL;
   sprintf(line,"   CreateSurface(colour-key texture, %s)",ck[k].name);
   if(!note(line,IDirectDraw7_CreateSurface(dd,&d,&kt,0))){check(line,0);continue;}
   memset(&d,0,sizeof(d));d.dwSize=sizeof(d);
   if(note("   Lock(texture)",IDirectDrawSurface7_Lock(kt,0,&d,DDLOCK_WAIT|DDLOCK_WRITEONLY,0))){
    for(y=0;y<8;y++)for(x=0;x<8;x++){DWORD c=x<4?ck[k].k:ck[k].n;
     if(ck[k].bits16)((WORD*)((BYTE*)d.lpSurface+y*d.lPitch))[x]=(WORD)pack16(c,0xF800,0x7E0,0x1F,0);else ((DWORD*)((BYTE*)d.lpSurface+y*d.lPitch))[x]=c;}
    note("   Unlock(texture)",IDirectDrawSurface7_Unlock(kt,0));}
   key.dwColorSpaceLowValue=key.dwColorSpaceHighValue=ck[k].key;
   if(note("   SetColorKey(DDCKEY_SRCBLT)",IDirectDrawSurface7_SetColorKey(kt,DDCKEY_SRCBLT,&key))&&note("   SetTexture(colour key)",IDirect3DDevice7_SetTexture(dev,0,kt))&&
      ts(D3DTSS_COLOROP,D3DTOP_SELECTARG1)&&ts(D3DTSS_COLORARG1,D3DTA_TEXTURE)&&ts(D3DTSS_ALPHAOP,D3DTOP_SELECTARG1)&&ts(D3DTSS_ALPHAARG1,D3DTA_TEXTURE)) {
    rs_set(D3DRENDERSTATE_COLORKEYENABLE,TRUE);
    if(tex_quad(0,1)&&read_points(pk,w2,2)){
     sprintf(line,"   colour key %s: K keyed, N drawn%s",ck[k].name,near_rgb(w2[0],ck[k].n&0xFFFFFFUL)&&near_rgb(w2[1],0x000060UL)?" (got the reverse: channel order swapped)":"");
     check(line,near_rgb(w2[0],0x000060UL)&&near_rgb(w2[1],ck[k].n&0xFFFFFFUL));}
    rs_set(D3DRENDERSTATE_COLORKEYENABLE,FALSE);
    if(k==0&&tex_quad(0,1)&&read_points(pk,w2,2))check("   colour key off: both halves drawn",near_rgb(w2[0],ck[k].k&0xFFFFFFUL)&&near_rgb(w2[1],ck[k].n&0xFFFFFFUL));
   }
   IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(kt);}
 }
 v199_textures(pts,want);
 stage_tests(t);
 v208_tests(t);
 v214_tests(t);
 check("   native GPU triangle counter increased",native_info(&info)&&info.triangles-before>=34&&!info.fault);
out:
 IDirect3DDevice7_SetTexture(dev,0,0);
 if(t)IDirectDrawSurface7_Release(t);if(sys)IDirectDrawSurface7_Release(sys);if(big)IDirectDrawSurface7_Release(big);if(x8)IDirectDrawSurface7_Release(x8);if(half)IDirectDrawSurface7_Release(half);
 fflush(logfile);
}
/* v2.6 (v10.177): every blend factor, alpha test, Z16 / D24S8 and clears on the 2D engine. Untextured flat triangles (diffuse only). */
static HRESULT CALLBACK zfmt(LPDDPIXELFORMAT f,LPVOID unused) {
 (void)unused;fprintf(logfile,"   Z format: flags %08lX depth %lu Z mask %08lX stencil mask %08lX\n",(unsigned long)f->dwFlags,(unsigned long)f->dwZBufferBitDepth,(unsigned long)f->dwZBitMask,(unsigned long)f->dwStencilBitMask);
 return D3DENUMRET_OK;
}
static DWORD bf(DWORD f,DWORD s,DWORD d,int sh) {
 DWORD sc=(s>>sh)&255,dc=(d>>sh)&255,sa=s>>24,da=255; /* the target is X8R8G8B8: destination alpha reads as 1 */
 switch(f){case D3DBLEND_ZERO:return 0;case D3DBLEND_ONE:return 255;case D3DBLEND_SRCCOLOR:return sc;case D3DBLEND_INVSRCCOLOR:return 255-sc;
  case D3DBLEND_SRCALPHA:return sa;case D3DBLEND_INVSRCALPHA:return 255-sa;case D3DBLEND_DESTALPHA:return da;case D3DBLEND_INVDESTALPHA:return 255-da;
  case D3DBLEND_DESTCOLOR:return dc;case D3DBLEND_INVDESTCOLOR:return 255-dc;case D3DBLEND_SRCALPHASAT:return sa<255-da?sa:255-da;}
 return 0;
}
static DWORD blend_want(DWORD fs,DWORD fd,DWORD s,DWORD d) {
 DWORD out=0,v;int sh;
 if(fs==D3DBLEND_BOTHSRCALPHA){fs=D3DBLEND_SRCALPHA;fd=D3DBLEND_INVSRCALPHA;}
 if(fs==D3DBLEND_BOTHINVSRCALPHA){fs=D3DBLEND_INVSRCALPHA;fd=D3DBLEND_SRCALPHA;}
 for(sh=0;sh<24;sh+=8){v=(((s>>sh)&255)*bf(fs,s,d,sh)+((d>>sh)&255)*bf(fd,s,d,sh)+127)/255;if(v>255)v=255;out|=v<<sh;}
 return out;
}
static int flat_draw(float z,DWORD c) {tl_vertex v[3];triangle(v,z,c,c,c);return draw(v);}
/* v2.27 (v10.201): the whole 256x192 target as two triangles; a line list / strip / point list of n vertices */
static int full_draw(float z,DWORD c) {
 tl_vertex v[6];int k;HRESULT hr;memset(v,0,sizeof(v));
 v[0].x=0;v[0].y=0;v[1].x=256;v[1].y=0;v[2].x=0;v[2].y=192;v[3]=v[1];v[4].x=256;v[4].y=192;v[5]=v[2];
 for(k=0;k<6;k++){v[k].z=z;v[k].rhw=1;v[k].color=c;}
 if(!note("BeginScene",IDirect3DDevice7_BeginScene(dev)))return 0;
 hr=IDirect3DDevice7_DrawPrimitive(dev,D3DPT_TRIANGLELIST,D3DFVF_TLVERTEX,v,6,0);
 if(!note("DrawPrimitive(full target)",hr)){IDirect3DDevice7_EndScene(dev);return 0;}
 return note("EndScene",IDirect3DDevice7_EndScene(dev));
}
static int prim_draw(D3DPRIMITIVETYPE t,const float *xy,int n,DWORD c) {
 tl_vertex v[8];int k;HRESULT hr;memset(v,0,sizeof(v));
 for(k=0;k<n;k++){v[k].x=xy[k*2];v[k].y=xy[k*2+1];v[k].z=.5F;v[k].rhw=1;v[k].color=c;}
 if(!note("BeginScene",IDirect3DDevice7_BeginScene(dev)))return 0;
 hr=IDirect3DDevice7_DrawPrimitive(dev,t,D3DFVF_TLVERTEX,v,n,0);
 if(!note("DrawPrimitive(lines/points)",hr)){IDirect3DDevice7_EndScene(dev);return 0;}
 return note("EndScene",IDirect3DDevice7_EndScene(dev));
}
static int any_near(const int *xy,int n,DWORD want) {DWORD g[8];int k;if(!read_points(xy,g,n))return 0;for(k=0;k<n;k++)if(near_rgb(g[k],want))return 1;return 0;}
static int one_point(int x,int y,DWORD *out) {int xy[2];xy[0]=x;xy[1]=y;return read_points(xy,out,1);}
static int clear(DWORD flags,DWORD c,float z) {return note("   Clear",IDirect3DDevice7_Clear(dev,0,0,flags,c,z,0));}
static void raster_tests(void) {
 static const DWORD fsrc[]={D3DBLEND_ONE,D3DBLEND_ZERO,D3DBLEND_ONE,D3DBLEND_SRCCOLOR,D3DBLEND_INVSRCCOLOR,D3DBLEND_SRCALPHA,D3DBLEND_INVSRCALPHA,
  D3DBLEND_DESTALPHA,D3DBLEND_INVDESTALPHA,D3DBLEND_DESTCOLOR,D3DBLEND_INVDESTCOLOR,D3DBLEND_SRCALPHASAT,D3DBLEND_BOTHSRCALPHA,D3DBLEND_BOTHINVSRCALPHA,
  D3DBLEND_ZERO,D3DBLEND_ZERO,D3DBLEND_ZERO,D3DBLEND_ZERO,D3DBLEND_ZERO,D3DBLEND_ONE,D3DBLEND_ONE,D3DBLEND_ONE};
 static const DWORD fdst[]={D3DBLEND_ZERO,D3DBLEND_ONE,D3DBLEND_ONE,D3DBLEND_ZERO,D3DBLEND_ZERO,D3DBLEND_INVSRCALPHA,D3DBLEND_SRCALPHA,
  D3DBLEND_ZERO,D3DBLEND_ONE,D3DBLEND_ZERO,D3DBLEND_ZERO,D3DBLEND_ONE,D3DBLEND_ZERO,D3DBLEND_ZERO,
  D3DBLEND_SRCCOLOR,D3DBLEND_INVSRCCOLOR,D3DBLEND_DESTCOLOR,D3DBLEND_INVDESTCOLOR,D3DBLEND_SRCALPHA,D3DBLEND_DESTALPHA,D3DBLEND_INVDESTALPHA,D3DBLEND_INVSRCCOLOR};
 static const char *bn[14]={"?","ZERO","ONE","SRCCOLOR","INVSRCCOLOR","SRCALPHA","INVSRCALPHA","DESTALPHA","INVDESTALPHA","DESTCOLOR","INVDESTCOLOR","SRCALPHASAT","BOTHSRCALPHA","BOTHINVSRCALPHA"};
 const DWORD src=0x80C04020UL,dst=0xFF406080UL;D3DDEVICEDESC7 caps;DWORD got,before,t0,t1,k;vx3dn_info info;char line[128];int z;
 fprintf(logfile,"== raster: blend factors, alpha test, depth formats, clears\n");
 memset(&caps,0,sizeof(caps));IDirect3DDevice7_GetCaps(dev,&caps);
 fprintf(logfile,"   caps: src blend %08lX dest blend %08lX alpha compare %08lX Z compare %08lX Z depths %08lX\n",(unsigned long)caps.dpcTriCaps.dwSrcBlendCaps,
  (unsigned long)caps.dpcTriCaps.dwDestBlendCaps,(unsigned long)caps.dpcTriCaps.dwAlphaCmpCaps,(unsigned long)caps.dpcTriCaps.dwZCmpCaps,(unsigned long)caps.dwDeviceZBufferBitDepth);
 note("   EnumZBufferFormats",IDirect3D7_EnumZBufferFormats(d3d,&IID_IDirect3DHALDevice,zfmt,0));
 before=native_info(&info)?info.triangles:0;
 /* blend: source 80C04020 (alpha 0x80) over the clear colour 406080 */
 rs_set(D3DRENDERSTATE_SHADEMODE,D3DSHADE_FLAT);rs_set(D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);
 for(k=0;k<sizeof(fsrc)/sizeof(fsrc[0]);k++) {
  DWORD want=blend_want(fsrc[k],fdst[k],src,dst);
  rs_set(D3DRENDERSTATE_SRCBLEND,fsrc[k]);rs_set(D3DRENDERSTATE_DESTBLEND,fdst[k]);
  sprintf(line,"   blend %s / %s = %06lX",bn[fsrc[k]],bn[fdst[k]],(unsigned long)want);
  if(clear(D3DCLEAR_TARGET,dst,1)&&flat_draw(.5F,src)&&one_point(80,72,&got))check(line,near_rgb(got,want));else check(line,0);
 }
 rs_set(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);
 /* alpha test: reference 0x90 */
 rs_set(D3DRENDERSTATE_ALPHATESTENABLE,TRUE);rs_set(D3DRENDERSTATE_ALPHAREF,0x90);
 {static const struct {DWORD func,alpha;int drawn;const char *name;} at[]={
   {D3DCMP_GREATEREQUAL,0x80,0,"GREATEREQUAL 0x90, alpha 0x80 rejected"},{D3DCMP_GREATEREQUAL,0xA0,1,"GREATEREQUAL 0x90, alpha 0xA0 drawn"},
   {D3DCMP_GREATEREQUAL,0x90,1,"GREATEREQUAL 0x90, alpha 0x90 drawn"},{D3DCMP_LESS,0x80,1,"LESS 0x90, alpha 0x80 drawn"},
   {D3DCMP_LESS,0xA0,0,"LESS 0x90, alpha 0xA0 rejected"},{D3DCMP_EQUAL,0x90,1,"EQUAL 0x90, alpha 0x90 drawn"},
   {D3DCMP_NOTEQUAL,0x90,0,"NOTEQUAL 0x90, alpha 0x90 rejected"},{D3DCMP_NEVER,0xFF,0,"NEVER rejected"},{D3DCMP_ALWAYS,0x00,1,"ALWAYS drawn"}};
  for(k=0;k<sizeof(at)/sizeof(at[0]);k++){sprintf(line,"   alpha test %s",at[k].name);rs_set(D3DRENDERSTATE_ALPHAFUNC,at[k].func);
   if(clear(D3DCLEAR_TARGET,dst,1)&&flat_draw(.5F,(at[k].alpha<<24)|0xFF0000UL)&&one_point(80,72,&got))check(line,near_rgb(got,at[k].drawn?0xFF0000UL:dst&0xFFFFFFUL));else check(line,0);}
 }
 rs_set(D3DRENDERSTATE_ALPHATESTENABLE,FALSE);
 /* v2.12 (v10.180): an X8R8G8B8 target has no alpha: after a draw with source alpha 0, DESTALPHA still reads 1 (target format 98, not 99) */
 {static const struct {DWORD a,fs,fd;const char *name;} da[]={{0x00,D3DBLEND_DESTALPHA,D3DBLEND_ZERO,"DESTALPHA / ZERO after alpha 00"},
   {0x40,D3DBLEND_DESTALPHA,D3DBLEND_ZERO,"DESTALPHA / ZERO after alpha 40"},{0x00,D3DBLEND_ONE,D3DBLEND_INVDESTALPHA,"ONE / INVDESTALPHA after alpha 00"}};
  for(k=0;k<3;k++){sprintf(line,"   XRGB destination alpha is 1: %s",da[k].name);
   rs_set(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);
   if(!clear(D3DCLEAR_TARGET,dst,1)||!flat_draw(.5F,(da[k].a<<24)|0xFF0000UL)){check(line,0);continue;}
   rs_set(D3DRENDERSTATE_ALPHABLENDENABLE,TRUE);rs_set(D3DRENDERSTATE_SRCBLEND,da[k].fs);rs_set(D3DRENDERSTATE_DESTBLEND,da[k].fd);
   if(flat_draw(.5F,0x8000C060UL)&&one_point(80,72,&got))check(line,near_rgb(got,0x00C060UL));else check(line,0);}
  rs_set(D3DRENDERSTATE_ALPHABLENDENABLE,FALSE);}
 /* v2.12: a NULL texture with COLORARG1 = TEXTURE ends the stage: the draw is the diffuse colour, whatever the unused state says */
 {LPDIRECTDRAWSURFACE7 t=make_texture(8,8,1,DDSCAPS_VIDEOMEMORY,0,"for the NULL-texture check");
  if(t){IDirect3DDevice7_SetTexture(dev,0,t);ts(D3DTSS_COLOROP,D3DTOP_MODULATE);ts(D3DTSS_COLORARG1,D3DTA_TEXTURE);ts(D3DTSS_COLORARG2,D3DTA_TFACTOR);
   ts(D3DTSS_ALPHAOP,D3DTOP_MODULATE);ts(D3DTSS_ALPHAARG1,D3DTA_TEXTURE);ts(D3DTSS_ALPHAARG2,D3DTA_TFACTOR);
   IDirect3DDevice7_SetTexture(dev,0,0);IDirectDrawSurface7_Release(t);}
  if(clear(D3DCLEAR_TARGET,dst,1)&&flat_draw(.5F,0xFF30A050UL)&&one_point(80,72,&got))check("   NULL texture after MODULATE(TEXTURE,TFACTOR) draws the diffuse colour",near_rgb(got,0x30A050UL));
  else check("   NULL texture after MODULATE(TEXTURE,TFACTOR) draws the diffuse colour",0);
  ts(D3DTSS_COLOROP,D3DTOP_SELECTARG1);ts(D3DTSS_COLORARG1,D3DTA_DIFFUSE);ts(D3DTSS_ALPHAOP,D3DTOP_SELECTARG1);ts(D3DTSS_ALPHAARG1,D3DTA_DIFFUSE);}
 /* clears: a partial clear with odd edges, then the time of a whole 256x192 clear */
 {RECT r;r.left=33;r.top=33;r.right=99;r.bottom=99;
  if(clear(D3DCLEAR_TARGET,0xFF000000UL,1)&&note("   Clear(rect 33,33-99,99)",IDirect3DDevice7_Clear(dev,1,(D3DRECT*)&r,D3DCLEAR_TARGET,0xFF20A040UL,1,0))) {
   static const int cp[]={33,33,98,98,32,40,99,40};DWORD c4[4];
   check("   partial clear inside, edges exact",read_points(cp,c4,4)&&near_rgb(c4[0],0x20A040UL)&&near_rgb(c4[1],0x20A040UL)&&c4[2]==0&&c4[3]==0);
  }
  t0=GetTickCount();for(k=0;k<200;k++)IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,k&1?0xFF304050UL:0xFF102030UL,1,0); /* v2.7: the last one (k = 199) is 304050 */
  one_point(10,10,&got);t1=GetTickCount();
  fprintf(logfile,"   200 clears of 256x192 X8R8G8B8: %lu ms (%lu us each)\n",(unsigned long)(t1-t0),(unsigned long)((t1-t0)*5));
  check("   last clear colour",near_rgb(got,0x304050UL));}
 /* v2.27 (v10.201): lines, points, dither */
 {static const float ll[4]={20.5F,100.5F,220.5F,100.5F},ls[6]={20.5F,120.5F,120.5F,120.5F,120.5F,170.5F},pt[2]={150.5F,150.5F};
  static const int pl[]={120,100,120,99,120,101},pb[]={120,110},ps1[]={70,120,70,119,70,121},ps2[]={120,150,119,150,121,150},pp[]={150,150,149,149,150,149,149,150};
  DWORD g1;
  rs_set(D3DRENDERSTATE_SHADEMODE,D3DSHADE_FLAT);
  if(clear(D3DCLEAR_TARGET,dst,1)&&prim_draw(D3DPT_LINELIST,ll,2,0xFFFFFFFFUL))check("   line list: a white horizontal line at y 100",any_near(pl,3,0xFFFFFFUL)&&one_point(120,110,&g1)&&near_rgb(g1,dst&0xFFFFFFUL));else check("   line list",0);
  if(clear(D3DCLEAR_TARGET,dst,1)&&prim_draw(D3DPT_LINESTRIP,ls,3,0xFF00FF00UL))check("   line strip: both segments green",any_near(ps1,3,0x00FF00UL)&&any_near(ps2,3,0x00FF00UL));else check("   line strip",0);
  if(clear(D3DCLEAR_TARGET,dst,1)&&prim_draw(D3DPT_POINTLIST,pt,1,0xFFFF00FFUL))check("   point list: a magenta point at (150,150)",any_near(pp,4,0xFF00FFUL)&&one_point(160,160,&g1)&&near_rgb(g1,dst&0xFFFFFFUL));else check("   point list",0);
  (void)pb;
  /* v2.29 (v10.203): eight magenta points; the whole target is scanned for magenta (8 to 32 pixels wanted) */
  {static const float p8[16]={30.5F,30.5F,60.5F,40.5F,90.5F,50.5F,120.5F,60.5F,150.5F,150.5F,180.5F,160.5F,210.5F,170.5F,240.5F,180.5F};
   DDSURFACEDESC2 sd;DWORD x,y,cnt=0,x0=999,y0=999,x1=0,y1=0;
   if(clear(D3DCLEAR_TARGET,dst,1)&&prim_draw(D3DPT_POINTLIST,p8,8,0xFFFF00FFUL)){
    memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
    if(note("   Lock(readback)",IDirectDrawSurface7_Lock(rt,0,&sd,DDLOCK_WAIT|DDLOCK_READONLY,0))){
     for(y=0;y<sd.dwHeight;y++)for(x=0;x<sd.dwWidth;x++)if(near_rgb(rgb(&sd,x,y),0xFF00FFUL)){cnt++;if(x<x0)x0=x;if(y<y0)y0=y;if(x>x1)x1=x;if(y>y1)y1=y;}
     IDirectDrawSurface7_Unlock(rt,0);
     fprintf(logfile,"   8 points: %lu magenta pixels, box (%lu,%lu)-(%lu,%lu), want about (30,30)-(240,180)\n",(unsigned long)cnt,(unsigned long)x0,(unsigned long)y0,(unsigned long)x1,(unsigned long)y1);
     check("   point list: 8 points drawn, 8 to 32 pixels",cnt>=8&&cnt<=32&&x0>=29&&x0<=31&&y1>=179&&y1<=181);}
    else check("   point list scan",0);}
   else check("   point list: 8 points",0);}}
 {vx3dn_info vi;
  if(native_info(&vi)&&vi.bpp==16) {DWORD g[16];int xy[32],k2,dif;
   for(k2=0;k2<16;k2++){xy[k2*2]=80+(k2&3);xy[k2*2+1]=72+(k2>>2);}
   rs_set(D3DRENDERSTATE_DITHERENABLE,FALSE);
   if(clear(D3DCLEAR_TARGET,dst,1)&&flat_draw(.5F,0xFF7C7C7CUL)&&read_points(xy,g,16)){for(dif=0,k2=1;k2<16;k2++)if(g[k2]!=g[0])dif=1;check("   dither off: 7C7C7C is one 565 value over a 4x4 block",!dif);}
   rs_set(D3DRENDERSTATE_DITHERENABLE,TRUE);
   /* v2.31 (v10.205): dither modes (TEXDBG 0x30+n), format 89: 0 01/00 bit 0 + 53 000C0F (default; want green 7D/79, R/B 7B); 1 bit 0 +
    * 53 008C0F (R/B move too); 2 53 000C0F without bit 0 (control: flat); 3 01/00 bits 3:0 = F (XP's value) + 53 000C0F.
    * The words ring 0 wrote for the last dithered draw are logged (TEXDBG 0x34). */
   {DWORD in,o[8],gt;char vals[200]; /* v2.87 (.530): the VxD's dither probe modes removed: the default form only */
    if(clear(D3DCLEAR_TARGET,dst,1)&&flat_draw(.5F,0xFF7C7C7CUL)&&read_points(xy,g,16)){
     in=0x34;DeviceIoControl(endpoint,0x85500022UL,&in,4,o,32,&gt,0);
     for(dif=0,k2=1;k2<16;k2++)if(g[k2]!=g[0])dif=1;
     vals[0]=0;for(k2=0;k2<4;k2++)sprintf(vals+strlen(vals)," %06lX %06lX",(unsigned long)g[k2],(unsigned long)g[k2+4]);
     fprintf(logfile,"   dither: 01/52 %06lX 01/53 %06lX 01/00 %06lX; %s, rows 72/73:%s\n",(unsigned long)o[1],(unsigned long)o[2],(unsigned long)o[3],dif?"mixed":"one value",vals);
     check("   dither on: 7C7C7C mixes neighbouring 565 values over a 4x4 block",dif);}
    else check("   dither: the draw and its read",0);}
   rs_set(D3DRENDERSTATE_DITHERENABLE,FALSE);
  } else fprintf(logfile,"   dither: the target is not 16-bit, nothing to measure\n");}
 /* depth formats: near red, then far blue rejected; Z cleared to 0.5: z 0.75 rejected, z 0.25 drawn */
 for(z=1;z<=3;z++) {
  drop_device();
  sprintf(line,"   %s device",zname[z]);if(!setup_z(z)){check(line,0);continue;}
  rs_set(D3DRENDERSTATE_SHADEMODE,D3DSHADE_FLAT);rs_set(D3DRENDERSTATE_ZENABLE,TRUE);rs_set(D3DRENDERSTATE_ZWRITEENABLE,TRUE);rs_set(D3DRENDERSTATE_ZFUNC,D3DCMP_LESS);
  sprintf(line,"   %s near red rejects later far blue",zname[z]);
  if(clear(D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,dst,1)&&flat_draw(.25F,0xFFFF0000UL)&&flat_draw(.75F,0xFF0000FFUL)&&one_point(80,72,&got))check(line,got==0xFF0000UL);else check(line,0);
  sprintf(line,"   %s far blue drawn first, near red over it",zname[z]);
  if(clear(D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,dst,1)&&flat_draw(.75F,0xFF0000FFUL)&&flat_draw(.25F,0xFFFF0000UL)&&one_point(80,72,&got))check(line,got==0xFF0000UL);else check(line,0);
  sprintf(line,"   %s Z cleared to 0.5: z 0.75 rejected",zname[z]);
  if(clear(D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,dst,.5F)&&flat_draw(.75F,0xFF0000FFUL)&&one_point(80,72,&got))check(line,near_rgb(got,dst&0xFFFFFFUL));else check(line,0);
  sprintf(line,"   %s Z cleared to 0.5: z 0.25 drawn",zname[z]);
  if(flat_draw(.25F,0xFFFF0000UL)&&one_point(80,72,&got))check(line,got==0xFF0000UL);else check(line,0);
  t0=GetTickCount();for(k=0;k<200;k++)IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_ZBUFFER,0,k&1?.5F:1.0F,0);
  one_point(10,10,&got);t1=GetTickCount(); /* v2.7: the last clear is 0.5; the readback makes the runtime submit the clears before the time is taken */
  fprintf(logfile,"   200 %s Z clears of 256x192: %lu ms (%lu us each)\n",zname[z],(unsigned long)(t1-t0),(unsigned long)((t1-t0)*5));
  sprintf(line,"   %s after 200 Z clears (last 0.5): z 0.75 rejected",zname[z]);
  if(clear(D3DCLEAR_TARGET,dst,1)&&flat_draw(.75F,0xFF0000FFUL)&&one_point(80,72,&got))check(line,near_rgb(got,dst&0xFFFFFFUL));else check(line,0);
  /* v2.27 (v10.201): ZBIAS: blue at the same depth as red loses under LESS, and wins with ZBIAS 1 */
  sprintf(line,"   %s equal depth, LESS: red stays; ZBIAS 1: blue wins",zname[z]);
  if(clear(D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,dst,1)&&flat_draw(.5F,0xFFFF0000UL)&&flat_draw(.5F,0xFF0000FFUL)&&one_point(80,72,&got)){DWORD g2=0;
   rs_set(D3DRENDERSTATE_ZBIAS,1);if(flat_draw(.5F,0xFF0000FFUL))one_point(80,72,&g2);rs_set(D3DRENDERSTATE_ZBIAS,0);
   fprintf(logfile,"   without ZBIAS %06lX, with ZBIAS 1 %06lX\n",(unsigned long)got,(unsigned long)g2);check(line,got==0xFF0000UL&&g2==0x0000FFUL);}
  else check(line,0);
  if(z==3) { /* v2.27 (v10.201): stencil: REPLACE 1 inside the triangle, then EQUAL 1 over the whole target; INCR, then EQUAL 2 */
   DWORD in=0,out=0;
   rs_set(D3DRENDERSTATE_ZFUNC,D3DCMP_ALWAYS);rs_set(D3DRENDERSTATE_STENCILENABLE,TRUE);rs_set(D3DRENDERSTATE_STENCILMASK,0xFF);rs_set(D3DRENDERSTATE_STENCILWRITEMASK,0xFF);
   rs_set(D3DRENDERSTATE_STENCILFAIL,D3DSTENCILOP_KEEP);rs_set(D3DRENDERSTATE_STENCILZFAIL,D3DSTENCILOP_KEEP);
   if(note("   Clear(target, Z, stencil 0)",IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,dst,1,0))) {
    rs_set(D3DRENDERSTATE_STENCILFUNC,D3DCMP_ALWAYS);rs_set(D3DRENDERSTATE_STENCILREF,1);rs_set(D3DRENDERSTATE_STENCILPASS,D3DSTENCILOP_REPLACE);
    flat_draw(.5F,0xFFFF0000UL);
    rs_set(D3DRENDERSTATE_STENCILFUNC,D3DCMP_EQUAL);rs_set(D3DRENDERSTATE_STENCILPASS,D3DSTENCILOP_KEEP);
    if(full_draw(.5F,0xFF00FF00UL)&&one_point(80,72,&in)&&one_point(200,150,&out))check("   stencil REPLACE 1, then EQUAL 1: green inside, clear colour outside",in==0x00FF00UL&&near_rgb(out,dst&0xFFFFFFUL));else check("   stencil EQUAL",0);
    rs_set(D3DRENDERSTATE_STENCILPASS,D3DSTENCILOP_INCR);full_draw(.5F,0xFFFFFF00UL);
    rs_set(D3DRENDERSTATE_STENCILREF,2);rs_set(D3DRENDERSTATE_STENCILPASS,D3DSTENCILOP_KEEP);
    if(full_draw(.5F,0xFF0000FFUL)&&one_point(80,72,&in)&&one_point(200,150,&out))check("   stencil INCR, then EQUAL 2: blue inside, clear colour outside",in==0x0000FFUL&&near_rgb(out,dst&0xFFFFFFUL));else check("   stencil INCR",0);
    rs_set(D3DRENDERSTATE_STENCILFUNC,D3DCMP_NOTEQUAL);rs_set(D3DRENDERSTATE_STENCILREF,2);
    if(full_draw(.5F,0xFFFF00FFUL)&&one_point(80,72,&in)&&one_point(200,150,&out))check("   stencil NOTEQUAL 2: magenta outside only",in==0x0000FFUL&&out==0xFF00FFUL);else check("   stencil NOTEQUAL",0);
   } else check("   stencil clear",0);
   rs_set(D3DRENDERSTATE_STENCILENABLE,FALSE);rs_set(D3DRENDERSTATE_ZFUNC,D3DCMP_LESS);
  }
  if(z==3) { /* v2.12 (v10.180): D24S8 clears keep what they do not clear (Z-only keeps the stencil byte, stencil-only keeps Z) */
   static const struct {DWORD flags;float zv;DWORD sv,want;const char *name;} sc[]={
    {D3DCLEAR_ZBUFFER,1.0F,0,0xFFFFFF6BUL,"Z-only clear to 1.0 keeps stencil 6B"},{D3DCLEAR_STENCIL,0,0x5A,0x1122335AUL,"stencil-only clear to 5A keeps Z"},
    {D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,.5F,0x33,0x80000033UL,"Z 0.5 + stencil 33"}};
   int q;
   for(q=0;q<3;q++){DDSURFACEDESC2 sd;DWORD y,x,word=0,ok=0;HRESULT hr;
    sprintf(line,"   D24S8 %s",sc[q].name);
    memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
    if(FAILED(hr=IDirectDrawSurface7_Lock(depth,0,&sd,DDLOCK_WAIT,0))){fprintf(logfile,"   Lock(D24S8) = %08lX\n",(unsigned long)hr);check(line,0);continue;}
    for(y=0;y<192;y++)for(x=0;x<256;x++)((DWORD*)((BYTE*)sd.lpSurface+y*sd.lPitch))[x]=0x1122336BUL;
    IDirectDrawSurface7_Unlock(depth,0);
    if(!note("   Clear(depth)",IDirect3DDevice7_Clear(dev,0,0,sc[q].flags,0,sc[q].zv,sc[q].sv))){check(line,0);continue;}
    memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
    if(SUCCEEDED(IDirectDrawSurface7_Lock(depth,0,&sd,DDLOCK_WAIT|DDLOCK_READONLY,0))){word=((DWORD*)((BYTE*)sd.lpSurface+72*sd.lPitch))[80];
     ok=word==sc[q].want&&((DWORD*)sd.lpSurface)[0]==sc[q].want&&((DWORD*)((BYTE*)sd.lpSurface+191*sd.lPitch))[255]==sc[q].want;IDirectDrawSurface7_Unlock(depth,0);}
    fprintf(logfile,"   depth word at (80,72) = %08lX, want %08lX\n",(unsigned long)word,(unsigned long)sc[q].want);check(line,ok);}
  }
 }
 check("   native GPU triangle counter increased",native_info(&info)&&info.triangles-before>=40&&!info.fault);
 fflush(logfile);
}
/* v2.67 (v10.244) --stencil: the stencil through Direct3D alone (no OpenGL, no VXGL), as asked after VXGL v1.8. The D24S8 buffer is
 * filled by the CPU (Z 1.0, stencil 0; stencil 3 in the rectangle 16,100-96,180), then for each ACCEL_D3D_STENCIL_ZW (v2.68: 0-2, TEXDBG
 * B2-B4; 1 is v10.245's stencil plane, 0 v10.241's state, which draws the band) and three Z states (Z on with writes, Z on without writes, Z off; ZFUNC ALWAYS): 1 EQUAL 3 over the target, green: is the
 * CPU's stencil seen (green at 40,140, not at 200,60)? 2 REPLACE 1 with the triangle 32,32-224,32-32,160: what reached memory (Lock:
 * stencil 1 bytes, Z words changed)? 3 EQUAL 1, blue: does the engine see its own stencil (blue at 80,72, not at 200,150)? And the top
 * 36 rows of the screen (GDI) before and after: written or not. */
#define ST_ROWS 36
static HDC st_dc;static HBITMAP st_bm;static DWORD *st_bits,*st_base;static int st_w;
static void st_grab(DWORD *d) {HDC s=GetDC(0);BitBlt(st_dc,0,0,st_w,ST_ROWS,s,0,0,SRCCOPY);ReleaseDC(0,s);GdiFlush();memcpy(d,st_bits,(size_t)st_w*ST_ROWS*4);}
static long st_top(int reset) {
 static DWORD *now;long n=0;int i;
 if(!st_bm)return -1;
 if(!now)now=(DWORD*)malloc((size_t)st_w*ST_ROWS*4);if(!now)return -1;
 st_grab(now);for(i=0;i<st_w*ST_ROWS;i++)if(now[i]!=st_base[i])n++;
 if(reset)memcpy(st_base,now,(size_t)st_w*ST_ROWS*4);
 return n;
}
/* v2.68: the CPU fills the left rectangle with stencil 3 in byte k of the D24S8 word (k 0-3, the rest as Z 1.0 / stencil 0 would be:
 * FFFFFF00 with byte k replaced), k 4 the whole buffer 03030303; k 5 the plain layout (Z 1.0, stencil 0; 3 in the low byte) */
static int st_zfill_k(int k) {
 DDSURFACEDESC2 sd;DWORD x,y,w,in3;memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 if(FAILED(IDirectDrawSurface7_Lock(depth,0,&sd,DDLOCK_WAIT,0)))return 0;
 for(y=0;y<192;y++)for(x=0;x<256;x++){
  in3=x>=16&&x<96&&y>=100&&y<180;w=0xFFFFFF00UL;
  if(k==4)w=0x03030303UL;else if(in3){if(k==5)w=0xFFFFFF03UL;else w=(w&~(0xFFUL<<(8*k)))|(3UL<<(8*k));}
  ((DWORD*)((BYTE*)sd.lpSurface+y*sd.lPitch))[x]=w;}
 IDirectDrawSurface7_Unlock(depth,0);return 1;
}
static int st_zfill(void) {return st_zfill_k(5);}
static int st_zread(long *s1,long *s3,long *zchg,DWORD *w3) {
 DDSURFACEDESC2 sd;DWORD x,y,w;static const int px[3]={80,200,40},py[3]={72,150,140};int q;memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);*s1=*s3=*zchg=0;
 if(FAILED(IDirectDrawSurface7_Lock(depth,0,&sd,DDLOCK_WAIT|DDLOCK_READONLY,0)))return 0;
 for(y=0;y<192;y++)for(x=0;x<256;x++){w=((DWORD*)((BYTE*)sd.lpSurface+y*sd.lPitch))[x];if((w&255)==1)(*s1)++;if((w&255)==3)(*s3)++;if((w>>8)!=0xFFFFFFUL)(*zchg)++;}
 for(q=0;q<3;q++)w3[q]=((DWORD*)((BYTE*)sd.lpSurface+py[q]*sd.lPitch))[px[q]];
 IDirectDrawSurface7_Unlock(depth,0);return 1;
}
/* the desktop drawn again (the band of an earlier row erased), so each row's screen check starts clean */
static void st_repaint(void) {
 MSG msg;DWORD t=GetTickCount();
 RedrawWindow(GetDesktopWindow(),0,0,RDW_INVALIDATE|RDW_ERASE|RDW_ALLCHILDREN|RDW_UPDATENOW);
 while(GetTickCount()-t<150){while(PeekMessageA(&msg,0,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageA(&msg);}Sleep(10);}
}
static void stencil_tests(void) {
 static const char *zs[3]={"Z on, writes on ","Z on, writes off","Z off           "};
 DWORD o[8],in,gt=0,was=1,g1,g2,w3[3];int m,z,k,have;BITMAPINFO bi;HDC s;
 fprintf(logfile,"== stencil through Direct3D (D24S8, 256x192, the CPU's 3 in 16,100-96,180)\n");
 s=GetDC(0);st_w=GetDeviceCaps(s,HORZRES);st_dc=CreateCompatibleDC(s);memset(&bi,0,sizeof(bi));bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=st_w;
 bi.bmiHeader.biHeight=-ST_ROWS;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 st_bm=CreateDIBSection(s,&bi,DIB_RGB_COLORS,(void**)&st_bits,0,0);ReleaseDC(0,s);
 if(st_bm){SelectObject(st_dc,st_bm);st_base=(DWORD*)malloc((size_t)st_w*ST_ROWS*4);if(st_base)st_grab(st_base);else st_bm=0;}
 drop_device();if(!setup_z(3)){check("   D24S8 device",0);return;}
 rs_set(D3DRENDERSTATE_SHADEMODE,D3DSHADE_FLAT);
 fprintf(logfile,"   EQUAL 3 found where the CPU put 3 in byte 0 1 2 3 / 03030303 everywhere (x = yes)\n");
 fprintf(logfile,"   after REPLACE 1 (triangle 32,32-224,32-32,160): stencil-1 low bytes, 3 low bytes, Z changed; words at 80,72 (in), 200,150 (out), 40,140 (CPU 3)\n");
 m=2;for(z=0;z<3;z++) { /* v2.87 (.530): mode 2 only (the VxD's fixed form; its switch removed) */
  long n1,n3,zc,top;int seen1,out1;char bytes[6];
  rs_set(D3DRENDERSTATE_ZENABLE,z<2);rs_set(D3DRENDERSTATE_ZWRITEENABLE,z==0);rs_set(D3DRENDERSTATE_ZFUNC,D3DCMP_ALWAYS);
  rs_set(D3DRENDERSTATE_STENCILENABLE,TRUE);rs_set(D3DRENDERSTATE_STENCILMASK,0xFF);rs_set(D3DRENDERSTATE_STENCILWRITEMASK,0xFF);
  rs_set(D3DRENDERSTATE_STENCILFAIL,D3DSTENCILOP_KEEP);rs_set(D3DRENDERSTATE_STENCILZFAIL,D3DSTENCILOP_KEEP);
  st_repaint();st_top(1);
  for(k=0;k<5;k++){
   clear(D3DCLEAR_TARGET,0xFF406080UL,1);if(!st_zfill_k(k)){bytes[k]='?';continue;}
   rs_set(D3DRENDERSTATE_STENCILFUNC,D3DCMP_EQUAL);rs_set(D3DRENDERSTATE_STENCILREF,3);rs_set(D3DRENDERSTATE_STENCILPASS,D3DSTENCILOP_KEEP);
   g1=0;full_draw(.5F,0xFF00FF00UL);one_point(40,140,&g1);bytes[k]=g1==0x00FF00UL?'x':'.';
  }
  bytes[5]=0;
  clear(D3DCLEAR_TARGET,0xFF406080UL,1);st_zfill();
  rs_set(D3DRENDERSTATE_STENCILFUNC,D3DCMP_ALWAYS);rs_set(D3DRENDERSTATE_STENCILREF,1);rs_set(D3DRENDERSTATE_STENCILPASS,D3DSTENCILOP_REPLACE);
  flat_draw(.5F,0xFF404040UL);
  w3[0]=w3[1]=w3[2]=0;st_zread(&n1,&n3,&zc,w3);
  rs_set(D3DRENDERSTATE_STENCILFUNC,D3DCMP_EQUAL);rs_set(D3DRENDERSTATE_STENCILPASS,D3DSTENCILOP_KEEP);
  clear(D3DCLEAR_TARGET,0xFF406080UL,1);g1=g2=0;full_draw(.5F,0xFF0000FFUL);one_point(80,72,&g1);one_point(200,150,&g2);seen1=g1==0x0000FFUL;out1=g2==0x0000FFUL;
  top=st_top(0);
  fprintf(logfile,"   mode %d, %s: EQUAL 3 %s | REPLACE 1: %5ld %5ld %5ld, words %08lX %08lX %08lX | own 1 seen %s/%s | screen top %ld pixels\n",
   m,zs[z],bytes,n1,n3,zc,(unsigned long)w3[0],(unsigned long)w3[1],(unsigned long)w3[2],seen1?"yes":"no ",out1?"YES":"no ",top);
  fflush(logfile);
 }
 rs_set(D3DRENDERSTATE_STENCILENABLE,FALSE);
 fprintf(logfile,"   (want, modes 1 and 2: EQUAL 3 x...x (the HAL's Lock shows the stencil plane in byte 0); REPLACE about 12300 stencil-1 bytes, 6400 3 bytes, Z changed 0 except with Z writes, words xxxxxx01 in,\n"
  "    FFFFFF00 out, FFFFFF03 CPU; own 1 seen yes/no; screen top 0)\n");
}
/* v2.69 --splane (after v10.245): where the engine keeps each pixel's stencil byte. v10.245 put the plane 256 bytes after the D24S8
 * Z words, as XP's T52, and the band stopped (VXDX7 v2.68: screen top 0 in modes 1 and 2), but the engine and the CPU disagree on
 * where a pixel's byte is: the CPU's 3 is not seen, and the engine's 1 at 80,72 is not at byte 72 x 256 + 80. The surface's plane is
 * taken from the HAL (VX_DD_StencilPlane: from then on its Lock does not copy plane <-> Z words, so the bytes are the engine's own).
 * 1 the engine writes 5A at one pixel (REPLACE, a 1x1 quad): which bytes changed, from 64 KB before the plane to 128 KB after its
 *   start (changes outside the plane are put back). 2 the CPU writes 5A at one byte of the plane: which pixels an EQUAL 5A draw finds.
 * 3 REPLACE 5A on the triangle then EQUAL 5A at once (no Lock between, as OpenGL's scene B), and with a Lock between (as v2.68). */
#define SP_LO 0x10000UL
#define SP_HI 0x20000UL
#define SP_BYTES (256UL*192UL)
static DWORD sp_zoff,sp_soff,sp_spitch,sp_hi;static BYTE *sp_was,*sp_now;
static BYTE *sp_map(DDSURFACEDESC2 *sd) {
 memset(sd,0,sizeof(*sd));sd->dwSize=sizeof(*sd);
 if(FAILED(IDirectDrawSurface7_Lock(depth,0,sd,DDLOCK_WAIT,0)))return 0;
 return (BYTE*)sd->lpSurface+(sp_soff-sp_zoff); /* the plane's first byte */
}
static void sp_unmap(void) {IDirectDrawSurface7_Unlock(depth,0);}
static void sp_quiet_draw(const tl_vertex *v,int n,D3DPRIMITIVETYPE t) {
 IDirect3DDevice7_BeginScene(dev);IDirect3DDevice7_DrawPrimitive(dev,t,D3DFVF_TLVERTEX,(void*)v,n,0);IDirect3DDevice7_EndScene(dev);
}
static void sp_pixel(int x,int y,DWORD c) {
 tl_vertex v[4];int k;memset(v,0,sizeof(v));
 v[0].x=(float)x;v[0].y=(float)y;v[1].x=(float)x+1;v[1].y=(float)y;v[2].x=(float)x;v[2].y=(float)y+1;v[3].x=(float)x+1;v[3].y=(float)y+1;
 for(k=0;k<4;k++){v[k].z=.5F;v[k].rhw=1;v[k].color=c;}
 sp_quiet_draw(v,4,D3DPT_TRIANGLESTRIP);
}
static void sp_full(DWORD c) {
 tl_vertex v[4];int k;memset(v,0,sizeof(v));
 v[1].x=256;v[2].y=192;v[3].x=256;v[3].y=192;for(k=0;k<4;k++){v[k].z=.5F;v[k].rhw=1;v[k].color=c;}
 sp_quiet_draw(v,4,D3DPT_TRIANGLESTRIP);
}
static void sp_tri(DWORD c) {tl_vertex v[3];triangle(v,.5F,c,c,c);sp_quiet_draw(v,3,D3DPT_TRIANGLELIST);}
static void sp_state(DWORD func,DWORD ref,DWORD pass) {
 rs_set(D3DRENDERSTATE_STENCILFUNC,func);rs_set(D3DRENDERSTATE_STENCILREF,ref);rs_set(D3DRENDERSTATE_STENCILPASS,pass);
}
static int sp_zero(void) {DDSURFACEDESC2 sd;BYTE *pl=sp_map(&sd);if(!pl)return 0;memset(pl,0,SP_BYTES);sp_unmap();return 1;}
static void sp_write_probe(int x,int y) {
 DDSURFACEDESC2 sd;BYTE *pl,*b;DWORD i,n=0,shown=0;char line[600];int at=0;
 if(!(pl=sp_map(&sd)))return;
 memset(pl,0,SP_BYTES);b=pl-SP_LO;memcpy(sp_was,b,SP_LO+sp_hi);sp_unmap();
 sp_state(D3DCMP_ALWAYS,0x5A,D3DSTENCILOP_REPLACE);sp_pixel(x,y,0xFF404040UL);
 if(!(pl=sp_map(&sd)))return;
 b=pl-SP_LO;memcpy(sp_now,b,SP_LO+sp_hi);
 at=sprintf(line,"   pixel %3d,%3d (linear %5lu):",x,y,(unsigned long)(y*sp_spitch+x));
 for(i=0;i<SP_LO+sp_hi;i++)if(sp_now[i]!=sp_was[i]){long off=(long)i-(long)SP_LO;n++;
  if(shown<12&&at<520){at+=sprintf(line+at," %s%lX=%02X",off<0?"-":"+",(unsigned long)(off<0?-off:off),sp_now[i]);shown++;}
  if(off<0||off>=(long)SP_BYTES)b[i]=sp_was[i];} /* put back what is not the plane */
 sp_unmap();
 fprintf(logfile,"%s%s | %lu byte(s) changed\n",line,n?"":" nothing",(unsigned long)n);fflush(logfile);
}
static void sp_read_probe(DWORD k) {
 DDSURFACEDESC2 sd;BYTE *pl;DWORD x,y,n=0,shown=0;char line[600];int at;
 if(!(pl=sp_map(&sd)))return;
 memset(pl,0,SP_BYTES);pl[k]=0x5A;sp_unmap();
 IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF406080UL,1,0);
 sp_state(D3DCMP_EQUAL,0x5A,D3DSTENCILOP_KEEP);sp_full(0xFF00FF00UL);
 at=sprintf(line,"   byte +%5lX (linear %3lu,%3lu):",(unsigned long)k,(unsigned long)(k%sp_spitch),(unsigned long)(k/sp_spitch));
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 if(SUCCEEDED(IDirectDrawSurface7_Lock(rt,0,&sd,DDLOCK_WAIT|DDLOCK_READONLY,0))){
  for(y=0;y<192;y++)for(x=0;x<256;x++)if(rgb(&sd,x,y)==0x00FF00UL){n++;if(shown<12&&at<520){at+=sprintf(line+at," %lu,%lu",(unsigned long)x,(unsigned long)y);shown++;}}
  IDirectDrawSurface7_Unlock(rt,0);
 }
 fprintf(logfile,"%s%s | %lu pixel(s) green\n",line,n?"":" none",(unsigned long)n);fflush(logfile);
}
static void sp_order(int lock) {
 DWORD g1=0,g2=0;DDSURFACEDESC2 sd;
 sp_zero();IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF406080UL,1,0);
 sp_state(D3DCMP_ALWAYS,0x5A,D3DSTENCILOP_REPLACE);sp_tri(0xFF404040UL);
 if(lock&&sp_map(&sd))sp_unmap();
 sp_state(D3DCMP_EQUAL,0x5A,D3DSTENCILOP_KEEP);sp_full(0xFF0000FFUL);
 one_point(80,72,&g1);one_point(200,150,&g2);
 fprintf(logfile,"   REPLACE then EQUAL, %s: 80,72 %06lX (want 0000FF), 200,150 %06lX (want 406080 or near)\n",lock?"Lock between":"nothing between",(unsigned long)g1,(unsigned long)g2);
 fflush(logfile);
}
/* v2.70: the orders OpenGL's stencil scenes use (VXGL: fail with ZW 1, scene B passes with 2): a stencil clear by Direct3D's Clear
 * (the VxD's clear path), then the engine's REPLACE or the CPU's byte, then EQUAL at once */
static void sp_gl_order(int how) {
 static const char *hn[3]={"Clear(stencil), REPLACE, EQUAL","Clear(stencil), CPU 5A at 80,72, EQUAL","CPU 5A at 80,72, EQUAL (no clear)"};
 DWORD g1=0,g2=0;DDSURFACEDESC2 sd;BYTE *pl;
 IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_TARGET,0xFF406080UL,1,0);
 if(how<2)IDirect3DDevice7_Clear(dev,0,0,D3DCLEAR_STENCIL,0,1,0);else sp_zero();
 if(how==0){sp_state(D3DCMP_ALWAYS,0x5A,D3DSTENCILOP_REPLACE);sp_tri(0xFF404040UL);}
 else if((pl=sp_map(&sd))!=0){pl[VX3DN_SPLANE_AT(sp_spitch,80,72)]=0x5A;sp_unmap();}
 sp_state(D3DCMP_EQUAL,0x5A,D3DSTENCILOP_KEEP);sp_full(0xFF0000FFUL);
 one_point(80,72,&g1);one_point(200,150,&g2);
 fprintf(logfile,"   %s: 80,72 %06lX (want 0000FF), 200,150 %06lX (want 406080 or near)\n",hn[how],(unsigned long)g1,(unsigned long)g2);fflush(logfile);
}
static void splane_tests(void) {
 static const int px[][2]={{0,0},{1,0},{2,0},{3,0},{4,0},{5,0},{7,0},{8,0},{9,0},{15,0},{16,0},{17,0},{31,0},{32,0},{33,0},{63,0},{64,0},{65,0},
  {127,0},{128,0},{129,0},{255,0},{0,1},{0,2},{0,3},{0,4},{0,5},{0,7},{0,8},{0,9},{0,15},{0,16},{0,17},{0,31},{0,32},{0,33},{0,63},{0,64},
  {0,65},{0,127},{0,128},{0,191},{1,1},{3,5},{37,53},{80,72},{200,150},{255,191}};
 static const DWORD kb[]={0,1,2,3,4,5,7,8,15,16,31,32,63,64,127,128,255,256,257,511,512,1023,1024,2047,2048,4095,4096,8192,16384,32768,49151};
 DWORD (WINAPI *surf)(void*,DWORD*,DWORD*),(WINAPI *plane)(void*,DWORD*,DWORD*);DWORD in,o[8],gt=0,was=1,pitch=0;int i,have,m;
 DDSURFACEDESC2 sd;BYTE *pl;long top;BITMAPINFO bi;HDC s;
 fprintf(logfile,"== stencil plane layout (D24S8 256x192)\n");
 in=0xB7;have=endpoint!=INVALID_HANDLE_VALUE&&DeviceIoControl(endpoint,0x85500022UL,&in,4,o,32,&gt,0)&&gt==32&&o[0]==0xB7;was=o[1];
 if(!have){fprintf(logfile,"   the VxD has no ACCEL_D3D_STENCIL_ZW switch\n");failures++;return;}
 surf=halmod?(DWORD (WINAPI *)(void*,DWORD*,DWORD*))GetProcAddress(halmod,"VX_DD_Surface"):0;
 plane=halmod?(DWORD (WINAPI *)(void*,DWORD*,DWORD*))GetProcAddress(halmod,"VX_DD_StencilPlane"):0;
 if(!surf||!plane){fprintf(logfile,"   VXDDRAW.DLL has no VX_DD_Surface / VX_DD_StencilPlane (v10.245 or later)\n");failures++;return;}
 drop_device();if(!setup_z(3)){check("   D24S8 device",0);return;}
 if(!surf(depth,&sp_zoff,&pitch)||!plane(depth,&sp_soff,&sp_spitch)){fprintf(logfile,"   the HAL gave no offset / plane for the depth buffer\n");failures++;return;}
 fprintf(logfile,"   Z words at %08lX pitch %lu, plane at %08lX pitch %lu (+%lX after the Z words)\n",(unsigned long)sp_zoff,(unsigned long)pitch,
  (unsigned long)sp_soff,(unsigned long)sp_spitch,(unsigned long)(sp_soff-sp_zoff));
 sp_was=(BYTE*)malloc(SP_LO+SP_HI);sp_now=(BYTE*)malloc(SP_LO+SP_HI);if(!sp_was||!sp_now){failures++;return;}
 sp_hi=SP_HI;
 if(!(pl=sp_map(&sd)))return;
 if(IsBadReadPtr(pl-SP_LO,SP_LO+sp_hi)||IsBadWritePtr(pl-SP_LO,SP_LO+sp_hi))sp_hi=SP_BYTES;
 if(IsBadReadPtr(pl-SP_LO,SP_LO+sp_hi)){sp_unmap();fprintf(logfile,"   the lock does not map the plane\n");failures++;return;}
 sp_unmap();
 fprintf(logfile,"   scanned: %lX bytes before the plane to %lX after its start\n",(unsigned long)SP_LO,(unsigned long)sp_hi);
 s=GetDC(0);st_w=GetDeviceCaps(s,HORZRES);st_dc=CreateCompatibleDC(s);memset(&bi,0,sizeof(bi));bi.bmiHeader.biSize=sizeof(bi.bmiHeader);bi.bmiHeader.biWidth=st_w;
 bi.bmiHeader.biHeight=-ST_ROWS;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
 st_bm=CreateDIBSection(s,&bi,DIB_RGB_COLORS,(void**)&st_bits,0,0);ReleaseDC(0,s);
 if(st_bm){SelectObject(st_dc,st_bm);st_base=(DWORD*)malloc((size_t)st_w*ST_ROWS*4);if(st_base)st_grab(st_base);else st_bm=0;}
 rs_set(D3DRENDERSTATE_SHADEMODE,D3DSHADE_FLAT);rs_set(D3DRENDERSTATE_ZENABLE,FALSE);rs_set(D3DRENDERSTATE_ZWRITEENABLE,FALSE);
 rs_set(D3DRENDERSTATE_STENCILENABLE,TRUE);rs_set(D3DRENDERSTATE_STENCILMASK,0xFF);rs_set(D3DRENDERSTATE_STENCILWRITEMASK,0xFF);
 rs_set(D3DRENDERSTATE_STENCILFAIL,D3DSTENCILOP_KEEP);rs_set(D3DRENDERSTATE_STENCILZFAIL,D3DSTENCILOP_KEEP);
 m=2;{ /* v2.87 (.530): the VxD's fixed form only (ACCEL_D3D_STENCIL_ZW removed) */
  fprintf(logfile,"-- the stencil plane (form %d)\n",m);
  fprintf(logfile,"   1: the engine writes 5A at one pixel; bytes changed, as +offset from the plane's start (-: before it) = new value\n");
  for(i=0;i<(int)(sizeof(px)/sizeof(px[0]));i++){if(m==2&&i!=0&&i!=1&&i!=8&&i!=23&&i!=44)continue;sp_write_probe(px[i][0],px[i][1]);}
  fprintf(logfile,"   2: the CPU writes 5A at one byte of the plane; pixels an EQUAL 5A draw finds\n");
  for(i=0;i<(int)(sizeof(kb)/sizeof(kb[0]));i++){if(m==2&&i>6)continue;sp_read_probe(kb[i]);}
  fprintf(logfile,"   3: the engine's own stencil\n");sp_order(0);sp_order(1);
  fprintf(logfile,"   4: OpenGL's orders\n");sp_gl_order(0);sp_gl_order(1);sp_gl_order(2);
 }
 rs_set(D3DRENDERSTATE_STENCILENABLE,FALSE);
 top=st_top(0);fprintf(logfile,"   screen top (rows 0-%d) changed: %ld pixels\n",ST_ROWS-1,top);
 drop_device();
}
/* v2.8 --enum: what a DirectX 5/6-era application sees (WinTune 98, v10.177: "Could not find any compatible devices", then a crash).
 * For the desktop and for full-screen 640x480 at 8, 16 and 32 bpp: IDirect3D3 and IDirect3D2 EnumDevices with the full HAL/HEL
 * descriptions, and IDirect3D3 EnumZBufferFormats for the hardware device. */
static void desc_line(const char *who,LPD3DDEVICEDESC d) {
 if(!d||!d->dwFlags){fprintf(logfile,"     %s: (none)\n",who);return;}
 fprintf(logfile,"     %s: size %lu flags %08lX colour model %lu devcaps %08lX render %08lX Z %08lX maxbuf %lu maxvtx %lu clipping %ld\n",who,(unsigned long)d->dwSize,(unsigned long)d->dwFlags,
  (unsigned long)d->dcmColorModel,(unsigned long)d->dwDevCaps,(unsigned long)d->dwDeviceRenderBitDepth,(unsigned long)d->dwDeviceZBufferBitDepth,(unsigned long)d->dwMaxBufferSize,(unsigned long)d->dwMaxVertexCount,(long)d->bClipping);
 fprintf(logfile,"       tri: misc %08lX raster %08lX zcmp %08lX src %08lX dst %08lX acmp %08lX shade %08lX tex %08lX filter %08lX blend %08lX address %08lX\n",
  (unsigned long)d->dpcTriCaps.dwMiscCaps,(unsigned long)d->dpcTriCaps.dwRasterCaps,(unsigned long)d->dpcTriCaps.dwZCmpCaps,(unsigned long)d->dpcTriCaps.dwSrcBlendCaps,(unsigned long)d->dpcTriCaps.dwDestBlendCaps,
  (unsigned long)d->dpcTriCaps.dwAlphaCmpCaps,(unsigned long)d->dpcTriCaps.dwShadeCaps,(unsigned long)d->dpcTriCaps.dwTextureCaps,(unsigned long)d->dpcTriCaps.dwTextureFilterCaps,(unsigned long)d->dpcTriCaps.dwTextureBlendCaps,(unsigned long)d->dpcTriCaps.dwTextureAddressCaps);
 fprintf(logfile,"       line: misc %08lX raster %08lX zcmp %08lX shade %08lX tex %08lX; transform %08lX lighting %08lX\n",(unsigned long)d->dpcLineCaps.dwMiscCaps,(unsigned long)d->dpcLineCaps.dwRasterCaps,
  (unsigned long)d->dpcLineCaps.dwZCmpCaps,(unsigned long)d->dpcLineCaps.dwShadeCaps,(unsigned long)d->dpcLineCaps.dwTextureCaps,(unsigned long)d->dtcTransformCaps.dwCaps,(unsigned long)d->dlcLightingCaps.dwCaps);
 if(d->dwSize>=200)fprintf(logfile,"       textures %lux%lu .. %lux%lu\n",(unsigned long)d->dwMinTextureWidth,(unsigned long)d->dwMinTextureHeight,(unsigned long)d->dwMaxTextureWidth,(unsigned long)d->dwMaxTextureHeight);
}
static HRESULT CALLBACK legacy_dev(GUID *g,LPSTR desc,LPSTR name,LPD3DDEVICEDESC hal,LPD3DDEVICEDESC hel,LPVOID tag) {
 (void)g;fprintf(logfile,"   %s device \"%s\" / \"%s\"\n",(const char*)tag,name?name:"",desc?desc:"");desc_line("HAL",hal);desc_line("HEL",hel);fflush(logfile);return D3DENUMRET_OK;
}
/* v2.9: Open Watcom's dxguid.lib has IID_IDirect3D2 wrong (6AAE1EC1-622A-..., the real one is 6AAE1EC1-662A-...), so v2.8's
 * IDirect3D2 query failed with E_NOINTERFACE. The real values, and IDirect3D (DirectX 3) as well. */
static const GUID iid_d3d2={0x6AAE1EC1UL,0x662A,0x11D0,{0x88,0x9D,0x00,0xAA,0x00,0xBB,0xB7,0x6A}};
static const GUID iid_d3d1={0x3BBA0080UL,0x2421,0x11CF,{0xA3,0x1A,0x00,0xAA,0x00,0xB9,0x33,0x56}};
static void enum_now(const char *when) {
 HRESULT (WINAPI *create1)(GUID *,LPDIRECTDRAW *,IUnknown *)=(HRESULT (WINAPI *)(GUID *,LPDIRECTDRAW *,IUnknown *))GetProcAddress(ddlib,"DirectDrawCreate");
 LPDIRECTDRAW d1=0;IDirect3D3 *d33=0;IDirect3D2 *d32=0;
 fprintf(logfile,"== %s\n",when);
 if(!create1||FAILED(create1(0,&d1,0))){fprintf(logfile,"   DirectDrawCreate failed\n");return;}
 if(note("   QueryInterface(IDirect3D3)",IDirectDraw_QueryInterface(d1,&IID_IDirect3D3,(void**)&d33))){
  note("   IDirect3D3::EnumDevices",IDirect3D3_EnumDevices(d33,legacy_dev,(void*)"D3D3"));
  note("   IDirect3D3::EnumZBufferFormats(HAL)",IDirect3D3_EnumZBufferFormats(d33,&IID_IDirect3DHALDevice,zfmt,0));
  IDirect3D3_Release(d33);}
 if(note("   QueryInterface(IDirect3D2)",IDirectDraw_QueryInterface(d1,&iid_d3d2,(void**)&d32))){
  note("   IDirect3D2::EnumDevices",IDirect3D2_EnumDevices(d32,legacy_dev,(void*)"D3D2"));IDirect3D2_Release(d32);}
 {IDirect3D *d31=0;
  if(note("   QueryInterface(IDirect3D)",IDirectDraw_QueryInterface(d1,&iid_d3d1,(void**)&d31))){
   note("   IDirect3D::EnumDevices",IDirect3D_EnumDevices(d31,legacy_dev,(void*)"D3D1"));IDirect3D_Release(d31);}}
 IDirectDraw_Release(d1);fflush(logfile);
}
static void enum_tests(void) {
 static const DWORD bpp[3]={8,16,32};int k;char line[80];
 enum_now("desktop mode");
 if(!note("SetCooperativeLevel(EXCLUSIVE|FULLSCREEN)",IDirectDraw7_SetCooperativeLevel(dd,window,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN)))return;
 for(k=0;k<3;k++){sprintf(line,"SetDisplayMode(640x480x%lu)",(unsigned long)bpp[k]);
  if(note(line,IDirectDraw7_SetDisplayMode(dd,640,480,bpp[k],0,0))){sprintf(line,"full-screen 640x480x%lu",(unsigned long)bpp[k]);enum_now(line);}}
 note("RestoreDisplayMode",IDirectDraw7_RestoreDisplayMode(dd));
 note("SetCooperativeLevel(NORMAL)",IDirectDraw7_SetCooperativeLevel(dd,window,DDSCL_NORMAL));
}
/* v2.10 --wintune: WinTune 98's Direct3D test (WinTunDD.ocx) is the DirectX 6 SDK framework (D3DEnum + CD3DFramework). This repeats what it
 * does, step by step with every HRESULT: the EnumNullDevice value it relies on, DirectDrawEnumerate, per driver IDirectDraw4 / IDirect3D3 /
 * GetCaps / EnumDisplayModes / EnumDevices with its acceptance rules (render depth for 16/24/32-bit modes, no 8-bit modes, desktop
 * compatible), then for every device the windowed set-up: SetCooperativeLevel(NORMAL), primary (with and without 3DDEVICE), clipper,
 * 3D back buffer, 16-bit Z buffer from EnumZBufferFormats, IDirect3D3::CreateDevice, viewport, a clear and an empty scene. */
static const GUID iid_dd4={0x9C59509AUL,0x39BD,0x11D1,{0x8C,0x4A,0x00,0xC0,0x4F,0xD9,0x30,0xC5}};
static const GUID iid_rgbdev={0xA4665C60UL,0x2673,0x11CF,{0xA3,0x1A,0x00,0xAA,0x00,0xB9,0x33,0x56}};
static const GUID iid_nulldev={0x8767DF22UL,0xBACC,0x11D1,{0x89,0x69,0x00,0xA0,0xC9,0x06,0x29,0xA8}};
typedef struct {GUID guid;int has;char name[64];} wt_drv;
typedef struct {GUID guid;char name[48];DWORD render,hw;} wt_dev;
static wt_drv wt_drivers[4];static int wt_ndrv;static wt_dev wt_devs[16];static int wt_ndev;static DWORD wt_modes[4],wt_desk;static int wt_null_seen;
static const char *gname(const GUID *g){static char b[40];sprintf(b,"%08lX-%04X",(unsigned long)g->Data1,(unsigned)g->Data2);return b;}
static BOOL WINAPI wt_drv_cb(GUID *g,LPSTR desc,LPSTR name,LPVOID unused) {
 (void)unused;fprintf(logfile,"   DirectDrawEnumerate: %s \"%s\" / \"%s\"\n",g?gname(g):"NULL (primary)",desc?desc:"",name?name:"");
 if(wt_ndrv<4){wt_drivers[wt_ndrv].has=g!=0;if(g)wt_drivers[wt_ndrv].guid=*g;sprintf(wt_drivers[wt_ndrv].name,"%.60s",desc?desc:"");wt_ndrv++;}
 return TRUE;
}
static HRESULT WINAPI wt_mode_cb(LPDDSURFACEDESC2 d,LPVOID unused) {
 DWORD b=d->ddpfPixelFormat.dwRGBBitCount;(void)unused;
 if(b==8)wt_modes[0]++;else if(b==16)wt_modes[1]++;else if(b==24)wt_modes[2]++;else if(b==32)wt_modes[3]++;
 return DDENUMRET_OK;
}
static HRESULT WINAPI wt_dev_cb(GUID *g,LPSTR desc,LPSTR name,LPD3DDEVICEDESC hal,LPD3DDEVICEDESC hel,LPVOID unused) {
 LPD3DDEVICEDESC d;DWORD hw,n=0;(void)unused;(void)desc;
 if(!g||!hal||!hel){fprintf(logfile,"     device with a NULL GUID or description (WinTune cancels the enumeration)\n");return D3DENUMRET_OK;}
 hw=hal->dwFlags!=0;d=hw?hal:hel;
 if(d->dwDeviceRenderBitDepth&DDBD_16)n+=wt_modes[1];if(d->dwDeviceRenderBitDepth&DDBD_24)n+=wt_modes[2];if(d->dwDeviceRenderBitDepth&DDBD_32)n+=wt_modes[3];
 fprintf(logfile,"     device %s \"%s\": %s, render %08lX, modes WinTune keeps %lu, desktop %lu bpp %s%s%s\n",gname(g),name?name:"",hw?"hardware":"software",
  (unsigned long)d->dwDeviceRenderBitDepth,(unsigned long)n,(unsigned long)wt_desk,
  ((wt_desk==16&&(d->dwDeviceRenderBitDepth&DDBD_16))||(wt_desk==24&&(d->dwDeviceRenderBitDepth&DDBD_24))||(wt_desk==32&&(d->dwDeviceRenderBitDepth&DDBD_32)))?"(desktop compatible)":"(NOT desktop compatible)",
  n?"":" -> REJECTED (no modes)",(name&&!strcmp(name,"Null device"))?" <- the device WinTune's Null test looks for":"");
 if(IsEqualGUID(g,&iid_nulldev)||(name&&!strcmp(name,"Null device")))wt_null_seen=1;
 if(n&&wt_ndev<16){wt_devs[wt_ndev].guid=*g;sprintf(wt_devs[wt_ndev].name,"%.40s",name?name:"");wt_devs[wt_ndev].render=d->dwDeviceRenderBitDepth;wt_devs[wt_ndev].hw=hw;wt_ndev++;}
 return D3DENUMRET_OK;
}
static DDPIXELFORMAT wt_zpf;static int wt_zfound;
static HRESULT CALLBACK wt_z_cb(LPDDPIXELFORMAT f,LPVOID unused) {
 (void)unused;fprintf(logfile,"       Z format: flags %08lX depth %lu\n",(unsigned long)f->dwFlags,(unsigned long)f->dwZBufferBitDepth);
 if(!wt_zfound&&f->dwFlags==DDPF_ZBUFFER&&f->dwZBufferBitDepth==16){wt_zpf=*f;wt_zfound=1;}
 return D3DENUMRET_OK;
}
static void wt_init(const wt_drv *drv,const wt_dev *dev,int prim3d) {
 HRESULT (WINAPI *create1)(GUID *,LPDIRECTDRAW *,IUnknown *)=(HRESULT (WINAPI *)(GUID *,LPDIRECTDRAW *,IUnknown *))GetProcAddress(ddlib,"DirectDrawCreate");
 LPDIRECTDRAW d1=0;IDirectDraw4 *d4=0;IDirect3D3 *d3=0;IDirectDrawSurface4 *front=0,*back=0,*zb=0;LPDIRECTDRAWCLIPPER clip=0;IDirect3DDevice3 *dv=0;IDirect3DViewport3 *vp=0;
 DDSURFACEDESC2 sd;D3DVIEWPORT2 v;RECT rc;DWORD w,h;HRESULT hr;
 fprintf(logfile,"   -- windowed set-up on \"%s\" (%s), primary %s\n",dev->name,dev->hw?"hardware":"software",prim3d?"PRIMARYSURFACE|3DDEVICE (no back buffer)":"PRIMARYSURFACE + 3D back buffer");
 GetClientRect(window,&rc);w=rc.right-rc.left;h=rc.bottom-rc.top;if(!w||!h){w=256;h=192;}
 if(!note("     DirectDrawCreate(driver)",create1(drv->has?(GUID*)&drv->guid:0,&d1,0)))return;
 if(!note("     QueryInterface(IDirectDraw4)",IDirectDraw_QueryInterface(d1,&iid_dd4,(void**)&d4)))goto out;
 if(!note("     SetCooperativeLevel(NORMAL)",IDirectDraw4_SetCooperativeLevel(d4,window,DDSCL_NORMAL)))goto out;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS;sd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|(prim3d?DDSCAPS_3DDEVICE:0);
 if(!note(prim3d?"     CreateSurface(PRIMARYSURFACE|3DDEVICE)":"     CreateSurface(PRIMARYSURFACE)",IDirectDraw4_CreateSurface(d4,&sd,&front,0)))goto out;
 if(!note("     CreateClipper",IDirectDraw4_CreateClipper(d4,0,&clip,0)))goto out;
 note("     Clipper SetHWnd",IDirectDrawClipper_SetHWnd(clip,0,window));note("     SetClipper",IDirectDrawSurface4_SetClipper(front,clip));
 if(prim3d){back=front;IDirectDrawSurface4_AddRef(back);}
 else {memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;sd.dwWidth=w;sd.dwHeight=h;sd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE;
  fprintf(logfile,"     back buffer %lux%lu\n",(unsigned long)w,(unsigned long)h);
  if(!note("     CreateSurface(OFFSCREENPLAIN|3DDEVICE)",IDirectDraw4_CreateSurface(d4,&sd,&back,0)))goto out;}
 if(!note("     QueryInterface(IDirect3D3)",IDirectDraw4_QueryInterface(d4,&IID_IDirect3D3,(void**)&d3)))goto out;
 wt_zfound=0;note("     EnumZBufferFormats",IDirect3D3_EnumZBufferFormats(d3,&dev->guid,wt_z_cb,0));
 if(!wt_zfound)fprintf(logfile,"     no 16-bit Z format: WinTune says \"No ZBuffer\"\n");
 else {memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;sd.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|(dev->hw?DDSCAPS_VIDEOMEMORY:DDSCAPS_SYSTEMMEMORY);
  if(prim3d){DDSURFACEDESC2 q;memset(&q,0,sizeof(q));q.dwSize=sizeof(q);IDirectDrawSurface4_GetSurfaceDesc(back,&q);sd.dwWidth=q.dwWidth;sd.dwHeight=q.dwHeight;}else{sd.dwWidth=w;sd.dwHeight=h;}
  sd.ddpfPixelFormat=wt_zpf;
  if(note("     CreateSurface(ZBUFFER 16)",IDirectDraw4_CreateSurface(d4,&sd,&zb,0)))note("     AddAttachedSurface(Z)",IDirectDrawSurface4_AddAttachedSurface(back,zb));}
 if(!note("     IDirect3D3::CreateDevice",IDirect3D3_CreateDevice(d3,&dev->guid,back,&dv,0)))goto out;
 if(!note("     CreateViewport",IDirect3D3_CreateViewport(d3,&vp,0)))goto out;
 note("     AddViewport",IDirect3DDevice3_AddViewport(dv,vp));
 memset(&v,0,sizeof(v));v.dwSize=sizeof(v);v.dwWidth=prim3d?256:w;v.dwHeight=prim3d?192:h;v.dvClipX=-1;v.dvClipY=1;v.dvClipWidth=2;v.dvClipHeight=2;v.dvMaxZ=1;
 note("     SetViewport2",IDirect3DViewport3_SetViewport2(vp,&v));note("     SetCurrentViewport",IDirect3DDevice3_SetCurrentViewport(dv,vp));
 {D3DRECT r;r.x1=0;r.y1=0;r.x2=v.dwWidth;r.y2=v.dwHeight;note("     Viewport Clear2",IDirect3DViewport3_Clear2(vp,1,&r,D3DCLEAR_TARGET|(zb?D3DCLEAR_ZBUFFER:0),0xFF204060UL,1,0));}
 hr=IDirect3DDevice3_BeginScene(dv);note("     BeginScene",hr);if(SUCCEEDED(hr))note("     EndScene",IDirect3DDevice3_EndScene(dv));
out:
 if(vp)IDirect3DViewport3_Release(vp);if(dv)IDirect3DDevice3_Release(dv);if(zb)IDirectDrawSurface4_Release(zb);if(back)IDirectDrawSurface4_Release(back);
 if(clip)IDirectDrawClipper_Release(clip);if(front)IDirectDrawSurface4_Release(front);if(d3)IDirect3D3_Release(d3);if(d4)IDirectDraw4_Release(d4);if(d1)IDirectDraw_Release(d1);
 fflush(logfile);
}
static void wintune_tests(void) {
 HRESULT (WINAPI *create1)(GUID *,LPDIRECTDRAW *,IUnknown *)=(HRESULT (WINAPI *)(GUID *,LPDIRECTDRAW *,IUnknown *))GetProcAddress(ddlib,"DirectDrawCreate");
 HRESULT (WINAPI *ddenum)(LPDDENUMCALLBACKA,LPVOID)=(HRESULT (WINAPI *)(LPDDENUMCALLBACKA,LPVOID))GetProcAddress(ddlib,"DirectDrawEnumerateA");
 HKEY k;DWORD val=0,sz=4,type=0;LONG r=1;int i,j;HMODULE adv=LoadLibraryA("ADVAPI32.DLL"); /* loaded here: the test programs import only KERNEL32, USER32, GDI32 */
 LONG (WINAPI *rok)(HKEY,LPCSTR,DWORD,REGSAM,PHKEY)=adv?(LONG (WINAPI *)(HKEY,LPCSTR,DWORD,REGSAM,PHKEY))GetProcAddress(adv,"RegOpenKeyExA"):0;
 LONG (WINAPI *rqv)(HKEY,LPCSTR,LPDWORD,LPDWORD,LPBYTE,LPDWORD)=adv?(LONG (WINAPI *)(HKEY,LPCSTR,LPDWORD,LPDWORD,LPBYTE,LPDWORD))GetProcAddress(adv,"RegQueryValueExA"):0;
 LONG (WINAPI *rck)(HKEY)=adv?(LONG (WINAPI *)(HKEY))GetProcAddress(adv,"RegCloseKey"):0;
 fprintf(logfile,"== WinTune 98 Direct3D test, repeated step by step\n");
 if(rok&&rqv&&rck)r=rok(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\Direct3D\\Drivers",0,KEY_READ,&k);
 if(r){fprintf(logfile,"   HKLM\\Software\\Microsoft\\Direct3D\\Drivers missing (%ld): WinTune stops with E_FAIL\n",(long)r);}
 else {r=rqv(k,"EnumNullDevice",0,&type,(BYTE*)&val,&sz);
  if(r)fprintf(logfile,"   EnumNullDevice not present (WinTune creates it as 1)\n");
  else fprintf(logfile,"   EnumNullDevice present: type %lu, value %lu (WinTune leaves an existing value as it is)\n",(unsigned long)type,(unsigned long)val);
  rck(k);}
 if(rok&&rqv&&rck&&!rok(HKEY_LOCAL_MACHINE,"Software\\Microsoft\\DirectX",0,KEY_READ,&k)){char ver[64];DWORD vs=sizeof(ver)-1;memset(ver,0,sizeof(ver));
  if(!rqv(k,"Version",0,&type,(BYTE*)ver,&vs))fprintf(logfile,"   DirectX runtime version (HKLM\\Software\\Microsoft\\DirectX Version): %s\n",ver);rck(k);}
 {HMODULE im=LoadLibraryA("D3DIM.DLL");fprintf(logfile,"   D3DIM.DLL (the DirectX 6 Direct3D that IDirect3D3 uses) %s\n",im?"loads":"does NOT load");if(im)FreeLibrary(im);}
 if(!ddenum||!create1){fprintf(logfile,"   DirectDrawEnumerateA / DirectDrawCreate missing\n");return;}
 wt_ndrv=0;note("   DirectDrawEnumerateA",ddenum(wt_drv_cb,0)); /* v2.11: it returns an HRESULT (v2.10 read it as a BOOL and logged E_FAIL) */
 for(i=0;i<wt_ndrv;i++) {
  LPDIRECTDRAW d1=0;IDirectDraw4 *d4=0;IDirect3D3 *d3=0;DDCAPS hw,hel;DDSURFACEDESC2 m;
  fprintf(logfile,"   driver %d \"%s\"\n",i,wt_drivers[i].name);
  if(!note("     DirectDrawCreate",create1(wt_drivers[i].has?&wt_drivers[i].guid:0,&d1,0)))continue;
  if(note("     QueryInterface(IDirectDraw4)",IDirectDraw_QueryInterface(d1,&iid_dd4,(void**)&d4))&&
     note("     QueryInterface(IDirect3D3)",IDirectDraw4_QueryInterface(d4,&IID_IDirect3D3,(void**)&d3))) {
   memset(&hw,0,sizeof(hw));hw.dwSize=sizeof(hw);memset(&hel,0,sizeof(hel));hel.dwSize=sizeof(hel);
   if(note("     GetCaps",IDirectDraw4_GetCaps(d4,&hw,&hel)))fprintf(logfile,"     caps %08lX caps2 %08lX (CANRENDERWINDOWED %s)\n",(unsigned long)hw.dwCaps,(unsigned long)hw.dwCaps2,(hw.dwCaps2&DDCAPS2_CANRENDERWINDOWED)?"yes":"NO");
   memset(wt_modes,0,sizeof(wt_modes));note("     EnumDisplayModes",IDirectDraw4_EnumDisplayModes(d4,0,0,0,wt_mode_cb));
   fprintf(logfile,"     modes: 8 bpp %lu, 16 bpp %lu, 24 bpp %lu, 32 bpp %lu\n",(unsigned long)wt_modes[0],(unsigned long)wt_modes[1],(unsigned long)wt_modes[2],(unsigned long)wt_modes[3]);
   memset(&m,0,sizeof(m));m.dwSize=sizeof(m);if(note("     GetDisplayMode",IDirectDraw4_GetDisplayMode(d4,&m)))wt_desk=m.ddpfPixelFormat.dwRGBBitCount;
   wt_ndev=0;wt_null_seen=0;note("     IDirect3D3::EnumDevices",IDirect3D3_EnumDevices(d3,wt_dev_cb,0));
   /* v2.13: WinTune's Null test (DiscoNull, WinTunDD.ocx) picks the device whose NAME is "Null device" (it never uses the GUID; a
    * device named otherwise is not chosen, there is no fallback): report it, and whether the runtime can create the Null device at all */
   fprintf(logfile,"     Null device enumerated: %s\n",wt_null_seen?"YES":"NO");
   {DDSURFACEDESC2 sd;IDirectDrawSurface4 *ns=0;IDirect3DDevice3 *nd=0;
    if(note("     SetCooperativeLevel(NORMAL)",IDirectDraw4_SetCooperativeLevel(d4,window,DDSCL_NORMAL))){
     memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;sd.dwWidth=64;sd.dwHeight=64;sd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE|DDSCAPS_SYSTEMMEMORY;
     if(note("     CreateSurface(64x64 3DDEVICE system memory)",IDirectDraw4_CreateSurface(d4,&sd,&ns,0))){
      note("     Null device CreateDevice(IID_IDirect3DNullDevice)",IDirect3D3_CreateDevice(d3,&iid_nulldev,ns,&nd,0));
      if(nd)IDirect3DDevice3_Release(nd);IDirectDrawSurface4_Release(ns);}}}
   for(j=0;j<wt_ndev;j++){if(IsEqualGUID(&wt_devs[j].guid,&iid_nulldev))continue;wt_init(&wt_drivers[i],&wt_devs[j],0);wt_init(&wt_drivers[i],&wt_devs[j],1);}
  }
  if(d3)IDirect3D3_Release(d3);if(d4)IDirectDraw4_Release(d4);IDirectDraw_Release(d1);
 }
 (void)iid_rgbdev;fflush(logfile);
}
/* v2.17 --tunnel: the DirectX 5 SDK's Tunnel ("D3D Example", d3dapp framework) on the DirectX 3 interfaces, step by step with every HRESULT:
 * DirectDrawCreate, IDirect3D, a 3D back buffer and a 16-bit Z buffer, the device by QueryInterface on the back buffer with the HAL GUID,
 * EnumTextureFormats with Tunnel's choice (8-bit palettized if offered, else the last one listed without alpha), four textures as Tunnel loads them (a system-memory
 * copy, a TEXTURE|ALLOCONLOAD destination, IDirect3DTexture::Load, GetHandle), its SetRenderState execute buffer (14 render states, 3 light
 * states), then cleared frames with one textured quad per texture, read back. --systemmemory does what Tunnel -systemmemory does (system
 * memory surfaces, the RGB device). --modes runs the same set-up full screen in a list of display modes, released and rebuilt between them as
 * d3dapp does on a mode change (640x480x16 -> 1024x768x32 crashed the board, v10.184); every log line is written to disk before the next step. */
static const GUID iid_haldev={0x84E63DE0UL,0x46AA,0x11CF,{0x81,0x6F,0x00,0x00,0xC0,0x20,0x15,0x6E}};
static const GUID iid_rgbdev1={0xA4665C60UL,0x2673,0x11CF,{0xA3,0x1A,0x00,0xAA,0x00,0xB9,0x33,0x56}};
static const GUID iid_tex1={0x2CDCD9E0UL,0x25A0,0x11CF,{0xA3,0x1A,0x00,0xAA,0x00,0xB9,0x33,0x56}};
static int commit_lines;
static void commit(void) {fflush(logfile);if(commit_lines)_commit(fileno(logfile));}
static int quiet_ok; /* repeated frames: only failures are logged */
static int cnote(const char *s,HRESULT hr) {int r;if(quiet_ok&&SUCCEEDED(hr))return 1;r=note(s,hr);commit();return r;}
typedef struct tn_rig {
 LPDIRECTDRAW d1;LPDIRECTDRAWSURFACE front,back,zb,tex[4];LPDIRECTDRAWCLIPPER clip;LPDIRECT3D d3;LPDIRECT3DDEVICE dv;LPDIRECT3DVIEWPORT vp;
 D3DTEXTUREHANDLE th[4];DWORD w,h;int sys,full,ntex;DDSURFACEDESC fmt;
} tn_rig;
static DDSURFACEDESC tn_fmts[16];static int tn_nfmt;
static HRESULT CALLBACK tn_fmt_cb(LPDDSURFACEDESC d,LPVOID unused) {
 const DDPIXELFORMAT *f=&d->ddpfPixelFormat;(void)unused;
 fprintf(logfile,"       texture format: flags %08lX %lu-bit R %08lX G %08lX B %08lX A %08lX%s\n",(unsigned long)f->dwFlags,(unsigned long)f->dwRGBBitCount,(unsigned long)f->dwRBitMask,
  (unsigned long)f->dwGBitMask,(unsigned long)f->dwBBitMask,(unsigned long)f->dwRGBAlphaBitMask,(f->dwFlags&DDPF_ALPHAPIXELS)?" (Tunnel skips formats with alpha)":"");
 if(!(f->dwFlags&(DDPF_ALPHAPIXELS|DDPF_PALETTEINDEXED4|DDPF_PALETTEINDEXED2|DDPF_PALETTEINDEXED1))&&tn_nfmt<16)tn_fmts[tn_nfmt++]=*d;
 return DDENUMRET_OK;
}
static DWORD mpack(DWORD mask,DWORD v8) {
 DWORD sh=0,w=0;if(!mask)return 0;
 while(!(mask&1)){mask>>=1;sh++;}while(mask&1){mask>>=1;w++;}
 if(w>8)w=8;return ((v8>>(8-w))&((1UL<<w)-1))<<sh;
}
static DWORD munpack(DWORD mask,DWORD px) {
 DWORD sh=0,w=0,v;if(!mask)return 0;
 while(!(mask&1)){mask>>=1;sh++;}while(mask&1){mask>>=1;w++;}
 v=(px>>sh)&((1UL<<w)-1);return w>=8?(v>>(w-8)):(v<<(8-w))|(v>>(2*w-8>0?2*w-8:0));
}
static DWORD pf_pack(const DDPIXELFORMAT *f,DWORD rgb) {return mpack(f->dwRBitMask,(rgb>>16)&255)|mpack(f->dwGBitMask,(rgb>>8)&255)|mpack(f->dwBBitMask,rgb&255);}
static DWORD pf_rgb(const DDPIXELFORMAT *f,DWORD px) {return munpack(f->dwRBitMask,px)<<16|munpack(f->dwGBitMask,px)<<8|munpack(f->dwBBitMask,px);}
static const DWORD tn_col[4][4]={{0xFF0000UL,0x00FF00UL,0x0000FFUL,0xFFFFFFUL},{0xFFFF00UL,0x00FFFFUL,0xFF00FFUL,0x808080UL},
 {0xFF8000UL,0x0080FFUL,0x80FF00UL,0xFF0080UL},{0x400000UL,0x004000UL,0x000040UL,0xC0C0C0UL}};
static void surf_line(const char *who,LPDIRECTDRAWSURFACE s) {
 LPDDRAWI_DDRAWSURFACE_INT i=(LPDDRAWI_DDRAWSURFACE_INT)s;
 if(!i||IsBadReadPtr(i,sizeof(*i))||!i->lpLcl||IsBadReadPtr(i->lpLcl,sizeof(*i->lpLcl))||!i->lpLcl->lpGbl){fprintf(logfile,"       %s: object unreadable\n",who);return;}
 fprintf(logfile,"       %s: caps %08lX fpVidMem %08lX pitch %ld size %ux%u handle %lu\n",who,(unsigned long)i->lpLcl->ddsCaps.dwCaps,(unsigned long)i->lpLcl->lpGbl->fpVidMem,
  (long)i->lpLcl->lpGbl->lPitch,(unsigned)i->lpLcl->lpGbl->wWidth,(unsigned)i->lpLcl->lpGbl->wHeight,
  (unsigned long)((i->lpLcl->lpSurfMore&&!IsBadReadPtr(i->lpLcl->lpSurfMore,sizeof(*i->lpLcl->lpSurfMore)))?i->lpLcl->lpSurfMore->dwSurfaceHandle:0));
}
static int tn_load(tn_rig *r,int k,DWORD w,DWORD h) {
 DDSURFACEDESC sd;LPDIRECTDRAWSURFACE src=0,dst=0;LPDIRECT3DTEXTURE ts=0,td=0;LPDIRECTDRAWPALETTE pal=0;char line[128];int ok=0;DWORD x,y,bits;
 int pal8=(r->fmt.ddpfPixelFormat.dwFlags&DDPF_PALETTEINDEXED8)!=0;
 fprintf(logfile,"     texture %d, %lux%lu\n",k,(unsigned long)w,(unsigned long)h);
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;sd.dwWidth=w;sd.dwHeight=h;
 sd.ddpfPixelFormat=r->fmt.ddpfPixelFormat;sd.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_SYSTEMMEMORY;
 if(!cnote("       CreateSurface(source, TEXTURE|SYSTEMMEMORY)",IDirectDraw_CreateSurface(r->d1,&sd,&src,0)))goto out;
 if(pal8) { /* v2.18: an 8-bit palettized texture (the RGB device offers one and Tunnel prefers it): palette entries 0-3 are the quadrant colours */
  PALETTEENTRY pe[256];int j;memset(pe,0,sizeof(pe));
  for(j=0;j<4;j++){pe[j].peRed=(BYTE)(tn_col[k][j]>>16);pe[j].peGreen=(BYTE)(tn_col[k][j]>>8);pe[j].peBlue=(BYTE)tn_col[k][j];}
  if(!cnote("       CreatePalette(8-bit)",IDirectDraw_CreatePalette(r->d1,DDPCAPS_8BIT|DDPCAPS_ALLOW256,pe,&pal,0))||
     !cnote("       SetPalette(source)",IDirectDrawSurface_SetPalette(src,pal)))goto out;
 }
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 if(!cnote("       Lock(source)",IDirectDrawSurface_Lock(src,0,&sd,DDLOCK_WAIT,0)))goto out;
 bits=pal8?8:r->fmt.ddpfPixelFormat.dwRGBBitCount; /* v2.18: v2.17 wrote 32-bit texels into the 8-bit texture (past its end) */
 for(y=0;y<h;y++)for(x=0;x<w;x++) {
  DWORD q=(x>=w/2?1:0)+(y>=h/2?2:0),px=pal8?q:pf_pack(&r->fmt.ddpfPixelFormat,tn_col[k][q]);BYTE *row=(BYTE*)sd.lpSurface+y*sd.lPitch;
  if(bits==8)row[x]=(BYTE)px;else if(bits==16)((WORD*)row)[x]=(WORD)px;else if(bits==32)((DWORD*)row)[x]=px;
 }
 IDirectDrawSurface_Unlock(src,0);
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;sd.dwWidth=w;sd.dwHeight=h;
 sd.ddpfPixelFormat=r->fmt.ddpfPixelFormat;sd.ddsCaps.dwCaps=DDSCAPS_TEXTURE|DDSCAPS_ALLOCONLOAD|(r->sys?DDSCAPS_SYSTEMMEMORY:0);
 sprintf(line,"       CreateSurface(destination, TEXTURE|ALLOCONLOAD%s, as Tunnel)",r->sys?"|SYSTEMMEMORY":"");
 if(!cnote(line,IDirectDraw_CreateSurface(r->d1,&sd,&dst,0)))goto out;
 surf_line("destination after CreateSurface",dst);
 if(pal8&&!cnote("       SetPalette(destination)",IDirectDrawSurface_SetPalette(dst,pal)))goto out;
 if(!cnote("       QueryInterface(source, IDirect3DTexture)",IDirectDrawSurface_QueryInterface(src,&iid_tex1,(void**)&ts))||
    !cnote("       QueryInterface(destination, IDirect3DTexture)",IDirectDrawSurface_QueryInterface(dst,&iid_tex1,(void**)&td))||
    !cnote("       IDirect3DTexture::Load",IDirect3DTexture_Load(td,ts)))goto out;
 surf_line("destination after Load",dst);
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 if(cnote("       GetSurfaceDesc(destination)",IDirectDrawSurface_GetSurfaceDesc(dst,&sd))&&!r->sys&&!(sd.ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY))
  fprintf(logfile,"       not in video memory: Tunnel stops (\"Could not put all the textures in video memory\")\n");
 if(!cnote("       IDirect3DTexture::GetHandle",IDirect3DTexture_GetHandle(td,r->dv,&r->th[k])))goto out;
 fprintf(logfile,"       handle %lu\n",(unsigned long)r->th[k]);
 surf_line("destination after GetHandle",dst);commit();
 r->tex[k]=dst;dst=0;ok=1;
out:
 if(td)IDirect3DTexture_Release(td);if(ts)IDirect3DTexture_Release(ts);if(src)IDirectDrawSurface_Release(src);if(dst)IDirectDrawSurface_Release(dst);
 if(pal)IDirectDrawPalette_Release(pal);
 commit();return ok;
}
static BYTE *xop(BYTE *p,BYTE code,BYTE size,WORD count) {p[0]=code;p[1]=size;p[2]=(BYTE)count;p[3]=(BYTE)(count>>8);return p+4;}
static BYTE *xst(BYTE *p,DWORD k,DWORD v) {memcpy(p,&k,4);memcpy(p+4,&v,4);return p+8;}
static DWORD fbits(float f) {DWORD d;memcpy(&d,&f,4);return d;}
static DWORD tn_ebcaps,tn_ebdata,tn_ebgot; /* v2.19: execute buffer memory asked for (D3DDEBCAPS_*, 0 = the runtime's choice), where Lock put it, the caps Lock reported */
static int tn_exec(tn_rig *r,const BYTE *ins,DWORD inslen,const tl_vertex *v,DWORD nv,const char *what) {
 D3DEXECUTEBUFFERDESC ed;D3DEXECUTEDATA xd;LPDIRECT3DEXECUTEBUFFER eb=0;DWORD vb=nv*sizeof(tl_vertex);HRESULT hr;int ok=0;char line[160];
 memset(&ed,0,sizeof(ed));ed.dwSize=sizeof(ed);ed.dwFlags=D3DDEB_BUFSIZE;ed.dwBufferSize=vb+inslen;
 if(tn_ebcaps){ed.dwFlags|=D3DDEB_CAPS;ed.dwCaps=tn_ebcaps;}
 sprintf(line,"     %s: CreateExecuteBuffer(%lu bytes)",what,(unsigned long)(vb+inslen));
 if(!cnote(line,IDirect3DDevice_CreateExecuteBuffer(r->dv,&ed,&eb,0)))return 0;
 memset(&ed,0,sizeof(ed));ed.dwSize=sizeof(ed);
 if(!cnote("       Lock",IDirect3DExecuteBuffer_Lock(eb,&ed)))goto out;
 tn_ebdata=(DWORD)ed.lpData;tn_ebgot=ed.dwCaps;
 if(!quiet_ok)fprintf(logfile,"       execute buffer memory %08lX (%s), caps %08lX\n",(unsigned long)tn_ebdata,tn_ebdata>=0xC0000000UL?"video memory":"system memory",(unsigned long)tn_ebgot);
 if(vb)memcpy(ed.lpData,v,vb);memcpy((BYTE*)ed.lpData+vb,ins,inslen);
 cnote("       Unlock",IDirect3DExecuteBuffer_Unlock(eb));
 memset(&xd,0,sizeof(xd));xd.dwSize=sizeof(xd);xd.dwVertexCount=nv;xd.dwInstructionOffset=vb;xd.dwInstructionLength=inslen;
 cnote("       SetExecuteData",IDirect3DExecuteBuffer_SetExecuteData(eb,&xd));
 hr=IDirect3DDevice_BeginScene(r->dv);cnote("       BeginScene",hr);
 if(SUCCEEDED(hr)){ok=cnote("       Execute(UNCLIPPED)",IDirect3DDevice_Execute(r->dv,eb,r->vp,D3DEXECUTE_UNCLIPPED));cnote("       EndScene",IDirect3DDevice_EndScene(r->dv));}
out:
 IDirect3DExecuteBuffer_Release(eb);return ok;
}
/* Tunnel's D3DAppISetRenderState execute buffer (TUNNEL.EXE 405EBD): STATERENDER x14, STATELIGHT x3, EXIT */
static DWORD tn_rs[14][2],tn_ls[3][2];
static void tn_state_lists(tn_rig *r) {
 static const DWORD k[14]={D3DRENDERSTATE_SHADEMODE,D3DRENDERSTATE_TEXTUREPERSPECTIVE,D3DRENDERSTATE_ZENABLE,D3DRENDERSTATE_ZWRITEENABLE,D3DRENDERSTATE_ZFUNC,
  D3DRENDERSTATE_TEXTUREMAG,D3DRENDERSTATE_TEXTUREMIN,D3DRENDERSTATE_TEXTUREMAPBLEND,D3DRENDERSTATE_FILLMODE,D3DRENDERSTATE_DITHERENABLE,D3DRENDERSTATE_SPECULARENABLE,
  D3DRENDERSTATE_ANTIALIAS,D3DRENDERSTATE_FOGENABLE,D3DRENDERSTATE_FOGCOLOR};
 DWORD v[14];int j;
 v[0]=D3DSHADE_GOURAUD;v[1]=TRUE;v[2]=r->zb?TRUE:FALSE;v[3]=r->zb?TRUE:FALSE;v[4]=D3DCMP_LESSEQUAL;v[5]=D3DFILTER_LINEAR;v[6]=D3DFILTER_LINEAR;v[7]=D3DTBLEND_MODULATE;
 v[8]=D3DFILL_SOLID;v[9]=TRUE;v[10]=FALSE;v[11]=D3DANTIALIAS_NONE;v[12]=TRUE;v[13]=0;
 for(j=0;j<14;j++){tn_rs[j][0]=k[j];tn_rs[j][1]=v[j];}
 tn_ls[0][0]=D3DLIGHTSTATE_FOGMODE;tn_ls[0][1]=D3DFOG_LINEAR;tn_ls[1][0]=D3DLIGHTSTATE_FOGSTART;tn_ls[1][1]=fbits(0.5f);tn_ls[2][0]=D3DLIGHTSTATE_FOGEND;tn_ls[2][1]=fbits(1.0f);
}
/* rs/ls: -1 all, -2 none, else that one state */
static DWORD tn_build(BYTE *b,int rs,int ls) {
 BYTE *p=b;int j;
 if(rs!=-2){p=xop(p,D3DOP_STATERENDER,8,(WORD)(rs==-1?14:1));for(j=0;j<14;j++)if(rs==-1||rs==j)p=xst(p,tn_rs[j][0],tn_rs[j][1]);}
 if(ls!=-2){p=xop(p,D3DOP_STATELIGHT,8,(WORD)(ls==-1?3:1));for(j=0;j<3;j++)if(ls==-1||ls==j)p=xst(p,tn_ls[j][0],tn_ls[j][1]);}
 p=xop(p,D3DOP_EXIT,0,0);
 return (DWORD)(p-b);
}
static int tn_states(tn_rig *r) {
 BYTE b[256];tn_state_lists(r);
 return tn_exec(r,b,tn_build(b,-1,-1),0,0,"SetRenderState execute buffer (Tunnel's)");
}
static DWORD dp2_calls(void) {vx_d3d_diag d;memset(&d,0,sizeof(d));d.cb=sizeof(d);return stats&&stats(&d)?d.dp2:0;}
/* v2.18: which part of Tunnel's buffer the runtime fails (8007000E on the board, v10.185): each piece alone, with and without vertices */
static void tn_bisect(tn_rig *r) {
 BYTE b[256];tl_vertex v[4];int j,ok;DWORD before,n;char what[96];
 memset(v,0,sizeof(v));for(j=0;j<4;j++){v[j].x=(float)(8+j*8);v[j].y=8;v[j].z=0.5f;v[j].rhw=1;v[j].color=0xFFFFFFFFUL;}
 fprintf(logfile,"     -- pieces of Tunnel's buffer, one Execute each (failures shown in full):\n");commit();
 for(j=-7;j<17;j++) {
  int rs=-2,ls=-2;DWORD nv=0;tn_ebcaps=0;
  if(j==-7){strcpy(what,"EXIT only, execute buffer SYSTEMMEMORY");tn_ebcaps=D3DDEBCAPS_SYSTEMMEMORY;}
  else if(j==-6){strcpy(what,"EXIT only, execute buffer VIDEOMEMORY");tn_ebcaps=D3DDEBCAPS_VIDEOMEMORY;}
  else if(j==-5){strcpy(what,"whole buffer, execute buffer SYSTEMMEMORY");rs=-1;ls=-1;tn_ebcaps=D3DDEBCAPS_SYSTEMMEMORY;}
  else if(j==-4)strcpy(what,"EXIT only, no vertices");
  else if(j==-3){strcpy(what,"EXIT only, 4 vertices");nv=4;}
  else if(j==-2){strcpy(what,"all 14 render states, 4 vertices");rs=-1;nv=4;}
  else if(j==-1){strcpy(what,"whole buffer, 4 vertices");rs=-1;ls=-1;nv=4;}
  else if(j<14){sprintf(what,"render state %lu = %lu alone",(unsigned long)tn_rs[j][0],(unsigned long)tn_rs[j][1]);rs=j;}
  else {sprintf(what,"light state %lu = %08lX alone",(unsigned long)tn_ls[j-14][0],(unsigned long)tn_ls[j-14][1]);ls=j-14;}
  n=tn_build(b,rs,ls);before=dp2_calls();
  quiet_ok=1;tn_ebdata=0;ok=tn_exec(r,b,n,nv?v:0,nv,what);quiet_ok=0;tn_ebcaps=0;
  fprintf(logfile,"       %-44s %s, DrawPrimitives2 calls +%lu, buffer at %08lX caps %08lX\n",what,ok?"ok":"FAILED",(unsigned long)(dp2_calls()-before),(unsigned long)tn_ebdata,(unsigned long)tn_ebgot);commit();
 }
 diagnostics();
}
static DWORD tn_pixel(LPDIRECTDRAWSURFACE s,DWORD x,DWORD y,int *ok) {
 DDSURFACEDESC sd;DWORD px=0;BYTE *row;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);*ok=0;
 if(FAILED(IDirectDrawSurface_Lock(s,0,&sd,DDLOCK_WAIT|DDLOCK_READONLY,0)))return 0;
 row=(BYTE*)sd.lpSurface+y*sd.lPitch;
 if(sd.ddpfPixelFormat.dwRGBBitCount==16)px=((WORD*)row)[x];else if(sd.ddpfPixelFormat.dwRGBBitCount==32)px=((DWORD*)row)[x];
 IDirectDrawSurface_Unlock(s,0);*ok=sd.ddpfPixelFormat.dwRGBBitCount==16||sd.ddpfPixelFormat.dwRGBBitCount==32;
 return pf_rgb(&sd.ddpfPixelFormat,px);
}
/* one cleared frame: a quad per texture, in the four quarters of the target (TEXTUREHANDLE in the execute buffer, as DirectX 3/5 programs set it) */
static int tn_frame(tn_rig *r,int check,int quiet) {
 tl_vertex v[16];BYTE b[512],*p=b;D3DRECT rc;int k,ok=1,m;DWORD hw=r->w/2,hh=r->h/2;HRESULT hr;
 rc.x1=0;rc.y1=0;rc.x2=r->w;rc.y2=r->h;
 hr=IDirect3DViewport_Clear(r->vp,1,&rc,D3DCLEAR_TARGET|(r->zb?D3DCLEAR_ZBUFFER:0));
 if(!quiet||FAILED(hr))cnote("     Viewport Clear",hr);if(FAILED(hr))return 0;
 for(k=0;k<r->ntex;k++) {
  float x0=(float)((k&1)*hw+4),y0=(float)((k>>1)*hh+4),x1=(float)((k&1)*hw+hw-4),y1=(float)((k>>1)*hh+hh-4);
  tl_vertex *q=v+k*4;int j;
  for(j=0;j<4;j++){q[j].z=0.5f;q[j].rhw=1;q[j].color=0xFFFFFFFFUL;q[j].spec=0xFF000000UL;}
  q[0].x=x0;q[0].y=y0;q[0].u=0;q[0].v=0;q[1].x=x1;q[1].y=y0;q[1].u=1;q[1].v=0;q[2].x=x1;q[2].y=y1;q[2].u=1;q[2].v=1;q[3].x=x0;q[3].y=y1;q[3].u=0;q[3].v=1;
 }
 p=xop(p,D3DOP_PROCESSVERTICES,16,1);
 {DWORD f=D3DPROCESSVERTICES_COPY,n=r->ntex*4;WORD z=0;memcpy(p,&f,4);memcpy(p+4,&z,2);memcpy(p+6,&z,2);memcpy(p+8,&n,4);f=0;memcpy(p+12,&f,4);p+=16;}
 for(k=0;k<r->ntex;k++) {
  WORD t[8];int j;
  p=xop(p,D3DOP_STATERENDER,8,1);p=xst(p,D3DRENDERSTATE_TEXTUREHANDLE,r->th[k]);
  if(!((p-b)&7))p=xop(p,D3DOP_TRIANGLE,8,0); /* the triangle data must start on a QWORD (d3dmacs.h OP_NOP) */
  p=xop(p,D3DOP_TRIANGLE,8,2);
  t[0]=(WORD)(k*4);t[1]=(WORD)(k*4+1);t[2]=(WORD)(k*4+2);t[3]=D3DTRIFLAG_EDGEENABLETRIANGLE;t[4]=(WORD)(k*4);t[5]=(WORD)(k*4+2);t[6]=(WORD)(k*4+3);t[7]=D3DTRIFLAG_EDGEENABLETRIANGLE;
  for(j=0;j<8;j++){memcpy(p,&t[j],2);p+=2;}
 }
 p=xop(p,D3DOP_EXIT,0,0);
 quiet_ok=quiet;ok=tn_exec(r,b,(DWORD)(p-b),v,r->ntex*4,"textured quads");quiet_ok=0;
 if(ok&&check) {
  for(k=0;k<r->ntex;k++) {
   DWORD x=(k&1)*hw+hw/4,y=(k>>1)*hh+hh/4,got=tn_pixel(r->back,x,y,&m),want=tn_col[k][0];
   fprintf(logfile,"     quad %d at (%lu,%lu): %06lX, texel %06lX%s\n",k,(unsigned long)x,(unsigned long)y,(unsigned long)got,(unsigned long)want,
    m&&near_rgb(got,want)?"":m?"  <- DIFFERENT":"  (back buffer not readable)");
   if(!m||!near_rgb(got,want))ok=0;
  }
  commit();
 }
 return ok;
}
static void tn_release(tn_rig *r) {
 int k;
 for(k=0;k<4;k++)if(r->tex[k]){IDirectDrawSurface_Release(r->tex[k]);r->tex[k]=0;}
 if(r->vp){if(r->dv)IDirect3DDevice_DeleteViewport(r->dv,r->vp);IDirect3DViewport_Release(r->vp);r->vp=0;}
 if(r->dv){IDirect3DDevice_Release(r->dv);r->dv=0;}
 if(r->zb){if(r->back)IDirectDrawSurface_DeleteAttachedSurface(r->back,0,r->zb);IDirectDrawSurface_Release(r->zb);r->zb=0;}
 if(r->back){IDirectDrawSurface_Release(r->back);r->back=0;}
 if(r->clip){IDirectDrawClipper_Release(r->clip);r->clip=0;}
 if(r->front){IDirectDrawSurface_Release(r->front);r->front=0;}
 if(r->d3){IDirect3D_Release(r->d3);r->d3=0;}
 cnote("     released (textures, viewport, device, Z, back, front, IDirect3D)",0);
}
/* the surfaces, device, viewport, textures and render states; r->d1 exists, the cooperative level and (full screen) the mode are set */
static void tunnel_where(tn_rig *r);
static int tn_setup(tn_rig *r) {
 DDSURFACEDESC sd;DDSCAPS caps;D3DVIEWPORT v;int k;char line[128];DWORD vid=r->sys?DDSCAPS_SYSTEMMEMORY:DDSCAPS_VIDEOMEMORY;
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);
 if(r->full&&!r->sys) {
  sd.dwFlags=DDSD_CAPS|DDSD_BACKBUFFERCOUNT;sd.dwBackBufferCount=1;sd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE|DDSCAPS_FLIP|DDSCAPS_COMPLEX|DDSCAPS_3DDEVICE;
  if(!cnote("     CreateSurface(primary flip chain, 1 back buffer, 3DDEVICE)",IDirectDraw_CreateSurface(r->d1,&sd,&r->front,0)))return 0;
  memset(&caps,0,sizeof(caps));caps.dwCaps=DDSCAPS_BACKBUFFER;
  if(!cnote("     GetAttachedSurface(back buffer)",IDirectDrawSurface_GetAttachedSurface(r->front,&caps,&r->back)))return 0;
 } else {
  sd.dwFlags=DDSD_CAPS;sd.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
  if(!cnote("     CreateSurface(primary)",IDirectDraw_CreateSurface(r->d1,&sd,&r->front,0)))return 0;
  if(!r->full){
   if(!cnote("     CreateClipper",IDirectDraw_CreateClipper(r->d1,0,&r->clip,0)))return 0;
   IDirectDrawClipper_SetHWnd(r->clip,0,window);cnote("     SetClipper",IDirectDrawSurface_SetClipper(r->front,r->clip));
  }
  memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;sd.dwWidth=r->w;sd.dwHeight=r->h;
  sd.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE|vid;
  sprintf(line,"     CreateSurface(back buffer %lux%lu, OFFSCREENPLAIN|3DDEVICE|%s)",(unsigned long)r->w,(unsigned long)r->h,r->sys?"SYSTEMMEMORY":"VIDEOMEMORY");
  if(!cnote(line,IDirectDraw_CreateSurface(r->d1,&sd,&r->back,0)))return 0;
 }
 surf_line("back buffer",r->back);
 memset(&sd,0,sizeof(sd));sd.dwSize=sizeof(sd);sd.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_ZBUFFERBITDEPTH;sd.dwWidth=r->w;sd.dwHeight=r->h;
 sd.dwZBufferBitDepth=16;sd.ddsCaps.dwCaps=DDSCAPS_ZBUFFER|vid;
 if(cnote("     CreateSurface(Z buffer, 16-bit)",IDirectDraw_CreateSurface(r->d1,&sd,&r->zb,0))) {
  if(!cnote("     AddAttachedSurface(Z)",IDirectDrawSurface_AddAttachedSurface(r->back,r->zb))){IDirectDrawSurface_Release(r->zb);r->zb=0;}
  else surf_line("Z buffer",r->zb);
 } else r->zb=0;
 if(!cnote("     QueryInterface(IDirectDraw, IDirect3D)",IDirectDraw_QueryInterface(r->d1,&iid_d3d1,(void**)&r->d3)))return 0;
 if(!cnote(r->sys?"     QueryInterface(back buffer, IID_IDirect3DRGBDevice)":"     QueryInterface(back buffer, IID_IDirect3DHALDevice)",
    IDirectDrawSurface_QueryInterface(r->back,r->sys?&iid_rgbdev1:&iid_haldev,(void**)&r->dv)))return 0;
 diagnostics();
 if(!cnote("     IDirect3D::CreateViewport",IDirect3D_CreateViewport(r->d3,&r->vp,0))||!cnote("     AddViewport",IDirect3DDevice_AddViewport(r->dv,r->vp)))return 0;
 memset(&v,0,sizeof(v));v.dwSize=sizeof(v);v.dwWidth=r->w;v.dwHeight=r->h;v.dvScaleX=r->w/2.0f;v.dvScaleY=r->h/2.0f;v.dvMaxX=1;v.dvMaxY=1;v.dvMaxZ=1;
 if(!cnote("     SetViewport",IDirect3DViewport_SetViewport(r->vp,&v)))return 0;
 tn_nfmt=0;cnote("     EnumTextureFormats",IDirect3DDevice_EnumTextureFormats(r->dv,tn_fmt_cb,0));
 tunnel_where(r);
 if(!tn_nfmt){fprintf(logfile,"     no texture format without alpha: Tunnel has none to use\n");failures++;return 0;}
 r->fmt=tn_fmts[tn_nfmt-1];for(k=0;k<tn_nfmt;k++)if(tn_fmts[k].ddpfPixelFormat.dwFlags&DDPF_PALETTEINDEXED8)r->fmt=tn_fmts[k]; /* Tunnel prefers 8-bit palettized */
 {const DDPIXELFORMAT *f=&r->fmt.ddpfPixelFormat;
  if(f->dwFlags&DDPF_PALETTEINDEXED8)fprintf(logfile,"     Tunnel's choice: 8-bit palettized\n");
  else fprintf(logfile,"     Tunnel's choice: %lu-bit R %08lX G %08lX B %08lX\n",(unsigned long)f->dwRGBBitCount,(unsigned long)f->dwRBitMask,(unsigned long)f->dwGBitMask,(unsigned long)f->dwBBitMask);}
 commit();
 {BYTE b[256];DWORD before=dp2_calls();int ok;tn_state_lists(r); /* v2.18: Tunnel's buffer once before any texture exists */
  ok=tn_exec(r,b,tn_build(b,-1,-1),0,0,"Tunnel's SetRenderState buffer BEFORE the textures are loaded");
  fprintf(logfile,"     before textures: %s, DrawPrimitives2 calls +%lu\n",ok?"ok":"FAILED",(unsigned long)(dp2_calls()-before));commit();}
 r->ntex=0;
 for(k=0;k<4;k++){if(!tn_load(r,k,k?256:8,k?256:8))return 0;r->ntex++;}
 diagnostics();
 if(!tn_states(r)){static int once;diagnostics();if(!once){once=1;tn_bisect(r);}} /* v2.18: go on to the pieces and the textured frames */
 diagnostics();
 return 1;
}
/* v2.20: where the runtime's IDirect3DDevice (DirectX 3) methods are: module, base, offset and first code bytes, thunks followed */
static void code_where(const char *name,void *f) {
 MEMORY_BASIC_INFORMATION mi;char mod[MAX_PATH];BYTE *p=(BYTE*)f;int k,hops=0;
 while(p&&!IsBadReadPtr(p,8)&&hops<4) { /* follow jmp rel32 / jmp [abs] thunks */
  if(p[0]==0xE9){p=p+5+*(LONG*)(p+1);hops++;continue;}
  if(p[0]==0xFF&&p[1]==0x25&&!IsBadReadPtr(*(void**)(p+2),4)){p=**(BYTE***)(p+2);hops++;continue;}
  break;
 }
 memset(&mi,0,sizeof(mi));mod[0]=0;
 if(p&&VirtualQuery(p,&mi,sizeof(mi)))GetModuleFileNameA((HMODULE)mi.AllocationBase,mod,sizeof(mod));
 fprintf(logfile,"       %s at %08lX (after %d thunk(s)): module %s base %08lX offset %08lX; code",name,(unsigned long)p,hops,mod[0]?mod:"?",(unsigned long)mi.AllocationBase,(unsigned long)(p-(BYTE*)mi.AllocationBase));
 for(k=0;k<32&&p&&!IsBadReadPtr(p+k,1);k++)fprintf(logfile," %02X",p[k]);
 fprintf(logfile,"\n");commit();
}
static void tunnel_where(tn_rig *r) {
 void **vt=r->dv?*(void***)r->dv:0;
 if(!vt||IsBadReadPtr(vt,22*4)){fprintf(logfile,"     device vtable unreadable\n");return;}
 fprintf(logfile,"     IDirect3DDevice (DirectX 3) object %08lX, vtable %08lX:\n",(unsigned long)r->dv,(unsigned long)vt);
 code_where("CreateExecuteBuffer",vt[6]);code_where("Execute",vt[8]);code_where("BeginScene",vt[19]);code_where("EndScene",vt[20]);
 {HMODULE m=GetModuleHandleA("D3DIM700.DLL"),m2=GetModuleHandleA("D3DIM.DLL"),m3=GetModuleHandleA("DDRAW.DLL");
  fprintf(logfile,"     loaded: D3DIM700.DLL %08lX, D3DIM.DLL %08lX, DDRAW.DLL %08lX\n",(unsigned long)m,(unsigned long)m2,(unsigned long)m3);}
 commit();
}
static void tunnel_test(int sys) {
 HRESULT (WINAPI *create1)(GUID *,LPDIRECTDRAW *,IUnknown *)=(HRESULT (WINAPI *)(GUID *,LPDIRECTDRAW *,IUnknown *))GetProcAddress(ddlib,"DirectDrawCreate");
 tn_rig r;RECT rc;int k,ok;
 memset(&r,0,sizeof(r));r.sys=sys;
 fprintf(logfile,"== Tunnel (DirectX 5 SDK), windowed, %s\n",sys?"-systemmemory: system memory, RGB device":"HAL device, video memory");
 GetClientRect(window,&rc);r.w=rc.right-rc.left;r.h=rc.bottom-rc.top;if(r.w<64||r.h<64){r.w=256;r.h=192;}
 if(!create1||!cnote("   DirectDrawCreate",create1(0,&r.d1,0)))return;
 if(cnote("   SetCooperativeLevel(NORMAL)",IDirectDraw_SetCooperativeLevel(r.d1,window,DDSCL_NORMAL))&&tn_setup(&r)) {
  ok=tn_frame(&r,1,0);check("   textured quads drawn with each texture",ok);
  for(k=0;k<30&&ok;k++)ok=tn_frame(&r,0,1);
  fprintf(logfile,"   30 more frames: %s\n",ok?"ok":"FAILED");
  if(r.front&&r.back){POINT o;RECT d;o.x=0;o.y=0;ClientToScreen(window,&o);d.left=o.x;d.top=o.y;d.right=o.x+r.w;d.bottom=o.y+r.h;IDirectDrawSurface_Blt(r.front,&d,r.back,0,DDBLT_WAIT,0);Sleep(1500);}
 }
 diagnostics();tn_release(&r);IDirectDraw_Release(r.d1);fflush(logfile);
}
static void modes_test(int sys) {
 static const DWORD m[7][3]={{640,480,16},{1024,768,32},{640,480,16},{800,600,16},{1024,768,16},{640,480,32},{1024,768,32}};
 HRESULT (WINAPI *create1)(GUID *,LPDIRECTDRAW *,IUnknown *)=(HRESULT (WINAPI *)(GUID *,LPDIRECTDRAW *,IUnknown *))GetProcAddress(ddlib,"DirectDrawCreate");
 tn_rig r;int i,k,ok;char line[128];
 commit_lines=1;memset(&r,0,sizeof(r));r.sys=sys;r.full=1;
 fprintf(logfile,"== Tunnel full screen through display modes, %s; each line is on disk before the next step\n",sys?"-systemmemory: system memory, RGB device":"HAL device, video memory");
 if(!create1||!cnote("   DirectDrawCreate",create1(0,&r.d1,0)))return;
 if(!cnote("   SetCooperativeLevel(EXCLUSIVE|FULLSCREEN)",IDirectDraw_SetCooperativeLevel(r.d1,window,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))){IDirectDraw_Release(r.d1);return;}
 for(i=0;i<7;i++) {
  sprintf(line,"   SetDisplayMode(%lux%lux%lu)",(unsigned long)m[i][0],(unsigned long)m[i][1],(unsigned long)m[i][2]);
  if(!cnote(line,IDirectDraw_SetDisplayMode(r.d1,m[i][0],m[i][1],m[i][2])))continue;
  diagnostics();commit();
  r.w=m[i][0];r.h=m[i][1];
  if(tn_setup(&r)) {
   ok=tn_frame(&r,1,0);
   for(k=0;k<20&&ok;k++) {
    ok=tn_frame(&r,0,1);
    if(ok)ok=SUCCEEDED(r.sys?IDirectDrawSurface_Blt(r.front,0,r.back,0,DDBLT_WAIT,0):IDirectDrawSurface_Flip(r.front,0,DDFLIP_WAIT));
   }
   sprintf(line,"   %lux%lux%lu: 20 frames presented",(unsigned long)m[i][0],(unsigned long)m[i][1],(unsigned long)m[i][2]);check(line,ok);commit();
  }
  diagnostics();tn_release(&r);
 }
 cnote("   RestoreDisplayMode",IDirectDraw_RestoreDisplayMode(r.d1));
 cnote("   SetCooperativeLevel(NORMAL)",IDirectDraw_SetCooperativeLevel(r.d1,window,DDSCL_NORMAL));
 IDirectDraw_Release(r.d1);commit();commit_lines=0;
}
int main(int argc,char **argv) {
 HRESULT (WINAPI *create)(GUID *,void **,REFIID,IUnknown *);WNDCLASSA wc;
 int with_depth=argc==2&&!strcmp(argv[1],"--draw");
 int do_draw=with_depth||(argc==2&&!strcmp(argv[1],"--triangle"));
 int do_flip=argc==2&&!strcmp(argv[1],"--flip");
 int do_tex=argc==1||(argc>=2&&!strcmp(argv[1],"--texture")); /* v2.38: no argument runs --texture; --query is the old default */
 int do_raster=argc==2&&!strcmp(argv[1],"--raster");
 int do_stencil=argc==2&&!strcmp(argv[1],"--stencil"); /* v2.67 */
 int do_splane=argc==2&&!strcmp(argv[1],"--splane"); /* v2.69 */
 int do_enum=argc==2&&!strcmp(argv[1],"--enum");
 int do_wt=argc==2&&!strcmp(argv[1],"--wintune");
 int do_draws=argc==2&&!strcmp(argv[1],"--draws"); /* v2.40 */
 int do_tunnel=argc>=2&&!strcmp(argv[1],"--tunnel"),do_modes=argc>=2&&!strcmp(argv[1],"--modes"),sysmem=argc==3&&!strcmp(argv[2],"--systemmemory"); /* v2.17 */
 {int k;for(k=1;k<argc;k++)if(!strcmp(argv[k],"--nohang"))no_hang=1;} /* v2.72 */
 if(argc==2&&no_hang)do_tex=1; /* VXDX7 --nohang: the texture tests without the hang */
 logfile=fopen("VXDX7.TXT","w");if(!logfile)return 1;
 fprintf(logfile,"VX855 Direct3D M2 (VXDX7 v2.88, driver .533); mode=%s%s. No software device fallback.\n",do_modes?"Tunnel full screen through display modes":do_tunnel?"Tunnel (DirectX 5 SDK), windowed":do_wt?"WinTune 98 Direct3D test, repeated":do_enum?"DirectX 5/6 device enumeration":do_raster?"blend, alpha test, depth formats, clears":do_stencil?"stencil through Direct3D":do_splane?"stencil plane layout":do_tex?"textures":do_flip?"full-screen flip chains":do_draw?"draw+readback":do_draws?"draw states another application sent":"query only",(do_tunnel||do_modes)?(sysmem?", -systemmemory":", HAL"):"");
 code_probe("at start, before DirectDraw");
 ddlib=LoadLibraryA("DDRAW.DLL");if(!ddlib){fprintf(logfile,"DDRAW.DLL missing\n");failures++;goto done;}
 create=(HRESULT (WINAPI *)(GUID *,void **,REFIID,IUnknown *))GetProcAddress(ddlib,"DirectDrawCreateEx");
 if(!create){fprintf(logfile,"DirectX 7+ DirectDrawCreateEx unavailable\n");failures++;goto done;}
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="VXDX7M2";RegisterClassA(&wc);
 window=CreateWindowA(wc.lpszClassName,"VX855 Direct3D M2",WS_OVERLAPPEDWINDOW,40,40,300,250,0,0,wc.hInstance,0);
 if(!window){failures++;goto done;}
 if(!note("DirectDrawCreateEx",create(0,(void**)&dd,&iid_dd7,0))||
    !note("SetCooperativeLevel(NORMAL)",IDirectDraw7_SetCooperativeLevel(dd,window,DDSCL_NORMAL))||
    !note("QueryInterface(IDirect3D7)",IDirectDraw7_QueryInterface(dd,&IID_IDirect3D7,(void**)&d3d))||
    !note("EnumDevices",IDirect3D7_EnumDevices(d3d,enumerate,0)))goto done;
 code_probe("after DirectDrawCreateEx, before this program loads VXDDRAW.DLL");
 halmod=GetModuleHandleA("VXDDRAW.DLL"); if(!halmod) halmod=LoadLibraryA("VXDDRAW.DLL");
 code_probe("after LoadLibrary(VXDDRAW.DLL)"); /* v10.149: DirectDraw maps the HAL in the shared arena without adding it to this process's module list */if(halmod)stats=(DWORD (WINAPI *)(vx_d3d_diag *))GetProcAddress(halmod,"VX_D3D_QueryStats");
 if(!stats)fprintf(logfile,"M2 HAL diagnostic export unavailable in this process.\n");
 endpoint=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);diagnostics();
 { /* v2.37: the builds actually loaded: VxD (TEXDBG 0x75) and VXDDRAW.DLL (QueryStats build) */
  DWORD in=0x75,o[8],gt=0;vx_d3d_diag d;unsigned long vb=0,db=0;memset(&d,0,sizeof(d));d.cb=sizeof(d);
  if(endpoint!=INVALID_HANDLE_VALUE&&DeviceIoControl(endpoint,0x85500022UL,&in,4,o,32,&gt,0)&&gt==32&&o[0]==0x75)vb=o[1];
  if(stats&&stats(&d))db=d.build;
  fprintf(logfile,"Loaded: VX85598.VXD build %lu, VXDDRAW.DLL build %lu (0 = older than v10.214 or not answering)\n",vb,db);}
 runtime_report();
 { /* v10.149: what decides the advertisement, and what the runtime made of it */
  DWORD code=0xFFFFFE50UL,o[4],got=0; DDCAPS hw,hel;
  memset(o,0,sizeof(o));
  if(endpoint!=INVALID_HANDLE_VALUE&&DeviceIoControl(endpoint,0x85500012UL,&code,4,o,16,&got,0)&&got==16&&o[1]==0x56584433UL) fprintf(logfile,"VxD registry ACCEL_D3D = %lu (default 1: DirectX 6/7 HAL; 3: DirectX 5-style HAL; 0: off; HKLM\\Software\\VX85598, then restart)\n",(unsigned long)o[0]);
  else fprintf(logfile,"VxD does not answer the ACCEL_D3D query (older than v10.149)\n");
  memset(&hw,0,sizeof(hw));hw.dwSize=sizeof(hw);memset(&hel,0,sizeof(hel));hel.dwSize=sizeof(hel);
  if(SUCCEEDED(IDirectDraw7_GetCaps(dd,&hw,&hel))) fprintf(logfile,"DirectDraw driver caps: dwCaps %08lX (DDCAPS_3D %s), dwCaps2 %08lX, surface caps %08lX (3DDEVICE %s), Z depths %08lX\n",
   (unsigned long)hw.dwCaps,(hw.dwCaps&DDCAPS_3D)?"yes":"no",(unsigned long)hw.dwCaps2,(unsigned long)hw.ddsCaps.dwCaps,(hw.ddsCaps.dwCaps&DDSCAPS_3DDEVICE)?"yes":"no",(unsigned long)hw.dwZBufferBitDepths);
  fflush(logfile);
 }
 if(do_draw){ShowWindow(window,SW_SHOW);UpdateWindow(window);if(setup(with_depth)&&run_tests(with_depth)&&!failures)preview();}
 if(do_tex){ShowWindow(window,SW_SHOW);UpdateWindow(window);if(setup(0))texture_tests();}
 if(do_raster){ShowWindow(window,SW_SHOW);UpdateWindow(window);if(setup(0))raster_tests();}
 if(do_stencil){ShowWindow(window,SW_SHOW);UpdateWindow(window);if(setup(0))stencil_tests();}
 if(do_splane){ShowWindow(window,SW_SHOW);UpdateWindow(window);if(setup(0))splane_tests();}
 if(do_enum){ShowWindow(window,SW_SHOW);UpdateWindow(window);enum_tests();}
 if(do_wt){ShowWindow(window,SW_SHOW);UpdateWindow(window);wintune_tests();}
 if(do_tunnel){ShowWindow(window,SW_SHOW);UpdateWindow(window);tunnel_test(sysmem);}
 if(do_modes){ShowWindow(window,SW_SHOW);UpdateWindow(window);modes_test(sysmem);}
 if(do_flip){
  DWORD (WINAPI *q)(DWORD *,DWORD)=0;DWORD o[2+8+128];
  ShowWindow(window,SW_SHOW);UpdateWindow(window);flip_test(16);flip_test(32);
  if(halmod)q=(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(halmod,"VX_D3D_QueryStates");
  if(q&&q(o,sizeof(o)/sizeof(o[0])))fprintf(logfile,"HAL render-target changes in DrawPrimitives2: %lu, last handle %lu at offset %08lX; last target offset %08lX\n",(unsigned long)o[6],(unsigned long)o[7],(unsigned long)o[8],(unsigned long)o[9]);
 }
done:
 diagnostics();
 if(do_draw||do_tex||do_raster||do_wt||do_tunnel)trace_report();
 if(do_draws)draws_report();
 if(do_draw&&halmod) { /* v1.5: legacy render states as the HAL received them */
  DWORD (WINAPI *q)(DWORD *,DWORD)=(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(halmod,"VX_D3D_QueryStates");DWORD o[2+8+128],i;
  if(q&&q(o,sizeof(o)/sizeof(o[0]))){
   fprintf(logfile,"HAL legacy render states received %lu; shade mode at the legacy draws:",(unsigned long)o[0]);
   for(i=0;i<8&&i<o[1];i++)fprintf(logfile," %lu",(unsigned long)o[2+i]);
   fprintf(logfile,"\n  ");for(i=0;i<64&&i<o[0];i++)fprintf(logfile," %lu=%lu",(unsigned long)o[10+i*2],(unsigned long)o[11+i*2]);fprintf(logfile,"\n");
  }
 }
 if(dev)IDirect3DDevice7_Release(dev);if(primary)IDirectDrawSurface7_Release(primary);
 if(clipper)IDirectDrawClipper_Release(clipper);if(rt)IDirectDrawSurface7_Release(rt);if(depth)IDirectDrawSurface7_Release(depth);
 if(d3d)IDirect3D7_Release(d3d);if(dd)IDirectDraw7_Release(dd);
 if(window&&IsWindow(window))DestroyWindow(window);if(endpoint!=INVALID_HANDLE_VALUE)CloseHandle(endpoint);if(ddlib)FreeLibrary(ddlib);
 fprintf(logfile,"Finished: devices=%lu failures=%lu.\n", /* v2.88 (.531): the old line about host builds removed */(unsigned long)devices,(unsigned long)failures);
 fclose(logfile);printf("VXDX7.TXT written: %lu failure(s).\n",(unsigned long)failures);return failures?1:0;
}
