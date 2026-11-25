/* VX855 display start (page flipping) and vertical blank for DirectDraw, v10.59.
 *
 * The driver draws straight into the BIOS frame buffer and never used hardware flipping. DirectDraw flip chains need
 * the display start address to follow the surface that the application flips to. The start address register layout
 * (CR0C/CR0D/CR34/CR48 on VIA chips, but in which unit?) is not guessed: the first time it is needed it is calibrated
 * against the BIOS. Ask the BIOS (INT 10h 4F07h) to show a line far into video memory, read the CRTC back, and derive
 * the unit. Only when the BIOS result matches the register layout exactly are the registers written directly;
 * otherwise the BIOS call itself is used on every flip (when it provably changes the CRTC), or flipping is refused.
 * Vertical blank is bit 3 of input status 1 (3DA/3BA); it is probed once and flips only wait for it when it toggles. */
#include "winhack.h"
#include "vmm.h"
#include "vxd_lib.h"
#include "vx85598.h"
#include "3d_accel.h"
#include "vx855_flip.h"
#include "vx855_2d.h"
#define IO_IN8
#define IO_OUT8
#include "io32.h"
extern volatile uint32_t vx3dn_rt_why; /* v10.473: the retire's cause (vx_native3d_vxd.c) */
int VX3DN_busy(void); /* .553: vx_native3d.h's (it was called below without a prototype, W131) */
extern FBHDA_t *hda;
void vesa_bios(CRS_32 *regs);
void load_client_state(CRS_32 *regs);
void vx_lock(void);void vx_unlock(void); /* .515 (vx855_2d.c) */
unsigned long VX_irq_save(void);
void VX_irq_restore(unsigned long flags);
#pragma aux VX_irq_save = "pushfd" "pop eax" "cli" value [eax] modify exact [eax];
#pragma aux VX_irq_restore = "push eax" "popfd" parm [eax] modify exact [];

/* registry (HKLM\Software\VX85598, DWORD): FLIP=0 turns display-start flipping off, FLIP_BIOS=0 forbids the per-flip BIOS fallback. Both default to 1. */
volatile unsigned long vx_scan_enable=0; /* v10.73: registry SCANLINE, default 0 */
uint32_t VX2D_mmio_read(uint32_t);
volatile unsigned long vx_flip_enable=1; /* FLIP_CAL=1 brings back the BIOS calibration (INT 10h 4F07 test lines); default 0 = unit is known (2-byte steps), the BIOS is not called */
static DWORD fl_state,fl_shift,fl_vb,fl_reason,fl_display,fl_flips,fl_bios,fl_cal_y,fl_cal_v,fl_changed,fl_frame3,fl_timeouts,fl_last_req,fl_last_v;
static DWORD k_w,k_h,k_b,k_p,k_set; /* the video mode the calibration belongs to */
static unsigned char snap[3][256];
/* v10.384 (board, GTA3: stutter): a flip waited, after writing the start address, for the next blank to begin: up to a whole refresh of
 * the CPU's time every frame, and the frame times stepped by whole refreshes. With FLIP_ASYNC (registry, default 1) the flip returns once
 * the address is written; the surface it moved away from (the old front) is still scanned until the next blank, so a draw, Lock or Blt
 * into it first waits for that (VX_flip_front_wait, VXF_FRONT), and the next flip waits for it before writing the address again.
 * Taken by the display: a blank seen since the address was written (it is only written outside a blank), the scan line counter
 * (0x1290) below the line it read then, or a frame and 2 ms gone by. vx_frame_stat (TEXDBG 0xBE): [0] flips, [1-6] the time between
 * flips under 20, 35, 50, 75, 100 ms and the rest, [7] the wait for the engine in a flip (TSC / 256), [8] the wait for the blank or the
 * last flip in a flip, [9] the longest time between flips (ms), [10] the longest engine wait, [11] the longest blank wait, [12] waits
 * for the old front, [13] their time, [14] flips that returned without the blank wait, [15] the time between flips summed (ms). */
volatile unsigned long vx_flip_async=1;
volatile uint32_t vx_flip_old_front=0xFFFFFFFFUL;
volatile uint32_t vx_flip_epoch; /* .620 */
uint32_t vx_frame_stat[16],vx_frame_worst[8][3];
static DWORD fl_pend,fl_apply_ms,fl_apply_line,fl_last_flip_ms,fl_eng_t;static int fl_line_ok;
/* v10.417 (XP's flip in the ring, vx_native3d_vxd.c VX3DN_ring_flip): fl_ring = ring flips in use (CR32
 * bit 0 set, as XP: the manual's "real-time flipping", by scan line rather than by frame); vx_flip_ring_pend = the old front waits for the ring flip's fence instead of the blank */
int VX3DN_ring_flip_ok(void);int VX3DN_ring_flip(uint32_t off,int two,int tear);int VX3DN_ring_flip_done(void);void VX3DN_ring_flip_wait(void);int VX3DN_ring_flip_lost(void); /* .532 */
/* .696: fl_rt = CR32 bit 0 set by a CPU tear flip (no ring); vx_flip_tear = ACCEL_FLIP_TEAR; vx_flip_tear_stat: [0] tear flips in the ring,
 * [1] tear flips by the CPU, [2] no-vsync flips kept as the one-frame flip (switch 0, a panel on IGA2, or IGA2 mirroring with switch 1) */
