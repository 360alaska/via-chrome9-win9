#ifndef __VXDCALL_H__INCLUDED__
#define __VXDCALL_H__INCLUDED__

BOOL VXD_VM_connect();


#endif /* __VXDCALL_H__INCLUDED__ */

#include "vx855_2d.h"
WORD VX_accel_submit(const vx2d_request __far *);
WORD VX_stretch_submit(const vx2d_stretch __far *);
WORD VX_xfer_submit(vx2d_xfer __far *);

WORD VX_accel_bind(LPWORD,LPWORD);
void VX_complete(void); /* v10.425: OP_VX_COMPLETE */
DWORD VX_refresh_hz(void); /* .577: OP_VX_HZ */
/* v10.425: bit 15 of wEnabled = the VxD has an asynchronous engine command running (it sets and clears the bit); anything about to let
 * DIBENG touch the screen finishes it first */
extern WORD wEnabled;
#define VX_SETTLE() do{if(wEnabled&0x8000U)VX_complete();}while(0)
/* v10.507 (dibcall.c): screen access without the VxD's cursor exclusion while the cursor is the hardware one (ACCEL_GDIFAST); 1 = taken, end with VX_fast_end */
int VX_fast_begin(void);
void VX_fast_end(void);
extern DWORD vxgt_cnt[16];
