/* Optional bounded brush diagnostics for v10.67b. MIT license.
 * Included by Win16 dibcall/control and the Win32 proof program.
 * It does not enable PATINVERT or change the 2D request/VxD ABI. */
#ifndef VX_BRUSH_DIAG_H
#define VX_BRUSH_DIAG_H
#include <stdint.h>
#define VXB_ESCAPE 0x8F71
#define VXB_MAGIC 0x31425856UL /* VXB1 */
#define VXB_ARM 1
#define VXB_STOP 2
#define VXB_READ 3
#define VXB_CAP 64
#define VXB_NONE 0xFFFFFFFFUL
#define VXB_ENTRY 0
#define VXB_PATINV16 1
#define VXB_DEPTH 2
#define VXB_TRANSPARENT 3
#define VXB_STYLE 4
#define VXB_MULTICOLOR 5
#define VXB_SUBMITTED 6
#define VXB_SOLID16 7
typedef struct {uint32_t cb,magic,command,reserved;} vxb_command;
typedef struct {
 uint32_t seq,rop,bpp,x,y,w,h,present,brush_flags,brush_bpp,style;
 uint32_t raw_fg,raw_bg,hatch,device_flags,reason,op,fg,bg,result;
 uint32_t bits0,bits1,phys; /* first two brush pixels; DIB_ColorInfo result for a solid brush */
} vxb_record;
typedef struct {
 uint32_t cb,magic,abi,count,dropped,active;
 vxb_record records[VXB_CAP];
} vxb_reply;
typedef char vxb_record_size_check[(sizeof(vxb_record)==92)?1:-1];
typedef char vxb_reply_size_check[(sizeof(vxb_reply)==5912)?1:-1];
#if defined(HWBLT) && !defined(VXD32)
LONG VX_bdiag_control(LPVOID in,LPVOID out);
#endif
#endif
