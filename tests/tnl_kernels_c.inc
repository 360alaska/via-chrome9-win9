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
 /* .707 (a review of .703: the point-light kernel, tnk_light and tnk_spec each made the group's eye-space normals, normalized them with a
  * square root and a divide, and made its eye-space position, again): the first kernel of a group that makes them keeps them here and
  * sets nready / eready; the others load them (the same operations made once: the same bits). tn_soa_finish_b clears both each group. */
 /* 9088 */ float nv[3][4];      /* the normals in eye space (normalized with NORMALIZENORMALS), x y z */
 /* 9136 */ float ev[3][4];      /* the eye-space position x y z */
 /* 9184 */ uint32_t nready,eready,pskip,pad21; /* .711: at 9184 (.707-.710 kept 128 free bytes before them, from a range-squared skip
  * removed in .708) */
 /* R1 (the T&L reworked): the fields of the tnq_ kernels below. One position formula for every path: m holds world x view x projection
  * with the viewport folded in (its x and y columns times the viewport's half sizes, its z column times the depth range), vp[0] vp[2]
  * vp[4] the window's centre and the depth range's start, so a window coordinate is one product and one sum after the reciprocal. */
 /* 9200 */ float qg[4][4];             /* [0] gxw [1] gyw: the side planes |X| <= gxw W, |Y| <= gyw W (the guard band, else the viewport); [2] zk: the far plane Z <= zk W; [3] the depth range's end (a depth is held to it) */
 /* 9264 */ float qv[2][4];             /* the viewport's half sizes (the viewport codes: |X| > vxa W, |Y| > vya W) */
 /* 9296 */ float qtwo[4],q15[4],qws[4]; /* 2.0 (the reciprocal's first Newton step), 1.5 (the reciprocal square root's); what the reciprocal is multiplied by for the record's 1/w (1, but for a projection scaled out of the floats' middle range: tn_planes) */
 /* 9344 */ uint32_t qlo[2][4],qlim[2][4]; /* tnq_pos: a record's words taken as inside when (word - lo) <= lim, unsigned, by pairs: [0] x y x y against the viewport, [1] z 1/w z 1/w against the depth range and 1 .. 7F7FFFFF; kept ready for the signed compare (lo + 80000000, lim ^ 80000000) */
 /* 9408 */ uint32_t qacc[4];           /* tnq_pos: eight 16-bit words, FFFF once a word of the range was not inside: x y x y, z 1/w z 1/w. tnq_bbxy: lanes with an x or y that is no number. tnq_vc: the codes ORed */
 /* 9424 */ float qmn[4],qmx[4];        /* tnq_bbxy: the smallest and largest window x y x y of a range (the caller starts them) */
 /* 9456 */ float qvlo[4],qvhi[4];      /* tnq_vc: the viewport's left top left top, right bottom right bottom */
 /* 9488 */ uint32_t qk12[4],qk48[4];   /* tnq_vc: 1 2 1 2, 4 8 4 8 */
 /* 9520 */ float ql[8][3][4];          /* tnq_lit: light k's direction in the mesh's own space (x y z splats) */
 /* 9904 */ float qc[8][3][4];          /* tnq_lit: light k's colour b g r: its diffuse x the material's x 255, or its diffuse alone when the vertex colour multiplies it (qvcol) */
 /* 10288 */ float qbase[3][4];         /* tnq_lit: the colour without the lights, b g r, x 255 */
 /* 10336 */ float qv0[3][4];           /* tnq_lit, qvcol: what the vertex colour multiplies before the lights (global and light ambient when the ambient source is the vertex's), b g r */
 /* 10384 */ uint32_t qalpha[4],qspec[4]; /* tnq_lit: the material's alpha byte by lane; the specular word by lane */
 /* 10416 */ uint32_t qnl,qnnorm,qvcol,qn; /* tnq_lit: lights, normals made unit length, the diffuse word as the material; qn: vertices (a multiple of four) */
 /* 10432 */ uint32_t qsstride,qdstride,qinorm,qidif; /* a source vertex's bytes, a record's, the normal's and the diffuse word's place in the vertex */
 /* 10448 */ uint32_t qadm,qadc,qado,qasm,qasc,qaso,qans,qapad; /* tnq_attr: the diffuse word (0 left, 1 copied from qado, 2 qadc), the specular word (0 left, 1 qasc, 2 the vertex's colour with alpha FF, 3 the vertex's whole), sets */
 /* 10480 */ uint32_t qato[4],qatv[4],qatd[4]; /* tnq_attr: by set, its place in the vertex (FFFFFFFF none), whether it has a second coordinate, its word in the record */
 /* 10528 */ uint32_t qtn,qtist,qtrst,qtcm,qtkept,qtpad[3]; /* tnq_cull: triangles, an index triple's bytes, a record's bytes, the cull mode (0 none, 2, 3); out: triangles kept */
 /* 10560 */ const uint8_t *qsrc;uint8_t *qdst; /* the range kernels' first vertex and first record */
 const uint8_t *qmark;uint8_t *qvc;     /* tnq_lit, tnq_attr: a byte a vertex of the range, 0 for one no kept triangle uses (0: every vertex is wanted); tnq_vc: a code a record */
 const uint8_t *qtidx;uint16_t *qtout;const uint8_t *qtrec,*qtvc;uint8_t *qtnd,*qttrash; /* tnq_cull: the triples in and out; the record, viewport code and mark of index value 0; where a dropped triangle's marks go */
} tnk_frame;
#define TNK_OFF(f,o) typedef char tnk_off_##f[(offsetof(tnk_frame,f)==(o))?1:-1];
TNK_OFF(m,0) TNK_OFF(vp,256) TNK_OFF(e,384) TNK_OFF(nm,640) TNK_OFF(ld,784) TNK_OFF(lc,1296) TNK_OFF(base,1808) TNK_OFF(fog,1872) TNK_OFF(nl,1936) TNK_OFF(stride,1960) TNK_OFF(first,1968) TNK_OFF(reuse_diffuse,1972) TNK_OFF(eye_z_ready,1976)
TNK_OFF(gx,1984) TNK_OFF(gdc,2080) TNK_OFF(od,2112) TNK_OFF(k255,2144) TNK_OFF(kexpc,2208) TNK_OFF(kneg80,2336) TNK_OFF(kinf,2352) TNK_OFF(ksign,2368) TNK_OFF(k127,2384) TNK_OFF(kmask24,2400) TNK_OFF(kb,2416) TNK_OFF(k7f,2512) TNK_OFF(sx,2528)
#ifdef __WATCOMC__
TNK_OFF(cx,2540) TNK_OFF(cc,2560) TNK_OFF(rec,2564) TNK_OFF(scratch,2576) TNK_OFF(npl,2704) TNK_OFF(pp,2720) TNK_OFF(pr,3104) TNK_OFF(pdc,3616) TNK_OFF(pac,4000)
TNK_OFF(sdc,4384) TNK_OFF(spow,5152) TNK_OFF(klog,5168) TNK_OFF(ksqrt2,5232) TNK_OFF(kmant,5248) TNK_OFF(spec,5264)
TNK_OFF(cvn,5280) TNK_OFF(cvdir,5312) TNK_OFF(cvl,5376) TNK_OFF(cvm,7680) TNK_OFF(cvamb,7936) TNK_OFF(kff,7984)
TNK_OFF(nspot,8000) TNK_OFF(pspot,8016) TNK_OFF(pmode,8048) TNK_OFF(psd,8080) TNK_OFF(pcone,8464) TNK_OFF(lbase,8848)
TNK_OFF(sdone,8912) TNK_OFF(sacc,8928) TNK_OFF(cvvary,8976) TNK_OFF(vc,8980) TNK_OFF(spcut,8992) TNK_OFF(vacc,9008) TNK_OFF(kvc,9024) TNK_OFF(nv,9088) TNK_OFF(ev,9136) TNK_OFF(nready,9184) TNK_OFF(pskip,9192)
#endif
#if defined(__WATCOMC__)||defined(__i386__) /* R1: the tnq_ kernels read these with aligned operands (the frame is on a 64-byte line) */
#define TNK_AL(f) typedef char tnk_al_##f[(offsetof(tnk_frame,f)%16==0)?1:-1];
TNK_AL(qg) TNK_AL(qv) TNK_AL(qtwo) TNK_AL(q15) TNK_AL(qws) TNK_AL(qlo) TNK_AL(qlim) TNK_AL(qacc) TNK_AL(qmn) TNK_AL(qmx) TNK_AL(qvlo) TNK_AL(qvhi) TNK_AL(qk12) TNK_AL(qk48) TNK_AL(ql) TNK_AL(qc) TNK_AL(qbase) TNK_AL(qv0) TNK_AL(qalpha) TNK_AL(qspec)
TNK_OFF(qtwo,9296) TNK_OFF(qlo,9344) TNK_OFF(qlim,9376) TNK_OFF(qacc,9408) TNK_OFF(qn,10428) TNK_OFF(qsstride,10432) TNK_OFF(qdstride,10436) TNK_OFF(qsrc,10560) TNK_OFF(qdst,10564) /* the hand-ordered _tnq_pos (tools/tnl_asm.py) has these places */
#endif
/* TNK KERNELS END 1 */
/* TNK KERNELS BEGIN 2 */
#include <emmintrin.h>
#include <stddef.h>
#define LD(p) _mm_loadu_ps((const float*)(p))
#define LDI(p) _mm_loadu_si128((const __m128i*)(p))
#define ST(p,v) _mm_storeu_ps((float*)(p),v)
/* R1: tnk_pos and tnk_vcodes (.544-.765) are tnq_gpos and tnq_vcodes below: the folded matrix, the reciprocal by estimate and Newton step */
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
/* .707: the group's eye-space normals: made by the first kernel that needs them (the operations each kernel made before, in the same
 * order: the same bits) and kept in the frame for the rest (nready) */
