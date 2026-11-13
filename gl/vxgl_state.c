/* VXGL.DLL: OpenGL state -> the core's render states (the Direct3D 7 numbering the core was proven with), and the fallback decision
 * (v10.239). What the engine cannot draw as asked goes to swrast: the fallback bits name why (vxgl.h). */
#include "vxgl.h"
#include "macros.h"
#include "colormac.h"
#include "tnl/t_context.h"

int vxgl_gl_logicop=1,vxgl_gl_colormask=1; /* v10.295: ACCEL_OPENGL_LOGICOP, ACCEL_OPENGL_COLORMASK */
int vxgl_gl_smoothatest=1; /* v10.353: ACCEL_OPENGL_SMOOTHATEST */
int vxgl_gl_nocolor=1; /* v10.346: ACCEL_OPENGL_NOCOLOR; v10.347: default 1 (board: Quake 2 right with it) */
int vxgl_gl_pstipple=1,vxgl_gl_query=1; /* v10.315: ACCEL_OPENGL_QUERY: occlusion queries counted by the engine */ /* v10.308: ACCEL_OPENGL_PSTIPPLE: polygon stipple on the engine */
static GLuint bitrev32(GLuint v) {v=((v>>1)&0x55555555U)|((v&0x55555555U)<<1);v=((v>>2)&0x33333333U)|((v&0x33333333U)<<2);v=((v>>4)&0x0F0F0F0FU)|((v&0x0F0F0F0FU)<<4);v=((v>>8)&0x00FF00FFU)|((v&0x00FF00FFU)<<8);return (v>>16)|(v<<16);}
int vxgl_gl_blendop=1; /* v10.304: ACCEL_OPENGL_BLENDOP: blend equations and the constant blend colour on the engine (HCM 01/34-3F) */
static GLuint cmp(GLenum f) {return (f>=GL_NEVER&&f<=GL_ALWAYS)?(GLuint)(f-GL_NEVER)+1:8;} /* GL_NEVER..GL_ALWAYS -> D3DCMP 1..8 */
/* GL blend factor -> D3DBLEND; dst alpha reads 1 (no alpha buffer); 0 = not expressible */
static GLuint blend(GLenum f) {
 switch(f) {
  case GL_ZERO:return 1;case GL_ONE:return 2;
  case GL_SRC_COLOR:return 3;case GL_ONE_MINUS_SRC_COLOR:return 4;
  case GL_SRC_ALPHA:return 5;case GL_ONE_MINUS_SRC_ALPHA:return 6;
  case GL_DST_ALPHA:return 2;case GL_ONE_MINUS_DST_ALPHA:return 1;
  case GL_DST_COLOR:return 9;case GL_ONE_MINUS_DST_COLOR:return 10;
  case GL_SRC_ALPHA_SATURATE:return 1; /* min(As, 1 - Ad) with Ad = 1 (no alpha buffer): 0 */
  /* v10.304: the constant blend colour (the core's 14 / 15) and its alpha (16 / 17) */
  case GL_CONSTANT_COLOR:return vxgl_gl_blendop?14:0;case GL_ONE_MINUS_CONSTANT_COLOR:return vxgl_gl_blendop?15:0;
  case GL_CONSTANT_ALPHA:return vxgl_gl_blendop?16:0;case GL_ONE_MINUS_CONSTANT_ALPHA:return vxgl_gl_blendop?17:0;
 }
 return 0;
}
static GLuint sop(GLenum o) {
 switch(o) {
  case GL_KEEP:return 1;case GL_ZERO:return 2;case GL_REPLACE:return 3;case GL_INCR:return 4;case GL_DECR:return 5;
  case GL_INVERT:return 6;case GL_INCR_WRAP_EXT:return 7;case GL_DECR_WRAP_EXT:return 8;
 }
 return 1;
}
static GLuint argb(const GLfloat *c) {
 GLubyte b[4];UNCLAMPED_FLOAT_TO_UBYTE(b[0],c[0]);UNCLAMPED_FLOAT_TO_UBYTE(b[1],c[1]);UNCLAMPED_FLOAT_TO_UBYTE(b[2],c[2]);UNCLAMPED_FLOAT_TO_UBYTE(b[3],c[3]);
 return ((GLuint)b[3]<<24)|((GLuint)b[0]<<16)|((GLuint)b[1]<<8)|b[2];
}

