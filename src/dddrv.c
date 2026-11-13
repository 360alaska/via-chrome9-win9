/*****************************************************************************

Copyright (c) 2023 Jaroslav Hensl <emulator@emulace.cz>

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

/* DirectDraw driver */

#include "winhack.h"
#include <gdidefs.h>
#include <dibeng.h>
#include "minidrv.h"
#include "drvlib.h"
#include "ddrawi.h"
#include <wchar.h> /* wchar_t */
#include <string.h> /* _fmemset */
#include <stddef.h>

#include "3d_accel.h"
#include "vmdahal.h"
#include "pm16_calls.h" /* .578: VX_refresh_hz had no prototype here (.577: its DWORD was taken as an int; the same value for a rate) */

const static DD32BITDRIVERDATA_t drv_vmhal9x = {
	"VXDDRAW.DLL",
	"DriverInit",
	0
};

const static DD32BITDRIVERDATA_t drv_vmhal486 = {
	"VXDDRAW.DLL",
	"DriverInit",
	0
};

/*
 * pre-declare our HAL fns
 */
DWORD __loadds __far __fastcall HALDestroyDriver(LPDDHAL_DESTROYDRIVERDATA);

#define FB_MEM_POOL 16777216UL

/*
 * callbacks from the DIRECTDRAW object
 */
static DDHAL_DDCALLBACKS_t cbDDCallbacks = {
	sizeof(DDHAL_DDCALLBACKS_t),
	0,                          // dwFlags
	HALDestroyDriver,           // DestroyDriver
	NULL,                       // CreateSurface
	NULL,                       // SetColorKey
	NULL,                       // SetMode
	NULL,                       // WaitForVerticalBlank -> uses 32-bit
	NULL,                       // CanCreateSurface
	NULL,                       // CreatePalette
	NULL                        // lpReserved1
};

/*
 * callbacks from the DIRECTDRAWSURFACE object
 */
static DDHAL_DDSURFACECALLBACKS_t cbDDSurfaceCallbacks =
{
	sizeof(DDHAL_DDSURFACECALLBACKS_t),
	0,                          // dwFlags
	NULL,                       // DestroySurface
	NULL,                       // Flip -> uses 32-bit
	NULL,                       // SetClipList
	NULL,                       // Lock -> uses 32-bit
	NULL,                       // Unlock
	NULL,                       // Blt -> uses 32-bit
	NULL,                       // SetColorKey
	NULL,                       // AddAttachedSurface
	NULL,                       // lpReserved
	NULL,                       // lpReserved
	NULL,                       // UpdateOverlay
	NULL,                       // lpReserved
	NULL,                       // lpReserved
	NULL                        // SetPalette
};

/*
 * callbacks from the DIRECTDRAWPALETTE object
 */
static DDHAL_DDPALETTECALLBACKS_t cbDDPaletteCallbacks =
{
	sizeof(DDHAL_DDPALETTECALLBACKS_t),
	0,                          // dwFlags
	NULL,                       // DestroyPalette
	NULL                        // SetEntries
};

static VMDAHAL_t __far *pm16VMDAHAL = NULL;
static DWORD linVMDAHAL = 0;

static LPDDHAL_SETINFO lpDDHAL_SetInfo = NULL;

#pragma code_seg( _INIT )

static BOOL DDGetPtr(VMDAHAL_t __far *__far *pm16ptr, DWORD __far *linear)
{
	if(pm16VMDAHAL == NULL)
	{
		pm16VMDAHAL = drv_malloc(sizeof(VMDAHAL_t), &linVMDAHAL);
		if(pm16VMDAHAL != NULL)
		{
			_fmemset(pm16VMDAHAL, 0, sizeof(VMDAHAL_t));
			pm16VMDAHAL->dwSize = sizeof(VMDAHAL_t);

			pm16VMDAHAL->pFBHDA32 = hda_linear;
			pm16VMDAHAL->pFBHDA16 = hda;
 		 	pm16VMDAHAL->FBHDA_version = 2024;
		}
	}
	
	if(pm16VMDAHAL == NULL)
	{
		return FALSE;
	}
	
	if(pm16ptr != NULL)
	{
		*pm16ptr = pm16VMDAHAL;
	}
	
	if(linear != NULL)
	{
		*linear = linVMDAHAL;
	}
	
	return TRUE;
}

/*
 * buildPixelFormat
 *
 * build DDPIXELFORMAT structure
 */
static void buildPixelFormat(LPDDHALMODEINFO lpMode, LPDDPIXELFORMAT lpddpf)
{
	lpddpf->dwFlags = DDPF_RGB;
	lpddpf->dwRGBBitCount = lpMode->dwBPP;

	if( lpMode->wFlags & DDMODEINFO_PALETTIZED)
	{
		lpddpf->dwFlags  |= DDPF_PALETTEINDEXED8;
	}

	lpddpf->dwRBitMask = lpMode->dwRBitMask;
	lpddpf->dwGBitMask = lpMode->dwGBitMask;
	lpddpf->dwBBitMask = lpMode->dwBBitMask;
	lpddpf->dwRGBAlphaBitMask = lpMode->dwAlphaBitMask;
} /* buildPixelFormat */

