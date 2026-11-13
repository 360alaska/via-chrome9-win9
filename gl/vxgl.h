/* VXGL.DLL: the VX855 OpenGL ICD (v10.239). Mesa 7.0.3 does OpenGL (state, transform, lighting, clipping); this driver hands
 * Mesa's transformed vertices to the HAL's portable core (src/vx_d3d_core.c, the packet builder proven with Direct3D), which
 * sends draw packets to the VxD (VX_IOCTL_3DN_SUBMIT). DirectDraw allocates the back buffer and depth buffer in video memory
 * and does the swap (a blit through the 2D engine, clipped to the window). What the engine cannot do as asked goes to Mesa's
 * software rasterizer (swrast), writing the same video memory through a DirectDraw lock. */
#ifndef VXGL_H
#define VXGL_H
#include <windows.h>
#include <ddraw.h>
#include "glheader.h"
#include "mtypes.h"
#include "../src/vx_native3d.h"
#include "../src/vx_d3d_core.h"
#include "math/m_matrix.h"
#include "tnl/t_context.h"
#include "tnl/t_vertex.h"

#define VXGL_VERSION "3.73"

/* a window's buffers: DirectDraw surfaces in video memory */
typedef struct vxgl_fb {
 struct gl_framebuffer Base;
 int swap_interval,swap_interval_set; /* v10.354: window state, WGL_EXT_swap_control (default 1) */
 int swap_logged; /* v10.422: swaps logged for this window (VXGL.LOG: the first 8 of each window, not of the process) */
 HWND hwnd;
 GLuint w,h;            /* client size the surfaces were made for */
 LPDIRECTDRAWSURFACE7 back,depth;
 vx3dn_surface sback,sdepth;  /* offsets in video memory for the VxD (sdepth.format 0: none) */
 GLubyte *pback,*pdepth;      /* CPU addresses (valid while video memory stays mapped; used under a lock) */
 GLubyte *pstencil;           /* v10.245: D24S8's stencil, one byte a pixel in its own plane after the Z words (as the engine keeps it) */
 DWORD soff,spitch;           /* the plane's video-memory offset (from the HAL, VX_DD_StencilPlane) and pitch */
 LPDIRECTDRAWSURFACE7 front;  /* v10.249: a double-buffered window's front buffer, made when first drawn or read (0: none yet) */
 vx3dn_surface sfront;        /* v10.317: its offset and pitch for the engine (format 0: not drawable by the engine) */
 GLubyte *pfront;             /* its CPU address under a lock (the back's when there is no front surface) */
 int fstate;                  /* v10.249: 1 front holds the window's pixels, 2 front drawn since it was last shown */
 int dmg_on[2];RECT dmg[2];   /* v10.366: what was drawn into the back [0] / front [1] since it was last shown (a single-buffered window's
                                * back and the front show only that: the window's other pixels, put there by GDI, stay) */
 int locked;                  /* swrast is writing: both surfaces locked (a count) */
 int held;                    /* which DirectDraw locks succeeded: 1 back, 2 depth, 4 front */
 int hold,hold_ok;            /* v10.267: swrast finished but the locks are kept for its next batch (vxgl_fb_settle lets them go) */
 int cb;                      /* colour bits fixed at SetPixelFormat (the desktop's then) */
 GLubyte *cmem,*zmem;         /* system memory standing in for a surface that could not be made or locked (zmem: the depth buffer
                                 when there is no depth surface; swrast draws everything then) */
 GLubyte *smem;               /* v10.245: the stencil plane's stand-in */
 DWORD cmem_size,zmem_size,smem_size;
 int pf;                      /* pixel format index (1-based) */
 int fsoft;                   /* v10.261: the front buffer is in system memory (its surface could not be made or locked) */
 void *fdc,*fbmp,*fold;       /* v10.261: that memory as a DIB section in a memory DC (GDI fills it from the window and shows it) */
 GLubyte *fbits;              /* v10.261: its pixels (the DIB's, or plain memory when no DIB could be made: then not shown) */
 GLubyte *fmem; DWORD fmem_size; /* v10.261: the plain memory */
 unsigned made;               /* v10.288: the times the surfaces were made (a context points the core at them again when it changed) */
 int attached;                /* Mesa's framebuffer and our renderbuffers are on it */
 int zdirty;                  /* v10.296: swrast wrote depth since the VxD was last told (vxgl_z_written) */
 struct vxgl_fb *next;
} vxgl_fb;