volatile uint32_t vx_flip_tear=1;uint32_t vx_flip_tear_stat[4];static int fl_rt;
static int fl_ring;int vx_flip_ring_pend;uint32_t vx_flip_ring_stat[4]; /* CR32 bit 0 set, cleared, CPU flips after ring flips, ring flips */
unsigned long VX_tsc(void);
#pragma aux VX_tsc = 0x0F 0x31 value [eax] modify exact [eax edx];

static unsigned char sr_get(unsigned char i) {
 unsigned long f=VX_irq_save(); unsigned char o=inp(0x3C4),v;
 outp(0x3C4,i); v=inp(0x3C5); outp(0x3C4,o); VX_irq_restore(f); return v;
}
static void sr_mask(unsigned char i,unsigned char mask,unsigned char val) {
 unsigned long f=VX_irq_save(); unsigned char o=inp(0x3C4),v;
 outp(0x3C4,i); v=inp(0x3C5); outp(0x3C5,(unsigned char)((v&~mask)|(val&mask))); outp(0x3C4,o); VX_irq_restore(f);
}
/* The extended CRTC registers need SR10 bit 0 (the same handshake as the register dump). */
static unsigned char ext_open(void) {unsigned char u=sr_get(0x10); sr_mask(0x10,0x01,0x01); return u;}
static void ext_close(unsigned char u) {sr_mask(0x10,0x01,(unsigned char)(u&1));}
static unsigned short crt_base(void) {return (inp(0x3CC)&1)?0x3D4:0x3B4;}
static unsigned char cr_get(unsigned short base,unsigned char i) {
 unsigned long f=VX_irq_save(); unsigned char o=inp(base),v;
 outp(base,i); v=inp((unsigned short)(base+1)); outp(base,o); VX_irq_restore(f); return v;
}
/* .598 (a Samsung NC20, VX800, on .597: every test passed and yet the pointer was invisible and Direct3D showed one buffer, flickering;
 * VXSTATE: CR6A C0, CR97 11, CR99 10, CR36 31, IGA2's timing 1280x800 in 1408x816, its pitch CR66/67 = the desktop's, its start CR62-64 0).
 * A laptop's panel is scanned by the second display unit (IGA2) from the same frame buffer; the first unit runs too, its monitor output
 * powered down. The start address, the blank and the hardware icon were the first unit's only. Now:
 * - fl_iga2: IGA2 is on (CR6A bit 7) and its pitch (CR66, CR67 bits 1:0, CR71 bit 7, in 8-byte units) is the mode's: it shows this frame
 *   buffer, so every start address is written to it as well (CR62 bits 7:1, CR63, CR64, CRA3 bits 2:0: the byte offset >> 3, OpenChrome
 *   viaIGA2SetFBStartingAddress) and the cursor is shown by its icon unit too (vx855_2d.c);
 * - fl_blank2: besides, the panel takes IGA2 (CR97 or CR99 bit 4) and the monitor output is powered down (CR36 bits 5:4): the picture
 *   seen is IGA2's alone, so the blank is IGA2's (MMIO 204 bit 2 clear, .599; VIA's CE driver's flip waits while it is set), no flip goes through the ring (its
 *   display address is the first unit's) and the first unit's line counter is not used. When that status bit does not toggle (vb_probe),
 *   a flip waits a frame's time after the address is written instead.
 * vx_flip_two (control FFFFFE48): [0] CR6A | CR36 << 8 | CR97 << 16 | CR99 << 24, [1] IGA2's pitch in bytes, [2] bit 0 fl_iga2, bit 1
 * fl_blank2, [3] flips written to IGA2, [4] flips that waited by time, [5] ring flips that moved IGA2 too (.695). */
uint32_t vx_flip_two[8];
static int fl_iga2,fl_blank2;
static void iga2_look(void) {
 unsigned char u=ext_open(); unsigned short base=crt_base();
 unsigned char a=cr_get(base,0x6A),p0=cr_get(base,0x66),p1=cr_get(base,0x67),p2=cr_get(base,0x71),d=cr_get(base,0x36),l1=cr_get(base,0x97),l0=cr_get(base,0x99);
 DWORD pitch=((DWORD)p0|((DWORD)(p1&3)<<8)|((DWORD)(p2&0x80)<<3))<<3;
 ext_close(u);
 fl_iga2=(a&0x80)&&hda&&pitch&&pitch==hda->pitch;
 fl_blank2=fl_iga2&&((l0|l1)&0x10)&&(d&0x30);
 vx_flip_two[0]=(DWORD)a|((DWORD)d<<8)|((DWORD)l1<<16)|((DWORD)l0<<24); vx_flip_two[1]=pitch; vx_flip_two[2]=(fl_iga2?1UL:0)|(fl_blank2?2UL:0);
}
int VX_flip_iga2(void) {iga2_look(); return fl_iga2;}
static int fl_taken(void);
int VX_flip_word_ok(void) {if(fl_pend&&!vx_flip_ring_pend)(void)fl_taken(); return fl_pend&&!vx_flip_ring_pend&&!fl_blank2;} /* .620: a CPU flip is still pending (looked at now) and the blank that counts is the first unit's */
static int in_blank(unsigned short p) {
 if(fl_blank2) {uint32_t s=VX2D_mmio_read(0x204UL); return s!=0xFFFFFFFFUL&&(s&4UL)==0;} /* .599 (NC20, VXPANEL v1.0 on .598: the bit was set in 98.06% of 217,803 samples and changed 120 times a second; IGA2 shows 800 of 816 lines = 98.04%): the bit is 1 while the picture is scanned and 0 in the blank. .598 read it the other way round: a flip was taken as shown at once */
 return (inp(p)&8)!=0;
}
static void snapshot(unsigned char *s) {
 unsigned char u=ext_open(); unsigned short base=crt_base(); unsigned i;
 for(i=0;i<256;i++) s[i]=cr_get(base,(unsigned char)i);
 ext_close(u);
}
static DWORD value_of(const unsigned char *s) {
 return ((DWORD)(s[0x48]&0x1F)<<24)|((DWORD)s[0x34]<<16)|((DWORD)s[0x0C]<<8)|s[0x0D];
}
/* Write the four start address registers. CR34 is the "fire" register: the chip commits the whole address when CR34 is written, so it
 * must come LAST (CR0D, CR0C, CR48, then CR34), as OpenChrome does. v10.63: it used to be written second, which committed an address built
 * from the previous low bytes while the readable registers ended up correct (the scan-out stayed shifted though every register read 0).
 * CR34 is written even when the value is unchanged or zero. */
