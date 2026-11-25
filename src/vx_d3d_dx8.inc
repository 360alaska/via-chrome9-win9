/* DX8 DDI: the DirectX 8 DDI (GetDriverInfo2, D3DCAPS8, the DX8 format list, the DX8 DrawPrimitives2 tokens), behind the live switch
 * VXREG DX8 ON / OFF (TEXDBG BC 458000x; the HAL takes it when it is built: DirectDraw's start or a mode change). Off by default.
 *
 * Why: under the DirectX 7 DDI, D3D8.DLL turns every DirectX 8 call into DirectX 7 tokens itself (it copies index buffers into the
 * command buffer and works out a vertex format from each declaration). With the DirectX 8 DDI the runtime hands the driver its vertex and
 * index buffers by handle, and the pixel-shader stage (ps.1.x, a later step) needs this DDI at all.
 *
 * How: a DirectX 8 context's command buffer is translated, command by command, into the DirectX 7 tokens the core already draws, in a
 * scratch buffer, and given to vx_d3d_dp2 in segments. A segment is a run of commands that read one vertex source (stream 0: a vertex
 * buffer by handle, or the call's own user-memory vertices), with one vertex format, from one byte offset. State commands are copied as they
 * are (a SETTRANSFORM's DirectX 8 world types 256-259 renumbered 1, 4-6); draws are rewritten:
 *   DRAWPRIMITIVE (52), DRAWPRIMITIVE2 (59)      -> POINTS (1), LINELIST (15), LINESTRIP (16), TRIANGLELIST (18), STRIP (19), FAN (21)
 *   DRAWINDEXEDPRIMITIVE (53), ..PRIMITIVE2 (60) -> INDEXEDLINELIST2 (27), INDEXEDLINESTRIP (17), INDEXEDTRIANGLELIST2 (26), INDEXEDTRIANGLESTRIP
 *                                                   (20), INDEXEDTRIANGLEFAN (22); indexed points as POINTS runs of one; the indices copied
 *                                                   from the index buffer (rebased when the base vertex or the index range needs it)
 *   CLIPPEDTRIANGLEFAN (58)                      -> TRIANGLEFAN_IMM (23) with its edge flags and its vertices inline
 * The commands that change the source (SETVERTEXSHADER 47, SETSTREAMSOURCE 49, SETSTREAMSOURCEUM 50) end a segment; so do BUFFERBLT (64,
 * done here between segments), MULTIPLYTRANSFORM (65, sent as a SETTRANSFORM of the product) and DELETEVERTEXSHADER (46). Vertex shaders
 * are fixed-function declarations (VertexShaderVersion 0: Direct3D 8 runs a programmable one itself and sends its transformed vertices):
 * a declaration that is stream 0 in an FVF's own order draws from the stream as it lies; any other (streams 0-15, any order, skips, texture
 * sets left out) has its vertices gathered into FVF vertices (see "gathered vertices" below), as do indices that span more than 65536
 * vertices and fans too long for one token. The texture filter states arrive in DirectX 8's values and are made DirectX 7's (dx8_filter).
 * Pixel shaders: version 0 (handle 0 accepted, any other counted). ADDDIRTYRECT / BOX: nothing
 * to do (textures are sampled where they lie). A busy answer (WASSTILLDRAWING) from the core is taken up here: the rest of the segment is
 * given again (dx8_flush). Any other failure, and a busy that lasts, reports the source command its failing token came from, so the
 * runtime's retry starts at the right DirectX 8 command; everything before it was drawn, and every change of the context's DirectX 8
 * state (shader, streams, indices) ends the segment, so that command runs again with the state it had.
 *
 * vx_d3d_dx8_stat (VX_D3D_QueryPhases words 561-600; 560 the switch as the HAL was built): see DX8S_* below. Board tool: VXDX8. Host test: tests/dx8_host.c. */
