/* VX855 native synchronous 2D, v10.58. No DMA, host-window or glyph writes.
 * Register recipes: supplied hardware notebook, sections 1/2/3 and 11.1.
 * Results distinguish preflight fallback from a command that has fired. */
#include "winhack.h"
#include "vmm.h"
#include "vxd_lib.h"
#include "vx85598.h"
#include "3d_accel.h"
#include "vx855_2d.h"
#define IO_IN8
#define IO_OUT8
#include "io32.h"
extern FBHDA_t *hda;
int VX3D_busy(void);
int VX3D_lab_busy(void);int VX3DN_stopped(void); /* .575 */
extern volatile uint32_t vx3dn_rt_why; /* v10.473: the retire's cause (vx_native3d_vxd.c) */
unsigned long VX_irq_save(void);
void VX_irq_restore(unsigned long flags);
#pragma aux VX_irq_save = "pushfd" "pop eax" "cli" value [eax] modify exact [eax];
#pragma aux VX_irq_restore = "push eax" "popfd" parm [eax] modify exact [];
/* v10.58zc: cycle counter (RDTSC, low 32 bits) for the per-phase cost profile read by VX2BENCH. */
unsigned long VX_tsc(void);
#pragma aux VX_tsc = 0x0F 0x31 value [eax] modify exact [eax edx];
/* .515: ACCEL_PRIVLOCK (1): the engine's own lock in place of the system critical section (Begin_Critical_Section) for the 2D, 3D and
 * flip paths (board, GTA3 .514: clears waited 85-194 ms for the system critical section while KRNL386 code and a VMM entry point at
 * 003B:02xx ran on other threads, most likely paging). It is recursive for its owner thread; a waiter blocks on a VMM semaphore. Mode sets
 * take it after the system critical section (the order wherever both are held: the critical section, then this lock: GDI stretches and
 * transfers, which lock pages, and VDD callbacks that may come with it held), so no engine user runs during one; the 3D paths take this
 * lock alone. 0: the system critical section as before. Set once (vx_lock_init, after the registry is read) before any engine call.
 * vx_lk: [0] outermost entries, [1] of them waited, [2] the waits' time (TSC / 256), [3] the longest wait, [4] waits of 2 ms and more,
 * [5] the longest hold, [6] holds of 2 ms and more, [7] 1 the private lock, 2 the system critical section (TEXDBG BC DB, DC clears) */
volatile unsigned long vx_privlock=1;
uint32_t vx_lk[8];
static uint32_t lk_sem,lk_on,lk_owner,lk_depth,lk_waiters,lk_t0,lk_ms2=7800UL;
extern volatile uint32_t vx_cs_waiter;
void vx_lock_init(uint32_t cyc_per_ms) {
 if(cyc_per_ms)lk_ms2=(cyc_per_ms*2UL)>>8;
 if(lk_on||lk_owner)return;
 if(vx_privlock&&!lk_sem)lk_sem=Create_Semaphore(0);
 lk_on=vx_privlock&&lk_sem;vx_lk[7]=lk_on?1UL:2UL;
}
void vx_lock(void) {
 uint32_t t;unsigned long f;
 if(!lk_on){Begin_Critical_Section(0);return;}
 t=(uint32_t)Get_Cur_Thread_Handle();f=VX_irq_save();
 if(lk_owner==t){lk_depth++;VX_irq_restore(f);return;}
 if(!lk_owner){lk_owner=t;lk_depth=1;lk_t0=VX_tsc();vx_lk[0]++;VX_irq_restore(f);return;}
 {uint32_t a=VX_tsc(),w;
  vx_cs_waiter=t;
  while(lk_owner){lk_waiters++;VX_irq_restore(f);Wait_Semaphore(lk_sem,BLOCK_SVC_INTS|BLOCK_THREAD_IDLE);f=VX_irq_save();lk_waiters--;}
  lk_owner=t;lk_depth=1;lk_t0=VX_tsc();if(vx_cs_waiter==t)vx_cs_waiter=0;
  w=(lk_t0-a)>>8;vx_lk[0]++;vx_lk[1]++;vx_lk[2]+=w;if(w>vx_lk[3])vx_lk[3]=w;if(w>=lk_ms2)vx_lk[4]++;}
 VX_irq_restore(f);
}
/* .515 (bug check): 1 when the current thread holds the engine lock (FBHDA_lock: the engine paths held the system critical section before,
 * and FBHDA_lock skipped hda_sem for them; without this every 3D packet waited on hda_sem, with the sequencer window open) */
int vx_lock_owned(void) {return lk_on&&lk_owner&&lk_owner==(uint32_t)Get_Cur_Thread_Handle();}
void vx_unlock(void) {
 unsigned long f;uint32_t sig=0;
 if(!lk_on){End_Critical_Section();return;}
 f=VX_irq_save();
 if(lk_owner==(uint32_t)Get_Cur_Thread_Handle()&&lk_depth&&!--lk_depth){
  uint32_t h=(VX_tsc()-lk_t0)>>8;if(h>vx_lk[5])vx_lk[5]=h;if(h>=lk_ms2)vx_lk[6]++;lk_owner=0;sig=lk_waiters!=0;}
 VX_irq_restore(f);
 if(sig)Signal_Semaphore(lk_sem);
}
static uint32_t vx_rev8(uint32_t b) {b=((b&0xF0UL)>>4)|((b&0x0FUL)<<4);b=((b&0xCCUL)>>2)|((b&0x33UL)<<2);return ((b&0xAAUL)>>1)|((b&0x55UL)<<1);} /* v10.309 */
static uint32_t ph_sum[8],ph_n; /* sum of cycle deltas >>4 per phase, over completed native operations */
/* v10.132: the phase profile only while armed (live control 0xF4000001 arms and clears it, 0xF4000000 disarms); off by default: 9 timestamp reads per operation saved.
 * v10.132: span batches: ACCEL_SPAN_WAIT / live 0xF2000000|v = what is waited for between the rectangles of one XF_SPANS batch: 0 the full idle (32 stable reads) after
 * each, as before; 1 nothing (the next rectangle's registers are written while the engine may still draw); 2 one idle read; 3 four stable idle reads. The last rectangle
 * always gets the full wait. v10.132: ACCEL_STR_FUSE / live 0xF3000000|v: 1-bit sources into 32bpp expand straight to screen pixels through a 16-entry nibble table (1, default). */
/* v10.135 (VX2SPAN8: the 8bpp colour-pattern difference is the same 11/12 pixels with every wait, including the full one: not the wait): ACCEL_SPAN_WAIT default 6 (one read for colour
 * patterns too). ACCEL_COL8_GUARD (live 0xEF000000|v, default 1): until the rule is known, 8bpp colour-pattern fills narrower than 16 pixels and 8bpp colour-pattern band batches go to DIBENG. */
/* v10.137 (VX2COL8 v1.1: 7680 single 8bpp colour-pattern fills at heights 1 to 3, every X and Y phase, all exact; VX2TIP: the driver's bands equal the star's region drawn with DIBENG
 * PatBlts pixel for pixel, while DIBENG's own Polygon shifts the pattern one pixel on its 2-3 pixel spans): the narrow-fill guard is gone; ACCEL_COL8_BANDS (live 0xEF000000|v,
 * default 1) = 8bpp colour-pattern band batches on the engine; 0 = to DIBENG (matches DIBENG's Polygon exactly, about 2.5 times slower). */
volatile unsigned long vx2d_col8=1,vx2d_col8_cfg=1;
volatile unsigned long vx2d_prof=0,vx2d_span_wait=5,vx2d_span_wait_cfg=5,vx2d_str_fuse=1,vx2d_str_fuse_cfg=1;
/* v10.133 (VX2OPT: one idle read between band rectangles drew the same pixels and was 3 times faster): ACCEL_SPAN_WAIT default 2. ACCEL_OP_WAIT (live 0xF1000000|v): after a single
 * engine operation (fill, copy, line) 0 = the full idle of 32 stable reads, 1 = one idle read (default). ACCEL_DREAD_ANY (live 0xF0000000|v): 1 = fills and copies whose ROP reads the
 * screen skip the size thresholds (DIBENG reads video memory at about 30 MB/s for them: a 96x96 PATINVERT took 517 us at 16bpp), 0 = thresholds for every ROP as before. */
volatile unsigned long vx2d_op_wait=1,vx2d_op_wait_cfg=1,vx2d_dread=1,vx2d_dread_cfg=1;
static uint32_t str_fused_rows;
static uint32_t flushv,m2d_linear,generation;
static int m2d_state,running;
static uint32_t fail_gen,lost_gen,recoveries,retries; /* v10.75: mode-set generation at which the engine went off (state 2) or lost (state 3) */
static int lost_fenced,hold_by_us;
static int native_fault; /* A lost Direct3D submission cannot use 2D-only recovery. */
static uint32_t vx2d_3d_resets; /* v10.281: engine resets by the 3D side (VX2D_3d_reset) */
static uint32_t gdi_off_seen; /* v10.73: GDI requests that reached the VxD while the DRV's enabled flag read 0 */
static volatile uint16_t *gdi_flags,*gdi_enabled;
/* v10.273: the DRV's deFlags (in GDI's PDEVICE) and wEnabled (in the DRV's data segment) were bound once, as linear addresses taken at
 * Enable. A 16-bit global block keeps its selector but can move in linear memory: on the board, after VXTEX's engine run, the VxD read
 * the enabled word as 0 at the bound address (every GDI request declined, reason 0204, so the whole desktop drew in software until a
 * restart) while the DRV's own wEnabled was 1 (its requests still came: the DRV checks wEnabled before calling). And the fences wrote
 * into whatever had taken that memory. Now the DRV binds selector:offset and the address is worked out from the selector's current
 * base (_SelectorMapFlat in the system VM) at every entry from the system VM. gdi_moves counts the times the base had changed. */
static uint32_t gdi_fp16,gdi_ep16,gdi_moves,gdi_maps,gdi_mapfail;
static volatile uint16_t *gdi_at(uint32_t p16,void *vm) {
 ULONG b;
 if(!p16||!vm) return 0;
 b=_SelectorMapFlat((ULONG)vm,p16>>16,0);
 if(b==0xFFFFFFFFUL||!b) {gdi_mapfail++; return 0;}
 return (volatile uint16_t *)(b+(p16&0xFFFFUL));
}
static void gdi_map(void) {
 void *vm; volatile uint16_t *f,*e;
 if(!gdi_fp16&&!gdi_ep16) return;
 vm=Get_Sys_VM_Handle(); f=gdi_at(gdi_fp16,vm); e=gdi_at(gdi_ep16,vm); gdi_maps++;
 if((gdi_flags&&f!=gdi_flags)||(gdi_enabled&&e!=gdi_enabled)) gdi_moves++;
 gdi_flags=f; gdi_enabled=e;
}
static unsigned char saved_unlock;
static vx2d_stats stats;
/* Shared-engine timeout from the Direct3D owner: fence DIB/cursor access
 * through the same existing words used by a failed 2D recovery. */
void VX3DN_ev(uint32_t type,uint32_t arg); /* v10.394: the driver's event log (vx_native3d_vxd.c) */
void VX2D_device_lost(void) {
 native_fault=1;m2d_state=3;lost_gen=vx_status.set_successes;lost_fenced=1;
 gdi_map(); /* v10.279 (review): the 3D side calls this without a 2D entry before it: the words' current place, not the last one mapped */
 if(gdi_flags)*gdi_flags|=0x0010;
 if(gdi_enabled)*gdi_enabled=0;
 vx_native_display_hold=1;
}
int VX2D_native_fault(void) {return native_fault;}
/* v10.281: the 3D side reset the engine (a hung draw): the 2D side initialises it again (e_first: its own reset and the clock check)
 * before its next operation, as after a mode set, instead of going on as if the engine still held what it set up (board: after VXDX7's
 * hang test the desktop showed wrong blocks where Explorer redrew under the pointer). Only state 1 (working) goes back to 0. */
void VX2D_3d_reset(void) {if(m2d_state==1) m2d_state=0; vx2d_3d_resets++;}
/* v10.282: the 3D side's clears (vx_native3d_vxd.c): a fill of an offscreen surface submitted with VX2D_ASYNC while vx2d_own3d is set is
 * left running as a DirectDraw blit in exclusive mode is (v10.95): its completion is the next call that touches the engine, the frame
 * buffer or the display (complete_pending: every 2D call, VX2D_access for a Lock, VX2D_busy at every 3D submit, flips, mode sets).
 * VX2D_pending says whether one is running. */
volatile uint32_t vx2d_own3d;
volatile unsigned long vx2d_ops,vx2d_fallbacks;
/* .531: vx_d3d_ix384 (ACCEL_D3D_IX384, out[2] bit 6) removed: always 384 */
volatile unsigned long vx_d3d_rttex=1,vx_d3d_mipcap=1,vx_d3d_texhint=1,vx_d3d_indexed=0,vx_d3d_rtnowait=1,vx_d3d_tnl=0; /* .534: ACCEL_D3D_TNL (out[2] bit 21; a feature under test, default 0) */ /* v10.477: ACCEL_D3D_RTNOWAIT (out[2] bit 12) */ /* v10.463: ACCEL_D3D_TEXHINT (out[2] bit 5; .527: always 1); .528: bit 6 ACCEL_D3D_TEXRUNS (was v10.468's ACCEL_D3D_PFVV2, removed in .527); v10.470: ACCEL_D3D_INDEXED (bit 7: 1 when the vertex area is there) */ /* v10.456: ACCEL_D3D_RTTEX, ACCEL_D3D_MIPCAP (the HAL's out[2] bits 3 and 4, bit 11 valid) */
volatile unsigned long vx_d3d_infocache=1,vx_d3d_v8=1; /* .524: IXPF removed (bit 18 free) */ /* .523: ACCEL_D3D_V8 (out[2] bit 19) */ /* .522: ACCEL_D3D_INFOCACHE, ACCEL_D3D_IXPF (out[2] bits 17, 18) */
volatile unsigned long vx_d3d_ixpass=1; /* .515: ACCEL_D3D_IXPASS (out[2] bit 16); .529: ACCEL_D3D_HWCULL (bits 14-15) removed, .528's TEXRUNS bit 6 retired: always on */
volatile unsigned long vx_d3d_feat_cfg=0x0787FFFFUL; /* .527: bits 27-30 clear (no switch: PAIRLIST, FASTVV, VRECIPE, FASTMATCH removed) */ /* v10.454: bit 28 ACCEL_D3D_FASTVV, default 1 */ /* v10.444: bit 28 clear (v10.449: ACCEL_D3D_QDIRECT's, removed) */ /* v10.362: bit 19 (ACCEL_3D_CLEAR3D) off, its default; v10.361: bit 23 (ACCEL_3D_GUARD) on, its default; v10.357: the opt-in bits 20-22 (STRICT, EXACTKEY, DDRAW_3DBLT; v10.358: 23 ACCEL_3D_GUARD) off before the registry is read, as their defaults; v10.212: ACCEL_D3D_STAGES .. ACCEL_D3D_CUBE, bits 0-8 (vxd_main.c) */
volatile unsigned long vx2d_copy_all=1,vx2d_enable=1;
volatile unsigned long vx2d_copy_all_cfg=1,vx2d_enable_cfg=1;
/* v10.58b: per-feature switches (registry ACCEL_16BPP, ACCEL_PATTERN, ACCEL_CLIP, ACCEL_DDRAW; default on). A switched-off feature falls back to software. */
/* v10.58c: per-call overhead is ~0.4 ms, so tiny ops stay on the CPU (registry ACCEL_FILL_MIN / ACCEL_COPY_MIN in pixels; 0 = always native). The 58zc build applies them to DirectDraw as well. */
volatile unsigned long vx2d_fill_min=2304UL,vx2d_copy_min=256UL,vx2d_force=0; /* v10.58zd: lowered with the per-op cost (was 131072 / 1024) */
/* v10.90: lines. The engine draws a plain pen copy only (VX2LINE: the ROP field is ignored for 00/FF/0F and the ROPs that read the destination hang it), so the DRV turns R2_BLACK, R2_NOTCOPYPEN and R2_COPYPEN into the colour. vx2d_line_min = registry ACCEL_LINE_MIN (pixels along the major axis), vx2d_fline = ACCEL_LINES. */
volatile unsigned long vx2d_line_min=32UL,vx2d_fline=1;
/* v10.142 (review O14): fills under ACCEL_FILL_MIN that are at least vx2d_fill_tall rows high still go to the engine: DIBENG's cost grows with the row count
 * (VX2TUNE 8bpp: 8x256 94 us in DIBENG against 52 on the engine, while 256x8 is 37 against 41). Registry ACCEL_FILL_TALL (rows, 0 = off), live 0xED000000 | rows. */
volatile unsigned long vx2d_fill_tall=256UL,vx2d_prefilter=1UL,vx2d_fill_tall_cfg=256UL,vx2d_prefilter_cfg=1UL; /* vx2d_prefilter: registry ACCEL_PREFILTER, live 0xEC000000 | v; 0 = the DRV sends everything (no published thresholds) */
/* v10.142 (review O14): the thresholds are published in the FBHDA block (res0 = 'VX' << 16 | bit 0 force | bit 1 screen-reading ROPs ignore the thresholds, res1 fill,
 * res2 line, res3 copy, rem2 tall rows) so the DRV can keep a request that would be declined for its size without the trip into the VxD. Republished after every
 * control call and at bind; a DRV that sees no 'VX' sends everything as before. */
static void vx2d_publish(void) {
 extern volatile unsigned long vx2d_dread;
 if(!hda) return;
 hda->res1=vx2d_fill_min; hda->res2=vx2d_line_min; hda->res3=vx2d_copy_min; hda->rem2=vx2d_fill_tall;
 hda->res0=vx2d_prefilter?(0x56580000UL|(vx2d_force?1UL:0UL)|(vx2d_dread?2UL:0UL)):0UL;
 {extern volatile unsigned long vx2d_gdi,vx2d_linemax,vx2d_linemaxy; uint32_t x=vx2d_linemax>>4,y=vx2d_linemaxy>>4; if(x>63) x=63; if(y>63) y=63;
  ((volatile uint16_t *)&hda->rem3)[1]=(uint16_t)(0x8000U|(uint16_t)(vx2d_gdi&3UL)|(uint16_t)(x<<2)|(uint16_t)(y<<8));} /* v10.507: the driver's half of rem3 (bits 15:0, its busy count) is not touched */
}
/* .530: v10.92's styled lines (ACCEL_DASH, default off: no engine line pattern drew the DIB engine's styles) removed; the DIB engine draws them */
/* v10.95: asynchronous DirectDraw blits. vx2d_async = live switch (registry ACCEL_ASYNC, default 0 until VX2ASYNC has passed on the machine), vx2d_async_gate = a DirectDraw exclusive mode is active (set by the HAL, cleared by it, and by any GDI request). */
volatile unsigned long vx2d_async=0,vx2d_async_cfg=0,vx2d_async_gate=0;
/* v10.424: asynchronous OpenGL presents. VXGL marks its swap's blits to the primary (VX_DD_Present); the HAL arms the VxD around them
 * (control 0xFFFFFE21, disarmed by 0xFFFFFE20 after the Blt, by any GDI request, and by a mode set), and an armed DirectDraw copy runs
 * asynchronously outside an exclusive mode too: SwapBuffers returns while the engine copies, and the copy is finished by whoever comes
 * next (the next frame's 3D submit, a Lock, any 2D call), as in an exclusive mode. Registry ACCEL_ASYNC_PRESENT (default 1; live
 * 0xFFFFFE22 off / 0xFFFFFE23 on; 0xFFFFFE24 reads: presents run asynchronously, armed, the switch, ACCEL_ASYNC). Needs ACCEL_ASYNC. */
volatile unsigned long vx2d_async_present=1,vx2d_async_present_cfg=1,vx2d_present_arm=0;static uint32_t present_async_ops;
/* v10.99/v10.100: CPU conversion and replication of DIB sources (StretchDIBits, DibToDevice) into the 16bpp or 32bpp screen. vx2d_str_mask = the verified classes (registry ACCEL_STRETCH, default 0 until VX2STRX has passed).
 * 16bpp screen: bit 0 555 source with both factors >= 2, bit 1 24-bit both >= 2, bit 2 32-bit both >= 2, bit 3 24/32-bit with one factor 1 (the other >= 2), bit 4 555 with one factor 1, bit 5 24/32-bit 1:1, bit 6 555 1:1.
 * 32bpp screen (32-bit sources are already fast in DIBENG): bit 7 555 source 1:1 or both factors >= 2, bit 8 555 one factor 1, bit 9 565 1:1 or both >= 2, bit 10 565 one factor 1, bit 11 24-bit 1:1 or both >= 2, bit 12 24-bit one factor 1. */
volatile unsigned long vx2d_str_mask=0,vx2d_str_cfg=0;
/* v10.104: 1-bit, 4-bit and RLE sources into the 32bpp screen (registry ACCEL_STRETCH2, live control 0xFA000000|mask, default 0 until VX2STR2 has passed). Bits per source kind and scale (kind*3+shape; shape 0 both factors >= 2, 1 one factor 1, 2 1:1):
 * 1-bit 1,2,4; 4-bit 8,16,32; RLE8 64,128,256; RLE4 512,1024,2048. Into the 16bpp screen these sources stay with DIBENG (its palette path is as fast as the CPU one). */
volatile unsigned long vx2d_str_mask2=0,vx2d_str_cfg2=0;
static uint32_t rle_ops,rle_declined,rle_pixels;
#define RLE_MAX 262144UL /* largest decoded RLE bitmap taken (width x height): 512x512, 640x400 */
static uint8_t rle_buf[RLE_MAX]; /* the decoded bitmap, one index byte a pixel, bottom line first */
static uint8_t str_unp[4100]; /* one run of a 1-bit or 4-bit row unpacked to index bytes */
static uint32_t str_ops,str_skips,str_pixels;
static uint32_t str_src[4100]; /* one converted source run */
static uint32_t str_line[4100]; /* one converted destination line (16bpp pixels in the low halves) */
static uint32_t async_ops,async_waits;
static struct {int active,excluded,opened,kmode; uint32_t saved[19]; uint32_t t0;} pend; /* v10.142: t0 = start time (Get_System_Time) for the status query */
int VX2D_pending(void) {return pend.active;} /* v10.282 */
static int hc_pos_ok; /* v10.142: the hardware cursor position registers hold the last position (cleared by hide, shape loads and engine resets) */
static volatile int completing;
volatile unsigned long vx2d_f16=1,vx2d_fpat=1,vx2d_fclip=1,vx2d_fddraw=1,vx2d_f8=1; /* v10.76: vx2d_f8 = registry ACCEL_8BPP */
/* v10.77: colour pattern fills read an 8x8 block from video memory (VX2COL). vx2d_brush_off is the byte offset of 4 KB reserved at the end of video memory (set by vxd_main.c, 0 = none),
 * vx2d_fcol = registry ACCEL_COLORBRUSH. */
volatile unsigned long vx2d_brush_off=0,vx2d_fcol=1;
static const uint32_t saved_regs[]={4,8,12,16,20,24,28,32,36,40,44,48,64,68,72,76,80,88,92};
/* v10.144 (ACCEL_KEEP, live 0xEB000000|v; default 1 since v10.145): the engine stays open between operations. The first operation unlocks the
 * sequencer (e_begin) and saves the 19 registers once; later operations neither reopen nor save/restore. Every command already writes each register it uses
 * (audited: the prologue writes 4, 8, 14, 1C, 20, 30, 48, 58, 5C; each operation the rest it needs, and its command bits gate clip, keys and patterns).
 * The context is handed back (registers restored, sequencer lock put back) by k_release: VX2D_sync (mode sets, DOS boxes, flips), VX2D_busy (3D),
 * VX2D_bind, the live switch going off, Windows exit, an engine timeout or loss, and before e_first / lost_recover. A context from an older mode generation
 * is dropped without writing its registers back. No critical section, semaphore or interrupt state is held between calls. */
volatile unsigned long vx2d_keep=1,vx2d_keep_cfg=1; /* v10.145: on by default (VX2KEEP: 4 modes x 3 captures byte-identical, registers handed back intact; 32x32 fill 21.4 -> 8.9 us) */
static struct {int open; uint32_t gen; uint32_t saved[19];} keep;
static uint32_t keep_opens,keep_releases;
static void flush(void) {
 _asm {
  xor eax,eax
  xchg [flushv],eax
 }
}
static unsigned char sq_get(unsigned char i) {outp(0x3C4,i); return inp(0x3C5);}
static void sq_put(unsigned char i,unsigned char v) {outp(0x3C4,i); outp(0x3C5,v);}
static uint32_t rd(uint32_t r) {return *(volatile uint32_t*)(m2d_linear+r);}
static void wr(uint32_t r,uint32_t v) {*(volatile uint32_t*)(m2d_linear+r)=v;} /* v10.58c: no per-write readback; one flush before the command */
/* v10.58zd: the 58zc profile showed ~180 us in "command + idle" even for an 8x8 operation, while two status reads cost 0.6 us.
 * The loop called Get_System_Time (a VMM service, several us) on every poll. The clock is now consulted every 256 polls only. */
/* v10.402: with XP's ring (vx_native3d_vxd.c) paused in system memory the command regulator's bit 4 may stay set: idle when 444
 * bit 31 says the fetcher is paused, once that has been seen */
extern volatile uint32_t vx3dn_xr_cr_pause;
static int idle_bits(uint32_t s) {return !(s&0x1FF7)||(vx3dn_xr_cr_pause&&(s&0x1FF7)==0x10&&(rd(0x444)&0x80000000UL));}
static int idle_n(int want) {
 uint32_t start=0,n; int stable=0,have=0;
 for(n=0;n<2000000UL;n++) {
  uint32_t s=rd(0x400); stats.last_status=s;
  if(s==0xFFFFFFFFUL) return 0;
  if(idle_bits(s)) {if(++stable>=want) return 1;} else stable=0;
  if((n&255)==255) {
   if(!have) {start=Get_System_Time(); have=1;}
   else if((uint32_t)(Get_System_Time()-start)>=100) break;
  }
 }
 return 0;
}
static int idle(void) {return idle_n(32);}
/* v10.58zc: before programming a command the engine only has to be idle now. Our own previous command already waited for
 * 32 stable idle reads, so two idle reads are enough here; anything else falls back to the full wait. */
