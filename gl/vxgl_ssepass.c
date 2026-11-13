/* v10.487: one SSE pass a vertex for a display list's draw (Lattice's case; v10.488: and a draw not from a list, glBegin/glEnd or vertex
 * arrays, its inputs copied into SoA arrays first (imm_build); lit draws only): transform, clip test, projection, lighting, fog and the
 * vertex emit together, four vertices at a time, from the list's geometry kept in structure-of-arrays form since the list was compiled.
 *
 * Where it sits. The first stage of the ICD's tnl pipeline (vxgl_tris.c vxgl_pipeline). glCallList's vertex list (vbo_save_draw.c) names
 * its node in the context's save.vx855_node around draw_prims; when the node has an SoA copy (built at glEndList, _vbo_vx855_soa_build: vertices in
 * padded arrays x[], y[], z[], nx[], ny[], nz[], s[], t[], r g b a, 16-byte aligned, four lanes a group) and the state is of the kind the
 * pass takes (below), the stage does the whole vertex side itself, calls the render stage and returns GL_FALSE: Mesa's vertex, normal,
 * lighting, fog, texgen and texture-matrix stages do not run for that draw. Anything else: GL_TRUE, the pipeline as before.
 *
 * What it takes (else the pipeline as before, counted in vxgl_ssepass_stat[3] with the reason bits in [6]):
 *   the engine drawing (no fallback), no vertex or fragment program, no user clip planes, no attenuated points, no index buffer, the list's
 *   whole node in one vertex buffer (no split), positions of 2 or 3 floats; lighting off, or lighting of light_rgba's kind in eye
 *   coordinates (one side, no separate specular colour, spotlights since v10.488, no material per vertex: colour material with the list's colours
 *   falls back); fog off, or GL_FOG from the fragment depth as blend factors (the engine's vertex fog); the vertex layout x y z rhw,
 *   diffuse, [pad + fog], two-float coordinate sets read straight from the list (no texgen, identity texture matrices, no padded copy's
 *   transform, no projective or three-float set).
 * The outputs are exactly what the pipeline leaves for the render stage: VB->ClipPtr (clip coordinates, needed by the clipper, with
 * room for its new vertices), VB->ClipMask / ClipOrMask / ClipAndMask, and the vertex store (c->verts) filled in the engine's layout
 * (t_vertex.c's EMIT_4F_VIEWPORT, EMIT_4UB_4F_BGRA, EMIT_1UB_1F, EMIT_2F) so the strip paths, the clipper (its interpolation reads the
 * store and ClipPtr) and the flat-shading paths work unchanged. The render stage's BuildVertices is a no-op for the call.
 *
 * Arithmetic. The same operations in the same order as the C code they replace, each rounded to single precision by SSE: transform
 * ((m0 x + m4 y) + m8 z) + m12 (vxgl_sse.c's xf loops); clip bits from w < c and w < -c (clip1); 1/w by a true divide; viewport
 * vp0 x + vp12; the normal by the inverse's rows ((ux m0 + uy m1) + uz m2), normalized by rsqrtps and one Newton step (vx855_rsq_sse's
 * steps, so the same values as the lighting's RSQRT path); lighting as light_vx855 (t_vb_light.c): compares on the floats' bits, the
 * attenuation's reciprocal by rcpps and a Newton step (vx855_rcp_sse), the specular from the shine table a lane at a time; fog from the
 * exp table a lane at a time (t_vb_fog.c's NEG_EXP); colour bytes as UNCLAMPED_FLOAT_TO_UBYTE (f * 255/256 + 32768, the low byte; < 0
 * gives 0, >= 0.996 gives 255). The x87 code keeps intermediates at the FPU's precision and rounds once at the store, so a result can
 * differ in the last bit of a float: a colour channel by 1 at most, a window coordinate in its last bit (the SSE transform has done the
 * same since v10.359). A group with a lane the SSE steps do not cover (a vertex at a positional light, a length outside 1e-30..1e30, a
 * normal of length 1e-20 or less under GL_NORMALIZE) is done again by the scalar reference (ref_group), which is Mesa's arithmetic.
 * MXCSR 0x1F80 for the pass (vxgl_sse.c mx_enter), only SSE1 instructions.
 *
 * ACCEL_OPENGL_SSEPASS (1, with SSE): the pass; 0: the pipeline as before. vxgl_ssepass_stat (VXGL.LOG at a context's end, VXGL_SsePassStats):
 * [0] draws by the pass, [1] their vertices, [2] groups done again by the reference, [3] list draws left to the pipeline, [4] lists compiled
 * without an SoA copy (an attribute the pass does not take), [5] draws whole outside, [6] the reason bits of the last draw left, [7] lit draws. */
#include <string.h>
#include <stddef.h>
#include "vxgl.h"
#include "macros.h"
#include "light.h"
#include "simple_list.h"
#include "tnl/t_context.h"
#include "tnl/t_pipeline.h"
#include "vbo/vbo_context.h"
#include "vbo/vbo_save.h"

int vxgl_ssepass_on=0;
int vxgl_ssepass_box=1;   /* v10.490: ACCEL_OPENGL_SSEBOX (a list's draw whole outside found from its bounding box's eight corners) */
int vxgl_ssepass_shine=1; /* v10.490: ACCEL_OPENGL_SSESHINE (the specular table's lookup four lanes at a time) */
int vxgl_ssepass_pack=1;  /* v10.490: ACCEL_OPENGL_SSEPACK (the colour and fog bytes four lanes at a time, no branch a channel) */
int vxgl_ssepass_cache=1; /* v10.492: ACCEL_OPENGL_SSECACHE (the per-draw set-up kept while the matrices, lights and material are the same) */
int vxgl_ssepass_mask=1;  /* v10.492: ACCEL_OPENGL_SSEMASK (the lighting's per-lane compares by SSE) */
int vxgl_ssepass_tight=1; /* v10.493: ACCEL_OPENGL_SSETIGHT (a group's clip masks from tables, one store; coordinate sets copied as words) */
/* .506: ACCEL_OPENGL_SSESNAP (v10.490, the grid words by snap_group for every group; off since v10.492, slower) removed: SSESNAP2 makes them */
int vxgl_ssepass_fuse=1;   /* v10.495: ACCEL_OPENGL_SSEFUSE (a phase's kernels in one, the values kept in the registers) */
int vxgl_ssepass_accept=1; /* v10.495: ACCEL_OPENGL_SSEACCEPT (a list's draw the box has inside every plane: no clip compares, every mask 0) */
int vxgl_ssepass_box2=1;   /* v10.495: ACCEL_OPENGL_SSEBOX2 (the box's test from its centre and half sizes, plane by plane; and inside, for SSEACCEPT) */
int vxgl_ssepass_partial=1; /* v10.495: ACCEL_OPENGL_SSEPARTIAL (only the material changed since the last set-up: only its derived values made again) */
int vxgl_ssepass_snap2=1; /* v10.497: ACCEL_OPENGL_SSESNAP2 (the strip path's 1/16 grid x y from the window x y vk_front keeps, four lanes at a time: no snapw a vertex) */
int vxgl_ssepass_outfast=1; /* v10.497: ACCEL_OPENGL_SSEOUTFAST (the whole-outside test without the store, plane by plane; MXCSR set once a draw) */
int vxgl_ssepass_early=1; /* v10.497: ACCEL_OPENGL_SSEEARLY (a lit draw not from a list whose lighting the pass does not take: declined before its vertices are copied) */
int vxgl_ssepass_immbox=1; /* v10.496: ACCEL_OPENGL_SSEIMMBOX (a draw not from a list: its box made as its positions are copied) */
int vxgl_ssepass_defer=1; /* .503: ACCEL_OPENGL_SSEDEFER (a lit draw with culling: the positions first, the lighting only for groups a drawn triangle uses) */
uint32_t vxgl_ssepass_defer_stat[3]; /* .503: [0] draws deferred, [1] groups lit, [2] groups left unlit */
int vxgl_ssepass_sse2=0; /* .503: ACCEL_OPENGL_SSE2 and the processor's SSE2 (CPUID 1 EDX bit 26): the colour words packed by SSE2 (vk_pack); the ICD sets it */
int vxgl_ssepass_outside=1; /* v10.489: ACCEL_OPENGL_SSEOUTSIDE (the clip masks first, a draw whole outside not lit) */ /* the ICD reads ACCEL_OPENGL_SSEPASS (vxgl_icd.c); the host tests set it */
unsigned long vxgl_ssepass_imm_stat[3]; /* v10.497: [2] draws not from a list declined before the copy (SSEEARLY) */ /* v10.496: draws not from a list whole outside by their box, inside by it */
unsigned long vxgl_ssepass_stat[20]; /* v10.495: [16] list draws inside every plane by the box (SSEACCEPT), [17] groups by the fused kernels, [18] partial set-ups (SSEPARTIAL), [19] list draws bound by the short path (LISTBIND) */ /* v10.488: [8] draws not from a list (glBegin/glEnd, vertex arrays) by the pass, [9] their vertices; v10.490: [10] draws whole
 * outside by the box, [11] vertices snapped by the pass, [12] triangles by the strip path's culling run (vx_d3d_core.c), [13] specular lanes by SSE, [14] by _mesa_pow;
 * v10.492: [15] lit draws whose lighting set-up was kept (ACCEL_OPENGL_SSECACHE) */
extern const GLfloat *_tnl_vx855_fog_exp_table(void); /* t_vb_fog.c */

/* ---- the SoA copy of a list (vbo_save_api.c builds it at glEndList, frees it with the node) ----------------------------------------- */
#define SOA_TEX 3 /* coordinate sets kept (the ICD emits at most two, a third stage reads one of them) */
typedef struct {
 GLuint count,n4;            /* vertices; lanes (count rounded up to four; the padding lanes repeat the last vertex) */
 GLuint have;                /* bit 1 normals, bit 2 colours (4 floats, alpha 1 when the list had three), bits 4.. texture set u */
 GLuint const_color;         /* the colours are all the same (glColor once in the list: colour material can take it once) */
 GLfloat *x,*y,*z,*nx,*ny,*nz,*r,*g,*b,*a,*s[SOA_TEX],*t[SOA_TEX];
 GLuint hasbox;GLfloat bmin[3],bmax[3]; /* v10.490: the positions' bounding box (every one finite), built with the copy; a draw not from a list: none */
} vx_soa;

void *_vbo_vx855_soa_build(const struct vbo_save_vertex_list *node,const GLfloat *verts) {
 const GLubyte *sz=node->attrsz;GLuint i,n=node->count,n4=(n+3)&~3U,arrays=3,have=0,off[VBO_ATTRIB_MAX],o=0;GLfloat *mem;vx_soa *so;GLubyte *blk;
 if(!n||sz[VBO_ATTRIB_POS]<2||sz[VBO_ATTRIB_POS]>3)return 0;
 for(i=0;i<VBO_ATTRIB_MAX;i++){off[i]=o;o+=sz[i];
  if(!sz[i]||i==VBO_ATTRIB_POS)continue;
  if(i==VBO_ATTRIB_NORMAL&&sz[i]==3){have|=1;arrays+=3;continue;}
  if(i==VBO_ATTRIB_COLOR0&&sz[i]>=3){have|=2;arrays+=4;continue;}
  if(i>=VBO_ATTRIB_TEX0&&i<VBO_ATTRIB_TEX0+SOA_TEX&&sz[i]>=1&&sz[i]<=4){have|=4U<<(i-VBO_ATTRIB_TEX0);arrays+=2;continue;}
  vxgl_ssepass_stat[4]++;return 0;} /* weights, a secondary colour, fog coordinates, indices, edge flags, materials, generic attributes */
 if(o!=node->vertex_size)return 0;
 blk=(GLubyte*)_mesa_align_malloc(sizeof(vx_soa)+16+arrays*n4*sizeof(GLfloat),16);
 if(!blk)return 0;
 so=(vx_soa*)blk;memset(so,0,sizeof(*so));so->count=n;so->n4=n4;so->have=have;so->const_color=(have&2)?1:0;
 mem=(GLfloat*)(blk+((sizeof(vx_soa)+15)&~15U));
#define SOA_ARRAY(p) do{(p)=mem;mem+=n4;}while(0)
 SOA_ARRAY(so->x);SOA_ARRAY(so->y);SOA_ARRAY(so->z);
 if(have&1){SOA_ARRAY(so->nx);SOA_ARRAY(so->ny);SOA_ARRAY(so->nz);}
 if(have&2){SOA_ARRAY(so->r);SOA_ARRAY(so->g);SOA_ARRAY(so->b);SOA_ARRAY(so->a);}
 for(i=0;i<SOA_TEX;i++)if(have&(4U<<i)){SOA_ARRAY(so->s[i]);SOA_ARRAY(so->t[i]);}
#undef SOA_ARRAY
 so->hasbox=1;
 for(i=0;i<n4;i++){const GLfloat *v=verts+(i<n?i:n-1)*node->vertex_size;GLuint u;
  so->x[i]=v[off[VBO_ATTRIB_POS]];so->y[i]=v[off[VBO_ATTRIB_POS]+1];so->z[i]=sz[VBO_ATTRIB_POS]>2?v[off[VBO_ATTRIB_POS]+2]:0.0F;
  if(have&1){so->nx[i]=v[off[VBO_ATTRIB_NORMAL]];so->ny[i]=v[off[VBO_ATTRIB_NORMAL]+1];so->nz[i]=v[off[VBO_ATTRIB_NORMAL]+2];}
  if(have&2){so->r[i]=v[off[VBO_ATTRIB_COLOR0]];so->g[i]=v[off[VBO_ATTRIB_COLOR0]+1];so->b[i]=v[off[VBO_ATTRIB_COLOR0]+2];so->a[i]=sz[VBO_ATTRIB_COLOR0]>3?v[off[VBO_ATTRIB_COLOR0]+3]:1.0F;
   if(i&&(so->r[i]!=so->r[0]||so->g[i]!=so->g[0]||so->b[i]!=so->b[0]||so->a[i]!=so->a[0]))so->const_color=0;}
  for(u=0;u<SOA_TEX;u++)if(have&(4U<<u)){so->s[u][i]=v[off[VBO_ATTRIB_TEX0+u]];so->t[u][i]=sz[VBO_ATTRIB_TEX0+u]>1?v[off[VBO_ATTRIB_TEX0+u]+1]:0.0F;}
  if(i<n){GLfloat p[3];int j;p[0]=so->x[i];p[1]=so->y[i];p[2]=so->z[i]; /* v10.490: the box (a NaN or an infinity: none, the vertices tested one by one) */
   for(j=0;j<3;j++){if(!(p[j]>-1e30F&&p[j]<1e30F))so->hasbox=0;if(!i||p[j]<so->bmin[j])so->bmin[j]=p[j];if(!i||p[j]>so->bmax[j])so->bmax[j]=p[j];}}}
 return so;
}
void _vbo_vx855_soa_free(void *soa) {if(soa)_mesa_align_free(soa);}

/* ---- four-lane vectors in aligned memory; the SSE kernels ------------------------------------------------------------------------------
 * Every kernel reads and writes 16-byte aligned v4s (the store below is allocated aligned, its members are multiples of 16 bytes); the
 * vertex store is written with movups (its stride is not a multiple of 16). Open Watcom: #pragma aux, the pointers in registers. The host
 * tests (gcc): the same operations by intrinsics. Order of operations identical in both. */
typedef struct {GLfloat f[4];} v4;
typedef struct {v4 x,y,z;} v4x3;
typedef struct {v4 x,y,z,w;} v4x4;
#define LANE_U(v,i) (((const GLuint*)(v).f)[i])

#if defined(__WATCOMC__) && defined(__386__)
static unsigned mx_enter(void);
#pragma aux mx_enter = "sub esp,8" "stmxcsr [esp]" "mov dword ptr [esp+4],1F80h" "ldmxcsr [esp+4]" "mov eax,[esp]" "add esp,8" value [eax] modify exact [eax];
static void mx_leave(unsigned old);
#pragma aux mx_leave = "push eax" "ldmxcsr [esp]" "add esp,4" parm [eax] modify exact [];
/* out[r] = ((m[r] x + m[4+r] y) + m[8+r] z) + m[12+r]: m 16 splats (the matrix's 16 elements, each in four lanes), rows 0-3 */
static void vk_xf4(const v4 *m,const GLfloat *x,const GLfloat *y,const GLfloat *z,v4x4 *out);
#pragma aux vk_xf4 = "movaps xmm4,[edx]" "movaps xmm5,[ebx]" "movaps xmm6,[ecx]" \
 "movaps xmm0,[eax]" "mulps xmm0,xmm4" "movaps xmm1,[eax+64]" "mulps xmm1,xmm5" "addps xmm0,xmm1" "movaps xmm1,[eax+128]" "mulps xmm1,xmm6" "addps xmm0,xmm1" "addps xmm0,[eax+192]" "movaps [edi],xmm0" \
 "movaps xmm0,[eax+16]" "mulps xmm0,xmm4" "movaps xmm1,[eax+80]" "mulps xmm1,xmm5" "addps xmm0,xmm1" "movaps xmm1,[eax+144]" "mulps xmm1,xmm6" "addps xmm0,xmm1" "addps xmm0,[eax+208]" "movaps [edi+16],xmm0" \
 "movaps xmm0,[eax+32]" "mulps xmm0,xmm4" "movaps xmm1,[eax+96]" "mulps xmm1,xmm5" "addps xmm0,xmm1" "movaps xmm1,[eax+160]" "mulps xmm1,xmm6" "addps xmm0,xmm1" "addps xmm0,[eax+224]" "movaps [edi+32],xmm0" \
 "movaps xmm0,[eax+48]" "mulps xmm0,xmm4" "movaps xmm1,[eax+112]" "mulps xmm1,xmm5" "addps xmm0,xmm1" "movaps xmm1,[eax+176]" "mulps xmm1,xmm6" "addps xmm0,xmm1" "addps xmm0,[eax+240]" "movaps [edi+48],xmm0" \
 parm [eax] [edx] [ebx] [ecx] [edi] modify exact [];
/* rows 0-2 only (eye coordinates) */
static void vk_xf3(const v4 *m,const GLfloat *x,const GLfloat *y,const GLfloat *z,v4x3 *out);
#pragma aux vk_xf3 = "movaps xmm4,[edx]" "movaps xmm5,[ebx]" "movaps xmm6,[ecx]" \
 "movaps xmm0,[eax]" "mulps xmm0,xmm4" "movaps xmm1,[eax+64]" "mulps xmm1,xmm5" "addps xmm0,xmm1" "movaps xmm1,[eax+128]" "mulps xmm1,xmm6" "addps xmm0,xmm1" "addps xmm0,[eax+192]" "movaps [edi],xmm0" \
 "movaps xmm0,[eax+16]" "mulps xmm0,xmm4" "movaps xmm1,[eax+80]" "mulps xmm1,xmm5" "addps xmm0,xmm1" "movaps xmm1,[eax+144]" "mulps xmm1,xmm6" "addps xmm0,xmm1" "addps xmm0,[eax+208]" "movaps [edi+16],xmm0" \
 "movaps xmm0,[eax+32]" "mulps xmm0,xmm4" "movaps xmm1,[eax+96]" "mulps xmm1,xmm5" "addps xmm0,xmm1" "movaps xmm1,[eax+160]" "mulps xmm1,xmm6" "addps xmm0,xmm1" "addps xmm0,[eax+224]" "movaps [edi+32],xmm0" \
 parm [eax] [edx] [ebx] [ecx] [edi] modify exact [];
/* normals: out = ((x n0 + y n1) + z n2), ((x n3 + y n4) + z n5), ((x n6 + y n7) + z n8): n the 9 splats of the inverse's m0 m1 m2 m4 m5 m6 m8 m9 m10 */
static void vk_xfn(const v4 *n,const GLfloat *x,const GLfloat *y,const GLfloat *z,v4x3 *out);
#pragma aux vk_xfn = "movaps xmm4,[edx]" "movaps xmm5,[ebx]" "movaps xmm6,[ecx]" \
 "movaps xmm0,xmm4" "mulps xmm0,[eax]" "movaps xmm1,xmm5" "mulps xmm1,[eax+16]" "addps xmm0,xmm1" "movaps xmm1,xmm6" "mulps xmm1,[eax+32]" "addps xmm0,xmm1" "movaps [edi],xmm0" \
 "movaps xmm0,xmm4" "mulps xmm0,[eax+48]" "movaps xmm1,xmm5" "mulps xmm1,[eax+64]" "addps xmm0,xmm1" "movaps xmm1,xmm6" "mulps xmm1,[eax+80]" "addps xmm0,xmm1" "movaps [edi+16],xmm0" \
 "movaps xmm0,xmm4" "mulps xmm0,[eax+96]" "movaps xmm1,xmm5" "mulps xmm1,[eax+112]" "addps xmm0,xmm1" "movaps xmm1,xmm6" "mulps xmm1,[eax+128]" "addps xmm0,xmm1" "movaps [edi+32],xmm0" \
 parm [eax] [edx] [ebx] [ecx] [edi] modify exact [];
/* len = (x x + y y) + z z (written to *len); r = rsqrtps, t = (r r) len, s = ((3 - t) r) 0.5 (vx855_rsq_sse's steps); v *= s where len != 0 */
static void vk_norm3(v4x3 *v,const v4 *k,v4 *len);
#pragma aux vk_norm3 = "movaps xmm0,[eax]" "movaps xmm1,[eax+16]" "movaps xmm2,[eax+32]" \
 "movaps xmm3,xmm0" "mulps xmm3,xmm3" "movaps xmm4,xmm1" "mulps xmm4,xmm1" "addps xmm3,xmm4" "movaps xmm4,xmm2" "mulps xmm4,xmm2" "addps xmm3,xmm4" "movaps [ebx],xmm3" \
 "rsqrtps xmm4,xmm3" "movaps xmm5,xmm4" "mulps xmm5,xmm4" "mulps xmm5,xmm3" "movaps xmm6,[edx+16]" "subps xmm6,xmm5" "mulps xmm6,xmm4" "mulps xmm6,[edx]" \
 "xorps xmm7,xmm7" "cmpneqps xmm7,xmm3" \
 "movaps xmm5,xmm0" "mulps xmm5,xmm6" "andps xmm5,xmm7" "movaps xmm3,xmm7" "andnps xmm3,xmm0" "orps xmm5,xmm3" "movaps [eax],xmm5" \
 "movaps xmm5,xmm1" "mulps xmm5,xmm6" "andps xmm5,xmm7" "movaps xmm3,xmm7" "andnps xmm3,xmm1" "orps xmm5,xmm3" "movaps [eax+16],xmm5" \
 "movaps xmm5,xmm2" "mulps xmm5,xmm6" "andps xmm5,xmm7" "movaps xmm3,xmm7" "andnps xmm3,xmm2" "orps xmm5,xmm3" "movaps [eax+32],xmm5" \
 parm [eax] [edx] [ebx] modify exact [];
static void vk_dot3(const v4x3 *a,const v4x3 *b,v4 *out);
#pragma aux vk_dot3 = "movaps xmm0,[eax]" "mulps xmm0,[edx]" "movaps xmm1,[eax+16]" "mulps xmm1,[edx+16]" "addps xmm0,xmm1" "movaps xmm1,[eax+32]" "mulps xmm1,[edx+32]" "addps xmm0,xmm1" "movaps [ebx],xmm0" \
 parm [eax] [edx] [ebx] modify exact [];
static void vk_sub3(v4x3 *out,const v4x3 *a,const v4x3 *b);
#pragma aux vk_sub3 = "movaps xmm0,[edx]" "subps xmm0,[ebx]" "movaps [eax],xmm0" "movaps xmm0,[edx+16]" "subps xmm0,[ebx+16]" "movaps [eax+16],xmm0" "movaps xmm0,[edx+32]" "subps xmm0,[ebx+32]" "movaps [eax+32],xmm0" \
 parm [eax] [edx] [ebx] modify exact [];
static void vk_mul3(v4x3 *out,const v4x3 *a,const v4x3 *b);
#pragma aux vk_mul3 = "movaps xmm0,[edx]" "mulps xmm0,[ebx]" "movaps [eax],xmm0" "movaps xmm0,[edx+16]" "mulps xmm0,[ebx+16]" "movaps [eax+16],xmm0" "movaps xmm0,[edx+32]" "mulps xmm0,[ebx+32]" "movaps [eax+32],xmm0" \
 parm [eax] [edx] [ebx] modify exact [];
static void vk_add3(v4x3 *out,const v4x3 *a,const v4x3 *b);
#pragma aux vk_add3 = "movaps xmm0,[edx]" "addps xmm0,[ebx]" "movaps [eax],xmm0" "movaps xmm0,[edx+16]" "addps xmm0,[ebx+16]" "movaps [eax+16],xmm0" "movaps xmm0,[edx+32]" "addps xmm0,[ebx+32]" "movaps [eax+32],xmm0" \
 parm [eax] [edx] [ebx] modify exact [];
static void vk_scale3(v4x3 *v,const v4 *s);
#pragma aux vk_scale3 = "movaps xmm1,[edx]" "movaps xmm0,[eax]" "mulps xmm0,xmm1" "movaps [eax],xmm0" "movaps xmm0,[eax+16]" "mulps xmm0,xmm1" "movaps [eax+16],xmm0" "movaps xmm0,[eax+32]" "mulps xmm0,xmm1" "movaps [eax+32],xmm0" \
 parm [eax] [edx] modify exact [];
/* out = a s + b, each component */
static void vk_madd3(v4x3 *out,const v4x3 *a,const v4 *s,const v4x3 *b);
#pragma aux vk_madd3 = "movaps xmm1,[ebx]" "movaps xmm0,[edx]" "mulps xmm0,xmm1" "addps xmm0,[ecx]" "movaps [eax],xmm0" "movaps xmm0,[edx+16]" "mulps xmm0,xmm1" "addps xmm0,[ecx+16]" "movaps [eax+16],xmm0" \
 "movaps xmm0,[edx+32]" "mulps xmm0,xmm1" "addps xmm0,[ecx+32]" "movaps [eax+32],xmm0" parm [eax] [edx] [ebx] [ecx] modify exact [];
/* out = mask ? a : b, each component (mask lanes all ones or all zeros) */
static void vk_blend3(v4x3 *out,const v4 *mask,const v4x3 *a,const v4x3 *b);
#pragma aux vk_blend3 = "movaps xmm1,[edx]" \
 "movaps xmm0,[ebx]" "andps xmm0,xmm1" "movaps xmm2,xmm1" "andnps xmm2,[ecx]" "orps xmm0,xmm2" "movaps [eax],xmm0" \
 "movaps xmm0,[ebx+16]" "andps xmm0,xmm1" "movaps xmm2,xmm1" "andnps xmm2,[ecx+16]" "orps xmm0,xmm2" "movaps [eax+16],xmm0" \
 "movaps xmm0,[ebx+32]" "andps xmm0,xmm1" "movaps xmm2,xmm1" "andnps xmm2,[ecx+32]" "orps xmm0,xmm2" "movaps [eax+32],xmm0" \
 parm [eax] [edx] [ebx] [ecx] modify exact [];
