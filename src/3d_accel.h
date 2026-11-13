/*****************************************************************************

Copyright (c) 2024 Jaroslav Hensl <emulator@emulace.cz>

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

/* const and types for GPU acceleration */

#ifndef __3D_ACCEL_H__
#define __3D_ACCEL_H__

#ifdef __WATCOMC__
#ifndef VXD32
#define FBHDA_SIXTEEN
#endif
#endif

#define API_3DACCEL_VER 20260213

#define ESCAPE_DRV_NT         0x1103 /* (4355) */

/* function codes */
#define OP_FBHDA_SETUP        0x110B /* VXD, DRV, ExtEscape, VxDCall */
#define OP_FBHDA_ACCESS_BEGIN 0x110C /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_FBHDA_ACCESS_END   0x110D /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_FBHDA_SWAP         0x110E /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_FBHDA_CLEAN        0x110F /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_FBHDA_PALETTE_SET  0x1110 /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_FBHDA_PALETTE_GET  0x1111 /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_FBHDA_ACCESS_RECT  0x1112 /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_FBHDA_OVERLAY_SETUP  0x1113 /* VXD, VxDCall, ESCAPE_DRV_NT */
#define OP_FBHDA_OVERLAY_LOCK   0x1114 /* VXD, VxDCall, ESCAPE_DRV_NT */
#define OP_FBHDA_OVERLAY_UNLOCK 0x1115 /* VXD, VxDCall, ESCAPE_DRV_NT */

#define OP_FBHDA_GAMMA_SET    0x1116 /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_FBHDA_GAMMA_GET    0x1117 /* VXD, DRV, ESCAPE_DRV_NT */

#define OP_FBHDA_PAGE_MOD     0x1118 /* VXD */
#define OP_FBHDA_MODE_QUERY   0x1119 /* VXD */
#define OP_FBHDA_REFRESH      0x1120 /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_FBHDA_PERIOD       0x1121 /* VXD, ESCAPE_DRV_NT */

#define OP_FBHDA_SURFACE_GET  0x1122 /* VXD, ESCAPE_DRV_NT */
#define OP_FBHDA_SURFACE_SET  0x1123 /* VXD, ESCAPE_DRV_NT */
#define OP_FBHDA_SURFACE_DELETE 0x1124 /* VXD, ESCAPE_DRV_NT */
#define OP_FBHDA_SURFACE_MODIFY 0x1125 /* VXD, ESCAPE_DRV_NT */
#define OP_FBHDA_SURFACE_NOTIFY 0x1126 /* VXD, ESCAPE_DRV_NT */
#define OP_FBHDA_SURFACE_WATCH  0x1127 /* VXD, ESCAPE_DRV_NT */
#define OP_VX_FILL            0x1130 /* VXD, DRV: EBX=(y<<16)|x ECX=(h<<16)|w ESI=colour EDI=ROP3 byte -> ECX=1 done */
#define OP_VX_COPY            0x1131 /* VXD, DRV: EBX=(dy<<16)|dx ECX=(h<<16)|w ESI=(sy<<16)|sx -> ECX=1 done */

#define OP_SVGA_VALID         0x2000  /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_SVGA_SETMODE       0x2001  /* DRV */
#define OP_SVGA_VALIDMODE     0x2002  /* DRV */
#define OP_SVGA_HW_ENABLE     0x2003  /* DRV */
#define OP_SVGA_HW_DISABLE    0x2004  /* DRV */
#define OP_SVGA_CMB_ALLOC     0x2005  /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_SVGA_CMB_FREE      0x2006  /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_SVGA_CMB_SUBMIT    0x2007  /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_SVGA_FENCE_GET     0x2008  /* VXD */
#define OP_SVGA_FENCE_QUERY   0x2009  /* VXD */
#define OP_SVGA_FENCE_WAIT    0x200A  /* VXD */
#define OP_SVGA_REGION_CREATE 0x200B  /* VXD */
#define OP_SVGA_REGION_FREE   0x200C  /* VXD */
#define OP_SVGA_QUERY         0x200D  /* VXD */
#define OP_SVGA_QUERY_VECTOR  0x200E  /* VXD */
#define OP_SVGA_DB_SETUP      0x200F  /* VXD */
#define OP_SVGA_OT_SETUP      0x2010  /* VXD */
#define OP_SVGA_FLUSHCACHE    0x2011  /* VXD */
#define OP_SVGA_VXDCMD        0x2012  /* VXD */

