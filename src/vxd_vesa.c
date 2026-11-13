/* VX2D_lost fences software cursor writes after an unrecoverable engine fault. */
#include "vx855_2d.h"
/**************************************************************************

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
#define VESA

#include "winhack.h"
#include "vmm.h"
#include "vxd.h"

#include "wram.h"
#include "async.h"

#include "vxd_lib.h"
#include "3d_accel.h"

#include "boxvint.h" /* VGA regitry */

#include "pci.h" /* re-use PCI functions from SVGA */

#include "vesa.h"
#include "vx85598.h"
#include "vx855_flip.h"
int VX3DN_busy(void);
static unsigned long vxd_irq_save(void);
#pragma aux vxd_irq_save = "pushfd" "pop eax" "cli" value [eax] modify exact [eax];
static void vxd_irq_restore(unsigned long f);
#pragma aux vxd_irq_restore = "push eax" "popfd" parm [eax] modify exact [];
#include "mtrr.h"

#define IO_IN8
#define IO_OUT8
#include "io32.h"

#include "vxd_terror.h"

#include "code32.h"

//#define CHOOSE_SCREEN_FREQ

#define ISA_LFB 0xE0000000UL

extern FBHDA_t *hda;
extern LONG fb_lock_cnt;
extern DWORD ThisVM;
extern ULONG hda_sem;


#define VESA_MODE_MAX_FREQS 16

typedef struct vesa_mode
{
	DWORD width;
	DWORD height;
	DWORD bpp;
	DWORD pitch;
	DWORD phy;
	WORD  mode_id;
	WORD  flags;
	WORD  freqs[VESA_MODE_MAX_FREQS];
} vesa_mode_t;

static vesa_mode_t *vesa_modes = NULL;
static DWORD vesa_modes_cnt = 0;
WORD vesa_version = 0;
static DWORD vesa_caps = 0;
static int act_mode = -1;
static DWORD video_bpp;
static DWORD video_pitch;
static BOOL vga_mode = TRUE;
static BOOL mttr_was_set = FALSE;
static BOOL timer_set = FALSE;

static blit_t wblit;

/* v10.24 direct-VRAM mode: the DIB engine draws straight into the screen, no shadow copy.
 * vx_direct=1 (default). Registry Software\VX85598 DIRECT_VRAM=0 restores the old shadow path. */
void VX_edid_probe(void);
int VX_edid_mode_ok(unsigned w, unsigned h);
extern int vx_edid_ok;
int VX_edid_set(const unsigned char *e, int bus);
static DWORD vx_bios_ddc_ax=0xFFFF; /* 0xFFFF = not tried */
static DWORD vx_direct=1;
static vesa_mode_t vx_held[24];
static DWORD vx_held_n=0;
static DWORD vx_base_modes=0, vx_extras_on=1, vx_bios_n=0, vx_bios_log[64*3];
static BOOL cur_drawn=FALSE;
/* v10.507: the cursor state the display driver reads (FBHDA rem4 = 'CU' << 16 | bit 0 direct VRAM | bit 1 software cursor drawn into the screen | bit 2 a show was put off),
 * and the driver's busy word (rem3 bits 15:0, written only by the driver): while the driver draws into the screen without a VxD call (ACCEL_GDIFAST, ACCEL_CPULINE) the
 * software cursor is not drawn; the show waits (bit 2) until the driver's next access, which the driver makes as soon as it is done. The driver takes that road only while
 * bit 0 is set and bit 1 clear: then nothing of the cursor is in the screen and nothing can put it there behind the driver's back. */
static BOOL cur_pend=FALSE;
static DWORD cur_defers;
static void cur_pub(void) {if(hda) hda->rem4=0x43550000UL|(vx_direct?1UL:0UL)|(cur_drawn?2UL:0UL)|(cur_pend?4UL:0UL);}
#define CUR_SET(v) do{cur_drawn=(v);cur_pub();}while(0)
DWORD VESA_cursor_defers(void) {return cur_defers;}
/* v10.105: hardware cursor state. While the hardware shows the shape the frame buffer is never touched, so the hide and show around CPU and engine access are skipped. */
extern volatile unsigned long vx_cursor_gen, vx2d_hwcur;
static BOOL cur_hw = FALSE, cur_hw_ok = FALSE;
static DWORD cur_hw_gen = 0xFFFFFFFFUL;
static BOOL cur_pal_used = FALSE; /* v10.262: the hardware cursor's shape took colours from the 8bpp palette */
static DWORD cur_epoch; /* v10.252: vx2d_reset_epoch when the hardware cursor was last programmed */
/* v10.333: VXPROBE (VX_IOCTL_PROBE). pr_set: the last mode set ([0] count, [1] w, [2] h, [3] bpp, [4] rr_min, [5] rr_max, [6] the rate
 * chosen (0 = the BIOS default timing), [7] AX of the set with the timing block, [8] AX of the plain set after it failed (0xFFFFFFFF none),
 * [9] 1 = done through vesa_bios_V86, [10] success, [11] vx_timing_fixed after it, [12] vx_timing_hz). pr_arm: one-shot test switches
 * for the next mode set, armed by VXPROBE within the last 20 s (bit 0 VX_fix_timing takes its failure path, review #107; bit 1 the set
 * with the timing block goes through vesa_bios_V86, review #172; v10.338: bit 2 a GTF 60 Hz timing block at any mode). cur_sw_shows: software cursor draws (review #80). */
static DWORD pr_set[13], pr_arm, pr_arm_ms, cur_sw_shows;
/* v10.339 (board, VXPROBE v1.4 bios: one ChangeDisplaySettings made about three mode sets; the first took the one-shot switch and the
 * later ones set the BIOS's own timing again): the switches hold for every set until VXPROBE disarms them (or 20 s pass); pr_hist keeps
 * the last 8 sets' records (pr_set[1..12]) */
static DWORD pr_hist[8][12], pr_hist_n;
static DWORD pr_take(DWORD bit) {DWORD on=(pr_arm&bit)&&(DWORD)(Get_System_Time()-pr_arm_ms)<20000UL; return on?1UL:0UL;}
static void cursor_hw_off(void)
{
	VX2D_hwcur_hide();
	VX2D_ovl_hide(); /* v10.106 */
	cur_hw = FALSE; cur_hw_ok = FALSE; cur_hw_gen = 0xFFFFFFFFUL;
}

/* v10.253: ACCEL_MOUSE_FAST (default 1): a mouse move while the hardware cursor shows the current shape writes the position registers
 * at once (VX2D_hwcur_move), without the frame-buffer access bracket around it, whose show waited until every other access (an
 * asynchronous blit, a 3D draw to the screen, a Lock) had ended: the pointer lagged and then jumped while Tunnel ran. The general
 * path stays for the software cursor, a new shape, after an engine reset (cur_epoch), mode changes and a lost device. */
volatile uint32_t vx_mouse_fast=1;
BOOL VESA_cursor_fast_move(LONG x, LONG y)
{
	if(!vx_mouse_fast || !vx_direct || !vx2d_hwcur || !cur_hw || !cur_hw_ok || cur_drawn || vga_mode || act_mode < 0) return FALSE;
	if(cur_hw_gen != vx_cursor_gen || cur_epoch != vx2d_reset_epoch || !wram->regs.s.cursor_visible || VX2D_is_lost()) return FALSE;
	if(!VX2D_hwcur_move(x, y)) return FALSE;
	wram->regs.s.cursor_x = x; wram->regs.s.cursor_y = y;
	return TRUE;
}

/* v10.371 (board, GTA3, VXD3DMON v1.3: the synchronous end of every batch with a draw at offset 0 cost 21.5 microseconds a packet): 1 when a
 * 3D draw into the frame-buffer bytes [off, off + bytes) needs the access bracket (the cursor taken off and the draw finished before it is
 * put back): the software cursor is drawn now, or may be (no hardware cursor and the cursor visible), on the surface being shown
 * (hda->surface, where cursor_show draws), and the draw touches that surface. A flip chain's back buffer, a hidden cursor (games) and the
 * hardware cursor need none. Without the direct frame buffer (the shadow copy) the rule stays as before: offset 0. */
int VESA_draw_needs_exclusion(DWORD off, DWORD bytes)
{
	BOOL lk; int r;
	if(!vx_direct) return off == 0;
	if(!hda || !wram) return 1;
	lk = FBHDA_lock();
	r = (cur_drawn || (!cur_hw && wram->regs.s.cursor_visible)) &&
	    off < hda->surface + hda->pitch * hda->height && hda->surface < off + bytes;
	FBHDA_release(lk);
	return r;
}

/* Explicit native-test isolation. Every desktop copy reaches VESA_draw. */
volatile DWORD vx_native_display_hold=0;
volatile DWORD vx_native_display_skips=0;
int VESA_native_span(uint32_t *first,uint32_t *last)
{
 DWORD i,at,bytes,low=0xFFFFFFFFUL,high=0;
 if(vga_mode || act_mode<0 || !wblit.dst_pitch || !wblit.dst_h ||
    wblit.dst_h>vx_status.fb_bytes/wblit.dst_pitch) return 0;
 bytes=wblit.dst_pitch*wblit.dst_h;
 for(i=0;i<3;i++) if(wblit.dst[i].flat.ptr) {
  if(wblit.dst[i].flat.dw<vx_status.fb_linear) return 0;
  at=wblit.dst[i].flat.dw-vx_status.fb_linear;
  if(at>vx_status.fb_bytes || bytes>vx_status.fb_bytes-at) return 0;
  if(at<low) low=at;
  if(at+bytes>high) high=at+bytes;
 }
 if(low==0xFFFFFFFFUL) return 0;
 *first=low; *last=high; return 1;
}

static void VESA_draw(blit_t *blit);

/* from gtf.c */
void gtf_calc(DWORD w, DWORD h, DWORD freq, vesa_crtc_info_t *crtc_info);

/* vxd_mouse.vxd */
BOOL mouse_get_rect(DWORD *ptr_left, DWORD *ptr_top,
	DWORD *ptr_right, DWORD *ptr_bottom);

