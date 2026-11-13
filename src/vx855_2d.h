/* Fixed-width, pointer-free ABI shared by Win16, VxD and Win32. */
#ifndef VX855_2D_H
#define VX855_2D_H
#include <stdint.h>
#define VX2D_ABI 0x00010000UL
#define VX_IOCTL_2D_SUBMIT 0x85500015UL
#define VX_IOCTL_2D_STATS 0x85500016UL
#define VX_IOCTL_2D_ACCESS 0x85500017UL
#define VX2D_QUERY 0xFFFFFFFEUL
#define VX2D_REGISTRY 0xFFFFFFFFUL
#define VX2D_SKIP 0
#define VX2D_DONE 1
#define VX2D_FAILED 2 /* command fired: NEVER replay, including XOR/invert */
#define VX2D_LOST 3 /* engine cannot be stopped: no CPU/cursor access */
#define VX2D_BUSY 4 /* v10.142: VX_IOCTL_2D_ACCESS action 3 (status): an asynchronous blit is still running */
#define VX2D_FILL 1
#define VX2D_COPY 2
#define VX2D_MONO 3
#define VX2D_COLOR 4
#define VX2D_LINE 5 /* v10.90: dx,dy = first point, sx,sy = end point (not drawn), width = pixels along the major axis, height = 1, rop 0xF0, fg = colour; flags CLIP only */
#define VX2D_CLIP 1
#define VX2D_SRC_KEY 2
#define VX2D_DST_KEY 4
#define VX2D_TRANSPARENT 8
#define VX2D_DDRAW 16
#define VX2D_FIXPAT 32 /* v10.84 copies: the pattern operand of the ROP is the solid colour fg (command bit 13), for ROPs that use P and S */
#define VX2D_DASH 64 /* v10.92, with VX2D_LINE: a styled line. mono[0] = the 32 pixel pattern in drawing order (bit 0 = the first pixel, 1 = pen colour fg), bg = the colour of the 0 bits, VX2D_TRANSPARENT = the 0 bits leave the destination alone */
#define VX2D_RT 256 /* v10.453, with VX2D_DDRAW and VX2D_COPY: a copy from a 3D render target into a texture (Direct3D 8's CopyRects): with XP's ring running it may be written into the ring after the draws (ACCEL_D3D_RINGBLT) */
#define VX2D_ASYNC 128 /* v10.95, with VX2D_DDRAW: the command is started and VX2D_submit returns at once; the VxD finishes it (registers, cursor, sequencer lock) before it does anything else. Honoured only inside a DirectDraw exclusive mode with ACCEL_ASYNC on. */
#define OP_VX_SUBMIT 0x1132
#define OP_VX_BIND 0x1133
#define OP_VX_STRETCH 0x1134 /* v10.99 */
#define OP_VX_XFER 0x1135 /* v10.116 */
#define OP_VX_HZ 0x1137 /* .577: -> ECX = the refresh rate of the mode shown in hertz, 0 unknown (vx855_flip.c VX_flip_hz) */
#define OP_VX_COMPLETE 0x1136 /* v10.425: finish an asynchronous 2D command now (the display driver, before DIBENG touches the screen) */
/* All coordinates nonnegative; clip bounds are exclusive. */
typedef struct { uint32_t offset,pitch,width,height; } vx2d_surface;
typedef struct {
 uint32_t cb,abi,op,flags,bpp;
 vx2d_surface dst,src;
 uint32_t dx,dy,sx,sy,width,height;
 uint32_t left,top,right,bottom;
 uint32_t fg,bg,rop,key,mono[2],pattern[64];
} vx2d_request;
/* v10.99/v10.100: StretchDIBits and DibToDevice sources (1:1 or a whole-number enlargement) into the 16bpp or 32bpp screen (bpp = the screen's), done by the CPU in the VxD (the engine cannot read system memory). Every source pixel becomes kx x ky destination pixels
 * (floor mapping, measured by VX2SDI2 for 555, 24-bit and 32-bit sources). The source rectangle is given in top-down coordinates (sy = first row of the rectangle counted from the top of the picture).
 * Colours: 24 and 32 bit are truncated to 565; a 555 source takes the stretch convention (green sixth bit = low bit of the 5-bit green) unless VX2D_STR_555ONE asks for the 1:1 one (sixth bit 0). */