/* our renderbuffer: the Mesa renderbuffer plus where its pixels are */
typedef struct vxgl_rb {
 struct gl_renderbuffer Base;
 vxgl_fb *fb;
 int depth;                    /* 0 colour, 16 depth16, 24 depth24/stencil8 */
 int front;                    /* v10.249: the front colour buffer of a double-buffered window (its own surface) */
 /* v10.296: a depth renderbuffer's own writers (vxgl_mesa.c calls them after noting the write: the engine's Coarse Z records) */
 void (*put_row)(GLcontext *,struct gl_renderbuffer *,GLuint,GLint,GLint,const void *,const GLubyte *);
 void (*put_mono_row)(GLcontext *,struct gl_renderbuffer *,GLuint,GLint,GLint,const void *,const GLubyte *);
 void (*put_values)(GLcontext *,struct gl_renderbuffer *,GLuint,const GLint *,const GLint *,const void *,const GLubyte *);
} vxgl_rb;

#define VXGL_FB(f) ((vxgl_fb*)(f))
#define VXGL_RB(r) ((vxgl_rb*)(r))

/* fallback reasons: any set -> swrast draws (bit names in vxgl_fallback_name) */
#define VXGL_FB_DEPTHBOUNDS 0x10000 /* v10.354: EXT_depth_bounds_test is performed by swrast */
#define VXGL_FB_TEXTURE    0x0001
#define VXGL_FB_HWSTATE    0x0002 /* vx_d3d_unsupported_why says no */
#define VXGL_FB_LOGICOP    0x0004
#define VXGL_FB_COLORMASK  0x0008
#define VXGL_FB_BLEND      0x0010
#define VXGL_FB_STIPPLE    0x0020
#define VXGL_FB_DRAWBUFFER 0x0040
#define VXGL_FB_RENDERMODE 0x0080 /* feedback / select */
#define VXGL_FB_NOHW       0x0100 /* no surfaces in video memory */
#define VXGL_FB_PROGRAM    0x0200 /* vertex/fragment programs or shaders */
#define VXGL_FB_REGISTRY   0x0400 /* ACCEL_OPENGL_HW = 0 */
#define VXGL_FB_PSTIPVERT  0x2000 /* v10.308: lines, points or unfilled polygons while polygon stipple is on (the engine's stipple would take them) */
#define VXGL_FB_QUERY      0x1000 /* v10.279: an occlusion query is counting (swrast counts the samples; the engine does not) */
#define VXGL_FB_ZNOTICE    0x8000 /* v10.342: swrast wrote the depth and the VxD has not been told yet (the notice failed): a depth-tested draw stays in software */
#define VXGL_FB_PRIMSTATE  0x4000 /* v10.341: points or lines drawn while a points-only or lines-only condition needs software (c->fall_pt / fall_ln) */
#define VXGL_FB_TEXVERT    0x0800 /* v10.268: textured lines or points, or q other than 1 (vxgl_tex_vertices_ok, decided before each pipeline run) */

