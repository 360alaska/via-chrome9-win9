/* v10.78: bounded capture of what DIBENG hands the driver for non-solid pattern brush fills (hatches above all). MIT license.
 * Armed and read through ExtEscape(0x8F72) by VX2AUTO; off until armed. It changes nothing about drawing. */
#ifndef VX_HATCH_DIAG_H
#define VX_HATCH_DIAG_H
#include <stdint.h>
#define VXH_ESCAPE 0x8F72
#define VXH_MAGIC 0x31485856UL /* VXH1 */
#define VXH_ARM 1
#define VXH_STOP 2
#define VXH_READ 3
#define VXH_ARM_CALLS 4 /* v10.91: also record the DDI calls that are not drawn by the driver's own hooks (see vx_rawcap in dibthunk.asm and 'OUTP' in dibcall.c); VXH_READ returns them as records, VXH_STOP ends the capture */
#define VXH_TAG_FBRD 0x44524246UL /* FastBorder: 20 raw stack words in flags..mask (last Pascal parameter first) */
#define VXH_TAG_STRB 0x42525453UL /* Strblt */
#define VXH_TAG_DTDV 0x56445444UL /* DibToDevice */
#define VXH_TAG_SDIB 0x42494453UL /* StretchDIBits */
#define VXH_TAG_SCLR 0x524C4353UL /* ScanLR */
#define VXH_TAG_PIXL 0x4C584950UL /* Pixel */
#define VXH_ARM_BLITS 5 /* v10.321: only the calls that put a memory bitmap or a DIB on the screen (BitBlt / StretchBlt from memory, StretchDIBits, DibToDevice; the screen as the destination), one structured record each: */
/* rop = tag; x, y = destination (signed); w, h = destination size; style = sx | sy << 16 (StretchDIBits: sy counted from the bottom); brush_bpp = source bits per pixel;
 * flags = BitBlt / StretchBlt: the source's first word (0 GDI bitmap, 5250 DIB engine) | its deFlags << 16; DIB calls: biCompression | 0x80000000 for a top-down DIB;
 * hatch = the driver's own attempt (FFFF none, 0 declined: DIBENG drew it, 1 drawn, 2 failed, 3 lost); text_color = DIBENG's return value (FFFFFFFF when DIBENG was not called);
 * raw_fg = ROP; raw_bg = StretchBlt / StretchDIBits sw | sh << 16, DibToDevice iScan | cScans << 16; dm_valid = 1 with a clip rectangle, mask[0] = left | top << 16, mask[1] = right | bottom << 16;
 * mono[0] = screen width | height << 16, mono[1] = screen pitch; bits[0] source width, bits[1] source height (signed), bits[2] source row bytes, bits[3] the source bits' linear address,
 * bits[4] the destination's deFlags, bits[5] biClrUsed (DIB calls), bits[6] 1 when the destination is the screen's PDEVICE, bits[7] DIB calls: dw | dh << 16 */
#define VXH_TAG_BBLT 0x544C4242UL /* BitBlt 'BBLT' */
#define VXH_TAG_SBLT 0x544C4253UL /* StretchBlt 'SBLT' */
#define VXH_TAG_XDIB 0x42494458UL /* StretchDIBits 'XDIB' */
#define VXH_TAG_XDTD 0x44544458UL /* DibToDevice 'XDTD' */
#define VXH_TAG_OUTP 0x5054554FUL /* an Output call that went to DIBENG: x,y first point, w count, h style, flags pen style | pen flags << 16, brush_bpp pen bpp, raw_fg pen colour, raw_bg pen mono, hatch brush style (FFFF none), mono[0] brush flags, mask[0] Rop2, bk_mode, bits[0..3] the first four points (x | y << 16) */
#define VXH_CAP 32
typedef struct {uint32_t cb,magic,command,reserved;} vxh_command;
typedef struct {
 uint32_t seq,rop,bpp,x,y,w,h;
 uint32_t flags,brush_bpp,style,hatch,raw_fg,raw_bg;
 uint32_t mono[2],mask[2];   /* row k's byte (index k*4 of the brush arrays) is byte k, row 0 lowest */
 uint32_t bits[8];            /* the first 8 realized pixels (bytes, words or dwords by depth) */
 uint32_t dm_valid,bk_mode,text_color,bk_color;
 uint32_t eq64;               /* v10.87: 8bpp, how many of the 64 brush bytes equal the first one */
} vxh_record;
typedef struct {uint32_t cb,magic,abi,count,dropped,active; vxh_record records[VXH_CAP];} vxh_reply;
#endif
