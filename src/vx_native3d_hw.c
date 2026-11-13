/* Native CPU-port renderer extracted from v10.88 laboratory recipes.
 * App-facing subset still needs hardware qualification. No DMA/GART or
 * vertex-buffer fetch; fixed untextured program, no general shader compiler.
 * No FPU use in this translation unit. Copyright 2026, MIT. */
#include "vx_native3d.h"
#ifdef __GNUC__
typedef uint32_t __attribute__((may_alias)) vx_u32m; /* .552: a word read over the 16-bit indices (the host's compilers) */
#else
typedef uint32_t vx_u32m;
#endif
/* .527: the source line of the last refusal in this file (VX3DN_BAD_HERE: every BAD_PACKET returned here notes its line first), for the VxD's
 * refusal record (vx_native3d_vxd.c, TEXDBG BC EC/ED) */
uint32_t vx3dn_bad_line;
#define VX3DN_BAD_HERE (vx3dn_bad_line=__LINE__,VX3DN_BAD_PACKET)
/* .553: vx_xp_programs.h is no longer included: nothing here reads it (its two tables were unused statics in the VxD) */

static uint32_t fuint(uint32_t v) {
 uint32_t n=v,e=0;
 if(!v) return 0;
 while(n>>1) {n>>=1;e++;}
 return ((e+127UL)<<23)|((v<<(23-e))&0x7FFFFFUL);
}
static int surface(const vx3dn_surface *s,const vx3dn_info *i,int depth) {
 uint32_t bytes=(s->format==VX3DN_RGB565||s->format==VX3DN_D16)?2:4,size;
 /* Both colour and Z pitches have ten bits in 32-byte units. Larger
  * pitches would spill into control/reserved bits instead of encoding stride. */
 if(!s->width||!s->height||s->width>2048||s->height>2048||
    !s->pitch||(s->pitch&31)||(s->offset&255)||s->pitch>0x7FE0UL||
    s->width>s->pitch/bytes||s->offset>=i->fb_bytes||
    s->height>(i->fb_bytes-s->offset)/s->pitch) return 0;
 if(depth?!VX3DN_IS_DEPTH(s->format):!VX3DN_IS_COLOR(s->format)) return 0; /* v10.460: + A8R8G8B8 */
 size=s->pitch*s->height;
 if(s->offset<i->primary_bytes) {
  if(depth||s->offset||s->pitch!=i->pitch||s->width!=i->width||s->height!=i->height||
     s->format!=(i->bpp==16?VX3DN_RGB565:VX3DN_XRGB8888)) return 0;
 }
 return size!=0;
}
static int full(const vx3dn_packet *p) {return p->cb==sizeof(*p);}
/* v10.177: D3DBLEND -> the chip's factor codes (cookbook 16.7). Colour: 0 src colour, 1 dest colour, 2 src alpha, 3 dest alpha, 5 zero,
 * 8 src alpha saturate, +10 inverse. Alpha channel: 1 src alpha, 2 dest alpha, 9 zero, +10 inverse. BOTHSRCALPHA / BOTHINVSRCALPHA are
 * resolved by the core into a source/destination pair. 0 = not a valid factor here. */
/* v10.304: the packet's factor is bits 4:0 (blend_src bits 7:5 the equation, 31:8 the constant); 14 / 15 = the constant colour and its
 * inverse, 16 / 17 = the constant's alpha (the constant register then holds it in R, G and B) and its inverse: code 5 (HCM: R, G, B of
 * HABLRFCa / HABLRFCb), + 10 inverse. ZERO and ONE are code 5 with the constant 0 (as XP), so a side's constant is 0 unless its factor is
 * 14..17. Alpha channel: the targets keep no alpha; the constant factors read as their alpha through the alpha constant (code 9). */
static uint32_t blend_colour(uint32_t f,int src) {
 static const uint32_t c[18]={0,5,0x15,0,0x10,2,0x12,3,0x13,1,0x11,8,0,0,5,0x15,5,0x15};
 f&=31;if(f<1||f>17||f==12||f==13||(!src&&f==11))return 0xFF;return c[f];
}
static uint32_t blend_alpha(uint32_t f,int src) {
 static const uint32_t a[18]={0,9,0x19,1,0x11,1,0x11,2,0x12,2,0x12,0x19,0,0,9,0x19,9,0x19};
 f&=31;if(f<1||f>17||f==12||f==13||(!src&&f==11))return 0xFF;return a[f];
}
static int blend_const(uint32_t f){f&=31;return f>=14&&f<=17;}
/* a side's constant register value (11-bit 0..1 fixed point, 1.0 = 0x400) from 8-bit c */
static uint32_t c11(uint32_t c){return (c*1024UL+127UL)/255UL;}
static int textured(const vx3dn_packet *p) {return p->cb==sizeof(*p)&&p->op==VX3DN_DRAW&&p->tex.enable;}
static uint32_t units(const vx3dn_packet *p) {return !textured(p)?0:p->stages.units>1?p->stages.units:1;}
/* v10.172: the draw carries an XP pixel program (count > 0): XP's vertex layout and program, see xp_setup */
static int fogged(const vx3dn_packet *p) {return p->cb==sizeof(*p)&&p->op==VX3DN_DRAW&&p->fog.enable==1;} /* v10.195 */
static int xp_path(const vx3dn_packet *p) {return p->cb==sizeof(*p)&&p->op==VX3DN_DRAW&&p->shader.count;}
/* v10.310 (board, VXDX7 v2.80 and VXGL v3.8: unit 2 read texel (0, 0) whatever its set, the green-quadrant case's red and VXGL's black
 * unit-2 quads): unit 2 takes its coordinates from the vertex's third set (TextureC), as unit 1 took (0, 0) from set 1 until 04/28 said the
 * vertex carried it (v10.209). With three units the vertex carries a third set, a copy of the set unit 2 reads (12 attributes: 04/00
 * A8000C, 04/04 = C S 10, T 11 after B T 0D, 04/25 and 04/28 bits 7:6 as A's 15:14 and B's 11:10); v10.313: FE/01 byte 2 names unit 2's set
 * (21 / 25), the third set is never read. ACCEL_D3D_UNIT2C = 0: as before. */
volatile uint32_t vx3dn_unit2c=1;
/* v10.312 probe (TEXDBG 0x58 + n; board, v10.311: VXDX7's stage 2 read set 1 for set 0 and set 0 for set 1, VXGL's quads with unit 2 on
 * a real texture showed a seam on the diagonal and stripes): three-unit draws with the third set (TextureC) set to a marker, (0.25, 0.75)
 * (the blue quadrant), so the colour drawn says what unit 2 reads. 1 the v10.311 layout, FE/01 byte 2 = 29 (TextureC); 2 byte 2 = 21; 3 = 25;
 * 4 the 10-word layout with byte 2 = 29 (no third set); 5 as 1 with 04/25 left F6CC00; 6 as 1 with 01/93 = 7; 7 the marker in B's R and Q
 * (codes 0E 0F, 04/25 and 04/28 bits 9:8), byte 2 = 25. 0: as v10.311. */
static int c3_layout(const vx3dn_packet *p) {return vx3dn_unit2c&&xp_path(p)&&full(p)&&!p->stages.layout&&!p->stages.ps&&units(p)==3;}
static int c3_three(const vx3dn_packet *p) {return xp_path(p)&&full(p)&&!p->stages.layout&&!p->stages.ps&&units(p)==3;}
/* v10.313 (board, VXDX7 v2.81): with the 12-attribute layout FE/01 byte 2's source picks unit 2's set like units 0 and 1's (21 slot A, 25
 * slot B: red / white and green at both samples, strips on and off); 29 (TextureC) read no third set (the marker never showed) but whatever
 * was left (v10.311's wrong sets, VXGL's seam and stripes). Without the third set declared (the 10-word vertex) unit 2 reads (0, 0) whatever
 * its source (v10.307-v10.310's black). So the layout stays (the third set is a copy, never read) and byte 2 names the set. */
static uint32_t c3_fe2(const vx3dn_packet *p) {if(!c3_three(p))return 0; /* .530: the v10.312 probe's forms removed */
 if(c3_layout(p))return 0x21UL|(p->stages.set[2]<<2);return 0;}
/* texture register 30 codes of the XP driver (CLXP3D T26-T30) */
static uint32_t tex_code(uint32_t f) {
 switch(f){case VX3DN_TEX_ARGB8888:return 0x990000UL;case VX3DN_TEX_XRGB8888:return 0x980000UL;case VX3DN_TEX_RGB565:return 0x890000UL;
  case VX3DN_TEX_ARGB1555:return 0x8A0000UL;case VX3DN_TEX_ARGB4444:return 0x8B0000UL;case VX3DN_TEX_XRGB1555:return 0x880000UL;
  case VX3DN_TEX_L8:return 0x130000UL;case VX3DN_TEX_DXT1:return 0x290000UL;case VX3DN_TEX_DXT3:return 0x2A0000UL;case VX3DN_TEX_DXT5:return 0x2B0000UL;
  case VX3DN_TEX_V8U8:return 0x380000UL;case VX3DN_TEX_L6V5U5:return 0x390000UL;case VX3DN_TEX_X8L8V8U8:return 0x3A0000UL;} /* v10.208: T146-T148 */
 return 0;
}
static int tex_dxt(uint32_t f) {return f>=VX3DN_TEX_DXT1&&f<=VX3DN_TEX_DXT5;}
/* v10.199: bytes of one row (of texels, or of 4x4 blocks for DXT) and the number of rows, for a level of w x h */
/* v10.200: the sampler steps rows at least 32 bytes apart (board, v10.199: 8x8 L8 and DXT1) */
/* v10.321: rows rounded up to 32 bytes (a power-of-two width's are already; an NPOT width's, 02/22 bit 15 clear, as the narrow ones' 32) */
static uint32_t tex_pitch(uint32_t f,uint32_t w) {
 uint32_t r=tex_dxt(f)?(w<4?1:w/4)*(f==VX3DN_TEX_DXT1?8UL:16UL):w*(f==VX3DN_TEX_ARGB8888||f==VX3DN_TEX_XRGB8888||f==VX3DN_TEX_X8L8V8U8?4UL:f==VX3DN_TEX_L8?1UL:2UL);
 return r<32?32:tex_dxt(f)?r:(r+31UL)&~31UL;
}
static uint32_t tex_rows(uint32_t f,uint32_t h) {return tex_dxt(f)?(h<4?1:h/4):h;}
static int full(const vx3dn_packet *p);
/* v10.206: units beyond tex: two and three texture stages */
static uint32_t units(const vx3dn_packet *p);
static int tex_ok(const vx3dn_surface *t,uint32_t filter,uint32_t au,uint32_t av,const vx3dn_info *i) {
 /* v10.169: power-of-two texture, the format's pitch (the texture registers carry no pitch), base 256-byte aligned, inside the aperture.
  * v10.321: any size up to 2048 but a DXT texture's (it sets the power-of-two mode); a cube map's and a volume's are checked there */
 return !(!tex_code(t->format)||!t->width||!t->height||t->width>2048||t->height>2048||
      (tex_dxt(t->format)&&((t->width&(t->width-1))||(t->height&(t->height-1))))||t->pitch!=tex_pitch(t->format,t->width)||(t->offset&255)||
      t->offset>=i->fb_bytes||tex_rows(t->format,t->height)>(i->fb_bytes-t->offset)/t->pitch||
      filter>VX3DN_FILTER_MIN_LINEAR||au>3||av>3);
}
/* v10.199: a mip chain: 1..12 levels, filter 0..2 (a chain needs one), anisotropy 1..16, bias 0..255, every level inside the frame buffer
 * (.528: a helper, for the packet's chain and each texture run's) */
static int mip_ok(const vx3dn_surface *t,const vx3dn_mip *m,const vx3dn_info *i) {
 uint32_t l;
 if(m->levels<1||m->levels>12||m->filter>2||m->aniso<1||m->aniso>16||m->bias>255||(m->levels>1&&!m->filter)) return 0;
 for(l=1;l<m->levels;l++){uint32_t w=t->width>>l,h=t->height>>l,o=m->level[l-1];if(!w)w=1;if(!h)h=1;
  if(!o||o>=0x10000UL||o>(i->fb_bytes-t->offset)>>8)return 0;
  o=t->offset+(o<<8);if(o>=i->fb_bytes||tex_rows(t->format,h)>(i->fb_bytes-o)/tex_pitch(t->format,w))return 0;}
 return 1;
}
static int stenciled(const vx3dn_packet *p) {return full(p)&&p->op==VX3DN_DRAW&&p->ext.stencil==1&&p->depth.format==VX3DN_D24S8;}
/* v10.199: registry ACCEL_D3D_MIP (default 1); 0 samples level 0 only. ACCEL_D3D_ANISO (default 1); 0 filters linearly instead. */
volatile uint32_t vx3dn_mipon=1,vx3dn_aniso=1;
static int mipped(const vx3dn_packet *p) {return vx3dn_mipon&&full(p)&&p->tex.enable&&p->mip.levels>1;}
static int cube(const vx3dn_packet *p) {return full(p)&&p->tex.enable&&p->stages.layout==VX3DN_LAYOUT_CUBE;}
/* v10.321: two units with a cube map or a 3D texture among them (packet stages.s3: the sets with a third coordinate) */
static int mixed3(const vx3dn_packet *p) {return full(p)&&p->tex.enable&&p->stages.units==2&&p->stages.s3&&p->shader.count;}
static int volume(const vx3dn_packet *p) {return full(p)&&p->tex.enable&&p->stages.layout==VX3DN_LAYOUT_VOLUME;} /* v10.317 */
/* v10.441: the prepacked form is what this emitter sends for the packet: an XP program's 10-word vertex, the plain loop (no s t q, no
 * swapped sets, no third set) */
extern uint32_t vx3dn_vfast;
int vx3dn_pre_ok(const vx3dn_packet *p) {return full(p)&&VX3DN_PRE_FORM(p)&&units(p)<=2&&vx3dn_vfast;}
/* v10.201: 01/01's depth bits: 8 test, 4 write, 10 stencil (with stencil and Z off, the test runs as ALWAYS without writes) */
static int stenciled(const vx3dn_packet *p);
uint32_t vx3dn_dither_last[3],vx3dn_set_last[7];
/* v10.290 (board, VXTEX v1.4 probe): with dither off the engine truncates to 565 (v >> shift) whatever 01/53 bits 16:15 hold (10, 11, 01 and 00
 * read the same grey ramp); swrast rounds to nearest, so the two can be two 5-bit steps apart where a filter's result is high */
/* v10.214: registry ACCEL_D3D_BURST (default 1 since v10.216) / TEXDBG 0x72 (on) 0x73 (off): a triangle list goes to the engine as one burst, 04/24 = the vertex count
 * (XP sends two triangles as 24 = 6, CLXP3D T04), one idle wait for the packet instead of one per triangle */
uint32_t vx3dn_burst=1; /* v10.216: default 1 (board, VXDX7 v2.38: cube maps and 40 batched triangles drew right with it) */
/* v10.205: 01/00 bit 0 is RT0's dither enable (bits 3:1 RT3..RT1; 01/01 bit 0 is the line pattern). 01/53 bits 16:15 = 00 table x (2^n-1),
 * 01 table, 10 round x (2^n-1) (dither off), 11 round. Format stays 89. Experiment (TEXDBG 0x30+n): 0 = 01/00 bit 0 + 53 000C0F (default),
 * 1 = bit 0 + 53 008C0F, 2 = 53 000C0F without bit 0 (control), 3 = 01/00 bits 3:0 = F (XP's T48/T49/T62 value) + 53 000C0F. */
static int dithered(const vx3dn_packet *p) {return full(p)&&(p->ext.dither&1)&&p->target.format==VX3DN_RGB565;} /* v10.481: bit 0 (bit 1 the flat shading) */
/* v10.415 (board, GTA3 at 32 bpp on v10.414: flat menu areas came out in a 4x4 pattern, channels 1 apart and some blue 15 apart; the same
 * screen on another card is one colour): 01/53, 5B, 63, 6B bit 16 is cleared for DITHERENABLE only on a 16-bit target; a 32-bit target keeps
 * it set (no dither, nothing to dither to). ACCEL_D3D_DITHER32 = 1: cleared for a 32-bit target too, as v10.201-v10.414. */