static __inline void tnk_nrm(tnk_frame *f,__m128 zero,__m128 one,__m128 *px,__m128 *py,__m128 *pz) {
 __m128 nx,ny,nz;
 if(f->nready){*px=LD(f->nv[0]);*py=LD(f->nv[1]);*pz=LD(f->nv[2]);return;}
 {const __m128 ux=LD(f->gnx),uy=LD(f->gny),uz=LD(f->gnz);
  nx=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[0])),_mm_mul_ps(uy,LD(f->nm[3]))),_mm_mul_ps(uz,LD(f->nm[6])));
  ny=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[1])),_mm_mul_ps(uy,LD(f->nm[4]))),_mm_mul_ps(uz,LD(f->nm[7])));
  nz=_mm_add_ps(_mm_add_ps(_mm_mul_ps(ux,LD(f->nm[2])),_mm_mul_ps(uy,LD(f->nm[5]))),_mm_mul_ps(uz,LD(f->nm[8])));}
 if(f->nnorm){__m128 l=_mm_sqrt_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,nx),_mm_mul_ps(ny,ny)),_mm_mul_ps(nz,nz))),m=_mm_cmpgt_ps(l,zero),r;
  r=_mm_and_ps(_mm_div_ps(one,l),m);nx=_mm_or_ps(_mm_and_ps(_mm_mul_ps(nx,r),m),_mm_andnot_ps(m,nx));ny=_mm_or_ps(_mm_and_ps(_mm_mul_ps(ny,r),m),_mm_andnot_ps(m,ny));
  nz=_mm_or_ps(_mm_and_ps(_mm_mul_ps(nz,r),m),_mm_andnot_ps(m,nz));}
 ST(f->nv[0],nx);ST(f->nv[1],ny);ST(f->nv[2],nz);f->nready=1;
 *px=nx;*py=ny;*pz=nz;
}
/* .707: the group's eye-space position (world x view), made once and kept (eready) */
static __inline void tnk_eyepos(tnk_frame *f,__m128 *pex,__m128 *pey,__m128 *pez) {
 if(f->eready){*pex=LD(f->ev[0]);*pey=LD(f->ev[1]);*pez=LD(f->ev[2]);return;}
 {const __m128 x=LD(f->gx),y=LD(f->gy),z=LD(f->gz);
  const __m128 ex=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[0])),_mm_mul_ps(y,LD(f->e[4]))),_mm_mul_ps(z,LD(f->e[8]))),LD(f->e[12]));
  const __m128 ey=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[1])),_mm_mul_ps(y,LD(f->e[5]))),_mm_mul_ps(z,LD(f->e[9]))),LD(f->e[13]));
  const __m128 ez=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[2])),_mm_mul_ps(y,LD(f->e[6]))),_mm_mul_ps(z,LD(f->e[10]))),LD(f->e[14]));
  ST(f->ev[0],ex);ST(f->ev[1],ey);ST(f->ev[2],ez);f->eready=1;*pex=ex;*pey=ey;*pez=ez;}
}
static void tnk_light(tnk_frame *f) {
 const __m128 zero=LD(f->vp[6]),one=LD(f->kone),k255=LD(f->k255); /* .707: 0 from the frame (vp[6]): the board's kernel is now clang's, which put literal zeros in memory */
 __m128 cb,cg,cr,ca,fa=one,nx=zero,ny=zero,nz=zero;
 if(f->fogmode){
  __m128 d;
  if(f->eye_z_ready)d=_mm_andnot_ps(LD(f->ksign),LD(f->gz));
  else if(f->eready||f->rfog){__m128 ex,ey,ez; /* .707: the position the point-light kernel made (or made here and kept) */
   tnk_eyepos(f,&ex,&ey,&ez);
   d=_mm_andnot_ps(LD(f->ksign),ez); /* |z| */
   if(f->rfog)d=_mm_sqrt_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(ex,ex),_mm_mul_ps(ey,ey)),_mm_mul_ps(ez,ez)));
  }
  else{
   const __m128 x=LD(f->gx),y=LD(f->gy),z=LD(f->gz);
   const __m128 ez=_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,LD(f->e[2])),_mm_mul_ps(y,LD(f->e[6]))),_mm_mul_ps(z,LD(f->e[10]))),LD(f->e[14]));
   d=_mm_andnot_ps(LD(f->ksign),ez); /* |z| */
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
    tnk_nrm(f,zero,one,&nx,&ny,&nz); /* .707: made once a group (tnk_nrm) */
    for(k=0;k<f->nl;k++){ /* a directional light: N.L > 0 adds N.L x (diffuse x diffuse) */
     __m128 ndl=_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,LD(f->ld[k][0])),_mm_mul_ps(ny,LD(f->ld[k][1]))),_mm_mul_ps(nz,LD(f->ld[k][2])));
     ndl=_mm_and_ps(ndl,_mm_cmpgt_ps(ndl,zero));
     cb=_mm_add_ps(cb,_mm_mul_ps(ndl,LD(f->lc[k][0])));cg=_mm_add_ps(cg,_mm_mul_ps(ndl,LD(f->lc[k][1])));cr=_mm_add_ps(cr,_mm_mul_ps(ndl,LD(f->lc[k][2])));}
   }
   /* bytes: times 255, rounded to nearest, saturated to 0..255 (a NaN gives 0x80000000: 0 after the saturation), b | g << 8 | r << 16 | a << 24 */
   {__m128i b=_mm_cvtps_epi32(_mm_mul_ps(cb,k255)),g=_mm_cvtps_epi32(_mm_mul_ps(cg,k255)),r=_mm_cvtps_epi32(_mm_mul_ps(cr,k255)),a=_mm_cvtps_epi32(_mm_mul_ps(ca,k255)),w;
    b=_mm_packs_epi32(b,b);b=_mm_packus_epi16(b,b);g=_mm_packs_epi32(g,g);g=_mm_packus_epi16(g,g);
    r=_mm_packs_epi32(r,r);r=_mm_packus_epi16(r,r);a=_mm_packs_epi32(a,a);a=_mm_packus_epi16(a,a);
    w=_mm_unpacklo_epi8(_mm_unpacklo_epi8(b,r),_mm_unpacklo_epi8(g,a)); /* b g r a a lane */
    _mm_storeu_si128((__m128i*)f->od,w);}
  }
  {__m128i a=_mm_cvtps_epi32(_mm_mul_ps(fa,k255));a=_mm_packs_epi32(a,a);a=_mm_packus_epi16(a,a);a=_mm_unpacklo_epi8(a,a);
   a=_mm_slli_epi32(_mm_unpacklo_epi16(a,a),24); /* 0 0 0 a: the specular's colour is 0 (no specular lighting), the alpha the fog (.707: each
    * lane's byte spread over its word and shifted up, the same word as 0-interleaving it, with no zero for clang to put in memory) */
   _mm_storeu_si128((__m128i*)f->os,a);}
 }
 else{ /* unlit: the words as they came, the specular's alpha the fog (255 without fog) */
  __m128i a=_mm_cvtps_epi32(_mm_mul_ps(fa,k255));a=_mm_packs_epi32(a,a);a=_mm_packus_epi16(a,a);a=_mm_unpacklo_epi8(a,a);a=_mm_slli_epi32(_mm_unpacklo_epi16(a,a),24); /* .707: as above */
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
static __inline __attribute__((always_inline)) __m128 tnk_pow(const tnk_frame *f,__m128 x,const float *pw,uint32_t mode); /* .739: inlined always (the kernels may hold no call; X2's inner loop made clang keep it apart) */
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
static void tnk_plight(tnk_frame *f) { /* .647: point lights alone, no spot light, no specular lighting: the C of tools/tnl_asm.py's hand-written
 * _tnk_plight, which the board runs (.707: kept as .706 had it: clang's tnk_splight ran 282 instructions a group for one light, the
 * hand-written one 198). It makes its own normals and position and adds into base, which tn_soa_finish_b fills from lbase first. */
 const __m128 zero=LD(f->vp[6]), /* .653: 0 from the frame (vp[6]), as tnk_spec: clang made max(N.L, 0) of the select, with its 0 in memory */one=LD(f->kone),x=LD(f->gx),y=LD(f->gy),z=LD(f->gz);
 __m128 ex,ey,ez,nx=zero,ny=zero,nz=zero,cb=LD(f->lbase[0]),cg=LD(f->lbase[1]),cr=LD(f->lbase[2]);uint32_t k; /* .741F: from lbase, as tnk_splight (the 64-byte copy into base a group is gone) */
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
  ST(f->nv[0],nx);ST(f->nv[1],ny);ST(f->nv[2],nz);f->nready=1; /* .741: tnk_light_k loads these and does not make them again. eready stays 0: the board's light fog recomputes eye z, and the host must too */
 }
 for(k=0;k<f->npl;k++){
  if(f->pskip&(1UL<<k))continue; /* .741F: out of the tile's reach (tn_tile_lights): it adds nothing to any lane */
  __m128 lx=_mm_sub_ps(LD(f->pp[k][0]),ex),ly=_mm_sub_ps(LD(f->pp[k][1]),ey),lz=_mm_sub_ps(LD(f->pp[k][2]),ez),d,m,r,q,qm,att,okr;
  /* .648 (board, VXTLP 6b on .647: a point light 38.6 ms a frame against the runtime's 36.1): no square root or divide. The reciprocal
   * square root estimate (12 bits) and one Newton step, r = r0 - r0 (d2 r0 r0 - 1) / 2; then d = d2 r. A vertex on the light (d2 0) keeps
   * its vector and a distance of 0, as before */
  {const __m128 d2=_mm_add_ps(_mm_add_ps(_mm_mul_ps(lx,lx),_mm_mul_ps(ly,ly)),_mm_mul_ps(lz,lz)),r0=_mm_rsqrt_ps(d2);
   r=_mm_sub_ps(r0,_mm_mul_ps(_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(_mm_mul_ps(d2,r0),r0),one),LD(f->khalf)),r0));
   m=_mm_cmplt_ps(zero,d2);d=_mm_and_ps(_mm_mul_ps(d2,r),m);}
  okr=_mm_cmple_ps(d,LD(f->pr[k][0]));
  if(!_mm_movemask_ps(okr))continue; /* .741F: no lane inside the range: the attenuation is masked to 0 below, every product a 0 (as tnk_splight, .697) */
  {
  lx=_mm_xor_ps(lx,_mm_and_ps(_mm_xor_ps(_mm_mul_ps(lx,r),lx),m));
  ly=_mm_xor_ps(ly,_mm_and_ps(_mm_xor_ps(_mm_mul_ps(ly,r),ly),m));
  lz=_mm_xor_ps(lz,_mm_and_ps(_mm_xor_ps(_mm_mul_ps(lz,r),lz),m));} /* a vertex on the light keeps its way (d 0) */
  q=_mm_add_ps(_mm_add_ps(_mm_mul_ps(LD(f->pr[k][2]),d),LD(f->pr[k][1])),_mm_mul_ps(_mm_mul_ps(d,LD(f->pr[k][3])),d));
  qm=_mm_cmpgt_ps(q,zero);
  {const __m128 y0=_mm_rcp_ps(q); /* .648: 1/q as the estimate and one Newton step, y0 - y0 (q y0 - 1) */
   att=_mm_xor_ps(_mm_and_ps(_mm_xor_ps(_mm_sub_ps(y0,_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(q,y0),one),y0)),one),qm),one);}
  att=_mm_and_ps(att,okr);
  if(f->pamb&(1UL<<k)){cb=_mm_add_ps(cb,_mm_mul_ps(att,LD(f->pac[k][0])));cg=_mm_add_ps(cg,_mm_mul_ps(att,LD(f->pac[k][1])));cr=_mm_add_ps(cr,_mm_mul_ps(att,LD(f->pac[k][2])));} /* .741F: an ambient product of 0 adds 0 (as tnk_splight, .697) */
  if(f->haveN){__m128 ndl=_mm_add_ps(_mm_add_ps(_mm_mul_ps(lx,nx),_mm_mul_ps(ly,ny)),_mm_mul_ps(lz,nz));
   ndl=_mm_and_ps(ndl,_mm_cmpgt_ps(ndl,zero));ndl=_mm_mul_ps(ndl,att);
   cb=_mm_add_ps(cb,_mm_mul_ps(ndl,LD(f->pdc[k][0])));cg=_mm_add_ps(cg,_mm_mul_ps(ndl,LD(f->pdc[k][1])));cr=_mm_add_ps(cr,_mm_mul_ps(ndl,LD(f->pdc[k][2])));}
 }
 ST(f->base[0],cb);ST(f->base[1],cg);ST(f->base[2],cr);ST(f->base[3],LD(f->lbase[3])); /* .741F: alpha as lbase's (the copy did it) */
 f->sdone=0;
}
static void tnk_splight(tnk_frame *f) { /* .653: the point and spot lights (.697: and the specular sums); .707: every group lit by point lights (the hand-written tnk_plight, point lights alone, retired) */
 const __m128 zero=LD(f->vp[6]), /* .653: 0 from the frame (vp[6]), as tnk_spec: clang made max(N.L, 0) of the select, with its 0 in memory */one=LD(f->kone);
 __m128 ex,ey,ez,nx=zero,ny=zero,nz=zero,cb=LD(f->lbase[0]),cg=LD(f->lbase[1]),cr=LD(f->lbase[2]),vx,vy,vz; /* .707: from lbase (the colour without the point lights): .647-.706 copied lbase into base first, every group */uint32_t k;
 const int sp=f->spec&&f->haveN; /* .697: the specular sums here (tnk_spec's lights need the normal: f->spec is only set with one) */
 tnk_eyepos(f,&ex,&ey,&ez); /* .707: made once a group and kept (tnk_light's fog and tnk_spec's view load it) */
 if(f->haveN)tnk_nrm(f,zero,one,&nx,&ny,&nz); /* .707: made once a group and kept (tnk_light and tnk_spec load them) */
 if(sp){const __m128 all=_mm_cmpeq_ps(one,one); /* .697: the directional lights' specular terms first, as tnk_spec */
  ST(f->sacc[0],zero);ST(f->sacc[1],zero);ST(f->sacc[2],zero);
  tnk_view(f,ex,ey,ez,&vx,&vy,&vz);
  for(k=0;k<f->nl;k++){const __m128 lx=LD(f->ld[k][0]),ly=LD(f->ld[k][1]),lz=LD(f->ld[k][2]);
   tnk_spec1(f,all,_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,lx),_mm_mul_ps(ny,ly)),_mm_mul_ps(nz,lz)),one,lx,ly,lz,nx,ny,nz,vx,vy,vz,k,f->sacc);}}
 else vx=vy=vz=zero;
 for(k=0;k<f->npl;k++){
  if(f->pskip&(1UL<<k))continue; /* .741F: out of the tile's reach (tn_tile_lights) */
  __m128 lx=_mm_sub_ps(LD(f->pp[k][0]),ex),ly=_mm_sub_ps(LD(f->pp[k][1]),ey),lz=_mm_sub_ps(LD(f->pp[k][2]),ez),d,m,r,q,qm,att,okr;
  /* .648 (board, VXTLP 6b on .647: a point light 38.6 ms a frame against the runtime's 36.1): no square root or divide. The reciprocal
   * square root estimate (12 bits) and one Newton step, r = r0 - r0 (d2 r0 r0 - 1) / 2; then d = d2 r. A vertex on the light (d2 0) keeps
   * its vector and a distance of 0, as before */
  {const __m128 d2=_mm_add_ps(_mm_add_ps(_mm_mul_ps(lx,lx),_mm_mul_ps(ly,ly)),_mm_mul_ps(lz,lz)),r0=_mm_rsqrt_ps(d2);
   r=_mm_sub_ps(r0,_mm_mul_ps(_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(_mm_mul_ps(d2,r0),r0),one),LD(f->khalf)),r0));
   m=_mm_cmplt_ps(zero,d2);d=_mm_and_ps(_mm_mul_ps(d2,r),m);}
  okr=_mm_cmple_ps(d,LD(f->pr[k][0]));
  if(!_mm_movemask_ps(okr))continue; /* .697 (tnk_splight; the hand-written tnk_plight does not skip): no lane inside the light's range: it adds nothing (the attenuation is masked to 0 below) */
  {r=_mm_and_ps(r,m);lx=_mm_mul_ps(lx,r);ly=_mm_mul_ps(ly,r);lz=_mm_mul_ps(lz,r);} /* .697 (tnk_splight): a vertex on the light (d2 0: its
   * way is 0 0 0, or so small its square is 0) gets the way 0 0 0, whose N.L and cone add nothing, as its kept way did; 4 operations, not 12 */
  q=_mm_add_ps(_mm_add_ps(_mm_mul_ps(LD(f->pr[k][2]),d),LD(f->pr[k][1])),_mm_mul_ps(_mm_mul_ps(d,LD(f->pr[k][3])),d));
  qm=_mm_cmpgt_ps(q,zero);
  {const __m128 y0=_mm_rcp_ps(q); /* .648: 1/q as the estimate and one Newton step, y0 - y0 (q y0 - 1) */
   att=_mm_xor_ps(_mm_and_ps(_mm_xor_ps(_mm_sub_ps(y0,_mm_mul_ps(_mm_sub_ps(_mm_mul_ps(q,y0),one),y0)),one),qm),one);}
  att=_mm_and_ps(att,okr);
  if(f->pspot[k])att=_mm_mul_ps(att,tnk_cone(f,k,lx,ly,lz)); /* .653 */
  if(f->pamb&(1UL<<k)){cb=_mm_add_ps(cb,_mm_mul_ps(att,LD(f->pac[k][0])));cg=_mm_add_ps(cg,_mm_mul_ps(att,LD(f->pac[k][1])));cr=_mm_add_ps(cr,_mm_mul_ps(att,LD(f->pac[k][2])));} /* .697: an ambient product of 0 adds 0 (tnk_splight; the hand-written tnk_plight adds it) */
  if(f->haveN){__m128 ndl=_mm_add_ps(_mm_add_ps(_mm_mul_ps(lx,nx),_mm_mul_ps(ly,ny)),_mm_mul_ps(lz,nz));
   if(sp)tnk_spec1(f,okr,ndl,att,lx,ly,lz,nx,ny,nz,vx,vy,vz,8+k,f->sacc); /* .697: this light's specular term now (tnk_spec's N.L: the same products) */
   ndl=_mm_and_ps(ndl,_mm_cmpgt_ps(ndl,zero));ndl=_mm_mul_ps(ndl,att);
   cb=_mm_add_ps(cb,_mm_mul_ps(ndl,LD(f->pdc[k][0])));cg=_mm_add_ps(cg,_mm_mul_ps(ndl,LD(f->pdc[k][1])));cr=_mm_add_ps(cr,_mm_mul_ps(ndl,LD(f->pdc[k][2])));}
 }
 ST(f->base[0],cb);ST(f->base[1],cg);ST(f->base[2],cr);ST(f->base[3],LD(f->lbase[3])); /* .707: alpha as lbase's (the copy did it) */
 f->sdone=sp;
}
/* .651 (board, 3DMark 2001's High Polygon Count on .650: a frame of 25 s, the test given as N/A; host: eight lights with specular
 * lighting 5 times the time without, every vertex through the one-vertex code and its double-precision power): the specular word for
 * four lanes, after tnk_light has written the fog in its alpha. For each directional and point light, as tn_light_color: only where
 * N.L > 0 (and inside a point light's range), H = L + V (V to the eye from the vertex with LOCALVIEWER, else (0, 0, -1)), N.H / |H| above
 * 0 raised to the material's power, times the attenuation, times the light's specular product. The power x^p = e^(p ln x): ln x from the
 * float's exponent and 2 atanh((m - 1) / (m + 1)) for the mantissa in [0.707, 1.414] (to z^9: 1e-9), e^y by tnk_exp; x >= 1 gives 1, as
 * tn_pow. Compiled for the board by clang from this source (tools/tnl_asm.py), so the same instructions run on both. */
