/* VXFORMAT v1.2 (v10.477): Direct3D 8 texture formats on this driver: advertised, created, sampled right, upload speed, and the cost of a
 * texture working set larger than video memory (the texture manager's eviction and re-upload).
 *
 * 1. Direct3D 8's view: the display format, GetDeviceCaps (DevCaps: TEXTUREVIDEOMEMORY 0x200, TEXTURENONLOCALVIDMEM 0x1000,
 *    TEXTURESYSTEMMEMORY 0x100; TextureCaps; stages and simultaneous textures; size limits), GetAvailableTextureMem.
 * 2. For each format: CheckDeviceFormat (texture), CreateTexture 64x64 MANAGED (one level and a 7-level chain) and DEFAULT (video
 *    memory), then a lock to write four quarters (red, green, blue, white; L8: four greys; DXT: solid blocks), a 64x64 quad sampling it,
 *    and the four quarters read back from the back buffer. P8 / A8P8, A8 and the bump formats are checked and created only.
 * 3. Upload: a 256x256 MANAGED texture in R5G6B5, X8R8G8B8 and DXT1: 16 rounds of lock, fill, unlock, draw (the draw is what moves a
 *    managed texture into video memory), timed apart: the lock-fill-unlock and the draw-to-end-of-scene, MB/s of texture bytes.
 * 4. Working set: N 256x256 MANAGED textures (DXT1, R5G6B5, X8R8G8B8), every one drawn each frame as an 8x8 quad, N doubling from 8
 *    until creation fails or 64 MB of texture: ms a frame (20 frames after 3 warm-up frames) and AvailableTextureMem. The N where ms a
 *    frame jumps is where the set stops fitting in video memory and the texture manager starts re-uploading every frame.
 * 5. Video memory capacity: 256x256 R5G6B5 DEFAULT textures created until CreateTexture fails (up to 2048): count x 128 KB.
 *
 * No Direct3D 8 headers: the interfaces are called through their vtables (d3d8.h's order, as VXRTT). Results to the screen and to
 * VXFORMAT.LOG beside the program. Run it on the desktop (16 or 32 bpp); it opens a 256x256 window for about a minute.
 * "VXFORMAT nosweep" skips 4 and 5. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef unsigned long U32;
#define VT(o,i) (((void**)(*(void***)(o)))[i])
typedef HRESULT (__stdcall *F0)(void*);
typedef HRESULT (__stdcall *F1)(void*,U32);
typedef HRESULT (__stdcall *F1P)(void*,void*);
typedef HRESULT (__stdcall *F2)(void*,U32,U32);
typedef HRESULT (__stdcall *F2P)(void*,U32,void*);
typedef HRESULT (__stdcall *F3P)(void*,U32,U32,void*);
typedef HRESULT (__stdcall *FCREATETEX)(void*,U32,U32,U32,U32,U32,U32,void**);
typedef HRESULT (__stdcall *FCHECK)(void*,U32,U32,U32,U32,U32,U32);
typedef HRESULT (__stdcall *FLOCK)(void*,U32,void*,void*,U32);
typedef HRESULT (__stdcall *FDRAWUP)(void*,U32,U32,const void*,U32);
typedef U32 (__stdcall *FU32)(void*);
static FILE *logf;
static void say(const char *f,...) {static char b[4096];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);if(logf){fputs(b,logf);fflush(logf);}} /* v10.474: bounded */