static int dither53(const vx3dn_packet *p) {return full(p)&&(p->ext.dither&1)&&p->target.format==VX3DN_RGB565;} /* .530: ACCEL_D3D_DITHER32 (32-bit targets, default 0) removed */
/* v10.243-v10.245: where the engine keeps the stencil. The board (VXGL v1.6-v1.8, VXDX7 v2.67 --stencil) showed every stencil draw
 * of ours wrote and read the stencil as an 8-bit plane at video memory 0, the visible screen (the band at the top of the desktop, EQUAL
 * tests passing on the screen's bytes), and never the Z words' low bytes. The plane is set by 01/28, 01/29 and 01/2A, which XP writes
 * for every stencil draw (T52) and we wrote 0 (vx_native3d.h, VX3DN_SPLANE_*). XP's other two differences go with it: 01/17 = 000080
 * (every XP capture with Z) and 01/01 | 2200 (T50, T52). ACCEL_D3D_STENCIL_ZW (TEXDBG B2-B4 set 0-2, B7 reads): 1 (default) XP's T52
 * state: the plane, 17 = 80, 01/01 | 2200, the plane's read and write bits as the draw needs them (T52: ALWAYS REPLACE 400, NOTEQUAL
 * INCR 1400); 2 the same with both bits always; 0 as v10.241. v10.247: 2 is the default. VXGL v2.0 on v10.246: with 1 every OpenGL
 * stencil scene whose EQUAL draw reads without writing found nothing (A, B, C, E), with 2 all pass; VXDX7's Direct3D orders pass with
 * either. XP was only captured with the write bit on (T52: 418410, 419410). */
uint32_t vx3dn_stencil_zw=2;
static int stenciled(const vx3dn_packet *p);
static int xp_st(const vx3dn_packet *p) {return vx3dn_stencil_zw&&stenciled(p);}
static uint32_t zbits(const vx3dn_packet *p) {return (p->z_enable?(p->z_write?12UL:8UL):(stenciled(p)?8UL:0))|(stenciled(p)?0x10UL:0);}
/* 01/29 for a stencil draw: read when the test is not ALWAYS, an op reads (INCR, DECR, INVERT) or the write mask is partial, write when
 * an op is not KEEP and the write mask is not 0 */
static uint32_t st29(const vx3dn_packet *p) {
 uint32_t w2=p->ext.stencil_w2,f=(w2>>16)&7,ops[3],k,rd=f!=7,wr=0;
 ops[0]=(w2>>6)&7;ops[1]=(w2>>3)&7;ops[2]=w2&7;
 for(k=0;k<3;k++){if(ops[k]>=3)rd=1;if(ops[k])wr=1;}
 if(!(p->ext.stencil_w1&0xFF))wr=0;
 if(wr&&(p->ext.stencil_w1&0xFF)!=0xFF)rd=1; /* a partial write mask keeps the other bits */
 if(vx3dn_stencil_zw==2)rd=wr=1;
 return 0x418000UL|(VX3DN_SPLANE_PITCH(p->depth.width)>>4)|(wr?0x400UL:0)|(rd?0x1000UL:0);
}
static uint32_t en00(const vx3dn_packet *p,uint32_t more) {return (p->blend?0x10UL:0)|(fogged(p)?0xF00UL:0)|more|(!dithered(p)?0:1UL);}
static uint32_t log2u(uint32_t v) {uint32_t e=0;while(v>1){v>>=1;e++;}return e;}
/* v10.358: ACCEL_3D_GUARD (registry, or VX2D control 0xFFFFFE31): a vertex coordinate from -0.5 to -0.0 where the clip edge is 0 (the
 * vertices OpenGL's clipping puts on the window's left or top edge, GL's 0 being the engine's -0.5); the engine's clip rectangle still
 * bounds the pixels. Default 0 until the board shows the engine draws them (VXGAME guard) */
uint32_t vx3dn_guard=1; /* v10.361: on by default (ACCEL_3D_GUARD 1), as the HAL's and VXGL's word */
#define GUARD_OK(w,lo) (vx3dn_guard&&!(lo)&&(w)>=0x80000000UL&&(w)<=0xBF000000UL)
/* v10.449 (ACCEL_D3D_ONECOPY, the VxD): a packet whose prepacked vertices are still in the caller's packet (vx3dn_vsrc_pkt, words at
 * vx3dn_vsrc): vx3dn_validate leaves their tests to the emitter, which copies them into the command block and tests the copy (pre_check,
 * the same tests); vx3dn_copyw is the VxD's block copy. */
const uint32_t *vx3dn_vsrc;const vx3dn_packet *vx3dn_vsrc_pkt;void (*vx3dn_copyw)(uint32_t *,const uint32_t *,uint32_t);
static int pre_check(const vx3dn_packet *p,const uint32_t *w,uint32_t n) { /* 1: every vertex passes vx3dn_validate's tests */
 uint32_t xr=fuint((uint32_t)p->clip.right),yb=fuint((uint32_t)p->clip.bottom),xl=fuint((uint32_t)p->clip.left),yt=fuint((uint32_t)p->clip.top);
 uint32_t xw=xr-xl,yh=yb-yt;const uint32_t *we=w+n*10;int tx=textured(p),two=full(p)&&p->stages.units>1;
 for(;w<we;w+=10){
  if((((w[0]-xl>xw)&&(w[0]<xl||!GUARD_OK(w[0],xl)))||((w[1]-yt>yh)&&(w[1]<yt||!GUARD_OK(w[1],yt)))||w[2]>0x3F800000UL||w[3]-1UL>=0x7F7FFFFFUL)||
     (tx&&((w[6]&0x7F800000UL)==0x7F800000UL||(w[7]&0x7F800000UL)==0x7F800000UL))||(two&&((w[8]&0x7F800000UL)==0x7F800000UL||(w[9]&0x7F800000UL)==0x7F800000UL)))return 0;
 }
 return 1;
}
static int vx_vbad; /* the emitter met a vertex that fails (pre_check) */
/* .527: .526's ACCEL_D3D_FASTCHK (a branch-free screen of the prepacked vertices before the exact loop) removed: board, .526: validation
 * 8.34 us a packet with it against 6.15 without; 3 of 625,820 GTA3 packets flagged */
/* .526: .525's ACCEL_D3D_IXCHK (vertices and indices checked while copied in) removed: the copy loop cost more than the check pass
 * it replaced (board: 0.9 us a packet more in GTA3, 1.25 in VXSHADOW) */
/* .524: st 0 (vx3dn_validate_geo): the packet's state (VX3DN_STATE_RANGES, every range) is byte for byte that of a packet validated
 * against the same info: the checks that read only the state are not done again; the vertex count, the runs or the indices, and every
 * per-vertex word are (the tests that use the state, the clip and the texturing, use the same state) */