static __inline __attribute__((always_inline)) __m128 tnk_pow(const tnk_frame *f,__m128 x,const float *pw,uint32_t mode) { /* .653: the power (splat) and its case as arguments (the specular power, a spot light's falloff) */
 const __m128 one=LD(f->kone);
 __m128i b,ei;__m128 m,big,ex,z,z2,lnm,r,lx;
 if(mode==1)return _mm_min_ps(x,one);
 if(mode==2)return one;
 if(mode==3){uint32_t n=f->spint;__m128 r,b=_mm_min_ps(x,one);if(!n)n=1; /* .659: an integer power (2-255) by squaring (x in 0..1: no overflow, and 1
   * stays 1); r starts at the lowest set bit's square, with no 1.0 for clang to put in memory */
  while(!(n&1)){b=_mm_mul_ps(b,b);n>>=1;}
  r=b;n>>=1;
  while(n){while(!(n&1)){b=_mm_mul_ps(b,b);n>>=1;}b=_mm_mul_ps(b,b);r=_mm_mul_ps(r,b);n>>=1;} /* .739 (item X2): a clear bit only squares
   * (an inner loop clang cannot turn into a select); .659-.738 multiplied r by 1.0 for it. The same squarings and products in the same
   * order: the same bits (a product with 1.0 is exact) */
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
 /* .741F: no lane inside the outer cone: 0 for all four (what the select below gives); no inside lane in the falloff band: 1 where inside.
  * The divide and the power only when a lane needs them: the same bits */
 if(!_mm_movemask_ps(in))return _mm_and_ps(in,one);
 if(!_mm_movemask_ps(_mm_and_ps(in,part)))return _mm_and_ps(in,one);
 {
 const __m128 p=tnk_pow(f,_mm_div_ps(_mm_sub_ps(rho,scp),_mm_sub_ps(sct,scp)),f->pcone[q][2],f->pmode[q]);
 return _mm_and_ps(in,_mm_or_ps(_mm_and_ps(part,p),_mm_andnot_ps(part,one)));}
}
static void tnk_spec(tnk_frame *f) { /* .697: the directional lights only (with point lights, tnk_splight made the sums: sdone), then the packing */
 __m128 cb,cg,cr;uint32_t k;
 if(f->sdone){cb=LD(f->sacc[0]);cg=LD(f->sacc[1]);cr=LD(f->sacc[2]);}
 else{const __m128 zero=LD(f->vp[6]),one=LD(f->kone),all=_mm_cmpeq_ps(one,one);
  __m128 ex=zero,ey=zero,ez=zero,nx,ny,nz,vx,vy,vz;
  ST(f->sacc[0],zero);ST(f->sacc[1],zero);ST(f->sacc[2],zero);
  if(f->lview)tnk_eyepos(f,&ex,&ey,&ez); /* .707: only the local viewer reads the position (tnk_view); kept if a kernel made it */
  tnk_nrm(f,zero,one,&nx,&ny,&nz); /* .707: tnk_light's normals (nready), not made again */
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
/* .741F (from the "vxstream" core's tile test): the eye-space box around n vertices (a tile of up to 32: the places in list, their source
 * vertices in sp, the position the first 12 bytes of each: 8 and 4 bytes loaded, nothing past them). The object-space minimum and maximum
 * first; the box of their image under the world x view matrix is T + the sum over the three columns of min / max (column x minimum,
 * column x maximum), which holds every vertex's eye position. scratch[6] the box's low corner (x y z in lanes 0-2), scratch[7] its high
 * corner, scratch[5] the sum of the terms' sizes (|T| + the larger |column x bound| a column): the rounding of a vertex's own eye position
 * is a small part of it (tn_tile_lights' margin). A NaN position drops out of the minimum and maximum (.765: the vertex the first operand; such a vertex takes no light: its
 * distance is no number); an infinite one makes the box infinite or no number, and no light is dropped. */
#define TNK_V3(p_) _mm_movelh_ps(_mm_castsi128_ps(_mm_loadl_epi64((const __m128i*)(const void*)(p_))),_mm_load_ss((const float*)(const void*)(p_)+2))
#define TNK_COL(c_) _mm_movelh_ps(_mm_unpacklo_ps(_mm_load_ss(f->e[(c_)*4]),_mm_load_ss(f->e[(c_)*4+1])),_mm_load_ss(f->e[(c_)*4+2]))
#define TNK_BOX(c_,sh_) {const __m128 col=TNK_COL(c_),a=_mm_mul_ps(col,_mm_shuffle_ps(mn,mn,sh_)),b=_mm_mul_ps(col,_mm_shuffle_ps(mx,mx,sh_)); \
  bl=_mm_add_ps(bl,_mm_min_ps(a,b));bh=_mm_add_ps(bh,_mm_max_ps(a,b));bm=_mm_add_ps(bm,_mm_max_ps(_mm_andnot_ps(sg,a),_mm_andnot_ps(sg,b)));}
static void tnk_bounds(tnk_frame *f,const uint8_t *const *sp,const uint16_t *list,uint32_t n) {
 const __m128 sg=LD(f->ksign);__m128 mn,mx,bl,bh,bm;uint32_t i;
 mn=mx=TNK_V3(sp[list[0]]);
 for(i=1;i<n;i++){const __m128 v=TNK_V3(sp[list[i]]);mn=_mm_min_ps(v,mn);mx=_mm_max_ps(v,mx);} /* .765 (outside audit of .757, item 4): the vertex first: minps / maxps give the second operand when either is no number, so a NaN vertex keeps the box so far (it set the box to NaN, and the next vertex then replaced it with itself, losing the earlier vertices: a light reaching them could be dropped). A NaN first vertex leaves the box no number: no light dropped */
 bl=bh=TNK_COL(3);bm=_mm_andnot_ps(sg,bl);
 TNK_BOX(0,0x00) TNK_BOX(1,0x55) TNK_BOX(2,0xAA)
 ST(f->scratch[5],bm);ST(f->scratch[6],bl);ST(f->scratch[7],bh);
}
#undef TNK_V3
#undef TNK_COL
#undef TNK_BOX
#undef LD
#undef LDI
#undef ST
/* TNK KERNELS END 2 */
/* TNK KERNELS BEGIN 3 */
/* R1 (the T&L reworked): the tnq_ kernels. What changed against tnk_pos and tnk_light, and why:
 *  - one position formula for every path (tnq_pos: a vertex range straight from the application's buffer into records; tnq_gpos:
 *    staged places, with the clip codes): X Y Z W by the folded matrix, then a window coordinate is X / W + centre. Per group of
 *    four: 12 products and 12 sums for the transform, 3 and 3 for the window (.544-.765: 6 and 6). The same operations in the same
 *    order in both kernels, so a vertex has the same words whichever path its draw takes (two passes over a mesh never differ in
 *    depth);
 *  - 1/w is the reciprocal estimate and two Newton steps, not a divide. Measured instruction times (InstLatx64's tables: VIA C7
 *    Esther, family 6 model A; VIA Nano): divps 72 cycles on the C7 and 82 on the Nano 2000 (24 on the 3000), one at a time; rcpps
 *    16-20 and 11-14, an SSE sum or product 2 cycles with 6 of latency on the C7. The second step is r + r (1 - w r): the result is
 *    within a unit and a half in the last place of the quotient and exact where w is a power of two (w = 1 gives 1, so a 2D or
 *    orthographic draw's pixel positions are the divide's);
 *  - no 16-byte load or store at an address that may not be on 16 bytes: such a load takes 9 cycles on the C7 and 21 on the Nano
 *    2000, a store 8.6 and 36 (an 8-byte half: 1 to 2). Vertices are read and records written by halves (movlps, movhps); the
 *    staged arrays are stored aligned (the callers start a draw's places on a multiple of four);
 *  - tnq_pos makes no clip codes: after a group's records are stored, their words are tested as integers against the viewport,
 *    the depth range and 1/w > 0, two operations a pair of vertices, ORed into qacc. A range with qacc clear lies wholly inside the
 *    viewport in front of the eye (most draws): nothing else is looked at. With only x or y outside, tnq_bbxy gives the range's
 *    bounds and the caller knows a draw inside the guard band or wholly beyond one side; else the clipper's path (tnq_gpos);
 *  - lighting by directional lights in the mesh's own space (tnq_lit): N.L' with L' the light's direction taken through the normal
 *    matrix once a draw (tn_qlights), so a vertex costs one dot product a light, not a normal transform first. The colours are
 *    kept times 255, so the bytes come from one conversion and three packs.
 * No kernel has a constant of its own in memory (the board's copies are bytes without relocations): constants are the frame's. */
#if defined(__i386__)
#define QA(p) _mm_load_ps((const float*)(const void*)(p))
#define QAI(p) _mm_load_si128((const __m128i*)(const void*)(p))
#define QSA(p,v) _mm_store_ps((float*)(void*)(p),v)
#define QSAI(p,v) _mm_store_si128((__m128i*)(void*)(p),v)
#else /* the 64-bit host: the frame's fields past its pointers are not on 16 bytes */
#define QA(p) _mm_loadu_ps((const float*)(const void*)(p))
#define QAI(p) _mm_loadu_si128((const __m128i*)(const void*)(p))
#define QSA(p,v) _mm_storeu_ps((float*)(void*)(p),v)
#define QSAI(p,v) _mm_storeu_si128((__m128i*)(void*)(p),v)
#endif
typedef uint32_t __attribute__((may_alias,aligned(1))) tnq_u32;
typedef uint16_t __attribute__((may_alias,aligned(1))) tnq_u16;
#define TNQ_INL static __inline __attribute__((always_inline))
/* 8 bytes into the low or the high half of a register, and out of them */
#define QLL(p) _mm_castsi128_ps(_mm_loadl_epi64((const __m128i*)(const void*)(p)))
#define QLH(v,p) _mm_loadh_pi(v,(const __m64*)(const void*)(p))
#define QSL(p,v) _mm_storel_pi((__m64*)(void*)(p),v)
#define QSH(p,v) _mm_storeh_pi((__m64*)(void*)(p),v)
/* ((x a + y b) + z c) + d for column c of the folded matrix */
#define TNQ_XF(f,x,y,z,c) _mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,QA((f)->m[c])),_mm_mul_ps(y,QA((f)->m[4+(c)]))),_mm_mul_ps(z,QA((f)->m[8+(c)]))),QA((f)->m[12+(c)]))
/* 1/w: the estimate, r0 (2 - w r0), then r + r (1 - w r) */
TNQ_INL __m128 tnq_rcp(__m128 w,__m128 one,__m128 two) {
 const __m128 r0=_mm_rcp_ps(w),r1=_mm_mul_ps(r0,_mm_sub_ps(two,_mm_mul_ps(w,r0)));
 return _mm_add_ps(r1,_mm_mul_ps(r1,_mm_sub_ps(one,_mm_mul_ps(w,r1))));
}
/* four vertices' three floats at p, ss bytes apart, as lanes x y z: read as 8 and 8 bytes each (the four bytes after z are read too) */
#define TNQ_IN3(p,ss,x,y,z) {const uint8_t *p0_=(p),*p1_=p0_+(ss),*p2_=p1_+(ss),*p3_=p2_+(ss); \
 const __m128 a_=QLH(QLL(p0_),p1_),b_=QLH(QLL(p2_),p3_),c_=QLH(QLL(p0_+8),p1_+8),e_=QLH(QLL(p2_+8),p3_+8); \
 x=_mm_shuffle_ps(a_,b_,0x88);y=_mm_shuffle_ps(a_,b_,0xDD);z=_mm_shuffle_ps(c_,e_,0x88);}
/* a pair register's words not inside (qlo, qlim: all ones in the lane) */
#define TNQ_OUT(f,t,k) _mm_cmpgt_epi32(_mm_sub_epi32(_mm_castps_si128(t),QAI((f)->qlo[k])),QAI((f)->qlim[k]))

/* a range's positions: qn vertices (a multiple of four) of qsstride bytes from qsrc, their records' first four words (window x y z,
 * 1/w) at qdst, qdstride apart. qacc gathers, as eight 16-bit words (x y x y, z 1/w z 1/w), whether a word was not inside (the
 * viewport, the depth range, 1/w above 0 and finite: a word that is no number or below 0 is not inside, by its bits). */
/* (The board's copy of tnq_pos is ordered by hand for the C7, POSQ in tools/tnl_asm.py: these operations in this association;
 * tests/test_tnq_asm.c compares every word.) */
static void tnq_pos(tnk_frame *f) {
 const uint8_t *s=f->qsrc;uint8_t *d=f->qdst;const uint32_t ss=f->qsstride,ds=f->qdstride;uint32_t n=f->qn;
 const __m128 one=QA(f->kone),two=QA(f->qtwo);__m128i acc=QAI(f->qacc);
 for(;n;n-=4,s+=4*ss,d+=4*ds){
  __m128 x,y,z,X,Y,Z,W,r;
  _mm_prefetch((const char*)(s+8*ss),_MM_HINT_T0);_mm_prefetch((const char*)(s+10*ss),_MM_HINT_T0); /* (.681: the application's buffer is read cold; two groups ahead) */
  TNQ_IN3(s,ss,x,y,z)
  W=TNQ_XF(f,x,y,z,3);r=tnq_rcp(W,one,two);
  X=TNQ_XF(f,x,y,z,0);Y=TNQ_XF(f,x,y,z,1);Z=TNQ_XF(f,x,y,z,2);
  X=_mm_add_ps(_mm_mul_ps(X,r),QA(f->vp[0]));Y=_mm_add_ps(_mm_mul_ps(Y,r),QA(f->vp[2]));Z=_mm_add_ps(_mm_mul_ps(Z,r),QA(f->vp[4]));
  {const __m128 t0=_mm_unpacklo_ps(X,Y),t1=_mm_unpacklo_ps(Z,r),t2=_mm_unpackhi_ps(X,Y),t3=_mm_unpackhi_ps(Z,r); /* x0 y0 x1 y1, z0 w0 z1 w1, ... */
   QSL(d,t0);QSL(d+8,t1);QSH(d+ds,t0);QSH(d+ds+8,t1);QSL(d+2*ds,t2);QSL(d+2*ds+8,t3);QSH(d+3*ds,t2);QSH(d+3*ds+8,t3);
   acc=_mm_or_si128(acc,_mm_packs_epi32(_mm_or_si128(TNQ_OUT(f,t0,0),TNQ_OUT(f,t2,0)),_mm_or_si128(TNQ_OUT(f,t1,1),TNQ_OUT(f,t3,1))));}
 }
 QSAI(f->qacc,acc);
}
/* the smallest and largest window x and y of qn records (a multiple of four) at qdst into qmn and qmx (x y x y lanes; the caller
 * starts them); qacc: all ones in a lane with an x or y that is no number or infinite (such a word is left out of the bounds). For a
 * range tnq_pos found in front of the eye and in the depth range but not wholly in the viewport. */
static void tnq_bbxy(tnk_frame *f) {
 const uint8_t *rec=f->qdst;const uint32_t rst=f->qdstride;uint32_t n=f->qn;
 __m128 lo=QA(f->qmn),hi=QA(f->qmx);__m128i bad=_mm_castps_si128(_mm_xor_ps(lo,lo));const __m128i inf=QAI(f->kinf);
 for(;n;n-=2,rec+=2*rst){const __m128 v=QLH(QLL(rec),rec+rst);
  bad=_mm_or_si128(bad,_mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(v),inf),inf));
  lo=_mm_min_ps(v,lo);hi=_mm_max_ps(v,hi);} /* (minps and maxps give their second operand for a NaN: the bounds so far) */
 QSA(f->qmn,lo);QSA(f->qmx,hi);QSAI(f->qacc,bad);
}
/* by record (qn of them, a multiple of four, at qdst): the viewport's sides it lies beyond, 1 x < left, 2 y < top, 4 x > right,
 * 8 y > bottom, a byte each at qvc (two bytes past the last are written too); qacc: the codes ORed. For a range wholly in front of
 * the eye (every 1/w above 0), so the window position decides. No movmskps (7 cycles on the C7): the bytes are packed. */