/* r = rsqrtps(in); t = (r r) in; out = ((3 - t) r) 0.5 */
static void vk_rsqrt_nr(v4 *out,const v4 *in,const v4 *k);
#pragma aux vk_rsqrt_nr = "movaps xmm0,[edx]" "rsqrtps xmm1,xmm0" "movaps xmm2,xmm1" "mulps xmm2,xmm1" "mulps xmm2,xmm0" "movaps xmm3,[ebx+16]" "subps xmm3,xmm2" "mulps xmm3,xmm1" "mulps xmm3,[ebx]" "movaps [eax],xmm3" \
 parm [eax] [edx] [ebx] modify exact [];
/* r = rcpps(in); t = (r in) r; out = (r + r) - t */
static void vk_rcp_nr(v4 *out,const v4 *in);
#pragma aux vk_rcp_nr = "movaps xmm0,[edx]" "rcpps xmm1,xmm0" "movaps xmm2,xmm1" "mulps xmm2,xmm0" "mulps xmm2,xmm1" "addps xmm1,xmm1" "subps xmm1,xmm2" "movaps [eax],xmm1" parm [eax] [edx] modify exact [];
static void vk_mul(v4 *out,const v4 *a,const v4 *b);
#pragma aux vk_mul = "movaps xmm0,[edx]" "mulps xmm0,[ebx]" "movaps [eax],xmm0" parm [eax] [edx] [ebx] modify exact [];
/* out = (a b) + c */
static void vk_mad(v4 *out,const v4 *a,const v4 *b,const v4 *c);
#pragma aux vk_mad = "movaps xmm0,[edx]" "mulps xmm0,[ebx]" "addps xmm0,[ecx]" "movaps [eax],xmm0" parm [eax] [edx] [ebx] [ecx] modify exact [];
/* the six compares of clip1 (vxgl_sse.c) for four lanes: bits 0-3 w < x, 4-7 w < y, 8-11 w < z, 12-15 w < -x, 16-19 w < -y, 20-23 w < -z; sign the sign-bit splat */
static unsigned vk_clipbits(const v4x4 *c,const v4 *sign);
#pragma aux vk_clipbits = "movaps xmm0,[eax]" "movaps xmm1,[eax+16]" "movaps xmm2,[eax+32]" "movaps xmm3,[eax+48]" "movaps xmm5,[edx]" \
 "movaps xmm4,xmm3" "cmpltps xmm4,xmm0" "movmskps eax,xmm4" \
 "movaps xmm4,xmm3" "cmpltps xmm4,xmm1" "movmskps ecx,xmm4" "shl ecx,4" "or eax,ecx" \
 "movaps xmm4,xmm3" "cmpltps xmm4,xmm2" "movmskps ecx,xmm4" "shl ecx,8" "or eax,ecx" \
 "xorps xmm0,xmm5" "movaps xmm4,xmm3" "cmpltps xmm4,xmm0" "movmskps ecx,xmm4" "shl ecx,12" "or eax,ecx" \
 "xorps xmm1,xmm5" "movaps xmm4,xmm3" "cmpltps xmm4,xmm1" "movmskps ecx,xmm4" "shl ecx,16" "or eax,ecx" \
 "xorps xmm2,xmm5" "movaps xmm4,xmm3" "cmpltps xmm4,xmm2" "movmskps ecx,xmm4" "shl ecx,20" "or eax,ecx" \
 parm [eax] [edx] value [eax] modify exact [eax ecx];
/* oow = 1 / w (one / w, a true divide); out = (x oow, y oow, z oow, oow) where keep is set, (0, 0, 0, 1) elsewhere (the clipped vertices, as vxgl_sse_cliptest4) */
static void vk_project(const v4x4 *c,const v4 *keep,const v4 *one,v4x4 *out);
#pragma aux vk_project = "movaps xmm3,[eax+48]" "movaps xmm4,[ebx]" "divps xmm4,xmm3" \
 "movaps xmm0,[eax]" "mulps xmm0,xmm4" "movaps xmm1,[eax+16]" "mulps xmm1,xmm4" "movaps xmm2,[eax+32]" "mulps xmm2,xmm4" \
 "movaps xmm5,[edx]" "andps xmm0,xmm5" "andps xmm1,xmm5" "andps xmm2,xmm5" "andps xmm4,xmm5" "movaps xmm6,xmm5" "andnps xmm6,[ebx]" "orps xmm4,xmm6" \
 "movaps [edi],xmm0" "movaps [edi+16],xmm1" "movaps [edi+32],xmm2" "movaps [edi+48],xmm4" parm [eax] [edx] [ebx] [edi] modify exact [];
/* window coordinates (vp[0] x + vp[12], ... ; vp the six splats sx sy sz tx ty tz) of four lanes, transposed and written as four vertices' x y z rhw at dst, dst + stride, ... */
static void vk_viewport_store(const v4x4 *p,const v4 *vp,GLubyte *dst,unsigned stride);
#pragma aux vk_viewport_store = "movaps xmm0,[eax]" "mulps xmm0,[edx]" "addps xmm0,[edx+48]" "movaps xmm1,[eax+16]" "mulps xmm1,[edx+16]" "addps xmm1,[edx+64]" \
 "movaps xmm2,[eax+32]" "mulps xmm2,[edx+32]" "addps xmm2,[edx+80]" "movaps xmm3,[eax+48]" \
 "movaps xmm4,xmm0" "unpcklps xmm4,xmm1" "movaps xmm5,xmm2" "unpcklps xmm5,xmm3" "unpckhps xmm0,xmm1" "unpckhps xmm2,xmm3" \
 "movaps xmm6,xmm4" "movlhps xmm6,xmm5" "movups [edi],xmm6" "movhlps xmm5,xmm4" "movups [edi+ecx],xmm5" \
 "movaps xmm6,xmm0" "movlhps xmm6,xmm2" "movups [edi+ecx*2],xmm6" "movhlps xmm2,xmm0" "lea edi,[edi+ecx*2]" "movups [edi+ecx],xmm2" \
 parm [eax] [edx] [edi] [ecx] modify exact [edi];
/* four lanes transposed to four x y z w at dst (the clip coordinates for the clipper) */
static void vk_tr_store(const v4x4 *c,GLfloat *dst);
#pragma aux vk_tr_store = "movaps xmm0,[eax]" "movaps xmm1,[eax+16]" "movaps xmm2,[eax+32]" "movaps xmm3,[eax+48]" \
 "movaps xmm4,xmm0" "unpcklps xmm4,xmm1" "movaps xmm5,xmm2" "unpcklps xmm5,xmm3" "unpckhps xmm0,xmm1" "unpckhps xmm2,xmm3" \
 "movaps xmm6,xmm4" "movlhps xmm6,xmm5" "movaps [edi],xmm6" "movhlps xmm5,xmm4" "movaps [edi+16],xmm5" \
 "movaps xmm6,xmm0" "movlhps xmm6,xmm2" "movaps [edi+32],xmm6" "movhlps xmm2,xmm0" "movaps [edi+48],xmm2" parm [eax] [edi] modify exact [];
/* out = in (255/256) + 32768 (k the two splats): the integer bits' low byte is UNCLAMPED_FLOAT_TO_UBYTE's byte for 0 <= in < 0.996 */
static void vk_bias(v4 *out,const v4 *in,const v4 *k);
#pragma aux vk_bias = "movaps xmm0,[edx]" "mulps xmm0,[ebx]" "addps xmm0,[ebx+16]" "movaps [eax],xmm0" parm [eax] [edx] [ebx] modify exact [];
/* v10.489: GL_LINEAR fog for four lanes: f = (end - |z|) d, clamped to 0..1 (t_vb_fog.c's order: CLAMP keeps 0 for a NaN as maxps does) */
static void vk_linfog(v4 *out,const v4 *z,const v4 *k);
#pragma aux vk_linfog = "movaps xmm0,[edx]" "andps xmm0,[ebx]" "movaps xmm1,[ebx+16]" "subps xmm1,xmm0" "mulps xmm1,[ebx+32]" "maxps xmm1,[ebx+48]" "minps xmm1,[ebx+64]" "movaps [eax],xmm1" \
 parm [eax] [edx] [ebx] modify exact [];
/* v10.490: UNCLAMPED_FLOAT_TO_UBYTE's byte for four lanes, no branch: x = max(in, 0) (a negative, -0: 0), x >= 0.99609375 (IEEE_0996): 1,
 * then x (255/256) + 32768, the bits' low byte (k: 255/256, 32768, 0, 0.99609375, 1). A NaN lane: vk_unord first, the scalar ub_from */
static void vk_ub(v4 *out,const v4 *in,const v4 *k);
#pragma aux vk_ub = "movaps xmm0,[edx]" "maxps xmm0,[ebx+32]" "movaps xmm1,[ebx+48]" "cmpleps xmm1,xmm0" "movaps xmm2,xmm1" "andnps xmm1,xmm0" "andps xmm2,[ebx+64]" "orps xmm1,xmm2" \
 "mulps xmm1,[ebx]" "addps xmm1,[ebx+16]" "movaps [eax],xmm1" parm [eax] [edx] [ebx] modify exact [];
/* .503 (ACCEL_OPENGL_SSE2): four vertices' colour words from vk_ub's biased lanes (rgb[0] red, rgb[1] green, rgb[2] blue, a alpha: each
 * lane's low byte the channel's byte), blue | green << 8 | red << 16 | alpha << 24, stored at d, d + vs, d + 2 vs, d + 3 vs: the same words
 * the C loop makes from the same lanes (pand, pslld, por; movd and psrldq for the stores) */
static void vk_pack(GLubyte *d,const v4 *rgb,const v4 *a,unsigned vs);
#pragma aux vk_pack = "pcmpeqd xmm7,xmm7" "psrld xmm7,24" \
 "movdqa xmm0,[edx+32]" "pand xmm0,xmm7" \
 "movdqa xmm1,[edx+16]" "pand xmm1,xmm7" "pslld xmm1,8" "por xmm0,xmm1" \
 "movdqa xmm1,[edx]" "pand xmm1,xmm7" "pslld xmm1,16" "por xmm0,xmm1" \
 "movdqa xmm1,[ebx]" "pslld xmm1,24" "por xmm0,xmm1" \
 "movd [eax],xmm0" "psrldq xmm0,4" "add eax,ecx" "movd [eax],xmm0" "psrldq xmm0,4" "add eax,ecx" "movd [eax],xmm0" "psrldq xmm0,4" "add eax,ecx" "movd [eax],xmm0" \
 parm [eax] [edx] [ebx] [ecx] modify exact [eax];
/* .505: the lanes with k[0] < v < k[1] (a NaN: not), their bits (range_bad by SSE: RB below) */
static unsigned vk_range(const v4 *v,const v4 *k);
#pragma aux vk_range = "movaps xmm0,[edx]" "cmpltps xmm0,[eax]" "movaps xmm1,[eax]" "cmpltps xmm1,[edx+16]" "andps xmm0,xmm1" "movmskps eax,xmm0" parm [eax] [edx] value [eax] modify exact [eax];
/* v10.492: mask = a >= b a lane (all ones or zeros; a NaN: zeros), its lanes' bits returned */
static unsigned vk_gemask(v4 *mask,const v4 *a,const v4 *b);
#pragma aux vk_gemask = "movaps xmm0,[ebx]" "cmpleps xmm0,[edx]" "movaps [eax],xmm0" "movmskps eax,xmm0" parm [eax] [edx] [ebx] value [eax] modify exact [eax];
/* the lanes holding a NaN (bit v) */
static unsigned vk_unord(const v4 *in);
#pragma aux vk_unord = "movaps xmm0,[eax]" "cmpunordps xmm0,xmm0" "movmskps eax,xmm0" parm [eax] value [eax] modify exact [eax];
/* v10.490: the shine table's index for four lanes (GET_SHINE_TAB_ENTRY): sh[0] f = n.h 255, sh[1] floor(f) (f + 1.5 2^23 - 1.5 2^23, less
 * 1 where that went past f), sh[2] floor(f) + 1.5 2^23 (its bits less 0x4B400000: the integer) (k: 255, 1.5 2^23, 1); 0 <= f < 255 */
static void vk_shine_a(v4 *sh,const v4 *ndh,const v4 *k);
#pragma aux vk_shine_a = "movaps xmm0,[edx]" "mulps xmm0,[ebx]" "movaps xmm1,xmm0" "addps xmm1,[ebx+16]" "subps xmm1,[ebx+16]" "movaps xmm2,xmm0" "cmpltps xmm2,xmm1" \
 "andps xmm2,[ebx+32]" "subps xmm1,xmm2" "movaps xmm3,xmm1" "addps xmm3,[ebx+16]" "movaps [eax],xmm0" "movaps [eax+16],xmm1" "movaps [eax+32],xmm3" parm [eax] [edx] [ebx] modify exact [];
/* out = t0 + (f - floor(f)) (t1 - t0): sh[0] f, sh[1] floor(f), sh[3] t0, sh[4] t1 (the table's entries k and k + 1) */
static void vk_shine_b(v4 *out,const v4 *sh);
#pragma aux vk_shine_b = "movaps xmm0,[edx]" "subps xmm0,[edx+16]" "movaps xmm1,[edx+64]" "subps xmm1,[edx+48]" "mulps xmm0,xmm1" "addps xmm0,[edx+48]" "movaps [eax],xmm0" parm [eax] [edx] modify exact [];
/* v10.490: snapw (vx_d3d_core.c) for four lanes of |x| < 2040: n = x 2048 rounded half away from zero (floor(|x| 2048), plus 1 where the
 * fraction is 1/2 or more, the sign put back), n / 128 to an integer down (up: vk_snapu), times 1/16. k: the abs mask, 2048, 1.5 2^23, 1, 0.5,
 * 1/128, 1/16, the sign mask. Every step exact: the same float as snapw's */
static void vk_snapd(v4 *out,const v4 *in,const v4 *k);
#pragma aux vk_snapd = "movaps xmm0,[edx]" "movaps xmm7,xmm0" "andps xmm7,[ebx+112]" "andps xmm0,[ebx]" "mulps xmm0,[ebx+16]" \
 "movaps xmm1,xmm0" "addps xmm1,[ebx+32]" "subps xmm1,[ebx+32]" "movaps xmm2,xmm0" "cmpltps xmm2,xmm1" "andps xmm2,[ebx+48]" "subps xmm1,xmm2" \
 "subps xmm0,xmm1" "movaps xmm2,[ebx+64]" "cmpleps xmm2,xmm0" "andps xmm2,[ebx+48]" "addps xmm1,xmm2" "orps xmm1,xmm7" "mulps xmm1,[ebx+80]" \
 "movaps xmm0,xmm1" "addps xmm0,[ebx+32]" "subps xmm0,[ebx+32]" "movaps xmm2,xmm1" "cmpltps xmm2,xmm0" "andps xmm2,[ebx+48]" "subps xmm0,xmm2" \
 "mulps xmm0,[ebx+96]" "movaps [eax],xmm0" parm [eax] [edx] [ebx] modify exact [];
static void vk_snapu(v4 *out,const v4 *in,const v4 *k);
#pragma aux vk_snapu = "movaps xmm0,[edx]" "movaps xmm7,xmm0" "andps xmm7,[ebx+112]" "andps xmm0,[ebx]" "mulps xmm0,[ebx+16]" \
 "movaps xmm1,xmm0" "addps xmm1,[ebx+32]" "subps xmm1,[ebx+32]" "movaps xmm2,xmm0" "cmpltps xmm2,xmm1" "andps xmm2,[ebx+48]" "subps xmm1,xmm2" \
 "subps xmm0,xmm1" "movaps xmm2,[ebx+64]" "cmpleps xmm2,xmm0" "andps xmm2,[ebx+48]" "addps xmm1,xmm2" "orps xmm1,xmm7" "mulps xmm1,[ebx+80]" \
 "movaps xmm0,xmm1" "addps xmm0,[ebx+32]" "subps xmm0,[ebx+32]" "movaps xmm2,xmm0" "cmpltps xmm2,xmm1" "andps xmm2,[ebx+48]" "addps xmm0,xmm2" \
 "mulps xmm0,[ebx+96]" "movaps [eax],xmm0" parm [eax] [edx] [ebx] modify exact [];
/* the lanes with |x| < 2040 (bit v; a NaN: not) (k[0] the abs mask, k[8] 2040) */
static unsigned vk_snapok(const v4 *in,const v4 *k);
#pragma aux vk_snapok = "movaps xmm0,[eax]" "andps xmm0,[edx]" "movaps xmm1,[edx+128]" "cmpltps xmm0,xmm1" "movmskps eax,xmm0" parm [eax] [edx] value [eax] modify exact [eax];
/* ---- v10.495 (ACCEL_OPENGL_SSEFUSE; board, Lattice v10.493: sse_group 35% of the samples, its kernels each loading their inputs from the
 * store and writing their result back): a phase's kernels in one, the values kept in the registers between them. The same instructions on
 * the same values in the same order as the kernels they replace (each named below), so every result is the same float; vxgl_ssepass_ktest 4
 * checks each against the kernels it replaces on the DLL's own code (tests/sse_emu.py). */
/* vk_xf4, vk_clipbits, the kept lanes (no compare set), vk_project, vk_viewport_store and vk_tr_store: k the store's mvp (sign +256, one +272,
 * vp +288, the stride +384); the compare bits returned (vk_clipbits'), the window vertices at dst, the clip coordinates at clip */
static unsigned vk_front(const v4 *k,const GLfloat *x,const GLfloat *y,const GLfloat *z,GLubyte *dst,GLfloat *clip);
#pragma aux vk_front = "movaps xmm4,[edx]" "movaps xmm5,[ebx]" "movaps xmm6,[ecx]" \
 "movaps xmm0,[eax]" "mulps xmm0,xmm4" "movaps xmm7,[eax+64]" "mulps xmm7,xmm5" "addps xmm0,xmm7" "movaps xmm7,[eax+128]" "mulps xmm7,xmm6" "addps xmm0,xmm7" "addps xmm0,[eax+192]" \
 "movaps xmm1,[eax+16]" "mulps xmm1,xmm4" "movaps xmm7,[eax+80]" "mulps xmm7,xmm5" "addps xmm1,xmm7" "movaps xmm7,[eax+144]" "mulps xmm7,xmm6" "addps xmm1,xmm7" "addps xmm1,[eax+208]" \
 "movaps xmm2,[eax+32]" "mulps xmm2,xmm4" "movaps xmm7,[eax+96]" "mulps xmm7,xmm5" "addps xmm2,xmm7" "movaps xmm7,[eax+160]" "mulps xmm7,xmm6" "addps xmm2,xmm7" "addps xmm2,[eax+224]" \
 "movaps xmm3,[eax+48]" "mulps xmm3,xmm4" "movaps xmm7,[eax+112]" "mulps xmm7,xmm5" "addps xmm3,xmm7" "movaps xmm7,[eax+176]" "mulps xmm7,xmm6" "addps xmm3,xmm7" "addps xmm3,[eax+240]" \
 "movaps xmm7,xmm3" "cmpltps xmm7,xmm0" "movmskps edx,xmm7" "movaps xmm4,xmm7" \
 "movaps xmm7,xmm3" "cmpltps xmm7,xmm1" "movmskps ebx,xmm7" "shl ebx,4" "or edx,ebx" "orps xmm4,xmm7" \
 "movaps xmm7,xmm3" "cmpltps xmm7,xmm2" "movmskps ebx,xmm7" "shl ebx,8" "or edx,ebx" "orps xmm4,xmm7" \
 "movaps xmm5,[eax+256]" \
 "movaps xmm6,xmm0" "xorps xmm6,xmm5" "movaps xmm7,xmm3" "cmpltps xmm7,xmm6" "movmskps ebx,xmm7" "shl ebx,12" "or edx,ebx" "orps xmm4,xmm7" \
 "movaps xmm6,xmm1" "xorps xmm6,xmm5" "movaps xmm7,xmm3" "cmpltps xmm7,xmm6" "movmskps ebx,xmm7" "shl ebx,16" "or edx,ebx" "orps xmm4,xmm7" \
 "movaps xmm6,xmm2" "xorps xmm6,xmm5" "movaps xmm7,xmm3" "cmpltps xmm7,xmm6" "movmskps ebx,xmm7" "shl ebx,20" "or edx,ebx" "orps xmm4,xmm7" \
 "movaps xmm5,xmm0" "unpcklps xmm5,xmm1" "movaps xmm6,xmm2" "unpcklps xmm6,xmm3" "movaps xmm7,xmm5" "movlhps xmm7,xmm6" "movaps [esi],xmm7" "movhlps xmm6,xmm5" "movaps [esi+16],xmm6" \
 "movaps xmm5,xmm0" "unpckhps xmm5,xmm1" "movaps xmm6,xmm2" "unpckhps xmm6,xmm3" "movaps xmm7,xmm5" "movlhps xmm7,xmm6" "movaps [esi+32],xmm7" "movhlps xmm6,xmm5" "movaps [esi+48],xmm6" \
 "xorps xmm7,xmm7" "cmpeqps xmm7,xmm7" "xorps xmm4,xmm7" \
 "movaps xmm5,[eax+272]" "divps xmm5,xmm3" "mulps xmm0,xmm5" "mulps xmm1,xmm5" "mulps xmm2,xmm5" "andps xmm0,xmm4" "andps xmm1,xmm4" "andps xmm2,xmm4" \
 "movaps xmm3,xmm5" "andps xmm3,xmm4" "movaps xmm6,xmm4" "andnps xmm6,[eax+272]" "orps xmm3,xmm6" \
 "mulps xmm0,[eax+288]" "addps xmm0,[eax+336]" "mulps xmm1,[eax+304]" "addps xmm1,[eax+352]" "mulps xmm2,[eax+320]" "addps xmm2,[eax+368]" "movaps [eax+400],xmm0" "movaps [eax+416],xmm1" "mov ecx,[eax+384]" \
 "movaps xmm4,xmm0" "unpcklps xmm4,xmm1" "movaps xmm5,xmm2" "unpcklps xmm5,xmm3" "unpckhps xmm0,xmm1" "unpckhps xmm2,xmm3" \
 "movaps xmm6,xmm4" "movlhps xmm6,xmm5" "movups [edi],xmm6" "movhlps xmm5,xmm4" "movups [edi+ecx],xmm5" \
 "movaps xmm6,xmm0" "movlhps xmm6,xmm2" "movups [edi+ecx*2],xmm6" "movhlps xmm2,xmm0" "lea edi,[edi+ecx*2]" "movups [edi+ecx],xmm2" "mov eax,edx" \
 parm [eax] [edx] [ebx] [ecx] [edi] [esi] value [eax] modify exact [eax ebx ecx edx edi];
/* v10.495 (ACCEL_OPENGL_SSEACCEPT): vk_front for a group the box has inside every plane (no compare can be set: every lane kept): no compares */
static void vk_front_in(const v4 *k,const GLfloat *x,const GLfloat *y,const GLfloat *z,GLubyte *dst,GLfloat *clip);
#pragma aux vk_front_in = "movaps xmm4,[edx]" "movaps xmm5,[ebx]" "movaps xmm6,[ecx]" \
 "movaps xmm0,[eax]" "mulps xmm0,xmm4" "movaps xmm7,[eax+64]" "mulps xmm7,xmm5" "addps xmm0,xmm7" "movaps xmm7,[eax+128]" "mulps xmm7,xmm6" "addps xmm0,xmm7" "addps xmm0,[eax+192]" \
 "movaps xmm1,[eax+16]" "mulps xmm1,xmm4" "movaps xmm7,[eax+80]" "mulps xmm7,xmm5" "addps xmm1,xmm7" "movaps xmm7,[eax+144]" "mulps xmm7,xmm6" "addps xmm1,xmm7" "addps xmm1,[eax+208]" \
 "movaps xmm2,[eax+32]" "mulps xmm2,xmm4" "movaps xmm7,[eax+96]" "mulps xmm7,xmm5" "addps xmm2,xmm7" "movaps xmm7,[eax+160]" "mulps xmm7,xmm6" "addps xmm2,xmm7" "addps xmm2,[eax+224]" \
 "movaps xmm3,[eax+48]" "mulps xmm3,xmm4" "movaps xmm7,[eax+112]" "mulps xmm7,xmm5" "addps xmm3,xmm7" "movaps xmm7,[eax+176]" "mulps xmm7,xmm6" "addps xmm3,xmm7" "addps xmm3,[eax+240]" \
 "movaps xmm5,xmm0" "unpcklps xmm5,xmm1" "movaps xmm6,xmm2" "unpcklps xmm6,xmm3" "movaps xmm7,xmm5" "movlhps xmm7,xmm6" "movaps [esi],xmm7" "movhlps xmm6,xmm5" "movaps [esi+16],xmm6" \
 "movaps xmm5,xmm0" "unpckhps xmm5,xmm1" "movaps xmm6,xmm2" "unpckhps xmm6,xmm3" "movaps xmm7,xmm5" "movlhps xmm7,xmm6" "movaps [esi+32],xmm7" "movhlps xmm6,xmm5" "movaps [esi+48],xmm6" \
 "movaps xmm5,[eax+272]" "divps xmm5,xmm3" "mulps xmm0,xmm5" "mulps xmm1,xmm5" "mulps xmm2,xmm5" "movaps xmm3,xmm5" \
 "mulps xmm0,[eax+288]" "addps xmm0,[eax+336]" "mulps xmm1,[eax+304]" "addps xmm1,[eax+352]" "mulps xmm2,[eax+320]" "addps xmm2,[eax+368]" "movaps [eax+400],xmm0" "movaps [eax+416],xmm1" "mov ecx,[eax+384]" \
 "movaps xmm4,xmm0" "unpcklps xmm4,xmm1" "movaps xmm5,xmm2" "unpcklps xmm5,xmm3" "unpckhps xmm0,xmm1" "unpckhps xmm2,xmm3" \
 "movaps xmm6,xmm4" "movlhps xmm6,xmm5" "movups [edi],xmm6" "movhlps xmm5,xmm4" "movups [edi+ecx],xmm5" \
 "movaps xmm6,xmm0" "movlhps xmm6,xmm2" "movups [edi+ecx*2],xmm6" "movhlps xmm2,xmm0" "lea edi,[edi+ecx*2]" "movups [edi+ecx],xmm2" \
 parm [eax] [edx] [ebx] [ecx] [edi] [esi] modify exact [ecx edi]; /* (v10.497: the window x y at +400, +416 as vk_front) */
