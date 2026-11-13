/* VX855 native 3D packet ABI, milestone 1. Copyright 2026, MIT. */
#ifndef VX_NATIVE3D_H
#define VX_NATIVE3D_H
#include <stdint.h>
#include <stddef.h> /* .524: offsetof (VX3DN_STATE_RANGES) */
#define VX3DN_ABI 0x00010000UL
#define VX3DN_ARM 0x33445856UL
#define VX_IOCTL_3DN_INFO 0x85500020UL
#define VX_IOCTL_3DN_SUBMIT 0x85500021UL
/* v10.275: n draw packets one after another (in: n * sizeof(vx3dn_packet), 1 <= n <= VX3DN_BATCH_MAX); out: [0] the result, [1] the packets taken */
#define VX_IOCTL_3DN_SUBMITN 0x85500024UL
/* v10.315: occlusion count (VX3DN_query). in: DWORD op (0 the count set to 0, 1 read; v10.322: 2 put aside, 3 taken back, 4 given up), DWORD
 * owner (v10.322: a context's token, not 0; a 4-byte request is owner 1); out: [0] the result, [1] the count */
#define VX_IOCTL_3DN_QUERY 0x85500025UL
/* v10.432: lock (in[0] = 1) or release (0) a range of the shared arena in memory: in[1] base, in[2] bytes; out: pages locked, base,
 * handles holding it, ACCEL_LOCK_HAL. The lock stays while any handle that asked for it is open (DIOC_CLOSEHANDLE, also at a process's
 * end, releases that handle's hold). One range at a time, at most 4 MB, 0x80000000-0xBFFFFFFF only. */
#define VX_IOCTL_LOCKMEM 0x85500028UL
#define VX3DN_BATCH_MAX 32
/* .560: what a caller sends in one call: 24 packets (777,120 bytes; 32 would be 1,036,160 with .559's 32,380-byte packet, 872,320 with
 * .558's). .560 lowered it on a wrong reading of VXTPL's fault on .559 (that fault was QueryPhases writing past a one-word buffer, fixed
 * in .561). It stays at 24 because that is what the board has run with this packet size (GTA3 on .560); 32 has not been run with it. */
#define VX3DN_BATCH_SEND 24
/* v10.174: texture-wrap experiment (VXDX7 --texture). In: DWORD mode (0 none, 1-4 see vx_native3d_hw.c; 0xFFFFFFFF = query only).
 * Out: 8 DWORDs: mode, then the last values written to sampler 32, texture 20, texture 22, FE 0B, FE 0D, and the textured draw count. */
#define VX_IOCTL_3DN_TEXDBG 0x85500022UL
extern uint32_t vx3dn_dither_last[3],vx3dn_set_last[7],vx3dn_burst,vx3dn_ix16_stat[3]; /* .530: the TEXDBG probes removed */
void vx3dn_forget(void);
#define VX3DN_MAX_VERTICES 192 /* v10.235: was 48 (16 triangles a packet); 64 triangles a packet, sent in bursts of VX3DN_BURST_MAX */
/* .529: an indexed packet (pre.on 2) holds up to VX3DN_IX_VERTICES distinct vertices in pre.w (its indices stay at most VX3DN_MAX_INDICES):
 * GTA3 on .528 sent 160,000 batches because 192 vertices were full, more than for any other reason. Every other packet keeps 192. */
#define VX3DN_IX_VERTICES 512 /* .559: was 384 (GTA3 on .557: 59,286 of 813,124 packets were ended because 384 vertices were full; the recording: 267.0 -> 251.2 packets a frame). The T&L's slot table holds 512 (TN_SLOT_STEP); the VxD's vertex area gives such a packet three 7,680-byte slots */
#define VX3DN_BURST_MAX 48 /* the burst size the board drew with since v10.216 */
#define VX3DN_IX_LONG 480UL /* .554: an index burst's data words (960 indices): what an inline burst of 48 ten-word vertices sends between two waits */
#define VX3DN_XRGB8888 1UL
#define VX3DN_RGB565 2UL
#define VX3DN_D32 3UL
/* v10.177: 16-bit Z and 24-bit Z + 8-bit stencil (Windows XP's set, CLXP3D T50/T52); stencil itself is not enabled yet */
#define VX3DN_D16 12UL
#define VX3DN_D24S8 13UL
#define VX3DN_IS_DEPTH(f) ((f)==VX3DN_D32||(f)==VX3DN_D16||(f)==VX3DN_D24S8)
/* v10.460: an A8R8G8B8 colour target (engine code 99, HCM p95; X8R8G8B8 is 98): the engine writes alpha. Render-target textures of that
 * format (board, VXRTT v1.1: as an X8R8G8B8 target a draw left alpha 0) */
