/* Portable legacy Direct3D DP2 core, independently implemented from the
 * Microsoft DDI documentation listed in docs/DDI_REFERENCES.md. MIT, 2026. */
#ifndef VX_D3D_CORE_H
#define VX_D3D_CORE_H
#include "vx_native3d.h"
#define VX_D3D_RS 256
#define VX_F_PRIMBATCH 16777216UL /* v10.368: ACCEL_D3D_PRIMBATCH: draws on the primary's memory queued for SUBMITN too (default 1) */
#define VX_F_RSSYNC 33554432UL /* v10.428: ACCEL_D3D_RSSYNC: pixel-pipeline render states the runtime recorded but never sent are taken from its state array (default 1) */
#define VX_F_GUARD 8388608UL /* v10.358: ACCEL_3D_GUARD: vertices at -0.5 on a window edge of 0 drawn by the engine (fast path, VxD); default 0 */
#define VX_D3D_RS_BLENDOP 171 /* v10.304: Direct3D 8's D3DRS_BLENDOP (1 ADD, 2 SUBTRACT, 3 REVSUBTRACT, 4 MIN, 5 MAX; 0 = ADD), set by the OpenGL ICD */
#define VX_D3D_RS_BLENDFACTOR 193 /* v10.304: Direct3D 9's D3DRS_BLENDFACTOR (ARGB) for blend factors 14-17, set by the ICD */
#define VX_D3D_RS_LOGICOP 255
#define VX_D3D_TSS_CARG0 26  /* v10.304: Direct3D 8's COLORARG0 / ALPHAARG0, for D3DTOP_LERP (26) on stage 0 (the ICD) */
#define VX_D3D_TSS_AARG0 27
#define VX_D3D_TSS_DOT3RGB 29 /* v10.317: the ICD's GL_DOT3_RGB: DOTPRODUCT3 (24) into colour only, alpha by the stage's ALPHAOP */
#define VX_D3D_TSS_CSCALE 30 /* v10.304: the ICD's result scale of a stage, 0 / 1 = x2 / 2 = x4 (colour, alpha) */
#define VX_D3D_TSS_ASCALE 31 /* v10.295: the OpenGL ICD's logic op (0 off, 0x10 | the 4-bit truth table of S 1100 D 1010); never set by DP2 */
#define VX_D3D_STAGES 8
#define VX_D3D_TSS 32
#define VX_D3D_BLOCKS 8
#define VX_D3D_INVALID 0x80070057UL
#define VX_D3D_UNSUPPORTED 0x80004001UL
#define VX_D3D_UNPARSED 0x88760BB8UL
#define VX_D3D_SURFACELOST 0x887601C2UL
#define VX_D3D_STILLDRAWING 0x8876021CUL
#define VX_D3D_CONTEXT_BAD 0x00000200UL
#define VX_D3D_WRONGTEXTUREFORMAT (0x88760000UL|2072UL)
#define VX_D3D_COLOROP (0x88760000UL|2073UL)
#define VX_D3D_COLORARG (0x88760000UL|2074UL)
#define VX_D3D_ALPHAOP (0x88760000UL|2075UL)
#define VX_D3D_ALPHAARG (0x88760000UL|2076UL)
#define VX_D3D_TOOMANYOPS (0x88760000UL|2077UL)
#define VX_D3D_CONFLICTINGSTATE (0x88760000UL|2081UL)
typedef struct vx_d3d_state {
 uint32_t rs[VX_D3D_RS],tss[VX_D3D_STAGES][VX_D3D_TSS];
 vx3dn_rect viewport;
 uint32_t stip[33]; /* v10.308: OpenGL's polygon stipple as the engine takes it ([0] on, [1..32] vx3dn_stipple.rows); Direct3D leaves it 0 */
} vx_d3d_state;
typedef struct vx_d3d_block {
 uint32_t handle,rs_mask[8],tss_mask[8],viewport_mask;
 vx_d3d_state state;
} vx_d3d_block;
typedef struct vx_d3d_context {
 uint32_t generation,lost,recording;
 vx3dn_surface target,depth;
 vx_d3d_state state;
 vx_d3d_block blocks[VX_D3D_BLOCKS];
 uint32_t target_handle,depth_handle;
} vx_d3d_context;
typedef struct vx_d3d_input {
 const uint8_t *commands,*vertices;
 uint32_t command_bytes,vertex_bytes,vertex_count,vertex_stride,fvf;
 uint32_t flags,*render_states,render_state_count;
} vx_d3d_input;
typedef struct vx_d3d_services {
 void *context;
 uint32_t (*submit)(void *,const vx3dn_packet *);
 int (*resolve)(void *,uint32_t,vx3dn_surface *);
 uint32_t (*unknown)(void *,const uint8_t *,const uint8_t **);
 uint32_t (*bind_vertices)(void *,vx_d3d_input *);
 int (*colorkey)(void *,uint32_t,uint32_t *); /* v10.197: texture handle -> its DDCKEY_SRCBLT value (1) or no key (0); may be NULL */
 uint32_t (*mips)(void *,uint32_t,uint32_t *,uint32_t); /* v10.199: texture handle -> levels, with each level's offset; may be NULL */
 uint32_t (*faces)(void *,uint32_t,uint32_t *); /* v10.214: texture handle -> 6 (a cube map, face offsets +X -X +Y -Y +Z -Z), 0 (not a cube), 0xFFFFFFFF (a cube that cannot be sampled); may be NULL */
 uint32_t (*submit_n)(void *,const vx3dn_packet *,uint32_t,uint32_t *); /* v10.276: n draw packets in one call (*done = the packets taken), or NULL: one at a time */
 /* v10.317: texture handle -> a 3D texture: out[0] its depth (slices), out[1] the bytes from one slice to the next; 0 = not one; may be NULL */
 uint32_t (*volume)(void *,uint32_t,uint32_t *);
 int (*target)(void *,uint32_t,vx3dn_surface *); /* v10.456: SETRENDERTARGET's colour handle -> the surface as a target (a render-target texture in its target format); NULL: resolve */
 void (*target_done)(void *); /* v10.479: SETRENDERTARGET made (after target and the depth handle were accepted); may be NULL */
} vx_d3d_services;
/* v10.317: faces() | VX_D3D_FACES_SHARED: the six faces' levels lie at the same offsets from their own level 0 (the ICD's block copies), so
 * the engine's one level table (HCM p116 base mode 1) serves all six and a cube map keeps its mip chain */
