/*****************************************************************************

Copyright (c) 2025 Jaroslav Hensl <emulator@emulace.cz>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

*****************************************************************************/

#include "winhack.h"
#include "vmm.h"
#include "vxd.h"

#include "vxd_lib.h"
#include "3d_accel.h"

#include "code32.h"

#include "wram.h"

wram_t *wram = NULL;

BOOL wram_init(DWORD bytes)
{
	DWORD x;
	DWORD pages = RoundToPages(bytes);

	if(pages < 16) return FALSE;

	wram = (wram_t*)_PageAllocate(pages, PG_SYS, 0, 0, PAGE_ALLOC_MIN, PAGE_ALLOC_MAX, NULL, PAGEFIXED|PAGEUSEALIGN);

	if(wram == NULL)
	{
		dbg_printf("wram_init: _PageAllocate FAILURE\n");
		return FALSE;
	}

	memset(wram, 0, WRAM_FB_TOP); // clear first 3 pages
	
	for(x = 0; x < 256; x++)
	{
		wram->gamma.red[x]   = x;
		wram->gamma.green[x] = x;
		wram->gamma.blue[x]  = x;
		wram->palette[x].dw  = ((x << 16) | (x << 8) | x);
	}

	/* default screen */
	wram->regs.s.fbstart = WRAM_FB_TOP;
	wram->regs.s.size    = bytes;
	wram->regs.s.width   = 640;
	wram->regs.s.height  = 480;
	wram->regs.s.mode    = MODE_32;
	wram->regs.s.pitch   = wram->regs.s.width*4;
	wram->regs.s.cursor_visible = 0;
	wram->regs.s.cursor_x = 0;
	wram->regs.s.cursor_y = 0;
	wram->regs.s.cursor_w = 0;
	wram->regs.s.cursor_h = 0;
	wram->regs.s.gamma_enable = 0;
	wram->regs.s.fbmin    = WRAM_FB_TOP;
	wram->regs.s.fbmax    = pages * P_SIZE;

	dbg_printf("WRAM init SUCCESS (regs.s.size=%ld)\n", wram->regs.s.size);

	return TRUE;
}

static DWORD readpx8(DWORD x, DWORD y)
{
	BYTE index = ((BYTE*)wram)[wram->regs.s.fbstart + wram->regs.s.pitch*y + x];
	return wram->palette[index].dw;
}

static DWORD readpx32(DWORD x, DWORD y)
{
	DWORD *ptr32 = (DWORD*)(&(((BYTE*)wram)[wram->regs.s.fbstart + wram->regs.s.pitch*y]));
	return ptr32[x];
}

static DWORD readpx24(DWORD x, DWORD y)
{
	/* v10.330 (review #46): three bytes, not a dword that reached the next pixel (or past the last one) */
	BYTE *p = &(((BYTE*)wram)[wram->regs.s.fbstart + wram->regs.s.pitch*y + x*3]);
	return (DWORD)p[0] | ((DWORD)p[1] << 8) | ((DWORD)p[2] << 16);
}

static DWORD readpx16(DWORD x, DWORD y)
{
	WORD px16 = ((WORD*)(&(((BYTE*)wram)[wram->regs.s.fbstart + wram->regs.s.pitch*y])))[x];
	return ((px16 & 0xF800) << 8) | ((px16 & 0x07E0) << 5) | ((px16 & 0x001F) << 3);
}

static DWORD readpx15(DWORD x, DWORD y)
{
	WORD px16 = ((WORD*)(&(((BYTE*)wram)[wram->regs.s.fbstart + wram->regs.s.pitch*y])))[x];
	return ((px16 & 0x7C00) << 9) | ((px16 & 0x03E0) << 6) | ((px16 & 0x001F) << 3);
}

static void writepx32(blit_t *blit, DWORD x, DWORD y, DWORD px32)
{
	((DWORD*)&(blit->dst[0].px8[blit->dst_pitch * y]))[x] = px32;
}

static void writepx16(blit_t *blit, DWORD x, DWORD y, DWORD px32)
{
	WORD px16 = ((px32 & 0xF80000) >> 8) | ((px32 & 0xFC00) >> 5) | ((px32 & 0xF8) >> 3);
	((WORD*)&(blit->dst[0].px8[blit->dst_pitch * y]))[x] = px16;
}

static void writepx15(blit_t *blit, DWORD x, DWORD y, DWORD px32)
{
	WORD px15 = ((px32 & 0xF80000) >> 9) | ((px32 & 0xF800) >> 6) | ((px32 & 0xF8) >> 3);
	((WORD*)&(blit->dst[0].px8[blit->dst_pitch * y]))[x] = px15;
}

