/* .544: four-wide transform and eligible lighting run in vx_d3d_tnl_soa.inc.
 * The scalar route handles clipped edges and state outside the grouped path.
 * tn_ix checks positions before it maps finished records to packet slots.
 * .536: the driver's own transform and lighting (included by vx_d3d_core.c before dp2_body). ACCEL_D3D_TNL (VxD key, default 0; the
 * feature under test, .534's probe grown up): the HAL offers hardware T&L (vx_d3d_hal.c: D3DDEVCAPS_HWTRANSFORMANDLIGHT, 8 lights,
 * VertexProcessingCaps 3F), so Direct3D 7's T&L HAL device and Direct3D 8's hardware vertex processing send the T&L commands and
 * untransformed vertices (DirectX 7 DDI), and this file does the vertex work:
 *  - the state: SETTRANSFORM (1 world, 2 view, 3 projection, 16-23 the texture matrices), SETMATERIAL, SETLIGHT (enable, disable, a
 *    D3DLIGHT7), ZRANGE, kept per context (tnl_of); CREATELIGHT and SETCLIPPLANE read past (no user clip planes are offered);
 *  - a draw of untransformed vertices (FVF position XYZ; XYZB* and points, lines and the _IMM commands are left out and counted): its
 *    triangles' vertices transformed into a scratch vertex buffer of pretransformed vertices (XYZRHW, DIFFUSE, SPECULAR, one or two
 *    coordinate sets), appended within the call, and the draw sent on as an indexed triangle list (26) into that buffer, through the same
 *    path as every pretransformed draw (templates, indexed packets, the VxD);
 *  - per vertex: position by world x view x projection; lighting (LIGHTING with normals: global ambient, emissive, directional and point
 *    lights with range and attenuation, spot lights as point lights (counted), material sources from COLORVERTEX and the *MATERIALSOURCE
 *    states; no normals: ambient and emissive only, as Direct3D does; specular lighting not done (counted)); vertex fog (FOGVERTEXMODE
 *    linear, exp, exp2 by eye depth or by range with RANGEFOGENABLE) into the specular alpha; the coordinate sets per stage: TEXCOORDINDEX's
 *    set or its generation (camera-space normal, position, reflection vector), then TEXTURETRANSFORMFLAGS's count with the stage's texture
 *    matrix ((u, v, 1, 0) for a set read from the vertex, (x, y, z, 1) for a generated one; PROJECTED divides by the last);
 *  - clipping: every triangle is tested against the six planes in clip space (near z >= 0, far z <= w, and since .543 x and y against
 *    w); one across them is clipped there (its new vertices interpolated before the divide and appended to the scratch buffer), one wholly
 *    outside a plane is dropped; a draw whose clipped output would not fit clips at near and far only, the rest by the core's screen
 *    clipper as for pretransformed draws;
 *  - for the draw, each active stage's TEXCOORDINDEX is set to its own output set and TEXTURETRANSFORMFLAGS to 0 (the pretransformed path
 *    reads them), and put back after it.
 * Counts (vx_d3d_tn, QueryPhases 290-301): [0] draws transformed, [1] their triangles, [2] vertices transformed, [3] triangles clipped,
 * [4] triangles dropped wholly outside, [5] vertices made by clipping, [6] scratch buffer restarts (the batch sent first), [7] draws left
 * out (unsupported command or FVF, a vertex past the buffer, too many triangles), [8] lit vertices, [9] fogged vertices, [10] vertices
 * with a generated or transformed coordinate set, [11] state not done (a spot light as a point light, specular lighting, a light index
 * past 15, a stage past the second's coordinates). */
uint32_t vx_d3d_tn[12];
typedef struct {uint32_t type,on,have;float dif[4],spe[4],amb[4],pos[3],dir[3],range,falloff,a0,a1,a2,theta,phi;} tnl_light;
/* .540 (review): the state holds no pointer (32-bit floats and words only: a recording's record 6 has one layout on Windows 98 and on a
 * 64-bit host; .536 kept the context's address in it and a host replay dropped it as the wrong size). The context's key is kept beside
 * it; vx_d3d_init gives a new context fresh state (vx_d3d_tnl_reset: a context made again at the same address inherited the old
 * matrices and lights), and the table holds more contexts than the HAL's 8 (a ninth evicted the first). */
typedef struct {float world[16],view[16],proj[16],tex[8][16],mat[17],zmin,zmax;tnl_light light[16];} tnl_state;
#define TNL_CTX 16
static tnl_state tnl_tab[TNL_CTX];static const void *tnl_key[TNL_CTX];static uint32_t tnl_next;
static void tnl_ident(float *m) {uint32_t k;for(k=0;k<16;k++)m[k]=(k%5)?0.0f:1.0f;}
static void tnl_fresh(tnl_state *t) {uint32_t k;memset(t,0,sizeof(*t));tnl_ident(t->world);tnl_ident(t->view);tnl_ident(t->proj);for(k=0;k<8;k++)tnl_ident(t->tex[k]);t->zmin=0.0f;t->zmax=1.0f;}
static tnl_state *tnl_of(const vx_d3d_context *c) {
 uint32_t k;tnl_state *t;
 for(k=0;k<TNL_CTX;k++)if(tnl_key[k]==c)return &tnl_tab[k];
 for(k=0;k<TNL_CTX&&tnl_key[k];k++){}
 if(k==TNL_CTX){k=tnl_next;tnl_next=(tnl_next+1)%TNL_CTX;}
 t=&tnl_tab[k];tnl_key[k]=c;tnl_fresh(t);return t;
}
void vx_d3d_tnl_reset(const vx_d3d_context *c) {uint32_t k;for(k=0;k<TNL_CTX;k++)if(tnl_key[k]==c)tnl_fresh(&tnl_tab[k]);}
/* the recorder (vx_d3d_rec.inc) keeps a context's T&L state with a recording; the replay puts it back (1: done, 0: the wrong size) */
uint32_t vx_d3d_tnl_get(const vx_d3d_context *c,void *o,uint32_t n) {tnl_state *t=tnl_of(c);if(n<sizeof(*t))return 0;memcpy(o,t,sizeof(*t));return sizeof(*t);}
uint32_t vx_d3d_tnl_set(const vx_d3d_context *c,const void *s,uint32_t n) {tnl_state *t=tnl_of(c);if(n!=sizeof(*t))return 0;memcpy(t,s,n);return 1;}
/* .543: reads and writes through the core's unaligned types and vx_fu (Watcom compiled each 4-byte memcpy as REP MOVSD and REP MOVSB with
 * their set-up: the GRID scene's tn_pos read x, y and z that way) */