#define OP_VBE_VALID          0x3000 /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_VBE_SETMODE        0x3001 /* DRV */
#define OP_VBE_VALIDMODE      0x3002 /* DRV */

#define OP_VESA_VALID         0x4000 /* VXD, DRV, ESCAPE_DRV_NT */
#define OP_VESA_SETMODE       0x4001 /* DRV */
#define OP_VESA_VALIDMODE     0x4002 /* DRV */
#define OP_VESA_HIRES         0x4003 /* DRV */

#define OP_MOUSE_BUFFER       0x1F00 /* DRV */
#define OP_MOUSE_LOAD         0x1F01 /* DRV */         
#define OP_MOUSE_MOVE         0x1F02 /* DRV */
#define OP_MOUSE_SHOW         0x1F03 /* DRV */
#define OP_MOUSE_HIDE         0x1F04 /* DRV */

/* VXDCall */
#define FBHDA_DEVICE_ID       0x4333
#define FBHDA_SERVICE_TABLE_OFFSET 0x110A
#define FBHDA__GET_VERSION 0
#define FBHDA__SETUP (OP_FBHDA_SETUP-FBHDA_SERVICE_TABLE_OFFSET)
#define FBHDA__OVERLAY_SETUP (OP_FBHDA_OVERLAY_SETUP-FBHDA_SERVICE_TABLE_OFFSET)
#define FBHDA__OVERLAY_LOCK (OP_FBHDA_OVERLAY_LOCK-FBHDA_SERVICE_TABLE_OFFSET)
#define FBHDA__OVERLAY_UNLOCK (OP_FBHDA_OVERLAY_UNLOCK-FBHDA_SERVICE_TABLE_OFFSET)

#pragma pack(push)
#pragma pack(1)

#ifdef FBHDA_SIXTEEN
# define FBPTR __far*
#else
# define FBPTR *
#endif

#define FBHDA_OVERLAYS_MAX 16
//#define FBHDA_ROW_ALIGN 8
#define FBHDA_ROW_ALIGN 4

typedef struct FBHDA_overlay
{
#ifndef FBHDA_SIXTEEN
	void *ptr;
#else
	DWORD ptr32;
#endif
	DWORD size;
} FBHDA_overlay_t;

typedef struct _FBHDA_DD_fifo_item_t
{
	void *surface_flat;
	DWORD uid;
	DWORD action;
} FBHDA_DD_fifo_item_t;

