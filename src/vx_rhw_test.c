/* VXRHW v1.6 (driver .548): the list's vertices drawn by index, each triangle paired with the one 100 further on (sparse runs: the
 * pass-through hands them to the indexed loop and comes back, QueryPhases 313 grows), against the list drawn in order; 0 pixels may differ.
 * VXRHW v1.5 (driver .547): the on-screen grid with culling off is checked once (its token's range proven, the pass-through's spans
 * inside it not checked again: QueryPhases 311 grows, spans checked by the kernel stay at the proof's one).
 * VXRHW v1.4 (driver .546): the indexed loop with culling on (GTA3's path): the grid by index against the same triangles as a list with
 * cull mode CW, on the screen (the token's vertex range proven inside at once: QueryPhases 309 grows) and with its left columns wholly
 * off the screen (those triangles dropped early: QueryPhases 307 grows); the same pictures.
 * VXRHW v1.3 (driver .545): what .545 changes, on the board. (a) A textured primitive whose vertices all have rhw 0 is drawn with rhw 1
 * (.544 left it out, drop reason 2000: GTA3's textured coronas): eight textured quads and lines, rhw 0 against rhw 1, the same picture,
 * counted as drawn with rhw 1. Table fog and mixed rhw are still left out. (b) The indexed pass-through's span kernels (SSE2, QueryPhases
 * 302-306): the 16x16 grid by index against the same triangles drawn as a plain list (another path, the same words), on the screen and
 * with some vertices past the left edge (the span's check fails there: the per-triangle path takes the rest); the kernels' counts must grow.
 * VXRHW v1.1 (driver .533): v1.0 built its vertices as XYZRHW, DIFFUSE, SPECULAR (24 bytes) for FVF 0x144, which is XYZRHW, DIFFUSE, TEX1
 * (28 bytes: GTA3's streaks carry one coordinate pair): Direct3D refused every draw and nothing was drawn either way; the left-out count of
 * the mixed case went below 0. The template build's kept answers (.531, part 3) were removed in .533. v1.0 (driver .531): what .531 changes, on the board.
 * 1. Primitives whose vertices all have rhw 0 (GTA3 draws its car light streaks so: lines of FVF 0x144: position, rhw, diffuse, one coordinate pair; diffuse colour only). .530 left them
 *    out (drop reason 2000); .531 draws them with rhw 1 when nothing reads rhw (no texture on an active stage, no table fog, no w-buffer).
 *    Eight gradient quads and eight gradient lines, FVF 0x144, Direct3D's clipping off: six shots rhw 0, rhw 1, ... each against the first
 *    rhw 1: the same picture; the HAL's count of such primitives (QueryPhases 242) grows by 24 a frame. Then the cases still left out, each
 *    with nothing drawn and nothing counted: a texture bound, table fog on, and triangles with one vertex rhw 1 and two rhw 0.
 * 2. 384 vertices an indexed packet, always (ACCEL_D3D_IX384 removed): the 16x16 grid by index in one call is one packet a frame; six
 *    shots, the same picture.
 * Results to the screen and VXRHW.LOG beside the program. Run it on the desktop at 16 or 32 bpp; it opens a 256x256 window for about half a
 * minute. VXD3DMON must not be running. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "vx_smp.h"
#include "vx_native3d.h"

typedef unsigned long U32;
#define VT(o,i) (((void**)(*(void***)(o)))[i])
typedef HRESULT (__stdcall *F0)(void*);
typedef HRESULT (__stdcall *F1)(void*,U32);
typedef HRESULT (__stdcall *F2)(void*,U32,U32);
typedef HRESULT (__stdcall *F2P)(void*,U32,void*);
typedef HRESULT (__stdcall *F3P)(void*,U32,U32,void*);
typedef HRESULT (__stdcall *FCREATEVB)(void*,U32,U32,U32,U32,void**);
typedef HRESULT (__stdcall *FCREATEIB)(void*,U32,U32,U32,U32,void**);
typedef HRESULT (__stdcall *FBLOCK)(void*,U32,U32,unsigned char**,U32);
typedef HRESULT (__stdcall *FDP)(void*,U32,U32,U32);
typedef HRESULT (__stdcall *FDIP)(void*,U32,U32,U32,U32,U32);
typedef HRESULT (__stdcall *FSTREAM)(void*,U32,void*,U32);
typedef HRESULT (__stdcall *FINDICES)(void*,void*,U32);
enum {D3D_REL=2,D3D_MODE=8,D3D_CREATEDEV=15};
enum {DEV_PRESENT=15,DEV_BACKBUF=16,DEV_CREATEVB=23,DEV_CREATEIB=24,DEV_BEGIN=34,DEV_END=35,DEV_CLEAR=36,DEV_SETRS=50,DEV_SETTEX=61,DEV_SETTSS=63,
      DEV_DRAWPRIM=70,DEV_DIP=71,DEV_SETVS=76,DEV_SETSTREAM=83,DEV_SETINDICES=85};
enum {BUF_LOCK=11,BUF_UNLOCK=12,SURF_LOCK=9,SURF_UNLOCK=10};
typedef struct {U32 w,h,refresh,format;} MODE;
typedef struct {U32 bw,bh,bfmt,bcount,ms,swap;HWND wnd;U32 windowed,autoz,zfmt,flags,rate,interval;} PP;
typedef struct {long pitch;void *bits;} LR;
typedef struct {float x,y,z,w;U32 c,s;float u,v;} VC; /* FVF 0x1C4 (v1.2: GTA3's form, textured: the HAL's indexed packets take only XP's programs; v1.1's untextured FVF 0x44 never reached them) */
#define FVF 0x1C4
#define GC 16
#define NV ((GC+1)*(GC+1))
#define NT (GC*GC*2)
#define WIN 256