typedef struct vxgl_ctx {
 GLcontext gl;                /* first */
 vx_d3d_context core;
 vx_d3d_services sv;
 GLuint fallback;
 GLuint fall_why;
 GLuint fall_pt,fall_ln; /* v10.341: software only for draws with points / lines (smoothing with the alpha test, stipple, wide, attenuation switched off) */ /* v10.336: which condition set VXGL_FB_STIPPLE / VXGL_FB_TEXTURE (vxgl_state.c FW_*), for VXGL.LOG */
 GLuint render_index;
 GLuint new_state;            /* Mesa state bits not yet looked at */
 GLuint vertex_size;          /* bytes */
 GLfloat vs_rcp;              /* v10.481: 1 / vertex_size (hw_prim's vertex numbers without a division) */
 GLuint coloroffset,specoffset; /* dwords */
 GLuint fvf;
 GLuint render_prim,hw_prim;
 GLfloat depth_scale;         /* 1 / DepthMax */
 GLfloat offset_scale;        /* polygon offset's unit in 0..1 depth: two steps of a 16-bit buffer, eight of a 24-bit one (a float z
                                 near 0.5 has 2^-24 steps; one 24-bit step was lost to rounding: host test scene 10) */
 GLmatrix ViewportMatrix;
 struct tnl_attr_map attrs[8];
 GLuint attr_count;
 GLubyte *verts;
 int join;                    /* strips may join (plain render path) */
 GLuint stats[8];             /* 0 tris hw, 1 lines hw, 2 points hw, 3 sw primitives, 4 packets, 5 clears hw, 6 fallback entries, 7 swaps */
 uint32_t fb_draws[17],fb_why_draws[11]; /* v10.500: draws in software by fallback bit (VXGL_FB_*) and by the state reason behind it (fall_why): the context's end in VXGL.LOG */
 GLuint fl_s0[8];char fl_buf[700];int fl_len,fl_after_clear;GLuint fl_frames; /* v10.343: VXGL_FRAMELOG's line for this frame */
 HDC hdc;
 vxgl_fb *fb;
 int pf;
 int cull_all;                /* v10.263: glCullFace(GL_FRONT_AND_BACK): every filled polygon dropped here, lines and points drawn */
 int twoside_stencil;         /* v10.263: front and back stencil state differ: each triangle gets its face's (vxgl_stencil_face) */
 int stencil_face;            /* v10.263: the face whose stencil state the core holds now (0 front, 1 back) */
 char *ext;                   /* v10.263: the extension string trimmed to ACCEL_OPENGL_EXTLEN (0: Mesa's own) */
 DWORD owner;                 /* v10.259: the thread it is current in (0 = none): another thread cannot bind or delete it */
 DWORD magic;                 /* v10.259: VXGL_MAGIC while alive (0 once torn down) */
 int tex_units;               /* v10.268: texture stages the engine draws with (0: untextured) */
 int tex_unit_of[3];          /* v10.268: the GL unit of each stage */
 int tex_cx[3];              /* v10.357: a stage's GL_CLAMP ring copy: s (1) / t (2) must stay in 0..1 in the draw (vxgl_tex_vertices_ok) */
 uint32_t tss_default[VX_D3D_STAGES][VX_D3D_TSS]; /* v10.268: the core's stage state as vx_d3d_init made it */
 int tex_changed;             /* v10.268: a copy was made or moved: the core's template (which holds the resolved surface) is built again */
 vx_d3d_state *xlate_old;     /* v10.268: during vxgl_translate_state, the state the batch was built with (a flush then sends it under that) */
 unsigned long batch_lost;    /* v10.279: vxgl_batch_lost when this context last heard of a lost engine */
 unsigned fb_made;            /* v10.288: fb->made when the core was last pointed at its surfaces */
 int tex_layout;              /* v10.295: stage 0's coordinates: 0 two (s t), 1 a cube map's s t r, 2 projective s t q (the pipeline's vxgl_proj stage puts q third) */
 GLvector4f proj_store;      /* v10.295: the projective coordinates s t q for the vertex emit */
 int proj_size;              /* v10.295: its allocated vertices */
 GLfloat wide_line,wide_point; /* v10.295: the line width (> 1: two triangles) and point size (> 1, or textured: two triangles), rounded as GL does */
 int stip_on;GLuint stip_count,stip_factor,stip_pattern; /* v10.300: line stipple on the engine (swrast's counter, reset as tnl resets it) */
 int point_atten,point_sprite; /* v10.300: attenuated points (the size per vertex), point sprites (coordinates across the point) */
 int aa_point,aa_line,aa_tri; /* v10.321: antialiased points, lines, polygons on the engine (vxgl_tris.c) */
 int aa_atest,atest_off; /* v10.353: smooth points / lines with GL_ALPHA_TEST, untextured: the test done here on the primitive's own alpha (vxgl_tris.c) */
 int fp_on,fp_nst,fp_tf_used,fp_ati,fp_kil;uint32_t fp_ops[3][10],fp_tf,fp_kilref; /* v10.353: fp_kil 1 KIL as the alpha test GREATEREQUAL fp_kilref, 2 every fragment killed */ /* v10.321: the fragment program's stages (vxgl_fp.c): colour op a1 a2 a0 scale, alpha the same */
 int q_forced; /* v10.300: the only unit's vertices carry q other than 1: drawn projective (layout 2) */
 int pstip;     /* v10.308: polygon stipple on the engine (the core's state.stip) */
 int query_hw;  /* v10.315: an occlusion query counts on the engine (VX3DN_query) */
 uint32_t query_owner; /* v10.322: the context's token for the VxD's count (one context's at a time) */
 int tex_set2;  /* v10.307: the set the third stage read last (0 or 1) */
 int tex_sets;  /* v10.307: coordinate sets in the vertex (at most 2; a third stage reads one of them, VX_D3D_TSS 11) */
 /* v10.317: a stage whose copy is padded (a non-power-of-two size, a rectangle texture, a ring of the border colour) takes s' = sa s + sb q,
  * t' = ta t + tb q: the vxgl_texxf stage after the texture matrix writes them into xf_store (only while the engine draws) */
 struct {int on;GLfloat sa,sb,ta,tb;} tex_xf[3];
 GLvector4f xf_store[3];int xf_size[3];
 int both,both_front; /* v10.340: glDrawBuffer(GL_FRONT_AND_BACK) on the engine: each draw sent to the back, then again to the front (vxgl_tris.c run_pipeline) */
 int front_hw;  /* v10.317: the engine draws into the double-buffered window's front surface (core.target is fb->sfront) */
 int tex_vol;   /* v10.317: stage 0 is a 3D texture (layout 3: s t r) */
 int tex_set3;  /* v10.321: set 1's coordinates s t r (a cube map or a 3D texture on stage 1) */
 int rtt;       /* v10.321: the engine draws into a framebuffer object (core.target and core.depth are its surfaces, not the window's) */
 int vp_flip;GLfloat vp_h; /* v10.321: the y flip and height vxgl_calc_viewport last used */
 int rtt_drawn; /* v10.321: the engine drew into a texture's copy since the target last changed (vxgl_fbo_drawn) */
} vxgl_ctx;
#define VXGL_MAGIC 0x4C47584EUL
#define VXGL_BROKEN 0x4B4F5242UL /* v10.261: a context found broken at delete: detached, never used again */
void vxgl_thread_gone(void);
void vxgl_stencil_face(vxgl_ctx *c,int face); /* v10.263 */