void vesa_bios_V86(CRS_32 *V86regs)
{
 CRS_32 saved;
 CRS_32 *pstate=&saved;
 VMCB_t *vm;
 CRS_32 *client;
 /* Save/Restore_Client_State operate on the current VM. Enter V86 mode
  * BEFORE installing request registers: the mode switch replaces the
  * protected-mode segment/stack image with the alternate V86 image.
  * Never install a whole caller state across that mode switch. */
 _asm mov edi,[pstate]
 VMMCall(Save_Client_State);
 VMMCall(Begin_Nest_V86_Exec);
 vm=(VMCB_t*)Get_Cur_VM_Handle();
 client=vm->CB_Client_Pointer;
 client->Client_EAX=V86regs->Client_EAX;
 client->Client_EBX=V86regs->Client_EBX;
 client->Client_ECX=V86regs->Client_ECX;
 client->Client_EDX=V86regs->Client_EDX;
 client->Client_ESI=V86regs->Client_ESI;
 client->Client_EDI=V86regs->Client_EDI;
 client->Client_EBP=V86regs->Client_EBP;
 client->Client_ES=V86regs->Client_ES;
 client->Client_EFlags &= ~0x401UL; /* BIOS input: carry and DF clear. */
 _asm mov eax,10h
 VMMCall(Exec_Int);
 /* Capture BIOS outputs before ending nested execution restores CS:IP. */
 _asm mov edi,[V86regs]
 VMMCall(Save_Client_State);
 VMMCall(End_Nest_Exec);
 _asm mov esi,[pstate]
 VMMCall(Restore_Client_State);
}

void __declspec(naked) vesa_bios(CRS_32 *V86regs)
{
 /* Exec_VxD_Int can change every general register, including EBP. Use
  * only ESP for the argument after the call, and save returned ESI before
  * using any register as an output pointer. The service pops the int #. */
 _asm {
  pushfd
  pushad
  mov esi,[esp+40]
  mov eax,[esi+0x1c]
  mov ebx,[esi+0x10]
  mov ecx,[esi+0x18]
  mov edx,[esi+0x14]
  mov edi,[esi+0x00]
  mov ebp,[esi+0x08]
  mov esi,[esi+0x04]
  push dword ptr 10h
 }
 VMMCall(Exec_VxD_Int);
 _asm {
  pushad
  pushfd
  mov edi,[esp+76]
  mov eax,[esp]
  mov [edi+0x2c],eax
  lea esi,[esp+4]
  mov ecx,8
  cld
  rep movsd
  add esp,36
  popad
  popfd
  ret
 }
}

void load_client_state(CRS_32 *V86regs)
{
//	_asm   mov       edi, [V86regs]
//	VMMCall(Save_Client_State);
	VMCB_t *vm = (VMCB_t*)Get_Cur_VM_Handle();
	memcpy(V86regs, vm->CB_Client_Pointer, sizeof(CRS_32));
}

static DWORD vesa_buf_v86;
static void *vesa_buf;
static vesa_palette_entry_t *vesa_pal;
static int vesa_pal_bits = 6;

static BOOL vesa_valid = FALSE;

static DWORD conf_dos_window = 0;
static DWORD conf_hw_double_buf = 1; /* much better performance on real HW */
static DWORD conf_mtrr = 1;

#define MODE_OFFSET 1024
#define CRTC_OFFSET 2048
#define PAL_OFFSET  3072 // pal size = 4*256


#define V86_SEG(_lin) ((_lin) >> 4)
#define V86_OFF(_lin) ((_lin) & 0xF)

#define LIN_FROM_V86(_flatptr) ((((_flatptr) >> 12) & 0xFFFF0UL) + ((_flatptr) & 0xFFFFUL))

#define VESA_SUCC(_r) (((_r).Client_EAX & 0xFFFF)==0x004F)

static void offset_calc(DWORD offset, DWORD *ox, DWORD *oy)
{
	DWORD ps = ((video_bpp+7)/8);
	*oy = offset/video_pitch;
	*ox = (offset % video_pitch)/ps;
}

static DWORD VESA_pitch(DWORD width, DWORD bpp)
{
	DWORD bp = (bpp + 7) / 8;
	return (bp * width + (FBHDA_ROW_ALIGN-1)) & (~((DWORD)FBHDA_ROW_ALIGN-1));
}

#define DSWAP(_a, _i, _j) {WORD tmp = _a[_i]; _a[_i] = _a[_j]; _a[_j] = tmp;}


DWORD vram_phy = 0;


void VESA_load_vbios_pm();

static char vesa_vxd_name[] = "CHROME9.VXD"; /* .580: the installed file's name (FBHDA vxdname) */
#include "vx855_vbe_init.inc"

static WORD *vbios32 = NULL;
static WORD vbios32_sel = 0;
static DWORD vbios32_mem = 0;

BOOL VESA_display_start_pm(BOOL vtrace, DWORD start_addr)
{
	BYTE cmd = vtrace ? 0x80 : 0;
	/* DOC: DX:CX is the 32 bit offset in display memory, aligned to a plane boundar,
	... but in 8+ bits per pixel modes this is the offset from the start of memory divided by 4 */
	WORD h1 = (start_addr >> 2) & 0xFFFF;
	WORD h2 = start_addr >> 18;

	if(vbios32 != NULL && vbios32[VESA_PMTABLE_OFF_DISPLAY_START])
	{
		//dbg_printf("VESA_display_start_pm ...");
		_asm
		{
			pushad
			push es
			mov eax, [vbios32]
			xor ebx, ebx
			mov bx,  [eax+2] /* VESA_PMTABLE_OFF_DISPLAY_START*2 */
			add eax, ebx
			mov bx, [vbios32_sel]
			or bx, bx
			jz skip_load_es
				mov es, bx
			skip_load_es:
			xor ebx, ebx
			xor ecx, ecx
			xor edx, edx
			mov bl,[cmd]
			mov cx,[h1]
			mov dx,[h2]
			call eax
			pop es
			popad
		};
		//dbg_printf("OK\n");
		
		return TRUE;
	}
	
	return FALSE;
}

void VESA_load_vbios_pm()
{
	CRS_32 regs;

	load_client_state(&regs);
	regs.Client_EAX = VESA_CMD_PM_ENTRY;
	regs.Client_EBX = 0;

	vesa_bios_V86(&regs);

	if(VESA_SUCC(regs))
	{
		WORD *bios_pm;
		WORD *ports;
		DWORD bios_pm_flat = regs.Client_ES * 0x10 + (regs.Client_EDI & 0xFFFF);
		dbg_printf("table_flat es=%lX, edi=%lX, code_size=%ld\n",
			regs.Client_ES, regs.Client_EDI, regs.Client_ECX & 0xFFFF);

		if(bios_pm_flat != 0)
		{
			DWORD code_size = regs.Client_ECX & 0xFFFF;
			DWORD code_pages = RoundToPages(code_size);

			bios_pm = (WORD*)bios_pm_flat;
			
			dbg_printf("set display off func: %X\n", bios_pm[VESA_PMTABLE_OFF_DISPLAY_START]);

			vbios32 = (WORD*)_PageAllocate(code_pages, PG_SYS, 0, 0x0, PAGE_ALLOC_MIN, PAGE_ALLOC_MAX, NULL, PAGEZEROINIT);
			if(vbios32 != NULL)
			{
				memset(vbios32, 0xFF, code_pages*P_SIZE);
				memcpy(vbios32, bios_pm, code_size);
				_PageModifyPermissions(((DWORD)vbios32) / P_SIZE, code_pages, 0, PC_USER | PC_WRITEABLE);
				dbg_printf("copy to vbios32 success\n");
			}
/*
			terror("Here is dump of pm32 table:\n");
			terrorf("vbios32[0] = %X\n", vbios32[0]);
			terrorf("vbios32[1] = %X\n", vbios32[1]);
			terrorf("vbios32[2] = %X\n", vbios32[2]);
			terrorf("vbios32[3] = %X\n", vbios32[3]);
			terrorf("code size = %d\n", code_size);
*/
			if(vbios32[VESA_PMTABLE_OFF_PORTS])
			{
				ports = vbios32 + vbios32[VESA_PMTABLE_OFF_PORTS];
				// ^ some BIOSes has this table after the code to copy
				
				while(*ports != 0xFFFF)
				{
					dbg_printf("port: %04X\n", *ports);
					//terrorf("port: = %X\n", *ports);
					ports++;
				}
				ports++;
				if(*ports != 0xFFFF)
				{
					DWORD mem_adr = *((DWORD*)ports);
					DWORD mem_size = ports[2];
					
					//terrorf("mem: = %X, size = %d\n", mem_adr, mem_size);
					
					vbios32_mem = _MapPhysToLinear(mem_adr, mem_size, 0);
					if(vbios32_mem != 0xFFFFFFFF)
					{
						DWORD hi  = 0;
						DWORD low = 0;
						DWORD selector;
						_BuildDescriptorDWORDs(mem_adr, RoundToPages(mem_adr), 0x92, 0x80, 0, &hi, &low);
	
						_Allocate_GDT_Selector(hi, low, 0, &selector, NULL);
						vbios32_sel = selector << 16;
					}
					
					dbg_printf("VESA PM need memory addr=0x%8X (size=%d)\n", mem_adr, mem_size);
				}
			}

		}
		else
		{
			dbg_printf("VESA_CMD_ADAPTER_INFO success but no table\n");
		}
	}
	else
	{
		dbg_printf("VESA_CMD_ADAPTER_INFO failed\n");
	}
}

BOOL VESA_valid()
{
	return vesa_valid;
}

void VESA_clear()
{
	/* v10.330 (review #99): the clear stays inside the mapped frame buffer (vram_size_virt: video memory, or the shadow); a mode whose
	 * height * pitch went past it wrote beyond the mapping */
	DWORD n = hda->stride;
	if(hda->system_surface >= hda->vram_size_virt) return;
	if(n > hda->vram_size_virt - hda->system_surface) n = hda->vram_size_virt - hda->system_surface;
	memset((BYTE*)hda->vram_pm32+hda->system_surface, 0, n);
	dbg_printf("Clear success\n");
}

static int VESA_modechoice(DWORD w, DWORD h, DWORD bpp)
{
 DWORD i;
 if(!vesa_valid) return vesa_modes_cnt;
 for(i=0; i<vesa_modes_cnt; i++)
  if(vesa_modes[i].width==w && vesa_modes[i].height==h &&
     vesa_modes[i].bpp==bpp) return i;
 return vesa_modes_cnt;
}