static uint32_t validate_x(const vx3dn_packet *p,const vx3dn_info *i,int st);
uint32_t vx3dn_validate(const vx3dn_packet *p,const vx3dn_info *i) {return validate_x(p,i,1);}
uint32_t vx3dn_validate_geo(const vx3dn_packet *p,const vx3dn_info *i) {return validate_x(p,i,0);}
static uint32_t validate_x(const vx3dn_packet *p,const vx3dn_info *i,int st) {
 uint32_t v,ae,be;
 if(!p||!i||(p->cb!=sizeof(*p)&&p->cb!=VX3DN_PACKET_V1_BYTES)||p->abi!=VX3DN_ABI||p->arm!=VX3DN_ARM) return VX3DN_BAD_HERE;
 if(p->generation!=i->generation) return VX3DN_STALE;
 if(!i->ready||i->fault) return VX3DN_LOST;
 if(st&&(!surface(&p->target,i,0)||p->clip.left<0||p->clip.top<0||
    p->clip.right<p->clip.left||p->clip.bottom<p->clip.top||
    (uint32_t)p->clip.right>p->target.width||(uint32_t)p->clip.bottom>p->target.height||
    p->z_enable>1||p->z_write>1||p->z_func<1||p->z_func>8||p->blend>1||(p->write_mask&~0x1FFUL)||(full(p)&&p->fog.enable>1))) return VX3DN_BAD_HERE;
 /* v10.215: a stencil-only draw (Z off) reads and writes the depth surface too: check it (review: its offset and pitch went unchecked) */
 if(st&&(p->z_enable||stenciled(p)||(p->op==VX3DN_CLEAR&&(p->clear_flags&(VX3DN_CLEAR_Z|VX3DN_CLEAR_STENCIL))))) {
  if(!surface(&p->depth,i,1)||p->depth.width<p->target.width||p->depth.height<p->target.height) return VX3DN_BAD_HERE;
  ae=p->target.offset+p->target.pitch*p->target.height;
  be=p->depth.offset+p->depth.pitch*p->depth.height;
  if(p->target.offset<be&&p->depth.offset<ae) return VX3DN_BAD_HERE;
  /* v10.245: a stencil draw or clear uses the plane after the Z buffer: inside video memory, off the target */
  if(p->depth.format==VX3DN_D24S8&&(stenciled(p)||(p->op==VX3DN_CLEAR&&(p->clear_flags&VX3DN_CLEAR_STENCIL)))) {
   /* .532 (review of .530, B3): and the region after it that 01/2A names (VX3DN_CZ_*, the HAL's VX_D3D_surface bounds it too): the
    * whole block plane, pad and region inside video memory and off the target (the plane and its pad only were checked) */
   uint32_t so=VX3DN_SPLANE_OFF(&p->depth),se=VX3DN_SPLANE_END(&p->depth),ce=VX3DN_CZ_END(&p->depth);
   if(se+256UL>i->fb_bytes||se<so||ce<se||ce>i->fb_bytes||(p->target.offset<ce&&so<ae)) return VX3DN_BAD_HERE;
  }
 }
 if(p->op==VX3DN_DRAW) {
  {uint32_t sv=(full(p)&&p->ext.prim==1)?2:(full(p)&&p->ext.prim==2)?1:3; /* .524: sv (was st: the parameter's name now) */
   if(!p->vertex_count||p->vertex_count>(full(p)&&p->pre.on==2?VX3DN_IX_VERTICES:VX3DN_MAX_VERTICES)||(!full(p)&&p->vertex_count>VX3DN_V1_VERTICES)||((!full(p)||!p->bursts.n)&&p->vertex_count%sv)||(p->vertex_count>VX3DN_MAX_VERTICES&&(p->stages.layout||p->stages.s3))) return VX3DN_BAD_HERE; /* .529: past 192 only an indexed packet, never with the arrays of 192 (q, r1) */ /* v10.237: a V1 packet holds 48 */
   /* v10.236: runs: triangles only, at most 32, lists in whole triangles, strips of 3 or more, adding up to the vertex count */
   /* v10.470: an indexed packet: one VX3DN_TOPO_INDEXED run of whole triangles, at most VX3DN_MAX_INDICES, every index below the vertex
    * count (its vertices are checked below, each once) */
   if(full(p)&&p->pre.on==2) {uint32_t k,ni;const uint16_t *ix=VX3DN_CIDX(p);
    if(p->ext.prim||p->bursts.n!=1||((p->bursts.b[0]>>16)&~(VX3DN_IX_BITS>>16))!=VX3DN_TOPO_INDEXED) return VX3DN_BAD_HERE; /* .523: V8 (.529: the cull bits refused) */
    if((p->bursts.b[0]&VX3DN_IX_V8)&&!VX3DN_V8_FORM(p)) return VX3DN_BAD_HERE; /* .523: 8-word vertices only in their form */
    ni=p->bursts.b[0]&0xFFFFUL;if(ni<3||ni%3||ni>VX3DN_MAX_INDICES) return VX3DN_BAD_HERE;
    /* .552: the indices two at a time (g.rec: 46,000 indices a frame at a load, a compare and a branch each, a quarter of this
     * function). An indexed packet has at most VX3DN_IX_VERTICES (384) vertices, so with K = 0x8000 - count in both halves of a word,
     * an index below 0x8000 is at or past the count exactly when adding K sets its bit 15 (the sum stays below 0x10000: no carry
     * into the other half), and an index with bit 15 set is past any count. The bad bits of every pair are gathered and tested once:
     * the answer is the loop's. The array is read as bytes pairs through a word pointer: the VxD's copy is word aligned (the packet
     * is), and an odd last index is tested alone. A count the trick does not cover (0 or above 0x8000: refused elsewhere) takes the loop. */
    {const uint32_t vc=p->vertex_count;
     if(vc&&vc<=0x8000UL&&!((uintptr_t)ix&3)){const uint32_t K=(0x8000UL-vc)*0x00010001UL;const vx_u32m *w=(const vx_u32m*)(const void*)ix,*we=w+(ni>>1);uint32_t bad=0;
      for(;w<we;w++){const uint32_t x=*w;bad|=x|((x&0x7FFF7FFFUL)+K);}
      if(ni&1)bad|=ix[ni-1]>=vc?0x8000UL:0;
      if(bad&0x80008000UL) return VX3DN_BAD_HERE;}
     else for(k=0;k<ni;k++)if(ix[k]>=vc) return VX3DN_BAD_HERE;}
    if(p->runs.n){uint32_t at=0; /* .528: texture runs, each texture checked as the packet's own (tex_ok, mip_ok) */
     if(p->runs.n>VX3DN_TEXRUNS_MAX||!VX3DN_TEXRUN_FORM(p)||!textured(p)||p->tex.mode!=1) return VX3DN_BAD_HERE;
     for(k=0;k<p->runs.n;k++){const vx3dn_texrun *r=&p->runs.r[k];
      if(r->at<=at||r->at>=ni||r->at%3||!tex_ok(&r->surface,r->filter,r->address_u,r->address_v,i)||!mip_ok(&r->surface,&r->mip,i)) return VX3DN_BAD_HERE;
      at=r->at;}}}
   else if(full(p)&&p->runs.n) return VX3DN_BAD_HERE; /* .528: runs only in an indexed packet */
   else if(full(p)&&(p->pre.on>2||(p->bursts.n&&((p->bursts.b[0]>>16)&0xFFUL)==VX3DN_TOPO_INDEXED))) return VX3DN_BAD_HERE; /* (the loop below refuses it in any later run) */
   else if(full(p)&&p->bursts.n) {uint32_t k,sum=0;
    if(p->ext.prim||p->bursts.n>VX3DN_MAX_BURSTS) return VX3DN_BAD_HERE;
    for(k=0;k<p->bursts.n;k++){uint32_t t=p->bursts.b[k]>>16,c=p->bursts.b[k]&0xFFFFUL;
     if(t>VX3DN_TOPO_STRIP||!c||(t==VX3DN_TOPO_LIST&&c%3)||(t==VX3DN_TOPO_STRIP&&c<3)) return VX3DN_BAD_HERE;sum+=c;}
    if(sum!=p->vertex_count) return VX3DN_BAD_HERE;}}
  /* v10.201: primitive type, dither, stencil (D24S8 only; w1 = ref, mask, write mask bytes; w2 = func 0..7, three ops 0..7) */
  if(st&&full(p)&&(p->ext.prim>2||p->ext.dither>3||p->ext.stencil>1||(p->ext.stencil&&p->depth.format!=VX3DN_D24S8)||
     p->ext.stencil_w1>0xFFFFFFUL||(p->ext.stencil_w2&~0x0701FFUL))) return VX3DN_BAD_HERE;
  {uint32_t xr=fuint((uint32_t)p->clip.right),yb=fuint((uint32_t)p->clip.bottom),xl=fuint((uint32_t)p->clip.left),yt=fuint((uint32_t)p->clip.top);
   /* v10.236: the four bounds once per packet (fuint loops over the bits; it ran four times per vertex) */
   /* v10.412 (board, v10.411: 5.0 microseconds a packet, nearly all in these loops, about 35 instructions a vertex): one loop, the texture
    * coordinates' check in it for a textured draw (both refusals are BAD_PACKET, nothing between them); a coordinate inside the clip is
    * one unsigned compare (x - left <= right - left: the encodings are ordered), the exact test only outside it; z and rhw one compare each */
   const vx3dn_vertex *a=p->vertices,*e=a+p->vertex_count;uint32_t xw=xr-xl,yh=yb-yt;
   const uint32_t *t=p->tex.uv[0];
#define VX_VPOS(a) ((((a)->x-xl>xw)&&((a)->x<xl||!GUARD_OK((a)->x,xl)))||(((a)->y-yt>yh)&&((a)->y<yt||!GUARD_OK((a)->y,yt)))|| \
     (a)->z>0x3F800000UL||(a)->rhw-1UL>=0x7F7FFFFFUL)
   /* Nonnegative, finite coordinates, clipped before ring 0. Reject NaNs,
    * infinities and sign bits using ordered nonnegative IEEE encodings. */
   if(p->pre.on==1&&p==vx3dn_vsrc_pkt){if(!vx3dn_pre_ok(p)) return VX3DN_BAD_HERE;} /* v10.449: tested by the emitter on the copy (pre_check); v10.470: an
    * indexed packet's words are always tested here (the VxD copies them: the emitter uploads the copy) */
   else if(p->pre.on){ /* v10.441: the prepacked words, the same tests (x y z rhw, u0 v0 of a textured draw; u1 v1 below) */
    const uint32_t vs=VX3DN_PRE_STRIDE(p),*w=p->pre.w,*we=w+p->vertex_count*vs;int tx=textured(p); /* .523: vs the words a vertex */
    if(!vx3dn_pre_ok(p)) return VX3DN_BAD_HERE;
#define VX_WPOS(w) ((((w)[0]-xl>xw)&&((w)[0]<xl||!GUARD_OK((w)[0],xl)))||(((w)[1]-yt>yh)&&((w)[1]<yt||!GUARD_OK((w)[1],yt)))|| \
     (w)[2]>0x3F800000UL||(w)[3]-1UL>=0x7F7FFFFFUL)
    for(;w<we;w+=vs)if(VX_WPOS(w)||(tx&&((w[6]&0x7F800000UL)==0x7F800000UL||(w[7]&0x7F800000UL)==0x7F800000UL))) return VX3DN_BAD_HERE;
#undef VX_WPOS
   }
   else if(textured(p)){for(;a<e;a++,t+=2)if(VX_VPOS(a)||(t[0]&0x7F800000UL)==0x7F800000UL||(t[1]&0x7F800000UL)==0x7F800000UL) return VX3DN_BAD_HERE;}
   else for(;a<e;a++)if(VX_VPOS(a)) return VX3DN_BAD_HERE;
#undef VX_VPOS
  }
  if(st&&textured(p)) {
   if(!tex_ok(&p->tex.surface,p->tex.filter,p->tex.address_u,p->tex.address_v,i)||p->tex.mode!=1) return VX3DN_BAD_HERE;
  }
  /* v10.206: units 1 and 2 (a pixel program, unit 0 set up), coordinate sets 0/1, finite uv1 */
  if(st&&full(p)&&(p->stages.units>3||p->stages.c22>1||(p->stages.c22&&!p->shader.count))) return VX3DN_BAD_HERE;
  /* v10.208: an explicit schedule (1..4 TAU words, 24-bit header values, up to four constants c0..c63), the 9-dword layout on one unit */
  if(st&&full(p)&&(p->stages.ps>1||(p->stages.ps&&(!xp_path(p)||!textured(p)||!p->stages.ntau||p->stages.ntau>4||p->stages.h90>0xFFFFFFUL||p->stages.h91>0xFFFFFFUL||
     p->stages.h93>0xFFFFFFUL||p->stages.h95>0xFFFFFFUL||(p->shader.kflags&2)))||p->stages.nkc>4||p->stages.layout>VX3DN_LAYOUT_VOLUME||(p->stages.layout&&(!xp_path(p)||!textured(p)||(p->stages.units>1&&(!mixed3(p)||p->stages.layout==VX3DN_LAYOUT_PROJ||!(p->stages.s3&1))))))) return VX3DN_BAD_HERE;
  /* v10.321: the 3D sets of a two-unit draw (no schedule, no projection), each unit's layout flat, cube or volume */
  if(st&&full(p)&&(p->stages.s3>3||(p->stages.s3&&(!mixed3(p)||p->stages.ps||!xp_path(p)||!textured(p))))) return VX3DN_BAD_HERE;
  if(st&&full(p)){uint32_t u;for(u=0;u<2;u++)if(p->stages.ulayout[u]&&((p->stages.ulayout[u]!=VX3DN_LAYOUT_CUBE&&p->stages.ulayout[u]!=VX3DN_LAYOUT_VOLUME)||u+1>=units(p)||!(p->stages.s3&(1U<<p->stages.set[u+1])))) return VX3DN_BAD_HERE;}
  if(full(p)&&(p->stages.s3&2))for(v=0;v<p->vertex_count;v++)if((p->stages.r1[v]&0x7F800000UL)==0x7F800000UL) return VX3DN_BAD_HERE;
  if(full(p)&&(p->stages.s3&1)&&!p->stages.layout)for(v=0;v<p->vertex_count;v++)if((p->stages.q[v]&0x7F800000UL)==0x7F800000UL) return VX3DN_BAD_HERE;
  if(full(p)&&p->stages.layout)for(v=0;v<p->vertex_count;v++)if((p->stages.q[v]&0x7F800000UL)==0x7F800000UL) return VX3DN_BAD_HERE; /* .524: before the state's checks */
  if(st&&full(p)){uint32_t k;for(k=0;k<p->stages.nkc;k++)if(p->stages.kc_reg[k]>63) return VX3DN_BAD_HERE;
   /* v10.317: a cube map's mip chain: every face's levels at the table's offsets from its own base, inside the frame buffer */
   if(p->stages.layout==VX3DN_LAYOUT_CUBE){vx3dn_surface f=p->tex.surface;if(f.width!=f.height||(f.width&(f.width-1))||p->mip.levels>12) return VX3DN_BAD_HERE; /* (review) the level table's 11 entries before the chain check below */
    for(k=0;k<6;k++){uint32_t l;f.offset=p->stages.face[k];if(!tex_ok(&f,p->tex.filter,p->tex.address_u,p->tex.address_v,i)) return VX3DN_BAD_HERE;
     for(l=1;mipped(p)&&l<p->mip.levels;l++){uint32_t w=f.width>>l,h=f.height>>l,o=p->mip.level[l-1];if(!w)w=1;if(!h)h=1;
      if(!o||o>=0x10000UL||o>(i->fb_bytes-f.offset)>>8)return VX3DN_BAD_HERE;
      o=f.offset+(o<<8);if(o>=i->fb_bytes||tex_rows(f.format,h)>(i->fb_bytes-o)/tex_pitch(f.format,w))return VX3DN_BAD_HERE;}}}
   /* v10.317: a 3D texture: depth a power of two (1..2048), each slice at least a slice long, the last one inside, the R address 0..3, no chain */
   if(p->stages.layout==VX3DN_LAYOUT_VOLUME){const vx3dn_surface *t=&p->tex.surface;uint32_t d=p->stages.face[0],sl=p->stages.face[1],sz;
    if(!d||d>2048||(d&(d-1))||(t->width&(t->width-1))||(t->height&(t->height-1))||p->stages.face[2]>3||mipped(p)||tex_dxt(t->format)) return VX3DN_BAD_HERE;
    sz=tex_rows(t->format,t->height)*t->pitch;
    if(sl<sz||(d>1&&sl>(i->fb_bytes-t->offset)/(d-1))||t->offset+sl*(d-1)>i->fb_bytes-sz) return VX3DN_BAD_HERE;}}
  if(full(p)&&p->stages.units>1) {
   uint32_t u;
   if(st&&(!textured(p)||!xp_path(p))) return VX3DN_BAD_HERE;
   if(st)for(u=0;u<p->stages.units;u++)if(p->stages.set[u]>1) return VX3DN_BAD_HERE;
   if(st)for(u=1;u<p->stages.units;u++){const vx3dn_unit *n=&p->stages.unit[u-1];uint32_t nl=p->stages.umip[u-1]&255,l;if(!tex_ok(&n->surface,n->filter,n->address_u,n->address_v,i)) return VX3DN_BAD_HERE;
    /* v10.321: a cube on the unit: square power-of-two faces, each inside, the chain's levels at the table's offsets from each face; a
     * volume: a power-of-two size and depth, each slice inside, no chain */
    if(p->stages.ulayout[u-1]==VX3DN_LAYOUT_CUBE){vx3dn_surface f=n->surface;uint32_t k2;if(f.width!=f.height||(f.width&(f.width-1))) return VX3DN_BAD_HERE;
     for(k2=0;k2<6;k2++){f.offset=p->stages.uface[u-1][k2];if(!tex_ok(&f,n->filter,n->address_u,n->address_v,i)) return VX3DN_BAD_HERE;
      for(l=1;l<nl&&l<=8;l++){uint32_t w=f.width>>l,o=p->stages.ulevel[u-1][l-1];if(!w)w=1;if(!o||o>=0x10000UL||o>(i->fb_bytes-f.offset)>>8)return VX3DN_BAD_HERE;
       o=f.offset+(o<<8);if(o>=i->fb_bytes||tex_rows(f.format,w)>(i->fb_bytes-o)/tex_pitch(f.format,w))return VX3DN_BAD_HERE;}}}
    else if(p->stages.ulayout[u-1]==VX3DN_LAYOUT_VOLUME){const vx3dn_surface *t=&n->surface;uint32_t d=p->stages.uface[u-1][0],sl=p->stages.uface[u-1][1],sz;
     if(!d||d>2048||(d&(d-1))||(t->width&(t->width-1))||(t->height&(t->height-1))||p->stages.uface[u-1][2]>3||nl>1||tex_dxt(t->format)) return VX3DN_BAD_HERE;
     sz=tex_rows(t->format,t->height)*t->pitch;
     if(sl<sz||(d>1&&sl>(i->fb_bytes-t->offset)/(d-1))||t->offset+sl*(d-1)>i->fb_bytes-sz) return VX3DN_BAD_HERE;}
    /* v10.304: the unit's mip chain: 1..12 levels, filter 1..2 with a chain, offsets of levels 1..8 inside the frame buffer */
    if(nl<1||nl>12||((p->stages.umip[u-1]>>8)&255)>2||(nl>1&&!((p->stages.umip[u-1]>>8)&255))||
       (p->stages.umip[u-1]>>16)>16||p->stages.ubias[u-1]>255) return VX3DN_BAD_HERE;
    for(l=1;l<nl&&l<=8;l++){uint32_t w=n->surface.width>>l,h=n->surface.height>>l,o=p->stages.ulevel[u-1][l-1];if(!w)w=1;if(!h)h=1;
     if(!o||o>=0x10000UL||o>(i->fb_bytes-n->surface.offset)>>8)return VX3DN_BAD_HERE;
     o=n->surface.offset+(o<<8);if(o>=i->fb_bytes||tex_rows(n->surface.format,h)>(i->fb_bytes-o)/tex_pitch(n->surface.format,w))return VX3DN_BAD_HERE;}}
   if(p->pre.on){if(p!=vx3dn_vsrc_pkt)for(v=0;v<p->vertex_count;v++)if((p->pre.w[v*10+8]&0x7F800000UL)==0x7F800000UL||(p->pre.w[v*10+9]&0x7F800000UL)==0x7F800000UL) return VX3DN_BAD_HERE;} /* v10.441 */
   else for(v=0;v<p->vertex_count;v++)if((p->stages.uv1[v][0]&0x7F800000UL)==0x7F800000UL||(p->stages.uv1[v][1]&0x7F800000UL)==0x7F800000UL) return VX3DN_BAD_HERE;
  }
  /* v10.199: mip chain: every level inside the frame buffer, 2..12 levels, filter 0..2, anisotropy 1..16 */
  if(st&&full(p)&&p->tex.enable) {
   const vx3dn_surface *t=&p->tex.surface;
   if(!mip_ok(t,&p->mip,i)) return VX3DN_BAD_HERE; /* .528: one helper for the packet's chain and each texture run's */
  }
  if(st&&xp_path(p)&&(p->shader.kflags>3||p->shader.count>((p->shader.kflags&2)?7UL:8UL)||((p->shader.kflags&2)&&p->shader.count<6)||((p->shader.kflags&2)&&!textured(p)))) return VX3DN_BAD_HERE;
  if(st&&full(p)&&p->stipple.on>1) return VX3DN_BAD_HERE; /* v10.308 */
  if(st&&full(p)&&(p->raster.alpha_func>8||p->raster.alpha_ref>255||(p->raster.blend_src&&(blend_colour(p->raster.blend_src,1)==0xFF||blend_colour(p->raster.blend_dst,0)==0xFF||((p->raster.blend_src>>5)&7)>VX3DN_BLENDOP_MAX)))) return VX3DN_BAD_HERE;
 } else if(p->op==VX3DN_CLEAR) {
  if(!VX3DN_CLEAR_WHAT(p->clear_flags)||(p->clear_flags&~0xFF07UL)||p->vertex_count||
     ((p->clear_flags&VX3DN_CLEAR_STENCIL)&&p->depth.format!=VX3DN_D24S8)||(VX3DN_CLEAR_SVAL(p->clear_flags)&&!(p->clear_flags&VX3DN_CLEAR_STENCIL))) return VX3DN_BAD_HERE;
 } else return VX3DN_UNSUPPORTED;
 return VX3DN_OK;
}
static void wr(const vx3dn_io *io,uint32_t r,uint32_t v) {io->write(io->context,r,v);}
static void type(const vx3dn_io *io,uint32_t t) {io->write(io->context,0x43C,t<<16);}
/* v10.282: the direct sink. Under the VxD's fetch capture a state word went reg -> wr -> io->write -> cap_write (its checks again for every
 * word): about 50 instructions a word, nearly all of the 28-30 microseconds a packet's 560 words took to build on the board (v10.275,
 * v10.280). When the capture has opened a block for direct words (vx3dn_wform: 2 an FE02 register block, 3 an FE03 data block; 0 = none,
 * every word through io->write as before), reg and dat store the word at vx3dn_wp themselves while it is below vx3dn_wend; the capture
 * takes the count back from the pointer (cap_sync) before it looks at its own. The words are the same, in the same order. */
uint32_t *vx3dn_wp,*vx3dn_wend,vx3dn_wform;
uint32_t vx3dn_stage;
static void reg_slow(const vx3dn_io *io,uint32_t r,uint32_t v) {
 uint32_t w=(r<<24)|(v&0xFFFFFFUL);
 if(vx3dn_wform&&vx3dn_wp<vx3dn_wend){*vx3dn_wp++=w;return;}
 wr(io,0x440,w);
}
/* a raw data word (programs, tables, the FE block): direct in an FE03 block, or in an FE02 block unless it would end it (FE / CC: the
 * capture refuses those, cap_err 8) */
static void dat_slow(const vx3dn_io *io,uint32_t w) {
 if((vx3dn_wform==3||(vx3dn_wform==2&&(w>>24)!=0xFEUL&&(w>>24)!=0xCCUL))&&vx3dn_wp<vx3dn_wend){*vx3dn_wp++=w;return;}
 wr(io,0x440,w);
}
/* v10.282: the constant runs of the state set-up, left out whole once the engine holds them (the VxD sets vx3dn_cgroups after a whole
 * state set-up in this state generation: bit 0 type 1, bit 1 type 4). A run is a register no other write sets to anything else
 * (texture_setup, xp_setup and the point set-up write none of them). */
/* v10.287: the fast path inline (board, v10.286: the emitter's 24.9 microseconds a packet were about 35 instructions a word in reg and its
 * call, at about 2.4 cycles an instruction: state set-up 10.6, texture 5.7, program 3.9). REG and DAT store the word themselves when
 * the sink has room (vx3dn_wend is 0 while no block is open for direct words, so the compare fails then); else
 * reg_slow / dat_slow as before. A word with a constant register and value is one store and a pointer step. */
#define REG(io,r,v) do{uint32_t r_=(uint32_t)(r),v_=(uint32_t)(v);if(vx3dn_wp<vx3dn_wend)*vx3dn_wp++=(r_<<24)|(v_&0xFFFFFFUL);else reg_slow(io,r_,v_);}while(0)
#define DAT(io,x) do{uint32_t d_=(uint32_t)(x);if(vx3dn_wform==3&&vx3dn_wp<vx3dn_wend)*vx3dn_wp++=d_;else dat_slow(io,d_);}while(0)
uint32_t vx3dn_cgroups;
/* v10.286: where the emitter's time goes (board, v10.285: 26 microseconds to build a packet's 439 words with the command block
 * write-combining at 985 MB/s, so not the stores): vx3dn_tsc (the VxD sets it; NULL elsewhere) and vx3dn_eprof = TSC cycles / 256 in
 * the state set-up, texture set-up, program set-up and the rest (primitive set-up and vertices) */
uint32_t (*vx3dn_tsc)(void),vx3dn_eprof[4];
/* v10.461: the prepacked vertex copy's parts (with vx3dn_tsc, TSC cycles / 16): [0] the block copies (rep movsd from the caller's packet
 * into the command block), [1] their checks (pre_check), [2] words copied; one copy of 64 (8 vertices or more) is done a second time from
 * the same words, now in the cache: [3] those second copies, [4] their words, [5] the same calls' first copies, [6] their words. A cold
 * first copy against a hot second one says how much of the copy is the caller's words coming from memory. TEXDBG BC AE reads, AF clears. */
uint32_t vx3dn_vprof[8],vx3dn_vprof_n;
/* v10.221: how many idle status reads in a row count as idle (ACCEL_D3D_IDLE_READS, default 8; TEXDBG 0x80 + n sets n = 1..32) */
uint32_t vx3dn_idle_stable=8; /* v10.222: 8 (board, VXDISCO v1.6: 32, 8, 4 and 2 drew the same frame; 8 saved 22% of the VxD's time) */
extern void (*vx3dn_write_n)(const uint32_t *w,uint32_t n);
static int idle(const vx3dn_io *io,uint32_t *last) {
 uint32_t n,start,stable=0,need=vx3dn_idle_stable<1?1:vx3dn_idle_stable;
 /* v10.238: under the VxD's fetch capture every read is 0 (idle): the waits between bursts are in the fetched stream's order, not here */
 if(vx3dn_write_n){*last=0;return 1;}
 start=io->milliseconds(io->context);
 for(n=0;n<1000000UL;n++) {
  *last=io->read(io->context,0x400);
  if(*last==0xFFFFFFFFUL) return 0;
  if(!(*last&0x1FF7UL)) {if(++stable>=need) return 1;} else stable=0;
  if(!(n&15)&&(uint32_t)(io->milliseconds(io->context)-start)>=100) break; /* v10.220: the clock every 16 reads (a VMM call each) */
 }
 return 0;
}
extern uint32_t vx3dn_topo,vx3dn_strip_hw;
static uint32_t stip_pre(const vx3dn_io *io,const vx3dn_packet *p); /* v10.307 (below) */
/* v10.412 (board, v10.411: the state set-up 7.5 microseconds a packet): its words through a pointer and an end held in locals (lw_, le_),
 * loaded from the sink (vx3dn_wp, vx3dn_wend) after every block switch and every call that can write,
 * and put back before them: REG then compares two locals and stores, instead of three loads of globals, two compares and a store back.
 * The same words in the same order; a word that does not fit goes through reg_slow / dat_slow as before. */