/* v10.263: one face's stencil state into the core's render states (Mesa keeps front in index 0, back in 1) */
static void stencil_regs(vxgl_ctx *c,int f) {
 GLcontext *ctx=&c->gl;uint32_t *rs=c->core.state.rs;
 rs[53]=sop(ctx->Stencil.FailFunc[f]);rs[54]=sop(ctx->Stencil.ZFailFunc[f]);rs[55]=sop(ctx->Stencil.ZPassFunc[f]);
 rs[56]=cmp(ctx->Stencil.Function[f]);rs[57]=(GLuint)ctx->Stencil.Ref[f]&255;rs[58]=ctx->Stencil.ValueMask[f]&255;rs[59]=ctx->Stencil.WriteMask[f]&255;
}
/* v10.263: two-sided stencil on the engine: before a triangle of the other face the batch so far goes out and the core takes that
 * face's stencil state (it was the whole pipeline to swrast) */
void vxgl_stencil_face(vxgl_ctx *c,int face) {
 if(face==c->stencil_face)return;
 vxgl_hw_flush(c);stencil_regs(c,face);c->stencil_face=face;vx_d3d_gl_state();
}
/* v10.321: whether the stages keep the fragment's alpha a factor times the vertex alpha (antialiasing's coverage, which swrast applies
 * after them) and its colour free of it. An argument carries it when it is the diffuse colour or the current one, uncomplemented; the
 * alpha must be selected from one, or modulated (unscaled) by one and something else; the colour must not read its alpha. */
static int aa_alpha_carried(const vxgl_ctx *c) {
 int s;
 if(!c->tex_units&&!c->fp_on)return 1; /* no texture: the diffuse colour as it is (the core leaves the stages out) */
 for(s=0;s<VX_D3D_STAGES;s++) {
  const uint32_t *t=c->core.state.tss[s];uint32_t op=t[1],aop=t[4],a1=t[5],a2=t[6];int k;
#define AA_C(x) (((x)&15)<=1&&!((x)&0x10)) /* carries the coverage */
#define AA_X(x) (((x)&15)<=1&&((x)&0x10)) /* complemented: neither */
  if(op==1)break;
  if(op==12||op==16||op==24)return 0; /* blend by the diffuse or current alpha; DOT3 writes the alpha */
  for(k=0;k<3;k++){uint32_t a=t[k==0?2:k==1?3:VX_D3D_TSS_CARG0];if((k<2||op==26)&&((a&15)<=1)&&(a&0x20))return 0;} /* the colour from a carried alpha */
  if(aop==1)continue;
  if(t[VX_D3D_TSS_ASCALE])return 0;
  if(aop==2){if(!AA_C(a1))return 0;}
  else if(aop==3){if(!AA_C(a2))return 0;}
  else if(aop==4){if(AA_X(a1)||AA_X(a2)||AA_C(a1)==AA_C(a2))return 0;}
  else return 0;
#undef AA_C
#undef AA_X
 }
 return 1;
}
/* v10.321: the engine drew into a texture's copy and now draws elsewhere (it may sample that texture next): its draws sent and the VxD's state
 * cache started over (VX_IOCTL_2D_ACCESS, as a DirectDraw lock of the texture does): the sampler state goes to the engine again, which is what
 * refreshes its texture cache (vx_native3d_vxd.c VX3DN_busy) */
