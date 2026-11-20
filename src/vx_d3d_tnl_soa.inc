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
 /* .647: point lights (tnk_plight): their count, then each one's eye-space position (x y z splats), range a0 a1 a2 (splats), and its
  * diffuse and ambient products by lane (b g r: the light's colour times the material's, or the vertex's colour as a material source) */
 /* 2704 */ uint32_t npl,pad5,pad6,pad7;
 /* 2720 */ float pp[8][3][4];
 /* 3104 */ float pr[8][4][4];
 /* 3616 */ float pdc[8][3][4];
 /* 4000 */ float pac[8][3][4];
 /* .651: specular lighting in groups (tnk_spec): each light's specular product by lane (b g r: the light's specular colour times the
  * material's, or the vertex's colour as the specular source), the directional lights at 0-7 and the point lights at 8-15; the power
  * (splat); the logarithm's constants; LOCALVIEWER; the power's case (0 a power, 1 the power 1: N.H itself, 2 a power of 0 or below: 1) */
 /* 4384 */ float sdc[16][3][4];
 /* 5152 */ float spow[4];
 /* 5168 */ float klog[4][4];          /* 1/3 1/5 1/7 1/9 splats */
 /* 5232 */ float ksqrt2[4];
 /* 5248 */ uint32_t kmant[4];         /* 007FFFFF */
 /* 5264 */ uint32_t spec,lview,spmode,pad8;
 /* .652: the colour from the vertices in groups (tnk_cv): the lights in the set-up's order (cvdir: 1 directional, 0 point), each one's
  * diffuse, ambient and specular colour (b g r splats), the material's colours (diffuse, ambient, emissive, specular; b g r a splats),
  * the global ambient, the sources (0 the material, 1 the vertex's diffuse word, 2 its specular word) */
 /* 5280 */ uint32_t cvn,dsrc,asrc,esrc,ssrc,cvconv;float kr255;uint32_t pad11; /* .694: kr255 = 1/255 (tnk_cv) */ /* .678: cvconv, the colour words tnk_cv converts (tn_soa_prepare_b) */
 /* 5312 */ uint32_t cvdir[16];
 /* 5376 */ float cvl[16][3][3][4];
 /* 7680 */ float cvm[4][4][4];
 /* 7936 */ float cvamb[3][4];
 /* 7984 */ uint32_t kff[4];
 /* .653: spot lights in groups (tnk_splight; tnk_spec): nspot, how many of the point lights are spot lights; by point light: a spot light
  * or not, the power's case of its falloff (as spmode), its direction in eye space (x y z splats), cos(phi/2) cos(theta/2) and the falloff
  * (splats) */
 /* 8000 */ uint32_t nspot,pad12,pad13,pad14;
 /* 8016 */ uint32_t pspot[8],pmode[8];
 /* 8080 */ float psd[8][3][4];
 /* 8464 */ float pcone[8][3][4];
 /* .654: the lanes' base colour without the point lights (was the static tn_lbase: 8-byte aligned in .652 and .653, across a cache line
  * and a page; copied into base every group lit by point lights) */
 /* 8848 */ float lbase[4][4];
 /* spint, an integer specular power 2-255 (spmode 3: x^n by squaring, in place of e^(n ln x), .659).
  * .697 (board, 3DMark 2001's High Polygon Count, 8 lights, N/A on .694: five frames of 2.6 s in its 12 s, under its minimum count; VXTLP
  * 6l: the group kernels 34% of the T&L): with point lights and specular lighting, tnk_splight does the specular sums itself, light by light
  * as it goes (sdone 1: the sums b g r in sacc, the directional lights' first, then the point lights' in order, as tnk_spec added them),
  * and tnk_spec only packs them after tnk_light (.659-.696 stored each light's way, attenuation and range mask, the normal and the eye-space
  * position, and tnk_spec loaded them all again and made N.L again: the same operations, now once). pamb: bit k, point light k's
  * ambient product is not 0 (the ones that are add nothing; tnk_cv leaves their 0 alone). cvfirst / cvvary: tnk_cv makes the products
  * whose inputs do not come from the vertices once a set-up (cvvary bits: 1 the diffuse source, 2 the ambient, 4 the emissive, 8 the
  * specular is the vertex's), every group only the rest: the same products, made once */
 /* 8912 */ uint32_t sdone,spint,pamb,cvfirst;
 /* 8928 */ float sacc[3][4];
 /* 8976 */ uint32_t cvvary; uint8_t *vc; uint32_t pad18,pad19; /* .700: vc, the guard band's viewport codes by place (tn_vc; 0 without the guard band or its codes); .702: tnk_vcodes writes them, tnk_pos only sets vacc */
 /* 8992 */ float spcut[4]; /* .697: tn_setup's spcut (splat): an N.H at or below it adds no specular term */
 /* 9008 */ uint32_t vacc[4]; /* .702: tnk_pos ORs here, by lane, whether a vertex lies past the viewport's side (vc set: the guard band with its codes) */
 /* 9024 */ uint32_t kvc[4][4]; /* .702: 1 2 4 8 (the viewport codes' bits for tnk_vcodes: no constant in memory) */
} tnk_frame;
#define TNK_OFF(f,o) typedef char tnk_off_##f[(offsetof(tnk_frame,f)==(o))?1:-1];
TNK_OFF(m,0) TNK_OFF(vp,256) TNK_OFF(e,384) TNK_OFF(nm,640) TNK_OFF(ld,784) TNK_OFF(lc,1296) TNK_OFF(base,1808) TNK_OFF(fog,1872) TNK_OFF(nl,1936) TNK_OFF(stride,1960) TNK_OFF(first,1968) TNK_OFF(reuse_diffuse,1972) TNK_OFF(eye_z_ready,1976)
TNK_OFF(gx,1984) TNK_OFF(gdc,2080) TNK_OFF(od,2112) TNK_OFF(k255,2144) TNK_OFF(kexpc,2208) TNK_OFF(kneg80,2336) TNK_OFF(kinf,2352) TNK_OFF(ksign,2368) TNK_OFF(k127,2384) TNK_OFF(kmask24,2400) TNK_OFF(kb,2416) TNK_OFF(k7f,2512) TNK_OFF(sx,2528)
#ifdef __WATCOMC__
TNK_OFF(cx,2540) TNK_OFF(cc,2560) TNK_OFF(rec,2564) TNK_OFF(scratch,2576) TNK_OFF(npl,2704) TNK_OFF(pp,2720) TNK_OFF(pr,3104) TNK_OFF(pdc,3616) TNK_OFF(pac,4000)
TNK_OFF(sdc,4384) TNK_OFF(spow,5152) TNK_OFF(klog,5168) TNK_OFF(ksqrt2,5232) TNK_OFF(kmant,5248) TNK_OFF(spec,5264)
TNK_OFF(cvn,5280) TNK_OFF(cvdir,5312) TNK_OFF(cvl,5376) TNK_OFF(cvm,7680) TNK_OFF(cvamb,7936) TNK_OFF(kff,7984)
TNK_OFF(nspot,8000) TNK_OFF(pspot,8016) TNK_OFF(pmode,8048) TNK_OFF(psd,8080) TNK_OFF(pcone,8464) TNK_OFF(lbase,8848)
TNK_OFF(sdone,8912) TNK_OFF(sacc,8928) TNK_OFF(cvvary,8976) TNK_OFF(vc,8980) TNK_OFF(spcut,8992) TNK_OFF(vacc,9008) TNK_OFF(kvc,9024)
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
  /* .702 (reviews of T700: the codes' 20 instructions a group were most of the guard band's cost in the positions, about 21 cycles a
   * vertex on the in-order C7, though most draws lie wholly inside the viewport): here only whether a lane is past a side, |x| > w/2 or
   * |y| > w/2, ORed into vacc (a NaN compares false; a w <= 0 makes w/2 <= 0, so the lane counts as past: the codes then decide). The
   * codes themselves are made by tnk_vcodes, over the draw's places, only when vacc is set. */
  if(f->vc){const __m128 hw=_mm_mul_ps(cw,LD(f->khalf)),sm=LD(f->ksign);
   const __m128 a=_mm_or_ps(_mm_cmplt_ps(hw,_mm_andnot_ps(sm,cx)),_mm_cmplt_ps(hw,_mm_andnot_ps(sm,cy)));
   ST(f->vacc,_mm_or_ps(LD(f->vacc),a));}
  /* the position words: vx0 + x rw vxs, vy0 + y rw vys, z0 + z rw zs, rw; -0 as +0 (adding +0.0 does that) */
  px=_mm_add_ps(_mm_add_ps(LD(f->vp[0]),_mm_mul_ps(_mm_mul_ps(cx,rw),LD(f->vp[1]))),zero);
  py=_mm_add_ps(_mm_add_ps(LD(f->vp[2]),_mm_mul_ps(_mm_mul_ps(cy,rw),LD(f->vp[3]))),zero);
  pz=_mm_add_ps(_mm_add_ps(LD(f->vp[4]),_mm_mul_ps(_mm_mul_ps(cz,rw),LD(f->vp[5]))),zero);
  {__m128 t0=_mm_unpacklo_ps(px,py),t1=_mm_unpacklo_ps(pz,rw),t2=_mm_unpackhi_ps(px,py),t3=_mm_unpackhi_ps(pz,rw);uint8_t *r=f->rec+(size_t)o*f->stride;
   ST(r,_mm_movelh_ps(t0,t1));ST(r+f->stride,_mm_movehl_ps(t1,t0));ST(r+2*f->stride,_mm_movelh_ps(t2,t3));ST(r+3*f->stride,_mm_movehl_ps(t3,t2));}
 }
}
/* .702: the viewport's codes for places first .. first+n-1 (n a multiple of four) from the clip arrays: 1 x < -w/2, 2 x > w/2, 4 y < -w/2,
 * 8 y > w/2 (the guard band halves clip x and y, so w/2 is the viewport's side). Each is a plane through the eye, so three corners beyond
 * the same one put the triangle beyond it whatever their w; a NaN gives no code. Run by tn_vdone only for a draw tnk_pos found past a side. */
