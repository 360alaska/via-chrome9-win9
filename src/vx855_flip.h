/* VX855 display-start (page flip) and vertical-blank interface, v10.59. Pointer-free, shared by the VxD, the Win32 DirectDraw
 * HAL DLL and the test programs. One DeviceIoControl: input vxf_request, output VXF_REPLY_DWORDS dwords. */
#ifndef VX855_FLIP_H
#define VX855_FLIP_H
#include <stdint.h>
#define VX_IOCTL_FLIP 0x85500018UL
#define VXF_REPLY_DWORDS 16
/* commands */
#define VXF_QUERY 0 /* calibrate once per video mode (flashes the screen for a moment) and report */
#define VXF_FLIP 1 /* arg = byte offset in video memory of the surface to display, flags = DDFLIP_* */
#define VXF_WAITVB 2 /* arg = 0 test, 1 return when the vertical blank begins, 2 return when it ends */
#define VXF_RESET 3 /* display offset 0 (the GDI desktop) */
#define VXF_INFO 4 /* report only, never touches the hardware */
#define VXF_FRONT 6 /* v10.384: arg = byte offset of a surface about to be drawn, locked or blitted: while the last flip has not yet been taken by the display and arg is the surface it flipped away from, wait until it has (0xFFFFFFFF: status only, no wait). reply[VXF_R_IN_VB] = 1 while that flip is still pending. */
#define VXF_SCAN 5 /* v10.73: sample the scan line counter (register window offset 0x1290, line = bits 31:16). Own reply layout, see VXF_S_* below. Works whatever the SCANLINE registry gate says (the gate only decides whether DirectDraw is offered GetScanLine). */
/* reply[VXF_R_RESULT] */
#define VXF_OK 0
#define VXF_UNSUPPORTED 1 /* request refused, nothing changed */
#define VXF_FAILED 2
#define VXF_TIMEOUT 3 /* done, but the vertical blank never showed up in time */
#define VXF_LOST 4 /* v10.105: a flip was asked for while the 2D engine is lost (an unfinished command may still be writing video memory): nothing was changed; DirectDraw reports DDERR_SURFACELOST */
/* reply[VXF_R_STATE] */
#define VXF_ST_UNTESTED 0
#define VXF_ST_DIRECT 1 /* CRTC start address written directly (units verified against the BIOS) */
#define VXF_ST_NONE 3
/* reply layout */
#define VXF_R_RESULT 0
#define VXF_R_STATE 1
#define VXF_R_SHIFT 2 /* start address = byte offset >> shift (direct state) */
#define VXF_R_VBLANK 3 /* 0 not probed, 1 vertical blank seen on port 3DA, 2 never seen */
#define VXF_R_DISPLAY 4 /* byte offset being displayed */
#define VXF_R_FLIPS 5
#define VXF_R_BIOS 6 /* INT 10h calls made so far */
#define VXF_R_REASON 7 /* why the state is NONE: 1 mode, 2 not desktop, 3 BIOS refused, 4 BIOS changed nothing, 5 could not return to 0, 6 unknown register layout (BIOS per flip), 7 FLIP=0 in the registry, 8 FLIP_BIOS=0 in the registry */
#define VXF_R_IN_VB 8
#define VXF_R_CAL_Y 9 /* the test line given to the BIOS */
#define VXF_R_CAL_V 10 /* CRTC start value the BIOS produced */
#define VXF_R_CHANGED 11 /* CRTC registers the BIOS call changed */
#define VXF_R_FRAME3MS 12 /* time of three frames in ms (0 when no vertical blank) */
#define VXF_R_TIMEOUTS 13
#define VXF_R_LAST_REQ 14
#define VXF_R_LAST_V 15
#define VXF_S_STATUS 1 /* 0 counter unavailable, 1 line valid, 2 vertical blank (no line), 3 counter outside the picture */
#define VXF_S_LINE 2
#define VXF_S_RAW 3 /* the register as read */
#define VXF_S_GATE 4 /* registry SCANLINE */
#define VXF_S_HEIGHT 5 /* lines in the current mode */
#define VXF_S_TRIES 6
typedef struct { uint32_t cb,cmd,arg,flags; } vxf_request;
#ifdef VXD32
extern volatile uint32_t vx_flip_old_front; /* v10.384: the surface the last flip moved away from while that flip is still pending, else 0xFFFFFFFF */
void VX_flip_front_wait(uint32_t off); /* v10.384: a draw into off: wait until the pending flip is taken when off is the old front */
void VX_flip_modeset(void);
void VX_flip_control(const vxf_request *q,uint32_t *r);
#endif
#endif
