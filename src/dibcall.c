/*****************************************************************************

Copyright (c) 2022  Michal Necasek
              2023  Jaroslav Hensl

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
#include <gdidefs.h>
#include <dibeng.h>
#include <minivdd.h>
#include <string.h>

#include "minidrv.h"
#include "cursor.h"

#include "3d_accel.h"
#include "dpmi.h"

DWORD mouse_buf_lin = 0;
void __far* mouse_buf = NULL;
BOOL mouse_vxd = FALSE;

/*
 * What's this all about? Most of the required exported driver functions can
 * be passed straight to the DIB Engine. The DIB Engine in some cases requires
 * an additional parameter.
 *
 * See the dibthunk.asm module for functions that can be handled easily. Note
 * that although the logic could be implemented in C, it can be done more
 * efficiently in assembly. Forwarders turn into simple jumps, and functions
 * with an extra parameter can be implemented with very small overhead, saving
 * stack space and extra copying.
 *
 * This module deals with the very few functions that need additional logic but
 * are not hardware specific.
 */

#pragma code_seg( _TEXT )

void WINAPI __loadds MoveCursor(int absX, int absY)
{	
	if(wEnabled)
	{
		if(mouse_vxd)
		{
			mouse_move(absX, absY);
		}
		else
		{
			DIB_MoveCursorExt(absX, absY, lpDriverPDevice);
		}
	}
}

/* v10.139 (review F04): the AND mask and a 1-bit XOR mask take cbWidth bytes a row (the VxD reads them that way; the old code used (cx+7)/8, which is short
 * for word-padded rows), a colour XOR mask (BitsPixel*cx+7)/8 bytes a row as the VxD reads it; 32-bit arithmetic; shapes larger than 32x32, with an unknown
 * depth or that do not fit the shared buffer give 0 (the cursor is hidden; Windows 98 never sends them: DIBENG's Inquire reports 32x32). */
static size_t mouse_cursor_size(CURSORSHAPE __far *lpCursor)
{
	DWORD s, cx = (DWORD)(long)lpCursor->cx, cy = (DWORD)(long)lpCursor->cy, cbw = (DWORD)(long)lpCursor->cbWidth, bp = lpCursor->BitsPixel;
	if(lpCursor->cx <= 0 || lpCursor->cy <= 0 || cx > 32 || cy > 32 || lpCursor->cbWidth < (short)((cx + 7) / 8) || lpCursor->cbWidth > 128) return 0;
	if(bp != 1 && bp != 8 && bp != 15 && bp != 16 && bp != 24 && bp != 32) return 0;
	s = cbw * cy; /* AND mask */
	if(bp == 1) s += cbw * cy; else s += (bp == 15 ? cx * 2 : (bp * cx + 7) / 8) * cy; /* v10.279 (review): a 15-bit pixel is a WORD (the VxD reads cx WORDs a row) */
	s += sizeof(CURSORSHAPE);
	if(s > 65000UL) return 0;
	return (size_t)s;
}

WORD WINAPI __loadds SetCursor_driver(CURSORSHAPE __far *lpCursor)
{
	if(wEnabled)
	{
		if(mouse_vxd)
		{
			if(lpCursor != NULL)
			{
				size_t ms = mouse_cursor_size(lpCursor);
				if(ms > 0)
				{
					_fmemcpy(mouse_buf, lpCursor, ms);
					if(mouse_load())
					{
						return 1;
					}
					return 0;
				}
			}
			mouse_hide();
			return 1;
		}
		else
		{
			DIB_SetCursorExt(lpCursor, lpDriverPDevice);
			return 1;
		}
		
	}
	
	return 0;
}

/* Exported as DISPLAY.104 */
void WINAPI __loadds CheckCursor( void )
{
	if(wEnabled)
	{
		if(!mouse_vxd)
		{
			DIB_CheckCursorExt(lpDriverPDevice);
		}
	}
}

/* If there is no hardware screen-to-screen BitBlt, there's no point in
 * this and we can just forward BitBlt to the DIB Engine.
 */
#ifdef HWBLT

#include "pm16_calls.h"

#define VX_ROP_SRCCOPY   0x00CC0020UL
#define VX_ROP_PATCOPY   0x00F00021UL

#include "vx_hatch_diag.h"
/* v10.78: hatch capture (armed by VX2AUTO through ExtEscape 0x8F72; nothing is recorded otherwise). No allocation, no I/O in the drawing hook. */
static vxh_reply vxh_log;
/* v10.91: raw argument capture of the calls dibthunk.asm forwards to DIBENG (DIBCAP): FastBorder, Strblt, DibToDevice, StretchDIBits, ScanLR, Pixel. 16 slots of 32 words; word 0 is the tag
 * (1 FBRD, 2 STRB, 3 DTDV, 4 SDIB, 5 SCLR, 6 PIXL), words 1..20 the stack words above the return address. Off unless armed with VXH_ARM_CALLS. */
WORD vx_rawcap_on,vx_rawcap_n,vx_rawcap_drop;
WORD vx_rawcap_buf[16*32];
static const DWORD vx_rawtag[7]={0,VXH_TAG_FBRD,VXH_TAG_STRB,VXH_TAG_DTDV,VXH_TAG_SDIB,VXH_TAG_SCLR,VXH_TAG_PIXL};
static void vx_rawcap_flush(void) {
 WORD i,t; vxh_record *p;
 for(i=0;i<vx_rawcap_n&&i<16;i++) {
  t=vx_rawcap_buf[i*32]; if(t<1||t>6) continue;
  if(vxh_log.count>=VXH_CAP) {vxh_log.dropped++; continue;}
  p=&vxh_log.records[(WORD)vxh_log.count]; _fmemset(p,0,sizeof(*p));
  p->seq=vxh_log.count++; p->rop=vx_rawtag[t]; p->bpp=wBpp;
  _fmemcpy(&p->flags,&vx_rawcap_buf[i*32+1],40);
 }
 vxh_log.dropped+=vx_rawcap_drop; vx_rawcap_n=0; vx_rawcap_drop=0;
}
LONG VX_hdiag_control(LPVOID in,LPVOID out) {
 vxh_command c;
 if(!in) return 0;
 _fmemcpy(&c,in,sizeof(c));
 if(c.cb!=sizeof(c)||c.magic!=VXH_MAGIC||c.reserved) return 0;
 if(c.command==VXH_ARM) {_fmemset(&vxh_log,0,sizeof(vxh_log)); vxh_log.cb=sizeof(vxh_log); vxh_log.magic=VXH_MAGIC; vxh_log.abi=1; vxh_log.active=1; vx_rawcap_on=0; vx_rawcap_n=0; vx_rawcap_drop=0; return 1;}
 if(c.command==VXH_ARM_CALLS) {_fmemset(&vxh_log,0,sizeof(vxh_log)); vxh_log.cb=sizeof(vxh_log); vxh_log.magic=VXH_MAGIC; vxh_log.abi=1; vxh_log.active=3; vx_rawcap_n=0; vx_rawcap_drop=0; vx_rawcap_on=1; return 1;}
 if(c.command==VXH_ARM_BLITS) {_fmemset(&vxh_log,0,sizeof(vxh_log)); vxh_log.cb=sizeof(vxh_log); vxh_log.magic=VXH_MAGIC; vxh_log.abi=1; vxh_log.active=4; vx_rawcap_on=0; vx_rawcap_n=0; vx_rawcap_drop=0; return 1;} /* v10.321 */
 if(c.command==VXH_STOP) {vxh_log.active=0; vx_rawcap_on=0; return 1;}
 if(c.command==VXH_READ&&out) {vx_rawcap_flush(); _fmemcpy(out,&vxh_log,sizeof(vxh_log)); return sizeof(vxh_log);}
 return 0;
}
/* v10.84: ExtTextOut diagnostic. Record 'TEXT' (rop field 0x54584554) per call while armed: x,y,w,h = the opaque rectangle (0 when absent), flags = which conditions held,
 * style = wOptions, hatch = the engine's answer (0xFFFF = no request sent), raw_fg = clip rectangle present, text_color/bk_color = draw mode colours. */
static void vxh_text(WORD cond,WORD opts,WORD rr,LPRECT opaque,LPRECT clip,LPDRAWMODE dm) {
 vxh_record *p;
 if(!(vxh_log.active&1)) return; /* v10.321: not with the blit capture (4) */
 if(vxh_log.count>=VXH_CAP) {vxh_log.dropped++; return;}
 p=&vxh_log.records[(WORD)vxh_log.count]; _fmemset(p,0,sizeof(*p));
 p->seq=vxh_log.count++; p->rop=0x54584554UL; p->bpp=wBpp; p->flags=cond; p->style=opts; p->hatch=rr; p->raw_fg=clip?1:0;
 if(opaque) {p->x=opaque->left; p->y=opaque->top; p->w=opaque->right-opaque->left; p->h=opaque->bottom-opaque->top;}
 if(dm) {p->dm_valid=1; p->bk_mode=(DWORD)(WORD)dm->bkMode; p->text_color=dm->TextColor; p->bk_color=dm->bkColor;}
}
static void vxh_capture(WORD x,WORD y,WORD w,WORD h,DWORD rop,LPBRUSH brush,LPDRAWMODE dm) {
 vxh_record *p; DIB_Brush32 __far *b=(DIB_Brush32 __far *)brush; DIB_Brush8 __far *b8=(DIB_Brush8 __far *)brush; int k;
 if(!(vxh_log.active&1)||!brush||((b->dp32BrushFlags&COLORSOLID)&&wBpp!=8)) return; /* v10.87: solid brushes are recorded at 8bpp (VX2AUTO v2.4: true solid fills never reached the engine there) */
 if(vxh_log.count>=VXH_CAP) {vxh_log.dropped++; return;}
 p=&vxh_log.records[(WORD)vxh_log.count]; _fmemset(p,0,sizeof(*p));
 p->seq=vxh_log.count++; p->rop=rop; p->bpp=wBpp; p->x=x; p->y=y; p->w=w; p->h=h;
 p->flags=b->dp32BrushFlags; p->brush_bpp=b->dp32BrushBpp; p->style=b->dp32BrushStyle; p->hatch=b->dp32Hatch; p->raw_fg=b->dp32FgColor; p->raw_bg=b->dp32BgColor;
 for(k=0;k<8;k++) {
  p->mono[k>>2]|=(DWORD)b->dp32BrushMono[k*4]<<((k&3)*8);
  p->mask[k>>2]|=(DWORD)b->dp32BrushMask[k*4]<<((k&3)*8);
  p->bits[k]=(wBpp==8)?(DWORD)b8->dp8BrushBits[k]:(wBpp==16)?(DWORD)((WORD __far *)b->dp32BrushBits)[k]:((DWORD __far *)b->dp32BrushBits)[k];
 }
 if(dm) {p->dm_valid=1; p->bk_mode=(DWORD)(WORD)dm->bkMode; p->text_color=dm->TextColor; p->bk_color=dm->bkColor;}
 if(wBpp==8) {for(k=0;k<64;k++) if(b8->dp8BrushBits[k]==b8->dp8BrushBits[0]) p->eq64++;}
}
static DWORD vx_lin(const void __far *p);
/* v10.321: blit capture (VXH_ARM_BLITS, vx_hatch_diag.h): where a memory bitmap or a DIB is put on the screen, and who drew it. Bounded (VXH_CAP records, then counted as dropped). */
static vxh_record __far *vxb_new(DWORD tag,int x,int y,WORD w,WORD h,WORD sx,WORD sy,DWORD rop,LPRECT clip,LPPDEVICE dst,WORD route,DWORD ret) {
 vxh_record __far *p;
 if(vxh_log.count>=VXH_CAP) {vxh_log.dropped++; return 0;}
 p=&vxh_log.records[(WORD)vxh_log.count]; _fmemset(p,0,sizeof(*p));
 p->seq=vxh_log.count++; p->rop=tag; p->bpp=wBpp; p->x=(DWORD)(long)x; p->y=(DWORD)(long)y; p->w=w; p->h=h; p->style=(DWORD)sx|((DWORD)sy<<16);
 p->hatch=route; p->text_color=ret; p->raw_fg=rop;
 if(clip) {p->dm_valid=1; p->mask[0]=(DWORD)(WORD)clip->left|((DWORD)(WORD)clip->top<<16); p->mask[1]=(DWORD)(WORD)clip->right|((DWORD)(WORD)clip->bottom<<16);}
 p->mono[0]=(DWORD)wScreenX|((DWORD)wScreenY<<16); p->mono[1]=hda?hda->pitch:0;
 if(dst) {p->bits[4]=((LPDIBENGINE)dst)->deFlags; p->bits[6]=dst==lpDriverPDevice;}
 return p;
}
static void vxb_mem(DWORD tag,LPDIBENGINE dst,int dx,int dy,WORD w,WORD h,LPPDEVICE src,WORD sx,WORD sy,DWORD rop,LPRECT clip,DWORD swh,WORD route,DWORD ret) {
 vxh_record __far *p; WORD typ;
 if(!(vxh_log.active&4)||!src||(LPVOID)src==(LPVOID)dst||(LPPDEVICE)dst!=lpDriverPDevice) return; /* (review) the screen only: GDI's own memory blits filled the slots */
 p=vxb_new(tag,dx,dy,w,h,sx,sy,rop,clip,(LPPDEVICE)dst,route,ret); if(!p) return;
 p->raw_bg=swh; typ=*(WORD __far *)src; p->flags=typ;
 if(typ==0) {BITMAP __far *bm=(BITMAP __far *)src; p->brush_bpp=bm->bmBitsPixel; p->bits[0]=(DWORD)bm->bmWidth; p->bits[1]=(DWORD)(long)bm->bmHeight; p->bits[2]=(DWORD)bm->bmWidthBytes; p->bits[3]=vx_lin(bm->bmBits);}
 else if(typ==TYPE_DIBENG) {LPDIBENGINE e=(LPDIBENGINE)src; p->flags|=(DWORD)e->deFlags<<16; p->brush_bpp=e->deBitsPixel; p->bits[0]=e->deWidth; p->bits[1]=e->deHeight; p->bits[2]=(DWORD)e->deDeltaScan;
  {DWORD b=DPMI_GetSegBase(e->deBitsSelector); p->bits[3]=(b&&b!=0xFFFFFFFFUL)?b+e->deBitsOffset:0;}}
}
static void vxb_dib(DWORD tag,LPPDEVICE dst,int dx,int dy,WORD w,WORD h,WORD sx,WORD sy,DWORD rop,LPRECT clip,LPVOID bits,LPBITMAPINFO info,DWORD swh,DWORD dwh,WORD route,DWORD ret) {
 vxh_record __far *p; LPBITMAPINFOHEADER ih=(LPBITMAPINFOHEADER)info;
 if(!(vxh_log.active&4)||dst!=lpDriverPDevice) return; /* (review) the screen only */
 p=vxb_new(tag,dx,dy,w,h,sx,sy,rop,clip,dst,route,ret); if(!p) return;
 p->raw_bg=swh; p->bits[7]=dwh; p->bits[3]=bits?vx_lin(bits):0;
 if(ih&&ih->biSize>=40) {p->brush_bpp=ih->biBitCount; p->flags=ih->biCompression|((long)ih->biHeight<0?0x80000000UL:0); /* .553: biHeight is a DWORD in this header: the top-down bit was never recorded (W124) */ p->bits[0]=(DWORD)ih->biWidth; p->bits[1]=(DWORD)ih->biHeight;
  p->bits[2]=(DWORD)((((DWORD)((long)ih->biWidth<0?0:ih->biWidth)*ih->biBitCount+31UL)/32UL)*4UL); p->bits[5]=ih->biClrUsed;}
}