static void tnq_vc(tnk_frame *f) {
 const uint8_t *rec=f->qdst;const uint32_t rst=f->qdstride;uint8_t *vc=f->qvc;uint32_t n=f->qn;
 const __m128 l=QA(f->qvlo),h=QA(f->qvhi);const __m128i k1=QAI(f->qk12),k2=QAI(f->qk48);__m128i any=_mm_castps_si128(_mm_xor_ps(l,l));
 for(;n;n-=2,rec+=2*rst,vc+=2){const __m128 v=QLH(QLL(rec),rec+rst);
  __m128i c=_mm_or_si128(_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(v,l)),k1),_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(h,v)),k2));
  c=_mm_or_si128(c,_mm_srli_epi64(c,32)); /* lanes 0 and 2: the two records' codes */
  any=_mm_or_si128(any,c);
  c=_mm_shuffle_epi32(c,0x08);c=_mm_packs_epi32(c,c);c=_mm_packus_epi16(c,c);
  *(tnq_u32*)(void*)vc=(uint32_t)_mm_cvtsi128_si32(c);}
 QSAI(f->qacc,any);
}
/* staged places first .. first+n-1 (first and n multiples of four) from lanes sx sy sz: X Y Z W into cx cy cz cw (the clipper's), the
 * codes, the records' position words. The codes: 1 Z < 0 or W <= 0, 2 Z > zk W, 8 X < -gxw W, 16 X > gxw W, 32 Y < -gyw W,
 * 64 Y > gyw W; 7F a position that is no number. 1/w is 0 where W <= 0; a depth is held to the range's end (1/w's last bit can
 * put a vertex on the far plane a step past it). The record's 1/w is the reciprocal times qws. With vc set, vacc gathers whether a
 * lane lies past a side of the viewport (tnq_vcodes then makes the codes). */