typedef struct FBHDA
{
	         DWORD cb;
           DWORD flags;
           DWORD version;
	volatile DWORD width;
	volatile DWORD height;
	volatile DWORD bpp;
	volatile DWORD pitch;
	volatile DWORD surface;
	volatile DWORD stride;
	volatile DWORD onflip;
#ifndef FBHDA_SIXTEEN
	         void *vram_pm32; /* frame buffer address */
	         DWORD vram_pm16; 
	         void *vram_phylin; /* frame buffer mapped phy address */
#else
           DWORD       vram_pm32;
           void __far *vram_pm16;
           DWORD       vram_phylin;
#endif
	         DWORD vram_size; /* real r/w memory size */
	         DWORD vram_size_bar; /* PCI region size, may be larger then vram_size */
	         DWORD vram_size_virt; /* virtual memory size (inc. textures) to reported to apps */
	         char vxdname[16]; /* file name or "NT" */
	         DWORD overlay;
	         FBHDA_overlay_t overlays[FBHDA_OVERLAYS_MAX];
	         DWORD overlays_size;
	         DWORD gamma; /* fixed decimal point, 65536 = 1.0 */
	         DWORD system_surface;
	         DWORD palette_update; /* INC by one everytime when the palette is updated */
	         DWORD gamma_update; /* INC by one everytime when the pallete is updated */
	         DWORD gpu_mem_total;
	         DWORD gpu_mem_used;
	         /* direct draw helpers */
#ifndef FBHDA_SIXTEEN
	         FBHDA_DD_fifo_item_t *dd_fifo;
#else
           DWORD dd_fifo;
#endif
	         DWORD dd_fifo_length; /* in elements */
	volatile DWORD dd_fifo_top; /* point TO last element */
	         DWORD rem2;
	         DWORD rem3;
	         DWORD rem4;
	         /* reserved */
	         DWORD res0;
	         DWORD res1;
	         DWORD res2;
	         DWORD res3;
} FBHDA_t;

/* v10.258: the legacy mono hardware cursor (0x2D0) takes its 1 KB pattern's address in bits 25:10 (the manual): the first 64 MB only, while the
 * HI cursor block sits at the top of video memory. The pattern lives in 1 KB right after the primary surface (1 KB aligned); the DirectDraw heap
 * (dddrv.c) starts after it. VX_PRIM_BYTES = the primary's size as dddrv.c rounds it (pages, then whole lines, for the VESA modes). */
#define VX_PRIM_ROUND(s) (((s) + 0x00000FFFUL) & 0xFFFFF000UL) /* v10.302 (review): the mask was 28 bits */
#define VX_PRIM_BYTES(h) ((((h)->flags & FB_VESA_MODES) && (h)->pitch) ? (VX_PRIM_ROUND((h)->stride) + ((VX_PRIM_ROUND((h)->stride) % (h)->pitch) ? (h)->pitch - VX_PRIM_ROUND((h)->stride) % (h)->pitch : 0UL)) : (h)->stride)
#define VX_MONO_CURSOR_OFF(h) (((h)->system_surface + VX_PRIM_BYTES(h) + 1023UL) & ~1023UL)
#define VX_MONO_CURSOR_BYTES 1024UL

#define FBHDA_MODE_MAX_REFRESH 16

typedef struct FBHDA_mode
{
	         DWORD cb;
	         DWORD width;
	         DWORD height;
	         DWORD bpp;
	         DWORD refresh[FBHDA_MODE_MAX_REFRESH];
} FBHDA_mode_t;

#define FB_VRAM_HEAP_GRANULARITY (4*32)
/* minimum of vram allocation (32px at 32bpp, or 64px at 16bpp) */

#define FB_SUPPORT_FLIPING     1
#define FB_ACCEL_VIRGE         2
#define FB_ACCEL_CHROMIUM      4
#define FB_ACCEL_QEMU3DFX      8
#define FB_ACCEL_VMSVGA        16
#define FB_ACCEL_VMSVGA3D      32
#define FB_ACCEL_VMSVGA10      64
#define FB_MOUSE_NO_BLIT      128
#define FB_FORCE_SOFTWARE     256
#define FB_ACCEL_VMSVGA10_ST  512 /* not used */
#define FB_BUG_VMWARE_UPDATE 1024
#define FB_ACCEL_GPUMEM      2048
#define FB_VESA_MODES        8192
#define FB_SUPPORT_VSYNC    16384
#define FB_SUPPORT_CLOCK    32768
#define FB_SUPPORT_TRIPLE   65536
#define FB_ACCEL_VIRGL     131072
#define FB_VX_DCI_OFF      0x00100000UL /* v10.250: ACCEL_DCI = 0: the DIB engine's DCI primary surface is passed on unchanged */

