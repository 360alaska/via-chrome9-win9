/* Narrow Win32 legacy DDI declarations from public Microsoft contracts.
 * SDK types/constants come from the locally installed Open Watcom headers.
 * Not WDDM d3dumddi.h. Copyright 2026, MIT. */
#ifndef VX_D3D_HAL_H
#define VX_D3D_HAL_H
#ifndef DIRECT3D_VERSION
#define DIRECT3D_VERSION 0x0700
#endif
#include <windows.h>
#include <ddraw.h>
#include <d3dtypes.h>
#include <d3dcaps.h>
#include "ddhal32/ddrawi.h"
#ifndef DDSCAPS_RESERVED2
#define DDSCAPS_RESERVED2 0x00800000UL
#endif
#include "vx_d3d_core.h"
#include "vx_d3d_diag.h"
#pragma pack(push,4)
typedef struct vx_ddi_desc {
 DWORD dwSize,dwFlags;D3DCOLORMODEL dcmColorModel;DWORD dwDevCaps;
 D3DTRANSFORMCAPS dtcTransformCaps;BOOL bClipping;D3DLIGHTINGCAPS dlcLightingCaps;
 D3DPRIMCAPS dpcLineCaps,dpcTriCaps;
 DWORD dwDeviceRenderBitDepth,dwDeviceZBufferBitDepth,dwMaxBufferSize,dwMaxVertexCount;
} vx_ddi_desc;
typedef struct vx_ddi_global {
 DWORD dwSize;vx_ddi_desc hwCaps;DWORD dwNumVertices,dwNumClipVertices,dwNumTextureFormats;
 DDSURFACEDESC *lpTextureFormats;
} vx_ddi_global;
typedef struct vx_ddi_create {
 DDRAWI_DIRECTDRAW_LCL *lpDDLcl;DDRAWI_DDRAWSURFACE_LCL *lpDDSLcl,*lpDDSZLcl;
 DWORD dwPID,dwhContext;HRESULT ddrval;
} vx_ddi_create;
typedef struct vx_ddi_destroy {DWORD dwhContext;HRESULT ddrval;} vx_ddi_destroy;
typedef struct vx_ddi_destroy_all {DWORD dwPID;HRESULT ddrval;} vx_ddi_destroy_all;
typedef struct vx_ddi_dp2 {
 DWORD dwhContext,dwFlags,dwVertexType;DDRAWI_DDRAWSURFACE_LCL *lpDDCommands;
 DWORD dwCommandOffset,dwCommandLength;
 union {DDRAWI_DDRAWSURFACE_LCL *lpDDVertex;void *lpVertices;};
 DWORD dwVertexOffset,dwVertexLength,dwReqVertexBufSize,dwReqCommandBufSize;DWORD *lpdwRStates;
 union {DWORD dwVertexSize;HRESULT ddrval;};DWORD dwErrorOffset;
} vx_ddi_dp2;
typedef struct vx_ddi_validate {DWORD dwhContext,dwFlags,dwReserved,dwNumPasses;HRESULT ddrval;} vx_ddi_validate;
typedef struct vx_ddi_setrt {DWORD dwhContext;DDRAWI_DDRAWSURFACE_LCL *lpDDS,*lpDDSZ;HRESULT ddrval;} vx_ddi_setrt;
typedef struct vx_ddi_renderstate {
 DWORD dwhContext,dwOffset,dwCount;DDRAWI_DDRAWSURFACE_LCL *lpExeBuf;HRESULT ddrval;
} vx_ddi_renderstate;
typedef struct vx_ddi_renderprimitive {
 DWORD dwhContext,dwOffset,dwStatus;DDRAWI_DDRAWSURFACE_LCL *lpExeBuf;
 DWORD dwTLOffset;DDRAWI_DDRAWSURFACE_LCL *lpTLBuf;D3DINSTRUCTION diInstruction;HRESULT ddrval;
} vx_ddi_renderprimitive;
typedef struct vx_ddi_getstate {DWORD dwhContext,dwWhich;D3DSTATE ddState;HRESULT ddrval;} vx_ddi_getstate;
typedef struct vx_ddi_scene {DWORD dwhContext,dwFlag;HRESULT ddrval;} vx_ddi_scene;
typedef struct vx_ddi_drawone {
 DWORD dwhContext,dwFlags,PrimitiveType,VertexType;LPVOID lpvVertices;
 DWORD dwNumVertices,dwReserved;HRESULT ddrval;
} vx_ddi_drawone;
typedef struct vx_ddi_drawindexed {
 DWORD dwhContext,dwFlags,PrimitiveType,VertexType;LPVOID lpvVertices;
 DWORD dwNumVertices;WORD *lpwIndices;DWORD dwNumIndices;HRESULT ddrval;
} vx_ddi_drawindexed;
typedef struct vx_ddi_clear {
 DWORD dwhContext,dwFlags,dwFillColor,dwFillDepth;D3DRECT *lpRects;DWORD dwNumRects;HRESULT ddrval;
} vx_ddi_clear;
typedef struct vx_ddi_callbacks {
 DWORD dwSize;
 DWORD (WINAPI *ContextCreate)(vx_ddi_create *);
 DWORD (WINAPI *ContextDestroy)(vx_ddi_destroy *);
 DWORD (WINAPI *ContextDestroyAll)(vx_ddi_destroy_all *);
 DWORD (WINAPI *SceneCapture)(vx_ddi_scene *);
 LPVOID Execute,ExecuteClipped;
 DWORD (WINAPI *RenderState)(vx_ddi_renderstate *);
 DWORD (WINAPI *RenderPrimitive)(vx_ddi_renderprimitive *);DWORD dwReserved;
 LPVOID TextureCreate,TextureDestroy,TextureSwap,TextureGetSurf;
 LPVOID MatrixCreate,MatrixDestroy,MatrixSetData,MatrixGetData,SetViewportData;
 LPVOID LightSet,MaterialCreate,MaterialDestroy,MaterialSetData,MaterialGetData;
 DWORD (WINAPI *GetState)(vx_ddi_getstate *);
 DWORD reserved[10];
} vx_ddi_callbacks;
typedef struct vx_ddi_callbacks2 {
 DWORD dwSize,dwFlags;DWORD (WINAPI *SetRenderTarget)(vx_ddi_setrt *);
 DWORD (WINAPI *Clear)(vx_ddi_clear *);
 DWORD (WINAPI *DrawOnePrimitive)(vx_ddi_drawone *);
 DWORD (WINAPI *DrawOneIndexedPrimitive)(vx_ddi_drawindexed *);
 LPVOID DrawPrimitives;
} vx_ddi_callbacks2;
typedef struct vx_ddi_callbacks3 {
 DWORD dwSize,dwFlags;LPVOID Clear2,lpvReserved;
 DWORD (WINAPI *ValidateTextureStageState)(vx_ddi_validate *);
 DWORD (WINAPI *DrawPrimitives2)(vx_ddi_dp2 *);
} vx_ddi_callbacks3;
typedef struct vx_ddi_extended {
 DWORD dwSize,dwMinTextureWidth,dwMaxTextureWidth,dwMinTextureHeight,dwMaxTextureHeight;
 DWORD dwMinStippleWidth,dwMaxStippleWidth,dwMinStippleHeight,dwMaxStippleHeight;
 DWORD dwMaxTextureRepeat,dwMaxTextureAspectRatio,dwMaxAnisotropy;
 float dvGuardBandLeft,dvGuardBandTop,dvGuardBandRight,dvGuardBandBottom,dvExtentsAdjust;
 DWORD dwStencilCaps,dwFVFCaps,dwTextureOpCaps;WORD wMaxTextureBlendStages,wMaxSimultaneousTextures;
 DWORD dwMaxActiveLights;float dvMaxVertexW;WORD wMaxUserClipPlanes,wMaxVertexBlendMatrices;
 DWORD dwVertexProcessingCaps,dwReserved1,dwReserved2,dwReserved3,dwReserved4;
} vx_ddi_extended;
#pragma pack(pop)
void VX_D3D_init(void *);
void VX_D3D_attach(int attach); /* v10.319: DllMain, process attach 1 / detach 0: the callback lock's owner token */
int VX_D3D_surface(DDRAWI_DDRAWSURFACE_LCL *,vx3dn_surface *);
int VX_D3D_surface_target(DDRAWI_DDRAWSURFACE_LCL *,vx3dn_surface *); /* v10.456 */
int VX_D3D_rt_texture(const DDRAWI_DDRAWSURFACE_LCL *); /* v10.456: ACCEL_D3D_RTTEX */
extern uint32_t vx_d3d_rttex,vx_d3d_mipcap,vx_d3d_rt_waits,vx_d3d_texhint,vx_d3d_indexed,vx_d3d_ix[4];
int VX_D3D_format(const DDPIXELFORMAT *,uint32_t *);
int VX_D3D_texture_ok(uint32_t,uint32_t,uint32_t);
uint32_t VX_D3D_texture_format(uint32_t);
uint32_t VX_D3D_format_bytes(uint32_t);
#define VX_RAW_1555 10UL
#define VX_RAW_4444 11UL
/* Internal DDPIXELFORMAT code, distinct from the native D16 value (12).
 * This raw code is translated to VX3DN_TEX_XRGB1555 before submission. */