static int idle_fast(void) {
 uint32_t a=rd(0x400),b=rd(0x400);
 if(idle_bits(a)&&idle_bits(b)) {stats.last_status=b; return 1;}
 return idle();
}
/* .592 (VX800 board, VXGL v5.4 on .591: after a swap the copy that loads the OpenGL front buffer from the window was taken as finished
 * at the first idle status read, the CPU then cleared the buffer, and the copy ran over the clear: the buffer read back as the
 * window's picture, the ICD's steps being those of a right try). The same
 * run under Microsoft's renderer, every asynchronous path off: a GDI fill after the swap read back as the old picture and as the fill
 * 30 ms later. Once XP's ring has paused, idle_bits takes "only the command regulator's bit, fetcher paused" as idle, and that is also
 * what the status shows while the regulator still holds a 2D command it has not handed to the engine. In that state (and right after an
 * asynchronous command was completed) a command is waited for from its start: up to 256 reads for the 2D engine's own busy bit (2),
 * then the full 32 stable idle reads. complete_pending the same. */
static uint32_t started_waits,started_unseen;
static int idle_started(void) {
 uint32_t i,s; started_waits++;
 for(i=0;i<256UL;i++) {s=rd(0x400); if(s==0xFFFFFFFFUL) return 0; if(s&2UL) break;}
 if(i>=256UL) started_unseen++;
 return idle_n(32);
}
/* .593 (VX800 board, VXGL v5.5 on .592: 1200 copies timed after swaps: the 2D engine's busy bit shows at the second status read,
 * never at the first; once the first read was 00000000 with the command just written, and the ring-paused rule was not in use at all.
 * .592's wait cured the tries that followed an asynchronous command; the ones left (a GDI fill under Microsoft's renderer, the load
 * copy after glFinish) are commands with nothing pending, whose wait was one idle read: ACCEL_OP_WAIT, v10.133). No command is taken
 * as finished on its first idle read: up to 16 reads for the busy bit, then 2 idle reads in a row (8 when it never showed); .594: 4 reads, then 2. */
static uint32_t start_waits,start_unseen,start_late;
static int idle_from_start(void) {
 uint32_t i,s; start_waits++;
 /* .594 (VX855 and VX800 boards on .593: 119,000 waits, the busy bit at the second read or not at all, never later; 42% of the
  * VX855's commands (short lines, small fills) had finished before it could show and paid 24 reads, about 6 us each: engine lines
  * up to 48 pixels 17.4 us against 12 to 15 for longer ones). Four reads for the busy bit, then two idle reads in a row. */
 for(i=0;i<4UL;i++) {s=rd(0x400); if(s==0xFFFFFFFFUL) return 0; if(s&2UL) break;}
 if(i>=4UL) start_unseen++; else if(i>1UL) start_late++;
 return idle_n(2);
}
static int e_begin(void) {
 unsigned long f=VX_irq_save(); unsigned char old=inp(0x3C4),a,b;
 saved_unlock=sq_get(0x10); sq_put(0x10,(unsigned char)(saved_unlock|1));
 a=sq_get(0x1A); b=sq_get(0x1E);
 if((a&0x42)||(sq_get(0x4E)&0x30)) goto fail;
 if(!(a&8)) sq_put(0x1A,(unsigned char)(a|8));
 if(!(b&1)) sq_put(0x1E,(unsigned char)(b|1));
 if(!(sq_get(0x1A)&8)||!(sq_get(0x1E)&1)) {sq_put(0x1A,a); sq_put(0x1E,b); goto fail;} /* v10.330 (review #149): the enables that did not read back are put back as found */
 outp(0x3C4,old); VX_irq_restore(f); return 1;
fail:
 sq_put(0x10,saved_unlock); outp(0x3C4,old); VX_irq_restore(f); return 0;
}
static void e_end(void) {
 unsigned long f=VX_irq_save(); unsigned char old=inp(0x3C4);
 sq_put(0x10,saved_unlock); outp(0x3C4,old); VX_irq_restore(f);
}
/* v10.144: hand the kept context back */
static void k_release(void) {
 int i;
 if(!keep.open) return;
 if(m2d_state!=3&&keep.gen==vx_status.set_successes) for(i=0;i<19;i++) wr(saved_regs[i],keep.saved[i]);
 e_end(); keep.open=0; keep_releases++;
}
/* v10.144: 1 when the kept context is open for this mode generation (nothing to do); 0 otherwise (a stale one is dropped first) */
static int k_held(void) {
 if(!vx2d_keep) {if(keep.open) k_release(); return 0;}
 if(keep.open&&keep.gen!=vx_status.set_successes) k_release();
 return keep.open;
}
/* v10.144: after e_begin and the idle check of a new context: save the registers into it */
static void k_take(void) {int i; for(i=0;i<19;i++) keep.saved[i]=rd(saved_regs[i]); keep.open=1; keep.gen=vx_status.set_successes; keep_opens++;}
static int delay5(void) {
 uint32_t n,start=Get_System_Time();
 for(n=0;n<2000000UL;n++) {if((uint32_t)(Get_System_Time()-start)>=5) return 1; (void)rd(0x400);}
 return 0;
}
volatile uint32_t vx2d_reset_epoch; /* v10.252: counts engine resets (2D here, 3D in vx_native3d_vxd.c): the cursor is programmed again after one */
static int reset(void) {
 unsigned long f; unsigned char old,v; int ok;
 hc_pos_ok=0;vx2d_reset_epoch++;
 if(rd(0x400)==0xFFFFFFFFUL) return 0;
 f=VX_irq_save(); old=inp(0x3C4); v=sq_get(0x4E);
 sq_put(0x4E,(unsigned char)((v&~0x0C)|4)); outp(0x3C4,old); VX_irq_restore(f);
 ok=delay5(); /* v10.329 (review): a failed wait no longer returns with the reset bit set; the bit is cleared either way */
 f=VX_irq_save(); old=inp(0x3C4); sq_put(0x4E,(unsigned char)(v&~0x0C)); outp(0x3C4,old); VX_irq_restore(f);
 if(!delay5()) ok=0;
 if(!ok) {vx2d_reset_epoch++; return 0;} /* v10.327 (review): the failed waits count the reset again too */
 vx2d_reset_epoch++; /* v10.326 (review): again after the pulses: a cursor programmed while the reset bit was set counts as not programmed */
 stats.resets++; return idle();
}
static void note_lost(void);
static int e_first(void) {
 uint32_t base=vx_status.bar1&0xFFFFFFF0UL,n,start; unsigned long f; unsigned char old,v;
 if((vx_status.bar1&15)||base<0x100000UL||base>0xFFC00000UL) return 0;
 if(!m2d_linear) m2d_linear=_MapPhysToLinear(base,0x400000UL,0);
 if(!m2d_linear||m2d_linear==0xFFFFFFFFUL) {m2d_linear=0; return 0;}
 k_release(); /* v10.144: a kept context of the old generation goes first (its registers are not written back) */
 if(!e_begin()) return 0;
 start=Get_System_Time();
 for(n=0;n<2000000UL;n++) {
  f=VX_irq_save(); old=inp(0x3C4); v=sq_get(0x3C); outp(0x3C4,old); VX_irq_restore(f);
  if(v&0x10) break;
  if((uint32_t)(Get_System_Time()-start)>=50) {e_end(); return 0;}
 }
 if(n==2000000UL) {e_end(); return 0;}
 n=reset(); if(!n) note_lost(); e_end(); return (int)n;
}
/* v10.58z: g=0 for DirectDraw requests. GDI's wEnabled flag read 0 whenever a DirectDraw request arrived
 * (diag code 0x204), so every DirectDraw operation was skipped. DirectDraw has its own lost-surface protocol. */
static int desktop_x(int g) {
 return (!g||(gdi_flags && gdi_enabled && *gdi_enabled)) && hda && hda->vram_pm32 && vx_status.stage==6 && !vx_status.failure && !vx_status.vga_mode &&
  hda->surface==0 && hda->system_surface==0 &&
  (uint32_t)hda->vram_pm32==vx_status.fb_linear && hda->pitch==vx_status.video_pitch &&
  hda->width==vx_status.width && hda->height==vx_status.height && hda->bpp==vx_status.bpp &&
  (hda->bpp==8||hda->bpp==16||hda->bpp==32); /* v10.76: 8bpp too (VX2D8: depth code 000 = 1 byte per pixel, fills, ROPs, copies, keys and clipping all matched the model) */
}
/* v10.71: CPU access to the primary surface (hiding the software cursor) is needed at 8bpp too, where the engine itself is not used */
static int desktop_cpu(void) {
 return hda && hda->vram_pm32 && vx_status.stage==6 && !vx_status.failure && !vx_status.vga_mode &&
  hda->surface==0 && hda->system_surface==0 &&
  (uint32_t)hda->vram_pm32==vx_status.fb_linear && hda->pitch==vx_status.video_pitch &&
  hda->width==vx_status.width && hda->height==vx_status.height && hda->bpp==vx_status.bpp &&
  (hda->bpp==8||hda->bpp==16||hda->bpp==32);
}
/* v10.58y diagnostic: why desktop() fails, as a bit mask (0 = desktop is fine). */
static unsigned desk_why(void) {
 unsigned w=0;
 if(!gdi_flags) w|=1;
 if(!gdi_enabled) w|=2; else if(!*gdi_enabled) w|=4;
 if(!hda||!hda->vram_pm32) w|=8;
 if(vx_status.stage!=6||vx_status.failure||vx_status.vga_mode) w|=16;
 if(hda&&(hda->surface||hda->system_surface)) w|=32;
 if(hda&&(uint32_t)hda->vram_pm32!=vx_status.fb_linear) w|=64;
 if(hda&&(hda->pitch!=vx_status.video_pitch||hda->width!=vx_status.width||hda->height!=vx_status.height||hda->bpp!=vx_status.bpp||(hda->bpp!=8&&hda->bpp!=16&&hda->bpp!=32))) w|=128;
 return w;
}
/* v10.422 (diagnostic): why the last surface() refused: 1 size or pitch, 2 overlays exceed video memory, 3 past the end, 4 not exactly the
 * primary though inside its allocation */
static uint32_t sf_why;
static int surface(const vx2d_surface *s,uint32_t bpp) {
 uint32_t limit,bytes=bpp>>3;
 if(!s->width||!s->height||s->width>4096||s->height>4096||!s->pitch||
    (s->pitch&15)||(s->pitch>>3)>0x7FF||(s->offset&7)||s->width>s->pitch/bytes) {sf_why=1;return 0;} /* v10.277: the engine's pitch is in 16-byte steps (VXKEY: a pitch of 16n + 8 was used as 16n) */
 limit=hda->vram_size_virt;
 if(limit>VX_FB_LIMIT) limit=VX_FB_LIMIT;
 if(hda->overlays_size>limit) {sf_why=2;return 0;}
 limit-=hda->overlays_size;
 if(s->offset>=limit||s->height>(limit-s->offset)/s->pitch) {sf_why=3;return 0;}
 /* Descriptors intersecting the primary allocation must exactly describe it. */
 if(s->offset<hda->stride) {if(!s->offset && s->pitch==hda->pitch && s->width==hda->width && s->height==hda->height)return 1;sf_why=4;return 0;}
 return 1;
}
static int box(const vx2d_surface *s,uint32_t x,uint32_t y,uint32_t w,uint32_t h) {
 return w&&h&&w<=4096&&h<=4096&&x<s->width&&y<s->height&&w<=s->width-x&&h<=s->height-y;
}
/* v10.84 (VX2ROP, hardware): the engine does every ROP3 exactly. ROP bit (P<<2|S<<1|D) is the result for those operands. */
static int rop_uses_s(uint32_t rop) {int i; for(i=0;i<8;i++) if(((rop>>i)&1)!=((rop>>(i^2))&1)) return 1; return 0;}
static int rop_uses_p(uint32_t rop) {int i; for(i=0;i<8;i++) if(((rop>>i)&1)!=((rop>>(i^4))&1)) return 1; return 0;}
static int validate_line(const vx2d_request *r) {
 uint32_t adx,ady,n;
 /* both end points on the surface; the line is end-exclusive and nonempty; the engine's counters are 12 bits (major length) and 14 bits (error terms) */
 if(r->dx>=r->dst.width||r->dy>=r->dst.height||r->sx>=r->dst.width||r->sy>=r->dst.height) return 0;
 if(r->flags&~(uint32_t)VX2D_CLIP) return 0; /* .530: styled lines (VX2D_DASH, ACCEL_DASH) removed: the DIB engine draws them */
 if(r->rop!=0xF0||r->height!=1) return 0;
 adx=r->sx>r->dx?r->sx-r->dx:r->dx-r->sx; ady=r->sy>r->dy?r->sy-r->dy:r->dy-r->sy;
 n=adx>ady?adx:ady;
 if(n<1||n>4000||r->width!=n) return 0;
 return 1;
}
/* v10.422 (diagnostic): which check refused the last request (validate's why, the 2D stats' last_result bits 8-15 with 0x300 in 8-19:
 * 0x3NN): 1 header, 2 bpp not the desktop's, 3 flags, 0x10+n destination surface (n: sf_why), 5 destination box, 6..: see VF() below,
 * 0x20+n source surface, 0x1F source box */
static uint32_t vwhy;
#define VF(n) do{vwhy=(n);return 0;}while(0)
static int validate(const vx2d_request *r) {
 if(r->cb!=sizeof(*r)||r->abi!=VX2D_ABI)VF(1);
 if(r->bpp!=hda->bpp)VF(2);
 if(r->flags&~511UL)VF(3); /* v10.453: + VX2D_RT */
 if(!surface(&r->dst,r->bpp))VF(0x10|sf_why);
 if(r->op!=VX2D_LINE&&!box(&r->dst,r->dx,r->dy,r->width,r->height))VF(5);
 if(r->flags&VX2D_DASH) VF(6);
 if(r->flags&VX2D_CLIP) {
  if(r->left>=r->right||r->top>=r->bottom||r->right>r->dst.width||r->bottom>r->dst.height) VF(7);
 }
 /* v10.76: at 8bpp only fills and copies were measured (VX2D8); mono stays off. v10.77: the colour pattern fill from video memory was measured at 8bpp too (VX2COL). */
 if(r->bpp==8&&r->op!=VX2D_FILL&&r->op!=VX2D_COPY&&r->op!=VX2D_COLOR&&r->op!=VX2D_LINE) VF(8);
 if(r->op==VX2D_LINE) {
  if(r->flags&VX2D_CLIP) {if(r->left>=r->right||r->top>=r->bottom||r->right>r->dst.width||r->bottom>r->dst.height) VF(9);}
  return validate_line(r);
 }
 if(r->op==VX2D_COPY) {
  if(r->rop>255||r->flags&VX2D_TRANSPARENT)VF(0x1E);
  if(!surface(&r->src,r->bpp))VF(0x20|sf_why);
  if(!box(&r->src,r->sx,r->sy,r->width,r->height))VF(0x1F);
  /* v10.84: any ROP3 copy. A ROP that uses P needs the fixed colour (VX2D_FIXPAT, VX2ROP T1); one that does not uses the plain form (T2). Keys stay with SRCCOPY. */
  if(r->rop!=0xCC&&(r->flags&(VX2D_SRC_KEY|VX2D_DST_KEY))) VF(10);
  if(rop_uses_p(r->rop)&&!(r->flags&VX2D_FIXPAT)) VF(11);
  if((r->flags&VX2D_FIXPAT)&&(r->flags&(VX2D_SRC_KEY|VX2D_DST_KEY))) VF(12);
  if((r->flags&(VX2D_SRC_KEY|VX2D_DST_KEY))==(VX2D_SRC_KEY|VX2D_DST_KEY)) VF(13);
  /* Aliased surfaces require the same base/pitch for proved direction rules. */
  if(r->src.offset!=r->dst.offset||r->src.pitch!=r->dst.pitch) {
   if(r->src.offset<r->dst.offset+r->dst.pitch*r->dst.height &&
      r->dst.offset<r->src.offset+r->src.pitch*r->src.height) VF(0x30);
  }
 } else {
  if(r->op<VX2D_FILL||r->op>VX2D_COLOR||r->flags&(VX2D_SRC_KEY|VX2D_DST_KEY|VX2D_FIXPAT)) VF(14);
  /* v10.84: fills and pattern fills take every ROP that does not use S (VX2ROP T1, T3, T4: all of them matched the software ROP3) */
  if(r->rop>255||rop_uses_s(r->rop)) VF(15);
  if(r->op!=VX2D_FILL&&!rop_uses_p(r->rop)) VF(16); /* a pattern fill that ignores the pattern is a plain fill */
  if((r->flags&VX2D_TRANSPARENT)&&r->op!=VX2D_MONO) VF(17);
  /* v10.77: the colour pattern fill reads its block from video memory (no pattern RAM): pattern ROPs only, and only when the block area was reserved. */
  if(r->op==VX2D_COLOR&&!vx2d_brush_off) VF(18);
 }
 return 1;
}
static uint32_t flags(void) {return (vx2d_copy_all?1:0)|(vx2d_enable?0:2)|(vx2d_force?4:0);}
/* v10.75: a lost engine (state 3) used to stay lost until reboot, and an engine that was switched off after a timeout (state 2) was never tried again.
 * Both are given one more chance per successful mode set: the engine is reset, and when that works the fences go away. */
/* v10.279 (review): fence and unfence map the GDI words themselves; they also run from VX2D_sync, VX2D_busy and VX2D_lost (mode sets,
 * DOS boxes, flips, the 3D side), which come without a 2D entry's mapping. */
static void fence(void) {
 gdi_map();
 if(gdi_flags) *gdi_flags|=0x0010;
 if(gdi_enabled) *gdi_enabled=0;
 if(!vx_native_display_hold) {vx_native_display_hold=1; hold_by_us=1;}
 lost_fenced=1;
}
static void note_off(void) {m2d_state=2; fail_gen=vx_status.set_successes;}
static void note_lost(void) {m2d_state=3; lost_gen=vx_status.set_successes;}
static void unfence(void) {
 if(lost_fenced) {
  gdi_map();
  if(gdi_flags) *gdi_flags&=(uint16_t)~0x0010;
  if(gdi_enabled) *gdi_enabled=1;
  if(hold_by_us) {vx_native_display_hold=0; hold_by_us=0;}
  lost_fenced=0;
 }
}
/* returns 1 when the engine is usable again (state 0: the next operation initialises it), 0 when it is still lost */
static uint32_t access_lost,recover_failed; /* v10.229 (VX2D_loss_info) */
static int lost_recover(int force) {
 uint32_t g=vx_status.set_successes; int ok=0;
 /* v10.88 can retry a 2D fault on bind/mode change. A partially retired
  * native 3D command remains fenced until reboot, including forced bind. */
 if(native_fault)return 0;
 if(m2d_state!=3) return 1;
 if(!m2d_linear||(!force&&g==lost_gen)) return 0;
 lost_gen=g; /* one attempt per mode set */
 k_release(); /* v10.144 */
 if(e_begin()) {ok=reset(); e_end();}
 if(!ok) {recover_failed++; return 0;}
 m2d_state=0; recoveries++; unfence();
 return 1;
}
/* v10.77: write the 8x8 colour block into the reserved video memory and drain the write (write, then read the last dword back) */
static void put_block(const vx2d_request *q) {
 volatile uint8_t *p=(volatile uint8_t *)hda->vram_pm32+vx2d_brush_off; int i;
 if(q->bpp==32) for(i=0;i<64;i++) ((volatile uint32_t *)p)[i]=q->pattern[i];
 else if(q->bpp==16) for(i=0;i<64;i++) ((volatile uint16_t *)p)[i]=(uint16_t)q->pattern[i];
 else for(i=0;i<64;i++) p[i]=(uint8_t)q->pattern[i];
 (void)((volatile uint32_t *)p)[(q->bpp*2)-1]; /* 64 pixels * bpp/8 bytes / 4 = 2*bpp dwords */
}
/* v10.92: the line registers (everything but the colours of the 0 bits). Returns the command without the pattern bits. v10.90: command F0102005 plus X dec 8000, Y dec 4000, Y-major 200000; registers 18 = (2m << 16) | (2(m-n) & 3FFF),
 * 2C = e0 & 3FFF, 0C = n, 10 = start; the engine runs e >= 0 -> minor step, e += 2(m-n), else e += 2m, and n pixels are drawn (the end point is not). DIBENG's line takes the minor step on a tie when Y decreases and does not
 * when Y increases (that is e0 = 2m-n, and 2m-n-1), whatever the major axis and the X direction. Register 24 is the pattern (all ones = solid); register 28 is written for a styled line only. */
static uint32_t line_prog(const vx2d_request *r,uint32_t mask,uint32_t col) {
 int ddx=(int)r->sx-(int)r->dx,ddy=(int)r->sy-(int)r->dy,adx=ddx<0?-ddx:ddx,ady=ddy<0?-ddy:ddy,ym=ady>adx,n=ym?ady:adx,m=ym?adx:ady;
 uint32_t cmd=0xF0002005UL|0x100000UL|(ddx<0?0x8000UL:0)|(ddy<0?0x4000UL:0)|(ym?0x200000UL:0);
 wr(0x24,0xFFFFFFFFUL);
 wr(0x4C,col&mask);
 wr(0x18,((uint32_t)(2*m)<<16)|((uint32_t)(2*(m-n))&0x3FFFUL));
 wr(0x2C,(uint32_t)(2*m-n-(ddy>0?1:0))&0x3FFFUL);
 if(r->flags&VX2D_CLIP) {cmd|=0x1000; wr(0x40,(r->top<<16)|r->left); wr(0x44,((r->bottom-1)<<16)|(r->right-1));}
 wr(12,r->width&0xFFFUL); wr(16,(r->dy<<16)|r->dx);
 return cmd;
}
/* v10.95: finish an asynchronous command: wait for the engine, put the 19 registers back, release the sequencer lock and the cursor exclusions that the submit kept. Called first thing by everything that touches the
 * engine, the frame buffer, the cursor or the display address (submit, access, control, flip, lost/busy). */
static void complete_pending(void) {
 int ok,i;
 if(!pend.active||completing) return;
 completing=1; async_waits++;
 ok=vx3dn_xr_cr_pause?idle_started():idle(); /* .592 */
 if(!ok) {stats.timeouts++; if(reset()) note_off(); else note_lost();}
 if(m2d_state!=3&&!pend.kmode) for(i=0;i<19;i++) wr(saved_regs[i],pend.saved[i]); /* v10.144: a kept context stays open */
 if(m2d_state==3) fence();
 if(pend.opened) e_end();
 if(pend.kmode&&m2d_state>=2) k_release();
 while(pend.excluded>0) {FBHDA_access_end(0); pend.excluded--;} /* v10.139 (review F05): released even when lost; cursor_show draws nothing while the engine is lost */
 pend.excluded=0; pend.opened=0; pend.kmode=0; pend.active=0; completing=0;
 gdi_map(); if(gdi_enabled&&(*gdi_enabled&0x8000U)) *gdi_enabled&=0x7FFFU; /* v10.425: the display driver's "a copy is running" bit */
}
/* v10.425 (board, VXPRES 1.0: a 40x40 GDI fill drawn right after an asynchronous present lost its lower rows to the copy, which was still
 * running: DIBENG writes the screen without a VxD call): while an asynchronous command runs, bit 15 of the display driver's wEnabled
 * word (bound by VX2D_bind; the driver only tests it for zero) is set, and the driver's DIBENG access hook (BeginAccess, enable.c) and
 * its DIBENG forwarders (dibcall.c) call OP_VX_COMPLETE first when they see it. Set only on a word that is not zero (a fenced driver
 * stays fenced). */
void VX2D_complete(void) {vx_lock(); gdi_map(); complete_pending(); vx_unlock();}
int VX3DN_busy(void);
int VX3DN_poll(void);
/* v10.419: the 2D side only (a pending asynchronous blit finished, a kept context handed back), the 3D ring left running: a flip that goes
 * into the ring (vx855_flip.c) runs after the ring's draws by itself; VX2D_sync's retire waited for the frame and the blank of the flip
 * before (board, GTA3 v10.418: 2542 retires, 17.4 s in 96 s, Flip 9.0 ms a frame) */
