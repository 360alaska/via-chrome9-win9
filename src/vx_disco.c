/* VXDISCO v2.19 (driver v10.329; v2.18 with v10.326): WinTune 98's Direct3D "Disco" test (WinTunDD.ocx, wtDisco2) rebuilt from its code, drawn on the
 * hardware (HAL) device and on Microsoft's RGB software device, and compared pixel by pixel. The scene, its states and its order are
 * WinTune's (DirectX 6 interfaces: IDirectDraw4, IDirect3D3, IDirect3DDevice3, Viewport3, D3DLIGHT lights, one material, D3DVERTEX
 * geometry lit by the runtime, no Z buffer, 800x600x16):
 *   InitDeviceObjects 0x10002149: material diffuse/ambient/specular (1,1,1,0), power 30; ambient 0x20202020; stage 0 MODULATE(TEXTURE,
 *     DIFFUSE), MIN/MAG LINEAR; DITHERENABLE, SPECULARENABLE; world identity; projection fov 1.57 aspect 1 near 1 far 100; four lights.
 *   FrameMove 0x100023a6: t = 0.05 a frame; three spot lights at the origin and one directional light, colours and directions from
 *     sin 2t, sin 2.246t, sin 2.64t; camera (4 sin a, 3, -4 cos a), a = 0.3 sin 2t - pi/4; sphere rotated 10 degrees about Z a frame
 *     (normals not), cube 10 degrees about Z for 36 frames then about X for 36.
 *   Render 0x10002939: clear to black (no Z), SetTexture NULL, a 108x108 grid three times (floor, wall x, wall z), the texture, the
 *     sphere (radius 1, 100 rings x 200 segments, inward normals, random texture coordinates from the MSVC rand, seed 1) and the cube.
 *   Texture: bitmap resource 201 (64x64 bands), R5G6B5 (the first 16-bit format without alpha this driver lists), managed.
 * Usage: VXDISCO [frame]   (default 30); VXDISCO --wintune [runs] [path of another WinTunDD.ocx] (v1.2, below). Writes VXDISCO.TXT, DISCOHAL.BMP and DISCOREF.BMP (the full frame on each device).
 * Each device draws five scenes in a row, as WinTune draws frame after frame: everything, walls only, sphere only, cube only, everything
 * again; each is compared with the RGB device's. v1.1: a failing call is logged with the HAL's event log, the HAL's counters and the VxD's
 * state; a failed scene is drawn again on a new device; every image is read back and compared even when a call failed. */
#include <windows.h>
#include <ddraw.h>
#include <d3d.h>
#include <stdio.h>
#include <string.h>
#include "vx_curchk.h"
#include <math.h>
#include "vx_disco_tex.h"
#include "vx_native3d.h"
#include "vx_d3d_diag.h"
/* D3DVERTEX as d3dtypes.h defines it for DirectX (this Watcom header drops the nx member, so it is spelled out here: 8 floats, FVF 0x112) */
typedef struct {float x,y,z,nx,ny,nz,tu,tv;} VTX;
static const GUID iid_dd4={0x9C59509AUL,0x39BD,0x11D1,{0x8C,0x4A,0x00,0xC0,0x4F,0xD9,0x30,0xC5}}; /* IID_IDirectDraw4 (not in this dxguid.lib) */

#define W 800
#define H 600
static FILE *logf;static unsigned long failures;
static int note(const char *s,HRESULT hr){fprintf(logf,"   %s = %08lX\n",s,(unsigned long)hr);fflush(logf);if(FAILED(hr)){failures++;return 0;}return 1;}

/* ---- geometry, exactly as OneTimeSceneInit builds it (re/gen_geometry.py) ---- */
static VTX grid[108*108];static WORD gidx[68694];
static VTX sph[20002];static WORD sidx[120000];
static VTX cube[24];static WORD cidx[36];
static unsigned long rseed=1;
static int msrand(void){rseed=rseed*214013UL+2531011UL;return (int)((rseed>>16)&0x7FFF);}
static float ruv(void){int a=msrand(),b=msrand();return (float)((double)(a-b)*(double)3.0518509447574615e-05f);}
static void vset(VTX *v,float x,float y,float z,float nx,float ny,float nz,float tu,float tv){v->x=x;v->y=y;v->z=z;v->nx=nx;v->ny=ny;v->nz=nz;v->tu=tu;v->tv=tv;}
static void build(void) {
 const float K=0.009345794096589088f,PI=3.1415927410125732f,CT=0.009900989942252636f,CP=0.009999999776482582f;
 int ix,iz,r,j,i,n=0,c,band,base;float dth,dph;
 for(ix=0;ix<108;ix++)for(iz=0;iz<108;iz++){float x=(float)((double)ix*(double)K),z=(float)((double)iz*(double)K);vset(&grid[ix*108+iz],x,0,z,0,1,0,x,z);}
 for(r=1;r<108;r++)for(j=0;j<107;j++){WORD a=(WORD)((r-1)*108+j),b=(WORD)(r*108+j);gidx[n++]=a;gidx[n++]=(WORD)(a+1);gidx[n++]=b;gidx[n++]=b;gidx[n++]=(WORD)(a+1);gidx[n++]=(WORD)(b+1);}
 {float tv=ruv(),tu=ruv();vset(&sph[0],0,1,0,0,-1,0,tu,tv);}
 {float tv=ruv(),tu=ruv();vset(&sph[1],0,-1,0,0,1,0,tu,tv);}
 dth=(float)((double)PI*(double)CT);dph=(float)((double)PI*(double)CP);
 for(i=1;i<=100;i++){double th=(double)i*(double)dth;
  for(j=0;j<200;j++){double ph=(double)j*(double)dph;float y=(float)cos(th),x=(float)(sin(th)*sin(ph)),z=(float)(sin(th)*cos(ph)),tv=ruv(),tu=ruv();
   vset(&sph[2+(i-1)*200+j],x,y,z,-x,-y,-z,tu,tv);}}
 n=0;for(c=0;c<200;c++){sidx[n++]=0;sidx[n++]=(WORD)(c+2);sidx[n++]=(WORD)(c==199?2:c+3);}
 base=2;for(band=0;band<99;band++){for(j=0;j<200;j++){int jn=j==199?0:j+1;sidx[n++]=(WORD)(base+j);sidx[n++]=(WORD)(base+j+200);sidx[n++]=(WORD)(base+jn);
   sidx[n++]=(WORD)(base+j+200);sidx[n++]=(WORD)(base+jn+200);sidx[n++]=(WORD)(base+jn);}base+=200;}
 for(c=0;c<200;c++){sidx[n++]=1;sidx[n++]=(WORD)(c==199?19802:19802+c+1);sidx[n++]=(WORD)(19802+c);}
 {static const float P[24][3]={{-.5F,.5F,-.5F},{.5F,.5F,-.5F},{.5F,-.5F,-.5F},{-.5F,-.5F,-.5F},{-.5F,.5F,.5F},{-.5F,-.5F,.5F},{.5F,-.5F,.5F},{.5F,.5F,.5F},
   {-.5F,.5F,.5F},{.5F,.5F,.5F},{.5F,.5F,-.5F},{-.5F,.5F,-.5F},{-.5F,-.5F,.5F},{-.5F,-.5F,-.5F},{.5F,-.5F,-.5F},{.5F,-.5F,.5F},
   {.5F,.5F,-.5F},{.5F,.5F,.5F},{.5F,-.5F,.5F},{.5F,-.5F,-.5F},{-.5F,.5F,-.5F},{-.5F,-.5F,-.5F},{-.5F,-.5F,.5F},{-.5F,.5F,.5F}};
  static const float N[6][3]={{0,0,-.5F},{0,0,.5F},{0,.5F,0},{0,-.5F,0},{.5F,0,0},{-.5F,0,0}};
#define t0 0.01f
#define t1 0.99f
  static const float T[24][2]={{t0,t0},{t1,t0},{t1,t1},{t0,t1},{t1,t0},{t1,t1},{t0,t1},{t0,t0},{t0,t0},{t1,t0},{t1,t1},{t0,t1},
   {t0,t1},{t0,t0},{t1,t0},{t1,t1},{t0,t0},{t1,t0},{t1,t1},{t0,t1},{t1,t0},{t1,t1},{t0,t1},{t0,t0}};
#undef t0
#undef t1
  for(i=0;i<24;i++)vset(&cube[i],P[i][0],(float)(P[i][1]+2.0f),P[i][2],N[i/4][0],N[i/4][1],N[i/4][2],T[i][0],T[i][1]);
  for(i=0;i<6;i++){int q=4*i;cidx[i*6]=(WORD)q;cidx[i*6+1]=(WORD)(q+1);cidx[i*6+2]=(WORD)(q+2);cidx[i*6+3]=(WORD)(q+2);cidx[i*6+4]=(WORD)(q+3);cidx[i*6+5]=(WORD)q;}}
}
/* ---- per-frame animation (FrameMove 0x100023a6) ---- */
static double rc,rs;static int cube_axis=1,cube_count;
static float t_now,s1,s2,s3;static D3DMATRIX view;
static void rot(VTX *v,int n,int about_z){int k;for(k=0;k<n;k++){
 if(about_z){double x=v[k].x,y=v[k].y;v[k].x=(float)(x*rc-y*rs);v[k].y=(float)(x*rs+y*rc);}
 else {double y=v[k].y,z=v[k].z;v[k].y=(float)(y*rc-z*rs);v[k].z=(float)(y*rs+z*rc);}}}