/* Return 0 only when no command has fired: DIB fallback is then safe. */
static DWORD vx_brush_color(DWORD c) {
 if(wBpp==16) return c&0xFFFFUL; /* DIB_Brush16 physical RGB565 */
 return (c&0xFF00UL)|((c&255UL)<<16)|((c>>16)&255UL);
}
/* v10.84: which operands a ROP3 byte uses (ROP bit index = P<<2|S<<1|D) */
static int vx_rop_s(BYTE r) {int i; for(i=0;i<8;i++) if(((r>>i)&1)!=((r>>(i^2))&1)) return 1; return 0;}
static int vx_rop_p(BYTE r) {int i; for(i=0;i<8;i++) if(((r>>i)&1)!=((r>>(i^4))&1)) return 1; return 0;}
/* v10.84: the colour of a solid brush, for the fixed pattern colour of a copy ROP. 0 = not a plain solid brush. */
static DWORD vx_pc; /* DS is not SS in this driver: a near pointer must not point at a stack variable, so the colour goes through a static */
static int vx_solid(LPBRUSH brush) {
 DIB_Brush32 __far *b=(DIB_Brush32 __far *)brush; DIB_Brush8 __far *b8=(DIB_Brush8 __far *)brush; int k;
 if(!brush) return 0;
 if(!(b->dp32BrushFlags&COLORSOLID)) { /* v10.122: a brush whose 64 realized pixels are one colour is solid in effect (VX2GDI: a 16bpp solid brush was not flagged COLORSOLID); a dithered one is not */
  if(wBpp==8||b->dp32BrushBpp!=(wBpp==16?20:32)||b->dp32BrushStyle==BS_HATCHED||b->dp32BrushStyle==BS_HOLLOW||(b->dp32BrushFlags&PATTERNMONO)) return 0;
  if(wBpp==16) {WORD __far *pb=(WORD __far *)b->dp32BrushBits; for(k=1;k<64;k++) if(pb[k]!=pb[0]) return 0; vx_pc=pb[0]; return 1;}
  {DWORD __far *pb=(DWORD __far *)b->dp32BrushBits; for(k=1;k<64;k++) if(pb[k]!=pb[0]) return 0; vx_pc=pb[0]&0x00FFFFFFUL; return 1;}
 }
 if(wBpp==8) {BYTE c8=b8->dp8BrushBits[0]; for(k=1;k<4;k++) if(b8->dp8BrushBits[k]!=c8) return 0; vx_pc=c8; return 1;}
 if(b->dp32BrushBpp!=(wBpp==16?20:32)) return 0;
 if(wBpp==16) {WORD __far *pb=(WORD __far *)b->dp32BrushBits; if(pb[1]!=pb[0]) return 0; vx_pc=pb[0]; return 1;}
 vx_pc=vx_brush_color(b->dp32FgColor); return 1;
}
static WORD vx_try_blit(LPDIBENGINE dst,WORD dx,WORD dy,LPPDEVICE src,WORD sx,WORD sy,WORD w,WORD h,DWORD rop,LPBRUSH brush,LPDRAWMODE dm,LPRECT clip);
RECT vx_empty_rect; /* all zero; not static so the map shows it */
/* v10.116: GDI drawing from system memory through the VxD (OP_VX_XFER, vx855_2d.h): memory bitmap -> screen BitBlt, text, scan line spans.
 * vx_host_mask is the VxD's live ACCEL_HOST mask (the VxD writes it here on every call and when a test changes it), so work the VxD would refuse is not prepared. */
WORD vx_host_mask=30; /* v10.130: bit 32 = scan bands with ROPs that only write (R2_COPYPEN and friends); v10.131: off (VX2WIDE: pixel exact but 3 to 7 times slower than DIBENG) */ /* the VxD default (ACCEL_HOST); the VxD writes its live value here */
static vx2d_xfer vx_xq;
static DWORD vx_lin(const void __far *p) {DWORD b=DPMI_GetSegBase((WORD)(((DWORD)p)>>16)); if(!b||b==0xFFFFFFFFUL) return 0; return b+(((DWORD)p)&0xFFFFUL);}
static void vx_xq_init(WORD op) {
 _fmemset(&vx_xq,0,sizeof(vx_xq)); vx_xq.cb=sizeof(vx_xq); vx_xq.abi=VX2D_ABI; vx_xq.op=op; vx_xq.bpp=wBpp; vx_xq.mask_lin=vx_lin(&vx_host_mask);
 vx_xq.mask_out=0xFFFFFFFFUL; /* v10.330 (review #139): 'not written': a call that never reached the VxD (no VM, no mapping) left 0 here, and the live mask became 0 */
}
static WORD vx_xq_run(void) {WORD r=VX_xfer_submit(&vx_xq); if(vx_xq.mask_out<64) vx_host_mask=(WORD)vx_xq.mask_out; return r;}
static int vx_rop_d(BYTE r) {return ((r^(r>>1))&0x55)!=0;}
/* v10.142 (review O14): 1 when the VxD would decline this request for its size alone (its thresholds, published in hda->res0..3 / rem2), so the trip into the
 * VxD is skipped and DIBENG draws it at once. Same rule as VX2D_submit: force, dashed lines and (with the bit set) screen-reading ROPs ignore the thresholds; fills at least
 * rem2 rows high go to the engine anyway. The VxD only ever declines more than this (its clipping can only shrink the area). No 'VX' tag: no opinion. */
static int vx_pre_declined(const vx2d_request *q) {
 DWORD p=hda->res0,area,min;
 if((p>>16)!=0x5658UL||(p&1UL)) return 0;
 if((p&2UL)&&q->op!=VX2D_LINE&&vx_rop_d((BYTE)q->rop)) return 0;
 if(q->op!=VX2D_COPY&&q->op!=VX2D_LINE&&hda->rem2&&q->height>=hda->rem2) return 0;
 area=q->width*q->height; min=q->op==VX2D_COPY?hda->res3:q->op==VX2D_LINE?hda->res2:hda->res1;
 return area<min;
}
static int vx_line_declined(int len) {DWORD p=hda->res0; return (p>>16)==0x5658UL&&!(p&1UL)&&(DWORD)len<hda->res2;}
#define VX2D_BUILT 9 /* vx_try_blit with vx_build_only: the request is in vx_built, nothing was sent */
static int vx_build_only; static vx2d_request vx_built;
/* BitBlt / StretchBlt (1:1) from a memory bitmap. A 1-bit GDI bitmap: 0 bits take the draw mode's text colour, 1 bits its background colour (both physical); SRCCOPY goes through the host window,
 * any ROP that reads the screen through the staging block. A bitmap in the screen's own format (a DIB engine bitmap or DIB section): ROPs that read the screen through the staging block (DIBENG would read
 * video memory at about 30 MB/s); SRCCOPY and the other ROPs that do not read the screen stay with DIBENG, which writes as fast as the CPU can. */
/* .595 logging (capture escape VXH_ARM): why a blit from a memory bitmap whose ROP uses the pattern was left to DIBENG. One record,
 * tag 'MBSK': style = which exit of vx_try_memblit (counted from its top), flags / brush_bpp / hatch = the brush's flags, depth byte
 * and style, raw_fg = the source's type word, raw_bg = its deFlags (DIB engine bitmaps), bits[0] = the ROP, bits[1] = the answer of
 * the PATCOPY build (0xFFFF not tried), bits[2] = the built request's kind. */
static LPBRUSH vx_mb_brush; static LPPDEVICE vx_mb_src; static DWORD vx_mb_rop; static WORD vx_mb_rr=0xFFFF,vx_mb_op;
static WORD vx_mb_skip(WORD n) {
 vxh_record *p; DIB_Brush32 __far *b=(DIB_Brush32 __far *)vx_mb_brush; DIB_Brush8 __far *b8=(DIB_Brush8 __far *)vx_mb_brush;
 if(!(vxh_log.active&1)||!vx_rop_p((BYTE)(vx_mb_rop>>16))||(vx_mb_rop>>24)) return VX2D_SKIP;
 if(vxh_log.count>=VXH_CAP) {vxh_log.dropped++; return VX2D_SKIP;}
 p=&vxh_log.records[(WORD)vxh_log.count]; _fmemset(p,0,sizeof(*p));
 p->seq=vxh_log.count++; p->rop=0x4B53424DUL; p->bpp=wBpp; p->style=n;
 if(b) {if(wBpp==8) {p->flags=b8->dp8BrushFlags; p->brush_bpp=b8->dp8BrushBpp; p->hatch=b8->dp8BrushStyle;} else {p->flags=b->dp32BrushFlags; p->brush_bpp=b->dp32BrushBpp; p->hatch=b->dp32BrushStyle;}} else p->hatch=0xFFFFUL;
 if(vx_mb_src) {p->raw_fg=*(WORD __far *)vx_mb_src; if(p->raw_fg==TYPE_DIBENG) p->raw_bg=((LPDIBENGINE)vx_mb_src)->deFlags;}
 p->bits[0]=vx_mb_rop; p->bits[1]=vx_mb_rr; p->bits[2]=vx_mb_op;
 return VX2D_SKIP;
}
/* .595: a brush that is not one colour (a dithered colour, a brush made from a colour bitmap) as the pattern of a staged blit: its 64
 * realized pixels go as the pattern (VX2D_XF_COLPAT), taken the way a PATCOPY fill of the same rectangle takes them. Hatches, 1-bit
 * bitmap brushes (their colours come from the draw mode, not measured with these ROPs) and hollow brushes stay with DIBENG.
 * .597: one routine for BitBlt's path and StretchDIBits' (VX800 board, VX2GDI v2.6: a blit from a DIB section arrives as StretchDIBits,
 * whose path still asked for a solid brush). vx_xq must be set up already. Returns 0 when the pattern is in, else 1 no brush, 2 a
 * brush kind that stays with DIBENG, 3 the PATCOPY build refused it. */