void VX2D_sync_2d(void) {vx_lock(); complete_pending(); k_release(); vx_unlock();}
void VX2D_sync(void) {vx_lock(); vx2d_present_arm=0; /* v10.424 */ vx3dn_rt_why=5;(void)VX3DN_busy(); /* v10.231: retires a 3D draw in flight */ complete_pending(); k_release(); vx_unlock();} /* v10.144: also hands a kept context back (mode sets, DOS boxes, flips, exit) */
int VX2D_submit(const vx2d_request *request) {
 vx2d_request r; uint32_t saved[19],cmd,i,dx,dy,sx,sy,mask;
 int decx=0,decy=0,overlap,ok,rc=VX2D_SKIP,opened=0,excluded=0,asyncq=0,kmode=0,after_pend=0;
 unsigned long f,t[9]; unsigned why=0; int timed=0,prof=vx2d_prof?1:0;
 vx_lock(); gdi_map(); /* v10.273 */
 VX3DN_ev(13,request?(uint32_t)request->op|((request->flags&VX2D_DDRAW)?0x100UL:0):0xFF); /* v10.394 */
 f=VX_irq_save();
 if(running) {VX_irq_restore(f); vx_unlock(); return VX2D_SKIP;}
 running=1; VX_irq_restore(f);
 after_pend=pend.active?1:0; /* .592 */
 complete_pending();
 if(!request||!(request->flags&VX2D_DDRAW)) {vx2d_async_gate=0;vx2d_present_arm=0;} /* a GDI request: whatever is going on, the exclusive DirectDraw session is over (v10.424: and an armed present) */
 if(prof) t[0]=VX_tsc();
 if(m2d_state==3&&!lost_recover(0)) {rc=VX2D_LOST; goto done;}
 if(m2d_state==2&&fail_gen!=vx_status.set_successes) {m2d_state=0; retries++;}
 if(!request) {why=0x100; goto skip;}
 if(!desktop_x(!(request->flags&VX2D_DDRAW))) {why=0x200|desk_why(); if(!(request->flags&VX2D_DDRAW)&&(why&4)) gdi_off_seen++; goto skip;}
 memcpy(&r,request,sizeof(r));
 if(!validate(&r)) {stats.rejected++; why=0x300|(vwhy&255); goto skip;} /* v10.422: + which check */
 if((r.bpp==16&&!vx2d_f16)||(r.bpp==8&&!vx2d_f8)||(r.op==VX2D_MONO&&!vx2d_fpat)||(r.op==VX2D_COLOR&&!(vx2d_fpat&&vx2d_fcol))||(r.op==VX2D_LINE&&!vx2d_fline)||((r.flags&VX2D_CLIP)&&!vx2d_fclip)||((r.flags&VX2D_DDRAW)&&!vx2d_fddraw)) {why=0x400; goto skip;}
 if((r.flags&VX2D_RT)&&r.op==VX2D_COPY&&vx2d_enable&&m2d_state!=2){ /* v10.453: a render target into a texture: into XP's ring after the draws when it can go there; v10.479 (review): not with ACCEL_2D 0 or the 2D engine failed (the usual path declines it, the HAL copies on the CPU) */
  extern int VX3DN_ring_copy(const vx2d_request *);int q=VX3DN_ring_copy(&r);
  if(q>0){rc=VX2D_DONE;vx2d_ops++;vx_status.draws++;stats.copies++;stats.ddraw++;goto done;}
  if(q<0){rc=VX2D_LOST;goto done;}}
 {int pres=vx2d_async&&vx2d_async_present&&vx2d_present_arm&&!vx2d_async_gate&&r.op==VX2D_COPY; /* v10.424 */
 asyncq=((r.flags&VX2D_ASYNC)&&(r.flags&VX2D_DDRAW)&&r.op!=VX2D_LINE&&(vx2d_own3d||(vx2d_async&&vx2d_async_gate)||pres))?1:0; /* v10.95; v10.282: or the 3D side's clear (vx2d_own3d) */
 if(asyncq&&pres)present_async_ops++;}
 /* v10.58zc: size thresholds now apply to DirectDraw too. The HAL DLL finishes a declined blit on the CPU (58zb), and the
  * 58zb benchmark showed a 128x128 engine fill at 0.28x of the CPU. ACCEL_FILL_MIN / ACCEL_COPY_MIN = 0 forces everything native. */
 {
  unsigned long area=(unsigned long)r.width*r.height;
  int dread=vx2d_dread&&r.op!=VX2D_LINE&&((r.rop^(r.rop>>1))&0x55)!=0; /* v10.133: the ROP reads the screen */
  int tall=vx2d_fill_tall&&r.op!=VX2D_COPY&&r.op!=VX2D_LINE&&r.height>=vx2d_fill_tall; /* v10.142 */
  if(!vx2d_force&&!dread&&!tall&&area<(r.op==VX2D_COPY?vx2d_copy_min:r.op==VX2D_LINE?vx2d_line_min:vx2d_fill_min)) {why=0x500; goto skip;}
 }
 {extern uint32_t vx3dn_rb_stat[8];int b;unsigned long t0=VX_tsc(); /* v10.453: the census: a render-target copy's wait for the 3D work before it */
 b=!vx2d_enable||m2d_state==2||(vx3dn_rt_why=(r.flags&VX2D_DDRAW)?1:2,VX3D_busy())||vx_native_display_hold;if(r.flags&VX2D_RT){vx3dn_rb_stat[4]+=(uint32_t)((VX_tsc()-t0+128)>>8);if(b)vx3dn_rb_stat[3]++;else vx3dn_rb_stat[2]++;}
 if(b) {why=0x600|(vx2d_enable?0:1)|(m2d_state==2?2:0)|((vx3dn_rt_why=(r.flags&VX2D_DDRAW)?1:2,VX3D_busy())?4:0)|(vx_native_display_hold?8:0); goto skip;}}
 if(r.op==VX2D_COPY&&r.src.offset==r.dst.offset) {
  overlap=r.sx<r.dx+r.width&&r.dx<r.sx+r.width&&r.sy<r.dy+r.height&&r.dy<r.sy+r.height;
  if(overlap) {
   if(r.dx>=r.sx&&r.dy>=r.sy) {decx=1; decy=1;}
   else if(vx2d_copy_all) {decx=r.dx>r.sx; decy=r.dy>r.sy;}
   else {why=0x700; goto skip;}
  }
 }
 if(!m2d_state||generation!=vx_status.set_successes) {
  if(!e_first()) {if(m2d_state==3) {lost_gen=vx_status.set_successes; rc=VX2D_LOST; goto done;} note_off(); why=0x800; goto skip;}
  m2d_state=1; generation=vx_status.set_successes;
 }
 if(prof) t[1]=VX_tsc();
 kmode=k_held(); /* v10.144: the kept context is already open */
 if(!kmode) {if(!e_begin()) {why=0x900; goto skip;} opened=1;}
 if(prof) t[2]=VX_tsc();
 if(!idle_fast()) {stats.timeouts++; if(reset()) note_off(); else note_lost(); rc=m2d_state==3?VX2D_LOST:VX2D_SKIP; goto done;}
 if(prof) t[3]=VX_tsc();
 if(kmode) ;
 else if(vx2d_keep) {k_take(); opened=0; kmode=1;} /* v10.144: this operation opens the kept context */
 else for(i=0;i<19;i++) saved[i]=rd(saved_regs[i]);
 /* Hide the custom software cursor on reads as well as writes of primary. */
 /* v10.58c: hide the software cursor only when it overlaps a touched rectangle.
  * access_begin hid and re-read it from video memory on every call (~0.9 ms). */
 if(!r.dst.offset) {
  if(r.op==VX2D_LINE) {
   uint32_t lx=r.dx<r.sx?r.dx:r.sx,ly=r.dy<r.sy?r.dy:r.sy,hx=r.dx<r.sx?r.sx:r.dx,hy=r.dy<r.sy?r.sy:r.dy;
   FBHDA_access_rect(lx,ly,hx+1,hy+1);
  } else FBHDA_access_rect(r.dx,r.dy,r.dx+r.width,r.dy+r.height);
  excluded++;
 }
 if(r.op==VX2D_COPY&&!r.src.offset) {FBHDA_access_rect(r.sx,r.sy,r.sx+r.width,r.sy+r.height); excluded++;}
 if(prof) t[4]=VX_tsc();
 flush(); mask=r.bpp==8?0xFFUL:r.bpp==16?0xFFFFUL:0xFFFFFFFFUL;
 wr(4,r.bpp==8?0x000:r.bpp==16?0x100:0x300); /* depth code: 000 = 8bpp (VX2D8), 100 = 16bpp, 300 = 32bpp */
 wr(8,((r.dst.pitch>>3)<<16)|(r.op==VX2D_COPY?(r.src.pitch>>3):0));
 wr(0x14,r.dst.offset>>3); wr(0x1C,r.op==VX2D_COPY?r.src.offset>>3:0);
 wr(0x30,0); wr(0x48,0); wr(0x58,r.fg&mask); wr(0x5C,r.bg&mask); wr(0x20,0);
 cmd=(r.rop<<24)|1; dx=r.dx; dy=r.dy; sx=r.sx; sy=r.sy;
 if(r.op==VX2D_COPY) {
  cmd|=(decx?0x8000UL:0)|(decy?0x4000UL:0)|((r.flags&VX2D_FIXPAT)?0x2000UL:0);
  if(decx) {dx+=r.width-1; sx+=r.width-1;}
  if(decy) {dy+=r.height-1; sy+=r.height-1;}
  wr(0x18,(sy<<16)|sx);
  if(r.flags&VX2D_FIXPAT) {wr(0x4C,r.fg&mask); wr(0x50,r.bg&mask);} /* VX2ROP T1 sets 4C and 50 beside 58 and 5C */
  if(r.flags&VX2D_SRC_KEY) {wr(0x50,r.key&mask); wr(0x48,0x4000);}
  if(r.flags&VX2D_DST_KEY) {wr(0x4C,r.key&mask); wr(0x48,0x8000);}
 } else if(r.op==VX2D_LINE) {
  /* v10.90 (VX2LINE v1.1, 32 and 16bpp, all eight octants, 170 to 1360 lines each): command F0102005 plus X dec 8000, Y dec 4000, Y-major 200000; registers 18 = (2m << 16) | (2(m-n) & 3FFF),
   * 2C = e0 & 3FFF, 0C = n, 10 = start; the engine runs e >= 0 -> minor step, e += 2(m-n), else e += 2m, and n pixels are drawn (the end point is not). DIBENG's line takes the minor
   * step on a tie when Y decreases and does not when Y increases (that is e0 = 2m-n, and 2m-n-1), whatever the major axis and the X direction. */
  cmd=line_prog(&r,mask,r.fg&mask);
 } else if(r.op==VX2D_FILL) {cmd|=0x2000; wr(0x4C,r.fg&mask);}
 else if(r.op==VX2D_MONO) {
  cmd|=0xA00; if(r.flags&VX2D_TRANSPARENT) cmd|=0x10000;
  /* v10.60, measured on the chip with VX2PAT: byte k of the 64 pattern bits (register 24 low byte first) is the pattern row used for
   * the k-th row of the rectangle, bit 7 is the leftmost pixel, and X is anchored to the screen while Y starts again at byte 0 for every
   * command. The request carries the GDI layout (row y in byte 3-(y&3) of mono[y/4], bit x = pixel x, rows anchored to the screen):
   * rotate the rows by the rectangle's top line and mirror the bits so the result is anchored to the screen on both axes. */
  {
   uint32_t m[2],rb,rv,k; m[0]=0; m[1]=0;
   for(k=0;k<8;k++) {
    rb=(r.mono[((dy+k)&7)>>2]>>((3-((dy+k)&3))*8))&255UL; rv=0;
    rv=vx_rev8(rb); /* v10.309 (review): the byte's bits mirrored in three swaps */
    m[k>>2]|=rv<<((k&3)*8);
   }
   wr(0x24,m[0]); wr(0x28,m[1]);
  }
 } else {
  /* v10.77, measured with VX2COL at 32, 16 and 8 bits per pixel: command F0000001 (ROP in the top byte) with register 20 = block address >> 3 fills from an 8x8 block of
   * pixels in video memory (row-major, 64 x bytes per pixel, 16-byte aligned), always starting at the rectangle's top-left corner; the X/Y offset bits (28:26, 31:29) add
   * to the block column and row there. The DIB brush is already laid out for the screen (pixel (x,y) = brush[(y&7)*8+(x&7)]), so offsets (dx&7, dy&7) reproduce that.
   * At 8bpp the same offsets also avoid the engine's row-pair swap that appears when the X start and X offset differ (x0&7 != xo). */
  put_block(&r);
  wr(0x20,(uint32_t)(((vx2d_brush_off>>3)&0x03FFFFFFUL)|((r.dy&7UL)<<29)|((r.dx&7UL)<<26)));
 }
 if(r.op!=VX2D_LINE) {
  if(r.flags&VX2D_CLIP) {
   cmd|=0x1000; wr(0x40,(r.top<<16)|r.left); wr(0x44,((r.bottom-1)<<16)|(r.right-1));
  }
  wr(12,((r.height-1)<<16)|(r.width-1)); wr(16,(dy<<16)|dx);
 }
 if(prof) t[5]=VX_tsc();
 stats.last_command=cmd; (void)rd(0x400); wr(0,cmd); ok=asyncq?1:(after_pend||vx3dn_xr_cr_pause)?idle_started():vx2d_op_wait?idle_from_start():idle_n(32);
 if(prof) t[6]=VX_tsc();
 if(asyncq) {memcpy(pend.saved,saved,sizeof(saved)); pend.excluded=excluded; pend.opened=opened; pend.kmode=kmode; excluded=0; opened=0; async_ops++;} /* the command is running: its completion is somebody else's next call */
 if(!ok) {stats.timeouts++; if(reset()) note_off(); else note_lost(); rc=m2d_state==3?VX2D_LOST:VX2D_FAILED;}
 else {
  timed=1;
  rc=VX2D_DONE; vx2d_ops++; vx_status.draws++;
  if(r.op==VX2D_FILL) stats.fills++;
  if(r.op==VX2D_COPY) stats.copies++;
  if(r.op==VX2D_MONO) stats.mono++;
  if(r.op==VX2D_COLOR) stats.color++;
  if(r.op==VX2D_LINE) stats.fills++; /* counted with the fills (the stats ABI has no line counter) */
  if(r.flags&VX2D_CLIP) stats.clips++;
  if(r.flags&VX2D_SRC_KEY) stats.src_keys++;
  if(r.flags&VX2D_DST_KEY) stats.dst_keys++;
  if(r.flags&VX2D_DDRAW) stats.ddraw++;
 }
 /* Do not restore MMIO or redraw a CPU cursor while the device is lost. */
 if(m2d_state!=3&&!asyncq&&!kmode) {
  for(i=0;i<19;i++) wr(saved_regs[i],saved[i]);
 }
 if(asyncq) {pend.active=1; pend.t0=Get_System_Time(); if(gdi_enabled&&*gdi_enabled) *gdi_enabled|=0x8000U;} /* v10.425 */
 if(prof) t[7]=VX_tsc();
 goto done;
skip:
 vx2d_fallbacks++;
 /* v10.279 (review): a declined DirectDraw blit is finished on the CPU by the HAL (or DirectDraw), and the early declines (sizes under
  * ACCEL_COPY_MIN / ACCEL_FILL_MIN, validation, desktop state) came before VX3D_busy, the only place this call retired a 3D draw still
  * in flight: a small blit from a render target read it before the engine had finished it. The draw is retired here, as VX2D_access
  * does for a Lock (its state cache starts over: the CPU may write a texture). */
 if(request&&(request->flags&VX2D_DDRAW)) {vx3dn_rt_why=1;(void)VX3DN_busy(); if(m2d_state==3||native_fault) rc=VX2D_LOST;}
 /* v10.426 (review M12 / Defect 2): a declined GDI request is drawn by DIBENG on the CPU: a 3D draw still writing the screen (a window's
  * Direct3D or OpenGL target in the desktop) is retired first. Only the draw in flight: the state cache stays (GDI does not touch 3D state). */
 else if(request) {int VX3DN_retire_inflight(void);vx3dn_rt_why=2;(void)VX3DN_retire_inflight(); if(m2d_state==3||native_fault) rc=VX2D_LOST;} /* v10.285 (review): a retire that failed and lost the device answers LOST, not SKIP (the HAL would have finished it on the CPU) */
done:
 if(m2d_state==3) {
  /* Fence every DIBENG forwarder, including unsupported GDI operations,
   * when a failed offscreen DirectDraw operation leaves the engine unsafe. */
  fence();
 }
 if(opened) e_end();
 if(kmode&&m2d_state>=2&&!pend.active) k_release(); /* v10.144: a timeout or loss hands the kept context back */
 while(excluded>0) {FBHDA_access_end(0); excluded--;} /* v10.139 (review F05) */
 if(prof&&timed&&rc==VX2D_DONE) {
  t[8]=VX_tsc(); ph_n++;
  for(i=0;i<8;i++) ph_sum[i]+=(uint32_t)(t[i+1]-t[i])>>4;
 }
 stats.last_result=rc==VX2D_SKIP?(rc|(why<<8)):rc; running=0; vx_unlock(); return rc;
}
/* v10.99/v10.100: the conversion. Returns VX2D_DONE when the visible part was drawn, VX2D_SKIP when nothing was touched (the DRV then lets DIBENG do it), VX2D_LOST like VX2D_access.
 * Measured on the hardware: 16bpp screen: 24/32-bit truncated, 555 green sixth bit (g5<<1)|(g5&1) when stretched and 0 at 1:1; 32bpp screen: a 5-bit value v becomes (v<<3)|(7 if v odd), a 6-bit one (v<<2)|(3 if odd), 24-bit unchanged. */
/* v10.101: one source run (cnt pixels) converted into destination pixels (16bpp: uint16 values, 32bpp: uint32 values). One tight loop per source format: the first
 * version converted pixel by pixel through a general function (about 80 ns a pixel) and was slower than DIBENG at 1:1. Nothing outside the cnt source pixels is read. */
static uint32_t str_pal[256]; /* the colour table of an 8-bit source as destination pixels */
static uint32_t str_tr[32],str_tg5[32],str_tg6[64],str_tb[32]; static int str_tab;
static void str_conv(void *out,const uint8_t *p,uint32_t cnt,uint32_t bits,uint32_t flags,uint32_t dbpp,uint32_t one) {
 uint32_t i,n=cnt>>1;
 if(dbpp==16) {
  uint16_t *o=(uint16_t *)out; uint32_t *o2=(uint32_t *)out; /* two destination pixels per 32-bit store */
  if(bits==8) {
   for(i=0;i<n;i++,p+=2) o2[i]=str_pal[p[0]]|(str_pal[p[1]]<<16);
   if(cnt&1) o[cnt-1]=(uint16_t)str_pal[p[0]];
  } else if(bits==16) {
   /* 555 -> 565: red and green move up one bit; green's new low bit is 0 at 1:1 and a copy of the old low bit when stretched (measured) */
   uint32_t keep=((flags&VX2D_STR_555ONE)||one)?0UL:0x00200020UL;
   for(i=0;i<n;i++) {uint32_t w=*(const uint32_t *)(p+i*4); o2[i]=((w&0x7FE07FE0UL)<<1)|(w&0x001F001FUL)|(w&keep);}
   if(cnt&1) {uint32_t v=*(const uint16_t *)(p+(cnt-1)*2); o[cnt-1]=(uint16_t)(((v&0x7FE0UL)<<1)|(v&0x1FUL)|(v&(keep&0x20UL)));}
  } else if(bits==24) {
   for(i=0;i<n;i++,p+=6) {
    uint32_t a=((uint32_t)(p[2]>>3)<<11)|((uint32_t)(p[1]>>2)<<5)|(p[0]>>3),b=((uint32_t)(p[5]>>3)<<11)|((uint32_t)(p[4]>>2)<<5)|(p[3]>>3);
    o2[i]=a|(b<<16);
   }
   if(cnt&1) o[cnt-1]=(uint16_t)(((uint32_t)(p[2]>>3)<<11)|((uint32_t)(p[1]>>2)<<5)|(p[0]>>3));
  } else {
   for(i=0;i<n;i++,p+=8) {
    uint32_t v=*(const uint32_t *)p,w=*(const uint32_t *)(p+4);
    o2[i]=(((v>>8)&0xF800UL)|((v>>5)&0x07E0UL)|((v>>3)&0x1FUL))|((((w>>8)&0xF800UL)|((w>>5)&0x07E0UL)|((w>>3)&0x1FUL))<<16);
   }
   if(cnt&1) {uint32_t v=*(const uint32_t *)p; o[cnt-1]=(uint16_t)(((v>>8)&0xF800UL)|((v>>5)&0x07E0UL)|((v>>3)&0x1FUL));}
  }
 } else {
  uint32_t *o=(uint32_t *)out;
  if(bits==8) {
   for(i=0;i<cnt;i++) o[i]=str_pal[p[i]];
  } else if(bits==16) {
   if(!str_tab) { /* a 5-bit value v becomes (v<<3)|(7 if odd), a 6-bit one (v<<2)|(3 if odd) (measured): three table lookups a pixel */
    for(i=0;i<32;i++) {uint32_t e=(i<<3)|((i&1)?7UL:0); str_tr[i]=e<<16; str_tg5[i]=e<<8; str_tb[i]=e;}
    for(i=0;i<64;i++) str_tg6[i]=((i<<2)|((i&1)?3UL:0))<<8;
    str_tab=1;
   }
   if(flags&VX2D_STR_565) {
    for(i=0;i<cnt;i++) {uint32_t v=*(const uint16_t *)(p+i*2); o[i]=str_tr[v>>11]|str_tg6[(v>>5)&63]|str_tb[v&31];}
   } else {
    for(i=0;i<cnt;i++) {uint32_t v=*(const uint16_t *)(p+i*2); o[i]=str_tr[(v>>10)&31]|str_tg5[(v>>5)&31]|str_tb[v&31];}
   }
  } else if(bits==24) {
   for(i=0;i<cnt;i++,p+=3) o[i]=(uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16);
  } else {
   for(i=0;i<cnt;i++,p+=4) o[i]=*(const uint32_t *)p&0x00FFFFFFUL;
  }
 }
}
/* a converted line into the screen with 32-bit stores (the 16-bit narrowing loop of v10.100 was a large part of the cost) */
static void str_put(uint8_t *d,const void *src,uint32_t cnt,uint32_t dbytes) {
 uint32_t i,n;
 if(dbytes==4) {const uint32_t *s=(const uint32_t *)src; for(i=0;i<cnt;i++) ((uint32_t *)d)[i]=s[i]; return;}
 {
  const uint16_t *s=(const uint16_t *)src;
  if(((unsigned long)d&2UL)&&cnt) {*(uint16_t *)d=*s++; d+=2; cnt--;}
  n=cnt>>1;
  for(i=0;i<n;i++) ((uint32_t *)d)[i]=*(const uint32_t *)(s+i*2);
  if(cnt&1) *(uint16_t *)(d+n*4)=s[cnt-1];
 }
}
/* v10.507-v10.509: GDI options (registry, live control 0xE9000000 | bits, 0xE8000000 | x | y << 12 (pixels); read with 0xFFFFFE26; published to the display driver in
 * FBHDA rem3 bits 31:16 = 0x8000 | bits 0..1 | (ACCEL_CPULINE_MAX / 16) << 2 (6 bits) | (ACCEL_CPULINE_MAXY / 16) << 8 (6 bits); 63 = no limit):
 * bit 0 ACCEL_CPULINE (default 1): the driver draws solid one-pixel lines itself (R2_COPYPEN, R2_NOTCOPYPEN, R2_BLACK, R2_XORPEN), DIBENG's rule, straight into the screen;
 *   a pen-copy segment goes to the engine instead from ACCEL_CPULINE_MAX pixels when it is shallow (X-major, default 368) and from ACCEL_CPULINE_MAXY when it is steep
 *   (Y-major, default 112): VXGTUNE v1.1's sweep, the engine about 11-16 us a line, the CPU 4.4 us + 25 ns a pixel shallow (one row, merged writes), 4.7 us + 70 ns steep
 *   (a row a pixel); 0 = every length on the engine, 1008 or more = none;
 * bit 1 ACCEL_GDIFAST (default 1): the driver's DIBENG access hooks (BeginAccess/EndAccess) and its lines skip the two VxD calls of the cursor exclusion while the
 *   cursor is the hardware one (vxd_vesa.c rem4: no software cursor in the screen; a show of the software cursor waits for the driver's next access).
 * (v10.508's ACCEL_CPUSPAN, the driver's own solid spans, is gone: VXGTUNE v1.1, Ellipse 0.75x, Polygon 0.89x, RoundRect 0.86x against DIBENG.) */
volatile unsigned long vx2d_gdi=0x03UL,vx2d_gdi_cfg=0x03UL,vx2d_linemax=368UL,vx2d_linemax_cfg=368UL,vx2d_linemaxy=112UL,vx2d_linemaxy_cfg=112UL;
static uint32_t str_rows_direct;
/* v10.104: a compressed bitmap decoded into one index byte a pixel (W*H bytes, the bottom line first: the order of a bottom-up bitmap). Only a stream that writes every pixel exactly once is taken: no delta, every row
 * complete when its end-of-line comes, nothing after the last pixel but end markers, nothing read past the n bytes. Anything else returns 0 (the pixels a delta or a short line leaves alone are DIBENG's business). */
