/* v10.434: the VxD's sampling profiler (VX_IOCTL_SAMPLE, vx_native3d_vxd.c), shared with VXD3DMON.
 * An asynchronous time-out every millisecond (as often as the system timer allows) records what the processor and the 3D engine are doing:
 * whether the render thread (the last thread that sent draw packets) is the one running and where (in the VxD waiting for the engine,
 * in the VxD otherwise, in VXDDRAW.DLL, in other 32-bit code: by 4 KB page, in 16-bit or V86 code), or another thread is; and whether
 * the engine is busy (0x400). The samples of a frame (flip to flip) are added to its class when it ends: gameplay frames (50 draw packets
 * or more) under 75 ms, gameplay frames of 75 ms and more, other frames (menus, loading). A late time-out (interrupts off, a critical
 * section) is counted in the category of the sample that ends it, and apart in late_ms. */
#ifndef VX_SMP_H
#define VX_SMP_H
#define VX_IOCTL_SAMPLE 0x85500029UL /* in[0] 1 start (in[1] VXDDRAW's base, in[2] its size; .521: in[3], when given, 1 asks for the RTC's clock; .524: the RTC always, in[3] ignored, no start when IRQ 8 or its periodic interrupt is taken), 0 stop, 2 read (out: vx_smp); .520: 3 spin
 * (VXSMPCAL): in[1] microseconds, in[2] the kind (0 a plain loop, 1 MOVNTI and CLFLUSH over the XP ring's pages as the ring's copies do,
 * 2 the same with interrupts off), the thread marked the render thread; out[0] TSC cycles spun */
