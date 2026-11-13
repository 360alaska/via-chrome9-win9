/* .544: the driver's T&L four vertices at a time (included by vx_d3d_tnl.inc before tn_draw). The .543 path did every vertex on its own:
 * one 4-wide kernel a matrix row, then scalar x87 code for the divide, the viewport, the clip codes, the dot products, the square roots
 * (three Newton steps with divides), the fog (a double polynomial), the colour bytes, with Watcom's 486/387 code generation between them.
 * Here a draw's vertices go through structure-of-arrays kernels in groups of four: the same operations in the same order for each
 * vertex, so a lane's result is what the one-vertex code gives, within the last bit where the step itself changed (the square root is the
 * exact sqrtps now, the fog's exponential a float polynomial: a colour byte can move by one; positions, 1/w and coordinates are the same
 * words).
 *
 * Two phases a draw (tn_draw):
 *   T  every vertex the triangles use: the clip-space position, its code, 1/w and the position words of its record (tnk_pos).
 *   L  every vertex a kept triangle uses (a vertex only dropped triangles use is never lit): eye position, the normal by the inverse
 *      transpose (normalized when NORMALIZENORMALS), the directional lights, the fog, the colour bytes, the coordinate set copied
 *      (tnk_light). The vertices of a clipped triangle are finished by the one-vertex code first (tn_attributes), since the
 *      clipper interpolates finished records.
 * The group path takes: the material from the material (mconst), directional lights only, no generated or transformed coordinate sets;
 * other draws finish attributes one vertex at a time. Unlit draws can also use the grouped fog path.
 *
 * The kernels: on the host SSE2 intrinsics (below); on the board the same instruction sequence assembled in src/vx_tnl_sse.asm (wasm, the
 * SSE instructions as bytes, since the HAL's assembler refuses the mnemonics: tools/tnl_asm.py writes it from the listing in that file).
 * Both read one frame of pointers and constants (tnk_frame) whose layout is fixed by the offsets below. Loads and stores are unaligned
 * (movups, movdqu): the arrays are plain statics. */