#define FMT_DXT1 0x31545844UL
#define FMT_DXT3 0x33545844UL
#define FMT_DXT5 0x35545844UL
typedef struct {U32 code;const char *name;int bytes;int kind;} FMT; /* kind: 0 RGB(A), 1 L8, 2 DXT, 3 checked/created only */
static const FMT fmts[]={
 {21,"A8R8G8B8",4,0},{22,"X8R8G8B8",4,0},{23,"R5G6B5",2,0},{24,"X1R5G5B5",2,0},{25,"A1R5G5B5",2,0},{26,"A4R4G4B4",2,0},
 {50,"L8",1,1},{51,"A8L8",2,3},{28,"A8",1,3},{41,"P8",1,3},{40,"A8P8",2,3},
 {FMT_DXT1,"DXT1",0,2},{FMT_DXT3,"DXT3",0,2},{FMT_DXT5,"DXT5",0,2},
 {60,"V8U8",2,3},{61,"L6V5U5",2,3},{62,"X8L8V8U8",4,3}};
#define NFMT (sizeof(fmts)/sizeof(fmts[0]))
static const char *fname(U32 f) {U32 k;for(k=0;k<NFMT;k++)if(fmts[k].code==f)return fmts[k].name;switch(f){case 80:return "D16";case 75:return "D24S8";}return "?";}

enum {D3D_REL=2,D3D_MODE=8,D3D_FMTCHECK=10,D3D_CAPS=13,D3D_CREATEDEV=15};
enum {DEV_AVAILMEM=4,DEV_PRESENT=15,DEV_BACKBUF=16,DEV_CREATETEX=20,DEV_BEGIN=34,DEV_END=35,DEV_CLEAR=36,DEV_SETRS=50,DEV_SETTEX=61,DEV_SETTSS=63,
      DEV_DRAWUP=72,DEV_SETVS=76};
enum {TEX_LOCK=16,TEX_UNLOCK=17,SURF_LOCK=9,SURF_UNLOCK=10};
typedef struct {U32 w,h,refresh,format;} MODE;
typedef struct {U32 bw,bh,bfmt,bcount,ms,swap;HWND wnd;U32 windowed,autoz,zfmt,flags,rate,interval;} PP;
typedef struct {long pitch;void *bits;} LR;
typedef struct {float x,y,z,w,u,v;} VT1;