static DWORD vx_custom_timing = 1;
DWORD vesa_set_freq; /* .577 */
static DWORD VESA_freqchoice(int mode, DWORD rr_min, DWORD rr_max)
{
	int k;
	if(!vx_custom_timing) return 0;
	for(k = 0; k < VESA_MODE_MAX_FREQS; k++)
	{
		DWORD f = vesa_modes[mode].freqs[k];
		if(f == 0)
		{
			return 0; /* default */
		}
		
		if(f >= rr_min && (rr_max == 0 || f <= rr_max)) /* v10.330 (review #103): rr_max 0 is no upper bound; the lower one still holds */
		{
			return f;
		}
	}
	return 0;
}

BOOL VESA_validmode(DWORD w, DWORD h, DWORD bpp)
{
	DWORD mode = VESA_modechoice(w, h, bpp);

	if(mode < vesa_modes_cnt)
	{
		return TRUE;
	}

	return FALSE;
}


/* v10.45: the BIOS keeps the 1024x768 scan-out timing for every mode above 1024x768 (only the pitch changes),
 * so the screen shows the top-left 1024x768. After the BIOS mode set, read back the scanned size and, when it is
 * smaller than the mode, program IGA1 timing, fetch count and dot clock directly (GTF 60 Hz).
 * Register meanings were checked against the Windows XP dumps (1280x1024: 1688x1066, 108.1 MHz, dm151 dn5 dr2). */
static unsigned char vt_cr_get(unsigned short base, unsigned char i) { outp(base, i); return inp((unsigned short)(base+1)); }
static void vt_cr_put(unsigned short base, unsigned char i, unsigned char v) { outp(base, i); outp((unsigned short)(base+1), v); }
static void vt_cr_mask(unsigned short base, unsigned char i, unsigned char m, unsigned char v) { vt_cr_put(base, i, (unsigned char)((vt_cr_get(base, i) & ~m) | (v & m))); }
static void vt_sr_put(unsigned char i, unsigned char v) { outp(0x3C4, i); outp(0x3C5, v); }
static unsigned char vt_sr_get(unsigned char i) { outp(0x3C4, i); return inp(0x3C5); }

unsigned long vt_irq_save(void);
void vt_irq_restore(unsigned long flags);
#pragma aux vt_irq_save = "pushfd" "pop eax" "cli" value [eax] modify exact [eax];
#pragma aux vt_irq_restore = "push eax" "popfd" parm [eax] modify exact [];
DWORD vx_timing_fixed; /* 0 = untouched, 1 = reprogrammed, 2 = tried, still wrong */
static DWORD vx_timing_hz; /* v10.330 (review #168): the refresh the reprogrammed timing gives (DMT 60 Hz, GTF 60 Hz, or 1152x864's 75 Hz DMT) */
static DWORD vx_1152_cvt = 1; /* v10.307: 1152x864 at 60 Hz (CVT); 0 = the 75 Hz DMT timing it had before */

typedef struct { unsigned short w, h, ht, hss, hse, vt, vss, vse; unsigned long khz; unsigned char hneg, vneg; } vx_dmt_t;
/* VESA DMT 60 Hz timings: monitors expect these porches, a GTF block leaves the picture shifted */
static const vx_dmt_t vx_dmt[] = {
 {1280,1024,1688,1328,1440,1066,1025,1028,108000UL,0,0},
 {1280, 960,1688,1328,1440,1066,1025,1028,108000UL,0,0}, /* 960 lines do not fit this 900-line panel 1:1; the 1024-line timing makes the monitor scale it, so nothing is cropped */
 {1280, 800,1680,1352,1480, 831, 803, 809, 83500UL,1,0},
 {1280, 768,1664,1344,1472, 798, 771, 778, 79500UL,1,0},
 {1368, 768,1800,1440,1584, 795, 769, 772, 85250UL,0,0},
 {1440, 900,1904,1520,1672, 934, 903, 909,106500UL,1,0},
 {1152, 864,1520,1216,1336, 897, 867, 871, 81750UL,1,0}, /* v10.307: CVT 60 Hz (DMT has 1152x864 only at 75 Hz) */
 {1152, 864,1600,1216,1344, 900, 865, 868,108000UL,0,0}, /* DMT 75 Hz: registry TIMING_1152_CVT = 0 */
 {1280, 720,1650,1390,1430, 750, 725, 730, 74250UL,0,0},
 {1360, 768,1792,1424,1536, 795, 771, 777, 85500UL,0,0},
 {1366, 768,1792,1436,1579, 798, 771, 774, 85500UL,0,0},
 {1600, 900,1800,1624,1704,1000, 901, 904,108000UL,0,0},
 {1600,1200,2160,1664,1856,1250,1201,1204,162000UL,0,0},
 {1680,1050,2240,1784,1960,1089,1053,1059,146250UL,1,0},
 {1920,1080,2200,2008,2052,1125,1084,1089,148500UL,0,0},
 {1920,1200,2592,2056,2256,1245,1203,1209,193250UL,1,0}
};

static void VX_fix_timing(DWORD w, DWORD h, DWORD bpp)
{
	vesa_crtc_info_t ci;
	unsigned short base;
	unsigned long fl;
	unsigned char hneg, vneg;
	unsigned char misc, unlock, cr7, cr35, sr1d, sidx, cidx, cr11;
	DWORD hd, vd, ht, vt, hss, hse, vss, vse, hbe, vbs, vtr, vsr;
	DWORD khz, best = 0, err = 0xFFFFFFFFUL, dm, dn, dr, bdm = 0, bdn = 0, bdr = 0, fetch, bytes;

	if(pr_take(1)) { vx_timing_fixed = 2; return; } /* v10.333 (VXPROBE timing, review #107): the failure path, once */
	/* v10.142 (review O20, known bug P09): the timing and the PLL are worked out before interrupts go off; only the register writes run with them off */
	{
		unsigned k; int found = 0;
		hneg = 1; vneg = 0;
		for(k = 0; k < sizeof(vx_dmt)/sizeof(vx_dmt[0]); k++)
			if(vx_dmt[k].w == w && vx_dmt[k].h == h && !(w == 1152 && h == 864 && (vx_dmt[k].khz == 81750UL) != (vx_1152_cvt != 0)))
			{
				ci.HorizontalTotal = vx_dmt[k].ht; ci.HorizontalSyncStart = vx_dmt[k].hss; ci.HorizontalSyncEnd = vx_dmt[k].hse;
				ci.VerticalTotal = vx_dmt[k].vt; ci.VerticalSyncStart = vx_dmt[k].vss; ci.VerticalSyncEnd = vx_dmt[k].vse;
				ci.PixelClock = vx_dmt[k].khz * 1000UL; hneg = vx_dmt[k].hneg; vneg = vx_dmt[k].vneg; found = 1; break;
			}
		if(!found) gtf_calc(w, h, 60, &ci);
	}
	ht = ci.HorizontalTotal / 8; hss = ci.HorizontalSyncStart / 8; hse = ci.HorizontalSyncEnd / 8;
	vt = ci.VerticalTotal; vss = ci.VerticalSyncStart; vse = ci.VerticalSyncEnd;
	khz = ci.PixelClock / 1000UL;
	if(ht < 8 || vt < h + 2 || khz < 10000UL) { vx_timing_fixed = 2; return; }

	/* dot clock: f = 14318 * dm / (dn << dr), keep the VCO (14318*dm/dn) between 300 and 600 MHz (XP uses 432, BIOS 521); the first minimum in dr 3..0 / dn 2..12 order wins ties (v10.145: the comment used to say the highest VCO).
	 * v10.142 (review O20): f grows with dm by at least 149 kHz a step (dn << dr <= 96), so the best dm for each dn/dr is q = floor(khz * (dn << dr) / 14318) or q + 1
	 * (f(q) <= khz < f(q + 1)), clamped to the legal range; visiting those two in ascending order with the same strict comparison picks exactly what the full
	 * search of every dm picked (88 candidates instead of about 6500; checked on the host for every clock from 10 to 400 MHz). */
	for(dr = 4; dr-- > 0; )
		for(dn = 2; dn <= 12; dn++)
		{
			DWORD lo = (300000UL * dn + 14317UL) / 14318UL, hi = (600000UL * dn) / 14318UL, /* v10.302 (review): lo rounded up, so the VCO is never below 300 MHz (it went to 293.5) */ q = (khz * (dn << dr)) / 14318UL, c[2], k;
			if(lo < 2) lo = 2;
			if(hi > 1023) hi = 1023;
			if(lo > hi) continue;
			c[0] = q < lo ? lo : q > hi ? hi : q;
			c[1] = q + 1 < lo ? lo : q + 1 > hi ? hi : q + 1;
			for(k = 0; k < 2; k++)
			{
				DWORD f, e;
				dm = c[k];
				if(k == 1 && dm == c[0]) break;
				f = (14318UL * dm) / (dn << dr); e = (f > khz) ? f - khz : khz - f;
				if(e < err) { err = e; best = f; bdm = dm; bdn = dn; bdr = dr; }
			}
		}
	if(!best || err > khz / 50) { vx_timing_fixed = 2; return; }

	bytes = (bpp + 7) / 8;
	fetch = (w * bytes + 15) / 16 + 4;

	fl = vt_irq_save();
	sidx = inp(0x3C4); /* v10.330 (review #105): read before SR10 is selected (it read back 0x10, not the caller's index) */
	unlock = vt_sr_get(0x10);
	vt_sr_put(0x10, (unsigned char)(unlock | 1));
	misc = inp(0x3CC);
	base = (misc & 1) ? 0x3D4 : 0x3B4;
	cidx = inp(base); /* v10.309 (review): the index registers as they were, as every other path leaves them */
	cr7 = vt_cr_get(base, 0x07); cr35 = vt_cr_get(base, 0x35);
	hd = ((DWORD)vt_cr_get(base, 0x01) + 1) * 8;
	vd = (DWORD)vt_cr_get(base, 0x12) | ((DWORD)((cr7 >> 1) & 1) << 8) | ((DWORD)((cr7 >> 6) & 1) << 9) | ((DWORD)((cr35 >> 2) & 1) << 10);
	vd += 1;
	/* always program: a BIOS timing that matches the size may still have the wrong porches or fetch */
	(void)hd; (void)vd;

	vt_sr_put(0x01, (unsigned char)(vt_sr_get(0x01) | 0x20)); /* screen off while retiming */
	cr11 = vt_cr_get(base, 0x11);                             /* v10.330 (review #106): CR11's lock bit as the BIOS left it, put back below */
	vt_cr_mask(base, 0x11, 0x80, 0x00);                       /* unlock CR00..07 */

	vtr = vt - 2; vsr = vss - 1; vbs = h - 1; hbe = (ht - 1) & 0x7F;
	vt_cr_put(base, 0x00, (unsigned char)((ht - 5) & 0xFF));
	vt_cr_mask(base, 0x36, 0x08, (unsigned char)((((ht - 5) >> 8) & 1) << 3));
	vt_cr_put(base, 0x01, (unsigned char)((w + 7) / 8 - 1));
	vt_cr_put(base, 0x02, (unsigned char)((w + 7) / 8 - 1));
	vt_cr_mask(base, 0x03, 0x1F, (unsigned char)(hbe & 0x1F));
	vt_cr_mask(base, 0x05, 0x80, (unsigned char)(((hbe >> 5) & 1) << 7));
	vt_cr_mask(base, 0x33, 0x20, (unsigned char)(((hbe >> 6) & 1) << 5));
	vt_cr_put(base, 0x04, (unsigned char)(hss & 0xFF));
	vt_cr_mask(base, 0x05, 0x1F, (unsigned char)(hse & 0x1F));
	vt_cr_mask(base, 0x33, 0x10, (unsigned char)(((hss >> 8) & 1) << 4)); /* hsync start bit 8 (1920x1200 needs it) */

	vt_cr_put(base, 0x06, (unsigned char)(vtr & 0xFF));
	vt_cr_put(base, 0x10, (unsigned char)(vsr & 0xFF));
	vt_cr_mask(base, 0x11, 0x0F, (unsigned char)((vse - 1) & 0x0F));
	vt_cr_put(base, 0x12, (unsigned char)(vbs & 0xFF));
	vt_cr_put(base, 0x15, (unsigned char)(vbs & 0xFF));
	vt_cr_put(base, 0x16, (unsigned char)((vtr + 1) & 0xFF));
	vt_cr_mask(base, 0x07, 0xEF, (unsigned char)(((vtr >> 8) & 1) | (((vbs >> 8) & 1) << 1) | (((vsr >> 8) & 1) << 2) | (((vbs >> 8) & 1) << 3) |
		(((vtr >> 9) & 1) << 5) | (((vbs >> 9) & 1) << 6) | (((vsr >> 9) & 1) << 7)) & 0xEF);
	vt_cr_mask(base, 0x09, 0x20, (unsigned char)(((vbs >> 9) & 1) << 5));
	vt_cr_mask(base, 0x35, 0x0F, (unsigned char)(((vtr >> 10) & 1) | (((vsr >> 10) & 1) << 1) | (((vbs >> 10) & 1) << 2) | (((vbs >> 10) & 1) << 3)));

	sr1d = vt_sr_get(0x1D);
	vt_sr_put(0x1C, (unsigned char)(fetch & 0xFF));
	vt_sr_put(0x1D, (unsigned char)((sr1d & ~3) | ((fetch >> 8) & 3)));

	{extern unsigned long vx_chip_id;
	if(vx_chip_id == 0x1122UL)
	{	/* .579: VX800 / VX820 hold the multiplier and the divider less two (OpenChrome ViaSetDotclock: "the VX855 and VX900 do not modify dm/dn, but
		 * earlier chipsets do"), SR46 without its top bit. Board (.585, VXTIME): eight sizes right; the BIOS's own words there read back in the same form */
		vt_sr_put(0x44, (unsigned char)((bdm - 2) & 0xFF));
		vt_sr_put(0x45, (unsigned char)(0x80 | ((bdr & 7) << 2) | (((bdm - 2) >> 8) & 3)));
		vt_sr_put(0x46, (unsigned char)((bdn - 2) & 0x7F));
		/* .583: and the PLL is restarted so it takes the new values (OpenChrome ViaSetPrimaryDotclock: SR40 bit 1 set, then cleared) */
		vt_sr_put(0x40, (unsigned char)(vt_sr_get(0x40) | 0x02)); vt_sr_put(0x40, (unsigned char)(vt_sr_get(0x40) & ~0x02));
	}
	else
	{
		vt_sr_put(0x44, (unsigned char)(bdm & 0xFF));
		vt_sr_put(0x45, (unsigned char)(0x80 | ((bdr & 7) << 2) | ((bdm >> 8) & 3)));
		vt_sr_put(0x46, (unsigned char)(0x80 | bdn));
	}}

	misc = (unsigned char)((misc & 0x3F) | (hneg ? 0x40 : 0) | (vneg ? 0x80 : 0));
	outp(0x3C2, misc);

	vt_cr_mask(base, 0x11, 0x80, cr11);
	vt_sr_put(0x01, (unsigned char)(vt_sr_get(0x01) & ~0x20));
	vt_sr_put(0x10, unlock);
	outp(base, cidx); outp(0x3C4, sidx);
	vx_timing_fixed = 1;
	vx_timing_hz = (khz * 1000UL + (ci.HorizontalTotal * ci.VerticalTotal) / 2) / (ci.HorizontalTotal * ci.VerticalTotal); /* v10.330 (review #168) */
	vt_irq_restore(fl);
	dbg_printf("timing fixed %dx%d: %d kHz dm%d dn%d dr%d\n", w, h, best, bdm, bdn, bdr);
}