static void tnq_gpos(tnk_frame *f) {
 uint32_t i;const __m128 zero=QA(f->vp[6]),one=QA(f->kone),two=QA(f->qtwo);const __m128i inf=QAI(f->kinf);
 for(i=0;i<f->n;i+=4){
  const uint32_t o=f->first+i;const __m128 x=QA(f->sx+i),y=QA(f->sy+i),z=QA(f->sz+i);__m128 X,Y,Z,W,r,rs,px,py,pz;__m128i c,t;
  W=TNQ_XF(f,x,y,z,3);r=_mm_and_ps(tnq_rcp(W,one,two),_mm_cmpgt_ps(W,zero));
  X=TNQ_XF(f,x,y,z,0);Y=TNQ_XF(f,x,y,z,1);Z=TNQ_XF(f,x,y,z,2);
  QSA(f->cx+o,X);QSA(f->cy+o,Y);QSA(f->cz+o,Z);QSA(f->cw+o,W);
  rs=_mm_mul_ps(r,QA(f->qws));
  {const __m128 gw=_mm_mul_ps(W,QA(f->qg[0])),gh=_mm_mul_ps(W,QA(f->qg[1])),fz=_mm_mul_ps(W,QA(f->qg[2]));
   c=_mm_and_si128(_mm_castps_si128(_mm_or_ps(_mm_cmplt_ps(Z,zero),_mm_cmple_ps(W,zero))),QAI(f->kb[0]));
   c=_mm_or_si128(c,_mm_and_si128(_mm_castps_si128(_mm_cmpgt_ps(Z,fz)),QAI(f->kb[1])));
   c=_mm_or_si128(c,_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(X,_mm_sub_ps(zero,gw))),QAI(f->kb[2])));
   c=_mm_or_si128(c,_mm_and_si128(_mm_castps_si128(_mm_cmpgt_ps(X,gw)),QAI(f->kb[3])));
   c=_mm_or_si128(c,_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(Y,_mm_sub_ps(zero,gh))),QAI(f->kb[4])));
   c=_mm_or_si128(c,_mm_and_si128(_mm_castps_si128(_mm_cmpgt_ps(Y,gh)),QAI(f->kb[5])));}
  t=_mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(X),inf),inf);t=_mm_or_si128(t,_mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(Y),inf),inf));
  t=_mm_or_si128(t,_mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(Z),inf),inf));t=_mm_or_si128(t,_mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(W),inf),inf));
  t=_mm_or_si128(t,_mm_cmpeq_epi32(_mm_and_si128(_mm_castps_si128(rs),inf),inf));
  c=_mm_or_si128(_mm_andnot_si128(t,c),_mm_and_si128(t,QAI(f->k7f)));
  c=_mm_packus_epi16(_mm_packs_epi32(c,c),c);
  *(tnq_u32*)(f->cc+o)=(uint32_t)_mm_cvtsi128_si32(c);
  if(f->vc){const __m128 sm=QA(f->ksign);
   const __m128 a=_mm_or_ps(_mm_cmplt_ps(_mm_mul_ps(W,QA(f->qv[0])),_mm_andnot_ps(sm,X)),_mm_cmplt_ps(_mm_mul_ps(W,QA(f->qv[1])),_mm_andnot_ps(sm,Y)));
   QSA(f->vacc,_mm_or_ps(QA(f->vacc),a));}
  px=_mm_add_ps(_mm_mul_ps(X,r),QA(f->vp[0]));py=_mm_add_ps(_mm_mul_ps(Y,r),QA(f->vp[2]));
  pz=_mm_min_ps(_mm_add_ps(_mm_mul_ps(Z,r),QA(f->vp[4])),QA(f->qg[3]));
  {const __m128 t0=_mm_unpacklo_ps(px,py),t1=_mm_unpacklo_ps(pz,rs),t2=_mm_unpackhi_ps(px,py),t3=_mm_unpackhi_ps(pz,rs);const uint32_t ds=f->stride;uint8_t *d=f->rec+(size_t)o*ds;
   QSL(d,t0);QSL(d+8,t1);QSH(d+ds,t0);QSH(d+ds+8,t1);QSL(d+2*ds,t2);QSL(d+2*ds+8,t3);QSH(d+3*ds,t2);QSH(d+3*ds+8,t3);}
 }
}
/* the viewport's codes for places first .. first+n-1 (multiples of four) from the clip arrays: 1 X < -vxa W, 2 X > vxa W, 4 Y < -vya W,
 * 8 Y > vya W. Each is a plane through the eye, so three corners beyond the same one put the triangle beyond it whatever their w; a
 * position that is no number gets no code. Run only for a draw tnq_gpos found past a side (vacc). */
