/* VX85598 native Chrome9 HCM bring-up. Copyright (c) 2026, MIT. */
#ifndef VX855_3D_H
#define VX855_3D_H
#include <stdint.h>

#define VX3D_MAGIC 0x44335856UL
#define VX3D_VERSION 0x00190000UL
#define VX3D_BUILD 0x00090031UL /* v9.13: native triangle shape suite, minimal bias recipe */
#define VX3D_MATRIX_CASES 32UL
#define VX3D_DUMP_IDS 7UL
#define VX3D_DUMP_SPACES 6UL
#define VX3D_DUMP_CASE 11UL
#define VX3D_PRODUCER 0x58444F43UL /* CODX */
#define VX3D_PROBES 21
#define VX3D_SF_PROBE 20
#define VX3D_SF_PAGES 19UL
#define VX3D_SF_BYTES (VX3D_SF_PAGES*4096UL)
#define VX3D_SF_RING_BYTES 65536UL
#define VX3D_SF_GART 0x003C0000UL
#define VX3D_SF_TABLE_BYTES 65536UL
#define VX3D_SF_TABLE_BEGIN (VX3D_SF_GART-4096UL)
#define VX3D_SF_TABLE_END (VX3D_SF_GART+VX3D_SF_TABLE_BYTES+4096UL)
#define VX3D_SF_TABLE_SPAN (VX3D_SF_TABLE_END-VX3D_SF_TABLE_BEGIN)
#define VX3D_PIPELINE_PROBE 18
#define VX3D_PIPELINE_BANKS 7
#define VX3D_DMA_PROBE 19
#define VX3D_DMA_WORDS 512
#define VX3D_TRANSPORT_FIRST 16
#define VX3D_TRANSPORT_COUNT 2
#define VX3D_STATE_READS 18
#define VX3D_PHASES 8
#define VX3D_SEQ_COUNT 7
#define VX_IOCTL_3D_PROBE 0x85500010UL
#define VX_IOCTL_3D_TEST  0x85500011UL
#define VX3D_ARM 0x54534554UL
#define VX3D_SIDE 64UL
#define VX3D_PIXELS (VX3D_SIDE*VX3D_SIDE)
/* Keep every GPU address in an unused, mapped part of the existing 4 MiB. */
#define VX3D_ARENA 0x00200000UL
#define VX3D_ARENA_BYTES 0x00010000UL
#define VX3D_TARGET (VX3D_ARENA+0x1000UL)
#define VX3D_DEPTH (VX3D_ARENA+0x6000UL)
#define VX3D_VERTICES (VX3D_ARENA+0xB000UL)
#define VX3D_CASE_SOURCE_BYTES 256UL
/* OpenChrome-style PCI virtual queue: 64 KiB declared. Reserve twice that
 * plus guards because its length formula (/8) differs from OGPM (/16).
 * Any actual write beyond the declared end is still a guard failure. */
#define VX3D_QUEUE 0x00220000UL
#define VX3D_QUEUE_BYTES 0x00010000UL
#define VX3D_QUEUE_STORAGE (VX3D_QUEUE_BYTES*2)
#define VX3D_QUEUE_REGION (VX3D_QUEUE-0x1000UL)
#define VX3D_QUEUE_REGION_END (VX3D_QUEUE+VX3D_QUEUE_STORAGE+0x1000UL)
#define VX3D_QUEUE_SPAN (VX3D_QUEUE_REGION_END-VX3D_QUEUE_REGION)
#define VX3D_BACKGROUND 0xFF182430UL
#define VX3D_CANARY 0xA569C33CUL
#define VX3D_BLIT_COLOR 0xFF26A0D0UL
#define VX3D_BUSY_MASK 0x00001FF7UL
#define VX3D_POLL_LIMIT 1000000UL
#define VX3D_TIMEOUT_MS 250UL