static void tnk_vcodes(tnk_frame *f) {
 uint32_t i;const __m128 zero=_mm_setzero_ps();
 for(i=0;i<f->n;i+=4){
  const uint32_t o=f->first+i;const __m128 cx=LD(f->cx+o),cy=LD(f->cy+o),hw=_mm_mul_ps(LD(f->cw+o),LD(f->khalf)),nh=_mm_sub_ps(zero,hw);__m128i v;
  v=_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(cx,nh)),LDI(f->kvc[0]));
  v=_mm_or_si128(v,_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(hw,cx)),LDI(f->kvc[1])));
  v=_mm_or_si128(v,_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(cy,nh)),LDI(f->kvc[2])));
  v=_mm_or_si128(v,_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(hw,cy)),LDI(f->kvc[3])));
  v=_mm_packus_epi16(_mm_packs_epi32(v,v),v);
  {const uint32_t v4=(uint32_t)_mm_cvtsi128_si32(v);uint8_t *q=f->vc+o;q[0]=(uint8_t)v4;q[1]=(uint8_t)(v4>>8);q[2]=(uint8_t)(v4>>16);q[3]=(uint8_t)(v4>>24);}
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
/* .647: the point lights for four gathered lanes, added into the lanes' base colour (base b g r) before tnk_light adds the directional
 * lights and packs the bytes. The one-vertex code's terms (tn_light_color): the eye-space position by world x view, the way to the light
 * and its length d; past the range nothing at all; the way made unit length where d > 0; attenuation 1 / (a0 + a1 d + a2 d d) where that
 * is above 0, else 1; the ambient product times it; with normals, N.L > 0 times it times the diffuse product. The selects are written as
 * xor, and, xor (the same bits as and, andnot, or) so that the board's assembly needs no spare register. */
static __m128 tnk_cone(const tnk_frame *f,uint32_t q,__m128 lx,__m128 ly,__m128 lz);
static __m128 tnk_pow(const tnk_frame *f,__m128 x,const float *pw,uint32_t mode);
/* .697: one light's specular term for four lanes, added into sb sg sr: as tnk_spec did it (.651-.696), N.L given (the same products in the
 * same order as tnk_spec's own N.L: nx lx + ny ly + nz lz); ok the lanes the light reaches (all, or a point light's range mask) */
static __inline void tnk_spec1(const tnk_frame *f,__m128 ok,__m128 ndl,__m128 att,__m128 lx,__m128 ly,__m128 lz,__m128 nx,__m128 ny,__m128 nz,
 __m128 vx,__m128 vy,__m128 vz,uint32_t s,float (*acc)[4]) { /* .697: the sums in memory (acc: b g r), not in registers (8 of them in 32-bit code) */
 const __m128 zero=_mm_setzero_ps(),one=LD(f->kone);__m128 hx,hy,hz,hl,ndh,pw;
 ok=_mm_and_ps(ok,_mm_cmpgt_ps(ndl,zero));
 if(!_mm_movemask_ps(ok))return; /* .654: no lane lit by this light: nothing to add */
 hx=_mm_add_ps(lx,vx);hy=_mm_add_ps(ly,vy);hz=_mm_add_ps(lz,vz);
 hl=_mm_add_ps(_mm_add_ps(_mm_mul_ps(hx,hx),_mm_mul_ps(hy,hy)),_mm_mul_ps(hz,hz)); /* .656: |H|^2; 1/|H| as rsqrt and one Newton step */
 ok=_mm_and_ps(ok,_mm_cmpgt_ps(hl,zero));
 {const __m128 r0=_mm_rsqrt_ps(hl);hl=_mm_sub_ps(r0,_mm_mul_ps(_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(_mm_mul_ps(hl,r0),r0),one),LD(f->khalf)),r0));}
 ndh=_mm_mul_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,hx),_mm_mul_ps(ny,hy)),_mm_mul_ps(nz,hz)),hl);
 ok=_mm_and_ps(ok,_mm_cmpgt_ps(ndh,LD(f->spcut))); /* .697: spcut (0 or more: tn_setup), was 0 */
 if(!_mm_movemask_ps(ok))return; /* .654: the same past the half vector */
 pw=_mm_and_ps(_mm_mul_ps(att,tnk_pow(f,_mm_and_ps(ndh,ok),f->spow,f->spmode)),ok);
 ST(acc[0],_mm_add_ps(LD(acc[0]),_mm_mul_ps(pw,LD(f->sdc[s][0]))));ST(acc[1],_mm_add_ps(LD(acc[1]),_mm_mul_ps(pw,LD(f->sdc[s][1]))));ST(acc[2],_mm_add_ps(LD(acc[2]),_mm_mul_ps(pw,LD(f->sdc[s][2]))));
}
/* the lanes' unit way to the eye (LOCALVIEWER), else (0, 0, -1). .697: 1/|e| as the reciprocal square root estimate and one Newton step, as
 * the way to a light (.648), not a square root and three divides (~18 cycles each on the C7, every group): within the last bits of the
 * one-vertex code's -e / |e| */
