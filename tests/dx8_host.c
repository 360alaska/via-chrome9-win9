/* Host test of src/vx_d3d_dx8.inc (the DirectX 8 DDI layer): random DirectX 8 command buffers (draws of every primitive type, indexed and
 * not, DRAWPRIMITIVE2 / DRAWINDEXEDPRIMITIVE2 byte offsets, negative base vertices, 16- and 32-bit indices, counts past 65535, stream and
 * shader switches, BUFFERBLT, state commands between draws) are given to dx8_dp2; a stand-in for vx_d3d_dp2 reads the DirectX 7 tokens it
 * emits, binds the segment's vertices through the same rules as the HAL's bind_vertices and lists the vertices each primitive reads (each
 * vertex carries a unique id). A reference reads the DirectX 8 buffer directly. The two lists must match, primitive for primitive, and
 * every copied state command must arrive in order. Also: declarations to FVF codes, and the caps / format answers.
 * Phase 2: declarations over one to three streams, in any order, with skips and texture sets left out, drawn plain and indexed (16- and
 * 32-bit, ranges of any width, bases before the stream): every vertex the core is given is compared, byte for byte, with the FVF vertex
 * put together from the streams by the reference. Phase 3: DirectX 8's texture filter values arrive as DirectX 7's.
 * Build: cc -O1 -Isrc tests/dx8_host.c -o /tmp/dx8_host (no Windows headers: minimal stand-ins below). Run: dx8_host [iterations [verbose-iteration [seed]]]. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef uint32_t DWORD;typedef uint16_t WORD;typedef int32_t LONG;typedef int32_t HRESULT;typedef int BOOL;typedef void *LPVOID;
typedef struct {DWORD Data1;WORD Data2,Data3;uint8_t Data4[8];} GUID;
#define DD_OK 0
#define DDERR_INVALIDPARAMS ((HRESULT)0x80070057UL)
#define DDERR_CURRENTLYNOTAVAIL ((HRESULT)0x80004001UL)
#define DDSCAPS_EXECUTEBUFFER 0x00000080UL
#define DDPF_D3DFORMAT 0x00200000UL
#define VX_D3D_INVALID 0x80070057UL
#define VX_D3D_UNSUPPORTED 0x80004001UL
#define VX_D3D_UNPARSED 0x88760BB8UL
#define VX_F_CUBE 1
#define VX_F_BUMP 2
#define VX_F_STAGES 4
#define CONTEXTS 8
typedef struct {DWORD dwSize,dwFlags,dwFourCC,dwRGBBitCount,dwRBitMask,dwGBitMask,dwBBitMask,dwRGBAlphaBitMask;} DDPIXELFORMAT;
typedef struct {uintptr_t fpVidMem;DWORD size;} GBL;typedef GBL DDRAWI_DDRAWSURFACE_GBL; /* E787 */
typedef struct {GBL *lpGbl;struct {DWORD dwCaps;} ddsCaps;} DDRAWI_DDRAWSURFACE_LCL;
typedef struct {DWORD dwSize;GUID guidInfo;DWORD dwExpectedSize;LPVOID lpvData;DWORD dwActualSize;HRESULT ddRVal;} DDHAL_GETDRIVERINFODATA,*LPDDHAL_GETDRIVERINFODATA;
typedef struct {DWORD dwSize,dwCaps,dwMiscCaps,dwRasterCaps,dwZCmpCaps,dwSrcBlendCaps,dwDestBlendCaps,dwAlphaCmpCaps,dwShadeCaps,dwTextureCaps,dwTextureFilterCaps,dwTextureBlendCaps,dwTextureAddressCaps,dwStippleWidth,dwStippleHeight;} D3DPRIMCAPS;
typedef struct {DWORD dwSize,dwFlags,dwDevCaps;D3DPRIMCAPS dpcLineCaps,dpcTriCaps;} vx_ddi_desc;
typedef struct {DWORD dwSize;vx_ddi_desc hwCaps;} vx_ddi_global;
typedef struct vx_ddi_extended {
 DWORD dwSize,dwMinTextureWidth,dwMaxTextureWidth,dwMinTextureHeight,dwMaxTextureHeight;
 DWORD dwMinStippleWidth,dwMaxStippleWidth,dwMinStippleHeight,dwMaxStippleHeight;
 DWORD dwMaxTextureRepeat,dwMaxTextureAspectRatio,dwMaxAnisotropy;
 float dvGuardBandLeft,dvGuardBandTop,dvGuardBandRight,dvGuardBandBottom,dvExtentsAdjust;
 DWORD dwStencilCaps,dwFVFCaps,dwTextureOpCaps;WORD wMaxTextureBlendStages,wMaxSimultaneousTextures;
 DWORD dwMaxActiveLights;float dvMaxVertexW;WORD wMaxUserClipPlanes,wMaxVertexBlendMatrices;
 DWORD dwVertexProcessingCaps,dwReserved1,dwReserved2,dwReserved3,dwReserved4;
} vx_ddi_extended;
typedef struct vx_ddi_dp2 {
 DWORD dwhContext,dwFlags,dwVertexType;DDRAWI_DDRAWSURFACE_LCL *lpDDCommands;DWORD dwCommandOffset,dwCommandLength;
 union {DDRAWI_DDRAWSURFACE_LCL *lpDDVertex;void *lpVertices;};
 DWORD dwVertexOffset,dwVertexLength,dwReqVertexBufSize,dwReqCommandBufSize;DWORD *lpdwRStates;union {DWORD dwVertexSize;HRESULT ddrval;};DWORD dwErrorOffset;
} vx_ddi_dp2;
typedef struct {int dummy;} vx_d3d_context;
typedef struct vx_d3d_input {const uint8_t *commands,*vertices;uint32_t command_bytes,vertex_bytes,vertex_count,vertex_stride,fvf;uint32_t flags,*render_states,render_state_count;} vx_d3d_input;
typedef struct vx_d3d_services {void *context;uint32_t (*bind_vertices)(void *,vx_d3d_input *);} vx_d3d_services;
typedef struct context_slot {uint32_t handle,owner,local;vx_d3d_context core;} context_slot;
typedef struct dp2_source {context_slot *slot;const vx_ddi_dp2 *args;uint32_t stride;} dp2_source;
static context_slot contexts[CONTEXTS];static vx_ddi_global global_caps;static uint32_t vx_d3d_feat=7,vx_d3d_rttex=1;
static int IsBadReadPtr(const void *p,size_t n){(void)n;return !p;}
volatile uint32_t vx_d3d_bd[16],vx_d3d_bc[24]; /* E786: the core's marks (the core itself is a stub here) */
static uint32_t fake_ms;static uint32_t GetTickCount(void){return fake_ms+=100;} /* each look at the clock 0.1 s later: the layer's busy wait (250 ms) gives the segment again twice */
static int IsBadWritePtr(void *p,size_t n){(void)n;return !p;}
static uint32_t surface_bytes(DDRAWI_DDRAWSURFACE_LCL *s){return s&&s->lpGbl?s->lpGbl->size:0;}
static float tm[4][16],gm[16];
int vx_d3d_tnl_matrix(const vx_d3d_context *c,uint32_t ty,float *o){(void)c;if(ty>3||!ty)return 0;memcpy(o,tm[ty],64);return 1;}
uint32_t vx_d3d_dp2(vx_d3d_context *,const vx_d3d_input *,const vx_d3d_services *,uint32_t *);
static DWORD get_driver_info(LPDDHAL_GETDRIVERINFODATA p){vx_ddi_extended e;memset(&e,0,sizeof(e));e.dwMaxTextureWidth=2048;e.dwMaxAnisotropy=16;e.wMaxTextureBlendStages=3;memcpy(p->lpvData,&e,sizeof(e));p->ddRVal=0;return 1;}
static uint32_t fixups;
#define DX8_FIXUP(c) ((void)(c),fixups++)
#include "vx_d3d_dx8.inc"
static uint32_t dx8_decl_fvf(const uint8_t *d,uint32_t bytes){dx8_vs v;return dx8_decl_parse(d,bytes,&v)?v.fvf:0;} /* the FVF a declaration draws as, 0 when none */
#define u32 dx8_u32
#define u16 dx8_u16
/* ---- the vertex lists ---- */
#define MAXP 4000000
static uint32_t *got,*want;static uint32_t ngot,nwant;static uint32_t gstate[100000],ngstate,wstate[100000],nwstate;
#define BAD 0xDEADDEADUL
static void put(uint32_t *l,uint32_t *n,uint32_t v){if(*n<MAXP)l[(*n)++]=v;}
static uint32_t vid(const uint8_t *base,uint32_t count,uint32_t stride,long long i){if(i<0||(uint64_t)i>=count)return BAD;return u32(base+(uint64_t)i*stride);}
static dp2_source *g_src;static int verbose;static long inject=-1,ntok,sticky;
static int g_sum;static uint32_t g_fvf,g_fvf_bad;
static uint32_t fvf_size(uint32_t f){uint32_t n=12,k,t=(f>>8)&15;if((f&0xE)>4)n+=(((f&0xE)-4)>>1)*4;if(f&0x10)n+=12;if(f&0x20)n+=4;if(f&0x40)n+=4;if(f&0x80)n+=4;
 for(k=0;k<t;k++){uint32_t c=(f>>(16+2*k))&3;n+=c==0?8:c==1?12:c==2?16:4;}return n;}
