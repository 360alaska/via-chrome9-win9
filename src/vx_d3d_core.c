/* DP2 is a legacy Windows DDI, not an application command format invented
 * for this driver. Bounds are bytes for commands, vertices for dwVertexLength.
 * No drawing or state replay is attempted after a submit failure. */
#include "vx_d3d_core.h"
#include <string.h>
#include <stddef.h>
/* v10.222: these run several times per vertex. Byte-by-byte loads and a library memcpy call for each float reinterpretation were a
 * quarter of the HAL's instructions per triangle (emulator profile). x86 loads unaligned words directly; a union reinterprets. */
#if defined(__GNUC__)
typedef uint32_t __attribute__((may_alias,aligned(1))) vx_u32a;
typedef uint16_t __attribute__((may_alias,aligned(1))) vx_u16a;
typedef float __attribute__((may_alias,aligned(1))) vx_f32a;
#else
typedef uint32_t vx_u32a;
typedef uint16_t vx_u16a;
typedef float vx_f32a;
#endif
typedef union {float f;uint32_t n;} vx_fu;
static uint32_t u32(const uint8_t *p) {return *(const vx_u32a *)p;}
static uint32_t u16(const uint8_t *p) {return *(const vx_u16a *)p;}
static float flt(uint32_t n) {vx_fu x;x.n=n;return x.f;}
static uint32_t bits(float f) {
 uint32_t n;vx_fu x;x.f=f;n=x.n;
 /* Ring 0 bounds-checks nonnegative IEEE encodings. Signed zero is a
  * valid screen/depth coordinate; publish its canonical positive encoding. */
 return (n&0x7FFFFFFFUL)?n:0;
}
static uint32_t bits_any(float f) {vx_fu x;x.f=f;return x.n;}
static int finite(uint32_t v) {return (v&0x7F800000UL)!=0x7F800000UL;}
static vx_d3d_state *state(vx_d3d_context *c) {return c->recording?&c->blocks[c->recording-1].state:&c->state;}
void vx_d3d_tnl_reset(const vx_d3d_context *c); /* .540 (vx_d3d_tnl.inc) */
static void template_reset(void); /* .553: below, with the template */
void vx_d3d_init(vx_d3d_context *c,const vx3dn_surface *t,const vx3dn_surface *z,uint32_t gen) {
 uint32_t s;
 template_reset(); /* .553: a new context can have the address of a destroyed one: nothing kept for that address (the template, the answers kept for a state) outlives it, as its T&L state does not (.540, next line) */
 vx_d3d_tnl_reset(c); /* .540: a context made again at an address another had starts with fresh T&L state */
 memset(c,0,sizeof(*c));c->target=*t;if(z)c->depth=*z;c->generation=gen;
 c->state.viewport.right=(int32_t)t->width;c->state.viewport.bottom=(int32_t)t->height;
 c->state.rs[7]=z&&z->format?1:0;c->state.rs[8]=3;c->state.rs[9]=2;c->state.rs[14]=1;
 c->state.rs[16]=1;c->state.rs[19]=2;c->state.rs[20]=1;c->state.rs[22]=3;c->state.rs[23]=4;
 c->state.rs[25]=8;c->state.rs[40]=0;c->state.rs[47]=0;c->state.rs[48]=0;
 c->state.rs[56]=8;c->state.rs[57]=0;c->state.rs[58]=0xFFFFFFFFUL;c->state.rs[59]=0xFFFFFFFFUL;
 c->state.rs[136]=1;c->state.rs[137]=1;c->state.rs[168]=15;c->state.rs[60]=0xFFFFFFFFUL; /* v10.172: TEXTUREFACTOR default */
 for(s=0;s<8;s++) {c->state.tss[s][1]=s?1:4;c->state.tss[s][2]=2;c->state.tss[s][3]=1;
  c->state.tss[s][4]=s?1:2;c->state.tss[s][5]=2;c->state.tss[s][6]=1;
  c->state.tss[s][11]=s;c->state.tss[s][13]=1;c->state.tss[s][14]=1;c->state.tss[s][16]=1;c->state.tss[s][17]=1;}
}
uint32_t vx_d3d_vertex_size(uint32_t f) {
 uint32_t n=16,i,count=(f>>8)&15;
 if((f&0xE)!=4||(f&~0xFFFF0FC4UL)||count>8) return 0;
 if(f&0x40)n+=4;if(f&0x80)n+=4;
 for(i=0;i<count;i++) {uint32_t fmt=(f>>(16+i*2))&3;n+=(fmt==0?2:fmt==1?3:fmt==2?4:1)*4;}
 return n;
}
/* v10.198: stage-0 colour and alpha are built as XP's driver builds them (CLXP3D T00-T15, T68-T115), not looked up per case. An instruction
 * is four words computing A*B+C: C register w0[23:16] and swizzle w0[15:4]; B register w1[19:12] and swizzle w1[11:0]; A register w2[15:8]
 * and swizzle (w2[7:0] << 4 | w1[31:28]); write mask w2[23:20] (7 colour, 8 alpha, F both); w2 bit 17 marks the last instruction that reads
 * the texture (or a temporary); w3 is the destination (bits 11:8: 8 output, C temporary), the x2/x4 scale (bits 13/14) and modifier bits.
 * Registers: 00 texture, 40 diffuse (CURRENT at stage 0), 41 specular, 82 constant c2 (TFACTOR), C0 none. Swizzle selectors (3 bits each):
 * 0-3 x y z w, 4 one, 5 zero; 055 = rgb, 053 = rgba, 6DB = alpha replicated, 924 = 1, B6D = 0.
 * Colour ops come from the captured colour instruction with the two arguments put in place; the alpha instruction is the same template
 * with its swizzles moved to alpha (055/053/6DD -> B6B, 050 -> 6DB), mask 8 and w3 bit 26 (checked against all six captured alpha forms).
 * Same op and arguments for colour and alpha (SELECT, MODULATE, ADD) fold into one instruction with mask F, as XP does. SPECULARENABLE sends
 * colour to a temporary and adds specular in a second instruction (a plain SELECT feeds the add directly), as XP does. */
/* (v10.198: the generated table vx_xp_programs.h is no longer used here) */
uint32_t vx_d3d_tss_seen[4],vx_d3d_addr_ring[16][2],vx_d3d_addr_count,vx_d3d_dp2_calls;
/* v10.420 (diagnostic): the HAL's trace of render-state commands (ACCEL_D3D_LOG); 0 = off (VXGL never sets it) */
void (*vx_d3d_rs_hook)(uint32_t id,uint32_t value,uint32_t at);
uint32_t (*vx_d3d_clock)(void);uint32_t vx_d3d_op_t[64],vx_d3d_op_n[64];static uint32_t ck_last,ck_cur; /* v10.390 */
void vx_d3d_ck_begin(void) {memset(vx_d3d_op_t,0,sizeof(vx_d3d_op_t));memset(vx_d3d_op_n,0,sizeof(vx_d3d_op_n));ck_cur=0;ck_last=vx_d3d_clock?vx_d3d_clock():0;}
static void ck_switch(uint32_t op) {uint32_t now;if(!vx_d3d_clock)return;now=vx_d3d_clock();vx_d3d_op_t[ck_cur]+=now-ck_last;ck_last=now;ck_cur=op&63;vx_d3d_op_n[ck_cur]++;}
void vx_d3d_ck_end(void) {uint32_t now;if(!vx_d3d_clock)return;now=vx_d3d_clock();vx_d3d_op_t[ck_cur]+=now-ck_last;ck_last=now;ck_cur=0;}
uint32_t (*vx_d3d_phclock)(void);uint32_t vx_d3d_ph_call[VX_PH_N],vx_d3d_ph_cnt[VX_PH_N];static uint32_t ph_cur,ph_last; /* v10.431 */
uint32_t vx_d3d_ph_set(uint32_t ph) {uint32_t now=vx_d3d_phclock(),o=ph_cur;vx_d3d_ph_call[o]+=now-ph_last;ph_last=now;ph_cur=ph<VX_PH_N?ph:VX_PH_HAL;vx_d3d_ph_cnt[ph_cur]++;return o;}
void vx_d3d_ph_begin(uint32_t ph) {memset(vx_d3d_ph_call,0,sizeof(vx_d3d_ph_call));ph_cur=ph;ph_last=vx_d3d_phclock?vx_d3d_phclock():0;}
void vx_d3d_ph_end(void) {if(vx_d3d_phclock){uint32_t now=vx_d3d_phclock();vx_d3d_ph_call[ph_cur]+=now-ph_last;ph_last=now;}}
enum {A_D,A_T,A_F,A_BAD};
static int arg(uint32_t a,int bound) {if(a<=1)return A_D;if(a==2)return bound?A_T:A_D;if(a==3)return A_F;return A_BAD;}
#define XR_TEX 0x00UL
#define XR_DIF 0x40UL
#define XR_SPE 0x41UL
#define XR_TF 0x82UL
#define XR_NUL 0xC0UL
#define XR_TMP 0x0CUL
enum {RO_N,RO_X,RO_Y,RO_T,RO_D,RO_F}; /* field role: keep, argument 1, argument 2, fixed texture, fixed diffuse, fixed TFACTOR */
typedef struct xp_tpl {uint8_t op,m1,m2,ra,rb,rc;uint32_t w[4];} xp_tpl;
#define CM 1
#define AR 2
static const xp_tpl xp_tpls[]={ /* colour forms, argument 1 = X, argument 2 = Y, as captured with X = texture, Y = diffuse */
 {2,0,0, RO_X,RO_N,RO_N,{0x01C00530UL,0x501C0053UL,0x00700005UL,0x000D0820UL}},  /* SELECTARG: T82 / T05 */
 {2,CM,0,RO_X,RO_N,RO_N,{0x0140B6D0UL,0x02140924UL,0x00700005UL,0x00100820UL}},  /* T105 */
 {2,AR,0,RO_X,RO_N,RO_N,{0x01C00530UL,0xD01C0053UL,0x0070006DUL,0x000D0820UL}},  /* T107 */
 {4,0,0, RO_X,RO_Y,RO_N,{0x0140B6D0UL,0x50040055UL,0x00700005UL,0x00000820UL}},  /* MODULATE: T01, T12 (TFACTOR), T114 (SPECULAR) */
 {4,0,CM,RO_X,RO_Y,RO_N,{0x2140B6D0UL,0x00040050UL,0x00700005UL,0x00100820UL}},  /* T104, T111 */
 {4,AR,0,RO_X,RO_Y,RO_N,{0x0140B6D0UL,0xD0040055UL,0x0070006DUL,0x00000820UL}},  /* T106 */
 {4,CM,AR,RO_X,RO_Y,RO_N,{0x0140B6D0UL,0x020406DBUL,0x00700005UL,0x00100820UL}}, /* T109 */
 {4,0,AR,RO_X,RO_Y,RO_N,{0x0140B6D0UL,0x500406DDUL,0x00700005UL,0x00000820UL}},  /* T112 */
 {7,0,0, RO_X,RO_N,RO_Y,{0x00400550UL,0x50140924UL,0x00700005UL,0x00000820UL}},  /* ADD: T06, T115 */
 {7,0,CM,RO_X,RO_N,RO_Y,{0x00400502UL,0x00140924UL,0x00700005UL,0x00100820UL}},  /* T110 */
 {8,0,0, RO_X,RO_N,RO_Y,{0x00400500UL,0x01140924UL,0x00700005UL,0x00100820UL}},  /* ADDSIGNED: T09 (2X: T97) */
 {10,0,0,RO_Y,RO_N,RO_X,{0x80000550UL,0x00140924UL,0x00704005UL,0x00000820UL}},  /* SUBTRACT: T96 */
 {11,0,0,RO_X,RO_Y,RO_X,{0x00000500UL,0x02040050UL,0x00700005UL,0x00100820UL}},  /* ADDSMOOTH: T98 */
 {12,0,0,RO_X,RO_Y,RO_D,{0x00406DB0UL,0x00040050UL,0x00700005UL,0x00110820UL}},  /* BLENDDIFFUSEALPHA: T11 (BLENDCURRENTALPHA T100) */
 {13,0,0,RO_X,RO_Y,RO_T,{0x00006DB0UL,0x00040050UL,0x00700005UL,0x00110820UL}},  /* BLENDTEXTUREALPHA: T10 */
 {14,0,0,RO_X,RO_Y,RO_F,{0x00826DB0UL,0x00040050UL,0x00700005UL,0x00110820UL}},  /* BLENDFACTORALPHA: T99 */
 {18,0,0,RO_X,RO_Y,RO_X,{0x00000550UL,0xD0040055UL,0x0070006DUL,0x00000820UL}},  /* MODULATEALPHA_ADDCOLOR: T101 */
 {19,0,0,RO_X,RO_Y,RO_X,{0x00006DD0UL,0x50040055UL,0x00700005UL,0x00000820UL}},  /* MODULATECOLOR_ADDALPHA: T102 */
 {15,0,0,RO_T,RO_Y,RO_X,{0x00000500UL,0xB2040050UL,0x0070006DUL,0x00100820UL}},  /* v10.206: BLENDTEXTUREALPHAPM: T138 */
 {20,0,0,RO_X,RO_Y,RO_X,{0x00000500UL,0xB2040050UL,0x0070006DUL,0x00100820UL}},  /* MODULATEINVALPHA_ADDCOLOR: T139 */
 {21,0,0,RO_X,RO_Y,RO_X,{0x00006DB0UL,0x02040050UL,0x00700005UL,0x00100820UL}},  /* MODULATEINVCOLOR_ADDALPHA: T140 */
};
static uint32_t f_creg(const uint32_t *w){return (w[0]>>16)&0xFF;}
static uint32_t f_csw(const uint32_t *w){return (w[0]>>4)&0xFFF;}
static uint32_t f_breg(const uint32_t *w){return (w[1]>>12)&0xFF;}
static uint32_t f_bsw(const uint32_t *w){return w[1]&0xFFF;}
static uint32_t f_areg(const uint32_t *w){return (w[2]>>8)&0xFF;}
static uint32_t f_asw(const uint32_t *w){return ((w[2]&0xFF)<<4)|(w[1]>>28);}
static void s_creg(uint32_t *w,uint32_t r){w[0]=(w[0]&~0xFF0000UL)|r<<16;}
static void s_csw(uint32_t *w,uint32_t z){w[0]=(w[0]&~0xFFF0UL)|z<<4;}
static void s_breg(uint32_t *w,uint32_t r){w[1]=(w[1]&~0xFF000UL)|r<<12;}
static void s_bsw(uint32_t *w,uint32_t z){w[1]=(w[1]&~0xFFFUL)|z;}
static void s_areg(uint32_t *w,uint32_t r){w[2]=(w[2]&~0xFF00UL)|r<<8;}
static void s_asw(uint32_t *w,uint32_t z){w[2]=(w[2]&~0xFFUL)|(z>>4);w[1]=(w[1]&0x0FFFFFFFUL)|(z&15)<<28;}
static int sw_const(uint32_t z){return z==0x924||z==0xB6D;}
static int reads(const uint32_t *w,uint32_t r) {
 return (f_areg(w)==r&&!sw_const(f_asw(w)))||(f_breg(w)==r&&!sw_const(f_bsw(w)))||(f_creg(w)==r&&!sw_const(f_csw(w)));
}
static uint32_t to_alpha_sw(uint32_t z){return (z==0x055||z==0x053||z==0x6DD)?0xB6B:z==0x050?0x6DB:z;}
/* D3DTA_* -> register; modifier bits -> CM / AR; 0xFF = unusable */
static uint32_t src_reg(uint32_t a,int bound,uint32_t *mods) {
 uint32_t s=a&15;*mods=((a&0x10)?CM:0)|((a&0x20)?AR:0);
 if(a&~0x3FUL)return 0xFF;
 if(s<=1)return XR_DIF;if(s==2)return bound?XR_TEX:XR_DIF;if(s==3)return XR_TF;if(s==4)return XR_SPE;return 0xFF;
}
static int commutative(uint32_t op){return op==4||op==5||op==6||op==7||op==8||op==9||op==11;}
/* v10.206: the texture register of the stage being built (RO_T) and the register BLENDCURRENTALPHA takes its alpha from (RO_D) */
static uint32_t g_treg=XR_TEX,g_dreg=XR_DIF;
static int is_tex(uint32_t r){return r<4;}
/* one instruction for op(x,y); alpha = 1 for the alpha form. 0 when not available */
static int build(uint32_t *w,uint32_t op,uint32_t x,uint32_t mx,uint32_t y,uint32_t my,int alpha) {
 uint32_t base=op,shift=0,k,swap;const xp_tpl *t=0;
 if(op==3){x=y;mx=my;op=base=2;} /* SELECTARG2 */
 if(op==5||op==6){shift=op==5?0x2000UL:0x4000UL;base=4;}
 if(op==9){shift=0x2000UL;base=8;}
 if(op==16)base=12; /* BLENDCURRENTALPHA: CURRENT is the diffuse colour at stage 0 */
 if(alpha){mx&=~AR;my&=~AR;}
 if(base==2){y=XR_NUL;my=0;}
 /* XP puts the texture first; commutative ops take either order */
 if(commutative(base)&&is_tex(y)&&!is_tex(x)){uint32_t r=x,m=mx;x=y;mx=my;y=r;my=m;}
 for(swap=0;swap<2&&!t;swap++) {
  for(k=0;k<sizeof(xp_tpls)/sizeof(xp_tpls[0]);k++)if(xp_tpls[k].op==base&&xp_tpls[k].m1==mx&&xp_tpls[k].m2==my){t=&xp_tpls[k];break;}
  if(!t&&!swap&&commutative(base)){uint32_t r=x,m=mx;x=y;mx=my;y=r;my=m;}else if(!t)break;
 }
 if(!t)return 0;
 for(k=0;k<4;k++)w[k]=t->w[k];
 {uint8_t ro[3];uint32_t f;ro[0]=t->ra;ro[1]=t->rb;ro[2]=t->rc;
  for(f=0;f<3;f++){uint32_t r;switch(ro[f]){case RO_X:r=x;break;case RO_Y:r=y;break;case RO_T:r=g_treg;break;case RO_D:r=op==16?g_dreg:XR_DIF;break;case RO_F:r=XR_TF;break;default:continue;}
   if(f==0)s_areg(w,r);else if(f==1)s_breg(w,r);else s_creg(w,r);}}
 w[3]|=shift;
 if(alpha) {
  if(f_areg(w)!=XR_NUL)s_asw(w,to_alpha_sw(f_asw(w)));
  if(f_breg(w)!=XR_NUL)s_bsw(w,to_alpha_sw(f_bsw(w)));
  if(f_creg(w)!=XR_NUL)s_csw(w,to_alpha_sw(f_csw(w)));
  w[2]=(w[2]&~0xF00000UL)|0x800000UL;w[3]|=0x04000000UL;
 }
 return 1;
}
static int op_ok(uint32_t op){return op==2||op==3||(op>=4&&op<=16)||(op>=18&&op<=21);}
static int scale_ok(uint32_t op,uint32_t s){return s<=2&&(!s||(op!=5&&op!=6&&op!=9));} /* v10.304 (below) */
/* v10.206: two and three texture stages, as XP lays them out (CLXP3D T116-T124, T138-T145, T149). Each stage's colour and alpha results
 * stay pending until something reads them: a later stage that reads CURRENT writes the pending instruction to temporary r0 first (colour
 * before alpha), the last stage's pending instructions write the output, and a value already in a register is copied out. Pass-through
 * SELECTARG(CURRENT) stages add nothing. Stage k's texture is the next free unit (register 0, 1, 2; TAU word 301001B0 + u * 01011001).
 * A write to a temporary saturates (w3 bit 12) unless it is a SELECT. Register last reads (XP's flags, checked against every capture):
 * A w2 bit 17, B w1 bit 21, C w0 bit 25, for textures and temporaries, once per register per instruction. DOTPRODUCT3 (T142): r0 =
 * 2 * (arg2 - 0.5), r1 = 2 * (arg1 - 0.5) with c22.y = -0.5, then dp3(r1, r0) into all four channels; a constant argument is copied to
 * the temporary first (one constant per instruction). PREMODULATE (T144) selects arg1 and the next stage's texture multiplies it. */
#define XR_R0 0x0CUL
#define XR_R1 0x0DUL
static int is_live(uint32_t r){return r<4||(r>=0x0C&&r<=0x0F);}
/* v10.208: the second register bank C0-C3 (destination 42 + 4i; T73, T125, T141) counts in the A operand and in a non-SELECT B
 * operand; a SELECT (w3 op 0D) ignores B and C, and a C0 in C is the templates' "none" */
static int is_cbank(uint32_t r){return r>=0xC0&&r<=0xC3;}
static int reads_c(const uint32_t *w,uint32_t r) {return f_areg(w)==r||(f_breg(w)==r&&((w[3]>>16)&15)!=0xD&&!sw_const(f_bsw(w)));}
static void last_reads(uint32_t *w,uint32_t n) {
 uint32_t i,j,k,d;
 for(i=0;i<n;i++){uint32_t *a=w+i*4,r[3],z[3],done[3],nd=0;
  a[2]&=~0x20000UL;a[1]&=~0x200000UL;a[0]&=~0x2000000UL;
  r[0]=f_areg(a);z[0]=f_asw(a);r[1]=f_breg(a);z[1]=f_bsw(a);r[2]=f_creg(a);z[2]=f_csw(a);
  for(k=0;k<3;k++){int later=0,cb=is_cbank(r[k]);
   if(cb?k!=0:(!is_live(r[k])||sw_const(z[k])))continue;
   for(d=0;d<nd;d++)if(done[d]==r[k])break;
   if(d<nd)continue;
   for(j=i+1;j<n&&!later;j++)if(cb?reads_c(w+j*4,r[k]):reads(w+j*4,r[k]))later=1;
   if(later)continue;
   done[nd++]=r[k];
   if(k==0)a[2]|=0x20000UL;else if(k==1)a[1]|=0x200000UL;else a[0]|=0x2000000UL;}}
}
typedef struct ms {uint32_t pw[12][4],pdp[12],np,out[32],no,rc,ra;int pc,pa,ovf;} ms;
static void ms_emit(ms *m,const uint32_t *w,int temp,int dp3) {
 uint32_t k;
 if(m->no>=8){m->ovf=1;return;}
 for(k=0;k<4;k++)m->out[m->no*4+k]=w[k];
 if(temp){uint32_t *o=m->out+m->no*4;o[3]=(o[3]&~0xFF0UL)|0xC20UL|((f_creg(o)!=XR_NUL||dp3)?0x1000UL:0);}
 m->no++;
}
static void ms_mat_c(ms *m) {
 int i=m->pc;if(i<0)return;
 ms_emit(m,m->pw[i],1,(int)m->pdp[i]);m->rc=XR_R0;m->pc=-1;
 if(m->pa==i){m->pa=-1;m->ra=XR_R0;}
}
static void ms_mat_a(ms *m) {
 int i=m->pa;if(i<0)return;
 if(m->pc>=0&&m->pc!=i&&reads(m->pw[m->pc],XR_R0))ms_mat_c(m); /* the pending colour reads r0.w: it goes first */
 if(m->pa<0)return;
 ms_emit(m,m->pw[i],1,(int)m->pdp[i]);m->ra=XR_R0;m->pa=-1;
 if(m->pc==i){m->pc=-1;m->rc=XR_R0;}
}
static int ms_add(ms *m,const uint32_t *w,int dp3) {uint32_t k;if(m->np>=12){m->ovf=1;return 0;}for(k=0;k<4;k++)m->pw[m->np][k]=w[k];m->pdp[m->np]=(uint32_t)dp3;return (int)m->np++;}
static int is_cur(uint32_t a){return (a&15)==1;}
/* DOT3's signed expansion of register r into temporary t: t = 2 * (r - 0.5) (T142); a constant is copied to t first */
/* v10.317: rgb = the expansion writes R, G, B only (GL_DOT3_RGB: the temporary's alpha, CURRENT's when it is there, is kept) */
static void ms_expand(ms *m,uint32_t r,uint32_t t,int rgb) {
 static const uint32_t sel[4]={0x01C00530UL,0x501C0053UL,0x00F00005UL,0x000D0C20UL},expand[4]={0x00962490UL,0x30140924UL,0x00F00005UL,0x00002C20UL};
 uint32_t w[4],k;
 if(r>=0x80){for(k=0;k<4;k++)w[k]=sel[k];s_areg(w,r);s_asw(w,0x053);w[3]|=t*0x40UL;if(rgb)w[2]=(w[2]&~0xF00000UL)|0x700000UL;ms_emit(m,w,0,0);r=XR_R0+t;}
 for(k=0;k<4;k++)w[k]=expand[k];s_areg(w,r);w[3]|=t*0x40UL;if(rgb)w[2]=(w[2]&~0xF00000UL)|0x700000UL;ms_emit(m,w,0,0);
}
static int pass(uint32_t op,uint32_t a1,uint32_t a2){return (op==2&&a1==1)||(op==3&&a2==1);}
/* argument register: diffuse, CURRENT (colour or, for alpha / ALPHAREPLICATE, alpha), the stage's texture (diffuse when unbound), TFACTOR, specular */
static uint32_t ms_reg(const ms *m,uint32_t a,uint32_t treg,int alpha,uint32_t *mods) {
 uint32_t s=a&15;*mods=((a&0x10)?CM:0)|((a&0x20)?AR:0);
 if(a&~0x3FUL)return 0xFF;
 if(s==0)return XR_DIF;if(s==1)return (alpha||(*mods&AR))?m->ra:m->rc;if(s==2)return treg!=0xFF?treg:XR_DIF;if(s==3)return XR_TF;if(s==4)return XR_SPE;
 return 0xFF;
}
/* v10.208: bump mapping as XP lays it out (CLXP3D T146 BUMPENVMAP, T147/T148 BUMPENVMAPLUMINANCE): TAU 0 reads the du/dv texel, the
 * coordinate instructions put texture 1's coordinates in C0 (C1 with luminance; matrix c11 = BUMPENVMAT00 01 10 11, luminance c35 =
 * LSCALE, LOFFSET), TAU 1 reads texture 1 into t0, and the luminance form scales it by C0.z. Stage 1 then runs on that texel: its
 * colour and alpha ops with TEXTURE = t0, CURRENT and DIFFUSE = diffuse (one instruction each, no third stage). */
static int compose_bump(const vx_d3d_context *c,int bound0,vx_d3d_xp *x,uint32_t *err) {
 static const uint32_t pre0[8]={0x02C10530UL,0x0008B092UL,0x00300000UL,0x00000C20UL,0x020C06C0UL,0x9008B2EDUL,0x00F00024UL,0x00000420UL};
 static const uint32_t pre1[16]={0x00C10530UL,0x0008B092UL,0x00300000UL,0x00000C20UL,0x00A3B4D0UL,0x500A3B45UL,0x004000B5UL,0x00001420UL,
  0x020C06C0UL,0x9008B2EDUL,0x00F00024UL,0x00000460UL,0x0140B6D0UL,0x502C0495UL,0x00700005UL,0x00000020UL};
 const uint32_t *t0=c->state.tss[0],*t1=c->state.tss[1];uint32_t lum=t0[1]==23,n=lum?4:2,k,op=t1[1],aop=t1[4],cx,cy,ax,ay,mcx=0,mcy=0,max=0,may=0,w[4];
 if(!bound0||!t1[0]||t1[1]==1||c->state.tss[2][1]!=1||(t0[2]&15)!=2){*err=VX_D3D_COLOROP;return -2;}
 for(k=0;k<n*4;k++)x->words[k]=lum?pre1[k]:pre0[k];
 g_treg=XR_TEX;g_dreg=XR_DIF;
 if(!op_ok(op)||(aop!=1&&(!op_ok(aop)||aop>=18))){*err=VX_D3D_COLOROP;return -2;}
 /* stage 1: TEXTURE = the bumped texel (t0), CURRENT = DIFFUSE (the bump stage passes the diffuse colour); stage 0's alpha is diffuse */
 {uint32_t a1=t1[2],a2=t1[3],b1=t1[5],b2=t1[6];
  if(aop==1){aop=2;b1=0;}
  cx=src_reg((a1&15)==1?(a1&~15UL):a1,1,&mcx);cy=src_reg((a2&15)==1?(a2&~15UL):a2,1,&mcy);ax=src_reg((b1&15)==1?(b1&~15UL):b1,1,&max);ay=src_reg((b2&15)==1?(b2&~15UL):b2,1,&may);}
 if(op==3){cx=cy;mcx=mcy;op=2;}
 if(aop==3){ax=ay;max=may;aop=2;}
 max&=~AR;may&=~AR;
 if(cx==0xFF||ax==0xFF||(op!=2&&cy==0xFF)||(aop!=2&&ay==0xFF)){*err=VX_D3D_COLORARG;return -2;}
 if(!build(w,op,cx,mcx,cy,mcy,0)){*err=VX_D3D_COLOROP;return -2;}
 for(k=0;k<4;k++)x->words[n*4+k]=w[k];
 if(!build(w,aop,ax,max,ay,may,1)){*err=VX_D3D_ALPHAOP;return -2;}
 for(k=0;k<4;k++)x->words[n*4+4+k]=w[k];
 last_reads(x->words+n*4,2); /* the coordinate instructions keep XP's flags */
 x->count=n+2;x->textured=1;x->units=2;x->unit_stage[0]=0;x->unit_stage[1]=1;x->unit_set[0]=t0[11]&0xFFFF;x->unit_set[1]=c->state.tss[1][11]&0xFFFF;
 for(k=0;k<x->count;k++)if(reads(x->words+k*4,XR_TF))x->tfactor=1;
 x->ps=1;x->ntau=2;x->h90=lum?0x18001UL:0x18002UL;x->h91=lum?3:1;x->h93=3;x->h95=lum?4:2;
 x->tau[0]=lum?0x301001B0UL:0x201001B0UL;x->tau[1]=lum?0x311011B1UL:0x311001B1UL;
 x->nkc=lum?2:1;x->kc_reg[0]=11;for(k=0;k<4;k++)x->kc[0][k]=t0[7+k];
 if(lum){x->kc_reg[1]=35;x->kc[1][0]=t0[22];x->kc[1][1]=t0[23];x->kc[1][2]=0;x->kc[1][3]=0;}
 return 0;
}
static int compose_multi(const vx_d3d_context *c,int bound0,vx_d3d_xp *x,uint32_t *err) {
 static const uint32_t spec_add[4]={0x00410550UL,0x50140924UL,0x00700005UL,0x00000820UL};
 static const uint32_t dp3[4]={0x01C00530UL,0x3000C053UL,0x00F00D05UL,0x00020820UL};
 ms m;uint32_t ns,s,k,w[4],units=0,premod=0;int spec=c->state.rs[29]!=0;
 memset(&m,0,sizeof(m));m.pc=m.pa=-1;m.rc=m.ra=XR_DIF;
 for(ns=0;ns<8&&c->state.tss[ns][1]!=1;ns++);
 if(ns>3){*err=VX_D3D_TOOMANYOPS;return -2;}
 for(s=0;s<ns;s++) {
  const uint32_t *t=c->state.tss[s];uint32_t op=t[1],aop=t[4],a1=t[2],a2=t[3],b1=t[5],b2=t[6],treg=0xFF,cx,cy,ax,ay,mcx=0,mcy=0,max=0,may=0;
  int bound=s?t[0]!=0:bound0,usestex,last=s+1==ns,cpass,apass;uint32_t cs=t[VX_D3D_TSS_CSCALE],as=t[VX_D3D_TSS_ASCALE];
  int d3rgb=op==24&&t[VX_D3D_TSS_DOT3RGB]==1; /* v10.317: GL_DOT3_RGB (the ICD's; Direct3D's DOTPRODUCT3 writes alpha too) */
  if(s&&!bound&&(a1&15)==2&&op!=3)break; /* a NULL texture as COLORARG1 ends the cascade, as at stage 0 */
  if(aop==1){aop=3;b2=1;} /* alpha DISABLE under an enabled colour: CURRENT */
  if(op==17&&!last)premod=2;else if(op==17)op=2;
  if(op==17)op=2;
  if((!op_ok(op)&&op!=24)||op==1){*err=VX_D3D_COLOROP;return -2;}
  if((op!=24||d3rgb)&&(!op_ok(aop)||aop>=18)){*err=VX_D3D_ALPHAOP;return -2;}
  if(!scale_ok(op,cs)||!scale_ok(aop,as)||(op==24&&!d3rgb&&cs!=as)){*err=VX_D3D_COLOROP;return -2;} /* v10.304 */
  /* v10.319 (audit C2): only the arguments the operation reads: SELECTARG2 (3) not ARG1, SELECTARG1 (2) not ARG2, DOTPRODUCT3 no alpha
   * arguments (its result is the alpha too); an unread TEXTURE took a unit and could leave the draw out (three units). BLENDTEXTUREALPHA(PM)
   * reads the texture whatever its arguments, as an alpha op too (review: a SELECTARG1 colour lost that unit) */
  usestex=bound&&((op!=3&&(a1&15)==2)||(op!=2&&(a2&15)==2)||((op!=24||d3rgb)&&((aop!=3&&(b1&15)==2)||(aop!=2&&(b2&15)==2)||aop==13||aop==15))||op==13||op==15||premod==1);
  if(usestex){if(units>=3){*err=VX_D3D_TOOMANYOPS;return -2;}treg=units;x->unit_stage[units]=s;x->unit_set[units]=t[11]&0xFFFF;units++;}
  if(premod==1) { /* the previous stage's PREMODULATE: CURRENT colour x this stage's texture */
   if(treg!=0xFF){ms_mat_c(&m);if(!build(w,4,treg,0,m.rc,0,0)){*err=VX_D3D_COLOROP;return -2;}m.pc=ms_add(&m,w,0);}
   premod=0;
  }
  if(premod==2)premod=1;
  cpass=op!=24&&pass(op,a1,a2);apass=(op==24&&!d3rgb)||pass(aop,b1,b2);
  if(cs&&op!=24)cpass=0;if(as&&(op!=24||d3rgb))apass=0; /* v10.304: a scaled pass-through is an instruction */
  /* what this stage reads of CURRENT */
  if(!cpass){if((op!=3&&is_cur(a1)&&!(a1&0x20))||(op!=2&&is_cur(a2)&&!(a2&0x20)))ms_mat_c(&m);
   if((op!=3&&is_cur(a1)&&(a1&0x20))||(op!=2&&is_cur(a2)&&(a2&0x20))||op==16)ms_mat_a(&m);}
  if(!apass&&((aop!=3&&is_cur(b1))||(aop!=2&&is_cur(b2))||aop==16))ms_mat_a(&m);
  if(m.ovf){*err=VX_D3D_TOOMANYOPS;return -2;}
  g_treg=treg!=0xFF?treg:XR_DIF;g_dreg=m.ra;
  if(op==24) { /* DOTPRODUCT3 into colour and alpha */
   uint32_t r1=ms_reg(&m,a1,treg,0,&mcx),r2=ms_reg(&m,a2,treg,0,&mcy);int one=0,na=-1;
   if(r1==0xFF||r2==0xFF||mcx||mcy){*err=VX_D3D_COLORARG;return -2;}
   /* v10.317: GL_DOT3_RGB. The expansions and the dot product write R, G, B only, so a temporary keeps its alpha (CURRENT's, when it was
    * put there). The alpha is the stage's own op (pending, read after the expansions: it reads only alpha) or, passed through, what was
    * pending: a previous colour-and-alpha instruction is first written to r0 (whose alpha the expansions keep). */
   if(d3rgb){
    if(!apass){uint32_t ax=ms_reg(&m,b1,treg,1,&max),ay=ms_reg(&m,b2,treg,1,&may);
     if(aop==3){ax=ay;max=may;aop=2;}
     max&=~AR;may&=~AR;
     if(ax==0xFF||(aop!=2&&ay==0xFF)){*err=VX_D3D_ALPHAARG;return -2;}
     if(!build(w,aop,ax,max,ay,may,1)){*err=VX_D3D_ALPHAOP;return -2;}
     w[3]|=as<<13;na=ms_add(&m,w,0);}
    else {if(m.pa>=0&&m.pa==m.pc)ms_mat_a(&m);na=m.pa;}
    if(m.ovf){*err=VX_D3D_TOOMANYOPS;return -2;}
   }
   if(r1==r2){ms_expand(&m,r1,0,d3rgb);r1=r2=XR_R0;one=1;}
   else {
    if(r1==XR_R0){ms_expand(&m,r1,1,d3rgb);r1=XR_R1;one=1;} /* arg1 is CURRENT in r0: before r0 is reused */
    ms_expand(&m,r2,0,d3rgb);r2=XR_R0;
    if(!one)ms_expand(&m,r1,1,d3rgb);
    r1=XR_R1;
   }
   for(k=0;k<4;k++)w[k]=dp3[k];s_areg(w,r1);w[3]|=cs<<13;
   if(d3rgb){w[2]=(w[2]&~0xF00000UL)|0x700000UL;m.pc=ms_add(&m,w,1);m.pa=na;}
   else m.pc=m.pa=ms_add(&m,w,1);
   x->c22=1;
   if(m.ovf){*err=VX_D3D_TOOMANYOPS;return -2;}
   continue;
  }
  cx=ms_reg(&m,a1,treg,0,&mcx);cy=ms_reg(&m,a2,treg,0,&mcy);ax=ms_reg(&m,b1,treg,1,&max);ay=ms_reg(&m,b2,treg,1,&may);
  if(op==3){cx=cy;mcx=mcy;op=2;}
  if(aop==3){ax=ay;max=may;aop=2;}
  max&=~AR;may&=~AR;
  if(!cpass&&(cx==0xFF||(op!=2&&cy==0xFF))){*err=VX_D3D_COLORARG;return -2;}
  if(!apass&&(ax==0xFF||(aop!=2&&ay==0xFF))){*err=VX_D3D_ALPHAARG;return -2;}
  if(!cpass&&!apass&&!(last&&spec)&&op==aop&&cx==ax&&!mcx&&!max&&cs==as&&(op==2||((op==4||op==7)&&cy==ay&&!mcy&&!may))) {
   if(!build(w,op,cx,0,cy,0,0)){*err=VX_D3D_COLOROP;return -2;}
   if(f_areg(w)!=XR_NUL&&f_asw(w)==0x055)s_asw(w,0x053);
   if(f_breg(w)!=XR_NUL&&f_bsw(w)==0x055)s_bsw(w,0x053);
   if(f_creg(w)!=XR_NUL&&f_csw(w)==0x055)s_csw(w,0x053);
   w[2]|=0xF00000UL;w[3]|=cs<<13;m.pc=m.pa=ms_add(&m,w,0);
  } else {
   int nc=m.pc,na=m.pa;
   if(!cpass){if(!build(w,op,cx,mcx,cy,mcy,0)){*err=VX_D3D_COLOROP;return -2;}w[3]|=cs<<13;nc=ms_add(&m,w,0);}
   if(!apass){if(!build(w,aop,ax,max,ay,may,1)){*err=VX_D3D_ALPHAOP;return -2;}w[3]|=as<<13;na=ms_add(&m,w,0);}
   if(!cpass&&m.pc>=0&&m.pc==m.pa&&apass){ms_mat_a(&m);na=m.pa;} /* a folded pending pair split by this stage keeps its alpha */
   m.pc=nc;m.pa=na;
  }
  if(m.ovf){*err=VX_D3D_TOOMANYOPS;return -2;}
 }
 /* the output: colour (through r0 and + specular with SPECULARENABLE), then alpha */
 if(!units&&m.pc<0&&m.pa<0&&m.rc==XR_DIF&&m.ra==XR_DIF&&!spec)return -1;
 {int folded=m.pc>=0&&m.pc==m.pa;
  if(spec){if(m.pc>=0){ms_mat_c(&m);folded=0;}for(k=0;k<4;k++)w[k]=spec_add[k];s_areg(w,m.rc);ms_emit(&m,w,0,0);}
  else if(m.pc>=0)ms_emit(&m,m.pw[m.pc],0,(int)m.pdp[m.pc]);
  else {if(!build(w,2,m.rc,0,XR_NUL,0,0)){*err=VX_D3D_COLOROP;return -2;}ms_emit(&m,w,0,0);}
  if(!folded||spec){
   if(m.pa>=0)ms_emit(&m,m.pw[m.pa],0,0);
   else {if(!build(w,2,m.ra,0,XR_NUL,0,1)){*err=VX_D3D_ALPHAOP;return -2;}ms_emit(&m,w,0,0);}
  }}
 if(m.ovf){*err=VX_D3D_TOOMANYOPS;return -2;}
 last_reads(m.out,m.no);
 x->count=m.no;for(k=0;k<m.no*4;k++)x->words[k]=m.out[k];
 x->units=units;x->textured=units>0;
 for(k=0;k<m.no;k++)if(reads(x->words+k*4,XR_TF))x->tfactor=1;
 g_treg=XR_TEX;g_dreg=XR_DIF;
 return 0;
}
/* v10.198: the stage-0 program. Returns -2 (state not drawable, *err set), -1 (plain diffuse: the milestone-1 path) or 0 (x filled). */
static int compose_core(const vx_d3d_context *c,int bound,vx_d3d_xp *x,uint32_t *err);
/* v10.208: TEXTURETRANSFORMFLAGS (24) COUNT3 | PROJECTED on the only unit: XP's projected read (T151: TAU 307001A0, s t q vertex) */
/* v10.212: feature switches from the registry (VX_F_*, all on by default): a switched-off op is not drawable (ValidateDevice says so and
 * the caps do not offer it); with ACCEL_D3D_STAGES = 0 stage 1 counts as disabled */
uint32_t vx_d3d_feat=0xFF87FFFFUL; /* v10.362: 19 (CLEAR3D) off by default */ /* v10.354: the opt-in bits 20-22 off; v10.358: and 23 (ACCEL_3D_GUARD); v10.361: 23 on (its default, with vx3dn_guard) */
/* .533: .531's kept programs (CM_N entries keyed by the stages) removed: board, GTA3 on .532, composing 1.8 against 2.2 microseconds a build
 * and nothing seen in the frame */
int vx_d3d_compose(const vx_d3d_context *c,int bound,vx_d3d_xp *x,uint32_t *err) {
 int r,s;uint32_t save=c->state.tss[1][1];
 for(s=0;s<3&&c->state.tss[s][1]!=1;s++){uint32_t op=c->state.tss[s][1];
  if(s&&!(vx_d3d_feat&VX_F_STAGES))break;
  if((op==22||op==23)&&(!(vx_d3d_feat&VX_F_BUMP)||!(vx_d3d_feat&VX_F_STAGES))){memset(x,0,sizeof(*x));*err=VX_D3D_COLOROP;return -2;}
  if((op==15||op==17||op==20||op==21||op==24)&&!(vx_d3d_feat&VX_F_NEWOPS)){memset(x,0,sizeof(*x));*err=VX_D3D_COLOROP;return -2;}}
 if(!(vx_d3d_feat&VX_F_STAGES))((vx_d3d_context *)c)->state.tss[1][1]=1;
 r=compose_core(c,bound,x,err);
 ((vx_d3d_context *)c)->state.tss[1][1]=save;
 if(r==0&&x->units==1&&!x->ps&&(vx_d3d_feat&VX_F_PROJECTED)&&c->state.tss[x->unit_stage[0]][24]==0x103UL)x->layout=2;
 return r;
}
/* v10.304: the stage's result scale (the OpenGL ICD's GL_RGB_SCALE / GL_ALPHA_SCALE as 1 = x2, 2 = x4: VX_D3D_TSS_CSCALE / ASCALE,
 * Direct3D leaves them 0): word 3 bits 14:13, as MODULATE2X / 4X carry it (cookbook 16.3). Not on an op that already scales. */
static void finish_prog(vx_d3d_xp *x,const uint32_t *t,uint32_t n) {
 uint32_t k,last=0xFF;
 x->count=n;
 for(k=0;k<n;k++)if(reads(x->words+k*4,XR_TEX))last=k;
 if(last!=0xFF){last_reads(x->words,n);x->textured=1;x->units=1;x->unit_set[0]=t[11]&0xFFFF;}
 for(k=0;k<n;k++)if(reads(x->words+k*4,XR_TF))x->tfactor=1;
}
/* v10.304: D3DTOP_LERP (Direct3D 8's 26: arg0 x arg1 + (1 - arg0) x arg2, arg0 in VX_D3D_TSS_CARG0 / AARG0) on stage 0 alone, for the
 * OpenGL ICD (GL_BLEND with any environment colour, GL_INTERPOLATE by a colour).
 * v10.306 (board, v10.304 and v10.305: GL_BLEND on INTENSITY with another colour, colour and alpha both LERP, hung the engine every
 * draw; a colour LERP alone drew right; moving the alpha's temporary from r0 to r1 changed nothing): one instruction an op, the engine's
 * own interpolation, w3 op 11 as XP builds BLENDTEXTUREALPHA (T10), BLENDDIFFUSEALPHA (T11) and BLENDCURRENTALPHA (T119):
 * A x C + B x (1 - C), A = arg1, B = arg2, C = arg0 (colour swizzle 050, or 6DB for its alpha); a complemented arg0 swaps A and B.
 * No temporary and no complement. What only the hung program had (black-environment INTENSITY, the same T104 alpha MODULATE written
 * to the output, drew right): the T104 alpha form written to a temporary, TFACTOR in B of an alpha instruction, a temporary read in C
 * of one, and a temporary written after the output (temp, colour out, temp, alpha out; XP writes the output last). VXDX7 v2.76 runs the
 * first three alone through Direct3D, and this form's alpha instruction (BLENDDIFFUSEALPHA with TFACTOR in A). */
static int lerp_ins(uint32_t *w,uint32_t a0,uint32_t a1,uint32_t a2,int bound,int alpha,uint32_t scale) {
 static const uint32_t tpl[4]={0x00006DB0UL,0x00040050UL,0x00700005UL,0x00110820UL}; /* T10 */
 uint32_t r0,r1,r2,m0,m1,m2,t,k,nk;
 r0=src_reg(a0,bound,&m0);r1=src_reg(a1,bound,&m1);r2=src_reg(a2,bound,&m2);
 if(alpha){m0&=~AR;m1&=~AR;m2&=~AR;}
 if(r0==0xFF||r1==0xFF||r2==0xFF||(m1&CM)||(m2&CM))return 0;
 if(m0&CM){t=r1;r1=r2;r2=t;t=m1;m1=m2;m2=t;} /* lerp(1 - f, a, b) = lerp(f, b, a) */
 nk=(r0>=0x80&&r0<0xC0)+((r1>=0x80&&r1<0xC0)&&r1!=r0)+((r2>=0x80&&r2<0xC0)&&r2!=r0&&r2!=r1);
 if(nk>1)return 0; /* one constant an instruction */
 for(k=0;k<4;k++)w[k]=tpl[k];
 s_areg(w,r1);s_breg(w,r2);s_creg(w,r0);
 s_asw(w,(m1&AR)?0x6DBUL:0x050UL);s_bsw(w,(m2&AR)?0x6DBUL:0x050UL);s_csw(w,(m0&AR)?0x6DBUL:0x050UL);
 if(alpha){s_asw(w,0x6DBUL);s_bsw(w,0x6DBUL);s_csw(w,0x6DBUL);w[2]=(w[2]&~0xF00000UL)|0x800000UL;w[3]|=0x04000000UL;}
 w[3]|=scale<<13;
 return 1;
}
static int compose_lerp(const vx_d3d_context *c,int bound,vx_d3d_xp *x,uint32_t *err) {
 const uint32_t *t=c->state.tss[0];uint32_t op=t[1],aop=t[4],cs=t[VX_D3D_TSS_CSCALE],as=t[VX_D3D_TSS_ASCALE],n=0,cx,cy,mcx=0,mcy=0;
 g_treg=XR_TEX;g_dreg=XR_DIF;
 if(c->state.rs[29]||cs>2||as>2){*err=VX_D3D_COLOROP;return -2;}
 if(op==26){if(!lerp_ins(x->words,t[VX_D3D_TSS_CARG0],t[2],t[3],bound,0,cs)){*err=VX_D3D_COLORARG;return -2;}n=1;}
 else {
  if(op==1){op=2;cx=cy=XR_DIF;}else{if(!op_ok(op)){*err=VX_D3D_COLOROP;return -2;}cx=src_reg(t[2],bound,&mcx);cy=src_reg(t[3],bound,&mcy);}
  if(op==3){cx=cy;mcx=mcy;op=2;}
  if(cx==0xFF||(op!=2&&cy==0xFF)||!scale_ok(op,cs)){*err=VX_D3D_COLORARG;return -2;}
  if(!build(x->words,op,cx,mcx,cy,mcy,0)){*err=VX_D3D_COLOROP;return -2;}
  x->words[3]|=cs<<13;n=1;
 }
 if(aop==26){if(!lerp_ins(x->words+n*4,t[VX_D3D_TSS_AARG0],t[5],t[6],bound,1,as)){*err=VX_D3D_ALPHAARG;return -2;}n++;}
 else {
  uint32_t ax,ay,max=0,may=0;
  if(aop==1){aop=2;ax=ay=XR_DIF;}else{if(!op_ok(aop)){*err=VX_D3D_ALPHAOP;return -2;}ax=src_reg(t[5],bound,&max);ay=src_reg(t[6],bound,&may);}
  if(aop==3){ax=ay;max=may;aop=2;}
  max&=~AR;may&=~AR;
  if(ax==0xFF||(aop!=2&&ay==0xFF)||!scale_ok(aop,as)){*err=VX_D3D_ALPHAARG;return -2;}
  if(!build(x->words+n*4,aop,ax,max,ay,may,1)){*err=VX_D3D_ALPHAOP;return -2;}
  x->words[n*4+3]|=as<<13;n++;
 }
 finish_prog(x,t,n);
 return 0;
}
static int compose_core(const vx_d3d_context *c,int bound,vx_d3d_xp *x,uint32_t *err) {
 const uint32_t *t=c->state.tss[0];uint32_t op=t[1],aop=t[4],cx,cy,ax,ay,mcx=0,mcy=0,max=0,may=0,n=0,k,last=0xFF,cs=t[VX_D3D_TSS_CSCALE],as=t[VX_D3D_TSS_ASCALE];int spec=c->state.rs[29]!=0,fold;
 *err=0;memset(x,0,sizeof(*x));
 /* NULL texture + COLORARG1 TEXTURE terminates this stage and the cascade (before validating arguments or stage 1).
  * v10.335 (review #196; board, VXPROBE stages: SELECTARG2 DIFFUSE with ALPHAOP SELECTARG1 TFACTOR drew the diffuse alpha when the
  * unread ARG1 was TEXTURE, the TFACTOR alpha when it was TFACTOR): only when the colour operation reads ARG1, as at stages 1 and 2 */
 if(!bound&&t[2]==2&&op!=3)op=1;
 if(op==22||op==23)return compose_bump(c,bound,x,err); /* v10.208 */
 if(op!=1&&(c->state.tss[1][1]!=1||op==24))return compose_multi(c,bound,x,err); /* v10.206 (DOT3 too); a disabled stage 0 ends the cascade */
 if(op==17)op=2; /* v10.206: PREMODULATE with no next stage */
 if(op==26||(op!=1&&aop==26))return compose_lerp(c,bound,x,err); /* v10.304 */
 g_treg=XR_TEX;g_dreg=XR_DIF;
 if(op==1){op=2;cx=XR_DIF;cy=XR_DIF;aop=2;ax=ay=XR_DIF;cs=as=0;}
 else {
  if(!op_ok(op)){*err=VX_D3D_COLOROP;return -2;}
  cx=src_reg(t[2],bound,&mcx);cy=src_reg(t[3],bound,&mcy);
  if((op!=3&&cx==0xFF)||(op!=2&&cy==0xFF)){*err=VX_D3D_COLORARG;return -2;}
  if(aop==1){aop=2;ax=ay=XR_DIF;}
  else {
   if(!op_ok(aop)){*err=VX_D3D_ALPHAOP;return -2;}
   ax=src_reg(t[5],bound,&max);ay=src_reg(t[6],bound,&may);
   if((aop!=3&&ax==0xFF)||(aop!=2&&ay==0xFF)){*err=VX_D3D_ALPHAARG;return -2;}
  }
 }
 if(op==3){cx=cy;mcx=mcy;op=2;}
 if(aop==3){ax=ay;max=may;aop=2;}
 max&=~AR;may&=~AR;
 /* plain diffuse, no specular: the milestone-1 path */
 if(!scale_ok(op,cs)||!scale_ok(aop,as)){*err=VX_D3D_COLOROP;return -2;} /* v10.304 */
 if(op==2&&cx==XR_DIF&&!mcx&&aop==2&&ax==XR_DIF&&!max&&!spec&&!cs&&!as)return -1;
 fold=!spec&&op==aop&&cx==ax&&!mcx&&!max&&cs==as&&(op==2||((op==4||op==7)&&cy==ay&&!mcy&&!may));
 if(fold) {
  if(!build(x->words,op,cx,0,cy,0,0)){*err=VX_D3D_COLOROP;return -2;}
  if(f_areg(x->words)!=XR_NUL&&f_asw(x->words)==0x055)s_asw(x->words,0x053);
  if(f_breg(x->words)!=XR_NUL&&f_bsw(x->words)==0x055)s_bsw(x->words,0x053);
  if(f_creg(x->words)!=XR_NUL&&f_csw(x->words)==0x055)s_csw(x->words,0x053);
  x->words[2]|=0xF00000UL;x->words[3]|=cs<<13;n=1;
 } else {
  uint32_t *w=x->words;
  if(!build(w,op,cx,mcx,cy,mcy,0)){*err=VX_D3D_COLOROP;return -2;}
  w[3]|=cs<<13;n=1;
  if(spec) {
   if(op==2&&!mcx&&!cs){w[0]=0x00410550UL;w[1]=0x50140924UL;w[2]=0x00700005UL;w[3]=0x00000820UL;s_areg(w,cx);} /* XP T14 / T94 / T95; v10.319 (audit C1): not with a scale (the add would replace it) */
   else {w[3]=(w[3]&~0xF00UL)|0xC00UL|(f_creg(w)!=XR_NUL?0x1000UL:0); /* to the temporary (T15, T89-T93) */
    w+=4;w[0]=0x00410550UL;w[1]=0x50140924UL;w[2]=0x00720C05UL;w[3]=0x00000820UL;n=2;} /* temporary + specular: its last read */
  }
  if(!build(x->words+n*4,aop,ax,max,ay,may,1)){*err=VX_D3D_ALPHAOP;return -2;}
  x->words[n*4+3]|=as<<13;n++;
 }
 x->count=n;
 for(k=0;k<n;k++)if(reads(x->words+k*4,XR_TEX))last=k;
 /* v10.303 (board, VXGL v3.2 scene 27): the texture's last read marked on the operand that reads it, as for two and three stages
  * (last_reads: A w2 bit 17, B w1 bit 21, C w0 bit 25). It was A's mark whatever read it: MODULATE(TEXTURE | COMPLEMENT, DIFFUSE), which
  * takes the texture as B (the template captured with the complement on B), drew garbage across each triangle (GL_BLEND, black
  * environment, LUMINANCE and INTENSITY). Every XP capture reads the texture last as A, which it marks the same. */
 if(last!=0xFF){last_reads(x->words,n);x->textured=1;x->units=1;x->unit_set[0]=t[11]&0xFFFF;}
 for(k=0;k<n;k++)if(reads(x->words+k*4,XR_TF))x->tfactor=1;
 return 0;
}
/* the earlier interface: -2 / -1 as above, otherwise 0 */
int vx_d3d_program(const vx_d3d_context *c,int bound,uint32_t *err) {vx_d3d_xp x;int r=vx_d3d_compose(c,bound,&x,err);return r<0?r:0;}
/* v10.215: what primitive() would leave out with these states and textures, for ValidateDevice: units on more than two coordinate sets
 * (drop reason 200), a cube map other than on the only unit (1000). Returns 0 or VX_D3D_CONFLICTINGSTATE. */
static uint32_t template_status(const vx_d3d_context *,const vx_d3d_services *);
uint32_t vx_d3d_draw_status(const vx_d3d_context *c,const vx_d3d_services *sv) {
 vx_d3d_xp x;uint32_t e,u,set,set1,fr,f6[6];int prog=vx_d3d_compose(c,c->state.tss[0][0]!=0,&x,&e);
 if(sv&&sv->resolve)return template_status(c,sv); /* validation uses the same texture/key/mip decisions as drawing */
 if(prog!=0||!x.textured)return 0;
 set=x.unit_set[0];set1=set;
 for(u=1;u<x.units;u++){if(x.unit_set[u]==set)continue;if(set1==set||x.unit_set[u]==set1)set1=x.unit_set[u];else return VX_D3D_CONFLICTINGSTATE;}
 if(!sv||!sv->faces)return 0;
 fr=sv->faces(sv->context,c->state.tss[x.unit_stage[0]][0],f6)&~VX_D3D_FACES_SHARED;
 /* v10.321: a cube map with a second unit, or on unit 1 (ACCEL_D3D_UNIT3D: two units) */
 {int two=(vx_d3d_feat&VX_F_UNIT3D)&&x.units==2;
  if(fr==6&&!((vx_d3d_feat&VX_F_CUBE)&&(x.units==1||two)&&!x.ps&&!x.layout))return VX_D3D_CONFLICTINGSTATE;
  if(fr&&fr!=6)return VX_D3D_CONFLICTINGSTATE;
  for(u=1;u<x.units;u++){uint32_t fu=sv->faces(sv->context,c->state.tss[x.unit_stage[u]][0],f6)&~VX_D3D_FACES_SHARED;
   if(fu&&(fu!=6||!two||!(vx_d3d_feat&VX_F_CUBE)||x.ps||x.layout==VX3DN_LAYOUT_PROJ))return VX_D3D_CONFLICTINGSTATE;}}
 return 0;
}
uint32_t vx_d3d_texture_status(const vx_d3d_context *c) {uint32_t e;(void)vx_d3d_program(c,c->state.tss[0][0]!=0,&e);return e;}
/* 1 when the chosen program samples the texture */
int vx_d3d_uses_texture(const vx_d3d_context *c) {
 uint32_t e;vx_d3d_xp x;if(!c->state.tss[0][0])return 0;
 return vx_d3d_compose(c,1,&x,&e)==0&&x.textured;
}
int vx_d3d_targets_valid(const vx3dn_surface *t,const vx3dn_surface *z) {
 uint32_t te,ze;
 if(!t||!VX3DN_IS_COLOR(t->format)|| /* v10.460: + A8R8G8B8 */
    !t->width||!t->height||!t->pitch||t->height>(0xFFFFFFFFUL-t->offset)/t->pitch||
    t->width>t->pitch/(t->format==VX3DN_RGB565?2UL:4UL))return 0; /* v10.330 (review #87): a row fits its pitch (the HAL's resolver checked; the ICD's surfaces come here unchecked) */
 if(!z||!z->format)return 1;
 if(!VX3DN_IS_DEPTH(z->format)||z->width<t->width||z->height<t->height||
    !z->pitch||z->height>(0xFFFFFFFFUL-z->offset)/z->pitch||z->width>z->pitch/(z->format==VX3DN_D16?2UL:4UL))return 0;
 te=t->offset+t->pitch*t->height;ze=z->offset+z->pitch*z->height;
 return t->offset>=ze||z->offset>=te;
}
/* v10.177: D3DBLEND pair as the packet takes it; BOTHSRCALPHA / BOTHINVSRCALPHA set both factors */
static int blend_pair(uint32_t s,uint32_t d,uint32_t *bs,uint32_t *bd) {
 if(s==12){*bs=5;*bd=6;return 1;}
 if(s==13){*bs=6;*bd=5;return 1;}
 /* v10.304: 14 / 15 BLENDFACTOR / INVBLENDFACTOR (Direct3D 9's numbering) and the ICD's 16 / 17 (the factor's alpha and its inverse) */
 if(s<1||s>17||s==12||s==13||d<1||d>17||d==11||d==12||d==13)return 0;
 *bs=s;*bd=d;return 1;
}
/* v10.304: VX_D3D_RS_BLENDOP (Direct3D 8's D3DBLENDOP: 1 ADD, 2 SUBTRACT, 3 REVSUBTRACT, 4 MIN, 5 MAX; 0 = ADD) -> the packet's equation */
static int blend_op(uint32_t o,uint32_t *op) {static const uint32_t m[6]={VX3DN_BLENDOP_ADD,VX3DN_BLENDOP_ADD,VX3DN_BLENDOP_SUB,VX3DN_BLENDOP_REVSUB,VX3DN_BLENDOP_MIN,VX3DN_BLENDOP_MAX};if(o>5)return 0;*op=m[o];return 1;}
/* the side's constant for factor f (24 bits, R << 16 | G << 8 | B): the blend factor's colour, or its alpha in all three */
static uint32_t blend_k(uint32_t f,uint32_t argb) {return f==14||f==15?argb&0xFFFFFFUL:f==16||f==17?(argb>>24)*0x010101UL:0;}
/* v10.177: why a draw cannot be done as asked (0 = it can). Bits: 1 target/depth surfaces, 2 fill or shade mode, 4 Z state, 8 cull,
 * 10 alpha test, 20 blend pair, 40 stencil, 80 colour write mask (DX8), 100 viewport, 200 texture stage state, 400 (v10.185) texture not samplable, 800 (v10.189) points/lines (not drawn yet), 1000 (v10.214) a cube map that cannot be drawn.
 * States with no effect this chip needs to reproduce are ignored, as other drivers do: DITHERENABLE, ANTIALIAS, EDGEANTIALIAS,
 * STIPPLEDALPHA, STIPPLEENABLE, RANGEFOGENABLE, CLIPPLANEENABLE (the runtime clips), and COLORKEYBLENDENABLE. COLORKEYENABLE keys
 * SELECTARG1-texture draws (v10.197, XP's T45 program); other programs draw without the key until captured. FOGENABLE draws with fog (v10.195). (DxDiag's Direct3D 7 test, v10.176: its
 * cube sets DITHERENABLE, every draw was refused and EndScene failed with D3DERR_SCENE_END_FAILED.) */
static uint32_t why_states(const vx_d3d_context *c) { /* .552: every bit but 200 (the texture stage states: the composed program's error) */
 const uint32_t *r=c->state.rs;uint32_t w=0,bs,bd;
 if(c->lost||!vx_d3d_targets_valid(&c->target,&c->depth))w|=1;
 /* v10.368 (audit N5): SHADEMODE Phong (3) is drawn as Gouraud and ZENABLE W-buffer (2) as a Z buffer, as other drivers do (neither is
  * advertised); both left every draw out */
 if(r[8]<1||r[8]>3||r[9]<1||r[9]>3)w|=2; /* v10.296: FILLMODE point (1) and wireframe (2) are drawn, as points and lines */
 if(r[7]>2||r[14]>1||r[23]<1||r[23]>8||(r[7]&&!c->depth.format))w|=4;
 if(r[22]<1||r[22]>3)w|=8;
 if(r[15]>1||(r[15]&&(r[25]<1||r[25]>8||r[24]>255)))w|=0x10;
 if(r[27]>1||(r[27]&&(!blend_pair(r[19],r[20],&bs,&bd)||!blend_op(r[VX_D3D_RS_BLENDOP],&bs))))w|=0x20;
 if(r[52]&&c->depth.format!=VX3DN_D24S8)w|=0x40; /* v10.201: stencil drawn on D24S8 */
 if(r[168]>15||(r[VX_D3D_RS_LOGICOP]&&(r[VX_D3D_RS_LOGICOP]&~0x1FUL)))w|=0x80; /* v10.295: a partial colour write mask (COLORWRITEENABLE order: R 1, G 2, B 4, A 8) and the ICD's logic op are drawn */
 if(c->state.viewport.left<0||c->state.viewport.top<0||c->state.viewport.right>(int32_t)c->target.width||c->state.viewport.bottom>(int32_t)c->target.height)w|=0x100;
 return w;
}
uint32_t vx_d3d_unsupported_why(const vx_d3d_context *c) {
 uint32_t w=why_states(c);
 if(!(w&1)&&vx_d3d_texture_status(c))w|=0x200;
 return w;
}
int vx_d3d_supported(const vx_d3d_context *c) {return vx_d3d_unsupported_why(c)==0;}
/* v10.177: a primitive the chip cannot draw as asked is left out and counted, and the batch goes on: a HAL that fails DrawPrimitives2
 * makes the application's EndScene fail (D3DERR_SCENE_END_FAILED). ValidateDevice still reports the state as unsupported. */
uint32_t vx_d3d_dropped,vx_d3d_drop_why,vx_d3d_recovered;
uint32_t vx_d3d_lost_rc,vx_d3d_lost_n; /* .539: the last submit failure that lost a context, and how many */
uint32_t vx_d3d_ckey_on=1; /* v10.208: ACCEL_D3D_CKEY, from the VxD */
uint32_t vx_d3d_mips=1; /* v10.199: mip chains on (the registry switch ACCEL_D3D_MIP is in ring 0) */
uint32_t vx_d3d_drop_state[8]; /* v10.184: at the last dropped primitive: texture error, TEXTUREHANDLE (rs 1), stage-0 texture, COLOROP, ARG1, ARG2, ALPHAOP, stage-1 COLOROP */
static void drop(const vx_d3d_context *c) {
 const uint32_t *t=c->state.tss[0];
 vx_d3d_dropped++;vx_d3d_drop_why=vx_d3d_unsupported_why(c);
 vx_d3d_drop_state[0]=vx_d3d_texture_status(c);vx_d3d_drop_state[1]=c->state.rs[1];vx_d3d_drop_state[2]=t[0];vx_d3d_drop_state[3]=t[1];
 vx_d3d_drop_state[4]=t[2];vx_d3d_drop_state[5]=t[3];vx_d3d_drop_state[6]=t[4];vx_d3d_drop_state[7]=c->state.tss[1][1];
}
uint32_t vx_d3d_submit_result(vx_d3d_context *c,uint32_t rc,uint32_t completed) {
 if(!rc)return 0;
 if(rc==VX3DN_RECOVERED){vx_d3d_recovered++;return 0;} /* v10.210: the VxD reset a hung engine; this primitive is dropped, the context stays */
 /* A failed preflight did not issue GPU work and leaves the context usable.
  * Once any packet in this call completed, replaying the callback/token
  * could blend earlier pixels twice. Contain that partial-call failure. */
 if(!completed) {
  if(rc==VX3DN_BAD_PACKET)return VX_D3D_INVALID;
  if(rc==VX3DN_UNSUPPORTED)return VX_D3D_UNSUPPORTED;
  if(rc==VX3DN_BUSY)return VX_D3D_STILLDRAWING;
 }
 /* .539 (board, GTA3 with ACCEL_D3D_TNL on .538: SURFACELOST from 60 s with no reason counted): this loss is counted too; the HAL notes
  * it (reason 26: the VxD's answer in the low 16 bits, bit 31 when packets of the call had completed) */
 vx_d3d_lost_rc=(rc&0xFFFFUL)|(completed?0x80000000UL:0);vx_d3d_lost_n++;
 c->lost=1;return VX_D3D_SURFACELOST;
}
/* v10.276: the call's queue (ACCEL_D3D_SUBMITN, a services' submit_n). An offscreen draw packet waits here with the offset of the command
 * that made it; the queue goes to ring 0 in one call when it is full, before any other packet (a clear, a draw on the primary), before
 * a command the runtime parses (it can change what the draws read), and at the end of every DrawPrimitives2 / DrawOne call, so no
 * packet outlives its call. Only packets ring 0 took count as completed; a failure reports the command of the first packet not taken
 * (a WASSTILLDRAWING retry starts there: nothing after it was sent). Static, under the HAL's callback_lock. */
#define CQ_MAX VX3DN_BATCH_SEND /* .560: 24 (vx_native3d.h: a call's bytes); v10.315 (review): 32, the VxD's VX3DN_BATCH_MAX; 16 made twice the calls */
static vx3dn_packet cq[CQ_MAX];static uint32_t cq_n,cq_cmd[CQ_MAX];uint32_t vx_d3d_cq_calls,vx_d3d_cq_packets;
static vx3dn_packet bq;static uint32_t bq_on; /* the batch (below; v10.449: a plain static again, ACCEL_D3D_QDIRECT removed) */
uint32_t vx_d3d_ft[32];static int ft_sm; /* v10.444: the fast path's sampled parts (fast_triangle, below); v10.453: 20-31, the token loop around it */
static uint32_t fail_cmd,fail_set;
/* v10.279 (review): the queue spans state commands (that is what it is for), but a WASSTILLDRAWING retry starts at the command of the
 * first packet not taken and draws it under the context's state, which by then held the state of the commands after it (a busy answer
 * turned triangles drawn [blend off, blend on] into [on, on]). Each queued packet keeps the number of its state epoch; before a render
 * state, texture stage state or viewport command changes the state that queued packets were made with, that state is kept (cq_snap, one
 * per epoch, at most one per packet). A failed send puts back the state of the first packet not taken, so the retry starts from the
 * state it was made with. A state block command (record, apply, capture: the blocks are not kept) and a render-target change send the
 * queue first, as a clear does. vx_d3d_cq_restores counts the states put back. */
static vx_d3d_state cq_snap[CQ_MAX];static uint32_t cq_ep[CQ_MAX],cq_epn;uint32_t vx_d3d_cq_restores;
/* .550: the queue held across DrawPrimitives2 calls (always on from .551, set by the HAL; ACCEL_D3D_HOLD, .550's A/B switch, removed: GTA3
 * median 93 -> 76 ms; vx_d3d_hold_on stays 0 in VXGL.DLL and the host tests unless they set it). The .549 VxD profile:
 * the batch call's own cost (dispatch, entry, sequencer open and close, between and after the packets) about a fifth of the VxD's time, at
 * 1.6 packets a call (one VxD call each DrawPrimitives2 call; cookbook 31.3: about 23 us a call, 3.5 ms a slow frame). A call that ends
 * with fewer than CQ_MAX packets queued keeps them (cq_held, the first cq_held of the queue) and the next call adds to them; the queue goes
 * when full, at any command that sends it now (state blocks, render target, clear, parsed commands, texture blits), and from the HAL
 * before anything that reads or moves what the packets draw: Lock, Blt, Flip, DestroySurface, SetEntries, the context's destroy
 * (vx_d3d_hold_send, VX_D3D_hold_sync in vx_d3d_hal.c). A held packet is a full copy (vertices, indices and state), so the runtime's
 * buffers may change meanwhile. Errors: a held packet belongs to a call that already answered DD_OK, so its failure cannot be retried by
 * the runtime: a held packet refused or still busy after the HAL's own waits (500 ms) is dropped and counted (hold_stat[3]); a loss is
 * reported to the call that sent it, at that call's start. The held packets' state epochs are cleared (nothing to put back for them).
 * vx_d3d_hold_stat: [0] calls that ended with packets held, [1] packets held, [2] sends of held packets by a later call, [3] held
 * packets dropped (refused, busy), [4] sends from the HAL's other callbacks, [5] held packets dropped unsent (their process gone) */
uint32_t vx_d3d_hold_on,vx_d3d_hold_stat[6];static uint32_t cq_held;static vx_d3d_context *hold_c;
static void cq_clear(void) {cq_n=0;cq_epn=0;cq_held=0;}
static void cq_keep_state(const vx_d3d_context *c) {if(cq_n&&cq_ep[cq_n-1]==cq_epn&&cq_epn<CQ_MAX){cq_snap[cq_epn]=c->state;cq_epn++;}}
/* .528: the batch's own epoch when a texture stage state command was let through without sending it (texture runs, tr_defer): the state
 * its first draws were made with, kept as the queue keeps a queued packet's; 0xFFFFFFFF none (it takes the epoch it is queued in) */
static uint32_t bq_ep=0xFFFFFFFFUL;
static uint32_t cq_send_body(const vx_d3d_services *s,vx_d3d_context *c,uint32_t *completed);
static uint32_t cq_send(const vx_d3d_services *s,vx_d3d_context *c,uint32_t *completed) {
 uint32_t o=VX_PH(VX_PH_BUF),rc=cq_send_body(s,c,completed);VX_PH_BACK(o);return rc;
}
static uint32_t cq_send_body(const vx_d3d_services *s,vx_d3d_context *c,uint32_t *completed) {
 uint32_t off=0,done,r,rc;int alone=0,again=0; /* .532: again (below) */
 /* v10.368: alone = the primary's packets queued before the switch went off go alone too */
 while(off<cq_n) {
  done=0;r=s->submit_n(s->context,cq+off,cq_n-off,&done);if(done>cq_n-off)done=cq_n-off;
  vx_d3d_cq_calls++;vx_d3d_cq_packets+=done;*completed+=done;off+=done;
  if(!r){if(done)continue;r=VX3DN_BUSY;} /* v10.330 (review #82): success with nothing taken left the rest of the queue unsent and was cleared as sent; it fails as BUSY now (the command and state are kept for the retry) */
  if(r==VX3DN_RECOVERED){vx_d3d_recovered++;if(!done&&again++)break;continue;} /* v10.210: the VxD reset a hung engine; the rest goes on. .532 (review of .530, B2):
   * with none taken the reset came before any of them was issued (the VxD counts in done every packet it issued), so they are sent once
   * more instead of dropped; a second such answer drops them as before */
  /* v10.368 (audit N2): busy after taking some of the packets is back-pressure, not a loss: the rest is sent again (the HAL waits out
   * busy itself; each pass takes at least one packet, so the loop ends). It went to submit_result with packets completed: surface lost. */
  if(r==VX3DN_BUSY&&done)continue;
  /* v10.368 (audit): every packet taken and an error with them: nothing is left to retry, so it is not a retry from the last packet (it
   * would draw that packet again under an earlier state); a loss still counts */
  if(off>=cq_n&&r!=VX3DN_LOST)break;
  /* v10.368 (audit N8): a VxD that sends the primary's packets alone refuses one in a batch: from then on they go alone (as before) */
  if(r==VX3DN_BAD_PACKET&&!done&&!cq[off].target.offset&&((vx_d3d_feat&VX_F_PRIMBATCH)||alone)){
   vx_d3d_feat&=~VX_F_PRIMBATCH;alone=1;r=s->submit(s->context,&cq[off]);
   if(!r||r==VX3DN_RECOVERED){if(r)vx_d3d_recovered++;else{vx_d3d_cq_packets++;(*completed)++;}off++;continue;}}
  if(off<cq_held&&r!=VX3DN_LOST){ /* .550: a held packet refused, or busy after the waits: dropped (its call answered long ago) */
   uint32_t m=r==VX3DN_BUSY?cq_held-off:1,t=cq_n-off-m;
   vx_d3d_hold_stat[3]+=m;
   {uint32_t j;for(j=0;j<t;j++){cq[off+j]=cq[off+m+j];cq_cmd[off+j]=cq_cmd[off+m+j];cq_ep[off+j]=cq_ep[off+m+j];}} /* forward copies (d3d_callbacks.py's emulator cannot run the C library's memmove) */
   cq_n-=m;cq_held-=m;if(off<cq_n)cq[off].clear_depth=0; /* the next one's state no longer follows the packet before it */
   continue;}
  rc=vx_d3d_submit_result(c,r,*completed);
  if(!fail_set&&off<cq_held){fail_set=1;fail_cmd=cq_held<cq_n?cq_cmd[cq_held]:0;} /* .550: a loss in a held packet: reported at this call's first queued command */
  if(!fail_set){uint32_t k=off<cq_n?off:cq_n-1;fail_set=1;fail_cmd=cq_cmd[k];if(cq_ep[k]<cq_epn){c->state=cq_snap[cq_ep[k]];template_reset();vx_d3d_cq_restores++;} /* .553: the state changed: the template and the kept answers end here (the call fails after this; every state write resets them) */}
  cq_clear();return rc;
 }
 cq_clear();return 0;
}
static uint32_t cq_flush(const vx_d3d_services *s,vx_d3d_context *c,uint32_t *completed) {return cq_n&&s->submit_n?cq_send(s,c,completed):0;}
/* .550: the held packets */
uint32_t vx_d3d_held(void) {return cq_held;}
uint32_t vx_d3d_hold_send(const vx_d3d_services *s) {uint32_t done=0,rc;if(!cq_held||!hold_c||!s||!s->submit_n){cq_clear();return 0;}
 rc=cq_send(s,hold_c,&done);cq_clear();return rc;}
void vx_d3d_hold_drop(void) {vx_d3d_hold_stat[5]+=cq_held;cq_clear();}
int vx_d3d_hold_is(const vx_d3d_context *c) {return cq_held&&hold_c==c;}
/* the end of a DrawPrimitives2 call: keep what is queued (below CQ_MAX: a full queue went already) */
static int cq_hold(const vx_d3d_services *s,vx_d3d_context *c) {uint32_t k;
 if(!vx_d3d_hold_on||!cq_n||cq_n>=CQ_MAX||!s->submit_n||!(vx_d3d_feat&VX_F_SUBMITN)||c->lost)return 0;
 for(k=0;k<cq_n;k++)cq_ep[k]=0xFFFFFFFFUL;cq_epn=0;cq_held=cq_n;hold_c=c;vx_d3d_hold_stat[0]++;vx_d3d_hold_stat[1]+=cq_n;return 1;}
/* the start of a call: held packets of another context go first (their context answers for them) */
static uint32_t cq_take(const vx_d3d_services *s,vx_d3d_context *c) {uint32_t done=0,rc=0;
 if(!cq_held){cq_clear();return 0;}
 if(c!=hold_c||!s->submit_n){rc=s->submit_n?cq_send(s,hold_c,&done):0;cq_clear();if(rc&&hold_c!=c)rc=0;return rc;}
 vx_d3d_hold_stat[2]++;return 0;}
/* .530: v10.354's 3D-engine depth clears (ACCEL_3D_CLEAR3D, off since v10.362: WinTune 139 MPixels/s against 278 with the 2D fill) and the
 * experimental DirectDraw blit through a textured rectangle (ACCEL_DDRAW_3DBLT, off) removed */
/* .524: state deltas (ACCEL_D3D_DELTA, the VxD's feature word bit 20; VX_D3D_Switch 14; vx_native3d.h): a draw queued after another
 * carries which of the state ranges 1-6 are byte for byte the previous queued packet's (the VxD then keeps its copy of them). The ranges
 * compared here in the HAL's own memory (cache-warm, 864 bytes at most) instead of read again from it by the VxD. vx_d3d_dl: [0] packets
 * marked, [1] ranges marked the same, [2] packets with every range the same */
uint32_t vx_d3d_delta=1,vx_d3d_dl[3];
static uint32_t dl_same(const vx3dn_packet *a,const vx3dn_packet *b) {
 static const uint32_t r[7][2]=VX3DN_STATE_RANGES;uint32_t k,m=0;
 for(k=1;k<7;k++)if(!memcmp((const char*)a+r[k][0],(const char*)b+r[k][0],r[k][1]-r[k][0])){m|=1UL<<(k-1);vx_d3d_dl[1]++;}
 vx_d3d_dl[0]++;if(m==VX3DN_SAME_MASK)vx_d3d_dl[2]++;
 return m;
}
static uint32_t submit_at_body(const vx_d3d_services *s,const vx3dn_packet *p,vx_d3d_context *c,uint32_t *completed,uint32_t cmd);
static uint32_t submit_at(const vx_d3d_services *s,const vx3dn_packet *p,vx_d3d_context *c,uint32_t *completed,uint32_t cmd) {
 uint32_t o=VX_PH(VX_PH_BUF),rc=submit_at_body(s,p,c,completed,cmd);VX_PH_BACK(o);return rc;
}
static uint32_t submit_at_body(const vx_d3d_services *s,const vx3dn_packet *p,vx_d3d_context *c,uint32_t *completed,uint32_t cmd) {
 uint32_t rc;
 if(s->submit_n&&(vx_d3d_feat&VX_F_SUBMITN)) {
  /* v10.368 (audit N8): draws on the primary's memory too (ACCEL_D3D_PRIMBATCH, default 1): in a flip chain every other frame is drawn
   * there (Flip swaps the surfaces' memory), and each of its packets went to ring 0 alone; the VxD excludes the cursor around the batch */
  if(p->op==VX3DN_DRAW&&(p->target.offset||(vx_d3d_feat&VX_F_PRIMBATCH))&&p->vertex_count<=(p->pre.on==2?VX3DN_IX_VERTICES:VX3DN_MAX_VERTICES)) { /* .529 */
   vx_d3d_packet_copy(&cq[cq_n],p);
   if(vx_d3d_delta&&cq_n)cq[cq_n].clear_depth=VX3DN_SAME_TAG|dl_same(&cq[cq_n],&cq[cq_n-1]); /* .524 */
   cq_cmd[cq_n]=cmd;cq_ep[cq_n]=p==&bq&&bq_ep!=0xFFFFFFFFUL?bq_ep:cq_epn;cq_n++; /* .528: a batch that spans texture stage states: the epoch of its first draws */
   return cq_n>=CQ_MAX?cq_send(s,c,completed):0;
  }
  rc=cq_flush(s,c,completed);if(rc)return rc;
 }
 rc=vx_d3d_submit_result(c,s->submit(s->context,p),*completed);
 if(!rc)(*completed)++;
 return rc;
}
/* v10.214: batching (ACCEL_D3D_BATCH). A draw packet waits in bq; the next draw made from the same draw template (v10.221: no state
 * command in between) and of the same primitive kind is appended, up to VX3DN_MAX_VERTICES. The batch goes to ring 0 when a draw does not fit or
 * differs, before a clear or a render-target change, and at the end of every DrawPrimitives2 / DrawOne call, so no packet outlives the
 * call that made it. Only packets that reached ring 0 count as completed. Static, under the HAL's callback_lock. */
static uint32_t bq_cmd,cur_cmd;uint32_t vx_d3d_batched; /* (bq, bq_on: above, v10.443) */
static uint32_t submit(const vx_d3d_services *s,const vx3dn_packet *p,vx_d3d_context *c,uint32_t *completed) {return submit_at(s,p,c,completed,p==&bq?bq_cmd:cur_cmd);}
/* a batch that fails reports the offset of the command that started it (bq_cmd), so a WASSTILLDRAWING retry starts there */
/* v10.363 (diagnostic, VXGL.LOG): the batches sent, by vertex count (1-16, 17-48, 49-96, 97-144, 145-191, 192 = full) and by what ended
 * them: 0 not named, 1 a new template (state changed), 2 another primitive kind, 3 no room for the vertices, 4 no room for another run,
 * 5 the ICD's flush (other), 6 the ICD's state translation, 7 a swap, glFlush or glFinish, 8 a clear, 9 a texture upload. The ICD sets
 * vx_d3d_cut_tag before its flushes; the core's own reasons below. */
uint32_t vx_d3d_vhist[6],vx_d3d_cut[10],vx_d3d_cut_tag;
uint32_t vx_d3d_empty_drops; /* .540 */
uint32_t vx_d3d_flush(vx_d3d_context *c,const vx_d3d_services *sv,uint32_t *completed) {
 uint32_t rc;if(!bq_on){vx_d3d_cut_tag=0;return 0;}
 /* .540: a draw packet with nothing in it (no vertices; an indexed one without a triangle) is not sent: the VxD refuses it as malformed,
  * and after earlier packets of the call that loses the context. Counted (vx_d3d_empty_drops) */
 if(bq.op==VX3DN_DRAW&&(!bq.vertex_count||(bq.pre.on==2&&(bq.bursts.b[0]&0xFFFFUL)<3))){bq_on=0;vx_d3d_cut_tag=0;vx_d3d_empty_drops++;return 0;}
 {uint32_t n=bq.vertex_count;vx_d3d_vhist[n<=16?0:n<=48?1:n<=96?2:n<=144?3:n<VX3DN_MAX_VERTICES?4:5]++;vx_d3d_cut[vx_d3d_cut_tag<10?vx_d3d_cut_tag:0]++;vx_d3d_cut_tag=0;}
 vx_d3d_pre_stat[bq.pre.on?0:1]++; /* v10.441 */
 bq_on=0;rc=submit(sv,&bq,c,completed);if(rc&&!fail_set){fail_set=1;fail_cmd=bq_cmd;}return rc;
}
/* v10.272: a packet's state, without its per-vertex arrays (vertices, coordinates, specular, the second set, q: 192 entries each, 8.6 of the
 * packet's 9.3 KB). Nothing reads an array past vertex_count (put_vertices writes every array for each vertex it adds, the VxD copies and
 * checks the first vertex_count entries), so a packet is cleared and a template copied by these ranges alone: three 9.3 KB passes a
 * state change (packet's memset, the template's copy, the batch's copy) were 45% of VXGL's time binding textures (host profile). */
#define PKO(f) offsetof(vx3dn_packet,f)
/* .552: constant spans remove the range-table loop. Four-byte fields use
 * direct word stores; larger spans retain the compiler's memcpy/memset.
 * Keep the former ranges exactly: unused vertex storage is never touched.
 * One list defines both operations, so their field coverage cannot diverge. */
#define PK_SPANS(X) \
 X(0,PKO(vertex_count)+4) \
 X(PKO(tex),PKO(tex)+offsetof(vx3dn_texture,uv)) \
 X(PKO(shader),PKO(shader)+offsetof(vx3dn_shader,spec)) \
 X(PKO(shader)+offsetof(vx3dn_shader,key),PKO(stages)+offsetof(vx3dn_stages,uv1)) \
 X(PKO(stages)+offsetof(vx3dn_stages,c22),PKO(stages)+offsetof(vx3dn_stages,q)) \
 X(PKO(stages)+offsetof(vx3dn_stages,face),PKO(stages)+offsetof(vx3dn_stages,r1)) \
 X(PKO(bursts),PKO(bursts)+4) \
 X(PKO(stipple),PKO(stipple)+sizeof(vx3dn_stipple)) \
 X(PKO(pre),PKO(pre)+4) \
 X(PKO(runs),PKO(runs)+4)
#define PK_ZERO(a,b) do{if((b)-(a)==4)*(vx_u32a*)((char*)p+(a))=0;else memset((char*)p+(a),0,(b)-(a));}while(0);
static void pk_clear(vx3dn_packet *p) {PK_SPANS(PK_ZERO)}
#undef PK_ZERO
#define PK_COPY(a,b) do{if((b)-(a)==4)*(vx_u32a*)((char*)d+(a))=*(const vx_u32a*)((const char*)s+(a));else memcpy((char*)d+(a),(const char*)s+(a),(b)-(a));}while(0);
static void pk_copy(vx3dn_packet *d,const vx3dn_packet *s) { /* the state, the vertex count and the runs */
 uint32_t n=s->bursts.n<VX3DN_MAX_BURSTS?s->bursts.n:VX3DN_MAX_BURSTS;
 PK_SPANS(PK_COPY)
 memcpy(d->bursts.b,s->bursts.b,n*4);
}
#undef PK_COPY
#undef PK_SPANS
/* v10.275: a packet for VXGL's queue: what the VxD reads (the state ranges, the runs, the first vertex_count entries of each per-vertex array) */
void vx_d3d_packet_copy(vx3dn_packet *d,const vx3dn_packet *s) {
 const uint32_t lim=s->pre.on==2?VX3DN_IX_VERTICES:VX3DN_MAX_VERTICES; /* .529: an indexed packet's 384 */
 uint32_t n=s->vertex_count<lim?s->vertex_count:lim;
 pk_copy(d,s);
 if(s->pre.on){memcpy(d->pre.w,s->pre.w,n*4*VX3DN_PRE_STRIDE(s)); /* v10.441: the 10-word vertices only; .523: 8 words in the 8-word form */
  if(s->pre.on==2&&s->bursts.n){uint32_t ni=s->bursts.b[0]&0xFFFFUL;if(ni>VX3DN_MAX_INDICES)ni=VX3DN_MAX_INDICES;memcpy(d->vertices,s->vertices,ni*2);} /* v10.470: and the indices */
  if(s->runs.n)memcpy(d->runs.r,s->runs.r,(s->runs.n<VX3DN_TEXRUNS_MAX?s->runs.n:VX3DN_TEXRUNS_MAX)*sizeof(vx3dn_texrun)); /* .528: the texture runs */
  return;}
 memcpy(d->vertices,s->vertices,n*sizeof(s->vertices[0]));
 /* v10.315: the arrays ring 0 reads for this packet only (vx_native3d_vxd.c copy_packet: the same tests) */
 if(s->tex.enable)memcpy(d->tex.uv,s->tex.uv,n*sizeof(s->tex.uv[0]));
 if(s->shader.count)memcpy(d->shader.spec,s->shader.spec,n*sizeof(s->shader.spec[0]));
 if(s->tex.enable&&s->stages.units>1)memcpy(d->stages.uv1,s->stages.uv1,n*sizeof(s->stages.uv1[0]));
 if(s->stages.layout||(s->stages.s3&1))memcpy(d->stages.q,s->stages.q,n*sizeof(s->stages.q[0]));
 if(s->stages.s3&2)memcpy(d->stages.r1,s->stages.r1,n*sizeof(s->stages.r1[0])); /* v10.321 */
}
/* v10.295: the packet's write mask: COLORWRITEENABLE's channels (bits 3:0), and an OpenGL logic op as 0x100 | its truth table << 4 */
static uint32_t vx_d3d_write_mask(const uint32_t *rs) {uint32_t m=rs[168]&15UL,l=rs[VX_D3D_RS_LOGICOP];return (l&0x10)?m|0x100UL|((l&15UL)<<4):m;}
/* v10.481: SHADEMODE flat (rs[9] 1) on the engine (vx_d3d_gl_hwflat, set by the OpenGL ICD for its own contexts: ACCEL_OPENGL_HWFLAT;
 * the Direct3D HAL leaves it 0): the packet carries ext.dither bit 1, the VxD draws its triangles with XP's flat mode word (each triangle
 * the colours of its first vertex), the colours are not copied here and the fast paths take it; otherwise flat is the colour copy (FLATSW) */
uint32_t vx_d3d_gl_hwflat,vx_d3d_gl_flatpv; /* v10.482: flatpv, the ICD's flat strip laid out for the engine's strip rule under test (strip_body) */
#define HWFLAT(c) ((c)->state.rs[9]==1&&vx_d3d_gl_hwflat)
#define FLATSW(c) ((c)->state.rs[9]==1&&!vx_d3d_gl_hwflat)
static void packet(vx3dn_packet *p,const vx_d3d_context *c,uint32_t op) {
 pk_clear(p);p->cb=sizeof(*p);p->abi=VX3DN_ABI;p->arm=VX3DN_ARM;p->generation=c->generation;p->op=op;
 p->target=c->target;p->depth=c->depth;p->clip=c->state.viewport;
 if(c->state.stip[0]&&op==VX3DN_DRAW){uint32_t k;p->stipple.on=1;for(k=0;k<32;k++)p->stipple.rows[k]=c->state.stip[1+k];} /* v10.308: the ICD's polygon stipple */
 p->z_enable=c->state.rs[7]?1:0;p->z_write=c->state.rs[14];p->z_func=c->state.rs[23];p->blend=c->state.rs[27];p->write_mask=vx_d3d_write_mask(c->state.rs);
 if(op==VX3DN_CLEAR){p->z_enable=0;p->z_write=0;p->z_func=1;p->blend=0;}
 else {
  if(p->blend){uint32_t op=0,k=c->state.rs[VX_D3D_RS_BLENDFACTOR];(void)blend_pair(c->state.rs[19],c->state.rs[20],&p->raster.blend_src,&p->raster.blend_dst);(void)blend_op(c->state.rs[VX_D3D_RS_BLENDOP],&op);
   p->raster.blend_src|=(op<<5)|(blend_k(p->raster.blend_src,k)<<8);p->raster.blend_dst|=blend_k(p->raster.blend_dst,k)<<8;} /* v10.304 */
  if(c->state.rs[15]){p->raster.alpha_func=c->state.rs[25];p->raster.alpha_ref=c->state.rs[24]&255;}
  /* v10.201: DITHERENABLE (26); stencil (52 enable, 53 fail, 54 zfail, 55 pass, 56 func, 57 ref, 58 mask, 59 write mask) on D24S8 */
  p->ext.dither=c->state.rs[26]&&(vx_d3d_feat&VX_F_DITHER)?1:0;
  if(HWFLAT(c))p->ext.dither|=2; /* v10.481: the engine's flat shading (04/23 = 9E02, XP's T48): each triangle the colours of its first vertex */
  if(c->state.rs[52]&&c->depth.format==VX3DN_D24S8) {
   const uint32_t *r=c->state.rs;uint32_t f=r[56]>=1&&r[56]<=8?r[56]-1:7,sf=r[53]>=1&&r[53]<=8?r[53]-1:0,zf=r[54]>=1&&r[54]<=8?r[54]-1:0,zp=r[55]>=1&&r[55]<=8?r[55]-1:0;
   p->ext.stencil=1;p->ext.stencil_w1=(r[57]&255)<<16|(r[58]&255)<<8|(r[59]&255);p->ext.stencil_w2=f<<16|sf<<6|zf<<3|zp;
  }
 }
}
/* v10.195: fog registers 70..78 as XP writes them (cookbook 16.7; CLXP3D T16 vertex, T17 LINEAR, T18 EXP). rs: 34 FOGCOLOR, 35 FOGTABLEMODE
 * (0 none = vertex fog, 1 EXP, 2 EXP2, 3 LINEAR), 36 FOGSTART, 37 FOGEND, 38 FOGDENSITY (floats). */
static void fog_registers(vx3dn_fog *f,const uint32_t *rs) {
 static const uint32_t mode[4]={0,4,5,2};uint32_t col=rs[34],q;float s=flt(rs[36]),e=flt(rs[37]),inv;
 memset(f,0,sizeof(*f));f->enable=1;
 f->r[0]=0x30|mode[rs[35]<4?rs[35]:0];
 f->r[1]=(((col>>8)&255)*1024/255)<<12|((col&255)*1024/255);f->r[2]=((col>>16)&255)*1024/255;
 f->r[3]=(rs[36]>>16)&0x7FFF;f->r[7]=(rs[37]>>16)&0x7FFF;
 inv=(finite(rs[36])&&finite(rs[37])&&e!=s)?1.0f/(e-s):1.0f;if(!finite(bits_any(inv)))inv=1.0f;
 q=bits_any(inv);f->r[5]=(q>>19)&0xF;f->r[6]=(q>>23)&0xFF;
 f->r[8]=((rs[38]>>18)&0x1FFF)<<8;
}
/* v10.197: XP's colour-key program (CLXP3D T45, COLORKEYENABLE on SELECTARG1 texture): five instructions that compare the texel with the
 * key constant and reject the pixel, then T00's. Ring 0 adds its header, TAU words and constants (vx3dn_shader kflags bit 1). */
static const uint32_t ckey_tex[24]= /* v10.198: the first 20 words are the key test */{0x80000550UL,0x00140924UL,0x0070AB05UL,0x00000C20UL,0x000C0558UL,0x5000C055UL,0x00700C05UL,0x00090C20UL,
 0x01C00530UL,0x5008024DUL,0x00F00C05UL,0x00021C20UL,0x800C0550UL,0x00140924UL,0x00708100UL,0x00000420UL,
 0x01C00530UL,0x301C0053UL,0x00820C05UL,0x040D0420UL,0x01C00530UL,0x301C0053UL,0x00F20005UL,0x000D0820UL};
/* the key in the texture's pixel format -> r g b as the sampler expands them (bit replication), alpha 1.0 as XP sends it */
static void key_floats(uint32_t *out,uint32_t fmt,uint32_t k) {
 uint32_t r,g,b,mr=255,mg=255,mb=255;
 switch(fmt) {
  case VX3DN_TEX_RGB565:r=(k>>11)&31;g=(k>>5)&63;b=k&31;mr=31;mg=63;mb=31;break;
  case VX3DN_TEX_ARGB1555:case VX3DN_TEX_XRGB1555:r=(k>>10)&31;g=(k>>5)&31;b=k&31;mr=mg=mb=31;break;
  case VX3DN_TEX_ARGB4444:r=(k>>8)&15;g=(k>>4)&15;b=k&15;mr=mg=mb=15;break;
  case VX3DN_TEX_L8:r=g=b=k&255;break; /* v10.199 */
  default:r=(k>>16)&255;g=(k>>8)&255;b=k&255;
 }
 /* v10.198: XP sends each channel over its own maximum (T74: R5G6B5 key 2418 -> 4/31 32/63 24/31); the test ignores alpha (T69, T74) */
 out[0]=bits_any((float)((double)r/mr));out[1]=bits_any((float)((double)g/mg));out[2]=bits_any((float)((double)b/mb));out[3]=0x3F800000UL;
}
/* v10.208: the colour key with a program that uses a temporary (XP T73: specular; T125: a second stage). The key test (texel - c43,
 * squared, summed, against 1/512 into C1, then a kill TAU) takes ALU 0 and 2-5; the program's first instruction, which writes r0, goes to
 * C0 at ALU 1 instead, and the rest of the program reads C0 for r0. Schedule: one unit 90=018001 91=3 93=3 95=020, TAU 301001B0 314011B1;
 * two units 93=6, TAU 301001B0 211111B1 314011B1. Constants c0 c1 (0.5 1 2 4, 1/512) and the key in c43. */
static int ckey_c0(vx_d3d_xp *x,uint32_t fmt,uint32_t key) {
 static const uint32_t k0[4]={0x80000550UL,0x00140924UL,0x0070AB05UL,0x00000C20UL},kt[16]={0x000C0558UL,0x5000C055UL,0x00700C05UL,0x00090C20UL,
  0x01C00530UL,0x5008024DUL,0x00F00C05UL,0x00021C20UL,0x800C0550UL,0x00140924UL,0x00708100UL,0x00000460UL,0x01C00530UL,0x301C0053UL,0x00820C05UL,0x040D0460UL};
 uint32_t w[32],n=x->count,k,i,f[4];int r0=1;
 if(n<2||n>3||x->units>2||((x->words[3]>>4)&0xFF)!=0xC2)return 0;
 for(k=0;k<4;k++){w[k]=k0[k];w[4+k]=x->words[k];}
 w[7]=(w[7]&~0xFF0UL)|0x420UL;if(((w[6]>>20)&15)==15&&((w[7]>>16)&15)==0xD)w[7]|=0x04000000UL; /* T125: a folded SELECT into C0 carries bit 26 */
 for(k=0;k<16;k++)w[8+k]=kt[k];
 for(i=1;i<n;i++){uint32_t *q=w+(5+i)*4;for(k=0;k<4;k++)q[k]=x->words[i*4+k];
  if(r0){if(f_areg(q)==XR_R0)s_areg(q,0xC0);if(f_breg(q)==XR_R0)s_breg(q,0xC0);if(f_creg(q)==XR_R0)s_creg(q,0xC0);}
  if(((q[3]>>4)&0xFF)==0xC2)r0=0;}
 n+=5;last_reads(w,n);
 for(k=0;k<n*4;k++)x->words[k]=w[k];
 x->count=n;
 x->ps=1;x->h90=0x18001UL;x->h91=3;x->h95=0x20;
 if(x->units==2){x->ntau=3;x->h93=6;x->tau[0]=0x301001B0UL;x->tau[1]=0x211111B1UL;x->tau[2]=0x314011B1UL;}
 else {x->ntau=2;x->h93=3;x->tau[0]=0x301001B0UL;x->tau[1]=0x314011B1UL;} /* v10.210: XP's T73 word (314001B1 is T45's, after 201001B0): it hung the engine on the board (v10.209) */
 key_floats(f,fmt,key);
 x->nkc=3;x->kc_reg[0]=0;x->kc[0][0]=0x3F000000UL;x->kc[0][1]=0x3F800000UL;x->kc[0][2]=0x40000000UL;x->kc[0][3]=0x40800000UL;
 x->kc_reg[1]=1;x->kc[1][0]=0x3B000000UL;x->kc[1][1]=0;x->kc[1][2]=0;x->kc[1][3]=0;
 x->kc_reg[2]=43;for(k=0;k<4;k++)x->kc[2][k]=f[k];
 return 1;
}
typedef struct cv {double x,y,z,rhw,c[4],u,v,s[4],u1,v1,q,q1;} cv; /* v10.206: u1 v1 = the second coordinate set; v10.208: q = set 0's third coordinate; v10.321: q1 = set 1's */
/* v10.169: byte offset of texture-coordinate set n in an FVF vertex, or 0 when the vertex has no such set. */
static uint32_t texcoord_offset(uint32_t f,uint32_t n,uint32_t *size) {
 uint32_t off=16,i,count=(f>>8)&15;
 if(f&0x40)off+=4;if(f&0x80)off+=4;
 if(n>=count)return 0;
 for(i=0;i<n;i++){uint32_t fmt=(f>>(16+i*2))&3;off+=(fmt==0?2:fmt==1?3:fmt==2?4:1)*4;}
 {uint32_t fmt=(f>>(16+n*2))&3;*size=fmt==0?2:fmt==1?3:fmt==2?4:1;}
 return off;
}
static int rhw0_fix; /* .531: bad_prim (below) */
static void vertex(cv *v,const uint8_t *p,uint32_t f,uint32_t set,uint32_t set1) {
 uint32_t color=(f&0x40)?u32(p+16):0xFFFFFFFFUL,spec=(f&0x80)?u32(p+((f&0x40)?20:16)):0xFF000000UL,k,size=0,off=texcoord_offset(f,set1,&size);
 v->u1=off?flt(u32(p+off)):0;v->v1=(off&&size>=2)?flt(u32(p+off+4)):0;
 if(!finite(off?u32(p+off):0))v->u1=0;if(off&&size>=2&&!finite(u32(p+off+4)))v->v1=0;
 v->q1=(off&&size>=3&&finite(u32(p+off+8)))?flt(u32(p+off+8)):1.0; /* v10.321 */
 size=0;off=texcoord_offset(f,set,&size);
 v->q=(off&&size>=3&&finite(u32(p+off+8)))?flt(u32(p+off+8)):1.0;
 for(k=0;k<4;k++)v->s[k]=(double)((spec>>(k*8))&255);
 v->x=flt(u32(p));v->y=flt(u32(p+4));v->z=flt(u32(p+8));v->rhw=flt(u32(p+12));
 if(rhw0_fix&&!(u32(p+12)&0x7FFFFFFFUL))v->rhw=1.0; /* .531: a primitive whose vertices all have rhw 0, in a state that does not read rhw (rhw0_ok) */
 for(k=0;k<4;k++)v->c[k]=(double)((color>>(k*8))&255);
 v->u=off?flt(u32(p+off)):0;v->v=(off&&size>=2)?flt(u32(p+off+4)):0;
 if(!finite(off?u32(p+off):0))v->u=0;if(off&&size>=2&&!finite(u32(p+off+4)))v->v=0;
}
static double distance(const cv *v,const vx3dn_rect *r,uint32_t plane) {
 switch(plane) {case 0:return v->x-r->left;case 1:return r->right-v->x;
 case 2:return v->y-r->top;case 3:return r->bottom-v->y;case 4:return v->z;default:return 1.0-v->z;}
}
static cv interpolate(const cv *a,const cv *b,double t) {
 cv v;uint32_t k;v.x=a->x+(b->x-a->x)*t;v.y=a->y+(b->y-a->y)*t;
 v.z=a->z+(b->z-a->z)*t;v.rhw=a->rhw+(b->rhw-a->rhw)*t;
 for(k=0;k<4;k++)v.c[k]=a->c[k]+(b->c[k]-a->c[k])*t;
 /* v10.172: texture coordinates are perspective-correct (u*rhw and v*rhw are linear in screen space) */
 if(v.rhw>0){v.u=(a->u*a->rhw+(b->u*b->rhw-a->u*a->rhw)*t)/v.rhw;v.v=(a->v*a->rhw+(b->v*b->rhw-a->v*a->rhw)*t)/v.rhw;
  v.u1=(a->u1*a->rhw+(b->u1*b->rhw-a->u1*a->rhw)*t)/v.rhw;v.v1=(a->v1*a->rhw+(b->v1*b->rhw-a->v1*a->rhw)*t)/v.rhw;
  v.q=(a->q*a->rhw+(b->q*b->rhw-a->q*a->rhw)*t)/v.rhw;v.q1=(a->q1*a->rhw+(b->q1*b->rhw-a->q1*a->rhw)*t)/v.rhw;}
 else {v.u=a->u+(b->u-a->u)*t;v.v=a->v+(b->v-a->v)*t;v.u1=a->u1+(b->u1-a->u1)*t;v.v1=a->v1+(b->v1-a->v1)*t;v.q=a->q+(b->q-a->q)*t;v.q1=a->q1+(b->q1-a->q1)*t;}
 for(k=0;k<4;k++)v.s[k]=a->s[k]+(b->s[k]-a->s[k])*t;
 return v;
}
/* v10.324: OpenGL triangles' x and y on swrast's 1/16-pixel grid (ACCEL_OPENGL_SNAP; s_tritemp.h: FloatToFixed(x + 0.5) & snapMask,
 * FloatToFixed(y - 0.5) & snapMask). In the engine's coordinates (x - 0.5, and H - 0.5 - y or y - 0.5) that is x rounded to 1/2048 and taken
 * down to the grid, y taken down (no flip: a framebuffer object) or up (the window's flip). The ICD sets vx_d3d_gl_snap for its context (0
 * off, 1 y down, 2 y up); tri_snap holds it only while a GL triangle is written (lines, points and Direct3D keep their coordinates). The
 * engine drew from the unsnapped positions: an edge between a pixel centre and the grid point fell on the other side (scene 56's red
 * triangle at texel 12,13: edge y 13.5176 unsnapped, 13.4712 snapped, the centre 13.5). */
uint32_t vx_d3d_gl_snap;static uint32_t tri_snap;
/* v10.352: the index of the highest set bit of a nonzero a (BSR) */
#ifdef __WATCOMC__
uint32_t vx_bsr(uint32_t a);
#pragma aux vx_bsr = "bsr eax,eax" parm [eax] value [eax] modify exact [eax];
#else
static uint32_t vx_bsr(uint32_t a) {return 31U-(uint32_t)__builtin_clz(a);}
#endif
/* v10.350 (board, VXGL_SAMPLE: snapw and Watcom's float-to-integer helper __CHP, which sets the FPU's rounding mode and back for each
 * conversion, were half of Quake 2's particle frames): the 1/2048 rounding on the float's encoding with integer instructions. x = m 2^(e - 150),
 * so x * 2048 = m 2^(e - 139), rounded half away from zero as before; NaN, infinity and |x * 2048| >= 2^31 keep the old conversion */
static uint32_t snapw_d(uint32_t w,int up) {
 double s=(double)flt(w)*2048.0;long n=(long)(s>=0?s+0.5:s-0.5);
 n=up?((n+127L)&~127L):(n&~127L);
 return bits((float)n*(1.0F/2048.0F));
}
static uint32_t snapw(uint32_t w,int up) {
 uint32_t e=(w>>23)&255UL,m;long n;
 if(e>=139UL+8UL)return snapw_d(w,up);
 if(e<139UL-24UL)n=0; /* |x * 2048| < 0.5 */
 else {
  m=(w&0x7FFFFFUL)|0x800000UL;
  if(e>=139UL)n=(long)(m<<(e-139UL));
  else {uint32_t sh=139UL-e;n=(long)((m+(1UL<<(sh-1UL)))>>sh);}
  if(w&0x80000000UL)n=-n;
 }
 n=up?((n+127L)&~127L):(n&~127L);
 /* v10.351: n / 2048 as a float built from its bits (n a multiple of 128, below 2^31: at most 24 significant bits, exact); was fild,
  * fmul, fstp and the float read back as an integer (the board's sampler: still a tenth of Quake 2's frames) */
 {uint32_t a=n<0?(uint32_t)0-(uint32_t)n:(uint32_t)n,hb;
  if(!a)return 0;
  hb=vx_bsr(a); /* v10.352: the highest set bit by BSR (was five compares and branches) */
  return (n<0?0x80000000UL:0)|((hb+116UL)<<23)|((hb<=23UL?a<<(23UL-hb):a>>(hb-23UL))&0x7FFFFFUL);}
}
/* v10.351: (uint32_t)(dd + 0.5) for a colour 0 <= dd <= 255, on the double's encoding (Watcom's conversion calls __CHP); anything else
 * (NaN) as before */
static uint32_t u8r(double dd) {
 union {double d;uint32_t w[2];} v;uint32_t e;v.d=dd+0.5;e=(v.w[1]>>20)&0x7FFUL;
 if(e<1023UL)return 0;if(e>1030UL||(v.w[1]&0x80000000UL))return (uint32_t)(dd+0.5);
 return ((v.w[1]&0xFFFFFUL)|0x100000UL)>>(1043UL-e);
}
static void output(vx3dn_vertex *o,const cv *v) {
 uint32_t k,color=0;double d;
 o->x=bits((float)v->x);o->y=bits((float)v->y);o->z=bits((float)v->z);o->rhw=bits((float)v->rhw);
 if(tri_snap){o->x=snapw(o->x,0);o->y=snapw(o->y,tri_snap==2);} /* v10.324 */
 for(k=0;k<4;k++) {d=v->c[k];if(d<0)d=0;if(d>255)d=255;color|=u8r(d)<<(k*8);} /* v10.351: u8r */
 o->color=color;
}
/* v10.221: the draw template. Everything a draw packet holds except its vertices depends on the state alone: the program (composed by
 * vx_d3d_compose), the textures (resolved through the HAL, IsBadReadPtr checks included), fog, stencil, the clip rectangle. It was all
 * redone for every triangle (board, v10.220: 42 us of HAL time a triangle, 3 of every 4 seconds of a WinTune frame). It is now built
 * once and kept until a state command, a render-target change, a clear or the end of the call; each triangle only reads, clips and
 * appends its vertices. tc.err: the state's error (every primitive returns it); tc.skip: why every primitive is left out. */
typedef struct {uint32_t fvf,coff,soff,t0,n0,t1,n1;} vx_layout; /* v10.222: byte offsets of diffuse, specular, the two coordinate sets (0 = none) and their sizes */
/* v10.358: a coordinate's float bits inside [lo, hi] (non-negative floats order as their bits), or with ACCEL_3D_GUARD on and the edge at 0,
 * between -0.5 and -0.0: the vertices Mesa's clipping puts on GL's window edge, the engine's -0.5 (the VxD takes them with the same switch) */
#define GB_IN(w,lo,hi) ((w)-(lo)<=(hi)-(lo)||(tc.guard&&!(lo)&&(w)>=0x80000000UL&&(w)<=0xBF000000UL))
static struct {uint32_t valid,id,err,skip,tex,set,set1,units,layout,s3,pre;int prog;const vx_d3d_context *ctx;vx_layout lay;uint32_t bxl,bxr,byt,byb,guard;double zb;vx3dn_packet p;} tc; /* v10.368: zb = ZBIAS's depth offset */ /* v10.236: b* = the clip edges as float bits */
static uint32_t vxs_template_valid; /* .553 (.552): the span frame's constants are this template's (vx_d3d_span.inc, vxs_tc); cleared with the template, so a template id that comes round again cannot match */
static uint32_t bq_tid;
/* .552: what a draw asks of the state before it reaches the template, kept for as long as the template may stand (ep: ended by
 * template_reset, which every state change, every DrawPrimitives2 call and every command the runtime parses already calls). GTA3 (g.rec):
 * vx_d3d_supported composed the whole pixel program for every draw token just to read its error word, 1,600 times a frame, and a template
 * build composed it again: now once for each state. ep.x is the composer's answer as it gave it (template_body takes a copy: it edits
 * its own for the colour key); the lost flag is read at each draw, not kept. vx_d3d_ep: [0] composes done, [1] answers used again,
 * [2] support answers worked out, [3] used again. */
uint32_t vx_d3d_ep[4];
/* ep_key: the driver's T&L (ACCEL_D3D_TNL) swaps the stages' TEXCOORDINDEX and TEXTURETRANSFORMFLAGS around each of its draws without a
 * template reset (tn_tss): 0 the application's stages, n + 1 the T&L's with n output sets. An answer is used again only under the key
 * it was worked out with. */
static uint32_t ep_key;
static struct {const vx_d3d_context *ctx,*sctx;uint32_t have,sup,err,ckey,skey;int bound,r;vx_d3d_xp x;} ep;
static void template_reset(void) {tc.valid=0;ep.have=0;vxs_template_valid=0;}
static int ep_compose(const vx_d3d_context *c,int bound,vx_d3d_xp *x,uint32_t *err) {
 if(!(ep.have&1)||ep.ctx!=c||ep.bound!=bound||ep.ckey!=ep_key){ep.have&=~1UL;ep.ctx=c;ep.bound=bound;ep.ckey=ep_key;ep.r=vx_d3d_compose(c,bound,&ep.x,&ep.err);ep.have|=1;vx_d3d_ep[0]++;}
 else vx_d3d_ep[1]++;
 if(x)*x=ep.x;
 *err=ep.err;return ep.r;
}
static int ep_supported(const vx_d3d_context *c) { /* vx_d3d_supported's answer */
 if(c->lost)return 0;
 if(!(ep.have&2)||ep.sctx!=c||ep.skey!=ep_key){uint32_t w=why_states(c),e=0;
  if(!(w&1)){(void)ep_compose(c,c->state.tss[0][0]!=0,0,&e);if(e)w|=0x200;}
  ep.sctx=c;ep.skey=ep_key;ep.sup=!w;ep.have|=2;vx_d3d_ep[2]++;}
 else vx_d3d_ep[3]++;
 return (int)ep.sup;
}
/* v10.428: a render state taken from the runtime's own record (the HAL's resync, vx_d3d_hal.c dp2_impl): applied as a RENDERSTATE command
 * applies it, the template built again for the next draw. Not while a state block is being recorded. */
uint32_t vx_d3d_rs_synced;
int vx_d3d_state_adopt(vx_d3d_context *c,uint32_t id,uint32_t value) {if(!c||c->recording||id>=VX_D3D_RS)return 0;c->state.rs[id]=value;template_reset();vx_d3d_rs_synced++;return 1;}
/* v10.309 (review): a state command whose every value is already the state (apps send unchanged states constantly) changes nothing that a
 * flush or a template rebuild is for: the batch's draws and the next ones are drawn under the same state. A command being recorded into a
 * state block, an id out of range or a viewport out of the target goes the usual way. The template is reset at every DrawPrimitives2 call,
 * so a surface changed between calls is still seen. */
/* v10.368 (audits N16, 2.1): render states nothing in the driver reads, which the runtime still forwards: changing one does not send the
 * batch or build the draw template again (the value is kept as before). WRAP0-7 (128-135), CLIPPING (136), LIGHTING (137), EXTENTS,
 * AMBIENT, FOGVERTEXMODE, COLORVERTEX, LOCALVIEWER, NORMALIZENORMALS, COLORKEYBLENDENABLE, the four material sources (138-148), VERTEXBLEND,
 * CLIPPLANEENABLE, SOFTWAREVERTEXPROCESSING (151-153), POINTSCALE_A/B/C (158-160), MULTISAMPLEANTIALIAS/MASK (161, 162), PATCHEDGESTYLE,
 * PATCHSEGMENTS, DEBUGMONITORTOKEN (163-165), INDEXEDVERTEXBLENDENABLE (167), TWEENFACTOR (170), POSITIONORDER, NORMALORDER (172, 173).
 * The runtime does the vertex processing on this HAL; the core reads none of these (any id it reads must not be listed here). */
static int rs_inert(uint32_t id) {return (id>=128&&id<=148)||(id>=151&&id<=153)||(id>=158&&id<=165)||id==167||id==170||id==172||id==173;}
static int state_same(const vx_d3d_context *c,uint32_t op,const uint8_t *p,uint32_t n) {
 uint32_t j;const vx_d3d_state *st=&c->state;
 if(c->recording)return 0;
 if(op==8){for(j=0;j<n;j++){uint32_t id=u32(p+j*8),v=u32(p+j*8+4);if(id>=256)return 0;if(id==VX_D3D_RS_LOGICOP||rs_inert(id))continue;
   if(st->rs[id]!=v||(id==1&&st->tss[0][0]!=v))return 0;}return 1;}
 if(op==25){for(j=0;j<n;j++){uint32_t stage=u16(p+j*8),id=u16(p+j*8+2),v=u32(p+j*8+4);if(stage>=8||id>=32)return 0;
   if(st->tss[stage][id]!=v||(id==12&&(st->tss[stage][13]!=v||st->tss[stage][14]!=v))||(!stage&&!id&&st->rs[1]!=v))return 0;}return 1;}
 for(j=0;j<n;j++){uint32_t x=u32(p+j*16),y=u32(p+j*16+4),w=u32(p+j*16+8),h=u32(p+j*16+12);
  if(x>c->target.width||y>c->target.height||w>c->target.width-x||h>c->target.height-y)return 0;
  if(st->viewport.left!=(int32_t)x||st->viewport.top!=(int32_t)y||st->viewport.right!=(int32_t)(x+w)||st->viewport.bottom!=(int32_t)(y+h))return 0;}
 return 1;
}
/* v10.354: MAXMIPLEVEL changes the largest sampled image, not the signed LOD bias.
 * Rebase the descriptor and relative offsets together; normalized coordinates keep their meaning. */
static uint32_t mip_pitch(uint32_t f,uint32_t w) {
 uint32_t r;
 if(f>=VX3DN_TEX_DXT1&&f<=VX3DN_TEX_DXT5)r=(w<4?1:w/4)*(f==VX3DN_TEX_DXT1?8UL:16UL);
 else r=w*((f==VX3DN_TEX_ARGB8888||f==VX3DN_TEX_XRGB8888||f==VX3DN_TEX_X8L8V8U8)?4UL:f==VX3DN_TEX_L8?1UL:2UL);
 return (r+31UL)&~31UL;
}
static uint32_t apply_maxmip(vx3dn_packet *p,const vx_d3d_context *c,const vx_d3d_services *sv,const vx_d3d_xp *xp,uint32_t u,int shared_cube) {
 const uint32_t *t=c->state.tss[xp->unit_stage[u]];uint32_t off[12],nl,b,k,delta,newn,base,layout,mf;
 vx3dn_surface *s=u?&p->stages.unit[u-1].surface:&p->tex.surface;
 if(!t[20])return 0;
 layout=u?p->stages.ulayout[u-1]:p->stages.layout;
 if(layout==VX3DN_LAYOUT_VOLUME||(layout==VX3DN_LAYOUT_CUBE&&!shared_cube)||!sv->mips)return VX_D3D_CONFLICTINGSTATE;
 nl=sv->mips(sv->context,t[0],off,12);
 if(!nl||nl>12||off[0]!=s->offset)return VX_D3D_WRONGTEXTUREFORMAT;
 for(k=1;k<nl;k++)if(off[k]<=off[k-1]||(off[k]&255)||((off[k]-off[0])>>8)>=0x10000UL)return VX_D3D_WRONGTEXTUREFORMAT;
 b=t[20]<nl?t[20]:nl-1;base=s->offset;delta=off[b]-base;
 s->offset=off[b];s->width>>=b;s->height>>=b;if(!s->width)s->width=1;if(!s->height)s->height=1;s->pitch=mip_pitch(s->format,s->width);
 if(layout==VX3DN_LAYOUT_CUBE){uint32_t *faces=u?p->stages.uface[u-1]:p->stages.face;
  for(k=0;k<6;k++){if(faces[k]>0xFFFFFFFFUL-delta)return VX_D3D_WRONGTEXTUREFORMAT;faces[k]+=delta;}}
 mf=t[18]==2?1:t[18]==3?2:0;newn=mf&&vx_d3d_mips?nl-b:1;
 if(!u){p->mip.levels=newn;p->mip.filter=mf;
  for(k=1;k<newn;k++)p->mip.level[k-1]=(off[b+k]-off[b])>>8;}
 else {uint32_t an=p->stages.umip[u-1]&0x1F0000UL;
  if(!(vx_d3d_feat&VX_F_UNITMIP))newn=1;
  p->stages.umip[u-1]=an|(newn>9?9:newn)|(mf<<8);
  for(k=1;k<newn&&k<=8;k++)p->stages.ulevel[u-1][k-1]=(off[b+k]-off[b])>>8;}
 return 0;
}
/* v10.201: ZBIAS (47) pulls the primitive toward the viewer by 2^-12 of the depth range a step (an approximation: XP's 01/16 encoding
 * has one sample, CLXP3D T53) */
static double zbias(const vx_d3d_context *c) {return c->state.rs[47]?(double)(c->state.rs[47]>16?16:c->state.rs[47])/4096.0:0;}
static uint32_t template_body(vx_d3d_context *c,const vx_d3d_services *sv,int validate);
/* v10.462: a template build's parts, while the phases are on (vx_d3d_ftprof; TSC cycles / 16): [0] builds measured, [1] composing the
 * program (vx_d3d_compose), [2] the textures resolved (the HAL's resolvers, faces, volumes), [3] the packet's state and texture fields
 * (mip chains included), [4] the program, colour key and constants into the packet, [5] the rest (MAXMIPLEVEL, the template's copy);
 * [6] builds whose state differed from the last build's in texture handles only, [7] in nothing at all (rebuilt after a reset); QueryPhases
 * words 212-219 */
uint32_t vx_d3d_tprof[8];static uint32_t tp_t;static uint32_t tp_rs[VX_D3D_RS],tp_tss[VX_D3D_STAGES][VX_D3D_TSS];static int tp_have;
#define TP(k) do{if(tp_t){uint32_t n_=vx_d3d_phclock();vx_d3d_tprof[k]+=(n_-tp_t+8)>>4;tp_t=n_;}}while(0)
/* .564: .559-.563 kept built templates in a table here; removed as a measured loss. Board, .563 (the tool VXTPL v1.4): with every one
 * of 384 requests a frame found, a frame took 12.99 ms against 11.96 building each template (95th percentile 13.77 against 12.25);
 * GTA3 on .560 found half and showed no gain. An entry was 3.3 KB in a 1.7 MB table: a find read some fifty cache lines the processor
 * did not hold, a build runs on code and data it does. */
static uint32_t template_build(vx_d3d_context *c,const vx_d3d_services *sv,int validate) {
 uint32_t o=VX_PH(VX_PH_STATE),rc;
 if(vx_d3d_ftprof&&vx_d3d_phclock&&!validate){uint32_t s,k,texonly=1,same;
  same=tp_have&&!memcmp(tp_rs,c->state.rs,sizeof(tp_rs))&&!memcmp(tp_tss,c->state.tss,sizeof(tp_tss));
  if(tp_have&&!same&&!memcmp(tp_rs,c->state.rs,sizeof(tp_rs))){for(s=0;s<VX_D3D_STAGES&&texonly;s++)for(k=1;k<VX_D3D_TSS;k++)if(tp_tss[s][k]!=c->state.tss[s][k]){texonly=0;break;}}else texonly=0;
  if(same)vx_d3d_tprof[7]++;else if(texonly)vx_d3d_tprof[6]++;
  memcpy(tp_rs,c->state.rs,sizeof(tp_rs));memcpy(tp_tss,c->state.tss,sizeof(tp_tss));tp_have=1;
  vx_d3d_tprof[0]++;tp_t=vx_d3d_phclock();if(!tp_t)tp_t=1;}
 rc=template_body(c,sv,validate);tp_t=0;VX_PH_BACK(o);return rc;
}
static uint32_t template_body(vx_d3d_context *c,const vx_d3d_services *sv,int validate) {
 static vx_d3d_xp xp;static vx3dn_packet p;
 uint32_t j,k,set=c->state.tss[0][11]&0xFFFF,set1,s0=0,hset[3],u,tex,perr;vx3dn_surface us[2];uint32_t face[6],vol[2];int prog,cube_mip=0;vx3dn_surface ts;
 uint32_t ul[2]={0,0},uf[2][6]={{0}},ucm[2]={0,0},s3=0; /* v10.321: units 1 and 2's layouts and faces, a cube's shared levels, the 3D sets; v10.330 (review #165): faces zeroed (a volume set fills 0..2, the packet copies all six) */
 static const uint32_t addr3[6]={2,2,3,1,0,0}; /* D3DTSS_ADDRESSW (25) as U and V */
 prog=ep_compose(c,c->state.tss[0][0]!=0,&xp,&perr);TP(1); /* .552: the draw's support check composed it already */
 tc.valid=1;tc.id++;vxs_template_valid=0;tc.ctx=c;tc.err=0;tc.skip=0;
 if(prog==-2){tc.err=perr;return 0;}
 /* v10.195: vertex fog takes its factor from specular alpha, which only XP's vertex layout carries: plain diffuse with vertex fog runs
  * XP's diffuse program (T13) */
 if(c->state.rs[28]&&!c->state.rs[35]&&prog==-1){static const uint32_t t13[4]={0x01C00530UL,0x301C0053UL,0x00F04005UL,0x000D0820UL};
  memset(&xp,0,sizeof(xp));xp.count=1;memcpy(xp.words,t13,sizeof(t13));prog=0;}
 tex=prog==0&&xp.textured;
 /* v10.206: unit 0 samples stage unit_stage[0]; the vertex carries two coordinate sets: unit 0's and one other */
 hset[0]=hset[1]=hset[2]=0;
 if(tex){s0=xp.unit_stage[0];set=xp.unit_set[0];}
 set1=set;
 for(u=1;tex&&u<xp.units;u++){if(xp.unit_set[u]==set)hset[u]=0;else if(set1==set||xp.unit_set[u]==set1){set1=xp.unit_set[u];hset[u]=1;}else{tc.skip=0x200;return 0;}}
 /* v10.211: a bump unit's dependent read takes its base coordinates from TextureB (board, v10.210: texture 1 on set 0 read set B's
  * defaults, 0 + du dv, while set 1 was right). Unit 1 is put on the B slot, which carries unit 1's set either way (set1 == set here). */
 if(tex&&xp.units>1&&(c->state.tss[s0][1]==22||c->state.tss[s0][1]==23))hset[1]=1;
 if(tex&&(!sv->resolve||!c->state.tss[s0][0]||!sv->resolve(sv->context,c->state.tss[s0][0],&ts)||!VX3DN_IS_TEX(ts.format))){tc.skip=0x400;return 0;} /* v10.173: the 16-bit formats too. v10.185: a texture that
  * cannot be sampled (no CreateSurfaceEx record, wrong format) leaves the primitive out (drop reason 400) instead of failing the batch */
 /* v10.214: a cube map on the only unit (T150): its six faces, the 9-dword s t r vertex (r in q), no mip chain; a cube that cannot be
  * sampled (faces of another size or format) or a cube with a second unit or a schedule is left out (drop reason 1000) */
 {uint32_t fr=(tex&&sv->faces)?sv->faces(sv->context,c->state.tss[s0][0],face):0;
  /* v10.317: a cube whose faces share the level layout (VX_D3D_FACES_SHARED) keeps its mip chain (ACCEL_D3D_CUBEMIP) */
  cube_mip=fr==(6|VX_D3D_FACES_SHARED)&&(vx_d3d_feat&VX_F_CUBEMIP);
  if((fr&~VX_D3D_FACES_SHARED)==6&&(vx_d3d_feat&VX_F_CUBE)&&(xp.units==1||(vx_d3d_feat&VX_F_UNIT3D))&&!xp.ps&&!xp.layout)xp.layout=VX3DN_LAYOUT_CUBE; /* v10.321: with other units too */
  else if(fr){tc.skip=0x1000;return 0;}}
 /* v10.317: a 3D texture (the ICD's): layout 3 on the only unit, its depth and slice step from the services */
 if(tex&&sv->volume&&sv->volume(sv->context,c->state.tss[s0][0],vol)){
  if((vx_d3d_feat&VX_F_VOLUME)&&(xp.units==1||(vx_d3d_feat&VX_F_UNIT3D))&&!xp.ps&&!xp.layout)xp.layout=VX3DN_LAYOUT_VOLUME;else{tc.skip=0x1000;return 0;}}
 for(u=1;tex&&u<xp.units;u++)if(!sv->resolve(sv->context,c->state.tss[xp.unit_stage[u]][0],&us[u-1])||!VX3DN_IS_TEX(us[u-1].format)){tc.skip=0x400;return 0;}
 /* v10.215: a cube map on unit 1 or 2 is not sampled as a flat texture. v10.321: it is a cube there (ACCEL_D3D_UNIT3D), as a 3D texture is a
  * volume: HCM p117-118 per texture, its set a three-dimensional one (FE/08) */
 for(u=1;tex&&u<xp.units;u++){uint32_t h=c->state.tss[xp.unit_stage[u]][0],fu=sv->faces?sv->faces(sv->context,h,uf[u-1]):0,v2[2];
  if(fu){if(!(vx_d3d_feat&VX_F_UNIT3D)||!(vx_d3d_feat&VX_F_CUBE)||(fu&~VX_D3D_FACES_SHARED)!=6||xp.ps||xp.layout==VX3DN_LAYOUT_PROJ){tc.skip=0x1000;return 0;}
   ul[u-1]=VX3DN_LAYOUT_CUBE;ucm[u-1]=fu==(6|VX_D3D_FACES_SHARED)&&(vx_d3d_feat&VX_F_CUBEMIP);}
  else if(sv->volume&&sv->volume(sv->context,h,v2)){uint32_t aw=c->state.tss[xp.unit_stage[u]][25];
   if(!(vx_d3d_feat&VX_F_UNIT3D)||!(vx_d3d_feat&VX_F_VOLUME)||xp.ps||xp.layout==VX3DN_LAYOUT_PROJ){tc.skip=0x1000;return 0;}
   ul[u-1]=VX3DN_LAYOUT_VOLUME;uf[u-1][0]=v2[0];uf[u-1][1]=v2[1];uf[u-1][2]=addr3[aw<5?aw:0];if(!(vx_d3d_feat&VX_F_BORDER)&&!uf[u-1][2])uf[u-1][2]=1;}}
 if(tex&&xp.units>1){if(xp.layout==VX3DN_LAYOUT_CUBE||xp.layout==VX3DN_LAYOUT_VOLUME)s3|=1;for(u=1;u<xp.units;u++)if(ul[u-1])s3|=1U<<hset[u];
  if(s3&&xp.units>2){tc.skip=0x1000;return 0;}} /* (two units: the VxD's layout has the two sets only) */
 TP(2);packet(&p,c,VX3DN_DRAW);
 if(c->state.rs[28])fog_registers(&p.fog,c->state.rs); /* v10.195 */
 if(tex) {
  static const uint32_t address[5]={2,2,3,1,0}; /* D3DTADDRESS_ WRAP 1, MIRROR 2, CLAMP 3, BORDER 4 -> hardware 2 wrap (XP, CLXP3D T19), 3 mirror, 1 clamp;
                                                   v10.304: 0 border (HCM p125), BORDERCOLOR (15) in sampler 35 / 36 */
  const uint32_t *t=c->state.tss[s0];
  p.tex.enable=1;p.tex.surface=ts;p.tex.mode=1;
  /* Enlargement and shrinking have independent fields in sampler 31.
   * Keep packet values 0/1 compatible with the original all-point/linear path. */
  p.tex.filter=t[16]==2?(t[17]==2?VX3DN_FILTER_LINEAR:VX3DN_FILTER_MAG_LINEAR):
                       (t[17]==2?VX3DN_FILTER_MIN_LINEAR:VX3DN_FILTER_POINT);
  p.tex.address_u=address[t[13]<5?t[13]:0];p.tex.address_v=address[t[14]<5?t[14]:0];if(!(vx_d3d_feat&VX_F_BORDER)){if(!p.tex.address_u)p.tex.address_u=1;if(!p.tex.address_v)p.tex.address_v=1;}p.stages.border[0]=t[15];
  /* v10.199: MINFILTER ANISOTROPIC (3) with MAXANISOTROPY (21) above 1; MAGFILTER ANISOTROPIC (5) magnifies linearly. MIPFILTER (18)
   * POINT / LINEAR with the texture's mip chain (CLXP3D T21-T23); MIPMAPLODBIAS (19) at 2 per level; MAXMIPLEVEL is applied below. */
  p.mip.levels=1;p.mip.aniso=1;
  if(t[17]==3){uint32_t an=t[21];p.mip.aniso=an<1?1:an>16?16:an;if(p.tex.filter==VX3DN_FILTER_POINT)p.tex.filter=VX3DN_FILTER_MIN_LINEAR;else if(p.tex.filter==VX3DN_FILTER_MAG_LINEAR)p.tex.filter=VX3DN_FILTER_LINEAR;}
  if(t[16]==5){if(p.tex.filter==VX3DN_FILTER_POINT)p.tex.filter=VX3DN_FILTER_MAG_LINEAR;else if(p.tex.filter==VX3DN_FILTER_MIN_LINEAR)p.tex.filter=VX3DN_FILTER_LINEAR;}
  if((t[18]==2||t[18]==3)&&sv->mips&&vx_d3d_mips) {
   uint32_t off[12],nl=sv->mips(sv->context,t[0],off,12),l,ok=nl>1&&nl<=12&&off[0]==ts.offset; /* v10.330 (review #85): a count over the array's 12 is refused */ /* block 10030000 holds 8 level offsets; v10.304: up to 12 levels, the VxD samples 9 (maximum level 8) */
   for(l=1;l<nl&&ok;l++)if(off[l]<=off[0]||((off[l]-off[0])&255)||((off[l]-off[0])>>8)>=0x10000UL)ok=0;
   if(ok){double b=flt(t[19]);int bi;p.mip.levels=nl;p.mip.filter=t[18]==2?1:2;for(l=1;l<nl;l++)p.mip.level[l-1]=(off[l]-off[0])>>8;
    if(!finite(t[19]))b=0;b*=2;if(b>127)b=127;if(b<-128)b=-128;bi=(int)(b<0?b-.5:b+.5);p.mip.bias=(uint32_t)bi&255;}
  }
   if(xp.units>1) { /* v10.206: units 1 and 2 (filter and address as stage 0's; no mip chain); v10.304: their mip chains and border colours */
   static const uint32_t addr2[5]={2,2,3,1,0};
   p.stages.units=xp.units;for(u=0;u<xp.units;u++)p.stages.set[u]=hset[u];p.stages.s3=s3; /* v10.321 */
   for(u=1;u<xp.units;u++){const uint32_t *t2=c->state.tss[xp.unit_stage[u]];vx3dn_unit *q=&p.stages.unit[u-1];q->surface=us[u-1];
    q->filter=(t2[16]==2||t2[16]==5)?((t2[17]==2||t2[17]==3)?VX3DN_FILTER_LINEAR:VX3DN_FILTER_MAG_LINEAR):((t2[17]==2||t2[17]==3)?VX3DN_FILTER_MIN_LINEAR:VX3DN_FILTER_POINT);
    q->address_u=addr2[t2[13]<5?t2[13]:0];q->address_v=addr2[t2[14]<5?t2[14]:0];p.stages.border[u]=t2[15];
    if(!(vx_d3d_feat&VX_F_BORDER)){if(!q->address_u)q->address_u=1;if(!q->address_v)q->address_v=1;}
    p.stages.umip[u-1]=1;p.stages.ubias[u-1]=0;
    p.stages.ulayout[u-1]=ul[u-1];for(j=0;j<6;j++)p.stages.uface[u-1][j]=ul[u-1]?uf[u-1][j]:0; /* v10.321 */
    if((t2[18]==2||t2[18]==3)&&sv->mips&&vx_d3d_mips&&(vx_d3d_feat&VX_F_UNITMIP)&&(!ul[u-1]||(ul[u-1]==VX3DN_LAYOUT_CUBE&&ucm[u-1]))){uint32_t off[12],nl=sv->mips(sv->context,t2[0],off,12),l,ok=nl>1&&nl<=12&&off[0]==q->surface.offset; /* v10.330 (review #85) */
     for(l=1;l<nl&&ok;l++)if(off[l]<=off[0]||((off[l]-off[0])&255)||((off[l]-off[0])>>8)>=0x10000UL)ok=0;
     if(ok){double b=flt(t2[19]);int bi;p.stages.umip[u-1]=(nl>9?9:nl)|((t2[18]==2?1UL:2UL)<<8);for(l=1;l<nl&&l<=8;l++)p.stages.ulevel[u-1][l-1]=(off[l]-off[0])>>8;
      if(!finite(t2[19]))b=0;b*=2;if(b>127)b=127;if(b<-128)b=-128;bi=(int)(b<0?b-.5:b+.5);p.stages.ubias[u-1]=(uint32_t)bi&255;}}
    if(t2[17]==3){uint32_t an=t2[21];if(an<1)an=1;if(an>16)an=16;p.stages.umip[u-1]|=an<<16;}}
  }
 }
 TP(3);
 if(prog>=0) { /* v10.172: XP's program and 10-dword vertices */
  p.shader.count=xp.count;for(k=0;k<xp.count*4;k++)p.shader.words[k]=xp.words[k];p.stages.c22=xp.c22;
  if(xp.tfactor){uint32_t tf=c->state.rs[60];p.shader.kflags=1;
   p.shader.konst[0]=bits_any((float)(((tf>>16)&255)/255.0));p.shader.konst[1]=bits_any((float)(((tf>>8)&255)/255.0));
   p.shader.konst[2]=bits_any((float)((tf&255)/255.0));p.shader.konst[3]=bits_any((float)((tf>>24)/255.0));}
  /* v10.197: COLORKEYENABLE (41) with a keyed texture: XP's five-instruction key test goes in front of the program (CLXP3D T68-T77: XP
   * does this for every program without a temporary). v10.198: with specular (a temporary) XP lays the program out differently (T73);
   * there the key is drawn and specular is not. */
  /* v10.208: with specular or a second stage XP keeps the texel in C0 (T73, T125): ckey_c0 */
  {uint32_t key;int keyed=0,required=tex&&c->state.rs[41]&&sv->colorkey&&sv->colorkey(sv->context,c->state.tss[0][0],&key);
   if(vx_d3d_ckey_on&&(vx_d3d_feat&VX_F_CKEYPS)&&tex&&!xp.ps&&!xp.layout&&!xp.c22&&s0==0&&c->state.rs[41]&&ts.format<VX3DN_TEX_DXT1&&sv->colorkey&&sv->colorkey(sv->context,c->state.tss[0][0],&key)&&
     (c->state.rs[29]||xp.units==2)&&ckey_c0(&xp,ts.format,key)){p.shader.count=xp.count;for(k=0;k<xp.count*4;k++)p.shader.words[k]=xp.words[k];keyed=1;}
  else if(vx_d3d_ckey_on&&tex&&xp.units<=1&&!xp.ps&&!xp.layout&&s0==0&&(c->state.tss[1][1]==1||!(vx_d3d_feat&VX_F_STAGES))&&c->state.rs[41]&&ts.format<VX3DN_TEX_DXT1&&sv->colorkey&&sv->colorkey(sv->context,c->state.tss[0][0],&key)){ /* (v10.199: not DXT) */
   if(c->state.rs[29]){vx_d3d_xp nx;uint32_t e2,save=c->state.rs[29];((vx_d3d_context *)c)->state.rs[29]=0;
    if(vx_d3d_compose(c,1,&nx,&e2)==0){xp=nx;p.shader.count=xp.count;for(k=0;k<xp.count*4;k++)p.shader.words[k]=xp.words[k];}
    ((vx_d3d_context *)c)->state.rs[29]=save;}
   if(p.shader.count<=2){for(k=p.shader.count*4;k-->0;)p.shader.words[k+20]=p.shader.words[k];for(k=0;k<20;k++)p.shader.words[k]=ckey_tex[k];
    p.shader.count+=5;p.shader.kflags|=2;key_floats(p.shader.key,ts.format,key);keyed=1;}}
   if(required&&!keyed&&validate){tc.err=VX_D3D_CONFLICTINGSTATE;return 0;}}
  /* v10.208: an explicit schedule, constants, the projected layout */
  if(xp.ps){p.stages.ps=1;p.stages.h90=xp.h90;p.stages.h91=xp.h91;p.stages.h93=xp.h93;p.stages.h95=xp.h95;p.stages.ntau=xp.ntau;for(k=0;k<4;k++)p.stages.tau[k]=xp.tau[k];}
  p.stages.nkc=xp.nkc;for(k=0;k<xp.nkc;k++){p.stages.kc_reg[k]=xp.kc_reg[k];for(j=0;j<4;j++)p.stages.kc[k][j]=xp.kc[k][j];}
  if(xp.layout&&tex){p.stages.layout=xp.layout;
   if(xp.layout==VX3DN_LAYOUT_CUBE){for(k=0;k<6;k++)p.stages.face[k]=face[k];if(!cube_mip)p.mip.levels=1;} /* v10.214: cube; v10.317: with its chain */
   else if(xp.layout==VX3DN_LAYOUT_VOLUME){ /* (addr3: D3DTSS_ADDRESSW (25) as U and V) */
    uint32_t aw=c->state.tss[s0][25];p.stages.face[0]=vol[0];p.stages.face[1]=vol[1];p.stages.face[2]=addr3[aw<5?aw:0];p.mip.levels=1;
    if(!(vx_d3d_feat&VX_F_BORDER)&&!p.stages.face[2])p.stages.face[2]=1;}}
 }
 TP(4);
 if(tex)for(u=0;u<xp.units;u++){uint32_t mr=apply_maxmip(&p,c,sv,&xp,u,u?ucm[u-1]:cube_mip);if(mr){tc.err=mr;return 0;}}
 tc.zb=zbias(c);pk_copy(&tc.p,&p);tc.tex=tex;tc.set=set;tc.set1=set1;tc.prog=prog;tc.units=tex?xp.units:0;tc.layout=(xp.layout&&tex)?xp.layout:0;tc.s3=tex?p.stages.s3:0;
 /* v10.441: prepacked vertices for this template's triangles when the words the arrays would carry are exactly the 10-word form (the
  * template's own fields agree with the packet's: textured, two units, an XP program) */
 tc.pre=vx_d3d_prepack&&VX3DN_PRE_FORM(&p)&&(tc.tex!=0)==(p.tex.enable!=0)&&((tc.tex&&tc.units>1)==(p.tex.enable&&p.stages.units>1))&&(tc.prog>=0)==(p.shader.count!=0)&&!tc.layout&&!tc.s3&&tc.units<=2;
 tc.bxl=bits((float)p.clip.left);tc.bxr=bits((float)p.clip.right);tc.byt=bits((float)p.clip.top);tc.byb=bits((float)p.clip.bottom);
 tc.guard=(vx_d3d_feat&VX_F_GUARD)!=0; /* v10.358: x or y down to -0.5 at a clip edge of 0 (ACCEL_3D_GUARD, the VxD's word) */
 tc.lay.fvf=0xFFFFFFFFUL; /* filled for the call's FVF at the first primitive */
 (void)j;(void)k;TP(5);
 return 0;
}
static uint32_t template_status(const vx_d3d_context *c,const vx_d3d_services *sv) {
 uint32_t rc;template_reset(); /* .553: ValidateDevice comes outside a DrawPrimitives2 call: nothing kept from the last call answers it (.552 used the last call's composed program when the context's address and its texture binding matched: a context created at a destroyed one's address was validated with the old one's stages) */
 template_build((vx_d3d_context*)c,sv,1);rc=tc.err?tc.err:tc.skip?VX_D3D_CONFLICTINGSTATE:0;template_reset();return rc;
}
/* the primitive's vertices into packet d at d->vertex_count (the template's per-vertex fields). v10.221 (review): a texture coordinate
 * that is not finite as a float (perspective division by a tiny clipped rhw) is sent as 0; the VxD refuses the packet otherwise. */
static uint32_t fbits(double d) {uint32_t b=bits_any((float)d);return finite(b)?b:0;}
static void put_vertices(vx3dn_packet *d,const cv *vb,const uint32_t *ord,uint32_t no) {
 uint32_t j,b=d->vertex_count;
 if(vx_d3d_geo_on)vx_d3d_geo[12]+=no; /* v10.440 */
 if(d->pre.on){ /* v10.441: the same words in the 10-word form */
  for(j=0;j<no;j++){const cv *q=&vb[ord[j]];vx3dn_vertex o;uint32_t *w=&d->pre.w[(b+j)*10],sp=0,ch;double dd;output(&o,q);
   w[0]=o.x;w[1]=o.y;w[2]=o.z;w[3]=o.rhw;w[4]=o.color;
   if(tc.prog>=0)for(ch=0;ch<4;ch++){dd=q->s[ch];if(dd<0)dd=0;if(dd>255)dd=255;sp|=u8r(dd)<<(ch*8);}
   w[5]=sp;
   if(tc.tex){w[6]=fbits(q->u);w[7]=fbits(q->v);}else{w[6]=0;w[7]=0;}
   if(tc.tex&&tc.units>1){w[8]=fbits(q->u1);w[9]=fbits(q->v1);}else{w[8]=w[6];w[9]=w[7];}}
  d->vertex_count=b+no;return;}
 for(j=0;j<no;j++) {
  const cv *q=&vb[ord[j]];output(&d->vertices[b+j],q);
  /* v10.272: every array entry written (0 where the state does not use it): the packet's arrays are no longer cleared */
  if(tc.tex){d->tex.uv[b+j][0]=fbits(q->u);d->tex.uv[b+j][1]=fbits(q->v);}else{d->tex.uv[b+j][0]=0;d->tex.uv[b+j][1]=0;}
  if(tc.tex&&tc.units>1){d->stages.uv1[b+j][0]=fbits(q->u1);d->stages.uv1[b+j][1]=fbits(q->v1);}else{d->stages.uv1[b+j][0]=0;d->stages.uv1[b+j][1]=0;}
  if(tc.prog>=0){uint32_t ch,sp=0;double dd;for(ch=0;ch<4;ch++){dd=q->s[ch];if(dd<0)dd=0;if(dd>255)dd=255;sp|=u8r(dd)<<(ch*8);} /* v10.351: u8r */d->shader.spec[b+j]=sp;}else d->shader.spec[b+j]=0;
  d->stages.q[b+j]=(tc.layout||(tc.s3&1))?fbits(q->q):0;
  d->stages.r1[b+j]=(tc.s3&2)?fbits(q->q1):0; /* v10.321 */
 }
 d->vertex_count=b+no;
}
/* v10.236: triangle strips (ACCEL_D3D_STRIPS, VX_F_STRIPS; with batching). A batched triangle packet keeps its vertices as runs
 * (packet bursts): a triangle list run, or a strip run. A fast-path triangle that shares an edge with the list triangle just before it
 * turns that triangle into the head of a strip (its vertices reordered: the unshared one first, then the shared two in the new
 * triangle's order) and adds one vertex; while each next triangle holds the strip's last two vertices it adds one vertex. A grid row
 * (WinTune's floor and walls, the sphere's bands) becomes one strip: about one vertex a triangle instead of three. The engine does not
 * cull, so the alternating winding of a strip does not matter (the HAL culls). st_*: the strip at the end of the batch (VB indices of
 * its last two vertices), lt_*: the list triangle at the end of the batch (VB indices, in packet order; the indices count only for the
 * same vertex buffer, stride and FVF). Which of the two shared
 * vertices the head ends with decides which edge the strip goes on through: while the strip is just its head and one vertex
 * (st_alt: the head's other shared vertex), a triangle on the edge (st_alt, x) swaps the head's two shared vertices, the same two
 * triangles, so a grid row (a b c), (c b d), (c d e) ... continues as a b c d e ... */
static uint32_t st_on,st_a,st_b,st_alt,lt_ok,lt[3],st_stride,st_fvf;static const uint8_t *st_vb;uint32_t vx_d3d_strip_vertices;
/* v10.440: the HAL's geometry counters (VX_D3D_QueryPhases words 148-171, on while the phases are): [0] triangles in, [1] of their
 * vertex references the first use of that index in the DrawPrimitives2 call (and vertex buffer): the source vertices, [2] the fast path
 * declined (outside the guard band, the depth range, rhw), [3] fast path culled or no area, [4] fast path: a strip's next triangle (one
 * vertex), [5] a strip's start (one vertex, the list triangle before it reordered), [6] a list triangle (three vertices), [7] the general
 * path (vertex conversion and clip), [8] of those clipped, [9] of those culled or no area, [10] the fast path not tried (flat shading,
 * point or wireframe fill), [11] fast-path vertices written, [12] general-path vertices written, [13..16] triangles sharing 0, 1, 2, 3
 * indices with the triangle before them in the call, [17..20] batches sent because the next primitive had another state, another kind,
 * no room in 192 vertices, no room for another run, [21] DrawPrimitives2 calls, [22] vertex buffers changed within a call, [23] indices
 * above 65535 (counted as first uses) */
uint32_t vx_d3d_cut_hist[16]; /* .573: cuts by cause: [tag] a plain packet about to open, [8 + tag] an indexed one: 1 another template, 2 another kind, 3 no room, 4 no room for a run, 5 the other packet form, 6 no room for indices */
uint32_t vx_d3d_geo[24],vx_d3d_geo_on;
/* v10.441: ACCEL_D3D_PREPACK (the HAL sets it from the VxD's feature word bit 26; 0 here: the OpenGL ICD and the host tests) */
uint32_t vx_d3d_prepack,vx_d3d_pre_stat[2]; /* stat: packets sent prepacked, not */
static uint16_t geo_st[65536];static uint32_t geo_gen,geo_prev[3],geo_prev_ok,geo_skip;static const uint8_t *geo_vb;
static void geo_call(void) {if(vx_d3d_geo_on){vx_d3d_geo[21]++;geo_vb=0;geo_prev_ok=0;}}
static void geo_tri(const vx_d3d_input *in,const uint32_t *idx) {
 uint32_t j,m=0;
 vx_d3d_geo[0]++;
 if(in->vertices!=geo_vb){if(geo_vb)vx_d3d_geo[22]++;geo_vb=in->vertices;geo_prev_ok=0;if(++geo_gen>0xFFFFUL){memset(geo_st,0,sizeof(geo_st));geo_gen=1;}}
 for(j=0;j<3;j++){uint32_t i=idx[j];if(i>0xFFFFUL){vx_d3d_geo[1]++;vx_d3d_geo[23]++;}else if(geo_st[i]!=(uint16_t)geo_gen){geo_st[i]=(uint16_t)geo_gen;vx_d3d_geo[1]++;}
  if(geo_prev_ok&&(i==geo_prev[0]||i==geo_prev[1]||i==geo_prev[2]))m++;}
 vx_d3d_geo[13+m]++;geo_prev[0]=idx[0];geo_prev[1]=idx[1];geo_prev[2]=idx[2];geo_prev_ok=1;
}
static void strip_forget(void) {st_on=0;lt_ok=0;}
/* v10.481: no strips the core makes itself with the engine's flat shading (its first-vertex rule is proven for lists only); v10.482: but the
 * ICD's own strips (vx_d3d_gl_strip, gl_strip_on): their vertices' colours are one, or laid out for the rule vx_d3d_gl_flatpv names */
static int gl_strip_on;
static int strips_on(uint32_t prim) {return (vx_d3d_feat&VX_F_STRIPS)&&(vx_d3d_feat&VX_F_BATCH)&&prim==0&&(!(tc.p.ext.dither&2)||gl_strip_on);}
/* the bursts after no list vertices are appended: 1, or 0 when a new run would not fit */
static int list_room(const vx3dn_packet *d) {return d->bursts.n&&(d->bursts.b[d->bursts.n-1]>>16)==VX3DN_TOPO_LIST?1:d->bursts.n<VX3DN_MAX_BURSTS;}
static void list_add(vx3dn_packet *d,uint32_t no) {
 if(d->bursts.n&&(d->bursts.b[d->bursts.n-1]>>16)==VX3DN_TOPO_LIST)d->bursts.b[d->bursts.n-1]+=no;
 else d->bursts.b[d->bursts.n++]=(VX3DN_TOPO_LIST<<16)|no;
}
static void swap_vertex(vx3dn_packet *d,uint32_t i,uint32_t j) {
 vx3dn_vertex v;uint32_t t,k;
 if(d->pre.on){uint32_t *a=&d->pre.w[i*10],*b=&d->pre.w[j*10];for(k=0;k<10;k++){t=a[k];a[k]=b[k];b[k]=t;}return;} /* v10.441 */
 v=d->vertices[i];d->vertices[i]=d->vertices[j];d->vertices[j]=v;
 for(k=0;k<2;k++){t=d->tex.uv[i][k];d->tex.uv[i][k]=d->tex.uv[j][k];d->tex.uv[j][k]=t;t=d->stages.uv1[i][k];d->stages.uv1[i][k]=d->stages.uv1[j][k];d->stages.uv1[j][k]=t;}
 t=d->shader.spec[i];d->shader.spec[i]=d->shader.spec[j];d->shader.spec[j]=t;t=d->stages.q[i];d->stages.q[i]=d->stages.q[j];d->stages.q[j]=t;
 t=d->stages.r1[i];d->stages.r1[i]=d->stages.r1[j];d->stages.r1[j]=t; /* v10.321 */
}
/* the packet the primitive's no vertices go into: the batch when it was made from this template for this primitive kind and has room,
 * else the batch is sent and a new one started; without batching a single packet, sent by close_slot */
static vx3dn_packet one;
static uint32_t open_slot(vx_d3d_context *c,const vx_d3d_services *sv,uint32_t no,uint32_t prim,uint32_t *completed,vx3dn_packet **d) {
 uint32_t rc;int st=strips_on(prim);
 if(!(vx_d3d_feat&VX_F_BATCH)){pk_copy(&one,&tc.p);one.ext.prim=prim;one.bursts.n=0;one.pre.on=tc.pre&&!prim;*d=&one;return 0;}
 /* v10.483: a batch's runs follow the batch, not this call: strips_on changes within a template since v10.482 (the engine's flat shading
  * takes strips only from the ICD's own strip calls, gl_strip_on), so list vertices joining a batch that has runs are counted in a run
  * (the triangles of a flat list after a flat strip run in one packet went uncounted: the VxD refused the packet), and the first run of
  * a batch that has list vertices and no runs counts them first */
 if(bq_on&&bq_tid==tc.id&&bq.ext.prim==prim&&bq.pre.on!=2&&bq.vertex_count+no<=VX3DN_MAX_VERTICES&&(!(st||bq.bursts.n)||list_room(&bq))){vx_d3d_batched++;*d=&bq;
  if(st&&!bq.bursts.n&&bq.vertex_count)bq.bursts.b[bq.bursts.n++]=(VX3DN_TOPO_LIST<<16)|bq.vertex_count;
  if(st||bq.bursts.n)list_add(&bq,no);return 0;}
 if(bq_on){vx_d3d_cut_tag=bq_tid!=tc.id?1:bq.ext.prim!=prim?2:bq.pre.on==2?5:bq.vertex_count+no>VX3DN_MAX_VERTICES?3:4;vx_d3d_cut_hist[vx_d3d_cut_tag]++;if(vx_d3d_geo_on)vx_d3d_geo[16+vx_d3d_cut_tag]++;if(ft_sm)vx_d3d_ft[15]++;
  {uint32_t f0=ft_sm?vx_d3d_phclock():0;rc=vx_d3d_flush(c,sv,completed);if(ft_sm)vx_d3d_ft[19]+=(vx_d3d_phclock()-f0+8)>>4;} /* v10.445: the send's own time (in [5]) */
  if(rc)return rc;}
 pk_copy(&bq,&tc.p);bq.ext.prim=prim;bq.pre.on=tc.pre&&!prim;bq_tid=tc.id;bq_on=1;bq_cmd=cur_cmd;*d=&bq;strip_forget();bq.bursts.n=0;bq_ep=0xFFFFFFFFUL;if(st)list_add(&bq,no);return 0;
}
static uint32_t close_slot(vx_d3d_context *c,const vx_d3d_services *sv,vx3dn_packet *d,uint32_t *completed) {return d==&one?submit(sv,&one,c,completed):0;}
static uint32_t emit(vx_d3d_context *c,const vx_d3d_services *sv,const cv *vb,const uint32_t *ord,uint32_t no,uint32_t prim,uint32_t *completed) {
 vx3dn_packet *d;uint32_t rc=open_slot(c,sv,no,prim,completed,&d);if(rc)return rc;strip_forget();
 put_vertices(d,vb,ord,no);return close_slot(c,sv,d,completed);
}
/* v10.222: the fast path. A triangle drawn as the runtime sent it (no ZBIAS, no flat shading) whose three vertices are finite, inside the
 * viewport and the depth range, with rhw > 0, needs no clipping: its words go into the packet straight from the vertex buffer, as the
 * general path would write them (x y z with -0 as +0, the colour and specular bytes, texture words that are not finite as 0, q 1.0 when
 * the set has no third coordinate). Returns 1 when it handled the triangle (*rc set), 0 for the general path. tc.lay is the template's
 * vertex layout for this call's FVF. */
/* one fast-path vertex into packet slot b (v10.222; v10.236: a function, the strip path writes single vertices) */
/* v10.366: the template's fields read once into locals (Watcom reloads a global after every store through a pointer, the packet's arrays
 * here: each tc.* test was a load after the stores before it), the stores in array order; the same words as before */
/* v10.441: the prepacked form: the same words, ten stores in order into one run of the packet (fast_vertex wrote six arrays) */
/* v10.490: ACCEL_OPENGL_SSESNAP: the ICD's SSE pass leaves each vertex's 1/16-grid x and y words (snapw's, four lanes at a time) for the
 * strip path (board, Lattice v10.489: snapw 2.7% of the samples). vx_d3d_gl_presnap names them for the draw (xy[2 i], xy[2 i + 1] for
 * vertex i < n of the store at base, by the rule mode: tri_snap's 1, 2); vx_d3d_gl_strip takes them when the call's store and rule are
 * those, for vertex indices under n (the clipper's new vertices, past the draw's count, by snapw) */
static const uint32_t *ps_xy;static uint32_t ps_n,ps_mode,ps_lim;static const uint8_t *ps_base;uint32_t vx_d3d_ps_used,vx_d3d_cullrun_tris;
static void gw_new(void);
void vx_d3d_gl_presnap(const uint32_t *xy,uint32_t n,uint32_t mode,const uint8_t *base) {gw_new();ps_xy=xy;ps_n=xy?n:0;ps_mode=mode;ps_base=base;}
const uint32_t *vx_d3d_gl_presnap_get(uint32_t *n,uint32_t *mode,const uint8_t **base) {*n=ps_n;*mode=ps_mode;*base=ps_base;return ps_xy;}
uint32_t vx_d3d_snapw(uint32_t w,int up) {return snapw(w,up);}
/* v10.492: ACCEL_OPENGL_FLATNOCOPY: the ICD's flat strip on the engine's flat shading (vx_d3d_gl_flatpv 3, a quad strip 4) sends the store's
 * own vertices; the strip run gives the vertex at strip position k the colours of the vertex at position k + 2 (a quad strip, pair-swapped:
 * (k & ~1) + 2), the engine's first-vertex rule then colouring each triangle as GL's last (provoking) vertex; the last two positions keep
 * their own (no triangle starts there). The specular word: its colour bytes too when vx_d3d_gl_flatspec (a secondary colour), its fog byte
 * the vertex's own. Was a copy of every vertex made by the ICD (board, Lattice v10.489: flat_hwstrip 3.3% of the samples). */
uint32_t vx_d3d_gl_flatspec;static const uint32_t *cs_s;static uint32_t cs_n,cs_on,cs_quad;
static const uint8_t *cs_vb;static uint32_t cs_stride; /* the store (gl_in's, set with cs_on) */
static const uint8_t *cs_pc(uint32_t pos,const uint8_t *p) {uint32_t q;if(!cs_on)return p;q=cs_quad?(pos&~1UL)+2:pos+2;return q<cs_n?cs_vb+cs_s[q]*cs_stride:p;}
static uint32_t cs_spec(const uint8_t *p,const uint8_t *pc,uint32_t soff) {uint32_t w=u32(p+soff);return pc==p||!vx_d3d_gl_flatspec?w:(w&0xFF000000UL)|(u32(pc+soff)&0x00FFFFFFUL);}
static void fast_vertex_pre_w(uint32_t *w,const uint8_t *p,uint32_t ix,const uint8_t *pc) {
 const uint32_t coff=tc.lay.coff,t0=tc.lay.t0,t1=tc.lay.t1,tex=tc.tex,two=tc.tex&&tc.units>1;const int prog=tc.prog,snap=tri_snap;
 const uint32_t n0=tc.lay.n0,n1=tc.lay.n1,soff=tc.lay.soff;
 uint32_t x=u32(p),y=u32(p+4),z=u32(p+8),u=0,vv=0;
 vx_d3d_geo[11]+=vx_d3d_geo_on;
 if(!(x&0x7FFFFFFFUL))x=0;if(!(y&0x7FFFFFFFUL))y=0;if(!(z&0x7FFFFFFFUL))z=0;
 if(snap){if(ix<ps_lim){x=ps_xy[2*ix];y=ps_xy[2*ix+1];vx_d3d_ps_used++;}else{x=snapw(x,0);y=snapw(y,snap==2);}} /* v10.490: the pass's words (-0 as 0 there too) */
 if(tc.zb!=0){double f=(double)flt(z)-tc.zb;z=f>0?bits((float)f):0;}
 if(tex&&t0){u=u32(p+t0);if(!finite(u))u=0;if(n0>=2){vv=u32(p+t0+4);if(!finite(vv))vv=0;}}
 w[0]=x;w[1]=y;w[2]=z;w[3]=u32(p+12);w[4]=coff?u32(pc+coff):0xFFFFFFFFUL;
 w[5]=prog>=0?(soff?cs_spec(p,pc,soff):0xFF000000UL):0;w[6]=u;w[7]=vv;
 if(two){uint32_t u1=0,v1=0;if(t1){u1=u32(p+t1);if(!finite(u1))u1=0;if(n1>=2){v1=u32(p+t1+4);if(!finite(v1))v1=0;}}w[8]=u1;w[9]=v1;}
 else{w[8]=u;w[9]=vv;}
}
static void fast_vertex_pre(vx3dn_packet *d,uint32_t b,const uint8_t *p,uint32_t ix,const uint8_t *pc) {fast_vertex_pre_w(&d->pre.w[b*10],p,ix,pc);}
static void fast_vertex_arrays(vx3dn_packet *d,uint32_t b,const uint8_t *p,uint32_t ix,const uint8_t *pc);
static void fast_vertex(vx3dn_packet *d,uint32_t b,const uint8_t *p) {if(d->pre.on)fast_vertex_pre(d,b,p,0xFFFFFFFFUL,p);else fast_vertex_arrays(d,b,p,0xFFFFFFFFUL,p);}
/* v10.490: the same for the store's vertex ix (the strip path: the pass's snapped words when ix < ps_lim) */
/* v10.492: pc the vertex whose colours it takes (cs_pc; p itself but for a flat strip without the copy) */
static uint32_t gw_on;static void gw_vertex(uint32_t *w,const uint8_t *p,uint32_t ix,const uint8_t *pc); /* .503: below; .505: pc too */
static void fast_vertex_ix(vx3dn_packet *d,uint32_t b,const uint8_t *p,uint32_t ix,const uint8_t *pc) {if(d->pre.on){if(gw_on)gw_vertex(&d->pre.w[b*10],p,ix,pc);else fast_vertex_pre(d,b,p,ix,pc);}else fast_vertex_arrays(d,b,p,ix,pc);}
static void fast_vertex_arrays(vx3dn_packet *d,uint32_t b,const uint8_t *p,uint32_t ix,const uint8_t *pc) {
 vx3dn_vertex *o=&d->vertices[b];
 const uint32_t coff=tc.lay.coff,t0=tc.lay.t0,t1=tc.lay.t1,tex=tc.tex,two=tc.tex&&tc.units>1,qon=tc.layout||(tc.s3&1);const int prog=tc.prog,snap=tri_snap;
 const uint32_t n0=tc.lay.n0,n1=tc.lay.n1,soff=tc.lay.soff,s3b=tc.s3&2;
 uint32_t x=u32(p),y=u32(p+4),z=u32(p+8),w=u32(p+12),col,u=0,vv=0,u1=0,v1=0,q=0;
 vx_d3d_geo[11]+=vx_d3d_geo_on; /* v10.440 */
 if(!(x&0x7FFFFFFFUL))x=0;if(!(y&0x7FFFFFFFUL))y=0;if(!(z&0x7FFFFFFFUL))z=0;
 if(snap){if(ix<ps_lim){x=ps_xy[2*ix];y=ps_xy[2*ix+1];vx_d3d_ps_used++;}else{x=snapw(x,0);y=snapw(y,snap==2);}} /* v10.324; v10.490: the pass's words */
 if(tc.zb!=0){double f=(double)flt(z)-tc.zb;z=f>0?bits((float)f):0;} /* v10.368 (audit N17): ZBIAS as primitive() applies it; it sent every
  * triangle through the general path */
 col=coff?u32(pc+coff):0xFFFFFFFFUL;
 if(tex&&t0){u=u32(p+t0);if(!finite(u))u=0;if(n0>=2){vv=u32(p+t0+4);if(!finite(vv))vv=0;}}
 if(two&&t1){u1=u32(p+t1);if(!finite(u1))u1=0;if(n1>=2){v1=u32(p+t1+4);if(!finite(v1))v1=0;}}
 if(qon){q=t0&&n0>=3?u32(p+t0+8):0x3F800000UL;if(!finite(q))q=0x3F800000UL;}
 o->x=x;o->y=y;o->z=z;o->rhw=w;o->color=col;
 /* v10.272: every array entry written (0 where unused): the arrays are no longer cleared */
 d->tex.uv[b][0]=u;d->tex.uv[b][1]=vv;
 d->stages.uv1[b][0]=u1;d->stages.uv1[b][1]=v1;
 d->shader.spec[b]=prog>=0?(soff?cs_spec(p,pc,soff):0xFF000000UL):0;
 d->stages.q[b]=q;
 if(s3b){uint32_t r=t1&&n1>=3?u32(p+t1+8):0x3F800000UL;d->stages.r1[b]=finite(r)?r:0x3F800000UL;}else d->stages.r1[b]=0; /* v10.321 */
}
/* v10.444: the fast path's parts, sampled (one triangle in 16 of the triangle-token loop, while the phases are on; vx_d3d_phclock, TSC
 * cycles / 16): [0] triangles sampled, [1] the vertices fetched and range-checked (x y z rhw), [2] the area (doubles) and the cull, [3] the
 * strip and pair matching (with the head's reorder), [4] the vertices written (10-word form or the arrays), [5] opening the batch slot
 * (template copy for a new batch; the flush of the old one, its queue copy and VxD call, when it ends one), [6] the rest (counts, the
 * strip state, the slot's close), [7] declined (the general path follows), [8] culled or no area, [9] strip next or start, [10] the loop
 * around it (the triangle's whole time in the token loop, the geometry counter's call included), [11] the geometry counter's call (a
 * profiling cost), [12] loop iterations sampled, [13] the clock's own cost (two reads back to back), [14] clock pairs, [15] sampled
 * triangles whose slot opening sent a batch, [16] vertex index checks made (the check loop before the triangles), [17] skipped as checked
 * earlier in the call, [18] list triangles (three vertices) */
uint32_t vx_d3d_ftprof;static uint32_t ft_cnt,ft_t; /* (vx_d3d_ft, ft_sm: above, with the batch) */
#define FT(k) do{if(ft_sm){uint32_t n_=vx_d3d_phclock();vx_d3d_ft[k]+=(n_-ft_t+8)>>4;ft_t=n_;vx_d3d_ft[26]++;}}while(0) /* v10.453: [26] the clock reads in the samples */
/* The recipe of a template and vertex format (the offsets, the specular source, the units), worked out once: the ICD's grid words
 * (gw_vertex) write a store vertex from it. .527: Direct3D's use of it (v10.445's ACCEL_D3D_VRECIPE, the VxD's feature word bit 29, with
 * .526's fixed-offset form for FVF 0x1C4) removed: no consistent saving in four GTA3 A/B runs (.523-.526); fast_vertex_pre_w writes
 * the same ten words. v10.445's ACCEL_D3D_FASTMATCH (bit 30) removed with it: it never saved time. */
typedef struct {uint32_t id,fvf,ok,coff,ut0,vt0,ut1,vt1,soff,specc,two,kind;} vx_vrec;
static vx_vrec vr;
static int vr_ready(void) {
 if(vr.id!=tc.id||vr.fvf!=tc.lay.fvf){const uint32_t tex=tc.tex,t0=tc.lay.t0,t1=tc.lay.t1;
  vr.id=tc.id;vr.fvf=tc.lay.fvf;vr.coff=tc.lay.coff;vr.ut0=tex&&t0?t0:0;vr.vt0=tex&&t0&&tc.lay.n0>=2?t0+4:0;
  vr.two=tc.tex&&tc.units>1;vr.ut1=vr.two&&t1?t1:0;vr.vt1=vr.two&&t1&&tc.lay.n1>=2?t1+4:0;
  vr.soff=tc.prog>=0?tc.lay.soff:0;vr.specc=tc.prog>=0?0xFF000000UL:0;vr.ok=tc.zb==0;
  /* .505: the store layouts the ICD's draws use most, for gw_vertex's fixed-offset forms: 1 colour, one coordinate pair (Quake 2 and 3: FVF
   * 0x144), 2 colour, specular, one pair (fog or a secondary colour); 0 any other (the recipe's offsets) */
  vr.kind=vr.two?0:vr.coff==16&&!vr.soff&&vr.specc==0xFF000000UL&&vr.ut0==20&&vr.vt0==24?1:vr.coff==16&&vr.soff==20&&vr.ut0==24&&vr.vt0==28?2:
   vr.coff==16&&vr.soff==20&&!vr.ut0&&!vr.vt0?3:0;} /* .505: 3 colour, specular, no coordinates (lit and fogged, untextured: Lattice's) */
 return vr.ok&&!tri_snap;
}
/* .503 (ACCEL_OPENGL_GLWORDS; host bench, .502: fast_vertex_pre 7% of Quake 3's frame and 5% of Quake 2's, about 120 instructions a vertex):
 * the ICD's strip and triangle-list calls write a store vertex's ten prepacked words from the recipe (vr, the template's offsets in locals)
 * with the snap: the ICD's grid words (ps_lim) or snapw, as fast_vertex_pre. Only for a vertex that takes its own colours (pc == p) and
 * without ZBIAS (else fast_vertex_pre). The same ten words. vx_d3d_gw[0]: vertices written from the recipe. (Each store vertex's words
 * kept for its later uses in the run was slower on the host, Quake 3's batches: +7% of the driver's time, few vertices used again.)
 * gw_ok: the triangle-list run's vertex checks (GLLIST, below), by store number under GW_N, valid while their tag is gw_g: a run
 * (vx_d3d_gl_runstart .. the next call of any other kind: gl_begin, gl_prim, gl_tris, a strip, unjoin, the grid words, the state) starts
 * them again (gw_new). */
#define GW_N 4096
uint32_t vx_d3d_gl_glwords=1,vx_d3d_gl_gllist=1,vx_d3d_gw[4]; /* [2] triangles through vx_d3d_gl_trilist's own checks (GLLIST) */
static uint32_t gw_ok[GW_N],gw_g=1,gw_tid,gw_fvf=0xFFFFFFFFUL;
static void gw_new(void) {if(!++gw_g){memset(gw_ok,0,sizeof(gw_ok));gw_g=1;}}
/* .505 (ACCEL_OPENGL_GLWORDS 2, the default): the two layouts above with their offsets fixed: the words loaded and stored in order with no
 * offset tests (Quake 3's vertex 81 instructions on the host with the recipe); the same checks and words as the recipe's */
#define GW_FIXED(COFF,SOFF,UT,VT) do{uint32_t x=u32(p),y=u32(p+4),z=u32(p+8),u=(UT)?u32(p+(UT)):0,v=(VT)?u32(p+(VT)):0;\
 if(!(z&0x7FFFFFFFUL))z=0;\
 if(snap&&ix<ps_lim){x=ps_xy[2*ix];y=ps_xy[2*ix+1];vx_d3d_ps_used++;}\
 else{if(!(x&0x7FFFFFFFUL))x=0;if(!(y&0x7FFFFFFFUL))y=0;if(snap){x=snapw(x,0);y=snapw(y,snap==2);}}\
 if(!finite(u))u=0;if(!finite(v))v=0;\
 w[0]=x;w[1]=y;w[2]=z;w[3]=u32(p+12);w[4]=u32(pc+(COFF));w[5]=(SOFF)?cs_spec(p,pc,(SOFF)):0xFF000000UL;w[6]=u;w[7]=v;w[8]=u;w[9]=v;vx_d3d_gw[1]++;return;}while(0)
uint32_t vx_d3d_gw_fixed=1; /* .505: ACCEL_OPENGL_GLWORDS 2 (1: the recipe only) */
static void gw_vertex(uint32_t *w,const uint8_t *p,uint32_t ix,const uint8_t *pc) { /* .505: pc the vertex whose colours it takes (cs_pc: the engine's flat strips), as fast_vertex_pre */
 uint32_t x,y,z,u=0,v=0,o;const uint32_t snap=tri_snap;
 if(vr.id!=tc.id||vr.fvf!=tc.lay.fvf)vr_ready();
 if(!vr.ok){fast_vertex_pre_w(w,p,ix,pc);return;}
 vx_d3d_geo[11]+=vx_d3d_geo_on;
 if(vr.kind&&vx_d3d_gw_fixed){vx_d3d_gw[0]++;if(vr.kind==1)GW_FIXED(16,0,20,24);if(vr.kind==2)GW_FIXED(16,20,24,28);GW_FIXED(16,20,0,0);}
 x=u32(p);y=u32(p+4);z=u32(p+8);
 if(!(x&0x7FFFFFFFUL))x=0;if(!(y&0x7FFFFFFFUL))y=0;if(!(z&0x7FFFFFFFUL))z=0;
 if(snap){if(ix<ps_lim){x=ps_xy[2*ix];y=ps_xy[2*ix+1];vx_d3d_ps_used++;}else{x=snapw(x,0);y=snapw(y,snap==2);}}
 if((o=vr.ut0)!=0){u=u32(p+o);if(!finite(u))u=0;}
 if((o=vr.vt0)!=0){v=u32(p+o);if(!finite(v))v=0;}
 w[0]=x;w[1]=y;w[2]=z;w[3]=u32(p+12);w[4]=vr.coff?u32(pc+vr.coff):0xFFFFFFFFUL;w[5]=vr.soff?cs_spec(p,pc,vr.soff):vr.specc;w[6]=u;w[7]=v;
 if(vr.two){uint32_t u1=0,v1=0;if((o=vr.ut1)!=0){u1=u32(p+o);if(!finite(u1))u1=0;}if((o=vr.vt1)!=0){v1=u32(p+o);if(!finite(v1))v1=0;}w[8]=u1;w[9]=v1;}
 else{w[8]=u;w[9]=v;}
 vx_d3d_gw[0]++;
}
/* v10.470 (ACCEL_D3D_INDEXED; the HAL sets vx_d3d_indexed from the VxD's DriverInit answer, bit 7, only when the VxD has its vertex area):
 * the fast path's triangles go into an indexed packet (pre.on = 2, VX3DN_TOPO_INDEXED): each distinct source vertex converted once into the
 * packet's 10-word vertices, the triangle's three source indices mapped to packet-local ones (ix_*: a 512-entry table by source index, its
 * entries this packet's and this vertex buffer's while their generation is ix_g), the local indices appended. No expansion to three
 * vertices a triangle, no strip or pair matching. A packet ends at 192 vertices, VX3DN_MAX_INDICES indices, another template, or a
 * primitive through any other path (open_slot does not add to an indexed packet). vx_d3d_ix: [0] triangles, [1] vertices converted, [2]
 * packets begun, [3] vertex buffer changes inside a packet. */
uint32_t vx_d3d_indexed,vx_d3d_ix[4];
/* v10.473: the triangles by draw command and path (QueryPhases 220-235; counted per token, not per triangle): [0..7] the triangles of
 * commands 3 (indexed list), 18 (list), 19 (strip), 20 (indexed strip), 21 (fan), 22 (indexed fan), 26 (indexed list 2), 23 (fan with its
 * vertices in the command); [8] those that went through ix_list, [9] of them culled or with no area, [10] its stops, [11] the triangles of
 * indexed lists in its tokens taken by the per-triangle path (stops, the template not built yet or not on the fast form), [12] indexed lists
 * kept out of ix_list with ACCEL_D3D_INDEXED off, [13] kept out with an index at or past the vertex data's end (vv_lim); v10.474: of [11],
 * [14] with the template not on the fast path (not built yet, another FVF, flat shading, not solid fill, an error or a skip), [15] on it
 * but not in the prepacked (10-word) form or with batching off; the rest of [11] are stops (the triangle ix_list stopped at). */
uint32_t vx_d3d_tk[16];
#define IXS_MAX 1024 /* .555: the triangles of an indexed strip drawn as a list (dp2_body) */
static uint16_t ixs_tri[IXS_MAX*3];uint32_t vx_d3d_ixs[4],vx_d3d_fan[3];
/* .557: a feature under test: a list run whose span holds a vertex that fails, drawn by the vertices it uses (ix_pass, ix_ref). 1 = on
 * (the default); a tool turns it off and on for its A/B (VX_D3D_QueryPhases bit 29 sets it from bit 28; no registry value). TO BE REMOVED
 * with its tests of 0 once the board confirms it. (.555-.556 it switched the strips as lists, the fans passed through and the tokens
 * taken whole: confirmed on the board with VXPASS v1.1 and GTA3 on .556, always on now.) */
/* .558: vx_d3d_pass (the run-time switch of .555-.557) removed: its last use, ix_ref, is confirmed on the board and always on */
#define IX_H 1024 /* .529: 384 vertices a packet (was 512 for 192) */
static uint32_t ix_key[IX_H],ix_gen[IX_H],ix_g=1,ix_ni,ix_stride,ix_fvf;static uint16_t ix_val[IX_H];static const uint8_t *ix_vb;
/* v10.472: the table by source index direct-mapped first (board, VXFETCH v2.5: the hash's lookups were most of ix_tri's 360-650 cycles a
 * triangle): ixd, IXD_N entries for the source indices ix_bb .. ix_bb + IXD_N - 1 (ix_bb, set after ix_new by the packet's first
 * triangle: its lowest index less IXD_N / 2, or 0; v10.473; v10.472 used the token's base vertex), each (generation << 16) | packet
 * index, valid while its generation is ixd_g (16 bits; the table cleared when it wraps); an index outside the window takes the hash as before. 32 KB: inside the C7's L1. */
#define IXD_N 8192
#define IX_NONE 0xFFFFUL
static uint32_t ixd[IXD_N],ixd_g=1,ix_bb=0xFFFFFFFFUL;
static void ix_new(const vx_d3d_input *in) {
 if(!++ix_g){memset(ix_gen,0,sizeof(ix_gen));ix_g=1;}
 if(++ixd_g>0xFFFFUL){memset(ixd,0,sizeof(ixd));ixd_g=1;}
 ix_bb=0xFFFFFFFFUL;ix_vb=in->vertices;ix_stride=in->vertex_stride;ix_fvf=in->fvf;
}
static uint32_t ix_hfind(uint32_t key) { /* the hash: the packet index or IX_NONE */
 uint32_t h=(uint32_t)(key*0x9E3779B1U)>>22; /* 10 bits: IX_H; the product in 32 bits (UL is 64 on the host) */
 for(;;h=(h+1)&(IX_H-1)){if(ix_gen[h]!=ix_g)return IX_NONE;if(ix_key[h]==key)return ix_val[h];}
}
static void ix_hput(uint32_t key,uint32_t b) {
 uint32_t h=(uint32_t)(key*0x9E3779B1U)>>22;
 for(;ix_gen[h]==ix_g;h=(h+1)&(IX_H-1));
 ix_gen[h]=ix_g;ix_key[h]=key;ix_val[h]=(uint16_t)b;
}
#define IX_FIND(i) ((i)-ix_bb<IXD_N?(ixd[(i)-ix_bb]>>16==ixd_g?ixd[(i)-ix_bb]&0xFFFFUL:IX_NONE):ix_hfind(i))
#define IX_PUT(i,b) do{if((i)-ix_bb<IXD_N)ixd[(i)-ix_bb]=(ixd_g<<16)|(b);else ix_hput((i),(b));}while(0)
/* v10.472: the open indexed packet made ready for three more vertices and indices (ix_tri's opening, shared with ix_list): another
 * packet begun when there is none, it is not this template's or not indexed, or it is full (the old one sent first); a new vertex buffer
 * starts the tables again. Returns 1 when the send failed (*rc). */
/* .515: need = the vertices about to be added, nt the triangles (.529: .515's engine-cull bits removed; an indexed packet holds
 * VX3DN_IX_VERTICES) */
uint32_t vx_d3d_ixpass,vx_d3d_px[6];
/* .529: the most vertices an indexed packet takes, 384 (VX3DN_IX_VERTICES); .533: a constant (.529-.532's VX3DN_IX_VERTICES, 192 or 384, only
 * the host tests and the replay still changed it) */
/* .523: ACCEL_D3D_V8 (default 1, the VxD's feature word bit 19; VX_D3D_Switch 13): an indexed packet of the 8-word form (VX3DN_V8_FORM: an XP
 * program, one texture unit, the plain layout) carries its vertices in 8 words (X Y Z W diffuse specular u v), the run's word marked
 * VX3DN_IX_V8; the 10-word form's last two were a copy of u v. IXW: the open indexed packet's vertex l. The writers below write the 10-word
 * form at it: a vertex is always the packet's next (vertex_count++), so the two words past an 8-word vertex are the next vertex's first
 * two, written over by it (the last vertex's two land inside pre.w, past what is sent). vx_d3d_v8_n: 8-word packets begun. */
uint32_t vx_d3d_v8=1,vx_d3d_v8_n;
#define IXW(l) (&bq.pre.w[(l)*((bq.bursts.b[0]&VX3DN_IX_V8)?8UL:10UL)])
/* .528 (ACCEL_D3D_TEXRUNS, VX_D3D_Switch 15; the VxD's answer bit 6; vx_native3d.h): an open indexed packet takes the next template's
 * triangles as a texture run when the two differ only in the unit-0 texture: every state range byte for byte the same but the texture's
 * surface, filter and addressing (range 1; enable and mode the same) and the mip chain (in range 3). GTA3 cut a packet at nearly every
 * texture change (.527: 62 texture set-up words a packet, the same as the packet before 0.5% of the time). A run that drew nothing yet
 * takes the new texture itself; the same texture as the open run adds none. vx_d3d_tr: [0] runs added, [1] template changes with other
 * state different (a new packet as before), [2] packets cut with every run used. 0 here (the OpenGL ICD keeps its packets); the HAL sets it. */
uint32_t vx_d3d_texruns,vx_d3d_tr[3];
static int tr_same_tex(const vx3dn_surface *s,uint32_t f,uint32_t au,uint32_t av,const vx3dn_mip *m,const vx3dn_packet *t) {
 return !memcmp(s,&t->tex.surface,sizeof(*s))&&f==t->tex.filter&&au==t->tex.address_u&&av==t->tex.address_v&&!memcmp(m,&t->mip,sizeof(*m));
}
/* the template fields a vertex's words depend on (fast_vertex_pre_w, gw_vertex): a run whose template converts vertices differently starts
 * a new index table, so its vertices are converted again rather than shared with the run before */
static struct {uint32_t coff,t0,t1,n0,n1,soff,tex,units,id;int prog;double zb;} tr_cv;
static void tr_key_put(void) {tr_cv.coff=tc.lay.coff;tr_cv.t0=tc.lay.t0;tr_cv.t1=tc.lay.t1;tr_cv.n0=tc.lay.n0;tr_cv.n1=tc.lay.n1;tr_cv.soff=tc.lay.soff;tr_cv.tex=tc.tex;tr_cv.units=tc.units;tr_cv.prog=tc.prog;tr_cv.zb=tc.zb;}
static int tr_key_same(void) {return tr_cv.coff==tc.lay.coff&&tr_cv.t0==tc.lay.t0&&tr_cv.t1==tc.lay.t1&&tr_cv.n0==tc.lay.n0&&tr_cv.n1==tc.lay.n1&&tr_cv.soff==tc.lay.soff&&
 tr_cv.tex==tc.tex&&tr_cv.units==tc.units&&tr_cv.prog==tc.prog&&tr_cv.zb==tc.zb;}
static int tr_merge(void) {
 static const uint32_t r[7][2]=VX3DN_STATE_RANGES;
 const uint32_t m0=PKO(mip),m1=PKO(mip)+sizeof(vx3dn_mip),at=ix_ni;uint32_t n=bq.runs.n,k;const vx3dn_packet *t=&tc.p;
 if(!at||!VX3DN_TEXRUN_FORM(&bq))return 0;
 if(memcmp(&bq,t,r[0][1])||bq.tex.enable!=t->tex.enable||bq.tex.mode!=t->tex.mode||memcmp((const char*)&bq+r[2][0],(const char*)t+r[2][0],r[2][1]-r[2][0])||
    memcmp((const char*)&bq+r[3][0],(const char*)t+r[3][0],m0-r[3][0])||memcmp((const char*)&bq+m1,(const char*)t+m1,r[3][1]-m1)){vx_d3d_tr[1]++;return 0;}
 for(k=4;k<7;k++)if(memcmp((const char*)&bq+r[k][0],(const char*)t+r[k][0],r[k][1]-r[k][0])){vx_d3d_tr[1]++;return 0;}
 {const vx3dn_texrun *c=n?&bq.runs.r[n-1]:0; /* the open run's texture: the same adds no run */
  if(c?tr_same_tex(&c->surface,c->filter,c->address_u,c->address_v,&c->mip,t):tr_same_tex(&bq.tex.surface,bq.tex.filter,bq.tex.address_u,bq.tex.address_v,&bq.mip,t))return 1;}
 if(n&&bq.runs.r[n-1].at==at)n--; /* the open run drew nothing: it takes this texture */
 else if(n>=VX3DN_TEXRUNS_MAX){vx_d3d_tr[2]++;return 0;}
 {vx3dn_texrun *w=&bq.runs.r[n];w->at=at;w->surface=t->tex.surface;w->filter=t->tex.filter;w->address_u=t->tex.address_u;w->address_v=t->tex.address_v;w->mip=t->mip;}
 bq.runs.n=n+1;vx_d3d_tr[0]++;return 1;
}
/* .528: a texture stage state command (op 25) normally sends the batch first (v10.215: a WASSTILLDRAWING retry starts at the batch's first
 * command and must find the state its draws were made with). With texture runs the batch may stay open over it, when it is an indexed
 * packet that can take another run and will go through the queue (whose failed send puts back a packet's state): the state its draws
 * were made with is kept as an epoch of the queue's (bq_ep), once a batch. The next draw then adds a run (tr_merge) or, when more than
 * the texture changed, sends the batch as before. 0: send it now. */
static int tr_defer(const vx_d3d_context *c,const vx_d3d_services *sv) {
 if(!vx_d3d_texruns||!bq_on||!VX3DN_TEXRUN_FORM(&bq)||bq.runs.n>=VX3DN_TEXRUNS_MAX||!sv->submit_n||!(vx_d3d_feat&VX_F_SUBMITN)||!(bq.target.offset||(vx_d3d_feat&VX_F_PRIMBATCH)))return 0;
 if(bq_ep==0xFFFFFFFFUL){
  const uint32_t e0=cq_epn;
  cq_keep_state(c); /* the queued packets' epoch, as at any state command */
  if(cq_epn!=e0)bq_ep=e0; /* just kept: the batch's draws were made in that same epoch */
  else{if(cq_epn>=CQ_MAX)return 0;cq_snap[cq_epn]=c->state;bq_ep=cq_epn;cq_epn++;}}
 else cq_keep_state(c);
 return 1;
}
static int ix_open(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,uint32_t need,uint32_t nt,uint32_t *completed,uint32_t *rc) {
 if(vx_d3d_texruns&&bq_on&&bq_tid!=tc.id&&bq.pre.on==2&&bq.vertex_count+need<=VX3DN_IX_VERTICES&&ix_ni+3*nt<=VX3DN_MAX_INDICES&&tr_merge()){ /* .528 */
  bq_tid=tc.id;if(!tr_key_same()){tr_key_put();ix_new(in);vx_d3d_ix[3]++;}}
 if(!(bq_on&&bq_tid==tc.id&&bq.pre.on==2&&bq.vertex_count+need<=VX3DN_IX_VERTICES&&ix_ni+3*nt<=VX3DN_MAX_INDICES)) {
  if(bq_on){vx_d3d_cut_tag=bq_tid!=tc.id?1:bq.pre.on!=2?5:bq.vertex_count+need>VX3DN_IX_VERTICES?3:6;vx_d3d_cut_hist[8+vx_d3d_cut_tag]++;if(vx_d3d_geo_on)vx_d3d_geo[16+(vx_d3d_cut_tag<5?vx_d3d_cut_tag:4)]++;if(ft_sm)vx_d3d_ft[15]++;
   {uint32_t f0=ft_sm?vx_d3d_phclock():0;*rc=vx_d3d_flush(c,sv,completed);if(ft_sm)vx_d3d_ft[19]+=(vx_d3d_phclock()-f0+8)>>4;}
   if(*rc)return 1;}
  pk_copy(&bq,&tc.p);bq.ext.prim=0;bq.pre.on=2;bq_tid=tc.id;bq_on=1;bq_cmd=cur_cmd;strip_forget();tr_key_put();bq_ep=0xFFFFFFFFUL; /* .528 */
  bq.vertex_count=0;bq.bursts.n=1;bq.bursts.b[0]=VX3DN_TOPO_INDEXED<<16;ix_ni=0;ix_new(in);vx_d3d_ix[2]++;
  if(vx_d3d_v8&&VX3DN_V8_FORM(&bq)){bq.bursts.b[0]|=VX3DN_IX_V8;vx_d3d_v8_n++;}} /* .523 */
 else if(ix_vb!=in->vertices||ix_stride!=in->vertex_stride||ix_fvf!=in->fvf){ix_new(in);vx_d3d_ix[3]++;}
 *rc=0;return 0;
}
static int ix_tri(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint32_t *idx,const uint8_t *const *v,uint32_t *completed,uint32_t *rc) {
 uint32_t j,need=3;uint16_t *ix;
 FT(3);
 if(ix_open(c,in,sv,need,1,completed,rc))return 1; /* .520: no engine cull here: every caller culled the triangle on the CPU already (the same
  * winding: the engine's cull dropped nothing more, and the mark split packets from ix_list's cull-none ones) */
 FT(5);
 ix=VX3DN_IDX(&bq)+ix_ni;
 if(ix_bb==0xFFFFFFFFUL)ix_bb=idx[0]>=IXD_N/2?idx[0]-IXD_N/2:0; /* v10.473: as ix_list's */
 for(j=0;j<3;j++){const uint32_t key=idx[j];uint32_t b=IX_FIND(key);
  if(b==IX_NONE){b=bq.vertex_count++;IX_PUT(key,b);
   if(gw_on)gw_vertex(IXW(b),v[j],key,v[j]);else fast_vertex_pre_w(IXW(b),v[j],key,v[j]); /* .523: IXW */vx_d3d_ix[1]++;} /* v10.501: by number (the ICD's grid words; Direct3D's ps_lim is 0) */
  ix[j]=(uint16_t)b;}
 ix_ni+=3;bq.bursts.b[0]=(bq.bursts.b[0]&0xFFFF0000UL)|ix_ni;
 FT(4);
 vx_d3d_batched++;vx_d3d_ix[0]++;if(vx_d3d_geo_on)vx_d3d_geo[6]++;
 FT(6);if(ft_sm)vx_d3d_ft[18]++;*rc=0;return 1;
}
static int fast_tri_place(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint32_t *idx,const uint8_t *const *v,uint32_t *completed,uint32_t *rc);
static int fast_triangle(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint32_t *idx,uint32_t *completed,uint32_t *rc) {
 const uint8_t *v[3];float x[3],y[3];uint32_t j,w;double area;const vx3dn_rect *r=&tc.p.clip;
 /* v10.236: the range checks on the IEEE encodings (non-negative floats order as their bits; NaN, infinity and every negative value,
  * -0.0 too, fall outside and take the general path, which writes the same words): x and y inside the clip, 0 <= z <= 1, rhw positive
  * and finite. They were float conversions and compares. */
 if(ft_sm){vx_d3d_ft[0]++;FT(21);} /* v10.444; v10.453: [21] the call and the function's entry (the clock's own cost is measured in the loop, outside the chain) */
 for(j=0;j<3;j++){uint32_t wx,wy;v[j]=in->vertices+idx[j]*in->vertex_stride;
  wx=u32(v[j]);wy=u32(v[j]+4);
  if(!GB_IN(wx,tc.bxl,tc.bxr)||!GB_IN(wy,tc.byt,tc.byb)||u32(v[j]+8)>0x3F800000UL||u32(v[j]+12)-1UL>=0x7F7FFFFFUL){if(vx_d3d_geo_on)vx_d3d_geo[2]++;FT(1);if(ft_sm)vx_d3d_ft[7]++;return 0;}
  x[j]=flt(wx);y[j]=flt(wy);}
 (void)r;(void)w;
 FT(1);
 area=((double)x[1]-x[0])*((double)y[2]-y[0])-((double)y[1]-y[0])*((double)x[2]-x[0]);
 if(area==0||(c->state.rs[22]==2&&area>0)||(c->state.rs[22]==3&&area<0)){if(vx_d3d_geo_on)vx_d3d_geo[3]++;FT(2);if(ft_sm)vx_d3d_ft[8]++;*rc=0;return 1;}
 FT(2);
 return fast_tri_place(c,in,sv,idx,v,completed,rc);
}
static vx_d3d_input gl_in; /* v10.239: the OpenGL ICD's vertices (vx_d3d_gl_begin) */
static uint32_t gl_noix; /* .573: set for vx_d3d_gl_prim without join and for vx_d3d_gl_tris (the flat batch's own buffer, written again a call) */
/* .503: fast_triangle's second half (a triangle that passed the checks and the cull into the packet), shared with vx_d3d_gl_trilist's run */
static int fast_tri_place(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint32_t *idx,const uint8_t *const *v,uint32_t *completed,uint32_t *rc) {
 uint32_t j,b;vx3dn_packet *d;
 if(vx_d3d_indexed&&tc.pre&&(vx_d3d_feat&VX_F_BATCH)&&!(in==&gl_in&&(gl_noix||cs_on||c->state.rs[9]==1)))return ix_tri(c,in,sv,idx,v,completed,rc); /* v10.470; .573: the ICD's flat triangles keep the plain form (a strip's vertex there has another vertex's colour: cs_on), and so do the triangles of a call whose vertices Mesa's template changes a triangle (gl_noix: the table would keep a vertex's words of an earlier triangle) */
 /* v10.236: strips (see strips_on): the next triangle of the strip at the end of the batch adds one vertex; a triangle sharing an edge
  * with the list triangle at the end of the batch turns it into a strip head and adds one vertex */
 if(strips_on(0)&&bq_on&&bq_tid==tc.id&&!bq.ext.prim&&bq.pre.on!=2&&st_vb==in->vertices&&st_stride==in->vertex_stride&&st_fvf==in->fvf&&bq.vertex_count<VX3DN_MAX_VERTICES) {
  uint32_t a=3,bb=3,x=3,k;
  if(st_on) {
   /* v10.321: the loop over the three indices written out (the same tests in the same order) */
   if(idx[0]==st_a)a=0;else if(idx[0]==st_b)bb=0;
   if(idx[1]==st_a&&a==3)a=1;else if(idx[1]==st_b&&bb==3)bb=1;
   if(idx[2]==st_a&&a==3)a=2;else if(idx[2]==st_b&&bb==3)bb=2;
   if(bb<3&&a==3&&st_alt!=0xFFFFFFFFUL) { /* the strip is its head and one vertex: the edge (st_alt, x) after swapping the head's shared two */
    for(k=0;k<3;k++)if(idx[k]==st_alt)a=k;
    if(a<3){swap_vertex(&bq,bq.vertex_count-3,bq.vertex_count-2);st_a=st_alt;}
   }
   if(a<3&&bb<3){x=3-a-bb;FT(3);fast_vertex_ix(&bq,bq.vertex_count,v[x],idx[x],v[x]);FT(4);bq.vertex_count++;bq.bursts.b[bq.bursts.n-1]++;st_a=st_b;st_b=idx[x];
    st_alt=0xFFFFFFFFUL;vx_d3d_batched++;vx_d3d_strip_vertices++;if(vx_d3d_geo_on)vx_d3d_geo[4]++;FT(6);if(ft_sm)vx_d3d_ft[9]++;*rc=0;return 1;}
  }
  else if(lt_ok) {
   uint32_t sh[3],m=0,p0=bq.vertex_count-3,lastc=bq.bursts.b[bq.bursts.n-1]&0xFFFFUL,u=3;
   for(k=0;k<3;k++)if(idx[k]==lt[0]||idx[k]==lt[1]||idx[k]==lt[2])sh[m++]=k;else x=k;
   /* .524: v10.442's ACCEL_D3D_PAIRLIST (a lone pair kept in the list run) removed: no saving in three GTA3 A/B runs */
   if(m==2&&x<3&&(lastc==3||bq.bursts.n<VX3DN_MAX_BURSTS)) {
    uint32_t s1=idx[sh[0]],s2=idx[sh[1]],want[3],pos,q;
    for(k=0;k<3;k++)if(lt[k]!=s1&&lt[k]!=s2)u=k;
    if(u<3) {
     want[0]=lt[u];want[1]=s1;want[2]=s2; /* the head (u, s1, s2): the new triangle is (s1, s2, x) */
     for(pos=0;pos<3;pos++){for(q=pos;q<3;q++)if(lt[q]==want[pos])break;
      if(q!=pos){uint32_t t=lt[pos];lt[pos]=lt[q];lt[q]=t;swap_vertex(&bq,p0+pos,p0+q);}}
     if(lastc==3)bq.bursts.b[bq.bursts.n-1]=(VX3DN_TOPO_STRIP<<16)|3;
     else {bq.bursts.b[bq.bursts.n-1]-=3;bq.bursts.b[bq.bursts.n++]=(VX3DN_TOPO_STRIP<<16)|3;}
     FT(3);fast_vertex_ix(&bq,bq.vertex_count,v[x],idx[x],v[x]);FT(4);bq.vertex_count++;bq.bursts.b[bq.bursts.n-1]++;
     st_on=1;lt_ok=0;st_a=s2;st_b=idx[x];st_alt=s1;vx_d3d_batched++;vx_d3d_strip_vertices++;if(vx_d3d_geo_on)vx_d3d_geo[5]++;FT(6);if(ft_sm)vx_d3d_ft[9]++;*rc=0;return 1;
    }
   }
  }
 }
 FT(3);
 *rc=open_slot(c,sv,3,0,completed,&d);if(*rc)return 1;
 FT(5);
 if(vx_d3d_geo_on)vx_d3d_geo[6]++; /* v10.440 */
 b=d->vertex_count;
 for(j=0;j<3;j++)fast_vertex_ix(d,b+j,v[j],idx[j],v[j]); /* v10.500: by number (the ICD's grid words, ps_lim; 0 for Direct3D) */
 FT(4);
 d->vertex_count=b+3;
 if(d==&bq&&strips_on(0)){st_on=0;lt_ok=1;lt[0]=idx[0];lt[1]=idx[1];lt[2]=idx[2];st_vb=in->vertices;st_stride=in->vertex_stride;st_fvf=in->fvf;}
 *rc=close_slot(c,sv,d,completed);FT(6);if(ft_sm)vx_d3d_ft[18]++;return 1;
}
/* (gl_in: defined before fast_tri_place since .573) v10.239: the OpenGL ICD's vertices (vx_d3d_gl_begin) */
/* v10.317: points and lines the same way (ACCEL_D3D_FASTPL, VX_F_FASTPL): every vertex inside the viewport and the depth range with rhw
 * > 0 (fast_triangle's checks), no ZBIAS, no flat shading. A line needs no clipping (segment leaves it as it is): its two vertices straight
 * from the vertex buffer. A Direct3D point (not the ICD's: OpenGL's point rule moves it) whose one-pixel square is inside the viewport
 * too (finish's clamp leaves it as it is): the vertex's words six times, x and y those of point_square's corners, (float)(x +- 0.5) as
 * output() makes them. The same words as the general path; 0 when it takes the primitive. */
static void copy_slot(vx3dn_packet *d,uint32_t to,uint32_t from) {
 if(d->pre.on){uint32_t k;for(k=0;k<10;k++)d->pre.w[to*10+k]=d->pre.w[from*10+k];return;} /* v10.441 */
 d->vertices[to]=d->vertices[from];d->tex.uv[to][0]=d->tex.uv[from][0];d->tex.uv[to][1]=d->tex.uv[from][1];
 d->stages.uv1[to][0]=d->stages.uv1[from][0];d->stages.uv1[to][1]=d->stages.uv1[from][1];d->shader.spec[to]=d->shader.spec[from];d->stages.q[to]=d->stages.q[from];d->stages.r1[to]=d->stages.r1[from];
}
static int fast_pl(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint32_t *idx,uint32_t nv,uint32_t *completed,uint32_t *rc) {
 static const uint32_t q[6]={0,1,2,0,2,3};static const double dx[4]={-.5,.5,.5,-.5},dy[4]={-.5,-.5,.5,.5};
 const uint8_t *v[2]={0,0};uint32_t j,b;vx3dn_packet *d;
 if(!(vx_d3d_feat&VX_F_FASTPL)||c->state.rs[47]||FLATSW(c)||(nv==1&&in==&gl_in))return 0;
 for(j=0;j<nv;j++){uint32_t wx,wy;v[j]=in->vertices+idx[j]*in->vertex_stride;wx=u32(v[j]);wy=u32(v[j]+4);
  if(!GB_IN(wx,tc.bxl,tc.bxr)||!GB_IN(wy,tc.byt,tc.byb)||u32(v[j]+8)>0x3F800000UL||u32(v[j]+12)-1UL>=0x7F7FFFFFUL)return 0;}
 if(nv==2) {
  *rc=open_slot(c,sv,2,1,completed,&d);if(*rc)return 1;strip_forget();
  b=d->vertex_count;fast_vertex(d,b,v[0]);fast_vertex(d,b+1,v[1]);d->vertex_count=b+2;
  *rc=close_slot(c,sv,d,completed);return 1;
 }
 {double x=flt(u32(v[0])),y=flt(u32(v[0]+4));const vx3dn_rect *r=&tc.p.clip;
  if(!(x-.5>=r->left&&x+.5<=r->right&&y-.5>=r->top&&y+.5<=r->bottom))return 0;
  *rc=open_slot(c,sv,6,0,completed,&d);if(*rc)return 1;strip_forget();
  b=d->vertex_count;fast_vertex(d,b,v[0]);
  for(j=5;j<6;j--){uint32_t px=bits((float)(x+dx[q[j]])),py=bits((float)(y+dy[q[j]]));if(j)copy_slot(d,b+j,b);
   if(d->pre.on){d->pre.w[(b+j)*10]=px;d->pre.w[(b+j)*10+1]=py;}else{d->vertices[b+j].x=px;d->vertices[b+j].y=py;}} /* v10.441 */
  d->vertex_count=b+6;*rc=close_slot(c,sv,d,completed);return 1;}
}
/* v10.201: one primitive: nv = 3 a triangle (culled, clipped as a polygon, drawn as a fan), 2 a line (clipped as a segment), 1 a point
 * (dropped outside the viewport) */
static double dfloor(double v) {double t=(double)(int32_t)v;return t>v?t-1:t;} /* |v| < 2^31 */
static int prim_tried; /* v10.238: set by the triangle-token loop around a primitive() call for a triangle fast_triangle declined */
/* v10.296: the end of every primitive: the vertices checked and kept inside the clip (see the comments in primitive), then emitted */
static uint32_t finish(vx_d3d_context *c,const vx_d3d_services *sv,cv *vs,uint32_t n,const uint32_t *ord,uint32_t no,uint32_t prim,uint32_t *completed) {
 uint32_t j;
 for(j=0;j<n;j++) {
  cv *v=&vs[j];
  /* v10.219: a vertex the clip left non-finite (NaN from inf - inf on huge coordinates) or with rhw <= 0 cannot be drawn: the primitive is
   * left out instead of sent to the VxD, whose refusal of a packet after an earlier one was drawn loses the device */
  /* v10.221 (review): also an rhw that becomes 0 as a float (the VxD refuses rhw 0). The lower bounds are taken unless the value is
   * above them, so -0.0 becomes +0.0: the VxD compares IEEE encodings, where -0.0 (80000000) is above every limit and was refused. */
  if(!finite(bits((float)v->x))||!finite(bits((float)v->y))||!finite(bits((float)v->z))||!finite(bits((float)v->rhw))||v->rhw<=0||!(bits((float)v->rhw)&0x7FFFFFFFUL)){drop(c);return 0;}
  if(!(v->x>tc.p.clip.left))v->x=tc.p.clip.left;if(v->x>tc.p.clip.right)v->x=tc.p.clip.right;
  if(!(v->y>tc.p.clip.top))v->y=tc.p.clip.top;if(v->y>tc.p.clip.bottom)v->y=tc.p.clip.bottom;
  if(!(v->z>0))v->z=0;if(v->z>1)v->z=1;
 }
 return emit(c,sv,vs,ord,no,prim,completed);
}
/* a line a to b, clipped as a segment */
static uint32_t segment(vx_d3d_context *c,const vx_d3d_services *sv,const cv *a,const cv *b,uint32_t *completed) {
 static cv v[2];static const uint32_t ord[2]={0,1};uint32_t plane;
 v[0]=*a;v[1]=*b;
 for(plane=0;plane<6;plane++) {
  double da=distance(&v[0],&c->state.viewport,plane),db=distance(&v[1],&c->state.viewport,plane);
  if((da<0&&db<=0)||(da<=0&&db<0))return 0; /* v10.302 (review): one end outside and the other on the plane is a point at most */
  if(da<0)v[0]=interpolate(&v[0],&v[1],da/(da-db));
  else if(db<0)v[1]=interpolate(&v[0],&v[1],da/(da-db));
 }
 return finish(c,sv,v,2,ord,2,1,completed);
}
/* v10.203: a point is a one-pixel square of two triangles (the chip's point mode drew nothing in four register sets, board v10.202) */
static uint32_t point_square(vx_d3d_context *c,const vx_d3d_services *sv,const cv *a,int gl,uint32_t *completed) {
 static cv v[4];static const uint32_t q[6]={0,1,2,0,2,3};static const double dx[4]={-.5,.5,.5,-.5},dy[4]={-.5,-.5,.5,.5};uint32_t plane,j;
 for(plane=0;plane<6;plane++)if(distance(a,&c->state.viewport,plane)<0)return 0;
 v[0]=*a;
 /* v10.239: OpenGL's point rule: a one-pixel point lights pixel (floor x, floor y) of the window. The ICD's coordinates are the
  * window's less a half pixel, y flipped (vxgl_calc_viewport), so a point on a pixel edge (whole window coordinates, as an ortho 2D
  * scene puts them) is a tie between two pixels, which Direct3D's top-left rule gives to the left one and OpenGL to the right. The
  * square is put on the pixel OpenGL chooses, its edges on half pixels, whatever the engine's sub-pixel precision. */
 if(gl&&v[0].x>-65536.0&&v[0].x<65536.0&&v[0].y>-65536.0&&v[0].y<65536.0) /* not NaN: the check in finish drops it */
  {v[0].x=dfloor(v[0].x+0.5);v[0].y=-dfloor(-0.5-v[0].y)-1;}
 for(j=3;j<4;j--){v[j]=v[0];v[j].x+=dx[j];v[j].y+=dy[j];}
 return finish(c,sv,v,4,q,6,0,completed);
}
/* v10.296: D3DTRIFLAG_EDGEENABLE1..3 of the triangle in hand (bit 0 edge a-b, 1 b-c, 2 c-a), for FILLMODE wireframe; the DP2 loop sets
 * them for INDEXEDTRIANGLELIST and TRIANGLEFAN_IMM (the runtime's clipped polygons: only their outline), every edge otherwise */
static uint32_t prim_edges=7;
static uint32_t primitive_body(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint32_t *idx,uint32_t nv,uint32_t *completed);
/* v10.475: the template made ready for this context and vertex format (primitive()'s first step, also ix_list's caller's: v10.474 sent
 * each draw's first triangle through primitive() to build it, about 1 in 31 of a game-like scene's triangles): built when it is not this
 * context's; its vertex layout set for this FVF. The build's failure is returned; tc.err and tc.skip are the caller's to read. */
static uint32_t tc_ready(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv) {
 if(!tc.valid||tc.ctx!=c){uint32_t rc=template_build(c,sv,0);if(rc)return rc;}
 if(!tc.err&&!tc.skip&&tc.lay.fvf!=in->fvf){uint32_t f=in->fvf;tc.lay.fvf=f;tc.lay.coff=(f&0x40)?16:0;tc.lay.soff=(f&0x80)?((f&0x40)?20:16):0;
  tc.lay.n0=tc.lay.n1=0;tc.lay.t0=texcoord_offset(f,tc.set,&tc.lay.n0);tc.lay.t1=texcoord_offset(f,tc.set1,&tc.lay.n1);}
 return 0;
}
static uint32_t primitive(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint32_t *idx,uint32_t nv,uint32_t *completed) {
 uint32_t o=VX_PH(VX_PH_VERT),rc=primitive_body(c,in,sv,idx,nv,completed);VX_PH_BACK(o);return rc;
}
static uint32_t primitive_body(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint32_t *idx,uint32_t nv,uint32_t *completed) {
 /* v10.213: the clip buffers are static: on the stack they made a large frame, which steps over Windows 98's one-page stack guard (the
  * DLL is built without stack probes). Every caller holds the HAL's callback_lock. */
 static cv verts[2][12];
 uint32_t n=nv,buf=0,plane,j,k,m,ord[36],no=0;double area;
 {uint32_t rc=tc_ready(c,in,sv);if(rc)return rc;}
 if(tc.err)return tc.err;
 if(tc.skip){drop(c);vx_d3d_drop_why|=tc.skip;return 0;}
 if(vx_d3d_geo_on&&nv==3){if(!geo_skip)geo_tri(in,idx);if(!prim_tried&&(FLATSW(c)||c->state.rs[8]!=3))vx_d3d_geo[10]++;} /* v10.440 */
 if(nv==3&&!prim_tried&&!FLATSW(c)&&c->state.rs[8]==3){uint32_t rc;VX_PH(VX_PH_ASM);if(fast_triangle(c,in,sv,idx,completed,&rc))return rc;VX_PH(VX_PH_VERT);}
 if(nv<3){uint32_t rc;VX_PH(VX_PH_ASM);if(fast_pl(c,in,sv,idx,nv,completed,&rc))return rc;VX_PH(VX_PH_VERT);} /* v10.317 */
 for(j=0;j<nv;j++)vertex(&verts[0][j],in->vertices+idx[j]*in->vertex_stride,in->fvf,tc.set,tc.set1);
 /* v10.201: ZBIAS (47) pulls the primitive toward the viewer by 2^-12 of the depth range a step (an approximation: XP's 01/16 encoding
  * has one sample, CLXP3D T53) */
 if(c->state.rs[47]){double zb=zbias(c);for(j=0;j<nv;j++){verts[0][j].z-=zb;if(verts[0][j].z<0)verts[0][j].z=0;}}
 if(FLATSW(c))for(j=1;j<nv;j++)for(k=0;k<4;k++){verts[0][j].c[k]=verts[0][0].c[k];verts[0][j].s[k]=verts[0][0].s[k];}
 if(nv<3)VX_PH(VX_PH_ASM);
 if(nv==1)return point_square(c,sv,&verts[0][0],in==&gl_in,completed);
 if(nv==2)return segment(c,sv,&verts[0][0],&verts[0][1],completed);
 area=(verts[0][1].x-verts[0][0].x)*(verts[0][2].y-verts[0][0].y)-(verts[0][1].y-verts[0][0].y)*(verts[0][2].x-verts[0][0].x);
 /* v10.296 (Tunnel's Ctrl+W: FILLMODE 2 was refused and every draw left out, the picture stood still): FILLMODE point and wireframe
  * draw the triangle's vertices as points and its enabled edges as lines, after the cull (as Direct3D: a culled face draws nothing) */
 if(c->state.rs[8]!=3) {
  uint32_t e=prim_edges,rc=0;VX_PH(VX_PH_ASM);
  if((c->state.rs[22]==2&&area>0)||(c->state.rs[22]==3&&area<0))return 0;
  if(c->state.rs[8]==1){if(!(vx_d3d_feat&VX_F_POINTS)){drop(c);return 0;}for(j=0;j<3&&!rc;j++)rc=point_square(c,sv,&verts[0][j],0,completed);return rc;}
  for(j=0;j<3&&!rc;j++)if(e&(1UL<<j))rc=segment(c,sv,&verts[0][j],&verts[0][(j+1)%3],completed);
  return rc;
 }
 if(vx_d3d_geo_on)vx_d3d_geo[7]++; /* v10.440 */
 if(area==0||(c->state.rs[22]==2&&area>0)||(c->state.rs[22]==3&&area<0)){if(vx_d3d_geo_on)vx_d3d_geo[9]++;return 0;}
 /* v10.221: a triangle inside every plane goes through unchanged (the clip would copy it as it is) */
 {uint32_t in6=1;for(plane=0;plane<6&&in6;plane++)for(j=0;j<3;j++)if(distance(&verts[0][j],&c->state.viewport,plane)<0){in6=0;break;}
  if(in6)plane=6;else{plane=0;if(vx_d3d_geo_on)vx_d3d_geo[8]++;}}
 /* v10.352 (board, VXGL_SAMPLE: the clip a sixth of Quake 2's frames; every polygon Mesa cut at the window's left or top edge has a vertex
  * at -0.5 here): each vertex's distance once a plane (was twice, an edge's two ends), a plane every vertex is inside of left out (it copied
  * the polygon unchanged: the same output), and the next vertex without a division (% n) */
 for(;plane<6&&n;plane++) {
  double dist[12];uint32_t out=0;
  for(j=0;j<n;j++){dist[j]=distance(&verts[buf][j],&c->state.viewport,plane);if(dist[j]<0)out=1;}
  if(!out)continue;
  m=0;
  for(j=0;j<n;j++) {
   uint32_t k=j+1==n?0:j+1;cv *aa=&verts[buf][j],*bb=&verts[buf][k];
   double da=dist[j],db=dist[k];
   if(da>=0) {if(m>=12)return VX_D3D_INVALID;verts[buf^1][m++]=*aa;}
   /* v10.302 (review): a crossing only when the ends are strictly on both sides: a vertex on the plane with its neighbour outside was kept
    * and then interpolated at t = 0 or 1 again (a duplicate vertex, a sliver triangle; the runtime's clipped polygons do it often) */
   if((da>0&&db<0)||(da<0&&db>0)) {if(m>=12)return VX_D3D_INVALID;verts[buf^1][m++]=interpolate(aa,bb,da/(da-db));}
  }
  n=m;buf^=1;
 }
 if(n<3)return 0;
 for(j=1;j+1<n;j++){ord[no++]=0;ord[no++]=j;ord[no++]=j+1;}
 VX_PH(VX_PH_ASM);
 return finish(c,sv,verts[buf],n,ord,no,0,completed);
}
static uint32_t triangle(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,uint32_t a,uint32_t b,uint32_t d,uint32_t *completed) {
 uint32_t i[3];i[0]=a;i[1]=b;i[2]=d;return primitive(c,in,sv,i,3,completed);
}
static uint32_t vs_fvf=0xFFFFFFFFUL,vs_size; /* v10.236: the last FVF's vertex size (it was worked out for every vertex) */
/* v10.367: VX_VERTEX_BAD = a vertex in range whose x y z rhw are not all finite or whose rhw <= 0: the primitives using it are left out
 * (counted, drop reason 2000) and the rest of the call is drawn. Board, GTA3: one such vertex in a LINELIST or TRIANGLEFAN failed the whole
 * DrawPrimitives2 call with DDERR_INVALIDPARAMS (VXD3D.LOG cb=2 hr=80070057), its later commands not drawn. An index past the vertices
 * or a buffer too short stays VX_D3D_INVALID (the call is malformed). The engine is still never sent such a vertex (a NaN loses the device). */
#define VX_VERTEX_BAD 1UL
#define VX_VERTEX_RHW0 2UL /* .531: rhw +0 or -0, the rest finite (bad_prim) */
uint32_t vx_d3d_bad_vertex_prims,vx_d3d_bad_why[4];
static void drop_bad(const vx_d3d_context *c) {drop(c);vx_d3d_drop_why|=0x2000UL;vx_d3d_bad_vertex_prims++;}
/* .531 (board, GTA3 on .530: 60,503 vertices with rhw 0, 20,167 primitives left out, all untextured, diffuse colour only): a vertex whose
 * rhw is +0 or -0 (VX_VERTEX_RHW0) is drawn with rhw 1 when every vertex of its primitive has rhw 0 and the state reads rhw nowhere: no
 * table fog (fog from w), no w-buffer (ZENABLE 2); .545: a texture allowed (the weights equal, see rhw0_ok). With the same rhw on
 * every vertex, the perspective weights are equal, so any rhw gives the same picture; 1 is what the engine accepts (the VxD refuses 0). A
 * primitive mixing rhw 0 with other values, or in a state that reads rhw, is still left out (drop reason 2000). rhw0_fix is set by
 * bad_prim for the primitive it passes and read by vertex(); bad_prim clears it before every check, and DrawPrimitives2 and the ICD's
 * begin clear it (a primitive that reaches vertex() without bad_prim has no rhw-0 vertex). vx_d3d_rhw0_prims counts them (QueryPhases 242). */
uint32_t vx_d3d_rhw0_prims;
/* .534: ACCEL_D3D_TNL (feature under test, default 0; delete once the games have been probed). The HAL offers hardware transform and
 * lighting (vx_d3d_hal.c) only so that a run can show which games choose it: Direct3D 7's T&L HAL device, or Direct3D 8's hardware or mixed
 * vertex processing (on this DirectX 7 DDI the runtime then sends the T&L commands and untransformed vertices). Nothing is transformed: the
 * T&L commands (33 SETMATERIAL, 34 SETLIGHT, 35 CREATELIGHT, 36 SETTRANSFORM, 44 SETCLIPPLANE) are read past and counted, and in a call whose
 * vertices are untransformed (FVF position XYZ or XYZB*, not XYZRHW) every draw command is left out and counted; state commands still apply.
 * vx_d3d_tlp (QueryPhases 271-289): [0] calls with untransformed vertices, [1] the draw commands left out in them, [2]-[6] the entries of
 * commands 33, 34, 35, 36, 44, [7] SETLIGHT entries carrying light data, [8]-[15] four FVFs of those calls with their call counts, [16] calls
 * whose other FVFs did not fit, [17] the primitives the left-out draws held. */
uint32_t vx_d3d_tnl,vx_d3d_tlp[18];
static void tlp_fvf(uint32_t fvf) {uint32_t k;for(k=0;k<4;k++){if(vx_d3d_tlp[9+k*2]&&vx_d3d_tlp[8+k*2]==fvf){vx_d3d_tlp[9+k*2]++;return;}if(!vx_d3d_tlp[9+k*2]){vx_d3d_tlp[8+k*2]=fvf;vx_d3d_tlp[9+k*2]=1;return;}}vx_d3d_tlp[16]++;}
static int tlp_draw(uint32_t op) {return op==1||op==2||op==3||(op>=15&&op<=24)||op==26||op==27;}
static int rhw0_ok(const vx_d3d_context *c) {
 uint32_t s;
 if(c->state.rs[7]==2)return 0;
 if(c->state.rs[28]&&c->state.rs[35])return 0;
 (void)s; /* .545: a texture no longer stops it: with one rhw on every vertex the perspective weights are equal, so the coordinates
  * interpolate the same for any common rhw (affine), and 1 gives the picture 0 means (GTA3 on .544: 31,244 rhw-0 vertices, its textured
  * coronas and streaks left out as drop reason 2000 with T&L off) */
 return 1;
}
/* r: the n vertices' valid_vertex (or vv) answers, none VX_D3D_INVALID; 1: the primitive is left out (counted), 0: draw it */
static int bad_prim(vx_d3d_context *c,uint32_t r0,uint32_t r1,uint32_t r2,uint32_t n) {
 uint32_t any=r0|(n>1?r1:0)|(n>2?r2:0),all=r0==VX_VERTEX_RHW0&&(n<2||r1==VX_VERTEX_RHW0)&&(n<3||r2==VX_VERTEX_RHW0);
 rhw0_fix=0;
 if(!any)return 0;
 if(all&&rhw0_ok(c)){rhw0_fix=1;vx_d3d_rhw0_prims++;return 0;}
 drop_bad(c);return 1;
}
static uint32_t valid_vertex(const vx_d3d_input *in,uint32_t idx) {
 uint32_t v,size;const uint8_t *p;
 if(in->fvf!=vs_fvf){vs_size=vx_d3d_vertex_size(in->fvf);vs_fvf=in->fvf;}
 size=vs_size;
 if(!size||!in->vertices||in->vertex_stride<size||idx>=in->vertex_count||
    idx>0xFFFFFFFFUL/in->vertex_stride||idx*in->vertex_stride>in->vertex_bytes||
    size>in->vertex_bytes-idx*in->vertex_stride) return VX_D3D_INVALID;
 p=in->vertices+idx*in->vertex_stride;
 for(v=0;v<3;v++)if(!finite(u32(p+v*4))){vx_d3d_bad_why[0]++;return VX_VERTEX_BAD;}
 if(!finite(u32(p+12))){vx_d3d_bad_why[1]++;return VX_VERTEX_BAD;}
 if(!(u32(p+12)&0x7FFFFFFFUL)){vx_d3d_bad_why[2]++;return VX_VERTEX_RHW0;} /* .531: +0 or -0, drawable in some states (bad_prim) */
 if(flt(u32(p+12))<=0){vx_d3d_bad_why[3]++;return VX_VERTEX_BAD;}
 return 0;
}
/* DX5 DrawOne* callbacks share the exact DP2 clip/cull/native path. Validate
 * every referenced vertex before emitting a primitive. Type: list/strip/fan.
 * The caller owns the checked index-buffer byte range. */
uint32_t vx_d3d_draw_primitive(vx_d3d_context *c,const vx_d3d_input *in,
 const vx_d3d_services *sv,uint32_t type,const uint16_t *ix,uint32_t count) {
 uint32_t n,i,j,a[3],rc,completed=0;
 if(!c||!in||!sv||!sv->submit||count>65535)return VX_D3D_INVALID;
 if(c->lost)return VX_D3D_SURFACELOST;
 if(c->recording)return VX_D3D_UNSUPPORTED;
 if(type!=4&&type!=5&&type!=6)return VX_D3D_UNSUPPORTED;
 if(!vx_d3d_supported(c)){drop(c);return 0;}
 if(!count)return 0;
 if(count<3||(type==4&&count%3))return VX_D3D_INVALID;
 n=type==4?count/3:count-2;bq_on=0;fail_set=0;if(cq_held){uint32_t z=0;if(sv->submit_n)(void)cq_send(sv,hold_c,&z);}cq_clear();template_reset(); /* .550: held packets (none in the ICD) sent first */
 for(i=0;i<count;i++) {
  j=ix?u16((const uint8_t*)ix+i*2):i;
  if(valid_vertex(in,j)==VX_D3D_INVALID)return VX_D3D_INVALID; /* v10.367: a bad value leaves only its triangles out (below) */
 }
 for(i=0;i<n;i++) {
  if(type==4){a[0]=i*3;a[1]=a[0]+1;a[2]=a[0]+2;}
  else if(type==5){a[0]=i;a[1]=i+1;a[2]=i+2;if(i&1){j=a[1];a[1]=a[2];a[2]=j;}}
  else {a[0]=i+1;a[1]=i+2;a[2]=0;}
  if(ix)for(j=0;j<3;j++)a[j]=u16((const uint8_t*)ix+a[j]*2);
  if(bad_prim(c,valid_vertex(in,a[0]),valid_vertex(in,a[1]),valid_vertex(in,a[2]),3))continue; /* v10.367; .531: bad_prim */
  rc=triangle(c,in,sv,a[0],a[1],a[2],&completed);
  /* v10.330 (review #167): the triangles before it are still sent; a send that lost the device is what the call returns (it was dropped
   * behind the triangle's error), otherwise the triangle's error as before */
  if(rc){uint32_t f=vx_d3d_flush(c,sv,&completed);if(!f)f=cq_flush(sv,c,&completed);cq_clear();return f&&c->lost?f:rc;}
 }
 rc=vx_d3d_flush(c,sv,&completed); /* v10.214 */
 if(!rc)rc=cq_flush(sv,c,&completed);cq_clear();return rc; /* v10.276 */
}
/* v10.239: the OpenGL ICD (VXGL.DLL, gl/vxgl_*.c). Mesa's vertex store (t_vertex: the FVF layout of a transformed vertex) feeds
 * primitive() a primitive at a time; the batch stays open across calls until vx_d3d_gl_flush (the ICD calls it before a state change,
 * a clear, a swap and every CPU access to the buffers). vx_d3d_gl_begin names the store for the primitives that follow; it is
 * rewritten between pipeline runs, so a strip joins only within one begin, and only for primitives sent with join set (the ICD
 * clears it when the vertices are changed per triangle: two-sided lighting, polygon offset, unfilled polygons). */
void vx_d3d_gl_begin(const uint8_t *vb,uint32_t stride,uint32_t fvf) {
 rhw0_fix=0; /* .531 */
 memset(&gl_in,0,sizeof(gl_in));gl_in.vertices=vb;gl_in.vertex_stride=stride;gl_in.fvf=fvf;gl_in.vertex_count=0xFFFFUL;gl_in.vertex_bytes=0xFFFFUL*stride;
 strip_forget();gw_new(); /* .503 */
 ix_vb=0; /* v10.501: the store is written again for every draw: an open indexed packet's table by store number starts again (ix_open) */
}
/* v10.330 (review #150): the store's real vertex count (after vx_d3d_gl_begin; the ICD knows it, the core assumed 65535): every index a
 * primitive or strip names is checked against it before a vertex is read. Not called: 65535 as before */
void vx_d3d_gl_count(uint32_t n) {if(n<=0xFFFFUL){gl_in.vertex_count=n;gl_in.vertex_bytes=n*gl_in.vertex_stride;}}
uint32_t vx_d3d_gl_prim(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *idx,uint32_t nv,int join) {
 uint32_t completed=0,rc;
 if(c->lost)return VX_D3D_SURFACELOST;
 if(!nv||nv>3)return VX_D3D_INVALID; /* v10.330 (review #84): a point, a line or a triangle; primitive() read verts[0][1..2] for nv 0 */
 for(rc=0;rc<nv;rc++)if(idx[rc]>=gl_in.vertex_count)return VX_D3D_INVALID; /* v10.330 (review #150) */
 if(!join)strip_forget();
 gw_new(); /* .503: the template's per-triangle path may change the store's vertices (two-sided colours) */
 tri_snap=nv==3?vx_d3d_gl_snap:0;
 ps_lim=nv==3&&ps_xy&&tri_snap&&ps_mode==tri_snap&&ps_base==gl_in.vertices?(ps_n<gl_in.vertex_count?ps_n:gl_in.vertex_count):0; /* v10.500: the ICD's grid words (STORESNAP), as vx_d3d_gl_strip */
 gl_noix=!join;rc=primitive(c,&gl_in,sv,idx,nv,&completed);gl_noix=0;tri_snap=0;ps_lim=0;
 if(!join)strip_forget();
 return rc;
}
/* v10.323: a strip from the OpenGL ICD (vx_d3d_gl_strip): its vertices s[0..n) in strip order, triangle i on s[i], s[i+1], s[i+2] (GL's
 * winding: s[i], s[i+1] swapped when i is odd, or even with flip; quad strips come pair-swapped with flip, the same triangles and diagonal as
 * Mesa's quads). A triangle the fast path takes (the template's fast conditions, every vertex inside the viewport and depth range with rhw
 * > 0, not culled, not zero area) adds one vertex to the strip run at the end of the batch, or starts one with three; anything else goes
 * through primitive() as before. The per-triangle path found the strip again for every triangle (about 450 instructions a triangle on the
 * host) and a quad strip's quads did not continue the run: the lit sphere sent 192 packets a frame of 64 triangles, 3 vertices each. */
static int fast_v(const uint8_t *p) {
 uint32_t wx=u32(p),wy=u32(p+4);
 return !(!GB_IN(wx,tc.bxl,tc.bxr)||!GB_IN(wy,tc.byt,tc.byb)||u32(p+8)>0x3F800000UL||u32(p+12)-1UL>=0x7F7FFFFFUL);
}
/* culling on (as fast_triangle): the area in GL's winding of tri; 1 when the triangle is culled or has no area (v10.490: a function, the
 * culling run below uses the same) */
static int strip_culled(const vx_d3d_context *c,const uint32_t *tri) {
 const uint8_t *a=gl_in.vertices+tri[0]*gl_in.vertex_stride,*bb=gl_in.vertices+tri[1]*gl_in.vertex_stride,*cc=gl_in.vertices+tri[2]*gl_in.vertex_stride;
 float x0=flt(u32(a)),y0=flt(u32(a+4)),x1=flt(u32(bb)),y1=flt(u32(bb+4)),x2=flt(u32(cc)),y2=flt(u32(cc+4));
 double area=((double)x1-x0)*((double)y2-y0)-((double)y1-y0)*((double)x2-x0);
 return area==0||(c->state.rs[22]==2&&area>0)||(c->state.rs[22]==3&&area<0);
}
static int strip_fast(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *w,const uint32_t *tri,uint32_t *completed,uint32_t *rc) {
 const uint8_t *v[3];uint32_t k,b,pb=cs_on?(uint32_t)(w-cs_s):0;vx3dn_packet *d;int cont;
 if(!(tc.valid&&tc.ctx==c&&!tc.err&&!tc.skip&&tc.lay.fvf==gl_in.fvf&&!FLATSW(c)&&c->state.rs[8]==3&&strips_on(0)))return 0;
 cont=st_on&&bq_on&&bq_tid==tc.id&&!bq.ext.prim&&st_vb==gl_in.vertices&&st_stride==gl_in.vertex_stride&&st_fvf==gl_in.fvf&&st_a==w[0]&&st_b==w[1]&&
  bq.vertex_count<VX3DN_MAX_VERTICES&&bq.bursts.n&&(bq.bursts.b[bq.bursts.n-1]>>16)==VX3DN_TOPO_STRIP;
 /* a continued run's first two vertices were checked when they went in: only the new one is */
 for(k=cont?2:0;k<3;k++)if(!fast_v(gl_in.vertices+w[k]*gl_in.vertex_stride))return 0;
 for(k=0;k<3;k++)v[k]=gl_in.vertices+w[k]*gl_in.vertex_stride;
 /* culling on (as fast_triangle): the area in GL's winding; a culled or zero-area triangle is not drawn and ends the run. Culling off: no
  * area (a zero-area triangle in a strip covers no pixel on the engine either) */
 if(c->state.rs[22]!=1&&strip_culled(c,tri)){strip_forget();*rc=0;return 1;} /* not drawn: the run ends here */
 /* .573: with indexed packets on, a strip triangle the pass above did not take (its primitive has a vertex outside) by index too: a
  * strip burst is the plain form, and the two forms cannot share a packet. Not with a flat strip's colours by position (cs_on). */
 if(vx_d3d_indexed&&tc.pre&&(vx_d3d_feat&VX_F_BATCH)&&!cs_on&&c->state.rs[9]!=1){strip_forget();return ix_tri(c,&gl_in,sv,w,v,completed,rc);}
 if(cont){fast_vertex_ix(&bq,bq.vertex_count,v[2],w[2],cs_pc(pb+2,v[2]));bq.vertex_count++;bq.bursts.b[bq.bursts.n-1]++;st_a=w[1];st_b=w[2];st_alt=0xFFFFFFFFUL;
  vx_d3d_batched++;vx_d3d_strip_vertices++;*rc=0;return 1;}
 *rc=open_slot(c,sv,3,0,completed,&d);if(*rc)return 1;
 b=d->vertex_count;for(k=0;k<3;k++)fast_vertex_ix(d,b+k,v[k],w[k],cs_pc(pb+k,v[k]));d->vertex_count=b+3;
 strip_forget();
 if(d==&bq&&strips_on(0)) {
  uint32_t *l=&bq.bursts.b[bq.bursts.n-1];int made=0;
  if((*l&0xFFFFUL)==3){*l=(VX3DN_TOPO_STRIP<<16)|3;made=1;}
  else if(bq.bursts.n<VX3DN_MAX_BURSTS){*l-=3;bq.bursts.b[bq.bursts.n++]=(VX3DN_TOPO_STRIP<<16)|3;made=1;}
  st_vb=gl_in.vertices;st_stride=gl_in.vertex_stride;st_fvf=gl_in.fvf;
  if(made){st_on=1;st_a=w[1];st_b=w[2];st_alt=0xFFFFFFFFUL;}else{lt_ok=1;lt[0]=w[0];lt[1]=w[1];lt[2]=w[2];}
 }
 *rc=close_slot(c,sv,d,completed);return 1;
}
/* v10.481: n independent triangles, the store's vertices idx[3i..3i+2] (or 3i, 3i+1, 3i+2 with idx NULL) (the ICD's flat strips with
 * their own colours, vxgl_tris.c flat_batch): each as vx_d3d_gl_prim with join 0 (no strip joins them), in one call */
uint32_t vx_d3d_gl_tris(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *idx,uint32_t n) {
 uint32_t completed=0,rc=0,i,t[3];
 if(c->lost)return VX_D3D_SURFACELOST;
 if(!idx&&n>gl_in.vertex_count/3)return VX_D3D_INVALID;
 if(idx&&n>0xFFFFFFFFUL/3)return VX_D3D_INVALID; /* .532 (review of .530, H1): 3*n must not wrap */
 if(idx)for(i=0;i<3*n;i++)if(idx[i]>=gl_in.vertex_count)return VX_D3D_INVALID; /* every index before any vertex is read */
 tri_snap=vx_d3d_gl_snap;gw_new(); /* .503 */
 ps_lim=ps_xy&&tri_snap&&ps_mode==tri_snap&&ps_base==gl_in.vertices?(ps_n<gl_in.vertex_count?ps_n:gl_in.vertex_count):0; /* v10.501: the ICD's grid words (STORESNAP) here too (board, VXFLAT v2.8: the flat triangles, quads and fans 2-5% slower with them made and not used) */
 for(i=0;i<n&&!rc;i++){if(idx){t[0]=idx[3*i];t[1]=idx[3*i+1];t[2]=idx[3*i+2];}else{t[0]=3*i;t[1]=3*i+1;t[2]=3*i+2;}strip_forget();gl_noix=1;rc=primitive(c,&gl_in,sv,t,3,&completed);gl_noix=0;strip_forget();}
 tri_snap=0;ps_lim=0;return rc;
}
/* v10.500 (ACCEL_OPENGL_TRILIST; host bench, Quake 3's batches: GL_TRIANGLES from glDrawElements went a triangle at a time through Mesa's
 * template, the ICD's triangle function, the vertex numbers worked back from pointers and vx_d3d_gl_prim, about a sixth of the frame): the
 * triangles idx[3i..3i+2] of one primitive run, each exactly as vx_d3d_gl_prim(c, sv, idx + 3i, 3, join) would draw it (the same checks,
 * strip joins and words), in one call. A triangle naming a vertex past the store is left out as gl_prim refuses it; the first refusal is
 * returned and *bad counts them (the ICD logs as for one triangle). */
/* .506 (ACCEL_OPENGL_PLAN: Quake 3 draws a batch's indices again for each pass under one array lock): a draw's triangle-run
 * decisions kept (pl_op by triangle: 1 drawn by the fast path, 2 culled or no area, 3 the general path; with its three indices), and the next
 * draw, when the ICD says its store's window vertices are the last draw's (vx_d3d_gl_plan_begin: the locked-array transform results taken
 * again, the same viewport, store and layout) and the template's clip edges, guard band, cull and layout are the recorded ones, takes them
 * for triangles with the same indices in the same order instead of the vertex checks and the area: the same decisions from the same
 * words. Anything different (an index, the order, the key, the fast conditions) drops the plan for the draw and the checks run as before.
 * vx_d3d_plan_stat: [0] triangles from the plan, [1] recorded, [2] plans dropped part way. */
#define PL_N 16384
uint32_t vx_d3d_gl_plan=1,vx_d3d_plan_stat[3];
static uint8_t pl_op[PL_N];static uint32_t pl_ix[PL_N*3],pl_n,pl_pos,pl_mode,pl_key[8],pl_have;static const vx_d3d_context *pl_c;static const uint8_t *pl_vb;
static void pl_drop(void) {pl_mode=0;pl_n=0;pl_have=0;}
static int pl_keyok(const vx_d3d_context *c) {
 uint32_t k[8];k[0]=tc.bxl;k[1]=tc.bxr;k[2]=tc.byt;k[3]=tc.byb;k[4]=tc.guard;k[5]=c->state.rs[22];k[6]=gl_in.fvf;k[7]=gl_in.vertex_stride; /* (C89: no initializer from variables) */
 if(!pl_have){memcpy(pl_key,k,sizeof(k));pl_c=c;pl_vb=gl_in.vertices;pl_have=1;return 1;}
 return pl_c==c&&pl_vb==gl_in.vertices&&!memcmp(pl_key,k,sizeof(k));
}
static void pl_put(const uint32_t *idx,uint32_t op) {
 if(pl_n>=PL_N){pl_drop();return;}
 pl_op[pl_n]=(uint8_t)op;pl_ix[3*pl_n]=idx[0];pl_ix[3*pl_n+1]=idx[1];pl_ix[3*pl_n+2]=idx[2];pl_n++;vx_d3d_plan_stat[1]++;
}
/* a draw begins: same, its store's window vertices are the last draw's: replay the plan the last draw recorded (all of it: pl_mode 1 at
 * its end), else record this draw's */
void vx_d3d_gl_plan_begin(int same) {
 if(!vx_d3d_gl_plan){pl_mode=0;pl_n=0;pl_have=0;return;}
 if(same&&pl_n&&(pl_mode==1||pl_mode==2)){pl_mode=2;pl_pos=0;}
 else{pl_mode=1;pl_n=0;pl_have=0;}
}
/* .573 (.574: permanent; vx_d3d_indexed is set where the VxD has its vertex area): the ICD's primitives by index. Host, .572: with
 * indexed packets on only vx_d3d_gl_trilist's triangles went by index (ix_tri, a table lookup a vertex); fans, polygons, strips and
 * quads wrote strip bursts into a plain packet, and the two forms cannot share one: the Quake 3 scene sent 837 packets for 230, 607
 * of them ended by the other form, which .509 measured as "indexed is slower". And a triangle of the plain form costs the strip
 * path's search for a run to join and a vertex a time (vx_d3d_gl_fan: 750 instructions a triangle in the Quake 2 scene).
 * gl_ix_emit takes a whole primitive as triangles t[3i..3i+2] (the store's vertex numbers, GL's winding) and writes it the way
 * Direct3D's lists go since .515: the vertices of its range lo..hi checked once (fast_v, the fast path's test), each triangle's
 * area by the fast path's expression and cull rule, the kept triangles' span of vertices converted once into the open indexed packet
 * (the ICD's recipe words or fast_vertex_pre_w by store number, so the grid words are taken as before) and their indices written.
 * All or nothing: 0 and nothing written when the template is not on the fast path, a flat strip brings its colours by position
 * (cs_on), an index is past the store, a vertex of the range fails (the clipper's case), the range is sparse (more than 3 vertices
 * a triangle and 32) or a triangle alone spans more than a packet: the caller's old path then draws the primitive as before.
 * vx_d3d_gl_ix: [0] primitives written so, [1] their triangles in, [2] triangles culled or with no area, [3] vertices converted,
 * [4] primitives declined, [5] of them for a vertex that failed. */
#define GIX_T 1024 /* triangles of a strip, fan, polygon or quads call taken by index */
uint32_t vx_d3d_gl_ix[6],vx_d3d_gl_ixw[12],vx_d3d_gl_ixr[5]; /* vx_d3d_gl_ixr: runs whose span's vertices were all new to the packet, all there already, some of each */
static uint16_t gix_slot[VX3DN_IX_VERTICES];static uint16_t gix_keep[VX3DN_MAX_INDICES];static uint32_t gix_t[3*GIX_T+6];
/* The triangles reach gl_ix_emit in their winding (the cull's rule); gix_sw, when set, marks each whose first two vertices change places
 * in the packet: the order the strip it replaces gave the engine (s[i], s[i+1], s[i+2]), so the engine starts its interpolation at the same
 * vertex and the pictures are the same to the last bit (host, 32 bpp: 8 pixels of the Quake 2 scene one step apart without) */
static uint8_t gix_swb[GIX_T+2];static const uint8_t *gix_sw;
static int gl_ix_emit(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *t,uint32_t n,uint32_t *completed,uint32_t *rc) {
 const uint8_t *const vb=gl_in.vertices;const uint32_t st=gl_in.vertex_stride,cnt=gl_in.vertex_count;
 uint32_t lo=0xFFFFFFFFUL,hi=0,i,k=0,cull;
 *rc=0;
 if(!vx_d3d_indexed||!n)return 0;
 if(cs_on||vx_d3d_geo_on||prim_tried){vx_d3d_gl_ixw[cs_on?0:vx_d3d_geo_on?1:2]++;vx_d3d_gl_ix[4]++;return 0;}
 {const uint32_t r=tc_ready(c,&gl_in,sv);if(r||tc.err||tc.skip||!tc.pre||!(vx_d3d_feat&VX_F_BATCH)||c->state.rs[9]==1||c->state.rs[8]!=3||tc.lay.fvf!=gl_in.fvf){vx_d3d_gl_ixw[r?3:tc.err?4:tc.skip?5:!tc.pre?6:c->state.rs[9]==1?7:c->state.rs[8]!=3?8:9]++;vx_d3d_gl_ix[4]++;return 0;}}
 for(i=0;i<3*n;i+=3){const uint32_t a=t[i],b=t[i+1],e=t[i+2];uint32_t l=a<b?a:b,h=a>b?a:b;if(e<l)l=e;if(e>h)h=e;
  if(h>=cnt||h-l>=VX3DN_IX_VERTICES){vx_d3d_gl_ixw[10]++;vx_d3d_gl_ix[4]++;return 0;}
  if(l<lo)lo=l;if(h>hi)hi=h;}
 cull=c->state.rs[22];
 if(hi-lo+1>3*n+32){ /* a sparse range (elements scattered over a locked array): the vertices used checked, then each triangle by
  * the open packet's table (ix_tri: a vertex once a packet, whichever primitive of the draw brought it) */
  for(i=0;i<3*n;i++)if(!fast_v(vb+t[i]*st)){vx_d3d_gl_ix[4]++;vx_d3d_gl_ix[5]++;return 0;}
  strip_forget();
  for(i=0;i<n;i++){const uint32_t *q=t+3*i;const uint8_t *v[3];v[0]=vb+q[0]*st;v[1]=vb+q[1]*st;v[2]=vb+q[2]*st;
   if(cull!=1){const float x0=flt(u32(v[0])),y0=flt(u32(v[0]+4)),x1=flt(u32(v[1])),y1=flt(u32(v[1]+4)),x2=flt(u32(v[2])),y2=flt(u32(v[2]+4));
    const double area=((double)x1-x0)*((double)y2-y0)-((double)y1-y0)*((double)x2-x0);
    if(area==0||(cull==2&&area>0)||(cull==3&&area<0)){vx_d3d_gl_ix[2]++;continue;}}
   if(gix_sw&&gix_sw[i]){uint32_t q3[3];const uint8_t *v3[3];q3[0]=q[1];q3[1]=q[0];q3[2]=q[2];v3[0]=v[1];v3[1]=v[0];v3[2]=v[2];ix_tri(c,&gl_in,sv,q3,v3,completed,rc);}
   else ix_tri(c,&gl_in,sv,q,v,completed,rc);
   if(*rc)return 1;}
  vx_d3d_gl_ix[0]++;vx_d3d_gl_ix[1]+=n;vx_d3d_gl_ixw[11]++;return 1;}
 for(i=lo;i<=hi;i++)if(!fast_v(vb+i*st)){vx_d3d_gl_ix[4]++;vx_d3d_gl_ix[5]++;return 0;}
 strip_forget();
 while(k<n){ /* a run: the triangles from k whose span fits the open packet's room (or a new packet's) */
  const int cur=bq_on&&bq_tid==tc.id&&bq.pre.on==2;
  const uint32_t rv_=cur?VX3DN_IX_VERTICES-bq.vertex_count:VX3DN_IX_VERTICES,rt=cur?(VX3DN_MAX_INDICES-ix_ni)/3:VX3DN_MAX_INDICES/3;
  uint32_t l=0xFFFFFFFFUL,h=0,m=0,nk=0,l2=0xFFFFFFFFUL,h2=0,d,j,sp;uint16_t *ix;const uint32_t *q=t+3*k;
  if(!k&&n<=rt&&hi-lo+1<=rv_){l=lo;h=hi;m=n;} /* the whole primitive fits: its range is known */
  else for(j=k;j<n&&m<rt;j++,m++){const uint32_t a=t[3*j],b=t[3*j+1],e=t[3*j+2];uint32_t x=l,y=h;
   if(a<x)x=a;if(b<x)x=b;if(e<x)x=e;if(a>y)y=a;if(b>y)y=b;if(e>y)y=e;
   if(y-x+1>rv_)break;l=x;h=y;}
  if(!m){ /* no room for the next triangle: a new packet (one triangle's span fits an empty one: checked above) */
   if(ix_open(c,&gl_in,sv,VX3DN_IX_VERTICES+1,1,completed,rc))return 1;
   continue;}
  if(cull==1){nk=m;l2=l;h2=h;} /* nothing culled: every triangle kept, the run's own range (the indices go from t below) */
  else for(j=0;j<m;j++){const uint32_t a=q[3*j],b=q[3*j+1],e=q[3*j+2];const uint8_t *pa=vb+a*st,*pb=vb+b*st,*pe=vb+e*st;
   const float x0=flt(u32(pa)),y0=flt(u32(pa+4)),x1=flt(u32(pb)),y1=flt(u32(pb+4)),x2=flt(u32(pe)),y2=flt(u32(pe+4));
   const double area=((double)x1-x0)*((double)y2-y0)-((double)y1-y0)*((double)x2-x0);
   if(area==0||(cull==2&&area>0)||(cull==3&&area<0))continue;
   if(gix_sw&&gix_sw[k+j]){gix_keep[3*nk]=(uint16_t)(b-l);gix_keep[3*nk+1]=(uint16_t)(a-l);}else{gix_keep[3*nk]=(uint16_t)(a-l);gix_keep[3*nk+1]=(uint16_t)(b-l);}
   gix_keep[3*nk+2]=(uint16_t)(e-l);nk++;
   if(a<l2)l2=a;if(b<l2)l2=b;if(e<l2)l2=e;if(a>h2)h2=a;if(b>h2)h2=b;if(e>h2)h2=e;}
  vx_d3d_gl_ix[1]+=m;vx_d3d_gl_ix[2]+=m-nk;k+=m;
  if(!nk)continue;
  /* The span's vertices into the packet, each once: the packet's table by store number (ix_tri's) says which are there already.
   * Mesa hands a draw over in runs when the clipper takes triangles between them (a grid at the screen's edge: a run a row, each
   * sharing a row of vertices with the last, and the clipped polygons' own triangles by the table between them). */
  sp=h2-l2+1;
  if(sp>nk+8){ /* more vertices in the span than its triangles can share (a run between clipped triangles: the others' vertices lie
   * in it): the vertices used only, by the table (ix_tri) */
   for(j=0;j<nk;j++){uint32_t q3[3];const uint8_t *v[3];if(cull==1){const int w_=gix_sw&&gix_sw[k-m+j];q3[0]=q[3*j+w_];q3[1]=q[3*j+1-w_];q3[2]=q[3*j+2];}else{q3[0]=gix_keep[3*j]+l;q3[1]=gix_keep[3*j+1]+l;q3[2]=gix_keep[3*j+2]+l;}
    v[0]=vb+q3[0]*st;v[1]=vb+q3[1]*st;v[2]=vb+q3[2]*st;ix_tri(c,&gl_in,sv,q3,v,completed,rc);if(*rc)return 1;}
   vx_d3d_gl_ixr[3]++;continue;}
  if(ix_open(c,&gl_in,sv,sp,nk,completed,rc))return 1;
  if(ix_bb==0xFFFFFFFFUL)ix_bb=l2>=IXD_N/2?l2-IXD_N/2:0;
  {uint32_t nw=0;
   for(j=0;j<sp;j++){const uint32_t key=l2+j;uint32_t b=IX_FIND(key);
    if(b==IX_NONE){const uint8_t *p=vb+key*st;b=bq.vertex_count++;IX_PUT(key,b);nw++;
     if(gw_on)gw_vertex(IXW(b),p,key,p);else fast_vertex_pre_w(IXW(b),p,key,p);}
    gix_slot[j]=(uint16_t)b;}
   vx_d3d_ix[1]+=nw;vx_d3d_gl_ix[3]+=nw;vx_d3d_gl_ixr[nw==sp?0:nw?2:1]++;}
  ix=VX3DN_IDX(&bq)+ix_ni;d=l-l2;
  if(cull==1){for(j=0;j<3*nk;j++)ix[j]=gix_slot[q[j]-l2];
   if(gix_sw)for(j=0;j<nk;j++)if(gix_sw[k-m+j]){const uint16_t x_=ix[3*j];ix[3*j]=ix[3*j+1];ix[3*j+1]=x_;}}
  else for(j=0;j<3*nk;j++)ix[j]=gix_slot[gix_keep[j]+d];
  ix_ni+=3*nk;bq.bursts.b[0]=(bq.bursts.b[0]&0xFFFF0000UL)|ix_ni;
  vx_d3d_batched+=nk;vx_d3d_ix[0]+=nk;}
 vx_d3d_gl_ix[0]++;return 1;
}
/* .573: a fan or polygon whose n vertices are n store numbers in a row (what glBegin's polygons and fans are: host, the Quake 2
 * scene, 3,262 of them, 2.6 triangles each, where gl_ix_emit's scans and table cost more than the strip path they replaced): the
 * vertices checked, the triangles the strips made of it (v[k] v[k+1] v[0], then v[0] v[k+1] v[k+2], v[0] v[k+2] v[k+3], k = 1, 4, ..)
 * culled by the same area, the n vertices converted in store order and the kept triangles' indices written, with no table. 1 done
 * (or a send failed: *rc), 0 not taken (gl_ix_emit or the strips follow). */
static int gl_ix_fan(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *v,uint32_t n,uint32_t *completed,uint32_t *rc) {
 const uint8_t *const vb=gl_in.vertices;const uint32_t st=gl_in.vertex_stride,cull=c->state.rs[22];
 uint32_t lo=v[0],hi=v[0],k,nk=0,d,j;uint16_t *ix;
 *rc=0;
 if(cs_on||vx_d3d_geo_on||prim_tried||n>VX3DN_IX_VERTICES)return 0;
 if(tc_ready(c,&gl_in,sv)||tc.err||tc.skip||!tc.pre||!(vx_d3d_feat&VX_F_BATCH)||c->state.rs[9]==1||c->state.rs[8]!=3||tc.lay.fvf!=gl_in.fvf)return 0;
 for(k=1;k<n;k++){const uint32_t a=v[k];if(a<lo)lo=a;if(a>hi)hi=a;}
 if(hi>=gl_in.vertex_count||hi-lo+1!=n)return 0;
 for(k=0;k<n;k++)if(!fast_v(vb+(lo+k)*st))return 0;
 #define GF_TRI(A,B,E,SW) do{const uint32_t a_=(A),b_=(B),e_=(E);int keep_=1;\
  if(cull!=1){const uint8_t *pa=vb+a_*st,*pb=vb+b_*st,*pe=vb+e_*st;const float x0=flt(u32(pa)),y0=flt(u32(pa+4)),x1=flt(u32(pb)),y1=flt(u32(pb+4)),x2=flt(u32(pe)),y2=flt(u32(pe+4));\
   const double area=((double)x1-x0)*((double)y2-y0)-((double)y1-y0)*((double)x2-x0);keep_=!(area==0||(cull==2&&area>0)||(cull==3&&area<0));}\
  if(keep_){if(SW){gix_keep[3*nk]=(uint16_t)(b_-lo);gix_keep[3*nk+1]=(uint16_t)(a_-lo);}else{gix_keep[3*nk]=(uint16_t)(a_-lo);gix_keep[3*nk+1]=(uint16_t)(b_-lo);}gix_keep[3*nk+2]=(uint16_t)(e_-lo);nk++;}}while(0)
 for(k=1;k+1<n;k+=3){GF_TRI(v[k],v[k+1],v[0],0);if(k+2<n){GF_TRI(v[0],v[k+1],v[k+2],1);if(k+3<n)GF_TRI(v[0],v[k+2],v[k+3],0);}}
 #undef GF_TRI
 vx_d3d_gl_ix[0]++;vx_d3d_gl_ix[1]+=n-2;vx_d3d_gl_ix[2]+=n-2-nk;vx_d3d_gl_ixr[4]++;
 strip_forget();
 if(!nk)return 1;
 if(ix_open(c,&gl_in,sv,n,nk,completed,rc))return 1;
 d=bq.vertex_count;
 {uint32_t *w=IXW(d);const uint32_t ws=(bq.bursts.b[0]&VX3DN_IX_V8)?8UL:10UL;const uint8_t *p=vb+lo*st;
  if(gw_on)for(k=0;k<n;k++,w+=ws,p+=st)gw_vertex(w,p,lo+k,p);
  else for(k=0;k<n;k++,w+=ws,p+=st)fast_vertex_pre_w(w,p,lo+k,p);}
 bq.vertex_count+=n;ix=VX3DN_IDX(&bq)+ix_ni;
 for(j=0;j<3*nk;j++)ix[j]=(uint16_t)(gix_keep[j]+d);
 ix_ni+=3*nk;bq.bursts.b[0]=(bq.bursts.b[0]&0xFFFF0000UL)|ix_ni;
 vx_d3d_batched+=nk;vx_d3d_ix[0]+=nk;vx_d3d_ix[1]+=n;vx_d3d_gl_ix[3]+=n;
 return 1;
}
uint32_t vx_d3d_gl_trilist(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *idx,uint32_t n,int join,uint32_t *bad) {
 uint32_t i,first=0,cnt=gl_in.vertex_count;const uint32_t snap=vx_d3d_gl_snap;
 const uint32_t lim=ps_xy&&snap&&ps_mode==snap&&ps_base==gl_in.vertices?(ps_n<cnt?ps_n:cnt):0; /* v10.500: the ICD's grid words (STORESNAP), as vx_d3d_gl_strip */
 int fm=-1,fm_new; /* .503: the run's own loop: -1 not known yet, 0 primitive() for every triangle, 1 the fast path's checks here */
 *bad=0;
 gw_on=vx_d3d_gl_glwords;
 if(vx_d3d_indexed&&!c->lost){uint32_t completed=0,rc=0; /* .573: the run by index (gl_ix_emit); the plan's replay has nothing to save there */
  if(!join)strip_forget();
  tri_snap=snap;ps_lim=lim;i=(uint32_t)gl_ix_emit(c,sv,idx,n,&completed,&rc);tri_snap=0;ps_lim=0;
  if(i){pl_drop();gw_on=0;if(rc)*bad=n;return rc;}}
 for(i=0;i<n;i++,idx+=3){uint32_t completed=0,rc;
  if(c->lost){*bad+=n-i;gw_on=0;return first?first:VX_D3D_SURFACELOST;}
  if(idx[0]>=cnt||idx[1]>=cnt||idx[2]>=cnt){if(!first)first=VX_D3D_INVALID;(*bad)++;continue;}
  if(!join)strip_forget();
  tri_snap=snap;ps_lim=lim;
  /* .503 (ACCEL_OPENGL_GLLIST; host bench, .502: primitive_body, tc_ready, the trilist loop and fast_triangle's vertex checks about a fifth of
   * Quake 3's driver time): primitive_body's conditions for fast_triangle (the template ready, no error or skip, no flat colour copies,
   * solid fill, the geometry counters off) found once for the run (again when the template is not this context's), each store vertex's
   * range check (fast_v, fast_triangle's) made once a run (gw_ok, while its tag is gw_g), the area and cull as fast_triangle, and the
   * triangle placed by fast_tri_place; a triangle with a vertex the checks refuse goes to primitive() as before. The same packets. */
  if(fm>0&&(!tc.valid||tc.ctx!=c))fm=-1;
  fm_new=fm<0;if(fm<0){fm=0;if(vx_d3d_gl_gllist&&!vx_d3d_geo_on&&!prim_tried&&!tc_ready(c,&gl_in,sv)&&!tc.err&&!tc.skip&&!FLATSW(c)&&c->state.rs[8]==3){fm=1;if(gw_tid!=tc.id||gw_fvf!=tc.lay.fvf){gw_tid=tc.id;gw_fvf=tc.lay.fvf;gw_new();}}}
  if(pl_mode&&fm_new&&!(fm>0&&pl_keyok(c)))pl_drop(); /* .506: once a call (the template's conditions found) */
  if(fm>0&&pl_mode==2){ /* .506 (PLAN): the last draw's decision for this triangle, its indices the same */
   if(pl_pos<pl_n&&pl_ix[3*pl_pos]==idx[0]&&pl_ix[3*pl_pos+1]==idx[1]&&pl_ix[3*pl_pos+2]==idx[2]){const uint32_t op=pl_op[pl_pos++];vx_d3d_plan_stat[0]++;
    if(op!=3){rc=0;if(op==1){const uint8_t *v[3];const uint8_t *vb=gl_in.vertices;const uint32_t vst=gl_in.vertex_stride;uint32_t o;v[0]=vb+idx[0]*vst;v[1]=vb+idx[1]*vst;v[2]=vb+idx[2]*vst;vx_d3d_gw[2]++;
      o=VX_PH(VX_PH_ASM);fast_tri_place(c,&gl_in,sv,idx,v,&completed,&rc);VX_PH_BACK(o);}
     tri_snap=0;ps_lim=0;if(!join)strip_forget();if(rc){if(!first)first=rc;(*bad)++;}continue;}
    rc=primitive(c,&gl_in,sv,idx,3,&completed);tri_snap=0;ps_lim=0;if(!join)strip_forget();if(rc){if(!first)first=rc;(*bad)++;}continue;}
   pl_drop();vx_d3d_plan_stat[2]++;}
  if(fm>0){const uint8_t *v[3];uint32_t k;const uint8_t *vb=gl_in.vertices;const uint32_t vst=gl_in.vertex_stride;
   for(k=0;k<3;k++){const uint32_t ix=idx[k];v[k]=vb+ix*vst;
    if(ix<GW_N&&gw_ok[ix]==gw_g)continue;
    if(!fast_v(v[k]))break;
    if(ix<GW_N)gw_ok[ix]=gw_g;}
   if(k==3){float x0=flt(u32(v[0])),y0=flt(u32(v[0]+4)),x1=flt(u32(v[1])),y1=flt(u32(v[1]+4)),x2=flt(u32(v[2])),y2=flt(u32(v[2]+4));const uint32_t cull=c->state.rs[22];
    double area=((double)x1-x0)*((double)y2-y0)-((double)y1-y0)*((double)x2-x0);const int drawn=!(area==0||(cull==2&&area>0)||(cull==3&&area<0));vx_d3d_gw[2]++;
    if(pl_mode==1)pl_put(idx,drawn?1:2);
    rc=0;if(drawn){uint32_t o=VX_PH(VX_PH_ASM);fast_tri_place(c,&gl_in,sv,idx,v,&completed,&rc);VX_PH_BACK(o);}
    tri_snap=0;ps_lim=0;
    if(!join)strip_forget();
    if(rc){if(!first)first=rc;(*bad)++;}
    continue;}
   if(pl_mode==1)pl_put(idx,3);}
  rc=primitive(c,&gl_in,sv,idx,3,&completed);tri_snap=0;ps_lim=0;
  if(!join)strip_forget();
  if(rc){if(!first)first=rc;(*bad)++;}
 }
 gw_on=0;
 return first;
}
/* .503: a triangle-list run of the ICD begins (tris_run): the kept words and checks start again; the run's calls keep them */
void vx_d3d_gl_runstart(void) {gw_new();}
static uint32_t strip_body(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *s,uint32_t n,int flip,uint32_t *completedp);
uint32_t vx_d3d_gl_cullrun=1; /* v10.490: ACCEL_OPENGL_CULLRUN */
uint32_t vx_d3d_gl_cullrun2=1; /* v10.493: ACCEL_OPENGL_CULLRUN2 (the run's area from kept floats) */
uint32_t vx_d3d_gl_strip(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *s,uint32_t n,int flip) {
 uint32_t rc,completed=0;
 if(c->lost)return VX_D3D_SURFACELOST;
 for(rc=0;rc<n;rc++)if(s[rc]>=gl_in.vertex_count)return VX_D3D_INVALID; /* v10.330 (review #150): every index before any vertex is read */
 tri_snap=vx_d3d_gl_snap;gl_strip_on=1;gw_new();gw_on=vx_d3d_gl_glwords; /* .503: the recipe's words */
 ps_lim=ps_xy&&tri_snap&&ps_mode==tri_snap&&ps_base==gl_in.vertices?(ps_n<gl_in.vertex_count?ps_n:gl_in.vertex_count):0; /* v10.490 */
 cs_on=vx_d3d_gl_flatpv==3||vx_d3d_gl_flatpv==4;cs_quad=vx_d3d_gl_flatpv==4;cs_s=s;cs_n=n;cs_vb=gl_in.vertices;cs_stride=gl_in.vertex_stride; /* v10.492 */
 if(cs_on)strip_forget(); /* a run from the last call (the strip's previous chunk) took its last two vertices' own colours: not joined */
 if(vx_d3d_indexed&&n>=3&&n-2<=GIX_T&&!cs_on&&c->state.rs[9]!=1){uint32_t i,m=0,r2=0; /* (.574: asked before the triangles are made: a flat strip made them for nothing, host +0.5% a flat scene) .573: the strip's triangles by index (gl_ix_emit), in strip_body's winding */
  for(i=0;i+2<n;i++){const int odd=((i&1)!=0)!=(flip!=0);gix_swb[i]=(uint8_t)odd;gix_t[m++]=odd?s[i+1]:s[i];gix_t[m++]=odd?s[i]:s[i+1];gix_t[m++]=s[i+2];}
  gix_sw=gix_swb;i=(uint32_t)gl_ix_emit(c,sv,gix_t,n-2,&completed,&r2);gix_sw=0;
  if(i){gl_strip_on=0;gw_on=0;tri_snap=0;ps_lim=0;cs_on=0;return r2;}}
 rc=strip_body(c,sv,s,n,flip,&completed);gl_strip_on=0;gw_on=0;tri_snap=0;ps_lim=0;cs_on=0;return rc;
}
static uint32_t strip_body(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *s,uint32_t n,int flip,uint32_t *completedp) {
 uint32_t completed=0,rc=0,i,tri[3];(void)completedp;
 for(i=0;i+2<n;i++){
  int odd=((i&1)!=0)!=(flip!=0);
  tri[0]=odd?s[i+1]:s[i];tri[1]=odd?s[i]:s[i+1];tri[2]=s[i+2];
  if(strip_fast(c,sv,s+i,tri,&completed,&rc)){
   if(rc)return rc;
   /* the run goes on with no culling: one check and one vertex a triangle (the conditions strip_fast checked hold for this call) */
   if(st_on&&st_b==s[i+2]&&c->state.rs[22]==1){
    while(i+3<n&&bq.vertex_count<VX3DN_MAX_VERTICES){const uint8_t *p=gl_in.vertices+s[i+3]*gl_in.vertex_stride;if(!fast_v(p))break;
     fast_vertex_ix(&bq,bq.vertex_count,p,s[i+3],cs_pc(i+3,p));bq.vertex_count++;bq.bursts.b[bq.bursts.n-1]++;vx_d3d_batched++;vx_d3d_strip_vertices++;i++;}
    st_a=s[i+1];st_b=s[i+2];st_alt=0xFFFFFFFFUL;}
   /* v10.490 (board, Lattice v10.489: culling on, every triangle through strip_fast, 8% of the samples with strip_body): with culling on, the
    * same run: the next triangle's new vertex checked, its area (strip_culled), one vertex appended; a culled one, no area, or a vertex the
    * fast path does not take ends the loop, and the triangle goes through strip_fast as before (which ends the run). ACCEL_OPENGL_CULLRUN 0:
    * every triangle through strip_fast */
   else if(vx_d3d_gl_cullrun&&st_on&&st_b==s[i+2]){
    if(vx_d3d_gl_cullrun2){ /* v10.493 (board, Lattice v10.492: strip_culled 4.7% of the samples): the run's last two vertices' x y kept as floats, each new
     * triangle's area from them and its new vertex only, in the same expression and order as strip_culled (the same double, the same decision) */
     const uint8_t *vb=gl_in.vertices;const uint32_t vst=gl_in.vertex_stride,cull=c->state.rs[22];float ax,ay,bx,by;
     {const uint8_t *pa=vb+s[i+1]*vst,*pb=vb+s[i+2]*vst;ax=flt(u32(pa));ay=flt(u32(pa+4));bx=flt(u32(pb));by=flt(u32(pb+4));}
     while(i+3<n&&bq.vertex_count<VX3DN_MAX_VERTICES){uint32_t j=i+1;int odd=((j&1)!=0)!=(flip!=0);const uint8_t *p=vb+s[j+2]*vst;float cx,cy,x0,y0,x1,y1;double area;
      if(!fast_v(p))break;
      cx=flt(u32(p));cy=flt(u32(p+4));
      if(odd){x0=bx;y0=by;x1=ax;y1=ay;}else{x0=ax;y0=ay;x1=bx;y1=by;}
      area=((double)x1-x0)*((double)cy-y0)-((double)y1-y0)*((double)cx-x0);
      if(area==0||(cull==2&&area>0)||(cull==3&&area<0))break;
      fast_vertex_ix(&bq,bq.vertex_count,p,s[j+2],cs_pc(j+2,p));bq.vertex_count++;bq.bursts.b[bq.bursts.n-1]++;vx_d3d_batched++;vx_d3d_strip_vertices++;vx_d3d_cullrun_tris++;i++;
      ax=bx;ay=by;bx=cx;by=cy;}}
    else
    while(i+3<n&&bq.vertex_count<VX3DN_MAX_VERTICES){uint32_t j=i+1,t[3];int odd=((j&1)!=0)!=(flip!=0);const uint8_t *p=gl_in.vertices+s[j+2]*gl_in.vertex_stride;
     if(!fast_v(p))break;
     t[0]=odd?s[j+1]:s[j];t[1]=odd?s[j]:s[j+1];t[2]=s[j+2];
     if(strip_culled(c,t))break;
     fast_vertex_ix(&bq,bq.vertex_count,p,s[j+2],cs_pc(j+2,p));bq.vertex_count++;bq.bursts.b[bq.bursts.n-1]++;vx_d3d_batched++;vx_d3d_strip_vertices++;vx_d3d_cullrun_tris++;i++;}
    st_a=s[i+1];st_b=s[i+2];st_alt=0xFFFFFFFFUL;}
   continue;}
  /* v10.482: a triangle the strip run does not take, on the engine's flat shading with the strip's colours laid out for a rule (vx_d3d_gl_flatpv
   * 1: each triangle's colours on its first strip vertex s[i], 2: on its last s[i+2]): turned to start at that vertex (the same winding) */
  if(HWFLAT(c)&&vx_d3d_gl_flatpv){uint32_t pv=vx_d3d_gl_flatpv==1?s[i]:vx_d3d_gl_flatpv==4?s[(i&~1UL)+2]:s[i+2],t0=tri[0],t1=tri[1],t2=tri[2]; /* v10.492: 3, 4 the store's own colours: GL's provoking vertex */
   if(pv==t1){tri[0]=t1;tri[1]=t2;tri[2]=t0;}else if(pv==t2){tri[0]=t2;tri[1]=t0;tri[2]=t1;}}
  strip_forget();rc=primitive(c,&gl_in,sv,tri,3,&completed);strip_forget();if(rc)return rc;
 }
 return 0;
}
/* .505 (ACCEL_OPENGL_GLFAN; host bench, Quake 2's world: a polygon went as strips of five, k k+1 0 k+2 k+3 (the ICD's fan_strips; quads
 * a b d c), each copied by strip_send and sent through vx_d3d_gl_strip's entry, its index checks and set-up, about 500 instructions a polygon
 * before its vertices): the polygon's strips in one call, the strip set-up once; each strip through strip_body as
 * vx_d3d_gl_strip(c, sv, strip, m, 0) drew it (the flat colour rule's strip, cs_s, per strip). quads: n / 4 quads, each the strip a b d c.
 * The first failure is returned; the strips after it are still drawn (as strip_send went on). */
/* .509: .506's indexed polygons (ACCEL_OPENGL_INDEXED) removed: VXFLAT v3.3, Quake 2 -3.7%, the clipped lists -4% and -7% (q3 arrays +3% alone) */
uint32_t vx_d3d_gl_fan(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *v,uint32_t n,int quads) {
 uint32_t i,k,m,s[5],rc,first=0,completed=0;
 if(c->lost)return VX_D3D_SURFACELOST;
 tri_snap=vx_d3d_gl_snap;gl_strip_on=1;gw_new();gw_on=vx_d3d_gl_glwords;
 ps_lim=ps_xy&&tri_snap&&ps_mode==tri_snap&&ps_base==gl_in.vertices?(ps_n<gl_in.vertex_count?ps_n:gl_in.vertex_count):0;
 cs_on=vx_d3d_gl_flatpv==3||vx_d3d_gl_flatpv==4;cs_quad=vx_d3d_gl_flatpv==4;cs_vb=gl_in.vertices;cs_stride=gl_in.vertex_stride;
 if(vx_d3d_indexed&&!quads&&n>=3&&!cs_on&&c->state.rs[9]!=1){uint32_t r2=0;if(gl_ix_fan(c,sv,v,n,&completed,&r2)){vx_d3d_gw[3]++;gl_strip_on=0;gw_on=0;tri_snap=0;ps_lim=0;cs_on=0;return r2;}} /* .573: store numbers in a row */
 if(vx_d3d_indexed&&n>=3&&n<=GIX_T&&!cs_on&&c->state.rs[9]!=1){uint32_t nt=0,r2=0,*g=gix_t; /* .573: the fan's, polygon's or quads' triangles by index (gl_ix_emit): the triangles
  * the strips below make of them, in the same order and winding (each strip: s0 s1 s2, then s2 s1 s3, then s2 s3 s4) */
  for(k=quads?0:1;quads?k+3<n:k+1<n;k+=quads?4:3){
   if(quads){*g++=v[k];*g++=v[k+1];*g++=v[k+3];*g++=v[k+3];*g++=v[k+1];*g++=v[k+2];gix_swb[nt]=0;gix_swb[nt+1]=1;nt+=2;}
   else{*g++=v[k];*g++=v[k+1];*g++=v[0];gix_swb[nt++]=0;if(k+2<n){*g++=v[0];*g++=v[k+1];*g++=v[k+2];gix_swb[nt++]=1;if(k+3<n){*g++=v[0];*g++=v[k+2];*g++=v[k+3];gix_swb[nt++]=0;}}}}
  gix_sw=gix_swb;i=nt?(uint32_t)gl_ix_emit(c,sv,gix_t,nt,&completed,&r2):0;gix_sw=0;
  if(i){vx_d3d_gw[3]++;gl_strip_on=0;gw_on=0;tri_snap=0;ps_lim=0;cs_on=0;return r2;}}
 for(k=quads?0:1;quads?k+3<n:k+1<n;k+=quads?4:3){
  if(quads){s[0]=v[k];s[1]=v[k+1];s[2]=v[k+3];s[3]=v[k+2];m=4;}
  else{m=0;s[m++]=v[k];s[m++]=v[k+1];s[m++]=v[0];if(k+2<n){s[m++]=v[k+2];if(k+3<n)s[m++]=v[k+3];}}
  if(c->lost){if(!first)first=VX_D3D_SURFACELOST;break;}
  for(i=0;i<m;i++)if(s[i]>=gl_in.vertex_count)break;
  if(i<m){if(!first)first=VX_D3D_INVALID;continue;} /* as vx_d3d_gl_strip: this strip refused, the next drawn */
  cs_s=s;cs_n=m;if(cs_on)strip_forget();
  rc=strip_body(c,sv,s,m,0,&completed);if(rc&&!first)first=rc;vx_d3d_gw[3]++;}
 gl_strip_on=0;gw_on=0;tri_snap=0;ps_lim=0;cs_on=0;return first;
}
uint32_t vx_d3d_gl_flush(vx_d3d_context *c,const vx_d3d_services *sv) {uint32_t completed=0;fail_set=0;return vx_d3d_flush(c,sv,&completed);}
/* the state (render states, texture stages, viewport, target, depth) changed: the next primitive builds its template again */
void vx_d3d_gl_state(void) {template_reset();gw_new();} /* after vx_d3d_gl_flush */
void vx_d3d_gl_drop(void) {bq_on=0;strip_forget();template_reset();gw_new();}
/* v10.360 (host clip check; Quake 2's one-frame streaks): Mesa's clipper writes each clipped polygon's new vertices into the same slots past
 * the vertex buffer's count, so an index names a different vertex in the next clipped polygon. The strip and list joins go by index: a
 * triangle of the next clipped polygon was joined to the last one's vertices (a duplicate vertex, a triangle left out, or one stretched to a
 * vertex of another polygon). The ICD calls this before each clipped polygon or line: the batch keeps its vertices, the joins start again. */
void vx_d3d_gl_unjoin(void) {strip_forget();ix_vb=0;gw_new();} /* .503: and the kept words */ /* v10.501: and the indexed table (the clipper's slots are reused) */ /* v10.288: the batch forgotten unsent (its context is not one of ours: it cannot send it) */
/* a clear of rect (in the target's coordinates) through the VxD: flags 1 colour, 2 depth, 4 stencil (D3DCLEAR_*); z in 0..1 */
uint32_t vx_d3d_gl_clear(vx_d3d_context *c,const vx_d3d_services *sv,uint32_t flags,uint32_t color,double z,uint32_t stencil,const vx3dn_rect *r) {
 static vx3dn_packet q;uint32_t completed=0,rc;
 if(c->lost)return VX_D3D_SURFACELOST;
 vx_d3d_cut_tag=8;rc=vx_d3d_gl_flush(c,sv);if(rc)return rc;
 if((flags&4)&&c->depth.format!=VX3DN_D24S8)flags&=~4UL;
 if((flags&6)&&!c->depth.format)flags&=~6UL;
 if(!(z>=0))z=0;if(z>1)z=1; /* v10.330 (review #88): a NaN is 0 (it failed both comparisons and reached the integer conversion) */
 packet(&q,c,VX3DN_CLEAR);q.clear_flags=(flags&7)|((flags&4)?(stencil&255UL)<<8:0);q.clear_color=color;q.clear_depth=(flags&2)?(uint32_t)(z*4294967295.0+0.5):0;
 if(!VX3DN_CLEAR_WHAT(q.clear_flags)||r->right<=r->left||r->bottom<=r->top)return 0;
 if(r->left<0||r->top<0||r->right>(int32_t)c->target.width||r->bottom>(int32_t)c->target.height)return VX_D3D_INVALID;
 q.clip=*r;return submit(sv,&q,c,&completed);
}
static void indices(uint32_t op,const uint8_t *p,uint32_t base,uint32_t i,uint32_t *a,uint32_t *b,uint32_t *c) {
 if(op==18) {*a=base+i*3;*b=*a+1;*c=*a+2;}
 else if(op==19) {*a=base+i;*b=base+i+1;*c=base+i+2;if(i&1){uint32_t t=*b;*b=*c;*c=t;}}
 else if(op==21) {*a=base+i+1;*b=base+i+2;*c=base;}
 else if(op==3||op==26) {uint32_t step=op==3?8:6;*a=base+u16(p+i*step);*b=base+u16(p+i*step+2);*c=base+u16(p+i*step+4);}
 else if(op==20) {*a=base+u16(p+i*2);*b=base+u16(p+i*2+2);*c=base+u16(p+i*2+4);if(i&1){uint32_t t=*b;*b=*c;*c=t;}}
 else {*a=base+u16(p+(i+1)*2);*b=base+u16(p+(i+2)*2);*c=base+u16(p);}
}
static uint8_t vv_ok[131072/8];static uint32_t vv_fresh,vv_blo=0xFFFFFFFFUL,vv_bhi; /* .535: vv_blo..vv_bhi the bytes of vv_ok set since the last clear */
/* .535: a call clears only the bytes the calls before it set (vv_blo..vv_bhi), not its whole vertex count: GTA3's DrawPrimitives2 calls
 * cleared up to 16 KB each byte by byte (__STOSB, 0.9% of its gameplay frames on .534). Every bit is clear at a call's start, as before. */
#define VV_MARK(b) do{uint32_t b_=(b);if(b_<vv_blo)vv_blo=b_;if(b_>vv_bhi)vv_bhi=b_;}while(0)
/* .527: v10.468's ACCEL_D3D_PFVV2 (the vertices not yet checked prefetched before a chunk's checks) removed: a loss in three of four GTA3
 * A/B runs (.523-.526). */
/* v10.454: vv_lim, worked out once a DrawPrimitives2 call with the check cache, is the first index whose whole vertex is not inside the
 * vertex data (an index below it passes every bounds test of valid_vertex); below it a vertex passes when x, y, z are finite and rhw is
 * positive and finite: one integer test of the four words (vv_good; rhw - 1 < 0x7F7FFFFF: positive, not 0, not infinite, not NaN).
 * The indexed path (ix_check) uses it; vv_fast_n counts the vertices it passed. .527: v10.454's ACCEL_D3D_FASTVV (the same test in the
 * list path's vertex checks, the VxD's feature word bit 28) removed: a small loss in every GTA3 A/B run (.523-.526). */
uint32_t vv_fast_n,vx_d3d_vvq;static uint32_t vv_lim; /* .548: vx_d3d_vvq the per-triangle path's vertices passed by vv_good before valid_vertex (QueryPhases 312) */
static void vv_limit(const vx_d3d_input *in) {
 uint32_t size=vx_d3d_vertex_size(in->fvf),n;
 vv_lim=0;
 if(!size||!in->vertices||in->vertex_stride<size||in->vertex_bytes<size)return;
 n=(in->vertex_bytes-size)/in->vertex_stride+1;if(n>in->vertex_count)n=in->vertex_count;if(n>131072UL)n=131072UL;
 vv_lim=n;
}
static int vv_good(const vx_d3d_input *in,uint32_t i) { /* i < vv_lim */
 const uint8_t *p=in->vertices+i*in->vertex_stride;uint32_t x=u32(p),y=u32(p+4),z=u32(p+8),w=u32(p+12);
 return ((x&0x7F800000UL)!=0x7F800000UL)&((y&0x7F800000UL)!=0x7F800000UL)&((z&0x7F800000UL)!=0x7F800000UL)&(w-1UL<0x7F7FFFFFUL);
}
static uint32_t vv(const vx_d3d_input *in,uint32_t i) {
 uint32_t r;if(i<in->vertex_count&&i<131072UL&&(vv_ok[i>>3]&(1u<<(i&7))))return 0; /* v10.237: a bit counts only below this call's vertex count (only that part is cleared) */
 r=valid_vertex(in,i);if(!r&&i<131072UL){vv_ok[i>>3]|=(uint8_t)(1u<<(i&7));VV_MARK(i>>3);}return r;
}
static uint32_t stateset(vx_d3d_context *c,const uint8_t *p) {
 uint32_t op=u32(p),handle=u32(p+4),i,j,k;vx_d3d_block *b=0;
 if(!handle)return VX_D3D_INVALID;
 for(i=0;i<VX_D3D_BLOCKS;i++)if(c->blocks[i].handle==handle){b=&c->blocks[i];break;}
 if(op==0) {
  if(c->recording||b)return VX_D3D_INVALID;
  for(i=0;i<VX_D3D_BLOCKS;i++)if(!c->blocks[i].handle)break;
  if(i==VX_D3D_BLOCKS)return VX_D3D_UNSUPPORTED;
  b=&c->blocks[i];memset(b,0,sizeof(*b));b->handle=handle;b->state=c->state;c->recording=i+1;return 0;
 }
 if(!b)return VX_D3D_INVALID;
 if(op==1) {if(c->recording!=i+1)return VX_D3D_INVALID;c->recording=0;return 0;}
 if(c->recording)return VX_D3D_INVALID;
 if(op==2){memset(b,0,sizeof(*b));return 0;}
 if(op!=3&&op!=4)return VX_D3D_UNSUPPORTED;
 for(j=0;j<VX_D3D_RS;j++)if(b->rs_mask[j/32]&(1UL<<(j%32))) {
  if(op==3)c->state.rs[j]=b->state.rs[j];else b->state.rs[j]=c->state.rs[j];
 }
 for(j=0;j<8;j++)for(k=0;k<32;k++)if(b->tss_mask[j]&(1UL<<k)) {
  if(op==3)c->state.tss[j][k]=b->state.tss[j][k];else b->state.tss[j][k]=c->state.tss[j][k];
 }
 if(b->viewport_mask){if(op==3)c->state.viewport=b->state.viewport;else b->state.viewport=c->state.viewport;}
 return 0;
}
/* v10.472: an indexed triangle list token's triangles into indexed packets in one loop (board, VXFETCH v2.5, tiny shuffled: 1,450 cycles a
 * triangle of bookkeeping: the index table's hash, every vertex range-checked for every triangle and checked again before the loop, a
 * six-argument call and the profiler's tests per triangle). The caller has checked the token's every index is below vv_lim (no index can
 * make the call fail) and that the template takes the fast path into indexed packets. A vertex is checked once, when it enters the packet
 * (valid, inside the viewport and the depth range, rhw positive: valid_vertex's and fast_triangle's tests); one already in the packet was
 * checked with this template. The cull is fast_triangle's, on the same doubles. A triangle with a vertex that fails stops the loop: the
 * caller draws it through the per-triangle path (the bad-vertex drop, the general path), then comes back. Returns the triangles done
 * (drawn or culled); *rc a send's failure. While the profile is on: vx_d3d_ft[27] its cycles / 16, [28] its triangles, [29] its stops,
 * [30] the vertices it converted, [31] of [27] the packets begun (ix_open: the full packet's send to the queue and the VxD, the template
 * copied). QueryPhases words 207-211. */
static int ix_check(const vx_d3d_input *in,uint32_t a,const uint8_t *p) { /* a vertex entering a packet: valid (a < vv_lim), inside */
 uint8_t *o=&vv_ok[a>>3];const uint8_t bit=(uint8_t)(1u<<(a&7));
 if(!(*o&bit)){if(!vv_good(in,a))return 0;*o|=bit;VV_MARK(a>>3);vv_fast_n++;}
 return GB_IN(u32(p),tc.bxl,tc.bxr)&&GB_IN(u32(p+4),tc.byt,tc.byb)&&u32(p+8)<=0x3F800000UL&&u32(p+12)-1UL<0x7F7FFFFFUL;
}
#include "vx_d3d_span.inc" /* .545: the pass-through's span checked and converted four vertices at a time (SSE2) */
/* .515: ACCEL_D3D_IXPASS (1): the token's triangles passed through when the engine culls them (ACCEL_D3D_HWCULL, cull mode 2 or 3) or
 * nothing is culled (mode 1): no per-triangle area, cull or index lookup. A run of triangles whose indices span at most the packet's free
 * vertices takes the span's source vertices once each (checked once, converted once, in order) and its indices as they are less the span's
 * start: a few compares a triangle for the span, a subtraction an index. A span much wider than the run's vertices (more than 3 a
 * triangle and 32), or a vertex that fails its check, ends the pass: the per-triangle loop below takes the rest. vx_d3d_px: [0] triangles
 * passed through, [1] vertices converted by it, [2] runs, [3] runs refused as sparse, [4] runs refused for a vertex, [5] packets begun */
/* .524: .522's ACCEL_D3D_IXPF (the run's source vertices prefetched ahead of their checks) removed: no saving in three GTA3 A/B runs.
 * .522 (was): ACCEL_D3D_IXPF: the run's source vertices asked for ahead of their checks
 * (PREFETCHT0 eight vertices ahead, the first eight at the run's start): the pass-through reads each of a run's vertices twice, the check
 * then the conversion, the first time from memory (GTA3: about 20,000 vertices a frame just written by the runtime, far more than the
 * board's 128 KB second-level cache); the misses overlap the checks instead of following one another, and the conversion finds them in
 * the cache. The same packets (a prefetch only reads). vx_d3d_ixpf_n: the prefetches. "Triangles in" for a run is counted in one step
 * (.520 called the per-triangle counter, 255 cycles a triangle while VXD3DMON watched; the runs' source vertices are their spans). */
/* .548: a run refused as sparse (its span wider than 3 a triangle and 32) handed the rest of the token to the per-triangle loop (GTA3 on
 * .547: 96,555 runs, about 22% of its indexed-list triangles one by one). ix_pass sets ixp_sparse to the refused run's triangles; ix_list
 * takes those one by one and then calls ix_pass again for the rest. vx_d3d_px_back counts the returns (QueryPhases 313). */
static uint32_t ixp_sparse,ixp_nonew;uint32_t vx_d3d_px_back,vx_d3d_tokrun[2]; /* .556: vx_d3d_tokrun, below */ /* ixp_nonew: on a return, ix_pass begins no packet (one more packet costs the VxD more than the loop saves) */
/* .557: a run of m triangles whose indices lie in lo..hi (at most a packet's vertices) drawn by the vertices it uses: ix_rmap[i - lo] the
 * packet index of source vertex i (IX_NONE: not met yet), ix_rsrc the source vertices in the order met. Pass 1 checks each vertex as it
 * is met (ix_check, as ix_list's loop) and stops before the first triangle that uses one that fails; then the packet is opened for what
 * was met, the vertices converted (fast_vertex_pre_w, the words ix_list's loop writes) and the indices written. Nothing is written
 * before the checks are done. Returns the triangles drawn (0: the first one uses a failing vertex); *rc a send's failure.
 * vx_d3d_ref: [0] runs, [1] their triangles, [2] vertices converted, [3] runs ended by a failing vertex. */
static uint16_t ix_rmap[VX3DN_IX_VERTICES],ix_rsrc[VX3DN_IX_VERTICES],ix_rst[VX3DN_IX_VERTICES];static uint32_t ix_rg;uint32_t vx_d3d_ref[4]; /* .559: ix_rst, the run that set a map entry (a stamp instead of clearing the span's entries for every run) */
static uint32_t ix_ref(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint8_t *q,uint32_t ist,uint32_t base,uint32_t m,uint32_t lo,uint32_t hi,uint32_t *completed,uint32_t *rc) {
 const uint8_t *const vb=in->vertices,*p=q;const uint32_t st=in->vertex_stride,span=hi-lo+1;uint32_t t,j,nv=0,d;uint16_t *ix;
 *rc=0;
 if(span>VX3DN_IX_VERTICES)return 0;
 if(++ix_rg>0xFFFFUL){memset(ix_rst,0,sizeof(ix_rst));ix_rg=1;}
 for(t=0;t<m;t++,p+=ist){
  for(j=0;j<3;j++){const uint32_t i=base+u16(p+2*j),r=i-lo;
   if(ix_rst[r]==(uint16_t)ix_rg)continue;
   if(!ix_check(in,i,vb+i*st))break;
   ix_rst[r]=(uint16_t)ix_rg;ix_rmap[r]=(uint16_t)nv;ix_rsrc[nv++]=(uint16_t)r;}
  if(j<3)break;} /* (a vertex of this triangle met before the one that fails stays: it passed, and is converted unused) */
 if(!t){vx_d3d_ref[3]++;return 0;}
 if(ix_open(c,in,sv,nv,t,completed,rc))return 0;
 d=bq.vertex_count;
 for(j=0;j<nv;j++){const uint8_t *v=vb+(lo+ix_rsrc[j])*st;fast_vertex_pre_w(IXW(d+j),v,0xFFFFFFFFUL,v);}
 bq.vertex_count+=nv;
 ix=VX3DN_IDX(&bq)+ix_ni;
 for(j=0,p=q;j<t;j++,p+=ist,ix+=3){ix[0]=(uint16_t)(d+ix_rmap[base+u16(p)-lo]);ix[1]=(uint16_t)(d+ix_rmap[base+u16(p+2)-lo]);ix[2]=(uint16_t)(d+ix_rmap[base+u16(p+4)-lo]);}
 ix_ni+=3*t;bq.bursts.b[0]=(bq.bursts.b[0]&0xFFFF0000UL)|ix_ni;
 if(vx_d3d_geo_on){vx_d3d_geo[0]+=t;vx_d3d_geo[1]+=nv;vx_d3d_geo[6]+=t;}
 vx_d3d_batched+=t;vx_d3d_ix[0]+=t;vx_d3d_ix[1]+=nv;vx_d3d_ref[0]++;vx_d3d_ref[1]+=t;vx_d3d_ref[2]+=nv;if(t<m)vx_d3d_ref[3]++;
 return t;
}
/* .567 (.572: always, its test switch vx_d3d_cullpass removed: board, .569, VXCULL v1.2 0 pixels differ and 35.68 -> 31.12 ms, 36.51
 * -> 35.36 ms a frame; WinTune's Direct3D test 198.3 -> 189.0 ms a frame against .567): the pass-through with CULLMODE clockwise or counter-clockwise. The engine's own cull draws nothing
 * on the board (.518), so a culled list went a triangle at a time through ix_list's loop (the lookup of each index in the packet's
 * table, the area, a vertex at a time into the packet): board, WinTune's Direct3D test on .566, every one of its 9.4 million list
 * triangles, 792 cycles each, 64 of a frame's 82 ms in the driver. Direct3D's default cull mode is counter-clockwise: this is the
 * path of most DirectX 5 to 7 programs, where GTA3 (cull none) never comes. Now a run is filtered first: each triangle's area from its
 * source vertices, with ix_list's own expression (doubles) and rule (no area, or the culled winding, is left out); the triangles kept
 * go into ixc_keep, the span narrows to the vertices they use, and the run goes on as any other: the span converted at once, the kept
 * indices written; when more than an eighth of that span (and 8 vertices) is used by culled triangles only, the vertices used are
 * converted one by one instead, in the order met (ix_ref's tables), so no more vertices go to the VxD than the loop sent. A run with
 * nothing kept opens no packet. vx_d3d_cp: [0] runs filtered, [1] triangles culled in them (and in fans), [2] runs with nothing kept,
 * [3] runs drawn by the vertices used. A run whose span holds a failing vertex goes to ix_list's loop as before (ix_ref does not cull). */
uint32_t vx_d3d_cp[6]; /* .568: [4] runs filtered with their vertices checked one by one (the span check failed), [5] runs a failing vertex ended */
static uint16_t ixc_keep[VX3DN_MAX_INDICES];static uint8_t ixc_bad[VX3DN_IX_VERTICES]; /* .569: ixc_bad, a span's vertices that fail (.568's ixc_ok gone) */
static int ix_off(const vx_d3d_input *in,uint32_t i0,uint32_t i1,uint32_t i2,const uint8_t *a,const uint8_t *b,const uint8_t *e);extern uint32_t vx_d3d_off[4]; /* .568: below */
static uint32_t ix_pass(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint8_t *q,uint32_t ist,uint32_t base,uint32_t n,uint32_t *completed,uint32_t *rc) {
 const uint8_t *const vb=in->vertices;const uint32_t st=in->vertex_stride,cull=c->state.rs[22];
 uint32_t k=0;
 *rc=0;
 while(k<n) {
  uint32_t lo,hi,m=0,j,rv_,rt,d;const uint8_t *p=q;uint16_t *ix;int chk=0; /* .568: chk, a culled run whose span check failed */
  const int cur=bq_on&&bq_tid==tc.id&&bq.pre.on==2&&ix_vb==vb&&ix_stride==st&&ix_fvf==in->fvf;
  if(ixp_nonew&&!cur)break; /* .548: a return fills the open packet only */
  rv_=cur?VX3DN_IX_VERTICES-bq.vertex_count:VX3DN_IX_VERTICES; /* .533: the limit is a constant, so an open packet never holds more (.532's clamp for a lowered limit gone) */rt=cur?(VX3DN_MAX_INDICES-ix_ni)/3:VX3DN_MAX_INDICES/3;
  /* .556: a whole token whose range was proven (ix_list: every vertex from its base to its largest index passes, and the range is at
   * most 3 a triangle and 32 more) and fits the packet is one run: its largest index is the token's (dp2_body's, VXS_MAX16), its
   * smallest one pass of compares, in place of the pass below that keeps both for every triangle and tests the room each time (about
   * 35 instructions a triangle; g.rec: 11,000 triangles a frame). The run is the one that pass would make. vx_d3d_tokrun: [0] tokens
   * taken whole, [1] their triangles. */
  if(!k&&n==ix_tok_n&&n<=rt&&ix_proof_state==1&&ix_proof_tid==tc.id){uint32_t mn=0xFFFFUL;const uint8_t *p3=q;
   if(ist==6)for(j=0;j<3*n;j++,p3+=2){const uint32_t a=u16(p3);if(a<mn)mn=a;}
   else for(j=0;j<n;j++,p3+=ist){const uint32_t a=u16(p3),b=u16(p3+2),e=u16(p3+4);if(a<mn)mn=a;if(b<mn)mn=b;if(e<mn)mn=e;}
   if(ix_tok_hi-(base+mn)+1<=rv_){lo=base+mn;hi=ix_tok_hi;m=n;vx_d3d_tokrun[0]++;vx_d3d_tokrun[1]+=n;}}
  if(!m){
  lo=hi=base+u16(p);
  for(j=k;j<n&&m<rt;j++,p+=ist){const uint32_t a=base+u16(p),b=base+u16(p+2),e=base+u16(p+4);
   uint32_t l2=lo,h2=hi;if(a<l2)l2=a;if(b<l2)l2=b;if(e<l2)l2=e;if(a>h2)h2=a;if(b>h2)h2=b;if(e>h2)h2=e;
   if(h2-l2+1>rv_)break;lo=l2;hi=h2;m++;}}
  /* .540: a new packet only for a triangle an empty one holds. One whose own span is wider (the driver's T&L: a clipped triangle's new
   * vertices at the end of the scratch buffer) left a new, empty packet open; ix_list then stopped at a bad vertex before adding any, and
   * the empty packet went to the VxD, which refused it (board, GTA3 on .539: the context lost, reason 26 value 801A0001) */
  if(!m){uint32_t a=base+u16(q),b=base+u16(q+2),e=base+u16(q+4),l=a<b?(a<e?a:e):(b<e?b:e),h=a>b?(a>e?a:e):(b>e?b:e);
   if(cur&&ix_ni&&h-l+1<=VX3DN_IX_VERTICES&&!ixp_nonew){if(ix_open(c,in,sv,VX3DN_IX_VERTICES+1,1,completed,rc))return k;vx_d3d_px[5]++;continue;}break;} /* no room: a new packet (.548: not on a return) */
  if(hi-lo+1>3*m+32){vx_d3d_px[3]++;ixp_sparse=m;break;} /* .548: ix_list takes these m one by one, then comes back (ixp_sparse) */
  if(ix_proof_state==1&&ix_proof_tid==tc.id&&lo>=ix_tok_lo&&hi<=ix_tok_hi)vx_d3d_vxs_skip++; /* .547: inside the token's proven range (board, GTA3 on .546: each proven
   * span was checked twice, 39.1 million vertices checked for 19.2 million converted) */
  else if(!vxs_span_ok(in,lo,hi)){vx_d3d_px[4]++; /* .545: vx_d3d_span.inc (was ix_check a vertex) */
   /* .557: the span holds a vertex that fails, most often one no triangle of the run uses (GTA3 on .556: 223,676 of 694,050 runs; the
    * rest of the token then went one triangle at a time through ix_list's loop, about 430 instructions each). The run is drawn by the
    * vertices it uses instead (ix_ref): each checked and converted once, in the order met. A triangle that uses a failing vertex ends
    * it: that one goes to ix_list's loop (ixp_sparse), which culls or stops as before and comes back here. */
   /* .558: always (board, .557: VXPASS v1.3 scene 5, 0 pixels differ, median 34.94 -> 33.83 ms; GTA3: 409,067 runs, nothing left out) */
   /* .568: a culled run with a failing vertex is filtered below with each vertex checked as a triangle first uses it (chk), as
    * ix_list's loop does. .567 sent these runs to the loop: board, WinTune's Direct3D test on .567, 25,826 runs, three quarters of
    * its triangles (the runtime's clipper leaves its off-screen vertices in the spans; no triangle sent uses them), 797 cycles a
    * triangle as before. */
   if(cull!=1)chk=1;
   else{const uint32_t t=ix_ref(c,in,sv,q,ist,base,m,lo,hi,completed,rc);
    if(*rc)return k;
    k+=t;q+=t*ist;if(t<m){ixp_sparse=1;break;}
    continue;}}
  if(cull!=1){ /* .567: the run filtered (above ix_pass); .569: in one pass with nothing kept a vertex */
   uint32_t nk=0,mm,offc=0,sp=hi-lo+1,nbad=0,r;int stopped=0,used;uint16_t *kp=ixc_keep;const uint8_t *p4=q;
   /* .569: chk (the span check failed): every vertex of the span tested once (ix_check, the loop's test) into ixc_bad. Board, .568:
    * testing each vertex as a triangle first used it, with a stamp a vertex and the vertices converted one by one, was slower than
    * the loop (VXCULL v1.1 scene 4: 36.67 -> 42.74 ms a frame; host 221 instructions a triangle against the loop's 192). */
   if(chk)for(r=0;r<sp;r++){const uint32_t b=!ix_check(in,lo+r,vb+(lo+r)*st);ixc_bad[r]=(uint8_t)b;nbad+=b;}
   for(j=0;j<m;j++,p4+=ist){const uint32_t a=base+u16(p4),b=base+u16(p4+2),e=base+u16(p4+4);
    const vx_f32a *const f0=(const vx_f32a *)(vb+a*st),*const f1=(const vx_f32a *)(vb+b*st),*const f2=(const vx_f32a *)(vb+e*st);
    double area;
    /* the loop's order: a triangle with a vertex that fails is left out when it is wholly past a clip edge (ix_off) and else ends
     * the run (ix_list's loop meets it next and stops there, as before); then the area */
    if(chk&&(ixc_bad[a-lo]|ixc_bad[b-lo]|ixc_bad[e-lo])){
     if(ix_off(in,a,b,e,(const uint8_t *)f0,(const uint8_t *)f1,(const uint8_t *)f2)){offc++;vx_d3d_off[0]++;continue;}
     stopped=1;break;}
    area=((double)f1[0]-f0[0])*((double)f2[1]-f0[1])-((double)f1[1]-f0[1])*((double)f2[0]-f0[0]);
    if(area==0||(cull==2&&area>0)||(cull==3&&area<0))continue;
    kp[0]=(uint16_t)(a-lo);kp[1]=(uint16_t)(b-lo);kp[2]=(uint16_t)(e-lo);kp+=3;nk++;}
   mm=j; /* the triangles taken: kept or left out; fewer than m when one stopped the run */
   vx_d3d_cp[0]++;vx_d3d_cp[1]+=mm-nk;vx_d3d_tk[9]+=mm-nk;if(chk)vx_d3d_cp[4]++;if(vx_d3d_geo_on){vx_d3d_geo[0]+=mm-offc;vx_d3d_geo[3]+=mm-nk;}
   if(!nk){vx_d3d_cp[2]++;k+=mm;q+=mm*ist;if(stopped){vx_d3d_cp[5]++;ixp_sparse=1;break;}continue;}
   /* The vertices. Fewer than half the run's triangles kept: only the vertices they use, one by one in the order met (a vertex only
    * culled triangles use costs the VxD's copy and the upload for nothing). Else the run's whole span four at a time, the vertices of
    * its culled triangles with it; a vertex that fails is in the span only unused (the engine fetches by index): its words are
    * written over with a kept triangle's first vertex, so every vertex of the packet is one that passed. A span that would be
    * converted with arithmetic (tri_snap, a z bias: vxs_span_conv's own test) never takes a failing vertex: the vertices used. */
   used=2*nk<m||(nbad&&(tri_snap||tc.zb!=0));
   if(used){uint32_t nv=0;
    if(++ix_rg>0xFFFFUL){memset(ix_rst,0,sizeof(ix_rst));ix_rg=1;}
    for(j=0;j<3*nk;j++){r=ixc_keep[j];if(ix_rst[r]!=(uint16_t)ix_rg){ix_rst[r]=(uint16_t)ix_rg;ix_rmap[r]=(uint16_t)nv;ix_rsrc[nv++]=(uint16_t)r;}}
    {const uint32_t g=ix_g;if(ix_open(c,in,sv,nv,nk,completed,rc))return k;if(ix_g!=g&&bq.vertex_count==0)vx_d3d_px[5]++;}
    d=bq.vertex_count;
    for(j=0;j<nv;j++){const uint8_t *v=vb+(lo+ix_rsrc[j])*st;fast_vertex_pre_w(IXW(d+j),v,0xFFFFFFFFUL,v);}
    bq.vertex_count+=nv;ix=VX3DN_IDX(&bq)+ix_ni;
    for(j=0;j<3*nk;j++)ix[j]=(uint16_t)(d+ix_rmap[ixc_keep[j]]);
    vx_d3d_cp[3]++;sp=nv;}
   else{
    {const uint32_t g=ix_g;if(ix_open(c,in,sv,sp,nk,completed,rc))return k;if(ix_g!=g&&bq.vertex_count==0)vx_d3d_px[5]++;}
    d=bq.vertex_count;vxs_span_conv(in,lo,hi,d);
    if(nbad){const uint32_t ws=(bq.bursts.b[0]&VX3DN_IX_V8)?8UL:10UL;const uint32_t *const good=IXW(d+ixc_keep[0]);
     for(r=0;r<sp;r++)if(ixc_bad[r]){uint32_t *w=IXW(d+r),x;for(x=0;x<ws;x++)w[x]=good[x];}}
    bq.vertex_count+=sp;ix=VX3DN_IDX(&bq)+ix_ni;
    {const uint32_t count=3*nk,pair=(d<<16)|d;
     /* .573 (peer review): two indices an addition (each at most 511 with the base: no carry out of the low half); host, linked HAL, a culled list: -5% instructions a call */
     for(j=0;j+1<count;j+=2)
      *(vx_u32a*)(void*)(ix+j)=u32((const uint8_t*)(ixc_keep+j))+pair;
     if(count&1)ix[count-1]=(uint16_t)(ixc_keep[count-1]+d);}}
   ix_ni+=3*nk;bq.bursts.b[0]=(bq.bursts.b[0]&0xFFFF0000UL)|ix_ni;
   vx_d3d_px[0]+=nk;vx_d3d_px[1]+=sp;vx_d3d_px[2]++;
   vx_d3d_batched+=nk;vx_d3d_ix[0]+=nk;vx_d3d_ix[1]+=sp;if(vx_d3d_geo_on){vx_d3d_geo[1]+=sp;vx_d3d_geo[6]+=nk;}
   k+=mm;q+=mm*ist;if(stopped){vx_d3d_cp[5]++;ixp_sparse=1;break;}continue;}
  {const uint32_t g=ix_g;if(ix_open(c,in,sv,hi-lo+1,m,completed,rc))return k;if(ix_g!=g&&bq.vertex_count==0)vx_d3d_px[5]++;}
  d=bq.vertex_count;
  vxs_span_conv(in,lo,hi,d);bq.vertex_count+=hi-lo+1; /* .545: vx_d3d_span.inc (was fast_vertex_pre_w a vertex; .523: IXW) */
  /* .527: fast_vertex_pre_w counts each vertex in vx_d3d_geo[11] (the span was counted here a second time) */
  ix=VX3DN_IDX(&bq)+ix_ni;d-=lo;
  if(vx_d3d_geo_on){vx_d3d_geo[0]+=m;vx_d3d_geo[1]+=hi-lo+1;} /* .520: "triangles in" counts them; .522: in one step, the span as their source vertices */
  /* .556: the run's indices two at a time when they are back to back (command 26, a strip's list): one 32-bit add moves two. The
   * gain may be negative (a run that begins above the packet's vertex count): gain * 0x10001 then has gain - 1 in its upper half, and
   * the lower index's add always carries (a packet index is never negative), so both halves come out right. */
  {const uint32_t gain=base+d;
  if(ist==6){const uint32_t nn=3*m,two=gain*0x00010001UL;
   for(j=0;j+1<nn;j+=2)*(vx_u32a*)(void*)(ix+j)=u32(q+2*j)+two;
   if(nn&1)ix[nn-1]=(uint16_t)(u16(q+2*(nn-1))+gain);
   q+=m*6;}
  else for(j=0;j<m;j++,q+=ist,ix+=3){ix[0]=(uint16_t)(base+u16(q)+d);ix[1]=(uint16_t)(base+u16(q+2)+d);ix[2]=(uint16_t)(base+u16(q+4)+d);}}
  ix_ni+=3*m;bq.bursts.b[0]=(bq.bursts.b[0]&0xFFFF0000UL)|ix_ni;
  vx_d3d_px[0]+=m;vx_d3d_px[1]+=hi-lo+1;vx_d3d_px[2]++;
  vx_d3d_batched+=m;vx_d3d_ix[0]+=m;vx_d3d_ix[1]+=hi-lo+1;if(vx_d3d_geo_on)vx_d3d_geo[6]+=m;
  k+=m;
 }
 return k;
}
/* .546: a triangle whose three vertices are valid (x y z finite, rhw positive and finite: vv_good) and all strictly past the same edge of
 * the clip rectangle (x < left, x > right, y < top, y > bottom: the edges are the packet's, right and bottom exclusive) covers no pixel the
 * engine may draw: the per-triangle path would clip it to nothing. Dropped here, as culled, instead of stopping the indexed loop (GTA3
 * draws with Direct3D's clipping off for 41% of its draws: its off-screen triangles reach the HAL). vx_d3d_off: [0] triangles dropped
 * so, [1] stops of the indexed loop for a vertex that failed (the triangle not wholly off the screen). QueryPhases 307-308. */
uint32_t vx_d3d_off[4]; /* .546: [2] tokens whose whole vertex range was proven inside at once (ix_list then checks no vertex), [3] tokens tried and not proven */
/* .546: the token's vertex range base .. base + its largest index (dp2_body sets them with ix_proof_new); ix_list proves it once, with the
 * span kernel, when the range is dense (at most 3 a triangle and 32 more): every vertex in it passes ix_check's tests, so none needs
 * checking as it enters a packet. The proof holds for the template it was made with (tc.id) */
/* (.547: ix_tok_*, ix_proof_* and ix_proof_new are in vx_d3d_span.inc, which ix_pass's span check reads) */
static int ix_off(const vx_d3d_input *in,uint32_t i0,uint32_t i1,uint32_t i2,const uint8_t *a,const uint8_t *b,const uint8_t *e) {
 float xa,xb,xe,ya,yb,ye;
 if(!vv_good(in,i0)||!vv_good(in,i1)||!vv_good(in,i2))return 0;
 xa=flt(u32(a));xb=flt(u32(b));xe=flt(u32(e));ya=flt(u32(a+4));yb=flt(u32(b+4));ye=flt(u32(e+4));
 {const float l=flt(tc.bxl),r=flt(tc.bxr),t=flt(tc.byt),bt=flt(tc.byb);
  return (xa<l&&xb<l&&xe<l)||(xa>r&&xb>r&&xe>r)||(ya<t&&yb<t&&ye<t)||(ya>bt&&yb>bt&&ye>bt);}
}
static uint32_t ix_list(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint8_t *q,uint32_t ist,uint32_t base,uint32_t n,uint32_t *completed,uint32_t *rc) {
 const uint8_t *const vb=in->vertices;const uint32_t st=in->vertex_stride,cull=c->state.rs[22];
 const uint32_t t0=vx_d3d_ftprof&&vx_d3d_phclock?vx_d3d_phclock():0;
 const int noarea=cull==1&&vx_d3d_ixpass; /* .515: nothing is culled (a triangle with no area draws nothing); .529: the engine's cull removed */
 uint32_t k=0,drawn=0,culled=0,newv=0,stop=0;
 int cur,allok;uint32_t resume=0xFFFFFFFFUL; /* .548: where the pass-through is tried again (after a sparse run) */
 *rc=0;
 if(ix_proof_state==0||ix_proof_tid!=tc.id){ix_proof_tid=tc.id; /* .546: the token's range proven inside once (vx_d3d_span.inc's kernel) */
  if(ix_tok_hi>=ix_tok_lo&&ix_tok_hi-ix_tok_lo+1<=3*ix_tok_n+32){ix_proof_state=vxs_span_ok(in,ix_tok_lo,ix_tok_hi)?1:2;vx_d3d_off[ix_proof_state==1?2:3]++;}else ix_proof_state=2;}
 allok=ix_proof_state==1;
 if((noarea||cull==2||cull==3)&&vx_d3d_ixpass){ixp_sparse=0;k=ix_pass(c,in,sv,q,ist,base,n,completed,rc); /* it counted its own triangles */ /* .567: a culled list too (vx_d3d_cullpass) */
  if(*rc||k>=n){vx_d3d_tk[8]+=k;if(t0){vx_d3d_ft[27]+=(vx_d3d_phclock()-t0+8)>>4;vx_d3d_ft[28]+=k;}return k;}
  q+=k*ist;if(ixp_sparse)resume=k+ixp_sparse;}
 /* the open packet's table answers only while it is this template's indexed packet on this vertex buffer with these cull bits: worked
  * out once, then changed only by ix_open below (nothing else here sends or begins a packet) */
 cur=bq_on&&bq_tid==tc.id&&bq.pre.on==2&&ix_vb==vb&&ix_stride==st&&ix_fvf==in->fvf;
 for(;k<n;k++,q+=ist) {
  if(k>=resume){uint32_t kp;resume=0xFFFFFFFFUL;ixp_sparse=0;vx_d3d_px_back++; /* .548: back to the pass-through after the sparse run */
   ixp_nonew=1;kp=ix_pass(c,in,sv,q,ist,base,n-k,completed,rc);ixp_nonew=0;k+=kp;q+=kp*ist;
   if(*rc||k>=n)break;
   if(ixp_sparse)resume=k+ixp_sparse;
   cur=bq_on&&bq_tid==tc.id&&bq.pre.on==2&&ix_vb==vb&&ix_stride==st&&ix_fvf==in->fvf;}
  {const uint32_t i0=base+u16(q),i1=base+u16(q+2),i2=base+u16(q+4);
  const uint8_t *const v0=vb+i0*st,*const v1=vb+i1*st,*const v2=vb+i2*st;
  uint32_t l0=IX_NONE,l1=IX_NONE,l2=IX_NONE;uint16_t *ix;
  if(cur){l0=IX_FIND(i0);l1=IX_FIND(i1);l2=IX_FIND(i2);}
  /* a vertex not in the packet is checked as it enters; one in it was checked with this template */
  if(!allok&&((l0==IX_NONE&&!ix_check(in,i0,v0))||(l1==IX_NONE&&!ix_check(in,i1,v1))||(l2==IX_NONE&&!ix_check(in,i2,v2)))){
   if(ix_off(in,i0,i1,i2,v0,v1,v2)){culled++;vx_d3d_off[0]++;continue;} /* .546: wholly past one clip edge: nothing to draw */
   stop=1;vx_d3d_off[1]++;break;}
  if(vx_d3d_geo_on){uint32_t t[3];t[0]=i0;t[1]=i1;t[2]=i2;geo_tri(in,t);}
  if(!noarea){double area;const vx_f32a *const f0=(const vx_f32a *)v0,*const f1=(const vx_f32a *)v1,*const f2=(const vx_f32a *)v2; /* loaded straight into the x87 (flt went through the stack) */
   area=((double)f1[0]-f0[0])*((double)f2[1]-f0[1])-((double)f1[1]-f0[1])*((double)f2[0]-f0[0]);
   /* zero area also takes every triangle with an index twice: below, the three indices are different */
   if(area==0||(cull==2&&area>0)||(cull==3&&area<0)){culled++;continue;}}
  else if(i0==i1||i1==i2||i0==i2){culled++;continue;} /* .515: an index twice: no area (the packet's tables map each source index once) */
  if(!cur||bq.vertex_count+3>VX3DN_IX_VERTICES||ix_ni+3>VX3DN_MAX_INDICES) {
   const uint32_t g=ix_g,o0=t0?vx_d3d_phclock():0;const int f=ix_open(c,in,sv,3,1,completed,rc);
   if(t0)vx_d3d_ft[31]+=(vx_d3d_phclock()-o0+8)>>4;
   if(f)break;
   if(ix_g!=g)l0=l1=l2=IX_NONE; /* a packet begun, or the tables started again: every vertex enters it */
   cur=1;if(ix_bb==0xFFFFFFFFUL){const uint32_t m=i0<i1?(i0<i2?i0:i2):(i1<i2?i1:i2);ix_bb=m>=IXD_N/2?m-IXD_N/2:0;}} /* v10.473: the window around
    * the packet's first triangle (v10.472 put it at the token's base vertex: with the runtime's indices into a large buffer past base +
    * 8,192, every lookup took the hash) */
  if(l0==IX_NONE){l0=bq.vertex_count++;IX_PUT(i0,l0);fast_vertex_pre_w(IXW(l0),v0,0xFFFFFFFFUL,v0);newv++;}
  if(l1==IX_NONE){l1=bq.vertex_count++;IX_PUT(i1,l1);fast_vertex_pre_w(IXW(l1),v1,0xFFFFFFFFUL,v1);newv++;}
  if(l2==IX_NONE){l2=bq.vertex_count++;IX_PUT(i2,l2);fast_vertex_pre_w(IXW(l2),v2,0xFFFFFFFFUL,v2);newv++;}
  ix=VX3DN_IDX(&bq)+ix_ni;ix[0]=(uint16_t)l0;ix[1]=(uint16_t)l1;ix[2]=(uint16_t)l2;
  ix_ni+=3;bq.bursts.b[0]=(bq.bursts.b[0]&0xFFFF0000UL)|ix_ni;drawn++;}
 }
 vx_d3d_batched+=drawn;vx_d3d_ix[0]+=drawn;vx_d3d_ix[1]+=newv;vx_d3d_tk[8]+=k;vx_d3d_tk[9]+=culled;vx_d3d_tk[10]+=stop;
 if(vx_d3d_geo_on){vx_d3d_geo[6]+=drawn;vx_d3d_geo[3]+=culled;}
 if(t0){vx_d3d_ft[27]+=(vx_d3d_phclock()-t0+8)>>4;vx_d3d_ft[28]+=k;vx_d3d_ft[29]+=stop;vx_d3d_ft[30]+=newv;}
 return k;
}
/* v10.238: a triangle token's vertex indices, a chunk at a time, with the opcode's switch outside the loop (they were worked out by
 * indices() per triangle, twice: once to check, once to draw); the same triangles as indices() */
#define VX_TRI_CHUNK 128
static uint32_t tri3[VX_TRI_CHUNK*3];
static void tri_fill(uint32_t op,const uint8_t *p,uint32_t base,uint32_t j0,uint32_t m) {
 uint32_t k,i,*t=tri3;const uint8_t *q;
 switch(op) {
  case 18:for(k=0,i=base+j0*3;k<m;k++,t+=3,i+=3){t[0]=i;t[1]=i+1;t[2]=i+2;}break;
  case 19:for(k=0,i=j0;k<m;k++,t+=3,i++){t[0]=base+i;if(i&1){t[1]=base+i+2;t[2]=base+i+1;}else{t[1]=base+i+1;t[2]=base+i+2;}}break;
  case 21:for(k=0,i=j0;k<m;k++,t+=3,i++){t[0]=base+i+1;t[1]=base+i+2;t[2]=base;}break;
  case 3:for(k=0,q=p+j0*8;k<m;k++,t+=3,q+=8){t[0]=base+u16(q);t[1]=base+u16(q+2);t[2]=base+u16(q+4);}break;
  case 26:for(k=0,q=p+j0*6;k<m;k++,t+=3,q+=6){t[0]=base+u16(q);t[1]=base+u16(q+2);t[2]=base+u16(q+4);}break;
  case 20:for(k=0,i=j0,q=p+j0*2;k<m;k++,t+=3,i++,q+=2){t[0]=base+u16(q);if(i&1){t[1]=base+u16(q+4);t[2]=base+u16(q+2);}else{t[1]=base+u16(q+2);t[2]=base+u16(q+4);}}break;
  default:{uint32_t c0=base+u16(p);for(k=0,q=p+(j0+1)*2;k<m;k++,t+=3,q+=2){t[0]=base+u16(q);t[1]=base+u16(q+2);t[2]=c0;}}break;
 }
}
#include "vx_d3d_tnl.inc" /* .536: the driver's transform and lighting (ACCEL_D3D_TNL) */
/* .536: for a draw transformed here, each active stage's TEXCOORDINDEX set to its own output set and TEXTURETRANSFORMFLAGS to 0 (set 1),
 * then put back (set 0: at the next command, after the loop, and on a failure) */
static uint32_t tn_save[8][2];static int tn_ov;
static void tn_tss(vx_d3d_context *c,int set) {uint32_t k;
 if(set&&!tn_ov){for(k=0;k<8;k++){tn_save[k][0]=c->state.tss[k][11];tn_save[k][1]=c->state.tss[k][24];c->state.tss[k][11]=k<tn_sets?k:tn_sets-1;c->state.tss[k][24]=0;}tn_ov=1;ep_key=tn_sets+1;} /* .552: ep_key (ep, above) */
 else if(!set&&tn_ov){for(k=0;k<8;k++){c->state.tss[k][11]=tn_save[k][0];c->state.tss[k][24]=tn_save[k][1];}tn_ov=0;ep_key=0;}}
static uint32_t dp2_body(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,uint32_t *error);
uint32_t vx_d3d_dp2(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,uint32_t *error) {
 uint32_t o=VX_PH(VX_PH_CMD),rc=dp2_body(c,in,sv,error);VX_PH_BACK(o);return rc;
}
static uint32_t dp2_body(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,uint32_t *error) {
 uint32_t pos=0,rc=0,bound=0,completed=0,lastcmd=0,tlp_n=0,tn_on=0,tn_bound=0;vx_d3d_input source,tn_src; /* .534: tlp_n the untransformed draws in this call; .536: tn_on a call of untransformed vertices (tn_src, bound once), drawn from the scratch buffer */
 if(!error)return VX_D3D_INVALID;*error=0;
 vx_d3d_dp2_calls++;bq_on=0;fail_set=0;cur_cmd=0;if(c&&sv&&cq_held){uint32_t r=cq_take(sv,c);if(r){cq_clear();return r;}}else cq_clear();template_reset(); /* .550: cq_take */vv_fresh=0;rhw0_fix=0;geo_call(); /* v10.440; .531: rhw0_fix */
 if(!c||!in||!sv||!sv->submit||(!in->commands&&in->command_bytes))return VX_D3D_INVALID;
 source=*in;in=&source;
 if(c->lost)return VX_D3D_SURFACELOST;
 if(vx_d3d_tnl&&(source.fvf&0xEUL)&&(source.fvf&0xEUL)!=4) { /* .536: the draws read the scratch buffer (vx_d3d_tnl.inc); the input is bound for the transform */
  tn_src=source;tn_on=1;tn_P=0;bound=1;source.vertices=tn_buf;source.fvf=0x1C4UL|0x100UL;source.vertex_stride=32;source.vertex_count=TN_MAX;source.vertex_bytes=sizeof(tn_buf);
 }
 while(pos<in->command_bytes) {
  uint32_t start=pos,op,n,len=0,j,base=0,a,b,d,imm=0;const uint8_t *p;
  if(tn_ov)tn_tss(c,0); /* .536: the stages as the application set them, before each command */
  tn_lazy=0; /* .541: a T&L draw's unfinished vertices are its own command's */
  lastcmd=start;cur_cmd=start;
  if(in->command_bytes-pos<4){rc=VX_D3D_INVALID;goto fail;}
  op=in->commands[pos];n=u16(in->commands+pos+2);pos+=4;p=in->commands+pos;ck_switch(op); /* v10.390 */
  /* bReserved belongs to the runtime and is deliberately ignored. */
  if(op==8||op==25||op==41)len=n*8;
  else if(op==28)len=n*16;
  else if(op==29||op==32)len=n*8;
  else if(op==39)len=n*12;
  else if(op==42)len=16+n*16; /* v10.372 (XP's S3gIGP.dll 0x39561, static RE v5: the command advances 20 + 16 n bytes, the 4-byte
   * header included): a zero-rectangle clear (D3DCLEAR_COMPUTERECTS, the viewport) is 16 bytes; it was read as 32 (RECT[1] reserved) */
  else if(op==18||op==19||op==21)len=2;
  else if(op==3)len=n*8;
  else if(op==26)len=2+n*6;
  else if(op==20||op==22)len=2+(n+2)*2;
  /* v10.189: TRIANGLEFAN_IMM (23): D3DHAL_DP2TRIANGLEFAN_IMM (edge flags), then n+2 vertices of the current FVF inline (the runtime's
   * clipped polygons; D3DIM.DLL sends them for DirectX 3/5 execute buffers, Tunnel). LINELIST_IMM (24): 2n inline vertices. Points and
   * lines (1 POINTS, 2 INDEXEDLINELIST, 15 LINELIST, 16 LINESTRIP, 17 INDEXEDLINESTRIP, 27 INDEXEDLINELIST2) are drawn since v10.201. */
  /* v10.219: the inline vertices start on the next DWORD boundary (DDK reference rasterizer: (pCmd + 1 [+ the edge flags] + 3) & ~3).
   * The runtime puts an _IMM command straight after INDEXEDTRIANGLELIST2 data, which ends on a 2-byte boundary when its count is even:
   * the vertices were read 2 bytes early (board, VXDISCO v1.2: every clipped wall polygon of WinTune's scene, offset = 2 mod 4, failed with
   * DDERR_INVALIDPARAMS and the rest of its batch with it; the garbage that did pass gave a NaN vertex the VxD refused, which lost the
   * device two frames into WinTune). imm = the vertices' offset from p. */
  else if(op==23||op==24) {
   uint32_t s=source.vertex_stride?source.vertex_stride:vx_d3d_vertex_size(source.fvf),k=op==23?n+2:n*2;
   if(!s||!vx_d3d_vertex_size(source.fvf)||s<vx_d3d_vertex_size(source.fvf)||k>(0xFFFFFFFFUL-8)/s){rc=VX_D3D_INVALID;goto fail;}
   imm=op==23?4:0;imm+=(uint32_t)((4-(((uintptr_t)(in->commands+pos)+imm)&3))&3);
   len=imm+k*s;
  }
  else if(op==1||op==2)len=n*4;
  else if(op==15||op==16)len=2;
  else if(op==17)len=2+(n+1)*2;
  else if(op==27)len=2+n*4;
  else if(vx_d3d_tnl&&(op==33||op==36))len=n*68; /* .534: D3DMATERIAL7; D3DHAL_DP2SETTRANSFORM (type, matrix) */
  else if(vx_d3d_tnl&&op==35)len=n*4; /* D3DHAL_DP2CREATELIGHT */
  else if(vx_d3d_tnl&&op==44)len=n*20; /* index, plane[4] */
  else if(vx_d3d_tnl&&op==34) { /* D3DHAL_DP2SETLIGHT (index, data type), then a D3DLIGHT7 (104 bytes) for D3DHAL_SETLIGHT_DATA (2) */
   /* .540 (review): the room left checked before each read (after a 112-byte entry, command_bytes - pos - len wrapped below 0 and the
    * next entry's type was read past the buffer) */
   const uint32_t room=in->command_bytes-pos;
   for(j=0;j<n;j++){if(len>room||room-len<8){rc=VX_D3D_INVALID;goto fail;}
    if(u32(p+len+4)==2){if(room-len<112){rc=VX_D3D_INVALID;goto fail;}vx_d3d_tlp[7]++;len+=112;}else len+=8;}
  }
  else {
   const uint8_t *next=0;
   if(sv->unknown) {
    rc=vx_d3d_flush(c,sv,&completed);if(!rc)rc=cq_flush(sv,c,&completed);if(rc)goto fail; /* v10.215: a command the runtime parses can change what the draws read; v10.276: the queue too */
    rc=sv->unknown(sv->context,in->commands+start,&next);
    if(!rc&&(uintptr_t)next>(uintptr_t)(in->commands+start)&&(uintptr_t)next<=(uintptr_t)(in->commands+in->command_bytes)){pos=(uint32_t)((uintptr_t)next-(uintptr_t)in->commands);template_reset();continue;} /* v10.330 (review #89): the template is built again after a command the runtime parsed (it may have changed what draws read) */
   }
   rc=VX_D3D_UNPARSED;goto fail;
  }
  if(len>in->command_bytes-pos){rc=VX_D3D_INVALID;goto fail;}
  if(vx_d3d_tnl) { /* .534: the T&L commands counted; .536: kept (vx_d3d_tnl.inc), and the draws of untransformed vertices transformed */
   if(op==33||op==34||op==35||op==36||op==44){vx_d3d_tlp[op==44?6:op-31]+=n;if(!c->recording)tnl_command(c,op,p,n);pos+=len;continue;}
   if(tn_on&&tlp_draw(op)){int r;
    if(!tlp_n){vx_d3d_tlp[0]++;tlp_fvf(tn_src.fvf);}tlp_n++;vx_d3d_tlp[1]++;vx_d3d_tlp[17]+=n;
    if(n&&!tn_bound&&sv->bind_vertices){rc=sv->bind_vertices(sv->context,&tn_src);if(rc)goto fail;tn_bound=1;}
    r=tn_draw(c,&tn_src,&op,&n,&p);
    if(r==2){rc=vx_d3d_flush(c,sv,&completed);if(!rc)rc=cq_flush(sv,c,&completed);if(rc)goto fail; /* the scratch buffer full: the batch sent, the buffer from the start */
     tn_P=0;vv_fresh=0;ix_vb=0;vx_d3d_tn[6]++;r=tn_draw(c,&tn_src,&op,&n,&p);}
    if(r!=1){pos+=len;continue;}
    if(source.fvf!=tn_fvf||source.vertex_stride!=tn_ostride){source.fvf=tn_fvf;source.vertex_stride=tn_ostride;vv_fresh=0;}
    tn_tss(c,1);
   }
  }
  /* v10.214: the batch is sent before a render-target change or a clear; v10.215: and before every state command (render state, texture
   * stage state, viewport, state block), so a batch never spans a state change: a WASSTILLDRAWING retry from the batch's first command
   * replays draws only, under the state they were made with (review: a retry redrew earlier triangles with later states) */
  if((op==8||op==25||op==28)&&state_same(c,op,p,n)){} /* v10.309 (review): values all as they are: no flush, the template stands */
  else if(op==25&&tr_defer(c,sv)){template_reset();} /* .528: texture runs: the batch kept open over a texture stage state (tr_defer) */
  else if(op==8||op==25||op==28||op==39||op==41||op==42){template_reset();rc=vx_d3d_flush(c,sv,&completed);if(rc)goto fail;
   if(op==39||op==41){rc=cq_flush(sv,c,&completed);if(rc)goto fail;}else if(op!=42)cq_keep_state(c);} /* v10.279 */
  /* v10.330 (review #166): a render-state, texture-stage or viewport token is checked whole before any entry is applied (a bad later
   * entry left the earlier ones applied and recorded into the state block) */
  if(op==8){for(j=0;j<n;j++)if(u32(p+j*8)>=256){rc=VX_D3D_INVALID;goto fail;}}
  else if(op==25){for(j=0;j<n;j++)if(u16(p+j*8)>=8||u16(p+j*8+2)>=32){rc=VX_D3D_INVALID;goto fail;}}
  else if(op==28){for(j=0;j<n;j++){uint32_t x=u32(p+j*16),y=u32(p+j*16+4),w=u32(p+j*16+8),h=u32(p+j*16+12);
   if(x>c->target.width||y>c->target.height||w>c->target.width-x||h>c->target.height-y){rc=VX_D3D_INVALID;goto fail;}}}
  if(op==8)for(j=0;j<n;j++) {
   uint32_t id=u32(p+j*8),value=u32(p+j*8+4);
   if(id>=256){rc=VX_D3D_INVALID;goto fail;}
   if(id==VX_D3D_RS_LOGICOP)continue; /* v10.295: the ICD's, not a Direct3D state */
   if(vx_d3d_rs_hook)vx_d3d_rs_hook(id,value,start); /* v10.420 */
   state(c)->rs[id]=value;
   if(c->recording)c->blocks[c->recording-1].rs_mask[id/32]|=1UL<<(id%32);
   /* v10.185: D3DRENDERSTATE_TEXTUREHANDLE (1) is stage 0's D3DTSS_TEXTUREMAP under another name (DDK: setting one sets the other); for a
    * DirectX 7 driver the value is a CreateSurfaceEx handle. It was refused as unsupported, which left out every textured DirectX 3/5 draw. */
   if(id==1){state(c)->tss[0][0]=value;if(c->recording)c->blocks[c->recording-1].tss_mask[0]|=1UL;}
   if((in->flags&2)&&in->render_states&&id<in->render_state_count)in->render_states[id]=value;
  } else if(op==25)for(j=0;j<n;j++) {
   uint32_t stage=u16(p+j*8),id=u16(p+j*8+2);
   if(stage>=8||id>=32){rc=VX_D3D_INVALID;goto fail;}
   if(!stage&&id>=12&&id<=14){uint32_t k2=vx_d3d_addr_count%16;vx_d3d_tss_seen[id-12]=u32(p+j*8+4);vx_d3d_tss_seen[3]|=1UL<<(id-12);
    vx_d3d_addr_ring[k2][0]=(vx_d3d_dp2_calls<<16)|id;vx_d3d_addr_ring[k2][1]=u32(p+j*8+4);vx_d3d_addr_count++;} /* v10.174/175: what the runtime sends for ADDRESS/U/V, in order */
   state(c)->tss[stage][id]=u32(p+j*8+4);
   if(c->recording)c->blocks[c->recording-1].tss_mask[stage]|=1UL<<id;
   if(!stage&&!id){state(c)->rs[1]=state(c)->tss[0][0];if(c->recording)c->blocks[c->recording-1].rs_mask[0]|=2UL;} /* v10.185: the same texture as TEXTUREHANDLE */
   /* v10.170: D3DTSS_ADDRESS (12) sets both ADDRESSU (13) and ADDRESSV (14); the runtime passes it through unsplit (board, VXDX7 --texture v10.169:
    * WRAP and MIRROR set this way drew as CLAMP) */
   if(id==12){state(c)->tss[stage][13]=state(c)->tss[stage][14]=state(c)->tss[stage][12];if(c->recording)c->blocks[c->recording-1].tss_mask[stage]|=(1UL<<13)|(1UL<<14);}
  } else if(op==28)for(j=0;j<n;j++) {
   uint32_t x=u32(p+j*16),y=u32(p+j*16+4),w=u32(p+j*16+8),h=u32(p+j*16+12);
   if(x>c->target.width||y>c->target.height||w>c->target.width-x||h>c->target.height-y){rc=VX_D3D_INVALID;goto fail;}
   state(c)->viewport.left=(int32_t)x;state(c)->viewport.top=(int32_t)y;
   state(c)->viewport.right=(int32_t)(x+w);state(c)->viewport.bottom=(int32_t)(y+h);
   if(c->recording)c->blocks[c->recording-1].viewport_mask=1;
  } else if(op==29||op==32) {
   /* Check the whole payload before updating the T&L depth range. */
   for(j=0;j<n*2;j++)if(!finite(u32(p+j*4))){rc=VX_D3D_INVALID;goto fail;}
   if(op==32&&vx_d3d_tnl)tnl_command(c,op,p,n);
  } else if(op==39)for(j=0;j<n;j++){rc=stateset(c,p+j*12);if(rc)goto fail;}
  else if(op==41)for(j=0;j<n;j++) {
   vx3dn_surface t,z;uint32_t hz=u32(p+j*8+4);
   memset(&z,0,sizeof(z));
   if(c->recording||!sv->resolve||!(sv->target?sv->target(sv->context,u32(p+j*8),&t):sv->resolve(sv->context,u32(p+j*8),&t))|| /* v10.456: sv->target */
      (hz&&!sv->resolve(sv->context,hz,&z))){rc=VX_D3D_INVALID;goto fail;}
   if(!vx_d3d_targets_valid(&t,&z)){rc=VX_D3D_INVALID;goto fail;}
   c->target=t;c->depth=z;c->target_handle=u32(p+j*8);c->depth_handle=hz;
   if(sv->target_done)sv->target_done(sv->context); /* v10.479 */
  } else if(op==42) {
   static vx3dn_packet q; /* v10.237: static (was a 9272-byte local: vx_d3d_dp2's frame stepped over Win98's one-page stack guard; every caller holds callback_lock) */
   uint32_t flags=u32(p),color=u32(p+4);float z=0;
   if(c->recording||(flags&~0xFUL)) {rc=VX_D3D_UNSUPPORTED;goto fail;} /* v10.179: + D3DCLEAR_STENCIL (4) */
   if((flags&4)&&c->depth.format!=VX3DN_D24S8)flags&=~4UL; /* no stencil storage: nothing to clear (the runtime checks the format) */
   /* v10.372 (XP's S3gIGP.dll 0x357AE-0x357E8, static RE v5): the depth and stencil bits go on only when a depth surface is bound;
    * without one they are dropped (the colour clear still runs). A depth clear with no depth surface failed the whole call. */
   if(!c->depth.format)flags&=~6UL;
   /* The depth payload is irrelevant to a color-only clear. Do not reject
    * it, or convert an unused NaN to an integer. v10.372: a finite depth outside 0..1 is clamped (XP passes the raw value on) */
   if(flags&2) {
    z=flt(u32(p+8));
    if(!finite(u32(p+8))){rc=VX_D3D_UNSUPPORTED;goto fail;}
    if(!(z>0))z=0;if(z>1)z=1;
   }
   packet(&q,c,VX3DN_CLEAR);q.clear_flags=(flags&7)|((flags&4)?(u32(p+12)&255UL)<<8:0);q.clear_color=color;q.clear_depth=(flags&2)?(uint32_t)((double)z*4294967295.0+0.5):0;
   if(!VX3DN_CLEAR_WHAT(q.clear_flags)){pos+=len;continue;}
   if(!n&&(flags&8)){rc=submit(sv,&q,c,&completed);if(rc)goto fail;}
   /* v10.330 (review #164): every rectangle is checked (pass 0) before the first is cleared (pass 1): a bad second rectangle left the
    * first cleared and the token refused */
   {uint32_t pass;for(pass=0;pass<2;pass++)for(j=0;j<n;j++) {
    int32_t l=(int32_t)u32(p+16+j*16),t=(int32_t)u32(p+20+j*16),r=(int32_t)u32(p+24+j*16),b2=(int32_t)u32(p+28+j*16);
    if(r<l||b2<t){rc=VX_D3D_INVALID;goto fail;}
    if(flags&8){if(l<c->state.viewport.left)l=c->state.viewport.left;if(t<c->state.viewport.top)t=c->state.viewport.top;
      if(r>c->state.viewport.right)r=c->state.viewport.right;if(b2>c->state.viewport.bottom)b2=c->state.viewport.bottom;}
    /* v10.372 (XP's S3gIGP.dll 0x358B4-0x358C5): a rectangle past the surface is clamped to it (it failed the whole call) */
    if(l<0)l=0;if(t<0)t=0;if(r>(int32_t)c->target.width)r=(int32_t)c->target.width;if(b2>(int32_t)c->target.height)b2=(int32_t)c->target.height;
    if(r<=l||b2<=t||!pass)continue;
    q.clip.left=l;q.clip.top=t;q.clip.right=r;q.clip.bottom=b2;rc=submit(sv,&q,c,&completed);if(rc)goto fail;
   }}
  } else if(op==1||op==2||op==15||op==16||op==17||op==24||op==27) {
   /* v10.201: points and lines (CLXP3D T62-T64): 1 POINTS {count, start} x n; 2 INDEXEDLINELIST {a, b} x n; 15 LINELIST start, n lines;
    * 16 LINESTRIP start, n lines; 17 INDEXEDLINESTRIP base, n + 1 indices; 24 LINELIST_IMM 2n inline vertices; 27 INDEXEDLINELIST2 base,
    * {a, b} x n. Every vertex is checked before the first is drawn. */
   vx_d3d_input im=source;const vx_d3d_input *vin=in;uint32_t pass,iv[2];
   if(c->recording){rc=VX_D3D_UNSUPPORTED;goto fail;}
   if(!ep_supported(c)||(op==1&&!(vx_d3d_feat&VX_F_POINTS))){drop(c);pos+=len;continue;} /* v10.212: ACCEL_D3D_POINTS = 0 leaves points out */
   if(op==24){uint32_t s2=source.vertex_stride?source.vertex_stride:vx_d3d_vertex_size(source.fvf);im.vertices=p+imm;im.vertex_stride=s2;im.vertex_count=n*2;im.vertex_bytes=n*2*s2;vin=&im;}
   else if(n&&!bound&&sv->bind_vertices){uint32_t o=VX_PH(VX_PH_BUF);rc=sv->bind_vertices(sv->context,&source);VX_PH_BACK(o);if(rc)goto fail;bound=1;}
   for(pass=0;pass<2;pass++) {
    if(op==1) {
     for(j=0;j<n;j++){uint32_t cnt=u16(p+j*4),st=u16(p+j*4+2),q; /* v10.204: D3DHAL_DP2POINTS is {wCount, wVStart} (it was read reversed) */
      for(q=0;q<cnt;q++){iv[0]=st+q;if(!pass){if(valid_vertex(vin,iv[0])==VX_D3D_INVALID){rc=VX_D3D_INVALID;goto fail;}}else{if(bad_prim(c,valid_vertex(vin,iv[0]),0,0,1))continue;rc=primitive(c,vin,sv,iv,1,&completed);if(rc)goto fail;}}}
    } else for(j=0;j<n;j++) {
     switch(op) {
      case 2:iv[0]=u16(p+j*4);iv[1]=u16(p+j*4+2);break;
      case 15:iv[0]=u16(p)+j*2;iv[1]=iv[0]+1;break;
      case 16:iv[0]=u16(p)+j;iv[1]=iv[0]+1;break;
      case 17:iv[0]=u16(p)+u16(p+2+j*2);iv[1]=u16(p)+u16(p+4+j*2);break;
      case 24:iv[0]=j*2;iv[1]=j*2+1;break;
      default:iv[0]=u16(p)+u16(p+2+j*4);iv[1]=u16(p)+u16(p+4+j*4);break;
     }
     if(!pass){if(valid_vertex(vin,iv[0])==VX_D3D_INVALID||valid_vertex(vin,iv[1])==VX_D3D_INVALID){rc=VX_D3D_INVALID;goto fail;}}
     else {if(bad_prim(c,valid_vertex(vin,iv[0]),valid_vertex(vin,iv[1]),0,2))continue;rc=primitive(c,vin,sv,iv,2,&completed);if(rc)goto fail;}
    }
   }
  } else if(op==23) {
   vx_d3d_input im=source;uint32_t s=source.vertex_stride?source.vertex_stride:vx_d3d_vertex_size(source.fvf);
   if(c->recording){rc=VX_D3D_UNSUPPORTED;goto fail;}
   if(!ep_supported(c)){drop(c);pos+=len;continue;}
   im.vertices=p+imm;im.vertex_stride=s;im.vertex_count=n+2;im.vertex_bytes=(n+2)*s;vx_d3d_tk[7]+=n; /* v10.473 */
   /* .555: the fan passed through whole. The runtime's clipper sends each clipped triangle as one of these (g.rec: 1,210 triangles a
    * frame); each vertex went through valid_vertex and each triangle through primitive, fast_triangle and ix_tri's tables (about 600
    * instructions a triangle). When the template is ready for this vertex format and takes the fast path into indexed packets, the
    * fill is solid and nothing is culled, and every vertex passes fast_triangle's tests (vxs_imm_ok), the n + 2 vertices are converted
    * as one span and the triangles added by index in the old order (j + 1, j + 2, 0). A triangle with no area is sent (it draws
    * nothing; fast_triangle left it out). Anything else takes the loop below as before. vx_d3d_fan: [0] fans through here, [1] their
    * triangles, [2] fans left to the loop. */
   if(n&&n+2<=VX3DN_IX_VERTICES&&vx_d3d_indexed&&vx_d3d_ixpass&&!tn_lazy&&tc.valid&&tc.ctx==c&&!tc.err&&!tc.skip&&tc.lay.fvf==im.fvf&&tc.pre&&
      (vx_d3d_feat&VX_F_BATCH)&&!FLATSW(c)&&c->state.rs[8]==3&&(c->state.rs[22]==1||c->state.rs[22]==2||c->state.rs[22]==3)&&vxs_imm_ok(&im,n+2)){
    uint32_t d0;uint16_t *ix;
    /* .567: with a cull mode too. A clipped triangle's fan has one winding: its area is taken once, over the whole
     * polygon in the fan's order (j + 1, j + 2, 0), each triangle's by the loop's expression; a fan whose triangles are all culled
     * (or have no area) is left out whole, one with any triangle kept and any culled goes to the loop. */
    if(c->state.rs[22]!=1){const uint32_t cm=c->state.rs[22];uint32_t nc=0;const vx_f32a *const f2=(const vx_f32a *)(im.vertices);
     for(j=0;j<n;j++){const vx_f32a *const f0=(const vx_f32a *)(im.vertices+(j+1)*s),*const f1=(const vx_f32a *)(im.vertices+(j+2)*s);
      const double area=((double)f1[0]-f0[0])*((double)f2[1]-f0[1])-((double)f1[1]-f0[1])*((double)f2[0]-f0[0]);
      if(area==0||(cm==2&&area>0)||(cm==3&&area<0))nc++;}
     if(nc==n){vx_d3d_tk[9]+=n;vx_d3d_cp[1]+=n;if(vx_d3d_geo_on){vx_d3d_geo[0]+=n;vx_d3d_geo[3]+=n;}vx_d3d_fan[0]++;pos+=len;continue;}
     if(nc)goto fan_loop;}
    ft_sm=0;if(ix_open(c,&im,sv,n+2,n,&completed,&rc))goto fail;
    d0=bq.vertex_count;vxs_span_conv(&im,0,n+1,d0);bq.vertex_count+=n+2;
    ix=VX3DN_IDX(&bq)+ix_ni;for(j=0;j<n;j++,ix+=3){ix[0]=(uint16_t)(d0+j+1);ix[1]=(uint16_t)(d0+j+2);ix[2]=(uint16_t)d0;}
    ix_ni+=3*n;bq.bursts.b[0]=(bq.bursts.b[0]&0xFFFF0000UL)|ix_ni;
    if(vx_d3d_geo_on){vx_d3d_geo[0]+=n;vx_d3d_geo[1]+=n+2;vx_d3d_geo[6]+=n;}
    vx_d3d_batched+=n;vx_d3d_ix[0]+=n;vx_d3d_ix[1]+=n+2;vx_d3d_fan[0]++;vx_d3d_fan[1]+=n;
    pos+=len;continue;}
   fan_loop:
   vx_d3d_fan[2]++;
   {uint32_t badv=0;for(j=0;j<n+2;j++){uint32_t r=valid_vertex(&im,j);if(r==VX_D3D_INVALID){rc=VX_D3D_INVALID;goto fail;}if(r)badv=1;}
   for(j=0;j<n;j++){uint32_t ef=u32(p),bit;indices(21,p,0,j,&a,&b,&d);
    if(badv&&bad_prim(c,valid_vertex(&im,a),valid_vertex(&im,b),valid_vertex(&im,d),3))continue; /* v10.367; .531: bad_prim */
    /* v10.296: the polygon's dwEdgeFlags, bit k the edge from vertex k to k + 1 (the last, back to 0): triangle j (j+1, j+2, 0) has edge
     * j+1, the closing edge when it is the last, edge 0 when it is the first; a bit past 31 is taken as set */
    bit=j+1;prim_edges=(bit>31||((ef>>bit)&1))?1UL:0;
    if(j==n-1){bit=n+1;if(bit>31||((ef>>bit)&1))prim_edges|=2;}
    if(j==0&&(ef&1))prim_edges|=4;
    rc=triangle(c,&im,sv,a,b,d,&completed);prim_edges=7;if(rc)goto fail;}}
  } else {
   if(c->recording){rc=VX_D3D_UNSUPPORTED;goto fail;}
   if(!ep_supported(c)){drop(c);pos+=len;continue;}
   /* MS D3dDrawPrimitives2: locate external vertex data only when an
    * actual vertex-consuming command is encountered, never for state-only
    * calls that may carry an unused/stale vertex union. */
   if(n&&!bound&&sv->bind_vertices){uint32_t o=VX_PH(VX_PH_BUF);rc=sv->bind_vertices(sv->context,&source);VX_PH_BACK(o);if(rc)goto fail;bound=1;}
   if(op==18||op==19||op==20||op==21||op==22||op==26){base=u16(p);p+=2;}
   if(op==3||op==26||(op>=18&&op<=22))vx_d3d_tk[op==3?0:op==26?6:op-17]+=n; /* v10.473 */
   /* Validate the WHOLE primitive token before issuing its first draw. v10.222: a vertex checked once in this call is not checked again
    * (a grid's vertices are shared by six triangles): vv_ok marks checked indices of the bound vertex buffer. */
   if(!vv_fresh&&n){if(vv_blo<=vv_bhi)memset(vv_ok+vv_blo,0,vv_bhi-vv_blo+1);vv_blo=0xFFFFFFFFUL;vv_bhi=0;vv_fresh=1;vv_limit(in);} /* .535: the bytes set since the last clear */ /* v10.237: clamped before rounding (no wrap); v10.454: vv_lim */
   /* v10.238: the indices a chunk at a time (tri_fill); a cached check is tested in the loop; a triangle goes to fast_triangle directly
    * while the template is ready for this FVF (primitive()'s own checks), else through primitive() */
   {uint32_t m,k,vc=in->vertex_count;int fast=-1;
    uint32_t badv=0,ph=VX_PH(VX_PH_VERT); /* v10.431 */
    /* v10.472: an indexed triangle list (26, 3) while the HAL makes indexed packets: no check pass over every index; when the token's
     * largest index is below vv_lim (no index can make the call fail), its triangles go through ix_list, which checks each vertex once
     * as it enters a packet, and the per-triangle path (with every triangle's vertices checked: badv) takes the ones it stops at */
    int ixl=0;const uint8_t *lp=p;uint32_t ln=n; /* .555: lp, ln: the triangles' index triples (the token's own, or ixs_tri) */
    /* .555: an indexed strip (20) as the indexed list it draws. Its triangles went one by one through tri_fill, the vertex check loop,
     * fast_triangle and ix_tri (g.rec: 1,101 a frame, about 600 instructions each against about 100 for a list's through ix_list).
     * Written out as index triples in tri_fill's order (an odd triangle's last two swapped), it takes the indexed list's path below:
     * ix_list's spans, its per-triangle loop with the cull on the same doubles, and for a triangle it stops at the per-triangle path
     * as before. A triangle with an index twice (a strip's stitches: 56% of g.rec's strip triangles) has no area and is left out
     * here, as fast_triangle left it out. A strip of more than IXS_MAX triangles, or with an index at or past vv_lim, takes the old
     * path. vx_d3d_ixs: [0] strips through here, [1] their triangles, [2] strips left to the old path, [3] triangles left out here. */
    if(op==20&&vx_d3d_indexed&&n&&vv_lim){
     const uint32_t mx=n<=IXS_MAX?VXS_MAX16(p,n+2):0xFFFFFFFFUL;
     if(mx!=0xFFFFFFFFUL&&base+mx<vv_lim){uint32_t i;uint16_t *t=ixs_tri;const uint8_t *q=p;
      for(i=0;i<n;i++,q+=2){const uint32_t a2=u16(q),b2=u16(q+2),d2=u16(q+4);
       if(a2==b2||b2==d2||a2==d2)continue;
       t[0]=(uint16_t)a2;if(i&1){t[1]=(uint16_t)d2;t[2]=(uint16_t)b2;}else{t[1]=(uint16_t)b2;t[2]=(uint16_t)d2;}t+=3;}
      ln=(uint32_t)(t-ixs_tri)/3;lp=(const uint8_t*)ixs_tri;ixl=1;ix_proof_new(base,base+mx,ln);vx_d3d_ixs[0]++;vx_d3d_ixs[1]+=n;vx_d3d_ixs[3]+=n-ln;}
     else vx_d3d_ixs[2]++;}
    if((op==26||op==3)&&vx_d3d_indexed&&n&&vv_lim){const uint32_t ist=op==3?8:6;uint32_t mx=0;const uint8_t *q=p;
     if(op==26)mx=VXS_MAX16(q,n*3); /* .545: vx_d3d_span.inc (the indices back to back) */
     else for(k=0;k<n;k++,q+=ist){const uint32_t a2=u16(q),b2=u16(q+2),d2=u16(q+4);if(a2>mx)mx=a2;if(b2>mx)mx=b2;if(d2>mx)mx=d2;}
     ixl=base+mx<vv_lim;if(ixl)ix_proof_new(base,base+mx,n);} /* .546: the range the indexed loop may prove at once */
    if(tn_lazy&&!ixl)tn_fin_all(); /* .541: the driver's T&L draw on a path that reads the records whole */
    if((op==26||op==3)&&n&&!ixl)vx_d3d_tk[vx_d3d_indexed?13:12]+=n;
    if(ixl){const uint32_t ist=op==3?8:6;
     VX_PH(VX_PH_ASM);badv=1;ft_sm=0;
     for(j=0;j<ln;) {
      uint32_t t[3];const uint8_t *q;
      if(fast<0){if((!tc.valid||tc.ctx!=c||tc.lay.fvf!=in->fvf)&&!FLATSW(c)&&c->state.rs[8]==3){VX_PH(VX_PH_VERT);rc=tc_ready(c,in,sv);VX_PH(VX_PH_ASM);if(rc)goto fail;} /* v10.475 */
       fast=tc.valid&&tc.ctx==c&&!tc.err&&!tc.skip&&tc.lay.fvf==in->fvf&&!FLATSW(c)&&c->state.rs[8]==3;}
      if(fast&&tc.pre&&(vx_d3d_feat&VX_F_BATCH)){j+=tn_lazy?tn_ix(c,in,sv,lp+j*ist,ist,base,ln-j,&completed,&rc):ix_list(c,in,sv,lp+j*ist,ist,base,ln-j,&completed,&rc);if(rc)goto fail;if(j>=ln)break;} /* .541: tn_ix, the T&L's fused path */
      else vx_d3d_tk[fast?15:14]++; /* v10.474: why this one is not through the loop */
      q=lp+j*ist;t[0]=base+u16(q);t[1]=base+u16(q+2);t[2]=base+u16(q+4);j++;vx_d3d_tk[11]++;
      if(tn_lazy){tn_fin(t[0]);tn_fin(t[1]);tn_fin(t[2]);} /* .541: lit before the per-triangle path reads them */
      if(bad_prim(c,vv(in,t[0]),vv(in,t[1]),vv(in,t[2]),3))continue; /* below vv_lim: bad, never invalid; .531: bad_prim */
      if(op==3&&c->state.rs[8]!=3)prim_edges=(u16(q+6)>>8)&7;
      if(vx_d3d_geo_on)geo_tri(in,t);
      geo_skip=1;
      if(fast){if(fast_triangle(c,in,sv,t,&completed,&rc)){geo_skip=0;if(rc)goto fail;continue;}
       prim_tried=1;rc=primitive(c,in,sv,t,3,&completed);prim_tried=0;}
      else rc=primitive(c,in,sv,t,3,&completed);
      geo_skip=0;fast=-1;prim_edges=7;if(rc)goto fail;
     }
    } else {
    for(j=0;j<n;j+=m){uint32_t q0=vx_d3d_ftprof&&vx_d3d_phclock?vx_d3d_phclock():0,q1=0;m=n-j<VX_TRI_CHUNK?n-j:VX_TRI_CHUNK;tri_fill(op,p,base,j,m);
     if(q0){q1=vx_d3d_phclock();vx_d3d_ft[23]+=(q1-q0+8)>>4;vx_d3d_ft[25]+=m;} /* v10.453: [23] the indices read (tri_fill), [25] their triangles */
     for(k=0;k<m*3;k++){uint32_t i=tri3[k],r;if(i<vc&&i<131072UL&&(vv_ok[i>>3]&(1u<<(i&7)))){vx_d3d_ft[17]+=vx_d3d_ftprof;continue;}vx_d3d_ft[16]+=vx_d3d_ftprof;if(i<vv_lim&&vv_good(in,i)){vv_ok[i>>3]|=(uint8_t)(1u<<(i&7));VV_MARK(i>>3);vx_d3d_vvq++;continue;}r=vv(in,i); /* .548: the one-test check first (valid_vertex's answer for a vertex below vv_lim that passes it) */if(r==VX_D3D_INVALID){rc=VX_D3D_INVALID;goto fail;}if(r)badv=1;}
     if(q0)vx_d3d_ft[24]+=(vx_d3d_phclock()-q1+8)>>4;} /* v10.453: [24] the vertex checks */
    VX_PH(VX_PH_ASM);
    for(j=0;j<n;j+=m){m=n-j<VX_TRI_CHUNK?n-j:VX_TRI_CHUNK;if(n>VX_TRI_CHUNK)tri_fill(op,p,base,j,m); /* v10.309 (review): one chunk is still in tri3 from the check */
     for(k=0;k<m;k++) {
      const uint32_t *t=tri3+k*3;uint32_t l0=0;
      /* v10.453: a sampled triangle's time is one chain of clock reads from the loop's head (ft_t) to after fast_triangle returns: [20]
       * the head (bad-vertex and edge tests, the fast test), [11] the geometry counter, [21] the call, fast_triangle's parts, [22] the
       * return; [10] the whole; [26] the reads in the chain, [13] / [14] the clock's own cost (two reads outside the chain) */
      ft_sm=vx_d3d_ftprof&&vx_d3d_phclock&&!(++ft_cnt&15); /* v10.444: one in 16 sampled */
      if(ft_sm){uint32_t c0=vx_d3d_phclock(),c1=vx_d3d_phclock();vx_d3d_ft[13]+=c1-c0;vx_d3d_ft[14]++;vx_d3d_ft[12]++;l0=ft_t=vx_d3d_phclock();}
      if(badv&&bad_prim(c,vv(in,t[0]),vv(in,t[1]),vv(in,t[2]),3)){ft_sm=0;continue;} /* v10.367: a triangle with a bad vertex left out, the rest drawn */
      if(op==3&&c->state.rs[8]!=3)prim_edges=(u16(p+(j+k)*8+6)>>8)&7; /* v10.296: D3DTRIFLAG_EDGEENABLE1..3 */
      /* v10.321: whether the template takes the fast path is worked out again only after primitive(), the one call here that can build
       * or reset the template (fast_triangle and the batch flushes in it leave tc as it is; the states do not change inside a token) */
      if(fast<0)fast=tc.valid&&tc.ctx==c&&!tc.err&&!tc.skip&&tc.lay.fvf==in->fvf&&!FLATSW(c)&&c->state.rs[8]==3;
      FT(20);
      if(vx_d3d_geo_on)geo_tri(in,t); /* v10.440: counted here once, not again in primitive() */
      FT(11);
      geo_skip=1;
      if(fast) {
       if(fast_triangle(c,in,sv,t,&completed,&rc)){geo_skip=0;if(ft_sm){FT(22);vx_d3d_ft[10]+=(ft_t-l0+8)>>4;ft_sm=0;}if(rc)goto fail;continue;}
       if(ft_sm){FT(22);vx_d3d_ft[10]+=(ft_t-l0+8)>>4;ft_sm=0;}
       prim_tried=1;rc=primitive(c,in,sv,t,3,&completed);prim_tried=0;
      } else rc=primitive(c,in,sv,t,3,&completed);
      geo_skip=0;ft_sm=0;
      fast=-1;prim_edges=7;if(rc)goto fail;
     }}}}
   (void)a;(void)b;(void)d;
  }
  pos+=len;continue;
fail:
  if(tn_ov)tn_tss(c,0);
  /* v10.214: the commands before this one are drawn first; v10.215: if that batch fails, its error is the one reported, at its first
   * command (this command fails again on the retry) */
  if(!fail_set){uint32_t r2=vx_d3d_flush(c,sv,&completed);if(!r2)r2=cq_flush(sv,c,&completed);if(r2)rc=r2;}
  cq_clear(); /* v10.276: a failed call sends nothing more (the queued packets come again with the retry) */
  *error=fail_set?fail_cmd:start;return rc;
 }
 if(tn_ov)tn_tss(c,0); /* .536 */
 rc=vx_d3d_flush(c,sv,&completed);if(!rc&&!cq_hold(sv,c))rc=cq_flush(sv,c,&completed);if(rc){*error=fail_set?fail_cmd:lastcmd;return rc;} /* .550: cq_hold */
 return 0;
}