static uint32_t dx8_u32(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint32_t dx8_u16(const uint8_t *p) {return (uint32_t)p[0]|(uint32_t)p[1]<<8;}
#define u32 dx8_u32
#define u16 dx8_u16
#define DX8_BUFS 1024U
#define DX8_SHADERS 256U
#define DX8_SB (256UL*1024UL)
#define DX8_MAP 4096U
#define DX8_GB (512UL*1024UL) /* the gather buffer: vertices put together from their streams, at most 65535 a segment */
#define DX8_ELEMS 15U
#define DX8_STRIDE_MAX 2048U
typedef struct {uint32_t owner,local,handle;uintptr_t lcl;DDRAWI_DDRAWSURFACE_LCL l;DDRAWI_DDRAWSURFACE_GBL g;} dx8_buf; /* E787: l, g: the surface as CreateSurfaceEx showed it (below) */ /* a vertex or index buffer's handle (CreateSurfaceEx); owner 0 free, 0xFFFFFFFF a deleted slot */
typedef struct {uint8_t stream,pad;uint16_t bytes,soff,doff;} dx8_el; /* a vertex element: `bytes` at soff of its stream's vertex go to doff of the FVF vertex */
/* a vertex shader (a fixed-function declaration): the FVF vertex it describes (0: none does) and how that vertex is put together. direct: the
 * declaration is stream 0 in the FVF's own order with nothing skipped, so stream 0 is drawn as it lies; else each vertex is gathered:
 * el[] copied, in a zeroed vertex when the declaration leaves a texture coordinate set out (zero); smask the streams read, sneed[] the bytes
 * of a stream's vertex they reach */
typedef struct {uint32_t ctx,handle,fvf;uint8_t n,direct,zero,pad;uint16_t ostride,smask,sneed[16];dx8_el el[DX8_ELEMS];} dx8_vs;
/* mode: 0 nothing drawable, 1 direct (stream 0 as an FVF), 2 gathered (plan) */
typedef struct {uint32_t handle,owner,dx8,fvf,vs,s0h,s0stride,s0um,ibh,ibstride,ps,mode,sh[16],sst[16];dx8_vs plan;} dx8_ctx;
static dx8_buf dx8_bufs[DX8_BUFS];static dx8_vs dx8_vss[DX8_SHADERS];static dx8_ctx dx8_cs[CONTEXTS];
static uint8_t dx8_sb[DX8_SB];static uint32_t dx8_map[DX8_MAP][2];static uint8_t dx8_gb[DX8_GB];
uint32_t vx_d3d_dx8_on; /* the DirectX 8 DDI advertised by this HAL build (VX_D3D_init) */
uint32_t vx_d3d_dx8_stat[40];
enum {DX8S_CALLS,DX8S_SEGS,DX8S_DRAW,DX8S_DRAWIX,DX8S_DRAW2,DX8S_DRAWIX2,DX8S_CLIPFAN,DX8S_REBASE,DX8S_IX32,DX8S_VSDROP,DX8S_PSDROP,
      DX8S_BUFBLT,DX8S_MULT,DX8S_BUFS,DX8S_BUFFULL,DX8S_BADBUF,DX8S_STREAMN,DX8S_DX7CTX,DX8S_UNKNOWN,DX8S_GDI2,DX8S_CAPS8,DX8S_FMT,
      DX8S_DXVER,DX8S_GDI2NO,DX8S_REC,DX8S_FLUSHFULL,DX8S_SCAN,DX8S_PATCH,DX8S_VSDECL,DX8S_VSFVF,DX8S_LASTVER,DX8S_FAIL,DX8S_PAST,
      DX8S_GATHER,DX8S_GVERTS,DX8S_DEINDEX,DX8S_TSSX,DX8S_VSGATHER,DX8S_FIXUP,DX8S_BUSY};
#define DX8_BUSY 0x8876021CUL /* DDERR_WASSTILLDRAWING */
#define DX8_PART_MS 2000UL /* E779: the longer wait when the busy answer comes inside a source command */
#define DX8_BUSY_MS 250UL /* a full ring empties in milliseconds; past this the answer goes back and the runtime waits */
#ifndef DX8_FIXUP
/* the core's texture filter states, held as they came while the context was not yet known to be a DirectX 8 one */
static uint32_t dx8_filter(uint32_t,uint32_t);
static void dx8_fix_tss(context_slot *c) {uint32_t s2,k;for(s2=0;s2<8;s2++)for(k=16;k<=18;k++)c->core.state.tss[s2][k]=dx8_filter(k,c->core.state.tss[s2][k]);}
#define DX8_FIXUP(c) dx8_fix_tss(c)
#endif
static uint32_t dx8_hash(uint32_t local,uint32_t h) {return ((local>>4)^(h*2654435761UL))%DX8_BUFS;}
/* the slot of (owner, local, handle), or 0 */
static dx8_buf *dx8_find(uint32_t owner,uint32_t local,uint32_t h) {
 uint32_t i=dx8_hash(local,h),k;
 for(k=0;k<DX8_BUFS;k++,i=(i+1)%DX8_BUFS){dx8_buf *b=&dx8_bufs[i];if(!b->owner)return 0;if(b->owner==owner&&b->local==local&&b->handle==h)return b;}
 return 0;
}
/* lcl 0: forget the handle. Returns 0 when the table is full. */
/* E787 (board, E786 on the VX855: with the interface on every stream was refused; the surface kept from CreateSurfaceEx read, at draw
 * time, memory address 00000004 and a size of 82C71EC4, its caps still an execute buffer's): the structures Direct3D 8 passes to
 * CreateSurfaceEx for a buffer are only good during that call (their memory is used again afterwards; E785 read through what lay there
 * and never came back). The driver keeps its own copy of the local and global parts, taken in the call: the address, the size and the
 * caps are read from the copy. A buffer the runtime moves is announced by another CreateSurfaceEx with the same handle. */
static void dx8_buf_copy(dx8_buf *b,uintptr_t lcl) {const DDRAWI_DDRAWSURFACE_LCL *s=(const DDRAWI_DDRAWSURFACE_LCL*)lcl;b->lcl=lcl;b->l=*s;b->g=*s->lpGbl;b->l.lpGbl=&b->g;}
static int dx8_buf_set(uint32_t owner,uint32_t local,uint32_t h,uintptr_t lcl) {
 dx8_buf *b=dx8_find(owner,local,h);uint32_t i,k;
 if(b){if(lcl)dx8_buf_copy(b,lcl);else{b->owner=0xFFFFFFFFUL;b->lcl=0;vx_d3d_dx8_stat[DX8S_BUFS]--;}return 1;}
 if(!lcl)return 1;
 for(i=dx8_hash(local,h),k=0;k<DX8_BUFS;k++,i=(i+1)%DX8_BUFS){b=&dx8_bufs[i];
  if(!b->owner||b->owner==0xFFFFFFFFUL){b->owner=owner;b->local=local;b->handle=h;dx8_buf_copy(b,lcl);vx_d3d_dx8_stat[DX8S_BUFS]++;return 1;}}
 vx_d3d_dx8_stat[DX8S_BUFFULL]++;return 0;
}
/* a buffer's surface by handle: the driver's copy of it (0: not one) */
static DDRAWI_DDRAWSURFACE_LCL *dx8_buf_get(const context_slot *c,uint32_t h) {
 dx8_buf *b=h?dx8_find(c->owner,c->local,h):0;DDRAWI_DDRAWSURFACE_LCL *s;
 if(!b||!b->lcl)return 0;s=(DDRAWI_DDRAWSURFACE_LCL*)&b->l; /* E787: the driver's copy */
 if(!(s->ddsCaps.dwCaps&DDSCAPS_EXECUTEBUFFER)||!s->lpGbl->fpVidMem){vx_d3d_dx8_stat[DX8S_BADBUF]++;return 0;}
 return s;
}
static void dx8_buf_forget(uintptr_t lcl,uint32_t local,uint32_t owner) { /* by surface, by DirectDraw local, or by process (the others 0); a
 * context's shaders go when its slot holds the next context (dx8_of) */
 uint32_t i;for(i=0;i<DX8_BUFS;i++){dx8_buf *b=&dx8_bufs[i];if(!b->owner||b->owner==0xFFFFFFFFUL)continue;
  if((lcl&&b->lcl==lcl)||(local&&b->local==local)||(owner&&b->owner==owner)){b->owner=0xFFFFFFFFUL;b->lcl=0;vx_d3d_dx8_stat[DX8S_BUFS]--;}}
}
static void dx8_buf_dead(void) {uint32_t i;for(i=0;i<DX8_BUFS;i++){dx8_buf *b=&dx8_bufs[i];if(b->owner&&b->owner!=0xFFFFFFFFUL&&!owner_alive(b->owner)){b->owner=0xFFFFFFFFUL;b->lcl=0;vx_d3d_dx8_stat[DX8S_BUFS]--;}}}
/* this context's DirectX 8 state, started again when the slot holds another context */
static dx8_ctx *dx8_of(const context_slot *c) {
 dx8_ctx *x=&dx8_cs[c-contexts];
 if(x->handle!=c->handle||x->owner!=c->owner){uint32_t i;for(i=0;i<DX8_SHADERS;i++)if(dx8_vss[i].ctx==x->handle&&x->handle)memset(&dx8_vss[i],0,sizeof(dx8_vss[i]));
  memset(x,0,sizeof(*x));x->handle=c->handle;x->owner=c->owner;}
 return x;
}
/* A DirectX 8 fixed-function vertex shader declaration as the FVF vertex it describes and the plan that puts that vertex together.
 * Declaration tokens: bits 29-31 the token type (0 NOP, 1 STREAM, 2 STREAMDATA, 3 TESSELLATOR, 4 CONSTMEM, 5 EXT, 7 END); STREAM bits 0-3
 * the stream (bit 28: the tessellator's); STREAMDATA bit 28 a skip of bits 16-19 DWORDs, else bits 16-19 the type (0-3 FLOAT1-4, 4 D3DCOLOR,
 * 5 UBYTE4, 6 SHORT2, 7 SHORT4) and bits 0-4 the register (0 position, 1 blend weights, 2 blend indices, 3 normal, 4 point size, 5 diffuse,
 * 6 specular, 7-14 texture coordinates 0-7; 15, 16 the tweening pair, not offered). The fixed-function types: position and normal FLOAT3,
 * weights FLOAT1-4, indices UBYTE4 or D3DCOLOR, point size FLOAT1, the colours D3DCOLOR, texture coordinates FLOAT1-4. An FVF lays these
 * out in register order with no gaps and texture sets counted from 0: a declaration that is exactly that, in stream 0, is drawn from the
 * stream as it lies (direct); any other order, skips, other streams and texture sets left out (given as zeros, two floats) are gathered.
 * Returns 0 when no FVF vertex describes the declaration (o->fvf 0). */
static int dx8_decl_parse(const uint8_t *d,uint32_t bytes,dx8_vs *o) {
 static const uint8_t size[8]={4,8,12,16,4,4,4,8};
 uint32_t k,n=bytes/4,stream=0,soff[16],have=0,last=0xFFFFFFFFUL,direct=1,dt_of[15],st_of[15],so_of[15],reg,ntex=0,w=0,betas,doff=0,fvf=0;
 memset(soff,0,sizeof(soff));o->fvf=0;o->n=0;o->direct=0;o->zero=0;o->ostride=0;o->smask=0;memset(o->sneed,0,sizeof(o->sneed));
 for(k=0;k<n;k++){uint32_t t=u32(d+k*4),ty=t>>29,dt;
  if(t==0xFFFFFFFFUL)break;
  if(ty==0)continue;
  if(ty==1){if(t&0x10000000UL)return 0;stream=t&15;if(stream)direct=0;continue;} /* the tessellator's stream: not offered */
  if(ty==4){k+=((t>>25)&15)*4;continue;}                /* constants: a programmable shader's, not the vertices' */
  if(ty==5){k+=(t>>24)&31;continue;}
  if(ty!=2)return 0;                                    /* the tessellator */
  if(t&0x10000000UL){soff[stream]+=((t>>16)&15)*4;direct=0;continue;}
  reg=t&31;dt=(t>>16)&15;
  if(reg>14||dt>7||(have&(1UL<<reg)))return 0;
  if(reg==0||reg==3){if(dt!=2)return 0;}
  else if(reg==1){if(dt>3)return 0;}
  else if(reg==2){if(dt!=5&&dt!=4)return 0;}
  else if(reg==4){if(dt!=0)return 0;}
  else if(reg==5||reg==6){if(dt!=4)return 0;}
  else if(dt>3)return 0;
  if(last!=0xFFFFFFFFUL&&reg<=last)direct=0;last=reg;
  have|=1UL<<reg;dt_of[reg]=dt;st_of[reg]=stream;so_of[reg]=soff[stream];soff[stream]+=size[dt];
  if(soff[stream]>DX8_STRIDE_MAX)return 0;}
 if(!(have&1))return 0;
 for(reg=7;reg<=14;reg++)if(have&(1UL<<reg))ntex=reg-6;
 for(reg=7;reg<7+ntex;reg++)if(!(have&(1UL<<reg))){direct=0;o->zero=1;}
 w=(have&2)?dt_of[1]+1:0;betas=w+((have&4)?1:0);
 if(betas>5)return 0;
 fvf=betas?(4UL+2UL*betas):2UL; /* XYZ, XYZB1-5: the core passes the betas over, four bytes each (no vertex blending is offered) */
 #define DX8_EL(R,B) do{if(have&(1UL<<(R))){dx8_el *e=&o->el[o->n++];e->stream=(uint8_t)st_of[R];e->pad=0;e->bytes=(uint16_t)(B);e->soff=(uint16_t)so_of[R];e->doff=(uint16_t)doff;\
   o->smask|=(uint16_t)(1U<<st_of[R]);if(so_of[R]+(B)>o->sneed[st_of[R]])o->sneed[st_of[R]]=(uint16_t)(so_of[R]+(B));}doff+=(B);}while(0)
 DX8_EL(0,12);
 if(have&2)DX8_EL(1,w*4);
 if(have&4)DX8_EL(2,4);
 if(have&8){fvf|=0x10;DX8_EL(3,12);}
 if(have&16){fvf|=0x20;DX8_EL(4,4);}
 if(have&32){fvf|=0x40;DX8_EL(5,4);}
 if(have&64){fvf|=0x80;DX8_EL(6,4);}
 for(reg=7;reg<7+ntex;reg++){static const uint8_t code[4]={3,0,1,2};
  if(have&(1UL<<reg)){fvf|=(uint32_t)code[dt_of[reg]]<<(16+2*(reg-7));DX8_EL(reg,size[dt_of[reg]]);}else doff+=8;}
 #undef DX8_EL
 o->fvf=fvf|ntex<<8;o->ostride=(uint16_t)doff;o->direct=(uint8_t)direct;
 return 1;
}
static const dx8_vs *dx8_vs_find(uint32_t ctx,uint32_t h) {uint32_t i;for(i=0;i<DX8_SHADERS;i++)if(dx8_vss[i].ctx==ctx&&dx8_vss[i].handle==h)return &dx8_vss[i];return 0;}
static int dx8_vs_set(uint32_t ctx,uint32_t h,const dx8_vs *v,int del) { /* E776 (outside audit of E768, #3): 0 when the table is full (it returned as if stored: the handle then bound to nothing and every draw after it was skipped) */
 uint32_t i,fr=DX8_SHADERS;for(i=0;i<DX8_SHADERS;i++){if(dx8_vss[i].ctx==ctx&&dx8_vss[i].handle==h){fr=i;break;}if(!dx8_vss[i].ctx&&fr==DX8_SHADERS)fr=i;}
 if(fr==DX8_SHADERS)return del?1:0;
 if(del){if(dx8_vss[fr].ctx==ctx&&dx8_vss[fr].handle==h)memset(&dx8_vss[fr],0,sizeof(dx8_vss[fr]));return 1;}
 dx8_vss[fr]=*v;dx8_vss[fr].ctx=ctx;dx8_vss[fr].handle=h;return 1;
}
/* E776 (#3): a context's declarations given up when it is destroyed (they stayed until its slot was taken by another context) */
static void dx8_ctx_gone(uint32_t handle) {uint32_t i;if(!handle)return;for(i=0;i<DX8_SHADERS;i++)if(dx8_vss[i].ctx==handle)memset(&dx8_vss[i],0,sizeof(dx8_vss[i]));}
/* the context's vertex shader made current: an FVF code (even), or a declaration's handle (odd) */
static void dx8_vs_use(dx8_ctx *x,uint32_t ctx,uint32_t h) {
 x->vs=h;x->fvf=0;x->mode=0;
 if(!(h&1)){x->fvf=h;x->mode=h?1:0;return;}
 {const dx8_vs *v=dx8_vs_find(ctx,h);if(v&&v->fvf){x->fvf=v->fvf;x->mode=v->direct?1:2;x->plan=*v;}}
}
/* The length of a DirectX 7 state command the runtime sends a DirectX 8 driver (the core's own reading), or 0xFFFFFFFF when not one of them */
static uint32_t dx8_dx7_len(uint32_t op,uint32_t n,const uint8_t *p,uint32_t room) {
 uint32_t j,len=0;
 switch(op){
  case 8:case 25:case 41:case 29:case 32:case 40:case 43:return n*8;
  case 28:return n*16;
  case 39:case 30:return n*12;
  case 42:return 16+n*16;
  case 33:case 36:return n*68;
  case 35:return n*4;
  case 44:return n*20;
  case 38:return n*36;
  case 34:for(j=0;j<n;j++){if(len>room||room-len<8)return 0xFFFFFFFEUL;len+=u32(p+len+4)==2?112:8;}return len;
  case 31:for(j=0;j<n;j++){if(len>room||room-len<8)return 0xFFFFFFFEUL;len+=8+(uint32_t)u16(p+len+6)*4;}return len;
 }
 return 0xFFFFFFFFUL;
}
static int dx8_is_draw7(uint32_t op) {return op==1||op==2||op==3||(op>=15&&op<=24)||op==26||op==27;}
/* Whether this call is a DirectX 8 context's: decided once per context, at the first DirectX 8 token (a DX8 context sends SETVERTEXSHADER
 * and SETSTREAMSOURCE before any draw) or the first DirectX 7 draw (a DirectX 7 runtime's context: D3DIM700 talks to this driver too) */