static uint32_t sum32(const uint8_t *p,uint32_t n){uint32_t h=2166136261u,i;for(i=0;i<n;i++)h=(h^p[i])*16777619u;return h==BAD?1:h;}
static void pv(const vx_d3d_input *b,long long i){uint32_t id=vid(b->vertices,b->vertex_count,b->vertex_stride,i);put(got,&ngot,id);
 if(g_sum)put(got,&ngot,id==BAD?BAD:sum32(b->vertices+(uint64_t)i*b->vertex_stride,fvf_size(b->fvf)));}
#define PH(k) put(got,&ngot,g_sum?(k)*2U:(k))
/* the stand-in core: reads DirectX 7 tokens */
uint32_t vx_d3d_dp2(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,uint32_t *err) {
 uint32_t pos=0,bound=0;vx_d3d_input b;(void)c;memset(&b,0,sizeof(b));*err=0;
 while(pos<in->command_bytes){uint32_t op=in->commands[pos],n=u16(in->commands+pos+2),k,len;const uint8_t *p=in->commands+pos+4;
  if(dx8_is_draw7(op)&&op!=23&&!bound){if(sv->bind_vertices(sv->context,&b)){*err=pos;return VX_D3D_INVALID;}bound=1;if(g_sum&&(b.fvf!=g_fvf||fvf_size(b.fvf)>b.vertex_stride))g_fvf_bad++;if(verbose){const vx_ddi_dp2 *q=((dp2_source*)sv->context)->args;printf("  bind: flags %u off %u len %u -> count %u stride %u\n",q->dwFlags,q->dwVertexOffset,q->dwVertexLength,b.vertex_count,b.vertex_stride);}}
  if(dx8_is_draw7(op)&&sticky>0){sticky--;*err=pos;return 0x8876021CUL;} /* still busy: every call's first draw token, `sticky` times */
  if(dx8_is_draw7(op)&&inject>=0&&ntok++==inject){*err=pos;if(sticky<0)sticky=1000;return 0x8876021CUL;} /* WASSTILLDRAWING at this token (sticky -1: and from then on) */
  if(verbose&&dx8_is_draw7(op))printf("  dx7 op %u n %u start %u idx %u %u %u\n",op,n,u16(in->commands+pos+4),u16(in->commands+pos+6),u16(in->commands+pos+8),u16(in->commands+pos+10));
  #define V(i) vid(b.vertices,b.vertex_count,b.vertex_stride,(long long)(i))
  switch(op){
   case 1:for(k=0;k<n;k++){uint32_t cnt=u16(p+k*4),s=u16(p+k*4+2),j;for(j=0;j<cnt;j++){PH(1);pv(&b,(long long)(s+j));}}len=n*4;break;
   case 15:{uint32_t s=u16(p);for(k=0;k<n;k++){PH(2);pv(&b,(long long)(s+2*k));pv(&b,(long long)(s+2*k+1));}len=2;break;}
   case 16:{uint32_t s=u16(p);for(k=0;k<n;k++){PH(2);pv(&b,(long long)(s+k));pv(&b,(long long)(s+k+1));}len=2;break;}
   case 18:{uint32_t s=u16(p);for(k=0;k<n;k++){PH(3);pv(&b,(long long)(s+3*k));pv(&b,(long long)(s+3*k+1));pv(&b,(long long)(s+3*k+2));}len=2;break;}
   case 19:{uint32_t s=u16(p);for(k=0;k<n;k++){PH(3);pv(&b,(long long)(s+k));pv(&b,(long long)(s+k+1+(k&1)));pv(&b,(long long)(s+k+2-(k&1)));}len=2;break;}
   case 21:{uint32_t s=u16(p);for(k=0;k<n;k++){PH(3);pv(&b,(long long)(s+k+1));pv(&b,(long long)(s+k+2));pv(&b,(long long)(s));}len=2;break;}
   case 26:{uint32_t s=u16(p);for(k=0;k<n;k++){PH(3);pv(&b,(long long)(s+u16(p+2+k*6)));pv(&b,(long long)(s+u16(p+4+k*6)));pv(&b,(long long)(s+u16(p+6+k*6)));}len=2+n*6;break;}
   case 20:{uint32_t s=u16(p);for(k=0;k<n;k++){uint32_t a=u16(p+2+k*2),bb=u16(p+4+k*2),d=u16(p+6+k*2);PH(3);pv(&b,(long long)(s+a));pv(&b,(long long)(s+((k&1)?d:bb)));pv(&b,(long long)(s+((k&1)?bb:d)));}len=2+(n+2)*2;break;}
   case 22:{uint32_t s=u16(p),c0=u16(p+2);for(k=0;k<n;k++){PH(3);pv(&b,(long long)(s+u16(p+4+k*2)));pv(&b,(long long)(s+u16(p+6+k*2)));pv(&b,(long long)(s+c0));}len=2+(n+2)*2;break;}
   case 27:{uint32_t s=u16(p);for(k=0;k<n;k++){PH(2);pv(&b,(long long)(s+u16(p+2+k*4)));pv(&b,(long long)(s+u16(p+4+k*4)));}len=2+n*4;break;}
   case 17:{uint32_t s=u16(p);for(k=0;k<n;k++){PH(2);pv(&b,(long long)(s+u16(p+2+k*2)));pv(&b,(long long)(s+u16(p+4+k*2)));}len=2+(n+1)*2;break;}
   case 23:{uint32_t imm=4+(uint32_t)((4-(((uintptr_t)p+4)&3))&3),st=in->vertex_stride;const uint8_t *v=p+imm;
    for(k=0;k<n;k++){PH(3);put(got,&ngot,u32(v+(k+1)*st));put(got,&ngot,u32(v+(k+2)*st));put(got,&ngot,u32(v));}len=imm+(n+2)*st;break;}
   default:len=dx8_dx7_len(op,n,p,in->command_bytes-pos-4);if(len>=0xFFFFFFFEUL){printf("stand-in: unknown op %u\n",op);exit(1);}
    if(op==36&&u32(p)==1)memcpy(tm[1],p+4,64); /* the T&L's world, as the core would hold it */
    {uint32_t j;gstate[ngstate++]=op;gstate[ngstate++]=n;for(j=0;j<len&&j<64;j+=4)gstate[ngstate++]=u32(p+j);}
  }
  pos+=4+len;}
 return 0;
}
/* the HAL's bind_vertices rules (T&L on: a vertex buffer bound whole from the offset; user memory: dwVertexLength) */
static uint32_t bind(void *ctx,vx_d3d_input *in){dp2_source *s=ctx;const vx_ddi_dp2 *p=s->args;uint32_t st=s->stride,base,avail=0;
 if(!st||(!p->dwVertexLength&&(p->dwFlags&1)))return VX_D3D_INVALID;
 if(p->dwFlags&1)base=0,in->vertices=(const uint8_t*)p->lpVertices+p->dwVertexOffset,in->vertex_count=p->dwVertexLength;
 else{avail=surface_bytes(p->lpDDVertex);if(!p->lpDDVertex||p->dwVertexOffset>avail)return VX_D3D_INVALID;(void)base;
  in->vertices=(const uint8_t*)p->lpDDVertex->lpGbl->fpVidMem+p->dwVertexOffset;in->vertex_count=(avail-p->dwVertexOffset)/st;if(!in->vertex_count)return VX_D3D_INVALID;}
 in->vertex_stride=st;in->fvf=p->dwVertexType;return 0;}