/* vk_xfn then vk_norm3: k 0.5, 3, the lengths written at k + 32 */
static void vk_xfn_norm(const v4 *n,const GLfloat *x,const GLfloat *y,const GLfloat *z,v4x3 *out,v4 *k);
#pragma aux vk_xfn_norm = "movaps xmm4,[edx]" "movaps xmm5,[ebx]" "movaps xmm6,[ecx]" \
 "movaps xmm0,xmm4" "mulps xmm0,[eax]" "movaps xmm7,xmm5" "mulps xmm7,[eax+16]" "addps xmm0,xmm7" "movaps xmm7,xmm6" "mulps xmm7,[eax+32]" "addps xmm0,xmm7" \
 "movaps xmm1,xmm4" "mulps xmm1,[eax+48]" "movaps xmm7,xmm5" "mulps xmm7,[eax+64]" "addps xmm1,xmm7" "movaps xmm7,xmm6" "mulps xmm7,[eax+80]" "addps xmm1,xmm7" \
 "movaps xmm2,xmm4" "mulps xmm2,[eax+96]" "movaps xmm7,xmm5" "mulps xmm7,[eax+112]" "addps xmm2,xmm7" "movaps xmm7,xmm6" "mulps xmm7,[eax+128]" "addps xmm2,xmm7" \
 "movaps xmm3,xmm0" "mulps xmm3,xmm3" "movaps xmm4,xmm1" "mulps xmm4,xmm1" "addps xmm3,xmm4" "movaps xmm4,xmm2" "mulps xmm4,xmm2" "addps xmm3,xmm4" "movaps [esi+32],xmm3" \
 "rsqrtps xmm4,xmm3" "movaps xmm5,xmm4" "mulps xmm5,xmm4" "mulps xmm5,xmm3" "movaps xmm6,[esi+16]" "subps xmm6,xmm5" "mulps xmm6,xmm4" "mulps xmm6,[esi]" \
 "xorps xmm7,xmm7" "cmpneqps xmm7,xmm3" \
 "movaps xmm5,xmm0" "mulps xmm5,xmm6" "andps xmm5,xmm7" "movaps xmm3,xmm7" "andnps xmm3,xmm0" "orps xmm5,xmm3" "movaps [edi],xmm5" \
 "movaps xmm5,xmm1" "mulps xmm5,xmm6" "andps xmm5,xmm7" "movaps xmm3,xmm7" "andnps xmm3,xmm1" "orps xmm5,xmm3" "movaps [edi+16],xmm5" \
 "movaps xmm5,xmm2" "mulps xmm5,xmm6" "andps xmm5,xmm7" "movaps xmm3,xmm7" "andnps xmm3,xmm2" "orps xmm5,xmm3" "movaps [edi+32],xmm5" \
 parm [eax] [edx] [ebx] [ecx] [edi] [esi] modify exact [];
/* a positional light: vk_sub3 (vp = p - e), vk_dot3 (d2), vk_rsqrt_nr (invd), vk_mul (d = d2 invd), vk_scale3 (vp invd); k 0.5, 3, d2 at +32, d at +48 */
static void vk_lpos(v4x3 *vp,const v4x3 *p,const v4x3 *e,v4 *k);
#pragma aux vk_lpos = "movaps xmm0,[edx]" "subps xmm0,[ebx]" "movaps xmm1,[edx+16]" "subps xmm1,[ebx+16]" "movaps xmm2,[edx+32]" "subps xmm2,[ebx+32]" \
 "movaps xmm3,xmm0" "mulps xmm3,xmm0" "movaps xmm7,xmm1" "mulps xmm7,xmm1" "addps xmm3,xmm7" "movaps xmm7,xmm2" "mulps xmm7,xmm2" "addps xmm3,xmm7" "movaps [ecx+32],xmm3" \
 "rsqrtps xmm4,xmm3" "movaps xmm5,xmm4" "mulps xmm5,xmm4" "mulps xmm5,xmm3" "movaps xmm6,[ecx+16]" "subps xmm6,xmm5" "mulps xmm6,xmm4" "mulps xmm6,[ecx]" \
 "movaps xmm7,xmm3" "mulps xmm7,xmm6" "movaps [ecx+48],xmm7" \
 "mulps xmm0,xmm6" "mulps xmm1,xmm6" "mulps xmm2,xmm6" "movaps [eax],xmm0" "movaps [eax+16],xmm1" "movaps [eax+32],xmm2" \
 parm [eax] [edx] [ebx] [ecx] modify exact [];
/* vk_dot3 (n.VP), vk_gemask (n.VP >= 0), vk_unord (n.VP), vk_madd3 (c = dif n.VP + amb), vk_blend3 (back-facing: amb): returns the mask's lanes, the NaN lanes << 4 */
static unsigned vk_ldiff(v4x3 *c,const v4x3 *n,const v4x3 *vp,const v4x3 *dif,const v4x3 *amb);
#pragma aux vk_ldiff = "movaps xmm0,[edx]" "mulps xmm0,[ebx]" "movaps xmm1,[edx+16]" "mulps xmm1,[ebx+16]" "addps xmm0,xmm1" "movaps xmm1,[edx+32]" "mulps xmm1,[ebx+32]" "addps xmm0,xmm1" \
 "xorps xmm7,xmm7" "cmpleps xmm7,xmm0" "movmskps edx,xmm7" "movaps xmm6,xmm0" "cmpunordps xmm6,xmm6" "movmskps ebx,xmm6" "shl ebx,4" "or edx,ebx" \
 "movaps xmm1,[ecx]" "mulps xmm1,xmm0" "addps xmm1,[esi]" "andps xmm1,xmm7" "movaps xmm2,xmm7" "andnps xmm2,[esi]" "orps xmm1,xmm2" "movaps [eax],xmm1" \
 "movaps xmm1,[ecx+16]" "mulps xmm1,xmm0" "addps xmm1,[esi+16]" "andps xmm1,xmm7" "movaps xmm2,xmm7" "andnps xmm2,[esi+16]" "orps xmm1,xmm2" "movaps [eax+16],xmm1" \
 "movaps xmm1,[ecx+32]" "mulps xmm1,xmm0" "addps xmm1,[esi+32]" "andps xmm1,xmm7" "movaps xmm2,xmm7" "andnps xmm2,[esi+32]" "orps xmm1,xmm2" "movaps [eax+32],xmm1" "mov eax,edx" \
 parm [eax] [edx] [ebx] [ecx] [esi] value [eax] modify exact [eax ebx edx];
/* vk_madd3 (t = c att + s), vk_blend3 (s = amask ? t : s) */
static void vk_lacc(v4x3 *s,const v4x3 *c,const v4 *att,const v4 *amask);
#pragma aux vk_lacc = "movaps xmm4,[ebx]" "movaps xmm5,[ecx]" \
 "movaps xmm0,[edx]" "mulps xmm0,xmm4" "addps xmm0,[eax]" "andps xmm0,xmm5" "movaps xmm1,xmm5" "andnps xmm1,[eax]" "orps xmm0,xmm1" "movaps [eax],xmm0" \
 "movaps xmm0,[edx+16]" "mulps xmm0,xmm4" "addps xmm0,[eax+16]" "andps xmm0,xmm5" "movaps xmm1,xmm5" "andnps xmm1,[eax+16]" "orps xmm0,xmm1" "movaps [eax+16],xmm0" \
 "movaps xmm0,[edx+32]" "mulps xmm0,xmm4" "addps xmm0,[eax+32]" "andps xmm0,xmm5" "movaps xmm1,xmm5" "andnps xmm1,[eax+32]" "orps xmm0,xmm1" "movaps [eax+32],xmm0" \
 parm [eax] [edx] [ebx] [ecx] modify exact [];
/* vk_madd3 (c = spc spec + c), then vk_lacc */
static void vk_laccs(v4x3 *s,const v4x3 *c,const v4 *att,const v4 *amask,const v4x3 *spc,const v4 *spec);
#pragma aux vk_laccs = "movaps xmm4,[ebx]" "movaps xmm5,[ecx]" "movaps xmm6,[edi]" \
 "movaps xmm0,[esi]" "mulps xmm0,xmm6" "addps xmm0,[edx]" "mulps xmm0,xmm4" "addps xmm0,[eax]" "andps xmm0,xmm5" "movaps xmm1,xmm5" "andnps xmm1,[eax]" "orps xmm0,xmm1" "movaps [eax],xmm0" \
 "movaps xmm0,[esi+16]" "mulps xmm0,xmm6" "addps xmm0,[edx+16]" "mulps xmm0,xmm4" "addps xmm0,[eax+16]" "andps xmm0,xmm5" "movaps xmm1,xmm5" "andnps xmm1,[eax+16]" "orps xmm0,xmm1" "movaps [eax+16],xmm0" \
 "movaps xmm0,[esi+32]" "mulps xmm0,xmm6" "addps xmm0,[edx+32]" "mulps xmm0,xmm4" "addps xmm0,[eax+32]" "andps xmm0,xmm5" "movaps xmm1,xmm5" "andnps xmm1,[eax+32]" "orps xmm0,xmm1" "movaps [eax+32],xmm0" \
 parm [eax] [edx] [ebx] [ecx] [esi] [edi] modify exact [];
/* the half vector: vk_sub3 (h = vp - v; vk_lhalf_add: vk_add3, h = vp + o), vk_norm3 (its length at out), vk_dot3 (n.h at out + 16); k 0.5, 3 */
#define VK_LHALF_TAIL \
 "movaps xmm3,xmm0" "mulps xmm3,xmm3" "movaps xmm4,xmm1" "mulps xmm4,xmm1" "addps xmm3,xmm4" "movaps xmm4,xmm2" "mulps xmm4,xmm2" "addps xmm3,xmm4" "movaps [eax],xmm3" \
 "rsqrtps xmm4,xmm3" "movaps xmm5,xmm4" "mulps xmm5,xmm4" "mulps xmm5,xmm3" "movaps xmm6,[esi+16]" "subps xmm6,xmm5" "mulps xmm6,xmm4" "mulps xmm6,[esi]" \
 "xorps xmm7,xmm7" "cmpneqps xmm7,xmm3" \
 "movaps xmm3,xmm7" "andnps xmm3,xmm0" "mulps xmm0,xmm6" "andps xmm0,xmm7" "orps xmm0,xmm3" \
 "movaps xmm3,xmm7" "andnps xmm3,xmm1" "mulps xmm1,xmm6" "andps xmm1,xmm7" "orps xmm1,xmm3" \
 "movaps xmm3,xmm7" "andnps xmm3,xmm2" "mulps xmm2,xmm6" "andps xmm2,xmm7" "orps xmm2,xmm3" \
 "movaps xmm3,[ecx]" "mulps xmm3,xmm0" "movaps xmm4,[ecx+16]" "mulps xmm4,xmm1" "addps xmm3,xmm4" "movaps xmm4,[ecx+32]" "mulps xmm4,xmm2" "addps xmm3,xmm4" "movaps [eax+16],xmm3"
static void vk_lhalf_sub(v4 *out,const v4x3 *vp,const v4x3 *o,const v4x3 *n,const v4 *k);
#pragma aux vk_lhalf_sub = "movaps xmm0,[edx]" "subps xmm0,[ebx]" "movaps xmm1,[edx+16]" "subps xmm1,[ebx+16]" "movaps xmm2,[edx+32]" "subps xmm2,[ebx+32]" VK_LHALF_TAIL \
 parm [eax] [edx] [ebx] [ecx] [esi] modify exact [];
static void vk_lhalf_add(v4 *out,const v4x3 *vp,const v4x3 *o,const v4x3 *n,const v4 *k);
#pragma aux vk_lhalf_add = "movaps xmm0,[edx]" "addps xmm0,[ebx]" "movaps xmm1,[edx+16]" "addps xmm1,[ebx+16]" "movaps xmm2,[edx+32]" "addps xmm2,[ebx+32]" VK_LHALF_TAIL \
 parm [eax] [edx] [ebx] [ecx] [esi] modify exact [];
/* v10.497 (ACCEL_OPENGL_SSESNAP2): the group's window x and y (vk_front's, at x and y) on snapw's grid: vk_snapd for x, vk_snapd (vk_snap2d) or
 * vk_snapu (vk_snap2u: y up, the window's flip) for y, the same steps; written as four x y pairs at o; returns vk_snapok's lanes for x, for y << 4
 * (a lane not in range: snapw itself, by the caller). k: vk_snapd's constants (the abs mask +0 .. 2040 +128, the sign mask +112) */
#define VK_SNAP_D "movaps xmm7,xmm0" "andps xmm7,[ecx+112]" "andps xmm0,[ecx]" "mulps xmm0,[ecx+16]" \
 "movaps xmm1,xmm0" "addps xmm1,[ecx+32]" "subps xmm1,[ecx+32]" "movaps xmm2,xmm0" "cmpltps xmm2,xmm1" "andps xmm2,[ecx+48]" "subps xmm1,xmm2" \
 "subps xmm0,xmm1" "movaps xmm2,[ecx+64]" "cmpleps xmm2,xmm0" "andps xmm2,[ecx+48]" "addps xmm1,xmm2" "orps xmm1,xmm7" "mulps xmm1,[ecx+80]" \
 "movaps xmm0,xmm1" "addps xmm0,[ecx+32]" "subps xmm0,[ecx+32]" "movaps xmm2,xmm1" "cmpltps xmm2,xmm0" "andps xmm2,[ecx+48]" "subps xmm0,xmm2" \
 "mulps xmm0,[ecx+96]"
#define VK_SNAP_U "movaps xmm7,xmm0" "andps xmm7,[ecx+112]" "andps xmm0,[ecx]" "mulps xmm0,[ecx+16]" \
 "movaps xmm1,xmm0" "addps xmm1,[ecx+32]" "subps xmm1,[ecx+32]" "movaps xmm2,xmm0" "cmpltps xmm2,xmm1" "andps xmm2,[ecx+48]" "subps xmm1,xmm2" \
 "subps xmm0,xmm1" "movaps xmm2,[ecx+64]" "cmpleps xmm2,xmm0" "andps xmm2,[ecx+48]" "addps xmm1,xmm2" "orps xmm1,xmm7" "mulps xmm1,[ecx+80]" \
 "movaps xmm0,xmm1" "addps xmm0,[ecx+32]" "subps xmm0,[ecx+32]" "movaps xmm2,xmm0" "cmpltps xmm2,xmm1" "andps xmm2,[ecx+48]" "addps xmm0,xmm2" \
 "mulps xmm0,[ecx+96]"
#define VK_SNAP_OK "movaps xmm3,[edx]" "andps xmm3,[ecx]" "movaps xmm4,[ecx+128]" "cmpltps xmm3,xmm4" "movmskps eax,xmm3" \
 "movaps xmm3,[ebx]" "andps xmm3,[ecx]" "movaps xmm4,[ecx+128]" "cmpltps xmm3,xmm4" "movmskps esi,xmm3" "shl esi,4" "or eax,esi"
/* v10.497: vk_xf4 then vk_clipbits, the clip coordinates kept in the registers (all_outside: no store) */
static unsigned vk_xfbits(const v4 *k,const GLfloat *x,const GLfloat *y,const GLfloat *z);
#pragma aux vk_xfbits = "movaps xmm4,[edx]" "movaps xmm5,[ebx]" "movaps xmm6,[ecx]" \
 "movaps xmm0,[eax]" "mulps xmm0,xmm4" "movaps xmm7,[eax+64]" "mulps xmm7,xmm5" "addps xmm0,xmm7" "movaps xmm7,[eax+128]" "mulps xmm7,xmm6" "addps xmm0,xmm7" "addps xmm0,[eax+192]" \
 "movaps xmm1,[eax+16]" "mulps xmm1,xmm4" "movaps xmm7,[eax+80]" "mulps xmm7,xmm5" "addps xmm1,xmm7" "movaps xmm7,[eax+144]" "mulps xmm7,xmm6" "addps xmm1,xmm7" "addps xmm1,[eax+208]" \
 "movaps xmm2,[eax+32]" "mulps xmm2,xmm4" "movaps xmm7,[eax+96]" "mulps xmm7,xmm5" "addps xmm2,xmm7" "movaps xmm7,[eax+160]" "mulps xmm7,xmm6" "addps xmm2,xmm7" "addps xmm2,[eax+224]" \
 "movaps xmm3,[eax+48]" "mulps xmm3,xmm4" "movaps xmm7,[eax+112]" "mulps xmm7,xmm5" "addps xmm3,xmm7" "movaps xmm7,[eax+176]" "mulps xmm7,xmm6" "addps xmm3,xmm7" "addps xmm3,[eax+240]" \
 "movaps xmm7,xmm3" "cmpltps xmm7,xmm0" "movmskps edx,xmm7" \
 "movaps xmm7,xmm3" "cmpltps xmm7,xmm1" "movmskps ebx,xmm7" "shl ebx,4" "or edx,ebx" \
 "movaps xmm7,xmm3" "cmpltps xmm7,xmm2" "movmskps ebx,xmm7" "shl ebx,8" "or edx,ebx" \
 "movaps xmm5,[eax+256]" \
 "movaps xmm6,xmm0" "xorps xmm6,xmm5" "movaps xmm7,xmm3" "cmpltps xmm7,xmm6" "movmskps ebx,xmm7" "shl ebx,12" "or edx,ebx" \
 "movaps xmm6,xmm1" "xorps xmm6,xmm5" "movaps xmm7,xmm3" "cmpltps xmm7,xmm6" "movmskps ebx,xmm7" "shl ebx,16" "or edx,ebx" \
 "movaps xmm6,xmm2" "xorps xmm6,xmm5" "movaps xmm7,xmm3" "cmpltps xmm7,xmm6" "movmskps ebx,xmm7" "shl ebx,20" "or edx,ebx" "mov eax,edx" \
 parm [eax] [edx] [ebx] [ecx] value [eax] modify exact [eax ebx edx];
/* v10.497: a draw's box four lanes at a time: b[0..5] min x, max x, min y, max y, min z, max z (minps / maxps), b[6] ORs |p| >= 1e30 or a
 * NaN (cmpnltps against b[8], b[7] the abs mask) */
static void vk_box3(v4 *b,const GLfloat *x,const GLfloat *y,const GLfloat *z);
#pragma aux vk_box3 = "movaps xmm7,[eax+112]" "movaps xmm6,[eax+128]" "movaps xmm5,[eax+96]" \
 "movaps xmm0,[edx]" "movaps xmm1,[eax]" "minps xmm1,xmm0" "movaps [eax],xmm1" "movaps xmm1,[eax+16]" "maxps xmm1,xmm0" "movaps [eax+16],xmm1" "andps xmm0,xmm7" "cmpnltps xmm0,xmm6" "orps xmm5,xmm0" \
 "movaps xmm0,[ebx]" "movaps xmm1,[eax+32]" "minps xmm1,xmm0" "movaps [eax+32],xmm1" "movaps xmm1,[eax+48]" "maxps xmm1,xmm0" "movaps [eax+48],xmm1" "andps xmm0,xmm7" "cmpnltps xmm0,xmm6" "orps xmm5,xmm0" \
 "movaps xmm0,[ecx]" "movaps xmm1,[eax+64]" "minps xmm1,xmm0" "movaps [eax+64],xmm1" "movaps xmm1,[eax+80]" "maxps xmm1,xmm0" "movaps [eax+80],xmm1" "andps xmm0,xmm7" "cmpnltps xmm0,xmm6" "orps xmm5,xmm0" \
 "movaps [eax+96],xmm5" parm [eax] [edx] [ebx] [ecx] modify exact [];
static unsigned vk_snap2d(GLuint *o,const v4 *x,const v4 *y,const v4 *k);
#pragma aux vk_snap2d = "movaps xmm0,[edx]" VK_SNAP_D "movaps xmm5,xmm0" "movaps xmm0,[ebx]" VK_SNAP_D \
 "movaps xmm6,xmm5" "unpcklps xmm6,xmm0" "movups [eax],xmm6" "unpckhps xmm5,xmm0" "movups [eax+16],xmm5" VK_SNAP_OK \
 parm [eax] [edx] [ebx] [ecx] value [eax] modify exact [eax esi];
static unsigned vk_snap2u(GLuint *o,const v4 *x,const v4 *y,const v4 *k);
#pragma aux vk_snap2u = "movaps xmm0,[edx]" VK_SNAP_D "movaps xmm5,xmm0" "movaps xmm0,[ebx]" VK_SNAP_U \
 "movaps xmm6,xmm5" "unpcklps xmm6,xmm0" "movups [eax],xmm6" "unpckhps xmm5,xmm0" "movups [eax+16],xmm5" VK_SNAP_OK \
 parm [eax] [edx] [ebx] [ecx] value [eax] modify exact [eax esi];
#define VK_SSE 1

#elif defined(__GNUC__) && defined(__SSE__) /* the host tests: the same steps by intrinsics */
#include <xmmintrin.h>
#define L(p) _mm_load_ps((const float*)(p))
#define S(p,v) _mm_store_ps((float*)(p),v)
static unsigned mx_enter(void) {unsigned o=_mm_getcsr();_mm_setcsr(0x1F80);return o;}
static void mx_leave(unsigned o) {_mm_setcsr(o);}
static void vk_xf4(const v4 *m,const GLfloat *x,const GLfloat *y,const GLfloat *z,v4x4 *out) {
 __m128 X=L(x),Y=L(y),Z=L(z);int r;v4 *o=&out->x;
 for(r=0;r<4;r++){__m128 a=_mm_mul_ps(L(&m[r]),X),b=_mm_mul_ps(L(&m[4+r]),Y);a=_mm_add_ps(a,b);b=_mm_mul_ps(L(&m[8+r]),Z);a=_mm_add_ps(a,b);a=_mm_add_ps(a,L(&m[12+r]));S(&o[r],a);}}
static void vk_xf3(const v4 *m,const GLfloat *x,const GLfloat *y,const GLfloat *z,v4x3 *out) {
 __m128 X=L(x),Y=L(y),Z=L(z);int r;v4 *o=&out->x;
 for(r=0;r<3;r++){__m128 a=_mm_mul_ps(L(&m[r]),X),b=_mm_mul_ps(L(&m[4+r]),Y);a=_mm_add_ps(a,b);b=_mm_mul_ps(L(&m[8+r]),Z);a=_mm_add_ps(a,b);a=_mm_add_ps(a,L(&m[12+r]));S(&o[r],a);}}
static void vk_xfn(const v4 *n,const GLfloat *x,const GLfloat *y,const GLfloat *z,v4x3 *out) {
 __m128 X=L(x),Y=L(y),Z=L(z);int r;v4 *o=&out->x;
 for(r=0;r<3;r++){__m128 a=_mm_mul_ps(X,L(&n[3*r])),b=_mm_mul_ps(Y,L(&n[3*r+1]));a=_mm_add_ps(a,b);b=_mm_mul_ps(Z,L(&n[3*r+2]));a=_mm_add_ps(a,b);S(&o[r],a);}}
static void vk_norm3(v4x3 *v,const v4 *k,v4 *len) {
 __m128 x=L(&v->x),y=L(&v->y),z=L(&v->z),l=_mm_add_ps(_mm_add_ps(_mm_mul_ps(x,x),_mm_mul_ps(y,y)),_mm_mul_ps(z,z)),r,t,s,m;
 S(len,l);r=_mm_rsqrt_ps(l);t=_mm_mul_ps(_mm_mul_ps(r,r),l);s=_mm_mul_ps(_mm_mul_ps(_mm_sub_ps(L(&k[1]),t),r),L(&k[0]));m=_mm_cmpneq_ps(_mm_setzero_ps(),l);
 S(&v->x,_mm_or_ps(_mm_and_ps(_mm_mul_ps(x,s),m),_mm_andnot_ps(m,x)));S(&v->y,_mm_or_ps(_mm_and_ps(_mm_mul_ps(y,s),m),_mm_andnot_ps(m,y)));S(&v->z,_mm_or_ps(_mm_and_ps(_mm_mul_ps(z,s),m),_mm_andnot_ps(m,z)));}
static void vk_dot3(const v4x3 *a,const v4x3 *b,v4 *out) {S(out,_mm_add_ps(_mm_add_ps(_mm_mul_ps(L(&a->x),L(&b->x)),_mm_mul_ps(L(&a->y),L(&b->y))),_mm_mul_ps(L(&a->z),L(&b->z))));}
static void vk_sub3(v4x3 *o,const v4x3 *a,const v4x3 *b) {S(&o->x,_mm_sub_ps(L(&a->x),L(&b->x)));S(&o->y,_mm_sub_ps(L(&a->y),L(&b->y)));S(&o->z,_mm_sub_ps(L(&a->z),L(&b->z)));}
static void vk_mul3(v4x3 *o,const v4x3 *a,const v4x3 *b) {S(&o->x,_mm_mul_ps(L(&a->x),L(&b->x)));S(&o->y,_mm_mul_ps(L(&a->y),L(&b->y)));S(&o->z,_mm_mul_ps(L(&a->z),L(&b->z)));}
static void vk_add3(v4x3 *o,const v4x3 *a,const v4x3 *b) {S(&o->x,_mm_add_ps(L(&a->x),L(&b->x)));S(&o->y,_mm_add_ps(L(&a->y),L(&b->y)));S(&o->z,_mm_add_ps(L(&a->z),L(&b->z)));}
static void vk_scale3(v4x3 *v,const v4 *s) {__m128 m=L(s);S(&v->x,_mm_mul_ps(L(&v->x),m));S(&v->y,_mm_mul_ps(L(&v->y),m));S(&v->z,_mm_mul_ps(L(&v->z),m));}
static void vk_madd3(v4x3 *o,const v4x3 *a,const v4 *s,const v4x3 *b) {__m128 m=L(s);S(&o->x,_mm_add_ps(_mm_mul_ps(L(&a->x),m),L(&b->x)));S(&o->y,_mm_add_ps(_mm_mul_ps(L(&a->y),m),L(&b->y)));S(&o->z,_mm_add_ps(_mm_mul_ps(L(&a->z),m),L(&b->z)));}
static void vk_blend3(v4x3 *o,const v4 *mask,const v4x3 *a,const v4x3 *b) {__m128 m=L(mask);
 S(&o->x,_mm_or_ps(_mm_and_ps(L(&a->x),m),_mm_andnot_ps(m,L(&b->x))));S(&o->y,_mm_or_ps(_mm_and_ps(L(&a->y),m),_mm_andnot_ps(m,L(&b->y))));S(&o->z,_mm_or_ps(_mm_and_ps(L(&a->z),m),_mm_andnot_ps(m,L(&b->z))));}
