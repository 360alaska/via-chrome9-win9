/* VXTLP v1.14 (driver .553: GRID, HW and SW read from the arguments only; the CSVs beside the program). VXTLP v1.13 (.552: 30 warm-up frames, each
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
enum {DEV_PRESENT=15,DEV_BACKBUF=16,DEV_BEGIN=34,DEV_END=35,DEV_CLEAR=36,DEV_SETXF=37,DEV_SETMAT=42,DEV_SETLIGHT=44,DEV_LIGHTON=46,DEV_SETRS=50,
      DEV_SETTEX=61,DEV_SETTSS=63,DEV_CREATEVB=23,DEV_DP=70,DEV_DPUP=72,DEV_DIPUP=73,DEV_SETVS=76,DEV_STREAM=83};
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
static HRESULT clear(void) {return ((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,3,0xFF000000UL,1.0f,0);}
static void rs(U32 s,U32 v) {((F2)VT(dev,DEV_SETRS))(dev,s,v);}
static void tss(U32 st,U32 s,U32 v) {((F3P)VT(dev,DEV_SETTSS))(dev,st,s,(void*)v);}
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
/* v1.8: 6. the grid scene (see the top); sw: the software-processing device's picture kept for the hardware one's comparison */
typedef struct {float x,y,z,nx,ny,nz;U32 c;float u,v;} GV; /* FVF 0x152 */
#define GN 24
static int g_secs; /* v1.10: GRID, the seconds each device runs (0: 20 frames) */
static GV g_vb[(GN+1)*(GN+1)];static unsigned short g_ix[GN*GN*6];static U32 g_sw[WIN*WIN];static int g_have;
static void grid_frame(void *tx) {
 static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};float pr[16],w[16],mat[17],lt[26];int k;U32 seed=541;
 const float zn=1.0f,zf=200.0f;
 memset(pr,0,sizeof(pr));pr[0]=1.5f;pr[5]=2.0f;pr[10]=zf/(zf-zn);pr[11]=1.0f;pr[14]=-zn*zf/(zf-zn);
 memset(mat,0,sizeof(mat));mat[0]=mat[1]=mat[2]=mat[3]=1.0f;mat[4]=mat[5]=mat[6]=0.3f;mat[7]=1.0f;
 memset(lt,0,sizeof(lt));*(U32*)&lt[0]=3;lt[1]=lt[2]=lt[3]=lt[4]=1.0f;lt[9]=lt[10]=lt[11]=0.2f;lt[12]=1.0f;lt[16]=0.3f;lt[17]=-1.0f;lt[18]=0.2f;lt[19]=1000.0f;
 ((F0)VT(dev,DEV_BEGIN))(dev);clear();base_states();
 ((F2P)VT(dev,DEV_SETXF))(dev,2,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)pr);
 ((HRESULT(__stdcall*)(void*,void*))VT(dev,DEV_SETMAT))(dev,mat);((F2P)VT(dev,DEV_SETLIGHT))(dev,0,lt);((F2)VT(dev,DEV_LIGHTON))(dev,0,1);
 rs(137,1);rs(139,0xFF404040UL);rs(145,1);rs(147,0);rs(148,0);rs(22,1);
 rs(28,1);rs(34,0xFF6080A0UL);rs(35,0);rs(140,3);{float fs=10.0f,fe=150.0f;rs(36,*(U32*)&fs);rs(37,*(U32*)&fe);} /* linear vertex fog */
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,tx);tss(0,1,4);tss(0,2,2);tss(0,3,0);tss(0,4,2);tss(0,5,0);
 ((F1)VT(dev,DEV_SETVS))(dev,0x152);
 for(k=0;k<30;k++){float x,y,z;seed=seed*1103515245UL+12345UL;x=(float)((seed>>8)%600)/10.0f-30.0f;seed=seed*1103515245UL+12345UL;y=-2.0f-(float)((seed>>8)%60)/10.0f;
  seed=seed*1103515245UL+12345UL;z=(float)((seed>>8)%700)/10.0f-10.0f;
  memcpy(w,id,sizeof(w));w[12]=x;w[13]=y;w[14]=z;((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)w);
  ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DIPUP))(dev,4,0,(GN+1)*(GN+1),GN*GN*2,g_ix,101,g_vb,sizeof(GV));}
 ((F0)VT(dev,DEV_END))(dev);((F2P)VT(dev,DEV_SETTEX))(dev,0,0);rs(28,0);rs(137,0);((F2P)VT(dev,DEV_SETXF))(dev,256,(void*)id);((F2P)VT(dev,DEV_SETXF))(dev,3,(void*)id);
}
/* .552: keep measurement I/O outside the timed run. Nearest-rank p50/p95
 * describe complete grid_frame + Present + message-pump wall intervals.
 * Present is not a GPU timer or a scanout timestamp. */