#define VXGL_CTX(c) ((vxgl_ctx*)(c))

/* v10.268: a texture's copy in video memory for the engine (vxgl_tex.c): levels 0..levels-1 from the base level, A8R8G8B8 or
 * X8R8G8B8, each level's offset 256-byte aligned and its rows the sampler's pitch (width x 4, 32 bytes at least) */
#define VXGL_TEXLEVELS 12
/* v10.317: block (bytes, 0 = the HAL's mip chain as before): the copy is one linear block of video memory (an offscreen surface) laid out by
 * vxgl_texmem_layout: level l at rel[l] from the block's start (256-byte aligned), rows the sampler's pitch. A cube map with mipmaps (each face
 * a block with the same layout: the engine takes face base + the level's offset, HCM p116 base mode 1) and a 3D texture (depth slices of
 * slice bytes from level 0) are kept so. */
typedef struct vxgl_texmem {void *surf[VXGL_TEXLEVELS];uint32_t off[VXGL_TEXLEVELS],pitch[VXGL_TEXLEVELS];GLuint w,h,levels,format;
 uint32_t block,rel[VXGL_TEXLEVELS],depth,slice;} vxgl_texmem;
uint32_t vxgl_texmem_layout(vxgl_texmem *m); /* v10.317 (vxgl_tex.c): rel, pitch and slice for a block copy; its bytes */
int vxgl_texmem_alloc(vxgl_texmem *m);   /* (vxgl_icd.c: DirectDraw texture surfaces; the host test: its video memory) */
void vxgl_texmem_free(vxgl_texmem *m);
GLubyte *vxgl_texmem_lock(vxgl_texmem *m,GLuint level,uint32_t *pitch); /* waits for the engine */
void vxgl_texmem_unlock(vxgl_texmem *m,GLuint level);
int vxgl_texmem_lost(vxgl_texmem *m);    /* the memory was lost (a mode change); v10.287: only the test, vxgl_texmem_restore gets it back */
int vxgl_texmem_restore(vxgl_texmem *m); /* v10.287: restored, each level's offset and pitch read again (0: freed, made again by the caller) */
extern unsigned long vxgl_texmem_freed;  /* v10.287: video memory given back (a texture's copy, a window's surfaces): a failed copy is tried again */
/* vxgl_tex.c */
void vxgl_tex_functions(struct dd_function_table *f);
int vxgl_tex_resolve(void *ctx,uint32_t handle,vx3dn_surface *s);
uint32_t vxgl_tex_mips(void *ctx,uint32_t handle,uint32_t *off,uint32_t max);
int vxgl_tex_state(vxgl_ctx *c);
int vxgl_tex_vertices_ok(vxgl_ctx *c,int set2);
extern int vxgl_gl_strips,vxgl_gl_snap; /* v10.323: ACCEL_OPENGL_STRIPS; v10.324: ACCEL_OPENGL_SNAP */
/* v10.487: the SSE pass for display lists (vxgl_ssepass.c): the pipeline's first stage; the vertex layout chosen ahead of the render stage */
extern const struct tnl_pipeline_stage vxgl_ssepass_stage;void vxgl_choose_vertex_state(GLcontext *ctx);extern int vxgl_ssepass_on;extern unsigned long vxgl_ssepass_stat[20];
extern int vxgl_gl_flatstrips,vxgl_gl_flatbatch,vxgl_gl_hwflatstrip;extern uint32_t vxgl_flat_stat[10]; /* v10.480: ACCEL_OPENGL_FLATSTRIP; strips sent whole, their triangles, strips left to Mesa's flat template, theirs */
extern int vxgl_gl_nocolor; /* v10.346: ACCEL_OPENGL_NOCOLOR */
extern int vxgl_gl_smoothatest,vxgl_gl_mirrorclamp; /* v10.353: ACCEL_OPENGL_SMOOTHATEST, ACCEL_OPENGL_MIRRORCLAMP */
extern int vxgl_gl_combine,vxgl_gl_cube,vxgl_gl_projected,vxgl_gl_logicop,vxgl_gl_colormask,vxgl_gl_wide; /* v10.295: ACCEL_OPENGL_COMBINE CUBE PROJECTED LOGICOP COLORMASK WIDE */
extern int vxgl_gl_smooth,vxgl_gl_fprog; /* v10.321: ACCEL_OPENGL_SMOOTH, ACCEL_OPENGL_FPROG */
int vxgl_fp_compile(vxgl_ctx *c,unsigned proj,int flat_only); /* v10.321 (vxgl_fp.c) */
extern int vxgl_gl_lines,vxgl_gl_points,vxgl_gl_blendenv,vxgl_gl_qvertex; /* v10.300: ACCEL_OPENGL_LINES POINTS BLENDENV QVERTEX */
int vxgl_tex_set2(vxgl_ctx *c);
extern int vxgl_gl_tex16,vxgl_gl_texring;extern unsigned long vxgl_ring_stat[4]; /* v10.366: ACCEL_OPENGL_TEX16, ACCEL_OPENGL_TEXRING (vxgl_tex.c) */
extern int vxgl_gl_units3,vxgl_gl_s3tc,vxgl_gl_pstipple,vxgl_gl_query,vxgl_gl_lodclamp; /* v10.307: ACCEL_OPENGL_UNITS3, ACCEL_OPENGL_S3TC; v10.315: ACCEL_OPENGL_QUERY */
extern int vxgl_gl_longmip,vxgl_gl_unitmip,vxgl_gl_border,vxgl_gl_scale,vxgl_gl_lerp,vxgl_gl_blendop; /* v10.304: ACCEL_OPENGL_LONGMIP UNITMIP BORDER SCALE LERP BLENDOP */
int vxgl_tex_q_varies(vxgl_ctx *c); /* v10.300 */
uint32_t vxgl_tex_faces(void *ctx,uint32_t handle,uint32_t *off);
/* v10.317 (each default 1, 0 = software as before): ACCEL_OPENGL_NPOT (non-power-of-two and rectangle textures, linear CLAMP_TO_BORDER),
 * ACCEL_OPENGL_FXT1 (the extension, decoded copies), ACCEL_OPENGL_DEPTHTEX (depth textures without compare), ACCEL_OPENGL_CTABLE (texture
 * colour tables), ACCEL_OPENGL_DOT3RGB, ACCEL_OPENGL_CUBEMIP, ACCEL_OPENGL_FRONT (drawing into the front buffer), ACCEL_OPENGL_TEX3D */
