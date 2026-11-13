/* VXGL.DLL v10.321: fragment programs (GL_ARB_fragment_program, and GLSL fragment shaders, which Mesa compiles to the same instructions)
 * on the engine (ACCEL_OPENGL_FPROG = 0: software). The engine's colour path is the core's texture stages (Direct3D's: up to three, an op
 * and two arguments each for colour and for alpha, results clamped to 0..1, one constant colour TFACTOR), which the core turns into the
 * XP ALU's instructions; so a program is drawn by the engine when it computes what some cascade of stages computes:
 *  - the program is run symbolically (each register channel a polynomial in the fragment's colours, the texels and clamped
 *    sub-expressions, with the constants folded in: local and env parameters, state, uniforms, literals), as swrast runs it
 *    (prog_execute.c: unclamped floats, _SAT clamping); an instruction that is not a polynomial of those (KIL, TXB, RCP of a varying, a
 *    depth or second output, a texture read at other coordinates than its unit's own set) leaves the program in software;
 *  - the result colour is then matched, last stage first, against each stage op with its arguments (the stage's texture, the diffuse
 *    and specular colours, TFACTOR, complemented or alpha-replicated, and CURRENT: the stages before, solved for and matched in turn);
 *    TFACTOR's channels are solved for where they appear, once each;
 *  - an intermediate stage result must stay within 0..1 for every input (the engine clamps each stage; a program does not), or be the
 *    clamp the program asked for (_SAT);
 *  - the textured stages are the units the program reads, in unit order, as the fixed-function path lays them out (vxgl_tex.c), and
 *    up to three stages in all.
 * The texel of each base format as swrast returns it (LUMINANCE l l l 1, ALPHA 0 0 0 a, INTENSITY i i i i, ...). Every match is an
 * identity of polynomials, so the engine computes the program's values (to its 8-bit precision). */
#include "vxgl.h"
#include "shader/prog_instruction.h"
#include "shader/prog_statevars.h"
#include "shader/prog_parameter.h"
#include "shader/atifragshader.h"
#include "colormac.h"
#include "macros.h"

int vxgl_gl_fprog=1; /* ACCEL_OPENGL_FPROG */

/* ---- polynomials in atoms (all within 0..1): the diffuse colour 1-4, the specular 5-7, texture i channel c 8 + 4i + c, a clamp 32 + j */
#define FP_NT 20 /* terms */
#define FP_ND 4  /* atoms a term */
#define AT_D 1
#define AT_S 5
#define AT_T 8
#define AT_CL 32
#define AT_X 31 /* the specular colour's alpha (the engine's holds the fog factor) */
#define FP_NCL 48
typedef struct {unsigned char n,a[FP_ND];float k;} fp_term;
typedef struct {int nt;fp_term t[FP_NT];} fp_poly;
static int fp_bad; /* an overflow or something not drawable: the program stays in software */
static fp_poly cl_in[FP_NCL];static int cl_n; /* the clamps' insides */

static int term_cmp(const fp_term *x,const fp_term *y) {int i;if(x->n!=y->n)return x->n<y->n?-1:1;for(i=0;i<x->n;i++)if(x->a[i]!=y->a[i])return x->a[i]<y->a[i]?-1:1;return 0;}
/* v10.328 (review): a term is dropped only when it is exactly 0. v10.322 dropped like terms that summed under 1e-6 of the largest (an
 * exact small difference: SUB 1 - 0.99999994 then MUL by 1e7 black); v10.326 dropped a sum within a bound on its rounding error, but
 * swrast computes the same float rounding: 0.1 added ten times, minus 1, times 1e6 is 0.119 in swrast (30 red) and was black. Every
 * nonzero term is kept, as the program computes it; a cancellation's residue that stays small is taken as 0 where it is matched
 * (p_eq), and one that a later instruction scales up is the program's own value. */
/* v10.329 (review: 1e30 * 1e30 drew black on the engine, 255 red in swrast): a coefficient that overflowed or is not a number leaves the
 * program in software, where swrast computes it as the program says; every operation that makes coefficients ends in p_norm or here */
static int k_fin(float k) {uint32_t u;memcpy(&u,&k,4);return (u&0x7F800000UL)!=0x7F800000UL;} /* the exponent all ones: an infinity or a NaN */
static void p_fin(const fp_poly *p) {int i;for(i=0;i<p->nt;i++)if(!k_fin(p->t[i].k)){fp_bad=1;return;}}
static void p_norm(fp_poly *p) {
 int i,j,n=0;
 for(i=1;i<p->nt;i++){fp_term t=p->t[i];j=i-1;while(j>=0&&term_cmp(&p->t[j],&t)>0){p->t[j+1]=p->t[j];j--;}p->t[j+1]=t;} /* sorted */
 for(i=0;i<p->nt;i++){if(n&&!term_cmp(&p->t[n-1],&p->t[i]))p->t[n-1].k+=p->t[i].k;else p->t[n++]=p->t[i];}
 for(i=j=0;i<n;i++)if(p->t[i].k!=0.0F)p->t[j++]=p->t[i];
 p->nt=j;
 p_fin(p);
}
static void p_const(fp_poly *p,float k) {p->nt=0;if(!k_fin(k)){fp_bad=1;return;}if(k!=0.0F){p->nt=1;p->t[0].n=0;p->t[0].k=k;}}
static void p_atom(fp_poly *p,int a) {p->nt=1;p->t[0].n=1;p->t[0].a[0]=(unsigned char)a;p->t[0].k=1.0F;}
static void p_addto(fp_poly *r,const fp_poly *a,float s) { /* r += s a */
 int i;
 for(i=0;i<a->nt;i++){if(r->nt>=FP_NT){p_norm(r);if(r->nt>=FP_NT){fp_bad=1;return;}}r->t[r->nt]=a->t[i];r->t[r->nt].k*=s;r->nt++;}
 p_norm(r);
}
static void p_lin(fp_poly *r,const fp_poly *a,float sa,const fp_poly *b,float sb) {fp_poly t;t.nt=0;p_addto(&t,a,sa);p_addto(&t,b,sb);*r=t;}
static void p_addk(fp_poly *r,float k) {fp_poly c;p_const(&c,k);p_addto(r,&c,1.0F);}
static void p_mul(fp_poly *r,const fp_poly *a,const fp_poly *b) {
 fp_poly t;int i,j,m,x,y;t.nt=0;
 for(i=0;i<a->nt;i++)for(j=0;j<b->nt;j++) {
  fp_term u;const fp_term *p=&a->t[i],*q=&b->t[j];
  if(p->n+q->n>FP_ND){fp_bad=1;return;}
  if(t.nt>=FP_NT){p_norm(&t);if(t.nt>=FP_NT){fp_bad=1;return;}}
  for(m=x=y=0;x<p->n||y<q->n;m++)u.a[m]=(y>=q->n||(x<p->n&&p->a[x]<=q->a[y]))?p->a[x++]:q->a[y++]; /* merged, sorted */
  u.n=(unsigned char)m;u.k=p->k*q->k;t.t[t.nt++]=u;
 }
 p_norm(&t);*r=t;
}
/* the same values to the engine's precision: like terms within 2e-4, a term on one side only within 2e-4 of 0 (v10.326: small terms
 * stay in the polynomials, so a value the program cancels to a float residue still matches a stage that gives 0 there) */
/* v10.334 (audit F03): and the differences together within half an 8-bit step (1/512): the atoms are 0..1, so the sum of the differences
 * bounds the error; nineteen terms each under 2e-4 passed while they added up to 0.0038 */
static int p_eq(const fp_poly *a,const fp_poly *b) {
 int i=0,j=0;float sum=0.0F;
 while(i<a->nt||j<b->nt){int c=i>=a->nt?1:j>=b->nt?-1:term_cmp(&a->t[i],&b->t[j]);float d=c<0?a->t[i++].k:c>0?-b->t[j++].k:a->t[i++].k-b->t[j++].k;if(d>2e-4F||d<-2e-4F)return 0;sum+=d<0.0F?-d:d;}
 return sum<=1.0F/512.0F;
}
static int p_isconst(const fp_poly *a,float *k) {if(!a->nt){*k=0.0F;return 1;}if(a->nt==1&&!a->t[0].n){*k=a->t[0].k;return 1;}return 0;}
static void p_range(const fp_poly *a,float *lo,float *hi) {int i;*lo=*hi=0.0F;for(i=0;i<a->nt;i++){float k=a->t[i].k;if(!a->t[i].n){*lo+=k;*hi+=k;}else if(k>0.0F)*hi+=k;else *lo+=k;}}
/* v10.329 (review: MUL_SAT by 1.000005, minus the colour, times 1e5 drew 128 red on the engine, black in swrast): a clamp is left out only
 * when the value is within 0..1 exactly; a value 1e-5 over was taken as within, and a later instruction scaled the difference up.
 * A stage's result (p_unit_stage) keeps the tolerance: the engine clamps it, and only the stages after it (at most 4x each) see that */