/* ---- buffers ---- */
#define NB 6
static GBL gbl[NB];static DDRAWI_DDRAWSURFACE_LCL lcl[NB];static uint8_t *mem[NB],*shadow0;static uint32_t nextid=1;
static void mkvb(int k,uint32_t bytes){free(mem[k]);mem[k]=malloc(bytes+16);gbl[k].fpVidMem=(uintptr_t)mem[k];gbl[k].size=bytes;lcl[k].lpGbl=&gbl[k];lcl[k].ddsCaps.dwCaps=DDSCAPS_EXECUTEBUFFER;
 {uint32_t j;for(j=0;j+4<=bytes;j+=4){uint32_t v=nextid++;memcpy(mem[k]+j,&v,4);}}}
static uint32_t rnd(uint32_t n){return n?(uint32_t)(((uint64_t)rand()<<15^rand())%n):0;}
static uint8_t cbuf[1<<22];static uint32_t clen;
static uint8_t *cmd(uint32_t op,uint32_t n,uint32_t bytes){uint8_t *o=cbuf+clen;o[0]=(uint8_t)op;o[1]=0;o[2]=(uint8_t)n;o[3]=(uint8_t)(n>>8);clen+=4+bytes;return o+4;}
static void w32(uint8_t *o,uint32_t v){memcpy(o,&v,4);}
/* the reference: DirectX 8 semantics read from the command buffer */
static void ref_prims(uint32_t t,const uint8_t *vb,uint32_t vcount,uint32_t st,long long first,uint32_t n,const uint8_t *ib,uint32_t is,uint32_t ix0,long long base){
 uint32_t k;
 #define I(j) (ib?(long long)(is==2?u16(ib+(uint64_t)(ix0+(j))*2):u32(ib+(uint64_t)(ix0+(j))*4))+base:first+(j))
 #define R(j) vid(vb,vcount,st,I(j))
 for(k=0;k<n;k++)switch(t){
  case 1:put(want,&nwant,1);put(want,&nwant,R(k));break;
  case 2:put(want,&nwant,2);put(want,&nwant,R(2*k));put(want,&nwant,R(2*k+1));break;
  case 3:put(want,&nwant,2);put(want,&nwant,R(k));put(want,&nwant,R(k+1));break;
  case 4:put(want,&nwant,3);put(want,&nwant,R(3*k));put(want,&nwant,R(3*k+1));put(want,&nwant,R(3*k+2));break;
  case 5:put(want,&nwant,3);put(want,&nwant,R(k));put(want,&nwant,R(k+1+(k&1)));put(want,&nwant,R(k+2-(k&1)));break;
  case 6:put(want,&nwant,3);put(want,&nwant,R(k+1));put(want,&nwant,R(k+2));put(want,&nwant,R(0));break;}
}
/* primitives that read a missing vertex are left out by the core: dropped from both lists */
static uint32_t clean(uint32_t *l,uint32_t n){uint32_t i=0,o=0;while(i<n){uint32_t k=l[i],j,bad=0;for(j=1;j<=k;j++)if(l[i+j]==BAD)bad=1;if(!bad){memmove(l+o,l+i,(k+1)*4);o+=k+1;}i+=k+1;}return o;}
static uint32_t prim_v(uint32_t t,uint32_t n){return dx8_verts(t,n);}
/* ---- phase 2: declarations over several streams (gathered): every vertex's bytes checked ---- */
typedef struct {uint32_t reg,dt,stream,soff;} elem2;
static elem2 el2[16];static uint32_t nel2,st2[3],sb2[3];static const uint8_t *sp2[3];
static const uint32_t dsize[8]={4,8,12,16,4,4,4,8};
/* the FVF vertex the declaration describes, for vertex i, put together from the streams: 0 when a stream does not hold it */
static int ref2_vertex(long long i,uint8_t *o,uint32_t *bytes){uint32_t reg,k,n=0,ntex=0;
 for(k=0;k<nel2;k++)if(el2[k].reg>=7&&el2[k].reg-6>ntex)ntex=el2[k].reg-6;
 for(reg=0;reg<7+ntex;reg++){const elem2 *e=0;for(k=0;k<nel2;k++)if(el2[k].reg==reg)e=&el2[k];
  if(!e){if(reg>=7){memset(o+n,0,8);n+=8;}continue;}
  if(i<0||(uint64_t)i*st2[e->stream]+e->soff+dsize[e->dt]>sb2[e->stream]||((uint64_t)i+1)*st2[e->stream]>sb2[e->stream])return 0; /* a whole vertex of the stream's stride */
  memcpy(o+n,sp2[e->stream]+(uint64_t)i*st2[e->stream]+e->soff,dsize[e->dt]);n+=dsize[e->dt];}
 *bytes=n;return 1;}
