/* VX85598 additions, Copyright (c) 2026. MIT; see LICENSE. */
#ifndef VX85598_H
#define VX85598_H
#include <stdint.h>
#define VX_MAGIC 0x38395856UL
#define VX_VERSION 0x00010002UL
#define VX_IOCTL_STATUS 0x85500001UL
#define VX_IOCTL_LOG 0x85500002UL
#define VX_IOCTL_2D_CTRL 0x85500012UL /* in: live flags (bit0 all copy directions, bit1 engine off); FFFFFFFE query; FFFFFFFF registry defaults. out: ops/fallbacks/state/flags. */
#define VX_IOCTL_EDID 0x85500013UL /* out: vx_edid_report */
typedef struct {
 uint32_t magic, edid_valid, edid_bus, base_modes, total_modes, extras_on, bios_count, reserved;
 uint32_t edid[32];       /* 128 raw EDID bytes */
 uint32_t bios[64*3];     /* per BIOS mode: [0]=id|(verdict<<16), [1]=width|(height<<16), [2]=bpp|(memory model<<8)|(attributes<<16) */
 uint32_t scan[36];       /* bus scan (only when the request asked for it): 3 buses x 12 DWORDs, see vx855_edid.c */
} vx_edid_report;
/* verdicts: 0 base mode, 1 extra mode accepted, 2 BIOS mode query failed, 3 not a linear direct-colour graphics mode, 4 bpp not 16/32 (8bpp is offered only as the three base modes),
   5 size outside 640x480..1920x1200, 6 layout/pitch/memory check failed, 7 same size and depth already present, 8 monitor does not list it,
   9 no EDID and larger than 1024x768, 10 mode table full */
#define VX_IOCTL_PROBE 0x85500026UL /* v10.333 (VXPROBE): in DWORD sub [, DWORD arg]; out 64 DWORDs (vxd_vesa.c VX_probe) */
#define VX_IOCTL_CRT 0x85500014UL /* out: vx_crt_report (read-only sequencer and CRTC register dump, independent of the 3D engine) */
typedef struct {
 uint32_t magic, misc, crtc_base, sr10_found;
 uint32_t sr[256];
 uint32_t cr[256];
} vx_crt_report;
#define VX_IOCTL_CACHE 0x85500019UL /* out: vx_cache_report (read-only: CPU type, MTRR/PAT registers, page-table bits of the framebuffer mapping, what the MTRR service returned) */
#define VX_CACHE_ABI 1UL
typedef struct {
 uint32_t magic, abi;
 uint32_t mtrr_called, mtrr_ver, mtrr_status, mtrr_base, mtrr_size, mtrr_conf; /* what the VxD asked the Windows MTRR service for the framebuffer, and what it returned */
 uint32_t has_cpuid, cpuid_max, vendor[3], sig, feat_edx, feat_ecx, ext_max;
 uint32_t msr_ok;                                  /* 1 when the MTRR/PAT registers below were read */
 uint32_t mtrrcap[2], deftype[2], pat[2];          /* IA32_MTRRCAP 0FEh, IA32_MTRR_DEF_TYPE 2FFh, IA32_PAT 277h (lo, hi) */
 uint32_t var_count;                               /* variable ranges listed in var[] */
 uint32_t var[16*4];                               /* per range: base lo, base hi, mask lo, mask hi (IA32_MTRR_PHYSBASEn/PHYSMASKn at 200h+2n) */
 uint32_t fb_phys, fb_linear, fb_map_bytes;
 uint32_t pte_count, pte_page[4], pte[4];          /* page-table entries of sample pages of the framebuffer mapping (bit3 PWT, bit4 PCD, bit7 PAT) */
} vx_cache_report;
#define VX_IOCTL_CACHE_SET 0x8550001AUL /* in: DWORD cache type for the framebuffer MTRR range (0 uncached, 1 cached, 2 framebuffer cached = write-combining, 3 hardware coherent); out: DWORD MTRR service status. Diagnostic (VX2READ): the driver puts type 2 back when the handle closes. */
#define VX_IOCTL_DAC 0x8550001BUL /* in: vx_dac_req (DirectDraw palette at 8bpp); out: DWORD 1 = done. first=0xFFFFFFFF with count 0 puts the GDI palette back; first=0xFFFFFFFD with count 0 and an output buffer of sizeof(vx_dac_req) reads the DAC back (first = bits per component, rgb = raw components). */
#define VX_DAC_ABI 1UL
typedef struct {unsigned long cb,abi,first,count,rgb[256];} vx_dac_req; /* rgb: 00RRGGBB, no pointers */
#define VX_FB_LIMIT (64UL*1024UL*1024UL) /* v10.73: was 16 MB (v10.54: 4 MB). Largest video memory the driver hands to DirectDraw, modes and the 2D engine; the engine reaches the whole window (VRAM REACH test), the real size comes from the host bridge, registry VIDMEM_MB lowers it */
#define VX_FB_MAP_LIMIT (512UL*1024UL*1024UL) /* v10.04: CPU mapping of the video memory window; modes and the FBHDA still use VX_FB_LIMIT */
#define VX3DN_CMD_BYTES 0x40000UL /* v10.228: the 3D command block (256 KB) the command fetcher reads draw packets from (vx_native3d_vxd.c fetch_draw) */
#define VX_SHADOW_MB 8
#define VX_LOG_CAPACITY 32768UL

enum vx_failure {
 VX_OK=0, VX_NO_DEVICE, VX_MULTIPLE_VGA, VX_BAD_PCI, VX_BAD_ROM,
 VX_NO_BIOS_BUFFER, VX_BAD_VBE, VX_NO_MODES, VX_BAD_MODE_TABLE,
 VX_NO_MODE_STORAGE, VX_NO_FB_MAP, VX_NO_SHADOW, VX_NO_HDA,
 VX_MODE_REJECTED, VX_BIOS_MODE_FAILED, VX_NO_SELECTOR, VX_NO_VDD_TABLE,
 VX_LATE_LOAD
};

typedef struct vx_status {
 uint32_t magic, cb, version, stage, failure;
 uint32_t pci_address, pci_command, bar0, bar1, rom_fingerprint;
 uint32_t fb_phys, fb_bytes, fb_linear, shadow_linear, shadow_bytes;
 uint32_t vbe_version, mode_count, active_mode, width, height, bpp;
 uint32_t shadow_pitch, video_pitch, set_attempts, set_successes, last_bios_ax;
 uint32_t draws, log_bytes, log_dropped, bios_buffer, vga_mode;
} vx_status_t;

#ifdef VXD32
extern vx_status_t vx_status;
extern volatile unsigned long vx_native_display_hold;
extern unsigned long vx_vbe_total_bytes, vx_chip_fb_bytes, vx_map_bytes;
unsigned long VX_pci_read(unsigned long address, unsigned long offset);
extern volatile unsigned long vx_native_display_skips;
int VESA_native_span(uint32_t *,uint32_t *);
void VX_log_char(char c);
unsigned long VX_log_copy(void *out, unsigned long bytes);
void VX_snapshot(vx_status_t *out);
int VX_check_hardware(void);
int VX_mode_allowed(unsigned long id, unsigned long w,
 unsigned long h, unsigned long bpp);
int VX_mode_layout(unsigned long version, unsigned long id, const void *info,
 unsigned long *pitch);
#endif
#endif