#define VX3DN_ARGB8888 30UL
#define VX3DN_IS_COLOR(f) ((f)==VX3DN_XRGB8888||(f)==VX3DN_RGB565||(f)==VX3DN_ARGB8888)
/* v10.245: the engine keeps a D24S8 surface's stencil as a separate 8-bit plane, not in the Z word's low byte. XP (CLXP3D T52, 256x192:
 * Z at 300500, pitch 1024) puts it after the Z buffer, the next 256-byte boundary plus 256 (330600), rows of the width rounded to 16
 * (256), and programs 01/28 = plane >> 8 (003306), 01/29 = 018000 | rows >> 4 | 400000 | 400 write | 1000 read (418410), 01/2A = (plane
 * end + 256) >> 8 (0033C7). The DirectDraw HAL allocates the Z surface with room for it (VX3DN_SPLANE_BLOCK) and moves the stencil
 * between the plane and the Z words' low bytes at Lock and Unlock, so programs still see D24S8. */
/* v10.246: the plane is tiled (VXDX7 v2.69 --splane, the engine's own writes and reads): 8x8-pixel tiles of 64 bytes, a tile's rows
 * 8 bytes each, tiles left to right (64 bytes apart), tile rows pitch x 8 bytes apart. Byte of pixel x,y: VX3DN_SPLANE_AT. Its size is
 * pitch x the height rounded up to 8. */
#define VX3DN_SPLANE_PITCH(w) ((((uint32_t)(w))+15UL)&~15UL)
#define VX3DN_SPLANE_ROWS(h) ((((uint32_t)(h))+7UL)&~7UL)
#define VX3DN_SPLANE_SIZE(w,h) (VX3DN_SPLANE_PITCH(w)*VX3DN_SPLANE_ROWS(h))
#define VX3DN_SPLANE_AT(pitch,x,y) ((((uint32_t)(y))>>3)*((uint32_t)(pitch))*8UL+(((uint32_t)(x))>>3)*64UL+((((uint32_t)(y))&7UL)<<3)+(((uint32_t)(x))&7UL))
#define VX3DN_SPLANE_OFF(s) (((((s)->offset)+((s)->pitch)*((s)->height)+255UL)&~255UL)+256UL)
#define VX3DN_SPLANE_END(s) (VX3DN_SPLANE_OFF(s)+VX3DN_SPLANE_SIZE((s)->width,(s)->height))
/* v10.294 (a review with the Chrome9 HCM manual, OGPM Part 2 pp. 79-80, 87-88; XP's CLXP3D T52/T58 decoded): 01/2A is not the plane's end
 * but the base of the engine's Coarse Z storage, 01/29 its pitch (bits 8:0, 32-byte units; bit 15 resets the Coarse Z cache tags) and 01/01
 * bits 13 / 9 (0x2200) enable Coarse Z tests / writes. XP's Z block holds the storage after the plane: one 16-byte record per 8x8 tile, a
 * row of them the plane's pitch x 2 bytes (T52, 256x192: 12 KB at 33C700), filled (FFFFFFFF for depth 1.0) before each Z clear. Ours ended
 * at the plane: the engine kept its Coarse Z records in whatever lay after the block (VXGL's first texture, at the same 256 bytes past
 * the plane), so a stencil draw tested against another surface's bytes (VXSTEN case 2 after VXTEX: 8x8 tiles wrong) and wrote over it.
 * The storage is in the block now; stencil draws point 01/2A at it as XP does. Coarse Z itself is not used (v10.299: right but slower). */
/* v10.295: Z16 keeps the storage too (XP T50: Z at 300500, 256x192, 2A = 003186: 256 past the Z words); ZEND = the depth surface's end
 * (D24S8: its stencil plane's) */