static void vk_rsqrt_nr(v4 *o,const v4 *in,const v4 *k) {__m128 a=L(in),r=_mm_rsqrt_ps(a),t=_mm_mul_ps(_mm_mul_ps(r,r),a);S(o,_mm_mul_ps(_mm_mul_ps(_mm_sub_ps(L(&k[1]),t),r),L(&k[0])));}
static void vk_rcp_nr(v4 *o,const v4 *in) {__m128 a=L(in),r=_mm_rcp_ps(a),t=_mm_mul_ps(_mm_mul_ps(r,a),r);S(o,_mm_sub_ps(_mm_add_ps(r,r),t));}
static void vk_mul(v4 *o,const v4 *a,const v4 *b) {S(o,_mm_mul_ps(L(a),L(b)));}
static void vk_mad(v4 *o,const v4 *a,const v4 *b,const v4 *c) {S(o,_mm_add_ps(_mm_mul_ps(L(a),L(b)),L(c)));}
static unsigned vk_clipbits(const v4x4 *c,const v4 *sign) {
 __m128 x=L(&c->x),y=L(&c->y),z=L(&c->z),w=L(&c->w),s=L(sign);unsigned r;
 r=(unsigned)_mm_movemask_ps(_mm_cmplt_ps(w,x));r|=(unsigned)_mm_movemask_ps(_mm_cmplt_ps(w,y))<<4;r|=(unsigned)_mm_movemask_ps(_mm_cmplt_ps(w,z))<<8;
 r|=(unsigned)_mm_movemask_ps(_mm_cmplt_ps(w,_mm_xor_ps(x,s)))<<12;r|=(unsigned)_mm_movemask_ps(_mm_cmplt_ps(w,_mm_xor_ps(y,s)))<<16;r|=(unsigned)_mm_movemask_ps(_mm_cmplt_ps(w,_mm_xor_ps(z,s)))<<20;return r;}
static void vk_project(const v4x4 *c,const v4 *keep,const v4 *one,v4x4 *o) {
 __m128 w=L(&c->w),oow=_mm_div_ps(L(one),w),k=L(keep);
 S(&o->x,_mm_and_ps(_mm_mul_ps(L(&c->x),oow),k));S(&o->y,_mm_and_ps(_mm_mul_ps(L(&c->y),oow),k));S(&o->z,_mm_and_ps(_mm_mul_ps(L(&c->z),oow),k));
 S(&o->w,_mm_or_ps(_mm_and_ps(oow,k),_mm_andnot_ps(k,L(one))));}
static void vk_viewport_store(const v4x4 *p,const v4 *vp,GLubyte *dst,unsigned stride) {
 __m128 x=_mm_add_ps(_mm_mul_ps(L(&p->x),L(&vp[0])),L(&vp[3])),y=_mm_add_ps(_mm_mul_ps(L(&p->y),L(&vp[1])),L(&vp[4])),z=_mm_add_ps(_mm_mul_ps(L(&p->z),L(&vp[2])),L(&vp[5])),w=L(&p->w);
 __m128 t0=_mm_unpacklo_ps(x,y),t1=_mm_unpacklo_ps(z,w),t2=_mm_unpackhi_ps(x,y),t3=_mm_unpackhi_ps(z,w);
 _mm_storeu_ps((float*)dst,_mm_movelh_ps(t0,t1));_mm_storeu_ps((float*)(dst+stride),_mm_movehl_ps(t1,t0));
 _mm_storeu_ps((float*)(dst+2*stride),_mm_movelh_ps(t2,t3));_mm_storeu_ps((float*)(dst+3*stride),_mm_movehl_ps(t3,t2));}
static void vk_tr_store(const v4x4 *c,GLfloat *dst) {
 __m128 x=L(&c->x),y=L(&c->y),z=L(&c->z),w=L(&c->w),t0=_mm_unpacklo_ps(x,y),t1=_mm_unpacklo_ps(z,w),t2=_mm_unpackhi_ps(x,y),t3=_mm_unpackhi_ps(z,w);
 S(dst,_mm_movelh_ps(t0,t1));S(dst+4,_mm_movehl_ps(t1,t0));S(dst+8,_mm_movelh_ps(t2,t3));S(dst+12,_mm_movehl_ps(t3,t2));}
static void vk_bias(v4 *o,const v4 *in,const v4 *k) {S(o,_mm_add_ps(_mm_mul_ps(L(in),L(&k[0])),L(&k[1])));}
static void vk_linfog(v4 *o,const v4 *z,const v4 *k) {__m128 a=_mm_and_ps(L(z),L(&k[0])),f=_mm_mul_ps(_mm_sub_ps(L(&k[1]),a),L(&k[2]));S(o,_mm_min_ps(_mm_max_ps(f,L(&k[3])),L(&k[4])));}
static void vk_ub(v4 *o,const v4 *in,const v4 *k) {__m128 x=_mm_max_ps(L(in),L(&k[2])),m=_mm_cmple_ps(L(&k[3]),x);x=_mm_or_ps(_mm_andnot_ps(m,x),_mm_and_ps(m,L(&k[4])));S(o,_mm_add_ps(_mm_mul_ps(x,L(&k[0])),L(&k[1])));}
static unsigned vk_unord(const v4 *in) {__m128 a=L(in);return (unsigned)_mm_movemask_ps(_mm_cmpunord_ps(a,a));}
static unsigned vk_range(const v4 *v,const v4 *k) {__m128 x=L(v);return (unsigned)_mm_movemask_ps(_mm_and_ps(_mm_cmplt_ps(L(&k[0]),x),_mm_cmplt_ps(x,L(&k[1]))));} /* .505 */
#include <emmintrin.h>
static void vk_pack(GLubyte *d,const v4 *rgb,const v4 *a,unsigned vs) { /* .503: the same steps by SSE2 intrinsics */
 const __m128i m=_mm_srli_epi32(_mm_set1_epi32(-1),24);__m128i x=_mm_and_si128(_mm_load_si128((const __m128i*)&rgb[2]),m);
 x=_mm_or_si128(x,_mm_slli_epi32(_mm_and_si128(_mm_load_si128((const __m128i*)&rgb[1]),m),8));
 x=_mm_or_si128(x,_mm_slli_epi32(_mm_and_si128(_mm_load_si128((const __m128i*)&rgb[0]),m),16));
 x=_mm_or_si128(x,_mm_slli_epi32(_mm_load_si128((const __m128i*)a),24));
 *(GLuint*)d=(GLuint)_mm_cvtsi128_si32(x);x=_mm_srli_si128(x,4);d+=vs;*(GLuint*)d=(GLuint)_mm_cvtsi128_si32(x);x=_mm_srli_si128(x,4);d+=vs;
 *(GLuint*)d=(GLuint)_mm_cvtsi128_si32(x);x=_mm_srli_si128(x,4);d+=vs;*(GLuint*)d=(GLuint)_mm_cvtsi128_si32(x);
}
static unsigned vk_gemask(v4 *mask,const v4 *a,const v4 *b) {__m128 m=_mm_cmple_ps(L(b),L(a));S(mask,m);return (unsigned)_mm_movemask_ps(m);}
static void vk_shine_a(v4 *sh,const v4 *ndh,const v4 *k) {__m128 f=_mm_mul_ps(L(ndh),L(&k[0])),t=_mm_sub_ps(_mm_add_ps(f,L(&k[1])),L(&k[1]));
 t=_mm_sub_ps(t,_mm_and_ps(_mm_cmplt_ps(f,t),L(&k[2])));S(&sh[0],f);S(&sh[1],t);S(&sh[2],_mm_add_ps(t,L(&k[1])));}
static void vk_shine_b(v4 *o,const v4 *sh) {S(o,_mm_add_ps(_mm_mul_ps(_mm_sub_ps(L(&sh[0]),L(&sh[1])),_mm_sub_ps(L(&sh[4]),L(&sh[3]))),L(&sh[3])));}
static __m128 snap_n(const v4 *in,const v4 *k) {__m128 x=L(in),sg=_mm_and_ps(x,L(&k[7])),a=_mm_mul_ps(_mm_and_ps(x,L(&k[0])),L(&k[1])),r=_mm_sub_ps(_mm_add_ps(a,L(&k[2])),L(&k[2]));
 r=_mm_sub_ps(r,_mm_and_ps(_mm_cmplt_ps(a,r),L(&k[3])));r=_mm_add_ps(r,_mm_and_ps(_mm_cmple_ps(L(&k[4]),_mm_sub_ps(a,r)),L(&k[3])));return _mm_mul_ps(_mm_or_ps(r,sg),L(&k[5]));}
static void vk_snapd(v4 *o,const v4 *in,const v4 *k) {__m128 q=snap_n(in,k),t=_mm_sub_ps(_mm_add_ps(q,L(&k[2])),L(&k[2]));t=_mm_sub_ps(t,_mm_and_ps(_mm_cmplt_ps(q,t),L(&k[3])));S(o,_mm_mul_ps(t,L(&k[6])));}
static void vk_snapu(v4 *o,const v4 *in,const v4 *k) {__m128 q=snap_n(in,k),t=_mm_sub_ps(_mm_add_ps(q,L(&k[2])),L(&k[2]));t=_mm_add_ps(t,_mm_and_ps(_mm_cmplt_ps(t,q),L(&k[3])));S(o,_mm_mul_ps(t,L(&k[6])));}
static unsigned vk_snapok(const v4 *in,const v4 *k) {return (unsigned)_mm_movemask_ps(_mm_cmplt_ps(_mm_and_ps(L(in),L(&k[0])),L(&k[8])));}
/* v10.495: the fused kernels' twins, the kernels they replace in turn (the DLL's own fused code is checked against those by vxgl_ssepass_ktest 4) */
#define A16 __attribute__((aligned(16)))
static unsigned vk_front(const v4 *k,const GLfloat *x,const GLfloat *y,const GLfloat *z,GLubyte *dst,GLfloat *clip) {
 v4x4 c4 A16,p4 A16;v4 keep A16;unsigned bits,v,kp=0;GLuint *u=(GLuint*)keep.f;
 vk_xf4(k,x,y,z,&c4);bits=vk_clipbits(&c4,&k[16]);
 for(v=0;v<4;v++)if(!((bits>>v)&0x111111U))kp|=1U<<v;
 for(v=0;v<4;v++)u[v]=(kp>>v)&1?0xFFFFFFFFU:0;
 vk_project(&c4,&keep,&k[17],&p4);vk_viewport_store(&p4,&k[18],dst,*(const GLuint*)k[24].f);vk_tr_store(&c4,clip);
 S(&((v4*)k)[25],_mm_add_ps(_mm_mul_ps(L(&p4.x),L(&k[18])),L(&k[21])));S(&((v4*)k)[26],_mm_add_ps(_mm_mul_ps(L(&p4.y),L(&k[19])),L(&k[22])));return bits;}
static void vk_front_in(const v4 *k,const GLfloat *x,const GLfloat *y,const GLfloat *z,GLubyte *dst,GLfloat *clip) {
 v4x4 c4 A16,p4 A16;v4 keep A16;GLuint *u=(GLuint*)keep.f;u[0]=u[1]=u[2]=u[3]=0xFFFFFFFFU;
 vk_xf4(k,x,y,z,&c4);vk_project(&c4,&keep,&k[17],&p4);vk_viewport_store(&p4,&k[18],dst,*(const GLuint*)k[24].f);vk_tr_store(&c4,clip);
 S(&((v4*)k)[25],_mm_add_ps(_mm_mul_ps(L(&p4.x),L(&k[18])),L(&k[21])));S(&((v4*)k)[26],_mm_add_ps(_mm_mul_ps(L(&p4.y),L(&k[19])),L(&k[22])));}
static void vk_xfn_norm(const v4 *n,const GLfloat *x,const GLfloat *y,const GLfloat *z,v4x3 *out,v4 *k) {vk_xfn(n,x,y,z,out);vk_norm3(out,k,&k[2]);}
static void vk_lpos(v4x3 *vp,const v4x3 *p,const v4x3 *e,v4 *k) {v4 invd A16;vk_sub3(vp,p,e);vk_dot3(vp,vp,&k[2]);vk_rsqrt_nr(&invd,&k[2],k);vk_mul(&k[3],&k[2],&invd);vk_scale3(vp,&invd);}
static unsigned vk_ldiff(v4x3 *c,const v4x3 *n,const v4x3 *vp,const v4x3 *dif,const v4x3 *amb) {
 v4 ndv A16,fm A16,zero A16;unsigned m;zero.f[0]=zero.f[1]=zero.f[2]=zero.f[3]=0.0F;vk_dot3(n,vp,&ndv);m=vk_gemask(&fm,&ndv,&zero);m|=vk_unord(&ndv)<<4;vk_madd3(c,dif,&ndv,amb);vk_blend3(c,&fm,c,amb);return m;}
static void vk_lacc(v4x3 *s,const v4x3 *c,const v4 *att,const v4 *amask) {v4x3 t A16;vk_madd3(&t,c,att,s);vk_blend3(s,amask,&t,s);}
static void vk_laccs(v4x3 *s,const v4x3 *c,const v4 *att,const v4 *amask,const v4x3 *spc,const v4 *spec) {v4x3 c2 A16;vk_madd3(&c2,spc,spec,c);vk_lacc(s,&c2,att,amask);}
static void vk_lhalf_sub(v4 *out,const v4x3 *vp,const v4x3 *o,const v4x3 *n,const v4 *k) {v4x3 h A16;vk_sub3(&h,vp,o);vk_norm3(&h,k,&out[0]);vk_dot3(n,&h,&out[1]);}
static void vk_lhalf_add(v4 *out,const v4x3 *vp,const v4x3 *o,const v4x3 *n,const v4 *k) {v4x3 h A16;vk_add3(&h,vp,o);vk_norm3(&h,k,&out[0]);vk_dot3(n,&h,&out[1]);}
static unsigned vk_snap2(GLuint *o,const v4 *x,const v4 *y,const v4 *k,int up) {v4 a A16,b A16;GLuint v;unsigned m;
 vk_snapd(&a,x,k);if(up)vk_snapu(&b,y,k);else vk_snapd(&b,y,k);for(v=0;v<4;v++){o[2*v]=LANE_U(a,v);o[2*v+1]=LANE_U(b,v);}
 m=vk_snapok(x,k);m|=vk_snapok(y,k)<<4;return m;}
static unsigned vk_snap2d(GLuint *o,const v4 *x,const v4 *y,const v4 *k) {return vk_snap2(o,x,y,k,0);}
static void vk_box3(v4 *b,const GLfloat *x,const GLfloat *y,const GLfloat *z) {const GLfloat *p[3];int j;p[0]=x;p[1]=y;p[2]=z;
 for(j=0;j<3;j++){__m128 v=L(p[j]);S(&b[2*j],_mm_min_ps(L(&b[2*j]),v));S(&b[2*j+1],_mm_max_ps(L(&b[2*j+1]),v));S(&b[6],_mm_or_ps(L(&b[6]),_mm_cmpnlt_ps(_mm_and_ps(v,L(&b[7])),L(&b[8]))));}}
static unsigned vk_xfbits(const v4 *k,const GLfloat *x,const GLfloat *y,const GLfloat *z) {v4x4 c4 A16;vk_xf4(k,x,y,z,&c4);return vk_clipbits(&c4,&k[16]);}
static unsigned vk_snap2u(GLuint *o,const v4 *x,const v4 *y,const v4 *k) {return vk_snap2(o,x,y,k,1);}
#undef A16
#undef L
#undef S
#define VK_SSE 1
#endif

/* ---- the stage ------------------------------------------------------------------------------------------------------------------- */
#define SP_LIGHTS MAX_LIGHTS
/* a light's splats: p the position (positional) or the direction's unit vector; h (0 0 1) or _h_inf_norm. 304 bytes: the vectors stay aligned */
typedef struct {v4x3 p,amb,dif,spc,h,la,ld,nd;v4 att,c,l,q;} sp_light; /* v10.488: nd a spotlight's unit direction */ /* la, ld: the light's ambient and diffuse (colour material: times the vertex colour) */
typedef struct {int positional,attconst,spec,spot;GLfloat cosc;const GLfloat (*sexp)[2];const GLfloat *spc_s;} sp_light_info; /* v10.488: spot, its cutoff's cosine and exponent table */
/* The store is allocated 16-byte aligned; every member up to tmpc is a multiple of 16 bytes, so each vector is aligned (the kernels use
 * movaps). Scalars and pointers come after. */
typedef struct {
 /* per-draw constants (splats) */
 v4 mvp[16],sign,one,vp[6],kvs,wxy[2]; /* v10.495: in this order (vk_front reads them from mvp: sign +256, one +272, vp +288, the store's stride +384); v10.497: wxy the group's window x and y (+400, +416: vk_front writes them for vk_snap2) */
 v4 mv[16],nm[9],krsq[2],kbias[2],sumA,kfog[5]; /* v10.489: kfog the abs mask, end, 1 / (end - start), 0, 1 */
 v4 bx[9]; /* v10.497: vk_box3's accumulators (min max of x y z, the flag) and constants (the abs mask, 1e30) */
 v4 kl[3],kq[4],lnd[2]; /* v10.495: kl 0.5 3 and the normals' length (vk_xfn_norm); kq 0.5 3, d2, d (vk_lpos); lnd the half vector's length and n.h (vk_lhalf) */
 v4 kalpha; /* .503: the constant alpha a lane (vk_pack) */
 v4 krb[3][2]; /* .505: range_bad's bounds for vk_range: [0] 1e-20 .. 1e30, [1] 1e-30 .. 1e30, [2] 1e-12 .. 1e30 */
 v4 kub[5],ksh[3],ksn[9]; /* v10.490: vk_ub's, vk_shine_a's and vk_snapd's constants */
 v4 kthr; /* v10.492: 0.001 (the attenuation's threshold) */
 v4x3 lma,em,ma,md; /* the light model's ambient; the material's emission, ambient and diffuse (colour material: the ones not tracking the colour) */
 sp_light lt[SP_LIGHTS];
 /* per-group temporaries */
 v4x4 c4,p4;v4x3 e3,n3,vp3,h3,v3,s3,cc3,t3,col3,lamb3[SP_LIGHTS],ldif3[SP_LIGHTS];v4 len,d2,invd,d,att,den,ndv,ndh,keep,fmask,amask,bias[4],pvd,fogf,fogb;
 v4 sh[5],spec,snx,sny,sxo,syo; /* v10.490: the specular's f, floor(f), its integer, the table's two entries; the result; window x y, snapped */
 GLfloat tmpc[16]; /* a last group of fewer than four: its clip coordinates */
 /* scalars */
 sp_light_info li[SP_LIGHTS];
 int nlights,local,lit,fog,fogmode,normmode;GLfloat fogd,fogend;const GLfloat *exptab;
 int colmat; /* colour material with the list's colours differing: bit 1 the material's ambient tracks the colour, 2 its diffuse, 4 its emission (Mesa: a material update a vertex) */
 int tex_sets,tex_src[2];GLuint tex_off[2],col_off,fog_off,vs;GLubyte const_col[4];const GLfloat *const_tex[2];
 const vx_soa *so;
 vx_soa imm;GLubyte *immbuf; /* v10.488: a draw not from a list, its vertices in SoA form (imm_build) */
 GLuint *snapbuf;int snapmode,snap2; /* v10.497: snap2 the grid words made in sse_group (ACCEL_OPENGL_SSESNAP2) */
 /* v10.492: the per-draw set-up kept (ACCEL_OPENGL_SSECACHE): the matrices as last splatted, the lighting's inputs as last set up (lk_n
  * floats), the bits its light loop added; kinit the constants made */
 int inside; /* v10.495: the draw's box inside every plane (sp_prepare, ACCEL_OPENGL_SSEACCEPT) */
 int kinit,lk_valid;GLuint lk_n,lk_why;GLfloat mvp_c[16],mv_c[16],nm_c[9],vp_c[6],lk[16+48+8+SP_LIGHTS*44]; /* v10.490: the strip path's x y words a vertex (vx_d3d_gl_presnap), its rule (1, 2 y up; 0 none this draw) */
 /* stores for the render stage */
 GLvector4f clip;GLubyte *clipmask;GLubyte tmpv[4*64];GLubyte tmpm[4];GLubyte *need;GLuint need_n; /* .503: need, SSEDEFER's vertices of drawn triangles */
} sp_store;
/* v10.495: vk_front's offsets from mvp */
typedef char sp_chk_sign[offsetof(sp_store,sign)-offsetof(sp_store,mvp)==256?1:-1];typedef char sp_chk_one[offsetof(sp_store,one)-offsetof(sp_store,mvp)==272?1:-1];
typedef char sp_chk_vp[offsetof(sp_store,vp)-offsetof(sp_store,mvp)==288?1:-1];typedef char sp_chk_kvs[offsetof(sp_store,kvs)-offsetof(sp_store,mvp)==384?1:-1];typedef char sp_chk_wxy[offsetof(sp_store,wxy)-offsetof(sp_store,mvp)==400?1:-1];

static void splat(v4 *d,GLfloat f) {d->f[0]=d->f[1]=d->f[2]=d->f[3]=f;}
static void splat3(v4x3 *d,const GLfloat *f) {splat(&d->x,f[0]);splat(&d->y,f[1]);splat(&d->z,f[2]);}
static GLint fbits(GLfloat f) {union {GLfloat f;GLint i;} u;u.f=f;return u.i;}
static GLubyte to_ub(GLfloat f) {GLubyte b;UNCLAMPED_FLOAT_TO_UBYTE(b,f);return b;}
/* UNCLAMPED_FLOAT_TO_UBYTE from the float's own bits (orig) and the biased value's bits (vk_bias) */
static GLubyte ub_from(GLuint orig,GLuint biased) {if((GLint)orig<0)return 0;if(orig>=0x3F7F0000UL)return 255;return (GLubyte)biased;}

/* ---- the scalar reference: Mesa's arithmetic for one vertex (a group with an edge lane; the host tests) ------------------------------ */
/* colour material with the colours differing: the derived values Mesa's update_materials / _mesa_update_material would make for this vertex's colour
 * (base = emission + ambient * the model's ambient; a light's ambient and diffuse times the material's), the untracked ones from the state */