static float tf(const uint8_t *p) {return *(const vx_f32a*)p;}
static uint32_t tn_bits(float x) {vx_fu u;u.f=x;return u.n;}
static float tn_flt(uint32_t n) {vx_fu u;u.n=n;return u.f;}
/* a T&L command into the state (the lengths were checked by the caller) */
static void tnl_command(vx_d3d_context *c,uint32_t op,const uint8_t *p,uint32_t n) {
 tnl_state *t=tnl_of(c);uint32_t j,k;
 if(op==36)for(j=0;j<n;j++){uint32_t ty=u32(p+j*68);float *m=ty==1?t->world:ty==2?t->view:ty==3?t->proj:ty>=16&&ty<24?t->tex[ty-16]:0;
  if(m)for(k=0;k<16;k++)m[k]=tf(p+j*68+4+k*4);}
 else if(op==33&&n)for(k=0;k<17;k++)t->mat[k]=tf(p+(n-1)*68+k*4);
 else if(op==34)for(j=0,k=0;j<n;j++){uint32_t i=u32(p+k),ty=u32(p+k+4);k+=8;
  if(i>=16){vx_d3d_tn[11]++;if(ty==2)k+=104;continue;}
  if(ty==0)t->light[i].on=1;else if(ty==1)t->light[i].on=0;
  else if(ty==2){tnl_light *l=&t->light[i];uint32_t q;l->type=u32(p+k);for(q=0;q<4;q++){l->dif[q]=tf(p+k+4+q*4);l->spe[q]=tf(p+k+20+q*4);l->amb[q]=tf(p+k+36+q*4);}
   for(q=0;q<3;q++){l->pos[q]=tf(p+k+52+q*4);l->dir[q]=tf(p+k+64+q*4);}
   l->range=tf(p+k+76);l->falloff=tf(p+k+80);l->a0=tf(p+k+84);l->a1=tf(p+k+88);l->a2=tf(p+k+92);l->theta=tf(p+k+96);l->phi=tf(p+k+100);l->have=1;k+=104;}}
 else if(op==32&&n){t->zmin=tf(p+(n-1)*8);t->zmax=tf(p+(n-1)*8+4);}
}
/* the square root without the C library (the host builds link none): a seed from the float's bits, three Newton steps (1e-7) */
static float tn_sqrt(float x) {float y;uint32_t i;if(!(x>0.0f))return 0.0f;i=0x1FBD1DF5UL+(tn_bits(x)>>1);y=tn_flt(i);y=0.5f*(y+x/y);y=0.5f*(y+x/y);y=0.5f*(y+x/y);return y;}
static void mmul(float *o,const float *a,const float *b) {uint32_t i,j;for(i=0;i<4;i++)for(j=0;j<4;j++)o[i*4+j]=a[i*4]*b[j]+a[i*4+1]*b[4+j]+a[i*4+2]*b[8+j]+a[i*4+3]*b[12+j];}
/* e^x for x <= 0 (the fog), within 1e-6: 2^(x log2 e) by its integer part and a polynomial for the rest */
/* .540 (review): a NaN failed both tests and reached the conversion to int (the x87 gives 0x80000000: a loop of 2^31 halvings, a hang):
 * only -80 < x < 0 goes on, so i is -116..-1 */
static float tn_exp(float x) {
 double y,f,r;int i;
 if(!(x>-80.0f))return 0.0f;if(!(x<0.0f))return 1.0f;
 y=(double)x*1.4426950408889634;i=(int)y;if((double)i>y)i--;f=y-i; /* 0 <= f < 1 */
 r=1.0+f*(0.6931471805599453+f*(0.2402265069591007+f*(0.0555041086648216+f*(0.0096181291076285+f*(0.0013333558146428+f*0.0001540353039338)))));
 /* .552: i is -116..-1. 2^i is an exactly representable normal float;
  * scaling this double by it is the same exact binary scaling as the old
  * loop of up to 116 halvings. Reuse the grouped fog kernel's exponent
  * construction without changing the polynomial or its rounding. */
 return (float)(r*(double)tn_flt((uint32_t)(i+127)<<23));
}
/* the scratch buffer: TN_MAX pretransformed vertices (at most 40 bytes), their clip-space positions and clip codes */
#define TN_MAX 16384 /* the HAL image stays under the VxD lock's 1024 pages (ACCEL_LOCK_HAL) */
#define TN_TRIS 8192 /* triangles in; .540: out at most 3 a triangle in (two planes cut it to a pentagon): tn_tok holds TN_OUT (.539 held
 * TN_TRIS and dropped the rest of a draw whose clipping made more) */
#define TN_OUT (TN_TRIS*3)
static uint8_t tn_buf[(TN_MAX+8)*40],tn_cc[TN_MAX+8];static uint16_t tn_tok[3+TN_OUT*3]; /* .544: eight places of slack: the group kernels write whole groups of four */
/* .540: tn_ref, the vertices a draw's triangles use (stamped with tn_epoch: only those are transformed, not every vertex between the
 * lowest and highest index); tn_uvq, a projected set's u, v and q before the divide (clipping interpolates them, then divides) */
static uint32_t tn_ref[TN_MAX],tn_epoch;static uint16_t tn_map[TN_MAX];static float tn_uvq[TN_MAX][2][3];static uint32_t tn_proj[2];
static uint32_t tn_P,tn_ostride,tn_sets,tn_fvf;
/* the call's own: the untransformed input (bound once) and the frame of the per-draw set-up */
typedef struct {float wvp[16],wv[16],nrm[9],amb[4],mdif[4],mamb[4],memi[4],ld[16][3],lpos[16][3],fs,fe,fd;uint32_t lit,fog,fmode,rfog,nl,li[16],ldir[16];
 uint32_t gen[2],idx[2],ttf[2],ins,inormal,idif,ispe,itex[8],itn[8],dsrc,asrc,esrc,mat_sources,nnorm;
 /* .542: nrm16, the normal matrix as tn_x4's 4x4 (rows padded, no translation); mconst: no material colour from the
  * vertex, so base4 (emissive + global ambient x ambient + every directional light's ambient x ambient, alpha the diffuse's) and per
  * light ld4 (diffuse x diffuse) and la4 (ambient x ambient) are the draw's: a lit vertex is base4 + the lights' terms (b g r a order) */
 uint32_t mconst,pointlight,gen_normal,gen_eye,fog_clip_w,lit_valid,lit_key[5],lit_color;float nrm16[16],base4[4],ld4[16][4],la4[16][4];
 /* .542: the viewport and depth range as scale and offset (tn_pos: x = vx0 + cx rhw vxs, y = vy0 + cy rhw vys, z = z0 + cz rhw zs) */
 float vx0,vxs,vy0,vys,z0,zs;
 tnl_state *ts; /* .543: the context's T&L state, found once a draw (tn_fin_to looked it up in the table for every vertex) */
} tn_setup;
static int tn_inf(float x) {return (tn_bits(x)&0x7F800000UL)==0x7F800000UL;} /* .538: NaN or infinity */
static void tn_col(uint32_t c,float *o) {o[0]=((c>>16)&255)/255.0f;o[1]=((c>>8)&255)/255.0f;o[2]=(c&255)/255.0f;o[3]=(c>>24)/255.0f;}
/* .542: SSE kernels; .543: the driver's T&L is SSE only. The HAL offers hardware T&L (ACCEL_D3D_TNL) only when the processor has SSE,
 * SSE2 and FXSR and the system saves the XMM registers (CR4 OSFXSR, the VxD's control 0xFFFFFE34, as VXGL.DLL checks): otherwise the
 * runtime's own T&L, as with the key off (the x87 copies of these steps removed). Three kernels, the same operations in the same order on
 * the board (Watcom #pragma aux) and on the host (SSE2 intrinsics), so the host's tests check the board's arithmetic:
 *  tn_x4  o[0..3] = v0 m[0..3] + v1 m[4..7] + v2 m[8..11] + m[12..15] (a position by a row-vector matrix: clip space, eye space, the
 *         normal by the padded normal matrix); every load and store unaligned (movups);
 *  tn_ma4 a[0..3] += s v[0..3];
 *  tn_pk4 four channels times k (255 for 0..1, 1 for 0..255), rounded (cvtps2dq, nearest), saturated to bytes (packssdw, packuswb: a
 *         NaN gives 0x80000000, so 0), lane 0 the lowest byte: b g r a in gives 0xAARRGGBB. */
static const float tn_255[4]={255.0f,255.0f,255.0f,255.0f},tn_one[4]={1.0f,1.0f,1.0f,1.0f};
#ifdef __WATCOMC__
/* the kernels as bytes (the HAL is built for the 486/x87 instruction set, where Watcom's assembler refuses SSE mnemonics): assembled
 * from movss/shufps/movups/mulps/addps (tn_x4: xmm0-3, tn_ma4: xmm0-2) and movups/mulps/cvtps2dq/packssdw/packuswb/movd (tn_pk4) by
 * Watcom with -6, the same registers as the parm lists */