/* TNK KERNELS BEGIN 1 (tests/test_tnl_asm.c takes the marked parts as they are) */
typedef struct {
 /* the constants first: their offsets are the same on the host (64-bit pointers) and the board; the pointers come last */
 /* 0 */ float m[16][4];                /* world x view x projection, each element in four lanes */
 /* 256 */ float vp[8][4];              /* vx0 vxs vy0 vys z0 zs, splats; [6] 0.0; [7] unused */
 /* 384 */ float e[16][4];              /* world x view (eye space; the kernels read rows 0-2) */
 /* 640 */ float nm[9][4];              /* the normal matrix, row-vector order (n' = n x nm) */
 /* 784 */ float ld[8][4][4];           /* light k: direction to the light (x y z splats), [3] unused */
 /* 1296 */ float lc[8][4][4];          /* light k: diffuse x material diffuse: b g r splats, [3] 0 */
 /* 1808 */ float base[4][4];           /* the base colour b g r a splats */
 /* 1872 */ float fog[4][4];            /* [0] density, [1] fe, [2] fe-fs, [3] unused */
 /* 1936 */ uint32_t nl,lit,fogmode,rfog,nnorm,haveN,stride,n; /* fogmode 0 none, 1 exp, 2 exp2, 3 linear; stride a record's bytes; n lanes (a multiple of four) */
 /* 1968 */ uint32_t first,reuse_diffuse,eye_z_ready,pad2;
 /* 1984 */ float gx[4],gy[4],gz[4],gnx[4],gny[4],gnz[4]; /* phase L's gathered lanes */
 /* 2080 */ uint32_t gdc[4],gsc[4];     /* their diffuse and specular words as they came */
 /* 2112 */ uint32_t od[4],os[4];       /* out: the diffuse and specular words */
 /* 2144 */ float k255[4],kone[4],khalf[4],kl2e[4],kexpc[8][4],kneg80[4],kinf[4],ksign[4]; /* constants */
 /* 2384 */ uint32_t k127[4],kmask24[4];
 /* 2416 */ uint32_t kb[6][4];             /* 1 2 8 16 32 64 (the clip code bits) */
 /* 2512 */ uint32_t k7f[4];
 /* 2528 */ const float *sx,*sy,*sz;    /* a place's object-space position (gathered) */
 /* 2540 (board) */ float *cx,*cy,*cz,*cw,*rw; /* clip space and 1/w by place */
 /* 2560 */ uint8_t *cc;                /* clip code by place */
 /* 2564 */ uint8_t *rec;               /* the records (tn_buf) */
 uint32_t pad3,pad4;                   /* the scratch 16-byte aligned on the board */
 /* 2576 (board) */ float scratch[8][4]; /* the assembly's spills (ex ey ez, the fog factor, the normal) */
} tnk_frame;
#define TNK_OFF(f,o) typedef char tnk_off_##f[(offsetof(tnk_frame,f)==(o))?1:-1];
TNK_OFF(m,0) TNK_OFF(vp,256) TNK_OFF(e,384) TNK_OFF(nm,640) TNK_OFF(ld,784) TNK_OFF(lc,1296) TNK_OFF(base,1808) TNK_OFF(fog,1872) TNK_OFF(nl,1936) TNK_OFF(stride,1960) TNK_OFF(first,1968) TNK_OFF(reuse_diffuse,1972) TNK_OFF(eye_z_ready,1976)
TNK_OFF(gx,1984) TNK_OFF(gdc,2080) TNK_OFF(od,2112) TNK_OFF(k255,2144) TNK_OFF(kexpc,2208) TNK_OFF(kneg80,2336) TNK_OFF(kinf,2352) TNK_OFF(ksign,2368) TNK_OFF(k127,2384) TNK_OFF(kmask24,2400) TNK_OFF(kb,2416) TNK_OFF(k7f,2512) TNK_OFF(sx,2528)
#ifdef __WATCOMC__
TNK_OFF(cx,2540) TNK_OFF(cc,2560) TNK_OFF(rec,2564) TNK_OFF(scratch,2576)
#endif
/* TNK KERNELS END 1 */
static uint8_t tnk_mem[sizeof(tnk_frame)+16];static tnk_frame *tnkp; /* the frame 16-byte aligned: the kernels' memory operands (andps, mulps, paddd with [mem]) need it */
#define tnk (*tnkp)
#define TN_CH 256 /* phase T runs as the gather fills a chunk of this many places (the staging stays in L1; .544 first staged every place: 256 KiB) */
static float tn_sx[TN_CH+8],tn_sy[TN_CH+8],tn_sz[TN_CH+8],tn_cx[TN_MAX+8],tn_cy[TN_MAX+8],tn_cz[TN_MAX+8],tn_cw[TN_MAX+8];
static void tnk_splat(float *o,float v) {o[0]=o[1]=o[2]=o[3]=v;}