static void crtc_set(DWORD v) {
 unsigned char u,o,t; unsigned short base; unsigned long f;
 iga2_look(); /* .598 */
 u=ext_open(); base=crt_base();
 f=VX_irq_save(); o=inp(base);
 if(fl_iga2&&!(v&3UL)) { /* .598: v counts 2 bytes, IGA2's address 8 */
  DWORD b=v>>2;
  outp(base,0x62); t=inp((unsigned short)(base+1)); outp((unsigned short)(base+1),(unsigned char)((t&1)|((b&0x7FUL)<<1)));
  outp(base,0x63); outp((unsigned short)(base+1),(unsigned char)(b>>7));
  outp(base,0x64); outp((unsigned short)(base+1),(unsigned char)(b>>15));
  outp(base,0xA3); t=inp((unsigned short)(base+1)); outp((unsigned short)(base+1),(unsigned char)((t&0xF8)|((b>>23)&7UL)));
  vx_flip_two[3]++;
 }
 outp(base,0x0D); outp((unsigned short)(base+1),(unsigned char)v);
 outp(base,0x0C); outp((unsigned short)(base+1),(unsigned char)(v>>8));
 outp(base,0x48); t=inp((unsigned short)(base+1)); outp((unsigned short)(base+1),(unsigned char)((t&0xE0)|((v>>24)&0x1F)));
 outp(base,0x34); outp((unsigned short)(base+1),(unsigned char)(v>>16));
 outp(base,o); VX_irq_restore(f);
 ext_close(u);
}
/* v10.417: CR32 bit 0, real-time flipping in the manual (XP 0xB7092: set before its first flip in the ring; cleared in its other display paths, 0xB6DB9) */
static void cr32_bit0(int on) {
 unsigned char u=ext_open(),o,t; unsigned short base=crt_base(); unsigned long f;
 f=VX_irq_save(); o=inp(base); outp(base,0x32); t=inp((unsigned short)(base+1));
 outp((unsigned short)(base+1),(unsigned char)(on?(t|1):(t&0xFE))); outp(base,o); VX_irq_restore(f);
 ext_close(u); vx_flip_ring_stat[on?0:1]++;
}
/* v10.417: back to CPU flips: the ring flip in flight run first, the CRTC given the address being shown, then CR32 bit 0 clear */
static void fl_ring_lost(void);
static void ring_off(void) {
 if(!fl_ring) return;
 if(vx_flip_ring_pend||!VX3DN_ring_flip_done()) {VX3DN_ring_flip_wait(); /* .686: also a flip to the address already shown (queued in the ring but not marked pending here) */
  if(VX3DN_ring_flip_lost()&&vx_flip_ring_pend) {fl_ring_lost(); return;} /* .694 (review of .693): a lost flip to a new address is written by the CPU and its old front stays guarded until the blank, as fl_ring_lost does elsewhere (it was dropped here: the next flip guarded the lost one's address while the old front was still shown) */
  vx_flip_ring_pend=0; fl_pend=0; vx_flip_old_front=0xFFFFFFFFUL;}
 else (void)VX3DN_ring_flip_lost(); /* .694 (review of .693): VX3DN_ring_flip_done may have just marked a same-address flip lost; consumed here so a later flip does not take it for its own */
 if(fl_state==VXF_ST_DIRECT) crtc_set(fl_display>>fl_shift);
 cr32_bit0(0); fl_ring=0; fl_rt=0;
}
/* v10.63: called by the VxD after every mode set: commit start 0 and forget the bookkeeping of the previous mode. */
void VX_flip_modeset(void) {
 if(fl_ring||fl_rt) {cr32_bit0(0); fl_ring=0; fl_rt=0;} vx_flip_ring_pend=0; (void)VX3DN_ring_flip_lost(); fl_pend=0; vx_flip_old_front=0xFFFFFFFFUL; /* v10.417 (the ring was stopped before the mode set); .532: and the pending flip's bookkeeping (B1) */
 crtc_set(0); k_set=0; fl_state=VXF_ST_UNTESTED; fl_vb=0; fl_display=0; fl_shift=0;
}
static int bios_start(DWORD x,DWORD y) {
 CRS_32 regs;
 load_client_state(&regs);
 regs.Client_EAX=0x4F07; regs.Client_EBX=0; regs.Client_ECX=x; regs.Client_EDX=y;
 vesa_bios(&regs); fl_bios++;
 return (regs.Client_EAX&0xFFFF)==0x004F;
}