static int p_unit(const fp_poly *a) {float lo,hi;p_range(a,&lo,&hi);return lo>=0.0F&&hi<=1.0F;}
static int p_unit_stage(const fp_poly *a) {float lo,hi;p_range(a,&lo,&hi);return lo>=-1e-5F&&hi<=1.0F+1e-5F;}
/* r = a / d, d a constant, a constant times an atom, or a constant times (1 - an atom); 0 when it does not divide */
static int p_div(fp_poly *r,const fp_poly *a,const fp_poly *d) {
 float k;int i,j;
 if(p_isconst(d,&k)){if(k>-1e-6F&&k<1e-6F)return 0;*r=*a;for(i=0;i<r->nt;i++)r->t[i].k/=k;p_fin(r);return 1;}
 if(d->nt==1&&d->t[0].n==1) { /* k x */
  int x=d->t[0].a[0];fp_poly t=*a;k=d->t[0].k;
  for(i=0;i<t.nt;i++){for(j=0;j<t.t[i].n&&t.t[i].a[j]!=x;j++);if(j==t.t[i].n)return 0;
   for(;j+1<t.t[i].n;j++)t.t[i].a[j]=t.t[i].a[j+1];t.t[i].n--;t.t[i].k/=k;}
  p_norm(&t);*r=t;return 1;
 }
 if(d->nt==2&&!d->t[0].n&&d->t[1].n==1&&d->t[0].k+d->t[1].k>-1e-6F&&d->t[0].k+d->t[1].k<1e-6F) { /* k (1 - x) */
  /* a = sum x^e P_e (P_e free of x, e <= n) = (1 - x) sum x^e Q_e: Q_e = P_e + Q_(e-1), and P_n + Q_(n-1) = 0 */
  int x=d->t[1].a[0],e,deg=0;static fp_poly part[FP_ND+1];fp_poly q,acc;k=d->t[0].k;
  for(e=0;e<=FP_ND;e++)part[e].nt=0;
  for(i=0;i<a->nt;i++){fp_term u=a->t[i];int m=0;for(j=e=0;j<u.n;j++)if(u.a[j]==x)e++;else u.a[m++]=u.a[j];u.n=(unsigned char)m;part[e].t[part[e].nt++]=u;if(e>deg)deg=e;}
  if(!deg)return 0; /* free of x: no polynomial quotient */
  r->nt=0;acc.nt=0;
  for(e=0;e<deg;e++) {
   p_norm(&part[e]);p_lin(&q,&part[e],1.0F,&acc,1.0F);acc=q;
   for(i=0;i<q.nt;i++){fp_term u=q.t[i];int z,c;
    for(c=0;c<e;c++){if(u.n>=FP_ND){fp_bad=1;return 0;}for(z=u.n;z>0&&u.a[z-1]>x;z--)u.a[z]=u.a[z-1];u.a[z]=(unsigned char)x;u.n++;}
    if(r->nt>=FP_NT){fp_bad=1;return 0;}u.k/=k;r->t[r->nt++]=u;}
  }
  p_norm(&part[deg]);p_lin(&q,&part[deg],1.0F,&acc,1.0F);if(q.nt)return 0; /* a remainder */
  p_norm(r);return 1;
 }
 /* v10.340 (board, VXPROBE fpcache: LRP by a program.local parameter between the texture and the colour went to software): a constant
  * multiple of any d, as LERP's TFACTOR gives: E - y = f (x - y) with x - y two atoms, which none of the forms above divides */
 if(d->nt>=1&&a->nt>=1) {
  fp_poly t,z;
  for(i=0;i<a->nt&&term_cmp(&a->t[i],&d->t[0]);i++);
  if(i==a->nt||(d->t[0].k>-1e-6F&&d->t[0].k<1e-6F))return 0;
  k=a->t[i].k/d->t[0].k;p_lin(&t,a,1.0F,d,-k);z.nt=0;
  if(fp_bad||!p_eq(&t,&z))return 0;
  p_const(r,k);return 1;
 }
 return 0;
}
/* v10.328 (review): the same clamp only for the same polynomial exactly; p_eq's tolerance (v10.326) made clamp(2c) and clamp(2c + 0.0001)
 * one atom, and (b - a) * 1000 black where swrast draws 26 red at c = 0.25 */
static int p_same(const fp_poly *a,const fp_poly *b) {int i;if(a->nt!=b->nt)return 0;for(i=0;i<a->nt;i++)if(term_cmp(&a->t[i],&b->t[i])||a->t[i].k!=b->t[i].k)return 0;return 1;}
static int cl_atom(const fp_poly *p) { /* the clamp of p to 0..1 as an atom (one per distinct p) */
 int j;for(j=0;j<cl_n;j++)if(p_same(&cl_in[j],p))return AT_CL+j;
 if(cl_n>=FP_NCL){fp_bad=1;return AT_CL;}
 cl_in[cl_n]=*p;return AT_CL+cl_n++;
}
static void p_sat(fp_poly *p) {float k;if(p_unit(p))return;if(p_isconst(p,&k)){p_const(p,k<0.0F?0.0F:k>1.0F?1.0F:k);return;}p_atom(p,cl_atom(p));}