/* for internal use in RING-0 by VXD only */
BOOL FBHDA_init_hw(); 
void FBHDA_release_hw();

/* for internal use by RING-3 application/driver */
void FBHDA_load();
void FBHDA_free();

#ifdef FBHDA_SIXTEEN
	void FBHDA_setup(FBHDA_t __far* __far* FBHDA, DWORD __far* FBHDA_linear);
#else
	FBHDA_t *FBHDA_setup();
#endif

#define FBHDA_ACCESS_RAW_BUFFERING 1
#define FBHDA_ACCESS_MOUSE_MOVE 2
#define FBHDA_ACCESS_SURFACE_DIRTY 4

#define FBHDA_ACCESS_EXCLUSIVE_BEGIN 8
#define FBHDA_ACCESS_EXCLUSIVE_END   16

#define FBHDA_SWAP_NOWAIT     1
#define FBHDA_SWAP_VTRACE     2
#define FBHDA_SWAP_QUERY      4

void FBHDA_access_begin(DWORD flags);
void FBHDA_access_end(DWORD flags);
void FBHDA_access_rect(DWORD left, DWORD top, DWORD right, DWORD bottom);
void FBHDA_clean();
void  FBHDA_palette_set(unsigned char index, DWORD rgb);

/* return pitch or 0 when failed */
DWORD FBHDA_overlay_setup(DWORD overlay, DWORD width, DWORD height, DWORD bpp);
void  FBHDA_overlay_lock(DWORD left, DWORD top, DWORD right, DWORD bottom);
void  FBHDA_overlay_unlock(DWORD flags);

/* format simitar to WINAPI Set/GetDeviceGammaRamp */
BOOL FBHDA_gamma_get(VOID FBPTR ramp, DWORD buffer_size);
BOOL FBHDA_gamma_set(VOID FBPTR ramp, DWORD buffer_size);

/* set refresh rate (Hz), this not affected monitor refresh rate (only for VESA)
 but internal timer, maximum frequency is about 250 Hz, default is 60 Hz */
void FBHDA_refresh(DWORD refresh_rate);
/* get period (in ms) between 2 frames */
DWORD FBHDA_period();

/* mouse */
#ifdef FBHDA_SIXTEEN
void mouse_buffer(void __far* __far* pBuf, DWORD __far* pLinear);
#else
void *mouse_buffer();
#endif
BOOL mouse_load();
void mouse_move(int x, int y);
void mouse_hide();

/* vxd internal */
void mouse_invalidate(); 

#define MOUSE_BUFFER_SIZE 65535

/* helper for some hacks */

/* query resulutions + refresh rate (need FB_VESA_MODES flag set) */

/*
 * VMWare SVGA-II API
 */

/*
 * Bochs VBE Extensions API
 */

/*
 * VESA Video API
 */

BOOL VESA_init_hw(); /* internal for VXD only */

void VESA_HIRES_enable();
void VESA_HIRES_disable();
BOOL VESA_valid();
BOOL VESA_validmode(DWORD w, DWORD h, DWORD bpp);
void VESA_configure_direct(DWORD direct, DWORD wc);
void VESA_configure_modes(DWORD extra);
void VESA_configure_timing(DWORD on);
void VESA_configure_1152(DWORD cvt);
void VESA_configure_noedid(DWORD high_res_on);
BOOL VESA_setmode(DWORD w, DWORD h, DWORD bpp, DWORD rr_min, DWORD rr_max);


/*
 * DirectDraw/X resource management
 */
typedef struct _FBHDA_DD_surface_attrs_t
{
	DWORD flags;
	DWORD palette_offset; /* vram offset */
	DWORD palette_update; /* changes when palette is updated */
	DWORD colorkey_low; /* inclusive */
	DWORD colorkey_high;/* inclusive */
} FBHDA_DD_surface_attrs_t;
 