#define VX_D3D_FACES_SHARED 0x100UL
void vx_d3d_init(vx_d3d_context *,const vx3dn_surface *,const vx3dn_surface *,uint32_t);
uint32_t vx_d3d_dp2(vx_d3d_context *,const vx_d3d_input *,const vx_d3d_services *,uint32_t *);
/* .550: the queue held across DrawPrimitives2 calls (vx_d3d_core.c, cq_hold) */
extern uint32_t vx_d3d_hold_on,vx_d3d_hold_stat[6];uint32_t vx_d3d_held(void);uint32_t vx_d3d_hold_send(const vx_d3d_services *);void vx_d3d_hold_drop(void);int vx_d3d_hold_is(const vx_d3d_context *);
int vx_d3d_state_adopt(vx_d3d_context *,uint32_t id,uint32_t value); /* v10.428 */
extern uint32_t vx_d3d_rs_synced;
extern uint32_t vx_d3d_tprof[8]; /* v10.462: a template build's parts */
uint32_t vx_d3d_vertex_size(uint32_t);
void vx_d3d_packet_copy(vx3dn_packet *d,const vx3dn_packet *s); /* v10.275 */
int vx_d3d_targets_valid(const vx3dn_surface *,const vx3dn_surface *);
uint32_t vx_d3d_submit_result(vx_d3d_context *,uint32_t,uint32_t);
int vx_d3d_supported(const vx_d3d_context *);
uint32_t vx_d3d_texture_status(const vx_d3d_context *);
uint32_t vx_d3d_draw_status(const vx_d3d_context *,const vx_d3d_services *);
int vx_d3d_uses_texture(const vx_d3d_context *);
/* v10.198: a stage-0 program, at most three instructions. v10.206: up to eight instructions over three texture stages; units = texture
 * units sampled (unit u is register u and samples stage unit_stage[u]'s texture with coordinate set unit_set[u]); c22 = the program reads
 * constant c22 (DOT3's -0.5) */