static WORD vx_colpat(LPDIBENGINE dst,int l,int t,WORD w,WORD h,LPBRUSH brush,LPDRAWMODE dm) {
 DIB_Brush32 __far *pb=(DIB_Brush32 __far *)brush; DIB_Brush8 __far *pb8=(DIB_Brush8 __far *)brush; WORD rr,fl,st;
 if(!brush) return 1;
 fl=wBpp==8?(WORD)pb8->dp8BrushFlags:(WORD)pb->dp32BrushFlags; st=wBpp==8?(WORD)pb8->dp8BrushStyle:(WORD)pb->dp32BrushStyle;
 if((fl&(COLORSOLID|PATTERNMONO))||st==BS_HATCHED||st==BS_HOLLOW) return 2;
 vx_build_only=1; rr=vx_try_blit(dst,(WORD)l,(WORD)t,NULL,0,0,w,h,0x00F00000UL,brush,dm,NULL); vx_build_only=0; vx_mb_rr=rr; vx_mb_op=(WORD)vx_built.op;
 if(rr!=VX2D_BUILT||(vx_built.op!=VX2D_COLOR&&vx_built.op!=VX2D_MONO)) return 3;
 _fmemcpy(&vx_xq.tpl,&vx_built,sizeof(vx_built)); vx_xq.flags|=VX2D_XF_COLPAT;
 return 0;
}
static WORD vx_try_memblit(LPDIBENGINE dst,WORD dx,WORD dy,LPPDEVICE src,WORD sx,WORD sy,WORD w,WORD h,DWORD rop,LPBRUSH brush,LPDRAWMODE dm,LPRECT clip) {
 BYTE r3=(BYTE)(rop>>16); WORD typ,W,H,bpp,sel; DWORD off,base,mask; long stride; int mono,l,t,r,b;
 vx_mb_brush=brush; vx_mb_src=src; vx_mb_rop=rop; vx_mb_rr=0xFFFF; vx_mb_op=0;
 if(!(vx_host_mask&7)||!src||(LPVOID)src==(LPVOID)dst) return vx_mb_skip(1);
 if(!wEnabled||dst!=lpDriverPDevice||(wBpp!=8&&wBpp!=16&&wBpp!=32)||!w||!h) return vx_mb_skip(2);
 if((short)w<=0||(short)h<=0) return vx_mb_skip(3); /* v10.302 (review): a negative (mirrored) extent made r < l, and with a clip DONE drew nothing: DIBENG's */
 if((rop>>24)!=0||!vx_rop_s(r3)) return vx_mb_skip(4);
 typ=*(WORD __far *)src;
 if(typ==0) { /* a GDI bitmap: only the 1-bit kind (one segment) */
  BITMAP __far *bm=(BITMAP __far *)src;
  if(bm->bmPlanes!=1||bm->bmBitsPixel!=1||bm->bmSegmentIndex||!bm->bmBits) return vx_mb_skip(5);
  bpp=1; W=(WORD)bm->bmWidth; H=(WORD)bm->bmHeight; stride=(long)bm->bmWidthBytes; sel=(WORD)(((DWORD)bm->bmBits)>>16); off=((DWORD)bm->bmBits)&0xFFFFUL;
  if(off+(DWORD)stride*H>65536UL) return vx_mb_skip(6);
 } else if(typ==TYPE_DIBENG) {
  LPDIBENGINE e=(LPDIBENGINE)src;
  if(e->deFlags&(VRAM|OFFSCREEN|BUSY|BANKEDVRAM|BANKEDSCAN|NOT_FRAMEBUFFER)) return vx_mb_skip(7);
  if(e->dePlanes!=1) return vx_mb_skip(8);
  bpp=e->deBitsPixel; W=e->deWidth; H=e->deHeight; stride=(long)e->deDeltaScan; sel=e->deBitsSelector; off=e->deBitsOffset;
  if(bpp==1) return vx_mb_skip(9); /* a 1-bit DIB engine bitmap takes its colours from its own table: not measured */
  if(bpp!=wBpp) return vx_mb_skip(10);
  if(wBpp==16&&((e->deFlags^lpDriverPDevice->deFlags)&FIVE6FIVE)) return vx_mb_skip(11);
  if(wBpp==8&&(e->deFlags&SELECTEDDIB)) return vx_mb_skip(12); /* an 8-bit DIB section's indices go through its colour table */
 } else return vx_mb_skip(13);
 mono=bpp==1;
 if(mono&&!dm) return vx_mb_skip(14);
 if(!(mono?(r3==0xCC?(vx_host_mask&1):(vx_host_mask&2)):(vx_host_mask&4))) return vx_mb_skip(15);
 if(!(mono&&r3==0xCC)&&!vx_rop_d(r3)) return vx_mb_skip(16);
 if(mono&&r3==0xCC&&wBpp==8) return vx_mb_skip(17);
 /* the visible rectangle: destination, clip, screen; the source moves with it */
 l=(int)(short)dx; t=(int)(short)dy;
 {long lr=(long)l+(long)w,lb=(long)t+(long)h;r=lr>32767L?32767:(int)lr;b=lb>32767L?32767:(int)lb;} /* v10.309 (review): 16-bit int: dx + w past 32767 wrapped negative and a clipped copy answered DONE drawing nothing */
 if(clip) {if(clip->left>l) l=clip->left; if(clip->top>t) t=clip->top; if(clip->right<r) r=clip->right; if(clip->bottom<b) b=clip->bottom;}
 if(l<0) l=0; if(t<0) t=0; if(r>(int)wScreenX) r=(int)wScreenX; if(b>(int)wScreenY) b=(int)wScreenY;
 if(r<=l||b<=t) return clip?VX2D_DONE:VX2D_SKIP;
 sx=(WORD)(sx+(l-(int)(short)dx)); sy=(WORD)(sy+(t-(int)(short)dy)); w=(WORD)(r-l); h=(WORD)(b-t);
 if(sx>=W||sy>=H||w>W-sx||h>H-sy) return vx_mb_skip(18);
 base=DPMI_GetSegBase(sel); if(!base||base==0xFFFFFFFFUL) return vx_mb_skip(19);
 mask=wBpp==8?0xFFUL:wBpp==16?0xFFFFUL:0xFFFFFFFFUL;
 vx_xq_init((mono&&r3==0xCC)?VX2D_XF_MONO:VX2D_XF_STAGE);
 vx_xq.rop=r3; vx_xq.dx=(DWORD)l; vx_xq.dy=(DWORD)t; vx_xq.w=w; vx_xq.h=h;
 vx_xq.src_lin=base+off; vx_xq.src_bits=bpp; vx_xq.src_x=sx; vx_xq.src_y=sy; vx_xq.src_w=W; vx_xq.src_h=H; vx_xq.src_stride=stride;
 if(mono) {vx_xq.fg=dm->bkColor&mask; vx_xq.bg=dm->TextColor&mask;} /* 1 bits = background colour, 0 bits = text colour */
 if(vx_rop_p(r3)) {
  if(vx_solid(brush)) {vx_xq.flags|=VX2D_XF_FIXPAT; vx_xq.pfg=vx_pc;}
  else {WORD cw=vx_colpat((LPDIBENGINE)dst,l,t,w,h,brush,dm); if(cw) return vx_mb_skip(19+cw);}
 }
 return vx_xq_run();
}
/* v10.116: scan lines (Output OS_SCANLINES): lpPoints[0].ycoord is the line, the other points are segments (xcoord = first pixel, ycoord = one past the last), filled with the brush and the draw mode's
 * ROP2. Only ROPs that read the screen go to the engine (DIBENG writes a plain fill as fast as the CPU can); one VxD call for the whole line. */
/* 1 drawn, 2 fired and failed (not replayed), 0 nothing drawn */
static int vx_spans(LPDIBENGINE dev,WORD wCount,LPPOINT lpPoints,LPPBRUSH lpPBrush,LPDRAWMODE dm,LPRECT clip) {
 PTTYPE __far *pt=(PTTYPE __far *)lpPoints; int rop2,i,y,cl=0,ct=0,cr=(int)wScreenX,cb=(int)wScreenY,a,b; WORD n=0,rr; BYTE r3=0;
 if(!(vx_host_mask&16)||!wEnabled||dev!=lpDriverPDevice||!(dev->deFlags&VRAM)||(dev->deFlags&BUSY)||(wBpp!=8&&wBpp!=16&&wBpp!=32)) return 0;
 if(wCount<2||wCount-1>VX2D_XF_SPANS_MAX||!pt||!lpPBrush||!dm) return 0;
 rop2=dm->Rop2; if(rop2<1||rop2>16) return 0;
 for(i=0;i<8;i++) {int P=(i>>2)&1,D=i&1; if(((rop2-1)>>(P*2+D))&1) r3|=(BYTE)(1<<i);} /* ROP2 (P, D) as a ROP3 (P, S, D) that ignores S */
 if(!vx_rop_d(r3)||r3==0xAA) return 0;
 if(clip) {if(clip->left>cl) cl=clip->left; if(clip->top>ct) ct=clip->top; if(clip->right<cr) cr=clip->right; if(clip->bottom<cb) cb=clip->bottom;}
 y=pt[0].ycoord; if(y<ct||y>=cb||cr<=cl) return 0;
 vx_xq_init(VX2D_XF_SPANS);
 for(i=1;i<(int)wCount;i++) {
  a=pt[i].xcoord; b=pt[i].ycoord; if(a<cl) a=cl; if(b>cr) b=cr;
  if(a<b) {vx_xq.sp[n*3]=(WORD)y; vx_xq.sp[n*3+1]=(WORD)a; vx_xq.sp[n*3+2]=(WORD)b; n++;}
 }
 if(!n) return 0;
 vx_build_only=1; rr=vx_try_blit(dev,vx_xq.sp[1],(WORD)y,NULL,0,0,(WORD)(vx_xq.sp[2]-vx_xq.sp[1]),1,(DWORD)r3<<16,lpPBrush,dm,NULL); vx_build_only=0;
 if(rr!=VX2D_BUILT) return 0;
 _fmemcpy(&vx_xq.tpl,&vx_built,sizeof(vx_built)); vx_xq.n=n;
 rr=vx_xq_run();
 if(rr==VX2D_LOST) {dev->deFlags|=BUSY; wEnabled=0; return 2;} /* v10.330 (review #131): lost is not handed to DIBENG either (0 sent the whole call to DIB_Output) */
 return rr==VX2D_DONE?1:rr==VX2D_FAILED?2:0; /* 2: fired and failed, never replayed */
}
/* v10.118: Output OS_POLYSCANLINE (VX2GDI v1.1: filled shapes and wide pens arrive this way, between OS_BEGINNSCAN and OS_ENDNSCAN). lpPoints is a list of wCount GDI region scans:
 * WORD count, top, bottom (exclusive), count x values (pairs: first pixel, one past the last), count again. Each pair is a rectangle; the brush and the draw mode's ROP2 fill it. ROPs that read the screen only. */