static __inline void tnk_view(const tnk_frame *f,__m128 ex,__m128 ey,__m128 ez,__m128 *vx,__m128 *vy,__m128 *vz) {
 const __m128 zero=_mm_setzero_ps(),one=LD(f->kone);
 *vx=zero;*vy=zero;*vz=_mm_sub_ps(zero,one);
 if(f->lview){const __m128 l2=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ex,ex),_mm_mul_ps(ey,ey)),_mm_mul_ps(ez,ez)),m=_mm_cmpgt_ps(l2,zero),r0=_mm_rsqrt_ps(l2);
  const __m128 r=_mm_sub_ps(zero,_mm_sub_ps(r0,_mm_mul_ps(_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(_mm_mul_ps(l2,r0),r0),one),LD(f->khalf)),r0))); /* -1/|e| */
  *vx=_mm_or_ps(_mm_and_ps(m,_mm_mul_ps(ex,r)),_mm_andnot_ps(m,*vx));*vy=_mm_or_ps(_mm_and_ps(m,_mm_mul_ps(ey,r)),_mm_andnot_ps(m,*vy));
  *vz=_mm_or_ps(_mm_and_ps(m,_mm_mul_ps(ez,r)),_mm_andnot_ps(m,*vz));}
}
static __inline void tnk_plight_g(tnk_frame *f,const int cones) { /* .653: cones 1: tnk_splight (spot lights too; .697: and the specular sums) */
 const __m128 zero=LD(f->vp[6]), /* .653: 0 from the frame (vp[6]), as tnk_spec: clang made max(N.L, 0) of the select, with its 0 in memory */one=LD(f->kone),x=LD(f->gx),y=LD(f->gy),z=LD(f->gz);
 __m128 ex,ey,ez,nx=zero,ny=zero,nz=zero,cb=LD(f->base[0]),cg=LD(f->base[1]),cr=LD(f->base[2]),vx,vy,vz;uint32_t k;
 const int sp=cones&&f->spec&&f->haveN; /* .697: the specular sums here (tnk_spec's lights need the normal: f->spec is only set with one) */
 ex=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[0])),_mm_mul_ps(y,LD(f->e[4]))),_mm_mul_ps(z,LD(f->e[8]))),LD(f->e[12]));
 ey=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[1])),_mm_mul_ps(y,LD(f->e[5]))),_mm_mul_ps(z,LD(f->e[9]))),LD(f->e[13]));
 ez=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[2])),_mm_mul_ps(y,LD(f->e[6]))),_mm_mul_ps(z,LD(f->e[10]))),LD(f->e[14]));
 if(f->haveN){
  __m128 ux=LD(f->gnx),uy=LD(f->gny),uz=LD(f->gnz);
  nx=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[0])),_mm_mul_ps(uy,LD(f->nm[3]))),_mm_mul_ps(uz,LD(f->nm[6])));
  ny=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[1])),_mm_mul_ps(uy,LD(f->nm[4]))),_mm_mul_ps(uz,LD(f->nm[7])));
  nz=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[2])),_mm_mul_ps(uy,LD(f->nm[5]))),_mm_mul_ps(uz,LD(f->nm[8])));
  if(f->nnorm){__m128 l=_mm_sqrt_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,nx),_mm_mul_ps(ny,ny)),_mm_mul_ps(nz,nz))),m=_mm_cmpgt_ps(l,zero),r;
   r=_mm_and_ps(_mm_div_ps(one,l),m);nx=_mm_or_ps(_mm_and_ps(_mm_mul_ps(nx,r),m),_mm_andnot_ps(m,nx));ny=_mm_or_ps(_mm_and_ps(_mm_mul_ps(ny,r),m),_mm_andnot_ps(m,ny));
   nz=_mm_or_ps(_mm_and_ps(_mm_mul_ps(nz,r),m),_mm_andnot_ps(m,nz));}
 }
 if(sp){const __m128 all=_mm_cmpeq_ps(one,one); /* .697: the directional lights' specular terms first, as tnk_spec */
  ST(f->sacc[0],zero);ST(f->sacc[1],zero);ST(f->sacc[2],zero);
  tnk_view(f,ex,ey,ez,&vx,&vy,&vz);
  for(k=0;k<f->nl;k++){const __m128 lx=LD(f->ld[k][0]),ly=LD(f->ld[k][1]),lz=LD(f->ld[k][2]);
   tnk_spec1(f,all,_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,lx),_mm_mul_ps(ny,ly)),_mm_mul_ps(nz,lz)),one,lx,ly,lz,nx,ny,nz,vx,vy,vz,k,f->sacc);}}
 else vx=vy=vz=zero;
 for(k=0;k<f->npl;k++){
  __m128 lx=_mm_sub_ps(LD(f->pp[k][0]),ex),ly=_mm_sub_ps(LD(f->pp[k][1]),ey),lz=_mm_sub_ps(LD(f->pp[k][2]),ez),d,m,r,q,qm,att,okr;
  /* .648 (board, VXTLP 6b on .647: a point light 38.6 ms a frame against the runtime's 36.1): no square root or divide. The reciprocal
   * square root estimate (12 bits) and one Newton step, r = r0 - r0 (d2 r0 r0 - 1) / 2; then d = d2 r. A vertex on the light (d2 0) keeps
   * its vector and a distance of 0, as before */
  {const __m128 d2=_mm_add_ps(_mm_add_ps(_mm_mul_ps(lx,lx),_mm_mul_ps(ly,ly)),_mm_mul_ps(lz,lz)),r0=_mm_rsqrt_ps(d2);
   r=_mm_sub_ps(r0,_mm_mul_ps(_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(_mm_mul_ps(d2,r0),r0),one),LD(f->khalf)),r0));
   m=_mm_cmplt_ps(zero,d2);d=_mm_and_ps(_mm_mul_ps(d2,r),m);}
  okr=_mm_cmple_ps(d,LD(f->pr[k][0]));
  if(cones&&!_mm_movemask_ps(okr))continue; /* .697 (tnk_splight; the hand-written tnk_plight does not skip): no lane inside the light's range: it adds nothing (the attenuation is masked to 0 below) */
  if(cones){r=_mm_and_ps(r,m);lx=_mm_mul_ps(lx,r);ly=_mm_mul_ps(ly,r);lz=_mm_mul_ps(lz,r);} /* .697 (tnk_splight): a vertex on the light (d2 0: its
   * way is 0 0 0, or so small its square is 0) gets the way 0 0 0, whose N.L and cone add nothing, as its kept way did; 4 operations, not 12 */
  else{
  lx=_mm_xor_ps(lx,_mm_and_ps(_mm_xor_ps(_mm_mul_ps(lx,r),lx),m));
  ly=_mm_xor_ps(ly,_mm_and_ps(_mm_xor_ps(_mm_mul_ps(ly,r),ly),m));
  lz=_mm_xor_ps(lz,_mm_and_ps(_mm_xor_ps(_mm_mul_ps(lz,r),lz),m));}
  q=_mm_add_ps(_mm_add_ps(_mm_mul_ps(LD(f->pr[k][2]),d),LD(f->pr[k][1])),_mm_mul_ps(_mm_mul_ps(d,LD(f->pr[k][3])),d));
  qm=_mm_cmpgt_ps(q,zero);
  {const __m128 y0=_mm_rcp_ps(q); /* .648: 1/q as the estimate and one Newton step, y0 - y0 (q y0 - 1) */
   att=_mm_xor_ps(_mm_and_ps(_mm_xor_ps(_mm_sub_ps(y0,_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(q,y0),one),y0)),one),qm),one);}
  att=_mm_and_ps(att,okr);
  if(cones&&f->pspot[k])att=_mm_mul_ps(att,tnk_cone(f,k,lx,ly,lz)); /* .653 */
  if(!cones||(f->pamb&(1UL<<k))){cb=_mm_add_ps(cb,_mm_mul_ps(att,LD(f->pac[k][0])));cg=_mm_add_ps(cg,_mm_mul_ps(att,LD(f->pac[k][1])));cr=_mm_add_ps(cr,_mm_mul_ps(att,LD(f->pac[k][2])));} /* .697: an ambient product of 0 adds 0 (tnk_splight; the hand-written tnk_plight adds it) */
  if(f->haveN){__m128 ndl=_mm_add_ps(_mm_add_ps(_mm_mul_ps(lx,nx),_mm_mul_ps(ly,ny)),_mm_mul_ps(lz,nz));
   if(sp)tnk_spec1(f,okr,ndl,att,lx,ly,lz,nx,ny,nz,vx,vy,vz,8+k,f->sacc); /* .697: this light's specular term now (tnk_spec's N.L: the same products) */
   ndl=_mm_and_ps(ndl,_mm_cmpgt_ps(ndl,zero));ndl=_mm_mul_ps(ndl,att);
   cb=_mm_add_ps(cb,_mm_mul_ps(ndl,LD(f->pdc[k][0])));cg=_mm_add_ps(cg,_mm_mul_ps(ndl,LD(f->pdc[k][1])));cr=_mm_add_ps(cr,_mm_mul_ps(ndl,LD(f->pdc[k][2])));}
 }
 ST(f->base[0],cb);ST(f->base[1],cg);ST(f->base[2],cr);
 f->sdone=sp;
}
/* .651 (board, 3DMark 2001's High Polygon Count on .650: a frame of 25 s, the test given as N/A; host: eight lights with specular
 * lighting 5 times the time without, every vertex through the one-vertex code and its double-precision power): the specular word for
 * four lanes, after tnk_light has written the fog in its alpha. For each directional and point light, as tn_light_color: only where
 * N.L > 0 (and inside a point light's range), H = L + V (V to the eye from the vertex with LOCALVIEWER, else (0, 0, -1)), N.H / |H| above
 * 0 raised to the material's power, times the attenuation, times the light's specular product. The power x^p = e^(p ln x): ln x from the
 * float's exponent and 2 atanh((m - 1) / (m + 1)) for the mantissa in [0.707, 1.414] (to z^9: 1e-9), e^y by tnk_exp; x >= 1 gives 1, as
 * tn_pow. Compiled for the board by clang from this source (tools/tnl_asm.py), so the same instructions run on both. */