#define VX2D_STR_555ONE 1
#define VX2D_STR_565 2 /* the 16-bit source is 5-6-5 (BI_BITFIELDS), not 5-5-5 */
#define VX2D_STR_RLE 4 /* v10.104: src_bits 8 (BI_RLE8) or 4 (BI_RLE4): the source is the compressed stream (src_bytes long, src_lin its first byte); src_stride = the bitmap's width in pixels, src_width the same; bottom-up only */
typedef struct {
 uint32_t cb,abi,flags,bpp;
 vx2d_surface dst;
 uint32_t dx,dy,kx,ky;
 uint32_t sx,sy,sw,sh;
 uint32_t left,top,right,bottom;
 uint32_t src_lin,src_stride,src_bits,src_height,src_topdown,src_bytes;
 uint32_t pal_n,pal[256]; /* v10.102: 8-bit sources: the colour table (RGBQUAD as dwords 0x00RRGGBB), pal_n entries (1..256), the rest zero; 0 for every other source. v10.104: also 1-bit (1..2 entries), 4-bit (1..16) and RLE sources */
 uint32_t src_width; /* v10.104: the bitmap's width in pixels (used by RLE sources, 0 otherwise) */
} vx2d_stretch;
/* v10.116: GDI drawing from system memory (OP_VX_XFER). The VxD reads the source through its linear address (pages locked for the call).
 * XF_MONO   a 1-bit source fed to the engine through the host window (cookbook "host-fed mono bitmap", command CC0A0141, rows padded to 32 pixels and the box clipped
 *           to the destination): 1 bits are drawn in fg, 0 bits in bg (XF_TRANS: 0 bits leave the screen alone). ROP SRCCOPY only.
 * XF_STAGE  the CPU writes the source (1-bit expanded to fg/bg, or the screen's own depth copied) into the staging block in video memory, the engine then copies it to the screen with the ROP3
 *           (the proven screen-to-screen copy: every ROP3, XF_FIXPAT = the pattern operand is the solid colour pfg). For ROPs that read the destination (the CPU reads video memory at 30 MB/s).
 * XF_TEXT   (v10.116-v10.121, removed in v10.122: DIBENG drawing into a 1-bit buffer plus the engine was slower than DIBENG drawing the text itself; the fields box_lin..soft stay in the packet, unused)
 * XF_SPANS  n spans (y, x0, x1; x1 exclusive) filled like tpl (op FILL, MONO or COLOR, its ROP, colours and pattern), one engine command each.
 * Registry ACCEL_HOST (default 22): 1 XF_MONO (off by default: no faster than DIBENG), 2 XF_STAGE with a 1-bit source, 4 XF_STAGE with a colour source, 8 unused, 16 XF_SPANS. ACCEL_HOST_MIN (default 256 pixels: XF_STAGE, XF_SPANS),
 * ACCEL_MONO_MIN (default 4096: XF_MONO). Live control 0xF7000000|mask (written to the DRV's copy too), counters 0xFFFFFE18 / 0xFFFFFE19. */