/* ---- vertical blank ---- */
static unsigned short st1_port(void) {return (inp(0x3CC)&1)?0x3DA:0x3BA;}
static int vb_wait(unsigned short p,int want,DWORD ms) { /* 1 = reached, 0 = timed out */
 DWORD n,start=0; int have=0;
 for(n=0;n<4000000UL;n++) {
  if(in_blank(p)==want) return 1;
  if((n&63)==63) {
   if(!have) {start=Get_System_Time(); have=1;}
   else if((DWORD)(Get_System_Time()-start)>=ms) return 0;
  }
 }
 return 0;
}
static int vb_begin(unsigned short p,DWORD ms) { /* a fresh start of the blank: leave one that is already running first */
 if(!vb_wait(p,0,ms)) return 0;
 return vb_wait(p,1,ms);
}
static DWORD fl_frame_ms(void) {return fl_frame3>=30?(fl_frame3+2)/3:17;}
/* .532 (review of .530, B1): a ring flip the ring never ran (it stopped first: VX3DN_ring_flip_lost): the address written by the CPU now,
 * CR32 bit 0 cleared (the CPU's address shown), and from here the flip is a CPU flip just written: fl_pend and the old front stay until
 * the blank (or the line) test of fl_taken passes. It answered "taken" and the old front was written into while still shown. */
uint32_t vx_flip_ring_lost;
static void fl_ring_lost(void) {
 DWORD raw;
 if(fl_state==VXF_ST_DIRECT) crtc_set(fl_display>>fl_shift);
 if(fl_ring) {cr32_bit0(0); fl_ring=0;}
 vx_flip_ring_pend=0; vx_flip_ring_lost++;
 raw=VX2D_mmio_read(0x1290UL); fl_apply_ms=Get_System_Time(); fl_apply_line=raw>>16; fl_line_ok=raw!=0xFFFFFFFFUL&&hda&&(raw>>16)<hda->height;
}
static int fl_taken(void) { /* v10.384: 1 when the pending flip has been taken by the display */
 unsigned short p;
 if(!fl_pend) return 1;
 if(vx_flip_ring_pend) {if(!VX3DN_ring_flip_done()) return 0; if(VX3DN_ring_flip_lost()) {fl_ring_lost(); return 0;} fl_pend=0; vx_flip_ring_pend=0; vx_flip_old_front=0xFFFFFFFFUL; return 1;} /* v10.417; .532: lost (B1) */
 p=st1_port();
 if(in_blank(p)||(DWORD)(Get_System_Time()-fl_apply_ms)>=fl_frame_ms()+2) {fl_pend=0; vx_flip_old_front=0xFFFFFFFFUL; return 1;}
 if(fl_line_ok&&hda) {DWORD raw=VX2D_mmio_read(0x1290UL),l=raw>>16; if(raw!=0xFFFFFFFFUL&&l<hda->height&&l<fl_apply_line) {fl_pend=0; vx_flip_old_front=0xFFFFFFFFUL; return 1;}}
 return 0;
}
/* .706 (a review of .703): fl_seq, one count a flip made pending. VX_flip_front_wait runs outside the engine lock (the 3D submit's wait
 * for the old front), and VX3DN_ring_flip_wait can block on that lock: a flip made meanwhile by another thread (it waits for this one
 * itself first) left this wait clearing the new flip's pending state, or writing the address back for a lost one. A wait that finds a
 * newer flip leaves the state to it. */
static DWORD fl_seq;
static void fl_wait_taken(void) {
 unsigned short p=st1_port(); DWORD t0=Get_System_Time(),s=fl_seq;
 if(vx_flip_ring_pend) {VX3DN_ring_flip_wait(); if(fl_seq!=s) return; if(VX3DN_ring_flip_lost()) fl_ring_lost(); else {fl_pend=0; vx_flip_ring_pend=0; vx_flip_old_front=0xFFFFFFFFUL; return;}} /* v10.417; .532: a lost one waits as a CPU flip (B1) */
 while(fl_seq==s&&!fl_taken()) { /* .765 (audit VXD-H3): the CPU-flip wait also leaves a newer flip's pending state to it (.706 covered the ring branch only) */
  if(vb_wait(p,1,1)) {if(fl_seq==s){fl_pend=0; vx_flip_old_front=0xFFFFFFFFUL;} break;}
  if((DWORD)(Get_System_Time()-t0)>=60) {if(fl_seq==s){fl_pend=0; vx_flip_old_front=0xFFFFFFFFUL; fl_timeouts++;} break;}
 }
}
void VX_flip_front_wait(uint32_t off) {
 uint32_t t;
 if(!fl_pend||off!=vx_flip_old_front) return;
 {extern int VX_smp_wait_begin(void);extern void VX_smp_wait_end(int);int w=VX_smp_wait_begin();t=VX_tsc(); fl_wait_taken(); VX_smp_wait_end(w);} /* v10.439 */ vx_frame_stat[12]++; vx_frame_stat[13]+=(VX_tsc()-t)>>8;
}
static void vb_probe(void) {
 unsigned short p=st1_port(); DWORD t0=0,t1=0; int i;
 iga2_look(); /* .598: which unit's blank */
 fl_vb=2; fl_frame3=0;
 if(!vb_begin(p,60)) return;
 t0=Get_System_Time();
 for(i=0;i<3;i++) if(!vb_begin(p,60)) return;
 t1=Get_System_Time();
 fl_vb=1; fl_frame3=t1-t0;
}

/* ---- mode bookkeeping ---- */
/* .577: the refresh rate of the mode shown, for DirectDraw's dwMonitorFrequency (GetMonitorFrequency answered 0: board, VXFLIPF v1.0; the
 * display driver asks with OP_VX_HZ when it fills DDHALINFO). The rate the mode was set with when the driver chose the timing; else the
 * rate measured on the blank signal (vb_probe: three frames in milliseconds), taken to the nearest usual rate; 0 when neither is known. */