#define MAKEFOURCC(_a, _b, _c, _d) (((DWORD)_d << 24) | ((DWORD)_c << 16) | ((DWORD)_b << 8) | ((DWORD)_a))

/*
 * buildDDHALInfo
 *
 * build DDHALInfo structure
 */
/* Only capabilities implemented by VXDDRAW.DLL and the native 2D backend. */
/* v10.58g: original VMDisp9x HAL description (DirectDraw rejected the reduced one) */
static DDHAL_DDEXEBUFCALLBACKS_t cbDDExeBufCallbacks = 
{
	sizeof(DDHAL_DDEXEBUFCALLBACKS_t),
	0,
	NULL,
	NULL,
	NULL,
	NULL,
	NULL
};

static void buildDDHALInfo(VMDAHAL_t __far *hal, int modeidx)
{
	
	int                 ii;
	BOOL                can_flip;
	WORD                heap;
	WORD                bytes_per_pixel;
	DWORD               stride;
	static DWORD    dwpFOURCCs[] = 
	{
		MAKEFOURCC('Y', 'U', 'Y', '2'), /* v10.108: overlay formats (the HAL accepts them only when the overlay is on) */
		MAKEFOURCC('Y', 'V', '1', '2'),
		MAKEFOURCC('I', '4', '2', '0'),
		MAKEFOURCC('I', 'Y', 'U', 'V'),
		MAKEFOURCC('D', 'X', 'T', '1'), /* v10.199: compressed textures (the HAL's texture formats) */
		MAKEFOURCC('D', 'X', 'T', '3'),
		MAKEFOURCC('D', 'X', 'T', '5')
	};
//	DWORD               bufpos;

	LPDWORD pGbl, pHAL;
	LPDDHAL_DDEXEBUFCALLBACKS pExeBuf;

	pGbl = hal->ddHALInfo.lpD3DGlobalDriverData;
	pHAL = hal->ddHALInfo.lpD3DHALCallbacks;
	pExeBuf = hal->ddHALInfo.lpDDExeBufCallbacks;

	// dont trash the 3D callbacks. only the 2D info.
	_fmemset(&hal->ddHALInfo, 0, sizeof(hal->ddHALInfo));
	hal->ddHALInfo.lpD3DHALCallbacks = pHAL;
	hal->ddHALInfo.lpD3DGlobalDriverData = pGbl;

	//clean 3d callback pointers on 8bpps
	if(hda->bpp > 8)
	{
		hal->ddHALInfo.lpDDExeBufCallbacks = pExeBuf;
	}

	bytes_per_pixel = (hda->bpp+7)/8;
	hal->ddHALInfo.dwSize = sizeof(hal->ddHALInfo);

  hal->ddHALInfo.hInstance = hal->hInstance;
	hal->ddHALInfo.lpPDevice = lpDriverPDevice;

	/*
	 * current video mode
	 */
	hal->ddHALInfo.dwModeIndex = (DWORD)modeidx;
	hal->ddHALInfo.dwMonitorFrequency = VX_refresh_hz(); /* .577: GetMonitorFrequency answered 0 */

	/*
	 * current primary surface attributes
	 */
	hal->ddHALInfo.vmiData.fpPrimary       = hda->vram_pm32 + hda->system_surface;
	hal->ddHALInfo.vmiData.dwDisplayWidth  = hda->width;
	hal->ddHALInfo.vmiData.dwDisplayHeight = hda->height;
	hal->ddHALInfo.vmiData.lDisplayPitch   = hda->pitch;

	_fmemset(&hal->ddHALInfo.vmiData.ddpfDisplay, 0, sizeof(hal->ddHALInfo.vmiData.ddpfDisplay));
	hal->ddHALInfo.vmiData.ddpfDisplay.dwSize = sizeof(DDPIXELFORMAT_t);
	if(modeidx >= 0)
	{
		buildPixelFormat(&(VMDAHAL_modes(hal)[modeidx]), &hal->ddHALInfo.vmiData.ddpfDisplay);
	}
	_fmemcpy(&hal->ddpf, &hal->ddHALInfo.vmiData.ddpfDisplay, sizeof(hal->ddpf)); /* v10.152: the 32-bit HAL's D3D checks read the display format here (it was never filled: CreateSurface(3DDEVICE) failed DDERR_UNSUPPORTED) */

	/*
	 * video memory pool information
	 */
	_fmemset(hal->vidMem, 0, sizeof(VIDMEM_t[4]));
	hal->ddHALInfo.vmiData.pvmList = hal->vidMem;

	/*
	 * set up the pointer to the first available video memory after
	 * the primary surface
	 */
	hal->ddHALInfo.vmiData.dwNumHeaps  = 0;
	heap = 0;
	/* v10.59: the 32-bit HAL registers Flip only after the VxD verified display-start flipping.
	 * v10.330 (review #72): Flip is advertised only with that callback; FB_SUPPORT_FLIPING alone (set on the shadow path, DIRECT_VRAM = 0, where the
	 * HAL registers no Flip) advertised flipping that nothing carried out */
	can_flip = hal->cb32.Flip != 0;
	
	stride = hda->stride;

	/* VESA driver need round system surface on pages */
	if(hda->flags & FB_VESA_MODES)
	{
		DWORD m;
		stride = (hda->stride + 0x00000FFFUL) & 0xFFFFF000UL; /* v10.302 (review): the mask was 28 bits */
		m = stride % hda->pitch;
		if(m)
		{
			stride += hda->pitch - m;
		}
	}

	hal->vidMem[0].dwFlags = VIDMEM_ISLINEAR;
	hal->vidMem[0].ddsCaps.dwCaps = 0;//DDSCAPS_OFFSCREENPLAIN; - what this memory CANNOT be used for
	/* v10.258: the mono cursor's 1 KB pattern (VX_MONO_CURSOR_OFF, 3d_accel.h) right after the primary, then the heap (still 256-byte aligned) */
	hal->vidMem[0].fpStart = hda->vram_pm32 + ((hda->system_surface + stride + 1023UL) & ~1023UL) + VX_MONO_CURSOR_BYTES;
	hal->vidMem[0].fpEnd   = hda->vram_pm32 + hda->vram_size_virt - hda->overlays_size - 1;
	/* v10.330 (review #71): a heap only when there is room after the primary, the cursor block and the reserved top (start past end, or the
	 * reserved top as large as the memory, published one before) */
	hal->ddHALInfo.vmiData.dwNumHeaps = (hda->overlays_size < hda->vram_size_virt && hal->vidMem[0].fpStart <= hal->vidMem[0].fpEnd) ? 1 : 0;

	/*
	 * capabilities supported
	 */
	hal->ddHALInfo.ddCaps.dwCaps = DDCAPS_GDI | /* HW is shared with GDI */
	                               DDCAPS_BLT | /* BLT is supported */
                                 DDCAPS_BLTCOLORFILL | /* color fill */
                                 DDCAPS_BLTSTRETCH   | /* stretching blt */
                                 DDCAPS_COLORKEY     | /* transparentBlt */
                                 DDCAPS_CANBLTSYSMEM; /* from to sysmem blt */
	/* DDK: The semantics of the DirectDraw DDCAPS_CANBLTSYSMEM capability bit
	   imply that the display driver is called for all blts from system memory
	   to display memory. Consequently, the driver may be called for such blts
	   from DXT surfaces to non-DXT surfaces. The only requirement in this case
	   is that the driver return DDHAL_DRIVER_NOTHANDLED if it cannot perform
	   the decompression. This causes DirectDraw to propagate a DDERR_UNSUPPORTED
	   error code to the application. It is acceptable to implement decompression
	   for blts from system memory to display memory in your driver, but this
	   is not required for DirectX 6.0 and later versions. */

	hal->ddHALInfo.ddCaps.dwCKeyCaps     = DDCKEYCAPS_SRCBLT | DDCKEYCAPS_DESTBLT;
	hal->ddHALInfo.ddCaps.dwFXCaps       = DDFXCAPS_BLTMIRRORLEFTRIGHT | /* v10.139 (review F09): no arithmetic stretch (the CPU stretch point-samples), no depth fill (no Z buffers), no 'YUV2' FOURCC (no such route) */
	                                       DDFXCAPS_BLTMIRRORUPDOWN |
	                                       DDFXCAPS_BLTSHRINKX |
	                                       DDFXCAPS_BLTSHRINKXN |
	                                       DDFXCAPS_BLTSHRINKY |
	                                       DDFXCAPS_BLTSHRINKYN |
	                                       DDFXCAPS_BLTSTRETCHX | 
	                                       DDFXCAPS_BLTSTRETCHXN |
	                                       DDFXCAPS_BLTSTRETCHY |
	                                       DDFXCAPS_BLTSTRETCHYN;
	                                       // DDFXCAPS_BLTROTATION
	                                       // DDFXCAPS_BLTROTATION90

	hal->ddHALInfo.ddCaps.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;

	/* v10.71: 256-colour palettes (the primary surface and offscreen 8-bit surfaces) */
	if(hda->bpp == 8)
	{
		hal->ddHALInfo.ddCaps.dwCaps |= DDCAPS_PALETTE;
		hal->ddHALInfo.ddCaps.dwPalCaps = 0x00000054UL; /* DDPCAPS_8BIT | DDPCAPS_ALLOW256 | DDPCAPS_PRIMARYSURFACE */
	}
	/* 3D support */
	if(hal->d3dhal_global != NULL)
	{
		hal->ddHALInfo.ddCaps.dwCaps         |= DDCAPS_3D;
		hal->ddHALInfo.ddCaps.dwCaps2        |= hal->d3dhal_flags.caps2; /* v10.150: was '=' */
		
  	hal->ddHALInfo.ddCaps.ddsCaps.dwCaps |= hal->d3dhal_flags.ddscaps;
  	hal->ddHALInfo.ddCaps.dwZBufferBitDepths |= hal->d3dhal_flags.zcaps;

		hal->ddHALInfo.ddCaps.dwAlphaBltConstBitDepths = hal->d3dhal_flags.alpha_const;
		hal->ddHALInfo.ddCaps.dwAlphaBltPixelBitDepths = hal->d3dhal_flags.alpha_pixel;
		hal->ddHALInfo.ddCaps.dwAlphaBltSurfaceBitDepths  = hal->d3dhal_flags.alpha_surface;
  }

	/* v10.355: IDirectDrawSurface7::Blt does not implement DDBLT_ALPHA flags.
	 * Internal CPU arithmetic is not a public API capability. Do not promise
	 * alpha blits or separate alpha surfaces that applications cannot use. */
	hal->ddHALInfo.ddCaps.dwCaps &= ~DDCAPS_ALPHA;
	hal->ddHALInfo.ddCaps.ddsCaps.dwCaps &= ~DDSCAPS_ALPHA;
	hal->ddHALInfo.ddCaps.dwAlphaBltConstBitDepths = 0;
	hal->ddHALInfo.ddCaps.dwAlphaBltPixelBitDepths = 0;
	hal->ddHALInfo.ddCaps.dwAlphaBltSurfaceBitDepths = 0;
	if(can_flip)
	{
		hal->ddHALInfo.ddCaps.ddsCaps.dwCaps |= DDSCAPS_FLIP | DDSCAPS_PRIMARYSURFACE | DDSCAPS_BACKBUFFER;
		/* v10.59: the 32-bit HAL honours DDFLIP_NOVSYNC and DDFLIP_INTERVALn (DirectDraw strips them unless advertised) */
		hal->ddHALInfo.ddCaps.dwCaps2 |= 0x00200000UL /* DDCAPS2_FLIPINTERVAL */ | 0x00400000UL /* DDCAPS2_FLIPNOVSYNC */;
	}

	/* v10.74: offscreen surfaces wider than the screen (DirectDraw refuses them with E_INVALIDARG unless this is advertised).
	 * VXCreate still limits the pitch to 0x3FF8 bytes. Set after every other dwCaps2 assignment so nothing clears it. */
	hal->ddHALInfo.ddCaps.dwCaps2 |= DDCAPS2_WIDESURFACES;

  /*
   * fill in rops supported
   */
	for(ii=0; ii<DD_ROP_SPACE; ii++)
	{
		hal->ddHALInfo.ddCaps.dwRops[ii] = 0xFFFFFFFFUL;// JH: we have all ROP3s :-)
	}

	/*
	 * required alignments of the scan lines for each kind of memory
	 */
	/* Native 3D target/depth origins must be 256-byte aligned. Preserve the
	 * original 2D allocation policy when the experimental HAL is disabled. */
	hal->ddHALInfo.vmiData.dwOffscreenAlign = hal->d3dhal_global ? 256UL : FBHDA_ROW_ALIGN;
	// ^ align offscreen surface to pages
	hal->ddHALInfo.vmiData.dwOverlayAlign = FBHDA_ROW_ALIGN;
	hal->ddHALInfo.vmiData.dwTextureAlign = hal->d3dhal_global ? 256UL : FBHDA_ROW_ALIGN; /* v10.169: texture bases are given to the sampler >> 8 */
	hal->ddHALInfo.vmiData.dwZBufferAlign = hal->d3dhal_global ? 256UL : FBHDA_ROW_ALIGN;

	/*
	 * callback functions
	 */
	_fmemset(&cbDDCallbacks, 0, sizeof(cbDDCallbacks));
	_fmemset(&cbDDSurfaceCallbacks, 0, sizeof(cbDDSurfaceCallbacks));
	_fmemset(&cbDDPaletteCallbacks, 0, sizeof(cbDDPaletteCallbacks));

	cbDDCallbacks.dwSize = sizeof(cbDDCallbacks);
	cbDDCallbacks.DestroyDriver = (LPDDHAL_DESTROYDRIVER) HALDestroyDriver;
	
	cbDDSurfaceCallbacks.dwSize = sizeof(cbDDSurfaceCallbacks);
	cbDDPaletteCallbacks.dwSize = sizeof(cbDDPaletteCallbacks);

	hal->ddHALInfo.lpDDCallbacks        = &cbDDCallbacks;
	hal->ddHALInfo.lpDDSurfaceCallbacks = &cbDDSurfaceCallbacks;
	hal->ddHALInfo.lpDDPaletteCallbacks = &cbDDPaletteCallbacks;

	/*
	 *  FOURCC
	 */
	hal->ddHALInfo.ddCaps.dwNumFourCCCodes = sizeof(dwpFOURCCs)/sizeof(DWORD);
	hal->ddHALInfo.lpdwFourCC = &dwpFOURCCs[0];

  hal->ddHALInfo.ddCaps.dwMinOverlayStretch   = 1000; /* v10.112: lowered to 63 (shrink down to 1/16, the HQV limit) when the overlay is on, below */
	hal->ddHALInfo.ddCaps.dwMinLiveVideoStretch = 1000;
	hal->ddHALInfo.ddCaps.dwMinHwCodecStretch   = 1000;
	hal->ddHALInfo.ddCaps.dwMaxOverlayStretch   = 9999;
	hal->ddHALInfo.ddCaps.dwMaxLiveVideoStretch = 9999;
	hal->ddHALInfo.ddCaps.dwMaxHwCodecStretch   = 9999;

	/* v10.106: video overlay (V1), advertised only when the 32-bit HAL found ACCEL_OVERLAY set in the VxD (it then fills cb32.UpdateOverlay): YUY2 surfaces (and RGB565 when enabled),
	 * destination colour key, stretching (no shrinking), one visible overlay. */
	if(hal->getScanLine32) hal->ddHALInfo.ddCaps.dwCaps |= DDCAPS_READSCANLINE; /* v10.139 (review F09): GetScanLine is offered (registry SCANLINE 1) */
	if(hal->cb32.UpdateOverlay)
	{
		hal->ddHALInfo.ddCaps.dwCaps |= DDCAPS_OVERLAY | DDCAPS_OVERLAYFOURCC | DDCAPS_OVERLAYSTRETCH;
		hal->ddHALInfo.ddCaps.dwCKeyCaps |= DDCKEYCAPS_DESTOVERLAY | DDCKEYCAPS_DESTOVERLAYONEACTIVE | DDCKEYCAPS_SRCOVERLAY | DDCKEYCAPS_SRCOVERLAYONEACTIVE | DDCKEYCAPS_SRCOVERLAYYUV;
		hal->ddHALInfo.ddCaps.dwCaps2 |= DDCAPS2_CANBOBINTERLEAVED | DDCAPS2_CANBOBNONINTERLEAVED;
		hal->ddHALInfo.ddCaps.dwFXCaps |= DDFXCAPS_OVERLAYSTRETCHX | DDFXCAPS_OVERLAYSTRETCHY;
		hal->ddHALInfo.ddCaps.ddsCaps.dwCaps |= DDSCAPS_OVERLAY;
		hal->ddHALInfo.ddCaps.dwMaxVisibleOverlays = 1;
		hal->ddHALInfo.ddCaps.dwMaxOverlayStretch = 9999;
		hal->ddHALInfo.ddCaps.dwMinOverlayStretch = 63; /* v10.112: shrinking runs through the HQV (the HAL refuses it when that is off) */
		hal->ddHALInfo.ddCaps.dwFXCaps |= DDFXCAPS_OVERLAYSHRINKX | DDFXCAPS_OVERLAYSHRINKY;
		hal->ddHALInfo.ddCaps.dwCaps2 |= DDCAPS2_CANBOBHARDWARE;
		hal->ddHALInfo.ddCaps.dwCaps2 |= DDCAPS2_CANFLIPODDEVEN; /* v10.115: bob without a video port: the runtime refused DDOVER_BOB (80070057) without it; fields come with DDFLIP_ODD/DDFLIP_EVEN */
		if(hal->cb32.Flip) hal->ddHALInfo.ddCaps.ddsCaps.dwCaps |= DDSCAPS_FLIP | DDSCAPS_BACKBUFFER;
	}

	/*
	 * mode information
   */
	hal->ddHALInfo.dwNumModes = hal->modes_count;
	hal->ddHALInfo.lpModeInfo = VMDAHAL_modes(hal);
	
	/*
	 * D3D HAL
	 */
	hal->ddHALInfo.lpD3DGlobalDriverData = (ULONG_PTR)hal->d3dhal_global;
	hal->ddHALInfo.lpD3DHALCallbacks     = (ULONG_PTR)hal->d3dhal_callbacks;
	
	/*
	 * Exec Buffers (active when fill)
	 */
	_fmemset(&cbDDExeBufCallbacks, 0, sizeof(DDHAL_DDEXEBUFCALLBACKS_t));
	cbDDExeBufCallbacks.dwSize = sizeof(DDHAL_DDEXEBUFCALLBACKS_t);
	if(hal->ddHALInfo.lpDDExeBufCallbacks == NULL)
	{
		hal->ddHALInfo.lpDDExeBufCallbacks = &cbDDExeBufCallbacks;
	}
} /* buildDDHALInfo */