static void tn_x4(const float *m,const float *v,float *o);
#pragma aux tn_x4 = 0xF3 0x0F 0x10 0x02 0x0F 0xC6 0xC0 0x00 0xF3 0x0F 0x10 0x4A 0x04 0x0F 0xC6 0xC9 0x00 0xF3 0x0F 0x10 0x52 0x08 0x0F 0xC6 0xD2 0x00 0x0F 0x10 0x18 0x0F 0x59 0xC3 0x0F 0x10 0x58 0x10 0x0F 0x59 0xCB 0x0F 0x58 0xC1 0x0F 0x10 0x58 0x20 0x0F 0x59 0xD3 0x0F 0x58 0xC2 0x0F 0x10 0x58 0x30 0x0F 0x58 0xC3 0x0F 0x11 0x01 parm [eax] [edx] [ecx] modify exact [];
static void tn_ma4(float *a,const float *s,const float *v);
#pragma aux tn_ma4 = 0xF3 0x0F 0x10 0x0A 0x0F 0xC6 0xC9 0x00 0x0F 0x10 0x01 0x0F 0x59 0xC1 0x0F 0x10 0x10 0x0F 0x58 0xD0 0x0F 0x11 0x10 parm [eax] [edx] [ecx] modify exact [];
static uint32_t tn_pk4(const float *bgra,const float *k255);
#pragma aux tn_pk4 = 0x0F 0x10 0x00 0x0F 0x10 0x0A 0x0F 0x59 0xC1 0x66 0x0F 0x5B 0xC0 0x66 0x0F 0x6B 0xC0 0x66 0x0F 0x67 0xC0 0x66 0x0F 0x7E 0xC0 parm [eax] [edx] value [eax] modify exact [eax];
#else
#include <emmintrin.h>
static void tn_x4(const float *m,const float *v,float *o) {__m128 a=_mm_set1_ps(v[0]),b=_mm_set1_ps(v[1]),d=_mm_set1_ps(v[2]);
 a=_mm_mul_ps(a,_mm_loadu_ps(m));b=_mm_mul_ps(b,_mm_loadu_ps(m+4));a=_mm_add_ps(a,b);d=_mm_mul_ps(d,_mm_loadu_ps(m+8));a=_mm_add_ps(a,d);
 a=_mm_add_ps(a,_mm_loadu_ps(m+12));_mm_storeu_ps(o,a);}
static void tn_ma4(float *a,const float *s,const float *v) {_mm_storeu_ps(a,_mm_add_ps(_mm_loadu_ps(a),_mm_mul_ps(_mm_loadu_ps(v),_mm_set1_ps(*s))));}
static uint32_t tn_pk4(const float *bgra,const float *k255) {__m128i i=_mm_cvtps_epi32(_mm_mul_ps(_mm_loadu_ps(bgra),_mm_loadu_ps(k255)));
 i=_mm_packs_epi32(i,i);i=_mm_packus_epi16(i,i);return (uint32_t)_mm_cvtsi128_si32(i);}