static void tnq_vcodes(tnk_frame *f) {
 uint32_t i;const __m128 zero=QA(f->vp[6]),ax=QA(f->qv[0]),ay=QA(f->qv[1]);
 for(i=0;i<f->n;i+=4){
  const uint32_t o=f->first+i;const __m128 X=QA(f->cx+o),Y=QA(f->cy+o),W=QA(f->cw+o),hx=_mm_mul_ps(W,ax),hy=_mm_mul_ps(W,ay);__m128i v;
  v=_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(X,_mm_sub_ps(zero,hx))),QAI(f->kvc[0]));
  v=_mm_or_si128(v,_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(hx,X)),QAI(f->kvc[1])));
  v=_mm_or_si128(v,_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(Y,_mm_sub_ps(zero,hy))),QAI(f->kvc[2])));
  v=_mm_or_si128(v,_mm_and_si128(_mm_castps_si128(_mm_cmplt_ps(hy,Y)),QAI(f->kvc[3])));
  v=_mm_packus_epi16(_mm_packs_epi32(v,v),v);
  *(tnq_u32*)(f->vc+o)=(uint32_t)_mm_cvtsi128_si32(v);
 }
}
/* the lanes' diffuse words for mesh-space normals nx ny nz: the colour without the lights and, for each of qnl directional lights,
 * max(N.L', 0) times its colour, all in units of a colour byte (the set-up multiplied by 255). With qnnorm the normals are made unit
 * length: 1/|N| as the reciprocal square root estimate and two Newton steps, y = r0 (1.5 - h r0 r0) with h half of N.N, then
 * y + y (0.5 - h y y): within about a unit in the last place, and 1 for a normal of length 1 (.541-.765: sqrtps and divps, 74 and
 * 72 cycles a group of four on the C7; rsqrtps 22 and ten operations of 2). A normal of length 0 gives no number, which max turns
 * into 0: no light, as before. With qvcol the vertex's diffuse word is the
 * material: its bytes times (qv0 + the lights' sum), plus qbase, and its alpha kept. A sum that is no number packs to 0. */