#define VX3DN_ZEND(s) ((s)->format==VX3DN_D24S8?VX3DN_SPLANE_END(s):(s)->offset+(s)->pitch*(s)->height)
#define VX3DN_CZ_OFF(s) ((VX3DN_ZEND(s)+256UL)&~255UL)
#define VX3DN_CZ_FORMAT(f) ((f)==VX3DN_D16||(f)==VX3DN_D24S8)
#define VX3DN_CZ_SIZE(w,h) (2UL*VX3DN_SPLANE_PITCH(w)*(VX3DN_SPLANE_ROWS(h)>>3))
#define VX3DN_CZ_END(s) (VX3DN_CZ_OFF(s)+VX3DN_CZ_SIZE((s)->width,(s)->height))
#define VX3DN_SPLANE_BLOCK(pitch,w,h) ((((((uint32_t)(pitch))*((uint32_t)(h))+255UL)&~255UL)+256UL+VX3DN_SPLANE_SIZE(w,h)+256UL+VX3DN_CZ_SIZE(w,h)+255UL)&~255UL) /* v10.288: a multiple of 256 (the plane's size is only one of 128: the surfaces DirectDraw put after it were 128 off the sampler's alignment); v10.294: the Coarse Z storage too */
#define VX3DN_Z16_BLOCK(pitch,w,h) ((((((uint32_t)(pitch))*((uint32_t)(h))+256UL)&~255UL)+VX3DN_CZ_SIZE(w,h)+255UL)&~255UL) /* v10.295: Z16 + its Coarse Z storage */
#define VX3DN_DRAW 1UL
#define VX3DN_DRAW_CLEAR 0x80000000UL /* clear_flags on a DRAW: internal clear, excluded from occlusion counts */
#define VX3DN_CLEAR 2UL
#define VX3DN_CLEAR_TARGET 1UL
#define VX3DN_CLEAR_Z 2UL
/* v10.179: stencil clear (D24S8 only); the stencil value rides in clear_flags bits 15:8 (the packet layout is unchanged) */
#define VX3DN_CLEAR_STENCIL 4UL
#define VX3DN_CLEAR_SVAL(f) (((f)>>8)&255UL)
#define VX3DN_CLEAR_WHAT(f) ((f)&7UL)
#define VX3DN_OK 0UL
#define VX3DN_BAD_PACKET 1UL
#define VX3DN_UNSUPPORTED 2UL
#define VX3DN_BUSY 3UL
#define VX3DN_LOST 4UL
#define VX3DN_STALE 5UL
/* v10.210: the draw hung the 3D engine, the VxD reset the engine and it is idle again; this draw is dropped, the device stays usable */
#define VX3DN_RECOVERED 6UL
typedef struct vx3dn_surface {
 uint32_t offset,pitch,width,height,format;
} vx3dn_surface;
typedef struct vx3dn_rect {int32_t left,top,right,bottom;} vx3dn_rect;
/* x/y/z/rhw are IEEE754 binary32 BIT PATTERNS, not integer coordinates.
 * The kernel emitter contains no floating-point instructions. */
typedef struct vx3dn_vertex {uint32_t x,y,z,rhw,color;} vx3dn_vertex;
/* v10.169: one texture (stage 0). The surface format is VX3DN_TEX_*; dimensions are powers of two, pitch width*4, base 256-byte aligned.
 * filter 0 point, 1 bilinear, 2 linear magnification only, 3 linear minification only;
 * address 1 clamp, 2 wrap, 3 mirror (the hardware's own codes); mode 1 = the texel is the output (colour and
 * alpha, the proven "sample t0 -> output" program). uv[] are IEEE binary32 bit patterns, one pair per vertex. A packet sent with the
 * original size (VX3DN_PACKET_V1_BYTES, no texture block) is still accepted and draws untextured. */
#define VX3DN_TEX_ARGB8888 4UL
#define VX3DN_TEX_XRGB8888 5UL
#define VX3DN_FILTER_POINT 0UL
#define VX3DN_FILTER_LINEAR 1UL
#define VX3DN_FILTER_MAG_LINEAR 2UL
#define VX3DN_FILTER_MIN_LINEAR 3UL
typedef struct vx3dn_texture {
 uint32_t enable;vx3dn_surface surface;uint32_t filter,address_u,address_v,mode;
 uint32_t uv[VX3DN_MAX_VERTICES][2];
} vx3dn_texture;
/* v10.172: further texture formats (Windows XP driver codes in brackets): R5G6B5 [89], A1R5G5B5 [8A], A4R4G4B4 [8B], X1R5G5B5 [88]; pitch width*2. */
#define VX3DN_TEX_RGB565 6UL
#define VX3DN_TEX_ARGB1555 7UL
#define VX3DN_TEX_ARGB4444 8UL
#define VX3DN_TEX_XRGB1555 9UL
/* v10.199: L8 [13], DXT1 [29], DXT3 [2A], DXT5 [2B] (CLXP3D T31, T33-T35). An L8 pitch is width; a DXT surface's pitch is one row of 4x4
 * blocks (max(1, width / 4) x 8 or 16 bytes) and it has max(1, height / 4) such rows. */