static __m128 tnk_pow(const tnk_frame *f,__m128 x,const float *pw,uint32_t mode) { /* .653: the power (splat) and its case as arguments (the specular power, a spot light's falloff) */
 const __m128 one=LD(f->kone);
 __m128i b,ei;__m128 m,big,ex,z,z2,lnm,r,lx;
 if(mode==1)return _mm_min_ps(x,one);
 if(mode==2)return one;
 if(mode==3){uint32_t n=f->spint;__m128 r,b=_mm_min_ps(x,one);if(!n)n=1; /* .659: an integer power (2-255) by squaring (x in 0..1: no overflow, and 1
   * stays 1); r starts at the lowest set bit's square, with no 1.0 for clang to put in memory */
  while(!(n&1)){b=_mm_mul_ps(b,b);n>>=1;}
  r=b;n>>=1;
  while(n){b=_mm_mul_ps(b,b);r=_mm_mul_ps(r,(n&1)?b:one);n>>=1;} /* one from the frame: clang made the select's 1.0 a constant in memory (.697: and makes a
   * branch around the multiply a select with 1.0 in memory too, so the multiply by 1 for a clear bit stays) */
  return r;}
 x=_mm_min_ps(x,one);
 b=_mm_castps_si128(x);
 ei=_mm_sub_epi32(_mm_srli_epi32(b,23),_mm_loadu_si128((const __m128i*)f->k127)); /* x > 0, normal: its exponent */
 m=_mm_castsi128_ps(_mm_or_si128(_mm_and_si128(b,_mm_loadu_si128((const __m128i*)f->kmant)),_mm_castps_si128(one))); /* [1, 2) */
 big=_mm_cmpgt_ps(m,LD(f->ksqrt2));
 m=_mm_or_ps(_mm_and_ps(big,_mm_mul_ps(m,LD(f->khalf))),_mm_andnot_ps(big,m));
 ex=_mm_add_ps(_mm_cvtepi32_ps(ei),_mm_and_ps(big,one));
 {const __m128 dn=_mm_add_ps(m,one),y0=_mm_rcp_ps(dn); /* .656 (board, 3DMark's High Polygon Count, 8 lights): 1/(m + 1) as the estimate and
   one Newton step (m + 1 in [1.7, 2.5], never 0), not the divide (a divide costs ~ 18 cycles on the board's C7, and this runs per light) */
  z=_mm_mul_ps(_mm_sub_ps(m,one),_mm_sub_ps(_mm_add_ps(y0,y0),_mm_mul_ps(_mm_mul_ps(dn,y0),y0)));}z2=_mm_mul_ps(z,z);
 r=_mm_add_ps(_mm_mul_ps(LD(f->klog[3]),z2),LD(f->klog[2]));r=_mm_add_ps(_mm_mul_ps(r,z2),LD(f->klog[1]));r=_mm_add_ps(_mm_mul_ps(r,z2),LD(f->klog[0]));
 r=_mm_add_ps(_mm_mul_ps(r,z2),one);
 lnm=_mm_mul_ps(_mm_add_ps(z,z),r);
 lx=_mm_add_ps(_mm_mul_ps(ex,LD(f->kexpc[1])),lnm); /* ln x = e ln 2 + ln m (kexpc[1] is ln 2) */
 r=tnk_exp(f,_mm_mul_ps(LD(pw),lx));
 return _mm_or_ps(_mm_and_ps(_mm_cmpge_ps(x,one),one),_mm_andnot_ps(_mm_cmpge_ps(x,one),r));
}
/* .653 (board, 3DMark 2001 on .652: 22,843 draws one vertex at a time for a spot light): point light q's cone for four lanes, L the unit
 * way from the vertex to the light (as tn_light_color: rho = -(L . direction); 0 where rho <= cos(phi/2), 1 where rho >= cos(theta/2),
 * between them ((rho - cos(phi/2)) / (cos(theta/2) - cos(phi/2))) ^ falloff), the factor the attenuation is multiplied by */