extern DWORD vesa_set_freq;
static void vb_probe(void);static void forget_if_changed(void);static void remember_mode(void);
DWORD VX_flip_hz(void) {
 static const DWORD usual[]={50,56,60,65,70,72,75,85,90,100,120};DWORD hz=0,k,best=0,bd=0xFFFFFFFFUL;
 if(!hda) return 0;
 vx_lock();
 if(vesa_set_freq>=40&&vesa_set_freq<=200) hz=vesa_set_freq;
 else {
  forget_if_changed();
  if(!k_set||fl_vb==0) {if(!k_set) {remember_mode(); fl_state=VXF_ST_UNTESTED;} vb_probe();}
  if(fl_vb==1&&fl_frame3>=15&&fl_frame3<=75) {
   DWORD m=(3000UL*10UL+fl_frame3/2)/fl_frame3; /* tenths of a hertz */
   for(k=0;k<sizeof(usual)/sizeof(usual[0]);k++) {DWORD d=usual[k]*10UL>m?usual[k]*10UL-m:m-usual[k]*10UL; if(d<bd) {bd=d; best=usual[k];}}
   hz=best;
  }
  if(!hz&&fl_blank2) hz=60; /* .598: a panel on IGA2 whose blank status cannot be timed */
 }
 vx_unlock();
 return hz;
}
static int same_mode(void) {return k_set&&k_w==hda->width&&k_h==hda->height&&k_b==hda->bpp&&k_p==hda->pitch;}
static void remember_mode(void) {k_w=hda->width; k_h=hda->height; k_b=hda->bpp; k_p=hda->pitch; k_set=1;}
static void forget_if_changed(void) {
 if(k_set&&!same_mode()) {if(fl_ring||fl_rt) {cr32_bit0(0); fl_ring=0; fl_rt=0;} vx_flip_ring_pend=0; (void)VX3DN_ring_flip_lost(); /* .706 (a review of .703): as VX_flip_modeset (a mode
  * set the VxD did not report left fl_ring and fl_rt set, so the next flip did not set CR32 bit 0 again, and a ring flip pending) */
  k_set=0; fl_state=VXF_ST_UNTESTED; fl_vb=0; fl_display=0; fl_shift=0; fl_pend=0; vx_flip_old_front=0xFFFFFFFFUL;}
}
static DWORD natural_pitch(void) {return (hda->width*(hda->bpp/8)+7)&~7UL;}

static void calibrate(void) {
 DWORD h=hda->height,pitch=hda->pitch,v0; /* .530: FLIP_CAL's BIOS calibration (INT 10h 4F07 test lines) removed: the unit is known */
 fl_state=VXF_ST_NONE; fl_reason=0; fl_shift=0; fl_changed=0; fl_cal_y=0; fl_cal_v=0; fl_display=0;
 remember_mode();
 vb_probe();
 if(!vx_flip_enable) {fl_reason=7; return;}
 if((hda->bpp!=8&&hda->bpp!=16&&hda->bpp!=32)||!pitch||!h||!hda->width||pitch!=natural_pitch()||pitch>0x3FF8UL) {fl_reason=1; return;}
 if(hda->surface||hda->system_surface) {fl_reason=2; return;}
 snapshot(snap[0]); v0=value_of(snap[0]);
 if(v0) { /* not at the top of video memory: put it there first */
  (void)bios_start(0,0); snapshot(snap[0]); v0=value_of(snap[0]);
  if(v0) {fl_reason=5; return;}
 }
  /* v10.62: the unit of the VX855 start address (2-byte steps, CR0C/0D/34/48) was verified against the BIOS on hardware in every mode.
   * The BIOS test lines are not needed, and calling INT 10h 4F07 left the next DirectDraw mode set shifted sideways. Only make sure the start is 0. */
  crtc_set(0); snapshot(snap[0]); v0=value_of(snap[0]); if(v0) {fl_reason=5; return;} /* v10.63: always commit 0 through CR34 */
  fl_shift=1; fl_changed=0; fl_cal_y=0; fl_cal_v=0; fl_state=VXF_ST_DIRECT;
}

static void ensure(void) {
 forget_if_changed();
 if(fl_state==VXF_ST_NONE&&fl_reason==2) fl_state=VXF_ST_UNTESTED; /* the desktop surface was busy: ask again */
 if(fl_state==VXF_ST_UNTESTED) calibrate();
}

static int apply(DWORD off) {
 if(fl_state==VXF_ST_DIRECT) {
  crtc_set(off>>fl_shift); fl_last_v=off>>fl_shift; return 1;
 }
 return 0;
}