#define LW_LOAD() do{lw_=vx3dn_wp;le_=vx3dn_wend;}while(0)
#define LW_SAVE() do{if(le_)vx3dn_wp=lw_;}while(0)
#define REGL(io,r,v) do{uint32_t r_=(uint32_t)(r),v_=(uint32_t)(v);if(lw_<le_)*lw_++=(r_<<24)|(v_&0xFFFFFFUL);else{LW_SAVE();reg_slow(io,r_,v_);LW_LOAD();}}while(0)
#define DATL(io,x) do{uint32_t d_=(uint32_t)(x);if(vx3dn_wform==3&&lw_<le_)*lw_++=d_;else{LW_SAVE();dat_slow(io,d_);LW_LOAD();}}while(0)
#define TYPEL(io,t) do{LW_SAVE();type(io,t);LW_LOAD();}while(0)
#define WRL(io,r,v) do{LW_SAVE();wr(io,r,v);LW_LOAD();}while(0)
#define C1VAR(k) (((k)>=0x23&&(k)<=0x26)||((k)>=0x28&&(k)<=0x2A)||((k)>=0x33&&(k)<=0x3F))
extern uint32_t vx3dn_lay8; /* .523 (below, with the 8-word form) */
/* .525 (ACCEL_D3D_SHADOW, default 1; TEXDBG BC E9 off, EA on, EB read): the state set-up's type-01 registers 03-8F (all but 29, the Coarse
 * Z tags, which a write resets) are written only when the value differs from the one the engine was last given. Board, GTA3 on .524:
 * about half the packets that miss the state cache send state set-up words identical to the packet before's; host replay: three
 * quarters of the type-01 writes repeat the register's value. sh_v / sh_m: each register's last value written by a set-up and whether it
 * is known; forgotten with the rest of the engine's state (vx3dn_forget: a reset, a refused or rebuilt group, anything that bumps the
 * state generation). Used only while the VxD has primed the engine in this generation (vx3dn_sh_ok, set and cleared by its build around
 * each packet, as vx3dn_cgroups); 00, 01 and 02 (the program enable, the cull and stipple word, the pulse) and 90-9B (the program's
 * registers, also written by the texture and program set-ups) always go. vx3dn_sh_stat: [0] set-ups with the shadow in use, [1] words
 * left out. */
volatile uint32_t vx3dn_shadow=1;uint32_t vx3dn_sh_ok,vx3dn_sh_stat[2];static uint32_t sh_v[256],sh_m[8];
#define SH1(k) ((k)>=3&&(k)<0x90&&(k)!=0x29)
#define REG1(io,r,v) do{uint32_t r1_=(uint32_t)(r),v1_=(uint32_t)(v)&0xFFFFFFUL;if(SH1(r1_)){if(vx3dn_sh_ok&&((sh_m[r1_>>5]>>(r1_&31))&1)&&sh_v[r1_]==v1_){vx3dn_sh_stat[1]++;break;}\
 sh_v[r1_]=v1_;sh_m[r1_>>5]|=1UL<<(r1_&31);}REGL(io,r1_,v1_);}while(0)
void vx3dn_sh_forget(void) {uint32_t k;for(k=0;k<8;k++)sh_m[k]=0;}
static void setup(const vx3dn_packet *p,const vx3dn_io *io) {
 static const uint32_t fe[16]={0x000E1004UL,0x08000249UL,0x0A0002C9UL,0x0B0002FBUL,
  0x0C000000UL,0x0D0002CBUL,0x0E000009UL,0x10000049UL,0x110002FFUL,0x12000008UL,
  0x130002DBUL,0x20000000UL,0x40008408UL,0x44000000UL,0x46000000UL,0x47000404UL};
 uint32_t k,*lw_,*le_;
 vx3dn_lay8=0xFFUL; /* .523: the set-up writes the vertex layout (04/00-04/04, 25, 28) */
 LW_LOAD();
 TYPEL(io,0x10);REGL(io,0x74,0);REGL(io,0x7C,0);REGL(io,0,0);REGL(io,2,0);REGL(io,7,0);REGL(io,0x68,0xA0000);
 TYPEL(io,0x11);REGL(io,0,0x248);TYPEL(io,0xFE);
 for(k=0;k<16;k++) DATL(io,fe[k]);
 TYPEL(io,1);if(vx3dn_sh_ok)vx3dn_sh_stat[0]++;
 /* Disable PS while replacing its instruction stream, as the lab does. */
 REG1(io,0,en00(p,0));REG1(io,1,0xCE1140UL|zbits(p));
 if(!(vx3dn_cgroups&1))for(k=3;k<=15;k++)REG1(io,k,0); /* v10.282: constant (vx3dn_cgroups) */
 REG1(io,2,2);REG1(io,2,0);
 REG1(io,0x10,(p->z_enable||stenciled(p))?p->depth.offset>>8:0);
 REG1(io,0x11,(p->z_enable||stenciled(p))?(p->depth.pitch>>5)<<13:0);
 /* XP's D32 path sets all three fixed-Z clamp bits. In particular Z=1.0
  * must become 0xFFFFFFFF, since unsigned fixed D32 cannot represent 1.0.
  * Match the XP field construction (format 4 | clamp mask 0x38). */
 /* v10.177: Z16 0x380004 and D24S8 0x3E0004 as XP writes them (CLXP3D T50, T52); D24S8's Z write mask leaves the stencil byte alone */
 REG1(io,0x12,p->depth.format==VX3DN_D16?0x380004UL:p->depth.format==VX3DN_D24S8?0x3E0004UL:0x3CUL<<16);REG1(io,0x13,(p->z_enable?p->z_func-1:7UL)<<16);
 /* Full D32 write mask and neutral Z-window defaults from sf_xp_state;
  * leaving these inherited from a previous owner can suppress Z writes. */
 REG1(io,0x14,p->depth.format==VX3DN_D24S8?0xFFFF00UL:0xFFFFFFUL);if(!(vx3dn_cgroups&1))REG1(io,0x15,0xFF);
 for(k=0x16;k<=0x18;k++)REG1(io,k,k==0x17&&xp_st(p)?0x80UL:0); /* v10.243: 01/17 = 80 for stencil, as XP */
 /* v10.315: 01/1C-1D (the occlusion count, HCM p83) are no longer written 0 here: a constant group sent again during an occlusion query
  * set the count back to 0. VX3DN_query sets it to 0 when a query begins. */
 if(!(vx3dn_cgroups&1)){REG1(io,0x19,0x800000);REG1(io,0x1A,0x800000);REG1(io,0x1B,0x803FBF);}
 /* Pixel Z clipping precedes the depth test even when depth is disabled:
  * far=1.0 (low24=800000, high8=3F), near=0. Keep occlusion writeback OFF. */
 if(!(vx3dn_cgroups&1)){REG1(io,0x1E,0x800000);REG1(io,0x1F,0);REG1(io,0x20,0x3F00);REG1(io,0x21,0);REG1(io,0x22,0);}
 for(k=0x23;k<=0x3F;k++) if((!(vx3dn_cgroups&1)||C1VAR(k))&&(k<0x28||k>0x2A)) REG1(io,k,0); /* v10.295: 28-2A written once below (a 29 of 0 then 018010 every draw is not XP's) */
 if(stenciled(p)){REG1(io,0x23,p->ext.stencil_w1);REG1(io,0x24,p->ext.stencil_w2);REG1(io,0x25,p->ext.stencil_w1);REG1(io,0x26,p->ext.stencil_w2);} /* v10.201 */
 /* v10.245: the stencil plane, as XP T52; v10.294: 01/2A is the Coarse Z base; v10.295: Coarse Z (T50): 29 = 018000 | pitch, the tags
  * reset first after a clear (008000, as T50 and T52 write it after the storage fill) */
 {uint32_t r29=xp_st(p)?st29(p):0;
  REG1(io,0x28,xp_st(p)?VX3DN_SPLANE_OFF(&p->depth)>>8:0);
  REG1(io,0x29,r29);REG1(io,0x2A,xp_st(p)?VX3DN_CZ_OFF(&p->depth)>>8:0);}
 if(full(p)&&(p->raster.blend_src||p->raster.alpha_func)) {
  /* v10.177: blend factors as XP encodes them (cookbook 16.7): 34 = 10000 | colour code << 4, 35 = 100 | code << 2, 36 = 10400 | alpha code << 4,
   * 37 = 200 | code << 2; alpha test: 01/00 bit 20 and 01/33 = (D3DCMP - 1) << 12 | ref * 1024 / 255 (XP T44; the lab's scaling, 1.0 = 0x400) */
  if(p->blend&&p->raster.blend_src) {
   uint32_t cs=blend_colour(p->raster.blend_src,1),cd=blend_colour(p->raster.blend_dst,0),as=blend_alpha(p->raster.blend_src,1),ad=blend_alpha(p->raster.blend_dst,0);
   /* v10.304 (HCM pp. 88-91): Cout = FCa x Ca op FCb x Cb; Ca 34[15:10] and Cb 35[13:8] (0 source, 1 destination), op 35[15:14] / 37[15:14]
    * (0 +, 1 -, 2 max, 3 min). SUBTRACT = src - dst; REVERSE_SUBTRACT = dst x its factor - src x its factor: Ca = destination with the
    * destination's factor, Cb = source with the source's; MIN and MAX take no factors (both ONE). */
   uint32_t op=(p->raster.blend_src>>5)&7,ca=0,cb=1,aa=1,ab=2,hop=0,ks=p->raster.blend_src>>8,kd=p->raster.blend_dst>>8;
   if(!blend_const(p->raster.blend_src))ks=0;if(!blend_const(p->raster.blend_dst))kd=0;
   if(op==VX3DN_BLENDOP_SUB)hop=1;
   else if(op==VX3DN_BLENDOP_REVSUB){uint32_t t;hop=1;ca=1;cb=0;aa=2;ab=1;t=cs;cs=cd;cd=t;t=as;as=ad;ad=t;t=ks;ks=kd;kd=t;}
   else if(op==VX3DN_BLENDOP_MIN||op==VX3DN_BLENDOP_MAX){hop=op==VX3DN_BLENDOP_MAX?2:3;cs=cd=0x15;as=ad=0x19;ks=kd=0;}
   REG1(io,0x34,0x10000UL|(ca<<10)|(cs<<4));REG1(io,0x35,(hop<<14)|(cb<<8)|(cd<<2));REG1(io,0x36,0x10000UL|(aa<<10)|(as<<4));REG1(io,0x37,(hop<<14)|(ab<<8)|(ad<<2));
   if(ks||kd){ /* the constant registers (HCM pp. 92-93); every set-up writes them 0 first (C1VAR), which ZERO and ONE need */
    /* FCa's constant: R 39[22:12], G 3A[22:12], B 3A[10:0]; FCb's: R 3C[22:12], G 3D[22:12], B 3D[10:0]; Ca / Cb's own constants
     * (38, 39[10:0], 3B, 3C[10:0]) and the alpha constants (3E, 3F) 0 */
    REG1(io,0x38,0);REG1(io,0x39,c11((ks>>16)&255)<<12);REG1(io,0x3A,(c11((ks>>8)&255)<<12)|c11(ks&255));
    REG1(io,0x3B,0);REG1(io,0x3C,c11((kd>>16)&255)<<12);REG1(io,0x3D,(c11((kd>>8)&255)<<12)|c11(kd&255));REG1(io,0x3E,0);REG1(io,0x3F,0);}
  } else {REG1(io,0x34,p->blend?0x020:0);REG1(io,0x35,p->blend?0x148:0);REG1(io,0x36,0);REG1(io,0x37,0);}
  if(p->raster.alpha_func) {REG1(io,0x33,((p->raster.alpha_func-1)<<12)|((p->raster.alpha_ref*1024UL+127)/255));REG1(io,0,en00(p,0x100000UL));}
 } else {REG1(io,0x34,p->blend?0x020:0);REG1(io,0x35,p->blend?0x148:0);REG1(io,0x36,0);REG1(io,0x37,0);}
 REG1(io,0x50,p->target.offset>>8);if(!(vx3dn_cgroups&1))REG1(io,0x51,0);
 /* XRGB has no alpha channel: destination alpha must read as 1.0 even
  * after an earlier draw wrote a nonopaque source alpha. HCM p95 / XP's
  * X8R8G8B8 target code is 98, whereas 99 is A8R8G8B8. */
 {uint32_t f=p->target.format==VX3DN_RGB565?0x89UL:p->target.format==VX3DN_ARGB8888?0x99UL:0x98UL, /* v10.460: 99 A8R8G8B8 */
  /* v10.295: bits 11:8 the logic op as a truth table of source 1100 and destination 1010 (C = COPY, the only one measured; the ICD's
   * glLogicOp: write_mask 0x100 | op << 4; board, v10.295: all 16 ops as worked out), bits 3:0 the channels written. v10.296 (board,
   * VXGL colour masks: red and blue came out swapped): B 1, G 2, R 4, A 8, the pixel's byte order; the packet keeps COLORWRITEENABLE's
   * R 1, G 2, B 4, A 8 */
  wm=p->write_mask,
  m=(dither53(p)?0:0x010000UL)|((wm&0x100UL)?((wm>>4)&15UL)<<8:0xC00UL)|(wm&10UL)|((wm&1UL)<<2)|((wm>>2)&1UL);
 REG1(io,0x52,(f<<16)|(p->target.pitch>>5));
 REG1(io,0x53,m);REG1(io,0x5B,m);REG1(io,0x63,m);REG1(io,0x6B,m); /* v10.201: bit 16 off = dither (XP T49) */
 if(dithered(p)){vx3dn_dither_last[0]=(f<<16)|(p->target.pitch>>5);vx3dn_dither_last[1]=m;vx3dn_dither_last[2]=en00(p,p->raster.alpha_func?0x100000UL:0);}}
 /* 80/82 are packed TOP:BOTTOM, 81/83 packed LEFT:RIGHT. Square-only
  * lab recipes conceal this distinction; program nonsquare targets fully. */
 REG1(io,0x80,((uint32_t)p->clip.top<<12)|(uint32_t)p->clip.bottom);
 REG1(io,0x81,((uint32_t)p->clip.left<<12)|(uint32_t)p->clip.right);
 REG1(io,0x82,((uint32_t)p->clip.top<<12)|(uint32_t)p->clip.bottom);
 REG1(io,0x83,((uint32_t)p->clip.left<<12)|(uint32_t)p->clip.right);
 if(!(vx3dn_cgroups&1))REG1(io,0x84,0);REG1(io,0x85,0);if(!(vx3dn_cgroups&1))REG1(io,0x88,0);REG1(io,0x89,p->target.width);if(!(vx3dn_cgroups&1))REG1(io,0x8A,0);
 for(k=0x58;k<=0x6B;k++)if((k&7)<4&&((k&7)==3||!(vx3dn_cgroups&1)))REG1(io,k,(k&7)==3?((dither53(p)?0:0x010000UL)|0xC0FUL):0);
 /* v10.196: fog sets 01/00 bits 11:8 (0xF00) as XP does (CLXP3D T16-T18: 01/00=000F0F); v10.195 set bit 8 only.
 * v10.195: fog (packet fog.r = registers 70..78 as XP writes them); without it, as before */
 for(k=0x70;k<=0x78;k++)REG1(io,k,fogged(p)?p->fog.r[k-0x70]:0);
 for(k=0x90;k<=0x9A;k++) REGL(io,k,k==0x90?0x58000UL:k==0x92?1:0);
 REGL(io,0x9B,0xFFFFFEUL);WRL(io,0x43C,0x20030000UL);
 DATL(io,0x01C00530UL);DATL(io,0x301C0053UL);DATL(io,0x00F04005UL);DATL(io,0x000D0820UL);
 /* The pixel program consumes interpolated diffuse color, not C0. */
 TYPEL(io,4);REGL(io,0,0xA80005UL);REGL(io,1,0x020100);REGL(io,2,0x000503);
 /* v10.282: 04/03, 04/04 are set by texture_setup and xp_setup; 04/05..10, 21, 22, 26, 27, 29, 2C..2F, 50..52 only here (vx3dn_cgroups bit 1) */
 for(k=3;k<=15;k++)if(k<5||!(vx3dn_cgroups&2))REGL(io,k,0);
 if(!(vx3dn_cgroups&2))REGL(io,0x10,0);REGL(io,0x20,0);if(!(vx3dn_cgroups&2)){REGL(io,0x21,0);REGL(io,0x22,0);}vx3dn_topo=0;
 REGL(io,0x25,0xF40000);if(!(vx3dn_cgroups&2)){REGL(io,0x26,0);REGL(io,0x27,0);}REGL(io,0x28,0xFD0000);if(!(vx3dn_cgroups&2)){REGL(io,0x29,0);REGL(io,0x2F,0x582000);
 REGL(io,0x2C,0);REGL(io,0x2D,0);REGL(io,0x2E,0);}
 REGL(io,0x40,0x800000);REGL(io,0x41,0x400000);REGL(io,0x42,0x493F);
 REGL(io,0x43,0x800000);REGL(io,0x44,0x400000);REGL(io,0x45,0x493F);
 REGL(io,0x46,0x800000);REGL(io,0x47,0);REGL(io,0x48,0x3F);
 if(!(vx3dn_cgroups&2)){REGL(io,0x50,0);REGL(io,0x51,0);REGL(io,0x52,0x147FFF);}
 /* No texturing. Invalidate/release the texture cache strobe. */
 WRL(io,0x43C,0xFE020000UL);
 /* Clear the documented shared texture/user-clip registers used by the
  * laboratory's clear_general recipe; leave reserved A06/A07/A0F alone. */
 for(k=1;k<=14;k++)if(k!=6&&k!=7)REGL(io,k,0);
 REGL(io,0,1);REGL(io,0,0);
 {uint32_t sb;LW_SAVE();sb=stip_pre(io,p);LW_LOAD();TYPEL(io,1);REGL(io,1,0xCE1540UL|zbits(p)|sb);vx3dn_r01=1;} /* v10.307: the stipple probe */
 LW_SAVE();
}
static uint32_t rgb565(uint32_t c) {return ((c>>8)&0xF800)|((c>>5)&0x7E0)|((c>>3)&31);}
/* v10.179: c is the word in the surface's own format, m the bits to replace (D24S8 Z-only keeps the stencil byte, stencil-only keeps Z:
 * read, merge, write); a full mask is a plain write. 2-byte formats are always replaced whole. */