static int vx_polyscan(LPDIBENGINE dev,WORD wCount,LPPOINT lpPoints,LPPBRUSH lpPBrush,LPDRAWMODE dm,LPRECT clip) {
 WORD __far *p=(WORD __far *)lpPoints; int rop2,i,cl=0,ct=0,cr=(int)wScreenX,cb=(int)wScreenY; WORD n=0,rr,s,k; BYTE r3=0;
 if(!(vx_host_mask&16)) return 0;
 if(!wEnabled||dev!=lpDriverPDevice||!(dev->deFlags&VRAM)||(dev->deFlags&BUSY)||(wBpp!=8&&wBpp!=16&&wBpp!=32)||!wCount||!p||!lpPBrush||!dm) return 0;
 rop2=dm->Rop2; if(rop2<1||rop2>16) return 0;
 for(i=0;i<8;i++) {int P=(i>>2)&1,D=i&1; if(((rop2-1)>>(P*2+D))&1) r3|=(BYTE)(1<<i);}
 if(!vx_rop_d(r3)&&!(vx_host_mask&32)) return 0; /* v10.130: write-only ROP2s (COPYPEN, NOTCOPYPEN, BLACK, WHITE) only with ACCEL_HOST bit 32 */
 if(r3==0xAA) return 1;
 if(clip) {if(clip->left>cl) cl=clip->left; if(clip->top>ct) ct=clip->top; if(clip->right<cr) cr=clip->right; if(clip->bottom<cb) cb=clip->bottom;}
 if(cr<=cl||cb<=ct) return 0;
 vx_xq_init(VX2D_XF_SPANS); vx_xq.flags=VX2D_XF_RECTS;
 for(s=0;s<wCount;s++) {
  WORD cnt=p[0]; int top=(int)(short)p[1],bot=(int)(short)p[2];
  if((cnt&1)||cnt>192||p[3+cnt]!=cnt) return 0; /* not the scan layout we know */
  if(top<ct) top=ct; if(bot>cb) bot=cb;
  for(k=0;k<cnt&&top<bot;k+=2) {
   int a=(int)(short)p[3+k],b=(int)(short)p[4+k]; if(a<cl) a=cl; if(b>cr) b=cr;
   if(a>=b) continue;
   if(n>=96) return 0;
   vx_xq.sp[n*4]=(WORD)top; vx_xq.sp[n*4+1]=(WORD)a; vx_xq.sp[n*4+2]=(WORD)b; vx_xq.sp[n*4+3]=(WORD)(bot-top); n++;
  }
  p+=4+cnt;
 }
 if(!n) return 1; /* everything clipped away: nothing to draw */
 vx_build_only=1; rr=vx_try_blit(dev,vx_xq.sp[1],vx_xq.sp[0],NULL,0,0,(WORD)(vx_xq.sp[2]-vx_xq.sp[1]),vx_xq.sp[3],(DWORD)r3<<16,lpPBrush,dm,NULL); vx_build_only=0;
 if(rr!=VX2D_BUILT) return 0;
 _fmemcpy(&vx_xq.tpl,&vx_built,sizeof(vx_built)); vx_xq.n=n;
 rr=vx_xq_run();
 if(rr==VX2D_LOST) {dev->deFlags|=BUSY; wEnabled=0; return 2;} /* v10.330 (review #131) */
 return rr==VX2D_DONE?1:rr==VX2D_FAILED?2:0;
}
static WORD vx_try_blit(LPDIBENGINE dst,WORD dx,WORD dy,LPPDEVICE src,
 WORD sx,WORD sy,WORD w,WORD h,DWORD rop,LPBRUSH brush,LPDRAWMODE dm,LPRECT clip) {
 static vx2d_request q;
 DIB_Brush32 __far *b=(DIB_Brush32 __far *)brush;
 WORD x,y,rr; DWORD c; BYTE row; int pattern=0;
 if(!wEnabled||dst!=lpDriverPDevice||(wBpp!=8&&wBpp!=16&&wBpp!=32)||!w||!h) return VX2D_SKIP;
 if(dx>=wScreenX||dy>=wScreenY||w>wScreenX-dx||h>wScreenY-dy) return VX2D_SKIP;
 _fmemset(&q,0,sizeof(q)); q.cb=sizeof(q); q.abi=VX2D_ABI; q.bpp=wBpp;
 q.dst.pitch=hda->pitch; q.dst.width=wScreenX; q.dst.height=wScreenY; q.src=q.dst;
 q.dx=dx; q.dy=dy; q.sx=sx; q.sy=sy; q.width=w; q.height=h;
 if(clip) {
  int l=clip->left,t=clip->top,r=clip->right,bot=clip->bottom;
  if(l<0) l=0; if(t<0) t=0; if(r>(int)wScreenX) r=(int)wScreenX; if(bot>(int)wScreenY) bot=(int)wScreenY; /* v10.302 (review): int against WORD compared unsigned: a negative right or bottom became the screen's edge */
  if(r<=l||bot<=t) return VX2D_DONE;
  q.flags|=VX2D_CLIP; q.left=l; q.top=t; q.right=r; q.bottom=bot;
 }
 if(rop==VX_ROP_SRCCOPY) {
  if((LPVOID)src!=(LPVOID)dst) return VX2D_SKIP;
  q.op=VX2D_COPY; q.rop=0xCC;
 } else if((rop>>24)==0&&vx_rop_s((BYTE)(rop>>16))) {
  /* v10.84 (VX2ROP, hardware: the engine does every ROP3 exactly): a ROP that uses the source, from the screen itself. One that also uses the pattern takes it as a solid colour only. */
  BYTE r3=(BYTE)(rop>>16);
  if((LPVOID)src!=(LPVOID)dst) return VX2D_SKIP;
  if(vx_rop_p(r3)) {if(!vx_solid(brush)) return VX2D_SKIP; q.flags|=VX2D_FIXPAT; q.fg=vx_pc; q.bg=vx_pc;}
  q.op=VX2D_COPY; q.rop=r3;
 } else {
  q.op=VX2D_FILL;
  switch(rop) {
   case 0x00000042UL:q.rop=0;break;
   case 0x00FF0062UL:q.rop=255;break;
   case 0x00550009UL:q.rop=0x55;break;
   case 0x00F00021UL:q.rop=0xF0;pattern=1;break;
   case 0x005A0049UL:q.rop=0x5A;pattern=1;break; /* v10.67f: the 16bpp failure was the raw brush colour (2070 instead of 68EC), not the ROP */
   default:
    /* v10.84: any other ROP that does not use the source (pattern ROPs and the fixed ones): the pattern machinery below, with the ROP as it is */
    if((rop>>24)!=0) return VX2D_SKIP;
    q.rop=(BYTE)(rop>>16);
    if(q.rop==0xAA) return VX2D_DONE; /* destination unchanged */
    if(vx_rop_p(q.rop)) pattern=1;
    break;
  }
  if(pattern) vxh_capture(dx,dy,w,h,rop,brush,dm);
  /* v10.84: hatches and 1-bit bitmap brushes draw with their draw mode colours; DIBENG's treatment of those with ROPs other than PATCOPY/PATINVERT is not measured, so they stay with DIBENG for the other ROPs. Solid and colour-pattern brushes take every source-free ROP (VX2ROP T1/T3). */
  if(pattern&&q.rop!=0xF0&&q.rop!=0x5A&&brush&&!(b->dp32BrushFlags&COLORSOLID)&&(b->dp32BrushStyle==BS_HATCHED||(b->dp32BrushFlags&PATTERNMONO))) return VX2D_SKIP;
  if(pattern&&wBpp==8) {
   /* v10.76: at 8bpp only a solid brush is sent. Like the 16bpp solid brush its dp8FgColor is the raw COLORREF, but the 64 brush bytes hold the
    * index DIBENG really draws, so all 64 must be equal and that byte is the colour (patterns, hatches and dithered colours stay with DIBENG). */
   DIB_Brush8 __far *b8=(DIB_Brush8 __far *)brush; BYTE c8; int k;
   if(!brush) return VX2D_SKIP;
   if(!(b8->dp8BrushFlags&COLORSOLID)) {
    /* v10.77: a patterned or dithered brush (not a mono bitmap brush, whose colours come from the draw mode; not a hatch): the engine fills from the 8x8 block of
     * realized indices (VX2COL measured the colour pattern fill at 8bpp). The brush bits are laid out for the screen, so the VxD anchors the block to the screen. */
    /* v10.79: a hatch (VX2AUTO v1.5 capture: flags 90 = MASKVALID, the 64 brush bytes are the final pixels, hatch colour on the lines and the draw mode's background colour elsewhere, screen-anchored with the brush origin applied) goes the same way; a hollow brush never does */
    if(b8->dp8BrushStyle==BS_HOLLOW||(b8->dp8BrushStyle==BS_HATCHED&&!(b8->dp8BrushFlags&MASKVALID))) return VX2D_SKIP;
    if((b8->dp8BrushFlags&(PATTERNMONO|MONOVALID))==(PATTERNMONO|MONOVALID)) {
     /* v10.83: a brush made from a 1-bit bitmap at 8bpp, opaque only (same rules as the 16/32bpp mono route, v10.60): dp8BrushMono holds the 8 rows (each row byte repeated 4 times, most significant bit =
      * leftmost pixel); 0 bits show the draw mode's text colour and 1 bits its background colour. The engine fills from a colour block of those two indices (the 8bpp mono pattern is not used). */
     int yy,xx;
     if(!dm) return VX2D_SKIP; /* v10.89: TRANSPARENT mode takes the same fill (VX2AUTO v2.7 classifies what DIBENG does with it) */
     for(yy=0;yy<8;yy++) for(xx=0;xx<8;xx++) q.pattern[yy*8+xx]=(b8->dp8BrushMono[yy*4]&(BYTE)(0x80>>xx))?(DWORD)(dm->bkColor&0xFFUL):(DWORD)(dm->TextColor&0xFFUL);
     q.op=VX2D_COLOR; q.fg=q.pattern[0]; q.bg=q.fg;
     if(vx_build_only) {_fmemcpy(&vx_built,&q,sizeof(q)); return VX2D_BUILT;}
     if(vx_pre_declined(&q)) return VX2D_SKIP; /* v10.142 */
     rr=VX_accel_submit(&q); return rr;
    }
    if(b8->dp8BrushFlags&PATTERNMONO) return VX2D_SKIP;
    /* v10.82: the draw mode's background mode is not looked at for a hatch: VX2AUTO v1.7 showed DIBENG paints the background colour (the realized pixels) in TRANSPARENT mode too, so matching it means the same fill as opaque */
    q.op=VX2D_COLOR; for(k=0;k<64;k++) q.pattern[k]=b8->dp8BrushBits[k];
    q.fg=q.pattern[0]; q.bg=q.fg;
   } else {
    /* v10.87 (VX2AUTO v2.5 capture): DIBENG fills only the first 4 bytes of a solid 8bpp brush (the colour index four times); the other 60 are stale */
   c8=b8->dp8BrushBits[0]; for(k=1;k<4;k++) if(b8->dp8BrushBits[k]!=c8) return VX2D_SKIP;
    q.fg=c8; q.bg=c8;
   }
  } else
  if(pattern) {
   if(!brush||b->dp32BrushBpp!=(wBpp==16?20:32)) return VX2D_SKIP; /* v10.67e: measured with the brush trace: a brush realized at 16bpp has depth byte 20 (not 16), at 32bpp 32 */
   q.fg=vx_brush_color(b->dp32FgColor); q.bg=vx_brush_color(b->dp32BgColor);
   if(wBpp==16&&(b->dp32BrushFlags&COLORSOLID)) {WORD __far *pb=(WORD __far *)b->dp32BrushBits; int k; for(k=1;k<2;k++) if(pb[k]!=pb[0]) return VX2D_SKIP; q.fg=pb[0]; q.bg=q.fg;} /* v10.67f: a solid brush keeps the raw COLORREF in dp32FgColor, but its BrushBits hold the converted 16-bit colour (VX2PROOF: equals DIB_ColorInfo and the pixel DIBENG draws, 32 of 32 colours). v10.66 sent the raw low word and the desktop turned red/maroon */
   if((b->dp32BrushFlags&(PATTERNMONO|MONOVALID))==(PATTERNMONO|MONOVALID)&&!(b->dp32BrushFlags&COLORSOLID)) {
    /* v10.60 (measured with VX2SYNC): a brush made from a 1-bit bitmap keeps its pattern in dp32BrushMono (8 rows, each row byte repeated
     * 4 times, most significant bit = leftmost pixel) and does not fill the colour bits; its colours come from the draw mode: 0 bits show
     * the text colour and 1 bits the background colour. Opaque draws only. */
    if(!dm) return VX2D_SKIP; /* v10.89: opaque-like in TRANSPARENT mode too, to be confirmed by VX2AUTO v2.7 against DIBENG */
    q.op=VX2D_MONO; q.fg=dm->TextColor&(wBpp==16?0xFFFFUL:0xFFFFFFFFUL); q.bg=dm->bkColor&(wBpp==16?0xFFFFUL:0xFFFFFFFFUL); /* v10.84: 32bpp keeps the whole dword (the alpha byte DIBENG writes too, VX2AUTO v1.9) */ /* measured: the draw mode's physical colours are already in the engine's order (no red/blue swap, unlike dp32FgColor) */
    for(y=0;y<8;y++) {
     BYTE mb=(BYTE)~b->dp32BrushMono[y*4]; row=0;
     for(x=0;x<8;x++) if(mb&(BYTE)(0x80>>x)) row|=(BYTE)(1<<x);
     q.mono[y/4]|=(DWORD)row<<((3-(y&3))*8);
    }
   } else if(!(b->dp32BrushFlags&COLORSOLID)) {
    /* DIB colour pixels retain realization/brush-origin phase. Convert an
     * opaque 8x8 colour brush to two colours when possible; this avoids
     * undocumented MONO/MASK validity and polarity rules. */
    if(b->dp32BrushStyle==BS_HOLLOW||(b->dp32BrushStyle==BS_HATCHED&&!(b->dp32BrushFlags&MASKVALID))) return VX2D_SKIP; /* v10.79: hatches with a valid mask use their realized pixels (see the 8bpp branch) */
    for(y=0;y<8;y++) for(x=0;x<8;x++) {
     /* The engine anchors patterns to absolute screen coordinates (cookbook
      * 11.x), so store the brush as realized: no dx,dy pre-shift. */
     WORD index=(WORD)(y*8+x);
     if(wBpp==16) c=((WORD __far *)b->dp32BrushBits)[index];
     else c=((DWORD __far *)b->dp32BrushBits)[index];
     q.pattern[y*8+x]=c;
    }
    q.fg=q.pattern[0]; q.bg=q.fg;
    for(x=0;x<64;x++) if(q.pattern[x]!=q.fg) {q.bg=q.pattern[x];break;}
    for(x=0;x<64;x++) if(q.pattern[x]!=q.fg&&q.pattern[x]!=q.bg) break;
    if(x==64) {
     q.op=VX2D_MONO;
     for(y=0;y<8;y++) {
      row=0; for(x=0;x<8;x++) if(q.pattern[y*8+x]==q.fg) row|=(BYTE)(1<<x);
      q.mono[y/4]|=(DWORD)row<<((3-(y&3))*8);
     }
    } else {
     /* v10.77: three or more colours. The pattern RAM keeps hidden state between commands (VX2PAT), so the VxD now writes the 8x8 block into video memory and the
      * engine fills from there (VX2COL: 32, 16 and 8 bits per pixel, anchored to the rectangle corner plus the offset bits). pattern[] already holds the realized pixels. */
     q.op=VX2D_COLOR;
    }
   }
  }
 }
 (void)dm; /* transparent hatch masks stay with the DIB engine */
 if(vx_build_only) {_fmemcpy(&vx_built,&q,sizeof(q)); return VX2D_BUILT;}
 if(vx_pre_declined(&q)) return VX2D_SKIP; /* v10.142 */
 rr=VX_accel_submit(&q); return rr;
}
static BOOL vx_result(WORD r,LPDIBENGINE dst) {
 if(r==VX2D_LOST) {dst->deFlags|=BUSY; wEnabled=0;}
 return r==VX2D_DONE;
}
/* v10.124: SaveScreenBitmap (USER saves the screen under a menu or another CS_SAVEBITS window): 0 SSB_SAVE, 1 SSB_RESTORE, 2 SSB_DISCARD. The VxD copies the rectangle into a save block in video memory
 * with the engine and back. 0 = not done: USER then does without (it saves the bits itself or redraws). */
static vx2d_xfer vx_ssq;
WORD WINAPI __loadds SaveScreenBitmap(LPRECT lpRect,WORD wCommand) {
 int l,t,r,b; WORD rr;
 if(wCommand>2||!wEnabled||!lpDriverPDevice||(lpDriverPDevice->deFlags&BUSY)) return 0;
 _fmemset(&vx_ssq,0,sizeof(vx_ssq)); vx_ssq.cb=sizeof(vx_ssq); vx_ssq.abi=VX2D_ABI; vx_ssq.op=VX2D_XF_SAVE; vx_ssq.bpp=wBpp; vx_ssq.mask_lin=vx_lin(&vx_host_mask); vx_ssq.mask_out=0xFFFFFFFFUL; /* v10.330 (review #139) */
 if(wCommand==2) {vx_ssq.flags=4; rr=VX_xfer_submit(&vx_ssq); if(rr==VX2D_LOST) {lpDriverPDevice->deFlags|=BUSY; wEnabled=0; return 0;} return rr==VX2D_DONE?1:0;} /* v10.330 (review #134): a lost engine is recorded here too, as for save and restore */
 if(!lpRect||!(vx_host_mask&8)) return 0;
 l=lpRect->left; t=lpRect->top; r=lpRect->right; b=lpRect->bottom;
 if(l<0) l=0; if(t<0) t=0; if(r>(int)wScreenX) r=(int)wScreenX; if(b>(int)wScreenY) b=(int)wScreenY;
 if(r<=l||b<=t) return 0;
 vx_ssq.flags=wCommand==0?1:2; vx_ssq.dx=(DWORD)l; vx_ssq.dy=(DWORD)t; vx_ssq.w=(DWORD)(r-l); vx_ssq.h=(DWORD)(b-t);
 rr=VX_xfer_submit(&vx_ssq);
 if(vx_ssq.mask_out<64) vx_host_mask=(WORD)vx_ssq.mask_out;
 if(rr==VX2D_LOST) {lpDriverPDevice->deFlags|=BUSY; wEnabled=0; return 0;}
 return rr==VX2D_DONE?1:0;
}
BOOL WINAPI __loadds BitBlt(LPDIBENGINE dst,WORD dx,WORD dy,LPPDEVICE src,
 WORD sx,WORD sy,WORD w,WORD h,DWORD rop,LPBRUSH brush,LPDRAWMODE dm) {
 WORD r,tried=0xFFFF; BOOL ok;
 if((dst->deFlags&VRAM)&&!(dst->deFlags&BUSY)) {
  if(src&&(LPVOID)src!=(LPVOID)dst) {r=vx_try_memblit(dst,dx,dy,src,sx,sy,w,h,rop,brush,dm,NULL); tried=r; if(r!=VX2D_SKIP) {vxb_mem(VXH_TAG_BBLT,dst,(short)dx,(short)dy,w,h,src,sx,sy,rop,NULL,0,r,0xFFFFFFFFUL); return vx_result(r,dst);}} /* v10.116 */
  r=vx_try_blit(dst,dx,dy,src,sx,sy,w,h,rop,brush,dm,NULL);
  if(r!=VX2D_SKIP) return vx_result(r,dst);
 }
 VX_SETTLE(); /* v10.425 */
 ok=DIB_BitBlt(dst,dx,dy,src,sx,sy,w,h,rop,brush,dm);
 vxb_mem(VXH_TAG_BBLT,dst,(short)dx,(short)dy,w,h,src,sx,sy,rop,NULL,0,tried,(DWORD)ok); /* v10.321 */
 return ok;
}
BOOL WINAPI __loadds StretchBlt(LPPDEVICE dst,WORD dx,WORD dy,WORD dw,WORD dh,
 LPPDEVICE src,WORD sx,WORD sy,WORD sw,WORD sh,DWORD rop,LPBRUSH brush,
 LPDRAWMODE dm,LPRECT clip) {
 WORD r,tried=0xFFFF; LPDIBENGINE d=(LPDIBENGINE)dst; BOOL ok;
 if(dw==sw&&dh==sh&&(d->deFlags&VRAM)&&!(d->deFlags&BUSY)) {
  if(src&&(LPVOID)src!=(LPVOID)dst) {r=vx_try_memblit(d,dx,dy,src,sx,sy,dw,dh,rop,brush,dm,clip); tried=r; if(r!=VX2D_SKIP) {vxb_mem(VXH_TAG_SBLT,d,(short)dx,(short)dy,dw,dh,src,sx,sy,rop,clip,(DWORD)sw|((DWORD)sh<<16),r,0xFFFFFFFFUL); return vx_result(r,d);}} /* v10.116 */
  r=vx_try_blit(d,dx,dy,src,sx,sy,dw,dh,rop,brush,dm,clip);
  if(r!=VX2D_SKIP) return vx_result(r,d);
 }
 VX_SETTLE(); /* v10.425 */
 ok=DIB_StretchBlt(dst,dx,dy,dw,dh,src,sx,sy,sw,sh,rop,brush,dm,clip);
 vxb_mem(VXH_TAG_SBLT,d,(short)dx,(short)dy,dw,dh,src,sx,sy,rop,clip,(DWORD)sw|((DWORD)sh<<16),tried,(DWORD)ok); /* v10.321 */
 return ok;
}