/* v10.330 (review #19): the mark stays when the wait did not reach the engine: tried again at the next state change */
static void rtt_done(vxgl_ctx *c) {if(c->rtt_drawn){vxgl_hw_flush(c);vxgl_batch_flush();if(vxgl_engine_wait())c->rtt_drawn=0;}}
static void translate(vxgl_ctx *c);
void vxgl_translate_state(vxgl_ctx *c) {VXGL_PROF_BEGIN;translate(c);VXGL_PROF_END(0);} /* v10.271: timed for VXGL.LOG */
static void translate(vxgl_ctx *c) {
 GLcontext *ctx=&c->gl;GLframebuffer *fb=ctx->DrawBuffer;vx_d3d_state old=c->core.state;uint32_t *rs=c->core.state.rs;GLuint fall=0,s,d;int32_t tw,th;
 c->new_state=0;
 /* v10.321: a framebuffer object the engine can draw into (vxgl_fbo.c: a texture's level or our renderbuffers, ACCEL_OPENGL_RTT): its surfaces
  * as the core's target and depth, not y-flipped; otherwise swrast as before. v10.317 (review): not the front either */
 if(!c->fb||fb!=&c->fb->Base){vx3dn_surface t,z;
  c->front_hw=0;
  if((c->fallback&(VXGL_FB_NOHW|VXGL_FB_REGISTRY))||!vxgl_fbo_target(c,fb,1,&t,&z)){
   /* (review) the window's surfaces back on the core: they were left as the framebuffer object's (its depth tested by the next window draw) */
   if(c->rtt){rtt_done(c);vxgl_hw_flush(c);c->core.target=c->fb->sback;c->core.depth=c->fb->sdepth;c->tex_changed=1;c->rtt=0;vx_d3d_gl_state();}
   vxgl_fallback(c,VXGL_FB_DRAWBUFFER,GL_TRUE);return;}
  if(!c->rtt||memcmp(&t,&c->core.target,sizeof(t))||memcmp(&z,&c->core.depth,sizeof(z))){rtt_done(c);vxgl_hw_flush(c);c->core.target=t;c->core.depth=z;c->tex_changed=1;}
  c->rtt=1;tw=(int32_t)t.width;th=(int32_t)t.height;}
 else {
  if(c->rtt){rtt_done(c);vxgl_hw_flush(c);c->core.depth=c->fb->sdepth;c->tex_changed=1;c->rtt=0;} /* (the colour target is put back below) */
  tw=(int32_t)c->fb->w;th=(int32_t)c->fb->h;}
 if(c->vp_flip!=vxgl_flip(c)||c->vp_h!=(GLfloat)th)vxgl_calc_viewport(ctx); /* the window's y flip, or none, and the height it is from */
 c->xlate_old=&old; /* v10.268: a flush from here on (a texture upload, the state change below) sends the batch under the state it was built with */
 /* fill, shade (flat is done by copying the provoking vertex's colour), last pixel off (GL lines leave the last pixel out) */
 rs[8]=3;rs[9]=vx_d3d_gl_hwflat&&ctx->Light.ShadeModel==GL_FLAT?1:2;rs[16]=0; /* v10.481: flat on the engine with ACCEL_OPENGL_HWFLAT (vxgl_tris.c sends each flat triangle from its provoking vertex) */
 /* depth */
 rs[7]=(ctx->Depth.Test&&c->core.depth.format)?1:0;rs[14]=(rs[7]&&ctx->Depth.Mask)?1:0;rs[23]=cmp(ctx->Depth.Func);
 /* v10.354: the advertised test examines stored Z, independently of Depth.Test. */
 if(ctx->Depth.BoundsTest&&fb->Visual.depthBits)fall|=VXGL_FB_DEPTHBOUNDS;
 /* alpha test */
 rs[15]=ctx->Color.AlphaEnabled?1:0;rs[25]=cmp(ctx->Color.AlphaFunc);{GLubyte r;CLAMPED_FLOAT_TO_UBYTE(r,ctx->Color.AlphaRef);rs[24]=r;}
 /* blend */
 rs[27]=0;rs[19]=2;rs[20]=1;rs[VX_D3D_RS_BLENDOP]=0;rs[VX_D3D_RS_BLENDFACTOR]=0;
 if(ctx->Color.BlendEnabled) {
  /* v10.302 (review): the alpha equation and factors only when the target keeps alpha (no visual does): glBlendFuncSeparate with
   * other alpha factors went to software for a result nothing stores. v10.304: SUBTRACT, REVERSE_SUBTRACT, MIN and MAX on the engine
   * (Direct3D 8's BLENDOP numbering in the core: 2, 3, 4, 5); MIN and MAX take no factors */
  GLenum eq=ctx->Color.BlendEquationRGB;uint32_t bo=eq==GL_FUNC_ADD?1:eq==GL_FUNC_SUBTRACT?2:eq==GL_FUNC_REVERSE_SUBTRACT?3:eq==GL_MIN?4:eq==GL_MAX?5:0;
  if(!bo||(bo>1&&!vxgl_gl_blendop)||(ctx->Visual.alphaBits&&(ctx->Color.BlendEquationA!=eq||ctx->Color.BlendSrcRGB!=ctx->Color.BlendSrcA||ctx->Color.BlendDstRGB!=ctx->Color.BlendDstA)))fall|=VXGL_FB_BLEND;
  s=blend(ctx->Color.BlendSrcRGB);d=blend(ctx->Color.BlendDstRGB); /* SRC_ALPHA_SATURATE: min(As, 1 - Ad) = 0 with Ad = 1: ZERO */
  if(bo>=4){s=2;d=2;}
  if(!s||!d)fall|=VXGL_FB_BLEND;
  else if(!(s==2&&d==1&&bo==1)){rs[27]=1;rs[19]=s;rs[20]=d;rs[VX_D3D_RS_BLENDOP]=bo;rs[VX_D3D_RS_BLENDFACTOR]=argb(ctx->Color.BlendColor);}
 }
 /* v10.295: the logic op on the engine (01/53 bits 11:8, the truth table of source 1100 and destination 1010: the GL op's number
  * with its four bits reversed); it replaces blending (GL). ACCEL_OPENGL_LOGICOP = 0: software. */
 rs[168]=15;rs[VX_D3D_RS_LOGICOP]=0;
 if(ctx->Color._LogicOpEnabled) {
  static const GLubyte tt[16]={0x0,0x8,0x4,0xC,0x2,0xA,0x6,0xE,0x1,0x9,0x5,0xD,0x3,0xB,0x7,0xF};GLuint op=ctx->Color.LogicOp-GL_CLEAR;
  if(vxgl_gl_logicop&&op<16){rs[VX_D3D_RS_LOGICOP]=0x10|tt[op];rs[27]=0;fall&=~VXGL_FB_BLEND;}else fall|=VXGL_FB_LOGICOP;
 }
 if(!ctx->Color.ColorMask[0]&&!ctx->Color.ColorMask[1]&&!ctx->Color.ColorMask[2]&&(!ctx->Color.ColorMask[3]||!ctx->Visual.alphaBits)) {
  /* v10.264: no colour written (shadow volumes, depth-only passes): blend ZERO, ONE on the engine; blend and logic op have no effect (was swrast) */
  rs[27]=1;rs[19]=1;rs[20]=2;rs[VX_D3D_RS_BLENDOP]=1;rs[VX_D3D_RS_LOGICOP]=0;fall&=~(VXGL_FB_BLEND|VXGL_FB_LOGICOP); /* v10.330 (review #37): the equation ADD too: MIN, MAX and the subtractions kept from the blend wrote colour (MIN / MAX take no factors) */
  /* v10.346: ACCEL_OPENGL_NOCOLOR (v10.347: default 1; = 0 the blend as before): instead the engine's channel write mask all off and no blend, so the engine need not
   * read the colour (depth-only passes and the engine's depth clear: half the memory traffic, if the chip skips the read) */
  if(vxgl_gl_nocolor&&vxgl_gl_colormask){rs[27]=0;rs[168]=0;}
 }
 else if(!ctx->Color.ColorMask[0]||!ctx->Color.ColorMask[1]||!ctx->Color.ColorMask[2]) {
  /* v10.295: the channels on the engine (01/53 bits 3:0, R 1 G 2 B 4 A 8 as the program's write masks; COLORWRITEENABLE's order).
   * ACCEL_OPENGL_COLORMASK = 0: software */
  if(vxgl_gl_colormask)rs[168]=(ctx->Color.ColorMask[0]?1U:0)|(ctx->Color.ColorMask[1]?2U:0)|(ctx->Color.ColorMask[2]?4U:0)|8U;else fall|=VXGL_FB_COLORMASK;
 }
 /* v10.340: GL_FRONT_AND_BACK (two colour buffers) on the engine, each draw twice (vxgl_tris.c run_pipeline): only where the second
  * draw gives what drawing both at once gives, i.e. it does not see the first: no depth test with depth writes, no stencil, no occlusion
  * query. Otherwise software, as before. */
 c->both=0;
 if(fb->_NumColorDrawBuffers[0]!=1) {
  if(!c->rtt&&vxgl_gl_front&&ctx->Visual.doubleBufferMode&&fb->_NumColorDrawBuffers[0]==2&&fb->_ColorDrawBufferMask[0]==(BUFFER_BIT_FRONT_LEFT|BUFFER_BIT_BACK_LEFT)&&
     (!ctx->Depth.Test||!ctx->Depth.Mask)&&!ctx->Stencil.Enabled&&!ctx->Query.CurrentOcclusionObject)c->both=1;
  else fall|=VXGL_FB_DRAWBUFFER;
 }
 /* v10.249: the front surface is swrast's. v10.317: the engine draws into it (ACCEL_OPENGL_FRONT) when it is the only buffer drawn and a
  * surface of the back buffer's pitch in video memory: filled from the window first (vxgl_front_load), shown at glFlush / glFinish */
 if(!c->rtt){int front=ctx->Visual.doubleBufferMode&&(fb->_ColorDrawBufferMask[0]&BUFFER_BIT_FRONT_LEFT);vx3dn_surface want;
  int favail=(front||c->both)&&vxgl_gl_front&&!(c->fallback&(VXGL_FB_NOHW|VXGL_FB_REGISTRY))&&vxgl_front_load(c->fb)&&c->fb->front&&!c->fb->fsoft&&c->fb->sfront.format;
  if(c->both){if(!favail){c->both=0;fall|=VXGL_FB_DRAWBUFFER;}c->front_hw=c->both&&c->both_front;}
  else {c->front_hw=front&&fb->_NumColorDrawBuffers[0]==1&&!(fb->_ColorDrawBufferMask[0]&~BUFFER_BIT_FRONT_LEFT)&&favail;
   if(front&&!c->front_hw)fall|=VXGL_FB_DRAWBUFFER;}
  want=c->front_hw?c->fb->sfront:c->fb->sback;
  if(want.format&&memcmp(&want,&c->core.target,sizeof(want))){vxgl_hw_flush(c);c->core.target=want;c->tex_changed=1;}} /* the template is made again (below) */
 /* cull (core: CULLMODE 2 culls area > 0, 3 culls area < 0; area = (x1-x0)(y2-y0) - (y1-y0)(x2-x0) on the surface, y down) */
 rs[22]=1;c->cull_all=0;
 if(ctx->Polygon.CullFlag) {
  /* the y flip keeps the sign of the core's area for GL's counter-clockwise triangles negative (host test, VXGL scene 4); v10.321: a
   * framebuffer object's surface is not flipped: positive */
  int front_neg=(ctx->Polygon.FrontFace==GL_CCW)==(vxgl_flip(c)!=0);
  if(ctx->Polygon.CullFaceMode==GL_BACK)rs[22]=front_neg?2:3;
  else if(ctx->Polygon.CullFaceMode==GL_FRONT)rs[22]=front_neg?3:2;
  else c->cull_all=1; /* v10.263: front and back: only points and lines are left; the filled triangles are dropped in vxgl_tris.c (was swrast) */
 }
 /* stencil (D24S8 only; one-sided) */
 rs[52]=(ctx->Stencil.Enabled&&c->core.depth.format==VX3DN_D24S8)?1:0;
 c->twoside_stencil=rs[52]&&ctx->Stencil._TestTwoSide; /* v10.263: front and back state differ: per triangle (vxgl_stencil_face), not swrast */
 stencil_regs(c,0);c->stencil_face=0;
 /* fog: per-vertex factors from tnl in the specular alpha */
 rs[28]=ctx->Fog.Enabled?1:0;rs[34]=argb(ctx->Fog.Color);rs[35]=0;
 rs[29]=(ctx->_TriangleCaps&DD_SEPARATE_SPECULAR)?1:0;
 rs[26]=ctx->Color.DitherFlag?1:0;
 rs[47]=0;
 /* the drawing rectangle: the scissor box inside the window, in the surface's coordinates (top row 0; v10.321: a framebuffer object's row 0
  * is GL's) */
 c->core.state.viewport.left=fb->_Xmin;c->core.state.viewport.right=fb->_Xmax;
 if(c->rtt){c->core.state.viewport.top=fb->_Ymin;c->core.state.viewport.bottom=fb->_Ymax;}
 else {c->core.state.viewport.top=th-fb->_Ymax;c->core.state.viewport.bottom=th-fb->_Ymin;}
 if(c->core.state.viewport.left<0)c->core.state.viewport.left=0;
 if(c->core.state.viewport.top<0)c->core.state.viewport.top=0;
 if(c->core.state.viewport.bottom>th)c->core.state.viewport.bottom=th;
 if(c->core.state.viewport.right>tw)c->core.state.viewport.right=tw;
 /* not drawn by the engine yet */
 if(ctx->RenderMode!=GL_RENDER)fall|=VXGL_FB_RENDERMODE;
 /* v10.279 (review): GL_ARB_occlusion_query (GL 1.5) counts in swrast's fragment path; the engine's draws were not counted (a visible
  * triangle drew under a query and the query answered 0 samples). Software while a query counts. */
 if(ctx->Query.CurrentOcclusionObject&&!c->query_hw)fall|=VXGL_FB_QUERY; /* v10.315: unless the engine counts it (vxgl_mesa.c begin_query) */
 if(ctx->ATIFragmentShader._Enabled&&!ctx->FragmentProgram._Current){if(!vxgl_gl_fprog)fall|=VXGL_FB_PROGRAM;rs[29]=0;} /* v10.321: an ATI shader as stages too (no colour sum) */
 /* v10.321: a fragment program as stages (vxgl_fp.c, from vxgl_tex_state; ACCEL_OPENGL_FPROG). swrast adds no specular colour (the
  * program reads it) and fogs only with a fog option (s_context.c), from GL's fog state: the engine's per-vertex factors when GL_FOG is
  * on in that mode */
 if(ctx->FragmentProgram._Current){const struct gl_fragment_program *fp=ctx->FragmentProgram._Current;
  /* v10.353: the program's fog option in any mode: tnl's per-vertex factors made in the option's mode for the engine's draws (run_pipeline
   * sets GL_FOG and its mode for tnl meanwhile; swrast fogs a program with an option whether or not GL_FOG is on) */
  if(!vxgl_gl_fprog)fall|=VXGL_FB_PROGRAM;
  rs[28]=fp->FogOption!=GL_NONE?1:0;rs[29]=0;}
 /* v10.336: which condition, for VXGL.LOG's "fallback on" line (board, Quake 2: 0021 at the end of every frame, the reason unknown) */
 c->fall_why=0;
 if(ctx->Polygon.StippleFlag&&!vxgl_gl_pstipple){fall|=VXGL_FB_STIPPLE;c->fall_why|=1;}
 /* v10.321: antialiasing on the engine (vxgl_tris.c), the coverage in the vertex alpha. swrast multiplies the alpha after the alpha test:
  * software with the test on (and below, a texture stage that does not carry the vertex alpha). A point sprite is not smoothed (swrast). */
 c->aa_point=ctx->Point.SmoothFlag&&!ctx->Point.PointSprite;c->aa_line=ctx->Line.SmoothFlag;c->aa_tri=ctx->Polygon.SmoothFlag;
 /* v10.341 (board, Quake 2 3.14: GL_POINT_SMOOTH and GL_ALPHA_TEST on for the whole game sent every triangle to software): a point's or
  * a line's condition sends only draws that have points or lines (run_pipeline, VXGL_FB_PRIMSTATE); a polygon's sends everything */
 c->fall_pt=0;c->fall_ln=0;
 /* v10.353: smooth points and lines with the alpha test and nothing that changes the alpha between the vertex and the test (no texture, no
  * fragment program or ATI shader): a primitive's alpha is its own (a point's, or a line's when both ends have the same: run_pipeline sends
  * other lines to software), so swrast's test (before the coverage) is made here per primitive, and the engine draws those that pass with
  * its own test off (run_pipeline). Smooth polygons with the test stay in software. ACCEL_OPENGL_SMOOTHATEST = 0: software, as before */
 c->aa_atest=vxgl_gl_smoothatest&&vxgl_gl_smooth&&ctx->Color.AlphaEnabled&&(c->aa_point||c->aa_line)&&!ctx->Texture._EnabledUnits&&
  !ctx->FragmentProgram._Current&&!ctx->ATIFragmentShader._Enabled;
 if((c->aa_point||c->aa_line||c->aa_tri)&&(!vxgl_gl_smooth||(ctx->Color.AlphaEnabled&&(!c->aa_atest||c->aa_tri)))){if(c->aa_tri)fall|=VXGL_FB_STIPPLE;if(c->aa_point)c->fall_pt=1;if(c->aa_line)c->fall_ln=1;c->fall_why|=(c->aa_point?2:0)|(c->aa_line?4:0)|(c->aa_tri?8:0)|(ctx->Color.AlphaEnabled?16:0);}
 /* v10.308: polygon stipple on the engine (board, v10.307 probe: block 14030000 row k = the target's row k from its top, bit b = column b
  * from its left, aligned to the target's corner; 01/01 bit 1). The engine draws into the window-sized back surface, whose row k is
  * window row h - 1 - k; Mesa's PolygonStipple[y % 32] has column x % 32 in bit 31 - x % 32: each row flipped and its bits reversed.
  * Lines and points are not stippled: a batch with them goes to software (vxgl_tris.c) */
 memset(c->core.state.stip,0,sizeof(c->core.state.stip));c->pstip=0;
 if(ctx->Polygon.StippleFlag&&vxgl_gl_pstipple){GLuint k,h=(GLuint)th;c->pstip=1;c->core.state.stip[0]=1;
  for(k=0;k<32;k++)c->core.state.stip[1+k]=bitrev32(ctx->PolygonStipple[c->rtt?k:(h+31-k)&31]);} /* v10.321: a framebuffer object's row k is GL's */
 /* v10.300: line stipple (ACCEL_OPENGL_LINES), attenuated points and point sprites (ACCEL_OPENGL_POINTS) on the engine, as swrast draws
  * them (vxgl_tris.c) */
 c->stip_on=ctx->Line.StippleFlag;c->stip_factor=(GLuint)(ctx->Line.StippleFactor<1?1:ctx->Line.StippleFactor);c->stip_pattern=ctx->Line.StipplePattern;
 /* v10.321: a vertex program's point size (VERTEX_PROGRAM_POINT_SIZE) per vertex as an attenuated point's (Mesa's TNL condition for the
  * size attribute, t_context.c); without a vertex program GL takes glPointSize */
 c->point_atten=ctx->Point._Attenuated||(ctx->VertexProgram._Enabled&&ctx->VertexProgram.PointSizeEnabled);c->point_sprite=ctx->Point.PointSprite;
 if(ctx->Line.StippleFlag&&(!vxgl_gl_lines||!vxgl_gl_wide)){c->fall_ln=1;c->fall_why|=32;}
 if((c->point_atten||ctx->Point.PointSprite)&&(!vxgl_gl_points||!vxgl_gl_wide)){c->fall_pt=1;c->fall_why|=64;}
 /* v10.295: wide lines and large points as two triangles each (vxgl_tris.c); the width and size rounded to whole pixels as GL does for
  * lines and points without antialiasing (Mesa's swrast: a line's width truncated, a point's size rounded). ACCEL_OPENGL_WIDE = 0: software */
 c->wide_line=0.0F;c->wide_point=0.0F;
 if(ctx->Line._Width!=1.0F){GLfloat w=(GLfloat)(int)ctx->Line._Width; /* (swrast truncates the width) */if(!vxgl_gl_wide){c->fall_ln=1;c->fall_why|=128;}else if(w>1.0F)c->wide_line=w;}
 if(vxgl_gl_wide)c->wide_point=ctx->Point._Size;else if(ctx->Point._Size!=1.0F){c->fall_pt=1;c->fall_why|=256;}
 /* v10.268: textures on the engine when nothing else sends the draw to software (the copies are only made then) */
 if(!fall&&!(c->fallback&(VXGL_FB_NOHW|VXGL_FB_REGISTRY))){if(!vxgl_tex_state(c)){fall|=VXGL_FB_TEXTURE;c->fall_why|=512;}}
 else {int st;c->tex_units=0;c->fp_on=0;for(st=0;st<VX_D3D_STAGES;st++)memcpy(c->core.state.tss[st],c->tss_default[st],sizeof(c->tss_default[st]));if(ctx->Texture._EnabledUnits)fall|=VXGL_FB_TEXTURE;}
 /* v10.353: a fragment program's KIL as the engine's alpha test (vxgl_fp.c); the application's own alpha test as well: software */
 if(!fall&&c->fp_on&&c->fp_kil){if(ctx->Color.AlphaEnabled){fall|=VXGL_FB_PROGRAM;c->fall_why|=2048;}else{rs[15]=1;rs[25]=cmp(c->fp_kil==2?GL_NEVER:GL_GEQUAL);rs[24]=c->fp_kilref;}}
 if(!fall&&(c->aa_point||c->aa_line||c->aa_tri)&&!aa_alpha_carried(c)){if(c->aa_tri)fall|=VXGL_FB_STIPPLE;if(c->aa_point)c->fall_pt=1;if(c->aa_line)c->fall_ln=1;c->fall_why|=1024|(c->aa_point?2:0)|(c->aa_line?4:0)|(c->aa_tri?8:0);} /* v10.321; v10.341 per primitive */
 /* v10.321 (review): a texture drawn into that is also sampled may have had its copy made again for the sampling (another level, another
  * layout: the target's memory freed): the target must still be the copy as it is now, or the draw goes to software */
 if(c->rtt&&!fall){vx3dn_surface t2,z2;if(!vxgl_fbo_target(c,fb,0,&t2,&z2)||memcmp(&t2,&c->core.target,sizeof(t2))||memcmp(&z2,&c->core.depth,sizeof(z2)))fall|=VXGL_FB_DRAWBUFFER;}
 if(fall&VXGL_FB_TEXTURE){int st;c->tex_units=0;for(st=0;st<VX_D3D_STAGES;st++)memcpy(c->core.state.tss[st],c->tss_default[st],sizeof(c->tss_default[st]));}
 if(memcmp(&old,&c->core.state,sizeof(old))||c->tex_changed){VXGL_PROF_BEGIN;vx_d3d_cut_tag=6;vxgl_hw_flush(c);vx_d3d_gl_state();c->tex_changed=0;VXGL_PROF_END(5);}
 c->xlate_old=0;
 /* with no buffers for the engine (NOHW, ACCEL_OPENGL_HW = 0) the core's answer is always "targets" (1): not asked; logged when it changes */
 if(!fall&&!(c->fallback&(VXGL_FB_NOHW|VXGL_FB_REGISTRY))) {
  static uint32_t logged=0xFFFFFFFFUL;uint32_t why=vx_d3d_unsupported_why(&c->core);
  if(why){fall|=VXGL_FB_HWSTATE;if(why!=logged)vxgl_log("state not drawable by the engine: %03lX\n",(unsigned long)why);}
  logged=why;
 }
 vxgl_fallback_bits(c,VXGL_FB_DEPTHBOUNDS|VXGL_FB_TEXTURE|VXGL_FB_HWSTATE|VXGL_FB_LOGICOP|VXGL_FB_COLORMASK|VXGL_FB_BLEND|VXGL_FB_STIPPLE|VXGL_FB_DRAWBUFFER|VXGL_FB_RENDERMODE|VXGL_FB_PROGRAM|VXGL_FB_QUERY,fall);
}