/* ---- the program, run symbolically */
#define FP_TEMPS 64
static fp_poly regs[FP_TEMPS][4],outc[4];
static fp_poly kil[4];static int kil_n; /* v10.353: the operand of the program's KIL (ARB's: a fragment is killed when any component is below 0) */
static int tex_of_unit[8],tex_base[3],tex_unit[3],tex_n,tex_txp[8],tex_tex[8]; /* the texture index of a unit, its base format, TXP / TEX used */
static const GLfloat *param_vec(GLcontext *ctx,const struct gl_program *prog,const struct prog_src_register *s) {
 switch(s->File){
 case PROGRAM_LOCAL_PARAM:return s->Index>=0&&s->Index<MAX_PROGRAM_LOCAL_PARAMS?prog->LocalParams[s->Index]:0;
 case PROGRAM_ENV_PARAM:return s->Index>=0&&s->Index<MAX_PROGRAM_ENV_PARAMS?ctx->FragmentProgram.Parameters[s->Index]:0;
 case PROGRAM_STATE_VAR:case PROGRAM_CONSTANT:case PROGRAM_UNIFORM:case PROGRAM_NAMED_PARAM:
  return prog->Parameters&&s->Index>=0&&s->Index<(GLint)prog->Parameters->NumParameters?prog->Parameters->ParameterValues[s->Index]:0;
 }
 return 0;
}
/* a source's four channels, swizzled, negated and |abs| as prog_execute.c fetch_vector4 does */
static void fetch(GLcontext *ctx,const struct gl_program *prog,const struct prog_src_register *s,fp_poly *o) {
 fp_poly v[4];int c;
 if(s->RelAddr){fp_bad=1;return;}
 if(s->File==PROGRAM_TEMPORARY){if(s->Index<0||s->Index>=FP_TEMPS){fp_bad=1;return;}for(c=0;c<4;c++)v[c]=regs[s->Index][c];}
 else if(s->File==PROGRAM_INPUT) {
  if(s->Index==FRAG_ATTRIB_COL0)for(c=0;c<4;c++)p_atom(&v[c],AT_D+c);
  else if(s->Index==FRAG_ATTRIB_COL1){for(c=0;c<3;c++)p_atom(&v[c],AT_S+c);p_atom(&v[3],AT_X);} /* its alpha: an atom no stage argument gives */
  else {fp_bad=1;return;} /* position, fog coordinate, texture coordinates other than a TEX's own, varyings */
 }
 else {const GLfloat *p=param_vec(ctx,prog,s);if(!p){fp_bad=1;return;}for(c=0;c<4;c++)p_const(&v[c],p[c]);}
 for(c=0;c<4;c++) {
  GLuint sw=GET_SWZ(s->Swizzle,c);
  if(sw==SWIZZLE_ZERO)p_const(&o[c],0.0F);else if(sw==SWIZZLE_ONE)p_const(&o[c],1.0F);else if(sw<4)o[c]=v[sw];else{fp_bad=1;return;}
  if(s->Abs){float k;if(!p_isconst(&o[c],&k)){float lo,hi;p_range(&o[c],&lo,&hi);if(lo<0.0F){fp_bad=1;return;}}else p_const(&o[c],k<0.0F?-k:k);
   if(s->NegateAbs){fp_poly z;z.nt=0;p_addto(&z,&o[c],-1.0F);o[c]=z;}}
  if(s->NegateBase&(1U<<c)){fp_poly z;z.nt=0;p_addto(&z,&o[c],-1.0F);o[c]=z;}
 }
}
/* a texel of texture index i as swrast returns it for the base format */
static void texel(int i,fp_poly *o) {
 int c,b=tex_base[i];
 for(c=0;c<4;c++)p_atom(&o[c],AT_T+4*i+c);
 switch(b){
 case GL_RGB:p_const(&o[3],1.0F);break;
 case GL_LUMINANCE:p_atom(&o[1],AT_T+4*i);p_atom(&o[2],AT_T+4*i);p_const(&o[3],1.0F);break;
 case GL_LUMINANCE_ALPHA:p_atom(&o[1],AT_T+4*i);p_atom(&o[2],AT_T+4*i);break;
 case GL_INTENSITY:for(c=1;c<4;c++)p_atom(&o[c],AT_T+4*i);break;
 case GL_ALPHA:for(c=0;c<3;c++)p_const(&o[c],0.0F);break;
 }
}
static float fconst(const fp_poly *p) {float k;if(!p_isconst(p,&k)){fp_bad=1;return 0.0F;}return k;}
/* the program into outc (the result colour); 0 = not drawable */
static int run(GLcontext *ctx,const struct gl_fragment_program *fp) {
 const struct gl_program *prog=&fp->Base;GLuint n;int c;
 for(n=0;n<FP_TEMPS;n++)for(c=0;c<4;c++)regs[n][c].nt=0;
 for(c=0;c<4;c++)outc[c].nt=0;
 kil_n=0;
 for(n=0;n<prog->NumInstructions&&!fp_bad;n++) {
  const struct prog_instruction *in=&prog->Instructions[n];fp_poly a[4],b[4],d[4],r[4];int k;float x;
  if(in->Opcode==OPCODE_END)break;
  if(in->Opcode==OPCODE_NOP)continue;
  if(in->CondUpdate||(in->DstReg.CondMask!=COND_TR&&in->DstReg.CondMask!=0)||in->SaturateMode==SATURATE_PLUS_MINUS_ONE){fp_bad=1;break;}
  switch(in->Opcode){
  case OPCODE_MOV:case OPCODE_SWZ:fetch(ctx,prog,&in->SrcReg[0],r);break;
  case OPCODE_ABS:fetch(ctx,prog,&in->SrcReg[0],r);for(c=0;c<4;c++){float lo,hi;if(p_isconst(&r[c],&x))p_const(&r[c],x<0?-x:x);else{p_range(&r[c],&lo,&hi);if(lo<0.0F){if(hi<=0.0F){fp_poly z;z.nt=0;p_addto(&z,&r[c],-1.0F);r[c]=z;}else fp_bad=1;}}}break;
  case OPCODE_ADD:case OPCODE_SUB:fetch(ctx,prog,&in->SrcReg[0],a);fetch(ctx,prog,&in->SrcReg[1],b);for(c=0;c<4;c++)p_lin(&r[c],&a[c],1.0F,&b[c],in->Opcode==OPCODE_SUB?-1.0F:1.0F);break;
  case OPCODE_MUL:fetch(ctx,prog,&in->SrcReg[0],a);fetch(ctx,prog,&in->SrcReg[1],b);for(c=0;c<4;c++)p_mul(&r[c],&a[c],&b[c]);break;
  case OPCODE_MAD:fetch(ctx,prog,&in->SrcReg[0],a);fetch(ctx,prog,&in->SrcReg[1],b);fetch(ctx,prog,&in->SrcReg[2],d);for(c=0;c<4;c++){p_mul(&r[c],&a[c],&b[c]);p_addto(&r[c],&d[c],1.0F);}break;
  case OPCODE_LRP:fetch(ctx,prog,&in->SrcReg[0],a);fetch(ctx,prog,&in->SrcReg[1],b);fetch(ctx,prog,&in->SrcReg[2],d); /* a b + (1 - a) c */
   for(c=0;c<4;c++){fp_poly t,u;p_mul(&t,&a[c],&b[c]);p_mul(&u,&a[c],&d[c]);p_lin(&r[c],&t,1.0F,&u,-1.0F);p_addto(&r[c],&d[c],1.0F);}break;
  case OPCODE_DP3:case OPCODE_DP4:case OPCODE_DPH:{fp_poly s,t;int m=in->Opcode==OPCODE_DP3?3:4;
   fetch(ctx,prog,&in->SrcReg[0],a);fetch(ctx,prog,&in->SrcReg[1],b);s.nt=0;
   if(in->Opcode==OPCODE_DPH)p_const(&a[3],1.0F);
   for(k=0;k<m;k++){p_mul(&t,&a[k],&b[k]);p_addto(&s,&t,1.0F);}
   for(c=0;c<4;c++)r[c]=s;break;}
  case OPCODE_XPD:fetch(ctx,prog,&in->SrcReg[0],a);fetch(ctx,prog,&in->SrcReg[1],b);
   for(c=0;c<3;c++){fp_poly t,u;int i1=(c+1)%3,i2=(c+2)%3;p_mul(&t,&a[i1],&b[i2]);p_mul(&u,&a[i2],&b[i1]);p_lin(&r[c],&t,1.0F,&u,-1.0F);}p_const(&r[3],1.0F);break;
  case OPCODE_MIN:case OPCODE_MAX:case OPCODE_SGE:case OPCODE_SLT:case OPCODE_CMP: /* constants only (folded) */
   fetch(ctx,prog,&in->SrcReg[0],a);fetch(ctx,prog,&in->SrcReg[1],b);if(in->Opcode==OPCODE_CMP)fetch(ctx,prog,&in->SrcReg[2],d);
   for(c=0;c<4;c++){float p=fconst(&a[c]),q;
    if(in->Opcode==OPCODE_CMP){r[c]=p<0.0F?b[c]:d[c];continue;}
    q=fconst(&b[c]);p_const(&r[c],in->Opcode==OPCODE_MIN?(p<q?p:q):in->Opcode==OPCODE_MAX?(p>q?p:q):in->Opcode==OPCODE_SGE?(p>=q?1.0F:0.0F):(p<q?1.0F:0.0F));}
   break;
  case OPCODE_FLR:case OPCODE_FRC: /* v10.334 (audit F01): vector operations, each component its own (constants only); they were taken with the
   * scalar group, X for all four (FLR (0.25, 1.5, -0.25, 3.75) gave 0 0 0 0 for 0 1 -1 3). Components the write mask leaves out are not read */
   fetch(ctx,prog,&in->SrcReg[0],a);
   for(c=0;c<4;c++){float p;if(!(in->DstReg.WriteMask&(1U<<c))){r[c].nt=0;continue;}p=fconst(&a[c]);p_const(&r[c],in->Opcode==OPCODE_FLR?(float)floor((double)p):p-(float)floor((double)p));}
   break;
  case OPCODE_RCP:case OPCODE_RSQ:case OPCODE_EX2:case OPCODE_LG2:case OPCODE_COS:case OPCODE_SIN:case OPCODE_POW:{float p,q=0.0F,v;
   fetch(ctx,prog,&in->SrcReg[0],a);p=fconst(&a[0]);if(in->Opcode==OPCODE_POW){fetch(ctx,prog,&in->SrcReg[1],b);q=fconst(&b[0]);}
   if(fp_bad)break;
   switch(in->Opcode){case OPCODE_RCP:if(p==0.0F){fp_bad=1;v=0;}else v=1.0F/p;break;case OPCODE_RSQ:if(p==0.0F){fp_bad=1;v=0;}else v=(float)(1.0/sqrt(fabs((double)p)));break;
    case OPCODE_EX2:v=(float)pow(2.0,(double)p);break;case OPCODE_LG2:if(p<=0.0F){fp_bad=1;v=0;}else v=(float)(log((double)p)/log(2.0));break;
    case OPCODE_COS:v=(float)cos((double)p);break;case OPCODE_SIN:v=(float)sin((double)p);break;
    default:if(p<0.0F||(p==0.0F&&q<=0.0F)){fp_bad=1;v=0;}else v=(float)pow((double)p,(double)q);}
   for(c=0;c<4;c++)p_const(&r[c],v);break;}
  case OPCODE_TEX:case OPCODE_TXP:{GLuint u=in->TexSrcUnit;const struct prog_src_register *s=&in->SrcReg[0];
   /* the unit's own coordinates as they come (swrast has their derivatives only then; the engine reads the stage's set) */
   int nco=in->TexSrcTarget==TEXTURE_1D_INDEX?1:(in->TexSrcTarget==TEXTURE_3D_INDEX||in->TexSrcTarget==TEXTURE_CUBE_INDEX)?3:2;
   if(u>=8||s->File!=PROGRAM_INPUT||s->Index!=(GLint)(FRAG_ATTRIB_TEX0+u)||s->NegateBase||s->Abs||s->RelAddr||tex_of_unit[u]<0){fp_bad=1;break;}
   for(k=0;k<nco;k++)if(GET_SWZ(s->Swizzle,k)!=(GLuint)k)fp_bad=1; /* the coordinates the target reads, unswizzled (GLSL's .xy) */
   if(in->Opcode==OPCODE_TXP&&GET_SWZ(s->Swizzle,3)!=3)fp_bad=1;
   if(fp_bad)break;
   if(in->Opcode==OPCODE_TXP)tex_txp[u]=1;else tex_tex[u]=1;
   texel(tex_of_unit[u],r);break;}
  case OPCODE_KIL: /* v10.353: one KIL, kept for the alpha test (vxgl_fp_compile); its operand as it is here */
   if(kil_n){fp_bad=1;break;}fetch(ctx,prog,&in->SrcReg[0],kil);kil_n=1;continue;
  default:fp_bad=1; /* TXB, TXD, DDX, DDY, LIT, DST, SCS, branches, packing, NV's condition codes (KIL_NV), a second KIL */
  }
  if(fp_bad)break;
  if(in->SaturateMode==SATURATE_ZERO_ONE)for(c=0;c<4;c++)p_sat(&r[c]);
  for(c=0;c<4;c++)if(in->DstReg.WriteMask&(1U<<c)) {
   if(in->DstReg.File==PROGRAM_TEMPORARY&&in->DstReg.Index<FP_TEMPS)regs[in->DstReg.Index][c]=r[c]; /* v10.330 (review #43): a negative index too goes to else (software) */
   else if(in->DstReg.File==PROGRAM_OUTPUT&&in->DstReg.Index==FRAG_RESULT_COLR)outc[c]=r[c];
   else fp_bad=1; /* depth, a second colour output, the address register */
  }
 }
 return !fp_bad;
}