/* v10.99/v10.100: the DIB conversion fast path. StretchDIBits (1:1 or enlarged a whole number of times) and DibToDevice (SetDIBitsToDevice and bottom-up 1:1 StretchDIBits) of a 555, 565, 24-bit or 32-bit
 * bitmap into the 16bpp or 32bpp screen: the VxD converts and draws it with the CPU (DIBENG needs about 9 ms for 24/32-bit 1:1 into 16bpp, 17 ms for 555/565 1:1 into 32bpp, about 20 ms for enlargements).
 * Everything else, and anything the VxD declines, goes to DIBENG through the capturing forwarders. What the measurements (VX2SDI2, VX2DTD) fixed: the source rectangle's y of a StretchDIBits is counted from the bottom of
 * the picture for bottom-up and top-down bitmaps alike; the clip rectangle is always given (right and bottom exclusive); the ROP is SRCCOPY; DibToDevice is called for bottom-up bitmaps only, with the whole bitmap
 * (iScan 0, cScans = height) at the origin of the picture (X, Y may be negative), the clip rectangle doing the cropping; for a partial bitmap (StartScan/cLines) the bits hold only the lines the clip shows. */
extern int WINAPI StretchDIBits_fwd(LPPDEVICE,WORD,WORD,WORD,WORD,WORD,WORD,WORD,WORD,WORD,LPVOID,LPBITMAPINFO,LPINT,DWORD,LPBRUSH,LPDRAWMODE,LPRECT);
extern WORD WINAPI DibToDevice_fwd(LPPDEVICE,WORD,WORD,WORD,WORD,LPRECT,LPDRAWMODE,LPVOID,LPBITMAPINFO,LPINT);
static vx2d_stretch vx_sq;
/* dx, dy: where the picture's top left lands (may be negative); kx, ky the enlargement; sx, sy0 the first source column and the first row counted from the top of the picture; sw, sh the source size */
static int vx_dib_565(LPBITMAPINFOHEADER h); /* .532 (below) */
static WORD vx_dib_draw(LPPDEVICE dst,long dx,long dy,WORD kx,WORD ky,WORD sx,WORD sy0,WORD sw,WORD sh,LPVOID bits,LPBITMAPINFO info,LPINT trans,LPRECT clip,int needbottomup) {
 LPDIBENGINE d=(LPDIBENGINE)dst; LPBITMAPINFOHEADER h; long W,Hh,vl,vt,vr,vb,stride; WORD H,bt,r,fl=0,rle=0; DWORD base;
 if(!bits||!info||!wEnabled||dst!=lpDriverPDevice||(wBpp!=16&&wBpp!=32)) return VX2D_SKIP;
 if((d->deFlags&(VRAM|BUSY))!=VRAM) return VX2D_SKIP;
 h=(LPBITMAPINFOHEADER)info;
 if(h->biSize<40||h->biPlanes!=1) return VX2D_SKIP;
 bt=h->biBitCount; if(bt!=1&&bt!=4&&bt!=8&&bt!=16&&bt!=24&&bt!=32) return VX2D_SKIP;
 if(bt==32&&wBpp==32) {vxgt_cnt[13]++; return VX2D_SKIP;} /* v10.509: the VxD has no class for 32-bit sources onto the 32bpp screen (DIBENG is as fast): no call (VXGTUNE v1.1: 2 us of 36 at 64x64) */
 /* v10.104: 1-bit, 4-bit and 8-bit (palette) sources go to the VxD only for the 32bpp screen (DIBENG's own palette path is as fast into 16bpp and rounds the colours its own way); the colour table is the one in the header. A translation table means index colours, left to DIBENG. */
 if(bt<=8&&(trans||wBpp!=32||h->biClrUsed>(1UL<<bt))) return VX2D_SKIP;
 if(bt<=8&&h->biCompression!=0) { /* v10.104: RLE8 (8-bit) and RLE4 (4-bit): the VxD decodes a stream that covers every pixel exactly once, bottom-up bitmaps only */
  if(!((h->biCompression==1&&bt==8)||(h->biCompression==2&&bt==4))||h->biSizeImage==0||h->biSizeImage>0x1000000UL) return VX2D_SKIP;
  rle=1; fl=VX2D_STR_RLE;
 }
 if(!rle&&h->biCompression!=0) { /* the only other compressed form taken: a 16-bit 5-6-5 BITFIELDS bitmap into a 32bpp screen */
  if(h->biCompression!=3||bt!=16||wBpp!=32) return VX2D_SKIP;
  if(!vx_dib_565(h)) return VX2D_SKIP; /* .532 (review B4): the masks of a V4/V5 header are inside it */
  fl=VX2D_STR_565;
 }
 W=h->biWidth; Hh=h->biHeight;
 if(W<=0||W>4096L||Hh==0||Hh>4096L||Hh<-4096L) return VX2D_SKIP;
 H=(WORD)(Hh<0?-Hh:Hh);
 if(needbottomup&&Hh<0) return VX2D_SKIP;
 if(rle&&(Hh<0||W*(long)Hh>262144L)) return VX2D_SKIP;
 /* v10.101: a top-down bitmap is taken only whole (all its rows). SetDIBitsToDevice of a top-down bitmap with StartScan/cLines reaches the DDI as a StretchDIBits whose rectangle names rows the buffer does not hold (VX2STRX v2.1 probe: DIBENG draws nothing); that case cannot be told from a legitimate rectangle, so any top-down sub-rectangle stays with DIBENG. */
 if(Hh<0&&(sy0!=0||sh!=H)) return VX2D_SKIP;
 if(sx>(WORD)W||sw>(WORD)W-sx||sy0>H||sh>H-sy0) return VX2D_SKIP;
 vl=dx; vt=dy; vr=dx+(long)sw*kx; vb=dy+(long)sh*ky;
 if(clip) {
  if(clip->left>vl) vl=clip->left; if(clip->top>vt) vt=clip->top; if(clip->right<vr) vr=clip->right; if(clip->bottom<vb) vb=clip->bottom;
 }
 if(vl<0) vl=0; if(vt<0) vt=0; if(vr>wScreenX) vr=wScreenX; if(vb>wScreenY) vb=wScreenY;
 if(vr<=vl||vb<=vt) return VX2D_SKIP;
 stride=((W*bt+31L)/32L)*4L;
 base=DPMI_GetSegBase((WORD)(((DWORD)bits)>>16));
 if(!base||base==0xFFFFFFFFUL) return VX2D_SKIP;
 _fmemset(&vx_sq,0,sizeof(vx_sq)); vx_sq.cb=sizeof(vx_sq); vx_sq.abi=VX2D_ABI; vx_sq.bpp=wBpp; vx_sq.flags=fl;
 vx_sq.dst.pitch=hda->pitch; vx_sq.dst.width=wScreenX; vx_sq.dst.height=wScreenY;
 vx_sq.dx=(DWORD)dx; vx_sq.dy=(DWORD)dy; vx_sq.kx=kx; vx_sq.ky=ky;
 vx_sq.sx=sx; vx_sq.sy=sy0; vx_sq.sw=sw; vx_sq.sh=sh;
 vx_sq.left=(DWORD)vl; vx_sq.top=(DWORD)vt; vx_sq.right=(DWORD)vr; vx_sq.bottom=(DWORD)vb;
 if(bt<=8) { /* the colour table: biClrUsed entries (0 = all 2^bits) of RGBQUAD, the rest stays zero */
  WORD n=h->biClrUsed?(WORD)h->biClrUsed:(WORD)(1<<bt);
  _fmemcpy(vx_sq.pal,(BYTE far *)info+h->biSize,(size_t)n*4); vx_sq.pal_n=n;
 }
 vx_sq.src_lin=base+(((DWORD)bits)&0xFFFFUL); vx_sq.src_stride=(DWORD)stride; vx_sq.src_bits=bt; vx_sq.src_height=H; vx_sq.src_topdown=Hh<0?1:0; vx_sq.src_bytes=(DWORD)stride*H;
 if(rle) {vx_sq.src_stride=(DWORD)W; vx_sq.src_width=(DWORD)W; vx_sq.src_bytes=h->biSizeImage;} /* the compressed stream as it is; the decoded width */
 r=VX_stretch_submit(&vx_sq);
 if(r==VX2D_LOST) {d->deFlags|=BUSY; wEnabled=0; return VX2D_LOST;}
 return r;
}
/* v10.118: StretchDIBits 1:1 with a ROP that reads the screen (VX2GDI v1.1: BitBlt from a DIB section never reaches BitBlt; GDI hands it here): a 32-bit source onto the 32bpp screen or a 5-6-5 BITFIELDS source
 * onto the 16bpp screen goes through the staging block like a DIB engine bitmap. sy counts from the bottom of the picture for both row orders (v10.101). */
/* .532 (review of .530, B4): a 16-bit BI_BITFIELDS DIB is 5-6-5 when its three masks are F800, 07E0, 001F. They sit at offset 40 in
 * every header form: after a 40-byte BITMAPINFOHEADER, inside BITMAPV2/V3/V4/V5 (52, 56, 108, 124 bytes). Both paths read them at
 * info + biSize, past a V4/V5 header (a fault at a segment's end, or garbage taken as 5-6-5). Any other size goes to DIBENG. */