/* window coordinates for the engine: y flipped (row 0 at the top), the pixel centre at the integer (Direct3D, -0.5), z in 0..1. v10.321: a
 * framebuffer object's surfaces are not flipped (its row 0 is GL's, a texture's first row) */
int vxgl_flip(const vxgl_ctx *c) {return c->fb&&c->gl.DrawBuffer==&c->fb->Base;}
static GLfloat target_h(const vxgl_ctx *c) {return vxgl_flip(c)?(GLfloat)c->fb->h:c->gl.DrawBuffer?(GLfloat)c->gl.DrawBuffer->Height:0.0F;}
GLfloat vxgl_wy(const vxgl_ctx *c,GLfloat sy) {return vxgl_flip(c)?target_h(c)-0.5F-sy:sy+0.5F;}
GLfloat vxgl_sy(const vxgl_ctx *c,GLfloat wy) {return vxgl_flip(c)?target_h(c)-0.5F-wy:wy-0.5F;}
void vxgl_calc_viewport(GLcontext *ctx) {
 vxgl_ctx *c=VXGL_CTX(ctx);const GLfloat *v=ctx->Viewport._WindowMap.m;GLfloat *m=c->ViewportMatrix.m;GLfloat h=target_h(c);int flip=vxgl_flip(c);
 GLfloat ds=ctx->DrawBuffer&&ctx->DrawBuffer->_DepthMaxF>0?1.0F/ctx->DrawBuffer->_DepthMaxF:c->depth_scale;
 c->depth_scale=ds;
 c->offset_scale=ctx->DrawBuffer&&ctx->DrawBuffer->Visual.depthBits>16?8.0F/16777215.0F:2.0F/65535.0F;
 m[MAT_SX]=v[MAT_SX];m[MAT_TX]=v[MAT_TX]-0.5F;
 if(flip){m[MAT_SY]=-v[MAT_SY];m[MAT_TY]=-v[MAT_TY]+h-0.5F;}else{m[MAT_SY]=v[MAT_SY];m[MAT_TY]=v[MAT_TY]-0.5F;}
 m[MAT_SZ]=v[MAT_SZ]*ds;m[MAT_TZ]=v[MAT_TZ]*ds;
 c->vp_flip=flip;c->vp_h=h;
}