static void *dev;static U32 bbfmt;static int fails;static LARGE_INTEGER qpf;
static double now_ms(void) {LARGE_INTEGER t;QueryPerformanceCounter(&t);return (double)t.QuadPart*1000.0/(double)qpf.QuadPart;}
static HRESULT clear(U32 flags,U32 color) {return ((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,flags,color,1.0f,0);}
static void rs(U32 s,U32 v) {((F2)VT(dev,DEV_SETRS))(dev,s,v);}
static void tss(U32 s,U32 v) {((F3P)VT(dev,DEV_SETTSS))(dev,0,s,(void*)v);}
static void settex(void *t) {((F2P)VT(dev,DEV_SETTEX))(dev,0,t);}
static void present(void) {((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);}
static U32 availmem(void) {return ((FU32)VT(dev,DEV_AVAILMEM))(dev);}
static void quad_t(float x0,float y0,float s) {
 VT1 v[6];int k;static const float u[6]={0,1,0,1,1,0},t[6]={0,0,1,0,1,1};
 for(k=0;k<6;k++){v[k].x=x0+u[k]*s-0.5f;v[k].y=y0+t[k]*s-0.5f;v[k].z=0.5f;v[k].w=1.0f;v[k].u=u[k];v[k].v=t[k];}
 ((F1)VT(dev,DEV_SETVS))(dev,0x104);((FDRAWUP)VT(dev,DEV_DRAWUP))(dev,4,2,v,sizeof(VT1));
}
static void sample_state(void) { /* stage 0: the texture's colour, no blend, no Z, no lighting, no mip filter, point sampling */
 rs(7,0);rs(14,0);rs(22,1);rs(27,0);rs(15,0);rs(137,0);rs(28,0);tss(1,2);tss(2,2);tss(4,2);tss(5,2);tss(16,1);tss(17,1);tss(18,0);tss(13,3);tss(14,3);
}
/* the back buffer's pixel at x, y as X8R8G8B8 (R5G6B5 widened); 0xFFFFFFFF when the lock fails */
static U32 bb_pixel(int x,int y) {
 void *bb=0;LR lr;U32 v=0xFFFFFFFFUL;
 if(((HRESULT(__stdcall*)(void*,U32,U32,void**))VT(dev,DEV_BACKBUF))(dev,0,0,&bb)||!bb)return v;
 if(!((HRESULT(__stdcall*)(void*,LR*,void*,U32))VT(bb,SURF_LOCK))(bb,&lr,0,0x10)) {
  unsigned char *row=(unsigned char*)lr.bits+y*lr.pitch;
  if(bbfmt==23){unsigned short p=((unsigned short*)row)[x];v=((U32)(p>>11)*255/31)<<16|((U32)((p>>5)&63)*255/63)<<8|((U32)(p&31)*255/31);}
  else if(bbfmt==24||bbfmt==25){unsigned short p=((unsigned short*)row)[x];v=((U32)((p>>10)&31)*255/31)<<16|((U32)((p>>5)&31)*255/31)<<8|((U32)(p&31)*255/31);}
  else v=((U32*)row)[x]&0xFFFFFFUL;
  ((F0)VT(bb,SURF_UNLOCK))(bb);
 }
 ((F0)VT(bb,D3D_REL))(bb);return v;
}
static int near_rgb(U32 a,U32 b,int tol) {int k;for(k=0;k<24;k+=8){int d=(int)((a>>k)&255)-(int)((b>>k)&255);if(d<-tol||d>tol)return 0;}return 1;}

/* one texel / one 4x4 block of colour c (X8R8G8B8) in format f */
static unsigned short to565(U32 c) {return (unsigned short)(((c>>8)&0xF800)|((c>>5)&0x7E0)|((c>>3)&0x1F));}
static void put_texel(const FMT *f,unsigned char *p,U32 c) {
 switch(f->code) {
 case 21:case 22:*(U32*)p=c|0xFF000000UL;break;
 case 23:*(unsigned short*)p=to565(c);break;
 case 24:case 25:*(unsigned short*)p=(unsigned short)(0x8000|((c>>9)&0x7C00)|((c>>6)&0x3E0)|((c>>3)&0x1F));break;
 case 26:*(unsigned short*)p=(unsigned short)(0xF000|((c>>12)&0xF00)|((c>>8)&0xF0)|((c>>4)&0xF));break;
 case 50:*p=(unsigned char)(c&255);break;
 }
}
static void put_block(const FMT *f,unsigned char *p,U32 c) {
 unsigned short c565=to565(c);
 if(f->code==FMT_DXT3){memset(p,0xFF,8);p+=8;}
 else if(f->code==FMT_DXT5){p[0]=0xFF;p[1]=0xFF;memset(p+2,0,6);p+=8;}
 /* DXT1 colour block: colour0 = colour1 = c, every index 0 (colour0 is read in both the 4- and the 3-colour mode) */
 p[0]=(unsigned char)(c565&255);p[1]=(unsigned char)(c565>>8);p[2]=p[0];p[3]=p[1];p[4]=p[5]=p[6]=p[7]=0;
}
/* fill a w x h level: four quarters (kind 0: red, green, blue, white; L8: 255, 128, 64, 0; DXT: the same four colours as solid blocks) */
static const U32 quarter_rgb[4]={0xFF0000UL,0x00FF00UL,0x0000FFUL,0xFFFFFFUL};
static const U32 quarter_l8[4]={0xFFFFFFUL,0x808080UL,0x404040UL,0x000000UL};
static int fill_level(void *tex,const FMT *f,U32 level,U32 w,U32 h) {
 LR lr;U32 x,y,q;unsigned char *row;
 if(((FLOCK)VT(tex,TEX_LOCK))(tex,level,&lr,0,0))return 0;
 if(f->kind==2) {
  /* DXT levels are written packed (block rows of bw x bs bytes, as RenderWare and XP's driver do): this driver reports lPitch as the
   * level's linear size (v10.199), so the lock's pitch is not a row stride */
  U32 bw=w<4?1:w/4,bh=h<4?1:h/4,bs=f->code==FMT_DXT1?8:16;
  for(y=0;y<bh;y++){row=(unsigned char*)lr.bits+y*bw*bs;for(x=0;x<bw;x++){q=(y>=bh/2?2:0)+(x>=bw/2?1:0);put_block(f,row+x*bs,quarter_rgb[q]);}}
 } else {
  /* v1.2: the top and bottom halves' rows built once and copied into every row (board, v1.1: the texel-by-texel loop was 95-154 ns a
   * texel, 6-10 ms of a 256 x 256 upload round, against a row copy's 0.1-0.5 ms: the upload line measured this program's loop) */
  static unsigned char hr[2][2048*4];U32 half;
  for(half=0;half<2;half++)for(x=0;x<w;x++){q=(half?2:0)+(x>=w/2?1:0);put_texel(f,hr[half]+x*f->bytes,f->kind==1?quarter_l8[q]:quarter_rgb[q]);}
  for(y=0;y<h;y++){row=(unsigned char*)lr.bits+y*lr.pitch;memcpy(row,hr[y>=h/2?1:0],w*f->bytes);}
 }
 ((F1)VT(tex,TEX_UNLOCK))(tex,level);return 1;
}
static void *create_tex(U32 w,U32 h,U32 levels,U32 fmt,U32 pool,HRESULT *hr) {void *t=0;*hr=((FCREATETEX)VT(dev,DEV_CREATETEX))(dev,w,h,levels,0,fmt,pool,&t);return (*hr||!t)?0:t;}
static void release(void *o) {if(o)((F0)VT(o,D3D_REL))(o);}

/* 2: one format */
static void test_format(void *d3d,U32 dispfmt,const FMT *f) {
 HRESULT chk,hr1,hrm,hrd;void *t1,*tm,*td;int k,wrong=0;const U32 *want=f->kind==1?quarter_l8:quarter_rgb;
 chk=((FCHECK)VT(d3d,D3D_FMTCHECK))(d3d,0,1,dispfmt,0,3,f->code);
 t1=create_tex(64,64,1,f->code,1,&hr1);tm=create_tex(64,64,7,f->code,1,&hrm);td=create_tex(64,64,1,f->code,0,&hrd);
 say("%-9s check %-8s  managed %-8s  7 levels %-8s  default %-8s",f->name,chk?"NO":"yes",hr1?"NO":"yes",hrm?"NO":"yes",hrd?"NO":"yes");
 if(chk||hr1||hrm||hrd)say("  (%08lX %08lX %08lX %08lX)",(U32)chk,(U32)hr1,(U32)hrm,(U32)hrd);
 if(f->kind==3||!t1){say("%s\n",f->kind==3?"  (not sampled)":"");release(t1);release(tm);release(td);if(f->kind!=3&&chk==0)fails++;return;}
 if(!fill_level(t1,f,0,64,64)){say("  lock FAILED\n");fails++;release(t1);release(tm);release(td);return;}
 ((F0)VT(dev,DEV_BEGIN))(dev);clear(1,0xFF000000UL);sample_state();settex(t1);quad_t(16,16,64);settex(0);((F0)VT(dev,DEV_END))(dev);
 for(k=0;k<4;k++){U32 got=bb_pixel(16+(k&1?48:8),16+(k&2?48:8));if(!near_rgb(got,want[k],f->bytes==2||f->kind==2||bbfmt!=22?20:4)){wrong++;say("\n   quarter %d sampled %06lX, want %06lX",k,got,want[k]);}}
 if(tm&&fill_level(tm,f,0,64,64)) { /* the chain's level 0 sampled with no mip filter: same picture expected */
  int w2=0;((F0)VT(dev,DEV_BEGIN))(dev);clear(1,0xFF000000UL);sample_state();settex(tm);quad_t(16,16,64);settex(0);((F0)VT(dev,DEV_END))(dev);
  for(k=0;k<4;k++){U32 got=bb_pixel(16+(k&1?48:8),16+(k&2?48:8));if(!near_rgb(got,want[k],20))w2++;}
  if(w2){wrong+=w2;say("\n   mip chain level 0: %d quarters wrong",w2);}
 }
 present();
 say("  sampled %s\n",wrong?"WRONG":"ok");fails+=wrong;
 release(t1);release(tm);release(td);
}

/* v1.1: where the upload round's time goes (board, v1.0: R5G6B5 10.4 ms for 128 KB, X8R8G8B8 6.7 ms for 256 KB): the Lock and the Unlock
 * apart from the fill; the fill as this program writes it (a call and a switch a texel) against a row copy (one row built, copied into
 * every row: the memory's own speed for the format's bytes); and whether the lock's pointer is in video memory (VX_DD_Vram's range) */
static DWORD (WINAPI *dd_vram)(DWORD *,DWORD *);
static void upload_parts(const FMT *f) {
 HRESULT hr;void *t=create_tex(256,256,1,f->code,1,&hr);int r;LR lr;double lk=0,fl=0,ul=0,rc=0,a;U32 bytes=256UL*256*f->bytes,y,x;
 static unsigned char rowb[256*4];DWORD vb=0,vs=0;int vid=-1;
 if(!t||f->kind!=0)return;
 for(x=0;x<256;x++)put_texel(f,rowb+x*f->bytes,quarter_rgb[x>=128?1:0]);
 if(dd_vram&&dd_vram(&vb,&vs)&&vb&&vs)vid=0;
 for(r=0;r<9;r++){double t0=now_ms(),t1,t2,t3,t4;
  if(((FLOCK)VT(t,TEX_LOCK))(t,0,&lr,0,0)){say("   %-8s lock FAILED\n",f->name);release(t);return;}
  t1=now_ms();if(vid>=0&&(DWORD)lr.bits-vb<vs)vid=1;
  for(y=0;y<256;y++){unsigned char *row=(unsigned char*)lr.bits+y*lr.pitch;for(x=0;x<256;x++)put_texel(f,row+x*f->bytes,quarter_rgb[(y>=128?2:0)+(x>=128?1:0)]);}
  t2=now_ms();
  for(y=0;y<256;y++)memcpy((unsigned char*)lr.bits+y*lr.pitch,rowb,256*f->bytes);
  t3=now_ms();((F1)VT(t,TEX_UNLOCK))(t,0);t4=now_ms();
  if(r){lk+=t1-t0;fl+=t2-t1;rc+=t3-t2;ul+=t4-t3;}}
 release(t);lk/=8;fl/=8;rc/=8;ul/=8;
 say("   %-8s Lock %.2f ms, this program's fill %.2f ms (%.0f ns a texel), a row copy %.2f ms (%.0f MB/s), Unlock %.2f ms; the lock's pointer %s\n",
  f->name,lk,fl,fl*1e6/65536.0,rc,rc>0?bytes/1048576.0/(rc/1000.0):0.0,ul,vid<0?"(video memory's range not known)":vid?"IN VIDEO MEMORY":"in system memory");
}
/* 3: upload speed */
static void test_upload(const FMT *f) {
 HRESULT hr;void *t=create_tex(256,256,1,f->code,1,&hr);int r;double a,b,c,lock_ms=0,draw_ms=0;U32 bytes=f->kind==2?(f->code==FMT_DXT1?32768UL:65536UL):256UL*256*f->bytes;
 if(!t){say("upload %-8s: CreateTexture 256x256 MANAGED %08lX\n",f->name,(U32)hr);return;}
 for(r=0;r<16;r++) {
  a=now_ms();if(!fill_level(t,f,0,256,256)){say("upload %-8s: lock FAILED\n",f->name);release(t);return;}
  b=now_ms();((F0)VT(dev,DEV_BEGIN))(dev);sample_state();settex(t);quad_t(8,8,8);settex(0);((F0)VT(dev,DEV_END))(dev);c=now_ms();
  if(r)lock_ms+=b-a,draw_ms+=c-b; /* the first round warms the managed copy */
 }
 present();lock_ms/=15;draw_ms/=15;
 say("upload %-8s 256x256 (%lu KB): lock+fill+unlock %.2f ms, draw+end %.2f ms a round: %.1f MB/s through the draw\n",f->name,bytes/1024,lock_ms,draw_ms,draw_ms>0?bytes/1048576.0/(draw_ms/1000.0):0.0);
}

/* 4: working set */
static void test_set(const FMT *f) {
 static void *tex[1024];U32 n,k,bytes=f->kind==2?(f->code==FMT_DXT1?32768UL:65536UL):256UL*256*f->bytes,made=0;HRESULT hr;int fr;double t0,t1;
 say("working set %s (256x256 MANAGED, %lu KB each; AvailableTextureMem before %lu KB)\n",f->name,bytes/1024,availmem()/1024);
 say("   N     MB  ms/frame  avail KB\n");
 for(n=8;n<=1024&&n*bytes<=64UL*1048576;n*=2) {
  for(;made<n;made++){tex[made]=create_tex(256,256,1,f->code,1,&hr);if(!tex[made]){say("   CreateTexture #%lu failed %08lX\n",made+1,(U32)hr);break;}
   if(!fill_level(tex[made],f,0,256,256)){say("   lock #%lu failed\n",made+1);release(tex[made]);tex[made]=0;break;}}
  if(made<n)break;
  for(fr=0;fr<23;fr++) {
   if(fr==3)t0=now_ms();
   ((F0)VT(dev,DEV_BEGIN))(dev);clear(1,0xFF000000UL);sample_state();
   for(k=0;k<n;k++){settex(tex[k]);quad_t((float)(k%30)*8+4,(float)((k/30)%30)*8+4,8);}
   settex(0);((F0)VT(dev,DEV_END))(dev);present();
  }
  t1=now_ms();
  say("%4lu %6.1f %9.2f %9lu\n",n,n*bytes/1048576.0,(t1-t0)/20.0,availmem()/1024);
 }
 for(k=0;k<made;k++)release(tex[k]);
}

/* 5: video memory capacity */
static void test_capacity(void) {
 static void *tex[2048];U32 n;HRESULT hr=0;
 for(n=0;n<2048;n++){tex[n]=create_tex(256,256,1,23,0,&hr);if(!tex[n])break;}
 say("DEFAULT-pool 256x256 R5G6B5 textures until CreateTexture fails: %lu (%lu MB; last result %08lX); AvailableTextureMem then %lu KB\n",n,n/8,(U32)hr,availmem()/1024);
 while(n--)release(tex[n]);
}

static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
int main(int argc,char **argv) {
 HMODULE d8;void *(__stdcall *create)(U32);void *d3d;MODE mode;static U32 caps[128];PP pp;HWND wnd;WNDCLASS wc;HRESULT hr;U32 k,dc;
 char path[MAX_PATH],*s;int sweep=!(argc>1&&!strcmp(argv[1],"nosweep"));
 QueryPerformanceFrequency(&qpf);
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXFORMAT.LOG");logf=fopen(path,"w");
 say("VXFORMAT v1.2 (driver .533): Direct3D 8 texture formats, upload speed and working-set cost\n");
 {char dll[MAX_PATH];UINT n=GetSystemDirectoryA(dll,MAX_PATH-16);HMODULE h=0; /* v1.1: the HAL by its full path (VXFETCH's way), before the device */
  if(n&&n<MAX_PATH-16){strcpy(dll+n,dll[n-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");h=LoadLibraryA(dll);}
  dd_vram=h?(DWORD (WINAPI *)(DWORD *,DWORD *))GetProcAddress(h,"VX_DD_Vram"):0;}
 d8=LoadLibrary("d3d8.dll");create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 if(!create){say("d3d8.dll / Direct3DCreate8 not found (DirectX 8 or later is needed)\n");return 1;}
 d3d=create(220);if(!d3d)d3d=create(120);
 if(!d3d){say("Direct3DCreate8 failed\n");return 1;}
 memset(&mode,0,sizeof(mode));hr=((F2P)VT(d3d,D3D_MODE))(d3d,0,&mode);
 say("display %lux%lu format %lu (%s): %08lX\n",mode.w,mode.h,mode.format,fname(mode.format),(U32)hr);
 hr=((F3P)VT(d3d,D3D_CAPS))(d3d,0,1,caps);dc=caps[7];
 say("GetDeviceCaps(HAL): %08lX\n",(U32)hr);
 say("  DevCaps %08lX: textures in video memory %s, non-local (AGP/system) video memory %s, system memory %s, DRAWPRIMITIVES2EX %s, HW T&L %s\n",dc,
  (dc&0x200)?"yes":"no",(dc&0x1000)?"yes":"NO",(dc&0x100)?"yes":"no",(dc&0x8000)?"yes":"no",(dc&0x10000)?"yes":"no");
 say("  TextureCaps %08lX (POW2 %s, SQUAREONLY %s, MIPMAP %s, CUBEMAP %s, PROJECTED %s)  filter %08lX  address %08lX  op caps %08lX\n",caps[15],
  (caps[15]&2)?"yes":"no",(caps[15]&0x20)?"yes":"no",(caps[15]&0x4000)?"yes":"no",(caps[15]&0x800)?"yes":"no",(caps[15]&0x400)?"yes":"no",caps[16],caps[19],caps[36]);
 say("  max texture %lux%lu, aspect %lu, blend stages %lu, simultaneous textures %lu, max anisotropy %lu\n",caps[22],caps[23],caps[26],caps[37],caps[38],caps[27]);
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXFORMAT";wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClass(&wc);
 wnd=CreateWindow("VXFORMAT","VXFORMAT",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,264,284,0,0,wc.hInstance,0);
 memset(&pp,0,sizeof(pp));pp.bw=256;pp.bh=256;pp.bfmt=mode.format;pp.bcount=1;pp.swap=3;pp.wnd=wnd;pp.windowed=1;pp.autoz=1;pp.zfmt=80;pp.flags=1;
 bbfmt=mode.format;
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);
 say("CreateDevice(HAL, 256x256 windowed, D16, software vertex processing): %08lX\n",(U32)hr);
 if(hr||!dev){((F0)VT(d3d,D3D_REL))(d3d);if(logf)fclose(logf);return 1;}
 say("AvailableTextureMem at start: %lu KB\n\n",availmem()/1024);
 say("format    advertised  CreateTexture 64x64                       sampled\n");
 for(k=0;k<NFMT;k++)test_format(d3d,mode.format,&fmts[k]);
 say("\n");
 test_upload(&fmts[2]);test_upload(&fmts[1]);test_upload(&fmts[11]);
 say("where an upload round's time goes (v1.1; MANAGED 256x256, 8 rounds after a warm-up; \"this program's fill\" is v1.1's texel loop, kept for comparison):\n");upload_parts(&fmts[2]);upload_parts(&fmts[1]);upload_parts(&fmts[0]); /* v1.1 */
 say("\n");
 if(sweep){test_set(&fmts[11]);test_set(&fmts[2]);test_set(&fmts[1]);say("\n");test_capacity();}
 say("AvailableTextureMem at end: %lu KB\n",availmem()/1024);
 release(dev);((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);
 say("%s: %d wrong\n",fails?"FAILED":"PASSED",fails);
 if(logf)fclose(logf);
 return fails?2:0;
}
