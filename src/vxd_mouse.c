/*****************************************************************************

Copyright (c) 2024 Jaroslav Hensl <emulator@emulace.cz>

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

#include "cursor.h"


#include "wram.h"
#include "code32.h"
#include "vx85598.h"

extern FBHDA_t *hda;

static void *mouse_buffer_mem = NULL;
volatile unsigned long vx_cursor_gen = 0; /* v10.105 */

#define CUR_MIN_SIZE (32*32*4)

#include "vxd_strings.h"

#include "vxd_mouse_conv.h"

void *mouse_buffer()
{
	if(mouse_buffer_mem == NULL)
	{
		mouse_buffer_mem = (void*)_PageAllocate(RoundToPages(MOUSE_BUFFER_SIZE), PG_SYS, 0, 0, PAGE_ALLOC_MIN, PAGE_ALLOC_MAX, NULL, PAGEFIXED|PAGEUSEALIGN);
	}
	
	return mouse_buffer_mem;
}

static void mouse_notify_accel()
{
	hda->flags |= FB_MOUSE_NO_BLIT;
}

DWORD vx_cursor_bits = 1; /* v10.330 (review #173): the loaded shape's XOR bits a pixel (vxd_vesa.c's 8bpp hardware-cursor colours) */

BOOL mouse_load()
{
	DWORD ms = 0;
	void *andmask_ptr;
	void *xormask_ptr;
	CURSORSHAPE *cur;
	DWORD cbw;
	LONG x, y;
	
	//dbg_printf(dbg_mouse_load);
	
	if(!mouse_buffer_mem) return FALSE;
		

	cur = (CURSORSHAPE*)mouse_buffer_mem;
	/* v10.139 (review F04): the expanded masks hold 32x32 pixels; anything else (Windows 98 never sends it: DIBENG's Inquire reports 32x32) is refused before conversion */
	if(cur->cx <= 0 || cur->cy <= 0 || cur->cx > CURSOR_DMAX || cur->cy > CURSOR_DMAX || cur->cbWidth < (cur->cx + 7) / 8 || cur->cbWidth > 128 || /* v10.302 (review): the row length bounded here too (the DRV caps it) */
		(cur->BitsPixel != 1 && cur->BitsPixel != 8 && cur->BitsPixel != 15 && cur->BitsPixel != 16 && cur->BitsPixel != 24 && cur->BitsPixel != 32)) return FALSE;
	
	/* erase cursor if present */
	FBHDA_access_begin(FBHDA_ACCESS_MOUSE_MOVE);

	/* check and alocate/resize buffer */
	ms = (cur->cx * cur->cy * 4);
	if(ms < CUR_MIN_SIZE)
		ms = CUR_MIN_SIZE;


	x = wram->regs.s.cursor_x + wram->regs.s.cursor_spotx;
	y = wram->regs.s.cursor_y + wram->regs.s.cursor_spoty;

	/* v10.330 (review #146): the hot spot kept inside the shape (Windows' are; a larger one moved the cursor by it) */
	wram->regs.s.cursor_spotx = cur->xHotSpot < 0 ? 0 : cur->xHotSpot >= cur->cx ? cur->cx - 1 : cur->xHotSpot;
	wram->regs.s.cursor_spoty = cur->yHotSpot < 0 ? 0 : cur->yHotSpot >= cur->cy ? cur->cy - 1 : cur->yHotSpot;

	wram->regs.s.cursor_x = x - wram->regs.s.cursor_spotx;
	wram->regs.s.cursor_y = y - wram->regs.s.cursor_spoty;
	wram->regs.s.cursor_w = cur->cx;
	wram->regs.s.cursor_h = cur->cy;

	andmask_ptr = (void*)(cur + 1);
	xormask_ptr = (void*)(((BYTE*)andmask_ptr) + cur->cbWidth*cur->cy);

	/* AND mask (always 1bpp) */
	expandmask(cur, cur->cbWidth, andmask_ptr, wram->cursor.andmask, 1);
	
	/* XOR mask (1bpp or screen bpp) */
	if(cur->BitsPixel == 1)
	{
		expandmask(cur, cur->cbWidth, xormask_ptr, wram->cursor.xormask, 1);
	}
	else
	{
		cbw = CURSOR_XOR_ROW(cur->BitsPixel, cur->cx); /* v10.279 (review): 15-bit rows are cx WORDs (expandmask15 reads them so); they were sized and stepped 15 bits a pixel */
		expandmask(cur, cbw, xormask_ptr, wram->cursor.xormask, cur->BitsPixel);
	}
	
	vx_cursor_bits = cur->BitsPixel;
	wram->regs.s.cursor_empty = cursor_is_empty();
	wram->regs.s.cursor_visible  = !wram->regs.s.cursor_empty;
	vx_cursor_gen++; /* v10.105: a new shape: the hardware cursor image is rebuilt on the next show */
	
	//dbg_printf(dbg_mouse_status, mouse_valid, mouse_visible, mouse_empty);
	
	//dbg_printf(dbg_cursor_empty, mouse_empty);

	/* blit new cursor */
	FBHDA_access_end(FBHDA_ACCESS_MOUSE_MOVE);
	
	mouse_notify_accel();
	
	return TRUE;
}

