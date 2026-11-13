/* v10.223: VXRING, the command-fetcher probe (cookbook 16.17). Copyright 2026, MIT.
 * op 0: read only. Sequencer registers, MMIO 0x400-0x5FC, the GPU page table's location and entries.
 * op 1: one 2D solid fill sent through the fetcher from a buffer in video memory, XP's one-shot form (s3ginv 0xBC770).
 * op 2: the same fill through XP's ring form (ring start 0xBB370, then one kick 0xBC5xx).
 * v1.2: op 3: one untextured 3D triangle: the packet the HAL would send, its register writes captured into the buffer as FE03043C blocks
 *   and fetched in the one-shot form. op 4: the same triangle through the register path, for comparison.
 * v1.3 (ABI 0x00010003): op 3 block forms in flags 0x10/0x20/0x40/0x80 (vx_native3d_vxd.c, cap_write); the reply carries the buffer's
 *   first VX_RING_STREAM words as written.
 * v1.4 (ABI 0x00010004): 0x41C = 00100000 written again just before the start words (flag 0x200: not, as v1.3); op 3 flag 0x100
 *   writes the type 0x10/0x11 blocks through the registers; the reply carries the register-path draw count since the VxD loaded.
 * v1.5 (ABI 0x00010005, v10.373): op 5, the ring append: the ring started, group A (flags bits 16-23: that many 2D fills, at least 1) kicked,
 *   then group B kicked into the next band at once (flag 0x400: after A is drawn and the fetcher waits at the pause), B a 2D fill or
 *   (flag 0x800) the 3D triangle in XP's forms. The registers before kick B and when both bands are drawn are returned.
 * v1.6 (ABI 0x00010006, v10.375): op 6, the kick onto an idle ring: A one fill, drawn (the fetcher idle at its pause), then B kicked:
 *   flags bits 16-23 fills (the last in its own colour) or (0x800) the 3D triangle; then either the VxD's wait (flag 0x1000: 8 idle
 *   reads in a row, then the stop at once) or the pixels' wait (as op 5); the first 32 status reads after the kick with their times.
 * v1.7 (ABI 0x00010007, v10.376): op 6 flag 0x4000: B is flags bits 16-23 3D triangles (each the whole packet, captured; the last in the
 *   request's colour, the others in another); flag 0x2000: a kick after every 8 of B's items; op 7: B alone in the one-shot form (no ring).
 * v1.8 (ABI 0x00010008, v10.377): ops 5-6 flag 0x8000: XP's kick (s3ginv 0xBC6EC: 0x41C, 64, 65), 68 not written again.
 * v1.9 (ABI 0x00010009, v10.379): ops 5-6 flag 0x1000000: the stale-words test (band C below B: 2D fills written after A's pause before
 *   A's kick, B written over them after). Reply: band C pixels changed, poison words, the largest 43C read while B ran.
 * v2.0 (ABI 0x0001000A, v10.380): op 8, the backlog: the ring started, flags bits 16-23 groups (32 when 0, at most 32) each one 2D fill of
 *   its own 8x8 cell of band B in its own colour, kicked one after another with no wait; flag 0x2000000 ends each group with a fence
 *   (type 0x11 registers 06/07 through FE020420: 0x494 = 0xA5000000 | k). Then 0x1000: the VxD's rule (8 idle reads, the stop at once),
 *   0x4000000: the fence rule (0x494 = the last group's, then 8 idle reads, the stop), neither: wait for every cell (200 ms) then the stop.
 *   Reply: cells drawn, the first missing, and the first 48 reads after the last kick of 400 41C 418 43C 440 494.
 * v2.1 (ABI 0x0001000B, v10.381): op 9, the stop and restart: the buffer filled with no-op blocks; run 1 (8 groups, band A's cells) kicked
 *   and drawn; 32 trap fills of band C's cells past its last pause (flag 1: after the stop; 2: written over with no-op blocks before the
 *   stop); the stop; band A painted back; run 2 at the buffer's middle (4: its start; 0x10: run 1's last pause; 0x20: none; 0x40: B in the
 *   one-shot form) with group B (band B, a 2D fill or, 8, the 3D triangle). Reply: bands A and C cell masks after the stop, after the new
 *   start and at the end; 440 at those points; the first 32 status reads after the new start.
 * v2.2 (ABI 0x0001000C, v10.383): op 9 flag 0x80: run 2's start written 60-63, 66, 67, 68, then 64/65. Op 10, the wrap without a stop:
 *   group A two blocks before the buffer's end, the trap in the last block, group B at the buffer's start, then 66/67 = A's pause and
 *   (flag 0) 64/65 = the start - 16 and a kick to B's pause, (1) 64/65 = B's pause at once, (2) as 0 without the kick; then group D
 *   (band A, another colour) kicked after B as usual.
 * Both fetch tests stop the fetcher afterwards (68 = 0A0000) and reset the engine if it does not come back to idle. */