#define VX_RAW_555 14UL
/* v10.199: L8 and DXT1/3/5 (texture use only) */
#define VX_RAW_L8 30UL
#define VX_RAW_DXT1 31UL
#define VX_RAW_DXT3 32UL
#define VX_RAW_DXT5 33UL
#define VX_RAW_IS_DXT(f) ((f)>=VX_RAW_DXT1&&(f)<=VX_RAW_DXT5)
/* v10.208: bump formats (texture use only) */
#define VX_RAW_V8U8 34UL
#define VX_RAW_L6V5U5 35UL
#define VX_RAW_X8L8V8U8 36UL
uint32_t VX_D3D_texture_pitch(uint32_t,uint32_t);
uint32_t VX_D3D_texture_packed(uint32_t,uint32_t);
uint32_t VX_D3D_texture_rows(uint32_t,uint32_t);
DWORD WINAPI VX_D3D_GetDriverInfo(LPDDHAL_GETDRIVERINFODATA);
DWORD WINAPI VX_D3D_Create(vx_ddi_create *);
DWORD WINAPI VX_D3D_Destroy(vx_ddi_destroy *);
DWORD WINAPI VX_D3D_DestroyAll(vx_ddi_destroy_all *);
DWORD WINAPI VX_D3D_DP2(vx_ddi_dp2 *);
DWORD WINAPI VX_D3D_Validate(vx_ddi_validate *);
DWORD WINAPI VX_D3D_QueryStats(vx_d3d_diag *);
DWORD WINAPI VX_D3D_QueryTrace(DWORD *,DWORD);
DWORD WINAPI VX_D3D_QueryStates(DWORD *,DWORD);
#endif