/* ---- GL_ATI_fragment_shader, run symbolically as swrast runs it (s_atifragshader.c): six registers, one or two passes, a sample into
 * register j from unit j, argument replication and modifiers, destination scales, saturation or the clamp to -8..8. A sampled texel
 * is read at level 0 with the magnification filter there (fetch_texel's lambda 0: vxgl_tex.c takes that filter, unmipmapped) */
static void ati_arg(GLcontext *ctx,const struct ati_fragment_shader *sh,const struct atifragshader_src_register *r,int optype,fp_poly R[6][4],fp_poly *v) {
 GLuint x=r->Index;int c,start=optype?3:0;fp_poly t;
 if(x>=GL_REG_0_ATI&&x<=GL_REG_5_ATI)for(c=0;c<4;c++)v[c]=R[x-GL_REG_0_ATI][c];
 else if(x>=GL_CON_0_ATI&&x<=GL_CON_7_ATI){GLuint k=x-GL_CON_0_ATI;const GLfloat *f=(sh->LocalConstDef&(1U<<k))?sh->Constants[k]:ctx->ATIFragmentShader.GlobalConstants[k];for(c=0;c<4;c++)p_const(&v[c],f[c]);}
 else if(x==GL_ONE||x==GL_ZERO)for(c=0;c<4;c++)p_const(&v[c],x==GL_ONE?1.0F:0.0F);
 else if(x==GL_PRIMARY_COLOR_EXT)for(c=0;c<4;c++)p_atom(&v[c],AT_D+c);
 else if(x==GL_SECONDARY_INTERPOLATOR_ATI){for(c=0;c<3;c++)p_atom(&v[c],AT_S+c);p_atom(&v[3],AT_X);}
 else {fp_bad=1;return;}
 if(r->argRep){int k=r->argRep==GL_RED?0:r->argRep==GL_GREEN?1:r->argRep==GL_BLUE?2:r->argRep==GL_ALPHA?3:-1;if(k<0){fp_bad=1;return;}t=v[k];for(c=start;c<4;c++)v[c]=t;}
 for(c=start;c<4;c++) {
  if(r->argMod&GL_COMP_BIT_ATI){t.nt=0;p_addto(&t,&v[c],-1.0F);p_addk(&t,1.0F);v[c]=t;}
  if(r->argMod&GL_BIAS_BIT_ATI)p_addk(&v[c],-0.5F);
  if(r->argMod&GL_2X_BIT_ATI){t.nt=0;p_addto(&t,&v[c],2.0F);v[c]=t;}
  if(r->argMod&GL_NEGATE_BIT_ATI){t.nt=0;p_addto(&t,&v[c],-1.0F);v[c]=t;}
 }
}
static void ati_dst(int optype,GLuint mod,GLuint mask,fp_poly *src,fp_poly *dst) {
 int c,start=optype?3:0,end=optype?4:3,sat=(mod&GL_SATURATE_BIT_ATI)!=0;float f=1.0F;
 mod&=~GL_SATURATE_BIT_ATI;
 f=mod==GL_2X_BIT_ATI?2.0F:mod==GL_4X_BIT_ATI?4.0F:mod==GL_8X_BIT_ATI?8.0F:mod==GL_HALF_BIT_ATI?0.5F:mod==GL_QUARTER_BIT_ATI?0.25F:mod==GL_EIGHTH_BIT_ATI?0.125F:1.0F;
 for(c=start;c<end;c++){fp_poly t;float lo,hi;t.nt=0;p_addto(&t,&src[c],f);
  if(sat)p_sat(&t);else{p_range(&t,&lo,&hi);if(lo<-8.0F||hi>8.0F)fp_bad=1;} /* (the -8..8 clamp: only where it does nothing) */
  if(optype||!mask||(mask&(1U<<c)))dst[c]=t;}
}
static int run_ati(GLcontext *ctx,const struct ati_fragment_shader *sh) {
 static fp_poly R[6][4],P[6][4];int pass,j,c,i;GLuint pc;
 for(j=0;j<6;j++)for(c=0;c<4;c++)R[j][c].nt=0;
 for(pass=0;pass<sh->NumPasses&&pass<2&&!fp_bad;pass++) {
  if(pass)memcpy(P,R,sizeof(P));
  for(j=0;j<MAX_NUM_FRAGMENT_REGISTERS_ATI;j++) {
   const struct atifs_setupinst *si=&sh->SetupInst[pass][j];
   if(si->Opcode==ATI_FRAGMENT_SHADER_PASS_OP) { /* coordinates as a colour: only a register of the pass before, undivided */
    GLuint k=si->src-GL_REG_0_ATI;
    if(si->src<GL_REG_0_ATI||si->src>GL_REG_5_ATI||(si->swizzle!=GL_SWIZZLE_STR_ATI&&si->swizzle!=GL_SWIZZLE_STQ_ATI)){fp_bad=1;break;}
    R[j][0]=P[k][0];R[j][1]=P[k][1];R[j][2]=P[k][si->swizzle==GL_SWIZZLE_STQ_ATI?3:2];R[j][3].nt=0;
   }
   else if(si->Opcode==ATI_FRAGMENT_SHADER_SAMPLE_OP) { /* unit j at its own coordinates */
    GLuint tg=ctx->Texture.Unit[j]._ReallyEnabled;int deep=(tg&(TEXTURE_3D_BIT|TEXTURE_CUBE_BIT))!=0;
    if(si->src!=GL_TEXTURE0_ARB+(GLuint)j||tex_of_unit[j]<0){fp_bad=1;break;}
    if(si->swizzle==GL_SWIZZLE_STR_ATI)tex_tex[j]=1;
    else if(si->swizzle==GL_SWIZZLE_STQ_ATI&&!deep)tex_tex[j]=1;
    else if(si->swizzle==GL_SWIZZLE_STQ_DQ_ATI&&!deep)tex_txp[j]=1;
    else {fp_bad=1;break;}
    texel(tex_of_unit[j],R[j]);
   }
  }
  for(pc=0;pc<sh->numArithInstr[pass]&&!fp_bad;pc++) {
   const struct atifs_instruction *in=&sh->Instructions[pass][pc];fp_poly src[2][3][4],dst[2][4];int optype;
   for(optype=0;optype<2;optype++)for(i=0;i<(int)in->ArgCount[optype]&&i<3;i++)ati_arg(ctx,sh,&in->SrcReg[optype][i],optype,R,src[optype][i]);
   if(fp_bad)break;
   for(optype=0;optype<2&&!fp_bad;optype++) {
    GLenum op=in->Opcode[optype];int c0=optype?3:0,c1=optype?4:3;
    if(!op)continue;
    if(op==GL_DOT2_ADD_ATI||op==GL_DOT3_ATI||op==GL_DOT4_ATI){fp_poly r,t;int n=op==GL_DOT2_ADD_ATI?2:op==GL_DOT3_ATI?3:4; /* from the colour op's sources */
     if(!in->Opcode[0]||in->ArgCount[0]<2||(op==GL_DOT2_ADD_ATI&&in->ArgCount[0]<3)){fp_bad=1;break;}
     r.nt=0;for(i=0;i<n;i++){p_mul(&t,&src[0][0][i],&src[0][1][i]);p_addto(&r,&t,1.0F);}
     if(op==GL_DOT2_ADD_ATI)p_addto(&r,&src[0][2][2],1.0F);
     for(c=c0;c<c1;c++)dst[optype][c]=r;continue;}
    for(c=c0;c<c1;c++){fp_poly *a=src[optype][0],*b=src[optype][1],*d=src[optype][2],t,u;
     switch(op){
     case GL_ADD_ATI:p_lin(&dst[optype][c],&a[c],1.0F,&b[c],1.0F);break;
     case GL_SUB_ATI:p_lin(&dst[optype][c],&a[c],1.0F,&b[c],-1.0F);break;
     case GL_MUL_ATI:p_mul(&dst[optype][c],&a[c],&b[c]);break;
     case GL_MAD_ATI:p_mul(&t,&a[c],&b[c]);p_lin(&dst[optype][c],&t,1.0F,&d[c],1.0F);break;
     case GL_LERP_ATI:p_mul(&t,&a[c],&b[c]);p_mul(&u,&a[c],&d[c]);p_lin(&dst[optype][c],&t,1.0F,&u,-1.0F);p_addto(&dst[optype][c],&d[c],1.0F);break;
     case GL_MOV_ATI:dst[optype][c]=a[c];break;
     case GL_CND_ATI:case GL_CND0_ATI:{float k=fconst(&d[c]);dst[optype][c]=(op==GL_CND_ATI?k>0.5F:k>=0.0F)?a[c]:b[c];break;} /* a constant condition only */
     default:fp_bad=1;
     }}
   }
   for(optype=0;optype<2&&!fp_bad;optype++) {
    GLenum aop=in->Opcode[1];GLuint d;
    if(!in->Opcode[optype])continue;
    d=in->DstReg[optype].Index-GL_REG_0_ATI;if(d>5){fp_bad=1;break;}
    if(optype==0||(aop!=GL_DOT2_ADD_ATI&&aop!=GL_DOT3_ATI&&aop!=GL_DOT4_ATI))ati_dst(optype,in->DstReg[optype].dstMod,in->DstReg[optype].dstMask,dst[optype],R[d]);
    else ati_dst(1,in->DstReg[0].dstMod,0,dst[1],R[d]);
   }
  }
 }
 for(c=0;c<4;c++)outc[c]=R[0][c];
 return !fp_bad;
}