static __m128 tnk_cone(const tnk_frame *f,uint32_t q,__m128 lx,__m128 ly,__m128 lz) {
 const __m128 one=LD(f->kone),scp=LD(f->pcone[q][0]),sct=LD(f->pcone[q][1]);
 const __m128 rho=_mm_sub_ps(_mm_setzero_ps(),_mm_add_ps(_mm_add_ps(_mm_mul_ps(lx,LD(f->psd[q][0])),_mm_mul_ps(ly,LD(f->psd[q][1]))),_mm_mul_ps(lz,LD(f->psd[q][2]))));
 const __m128 in=_mm_cmpgt_ps(rho,scp),part=_mm_cmplt_ps(rho,sct);
 const __m128 p=tnk_pow(f,_mm_div_ps(_mm_sub_ps(rho,scp),_mm_sub_ps(sct,scp)),f->pcone[q][2],f->pmode[q]);
 return _mm_and_ps(in,_mm_or_ps(_mm_and_ps(part,p),_mm_andnot_ps(part,one)));
}
static void tnk_plight(tnk_frame *f){tnk_plight_g(f,0);} /* the board: tools/tnl_asm.py's hand-written _tnk_plight (no spot lights) */
static void tnk_splight(tnk_frame *f){tnk_plight_g(f,1);} /* .653: with spot lights (the board: compiled from this C by clang) */
static void tnk_spec(tnk_frame *f) { /* .697: the directional lights only (with point lights, tnk_splight made the sums: sdone), then the packing */
 __m128 cb,cg,cr;uint32_t k;
 if(f->sdone){cb=LD(f->sacc[0]);cg=LD(f->sacc[1]);cr=LD(f->sacc[2]);}
 else{const __m128 zero=_mm_setzero_ps(),one=LD(f->kone),x=LD(f->gx),y=LD(f->gy),z=LD(f->gz),all=_mm_cmpeq_ps(one,one);
  __m128 ex,ey,ez,nx,ny,nz,vx,vy,vz;const __m128 ux=LD(f->gnx),uy=LD(f->gny),uz=LD(f->gnz);
  ST(f->sacc[0],zero);ST(f->sacc[1],zero);ST(f->sacc[2],zero);
  ex=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[0])),_mm_mul_ps(y,LD(f->e[4]))),_mm_mul_ps(z,LD(f->e[8]))),LD(f->e[12]));
  ey=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[1])),_mm_mul_ps(y,LD(f->e[5]))),_mm_mul_ps(z,LD(f->e[9]))),LD(f->e[13]));
  ez=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[2])),_mm_mul_ps(y,LD(f->e[6]))),_mm_mul_ps(z,LD(f->e[10]))),LD(f->e[14]));
  nx=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[0])),_mm_mul_ps(uy,LD(f->nm[3]))),_mm_mul_ps(uz,LD(f->nm[6])));
  ny=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[1])),_mm_mul_ps(uy,LD(f->nm[4]))),_mm_mul_ps(uz,LD(f->nm[7])));
  nz=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[2])),_mm_mul_ps(uy,LD(f->nm[5]))),_mm_mul_ps(uz,LD(f->nm[8])));
  if(f->nnorm){__m128 l=_mm_sqrt_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,nx),_mm_mul_ps(ny,ny)),_mm_mul_ps(nz,nz))),m=_mm_cmpgt_ps(l,zero),r;
   r=_mm_and_ps(_mm_div_ps(one,l),m);nx=_mm_or_ps(_mm_and_ps(_mm_mul_ps(nx,r),m),_mm_andnot_ps(m,nx));ny=_mm_or_ps(_mm_and_ps(_mm_mul_ps(ny,r),m),_mm_andnot_ps(m,ny));
   nz=_mm_or_ps(_mm_and_ps(_mm_mul_ps(nz,r),m),_mm_andnot_ps(m,nz));}
  tnk_view(f,ex,ey,ez,&vx,&vy,&vz);
  for(k=0;k<f->nl;k++){const __m128 lx=LD(f->ld[k][0]),ly=LD(f->ld[k][1]),lz=LD(f->ld[k][2]);
   tnk_spec1(f,all,_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,lx),_mm_mul_ps(ny,ly)),_mm_mul_ps(nz,lz)),one,lx,ly,lz,nx,ny,nz,vx,vy,vz,k,f->sacc);}
  cb=LD(f->sacc[0]);cg=LD(f->sacc[1]);cr=LD(f->sacc[2]);}
 {const __m128 k255=LD(f->k255);const __m128i z0=_mm_castps_si128(LD(f->vp[6])); /* 0 from the frame (vp[6]): a literal 0 here became a constant in memory, which the board's bytes cannot carry */
  __m128i b=_mm_cvtps_epi32(_mm_mul_ps(cb,k255)),g=_mm_cvtps_epi32(_mm_mul_ps(cg,k255)),r=_mm_cvtps_epi32(_mm_mul_ps(cr,k255)),w;
  b=_mm_packus_epi16(_mm_packs_epi32(b,b),z0);g=_mm_packus_epi16(_mm_packs_epi32(g,g),z0);r=_mm_packus_epi16(_mm_packs_epi32(r,r),z0);
  w=_mm_unpacklo_epi8(_mm_unpacklo_epi8(b,r),_mm_unpacklo_epi8(g,z0)); /* b g r 0 a lane */
  _mm_storeu_si128((__m128i*)f->os,_mm_or_si128(_mm_andnot_si128(_mm_loadu_si128((const __m128i*)f->kmask24),_mm_loadu_si128((const __m128i*)f->os)),w));}
}
/* .652 (board, VXTLP 6e on .651: the groups of four 40% of the driver's T&L, 28.6 ms a frame; their lanes' colour products came from
 * scalar code, every vertex, when the colour comes from the vertices, Direct3D 8's default): a group's material colours and every light's
 * products from its four colour words at once, the same operations in the same order as tn_light_color's: a byte to 0..1 as k / 255
 * (the one-vertex code's table holds the same quotients), base = emissive + ambient x material ambient, each directional light's
 * ambient x material ambient added in order; each light's diffuse x material diffuse into its lane (lc, or pdc and pac for a point
 * light), its specular x material specular (sdc). Writes tnk.base and the lanes' base without the point lights (cvb). */
static void tnk_cv(tnk_frame *f,float (*cvb)[4]) {
 const __m128 k255=LD(f->k255);const __m128i ff=_mm_loadu_si128((const __m128i*)f->kff);
 const __m128i dw=_mm_loadu_si128((const __m128i*)f->gdc),sw=_mm_loadu_si128((const __m128i*)f->gsc);
 __m128 vd[4],vs[4],m[4][4];uint32_t c,k,di=0,pi=0;
 /* .678: only the words a material source reads are turned into 0..1 (each is 4 divps, ~18 cycles apiece on the C7; cvconv, worked out
  * once a set-up: 1 the diffuse word's b g r, 2 its alpha, 4 the specular word's b g r, 8 its alpha). Direct3D's defaults (diffuse from
  * colour 1, the rest from the material) and a vertex without a specular word convert the diffuse word alone. The same quotients as
  * before where they are read; the words not converted are never read (alpha only through the diffuse source, base[3]). */
 const uint32_t cv=f->cvconv,ss=f->spec?f->ssrc:0;
 /* .694: a byte to 0..1 as k x (1/255) (a multiply; the divide took ~18 cycles on the C7, 4 to 8 a group): within one unit in the last place
  * of k / 255, the one-vertex table (tn_c255) holds the same products, so both paths still agree word for word */
 const __m128 kr=_mm_set1_ps(f->kr255);(void)k255;
 #define TNK_B2F(w,sh) _mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128((sh)?_mm_srli_epi32(w,sh):(w),ff)),kr)
 if(cv&1){vd[0]=TNK_B2F(dw,0);vd[1]=TNK_B2F(dw,8);vd[2]=TNK_B2F(dw,16);vd[3]=(cv&2)?TNK_B2F(dw,24):_mm_setzero_ps();}
 else vd[0]=vd[1]=vd[2]=vd[3]=_mm_setzero_ps();
 if(cv&4){vs[0]=TNK_B2F(sw,0);vs[1]=TNK_B2F(sw,8);vs[2]=TNK_B2F(sw,16);vs[3]=(cv&8)?TNK_B2F(sw,24):_mm_setzero_ps();}
 else vs[0]=vs[1]=vs[2]=vs[3]=_mm_setzero_ps();
 #undef TNK_B2F
 /* .697: the products whose inputs are all the material's are made by the set-up's first group (cvfirst) and kept; later groups make only
  * those with a source from the vertices (cvvary: 1 diffuse, 2 ambient, 4 emissive, 8 specular). The same products, in the same order for
  * the base's sum (emissive + global ambient x ambient, then each directional light's ambient x ambient in light order); a material row is
  * made only when something reads it, and each light's three channels together */
 {const uint32_t all=f->cvfirst,vd1=all|(f->cvvary&1),va=all|(f->cvvary&2),vb=all|(f->cvvary&6),vs1=f->spec&&(all|(f->cvvary&8));
  #define TNK_ROW(r,src,q) {if((src)==1){r[0]=vd[0];r[1]=vd[1];r[2]=vd[2];r[3]=vd[3];}else if((src)==2){r[0]=vs[0];r[1]=vs[1];r[2]=vs[2];r[3]=vs[3];}else{r[0]=LD(f->cvm[q][0]);r[1]=LD(f->cvm[q][1]);r[2]=LD(f->cvm[q][2]);r[3]=LD(f->cvm[q][3]);}}
  TNK_ROW(m[0],f->dsrc,0) /* the diffuse row: its alpha is the base's alpha every group */
  if(va||vb){TNK_ROW(m[1],f->asrc,1)}
  if(vb){TNK_ROW(m[2],f->esrc,2)}
  if(vs1){TNK_ROW(m[3],ss,3)}
  #undef TNK_ROW
  if(vb)for(c=0;c<3;c++){__m128 b=_mm_add_ps(m[2][c],_mm_mul_ps(LD(f->cvamb[c]),m[1][c]));
   for(k=0;k<f->cvn;k++)if(f->cvdir[k])b=_mm_add_ps(b,_mm_mul_ps(LD(f->cvl[k][1][c]),m[1][c]));
   ST(cvb[c],b);}
  for(c=0;c<3;c++)ST(f->base[c],LD(cvb[c]));
  for(k=0;k<f->cvn;k++){
   if(f->cvdir[k]){
    if(vs1){ST(f->sdc[di][0],_mm_mul_ps(LD(f->cvl[k][2][0]),m[3][0]));ST(f->sdc[di][1],_mm_mul_ps(LD(f->cvl[k][2][1]),m[3][1]));ST(f->sdc[di][2],_mm_mul_ps(LD(f->cvl[k][2][2]),m[3][2]));}
    if(vd1){ST(f->lc[di][0],_mm_mul_ps(LD(f->cvl[k][0][0]),m[0][0]));ST(f->lc[di][1],_mm_mul_ps(LD(f->cvl[k][0][1]),m[0][1]));ST(f->lc[di][2],_mm_mul_ps(LD(f->cvl[k][0][2]),m[0][2]));}
    di++;}
   else{
    if(va&&(f->pamb&(1UL<<pi))){ST(f->pac[pi][0],_mm_mul_ps(LD(f->cvl[k][1][0]),m[1][0]));ST(f->pac[pi][1],_mm_mul_ps(LD(f->cvl[k][1][1]),m[1][1]));ST(f->pac[pi][2],_mm_mul_ps(LD(f->cvl[k][1][2]),m[1][2]));} /* an ambient of 0: the set-up's 0 stays */
    if(vs1){ST(f->sdc[8+pi][0],_mm_mul_ps(LD(f->cvl[k][2][0]),m[3][0]));ST(f->sdc[8+pi][1],_mm_mul_ps(LD(f->cvl[k][2][1]),m[3][1]));ST(f->sdc[8+pi][2],_mm_mul_ps(LD(f->cvl[k][2][2]),m[3][2]));}
    if(vd1){ST(f->pdc[pi][0],_mm_mul_ps(LD(f->cvl[k][0][0]),m[0][0]));ST(f->pdc[pi][1],_mm_mul_ps(LD(f->cvl[k][0][1]),m[0][1]));ST(f->pdc[pi][2],_mm_mul_ps(LD(f->cvl[k][0][2]),m[0][2]));}
    pi++;}}
  ST(f->base[3],m[0][3]);ST(cvb[3],m[0][3]);
  f->cvfirst=0;}
}
#undef LD
#undef LDI
#undef ST
/* TNK KERNELS END 2 */
#else
/* the board: src/vx_tnl_sse.asm (wasm), the same sequences; cdecl, one argument */
extern void __cdecl tnk_pos(tnk_frame *f);
extern void __cdecl tnk_light(tnk_frame *f);
extern void __cdecl tnk_plight(tnk_frame *f); /* .647 */
extern void __cdecl tnk_spec(tnk_frame *f); /* .651: compiled from the C above by clang (tools/tnl_asm.py) */
extern void __cdecl tnk_cv(tnk_frame *f,float (*cvb)[4]); /* .652 */
extern void __cdecl tnk_splight(tnk_frame *f); /* .653 */
extern void __cdecl tnk_vcodes(tnk_frame *f); /* .702: compiled from the C above by clang */
#endif