extern int vxgl_gl_npotwrap,vxgl_gl_unit3d; /* v10.321: ACCEL_OPENGL_NPOTWRAP, ACCEL_OPENGL_UNIT3D */
extern int vxgl_gl_npot,vxgl_gl_fxt1,vxgl_gl_depthtex,vxgl_gl_ctable,vxgl_gl_dot3rgb,vxgl_gl_cubemip,vxgl_gl_front,vxgl_gl_tex3d;
uint32_t vxgl_tex_volume(void *ctx,uint32_t handle,uint32_t *out); /* v10.317: a 3D texture's depth and slice step (0: not one) */

/* vxgl_icd.c */
extern HANDLE vxgl_vxd;
extern LPDIRECTDRAW7 vxgl_dd;
extern DWORD vxgl_vram_linear,vxgl_vram_size;
extern int vxgl_hw_enabled;
void vxgl_log(const char *fmt,...);
unsigned long vxgl_cycles(void); /* v10.271: the CPU's time stamp counter, low 32 bits (VXGL.LOG's profile) */
int vxgl_prof_on(void);
void vxgl_prof_swap(void); /* v10.347 */
extern double vxgl_prof_us[16];extern unsigned long vxgl_prof_n[16];extern double vxgl_prof_texels[2]; /* v10.303: 8-12 a texture upload's parts (vxgl_tex.c); texels copied directly, by FetchTexelc */ /* v10.289: 4 texture uploads, 5 the translation's send of the batch (its state changed) */ /* (cycles until the report converts them) */
/* .506 (ACCEL_OPENGL_TSCPROF, VXFLAT turns it on for its children): the CPU's cycles in the ICD's main parts, for the board's own ranking:
 * [0] pipeline runs (a draw's whole ICD side), [1] triangle-list runs (the core's triangle loop), [2] strips, polygons, fans and quads to the
 * core, [3] the SSE pass's transform and lighting (not its render), [4] Mesa's vertex emit into the store (build_vertices), [5] packets to the
 * VxD (vxgl_submit), [6] swaps; read and cleared by VXGL_Stats506 */