static int dx8_context(dx8_ctx *x,const uint8_t *cmd,uint32_t bytes) {
 uint32_t pos=0;
 if(x->dx8&0xFF)return (x->dx8&0xFF)==1;
 vx_d3d_dx8_stat[DX8S_SCAN]++;
 while(bytes-pos>=4){uint32_t op=cmd[pos],n=u16(cmd+pos+2),len;
  if(op>=45&&op<=67){x->dx8=1;return 1;}
  if(dx8_is_draw7(op)){x->dx8=2;vx_d3d_dx8_stat[DX8S_DX7CTX]++;return 0;}
  len=dx8_dx7_len(op,n,cmd+pos+4,bytes-pos-4);if(len>=0xFFFFFFFEUL||len>bytes-pos-4)return 0;pos+=4+len;}
 return 0;
}
/* ---- the translation of one call ---- */
typedef struct {
 context_slot *c;dx8_ctx *x;const vx_ddi_dp2 *p;dp2_source *src;vx_d3d_services *sv;const uint8_t *cmd;
 uint32_t used,nmap,draws,seg_off,seg_need,seg_set,src_cmd;
 uint32_t drop_ok,drop_src; /* E779B: the source command whose rest is being left out (its later segments are not sent, and not waited for) */
 uint32_t done_ok,done_src; /* E779: the source command of the last token the core took in an earlier segment of this call */
 uint32_t kind,gn,g_fvf,g_stride,seq,um_vs; /* kind: the segment's vertices: 1 stream 0 as it lies, 2 the gather buffer (gn vertices of g_fvf, g_stride); seq: segments sent; um_vs: the call's user-memory vertex size */
 dx8_vs p1;                                 /* stream 0 as a one-element plan (dx8_plan1) */
} dx8_run_t;
/* the segment's vertex source as a DrawPrimitives2 argument block, for bind_vertices */
static uint32_t dx8_flush(dx8_run_t *r,uint32_t *err_src) {
 vx_ddi_dp2 q;vx_d3d_input in;uint32_t rc,err=0,k,st=r->x->s0stride,at0=0;
 if(!r->used)return 0;
 q=*r->p;q.dwVertexType=r->x->fvf;
 if(r->kind==2){q.dwFlags|=1UL;q.lpVertices=dx8_gb;q.dwVertexOffset=0;q.dwVertexType=r->g_fvf;st=r->g_stride;}
 else if(r->x->s0um){q.dwVertexOffset=r->p->dwVertexOffset+r->seg_off;
  /* the user-memory vertices the runtime says it passed (dwVertexLength of its vertex size) bound the segment: a draw's claimed range past them is read as missing vertices */
  if(r->p->dwVertexLength){uint32_t vs=r->um_vs?r->um_vs:r->x->s0stride,have=r->p->dwVertexLength<=0xFFFFFFFFUL/vs?r->p->dwVertexLength*vs:0xFFFFFFFFUL,use=have>r->seg_off?(have-r->seg_off)/r->x->s0stride:0;
   if(r->seg_need>use)r->seg_need=use;}}
 else{DDRAWI_DDRAWSURFACE_LCL *s=r->draws?dx8_buf_get(r->c,r->x->s0h):0;
  if(r->draws&&!s){*err_src=r->src_cmd;r->used=0;r->nmap=0;return VX_D3D_INVALID;}
  q.lpDDVertex=s;q.dwFlags&=~1UL;q.dwVertexOffset=r->seg_off;
  /* E786 (board, E785: the first DirectX 8 draw stopped at the T&L's first read of the stream's memory): the stream's memory is tried
   * before the core reads it; memory that cannot be read refuses the segment (counted as a bad buffer) */
  if(s){extern volatile uint32_t vx_d3d_bd[16],vx_d3d_bc[24];const uint32_t by=surface_bytes(s);uint32_t n=by>r->seg_off?by-r->seg_off:0;if(n>65536UL)n=65536UL;
   vx_d3d_bd[8]=(uint32_t)(uintptr_t)s;vx_d3d_bd[9]=r->x->s0h;vx_d3d_bd[10]=(uint32_t)s->lpGbl->fpVidMem;vx_d3d_bd[11]=s->ddsCaps.dwCaps;vx_d3d_bd[12]=by;vx_d3d_bd[13]=r->seg_off;vx_d3d_bd[14]=n;vx_d3d_bd[15]=0;
   vx_d3d_bc[2]=80;
   if(!n||IsBadReadPtr((const uint8_t*)s->lpGbl->fpVidMem+r->seg_off,n)){vx_d3d_bd[15]=2;vx_d3d_bc[2]=82;vx_d3d_dx8_stat[DX8S_BADBUF]++;*err_src=r->src_cmd;r->used=0;r->nmap=0;return VX_D3D_INVALID;}
   vx_d3d_bd[15]=1;vx_d3d_bc[2]=81;}}
 q.dwVertexLength=r->kind==2?r->gn:r->seg_need;
 r->src->args=&q;r->src->stride=st;
 memset(&in,0,sizeof(in));in.commands=dx8_sb;in.command_bytes=r->used;in.flags=q.dwFlags;in.render_states=(uint32_t*)q.lpdwRStates;in.render_state_count=256;
 in.fvf=q.dwVertexType;in.vertex_stride=st;
 /* E779B: a later segment of a command whose rest is being left out (below): its tokens of that command are passed over without a call
  * or a wait (a draw of thousands of tokens would otherwise wait once a segment) */
 if(r->drop_ok&&r->nmap&&dx8_map[0][1]==r->drop_src){for(k=1;k<r->nmap;k++)if(dx8_map[k][1]!=r->drop_src)break;
  at0=k<r->nmap?dx8_map[k][0]:r->used;in.commands=dx8_sb+at0;in.command_bytes=r->used-at0;}
 /* a segment without a draw binds nothing: bind_vertices is called only for a draw */
 rc=in.command_bytes?vx_d3d_dp2(&r->c->core,&in,r->sv,&err):0;vx_d3d_dx8_stat[DX8S_SEGS]++;
 /* Busy: the rest of the segment is given again from the token that was not taken, here, as the runtime does with its own DirectX 7
  * tokens. Given back to the runtime, the busy answer names a DirectX 8 command, and one drawn in several tokens would be drawn again
  * from its first. After DX8_BUSY_MS the answer does go back (the command named, what came before it drawn). */
 /* E779 (outside audit of E768, #2): when the busy answer comes in the middle of a source command (tokens of it already taken, in this
  * segment or the one before), it is not given back: the runtime would send the whole command again and its first part would be
  * drawn twice (65,534 of 65,536 points drawn, then all 65,536 again). The wait goes on to DX8_PART_MS; still busy then, the rest of
  * that one command in this segment is left out (counted as a failed segment, as a held packet that stays busy is dropped) and the
  * commands after it are sent. A busy answer at a command's first token goes back as before. */
 if(rc!=DX8_BUSY)err+=at0;
 if(rc==DX8_BUSY){uint32_t t0=GetTickCount(),at=at0,lim=DX8_BUSY_MS;
  while(rc==DX8_BUSY&&err<in.command_bytes){
   if(GetTickCount()-t0<lim){at+=err;in.commands=dx8_sb+at;in.command_bytes=r->used-at;err=0;
    rc=vx_d3d_dp2(&r->c->core,&in,r->sv,&err);vx_d3d_dx8_stat[DX8S_BUSY]++;continue;}
   {const uint32_t pos=at+err;uint32_t s=r->nmap?dx8_map[0][1]:r->src_cmd,ks=0,o2=0xFFFFFFFFUL;
    for(k=0;k<r->nmap&&dx8_map[k][0]<=pos;k++){s=dx8_map[k][1];ks=k;}
    if(!r->nmap||!(dx8_map[ks][0]<pos||(ks==0&&r->done_ok&&r->done_src==s)))break; /* at a command's first token: the answer goes back */
    if(lim==DX8_BUSY_MS){lim=DX8_PART_MS;continue;}
    for(k=ks+1;k<r->nmap;k++)if(dx8_map[k][1]!=s){o2=dx8_map[k][0];break;}
    vx_d3d_dx8_stat[DX8S_FAIL]++;r->drop_ok=1;r->drop_src=s;
    if(o2==0xFFFFFFFFUL||o2>=r->used){rc=0;err=0;at=r->used;break;} /* nothing after it in this segment */
    at=o2;err=0;in.commands=dx8_sb+at;in.command_bytes=r->used-at;t0=GetTickCount();lim=DX8_BUSY_MS;
    rc=vx_d3d_dp2(&r->c->core,&in,r->sv,&err);vx_d3d_dx8_stat[DX8S_BUSY]++;}}
  err+=at;}
 if(!rc&&r->nmap){r->done_ok=1;r->done_src=dx8_map[r->nmap-1][1];}
 r->src->args=r->p;r->src->stride=r->um_vs; /* E775 (HAL-6): the caller's stride back after a segment's own */
 if(rc){uint32_t s=r->nmap?dx8_map[0][1]:r->src_cmd;for(k=0;k<r->nmap&&dx8_map[k][0]<=err;k++)s=dx8_map[k][1];*err_src=s;vx_d3d_dx8_stat[DX8S_FAIL]++;}
 r->used=0;r->nmap=0;r->draws=0;r->seg_need=0;r->seg_set=0;r->gn=0;r->seq++;
 return rc;
}
/* room for a token of `bytes` in the scratch buffer (the segment sent when full); its source command recorded */
static uint8_t *dx8_room(dx8_run_t *r,uint32_t bytes,uint32_t *rc,uint32_t *err_src) {
 uint8_t *o;
 if(bytes>DX8_SB){*rc=VX_D3D_INVALID;*err_src=r->src_cmd;return 0;}
 if(DX8_SB-r->used<bytes||r->nmap==DX8_MAP){uint32_t keep_off=r->seg_off,keep_set=r->seg_set,keep_need;vx_d3d_dx8_stat[DX8S_FLUSHFULL]++;
  keep_need=r->seg_need;*rc=dx8_flush(r,err_src);if(*rc)return 0;r->seg_off=keep_off;r->seg_set=keep_set;r->seg_need=keep_need;} /* the same source: the offset and the vertices the token placed stay */
 if(!r->nmap||dx8_map[r->nmap-1][1]!=r->src_cmd){dx8_map[r->nmap][0]=r->used;dx8_map[r->nmap][1]=r->src_cmd;r->nmap++;}
 o=dx8_sb+r->used;r->used+=bytes;return o;
}
static void dx8_w16(uint8_t *o,uint32_t v) {o[0]=(uint8_t)v;o[1]=(uint8_t)(v>>8);}
static void dx8_w32(uint8_t *o,uint32_t v) {o[0]=(uint8_t)v;o[1]=(uint8_t)(v>>8);o[2]=(uint8_t)(v>>16);o[3]=(uint8_t)(v>>24);}
static void dx8_hdr(uint8_t *o,uint32_t op,uint32_t n) {o[0]=(uint8_t)op;o[1]=0;dx8_w16(o+2,n);}
/* Make the vertex at byte `boff` of stream 0, with `span` vertices from it, addressable from the segment by a 16-bit start: the start
 * relative to the segment's offset, or a new segment (the current one sent) whose offset is boff. Returns the start, or -1 (with *rc). */