static void clear_surface(const vx3dn_packet *p,const vx3dn_io *io,const vx3dn_surface *s,uint32_t c,uint32_t m) {
 uint32_t x,y,at,a,b,two=s->format==VX3DN_RGB565||s->format==VX3DN_D16;
 for(y=(uint32_t)p->clip.top;y<(uint32_t)p->clip.bottom;y++) {
  at=s->offset+y*s->pitch;
  if(two) {
   /* v10.167: whole pixel pairs are written, only an odd edge pixel is read back (a read of uncached video memory per pixel made a
    * 640x480 clear take most of a 100 ms frame on the board) */
   x=(uint32_t)p->clip.left;
   if((x&1)&&x<(uint32_t)p->clip.right){a=at+(x&~1UL)*2;b=io->read_fb(io->context,a);io->write_fb(io->context,a,(b&0xFFFFUL)|(c<<16));x++;}
   for(;x+1<(uint32_t)p->clip.right;x+=2)io->write_fb(io->context,at+x*2,c|(c<<16));
   if(x<(uint32_t)p->clip.right){a=at+x*2;b=io->read_fb(io->context,a);io->write_fb(io->context,a,(b&0xFFFF0000UL)|c);}
  } else if(m==0xFFFFFFFFUL) for(x=(uint32_t)p->clip.left;x<(uint32_t)p->clip.right;x++) io->write_fb(io->context,at+x*4,c);
  else for(x=(uint32_t)p->clip.left;x<(uint32_t)p->clip.right;x++) {a=at+x*4;io->write_fb(io->context,a,(io->read_fb(io->context,a)&~m)|(c&m));}
 }
 if(p->clip.right>p->clip.left&&p->clip.bottom>p->clip.top)
  (void)io->read_fb(io->context,s->offset+((uint32_t)p->clip.bottom-1)*s->pitch+(((uint32_t)p->clip.right-1)*(two?2:4)&~3UL));
}
void vx3dn_depth_word(const vx3dn_packet *p,uint32_t *v,uint32_t *m) {
 uint32_t f=p->depth.format,z=p->clear_depth,w=VX3DN_CLEAR_WHAT(p->clear_flags);
 *v=0;*m=0;
 if(f==VX3DN_D16){if(w&VX3DN_CLEAR_Z){*v=z>>16;*m=0xFFFFUL;}return;} /* v10.177: clear_depth is the 32-bit fraction */
 if(f==VX3DN_D32){if(w&VX3DN_CLEAR_Z){*v=z;*m=0xFFFFFFFFUL;}return;}
 /* v10.249: with the stencil plane (ACCEL_D3D_STENCIL_ZW 1, 2) the plane holds the stencil and the Z words' low byte is only what
  * a Lock shows (the HAL copies the plane there): a Z clear writes whole words (one fill, not the masked two), a stencil-only clear
  * none (the plane is filled by vx3dn_plane_clear or the 2D plane fill) */
 if(f==VX3DN_D24S8&&vx3dn_stencil_zw){
  /* v10.296 (board, VXGL.LOG at 16 bpp: 21 clears of 256x256, 2.2 ms each, all by the CPU): the free low byte takes bits 23:16, so the
   * usual clears (1.0, 0.0) are words of two equal halves the 2D engine fills at 16 bpp (it declined FFFFFF00) */
  if(w&VX3DN_CLEAR_Z){*v=(z&0xFFFFFF00UL)|((z>>16)&0xFFUL);*m=0xFFFFFFFFUL;}
  return;
 }
 if(f==VX3DN_D24S8){
  if(w&VX3DN_CLEAR_Z){*v|=z&0xFFFFFF00UL;*m|=0xFFFFFF00UL;} /* Z in the high 24 bits */
  if(w&VX3DN_CLEAR_STENCIL){*v|=VX3DN_CLEAR_SVAL(p->clear_flags);*m|=0xFFUL;}
 }
}
/* v10.245: a stencil clear of a D24S8 surface also fills its stencil plane (the engine's copy) over the clear rectangle */
void vx3dn_plane_clear(const vx3dn_packet *p,const vx3dn_io *io) {
 uint32_t so,sp,y,x,v,a,b,l,r;
 if(p->depth.format!=VX3DN_D24S8||!(p->clear_flags&VX3DN_CLEAR_STENCIL)||p->clip.right<=p->clip.left||p->clip.bottom<=p->clip.top)return;
 so=VX3DN_SPLANE_OFF(&p->depth);sp=VX3DN_SPLANE_PITCH(p->depth.width);v=VX3DN_CLEAR_SVAL(p->clear_flags)&255;v|=v<<8;v|=v<<16;
 l=(uint32_t)p->clip.left;r=(uint32_t)p->clip.right;
 for(y=(uint32_t)p->clip.top;y<(uint32_t)p->clip.bottom;y++) {
  for(x=l&~3UL;x<r;x+=4) {
   a=so+VX3DN_SPLANE_AT(sp,x,y); /* v10.246: tiled; 4 pixels from a multiple of 4 are 4 bytes of one tile row */
   if(x>=l&&x+4<=r)io->write_fb(io->context,a,v);
   else {uint32_t m=0,k;b=io->read_fb(io->context,a);for(k=0;k<4;k++)if(x+k>=l&&x+k<r)m|=0xFFUL<<(8*k);io->write_fb(io->context,a,(b&~m)|(v&m));}
  }
 }
}
/* v10.169: texture stage 0 from the cookbook's proven recipe (section "3D recipes", texturing; XP's captured state): general texture state,
 * texture 0 attributes, sampler stage 0 with the eight-zero sampler block, the "sample t0 -> output" program (TAU 301001B0, ALU
 * 01C00530 301C0053 00F20005 000D0820, A90..A9B) and the 7-word vertex layout (X Y Z W colour S T) with the texture-coordinate output mask.
 * Register 04/00 keeps XP's A8 high byte of this emitter's colour path with the recipe's attribute count 7. */
/* v10.304: sampler 35 / 36 = the border colour of unit u (HCM p126: s1.10, R 35[23:12] G 35[11:0], B 36[23:12] A 36[11:0]), read
 * where the address code is 0 */
static void border(const vx3dn_io *io,const vx3dn_packet *p,uint32_t u) {
 uint32_t c=full(p)?p->stages.border[u]:0;
 REG(io,0x35,(c11((c>>16)&255)<<12)|c11((c>>8)&255));REG(io,0x36,(c11(c&255)<<12)|c11(c>>24));
}
/* v10.450 (ACCEL_D3D_TEXONLY, the VxD's vx3dn_tex_only): the texture cache strobe of setup() alone, for a packet whose state differs from
 * the last packet's only in its textures (setup's other words, the program and the layout are what the engine already holds) */
static void tex_strobe(const vx3dn_io *io) {
 uint32_t k,*lw_,*le_;
 LW_LOAD();
 WRL(io,0x43C,0xFE020000UL);for(k=1;k<=14;k++)if(k!=6&&k!=7)REGL(io,k,0);REGL(io,0,1);REGL(io,0,0);
 LW_SAVE();
}
/* v10.450: units_only: the texture units' words alone (the general texture state, each unit's attributes, sampler and level block),
 * without the default program and 7-word layout after them (unchanged in a textures-only packet: the engine holds the last packet's) */
/* .527: .526's ACCEL_D3D_TEXSHADOW (the texture set-up's per-unit registers written only when changed) removed: board, .526: the
 * compares cost more than the 7 words a set-up they saved (GTA3 A/B: 0.61 us a packet faster without). Every texture register goes. */
static void texture_setup(const vx3dn_packet *p,const vx3dn_io *io,int units_only) {
 uint32_t *lw_=vx3dn_wp,*le_=vx3dn_wend; /* v10.412 (above setup) */
 const vx3dn_surface *t=&p->tex.surface;uint32_t k,fl,mp;
 /* v10.173: the general texture state as XP's setup writes it (CLXP3D T00: 08 = 010102, 0D = 810000, 10 = 800000); with the lab's
  * 08 = 000101 and 0D/10 left 0, sampler address code 2 (XP's WRAP) clamps on the board (v10.170, v10.172). */
 {uint32_t n=units(p),u,fe1=0;for(u=0;u<n;u++)fe1|=((u==2&&c3_fe2(p))?c3_fe2(p):(0x21UL|((n>1?p->stages.set[u]:0)<<2)))<<(8*u); /* v10.206: T116 00=21 01=2521, T149 00=31 01=212521; v10.311: unit 2 on TextureC (29); v10.312: the probe's */
 if(full(p)&&p->stages.layout)fe1=(fe1&~0xFFUL)|0x22UL; /* v10.208: S, T, R (T150/T151 212522) */
 if(mixed3(p)&&p->stages.ulayout[0])fe1=(fe1&~0xFF00UL)|((0x22UL|(p->stages.set[1]<<2))<<8); /* v10.321: unit 1 S, T, R from its set */
 /* v10.207: HCM p128: FE/01 byte u = dimension 1:0 (01 = S, T), source 4:2 (0 TextureA, 1 TextureB), perspective 6:5 (01). The readback
  * (TEXDBG 0x54) keeps what the last textured draw sent: FE/01, units, set 1, unit 1's source set, the first vertex's uv0 and uv1. */
 vx3dn_set_last[0]=fe1;vx3dn_set_last[1]=n;vx3dn_set_last[2]=n>1?p->stages.set[1]:0;{const uint32_t *pw=p==vx3dn_vsrc_pkt?vx3dn_vsrc:p->pre.w; /* v10.449 */
 vx3dn_set_last[3]=p->pre.on?pw[6]:p->tex.uv[0][0];vx3dn_set_last[4]=p->pre.on?pw[7]:p->tex.uv[0][1];
 vx3dn_set_last[5]=n>1?(p->pre.on?pw[8]:p->stages.uv1[0][0]):0;vx3dn_set_last[6]=n>1?(p->pre.on?pw[9]:p->stages.uv1[0][1]):0;}
 WRL(io,0x43C,0xFE020000UL);REGL(io,0x00,(n<<4)|1);REGL(io,0x01,fe1);}REGL(io,0x08,mixed3(p)?(2UL|((p->stages.s3&1)?0x200UL:0x100UL)|((p->stages.s3&2)?0x20000UL:0x10000UL)):(cube(p)||volume(p))?0x010201UL:0x010102UL); /* v10.214: T150 08 = 010201 (texture A three-dimensional); v10.317: a volume's too (HCM p132); v10.321: each set's dimension */REGL(io,0x04,0);REGL(io,0x0D,0x810000UL);REGL(io,0x10,0x800000UL);
 /* v10.199: 20 = height << 12 | width and 22 = log2 height << 4 | log2 width (CLXP3D T128, 2048x8: 20 = 008800, 22 = 00003B); earlier
  * versions swapped them, which only squares survived. DXT sets 22 bit 15 (T33-T35). */
 WRL(io,0x43C,0x00020000UL);
 /* v10.214: a cube map (T150): 02/00..05 = the six faces' level-0 bases (HCM p116), 18 = 001300 (mode 3 = cube, HCM p117) */
 /* v10.317: a 3D texture (HCM p117-118): 18 = 001200 (mode 2, volume), 21 = its depth, 22 bits 11:8 = log2 depth; its slices follow slice 0 */
 /* v10.318 (board, v10.317: every slice drew as slice 0, VXGL 48 and VXTEX 15): XP's ICD (s3gIGPgl.dll, its texture set-up) sets 22 bit 15
  * (width and height powers of two) for a volume and writes no level offsets for it; HCM p117: with bit 15 the sampler takes the offset
  * mode for its addresses. Board, v10.319: with bit 15 every slice draws right, without it every slice draws as slice 0: bit 15 is what
  * the sampler needs (the probe was removed in .530) */
 if(cube(p)){for(k=0;k<6;k++)REGL(io,k,p->stages.face[k]>>8);REGL(io,0x18,0x1300);}else{REGL(io,0x00,t->offset>>8);REGL(io,0x18,volume(p)?0x1200UL:0x1100UL);}
 REGL(io,0x20,(t->height<<12)|t->width);REGL(io,0x21,volume(p)?p->stages.face[0]:1);
 REGL(io,0x22,(log2u(t->height)<<4)|log2u(t->width)|(volume(p)?(log2u(p->stages.face[0])<<8)|0x8000UL:0)|(tex_dxt(t->format)?0x8000UL:0));REGL(io,0x30,tex_code(t->format));
 /* HCM manual p125: S/T enlargement at 13/7, shrinking at 10/4. */
 fl=(p->tex.filter==VX3DN_FILTER_LINEAR||p->tex.filter==VX3DN_FILTER_MAG_LINEAR)?((1UL<<13)|(1UL<<7)):0;
 if(p->tex.filter==VX3DN_FILTER_LINEAR||p->tex.filter==VX3DN_FILTER_MIN_LINEAR)fl|=(1UL<<10)|(1UL<<4);
 mp=p->tex.address_u|(p->tex.address_v<<3);
 /* v10.199: anisotropic shrinking at 11/5 with the maximum at 20:16 (T47: 31 = 020820); mipmaps: 30 = 000140 | bias << 16, 31 bits 1:0 =
  * filter, 32 | 090000, level offsets in block 10030000 (T21: 30 = 000140 31 = 012491 32 = 090092, block 10 14 15 16 17) */
 {uint32_t an=(full(p)&&vx3dn_aniso)?p->mip.aniso:1,s30=0;if(an<1)an=1;
  if(an>1)fl=(fl&~((1UL<<10)|(1UL<<4)))|(1UL<<11)|(1UL<<5);
  /* v10.304: 30 bits 11:6 = the maximum level (HCM p124): the chain's last level, at most 8 (block 10030000 holds levels 1-8, HCM p138;
   * levels 9-11 have no offsets of their own); XP's 000140 (T21) is its 6-level chain's 5. It was 5 for every chain: a 7- to 9-level
   * chain never sampled its last levels. */
  if(mipped(p)){uint32_t ml=p->mip.levels-1;if(ml>8)ml=8;s30=(ml<<6)|(p->mip.bias<<16);fl|=p->mip.filter;mp|=0x090000UL;}
  /* v10.317: a volume's R axis (sampler 32 bits 8:6, the 2D draws' 0x80 = repeat) and R filters (21:19 enlargement, 18:16 shrinking:
   * 001 linear), as its S and T */
  if(volume(p)){mp|=p->stages.face[2]<<6;if(fl&(1UL<<13))mp|=0x080000UL;if(fl&(1UL<<10))mp|=0x010000UL;}else mp|=0x80UL;
  WRL(io,0x43C,0x20020000UL);REGL(io,0x30,s30);REGL(io,0x31,fl|(an<<16));REGL(io,0x32,mp);LW_SAVE();border(io,p,0);LW_LOAD();}
 WRL(io,0x43C,0x10030000UL);for(k=0;k<8;k++)DATL(io,(mipped(p)&&k+1<p->mip.levels)?p->mip.level[k]:0);
 /* v10.206: units 1 and 2: texture (u << 8 | 02), sampler (u + 20) << 8 | 02, no mip chain (block 10030000 + 8u), as XP T116/T149 */
 {uint32_t u;for(u=1;u<units(p);u++){const vx3dn_unit *n=&p->stages.unit[u-1];const vx3dn_surface *q=&n->surface;uint32_t f2;
  uint32_t ul=full(p)?p->stages.ulayout[u-1]:0; /* v10.321: a cube (faces, mode 3) or a volume (mode 2, its depth, bit 15) on the unit, as on unit 0 */
  WRL(io,0x43C,0x00020000UL|(u<<24));
  if(ul==VX3DN_LAYOUT_CUBE){for(k=0;k<6;k++)REGL(io,k,p->stages.uface[u-1][k]>>8);REGL(io,0x18,0x1300);}else{REGL(io,0x00,q->offset>>8);REGL(io,0x18,ul==VX3DN_LAYOUT_VOLUME?0x1200UL:0x1100UL);}
  REGL(io,0x20,(q->height<<12)|q->width);REGL(io,0x21,ul==VX3DN_LAYOUT_VOLUME?p->stages.uface[u-1][0]:1);
  REGL(io,0x22,(log2u(q->height)<<4)|log2u(q->width)|(ul==VX3DN_LAYOUT_VOLUME?(log2u(p->stages.uface[u-1][0])<<8)|0x8000UL:0)|(tex_dxt(q->format)?0x8000UL:0));REGL(io,0x30,tex_code(q->format));
  f2=(n->filter==VX3DN_FILTER_LINEAR||n->filter==VX3DN_FILTER_MAG_LINEAR)?((1UL<<13)|(1UL<<7)):0;
  if(n->filter==VX3DN_FILTER_LINEAR||n->filter==VX3DN_FILTER_MIN_LINEAR)f2|=(1UL<<10)|(1UL<<4);
  /* v10.304: the unit's mip chain as unit 0's (30 = bias << 16 | maximum level << 6, 31 bits 1:0 the mip filter, 32 | 090000, block
   * 10030000 + 8u its level offsets) */
  {uint32_t nl=full(p)?p->stages.umip[u-1]&255:1,s30=0,m2=n->address_u|(n->address_v<<3)|(ul==VX3DN_LAYOUT_VOLUME?(p->stages.uface[u-1][2]<<6)|((f2&(1UL<<13))?0x080000UL:0)|((f2&(1UL<<10))?0x010000UL:0):0x80UL);
   if(vx3dn_mipon&&nl>1){uint32_t ml=nl-1;if(ml>8)ml=8;s30=(ml<<6)|(p->stages.ubias[u-1]<<16);f2|=(p->stages.umip[u-1]>>8)&3;m2|=0x090000UL;}else nl=1;
   {uint32_t an=vx3dn_aniso?(p->stages.umip[u-1]>>16):1;if(an<1)an=1;
    if(an>1)f2=(f2&~((1UL<<10)|(1UL<<4)))|(1UL<<11)|(1UL<<5);
    WRL(io,0x43C,0x20020000UL|(u<<24));REGL(io,0x30,s30);REGL(io,0x31,f2|(an<<16));REGL(io,0x32,m2);LW_SAVE();border(io,p,u);LW_LOAD();}
   WRL(io,0x43C,0x10030000UL|(u*8));for(k=0;k<8;k++)DATL(io,k+1<nl?p->stages.ulevel[u-1][k]:0);}}}
 if(units_only){LW_SAVE();return;} /* v10.450 */
 TYPEL(io,1);REGL(io,0x01,0xCE1140UL|zbits(p));
 REGL(io,0x90,0x18000UL);REGL(io,0x91,1);REGL(io,0x92,0x10001UL);REGL(io,0x93,1);for(k=0x94;k<=0x9A;k++)REGL(io,k,0);REGL(io,0x9B,0xFFFFFEUL);
 WRL(io,0x43C,0x21030000UL);DATL(io,0x301001B0UL);
 WRL(io,0x43C,0x20030000UL);DATL(io,0x01C00530UL);DATL(io,0x301C0053UL);DATL(io,0x00F20005UL);DATL(io,0x000D0820UL);
 {uint32_t sb;LW_SAVE();sb=stip_pre(io,p);LW_LOAD();TYPEL(io,1);REGL(io,0x01,0xCE1540UL|zbits(p)|sb);vx3dn_r01=1;}
 TYPEL(io,4);REGL(io,0x00,0xA80007UL);REGL(io,0x01,0x020100);REGL(io,0x02,0x080503);REGL(io,0x03,0x09);REGL(io,0x25,0xF4C000UL);REGL(io,0x28,0xFDC000UL);
 LW_SAVE();
}
/* v10.172: XP's program path (section 16 of the cookbook). The program goes in with the pixel shader disabled, as texture_setup does;
 * 01/92 = 0x10000 | instruction count; 01/9B is a tag that changes whenever the program differs from the last one loaded (XP lowers it for
 * every new program). Then XP's 10-attribute vertex layout, including 04/2A and 04/2B = 1.0, which the 7-word layout never set. */