/* v10.208: ps = an explicit schedule (h90 h91 h93 h95, ntau TAU words), nkc constants (kc_reg = N of cN, four float bits each),
 * layout 2 = projected unit 0 (9-dword vertex, TAU 307001A0) */
typedef struct vx_d3d_xp {uint32_t count,words[32],textured,tfactor,units,unit_stage[3],unit_set[3],c22;
 uint32_t ps,h90,h91,h93,h95,ntau,tau[4],nkc,kc_reg[4],kc[4][4],layout;} vx_d3d_xp;
extern uint32_t vx_d3d_ckey_on;
/* v10.212: registry feature switches (HKLM\Software\VX85598, DWORD, 1 on / 0 off, default 1), from the VxD's query 0xFFFFFE50 out[3] */
#define VX_F_STAGES 1UL    /* ACCEL_D3D_STAGES: texture stages 1 and 2 (v10.206) */
#define VX_F_NEWOPS 2UL    /* ACCEL_D3D_NEWOPS: DOTPRODUCT3, BLENDTEXTUREALPHAPM, PREMODULATE, MODULATEINVALPHA_ADDCOLOR, MODULATEINVCOLOR_ADDALPHA (v10.206) */
#define VX_F_BUMP 4UL      /* ACCEL_D3D_BUMP: BUMPENVMAP, BUMPENVMAPLUMINANCE and the V8U8 / L6V5U5 / X8L8V8U8 formats (v10.208) */
#define VX_F_PROJECTED 8UL /* ACCEL_D3D_PROJECTED: projected textures (v10.208) */
#define VX_F_CKEYPS 16UL   /* ACCEL_D3D_CKEYPS: colour key with specular or a second stage, XP's C0 form (v10.208) */
#define VX_F_DITHER 32UL   /* ACCEL_D3D_DITHER: DITHERENABLE on 16-bit targets (v10.205) */
#define VX_F_POINTS 64UL   /* ACCEL_D3D_POINTS: point lists (v10.204) */
#define VX_F_BATCH 128UL   /* ACCEL_D3D_BATCH: consecutive primitives with the same state in one packet (v10.214) */
#define VX_F_CUBE 256UL    /* ACCEL_D3D_CUBE: cube maps (v10.214) */
#define VX_F_STRIPS 512UL  /* ACCEL_D3D_STRIPS: triangles that share an edge with the one before go out as triangle strips (v10.236) */
#define VX_F_BORDER 2048UL  /* ACCEL_D3D_BORDER: D3DTADDRESS_BORDER with BORDERCOLOR (v10.304; 0 clamps as before) */
#define VX_F_UNITMIP 4096UL
#define VX_F_TEXBLT 8192UL  /* ACCEL_D3D_TEXBLT: a video-memory texture in the desktop's format as a 2D source or destination of the HAL's Blt (v10.304; 0 = DirectDraw's own code, as before) */ /* ACCEL_D3D_UNITMIP: mip chains on texture units 1 and 2 (v10.304; 0 = level 0 as before) */
#define VX_F_DXTCHAIN 16384UL /* ACCEL_D3D_DXTCHAIN: a DXT mip chain's levels copied where the sampler reads them, after level 0 at their row size (v10.310) */
#define VX_F_SUBMITN 1024UL /* ACCEL_D3D_SUBMITN: a DrawPrimitives2 call's draw packets to ring 0 several at a time (v10.276) */
#define VX_F_CUBEMIP 32768UL /* ACCEL_D3D_CUBEMIP: a cube map's mip chain when its faces share the level layout (v10.317) */
#define VX_F_VOLUME 65536UL  /* ACCEL_D3D_VOLUME: 3D textures (packet layout 3, v10.317; only the OpenGL ICD makes them) */
#define VX_F_FASTPL 131072UL /* ACCEL_D3D_FASTPL: points and lines inside the viewport straight from the vertex buffer (v10.317) */
#define VX_F_UNIT3D 262144UL /* ACCEL_D3D_UNIT3D: a cube map or a 3D texture with other units, or on unit 1 or 2 (v10.321) */
uint32_t vx_d3d_flush(vx_d3d_context *c,const vx_d3d_services *sv,uint32_t *completed);
extern uint32_t vx_d3d_batched;
extern uint32_t vx_d3d_feat;
int vx_d3d_compose(const vx_d3d_context *,int,vx_d3d_xp *,uint32_t *);
int vx_d3d_program(const vx_d3d_context *,int,uint32_t *);
uint32_t vx_d3d_unsupported_why(const vx_d3d_context *);
extern uint32_t vx_d3d_dropped,vx_d3d_drop_why,vx_d3d_drop_state[8],vx_d3d_recovered;
/* v10.390: each DrawPrimitives2 command's time by opcode, when the HAL gives a clock (vx_d3d_clock; vx_d3d_ck_begin before the call,
 * vx_d3d_ck_end after it; [0] the time before the first command) */