#define VX3DN_TEX_L8 20UL /* (20-23: clear of the depth codes 12 and 13) */
#define VX3DN_TEX_DXT1 21UL
#define VX3DN_TEX_DXT3 22UL
#define VX3DN_TEX_DXT5 23UL
/* v10.208: bump (du/dv) formats V8U8 [38], L6V5U5 [39], X8L8V8U8 [3A] (CLXP3D T146-T148) */
#define VX3DN_TEX_V8U8 24UL
#define VX3DN_TEX_L6V5U5 25UL
#define VX3DN_TEX_X8L8V8U8 26UL
#define VX3DN_IS_TEX(f) (((f)>=VX3DN_TEX_ARGB8888&&(f)<=VX3DN_TEX_XRGB1555)||((f)>=VX3DN_TEX_L8&&(f)<=VX3DN_TEX_X8L8V8U8))
#define VX3DN_IS_BUMP(f) ((f)>=VX3DN_TEX_V8U8&&(f)<=VX3DN_TEX_X8L8V8U8)
#define VX3DN_LAYOUT_CUBE 1UL    /* stages.layout (v10.214) */
#define VX3DN_LAYOUT_PROJ 2UL    /* (v10.208) */
#define VX3DN_LAYOUT_VOLUME 3UL  /* (v10.317) */
/* v10.172: a pixel program taken from the Windows XP driver's command stream (CLXP3D captures). count = instructions (1..3, four words
 * each); 0 keeps the milestone-1 path. konst[] = constant c2 (IEEE floats r g b a, written when kflags bit 0 is set). The draw then uses
 * XP's 10-dword vertex layout: X Y Z W(=rhw) diffuse specular u0 v0 u1 v1, with spec[] the per-vertex specular colours.
 * v10.197: kflags bit 1 = XP's colour-key form (CLXP3D T45): count 1..6, header 92 = 020000 | count, 93 = 3, 95 = 010, TAU 201001B0 314001B1,
 * constant bank 0 (0.5 1 2 4 1/512) and the key (IEEE floats r g b a in key[]) at 220300AC. The program's last instruction is the one the
 * draw would run without the key (with the key off in the registry, ring 0 runs that one alone).
 * v10.198: count 6..7 with the key (the five-instruction key test, then the program), the program being the instructions after the 20th word. */
typedef struct vx3dn_shader {
 uint32_t count,words[32],konst[4],kflags; /* v10.198: 32 words (the key test + a two-instruction program) */
 uint32_t spec[VX3DN_MAX_VERTICES];
 uint32_t key[4];
} vx3dn_shader;
/* v10.177: blend factors and alpha test. blend_src/blend_dst = D3DBLEND values (0 = the milestone-1 SRCALPHA/INVSRCALPHA);
 * alpha_func = D3DCMPFUNC (0 = alpha test off), alpha_ref 0..255.
 * v10.304 (HCM Part 2 pp. 88-93, 01/34-3F): bits 4:0 of each = the factor, D3DBLEND 1..11 or 14 BLENDFACTOR, 15 INVBLENDFACTOR (the
 * factor's constant), 16 the constant's alpha, 17 its inverse (the core puts the alpha in R, G and B); blend_src bits 7:5 = the equation (VX3DN_BLENDOP_*); bits 31:8 of each =
 * the constant colour (R << 16 | G << 8 | B) for that side's factor (src: 01/38-3A HABLRFCa, dst: 01/3B-3D HABLRFCb). */
#define VX3DN_BLENDOP_ADD 0
#define VX3DN_BLENDOP_SUB 1
#define VX3DN_BLENDOP_REVSUB 2
#define VX3DN_BLENDOP_MIN 3
#define VX3DN_BLENDOP_MAX 4
typedef struct vx3dn_raster {uint32_t blend_src,blend_dst,alpha_func,alpha_ref;} vx3dn_raster;
/* v10.195: fog, as XP programs it (cookbook 16.7, CLXP3D T16-T19). enable = 1 sets 01/00 bit 8 (render target 0); r[0..8] are the type-1
 * registers 70..78 ready to write (74 reserved, written 0): 70 = 30 | mode (0 vertex: factor from specular alpha, 2 LINEAR, 4 EXP, 5 EXP2),
 * 71 = G << 12 | B and 72 = R (c * 1024 / 255), 73/77 start/end = bits(float) >> 16, 75/76 = 1/(end - start) fraction (q >> 19) & F and
 * exponent (q >> 23) & FF, 78 = ((bits(density) >> 18) & 1FFF) << 8. The core computes them; ring 0 does no floating point. */
typedef struct vx3dn_fog {uint32_t enable,r[9];} vx3dn_fog;
/* v10.199: mipmaps and anisotropy, as XP programs them (CLXP3D T21-T23, T47). levels = 1 (no mipmaps) .. 12; level[k] = level k+1's
 * offset from level 0 in 256-byte units (sampler block 10030000; the first 8 are sent); filter = 0 none, 1 point, 2 linear (sampler 31
 * bits 1:0); bias = sampler 30 bits 23:16 (2 per level: T23's +1.0 = 02, signed); aniso = 1..16 (sampler 31 bits 20:16, with the
 * shrinking filter at bits 11/5 when above 1). */