#define tn_lbase (tnk.lbase) /* .654: in the frame (16-byte aligned) */
/* .652 (board, 3DMark 2001's High Polygon Count on .651: still a frame of about 30 s, 15.9 million vertices one at a time, cause unseen):
 * why a draw's vertices are finished one at a time (tn_soa_why, set by tn_soa_prepare's full build and kept with the set-up), counted
 * by draw and by vertex in tn_draw (vx_d3d_tn_why, QueryPhases 403-412): [0] the fog mode, [1] specular lighting without normals,
 * [2] more than 8 directional lights, [3] a spot light, [4] more than 8 point lights, [5] a generated or transformed coordinate set,
 * [6] a coordinate set the vertex lacks, [7] the group path switched off; [8] draws, [9] their vertices. .653: [3], [5] and [6] are no
 * longer causes (spot lights and every coordinate set go in groups); the words stay so the monitor's layout does not move */
static uint32_t tn_soa_why,tn_soa_tex;uint32_t vx_d3d_tn_why[10]; /* .647: the lanes' base colour without the point lights (tnk_plight adds them into tnk.base every group) */
uint32_t vx_d3d_tn_soa_on=1;static int tn_soa_kept_ok;static uint32_t tn_soa_kept_on; /* .647: the frame kept with the set-up (tn_same) */ /* .643: 0 in host tests that compare the group path with the one-vertex code */
/* the frame's constants for the draw (after tn_prepare filled tn_s); 1 when the group path can finish this draw's vertices */
static int tn_soa_prepare_b(void);
static int tn_soa_prepare(void) {uint32_t o=TNP(0);int r=tn_soa_prepare_b();TNP_BACK(o);return r;}
static int tn_soa_prepare_b(void) {
 const tn_setup *s=&tn_s;uint32_t k,q;int ok=vx_d3d_tn_soa_on!=0;
 static const float expc[7]={1.0f,0.6931471805599453f,0.2402265069591007f,0.0555041086648216f,0.0096181291076285f,0.0013333558146428f,0.0001540353039338f};
 if(tn_same&&tn_soa_kept_valid&&tn_soa_kept_on==vx_d3d_tn_soa_on){ /* .647: the set-up kept: only the matrices again when the world matrix changed */
  if(tn_same==1||tn_soa_stale){tn_soa_stale=0;
   for(k=0;k<16;k++)tnk_splat(tnk.m[k],s->wvp[k]);
   tnk.eye_z_ready=s->fog_clip_w;
   if(tnk.npl||tnk.spec)for(k=0;k<16;k++)tnk_splat(tnk.e[k],s->wv[k]);
   else if(s->fog&&!tnk.eye_z_ready){const uint32_t step=s->rfog?1:4;for(k=s->rfog?0:2;k<16;k+=step)tnk_splat(tnk.e[k],s->wv[k]);}
   if(s->lit&&tnk.haveN)for(k=0;k<9;k++)tnk_splat(tnk.nm[k],s->nrm[k]);}
  return tn_soa_kept_ok;}
 tn_soa_kept_valid=0;tn_soa_stale=0;
 tn_soa_why=ok?0:128; /* .652: why the draw is finished one vertex at a time (vx_d3d_tn_why; kept with the set-up) */
 if(!tnkp){
  tnkp=(tnk_frame*)(void*)(tnk_mem+((16-((size_t)tnk_mem&15))&15));
  memset(tnkp,0,sizeof(*tnkp));
  tnk_splat(tnk.kone,1.0f);tnk_splat(tnk.k255,255.0f);tnk.kr255=1.0f/255.0f;tnk_splat(tnk.khalf,0.5f);tnk_splat(tnk.kl2e,1.4426950408889634f);tnk_splat(tnk.kneg80,-80.0f);
  for(k=0;k<7;k++)tnk_splat(tnk.kexpc[k],expc[k]);{vx_fu u;u.n=0x7F800000UL;tnk_splat(tnk.kinf,u.f);u.n=0x80000000UL;tnk_splat(tnk.ksign,u.f);}
  {static const float kl[4]={1.0f/3,1.0f/5,1.0f/7,1.0f/9};for(k=0;k<4;k++)tnk_splat(tnk.klog[k],kl[k]);tnk_splat(tnk.ksqrt2,1.4142135623730951f);} /* .651 */
  for(k=0;k<4;k++){tnk.kff[k]=0xFFUL;tnk.kmant[k]=0x007FFFFFUL;tnk.k127[k]=127;tnk.kmask24[k]=0x00FFFFFFUL;tnk.k7f[k]=0x7F;tnk.kvc[0][k]=1;tnk.kvc[1][k]=2;tnk.kvc[2][k]=4;tnk.kvc[3][k]=8;tnk.kb[0][k]=1;tnk.kb[1][k]=2;tnk.kb[2][k]=8;tnk.kb[3][k]=16;tnk.kb[4][k]=32;tnk.kb[5][k]=64;}
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
  if(s->fmode!=1&&s->fmode!=2&&s->fmode!=3){ok=0;tn_soa_why|=1;}
  if(s->fmode==3&&!(s->fe>s->fs)){ok=0;tn_soa_why|=1;}} /* linear fog with fe <= fs is 1 in the one-vertex code: left to it (rare) */
 tnk.nl=0;tnk.npl=0;tnk.nspot=0;tnk.pamb=0;tnk.sdone=0;
 /* .651: specular lighting in groups too (tnk_spec; .645-.650 sent every such vertex through the one-vertex code) */
 tnk.spec=s->spec_on&&tnk.haveN;
 if(tnk.spec){tnk_splat(tnk.spow,s->mpow);tnk_splat(tnk.spcut,s->spcut);tnk.spmode=s->mpow==1.0f?1U:!(s->mpow>0.0f)?2U:0U;tnk.lview=s->localview;
  if(!tnk.spmode&&s->mpow>=2.0f&&s->mpow<=255.0f&&(float)(int)s->mpow==s->mpow){tnk.spmode=3;tnk.spint=(uint32_t)(int)s->mpow;}} /* .659: an integer power by squaring */
 else if(s->spec_on){ok=0;tn_soa_why|=2;}
 if(s->lit){
  if(tnk.haveN)for(k=0;k<9;k++)tnk_splat(tnk.nm[k],s->nrm[k]);
  /* .642: a material colour from the vertices no longer leaves the group path: tn_soa_finish fills the base and the lights' colours lane by lane */
  /* .647 (board, VXTLP WHY on .640: one point light 1.6 times the runtime's time, every vertex through the one-vertex code): point lights
   * go in groups too (tnk_plight); .651 specular (tnk_spec); .653 spot lights (tnk_splight) */
  for(k=0;k<s->nl;k++){
   if(s->ldir[k]){
    if(tnk.nl<8){tnk_splat(tnk.ld[tnk.nl][0],s->ld[k][0]);tnk_splat(tnk.ld[tnk.nl][1],s->ld[k][1]);tnk_splat(tnk.ld[tnk.nl][2],s->ld[k][2]);
     for(q=0;q<3;q++)tnk_splat(tnk.lc[tnk.nl][q],s->ld4[k][q]);tnk_splat(tnk.lc[tnk.nl][3],0.0f);
     if(tnk.spec)for(q=0;q<3;q++)tnk_splat(tnk.sdc[tnk.nl][q],s->ts->light[s->li[k]].spe[2-q]*s->mspec[2-q]); /* .651 (b g r) */
     tnk.nl++;}
    else{ok=0;tn_soa_why|=4;}}
   else if(tnk.npl<8){const tnl_light *l=&s->ts->light[s->li[k]];const uint32_t p=tnk.npl;
    tnk.pspot[p]=(s->spotmask&(1UL<<k))!=0; /* .653: a spot light is a point light with its cone (tnk_splight) */
    if(tnk.pspot[p]){const float fa=tn_sfall[k];tnk.nspot++;for(q=0;q<3;q++)tnk_splat(tnk.psd[p][q],tn_sdir[k][q]);
     tnk_splat(tnk.pcone[p][0],tn_scp[k]);tnk_splat(tnk.pcone[p][1],tn_sct[k]);tnk_splat(tnk.pcone[p][2],fa);tnk.pmode[p]=fa==1.0f?1U:!(fa>0.0f)?2U:0U;}
    for(q=0;q<3;q++){tnk_splat(tnk.pp[p][q],s->lpos[k][q]);tnk_splat(tnk.pdc[p][q],s->ld4[k][q]);tnk_splat(tnk.pac[p][q],s->la4[k][q]);}
    if(l->amb[0]!=0.0f||l->amb[1]!=0.0f||l->amb[2]!=0.0f)tnk.pamb|=1UL<<p; /* .697 */
    tnk_splat(tnk.pr[p][0],l->range);tnk_splat(tnk.pr[p][1],l->a0);tnk_splat(tnk.pr[p][2],l->a1);tnk_splat(tnk.pr[p][3],l->a2);
    if(tnk.spec)for(q=0;q<3;q++)tnk_splat(tnk.sdc[8+p][q],l->spe[2-q]*s->mspec[2-q]); /* .651 */
    tnk.npl++;}
   else{ok=0;tn_soa_why|=16;}}
  for(q=0;q<4;q++){tnk_splat(tnk.base[q],s->base4[q]);tnk_splat(tn_lbase[q],s->base4[q]);}
  if(!s->mconst){const tnl_state *t=s->ts;const float *mm[4];uint32_t c; /* .652: tnk_cv's constants, b g r (a) */
   mm[0]=s->mdif;mm[1]=s->mamb;mm[2]=s->memi;mm[3]=s->mspec;
   tnk.dsrc=s->dsrc;tnk.asrc=s->asrc;tnk.esrc=s->esrc;tnk.ssrc=s->ssrc;{const uint32_t ss=tnk.spec?s->ssrc:0; /* .678: tnk_cv's cvconv (tnk.spec is set above) */
    tnk.cvconv=(s->dsrc==1||s->asrc==1||s->esrc==1||ss==1?1u:0u)|(s->dsrc==1?2u:0u)|(s->dsrc==2||s->asrc==2||s->esrc==2||ss==2?4u:0u)|(s->dsrc==2?8u:0u);
    tnk.cvvary=(s->dsrc?1u:0u)|(s->asrc?2u:0u)|(s->esrc?4u:0u)|(ss?8u:0u);tnk.cvfirst=1;} /* .697: the material-only products made once */
   for(q=0;q<4;q++)for(c=0;c<4;c++)tnk_splat(tnk.cvm[q][c],mm[q][c<3?2-c:3]);
   for(c=0;c<3;c++)tnk_splat(tnk.cvamb[c],s->amb[2-c]);
   tnk.cvn=s->nl>16?16:s->nl;
   for(k=0;k<tnk.cvn;k++){const tnl_light *lt=&t->light[s->li[k]];tnk.cvdir[k]=s->ldir[k]!=0;
    for(c=0;c<3;c++){tnk_splat(tnk.cvl[k][0][c],lt->dif[2-c]);tnk_splat(tnk.cvl[k][1][c],lt->amb[2-c]);tnk_splat(tnk.cvl[k][2][c],lt->spe[2-c]);}}}
  if(tnk.npl||tnk.spec)for(k=0;k<16;k++)tnk_splat(tnk.e[k],s->wv[k]);}
 /* .653 (board, 3DMark 2001 on .652: 21,071 draws one vertex at a time for a generated or transformed set, 23,709 for a set the vertex
  * lacks): both in the group path now, the sets written by the one-vertex code's tn_texsets (generated, transformed) or 0 (lacking) */
 tn_soa_tex=tn_r3!=0;for(k=0;k<tn_sets;k++)if(s->gen[k]||(s->ttf[k]&0xFF))tn_soa_tex=1; /* a set read straight from the vertex, or none (.663: an r set: tn_texsets writes it) */
 tnk.stride=tn_ostride;tnk.rec=tn_buf;tnk.sx=tn_sx;tnk.sy=tn_sy;tnk.sz=tn_sz;tnk.cx=tn_cx;tnk.cy=tn_cy;tnk.cz=tn_cz;tnk.cw=tn_cw;tnk.rw=0;tnk.cc=tn_cc;
 tn_soa_kept_ok=ok;tn_soa_kept_valid=1;tn_soa_kept_on=vx_d3d_tn_soa_on;
 return ok;
}
/* phase T over places first .. first+n-1 (n <= TN_CH, staged in tn_sx tn_sy tn_sz lanes 0 .. n-1): the records' position words, tn_cc, the clip arrays */
static void tn_soa_pos(uint32_t first,uint32_t n) {
 uint32_t k,po=TNP(1);
 tnk.first=first;tnk.n=(n+3)&~3U;tnk.vc=tn_gb&&vx_d3d_gband==1?tn_vc:0; /* .702: 2, the band without the codes (ACCEL_D3D_GUARDBAND 2: does their rejection pay?) */ /* .700 */
 tnk_pos(tnkp);
 for(k=first;k<first+n;k++){tn_done[k]=0;if(tn_cc[k]==0x7F){memset(tn_buf+k*tn_ostride,0,tn_ostride);tn_done[k]=1;}} /* a position not finite: the record zeros, never sent */
 /* .698-.699: tn_vc, the viewport's own sides by place (1 left, 2 right, 4 top, 8 bottom), so a triangle wholly beyond one of them is
  * still dropped with the guard band; .700: made by tnk_pos with the positions (tnk.vc, set above), not a second loop here */
 TNP_BACK(po);
}
/* .702: after a draw's positions (places P0 .. P0+used-1): the viewport's codes only if tnk_pos saw a vertex past a side (tn_vany: the
 * drop tests read tn_vc only then); vacc cleared for the next draw */