#ifndef __WATCOMC__
/* TNK KERNELS BEGIN 2 */
#include <emmintrin.h>
#include <stddef.h>
#define LD(p) _mm_loadu_ps((const float*)(p))
#define LDI(p) _mm_loadu_si128((const __m128i*)(p))
#define ST(p,v) _mm_storeu_ps((float*)(p),v)
/* phase T: places first .. first+n-1 (n a multiple of four) from staging lanes 0 .. n-1 */
static void tnk_pos(tnk_frame *f) {
 uint32_t i;
 const __m128 one=LD(f->kone),zero=_mm_setzero_ps(),inf=LD(f->kinf);
 for(i=0;i<f->n;i+=4){
  const uint32_t o=f->first+i;
  __m128 x=LD(f->sx+i),y=LD(f->sy+i),z=LD(f->sz+i),cx,cy,cz,cw,rw,px,py,pz,m; /* the staging by lane, the outputs by place */
  cx=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->m[0])),_mm_mul_ps(y,LD(f->m[4]))),_mm_mul_ps(z,LD(f->m[8]))),LD(f->m[12]));
  cy=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->m[1])),_mm_mul_ps(y,LD(f->m[5]))),_mm_mul_ps(z,LD(f->m[9]))),LD(f->m[13]));
  cz=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->m[2])),_mm_mul_ps(y,LD(f->m[6]))),_mm_mul_ps(z,LD(f->m[10]))),LD(f->m[14]));
  cw=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->m[3])),_mm_mul_ps(y,LD(f->m[7]))),_mm_mul_ps(z,LD(f->m[11]))),LD(f->m[15]));
  ST(f->cx+o,cx);ST(f->cy+o,cy);ST(f->cz+o,cz);ST(f->cw+o,cw);
  /* 1/w where w > 0, else 0 */
  m=_mm_cmpgt_ps(cw,zero);rw=_mm_and_ps(_mm_div_ps(one,cw),m);
  /* the codes: 1 z < 0 or w <= 0, 2 z > w, 8 x < -w, 16 x > w, 32 y < -w, 64 y > w; 0x7F a position not finite ((bits & 7F800000) ==
   * 7F800000 for any of x y z w, or 1/w overflowed: not a vertex the packet can take). Four codes packed to bytes (packssdw, packuswb). */
  {__m128i a=_mm_castps_si128(inf),c,t;__m128 nw=_mm_sub_ps(zero,cw);
   c=_mm_and_si128(_mm_castps_si128(_mm_or_ps(_mm_cmplt_ps(cz,zero),_mm_cmple_ps(cw,zero))),LDI(f->kb[0]));
   c=_mm_or_si128(c,_mm_and_si128(_mm_castps_si128(_mm_cmpgt_ps(cz,cw)),LDI(f->kb[1])));
   c=_mm_or_si128(c,_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(cx,nw)),LDI(f->kb[2])));
   c=_mm_or_si128(c,_mm_and_si128(_mm_castps_si128(_mm_cmpgt_ps(cx,cw)),LDI(f->kb[3])));
   c=_mm_or_si128(c,_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(cy,nw)),LDI(f->kb[4])));
   c=_mm_or_si128(c,_mm_and_si128(_mm_castps_si128(_mm_cmpgt_ps(cy,cw)),LDI(f->kb[5])));
   t=_mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(cx),a),a);t=_mm_or_si128(t,_mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(cy),a),a));
   t=_mm_or_si128(t,_mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(cz),a),a));t=_mm_or_si128(t,_mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(cw),a),a));
   t=_mm_or_si128(t,_mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(rw),a),a));
   c=_mm_or_si128(_mm_andnot_si128(t,c),_mm_and_si128(t,LDI(f->k7f)));
   c=_mm_packus_epi16(_mm_packs_epi32(c,c),c);
   {const uint32_t cw4=(uint32_t)_mm_cvtsi128_si32(c);uint8_t *q=f->cc+o;q[0]=(uint8_t)cw4;q[1]=(uint8_t)(cw4>>8);q[2]=(uint8_t)(cw4>>16);q[3]=(uint8_t)(cw4>>24);}} /* (the board writes the dword with movd) */
  /* the position words: vx0 + x rw vxs, vy0 + y rw vys, z0 + z rw zs, rw; -0 as +0 (adding +0.0 does that) */
  px=_mm_add_ps(_mm_add_ps(LD(f->vp[0]),_mm_mul_ps(_mm_mul_ps(cx,rw),LD(f->vp[1]))),zero);
  py=_mm_add_ps(_mm_add_ps(LD(f->vp[2]),_mm_mul_ps(_mm_mul_ps(cy,rw),LD(f->vp[3]))),zero);
  pz=_mm_add_ps(_mm_add_ps(LD(f->vp[4]),_mm_mul_ps(_mm_mul_ps(cz,rw),LD(f->vp[5]))),zero);
  {__m128 t0=_mm_unpacklo_ps(px,py),t1=_mm_unpacklo_ps(pz,rw),t2=_mm_unpackhi_ps(px,py),t3=_mm_unpackhi_ps(pz,rw);uint8_t *r=f->rec+(size_t)o*f->stride;
   ST(r,_mm_movelh_ps(t0,t1));ST(r+f->stride,_mm_movehl_ps(t1,t0));ST(r+2*f->stride,_mm_movelh_ps(t2,t3));ST(r+3*f->stride,_mm_movehl_ps(t3,t2));}
 }
}
/* e^x for four lanes, x <= 0 (the fog): 2^(x log2 e) = 2^i 2^g, i the floor, g in [0, 1) by a polynomial; x <= -80 gives 0, x >= 0 gives 1 */
static __m128 tnk_exp(const tnk_frame *f,__m128 x) {
 const __m128 zero=_mm_setzero_ps(),one=LD(f->kone),x0=x;
 __m128 lo=_mm_cmpgt_ps(x,LD(f->kneg80)),hi=_mm_cmplt_ps(x,zero),y,g,p,r;__m128i i;
 x=_mm_max_ps(x,LD(f->kneg80));x=_mm_min_ps(x,zero);
 y=_mm_mul_ps(x,LD(f->kl2e));i=_mm_cvttps_epi32(y); /* toward zero: y <= 0, so up; one down where y is not integral */
 i=_mm_sub_epi32(i,_mm_srli_epi32(_mm_castps_si128(_mm_cmplt_ps(y,_mm_cvtepi32_ps(i))),31));
 g=_mm_sub_ps(y,_mm_cvtepi32_ps(i)); /* 0 <= g < 1 */
 p=LD(f->kexpc[6]);p=_mm_add_ps(_mm_mul_ps(p,g),LD(f->kexpc[5]));p=_mm_add_ps(_mm_mul_ps(p,g),LD(f->kexpc[4]));p=_mm_add_ps(_mm_mul_ps(p,g),LD(f->kexpc[3]));
 p=_mm_add_ps(_mm_mul_ps(p,g),LD(f->kexpc[2]));p=_mm_add_ps(_mm_mul_ps(p,g),LD(f->kexpc[1]));p=_mm_add_ps(_mm_mul_ps(p,g),LD(f->kexpc[0]));
 r=_mm_mul_ps(p,_mm_castsi128_ps(_mm_slli_epi32(_mm_add_epi32(i,_mm_loadu_si128((const __m128i*)f->k127)),23))); /* 2^i, i in -116..0 */
 r=_mm_and_ps(_mm_and_ps(r,lo),hi); /* x <= -80: 0; x >= 0: 1; a NaN: 0 (as the one-vertex tn_exp) */
 r=_mm_or_ps(r,_mm_and_ps(one,_mm_cmpge_ps(x0,zero)));
 return r;
}
/* phase L for the four gathered lanes: the diffuse and specular words (od, os) */
static void tnk_light(tnk_frame *f) {
 const __m128 zero=_mm_setzero_ps(),one=LD(f->kone),k255=LD(f->k255);
 __m128 cb,cg,cr,ca,fa=one,nx=zero,ny=zero,nz=zero;
 if(f->fogmode){
  __m128 d;
  if(f->eye_z_ready)d=_mm_andnot_ps(LD(f->ksign),LD(f->gz));
  else{
   const __m128 x=LD(f->gx),y=LD(f->gy),z=LD(f->gz);
   const __m128 ez=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[2])),_mm_mul_ps(y,LD(f->e[6]))),_mm_mul_ps(z,LD(f->e[10]))),LD(f->e[14]));
   d=_mm_andnot_ps(LD(f->ksign),ez); /* |z| */
   if(f->rfog){
    const __m128 ex=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[0])),_mm_mul_ps(y,LD(f->e[4]))),_mm_mul_ps(z,LD(f->e[8]))),LD(f->e[12]));
    const __m128 ey=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[1])),_mm_mul_ps(y,LD(f->e[5]))),_mm_mul_ps(z,LD(f->e[9]))),LD(f->e[13]));
    d=_mm_sqrt_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(ex,ex),_mm_mul_ps(ey,ey)),_mm_mul_ps(ez,ez)));
   }
  }
  if(f->fogmode==3)fa=_mm_div_ps(_mm_sub_ps(LD(f->fog[1]),d),LD(f->fog[2])); /* (fe - d) / (fe - fs), the divide as tn_fin_to's (a reciprocal moved a byte); fe <= fs: left to tn_fin_to */
  else if(f->fogmode==1)fa=tnk_exp(f,_mm_sub_ps(zero,_mm_mul_ps(LD(f->fog[0]),d)));
  else{__m128 u=_mm_mul_ps(LD(f->fog[0]),d);fa=tnk_exp(f,_mm_sub_ps(zero,_mm_mul_ps(u,u)));}
 }
 if(f->lit){
  if(!f->reuse_diffuse){
   uint32_t k;
   cb=LD(f->base[0]);cg=LD(f->base[1]);cr=LD(f->base[2]);ca=LD(f->base[3]);
   if(f->haveN){
    __m128 ux=LD(f->gnx),uy=LD(f->gny),uz=LD(f->gnz);
    nx=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[0])),_mm_mul_ps(uy,LD(f->nm[3]))),_mm_mul_ps(uz,LD(f->nm[6])));
    ny=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[1])),_mm_mul_ps(uy,LD(f->nm[4]))),_mm_mul_ps(uz,LD(f->nm[7])));
    nz=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[2])),_mm_mul_ps(uy,LD(f->nm[5]))),_mm_mul_ps(uz,LD(f->nm[8])));
    if(f->nnorm){__m128 l=_mm_sqrt_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,nx),_mm_mul_ps(ny,ny)),_mm_mul_ps(nz,nz))),m=_mm_cmpgt_ps(l,zero),r;
     r=_mm_and_ps(_mm_div_ps(one,l),m);nx=_mm_or_ps(_mm_and_ps(_mm_mul_ps(nx,r),m),_mm_andnot_ps(m,nx));ny=_mm_or_ps(_mm_and_ps(_mm_mul_ps(ny,r),m),_mm_andnot_ps(m,ny));
     nz=_mm_or_ps(_mm_and_ps(_mm_mul_ps(nz,r),m),_mm_andnot_ps(m,nz));}
    for(k=0;k<f->nl;k++){ /* a directional light: N.L > 0 adds N.L x (diffuse x diffuse) */
     __m128 ndl=_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,LD(f->ld[k][0])),_mm_mul_ps(ny,LD(f->ld[k][1]))),_mm_mul_ps(nz,LD(f->ld[k][2])));
     ndl=_mm_and_ps(ndl,_mm_cmpgt_ps(ndl,zero));
     cb=_mm_add_ps(cb,_mm_mul_ps(ndl,LD(f->lc[k][0])));cg=_mm_add_ps(cg,_mm_mul_ps(ndl,LD(f->lc[k][1])));cr=_mm_add_ps(cr,_mm_mul_ps(ndl,LD(f->lc[k][2])));}
   }
   /* bytes: times 255, rounded to nearest, saturated to 0..255 (a NaN gives 0x80000000: 0 after the saturation), b | g << 8 | r << 16 | a << 24 */
   {__m128i b=_mm_cvtps_epi32(_mm_mul_ps(cb,k255)),g=_mm_cvtps_epi32(_mm_mul_ps(cg,k255)),r=_mm_cvtps_epi32(_mm_mul_ps(cr,k255)),a=_mm_cvtps_epi32(_mm_mul_ps(ca,k255)),w;
    b=_mm_packus_epi16(_mm_packs_epi32(b,b),_mm_setzero_si128());g=_mm_packus_epi16(_mm_packs_epi32(g,g),_mm_setzero_si128());
    r=_mm_packus_epi16(_mm_packs_epi32(r,r),_mm_setzero_si128());a=_mm_packus_epi16(_mm_packs_epi32(a,a),_mm_setzero_si128());
    w=_mm_unpacklo_epi8(_mm_unpacklo_epi8(b,r),_mm_unpacklo_epi8(g,a)); /* b g r a a lane */
    _mm_storeu_si128((__m128i*)f->od,w);}
  }
  {__m128i a=_mm_cvtps_epi32(_mm_mul_ps(fa,k255));a=_mm_packus_epi16(_mm_packs_epi32(a,a),_mm_setzero_si128());a=_mm_unpacklo_epi8(_mm_setzero_si128(),a);
   a=_mm_unpacklo_epi8(_mm_setzero_si128(),a); /* 0 0 0 a: the specular's colour is 0 (no specular lighting), the alpha the fog */
   _mm_storeu_si128((__m128i*)f->os,a);}
 }
 else{ /* unlit: the words as they came, the specular's alpha the fog (255 without fog) */
  __m128i a=_mm_cvtps_epi32(_mm_mul_ps(fa,k255));a=_mm_packus_epi16(_mm_packs_epi32(a,a),_mm_setzero_si128());a=_mm_unpacklo_epi8(_mm_setzero_si128(),a);a=_mm_unpacklo_epi8(_mm_setzero_si128(),a);
  _mm_storeu_si128((__m128i*)f->od,_mm_loadu_si128((const __m128i*)f->gdc));
  _mm_storeu_si128((__m128i*)f->os,_mm_or_si128(_mm_and_si128(_mm_loadu_si128((const __m128i*)f->gsc),_mm_loadu_si128((const __m128i*)f->kmask24)),a));
 }
}
#undef LD
#undef LDI
#undef ST
/* TNK KERNELS END 2 */
#else
/* the board: src/vx_tnl_sse.asm (wasm), the same sequences; cdecl, one argument */
extern void __cdecl tnk_pos(tnk_frame *f);
extern void __cdecl tnk_light(tnk_frame *f);
#endif