#endif
/* the draw's set-up: the matrices, the lights in eye space, the input FVF's layout, the stages' coordinate sets */
static int tn_prepare(vx_d3d_context *c,const vx_d3d_input *src,tn_setup *s) {
 tnl_state *t=tnl_of(c);vx_d3d_state *st=&c->state;uint32_t f=src->fvf,off=12,k,n,i;float a[16],det;
 memset(s,0,sizeof(*s));
 if((f&0xE)!=2)return 0; /* XYZ only */
 if(f&0x10){s->inormal=off;off+=12;}else s->inormal=0xFFFFFFFFUL;
 if(f&0x20)off+=4; /* PSIZE */
 if(f&0x40){s->idif=off;off+=4;}else s->idif=0xFFFFFFFFUL;
 if(f&0x80){s->ispe=off;off+=4;}else s->ispe=0xFFFFFFFFUL;
 n=(f>>8)&15;if(n>8)return 0;
 for(k=0;k<n;k++){uint32_t fm=(f>>(16+k*2))&3;s->itex[k]=off;s->itn[k]=fm==0?2:fm==1?3:fm==2?4:1;off+=s->itn[k]*4;}
 s->ins=n;if(src->vertex_stride<off)return 0;
 mmul(s->wv,t->world,t->view);mmul(s->wvp,s->wv,t->proj);
 /* the normal matrix: the inverse transpose of wv's 3x3 (a, cofactors) */
 a[0]=s->wv[5]*s->wv[10]-s->wv[6]*s->wv[9];a[1]=s->wv[6]*s->wv[8]-s->wv[4]*s->wv[10];a[2]=s->wv[4]*s->wv[9]-s->wv[5]*s->wv[8];
 a[3]=s->wv[2]*s->wv[9]-s->wv[1]*s->wv[10];a[4]=s->wv[0]*s->wv[10]-s->wv[2]*s->wv[8];a[5]=s->wv[1]*s->wv[8]-s->wv[0]*s->wv[9];
 a[6]=s->wv[1]*s->wv[6]-s->wv[2]*s->wv[5];a[7]=s->wv[2]*s->wv[4]-s->wv[0]*s->wv[6];a[8]=s->wv[0]*s->wv[5]-s->wv[1]*s->wv[4];
 det=s->wv[0]*a[0]+s->wv[1]*a[1]+s->wv[2]*a[2];if(det==0.0f)det=1.0f;
 for(k=0;k<9;k++)s->nrm[k]=a[k]/det; /* nrm[i*3+j] = cofactor C(i,j) / det: the inverse transpose. n'(j) = sum over i of n(i) nrm[i*3+j]
  * (row vectors, as positions); .540 (review): .536 summed along the rows (n x the inverse: a rotation turned the normals the wrong way) */
 s->nnorm=st->rs[143]!=0; /* .540: NORMALIZENORMALS: the transformed normal is made unit length only when it is on, as Direct3D does */
 s->lit=st->rs[137]!=0;
 s->dsrc=st->rs[145];s->esrc=st->rs[148];s->asrc=st->rs[147];
 for(k=0;k<4;k++){s->mdif[k]=t->mat[k];s->mamb[k]=t->mat[4+k];s->memi[k]=t->mat[12+k];}
 tn_col(st->rs[139],s->amb);
 if(s->lit){if(st->rs[29])vx_d3d_tn[11]++;
  for(i=0;i<16;i++){tnl_light *l=&t->light[i];if(!l->on||!l->have)continue;
   if(l->type==3){float x=-l->dir[0],y=-l->dir[1],z=-l->dir[2],e[3],m; /* to the light, in eye space by the view's 3x3 */
    e[0]=x*t->view[0]+y*t->view[4]+z*t->view[8];e[1]=x*t->view[1]+y*t->view[5]+z*t->view[9];e[2]=x*t->view[2]+y*t->view[6]+z*t->view[10];
    m=tn_sqrt(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]);if(m>0){e[0]/=m;e[1]/=m;e[2]/=m;}
    s->ld[s->nl][0]=e[0];s->ld[s->nl][1]=e[1];s->ld[s->nl][2]=e[2];s->ldir[s->nl]=1;}
   else{const float *v=t->view;if(l->type==2)vx_d3d_tn[11]++;
    s->lpos[s->nl][0]=l->pos[0]*v[0]+l->pos[1]*v[4]+l->pos[2]*v[8]+v[12];s->lpos[s->nl][1]=l->pos[0]*v[1]+l->pos[1]*v[5]+l->pos[2]*v[9]+v[13];
    s->lpos[s->nl][2]=l->pos[0]*v[2]+l->pos[1]*v[6]+l->pos[2]*v[10]+v[14];s->ldir[s->nl]=0;s->pointlight=1;}
   s->li[s->nl++]=i;}}
 /* .542: the per-draw parts (see tn_setup) */
 s->ts=t;
 {const vx3dn_rect *vp=&st->viewport;s->vxs=(float)(vp->right-vp->left)*0.5f;s->vx0=(float)vp->left+s->vxs;s->vys=-(float)(vp->bottom-vp->top)*0.5f;s->vy0=(float)vp->top-s->vys;
  s->z0=t->zmin;s->zs=t->zmax-t->zmin;}
 for(k=0;k<16;k++)s->nrm16[k]=0.0f;for(k=0;k<3;k++){s->nrm16[k*4]=s->nrm[k*3];s->nrm16[k*4+1]=s->nrm[k*3+1];s->nrm16[k*4+2]=s->nrm[k*3+2];}
 {
  const uint32_t vd=st->rs[141]&&s->idif!=0xFFFFFFFFUL?1:0;
  const uint32_t vs=st->rs[141]&&s->ispe!=0xFFFFFFFFUL?2:0;
  if(s->dsrc!=vd&&s->dsrc!=vs)s->dsrc=0;
  if(s->asrc!=vd&&s->asrc!=vs)s->asrc=0;
  if(s->esrc!=vd&&s->esrc!=vs)s->esrc=0;
  s->mat_sources=s->dsrc|s->asrc|s->esrc;
  s->mconst=!s->mat_sources;
 }
 if(s->lit&&s->mconst){static const uint32_t bgr[3]={2,1,0};
  for(k=0;k<3;k++)s->base4[k]=s->memi[bgr[k]]+s->amb[bgr[k]]*s->mamb[bgr[k]];s->base4[3]=s->mdif[3];
  for(i=0;i<s->nl;i++){const tnl_light *l=&t->light[s->li[i]];
   for(k=0;k<3;k++){s->ld4[i][k]=l->dif[bgr[k]]*s->mdif[bgr[k]];s->la4[i][k]=l->amb[bgr[k]]*s->mamb[bgr[k]];}s->ld4[i][3]=s->la4[i][3]=0.0f;
   if(s->ldir[i])for(k=0;k<3;k++)s->base4[k]+=s->la4[i][k];}}
 s->fog=st->rs[28]&&!st->rs[35]&&st->rs[140];s->fmode=st->rs[140];s->rfog=st->rs[48]!=0;
 s->fs=tf((const uint8_t*)&st->rs[36]);s->fe=tf((const uint8_t*)&st->rs[37]);s->fd=tf((const uint8_t*)&st->rs[38]);
 /* the output sets: one a stage up to the first disabled one, at most two */
 tn_sets=0;for(k=0;k<2;k++){if(st->tss[k][1]==1)break;s->gen[k]=st->tss[k][11]>>16;s->idx[k]=st->tss[k][11]&0xFFFF;s->ttf[k]=st->tss[k][24];tn_sets++;}
 if(!tn_sets){tn_sets=1;s->gen[0]=0;s->idx[0]=0;s->ttf[0]=0;}
 if(tn_sets>1&&st->tss[2][1]!=1)vx_d3d_tn[11]++;
 /* .540 (review): TEXTURETRANSFORMFLAGS' count is 0-4 (D3DTTFF_COUNT1-4) with PROJECTED (256); any other value is not a transform this
 * does (counted, the set passed through): .536 indexed o4[count - 1] with the count as given */
 for(k=0;k<tn_sets;k++){
  uint32_t cnt=s->ttf[k]&0xFF;
  if(cnt>4||(s->ttf[k]&~0x1FFUL)){vx_d3d_tn[11]++;s->ttf[k]=0;}
  tn_proj[k]=(s->ttf[k]&256)&&(s->ttf[k]&0xFF)>=2;
  if(s->gen[k]==1||s->gen[k]==3)s->gen_normal=1;
  if(s->gen[k]==2||s->gen[k]==3)s->gen_eye=1;
 }
 /* A standard perspective projection can leave clip W equal to eye Z.
  * Check the matrix columns bit for bit, including signs of zero, before
  * using the existing transform for depth fog. */
 if(s->fog&&!s->rfog&&!s->pointlight&&!s->gen_eye){
  s->fog_clip_w=1;
  for(k=0;k<16;k+=4)if(tn_bits(s->wvp[k+3])!=tn_bits(s->wv[k+2]))s->fog_clip_w=0;
 }
 /* .540 (review): XYZRHW | DIFFUSE | SPECULAR (C4) and the set count: .536 put the count over 1C4, which already says one set, so two
 * sets read as three (FVF 3C4, 48 bytes, against the records' 40: the core refused every two-stage draw, DDERR_INVALIDPARAMS) */
 tn_fvf=0xC4|(tn_sets<<8);tn_ostride=24+tn_sets*8;
 return 1;
}
/* .541: a vertex in two steps. tn_pos (every vertex a draw's triangles use, in tn_draw): the clip-space position and code (tn_cv, tn_cc)
 * and its source (tn_sp). tn_fin (once, when the vertex is first needed: entering a packet in tn_ix, an end of a clipped edge, a
 * triangle on the general path): lighting, fog, the coordinate sets, the divide and the viewport, the record into tn_buf. A vertex only
 * dropped triangles use is never lit (GTA3: 2.2 of 15.8 million triangles dropped wholly outside). tn_s and tn_c: the draw's set-up. */
static tn_setup tn_s;static vx_d3d_context *tn_c;static const uint8_t *tn_sp[TN_MAX+8];static uint8_t tn_done[TN_MAX+8];static uint32_t tn_lazy,tn_lz0;
/* .544: tn_pos (the one-vertex clip-space position, code and position words) is the group kernel tnk_pos (vx_d3d_tnl_soa.inc) */
#include "vx_d3d_tnl_soa.inc" /* .544: the group kernels, the frame, phases T and L */
static void tn_normal(const tn_setup *s,const uint8_t *v,float *nn) {
 tn_x4(s->nrm16,(const float*)(const void*)(v+s->inormal),nn);
 if(s->nnorm){
  float l=tn_sqrt(nn[0]*nn[0]+nn[1]*nn[1]+nn[2]*nn[2]);
  if(l>0){nn[0]/=l;nn[1]/=l;nn[2]/=l;}
 }
}
static uint32_t tn_light_color(const tn_setup *s,const float *e,const float *nn,int haveN,uint32_t dc,uint32_t sc) {
 float col[4],vb[4],vld[16][4],vla[16][4];
 const tnl_state *t=s->ts;
 const float *base=s->base4;
 const float (*ld)[4]=s->ld4,(*la)[4]=s->la4;
 uint32_t k;
  if(!s->mconst){
   static const uint32_t bgr[3]={2,1,0};
   float vd[4],vs[4];
   const float *md=s->dsrc==1?vd:s->dsrc==2?vs:s->mdif;
   const float *ma=s->asrc==1?vd:s->asrc==2?vs:s->mamb;
   const float *me=s->esrc==1?vd:s->esrc==2?vs:s->memi;
   uint32_t i;
   if(s->mat_sources&1)tn_col(dc,vd);
   if(s->mat_sources&2)tn_col(sc,vs);
   for(k=0;k<3;k++)vb[k]=me[bgr[k]]+s->amb[bgr[k]]*ma[bgr[k]];vb[3]=md[3];
   for(i=0;i<s->nl;i++){const tnl_light *l=&t->light[s->li[i]];
    for(k=0;k<3;k++){vld[i][k]=l->dif[bgr[k]]*md[bgr[k]];vla[i][k]=l->amb[bgr[k]]*ma[bgr[k]];}vld[i][3]=vla[i][3]=0.0f;
    if(s->ldir[i])for(k=0;k<3;k++)vb[k]+=vla[i][k];}
   base=vb;ld=(const float (*)[4])vld;la=(const float (*)[4])vla;}
  col[0]=base[0];col[1]=base[1];col[2]=base[2];col[3]=base[3];
  for(k=0;k<s->nl;k++){float ndl,att=1.0f;const float *L;float Lp[3];
   if(s->ldir[k])L=s->ld[k];
   else{const tnl_light *l=&t->light[s->li[k]];float dd,q;Lp[0]=s->lpos[k][0]-e[0];Lp[1]=s->lpos[k][1]-e[1];Lp[2]=s->lpos[k][2]-e[2];dd=tn_sqrt(Lp[0]*Lp[0]+Lp[1]*Lp[1]+Lp[2]*Lp[2]);
    if(dd>l->range)continue;if(dd>0){Lp[0]/=dd;Lp[1]/=dd;Lp[2]/=dd;}q=l->a0+l->a1*dd+l->a2*dd*dd;att=q>0?1.0f/q:1.0f;L=Lp;tn_ma4(col,&att,la[k]);}
   if(!haveN)continue;
   ndl=nn[0]*L[0]+nn[1]*L[1]+nn[2]*L[2];if(ndl<=0)continue;ndl*=att;tn_ma4(col,&ndl,ld[k]);}
 return tn_pk4(col,tn_255);
}

