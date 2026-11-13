
#define EXPAND_BIT(_dsttype, _bit) (~(((_dsttype)(_bit))-1))

/* v10.279 (review): bytes in a colour XOR mask row as the converters below read it: a 15-bit pixel is a WORD (expandmask15 reads cx WORDs),
 * the others BitsPixel bits. The DRV (dibcall.c mouse_cursor_size) copies the shape with the same rule. */
#define CURSOR_XOR_ROW(_bp, _cx) ((_bp) == 15 ? (DWORD)(_cx) * 2 : ((DWORD)(_bp) * (DWORD)(_cx) + 7) / 8)

#define DEF_CONVMASK(_bpp, _type) \
static void convmask ## _bpp(CURSORSHAPE *lpCursor, DWORD cbWidth, void *src, void *dst) { \
	int x, y, xb, x8; \
	BYTE pix8; \
	for(y = 0; y < lpCursor->cy; y++) { \
		_type *ptr = (_type *)(((DWORD*)dst)+CURSOR_DMAX*y); \
		for(x = 0; x < cbWidth; x++) { \
			pix8 = ((BYTE *)src)[cbWidth*y + x]; \
			x8 = x*8; \
			for(xb = 7; xb >= 0 && x8 < lpCursor->cx; xb--,x8++) { \ 
				*(ptr++) = EXPAND_BIT(_type, ((pix8 >> xb) & 0x1)); \
} } } }

//DEF_CONVMASK( 8,  BYTE)
//DEF_CONVMASK(16,  WORD)
DEF_CONVMASK(32, DWORD)

/* v10.139 (review F03): one 16-bit source pixel per step (the old loop read every other pixel and ran past the row), channels widened to 8 bits by
 * replicating their top bits (31 -> 255, 63 -> 255, 0 -> 0, so black and white stay exact). */
static void expandmask16(CURSORSHAPE *lpCursor, DWORD cbWidth, void *src, void *dst)
{
	LONG y, x;
	WORD px16;
	DWORD r, g, b;
	for(y = 0; y < lpCursor->cy; y++)
	{
		DWORD *ptr = ((DWORD*)dst)+CURSOR_DMAX*y;
		const WORD *row = (const WORD *)(((BYTE*)src)+cbWidth*y);
		for(x = 0; x < lpCursor->cx; x++)
		{
			px16 = row[x];
			r = (px16 >> 11) & 31; g = (px16 >> 5) & 63; b = px16 & 31;
			*ptr++ = (((r << 3) | (r >> 2)) << 16) | (((g << 2) | (g >> 4)) << 8) | ((b << 3) | (b >> 2));
		}
	}
}

static void expandmask15(CURSORSHAPE *lpCursor, DWORD cbWidth, void *src, void *dst)
{
	LONG y, x;
	WORD px16;
	DWORD r, g, b;
	for(y = 0; y < lpCursor->cy; y++)
	{
		DWORD *ptr = ((DWORD*)dst)+CURSOR_DMAX*y;
		const WORD *row = (const WORD *)(((BYTE*)src)+cbWidth*y);
		for(x = 0; x < lpCursor->cx; x++)
		{
			px16 = row[x];
			r = (px16 >> 10) & 31; g = (px16 >> 5) & 31; b = px16 & 31;
			*ptr++ = (((r << 3) | (r >> 2)) << 16) | (((g << 3) | (g >> 2)) << 8) | ((b << 3) | (b >> 2));
		}
	}
}

static void expandmask8(CURSORSHAPE *lpCursor, DWORD cbWidth, void *src, void *dst)
{
	LONG y, x;
	for(y = 0; y < lpCursor->cy; y++)
	{
		DWORD *ptr = ((DWORD*)dst)+CURSOR_DMAX*y;
		for(x = 0; x < cbWidth; x++)
		{
			BYTE index = ((BYTE*)src)[cbWidth*y + x];
#ifdef VX85598_BUILD
			/* v10.71: the native indexed compositor combines index bits directly; the shadow-to-RGB compositor keeps the palette colour */
			if(hda->bpp == 8 && (DWORD)hda->vram_pm32 == vx_status.fb_linear) *ptr = index;
			else
#endif
			*ptr = wram->palette[index].dw;
			ptr++;
		}
	}
}

static void expandmask24(CURSORSHAPE *lpCursor, DWORD cbWidth, void *src, void *dst)
{
	LONG y, x;
	pixel_32_t px;
	px.dw = 0;

	for(y = 0; y < lpCursor->cy; y++)
	{
		DWORD *ptr = ((DWORD*)dst)+CURSOR_DMAX*y;
		for(x = 0; x < cbWidth; x += 3)
		{
			px.c.b = ((BYTE*)src)[cbWidth*y + x]; /* v10.139: a 24-bit pixel is B, G, R in memory (red and blue were swapped) */
			px.c.g = ((BYTE*)src)[cbWidth*y + x + 1];
			px.c.r = ((BYTE*)src)[cbWidth*y + x + 2];
			*ptr = px.dw;
			ptr++;
		}
	}
}

static void expandmask32(CURSORSHAPE *lpCursor, DWORD cbWidth, void *src, void *dst)
{
	LONG y, x;
	for(y = 0; y < lpCursor->cy; y++)
	{
		DWORD *ptr = ((DWORD*)dst)+CURSOR_DMAX*y;
		for(x = 0; x < cbWidth; x += 4)
		{
			*ptr = *((DWORD*)(((BYTE*)src)+cbWidth*y + x));
			ptr++;
		}
	}
}

static void expandmask(CURSORSHAPE *lpCursor, DWORD cbWidth, void *src, void *dst, int bits)
{
	switch(bits)
	{
		case 1:
			convmask32(lpCursor, cbWidth, src, dst);
			break;
		case 8:
			expandmask8(lpCursor, cbWidth, src, dst);
			break;
		case 15:
			expandmask15(lpCursor, cbWidth, src, dst);
			break;
		case 16:
			expandmask16(lpCursor, cbWidth, src, dst);
			break;
		case 24:
			expandmask24(lpCursor, cbWidth, src, dst);
			break;
		case 32:
			expandmask32(lpCursor, cbWidth, src, dst);
			break;
	}
}

static BOOL cursor_is_empty()
{
	LONG x, y;	
	for(y = 0; y < wram->regs.s.cursor_h; y++)
	{
		for(x = 0; x < wram->regs.s.cursor_w; x++)
		{
			if(wram->cursor.andmask[CURSOR_DMAX*y + x] != 0xFFFFFFFFUL || /* v10.139 (review P10): convmask32 expands an AND bit to all ones */
				wram->cursor.xormask[CURSOR_DMAX*y + x] != 0x00)
			{
				return FALSE;
			}
		}
	}
	return TRUE;
}