BOOL VESA_setmode_phy(DWORD w, DWORD h, DWORD bpp, DWORD rr_min, DWORD rr_max)
{
	DWORD mode = 0;
	DWORD num_buffers = 0;
	DWORD video_stride;
	DWORD freq;
	CRS_32 regs;
	vesa_crtc_info_t *crtc_info = NULL;
	DWORD refresh = ASYNC_DEFAULT;

	vx_status.set_attempts++;
 mode = VESA_modechoice(w, h, bpp);
 if(mode >= vesa_modes_cnt) { vx_status.failure=VX_MODE_REJECTED; return FALSE; }
		
	freq = VESA_freqchoice(mode, rr_min, rr_max);
	if(pr_take(4) && !freq) freq = 60; /* v10.338 (VXPROBE bios, review #172): the timing block at a mode up to 1024x768 too, once */
	if(pr_set[0]) {DWORD k; for(k = 0; k < 12; k++) pr_hist[pr_hist_n & 7][k] = pr_set[1 + k]; pr_hist_n++;} /* v10.339: the set before */
	pr_set[0]++; pr_set[1] = w; pr_set[2] = h; pr_set[3] = bpp; pr_set[4] = rr_min; pr_set[5] = rr_max; pr_set[6] = freq; /* v10.333 */
	vesa_set_freq = freq; /* .577: the rate asked for with this mode (0 = the BIOS's own), for DirectDraw's monitor frequency (VX_flip_hz) */
	pr_set[7] = pr_set[8] = 0xFFFFFFFFUL; pr_set[9] = 0; pr_set[10] = 0; pr_set[11] = pr_set[12] = 0;
	
	if(freq != 0)
	{
		crtc_info = (vesa_crtc_info_t*)((char*)vesa_buf + CRTC_OFFSET);
		gtf_calc(vesa_modes[mode].width, vesa_modes[mode].height, freq, crtc_info);
	}

	load_client_state(&regs);

	// set mode
	regs.Client_EAX = VESA_CMD_MODE_SET;
	regs.Client_EBX = vesa_modes[mode].mode_id | VESA_SETMODE_LFB;
	if(crtc_info)
	{
		regs.Client_EBX |= VESA_SETMODE_CRTC; /* without bit 11 the BIOS ignores the timing block */
		regs.Client_ES  = V86_SEG(vesa_buf_v86+CRTC_OFFSET);
		regs.Client_EDI = V86_OFF(vesa_buf_v86+CRTC_OFFSET);
		
		if(pr_take(2)) {pr_set[9] = 1; vesa_bios_V86(&regs);} else vesa_bios(&regs); /* v10.333 (VXPROBE refresh, review #172) */
		pr_set[7] = regs.Client_EAX & 0xFFFF;
		if(!VESA_SUCC(regs))
		{
			/* fail to set mode, try with default frequency */
			load_client_state(&regs);
			regs.Client_EAX = VESA_CMD_MODE_SET;
			regs.Client_EBX = vesa_modes[mode].mode_id | VESA_SETMODE_LFB;
			regs.Client_ES = 0;
			regs.Client_EDI = 0;
			dbg_printf("freq=%d failed, try default\n", freq);
			vesa_bios(&regs);
			pr_set[8] = regs.Client_EAX & 0xFFFF;
		}
		else
		{
			refresh = 1000 / freq;
		}
	}
	else
	{
		regs.Client_ES = 0;
		regs.Client_EDI = 0;
		vesa_bios(&regs);
	}
	
	vx_status.last_bios_ax=regs.Client_EAX & 0xFFFF;
 if(VESA_SUCC(regs))
 {
  DWORD test_x;
		DWORD test_y;

		VESA_HIRES_enable();
		vx_timing_fixed = 0; vx_timing_hz = 0; /* v10.330 (review #108): this mode's result, not the mode before's */
		if(vx_custom_timing && (w > 1024 || h > 768)) VX_fix_timing(w, h, bpp);
		/* v10.330 (review #168): above 1024x768 the timing is the driver's own (60 Hz, or 1152x864 at 75 Hz with TIMING_1152_CVT = 0), not the
		 * rate asked of the BIOS: the asynchronous refresh interval follows what is scanned out */
		if(vx_timing_fixed == 1 && vx_timing_hz) refresh = 1000 / vx_timing_hz;
		pr_set[10] = 1; pr_set[11] = vx_timing_fixed; pr_set[12] = vx_timing_hz; /* v10.333 */
		vx_status.set_successes++;
		vx_status.failure=VX_OK;
		dbg_printf("SET success: %d %d %d\n", w, h, bpp);
		hda->width  = w;
		hda->height = h;
		hda->bpp    = bpp;
		hda->pitch  = vx_direct ? vesa_modes[mode].pitch : VESA_pitch(w, bpp); /* direct mode must match the BIOS scanout pitch */
		CUR_SET(FALSE); /* the mode set cleared the screen */
		cursor_hw_off(); /* v10.105: the mode set may have cleared the hardware cursor registers */
		hda->stride = h * hda->pitch;
		VX2D_gamma_load(NULL); /* v10.123: the gamma table again (the mode set reset CR33 and the DAC), or the bit cleared at 8 bpp */
		hda->surface = 0;
		
		video_bpp    = vesa_modes[mode].bpp;
		video_pitch  = vesa_modes[mode].pitch;
		video_stride = h * video_pitch;

		/* set DAC to 8BIT if supported */
		if(bpp <= 8)
		{
			if((vesa_caps & VESA_CAP_DAC8BIT) != 0)
			{
				/* set DAC to 8 bpp */
				regs.Client_EAX = VESA_CMD_PALETTE_FORMAT;
				regs.Client_EBX = VESA_DAC_SETFORMAT | VESA_DAC_SET_8BIT;
				vesa_bios(&regs);
				if(VESA_SUCC(regs))
				{
					vesa_pal_bits = (regs.Client_EBX >> 8) & 0xFF;
				}
				else
				{
					vesa_pal_bits = 6;
				}
			}
			else
			{
				vesa_pal_bits = 6;
			}
		}
		num_buffers = 1;

		/* I found there is some deadlock hazard on V86 or PM16 ints,
		 * and when is called on very frame there is big chance on it.
		 * This is why HW double/tripple buffering is available only
		 * when there is PM32 procedure in VBIOS.
		 */
		if(vbios32 && vbios32[VESA_PMTABLE_OFF_DISPLAY_START])
		{
			if(conf_hw_double_buf > 0)
			{
				if(conf_hw_double_buf >= 2)
				{
					/* test if HW flip and vtrace supported */
					offset_calc(video_stride, &test_x, &test_y);
					regs.Client_EAX = VESA_CMD_DISPLAY_START;
					regs.Client_EBX = VESA_DISPLAYSTART_VTRACE;
					regs.Client_ECX = test_x;
					regs.Client_EDX = test_y;
					vesa_bios(&regs);
					if(VESA_SUCC(regs))
					{
						num_buffers = 3;
	
						regs.Client_EAX = VESA_CMD_DISPLAY_START;
						regs.Client_EBX = VESA_DISPLAYSTART_SET;
						regs.Client_ECX = 0;
						regs.Client_EDX = 0;
						vesa_bios(&regs);
					}
				}
	
				if(num_buffers <= 1)
				{
					/* test if HW flip supported */
					offset_calc(video_stride, &test_x, &test_y);
					regs.Client_EAX = VESA_CMD_DISPLAY_START;
					regs.Client_EBX = VESA_DISPLAYSTART_SET;
					regs.Client_ECX = test_x;
					regs.Client_EDX = test_y;
					vesa_bios(&regs);
	
					if(VESA_SUCC(regs))
					{
						num_buffers = 2;
	
						regs.Client_EAX = VESA_CMD_DISPLAY_START;
						regs.Client_EBX = VESA_DISPLAYSTART_SET;
						regs.Client_ECX = 0;
						regs.Client_EDX = 0;
						vesa_bios(&regs);
					}
				}
			}
		}

		hda->system_surface = 0;
		hda->surface        = 0;
		if(num_buffers >= 3)
		{
			hda->flags |= FB_SUPPORT_VSYNC;
		}
		else
		{
			hda->flags &= ~FB_SUPPORT_VSYNC;
		}
		
		/* set VRAM destination */
		wblit.base.ptr = hda->vram_phylin;
		wblit.dst[0].flat.ptr = hda->vram_phylin;
		wblit.dst[1].flat.ptr = NULL;
		wblit.dst[2].flat.ptr = NULL;
		wblit.dst_w     = vesa_modes[mode].width;
		wblit.dst_h     = vesa_modes[mode].height;
		wblit.dst_pitch = vesa_modes[mode].pitch;

		switch(vesa_modes[mode].bpp)
		{
			case 8:  wblit.dst_mode = MODE_8;  break;
			case 15: wblit.dst_mode = MODE_15; break;
			case 16: wblit.dst_mode = MODE_16; break;
			case 24: wblit.dst_mode = MODE_24; break;
			case 32: wblit.dst_mode = MODE_32; break;
		}
	
		if(wblit.dst_w > w || wblit.dst_h > h)
		{
			DWORD scans = wblit.dst_w/w;
			if((scans*h) > wblit.dst_h)
			{
				scans = wblit.dst_h/h; /* v10.330: the height's factor (it divided the width: 640x480 in 1280x720 kept 2, 960 rows > 720) */
			}
			if(!scans) scans = 1;
			
			wblit.dst_scans = scans;
			wblit.dst_padx  = (wblit.dst_w - scans*w) / 2;
			wblit.dst_pady  = (wblit.dst_h - scans*h) / 2;	
		}
		else
		{
			wblit.dst_scans = 1;
			wblit.dst_padx  = 0;
			wblit.dst_pady  = 0;
		}
		
		/* set WRAM source */
		wram->regs.s.width  = w;
		wram->regs.s.height = h;
		switch(bpp)
		{
			case 8:  wram->regs.s.mode = MODE_8;  break;
			case 15: wram->regs.s.mode = MODE_15; break;
			case 16: wram->regs.s.mode = MODE_16; break;
			case 24: wram->regs.s.mode = MODE_24; break;
			case 32: wram->regs.s.mode = MODE_32; break;
		}
		wram->regs.s.pitch = hda->pitch;

		act_mode = mode;
  vx_status.stage=6;
  VX_flip_modeset(); /* v10.63: commit display start 0 through CR34 after every mode set */
  vx_status.active_mode=vesa_modes[mode].mode_id;
  vx_status.width=w; vx_status.height=h; vx_status.bpp=bpp;
  vx_status.shadow_pitch=hda->pitch; vx_status.video_pitch=video_pitch;

		/* check if we have enought memory for all buffers */
		if(num_buffers >= 2)
		{
			DWORD pages = (video_stride + P_SIZE - 1) / P_SIZE;
			DWORD lines = ((pages * P_SIZE) + video_pitch - 1) / video_pitch;
			DWORD page_stride = lines * video_pitch;

			if(hda->vram_size >= (page_stride + video_stride))
			{
				wblit.dst[1].flat.dw = wblit.dst[0].flat.dw + page_stride;
			}
			else
			{
				num_buffers = 1;
			}
			
			if(num_buffers >= 3)
			{
				if(hda->vram_size >= ((page_stride*2) + video_stride))
				{
					wblit.dst[2].flat.dw = wblit.dst[1].flat.dw + page_stride;
				}
				else
				{
					num_buffers = 2;
				}
			}
		}
		
		async_blit_settime(refresh);

		dbg_printf("mode set, mode_id=%d, i=%d, pitch=%d, buffers=%d\n", vesa_modes[mode].mode_id, mode, wram->regs.s.pitch, num_buffers);
		return TRUE;
	}
	else
	{
		vx_status.failure=VX_BIOS_MODE_FAILED;
		dbg_printf("vbios fail: eax=0x%lX\n", regs.Client_EAX);
	}

	dbg_printf("fail to set %ld %ld %ld\n", w, h, bpp);
	return FALSE;
}