typedef struct _FBHDA_DD_surface_t
{
	DWORD uid;
	DWORD four_cc;
	DWORD size;
	DWORD width;
	DWORD height;
	DWORD pitch;
	void *data;
	FBHDA_DD_surface_attrs_t attrs;
} FBHDA_DD_surface_t;

typedef void (__stdcall *FBHDA_DD_watch_callback_t)(void *flat, DWORD action);



/* Register user callback for watch surface operation.
 * Callback fired when:
 *   - modify (someone call FBHDA_DD_surface_modify)
 *   - delete (someone call FBHDA_DD_surface_deleted)
 *   - read (someone call FBHDA_DD_surface_notify)
 */

/* notify watchman than we started operation with surface */

#define FBHDA_DD_NOP    0
#define FBHDA_DD_CREATE 1
#define FBHDA_DD_MODIFY 2
#define FBHDA_DD_DELETE 3
#define FBHDA_DD_NOTIFY 4

#define FBHDA_DD_FLAG_TEXTURE  1
#define FBHDA_DD_FLAG_COLORKEY 2
#define FBHDA_DD_FLAG_PALETTE  4

#pragma pack(pop)

/* DLL handlers */
#define VMDISP9X_LIB "vmdisp9x.dll"

typedef FBHDA_t *(__cdecl *FBHDA_setup_t)();
typedef void (__cdecl *FBHDA_access_begin_t)(DWORD flags);
typedef void (__cdecl *FBHDA_access_end_t)(DWORD flags);
typedef void (__cdecl *FBHDA_access_rect_t)(DWORD left, DWORD top, DWORD right, DWORD bottom);
typedef BOOL (__cdecl *FBHDA_swap_t)(DWORD offset, DWORD flags);
typedef BOOL (__cdecl *FBHDA_page_modify_t)(DWORD flat_address, DWORD size, const BYTE *new_data);
typedef void (__cdecl *FBHDA_clean_t)(void);
typedef BOOL (__cdecl *FBHDA_mode_query_t)(DWORD index, FBHDA_mode_t *mode);

typedef BOOL (__cdecl *FBHDA_DD_surface_get_t)(void *flat, FBHDA_DD_surface_t *info);
typedef BOOL (__cdecl *FBHDA_DD_surface_set_t)(void *flat, FBHDA_DD_surface_t *info);
typedef void (__cdecl *FBHDA_DD_surface_delete_t)(void *flat);
typedef BOOL (__cdecl *FBHDA_DD_surface_modify_t)(void *flat);
typedef BOOL (__cdecl *FBHDA_DD_surface_watch_t)(void *flat, FBHDA_DD_watch_callback_t callback);
typedef void (__cdecl *FBHDA_DD_surface_notify_t)(void *flat);

typedef struct _fbhda_lib_t
{
	HMODULE lib;
	LONG lock;
	FBHDA_setup_t pFBHDA_setup;
	FBHDA_access_begin_t pFBHDA_access_begin;
	FBHDA_access_end_t pFBHDA_access_end;
	FBHDA_access_rect_t pFBHDA_access_rect;
	FBHDA_swap_t pFBHDA_swap;
	FBHDA_page_modify_t pFBHDA_page_modify;
	FBHDA_clean_t pFBHDA_clean;
	FBHDA_mode_query_t pFBHDA_mode_query;
	FBHDA_DD_surface_get_t pFBHDA_DD_surface_get;
	FBHDA_DD_surface_set_t pFBHDA_DD_surface_set;
	FBHDA_DD_surface_delete_t pFBHDA_DD_surface_delete;
	FBHDA_DD_surface_modify_t pFBHDA_DD_surface_modify;
	FBHDA_DD_surface_watch_t pFBHDA_DD_surface_watch;
	FBHDA_DD_surface_notify_t pFBHDA_DD_surface_notify;
} fbhda_lib_t;

#endif /* __3D_ACCEL_H__ */