static int rle_decode(const uint8_t *s,uint32_t n,uint32_t bits,uint32_t W,uint32_t H) {
 uint32_t i=0,x=0,y=0,total=W*H,done=0,k,b0,b1,nb,pad;
 while(i+2<=n) {
  b0=s[i]; b1=s[i+1]; i+=2;
  if(b0) { /* an encoded run: b0 pixels of the colour b1 (RLE4: the two nibbles of b1 alternate) */
   if(done+b0>total||x+b0>W) return 0;
   if(bits==8) for(k=0;k<b0;k++) rle_buf[y*W+x+k]=(uint8_t)b1;
   else for(k=0;k<b0;k++) rle_buf[y*W+x+k]=(uint8_t)((k&1)?(b1&15):(b1>>4));
   x+=b0; done+=b0;
  } else if(b1==0) { /* end of line: the row must be complete */
   if(x!=W||y>=H) return 0;
   y++; x=0;
  } else if(b1==1) { /* end of bitmap */
   return done==total;
  } else if(b1==2) return 0; /* delta: pixels are skipped */
  else { /* an absolute run of b1 pixels, padded to a 16-bit boundary */
   nb=bits==8?b1:(b1+1)>>1; pad=(nb+1)&~1UL;
   if(i+pad>n||done+b1>total||x+b1>W) return 0;
   if(bits==8) for(k=0;k<b1;k++) rle_buf[y*W+x+k]=s[i+k];
   else for(k=0;k<b1;k++) rle_buf[y*W+x+k]=(uint8_t)((k&1)?(s[i+(k>>1)]&15):(s[i+(k>>1)]>>4));
   i+=pad; x+=b1; done+=b1;
  }
 }
 return done==total;
}
/* v10.104: cnt pixels of a 1-bit or 4-bit row starting at column col, as index bytes (the leftmost pixel is the high bit / high nibble) */
static void str_unpack(uint8_t *o,const uint8_t *row,uint32_t col,uint32_t cnt,uint32_t bits) {
 uint32_t i=0,b;
 if(bits==4) {
  if((col&1)&&i<cnt) {o[i++]=(uint8_t)(row[col>>1]&15); col++;}
  while(i+2<=cnt) {b=row[col>>1]; o[i]=(uint8_t)(b>>4); o[i+1]=(uint8_t)(b&15); i+=2; col+=2;}
  if(i<cnt) o[i]=(uint8_t)(row[col>>1]>>4);
 } else {
  while(i<cnt&&(col&7)) {o[i++]=(uint8_t)((row[col>>3]>>(7-(col&7)))&1); col++;}
  while(i+8<=cnt) {b=row[col>>3]; o[i]=(uint8_t)(b>>7); o[i+1]=(uint8_t)((b>>6)&1); o[i+2]=(uint8_t)((b>>5)&1); o[i+3]=(uint8_t)((b>>4)&1); o[i+4]=(uint8_t)((b>>3)&1); o[i+5]=(uint8_t)((b>>2)&1); o[i+6]=(uint8_t)((b>>1)&1); o[i+7]=(uint8_t)(b&1); i+=8; col+=8;}
  while(i<cnt) {o[i++]=(uint8_t)((row[col>>3]>>(7-(col&7)))&1); col++;}
 }
}
/* v10.132: a 1-bit run expanded straight to 32bpp screen pixels (a fused routine: a 16-entry nibble table of four pixels, built once per call from the two colours) */
static uint32_t str_nib[16][4];
static void str_nib_build(uint32_t c0,uint32_t c1) {uint32_t n,k; for(n=0;n<16;n++) for(k=0;k<4;k++) str_nib[n][k]=(n&(8UL>>k))?c1:c0;}
static void str_mono32(uint32_t *o,const uint8_t *row,uint32_t col,uint32_t cnt,uint32_t c0,uint32_t c1) {
 const uint32_t *a,*b; uint32_t v;
 while(cnt&&(col&7)) {*o++=((row[col>>3]>>(7-(col&7)))&1)?c1:c0; col++; cnt--;}
 while(cnt>=8) {v=row[col>>3]; a=str_nib[v>>4]; b=str_nib[v&15]; o[0]=a[0]; o[1]=a[1]; o[2]=a[2]; o[3]=a[3]; o[4]=b[0]; o[5]=b[1]; o[6]=b[2]; o[7]=b[3]; o+=8; col+=8; cnt-=8;}
 while(cnt) {*o++=((row[col>>3]>>(7-(col&7)))&1)?c1:c0; col++; cnt--;}
}
/* where the source pixels of row t (counted from the top of the picture) starting at column c0 of the rectangle are, as bytes of format *eb (8, 16, 24 or 32 bits a pixel): 1-bit and 4-bit rows are unpacked, an RLE bitmap is read from its decoded copy */
static const uint8_t *str_rowptr(const vx2d_stretch *r,uint32_t sbase,uint32_t t,uint32_t c0,uint32_t cnt,uint32_t *eb) {
 uint32_t row=r->src_topdown?t:(r->src_height-1-t); const uint8_t *rp=(const uint8_t *)(sbase+row*r->src_stride);
 if(!(r->flags&VX2D_STR_RLE)&&r->src_bits<8) {str_unpack(str_unp,rp,r->sx+c0,cnt,r->src_bits); *eb=8; return str_unp;}
 *eb=(r->flags&VX2D_STR_RLE)?8UL:r->src_bits;
 return rp+(r->sx+c0)*(*eb>>3);
}
static int str_valid(const vx2d_stretch *r) {
 uint32_t w,h,rowneed; int32_t sdx=(int32_t)r->dx,sdy=(int32_t)r->dy;
 if(r->cb!=sizeof(*r)||r->abi!=VX2D_ABI||(r->bpp!=16&&r->bpp!=32)||(r->flags&~(VX2D_STR_555ONE|VX2D_STR_565|VX2D_STR_RLE))) return 0;
 if(r->src_bits!=1&&r->src_bits!=4&&r->src_bits!=8&&r->src_bits!=16&&r->src_bits!=24&&r->src_bits!=32) return 0;
 if(r->src_bits<=8?(r->pal_n<1||r->pal_n>(1UL<<r->src_bits)):r->pal_n!=0) return 0;
 if((r->flags&VX2D_STR_565)&&r->src_bits!=16) return 0;
 if((r->flags&VX2D_STR_RLE)&&r->src_bits!=8&&r->src_bits!=4) return 0;
 if(!surface(&r->dst,r->bpp)||r->dst.offset!=0) return 0;
 if(!r->kx||!r->ky||r->kx>4096||r->ky>4096||!r->sw||!r->sh||r->sw>4096||r->sh>4096) return 0;
 w=r->sw*r->kx; h=r->sh*r->ky; if(w>4096||h>4096) return 0;
 if(sdx<-8192||sdx>8192||sdy<-8192||sdy>8192) return 0;
 if(r->left>=r->right||r->top>=r->bottom||(int32_t)r->left<sdx||(int32_t)r->top<sdy||(int32_t)r->right>sdx+(int32_t)w||(int32_t)r->bottom>sdy+(int32_t)h) return 0;
 if(r->left>8192||r->top>8192||r->right>r->dst.width||r->bottom>r->dst.height) return 0;
 if(!r->src_height||r->src_height>4096) return 0;
 if(r->sx>4096||r->sy>4096||r->sy+r->sh>r->src_height) return 0;
 if(r->flags&VX2D_STR_RLE) { /* compressed: src_bytes = the stream, src_stride = src_width = the width of the decoded bitmap */
  if(r->src_topdown||!r->src_width||r->src_width>4096||r->src_stride!=r->src_width||r->sx+r->sw>r->src_width||r->src_width*r->src_height>RLE_MAX) return 0;
  if(!r->src_bytes||r->src_bytes>0x1000000UL) return 0;
 } else {
  rowneed=(((r->sx+r->sw)*r->src_bits)+7)>>3; if(r->src_stride<rowneed||r->src_stride>65536UL||(r->src_stride&3)) return 0;
  if(r->src_bytes!=r->src_stride*r->src_height||r->src_bytes>0x1000000UL) return 0;
 }
 if(r->src_lin<0x400000UL||r->src_lin+r->src_bytes>0xC0000000UL||r->src_lin+r->src_bytes<r->src_lin) return 0;
 return 1;
}
static unsigned long str_class(const vx2d_stretch *r) {
 uint32_t mn=r->kx<r->ky?r->kx:r->ky; int one=r->kx==1&&r->ky==1;
 if((r->flags&VX2D_STR_RLE)||r->src_bits<8) return 0; /* 1-bit, 4-bit and compressed sources: str_class2 */
 if(r->src_bits==8) {
  if(r->flags&VX2D_STR_565) return 0;
  if(r->bpp==16) return 0; /* DIBENG is already as fast (2.2 ms for a 2x stretch) and rounds the colours differently (VX2STRX v2.2): left to it */
  return one?2097152UL:mn>=2?524288UL:1048576UL;
 }
 if(r->bpp==16) {
  if(r->src_bits==16) {if(r->flags&VX2D_STR_565) return 0; return one?64UL:mn>=2?1UL:16UL;}
  if(r->src_bits==24) return one?32UL:mn>=2?2UL:8UL;
  return one?32UL:mn>=2?4UL:8UL;
 }
 if(r->src_bits==16) return (r->flags&VX2D_STR_565)?(one?16384UL:mn>=2?512UL:1024UL):(one?8192UL:mn>=2?128UL:256UL);
 if(r->src_bits==24) return one?32768UL:mn>=2?2048UL:4096UL;
 return 0; /* 32-bit sources into a 32bpp screen are fast in DIBENG (VXGTUNE .507: 1:1 0.64 ms against 0.73 at best for 256x256) */
}
static unsigned long str_class2(const vx2d_stretch *r) {
 uint32_t mn=r->kx<r->ky?r->kx:r->ky,kind,shape;
 if(r->bpp!=32||(r->flags&VX2D_STR_565)) return 0;
 if(r->flags&VX2D_STR_RLE) kind=r->src_bits==8?2UL:3UL;
 else if(r->src_bits==1) kind=0; else if(r->src_bits==4) kind=1; else return 0;
 shape=(r->kx==1&&r->ky==1)?2UL:mn>=2?0UL:1UL;
 return 1UL<<(kind*3+shape);
}
static int stretch_body(const vx2d_stretch *request) {
 vx2d_stretch r; int rc=VX2D_SKIP,excluded=0,locked=0; unsigned long f; uint32_t page=0,npages=0;
 vx_lock(); gdi_map(); /* v10.273 */
 f=VX_irq_save();
 if(running) {VX_irq_restore(f); vx_unlock(); return VX2D_SKIP;}
 running=1; VX_irq_restore(f);
 complete_pending();
 vx2d_async_gate=0; vx2d_present_arm=0; /* a GDI request: an exclusive DirectDraw session is over (v10.424: and an armed present) */
 if(m2d_state==3&&!lost_recover(0)) {rc=VX2D_LOST; goto done;}
 if(!request||!(vx2d_str_mask|vx2d_str_mask2)) goto skip;
 if(!desktop_x(1)||(vx3dn_rt_why=2,VX3D_busy())||vx_native_display_hold) goto skip;
 memcpy(&r,request,sizeof(r));
 if(!str_valid(&r)) {stats.rejected++; goto skip;}
 if(r.bpp!=hda->bpp) goto skip;
 if(!((vx2d_str_mask&str_class(&r))|(vx2d_str_mask2&str_class2(&r)))) goto skip;
 if(r.src_bits<=8) { /* the colour table as destination pixels: the 24-bit rule (565 truncation into 16bpp, the 24 bits unchanged into 32bpp) */
  uint32_t k,kn=r.src_bits==1?2UL:r.src_bits==4?16UL:256UL; /* v10.142 (review O09): only the indices the source can address */
  for(k=0;k<kn;k++) {uint32_t q=r.pal[k]; str_pal[k]=r.bpp==16?(((q>>19)&31)<<11)|(((q>>10)&63)<<5)|((q>>3)&31):(q&0x00FFFFFFUL);}
 }
 {
  uint32_t fuse=(vx2d_str_fuse&&r.src_bits==1&&!(r.flags&VX2D_STR_RLE)&&r.bpp==32)?1UL:0UL,bytes=r.src_bits>>3,dbytes=r.bpp>>3,y,c,c0,c1,n,last=0xFFFFFFFFUL,rowsrc,t,tlo,thi,mlo,mhi,lo_addr,hi_end,one=(r.kx==1&&r.ky==1)?1UL:0UL,sbase=r.src_lin,eb,direct=0;
  int32_t sdx=(int32_t)r.dx,sdy=(int32_t)r.dy;
  uint8_t *fb=(uint8_t *)hda->vram_pm32;
  c0=(uint32_t)((int32_t)r.left-sdx)/r.kx; c1=(uint32_t)((int32_t)r.right-1-sdx)/r.kx;
  /* lock exactly the bytes that will be read: the first and last needed rows (counted from the bottom unless top-down) and the first and last needed columns */
  tlo=r.sy+(uint32_t)((int32_t)r.top-sdy)/r.ky; thi=r.sy+(uint32_t)((int32_t)r.bottom-1-sdy)/r.ky;
  mlo=r.src_topdown?tlo:r.src_height-1-thi; mhi=r.src_topdown?thi:r.src_height-1-tlo;
  if(r.flags&VX2D_STR_RLE) {lo_addr=r.src_lin; hi_end=r.src_lin+r.src_bytes;} /* the whole compressed stream is read */
  else {lo_addr=r.src_lin+mlo*r.src_stride+(((r.sx+c0)*r.src_bits)>>3); hi_end=r.src_lin+mhi*r.src_stride+(((r.sx+c1+1)*r.src_bits+7)>>3);}
  page=lo_addr>>12; npages=((lo_addr&0xFFFUL)+(hi_end-lo_addr)+0xFFFUL)>>12;
  if(!_LinPageLock(page,npages,0)) goto skip;
  locked=1;
  if(r.flags&VX2D_STR_RLE) { /* decode once into rle_buf; a stream that does not cover every pixel exactly once is DIBENG's */
   if(!rle_decode((const uint8_t *)r.src_lin,r.src_bytes,r.src_bits,r.src_width,r.src_height)) {rle_declined++; goto skip;}
   sbase=(uint32_t)rle_buf; rle_ops++; rle_pixels+=r.src_width*r.src_height;
  }
  if(fuse) str_nib_build(str_pal[0],str_pal[1]);
  FBHDA_access_rect(r.left,r.top,r.right,r.bottom); excluded=1;
  direct=one&&dbytes==4&&!fuse&&!(r.flags&VX2D_STR_RLE)&&r.src_bits>=8; /* v10.507: 1:1 onto the 32bpp screen converted straight into the screen (VXGTUNE: 555 256x256 2.61 -> 2.12 ms) */
  for(y=r.top;y<r.bottom;y++) {
   rowsrc=(uint32_t)((int32_t)y-sdy)/r.ky;
   if(direct) {
    uint8_t *d=fb+(unsigned long)y*hda->pitch+(unsigned long)r.left*4; uint32_t cnt=r.right-r.left;
    const uint8_t *p=(const uint8_t *)(r.src_lin+(r.src_topdown?(r.sy+rowsrc):(r.src_height-1-(r.sy+rowsrc)))*r.src_stride+(r.sx+c0)*bytes);
    str_conv(d,p,cnt,r.src_bits,r.flags,r.bpp,one);
    str_rows_direct++;
    continue;
   }
   if(r.kx==1&&r.ky==1&&dbytes==2) { /* 1:1 into the 16bpp screen: convert straight into the screen, two pixels a store (no staging line) */
    uint8_t *d=fb+(unsigned long)y*hda->pitch+(unsigned long)r.left*2; uint32_t cnt=r.right-r.left;
    const uint8_t *p=(const uint8_t *)(r.src_lin+(r.src_topdown?(r.sy+rowsrc):(r.src_height-1-(r.sy+rowsrc)))*r.src_stride+(r.sx+c0)*bytes);
    if(((unsigned long)d&2UL)&&cnt) {uint16_t t1; str_conv(&t1,p,1,r.src_bits,r.flags,r.bpp,one); *(uint16_t *)d=t1; d+=2; p+=bytes; cnt--;}
    if(cnt) str_conv(d,p,cnt,r.src_bits,r.flags,r.bpp,one);
    continue;
   }
   if(rowsrc!=last) {
    const uint8_t *p; int32_t xs0;
    last=rowsrc; t=r.sy+rowsrc;
    if(fuse) { /* v10.132: 1-bit into 32bpp without the index-byte line */
     uint32_t row=r.src_topdown?t:(r.src_height-1-t);
     str_mono32(r.kx==1?str_line:str_src,(const uint8_t *)(sbase+row*r.src_stride),r.sx+c0,c1-c0+1,str_pal[0],str_pal[1]); str_fused_rows++;
    } else {
     p=str_rowptr(&r,sbase,t,c0,c1-c0+1,&eb);
     if(r.kx==1) str_conv(str_line,p,c1-c0+1,eb,r.flags,r.bpp,one); /* 1 source pixel = 1 screen pixel: the converted run is the line */
     else str_conv(str_src,p,c1-c0+1,eb,r.flags,r.bpp,one);
    }
    if(r.kx!=1) {
     if(dbytes==4) {
      uint32_t *o=str_line;
      for(c=c0;c<=c1;c++) {
       uint32_t v=str_src[c-c0]; xs0=sdx+(int32_t)(c*r.kx); n=r.kx;
       if(xs0<(int32_t)r.left) n-=(uint32_t)((int32_t)r.left-xs0);
       if(xs0+(int32_t)r.kx>(int32_t)r.right) n-=(uint32_t)(xs0+(int32_t)r.kx-(int32_t)r.right);
       while(n--) *o++=v;
      }
     } else {
      uint16_t *o=(uint16_t *)str_line; const uint16_t *s16=(const uint16_t *)str_src;
      for(c=c0;c<=c1;c++) {
       uint16_t v=s16[c-c0]; xs0=sdx+(int32_t)(c*r.kx); n=r.kx;
       if(xs0<(int32_t)r.left) n-=(uint32_t)((int32_t)r.left-xs0);
       if(xs0+(int32_t)r.kx>(int32_t)r.right) n-=(uint32_t)(xs0+(int32_t)r.kx-(int32_t)r.right);
       while(n--) *o++=v;
      }
     }
    }
   }
   str_put(fb+(unsigned long)y*hda->pitch+(unsigned long)r.left*dbytes,str_line,r.right-r.left,dbytes);
  }
  str_pixels+=(r.right-r.left)*(r.bottom-r.top);
 }
 str_ops++; rc=VX2D_DONE;
 goto done;
skip:
 str_skips++;
 /* v10.330 (review #187): a loss latched during this call (VX3D_busy retiring a hung 3D draw) answers LOST, not SKIP: SKIP lets the DRV
  * draw the call with DIBENG on a device the loss has fenced */
 if(m2d_state==3) rc=VX2D_LOST;
done:
 if(excluded) FBHDA_access_end(0); /* v10.139 (review F05): released even when lost */
 if(locked) _LinPageUnLock(page,npages,0);
 if(m2d_state==3) fence();
 running=0; vx_unlock(); return rc;
}
/* v10.116: GDI drawing from system memory (see vx855_2d.h, VX2D_XF_*). Everything is checked before anything is drawn: a SKIP has touched nothing, so DIBENG can still draw the call. */
volatile unsigned long vx2d_host=30UL,vx2d_host_cfg=30UL,vx2d_host_min=256UL,vx2d_mono_min=4096UL,vx2d_stage_off=0;
static uint32_t xf_ops[5],xf_skips,xf_why,xf_pixels,xf_mask_p16,xf_mask_miss;
/* v10.279 (review): the DRV's copy of the live mask (vx_host_mask, a WORD in its data segment) was kept as the linear address the last
 * request carried and written there by a later control call (a test tool changing ACCEL_HOST): after the segment moved in linear memory
 * (as wEnabled did, v10.273) that wrote into whatever had taken the old place, and the DRV kept the old mask. The address is now kept as
 * selector:offset in the segment of the bound wEnabled (the same DGROUP) and worked out again at every write, as the GDI words are. An
 * address outside that segment is not kept (xf_mask_miss); the binding goes with every bind and unbind. */
static void xf_mask_bind(uint32_t lin) {
 uint32_t sel=gdi_ep16>>16; ULONG b;
 xf_mask_p16=0;
 if(!sel) {xf_mask_miss++; return;}
 b=_SelectorMapFlat((ULONG)Get_Sys_VM_Handle(),sel,0);
 if(b==0xFFFFFFFFUL||!b||lin<b||lin-b>0xFFFEUL) {xf_mask_miss++; return;}
 xf_mask_p16=(sel<<16)|(lin-b);
}
static vx2d_xfer xf_r;
static uint32_t xf_saved[19];
static uint8_t xf_row[520]; /* one row of host data: at most 4096 pixels padded to 32 */
static void xf_mask_out(void) {volatile uint16_t *m=xf_mask_p16?gdi_at(xf_mask_p16,Get_Sys_VM_Handle()):0; if(m) *m=(uint16_t)vx2d_host;}
/* open the engine for our own commands: initialised for this mode, sequencer unlocked, idle, the 19 registers saved. Nothing has fired when it fails. */
static int xf_kmode; /* v10.144: this transfer runs in the kept context */
static int xf_open(void) {
 int i;
 if(!m2d_state||generation!=vx_status.set_successes) {
  if(!e_first()) {if(m2d_state==3) {lost_gen=vx_status.set_successes; return VX2D_LOST;} note_off(); return VX2D_SKIP;}
  m2d_state=1; generation=vx_status.set_successes;
 }
 xf_kmode=k_held();
 if(!xf_kmode&&!e_begin()) return VX2D_SKIP;
 if(!idle_fast()) {stats.timeouts++; if(reset()) note_off(); else note_lost(); if(xf_kmode) k_release(); else e_end(); return m2d_state==3?VX2D_LOST:VX2D_SKIP;}
 if(xf_kmode) ;
 else if(vx2d_keep) {k_take(); xf_kmode=1;}
 else for(i=0;i<19;i++) xf_saved[i]=rd(saved_regs[i]);
 return VX2D_DONE;
}
static void xf_close(void) {int i; if(xf_kmode) {if(m2d_state>=2) k_release(); return;} if(m2d_state!=3) for(i=0;i<19;i++) wr(saved_regs[i],xf_saved[i]); e_end();}
/* a fired command did not finish */
static int xf_timeout(void) {stats.timeouts++; if(reset()) note_off(); else note_lost(); return m2d_state==3?VX2D_LOST:VX2D_FAILED;}
static uint32_t xf_depth(uint32_t bpp) {return bpp==8?0x000UL:bpp==16?0x100UL:0x300UL;}
/* nbits bits of a 1-bit row (MSB = leftmost pixel) starting at source bit sb, placed at bit db of xf_row (cleared to nbytes first); rowbytes = bytes the row may be read at */
static void xf_bits(const uint8_t *s,uint32_t sb,uint32_t db,uint32_t nbits,uint32_t nbytes,uint32_t rowbytes) {
 uint32_t i;
 for(i=0;i<nbytes;i++) xf_row[i]=0;
 while(nbits) {
  uint32_t sbyte=sb>>3,soff=sb&7,take=8-(db&7),v,bits;
  if(take>nbits) take=nbits;
  v=((uint32_t)s[sbyte]<<8)|((sbyte+1<rowbytes)?(uint32_t)s[sbyte+1]:0UL);
  bits=(v>>(16-soff-take))&((1UL<<take)-1UL);
  xf_row[db>>3]|=(uint8_t)(bits<<(8-(db&7)-take));
  db+=take; sb+=take; nbits-=take;
 }
}
/* the mono host command over box x,y,w32 x h (w32 a multiple of 32), clipped to cl..cr-1, ct..cb-1; rows come from row(k) through xf_row. 1 bits c1, 0 bits c0 (trans: left alone). */
static const uint8_t *xf_src; static int32_t xf_sstride; static uint32_t xf_sbit,xf_lp,xf_nb,xf_rowbytes;
static int xf_mono_run(uint32_t x,uint32_t y,uint32_t w32,uint32_t h,uint32_t cl,uint32_t ct,uint32_t cr,uint32_t cb,uint32_t c1,uint32_t c0,int trans,uint32_t bpp) {
 volatile uint32_t *hw=(volatile uint32_t *)(m2d_linear+0x200000UL); uint32_t k,j,n32=w32>>5,mask=bpp==16?0xFFFFUL:0xFFFFFFFFUL,cmd;
 wr(4,xf_depth(bpp)); wr(8,(hda->pitch>>3)<<16); wr(0x14,0); wr(0x1C,0); wr(0x30,0); wr(0x48,0); wr(0x20,0); wr(0x18,0);
 wr(0x58,c1&mask); wr(0x5C,c0&mask); wr(0x4C,c1&mask); wr(0x50,c0&mask);
 wr(0x40,(ct<<16)|cl); wr(0x44,((cb-1)<<16)|(cr-1));
 wr(12,((h-1)<<16)|(w32-1)); wr(16,(y<<16)|x);
 cmd=0xCC0A1141UL|(trans?0x400UL:0UL); /* cookbook: host-fed mono bitmap CC0A0141 (dword rows), bit 10 transparent 0 bits, bit 12 clip */
 stats.last_command=cmd; flush(); (void)rd(0x400); wr(0,cmd);
 for(k=0;k<h;k++) {
  xf_bits(xf_src+(int32_t)k*xf_sstride,xf_sbit,xf_lp,xf_nb,n32*4,xf_rowbytes);
  for(j=0;j<n32;j++) *hw=(uint32_t)xf_row[j*4]|((uint32_t)xf_row[j*4+1]<<8)|((uint32_t)xf_row[j*4+2]<<16)|((uint32_t)xf_row[j*4+3]<<24);
 }
 return idle();
}
/* the box for w pixels at dx: a 32-pixel multiple inside the screen; the padding goes on the right while the screen allows it, the rest on the left (lp = padding pixels before the first real one) */
static int xf_box(uint32_t dx,uint32_t w,uint32_t *x,uint32_t *w32,uint32_t *lp) {
 uint32_t W=(w+31)&~31UL;
 if(W>hda->width) return 0;
 *x=dx+W<=hda->width?dx:hda->width-W; *lp=dx-*x; *w32=W; return 1;
}
/* the byte range a rectangle of rows reads: rows y0..y0+h-1 of stride st from base, bytes c0..c1-1 of each */
static uint32_t xf_min=0x400000UL; /* lowest address accepted: application memory 4 MB up; the DRV's own data segment (text buffers) may be anywhere above 64 KB */
static int xf_range(uint32_t base,int32_t st,uint32_t y0,uint32_t h,uint32_t c0,uint32_t c1,uint32_t *lo,uint32_t *hi) {
 int32_t a=(int32_t)y0*st,b=(int32_t)(y0+h-1)*st; uint32_t first=base+(uint32_t)(a<b?a:b),last=base+(uint32_t)(a<b?b:a);
 if(base<xf_min||first<xf_min||last+c1>0xC0000000UL||last+c1<last) return 0;
 *lo=first+c0; *hi=last+c1; return 1;
}
static uint32_t xf_lock_page,xf_lock_n; static int xf_locked;
static int xf_lock(uint32_t lo,uint32_t hi) {
 if(lo<0x400000UL) return 1; /* below 4 MB: the system VM's own low memory (the DRV's fixed data), not pageable */
 xf_lock_page=lo>>12; xf_lock_n=((lo&0xFFFUL)+(hi-lo)+0xFFFUL)>>12;
 if(!xf_lock_n||xf_lock_n>4096UL||!_LinPageLock(xf_lock_page,xf_lock_n,0)) return 0;
 xf_locked=1; return 1;
}
/* scan a 1-bit buffer: the bounding box of its set bits (returns 0 when there are none) */
static int xf_valid(const vx2d_xfer *r) {
 if(r->cb!=sizeof(*r)||r->abi!=VX2D_ABI||r->bpp!=hda->bpp||(r->bpp!=8&&r->bpp!=16&&r->bpp!=32)) return 0;
 if(r->op==VX2D_XF_SPANS) return r->n>=1&&r->n<=((r->flags&VX2D_XF_RECTS)?96UL:VX2D_XF_SPANS_MAX)&&!(r->flags&~VX2D_XF_RECTS);
 if(!r->w||!r->h||r->w>4096||r->h>4096||r->dx>=hda->width||r->dy>=hda->height||r->w>hda->width-r->dx||r->h>hda->height-r->dy) return 0;
 if(r->src_bits!=1&&r->src_bits!=8&&r->src_bits!=16&&r->src_bits!=32) return 0;
 if(!r->src_w||!r->src_h||r->src_w>8192||r->src_h>8192||r->src_x>r->src_w||r->src_y>r->src_h||r->w>r->src_w-r->src_x||r->h>r->src_h-r->src_y) return 0;
 /* v10.279 (review): the range first: -INT32_MIN does not exist (Watcom wraps it to itself, a negative stride that passed every test below) */
 if(r->src_stride< -65536L||r->src_stride>65536L) return 0;
 {int32_t st=r->src_stride<0?-r->src_stride:r->src_stride; if(!st||(uint32_t)st<((r->src_w*r->src_bits+7)>>3)) return 0;}
 return 1;
}