static void writepx24(blit_t *blit, DWORD x, DWORD y, DWORD px32)
{
	BYTE *ptr8 = &blit->dst[0].px8[blit->dst_pitch * y + x * 3]; /* v10.330 (review #45): three bytes a pixel (it stepped one) */
	ptr8[0] = px32;       /* v10.330: blue, green, red in memory, as readpx24 and the 32-bit writer take them (it wrote red first) */
	ptr8[1] = px32 >> 8;
	ptr8[2] = px32 >> 16;
}

/* v10.330 (review #141): an 8bpp shadow to an 8bpp screen copies palette indices (no colour conversion can give an index back); the
 * cursor is applied to the index bytes as the DIB engine's 8bpp cursor is (AND, then XOR with the shape's index or FF to invert) */
static void wram_blit8(blit_t *blit)
{
	DWORD x, y;
	LONG cex = wram->regs.s.cursor_x + wram->regs.s.cursor_w, cey = wram->regs.s.cursor_y + wram->regs.s.cursor_h;
	for(y = blit->src_sy; y < blit->src_ey; y++)
	{
		BYTE *d = blit->dst[0].px8 + y * blit->dst_pitch;
		const BYTE *sp = (const BYTE*)wram + wram->regs.s.fbstart + y * wram->regs.s.pitch;
		memcpy(d + blit->src_sx, sp + blit->src_sx, blit->src_ex - blit->src_sx);
		if(wram->regs.s.cursor_visible && (LONG)y >= wram->regs.s.cursor_y && (LONG)y < cey)
			for(x = blit->src_sx; x < blit->src_ex; x++)
				if((LONG)x >= wram->regs.s.cursor_x && (LONG)x < cex)
				{
					DWORD cp = ((y - wram->regs.s.cursor_y)*CURSOR_DMAX) + (x - wram->regs.s.cursor_x);
					d[x] = (BYTE)((sp[x] & (BYTE)wram->cursor.andmask[cp]) ^ (BYTE)wram->cursor.xormask[cp]);
				}
	}
}

typedef DWORD (*readfn_h)(DWORD x, DWORD y);
typedef void (*writefn_h)(blit_t *blit, DWORD x, DWORD y, DWORD px32);