typedef struct vx3dn_mip {uint32_t levels,filter,bias,aniso,level[11];} vx3dn_mip;
/* v10.201: prim = 0 triangles (vertex_count a multiple of 3, mode 00CE02), 1 lines (pairs, 00CE01, CLXP3D T62), 2 points (001E00 with
 * 04/41 = 0, 42 = 3F, 44 = 0, 45 = 3F, T64); lines and points set 01/85 = 800000. dither bit 0 clears 01/53 bit 16 (and 5B/63/6B: T49);
 * v10.481: dither bit 1 draws triangles flat, mode 009E02 (T48: each triangle the colours of its first vertex; triangle lists only).
 * stencil = 1 (D24S8 only) sets 01/01 | 10 with 01/23 = 01/25 = w1 = ref << 16 | mask << 8 | write mask and 01/24 = 01/26 = w2 =
 * func << 16 | fail << 6 | zfail << 3 | pass (func 0..7 = D3DCMP - 1, ops 0..7 = D3DSTENCILOP - 1; cookbook 6.5, XP T52). */
typedef struct vx3dn_ext {uint32_t prim,dither,stencil,stencil_w1,stencil_w2;} vx3dn_ext;
/* v10.206: two and three texture units (CLXP3D T116-T125, T149). units = 1..3 (0 = tex alone); unit 0 is tex (with mip), units 1 and 2
 * are unit[0..1] (no mip chain). set[u] = the vertex coordinate set unit u reads (0 = tex.uv, 1 = uv1; FE/01 byte u = 21 | set << 2).
 * c22 = 1 writes constant c22 = (0.318, -0.5, 0.4545, 0) at 22030058, as XP's context does (DOT3's -0.5, T142). The program is
 * shader.words (1..8 instructions): header 91 = (1 << units) - 1, 92 = units << 16 | count, 93 = 1, 2, 4; TAU word u = 301001B0 + u * 01011001. */
typedef struct vx3dn_unit {vx3dn_surface surface;uint32_t filter,address_u,address_v;} vx3dn_unit;
/* v10.208: explicit program schedule (ps = 1): 01/90, 91, 93, 95 and the TAU words as given (ntau 1..4), 92 = ntau << 16 | count; used by
 * bump mapping (T146/T147) and the colour key with a program (T73/T125). kc[] = up to four constant vectors: header 22030000 + 4N in
 * kc_reg[k] (N 0..63), four IEEE floats each. layout 1 (cube) / 2 (projected: TAU 307001A0) = XP's 9-dword vertex X Y Z W diffuse specular s t q (TextureA S, T, R; T150/T151:
 * 04/00 = A80009, 04/03 = 0A0908, 04/25 = F6E000, FE02/01 unit 0 = 22) with q[] the third coordinate. */
typedef struct vx3dn_stages {uint32_t units,set[3];vx3dn_unit unit[2];uint32_t uv1[VX3DN_MAX_VERTICES][2];uint32_t c22;
 uint32_t ps,h90,h91,h93,h95,ntau,tau[4];uint32_t nkc,kc_reg[4],kc[4][4];uint32_t layout,q[VX3DN_MAX_VERTICES];
 uint32_t face[6]; /* v10.214: layout 1 = cube map, face[] = the six face offsets +X -X +Y -Y +Z -Z (T150: 02/00..05) */
 /* v10.317: layout 3 = a 3D texture on the only unit (HCM p117 mode 0010, 02/21 its depth, 22 bits 11:8 log2 depth; sampler 32 bits 8:6 the R
  * address, 21:19 / 18:16 the R filters): face[0] = the depth (slices, a power of two up to 2048), face[1] = the bytes from one slice to the
  * next, face[2] = the R address code (as address_u); the vertex is layout 1's s t r (q[] = r); no mip chain (HCM: none for volumes) */
 /* v10.354: umip also carries anisotropy (1..16) in bits 20:16; 0 means the legacy value 1. Packet size is unchanged.
  * v10.304: units 1 and 2's mip chains (umip = levels | mip filter << 8, ubias as mip.bias, ulevel = level 1..8 offsets from level 0
  * in 256 bytes, sampler block 10030000 + 8u, HCM p138) and the border colour of units 0..2 (ARGB, address code 0, sampler 35/36) */
 uint32_t umip[2],ubias[2],ulevel[2][8],border[3];
 /* v10.321: a cube map or a 3D texture on units 1 and 2, or on unit 0 with other units (HCM p132 FE/08: each vertex coordinate set's
  * dimension). ulayout[u - 1] = unit u's layout (0 flat, 1 cube, 3 volume) and uface[u - 1] its faces (as face[]: the six offsets, or the
  * depth, slice step and R address). s3 bit 0: TextureA (set 0) carries a third coordinate (q[]), bit 1: TextureB (set 1) does (r1[]);
  * the vertex is then X Y Z W diffuse specular S_A T_A [R_A] S_B T_B [R_B] (04/xx attribute codes 08 09 0A, 0C 0D 0E) */
 uint32_t ulayout[2],uface[2][6],s3,r1[VX3DN_MAX_VERTICES];} vx3dn_stages;