/* ---- the stages: last first, CURRENT solved for and matched by the stages before */
typedef struct {uint32_t op,a1,a2,a0,sc;} fp_half;
static fp_half hc[3],ha[3];
static float tfv[4];static int tfs[4];
static int nst,ntx,lerp_ok;
/* a value p X + q in one channel: X the unknown (u 0 none, 1 CURRENT's channel, 2 + c TFACTOR's channel c) */
typedef struct {fp_poly p,q;int u;} fp_aff;
static void aff_known(fp_aff *v,const fp_poly *q) {v->p.nt=0;v->q=*q;v->u=0;}
static int aff_add(fp_aff *r,const fp_aff *a,float sa,const fp_aff *b,float sb) {
 if(a->u&&b->u&&a->u!=b->u)return 0;
 r->u=a->u?a->u:b->u;p_lin(&r->p,&a->p,sa,&b->p,sb);p_lin(&r->q,&a->q,sa,&b->q,sb);return !fp_bad;
}
static int aff_mul(fp_aff *r,const fp_aff *a,const fp_aff *b) { /* one side known */
 fp_poly t;
 if(a->u&&a->p.nt&&b->u&&b->p.nt)return 0;
 if(a->u&&a->p.nt){p_mul(&r->p,&a->p,&b->q);p_mul(&r->q,&a->q,&b->q);r->u=a->u;}
 else if(b->u&&b->p.nt){p_mul(&r->p,&b->p,&a->q);p_mul(&r->q,&a->q,&b->q);r->u=b->u;}
 else {p_mul(&t,&a->q,&b->q);r->p.nt=0;r->q=t;r->u=0;}
 return !fp_bad;
}
static void aff_addk(fp_aff *r,float k) {p_addk(&r->q,k);}
static void aff_scale(fp_aff *r,float k) {int i;for(i=0;i<r->p.nt;i++)r->p.t[i].k*=k;for(i=0;i<r->q.nt;i++)r->q.t[i].k*=k;p_fin(&r->p);p_fin(&r->q);}
/* argument a (Direct3D's: 0 diffuse, 1 CURRENT, 2 texture, 3 TFACTOR, 4 specular; 0x10 complement, 0x20 alpha) at stage s in channel c */
static int arg_val(uint32_t a,int s,int c,int alpha,fp_aff *v) {
 int ch=(alpha||(a&0x20))?3:c;fp_poly t;
 switch(a&15){
 case 0:p_atom(&t,AT_D+ch);aff_known(v,&t);break;
 case 1:if((a&0x20)&&!alpha)return 0;v->p.nt=0;p_const(&v->p,1.0F);v->q.nt=0;v->u=1;break;
 case 2:{fp_poly x[4];if(s>=ntx)return 0;if(tex_base[s]==GL_ALPHA&&ch<3)return 0; /* the engine's copy's colour is not GL's 0 */
  texel(s,x);aff_known(v,&x[ch]);break;}
 case 3:if(tfs[ch]){p_const(&t,tfv[ch]);aff_known(v,&t);}else{p_const(&v->p,1.0F);v->q.nt=0;v->u=2+ch;}break;
 case 4:if(ch>=3)return 0;p_atom(&t,AT_S+ch);aff_known(v,&t);break;
 default:return 0;
 }
 if(a&0x10){aff_scale(v,-1.0F);aff_addk(v,1.0F);}
 return 1;
}
static float op_scale(uint32_t op,uint32_t sc) {return (float)(op==5||op==9?2:op==6?4:1)*(float)(1<<sc);}
/* the stage's result in channel c as p X + q */
static int op_val(const fp_half *h,int s,int c,int alpha,fp_aff *r) {
 fp_aff x,y,z,t;uint32_t op=h->op;
 if(!arg_val(h->a1,s,c,alpha,&x))return 0;
 if(op!=2&&!arg_val(h->a2,s,c,alpha,&y))return 0;
 switch(op){
 case 2:*r=x;break;
 case 4:case 5:case 6:if(!aff_mul(r,&x,&y))return 0;break;
 case 7:if(!aff_add(r,&x,1.0F,&y,1.0F))return 0;break;
 case 8:case 9:if(!aff_add(r,&x,1.0F,&y,1.0F))return 0;aff_addk(r,-0.5F);break;
 case 10:if(!aff_add(r,&x,1.0F,&y,-1.0F))return 0;break;
 case 11:if(!aff_mul(&t,&x,&y)||!aff_add(&z,&x,1.0F,&y,1.0F)||!aff_add(r,&z,1.0F,&t,-1.0F))return 0;break; /* x + y - x y */
 case 12:case 13:case 14:case 26:{fp_aff f,g,fy; /* x f + y (1 - f) */
  if(op==26){if(!arg_val(h->a0,s,c,alpha,&f))return 0;}
  else if(!arg_val(op==12?0x20:op==13?0x22:0x23,s,c,1,&f))return 0;
  if(!aff_mul(&t,&x,&f)||!aff_mul(&g,&y,&f)||!aff_add(&fy,&y,1.0F,&g,-1.0F)||!aff_add(r,&t,1.0F,&fy,1.0F))return 0;break;}
 default:return 0;
 }
 aff_scale(r,op_scale(op,h->sc));
 return !fp_bad;
}
static int match(const fp_poly *E,int nc,int k,int alpha,int clampok);
/* v10.337 (board, VXPROBE fpcache: an LRP by a program.local parameter searched 400000 steps for each of 1-3 stages, 4.1 s, then went
 * to software; host: the largest search that found a cascade took 3073 steps over every host program): one budget of 20000 steps for the
 * whole search (about 0.2 s on the board), and a program that ran out of it is not searched again for other parameter values (fp_neg) */