static uint32_t ref2_fvf(void){uint32_t k,f=0,ntex=0,w=0,bi=0,betas;
 for(k=0;k<nel2;k++){uint32_t r=el2[k].reg,dt=el2[k].dt;static const uint32_t code[4]={3,0,1,2};
  if(r==1)w=dt+1;else if(r==2)bi=1;else if(r==3)f|=0x10;else if(r==4)f|=0x20;else if(r==5)f|=0x40;else if(r==6)f|=0x80;
  else if(r>=7){f|=code[dt]<<(16+2*(r-7));if(r-6>ntex)ntex=r-6;}}
 betas=w+bi;return f|(betas?4+2*betas:2)|ntex<<8;}
static void ref2_prims(uint32_t t,long long first,uint32_t n,const uint8_t *ib,uint32_t is,uint32_t ix0,long long base){
 uint32_t k,j,a[3],pvn;uint8_t v[256];uint32_t vb;
 for(k=0;k<n;k++){pvn=dx8_prim_ix(t,k,a);put(want,&nwant,pvn*2);
  for(j=0;j<pvn;j++){long long i=ib?(long long)(is==2?u16(ib+(uint64_t)(ix0+a[j])*2):u32(ib+(uint64_t)(ix0+a[j])*4))+base:first+a[j];
   if(ref2_vertex(i,v,&vb)){put(want,&nwant,u32(v));put(want,&nwant,sum32(v,vb));}else{put(want,&nwant,BAD);put(want,&nwant,BAD);}}}}