/* Verified against the PE DLL by verification/verify.py. */
#include "vx855_2d.h"
const DWORD VXDDABI[]={sizeof(VMDAHAL_t),offsetof(VMDAHAL_t,cb32),offsetof(VMDAHAL_t,modes),
 sizeof(DDHALINFO_t),sizeof(DDHALMODEINFO2_t),sizeof(DDPIXELFORMAT_t),sizeof(vx2d_request)};

static WORD syn_valid; static DWORD syn_w, syn_h, syn_bpp; /* v10.330 (review #182): the one-entry table made below, while it stands */
BOOL DDCreateDriverObject(int bReset)
{
	VMDAHAL_t __far *hal;
	int modeidx = -1;
	
	
	if(lpDDHAL_SetInfo == NULL || (wBpp!=8 && wBpp!=16 && wBpp!=32))
	{
		
		dbg_printf("DDCreateDriverObject: lpDDHAL_SetInfo = NULL\n");
		return FALSE;
	}
	
	if(!DDGetPtr(&hal, NULL))
	{
		
		dbg_printf("DDCreateDriverObject: DDGetPtr = NULL\n");
		return FALSE;
	}

	/* v10.330 (review #182): a table this function made itself (one entry) is made again for the current mode after a mode change; one the
	 * 32-bit DLL filled is left as it is */
	if(!(syn_valid && hal->modes_count == 1 && hal->modes[0].dwWidth == syn_w && hal->modes[0].dwHeight == syn_h && hal->modes[0].dwBPP == syn_bpp))
	{
		syn_valid = 0; /* the 32-bit DLL's table (or none yet) */
	}
	else if(syn_w != hda->width || syn_h != hda->height || syn_bpp != hda->bpp)
	{
		hal->modes_count = 0; /* ours, for the mode before */
	}
	if(hal->modes_count == 0)
	{
		/* no 32-bit DLL filled the mode table: describe the current mode only */
		DDHALMODEINFO2_t __far *m = &hal->modes[0];
		_fmemset(m, 0, sizeof(*m));
		m->dwWidth = hda->width; m->dwHeight = hda->height; m->dwBPP = hda->bpp;
		m->lPitch = hda->pitch;
		m->wFlags = hda->bpp == 8 ? DDMODEINFO_PALETTIZED : 0;
		m->dwRBitMask = hda->bpp == 8 ? 0 : (hda->bpp == 16) ? 0xF800UL : 0xFF0000UL;
		m->dwGBitMask = hda->bpp == 8 ? 0 : (hda->bpp == 16) ? 0x07E0UL : 0x00FF00UL;
		m->dwBBitMask = hda->bpp == 8 ? 0 : (hda->bpp == 16) ? 0x001FUL : 0x0000FFUL;
		hal->modes_count = 1;
		syn_valid = 1; syn_w = hda->width; syn_h = hda->height; syn_bpp = hda->bpp;
	}
	for(modeidx = 0; modeidx < hal->modes_count; modeidx++)
	{
		if((hal->modes[modeidx].dwWidth == hda->width) &&
       (hal->modes[modeidx].dwHeight == hda->height) &&
       (hal->modes[modeidx].dwBPP == hda->bpp) )
    {
    	break;
    }
	}
	
	if(modeidx == hal->modes_count)
	{
		
		
		return FALSE;
	}
	
	hal->vramLinear = hda->vram_pm32;
	hal->vramSize   = hda->vram_size_virt;

  /*
   * set up hal info
   */
	buildDDHALInfo(hal, modeidx);

  /*
   * copy new data into area of memory shared with 32-bit DLL
   */
	hal->dwWidth  = hda->width;
	hal->dwHeight = hda->height;
	hal->dwBpp = hda->bpp;
	hal->dwPitch = hda->pitch;
  /*
   * get addresses of 32-bit routines
   */
  /* v10.71: palette callbacks, wired at every depth; the 32-bit HAL declines them outside 8bpp */
  if(hal->createPalette32) {cbDDCallbacks.CreatePalette = hal->createPalette32; cbDDCallbacks.dwFlags |= DDHAL_CB32_CREATEPALETTE;}
  if(hal->destroyPalette32) {cbDDPaletteCallbacks.DestroyPalette = hal->destroyPalette32; cbDDPaletteCallbacks.dwFlags |= DDHAL_PALCB32_DESTROYPALETTE;}
  if(hal->setEntries32) {cbDDPaletteCallbacks.SetEntries = hal->setEntries32; cbDDPaletteCallbacks.dwFlags |= DDHAL_PALCB32_SETENTRIES;}
  if(hal->setPalette32) {cbDDSurfaceCallbacks.SetPalette = hal->setPalette32; cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_SETPALETTE;}
  if(hal->cb32.CanCreateSurface)
  {
		cbDDCallbacks.CanCreateSurface = hal->cb32.CanCreateSurface;
		cbDDCallbacks.dwFlags |= DDHAL_CB32_CANCREATESURFACE;
	}

	if(hal->cb32.CreateSurface)
	{
		cbDDCallbacks.CreateSurface = hal->cb32.CreateSurface;	
		cbDDCallbacks.dwFlags |= DDHAL_CB32_CREATESURFACE;
	}

	if(hal->cb32.Blt)
	{
		cbDDSurfaceCallbacks.Blt = hal->cb32.Blt;
		cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_BLT;
	}

	if(hal->cb32.Flip)
	{
		cbDDSurfaceCallbacks.Flip = hal->cb32.Flip;
		cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_FLIP;
	}

	if(hal->cb32.Lock)
	{
		cbDDSurfaceCallbacks.Lock = hal->cb32.Lock;
		cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_LOCK;
	}

	if(hal->cb32.Unlock)
	{
		cbDDSurfaceCallbacks.Unlock = hal->cb32.Unlock;
		cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_UNLOCK;
	}

	if(hal->cb32.GetBltStatus)
	{
		cbDDSurfaceCallbacks.GetBltStatus = hal->cb32.GetBltStatus;
		cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_GETBLTSTATUS;
	}

	if(hal->cb32.GetFlipStatus)
	{
		cbDDSurfaceCallbacks.GetFlipStatus = hal->cb32.GetFlipStatus;
		cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_GETFLIPSTATUS;
	}

	if(hal->cb32.DestroySurface)
	{
		cbDDSurfaceCallbacks.DestroySurface = hal->cb32.DestroySurface;
		cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_DESTROYSURFACE;
	}
	
	if(hal->cb32.GetDriverInfo)
	{
		hal->ddHALInfo.GetDriverInfo = hal->cb32.GetDriverInfo;
		hal->ddHALInfo.dwFlags |= DDHALINFO_GETDRIVERINFOSET;
	}
	
	hal->ddHALInfo.dwFlags |= hal->cb32.flags;
	hal->ddHALInfo.dwFlags |= (DDHALINFO_ISPRIMARYDISPLAY | DDHALINFO_MODEXILLEGAL); /* v10.58o: also without the 32-bit DLL */
	
	if(hal->cb32.WaitForVerticalBlank)
	{
		cbDDCallbacks.WaitForVerticalBlank = hal->cb32.WaitForVerticalBlank;
		cbDDCallbacks.dwFlags |= DDHAL_CB32_WAITFORVERTICALBLANK;
	}

	if(hal->getScanLine32)
	{
		cbDDCallbacks.GetScanLine = hal->getScanLine32; /* v10.73 */
		cbDDCallbacks.dwFlags |= DDHAL_CB32_GETSCANLINE;
	}

	if(hal->cb32.SetMode)
	{
		cbDDCallbacks.SetMode = hal->cb32.SetMode;
		cbDDCallbacks.dwFlags |= DDHAL_CB32_SETMODE;
	}

	if(hal->cb32.SetExclusiveMode)
	{
		cbDDCallbacks.SetExclusiveMode = hal->cb32.SetExclusiveMode;
		cbDDCallbacks.dwFlags |= DDHAL_CB32_SETEXCLUSIVEMODE;
	}

	if(hal->cb32.FlipToGDISurface)
	{
		cbDDCallbacks.FlipToGDISurface = hal->cb32.FlipToGDISurface;
		cbDDCallbacks.dwFlags |= DDHAL_CB32_FLIPTOGDISURFACE;
	}

	if(hal->cb32.SetColorKey)
	{
		cbDDSurfaceCallbacks.SetColorKey = hal->cb32.SetColorKey;
		cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_SETCOLORKEY;
	}

	if(hal->cb32.AddAttachedSurface)
	{
		cbDDSurfaceCallbacks.AddAttachedSurface = hal->cb32.AddAttachedSurface;
		cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_ADDATTACHEDSURFACE;
	}

	if(hal->cb32.SetOverlayPosition)
	{
		cbDDSurfaceCallbacks.SetOverlayPosition = hal->cb32.SetOverlayPosition;
		cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_SETOVERLAYPOSITION;
	}

	if(hal->cb32.UpdateOverlay)
	{
		cbDDSurfaceCallbacks.UpdateOverlay = hal->cb32.UpdateOverlay; /* v10.106 */
		cbDDSurfaceCallbacks.dwFlags |= DDHAL_SURFCB32_UPDATEOVERLAY;
	}

	if(hal->cb32.DestroyDriver)
	{
		cbDDCallbacks.DestroyDriver = hal->cb32.DestroyDriver;
		cbDDCallbacks.dwFlags |= DDHAL_CB32_DESTROYDRIVER;
	}
	
	if(hal->ddHALInfo.lpDDExeBufCallbacks)
	{
		if(hal->d3dhal_exebuffcallbacks.CanCreateExecuteBuffer)
		{
			hal->ddHALInfo.lpDDExeBufCallbacks->CanCreateExecuteBuffer = hal->d3dhal_exebuffcallbacks.CanCreateExecuteBuffer;
			hal->ddHALInfo.lpDDExeBufCallbacks->dwFlags |= DDHAL_EXEBUFCB32_CANCREATEEXEBUF;
		}

		if(hal->d3dhal_exebuffcallbacks.CreateExecuteBuffer)
		{
			hal->ddHALInfo.lpDDExeBufCallbacks->CreateExecuteBuffer = hal->d3dhal_exebuffcallbacks.CreateExecuteBuffer;
			hal->ddHALInfo.lpDDExeBufCallbacks->dwFlags |= DDHAL_EXEBUFCB32_CREATEEXEBUF;
		}

		if(hal->d3dhal_exebuffcallbacks.DestroyExecuteBuffer)
		{
			hal->ddHALInfo.lpDDExeBufCallbacks->DestroyExecuteBuffer = hal->d3dhal_exebuffcallbacks.DestroyExecuteBuffer;
			hal->ddHALInfo.lpDDExeBufCallbacks->dwFlags |= DDHAL_EXEBUFCB32_DESTROYEXEBUF;
		}

		if(hal->d3dhal_exebuffcallbacks.LockExecuteBuffer)
		{
			hal->ddHALInfo.lpDDExeBufCallbacks->LockExecuteBuffer = hal->d3dhal_exebuffcallbacks.LockExecuteBuffer;
			hal->ddHALInfo.lpDDExeBufCallbacks->dwFlags |= DDHAL_EXEBUFCB32_LOCKEXEBUF;
		}

		if(hal->d3dhal_exebuffcallbacks.UnlockExecuteBuffer)
		{
			hal->ddHALInfo.lpDDExeBufCallbacks->UnlockExecuteBuffer = hal->d3dhal_exebuffcallbacks.UnlockExecuteBuffer;
			hal->ddHALInfo.lpDDExeBufCallbacks->dwFlags |= DDHAL_EXEBUFCB32_UNLOCKEXEBUF;
		}
	}

	dbg_printf("Dump DD32 calls\n");
	dbg_printf("  CanCreateSurface = %lX\n", cbDDCallbacks.CanCreateSurface);
	dbg_printf("  CreateSurface = %lX\n", cbDDCallbacks.CreateSurface);
	dbg_printf("  Blt = %lX\n", cbDDSurfaceCallbacks.Blt);
	dbg_printf("  Flip = %lX\n", cbDDSurfaceCallbacks.Flip);
	dbg_printf("  Lock = %lX\n", cbDDSurfaceCallbacks.Lock);
	dbg_printf("  Unlock = %lX\n", cbDDSurfaceCallbacks.Unlock);
	dbg_printf("  GetBltStatus = %lX\n", cbDDSurfaceCallbacks.GetBltStatus);
	dbg_printf("  GetFlipStatus = %lX\n", cbDDSurfaceCallbacks.GetFlipStatus);
	dbg_printf("  DestroySurface = %lX\n", cbDDSurfaceCallbacks.DestroySurface);
	dbg_printf("  GetDriverInfo = %lX\n", hal->ddHALInfo.GetDriverInfo);
	dbg_printf("  WaitForVerticalBlank = %lX\n", cbDDCallbacks.WaitForVerticalBlank);
	dbg_printf("  DestroyDriver = %lX\n\n", cbDDCallbacks.DestroyDriver);

	{
		BOOL sr;
		
		
		
		
		
		
		sr = lpDDHAL_SetInfo(&(hal->ddHALInfo), bReset);
		hal->vxBuilds++; hal->vxSetInfo = (sr ? 1UL : 0UL) | (hal->ddHALInfo.lpD3DGlobalDriverData ? 0x100UL : 0UL); /* v10.150: read back by VXDX7 */
		hal->vxBuiltCaps = hal->ddHALInfo.ddCaps.dwCaps; hal->vxBuiltSurfCaps = hal->ddHALInfo.ddCaps.ddsCaps.dwCaps;
		
		return sr;
	}
}