void VX3DN_ev(uint32_t type,uint32_t arg); /* v10.394 */
static DWORD flip_to(DWORD off,DWORD flags) {
 DWORD frames,n,vsize=hda->vram_size,need; unsigned short p; int sync;
 VX3DN_ev(12,off>>12);
 if(!vsize||(hda->vram_size_virt&&hda->vram_size_virt<vsize)) vsize=hda->vram_size_virt;
 ensure();
 if(fl_state!=VXF_ST_DIRECT) return VXF_UNSUPPORTED;
 need=hda->height*hda->pitch;
 if(off>vsize||need>vsize-off) return VXF_UNSUPPORTED;
 if(fl_iga2&&(off&7UL)) return VXF_UNSUPPORTED; /* .598: IGA2's address counts 8 bytes */
 if(fl_state==VXF_ST_DIRECT) {if(off&((1UL<<fl_shift)-1)) return VXF_UNSUPPORTED; if((off>>fl_shift)>0x3FFFFFFUL) return VXF_UNSUPPORTED;}
 else if(off%hda->pitch) return VXF_UNSUPPORTED;
 /* .555: DDFLIP_NOVSYNC (flags bit 3) is carried out as the one-frame flip. Until .554 it made a CPU flip "shown at once, torn or not"
  * with no pending flip kept. But the chip takes a new start address at the next blank only (below: "the picture is taken from the new
  * address from the next blank on"), so the old front stayed on the screen for up to a refresh while the application cleared and drew
  * its next frame into it, unguarded (board, GTA3 with Frame Sync off on .554: the picture flashed like a strobe; with it on, fine).
  * Now: in the ring when it runs (no wait in the flip), else the CPU flip that returns once the address is written; either way a draw,
  * clear, Blt or Lock on the old front waits until the display has taken the flip. Nothing is torn and nothing is shown half drawn;
  * an application cannot flip more often than the refresh.
  * .696 (a .637 report, 3DMark2000 on a Wyse Cx0: frame rates locked to 30, 15 and 7.5): that one-frame flip makes a frame that misses a
  * refresh wait for the next, and a ring flip's 6C wait idles the engine to the blank, so even triple buffering locks to 60 / n. VIA's
  * manual: CR32 bit 0 = real-time flipping, "flip by each scan line". A no-vsync flip (tear) now sets it and is taken from the next line:
  * the old front is not scanned past that line, so drawing into it may start at once (a tear line, no black flash as on .554, whose
  * address waited for the frame). In the ring: the swap without the 6C wait; by the CPU: the address written, nothing pending. IGA2
  * has no documented line flip: a panel on IGA2 alone keeps the one-frame flip; IGA2 mirroring tears only with ACCEL_FLIP_TEAR 2. */
 p=st1_port(); sync=fl_vb==1;
 frames=(flags>>24)&7; if(frames<1) frames=1;
 {DWORD t=VX_tsc(),old=fl_display,res=VXF_OK,w;
 int tear;
 iga2_look(); /* .695: looked at now (a ring flip does not go through crtc_set, which looked before) */
 tear=(flags&8UL)&&frames==1&&vx_flip_tear&&!fl_blank2&&(!fl_iga2||vx_flip_tear>=2); /* .696 */
 if((flags&8UL)&&!tear) vx_flip_tear_stat[2]++;
 if((sync||tear)&&vx_flip_async&&frames==1&&fl_state==VXF_ST_DIRECT&&!(off&7)&&!fl_blank2&&VX3DN_ring_flip_ok()) { /* .695 (review of .694): a mirrored IGA2 (fl_iga2) goes in the ring too, its own swap words (VX3DN_ring_flip two); a panel shown by IGA2 alone (fl_blank2) does not: the ring waits for IGA1's blank */ /* v10.417: XP's flip in the ring; .694 (review of .693): not while IGA2 shows this frame buffer too (its start address is written by crtc_set only: the ring's swap moves IGA1 alone) */
  if(fl_pend) {extern int VX_smp_wait_begin(void);extern void VX_smp_wait_end(int);int w=VX_smp_wait_begin();fl_wait_taken();VX_smp_wait_end(w);} /* v10.439 */ /* v10.434 (the sampler); the flip before has run (a ring flip: its fence; at most one frame queued) */
  if(!fl_ring) {if(!fl_rt) cr32_bit0(1); fl_ring=1; fl_rt=0;} /* .696: already set by a CPU tear flip */
  if(VX3DN_ring_flip(off,fl_iga2,tear)) {
   if(tear) vx_flip_tear_stat[0]++;
   fl_last_req=off; fl_display=off; fl_flips++; fl_last_v=off; vx_flip_ring_stat[3]++; fl_eng_t=0; if(fl_iga2) vx_flip_two[5]++;
   if(old!=off) {fl_pend=1; vx_flip_ring_pend=1; vx_flip_old_front=old; fl_seq++;}
   vx_frame_stat[14]++;
   goto timed;
  }
 }
 if(fl_ring) {vx_flip_ring_stat[2]++; ring_off();} /* v10.417: a CPU flip after ring flips */
 if(fl_eng_t==0xFFFFFFFFUL) { /* v10.417: the engine wait left out for a ring flip, done now; .573 (peer review): its answer used as VX_flip_control uses it (the address was written with the engine lost or still drawing) */
  DWORD t2=VX_tsc();int blocked;
  vx3dn_rt_why=4;blocked=VX3DN_busy();
  fl_eng_t=(VX_tsc()-t2)>>8;vx_frame_stat[7]+=fl_eng_t;
  if(blocked)return VX2D_is_lost()?VXF_LOST:VXF_FAILED;
 }
 if(tear) { /* .696: the CPU's tear flip: real-time flipping on, the address taken from the next scan line; nothing pending */
  if(!fl_rt) {cr32_bit0(1); fl_rt=1;}
  if(!apply(off)) return VXF_FAILED;
  fl_last_req=off; fl_display=off; fl_flips++; fl_pend=0; vx_flip_old_front=0xFFFFFFFFUL; vx_flip_tear_stat[1]++; vx_frame_stat[14]++;
  goto timed;
 }
 if(fl_rt) {cr32_bit0(0); fl_rt=0;} /* .696: a flip by the frame again */
 if(sync) fl_wait_taken(); /* v10.384: the flip before is shown first (a new address now would replace it unseen) */
 else {fl_pend=0; vx_flip_old_front=0xFFFFFFFFUL;} /* no usable blank status (fl_vb): shown when the chip takes it, nothing to wait on */
 /* never write the address while a blank is running: it may be the very moment the chip latches it */
 if(sync&&in_blank(p)) (void)vb_wait(p,0,40);
 if(!apply(off)) return VXF_FAILED;
 fl_last_req=off; fl_display=off; fl_flips++;
 if(sync&&vx_flip_async&&frames==1) { /* v10.384: return now; the old front is waited for when it is drawn into */
  DWORD raw=VX2D_mmio_read(0x1290UL);
  fl_apply_ms=Get_System_Time(); fl_apply_line=raw>>16; fl_line_ok=!fl_blank2&&raw!=0xFFFFFFFFUL&&(raw>>16)<hda->height; /* .598: the line counter is the first unit's */
  if(old!=off) {fl_pend=1; vx_flip_old_front=old; vx_flip_epoch++; fl_seq++;} /* .620: one count a CPU flip (the VX900's wait word in the ring) */
  vx_frame_stat[14]++;
 } else if(sync) {
 /* The picture is taken from the new address from the next blank on; from the start of that blank the old
  * front buffer is no longer being scanned, so the application may draw into it. */
  for(n=0;n<frames;n++) if(!vb_begin(p,60)) {fl_timeouts++; res=VXF_TIMEOUT; break;}
 } else if(fl_blank2&&old!=off) { /* .598: IGA2 without a usable blank status: the old front is scanned until IGA2's next blank, so a frame's time passes before the application may draw into it */
  DWORD t0=Get_System_Time(),ms=18UL*frames; while((DWORD)(Get_System_Time()-t0)<ms) ; vx_flip_two[4]++;
 }
 timed:
 w=(VX_tsc()-t)>>8; vx_frame_stat[8]+=w; if(w>vx_frame_stat[11]) vx_frame_stat[11]=w;
 /* the time between flips and the worst 8 */
 {DWORD now=Get_System_Time(),iv=fl_last_flip_ms?now-fl_last_flip_ms:0,k,lo=0;fl_last_flip_ms=now;vx_frame_stat[0]++;
  {extern void VX_smp_frame(uint32_t);VX_smp_frame(iv);} /* v10.434 */
  if(iv){vx_frame_stat[iv<20?1:iv<35?2:iv<50?3:iv<75?4:iv<100?5:6]++;vx_frame_stat[15]+=iv;if(iv>vx_frame_stat[9])vx_frame_stat[9]=iv;
   for(k=1;k<8;k++)if(vx_frame_worst[k][0]<vx_frame_worst[lo][0])lo=k;
   if(iv<2000&&iv>vx_frame_worst[lo][0]){vx_frame_worst[lo][0]=iv;vx_frame_worst[lo][1]=fl_eng_t;vx_frame_worst[lo][2]=w;}}}
 return res;}
}