enum vx3d_result {
 VX3D_NOT_RUN=0, VX3D_PASS, VX3D_NOT_READY, VX3D_BAD_BAR,
 VX3D_NO_MMIO, VX3D_BUSY, VX3D_TIMEOUT, VX3D_GUARD_FAILURE,
 VX3D_SOLID_FAILURE, VX3D_SHADE_FAILURE, VX3D_DEPTH_FAILURE,
 VX3D_DISABLED, VX3D_BAD_REQUEST, VX3D_INIT_FAILURE, VX3D_BLIT_FAILURE,
 VX3D_CONTROL_UNCONFIRMED, VX3D_FETCH_UNCONFIRMED,
 VX3D_SF_MAPPING_FAILURE, VX3D_SF_PAUSE_UNCONFIRMED
};
extern uint32_t vx3d_x_reach; /* v10.11: request->reserved bits 23:16: 1 = render target, 2 = depth buffer, 3 = texture at the sf_reach_off[] video-memory offsets */
extern uint32_t vx3d_x_select; /* v9.29: 0 = rotation extras, 1..6 = one host-data variant */
/* v9.62: 2D lab. reserved&0xFF == 21 runs one operator-defined 2D operation
 * on the desktop window (448,320) 64x64 and returns the before image in
 * sf_x_raw (flat 64x64) and the after image in image[]. */