BOOL DDGet32BitDriverName(DD32BITDRIVERDATA_t __far *dd32)
{
	if(drv_is_p2())
	{
		_fmemcpy(dd32, &drv_vmhal9x, sizeof(drv_vmhal9x));
	}
	else
	{
		_fmemcpy(dd32, &drv_vmhal486, sizeof(drv_vmhal486));
	}

	
	if(DDGetPtr(NULL, &(dd32->dwContext)))
	{
		return TRUE;
	}
	
	return FALSE;
}

BOOL DDNewCallbackFns(DCICMD_t __far *lpDCICMD)
{
  LPDDHALDDRAWFNS pfns;
  
	if(lpDCICMD == NULL || lpDCICMD->dwParam1 == 0) return FALSE; /* v10.330 (review #133): no callback structure, nothing taken */
	pfns = (LPVOID) lpDCICMD->dwParam1;
	lpDDHAL_SetInfo = pfns->lpSetInfo;
	
	
	return TRUE;
}

DWORD __loadds __far __fastcall HALDestroyDriver(LPDDHAL_DESTROYDRIVERDATA lpDestroyDriverData)
{
	lpDestroyDriverData->ddRVal = DD_OK;
	
	lpDDHAL_SetInfo = NULL;
	
	return DDHAL_DRIVER_HANDLED;
} /* HALDestroyDriver */

void DDGetVersion(DDVERSIONDATA_t __far *lpVer)
{
	_fmemset(lpVer, 0, sizeof(DDVERSIONDATA_t));
	lpVer->dwHALVersion = 0x0600UL; /* v10.58j: DirectDraw 6.1 may reject a HAL newer than itself (was DD_RUNTIME_VERSION 0x700) */
	
	
	dbg_printf("struct size: %d (%d %d %d %d)\n",
		sizeof(VMDAHAL_t),
		offsetof(VMDAHAL_t, ddpf),
		offsetof(VMDAHAL_t, ddHALInfo),
		offsetof(VMDAHAL_t, cb32),
		offsetof(VMDAHAL_t, FBHDA_version)
	);
}

DWORD DDHinstance(void)
{
	VMDAHAL_t __far *hal;
	
	if(!DDGetPtr(&hal, NULL))
	{
		return 0;
	}
	
	return hal->ddHALInfo.hInstance;	
}