static void ref_material(GLcontext *ctx,const sp_store *st,const GLfloat *col,GLfloat *base,GLfloat (*amb)[3],GLfloat (*dif)[3]) {
 const GLfloat *A=(st->colmat&1)?col:ctx->Light.Material.Attrib[MAT_ATTRIB_FRONT_AMBIENT],*D=(st->colmat&2)?col:ctx->Light.Material.Attrib[MAT_ATTRIB_FRONT_DIFFUSE],
  *E=(st->colmat&4)?col:ctx->Light.Material.Attrib[MAT_ATTRIB_FRONT_EMISSION];struct gl_light *light;int n=0;
 COPY_3V(base,E);ACC_SCALE_3V(base,A,ctx->Light.Model.Ambient);
 foreach(light,&ctx->Light.EnabledList){if(n>=SP_LIGHTS)break;SCALE_3V(amb[n],light->Ambient,A);SCALE_3V(dif[n],light->Diffuse,D);n++;}
}
static void ref_light(GLcontext *ctx,const sp_store *st,const GLfloat *vertex,const GLfloat *normal,const GLfloat *col,GLfloat *out) {
 const GLfloat *base=ctx->Light._BaseColor[0];GLfloat s0,s1,s2,mbase[3],mamb[SP_LIGHTS][3],mdif[SP_LIGHTS][3];const GLint thr=fbits(1e-3F);struct gl_light *light;int n=0;
 if(st->colmat){ref_material(ctx,st,col,mbase,mamb,mdif);base=mbase;}
 s0=base[0];s1=base[1];s2=base[2];
 foreach(light,&ctx->Light.EnabledList) {
  GLfloat VP[3],attenuation,n_dot_VP,n_dot_h,c0,c1,c2;const GLfloat *amb,*dif,*h;
  if(!(light->_Flags&LIGHT_POSITIONAL)){COPY_3V(VP,light->_VP_inf_norm);attenuation=light->_VP_inf_spot_attenuation;}
  else {GLfloat d;SUB_3V(VP,light->_Position,vertex);
   if(_mesa_vx855_rsqrt){GLfloat d2=(GLfloat)LEN_SQUARED_3FV(VP);if(d2>1e-12F){GLfloat invd=vx_inv_sqrtf(d2);d=d2*invd;SELF_SCALE_SCALAR_3V(VP,invd);}else d=(GLfloat)SQRTF(d2);}
   else {d=(GLfloat)LEN_3FV(VP);if(d>1e-6){GLfloat invd=1.0F/d;SELF_SCALE_SCALAR_3V(VP,invd);}}
   if(light->LinearAttenuation==0.0F&&light->QuadraticAttenuation==0.0F)attenuation=light->ConstantAttenuation==1.0F?1.0F:1.0F/light->ConstantAttenuation;
   else attenuation=vx_recipf(light->ConstantAttenuation+d*(light->LinearAttenuation+d*light->QuadraticAttenuation));
   if(light->_Flags&LIGHT_SPOT){GLfloat PV_dot_dir=-DOT3(VP,light->_NormDirection); /* v10.488: as light_vx855 */
    if(PV_dot_dir<light->_CosCutoff){n++;continue;}
    else{GLdouble x=PV_dot_dir*(EXP_TABLE_SIZE-1);GLint k=vx_itruncd(x);GLfloat spot=(GLfloat)(light->_SpotExpTable[k][0]+(x-k)*light->_SpotExpTable[k][1]);attenuation*=spot;}}}
  n++;
  if(fbits(attenuation)<thr)continue;
  amb=st->colmat?mamb[n-1]:light->_MatAmbient[0];n_dot_VP=DOT3(normal,VP);
  if((GLuint)fbits(n_dot_VP)>0x80000000UL){s0+=attenuation*amb[0];s1+=attenuation*amb[1];s2+=attenuation*amb[2];continue;}
  dif=st->colmat?mdif[n-1]:light->_MatDiffuse[0];c0=amb[0];c1=amb[1];c2=amb[2];c0+=n_dot_VP*dif[0];c1+=n_dot_VP*dif[1];c2+=n_dot_VP*dif[2];
  if(st->local){GLfloat v[3],len;COPY_3V(v,vertex);len=(GLfloat)LEN_SQUARED_3FV(v);if(len){len=vx_inv_sqrtf(len);v[0]*=len;v[1]*=len;v[2]*=len;}
   SUB_3V(VP,VP,v);len=(GLfloat)LEN_SQUARED_3FV(VP);if(len){len=vx_inv_sqrtf(len);VP[0]*=len;VP[1]*=len;VP[2]*=len;}h=VP;}
  else if(light->_Flags&LIGHT_POSITIONAL){GLfloat len;ACC_3V(VP,ctx->_EyeZDir);len=(GLfloat)LEN_SQUARED_3FV(VP);if(len){len=vx_inv_sqrtf(len);VP[0]*=len;VP[1]*=len;VP[2]*=len;}h=VP;}
  else h=light->_h_inf_norm;
  n_dot_h=1.0F*DOT3(normal,h);
  if(fbits(n_dot_h)>0){GLfloat spec_coef;const GLfloat *spc=light->_MatSpecular[0];GET_SHINE_TAB_ENTRY(ctx->_ShineTable[0],n_dot_h,spec_coef);c0+=spec_coef*spc[0];c1+=spec_coef*spc[1];c2+=spec_coef*spc[2];}
  s0+=attenuation*c0;s1+=attenuation*c1;s2+=attenuation*c2;
 }
 out[0]=s0;out[1]=s1;out[2]=s2;
}
static GLfloat ref_fog(const sp_store *st,GLfloat z) {
 GLfloat d=st->fogd;
 if(st->fogmode==GL_LINEAR){GLfloat f=(st->fogend-z)*d;return CLAMP(f,0.0F,1.0F);}
 {GLfloat narg=st->fogmode==GL_EXP?d*z:d*z*z,f=(GLfloat)(narg*(1.0/(10.0/256)));GLint k=vx_itrunc(f);
  if(k>256-2)return (GLfloat).0006595;return st->exptab[k]+(f-k)*(st->exptab[k+1]-st->exptab[k]);}
}
/* vertex i of the list: clip coordinates to clip[4], its clip mask, and the engine's vertex at dst */
static GLubyte ref_vertex(GLcontext *ctx,const sp_store *st,GLuint i,GLfloat *clip,GLubyte *dst) {
 const vx_soa *so=st->so;const GLfloat *m=ctx->_ModelProjectMatrix.m,*e=ctx->ModelviewMatrixStack.Top->m,*vp=VXGL_CTX(ctx)->ViewportMatrix.m;
 GLfloat ox=so->x[i],oy=so->y[i],oz=so->z[i],eye[3],col[4],proj[4],*w=(GLfloat*)dst;GLubyte mask=0;int u;
 clip[0]=m[0]*ox+m[4]*oy+m[8]*oz+m[12];clip[1]=m[1]*ox+m[5]*oy+m[9]*oz+m[13];clip[2]=m[2]*ox+m[6]*oy+m[10]*oz+m[14];clip[3]=m[3]*ox+m[7]*oy+m[11]*oz+m[15];
 if(clip[3]<clip[0])mask|=CLIP_RIGHT_BIT;if(clip[3]<-clip[0])mask|=CLIP_LEFT_BIT;if(clip[3]<clip[1])mask|=CLIP_TOP_BIT;if(clip[3]<-clip[1])mask|=CLIP_BOTTOM_BIT;
 if(clip[3]<clip[2])mask|=CLIP_FAR_BIT;if(clip[3]<-clip[2])mask|=CLIP_NEAR_BIT;
 if(mask){proj[0]=proj[1]=proj[2]=0.0F;proj[3]=1.0F;}else{GLfloat oow=1.0F/clip[3];proj[0]=clip[0]*oow;proj[1]=clip[1]*oow;proj[2]=clip[2]*oow;proj[3]=oow;}
 w[0]=vp[0]*proj[0]+vp[12];w[1]=vp[5]*proj[1]+vp[13];w[2]=vp[10]*proj[2]+vp[14];w[3]=proj[3];
 eye[0]=e[0]*ox+e[4]*oy+e[8]*oz+e[12];eye[1]=e[1]*ox+e[5]*oy+e[9]*oz+e[13];eye[2]=e[2]*ox+e[6]*oy+e[10]*oz+e[14];
 {const GLfloat *inv=ctx->ModelviewMatrixStack.Top->inv;GLfloat ux=so->nx[i],uy=so->ny[i],uz=so->nz[i],n[3];
  if(st->normmode==2){GLfloat sc=ctx->_ModelViewInvScale;n[0]=ux*(sc*inv[0])+uy*(sc*inv[1])+uz*(sc*inv[2]);n[1]=ux*(sc*inv[4])+uy*(sc*inv[5])+uz*(sc*inv[6]);n[2]=ux*(sc*inv[8])+uy*(sc*inv[9])+uz*(sc*inv[10]);}
  else {n[0]=ux*inv[0]+uy*inv[1]+uz*inv[2];n[1]=ux*inv[4]+uy*inv[5]+uz*inv[6];n[2]=ux*inv[8]+uy*inv[9]+uz*inv[10];
   if(st->normmode==1){GLdouble len=n[0]*n[0]+n[1]*n[1]+n[2]*n[2];if(len>1e-20){GLfloat sc=1.0F/SQRTF(len);n[0]*=sc;n[1]*=sc;n[2]*=sc;}else n[0]=n[1]=n[2]=0;}}
  {GLfloat vc[4];vc[0]=vc[1]=vc[2]=0.0F;vc[3]=1.0F;if(so->have&2){vc[0]=so->r[i];vc[1]=so->g[i];vc[2]=so->b[i];vc[3]=so->a[i];}
  ref_light(ctx,st,eye,n,vc,col);col[3]=(st->colmat&2)?vc[3]:st->sumA.f[0];}}
 dst[st->col_off+2]=to_ub(col[0]);dst[st->col_off+1]=to_ub(col[1]);dst[st->col_off]=to_ub(col[2]);dst[st->col_off+3]=to_ub(col[3]); /* .506: lit only */
 if(st->fog)dst[st->fog_off]=to_ub(ref_fog(st,FABSF(eye[2])));
 for(u=0;u<st->tex_sets;u++){GLfloat *t=(GLfloat*)(dst+st->tex_off[u]);if(st->tex_src[u]>=0){t[0]=so->s[st->tex_src[u]][i];t[1]=so->t[st->tex_src[u]][i];}else{t[0]=st->const_tex[u][0];t[1]=st->const_tex[u][1];}}
 return mask;
}
static void ref_group(GLcontext *ctx,sp_store *st,GLuint i0,GLuint n,GLubyte *verts) {
 GLuint i;GLfloat (*clip)[4]=(GLfloat(*)[4])st->clip.start;
 for(i=i0;i<i0+n;i++)st->clipmask[i]=ref_vertex(ctx,st,i,clip[i],verts+i*st->vs);
 vxgl_ssepass_stat[2]++;
}

#ifdef VK_SSE
static const GLubyte bits_hi[8]={0,CLIP_RIGHT_BIT,CLIP_TOP_BIT,CLIP_RIGHT_BIT|CLIP_TOP_BIT,CLIP_FAR_BIT,CLIP_RIGHT_BIT|CLIP_FAR_BIT,CLIP_TOP_BIT|CLIP_FAR_BIT,CLIP_RIGHT_BIT|CLIP_TOP_BIT|CLIP_FAR_BIT};
static const GLubyte bits_lo[8]={0,CLIP_LEFT_BIT,CLIP_BOTTOM_BIT,CLIP_LEFT_BIT|CLIP_BOTTOM_BIT,CLIP_NEAR_BIT,CLIP_LEFT_BIT|CLIP_NEAR_BIT,CLIP_BOTTOM_BIT|CLIP_NEAR_BIT,CLIP_LEFT_BIT|CLIP_BOTTOM_BIT|CLIP_NEAR_BIT};
/* v10.493 (board, Lattice v10.492: the masks' bit-by-bit assembly a sixth of sse_group): plane p's compare bits for the four lanes (vk_clipbits'
 * nibble p) to the four lanes' mask bytes in one word, lane v in byte v; the six ORed: the group's masks, stored as one word */
static GLuint clip_tab[6][16];static int clip_tab_ok;
static void clip_tab_make(void) {
 static const GLubyte pb[6]={CLIP_RIGHT_BIT,CLIP_TOP_BIT,CLIP_FAR_BIT,CLIP_LEFT_BIT,CLIP_BOTTOM_BIT,CLIP_NEAR_BIT};GLuint p,nb,v;
 for(p=0;p<6;p++)for(nb=0;nb<16;nb++){GLuint w=0;for(v=0;v<4;v++)if((nb>>v)&1)w|=(GLuint)pb[p]<<(8*v);clip_tab[p][nb]=w;}
 clip_tab_ok=1;
}
static void lane_mask(v4 *m,GLuint lanes) {GLuint *u=(GLuint*)m->f;u[0]=lanes&1?0xFFFFFFFFUL:0;u[1]=lanes&2?0xFFFFFFFFUL:0;u[2]=lanes&4?0xFFFFFFFFUL:0;u[3]=lanes&8?0xFFFFFFFFUL:0;}
/* a lane's value is outside the SSE steps' range: the reference does the group */
static int range_bad(const v4 *v,GLuint n,GLfloat lo,GLfloat hi) {GLuint i;for(i=0;i<n;i++){GLfloat f=v->f[i];if(!(f>lo&&f<hi))return 1;}return 0;}
/* .505 (ACCEL_OPENGL_SSERANGE): range_bad four lanes at once (the same compares: a lane bad unless lo < f < hi, a NaN bad) */
int vxgl_ssepass_range=1;
#define RB(v,n,K,lo,hi) (vxgl_ssepass_range?((~vk_range((v),st->krb[K]))&((1U<<(n))-1U))!=0:range_bad((v),(n),(lo),(hi)))

/* four vertices from lane i0: the clip coordinates and masks into the stores, the engine's vertices at verts (4 * vs bytes); returns the
 * or of the four masks, or -1 when a lane needs the reference (n lanes are real vertices) */
extern GLuint vx_d3d_snapw(GLuint w,int up);
/* v10.497: the strip path's grid words for the group's n lanes from vk_front's window x y (vk_snap2d / vk_snap2u: snap_group's kernels, the
 * same words); a lane out of the kernels' range by snapw itself */
static void snap2_group(sp_store *st,GLuint i0,GLuint n) {
 GLuint *o=st->snapbuf+2*i0,v;unsigned ok=st->snapmode==2?vk_snap2u(o,&st->wxy[0],&st->wxy[1],st->ksn):vk_snap2d(o,&st->wxy[0],&st->wxy[1],st->ksn);
 if((ok&0xFFU)!=0xFFU)for(v=0;v<n;v++){if(!((ok>>v)&1))o[2*v]=vx_d3d_snapw(LANE_U(st->wxy[0],v),0);if(!((ok>>(4+v))&1))o[2*v+1]=vx_d3d_snapw(LANE_U(st->wxy[1],v),st->snapmode==2);}
}
/* .503: part 1 the positions (window vertices, clip coordinates, masks, grid words), 2 the rest (lighting, colours, fog, texture
 * coordinates), 3 both (SSEDEFER: the parts apart, the lighting only for groups a drawn triangle uses) */
static int sse_group(GLcontext *ctx,sp_store *st,GLuint i0,GLuint n,GLubyte *verts,GLfloat *clipout,GLubyte *maskout,int part) {
 const vx_soa *so=st->so;unsigned bits,keep=0;GLuint v,ormask=0;int edge=0,u;const GLint thr=fbits(1e-3F);
 const GLuint vs=st->vs,co=st->col_off,fo=st->fog_off; /* v10.493: in locals (Watcom read them again after every byte stored) */
 const int fz=vxgl_ssepass_fuse&&vxgl_ssepass_mask; /* v10.495: the fused kernels (the lane compares by SSE, as SSEMASK's) */
 if(part&1){
 if(fz)vxgl_ssepass_stat[17]++;
 if(fz&&st->inside){vk_front_in(st->mvp,so->x+i0,so->y+i0,so->z+i0,verts,clipout);*(GLuint*)maskout=0;ormask=0;if(st->snap2)snap2_group(st,i0,n);} /* v10.495: no compare can be set */
 else {
 if(fz){bits=vk_front(st->mvp,so->x+i0,so->y+i0,so->z+i0,verts,clipout);if(st->snap2)snap2_group(st,i0,n);} /* v10.495: the window vertices and the clip coordinates stored too; v10.497: their grid words */
 else {vk_xf4(st->mvp,so->x+i0,so->y+i0,so->z+i0,&st->c4);
 bits=vk_clipbits(&st->c4,&st->sign);}
 if(vxgl_ssepass_tight){GLuint w=clip_tab[0][bits&15]|clip_tab[1][(bits>>4)&15]|clip_tab[2][(bits>>8)&15]|clip_tab[3][(bits>>12)&15]|clip_tab[4][(bits>>16)&15]|clip_tab[5][(bits>>20)&15],o; /* v10.493 */
  *(GLuint*)maskout=w;
  keep=((w&0xFFU)?0U:1U)|((w&0xFF00U)?0U:2U)|((w&0xFF0000UL)?0U:4U)|((w&0xFF000000UL)?0U:8U);
  o=n>=4?w:w&((1UL<<(8*n))-1UL);o|=o>>16;o|=o>>8;ormask=o&0xFFU;}
 else
 for(v=0;v<4;v++){unsigned hi=((bits>>v)&1)|((bits>>(3+v))&2)|((bits>>(6+v))&4),lo=((bits>>(12+v))&1)|((bits>>(15+v))&2)|((bits>>(18+v))&4);GLubyte m=(GLubyte)(bits_hi[hi]|bits_lo[lo]);
  maskout[v]=m;if(!m)keep|=1U<<v;if(v<n)ormask|=m;}
 if(!fz){
 lane_mask(&st->keep,keep);
 vk_project(&st->c4,&st->keep,&st->one,&st->p4);
 vk_viewport_store(&st->p4,st->vp,verts,vs);
 vk_tr_store(&st->c4,clipout);}}
 }
 if(!(part&2))return (int)ormask;
 vk_xf3(st->mv,so->x+i0,so->y+i0,so->z+i0,&st->e3); /* .506: every draw the pass takes is lit */
 {
  int li;
  if(fz&&st->normmode==1){vk_xfn_norm(st->nm,so->nx+i0,so->ny+i0,so->nz+i0,&st->n3,st->kl);if(RB(&st->kl[2],n,0,1e-20F,1e30F))edge=1;} /* v10.495 */
  else {
  vk_xfn(st->nm,so->nx+i0,so->ny+i0,so->nz+i0,&st->n3);
  if(st->normmode==1){vk_norm3(&st->n3,st->krsq,&st->len);if(RB(&st->len,n,0,1e-20F,1e30F))edge=1;}} /* Mesa: len > 1e-20 by 1/sqrt, else 0 */
  if(st->local){st->v3=st->e3;vk_norm3(&st->v3,st->krsq,&st->len);if(RB(&st->len,n,1,1e-30F,1e30F))edge=1;} /* NORMALIZE_3FV: len 0 leaves the vector; the rsqrt range */
  if(st->colmat){ /* this group's colours: base = emission + ambient (model ambient); each light's ambient and diffuse times the material's (as _mesa_update_material) */
   const v4x3 *A,*E;memcpy(&st->col3.x,so->r+i0,16);memcpy(&st->col3.y,so->g+i0,16);memcpy(&st->col3.z,so->b+i0,16);
   A=(st->colmat&1)?&st->col3:&st->ma;E=(st->colmat&4)?&st->col3:&st->em;
   vk_mul3(&st->t3,A,&st->lma);vk_add3(&st->s3,E,&st->t3);
   for(li=0;li<st->nlights;li++){if(st->colmat&1)vk_mul3(&st->lamb3[li],&st->lt[li].la,&st->col3);if(st->colmat&2)vk_mul3(&st->ldif3[li],&st->lt[li].ld,&st->col3);}}
  else splat3(&st->s3,ctx->Light._BaseColor[0]);
  for(li=0;li<st->nlights;li++) {
   sp_light *L=&st->lt[li];const sp_light_info *I=&st->li[li];GLuint am=0,fm=0;
   const v4x3 *amb=(st->colmat&1)?&st->lamb3[li]:&L->amb,*dif=(st->colmat&2)?&st->ldif3[li]:&L->dif;
   if(fz){ /* v10.495: the light by the fused kernels (vk_lpos, vk_ldiff, vk_lhalf_*, vk_lacc / vk_laccs): the same steps as below */
    const v4x3 *vp;const v4 *att,*ndh;int sp=0;GLuint r;
    if(!I->positional){vp=&L->p;att=&L->att;}
    else {vk_lpos(&st->vp3,&L->p,&st->e3,st->kq);vp=&st->vp3;
     if(RB(&st->kq[2],n,2,1e-12F,1e30F))edge=1;
     if(I->attconst)att=&L->att;
     else {vk_mad(&st->den,&st->kq[3],&L->q,&L->l);vk_mad(&st->den,&st->kq[3],&st->den,&L->c);
      for(v=0;v<n;v++){GLfloat f=st->den.f[v];if(!((f>1e-30F&&f<1e30F)||(f<-1e-30F&&f>-1e30F)))edge=1;}
      vk_rcp_nr(&st->att,&st->den);att=&st->att;}}
    if(I->spot){GLuint sm=0;if(att!=&st->att){st->att=*att;att=&st->att;}
     vk_dot3(vp,&L->nd,&st->pvd);
     for(v=0;v<4;v++){GLfloat pv=-st->pvd.f[v];if(pv<I->cosc)continue;sm|=1U<<v;
      {GLdouble x=pv*(EXP_TABLE_SIZE-1);GLint k=vx_itruncd(x);GLfloat spot=(GLfloat)(I->sexp[k][0]+(x-k)*I->sexp[k][1]);st->att.f[v]*=spot;}}
     am=vk_gemask(&st->amask,att,&st->kthr)&sm;if(vk_unord(att)&((1U<<n)-1U))edge=1;
     lane_mask(&st->amask,am);}
    else {am=vk_gemask(&st->amask,att,&st->kthr);if(vk_unord(att)&((1U<<n)-1U))edge=1;}
    r=vk_ldiff(&st->cc3,&st->n3,vp,dif,amb);fm=r&15U;if((r>>4)&((1U<<n)-1U))edge=1;
    if(I->spec&&(fm&am)) {
     if(st->local){vk_lhalf_sub(st->lnd,vp,&st->v3,&st->n3,st->krsq);if(RB(&st->lnd[0],n,1,1e-30F,1e30F))edge=1;}
     else if(I->positional){vk_lhalf_add(st->lnd,vp,&L->h,&st->n3,st->krsq);if(RB(&st->lnd[0],n,1,1e-30F,1e30F))edge=1;}
     else vk_dot3(&st->n3,&L->h,&st->lnd[1]);
     ndh=&st->lnd[1];
    if(vxgl_ssepass_shine){ /* v10.490 (board, Lattice v10.489: the lookup a lane at a time on the x87, a sixth of sse_group's samples): the index and the
     * interpolation four lanes at a time (vk_shine_a, vk_shine_b), the two entries a lane gathered; f >= 255 (k > 254) _mesa_pow as Mesa; a NaN: the reference */
     GLuint em=0,pw=0;const struct gl_shine_tab *tab=ctx->_ShineTable[0];
     for(v=0;v<4;v++){GLuint b=LANE_U(*ndh,v);if(((fm>>v)&1)&&(GLint)b>0){if(b>0x7F800000UL){if(v<n)edge=1;}else em|=1U<<v;}}
     if(em){
      vk_shine_a(st->sh,ndh,st->ksh);
      for(v=0;v<4;v++){GLuint *t0=(GLuint*)st->sh[3].f,*t1=(GLuint*)st->sh[4].f;
       if(((em>>v)&1)&&LANE_U(st->sh[0],v)<0x437F0000UL){GLuint k=LANE_U(st->sh[2],v)-0x4B400000UL;t0[v]=((const GLuint*)tab->tab)[k];t1[v]=((const GLuint*)tab->tab)[k+1];}
       else{t0[v]=t1[v]=0;if((em>>v)&1)pw|=1U<<v;}}
      vk_shine_b(&st->spec,st->sh);
      for(v=0;v<4;v++){if((pw>>v)&1){st->spec.f[v]=(GLfloat)_mesa_pow(ndh->f[v],tab->shininess);if(v<n)vxgl_ssepass_stat[14]++;}
       else if(!((em>>v)&1))((GLuint*)st->spec.f)[v]=0;else if(v<n)vxgl_ssepass_stat[13]++;}
      sp=1;}} /* v10.495: c += spec_coef spc in vk_laccs */
    else
    for(v=0;v<n;v++)if((fm>>v)&1&&(GLint)LANE_U(*ndh,v)>0){GLfloat spec_coef,n_dot_h=ndh->f[v];GET_SHINE_TAB_ENTRY(ctx->_ShineTable[0],n_dot_h,spec_coef);
     st->cc3.x.f[v]+=spec_coef*I->spc_s[0];st->cc3.y.f[v]+=spec_coef*I->spc_s[1];st->cc3.z.f[v]+=spec_coef*I->spc_s[2];}
   }
    if(sp)vk_laccs(&st->s3,&st->cc3,att,&st->amask,&L->spc,&st->spec);else vk_lacc(&st->s3,&st->cc3,att,&st->amask);
    continue;}
   if(!I->positional){st->vp3=L->p;st->att=L->att;}
   else {
    vk_sub3(&st->vp3,&L->p,&st->e3);vk_dot3(&st->vp3,&st->vp3,&st->d2);
    if(RB(&st->d2,n,2,1e-12F,1e30F))edge=1; /* d2 <= 1e-12: Mesa's sqrt path; above 1e30: the x87 1/sqrt */
    vk_rsqrt_nr(&st->invd,&st->d2,st->krsq);vk_mul(&st->d,&st->d2,&st->invd);vk_scale3(&st->vp3,&st->invd);
    if(I->attconst)st->att=L->att;
    else {vk_mad(&st->den,&st->d,&L->q,&L->l);vk_mad(&st->den,&st->d,&st->den,&L->c); /* c + d (l + d q) */
     for(v=0;v<n;v++){GLfloat f=st->den.f[v];if(!((f>1e-30F&&f<1e30F)||(f<-1e-30F&&f>-1e30F)))edge=1;} /* vx_recipf's range */
     vk_rcp_nr(&st->att,&st->den);}
   }
   if(I->spot){GLuint sm=0; /* v10.488: a spotlight (as light_vx855): a lane outside the cone takes nothing from the light, else the attenuation times the
     * exponent table's value */
    vk_dot3(&st->vp3,&L->nd,&st->pvd);
    for(v=0;v<4;v++){GLfloat pv=-st->pvd.f[v];if(pv<I->cosc)continue;sm|=1U<<v;
     {GLdouble x=pv*(EXP_TABLE_SIZE-1);GLint k=vx_itruncd(x);GLfloat spot=(GLfloat)(I->sexp[k][0]+(x-k)*I->sexp[k][1]);st->att.f[v]*=spot;}}
    if(vxgl_ssepass_mask){am=vk_gemask(&st->amask,&st->att,&st->kthr)&sm;if(vk_unord(&st->att)&((1U<<n)-1U))edge=1;}
    else for(v=0;v<4;v++)if(((sm>>v)&1)&&(GLint)LANE_U(st->att,v)>=thr)am|=1U<<v;
    lane_mask(&st->amask,am);}
   else if(vxgl_ssepass_mask){am=vk_gemask(&st->amask,&st->att,&st->kthr);if(vk_unord(&st->att)&((1U<<n)-1U))edge=1;} /* v10.492: att >= 0.001 by SSE (as the bits'
     * signed compare but for a NaN: the reference) */
   else {
   for(v=0;v<4;v++)if((GLint)LANE_U(st->att,v)>=thr)am|=1U<<v; /* attenuation not under 0.001 on the bits (a signed compare, as light_vx855) */
   lane_mask(&st->amask,am);}
   vk_dot3(&st->n3,&st->vp3,&st->ndv);
   if(vxgl_ssepass_mask){fm=vk_gemask(&st->fmask,&st->ndv,&st->kub[2]);if(vk_unord(&st->ndv)&((1U<<n)-1U))edge=1;} /* v10.492: n.VP >= 0 (-0 too) */
   else {
   for(v=0;v<4;v++)if(LANE_U(st->ndv,v)<=0x80000000UL)fm|=1U<<v; /* n.VP not negative (-0 counts as 0) */
   lane_mask(&st->fmask,fm);}
   vk_madd3(&st->cc3,dif,&st->ndv,amb); /* c = amb + n.VP dif */
   vk_blend3(&st->cc3,&st->fmask,&st->cc3,amb); /* back-facing: amb alone */
   if(I->spec&&(fm&am)) {
    if(st->local){vk_sub3(&st->h3,&st->vp3,&st->v3);vk_norm3(&st->h3,st->krsq,&st->len);if(RB(&st->len,n,1,1e-30F,1e30F))edge=1;}
    else if(I->positional){vk_add3(&st->h3,&st->vp3,&L->h);vk_norm3(&st->h3,st->krsq,&st->len);if(RB(&st->len,n,1,1e-30F,1e30F))edge=1;} /* VP + (0 0 1), normalized */
    else st->h3=L->h;
    vk_dot3(&st->n3,&st->h3,&st->ndh);
    if(vxgl_ssepass_shine){ /* v10.490 (board, Lattice v10.489: the lookup a lane at a time on the x87, a sixth of sse_group's samples): the index and the
     * interpolation four lanes at a time (vk_shine_a, vk_shine_b), the two entries a lane gathered; f >= 255 (k > 254) _mesa_pow as Mesa; a NaN: the reference */
     GLuint em=0,pw=0;const struct gl_shine_tab *tab=ctx->_ShineTable[0];
     for(v=0;v<4;v++){GLuint b=LANE_U(st->ndh,v);if(((fm>>v)&1)&&(GLint)b>0){if(b>0x7F800000UL){if(v<n)edge=1;}else em|=1U<<v;}}
     if(em){
      vk_shine_a(st->sh,&st->ndh,st->ksh);
      for(v=0;v<4;v++){GLuint *t0=(GLuint*)st->sh[3].f,*t1=(GLuint*)st->sh[4].f;
       if(((em>>v)&1)&&LANE_U(st->sh[0],v)<0x437F0000UL){GLuint k=LANE_U(st->sh[2],v)-0x4B400000UL;t0[v]=((const GLuint*)tab->tab)[k];t1[v]=((const GLuint*)tab->tab)[k+1];}
       else{t0[v]=t1[v]=0;if((em>>v)&1)pw|=1U<<v;}}
      vk_shine_b(&st->spec,st->sh);
      for(v=0;v<4;v++){if((pw>>v)&1){st->spec.f[v]=(GLfloat)_mesa_pow(st->ndh.f[v],tab->shininess);if(v<n)vxgl_ssepass_stat[14]++;}
       else if(!((em>>v)&1))((GLuint*)st->spec.f)[v]=0;else if(v<n)vxgl_ssepass_stat[13]++;}
      vk_madd3(&st->cc3,&L->spc,&st->spec,&st->cc3);}} /* c += spec_coef spc (0 in the other lanes) */
    else
    for(v=0;v<n;v++)if((fm>>v)&1&&(GLint)LANE_U(st->ndh,v)>0){GLfloat spec_coef,n_dot_h=st->ndh.f[v];GET_SHINE_TAB_ENTRY(ctx->_ShineTable[0],n_dot_h,spec_coef);
     st->cc3.x.f[v]+=spec_coef*I->spc_s[0];st->cc3.y.f[v]+=spec_coef*I->spc_s[1];st->cc3.z.f[v]+=spec_coef*I->spc_s[2];}
   }
   vk_madd3(&st->t3,&st->cc3,&st->att,&st->s3);  /* s + att c */
   vk_blend3(&st->s3,&st->amask,&st->t3,&st->s3);  /* lanes under 0.001: unchanged */
  }
  if(vxgl_ssepass_pack&&!(vk_unord(&st->s3.x)|vk_unord(&st->s3.y)|vk_unord(&st->s3.z)|((st->colmat&2)?vk_unord((const v4*)(so->a+i0)):0))){ /* v10.490: no branch a channel */
   const GLuint *b0=(const GLuint*)st->bias[0].f,*b1=(const GLuint*)st->bias[1].f,*b2=(const GLuint*)st->bias[2].f,*b3=(const GLuint*)st->bias[3].f;
   vk_ub(&st->bias[0],&st->s3.x,st->kub);vk_ub(&st->bias[1],&st->s3.y,st->kub);vk_ub(&st->bias[2],&st->s3.z,st->kub);
   if(st->colmat&2){GLubyte *d=verts+co;vk_ub(&st->bias[3],(const v4*)(so->a+i0),st->kub);if(vxgl_ssepass_sse2)vk_pack(d,st->bias,&st->bias[3],vs);else for(v=0;v<4;v++,d+=vs)*(GLuint*)d=(b2[v]&255U)|(b1[v]&255U)<<8|(b0[v]&255U)<<16|(b3[v]&255U)<<24;}
   else {GLuint a=(GLuint)st->const_col[3]<<24;GLubyte *d=verts+co;if(vxgl_ssepass_sse2){GLuint *k=(GLuint*)st->kalpha.f;k[0]=k[1]=k[2]=k[3]=st->const_col[3];vk_pack(d,st->bias,&st->kalpha,vs);}else for(v=0;v<4;v++,d+=vs)*(GLuint*)d=(b2[v]&255U)|(b1[v]&255U)<<8|(b0[v]&255U)<<16|a;}}
  else {
  vk_bias(&st->bias[0],&st->s3.x,st->kbias);vk_bias(&st->bias[1],&st->s3.y,st->kbias);vk_bias(&st->bias[2],&st->s3.z,st->kbias);
  if(st->colmat&2)vk_bias(&st->bias[3],(const v4*)(so->a+i0),st->kbias); /* the diffuse alpha tracks the colour's */
  for(v=0;v<4;v++){GLubyte *d=verts+v*vs+co;d[2]=ub_from(LANE_U(st->s3.x,v),LANE_U(st->bias[0],v));d[1]=ub_from(LANE_U(st->s3.y,v),LANE_U(st->bias[1],v));d[0]=ub_from(LANE_U(st->s3.z,v),LANE_U(st->bias[2],v));
   d[3]=(st->colmat&2)?ub_from(LANE_U(*(const v4*)(so->a+i0),v),LANE_U(st->bias[3],v)):st->const_col[3];}}
 } /* .506: the unlit colour branches removed with ACCEL_OPENGL_SSEUNLIT (the pass takes lit draws only) */
 if(st->fog&&st->fogmode==GL_LINEAR){ /* v10.489: four lanes at once (board: the scalar per-lane fog the pass's hottest code in Lattice) */
  vk_linfog(&st->fogf,&st->e3.z,st->kfog);
  if(vxgl_ssepass_pack){const GLuint *fb=(const GLuint*)st->fogb.f;GLubyte *d=verts+fo;vk_ub(&st->fogb,&st->fogf,st->kub);d[0]=(GLubyte)fb[0];d[vs]=(GLubyte)fb[1];d[2*vs]=(GLubyte)fb[2];d[3*vs]=(GLubyte)fb[3];} /* v10.490 (vk_linfog: never a NaN) */
  else {vk_bias(&st->fogb,&st->fogf,st->kbias);
  for(v=0;v<4;v++)verts[v*vs+fo]=ub_from(LANE_U(st->fogf,v),LANE_U(st->fogb,v));}}
 else if(st->fog)for(v=0;v<4;v++)verts[v*vs+fo]=to_ub(ref_fog(st,FABSF(st->e3.z.f[v])));
 for(u=0;u<st->tex_sets;u++){int src=st->tex_src[u];
  GLubyte *d=verts+st->tex_off[u];
  if(vxgl_ssepass_tight){ /* v10.493: the words copied (the x87's load and store, as before, only quietened a signalling NaN; the strip path takes a coordinate that is
   * not finite as 0 either way) */
   if(src>=0){const GLuint *s=(const GLuint*)so->s[src]+i0,*t=(const GLuint*)so->t[src]+i0;
    ((GLuint*)d)[0]=s[0];((GLuint*)d)[1]=t[0];d+=vs;((GLuint*)d)[0]=s[1];((GLuint*)d)[1]=t[1];d+=vs;((GLuint*)d)[0]=s[2];((GLuint*)d)[1]=t[2];d+=vs;((GLuint*)d)[0]=s[3];((GLuint*)d)[1]=t[3];}
   else {GLuint a,b;memcpy(&a,&st->const_tex[u][0],4);memcpy(&b,&st->const_tex[u][1],4);for(v=0;v<4;v++,d+=vs){((GLuint*)d)[0]=a;((GLuint*)d)[1]=b;}}}
  else if(src>=0){const GLfloat *s=so->s[src]+i0,*t=so->t[src]+i0;for(v=0;v<4;v++,d+=vs){((GLfloat*)d)[0]=s[v];((GLfloat*)d)[1]=t[v];}}
  else for(v=0;v<4;v++,d+=vs){((GLfloat*)d)[0]=st->const_tex[u][0];((GLfloat*)d)[1]=st->const_tex[u][1];}}
 return edge?-1:(int)ormask;
}
#endif