#define VX_SMP_VERSION 0x549 /* .549: the VxD's own lines (vxd_ref, vline...); .520: the time-weighted fields at the end; .521: clk, in[3] of a start; .534: the 64-byte lines */
#define VX_SMP_CATS 8
#define VX_SMP_RWAIT 0 /* render thread: in the VxD, waiting for the engine */
#define VX_SMP_RVXD 1 /* render thread: in the VxD otherwise */
#define VX_SMP_RHAL 2 /* render thread: VXDDRAW.DLL (ring 3) */
#define VX_SMP_RCODE 3 /* render thread: other 32-bit code (the game, the Direct3D runtime, system DLLs): by page */
#define VX_SMP_R16 4 /* render thread: 16-bit or V86 code, or its registers not readable */
#define VX_SMP_OCODE 5 /* another thread: 32-bit code (shared-arena pages by page) */
#define VX_SMP_O16 6 /* another thread: 16-bit or V86 code, or not readable */
#define VX_SMP_NONE 7 /* no render thread yet */
/* .520: the VxD's phase while the render thread is inside it (vx_smp_ph, set by the VxD at its steps) */
#define VX_SMP_PH_NONE 0 /* not set (a call before the sampler knew its phase) */
#define VX_SMP_PH_CALL 1 /* a call other than a draw submit (Lock, flip, query, 2D, status) */
#define VX_SMP_PH_SUBMIT 2 /* a submit: the lock, the front wait, the set-up and close around the packets */
#define VX_SMP_PH_COPY 3 /* a packet copied in */
#define VX_SMP_PH_VALID 4 /* a packet validated */
#define VX_SMP_PH_BUILD 5 /* a packet's command words built (the emitter) */
#define VX_SMP_PH_KICK 6 /* a group started: the cache flush or copy, fence, kick */
#define VX_SMP_PH_WAIT 7 /* waits for the engine or the ring (retires, room after a wrap) */
#define VX_SMP_PH_SINGLE 8 /* a single submit (VX3DN_submit) or a clear */
#define VX_SMP_PH_CAL 9 /* VXSMPCAL's spin inside the VxD */
/* .526: VX_SMP_PH_SUBMIT split (it was a quarter of the VxD's time in GTA3, unaccounted for): a batch call's parts */
#define VX_SMP_PH_B_ENTER 10 /* the batch's entry: the engine lock, the busy checks, the mapping */
#define VX_SMP_PH_B_OPEN 11 /* the sequencer opened, the lock bit read, the status published, the fetcher checked idle */
#define VX_SMP_PH_B_LOOP 12 /* between a batch's packets: the cursor exclusion, groups opened, the counts, the prefetch */
#define VX_SMP_PH_B_CLOSE 13 /* after the packets: the status published, the sequencer closed, the cursor, the lock left */
#define VX_SMP_PAGES 1024
#define VX_SMP_OPAGES 256
#define VX_SMP_HIST 1001
#define VX_SMP_VLINES 2048 /* .549: the render thread's samples inside this VxD by 64-byte line (its ring-0 code) */
#define VX_SMP_LINES 8192 /* .534: the render thread's ring-3 code (the HAL and other 32-bit code) by 64-byte line, gameplay frames */
typedef struct vx_smp {
 unsigned long version,on,samples,late_ms,flat_cs,hal_base,hal_size,tick_cycles; /* tick_cycles: TSC / 256 spent in the sampler */
 unsigned long start_ms,stop_ms,frames_seen,pk_threshold;
 unsigned long cat[3][VX_SMP_CATS],eng[3][VX_SMP_CATS]; /* [class][category] ms (samples + late); eng: of those, the engine busy */
 unsigned long frames[3],frame_ms[3],run_ms[3],run_n[3]; /* run_ms: the flipping thread's own run time (_GetThreadExecTime between its flips), run_n frames it was read for */
 unsigned long page[VX_SMP_PAGES],pn[VX_SMP_PAGES][2],page_other[2]; /* the render thread's 32-bit code, gameplay frames: page (EIP >> 12), [0] under 75 ms, [1] 75 ms+ */
 unsigned long opage[VX_SMP_OPAGES],opn[VX_SMP_OPAGES][2],opage_other[2]; /* other threads, shared arena only (0x80000000+), gameplay frames */
 unsigned long hist[VX_SMP_HIST]; /* gameplay frames by their time, 1 ms buckets, [1000] = 1000 ms and more */
 unsigned long cs16[16],cs16n[16][2],cs16_other,v86n[2]; /* v10.438: the 16-bit samples (render [0] and other threads [1]) by code selector (cs16n), V86 apart (v86n) */
 unsigned long blk_hist[8],blk_ms,blk_code[8],blk_code_n[8],blk_code_ms[8],blk_v86ms; /* v10.439: stretches the render thread was not running
  (1, 2-5, 6-10, 11-15, 16-20, 21-50, 51-200, 201+ ms), their ms in all and the V86 ms in them; by the render thread's last VxD call before
  each (its IOCTL code; 0 none yet) */
 unsigned long vxd_cyc[3],tt_full; /* v10.439: the render thread's time inside the VxD by frame class, measured (TSC / 256); calls the thread table could not hold */
 unsigned long tri[3]; /* v10.441: the triangles the VxD sent, by frame class */
 /* .520: the time-weighted sampler. A sample stands for the time since the sample before it (TSC / 1024, about a microsecond at the board's
  1000 MHz), not for 1 ms: a tick held back (the render thread's ring-3 code running while the time-out could not be delivered) and given at
  the next ring-0 entry is no longer counted as a whole millisecond where it landed. tw: as cat, time-weighted; gap_n: the gaps before the
  ticks by where the tick landed ([0] render thread ring 3 (HAL, other 32-bit, 16-bit), [1] render thread inside the VxD, [2] another
  thread, [3] no render thread yet) and length (us: under 500, 500-899, 900-1099, 1100-1499, 1500-2499, 2500-4999, 5000-19999, 20000+);
  gap_t: their TSC / 1024 in all; ph_n / ph_t: the render thread's samples inside the VxD by the VxD's phase (VX_SMP_PH_*), counted and
  time-weighted; tsc_khz: the TSC's rate, measured from the start (TSC / 1024 per ms = tsc_khz / 1024) */
 unsigned long tw[3][VX_SMP_CATS],gap_n[4][8],gap_t[4],ph_n[16],ph_t[16],tsc_khz,tw_total;
 unsigned long clk; /* .521: the sampler's clock: 0 the VMM's time-out (re-armed each tick, the PIT's rate; removed in .524), 1 the RTC's periodic interrupt (IRQ 8,
  1024 a second, a hardware interrupt: the sample point is wherever the processor was) */
 unsigned long n_all[VX_SMP_CATS],tw_all[VX_SMP_CATS]; /* .520: every sample by category since the start, counted and time-weighted, whatever the frames (VXSMPCAL) */
 unsigned long line[VX_SMP_LINES],ln[VX_SMP_LINES],line_other; /* .534: the render thread's samples in VXDDRAW.DLL and other 32-bit code (VX_SMP_RHAL, VX_SMP_RCODE) in
  gameplay frames by line (EIP >> 6; 0 an empty slot), both frame classes together; line_other: samples whose line found no slot. VXD3DMON names
  the HAL's lines from VXDDRAW.MAP and D3D8.DLL's from its function table */
 /* .549: the render thread's samples inside the VxD (VX_SMP_RVXD), gameplay frames, by the interrupted ring-0 code's 64-byte line: the
  interrupt's frame (EIP, CS 28h, EFLAGS with IF) found on the ring-0 stack between the sampler's own stack pointer and the thread's client
  registers. vxd_ref: this VxD's smp_tick at run time (VXD3DMON takes the load base from it and VX85598_VXD.MAP); vline_other: lines with no
  slot; vline_miss: samples whose frame was not found; vline_vmm: frames found in other ring-0 code (VMM, other VxDs: EIP outside
  vxd_ref - 0x60000 .. vxd_ref + 0x20000) */
 unsigned long vxd_ref,vline[VX_SMP_VLINES],vln[VX_SMP_VLINES],vline_other,vline_miss,vline_vmm;
} vx_smp;
#endif