static void lookat(D3DMATRIX *m,float ex,float ey,float ez) {
 float vx=-ex,vy=-ey,vz=-ez,l=(float)sqrt(vx*vx+vy*vy+vz*vz),d,ux,uy,uz,rx,ry,rz;
 vx/=l;vy/=l;vz/=l;d=vy;ux=0-d*vx;uy=1-d*vy;uz=0-d*vz;l=(float)sqrt(ux*ux+uy*uy+uz*uz);ux/=l;uy/=l;uz/=l;
 rx=uy*vz-uz*vy;ry=uz*vx-ux*vz;rz=ux*vy-uy*vx;memset(m,0,sizeof(*m));
 m->_11=rx;m->_12=ux;m->_13=vx;m->_21=ry;m->_22=uy;m->_23=vy;m->_31=rz;m->_32=uz;m->_33=vz;
 m->_41=-(ex*rx+ey*ry+ez*rz);m->_42=-(ex*ux+ey*uy+ez*uz);m->_43=-(ex*vx+ey*vy+ez*vz);m->_44=1;
}
static void frame_move(void) {
 float a;s1=(float)sin(2.0f*t_now);s2=(float)sin(2.246f*t_now);s3=(float)sin(2.64f*t_now);
 a=(float)(0.3f*s1-3.1415927410125732f*0.25f);
 lookat(&view,(float)(4.0*sin(a)),3.0f,(float)(-4.0*cos(a)));
 rot(sph,20002,1);rot(cube,24,cube_axis>0);
 if(++cube_count>35){cube_count=0;cube_axis=-cube_axis;}
}
static void lights_at(D3DLIGHT *l) { /* the four lights for the current t (FrameMove's SetLight calls) */
 float A=(s1+1)/2,B=(s2+1)/2,C=(s3+1)/2;int k;
 static const float sg[3][3]={{1,1,1},{1,1,-1},{1,-1,1}};
 for(k=0;k<4;k++){memset(&l[k],0,sizeof(l[k]));l[k].dwSize=sizeof(D3DLIGHT);}
 for(k=0;k<3;k++){l[k].dltType=D3DLIGHT_SPOT;l[k].dvRange=(float)sqrt(3.402823466e38);l[k].dvFalloff=100.0f;l[k].dvAttenuation0=1.0f;l[k].dvTheta=0.8f;l[k].dvPhi=2.5f;
  l[k].dvDirection.x=s1*sg[k][0];l[k].dvDirection.y=s2*sg[k][1];l[k].dvDirection.z=s3*sg[k][2];}
 l[0].dcvColor.r=A;l[0].dcvColor.g=1-B;l[0].dcvColor.b=C;l[1].dcvColor.r=A;l[1].dcvColor.g=B;l[1].dcvColor.b=C;l[2].dcvColor.r=1-A;l[2].dcvColor.g=B;l[2].dcvColor.b=C;
 l[3].dltType=D3DLIGHT_DIRECTIONAL;l[3].dcvColor.r=A;l[3].dcvColor.g=B;l[3].dcvColor.b=C;l[3].dvDirection.x=-s1;l[3].dvDirection.y=-s2;l[3].dvDirection.z=s3;
}
/* ---- one device ---- */
typedef struct {const char *name;GUID guid;int found;} devpick;
static devpick hal={"Direct3D HAL"},ref={"RGB Emulation"};
static HRESULT CALLBACK enum_dev(GUID *g,LPSTR desc,LPSTR name,LPD3DDEVICEDESC hw,LPD3DDEVICEDESC sw,LPVOID u) {
 (void)desc;(void)hw;(void)sw;(void)u;
 if(!strcmp(name,hal.name)&&!hal.found){hal.guid=*g;hal.found=1;}
 if(!strcmp(name,ref.name)&&!ref.found){ref.guid=*g;ref.found=1;}
 fprintf(logf,"   device: %s\n",name);return D3DENUMRET_OK;
}
static LPDIRECTDRAW4 dd4;static LPDIRECT3D3 d3d3;
#define SETS 5
static const char *set_name[SETS]={"full frame","walls only","sphere only","cube only","full again"};
static unsigned char img[2][SETS][W*H*3]; /* [device][set] RGB */
static int have[2][SETS];
/* ---- v1.1: what the driver saw (HAL event log, HAL counters, VxD state), printed when something fails ---- */
static DWORD (WINAPI *querydraws)(DWORD *,DWORD);
static DWORD (WINAPI *queryev)(DWORD *,DWORD);
static DWORD (WINAPI *querystats)(vx_d3d_diag *);
static HANDLE endpoint=INVALID_HANDLE_VALUE;
static DWORD ev_shown;
static void hal_events(void) {
 static const char *cbn[31]={"?","ContextCreate","DrawPrimitives2","?","?","ValidateTextureStageState","CreateSurfaceEx","packet refused","refused packet offsets","refused packet vertex","refused packet state","fog","packet waited (VxD busy)",
  "?","?","?","?","?","?","?","Blt","Flip","Lock","Unlock","GetBltStatus","GetFlipStatus","CreateSurface","CanCreateSurface","WaitForVerticalBlank","SetExclusiveMode","DestroySurface"}; /* v1.8: the 2D callbacks */
 static DWORD o[1+16*6];DWORD c,m,first,k;
 if(!queryev||(c=queryev(o,sizeof(o)/sizeof(o[0])))==0||o[0]==ev_shown)return;
 m=(c-1)/6;first=o[0]-m;
 if(first>ev_shown)fprintf(logf,"   (HAL events %lu..%lu were overwritten)\n",(unsigned long)ev_shown+1,(unsigned long)first);
 for(k=0;k<m;k++)if(first+k>=ev_shown)fprintf(logf,"   HAL event #%lu %s: result %08lX reason %lX, %08lX %08lX %08lX\n",(unsigned long)(first+k+1),cbn[o[1+k*6]<31?o[1+k*6]:0],
  (unsigned long)o[2+k*6],(unsigned long)o[3+k*6],(unsigned long)o[4+k*6],(unsigned long)o[5+k*6],(unsigned long)o[6+k*6]);
 ev_shown=o[0];
}
static void hal_state(const char *when) {
 vx_d3d_diag d;vx3dn_info n;DWORD got=0;
 memset(&d,0,sizeof(d));d.cb=sizeof(d);
 if(querystats&&querystats(&d))
  fprintf(logf,"   HAL %s: build %lu, DP2 calls %lu, packets %lu, last failing callback %lu hr %08lX opcode %lu offset %lu; left out %lu (why %03lX); context lost %lu times, last why %03lX value %08lX; engine recoveries %lu\n",
   when,(unsigned long)d.build,(unsigned long)d.dp2,(unsigned long)d.native_packets,(unsigned long)d.last_callback,(unsigned long)d.last_hr,(unsigned long)d.last_opcode,(unsigned long)d.last_offset,
   (unsigned long)d.dropped,(unsigned long)d.drop_why,(unsigned long)d.lost_count,(unsigned long)d.lost_why,(unsigned long)d.lost_value,(unsigned long)d.recovered);
 memset(&n,0,sizeof(n));
 if(endpoint!=INVALID_HANDLE_VALUE&&DeviceIoControl(endpoint,VX_IOCTL_3DN_INFO,0,0,&n,sizeof(n),&got,0)&&got==sizeof(n))
  fprintf(logf,"   VxD: ready %lu, generation %lu, %lux%lux%lu, submitted %lu, triangles %lu, clears %lu, fault %lu, last result %08lX, last status %08lX\n",
   (unsigned long)n.ready,(unsigned long)n.generation,(unsigned long)n.width,(unsigned long)n.height,(unsigned long)n.bpp,(unsigned long)n.submitted,(unsigned long)n.triangles,
   (unsigned long)n.clears,(unsigned long)n.fault,(unsigned long)n.last_result,(unsigned long)n.last_status);
 {DWORD in=0xA5,o[8];got=0;memset(o,0,sizeof(o)); /* v1.8 */
  if(endpoint!=INVALID_HANDLE_VALUE&&DeviceIoControl(endpoint,VX_IOCTL_3DN_TEXDBG,&in,4,o,32,&got,0)&&got==32&&o[0]==0xA5)
   fprintf(logf,"   VxD 2D: state %lu, native fault %lu, CPU access refused %lu times (lost at generation %lu, now %lu), failed recoveries %lu\n",
    (unsigned long)o[1],(unsigned long)o[2],(unsigned long)o[3],(unsigned long)o[4],(unsigned long)o[5],(unsigned long)o[6]);}
 {HMODULE hm=GetModuleHandleA("VXDDRAW.DLL");DWORD (WINAPI *ro)(void)=hm?(DWORD (WINAPI *)(void))GetProcAddress(hm,"VX_DD_Reopened"):0; /* v1.9 */
  if(ro)fprintf(logf,"   DirectDraw HAL: VxD handles opened again after a failed call: %lu\n",(unsigned long)ro());}
 hal_events();
}
/* ---- v1.5: where a scene's time goes: the HAL's DrawPrimitives2 time (its own CPU work plus the submit IOCTLs), the VxD's time in the
 * submits, engine writes and status reads (VX_D3D_QueryProfile, TEXDBG 0x76/0x77); the rest is the Direct3D runtime (transform,
 * lighting, clipping) and the application ---- */
