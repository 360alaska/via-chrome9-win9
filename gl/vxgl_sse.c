/* v10.359: SSE vertex transform, clip test and perspective divide for Mesa's software T&L (VXGL 3.65).
 *
 * Mesa's tnl vertex stage (t_vb_vertex.c) calls _mesa_transform_tab[size][matrix type] for the model-view-projection (and the model-view
 * for eye coordinates when lighting or fog needs them), then _mesa_clip_tab[4] / _mesa_clip_np_tab[4] for the clip mask and the divide.
 * These replace the entries a game uses most: points 2/3/4 by a general matrix, points 3 by a 3D matrix (size 3 out), and the size-4 clip
 * tests. Everything else stays Mesa's C code.
 *
 * Results follow Mesa's arithmetic order, each operation rounded to single precision:
 *   x' = ((m0*x + m4*y) + m8*z) + m12   (m12*w for points 4); the clip mask from compares (w < c, w < -c), which give the same bits as
 *   Mesa's (-c + w < 0, c + w < 0) for every value (an IEEE difference is negative exactly when the first operand is smaller; NaN gives no
 *   bit either way); oow = 1/w by a true divide (divss, not the approximate rcpps), then c*oow.
 * The C code under x87 keeps the intermediate sums at the FPU's precision and rounds once at the store, so a result can differ from it in
 * the last bit of the float: never in the clip mask.
 *
 * The thread's MXCSR is saved, set to 0x1F80 (round to nearest, every exception masked, no flush-to-zero, no denormals-are-zero) for the
 * call and put back after it: a program's own SSE settings are not changed and do not change these results.
 * Used only when the processor has SSE and FXSR and the system has turned on OSFXSR (CR4 bit 9, read by the VxD: control 0xFFFFFE34), and
 * ACCEL_OPENGL_SSE is not 0. Only SSE1 instructions. */
#include "glheader.h"
#include "mtypes.h"
#include "math/m_xform.h"
#include "math/m_matrix.h"
#include "math/m_vector.h"

int vxgl_sse_active=0; /* 1 once the kernels are in Mesa's tables */
unsigned long vxgl_sse_calls[3]; /* transforms, clip tests (with divide), clip tests without */

static unsigned mx_enter(void);
#pragma aux mx_enter = "sub esp,8" "stmxcsr [esp]" "mov dword ptr [esp+4],1F80h" "ldmxcsr [esp+4]" "mov eax,[esp]" "add esp,8" value [eax] modify exact [eax];
static void mx_leave(unsigned old);
#pragma aux mx_leave = "push eax" "ldmxcsr [esp]" "add esp,4" parm [eax] modify exact [];

/* esi from, edi to (16-byte elements), ecx count, edx stride, eax matrix */
static void xf4_loop(const GLfloat *from,GLfloat *to,GLuint n,GLuint stride,const GLfloat *m);
#pragma aux xf4_loop = "movups xmm4,[eax]" "movups xmm5,[eax+16]" "movups xmm6,[eax+32]" "movups xmm7,[eax+48]" \
 "test ecx,ecx" "jz L2" \
 "L1: movss xmm0,[esi]" "shufps xmm0,xmm0,0" "mulps xmm0,xmm4" \
 "movss xmm1,[esi+4]" "shufps xmm1,xmm1,0" "mulps xmm1,xmm5" "addps xmm0,xmm1" \
 "movss xmm1,[esi+8]" "shufps xmm1,xmm1,0" "mulps xmm1,xmm6" "addps xmm0,xmm1" \
 "movss xmm1,[esi+12]" "shufps xmm1,xmm1,0" "mulps xmm1,xmm7" "addps xmm0,xmm1" \
 "movups [edi],xmm0" "add esi,edx" "add edi,16" "dec ecx" "jnz L1" "L2:" \
 parm [esi] [edi] [ecx] [edx] [eax] modify exact [esi edi ecx];
static void xf3_loop(const GLfloat *from,GLfloat *to,GLuint n,GLuint stride,const GLfloat *m);
#pragma aux xf3_loop = "movups xmm4,[eax]" "movups xmm5,[eax+16]" "movups xmm6,[eax+32]" "movups xmm7,[eax+48]" \
 "test ecx,ecx" "jz L2" \
 "L1: movss xmm0,[esi]" "shufps xmm0,xmm0,0" "mulps xmm0,xmm4" \
 "movss xmm1,[esi+4]" "shufps xmm1,xmm1,0" "mulps xmm1,xmm5" "addps xmm0,xmm1" \
 "movss xmm1,[esi+8]" "shufps xmm1,xmm1,0" "mulps xmm1,xmm6" "addps xmm0,xmm1" \
 "addps xmm0,xmm7" \
 "movups [edi],xmm0" "add esi,edx" "add edi,16" "dec ecx" "jnz L1" "L2:" \
 parm [esi] [edi] [ecx] [edx] [eax] modify exact [esi edi ecx];
