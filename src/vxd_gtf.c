/*
 * This code is based on gtf.c from XFree86 project (.769: rewritten in integer arithmetic, see gtf_calc):
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

/* .769 (audit VXD-C2): the GTF in integers. The float code it replaces ran the x87 in ring 0 at a mode set with no FPU state saved (the
 * calling thread's x87 registers could be changed). The same formula, no margins, not interlaced, every step exact or rounded as the
 * spreadsheet rounds (rint = round half up): h_period = 1e6 / (f * total lines) microseconds, so the blanking duty cycle
 * 30 - 300000 / D (D = f * total lines) and the pixel clock tp * f * total lines exactly. Host check (38,760 modes, 320-2055 x
 * 200-1536 at 50-100 Hz) against the float code: every timing the same but one rounding tie (1687x768 at 70 Hz), pixel clocks within
 * 0.25 ppm. */
#define V_SYNC_RQD 3 /* lines */
void gtf_calc(DWORD w, DWORD h, DWORD freq, vesa_crtc_info_t *crtc_info)
{
	uint32 f=freq?freq:60, hpr=((w+4)/8)*8, den, vsbp, tvl, D, cells, hblank, tp, hsync;
	int hfp;
	if(f>240)f=240;                                   /* every product below inside 32 bits up to 2048 x 1536 (the driver offers 1920 x 1200 at most) */
	den=1000000UL-550UL*f;                            /* (1e6/f - MIN_VSYNC_PLUS_BP) times f */
	vsbp=(550UL*f*(h+1)+den/2)/den;                   /* [V SYNC+BP] = rint(550 / [H PERIOD EST]) */
	tvl=h+vsbp+1;                                     /* [TOTAL V LINES]: + MIN_PORCH */
	D=f*tvl;                                          /* [H PERIOD] = 1e6 / D microseconds */
	if(D<=10000UL)cells=0;                            /* the ideal duty cycle at or below 0 */
	else{uint32 a=hpr*3UL*(D-10000UL),b=16UL*(7UL*D+30000UL);cells=(a+b/2)/b;} /* rint(hpr * idc / (100 - idc) / 16), idc = 30 - 300000 / D */
	hblank=cells*16;
	tp=hpr+hblank;                                    /* [TOTAL PIXELS] */
	hsync=((tp+50)/100)*8;                            /* rint(8% of tp / 8) * 8 */
	hfp=(int)(hblank/2)-(int)hsync;

	memset(crtc_info, 0, sizeof(vesa_crtc_info_t));
	crtc_info->HorizontalTotal     = tp;
	crtc_info->HorizontalSyncStart = hpr+hfp;
	crtc_info->HorizontalSyncEnd   = hpr+hfp+hsync;
	crtc_info->VerticalTotal       = tvl;
	crtc_info->VerticalSyncStart   = h+1;
	crtc_info->VerticalSyncEnd     = h+1+V_SYNC_RQD;
	crtc_info->PixelClock          = tp*f*tvl;           /* Hz: total pixels / h_period */
	crtc_info->RefreshRate         = (uint16)(f * 100);    /* 0.01 Hz units (the rate the timing has: 0 asked gives 60) */

	crtc_info->Flags               = VESA_CRTC_FLAG_HSYC_NEG | VESA_CRTC_FLAG_VSYC_POS; 
}