extern uint32_t (*vx_d3d_clock)(void);extern uint32_t vx_d3d_op_t[64],vx_d3d_op_n[64];
void vx_d3d_ck_begin(void);void vx_d3d_ck_end(void);
/* v10.431: a DrawPrimitives2 call's time in exclusive phases, while the HAL's profile is on (vx_d3d_phclock set by the HAL for the call;
 * NULL: nothing is read). One phase at a time: entering one charges the time since the last switch to the phase being left. */
#define VX_PH_N 10
#define VX_PH_ADMIT 0 /* waiting for the callback lock (the HAL) */
#define VX_PH_CMD 1 /* the command stream: decoding commands, render and texture stage states */
#define VX_PH_STATE 2 /* the draw template: the state turned into a packet's fields and program */
#define VX_PH_VERT 3 /* vertices: validation, fetch, clipping */
#define VX_PH_ASM 4 /* primitives into packets (the fast paths, put_vertices, batching into the open packet) */
#define VX_PH_BUF 5 /* buffers: packet copies into the queue, binding the vertex buffer, staging before a VxD call */
#define VX_PH_VXD 6 /* VxD calls: the submits and the call's engine-state query, DeviceIoControl to its return */
#define VX_PH_WAIT 7 /* waits in the HAL (busy retries) */
#define VX_PH_HAL 8 /* the HAL around the core: context, surfaces, render-state resync, records */
#define VX_PH_PROF 9 /* the profiler's own VxD calls (the thread's run time) */
extern uint32_t vx_d3d_tk[16]; /* v10.473 */
extern uint32_t (*vx_d3d_phclock)(void);extern uint32_t vx_d3d_ph_call[VX_PH_N],vx_d3d_ph_cnt[VX_PH_N];
uint32_t vx_d3d_ph_set(uint32_t ph);void vx_d3d_ph_begin(uint32_t ph);void vx_d3d_ph_end(void);
#define VX_PH(p) (vx_d3d_phclock?vx_d3d_ph_set(p):0UL)
#define VX_PH_BACK(o) do{if(vx_d3d_phclock)(void)vx_d3d_ph_set(o);}while(0)
/* v10.431: the vertices that left primitives out (drop reason 2000): [0] x, y or z not finite, [1] rhw not finite, [2] rhw 0, [3] rhw < 0 */
extern uint32_t vx_d3d_bad_why[4],vx_d3d_rhw0_prims,vx_d3d_tnl,vx_d3d_tlp[18]; /* .534: ACCEL_D3D_TNL and its counts (vx_d3d_core.c) */ /* .531: primitives drawn with rhw 1 (all their vertices rhw 0) */
extern uint32_t vx_d3d_geo[24],vx_d3d_geo_on; /* v10.440: the geometry counters (vx_d3d_core.c) */
extern uint32_t vx_d3d_prepack,vx_d3d_pre_stat[2],vx_d3d_ft[32],vx_d3d_ftprof,vv_fast_n,vx_d3d_texruns,vx_d3d_tr[3]; /* vv_fast_n: vertices passed by the indexed path's one-test check (.527: FASTVV, VRECIPE, FASTMATCH removed) */ /* v10.441: ACCEL_D3D_PREPACK; batches sent prepacked, not */
extern uint32_t vx_d3d_tss_seen[4],vx_d3d_addr_ring[16][2],vx_d3d_addr_count,vx_d3d_dp2_calls;
/* v10.239: the OpenGL ICD's entries (vx_d3d_core.c) */
void vx_d3d_gl_begin(const uint8_t *vb,uint32_t stride,uint32_t fvf);
void vx_d3d_gl_count(uint32_t n); /* v10.330: the store's vertex count (indices are checked against it) */
/* v10.490: the ICD's SSE pass's 1/16-grid x y words for the store's vertices (vx_d3d_core.c); _get: those named now (0 when none) */
void vx_d3d_gl_presnap(const uint32_t *xy,uint32_t n,uint32_t mode,const uint8_t *base);
const uint32_t *vx_d3d_gl_presnap_get(uint32_t *n,uint32_t *mode,const uint8_t **base);
extern uint32_t vx_d3d_gl_flatspec; /* v10.492: with vx_d3d_gl_flatpv 3, 4: the specular word's colour bytes taken too */
uint32_t vx_d3d_gl_prim(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *idx,uint32_t nv,int join);
uint32_t vx_d3d_gl_strip(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *s,uint32_t n,int flip); /* v10.323: a GL strip, one vertex a triangle */
uint32_t vx_d3d_gl_tris(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *idx,uint32_t n);
uint32_t vx_d3d_gl_trilist(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *idx,uint32_t n,int join,uint32_t *bad); /* v10.500: a GL_TRIANGLES run, each as vx_d3d_gl_prim */ /* v10.481: n independent triangles, vertices idx[3i..3i+2] (NULL: 3i..3i+2) */
extern uint32_t vx_d3d_gl_hwflat,vx_d3d_gl_flatpv; /* v10.481: SHADEMODE flat on the engine (the OpenGL ICD's ACCEL_OPENGL_HWFLAT); v10.482: flatpv, a flat strip's colours laid out on each triangle's first (1) or last (2) strip vertex */
uint32_t vx_d3d_gl_flush(vx_d3d_context *c,const vx_d3d_services *sv);
void vx_d3d_gl_state(void);
void vx_d3d_gl_drop(void); /* v10.288 */
extern uint32_t vx_d3d_vhist[6],vx_d3d_cut[10],vx_d3d_cut_tag; /* v10.363: diagnostics */
uint32_t vx_d3d_gl_fan(vx_d3d_context *c,const vx_d3d_services *sv,const uint32_t *v,uint32_t n,int quads); /* .505: a polygon or fan's strips (quads: a b d c each) in one call */
void vx_d3d_gl_plan_begin(int same); /* .506: a draw begins; same: its store's window vertices are the last draw's (ACCEL_OPENGL_PLAN) */
extern uint32_t vx_d3d_gl_plan,vx_d3d_plan_stat[3],vx_d3d_ix_fan;
void vx_d3d_gl_runstart(void); /* .503: a triangle-list run begins (the store's kept words and checks start again) */
extern uint32_t vx_d3d_gl_gllist,vx_d3d_gl_glwords,vx_d3d_gw[4]; /* .503: ACCEL_OPENGL_GLLIST, ACCEL_OPENGL_GLWORDS; [0] vertices from the recipe (GLWORDS), [2] triangles through the run loop (GLLIST) */
void vx_d3d_gl_unjoin(void); /* v10.360: before each polygon or line from Mesa's clipper (its new vertices reuse slots) */
uint32_t vx_d3d_gl_clear(vx_d3d_context *c,const vx_d3d_services *sv,uint32_t flags,uint32_t color,double z,uint32_t stencil,const vx3dn_rect *r);
int vx_d3d_supported(const vx_d3d_context *c);
uint32_t vx_d3d_draw_primitive(vx_d3d_context *,const vx_d3d_input *,
 const vx_d3d_services *,uint32_t,const uint16_t *,uint32_t);
#endif