#define FP_STEPS 20000L
static long fp_steps; /* the search's budget (beyond it: software) */
/* what stages 0..k can give at all: textures of those stages only, degree at most k + 2 (each stage multiplies by one argument at most) */
static int reachable(const fp_poly *E,int nc,int k) {
 int c,i,j;
 for(c=0;c<nc;c++)for(i=0;i<E[c].nt;i++){const fp_term *t=&E[c].t[i];if(t->n>k+2)return 0;
  for(j=0;j<t->n;j++){int a=t->a[j];if(a==AT_X||(a>=AT_T&&a<AT_T+12&&(a-AT_T)/4>k))return 0;}}
 return 1;
}
/* the op h at stage k gives E in the group's channels: the unknown solved (TFACTOR set, CURRENT matched by the stages before) */
/* the search goes on past a complete cascade the core will not compose (another arrangement may compose): a complete one for the colour
 * goes on to the alpha, a complete alpha to the check (fp_valid); a 0 from it makes the search try the next */
static int fp_group; /* 0 the colour being matched, 1 the alpha */
static int fp_valid(void);
static int fp_done(void) {int r;if(fp_group)return fp_valid();fp_group=1;r=match(&outc[3],1,nst-1,1,1);fp_group=0;return r;}
static void pass_below(int k,int alpha) {int i;for(i=0;i<k;i++){fp_half *o=alpha?&ha[i]:&hc[i];o->op=2;o->a1=1;o->a2=1;o->a0=1;o->sc=0;}}
static int try_op(const fp_poly *E,int nc,int k,int alpha,const fp_half *h) {
 fp_poly C[3];int c,u=0;float tsave[4];int ssave[4];
 if(++fp_steps>FP_STEPS){fp_bad=1;return 0;}
 memcpy(tsave,tfv,sizeof(tfv));memcpy(ssave,tfs,sizeof(tfs));
 for(c=0;c<nc;c++) {
  fp_aff r;fp_poly d;
  C[c].nt=0; /* v10.330 (review #39): a channel that does not read CURRENT is matched as 0 below, not as whatever the stack held */
  if(!op_val(h,k,alpha?3:c,alpha,&r))goto no;
  p_lin(&d,&E[c],1.0F,&r.q,-1.0F);
  if(!r.u||!r.p.nt){if(d.nt)goto no;if(r.u==1){p_const(&C[c],0.0F);u=1;}continue;} /* (CURRENT unread in this channel: matched as 0) */
  if(!p_div(&C[c],&d,&r.p))goto no;
  if(r.u>=2){float t;int ch=r.u-2;if(!p_isconst(&C[c],&t)||t<-1e-4F||t>1.0F+1e-4F)goto no;
   if(tfs[ch]){if(t-tfv[ch]>1e-3F||tfv[ch]-t>1e-3F)goto no;}else{tfs[ch]=1;tfv[ch]=t<0.0F?0.0F:t>1.0F?1.0F:t;}}
  else u=1;
 }
 if(fp_bad)goto no;
 if(alpha)ha[k]=*h;else hc[k]=*h;
 if(u){if(match(C,nc,k-1,alpha,0))return 1;} /* CURRENT: the stages before */
 else {pass_below(k,alpha);if(fp_done())return 1;}
no:
 memcpy(tfv,tsave,sizeof(tfv));memcpy(tfs,ssave,sizeof(tfs));return 0;
}
/* Direct3D's DOTPRODUCT3 (into the colour; the alpha its own, VX_D3D_TSS_DOT3RGB): 4 sum (x - 0.5)(y - 0.5), both arguments known */
static int try_dot3(const fp_poly *E,int k,uint32_t a1,uint32_t a2) {
 fp_poly P;int c;P.nt=0;
 if(++fp_steps>FP_STEPS){fp_bad=1;return 0;}
 for(c=0;c<3;c++){fp_aff x,y;fp_poly t;if(!arg_val(a1,k,c,0,&x)||!arg_val(a2,k,c,0,&y)||x.u||y.u)return 0;p_addk(&x.q,-0.5F);p_addk(&y.q,-0.5F);p_mul(&t,&x.q,&y.q);p_addto(&P,&t,4.0F);}
 for(c=0;c<3;c++)if(!p_eq(&E[c],&P))return 0;
 hc[k].op=24;hc[k].a1=a1;hc[k].a2=a2;hc[k].a0=1;hc[k].sc=0;pass_below(k,0);
 return fp_done();
}
static int match(const fp_poly *Ein,int nc,int k,int alpha,int clampok) {
 static const uint32_t ops2[]={4,7,10,8,11,12,13,14,5,6,9};
 fp_poly E[3];uint32_t cand[24];int nca=0,c,i,j,o;
 for(c=0;c<nc;c++)E[c]=Ein[c];
 /* every channel a clamp: the stage's own clamp gives it */
 for(c=0;c<nc;c++)if(!(E[c].nt==1&&E[c].t[0].n==1&&E[c].t[0].k==1.0F&&E[c].t[0].a[0]>=AT_CL))break;
 if(c==nc){for(c=0;c<nc;c++)E[c]=cl_in[E[c].t[0].a[0]-AT_CL];clampok=1;}
 if(fp_bad)return 0;
 if(!clampok)for(c=0;c<nc;c++)if(!p_unit_stage(&E[c]))return 0; /* the engine would clamp it here; the program does not */
 if(!reachable(E,nc,k<0?0:k))return 0;
 if(k<0){for(c=0;c<nc;c++){fp_poly d;p_atom(&d,AT_D+(alpha?3:c));if(!p_eq(&E[c],&d))return 0;}return fp_done();} /* CURRENT at stage 0: the diffuse colour */
 /* this stage passes CURRENT through */
 {fp_half *o=alpha?&ha[k]:&hc[k];o->op=2;o->a1=1;o->a2=1;o->a0=1;o->sc=0;if(match(E,nc,k-1,alpha,clampok))return 1;}
 for(i=0;i<5;i++){uint32_t b=i,m;if(b==1||(b==2&&k>=ntx)||(b==4&&alpha))continue;
  for(m=0;m<(alpha?2U:4U);m++){if(b==4&&(m&2))continue;cand[nca++]=b|((m&1)?0x10:0)|((m&2)?0x20:0);}}
 cand[nca++]=1;cand[nca++]=0x11;
 for(i=0;i<nca;i++)if(cand[i]!=1&&cand[i]!=0x11){fp_half h;h.op=2;h.a1=cand[i];h.a2=1;h.a0=1;h.sc=0;if(try_op(E,nc,k,alpha,&h))return 1;}
 for(o=0;o<(int)(sizeof(ops2)/sizeof(ops2[0]));o++) {
  uint32_t op=ops2[o];int comm=op!=10&&op<12;
  if(op==13&&k>=ntx)continue;
  for(i=0;i<nca;i++)for(j=comm?i:0;j<nca;j++){fp_half h;h.op=op;h.a1=cand[i];h.a2=cand[j];h.a0=1;h.sc=0;if(try_op(E,nc,k,alpha,&h))return 1;}
 }
 /* a scaled select, add or subtract (the stage's result scale) */
 for(o=0;o<3;o++){uint32_t op=o==0?2:o==1?7:10,sc;
  for(sc=1;sc<=2;sc++)for(i=0;i<nca;i++)for(j=0;j<(op==2?1:nca);j++){fp_half h;h.op=op;h.a1=cand[i];h.a2=op==2?1:cand[j];h.a0=1;h.sc=sc;if((op==2&&(h.a1&15)==1))continue;if(try_op(E,nc,k,alpha,&h))return 1;}}
 if(!alpha&&vxgl_gl_dot3rgb)for(i=0;i<nca;i++)for(j=i;j<nca;j++)if((cand[i]&15)!=1&&(cand[j]&15)!=1&&try_dot3(E,k,cand[i],cand[j]))return 1;
 if(k==0&&nst==1&&lerp_ok&&vxgl_gl_lerp)for(c=0;c<nca;c++)for(i=0;i<nca;i++)for(j=0;j<nca;j++){fp_half h;
  if((cand[c]&15)==1||(cand[i]&15)==1||(cand[j]&15)==1)continue;
  h.op=26;h.a0=cand[c];h.a1=cand[i];h.a2=cand[j];h.sc=0;if(try_op(E,nc,k,alpha,&h))return 1;}
 return 0;
}