/* v10.236: a triangle packet's vertices as runs (n = 0: one triangle list, as before). b[k] = topology << 16 | count: topology 0 a triangle
 * list (count a multiple of 3), 1 a triangle strip (count >= 3; triangle i = vertices i, i+1, i+2; the engine does not cull, so the winding
 * of alternate triangles does not matter). The counts add up to vertex_count. The engine draws a strip with 04/20 = 386300 (XP T59) and a
 * list with 04/20 = 0 (T61); a strip longer than a burst goes in bursts of 48 that overlap by two vertices. */
#define VX3DN_MAX_BURSTS 64 /* v10.364: was 32 (board, v10.363: 76% of Quake 2's packets ended at 32 runs, 97-144 vertices each: a polygon a run); 64 = 192 / 3, the most runs 192 vertices can hold */
#define VX3DN_TOPO_LIST 0UL
#define VX3DN_TOPO_STRIP 1UL
/* v10.470 (ACCEL_D3D_INDEXED): an indexed packet (pre.on = 2): pre.w holds its vertex_count distinct vertices (the 10-word form), the
 * unused vertices[] array its indices (16-bit, packet-local, VX3DN_IDX), bursts one run of VX3DN_TOPO_INDEXED whose count is the number of
 * indices (whole triangles). The engine reads the vertices by index from video memory (HCM Part 2 p148-162; board, VXFETCH v10.469: the
 * 10-word records at a 40-byte stride, 48-index bursts, the same picture as inline). */
#define VX3DN_TOPO_INDEXED 2UL
/* .529: .515's engine cull of indexed packets (VX3DN_IX_HWCULL, VX3DN_IX_CCW: 01/01 bit 7, 04/23 bit 8) removed: no saving in two GTA3
 * A/B runs (it draws its indexed lists with cull none), and every caller culls on the CPU. Bits 24-25 of the run's word are refused. */
/* .523 (board, VXV8 on .522: the engine draws 8-word vertices from the vertex area, 32-byte stride, 04/00 = A80008, codes 0 1 2 3 5 6 8 9,
 * the picture the same as the 10-word form's): VX3DN_IX_V8, a bit of the indexed run's word: pre.w holds 8 words a vertex (X Y Z W
 * diffuse specular u v; vertex k at w[k * 8]), only for VX3DN_V8_FORM (an XP program, one texture unit, the plain layout); the VxD
 * refuses it on any other packet. ACCEL_D3D_V8 (default 1). */
#define VX3DN_IX_V8 0x04000000UL
#define VX3DN_IX_BITS VX3DN_IX_V8
#define VX3DN_V8_FORM(p) ((p)->cb==sizeof(*(p))&&(p)->op==VX3DN_DRAW&&(p)->shader.count&&(p)->tex.enable&&(p)->stages.units<=1&&!(p)->stages.layout&&!(p)->stages.s3)
#define VX3DN_PRE_STRIDE(p) ((p)->pre.on==2&&(p)->bursts.n&&((p)->bursts.b[0]&VX3DN_IX_V8)?8UL:10UL)
extern uint32_t vx3dn_r01;
#define VX3DN_MAX_INDICES 1920UL /* sizeof(vertices) / 2 */
#define VX3DN_IDX(p) ((uint16_t*)(void*)(p)->vertices)
#define VX3DN_CIDX(p) ((const uint16_t*)(const void*)(p)->vertices)
#define VX3DN_STRIP_0420 0x386300UL
typedef struct vx3dn_bursts {uint32_t n,b[VX3DN_MAX_BURSTS];} vx3dn_bursts;
/* v10.308: polygon stipple (board, v10.307 probe): on = 01/01 bit 1; rows[k] = the target's rows k, k + 32, ... from its top, bit b = pixel
 * columns b, b + 32, ... from its left (block 14030000) */