/* v10.73: one scan line sample. A line read while the blank is on, or one that straddles a blank, is not trusted. */
static void scan_sample(uint32_t *r) {
 DWORD i,raw,line; unsigned short p=st1_port();
 r[VXF_R_RESULT]=VXF_OK; r[VXF_S_STATUS]=0; r[VXF_S_GATE]=vx_scan_enable; r[VXF_S_HEIGHT]=hda->height;
 for(i=0;i<4;i++) {
  r[VXF_S_TRIES]=i+1;
  /* .728 (Fable's check of Gemini's review, #6): the blank of the unit that shows the picture (in_blank: IGA2's 204h bit 2 when
   * fl_blank2); 1290h counts IGA1's lines, so with the picture on IGA2 no line is given (status 3, as a line never in range) */
  if(in_blank(p)) {r[VXF_S_STATUS]=2; return;}
  if(fl_blank2) {r[VXF_S_STATUS]=4; return;} /* .737 (fourth review #14): no line for IGA2 (status 4); it left the loop as 3, which the HAL
  * answers as "vertical blank in progress", so a program polling for the blank's end during scan-out never saw it end */
  raw=VX2D_mmio_read(0x1290UL); r[VXF_S_RAW]=raw;
  if(in_blank(p)) {r[VXF_S_STATUS]=2; return;}
  if(raw==0xFFFFFFFFUL) return;
  line=raw>>16;
  if(line<hda->height) {r[VXF_S_LINE]=line; r[VXF_S_STATUS]=1; return;}
 }
 r[VXF_S_STATUS]=3;
}
static void flip_control_unlocked(const vxf_request *q,uint32_t *r) {
 DWORD res=VXF_OK,i; unsigned short p; vxf_request qc;
 if(q) {const unsigned char *a=(const unsigned char *)q; unsigned char *b=(unsigned char *)&qc; for(i=0;i<sizeof(qc);i++) b[i]=a[i]; q=&qc;} /* v10.139 (review F19): copy the request before the reply is cleared (they may share a buffer) */
 for(i=0;i<VXF_REPLY_DWORDS;i++) r[i]=0;
 {void VX2D_sync_2d(void);if(q&&q->cmd==VXF_FLIP&&fl_eng_t==0xFFFFFFFFUL)VX2D_sync_2d(); /* v10.419: a ring flip: the 2D side only */
  else VX2D_sync();} /* v10.95: an asynchronous DirectDraw blit may still be writing the surface about to be shown (or being hidden) */
 if(!hda||!q||q->cb!=sizeof(*q)) {r[VXF_R_RESULT]=VXF_FAILED; return;}
 switch(q->cmd) {
  case VXF_QUERY: ensure(); if(fl_state!=VXF_ST_DIRECT) res=VXF_UNSUPPORTED; break;
  case VXF_FLIP: res=VX2D_lost()?VXF_LOST:flip_to(q->arg,q->flags); break; /* v10.105 */
  case VXF_WAITVB:
   forget_if_changed();
   if(!k_set||fl_vb==0) {if(!k_set) {remember_mode(); fl_state=VXF_ST_UNTESTED;} vb_probe();}
   if(fl_vb!=1) {res=VXF_UNSUPPORTED; break;}
   p=st1_port();
   if(q->arg==0) {r[VXF_R_IN_VB]=in_blank(p)?1:0; break;}
   if(q->arg==1) {if(!vb_begin(p,60)) {fl_timeouts++; res=VXF_TIMEOUT;} break;}
   if(q->arg==2) {if(!vb_wait(p,1,60)||!vb_wait(p,0,60)) {fl_timeouts++; res=VXF_TIMEOUT;} break;}
   res=VXF_UNSUPPORTED; break;
  case VXF_RESET:
   forget_if_changed();
   ring_off(); /* v10.417 */
   if(fl_rt) {cr32_bit0(0); fl_rt=0;} /* .696 */
   fl_pend=0; vx_flip_old_front=0xFFFFFFFFUL; /* v10.384 */
   if(fl_state==VXF_ST_DIRECT) { /* v10.63: commit 0 even when the bookkeeping says the desktop is shown */
    if(apply(0)) fl_display=0; else res=VXF_FAILED;
   } else crtc_set(0);
   break;
  case VXF_INFO: break;
  case VXF_SCAN: scan_sample(r); return;
  default: res=VXF_UNSUPPORTED;
 }
 r[VXF_R_RESULT]=res; r[VXF_R_STATE]=fl_state; r[VXF_R_SHIFT]=fl_shift; r[VXF_R_VBLANK]=fl_vb; r[VXF_R_DISPLAY]=fl_display;
 r[VXF_R_FLIPS]=fl_flips; r[VXF_R_BIOS]=fl_bios; r[VXF_R_REASON]=fl_reason; r[VXF_R_CAL_Y]=fl_cal_y; r[VXF_R_CAL_V]=fl_cal_v;
 r[VXF_R_CHANGED]=fl_changed; r[VXF_R_FRAME3MS]=fl_frame3; r[VXF_R_TIMEOUTS]=fl_timeouts; r[VXF_R_LAST_REQ]=fl_last_req; r[VXF_R_LAST_V]=fl_last_v;
}
/* Scanout writes and BIOS calibration share the engine's critical section.
 * The read-only INFO request remains available after native device loss. */