static uint32_t phase2(int iters){
 uint32_t fails=0;int iter;
 for(iter=0;iter<iters&&fails<10;iter++){
  vx_ddi_dp2 p;dp2_source src;vx_d3d_services sv;vx_d3d_input in;uint32_t err=0,rc,k,j,sh[NB],decl[64],nd=0,soff[3]={0,0,0},h=(1+rnd(50))*2+1,um=rnd(4)==0,nvmin=0xFFFFFFFFUL,ndraw=1+rnd(6),ibcur[NB]={0},ibset=0;
  uint32_t r_ib=3,r_is=2;dx8_ctx *x=dx8_of(&contexts[0]);
  memset(x,0,sizeof(*x));x->handle=7;x->owner=1;x->dx8=1|0x200;ngot=nwant=ngstate=nwstate=0;clen=0;g_sum=1;g_fvf_bad=0;
  for(k=0;k<NB;k++){mkvb((int)k,k<3?(64+rnd(k==0?(rnd(4)==0?3000000:400000):60000))&~3U:(2+rnd(200000))*4);sh[k]=100+k*3;dx8_buf_set(1,0x1000,sh[k],(uintptr_t)&lcl[k]);}
  /* the declaration: position, then a random choice of the others, each in a random stream, in a random order there, skips between */
  {uint32_t regs[16],nr=0,ns=1+rnd(3),ord=rnd(4)==0,s2,gap=rnd(5)==0,ntex=rnd(4);nel2=0;
   regs[nr++]=0;if(rnd(4)==0)regs[nr++]=1;if(rnd(6)==0)regs[nr++]=2;if(rnd(2))regs[nr++]=3;if(rnd(6)==0)regs[nr++]=4;if(rnd(2))regs[nr++]=5;if(rnd(3)==0)regs[nr++]=6;
   for(k=0;k<ntex;k++)if(!(gap&&k==0&&ntex>1))regs[nr++]=7+k;
   if(ord)ns=1; /* a declaration an FVF describes as it lies: drawn direct */
   else for(k=nr-1;k>0;k--){uint32_t q=rnd(k+1),t2=regs[k];regs[k]=regs[q];regs[q]=t2;}
   for(s2=0;s2<ns;s2++){decl[nd++]=0x20000000UL|s2;
    for(k=0;k<nr;k++){uint32_t reg=regs[k],dt=reg==0||reg==3?2:reg==1?rnd(4):reg==2?(rnd(2)?5:4):reg==4?0:reg==5||reg==6?4:rnd(4);
     if((ord?0:k%ns)!=s2)continue;
     if(!ord&&rnd(4)==0){uint32_t c=1+rnd(3);decl[nd++]=0x50000000UL|c<<16;soff[s2]+=c*4;}
     decl[nd++]=0x40000000UL|dt<<16|reg;el2[nel2].reg=reg;el2[nel2].dt=dt;el2[nel2].stream=s2;el2[nel2].soff=soff[s2];nel2++;soff[s2]+=dsize[dt];}}
   decl[nd++]=0xFFFFFFFFUL;
   for(s2=0;s2<3;s2++)st2[s2]=soff[s2]+4*rnd(4);
   if(!st2[0])st2[0]=4;}
  g_fvf=ref2_fvf();
  memset(&p,0,sizeof(p));
  if(um){p.dwFlags=1;p.lpVertices=mem[0];p.dwVertexOffset=rnd(16)*4;p.dwVertexLength=(gbl[0].size-p.dwVertexOffset)/st2[0];sp2[0]=mem[0]+p.dwVertexOffset;sb2[0]=p.dwVertexLength*st2[0];}
  else{sp2[0]=mem[0];sb2[0]=gbl[0].size;}
  sp2[1]=mem[1];sb2[1]=gbl[1].size;sp2[2]=mem[2];sb2[2]=gbl[2].size;
  for(k=0;k<3;k++){uint32_t used=0,need=0;for(j=0;j<nel2;j++)if(el2[j].stream==k){used=1;if(el2[j].soff+dsize[el2[j].dt]>need)need=el2[j].soff+dsize[el2[j].dt];}
   if(used&&st2[k]){uint32_t c=sb2[k]>=need?(sb2[k]-need)/st2[k]+1:0;if(c>sb2[k]/st2[k])c=sb2[k]/st2[k];if(c<nvmin)nvmin=c;}}
  if(nvmin==0xFFFFFFFFUL||nvmin<8){iter--;continue;}
  {uint8_t *o=cmd(45,1,12+nd*4);w32(o,h);w32(o+4,nd*4);w32(o+8,0);memcpy(o+12,decl,nd*4);o=cmd(47,1,4);w32(o,h);}
  for(k=0;k<3;k++){if(k==0&&um){uint8_t *o=cmd(50,1,8);w32(o,0);w32(o+4,st2[0]);}else{uint8_t *o=cmd(49,1,12);w32(o,k);w32(o+4,sh[k]);w32(o+8,st2[k]);}}
  for(k=0;k<ndraw;k++){uint32_t t=1+rnd(6),big=rnd(30)==0,n=big?65530+rnd(70000):1+rnd(400);
   if(rnd(5)==0){uint8_t *o=cmd(25,1,8);o[0]=0;o[1]=0;o[2]=1;o[3]=0;w32(o+4,5);wstate[nwstate++]=25;wstate[nwstate++]=1;wstate[nwstate++]=0x00010000;wstate[nwstate++]=5;} /* a state between draws keeps its place */
   if(rnd(2)){uint32_t nv=dx8_verts(t,n),first;if(rnd(8)&&nv>nvmin)n=1+rnd(nvmin/3);nv=dx8_verts(t,n);first=rnd(8)?rnd(nvmin>nv?nvmin-nv+1:1):nvmin-rnd(6);
    {uint8_t *o=cmd(52,1,12);w32(o,t);w32(o+4,first);w32(o+8,n);}ref2_prims(t,first,n,0,2,0,0);}
   else{uint32_t ni=dx8_verts(t,n),ibcap,ix0,mn,nv,i2;long long base;uint8_t *ib;
    if(rnd(3)==0){uint8_t *o;r_ib=3+rnd(3);r_is=rnd(3)?2:4;o=cmd(51,1,8);w32(o,sh[r_ib]);w32(o+4,r_is);}
    else if(!ibset){uint8_t *o=cmd(51,1,8);w32(o,sh[r_ib]);w32(o+4,r_is);}
    ibset=1;
    ib=mem[r_ib];ibcap=gbl[r_ib].size/r_is;ix0=(ibcur[r_ib]+r_is-1)/r_is;if(ix0+ni>ibcap)continue;ibcur[r_ib]=(ix0+ni)*r_is+4*rnd(4); /* regions apart: the buffer is read when the call runs */
    nv=1+rnd(rnd(3)?(nvmin<3000?nvmin:3000):nvmin);mn=rnd(r_is==4?200000:65536-(nv<65536?nv:65535));if(r_is==2&&mn+nv>65536)nv=65536-mn;
    base=(long long)rnd(nvmin)-(long long)mn;if(rnd(6)==0)base-=rnd(50);if(rnd(6)==0)base+=rnd(50);
    for(i2=0;i2<ni;i2++){uint32_t v=mn+rnd(nv);if(r_is==2){uint16_t v16=(uint16_t)v;memcpy(ib+(ix0+i2)*2,&v16,2);}else memcpy(ib+(ix0+i2)*4,&v,4);}
    {uint8_t *o=cmd(53,1,24);w32(o,t);w32(o+4,(uint32_t)(int32_t)base);w32(o+8,mn);w32(o+12,nv);w32(o+16,ix0);w32(o+20,n);}
    ref2_prims(t,0,n,ib,r_is,ix0,base);}}
  memset(&src,0,sizeof(src));src.slot=&contexts[0];src.args=&p;src.stride=st2[0];memset(&sv,0,sizeof(sv));sv.context=&src;sv.bind_vertices=bind;
  memset(&in,0,sizeof(in));in.commands=cbuf;in.command_bytes=clen;inject=rnd(3)==0?(long)rnd(12):-1;ntok=0;sticky=0;
  rc=dx8_dp2(&contexts[0],&p,&in,&sv,&src,&err);inject=-1;
  if(rc){printf("phase 2 iter %d: rc %X at %u (op %u: %u %u %u), mode %u fvf %X um %u strides %u %u %u, %u vertices\n",iter,rc,err,cbuf[err],u32(cbuf+err+4),u32(cbuf+err+8),u32(cbuf+err+12),x->mode,g_fvf,um,st2[0],st2[1],st2[2],nvmin);fails++;continue;}
  ngot=clean(got,ngot);nwant=clean(want,nwant);
  if(g_fvf_bad){printf("phase 2 iter %d: a segment bound with another FVF than %X (mode %u)\n",iter,g_fvf,x->mode);fails++;}
  if(ngot!=nwant||memcmp(got,want,ngot*4)){uint32_t q=0;while(q<ngot&&q<nwant&&got[q]==want[q])q++;printf("phase 2 iter %d: %u/%u words, first difference at %u (%X vs %X), mode %u fvf %X um %u\n",iter,ngot,nwant,q,q<ngot?got[q]:0,q<nwant?want[q]:0,x->mode,g_fvf,um);fails++;}
  if(ngstate!=nwstate||memcmp(gstate,wstate,ngstate*4)){printf("phase 2 iter %d: state commands differ (%u/%u)\n",iter,ngstate,nwstate);fails++;}
 }
 g_sum=0;
 printf("phase 2: %d iterations, %u failures; gathered draws %u (%u vertices), primitive-by-primitive %u, direct declarations %u, gathered %u; busy answers taken up %u\n",iter,fails,vx_d3d_dx8_stat[DX8S_GATHER],vx_d3d_dx8_stat[DX8S_GVERTS],vx_d3d_dx8_stat[DX8S_DEINDEX],vx_d3d_dx8_stat[DX8S_VSFVF],vx_d3d_dx8_stat[DX8S_VSGATHER],vx_d3d_dx8_stat[DX8S_BUSY]);
 return fails;
}
/* the texture filter values: DirectX 8's made DirectX 7's, each state and value; the fix-up runs once a context */
static uint32_t phase3(void){
 static const uint32_t mag[6]={1,1,2,5,3,4},mn[6]={1,1,2,3,2,2},mip[6]={1,2,3,3,3,3};
 vx_ddi_dp2 p;dp2_source src;vx_d3d_services sv;vx_d3d_input in;uint32_t err=0,rc,v,k,fails=0,f0=fixups;dx8_ctx *x=dx8_of(&contexts[0]);
 memset(x,0,sizeof(*x));x->handle=7;x->owner=1;x->dx8=1;ngstate=0;clen=0;memset(&p,0,sizeof(p));
 for(v=0;v<6;v++)for(k=16;k<=19;k++){uint8_t *o=cmd(25,1,8);o[0]=2;o[1]=0;o[2]=(uint8_t)k;o[3]=0;w32(o+4,v);}
 memset(&src,0,sizeof(src));src.slot=&contexts[0];src.args=&p;src.stride=4;memset(&sv,0,sizeof(sv));sv.context=&src;sv.bind_vertices=bind;
 memset(&in,0,sizeof(in));in.commands=cbuf;in.command_bytes=clen;
 rc=dx8_dp2(&contexts[0],&p,&in,&sv,&src,&err);if(!rc)rc=dx8_dp2(&contexts[0],&p,&in,&sv,&src,&err);
 if(rc||ngstate!=2*24*4){printf("phase 3: rc %X, %u state words\n",rc,ngstate);return 1;}
 for(v=0;v<6;v++)for(k=16;k<=19;k++){const uint32_t *g=gstate+(v*4+(k-16))*4,w=k==16?mag[v]:k==17?mn[v]:k==18?mip[v]:v;
  if(g[0]!=25||g[2]!=(2|k<<16)||g[3]!=w){printf("phase 3: state %u value %u arrived as %u (want %u)\n",k,v,g[3],w);fails++;}}
 if(fixups!=f0+1){printf("phase 3: the fix-up ran %u times in two calls\n",fixups-f0);fails++;}
 printf("phase 3: filter values, %u failures\n",fails);
 return fails;
}
int main(int argc,char **argv){
 int iter,iters=argc>1?atoi(argv[1]):3000;uint32_t fails=0;
 if(argc>3)srand((unsigned)atoi(argv[3])); /* another sequence: dx8_host <iterations> <verbose iteration or -1> <seed> */
 got=malloc(MAXP*4);want=malloc(MAXP*4);
 /* declarations */
 {static const struct {uint32_t d[8];uint32_t fvf;} dc[]={
   {{0x20000000,0x40020000,0x40020003,0x40040005,0x40010007,0xFFFFFFFF},0x152}, /* pos3 normal3 diffuse tex0(float2): XYZ|NORMAL|DIFFUSE|TEX1 */
   {{0x20000000,0x40020000,0x40040005,0x40040006,0x40010007,0x40000008,0xFFFFFFFF},0x2C2|0xC0000UL}, /* + spec, tex1 float1 (size code 3 at bits 18-19) */
   {{0x20000000,0x40020003,0x40020000,0xFFFFFFFF},0x12},  /* normal before position: gathered as XYZ | NORMAL */
   {{0x20000001,0x40020000,0xFFFFFFFF},0x2},              /* stream 1: gathered */
   {{0x20000000,0x40020000,0x50010000,0xFFFFFFFF},0x2},   /* a skip */
   {{0x20000000,0x40020000,0x40010008,0xFFFFFFFF},0x202}, /* tex1 without tex0: tex0 given as zeros */
   {{0x20000000,0x40020000,0x40020000,0xFFFFFFFF},0},     /* a register twice */
   {{0x30000000,0x40020000,0xFFFFFFFF},0},                /* the tessellator's stream */
   {{0x20000000,0x40020000,0x4002000F,0xFFFFFFFF},0},     /* the tweening position */
   {{0x20000000,0x40030000,0xFFFFFFFF},0}};             /* position float4 */
  uint32_t k;for(k=0;k<sizeof(dc)/sizeof(dc[0]);k++){uint32_t f=dx8_decl_fvf((const uint8_t*)dc[k].d,32);if(f!=dc[k].fvf){printf("decl %u: fvf %X want %X\n",k,f,dc[k].fvf);fails++;}}}
 /* caps and formats */
 {uint8_t b[256];DDHAL_GETDRIVERINFODATA q;vx_caps8 *k=(vx_caps8*)b;vx_d3d_dx8_on=1;memset(&q,0,sizeof(q));memset(b,0,sizeof(b));((DWORD*)b)[1]=0xFFFFFFFFUL;((DWORD*)b)[2]=1;((DWORD*)b)[3]=212;q.dwExpectedSize=212;q.lpvData=b;
  if(!dx8_gdi2(&q)||q.ddRVal||q.dwActualSize!=212||k->MaxStreams!=16||k->MaxVertexIndex!=0xFFFFFF||k->PresentationIntervals!=0x8000000FUL||!(k->PrimitiveMiscCaps&0x80)||k->DeviceType!=1||k->MaxTextureWidth!=2048||k->MaxTextureBlendStages!=3){printf("caps8 wrong\n");fails++;}
  memset(b,0,sizeof(b));((DWORD*)b)[1]=0xFFFFFFFFUL;((DWORD*)b)[2]=2;((DWORD*)b)[3]=24;q.dwExpectedSize=24;if(!dx8_gdi2(&q)||q.ddRVal||((DWORD*)b)[4]!=16){printf("format count wrong\n");fails++;}
  {uint32_t i,disp=0,z=0;for(i=0;i<16;i++){DDPIXELFORMAT *f=(DDPIXELFORMAT*)(b+20);memset(b,0,sizeof(b));((DWORD*)b)[1]=0xFFFFFFFFUL;((DWORD*)b)[2]=3;((DWORD*)b)[3]=52;((DWORD*)b)[4]=i;q.dwExpectedSize=52;
   if(!dx8_gdi2(&q)||q.ddRVal||f->dwFlags!=DDPF_D3DFORMAT||!f->dwRBitMask||f->dwRGBBitCount){printf("format %u wrong\n",i);fails++;}if(f->dwRBitMask&0x400)disp++;if(f->dwRBitMask&0x40)z++; /* E782: dwOperations is dwRBitMask's place (offset 16); dwRGBBitCount's (dwPrivateFormatBitCount) stays 0 */}
   if(disp!=2||z!=3){printf("display formats %u, depth %u\n",disp,z);fails++;}}
  ((DWORD*)b)[1]=0x1234;q.dwExpectedSize=24;if(dx8_gdi2(&q)){printf("stereo taken as GetDriverInfo2\n");fails++;}}
 contexts[0].handle=7;contexts[0].owner=1;contexts[0].local=0x1000;
 verbose=0;for(iter=0;iter<iters;iter++){if(argc>2)verbose=iter==atoi(argv[2]);
  vx_ddi_dp2 p;dp2_source src;vx_d3d_services sv;vx_d3d_input in;uint32_t err=0,rc,k,ncmd=1+rnd(12),sh[NB],ust;uint8_t *um;
  /* reference state */
  uint32_t r_fvf=0,r_s0=0,r_st=0,r_um=0,r_ib=0,r_is=2,r_ibset=0,ibcur[NB]={0};dx8_ctx *x=dx8_of(&contexts[0]);
  memset(x,0,sizeof(*x));x->handle=7;x->owner=1;x->dx8=1;ngot=nwant=ngstate=nwstate=0;clen=0;
  for(k=0;k<NB;k++){mkvb((int)k,k<3?(1024+rnd(k==0?2000000:40000))&~3U:(2+rnd(300000))*4);sh[k]=100+k*3;dx8_buf_set(1,0x1000,sh[k],(uintptr_t)&lcl[k]);}
  {uint32_t j;for(j=0;j<16;j++)gm[j]=tm[1][j]=(float)rnd(5);}
  free(shadow0);shadow0=malloc(gbl[0].size);memcpy(shadow0,mem[0],gbl[0].size); /* the reference reads vertex buffer 0 as BUFFERBLT has left it at each command */
  ust=(1+rnd(16))*4;um=mem[2];
  memset(&p,0,sizeof(p));p.dwFlags=1;p.lpVertices=um;p.dwVertexOffset=rnd(64)*4;p.dwVertexLength=(gbl[2].size-p.dwVertexOffset)/ust;
  static uint32_t dcmd[64];uint32_t ndcmd=0,wcut[64];
  for(k=0;k<ncmd;k++){uint32_t c=rnd(18);
   if(c>=4&&ndcmd<64){dcmd[ndcmd]=clen;wcut[ndcmd]=nwant;}
   if(c==0){uint8_t *o=cmd(47,1,4);r_fvf=rnd(5)?0x44+ (rnd(2)<<8):0;w32(o,r_fvf);}
   else if(c==1){uint8_t *o=cmd(49,1,12);r_s0=rnd(2);r_st=(1+rnd(16))*4;r_um=0;w32(o,0);w32(o+4,sh[r_s0]);w32(o+8,r_st);}
   else if(c==2){uint8_t *o=cmd(50,1,8);r_um=1;r_st=ust;w32(o,0);w32(o+4,ust);}
   else if(c==3){uint8_t *o=cmd(51,1,8);r_ib=3+rnd(3);r_is=rnd(4)?2:4;r_ibset=1;w32(o,sh[r_ib]);w32(o+4,r_is);}
   else if(c==14&&r_fvf&&r_st&&r_st>=12){ /* a clipped fan */
    const uint8_t *vb=r_um?um+p.dwVertexOffset:r_s0?mem[r_s0]:shadow0;uint32_t vc=r_um?p.dwVertexLength:gbl[r_s0].size/r_st,nn=1+rnd(8),f0;uint8_t *o;
    if(nn+2>vc)continue;f0=rnd(vc-nn-1);o=cmd(58,1,12);w32(o,f0*r_st);w32(o+4,rnd(256));w32(o+8,nn);ndcmd++;
    {uint32_t j;for(j=0;j<nn;j++){put(want,&nwant,3);put(want,&nwant,u32(vb+(f0+j+1)*r_st));put(want,&nwant,u32(vb+(f0+j+2)*r_st));put(want,&nwant,u32(vb+f0*r_st));}}}
   else if(c==15){ /* MULTIPLYTRANSFORM: world, as DirectX 8's 256 */
    uint8_t *o=cmd(65,1,68);float m[16],pr[16];uint32_t j;for(j=0;j<16;j++)m[j]=(float)rnd(9)-4;w32(o,256);memcpy(o+4,m,64);
    dx8_mul(pr,m,gm);memcpy(gm,pr,64);wstate[nwstate++]=36;wstate[nwstate++]=1;wstate[nwstate++]=1;for(j=0;j<15;j++)memcpy(&wstate[nwstate++],&pr[j],4);}
   else if(c==16){ /* BUFFERBLT from VB 1 into VB 0 */
    uint32_t sz=4*(1+rnd(200)),so=4*rnd(gbl[1].size/4-sz/4),dof=4*rnd(gbl[0].size/4-sz/4);uint8_t *o=cmd(64,1,24);
    w32(o,sh[0]);w32(o+4,sh[1]);w32(o+8,dof);w32(o+12,so);w32(o+16,sz);w32(o+20,0);memmove(shadow0+dof,mem[1]+so,sz);}
   else if(c==17){ /* a declaration (XYZ | DIFFUSE | TEX1) as the vertex shader */
    static const uint32_t d[]={0x20000000,0x40020000,0x40040005,0x40010007,0xFFFFFFFF};uint8_t *o=cmd(45,1,12+20);uint32_t h=(1+rnd(20))*2+1;
    w32(o,h);w32(o+4,20);w32(o+8,0);memcpy(o+12,d,20);o=cmd(47,1,4);w32(o,h);r_fvf=0x142;}
   else if(c==4){uint8_t *o=cmd(8,2,16);uint32_t j;for(j=0;j<4;j++)w32(o+j*4,rnd(100));wstate[nwstate++]=8;wstate[nwstate++]=2;for(j=0;j<4;j++)wstate[nwstate++]=u32(o+j*4);}
   else {/* a draw */
    uint32_t t=1+rnd(6),indexed=r_ibset&&rnd(2),two=rnd(3)==0,big=rnd(40)==0,n=big?65530+rnd(70000):1+rnd(300),st=r_st;
    const uint8_t *vb;uint32_t vcount;
    if(!r_fvf||!st)continue;
    if(r_um){vb=um+p.dwVertexOffset;vcount=p.dwVertexLength;}else{vb=r_s0?mem[r_s0]:shadow0;vcount=gbl[r_s0].size/st;}
    if(!indexed){uint32_t nv=prim_v(t,n),first;if(nv>vcount)continue;first=rnd(vcount-nv+1);
     if(two&&!r_um){uint32_t off=first*st+(rnd(3)==0?rnd(st/4)*4:0);if(off+nv*st>gbl[r_s0].size)continue;uint8_t *o=cmd(59,1,12);w32(o,t);w32(o+4,off);w32(o+8,n);
      ref_prims(t,vb+off,(gbl[r_s0].size-off)/st,st,0,n,0,2,0,0);ndcmd++;}
     else{uint8_t *o=cmd(52,1,12);w32(o,t);w32(o+4,first);w32(o+8,n);ref_prims(t,vb,vcount,st,first,n,0,2,0,0);ndcmd++;if(verbose)printf("dx8 draw t %u first %u n %u st %u um %u\n",t,first,n,st,r_um);}}
    else{uint32_t ni=prim_v(t,n),ibcap=gbl[r_ib].size/r_is,ix0,mn,nv,j;long long base;uint8_t *ib=mem[r_ib];
     if(ibcur[r_ib]*2/r_is+ni>ibcap)continue;ix0=ibcur[r_ib]*2/r_is+rnd(64);if(ix0+ni>ibcap)ix0=ibcap-ni;ibcur[r_ib]=(ix0+ni)*r_is/2+2; /* regions apart: the buffer is read when the call runs */
     nv=1+rnd(r_is==4&&rnd(3)==0?vcount:vcount<65536?vcount:65536);mn=rnd(r_is==4?100000:65536-(nv<65536?nv:65535));if(r_is==2&&mn+nv>65536)nv=65536-mn;
     base=(long long)rnd(vcount)-(long long)mn;if(rnd(4)==0)base=-(long long)rnd(mn+1);
     for(j=0;j<ni;j++){uint32_t v=mn+rnd(nv);if(r_is==2)memcpy(ib+(ix0+j)*2,&(uint16_t){(uint16_t)v},2);else memcpy(ib+(ix0+j)*4,&v,4);}
     if(two&&!r_um){uint8_t *o=cmd(60,1,24);w32(o,t);w32(o+4,(uint32_t)(int32_t)(base*st));w32(o+8,mn);w32(o+12,nv);w32(o+16,ix0*r_is);w32(o+20,n);}
     else{uint8_t *o=cmd(53,1,24);w32(o,t);w32(o+4,(uint32_t)(int32_t)base);w32(o+8,mn);w32(o+12,nv);w32(o+16,ix0);w32(o+20,n);}
     ref_prims(t,vb,vcount,st,0,n,ib,r_is,ix0,base);ndcmd++;if(verbose)printf("dx8 ix%s t %u base %lld mn %u nv %u ix0 %u n %u is %u st %u um %u vcount %u\n",two&&!r_um?"2":"",t,base,mn,nv,ix0,n,r_is,st,r_um,vcount);}
   }}
  memset(&src,0,sizeof(src));src.slot=&contexts[0];src.args=&p;src.stride=ust;memset(&sv,0,sizeof(sv));sv.context=&src;sv.bind_vertices=bind;
  memset(&in,0,sizeof(in));in.commands=cbuf;in.command_bytes=clen;
  if(verbose){uint32_t q=0;while(q<clen){uint32_t op=cbuf[q],n=u16(cbuf+q+2);printf("cmd@%u op %u n %u: %u %u %u %u %u\n",q,op,n,u32(cbuf+q+4),u32(cbuf+q+8),u32(cbuf+q+12),u32(cbuf+q+16),u32(cbuf+q+20));
   q+=4+(op==47?4*n:op==49?12:op==50||op==51?8:op==52||op==59||op==58?12:op==53||op==60?24:op==8?16:op==65?68:op==64?24:op==45?12+u32(cbuf+q+8)+u32(cbuf+q+12):0);}}
  inject=-1;ntok=0;sticky=0;
  if(rnd(4)==0){ /* a busy answer that lasts, from a random DirectX 7 draw token on: the layer gives up after its wait; the failing command must be a DirectX 8 draw, everything before it drawn */
   uint32_t saved_n=nwant,ok=0,j;inject=rnd(8);sticky=-1;
   memcpy(cbuf+(1<<21),cbuf,clen); /* the same buffer runs twice */
   for(j=0;j<NB;j++)dx8_buf_set(1,0x1000,sh[j],(uintptr_t)&lcl[j]);
   {dx8_ctx save=*x;float tw[16];uint8_t *m0=malloc(gbl[0].size);memcpy(m0,mem[0],gbl[0].size);memcpy(tw,tm[1],64);in.commands=cbuf;in.command_bytes=clen;src.args=&p;src.stride=ust;memset(&sv,0,sizeof(sv));sv.context=&src;sv.bind_vertices=bind;
    rc=dx8_dp2(&contexts[0],&p,&in,&sv,&src,&err);*x=save;memcpy(tm[1],tw,64);memcpy(mem[0],m0,gbl[0].size);free(m0);}
   if(rc==0x8876021CUL){for(j=0;j<ndcmd;j++)if(dcmd[j]==err)ok=1;
    if(!ok){printf("iter %d: busy reported at %u, not a draw command\n",iter,err);fails++;}
    else{uint32_t cut=0;for(j=0;j<ndcmd;j++)if(dcmd[j]==err)cut=wcut[j];
     {uint32_t *wc=malloc((cut+1)*4);memcpy(wc,want,cut*4);cut=clean(wc,cut);ngot=clean(got,ngot);
     if(ngot<cut||memcmp(got,wc,cut*4)){free(wc);printf("iter %d: before the busy command %u words drawn, %u wanted\n",iter,ngot,cut);fails++;}else free(wc);}}}
   else if(rc){printf("iter %d: injected run rc %X\n",iter,rc);fails++;}
   ngot=0;ngstate=0;inject=-1;sticky=0;(void)saved_n;}
  ntok=0;if(rnd(3)==0)inject=rnd(12); /* a busy answer once, at a random token: the layer gives the rest again itself; nothing twice, nothing left out */
  memset(&src,0,sizeof(src));src.slot=&contexts[0];src.args=&p;src.stride=ust;memset(&sv,0,sizeof(sv));sv.context=&src;sv.bind_vertices=bind;
  rc=dx8_dp2(&contexts[0],&p,&in,&sv,&src,&err);inject=-1;
  if(rc){printf("iter %d: rc %X at %u (op %u: %u %u %u %u %u %u) s0um %u st %u\n",iter,rc,err,cbuf[err],u32(cbuf+err+4),u32(cbuf+err+8),u32(cbuf+err+12),u32(cbuf+err+16),u32(cbuf+err+20),u32(cbuf+err+24),x->s0um,x->s0stride);fails++;continue;}
  ngot=clean(got,ngot);nwant=clean(want,nwant);
  if(ngot!=nwant||memcmp(got,want,ngot*4)){uint32_t j=0;while(j<ngot&&j<nwant&&got[j]==want[j])j++;printf("iter %d: %u/%u words, first difference at %u (%X vs %X)\n",iter,ngot,nwant,j,j<ngot?got[j]:0,j<nwant?want[j]:0);fails++;}
  if(ngstate!=nwstate||memcmp(gstate,wstate,ngstate*4)){printf("iter %d: state commands differ (%u/%u)\n",iter,ngstate,nwstate);fails++;}
  if(fails>10)break;
 }
 printf("dx8_host: %d iterations, %u failures; segments %u, rebases %u, full flushes %u; gathered draws %u, primitive-by-primitive %u\n",iter,fails,vx_d3d_dx8_stat[DX8S_SEGS],vx_d3d_dx8_stat[DX8S_REBASE],vx_d3d_dx8_stat[DX8S_FLUSHFULL],vx_d3d_dx8_stat[DX8S_GATHER],vx_d3d_dx8_stat[DX8S_DEINDEX]);
 fails+=phase2(iters);fails+=phase3();
 return fails!=0;
}