TNQ_INL __m128i tnq_lit_core(const tnk_frame *f,__m128 nx,__m128 ny,__m128 nz,__m128i dc,const int nn,const int vcol) { /* (nn and vcol constants at each call: a loop for each case, nothing chosen inside it; a product by 1 chosen at run time would also be a constant in memory) */
 const __m128 zero=QA(f->vp[6]);const uint32_t nl=f->qnl;uint32_t k;
 __m128 cb,cg,cr;__m128i ai;
 if(vcol){cb=QA(f->qv0[0]);cg=QA(f->qv0[1]);cr=QA(f->qv0[2]);}else{cb=QA(f->qbase[0]);cg=QA(f->qbase[1]);cr=QA(f->qbase[2]);}
 if(nl){
  __m128 rs=zero;
  if(nn){const __m128 d=_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,nx),_mm_mul_ps(ny,ny)),_mm_mul_ps(nz,nz)),r0=_mm_rsqrt_ps(d),half=QA(f->khalf),h=_mm_mul_ps(d,half);
   const __m128 y=_mm_mul_ps(r0,_mm_sub_ps(QA(f->q15),_mm_mul_ps(_mm_mul_ps(h,r0),r0)));
   rs=_mm_add_ps(y,_mm_mul_ps(y,_mm_sub_ps(half,_mm_mul_ps(_mm_mul_ps(h,y),y))));}
  for(k=0;k<nl;k++){
   __m128 t=_mm_add_ps(_mm_add_ps(_mm_mul_ps(nx,QA(f->ql[k][0])),_mm_mul_ps(ny,QA(f->ql[k][1]))),_mm_mul_ps(nz,QA(f->ql[k][2])));
   if(nn)t=_mm_mul_ps(t,rs);
   t=_mm_max_ps(t,zero);
   cb=_mm_add_ps(cb,_mm_mul_ps(t,QA(f->qc[k][0])));cg=_mm_add_ps(cg,_mm_mul_ps(t,QA(f->qc[k][1])));cr=_mm_add_ps(cr,_mm_mul_ps(t,QA(f->qc[k][2])));}
 }
 if(vcol){const __m128i ff=QAI(f->kff);
  cb=_mm_add_ps(QA(f->qbase[0]),_mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(dc,ff)),cb));
  cg=_mm_add_ps(QA(f->qbase[1]),_mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(dc,8),ff)),cg));
  cr=_mm_add_ps(QA(f->qbase[2]),_mm_mul_ps(_mm_cvtepi32_ps(_mm_and_si128(_mm_srli_epi32(dc,16),ff)),cr));
  ai=_mm_srli_epi32(dc,24);}
 else ai=QAI(f->qalpha);
 {__m128i p=_mm_packus_epi16(_mm_packs_epi32(_mm_cvtps_epi32(cb),_mm_cvtps_epi32(cr)),_mm_packs_epi32(_mm_cvtps_epi32(cg),ai)); /* b b b b r r r r g g g g a a a a */
  p=_mm_unpacklo_epi8(p,_mm_unpackhi_epi64(p,p)); /* b g b g b g b g r a r a r a r a */
  return _mm_unpacklo_epi16(p,_mm_unpackhi_epi64(p,p));} /* b g r a a lane */
}
/* a range's diffuse and specular words: qn vertices (a multiple of four) from qsrc, the normal at qinorm (read as tnq_pos reads a
 * position) and with qvcol the diffuse word at qidif; words 4 and 5 of each record at qdst (the specular word is qspec: no fog,
 * no specular lighting). With qmark, a group of four none of whose vertices a kept triangle uses is passed over. */
#define TNQ_LIT_LOOP(NN_,VC_) for(i=0;i<n;i+=4,s+=4*ss,d+=4*ds){ \
  __m128 nx=_mm_setzero_ps(),ny=nx,nz=nx;__m128i dc=_mm_setzero_si128(),w; \
  _mm_prefetch((const char*)(s+8*ss),_MM_HINT_T0);_mm_prefetch((const char*)(s+10*ss),_MM_HINT_T0); \
  if(nd&&!*(const tnq_u32*)(const void*)(nd+i))continue; \
  if(nl)TNQ_IN3(s+no,ss,nx,ny,nz) \
  if(VC_)dc=_mm_unpacklo_epi64(_mm_unpacklo_epi32(_mm_cvtsi32_si128((int)*(const tnq_u32*)(s+co)),_mm_cvtsi32_si128((int)*(const tnq_u32*)(s+ss+co))), \
   _mm_unpacklo_epi32(_mm_cvtsi32_si128((int)*(const tnq_u32*)(s+2*ss+co)),_mm_cvtsi32_si128((int)*(const tnq_u32*)(s+3*ss+co)))); \
  w=tnq_lit_core(f,nx,ny,nz,dc,NN_,VC_); \
  {const __m128 lo=_mm_castsi128_ps(_mm_unpacklo_epi32(w,sp)),hi=_mm_castsi128_ps(_mm_unpackhi_epi32(w,sp)); \
   QSL(d+16,lo);QSH(d+ds+16,lo);QSL(d+2*ds+16,hi);QSH(d+3*ds+16,hi);}}
static void tnq_lit(tnk_frame *f) {
 const uint8_t *s=f->qsrc,*nd=f->qmark;uint8_t *d=f->qdst;const uint32_t ss=f->qsstride,ds=f->qdstride,no=f->qinorm,co=f->qidif,nl=f->qnl,n=f->qn;uint32_t i;
 const __m128i sp=QAI(f->qspec);
 if(f->qvcol){if(f->qnnorm)TNQ_LIT_LOOP(1,1)else TNQ_LIT_LOOP(0,1)}
 else{if(f->qnnorm)TNQ_LIT_LOOP(1,0)else TNQ_LIT_LOOP(0,0)}
}
#undef TNQ_LIT_LOOP
/* the same for four gathered lanes (gnx gny gnz, gdc): od. The staged path's, so a vertex has the same word whichever path lit it. */
static void tnq_lit4(tnk_frame *f) {
 const __m128 nx=QA(f->gnx),ny=QA(f->gny),nz=QA(f->gnz);const __m128i dc=QAI(f->gdc);
 if(f->qvcol){if(f->qnnorm)QSAI(f->od,tnq_lit_core(f,nx,ny,nz,dc,1,1));else QSAI(f->od,tnq_lit_core(f,nx,ny,nz,dc,0,1));}
 else{if(f->qnnorm)QSAI(f->od,tnq_lit_core(f,nx,ny,nz,dc,1,0));else QSAI(f->od,tnq_lit_core(f,nx,ny,nz,dc,0,0));}
}
/* a range's words copied from the vertices: the diffuse word (qadm: 1 from the vertex at qado, 2 the constant qadc, 0 left as the
 * lighting wrote it), the specular word (qasm: 1 qasc, 2 the vertex's colour with alpha FF, 3 the vertex's whole word, 0 left), and
 * qans coordinate sets (u and v from qato, v only with qatv; a set the vertex lacks 0 0; a coordinate that is no number 0: the VxD
 * refuses those). Any count of vertices (qn); with qmark, only the vertices a kept triangle uses. The two common shapes (one or two
 * sets read whole) have loops of their own. */
