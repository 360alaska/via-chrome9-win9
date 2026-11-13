/* .545: the indexed pass-through's span (ix_pass), four vertices at a time where the processor has SSE2 (included by vx_d3d_core.c after
 * ix_check and fast_vertex_pre_w). GTA3 on .544, T&L off: the HAL took 39 ms of a 132 ms frame on pretransformed vertices, its first
 * functions fast_vertex_pre_w, ix_pass and ix_check: per vertex a call, the template's fields read again, a dozen branches.
 *
 * Two kernels over one frame (vxs_frame):
 *   vxs_check  the span's vertices tested as ix_check tests them (x and y inside the viewport or the guard band, 0 <= z <= 1 by the
 *              bits, rhw positive and finite), four at a time: the 16 bytes x y z rhw of four vertices transposed, the tests as biased
 *              signed compares. Returns how many leading vertices pass, a multiple of four or the first that fails; the caller tests the
 *              rest (fewer than four, or the one that failed) with ix_check, so the result is ix_check's.
 *   vxs_conv   the span's vertices into the packet's records as fast_vertex_pre_w writes them (no snapping, no ZBIAS, the vertex's own
 *              colours: the only case ix_pass has with them off): x y z with -0 as +0 (one pcmpeqd/pandn on the 16 bytes; rhw as it is),
 *              the colour word or white, the specular word or 0xFF000000 or 0, the coordinate words with a non-finite word as 0, the
 *              second set or the first again. Ten words a vertex, in vertex order, at wstride bytes apart (8 or 10 words: the 8-word
 *              packets take the last two words of a vertex as the next one's first two, overwritten by it, as fast_vertex_pre_w does).
 *
 *   vxs_max16  the largest index of an INDEXEDTRIANGLELIST2 token (the check that every index is below vv_lim), eight at a time
 *              (pmaxsw on the indices xor 0x8000); the tail one at a time.
 *
 * The board: src/vx_hal_sse.asm (wasm, the instructions as bytes: tools/hal_asm.py writes it); the C below is the reference, used on the
 * host, on the board without SSE2 (vx_d3d_sse2 0), and against the bytes in tests/test_hal_asm.c. */