static DWORD (WINAPI *queryprof)(DWORD *,DWORD);
static double mhz;
unsigned long app_tsc(void);
#pragma aux app_tsc = 0x0F 0x31 value [eax] modify exact [eax edx];
static void measure_mhz(void) {DWORD t=GetTickCount(),c;unsigned long a;while(GetTickCount()==t);t=GetTickCount();a=app_tsc();while(GetTickCount()-t<250);c=GetTickCount()-t;mhz=(double)(app_tsc()-a)/(c*1000.0);}
static int texdbg(DWORD code,DWORD *o) {DWORD in=code,got=0;memset(o,0,32);return endpoint!=INVALID_HANDLE_VALUE&&DeviceIoControl(endpoint,VX_IOCTL_3DN_TEXDBG,&in,4,o,32,&got,0)&&got==32;}
static int vxd_prof(DWORD code,DWORD *o) {DWORD in=code,got=0;memset(o,0,32);return endpoint!=INVALID_HANDLE_VALUE&&DeviceIoControl(endpoint,VX_IOCTL_3DN_TEXDBG,&in,4,o,32,&got,0)&&got==32&&o[0]==code;}
static DWORD prof_t0;
static DWORD (WINAPI *querylat)(DWORD *,DWORD); /* v2.15 (driver v10.321): latency distributions */
static void prof_start(void) {DWORD o[8],l[18];if(queryprof)queryprof(o,0x80000008UL);if(querylat)querylat(l,0x80000012UL);vxd_prof(0x68,o);vxd_prof(0x77,o);texdbg(0xA4,o);texdbg(0xA9,o);texdbg(0xAD,o);texdbg(0xB0,o);prof_t0=GetTickCount();}
/* v1.7: the VxD's command-fetcher state (TEXDBG 0xA3: 0xA1 on / 0xA2 off, packets fetched, words, packets sent through the registers
 * instead, fetches that did not finish, most words in a packet, the command block's offset) */