/* the pass does not take this draw: the reason bits (vxgl_ssepass_stat[6]) */
#define SP_NO_LIST 1U
#define SP_FALLBACK 2U
#define SP_PROGRAM 4U
#define SP_SPLIT 8U
#define SP_CLIPPLANES 0x10U
#define SP_LIGHTKIND 0x20U
#define SP_SPOT 0x40U
#define SP_MATERIAL 0x80U
#define SP_FOGKIND 0x100U
#define SP_LAYOUT 0x200U
#define SP_TEXSTATE 0x400U
#define SP_POINTS 0x800U
#define SP_NORMALS 0x1000U
#define SP_NOSOA 0x2000U
#define SP_UNLIT 0x4000U  /* v10.488: unlit (the pass takes lit draws only) */
#define SP_IMM 0x8000U
#define SP_OUTSIDE 0x80000000U /* v10.489: every vertex outside one plane: nothing drawn (not a reason the pass declines) */    /* v10.488: a draw not from a list the pass cannot take (indexed, positions of 4, ACCEL_OPENGL_SSEIMM 0) */

/* .503 (ACCEL_OPENGL_SSEDEFER): whether a lit draw's lighting may wait for its triangles: culling one face, both faces filled, smooth
 * shading (the engine's flat strips take colours from another vertex), rendering, no fallback or smoothing, and only triangles, strips
 * and fans: the cull as the core's (vxgl_state.c: rs[22] 2 culls area > 0, 3 area < 0); 0 when the draw does not qualify */
static GLuint defer_cull(GLcontext *ctx) {
 vxgl_ctx *c=VXGL_CTX(ctx);TNLcontext *tnl=TNL_CONTEXT(ctx);struct vertex_buffer *VB=&tnl->vb;GLuint p;int front_neg;
 if(!ctx->Polygon.CullFlag||ctx->Polygon.CullFaceMode==GL_FRONT_AND_BACK||ctx->Polygon.FrontMode!=GL_FILL||ctx->Polygon.BackMode!=GL_FILL||
  ctx->Light.ShadeModel!=GL_SMOOTH||ctx->RenderMode!=GL_RENDER||c->fallback||c->aa_tri||c->cull_all||!VB->Primitive||!VB->PrimitiveCount)return 0;
 for(p=0;p<VB->PrimitiveCount;p++){GLuint m=VB->Primitive[p].mode&PRIM_MODE_MASK;if(m!=GL_TRIANGLES&&m!=GL_TRIANGLE_STRIP&&m!=GL_TRIANGLE_FAN)return 0;}
 front_neg=(ctx->Polygon.FrontFace==GL_CCW)==(vxgl_flip(c)!=0);
 return ctx->Polygon.CullFaceMode==GL_BACK?(front_neg?2U:3U):(front_neg?3U:2U);
}
/* need[v] set for every vertex of a triangle that may be drawn: one with a vertex outside a clip plane (the clipper's), no area, or not
 * culled by the core's rule (its area from the store's window x y, in the core's vertex order and expression: (x1-x0)(y2-y0)-(y1-y0)(x2-x0)
 * in double, exact but for one rounding, so its sign is the core's) */
static void defer_mark(GLcontext *ctx,sp_store *st,const GLubyte *verts,GLubyte *need,GLuint n,GLuint cull) {
 TNLcontext *tnl=TNL_CONTEXT(ctx);struct vertex_buffer *VB=&tnl->vb;const GLuint *elt=VB->Elts;const GLuint vs=st->vs;GLuint p;
 memset(need,0,n);
 for(p=0;p<VB->PrimitiveCount;p++){const struct _mesa_prim *pr=&VB->Primitive[p];GLuint m=pr->mode&PRIM_MODE_MASK,j,a,b,d,e=pr->start+pr->count;
  for(j=pr->start+2;j<e;j+=(m==GL_TRIANGLES?3:1)){
   if(m==GL_TRIANGLES){a=j-2;b=j-1;d=j;}
   else if(m==GL_TRIANGLE_FAN){a=pr->start;b=j-1;d=j;}
   else if((j-pr->start)&1){a=j-1;b=j-2;d=j;} /* a strip's odd triangle: the first two swapped */
   else {a=j-2;b=j-1;d=j;}
   if(elt){a=elt[a];b=elt[b];d=elt[d];}
   if(a>=n||b>=n||d>=n){memset(need,1,n);return;}
   if(!(st->clipmask[a]|st->clipmask[b]|st->clipmask[d])){const GLfloat *v0=(const GLfloat*)(verts+a*vs),*v1=(const GLfloat*)(verts+b*vs),*v2=(const GLfloat*)(verts+d*vs);
    double area=((double)v1[0]-v0[0])*((double)v2[1]-v0[1])-((double)v1[1]-v0[1])*((double)v2[0]-v0[0]);
    if((cull==2&&area>0)||(cull==3&&area<0))continue;}
   need[a]=need[b]=need[d]=1;}}
}
static void nobuild(GLcontext *ctx,GLuint start,GLuint end,GLuint newinputs) {(void)ctx;(void)start;(void)end;(void)newinputs;}

extern GLuint vx_d3d_snapw(GLuint w,int up);
extern GLuint vx_d3d_ps_used,vx_d3d_cullrun_tris;
extern int vxgl_gl_snap; /* vxgl_tris.c: ACCEL_OPENGL_SNAP */
#ifdef VK_SSE
/* v10.490: the strip path's x y words for vertices i .. i + k - 1 (k <= 4) from the store as the group left it: snapw's grid four lanes at a
 * time (vk_snapd, vk_snapu for y with the window's flip); a lane of |x| >= 2040 (a NaN too) by snapw itself */
static void snap_group(sp_store *st,const GLubyte *verts,GLuint i,GLuint k) {
 GLuint v,okx,oky,*o=st->snapbuf+2*i,*xs=(GLuint*)st->snx.f,*ys=(GLuint*)st->sny.f;
 for(v=0;v<4;v++){if(v<k){const GLuint *w=(const GLuint*)(verts+v*st->vs);xs[v]=w[0];ys[v]=w[1];}else xs[v]=ys[v]=0;}
 okx=vk_snapok(&st->snx,st->ksn);oky=vk_snapok(&st->sny,st->ksn);
 vk_snapd(&st->sxo,&st->snx,st->ksn);if(st->snapmode==2)vk_snapu(&st->syo,&st->sny,st->ksn);else vk_snapd(&st->syo,&st->sny,st->ksn);
 for(v=0;v<k;v++){o[2*v]=(okx>>v)&1?LANE_U(st->sxo,v):vx_d3d_snapw(xs[v],0);o[2*v+1]=(oky>>v)&1?LANE_U(st->syo,v):vx_d3d_snapw(ys[v],st->snapmode==2);}
}
/* v10.500 (ACCEL_OPENGL_STORESNAP; host bench: snapw 4-5% of Quake 2's and Quake 3's frames, two a vertex, a vertex of an indexed list
 * snapped again for each of its triangles): the grid words of the store's first n vertices (x at +0, y at +4, stride vs) four at a time by
 * vk_snap2d / vk_snap2u (y up: the window's flip), a lane out of the kernels' range by snapw: the words fast_vertex_arrays would make
 * (snapw takes -0 as 0, as its caller's normalization; the kernels with snapw's fallback are checked against snapw by tests/sse_emu.py).
 * Returns the words (two a vertex) or 0 (no memory). */
/* (the words into buf, room for n + 4 vertices: also tests/sse_emu.py's entry, on the DLL's own code against its snapw) */
void vxgl_ssepass_snapstore_to(GLuint *buf,const GLubyte *verts,GLuint vs,GLuint n,int up) {
 static v4 kb[13];v4 *k=(v4*)(((unsigned long)kb+15)&~15UL),*x=k+9,*y=k+10;GLuint i,v,*xs=(GLuint*)x->f,*ys=(GLuint*)y->f;unsigned mx;
 if(!LANE_U(k[0],0)){GLuint *u=(GLuint*)k[0].f;u[0]=u[1]=u[2]=u[3]=0x7FFFFFFFUL;u=(GLuint*)k[7].f;u[0]=u[1]=u[2]=u[3]=0x80000000UL;
  splat(&k[1],2048.0F);splat(&k[2],12582912.0F);splat(&k[3],1.0F);splat(&k[4],0.5F);splat(&k[5],1.0F/128.0F);splat(&k[6],1.0F/16.0F);splat(&k[8],2040.0F);} /* as sp_run's ksn */
 mx=mx_enter();
 for(i=0;i<n;i+=4){GLuint m=n-i<4?n-i:4,*o=buf+2*i;unsigned ok;
  for(v=0;v<4;v++){if(v<m){const GLuint *w=(const GLuint*)(verts+(i+v)*vs);xs[v]=w[0];ys[v]=w[1];}else xs[v]=ys[v]=0;}
  ok=up?vk_snap2u(o,x,y,k):vk_snap2d(o,x,y,k);
  if((ok&0xFFU)!=0xFFU)for(v=0;v<m;v++){if(!((ok>>v)&1))o[2*v]=vx_d3d_snapw(xs[v],0);if(!((ok>>(4+v))&1))o[2*v+1]=vx_d3d_snapw(ys[v],up);}}
 mx_leave(mx);
}
GLuint *vxgl_ssepass_snapstore(const GLubyte *verts,GLuint vs,GLuint n,int up) {
 static GLuint *buf;static GLuint cap;
 if(n+4>cap){GLuint *b=(GLuint*)_mesa_align_malloc((n+68)*2*sizeof(GLuint),16);if(!b)return 0;if(buf)_mesa_align_free(buf);buf=b;cap=n+68;}
 vxgl_ssepass_snapstore_to(buf,verts,vs,n,up);return buf;
}
/* v10.490: the new kernels as the pass uses them, for the tests (tests/sse_emu.py runs the DLL's own code; the host check the gcc twins):
 * which 0 snapw's grid (snap_group's x), 1 the same up (y with the flip), 2 UNCLAMPED_FLOAT_TO_UBYTE (vk_ub; a group with a NaN by ub_from),
 * 3 the specular table's lookup (tab: the table); n words from in to out */
void vxgl_ssepass_ktest(GLuint which,const GLuint *in,GLuint *out,GLuint n,const GLfloat *tab) {
 static v4 buf[16];v4 *k=(v4*)(((unsigned long)buf+15)&~15UL),*x=k+10,*y=k+11,*b=k+12;GLuint i,v,*xs=(GLuint*)x->f;unsigned mx=mx_enter();
 if(which==4){ /* v10.495: each fused kernel against the kernels it replaces, n words of in as the floats (cycled), a trial per 64 words; out[0..8]
  * the mismatches: vk_front, vk_front_in, vk_xfn_norm, vk_lpos, vk_ldiff, vk_lacc, vk_laccs, vk_lhalf_sub, vk_lhalf_add; out[9] the trials */
  static v4 pool[220];v4 *P=(v4*)(((unsigned long)pool+15)&~15UL),*kb=P,*X=P+27,*Y=P+28,*Z=P+29,*R=P+30;GLuint t,j,w=0,nt=n/64?n/64:1,e[13];
  static GLubyte d1[4*28+16],d2[4*28+16];
  for(j=0;j<13;j++)e[j]=0;
  for(t=0;t<nt;t++){GLuint *u;
   /* 28 v4s of inputs: kb (25: mvp, sign, one, vp, stride), x y z; then R.. the other kernels' inputs from the same words */
   for(j=0;j<25*4;j++)((GLuint*)kb)[j]=in[(w+j)%n];
   u=(GLuint*)kb[16].f;u[0]=u[1]=u[2]=u[3]=0x80000000UL;splat(&kb[17],1.0F);u=(GLuint*)kb[24].f;u[0]=u[1]=u[2]=u[3]=28;
   for(j=0;j<12;j++)((GLuint*)X)[j]=in[(w+100+j)%n];
   for(j=0;j<40*4;j++)((GLuint*)R)[j]=in[(w+7*j+3)%n];
   w+=64;
   { /* vk_front, vk_front_in */
    v4x4 *c4=(v4x4*)(R+40),*p4=(v4x4*)(R+44);v4 *keep=R+48;GLfloat *c1=(GLfloat*)(R+50),*c2=(GLfloat*)(R+54);unsigned b1,b2,kp=0,q;
    memset(d1,0x5A,sizeof(d1));memset(d2,0x5A,sizeof(d2));
    b1=vk_front(kb,X->f,Y->f,Z->f,d1,c1);
    vk_xf4(kb,X->f,Y->f,Z->f,c4);b2=vk_clipbits(c4,&kb[16]);for(q=0;q<4;q++)if(!((b2>>q)&0x111111U))kp|=1U<<q;lane_mask(keep,kp);
    vk_project(c4,keep,&kb[17],p4);vk_viewport_store(p4,&kb[18],d2,28);vk_tr_store(c4,c2);
    {v4 *wx=R+88,*wy=R+89;vk_mad(wx,&p4->x,&kb[18],&kb[21]);vk_mad(wy,&p4->y,&kb[19],&kb[22]); /* v10.497: the window x y vk_front keeps */
    if(b1!=b2||memcmp(d1,d2,sizeof(d1))||memcmp(c1,c2,64)||memcmp(&kb[25],wx,16)||memcmp(&kb[26],wy,16))e[0]++;}
    if(vk_xfbits(kb,X->f,Y->f,Z->f)!=b2)e[9]++; /* v10.497 */
    { /* v10.497: vk_snap2d, vk_snap2u against vk_snapd / vk_snapu and vk_snapok on the same x y (kb[25], kb[26] as vk_front left them; and R[0], R[1]) */
     v4 *ks=R+90,*sx=R+99,*sy=R+100;GLuint o1[8],q,up,m1,m2;
     {GLuint *u=(GLuint*)ks[0].f;u[0]=u[1]=u[2]=u[3]=0x7FFFFFFFUL;u=(GLuint*)ks[7].f;u[0]=u[1]=u[2]=u[3]=0x80000000UL;}
     splat(&ks[1],2048.0F);splat(&ks[2],12582912.0F);splat(&ks[3],1.0F);splat(&ks[4],0.5F);splat(&ks[5],1.0F/128.0F);splat(&ks[6],1.0F/16.0F);splat(&ks[8],2040.0F);
     for(up=0;up<2;up++){int src;for(src=0;src<2;src++){const v4 *ix=src?&R[0]:&kb[25],*iy=src?&R[1]:&kb[26];
      m1=up?vk_snap2u(o1,ix,iy,ks):vk_snap2d(o1,ix,iy,ks);vk_snapd(sx,ix,ks);if(up)vk_snapu(sy,iy,ks);else vk_snapd(sy,iy,ks);m2=vk_snapok(ix,ks)|(vk_snapok(iy,ks)<<4);
      for(q=0;q<4;q++)if(o1[2*q]!=LANE_U(*sx,q)||o1[2*q+1]!=LANE_U(*sy,q))m2=~0U;
      if(m1!=m2)e[10+up]++;}}}
    memset(d1,0x5A,sizeof(d1));memset(d2,0x5A,sizeof(d2));
    vk_front_in(kb,X->f,Y->f,Z->f,d1,c1);lane_mask(keep,15);vk_project(c4,keep,&kb[17],p4);vk_viewport_store(p4,&kb[18],d2,28);vk_tr_store(c4,c2);
    if(memcmp(d1,d2,sizeof(d1))||memcmp(c1,c2,64))e[1]++;}
   { /* vk_xfn_norm: R[0..8] the normal matrix's splats */
    v4 *kl=R+60;v4x3 *o1=(v4x3*)(R+64),*o2=(v4x3*)(R+68);v4 *len2=R+72;
    splat(&kl[0],0.5F);splat(&kl[1],3.0F);vk_xfn_norm(R,X->f,Y->f,Z->f,o1,kl);vk_xfn(R,X->f,Y->f,Z->f,o2);vk_norm3(o2,kl,len2);
    if(memcmp(o1,o2,48)||memcmp(&kl[2],len2,16))e[2]++;}
   { /* vk_lpos: p R[9..11], e R[12..14] */
    v4 *kq=R+60,*inv=R+73,*d2v=R+74,*dv=R+75;v4x3 *o1=(v4x3*)(R+64),*o2=(v4x3*)(R+68);
    splat(&kq[0],0.5F);splat(&kq[1],3.0F);vk_lpos(o1,(v4x3*)(R+9),(v4x3*)(R+12),kq);
    vk_sub3(o2,(v4x3*)(R+9),(v4x3*)(R+12));vk_dot3(o2,o2,d2v);vk_rsqrt_nr(inv,d2v,kq);vk_mul(dv,d2v,inv);vk_scale3(o2,inv);
    if(memcmp(o1,o2,48)||memcmp(&kq[2],d2v,16)||memcmp(&kq[3],dv,16))e[3]++;}
   { /* vk_ldiff: n R[15..17], vp R[18..20], dif R[21..23], amb R[24..26] */
    v4x3 *o1=(v4x3*)(R+64),*o2=(v4x3*)(R+68);v4 *ndv=R+73,*fm=R+74,*zero=R+75;unsigned r1,r2;
    splat(zero,0.0F);r1=vk_ldiff(o1,(v4x3*)(R+15),(v4x3*)(R+18),(v4x3*)(R+21),(v4x3*)(R+24));
    vk_dot3((v4x3*)(R+15),(v4x3*)(R+18),ndv);r2=vk_gemask(fm,ndv,zero);r2|=vk_unord(ndv)<<4;vk_madd3(o2,(v4x3*)(R+21),ndv,(v4x3*)(R+24));vk_blend3(o2,fm,o2,(v4x3*)(R+24));
    if(r1!=r2||memcmp(o1,o2,48))e[4]++;}
   { /* vk_lacc, vk_laccs: s R[27..29], c R[30..32], att R[33], amask from R[34]'s signs, spc R[35..37], spec R[38] */
    v4x3 *s1=(v4x3*)(R+64),*s2=(v4x3*)(R+68),*t3=(v4x3*)(R+76),*c2=(v4x3*)(R+80);v4 *am=R+73;GLuint q,m=0;
    for(q=0;q<4;q++)if((GLint)LANE_U(R[34],q)<0)m|=1U<<q;lane_mask(am,m);
    memcpy(s1,R+27,48);memcpy(s2,R+27,48);vk_lacc(s1,(v4x3*)(R+30),R+33,am);vk_madd3(t3,(v4x3*)(R+30),R+33,s2);vk_blend3(s2,am,t3,s2);
    if(memcmp(s1,s2,48))e[5]++;
    memcpy(s1,R+27,48);memcpy(s2,R+27,48);vk_laccs(s1,(v4x3*)(R+30),R+33,am,(v4x3*)(R+35),R+38);
    vk_madd3(c2,(v4x3*)(R+35),R+38,(v4x3*)(R+30));vk_madd3(t3,c2,R+33,s2);vk_blend3(s2,am,t3,s2);
    if(memcmp(s1,s2,48))e[6]++;}
   { /* vk_lhalf_sub, vk_lhalf_add: vp R[18..20], o R[9..11], n R[15..17] */
    v4 *o1=R+60,*kk=R+62,*len2=R+73,*ndh2=R+74;v4x3 *h=(v4x3*)(R+64);
    splat(&kk[0],0.5F);splat(&kk[1],3.0F);
    vk_lhalf_sub(o1,(v4x3*)(R+18),(v4x3*)(R+9),(v4x3*)(R+15),kk);vk_sub3(h,(v4x3*)(R+18),(v4x3*)(R+9));vk_norm3(h,kk,len2);vk_dot3((v4x3*)(R+15),h,ndh2);
    if(memcmp(&o1[0],len2,16)||memcmp(&o1[1],ndh2,16))e[7]++;
    vk_lhalf_add(o1,(v4x3*)(R+18),(v4x3*)(R+9),(v4x3*)(R+15),kk);vk_add3(h,(v4x3*)(R+18),(v4x3*)(R+9));vk_norm3(h,kk,len2);vk_dot3((v4x3*)(R+15),h,ndh2);
    if(memcmp(&o1[0],len2,16)||memcmp(&o1[1],ndh2,16))e[8]++;}
   { /* v10.497: vk_box3 on R[0..2] (one group) against the C compares */
    v4 *b=R+101;GLuint q,c,bad=0;const v4 *in3[3];in3[0]=&R[0];in3[1]=&R[1];in3[2]=&R[2];
    splat(&b[0],3.4e38F);splat(&b[1],-3.4e38F);b[2]=b[0];b[3]=b[1];b[4]=b[0];b[5]=b[1];memset(&b[6],0,16);{GLuint *u=(GLuint*)b[7].f;u[0]=u[1]=u[2]=u[3]=0x7FFFFFFFUL;}splat(&b[8],1e30F);
    vk_box3(b,R[0].f,R[1].f,R[2].f);
    {GLuint fl=0;for(c=0;c<3;c++)for(q=0;q<4;q++){GLfloat f=in3[c]->f[q];if(!(f>-1e30F&&f<1e30F))fl|=1U<<q;}for(q=0;q<4;q++)if(((fl>>q)&1)!=(LANE_U(b[6],q)!=0))bad=1;}
    {GLuint ok=1;for(c=0;c<3;c++)for(q=0;q<4;q++){GLfloat f=in3[c]->f[q];if(!(f>-1e30F&&f<1e30F))ok=0;}
     if(ok)for(c=0;c<3;c++)for(q=0;q<4;q++)if(b[2*c].f[q]!=in3[c]->f[q]||b[2*c+1].f[q]!=in3[c]->f[q])bad=1;}
    if(bad)e[12]++;}}
  for(j=0;j<13;j++)out[j]=e[j];out[13]=nt;mx_leave(mx);return;}
 {GLuint *u=(GLuint*)k[0].f;u[0]=u[1]=u[2]=u[3]=0x7FFFFFFFUL;u=(GLuint*)k[7].f;u[0]=u[1]=u[2]=u[3]=0x80000000UL;}
 if(which==3){splat(&k[0],(GLfloat)(SHINE_TABLE_SIZE-1));splat(&k[1],12582912.0F);splat(&k[2],1.0F);} /* k[3..7] the lookup's temporaries */
 else if(which<2){splat(&k[1],2048.0F);splat(&k[2],12582912.0F);splat(&k[3],1.0F);splat(&k[4],0.5F);splat(&k[5],1.0F/128.0F);splat(&k[6],1.0F/16.0F);splat(&k[8],2040.0F);}
 else{splat(&k[0],255.0F/256.0F);splat(&k[1],32768.0F);splat(&k[2],0.0F);{GLuint *u=(GLuint*)k[3].f;u[0]=u[1]=u[2]=u[3]=0x3F7F0000UL;}splat(&k[4],1.0F);}
 for(i=0;i<n;i+=4){GLuint m=n-i<4?n-i:4;
  for(v=0;v<4;v++)xs[v]=v<m?in[i+v]:0;
  if(which<2){GLuint ok=vk_snapok(x,k);if(which)vk_snapu(y,x,k);else vk_snapd(y,x,k);for(v=0;v<m;v++)out[i+v]=(ok>>v)&1?LANE_U(*y,v):vx_d3d_snapw(xs[v],(int)which);}
  else if(which==3){ /* the specular lookup as sse_group does it, from tab (257 entries) for n.h in in: 0xFFFFFFFF where _mesa_pow would be called */
   v4 *sh=k+3;GLuint *t0=(GLuint*)sh[3].f,*t1=(GLuint*)sh[4].f;
   vk_shine_a(sh,x,k);
   for(v=0;v<4;v++){if(LANE_U(sh[0],v)<0x437F0000UL){GLuint kk=LANE_U(sh[2],v)-0x4B400000UL;t0[v]=((const GLuint*)tab)[kk];t1[v]=((const GLuint*)tab)[kk+1];}else t0[v]=t1[v]=0;}
   vk_shine_b(b,sh);for(v=0;v<m;v++)out[i+v]=LANE_U(sh[0],v)<0x437F0000UL?LANE_U(*b,v):0xFFFFFFFFUL;}
  else if(vk_unord(x)){vk_bias(b,x,k);for(v=0;v<m;v++)out[i+v]=ub_from(xs[v],LANE_U(*b,v));}
  else{vk_ub(b,x,k);for(v=0;v<m;v++)out[i+v]=LANE_U(*b,v)&255U;}}
 mx_leave(mx);
}
#endif
#ifndef VK_SSE
GLuint *vxgl_ssepass_snapstore(const GLubyte *verts,GLuint vs,GLuint n,int up) {(void)verts;(void)vs;(void)n;(void)up;return 0;} /* v10.500 */
#endif