/* v10.253: the hardware cursor's position-only path (vxd_vesa.c) */
BOOL VESA_cursor_fast_move(LONG x, LONG y);
static void mouse_move_impl(int x, int y);
void mouse_move(int x, int y)
{
	if(wram->regs.s.cursor_visible && VESA_cursor_fast_move(x - wram->regs.s.cursor_spotx, y - wram->regs.s.cursor_spoty)) return;
	mouse_move_impl(x, y);
}
static void mouse_move_impl(int x, int y)
{
	//dbg_printf(dbg_mouse_move, x, y);
	
	
	if(wram->regs.s.cursor_visible)
	{
		FBHDA_access_begin(FBHDA_ACCESS_MOUSE_MOVE);
		wram->regs.s.cursor_x = x - wram->regs.s.cursor_spotx;
		wram->regs.s.cursor_y = y - wram->regs.s.cursor_spoty;
		FBHDA_access_end(FBHDA_ACCESS_MOUSE_MOVE);
	}
	else
	{
		wram->regs.s.cursor_x = x - wram->regs.s.cursor_spotx;
		wram->regs.s.cursor_y = y - wram->regs.s.cursor_spoty;
	}
}

void mouse_hide()
{
	//dbg_printf(dbg_mouse_hide);
	
	
	FBHDA_access_begin(FBHDA_ACCESS_MOUSE_MOVE);
	wram->regs.s.cursor_visible = 0;
	FBHDA_access_end(FBHDA_ACCESS_MOUSE_MOVE);
	
	mouse_notify_accel();
}

void mouse_invalidate()
{
	//dbg_printf(dbg_mouse_invalidate);
	
	
	wram->regs.s.cursor_empty   = 0;
	wram->regs.s.cursor_visible = 0;
	mouse_notify_accel();
}

BOOL mouse_get_rect(DWORD *ptr_left, DWORD *ptr_top,
	DWORD *ptr_right, DWORD *ptr_bottom)
{
	LONG mx;
	LONG my;
	LONG mw;
	LONG mh;
	/* v10.330 (review #184): the screen size as signed values, and a rectangle left empty by the clip at the left or top refused at once:
	 * a cursor wholly off the left edge (mw < 0) compared as a huge unsigned width and gave a full-width rectangle */
	LONG sw = (LONG)wram->regs.s.width;
	LONG sh = (LONG)wram->regs.s.height;
	
	if(wram->regs.s.cursor_visible)
	{
		mw = wram->regs.s.cursor_w;
		mx = wram->regs.s.cursor_x;
		
		if(mx < 0)
		{
			mw += mx;
			mx = 0;
		}
		if(mw <= 0 || mx >= sw) return FALSE;
		if(mw > sw - mx)
		{
			mw = sw - mx;
		}
		
		mh = wram->regs.s.cursor_h;
		my = wram->regs.s.cursor_y;
		
		if(my < 0)
		{
			mh += my;
			my = 0;
		}
		if(mh <= 0 || my >= sh) return FALSE;
		if(mh > sh - my)
		{
			mh = sh - my;
		}
		
		*ptr_left    = (DWORD)mx;
		*ptr_top     = (DWORD)my;
		*ptr_right   = (DWORD)(mx + mw);
		*ptr_bottom  = (DWORD)(my + mh);
		return TRUE;
	}

	return FALSE;
}