/* VXS KERNELS BEGIN */
typedef struct {
 /* 0 */ const uint8_t *vb;           /* the span's first vertex */
 /* 4 */ uint32_t stride,n;           /* the vertex stride; the vertices */
 /* 12 */ uint32_t *w;                /* vxs_conv: the first record */
 /* 16 */ uint32_t wstride;           /* bytes from one record to the next (32 or 40) */
 /* 20 */ uint32_t coff;              /* the colour's offset, 0: none (white) */
 /* 24 */ uint32_t smode,soff;        /* specular: 0 none (0), 1 white (0xFF000000), 2 the word at soff */
 /* 32 */ uint32_t t0,v0on;           /* the first set's offset (0: no coordinates, 0 0), its second word read */
 /* 40 */ uint32_t two,t1,v1on;       /* two units: the second set's offset (0: 0 0), its second word read; one unit: the first set again */
 /* 52 */ uint32_t pad0,pad1,pad2;
 /* 64 */ uint32_t xlo,xrange,ylo,yrange; /* GB_IN's: w - lo <= range */
 /* 80 */ uint32_t xg,yg;             /* 0xFFFFFFFF: the guard band also takes 0x80000000..0xBF000000 (tc.guard and lo 0) */
 /* 88 */ uint32_t pad3,pad4;
 /* 96: the kernels' lane constants (the C does not read them): fixed (vxs_init) */
 /* 96 */ uint32_t kbias[4],kgu[4],kz[4],kw[4],kone[4],kneg0[4]; /* 0x80000000; 0xBF000000, 0x3F800000, 0x7F7FFFFF each xor the bias; 1; -0 in lanes 0-2 */
 /* 192: the template's (vxs_tc) */
 /* 192 */ uint32_t kxlo[4],kxr[4],kylo[4],kyr[4],kxg[4],kyg[4]; /* xlo, xrange ^ bias, ylo, yrange ^ bias, xg, yg */
} vxs_frame;
static uint32_t vxs_check_c(const vxs_frame *f) {
 uint32_t i;const uint8_t *p=f->vb;
 for(i=0;i<f->n;i++,p+=f->stride){const uint32_t x=u32(p),y=u32(p+4),z=u32(p+8),w=u32(p+12);
  const int gx=x-f->xlo<=f->xrange||(f->xg&&x>=0x80000000UL&&x<=0xBF000000UL),gy=y-f->ylo<=f->yrange||(f->yg&&y>=0x80000000UL&&y<=0xBF000000UL);
  if(!(gx&&gy&&z<=0x3F800000UL&&w-1UL<0x7F7FFFFFUL))break;}
 return i;
}
static uint32_t vxs_fin(uint32_t u) {return (u&0x7F800000UL)==0x7F800000UL?0:u;}
static void vxs_conv_c(const vxs_frame *f) {
 uint32_t i;const uint8_t *p=f->vb;uint8_t *o=(uint8_t*)f->w;
 for(i=0;i<f->n;i++,p+=f->stride,o+=f->wstride){uint32_t *w=(uint32_t*)(void*)o,x=u32(p),y=u32(p+4),z=u32(p+8),u=0,v=0;
  if(x==0x80000000UL)x=0;if(y==0x80000000UL)y=0;if(z==0x80000000UL)z=0;
  if(f->t0){u=vxs_fin(u32(p+f->t0));if(f->v0on)v=vxs_fin(u32(p+f->t0+4));}
  w[0]=x;w[1]=y;w[2]=z;w[3]=u32(p+12);w[4]=f->coff?u32(p+f->coff):0xFFFFFFFFUL;
  w[5]=f->smode==2?u32(p+f->soff):f->smode==1?0xFF000000UL:0;w[6]=u;w[7]=v;
  if(f->two){uint32_t u1=0,v1=0;if(f->t1){u1=vxs_fin(u32(p+f->t1));if(f->v1on)v1=vxs_fin(u32(p+f->t1+4));}w[8]=u1;w[9]=v1;}
  else{w[8]=u;w[9]=v;}
 }
}
/* the largest of count 16-bit indices (INDEXEDTRIANGLELIST2's token: three a triangle, back to back) */
static uint32_t vxs_max16_c(const uint8_t *p,uint32_t count) {uint32_t m=0,k,a;for(k=0;k<count;k++){a=(uint32_t)p[k*2]|(uint32_t)p[k*2+1]<<8;if(a>m)m=a;}return m;}
/* VXS KERNELS END */
#define VXS_OFF(f,o) typedef char vxs_off_##f[(offsetof(vxs_frame,f)==(o))?1:-1];
#ifdef __WATCOMC__
VXS_OFF(vb,0) VXS_OFF(stride,4) VXS_OFF(n,8) VXS_OFF(w,12) VXS_OFF(wstride,16) VXS_OFF(coff,20) VXS_OFF(smode,24) VXS_OFF(soff,28) VXS_OFF(t0,32)
VXS_OFF(v0on,36) VXS_OFF(two,40) VXS_OFF(t1,44) VXS_OFF(v1on,48) VXS_OFF(xlo,64) VXS_OFF(xrange,68) VXS_OFF(ylo,72) VXS_OFF(yrange,76) VXS_OFF(xg,80) VXS_OFF(yg,84)
VXS_OFF(kbias,96) VXS_OFF(kgu,112) VXS_OFF(kz,128) VXS_OFF(kw,144) VXS_OFF(kone,160) VXS_OFF(kneg0,176) VXS_OFF(kxlo,192) VXS_OFF(kxr,208) VXS_OFF(kylo,224) VXS_OFF(kyr,240) VXS_OFF(kxg,256) VXS_OFF(kyg,272)
extern uint32_t __cdecl vxs_check(const vxs_frame *f);
extern void __cdecl vxs_conv(const vxs_frame *f);
extern uint32_t __cdecl vxs_max16(const uint8_t *p,uint32_t count);
#define VXS_MAX16(p,n) (vx_d3d_sse2?vxs_max16(p,n):vxs_max16_c(p,n))
#define VXS_CHECK(f) (vx_d3d_sse2?vxs_check(f):vxs_check_c(f))
#define VXS_CONV(f) do{if(vx_d3d_sse2)vxs_conv(f);else vxs_conv_c(f);}while(0)
#else
#define VXS_CHECK(f) vxs_check_c(f)
#define VXS_CONV(f) vxs_conv_c(f)
#define VXS_MAX16(p,n) vxs_max16_c(p,n)
#endif
uint32_t vx_d3d_sse2,vx_d3d_vxs[4]; /* vx_d3d_sse2: the HAL sets it from the VxD's 0xFFFFFE34 (SSE2 and OSFXSR); vx_d3d_vxs: [0] spans,
 * [1] vertices passed by vxs_check, [2] spans converted by vxs_conv, [3] their vertices */