#define TNQ_UV(src_,r_,w_) {uint32_t u_=*(const tnq_u32*)(src_),v_=*(const tnq_u32*)((src_)+4);if((u_&0x7F800000UL)==0x7F800000UL)u_=0;if((v_&0x7F800000UL)==0x7F800000UL)v_=0;(r_)[w_]=u_;(r_)[(w_)+1]=v_;}
static void tnq_attr(tnk_frame *f) {
 const uint8_t *s=f->qsrc,*nd=f->qmark;uint8_t *d=f->qdst;const uint32_t ss=f->qsstride,ds=f->qdstride,dm=f->qadm,sm=f->qasm,ns=f->qans,n=f->qn;uint32_t i,k;
 const uint32_t dco=f->qado,dcc=f->qadc,scc=f->qasc,t0=f->qato[0],t1=f->qato[1];
 const int one=ns==1&&t0!=0xFFFFFFFFUL&&f->qatv[0]&&f->qatd[0]==6,two=ns==2&&t0!=0xFFFFFFFFUL&&t1!=0xFFFFFFFFUL&&f->qatv[0]&&f->qatv[1]&&f->qatd[0]==6&&f->qatd[1]==8;
 if((one||two)&&sm<2){
#define TNQ_AL(D_,S_,T_) for(i=0;i<n;i++,s+=ss,d+=ds){tnq_u32 *r=(tnq_u32*)(void*)d;if(nd&&!nd[i])continue;D_ S_ TNQ_UV(s+t0,r,6) T_}
#define TNQ_AS(D_) {if(sm){if(two)TNQ_AL(D_,r[5]=scc;,TNQ_UV(s+t1,r,8))else TNQ_AL(D_,r[5]=scc;,)}else{if(two)TNQ_AL(D_,,TNQ_UV(s+t1,r,8))else TNQ_AL(D_,,)}}
  if(dm==1)TNQ_AS(r[4]=*(const tnq_u32*)(s+dco);)
  else if(dm==2)TNQ_AS(r[4]=dcc;)
  else TNQ_AS()
#undef TNQ_AS
#undef TNQ_AL
  return;}
 for(i=0;i<n;i++,s+=ss,d+=ds){
  tnq_u32 *r=(tnq_u32*)(void*)d;
  if(nd&&!nd[i])continue;
  if(dm==1)r[4]=*(const tnq_u32*)(s+dco);else if(dm==2)r[4]=dcc;
  if(sm==1)r[5]=scc;else if(sm==2)r[5]=(*(const tnq_u32*)(s+f->qaso)&0x00FFFFFFUL)|0xFF000000UL;else if(sm==3)r[5]=*(const tnq_u32*)(s+f->qaso);
  for(k=0;k<ns;k++){uint32_t u=0,v=0;const uint32_t to=f->qato[k];
   if(to!=0xFFFFFFFFUL){u=*(const tnq_u32*)(s+to);if(f->qatv[k])v=*(const tnq_u32*)(s+to+4);}
   if((u&0x7F800000UL)==0x7F800000UL)u=0;if((v&0x7F800000UL)==0x7F800000UL)v=0;
   r[f->qatd[k]]=u;r[f->qatd[k]+1]=v;}
 }
}
#undef TNQ_UV
/* the triangles kept of qtn index triples (uint16 a b c, qtist bytes apart, at qtidx): written back to back at qtout (which may be
 * qtidx: a kept triple never lands past its source), each kept triangle's corners marked in qtnd. Dropped: with qtvc, a triangle whose
 * three viewport codes share a bit (wholly beyond one side); with qtcm, by the area of the records' window positions,
 * (x1 - x0) (y2 - y0) - (y1 - y0) (x2 - x0) in doubles (the differences and products of floats are exact there, so the sign and a zero
 * are exact): zero always, above zero with qtcm 2, below with qtcm 3; an area that is no number is kept (vx_area_cull's answers).
 * qtrec, qtvc and qtnd are the record, code and mark of index value 0. No branch on a triangle's answer: the triple is written every
 * time and the output moves on by three or by nothing, and a dropped triangle's marks go to qttrash (as large as the marks; a
 * read-modify-write of a byte in memory takes 5.5 cycles on the C7, a store 1). qtkept: the count. */
#define TNQ_XY(i_,RS_) _mm_cvtps_pd(QLL(rec+(size_t)(i_)*(RS_)))
#define TNQ_CULL(RS_) for(;n;n--,q+=ist){ \
  const uint32_t a=*(const tnq_u16*)q,b=*(const tnq_u16*)(q+2),c=*(const tnq_u16*)(q+4);uint32_t keep=1;uint8_t *m; \
  if(vc)keep=(vc[a]&vc[b]&vc[c])==0; \
  if(cm){const __m128d p0=TNQ_XY(a,RS_),d1=_mm_sub_pd(TNQ_XY(b,RS_),p0),d2=_mm_sub_pd(TNQ_XY(c,RS_),p0); \
   const __m128d pr=_mm_mul_pd(d1,_mm_shuffle_pd(d2,d2,1)); /* (x1 - x0) (y2 - y0), (y1 - y0) (x2 - x0) */ \
   keep&=~(uint32_t)_mm_movemask_pd(_mm_cmple_sd(_mm_setzero_pd(),_mm_xor_pd(_mm_sub_sd(pr,_mm_unpackhi_pd(pr,pr)),flip)));} /* 0 <= area (its sign turned for qtcm 3): culled; no number: kept */ \
  *(tnq_u32*)(void*)o=a|(b<<16);o[2]=(uint16_t)c; \
  m=keep?nd:tr;m[a]=1;m[b]=1;m[c]=1; \
  o+=keep*3;}
static void tnq_cull(tnk_frame *f) {
 const uint8_t *q=f->qtidx,*rec=f->qtrec,*vc=f->qtvc;uint8_t *nd=f->qtnd,*tr=f->qttrash;tnq_u16 *o=(tnq_u16*)(void*)f->qtout;const tnq_u16 *o0=o;
 const uint32_t ist=f->qtist,rst=f->qtrst,cm=f->qtcm;uint32_t n=f->qtn;
 const __m128d flip=_mm_castsi128_pd(_mm_slli_epi64(_mm_cvtsi32_si128(cm==3),63));
 if(rst==32)TNQ_CULL(32)else if(rst==40)TNQ_CULL(40)else TNQ_CULL(rst)
 f->qtkept=(uint32_t)(o-o0)/3U;
}
#undef TNQ_CULL
#undef TNQ_XY
/* n indices from src to dst, each plus d (a packet's slots from a list's indices: the range copied in order) */
static void tnq_ixadd(uint16_t *dst,const uint8_t *src,uint32_t n,uint32_t d) {
 const __m128i dd=_mm_shufflelo_epi16(_mm_cvtsi32_si128((int)d),0);uint32_t i=0;
 for(;i+4<=n;i+=4)_mm_storel_epi64((__m128i*)(void*)(dst+i),_mm_add_epi16(_mm_loadl_epi64((const __m128i*)(const void*)(src+i*2)),dd)); /* 8 bytes at a time: neither end need be on 16 */
 for(;i<n;i++)((tnq_u16*)(void*)dst)[i]=(uint16_t)(((const tnq_u16*)(const void*)src)[i]+d);
}
/* n indices from src to dst through slot (by index value less bias: the packet slot of each kept vertex) */
static void tnq_ixmap(uint16_t *dst,const uint8_t *src,uint32_t n,const uint16_t *slot,uint32_t bias) {
 const uint16_t *const sb=slot-bias;uint32_t i; /* (the table's address for index value 0: no subtraction an index) */
 for(i=0;i<n;i++)((tnq_u16*)(void*)dst)[i]=sb[((const tnq_u16*)(const void*)src)[i]];
}
/* records into a packet's vertex words: nv records from rec (rst bytes apart), those marked in nd (all of them when nd is 0), each as
 * 8 words (x y z 1/w diffuse specular u v) or, form bit 0, as 10: the second set's two (form bit 1: the record's words 8 and 9) or
 * the first set's again. slot (when not 0) gets each copied record's packet slot, b0 on. Returns the records copied. By halves of a
 * register (8 bytes: neither a record nor a packet's vertex need be on 16; four loads and four stores a record, 12 cycles on the C7,
 * where eight words through a general register are 16 instructions and wait on each load). */
#define TNQ_PK8(r_,w_) {const __m128 a_=QLH(QLL(r_),(r_)+8),b_=QLH(QLL((r_)+16),(r_)+24);QSL(w_,a_);QSH((w_)+8,a_);QSL((w_)+16,b_);QSH((w_)+24,b_);}
static uint32_t tnq_pack(uint32_t *dst,const uint8_t *rec,uint32_t rst,const uint8_t *nd,uint32_t nv,uint32_t form,uint16_t *slot,uint32_t b0) {
 uint32_t q,cnt=0;const uint32_t ten=form&1U,o8=(form&2U)?32U:24U;uint8_t *w=(uint8_t*)(void*)dst;
 if(nd&&slot){uint32_t sl=b0; /* the usual call (marks and a slot table): a loop for each form, nothing but the mark tested in it */
  if(ten)for(q=0;q<nv;q++,rec+=rst){if(!nd[q])continue;TNQ_PK8(rec,w)QSL(w+32,QLL(rec+o8));w+=40;slot[q]=(uint16_t)sl;sl++;}
  else for(q=0;q<nv;q++,rec+=rst){if(!nd[q])continue;TNQ_PK8(rec,w)w+=32;slot[q]=(uint16_t)sl;sl++;}
  return sl-b0;}
 for(q=0;q<nv;q++,rec+=rst){
  if(nd&&!nd[q])continue;
  TNQ_PK8(rec,w)
  if(ten){QSL(w+32,QLL(rec+o8));w+=40;}else w+=32;
  if(slot)slot[q]=(uint16_t)(b0+cnt);
  cnt++;}
 return cnt;
}
#undef TNQ_PK8
/* o = a x b, 4x4 matrices of row vectors: row i of o is ((a[i][0] b0 + a[i][1] b1) + a[i][2] b2) + a[i][3] b3 with b's rows four wide
 * (the set-up's matrix products: 16 products and 12 sums; .536-.765 made each element on the x87, 64 and 48). o is neither a nor b. */
static void tnq_mm4(float *o,const float *a,const float *b) {
 const __m128 b0=_mm_loadu_ps(b),b1=_mm_loadu_ps(b+4),b2=_mm_loadu_ps(b+8),b3=_mm_loadu_ps(b+12);uint32_t i;
 for(i=0;i<16;i+=4)_mm_storeu_ps(o+i,_mm_add_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(a[i]),b0),_mm_mul_ps(_mm_set1_ps(a[i+1]),b1)),_mm_mul_ps(_mm_set1_ps(a[i+2]),b2)),_mm_mul_ps(_mm_set1_ps(a[i+3]),b3)));
}
#undef QA
#undef QAI
#undef QSA
#undef QSAI
#undef QLL
#undef QLH
#undef QSL
#undef QSH
/* TNK KERNELS END 3 */