#define VX3D_LAB_REGS 16
#define VX3D_LAB_DATA 1024
typedef struct vx3d_request {
 uint32_t cb, version, arm, reserved, probe;
 uint32_t lab_cmd, lab_mode, lab_sx, lab_sy, lab_dx, lab_dy, lab_w, lab_h;
 uint32_t lab_fg, lab_bg, lab_flags, lab_nregs, lab_ndata;
 uint32_t lab_reg[VX3D_LAB_REGS][2];
 uint32_t lab_data[VX3D_LAB_DATA];
} vx3d_request;
typedef struct vx3d_case_report {
 uint32_t flags, mode, vertices, layout, first, address, pause, count;
 uint32_t fill_rects, fill_ms; /* v9.8: fill rectangles drawn, milliseconds */
 uint32_t before[3], after[3];
 uint32_t changed, errors, sample, orange, latch_expected, latch_observed;
 uint32_t program[128];
 /* Issued values, not calibrated readback: FE A40, T01 A01, T04 A28,
  * T01 A1E/A1F/A20 (far/near), T01 A13 (Z compare), T04 A20 (cycle). */
 /* v20 adds exact issued T04 A40..A48 viewport coefficients. */
 /* v22 adds T01 A88/A89/A8A, T04 A50/A51/A52, T04 A2F,
  * and the non-length bits of T04 A00. All are issued values. */
 uint32_t state[25];
 /* v21: FVF words/input mask, VB stride/index bits, geometry, source
  * address/DWORDs, payload DWORDs, control dependency, flags (1 XP
  * inline prefix, 2 reverse winding, 4 fan order), FVF A01/A02. */
 uint32_t config[12];
 uint32_t input[24];
 /* v23: kind (0 off, 1 C0, 2 diffuse), expected target color,
  * T01 A90..A9A low-24 values, 4 native ALU DWORDs, float4 C0.
  * Captured configuration, not hardware readback. */
 uint32_t shader[21];
 /* v24: count plus up to 23 raw FE override words, submitted after the
  * unchanged 16-word startup initialization. These are issued values. */
 uint32_t fe_override[24];
} vx3d_case_report;
typedef struct vx3d_report {
 uint32_t magic, cb, version, result;
 uint32_t capabilities, stage, status_before, status_after;
 uint32_t writes, polls, elapsed_ms, guard_errors;
 uint32_t solid_errors, shade_errors, depth_errors, changed_pixels;
 uint32_t target_offset, pitch, width, height;
 uint32_t solid_sample, shade_sample, depth_sample, fault_latched;
 uint32_t image[VX3D_PIXELS];
 /* Sequencer order: SR10, SR1A, SR1E, SR2D, SR3F, SR4E, SR3C. */
 uint32_t build, seq_before[VX3D_SEQ_COUNT], seq_active[VX3D_SEQ_COUNT];
 uint32_t seq_after[VX3D_SEQ_COUNT], init_flags;
 uint32_t blit_errors, blit_sample, blit_changed_pixels;
 uint32_t status_or, status_setup, status_blit, status_solid;
 uint32_t pll_polls;
 uint32_t probe_id, display_isolated, display_begin, display_end;
 uint32_t display_draws_before, display_draws_after;
 uint32_t display_skips_before, display_skips_after;
 uint32_t vertex_words, vertex_vertices, vertex_stride, checks_run;
 /* Added in v4. Profiles 12..15 test acceptance as well as pixels.
  * path_flags: 1=CR at 041C/0420, 2=006C sync, 4=old v4 port probe (unused),
  * 8=acceptance diagnostics. Raw readback is retained even on no match. */
 uint32_t path_flags, phase, parameter_type, phase_status[VX3D_PHASES];
 uint32_t acceptance, state_matches;
 /* fence: original, A, B, post-draw, restored, expected post-draw.
  * markers: CR T11AA / VP T04AA / general T01AA, each original/A/B/restore. */
 uint32_t fence[6], marker[3][4];
 uint32_t state_readback[VX3D_STATE_READS][4];
 /* v5: aligned texture-general groups 00..03 and 04..07, before/after
  * initialization; occlusion 1C..1F original/cleared/post-draw; depth pixels. */
 uint32_t general_before[8], general_after[8], occlusion[3][4];
 uint32_t depth_changed_pixels;
 /* v6: command-regulator queue observations, not live engine-state proof.
  * queue_phase: 1=snapshot, 2=program, 3=capture, 4=disable/restore,
  * 5=known disabled baseline established, 6=memory restored. End is exclusive.
  * The WO controls are observations only; snapshots are never replayed. */
 uint32_t queue_flags, queue_control, queue_start, queue_end, queue_length;
 uint32_t queue_storage_bytes;
 uint32_t queue_original[8], queue_active[8], queue_disabled[8], queue_restored[8];
 uint32_t queue_other_original[4];
 uint32_t queue_guard_errors, queue_changed_words, queue_phase;
 /* v7: explicit producer, actual failed wait, independent CR-fence evidence.
  * wait_failure_reason: 0=none, 1=poll limit, 2=time limit, 3=unreadable,
  * 4=wrapper final status busy/unreadable. Raw WO mismatch is not a timeout. */
 uint32_t producer, wait_failure_reason, wait_failure_status;
 uint32_t queue_fence_original, queue_claim_fence, queue_disable_fence;
 uint32_t queue_fence_restored;
 /* v8: two legal state markers, before vertices, via HCM A02 IDs 0..6.
  * raw[marker phase][bank][query 0100/0184][raw aligned lane].
  * match masks test two changing values and the index tag, never live-state
  * or rendering proof. Identical records equal bank 0 for both markers.
  * flags: 1=started, 2=render state reissued, 4=bank0 reselected, 8=completed,
  * 16=CR bank positively contradicts rendering-state restoration. */
 uint32_t pipeline_flags, pipeline_match[2], pipeline_identical;
 uint32_t pipeline_raw[2][VX3D_PIPELINE_BANKS][2][4];
 /* v8/1.3n: local-framebuffer command fetch, no host/GART DMA addresses.
  * programs 0=fence control, 1=state/triangle/wait/fence. Count in DWORDs.
  * cursor/status: before/after launch, raw 043C/0444 observations.
  * phase: 1=claim, 2=control built, 3=control launched, 4=control parked,
  * 5=draw built, 6=draw launched, 7=draw parked, 8=memory restored. */
 uint32_t dma_flags, dma_phase, dma_count[2], dma_expected[2], dma_observed[2];
 uint32_t dma_cursor[2][2], dma_status[2][2];
 uint32_t dma_source_errors, dma_guard_errors, dma_base, dma_limit;
 uint32_t dma_stop[2], dma_park_fence[2];
 uint32_t dma_program[2][VX3D_DMA_WORDS];
 /* v10: owned pinned host ring, local RTSF table, two fence destinations.
  * GTI order: SR6A, SR6B, SR6C, SR6F, SR7B (sampled, never written).
  * rows: prime, fence 41C, fence 43C, triangle, retirement.
  * Host PTEs are raw VMM page-table snapshots, not guessed bus addresses. */
 uint32_t sf_flags, sf_phase, sf_host_linear, sf_host_bytes;
 uint32_t sf_host_pte[VX3D_SF_PAGES];
 uint32_t sf_gart_offset, sf_gart_bytes, sf_location_word, sf_pci_command;
 uint32_t sf_gti_before[5], sf_gti_active[5], sf_gti_after[5];
 uint32_t sf_flush_polls, sf_failure_phase, sf_source_errors, sf_table_errors;
 uint32_t sf_guard_errors, sf_pause[5], sf_expected[5], sf_observed[5];
 uint32_t sf_before[5][3], sf_after[5][3];
 uint32_t sf_control43c[64];
 /* v11: independently observe fetched Header0 2D writes and target pixels.
  * These are packet-execution evidence even if T11 fences are ignored. */
 uint32_t sf_transfer_flags, sf_2d_saved[7], sf_2d_restored[7];
 uint32_t sf_2d_color[2], sf_2d_before[2], sf_2d_observed[2];
 uint32_t sf_2d_errors[2], sf_2d_changed[2];
 /* Header0-only replay is a separate, prepublished second triangle path.
  * primary[] = changed pixels, sample errors, sample for Header2/Header3.
  * alt flags: 1 built, 2 submitted, 4 settled, 8 fenced, 16 pixels passed. */
 uint32_t sf_primary[3];
 uint32_t sf_alt_flags, sf_alt_count, sf_alt_expected, sf_alt_observed;
 uint32_t sf_alt_pause, sf_alt_before[3], sf_alt_after[3];
 uint32_t sf_alt_changed, sf_alt_errors, sf_alt_sample;
 uint32_t sf_alt_program[VX3D_DMA_WORDS];
 uint32_t sf_memory[3]; /* SR68 SL size, SR6D/6E SL physical-base bytes */
 /* v12: distinct FE82 command/parameter branches and FE83 raw vertices.
  * v14 keeps the field layout but replaces both FE82 controls with inline
  * Header0 2D/fence packets; branch_payload/address/blocks[0..1] stay zero.
  * SF rows 1/2 and flags 100/200,400/800 now refer to inline controls.
  * flags: 1 built, 2 CPU branch-enable issued, 4 FE83 built,
  * 8 disable issued after retirement, 16 draw tried without control fence. */
 uint32_t branch_flags, branch_address[3], branch_blocks[3];
 uint32_t branch_control_count, branch_control[VX3D_DMA_WORDS];
 uint32_t branch_payload[2][16], branch_vertices[16];
 uint32_t cpu_flags, cpu_original, cpu_expected[2], cpu_observed[2], cpu_restored;
 uint32_t diagnostic_pci[4], diagnostic_mmio[2][4];
 uint32_t cpu_after_fe[2]; /* expected, observed; neither is a fetched ID */
 /* v13: retries after documented CR initialization, before mapping/fetch.
  * rows: OpenChrome PCIe baseline, earlier v6 CR/FIFO baseline.
  * flags: 1 attempted, 2 port41C hit, 4 port43C hit, 8 original restored.
  * selected: 0 bare, 1 OpenChrome, 2 v6 baseline, FFFFFFFF none.
  * mmio columns: 400,43C,444,494, sampled after the fence/restore pair. */
 uint32_t cpu_retry_flags[2], cpu_retry_expected[2][2], cpu_retry_observed[2][2];
 uint32_t cpu_retry_restored[2], cpu_retry_mmio[2][4], cpu_selected;
 /* v15: primary=dma_program[1] inline Header3, with no branch. The FE83
  * fallback uses branch_control/count at SF+1400; inline control1 moves
  * to the existing 64-DWORD sf_control43c field. H0 fallback is SF+2000.
  * branch-test flags: 1 built, 2 submitted, 4 settled, 8 fenced,
  * 16 pixels passed, 32 target observed before completion was confirmed.
  * observation flags: 1 inline, 2 FE83, 4 H0 read while unconfirmed.
  * Such observations are not completed-rendering or PASS evidence. */
 uint32_t sf_branch_flags, sf_branch_pause, sf_branch_before[3], sf_branch_after[3];
 uint32_t sf_branch_expected, sf_branch_observed;
 uint32_t sf_branch_changed, sf_branch_errors, sf_branch_sample, sf_draw_observation;
 /* v16: preload 3D state by CPU before the first fetched vertex-only draw.
  * flags: 1 state issued, 2 idle, 4/8 CPU marker seen CR/FE,
  * 10/20 fetched marker seen CR/FE, 40 trailing H0 marker seen,
  * 80 RC-cache clear release issued, 100/200 CPU/GPU queries ran.
  * raw[CPU/GPU phase][CR/FE bank][aligned A8..AB lane]. Markers measure
  * the selected writeback path, not rasterization or pipeline acceptance. */
 uint32_t sf_state_flags, sf_cpu_state_status, sf_state_marker[4];
 uint32_t sf_state_raw[2][2][4], sf_state_fvf[4], sf_state_target[4];
 uint32_t sf_draw_marker_expected, sf_draw_marker_observed;
 /* v17: retry the CPU-state/inline-vertex draw with XP's AGP CMDQ on.
  * The existing sf_branch_* record now describes this inline retry,
  * not an FE83 branch. No branch unit is enabled by the SF test.
  * flags: 1 built, 2 enable issued, 4 idle, 8 CPU state reissued,
  * 10 trailing H0 latch matched. Queue bytes are GPU-owned once enabled. */
 uint32_t sf_queue_compare_flags, sf_queue_compare_control;
 uint32_t sf_queue_compare_start, sf_queue_compare_end, sf_queue_compare_length;
 uint32_t sf_queue_compare_status[2], sf_queue_compare_latch;
 /* Same-target 16x16 3D point/rectangle trials, CMDQ OFF then ON.
  * These avoid triangle assembly but use one vertex: not a FE bypass.
  * flags: 1 built, 2 submitted, 4 settled, 8 exact pixels passed,
  * 10 unconfirmed pixel observation, 20 CPU point state issued,
  * 40 trailing H0 latch matched. Zero fields without flags are unrun. */
 uint32_t sf_point_flags[2], sf_point_count[2], sf_point_pause[2];
 uint32_t sf_point_before[2][3], sf_point_after[2][3];
 uint32_t sf_point_changed[2], sf_point_errors[2], sf_point_sample[2];
 uint32_t sf_point_latch[2], sf_point_program[2][64];
 /* v18: fixed transport/CMDQ OFF, independent primitive/config cases.
  * Case flags hex: 1 built, 2 CPU state issued, 4 submitted, 8 settled,
  * 10 coverage passed, 20 trailing H0 latch, 40 unconfirmed observation,
  * 80 any changed pixel. Pixels, counts and checks are full resolution.
  * v24: 15 cases; flag 100 marks a failed/unrun path control, not a skip.
  * image[] is a 5x5 contact sheet (12-pixel cells) until a triangle
  * passes, then the full verified triangle. Inputs are never CPU-drawn
  * into the render target. Stop after a verified triangle or timeout. */
 uint32_t sf_matrix_count, sf_matrix_triangle_pass, sf_matrix_last, sf_matrix_observed;
 uint32_t sf_fill_spans, sf_fill_pixels; /* v9.7 totals over the fill cases */
 vx3d_case_report sf_matrix[VX3D_MATRIX_CASES];
 /* v25: register read-back dump of every stage (ID 0..6) for five
  * address spaces (0001,0004,FE02,00FE,0002) x 64 aligned groups of four
  * tagged words, taken after case VX3D_DUMP_CASE setup. */
 uint32_t dump_flags, dump_words[VX3D_DUMP_IDS][VX3D_DUMP_SPACES][64][4];
 /* v9.15: per-case coverage bitmaps (cases 0..15, 64x64 bits, row-major,
  * bit set = pixel differs from background) and full Gouraud colour map of case 9. */
 uint32_t sf_cover[16][128];
 uint32_t sf_gouraud[VX3D_PIXELS];
 uint32_t sf_raw[14][64]; /* v9.23: raw pixels of window (4..11,4..11), cases 16..25 */
 uint32_t sf_cls[12][256]; /* v9.22: 2D proof window classes, 2 bits per pixel, cases 16..25 */
 uint32_t sf_x_info[8][8]; /* v9.27 post-phase isolated 2D variants */
 uint32_t sf_x_raw[8][512];
 uint32_t sf_x_cls[8][32];
 uint32_t sf_miss[VX3D_MATRIX_CASES][33]; /* v9.18: count, then first 32 mismatches (y<<16|x, bit31 = drawn outside model) */
} vx3d_report;
typedef char vx3d_report_v24_size[(sizeof(vx3d_report)==166308)?1:-1];
enum vx3d_queue_flags {
 VX3D_Q_SNAPSHOT=1, VX3D_Q_OWNED=2, VX3D_Q_ENABLED=4,
 VX3D_Q_DISABLED=8, VX3D_Q_RESTORED=16, VX3D_Q_MEMORY_RESTORED=32,
 VX3D_Q_CONFIG_MISMATCH=64, VX3D_Q_BAD_READBACK=128,
 VX3D_Q_ALREADY_ENABLED=256, VX3D_Q_RESTORE_MISMATCH=512,
 VX3D_Q_WO_OBSERVATION=1024, VX3D_Q_COMPLETION_UNCONFIRMED=2048,
 VX3D_Q_READBACK_RESPONDED=4096
};
/* Q_RESTORED means a known OFF baseline, not firmware register restoration.
 * ENABLED means the enable write was submitted. It is not physical proof.
 * A raw mismatch never vetoes a documented WO write. Only a bank that first
 * matched all programmed fields AND changed from its original snapshot can
 * positively contradict shutdown. Completion fences do not prove rendering. */