/* the frame's constants for the draw (after tn_prepare filled tn_s); 1 when the group path can finish this draw's vertices */
static int tn_soa_prepare(void) {
 const tn_setup *s=&tn_s;uint32_t k,q;int ok=1;
 static const float expc[7]={1.0f,0.6931471805599453f,0.2402265069591007f,0.0555041086648216f,0.0096181291076285f,0.0013333558146428f,0.0001540353039338f};
 if(!tnkp){
  tnkp=(tnk_frame*)(void*)(tnk_mem+((16-((size_t)tnk_mem&15))&15));
  memset(tnkp,0,sizeof(*tnkp));
  tnk_splat(tnk.kone,1.0f);tnk_splat(tnk.k255,255.0f);tnk_splat(tnk.khalf,0.5f);tnk_splat(tnk.kl2e,1.4426950408889634f);tnk_splat(tnk.kneg80,-80.0f);
  for(k=0;k<7;k++)tnk_splat(tnk.kexpc[k],expc[k]);{vx_fu u;u.n=0x7F800000UL;tnk_splat(tnk.kinf,u.f);u.n=0x80000000UL;tnk_splat(tnk.ksign,u.f);}
  for(k=0;k<4;k++){tnk.k127[k]=127;tnk.kmask24[k]=0x00FFFFFFUL;tnk.k7f[k]=0x7F;tnk.kb[0][k]=1;tnk.kb[1][k]=2;tnk.kb[2][k]=8;tnk.kb[3][k]=16;tnk.kb[4][k]=32;tnk.kb[5][k]=64;}
 }
 for(k=0;k<16;k++)tnk_splat(tnk.m[k],s->wvp[k]);
 tnk_splat(tnk.vp[0],s->vx0);tnk_splat(tnk.vp[1],s->vxs);tnk_splat(tnk.vp[2],s->vy0);tnk_splat(tnk.vp[3],s->vys);tnk_splat(tnk.vp[4],s->z0);tnk_splat(tnk.vp[5],s->zs);tnk_splat(tnk.vp[6],0.0f);
 tnk.eye_z_ready=s->fog_clip_w;
 tnk.lit=s->lit;tnk.nnorm=s->nnorm;tnk.haveN=s->lit&&s->nl&&s->inormal!=0xFFFFFFFFUL;tnk.rfog=s->rfog;tnk.fogmode=s->fog?s->fmode:0;
 if(s->fog){
  if(!tnk.eye_z_ready){
   const uint32_t step=s->rfog?1:4;
   for(k=s->rfog?0:2;k<16;k+=step)tnk_splat(tnk.e[k],s->wv[k]);
  }
  tnk_splat(tnk.fog[0],s->fd);tnk_splat(tnk.fog[1],s->fe);tnk_splat(tnk.fog[2],s->fe-s->fs); /* exp and exp2 read fd; linear fe and fe-fs */
  if(s->fmode!=1&&s->fmode!=2&&s->fmode!=3)ok=0;
  if(s->fmode==3&&!(s->fe>s->fs))ok=0;} /* linear fog with fe <= fs is 1 in the one-vertex code: left to it (rare) */
 tnk.nl=0;
 if(s->lit){
  if(tnk.haveN)for(k=0;k<9;k++)tnk_splat(tnk.nm[k],s->nrm[k]);
  if(!s->mconst)ok=0;
  for(k=0;k<s->nl;k++){if(!s->ldir[k]){ok=0;break;}
   if(tnk.nl<8){tnk_splat(tnk.ld[tnk.nl][0],s->ld[k][0]);tnk_splat(tnk.ld[tnk.nl][1],s->ld[k][1]);tnk_splat(tnk.ld[tnk.nl][2],s->ld[k][2]);
    for(q=0;q<3;q++)tnk_splat(tnk.lc[tnk.nl][q],s->ld4[k][q]);tnk_splat(tnk.lc[tnk.nl][3],0.0f);tnk.nl++;}
   else ok=0;}
  for(q=0;q<4;q++)tnk_splat(tnk.base[q],s->base4[q]);}
 for(k=0;k<tn_sets;k++){if(s->gen[k]||(s->ttf[k]&0xFF))ok=0;if(s->idx[k]>=s->ins)ok=0;} /* a set read straight from the vertex, or none */
 tnk.stride=tn_ostride;tnk.rec=tn_buf;tnk.sx=tn_sx;tnk.sy=tn_sy;tnk.sz=tn_sz;tnk.cx=tn_cx;tnk.cy=tn_cy;tnk.cz=tn_cz;tnk.cw=tn_cw;tnk.rw=0;tnk.cc=tn_cc;
 return ok;
}
/* phase T over places first .. first+n-1 (n <= TN_CH, staged in tn_sx tn_sy tn_sz lanes 0 .. n-1): the records' position words, tn_cc, the clip arrays */
static void tn_soa_pos(uint32_t first,uint32_t n) {
 uint32_t k;
 tnk.first=first;tnk.n=(n+3)&~3U;
 tnk_pos(tnkp);
 for(k=first;k<first+n;k++){tn_done[k]=0;if(tn_cc[k]==0x7F){memset(tn_buf+k*tn_ostride,0,tn_ostride);tn_done[k]=1;}} /* a position not finite: the record zeros, never sent */
}
/* phase L: the places in list (n of them) finished by groups of four (a group's spare lanes repeat its last place) */
static void tn_soa_finish(const uint16_t *list,uint32_t n) {
 const tn_setup *s=&tn_s;
 uint32_t i,j,key[3];
 int uniform=tnk.haveN&&n>=8;
 /* Try reuse only when the first eight normals agree. The first group
  * still runs the full SSE2 kernel and supplies the diffuse words. */
 if(uniform){
  const uint8_t *p=tn_sp[list[0]]+s->inormal;
  key[0]=u32(p);key[1]=u32(p+4);key[2]=u32(p+8);
  for(j=1;j<8;j++){
   p=tn_sp[list[j]]+s->inormal;
   if(u32(p)!=key[0]||u32(p+4)!=key[1]||u32(p+8)!=key[2]){uniform=0;break;}
  }
 }
 for(i=0;i<n;i+=4){
  uint32_t o4[4];
  if(uniform&&i>=8){
   for(j=0;j<4&&i+j<n;j++){
    const uint8_t *p=tn_sp[list[i+j]]+s->inormal;
    if(u32(p)!=key[0]||u32(p+4)!=key[1]||u32(p+8)!=key[2]){uniform=0;break;}
   }
  }
  tnk.reuse_diffuse=uniform&&i!=0;
  for(j=0;j<4;j++){uint32_t o=list[i+j<n?i+j:n-1];const uint8_t *v=tn_sp[o];o4[j]=o;
   if(tnk.eye_z_ready)tnk.gz[j]=tn_cw[o];
   else if(tnk.fogmode){tnk.gx[j]=tf(v);tnk.gy[j]=tf(v+4);tnk.gz[j]=tf(v+8);}
   if(tnk.haveN&&!tnk.reuse_diffuse){tnk.gnx[j]=tf(v+s->inormal);tnk.gny[j]=tf(v+s->inormal+4);tnk.gnz[j]=tf(v+s->inormal+8);}
   if(!tnk.lit){tnk.gdc[j]=s->idif!=0xFFFFFFFFUL?u32(v+s->idif):0xFFFFFFFFUL;tnk.gsc[j]=s->ispe!=0xFFFFFFFFUL?u32(v+s->ispe):0;}
  }
  if(!tnk.reuse_diffuse||tnk.fogmode)tnk_light(tnkp);
  for(j=0;j<4&&i+j<n;j++){const uint32_t o=o4[j];uint32_t *r=(uint32_t*)(void*)(tn_buf+o*tn_ostride),k;const uint8_t *v=tn_sp[o];
   r[4]=tnk.od[j];r[5]=tnk.os[j];
   for(k=0;k<tn_sets;k++){const uint8_t *q=v+s->itex[s->idx[k]];uint32_t u=u32(q),vv=s->itn[s->idx[k]]>1?u32(q+4):0;
    if((u&0x7F800000UL)==0x7F800000UL)u=0;if((vv&0x7F800000UL)==0x7F800000UL)vv=0; /* a non-finite coordinate: 0 (the VxD refuses them) */
    r[6+k*2]=u;r[7+k*2]=vv;}
   tn_done[o]=1;}
  vx_d3d_tn[8]+=s->lit?(i+4<=n?4:n-i):0;if(s->fog)vx_d3d_tn[9]+=i+4<=n?4:n-i;
 }
}