static int fetch_state(DWORD *o) {return texdbg(0xA3,o)&&(o[0]==0xA1||o[0]==0xA2);}
static void prof_print(void) {
 DWORD h[8],v[8],ms=GetTickCount()-prof_t0;double k=mhz>0?256.0/(mhz*1000.0):0; /* the counters are TSC cycles / 256 */
 memset(h,0,sizeof(h));if(queryprof)queryprof(h,8);
 fprintf(logf,"   time %lu ms",(unsigned long)ms);
 if(queryprof&&k)fprintf(logf,"; HAL DrawPrimitives2 %.0f ms in %lu calls, of it %.0f ms in %lu submit IOCTLs; %lu triangles",h[1]*k,(unsigned long)h[0],h[3]*k,(unsigned long)h[2],(unsigned long)h[4]);
 if(vxd_prof(0x76,v)&&k)fprintf(logf,"; VxD %.0f ms for %lu draw packets (%lu engine writes, %lu status reads, %lu other reads), %lu clears %.0f ms",
  v[2]*k,(unsigned long)v[1],(unsigned long)v[3],(unsigned long)v[4],(unsigned long)v[5],(unsigned long)v[6],v[7]*k);
 {DWORD f[8];if(fetch_state(f))fprintf(logf,"; fetched %lu packets (%lu words, at most %lu; %lu left running), %lu through the registers instead, %lu did not finish",
  (unsigned long)f[1],(unsigned long)f[2],(unsigned long)f[5],(unsigned long)(f[7]&0x7FFFFFFFUL),(unsigned long)f[3],(unsigned long)f[4]);
  if(texdbg(0xA8,f)&&f[0]==0xA8&&k)fprintf(logf,"; %lu sent without state; fetch path %.0f ms (building %.0f ms, waiting for the draw before %.0f ms)",
   (unsigned long)f[2],f[5]*k,f[3]*k,f[4]*k);
  if(texdbg(0xAC,f)&&f[0]==0xAC&&k)fprintf(logf,"; VxD draw submits: checks and copy %.0f ms, sequencer and clock %.0f ms, draw %.0f ms, close %.0f ms",f[1]*k,f[2]*k,f[3]*k,f[4]*k);
  if(texdbg(0xB1,f)&&f[0]==0xAE)fprintf(logf,"; %lu strips (%lu vertices) and %lu list runs, strips drawn %s",(unsigned long)f[2],(unsigned long)f[3],(unsigned long)f[4],f[1]?"as strips":"as triangles");}
 fprintf(logf,"\n");
 /* v2.16 (v10.322; board, v10.321: the first scenes after VXDX7's hang test had groups that did not finish): the VxD's engine recoveries so
  * far (TEXDBG 0x70) and, when a group did not finish since the last scene, its packets (TEXDBG 0xE0 + n) */
 {static DWORD last_hang=0xFFFFFFFFUL;DWORD r[8],o[8];
  if(texdbg(0x70,r)&&r[0]==0x70)fprintf(logf,"   VxD engine recoveries %lu, failed %lu, last step %lu, status before the last reset %08lX\n",(unsigned long)r[1],(unsigned long)r[2],(unsigned long)r[5],(unsigned long)r[3]);
  if(texdbg(0xE0,o)&&o[0]==0xE0&&o[1]!=last_hang){DWORD i,n=o[3]&0xFFFF;
   if(last_hang!=0xFFFFFFFFUL){fprintf(logf,"   the last group that did not finish (%lu so far): status %08lX, %lu packets\n",(unsigned long)o[1],(unsigned long)o[2],(unsigned long)n);
    for(i=0;i<n&&i<16;i++)if(texdbg(0xE0+i,o)){DWORD a=o[4],b=o[5];
     fprintf(logf,"     %2lu: op %lu target %02lX depth %02lX, %lu vertices; z %lu write %lu func %lu; blend %lu; textured %lu units %lu; prim %lu; program %lu; %lu words\n",(unsigned long)i,
      (unsigned long)(a&255),(unsigned long)((a>>8)&255),(unsigned long)((a>>16)&255),(unsigned long)(a>>24),(unsigned long)(b&1),(unsigned long)((b>>1)&1),(unsigned long)((b>>4)&15),
      (unsigned long)((b>>8)&1),(unsigned long)((b>>9)&1),(unsigned long)((b>>10)&3),(unsigned long)((b>>12)&15),(unsigned long)(b>>24),(unsigned long)(o[7]>>16));}}
   if(texdbg(0xE0,o))last_hang=o[1];}}
 /* v2.17 (v10.325): the hardware cursor read back at every scene against the first scene's reading (vx_curchk.h) */
 {static vx_curchk cb;static int hb=-1;vx_curchk cn;
  if(hb<0)hb=curchk_base(endpoint,&cb);
  if(hb&&curchk_read(endpoint,&cn))curchk_log(logf,"",&cn,&cb);}
 /* v2.15: p50 / p95 / p99 / max in microseconds (the busy retries in attempts) */
 if(mhz>0){static const char *const hn[3]={"HAL DrawPrimitives2","HAL submit IOCTL","HAL busy retries (attempts)"};
  static const char *const vn[4]={"VxD 3D lock held","VxD idle wait","VxD wait for the draw before","VxD draw submit"};DWORD l[18],o[8];int r;
  if(querylat&&querylat(l,18)==18)for(r=0;r<3;r++)if(l[r*6]){double u=r==2?1:1/mhz;
   fprintf(logf,"   %s: %lu, p50 %.1f p95 %.1f p99 %.1f max %.1f%s\n",hn[r],(unsigned long)l[r*6],l[r*6+1]*u,l[r*6+2]*u,l[r*6+3]*u,l[r*6+4]*u,r==2?"":" us");}
  for(r=0;r<4;r++)if(vxd_prof(0x64+r,o)&&o[1])
   fprintf(logf,"   %s: %lu, p50 %.1f p95 %.1f p99 %.1f max %.1f us\n",vn[r],(unsigned long)o[1],o[2]/mhz,o[3]/mhz,o[4]/mhz,o[5]/mhz);
  {DWORD f[8],h[8];memset(h,0,sizeof(h));if(queryprof)queryprof(h,8);if(fetch_state(f)&&h[4])fprintf(logf,"   %.1f fetched command words a triangle\n",(double)f[2]/h[4]);}}
}
/* note() for the HAL: a failing call also prints the driver's side */
static int hnote(int dv,const char *s,HRESULT hr) {int ok=note(s,hr);if(!dv){if(!ok)hal_state("after it");else hal_events();}return ok;}
typedef struct {LPDIRECTDRAWSURFACE4 rt,tx;LPDIRECT3DDEVICE3 dev;LPDIRECT3DVIEWPORT3 vp;LPDIRECT3DMATERIAL3 mat;LPDIRECT3DLIGHT lt[4];LPDIRECT3DTEXTURE2 t2;} gfx;
static void teardown(gfx *g) {
 int k;
 for(k=0;k<4;k++)if(g->lt[k]){if(g->vp)IDirect3DViewport3_DeleteLight(g->vp,g->lt[k]);IDirect3DLight_Release(g->lt[k]);}
 if(g->t2)IDirect3DTexture2_Release(g->t2);if(g->tx)IDirectDrawSurface4_Release(g->tx);if(g->mat)IDirect3DMaterial3_Release(g->mat);
 if(g->vp){if(g->dev)IDirect3DDevice3_DeleteViewport(g->dev,g->vp);IDirect3DViewport3_Release(g->vp);}
 if(g->dev)IDirect3DDevice3_Release(g->dev);if(g->rt)IDirectDrawSurface4_Release(g->rt);
 memset(g,0,sizeof(*g));
}
static int setup(int dv,gfx *g) {
 devpick *pk=dv?&ref:&hal;DDSURFACEDESC2 s;D3DVIEWPORT2 v;D3DMATERIAL m;D3DMATERIALHANDLE hm;D3DLIGHT l[4];D3DMATRIX id,proj;int k,x,y;
 memset(g,0,sizeof(*g));
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT;s.dwWidth=W;s.dwHeight=H;
 s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE|(dv?DDSCAPS_SYSTEMMEMORY:DDSCAPS_VIDEOMEMORY);
 s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);s.ddpfPixelFormat.dwFlags=DDPF_RGB;s.ddpfPixelFormat.dwRGBBitCount=16;
 s.ddpfPixelFormat.dwRBitMask=0xF800;s.ddpfPixelFormat.dwGBitMask=0x7E0;s.ddpfPixelFormat.dwBBitMask=0x1F;
 if(!hnote(dv,"CreateSurface(800x600 R5G6B5 render target)",IDirectDraw4_CreateSurface(dd4,&s,&g->rt,0)))return 0;
 if(!hnote(dv,"CreateDevice",IDirect3D3_CreateDevice(d3d3,&pk->guid,g->rt,&g->dev,0)))return 0;
 if(!hnote(dv,"CreateViewport",IDirect3D3_CreateViewport(d3d3,&g->vp,0))||!hnote(dv,"AddViewport",IDirect3DDevice3_AddViewport(g->dev,g->vp)))return 0;
 memset(&v,0,sizeof(v));v.dwSize=sizeof(v);v.dwWidth=W;v.dwHeight=H;v.dvClipX=-1;v.dvClipY=1;v.dvClipWidth=2;v.dvClipHeight=2;v.dvMinZ=0;v.dvMaxZ=1;
 if(!hnote(dv,"SetViewport2",IDirect3DViewport3_SetViewport2(g->vp,&v))||!hnote(dv,"SetCurrentViewport",IDirect3DDevice3_SetCurrentViewport(g->dev,g->vp)))return 0;
 /* material, ambient */
 if(!hnote(dv,"CreateMaterial",IDirect3D3_CreateMaterial(d3d3,&g->mat,0)))return 0;
 memset(&m,0,sizeof(m));m.dwSize=sizeof(m);m.diffuse.r=m.diffuse.g=m.diffuse.b=1;m.ambient.r=m.ambient.g=m.ambient.b=1;m.specular.r=m.specular.g=m.specular.b=1;m.power=30;m.dwRampSize=16;
 if(!hnote(dv,"SetMaterial",IDirect3DMaterial3_SetMaterial(g->mat,&m))||!hnote(dv,"GetHandle(material)",IDirect3DMaterial3_GetHandle(g->mat,g->dev,&hm)))return 0;
 IDirect3DDevice3_SetLightState(g->dev,D3DLIGHTSTATE_MATERIAL,hm);IDirect3DDevice3_SetLightState(g->dev,D3DLIGHTSTATE_AMBIENT,0x20202020UL);
 /* texture: 64x64 R5G6B5, managed on the HAL as WinTune's D3DTextr makes it; system memory on the RGB device */
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT|DDSD_TEXTURESTAGE;s.dwWidth=64;s.dwHeight=64;
 s.ddsCaps.dwCaps=DDSCAPS_TEXTURE|(dv?DDSCAPS_SYSTEMMEMORY:0);s.ddsCaps.dwCaps2=dv?0:DDSCAPS2_TEXTUREMANAGE;
 s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);s.ddpfPixelFormat.dwFlags=DDPF_RGB;s.ddpfPixelFormat.dwRGBBitCount=16;
 s.ddpfPixelFormat.dwRBitMask=0xF800;s.ddpfPixelFormat.dwGBitMask=0x7E0;s.ddpfPixelFormat.dwBBitMask=0x1F;
 if(!hnote(dv,"CreateSurface(texture 64x64 R5G6B5)",IDirectDraw4_CreateSurface(dd4,&s,&g->tx,0)))return 0;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
 if(!hnote(dv,"Lock(texture)",IDirectDrawSurface4_Lock(g->tx,0,&s,DDLOCK_WAIT|DDLOCK_WRITEONLY,0)))return 0;
 for(y=0;y<64;y++)for(x=0;x<64;x++)((WORD*)((BYTE*)s.lpSurface+y*s.lPitch))[x]=disco_tex[y*64+x];
 IDirectDrawSurface4_Unlock(g->tx,0);
 if(!hnote(dv,"QueryInterface(IDirect3DTexture2)",IDirectDrawSurface4_QueryInterface(g->tx,&IID_IDirect3DTexture2,(void**)&g->t2)))return 0;
 /* states, in WinTune's order */
 IDirect3DDevice3_SetTextureStageState(g->dev,0,D3DTSS_COLORARG1,D3DTA_TEXTURE);IDirect3DDevice3_SetTextureStageState(g->dev,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE);
 IDirect3DDevice3_SetTextureStageState(g->dev,0,D3DTSS_COLOROP,D3DTOP_MODULATE);IDirect3DDevice3_SetTextureStageState(g->dev,0,D3DTSS_MINFILTER,D3DTFN_LINEAR);
 IDirect3DDevice3_SetTextureStageState(g->dev,0,D3DTSS_MAGFILTER,D3DTFG_LINEAR);
 IDirect3DDevice3_SetRenderState(g->dev,D3DRENDERSTATE_DITHERENABLE,TRUE);IDirect3DDevice3_SetRenderState(g->dev,D3DRENDERSTATE_SPECULARENABLE,TRUE);
 memset(&id,0,sizeof(id));id._11=id._22=id._33=id._44=1;IDirect3DDevice3_SetTransform(g->dev,D3DTRANSFORMSTATE_WORLD,&id);
 memset(&proj,0,sizeof(proj));proj._11=proj._22=1.0007966f;proj._33=1.010101f;proj._34=1;proj._43=-1.010101f;IDirect3DDevice3_SetTransform(g->dev,D3DTRANSFORMSTATE_PROJECTION,&proj);
 for(k=0;k<4;k++){if(!hnote(dv,"CreateLight",IDirect3D3_CreateLight(d3d3,&g->lt[k],0))||!hnote(dv,"AddLight",IDirect3DViewport3_AddLight(g->vp,g->lt[k])))return 0;}
 /* the frame's lights and camera (the vertices were animated before this call) */
 lights_at(l);for(k=0;k<4;k++)IDirect3DLight_SetLight(g->lt[k],&l[k]);
 IDirect3DDevice3_SetTransform(g->dev,D3DTRANSFORMSTATE_VIEW,&view);
 return 1;
}
static void print_draws(void) {
 static DWORD o[1+32*40];DWORD n=querydraws(o,0x40000000UL|(sizeof(o)/sizeof(o[0]))),i; /* driver v10.321: capture off again, so later timings carry none of it */int k;
 fprintf(logf,"   draw states the HAL sent: %lu\n",n?(unsigned long)o[0]:0UL);
 for(i=0;n&&i*40+1<n;i++){const DWORD *r=o+1+i*40;
  fprintf(logf,"   #%lu: %lu packets, %lu triangles; tex %lu fmt %lu %lux%lu; z %03lX blend %06lX alpha %04lX mask/dither %03lX fog %04lX; program %lu kflags %lX:",(unsigned long)i,(unsigned long)r[1],(unsigned long)r[2],
   (unsigned long)r[8],(unsigned long)r[9],(unsigned long)(r[10]>>16),(unsigned long)(r[10]&0xFFFF),(unsigned long)r[12],(unsigned long)r[13],(unsigned long)r[14],(unsigned long)r[15],(unsigned long)r[16],(unsigned long)r[17],(unsigned long)r[18]);
  for(k=0;k<8;k++)fprintf(logf," %08lX",(unsigned long)r[32+k]);
  fprintf(logf,"\n      vertex 0 colour %08lX specular %08lX; diffuse min %08lX max %08lX, specular max %08lX\n",(unsigned long)r[20],(unsigned long)r[21],(unsigned long)r[28],(unsigned long)r[29],(unsigned long)r[30]);}
}
/* one scene of the set; the image is read back even when a call failed (have[][] = it was read). Returns 1 when every call succeeded. */
static int scene(int dv,gfx *g,int set,int first) {
 static const D3DMATRIX wfloor={10,0,0,0, 0,10,0,0, 0,0,10,0, -5,-5,-5,1};
 static const D3DMATRIX wwx={-4.371139e-07f,10,0,0, -10,-4.371139e-07f,0,0, 0,0,10,0, 5,-5,-5,1};
 static const D3DMATRIX wwz={10,0,0,0, 0,-4.371139e-07f,-10,0, 0,10,-4.371139e-07f,0, -5,-5,5,1};
 DDSURFACEDESC2 s;D3DMATRIX id;D3DRECT rect;DWORD o0[4];int ok=1,x,y,walls=set==0||set==1||set==4,sphere=set==0||set==2||set==4,cubes=set==0||set==3||set==4;
 memset(&id,0,sizeof(id));id._11=id._22=id._33=id._44=1;
 rect.x1=0;rect.y1=0;rect.x2=W;rect.y2=H;
 if(!dv&&first&&querydraws)querydraws(o0,0x80000001UL); /* clear the HAL's draw records */
 prof_start();
 ok&=hnote(dv,"Clear2",IDirect3DViewport3_Clear2(g->vp,1,&rect,D3DCLEAR_TARGET,0,1.0f,0));
 if(!hnote(dv,"BeginScene",IDirect3DDevice3_BeginScene(g->dev)))return 0;
 IDirect3DDevice3_SetTexture(g->dev,0,0);
 if(walls){
  IDirect3DDevice3_SetTransform(g->dev,D3DTRANSFORMSTATE_WORLD,(LPD3DMATRIX)&wfloor);ok&=hnote(dv,"DrawIndexedPrimitive(floor)",IDirect3DDevice3_DrawIndexedPrimitive(g->dev,D3DPT_TRIANGLELIST,D3DFVF_VERTEX,grid,11664,gidx,68694,0));
  IDirect3DDevice3_SetTransform(g->dev,D3DTRANSFORMSTATE_WORLD,(LPD3DMATRIX)&wwx);ok&=hnote(dv,"DrawIndexedPrimitive(wall x)",IDirect3DDevice3_DrawIndexedPrimitive(g->dev,D3DPT_TRIANGLELIST,D3DFVF_VERTEX,grid,11664,gidx,68694,0));
  IDirect3DDevice3_SetTransform(g->dev,D3DTRANSFORMSTATE_WORLD,(LPD3DMATRIX)&wwz);ok&=hnote(dv,"DrawIndexedPrimitive(wall z)",IDirect3DDevice3_DrawIndexedPrimitive(g->dev,D3DPT_TRIANGLELIST,D3DFVF_VERTEX,grid,11664,gidx,68694,0));}
 IDirect3DDevice3_SetTexture(g->dev,0,g->t2);IDirect3DDevice3_SetTransform(g->dev,D3DTRANSFORMSTATE_WORLD,&id);
 if(sphere)ok&=hnote(dv,"DrawIndexedPrimitive(sphere)",IDirect3DDevice3_DrawIndexedPrimitive(g->dev,D3DPT_TRIANGLELIST,D3DFVF_VERTEX,sph,20002,sidx,120000,0));
 if(cubes)ok&=hnote(dv,"DrawIndexedPrimitive(cube)",IDirect3DDevice3_DrawIndexedPrimitive(g->dev,D3DPT_TRIANGLELIST,D3DFVF_VERTEX,cube,24,cidx,36,0));
 ok&=hnote(dv,"EndScene",IDirect3DDevice3_EndScene(g->dev));
 if(!dv)prof_print();else fprintf(logf,"   time %lu ms\n",(unsigned long)(GetTickCount()-prof_t0));
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
 if(!hnote(dv,"Lock(target)",IDirectDrawSurface4_Lock(g->rt,0,&s,DDLOCK_WAIT|DDLOCK_READONLY,0)))return 0;
 for(y=0;y<H;y++)for(x=0;x<W;x++){WORD p=((WORD*)((BYTE*)s.lpSurface+y*s.lPitch))[x];unsigned char *q=&img[dv][set][(y*W+x)*3];
  q[0]=(unsigned char)(((p>>11)&31)*255/31);q[1]=(unsigned char)(((p>>5)&63)*255/63);q[2]=(unsigned char)((p&31)*255/31);}
 IDirectDrawSurface4_Unlock(g->rt,0);have[dv][set]=1;
 if(!dv&&first&&querydraws)print_draws(); /* what the HAL received for the full frame */
 return ok;
}
static void save_bmp(const char *name,const unsigned char *rgb);
/* v1.1: WinTune draws frame after frame on one device; so do these five scenes (everything, walls, sphere, cube, everything again). A
 * scene with a failing call is logged with the driver's side and drawn once more on a new device, so each set still gets an image. */