extern unsigned long vx_mtrr_called, vx_mtrr_ver, vx_mtrr_status, vx_mtrr_base, vx_mtrr_size, vx_mtrr_conf;
static void mtrr_setup()
{
	if(conf_mtrr && !mttr_was_set)
	{
		if(vram_phy > 1*1024*1024)
		{
			vx_mtrr_ver = MTRR_GetVersion(); vx_mtrr_conf = conf_mtrr;
			if(vx_mtrr_ver)
			{
				dbg_printf("MTRR supported\n");
				dbg_printf("MTRR_SetPhysicalCacheTypeRange(%lX, 0, %ld, %ld)\n", vram_phy, hda->vram_size, MTRR_FRAMEBUFFERCACHED);
				vx_mtrr_status = MTRR_SetPhysicalCacheTypeRange(vram_phy, 0, hda->vram_size, MTRR_FRAMEBUFFERCACHED); /* v10.68: keep the answer for VX2MEM */
				vx_mtrr_called = 1; vx_mtrr_base = vram_phy; vx_mtrr_size = hda->vram_size;
				mttr_was_set = (vx_mtrr_status == 0); /* v10.139 (review F16): only a successful setup stops the retries at later mode sets */
			}
			else
			{
				dbg_printf("MTRR unsupported\n");
			}
		}
	}
}

static void VESA_setptr(DWORD offset, BOOL triplebuf)
{
	if(!VESA_display_start_pm(triplebuf, offset))
	{
		CRS_32 regs;
		DWORD off_x, off_y;
	
		offset_calc(offset, &off_x, &off_y);
	
		load_client_state(&regs);
	
		regs.Client_EAX = VESA_CMD_DISPLAY_START;
		regs.Client_EBX = triplebuf ? VESA_DISPLAYSTART_VTRACE : VESA_DISPLAYSTART_SET;
		regs.Client_ECX = off_x;
		regs.Client_EDX = off_y;
	
		vesa_bios(&regs);
	}
}

BOOL VESA_setmode(DWORD w, DWORD h, DWORD bpp, DWORD rr_min, DWORD rr_max)
{
	BOOL valid,lk;
	{extern volatile uint32_t vx3dn_rt_why;vx3dn_rt_why=6;}if(VX3DN_busy())return FALSE; /* preserve v10.88 2D-only recovery; fence the native 3D owner */
	{extern void VX3DN_ring_stop(void);VX3DN_ring_stop();} /* v10.391: a parked ring stopped */
	
	VX2D_sync(); /* v10.139 (review F01): finish a pending asynchronous blit while the old mode is still valid */
	lk = FBHDA_lock();
	valid = VESA_setmode_phy(w, h, bpp, rr_min, rr_max);
	/* v10.330 (review #98, #64): the clear and the cursor's reset inside the mode set's exclusion; a cursor drawn between the release and
	 * the clear (another thread waiting on the semaphore) was cleared away while it counted as drawn */
	if(valid)
	{
		mtrr_setup();
		VESA_clear();
		mouse_invalidate();
	}
	FBHDA_release(lk);
	return valid;
}