static uint32_t xp_last[32],xp_last_count,xp_tag=0xFFFFFEUL;
/* v10.210: after an engine reset nothing the engine held is trusted: the next program gets a new 01/9B tag */
/* v10.232: vx3dn_state_gen counts every event after which the engine's 3D state may differ from the last packet's (the VxD's state cache
 * compares it); vx3dn_vertices_only: emit the primitive set-up and vertices only */
uint32_t vx3dn_state_gen,vx3dn_vertices_only;
/* v10.450: vx3dn_tex_only: the VxD found this packet's state the last packet's but for its textures: the strobe and the texture units only
 * (cleared here when the packet is not textured, then the whole set-up goes) */
uint32_t vx3dn_tex_only;
void vx3dn_forget(void) {xp_last_count=0xFFFFFFFFUL;vx3dn_state_gen++;vx3dn_r01=0;vx3dn_lay8=0xFFUL;vx3dn_sh_forget();} /* .525: the shadow */ /* .523: lay8 */
/* v10.197: registry ACCEL_D3D_CKEY (default 1); 0 draws colour-keyed textures without the key (the program's last instruction only) */
volatile uint32_t vx3dn_ckey=1;
/* v10.206: TAU word of unit u. v10.208 experiment (TEXDBG 0x60+n) for unit 1: 0 XP's 311111B1, 1 311011B1, 2 311001B1 (the coordinate
 * fields of the bump reads, T146/T147), 3 XP's word with FE/01 texture 1 from TextureA. A projected unit 0 (layout 1, TTFF PROJECTED) is
 * T151's 307001A0. */
static uint32_t tau_word(const vx3dn_packet *p,uint32_t u) {
 if(u==0&&full(p)&&p->stages.layout==2)return 0x307001A0UL;
 return 0x301001B0UL+u*0x01011001UL;
}
/* v10.303 experiment (TEXDBG 0x40 + n; board, VXGL v3.2 scene 27: GL_BLEND on LUMINANCE and INTENSITY textures wrong): where a texture
 * register's last read is marked in the last instruction that reads it (A w2 bit 17, B w1 bit 21, C w0 bit 25). 0 as the core sent it,
 * 1 the last operand that reads it (C, else B, else A), 2 every operand that reads it, 3 the first (A, else B, else C), 4 A whatever reads
 * it (the core's mark before v10.303) */
/* v10.308: polygon stipple: block 14030000 holds the 32x32 pattern, a word a row; 01/01 bit 1 enables it (the v10.307 probe found it) */
static uint32_t stip_pre(const vx3dn_io *io,const vx3dn_packet *p) {
 uint32_t k;
 if(full(p)&&p->op==VX3DN_DRAW&&p->stipple.on){wr(io,0x43C,0x14030000UL);for(k=0;k<32;k++)DAT(io,p->stipple.rows[k]);return 2;} /* v10.308 */
 return 0;
}
static void xp_setup(const vx3dn_packet *p,const vx3dn_io *io) {
 uint32_t *lw_=vx3dn_wp,*le_=vx3dn_wend; /* v10.412 (above setup) */
 uint32_t ckey=(p->shader.kflags&2)&&vx3dn_ckey,count=ckey||!(p->shader.kflags&2)?p->shader.count:p->shader.count-5;
 const uint32_t *words=p->shader.words+(p->shader.count-count)*4;
 uint32_t k,n=count*4,zb=zbits(p),same=count==xp_last_count,zbits=zb;int ps;
 vx3dn_lay8=0; /* .523: the 10-word (or its other forms') layout written below */
 for(k=0;k<n&&same;k++)if(words[k]!=xp_last[k])same=0;
 if(!same){xp_tag=(xp_tag-1)&0xFFFFFFUL;if(xp_tag<0x800000UL)xp_tag=0xFFFFFEUL;xp_last_count=count;for(k=0;k<n;k++)xp_last[k]=words[k];}
 TYPEL(io,1);REGL(io,0x01,0xCE1140UL|zbits);
 /* v10.194: an untextured program (T13 diffuse, T14 diffuse + specular) has no texture unit: XP sends 90=058000 91=0 92=count 93=0 and
  * FE/00=1 (CLXP3D T13/T14). The textured header (one TAU: 90=018000 91=1 92=010000|count 93=1) made the engine wait for a texture that
  * was never set up: WinTune's first untextured specular triangle hung the 3D engine (board, v10.193: wait 3 timed out, status 04000701). */
 ps=full(p)&&p->stages.ps;
 if(ps){REGL(io,0x90,p->stages.h90);REGL(io,0x91,p->stages.h91);REGL(io,0x92,(p->stages.ntau<<16)|count);REGL(io,0x93,p->stages.h93);} /* v10.208: T73, T125, T146, T147 */
 else if(ckey){REGL(io,0x90,0x18000UL);REGL(io,0x91,1);REGL(io,0x92,0x20000UL|count);REGL(io,0x93,3);} /* v10.197: CLXP3D T45 */
 else if(textured(p)){uint32_t n=units(p);REGL(io,0x90,0x18000UL);REGL(io,0x91,(1UL<<n)-1);REGL(io,0x92,(n<<16)|count);REGL(io,0x93,n==3?4UL:n);} /* v10.206: T116, T149 */
 else {REGL(io,0x90,0x58000UL);REGL(io,0x91,0);REGL(io,0x92,count);REGL(io,0x93,0);}
 for(k=0x94;k<=0x9A;k++)REGL(io,k,ps&&k==0x95?p->stages.h95:(!ps&&ckey&&k==0x95)?0x10UL:0);REGL(io,0x9B,xp_tag);
 WRL(io,0x43C,0x20030000UL);for(k=0;k<n;k++)DATL(io,words[k]);
 if(ps){WRL(io,0x43C,0x21030000UL);for(k=0;k<p->stages.ntau;k++)DATL(io,p->stages.tau[k]);
  for(k=0;k<p->stages.nkc;k++){uint32_t j;WRL(io,0x43C,0x22030000UL|(p->stages.kc_reg[k]*4UL));for(j=0;j<4;j++)DATL(io,p->stages.kc[k][j]);}}
 else if(ckey){static const uint32_t bank0[8]={0x3F000000UL,0x3F800000UL,0x40000000UL,0x40800000UL,0x3B000000UL,0,0,0}; /* 0.5 1 2 4 1/512 */
  WRL(io,0x43C,0x21030000UL);DATL(io,0x201001B0UL);DATL(io,0x314001B1UL);
  WRL(io,0x43C,0x22030000UL);for(k=0;k<8;k++)DATL(io,bank0[k]);
  WRL(io,0x43C,0x220300ACUL);for(k=0;k<4;k++)DATL(io,p->shader.key[k]);}
 else if(textured(p)){WRL(io,0x43C,0x21030000UL);for(k=0;k<units(p);k++)DATL(io,tau_word(p,k));}
 if(p->shader.kflags&1){WRL(io,0x43C,0x22030008UL);for(k=0;k<4;k++)DATL(io,p->shader.konst[k]);}
 if(full(p)&&p->stages.c22){static const uint32_t c22[4]={0x3EA2F984UL,0xBF000000UL,0x3EE8BA2EUL,0};WRL(io,0x43C,0x22030058UL);for(k=0;k<4;k++)DATL(io,c22[k]);} /* v10.206 */
 {uint32_t sb;LW_SAVE();sb=stip_pre(io,p);LW_LOAD();TYPEL(io,1);REGL(io,0x01,0xCE1540UL|zbits|sb);vx3dn_r01=1;} /* v10.307: the stipple probe */
 if(!textured(p)){WRL(io,0x43C,0xFE020000UL);REGL(io,0x00,1);} /* v10.194: XP's FE/00 = 1 for an untextured program (0x11 textured) */
 if(mixed3(p)){ /* v10.321: S_A T_A [R_A] S_B T_B [R_B]: attribute codes 08 09 0A and 0C 0D 0E (TextureA's S T R Q at 08-0B, TextureB's at
  * 0C-0F, as the layouts above: 04/03 = 0A0908 for T150's s t r, 0C0908 0D for the two sets), three to a register after X Y Z W diffuse
  * specular; 04/25 and 04/28 a bit each at 23 - code (C000 TextureA's S T, 2000 its R, 0C00 TextureB's S T, 0200 its R) */
  uint32_t c[16],nc=0,k2,m=0;static const uint32_t base[6]={0,1,2,3,5,6};
  for(k2=0;k2<6;k2++)c[nc++]=base[k2];
  c[nc++]=8;c[nc++]=9;if(p->stages.s3&1)c[nc++]=10;c[nc++]=12;c[nc++]=13;if(p->stages.s3&2)c[nc++]=14;
  for(k2=6;k2<nc;k2++)m|=1UL<<(23-c[k2]);
  TYPEL(io,4);REGL(io,0x00,0xA80000UL|nc);
  for(k2=0;k2<nc;k2+=3)REGL(io,1+k2/3,c[k2]|(k2+1<nc?c[k2+1]<<8:0)|(k2+2<nc?c[k2+2]<<16:0));
  REGL(io,0x25,0xF60000UL|m);REGL(io,0x28,0xFF0000UL|m);REGL(io,0x2A,0x3F8000UL);REGL(io,0x2B,0x3F8000UL);LW_SAVE();return;}
 if(full(p)&&p->stages.layout){TYPEL(io,4);REGL(io,0x00,0xA80009UL);REGL(io,0x01,0x020100UL);REGL(io,0x02,0x060503UL);REGL(io,0x03,0x0A0908UL);REGL(io,0x04,0x00000DUL);
  REGL(io,0x25,0xF6E000UL);REGL(io,0x28,0xFFE000UL);REGL(io,0x2A,0x3F8000UL);REGL(io,0x2B,0x3F8000UL);LW_SAVE();return;} /* v10.208: T150/T151 */
 if(c3_layout(p)){TYPEL(io,4);REGL(io,0x00,0xA8000CUL);REGL(io,0x01,0x020100UL);REGL(io,0x02,0x060503UL);REGL(io,0x03,0x0C0908UL);REGL(io,0x04,0x11100DUL);
  REGL(io,0x25,0xF6CCC0UL);REGL(io,0x28,0xFFCCC0UL);REGL(io,0x2A,0x3F8000UL);REGL(io,0x2B,0x3F8000UL);LW_SAVE();return;} /* v10.311; v10.312: the probe's */
 TYPEL(io,4);REGL(io,0x00,0xA8000AUL);REGL(io,0x01,0x020100UL);REGL(io,0x02,0x060503UL);REGL(io,0x03,0x0C0908UL);REGL(io,0x04,0x00000DUL);
 /* v10.209: 04/28 (HCM p160) says which vertex parameters the vertex carries; the rest take defaults. Bits 11:10 are coordinate set 1
  * (TextureB) S and T: with them clear, texture 1 on set 1 read (0, 0) (board, v10.208). XP sends FFCC00 for a unit on set 1 (CLXP3D T24). */
 {uint32_t n=units(p),u,b=0;if(full(p)&&n>1)for(u=0;u<n;u++)if(p->stages.set[u])b=0xC00UL;
 REGL(io,0x25,0xF6CC00UL);REGL(io,0x28,0xFFC000UL|b);}REGL(io,0x2A,0x3F8000UL);REGL(io,0x2B,0x3F8000UL);
 LW_SAVE();
}
/* v10.174: the wrap experiment (four quads). The mode's writes are the last ones on the draw, after texture setup:
 * 1 texture 22 bit 15 (HTXnPower2), 2 sampler 32 = 000089 (clamp on purpose) + FE 0D = 000001 (modulus on texture A),
 * 3 FE 0B = 00000C (wrap correction S/T), 4 sampler 32 = 000089 + FE 0D = 030001 (modulus 8). */