static GLboolean sp_alloc(GLcontext *ctx,struct tnl_pipeline_stage *stage) {
 TNLcontext *tnl=TNL_CONTEXT(ctx);sp_store *st=(sp_store*)_mesa_align_malloc(sizeof(sp_store),16);
 if(!st)return GL_FALSE;
 memset(st,0,sizeof(*st));
#ifdef VK_SSE
 if(!clip_tab_ok)clip_tab_make(); /* v10.493 */
 {GLuint *u=(GLuint*)st->sign.f;u[0]=u[1]=u[2]=u[3]=0x80000000UL;} /* v10.497: once (vk_xfbits, vk_front) */
 {static const GLfloat lo[3]={1e-20F,1e-30F,1e-12F};int k;for(k=0;k<3;k++){splat(&st->krb[k][0],lo[k]);splat(&st->krb[k][1],1e30F);}} /* .505 */
#endif
 _mesa_vector4f_alloc(&st->clip,0,tnl->vb.Size,32);
 st->clipmask=(GLubyte*)_mesa_align_malloc(tnl->vb.Size,32);
 st->immbuf=(GLubyte*)_mesa_align_malloc(((tnl->vb.Size+3)&~3U)*16*sizeof(GLfloat),16); /* v10.488: x y z, normals, r g b a, three coordinate pairs */
 st->snapbuf=(GLuint*)_mesa_align_malloc((tnl->vb.Size+4)*2*sizeof(GLuint),16); /* v10.490 */
 if(!st->clip.data||!st->clipmask||!st->immbuf||!st->snapbuf){if(st->clip.data)_mesa_vector4f_free(&st->clip);if(st->clipmask)_mesa_align_free(st->clipmask);if(st->immbuf)_mesa_align_free(st->immbuf);
  if(st->snapbuf)_mesa_align_free(st->snapbuf);_mesa_align_free(st);return GL_FALSE;}
 stage->privatePtr=st;
 return GL_TRUE;
}
static void sp_free(struct tnl_pipeline_stage *stage) {
 sp_store *st=(sp_store*)stage->privatePtr;
 if(st){if(st->need)_mesa_free(st->need); /* .503 */_mesa_vector4f_free(&st->clip);_mesa_align_free(st->clipmask);_mesa_align_free(st->immbuf);_mesa_align_free(st->snapbuf);_mesa_align_free(st);stage->privatePtr=0;}
}

/* v10.488: a draw not from a list (glBegin/glEnd, glDrawArrays): its vertex buffer's inputs into the store's SoA arrays, as the list's copy
 * is made at glEndList (the padding lanes repeat the last vertex); an input with stride 0 (the current value) is copied into every lane for
 * the normals, and left out for the colours and coordinate sets (the pass's constant paths); 0 when the pass cannot take the draw */
static const vx_soa *imm_build(GLcontext *ctx,sp_store *st) { /* .506: lit draws only (the normals always) */
 TNLcontext *tnl=TNL_CONTEXT(ctx);struct vertex_buffer *VB=&tnl->vb;vx_soa *so=&st->imm;GLuint n=VB->Count,n4=(n+3)&~3U,i,u;GLfloat *mem=(GLfloat*)st->immbuf;
 const GLvector4f *P=VB->AttribPtr[_TNL_ATTRIB_POS],*N=VB->AttribPtr[_TNL_ATTRIB_NORMAL],*C=VB->AttribPtr[_TNL_ATTRIB_COLOR0];
 if(!n||VB->Elts||!P||P->size<2||P->size>3||n4>((tnl->vb.Size+3)&~3U))return 0;
 memset(so,0,sizeof(*so));so->count=n;so->n4=n4;
 so->x=mem;mem+=n4;so->y=mem;mem+=n4;so->z=mem;mem+=n4;
 {const GLubyte *b=(const GLubyte*)P->data;GLuint ps=P->stride,sz=P->size;
  for(i=0;i<n4;i++){const GLfloat *v=(const GLfloat*)(b+(i<n?i:n-1)*ps);so->x[i]=v[0];so->y[i]=v[1];so->z[i]=sz>2?v[2]:0.0F;}
  if(vxgl_ssepass_immbox){ /* v10.496 (ACCEL_OPENGL_SSEIMMBOX): the box of the draw's positions as a list's is made (box_test: whole outside, or inside for SSEACCEPT);
   * v10.497 (board, VXFLAT v2.4: the x87 loop cost more than the inside draws saved): four lanes at a time (vk_box3: min, max, and |p| >= 1e30 or a NaN), the
   * padding lanes the last vertex again */
#ifdef VK_SSE
   v4 *b=st->bx;GLuint j;splat(&b[0],3.4e38F);splat(&b[1],-3.4e38F);b[2]=b[0];b[3]=b[1];b[4]=b[0];b[5]=b[1];memset(&b[6],0,16);
   {GLuint *u=(GLuint*)b[7].f;u[0]=u[1]=u[2]=u[3]=0x7FFFFFFFUL;}splat(&b[8],1e30F);
   for(i=0;i<n4;i+=4)vk_box3(b,so->x+i,so->y+i,so->z+i);
   so->hasbox=!(LANE_U(b[6],0)|LANE_U(b[6],1)|LANE_U(b[6],2)|LANE_U(b[6],3));
   for(j=0;j<3;j++){GLuint v;GLfloat lo=b[2*j].f[0],hi=b[2*j+1].f[0];for(v=1;v<4;v++){if(b[2*j].f[v]<lo)lo=b[2*j].f[v];if(b[2*j+1].f[v]>hi)hi=b[2*j+1].f[v];}so->bmin[j]=lo;so->bmax[j]=hi;}
#else
   so->hasbox=1;
   for(i=0;i<n;i++){GLfloat p[3];int j;p[0]=so->x[i];p[1]=so->y[i];p[2]=so->z[i];
    for(j=0;j<3;j++){if(!(p[j]>-1e30F&&p[j]<1e30F))so->hasbox=0;if(!i||p[j]<so->bmin[j])so->bmin[j]=p[j];if(!i||p[j]>so->bmax[j])so->bmax[j]=p[j];}}
#endif
  }}
 if(N){const GLubyte *b=(const GLubyte*)N->data;GLuint ns=N->stride;so->have|=1;so->nx=mem;mem+=n4;so->ny=mem;mem+=n4;so->nz=mem;mem+=n4;
  for(i=0;i<n4;i++){const GLfloat *v=(const GLfloat*)(b+(i<n?i:n-1)*ns);so->nx[i]=v[0];so->ny[i]=v[1];so->nz[i]=v[2];}}
 if(C&&C->stride&&C->size>=3){const GLubyte *b=(const GLubyte*)C->data;GLuint cs=C->stride,sz=C->size;so->have|=2;so->const_color=1;
  so->r=mem;mem+=n4;so->g=mem;mem+=n4;so->b=mem;mem+=n4;so->a=mem;mem+=n4;
  for(i=0;i<n4;i++){const GLfloat *v=(const GLfloat*)(b+(i<n?i:n-1)*cs);so->r[i]=v[0];so->g[i]=v[1];so->b[i]=v[2];so->a[i]=sz>3?v[3]:1.0F;
   if(i&&(so->r[i]!=so->r[0]||so->g[i]!=so->g[0]||so->b[i]!=so->b[0]||so->a[i]!=so->a[0]))so->const_color=0;}}
 for(u=0;u<SOA_TEX;u++){const GLvector4f *T=VB->AttribPtr[_TNL_ATTRIB_TEX0+u];
  if(T&&T->stride&&T->size>=1&&T->size<=4){const GLubyte *b=(const GLubyte*)T->data;GLuint ts=T->stride,sz=T->size;so->have|=4U<<u;so->s[u]=mem;mem+=n4;so->t[u]=mem;mem+=n4;
   for(i=0;i<n4;i++){const GLfloat *v=(const GLfloat*)(b+(i<n?i:n-1)*ts);so->s[u][i]=v[0];so->t[u][i]=sz>1?v[1]:0.0F;}}}
 return so;
}
/* v10.489 (board, Lattice v10.488: 1.9 frames a second against 2.2 with Mesa's stages; 227,237 of its 461,487 draws whole outside, and the
 * pass lit, fogged and emitted every vertex of them): as Mesa's vertex stage, which stops the pipeline after the clip test when every vertex
 * is outside one plane, the draw's clip masks first (the transform and the compares only, a group at a time, stopping at the first group
 * that leaves no plane all vertices are outside of): 1 when nothing is to be drawn. The same compares as sse_group's (clip1's). */
/* v10.490 (board, Lattice v10.489: sp_prepare 6% of the samples, all_outside transforming every vertex of the half of the draws whole
 * outside): a list's draw whole outside one plane from its bounding box. The clip coordinates are affine in the position, so x - w (the
 * right plane's w < x; the others alike) over the box is least at a corner: when every corner's value, in double, is above twice the bound
 * on the single-precision transform's rounding (each row's |m| times the box's largest |coordinate|, 8 units of 2^-24 for the four
 * products and three sums), the pass's own compares (vk_xf4, vk_clipbits) find every vertex outside that plane. 1: whole outside; 0: not
 * known (the vertices tested as before). */
static int box_outside(const GLfloat *m,const vx_soa *so) {
 double A[3],E[4],c[4];int r,j,k,out=0x3F;
 for(j=0;j<3;j++){double lo=so->bmin[j]<0?-(double)so->bmin[j]:so->bmin[j],hi=so->bmax[j]<0?-(double)so->bmax[j]:so->bmax[j];A[j]=lo>hi?lo:hi;}
 for(r=0;r<4;r++){double t=0;for(j=0;j<4;j++){double e=m[4*j+r]<0?-(double)m[4*j+r]:m[4*j+r];t+=e*(j<3?A[j]:1.0);}E[r]=t*(8.0/16777216.0);}
 for(k=0;k<8&&out;k++){double p[3];p[0]=(k&1)?so->bmax[0]:so->bmin[0];p[1]=(k&2)?so->bmax[1]:so->bmin[1];p[2]=(k&4)?so->bmax[2]:so->bmin[2];
  for(r=0;r<4;r++)c[r]=((double)m[r]*p[0]+(double)m[4+r]*p[1])+(double)m[8+r]*p[2]+(double)m[12+r];
  for(j=0;j<3;j++){double mg=2.0*(E[j]+E[3]);if(!(c[j]-c[3]>mg))out&=~(1<<(2*j));if(!(-c[j]-c[3]>mg))out&=~(2<<(2*j));}}
 return out!=0;
}
/* v10.495 (ACCEL_OPENGL_SSEBOX2; board, Lattice v10.493: box_outside 1.9% of the samples, eight corners by four rows in double): the same
 * test from the box's centre and half sizes a plane at a time. For a plane, x - w (right; s x - w, s = +-1, for the others alike) is affine in
 * the position, a.p + b; over the box its least value is a.C + b - |a|.H and its greatest a.C + b + |a|.H (C the centre, H the half sizes), the
 * corners' least and greatest. The double arithmetic's own rounding (a few units of 2^-53 of the terms' sizes) is covered a million times over
 * by the margin's extra millionth (the margin is 8 units of 2^-24 of the same sizes, doubled). Returns -1 when every vertex is outside one plane
 * (the least above the margin, as box_outside); 1 when every vertex is inside every plane (each greatest below minus the margin: the pass's
 * single-precision values then set no compare, ACCEL_OPENGL_SSEACCEPT); 0 not known (a NaN or an infinity in the matrix: neither). */
static int box_test(const GLfloat *m,const vx_soa *so) {
 double A[3],E[4],C[3],H[3];int r,j,sg,in=1;
 for(j=0;j<3;j++){double lo=so->bmin[j]<0?-(double)so->bmin[j]:so->bmin[j],hi=so->bmax[j]<0?-(double)so->bmax[j]:so->bmax[j];A[j]=lo>hi?lo:hi;
  C[j]=0.5*((double)so->bmin[j]+(double)so->bmax[j]);H[j]=0.5*((double)so->bmax[j]-(double)so->bmin[j]);}
 for(r=0;r<4;r++){double t=0;for(j=0;j<4;j++){double e=m[4*j+r]<0?-(double)m[4*j+r]:m[4*j+r];t+=e*(j<3?A[j]:1.0);}E[r]=t*(8.0/16777216.0);}
 for(j=0;j<3;j++){double mg=2.000001*(E[j]+E[3]);
  for(sg=1;sg>=-1;sg-=2){double a0=sg*(double)m[j]-(double)m[3],a1=sg*(double)m[4+j]-(double)m[7],a2=sg*(double)m[8+j]-(double)m[11],b=sg*(double)m[12+j]-(double)m[15];
   double f=((a0*C[0]+a1*C[1])+a2*C[2])+b,h=(a0<0?-a0:a0)*H[0]+(a1<0?-a1:a1)*H[1]+(a2<0?-a2:a2)*H[2];
   if(f-h>mg)return -1;
   if(!(f+h<-mg))in=0;}}
 return in;
}
/* v10.492: the model-projection matrix's splats, made again only when it changed (ACCEL_OPENGL_SSECACHE) */
static void set_mvp(sp_store *st,const GLfloat *m) {
 GLuint i;if(vxgl_ssepass_cache){const GLuint *a=(const GLuint*)st->mvp_c,*b=(const GLuint*)m; /* v10.497: the words compared inline (memcmp a call) */
  for(i=0;i<16&&a[i]==b[i];i++);if(i==16)return;}
 memcpy(st->mvp_c,m,sizeof(st->mvp_c));for(i=0;i<16;i++)splat(&st->mvp[i],m[i]);
}
static int all_outside(GLcontext *ctx,sp_store *st,const vx_soa *so) {
#ifdef VK_SSE
 const GLfloat *m=ctx->_ModelProjectMatrix.m;GLuint i,v,n=so->count,am=CLIP_FRUSTUM_BITS;unsigned mx;
 if(vxgl_ssepass_box&&so->hasbox){
  if(vxgl_ssepass_box2){int b=box_test(m,so);if(b<0){vxgl_ssepass_stat[10]++;if(so==&st->imm)vxgl_ssepass_imm_stat[0]++;return 1;}if(b>0){st->inside=vxgl_ssepass_accept;if(so==&st->imm&&st->inside)vxgl_ssepass_imm_stat[1]++;return 0;}} /* v10.495: inside: no group can have a vertex outside; v10.496: [0] [1] of them not from a list */
  else if(box_outside(m,so)){vxgl_ssepass_stat[10]++;return 1;}} /* v10.490 */
 set_mvp(st,m);
 if(vxgl_ssepass_outfast){ /* v10.497: the transform and the compares in the registers (vk_xfbits), the planes every vertex so far is outside kept as six bits
  * (the same decision as the masks' AND below); MXCSR set by sp_run */
  GLuint pm=0x3FU;
  for(i=0;i<n&&pm;i+=4){GLuint k=n-i<4?n-i:4,lm=(1U<<k)-1U,p;unsigned bits=vk_xfbits(st->mvp,so->x+i,so->y+i,so->z+i);
   for(p=0;p<6;p++)if(((bits>>(4*p))&lm)!=lm)pm&=~(1U<<p);}
  return pm!=0;}
 {GLuint *u=(GLuint*)st->sign.f;u[0]=u[1]=u[2]=u[3]=0x80000000UL;}
 mx=mx_enter();
 for(i=0;i<n&&am;i+=4){GLuint k=n-i<4?n-i:4;unsigned bits;
  vk_xf4(st->mvp,so->x+i,so->y+i,so->z+i,&st->c4);bits=vk_clipbits(&st->c4,&st->sign);
  for(v=0;v<k;v++){unsigned hi=((bits>>v)&1)|((bits>>(3+v))&2)|((bits>>(6+v))&4),lo=((bits>>(12+v))&1)|((bits>>(15+v))&2)|((bits>>(18+v))&4);am&=(GLuint)(bits_hi[hi]|bits_lo[lo]);}}
 mx_leave(mx);
 return am!=0;
#else
 (void)ctx;(void)st;(void)so;return 0;
#endif
}
/* v10.492 (ACCEL_OPENGL_SSECACHE): everything the lighting's set-up reads (the material, the model, each enabled light's colours, position,
 * directions, attenuation and spot, colour material's tracking) as words in k; the count, or 0 with more lights than the pass takes (no keeping).
 * The same words: _mesa_update_material, the shine tables and the light splats would come out the same, so the last ones are kept. */