typedef struct vx3dn_stipple {uint32_t on,rows[32];} vx3dn_stipple;
/* v10.441: prepacked vertices (ACCEL_D3D_PREPACK). on = 1: the packet's vertices are w[], XP's 10-word layout as the VxD's emitter sends
 * it (X Y Z W(=rhw) diffuse specular u0 v0 u1 v1, u1 v1 = u0 v0 with one unit), vertex k at w[k * 10]; vertices[] and the per-vertex arrays
 * of tex, shader and stages are not used. Only a triangle draw with an XP program, no projected / cube / volume layout, no third
 * coordinates (s3) and at most two units (VX3DN_PRE_FORM); the VxD checks the form and the words as it checks the arrays, and unpacks
 * them into the arrays when its own emitter would not send this layout (a debug switch). */
typedef struct vx3dn_prepack {uint32_t on,w[VX3DN_IX_VERTICES*10];} vx3dn_prepack; /* .529: an indexed packet's 384 (else 192) */
/* .528 (ACCEL_D3D_TEXRUNS, default 1): texture runs. An indexed packet (pre.on = 2) whose draws differ only in their unit-0 texture carries
 * them as runs instead of one packet each: the packet's own tex and mip are run 0's (indices 0 .. r[0].at - 1); run k + 1 starts at index
 * r[k].at and draws with r[k]'s surface, filter, addressing and mip chain (enable and mode stay the packet's). r[k].at a multiple of 3,
 * above the one before, below the packet's index count; at most VX3DN_TEXRUNS_MAX; only for a textured packet on one unit with the plain
 * layout (VX3DN_TEXRUN_FORM). The VxD checks every run's texture as the packet's; between runs the emitter writes what a packet whose state
 * differs only in its texture gets (the vertex-fetch registers set back, the texture cache strobe, the unit's words). n = 0: none. */
#define VX3DN_TEXRUNS_MAX 15
typedef struct vx3dn_texrun {uint32_t at;vx3dn_surface surface;uint32_t filter,address_u,address_v;vx3dn_mip mip;} vx3dn_texrun;
typedef struct vx3dn_texruns {uint32_t n;vx3dn_texrun r[VX3DN_TEXRUNS_MAX];} vx3dn_texruns;
#define VX3DN_TEXRUN_FORM(p) ((p)->op==VX3DN_DRAW&&(p)->pre.on==2&&(p)->tex.enable&&(p)->stages.units<=1&&!(p)->stages.layout&&!(p)->stages.s3)
#define VX3DN_PRE_FORM(p) ((p)->op==VX3DN_DRAW&&(p)->shader.count&&!(p)->stages.layout&&!(p)->stages.s3&&(!(p)->tex.enable||(p)->stages.units<=2)&&!(p)->ext.prim)
typedef struct vx3dn_packet {
 uint32_t cb,abi,arm,op,generation;
 vx3dn_surface target,depth;
 vx3dn_rect clip;
 uint32_t z_enable,z_write,z_func,blend,write_mask;
 uint32_t clear_flags,clear_color,clear_depth;
 uint32_t vertex_count;
 vx3dn_vertex vertices[VX3DN_MAX_VERTICES];
 vx3dn_texture tex;
 vx3dn_shader shader;
 vx3dn_raster raster;
 vx3dn_fog fog; /* v10.195 */
 vx3dn_mip mip; /* v10.199 */
 vx3dn_ext ext; /* v10.201 */
 vx3dn_stages stages; /* v10.206 */
 vx3dn_bursts bursts; /* v10.236 */
 vx3dn_stipple stipple; /* v10.308 */
 vx3dn_prepack pre; /* v10.441 */
 vx3dn_texruns runs; /* .528 */
} vx3dn_packet;
typedef char vx3dn_batch_send_fits[(VX3DN_BATCH_SEND*sizeof(vx3dn_packet)<=872320UL&&VX3DN_BATCH_SEND<=VX3DN_BATCH_MAX)?1:-1]; /* .560 */
/* .524: state deltas (ACCEL_D3D_DELTA, default 1). The packet's state is VX3DN_STATE_RANGES (offsets from the packet's start, in bytes):
 * range 0 the header (op, generation, target, depth, clip, Z, blend, write mask, the clear words), 1 the texture, 2 the program words,
 * 3 the key, raster, fog, mip chain, primitive and stencil words and the units, 4 the schedule and the constants, 5 the faces, the units'
 * chains, borders and 3D layouts, 6 the stipple (972 bytes in all; the VxD has copied and compared them since v10.236). In a batch
 * (VX_IOCTL_3DN_SUBMITN) the HAL may mark a draw packet's ranges 1-6 that are byte for byte the previous packet's of the same call: the
 * draw's clear_depth (a clear's word, unused by a draw) = VX3DN_SAME_TAG | bit k - 1 for range k. The VxD reads the word from its copy of
 * the header, writes 0 there (a draw's value), and keeps its copy of each marked range from the previous packet (the one it validated)
 * instead of reading it; with every range marked and the header the same as the last validated one, the state's checks are not done
 * again (the vertices' and the indices' are). The first packet of a call, a packet sent alone and a group built again after a reset copy
 * everything. Untagged packets (VXGL's queue, older HALs) as before. */