/* Without point lights, the diffuse result depends only on the normal and
 * material inputs. Reuse the last result within this draw, where the matrices,
 * light state and material are fixed. Fog still uses each vertex's depth. */
static uint32_t tn_diffuse(tn_setup *s,const uint8_t *v,const float *e,float *nn,int haveN,uint32_t dc,uint32_t sc) {
 uint32_t nx=0,ny=0,nz=0,dc_key,sc_key,lit;
 if(s->pointlight){
  if(haveN&&!s->gen_normal)tn_normal(s,v,nn);
  return tn_light_color(s,e,nn,haveN,dc,sc);
 }
 if(haveN&&s->nl){
  const uint8_t *p=v+s->inormal;
  nx=u32(p);ny=u32(p+4);nz=u32(p+8);
 }
 dc_key=(s->mat_sources&1)?dc:0;sc_key=(s->mat_sources&2)?sc:0;
 if(s->lit_valid&&s->lit_key[0]==nx&&s->lit_key[1]==ny&&s->lit_key[2]==nz&&
    s->lit_key[3]==dc_key&&s->lit_key[4]==sc_key)return s->lit_color;
 if(haveN&&s->nl&&!s->gen_normal)tn_normal(s,v,nn);
 lit=tn_light_color(s,e,nn,haveN,dc,sc);
 s->lit_key[0]=nx;s->lit_key[1]=ny;s->lit_key[2]=nz;s->lit_key[3]=dc_key;s->lit_key[4]=sc_key;
 s->lit_color=lit;s->lit_valid=1;
 return lit;
}

/* Finish the attributes once, on first use by a packet or by the clipper. */
static void tn_attributes(uint32_t o) {
 tn_setup *s=&tn_s;
 const uint8_t *v=tn_sp[o];
 const tnl_state *t=s->ts;
 float e[4]={0,0,0,0},nn[4]={0,0,0,0};
 uint32_t k,dc,sc,s32=0xFF000000UL;
 uint32_t *r=(uint32_t*)(void*)(tn_buf+o*tn_ostride);
 const int haveN=s->inormal!=0xFFFFFFFFUL;
 tn_done[o]=1;
 if(s->fog_clip_w)e[2]=tn_cw[o];
 else if(s->fog||s->pointlight||s->gen_eye)tn_x4(s->wv,(const float*)(const void*)v,e);
 if(haveN&&s->gen_normal)tn_normal(s,v,nn);
 dc=s->idif!=0xFFFFFFFFUL?u32(v+s->idif):0xFFFFFFFFUL;
 sc=s->ispe!=0xFFFFFFFFUL?u32(v+s->ispe):0;
 if(s->fog){
  float d=s->rfog?tn_sqrt(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]):(e[2]<0?-e[2]:e[2]);
  float sp[4]={0,0,0,1.0f};
  vx_d3d_tn[9]++;
  if(s->fmode==3)sp[3]=s->fe>s->fs?(s->fe-d)/(s->fe-s->fs):1.0f;
  else if(s->fmode==1)sp[3]=tn_exp(-s->fd*d);
  else{float u=s->fd*d;sp[3]=tn_exp(-u*u);}
  s32=tn_pk4(sp,tn_255);
 }
 if(s->lit){vx_d3d_tn[8]++;r[4]=tn_diffuse(s,v,e,nn,haveN,dc,sc);}
 else{r[4]=dc;s32|=sc&0x00FFFFFFUL;}
 r[5]=s32;
 for(k=0;k<tn_sets;k++){float in4[4],u=0,vv=0;uint32_t g=s->gen[k],cnt=s->ttf[k]&0xFF;
  if(!g&&!cnt){
   uint32_t u_bits=0,v_bits=0;
   if(s->idx[k]<s->ins){
    const uint32_t idx=s->idx[k];
    const uint8_t *q=v+s->itex[idx];
    u_bits=u32(q);if(s->itn[idx]>1)v_bits=u32(q+4);
   }
   r[6+k*2]=(u_bits&0x7FFFFFFFUL)>=0x7F800000UL?0:u_bits;
   r[7+k*2]=(v_bits&0x7FFFFFFFUL)>=0x7F800000UL?0:v_bits;
   continue;
  }
  if(g==1&&haveN){in4[0]=nn[0];in4[1]=nn[1];in4[2]=nn[2];in4[3]=1;}
  else if(g==2){in4[0]=e[0];in4[1]=e[1];in4[2]=e[2];in4[3]=1;}
  else if(g==3){float E[3],l=tn_sqrt(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]),ne;
   if(tn_c->state.rs[142]&&l>0){E[0]=-e[0]/l;E[1]=-e[1]/l;E[2]=-e[2]/l;}else{E[0]=0;E[1]=0;E[2]=-1;}
   ne=nn[0]*E[0]+nn[1]*E[1]+nn[2]*E[2];in4[0]=2*ne*nn[0]-E[0];in4[1]=2*ne*nn[1]-E[1];in4[2]=2*ne*nn[2]-E[2];in4[3]=1;}
  else if(s->idx[k]<s->ins){const uint8_t *q=v+s->itex[s->idx[k]];in4[0]=tf(q);in4[1]=s->itn[s->idx[k]]>1?tf(q+4):0;in4[2]=1;in4[3]=0;g=0;}
  else{in4[0]=0;in4[1]=0;in4[2]=1;in4[3]=0;}
  if(g||cnt)vx_d3d_tn[10]++;
  if(cnt){const float *M=t->tex[k];float o4[4];uint32_t j;for(j=0;j<4;j++)o4[j]=in4[0]*M[j]+in4[1]*M[4+j]+in4[2]*M[8+j]+in4[3]*M[12+j];
   u=o4[0];vv=cnt>1?o4[1]:0;
   if(tn_proj[k]){float qq=o4[cnt-1];tn_uvq[o][k][0]=u;tn_uvq[o][k][1]=vv;tn_uvq[o][k][2]=qq;if(qq!=0.0f){u/=qq;vv/=qq;}}} /* cnt 2-4 here */
  else{u=in4[0];vv=in4[1];}
  if(tn_inf(u))u=0;if(tn_inf(vv))vv=0; /* .540 (review): the VxD refuses a non-finite coordinate (vx_native3d_hw.c); the core does not check them */
  r[6+k*2]=tn_bits(u);r[7+k*2]=tn_bits(vv);}
}
static void tn_fin(uint32_t o) {if(!tn_done[o])tn_attributes(o);}
/* Keep the normal float interpolation. Opposite finite endpoints can
 * overflow the float difference even though the interpolant is finite. */