static int vx_dib_565(LPBITMAPINFOHEADER h) {
 DWORD far *m;
 if(h->biCompression!=3||(h->biSize!=40&&h->biSize<52)) return 0;
 m=(DWORD far *)((BYTE far *)h+40);
 return m[0]==0xF800UL&&m[1]==0x07E0UL&&m[2]==0x001FUL;
}
static WORD vx_dib_rop(LPPDEVICE dst,int dx,int dy,WORD w,WORD h,WORD sx,WORD sy,LPVOID bits,LPBITMAPINFO info,LPINT trans,DWORD rop,LPBRUSH brush,LPRECT clip) {
 LPDIBENGINE d=(LPDIBENGINE)dst; LPBITMAPINFOHEADER bh=(LPBITMAPINFOHEADER)info; BYTE r3=(BYTE)(rop>>16); long W,Hh,stride; WORD H,t0; DWORD base; int l,t,r,b;
 if(!(vx_host_mask&4)||!bits||!info) return VX2D_SKIP;
 if(!wEnabled||dst!=lpDriverPDevice||(d->deFlags&(VRAM|BUSY))!=VRAM||(wBpp!=16&&wBpp!=32)) return VX2D_SKIP;
 if((rop>>24)||!vx_rop_s(r3)||!vx_rop_d(r3)) return VX2D_SKIP;
 if(bh->biSize<40||bh->biPlanes!=1||bh->biBitCount!=wBpp||trans) return VX2D_SKIP;
 if(wBpp==32&&bh->biCompression!=0) return VX2D_SKIP;
 if(wBpp==16&&!vx_dib_565(bh)) return VX2D_SKIP; /* .532 (review B4) */
 W=bh->biWidth; Hh=bh->biHeight; if(W<=0||W>4096L||!Hh||Hh>4096L||Hh<-4096L) return VX2D_SKIP;
 H=(WORD)(Hh<0?-Hh:Hh); stride=((W*wBpp+31L)/32L)*4L;
 if(sx>(WORD)W||w>(WORD)W-sx||sy>H||h>H-sy) return VX2D_SKIP;
 t0=(WORD)(H-sy-h); /* the rectangle's first row counted from the top */
 l=dx; t=dy; r=dx+(int)w; b=dy+(int)h;
 if(clip) {if(clip->left>l) l=clip->left; if(clip->top>t) t=clip->top; if(clip->right<r) r=clip->right; if(clip->bottom<b) b=clip->bottom;}
 if(l<0) l=0; if(t<0) t=0; if(r>(int)wScreenX) r=(int)wScreenX; if(b>(int)wScreenY) b=(int)wScreenY;
 if(r<=l||b<=t) return VX2D_DONE;
 sx=(WORD)(sx+(l-dx)); t0=(WORD)(t0+(t-dy)); w=(WORD)(r-l); h=(WORD)(b-t);
 base=DPMI_GetSegBase((WORD)(((DWORD)bits)>>16)); if(!base||base==0xFFFFFFFFUL) return VX2D_SKIP;
 base+=((DWORD)bits)&0xFFFFUL;
 vx_xq_init(VX2D_XF_STAGE);
 vx_xq.rop=r3; vx_xq.dx=(DWORD)l; vx_xq.dy=(DWORD)t; vx_xq.w=w; vx_xq.h=h; vx_xq.src_bits=wBpp; vx_xq.src_x=sx; vx_xq.src_y=t0; vx_xq.src_w=(DWORD)W; vx_xq.src_h=H;
 if(Hh<0) {vx_xq.src_lin=base; vx_xq.src_stride=stride;} else {vx_xq.src_lin=base+(DWORD)stride*(H-1); vx_xq.src_stride=-stride;} /* row 0 = the top row */
 if(vx_rop_p(r3)) {if(vx_solid(brush)) {vx_xq.flags|=VX2D_XF_FIXPAT; vx_xq.pfg=vx_pc;} else if(vx_colpat(d,l,t,w,h,brush,NULL)) return VX2D_SKIP;} /* .597 */
 {WORD rr=vx_xq_run(); if(rr==VX2D_LOST) {d->deFlags|=BUSY; wEnabled=0;} return rr;}
}
int WINAPI __loadds StretchDIBits(LPPDEVICE dst,WORD fGet,WORD dx,WORD dy,WORD dw,WORD dh,WORD sx,WORD sy,WORD sw,WORD sh,
 LPVOID bits,LPBITMAPINFO info,LPINT trans,DWORD rop,LPBRUSH brush,LPDRAWMODE dm,LPRECT clip) {
 WORD tried=0xFFFF; int n;
 if(!fGet&&rop!=VX_ROP_SRCCOPY&&dw==sw&&dh==sh&&sw&&sh&&(short)dw>0&&(short)dh>0) { /* v10.118 */
  WORD r=vx_dib_rop(dst,(int)(short)dx,(int)(short)dy,dw,dh,sx,sy,bits,info,trans,rop,brush,clip);tried=r;
  if(r==VX2D_DONE||r==VX2D_FAILED||r==VX2D_LOST) {vxb_dib(VXH_TAG_XDIB,dst,(short)dx,(short)dy,dw,dh,sx,sy,rop,clip,bits,info,(DWORD)sw|((DWORD)sh<<16),(DWORD)dw|((DWORD)dh<<16),r,0xFFFFFFFFUL); return r==VX2D_DONE?(int)sh:0;}
 }
 if(!fGet&&rop==VX_ROP_SRCCOPY&&(short)dw>0&&(short)dh>0&&sw&&sh&&!(dw%sw)&&!(dh%sh)&&info) {
  LPBITMAPINFOHEADER ih=(LPBITMAPINFOHEADER)info; long hh=ih->biHeight; WORD H;
  if(hh<0) hh=-hh;
  if(hh>0&&hh<=4096L) {
   H=(WORD)hh;
   if((long)sy+sh<=(long)H){tried=vx_dib_draw(dst,(long)(short)dx,(long)(short)dy,dw/sw,dh/sh,sx,(WORD)(H-sy-sh),sw,sh,bits,info,trans,clip,0);
    if(tried==VX2D_DONE||tried==VX2D_LOST) {vxb_dib(VXH_TAG_XDIB,dst,(short)dx,(short)dy,dw,dh,sx,sy,rop,clip,bits,info,(DWORD)sw|((DWORD)sh<<16),(DWORD)dw|((DWORD)dh<<16),tried,0xFFFFFFFFUL); return tried==VX2D_DONE?(int)sh:0;}} /* v10.330 (review #73): LOST (engine gone, the device marked BUSY) is not handed to DIBENG; FAILED (the engine timed out and was reset, idle again) is: a copy drawn again over itself is the same picture */
  }
 }
 n=StretchDIBits_fwd(dst,fGet,dx,dy,dw,dh,sx,sy,sw,sh,bits,info,trans,rop,brush,dm,clip);
 if(!fGet) vxb_dib(VXH_TAG_XDIB,dst,(short)dx,(short)dy,dw,dh,sx,sy,rop,clip,bits,info,(DWORD)sw|((DWORD)sh<<16),(DWORD)dw|((DWORD)dh<<16),tried,(DWORD)(long)n); /* v10.321 */
 return n;
}
WORD WINAPI __loadds DibToDevice(LPPDEVICE dst,WORD x,WORD y,WORD iScan,WORD cScans,LPRECT clip,LPDRAWMODE dm,LPVOID bits,LPBITMAPINFO info,LPINT trans) {
 WORD tried=0xFFFF,n;
 if(!iScan&&info) {
  LPBITMAPINFOHEADER ih=(LPBITMAPINFOHEADER)info; long hh=ih->biHeight,ww=ih->biWidth;
  if(hh>0&&hh<=4096L&&ww>0&&ww<=4096L&&(long)cScans==hh){tried=vx_dib_draw(dst,(long)(short)x,(long)(short)y,1,1,0,0,(WORD)ww,cScans,bits,info,trans,clip,1);
   if(tried==VX2D_DONE||tried==VX2D_LOST) {vxb_dib(VXH_TAG_XDTD,dst,(short)x,(short)y,0,cScans,0,0,VX_ROP_SRCCOPY,clip,bits,info,(DWORD)iScan|((DWORD)cScans<<16),0,tried,0xFFFFFFFFUL); return tried==VX2D_DONE?cScans:0;}} /* v10.330 (review #74): as StretchDIBits (LOST not to DIBENG; FAILED, a copy, drawn again by it) */
 }
 n=DibToDevice_fwd(dst,x,y,iScan,cScans,clip,dm,bits,info,trans);
 vxb_dib(VXH_TAG_XDTD,dst,(short)x,(short)y,0,cScans,0,0,VX_ROP_SRCCOPY,clip,bits,info,(DWORD)iScan|((DWORD)cScans<<16),0,tried,(DWORD)n); /* v10.321 */
 return n;
}

#endif

#ifndef ETO_GLYPH_INDEX
#define ETO_GLYPH_INDEX 0x0010
#endif

DWORD WINAPI __loadds ExtTextOut( LPDIBENGINE lpDestDev, WORD wDestXOrg, WORD wDestYOrg, LPRECT lpClipRect,
                                        LPSTR lpString, int wCount, LPFONTINFO lpFontInfo, LPDRAWMODE lpDrawMode,
                                        LPTEXTXFORM lpTextXForm, LPSHORT lpCharWidths, LPRECT lpOpaqueRect, WORD wOptions )
{
	WORD tcond=0,trr=0xFFFF,topts=wOptions; LPRECT topaque=lpOpaqueRect;
	if(wOptions&0x102) tcond|=1;
	if(lpOpaqueRect) tcond|=2;
	if(lpDrawMode) tcond|=4;
	if(wEnabled) tcond|=8;
	if(lpDestDev==(LPDIBENGINE)lpDriverPDevice) tcond|=16;
	if(lpDestDev->deFlags&VRAM) tcond|=32;
	if(!(lpDestDev->deFlags&BUSY)) tcond|=64;
	if(wBpp==8||wBpp==16||wBpp==32) tcond|=128;
	/* v10.83: the opaque rectangle (ETO_OPAQUE: filled with the draw mode's background colour before the text is drawn) is a plain fill; large ones go to the engine, then the
	 * text is drawn by DIBENG without the opaque flag. A rectangle the engine declines (small, engine off, ...) stays with DIBENG as before. */
	/* v10.84: VX2AUTO v2.0's diagnostic showed GDI hands the driver the opaque rectangle with options 0x100 (not ETO_OPAQUE 0x0002) for ETO_OPAQUE calls, so both bits count and both are cleared. */
	if((wOptions&0x102)&&lpOpaqueRect&&lpDrawMode&&wEnabled&&lpDestDev==(LPDIBENGINE)lpDriverPDevice&&(lpDestDev->deFlags&VRAM)&&!(lpDestDev->deFlags&BUSY)&&(wBpp==8||wBpp==16||wBpp==32))
	{
		int l=lpOpaqueRect->left,t=lpOpaqueRect->top,r=lpOpaqueRect->right,b=lpOpaqueRect->bottom;
		if(lpClipRect)
		{
			if(l<lpClipRect->left) l=lpClipRect->left;
			if(t<lpClipRect->top) t=lpClipRect->top;
			if(r>lpClipRect->right) r=lpClipRect->right;
			if(b>lpClipRect->bottom) b=lpClipRect->bottom;
		}
		if(l<0) l=0;
		if(t<0) t=0;
		if(r>(int)wScreenX) r=(int)wScreenX;
		if(b>(int)wScreenY) b=(int)wScreenY;
		if(r>l&&b>t)
		{
			static vx2d_request q;
			WORD rr;
			tcond|=256;
			_fmemset(&q,0,sizeof(q)); q.cb=sizeof(q); q.abi=VX2D_ABI; q.bpp=wBpp;
			q.dst.pitch=hda->pitch; q.dst.width=wScreenX; q.dst.height=wScreenY; q.src=q.dst;
			q.op=VX2D_FILL; q.rop=0xF0; q.dx=l; q.dy=t; q.width=r-l; q.height=b-t;
			q.fg=lpDrawMode->bkColor&(wBpp==16?0xFFFFUL:wBpp==8?0xFFUL:0xFFFFFFFFUL); q.bg=q.fg; /* v10.330 (review #183): 32bpp keeps the whole dword, as the mono and span paths do (v10.84: DIBENG writes the top byte too) */
			rr=vx_pre_declined(&q)?VX2D_SKIP:VX_accel_submit(&q); trr=rr; /* v10.142: a rectangle the VxD would decline for its size stays with DIBENG without the trip */
			/* v10.330 (review #132): LOST: nothing more is drawn (the device is marked BUSY, no CPU access); FAILED (reset, idle again): DIBENG
			 * fills the rectangle again (a solid fill drawn twice is the same) with the glyphs, as before */
			if(rr==VX2D_LOST) {lpDestDev->deFlags|=BUSY; wEnabled=0; vxh_text(tcond,topts,trr,topaque,lpClipRect,lpDrawMode); return 0;}
			else if(rr==VX2D_DONE) {lpOpaqueRect=(LPRECT)&vx_empty_rect; /* v10.85: the options stay as GDI sent them, so DIBENG draws the text by the same routine as with its own opaque rectangle (VX2AUTO v2.1: clearing the flag changed the glyph pixels); the rectangle it is given is empty */}
		}
	}
	vxh_text(tcond,topts,trr,topaque,lpClipRect,lpDrawMode);
	VX_SETTLE(); /* v10.425 */
	return DIB_ExtTextOut(lpDestDev, wDestXOrg, wDestYOrg, lpClipRect, lpString, wCount, lpFontInfo, lpDrawMode, lpTextXForm, lpCharWidths, lpOpaqueRect, wOptions);
}

/* v10.90: lines. GDI hands Polyline and LineTo to Output (style OS_POLYLINE) with the pen and the draw mode. VX2LINE (hardware, v1.1/v1.2) measured DIBENG's lines: every segment is end-exclusive (a Polyline is
 * its segments, the last point is not drawn); a tie takes the minor step when Y decreases and not when Y increases; the engine runs the same recurrence, so the VxD reproduces that (vx855_2d.c).
 * The engine's line command draws a plain pen copy only, so R2_COPYPEN, R2_BLACK and R2_NOTCOPYPEN go to the engine (colour = pen, 0, ~pen); every other raster operation, wide or styled pens, anything
 * off the screen and any segment the engine declines (too short, engine off) stays with DIBENG, which draws exactly the same pixels, one segment at a time. */
/* v10.91: an Output call that goes to DIBENG is recorded while VXH_ARM_CALLS is on (the tag is described in vx_hatch_diag.h). */
static void vxo_record(WORD wStyle,WORD wCount,LPPOINT lpPoints,LPPPEN lpPPen,LPPBRUSH lpPBrush,LPDRAWMODE lpDrawMode)
{
	vxh_record *p; WORD i;
	if(!(vxh_log.active&2)) return;
	if(vxh_log.count>=VXH_CAP) {vxh_log.dropped++; return;}
	p=&vxh_log.records[(WORD)vxh_log.count]; _fmemset(p,0,sizeof(*p));
	p->seq=vxh_log.count++; p->rop=VXH_TAG_OUTP; p->bpp=wBpp; p->w=wCount; p->h=wStyle; p->style=wStyle; p->hatch=0xFFFF;
	if(lpPoints&&wCount) {PTTYPE __far *pt=(PTTYPE __far *)lpPoints; p->x=(WORD)pt[0].xcoord; p->y=(WORD)pt[0].ycoord; for(i=0;i<wCount&&i<4;i++) p->bits[i]=((DWORD)(WORD)pt[i].ycoord<<16)|(WORD)pt[i].xcoord;}
	if(lpPPen) {DIB_Pen __far *pen=(DIB_Pen __far *)lpPPen; p->flags=pen->dpPenStyle|((DWORD)pen->dpPenFlags<<16); p->brush_bpp=pen->dpPenBpp; p->raw_fg=pen->dpPenColor; p->raw_bg=pen->dpPenMono;}
	if(lpPBrush) {DIB_Brush32 __far *b=(DIB_Brush32 __far *)lpPBrush; p->hatch=b->dp32BrushStyle; p->mono[0]=b->dp32BrushFlags;}
	if(lpDrawMode) {p->dm_valid=1; p->mask[0]=(WORD)lpDrawMode->Rop2; p->bk_mode=(WORD)lpDrawMode->bkMode; p->text_color=lpDrawMode->TextColor; p->bk_color=lpDrawMode->bkColor;}
}
/* v10.507: GDI options published by the VxD in FBHDA rem3 bits 31:16 (0x8000 | bit 0 ACCEL_CPULINE | bit 1 ACCEL_GDIFAST), the cursor state in rem4 ('CU' << 16 | bit 0 direct VRAM |
 * bit 1 software cursor in the screen | bit 2 a show put off), the driver's busy count in rem3 bits 15:0 (written only here). VX_fast_begin raises the count first and then reads the
 * cursor state: from then on the VxD does not draw the software cursor (it puts the show off and says so in bit 2), so while bit 0 is set and bit 1 clear nothing of the cursor
 * is in the screen and nothing can come into it while the driver draws. VX_fast_end lowers the count and, when a show was put off, makes the empty access that lets the VxD draw it. */