#ifndef VX_RING_H
#define VX_RING_H
#include <stdint.h>
#define VX_IOCTL_RING 0x85500023UL
#define VX_RING_ABI 0x0001000EUL
/* v2.7 (ABI 0x0001000E, v10.397): op 12, the pair test: tape groups a0-a1 run as their own ring (written, started, kicked, waited for,
 * stopped), then at once, with no reset between, groups b0-b1 the same way; flags bits 16-23 times, mode bit 0 an engine reset before
 * each time, bit 1 no range A (B alone). Reply: rp_runs, rp_hangs (B's), rp_first, rp_status, rp_440, rp_fence (B's first hang);
 * rp_fence_hist[0] A's hangs, [1] A's first hang status, [2] its fence. */
/* v2.3 (ABI 0x0001000D, v10.393): the ring tape and op 11, the replay. At the first hang in the ring the VxD keeps the hung ring's words
 * (from its first group in the recorder, at most VX_TAPE_WORDS, to the hung group's end) and each group's start, pause, words and fence
 * number (VX_IOCTL_TAPE: in DWORD 0 reads it out; in DWORD 1 + a vx_tape puts one back, from a file of an earlier boot, refused unless
 * the command block is at the same offset). Op 11 writes the tape's words back where they were, a start block 256 bytes before the first
 * group, starts the ring and kicks each group's pause as the driver did; then up to 100 ms for the engine idle (0x101FF3 clear 8 reads),
 * the stop, an engine reset after a hang. Flags: bits 0-15 microseconds between kicks (a TSC spin), bits 16-23 times (0 = 1), 0x1000000
 * wait up to 2 ms for status bit 0x10 clear before each kick, 0x2000000 one kick to the last pause (the whole tape as one group),
 * 0x4000000 the engine idle before each kick (serial: the control), 0x8000000 the groups' fences left out (their blocks made no-ops).
 * v2.4 (same ABI, v10.394): 0x10000000 live: only group 0 written before the start, each later group written just before its kick
 * (after the spacing) while the one before runs, as the driver does; the area after group 0 first left as it was, or 0x20000000 zeros,
 * 0x40000000 pseudo-random words (the stale words the fetcher would find if it read ahead of its pause). */