static float tn_lerp_float(float a,float b,float t) {
 float value=a+(b-a)*t;
 if(tn_inf(value)&&!tn_inf(a)&&!tn_inf(b)&&!tn_inf(t))
  value=(float)((double)a+((double)b-(double)a)*(double)t);
 return value;
}
/* a vertex made by clipping, between scratch vertices a and b at t (clip space, before the divide) */
static uint32_t tn_lerp(vx_d3d_context *c,uint32_t a,uint32_t b,float tt,uint32_t pl) {
 uint32_t o=tn_P++,k;const uint8_t *A=tn_buf+a*tn_ostride,*B=tn_buf+b*tn_ostride;uint8_t *w=tn_buf+o*tn_ostride;float p[4],rw,sx,sy,sz,l0,h0;
 (void)c;
 p[0]=tn_lerp_float(tn_cx[a],tn_cx[b],tt);
 p[1]=tn_lerp_float(tn_cy[a],tn_cy[b],tt);
 p[2]=tn_lerp_float(tn_cz[a],tn_cz[b],tt);
 p[3]=tn_lerp_float(tn_cw[a],tn_cw[b],tt);
 /* on the plane exactly (rounding put the vertex a little outside it); .543: the six planes */
 if(pl==0)p[2]=0.0f;else if(pl==1)p[2]=p[3];else if(pl==2)p[0]=-p[3];else if(pl==3)p[0]=p[3];else if(pl==4)p[1]=-p[3];else p[1]=p[3];
 tn_cx[o]=p[0];tn_cy[o]=p[1];tn_cz[o]=p[2];tn_cw[o]=p[3];rw=p[3]>0?1.0f/p[3]:0.0f;tn_cc[o]=0;
 /* .543: as tn_pos, then held inside the viewport and the depth range (1/w's rounding put a vertex on an edge just past it: the packet's
  * check would have sent its triangle to the per-triangle path) */
 sx=tn_s.vx0+p[0]*rw*tn_s.vxs;sy=tn_s.vy0+p[1]*rw*tn_s.vys;sz=tn_s.z0+p[2]*rw*tn_s.zs;
 l0=tn_s.vx0-tn_s.vxs;h0=tn_s.vx0+tn_s.vxs;if(sx<l0)sx=l0;if(sx>h0)sx=h0;
 l0=tn_s.vy0+tn_s.vys;h0=tn_s.vy0-tn_s.vys;if(sy<l0)sy=l0;if(sy>h0)sy=h0;
 l0=tn_s.zs<0?tn_s.z0+tn_s.zs:tn_s.z0;h0=tn_s.zs<0?tn_s.z0:tn_s.z0+tn_s.zs;if(sz<l0)sz=l0;if(sz>h0)sz=h0;
 if(tn_bits(sx)==0x80000000UL)sx=0.0f;if(tn_bits(sy)==0x80000000UL)sy=0.0f;if(tn_bits(sz)==0x80000000UL)sz=0.0f;
 *(vx_f32a*)w=sx;*(vx_f32a*)(w+4)=sy;*(vx_f32a*)(w+8)=sz;*(vx_f32a*)(w+12)=rw;
 for(k=16;k<24;k+=4){float f4[4];uint32_t j;for(j=0;j<4;j++){float x=(float)A[k+j],y=(float)B[k+j];f4[j]=x+(y-x)*tt;}*(vx_u32a*)(w+k)=tn_pk4(f4,tn_one);} /* the colour bytes (.543: tn_pk4, as 0..255) */
 for(k=24;k<tn_ostride;k+=4){
  float v=tn_lerp_float(tf(A+k),tf(B+k),tt);
  *(vx_f32a*)(w+k)=tn_inf(v)?0.0f:v;
 }
 /* .540 (review): a projected set: u, v and q interpolated, then divided (the quotients interpolated were wrong across the cut) */
 for(k=0;k<tn_sets;k++)if(tn_proj[k]){float u,v,q;uint32_t j;for(j=0;j<3;j++)tn_uvq[o][k][j]=tn_lerp_float(tn_uvq[a][k][j],tn_uvq[b][k][j],tt);
  u=tn_uvq[o][k][0];v=tn_uvq[o][k][1];q=tn_uvq[o][k][2];if(q!=0.0f){u/=q;v/=q;}if(tn_inf(u))u=0;if(tn_inf(v))v=0;*(vx_f32a*)(w+24+k*8)=u;*(vx_f32a*)(w+28+k*8)=v;}
tn_done[o]=1;vx_d3d_tn[5]++;return o;
}
/* a triangle across planes it crosses (u: its vertices' codes together; .543: the side planes as well as near and far): clipped
 * (Sutherland-Hodgman, each plane in clip space; a vertex made by clipping lies inside every plane its ends are), the fan's triangles into
 * the token. At most 9 vertices (3 + one a plane), 7 triangles. */
static float tn_pd(uint32_t pl,uint32_t i) {return pl==0?tn_cz[i]:pl==1?tn_cw[i]-tn_cz[i]:pl==2?tn_cx[i]+tn_cw[i]:pl==3?tn_cw[i]-tn_cx[i]:pl==4?tn_cy[i]+tn_cw[i]:tn_cw[i]-tn_cy[i];}
/* Finite clip coordinates can overflow the float plane distance or
 * the distance difference. Rebuild the affected crossing in double. */
static double tn_pd_wide(uint32_t pl,uint32_t i) {
 const double w=tn_cw[i];
 switch(pl){
 case 0:return tn_cz[i];
 case 1:return w-tn_cz[i];
 case 2:return w+tn_cx[i];
 case 3:return w-tn_cx[i];
 case 4:return w+tn_cy[i];
 default:return w-tn_cy[i];
 }
}
static float tn_clip_fraction(uint32_t pl,uint32_t i,uint32_t j,float di,float dj) {
 const float span=di-dj;
 if(tn_inf(span)){
  const double a=tn_pd_wide(pl,i),b=tn_pd_wide(pl,j);
  return (float)(a/(a-b));
 }
 return di/span;
}
static uint32_t tn_clip(vx_d3d_context *c,uint32_t a,uint32_t b,uint32_t d,uint16_t *ix,uint32_t m,uint32_t u) {
 static const uint8_t bit[6]={1,2,8,16,32,64};
 uint32_t pa[9],pb[9],na=3,pl,k;
 pa[0]=a;pa[1]=b;pa[2]=d;
 for(pl=0;pl<6;pl++)if(u&bit[pl]){
  uint32_t i=pa[0],nb=0,crossings=0;
  float di=tn_pd(pl,i);
  for(k=0;k<na;k++){
   const uint32_t j=pa[k+1<na?k+1:0];
   const float dj=tn_pd(pl,j);
   if(di>=0){if(nb==9)return m;pb[nb++]=i;}
   if((di>0&&dj<0)||(di<0&&dj>0)){
    /* Two crossings per plane bound scratch growth and token expansion.
     * Stop even if extreme coordinates violate convexity after rounding. */
    if(nb==9||++crossings>2)return m;
    tn_fin(i);tn_fin(j);
    pb[nb++]=tn_lerp(c,i,j,tn_clip_fraction(pl,i,j,di,dj),pl);
   }
   i=j;di=dj;
  }
  if(nb<3)return m;
  for(k=0;k<nb;k++)pa[k]=pb[k];
  na=nb;
 }
 for(k=1;k+1<na;k++,m++,ix+=3){
  ix[0]=(uint16_t)pa[0];ix[1]=(uint16_t)pa[k];ix[2]=(uint16_t)pa[k+1];
 }
 return m;
}