/* Additional path_flags: 16=no-texture/user-clip initialization,
 * 32=explicit viewport, 64=C2S transform, 128=clipping, 256=direct depth only.
 * 512=matched transport probe, 1024=enable its owned PCI virtual queue,
 * 2048=per-engine marker readback audit (probe 18, queue OFF),
 * 4096=fetched-command-buffer test (probe 19, virtual queue OFF).
 * Probe 00 retains the old setup as a control. Probes 01..15 use corrected
 * setup and the vendor's 041C control port. checks_run 64=depth-only check. */
/* acceptance: 1=fence A+B, 2=CR tag A+B, 4=VP tag A+B,
 * 8=general tag A+B, 16=post-draw fence, 32=fence restored.
 * These are command-path evidence, never a substitute for rendered pixels.
 * phase_status: 0=preflight/CR, 1=2D, 2=FE, 3=state, 4=acceptance,
 * 5=draw, 6=post-draw readback, 7=cleanup. Each value is an OR of samples. */
/* capabilities: bit 0 = native test endpoint. No Direct3D HAL is advertised. */
/* init_flags: 1=snapshot, 2=enabled/readback checked, 4=restored,
 * 8=left enabled after unsafe/unconfirmed completion, 16=readback mismatch, 32=reset asserted,
 * 64=controls programmed, 128=restore mismatch, 256=PLL did not lock,
 * 512=VX855 FE/global configuration submitted (not a readback claim),
 * 1024=general no-texture/user-clip clear did not read back. */