#define VX2D_XF_MONO 1
#define VX2D_XF_STAGE 2
#define VX2D_XF_TEXT 3
#define VX2D_XF_SPANS 4
#define VX2D_XF_SAVE 5 /* v10.124: SaveScreenBitmap: flags 1 save the rectangle dx,dy,w,h of the screen into the save block, 2 restore it (same rectangle, same mode), 4 discard. Engine copies; ACCEL_HOST bit 8 */
#define VX2D_XF_SAVE_SIZE 0x400000UL
#define VX2D_XF_TRANS 1
#define VX2D_XF_FIXPAT 2
#define VX2D_XF_OPQ 4
#define VX2D_XF_BOXR 8
#define VX2D_XF_INK 32 /* v10.119, XF_TEXT: the string has characters that draw something: a buffer without a set bit means DIBENG did not draw into it; handed back (reason 24) */
#define VX2D_XF_RECTS 16 /* v10.118, XF_SPANS: the entries are rectangles of 4 words (y, x0, x1, height), at most 96 */
#define VX2D_XF_COLPAT 64 /* .595, XF_STAGE: the ROP's pattern operand is the 8x8 block of realized brush pixels in tpl.pattern (screen-anchored, as VX2D_COLOR), not a solid colour */
#define VX2D_XF_SPANS_MAX 128
#define VX2D_XF_STAGE_SIZE 0x200000UL
typedef struct {
 uint32_t cb,abi,op,flags,bpp,rop,fg,bg,pfg;
 uint32_t dx,dy,w,h; /* destination on the screen (XF_TEXT: where the buffer's pixel 0,0 lands, w x h = the buffer's size) */
 uint32_t src_lin,src_bits,src_x,src_y,src_w,src_h; /* the bitmap: linear address of row 0, bits per pixel, the rectangle's first column and row, the bitmap's size */
 int32_t src_stride; /* bytes from row y to row y+1 (negative: the rows go up in memory) */
 uint32_t box_lin; /* XF_TEXT: the cell-box buffer (same layout), 0 = none */
 int32_t ol,ot,orr,ob; /* XF_TEXT with XF_OPQ: the opaque rectangle in buffer coordinates (right, bottom exclusive) */
 uint32_t soft; /* XF_TEXT: estimated edges */
 uint32_t mask_lin; /* linear address of the DRV's copy of the live mask (a WORD), 0 = none */
 uint32_t done,mask_out,result; /* written back: spans drawn, the live mask, the reason of a SKIP */
 uint32_t n; /* XF_SPANS: number of spans */
 uint16_t sp[3*VX2D_XF_SPANS_MAX];
 vx2d_request tpl; /* XF_SPANS: the fill */
} vx2d_xfer;
/* v10.106: video overlay (V1 video engine, registers 0x200..0x2FF of the register window). One request, VXO_REPLY_DWORDS dwords back (VXO_R_*). Registry ACCEL_OVERLAY (default 1: VX2OVL passed on the VX855; 0 switches the overlay off):
 * bits 0-1 = the display's register set (1 primary, 2 second display), bit 3 = alternative source width count (width instead of width-1), bit 4 = at 16bpp the colour key's low bits are replicated (default zero),
 * v10.108: bit 6 (64) = the HQV scaler is used for shrinking and for RGB sources (RGB565, RGB555, RGB32), bit 7 (128) = planar YV12/I420 sources, bit 8 (256) = source colour (chroma) key, bit 9 (512) = bob. Bits 2 and 5 are unused now. */