static long dx8_place(dx8_run_t *r,uint32_t boff,uint32_t span,int lim,uint32_t *rc,uint32_t *err_src) {
 /* lim: every vertex of the span must be 16-bit addressable too (indexed points: each a POINTS run's start) */
 const uint32_t st=r->x->s0stride;
 if(r->seg_set&&boff>=r->seg_off&&(boff-r->seg_off)%st==0&&(boff-r->seg_off)/st<=65535UL&&(!lim||(boff-r->seg_off)/st+span<=65536UL)){uint32_t v=(boff-r->seg_off)/st;if(v+span>r->seg_need)r->seg_need=v+span;return (long)v;}
 if(r->draws){vx_d3d_dx8_stat[DX8S_REBASE]++;*rc=dx8_flush(r,err_src);if(*rc)return -1;}
 r->seg_set=1;r->seg_off=boff%st;if((boff-r->seg_off)/st>65535UL||(lim&&(boff-r->seg_off)/st+span>65536UL))r->seg_off=boff; /* from the buffer's start when it reaches, so later draws share it */
 {uint32_t v=(boff-r->seg_off)/st;if(v+span>r->seg_need)r->seg_need=v+span;return (long)v;}
}
/* the bytes of stream 0 there are (user memory: as the call says, or unbounded when it says nothing) */
static uint32_t dx8_s0_avail(dx8_run_t *r) {
 if(r->x->s0um){uint32_t vs=r->um_vs?r->um_vs:r->x->s0stride;if(!r->p->dwVertexLength)return 0xFFFFFFFFUL;return r->p->dwVertexLength<=0xFFFFFFFFUL/vs?r->p->dwVertexLength*vs:0xFFFFFFFFUL;}
 {DDRAWI_DDRAWSURFACE_LCL *s=dx8_buf_get(r->c,r->x->s0h);return s?surface_bytes(s):0;}
}
/* ---- gathered vertices ----
 * A draw the stream cannot be drawn from as it lies (a declaration over several streams, in another order or with skips; indices that span
 * more than 65536 vertices; a fan too long for one token) has its vertices copied, element by element, into the gather buffer as the FVF
 * vertices the core draws; the segment is then bound as user-memory vertices. Indexed draws: the range the indices claim, gathered once a
 * segment and drawn with the indices made relative to it, or, when the range is wide against the indices (or does not fit), each
 * primitive's own vertices (lists, no indices). A vertex outside a stream is a missing vertex: its primitives are left out, as the core
 * leaves them out. */