static GLuint light_key(GLcontext *ctx,const sp_store *st,GLfloat *k) {
 GLuint n=0,a,cnt=0;struct gl_light *light;
#define LK_W(x) do{GLuint w_=(GLuint)(x);memcpy(k+n,&w_,4);n++;}while(0)
#define LK_F(p,m) do{memcpy(k+n,(p),(m)*sizeof(GLfloat));n+=(m);}while(0)
 LK_W(st->colmat);LK_W(ctx->Light.ColorMaterialBitmask);LK_W(ctx->Light.ColorMaterialEnabled);LK_W(ctx->Light.Model.LocalViewer);
 for(a=0;a<MAT_ATTRIB_MAX;a++)LK_F(ctx->Light.Material.Attrib[a],4);
 LK_F(ctx->Light.Model.Ambient,4);LK_F(ctx->_EyeZDir,3);
 foreach(light,&ctx->Light.EnabledList){if(++cnt>SP_LIGHTS)return 0;
  {unsigned long pa=(unsigned long)light;memcpy(k+n,&pa,sizeof(pa));n+=(GLuint)((sizeof(pa)+3)/4);}
  LK_W(light->_Flags);LK_F(light->Ambient,4);LK_F(light->Diffuse,4);LK_F(light->Specular,4);LK_F(light->_Position,4);LK_F(light->_VP_inf_norm,3);LK_F(light->_h_inf_norm,3);
  LK_F(light->_NormDirection,3);LK_F(&light->_CosCutoff,1);LK_F(&light->_VP_inf_spot_attenuation,1);LK_F(&light->ConstantAttenuation,1);LK_F(&light->LinearAttenuation,1);
  LK_F(&light->QuadraticAttenuation,1);LK_F(&light->SpotExponent,1);LK_F(&light->SpotCutoff,1);}
#undef LK_W
#undef LK_F
 return n;
}
/* the state and the draw: 0 when the pass takes it, else the reason bits; fills the store's constants */
static GLuint sp_prepare(GLcontext *ctx,sp_store *st) {
 vxgl_ctx *c=VXGL_CTX(ctx);TNLcontext *tnl=TNL_CONTEXT(ctx);struct vertex_buffer *VB=&tnl->vb;const struct vbo_save_vertex_list *node=vbo_context(ctx)->save.vx855_node;
 const vx_soa *so=0;GLuint why=0,i;
 st->inside=0; /* v10.495 */
 if(node){so=(const vx_soa*)node->vx855_soa;if(!so)return SP_NOSOA;}
 else if(!(vxgl_ssepass_on&4))return SP_NO_LIST|SP_IMM;
 if(c->fallback)why|=SP_FALLBACK;
 if(ctx->VertexProgram._Current||ctx->FragmentProgram._Current||ctx->VertexProgram._MaintainTnlProgram)why|=SP_PROGRAM;
 if(node&&(VB->Count!=so->count||VB->Elts||!VB->AttribPtr[_TNL_ATTRIB_POS]||VB->AttribPtr[_TNL_ATTRIB_POS]->stride!=node->vertex_size*sizeof(GLfloat)
  ||(const GLubyte*)VB->AttribPtr[_TNL_ATTRIB_POS]->data!=(const GLubyte*)node->vertex_store->bufferobj->Pointer+node->buffer_offset))why|=SP_SPLIT;
 if(ctx->Transform.ClipPlanesEnabled)why|=SP_CLIPPLANES;
 if(ctx->Point._Attenuated)why|=SP_POINTS;
 if(!ctx->Light.Enabled)why|=SP_UNLIT; /* v10.488; .506: always (ACCEL_OPENGL_SSEUNLIT, unlit draws by the pass, 4% slower on the board, removed) */
 if(why)return why;
 /* v10.502 (board, VXFLAT v2.9: every q3 arrays draw's byte colours converted, v10.501 did it at the pass's entry, before the draw was
  * declined as unlit): the floats made only for a draw the pass may take (it reads the colours from here on) */
 {extern int _tnl_vx855_ub_is_active(GLcontext*);extern void _tnl_vx855_ub_float(GLcontext*);if(_tnl_vx855_ub_is_active(ctx))_tnl_vx855_ub_float(ctx);}
 if(!node&&vxgl_ssepass_early&&ctx->Light.Enabled){ /* v10.497 (ACCEL_OPENGL_SSEEARLY; board, VXFLAT v2.4: the lit scenes Mesa's fast lighting draws 5-12% slower with
  * the box of v10.496, every vertex copied for a draw the pass then left): the state the lighting checks below decline, before the copy */
  GLuint e=0;
  if(!ctx->Visual.rgbMode||!ctx->Light._NeedVertices||!ctx->_NeedEyeCoords||ctx->Light.Model.TwoSide||ctx->Light.Model.ColorControl==GL_SEPARATE_SPECULAR_COLOR)e|=SP_LIGHTKIND;
  if(!ctx->ModelviewMatrixStack.Top->inv)e|=SP_NORMALS;
  if(e){if(RENDERINPUTS_TEST(tnl->render_inputs_bitset,_TNL_ATTRIB_COLOR1))e|=SP_LAYOUT;vxgl_ssepass_imm_stat[2]++;return e;}}
 if(!node){so=imm_build(ctx,st);if(!so)return SP_IMM;} /* v10.488 */
 if(vxgl_ssepass_outside&&all_outside(ctx,st,so))return SP_OUTSIDE; /* v10.489: before the material and the lighting, as Mesa's pipeline */
 /* the vertex layout the render stage will use (vxgl_tris.c choose_vertex_state; a second call there changes nothing) */
 vxgl_choose_vertex_state(ctx);
 st->vs=c->vertex_size;st->col_off=c->coloroffset*4;st->fog_off=c->specoffset?c->specoffset*4+3:0;st->tex_sets=c->tex_sets;
 if(c->coloroffset!=4||(c->specoffset&&c->specoffset!=5)||c->tex_layout||c->tex_set3||c->tex_sets>2||c->attr_count<2)why|=SP_LAYOUT;
 if(RENDERINPUTS_TEST(tnl->render_inputs_bitset,_TNL_ATTRIB_COLOR1))why|=SP_LAYOUT;
 if(st->vs!=(c->specoffset?24U:20U)+8U*(GLuint)c->tex_sets)why|=SP_LAYOUT;
 for(i=0;i<(GLuint)c->tex_sets;i++){int u=c->tex_unit_of[i];const GLvector4f *tv;
  st->tex_off[i]=(c->specoffset?24U:20U)+8U*i;
  if(u<0||u>=SOA_TEX||(c->tex_units>(int)i&&c->tex_xf[i].on))why|=SP_TEXSTATE;
  if((ctx->Texture._TexGenEnabled|ctx->Texture._TexMatEnabled)&(1U<<u))why|=SP_TEXSTATE;
  if(why)break;
  tv=VB->AttribPtr[_TNL_ATTRIB_TEX0+u];
  if(so->have&(4U<<u)){st->tex_src[i]=u;if(!tv||tv->stride==0)why|=SP_TEXSTATE;} /* the list has the set: the vertex buffer must read it from the list */
  else {st->tex_src[i]=-1;if(!tv||tv->stride!=0)why|=SP_TEXSTATE;st->const_tex[i]=(const GLfloat*)tv->data;}}
 if(ctx->Fog.Enabled&&c->specoffset){if(ctx->Fog.FogCoordinateSource!=GL_FRAGMENT_DEPTH_EXT||!tnl->_DoVertexFog||!RENDERINPUTS_TEST(tnl->render_inputs_bitset,_TNL_ATTRIB_FOG))why|=SP_FOGKIND;
  st->fog=1;st->fogmode=ctx->Fog.Mode;st->fogd=ctx->Fog.Mode==GL_EXP2?ctx->Fog.Density*ctx->Fog.Density:ctx->Fog.Mode==GL_EXP?ctx->Fog.Density:(ctx->Fog.Start==ctx->Fog.End?1.0F:1.0F/(ctx->Fog.End-ctx->Fog.Start));
  st->fogend=ctx->Fog.End;st->exptab=_tnl_vx855_fog_exp_table();if(ctx->Fog.Mode!=GL_LINEAR&&ctx->Fog.Mode!=GL_EXP&&ctx->Fog.Mode!=GL_EXP2)why|=SP_FOGKIND;}
 else {st->fog=0;if(ctx->Fog.Enabled||(c->specoffset&&RENDERINPUTS_TEST(tnl->render_inputs_bitset,_TNL_ATTRIB_FOG)))why|=SP_FOGKIND;}
 st->lit=ctx->Light.Enabled?1:0;st->colmat=0;
 if(st->lit) {
  struct gl_light *light;int n=0,keep=0;
  if(!ctx->Visual.rgbMode||!ctx->Light._NeedVertices||!ctx->_NeedEyeCoords||ctx->Light.Model.TwoSide||ctx->Light.Model.ColorControl==GL_SEPARATE_SPECULAR_COLOR)why|=SP_LIGHTKIND;
  if(!(so->have&1)||!ctx->ModelviewMatrixStack.Top->inv)why|=SP_NORMALS; /* the normals from the list (a constant normal: the pipeline) */
  {GLuint a;for(a=_TNL_FIRST_MAT;a<=_TNL_LAST_MAT;a++)if(VB->AttribPtr[a]&&VB->AttribPtr[a]->stride)why|=SP_MATERIAL;} /* a material per vertex: the list's (not built) */
  if(ctx->Light.ColorMaterialEnabled&&(so->have&2)&&!so->const_color){ /* colours differing with colour material: Mesa updates the material a vertex (update_materials); the
   * pass works the tracked ambient, diffuse and emission out a group at a time; a tracked specular, shininess or index: the pipeline */
   GLuint bm=ctx->Light.ColorMaterialBitmask;const GLvector4f *cv=VB->AttribPtr[_TNL_ATTRIB_COLOR0];
   if(bm&(MAT_BIT_FRONT_SPECULAR|MAT_BIT_FRONT_SHININESS|MAT_BIT_FRONT_INDEXES|MAT_BIT_BACK_SPECULAR|MAT_BIT_BACK_SHININESS|MAT_BIT_BACK_INDEXES))why|=SP_MATERIAL;
   if(!cv||cv->stride==0)why|=SP_LAYOUT; /* the list's colours must be the ones bound */
   st->colmat=((bm&MAT_BIT_FRONT_AMBIENT)?1:0)|((bm&MAT_BIT_FRONT_DIFFUSE)?2:0)|((bm&MAT_BIT_FRONT_EMISSION)?4:0);}
  if(!why) { /* as prepare_materials: with colour material and the list's one colour, that colour into the tracking materials once (Mesa does it a vertex at a time, the
             * same colour each time: the state after the draw the same); then the derived light-material values and the shine tables made current */
   GLuint bm=0,kn;GLfloat key[sizeof(st->lk)/sizeof(GLfloat)];
   if(ctx->Light.ColorMaterialEnabled&&(so->have&2)){GLuint a;GLfloat col[4];col[0]=so->r[0];col[1]=so->g[0];col[2]=so->b[0];col[3]=so->a[0];bm=ctx->Light.ColorMaterialBitmask;
    for(a=0;a<MAT_ATTRIB_MAX;a++)if(bm&(1U<<a))COPY_4FV(ctx->Light.Material.Attrib[a],col);}
   kn=vxgl_ssepass_cache?light_key(ctx,st,key):0; /* v10.492: the same inputs as the last set-up: kept */
   if(kn&&st->lk_valid&&kn==st->lk_n&&!memcmp(key,st->lk,kn*sizeof(GLfloat)))keep=1;
   else if(vxgl_ssepass_partial&&kn&&st->lk_valid&&kn==st->lk_n&&!memcmp(key,st->lk,4*sizeof(GLfloat))
    &&!memcmp(key+4+4*MAT_ATTRIB_MAX,st->lk+4+4*MAT_ATTRIB_MAX,(kn-4-4*MAT_ATTRIB_MAX)*sizeof(GLfloat))){ /* v10.495 (ACCEL_OPENGL_SSEPARTIAL; board, Lattice v10.493: 151k
    * of its 181k lit set-ups made whole, each list's colour the tracked material): only materials differ from the last set-up's (the lights, the
    * model, colour material's mode the same): _mesa_update_material for those (Mesa's own per-attribute update; with the shininess the shine
    * tables validated), the other derived values as they are (their inputs the same as when they were made), the lights' material products
    * splatted again below */
    GLuint a,chg=0;
    for(a=0;a<MAT_ATTRIB_MAX;a++)if(memcmp(key+4+4*a,st->lk+4+4*a,4*sizeof(GLfloat)))chg|=1U<<a;
    _mesa_update_material(ctx,chg);if(chg&(MAT_BIT_FRONT_SHININESS|MAT_BIT_BACK_SHININESS))_mesa_validate_all_lighting_tables(ctx);
    memcpy(st->lk,key,kn*sizeof(GLfloat));keep=2;}
   if(!keep){_mesa_update_material(ctx,~0U);_mesa_validate_all_lighting_tables(ctx);st->lk_valid=0;if(kn){memcpy(st->lk,key,kn*sizeof(GLfloat));st->lk_n=kn;st->lk_valid=2;}}}
  st->local=ctx->Light.Model.LocalViewer?1:0;
  st->normmode=ctx->Transform.Normalize?1:(ctx->Transform.RescaleNormals&&ctx->_ModelViewInvScale!=1.0F)?2:0;
  if(keep==2){n=0;why|=st->lk_why;vxgl_ssepass_stat[18]++; /* v10.495: the lights' material products splatted again */
   foreach(light,&ctx->Light.EnabledList){if(n>=st->nlights)break;
    splat3(&st->lt[n].amb,light->_MatAmbient[0]);splat3(&st->lt[n].dif,light->_MatDiffuse[0]);splat3(&st->lt[n].spc,light->_MatSpecular[0]);
    st->li[n].spec=light->_MatSpecular[0][0]!=0.0F||light->_MatSpecular[0][1]!=0.0F||light->_MatSpecular[0][2]!=0.0F;n++;}
   n=st->nlights;}
  else if(keep){why|=st->lk_why;n=st->nlights;vxgl_ssepass_stat[15]++;}
  else
  foreach(light,&ctx->Light.EnabledList) {
   if(n>=SP_LIGHTS){why|=SP_LIGHTKIND;break;}
   if(why)break;
   st->li[n].positional=(light->_Flags&LIGHT_POSITIONAL)?1:0;
   if(st->li[n].positional){splat3(&st->lt[n].p,light->_Position);
    st->li[n].attconst=light->LinearAttenuation==0.0F&&light->QuadraticAttenuation==0.0F;
    splat(&st->lt[n].att,light->ConstantAttenuation==1.0F?1.0F:1.0F/light->ConstantAttenuation);
    splat(&st->lt[n].c,light->ConstantAttenuation);splat(&st->lt[n].l,light->LinearAttenuation);splat(&st->lt[n].q,light->QuadraticAttenuation);
    splat3(&st->lt[n].h,ctx->_EyeZDir);}
   else {splat3(&st->lt[n].p,light->_VP_inf_norm);splat(&st->lt[n].att,light->_VP_inf_spot_attenuation);st->li[n].attconst=1;splat3(&st->lt[n].h,light->_h_inf_norm);}
   st->li[n].spot=st->li[n].positional&&(light->_Flags&LIGHT_SPOT); /* v10.488: spotlights (a directional light has none: Mesa folds it into _VP_inf_spot_attenuation) */
   if(st->li[n].spot){splat3(&st->lt[n].nd,light->_NormDirection);st->li[n].cosc=light->_CosCutoff;st->li[n].sexp=(const GLfloat (*)[2])light->_SpotExpTable;}
   splat3(&st->lt[n].amb,light->_MatAmbient[0]);splat3(&st->lt[n].dif,light->_MatDiffuse[0]);splat3(&st->lt[n].spc,light->_MatSpecular[0]);
   splat3(&st->lt[n].la,light->Ambient);splat3(&st->lt[n].ld,light->Diffuse);
   st->li[n].spc_s=light->_MatSpecular[0];
   st->li[n].spec=light->_MatSpecular[0][0]!=0.0F||light->_MatSpecular[0][1]!=0.0F||light->_MatSpecular[0][2]!=0.0F;
   n++;}
  if(!keep){st->nlights=n;st->lk_valid=st->lk_valid==2&&!why?1:0;st->lk_why=0;} /* v10.492: kept only for a set-up made whole now (a loop left early: nothing kept) */
  splat(&st->sumA,ctx->Light.Material.Attrib[MAT_ATTRIB_FRONT_DIFFUSE][3]);st->const_col[3]=to_ub(st->sumA.f[0]);
  splat3(&st->lma,ctx->Light.Model.Ambient);splat3(&st->em,ctx->Light.Material.Attrib[MAT_ATTRIB_FRONT_EMISSION]);splat3(&st->ma,ctx->Light.Material.Attrib[MAT_ATTRIB_FRONT_AMBIENT]);splat3(&st->md,ctx->Light.Material.Attrib[MAT_ATTRIB_FRONT_DIFFUSE]);
  if(!why){const GLfloat *inv=ctx->ModelviewMatrixStack.Top->inv;GLfloat sc=st->normmode==2?ctx->_ModelViewInvScale:1.0F,t[9];static const int ix[9]={0,1,2,4,5,6,8,9,10};
   for(i=0;i<9;i++)t[i]=st->normmode==2?sc*inv[ix[i]]:inv[ix[i]];
   if(!vxgl_ssepass_cache||memcmp(t,st->nm_c,sizeof(t))){memcpy(st->nm_c,t,sizeof(t));for(i=0;i<9;i++)splat(&st->nm[i],t[i]);}} /* v10.492: splatted only when changed */
 }
 else if(!(so->have&2)){const GLvector4f *cv=VB->AttribPtr[_TNL_ATTRIB_COLOR0];const GLfloat *f;
  if(!cv||cv->stride!=0||cv->size<3)why|=SP_LAYOUT;else{f=(const GLfloat*)cv->data;st->const_col[2]=to_ub(f[0]);st->const_col[1]=to_ub(f[1]);st->const_col[0]=to_ub(f[2]);st->const_col[3]=cv->size>3?to_ub(f[3]):255;}}
 else {const GLvector4f *cv=VB->AttribPtr[_TNL_ATTRIB_COLOR0];if(!cv||cv->stride==0)why|=SP_LAYOUT;} /* the list's colours must be the ones bound */
 if(why)return why;
 {const GLfloat *m=ctx->_ModelProjectMatrix.m,*e=ctx->ModelviewMatrixStack.Top->m,*vp=c->ViewportMatrix.m;GLfloat v6[6];
  set_mvp(st,m); /* v10.492: each splatted only when changed */
  if(!vxgl_ssepass_cache||memcmp(st->mv_c,e,sizeof(st->mv_c))){memcpy(st->mv_c,e,sizeof(st->mv_c));for(i=0;i<16;i++)splat(&st->mv[i],e[i]);}
  v6[0]=vp[0];v6[1]=vp[5];v6[2]=vp[10];v6[3]=vp[12];v6[4]=vp[13];v6[5]=vp[14];
  if(!vxgl_ssepass_cache||memcmp(st->vp_c,v6,sizeof(v6))){memcpy(st->vp_c,v6,sizeof(v6));for(i=0;i<6;i++)splat(&st->vp[i],v6[i]);}}
 if(!st->kinit){st->kinit=1; /* v10.492: the constants once */
 splat(&st->krsq[0],0.5F);splat(&st->krsq[1],3.0F);splat(&st->one,1.0F);{GLuint *u=(GLuint*)st->sign.f;u[0]=u[1]=u[2]=u[3]=0x80000000UL;}
 splat(&st->kbias[0],255.0F/256.0F);splat(&st->kbias[1],32768.0F);
 st->kub[0]=st->kbias[0];st->kub[1]=st->kbias[1];splat(&st->kub[2],0.0F);{GLuint *u=(GLuint*)st->kub[3].f;u[0]=u[1]=u[2]=u[3]=0x3F7F0000UL;}splat(&st->kub[4],1.0F); /* v10.490 */
 splat(&st->ksh[0],(GLfloat)(SHINE_TABLE_SIZE-1));splat(&st->ksh[1],12582912.0F);splat(&st->ksh[2],1.0F);
 {GLuint *u=(GLuint*)st->ksn[0].f;u[0]=u[1]=u[2]=u[3]=0x7FFFFFFFUL;u=(GLuint*)st->ksn[7].f;u[0]=u[1]=u[2]=u[3]=0x80000000UL;}
 splat(&st->ksn[1],2048.0F);splat(&st->ksn[2],12582912.0F);splat(&st->ksn[3],1.0F);splat(&st->ksn[4],0.5F);splat(&st->ksn[5],1.0F/128.0F);splat(&st->ksn[6],1.0F/16.0F);splat(&st->ksn[8],2040.0F);
 splat(&st->kthr,1e-3F);
 splat(&st->kl[0],0.5F);splat(&st->kl[1],3.0F);splat(&st->kq[0],0.5F);splat(&st->kq[1],3.0F);} /* v10.495 */
 {GLuint *u=(GLuint*)st->kfog[0].f;u[0]=u[1]=u[2]=u[3]=0x7FFFFFFFUL;}splat(&st->kfog[1],st->fogend);splat(&st->kfog[2],st->fogd);splat(&st->kfog[3],0.0F);splat(&st->kfog[4],1.0F); /* v10.489 */
 st->so=so;
 {GLuint *u=(GLuint*)st->kvs.f;u[0]=u[1]=u[2]=u[3]=st->vs;} /* v10.495: vk_front's stride */
 if(st->inside)vxgl_ssepass_stat[16]++;
 return 0;
}

static GLboolean sp_run(GLcontext *ctx,struct tnl_pipeline_stage *stage) {
 sp_store *st=(sp_store*)stage->privatePtr;TNLcontext *tnl=TNL_CONTEXT(ctx);struct vertex_buffer *VB=&tnl->vb;vxgl_ctx *c=VXGL_CTX(ctx);
 GLuint why,n,i,ormask=0,andmask=CLIP_FRUSTUM_BITS;GLubyte *verts;GLfloat (*clip)[4];unsigned mx=0;
 if(!st||!vxgl_ssepass_on||(!vbo_context(ctx)->save.vx855_node&&!(vxgl_ssepass_on&4)))return GL_TRUE;
 #ifdef VK_SSE
 if(vxgl_ssepass_outfast)mx=mx_enter(); /* v10.497: MXCSR for the draw's whole-outside test and its groups at once (all_outside set it a call) */
 #endif
 why=sp_prepare(ctx,st);
 #ifdef VK_SSE
 if(why&&vxgl_ssepass_outfast)mx_leave(mx);
 #endif
 if(why==SP_OUTSIDE){vxgl_ssepass_stat[0]++;vxgl_ssepass_stat[5]++;return GL_FALSE;} /* v10.489: nothing drawn (Mesa's vertex stage returns GL_FALSE here too) */
 if(why){vxgl_ssepass_stat[3]++;vxgl_ssepass_stat[6]=why;return GL_TRUE;}
 n=VB->Count;verts=c->verts;clip=(GLfloat(*)[4])st->clip.start;
 st->snap2=vxgl_ssepass_snap2&&vxgl_ssepass_fuse&&vxgl_ssepass_mask; /* v10.497 */
 st->snapmode=st->snap2&&vxgl_gl_snap?(vxgl_flip(c)?2:1):0; /* v10.490: as render_start's vx_d3d_gl_snap */
 if(!st->snapmode)st->snap2=0;
#ifdef VK_SSE
 {const GLuint cull=vxgl_ssepass_defer&&st->lit?defer_cull(ctx):0;const int part=cull?1:3;VXT_BEGIN; /* .506: [3] */
  if(!vxgl_ssepass_outfast)mx=mx_enter();
  for(i=0;i<n;i+=4) {
   GLuint k=n-i<4?n-i:4;int r;
   if(k==4)r=sse_group(ctx,st,i,4,verts+i*st->vs,clip[i],st->clipmask+i,part);
   else {r=sse_group(ctx,st,i,k,st->tmpv,st->tmpc,st->tmpm,part);
    if(r>=0){GLuint v;for(v=0;v<k;v++){memcpy(verts+(i+v)*st->vs,st->tmpv+v*st->vs,st->vs);memcpy(clip[i+v],st->tmpc+4*v,16);st->clipmask[i+v]=st->tmpm[v];}}}
   {int ref=r<0; /* v10.497: a group done again by the reference: its grid words from the store as before */
   if(r<0){ref_group(ctx,st,i,k,verts);r=0;{GLuint v;for(v=0;v<k;v++)r|=st->clipmask[i+v];}}
   {GLuint v;for(v=0;v<k;v++){GLubyte m=st->clipmask[i+v];ormask|=m;andmask&=m;}}
   if(st->snapmode&&(!st->snap2||ref))snap_group(st,verts+i*st->vs,i,k);} /* v10.490 */
  }
  /* .503 (SSEDEFER): the lighting for the groups a drawn triangle uses; a group the reference does again (its positions too) and the groups
   * left unlit lit after all (the reference's positions may draw a triangle the pass's culled), the masks gathered again */
  if(cull){GLuint pass,ref_seen=0,v;
   if(st->need_n<n){_mesa_free(st->need);st->need=(GLubyte*)_mesa_malloc(n+64);st->need_n=st->need?n+64:0;}
   if(st->need)defer_mark(ctx,st,verts,st->need,n,cull);
   vxgl_ssepass_defer_stat[0]++;
   for(pass=0;pass<2;pass++){
    for(i=0;i<n;i+=4){GLuint k=n-i<4?n-i:4,w=0;int r;
     if(st->need)for(v=0;v<k;v++)w|=st->need[i+v];else w=1;
     if(pass==0?!w:w){if(pass==0)vxgl_ssepass_defer_stat[2]++;continue;}
     if(pass==0)vxgl_ssepass_defer_stat[1]++;else vxgl_ssepass_defer_stat[2]--,vxgl_ssepass_defer_stat[1]++;
     if(k==4)r=sse_group(ctx,st,i,4,verts+i*st->vs,clip[i],st->clipmask+i,2);
     else {r=sse_group(ctx,st,i,k,st->tmpv,st->tmpc,st->tmpm,2);
      if(r>=0)for(v=0;v<k;v++)memcpy(verts+(i+v)*st->vs,st->tmpv+v*st->vs,st->vs);}
     if(r<0){ref_group(ctx,st,i,k,verts);ref_seen=1;if(st->snapmode)snap_group(st,verts+i*st->vs,i,k);}}
    if(!ref_seen)break;}
   if(ref_seen){ormask=0;andmask=CLIP_FRUSTUM_BITS;for(i=0;i<n;i++){ormask|=st->clipmask[i];andmask&=st->clipmask[i];}}}
  mx_leave(mx);VXT_END(3);}
#else
 ref_group(ctx,st,0,n,verts);for(i=0;i<n;i++){ormask|=st->clipmask[i];andmask&=st->clipmask[i];}
 st->snapmode=0;
#endif
 vxgl_ssepass_stat[0]++;vxgl_ssepass_stat[1]+=n;if(st->lit)vxgl_ssepass_stat[7]++;if(st->so==&st->imm){vxgl_ssepass_stat[8]++;vxgl_ssepass_stat[9]+=n;}
 if(st->colmat){ /* the state after the draw as Mesa's update_materials leaves it: the last vertex's colour in the tracking materials */
  GLuint a,bm=ctx->Light.ColorMaterialBitmask;GLfloat col[4];const vx_soa *so=st->so;col[0]=so->r[n-1];col[1]=so->g[n-1];col[2]=so->b[n-1];col[3]=so->a[n-1];
  for(a=0;a<MAT_ATTRIB_MAX;a++)if(bm&(1U<<a))COPY_4FV(ctx->Light.Material.Attrib[a],col);
  _mesa_update_material(ctx,bm);_mesa_validate_all_lighting_tables(ctx);st->lk_valid=0;} /* v10.492: the derived values now the last colour's: the next draw sets up again */
 st->clip.size=4;st->clip.count=n;st->clip.stride=4*sizeof(GLfloat);st->clip.flags|=VEC_SIZE_4; /* (the storage's VEC_MALLOC kept) */
 VB->ClipPtr=&st->clip;VB->NdcPtr=&st->clip;VB->ClipMask=st->clipmask;VB->ClipOrMask=(GLubyte)ormask;VB->ClipAndMask=(GLubyte)andmask;
 if(andmask){vxgl_ssepass_stat[5]++;return GL_FALSE;} /* every vertex outside one plane: nothing to draw (as the vertex stage) */
 {tnl_setup_func build=tnl->Driver.Render.BuildVertices;
  tnl->Driver.Render.BuildVertices=nobuild; /* the store is filled already */
  if(st->snapmode)vx_d3d_gl_presnap(st->snapbuf,n,(GLuint)st->snapmode,verts); /* v10.490: for this draw's strips only */
  _tnl_render_stage.run(ctx,(struct tnl_pipeline_stage*)&_tnl_render_stage);
  if(st->snapmode)vx_d3d_gl_presnap(0,0,0,0);
  tnl->Driver.Render.BuildVertices=build;}
 return GL_FALSE;
}

const struct tnl_pipeline_stage vxgl_ssepass_stage={"vxgl SSE pass (display lists)",NULL,sp_alloc,sp_free,NULL,sp_run};

/* VXFLAT: VXGL_SsePassStats, the eight counts (cleared when clear is set); returns ACCEL_OPENGL_SSEPASS */
uint32_t __stdcall VXGL_SsePassStats(uint32_t *o,uint32_t clear) {int k;if(o)for(k=0;k<8;k++)o[k]=(uint32_t)vxgl_ssepass_stat[k];if(clear){for(k=0;k<20;k++)vxgl_ssepass_stat[k]=0;vx_d3d_ps_used=vx_d3d_cullrun_tris=0;}return (uint32_t)(vxgl_ssepass_on!=0);}
/* v10.488: the ten counts ([8] draws not from a list, [9] their vertices); returns the switches (1 the pass, 2 unlit draws, 4 draws not from a list) */
uint32_t __stdcall VXGL_SsePassStats10(uint32_t *o,uint32_t clear) {int k;if(o)for(k=0;k<10;k++)o[k]=(uint32_t)vxgl_ssepass_stat[k];if(clear){for(k=0;k<20;k++)vxgl_ssepass_stat[k]=0;vx_d3d_ps_used=vx_d3d_cullrun_tris=0;}return (uint32_t)vxgl_ssepass_on;}
/* v10.490: the sixteen counts ([10] draws whole outside by the box, [11] vertices the strip path took snapped by the pass, [12] triangles by
 * its culling run, [13] specular lanes by SSE, [14] by _mesa_pow); returns the switches: 1 the pass, 2 unlit, 4 not from a list, 8 SSEBOX,
 * 16 SSESHINE, 32 SSEPACK, 64 SSESNAP, 128 CULLRUN */
uint32_t __stdcall VXGL_SsePassStats16(uint32_t *o,uint32_t clear) {extern uint32_t vx_d3d_gl_cullrun,vx_d3d_gl_cullrun2;extern int _tnl_vx855_fastbind;int k;
 vxgl_ssepass_stat[11]=vx_d3d_ps_used;vxgl_ssepass_stat[12]=vx_d3d_cullrun_tris;
 if(o)for(k=0;k<16;k++)o[k]=(uint32_t)vxgl_ssepass_stat[k];
 if(clear){for(k=0;k<20;k++)vxgl_ssepass_stat[k]=0;vx_d3d_ps_used=vx_d3d_cullrun_tris=0;}
 return (uint32_t)vxgl_ssepass_on|(vxgl_ssepass_box?8U:0)|(vxgl_ssepass_shine?16U:0)|(vxgl_ssepass_pack?32U:0)|(vx_d3d_gl_cullrun?128U:0)|(vxgl_ssepass_cache?256U:0)|(vxgl_ssepass_mask?512U:0)|(vxgl_ssepass_tight?1024U:0)|(vx_d3d_gl_cullrun2?2048U:0)|(_tnl_vx855_fastbind?4096U:0);} /* v10.492: 256 SSECACHE, 512 SSEMASK; v10.494: 2048 CULLRUN2, 4096 FASTBIND */
/* v10.495: the twenty counts ([16] list draws the box found inside every plane (SSEACCEPT), [17] groups of four by the fused kernels (SSEFUSE),
 * [18] lit set-ups made only for the tracked material (SSEPARTIAL), [19] list draws bound by the short path (LISTBIND)); returns VXGL_SsePassStats16's
 * switches and 8192 SSEFUSE, 16384 SSEACCEPT, 32768 SSEBOX2, 65536 SSEPARTIAL, 131072 LISTBIND */
uint32_t __stdcall VXGL_SsePassStats20(uint32_t *o,uint32_t clear) {extern int _vbo_vx855_listbind;extern unsigned long _vbo_vx855_listbind_n;int k;uint32_t sw=VXGL_SsePassStats16(0,0);
 vxgl_ssepass_stat[11]=vx_d3d_ps_used;vxgl_ssepass_stat[12]=vx_d3d_cullrun_tris;vxgl_ssepass_stat[19]=_vbo_vx855_listbind_n;
 if(o)for(k=0;k<20;k++)o[k]=(uint32_t)vxgl_ssepass_stat[k];
 if(clear){for(k=0;k<20;k++)vxgl_ssepass_stat[k]=0;vx_d3d_ps_used=vx_d3d_cullrun_tris=0;_vbo_vx855_listbind_n=0;}
 return sw|(vxgl_ssepass_fuse?8192U:0)|(vxgl_ssepass_accept?16384U:0)|(vxgl_ssepass_box2?32768U:0)|(vxgl_ssepass_partial?65536U:0)|(_vbo_vx855_listbind?131072U:0)|(vxgl_ssepass_immbox?262144U:0)|(vxgl_ssepass_snap2?524288U:0)|(vxgl_ssepass_outfast?1048576U:0)|(vxgl_ssepass_early?2097152U:0);} /* v10.498: 4194304 ALIGNHEAP (removed .506) */ /* v10.496: 262144 SSEIMMBOX; v10.497: 524288 SSESNAP2, 1048576 SSEOUTFAST, 2097152 SSEEARLY */