/* v10.124: an engine SRCCOPY between two surfaces in video memory (offsets and pitches in bytes), no overlap, no clip */
static int xf_copy(uint32_t doff,uint32_t dp,uint32_t dx,uint32_t dy,uint32_t soff,uint32_t sp,uint32_t sx,uint32_t sy,uint32_t w,uint32_t h,uint32_t bpp) {
 uint32_t cmd=0xCC000001UL;
 wr(4,xf_depth(bpp)); wr(8,((dp>>3)<<16)|(sp>>3)); wr(0x14,doff>>3); wr(0x1C,soff>>3); wr(0x30,0); wr(0x48,0); wr(0x20,0);
 wr(0x58,0); wr(0x5C,0); /* the prologue's pattern colours (not used by SRCCOPY) */
 wr(0x18,(sy<<16)|sx); wr(12,((h-1)<<16)|(w-1)); wr(16,(dy<<16)|dx);
 stats.last_command=cmd; flush(); (void)rd(0x400); wr(0,cmd);
 return idle();
}
volatile unsigned long vx2d_save_off=0;
static uint32_t ss_x,ss_y,ss_w,ss_h,ss_gen,ss_saves,ss_restores,ss_refused; static int ss_valid;
/* .515 (bug check): GDI stretches and transfers lock the caller's pages (_LinPageLock) inside the engine lock; they claim the system
 * critical section first, as before, so the order is the critical section, then the engine lock, wherever both are held (mode sets too):
 * a VDD callback that comes with the critical section held cannot wait for an engine-lock owner that waits for the critical section */
int VX2D_stretch(const vx2d_stretch *request) {int rc;Begin_Critical_Section(0);rc=stretch_body(request);End_Critical_Section();return rc;}
static int xfer_body(vx2d_xfer *request) {
 vx2d_xfer *r=&xf_r; int rc=VX2D_SKIP,opened=0,excluded=0; unsigned long f; uint32_t why=1,lo,hi,mask;
 if(!request) return VX2D_SKIP; /* v10.279 (review): the end of the call writes the results back through it (the ioctl dispatch never passes NULL) */
 vx_lock(); gdi_map(); /* v10.273 */
 f=VX_irq_save();
 if(running) {VX_irq_restore(f); vx_unlock(); return VX2D_SKIP;}
 running=1; VX_irq_restore(f);
 complete_pending();
 vx2d_async_gate=0; vx2d_present_arm=0; /* a GDI request (v10.424: and an armed present) */
 xf_locked=0;
 if(m2d_state==3&&!lost_recover(0)) {rc=VX2D_LOST; goto done;}
 if(m2d_state==2&&fail_gen!=vx_status.set_successes) {m2d_state=0; retries++;}
 if(!request) goto skip;
 memcpy(r,request,sizeof(*r));
 if(r->mask_lin>=0x10000UL&&r->mask_lin<0xC0000000UL) xf_mask_bind(r->mask_lin); /* v10.279: as selector:offset */
 xf_min=0x400000UL;
 if(r->op==VX2D_XF_SAVE) { /* v10.124 SaveScreenBitmap */
  uint32_t bytes=hda?hda->bpp>>3:0,P;
  if(r->flags==4) {if(r->cb!=sizeof(*r)||r->abi!=VX2D_ABI) {why=6; ss_refused++; goto skip;} ss_valid=0; rc=VX2D_DONE; goto done;} /* v10.330 (review #162): the header checked first */
  why=3; if(!(vx2d_host&8)) {ss_refused++; goto skip;}
  why=4; if(!desktop_x(1)) {ss_refused++; goto skip;}
  why=5; if(!vx2d_enable||m2d_state==2||(vx3dn_rt_why=2,VX3D_busy())||vx_native_display_hold||!vx2d_save_off) {ss_refused++; goto skip;}
  why=6; if(r->cb!=sizeof(*r)||r->abi!=VX2D_ABI||r->bpp!=hda->bpp||(r->flags!=1&&r->flags!=2)||!r->w||!r->h||r->dx>=hda->width||r->dy>=hda->height||r->w>hda->width-r->dx||r->h>hda->height-r->dy) {ss_refused++; goto skip;}
  P=((r->w*bytes)+15)&~15UL;
  why=16; if(P>16376UL||P*r->h>VX2D_XF_SAVE_SIZE) {ss_refused++; goto skip;}
  if(r->flags==2&&(!ss_valid||ss_x!=r->dx||ss_y!=r->dy||ss_w!=r->w||ss_h!=r->h||ss_gen!=vx_status.set_successes)) {why=25; ss_refused++; goto skip;} /* nothing saved for this rectangle in this mode: GDI redraws */
  rc=xf_open(); if(rc!=VX2D_DONE) {if(rc==VX2D_SKIP) {why=12; ss_refused++; goto skip;} goto done;}
  opened=1; FBHDA_access_rect(r->dx,r->dy,r->dx+r->w,r->dy+r->h); excluded=1;
  if(r->flags==1) {if(!xf_copy(vx2d_save_off,P,0,0,0,hda->pitch,r->dx,r->dy,r->w,r->h,r->bpp)) {ss_valid=0; rc=xf_timeout(); goto done;} ss_x=r->dx; ss_y=r->dy; ss_w=r->w; ss_h=r->h; ss_gen=vx_status.set_successes; ss_valid=1; ss_saves++;}
  else {if(!xf_copy(0,hda->pitch,r->dx,r->dy,vx2d_save_off,P,0,0,r->w,r->h,r->bpp)) {rc=xf_timeout(); goto done;} ss_restores++;}
  stats.copies++; rc=VX2D_DONE; goto done;
 }
 why=2; if(r->op<VX2D_XF_MONO||r->op>VX2D_XF_SPANS) goto skip;
 {static const uint32_t need[5]={0,1,2,0,16}; /* v10.122: XF_TEXT removed (slower than DIBENG on this machine) */ uint32_t bit=need[r->op]; if(r->op==VX2D_XF_STAGE&&r->src_bits!=1) bit=4; why=3; if(!(vx2d_host&bit)) goto skip;}
 why=4; if(!desktop_x(1)) goto skip;
 why=5; if(!vx2d_enable||m2d_state==2||(vx3dn_rt_why=2,VX3D_busy())||vx_native_display_hold) goto skip;
 why=6; if(!xf_valid(r)) {stats.rejected++; goto skip;}
 why=7; if(r->bpp==16&&!vx2d_f16) goto skip;
 if(r->bpp==8&&!vx2d_f8) goto skip;
 mask=r->bpp==8?0xFFUL:r->bpp==16?0xFFFFUL:0xFFFFFFFFUL;
 if(r->op==VX2D_XF_MONO) {
  uint32_t x,w32,lp,rb=(r->src_w+7)>>3;
  why=8; if(r->src_bits!=1||r->bpp==8||r->rop!=0xCC||(r->flags&~VX2D_XF_TRANS)) goto skip; /* the mono host command was measured at 32bpp; 8bpp not at all */
  why=9; if(!vx2d_force&&r->w*r->h<vx2d_mono_min) goto skip;
  why=10; if(!xf_box(r->dx,r->w,&x,&w32,&lp)) goto skip;
  why=11; if(!xf_range(r->src_lin,r->src_stride,r->src_y,r->h,0,rb,&lo,&hi)||!xf_lock(lo,hi)) goto skip;
  rc=xf_open(); if(rc!=VX2D_DONE) {if(rc==VX2D_SKIP) {why=12; goto skip;} goto done;}
  opened=1; FBHDA_access_rect(r->dx,r->dy,r->dx+r->w,r->dy+r->h); excluded=1;
  xf_src=(const uint8_t *)r->src_lin+(int32_t)r->src_y*r->src_stride; xf_sstride=r->src_stride; xf_sbit=r->src_x; xf_lp=lp; xf_nb=r->w; xf_rowbytes=rb;
  if(!xf_mono_run(x,r->dy,w32,r->h,r->dx,r->dy,r->dx+r->w,r->dy+r->h,r->fg,r->bg,(r->flags&VX2D_XF_TRANS)!=0,r->bpp)) {rc=xf_timeout(); goto done;}
  xf_ops[1]++; xf_pixels+=r->w*r->h; rc=VX2D_DONE; goto done;
 }
 if(r->op==VX2D_XF_STAGE) {
  uint32_t bytes=r->bpp>>3,P=((r->w*bytes)+15)&~15UL,band,y0,k,i,sb=r->src_bits,rb=((r->src_x+r->w)*sb+7)>>3,c0=(r->src_x*sb)>>3,cmd;
  volatile uint8_t *fb=(volatile uint8_t *)hda->vram_pm32;
  /* .595: a brush that is not one colour (dithered, or made from a colour bitmap) as the pattern operand: the command without the
   * fixed-colour bit, register 20 = the 8x8 block and the rectangle's phase, as the colour pattern fill (v10.77), with the source
   * from the staging block. Was DIBENG's (VX2GDI: the 16 bpp brush case drew nothing: its colour is dithered there). */
  why=13; if(!vx2d_stage_off||(sb!=1&&sb!=r->bpp)||r->rop>255||!rop_uses_s(r->rop)||(r->flags&~(VX2D_XF_FIXPAT|VX2D_XF_COLPAT))) goto skip;
  why=14; if(rop_uses_p(r->rop)!=((r->flags&(VX2D_XF_FIXPAT|VX2D_XF_COLPAT))!=0)||(r->flags&(VX2D_XF_FIXPAT|VX2D_XF_COLPAT))==(VX2D_XF_FIXPAT|VX2D_XF_COLPAT)) goto skip;
  why=18; if((r->flags&VX2D_XF_COLPAT)&&(!vx2d_brush_off||r->tpl.bpp!=r->bpp)) goto skip;
  why=15; if(!vx2d_force&&r->w*r->h<vx2d_host_min) goto skip;
  why=16; if(P>16376UL||P*r->h<P) goto skip;
  band=VX2D_XF_STAGE_SIZE/P; if(!band) goto skip; if(band>r->h) band=r->h;
  why=11; if(!xf_range(r->src_lin,r->src_stride,r->src_y,r->h,c0,rb,&lo,&hi)||!xf_lock(lo,hi)) goto skip;
  rc=xf_open(); if(rc!=VX2D_DONE) {if(rc==VX2D_SKIP) {why=12; goto skip;} goto done;}
  opened=1; FBHDA_access_rect(r->dx,r->dy,r->dx+r->w,r->dy+r->h); excluded=1;
  if(r->flags&VX2D_XF_COLPAT) put_block(&r->tpl);
  for(y0=0;y0<r->h;y0+=band) {
   uint32_t nh=r->h-y0<band?r->h-y0:band;
   for(k=0;k<nh;k++) {
    const uint8_t *s=(const uint8_t *)r->src_lin+(int32_t)(r->src_y+y0+k)*r->src_stride; volatile uint8_t *d=fb+vx2d_stage_off+k*P;
    if(sb==1) {
     uint32_t c1=r->fg&mask,cz=r->bg&mask,x=r->src_x;
     if(bytes==4) for(i=0;i<r->w;i++,x++) ((volatile uint32_t *)d)[i]=((s[x>>3]>>(7-(x&7)))&1)?c1:cz;
     else if(bytes==2) for(i=0;i<r->w;i++,x++) ((volatile uint16_t *)d)[i]=(uint16_t)(((s[x>>3]>>(7-(x&7)))&1)?c1:cz);
     else for(i=0;i<r->w;i++,x++) d[i]=(uint8_t)(((s[x>>3]>>(7-(x&7)))&1)?c1:cz);
    } else {
     const uint32_t *s4=(const uint32_t *)(s+r->src_x*bytes); uint32_t nd=(r->w*bytes)>>2;
     for(i=0;i<nd;i++) ((volatile uint32_t *)d)[i]=s4[i];
     for(i=nd*4;i<r->w*bytes;i++) d[i]=s[r->src_x*bytes+i];
    }
   }
   flush(); (void)*(volatile uint32_t *)(fb+vx2d_stage_off+(nh-1)*P); /* the CPU writes have left the write-combining buffers before the engine reads them */
   wr(4,xf_depth(r->bpp)); wr(8,((hda->pitch>>3)<<16)|(P>>3)); wr(0x14,0); wr(0x1C,vx2d_stage_off>>3); wr(0x30,0); wr(0x48,0);
   wr(0x20,(r->flags&VX2D_XF_COLPAT)?(uint32_t)(((vx2d_brush_off>>3)&0x03FFFFFFUL)|(((r->dy+y0)&7UL)<<29)|((r->dx&7UL)<<26)):0);
   wr(0x58,r->pfg&mask); wr(0x5C,r->pfg&mask);
   cmd=(r->rop<<24)|1UL;
   if(r->flags&VX2D_XF_FIXPAT) {cmd|=0x2000UL; wr(0x4C,r->pfg&mask); wr(0x50,r->pfg&mask);}
   wr(0x18,0); wr(12,((nh-1)<<16)|(r->w-1)); wr(16,((r->dy+y0)<<16)|r->dx);
   stats.last_command=cmd; flush(); (void)rd(0x400); wr(0,cmd);
   if(!idle()) {rc=xf_timeout(); goto done;}
  }
  xf_ops[2]++; xf_pixels+=r->w*r->h; stats.copies++; rc=VX2D_DONE; goto done;
 }
 { /* XF_SPANS */
  vx2d_request t; uint32_t i,minx=0xFFFF,miny=0xFFFF,maxx=0,maxy=0,area=0,cmd,blk=0,sw=vx2d_span_wait; int rects=(r->flags&VX2D_XF_RECTS)!=0;
  memcpy(&t,&r->tpl,sizeof(t));
  why=21; if(t.op!=VX2D_FILL&&t.op!=VX2D_MONO&&t.op!=VX2D_COLOR) goto skip;
  if(t.flags||t.bpp!=r->bpp||t.dst.offset||t.dst.pitch!=hda->pitch||t.dst.width!=hda->width||t.dst.height!=hda->height) goto skip;
  if(t.rop>255||rop_uses_s(t.rop)||(!((t.rop^(t.rop>>1))&0x55)&&!((r->flags&VX2D_XF_RECTS)&&(vx2d_host&32UL)))) goto skip; /* the ROP must read the destination (else DIBENG writes as fast); v10.130: rectangle bands with ACCEL_HOST bit 32 may also only write (wide COPYPEN pens: many bands per call) */
  if((t.op==VX2D_MONO&&!vx2d_fpat)||(t.op==VX2D_COLOR&&!(vx2d_fpat&&vx2d_fcol))) goto skip;
  if(!vx2d_col8&&t.bpp==8&&t.op==VX2D_COLOR) {why=24; goto skip;} /* v10.137: ACCEL_COL8_BANDS 0 */
  if(t.op==VX2D_COLOR&&!(sw&4UL)) sw=0; /* v10.134: colour-pattern rectangles keep the full wait (VX2OPT2: 19 pixels differed at 8bpp with one read) unless ACCEL_SPAN_WAIT bit 4 */
  sw&=3UL;
  for(i=0;i<r->n;i++) {
   uint32_t y,a,b,hh;
   if(rects) {y=r->sp[i*4]; a=r->sp[i*4+1]; b=r->sp[i*4+2]; hh=r->sp[i*4+3];} else {y=r->sp[i*3]; a=r->sp[i*3+1]; b=r->sp[i*3+2]; hh=1;}
   why=22; if(y>=hda->height||a>=b||b>hda->width||!hh||hh>hda->height-y) goto skip;
   t.dx=a; t.dy=y; t.width=b-a; t.height=hh;
   if(!validate(&t)) {stats.rejected++; goto skip;}
   if(a<minx) minx=a; if(b>maxx) maxx=b; if(y<miny) miny=y; if(y+hh>maxy) maxy=y+hh; area+=(b-a)*hh;
  }
  why=23; if(!vx2d_force&&area<vx2d_host_min) goto skip;
  rc=xf_open(); if(rc!=VX2D_DONE) {if(rc==VX2D_SKIP) {why=12; goto skip;} goto done;}
  opened=1; FBHDA_access_rect(minx,miny,maxx,maxy); excluded=1;
  wr(4,xf_depth(r->bpp)); wr(8,(hda->pitch>>3)<<16); wr(0x14,0); wr(0x1C,0); wr(0x30,0); wr(0x48,0); wr(0x20,0);
  wr(0x58,t.fg&mask); wr(0x5C,t.bg&mask);
  cmd=(t.rop<<24)|1UL;
  if(t.op==VX2D_FILL) {cmd|=0x2000UL; wr(0x4C,t.fg&mask);}
  else if(t.op==VX2D_MONO) cmd|=0xA00UL;
  else {put_block(&t); blk=(vx2d_brush_off>>3)&0x03FFFFFFUL;}
  for(i=0;i<r->n;i++) {
   uint32_t y,a,b,hh;
   if(rects) {y=r->sp[i*4]; a=r->sp[i*4+1]; b=r->sp[i*4+2]; hh=r->sp[i*4+3];} else {y=r->sp[i*3]; a=r->sp[i*3+1]; b=r->sp[i*3+2]; hh=1;}
   if(t.op==VX2D_MONO) { /* the 8 pattern rows from this rectangle's first line, bits mirrored (as VX2D_submit anchors a VX2D_MONO fill) */
    uint32_t m0=0,m1=0,k;
    for(k=0;k<8;k++) {uint32_t rb=(t.mono[((y+k)&7)>>2]>>((3-((y+k)&3))*8))&255UL,rv=vx_rev8(rb); if(k<4) m0|=rv<<(k*8); else m1|=rv<<((k-4)*8);}
    wr(0x24,m0); wr(0x28,m1);
   } else if(t.op==VX2D_COLOR) wr(0x20,blk|((y&7UL)<<29)|((a&7UL)<<26));
   wr(12,((hh-1)<<16)|(b-a-1)); wr(16,(y<<16)|a);
   stats.last_command=cmd; (void)rd(0x400); wr(0,cmd);
   if(i+1==r->n||!sw) {if(!idle()) {request->done=i; rc=xf_timeout(); goto done;}}
   else if(sw>=2&&!(sw==2?idle_from_start():idle_n(4))) {request->done=i; rc=xf_timeout(); goto done;} /* v10.132: ACCEL_SPAN_WAIT */
  }
  request->done=r->n; xf_ops[4]+=r->n; xf_pixels+=area; stats.fills+=r->n; rc=VX2D_DONE; goto done;
 }
skip:
 xf_skips++; xf_why=why; rc=m2d_state==3?VX2D_LOST:VX2D_SKIP; /* v10.330 (review #187): as VX2D_stretch */
done:
 if(opened) xf_close();
 if(rc==VX2D_DONE) {vx2d_ops++; vx_status.draws++;}
 if(excluded) FBHDA_access_end(0); /* v10.139 (review F05): released even when lost */
 if(xf_locked) {_LinPageUnLock(xf_lock_page,xf_lock_n,0); xf_locked=0;}
 if(m2d_state==3) fence();
 request->mask_out=vx2d_host; request->result=rc==VX2D_SKIP?why:0;
 xf_mask_out();
 running=0; vx_unlock(); return rc;
}
int VX2D_xfer(vx2d_xfer *request) {int rc;Begin_Critical_Section(0);rc=xfer_body(request);End_Critical_Section();return rc;} /* .515: as VX2D_stretch */

/* v10.123: hardware gamma (the cookbook's gamma row; OpenChrome viaIGA1SetGamma): at 16 and 32 bpp CR33 bit 7 makes the DAC a gamma table for IGA1, loaded through the palette ports
 * (PEL mask 3C6 = FF, index 3C8, three 8-bit values per entry on 3C9; the DAC is 8 bits on this board). At 8 bpp the DAC is the palette: the bit is cleared and nothing is loaded.
 * Registry ACCEL_GAMMA (default 1), live control 0xF6000000|v, counters 0xFFFFFE1A: out[0] loads, out[1] CR33 as read back, out[2] live switch, out[3] entry 128 of the red ramp. */
volatile unsigned long vx2d_gamma=1,vx2d_gamma_cfg=1;
static uint32_t gm_loads,gm_cr33,gm_r128; static int gm_have; static uint16_t gm_ramp[3][256];
static void gm_write(int on) {
 unsigned long f=VX_irq_save(); unsigned short cb=(inp(0x3CC)&1)?0x3D4:0x3B4; /* v10.309 (review): the CRTC base as flip and EDID take it */ unsigned char o4=inp(0x3C4),o5=inp(cb),sr10,cr; int i;
 outp(0x3C4,0x10); sr10=inp(0x3C5); outp(0x3C5,(unsigned char)(sr10|1)); /* extended registers open */
 outp(cb,0x33); cr=inp((unsigned short)(cb+1));
 if(on) {
  outp(0x3C6,0xFF); outp(0x3C8,0);
  for(i=0;i<256;i++) {outp(0x3C9,(unsigned char)(gm_ramp[0][i]>>8)); outp(0x3C9,(unsigned char)(gm_ramp[1][i]>>8)); outp(0x3C9,(unsigned char)(gm_ramp[2][i]>>8));}
  cr|=0x80; gm_loads++;
 } else cr&=(unsigned char)~0x80;
 outp((unsigned short)(cb+1),cr); outp(cb,0x33); gm_cr33=inp((unsigned short)(cb+1));
 outp(cb,o5); outp(0x3C4,0x10); outp(0x3C5,sr10); outp(0x3C4,o4);
 VX_irq_restore(f);
}
/* ramp: 3 x 256 WORDs (red, green, blue), NULL = apply the stored one again (after a mode set). Returns 1 when the hardware took it. */
int VX2D_gamma_load(const uint16_t *ramp) {
 int on;
 if(ramp) {memcpy(gm_ramp,ramp,sizeof(gm_ramp)); gm_have=1; gm_r128=gm_ramp[0][128];}
 if(!hda||!vx_status.fb_linear||vx_status.vga_mode) return 0;
 on=vx2d_gamma&&gm_have&&(hda->bpp==16||hda->bpp==32);
 if(!on&&!(hda->bpp==8||!vx2d_gamma)) return 0; /* never set: leave the BIOS state alone */
 gm_write(on); return on;
}
void VX2D_gamma_off(void) {if(hda&&gm_have) gm_write(0);} /* a DOS box or text mode: the table must not apply */
int VX2D_gamma_wanted(void) {return vx2d_gamma&&hda&&(hda->bpp==16||hda->bpp==32);}
/* v10.105: hardware cursor (VX855 "hardware icon" engine). Registers and start-up values as OpenChrome's via_display.c uses them for VX855 (VX2CUR checks them on the hardware).
 * Mode 1: the primary display's set (BAR1 0x2E4 0x2E8 0x2EC 0x2F0 control 0x2F4 image address 0x2F8 position 0x2FC origin offset 0x120C inverse colour);
 * mode 2: the second display's set (0x260 control 0x224 image address 0x208 position 0x20C origin offset 0x268 0x270 0x274 0x278).
 * The image is 64x64 pixels, 32 bits ARGB (alpha 0 = transparent), 16 KB, kept in a block at the end of video memory (vx2d_hwcur_off, 0 = none). Windows cursors are AND/XOR masks:
 * a pixel with AND 0 is drawn opaque in its XOR colour, AND 1 with XOR 0 is transparent; AND 1 with XOR not 0 inverts the screen, which this cursor cannot do, so such a shape stays
 * with the software cursor. Only 16 and 32 bpp, and only while the frame buffer is the real video memory (not the shadow copy). Registry ACCEL_HWCURSOR: 0 off (default until VX2CUR passes), 1 or 2 = register set. */
volatile unsigned long vx2d_hwcur_off=0,vx2d_hwcur_cfg=0,vx2d_hwcur=0;
/* v10.124: inverting pixels (AND 1, XOR white: the I-beam, the size cursors). The manual has an inverse colour register (0x120C primary, 0x274 second display) next to the transparent colour (0x2EC):
 * an icon pixel of that colour shows the screen inverted. How the compare meets the alpha-blended RGB32 mode is not written down, so VX2CUR2 picks the variant on the board (registry ACCEL_HWCURSOR_INV):
 * 0 inverting shapes stay with the software cursor (default), 1 pixel 0x00FFFFFF (alpha 0, white; inverse colour register FFFFFF as before), 2 pixel 0xFFFF00FE with inverse colour FF00FE,
 * 3 pixel 0x00FF00FE with inverse colour FF00FE. Live control 0xF5000000|v.
 * v10.126 (VX2CUR2 v1.1 on the board: variant 2 drew a solid magenta I-beam, 1 and 3 nothing: in the alpha-blended mode the colour keys are not compared): variant 4 = key mode: control without
 * blending (bits 29, 28, 2 clear; variant 5 the same with bit 2 HI blending enable set; 64x64, RGB32, constant alpha F), transparent pixels = the transparent key FF00FF (0x2EC), inverting pixels = the inverse key FF00FE (0x120C), opaque pixels their colour
 * (a colour equal to a key moves one step in green). Primary display register set only. */