extern int vxgl_tsc_on;extern double vxgl_tsc[8];extern unsigned long vxgl_tsc_n[8];
#define VXT_BEGIN unsigned long vxt_t0_=vxgl_tsc_on?vxgl_cycles():0
#define VXT_END(k) do{if(vxgl_tsc_on){vxgl_tsc[k]+=(double)(unsigned long)(vxgl_cycles()-vxt_t0_);vxgl_tsc_n[k]++;}}while(0)
#define VXGL_PROF_BEGIN int prof_on_=vxgl_prof_on();unsigned long prof_t0_=prof_on_?vxgl_cycles():0
#define VXGL_PROF_END(k) do{if(prof_on_){vxgl_prof_us[k]+=(double)(unsigned long)(vxgl_cycles()-prof_t0_);vxgl_prof_n[k]++;}}while(0)
uint32_t vxgl_submit(void *ctx,const vx3dn_packet *p);
int vxgl_fb_validate(vxgl_ctx *c,vxgl_fb *fb);   /* surfaces exist for the window's size */
int vxgl_fb_lock(vxgl_fb *fb);    /* CPU access (waits for the engine) */
void vxgl_fb_unlock(vxgl_fb *fb);
void vxgl_fb_settle(vxgl_fb *fb); /* v10.267: the kept locks let go (before the engine, a blit, a swap, a switch) */
int vxgl_pf_info(int pf,int *db,int *zbits,int *sbits,int *cbits);
int vxgl_swap_blit(vxgl_fb *fb);
int vxgl_front_load(vxgl_fb *fb);    /* v10.249: the front surface made and filled from the window, if it is not already */
void vxgl_front_present(vxgl_fb *fb); /* v10.249: the front surface's drawing shown in the window */
void vxgl_fb_release(vxgl_fb *fb); /* the window is gone: its reference dropped; freed when no context holds it */
void vxgl_fb_destroy(vxgl_fb *fb); /* (vxgl_icd.c) the surfaces and the structure freed */
vxgl_fb *vxgl_fb_for(HDC hdc);
void vxgl_engine_lock(void);   /* the core and the engine are one per process: a recursive lock around every use (two threads) */
void vxgl_engine_unlock(void);
int vxgl_vxd_info(vx3dn_info *i);
/* v10.302 (review): the state groups the translation reads (vxgl_state.c, vxgl_tex.c): a change of the modelview or projection
 * matrix, the arrays, hints, accumulation, pixel packing or evaluators no longer translates the state again (a 2 KB compare and a lost
 * check of each texture for every glRotate between draws). A mode change still does: the surfaces made again set every bit. */