/* v10.192: which wait timed out when a draw comes back lost (VxD last_result bits 15:8): 1 the engine was already busy before the packet,
 * 2 after the state set-up, 3 + n after triangle n */
extern uint32_t vx3dn_vertices_only;
/* v10.236: the vertex format of a draw, worked out once; one vertex built in w[] and sent as one run (vx3dn_write_n, the VxD's fetch
 * capture; v10.232), else word by word */
typedef struct {int xp,tx,stq,c3,m3;uint32_t un,nw;} vx_vfmt; /* v10.321: m3 = the two-unit 3D layout's sets (stages.s3) */
void (*vx3dn_write_n)(const uint32_t *w,uint32_t n);
/* v10.238: the VxD's fetch capture can hand out room for a run of vertex words in the command block (NULL: not here, then a vertex at
 * a time as before); the vertices are then built straight into it */
uint32_t *(*vx3dn_reserve)(uint32_t n);
/* v10.315: every word is worked out first and the vertex written in order, w[0] to w[n - 1]: w is the command block, write-combining
 * memory, where a word written out of order (diffuse before W, as before) can close a partly filled line early (board, v10.313: building
 * a packet's vertex words was 33 of its 56 microseconds) */
static void build_vertex(const vx3dn_packet *p,const vx_vfmt *f,uint32_t i,uint32_t *w) {
 const vx3dn_vertex *a=&p->vertices[i];uint32_t x=a->x,y=a->y,z=a->z,c=a->color;
 if(f->xp) { /* X Y Z W(=rhw) diffuse specular u0 v0 u1 v1 [u2 v2] */
  uint32_t rhw=a->rhw,sp=p->shader.spec[i],u=f->tx?p->tex.uv[i][0]:0,t=f->tx?p->tex.uv[i][1]:0,u1=f->un>1?p->stages.uv1[i][0]:u,t1=f->un>1?p->stages.uv1[i][1]:t,w6,w7,w8,w9;
  if(f->m3){uint32_t n=8;w[0]=x;w[1]=y;w[2]=z;w[3]=rhw;w[4]=c;w[5]=sp;w[6]=u;w[7]=t; /* v10.321: S_A T_A [R_A] S_B T_B [R_B] */
   if(f->m3&1)w[n++]=p->stages.q[i];w[n++]=u1;w[n++]=t1;if(f->m3&2)w[n++]=p->stages.r1[i];return;}
  if(f->stq){w6=u;w7=t;w8=p->stages.q[i];w9=0;} /* v10.208: s t q */
  else {w6=u;w7=t;w8=u1;w9=t1;}
  w[0]=x;w[1]=y;w[2]=z;w[3]=rhw;w[4]=c;w[5]=sp;w[6]=w6;w[7]=w7;w[8]=w8;
  if(f->stq)return;
  w[9]=w9;
  if(f->c3){uint32_t u2=p->stages.set[2]?u1:u,t2=p->stages.set[2]?t1:t;w[10]=u2;w[11]=t2;} /* v10.311: TextureC, unit 2's; v10.312 probe: the marker (0.25, 0.75) */
 } else {
  w[0]=x;w[1]=y;w[2]=z;w[3]=0x3F800000UL;w[4]=c;
  if(f->tx){w[5]=p->tex.uv[i][0];w[6]=p->tex.uv[i][1];}
 }
}
/* v10.317: the two common layouts as plain loops (ACCEL_D3D_VFAST, default 1; 0: build_vertex for every vertex as before): XP's 10 words
 * (X Y Z W diffuse specular u0 v0 u1 v1, no s t q, no swapped sets, no TextureC) and the 2D path's 5 or 7. The same words in the same
 * order as build_vertex; the format's tests and pointer choices once a run, not once a vertex (board, v10.316: about 35 microseconds a
 * packet building vertex words) */
uint32_t vx3dn_vfast=1;
static int fast_ok(const vx_vfmt *f) {return vx3dn_vfast&&!f->stq&&!f->c3&&!f->m3;}
static void fast_vertices(const vx3dn_packet *p,const vx_vfmt *f,uint32_t first,uint32_t count,uint32_t *d) {
 static const uint32_t zero[2]={0,0};
 const vx3dn_vertex *a=&p->vertices[first];uint32_t k;
 if(f->xp) {
  const uint32_t *sp=&p->shader.spec[first];
  const uint32_t *u0=f->tx?p->tex.uv[first]:zero,*u1=f->un>1?p->stages.uv1[first]:u0;
  uint32_t s0=f->tx?2:0,s1=f->un>1?2:s0;
  for(k=0;k<count;k++,a++,d+=10,u0+=s0,u1+=s1){d[0]=a->x;d[1]=a->y;d[2]=a->z;d[3]=a->rhw;d[4]=a->color;d[5]=sp[k];d[6]=u0[0];d[7]=u0[1];d[8]=u1[0];d[9]=u1[1];}
  return;
 }
 if(f->tx){const uint32_t *u0=p->tex.uv[first];
  for(k=0;k<count;k++,a++,d+=7,u0+=2){d[0]=a->x;d[1]=a->y;d[2]=a->z;d[3]=0x3F800000UL;d[4]=a->color;d[5]=u0[0];d[6]=u0[1];}}
 else for(k=0;k<count;k++,a++,d+=5){d[0]=a->x;d[1]=a->y;d[2]=a->z;d[3]=0x3F800000UL;d[4]=a->color;}
}
/* v10.440: the packet's word count by part, for the VxD's command-density profile (TEXDBG BC 9E-A1): vx3dn_mark(k) at the state set-up's
 * start (0) and end (1), after the texture set-up (2) and after the program set-up (3), NULL when not profiling; vx3dn_geo = vertex words,
 * vertices, triangles and strip vertices sent (the VxD reads them around each packet) */
void (*vx3dn_mark)(uint32_t k);
uint32_t vx3dn_geo[4];
/* vertices first .. first + count - 1, f->nw words each */
static void put_vertices(const vx3dn_packet *p,const vx3dn_io *io,const vx_vfmt *f,uint32_t first,uint32_t count) {
 uint32_t w[12],k,j,*d;
 vx3dn_geo[0]+=count*f->nw;vx3dn_geo[1]+=count; /* v10.440 */
 if(p->pre.on&&p==vx3dn_vsrc_pkt){const uint32_t *s=vx3dn_vsrc+first*10; /* v10.449: from the caller's packet: copied once, the copy tested */
  d=vx3dn_reserve&&vx3dn_copyw?vx3dn_reserve(count*10):0;
  if(d){uint32_t t0=vx3dn_tsc?vx3dn_tsc():0,t1=0;
   vx3dn_copyw(d,s,count*10);
   if(vx3dn_tsc){t1=vx3dn_tsc();vx3dn_vprof[0]+=(t1-t0+8)>>4;vx3dn_vprof[2]+=count*10;
    if(count>=8&&!(++vx3dn_vprof_n&63)){static uint32_t hot[VX3DN_IX_VERTICES*10];uint32_t t2; /* the same words copied again, now in the cache */
     vx3dn_vprof[5]+=(t1-t0+8)>>4;vx3dn_vprof[6]+=count*10;t2=vx3dn_tsc();vx3dn_copyw(hot,s,count*10);vx3dn_vprof[3]+=(vx3dn_tsc()-t2+8)>>4;vx3dn_vprof[4]+=count*10;}
    t1=vx3dn_tsc();}
   if(!pre_check(p,d,count))vx_vbad=1;
   if(vx3dn_tsc)vx3dn_vprof[1]+=(vx3dn_tsc()-t1+8)>>4;
   return;}
  for(k=0;k<count;k++,s+=10){for(j=0;j<10;j++)w[j]=s[j];if(!pre_check(p,w,1)){vx_vbad=1;return;}
   if(vx3dn_write_n)vx3dn_write_n(w,10);else for(j=0;j<10;j++)DAT(io,w[j]);}
  return;}
 if(p->pre.on){const uint32_t *s=&p->pre.w[first*10]; /* v10.441: the words as they came (vx3dn_execute checked the form: 10 words), one
  * copy into the command block (the VxD's rep movsd) */
  if(vx3dn_write_n)vx3dn_write_n(s,count*10);else for(k=0;k<count*10;k++)DAT(io,s[k]);
  return;}
 d=vx3dn_reserve?vx3dn_reserve(count*f->nw):0;
 if(d){if(fast_ok(f)){fast_vertices(p,f,first,count,d);return;}
  for(k=0;k<count;k++,d+=f->nw)build_vertex(p,f,first+k,d);return;}
 for(k=0;k<count;k++){build_vertex(p,f,first+k,w);if(vx3dn_write_n)vx3dn_write_n(w,f->nw);else for(j=0;j<f->nw;j++)DAT(io,w[j]);}
}
/* v10.236: vx3dn_topo = the topology the engine was last given in 04/20 (0 list: the state set-up writes 0); ACCEL_D3D_STRIP_HW
 * (vx3dn_strip_hw, default 1): strips go to the engine as strips; 0: the VxD sends a strip's triangles as a list (the same triangles) */
uint32_t vx3dn_topo,vx3dn_strip_hw=1,vx3dn_strip_stat[3]; /* stat: strip runs, strip vertices, list runs */
/* v10.470: indexed packets (VX3DN_TOPO_INDEXED; v10.469's FETCH experiment made the production path). vx3dn_varena: the VxD's vertex
 * area slot for n bytes (its video memory offset, 256-byte aligned) or 0xFFFFFFFF when none is free (the packet is then drawn by its
 * indices expanded inline); without it (the host build) vx3dn_va_base / vx3dn_va_bytes are used round robin. vx3dn_vwrite: the VxD's
 * bulk write into video memory (else io->write_fb a word at a time). vx3dn_ix_stat: [0] packets drawn by index, [1] drawn by their
 * indices expanded (no slot), [2] vertices uploaded, [3] indices sent, [4] host round-robin wraps, [5] upload cycles / 16 (with the
 * profile). */
uint32_t (*vx3dn_varena)(uint32_t bytes);void (*vx3dn_vwrite)(uint32_t off,const uint32_t *w,uint32_t n);
uint32_t vx3dn_va_base,vx3dn_va_bytes,vx3dn_ix_stat[6],vx3dn_ixp[8];
/* .555: the index bursts in the form XP's driver sends (s3ginv.dll, VX855: its indexed list at 0x11B030, the head at 0x118350, 04/20's
 * index bits built at 0xD8AA0), always: 04/20 = 1 and the indices 16 bits each, two a data word, the first in the low half (XP copies
 * the application's 16-bit indices as they are); one head for up to VX3DN_IX_LONG data words (XP: one 04/24 for the whole draw).
 * .554 had it as ACCEL_D3D_IX16 (0-3) beside .553's form (04/20 = 3, one index a word, 48 a burst). Board, VXIX16 v1.0 on .554: the same
 * picture in all four forms; 12,800 triangles a frame 21.17 ms (.553's form) against 20.61 ms (this one), the 95th percentile 21.57
 * against 20.79. vx3dn_ix16_stat (TEXDBG BC E4 reads, E0 clears): [0] heads, [1] index data words, [2] indices. */
uint32_t vx3dn_ix16_stat[3]; /* v10.473: vx3dn_ixp (vx_native3d_vxd.c) */static uint32_t va_host_next;
static uint32_t va_get(uint32_t bytes) {
 uint32_t off;
 if(vx3dn_varena)return vx3dn_varena(bytes);
 if(!vx3dn_va_bytes||bytes>vx3dn_va_bytes)return 0xFFFFFFFFUL;
 if(va_host_next+bytes>vx3dn_va_bytes){va_host_next=0;vx3dn_ix_stat[4]++;}
 off=vx3dn_va_base+va_host_next;va_host_next+=(bytes+255UL)&~255UL;return off;
}
/* v10.468: the primitive set-up and vertices' parts (with vx3dn_tsc, TSC cycles / 16): [0] the burst heads (their two blocks and 04/20,
 * 23, 24), [1] bursts, [2] runs, [3] the time from the end of the set-ups to the first burst (the point and line registers, the vertex
 * layout), [4] packets; the copy and its checks are vx3dn_vprof's. TEXDBG BC BB reads, AF clears (with vx3dn_vprof). */
uint32_t vx3dn_rprof[5];
/* .529: .515's engine cull of indexed packets (01/01 bit 7, 04/23 bit 8) removed. vx3dn_r01: 1 once a set-up has written 01/01 since the
 * engine's state was last forgotten (0 at the start and after vx3dn_forget) */
uint32_t vx3dn_r01;
/* .523: vx3dn_lay8, the vertex layout the engine was last given: 0 the set-up's (xp_setup writes it), 1 the 8-word one (an 8-word packet's
 * slot draw), 0xFF not known (vx3dn_forget, the other set-ups). An 8-word packet writes the 8-word layout unless the engine has it; a packet
 * that would skip its set-up (vertices or textures only) while the engine has the 8-word layout gets its whole set-up (vx3dn_execute).
 * vx3dn_v8_pk: 8-word packets drawn from the area; vx3dn_v8_lay: the 8-word layout written for them; vx3dn_v8_full: whole set-ups made for
 * the layout. */
uint32_t vx3dn_lay8=0xFFUL,vx3dn_v8_pk,vx3dn_v8_lay,vx3dn_v8_full;
static void burst_head_x(const vx3dn_io *io,uint32_t topo,uint32_t mode,uint32_t count);
static void burst_head(const vx3dn_io *io,uint32_t topo,uint32_t mode,uint32_t count) {
 if(vx3dn_tsc){uint32_t t0=vx3dn_tsc();burst_head_x(io,topo,mode,count);vx3dn_rprof[0]+=(vx3dn_tsc()-t0+8)>>4;vx3dn_rprof[1]++;}
 else burst_head_x(io,topo,mode,count);
}
static void burst_head_x(const vx3dn_io *io,uint32_t topo,uint32_t mode,uint32_t count) {
 type(io,4);if(topo!=vx3dn_topo){REG(io,0x20,topo==VX3DN_TOPO_STRIP?VX3DN_STRIP_0420:0);vx3dn_topo=topo;}REG(io,0x23,mode);REG(io,0x24,count);type(io,0);
}
/* .528 (texture runs, vx_native3d.h): before run k + 1's first index burst, the words a packet whose state differs only in its texture gets
 * (vx3dn_execute's v10.450 form) after the fetch registers are set back as at an indexed packet's end: 04/20, 21, 22, 2C = 0, the texture
 * cache strobe, then the unit's words with the run's surface, filter, addressing and mip chain. The packet's own texture and chain are
 * swapped for the run's while texture_setup reads them and put back after (the VxD's copy, the HAL's queue: the packet as validated).
 * vx3dn_texrun_stat: [0] packets drawn with runs, [1] runs drawn (texture switches inside packets) */