/* points 3 by a 3D matrix: x, y, z only (Mesa's transform_points3_3d leaves the fourth float as it was) */
static void xf3d_loop(const GLfloat *from,GLfloat *to,GLuint n,GLuint stride,const GLfloat *m);
#pragma aux xf3d_loop = "movups xmm4,[eax]" "movups xmm5,[eax+16]" "movups xmm6,[eax+32]" "movups xmm7,[eax+48]" \
 "test ecx,ecx" "jz L2" \
 "L1: movss xmm0,[esi]" "shufps xmm0,xmm0,0" "mulps xmm0,xmm4" \
 "movss xmm1,[esi+4]" "shufps xmm1,xmm1,0" "mulps xmm1,xmm5" "addps xmm0,xmm1" \
 "movss xmm1,[esi+8]" "shufps xmm1,xmm1,0" "mulps xmm1,xmm6" "addps xmm0,xmm1" \
 "addps xmm0,xmm7" \
 "movlps [edi],xmm0" "movhlps xmm1,xmm0" "movss [edi+8],xmm1" "add esi,edx" "add edi,16" "dec ecx" "jnz L1" "L2:" \
 parm [esi] [edi] [ecx] [edx] [eax] modify exact [esi edi ecx];
static void xf2_loop(const GLfloat *from,GLfloat *to,GLuint n,GLuint stride,const GLfloat *m);
#pragma aux xf2_loop = "movups xmm4,[eax]" "movups xmm5,[eax+16]" "movups xmm7,[eax+48]" \
 "test ecx,ecx" "jz L2" \
 "L1: movss xmm0,[esi]" "shufps xmm0,xmm0,0" "mulps xmm0,xmm4" \
 "movss xmm1,[esi+4]" "shufps xmm1,xmm1,0" "mulps xmm1,xmm5" "addps xmm0,xmm1" \
 "addps xmm0,xmm7" \
 "movups [edi],xmm0" "add esi,edx" "add edi,16" "dec ecx" "jnz L1" "L2:" \
 parm [esi] [edi] [ecx] [edx] [eax] modify exact [esi edi ecx];

/* One clip vertex: esi the clip coordinates, edi the projected output, edx the constants (sign bits x4, then 1.0). Returns the compare bits:
 * 0-2 w < x, y, z (right, top, far); 4-6 w < -x, -y, -z (left, bottom, near). With none set the projected vertex is written. */
static unsigned clip1(const GLfloat *c,GLfloat *proj,const GLfloat *k);
#pragma aux clip1 = "movups xmm0,[esi]" "movaps xmm1,xmm0" "shufps xmm1,xmm1,0FFh" \
 "movaps xmm2,xmm1" "cmpltps xmm2,xmm0" "movmskps eax,xmm2" \
 "movups xmm3,[edx]" "xorps xmm3,xmm0" "movaps xmm2,xmm1" "cmpltps xmm2,xmm3" "movmskps ecx,xmm2" \
 "shl ecx,4" "or eax,ecx" "and eax,77h" "jnz L1" \
 "movss xmm2,[edx+16]" "divss xmm2,xmm1" "shufps xmm2,xmm2,0" "mulps xmm0,xmm2" "movups [edi],xmm0" "movss [edi+12],xmm2" \
 "L1:" parm [esi] [edi] [edx] value [eax] modify exact [eax ecx];
/* the same compares without the divide */
static unsigned clip1np(const GLfloat *c,const GLfloat *k);
#pragma aux clip1np = "movups xmm0,[esi]" "movaps xmm1,xmm0" "shufps xmm1,xmm1,0FFh" \
 "movaps xmm2,xmm1" "cmpltps xmm2,xmm0" "movmskps eax,xmm2" \
 "movups xmm3,[edx]" "xorps xmm3,xmm0" "movaps xmm2,xmm1" "cmpltps xmm2,xmm3" "movmskps ecx,xmm2" \
 "shl ecx,4" "or eax,ecx" "and eax,77h" parm [esi] [edx] value [eax] modify exact [eax ecx];

/* .506: ACCEL_OPENGL_CLIPINT (v10.485, the compares in integer registers; off since v10.486, 2-7% slower on the board) removed */
static const unsigned long kconst[5]={0x80000000UL,0x80000000UL,0x80000000UL,0x80000000UL,0x3F800000UL};
static const GLubyte bits_hi[8]={0,CLIP_RIGHT_BIT,CLIP_TOP_BIT,CLIP_RIGHT_BIT|CLIP_TOP_BIT,CLIP_FAR_BIT,CLIP_RIGHT_BIT|CLIP_FAR_BIT,
 CLIP_TOP_BIT|CLIP_FAR_BIT,CLIP_RIGHT_BIT|CLIP_TOP_BIT|CLIP_FAR_BIT};
static const GLubyte bits_lo[8]={0,CLIP_LEFT_BIT,CLIP_BOTTOM_BIT,CLIP_LEFT_BIT|CLIP_BOTTOM_BIT,CLIP_NEAR_BIT,CLIP_LEFT_BIT|CLIP_NEAR_BIT,
 CLIP_BOTTOM_BIT|CLIP_NEAR_BIT,CLIP_LEFT_BIT|CLIP_BOTTOM_BIT|CLIP_NEAR_BIT};