static FILE *logf;static int fails;
static void say(const char *f,...) {static char b[2048];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);if(logf){fputs(b,logf);fflush(logf);}}
static void *dev,*vb,*ib;static U32 bbfmt;
static DWORD (WINAPI *hal_phases)(DWORD *,DWORD);
static HRESULT clear(U32 flags,U32 color) {return ((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,flags,color,1.0f,0);}
static void rs(U32 s,U32 v) {((F2)VT(dev,DEV_SETRS))(dev,s,v);}
static void tss(U32 s,U32 v) {((F3P)VT(dev,DEV_SETTSS))(dev,0,s,(void*)v);}
static U32 seed=525;
static U32 rnd(void) {seed=seed*1103515245UL+12345UL;return (seed>>8)&0xFFFFFFUL;}
static VC verts[NV];static unsigned short idx[NT*3];
static void build_mesh(void) {
 U32 x,y,k=0;
 for(y=0;y<=GC;y++)for(x=0;x<=GC;x++){VC *v=&verts[y*(GC+1)+x];v->x=8.0f+x*15.0f+(x&&x<GC?(float)(rnd()%7)-3.0f:0.0f);v->y=8.0f+y*15.0f+(y&&y<GC?(float)(rnd()%7)-3.0f:0.0f);
  v->z=0.5f;v->w=1.0f;v->c=((rnd()&0xFF)<<24)|rnd()|0x202020UL; /* VXSHADOW: alpha varies (blending, the alpha test) */v->s=0xFF000000UL;v->u=x/4.0f;v->v=y/4.0f;}
 for(y=0;y<GC;y++)for(x=0;x<GC;x++){unsigned short a=(unsigned short)(y*(GC+1)+x),b=(unsigned short)(a+1),c=(unsigned short)(a+GC+1),d=(unsigned short)(c+1);
  if(rnd()&1){idx[k++]=a;idx[k++]=b;idx[k++]=c;}else{idx[k++]=a;idx[k++]=c;idx[k++]=b;}
  if(rnd()&1){idx[k++]=b;idx[k++]=d;idx[k++]=c;}else{idx[k++]=b;idx[k++]=c;idx[k++]=d;}}
}
static int make_buffers(void) {
 unsigned char *p;U32 k;
 if(((FCREATEVB)VT(dev,DEV_CREATEVB))(dev,NV*sizeof(VC),0,FVF,1,&vb)||!vb){say("CreateVertexBuffer failed\n");return 0;}
 if(((FCREATEIB)VT(dev,DEV_CREATEIB))(dev,NT*6,0,101,1,&ib)||!ib){say("CreateIndexBuffer failed\n");return 0;}
 if(((FBLOCK)VT(vb,BUF_LOCK))(vb,0,0,&p,0)){say("vertex buffer lock failed\n");return 0;}
 memcpy(p,verts,sizeof(verts));((F0)VT(vb,BUF_UNLOCK))(vb);
 if(((FBLOCK)VT(ib,BUF_LOCK))(ib,0,0,&p,0)){say("index buffer lock failed\n");return 0;}
 memcpy(p,idx,sizeof(idx));((F0)VT(ib,BUF_UNLOCK))(ib);
 return 1;
}
static void *tex;
/* v1.2: a 64x64 checker texture, MODULATE with the diffuse colour (VXFETCH's state): the XP program the indexed packets need */
static int make_one(void **t,U32 a,U32 b,U32 sh) {
 LR lr;U32 x,y;
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,20))(dev,64,64,1,0,22,1,t)||!*t){say("CreateTexture 64x64 failed\n");return 0;}
 if(((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(*t,16))(*t,0,&lr,0,0)){say("texture lock failed\n");return 0;}
 for(y=0;y<64;y++){U32 *row=(U32*)((unsigned char*)lr.bits+y*lr.pitch);for(x=0;x<64;x++)row[x]=((x>>sh)^(y>>sh))&1?a:b;}
 ((F1)VT(*t,17))(*t,0);return 1;
}
static int make_texture(void) {return make_one(&tex,0xFFE0E0E0UL,0xFF909090UL,3);}
static void tssn(U32 st,U32 s,U32 v) {((F3P)VT(dev,DEV_SETTSS))(dev,st,s,(void*)v);}
static void states(U32 cull) {rs(136,cull==2?0:1); /* v1.4: Direct3D's clipping off for the culled grid (GTA3's 41%): its off-screen triangles reach the HAL */rs(7,0);rs(14,0);rs(27,0);rs(15,0);rs(137,0);rs(28,0);rs(29,1);rs(9,2);rs(8,3);rs(22,cull);((F2P)VT(dev,DEV_SETTEX))(dev,0,tex);
 tss(1,4);tss(2,2);tss(3,0);tss(4,4);tss(5,2);tss(6,0);tss(16,1);tss(17,1);tss(18,0);tss(13,1);tss(14,1);
 ((F2P)VT(dev,DEV_SETTEX))(dev,1,0);tssn(1,1,1);tssn(1,4,1);}