uint32_t vx3dn_texrun_stat[2];
static void tex_run(const vx3dn_packet *p,const vx3dn_io *io,const vx3dn_texrun *r) {
 vx3dn_packet *q=(vx3dn_packet*)p;vx3dn_surface s0=q->tex.surface;uint32_t f0=q->tex.filter,u0=q->tex.address_u,v0=q->tex.address_v;vx3dn_mip m0=q->mip;
 type(io,4);REG(io,0x20,0);REG(io,0x21,0);REG(io,0x22,0);REG(io,0x2C,0);vx3dn_topo=0xFFFFFFFFUL;
 q->tex.surface=r->surface;q->tex.filter=r->filter;q->tex.address_u=r->address_u;q->tex.address_v=r->address_v;q->mip=r->mip;
 tex_strobe(io);texture_setup(q,io,1);
 q->tex.surface=s0;q->tex.filter=f0;q->tex.address_u=u0;q->tex.address_v=v0;q->mip=m0;
 vx3dn_texrun_stat[1]++;
}
uint32_t vx3dn_execute(const vx3dn_packet *p,const vx3dn_io *io,uint32_t *last) {
 uint32_t v,step=3,mode=0xCE02UL,tq;
 vx3dn_stage=1;vx_vbad=0;
 if(!idle(io,last)) return VX3DN_LOST;
 if(p->clip.right==p->clip.left||p->clip.bottom==p->clip.top) return VX3DN_OK;
 if(p->op==VX3DN_CLEAR) {
  /* Explicit synchronous CPU clear; no claim of accelerated depth clear. */
  if(p->clear_flags&VX3DN_CLEAR_TARGET) clear_surface(p,io,&p->target,p->target.format==VX3DN_RGB565?rgb565(p->clear_color):p->clear_color,0xFFFFFFFFUL);
  if(p->clear_flags&(VX3DN_CLEAR_Z|VX3DN_CLEAR_STENCIL)) {uint32_t dv,dm;vx3dn_depth_word(p,&dv,&dm);if(dm)clear_surface(p,io,&p->depth,dv,dm);}
  vx3dn_plane_clear(p,io); /* v10.245 */
  return VX3DN_OK;
 }
 /* .515-.530: after the engine's state is forgotten (vx3dn_forget: a reset, a switch, a refused packet) the next packet sends its whole set-up,
  * whatever the state cache says (vx3dn_r01: a set-up's 01/01 written since; .529 lost this with the engine cull's 01/01 record) */
 if((vx3dn_vertices_only||vx3dn_tex_only)&&!vx3dn_r01){vx3dn_vertices_only=0;vx3dn_tex_only=0;}
 if(!vx3dn_vertices_only&&vx3dn_tex_only&&!textured(p))vx3dn_tex_only=0; /* v10.450 */
 if((vx3dn_vertices_only||vx3dn_tex_only)&&vx3dn_lay8==1&&VX3DN_PRE_STRIDE(p)!=8){vx3dn_vertices_only=0;vx3dn_tex_only=0;vx3dn_v8_full++;} /* .523: the engine has the 8-word layout, this packet needs its set-up's */
 if(!vx3dn_vertices_only&&vx3dn_tex_only) { /* v10.450: the strobe (counted as the state set-up) and the texture units */
 uint32_t t0=vx3dn_tsc?vx3dn_tsc():0,t1;
 if(vx3dn_mark)vx3dn_mark(0);
 tex_strobe(io);
 if(vx3dn_mark)vx3dn_mark(1);
 if(vx3dn_tsc){t1=vx3dn_tsc();vx3dn_eprof[0]+=(t1-t0+128)>>8;t0=t1;}
 texture_setup(p,io,1);
 if(vx3dn_mark)vx3dn_mark(2);
 if(vx3dn_mark)vx3dn_mark(3);
 if(vx3dn_tsc){t1=vx3dn_tsc();vx3dn_eprof[1]+=(t1-t0+128)>>8;}
 } else
 if(!vx3dn_vertices_only) { /* v10.232: the VxD's fetch path sets it when the engine already holds this packet's state */
 uint32_t t0=vx3dn_tsc?vx3dn_tsc():0,t1;
 if(vx3dn_mark)vx3dn_mark(0); /* v10.440 */
 setup(p,io);
 if(vx3dn_mark)vx3dn_mark(1);
 if(vx3dn_tsc){t1=vx3dn_tsc();vx3dn_eprof[0]+=(t1-t0+128)>>8;t0=t1;}
 if(textured(p)) texture_setup(p,io,0);
 if(vx3dn_mark)vx3dn_mark(2);
 if(vx3dn_tsc){t1=vx3dn_tsc();vx3dn_eprof[1]+=(t1-t0+128)>>8;t0=t1;}
 if(xp_path(p)) xp_setup(p,io);
 if(vx3dn_mark)vx3dn_mark(3);
 if(vx3dn_tsc){t1=vx3dn_tsc();vx3dn_eprof[2]+=(t1-t0+128)>>8;}
 }
 vx3dn_stage=2;
 if(!idle(io,last)) return VX3DN_LOST;
 tq=vx3dn_tsc?vx3dn_tsc():0; /* v10.468 */
 {uint32_t prim=full(p)?p->ext.prim:0;step=prim==1?2:prim==2?1:3;
  if(prim==0&&vx3dn_burst&&p->vertex_count>3)step=p->vertex_count>VX3DN_BURST_MAX?VX3DN_BURST_MAX:p->vertex_count; /* v10.214: one burst; v10.235: at most 48 vertices (a multiple of 3; the last burst takes the rest) */
  /* v10.202: XP T64 writes no 01/85 for points and sets 04/20 and 04/40..48 */
  if(prim==1){type(io,1);REG(io,0x85,0x800000UL);sh_m[0x85>>5]&=~(1UL<<(0x85&31));} /* .525: 01/85 no longer the shadow's value */
  if(prim==2){type(io,4);REG(io,0x20,0);vx3dn_topo=0;REG(io,0x40,0x800000UL);REG(io,0x41,0);REG(io,0x42,0x3F);REG(io,0x43,0x800000UL);
   REG(io,0x44,0);REG(io,0x45,0x3F);REG(io,0x46,0x7FFF00UL);REG(io,0x47,0);REG(io,0x48,0x3F);}
  mode=prim==1?0xCE01UL:prim==2?0x1E00UL:(full(p)&&(p->ext.dither&2))?0x9E02UL:0xCE02UL;} /* v10.481: ext.dither bit 1, XP's flat triangles (CLXP3D T48: 04/23 = 9E02, each triangle the colours of its first vertex) */
 {vx_vfmt f;f.xp=xp_path(p);f.tx=textured(p);f.stq=full(p)&&p->stages.layout;f.un=units(p);f.c3=c3_layout(p);
 f.m3=mixed3(p)?(int)p->stages.s3:0;if(f.m3){f.stq=0;f.c3=0;}
 f.nw=f.m3?10+(f.m3&1)+((f.m3>>1)&1):f.xp?(f.stq?9:f.c3?12:10):(f.tx?7:5);
 if(p->pre.on&&(!vx3dn_pre_ok(p)||!f.xp||f.nw!=10||!fast_ok(&f)))return VX3DN_BAD_HERE; /* v10.441: only the form the HAL built */
 if(vx3dn_tsc){vx3dn_rprof[3]+=(vx3dn_tsc()-tq+8)>>4;vx3dn_rprof[4]++;}
 /* v10.470: an indexed packet (VX3DN_TOPO_INDEXED, vx3dn_validate checked its form, every vertex and every index): its vertices uploaded to
  * a slot of the VxD's vertex area and drawn by index (04/21 base, 04/22 = 40 << 8 | base >> 24, 04/2C the last vertex, 04/20 = 3:
  * 32-bit indices, one dword an index on 0x440, bursts of up to 48), 04/20, 21, 22 and 2C written 0 again after it (setup()'s constant
  * group: once primed it no longer writes them); with no slot free, its indices expanded into inline bursts as a list */
/* .523: the 8-word form (VX3DN_IX_V8): Direct3D 8's pre-transformed vertices of 8 words (x y z rhw, diffuse, specular, u v) go to the
 * area as they are (32-byte stride: 04/22 = 32 << 8) and, when the engine has the 10-word layout, the 8-word one is written before the index
 * bursts: 04/00 = A80008, 04/01-04/03 the codes 0 1 2 / 3 5 6 / 8 9, 04/04 = 0, 04/25 = F6C000, 04/28 = FFC000 (proven by the .522 probe,
 * TEXDBG BC E1-E3, removed in .524). */
 if(full(p)&&p->pre.on==2&&p->bursts.n==1&&((p->bursts.b[0]>>16)&0xFFUL)==VX3DN_TOPO_INDEXED) {
  const uint16_t *ix=VX3DN_CIDX(p);const uint32_t vs=VX3DN_PRE_STRIDE(p);uint32_t n=p->vertex_count,ni=p->bursts.b[0]&0xFFFFUL,bytes=n*vs*4UL,off,k,s; /* .523: vs 8 or 10 words a vertex */
  if(!f.xp||f.nw!=10||(mode!=0xCE02UL&&mode!=0x9E02UL))return VX3DN_BAD_HERE;
  vx3dn_geo[2]+=ni/3;vx3dn_ix_stat[3]+=ni;
  {const uint32_t ta=vx3dn_tsc?vx3dn_tsc():0;
  off=va_get(bytes);
  if(off!=0xFFFFFFFFUL) {
   uint32_t t0=vx3dn_tsc?vx3dn_tsc():0,tb2=0;
   const uint32_t vb=vs*4UL; /* .524: the stride, 32 bytes for the 8-word form (the .522 probe's E1-E3 forms removed) */
   if(vx3dn_vwrite)vx3dn_vwrite(off,p->pre.w,n*vs);else for(k=0;k<n*vs;k++)io->write_fb(io->context,off+k*4,p->pre.w[k]); /* .523: the HAL's words as they are */
   if(vs==8&&vx3dn_lay8!=1){type(io,4);vx3dn_lay8=1;REG(io,0x00,0xA80008UL);REG(io,0x01,0x020100UL);REG(io,0x02,0x060503UL);REG(io,0x03,0x000908UL);REG(io,0x04,0);REG(io,0x25,0xF6C000UL);REG(io,0x28,0xFFC000UL);vx3dn_v8_lay++;}
   if(vs==8)vx3dn_v8_pk++;
   if(vx3dn_tsc)vx3dn_ix_stat[5]+=(vx3dn_tsc()-t0+8)>>4;
   vx3dn_geo[0]+=n*vs;vx3dn_geo[1]+=n;vx3dn_ix_stat[2]+=n;
   if(vx3dn_tsc){tb2=vx3dn_tsc();vx3dn_ixp[4]+=(tb2-ta+128)>>8;}
   {uint32_t rk=0,rend=p->runs.n?p->runs.r[0].at:ni; /* .528: texture runs */
   if(p->runs.n)vx3dn_texrun_stat[0]++;
   {const uint32_t cap=VX3DN_IX_LONG*2UL,r21=off&0xFFFFFFUL,r22=(vb<<8)|(off>>24); /* .555: the indices a burst; .522: vb the stride */
   for(v=0;v<ni;v+=s){const uint16_t *q;uint32_t nd,full2;
    if(v==rend){tex_run(p,io,&p->runs.r[rk]);rk++;rend=rk<p->runs.n?p->runs.r[rk].at:ni;}
    s=rend-v<cap?rend-v:cap;
    type(io,4);REG(io,0x21,r21);REG(io,0x22,r22);REG(io,0x2C,n-1);REG(io,0x2D,0);REG(io,0x2E,0);
    REG(io,0x20,1);REG(io,0x23,mode);REG(io,0x24,s);type(io,0);
    q=ix+v;nd=(s+1)>>1;full2=s>>1; /* two indices a word; an odd burst's last word has a 0 above its index */
    vx3dn_ix16_stat[0]++;vx3dn_ix16_stat[1]+=nd;vx3dn_ix16_stat[2]+=s;
    if(vx3dn_wform==3&&vx3dn_wp<vx3dn_wend&&(uint32_t)(vx3dn_wend-vx3dn_wp)>=nd){uint32_t *d=vx3dn_wp; /* .546: through a local pointer (Watcom
      * keeps globals in memory across stores through a pointer) */
     if(!((uintptr_t)q&3)){const vx_u32m *w=(const vx_u32m*)(const void*)q;for(k=0;k<full2;k++)d[k]=w[k];} /* the packet's words as they are */
     else for(k=0;k<full2;k++)d[k]=(uint32_t)q[2*k]|((uint32_t)q[2*k+1]<<16); /* a run that begins at an odd index */
     if(s&1)d[full2]=q[s-1];
     vx3dn_wp=d+nd;}
    else{for(k=0;k<full2;k++)DAT(io,(uint32_t)q[2*k]|((uint32_t)q[2*k+1]<<16));if(s&1)DAT(io,q[s-1]);}
    vx3dn_stage=3+v/VX3DN_BURST_MAX;if(!idle(io,last))return VX3DN_LOST;}}}
   {uint32_t tc2=vx3dn_tsc?vx3dn_tsc():0;
   type(io,4);REG(io,0x20,0);REG(io,0x21,0);REG(io,0x22,0);REG(io,0x2C,0);vx3dn_topo=0xFFFFFFFFUL; /* .523: vx3dn_lay8 says which layout the engine has */
   if(vx3dn_tsc){vx3dn_ixp[5]+=(tc2-tb2+128)>>8;vx3dn_ixp[6]+=(vx3dn_tsc()-tc2+128)>>8;vx3dn_ixp[7]++;}}
   vx3dn_ix_stat[0]++;
   return VX3DN_OK;
  }}
  if(vx3dn_lay8==1){type(io,4);REG(io,0x00,0xA8000AUL);REG(io,0x01,0x020100UL);REG(io,0x02,0x060503UL);REG(io,0x03,0x0C0908UL);REG(io,0x04,0x00000DUL);REG(io,0x25,0xF6CC00UL);REG(io,0x28,0xFFC000UL);vx3dn_lay8=0;} /* .523: the
   * inline list is 10 words a vertex: the engine left with the 8-word layout gets the plain 10-word one (an 8-word packet is that form) */
  {uint32_t rk=0,rend=p->runs.n?p->runs.r[0].at:ni; /* .528: texture runs */
  if(p->runs.n)vx3dn_texrun_stat[0]++;
  for(v=0;v<ni;v+=s){uint32_t *d;if(v==rend){tex_run(p,io,&p->runs.r[rk]);rk++;rend=rk<p->runs.n?p->runs.r[rk].at:ni;}
   s=rend-v<VX3DN_BURST_MAX?rend-v:VX3DN_BURST_MAX; /* no slot free: the list inline */
   burst_head(io,VX3DN_TOPO_LIST,mode,s);
   d=vx3dn_reserve?vx3dn_reserve(s*10):0;
   if(d){for(k=0;k<s;k++){const uint32_t *w=&p->pre.w[ix[v+k]*vs];uint32_t j;for(j=0;j<8;j++)*d++=w[j];if(vs==8){*d++=w[6];*d++=w[7];}else{*d++=w[8];*d++=w[9];}}} /* .523: an 8-word vertex
    * as the 10-word form (texture B's u v a copy of A's, as the HAL writes them) */
   else for(k=0;k<s;k++){const uint32_t *w=&p->pre.w[ix[v+k]*vs];uint32_t j,x[10];for(j=0;j<8;j++)x[j]=w[j];x[8]=vs==8?w[6]:w[8];x[9]=vs==8?w[7]:w[9];if(vx3dn_write_n)vx3dn_write_n(x,10);else for(j=0;j<10;j++)DAT(io,x[j]);}
   vx3dn_geo[0]+=s*10;vx3dn_geo[1]+=s;
   vx3dn_stage=3+v/VX3DN_BURST_MAX;if(!idle(io,last))return VX3DN_LOST;}}
  vx3dn_ix_stat[1]++;
  return VX3DN_OK;
 }
 if(full(p)&&p->bursts.n) { /* v10.236: runs of triangle lists and strips */
  uint32_t r,base=0,bursts=0;
  if(vx3dn_tsc)vx3dn_rprof[2]+=p->bursts.n;
  for(r=0;r<p->bursts.n;r++) {
   uint32_t topo=p->bursts.b[r]>>16,cnt=p->bursts.b[r]&0xFFFFUL,off=0;
   if(topo==VX3DN_TOPO_STRIP){vx3dn_strip_stat[0]++;vx3dn_strip_stat[1]+=cnt;vx3dn_geo[3]+=cnt;vx3dn_geo[2]+=cnt>=2?cnt-2:0;}else{vx3dn_strip_stat[2]++;if(mode==0xCE02UL||mode==0x9E02UL)vx3dn_geo[2]+=cnt/3;} /* v10.440: triangles */
   if(topo==VX3DN_TOPO_STRIP&&!vx3dn_strip_hw) { /* ACCEL_D3D_STRIP_HW 0: the strip's triangles sent as a list */
    uint32_t t=0,nt=cnt-2;
    while(t<nt){uint32_t m=nt-t>16?16:nt-t,k;
     burst_head(io,0,mode,m*3);
     for(k=0;k<m;k++)put_vertices(p,io,&f,base+t+k,3);
     if(vx_vbad)return VX3DN_BAD_HERE; /* v10.449 */
     t+=m;vx3dn_stage=3+bursts++;if(!idle(io,last)) return VX3DN_LOST;}
   } else while(off<cnt) {
    uint32_t s=cnt-off;
    if(s>VX3DN_BURST_MAX)s=VX3DN_BURST_MAX;
    if(topo==VX3DN_TOPO_LIST&&!vx3dn_burst&&s>3)s=3;
    burst_head(io,topo,mode,s);
    put_vertices(p,io,&f,base+off,s);
    if(vx_vbad)return VX3DN_BAD_HERE; /* v10.449 */
    vx3dn_stage=3+bursts++;if(!idle(io,last)) return VX3DN_LOST;
    if(topo==VX3DN_TOPO_STRIP){if(off+s>=cnt)break;off+=s-2;}else off+=s; /* a strip's next burst repeats its last two vertices */
   }
   base+=cnt;
  }
  return VX3DN_OK;
 }
 if(mode==0xCE02UL||mode==0x9E02UL)vx3dn_geo[2]+=p->vertex_count/3; /* v10.440 */
 for(v=0;v<p->vertex_count;v+=step) {
  uint32_t s=p->vertex_count-v<step?p->vertex_count-v:step; /* v10.235: a shorter last burst */
  burst_head(io,0,mode,s); /* writes 04/20 = 0 first only after a strip (v10.236) */
  put_vertices(p,io,&f,v,s);
  if(vx_vbad)return VX3DN_BAD_HERE; /* v10.449 */
  vx3dn_stage=3+v/step;
  if(!idle(io,last)) return VX3DN_LOST;
 }}
 return VX3DN_OK;
}