static void render(int dv) {
 devpick *pk=dv?&ref:&hal;gfx g;int set;
 fprintf(logf,"== %s\n",pk->name);
 if(!pk->found){fprintf(logf,"   not enumerated\n");failures++;return;}
 if(!setup(dv,&g)){teardown(&g);return;}
 for(set=0;set<SETS;set++) {
  fprintf(logf,"-- scene %d on this device: %s\n",set+1,set_name[set]);
  if(scene(dv,&g,set,set==0))continue;
  if(!dv)hal_state("after the scene");
  fprintf(logf,"-- %s again, on a new device\n",set_name[set]);
  teardown(&g);if(!setup(dv,&g)){teardown(&g);return;}
  if(!scene(dv,&g,set,0)&&!dv)hal_state("after the scene");
 }
 if(!dv)hal_state("at the end");
 teardown(&g);
 if(have[dv][0]){save_bmp(dv?"DISCOREF.BMP":"DISCOHAL.BMP",img[dv][0]);fprintf(logf,"   wrote %s\n",dv?"DISCOREF.BMP":"DISCOHAL.BMP");}
}
static void save_bmp(const char *name,const unsigned char *rgb) {
 FILE *f=fopen(name,"wb");BITMAPFILEHEADER fh;BITMAPINFOHEADER ih;int y,x;unsigned char row[W*3];
 if(!f)return;memset(&fh,0,sizeof(fh));memset(&ih,0,sizeof(ih));
 fh.bfType=0x4D42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+W*H*3;
 ih.biSize=sizeof(ih);ih.biWidth=W;ih.biHeight=H;ih.biPlanes=1;ih.biBitCount=24;ih.biSizeImage=W*H*3;
 fwrite(&fh,sizeof(fh),1,f);fwrite(&ih,sizeof(ih),1,f);
 for(y=H-1;y>=0;y--){for(x=0;x<W;x++){const unsigned char *p=rgb+(y*W+x)*3;row[x*3]=p[2];row[x*3+1]=p[1];row[x*3+2]=p[0];}fwrite(row,W*3,1,f);}
 fclose(f);
}
static void compare(int set,const char *what) {
 const unsigned char *a=img[0][set],*b=img[1][set];unsigned long cov=0,bad=0,hal_on=0,ref_on=0,k;double sum[3]={0,0,0},sa[3]={0,0,0},sb[3]={0,0,0};
 for(k=0;k<(unsigned long)W*H;k++){const unsigned char *p=a+k*3,*q=b+k*3;int c,hb=p[0]|p[1]|p[2],rb=q[0]|q[1]|q[2],big=0;
  if(hb)hal_on++;if(rb)ref_on++;if(!hb&&!rb)continue;cov++;
  for(c=0;c<3;c++){int d=(int)p[c]-(int)q[c];sum[c]+=d;sa[c]+=p[c];sb[c]+=q[c];if(d>24||d<-24)big=1;}
  if(big)bad++;}
 fprintf(logf,"%-12s covered %6lu px (HAL %6lu, RGB %6lu); differing by more than 24: %6lu (%5.1f%%); mean HAL %3.0f %3.0f %3.0f, RGB %3.0f %3.0f %3.0f: %s\n",what,cov,hal_on,ref_on,bad,cov?100.0*bad/cov:0,
  cov?sa[0]/cov:0,cov?sa[1]/cov:0,cov?sa[2]/cov:0,cov?sb[0]/cov:0,cov?sb[1]/cov:0,cov?sb[2]/cov:0,(cov&&bad*20<cov)?"PASS":"FAIL");
 if(!(cov&&bad*20<cov))failures++;
 (void)sum;
}
/* ---- v1.2: VXDISCO --wintune [runs] [ocx]: the original WinTunDD.ocx's IwtDisco2.DiscoPrimary (WinTune's own sphere test, unmodified,
 * not registered), invoked through its type library as the earlier host does; before and after each run the HAL's counters, event log
 * and VxD state, and after each run the draw states the HAL sent. The OCX is taken from the given path, the current directory or the
 * system directory. ---- */