void FBHDA_palette_set(unsigned char index, DWORD rgb)
{
	wram->palette[index].dw = rgb; /* v10.71: the shadow keeps the GDI palette whatever the mode (DirectDraw restores from it) */
	if(cur_pal_used && hda->bpp == 8) {extern volatile unsigned long vx_cursor_gen; vx_cursor_gen++; cur_pal_used = FALSE;} /* v10.262: the hardware cursor's colours came from the palette: made again at the next show */
	if(wblit.dst_mode != MODE_8)
	{
		return;
	}
	if(vga_mode) return; /* v10.330 (review #197): a DOS box or text mode owns the DAC; the shadow above still takes the colour */
	
	if((vesa_caps & VESA_CAP_NONVGA) == 0)
	{
		/* if mode is VGA compatible, we can use VGA regitry */
		outp(VGA_DAC_W_INDEX, index);    /* Set starting index. */
		if(vesa_pal_bits == 8)
		{
			outp(VGA_DAC_DATA,   (rgb >> 16) & 0xFF);
			outp(VGA_DAC_DATA,   (rgb >>  8) & 0xFF);
			outp(VGA_DAC_DATA,    rgb        & 0xFF);
		}
		else
		{
			outp(VGA_DAC_DATA,   (rgb >> 18) & 0x3F);
			outp(VGA_DAC_DATA,   (rgb >> 10) & 0x3F);
			outp(VGA_DAC_DATA,   (rgb >>  2) & 0x3F);
		}
	}
	else
	{
		CRS_32 regs;
		DWORD v86_ptr = vesa_buf_v86+PAL_OFFSET+4*index;

		if(vesa_pal_bits == 8)
		{
			vesa_pal[index].Red     = (rgb >> 16) & 0xFF;
			vesa_pal[index].Green   = (rgb >>  8) & 0xFF;
			vesa_pal[index].Blue    =  rgb        & 0xFF;
		}
		else
		{
			vesa_pal[index].Red     = (rgb >> 18) & 0x3F;
			vesa_pal[index].Green   = (rgb >> 10) & 0x3F;
			vesa_pal[index].Blue    = (rgb >>  2) & 0x3F;
		}

		load_client_state(&regs);
		regs.Client_EAX = VESA_CMD_PALETTE_DATA;
		regs.Client_EBX = VESA_RAMDAC_DATA_SET;
		regs.Client_ECX = 1;
		regs.Client_EDX = index;
		regs.Client_ES  = V86_SEG(v86_ptr);
		regs.Client_EDI = V86_OFF(v86_ptr);
		//regs.Client_EDI = ((DWORD)vesa_buf)+PAL_OFFSET+4*index;
		
		vesa_bios_V86(&regs);
	}
}

static void VESA_draw(blit_t *blit)
{
	if(vx_direct) {blit->num_changes = 0; return;}
	if(vx_native_display_hold) {vx_native_display_skips++; return;}
	if(vga_mode) return;
	if(act_mode < 0) return;

	if(blit->num_changes)
	{
		vx_status.draws++;
		if(blit->dst[1].flat.ptr == NULL && blit->dst[2].flat.ptr == NULL)
		{
			/* single buffer, write only changes */
			wram_blit(&wblit);
		}
		else if(blit->dst[2].flat.ptr == NULL)
		{
			/* double buffer = write full frame
			
				dst[0] <- visible
				dst[1] <- CPU write
			*/
			DWORD offset;
			{
				void *new_ptr = blit->dst[1].flat.ptr;
				blit->dst[1].flat.ptr = blit->dst[0].flat.ptr;
				blit->dst[0].flat.ptr = new_ptr;
			}
			offset = blit->dst[0].flat.dw - blit->base.dw;
			blit->src_sx = 0;
			blit->src_sy = 0;
			blit->src_ex = hda->width;
			blit->src_ey = hda->height;
			wram_blit(&wblit);
			
			VESA_setptr(offset, FALSE);
		}
		else
		{
			/* triple buffer, write full frame
				dst[0] <- unvisible -> visible (swap on VSYNC)
				dst[1] <- CPU write
				dst[2] <- visible -> unvisible ( will be on next VSYNC)
			*/
			DWORD offset;
			{
				void *old_ptr = blit->dst[0].flat.ptr;
				blit->dst[0].flat.ptr = blit->dst[1].flat.ptr;
				blit->dst[1].flat.ptr = blit->dst[2].flat.ptr;
				blit->dst[2].flat.ptr = old_ptr;
			}
			offset = blit->dst[0].flat.dw - blit->base.dw;
			blit->src_sx = 0;
			blit->src_sy = 0;
			blit->src_ex = hda->width;
			blit->src_ey = hda->height;
			wram_blit(&wblit);
			
			VESA_setptr(offset, TRUE);
		}
		blit->num_changes = 0;
	}
}

/* ---- direct VRAM software cursor: AND/XOR masks composited into the screen, saved pixels restored on hide ---- */
static BYTE cur_save[CURSOR_DMAX*CURSOR_DMAX*4];
static LONG cur_sx, cur_sy, cur_ex, cur_ey; /* screen rectangle covered while drawn */

static DWORD dpx_bytes(void)
{
	switch(hda->bpp) {case 32: return 4; case 24: return 3; case 16: case 15: return 2; case 8: return 1;}
	return 0;
}

static DWORD dpx_read(BYTE *p)
{
	DWORD v;
	switch(hda->bpp)
	{
		case 8: return *p; /* v10.71: palette index; the cursor masks hold indices in the direct path */
		case 32: return *(DWORD*)p;
		case 24: return ((DWORD)p[2] << 16) | ((DWORD)p[1] << 8) | p[0];
		case 16: v = *(WORD*)p; return ((v & 0xF800) << 8) | ((v & 0x07E0) << 5) | ((v & 0x001F) << 3);
		case 15: v = *(WORD*)p; return ((v & 0x7C00) << 9) | ((v & 0x03E0) << 6) | ((v & 0x001F) << 3);
	}
	return 0;
}

static void dpx_write(BYTE *p, DWORD px)
{
	switch(hda->bpp)
	{
		case 8: *p = (BYTE)px; break;
		case 32: *(DWORD*)p = px; break;
		case 24: p[0] = (BYTE)px; p[1] = (BYTE)(px >> 8); p[2] = (BYTE)(px >> 16); break;
		case 16: *(WORD*)p = (WORD)(((px & 0xF80000) >> 8) | ((px & 0xFC00) >> 5) | ((px & 0xF8) >> 3)); break;
		case 15: *(WORD*)p = (WORD)(((px & 0xF80000) >> 9) | ((px & 0xF800) >> 6) | ((px & 0xF8) >> 3)); break;
	}
}

static void cursor_hide(void)
{
	DWORD b, y, rowbytes;
	BYTE *base;
	if(cur_hw) return; /* v10.105 */
	if(VX2D_native_fault()) return; /* The hung native owner may still access VRAM; retain v10.140's restore for ordinary 2D loss. */
	if(!cur_drawn) return; /* v10.140: the restore is a plain copy (no hda_sem, no engine), so it runs even while the engine is lost; skipping it left the sprite burned in and pasted a stale save after recovery. Only cursor_show refuses while lost. */
	CUR_SET(FALSE);
	b = dpx_bytes();
	if(!b || !hda->vram_pm32) return;
	base = (BYTE*)hda->vram_pm32 + hda->surface;
	rowbytes = (DWORD)(cur_ex - cur_sx) * b;
	for(y = 0; y < (DWORD)(cur_ey - cur_sy); y++)
		memcpy(base + (DWORD)(cur_sy + (LONG)y) * hda->pitch + (DWORD)cur_sx * b, cur_save + y * rowbytes, rowbytes);
}

/* v10.262: the hardware cursor on the 8bpp desktop. The expanded XOR mask holds palette indices there (colour cursors, BitsPixel 8;
 * vxd_mouse_conv.h) or FFFFFFFF/0 (monochrome ones, BitsPixel 1): the cursor units take colours, so each index goes through the GDI
 * palette (wram->palette, what FBHDA_palette_set keeps). A shape that used the palette is made again after a palette change
 * (cur_pal_used: FBHDA_palette_set moves vx_cursor_gen on). */
static DWORD cur_x8[CURSOR_DMAX * CURSOR_DMAX];
extern DWORD vx_cursor_bits; /* v10.330: the shape's XOR bits a pixel (vxd_mouse.c) */
static const uint32_t *cursor_xor_colours(void)
{
	DWORD i, v;
	if(hda->bpp != 8) return (const uint32_t *)wram->cursor.xormask;
	cur_pal_used = FALSE;
	for(i = 0; i < CURSOR_DMAX * CURSOR_DMAX; i++)
	{
		v = wram->cursor.xormask[i];
		/* v10.330 (review #173): an opaque pixel (AND 0) of a colour cursor is a palette index, 0 too (it was taken as black whatever
		 * palette entry 0 held); monochrome shapes keep 0 = black and FFFFFFFF = white */
		if(vx_cursor_bits == 8 && wram->cursor.andmask[i] == 0) {cur_x8[i] = wram->palette[v & 0xFF].dw & 0x00FFFFFFUL; cur_pal_used = TRUE; continue;}
		if(v == 0xFFFFFFFFUL) cur_x8[i] = 0x00FFFFFFUL; /* monochrome: white / invert */
		else if(v == 0) cur_x8[i] = 0; /* transparent (AND 1) or black (AND 0, index 0: black in the static palette) */
		else {cur_x8[i] = wram->palette[v & 0xFF].dw & 0x00FFFFFFUL; cur_pal_used = TRUE;}
	}
	return (const uint32_t *)cur_x8;
}

/* v10.105: 1 when the hardware cursor looks after the cursor (shown, moved, or switched off because it is hidden), 0 when the software cursor has to */
static BOOL cursor_hw_show(void)
{
	if(!vx2d_hwcur) {if(cur_hw) cursor_hw_off(); return FALSE;}
	if(vga_mode || act_mode < 0 || cur_drawn) return FALSE;
	/* v10.252 (review): an engine reset since the cursor was programmed: its unit may have lost its registers while the driver still
	 * thought it shown (the image stuck in place while the pointer moved). Program the shape and the unit again from the kept masks. */
	if(cur_epoch != vx2d_reset_epoch) {cur_epoch = vx2d_reset_epoch; if(cur_hw || cur_hw_ok) {VX2D_hwcur_invalidate(); cur_hw = FALSE; cur_hw_gen = 0xFFFFFFFFUL;}}
	if(!wram->regs.s.cursor_visible) {if(cur_hw) {VX2D_hwcur_hide(); cur_hw = FALSE;} return FALSE;}
	if(cur_hw_gen != vx_cursor_gen)
	{
		cur_hw_ok = VX2D_hwcur_set((const uint32_t *)wram->cursor.andmask, cursor_xor_colours(), CURSOR_DMAX, wram->regs.s.cursor_w, wram->regs.s.cursor_h, hda->bpp); /* v10.262: colours at 8bpp */
		cur_hw_gen = vx_cursor_gen;
	}
	if(!cur_hw_ok) {if(cur_hw) {VX2D_hwcur_hide(); cur_hw = FALSE;} return FALSE;}
	if(!VX2D_hwcur_move(wram->regs.s.cursor_x, wram->regs.s.cursor_y)) {VX2D_hwcur_hide(); cur_hw = FALSE; cur_hw_gen = 0xFFFFFFFFUL; return FALSE;} /* v10.330 (review #97): the unit switched off before the software cursor takes over (hide does nothing when it is not shown or the engine is fenced) */
	cur_hw = TRUE;
	return TRUE;
}