/* 9 bits for the packet slot, 7 for the stamp. The table is invalidated
 * on scratch restart and whenever ix_new changes the packet mapping. */
#define TN_SLOT_STEP 512
#if VX3DN_IX_VERTICES > TN_SLOT_STEP
#error T&L packet slots do not fit
#endif
static uint16_t tn_slot[TN_MAX];
static uint32_t tn_slot_tag,tn_slot_g;
static void tn_slot_reset(void) {
 tn_slot_g=ix_g;
 tn_slot_tag+=TN_SLOT_STEP;
 if(tn_slot_tag==0x10000UL){
  memset(tn_slot,0,sizeof(tn_slot));
  tn_slot_tag=TN_SLOT_STEP;
 }
}
/* a draw command of untransformed vertices: its triangles' vertices transformed into the scratch buffer and the command rewritten as an
 * indexed triangle list into it (tn_tok: header, base 0, indices). 1: done (*op, *n, *p set to the list), 0: the draw left out (counted),
 * 2: the scratch buffer is full (the caller sends the batch and calls again with tn_P 0). */
uint32_t vx_d3d_tn_snap[2];
static uint16_t tn_need[TN_MAX+8];
static uint8_t tn_nd[TN_MAX+8];
static int tn_draw(vx_d3d_context *c,const vx_d3d_input *src,uint32_t *op,uint32_t *n,const uint8_t **p) {
 uint32_t o=*op,cnt=*n,base=0,lo=0xFFFFFFFFUL,hi=0,i,m=0,nv,need,mask,used=0,P0;const uint8_t *data=*p;uint16_t *tri;int soa;
 if(o!=18&&o!=19&&o!=20&&o!=21&&o!=22&&o!=26&&o!=3){vx_d3d_tn[7]++;return 0;}
 if(o!=3){base=u16(data);data+=2;}
 if(!cnt||cnt>TN_TRIS){vx_d3d_tn[7]++;return 0;}
 /* Stage the source-relative triples at the tail of the output token.
  * Reserve the space before it for all potential expansion. */
 tri=tn_tok+3+(TN_OUT-cnt)*3;
 if(o==26){const uint32_t ni=cnt*3;uint32_t a; /* .548 (.547 review): INDEXEDTRIANGLELIST2's indices are back to back: copied once
  * into the snapshot with the bounds in the same pass (tri_fill widened them to 32 bits in tri3, then they were read back and narrowed);
  * the same triples. vx_d3d_tn_snap: [0] tokens, [1] indices (QueryPhases 314-315) */
  for(i=0;i<ni;i++){a=u16(data+i*2);if(a<lo)lo=a;if(a>hi)hi=a;tri[i]=(uint16_t)a;}
  vx_d3d_tn_snap[0]++;vx_d3d_tn_snap[1]+=ni;}
 else for(i=0;i<cnt;i+=VX_TRI_CHUNK){
  const uint32_t chunk=cnt-i<VX_TRI_CHUNK?cnt-i:VX_TRI_CHUNK;
  uint32_t j;
  tri_fill(o,data,0,i,chunk);
  for(j=0;j<chunk*3;j++){
   const uint32_t idx=tri3[j];
   if(idx<lo)lo=idx;
   if(idx>hi)hi=idx;
   tri[i*3+j]=(uint16_t)idx;
  }
 }
 nv=hi-lo+1;hi+=base; /* the staged indices and lo are still relative to base */
 need=(nv<3*cnt?nv:3*cnt)+cnt*4; /* the vertices used (at most 3 a triangle, at most the span), then each clipped triangle makes at most four */
 if(!src->vertices||!src->vertex_stride||hi>=src->vertex_bytes/src->vertex_stride||nv>TN_MAX||need>TN_MAX){vx_d3d_tn[7]++;return 0;}
 {uint32_t was=tn_ostride;if(!tn_prepare(c,src,&tn_s)){vx_d3d_tn[7]++;return 0;}tn_c=c;
  if(tn_P&&tn_ostride!=was)return 2;} /* one record size in the buffer: a state command between draws changed the stages' sets */
 if(tn_P+need>TN_MAX)return 2;
 soa=tn_soa_prepare();
 /* .540 (review): only the vertices the triangles use are transformed (stamped). .541: and they take the scratch buffer's next places in
  * the order the triangles first use them (tn_map: source index less lo to place), so a draw's vertices are dense there. .544: gathered
  * into the staging arrays (tn_sx tn_sy tn_sz, a chunk of TN_CH places), phase T over each chunk in groups of four */
 if(++tn_epoch==0){memset(tn_ref,0,sizeof(tn_ref));tn_epoch=1;}
 P0=tn_P;
 if(!P0)tn_slot_reset();
 for(i=0;i<cnt*3;i++){const uint32_t qq=tri[i]-lo;
  if(tn_ref[qq]!=tn_epoch){const uint8_t *v=src->vertices+(base+tri[i])*src->vertex_stride;const uint32_t pl=P0+used,ln=used&(TN_CH-1);
   tn_ref[qq]=tn_epoch;tn_map[qq]=(uint16_t)pl;tn_sp[pl]=v;tn_sx[ln]=tf(v);tn_sy[ln]=tf(v+4);tn_sz[ln]=tf(v+8);used++;
   if(ln==TN_CH-1)tn_soa_pos(pl+1-TN_CH,TN_CH);
  }
  tri[i]=tn_map[qq];
 }
 if(used&(TN_CH-1))tn_soa_pos(P0+(used&~(TN_CH-1)),used&(TN_CH-1));
 vx_d3d_tn[0]++;vx_d3d_tn[1]+=cnt;vx_d3d_tn[2]+=used;tn_P+=used;tn_lz0=P0;
 /* .543: the side planes clipped when the room allows (each triangle clipped there makes at most 12 vertices and 7 triangles); else near
  * and far only, as before (the room for that was checked above), the rest to the per-triangle path */
 mask=0x7F;
 /* For small draws the worst-case reservation already covers every
  * triangle. Count crossings only when that bound will not fit. */
 if(tn_P+cnt*12>TN_MAX||cnt*7>TN_OUT){
  uint32_t nc=0;
  for(i=0;i<cnt;i++){const uint32_t x=tri[i*3],y=tri[i*3+1],z=tri[i*3+2],u=tn_cc[x]|tn_cc[y]|tn_cc[z];
   if(u&&!(tn_cc[x]&tn_cc[y]&tn_cc[z]))nc++;}
  if(tn_P+nc*12>TN_MAX||cnt+nc*6>TN_OUT)mask=0x07;}
 /* the triangles: kept whole (their places marked for phase L), dropped, or clipped (their vertices finished one by one first, as the
  * clipper interpolates finished records) */
 if(soa)memset(tn_nd+P0,0,used);
 {uint16_t *ix=tn_tok+3;uint32_t nneed=0;
  /* Each source triple is loaded before its output is written. Six
   * extra triples per crossing fit ahead of the unread tail. For the
   * near/far fallback, TN_OUT already reserves two extras per input. */
  for(i=0;i<cnt;i++){const uint32_t x=tri[i*3],y=tri[i*3+1],z=tri[i*3+2],u=(tn_cc[x]|tn_cc[y]|tn_cc[z])&mask;
   if(!u){if(m>=TN_OUT){vx_d3d_tn[7]++;break;}ix[m*3]=(uint16_t)x;ix[m*3+1]=(uint16_t)y;ix[m*3+2]=(uint16_t)z;m++;
    if(soa){
     if(!tn_nd[x]){tn_nd[x]=1;tn_need[nneed++]=(uint16_t)x;}
     if(!tn_nd[y]){tn_nd[y]=1;tn_need[nneed++]=(uint16_t)y;}
     if(!tn_nd[z]){tn_nd[z]=1;tn_need[nneed++]=(uint16_t)z;}
    }}
   else if((tn_cc[x]&tn_cc[y]&tn_cc[z]&0x7F)||(u&4))vx_d3d_tn[4]++; /* .538: 4, a position not finite; .543: wholly outside any plane */
   else{vx_d3d_tn[3]++;m=tn_clip(c,x,y,z,ix+m*3,m,u);}}
  /* phase L: the kept triangles' vertices not yet finished (a clipped edge's end may be) */
  if(soa){
   uint32_t w=0;
   for(i=0;i<nneed;i++)if(!tn_done[tn_need[i]])tn_need[w++]=tn_need[i];
   tn_soa_finish(tn_need,w);
  }
 }
 /* A fully rejected draw owns no records needed by DP2. */
 tn_lazy=m!=0;
 tn_tok[0]=26;tn_tok[1]=(uint16_t)m;tn_tok[2]=0;
 *op=26;*n=m;*p=(const uint8_t*)(tn_tok+2);
 return 1;
}
/* .541: every vertex of the draw finished (the paths that read the scratch records as they are: the per-triangle path, a draw not on
 * the indexed path) */