static const GUID clsid_disco={0x35231483UL,0xE29D,0x11D2,{0xA8,0xC1,0x00,0xA0,0xC9,0x11,0x9E,0x22}};
static const GUID iid_cf={0x00000001UL,0,0,{0xC0,0,0,0,0,0,0,0x46}},iid_disp={0x00020400UL,0,0,{0xC0,0,0,0,0,0,0,0x46}};
#define VTF(o,i) ((*(void ***)(o))[i]) /* entry i of a COM object's function table */
/* v1.3: the original WinTunDD.ocx is carried at the end of VXDISCO.EXE (build.py appends it, then "VXWTOCX1" and its length); it is
 * written to the Windows TEMP directory and loaded from there */
static int extract_ocx(char *path) {
 char self[300];FILE *f,*o;long end;unsigned char tail[12];DWORD len;static unsigned char buf[262144];
 if(!GetModuleFileNameA(0,self,sizeof(self))||(f=fopen(self,"rb"))==0)return 0;
 fseek(f,0,SEEK_END);end=ftell(f);
 if(end<12||fseek(f,end-12,SEEK_SET)||fread(tail,1,12,f)!=12||memcmp(tail,"VXWTOCX1",8)){fclose(f);return 0;}
 memcpy(&len,tail+8,4);
 if(!len||len>sizeof(buf)||(long)len>end-12||fseek(f,end-12-(long)len,SEEK_SET)||fread(buf,1,len,f)!=len){fclose(f);return 0;}
 fclose(f);
 if(!GetTempPathA(200,path))strcpy(path,"C:\\");
 strcat(path,"VXWTDD.OCX");
 if((o=fopen(path,"wb"))==0)return 0;
 if(fwrite(buf,1,len,o)!=len){fclose(o);return 0;}
 fclose(o);return 1;
}
static int wintune(int runs,const char *given) {
 char path[300];HMODULE ocx=0,ole=0,aut=0;HRESULT (WINAPI *getcls)(const GUID *,const GUID *,void **);HRESULT (WINAPI *coinit)(void *);
 HRESULT (WINAPI *loadtl)(const WCHAR *,void **);void (WINAPI *freestr)(void *);void *fac=0,*disp=0,*tl=0,*ti=0;WCHAR wpath[300],wname[16];
 HRESULT hr;unsigned n,j;int k,run;
 if(given&&given[0]){strncpy(path,given,sizeof(path)-1);path[sizeof(path)-1]=0;ocx=LoadLibraryA(path);}
 if(!ocx&&extract_ocx(path))ocx=LoadLibraryA(path);
 if(!ocx){fprintf(logf,"WinTunDD.ocx could not be loaded\n");failures++;return 0;}
 {char full[300];if(!GetModuleFileNameA(ocx,full,sizeof(full)))strcpy(full,path);fprintf(logf,"   WinTunDD.ocx: %s\n",full);MultiByteToWideChar(CP_ACP,0,full,-1,wpath,300);}
 ole=LoadLibraryA("OLE32.DLL");aut=LoadLibraryA("OLEAUT32.DLL");
 coinit=ole?(HRESULT (WINAPI *)(void *))GetProcAddress(ole,"CoInitialize"):0;
 loadtl=aut?(HRESULT (WINAPI *)(const WCHAR *,void **))GetProcAddress(aut,"LoadTypeLib"):0;
 freestr=aut?(void (WINAPI *)(void *))GetProcAddress(aut,"SysFreeString"):0;
 getcls=(HRESULT (WINAPI *)(const GUID *,const GUID *,void **))GetProcAddress(ocx,"DllGetClassObject");
 if(!coinit||!loadtl||!freestr||!getcls){fprintf(logf,"   OLE32/OLEAUT32 or DllGetClassObject missing\n");failures++;return 0;}
 note("CoInitialize",coinit(0));
 if(!note("DllGetClassObject(IwtDisco2)",getcls(&clsid_disco,&iid_cf,&fac)))return 0;
 hr=((HRESULT (WINAPI *)(void *,void *,const GUID *,void **))VTF(fac,3))(fac,0,&iid_disp,&disp);
 ((ULONG (WINAPI *)(void *))VTF(fac,2))(fac);
 if(!note("IClassFactory::CreateInstance(IDispatch)",hr))return 0;
 if(!note("LoadTypeLib(WinTunDD.ocx)",loadtl(wpath,&tl)))goto out;
 n=((unsigned (WINAPI *)(void *))VTF(tl,3))(tl);
 for(j=0;j<n&&j<64&&!ti;j++){WCHAR *name=0;char a[64];
  if(SUCCEEDED(((HRESULT (WINAPI *)(void *,int,WCHAR **,void *,void *,void *))VTF(tl,9))(tl,(int)j,&name,0,0,0))&&name){
   WideCharToMultiByte(CP_ACP,0,name,-1,a,sizeof(a),0,0);freestr(name);
   if(!strcmp(a,"IwtDisco2"))((HRESULT (WINAPI *)(void *,unsigned,void **))VTF(tl,4))(tl,j,&ti);}}
 if(!ti){fprintf(logf,"   IwtDisco2 not in the type library\n");failures++;goto out;}
 for(run=1;run<=runs;run++) {
  WCHAR *pn=wname;long id=0;struct {void *a,*b;unsigned c,d;} dp={0,0,0,0};unsigned char result[16],ex[32];unsigned bad=0;DWORD t0,o0[4];float v;
  MultiByteToWideChar(CP_ACP,0,"DiscoPrimary",-1,wname,16);
  if(!note("GetIDsOfNames(DiscoPrimary)",((HRESULT (WINAPI *)(void *,WCHAR **,unsigned,long *))VTF(ti,10))(ti,&pn,1,&id)))break;
  fprintf(logf,"== WinTune DiscoPrimary, run %d of %d\n",run,runs);
  if(querydraws&&run==1)querydraws(o0,0x80000001UL); /* the first run's draw states; later runs are timed without capture */
  hal_state("before");
  prof_start();memset(result,0,sizeof(result));memset(ex,0,sizeof(ex));t0=GetTickCount();
  hr=((HRESULT (WINAPI *)(void *,void *,long,unsigned short,void *,void *,void *,unsigned *))VTF(ti,11))(ti,disp,id,1,&dp,result,ex,&bad);
  fprintf(logf,"   DiscoPrimary = %08lX after %lu ms\n",(unsigned long)hr,(unsigned long)(GetTickCount()-t0));
  prof_print();
  if(FAILED(hr))failures++;
  memcpy(&v,result+8,4);
  if(*(unsigned short*)result==4)fprintf(logf,"   result (WinTune's score) %.4f\n",v);else fprintf(logf,"   result type %u\n",(unsigned)*(unsigned short*)result);
  for(k=1;k<4;k++){void **b=(void **)(ex+4+4*(k-1));if(*b)freestr(*b);}
  hal_state("after");
  if(querydraws&&run==1)print_draws();
 }
out:
 if(ti)((ULONG (WINAPI *)(void *))VTF(ti,2))(ti);
 if(tl)((ULONG (WINAPI *)(void *))VTF(tl,2))(tl);
 if(disp)((ULONG (WINAPI *)(void *))VTF(disp,2))(disp);
 return 1;
}
/* v2.7: built with driver v10.238 (no change of its own). */
/* v2.8: built with driver v10.239 (no change of its own). */
/* v2.9: built with driver v10.240 (no change of its own). */
/* v2.10: built with driver v10.241 (no change of its own). */
/* v2.11: built with driver v10.242 (no change of its own). */
/* v2.12: built with driver v10.243 (no change of its own). */
/* v2.13: built with driver v10.244 (no change of its own). */
/* v2.14: built with driver v10.245 (no change of its own). */
/* v2.15 (driver v10.321): latency distributions (HAL DrawPrimitives2, submit IOCTL, busy retries; the VxD's lock, idle wait, wait for the
 * draw before, submit) and command words a triangle after each timing; the HAL's draw records only for the first frame or run. */