/* .530: the cursor check (ACCEL_CURSOR_CHECK: the shadow of the cursor registers and the kept image, controls 0xFFFFFE1D-1F, 70+i) removed */
static void hcw(uint32_t r,uint32_t v) {wr(r,v);}
static const uint32_t hc_inv=0x00FFFFFFUL; /* the inverse colour register (0x120C, 0x274): white */
static uint32_t hc_shows,hc_hides,hc_moves,hc_declined,hc_sets,hc_ctrl;
static int hc_on,hc_ready,hc_off_pending; static int32_t hc_lx,hc_ly; /* v10.142 (review O02): position last written while shown */
static int hc_map(void) {
 uint32_t base=vx_status.bar1&0xFFFFFFF0UL;
 if(native_fault) return 0; /* A lost native owner also fences cursor/video writes. */
 if((vx_status.bar1&15)||base<0x100000UL||base>0xFFC00000UL) return 0;
 if(!m2d_linear) {
  m2d_linear=_MapPhysToLinear(base,0x400000UL,0);
  if(!m2d_linear||m2d_linear==0xFFFFFFFFUL) {m2d_linear=0; return 0;}
 }
 return 1;
}
static int hc_rmap(void) {return m2d_linear?1:hc_map();} /* v10.326: readings for the cursor check, through the mapping made even after a loss */
static uint32_t hc_reg(int k) { /* 0 control 1 image address 2 position 3 origin offset */
 static const uint32_t r1[4]={0x2F0UL,0x2F4UL,0x2F8UL,0x2FCUL},r2[4]={0x260UL,0x224UL,0x208UL,0x20CUL};
 return vx2d_hwcur==2?r2[k]:r1[k];
}
/* v10.330 (review #188): a hide that could not reach the unit (no mapping, or the native loss fence) is kept pending and tried again at the
 * next hide; it cleared hc_on and was forgotten, so the sprite could stay while the driver counted it hidden */
void VX2D_hwcur_hide(void) {
 hc_pos_ok=0;
 if(!hc_on&&!hc_off_pending) return;
 if(hc_on) {hc_on=0; hc_hides++;}
 if(vx2d_hwcur&&hc_ready&&hc_map()) {hc_ctrl&=~1UL; hcw(hc_reg(0),hc_ctrl); hc_off_pending=0;}
 else hc_off_pending=(vx2d_hwcur&&hc_ready)?1:0;
}
/* v10.258: brightness (0..255*1000) for the mono cursor's two colours; a 24-bit colour as the mono cursor's colour registers take it
 * (10 bits a channel, red 29:20, green 19:10, blue 9:0, as viafb writes them for VX855; white = 3FFFFFFF, black = 0) */
/* the shape: am/xm = per-pixel masks (stride dwords a row, 24-bit colours), w*h <= 32x32, bpp = the screen's. 1 = the hardware shows it; 0 = left to the software cursor. */
int VX2D_hwcur_set(const uint32_t *am,const uint32_t *xm,uint32_t stride,uint32_t w,uint32_t h,uint32_t bpp) {
 volatile uint32_t *img; uint32_t x,y,a,c; /* .530: ACCEL_HWCURSOR_INV removed (default 0 since v10.357): the inverting and colour-XOR variants
  * (1-9: the HI's key mode and inverse colour, the legacy mono unit) are gone; a shape with such pixels is the software cursor's */
 VX2D_hwcur_hide(); hc_ready=0; hc_pos_ok=0; /* v10.251 (review): hide while the old shape is still ready, or a refused new shape left the old one enabled in the hardware with hc_on 0 */
 if(!vx2d_hwcur||vx2d_hwcur>2||!vx2d_hwcur_off||!hda||!hda->vram_pm32||(uint32_t)hda->vram_pm32!=vx_status.fb_linear) {hc_declined++; return 0;}
 if((bpp!=8&&bpp!=16&&bpp!=32)||!am||!xm||!w||!h||w>32||h>32||stride<w|| /* v10.262: 8bpp too (the caller hands colours, not palette indices) */
    (vx2d_hwcur_off&0x3FF)||vx2d_hwcur_off>hda->vram_size_virt||0x4000UL>hda->vram_size_virt-vx2d_hwcur_off) {hc_declined++; return 0;} /* v10.330 (review #193): no sum that can wrap */
 for(y=0;y<h;y++) for(x=0;x<w;x++) {
  a=am[y*stride+x]; c=xm[y*stride+x]&0xFFFFFFUL;
  if(!a) continue; /* opaque */
  if(a==0xFFFFFFFFUL&&!c) continue; /* transparent */
  hc_declined++; return 0; /* inverting or colour-XOR: the software cursor */
 }
 if(!hc_map()) {hc_declined++; return 0;}
 img=(volatile uint32_t*)((uint8_t*)hda->vram_pm32+vx2d_hwcur_off);
 hcw(0x2D0UL,0); if(vx2d_hwcur==1) hcw(0x2F0UL,hc_ctrl&~1UL);
 for(y=0;y<64;y++) for(x=0;x<64;x++) {
  uint32_t v=0;
  if(x<w&&y<h) {a=am[y*stride+x]; c=xm[y*stride+x]&0xFFFFFFUL; if(a==0) v=0xFF000000UL|c;}
  img[y*64+x]=v;
 }
 if(vx2d_hwcur==1) {hcw(0x2ECUL,0); hcw(0x120CUL,hc_inv); hcw(0x2E4UL,0x00FFFFFFUL); hcw(0x2E8UL,0x0D000D0FUL); hc_ctrl=0x36000004UL;}
 else {hcw(0x270UL,0); hcw(0x274UL,hc_inv); hcw(0x268UL,0x000E0000UL); hcw(0x278UL,0x0E0F0000UL); hc_ctrl=0xB6000004UL;}
 (void)img[4095]; /* v10.327 (review): the write-combined image drained before the base register is written */
 hcw(hc_reg(0),hc_ctrl); hcw(hc_reg(1),(uint32_t)vx2d_hwcur_off);
 hc_ready=1; hc_sets++; return 1;
}
/* position of the image's top left corner (may be negative); shows the cursor when it was hidden */
int VX2D_hwcur_move(int32_t x,int32_t y) {
 uint32_t xo=0,yo=0;
 VX3DN_ev(14,0);
 if(!hc_ready||!vx2d_hwcur||!hc_map()) return 0;
 if(hc_on&&hc_pos_ok&&x==hc_lx&&y==hc_ly) return 1; /* v10.142: shown at this position already: nothing to write */
 /* v10.330 (review #191): a position the registers cannot hold (the 64x64 image wholly left of or above the screen: the origin field is 8 bits;
  * x or y past 2047: the fields wrap) hides the unit instead of wrapping onto the screen; the next move inside shows it again */
 if(x<=-64||y<=-64||x>0x7FF||y>0x7FF) {VX2D_hwcur_hide(); hc_lx=x; hc_ly=y; return 1;}
 hc_lx=x; hc_ly=y;
 if(x<0) {xo=(uint32_t)(-x)&0xFEUL; x=0;}
 if(y<0) {yo=(uint32_t)(-y)&0xFEUL; y=0;}
 hcw(hc_reg(2),((uint32_t)x<<16)|((uint32_t)y&0x7FFUL)); hcw(hc_reg(3),(xo<<16)|(yo&0x7FFUL));
 hc_moves++;
 if(!hc_on) {hc_on=1; hc_shows++; hc_ctrl|=1UL; hcw(hc_reg(0),hc_ctrl);}
 hc_pos_ok=1;
 return 1;
}
int VX2D_hwcur_active(void) {return hc_on;}
/* v10.252 (review): after an engine reset the cursor unit's registers may be gone while hc_on / hc_ready still say shown: forget them,
 * so the next set programs the unit and the image again and the next move enables it */
void VX2D_hwcur_invalidate(void) {
 if(vx2d_hwcur&&hc_map()) {hcw(hc_reg(0),hc_ctrl&~1UL); hc_off_pending=0;} /* off whatever the reset left: shown again only if it should be */
 hc_on=0;hc_ready=0;hc_pos_ok=0;
}
/* v10.106: video overlay on the VX855 V1 video engine, programmed as OpenChrome's via_xv_overlay.c does it for YUY2 without the HQV unit (V1 control 0x230, QWORDs per line 0x234,
 * start 0x254, stride 0x23C, window 0x240/0x244, source height and width count 0x26C, zoom 0x24C, mini 0x250, colour keys 0x220/0x264, FIFO 0x258, compose/fire 0x298).
 * Only enlargements (or 1:1) are done here: the vertical shrink of the V1 unit needs the HQV unit, and the horizontal one is left out. The register writes are
 * committed together by the fire bit of 0x298 (the command loads at the next vertical blank). Registry ACCEL_OVERLAY, see vx855_2d.h. */
volatile unsigned long vx2d_ovl_cfg=0,vx2d_ovl=0;
static uint32_t ov_updates,ov_flips,ov_hides,ov_refused,ov_reason;
static int ov_on,ov_want,ov_hqv_on,ov_odd;
static vx_ovl_req ov_last;
volatile unsigned long vx2d_hqv_off=0; /* v10.108: 3 buffers of 2 MB at the end of the video memory for the HQV scaler (0 = none) */
#define HQV_BUF 0x200000UL
static uint32_t ov_stage_n,ov_cofs,ov_probe_val,ov_last_refuse; /* ov_last_refuse: reason of the last refused request, returned by QUERY */
/* The HQV of this chip family takes planar video as NV12 (a Y plane and one plane of interleaved U,V pairs), as OpenChrome feeds it:
 * the V and U planes of a YV12/I420 surface are interleaved into a staging area (two of 1 MB behind the HQV buffers, used alternately) and its address goes to the HQV. */
static uint32_t ov_nv12(const vx_ovl_req *q) {
 uint8_t *base=(uint8_t*)hda->vram_pm32,*d,*a,*b;
 uint32_t cp=q->pitch>>1,rows=q->sh>>1,w2=q->sw>>1,x,y,off=vx2d_hqv_off+3UL*HQV_BUF+(ov_stage_n&1UL)*0x100000UL;
 const uint8_t *c1=base+q->offset+q->pitch*(uint32_t)q->sh,*c2=c1+cp*rows,*u,*v;
 ov_stage_n++;
 if(q->fmt==VXO_FMT_YV12) {v=c1; u=c2;} else {u=c1; v=c2;}
 d=base+off;
 for(y=0;y<rows;y++) {
  a=(uint8_t*)u+y*cp; b=(uint8_t*)v+y*cp;
  /* v10.142 (review O04): four U and four V bytes per pair of dword reads (video memory reads are the slow part), two dwords out; the tail byte by byte */
  for(x=0;x+4<=w2;x+=4) {
   uint32_t uu=*(const uint32_t*)(a+x),vv=*(const uint32_t*)(b+x);
   *(uint32_t*)(d+2*x)=(uu&0xFFUL)|((vv&0xFFUL)<<8)|((uu&0xFF00UL)<<8)|((vv&0xFF00UL)<<16);
   *(uint32_t*)(d+2*x+4)=((uu>>16)&0xFFUL)|((vv>>8)&0xFF00UL)|((uu>>8)&0xFF0000UL)|(vv&0xFF000000UL);
  }
  for(;x<w2;x++) {d[2*x]=a[x]; d[2*x+1]=b[x];}
  d+=q->pitch;
 }
 return off;
}
/* the previous command must have been loaded. v10.337 (board, VXPROBE ovkey: of two UpdateOverlay calls in a row, the second was refused
 * now and then, and a refused update hides the overlay): the fire bits clear at the next vertical blank, up to a frame (16.7 ms at 60
 * Hz) after the previous update; 50000 register reads ended about there. The wait is now up to 40 ms of the clock (two frames at 50 Hz),
 * the clock read every 256 polls (a VMM service, several microseconds) */
static int ov_wait(void) {
 uint32_t n,start=0; int have=0;
 for(n=0;n<5000000UL;n++) {
  if(!(rd(0x298UL)&0xC0000000UL)) return 1;
  if(!(n&255UL)) {if(!have) {start=Get_System_Time(); have=1;} else if((uint32_t)(Get_System_Time()-start)>=40UL) return 0;}
 }
 return 0;
}
/* v10.330 (review #189): the HQV flag and the overlay's are cleared only when the registers were written; a hide that could not reach
 * them (no mapping, the native loss fence) is kept pending and tried again at the next hide, instead of the overlay counting as hidden
 * while the hardware still shows it */
static int ov_off_pending;
static void ov_hqv_off(void) { /* switch the HQV scaler off */
 if(ov_hqv_on&&hc_map()) {wr(0x3D0UL,rd(0x3D0UL)&~0x08000000UL); ov_hqv_on=0;}
}
static void ov_hw_hide(void) {
 uint32_t c;
 if(!ov_on&&!ov_off_pending) return;
 if(ov_on) {ov_on=0; ov_hides++;}
 if(!hc_map()) {ov_off_pending=1; return;}
 ov_off_pending=0;
 (void)ov_wait();
 wr(0x258UL,0x0C000000UL|0x00000800UL|0x0FUL); /* the FIFO setting OpenChrome leaves behind */
 if(vx2d_ovl&VXO_M_ALPHA) {wr(0x210UL,0xFF00UL); wr(0x298UL,rd(0x298UL)|0x00100000UL);}
 if(vx2d_ovl&VXO_M_CKEY) {wr(0x228UL,0UL); wr(0x22CUL,0UL);}
 ov_hqv_off();
 wr(0x230UL,rd(0x230UL)&~1UL);
 c=rd(0x298UL)&~(0xC0000000UL|0x2FUL); wr(0x298UL,c|0x80000000UL);
 (void)rd(0x298UL);
}
void VX2D_ovl_hide(void) {ov_want=0; ov_hw_hide();}
/* a*b/c without 64 bit arithmetic (a up to 2^26, b and c up to 2^12) */
static uint32_t ov_muldiv(uint32_t a,uint32_t b,uint32_t c) {return (a/c)*b+((a%c)*b)/c;}
static int ov_planar(uint32_t f) {return f==VXO_FMT_YV12||f==VXO_FMT_I420;}
/* v10.288 (review): the source surface inside video memory: its pitch, alignment, and the offset against the size by subtraction
 * (offset + size wrapped at 32 bits and passed). UPDATE, MOVE (through ov_program) and FLIP (the scaler takes the new offset directly
 * and ov_nv12 reads its chroma with the CPU) all check through here. q's source rectangle was checked before (ov_program). */