typedef struct vx3d_io {
 void *context;
 uint32_t (*read_mmio)(void *, uint32_t);
 void (*write_mmio)(void *, uint32_t, uint32_t);
 uint32_t (*read_fb)(void *, uint32_t);
 void (*write_fb)(void *, uint32_t, uint32_t);
 uint32_t (*milliseconds)(void *);
} vx3d_io;

typedef struct vx3d_sf_io {
 void *context;
 uint32_t (*read_host)(void *, uint32_t);
 void (*write_host)(void *, uint32_t, uint32_t);
 uint32_t (*read_seq)(void *, uint32_t);
 void (*write_seq)(void *, uint32_t, uint32_t);
 void (*publish)(void *);
} vx3d_sf_io;
#define VX3D_SF_SAFE_RELEASE 0x00010000UL
void vx3d_run_lab(const vx3d_io *, const vx3d_sf_io *, vx3d_report *, const vx3d_request *);
void vx3d_run_sf(const vx3d_io *, const vx3d_sf_io *, vx3d_report *, uint32_t *, uint32_t *, uint32_t *);
void vx3d_run(const vx3d_io *, vx3d_report *, uint32_t *backup);
void vx3d_run_probe(const vx3d_io *, vx3d_report *, uint32_t *backup);
void vx3d_run_transport(const vx3d_io *, vx3d_report *, uint32_t *backup, uint32_t *queue_backup);
#ifdef VXD32
unsigned long VX3D_probe(vx3d_report *);
unsigned long VX3D_test(const vx3d_request *, vx3d_report *);
#endif
#endif