#define VX_IOCTL_OVL 0x8550001CUL
#define VXO_ABI 0x00010000UL
#define VXO_QUERY 0 /* report only */
#define VXO_UPDATE 1 /* show/move/scale: surface at offset, source and destination rectangles, optional colour keys */
#define VXO_FLIP 2 /* offset = the new surface (same rectangles as the last update) */
#define VXO_HIDE 3
#define VXO_REGW 6 /* diagnostic (mask bit 2048): write the register at offset (0x200..0x3FC) with key; the reply's VXO_R_UPDATES holds the register read back */
#define VXO_REGR 7 /* diagnostic: read the register at offset */
#define VXO_MOVE 4 /* dl, dt = the new top left corner of the destination, size as in the last update */
#define VXO_KEY 1 /* flags: key holds the destination colour key (pixel value in the screen's format) */
#define VXO_SRCKEY 2 /* flags: ckl..ckh hold the source colour key as a 24-bit value (Y<<16|Cb<<8|Cr) */
#define VXO_BOB 4 /* flags: bob de-interlacing */
#define VXO_INTERLEAVED 8 /* flags: with VXO_BOB, the two fields are interleaved in the surface */
#define VXO_ALPHA 16 /* flags: constant alpha for the video over the graphics, 0..255 in bits 31..24 of key (255 = opaque) */
#define VXO_FIRE 32UL
#define VXO_FLD_EVEN 64UL /* v10.115 flags of VXO_FLIP: DDFLIP_EVEN, the new surface holds the even field (lines 0,2,..) */
#define VXO_FLD_ODD 128UL /* v10.115 flags of VXO_FLIP: DDFLIP_ODD; neither = toggle the field as before */ /* probe REGW: fire the V1 update (0x298 bit 31) after the write */
#define VXO_FMT_YUY2 0
#define VXO_FMT_RGB16 1
#define VXO_FMT_YV12 2 /* planes Y, V, U */
#define VXO_FMT_I420 3 /* planes Y, U, V */
#define VXO_FMT_RGB32 4
#define VXO_FMT_RGB15 5
#define VXO_M_HQV 64UL
#define VXO_M_PLANAR 128UL
#define VXO_M_CKEY 256UL
#define VXO_M_BOB 512UL
#define VXO_M_ALPHA 1024UL
#define VXO_M_PROBE 2048UL
#define VXO_M_KEYRAW 4096UL /* .585: at 16 bits the destination key is the 5-6-5 pixel value itself (OpenChrome writes the pixel value at every depth) */
#define VXO_R_RESULT 0 /* 0 done, 1 refused (VXO_R_REASON), 2 done but nothing visible (fully clipped) */
#define VXO_R_REASON 1
#define VXO_R_ON 2
#define VXO_R_MASK 3
#define VXO_R_UPDATES 4
#define VXO_R_FLIPS 5
#define VXO_R_REFUSED 6
#define VXO_R_HIDES 7
#define VXO_R_V1CTL 8 /* 0x230 as read after the last command */
#define VXO_R_COMPOSE 9 /* 0x298 */
#define VXO_R_START 10 /* 0x254 */
#define VXO_R_SRCH 11 /* 0x26C */
#define VXO_R_WIN 12 /* 0x240 */
#define VXO_R_BUF1 13 /* 0x238, the second buffer address */
#define VXO_R_HQV 14 /* 1 when the last picture goes through the HQV scaler */
#define VXO_R_HQVCTL 15 /* 0x3D0 */
#define VXO_REPLY_DWORDS 16
typedef struct {
 uint32_t cb,abi,cmd,flags,fmt,offset,pitch,sw,sh;
 int32_t sl,st,sr,sb;
 int32_t dl,dt,dr,db;
 uint32_t key,ckl,ckh;
} vx_ovl_req;
typedef struct {
 uint32_t cb,abi,state,flags,ops,fallbacks,fills,copies;
 uint32_t mono,color,clips,src_keys,dst_keys,ddraw,timeouts;
 uint32_t resets,rejected,last_command,last_status,last_result;
} vx2d_stats;
#ifdef VXD32
int VX2D_bind(uint32_t,uint32_t);
int VX2D_lost(void);
int VX2D_is_lost(void); /* v10.139: the lost state without finishing pending work (safe under hda_sem) */
void VX2D_device_lost(void);
int VX2D_native_fault(void); /* Native D3D fault only; no completion or recovery. */
int VX2D_access(uint32_t);
int VX2D_submit(const vx2d_request *);
int VX2D_stretch(const vx2d_stretch *);
int VX2D_xfer(vx2d_xfer *);
int VX2D_gamma_load(const uint16_t *);
void VX2D_gamma_off(void);
int VX2D_gamma_wanted(void);
void VX2D_control(uint32_t,uint32_t *);
void VX2D_sync(void);
void VX2D_complete(void); /* v10.425 */
void VX2D_stats(vx2d_stats *);
uint32_t VX2D_mmio_read(uint32_t);
void VX2D_hwcur_hide(void);
int VX2D_hwcur_set(const uint32_t *,const uint32_t *,uint32_t,uint32_t,uint32_t,uint32_t);
int VX2D_hwcur_move(int32_t,int32_t);
int VX2D_hwcur_active(void);
void VX2D_hwcur_invalidate(void); /* v10.252: after an engine reset */
extern volatile uint32_t vx2d_reset_epoch;
void VX2D_ovl(const vx_ovl_req *,uint32_t *);
void VX2D_ovl_hide(void);
#endif
#endif