/* the cascade as the core will compose it (vx_d3d_program): the context's state with these stages, a texture on each textured one */
static vxgl_ctx *fp_ctx;
static int fp_valid(void) {
 static vx_d3d_context sc;int s;uint32_t e;
 sc.state=fp_ctx->core.state;sc.state.rs[29]=0;
 for(s=0;s<VX_D3D_STAGES;s++)memcpy(sc.state.tss[s],fp_ctx->tss_default[s],sizeof(sc.state.tss[s]));
 for(s=0;s<nst;s++){uint32_t *t=sc.state.tss[s];
  t[0]=s<ntx?1U:0U;t[11]=s<2?(uint32_t)s:0U;
  t[1]=hc[s].op;t[2]=hc[s].a1;t[3]=hc[s].a2;t[VX_D3D_TSS_CARG0]=hc[s].a0;t[VX_D3D_TSS_CSCALE]=hc[s].sc;
  t[4]=ha[s].op;t[5]=ha[s].a1;t[6]=ha[s].a2;t[VX_D3D_TSS_AARG0]=ha[s].a0;t[VX_D3D_TSS_ASCALE]=ha[s].sc;t[VX_D3D_TSS_DOT3RGB]=hc[s].op==24?1U:0U;}
 if(s<VX_D3D_STAGES){sc.state.tss[s][1]=1;sc.state.tss[s][4]=1;}
 return vx_d3d_program(&sc,ntx>0,&e)!=-2;
}
/* v10.321: the last answer, kept while what it depends on hashes the same (state translation runs at every state change) */
/* v10.335 (review #22; board, VXPROBE fpcache: two programs alternating drew 49 ms a frame against 1.8 for one: every switch missed the
 * one-entry memo and translated again, 656 microseconds each; a hit cost 69 microseconds, hashing every local and environment parameter,
 * 8 KB, a byte at a time): sixteen entries, a 64-bit key (two independent word hashes) with the program's address and instruction count,
 * and only the parameters the program's instructions name (a parameter it does not read cannot change the translation) */
typedef struct {uint32_t a,b;} fp_key;
static void hk(fp_key *k,const void *p,size_t n) {
 const unsigned char *q=(const unsigned char*)p;uint32_t w;
 while(n>=4){memcpy(&w,q,4);q+=4;n-=4;k->a=(k->a^w)*16777619UL;k->b=(k->b+w)*0x9E3779B1UL;k->b^=k->b>>15;}
 while(n--){w=*q++;k->a=(k->a^w)*16777619UL;k->b=(k->b+w)*0x9E3779B1UL;k->b^=k->b>>15;}
}
#define FP_MEMO_N 16
typedef struct {fp_key key;const void *prog;GLuint ninst;int valid,ok,on,nst,tf_used,ati,kil;uint32_t ops[3][10],tf,kilref;} fp_memo;
static fp_memo memos[FP_MEMO_N];static int memo_next;
/* v10.337: programs whose search ran out of budget, keyed without their parameter values: software at once for any value */
#define FP_NEG_N 8
static fp_key fp_neg[FP_NEG_N];static int fp_neg_n,fp_neg_next;
/* v10.333 (VXPROBE fpcache, review #22): how often the one-entry memo is hit, and (while VXPROBE turns timing on) what a translation costs */
static uint32_t fps_calls,fps_hits,fps_miss,fps_sw,fps_on;static double fps_t0,fps_miss_us,fps_miss_max,fps_hit_us,fps_freq;
static double fps_now(void){LARGE_INTEGER c;if(fps_freq<=0){LARGE_INTEGER f;fps_freq=QueryPerformanceFrequency(&f)&&f.QuadPart?(double)f.QuadPart:1;}QueryPerformanceCounter(&c);return (double)c.QuadPart*1e6/fps_freq;}
static void fps_miss_end(void){if(fps_on){double d=fps_now()-fps_t0;fps_miss_us+=d;if(d>fps_miss_max)fps_miss_max=d;}}
/* o[0] translations asked for, [1] memo hits, [2] misses (translated), [3] to software, [4] microseconds in misses, [5] the longest miss,
 * [6] microseconds in hits (the key), [7] timing on; on: 0 off, 1 on, 2 counters cleared and on */
uint32_t __stdcall VXGL_FpStats(uint32_t *o,uint32_t on) {
 if(o){o[0]=fps_calls;o[1]=fps_hits;o[2]=fps_miss;o[3]=fps_sw;o[4]=(uint32_t)fps_miss_us;o[5]=(uint32_t)fps_miss_max;o[6]=(uint32_t)fps_hit_us;o[7]=fps_on;}
 if(on==2){fps_calls=fps_hits=fps_miss=fps_sw=0;fps_miss_us=fps_miss_max=fps_hit_us=0;}
 fps_on=on?1U:0U;return 1;
}
/* the current fragment program as stages (c->fp_*): 1 drawable, 0 software. The units it reads are the first textured stages in unit
 * order (vxgl_tex_state lays them out); proj: the units whose coordinates are projective there (their reads must be TXP) */
