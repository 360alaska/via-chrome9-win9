/*
 * This code is based on gtf.c from XFree86 project:
 *
 * gtf.c  Generate mode timings using the GTF Timing Standard
 *
 * gcc gtf.c -o gtf -lm -Wall
 *
 * Copyright (c) 2001, Andy Ritger  aritger@nvidia.com
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * o Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * o Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer
 *   in the documentation and/or other materials provided with the
 *   distribution.
 * o Neither the name of NVIDIA nor the names of its contributors
 *   may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT
 * NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * 
 *
 * This program is based on the Generalized Timing Formula(GTF TM)
 * Standard Version: 1.0, Revision: 1.0
 *
 * The GTF Document contains the following Copyright information:
 *
 * Copyright (c) 1994, 1995, 1996 - Video Electronics Standards
 * Association. Duplication of this document within VESA member
 * companies for review purposes is permitted. All other rights
 * reserved.
 *
 * While every precaution has been taken in the preparation
 * of this standard, the Video Electronics Standards Association and
 * its contributors assume no responsibility for errors or omissions,
 * and make no warranties, expressed or implied, of functionality
 * of suitability for any purpose. The sample code contained within
 * this standard may be used without restriction.
 *
 * 
 *
 * The GTF EXCEL(TM) SPREADSHEET, a sample (and the definitive)
 * implementation of the GTF Timing Standard, is available at:
 *
 * ftp://ftp.vesa.org/pub/GTF/GTF_V1R1.xls
 *
 *
 *
 * This program takes a desired resolution and vertical refresh rate,
 * and computes mode timings according to the GTF Timing Standard.
 * These mode timings can then be formatted as an XFree86 modeline
 * or a mode description for use by fbset(8).
 *
 *
 *
 * NOTES:
 *
 * The GTF allows for computation of "margins" (the visible border
 * surrounding the addressable video); on most non-overscan type
 * systems, the margin period is zero.  I've implemented the margin
 * computations but not enabled it because 1) I don't really have
 * any experience with this, and 2) neither XFree86 modelines nor
 * fbset fb.modes provide an obvious way for margin timings to be
 * included in their mode descriptions (needs more investigation).
 * 
 * The GTF provides for computation of interlaced mode timings;
 * I've implemented the computations but not enabled them, yet.
 * I should probably enable and test this at some point.
 *
 * 
 *
 * TODO:
 *
 * o Add support for interlaced modes.
 *
 * o Implement the other portions of the GTF: compute mode timings
 *   given either the desired pixel clock or the desired horizontal
 *   frequency.
 *
 * o It would be nice if this were more general purpose to do things
 *   outside the scope of the GTF: like generate double scan mode
 *   timings, for example.
 *   
 * o Printing digits to the right of the decimal point when the
 *   digits are 0 annoys me.
 *
 * o Error checking.
 *
 */
#include "winhack.h"
#include "vmm.h"
#include "vxd.h"

//#include "wram.h"
//#include "async.h"

#include "vxd_lib.h"
#include "3d_accel.h"

#include "vesa.h"
#include "mtrr.h"

#include "code32.h"

/* E782 (outside audit of E768, VXD-2; the method from the owner's other build line, .768B / .769B): the GTF formula in whole numbers.
 * The XFree86 code this file held worked in float and double, in ring 0, in the calling program's thread, with nothing saving that
 * thread's x87 registers: a mode set could leave the program's floating-point state changed.
 * The driver uses GTF with no margins and no interlace, and then every quantity is a ratio of whole numbers. With f the refresh rate
 * and h the lines:
 *   the estimated line period is (1,000,000 - 550 f) / (f (h + 1)) microseconds, so
 *   vertical sync + back porch = round(550 f (h + 1) / (1,000,000 - 550 f)) lines (550 us at least),
 *   total lines vt = h + that + 1 (the front porch), and the line period is exactly 1,000,000 / D microseconds, D = f x vt;
 *   the ideal blanking duty cycle is 30 - 300,000 / D percent (C' 30, M' 300), so
 *   horizontal blanking = round(w x 3 (D - 10,000) / (16 (7 D + 30,000))) cells of 16 pixels;
 *   horizontal sync = round(total / 100) cells of 8 pixels (8% of the line); the front porch = blanking / 2 - sync;
 *   the pixel clock = total x D Hz, exactly.
 * round is half up: (n + d / 2) / d (the old code's rint rounded an exact half to even: a width of 16 k + 4, such as 1364, went down
 * to 1360 and now goes up to 1368; no size this driver offers is one). E787 (outside review of this arithmetic): a size and rate whose
 * products would pass 32 bits (beyond 2048x1536 at 240 Hz) is timed at 60 Hz instead of with wrapped numbers.
 * Host check against the old float code, 120,032 modes (widths 320-2055, heights 200-1536, 50-100 Hz): the same timings but for 8
 * where the blanking is exactly half a cell (369x262 at 80 Hz: 4.5 cells), which the float code rounded down by its own error; pixel
 * clocks within 0.24 parts in a million. The refresh rate is held to 240 Hz (0 means 60): up to 2048x1536 no product passes 32 bits.
 * The rate reported is the one the timing was made for. */
void gtf_calc(DWORD w, DWORD h, DWORD freq, vesa_crtc_info_t *crtc_info)
{
	DWORD f = freq, wr, den, vbp, vt, D, a, b, blank, total, hs, fp;

	if(!f) f = 60;
	if(f > 240) f = 240;
	wr = ((w + 4) / 8) * 8;
	den = 1000000UL - 550UL * f;
	vbp = (550UL * f * (h + 1) + den / 2) / den;
	vt = h + vbp + 1;
	D = f * vt;
	if(f > 60 && D > 10000UL && (wr * 3UL > 0xFFFFFFFFUL / (D - 10000UL) || (wr + wr / 2) > 0xFFFFFFFFUL / D))
	{	/* E787: the products below would not fit 32 bits (total is under 1.5 wr): 60 Hz */
		f = 60; den = 1000000UL - 550UL * f; vbp = (550UL * f * (h + 1) + den / 2) / den; vt = h + vbp + 1; D = f * vt;
	}
	a = D > 10000UL ? wr * 3UL * (D - 10000UL) : 0; /* (a rate so low that the duty cycle would be negative: no blanking, not a wrapped number) */
	b = 16UL * (7UL * D + 30000UL);
	blank = ((a + b / 2) / b) * 16;
	total = wr + blank;
	hs = ((total + 50) / 100) * 8;
	fp = blank / 2 - hs; /* (as the old code: below zero for the very smallest modes, 320x200 at 70 Hz, where the sync is wider than half the blanking; the sums below then come out as its did) */

	memset(crtc_info, 0, sizeof(vesa_crtc_info_t));

	crtc_info->HorizontalTotal     = (uint16)total;
	crtc_info->HorizontalSyncStart = (uint16)(wr + fp);
	crtc_info->HorizontalSyncEnd   = (uint16)(wr + fp + hs);
	crtc_info->VerticalTotal       = (uint16)vt;
	crtc_info->VerticalSyncStart   = (uint16)(h + 1);
	crtc_info->VerticalSyncEnd     = (uint16)(h + 4); /* 3 lines of sync */
	crtc_info->PixelClock          = total * D;       /* Hz */
	crtc_info->RefreshRate         = (uint16)(f * 100); /* 0.01 Hz units */

	crtc_info->Flags               = VESA_CRTC_FLAG_HSYC_NEG | VESA_CRTC_FLAG_VSYC_POS;
}