typedef struct {const uint8_t *p;uint32_t bytes,stride;} dx8_sv;
/* the segment made of this kind (the one before sent when it holds draws of the other kind, or of another vertex) */
static uint32_t dx8_kind(dx8_run_t *r,uint32_t kind,const dx8_vs *d,uint32_t *err_src) {
 if(r->kind==kind&&(kind!=2||(r->g_fvf==d->fvf&&r->g_stride==d->ostride)))return 0;
 if(r->draws){uint32_t rc=dx8_flush(r,err_src);if(rc)return rc;}
 r->kind=kind;r->gn=0;r->seg_set=0;r->seg_need=0;
 if(kind==2){r->g_fvf=d->fvf;r->g_stride=d->ostride;}
 return 0;
}
/* room for a token of `bytes` without sending the segment later (dx8_room after this does not flush) */
static uint32_t dx8_need(dx8_run_t *r,uint32_t bytes,uint32_t *err_src) {
 if(bytes>DX8_SB){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 if(DX8_SB-r->used<bytes||r->nmap+1>=DX8_MAP){vx_d3d_dx8_stat[DX8S_FLUSHFULL]++;return dx8_flush(r,err_src);}
 return 0;
}
/* nv vertex slots in the gather buffer (the segment sent first when they do not fit): the first slot, or -1 */
static long dx8_groom(dx8_run_t *r,uint32_t nv,uint32_t *rc,uint32_t *err_src) {
 uint32_t cap=DX8_GB/r->g_stride;long v;
 if(cap>65535UL)cap=65535UL;
 if(nv>cap){*rc=VX_D3D_INVALID;*err_src=r->src_cmd;return -1;}
 if(cap-r->gn<nv){*rc=dx8_flush(r,err_src);if(*rc)return -1;}
 v=(long)r->gn;r->gn+=nv;return v;
}
/* the plan's streams as this draw reads them; *nvert the vertices every one of them holds. 0: a stream is not there */
static int dx8_streams(dx8_run_t *r,const dx8_vs *d,dx8_sv *sv,uint32_t *nvert) {
 uint32_t k,nv=0xFFFFFFFFUL;
 for(k=0;k<16;k++){uint32_t st,by,have;const uint8_t *p;
  if(!(d->smask&(1U<<k)))continue;
  if(k==0&&r->x->s0um){uint32_t vs=r->um_vs?r->um_vs:r->x->s0stride;
   if(!(r->p->dwFlags&1)||!r->p->lpVertices)return 0;
   p=(const uint8_t*)r->p->lpVertices+r->p->dwVertexOffset;st=r->x->s0stride;
   by=!r->p->dwVertexLength?0xFFFFFFFFUL:r->p->dwVertexLength<=0xFFFFFFFFUL/vs?r->p->dwVertexLength*vs:0xFFFFFFFFUL;}
  else{DDRAWI_DDRAWSURFACE_LCL *b=dx8_buf_get(r->c,k?r->x->sh[k]:r->x->s0h);if(!b)return 0;
   p=(const uint8_t*)b->lpGbl->fpVidMem;by=surface_bytes(b);st=k?r->x->sst[k]:r->x->s0stride;}
  if(!st)return 0;
  have=by>=d->sneed[k]?(by-d->sneed[k])/st+1:0; /* the vertices whose elements the stream holds, */
  if(have>by/st)have=by/st;                     /* whole ones (a stride with room after its elements), as a stream drawn direct counts them */
  if(have<nv)nv=have;
  sv[k].p=p;sv[k].bytes=by;sv[k].stride=st;}
 *nvert=nv==0xFFFFFFFFUL?0:nv;return 1;
}
static void dx8_gv(const dx8_vs *d,const dx8_sv *sv,uint32_t i,uint8_t *dst) {
 uint32_t k;
 if(d->zero)memset(dst,0,d->ostride);
 for(k=0;k<d->n;k++){const dx8_el *e=&d->el[k];memcpy(dst+e->doff,sv[e->stream].p+(size_t)i*sv[e->stream].stride+e->soff,e->bytes);}
}
/* stream 0 as it lies, as a plan (for the draws the direct way cannot address): `rem` the bytes into a vertex the draw's base lies */
static int dx8_plan1(dx8_run_t *r,uint32_t rem) {
 dx8_vs *d=&r->p1;uint32_t st=r->x->s0stride;
 if(!st||st>DX8_STRIDE_MAX||rem>=st)return 0;
 memset(d,0,sizeof(*d));d->fvf=r->x->fvf;d->n=1;d->ostride=(uint16_t)st;d->smask=1;d->sneed[0]=(uint16_t)(rem+st);
 d->el[0].bytes=(uint16_t)st;d->el[0].soff=(uint16_t)rem;return 1;
}
static const uint8_t dx8_op_nonix[7]={0,1,15,16,18,19,21},dx8_op_ix[7]={0,1,27,17,26,20,22};
/* vertices a primitive count uses (D3DPRIMITIVETYPE 1 points .. 6 fan) */
static uint32_t dx8_verts(uint32_t t,uint32_t n) {return t==1?n:t==2?n*2:t==3?n+1:t==4?n*3:(t==5||t==6)?n+2:0;}
/* a gathered draw without indices: `n` primitives from vertex `first` */
static uint32_t dx8_gdraw(dx8_run_t *r,const dx8_vs *d,uint32_t t,long long first,uint32_t n,uint32_t *err_src) {
 dx8_sv sv[16];uint32_t nvert,rc=0,cap,done=0,have,mx;
 if(t<1||t>6){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 if(!dx8_streams(r,d,sv,&nvert)){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 if(first<0||(unsigned long long)first>=nvert){vx_d3d_dx8_stat[DX8S_PAST]++;return 0;}
 have=nvert-(uint32_t)first;mx=t==1?have:t==2?have/2:t==3?have-1:t==4?have/3:(have>=2?have-2:0);
 if(n>mx){n=mx;vx_d3d_dx8_stat[DX8S_PAST]++;} /* the primitives whose vertices the streams hold */
 if(!n)return 0;
 rc=dx8_kind(r,2,d,err_src);if(rc)return rc;
 cap=DX8_GB/d->ostride;if(cap>65535UL)cap=65535UL;
 while(done<n){
  uint32_t m=n-done,room,nvp,j,f0;const uint32_t minv=t==1?1:(t==2||t==3)?2:t==5?4:3;long v;uint8_t *o;
  rc=dx8_need(r,8,err_src);if(rc)return rc;
  room=cap-r->gn;if(room<minv){rc=dx8_flush(r,err_src);if(rc)return rc;room=cap;}
  switch(t){
   case 1:if(m>room)m=room;nvp=m;f0=done;break;
   case 2:if(m>room/2)m=room/2;nvp=2*m;f0=2*done;break;
   case 3:if(m>room-1)m=room-1;nvp=m+1;f0=done;break;
   case 4:if(m>room/3)m=room/3;nvp=3*m;f0=3*done;break;
   case 5:if(m>room-2){m=room-2;if(m&1)m--;}nvp=m+2;f0=done;break; /* an even piece: the next keeps the strip's winding */
   default:if(m>room-2)m=room-2;nvp=m+2;f0=done+1;break;            /* a fan's piece: its centre again, then its run */
  }
  v=dx8_groom(r,nvp,&rc,err_src);if(v<0)return rc;
  o=dx8_room(r,t==1?8:6,&rc,err_src);if(!o)return rc;
  if(t==1){dx8_hdr(o,1,1);dx8_w16(o+4,m);dx8_w16(o+6,(uint32_t)v);}else{dx8_hdr(o,dx8_op_nonix[t],m);dx8_w16(o+4,(uint32_t)v);}
  if(t==6){dx8_gv(d,sv,(uint32_t)first,dx8_gb+(size_t)v*d->ostride);
   for(j=0;j<=m;j++)dx8_gv(d,sv,(uint32_t)first+f0+j,dx8_gb+(size_t)((uint32_t)v+1+j)*d->ostride);}
  else for(j=0;j<nvp;j++)dx8_gv(d,sv,(uint32_t)first+f0+j,dx8_gb+(size_t)((uint32_t)v+j)*d->ostride);
  r->draws++;done+=m;vx_d3d_dx8_stat[DX8S_GVERTS]+=nvp;}
 vx_d3d_dx8_stat[DX8S_GATHER]++;
 return 0;
}
/* primitive k's index positions, in the order the DirectX 7 tokens read a primitive's vertices */
static uint32_t dx8_prim_ix(uint32_t t,uint32_t k,uint32_t *a) {
 switch(t){
  case 1:a[0]=k;return 1;
  case 2:a[0]=2*k;a[1]=2*k+1;return 2;
  case 3:a[0]=k;a[1]=k+1;return 2;
  case 4:a[0]=3*k;a[1]=3*k+1;a[2]=3*k+2;return 3;
  case 5:a[0]=k;a[1]=k+1+(k&1);a[2]=k+2-(k&1);return 3;
  default:a[0]=k+1;a[1]=k+2;a[2]=0;return 3;}
}
/* a gathered indexed draw: indices from the index buffer at byte ioff, vertex = base + index (base signed), mn / nv the range they claim */
static uint32_t dx8_gdrawix(dx8_run_t *r,const dx8_vs *d,uint32_t t,long long base,uint32_t mn,uint32_t nv,uint32_t ioff,uint32_t n,uint32_t *err_src) {
 const uint32_t is=r->x->ibstride;dx8_sv sv[16];uint32_t nvert,rc=0,cap,ni,avail,done=0;DDRAWI_DDRAWSURFACE_LCL *ib;const uint8_t *ix;
 const long long start=base+(long long)mn;
 if(t<1||t>6||(is!=2&&is!=4)){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 ib=dx8_buf_get(r->c,r->x->ibh);if(!ib){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 if(!n)return 0;
 ni=dx8_verts(t,n);avail=surface_bytes(ib);
 if(ioff>avail||ni>(avail-ioff)/is){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 ix=(const uint8_t*)ib->lpGbl->fpVidMem+ioff;
 if(is==4)vx_d3d_dx8_stat[DX8S_IX32]++;
 if(!dx8_streams(r,d,sv,&nvert)){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 rc=dx8_kind(r,2,d,err_src);if(rc)return rc;
 cap=DX8_GB/d->ostride;if(cap>65535UL)cap=65535UL;
 #define DX8_IX(j) (is==2?u16(ix+(size_t)(j)*2):u32(ix+(size_t)(j)*4))
 if(start>=0&&(unsigned long long)start<nvert&&nv&&nv<=cap&&nv/4<=ni+16){ /* the claimed range, gathered once a segment */
  uint32_t nvg=nvert-(uint32_t)start,seq=0;long v0=-1;
  if(nvg>nv)nvg=nv;
  while(done<n){
   const uint32_t per=t==1?4:t==2?4:t==4?6:2,hdr=t==1?4:t==6?8:6;uint32_t m=n-done,first,cnt,k,tb;uint8_t *o,*q;
   if(m>65534UL)m=65534UL;
   if(m>(DX8_SB/2)/per)m=(DX8_SB/2)/per;
   if(t==5&&m<n-done&&(m&1))m--;
   first=t==2?done*2:t==4?done*3:t==6?done+1:done;cnt=t==6?m+1:dx8_verts(t,m);
   tb=t==1?4+m*4:hdr+cnt*2;
   rc=dx8_need(r,tb,err_src);if(rc)return rc;
   if(v0<0||seq!=r->seq){uint32_t j;v0=dx8_groom(r,nvg,&rc,err_src);if(v0<0)return rc;seq=r->seq;
    for(j=0;j<nvg;j++)dx8_gv(d,sv,(uint32_t)start+j,dx8_gb+(size_t)((uint32_t)v0+j)*d->ostride);
    vx_d3d_dx8_stat[DX8S_GVERTS]+=nvg;}
   o=dx8_room(r,tb,&rc,err_src);if(!o)return rc;
   /* an index outside the range gathered: 0xFFFF, past the at most 65535 vertices a segment binds: the core leaves its primitive out */
   if(t==1){dx8_hdr(o,1,m);for(k=0;k<m;k++){uint32_t i=DX8_IX(first+k)-mn;dx8_w16(o+4+k*4,1);dx8_w16(o+6+k*4,i<nvg?(uint32_t)v0+i:0xFFFFUL);}}
   else{dx8_hdr(o,dx8_op_ix[t],m);dx8_w16(o+4,(uint32_t)v0);q=o+6;
    if(t==6){uint32_t i=DX8_IX(0)-mn;dx8_w16(q,i<nvg?i:0xFFFFUL);q+=2;}
    for(k=0;k<cnt;k++){uint32_t i=DX8_IX(first+k)-mn;dx8_w16(q+k*2,i<nvg?i:0xFFFFUL);}}
   r->draws++;done+=m;}
  vx_d3d_dx8_stat[DX8S_GATHER]++;}
 else{ /* each primitive's own vertices: points, line and triangle lists */
  const uint32_t pv=t==1?1:(t==2||t==3)?2:3;uint32_t k=0;
  while(k<n){
   uint32_t room,mmax,cntp=0;long v;uint8_t *o;
   rc=dx8_need(r,8,err_src);if(rc)return rc;
   room=cap-r->gn;if(room<pv){rc=dx8_flush(r,err_src);if(rc)return rc;room=cap;}
   mmax=room/pv;v=(long)r->gn;
   while(k<n&&cntp<mmax){uint32_t a[3],j,ok=1;long long vi[3];
    dx8_prim_ix(t,k,a);
    for(j=0;j<pv;j++){vi[j]=base+(long long)DX8_IX(a[j]);if(vi[j]<0||(unsigned long long)vi[j]>=nvert)ok=0;}
    if(ok){for(j=0;j<pv;j++){dx8_gv(d,sv,(uint32_t)vi[j],dx8_gb+(size_t)r->gn*d->ostride);r->gn++;}cntp++;}
    k++;}
   if(cntp){o=dx8_room(r,t==1?8:6,&rc,err_src);if(!o)return rc;
    if(t==1){dx8_hdr(o,1,1);dx8_w16(o+4,cntp);dx8_w16(o+6,(uint32_t)v);}else{dx8_hdr(o,pv==2?15:18,cntp);dx8_w16(o+4,(uint32_t)v);}
    r->draws++;vx_d3d_dx8_stat[DX8S_GVERTS]+=cntp*pv;}}
  vx_d3d_dx8_stat[DX8S_DEINDEX]++;}
 #undef DX8_IX
 return 0;
}
/* a non-indexed draw: primitive type t, `n` primitives from the vertex at byte boff */
static uint32_t dx8_draw(dx8_run_t *r,uint32_t t,uint32_t boff,uint32_t n,uint32_t *err_src) {
 const uint32_t st=r->x->s0stride,avail=dx8_s0_avail(r);uint32_t rc=0;
 if(t<1||t>6||!st){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 if(t==6&&n>65535UL){ /* a fan of more than 65535 triangles: gathered in pieces, its centre with each */
  if(!dx8_plan1(r,boff%st)){*err_src=r->src_cmd;return VX_D3D_INVALID;}
  return dx8_gdraw(r,&r->p1,6,(long long)(boff/st),n,err_src);}
 rc=dx8_kind(r,1,0,err_src);if(rc)return rc;
 while(n){
  uint32_t m=n>65535UL?65534UL:n,vs;long v;uint8_t *o; /* 65534: even, so a strip's next piece keeps its winding */
  if(boff>=avail||avail-boff<st){vx_d3d_dx8_stat[DX8S_PAST]++;return 0;} /* no whole vertex of it inside the stream: drawn as the core draws missing vertices, not at all (a segment from there could not be bound) */
  vs=dx8_verts(t,m);v=dx8_place(r,boff,vs,0,&rc,err_src);if(v<0)return rc;
  o=dx8_room(r,t==1?8:6,&rc,err_src);if(!o)return rc;
  if(t==1){dx8_hdr(o,1,1);dx8_w16(o+4,m);dx8_w16(o+6,(uint32_t)v);}else{dx8_hdr(o,dx8_op_nonix[t],m);dx8_w16(o+4,(uint32_t)v);}
  r->draws++;n-=m;
  boff+=(t==1?m:t==2?2*m:t==3?m:t==4?3*m:m)*st; /* the next piece: points, lines, a strip from its m-th vertex, a list after its 3m */
 }
 return 0;
}
/* an indexed draw: indices from the index buffer at byte ioff (stride 2 or 4), vertex = base + index, base at byte boff of stream 0
 * (signed: DirectX 8's BaseVertexIndex may be negative); mn, nv: the range the indices claim (rebased to it when the base cannot carry them) */
static uint32_t dx8_drawix(dx8_run_t *r,uint32_t t,long long boff,uint32_t mn,uint32_t nv,uint32_t ioff,uint32_t n,uint32_t *err_src) {
 const uint32_t st=r->x->s0stride,is=r->x->ibstride,s0a=dx8_s0_avail(r);uint32_t rc=0,ni,avail,k;DDRAWI_DDRAWSURFACE_LCL *ib;const uint8_t *ix;
 if(t<1||t>6||!st||(is!=2&&is!=4)){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 ib=dx8_buf_get(r->c,r->x->ibh);if(!ib){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 ni=dx8_verts(t,n);avail=surface_bytes(ib);
 if(!n)return 0;
 if(ioff>avail||ni>(avail-ioff)/is){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 ix=(const uint8_t*)ib->lpGbl->fpVidMem+ioff;
 /* what one segment's 16-bit starts and indices cannot reach (a fan of more than 65535 triangles; a base before the stream or 32-bit
  * indices past 65535 whose range is wider than 65536 vertices or starts before the stream): gathered */
 if((t==6&&n>65535UL)||((boff<0||(is==4&&(mn>65536UL||nv>65536UL-mn))||(t==1&&mn))&&(boff+(long long)mn*st<0||boff+(long long)mn*st>0xFFFFFFFFLL||nv>65536UL))){
  const long long rem=((boff%(long long)st)+(long long)st)%(long long)st;
  if(!dx8_plan1(r,(uint32_t)rem)){*err_src=r->src_cmd;return VX_D3D_INVALID;}
  return dx8_gdrawix(r,&r->p1,t,(boff-rem)/(long long)st,mn,nv,ioff,n,err_src);}
 if(is==4)vx_d3d_dx8_stat[DX8S_IX32]++;
 rc=dx8_kind(r,1,0,err_src);if(rc)return rc;
 {uint32_t done=0;
  while(done<n){
   uint32_t m=n-done,per,hdr,first,cnt,shift=0;long v;uint8_t *o;long long b=boff;
   /* the piece's indices: a list's own, a strip's from its first triangle (two shared with the piece before), a fan's centre then its run */
   per=t==1?4:t==2?4:t==4?6:2;hdr=t==1?4:6;
   if(m>65534UL)m=65534UL;
   {uint32_t fit=(DX8_SB-(r->used<DX8_SB?r->used:DX8_SB));if(fit<hdr+per*4+8)fit=DX8_SB; /* the next flush empties it */
    if(fit>hdr+8&&(fit-hdr-8)/per<m){uint32_t mm=(fit-hdr-8)/per;if(t==5&&(mm&1))mm--;if(mm>=2)m=mm;}}
   first=t==2?done*2:t==3?done:t==4?done*3:t==5?done:t==6?done+1:done; /* the piece's first index */
   cnt=t==6?m+1:dx8_verts(t,m); /* indices this piece writes (a fan's centre apart) */
   /* a negative base (DirectX 8 allows it), or 32-bit indices past 65535: the indices made relative to MinIndex, the base moved to it */
   if(b<0||(is==4&&(mn>65536UL||nv>65536UL-mn))||(t==1&&mn)){shift=mn;b+=(long long)mn*st;vx_d3d_dx8_stat[DX8S_REBASE]++;}
   if(b>=(long long)s0a||(long long)s0a-b<(long long)st){vx_d3d_dx8_stat[DX8S_PAST]++;done+=m;continue;} /* every vertex it can read lies past the stream's last whole one: none drawn, as missing vertices are */
   if(nv>0xFFFFFFFFUL-mn){*err_src=r->src_cmd;return VX_D3D_INVALID;} /* E776 (outside audit, HAL-9): the range's end in 32 bits (it wrapped to a small count and passed the tests above) */
   v=dx8_place(r,(uint32_t)b,mn+nv-shift,t==1,&rc,err_src);if(v<0)return rc;
   if(t==1){o=dx8_room(r,4+m*4,&rc,err_src);if(!o)return rc;dx8_hdr(o,1,m);
    for(k=0;k<m;k++){uint32_t i=(is==2?u16(ix+(first+k)*2):u32(ix+(first+k)*4))-shift,a=(uint32_t)v+i;if(i>65535UL||a>65535UL)a=0xFFFFUL;dx8_w16(o+4+k*4,1);dx8_w16(o+6+k*4,a);}}
   else{uint8_t *q;o=dx8_room(r,6+(t==6?(cnt+1):cnt)*2,&rc,err_src);if(!o)return rc;dx8_hdr(o,dx8_op_ix[t],m);dx8_w16(o+4,(uint32_t)v);q=o+6;
    if(t==6){uint32_t i=(is==2?u16(ix):u32(ix))-shift;dx8_w16(q,i>65535UL?0xFFFFUL:i);q+=2;}
    if(is==2&&!shift){memcpy(q,ix+first*2,cnt*2);}
    else for(k=0;k<cnt;k++){uint32_t i=(is==2?u16(ix+(first+k)*2):u32(ix+(first+k)*4))-shift;dx8_w16(q+k*2,i>65535UL?0xFFFFUL:i);}} /* 0xFFFF: past any 16-bit range the segment binds: the core leaves it out */
   r->draws++;done+=m;}}
 return 0;
}
/* CLIPPEDTRIANGLEFAN: the runtime's clipped polygon, pretransformed vertices in stream 0 from byte offset off, as TRIANGLEFAN_IMM with
 * its edge flags and the vertices inline, starting on the DWORD boundary the core reads them from (its imm: the address after the flags,
 * rounded up to 4) */
static uint32_t dx8_clipfan(dx8_run_t *r,uint32_t off,uint32_t ef,uint32_t n,uint32_t *err_src) {
 const uint32_t st=r->x->s0stride,nv=n+2;uint32_t rc=0,pad,avail;const uint8_t *vb;uint8_t *o;
 if(!n)return 0;
 if(!st||n>65535UL||nv>(DX8_SB-16)/st){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 rc=dx8_kind(r,1,0,err_src);if(rc)return rc;
 if(r->x->s0um){if(!(r->p->dwFlags&1)||!r->p->lpVertices){*err_src=r->src_cmd;return VX_D3D_INVALID;}vb=(const uint8_t*)r->p->lpVertices+r->p->dwVertexOffset;avail=dx8_s0_avail(r);} /* E776 (outside audit, HAL-12): the caller's own count of its vertices (it was unbounded: a fan past the end of user memory was copied) */
 else{DDRAWI_DDRAWSURFACE_LCL *s=dx8_buf_get(r->c,r->x->s0h);if(!s){*err_src=r->src_cmd;return VX_D3D_INVALID;}vb=(const uint8_t*)s->lpGbl->fpVidMem;avail=surface_bytes(s);}
 if(off>avail||nv>(avail-off)/st){*err_src=r->src_cmd;return VX_D3D_INVALID;}
 if(DX8_SB-r->used<8+3+nv*st){uint32_t ko=r->seg_off,ks=r->seg_set,kn=r->seg_need;rc=dx8_flush(r,err_src);if(rc)return rc;r->seg_off=ko;r->seg_set=ks;r->seg_need=kn;}
 o=dx8_room(r,8,&rc,err_src);if(!o)return rc;
 dx8_hdr(o,23,n);dx8_w32(o+4,ef);
 pad=(uint32_t)((4-((uintptr_t)(dx8_sb+r->used)&3))&3);
 memset(dx8_sb+r->used,0,pad);memcpy(dx8_sb+r->used+pad,vb+off,nv*st);r->used+=pad+nv*st;
 r->draws++;
 return 0;
}
/* DirectX 8's texture filter values (D3DTEXTUREFILTERTYPE: 0 NONE, 1 POINT, 2 LINEAR, 3 ANISOTROPIC, 4 FLATCUBIC, 5 GAUSSIANCUBIC, the same
 * for all three states) as the DirectX 7 values the core reads: MAGFILTER (16) D3DTFG_ 1 POINT, 2 LINEAR, 3 FLATCUBIC, 4 GAUSSIANCUBIC,
 * 5 ANISOTROPIC; MINFILTER (17) D3DTFN_ 1 POINT, 2 LINEAR, 3 ANISOTROPIC; MIPFILTER (18) D3DTFP_ 1 NONE, 2 POINT, 3 LINEAR. A DirectX 8
 * driver is given the DirectX 8 values as they are (the runtime translates them only for a DirectX 7 driver). */
static uint32_t dx8_filter(uint32_t state,uint32_t v) {
 if(state==16)return v<=1?1:v==2?2:v==3?5:v==4?3:4;
 if(state==17)return v<=1?1:v==3?3:2;
 if(state==18)return v==0?1:v==1?2:3;
 return v;
}
/* the command with its source operand copied: a SETTRANSFORM with DirectX 8's world numbers made DirectX 7's, a TEXTURESTAGESTATE's filter
 * values made DirectX 7's */
static uint32_t dx8_copy(dx8_run_t *r,uint32_t op,uint32_t n,const uint8_t *p,uint32_t len,uint32_t *err_src) {
 uint32_t rc=0,j;uint8_t *o=dx8_room(r,4+len,&rc,err_src);if(!o)return rc;
 dx8_hdr(o,op,n);memcpy(o+4,p,len);
 if(op==36)for(j=0;j<n;j++){uint32_t ty=u32(o+4+j*68);if(ty>=256&&ty<=259)dx8_w32(o+4+j*68,ty==256?1:ty+(4-257));}
 if(op==25)for(j=0;j<n;j++){uint32_t ty=u16(o+4+j*8+2),v=u32(o+4+j*8+4),w=dx8_filter(ty,v);if(w!=v){dx8_w32(o+4+j*8+4,w);vx_d3d_dx8_stat[DX8S_TSSX]++;}}
 return 0;
}
int vx_d3d_tnl_matrix(const vx_d3d_context *,uint32_t,float *);
static void dx8_mul(float *o,const float *a,const float *b) {int i,j,k;for(i=0;i<4;i++)for(j=0;j<4;j++){float s=0;for(k=0;k<4;k++)s+=a[i*4+k]*b[k*4+j];o[i*4+j]=s;}}
/* The DirectX 8 call. *error: the source offset of the failing command. */
static uint32_t dx8_dp2(context_slot *c,const vx_ddi_dp2 *p,const vx_d3d_input *in0,vx_d3d_services *sv,dp2_source *src,uint32_t *error) {
 dx8_run_t r;uint32_t pos=0,rc=0,esrc=0;const uint8_t *cmd=in0->commands;const uint32_t bytes=in0->command_bytes;
 memset(&r,0,sizeof(r));r.c=c;r.x=dx8_of(c);r.p=p;r.src=src;r.sv=sv;r.cmd=cmd;r.um_vs=src->stride;
 vx_d3d_dx8_stat[DX8S_CALLS]++;
 /* the filter states the context was given before it was known to be a DirectX 8 one (calls with state commands alone) were kept as
  * they came: made DirectX 7's once */
 if(!(r.x->dx8&0x200)){r.x->dx8|=0x200;DX8_FIXUP(c);vx_d3d_dx8_stat[DX8S_FIXUP]++;}
 *error=0;
 while(pos<bytes){
  uint32_t op,n,len=0,j;const uint8_t *q;
  if(bytes-pos<4){rc=VX_D3D_INVALID;esrc=pos;goto out_flush;}
  op=cmd[pos];n=u16(cmd+pos+2);q=cmd+pos+4;r.src_cmd=pos;
  #define NEED(L) do{len=(L);if(len>bytes-pos-4){rc=VX_D3D_INVALID;esrc=pos;goto out_flush;}}while(0)
  {dx8_ctx rec_keep;const int rec_st=(r.x->dx8&0x100)&&(op==47||(op>=49&&op<=51)||op==56);if(rec_st)rec_keep=*r.x; /* E776 (outside audit of E768, HAL-7): a DirectX 8 state token (shader, stream source, indices) inside a state block being recorded is not applied to the live state (its commands are still checked and counted) */
  switch(op){
   case 47:NEED(n*4); /* SETVERTEXSHADER: an FVF code (even), or a declaration's handle (odd) */
    if(n){uint32_t h=u32(q+(n-1)*4);
     if(h!=r.x->vs||!r.x->mode){rc=dx8_flush(&r,&esrc);if(rc)goto out;dx8_vs_use(r.x,c->handle,h);}}
    break;
   case 49:NEED(n*12);
    for(j=0;j<n;j++){uint32_t s=u32(q+j*12),h=u32(q+j*12+4),st=u32(q+j*12+8);
     if(s>15){vx_d3d_dx8_stat[DX8S_STREAMN]++;continue;}
     /* a change ends the segment (as every change of this state does): the commands of a segment all ran with one state, so the command
      * a failure names can be run again as it stands */
     if(s){if(h!=r.x->sh[s]||st!=r.x->sst[s]){rc=dx8_flush(&r,&esrc);if(rc)goto out;r.x->sh[s]=h;r.x->sst[s]=st;}continue;}
     if(r.x->s0um||h!=r.x->s0h||st!=r.x->s0stride){rc=dx8_flush(&r,&esrc);if(rc)goto out;r.x->s0um=0;r.x->s0h=h;r.x->s0stride=st;}}
    break;
   case 50:NEED(n*8);
    for(j=0;j<n;j++){uint32_t s=u32(q+j*8),st=u32(q+j*8+4);
     if(s){vx_d3d_dx8_stat[DX8S_STREAMN]++;continue;}
     if(!r.x->s0um||st!=r.x->s0stride){rc=dx8_flush(&r,&esrc);if(rc)goto out;r.x->s0um=1;r.x->s0h=0;r.x->s0stride=st;}}
    break;
   case 51:NEED(n*8);
    if(n){uint32_t h=u32(q+(n-1)*8),st=u32(q+(n-1)*8+4);if(h!=r.x->ibh||st!=r.x->ibstride){rc=dx8_flush(&r,&esrc);if(rc)goto out;r.x->ibh=h;r.x->ibstride=st;}}
    break;
   case 52:NEED(n*12);
    for(j=0;j<n;j++){if(!r.x->mode){vx_d3d_dx8_stat[DX8S_VSDROP]++;continue;}vx_d3d_dx8_stat[DX8S_DRAW]++;
     if(r.x->mode==2)rc=dx8_gdraw(&r,&r.x->plan,u32(q+j*12),(long long)u32(q+j*12+4),u32(q+j*12+8),&esrc);
     else if(r.x->s0stride&&u32(q+j*12+4)>0xFFFFFFFFUL/r.x->s0stride){vx_d3d_dx8_stat[DX8S_PAST]++;continue;}
     else rc=dx8_draw(&r,u32(q+j*12),u32(q+j*12+4)*r.x->s0stride,u32(q+j*12+8),&esrc);
     if(rc)goto out_flush;}
    break;
   case 59:NEED(n*12);
    for(j=0;j<n;j++){if(r.x->mode!=1){vx_d3d_dx8_stat[DX8S_VSDROP]++;continue;}vx_d3d_dx8_stat[DX8S_DRAW2]++;
     rc=dx8_draw(&r,u32(q+j*12),u32(q+j*12+4),u32(q+j*12+8),&esrc);if(rc)goto out_flush;}
    break;
   case 53:NEED(n*24);
    for(j=0;j<n;j++){const uint8_t *e=q+j*24;if(!r.x->mode){vx_d3d_dx8_stat[DX8S_VSDROP]++;continue;}vx_d3d_dx8_stat[DX8S_DRAWIX]++;
     if(r.x->ibstride&&u32(e+16)>0xFFFFFFFFUL/r.x->ibstride){rc=VX_D3D_INVALID;esrc=pos;goto out_flush;}
     if(r.x->mode==2)rc=dx8_gdrawix(&r,&r.x->plan,u32(e),(long long)(int32_t)u32(e+4),u32(e+8),u32(e+12),u32(e+16)*r.x->ibstride,u32(e+20),&esrc);
     else rc=dx8_drawix(&r,u32(e),(long long)(int32_t)u32(e+4)*r.x->s0stride,u32(e+8),u32(e+12),u32(e+16)*r.x->ibstride,u32(e+20),&esrc);
     if(rc)goto out_flush;}
    break;
   case 60:NEED(n*24);
    for(j=0;j<n;j++){const uint8_t *e=q+j*24;if(r.x->mode!=1){vx_d3d_dx8_stat[DX8S_VSDROP]++;continue;}vx_d3d_dx8_stat[DX8S_DRAWIX2]++;
     rc=dx8_drawix(&r,u32(e),(long long)(int32_t)u32(e+4),u32(e+8),u32(e+12),u32(e+16),u32(e+20),&esrc);if(rc)goto out_flush;}
    break;
   case 58:NEED(n*12);
    for(j=0;j<n;j++){if(r.x->mode!=1){vx_d3d_dx8_stat[DX8S_VSDROP]++;continue;}vx_d3d_dx8_stat[DX8S_CLIPFAN]++;
     rc=dx8_clipfan(&r,u32(q+j*12),u32(q+j*12+4),u32(q+j*12+8),&esrc);if(rc)goto out_flush;}
    break;
   case 45: /* CREATEVERTEXSHADER: handle, declaration size, code size, then both */
    for(j=0;j<n;j++){uint32_t h,ds,cs;if(bytes-pos-4<len+12){rc=VX_D3D_INVALID;esrc=pos;goto out_flush;}
     h=u32(q+len);ds=u32(q+len+4);cs=u32(q+len+8);
     if(ds>bytes||cs>bytes||ds+cs>bytes-pos-4-len-12){rc=VX_D3D_INVALID;esrc=pos;goto out_flush;}
     {dx8_vs v;if(cs||!dx8_decl_parse(q+len+12,ds,&v))v.fvf=0; /* a programmable shader (VertexShaderVersion 0: the runtime runs those itself) or a declaration no FVF vertex describes: nothing drawn with it */
      vx_d3d_dx8_stat[!v.fvf?DX8S_VSDECL:v.direct?DX8S_VSFVF:DX8S_VSGATHER]++;if(!dx8_vs_set(c->handle,h,&v,0)){rc=VX_D3D_INVALID;esrc=pos;vx_d3d_dx8_stat[DX8S_FAIL]++;goto out_flush;}
      if(h==r.x->vs){rc=dx8_flush(&r,&esrc);if(rc)goto out;dx8_vs_use(r.x,c->handle,h);}}
     len+=12+ds+cs;}
    break;
   case 46:NEED(n*4);rc=dx8_flush(&r,&esrc);if(rc)goto out;
    for(j=0;j<n;j++){(void)dx8_vs_set(c->handle,u32(q+j*4),0,1);if(u32(q+j*4)==r.x->vs)dx8_vs_use(r.x,c->handle,r.x->vs);}
    break;
   case 48:case 57: /* shader constants: register, count, then count float4s */
    for(j=0;j<n;j++){uint32_t k2;if(bytes-pos-4<len+8){rc=VX_D3D_INVALID;esrc=pos;goto out_flush;}k2=u32(q+len+4);
     if(k2>(bytes-pos-4-len-8)/16){rc=VX_D3D_INVALID;esrc=pos;goto out_flush;}len+=8+k2*16;}
    break;
   case 54: /* CREATEPIXELSHADER: handle, code size, code (PixelShaderVersion 0: never drawn) */
    for(j=0;j<n;j++){uint32_t cs;if(bytes-pos-4<len+8){rc=VX_D3D_INVALID;esrc=pos;goto out_flush;}cs=u32(q+len+4);
     if(cs>bytes-pos-4-len-8){rc=VX_D3D_INVALID;esrc=pos;goto out_flush;}len+=8+cs;vx_d3d_dx8_stat[DX8S_PSDROP]++;}
    break;
   case 55:NEED(n*4);break;
   case 56:NEED(n*4);for(j=0;j<n;j++){r.x->ps=u32(q+j*4);if(r.x->ps)vx_d3d_dx8_stat[DX8S_PSDROP]++;}break;
   case 66:NEED(n*20);break; /* ADDDIRTYRECT */
   case 67:NEED(n*28);break; /* ADDDIRTYBOX */
   case 64:NEED(n*24); /* BUFFERBLT: dest handle, source handle, dest offset, source range (offset, size), flags */
    rc=dx8_flush(&r,&esrc);if(rc)goto out;
    for(j=0;j<n;j++){const uint8_t *e=q+j*24;DDRAWI_DDRAWSURFACE_LCL *d=dx8_buf_get(c,u32(e)),*s=dx8_buf_get(c,u32(e+4));
     uint32_t doff=u32(e+8),so=u32(e+12),sz=u32(e+16),da,sa;
     if(!d||!s){rc=VX_D3D_INVALID;esrc=pos;goto out;}
     da=surface_bytes(d);sa=surface_bytes(s);
     if(so>sa||sz>sa-so||doff>da||sz>da-doff){rc=VX_D3D_INVALID;esrc=pos;goto out;}
     memmove((uint8_t*)d->lpGbl->fpVidMem+doff,(const uint8_t*)s->lpGbl->fpVidMem+so,sz);
     vx_d3d_dx8_stat[DX8S_BUFBLT]++;}
    break;
   case 65:NEED(n*68); /* MULTIPLYTRANSFORM: the state times the matrix given (D3D: pMatrix x State), sent as SETTRANSFORM */
    rc=dx8_flush(&r,&esrc);if(rc)goto out;
    for(j=0;j<n;j++){uint32_t ty=u32(q+j*68);float cur[16],m[16],o[16];uint8_t *w;
     if(ty>=256&&ty<=259)ty=ty==256?1:ty+(4-257);
     memcpy(m,q+j*68+4,64);
     if(!vx_d3d_tnl_matrix(&c->core,ty,cur)){vx_d3d_dx8_stat[DX8S_UNKNOWN]++;continue;}
     dx8_mul(o,m,cur);w=dx8_room(&r,4+68,&rc,&esrc);if(!w)goto out;dx8_hdr(w,36,1);dx8_w32(w+4,ty);memcpy(w+8,o,64);vx_d3d_dx8_stat[DX8S_MULT]++;
     if(j+1<n){rc=dx8_flush(&r,&esrc);if(rc)goto out;}} /* E771 (outside audit of E768, item 1): each product set before the next is made (two multiplies of one transform in a token both read the matrix from before the token: the first was lost) */
    break;
   case 39: /* STATESET: copied; a state block recorded with DirectX 8 commands in it is not this layer's (counted: the non-pure runtime keeps its own) */
    NEED(n*12);for(j=0;j<n;j++){uint32_t o2=u32(q+j*12);if(o2==0)r.x->dx8|=0x100;else if(o2==1)r.x->dx8&=~0x100UL;}
    rc=dx8_copy(&r,op,n,q,len,&esrc);if(rc)goto out;break;
   default:
    if(op>=45&&op<=67){vx_d3d_dx8_stat[op==61||op==62?DX8S_PATCH:DX8S_UNKNOWN]++;rc=VX_D3D_UNSUPPORTED;esrc=pos;goto out_flush;}
    if(dx8_is_draw7(op)){rc=VX_D3D_INVALID;esrc=pos;vx_d3d_dx8_stat[DX8S_UNKNOWN]++;goto out_flush;} /* a DirectX 7 draw in a DirectX 8 context: no source to read */
    len=dx8_dx7_len(op,n,q,bytes-pos-4);
    if(len==0xFFFFFFFFUL){ /* a command the core gives the runtime's parser: the segment sent, then this command alone through the core */
     uint32_t e2=0;vx_d3d_input one;rc=dx8_flush(&r,&esrc);if(rc)goto out;
     one=*in0;one.commands=cmd+pos;one.command_bytes=bytes-pos;src->args=p;src->stride=r.um_vs; /* E775 (outside audit, HAL-6): the stride as the call came (dwVertexSize shares its place with ddrval, already written) */
     vx_d3d_dx8_stat[DX8S_UNKNOWN]++;
     rc=vx_d3d_dp2(&c->core,&one,sv,&e2);
     /* the core stops at the next command it cannot read, a DirectX 8 one: what it drew before that stands, and this layer goes on there */
     if(rc==VX_D3D_UNPARSED&&e2>0&&e2<bytes-pos&&cmd[pos+e2]>=45&&cmd[pos+e2]<=67){rc=0;pos+=e2;continue;}
     if(rc){esrc=pos+e2;goto out;}
     pos=bytes;continue;}
    if(len>=0xFFFFFFFEUL||len>bytes-pos-4){rc=VX_D3D_INVALID;esrc=pos;goto out_flush;}
    if(r.x->dx8&0x100)vx_d3d_dx8_stat[DX8S_REC]+=(op!=39);
    rc=dx8_copy(&r,op,n,q,len,&esrc);if(rc)goto out;
    break;
  }
  if(rec_st){*r.x=rec_keep;vx_d3d_dx8_stat[DX8S_REC]++;}}
  #undef NEED
  pos+=4+len;
 }
 rc=dx8_flush(&r,&esrc);
 out:
 if(rc)*error=esrc;else *error=0;
 src->args=p;
 return rc;
 out_flush: /* a command this layer refuses: the commands before it drawn first; their failure, if they fail, is the one reported */
 {uint32_t e0=esrc,r2=dx8_flush(&r,&esrc);if(r2)rc=r2;else esrc=e0;}
 goto out;
}
/* ---- GetDriverInfo2 ---- */
#pragma pack(push,4)
typedef struct {DWORD dwReserved,dwMagic,dwType,dwExpectedSize;} vx_gdi2;
typedef struct {
 DWORD DeviceType,AdapterOrdinal,Caps,Caps2,Caps3,PresentationIntervals,CursorCaps,DevCaps,PrimitiveMiscCaps,RasterCaps,ZCmpCaps,
  SrcBlendCaps,DestBlendCaps,AlphaCmpCaps,ShadeCaps,TextureCaps,TextureFilterCaps,CubeTextureFilterCaps,VolumeTextureFilterCaps,
  TextureAddressCaps,VolumeTextureAddressCaps,LineCaps,MaxTextureWidth,MaxTextureHeight,MaxVolumeExtent,MaxTextureRepeat,
  MaxTextureAspectRatio,MaxAnisotropy;
 float MaxVertexW,GuardBandLeft,GuardBandTop,GuardBandRight,GuardBandBottom,ExtentsAdjust;
 DWORD StencilCaps,FVFCaps,TextureOpCaps,MaxTextureBlendStages,MaxSimultaneousTextures,VertexProcessingCaps,MaxActiveLights,
  MaxUserClipPlanes,MaxVertexBlendMatrices,MaxVertexBlendMatrixIndex;
 float MaxPointSize;
 DWORD MaxPrimitiveCount,MaxVertexIndex,MaxStreams,MaxStreamStride,VertexShaderVersion,MaxVertexShaderConst,PixelShaderVersion;
 float MaxPixelShaderValue;
} vx_caps8;
#pragma pack(pop)
typedef char vx_caps8_size[sizeof(vx_caps8)==212?1:-1]; /* sizeof(D3DCAPS8): D3DCAPS9 up to PixelShader1xMaxValue */
typedef char vx_gdi2_size[sizeof(vx_gdi2)==16?1:-1];
static DWORD get_driver_info(LPDDHAL_GETDRIVERINFODATA p);
/* D3DCAPS8 from this HAL's DirectX 7 description (the device description and the extended caps, as D3D8.DLL itself made them from it under
 * the DirectX 7 DDI), plus what the DirectX 8 DDI adds: one stream, 16-bit indices, no shaders */
static void dx8_caps(vx_caps8 *k) {
 vx_ddi_extended e;DDHAL_GETDRIVERINFODATA q;const vx_ddi_desc *h=&global_caps.hwCaps;const D3DPRIMCAPS *t=&h->dpcTriCaps;
 static const uint8_t b[8]={0x89,0xAB,0,0xA0,0xC9,5,0x41,0x29};
 memset(&e,0,sizeof(e));memset(&q,0,sizeof(q));q.dwSize=sizeof(q);q.guidInfo.Data1=0x7DE41F80UL;q.guidInfo.Data2=0x9D93;q.guidInfo.Data3=0x11D0;memcpy(q.guidInfo.Data4,b,8);
 q.dwExpectedSize=sizeof(e);q.lpvData=&e;get_driver_info(&q);
 memset(k,0,sizeof(*k));
 k->DeviceType=1;k->Caps2=0x00080000UL|0x00020000UL; /* D3DDEVTYPE_HAL; D3DCAPS2_CANRENDERWINDOWED, FULLSCREENGAMMA (DirectDraw's PRIMARYGAMMA) */
 k->PresentationIntervals=0x8000000FUL; /* IMMEDIATE, ONE .. FOUR: what DirectDraw's FLIPNOVSYNC and FLIPINTERVAL give the DirectX 7 DDI's device */
 k->DevCaps=h->dwDevCaps;
 k->PrimitiveMiscCaps=t->dwMiscCaps|0x80UL|0x800UL; /* + D3DPMISCCAPS_COLORWRITEENABLE, BLENDOP: DirectX 8's bits for what the core draws (v10.295, v10.304) */
 k->RasterCaps=t->dwRasterCaps;k->ZCmpCaps=t->dwZCmpCaps;k->SrcBlendCaps=t->dwSrcBlendCaps;
 k->DestBlendCaps=t->dwDestBlendCaps;k->AlphaCmpCaps=t->dwAlphaCmpCaps;k->ShadeCaps=t->dwShadeCaps;k->TextureCaps=t->dwTextureCaps;
 k->TextureFilterCaps=t->dwTextureFilterCaps&0x07070700UL; /* the DirectX 8 bits (MIN, MIP, MAG); the DirectX 5 ones (NEAREST .. LINEARMIPLINEAR) are not DirectX 8's */
 /* D3DPTEXTURECAPS_MIPMAP (0x4000, DirectX 8's own bit) is already in the DirectX 7 caps (v10.456); a cube map's levels are not sampled
  * (no MIPCUBEMAP), so the cube filter caps carry no mip filters */
 k->CubeTextureFilterCaps=(t->dwTextureCaps&0x800)?k->TextureFilterCaps&~0x00030000UL:0;
 k->TextureAddressCaps=t->dwTextureAddressCaps;
 k->LineCaps=0x1F; /* TEXTURE ZTEST BLEND ALPHACMP FOG: lines draw with every triangle state (v10.201) */
 k->MaxTextureWidth=e.dwMaxTextureWidth;k->MaxTextureHeight=e.dwMaxTextureHeight;k->MaxTextureRepeat=e.dwMaxTextureRepeat;
 k->MaxTextureAspectRatio=e.dwMaxTextureAspectRatio;k->MaxAnisotropy=e.dwMaxAnisotropy;k->MaxVertexW=e.dvMaxVertexW;
 k->GuardBandLeft=e.dvGuardBandLeft;k->GuardBandTop=e.dvGuardBandTop;k->GuardBandRight=e.dvGuardBandRight;k->GuardBandBottom=e.dvGuardBandBottom;
 k->ExtentsAdjust=e.dvExtentsAdjust;k->StencilCaps=e.dwStencilCaps;k->FVFCaps=e.dwFVFCaps;k->TextureOpCaps=e.dwTextureOpCaps;
 k->MaxTextureBlendStages=e.wMaxTextureBlendStages;k->MaxSimultaneousTextures=e.wMaxSimultaneousTextures;
 k->VertexProcessingCaps=e.dwVertexProcessingCaps;k->MaxActiveLights=e.dwMaxActiveLights;k->MaxUserClipPlanes=e.wMaxUserClipPlanes;
 k->MaxVertexBlendMatrices=e.wMaxVertexBlendMatrices;k->MaxPointSize=1.0f;
 k->MaxPrimitiveCount=0xFFFFFUL;k->MaxVertexIndex=0x00FFFFFFUL; /* drawn in pieces; 32-bit index buffers: any range (wide ones gathered) */
 k->MaxStreams=16;k->MaxStreamStride=256; /* fixed-function declarations over any streams (gathered) */
}
#define DX8_NFMT 16
/* the DirectX 8 format list: the DirectX 7 texture list in DirectX 8's codes, the two display formats this HAL renders to, the depth formats */
static void dx8_format(uint32_t i,DDPIXELFORMAT *f) {
 /* D3DFORMAT_OP: TEXTURE 1, CUBETEXTURE 4, OFFSCREEN_RENDERTARGET 8, SAME_FORMAT_RENDERTARGET 0x10, ZSTENCIL 0x40, ..ARBITRARY_COLOR_DEPTH
  * 0x80, SAME_FORMAT_UP_TO_ALPHA_RENDERTARGET 0x100, DISPLAYMODE 0x400, 3DACCELERATION 0x800, BUMPMAP 0x10000 */
 static const DWORD code[DX8_NFMT]={21,22,23,25,26,24,50,0x31545844UL,0x33545844UL,0x35545844UL,60,61,62,80,75,71};
 const DWORD cube=(vx_d3d_feat&VX_F_CUBE)?4:0,rt=vx_d3d_rttex?0x10:0;DWORD ops=0;
 memset(f,0,sizeof(*f));f->dwSize=sizeof(*f);f->dwFlags=DDPF_D3DFORMAT;if(i>=DX8_NFMT)return;
 f->dwFourCC=code[i];
 if(i<=9)ops=1|cube;
 if(i==0)ops|=rt|0x100;                 /* A8R8G8B8: a render target with the X8R8G8B8 display (v10.459) */
 if(i==1||i==2)ops|=0x400|0x800|0x10;   /* X8R8G8B8, R5G6B5: display modes with 3D, and render targets in the display's own format (the back buffer; Direct3D 8 refuses the device without it) */
 if(i>=10&&i<=12)ops=1|0x10000UL;       /* the bump formats (VX_F_BUMP with VX_F_STAGES: the count below leaves them out otherwise) */
 if(i>=13&&i<=15)ops=0x40|0x80;         /* D16, D24S8, D32: with either colour depth (vx_d3d_targets_valid ties none) */
 f->dwRBitMask=ops;                    /* dwOperations. E782 (board, E780: with the interface on Direct3D 8 answered GetDeviceCaps with
  * D3DERR_NOTAVAILABLE and made no device): the operations were written into dwRGBBitCount, which in this structure's DirectX 8 form is
  * dwPrivateFormatBitCount; dwOperations shares dwRBitMask's place (DirectDraw's own code reads it at offset 16: test [f + 10h], 1000h,
  * D3DFORMAT_OP_PIXELSIZE). Every format came out with no operation: no display mode format, so no HAL device */
}
static int dx8_fmt_index(uint32_t k) {uint32_t bump=(vx_d3d_feat&VX_F_BUMP)&&(vx_d3d_feat&VX_F_STAGES);if(!bump&&k>=10)k+=3;return k<16?(int)k:-1;}
static uint32_t dx8_nfmt_now(void) {return ((vx_d3d_feat&VX_F_BUMP)&&(vx_d3d_feat&VX_F_STAGES))?16:13;}
/* GUID_GetDriverInfo2 (GUID_DDStereoMode's value: dwMagic 0xFFFFFFFF tells them apart). Returns 1 when answered. */
static int dx8_gdi2(LPDDHAL_GETDRIVERINFODATA p) {
 vx_gdi2 *g=(vx_gdi2*)p->lpvData;
 if(!vx_d3d_dx8_on||!g||p->dwExpectedSize<sizeof(*g)||IsBadReadPtr(g,sizeof(*g))||g->dwMagic!=0xFFFFFFFFUL)return 0;
 vx_d3d_dx8_stat[DX8S_GDI2]++;
 p->dwActualSize=0;p->ddRVal=DDERR_CURRENTLYNOTAVAIL;
 if(g->dwType==1){vx_caps8 k;uint32_t n=p->dwExpectedSize<sizeof(k)?p->dwExpectedSize:sizeof(k); /* GETD3DCAPS8: the caps over the whole buffer, header included */
  dx8_caps(&k);if(IsBadWritePtr(p->lpvData,n))return 1;memcpy(p->lpvData,&k,n);p->dwActualSize=n;p->ddRVal=DD_OK;vx_d3d_dx8_stat[DX8S_CAPS8]++;return 1;}
 if(g->dwType==2&&p->dwExpectedSize>=24&&!IsBadWritePtr(p->lpvData,24)){ /* E775 (outside audit, HAL-10): the words past the 16-byte head tested before they are touched (types 2-4) */((DWORD*)p->lpvData)[4]=dx8_nfmt_now();p->dwActualSize=24;p->ddRVal=DD_OK;return 1;} /* GETFORMATCOUNT */
 if(g->dwType==3&&p->dwExpectedSize>=20+sizeof(DDPIXELFORMAT)&&!IsBadWritePtr(p->lpvData,20+sizeof(DDPIXELFORMAT))){uint32_t i=((DWORD*)p->lpvData)[4];int k=dx8_fmt_index(i); /* GETFORMAT */
  if(i>=dx8_nfmt_now()||k<0){p->ddRVal=DDERR_INVALIDPARAMS;return 1;}
  dx8_format((uint32_t)k,(DDPIXELFORMAT*)((uint8_t*)p->lpvData+20));p->dwActualSize=20+sizeof(DDPIXELFORMAT);p->ddRVal=DD_OK;vx_d3d_dx8_stat[DX8S_FMT]++;return 1;}
 if(g->dwType==4&&p->dwExpectedSize>=20&&!IsBadReadPtr(p->lpvData,20)){vx_d3d_dx8_stat[DX8S_DXVER]++;vx_d3d_dx8_stat[DX8S_LASTVER]=((DWORD*)p->lpvData)[4];p->dwActualSize=p->dwExpectedSize<24?p->dwExpectedSize:24;p->ddRVal=DD_OK;return 1;} /* DXVERSION */
 /* GETD3DCAPS9 (0x10), GETDDIVERSION (0x23), queries, extended modes, multisample levels: not this DirectX 8 level driver's: the DirectX 9
  * runtime takes it as one */
 vx_d3d_dx8_stat[DX8S_GDI2NO]++;
 return 1;
}
#undef u32
#undef u16