DWORD vxgt_cnt[16]; /* VXGTUNE (escape VXGT_ESCAPE): 0 CPU polylines, 1 segments, 2 pixels, 3 clipped segments, 4 CPU polylines with the exclusion calls, 5 fast DIBENG accesses, 6 DIBENG accesses with the calls, 7 put-off shows released, 8 polylines left to the engine/DIBENG,
 * 12 long segments sent to the engine (v10.508), 13 32-bit DIBs onto the 32bpp screen left to DIBENG without the VxD call (v10.509) */
static WORD vx_busy;
#define VX_GDI_FEAT() (((volatile WORD __far *)&hda->rem3)[1])
#define VX_CUR_TAG() (((volatile WORD __far *)&hda->rem4)[1])
#define VX_CUR_LO() (((volatile WORD __far *)&hda->rem4)[0])
void VX_fast_end(void) {
 if(vx_busy) vx_busy--;
 ((volatile WORD __far *)&hda->rem3)[0]=vx_busy;
 if(!vx_busy&&(VX_CUR_LO()&4)) {FBHDA_access_rect(0,0,0,0); FBHDA_access_end(0); vxgt_cnt[7]++;}
}
int VX_fast_begin(void) {
 if(!hda||(VX_GDI_FEAT()&0x8002U)!=0x8002U||VX_CUR_TAG()!=0x4355U) return 0;
 vx_busy++; ((volatile WORD __far *)&hda->rem3)[0]=vx_busy; /* busy before the cursor state is read */
 if((VX_CUR_LO()&3)!=1) {VX_fast_end(); return 0;}
 return 1;
}
/* v10.507 (ACCEL_CPULINE): solid one-pixel lines drawn here, straight into the screen, with the rule the engine path and DIBENG use (VX2LINE): n = the major length, m = the minor one,
 * e0 = 2m - n - (1 when Y increases), each of the n pixels is drawn, then e >= 0 takes the minor step and adds 2(m - n), else adds 2m; the major step always; the end point is not drawn.
 * Pen copy (R2_COPYPEN, R2_NOTCOPYPEN, R2_BLACK: the colours the engine path writes) and R2_XORPEN (pen colour XOR the screen). A segment that leaves the clip rectangle is stepped in C
 * to its first and last visible pixel (the visible pixels of a line in a rectangle are one run) and that run is drawn. */
static DWORD ln_off,ln_n,ln_e,ln_ilt,ln_ige,ln_maj,ln_min,ln_col; static WORD ln_sel,ln_kind;
static void vx_line_run(void)
{
 _asm {
  .386
  push eax
  push ebx
  push ecx
  push edx
  push esi
  push edi
  push es
  mov ax,[ln_sel]
  mov es,ax
  mov edi,[ln_off]
  mov ecx,[ln_n]
  mov edx,[ln_e]
  mov esi,[ln_maj]
  mov ebx,[ln_min]
  mov eax,[ln_col]
  test ecx,ecx
  jz LDONE
  cmp word ptr [ln_kind],2
  je L32
  cmp word ptr [ln_kind],1
  je L16
  cmp word ptr [ln_kind],5
  je X32
  cmp word ptr [ln_kind],4
  je X16
  cmp word ptr [ln_kind],3
  je X8
 L8:
  mov es:[edi],al
  add edi,esi
  test edx,edx
  js N8
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz L8
  jmp LDONE
 N8:
  add edx,[ln_ilt]
  dec ecx
  jnz L8
  jmp LDONE
 L16:
  mov es:[edi],ax
  add edi,esi
  test edx,edx
  js N16
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz L16
  jmp LDONE
 N16:
  add edx,[ln_ilt]
  dec ecx
  jnz L16
  jmp LDONE
 L32:
  mov es:[edi],eax
  add edi,esi
  test edx,edx
  js N32
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz L32
  jmp LDONE
 N32:
  add edx,[ln_ilt]
  dec ecx
  jnz L32
  jmp LDONE
 X8:
  xor es:[edi],al
  add edi,esi
  test edx,edx
  js Y8
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz X8
  jmp LDONE
 Y8:
  add edx,[ln_ilt]
  dec ecx
  jnz X8
  jmp LDONE
 X16:
  xor es:[edi],ax
  add edi,esi
  test edx,edx
  js Y16
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz X16
  jmp LDONE
 Y16:
  add edx,[ln_ilt]
  dec ecx
  jnz X16
  jmp LDONE
 X32:
  xor es:[edi],eax
  add edi,esi
  test edx,edx
  js Y32
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz X32
  jmp LDONE
 Y32:
  add edx,[ln_ilt]
  dec ecx
  jnz X32
 LDONE:
  pop es
  pop edi
  pop esi
  pop edx
  pop ecx
  pop ebx
  pop eax
 }
}
/* .574 (.575: permanent; board, VXHLINE v1.0, 32 bpp: 16 pixels 1.05x, 64 1.16x, 160 1.43x, 352 1.92x, rectangle outlines 1.12x, random
 * lines 1.00x, 0 pixels differ): a horizontal pen-copy segment (no minor step, the major step one pixel) is one string store (PHFILL: REP STOS with 32-bit ECX and EDI, the
 * prefixes as bytes; at 16 bpp two pixels a store from an address that is a multiple of four; a leftward one filled from its last pixel): the same n
 * pixels the loop writes (the end point not drawn), the address left where the loop leaves it. Board, VXGTUNE: the loop is 25 ns a pixel. */
/* .510: a polyline whose points all lie inside the clip rectangle, segment after segment in one call: each segment's set-up (lengths, steps, e0) in 32-bit
 * registers, and its first pixel's address where the last segment ended (after n major and m minor steps the address is the next point's), so no multiply
 * and no C per segment. Stops at the last point, or before a pen-copy segment long enough for the engine (pl_i is then that segment: C sends it). */
static WORD pl_pseg,pl_poff,pl_i,pl_last,pl_dyp; static DWORD pl_off,pl_pitch,pl_bpb,pl_lmx,pl_lmy,pl_px,pl_segs;
static void vx_poly_run(void)
{
 _asm {
  .386
  push eax
  push ebx
  push ecx
  push edx
  push esi
  push edi
  push es
  push gs
  mov ax,[ln_sel]
  mov es,ax
  mov ax,[pl_pseg]
  mov gs,ax
  mov edi,[pl_off]
 PSEG:
  mov ax,[pl_i]
  cmp ax,[pl_last]
  jae PEND
  movzx esi,ax
  shl esi,2
  movzx eax,word ptr [pl_poff]
  add esi,eax
  movsx eax,word ptr gs:[esi]
  movsx ecx,word ptr gs:[esi+4]
  sub ecx,eax
  movsx eax,word ptr gs:[esi+2]
  movsx edx,word ptr gs:[esi+6]
  sub edx,eax
  mov esi,[pl_bpb]
  test ecx,ecx
  jns PXP
  neg ecx
  neg esi
 PXP:
  mov ebx,[pl_pitch]
  mov word ptr [pl_dyp],0
  test edx,edx
  jz PYZ
  jns PYP
  neg edx
  neg ebx
  jmp PYZ
 PYP:
  mov word ptr [pl_dyp],1
 PYZ:
  cmp edx,ecx
  jbe PSHAL
  xchg ecx,edx
  xchg esi,ebx
  mov eax,[pl_lmy]
  jmp PLIM
 PSHAL:
  mov eax,[pl_lmx]
 PLIM:
  cmp word ptr [ln_kind],3
  jae PNOLIM
  cmp ecx,eax
  jae PEND
 PNOLIM:
  add edx,edx
  mov [ln_ilt],edx
  mov eax,edx
  sub eax,ecx
  sub eax,ecx
  mov [ln_ige],eax
  sub edx,ecx
  movzx eax,word ptr [pl_dyp]
  sub edx,eax
  add [pl_px],ecx
  inc dword ptr [pl_segs]
  inc word ptr [pl_i]
  cmp dword ptr [ln_ilt],0
  jne PGEN
  cmp word ptr [ln_kind],3
  jae PGEN
  mov eax,[pl_bpb]
  mov word ptr [pl_dyp],0
  cmp esi,eax
  je PHFILL
  neg eax
  cmp esi,eax
  jne PGEN
  mov eax,ecx
  imul eax,[pl_bpb]
  sub edi,eax
  mov edx,edi
  mov word ptr [pl_dyp],1
  add edi,[pl_bpb]
 PHFILL:
  cld
  mov eax,[ln_col]
  cmp word ptr [ln_kind],1
  je PH16
  ja PH32
  db 0F3h,067h,0AAh
  jmp PHEND
 PH32:
  db 0F3h,067h,066h,0ABh
  jmp PHEND
 PH16:
  mov ebx,eax
  shl ebx,16
  or eax,ebx
  test edi,2
  jz PH16A
  mov es:[edi],ax
  add edi,2
  dec ecx
 PH16A:
  mov ebx,ecx
  shr ecx,1
  db 0F3h,067h,066h,0ABh
  test bl,1
  jz PHEND
  mov es:[edi],ax
  add edi,2
 PHEND:
  cmp word ptr [pl_dyp],0
  je PSEG
  mov edi,edx
  jmp PSEG
 PGEN:
  mov eax,[ln_col]
  cmp word ptr [ln_kind],2
  je P32
  cmp word ptr [ln_kind],1
  je P16
  cmp word ptr [ln_kind],5
  je Q32
  cmp word ptr [ln_kind],4
  je Q16
  cmp word ptr [ln_kind],3
  je Q8

 P8:
  mov es:[edi],al
  add edi,esi
  test edx,edx
  js P8N
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz P8
  jmp PSEG
 P8N:
  add edx,[ln_ilt]
  dec ecx
  jnz P8
  jmp PSEG
 P16:
  mov es:[edi],ax
  add edi,esi
  test edx,edx
  js P16N
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz P16
  jmp PSEG
 P16N:
  add edx,[ln_ilt]
  dec ecx
  jnz P16
  jmp PSEG
 P32:
  mov es:[edi],eax
  add edi,esi
  test edx,edx
  js P32N
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz P32
  jmp PSEG
 P32N:
  add edx,[ln_ilt]
  dec ecx
  jnz P32
  jmp PSEG
 Q8:
  xor es:[edi],al
  add edi,esi
  test edx,edx
  js Q8N
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz Q8
  jmp PSEG
 Q8N:
  add edx,[ln_ilt]
  dec ecx
  jnz Q8
  jmp PSEG
 Q16:
  xor es:[edi],ax
  add edi,esi
  test edx,edx
  js Q16N
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz Q16
  jmp PSEG
 Q16N:
  add edx,[ln_ilt]
  dec ecx
  jnz Q16
  jmp PSEG
 Q32:
  xor es:[edi],eax
  add edi,esi
  test edx,edx
  js Q32N
  add edi,ebx
  add edx,[ln_ige]
  dec ecx
  jnz Q32
  jmp PSEG
 Q32N:
  add edx,[ln_ilt]
  dec ecx
  jnz Q32
  jmp PSEG
 PEND:
  pop gs
  pop es
  pop edi
  pop esi
  pop edx
  pop ecx
  pop ebx
  pop eax
 }
}
/* v10.508/v10.509: a pen-copy segment goes to the engine from ACCEL_CPULINE_MAX pixels when shallow, ACCEL_CPULINE_MAXY when steep (one VxD call; VXGTUNE v1.1:
 * the engine 11-16 us a line, the CPU 4.4 us + 25 ns a pixel shallow, 70 ns steep). The VxD clips it to the clip rectangle. SKIP: the CPU draws it. */