#define GRID_SAMPLES 65536UL
static double grid_samples[GRID_SAMPLES];
static int grid_cmp(const void *a,const void *b) {double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y?1:0;}
static void test_grid(int sw) {
 void *tx=0;LR lr;int i,j,k,l;LARGE_INTEGER f,a,b,prev;FILE *csv;
 say("6. %s: 30 lit, fogged, textured 24x24 grids a frame (34,560 triangles), some across the near plane and off the screen\n",sw?"software (the runtime's T&L)":"hardware (the driver's T&L)");
 for(j=0;j<=GN;j++)for(i=0;i<=GN;i++){GV *g=&g_vb[j*(GN+1)+i];g->x=(float)i-GN/2;g->y=0;g->z=(float)j;g->nx=0.3f;g->ny=1.0f;g->nz=0.1f;g->c=0xFF806040UL;g->u=(float)i/GN;g->v=(float)j/GN;}
 for(k=0,j=0;j<GN;j++)for(i=0;i<GN;i++){unsigned short a0=(unsigned short)(j*(GN+1)+i);g_ix[k++]=a0;g_ix[k++]=(unsigned short)(a0+GN+1);g_ix[k++]=(unsigned short)(a0+1);g_ix[k++]=(unsigned short)(a0+1);g_ix[k++]=(unsigned short)(a0+GN+1);g_ix[k++]=(unsigned short)(a0+GN+2);}
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,8,8,1,0,22,1,&tx)||!tx){wrong("CreateTexture failed");return;}
 if(!((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(tx,16))(tx,0,&lr,0,0)){for(k=0;k<8;k++){U32 *r=(U32*)((char*)lr.bits+k*lr.pitch);for(i=0;i<8;i++)r[i]=((i^k)&1)?0xFFE0E0E0UL:0xFF909090UL;}((F1)VT(tx,17))(tx,0);}
 grid_frame(tx);l=read_bb(cur)?lit(cur):0;present();
 QueryPerformanceFrequency(&f);
 if(!f.QuadPart){wrong("no performance-counter frequency");((F0)VT(tx,D3D_REL))(tx);return;}
 for(i=0;i<30;i++){grid_frame(tx);present();} /* warmup, excluded from samples */
 QueryPerformanceCounter(&a);prev=a;
 for(k=0;;k++){MSG msg;if(g_secs){QueryPerformanceCounter(&b);if(f.QuadPart&&(b.QuadPart-a.QuadPart)>=f.QuadPart*g_secs)break;}else if(k==20)break;
  if((U32)k>=GRID_SAMPLES)break;
  grid_frame(tx);present();while(PeekMessage(&msg,0,0,0,PM_REMOVE))DispatchMessage(&msg);
  QueryPerformanceCounter(&b);grid_samples[k]=(double)(b.QuadPart-prev.QuadPart)*1000.0/f.QuadPart;prev=b;}
 QueryPerformanceCounter(&b);
 ((F0)VT(tx,D3D_REL))(tx);
 say("   lit %d; %d frames: %.2f ms a frame\n",l,k,f.QuadPart&&k?(double)(b.QuadPart-a.QuadPart)*1000.0/f.QuadPart/k:0.0);
 csv=beside(sw?"VXG_SW.CSV":"VXG_HW.CSV");
 if(csv){fprintf(csv,"frame,ms\n");for(i=0;i<k;i++)fprintf(csv,"%d,%.6f\n",i+1,grid_samples[i]);fclose(csv);}
 else wrong("could not write the frame-time CSV");
 if(k){qsort(grid_samples,k,sizeof(grid_samples[0]),grid_cmp);
  say("   warmup 30; p50 %.3f ms; p95 %.3f ms; CSV %s (%s + driver packing)\n",grid_samples[(k-1)/2],grid_samples[((U32)k*95+99)/100-1],sw?"VXG_SW.CSV":"VXG_HW.CSV",sw?"runtime T&L":"custom T&L");}
 if((U32)k==GRID_SAMPLES)say("   sample limit reached; run ended at 65536 frames\n");
 if(!l){wrong("the grids were not drawn");return;}
 if(sw){memcpy(g_sw,cur,sizeof(cur));g_have=1;}
 else if(g_have){int d=ndiff(g_sw,cur),ls=lit(g_sw);say("   against the software device's picture: %d pixels differ of %d lit\n",d,ls);
  if(d*100>ls){bmp("VXTLP_A.BMP",g_sw);bmp("VXTLP_B.BMP",cur);wrong("not the software device's picture (VXTLP_A.BMP software, VXTLP_B.BMP the driver's)");}}
}
static int make_device(U32 behaviour) {
 PP pp;HRESULT hr;
 memset(&pp,0,sizeof(pp));pp.bw=WIN;pp.bh=WIN;pp.bfmt=bbfmt;pp.bcount=1;pp.swap=3;pp.wnd=wnd;pp.windowed=1;pp.autoz=1;pp.zfmt=80;pp.flags=1;dev=0;
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,behaviour,&pp,&dev);
 say("   CreateDevice(HAL, %s vertex processing): %08lX\n",behaviour==0x20?"software":"hardware",(U32)hr);
 if(hr||!dev){dev=0;return 0;}return 1;
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
  if(!strstr(args(),"HW")){if(make_device(0x20)){test_grid(1);((F0)VT(dev,D3D_REL))(dev);dev=0;}else wrong("no software-processing device");}
  if(!strstr(args(),"SW")){if(make_device(0x40)){test_grid(0);((F0)VT(dev,D3D_REL))(dev);dev=0;}else wrong("no hardware-processing device (ACCEL_D3D_TNL off?)");}
  ((F0)VT(d3d,D3D_REL))(d3d);d3d=0;return;}
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
  test_grid(1); /* v1.8 */
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
  test_grid(0); /* v1.8 */
  if(six_tl("hardware vertex processing")&&have_sw){int d=ndiff(sw_ref,ref);say("   against the software device's picture: %d pixels differ\n",d);if(d){bmp("VXTLP_A.BMP",sw_ref);bmp("VXTLP_B.BMP",ref);wrong("not the software device's picture");}}
  ((F0)VT(dev,D3D_REL))(dev);dev=0;}
 ((F0)VT(d3d,D3D_REL))(d3d);d3d=0;
}
static void cpuid1(unsigned long *out); /* v1.9: CPUID leaf 1 into out[0..3] (the driver's bytes: its build refuses the mnemonic) */
#pragma aux cpuid1 = 0x53 0xB8 0x01 0x00 0x00 0x00 0x0F 0xA2 0x89 0x06 0x89 0x5E 0x04 0x89 0x4E 0x08 0x89 0x56 0x0C 0x5B parm [esi] modify exact [eax ecx edx];
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
int main(void) {
 char path[MAX_PATH],*s;WNDCLASS wc;
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXTLP.LOG");logf=fopen(path,"w");
 say("VXTLP v1.14 (driver .553): the driver's T&L (ACCEL_D3D_TNL); GRID: median and 95th percentile, a CSV of the frame times\n");
 {char dll[MAX_PATH];UINT n=GetSystemDirectoryA(dll,MAX_PATH-16);HMODULE h=0;
  if(n&&n<MAX_PATH-16){strcpy(dll+n,dll[n-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");h=LoadLibraryA(dll);}
  hal_phases=h?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(h,"VX_D3D_QueryPhases"):0;}
 if(!q_read(qa)){say("VX_D3D_QueryPhases with 302 words: not there (a driver before .536)\nFAILED\n");if(logf)fclose(logf);return 2;}
 {HANDLE v=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);DWORD code=0xFFFFFE50UL,o[4],got=0;memset(o,0,sizeof(o)); /* v1.4 */
  if(v!=INVALID_HANDLE_VALUE){if(!DeviceIoControl(v,0x85500012UL,&code,4,o,16,&got,0)||got!=16||o[1]!=0x56584433UL)o[2]=0;CloseHandle(v);}
  probe=(o[2]&0x800UL)&&((o[2]>>21)&1);}
 {HANDLE v=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);DWORD in=0x75,o[8],got=0;memset(o,0,sizeof(o));if(v!=INVALID_HANDLE_VALUE){DeviceIoControl(v,0x85500022UL,&in,4,o,32,&got,0);CloseHandle(v);}say("the driver: build .%lu\n",o[1]);}
 say("the driver runs with ACCEL_D3D_TNL %d (%s)\n",probe,probe?"hardware T&L offered and done by the driver":"off, the default");
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXTLP";wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClass(&wc);
 wnd=CreateWindow("VXTLP","VXTLP",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,WIN+8,WIN+28,0,0,wc.hInstance,0);
 {unsigned long r[4]={0,0,0,0}; /* v1.9: the CPU's SSE and SSE2 (CPUID 1, EDX bits 25 and 26), as the driver's T&L checks them */
  cpuid1(r);
  say("the CPU: SSE %s, SSE2 %s: the driver's T&L uses %s\n",(r[3]>>25)&1?"yes":"no",(r[3]>>26)&1?"yes":"no",((r[3]>>25)&3)==3?"SSE":"the x87");}
 if(strstr(args(),"GRID"))g_secs=strstr(args(),"HW")?60:30; /* v1.10; v1.11: HW, the driver's alone, 60 s */
 if(!g_secs)test_d3d7();
 test_d3d8();
 DestroyWindow(wnd);
 say("%s: %d wrong\n",fails?"FAILED":"PASSED",fails);
 if(logf)fclose(logf);
 return fails?2:0;
}