static void cursor_show(void)
{
	LONG x0, y0, x, y;
	DWORD b, rowbytes;
	BYTE *base;
	if(cur_pend) {cur_pend = FALSE; cur_pub();} /* v10.507: a show that was put off is this one (or no longer wanted) */
	if(cursor_hw_show()) return; /* v10.105 */
	if(VX2D_is_lost() || cur_drawn || vga_mode || act_mode < 0 || !wram->regs.s.cursor_visible) return;
	if(hda && (hda->rem3 & 0xFFFFUL)) {cur_pend = TRUE; cur_defers++; cur_pub(); return;} /* v10.507: the driver is drawing into the screen without the exclusion: shown at its next access */
	b = dpx_bytes();
	if(!b || !hda->vram_pm32 || !hda->pitch) return;
	x0 = wram->regs.s.cursor_x; y0 = wram->regs.s.cursor_y;
	cur_sx = x0 < 0 ? 0 : x0; cur_sy = y0 < 0 ? 0 : y0;
	cur_ex = x0 + wram->regs.s.cursor_w; cur_ey = y0 + wram->regs.s.cursor_h;
	if(wram->regs.s.cursor_w > CURSOR_DMAX) cur_ex = x0 + CURSOR_DMAX;
	if(wram->regs.s.cursor_h > CURSOR_DMAX) cur_ey = y0 + CURSOR_DMAX;
	if(cur_ex > (LONG)hda->width) cur_ex = (LONG)hda->width;
	if(cur_ey > (LONG)hda->height) cur_ey = (LONG)hda->height;
	if(cur_sx >= cur_ex || cur_sy >= cur_ey) return;
	cur_sw_shows++; /* v10.333 (VXPROBE cursor) */
	base = (BYTE*)hda->vram_pm32 + hda->surface;
	rowbytes = (DWORD)(cur_ex - cur_sx) * b;
	for(y = cur_sy; y < cur_ey; y++)
		memcpy(cur_save + (DWORD)(y - cur_sy) * rowbytes, base + (DWORD)y * hda->pitch + (DWORD)cur_sx * b, rowbytes);
	/* v10.142 (review O01): the background comes from the copy just saved (system memory) instead of a second read of video memory; transparent pixels (AND all ones, XOR 0) are not written */
	for(y = cur_sy; y < cur_ey; y++)
		for(x = cur_sx; x < cur_ex; x++)
		{
			DWORD cp = (DWORD)(y - y0) * CURSOR_DMAX + (DWORD)(x - x0);
			DWORD am = wram->cursor.andmask[cp], xm = wram->cursor.xormask[cp], px;
			if(am == 0xFFFFFFFFUL && xm == 0) continue;
			px = dpx_read(cur_save + (DWORD)(y - cur_sy) * rowbytes + (DWORD)(x - cur_sx) * b);
			px &= am;
			px ^= xm;
			dpx_write(base + (DWORD)y * hda->pitch + (DWORD)x * b, px);
		}
	CUR_SET(TRUE);
}

/* v10.37: registry EDID_MODES = 0 hides the extra modes again */
void VESA_configure_1152(DWORD cvt) {vx_1152_cvt = cvt ? 1 : 0;} /* v10.307: registry TIMING_1152_CVT (default 1) */
void VESA_configure_timing(DWORD on)
{
	vx_custom_timing = on ? 1 : 0;
	dbg_printf("VX85598 custom timing=%ld\n", vx_custom_timing);
}

void VESA_configure_modes(DWORD extra)
{
	vx_extras_on = extra > 2 ? 1 : extra; /* v10.330 (review #206): 2 (every BIOS mode) kept apart from 1 in the EDID report */
	if(!extra && vx_base_modes && vesa_modes_cnt > vx_base_modes) vesa_modes_cnt = vx_base_modes;
	if(extra == 2)
	{
		/* offer every BIOS mode that passed the layout check, whatever the monitor says */
		DWORD i;
		for(i = 0; i < vx_held_n && vesa_modes_cnt < 48; i++) vesa_modes[vesa_modes_cnt++] = vx_held[i];
		vx_held_n = 0;
	}
	vx_status.mode_count = vesa_modes_cnt;
	dbg_printf("VX85598 extra modes=%ld total modes=%ld\n", vx_extras_on, vesa_modes_cnt);
}

/* v10.57: without a readable EDID, sizes above 1024x768 are hidden unless registry NOEDIDHIGHRES=1 (EDID_MODES=2 also offers them). */
void VESA_configure_noedid(DWORD high_res_on)
{
	DWORD i, j = 0;
	if(high_res_on || vx_edid_ok) return;
	for(i = 0; i < vesa_modes_cnt; i++)
	{
		if(vesa_modes[i].width > 1024 || vesa_modes[i].height > 768) continue;
		if(i != j) vesa_modes[j] = vesa_modes[i];
		j++;
	}
	vesa_modes_cnt = j;
	vx_status.mode_count = vesa_modes_cnt;
	dbg_printf("VX85598 no EDID: modes above 1024x768 hidden, total modes=%ld\n", vesa_modes_cnt);
}

/* v10.333: VXPROBE (VX_IOCTL_PROBE, 64 DWORDs out). sub 1: the state report (nothing written); sub 2: the refresh measured on the
 * vertical retrace bit (3DA bit 3) for about half a second (nothing written); sub 3: arm the one-shot switches (in[1], see pr_arm). */
#pragma aux vxp_tsc = 0x0F 0x31 value [eax] modify exact [eax edx];
unsigned long vxp_tsc(void);
void VX_probe(const DWORD *in, DWORD n, DWORD *o)
{
	DWORD i, sub = n ? in[0] : 0;
	memset(o, 0, 64 * 4);
	o[0] = VX_MAGIC; o[1] = sub;
	if(sub == 1)
	{
		for(i = 0; i < 13; i++) o[2 + i] = pr_set[i];
		o[15] = vx_custom_timing; o[16] = vx_timing_fixed; o[17] = vx_timing_hz;
		o[18] = cur_drawn; o[19] = cur_hw; o[20] = cur_hw_ok; o[21] = wram ? wram->regs.s.cursor_visible : 0;
		o[22] = wram ? (DWORD)wram->regs.s.cursor_x : 0; o[23] = wram ? (DWORD)wram->regs.s.cursor_y : 0;
		o[24] = (DWORD)act_mode; o[25] = vga_mode; o[26] = cur_sw_shows; o[27] = vx_cursor_gen; o[28] = vx2d_hwcur;
		o[29] = hda ? hda->width : 0; o[30] = hda ? hda->height : 0; o[31] = hda ? hda->bpp : 0;
		o[32] = vx_edid_ok; o[33] = vx_base_modes; o[34] = vesa_modes_cnt; o[35] = vx_held_n; o[36] = vx_extras_on; o[37] = pr_arm;
		for(i = 0; i < vesa_modes_cnt && i < 24; i++) o[40 + i] = vesa_modes[i].width | ((DWORD)vesa_modes[i].height << 12) | ((DWORD)vesa_modes[i].bpp << 24);
		o[38] = i;
	}
	else if(sub == 2)
	{
		DWORD ms0 = Get_System_Time(), t0 = vxp_tsc(), ta = 0, tb = 0, edges = 0, last, now, polls = 0;
		last = inp(0x3DA) & 8;
		while((DWORD)(Get_System_Time() - ms0) < 600UL && polls < 400000000UL)
		{
			now = inp(0x3DA) & 8; polls++;
			if(now && !last) {tb = vxp_tsc(); if(!edges) ta = tb; edges++;}
			last = now;
		}
		o[2] = edges; o[3] = tb - ta; o[4] = vxp_tsc() - t0; o[5] = Get_System_Time() - ms0; o[6] = polls;
	}
	else if(sub == 4) /* v10.339: the last mode sets, oldest first: [2] how many sets in all, then up to 5 x 12 DWORDs (pr_set[1..12]), the current last; [63] how many */
	{
		DWORD k, j, n = pr_hist_n < 4 ? pr_hist_n : 4, at = 3;
		o[2] = pr_set[0];
		for(k = 0; k < n; k++) for(j = 0; j < 12; j++) o[at++] = pr_hist[(pr_hist_n - n + k) & 7][j];
		for(j = 0; j < 12; j++) o[at++] = pr_set[1 + j];
		o[63] = n + 1;
	}
	else if(sub == 3)
	{
		pr_arm = n > 1 ? (in[1] & 7) : 0; pr_arm_ms = Get_System_Time(); o[2] = pr_arm;
	}
	else if(sub == 5) /* .583 (VXTIME: the driver's own timing on a device where it is still under test): in[1] 0 / 1 sets the switch for the mode
	                   * sets that follow (anything else only reads); [2] the switch before, [3] now, [4] the last set's result (0 the BIOS's
	                   * timing, 1 the driver's, 2 refused), [5] its rate, [6] SR44 | SR45 << 8 | SR46 << 16 | SR40 << 24 (the primary PLL) */
	{
		unsigned long fl; unsigned char sidx, unlock;
		o[2] = vx_custom_timing;
		if(n > 1 && in[1] < 2) vx_custom_timing = in[1];
		o[3] = vx_custom_timing; o[4] = vx_timing_fixed; o[5] = vx_timing_hz;
		fl = vt_irq_save(); sidx = inp(0x3C4); unlock = vt_sr_get(0x10); vt_sr_put(0x10, (unsigned char)(unlock | 1));
		o[6] = (DWORD)vt_sr_get(0x44) | ((DWORD)vt_sr_get(0x45) << 8) | ((DWORD)vt_sr_get(0x46) << 16) | ((DWORD)vt_sr_get(0x40) << 24);
		vt_sr_put(0x10, unlock); outp(0x3C4, sidx); vt_irq_restore(fl);
	}
}