static vx2d_request vx_lq;
static WORD vx_line_engine(int x0,int y0,int x1,int y1,int n,DWORD c,LPRECT clip,int cl,int ct,int cr,int cb) {
 _fmemset(&vx_lq,0,sizeof(vx_lq)); vx_lq.cb=sizeof(vx_lq); vx_lq.abi=VX2D_ABI; vx_lq.bpp=wBpp;
 vx_lq.dst.pitch=hda->pitch; vx_lq.dst.width=wScreenX; vx_lq.dst.height=wScreenY; vx_lq.src=vx_lq.dst;
 vx_lq.op=VX2D_LINE; vx_lq.rop=0xF0; vx_lq.fg=c; vx_lq.bg=c;
 vx_lq.dx=x0; vx_lq.dy=y0; vx_lq.sx=x1; vx_lq.sy=y1; vx_lq.width=n; vx_lq.height=1;
 if(clip) {vx_lq.flags|=VX2D_CLIP; vx_lq.left=cl; vx_lq.top=ct; vx_lq.right=cr; vx_lq.bottom=cb;}
 return VX_accel_submit(&vx_lq);
}
/* 1 = drawn (all of it), 0 = nothing drawn: the old path takes the call */
static int vx_cpu_lines(LPDIBENGINE dev,WORD wCount,PTTYPE __far *pt,LPPPEN lpPPen,LPDRAWMODE dm,LPRECT clip)
{
 DIB_Pen __far *pen=(DIB_Pen __far *)lpPPen;
 DWORD mask=(wBpp==8)?0xFFUL:(wBpp==16)?0xFFFFUL:0xFFFFFFFFUL,col,base;
 long pitch,bpb=wBpp>>3;
 int cl=0,ct=0,cr=(int)wScreenX,cb=(int)wScreenY,i,l,t,r,b,fast,kind=(wBpp==8)?0:(wBpp==16)?1:2,inside;
 WORD feat,fx_,fy_; long lmx,lmy;
 if(!hda) return 0;
 feat=VX_GDI_FEAT();
 if((feat&0x8001U)!=0x8001U||pen->dpPenStyle!=0) return 0;
 fx_=(WORD)((feat>>2)&63U); fy_=(WORD)((feat>>8)&63U); lmx=fx_==63?0x7FFFFFFFL:(long)fx_*16L; lmy=fy_==63?0x7FFFFFFFL:(long)fy_*16L; /* v10.509: 63 = no limit */
 switch(dm->Rop2) {
  case 13: col=pen->dpPenColor&mask; break;
  case 4: col=(~pen->dpPenColor)&mask; break;
  case 1: col=0; break;
  case 7: col=pen->dpPenColor&mask; kind+=3; break;
  default: return 0;
 }
 pitch=(long)dev->deDeltaScan;
 if(pitch<=0||!dev->deBitsSelector) return 0;
 if(clip) {
  if(clip->left>cl) cl=clip->left;
  if(clip->top>ct) ct=clip->top;
  if(clip->right<cr) cr=clip->right;
  if(clip->bottom<cb) cb=clip->bottom;
  if(cr<=cl||cb<=ct) return 0;
 }
 l=r=pt[0].xcoord; t=b=pt[0].ycoord;
 for(i=0;i<(int)wCount;i++) {
  int x=pt[i].xcoord,y=pt[i].ycoord;
  if(x<0||y<0||x>=(int)wScreenX||y>=(int)wScreenY) return 0;
  if(i&&x==pt[i-1].xcoord&&y==pt[i-1].ycoord) return 0; /* a zero-length segment: DIBENG's rule for it was not measured */
  if(x<l) l=x; if(x>r) r=x; if(y<t) t=y; if(y>b) b=y;
 }
 inside=l>=cl&&t>=ct&&r<cr&&b<cb; /* .510: no segment leaves the clip rectangle: vx_poly_run */
 if(l<cl) l=cl; if(t<ct) t=ct; if(r>=cr) r=cr-1; if(b>=cb) b=cb-1;
 VX_SETTLE();
 fast=VX_fast_begin();
 if(!fast) {FBHDA_access_rect((DWORD)l,(DWORD)t,(DWORD)r+1,(DWORD)b+1); vxgt_cnt[4]++;}
 base=dev->deBitsOffset; ln_sel=dev->deBitsSelector; ln_col=col; ln_kind=(WORD)kind;
 pl_pseg=(WORD)(((DWORD)(void __far *)pt)>>16); pl_poff=(WORD)(DWORD)(void __far *)pt; pl_last=(WORD)(wCount-1); pl_pitch=(DWORD)pitch; pl_bpb=(DWORD)bpb;
 pl_lmx=(DWORD)lmx; pl_lmy=(DWORD)lmy;
 for(i=0;i+1<(int)wCount;i++) {
  if(inside) { /* .510: the segments up to the next one for the engine (or the end) in one call */
   pl_i=(WORD)i; pl_off=base+(DWORD)pt[i].ycoord*(DWORD)pitch+(DWORD)pt[i].xcoord*(DWORD)bpb; pl_segs=0; pl_px=0;
   vx_poly_run();
   vxgt_cnt[1]+=pl_segs; vxgt_cnt[2]+=pl_px; i=(int)pl_i;
   if(i+1>=(int)wCount) break;
  }
  { /* (C89: the declarations open a block) */
  int x0=pt[i].xcoord,y0=pt[i].ycoord,x1=pt[i+1].xcoord,y1=pt[i+1].ycoord;
  int ddx=x1-x0,ddy=y1-y0,adx=ddx<0?-ddx:ddx,ady=ddy<0?-ddy:ddy,ym=ady>adx;
  long n=ym?ady:adx,m=ym?adx:ady,e=2*m-n-(ddy>0?1:0),ilt=2*m,ige=2*(m-n),xs=ddx<0?-bpb:bpb,ys=ddy<0?-pitch:pitch;
  int fx=x0,fy=y0; long cnt=n;
  if(kind<3&&n>=(ym?lmy:lmx)) { /* v10.508/v10.509: long pen-copy segments to the engine */
   WORD rr=vx_line_engine(x0,y0,x1,y1,(int)n,col,clip,cl,ct,cr,cb);
   if(rr==VX2D_DONE) {vxgt_cnt[12]++; continue;}
   if(rr==VX2D_LOST) {dev->deFlags|=BUSY; wEnabled=0; break;}
   /* SKIP or FAILED (fired, timed out): the CPU draws it (a pen copy: drawing a pixel twice is harmless) */
  }
  if((x0<cl||x0>=cr||x1<cl||x1>=cr||y0<ct||y0>=cb||y1<ct||y1>=cb)) { /* leaves the clip rectangle: its visible run worked out, not walked (.510) */
   /* Pixel k (0..n-1) is k major steps from (x0,y0) and j(k) minor ones. With A = e0 - 2m + 2n (0 <= A < 2n) the error before pixel k stays in [2m-2n, 2m), so
    * j(k) = floor((A + 2mk) / 2n), and the error there is e0 + 2mk - 2n j(k). The major range of the clip rectangle gives k in [k0, k1]; the minor range gives
    * j(k) in [j0, j1], and j(k) never falls, so k >= ceil((2n j0 - A) / 2m) and k <= floor((2n (j1+1) - A - 1) / 2m). The visible pixels are k0..k1 (one run). */
   long A=e-ilt+2*n,M0=ym?y0:x0,m0=ym?x0:y0,Mlo=ym?ct:cl,Mhi=ym?cb:cr,mlo=ym?cl:ct,mhi=ym?cr:cb,k0,k1,j0,j1,t,jf;
   int sM=ym?(ddy<0?-1:1):(ddx<0?-1:1),sm=ym?(ddx<0?-1:1):(ddy<0?-1:1);
   vxgt_cnt[3]++;
   if(sM>0) {k0=Mlo-M0; k1=Mhi-1-M0;} else {k0=M0-(Mhi-1); k1=M0-Mlo;}
   if(k0<0) k0=0; if(k1>n-1) k1=n-1;
   if(sm>0) {j0=mlo-m0; j1=mhi-1-m0;} else {j0=m0-(mhi-1); j1=m0-mlo;}
   if(j0<0) j0=0;
   if(k0>k1||j1<j0) continue;
   if(m>0) {
    t=2*n*j0-A; if(t>0) {t=(t+ilt-1)/ilt; if(t>k0) k0=t;}
    t=(2*n*(j1+1)-A-1)/ilt; if(t<k1) k1=t;
   } else if(j0>0) continue;
   if(k0>k1) continue;
   jf=(A+ilt*k0)/(2*n);
   if(ym) {fy=(int)(y0+sM*k0); fx=(int)(x0+sm*jf);} else {fx=(int)(x0+sM*k0); fy=(int)(y0+sm*jf);}
   e=e+ilt*k0-2*n*jf; cnt=k1-k0+1;
  }
  ln_off=base+(DWORD)fy*(DWORD)pitch+(DWORD)fx*(DWORD)bpb; ln_n=(DWORD)cnt; ln_e=(DWORD)e; ln_ilt=(DWORD)ilt; ln_ige=(DWORD)ige;
  ln_maj=(DWORD)(ym?ys:xs); ln_min=(DWORD)(ym?xs:ys);
  vx_line_run();
  vxgt_cnt[1]++; vxgt_cnt[2]+=(DWORD)cnt;
  }
 }
 if(fast) VX_fast_end(); else FBHDA_access_end(0);
 vxgt_cnt[0]++;
 return 1;
}
/* v10.92: segments shorter than this are not offered to the VxD at all (the VxD's own threshold is 32 by default; the engine call costs about 0.4 ms against roughly 12 us per pixel on DIBENG, so a 1 to 5 pixel segment of an
 * Arc or an Ellipse outline is never worth the trip). Consecutive segments that stay with DIBENG are handed over as one polyline, as DIBENG draws a Polyline as its segments (VX2LINE T5). */
#define VXO_PRE 8
WORD WINAPI __loadds Output( LPDIBENGINE lpDestDev, WORD wStyle, WORD wCount, LPPOINT lpPoints, LPPPEN lpPPen, LPPBRUSH lpPBrush, LPDRAWMODE lpDrawMode, LPRECT lpClipRect )
{
	if(wStyle==OS_POLYLINE&&wCount>=2&&wCount<=16384&&lpPoints&& /* v10.309 (review): a segment holds 16384 points; (int)wCount past 0x7FFF skipped both loops */
	   lpPPen&&lpDrawMode&&wEnabled&&lpDestDev==(LPDIBENGINE)lpDriverPDevice&&(lpDestDev->deFlags&VRAM)&&!(lpDestDev->deFlags&BUSY)&&(wBpp==8||wBpp==16||wBpp==32))
	{
		DIB_Pen __far *pen=(DIB_Pen __far *)lpPPen;
		PTTYPE __far *pt=(PTTYPE __far *)lpPoints;
		DWORD mask=(wBpp==8)?0xFFUL:(wBpp==16)?0xFFFFUL:0xFFFFFFFFUL,c=0;
		int ok=1,i,cl=0,ct=0,cr=0,cb=0,haveclip=0;
		if(vx_cpu_lines(lpDestDev,wCount,(PTTYPE __far *)lpPoints,lpPPen,lpDrawMode,lpClipRect)) return 1; /* v10.507 */
		vxgt_cnt[8]++;
		if(pen->dpPenStyle!=0) ok=0; /* .530: styled pens are the DIB engine's (the VxD's styled lines removed) */
		switch(lpDrawMode->Rop2)
		{
			case 13: c=pen->dpPenColor&mask; break;
			case 4:  c=(~pen->dpPenColor)&mask; break;
			case 1:  c=0; break;
			default: ok=0; break;
		}
		if(lpClipRect)
		{
			cl=lpClipRect->left; ct=lpClipRect->top; cr=lpClipRect->right; cb=lpClipRect->bottom;
			if(cl<0) cl=0;
			if(ct<0) ct=0;
			if(cr>(int)wScreenX) cr=(int)wScreenX;
			if(cb>(int)wScreenY) cb=(int)wScreenY;
			if(cr<=cl||cb<=ct) ok=0;
			haveclip=1;
		}
		for(i=0;ok&&i<(int)wCount;i++)
		{
			if(pt[i].xcoord<0||pt[i].ycoord<0||pt[i].xcoord>=(int)wScreenX||pt[i].ycoord>=(int)wScreenY) ok=0;
			if(i&&pt[i].xcoord==pt[i-1].xcoord&&pt[i].ycoord==pt[i-1].ycoord) ok=0; /* a zero-length segment: DIBENG's rule for it was not measured */
		}
		if(ok)
		{
			static vx2d_request q;
			WORD rr=VX2D_SKIP; int allok=1,pend=-1;
			for(i=0;i+1<(int)wCount;i++)
			{
				int dx=pt[i+1].xcoord-pt[i].xcoord,dy=pt[i+1].ycoord-pt[i].ycoord,adx=dx<0?-dx:dx,ady=dy<0?-dy:dy,len=(adx>ady)?adx:ady;
				if(len<VXO_PRE||vx_line_declined(len)) {if(pend<0) pend=i; continue;} /* v10.142: also the lengths under the VxD's ACCEL_LINE_MIN */
				_fmemset(&q,0,sizeof(q)); q.cb=sizeof(q); q.abi=VX2D_ABI; q.bpp=wBpp;
				q.dst.pitch=hda->pitch; q.dst.width=wScreenX; q.dst.height=wScreenY; q.src=q.dst;
				q.op=VX2D_LINE; q.rop=0xF0; q.fg=c; q.bg=c;
				q.dx=pt[i].xcoord; q.dy=pt[i].ycoord; q.sx=pt[i+1].xcoord; q.sy=pt[i+1].ycoord; q.width=len; q.height=1;
				if(haveclip) {q.flags|=VX2D_CLIP; q.left=cl; q.top=ct; q.right=cr; q.bottom=cb;}
				rr=VX_accel_submit(&q);
				if(rr==VX2D_DONE)
				{
					if(pend>=0) {VX_SETTLE();if(!DIB_Output(lpDestDev,wStyle,(WORD)(i-pend+1),(LPPOINT)(pt+pend),lpPPen,lpPBrush,lpDrawMode,lpClipRect)) allok=0; pend=-1;}
					continue;
				}
				if(rr==VX2D_SKIP)
				{
					if(pend<0) pend=i; /* this segment goes to DIBENG, drawn with the others that wait as one polyline from the same array */
					continue;
				}
				if(rr==VX2D_LOST) {lpDestDev->deFlags|=BUSY; wEnabled=0;}
				return 0; /* VX2D_FAILED / LOST: the command fired or the engine is gone, nothing is replayed */
			}
			if(pend>=0) {VX_SETTLE();if(!DIB_Output(lpDestDev,wStyle,(WORD)(wCount-pend),(LPPOINT)(pt+pend),lpPPen,lpPBrush,lpDrawMode,lpClipRect)) allok=0;}
			return allok?1:0;
		}
	}
	if(wStyle==OS_POLYSCANLINE) {int k=vx_polyscan(lpDestDev,wCount,lpPoints,lpPBrush,lpDrawMode,lpClipRect); if(k==1) return 1; if(k==2) return 0;} /* v10.118 */
	if(wStyle==OS_SCANLINES) {int k=vx_spans(lpDestDev,wCount,lpPoints,lpPBrush,lpDrawMode,lpClipRect); if(k==1) return 1; if(k==2) return 0;} /* v10.116 */
	vxo_record(wStyle,wCount,lpPoints,lpPPen,lpPBrush,lpDrawMode);
	VX_SETTLE(); /* v10.425 */
	return DIB_Output(lpDestDev, wStyle, wCount, lpPoints, lpPPen, lpPBrush, lpDrawMode, lpClipRect);
}

BOOL WINAPI __loadds DDIGammaRamp(LPDIBENGINE lpDev, BOOL fGetSet, LPARAM lpGammaRamp)
{
	BOOL rc;
	if(fGetSet)
	{
		rc = FBHDA_gamma_set((VOID FBPTR)lpGammaRamp, sizeof(WORD)*256*3);
		dbg_printf("set the ramp: %d\n", rc);
	}
	else
	{
		rc = FBHDA_gamma_get((VOID FBPTR)lpGammaRamp, sizeof(WORD)*256*3);
		dbg_printf("get the ramp\n");
	}
	
	return rc;
}