static void out4(GLvector4f *to_vec,const GLvector4f *from_vec) {to_vec->size=4;to_vec->flags|=VEC_SIZE_4;to_vec->count=from_vec->count;}

void vxgl_sse_points4_general(GLvector4f *to_vec,const GLfloat m[16],const GLvector4f *from_vec) {
 unsigned o=mx_enter();xf4_loop(from_vec->start,(GLfloat *)to_vec->start,from_vec->count,from_vec->stride,m);mx_leave(o);
 out4(to_vec,from_vec);vxgl_sse_calls[0]++;
}
void vxgl_sse_points3_general(GLvector4f *to_vec,const GLfloat m[16],const GLvector4f *from_vec) {
 unsigned o=mx_enter();xf3_loop(from_vec->start,(GLfloat *)to_vec->start,from_vec->count,from_vec->stride,m);mx_leave(o);
 out4(to_vec,from_vec);vxgl_sse_calls[0]++;
}
void vxgl_sse_points2_general(GLvector4f *to_vec,const GLfloat m[16],const GLvector4f *from_vec) {
 unsigned o=mx_enter();xf2_loop(from_vec->start,(GLfloat *)to_vec->start,from_vec->count,from_vec->stride,m);mx_leave(o);
 out4(to_vec,from_vec);vxgl_sse_calls[0]++;
}
void vxgl_sse_points3_3d(GLvector4f *to_vec,const GLfloat m[16],const GLvector4f *from_vec) {
 unsigned o=mx_enter();xf3d_loop(from_vec->start,(GLfloat *)to_vec->start,from_vec->count,from_vec->stride,m);mx_leave(o);
 to_vec->size=3;to_vec->flags|=VEC_SIZE_3;to_vec->count=from_vec->count;vxgl_sse_calls[0]++;
}

GLvector4f *vxgl_sse_cliptest4(GLvector4f *clip_vec,GLvector4f *proj_vec,GLubyte clipMask[],GLubyte *orMask,GLubyte *andMask) {
 const GLuint stride=clip_vec->stride,count=clip_vec->count;
 const GLubyte *from=(const GLubyte *)clip_vec->start;GLfloat (*vProj)[4]=(GLfloat (*)[4])proj_vec->start;
 GLubyte tmpAnd=*andMask,tmpOr=*orMask;GLuint i,c=0;unsigned o=mx_enter();
 for(i=0;i<count;i++,from+=stride) {
  unsigned r;GLubyte mask;
  r=clip1((const GLfloat *)from,vProj[i],(const GLfloat *)kconst);
  mask=(GLubyte)(bits_hi[r&7]|bits_lo[(r>>4)&7]);
  clipMask[i]=mask;
  if(mask){c++;tmpAnd&=mask;tmpOr|=mask;vProj[i][0]=0;vProj[i][1]=0;vProj[i][2]=0;vProj[i][3]=1;}
 }
 mx_leave(o);
 *orMask=tmpOr;*andMask=(GLubyte)(c<count?0:tmpAnd);
 proj_vec->flags|=VEC_SIZE_4;proj_vec->size=4;proj_vec->count=clip_vec->count;vxgl_sse_calls[1]++;
 return proj_vec;
}
GLvector4f *vxgl_sse_cliptest_np4(GLvector4f *clip_vec,GLvector4f *proj_vec,GLubyte clipMask[],GLubyte *orMask,GLubyte *andMask) {
 const GLuint stride=clip_vec->stride,count=clip_vec->count;const GLubyte *from=(const GLubyte *)clip_vec->start;
 GLubyte tmpAnd=*andMask,tmpOr=*orMask;GLuint i,c=0;unsigned o=mx_enter();
 (void)proj_vec;
 for(i=0;i<count;i++,from+=stride) {
  unsigned r;GLubyte mask;
  r=clip1np((const GLfloat *)from,(const GLfloat *)kconst);
  mask=(GLubyte)(bits_hi[r&7]|bits_lo[(r>>4)&7]);
  clipMask[i]=mask;if(mask){c++;tmpAnd&=mask;tmpOr|=mask;}
 }
 mx_leave(o);
 *orMask=tmpOr;*andMask=(GLubyte)(c<count?0:tmpAnd);vxgl_sse_calls[2]++;
 return clip_vec;
}

/* Into Mesa's tables, once per process, after Mesa's one-time set-up (_mesa_initialize_context has filled them). */
void vxgl_sse_install(void) {
 if(vxgl_sse_active)return;
 _mesa_transform_tab[2][MATRIX_GENERAL]=vxgl_sse_points2_general;
 _mesa_transform_tab[3][MATRIX_GENERAL]=vxgl_sse_points3_general;
 _mesa_transform_tab[4][MATRIX_GENERAL]=vxgl_sse_points4_general;
 _mesa_transform_tab[3][MATRIX_3D]=vxgl_sse_points3_3d;
 _mesa_clip_tab[4]=vxgl_sse_cliptest4;
 _mesa_clip_np_tab[4]=vxgl_sse_cliptest_np4;
 vxgl_sse_active=1;
}