/* v2.6: built with driver v10.237 (no change of its own). */
/* v2.5: strip counts (TEXDBG 0xB1, cleared by 0xB0) in the time split; a tuning row with strips sent as triangles (0xAF). */
/* v2.4: built with driver v10.235 (192-vertex packets; no change of its own). */
/* v2.3: built with driver v10.234 (no change of its own). */
/* v2.2: the VxD's draw submit split (TEXDBG 0xAC/0xAD). */
/* v2.1: the state cache (ACCEL_D3D_STATE_CACHE, TEXDBG 0xAA/0xAB): draws sent without state, the fetch path's time split (0xA8/0xA9), a tuning row without it. */
/* v2.0: the driver's asynchronous draws (ACCEL_D3D_ASYNC, TEXDBG 0xA6/0xA7): shown, counted, and switched in the tuning pass. */
/* v1.9: each state line shows how often the DirectDraw HAL had to open its VxD handle again (v10.230). */
/* v1.8: the 2D callbacks' names in the event lines; each state line also shows the VxD's 2D loss state (TEXDBG 0xA5). Lock/Unlock
 * events carry the access answer: 0-4 the VxD's (3 = lost), 8xxxxx01 the DeviceIoControl failed (error in bits 23:8). */
/* v1.7: the time split shows the VxD's fetched packets; the tuning pass also runs the frame with the command fetcher off. */
/* v1.6: the full frame again on the HAL with the VxD's engine-write and idle-wait settings varied (TEXDBG 0x78/0x79 posted writes,
 * 0x80 + n idle reads), each compared with the RGB device's frame and timed; the settings in force before are put back.
 * v1.7: and with the command fetcher on and off (TEXDBG 0xA1/0xA2) */