static uint32_t tn_vany,tn_voff; /* .703: tn_voff, the draw's places past a side of the viewport (counted when tn_vany) */
static void tn_vdone(uint32_t P0,uint32_t used) {
 tn_vany=0;tn_voff=0;
 if(tnk.vc&&(tnk.vacc[0]|tnk.vacc[1]|tnk.vacc[2]|tnk.vacc[3])){uint32_t k;tnk.first=P0;tnk.n=(used+3)&~3U;tnk_vcodes(tnkp);tn_vany=1;vx_d3d_vcd[1]++;
  for(k=0;k<used;k++)tn_voff+=tn_vc[P0+k]!=0;}
 else if(tnk.vc)vx_d3d_vcd[0]++;
 tnk.vacc[0]=tnk.vacc[1]=tnk.vacc[2]=tnk.vacc[3]=0;
}
/* phase L: the places in list (n of them) finished by groups of four (a group's spare lanes repeat its last place) */
static void tn_soa_finish_b(const uint16_t *list,uint32_t n);
static void tn_soa_finish(const uint16_t *list,uint32_t n) {uint32_t o=TNP(2);tn_soa_finish_b(list,n);TNP_BACK(o);}
static void tn_soa_finish_b(const uint16_t *list,uint32_t n) {
 const tn_setup *s=&tn_s;
 uint32_t i,j,key[3];
 /* .642 (board, VXTLP WHY and GTA3's profile on .640/.641: with the diffuse, ambient or emissive colour taken from the vertices, Direct3D
  * 8's default, every vertex went through the one-vertex code, tn_attributes: 3 of the T&L's 11 ms a frame): the kernel reads its base
  * colour and each light's colour as four lanes, so a vertex's own products are written into its lane (the one-vertex code's sums, in its
  * order) and the kernel does the rest as for a constant material. */
 const int vmat=s->lit&&!s->mconst;
 /* .644: with the colour a material source, a draw whose vertices all carry one colour (and one normal) is lit once, as a constant
  * material's is (.643 lit every group: the grid scene's one colour had been one cached answer in the one-vertex code); and a lane
  * whose vertex has the colour its last vertex had kept its products (.644-.651; .652: tnk_cv makes a group's products at once) */
 int uniform=(tnk.haveN||vmat)&&n>=8&&!tnk.npl&&!tnk.spec;uint32_t kd=0,ks=0; /* .647: not with point lights (they depend on the vertex's place) */
#define TN_DC(v_) ((s->mat_sources&1)?(s->idif!=0xFFFFFFFFUL?u32((v_)+s->idif):0xFFFFFFFFUL):0)
#define TN_SC(v_) ((s->mat_sources&2)?(s->ispe!=0xFFFFFFFFUL?u32((v_)+s->ispe):0):0)
#define TN_SAME(v_) ((!tnk.haveN||(u32((v_)+s->inormal)==key[0]&&u32((v_)+s->inormal+4)==key[1]&&u32((v_)+s->inormal+8)==key[2]))&&(!vmat||(TN_DC(v_)==kd&&TN_SC(v_)==ks)))
 /* Try reuse only when the first eight normals agree. The first group
  * still runs the full SSE2 kernel and supplies the diffuse words. */
 if(uniform){
  const uint8_t *p=tn_sp[list[0]];
  key[0]=key[1]=key[2]=0;
  if(tnk.haveN){key[0]=u32(p+s->inormal);key[1]=u32(p+s->inormal+4);key[2]=u32(p+s->inormal+8);}
  if(vmat){kd=TN_DC(p);ks=TN_SC(p);}
  for(j=1;j<8;j++){
   p=tn_sp[list[j]];
   if(!TN_SAME(p)){uniform=0;break;}
  }
 }
 for(i=0;i<n;i+=4){
  uint32_t o4[4];
  if(uniform&&i>=8){
   for(j=0;j<4&&i+j<n;j++){
    const uint8_t *p=tn_sp[list[i+j]];
    if(!TN_SAME(p)){uniform=0;break;}
   }
  }
  tnk.reuse_diffuse=uniform&&i!=0;
  for(j=0;j<4;j++){uint32_t o=list[i+j<n?i+j:n-1];const uint8_t *v=tn_sp[o];o4[j]=o;
   if(tnk.eye_z_ready)tnk.gz[j]=tn_cw[o];
   else if(tnk.fogmode||tnk.npl||tnk.spec){tnk.gx[j]=tf(v);tnk.gy[j]=tf(v+4);tnk.gz[j]=tf(v+8);}
   if(tnk.haveN&&!tnk.reuse_diffuse){tnk.gnx[j]=tf(v+s->inormal);tnk.gny[j]=tf(v+s->inormal+4);tnk.gnz[j]=tf(v+s->inormal+8);}
   if(vmat&&!tnk.reuse_diffuse){tnk.gdc[j]=s->idif!=0xFFFFFFFFUL?u32(v+s->idif):0xFFFFFFFFUL;tnk.gsc[j]=s->ispe!=0xFFFFFFFFUL?u32(v+s->ispe):0;} /* .652: the colour words for tnk_cv */
   if(!tnk.lit){tnk.gdc[j]=s->idif!=0xFFFFFFFFUL?u32(v+s->idif):0xFFFFFFFFUL;tnk.gsc[j]=s->ispe!=0xFFFFFFFFUL?u32(v+s->ispe):0;
    if(!tnk.fogmode){tnk.od[j]=tnk.gdc[j];tnk.os[j]=(tnk.gsc[j]&0x00FFFFFFUL)|0xFF000000UL;}} /* .643: unlit, no fog: the kernel's answer, without the call */
  }
  {const uint32_t ko=TNP(7); /* .652: the kernels apart from the gather and the records (vx_d3d_tnp[7]) */
  if(vmat&&!tnk.reuse_diffuse)tnk_cv(tnkp,tn_lbase); /* .652: every lane's products at once (the scalar loop and its lane cache removed) */
  tnk.sdone=0; /* .697: set by tnk_splight when it made the specular sums */
  if(tnk.lit&&tnk.npl){memcpy(tnk.base,tn_lbase,sizeof(tn_lbase));if(tnk.nspot||tnk.spec)tnk_splight(tnkp);else tnk_plight(tnkp);vx_d3d_tn_plg++;} /* .659: with specular lighting the C kernel (.697: which makes the specular sums too) */ /* .647; vx_d3d_tn_plg: groups of four lit by point lights (QueryPhases 393) */
  if((!tnk.reuse_diffuse||tnk.fogmode)&&(tnk.lit||tnk.fogmode))tnk_light(tnkp);
  if(tnk.spec)tnk_spec(tnkp); /* .651 */
  TNP_BACK(ko);}
  for(j=0;j<4&&i+j<n;j++){const uint32_t o=o4[j];uint32_t *r=(uint32_t*)(void*)(tn_buf+o*tn_ostride),k;const uint8_t *v=tn_sp[o];
   r[4]=tnk.od[j];r[5]=tnk.os[j];
   if(tn_soa_tex){float e[4]={0,0,0,0},nn[4]={0,0,0,0};const int hn=s->inormal!=0xFFFFFFFFUL; /* .653: as tn_attributes */
    if(s->gen_eye)tn_x4(s->wv,(const float*)(const void*)v,e);if(hn&&s->gen_normal)tn_normal(s,v,nn);tn_texsets(o,v,e,nn,hn,r);}
   else for(k=0;k<tn_sets;k++){uint32_t u=0,vv=0;
    if(s->idx[k]<s->ins){const uint8_t *q=v+s->itex[s->idx[k]];u=u32(q);vv=s->itn[s->idx[k]]>1?u32(q+4):0;} /* .653: a set the vertex lacks: 0, as tn_attributes */
    if((u&0x7F800000UL)==0x7F800000UL)u=0;if((vv&0x7F800000UL)==0x7F800000UL)vv=0; /* a non-finite coordinate: 0 (the VxD refuses them) */
    r[tn_toff[k]]=u;r[tn_toff[k]+1]=vv;}
   tn_done[o]=1;}
  vx_d3d_tn[8]+=s->lit?(i+4<=n?4:n-i):0;if(s->fog)vx_d3d_tn[9]+=i+4<=n?4:n-i;
 }
#undef TN_DC
#undef TN_SC
#undef TN_SAME
}