int vxgl_fp_compile(vxgl_ctx *c,unsigned proj,int flat_only) {
 GLcontext *ctx=&c->gl;const struct gl_fragment_program *fp=ctx->FragmentProgram._Current;int u,s,k,ok=0,ati=!fp&&ctx->ATIFragmentShader._Enabled,fps_m=0;fp_memo *memo=0;fp_key neg_key;
 static const char *why;
 c->fp_on=0;c->fp_nst=0;c->fp_tf_used=0;c->fp_ati=0;c->fp_kil=0;c->fp_kilref=0;
 if(!fp&&!ati)return 1;
 fps_calls++;if(fps_on)fps_t0=fps_now(); /* v10.333 */
 why="disabled (ACCEL_OPENGL_FPROG)";if(!vxgl_gl_fprog)goto sw;
 if(fp){why="not an ARB program";if(fp->Base.Target!=GL_FRAGMENT_PROGRAM_ARB)goto sw;
  if(fp->Base.Parameters)_mesa_load_state_parameters(ctx,fp->Base.Parameters);} /* as swrast does before running it */
 else {why="no ATI shader";if(!ctx->ATIFragmentShader.Current)goto sw;}
 for(u=0;u<8;u++){tex_of_unit[u]=-1;tex_txp[u]=tex_tex[u]=0;}
 tex_n=0;
 for(u=0;u<8;u++)if(ctx->Texture._EnabledUnits&(1U<<u)) {
  const struct gl_texture_object *t=ctx->Texture.Unit[u]._Current;const struct gl_texture_image *im;GLenum b;
  why="more than three textures";if(tex_n>=3||u>=3)goto sw;
  im=t?t->Image[0][t->BaseLevel]:0;why="no texture image";if(!im)goto sw;
  b=im->_BaseFormat;if(b==GL_DEPTH_COMPONENT||b==GL_DEPTH_STENCIL_EXT)b=t->DepthMode;if(b==GL_COLOR_INDEX)b=GL_RGBA;
  tex_base[tex_n]=(int)b;tex_unit[tex_n]=u;tex_of_unit[u]=tex_n++;
 }
 { /* the key: the program (its instructions, parameters, constants), the units' textures, and what the stages may use */
  fp_key h,hs;uint32_t v[10];const void *pid;GLuint ni;int m;
  h.a=2166136261UL;h.b=0x6A09E667UL;
  if(fp){const struct gl_program *pr=&fp->Base;GLuint i,j;pid=fp;ni=pr->NumInstructions;hk(&h,&fp,sizeof(fp));hk(&h,&pr->NumInstructions,sizeof(pr->NumInstructions));
   if(pr->Instructions)hk(&h,pr->Instructions,pr->NumInstructions*sizeof(struct prog_instruction));
   hs=h; /* v10.337: the program without its parameter values */
   if(pr->Parameters&&pr->Parameters->ParameterValues)hk(&h,pr->Parameters->ParameterValues,pr->Parameters->NumParameters*4*sizeof(GLfloat));
   for(i=0;pr->Instructions&&i<pr->NumInstructions;i++)for(j=0;j<3;j++){const struct prog_src_register *sr=&pr->Instructions[i].SrcReg[j];
    if(sr->File==PROGRAM_LOCAL_PARAM&&sr->Index>=0&&sr->Index<MAX_PROGRAM_LOCAL_PARAMS)hk(&h,pr->LocalParams[sr->Index],4*sizeof(GLfloat));
    else if(sr->File==PROGRAM_ENV_PARAM&&sr->Index>=0&&sr->Index<MAX_PROGRAM_ENV_PARAMS)hk(&h,ctx->FragmentProgram.Parameters[sr->Index],4*sizeof(GLfloat));}}
  else {const struct ati_fragment_shader *sh=ctx->ATIFragmentShader.Current;int p2;pid=sh;ni=0;hk(&h,&sh,sizeof(sh));hk(&h,&sh->NumPasses,1);hk(&h,sh->numArithInstr,2);
   for(p2=0;p2<2;p2++){if(sh->Instructions[p2])hk(&h,sh->Instructions[p2],sh->numArithInstr[p2]*sizeof(struct atifs_instruction));
    if(sh->SetupInst[p2])hk(&h,sh->SetupInst[p2],MAX_NUM_FRAGMENT_REGISTERS_ATI*sizeof(struct atifs_setupinst));}
   hs=h;
   hk(&h,sh->Constants,sizeof(sh->Constants));hk(&h,&sh->LocalConstDef,sizeof(sh->LocalConstDef));hk(&h,ctx->ATIFragmentShader.GlobalConstants,sizeof(ctx->ATIFragmentShader.GlobalConstants));}
  v[0]=ctx->Texture._EnabledUnits;v[1]=proj;v[2]=(uint32_t)flat_only;v[3]=(uint32_t)vxgl_gl_dot3rgb|((uint32_t)vxgl_gl_lerp<<1);v[4]=vx_d3d_feat;
  v[5]=(uint32_t)tex_n;v[6]=tex_n>0?(uint32_t)tex_base[0]:0;v[8]=tex_n>1?(uint32_t)tex_base[1]:0;v[9]=tex_n>2?(uint32_t)tex_base[2]:0; /* .518 (audit): each active texture's base format in its own word (the signed shifts overflowed for GL_INTENSITY in the third, and folded enums wider than a byte; stale entries of an earlier draw no longer count) */v[7]=ctx->Texture.Unit[0]._ReallyEnabled|(ctx->Texture.Unit[1]._ReallyEnabled<<8)|(ctx->Texture.Unit[2]._ReallyEnabled<<16);
  hk(&h,v,sizeof(v));hk(&hs,v,sizeof(v));
  for(m=0;m<FP_MEMO_N;m++){fp_memo *e=&memos[m];
   if(e->valid&&e->key.a==h.a&&e->key.b==h.b&&e->prog==pid&&e->ninst==ni){
    fps_hits++;if(fps_on)fps_hit_us+=fps_now()-fps_t0;if(!e->ok){fps_sw++;return 0;}
    c->fp_on=e->on;c->fp_nst=e->nst;c->fp_tf_used=e->tf_used;c->fp_ati=e->ati;memcpy(c->fp_ops,e->ops,sizeof(e->ops));c->fp_tf=e->tf;c->fp_kil=e->kil;c->fp_kilref=e->kilref;return 1;}}
  memo=&memos[memo_next];memo_next=(memo_next+1)%FP_MEMO_N;
  memo->key=h;memo->prog=pid;memo->ninst=ni;memo->valid=1;memo->ok=0;fps_miss++;fps_m=1;
  for(m=0;m<fp_neg_n;m++)if(fp_neg[m].a==hs.a&&fp_neg[m].b==hs.b){why="too complex (as with other parameter values)";goto sw;}
  neg_key=hs;
 }
 fp_bad=0;cl_n=0;kil_n=0;
 why="an instruction or register not drawable";if(fp?!run(ctx,fp):!run_ati(ctx,ctx->ATIFragmentShader.Current))goto sw;
 /* TEX reads coordinates as they are: not where the engine divides them (projective) */
 why="TEX on projective coordinates";for(u=0;u<8;u++)if(tex_tex[u]&&(proj&(1U<<u)))goto sw;
 ntx=tex_n;lerp_ok=tex_n<=1&&flat_only;
 fp_steps=0; /* v10.337: one budget for every stage count */
 for(s=ntx;s<=3;s++) { /* the fewest stages */
  if(!s){fp_poly d;for(k=0;k<4;k++){p_atom(&d,AT_D+k);if(!p_eq(&outc[k],&d))break;}if(k==4){ok=1;break;}continue;}
  nst=s;memset(tfs,0,sizeof(tfs));memset(tfv,0,sizeof(tfv));fp_group=0;fp_ctx=c;
  if(match(outc,3,s-1,0,1)){ok=1;break;} /* (the alpha and the check from fp_done) */
  if(fp_bad)break;
 }
 if(!ok&&fp_steps>FP_STEPS){fp_neg[fp_neg_next]=neg_key;fp_neg_next=(fp_neg_next+1)%FP_NEG_N;if(fp_neg_n<FP_NEG_N)fp_neg_n++;} /* v10.337 */
 why=fp_bad?"too complex":"no cascade of the engine's stages computes it";if(!ok)goto sw;
 /* v10.353: KIL as the engine's alpha test. Its operand's components: a constant below 0 kills every fragment (the test NEVER), one at 0
  * or above none; every other component the same polynomial P, which must be a * (the result alpha) + b with a > 0: killed when P < 0, kept
  * when the alpha >= -b / a, the test GREATEREQUAL with that reference (in 0..1 exclusive of 0: the engine's alpha is clamped, a program's
  * is not); otherwise software. The test the application set is the engine's only one: with GL_ALPHA_TEST on as well, software (vxgl_state.c) */
 if(kil_n){int kv=-1,never=0;float a=0.0F,b=0.0F,ref;
  why="KIL not an alpha test";
  for(k=0;k<4;k++){float kc;if(p_isconst(&kil[k],&kc)){if(kc<0.0F)never=1;continue;}if(kv<0)kv=k;else if(!p_same(&kil[k],&kil[kv]))goto sw;}
  if(never){c->fp_kil=2;c->fp_kilref=0;}
  else if(kv>=0){fp_poly R;const fp_poly *P=&kil[kv],*A=&outc[3];int i,j;
   if(!A->nt)goto sw;
   for(i=0;i<P->nt&&!a;i++)if(P->t[i].n)for(j=0;j<A->nt;j++)if(!term_cmp(&P->t[i],&A->t[j])){a=P->t[i].k/A->t[j].k;break;}
   if(!(a>0.0F))goto sw;
   R.nt=0;p_addto(&R,P,1.0F);p_addto(&R,A,-a);p_norm(&R);
   if(fp_bad||!p_isconst(&R,&b))goto sw;
   ref=-b/a;if(!(ref>0.0F&&ref<=1.0F))goto sw;
   {double r255=(double)ref*255.0;uint32_t rb=(uint32_t)r255;if((double)rb<r255-1e-4)rb++; /* the alpha byte at or above ref */
    c->fp_kil=1;c->fp_kilref=rb>255U?255U:rb;}}}
 c->fp_on=1;c->fp_nst=s;c->fp_ati=ati;
 for(k=0;k<3;k++){const fp_half *x=&hc[k],*y=&ha[k];uint32_t *o=c->fp_ops[k];o[0]=x->op;o[1]=x->a1;o[2]=x->a2;o[3]=x->a0;o[4]=x->sc;o[5]=y->op;o[6]=y->a1;o[7]=y->a2;o[8]=y->a0;o[9]=y->sc;}
 for(k=0;k<4;k++)if(tfs[k])c->fp_tf_used=1;
 if(c->fp_tf_used){GLfloat v[4];for(k=0;k<4;k++)v[k]=tfs[k]?tfv[k]:1.0F;
  {GLubyte b[4];for(k=0;k<4;k++)UNCLAMPED_FLOAT_TO_UBYTE(b[k],v[k]);c->fp_tf=((uint32_t)b[3]<<24)|((uint32_t)b[0]<<16)|((uint32_t)b[1]<<8)|b[2];}}
 memo->ok=1;memo->on=c->fp_on;memo->nst=c->fp_nst;memo->tf_used=c->fp_tf_used;memo->ati=c->fp_ati;memo->kil=c->fp_kil;memo->kilref=c->fp_kilref;memcpy(memo->ops,c->fp_ops,sizeof(memo->ops));memo->tf=c->fp_tf;
 fps_miss_end();
 return 1;
sw:
 fps_sw++;if(fps_m)fps_miss_end();
 {static int n;if(n++<16)vxgl_log("fragment program to software: %s\n",why);}
 return 0;
}