void wram_blit(blit_t *blit)
{
	readfn_h readfn = NULL;
	writefn_h writefn = NULL;
	DWORD x, y, sx, sy;
	pixel_32_t px;
	LONG cursor_ex = wram->regs.s.cursor_x + wram->regs.s.cursor_w;
	LONG cursor_ey = wram->regs.s.cursor_y + wram->regs.s.cursor_h;
	DWORD scans = blit->dst_scans;

	switch(wram->regs.s.mode)
	{
		case MODE_32: readfn = readpx32; break;
		case MODE_24: readfn = readpx24; break;
		case MODE_16: readfn = readpx16; break;
		case MODE_15: readfn = readpx15; break;
		case MODE_8:  readfn = readpx8;  break;
	}

	switch(blit->dst_mode)
	{
		case MODE_32: writefn = writepx32; break;
		case MODE_24: writefn = writepx24; break;
		case MODE_16: writefn = writepx16; break;
		case MODE_15: writefn = writepx15; break;
	}

	/* v10.330 (review #54): the source rectangle inside the shadow screen, and the scaled rectangle inside the destination, before either
	 * path reads or writes (the conversion path checked neither) */
	if(blit->src_sx > blit->src_ex || blit->src_sy > blit->src_ey ||
	   blit->src_ex > wram->regs.s.width || blit->src_ey > wram->regs.s.height) return;
	if(scans == 0 || blit->dst_padx + blit->src_ex * scans > blit->dst_w || blit->dst_pady + blit->src_ey * scans > blit->dst_h) return;
	if(wram->regs.s.fbstart + wram->regs.s.pitch * wram->regs.s.height > wram->regs.s.fbmax) return; /* the shadow's allocation */

	if(wram->regs.s.mode == MODE_8 && blit->dst_mode == MODE_8)
	{
		if(scans == 1 && blit->dst_padx == 0 && blit->dst_pady == 0) wram_blit8(blit);
		return;
	}

	if(readfn == NULL || writefn == NULL) return;

 /* Exact 16/32-bit modes need no conversion or scaling. Copy rows directly,
  * then composite only the cursor intersection. This preserves framebuffer
  * bits and avoids converting every desktop pixel on every update. */
 if(scans==1 && blit->dst_padx==0 && blit->dst_pady==0 &&
    wram->regs.s.mode==blit->dst_mode && !wram->regs.s.gamma_enable &&
    (blit->dst_mode==MODE_16 || blit->dst_mode==MODE_32)) {
  DWORD bytes=blit->dst_mode==MODE_16 ? 2 : 4;
  DWORD row_bytes;
  DWORD cx0,cy0,cx1,cy1;
  if(blit->src_sx>blit->src_ex || blit->src_sy>blit->src_ey ||
     blit->src_ex>wram->regs.s.width || blit->src_ey>wram->regs.s.height)
   return;
  row_bytes=(blit->src_ex-blit->src_sx)*bytes;
  for(y=blit->src_sy; y<blit->src_ey; y++)
   memcpy(blit->dst[0].px8+y*blit->dst_pitch+blit->src_sx*bytes,
    (BYTE*)wram+wram->regs.s.fbstart+y*wram->regs.s.pitch+blit->src_sx*bytes,
    row_bytes);
  if(!wram->regs.s.cursor_visible) return;
  cx0=wram->regs.s.cursor_x<0 ? 0 : (DWORD)wram->regs.s.cursor_x;
  cy0=wram->regs.s.cursor_y<0 ? 0 : (DWORD)wram->regs.s.cursor_y;
  cx1=cursor_ex<0 ? 0 : (DWORD)cursor_ex;
  cy1=cursor_ey<0 ? 0 : (DWORD)cursor_ey;
  if(cx0<blit->src_sx) cx0=blit->src_sx;
  if(cy0<blit->src_sy) cy0=blit->src_sy;
  if(cx1>blit->src_ex) cx1=blit->src_ex;
  if(cy1>blit->src_ey) cy1=blit->src_ey;
  for(y=cy0; y<cy1; y++) for(x=cx0; x<cx1; x++) {
   DWORD cp=((y-wram->regs.s.cursor_y)*CURSOR_DMAX)+(x-wram->regs.s.cursor_x);
   px.dw=readfn(x,y);
   px.dw &= wram->cursor.andmask[cp];
   px.dw ^= wram->cursor.xormask[cp];
   writefn(blit,x,y,px.dw);
  }
  return;
 }

	//dbg_printf("wram_blit, mode=%d -> mode=%d, dst=%lX\n", wram->regs.s.mode, blit->dst_mode, blit->dst[0].flat.dw);

	for(y = blit->src_sy; y < blit->src_ey; y++)
	{
		for(x = blit->src_sx; x < blit->src_ex; x++)
		{
			px.dw = readfn(x, y);
			if(wram->regs.s.gamma_enable)
			{
				px.c.r = wram->gamma.red[px.c.r];
				px.c.g = wram->gamma.green[px.c.g];
				px.c.b = wram->gamma.blue[px.c.b];
			}
			if(wram->regs.s.cursor_visible)
			{
				if((LONG)x >= wram->regs.s.cursor_x && (LONG)x < cursor_ex)
				{
					if((LONG)y >= wram->regs.s.cursor_y && (LONG)y < cursor_ey)
					{
						DWORD cp = ((y - wram->regs.s.cursor_y)*CURSOR_DMAX) + (x - wram->regs.s.cursor_x);
						px.dw &= wram->cursor.andmask[cp];
						px.dw ^= wram->cursor.xormask[cp];
					}
				}
			}

			for(sy = 0; sy < scans; sy++)
			{
				for(sx = 0; sx < scans; sx++)
				{
					writefn(blit,
						blit->dst_padx + x*scans + sx,
						blit->dst_pady + y*scans + sy,
						px.dw);
				}
			}
		}
	}
}

#define SWAP_DW(_a, _b) do{\
	DWORD swtmp = _a; \
	_a = _b; \
	_b = swtmp;}while(0)

void wram_changes(blit_t *blit, DWORD sx, DWORD sy, DWORD ex, DWORD ey)
{
	if(sx > ex)
	{
		SWAP_DW(sx, ex);
	}

	if(sy > ey)
	{
		SWAP_DW(sy, ey);
	}

	/* v10.330 (review #55): clipped to the screen first; a rectangle left empty (wholly off screen, or no width or height) is not a change
	 * (it became a one-pixel change at the screen's edge) */
	if(ex > wram->regs.s.width)  ex = wram->regs.s.width;
	if(ey > wram->regs.s.height) ey = wram->regs.s.height;
	if(sx >= ex || sy >= ey) return;

	if(blit->num_changes == 0)
	{
		blit->src_sx = sx;
		blit->src_ex = ex;
		blit->src_sy = sy;
		blit->src_ey = ey;
	}
	else
	{
		if(sx < blit->src_sx)	blit->src_sx = sx;
		if(sy < blit->src_sy) blit->src_sy = sy;
		
		if(ex > blit->src_ex) blit->src_ex = ex;
		if(ey > blit->src_ey) blit->src_ey = ey;		
	}
	blit->num_changes++;
}