#define VX_IOCTL_TAPE 0x85500027UL
#define VX_TAPE_WORDS 16384
#define VX_TAPE_GROUPS 64
#define VX_TAPE_MAGIC 0x45504154UL /* 'TAPE' */
typedef struct vx_tape {
 uint32_t magic,version,captured,cmd_off,block_end,start,words,ngroups,bpp,status,r440,fence,boot_ms,ring_kicks,flags,fence_off;
 uint32_t group[VX_TAPE_GROUPS][4]; /* start, pause, words, fence number */
 uint32_t w[VX_TAPE_WORDS];
 /* v2.4 (v10.394): the driver's last 128 events before the hang, oldest first: TSC low word, type | argument << 8 (vx_native3d_vxd.c VX3DN_ev) */
 uint32_t nev,ev_tsc_hang,ev[128][2];
 uint32_t mode_w,mode_h,mode_pitch; /* v2.5 (v10.395): the display mode at the hang */
} vx_tape;
typedef struct vx_ring_req {
 uint32_t cb,abi,op;
 uint32_t buf_linear,buf_bytes;   /* command buffer: linear address of video memory (a DirectDraw surface's lpSurface), at least 8 KB */
 uint32_t tgt_linear,tgt_pitch;   /* fill target: linear address and pitch of a surface in video memory */
 uint32_t width,height,color;     /* fill rectangle (at 0,0 of the target) and its colour in the desktop format */
 uint32_t flags;                  /* 1: set bit 0 of register 60 (XP sets it in ring mode on 5122); 2: also in one-shot form;
                                     4: ring: no kick (start block only); 8: ring: 68 = 0A0001 again after the kick;
                                     op 3: 0x10 XP's FE02/FE03 forms, 0x20 FE03 for vertices only, 0x40 keep type 0x11,
                                     0x80 state through the registers, only the vertex blocks fetched; 0x100 types 0x10/0x11 through
                                     the registers; 0x200 no 0x41C write before the start words */
 uint32_t a0,a1,b0,b1,mode;       /* v2.7: op 12 */
} vx_ring_req;
#define VX_RING_SAMPLES 8
#define VX_RING_STREAM 96
typedef struct vx_ring_reply {
 uint32_t cb,abi,result;          /* result: 0 done; 1 bad request; 2 engine busy or lost; 3 not idle after 200 ms (engine reset tried) */
 uint32_t sr[16];                 /* SR68 6A 6B 6C 6D 6E 6F 39 3F 1A 1E 4E 10 3C 1B 2D */
 uint32_t fb_bytes,bpp,pitch,table_offset,table_readable;
 uint32_t table_first[8];         /* entries 0..7 */
 uint32_t table_buf[2],table_tgt; /* entries for the first and last page of the buffer and for the target */
 uint32_t mmio[128];              /* MMIO 0x400-0x5FC before the test (after it for op 0) */
 uint32_t buf_offset,tgt_offset,gpu_start,gpu_end,gpu_pause,words;
 uint32_t prec[10];               /* the words written to 0x420 in order */
 uint32_t sample_ms[VX_RING_SAMPLES],sample_400[VX_RING_SAMPLES],sample_43c[VX_RING_SAMPLES],sample_444[VX_RING_SAMPLES];
 uint32_t samples,polls,elapsed_ms,idle;
 uint32_t after[6];               /* 400 43C 440 444 45C 494 after the stop */
 uint32_t reset_tried,reset_ok,pixels_set,pixels_changed,first_pixel;
 uint32_t checksum,capture_error,capture_blocks; /* v1.2: sum of the target's pixels; op 3 (bits, v1.3): 1 buffer full, 2 data before a header,
                                     4 another register, 8 a word in an FE02 block with top byte FE or CC; 0x1nn validate, 0x2nn execute */
 uint32_t stream[VX_RING_STREAM];   /* v1.3: the buffer's first words */
 uint32_t reg_draws;               /* v1.4: 3D register-path draws since the VxD loaded, before this test */
 /* v1.5, op 5: pauses of the two kicks; 400 43C 440 444 45C 494 just before kick B and when both bands were drawn; the time from kick B
  * to both drawn (ms, 0xFFFFFFFF never); band A drawn before kick B (1: A was finished, not mid-fetch); band B pixels set / changed */
 uint32_t pause_a,pause_b,mid[6],done[6],done_ms,a_before_b,b_set,b_changed,b_checksum,a_fills;
 /* v1.6, op 6: after kick B: status reads until the first busy one (0xFFFFFFFF none in 4000) and until idle again; TSC cycles / 16 at
  * those points; the VxD-style wait's reads; the first 32 reads (value, TSC cycles / 16 since the kick); band B's last-fill pixels */
 uint32_t k_reads_busy,k_reads_idle,k_t_busy,k_t_idle,k_wait_reads,k_400[32],k_t[32],b_last_set;
 uint32_t c_changed,poison_words,r43c_max; /* v1.9 */
 uint32_t bl_groups,bl_cells,bl_first_missing,bl_decide_reads,bl_last_fence,bl_fence_reads,bl_s[48][6],bl_t[48]; /* v2.0, op 8 */
 /* v2.1, op 9: band A cells drawn by run 1 (mask); the trap's first GPU address, words, cells; run 2's start (0xFFFFFFFF none); idle after
  * the stop; band C cells changed after the stop, after the new start, at the end; band A cells after the new start, at the end; 440 just
  * before the stop, after the new start, at the end; idle after the new start (before B) */
 uint32_t rs_a_run1,rs_trap_gpu,rs_trap_words,rs_trap_cells,rs_new_start,rs_stop_idle,rs_c_stop,rs_c_start,rs_c_end,rs_a_start,rs_a_end,rs_440[3],rs_start_idle;
 /* v2.3, op 11: times run, hangs, the first hung time (0xFFFFFFFF none), at the first hang: status, 440, the fence number read; the longest
  * time to idle of a time that finished (ms); TSC cycles a microsecond; hung times (bit k = time k); the groups kicked a time; tape words */
 uint32_t rp_runs,rp_hangs,rp_first,rp_status,rp_440,rp_fence,rp_ms_max,rp_cyc_us,rp_bits[8],rp_groups,rp_words,rp_fence_hist[8];
} vx_ring_reply;
/* .504: XP's ring in system memory (vx_native3d_vxd.c, xr_*): why it is off (vx3dn_xr_stat[1]), one list for the VxD and VXD3DMON.
 * A new reason goes here: its number, then its text in VX_XR_OFF_NAMES (VXD3DMON checks the count when it is built). */
#define VX_XR_OFF_NONE 0
#define VX_XR_OFF_NOTABLE 1   /* no page-table block in video memory */
#define VX_XR_OFF_ALLOC 2     /* system memory not allocated */
#define VX_XR_OFF_PAGES 3     /* page entries not usable */
#define VX_XR_OFF_NOPAUSE 4   /* the fetcher did not reach its pause in the ring at the first start */
#define VX_XR_OFF_TLBFLUSH 5  /* the translation-cache flush (SR6F bit 7) did not finish within 10 ms */
#define VX_XR_OFF_N 6
#define VX_XR_OFF_NAMES {"","no page-table block in video memory","system memory not allocated","page entries not usable",\
 "the fetcher did not reach its pause in the ring at the first start","translation-cache flush failed"}
/* .504: the ring's cache work by unit (TEXDBG 0xBC 0x9B, vx3dn_xr_unit): [0] cache lines flushed (CLFLUSH), [1] WBINVD calls, [2] words
 * copied with MOVNTI from the staging buffer. vx3dn_xr_stat[10] (all three in one slot before) is no longer written. */
#define VX_XR_UNIT_LINES 0
#define VX_XR_UNIT_WBINVD 1
#define VX_XR_UNIT_NTWORDS 2
#define VX_XR_UNIT_N 3
uint32_t VX3DN_tape(uint32_t op,const void *in,uint32_t in_bytes,void *out,uint32_t out_bytes);
uint32_t VX3DN_ring(const vx_ring_req *q,vx_ring_reply *r);
#endif