#define VXGL_STATE_MASK (~(GLuint)(_NEW_MODELVIEW|_NEW_PROJECTION|_NEW_ACCUM|_NEW_EVAL|_NEW_HINT|_NEW_PACKUNPACK|_NEW_ARRAY|_NEW_TRACK_MATRIX))
extern int vxgl_framelog; /* v10.343 */
int vxgl_vxd_query(uint32_t op,uint32_t owner,uint32_t *count); /* v10.322: owner = the context's query_owner; 2 put aside, 3 taken back, 4 given up */ /* v10.315: VX_IOCTL_3DN_QUERY: 0 the engine's occlusion count set to 0, 1 read; 1 = done */
int vxgl_engine_wait(void); /* v10.302: waits for the engine (2D and 3D) without a surface lock; 0 when the VxD was not reached */
int vxgl_z_written(vxgl_fb *fb); /* v10.296: swrast wrote the depth surface: the VxD drops its Coarse Z records; v10.301: 1 when it was told */
/* vxgl_batch.c (v10.275): the packet queue; vxgl_batch_send is the ICD's (the VxD) or the host test's (its model) */
extern DWORD vxgl_batch_max;
extern unsigned long vxgl_batch_calls,vxgl_batch_packets,vxgl_batch_flushes[4];
extern unsigned long vxgl_batch_lost; /* v10.279: queued packets the VxD answered VX3DN_LOST for (each context's next submit hears of it) */
int vxgl_batch_add(const vx3dn_packet *p);   /* 1 queued; 0 the packet goes alone (send the queue first) */
uint32_t vxgl_batch_flush(void);             /* everything queued sent; the packets that went */
uint32_t vxgl_batch_flush_why(int why);
uint32_t vxgl_batch_send(const vx3dn_packet *p,uint32_t n,uint32_t *done);
extern uint32_t vxgl_seq_q,vxgl_seq_sent,vxgl_seq_done;extern int vxgl_seq_ring; /* v10.378: the VxD's ring on */ /* v10.366: packets handed over, given to the VxD, finished by the engine */
void vxgl_seq_idle(void);          /* the engine waited for: everything given is finished */
void vxgl_seq_alone(uint32_t rc);  /* a packet sent alone, with the VxD's answer */
GLubyte *vxgl_texmem_direct(vxgl_texmem *m,GLuint level,uint32_t *pitch); /* v10.366: the level's memory without a lock (no wait); 0: lock it */