static int ov_src_ok(const vx_ovl_req *q,int planar) {
 uint32_t sizeb,vram=hda?hda->vram_size_virt:0UL;
 if(q->pitch<32||q->pitch>0x1FFFUL||(q->pitch&15)||(q->offset&15)||q->sb<=0||q->sh>2048UL) return 0;
 /* v10.330 (review #195): a row of the source fits in its pitch (4 bytes a pixel RGB32, 2 YUY2 / RGB16 / RGB15, 1 the planar luma) */
 if(q->sw>0x1FFFUL||q->sw*(q->fmt==VXO_FMT_RGB32?4UL:planar?1UL:2UL)>q->pitch) return 0;
 sizeb=q->pitch*(uint32_t)q->sb; if(planar) sizeb=q->pitch*(uint32_t)q->sh+(q->pitch*(uint32_t)q->sh>>1);
 if(sizeb>vram||q->offset>vram-sizeb) return 0;
 if(planar&&((q->sw&1)||(q->sh&1)||(q->pitch&31))) return 0;
 return 1;
}
/* program the engine from q (offset is the surface offset); 0 = done (visible), 1 = refused, 2 = done, nothing visible */
static int ov_program(const vx_ovl_req *q) {
 uint32_t scrw,scrh,vctl,comp,sw0,sh0,dw,dh,sl,st,sr,sb,dl,dt,dr,db,sw2,sh2,dw2,dh2,zoom=0,mini=0,fetch,rel,a0,cnt;
 int32_t t;
 uint32_t set=(uint32_t)(vx2d_ovl&3UL),fm=q->fmt,m=(uint32_t)vx2d_ovl;
 int rgb=(fm==VXO_FMT_RGB16||fm==VXO_FMT_RGB32||fm==VXO_FMT_RGB15),planar=ov_planar(fm),hqv=0,bob=(q->flags&VXO_BOB)!=0,inter=(q->flags&VXO_INTERLEAVED)!=0;
 if(!set) {ov_reason=1; return 1;}
 if(!hda||vx_status.stage!=6||vx_status.failure||vx_status.vga_mode||(hda->bpp!=8&&hda->bpp!=16&&hda->bpp!=32)||!hda->width||!hda->height) {ov_reason=2; return 1;} /* v10.130: 8bpp desktops too (the key is a palette index) */
 if(fm>VXO_FMT_RGB15||(rgb&&!(m&VXO_M_HQV))||(planar&&!(m&VXO_M_PLANAR))) {ov_reason=3; return 1;}
 if((q->flags&VXO_SRCKEY)&&(!(m&VXO_M_CKEY)||rgb)) {ov_reason=12; return 1;}
 if(bob&&(!(m&VXO_M_BOB)||rgb)) {ov_reason=13; return 1;}
 if((q->flags&VXO_ALPHA)&&!(m&VXO_M_ALPHA)) {ov_reason=15; return 1;}
 if(!hc_map()) {ov_reason=4; return 1;}
 scrw=hda->width; scrh=hda->height;
 if(q->sl<0||q->st<0||q->sr<=q->sl||q->sb<=q->st||(uint32_t)q->sr>q->sw||(uint32_t)q->sb>q->sh||q->sw>2048UL||q->sh>2048UL) {ov_reason=5; return 1;}
 if(!ov_src_ok(q,planar)) {ov_reason=6; return 1;}
 if(q->dl<-16000||q->dt<-16000||q->dr>16000||q->db>16000||q->dr<=q->dl||q->db<=q->dt) {ov_reason=7; return 1;}
 sl=(uint32_t)q->sl; st=(uint32_t)q->st; sr=(uint32_t)q->sr; sb=(uint32_t)q->sb;
 sw0=sr-sl; sh0=sb-st; dw=(uint32_t)(q->dr-q->dl); dh=(uint32_t)(q->db-q->dt);
 if(dw>sw0*16UL||dh>sh0*16UL) {ov_reason=9; return 1;}
 /* clip to the screen: the source rectangle follows the destination proportionally (rounded, as OpenChrome does) */
 dl=q->dl<0?0:(uint32_t)q->dl; dt=q->dt<0?0:(uint32_t)q->dt;
 dr=q->dr>(int32_t)scrw?scrw:(uint32_t)q->dr; db=q->db>(int32_t)scrh?scrh:(uint32_t)q->db;
 if(q->dl>=(int32_t)scrw||q->dt>=(int32_t)scrh||q->dr<=0||q->db<=0||dr<=dl||db<=dt) return 2;
 if(q->dl<0) sl+=((uint32_t)(-q->dl)*sw0+(dw>>1))/dw;
 if(q->dr>(int32_t)scrw) sr=(uint32_t)q->sl+(((scrw-(uint32_t)q->dl)*sw0+(dw>>1))/dw);
 if(q->dt<0) st+=((uint32_t)(-q->dt)*sh0+(dh>>1))/dh;
 if(q->db>(int32_t)scrh) sb=(uint32_t)q->st+(((scrh-(uint32_t)q->dt)*sh0+(dh>>1))/dh);
 if(sr<=sl||sb<=st||sr>q->sw||sb>q->sh) return 2;
 sw2=sr-sl; sh2=sb-st; dw2=dr-dl; dh2=db-dt;
 if(sw2>dw2||sh2>dh2||rgb||planar) hqv=1; /* planar video goes through the HQV (as an NV12 copy) on this chip */
 if(hqv&&(q->flags&VXO_SRCKEY)) {ov_reason=12; return 1;}
 if(hqv&&(!((m&VXO_M_HQV)||planar)||!vx2d_hqv_off)) {ov_reason=8; return 1;}
 if(hqv&&(q->pitch*(uint32_t)q->sh*(planar?2UL:1UL)>HQV_BUF)) {ov_reason=14; return 1;}
 if(!ov_wait()) {ov_reason=10; return 1;}
 comp=(rd(0x298UL)&~(0x00000001UL|0xC0000000UL))|0x10000000UL;
 comp&=~0x2FUL;
 /* colour keys */
 if(q->flags&VXO_KEY) {
  uint32_t k=q->key&0xFFFFFFUL;
  if(hda->bpp==8) k&=0xFFUL; /* v10.130: manual (0x220): 256 colours = bits 7:0, the palette index itself, no DAC lookup */
  if(hda->bpp==16) { /* the key is a 5-6-5 pixel; the engine compares 24-bit colours: the low bits are zero, or (bit 4) replicated */
   uint32_t r5=(k>>11)&31UL,g6=(k>>5)&63UL,b5=k&31UL;
   if(m&VXO_M_KEYRAW) k&=0xFFFFUL; /* .585 */
   else if(m&16UL) k=(((r5<<3)|(r5>>2))<<16)|(((g6<<2)|(g6>>4))<<8)|((b5<<3)|(b5>>2));
   else k=((r5<<3)<<16)|((g6<<2)<<8)|(b5<<3);
  }
  k|=0x40000000UL;
  wr(0x220UL,k); wr(0x264UL,k);
  comp|=0x00000001UL|0x00000020UL;
  if(set==2) comp|=0x00010000UL;
 } else {wr(0x220UL,0UL); wr(0x264UL,0UL);} /* no key: clear both key registers, or the key of an earlier update (with its enable bit) stays in force */
 if(m&VXO_M_ALPHA) { /* constant alpha of the video over the graphics (0x210 bits 15..8, bits 1..0 = 0), applied in the alpha window = the overlay's window */
  /* DirectFB's UniChrome sequence (VIAAlphaWin): constant mode, factor in bits 15..8, bits 20..16 (the alpha stream request expire field) = 8; no alpha window; fired with the rest of the update */
  wr(0x210UL,0x00080000UL|((((q->flags&VXO_ALPHA)?((q->key>>24)&0xFFUL):0xFFUL))<<8));
  /* hardware result (probes H1/H6): the blend only works with 0x298 bit 20 = 0 (V1 on top); the machine powers up with it 1 (V3 on top) and the alpha window must stay unset */
  if(q->flags&VXO_ALPHA) comp&=~0x00100000UL; else comp|=0x00100000UL;
 }
 if(!(q->flags&VXO_SRCKEY)&&(m&VXO_M_CKEY)) {wr(0x228UL,0UL); wr(0x22CUL,0UL);} /* the chroma key stays in force (0x228 bit 30) until it is cleared */
 vctl=1UL|0x00050000UL|0x80UL;
 if(set==2) vctl|=0x80000000UL;
 if(!hqv) {
  /* ---- the V1 unit alone: YUY2 or planar, enlargement only ---- */
  if(ov_hqv_on) ov_hqv_off();
  if(sw2<dw2) {zoom|=(((sw2*0x800UL/dw2)&0x7FFUL)<<16)|0x80000000UL; mini|=2UL;}
  if(sh2<dh2) {t=(int32_t)(sh2*0x400UL/dh2)-1; zoom|=((uint32_t)t&0x3FFUL)|0x8000UL; mini|=5UL;}
  cnt=(m&8UL)?sw2:sw2-1;
  if(planar) {
   uint32_t offy,offuv,c1,c2,hh=sh2;
   offy=((st&~3UL)*q->pitch+sl)&~31UL; offuv=st>0?(((((st&~3UL)>>1)*q->pitch+sl)&~31UL)>>1):(offy>>1);
   a0=q->offset+offy;
   c1=q->offset+q->pitch*(uint32_t)q->sh+offuv; c2=c1+q->pitch*((uint32_t)q->sh>>2);
   fetch=((sw2+31UL)&~31UL)>>4; if(fetch<4) fetch=4;
   vctl|=0x10UL;
   if(fm==VXO_FMT_YV12) {wr(0x28CUL,c2); wr(0x2F0UL,c1);} else {wr(0x28CUL,c1); wr(0x2F0UL,c2);} /* CB0 = U, CR0 = V; YV12 holds V first */
   wr(0x258UL,(sw2<=80UL)?(15UL):(15UL|(12UL<<24)|(8UL<<8)));
   (void)hh;
  } else {
   fetch=(((sw2<<1)+15UL)&~15UL)>>4; fetch+=1; if(fetch<4) fetch=4;
   rel=st*q->pitch+((sl<<1)&~15UL); a0=q->offset+rel;
   wr(0x258UL,31UL|(29UL<<24)|(16UL<<8));
  }
  if(bob) {vctl|=0x00400000UL; if(inter) {vctl|=0x01000000UL; sh2>>=1; if(!sh2) sh2=1;}}
  if(q->flags&VXO_SRCKEY) { /* source (chroma) key: no interpolation next to the keyed pixels */
   uint32_t lo=q->ckl&0xFFFFFFUL,hi=q->ckh&0xFFFFFFUL;
   wr(0x228UL,(lo|0x40000000UL)&~0x80000000UL); wr(0x22CUL,hi);
   mini&=~7UL;
   comp=(comp&~0x0FUL)|((q->flags&VXO_KEY)?3UL:2UL)|((q->flags&VXO_KEY)?0x20UL:0UL);
  }
  wr(0x238UL,a0); wr(0x248UL,a0); wr(0x254UL,a0|(ov_odd?0x80000000UL:0UL)); /* the V1 unit shows a new picture only when all three buffer addresses change */
  wr(0x23CUL,q->pitch|(q->pitch<<15));
  wr(0x244UL,((dr-1)<<16)|(db-1)); wr(0x240UL,(dl<<16)|dt);
  wr(0x234UL,fetch<<20);
  wr(0x26CUL,(sh2<<16)|cnt);
  wr(0x250UL,mini); wr(0x24CUL,zoom);
  wr(0x284UL,0x13000DEDUL); wr(0x288UL,0x13171000UL); /* YCbCr to RGB matrix with OpenChrome's defaults (hue 0, saturation/contrast 10000, brightness 5000) for this chip family: without it the V1 unit shows unconverted values */
  wr(0x230UL,vctl);
  wr(0x298UL,comp|0x80000000UL);
  (void)rd(0x298UL);
  ov_on=1;
  return 0;
 } else {
  /* ---- through the HQV scaler (conversion of RGB sources, shrinking): OpenChrome's Upd_Video for this chip family ---- */
  uint32_t n=(fm==VXO_FMT_RGB32)?2UL:1UL,hqvc,hf=0,scH=0,scV=0,zH=0,zV=0,tmp,dwx,sw1,d,dtop=0,offs=0,hqvoff=0,b0,b1,b2,sxr,sbr,i,first=!ov_hqv_on,shv=0;
  static const uint32_t filtH[5]={0x40UL,0x42UL,0x42UL,0x402UL,0x402UL};
  vctl|=0x02000000UL; hqvc=0x08000000UL|0x04000000UL|0x1UL; /* V1 takes the HQV output; HQV on, triple buffered, flip status */
  if(bob) {hqvc|=inter?0x60000UL:0x20000UL; if(ov_odd) hqvc|=0x20UL;} /* 0x3D0 bit 17 field to frame, bit 18 frame to field, bit 5 the field to play */
  if(planar) hqvc|=0xC0000000UL; /* HQV_YUV420 (NV12 here); the V1 unit then sees packed YUV */
  else if(fm==VXO_FMT_RGB32) {vctl|=0x4UL; hqvc|=0x00000000UL;}
  else if(fm==VXO_FMT_RGB16) {vctl|=0xCUL; hqvc|=0x20000000UL;}
  else if(fm==VXO_FMT_RGB15) {vctl|=0x8UL; hqvc|=0x30000000UL;}
  else hqvc|=0x80000000UL;
  if(sl||st) {
   uint32_t lo=(sl<<n)&~31UL; dtop=st&~3UL;
   offs=dtop*q->pitch+lo;
   hqvoff=(sh2>dh2?ov_muldiv(dtop,dh2,sh2):dtop)*q->pitch+(sw2>dw2?ov_muldiv(lo,dw2,sw2):lo);
   if(sw2>dw2) offs=ov_muldiv(offs,dw2,sw2);
  }
  if(planar) {offs=0; hqvoff=0; if(sl||st) hqvoff=(st&~3UL)*(q->pitch<<1)+((sl<<1)&~31UL);}
  fetch=(((((sw2>=dw2)?dw2:sw2)<<n)+15UL)&~15UL)>>4; fetch+=1; if(fetch<4) fetch=4;
  if(planar) ov_cofs=ov_nv12(q);
  /* horizontal */
  if(sw2==dw2) hf|=0x40UL;
  else if(sw2<dw2) {zoom=(((sw2*0x800UL/dw2)&0x7FFUL)<<16)|0x80000000UL; mini|=2UL; zH=1; hf|=0x40UL;}
  else {
   dwx=dw2;
   if(sw2>(dwx<<3)) {
    if(dwx<=32) dwx=33;
    tmp=(sw2>(dwx<<5))?(0x1000UL/31UL):((dwx*0x1000UL)/sw2);
    scH=0x30000000UL;
   } else if(sw2==(dwx<<3)) {tmp=((dwx-1)*0x1000UL)/sw2; scH=0x30000000UL;}
   else if(sw2>(dwx<<2)) {tmp=(sw2*0x1000UL)/dwx; scH=0x20000000UL;}
   else {tmp=(sw2*0x2000UL)/((dwx<<1)+1); scH=0x10000000UL;}
   scH|=(tmp&0x7FFFUL)|0x80000000UL;
   sw1=sw2;
   for(d=1;d<5;d++) {sw1>>=1; if(sw1<=dwx) break;}
   if(d==5) {ov_reason=8; return 1;} /* shrinking by more than 32 */
   hf|=filtH[d];
  }
  /* vertical: a field source (bob without interleaving) is doubled to a frame first */
  shv=(bob&&!inter)?sh2*2UL:sh2;
  if(shv==dh2) hf|=0x00420000UL;
  else if(shv<dh2) {t=(int32_t)(shv*0x400UL/dh2)-1; zoom|=((uint32_t)t&0x3FFUL)|0x8000UL; mini|=5UL; zV=1; hf|=0x00420000UL;}
  else {
   tmp=(shv*0x2000UL)/((dh2<<1)+1);
   if((tmp&0x1FFFFUL)==0) tmp=0x1FFFFUL;
   scV=(tmp&0x1FFFFUL)|0x80000000UL|0x10000000UL;
  }
  b0=vx2d_hqv_off; b1=b0+HQV_BUF; b2=b1+HQV_BUF;
  sxr=sr-1; sbr=sb-1;
  /* registers (the order of OpenChrome's buffer) */
  wr(0x258UL,31UL|(29UL<<24)|(16UL<<8));
  wr(0x254UL,b0+hqvoff); wr(0x238UL,b1+hqvoff); wr(0x248UL,b2+hqvoff);
  wr(0x1CCUL,offs);
  wr(0x3D4UL,q->offset); if(planar) wr(0x3D8UL,ov_cofs);
  wr(0x3ECUL,b0); wr(0x3F0UL,b1); wr(0x3FCUL,b2);
  if(planar) {wr(0x23CUL,q->pitch<<1); wr(0x3F8UL,(((q->pitch>>1)<<16)|q->pitch|0x10000UL)); wr(0x3F4UL,q->pitch<<1);}
  else {wr(0x23CUL,q->pitch); wr(0x3F8UL,q->pitch); wr(0x3F4UL,q->pitch);}
  wr(0x244UL,((dr-1)<<16)|(db-1)); wr(0x240UL,(dl<<16)|dt);
  wr(0x234UL,fetch<<20);
  wr(0x26CUL,(sh2<<16)|(sw2-1));
  if(zH||zV) {
   uint32_t mm=0,zz=0;
   if(zH) {mm=2UL; if(sh2>=1080UL) mm&=~2UL; zz=zoom&0xFFFF0000UL;}
   if(zV) {mm|=5UL; if(sw2>=1920UL) mm&=~1UL; zz|=zoom&0xFFFFUL; hf&=0xFFFDFFFFUL;}
   wr(0x250UL,mm); wr(0x24CUL,zz);
  } else {
   if(shv==dh2) hf&=0xFFFDFFFFUL;
   wr(0x250UL,0UL); wr(0x24CUL,0UL);
  }
  wr(0x3B0UL,scH); wr(0x3B4UL,scV); wr(0x3E4UL,hf);
  {extern unsigned long vx_chip_id;
  if(vx_chip_id==0x1122UL) {
   /* .583: the VX800's HQV map (VIA's VX800 manual, part II; OpenChrome's hqv_cme_regs and SetHQVFetch): the four source data offset
    * registers are at 0x380-0x38C (the VX855's destination crop there), there is no source crop at 0x370, no background (0x3CC) or
    * extended control (0x394), and 0x3E0 is the source's line count and bytes fetched a line (the VX855's subpicture scale, written 0
    * there; 0x3E8 does not exist). The scale registers 0x3B0 / 0x3B4 and everything else are the VX855's. */
   uint32_t fb=q->sw<<(planar?0UL:(fm==VXO_FMT_RGB32)?2UL:1UL);
   wr(0x380UL,0UL); wr(0x388UL,(sxr<<16)|sbr); wr(0x384UL,0UL); wr(0x38CUL,(sxr<<16)|sbr);
   wr(0x3E0UL,((fb-1)<<16)|(((bob&&!inter)?q->sh*2UL:q->sh)-1)); wr(0x3B8UL,0x0643212CUL);
  } else {
  wr(0x370UL,0UL); wr(0x378UL,(sxr<<16)|sbr); wr(0x374UL,0UL); wr(0x37CUL,(sxr<<16)|sbr);
  wr(0x380UL,0UL); wr(0x388UL,(sxr<<16)|sbr); wr(0x384UL,0UL); wr(0x38CUL,(sxr<<16)|sbr);
  wr(0x3CCUL,(sxr<<16)|sbr); wr(0x394UL,0UL); wr(0x3E0UL,0UL); wr(0x3E8UL,0UL); wr(0x3B8UL,0x0643212CUL);
  }}
  wr(0x284UL,0x13000DEDUL); wr(0x288UL,0x13171000UL);
  if(first) { /* first picture: wait until the HQV is idle, then start it and wait for the first flip */
   for(i=0;i<20000UL&&!(rd(0x3D0UL)&0x8UL);i++) ;
   wr(0x3D0UL,hqvc&~0x10UL); wr(0x3D0UL,hqvc|0x10UL);
   for(i=0;i<20000UL&&(rd(0x3D0UL)&0x8000UL);i++) ;
   wr(0x230UL,vctl); wr(0x298UL,comp|0x80000000UL);
  } else {
   wr(0x3D0UL,hqvc|0x1UL); wr(0x230UL,vctl); wr(0x298UL,comp|0x80000000UL);
  }
  (void)rd(0x298UL);
  ov_hqv_on=1; ov_on=1;
  return 0;
 }
}
void VX2D_ovl(const vx_ovl_req *q,uint32_t *r) {
 int rc=0;
 uint32_t i;
 vx_ovl_req req; /* v10.319 (audit V3): the request copied before the reply is cleared (the two may be one buffer) */
 if(q){req=*q;q=&req;}
 for(i=0;i<VXO_REPLY_DWORDS;i++) r[i]=0;
 vx_lock();
 complete_pending();
 ov_reason=0;
 if(!q||q->cb!=sizeof(*q)||q->abi!=VXO_ABI) {r[VXO_R_RESULT]=1; r[VXO_R_REASON]=99; vx_unlock(); return;}
 {extern int VX_chip_video(void); if(!VX_chip_video()) {r[VXO_R_RESULT]=q->cmd==VXO_QUERY?0:1; r[VXO_R_REASON]=98; vx_unlock(); return;}} /* .579: the VX900's video engine is not driven yet: the mask answered 0, so DirectDraw offers no overlay there (.583: the VX800's is) */
 switch(q->cmd) {
  case VXO_QUERY: rc=0; ov_reason=ov_last_refuse; break;
  case VXO_HIDE: VX2D_ovl_hide(); rc=0; break;
  case VXO_UPDATE:
   ov_last=*q; ov_odd=0; rc=ov_program(q);
   if(rc==1) {ov_refused++; ov_hw_hide(); ov_want=0;} else {ov_updates++; ov_want=1; if(rc==2) ov_hw_hide();}
   break;
  case VXO_FLIP:
   if(ov_last.cb!=sizeof(ov_last)) {ov_reason=11; rc=1; ov_refused++; break;}
   {vx_ovl_req t=ov_last; t.offset=q->offset;
    if(!ov_src_ok(&t,ov_planar(t.fmt))) {ov_reason=6; rc=1; ov_refused++; break;}} /* v10.288: a bad new source is refused; the current frame stays */
   ov_last.offset=q->offset;
   if(!ov_want) {rc=0; break;}
   if(ov_hqv_on&&hc_map()&&ov_hqv_on) { /* through the scaler: the HQV takes the new source and the V1 unit follows it */
    uint32_t n;
    if(ov_last.flags&VXO_BOB) ov_odd=(q->flags&VXO_FLD_ODD)?1:(q->flags&VXO_FLD_EVEN)?0:!ov_odd;
    for(n=0;n<50000UL&&(rd(0x3D0UL)&0x10UL);n++) ;
    wr(0x3D4UL,q->offset);
    if(ov_last.fmt==VXO_FMT_YV12||ov_last.fmt==VXO_FMT_I420) wr(0x3D8UL,ov_nv12(&ov_last));
    wr(0x3D0UL,(rd(0x3D0UL)&~0x20UL)|(ov_odd?0x20UL:0UL)|0x10UL|0x1UL);
    ov_flips++; break;
   }
   if(ov_last.flags&VXO_BOB) ov_odd=(q->flags&VXO_FLD_ODD)?1:(q->flags&VXO_FLD_EVEN)?0:!ov_odd;
   rc=ov_program(&ov_last);
   if(rc==1) {ov_refused++; ov_hw_hide(); ov_want=0;} else {ov_flips++; if(rc==2) ov_hw_hide();}
   break;
  case VXO_MOVE:
   if(ov_last.cb!=sizeof(ov_last)) {ov_reason=11; rc=1; ov_refused++; break;}
   /* v10.330 (review #152): the new place is worked out in a copy, within +-32768 (no overflow in the sums), and kept only when it is
    * accepted (or nothing is shown); a refused move hides the overlay as before but leaves the last accepted place */
   {vx_ovl_req t=ov_last;int32_t w=ov_last.dr-ov_last.dl,h=ov_last.db-ov_last.dt;
    if(q->dl<-32768L||q->dl>32768L||q->dt<-32768L||q->dt>32768L||w<0||h<0||w>32768L||h>32768L) {ov_reason=17; rc=1; ov_refused++; if(ov_want) {ov_hw_hide(); ov_want=0;} break;}
    t.dl=q->dl; t.dt=q->dt; t.dr=q->dl+w; t.db=q->dt+h;
    if(!ov_want) {ov_last=t; rc=0; break;}
    rc=ov_program(&t);
    if(rc==1) {ov_refused++; ov_hw_hide(); ov_want=0;} else {ov_last=t; ov_updates++; if(rc==2) ov_hw_hide();}}
   break;
  case VXO_REGW: case VXO_REGR:
   if(!(vx2d_ovl&VXO_M_PROBE)||!hc_map()||q->offset<0x200UL||q->offset>0x3FCUL||(q->offset&3UL)) {ov_reason=16; rc=1; break;}
   if(q->cmd==VXO_REGW) {
    wr(q->offset,q->key);
    if(q->flags&VXO_FIRE) {uint32_t c; (void)ov_wait(); c=rd(0x298UL)&~0xC0000000UL; wr(0x298UL,c|0x80000000UL); (void)rd(0x298UL); (void)ov_wait();}
   }
   ov_probe_val=rd(q->offset); rc=0; break;
  default: ov_reason=98; rc=1; ov_refused++; break;
 }
 if(rc==1&&q->cmd!=VXO_QUERY&&ov_reason) ov_last_refuse=ov_reason;
 r[VXO_R_RESULT]=(uint32_t)rc; r[VXO_R_REASON]=ov_reason; r[VXO_R_ON]=ov_on; r[VXO_R_MASK]=(uint32_t)vx2d_ovl;
 r[VXO_R_UPDATES]=ov_updates; r[VXO_R_FLIPS]=ov_flips; r[VXO_R_REFUSED]=ov_refused; r[VXO_R_HIDES]=ov_hides; r[VXO_R_HQV]=ov_hqv_on?1UL:0UL;
 if(hc_map()) {r[VXO_R_WIN]=rd(0x240UL); r[VXO_R_BUF1]=rd(0x238UL); r[VXO_R_V1CTL]=rd(0x230UL); r[VXO_R_COMPOSE]=rd(0x298UL); r[VXO_R_START]=rd(0x254UL); r[VXO_R_SRCH]=rd(0x26CUL); r[VXO_R_HQVCTL]=rd(0x3D0UL);}
 if(q&&(q->cmd==VXO_REGW||q->cmd==VXO_REGR)) r[VXO_R_UPDATES]=ov_probe_val;
 vx_unlock();
}
/* v10.359: mov eax,cr4 and CPUID function 1 (control 0xFFFFFE34) */
static uint32_t vx_cr4(void);
#pragma aux vx_cr4 = 0x0F 0x20 0xE0 value [eax] modify exact [eax];
static void vx_cpuid1_raw(uint32_t *edx_out,uint32_t *ecx_out);
#pragma aux vx_cpuid1_raw = "push ebx" "push esi" "push edi" "mov esi,eax" "mov edi,edx" "mov eax,1" 0x0F 0xA2 "mov [esi],edx" "mov [edi],ecx" "pop edi" "pop esi" "pop ebx" parm [eax] [edx] modify exact [eax ecx edx];
static void vx_cpuid1(uint32_t *d,uint32_t *c) {*d=0;*c=0;vx_cpuid1_raw(d,c);}
static void control_body(uint32_t value,uint32_t *out);
void VX2D_control(uint32_t value,uint32_t *out) {
 /* v10.424: arming a present is one word: not through control_body, whose complete_pending would wait for the copy the arm is for */
 if((value&~1UL)==0xFFFFFE20UL) {vx2d_present_arm=value&1UL; out[0]=present_async_ops; out[1]=vx2d_present_arm; out[2]=vx2d_async_present; out[3]=vx2d_async; return;}
 control_body(value,out); vx2d_publish();} /* v10.142 */