static void tn_fin_all(void) {uint32_t o;for(o=tn_lz0;o<tn_P;o++)tn_fin(o);tn_lazy=0;}
/* .541: the driver's T&L draw into the open indexed packet (the fused path: ix_list's work for the scratch buffer's triangles, without
 * ix_pass's span pass and without the source checks a vertex buffer needs). Each vertex is lit when it first enters a packet (tn_fin),
 * its words written there once (fast_vertex_pre_w, the same words as before); a vertex whose position is outside the guard band or
 * the depth range, or whose rhw is not positive and finite, stops it (return: that triangle and on go back to the caller's per-triangle
 * path, which clips it on the screen). The records are the T&L's own (finite coordinates, tn_vertex), so no vv_good. */
static uint32_t tn_ix(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint8_t *q,uint32_t ist,uint32_t base,uint32_t n,uint32_t *completed,uint32_t *rc) {
 const uint8_t *const vb=in->vertices;const uint32_t st=in->vertex_stride,cull=c->state.rs[22];
 const int noarea=cull==1&&vx_d3d_ixpass;
 uint32_t k=0,drawn=0,culled=0,newv=0;int cur;
 /* Grouped lighting ran in tn_draw. Scalar attributes are finished when
  * a vertex first enters a packet. Plain packets can copy the record. */
 const uint32_t ten=tn_sets>1;
 /* .542: plain, the template takes the record's words as they are; otherwise (ZBIAS, snapping, another layout) fast_vertex_pre_w converts
  * them as before (.544 first copied the words in every case: ZBIAS lost) */
 const int two=tc.tex&&tc.units>1;
 const int plain=tc.lay.coff==16&&tc.lay.soff==20&&tc.prog>=0&&tc.tex&&tc.lay.t0==24&&tc.lay.n0>=2&&tc.zb==0&&!tri_snap&&
  (two?tn_sets==2&&tc.lay.t1==32&&tc.lay.n1>=2:tn_sets==1);
 *rc=0;
 cur=bq_on&&bq_tid==tc.id&&bq.pre.on==2&&ix_vb==vb&&ix_stride==st&&ix_fvf==in->fvf;
 if(cur&&tn_slot_g!=ix_g)tn_slot_reset();
 for(;k<n;k++,q+=ist) {
  const uint32_t i0=base+u16(q),i1=base+u16(q+2),i2=base+u16(q+4);
  uint32_t l0=IX_NONE,l1=IX_NONE,l2=IX_NONE;uint16_t *ix;
  if(cur){
   const uint32_t a=tn_slot[i0]^tn_slot_tag,b=tn_slot[i1]^tn_slot_tag,d=tn_slot[i2]^tn_slot_tag;
   if(a<VX3DN_IX_VERTICES)l0=a;
   if(b<VX3DN_IX_VERTICES)l1=b;
   if(d<VX3DN_IX_VERTICES)l2=d;
  }
  /* .543's test, on the record (the words of a kept triangle are inside the viewport and the depth range when the side planes were
   * clipped; not when the room allowed near and far only, and a vertex made by clipping can have a 1/w of 0 or not finite): such a
   * triangle and the rest go back to the caller's per-triangle path, which clips them on the screen (tested before a packet is opened:
   * the per-triangle path then adds to the open one) */
#define TN_OK(i) (GB_IN(u32(tn_buf+(i)*tn_ostride),tc.bxl,tc.bxr)&&GB_IN(u32(tn_buf+(i)*tn_ostride+4),tc.byt,tc.byb)&&u32(tn_buf+(i)*tn_ostride+8)<=0x3F800000UL&&u32(tn_buf+(i)*tn_ostride+12)-1UL<0x7F7FFFFFUL)
  if((l0==IX_NONE&&!TN_OK(i0))||(l1==IX_NONE&&!TN_OK(i1))||(l2==IX_NONE&&!TN_OK(i2)))break;
#undef TN_OK
  if(!noarea){double area;const vx_f32a *const f0=(const vx_f32a *)(vb+i0*st),*const f1=(const vx_f32a *)(vb+i1*st),*const f2=(const vx_f32a *)(vb+i2*st);
   area=((double)f1[0]-f0[0])*((double)f2[1]-f0[1])-((double)f1[1]-f0[1])*((double)f2[0]-f0[0]);
   if(area==0||(cull==2&&area>0)||(cull==3&&area<0)){culled++;continue;}}
  else if(i0==i1||i1==i2||i0==i2){culled++;continue;}
  if(!cur||bq.vertex_count+3>VX3DN_IX_VERTICES||ix_ni+3>VX3DN_MAX_INDICES) {
   const uint32_t g=ix_g;
   if(ix_open(c,in,sv,3,1,completed,rc))break;
   if(ix_g!=g||tn_slot_g!=ix_g){tn_slot_reset();l0=l1=l2=IX_NONE;}
   cur=1;}
#define TN_PUT(i,l) do{const uint8_t *p_=tn_buf+(i)*tn_ostride;const uint32_t *r_=(const uint32_t*)(const void*)p_;uint32_t *w_;l=bq.vertex_count++;tn_slot[i]=(uint16_t)(tn_slot_tag|l);tn_fin(i);w_=IXW(l); \
 if(plain){w_[0]=r_[0];w_[1]=r_[1];w_[2]=r_[2];w_[3]=r_[3];w_[4]=r_[4];w_[5]=r_[5];w_[6]=r_[6];w_[7]=r_[7];if(!(bq.bursts.b[0]&VX3DN_IX_V8)){w_[8]=r_[6+2*ten];w_[9]=r_[7+2*ten];}} \
 else fast_vertex_pre_w(w_,p_,0xFFFFFFFFUL,p_);newv++;}while(0)
  if(l0==IX_NONE)TN_PUT(i0,l0);
  if(l1==IX_NONE)TN_PUT(i1,l1);
  if(l2==IX_NONE)TN_PUT(i2,l2);
#undef TN_PUT
  ix=VX3DN_IDX(&bq)+ix_ni;ix[0]=(uint16_t)l0;ix[1]=(uint16_t)l1;ix[2]=(uint16_t)l2;
  ix_ni+=3;bq.bursts.b[0]=(bq.bursts.b[0]&0xFFFF0000UL)|ix_ni;drawn++;
 }
 vx_d3d_batched+=drawn;vx_d3d_ix[0]+=drawn;vx_d3d_ix[1]+=newv;vx_d3d_tk[8]+=k;vx_d3d_tk[9]+=culled;
 if(vx_d3d_geo_on){vx_d3d_geo[6]+=drawn;vx_d3d_geo[3]+=culled;}
 return k;
}