void VX_edid_report(DWORD *out, DWORD scan)
{
	DWORD i;
	vx_edid_report *r = (vx_edid_report*)out;
	extern unsigned char vx_edid[128], vx_edid_late[128]; extern int vx_edid_ok, vx_edid_bus, vx_edid_late_ok, vx_edid_late_bus;
	extern void VX_edid_scan(DWORD *o);
	memset(r, 0, sizeof(*r));
	if(scan) VX_edid_scan((DWORD*)r->scan);
	r->magic = VX_MAGIC; r->edid_valid = vx_edid_ok; r->edid_bus = vx_edid_bus;
	r->reserved = vx_bios_ddc_ax; r->base_modes = vx_base_modes; r->total_modes = vesa_modes_cnt; r->extras_on = vx_extras_on; r->bios_count = vx_bios_n;
	for(i = 0; i < 128; i++) ((unsigned char*)r->edid)[i] = vx_edid[i];
	if(!vx_edid_ok && vx_edid_late_ok)
	{
		r->edid_valid = 2; r->edid_bus = vx_edid_late_bus;
		for(i = 0; i < 128; i++) ((unsigned char*)r->edid)[i] = vx_edid_late[i];
	}
	for(i = 0; i < vx_bios_n * 3 && i < 192; i++) r->bios[i] = vx_bios_log[i];
}

/* called by the VxD init after registry values are read */
void VESA_configure_direct(DWORD direct, DWORD wc)
{
	if(!direct && vx_direct)
	{
		/* back to the shadow path */
		cursor_hide();
		vx_direct = 0; cur_pub();
		hda->vram_pm32 = (BYTE*)wram + wram->regs.s.fbmin;
		hda->vram_size_virt = wram->regs.s.fbmax - wram->regs.s.fbmin;
		hda->flags |= FB_SUPPORT_FLIPING;
		vx_status.shadow_linear = (DWORD)hda->vram_pm32;
		vx_status.shadow_bytes = hda->vram_size_virt;
	}
	conf_mtrr = wc; vx_mtrr_conf = wc;
	dbg_printf("VX85598 direct VRAM=%ld write-combining=%ld\n", vx_direct, conf_mtrr);
}

void FBHDA_access_begin(DWORD flags)
{
	BOOL lk = FBHDA_lock(); /* v10.319 (audit V1) */
	fb_lock_cnt++;

	if(vx_direct)
	{
		cursor_hide();
	}
	else if(flags & FBHDA_ACCESS_MOUSE_MOVE)
	{
		DWORD left, top, right, bottom;
		if(mouse_get_rect(&left, &top, &right, &bottom))
		{
			wram_changes(&wblit, left, top, right, bottom);
		}
	}
	else
	{
		wram_changes(&wblit, 0, 0, hda->width, hda->height);
	}

	FBHDA_release(lk);
}

void FBHDA_access_rect(DWORD left, DWORD top, DWORD right, DWORD bottom)
{
	BOOL lk = FBHDA_lock(); /* v10.319 (audit V1) */
	fb_lock_cnt++;
	if(vx_direct)
	{
		if(cur_drawn && (LONG)left < cur_ex && (LONG)right > cur_sx && (LONG)top < cur_ey && (LONG)bottom > cur_sy)
			cursor_hide();
	}
	else
	{
		wram_changes(&wblit, left, top, right, bottom);
	}
	FBHDA_release(lk);
}

void FBHDA_access_end(DWORD flags)
{
	BOOL lk = FBHDA_lock(); /* v10.319 (audit V1) */

	if(!vx_direct && (flags & FBHDA_ACCESS_MOUSE_MOVE))
	{
		DWORD left, top, right, bottom;
		if(mouse_get_rect(&left, &top, &right, &bottom))
		{
			wram_changes(&wblit, left, top, right, bottom);
		}
	}

	fb_lock_cnt--;
	if(fb_lock_cnt < 0) fb_lock_cnt = 0;

	if(vx_direct)
	{
		if(fb_lock_cnt == 0) cursor_show();
	}
	else if(!timer_set || !async_blit_running())
	{
		if(fb_lock_cnt == 0)
		{
			VESA_draw(&wblit);
		}
	}

	FBHDA_release(lk);
}

DWORD FBHDA_overlay_setup(DWORD overlay, DWORD width, DWORD height, DWORD bpp)
{
	return 0;
}

void FBHDA_overlay_lock(DWORD left, DWORD top, DWORD right, DWORD bottom)
{

}

void  FBHDA_overlay_unlock(DWORD flags)
{

}

void VESA_HIRES_enable()
{
	dbg_printf("VESA_HIRES_enable\n");
	CUR_SET(FALSE);
	cursor_hw_off();
	vga_mode = FALSE;
	vx_status.vga_mode=0;
	VX2D_gamma_load(NULL); /* v10.123: back from a DOS box */
}

void VESA_HIRES_disable()
{
	dbg_printf("VESA_HIRES_disable\n");
	VX2D_sync(); /* v10.139 (review F01) */
	if(FBHDA_lock()) {cursor_hide(); FBHDA_release(TRUE);} else cursor_hide(); /* v10.140: the sync can release the cursor exclusion and draw the software cursor; put the background back instead of only clearing the flag */
	CUR_SET(FALSE);
	cursor_hw_off(); /* v10.105: a DOS box or the text mode must not show the hardware cursor */
	VX2D_gamma_off(); /* v10.123: nor the gamma table */
	vga_mode = TRUE;
	vx_status.vga_mode=1;
}

extern BOOL mode_changing;

static BYTE saved_vga_mode = 0xFF;

BOOL VESA_check_int10h(DWORD vm, PCRS_32 regs)
{
	if(vm != ThisVM)
	{
		BYTE ah = (regs->Client_EAX >> 8) & 0xFF;
		BYTE al =  regs->Client_EAX & 0xFF;
		if(ah == 0)
		{
			BYTE mode = al & 0x7F;
			/* A newly created DOS VM may request mode 03 before Windows sends
			 * PRE_HIRES_TO_VGA. Do not swallow standard text initialization:
			 * return TRUE so the remaining V86 INT 10h chain, including the
			 * Main VDD, can process/virtualize it. This is not a direct BIOS
			 * call and must also work while the DOS VM is windowed.
			 */
			if(mode <= 3 || mode == 7)
			{
				dbg_printf("DOS text mode chained: VM=%lX AX=%lX VGA=%ld changing=%ld\n",
				 vm, regs->Client_EAX, vga_mode, mode_changing);
				return TRUE;
			}
			if(!vga_mode && conf_dos_window == 0)
			{
				saved_vga_mode = mode;
				return FALSE;
			}
		}

		if(ah == 0x4F)
		{
			if(al == 0x02) /* mode set */
			{
				if(!vga_mode && conf_dos_window == 0)
				{
					regs->Client_EAX &= 0xFFFF0000UL;
					regs->Client_EAX |= 0x034F; /* Function call invalid in current video mode */
					return FALSE;
				}
			}
		}
	}
	return TRUE;
}


BOOL VESA_check_switch(DWORD mode)
{
	return TRUE;
}


/* v10.71: DirectDraw palette at 8bpp (HAL -> VX_IOCTL_DAC). The GDI palette stays in wram->palette[] and is written back when DirectDraw lets go. */
static int dac_ready(void)
{
	return hda && wram && hda->bpp == 8 && vx_status.stage == 6 && !vx_status.failure && !vx_status.vga_mode && !vx_native_display_hold;
}
static void dac_write(unsigned long first, unsigned long count, const unsigned long *rgb)
{
	unsigned long i, f;
	if(!(vesa_caps & VESA_CAP_NONVGA))
	{
		f = vxd_irq_save();
		outp(0x3C6, 0xFF); outp(0x3C8, (unsigned char)first);
		for(i = 0; i < count; i++)
		{
			unsigned long c = rgb[i];
			if(vesa_pal_bits == 8) {outp(0x3C9, (c >> 16) & 255); outp(0x3C9, (c >> 8) & 255); outp(0x3C9, c & 255);}
			else {outp(0x3C9, (c >> 18) & 63); outp(0x3C9, (c >> 10) & 63); outp(0x3C9, (c >> 2) & 63);}
		}
		vxd_irq_restore(f);
	}
	else for(i = 0; i < count; i++)
	{
		DWORD keep = wram->palette[first + i].dw;
		FBHDA_palette_set((unsigned char)(first + i), rgb[i]);
		wram->palette[first + i].dw = keep;
	}
}
int VX_dac(const vx_dac_req *q)
{
	unsigned long i, all[256];
	if(!q || q->cb != sizeof(*q) || q->abi != VX_DAC_ABI || !dac_ready()) return 0;
	if(q->first == 0xFFFFFFFEUL && !q->count) return 1; /* probe */
	if(q->first == 0xFFFFFFFFUL && !q->count)
	{
		for(i = 0; i < 256; i++) all[i] = wram->palette[i].dw & 0xFFFFFFUL;
		dac_write(0, 256, all);
		return 1;
	}
	if(q->first >= 256 || !q->count || q->count > 256 - q->first) return 0;
	for(i = 0; i < q->count; i++) all[i] = q->rgb[i] & 0xFFFFFFUL;
	dac_write(q->first, q->count, all);
	return 1;
}

/* v10.71: read the DAC back (VX2AUTO). out->first = bits per component (6 or 8), out->count = 256, out->rgb = the raw components as (r<<16)|(g<<8)|b. VGA-compatible DAC only. */
int VX_dac_read(vx_dac_req *out)
{
	unsigned long i, f;
	if(!out || !dac_ready() || (vesa_caps & VESA_CAP_NONVGA)) return 0;
	out->cb = sizeof(*out); out->abi = VX_DAC_ABI; out->first = vesa_pal_bits; out->count = 256;
	f = vxd_irq_save();
	outp(0x3C7, 0);
	for(i = 0; i < 256; i++)
	{
		unsigned long r = inp(0x3C9) & 0xFF, g = inp(0x3C9) & 0xFF, b = inp(0x3C9) & 0xFF;
		out->rgb[i] = (r << 16) | (g << 8) | b;
	}
	vxd_irq_restore(f);
	return 1;
}