static vxs_frame vxs;static int vxs_ready;static uint32_t vxs_tid,vxs_fvf;
static void vxs_k(uint32_t *k,uint32_t v) {k[0]=k[1]=k[2]=k[3]=v;}
/* the template's part of the frame (once a span: tc can change between spans) */
static void vxs_tc(void) {
 const uint32_t tex=tc.tex,two=tc.tex&&tc.units>1;
 /* .552: the frame's state is constant for this template and FVF. Span
  * pointers, lengths and output strides are still set at every use below.
  * Include FVF: tc_ready can change the layout without rebuilding tc.id. */
 if(vxs_ready&&vxs_template_valid&&vxs_tid==tc.id&&vxs_fvf==tc.lay.fvf)return;
 if(!vxs_ready){vxs_k(vxs.kbias,0x80000000UL);vxs_k(vxs.kgu,0xBF000000UL^0x80000000UL);vxs_k(vxs.kz,0x3F800000UL^0x80000000UL);vxs_k(vxs.kw,0x7F7FFFFFUL^0x80000000UL);
  vxs_k(vxs.kone,1);vxs_k(vxs.kneg0,0x80000000UL);vxs.kneg0[3]=0;vxs_ready=1;}
 vxs.xlo=tc.bxl;vxs.xrange=tc.bxr-tc.bxl;vxs.ylo=tc.byt;vxs.yrange=tc.byb-tc.byt;
 vxs.xg=tc.guard&&!tc.bxl?0xFFFFFFFFUL:0;vxs.yg=tc.guard&&!tc.byt?0xFFFFFFFFUL:0;
 vxs_k(vxs.kxlo,vxs.xlo);vxs_k(vxs.kxr,vxs.xrange^0x80000000UL);vxs_k(vxs.kylo,vxs.ylo);vxs_k(vxs.kyr,vxs.yrange^0x80000000UL);vxs_k(vxs.kxg,vxs.xg);vxs_k(vxs.kyg,vxs.yg);
 vxs.coff=tc.lay.coff;vxs.smode=tc.prog>=0?(tc.lay.soff?2:1):0;vxs.soff=tc.lay.soff;
 vxs.t0=tex?tc.lay.t0:0;vxs.v0on=tc.lay.n0>=2;vxs.two=two;vxs.t1=two?tc.lay.t1:0;vxs.v1on=tc.lay.n1>=2;
 vxs_tid=tc.id;vxs_fvf=tc.lay.fvf;vxs_template_valid=1;
}
/* .546: the indexed token's vertex range (dp2_body sets it with ix_proof_new) and ix_list's proof that every vertex in it passes
 * (0 not tried, 1 proven, 2 not), for the template tc.id */
static uint32_t ix_tok_lo,ix_tok_hi,ix_tok_n,ix_proof_tid,ix_proof_state;
static void ix_proof_new(uint32_t lo,uint32_t hi,uint32_t n) {ix_tok_lo=lo;ix_tok_hi=hi;ix_tok_n=n;ix_proof_state=0;}
uint32_t vx_d3d_vxs_skip; /* .547: spans inside a proven range, not checked again (QueryPhases 311) */
/* the span's vertices checked: 1 when every one passes (ix_check's answer for each) */
static int vxs_span_ok(const vx_d3d_input *in,uint32_t lo,uint32_t hi) {
 const uint8_t *const vb=in->vertices;const uint32_t st=in->vertex_stride;uint32_t v,r;
 vxs_tc();vxs.vb=vb+lo*st;vxs.stride=st;vxs.n=hi-lo+1;
 r=VXS_CHECK(&vxs);vx_d3d_vxs[0]++;vx_d3d_vxs[1]+=r;vv_fast_n+=r;
 for(v=lo+r;v<=hi;v++)if(!ix_check(in,v,vb+v*st))return 0;
 return 1;
}
/* .555: the first nv vertices of an input that is not the bound vertex buffer (a fan's vertices in its command): 1 when every one passes
 * fast_triangle's tests (x and y inside the clip edges or the guard band, 0 <= z <= 1, rhw positive and finite). vxs_span_ok is not used
 * for them: ix_check marks vv_ok, whose bits are the bound buffer's indices. */
static int vxs_imm_ok(const vx_d3d_input *in,uint32_t nv) {
 const uint8_t *p=in->vertices;const uint32_t st=in->vertex_stride;uint32_t v;
 vxs_tc();vxs.vb=p;vxs.stride=st;vxs.n=nv;
 v=VXS_CHECK(&vxs);vx_d3d_vxs[0]++;vx_d3d_vxs[1]+=v;
 for(p+=v*st;v<nv;v++,p+=st)if(!(GB_IN(u32(p),tc.bxl,tc.bxr)&&GB_IN(u32(p+4),tc.byt,tc.byb)&&u32(p+8)<=0x3F800000UL&&u32(p+12)-1UL<0x7F7FFFFFUL))return 0;
 return 1;
}
/* the span's vertices into records l, l + 1, ... of the open indexed packet, as fast_vertex_pre_w(IXW(l), ...) one by one */
static void vxs_span_conv(const vx_d3d_input *in,uint32_t lo,uint32_t hi,uint32_t l) {
 const uint8_t *const vb=in->vertices;const uint32_t st=in->vertex_stride;
 if(tri_snap||tc.zb!=0){uint32_t v;for(v=lo;v<=hi;v++,l++)fast_vertex_pre_w(IXW(l),vb+v*st,0xFFFFFFFFUL,vb+v*st);return;}
 vxs_tc();vxs.vb=vb+lo*st;vxs.stride=st;vxs.n=hi-lo+1;vxs.w=IXW(l);vxs.wstride=(bq.bursts.b[0]&VX3DN_IX_V8)?32UL:40UL;
 VXS_CONV(&vxs);vx_d3d_vxs[2]++;vx_d3d_vxs[3]+=vxs.n;vx_d3d_geo[11]+=vx_d3d_geo_on*vxs.n;
}