int VX3DN_busy(void);
int VX3D_busy(void);
int VX3D_lab_busy(void);
void VX_flip_control(const vxf_request *q,uint32_t *r) {
 DWORD i,cmd=q?q->cmd:VXF_INFO;int owner_busy;
 vx_lock();
 if(q&&cmd==VXF_PRESENT) { /* .661: VXD3DMON's sampler had no frames for a windowed program (VXTLP: its line samples were never kept) */
  static DWORD last_present;DWORD now=Get_System_Time();
  for(i=0;i<VXF_REPLY_DWORDS;i++)r[i]=0;
  if(q->cb==sizeof(*q)&&(!fl_last_flip_ms||(DWORD)(now-fl_last_flip_ms)>1000UL)){DWORD iv=last_present?now-last_present:0;extern void VX_smp_frame(uint32_t);VX_smp_frame(iv);}
  last_present=now;vx_unlock();return;
 }
 if(q&&cmd==VXF_FRONT) { /* v10.384: no engine wait: only the pending flip */
  DWORD cb=q->cb,arg=q->arg; /* .573 (peer review): the request and the reply may share a buffer (as v10.139 below); the size checked here too */
  for(i=0;i<VXF_REPLY_DWORDS;i++)r[i]=0;
  if(cb!=sizeof(*q)) r[VXF_R_RESULT]=VXF_FAILED;
  else {
   if(arg!=0xFFFFFFFFUL) VX_flip_front_wait(arg); else (void)fl_taken();
   r[VXF_R_IN_VB]=fl_pend?1:0; r[VXF_R_DISPLAY]=fl_display;
  }
  vx_unlock(); return;
 }
 {uint32_t t=VX_tsc();
 /* v10.417: a flip that can go in the ring does not wait for the engine: the ring runs the frame's draws before it (flip_to waits
  * as before when it turns out to be a CPU flip, fl_eng_t = FFFFFFFF) */
 if(cmd==VXF_FLIP&&vx_flip_async&&!fl_blank2&&VX3DN_ring_flip_ok()) /* .695: a mirrored IGA2 too */{owner_busy=VX3D_lab_busy();fl_eng_t=0xFFFFFFFFUL;}
 else{vx3dn_rt_why=4;owner_busy=VX3DN_busy()||VX3D_lab_busy(); /* v10.309 (review): VX3D_busy would run VX3DN_busy's status reads a second time */
 if(cmd==VXF_FLIP){fl_eng_t=(VX_tsc()-t)>>8;vx_frame_stat[7]+=fl_eng_t;if(fl_eng_t>vx_frame_stat[10])vx_frame_stat[10]=fl_eng_t;}}}
 if(q&&cmd!=VXF_INFO&&(owner_busy||(vx_native_display_hold&&!VX2D_is_lost()))) {
  for(i=0;i<VXF_REPLY_DWORDS;i++)r[i]=0;
  r[VXF_R_RESULT]=cmd==VXF_FLIP&&VX2D_is_lost()?VXF_LOST:VXF_FAILED;
 } else flip_control_unlocked(q,r);
 vx_unlock();
}