#define VX3DN_SAME_TAG 0x5D000000UL
#define VX3DN_SAME_MASK 0x3FUL
#define VX3DN_IS_SAME(w) (((w)&0xFFFFFFC0UL)==VX3DN_SAME_TAG)
#define VX3DN_STATE_RANGES {{0,offsetof(vx3dn_packet,vertex_count)},\
 {offsetof(vx3dn_packet,tex),offsetof(vx3dn_packet,tex)+offsetof(vx3dn_texture,uv)},\
 {offsetof(vx3dn_packet,shader),offsetof(vx3dn_packet,shader)+offsetof(vx3dn_shader,spec)},\
 {offsetof(vx3dn_packet,shader)+offsetof(vx3dn_shader,key),offsetof(vx3dn_packet,stages)+offsetof(vx3dn_stages,uv1)},\
 {offsetof(vx3dn_packet,stages)+offsetof(vx3dn_stages,c22),offsetof(vx3dn_packet,stages)+offsetof(vx3dn_stages,q)},\
 {offsetof(vx3dn_packet,stages)+offsetof(vx3dn_stages,face),offsetof(vx3dn_packet,stages)+offsetof(vx3dn_stages,r1)},\
 {offsetof(vx3dn_packet,stipple),offsetof(vx3dn_packet,stipple)+sizeof(vx3dn_stipple)}}
/* v10.237: the V1 packet is the historical one (v10.149): the header and 48 vertices, 1072 bytes. It was worked out from the current packet,
 * so it grew to 3952 bytes with the 192-vertex array and the original size was refused. */
#define VX3DN_V1_VERTICES 48
#define VX3DN_PACKET_V1_BYTES (sizeof(vx3dn_packet)-sizeof(vx3dn_texture)-sizeof(vx3dn_shader)-sizeof(vx3dn_raster)-sizeof(vx3dn_fog)-sizeof(vx3dn_mip)-sizeof(vx3dn_ext)-sizeof(vx3dn_stages)-sizeof(vx3dn_bursts)-sizeof(vx3dn_stipple)-sizeof(vx3dn_prepack)-sizeof(vx3dn_texruns)-(VX3DN_MAX_VERTICES-VX3DN_V1_VERTICES)*sizeof(vx3dn_vertex))
typedef struct vx3dn_info {
 uint32_t cb,abi,ready,generation,fb_bytes,primary_bytes;
 uint32_t width,height,pitch,bpp,submitted,triangles,clears,fault;
 uint32_t last_status,last_result;
} vx3dn_info;
typedef struct vx3dn_io {
 void *context;
 uint32_t (*read)(void *,uint32_t);
 void (*write)(void *,uint32_t,uint32_t);
 uint32_t (*read_fb)(void *,uint32_t);
 void (*write_fb)(void *,uint32_t,uint32_t);
 uint32_t (*milliseconds)(void *);
} vx3dn_io;
/* A descriptor is authorized against the real mapped aperture AND primary.
 * Offscreen allocations must additionally be owned by the HAL/runtime. */
uint32_t vx3dn_validate(const vx3dn_packet *,const vx3dn_info *);
uint32_t vx3dn_validate_geo(const vx3dn_packet *,const vx3dn_info *); /* .524: the state already validated (vx_native3d_hw.c) */
uint32_t vx3dn_execute(const vx3dn_packet *,const vx3dn_io *,uint32_t *);
/* v10.179: the depth surface's clear word and the bits it replaces (Z and/or stencil; 0 mask = nothing to do) */
void vx3dn_depth_word(const vx3dn_packet *,uint32_t *value,uint32_t *mask);
void vx3dn_plane_clear(const vx3dn_packet *p,const vx3dn_io *io); /* v10.245 */
uint32_t VX3DN_submit(const vx3dn_packet *);
uint32_t VX3DN_submit_batch(const vx3dn_packet *,uint32_t,uint32_t *); /* v10.275 */
uint32_t VX3DN_query(uint32_t op,uint32_t owner,uint32_t *count); /* v10.315; v10.322: the owner (a context's token) */
void VX3DN_info(vx3dn_info *);
int VX3DN_busy(void);
#endif