static void control_body(uint32_t value,uint32_t *out) {
 /* .530: the live switches below (E8/E9, EB-ED, EF, F0-F4, F6-F9, FA-FD, FFFFFE22/23, FFFFFE30/31, VX2D_REGISTRY, the low bits 0-7) are
  * test hooks for the 2D tools, kept for development: DELETE THEM WHEN DEVELOPMENT IS FINISHED */
 int i2;
 vx_lock(); gdi_map(); /* v10.273 */
 complete_pending();
 /* .589 (VX800 board, VXGL v5.0: a front buffer filled from the window and then cleared kept the window's picture once or twice in 30
  * tries; Microsoft's renderer read the screen before a fill had landed): how soon after a command is written the status register
  * shows the engine busy. 0xFFFFFE40 runs 200 copies of 256 x 256 inside the staging block (nothing on the screen) and counts the
  * status reads until the first busy one: out[0] commands run (0xFFFFFFFF not run), out[1] never seen busy in 4000 reads, out[2] the
  * most reads before busy, out[3] the most reads from busy to idle. 0xFFFFFE41..43 read the last run's histogram: reads before busy
  * 0, 1, 2-3, 4-7 | 8-15, 16-31, 32-63, 64-127 | 128 and more, the fewest reads from busy to idle, the bytes a pixel, 0. The wait after
  * every command (idle_n) takes 32 idle reads in a row as "finished": a first busy read later than that is a wait that ends early. */
 {static uint32_t ip_h[9],ip_min,ip_bpp;
  if(value==0xFFFFFE40UL) {
   uint32_t n,i,never=0,maxfirst=0,maxdone=0,bpp=hda?hda->bpp:0,P=1024UL; int rc2;
   out[0]=0xFFFFFFFFUL; out[1]=out[2]=out[3]=0;
   for(i=0;i<9;i++) ip_h[i]=0; ip_min=0xFFFFFFFFUL; ip_bpp=bpp>>3;
   if(!vx2d_enable||!vx2d_stage_off||!desktop_x(0)||(bpp!=16&&bpp!=32)||(vx3dn_rt_why=2,VX3D_busy())||vx_native_display_hold) {vx_unlock(); return;}
   rc2=xf_open(); if(rc2!=VX2D_DONE) {vx_unlock(); return;}
   for(n=0;n<200;n++) {
    uint32_t first=0,done=0,s; int seen=0;
    wr(4,xf_depth(bpp)); wr(8,((P>>3)<<16)|(P>>3)); wr(0x14,vx2d_stage_off>>3); wr(0x1C,vx2d_stage_off>>3); wr(0x30,0); wr(0x48,0); wr(0x20,0);
    wr(0x58,0); wr(0x5C,0); wr(0x18,0); wr(12,(255UL<<16)|255UL); wr(16,(256UL<<16)|0UL); /* the registers the staged copy sets */
    flush(); (void)rd(0x400); wr(0,(0xCCUL<<24)|1UL);
    for(i=0;i<20000UL;i++) {
     s=rd(0x400);
     if(!seen) {if(!idle_bits(s)) {seen=1; first=i;} else if(i>=4000UL) break;}
     else if(idle_bits(s)) {done=i-first; break;}
    }
    if(!seen) never++;
    else {uint32_t b=first==0?0:first==1?1:first<4?2:first<8?3:first<16?4:first<32?5:first<64?6:first<128?7:8; ip_h[b]++; if(first>maxfirst) maxfirst=first; if(done>maxdone) maxdone=done; if(done<ip_min) ip_min=done;}
    if(!idle()) {xf_timeout(); break;}
   }
   xf_close();
   out[0]=n; out[1]=never; out[2]=maxfirst; out[3]=maxdone; vx_unlock(); return;
  }
  /* .592 (VX800 board, .591's trace: in a wrong try the ICD's steps are those of a right one: the window is copied into the front, the
   * CPU clears it, and it reads back as the window's picture: the copy ran after it was taken as finished). 0xFFFFFE45: one copy as
   * above, in whatever state the engines are in when it is asked (after a swap, with XP's ring paused): out[0] status reads before
   * the 2D engine's own busy bit (2) shows, 0xFFFFFFFF never in 60000 reads; out[1] reads from then until that bit clears; out[2] the
   * status before the command | reads before the status first passes the driver's idle test << 16 (0 = it passed at the first read,
   * which is what ends the driver's wait early if the engine has not started); out[3] the first status after the command. */
  if(value==0xFFFFFE45UL) {
   uint32_t i,bpp=hda?hda->bpp:0,P=1024UL,s,s0,s1=0,first=0xFFFFFFFFUL,busy=0,pass=0xFFFFUL; int seen=0,passed=0;
   out[0]=0xFFFFFFFEUL; out[1]=out[2]=out[3]=0;
   if(!vx2d_enable||!vx2d_stage_off||!desktop_x(0)||(bpp!=16&&bpp!=32)||vx_native_display_hold) {vx_unlock(); return;}
   if(xf_open()!=VX2D_DONE) {vx_unlock(); return;}
   wr(4,xf_depth(bpp)); wr(8,((P>>3)<<16)|(P>>3)); wr(0x14,vx2d_stage_off>>3); wr(0x1C,vx2d_stage_off>>3); wr(0x30,0); wr(0x48,0); wr(0x20,0);
   wr(0x58,0); wr(0x5C,0); wr(0x18,0); wr(12,(255UL<<16)|255UL); wr(16,(256UL<<16)|0UL);
   flush(); s0=rd(0x400); wr(0,(0xCCUL<<24)|1UL);
   for(i=0;i<60000UL;i++) {
    s=rd(0x400); if(!i) s1=s;
    if(!passed&&idle_bits(s)) {passed=1; pass=i>0xFFFEUL?0xFFFEUL:i;}
    if(!seen) {if(s&2UL) {seen=1; first=i;}}
    else if(!(s&2UL)) {busy=i-first; break;}
   }
   if(!idle()) xf_timeout();
   xf_close();
   out[0]=first; out[1]=busy; out[2]=(s0&0xFFFFUL)|(pass<<16); out[3]=s1; vx_unlock(); return;
  }
  if(value==0xFFFFFE47UL) {out[0]=start_waits; out[1]=start_unseen; out[2]=start_late; out[3]=stats.timeouts; vx_unlock(); return;}
  if(value==0xFFFFFE46UL) {out[0]=started_waits; out[1]=started_unseen; out[2]=vx3dn_xr_cr_pause; out[3]=async_waits; vx_unlock(); return;
  }
  if(value>=0xFFFFFE41UL&&value<=0xFFFFFE43UL) {
   if(value==0xFFFFFE41UL) {out[0]=ip_h[0]; out[1]=ip_h[1]; out[2]=ip_h[2]; out[3]=ip_h[3];}
   else if(value==0xFFFFFE42UL) {out[0]=ip_h[4]; out[1]=ip_h[5]; out[2]=ip_h[6]; out[3]=ip_h[7];}
   else {out[0]=ip_h[8]; out[1]=ip_min; out[2]=ip_bpp; out[3]=0;}
   vx_unlock(); return;
  }}
 /* v10.58zc: value 0xFFFFFE00+k (k 0..7) reads phase k of the native-op cost profile: out[0] ops timed, out[1] sum of cycles/16. */
 if((value&0xFFFFFFF0UL)==0xFFFFFE00UL) {
  out[0]=ph_n; out[1]=(value&15)<8?ph_sum[value&15]:0; out[2]=m2d_state; out[3]=flags(); vx_unlock(); return;
 }
 /* v10.73: value 0xFFFFFE10 reads the GDI binding: out[0] bit 0 bound to the DRV's flag words, bit 1 the enabled word reads 0, bit 2 the DRV's BUSY flag is set;
  * out[1] GDI requests that arrived while the enabled word read 0 (such requests only arrive when something else asked; the DRV itself skips before calling). */
 /* v10.273: 0xFFFFFE1C reads the binding's mapping: out[0] times the PDEVICE or the DRV's data segment had moved in linear memory, out[1] mappings, out[2] failed ones, out[3] the enabled word's address now */
 if(value==0xFFFFFE1CUL) {out[0]=gdi_moves; out[1]=gdi_maps; out[2]=gdi_mapfail; out[3]=(uint32_t)gdi_enabled; vx_unlock(); return;}
 if(value==0xFFFFFE10UL) {
  out[0]=(gdi_flags&&gdi_enabled?1:0)|((gdi_enabled&&!*gdi_enabled)?2:0)|((gdi_flags&&(*gdi_flags&0x0010))?4:0);
  out[1]=gdi_off_seen; out[2]=m2d_state; out[3]=flags(); vx_unlock(); return;
 }
 /* v10.75: value 0xFFFFFE11 reads the recovery counters: out[0] lost-engine recoveries, out[1] switched-off engine retried after a mode set, out[2] state, out[3] 1 when the GDI fences are set. */
 if(value==0xFFFFFE11UL) {
  out[0]=recoveries; out[1]=retries; out[2]=m2d_state; out[3]=lost_fenced?1:0; vx_unlock(); return;
 }
 /* v10.95: 0xFFFFFE12 reads the async counters: out[0] commands started asynchronously, out[1] completions that had to wait for one, out[2] 1 while one is pending, out[3] bit 0 async on, bit 1 exclusive-mode gate.
  * 0xFC000000 | v sets the live async switch (bit 0); 0xFD000000 | v sets the gate (bit 0; it records the exclusive mode whether async is on or not: a blit goes async only when both are on, so the switch can be turned on later); the HAL uses the second one when a DirectDraw exclusive mode begins and ends. */
 /* v10.99: 0xFFFFFE13 reads the stretch counters: out[0] stretches drawn, out[1] stretches handed back to DIBENG, out[2] destination pixels drawn, out[3] the verified-class mask; 0xFB000000 | v (5 bits) sets the mask for a test. */
 /* v10.507: 0xFFFFFE26 reads the GDI options: out[0] the live bits (vx2d_gdi), out[1] ACCEL_CPULINE_MAX | ACCEL_CPULINE_MAXY << 16, out[2] 1:1 DIB rows converted straight into the screen,
  * out[3] software cursor shows put off while the driver drew; 0xE9000000 | bits sets the bits, 0xE8000000 | x | y << 12 the two line lengths for a test (0xFFFFFFFF: the registry values). */
 if(value==0xFFFFFE26UL||(value&0xFE000000UL)==0xE8000000UL) {
  extern DWORD VESA_cursor_defers(void);
  if((value&0xFF000000UL)==0xE9000000UL) vx2d_gdi=value&3UL;
  if((value&0xFF000000UL)==0xE8000000UL) {vx2d_linemax=value&4095UL; vx2d_linemaxy=(value>>12)&4095UL;}
  out[0]=vx2d_gdi; out[1]=vx2d_linemax|(vx2d_linemaxy<<16); out[2]=str_rows_direct; out[3]=VESA_cursor_defers(); vx_unlock(); return;
 }
 if(value==0xFFFFFE13UL) {out[0]=str_ops; out[1]=str_skips; out[2]=str_pixels; out[3]=vx2d_str_mask; vx_unlock(); return;}
 if((value&0xFF000000UL)==0xFB000000UL) {vx2d_str_mask=value&0xFFFFFFUL; out[0]=str_ops; out[1]=str_skips; out[2]=str_pixels; out[3]=vx2d_str_mask; vx_unlock(); return;}
 /* v10.104: 0xFFFFFE14 reads the 1-bit/4-bit/RLE counters: out[0] RLE bitmaps decoded, out[1] RLE streams that did not cover every pixel exactly once (left to DIBENG), out[2] RLE pixels decoded, out[3] the class mask 2; 0xFA000000 | v (24 bits) sets that mask for a test. */
 /* v10.105: 0xFFFFFE15 reads the hardware cursor counters: out[0] the live mode (0 off, 1 or 2 register set), out[1] times it was switched on, out[2] shapes left to the software cursor, out[3] the image block's byte offset (0 = none); out[0..3] again after 0xF9000000 | mode sets the live mode (a test). out of 0xFFFFFE16: moves, shape loads, hides, 1 while shown. */
 /* v10.106: 0xFFFFFE17 reads the overlay counters: out[0] the live mask, out[1] updates, out[2] refused, out[3] 1 while shown; 0xF8000000 | mask sets the live mask (a test; the overlay is hidden first). */
 if(value==0xFFFFFE17UL||(value&0xFF000000UL)==0xF8000000UL) {
  if((value&0xFF000000UL)==0xF8000000UL) {VX2D_ovl_hide(); vx2d_ovl=value&8191UL; ov_last.cb=0;}
  out[0]=(uint32_t)vx2d_ovl; out[1]=ov_updates; out[2]=ov_refused; out[3]=ov_on; vx_unlock(); return;
 }
/* v10.325: 0xFFFFFE1D the cursor against what the driver wrote: out[0] bit 0 an image kept, bit 1 shape ready, bit 2 shown, bit 3 mono
  * cursor, bit 4 a mode set since the image was written, bits 9:8 the live mode; out[1] image dwords that differ, out[2] the first that
  * differs (FFFFFFFF none), out[3] bit i: register i reads other than written, bits 31:24 registers kept. 0xFFFFFE1E: out[0] the engine
  * reset count, out[1] shapes written, out[2] the image's byte offset, out[3] its dwords. 0xFFFFFE70 + i: register i (offset, written,
  * live). 0xFFFFFE60 + k: the display registers 0x200 + 16k .. + 12 as they read now.
  * v10.326 (review): out[0] bit 5 the image was compared (out[1] counts only then), bit 6 the registers could not be read (no mapping:
  * 70 + i and 60 + k then hold no readings), bit 7 the engine is marked lost; the readings go through the mapping already made even
  * when the engine is marked lost (hc_map refuses then, and the readings were zeros and FFFFFFFF that looked like changed registers). */
 if(value==0xFFFFFE1DUL) { /* .530: the cursor's state (the image and register check removed) */
  out[0]=(hc_ready?2UL:0)|(hc_on?4UL:0)|((vx2d_hwcur&3UL)<<8)|(native_fault?128UL:0); out[1]=0; out[2]=0xFFFFFFFFUL; out[3]=0; vx_unlock(); return;
 }
 /* v10.329: 0xFFFFFE1F turns the check on until restart (the shadow starts empty: the cursor is programmed again at its next show, as after
  * an engine reset); then answers as 0xFFFFFE1D */
 /* v10.358: ACCEL_3D_GUARD live (VXGAME's guard configuration): 0xFFFFFE30 off, 0xFFFFFE31 on, 0xFFFFFE32 read. The HAL takes the word at
  * DriverInit and VXGL.DLL when a process loads it: programs started after the change see it */
 if((value&0xFFFFFFFCUL)==0xFFFFFE30UL&&(value&3UL)<3UL) {extern uint32_t vx3dn_guard; if((value&3UL)<2UL){vx3dn_guard=value&1UL; if(vx3dn_guard) vx_d3d_feat_cfg|=0x800000UL; else vx_d3d_feat_cfg&=~0x800000UL;} out[0]=vx3dn_guard; out[1]=0x47524431UL; vx_unlock(); return;}
 /* v10.359: 0xFFFFFE34 reports the processor's streaming SIMD state for VXGL.DLL's SSE transform: out[0] CR4 (bit 9 OSFXSR: the system saves
  * the XMM registers at a task switch), out[1] 'SSE1', out[2]/out[3] CPUID 1 EDX/ECX. CR4 can be read only at ring 0, so the VxD answers */
 /* v10.363: 0xFFFFFE36 the display FIFO's registers as the mode set left them: out[0] SR16 | SR17 << 8 | SR18 << 16 | SR22 << 24, out[1] CR68 |
  * CR94 << 8 | CR95 << 16 | SR21 << 24, out[2] 'FIFO', out[3] ACCEL_3D_ZFIX | draws that sent it << 8 */
 /* v10.364: the display FIFO live (VXFIFO.EXE), no mode set: 0xFFFFFE37 writes openchrome's VX855 values (SR17 C7: 400 entries; SR16, SR18
  * 90: thresholds 320, bit 6 kept; SR22 28: expire 160, bits 7:5 kept), the values found first kept; 0xFFFFFE38 puts those back. Then
  * answers as 0xFFFFFE36. Board, v10.363: the BIOS leaves SR16 20 SR17 7F SR18 60 SR22 10 (slanted rows under heavy 2D fills). */
 if(value==0xFFFFFE36UL) {unsigned char si=inp(0x3C4),s10,ci=inp(0x3D4);
  s10=sq_get(0x10);outp(0x3C5,(unsigned char)(s10|1));
  out[0]=(uint32_t)sq_get(0x16)|(uint32_t)sq_get(0x17)<<8|(uint32_t)sq_get(0x18)<<16|(uint32_t)sq_get(0x22)<<24;
  out[1]=(uint32_t)sq_get(0x21)<<24;
  outp(0x3D4,0x68);out[1]|=inp(0x3D5);outp(0x3D4,0x94);out[1]|=(uint32_t)inp(0x3D5)<<8;outp(0x3D4,0x95);out[1]|=(uint32_t)inp(0x3D5)<<16;outp(0x3D4,ci);
  outp(0x3C4,0x10);outp(0x3C5,s10);outp(0x3C4,si);
  out[2]=0x4F464946UL;out[3]=0;vx_unlock();return;}
 if(value==0xFFFFFE34UL) {out[0]=vx_cr4(); out[1]=0x31455353UL; vx_cpuid1(&out[2],&out[3]); vx_unlock(); return;}
 if(value==0xFFFFFE1EUL) {out[0]=vx2d_reset_epoch; out[1]=hc_sets; out[2]=0; out[3]=0; vx_unlock(); return;}
 /* v10.332: read-only dump for VXSTATE: 0xFFFFFE80 + k (k 0..63) the MMIO dwords 0x000 + 16k .. + 12 (the 2D engine, the display and video
  * units up to 0x3FC); 0xFFFFFEC0 + k (k 0..63) 0x1200 + 16k .. + 12. Nothing is written; 0x400 and up (the 3D ports) are not read */
 if((value&0xFFFFFFC0UL)==0xFFFFFE80UL||(value&0xFFFFFFC0UL)==0xFFFFFEC0UL) {uint32_t k=value&63UL,b=((value&0xFFFFFFC0UL)==0xFFFFFE80UL?0UL:0x1200UL)+k*16UL,i; for(i=0;i<4;i++) out[i]=hc_rmap()?rd(b+i*4UL):0xFFFFFFFFUL; vx_unlock(); return;}
 if((value&0xFFFFFFF0UL)==0xFFFFFE60UL) {uint32_t k=value&15UL,i; for(i=0;i<4;i++) out[i]=hc_rmap()?rd(0x200UL+k*16UL+i*4UL):0xFFFFFFFFUL; vx_unlock(); return;}
 if(value==0xFFFFFE16UL) {out[0]=hc_moves; out[1]=hc_sets; out[2]=hc_hides; out[3]=hc_on; vx_unlock(); return;}
 if(value==0xFFFFFE15UL||(value&0xFF000000UL)==0xF9000000UL) {
  if((value&0xFF000000UL)==0xF9000000UL) {if(hc_on) VX2D_hwcur_hide(); vx2d_hwcur=value&3UL; if(vx2d_hwcur>2) vx2d_hwcur=0; hc_ready=0;}
  out[0]=vx2d_hwcur; out[1]=hc_shows; out[2]=hc_declined; out[3]=vx2d_hwcur_off; vx_unlock(); return;
 }
 if(value==0xFFFFFE14UL) {out[0]=rle_ops; out[1]=rle_declined; out[2]=rle_pixels; out[3]=vx2d_str_mask2; vx_unlock(); return;}
 if((value&0xFF000000UL)==0xF4000000UL) {vx2d_prof=value&1UL; if(vx2d_prof) {for(i2=0;i2<8;i2++) ph_sum[i2]=0; ph_n=0;} out[0]=vx2d_prof; out[1]=ph_n; out[2]=0; out[3]=0; vx_unlock(); return;} /* v10.132: phase profile on/off */
 if((value&0xFF000000UL)==0xF3000000UL) {vx2d_str_fuse=value&1UL; out[0]=vx2d_str_fuse; out[1]=str_fused_rows; out[2]=str_ops; out[3]=0; vx_unlock(); return;} /* v10.132: fused 1-bit expansion */
 if((value&0xFF000000UL)==0xEB000000UL) {vx2d_keep=value&1UL; if(!vx2d_keep) k_release(); out[0]=vx2d_keep; out[1]=keep_opens; out[2]=keep_releases; out[3]=keep.open; vx_unlock(); return;} /* v10.144: kept engine context */
 if(value==0xFFFFFE50UL) {extern volatile unsigned long vx_accel_d3d; out[0]=vx_accel_d3d; out[1]=0x56584433UL; {extern volatile unsigned long vx_heap_align; out[2]=0x100UL|(vx_heap_align&1);} {extern volatile unsigned long vx_d3d_log; extern volatile unsigned long vx3dn_ckey; out[2]|=0x400UL|((vx_d3d_log&1)<<1)|((vx3dn_ckey&1)<<2)|(((vx_d3d_log>>1)&1)<<13);{extern volatile unsigned long vx3dn_delta; /* .524: ACCEL_D3D_DELTA, bit 20 */ out[2]|=0x800UL|((vx_d3d_rttex&1)<<3)|((vx_d3d_mipcap&1)<<4)|((vx_d3d_texhint&1)<<5)|((vx_d3d_indexed&1)<<7)|((vx_d3d_rtnowait&1)<<12)|((vx_d3d_ixpass&1)<<16)|((vx_d3d_infocache&1)<<17)|((vx_d3d_v8&1)<<19)|((vx3dn_delta&1)<<20)|((vx_d3d_tnl&1)<<21);}} /* v10.179: bit 8 = field valid, bit 0 = ACCEL_HEAP_ALIGN; v10.202: bit 10 valid, bit 1 = ACCEL_D3D_LOG; v10.208: bit 2 = ACCEL_D3D_CKEY */ {extern volatile unsigned long vx_d3d_feat_cfg; out[3]=0x80000000UL|(vx_d3d_feat_cfg&0x7FFFFFFFUL);} /* v10.212: bit 31 valid, the ACCEL_D3D_* feature switches */ vx_unlock(); return;} /* v10.149: registry ACCEL_D3D for the HAL's DriverInit */
 if(value==0xFFFFFE3FUL) {out[0]=keep_opens; out[1]=keep_releases; out[2]=keep.open; out[3]=vx2d_keep; vx_unlock(); return;}
 if((value&0xFFFFFFE0UL)==0xFFFFFE40UL&&(value&31UL)<19UL) {uint32_t k=value&31UL; out[0]=m2d_linear?rd(saved_regs[k]):0xFFFFFFFFUL; out[1]=saved_regs[k]; out[2]=keep.saved[k]; out[3]=keep.open; vx_unlock(); return;} /* v10.144: register k of the 19 now, and in the kept context */
 if((value&0xFF000000UL)==0xEC000000UL) {vx2d_prefilter=value&1UL; out[0]=vx2d_prefilter; out[1]=vx2d_ops; out[2]=vx2d_fallbacks; out[3]=0; vx_unlock(); return;} /* v10.142: DRV size pre-check */
 if((value&0xFF000000UL)==0xED000000UL) {vx2d_fill_tall=value&0xFFFFUL; out[0]=vx2d_fill_tall; out[1]=vx2d_ops; out[2]=vx2d_fallbacks; out[3]=0; vx_unlock(); return;} /* v10.142: tall-fill rows */
 if((value&0xFF000000UL)==0xEF000000UL) {vx2d_col8=value&1UL; out[0]=vx2d_col8; out[1]=vx2d_ops; out[2]=vx2d_fallbacks; out[3]=0; vx_unlock(); return;} /* v10.135 */
 if((value&0xFF000000UL)==0xF1000000UL) {vx2d_op_wait=value&1UL; out[0]=vx2d_op_wait; out[1]=vx2d_ops; out[2]=vx2d_fallbacks; out[3]=0; vx_unlock(); return;} /* v10.133 */
 if((value&0xFF000000UL)==0xF0000000UL) {vx2d_dread=value&1UL; out[0]=vx2d_dread; out[1]=vx2d_ops; out[2]=vx2d_fallbacks; out[3]=0; vx_unlock(); return;} /* v10.133 */
 if((value&0xFF000000UL)==0xF2000000UL) {vx2d_span_wait=(value&7UL); out[0]=vx2d_span_wait; out[1]=xf_ops[4]; out[2]=xf_skips; out[3]=xf_why; vx_unlock(); return;} /* v10.132: wait between span rectangles */
 if((value&0xFF000000UL)==0xFA000000UL) {vx2d_str_mask2=value&0xFFFFFFUL; out[0]=rle_ops; out[1]=rle_declined; out[2]=rle_pixels; out[3]=vx2d_str_mask2; vx_unlock(); return;}
 if(value==0xFFFFFE22UL||value==0xFFFFFE23UL) vx2d_async_present=value&1UL; /* v10.424: live switch */
 if(value>=0xFFFFFE22UL&&value<=0xFFFFFE24UL) {out[0]=present_async_ops; out[1]=vx2d_present_arm; out[2]=vx2d_async_present; out[3]=vx2d_async; vx_unlock(); return;}
 if(value==0xFFFFFE12UL) {out[0]=async_ops; out[1]=async_waits; out[2]=pend.active; out[3]=(vx2d_async?1:0)|(vx2d_async_gate?2:0); vx_unlock(); return;}
 if((value&0xFF000000UL)==0xFC000000UL) {vx2d_async=value&1; out[0]=async_ops; out[1]=async_waits; out[2]=m2d_state; out[3]=flags(); vx_unlock(); return;}
 if((value&0xFF000000UL)==0xFD000000UL) {vx2d_async_gate=value&1; out[0]=async_ops; out[1]=async_waits; out[2]=m2d_state; out[3]=flags(); vx_unlock(); return;}
 /* v10.92: 0xFE000000 | k << 20 | v (20 bits) sets styled-line parameter k (0 on/off, 1 opaque command bits, 2 transparent command bits, 3 format) for a test; VX2D_REGISTRY restores the registry values. */
 /* v10.116: 0xFFFFFE18 reads the system-memory drawing counters: out[0] XF_MONO, out[1] XF_STAGE, out[2] XF_TEXT, out[3] spans; 0xFFFFFE19: out[0] calls handed back, out[1] the last reason, out[2] the live mask, out[3] the staging block's offset;
  * 0xF7000000 | mask sets the live mask (ACCEL_HOST bits) and the DRV's copy of it. */
 if(value==0xFFFFFE1BUL) {out[0]=ss_saves; out[1]=ss_restores; out[2]=ss_refused; out[3]=vx2d_save_off; vx_unlock(); return;} /* v10.124 SaveScreenBitmap counters */
 if(value==0xFFFFFE1AUL||(value&0xFF000000UL)==0xF6000000UL) { /* v10.123 gamma */
  if((value&0xFF000000UL)==0xF6000000UL) {vx2d_gamma=value&1UL; VX2D_gamma_load(NULL);}
  out[0]=gm_loads; out[1]=gm_cr33; out[2]=vx2d_gamma; out[3]=gm_r128; vx_unlock(); return;
 }
 if(value==0xFFFFFE18UL) {out[0]=xf_ops[1]; out[1]=xf_ops[2]; out[2]=xf_ops[3]; out[3]=xf_ops[4]; vx_unlock(); return;}
 if(value==0xFFFFFE19UL||(value&0xFF000000UL)==0xF7000000UL) {
  if((value&0xFF000000UL)==0xF7000000UL) {vx2d_host=value&63UL; xf_mask_out();}
  out[0]=xf_skips; out[1]=xf_why; out[2]=vx2d_host; out[3]=vx2d_stage_off; vx_unlock(); return;
 }
 if(value==VX2D_REGISTRY) { vx2d_gdi=vx2d_gdi_cfg; vx2d_linemax=vx2d_linemax_cfg; vx2d_linemaxy=vx2d_linemaxy_cfg; vx2d_keep=vx2d_keep_cfg; if(!vx2d_keep) k_release(); vx2d_fill_tall=vx2d_fill_tall_cfg; vx2d_prefilter=vx2d_prefilter_cfg; vx2d_col8=vx2d_col8_cfg; vx2d_op_wait=vx2d_op_wait_cfg; vx2d_dread=vx2d_dread_cfg; vx2d_span_wait=vx2d_span_wait_cfg; vx2d_str_fuse=vx2d_str_fuse_cfg; vx2d_prof=0; vx2d_host=vx2d_host_cfg; xf_mask_out(); if(vx2d_gamma!=vx2d_gamma_cfg) {vx2d_gamma=vx2d_gamma_cfg; VX2D_gamma_load(NULL);}if(vx2d_ovl!=vx2d_ovl_cfg) {VX2D_ovl_hide(); ov_last.cb=0; vx2d_ovl=vx2d_ovl_cfg;} vx2d_str_mask=vx2d_str_cfg; vx2d_str_mask2=vx2d_str_cfg2; if(vx2d_hwcur!=vx2d_hwcur_cfg) {if(hc_on) VX2D_hwcur_hide(); hc_ready=0; vx2d_hwcur=vx2d_hwcur_cfg;} vx2d_async=vx2d_async_cfg; vx2d_async_gate=0; vx2d_async_present=vx2d_async_present_cfg; vx2d_present_arm=0; vx2d_copy_all=vx2d_copy_all_cfg; vx2d_enable=vx2d_enable_cfg; vx2d_force=0;}
 else if(value!=VX2D_QUERY&&(value&~7UL)==0) {vx2d_copy_all=(value&1)!=0; {extern int VX_chip_accel(void);vx2d_enable=(value&2)==0&&VX_chip_accel();} vx2d_force=(value&4)!=0;} /* bit 2: ignore size thresholds (tests) */
 out[0]=vx2d_ops; out[1]=vx2d_fallbacks; out[2]=m2d_state; out[3]=flags(); vx_unlock();
}
void VX2D_stats(vx2d_stats *out) {
 vx_lock(); stats.cb=sizeof(stats); stats.abi=VX2D_ABI;
 stats.state=m2d_state; stats.flags=flags(); stats.ops=vx2d_ops; stats.fallbacks=vx2d_fallbacks;
 memcpy(out,&stats,sizeof(stats)); vx_unlock();
}

/* v10.229: why CPU access was refused: [0] m2d_state, [1] native_fault, [2] answers VX2D_LOST, [3] generation at the loss, [4] mode generation,
 * [5] lost_recover attempts that failed (TEXDBG 0xA5) */
void VX2D_loss_info(uint32_t *o) {o[0]=(uint32_t)m2d_state;o[1]=(uint32_t)native_fault;o[2]=access_lost;o[3]=lost_gen;o[4]=vx_status.set_successes;o[5]=recover_failed;}
int VX2D_access(uint32_t action) {
 int rc=VX2D_DONE;
 VX3DN_ev(15,action);
 vx_lock(); gdi_map(); /* v10.273 */
 /* v10.231: CPU access (Lock, Unlock, software blits) waits for a 3D draw in flight, whatever the mode. v10.285 (review): the status
  * query (action 3, GetBltStatus) does not: a draw still running answers BUSY at once (VX3DN_poll: one status read, the retire only
  * when the engine is already idle, or after 100 ms), without waiting, resetting or starting the 3D state cache over. */
 if(action==3) {if(VX3DN_poll()) {vx_unlock(); return VX2D_BUSY;}}
 else {vx3dn_rt_why=3;(void)VX3DN_busy();}
 /* v10.142 (review O16, known bug P02): action 3 = status only. A pending asynchronous blit that the engine is still running answers VX2D_BUSY at once
  * (GetBltStatus then says DDERR_WASSTILLDRAWING) instead of waiting; when the engine is idle, or after 100 ms, the blit is completed as before. */
 {const int pend_was=pend.active!=0;
 if(action==3&&pend.active&&!completing&&m2d_state!=3) {
  uint32_t s=rd(0x400);
  if(s!=0xFFFFFFFFUL&&!idle_bits(s)&&(uint32_t)(Get_System_Time()-pend.t0)<100UL) {vx_unlock(); return VX2D_BUSY;} /* v10.465 (review): idle_bits (the 3D ring's paused fetcher reads idle), as the waits */
 }
 complete_pending();
 if(m2d_state==3&&!lost_recover(0)) {rc=VX2D_LOST;access_lost++;}
 /* .575 (peer review of .572, finding 10): a status query that finished no blit touched no register: the 3D state cache is kept (VX3D_busy
  * started it over every time: the next draw sent all its state again after a GetBltStatus or GetFlipStatus). VX3DN_poll above retired
  * whatever was in flight. A query that completed a pending blit, and every other action, as before. */
 else if(!desktop_cpu()||running||((action==3&&!pend_was)?(VX3D_lab_busy()||VX3DN_stopped()):(vx3dn_rt_why=2,VX3D_busy()))||vx_native_display_hold) rc=VX2D_SKIP;
 else if(action==1) FBHDA_access_begin(0);
 else if(action==2) FBHDA_access_end(0);
 else if(action!=0&&action!=3) rc=VX2D_SKIP;
 }
 vx_unlock(); return rc;
}
int VX2D_busy(void) {complete_pending(); if(!running) k_release(); return running||m2d_state==3;} /* v10.144: the 3D side asks before it touches the engine: hand a kept context back */
/* v10.73: read one dword of the register window (BAR1), for the scan line counter; 0xFFFFFFFF when the window is not usable. */
uint32_t VX2D_mmio_read(uint32_t off) {
 uint32_t base=vx_status.bar1&0xFFFFFFF0UL;
 if(off>0x3FFFFCUL||(off&3)||(vx_status.bar1&15)||base<0x100000UL||base>0xFFC00000UL) return 0xFFFFFFFFUL;
 if(!m2d_linear) {
  m2d_linear=_MapPhysToLinear(base,0x400000UL,0);
  if(!m2d_linear||m2d_linear==0xFFFFFFFFUL) {m2d_linear=0; return 0xFFFFFFFFUL;}
 }
 return rd(off);
}

int VX2D_bind(uint32_t flags_ptr,uint32_t enabled_ptr) {
 int rc=VX2D_DONE;
 vx_lock();
 complete_pending(); /* v10.139 (review F01): nothing pending across a bind or unbind */
 k_release(); /* v10.144 */
 gdi_fp16=flags_ptr; gdi_ep16=enabled_ptr; gdi_flags=gdi_enabled=0; gdi_map(); /* v10.273: selector:offset, mapped at each entry */
 xf_mask_p16=0; /* v10.279: the next request binds the mask again (an unbind leaves nothing to write) */
 /* v10.75: the display driver's Enable used to fail (VX2D_LOST) when the engine was lost, which left the machine without its display driver for a fault
  * that only concerns acceleration. The engine gets a reset attempt here; if it is still lost the GDI words are fenced (DIB engine, no acceleration)
  * and the bind succeeds. */
 if(m2d_state==3&&!lost_recover(1)) {if(gdi_flags) *gdi_flags|=0x0010; if(gdi_enabled) *gdi_enabled=0; lost_fenced=1;}
 vx2d_publish(); /* v10.142 */
 vx_unlock(); return rc;
}
int VX2D_lost(void) {complete_pending(); return m2d_state==3;}
int VX2D_is_lost(void) {return m2d_state==3;}