static U32 fbits(float f) {U32 u;memcpy(&u,&f,4);return u;}
static int read_bb(U32 *out) {
 void *bb=0;LR lr;int ok=0;U32 x,y;
 if(((HRESULT(__stdcall*)(void*,U32,U32,void**))VT(dev,DEV_BACKBUF))(dev,0,0,&bb)||!bb)return 0;
 if(!((HRESULT(__stdcall*)(void*,LR*,void*,U32))VT(bb,SURF_LOCK))(bb,&lr,0,0x10)){
  for(y=0;y<WIN;y++){unsigned char *row=(unsigned char*)lr.bits+y*lr.pitch;
   for(x=0;x<WIN;x++){if(bbfmt==23){unsigned short p=((unsigned short*)row)[x];out[y*WIN+x]=((U32)(p>>11)<<19)|((U32)((p>>5)&63)<<10)|((U32)(p&31)<<3);}else out[y*WIN+x]=((U32*)row)[x]&0xFFFFFFUL;}}
  ((F0)VT(bb,SURF_UNLOCK))(bb);ok=1;}
 ((F0)VT(bb,D3D_REL))(bb);return ok;
}
static void present(void) {((HRESULT(__stdcall*)(void*,void*,void*,void*,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);}
static int differs(U32 a,U32 b) {int k;for(k=0;k<24;k+=8){int d=(int)((a>>k)&255)-(int)((b>>k)&255);if(d<-8||d>8)return 1;}return 0;}
static int lit(const U32 *p) {int n=0,k;for(k=0;k<WIN*WIN;k++)if(p[k])n++;return n;}
static int lit_rows(const U32 *p,int y0,int y1) {int n=0,k;for(k=y0*WIN;k<y1*WIN;k++)if(p[k])n++;return n;}
static U32 ref[WIN*WIN],cur[WIN*WIN];static int saved;
static int ndiff2(const U32 *a,const U32 *b) {int n=0,k;for(k=0;k<WIN*WIN;k++)if(differs(a[k],b[k]))n++;return n;}
static void bmp(const char *name,const U32 *p) {
 char path[MAX_PATH],*q;FILE *f;U32 y,x;unsigned char h[54];
 GetModuleFileName(0,path,sizeof(path));q=strrchr(path,'\\');if(q)strcpy(q+1,name);f=fopen(path,"wb");if(!f)return;
 memset(h,0,54);h[0]='B';h[1]='M';*(U32*)(h+2)=54+WIN*WIN*3;*(U32*)(h+10)=54;*(U32*)(h+14)=40;*(U32*)(h+18)=WIN;*(U32*)(h+22)=WIN;h[26]=1;h[28]=24;*(U32*)(h+34)=WIN*WIN*3;
 fwrite(h,1,54,f);for(y=WIN;y-->0;)for(x=0;x<WIN;x++){U32 c=p[y*WIN+x];unsigned char b3[3];b3[0]=(unsigned char)c;b3[1]=(unsigned char)(c>>8);b3[2]=(unsigned char)(c>>16);fwrite(b3,1,3,f);}
 fclose(f);
}
static DWORD qw(int i) {DWORD y[316];DWORD n=i<244?244:i<307?307:i<311?311:i<312?312:316;return hal_phases&&hal_phases(y,n)==n?y[i]:0;} /* v1.3: 302-306 (.545) */
static U32 g_cull=1; /* v1.4: the grid's cull mode */
static void draw_big(void) {
 ((F0)VT(dev,DEV_BEGIN))(dev);clear(3,0xFF000000UL);states(g_cull);((F1)VT(dev,DEV_SETVS))(dev,FVF);
 ((FSTREAM)VT(dev,DEV_SETSTREAM))(dev,0,vb,sizeof(VC));((FINDICES)VT(dev,DEV_SETINDICES))(dev,ib,0);
 ((FDIP)VT(dev,DEV_DIP))(dev,4,0,NV,0,NT);
 ((F0)VT(dev,DEV_END))(dev);
}
static void *lvb; /* v1.3: the grid's triangles as a plain list (NT*3 vertices) */
static unsigned short ord_ix[NT*3]; /* v1.6: the index order drawn (idx, or interleaved from both ends) */
static int fill_grid(int edge) { /* v1.3: the grid into vb (by index) and lvb (the list); edge: some vertices past the left edge; v1.6: 3 interleaved */
 unsigned char *p,*q;U32 k;static VC v[NV];
 for(k=0;k<NT;k++){U32 t=edge==3?((k&1)?NT-1-k/2:k/2):k;ord_ix[k*3]=idx[t*3];ord_ix[k*3+1]=idx[t*3+1];ord_ix[k*3+2]=idx[t*3+2];}
 if(((FBLOCK)VT(ib,BUF_LOCK))(ib,0,0,&p,0)){say("index buffer lock failed\n");fails++;return 0;}
 memcpy(p,ord_ix,sizeof(ord_ix));((F0)VT(ib,BUF_UNLOCK))(ib);
 memcpy(v,verts,sizeof(v));if(edge==1)for(k=0;k<NV;k+=GC+1)if((k/(GC+1))%3==1)v[k].x=-20.0f;
 if(edge==2)for(k=0;k<NV;k++)if(k%(GC+1)<4)v[k].x=-200.0f+v[k].x; /* v1.4: columns 0-3 wholly off the screen */
 if(!lvb&&(((FCREATEVB)VT(dev,DEV_CREATEVB))(dev,NT*3*sizeof(VC),0,FVF,1,&lvb)||!lvb)){say("CreateVertexBuffer (list) failed\n");fails++;return 0;}
 if(((FBLOCK)VT(vb,BUF_LOCK))(vb,0,0,&p,0)||((FBLOCK)VT(lvb,BUF_LOCK))(lvb,0,0,&q,0)){say("vertex buffer lock failed\n");fails++;return 0;}
 memcpy(p,v,sizeof(v));for(k=0;k<NT*3;k++)memcpy(q+k*sizeof(VC),&v[ord_ix[k]],sizeof(VC));
 ((F0)VT(vb,BUF_UNLOCK))(vb);((F0)VT(lvb,BUF_UNLOCK))(lvb);return 1;
}
static int shot_list(U32 *out) {
 ((F0)VT(dev,DEV_BEGIN))(dev);clear(3,0xFF000000UL);states(g_cull);((F1)VT(dev,DEV_SETVS))(dev,FVF);
 ((FSTREAM)VT(dev,DEV_SETSTREAM))(dev,0,lvb,sizeof(VC));((FDP)VT(dev,DEV_DRAWPRIM))(dev,4,0,NT);
 ((F0)VT(dev,DEV_END))(dev);if(!read_bb(out)){say("   back buffer lock failed\n");fails++;return 0;}present();return 1;
}
static int shot_any(U32 *out) {draw_big();if(!read_bb(out)){say("   back buffer lock failed\n");fails++;return 0;}present();return 1;}
static void frames_any(int nf) {int k;for(k=0;k<nf;k++){draw_big();present();}}
/* 1. rhw 0 */
typedef struct {float x,y,z,w;U32 c;float u,v;} TL; /* FVF 0x144: XYZRHW, DIFFUSE, TEX1 (GTA3's streaks); v1.1: the coordinate pair (v1.0 had a specular word: 24 bytes, not 28) */
#define TLF 0x144
#define NQ 8
#define NL 8
#define NTL (NQ*6+NL*2)
static void *tlv[4]; /* 0: every rhw 0, 1: every rhw 1, 2: each triangle's first vertex rhw 1, the rest 0 (mixed), 3: unused */
static U32 grad(U32 k,U32 j) {static const U32 c0[4]={0xFFFF2020UL,0xFF20FF20UL,0xFF2020FFUL,0xFFFFFF20UL};return c0[(k+j)&3]^((k*37UL)<<8&0xFF00UL);}
static int make_tl(void) {
 TL v[NTL];U32 k,j,n=0,f;unsigned char *p;
 for(k=0;k<NQ;k++){float x=12.0f+(k&3)*60.0f,y=12.0f+(k>>2)*60.0f;static const float qx[6]={0,48,0,48,48,0},qy[6]={0,0,48,0,48,48};
  for(j=0;j<6;j++){TL *t=&v[n++];t->x=x+qx[j]+(float)(k&1)*0.25f;t->y=y+qy[j];t->z=0.0f;t->w=0.0f;t->c=grad(k,j==4?3:j%3);t->u=qx[j]/48.0f;t->v=qy[j]/48.0f;}}
 for(k=0;k<NL;k++){float y=140.0f+k*13.0f;for(j=0;j<2;j++){TL *t=&v[n++];t->x=16.0f+j*(200.0f+k*3.0f)+(float)k;t->y=y+j*(float)(k*2);t->z=0.0f;t->w=0.0f;t->c=j?0xFF080000UL:0xFF310000UL+k*0x0400UL;t->u=(float)j;t->v=0.0f;}}
 for(f=0;f<3;f++){
  if(((FCREATEVB)VT(dev,DEV_CREATEVB))(dev,NTL*sizeof(TL),0,TLF,1,&tlv[f])||!tlv[f]){say("CreateVertexBuffer (TL) failed\n");return 0;}
  if(((FBLOCK)VT(tlv[f],BUF_LOCK))(tlv[f],0,0,&p,0)){say("vertex buffer lock failed\n");return 0;}
  for(k=0;k<NTL;k++){TL t=v[k];t.w=f==1?1.0f:(f==2&&k<NQ*6&&k%3==0)?1.0f:0.0f;memcpy(p+k*sizeof(TL),&t,sizeof(TL));}
  ((F0)VT(tlv[f],BUF_UNLOCK))(tlv[f]);}
 return 1;
}
static int tl_mode; /* 0 diffuse only, 1 a texture bound (stage 0 MODULATE), 2 table fog */
static int tl_set; /* the vertex buffer: tlv[tl_set] */
static void draw_tl(void) {
 ((F0)VT(dev,DEV_BEGIN))(dev);clear(3,0xFF000000UL);
 rs(7,0);rs(14,0);rs(27,0);rs(15,0);rs(137,0);rs(136,0);rs(22,1);rs(8,3);rs(9,2);rs(29,0);rs(26,0);rs(168,15);
 rs(28,tl_mode==2);if(tl_mode==2){rs(34,0x00406080UL);rs(35,3);rs(36,fbits(0.0f));rs(37,fbits(1.0f));}else rs(35,0);
 ((F2P)VT(dev,DEV_SETTEX))(dev,0,tl_mode==1?tex:0);((F2P)VT(dev,DEV_SETTEX))(dev,1,0);
 if(tl_mode==1){tss(1,4);tss(2,2);tss(3,0);tss(4,4);tss(5,2);tss(6,0);}else{tss(1,2);tss(2,0);tss(4,2);tss(5,0);}
 tssn(1,1,1);tssn(1,4,1);
 ((F1)VT(dev,DEV_SETVS))(dev,TLF);((FSTREAM)VT(dev,DEV_SETSTREAM))(dev,0,tlv[tl_set],sizeof(TL));
 ((FDP)VT(dev,DEV_DRAWPRIM))(dev,4,0,NQ*2);((FDP)VT(dev,DEV_DRAWPRIM))(dev,2,NQ*6,NL);
 ((F0)VT(dev,DEV_END))(dev);
}
static int shot_tl(U32 *out) {draw_tl();if(!read_bb(out)){say("   back buffer lock failed\n");fails++;return 0;}present();return 1;}
static void test_rhw(void) {
 int k,d[6],bad=0;DWORD a0,a1;
 if(!hal_phases||qw(0)!=0x431){say("VX_D3D_QueryPhases: not there (an older HAL)\n");fails++;return;}
 if(!make_tl())return;
 say("rhw 0, nothing reading it (8 gradient quads and 8 lines, FVF 0x144, clipping off):\n");
 tl_mode=0;
 for(k=0;k<6;k++){tl_set=k&1?0:1;if(!shot_tl(k?cur:ref))return;d[k]=k?ndiff2(ref,cur):0;if(d[k]){bad++;if(!saved){bmp("VXRHW_A.BMP",ref);bmp("VXRHW_B.BMP",cur);saved=1;}}}
 say("   pixels differing from the first rhw 1: rhw 0 %d, rhw 1 %d, rhw 0 %d, rhw 1 %d, rhw 0 %d; lit %d\n",d[1],d[2],d[3],d[4],d[5],lit(ref));
 if(!lit(ref)){say("   WRONG: nothing drawn with rhw 1\n");fails++;}
 if(bad){say("   WRONG: rhw 0 is drawn differently from rhw 1 (see VXRHW_A/B.BMP: A rhw 1, B the shot that differed)\n");fails++;}
 tl_set=0;a0=qw(242);for(k=0;k<10;k++){draw_tl();present();}a1=qw(242)-a0;
 say("   10 frames with rhw 0: %lu primitives drawn with rhw 1 (the HAL's count; %d expected)\n",a1,10*(NQ*2+NL));
 if(a1!=10UL*(NQ*2+NL)){say("   WRONG: not every rhw-0 primitive was counted as drawn\n");fails++;}
 /* v1.3 (.545): textured, rhw 0 against rhw 1 */
 say("rhw 0 with a texture bound (MODULATE), drawn with rhw 1 from .545:\n");
 tl_mode=1;bad=0;
 for(k=0;k<4;k++){tl_set=k&1?0:1;if(!shot_tl(k?cur:ref))return;d[k]=k?ndiff2(ref,cur):0;if(d[k]){bad++;if(!saved){bmp("VXRHW_A.BMP",ref);bmp("VXRHW_B.BMP",cur);saved=1;}}}
 say("   pixels differing from the first rhw 1: rhw 0 %d, rhw 1 %d, rhw 0 %d; lit %d\n",d[1],d[2],d[3],lit(ref));
 if(!lit(ref)){say("   WRONG: nothing drawn with rhw 1\n");fails++;}
 if(bad){say("   WRONG: textured rhw 0 is drawn differently from rhw 1 (VXRHW_A/B.BMP)\n");fails++;}
 tl_set=0;a0=qw(242);for(k=0;k<10;k++){draw_tl();present();}a1=qw(242)-a0;
 say("   10 frames with rhw 0: %lu primitives drawn with rhw 1 (%d expected)\n",a1,10*(NQ*2+NL));
 if(a1!=10UL*(NQ*2+NL)){say("   WRONG: not every textured rhw-0 primitive was drawn\n");fails++;}
 tl_mode=0;
 {static const char *const nm[2]={"table fog (linear)","one vertex rhw 1, two rhw 0"};int m;
  say("still left out (nothing drawn, nothing counted):\n");
  for(m=0;m<2;m++){int ln;tl_mode=m==0?2:0;tl_set=m==1?2:0;
   a0=qw(242);if(!shot_tl(cur))return;a1=qw(242)-a0;
   ln=m==1?lit_rows(cur,0,136):lit(cur); /* the mixed set changes only the triangles: its lines (rhw 0) are still drawn */
   say("   %-30s lit %d, counted %ld\n",nm[m],ln,m==1?(long)a1-NL:(long)a1); /* v1.1: signed */
   if(ln||(m==1?a1!=NL:a1!=0)){say("   WRONG: drawn or counted\n");fails++;}}
  tl_mode=0;tl_set=0;}
}
/* 2. 384 vertices an indexed packet */
static void test_big(void) {
 DWORD p0,pn;int k,d[6],bad=0;
 say("384 vertices an indexed packet (always from .531):\n");
 frames_any(2);p0=qw(241);frames_any(10);pn=qw(241)-p0;
 say("   the grid in one call (289 vertices, 512 triangles), 10 frames: %lu packets\n",pn);
 if(pn!=10){say("   WRONG: not one packet a frame\n");fails++;}
 for(k=0;k<6;k++){if(!shot_any(k?cur:ref))return;d[k]=k?ndiff2(ref,cur):0;if(d[k])bad++;}
 say("   six shots, pixels differing from the first: %d %d %d %d %d; lit %d\n",d[1],d[2],d[3],d[4],d[5],lit(ref));
 if(!lit(ref)){say("   WRONG: nothing drawn\n");fails++;}
 if(bad){say("   WRONG: the picture is not steady\n");fails++;}
 /* v1.3 (.545): the span kernels */
 {int e;say("the indexed pass-through's span kernels (.545): SSE2 %s\n",qw(302)?"on":"off (the C; a processor without SSE2 or an older HAL)");
  for(e=0;e<2;e++){DWORD s0,v0,c0,n0;int dd;
   if(!fill_grid(e))return;
   s0=qw(303);v0=qw(304);c0=qw(305);n0=qw(306);{DWORD k0=qw(311);if(!shot_any(cur))return;
    if(!e){say("   (.547) spans inside the proven range not checked again: %lu\n",qw(311)-k0);if(qw(311)==k0){say("      WRONG: the proven grid's spans were checked again\n");fails++;}}}
   say("   %s: spans checked %lu (%lu vertices four at a time), converted %lu (%lu vertices)\n",e?"vertices past the left edge":"on the screen",qw(303)-s0,qw(304)-v0,qw(305)-c0,qw(306)-n0);
   if(!shot_list(ref))return;dd=ndiff2(ref,cur);
   say("      against the same triangles as a list: %d pixels differ; lit %d\n",dd,lit(cur));
   if(dd){if(!saved){bmp("VXRHW_A.BMP",ref);bmp("VXRHW_B.BMP",cur);saved=1;}say("      WRONG: the indexed grid differs from the list (VXRHW_A list, B indexed)\n");fails++;}
   if(!lit(cur)){say("      WRONG: nothing drawn\n");fails++;}
   if(qw(303)==s0){say("      WRONG: no span reached the kernels\n");fails++;}
   if(!e&&qw(305)==c0){say("      WRONG: no span converted\n");fails++;}}
  fill_grid(0);}
 /* v1.4 (.546): the indexed loop with culling (cull mode CW): the range proof and the early off-screen drop */
 {int e;say("the indexed loop with culling on (.546):\n");g_cull=2;
  for(e=0;e<2;e++){DWORD p0,q0,o0;int dd;
   if(!fill_grid(e?2:0))return;
   p0=qw(309);q0=qw(310);o0=qw(307);if(!shot_any(cur))return;
   say("   %s: tokens proven inside at once %lu (not %lu), triangles dropped off the screen %lu\n",e?"left columns wholly off the screen":"on the screen",qw(309)-p0,qw(310)-q0,qw(307)-o0);
   if(!shot_list(ref))return;dd=ndiff2(ref,cur);
   say("      against the same triangles as a list: %d pixels differ; lit %d\n",dd,lit(cur));
   if(dd){if(!saved){bmp("VXRHW_A.BMP",ref);bmp("VXRHW_B.BMP",cur);saved=1;}say("      WRONG: the indexed grid differs from the list (VXRHW_A list, B indexed)\n");fails++;}
   if(!lit(cur)){say("      WRONG: nothing drawn\n");fails++;}
   if(!e&&qw(309)==p0){say("      WRONG: the on-screen grid's range was not proven\n");fails++;}
   if(e&&qw(307)==o0){say("      WRONG: no off-screen triangle dropped early\n");fails++;}}
  g_cull=1;fill_grid(0);}
 /* v1.6 (.548): sparse runs: the list's 1,536 vertices drawn by index, each triangle paired with the one 100 further on (a run's span grows
  * past 3 a triangle and 32 long before it reaches a packet's 384: the pass-through hands the run to the indexed loop, then comes back) */
 {DWORD b0;int dd;void *ib2=0;unsigned char *p;U32 k,n=0;static unsigned short o2[NT*3];
  say("sparse runs (.548): the list's vertices by index, triangles paired 100 apart\n");
  for(k=0;k<NT&&n<NT;k++){U32 b=k/100,i=k%100,t;if(b&1)continue;t=b*100+i;if(t<NT){o2[n*3]=(unsigned short)(t*3);o2[n*3+1]=(unsigned short)(t*3+1);o2[n*3+2]=(unsigned short)(t*3+2);n++;}
   t+=100;if(t<NT&&n<NT){o2[n*3]=(unsigned short)(t*3);o2[n*3+1]=(unsigned short)(t*3+1);o2[n*3+2]=(unsigned short)(t*3+2);n++;}}
  if(!fill_grid(0))return;
  if(((FCREATEIB)VT(dev,DEV_CREATEIB))(dev,n*6,0,101,1,&ib2)||!ib2){say("CreateIndexBuffer failed\n");fails++;return;}
  if(((FBLOCK)VT(ib2,BUF_LOCK))(ib2,0,0,&p,0)){say("index buffer lock failed\n");fails++;((F0)VT(ib2,D3D_REL))(ib2);return;}
  memcpy(p,o2,n*6);((F0)VT(ib2,BUF_UNLOCK))(ib2);
  b0=qw(313);
  ((F0)VT(dev,DEV_BEGIN))(dev);clear(3,0xFF000000UL);states(1);((F1)VT(dev,DEV_SETVS))(dev,FVF);
  ((FSTREAM)VT(dev,DEV_SETSTREAM))(dev,0,lvb,sizeof(VC));((FINDICES)VT(dev,DEV_SETINDICES))(dev,ib2,0);
  ((FDIP)VT(dev,DEV_DIP))(dev,4,0,NT*3,0,n);((F0)VT(dev,DEV_END))(dev);
  if(!read_bb(cur)){say("   back buffer lock failed\n");fails++;((F0)VT(ib2,D3D_REL))(ib2);return;}present();
  ((FINDICES)VT(dev,DEV_SETINDICES))(dev,ib,0);((F0)VT(ib2,D3D_REL))(ib2);
  say("   %lu triangles; returns to the pass-through after a sparse run: %lu\n",n,qw(313)-b0);
  if(!shot_list(ref))return;dd=ndiff2(ref,cur);
  say("      against the whole list drawn in order: %d pixels differ; lit %d\n",dd,lit(cur));
  if(n!=NT){say("      WRONG: the pairing missed triangles\n");fails++;}
  if(dd){if(!saved){bmp("VXRHW_A.BMP",ref);bmp("VXRHW_B.BMP",cur);saved=1;}say("      WRONG: the paired draw differs from the list (VXRHW_A list, B indexed)\n");fails++;}
  if(qw(313)==b0){say("      WRONG: no return to the pass-through\n");fails++;}}
}
static LRESULT CALLBACK wp(HWND h,UINT m,WPARAM w,LPARAM l) {return DefWindowProc(h,m,w,l);}
int main(void) {
 HMODULE d8;void *(__stdcall *create)(U32);void *d3d;MODE mode;PP pp;HWND wnd;WNDCLASS wc;HRESULT hr;char path[MAX_PATH],*s;
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXRHW.LOG");logf=fopen(path,"w");
 say("VXRHW v1.6 (driver .548): rhw-0 primitives drawn (textured too), the pass-through's span kernels, the culled indexed loop\n");
 {char dll[MAX_PATH];UINT n=GetSystemDirectoryA(dll,MAX_PATH-16);HMODULE h=0;
  if(n&&n<MAX_PATH-16){strcpy(dll+n,dll[n-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");h=LoadLibraryA(dll);}
  hal_phases=h?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(h,"VX_D3D_QueryPhases"):0;}
 d8=LoadLibrary("d3d8.dll");create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 if(!create){say("d3d8.dll / Direct3DCreate8 not found (DirectX 8 or later is needed)\n");return 1;}
 d3d=create(220);if(!d3d)d3d=create(120);
 if(!d3d){say("Direct3DCreate8 failed\n");return 1;}
 memset(&mode,0,sizeof(mode));((F2P)VT(d3d,D3D_MODE))(d3d,0,&mode);bbfmt=mode.format;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXRHW";wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClass(&wc);
 wnd=CreateWindow("VXRHW","VXRHW",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,WIN+8,WIN+28,0,0,wc.hInstance,0);
 memset(&pp,0,sizeof(pp));pp.bw=WIN;pp.bh=WIN;pp.bfmt=mode.format;pp.bcount=1;pp.swap=3;pp.wnd=wnd;pp.windowed=1;pp.autoz=1;pp.zfmt=80;pp.flags=1;
 hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,0x20,&pp,&dev);
 say("display format %lu; CreateDevice(HAL, %dx%d windowed): %08lX\n",mode.format,WIN,WIN,(U32)hr);
 if(hr||!dev){((F0)VT(d3d,D3D_REL))(d3d);if(logf)fclose(logf);return 1;}
 build_mesh();
 if(make_texture()&&make_buffers()){
  test_rhw();test_big();}else fails++;
 {int k;for(k=0;k<3;k++)if(tlv[k])((F0)VT(tlv[k],D3D_REL))(tlv[k]);}
 if(tex)((F0)VT(tex,D3D_REL))(tex);
 if(vb)((F0)VT(vb,D3D_REL))(vb);if(ib)((F0)VT(ib,D3D_REL))(ib);if(lvb)((F0)VT(lvb,D3D_REL))(lvb);
 ((F0)VT(dev,D3D_REL))(dev);((F0)VT(d3d,D3D_REL))(d3d);DestroyWindow(wnd);
 say("%s: %d wrong\n",fails?"FAILED":"PASSED",fails);
 if(logf)fclose(logf);
 return fails?2:0;
}