/* vxgl_mesa.c */
vxgl_ctx *vxgl_create_context(int pf,HDC hdc,vxgl_ctx *share);
int vxgl_destroy_context(vxgl_ctx *c); /* v10.261: 1 freed, 0 not (a broken context: detached and quarantined) */
int vxgl_make_current(vxgl_ctx *c,HDC hdc);
int vxgl_swap(vxgl_ctx *c,HDC hdc);
void vxgl_hw_flush(vxgl_ctx *c);
extern vxgl_ctx *vxgl_batch_ctx; /* v10.287: the context whose draws the core's batch holds */
void vxgl_hw_flush_all(void);    /* v10.287: the core's batch sent, whichever context drew it */
void vxgl_span_start(GLcontext *ctx);
void vxgl_span_finish(GLcontext *ctx);
void vxgl_set_span_functions(vxgl_rb *rb,int bpp);

/* vxgl_tris.c */
void vxgl_init_tri_funcs(GLcontext *ctx);
void vxgl_fallback(vxgl_ctx *c,GLuint bit,GLboolean on);
void vxgl_fallback_bits(vxgl_ctx *c,GLuint mask,GLuint bits);
void vxgl_choose_render_state(GLcontext *ctx);
void vxgl_init_pipeline(GLcontext *ctx); /* v10.295: Mesa's pipeline with the projective stage */

/* vxgl_state.c */
void vxgl_translate_state(vxgl_ctx *c);
void vxgl_calc_viewport(GLcontext *ctx);
/* v10.321: the engine's surfaces and GL's window coordinates. The window's surfaces are y-flipped (row 0 at the top); a framebuffer object's
 * are not (row 0 is GL's row 0, a texture's first row). vxgl_wy: GL's window y of the engine's y; vxgl_sy: the engine's y of GL's window y */
int vxgl_flip(const vxgl_ctx *c);
GLfloat vxgl_wy(const vxgl_ctx *c,GLfloat sy);
GLfloat vxgl_sy(const vxgl_ctx *c,GLfloat wy);

/* vxgl_fbo.c (v10.321): framebuffer objects on the engine (ACCEL_OPENGL_RTT): a texture attachment (level of a 2D texture of base format RGB)
 * drawn in its engine copy, colour (RGB) and depth renderbuffers in video memory (X8R8G8B8, D16); swrast reaches them through these */
extern int vxgl_gl_rtt;
void vxgl_fbo_functions(struct dd_function_table *f);
int vxgl_fbo_target(vxgl_ctx *c,struct gl_framebuffer *fb,int make,vx3dn_surface *t,vx3dn_surface *z); /* 1: the engine draws into fb (make 0: only with the copies as they are) */
void vxgl_fbo_drawn(vxgl_ctx *c);     /* the engine drew or cleared the draw framebuffer's colour: its texture's image is older than the copy */
void vxgl_fbo_span_finish(GLcontext *ctx); /* after swrast: what it wrote through our renderbuffers, and Mesa's own texture wrappers */
void vxgl_fbo_settle(void);           /* our renderbuffers' CPU locks let go (before the engine draws or a copy is read back) */
/* vxgl_tex.c (v10.321) */
int vxgl_tex_rtt(vxgl_ctx *c,struct gl_texture_object *t,GLuint level,int make,vxgl_texmem **m,GLuint *l); /* the copy level the engine draws into (make 0: only an existing one) */
void vxgl_tex_drawn(struct gl_texture_object *t,GLuint level); /* that level of the copy is newer than Mesa's image */
int vxgl_tex_sync(GLcontext *ctx,struct gl_texture_object *t); /* Mesa's images read back from the copy where it is newer; v10.329: 0 while a level could not be */
void vxgl_tex_drop_drawn(struct gl_texture_object *t); /* v10.329: what the engine drew into the copy given up (the image is to be drawn by swrast instead) */
void vxgl_tex_changed(struct gl_texture_object *t); /* Mesa's image changed (Mesa's own render-to-texture): the copy is made again */
void vxgl_tex_sync_units(GLcontext *ctx); /* before swrast samples: the enabled units' textures read back */
GLubyte *vxgl_texmem_map(vxgl_texmem *m,GLuint level,uint32_t *pitch); /* v10.321: as vxgl_texmem_lock, for reading too */
int vxgl_z_written_at(const vx3dn_surface *d); /* v10.321: swrast wrote this depth surface (the VxD drops its Coarse Z records) */
#endif