static void tuning(void) {
 static const DWORD posted[7]={1,1,1,1,1,1,1},reads[7]={8,8,2,8,8,8,32},fetch[7]={1,1,1,1,1,0,0},async[7]={1,1,1,1,0,0,0},cache[7]={1,1,1,0,0,0,0},shw[7]={1,0,1,1,1,1,1};DWORD o[8],was_p=1,was_r=32,was_f=0,was_a=0,was_c=1,was_s=1;gfx g;int k,hf,hc,hs;char what[96];
 if(!texdbg(0x7A,o)||(o[0]!=0x78&&o[0]!=0x79)||!texdbg(0x7C,o)||o[0]!=0x7C){fprintf(logf,"== tuning: the VxD has no switches for it\n");return;}
 texdbg(0x7A,o);was_p=o[0]==0x78;texdbg(0x7C,o);was_r=o[1];hf=fetch_state(o);was_f=o[0]==0xA1;was_a=(o[7]>>31)&1;hc=texdbg(0xA8,o)&&o[0]==0xA8;was_c=o[1];hs=texdbg(0xB1,o)&&o[0]==0xAE;was_s=o[1];
 if(was_r<1||was_r>32){fprintf(logf,"== tuning skipped: ACCEL_D3D_IDLE_READS %lu could not be put back afterwards\n",(unsigned long)was_r);return;}
 fprintf(logf,"== tuning: the full frame on the HAL with other engine settings (before: fetch %lu, async %lu, state cache %lu, posted %lu, idle reads %lu)\n",(unsigned long)was_f,(unsigned long)was_a,(unsigned long)was_c,(unsigned long)was_p,(unsigned long)was_r);
 if(!setup(0,&g)){teardown(&g);return;}
 for(k=0;k<7;k++) {
  if(fetch[k]&&!hf)continue;
  texdbg(posted[k]?0x78:0x79,o);texdbg(0x80+reads[k],o);if(hf){texdbg(fetch[k]?0xA1:0xA2,o);texdbg(async[k]?0xA6:0xA7,o);}if(hc)texdbg(cache[k]?0xAA:0xAB,o);if(hs)texdbg(shw[k]?0xAE:0xAF,o);
  sprintf(what,"fetch %lu, async %lu, state cache %lu, strips %s, posted %lu, reads %2lu",(unsigned long)(hf?fetch[k]:0),(unsigned long)(hf?async[k]:0),(unsigned long)(hc?cache[k]:0),hs?(shw[k]?"hw":"as triangles"):"-",(unsigned long)posted[k],(unsigned long)reads[k]);fprintf(logf,"-- %s\n",what);
  have[0][0]=0;if(scene(0,&g,0,0)&&have[0][0])compare(0,what);
 }
 texdbg(was_p?0x78:0x79,o);texdbg(0x80+(was_r>=1&&was_r<=32?was_r:32),o);if(hf){texdbg(was_f?0xA1:0xA2,o);texdbg(was_a?0xA6:0xA7,o);}if(hc)texdbg(was_c?0xAA:0xAB,o);if(hs)texdbg(was_s?0xAE:0xAF,o);
 teardown(&g);
}
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcA(h,m,w,l);}
int main(int argc,char **argv) {
 int wt=argc>1&&!strcmp(argv[1],"--wintune"),frame=argc>1&&!wt?atoi(argv[1]):30,f;HMODULE ddlib,halmod;HRESULT (WINAPI *create)(GUID *,LPDIRECTDRAW *,IUnknown *);LPDIRECTDRAW dd=0;WNDCLASSA wc;HWND win;
 logf=fopen("VXDISCO.TXT","w");if(!logf)return 1;
 if(frame<0)frame=0;if(frame>10000)frame=10000;
 if(wt)fprintf(logf,"VXDISCO v2.19 (driver v10.329): WinTune 98's own Direct3D test (WinTunDD.ocx DiscoPrimary) with the driver's side logged\n");
 else fprintf(logf,"VXDISCO v2.19 (driver v10.329): WinTune 98 Direct3D \"Disco\" scene, frame %d, HAL against the RGB device\n",frame);
 rc=cos(0.17453292519943295);rs=sin(0.17453292519943295);
 build();
 for(f=0;f<=frame;f++){frame_move();if(f<frame)t_now+=0.05f;}
 fprintf(logf,"t = %.4f; sphere vertex 2 (%.4f %.4f %.4f); cube vertex 0 (%.4f %.4f %.4f); light dirs (%.3f %.3f %.3f)\n",t_now,sph[2].x,sph[2].y,sph[2].z,cube[0].x,cube[0].y,cube[0].z,s1,s2,s3);
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="VXDISCO";RegisterClassA(&wc);
 win=CreateWindowA(wc.lpszClassName,"VXDISCO",WS_OVERLAPPEDWINDOW,40,40,200,150,0,0,wc.hInstance,0);
 ddlib=LoadLibraryA("DDRAW.DLL");create=ddlib?(HRESULT (WINAPI *)(GUID *,LPDIRECTDRAW *,IUnknown *))GetProcAddress(ddlib,"DirectDrawCreate"):0;
 if(!win||!create){fprintf(logf,"no window or DirectDrawCreate\n");failures++;goto done;}
 if(!note("DirectDrawCreate",create(0,&dd,0))||!note("QueryInterface(IDirectDraw4)",IDirectDraw_QueryInterface(dd,&iid_dd4,(void**)&dd4))||
    !note("SetCooperativeLevel(NORMAL)",IDirectDraw4_SetCooperativeLevel(dd4,win,DDSCL_NORMAL))||
    !note("QueryInterface(IDirect3D3)",IDirectDraw4_QueryInterface(dd4,&IID_IDirect3D3,(void**)&d3d3))||!note("EnumDevices",IDirect3D3_EnumDevices(d3d3,enum_dev,0)))goto done;
 halmod=GetModuleHandleA("VXDDRAW.DLL");if(!halmod)halmod=LoadLibraryA("VXDDRAW.DLL");
 querydraws=halmod?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(halmod,"VX_D3D_QueryDraws"):0;
 queryev=halmod?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(halmod,"VX_D3D_QueryEvents"):0;
 querystats=halmod?(DWORD (WINAPI *)(vx_d3d_diag *))GetProcAddress(halmod,"VX_D3D_QueryStats"):0;
 queryprof=halmod?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(halmod,"VX_D3D_QueryProfile"):0;
 querylat=halmod?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(halmod,"VX_D3D_QueryLatency"):0; /* v2.15 */
 endpoint=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);
 {DWORD o[1+16*6];ev_shown=queryev&&queryev(o,sizeof(o)/sizeof(o[0]))?o[0]:0;} /* events from before this run are not repeated */
 fprintf(logf,"   HAL hooks: draw records %s, events %s (%lu before this run), counters %s\n",querydraws?"yes":"no",queryev?"yes":"no",(unsigned long)ev_shown,querystats?"yes":"no");
 hal_state("at the start");
 measure_mhz();
 {DWORD o[8],r[8];int a=texdbg(0x7A,o)&&(o[0]==0x78||o[0]==0x79),b=texdbg(0x7C,r)&&r[0]==0x7C;
  fprintf(logf,"   CPU %.0f MHz; engine writes %s; idle after ",mhz,a?(o[0]==0x78?"posted (ACCEL_D3D_POSTED 1)":"each followed by a status read (ACCEL_D3D_POSTED 0)"):"?");
  if(b)fprintf(logf,"%lu idle status reads in a row (ACCEL_D3D_IDLE_READS)\n",(unsigned long)r[1]);else fprintf(logf,"32 idle status reads in a row (older VxD)\n");
  if(fetch_state(o)){DWORD c[8];int hc=texdbg(0xA8,c)&&c[0]==0xA8;fprintf(logf,"   draws %s (ACCEL_D3D_FETCH %lu, ACCEL_D3D_ASYNC %lu, ACCEL_D3D_STATE_CACHE %s), command block at %08lX\n",o[0]==0xA1&&o[6]?"through the command fetcher":"through the engine registers",(unsigned long)(o[0]==0xA1),(unsigned long)(o[7]>>31),hc?(c[1]?"1":"0"):"-",(unsigned long)o[6]);}
  else fprintf(logf,"   no command fetcher (older VxD)\n");}
 if(wt){ /* the OCX makes its own DirectDraw objects; ours goes first (the HAL DLL stays loaded through halmod) */
  if(d3d3){IDirect3D3_Release(d3d3);d3d3=0;}if(dd4){IDirectDraw4_Release(dd4);dd4=0;}if(dd){IDirectDraw_Release(dd);dd=0;}
  {int runs=argc>2?atoi(argv[2]):2;if(runs<1)runs=1;if(runs>10)runs=10;wintune(runs,argc>3?argv[3]:0);}
  goto done;}
 render(0);render(1);
 fprintf(logf,"== comparison, HAL against RGB (a pixel differs when a channel is more than 24 apart; a set passes under 5%% differing)\n");
 for(f=0;f<SETS;f++)if(have[0][f]&&have[1][f])compare(f,set_name[f]);else{fprintf(logf,"%-12s not compared (no %s image)\n",set_name[f],have[0][f]?"RGB":"HAL");failures++;}
 if(hal.found&&have[1][0])tuning();
done:
 if(endpoint!=INVALID_HANDLE_VALUE)CloseHandle(endpoint);
 if(d3d3)IDirect3D3_Release(d3d3);if(dd4)IDirectDraw4_Release(dd4);if(dd)IDirectDraw_Release(dd);if(win)DestroyWindow(win);
 fprintf(logf,"Finished: failures=%lu\n",failures);fclose(logf);printf("VXDISCO.TXT written: %lu failure(s).\n",failures);return failures?1:0;
}
