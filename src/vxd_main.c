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

/* 
 * Code for QEMU is inspired by boxvmini.asm by Philip Kelley
 */

#ifndef VXD32
#error VXD32 not defined!
#endif

#define VXD_MAIN


#include "winhack.h"
#include "vmm.h"
#include "vxd.h"
#include "vxd_lib.h"
#include "vx85598.h"
#include "vx855_3d.h"
#include "vx855_2d.h"
#include "vx855_flip.h"
void vx_lock(void);void vx_unlock(void);void vx_lock_init(uint32_t cyc_per_ms); /* .515: the engine's own lock (vx855_2d.c) */

#include "3d_accel.h"
#include "wram.h"
#include "async.h"
#include "vxd_vdd.h"


BOOL VESA_check_int10h(DWORD vm, PCRS_32 regs);

#include "version.h"

#include "code32.h"

void VXD_control();
void VXD_API_entry();
static DWORD device_ready=0;
BOOL __stdcall Device_Init_proc(DWORD VM);
void __stdcall Device_Exit_proc(DWORD VM);
void __stdcall Device_Init_Complete(DWORD VM);
void __stdcall win32_destroy_process_proc(DWORD pid);

#define SERVICE_TABLE_CNT (FBHDA__OVERLAY_UNLOCK+1)
extern DWORD service_table[SERVICE_TABLE_CNT];

/*
  VXD structure
  this variable must be in first address in code segment.
  In other cases VXD isn't loadable (WLINK bug?)
*/
DDB VXD_DDB = {
	NULL,                       // must be NULL
	DDK_VERSION,                // DDK_Version
	VXD_DEVICE_ID,              // Device ID
	VXD_MAJOR_VER,              // Major Version
	VXD_MINOR_VER,              // Minor Version
	NULL,
	VXD_DEVICE_NAME,
	VDD_Init_Order,
	(DWORD)VXD_control,
	(DWORD)VXD_API_entry,
	(DWORD)VXD_API_entry,
	NULL,// CS:IP of API entry point
	NULL,// CS:IP of API entry point
	NULL,// Reference data from real mode
	(DWORD)&service_table,// Pointer to service table
	SERVICE_TABLE_CNT,// Number of services
	NULL, // DDB_Win32_Service_Table
	'Prev', //NULL, // prev
	sizeof(DDB),
	'Rsv1',
	'Rsv2',
	'Rsv3',
};

#include "vxd_strings.h"

DWORD *DispatchTable = 0;
DWORD DispatchTableLength = 0;
DWORD ThisVM = 0;
void *DeviceCTX = NULL;
DWORD is_qemu = FALSE;

void __stdcall port_info(DWORD p_eax, DWORD p_ecx, DWORD p_edx)
{
	dbg_printf("virtual_0x1ce eax=%lX ecx=%lX edx=%lX\n", p_eax, p_ecx, p_edx);
}

DWORD __stdcall virtual_int_proc(DWORD int_num, DWORD vm, PCRS_32 regs)
{
	if(int_num == 0x10)
	{
		if(!VESA_check_int10h(vm, regs))
		{
			dbg_printf("INT 10h ignored for EAX=%lX\n", regs->Client_EAX);
			return 1;
		}
	}
	
	return 0;
}

/**
 * This is fix of broken screen when open DOS window
 *
 **/
void __declspec(naked) virtual_0x1ce()
{
/*
 * AX/AL contains the value to be read or written on the port.
 * EBX contains the handle of the VM accessing the port.
 * ECX contains the direction (in/out) and size (byte/word) of the operation.
 * EDX contains the port number, which for us will either be 1CEh or 1CFh.
 */
/*
	_asm{
		pushad
		push edx
		push ecx
		push eax
		call port_info
		popad
	}*/
	VxDCall(VDD, Get_VM_Info); // esi = result
	_asm{
		cmp edi, ThisVM             ; Is the CRTC controlled by Windows?
    jne _Virtual1CEPhysical     ; If not, we should allow the I/O
    cmp ebx, edi                ; Is the calling VM Windows?
    je _Virtual1CEPhysical      ; If not, we should eat the I/O
    	ret
		_Virtual1CEPhysical:
	}
	VxDJmp(VDD, Do_Physical_IO);
}

static DWORD int_10h_next_hook = 0;
void __declspec(naked) virtual_int_10h()
{
	_asm
	{
		; header (8 bytes)
		jmp short virtual_int_10h_entry
		jmp [int_10h_next_hook]
		virtual_int_10h_entry:

		pushad
		push ebp
		push ebx
		push eax
		call virtual_int_proc
		sub eax,1
		popad
		retn

		jmp [int_10h_next_hook]
	}
}


/**
 * VDD calls wrapers
 **/
void VDD_Get_Mini_Dispatch_Table()
{
	VxDCall(VDD, Get_Mini_Dispatch_Table);
	_asm mov [DispatchTable],edi
	_asm mov [DispatchTableLength],ecx
}

/**
 * Control Handles
 **/
void Sys_Critical_Init_proc()
{
	// nop
}

DWORD __stdcall Device_IO_Control_proc(DWORD vmhandle, struct DIOCParams *params);

/*
 * 32-bit DeviceIoControl ends here
 *
 */
void __declspec(naked) Device_IO_Control_entry()
{
	_asm {
		push esi /* struct DIOCParams */
		push ebx /* VMHandle */
		call Device_IO_Control_proc
		retn
	}
}

void VESA_check_state();

void __stdcall Set_Device_Focus_proc(DWORD VMHandle, DWORD VID, DWORD Flags, DWORD AssocVMHandle)
{
//	dbg_printf("Set_Device_Focus\n");
}

#ifdef DBGPRINT
void __stdcall print_ctrl(DWORD type)
{
	VMCB_t *cur = Get_Cur_VM_Handle();
	dbg_printf("VXD_control: type=%lX curVM=%lX thisVM=%lX\n", type, cur, ThisVM);
}
#endif

/*
 * service module calls (init, exit, deviceIoControl, ...)
 * clear carry if succes (this is always success)
 */
void __declspec(naked) VXD_control()
{
	// eax = 0x00 - Sys Critical Init
	// eax = 0x01 - sys dynamic init
	// eax = 0x1B - dynamic init
	// eax = 0x1C - dynamic exit
	// eax = 0x23 - device IO control

	
	_asm {
		cmp eax,Sys_Critical_Init
		jnz control_1
			pushad
			call Sys_Critical_Init_proc
			popad
			clc
			ret
		control_1: 
		cmp eax,Device_Init
		jnz control_2
			pushad
			push ebx ; VM handle
		  call Device_Init_proc
			test eax,eax
			jz control_init_failed
			popad
			clc
			ret
		control_2:
		cmp eax,Init_Complete
		jnz control_3
			pushad
			push ebx ; VM handle
		  call Device_Init_Complete
			popad
			clc
			ret
		control_3:
		cmp eax,Sys_Dynamic_Device_Init
		jnz control_4
			pushad
			push ebx ; VM handle
		  call Device_Init_proc
		  test eax,eax
		  jz control_init_failed
		  popad
			clc
			ret
		control_init_failed:
			popad
			stc
			ret
		control_4:
		cmp eax,W32_DEVICEIOCONTROL
		jnz control_5
			jmp Device_IO_Control_entry
		control_5:
		cmp eax,System_Exit
		jnz control_6
			pushad
			push ebx ; VM handle
		  call Device_Exit_proc
			popad
			clc
			ret
/*
	https://www.codeproject.com/Articles/3874/The-Ultimate-Process-Thread-spy-for-Windows-9x
	
	"You will find the definition of CREATE_PROCESS, but wont find how to get the
	PID of a newly created process. For thread (CREATE_THREAD), the thread ID is
	passed in EDI register, but for CREATE_PROCESS it's not there. At last, by
	trial and error I found where it is, the PID value, it is in EDX reg."
*/
		control_6:
		cmp eax,DESTROY_PROCESS
		jnz control_7
			pushad
			push edx ; PID
			call win32_destroy_process_proc
			popad
			clc
			ret
		control_7:
		cmp eax, Set_Device_Focus
		jnz control_8
			pushad
			push edi ; AssocVMHandle
			push esi ; Flags
			push edx ; VID
			push ebx ; VMHandle
			call Set_Device_Focus_proc
			popad
			clc
			ret
		control_8:
		cmp eax,Sys_Dynamic_Device_Exit
		jnz control_9
			/* VDD dispatch and the INT 10 chain point into this module.
			 * Keep it resident until restart, including after DRV Disable. */
			cmp [device_ready],0
			jnz control_unload_refused
			clc
			ret
		control_unload_refused:
			stc
			ret
		control_9:
;			pushad
;			push eax
;			call print_ctrl
;			popad
		clc
	  ret
	};
}

void VX_edid_report(DWORD *out, DWORD scan);
void VX_cache_report(vx_cache_report *r);
unsigned long VX_cache_set(unsigned long type);
void VX_cache_restore(void);
int VX_dac(const vx_dac_req *q);
int VX_dac_read(vx_dac_req *out);
void VX_crt_dump(DWORD *out);
void VX_probe(const DWORD *in, DWORD n, DWORD *o); /* v10.333 */
/* process V86/PM16 calls */
WORD __stdcall VXD_API_Proc(PCRS_32 state)
{
	WORD rc = 0xFFFF;
	WORD service = state->Client_EDX & 0xFFFF;
	
	//dbg_printf("VXD_API_Proc, service: %X\n", service);
	//Begin_Critical_Section(0);
	
	switch(service)
	{
		case VXD_PM16_VERSION:
			rc = (VXD_MAJOR_VER << 8) | VXD_MINOR_VER;
			break;
		case VXD_PM16_VMM_VERSION:
			rc = Get_VMM_Version();
			break;
		case OP_FBHDA_SETUP:
			{
				DWORD dptr = (DWORD)FBHDA_setup();
				if(dptr)
				{
					state->Client_ECX = dptr;
					rc = 1;
				}
				break;
			}
		case OP_FBHDA_ACCESS_BEGIN:
			FBHDA_access_begin(state->Client_ECX);
			rc = 1;
			break;
		case OP_FBHDA_ACCESS_END:
			FBHDA_access_end(state->Client_ECX);
			rc = 1;
			break;
		case OP_FBHDA_ACCESS_RECT:
			FBHDA_access_rect(state->Client_EBX, state->Client_ECX, state->Client_ESI, state->Client_EDI);
			rc = 1;
			break;
		case OP_FBHDA_CLEAN:
			FBHDA_clean();
			rc = 1;
			break;
		case OP_FBHDA_PALETTE_SET:
			FBHDA_palette_set(state->Client_ECX & 0xFF, state->Client_EDI);
			rc = 1;
			break;
		case OP_FBHDA_GAMMA_GET:
			state->Client_ECX = FBHDA_gamma_get((void *)state->Client_EDI, state->Client_ECX);
			rc = 1;
			break;
		case OP_FBHDA_GAMMA_SET:
			state->Client_ECX = FBHDA_gamma_set((void *)state->Client_ESI, state->Client_ECX);
			rc = 1;
			break;
		case OP_FBHDA_REFRESH:
			FBHDA_refresh(state->Client_ECX);
			break;
		/* mouse */
		case OP_MOUSE_LOAD:
			critical_section_enter();
			state->Client_ECX = mouse_load();
			critical_section_leave();
			rc = 1;
			break;
		case OP_MOUSE_MOVE:
			mouse_move(state->Client_ESI, state->Client_EDI);
			rc = 1;
			break;
		case OP_MOUSE_HIDE:
			mouse_hide();
			rc = 1;
			break;


		case OP_VESA_VALID:
		{
			BOOL rs;
			rs = VESA_valid();
			state->Client_ECX = (DWORD)rs;
			rc = 1;
			break;
		}
		case OP_VESA_SETMODE:
		{
			BOOL rs;
			DWORD w, h, rr_min, rr_max;
			w = state->Client_ESI >> 16;
			h = state->Client_ESI & 0xFFFF;
			rr_min = state->Client_EDI >> 16;
			rr_max = state->Client_EDI & 0xFFFF;

			critical_section_enter();
			vx_lock(); /* .515: the system critical section first, then the engine's lock (vx855_2d.c) */
			rs = VESA_setmode(w, h, state->Client_ECX, rr_min, rr_max);
			vx_unlock();
			critical_section_leave();
			state->Client_ECX = (DWORD)rs;
			rc = 1;
			break;
		}
		case OP_VESA_VALIDMODE:
		{
			BOOL rs;
			rs = VESA_validmode(state->Client_ESI, state->Client_EDI, state->Client_ECX);
			state->Client_ECX = (DWORD)rs;
			rc = 1;
			break;
		}
		case OP_VX_BIND:
   state->Client_ECX=VX2D_bind(state->Client_ESI,state->Client_EDI);
   rc=1;
   break;
  case OP_VX_HZ: /* .577 */
   {extern DWORD VX_flip_hz(void);state->Client_ECX=VX_flip_hz();}
   rc=1;
   break;
  case OP_VX_COMPLETE: /* v10.425 */
   VX2D_complete();
   state->Client_ECX=1;
   rc=1;
   break;
  case OP_VX_STRETCH:
  if(state->Client_ECX==sizeof(vx2d_stretch) && state->Client_ESI)
   state->Client_ECX=VX2D_stretch((const vx2d_stretch*)state->Client_ESI);
  else state->Client_ECX=VX2D_SKIP;
  rc=1;
  break;
  case OP_VX_XFER: /* v10.116 */
  if(state->Client_ECX==sizeof(vx2d_xfer) && state->Client_ESI)
   state->Client_ECX=VX2D_xfer((vx2d_xfer*)state->Client_ESI);
  else state->Client_ECX=VX2D_SKIP;
  rc=1;
  break;
  case OP_VX_SUBMIT:
  if(state->Client_ECX==sizeof(vx2d_request) && state->Client_ESI)
   state->Client_ECX=VX2D_submit((const vx2d_request*)state->Client_ESI);
  else state->Client_ECX=VX2D_SKIP;
  rc=1;
  break;
		case OP_VESA_HIRES:
		{
			DWORD enable = state->Client_ECX;
			if(enable)
			{
				VESA_HIRES_enable();
			}
			else
			{
				VESA_HIRES_disable();
			}
			rc = 1;
			break;
		}

	}
	
	if(rc == 0xFFFF)
	{
		state->Client_EFlags |= 0x1; // set carry
	}
	else
	{
		state->Client_EFlags &= 0xFFFFFFFEUL; // clear carry
	}
	
	//dbg_printf("VXD_API_Proc end\n");
	
	//End_Critical_Section();
	
	return rc;
}

/*
 * Service calls from protected mode (usually 16 bit) and virtual 86 mode
 * CPU state before call is already on stack (EBP points it).
 * Converts the call to __stdcall and call 'VXD_API_Proc', result of this
 * function is placed to saved AX register.
 *
 */
/* v10.139 (review P03): the CR3 reload (a whole TLB flush on every call from the DRV) is inherited from VMDisp9x with the note 'TLB bug here? Looks like...';
 * registry ACCEL_CR3 (default 1 = keep it) and live control 0xEE000000|v switch it so its cost can be measured.
 * v10.141: default 0. VX2CR3 (1150 rounds on the Wyse: 16,848 scenes drawn from remapped pages identical with and without it, no hang) and VX2FIX (4.2 us per DRV call).
 * The reload only papers over the Windows 9x TLB invalidation bug in VMM (CPUs whose page walks are not coherent, e.g. recent AMD/Intel or VMs on them; the real fix is
 * patcher9x). .530: removed with ACCEL_CR3 and the live control 0xEE000000. */
volatile unsigned long vx_accel_d3d = 1; /* v10.149: registry ACCEL_D3D; v10.165: default 1 */
void __declspec(naked) VXD_API_entry()
{
	_asm {
		push ebp
		call VXD_API_Proc
		mov [ebp+1Ch], ax
		retn
	}
}

const char reg_path[] = "Software\\VX85598";

static void configure_FBHDA()
{
	FBHDA_t *fbhda;
	
	fbhda = FBHDA_setup();
	
	if(fbhda)
	{
		
						
		/* v10.73: VIDMEM_MB (DWORD, default 0 = all the video memory the driver found, up to VX_FB_LIMIT) lowers the memory handed to DirectDraw.
		 * Not below 8 MB. Applied before the overlay reservations below, which count down from the end. */
		{
			DWORD cap_mb = 0;
			RegReadConf(HKEY_LOCAL_MACHINE, reg_path, "VIDMEM_MB", &cap_mb);
			if(cap_mb >= 8 && cap_mb < 4096 && fbhda->vram_size_virt > cap_mb*(1024UL*1024UL))
			{
				fbhda->vram_size_virt = cap_mb*(1024UL*1024UL);
				vx_status.shadow_bytes = fbhda->vram_size_virt;
			}
		}

		/* .530: the OVERLAY_n_SIZE reservations (inherited, 0 by default) removed: no block is reserved for overlays */

		/* v10.77: 4 KB at the very end of the video memory (below every overlay) hold the 8x8 pixel block of the engine's colour pattern fills (VX2COL).
		 * The block is not part of what DirectDraw is given: dddrv.c ends the heap at vram_size_virt - overlays_size. */
		{
			extern volatile unsigned long vx2d_brush_off;
			vx2d_brush_off = 0;
			if(fbhda->vram_size_virt >= fbhda->overlays_size + 0x00800000UL && fbhda->vram_size_virt <= VX_FB_MAP_LIMIT && !(fbhda->vram_size_virt & 0xFFF) && !(fbhda->overlays_size & 0xFFF))
			{
				fbhda->overlays_size += 4096UL;
				vx2d_brush_off = fbhda->vram_size_virt - fbhda->overlays_size;
			}
			dbg_printf("Colour block at %lx\n", vx2d_brush_off);
		}

		/* v10.105: 16 KB (64x64 ARGB, 1 KB aligned) below the colour block hold the hardware cursor image; not part of what DirectDraw is given either. */
		{
			extern volatile unsigned long vx2d_hwcur_off, vx2d_brush_off;
			vx2d_hwcur_off = 0;
			if(vx2d_brush_off && fbhda->overlays_size >= 4096UL && fbhda->vram_size_virt >= fbhda->overlays_size + 0x00800000UL)
			{
				fbhda->overlays_size += 16384UL;
				vx2d_hwcur_off = fbhda->vram_size_virt - fbhda->overlays_size;
			}
			dbg_printf("Cursor block at %lx\n", vx2d_hwcur_off);
		}

		/* v10.108: 6 MB (three buffers of 2 MB) below the cursor block are the HQV scaler's output buffers (video overlay: shrinking, RGB sources); not part of what DirectDraw is given either. */
		{
			extern volatile unsigned long vx2d_hqv_off, vx2d_hwcur_off;
			vx2d_hqv_off = 0;
			if(vx2d_hwcur_off && fbhda->vram_size_virt >= fbhda->overlays_size + 0x00C00000UL)
			{
				fbhda->overlays_size += 4UL*0x200000UL; /* 3 HQV buffers and the NV12 staging area for planar video */
				vx2d_hqv_off = fbhda->vram_size_virt - fbhda->overlays_size;
			}
			dbg_printf("HQV block at %lx\n", vx2d_hqv_off);
		}

		/* v10.116: 2 MB below the HQV block (or below the cursor block) are the staging area of the GDI blits from system memory whose ROP reads the screen (the CPU writes the source there,
		 * the engine copies it to the screen with the ROP); not part of what DirectDraw is given either. Kept only when at least 8 MB stay for the desktop and DirectDraw. */
		{
			extern volatile unsigned long vx2d_stage_off, vx2d_hwcur_off;
			vx2d_stage_off = 0;
			if(vx2d_hwcur_off && fbhda->vram_size_virt >= fbhda->overlays_size + 0x00A00000UL)
			{
				fbhda->overlays_size += VX2D_XF_STAGE_SIZE;
				vx2d_stage_off = fbhda->vram_size_virt - fbhda->overlays_size;
			}
			dbg_printf("Staging block at %lx\n", vx2d_stage_off);
		}

		/* v10.124: 4 MB below the staging block keep the screen area under a menu for SaveScreenBitmap (engine copies); not part of what DirectDraw is given. Only with at least 8 MB left. */
		{
			extern volatile unsigned long vx2d_save_off, vx2d_stage_off;
			vx2d_save_off = 0;
			if(vx2d_stage_off && fbhda->vram_size_virt >= fbhda->overlays_size + VX2D_XF_SAVE_SIZE + 0x00800000UL)
			{
				fbhda->overlays_size += VX2D_XF_SAVE_SIZE;
				vx2d_save_off = fbhda->vram_size_virt - fbhda->overlays_size;
			}
			dbg_printf("Save block at %lx\n", vx2d_save_off);
		}

		/* v10.228: 256 KB below the save block (or the lowest block above) are the 3D command block: draw packets are written there and
		 * fetched by the engine's command fetcher; not part of what DirectDraw is given. Only with at least 8 MB left. */
		{
			extern volatile unsigned long vx3dn_cmd_off, vx2d_hwcur_off;
			vx3dn_cmd_off = 0;
			if(vx2d_hwcur_off && fbhda->vram_size_virt >= fbhda->overlays_size + VX3DN_CMD_BYTES + 0x00800000UL)
			{
				fbhda->overlays_size += VX3DN_CMD_BYTES;
				vx3dn_cmd_off = fbhda->vram_size_virt - fbhda->overlays_size;
				if(vx3dn_cmd_off & 0xFFF) { fbhda->overlays_size -= VX3DN_CMD_BYTES; vx3dn_cmd_off = 0; }
			}
			dbg_printf("3D command block at %lx\n", vx3dn_cmd_off);
		}

		/* v10.402: 64 KB (64 KB aligned) below the command block: the page table of XP's ring in system memory (vx_native3d_vxd.c xr_setup,
		 * as XP's miniport keeps its table at the top of video memory); v10.403: and 64 KB above it for the command regulator's virtual
		 * queue (XP's AGP form); not part of what DirectDraw is given. */
		{
			extern volatile unsigned long vx3dn_gart_off, vx3dn_cmd_off;
			vx3dn_gart_off = 0;
			if(vx3dn_cmd_off && fbhda->vram_size_virt >= fbhda->overlays_size + 0x00820000UL)
			{
				unsigned long at = (fbhda->vram_size_virt - fbhda->overlays_size - 0x20000UL) & ~0xFFFFUL;
				fbhda->overlays_size = fbhda->vram_size_virt - at;
				vx3dn_gart_off = at;
			}
			dbg_printf("Page table block at %lx\n", vx3dn_gart_off);
		}

		/* v10.470 (ACCEL_D3D_INDEXED, default 1; 0 = no area, the HAL builds no indexed packets): 1020 KB (136 slots of 7680 bytes, 256-byte
		 * aligned) below the page table block: the vertices of indexed packets, read by the engine by index (vx_native3d_vxd.c va_alloc);
		 * not part of what DirectDraw is given. Only with at least 8 MB left. */
		{
			extern unsigned long vx3dn_va_off; extern volatile unsigned long vx3dn_gart_off, vx_d3d_indexed; DWORD on = 1UL;
			/* .524: always on (ACCEL_D3D_INDEXED retired) */ vx_d3d_indexed = on ? 1UL : 0UL;
			vx3dn_va_off = 0;
			if(vx_d3d_indexed && vx3dn_gart_off && fbhda->vram_size_virt >= fbhda->overlays_size + 136UL*7680UL + 0x00800000UL)
			{
				unsigned long at = (fbhda->vram_size_virt - fbhda->overlays_size - 136UL*7680UL) & ~0xFFFUL;
				fbhda->overlays_size = fbhda->vram_size_virt - at;
				vx3dn_va_off = at;
			}
			if(!vx3dn_va_off) vx_d3d_indexed = 0;
			dbg_printf("Vertex area at %lx\n", vx3dn_va_off);
		}
		
		/* .530: FORCE_SOFTWARE, FORCE_QEMU3DFX (the virtual machines' ICDs) and ACCEL_DCI removed: our ICD and the DCI correction always */

		{
			DWORD direct = 1, wc = 1;
									VESA_configure_direct(direct, wc);
			{
				DWORD xm = 1;
				RegReadConf(HKEY_LOCAL_MACHINE, reg_path, "EDID_MODES", &xm);
				VESA_configure_modes(xm);
				if(xm != 2)
				{
					DWORD hr = 0;
					RegReadConf(HKEY_LOCAL_MACHINE, reg_path, "NOEDIDHIGHRES", &hr);
					VESA_configure_noedid(hr);
				}
				{extern unsigned long vx_chip_id; xm = (vx_chip_id == 0x5122UL || vx_chip_id == 0x1122UL) ? 1 : 0;} /* the driver's own timing above 1024x768 on the VX855 and (.586; board, VXTIME v1.1: eight sizes, 59.7-60.1 Hz, steady) the VX800; the BIOS's on the VX900 until a board has shown the driver's there (VXTIME switches it live) */
				RegReadConf(HKEY_LOCAL_MACHINE, reg_path, "CUSTOM_TIMING", &xm);
				VESA_configure_timing(xm);
				xm = 1;
				RegReadConf(HKEY_LOCAL_MACHINE, reg_path, "TIMING_1152_CVT", &xm);
				VESA_configure_1152(xm);
			}
			{
				DWORD acc = 1;
				extern volatile unsigned long vx2d_enable, vx2d_enable_cfg, vx2d_copy_all, vx2d_copy_all_cfg;
				{extern int VX_chip_accel(void); if(!VX_chip_accel()) acc = 0;} /* .579: VX900: the engines not used yet */
								vx2d_enable = acc;
				vx2d_enable_cfg = acc;
				acc = 1;
								vx2d_copy_all = acc;
				vx2d_copy_all_cfg = acc;
				{
					extern volatile unsigned long vx2d_f16, vx2d_fpat, vx2d_fclip, vx2d_fddraw, vx2d_f8, vx2d_fcol;
					extern volatile unsigned long vx2d_fill_min, vx2d_copy_min, vx2d_line_min, vx2d_fline;
					acc = 2304UL; /* v10.138: VX2TUNE (48x48: the engine wins for every brush at 8, 16 and 32bpp) */ vx2d_fill_min = acc;
					{extern volatile unsigned long vx2d_fill_tall, vx2d_fill_tall_cfg; acc = 256UL; /* v10.143: VX2FAST: the engine wins every narrow fill from 256 rows at 8, 16 and 32bpp */ vx2d_fill_tall = vx2d_fill_tall_cfg = acc & 0xFFFFUL;}
					{extern volatile unsigned long vx2d_prefilter, vx2d_prefilter_cfg; acc = 1; vx2d_prefilter = vx2d_prefilter_cfg = acc ? 1UL : 0UL;}
					{extern volatile uint32_t vx3dn_mipon, vx3dn_aniso; acc = 1UL; vx3dn_mipon = acc ? 1UL : 0UL; acc = 1UL; vx3dn_aniso = acc ? 1UL : 0UL;} /* v10.199: mipmaps, anisotropic filtering */
					{extern volatile uint32_t vx3dn_frontz; acc = 1UL; vx3dn_frontz = acc ? 1UL : 0UL;} /* v10.416 */
					{extern volatile uint32_t vx3dn_flip_ring; acc = 1UL; vx3dn_flip_ring = acc ? 1UL : 0UL;} /* v10.417: XP's flip in the ring */
					{extern volatile uint32_t vx3dn_ringclear; acc = 1UL; vx3dn_ringclear = acc ? 1UL : 0UL;} /* v10.418 */
					{extern volatile uint32_t vx3dn_unit2c; acc = 1UL; vx3dn_unit2c = acc ? 1UL : 0UL;} /* v10.310: unit 2 on its own coordinate set (TextureC) */
					{extern volatile uint32_t vx3dn_ckey; acc = 1UL; vx3dn_ckey = acc ? 1UL : 0UL;} /* v10.197: Direct3D texture colour key (XP's T45 program) */
					{extern uint32_t vx3dn_guard; vx3dn_guard = 1UL;} /* .530: the feature word fixed (vx855_2d.c: 0x0787FFFF, the registry switches removed); ACCEL_3D_GUARD (bit 23) on */
					{extern uint32_t vx3dn_idle_stable; acc = 8UL; vx3dn_idle_stable = acc<1?1UL:acc>1000?1000UL:acc;} /* v10.221 */
					{extern volatile uint32_t vx3dn_posted; acc = 1UL; vx3dn_posted = acc ? 1UL : 0UL;} /* v10.220: posted engine writes */
					{extern volatile uint32_t vx3dn_fetch; acc = 1UL; vx3dn_fetch = acc ? 1UL : 0UL;} /* v10.228: draws through the command fetcher */
					{extern volatile uint32_t vx3dn_gap_wipe; acc = 1UL; vx3dn_gap_wipe = acc ? 1UL : 0UL;}
					{extern volatile uint32_t vx3dn_onecopy; vx3dn_onecopy = 1UL; /* .524: always on (ACCEL_D3D_ONECOPY retired) */}
					{extern volatile uint32_t vx3dn_ringblt; vx3dn_ringblt = 1UL; /* .524: always on (ACCEL_D3D_RINGBLT retired) */}
					{extern volatile uint32_t vx3dn_ringprog; acc = 1UL; vx3dn_ringprog = acc ? 1UL : 0UL;}
					{extern volatile uint32_t vx3dn_ringcap; acc = 1UL; vx3dn_ringcap = acc ? 1UL : 0UL;} /* v10.466 */ /* v10.465: the ring wait bounded by progress */
					{extern volatile unsigned long vx_d3d_rttex,vx_d3d_mipcap; acc = 1UL; vx_d3d_rttex = acc ? 1UL : 0UL;
					 acc = 1UL; vx_d3d_mipcap = acc ? 1UL : 0UL;
					 {extern volatile unsigned long vx_d3d_texhint; vx_d3d_texhint = 1UL; /* .527: always on (ACCEL_D3D_TEXHINT retired) */}
					 {extern volatile unsigned long vx_d3d_rtnowait; vx_d3d_rtnowait = 1UL; /* .524: always on (ACCEL_D3D_RTNOWAIT retired) */}
					 {extern volatile unsigned long vx_d3d_tnl; acc = 0UL; RegReadConf(HKEY_LOCAL_MACHINE, reg_path, "ACCEL_D3D_TNL", &acc); vx_d3d_tnl = acc ? 1UL : 0UL; /* .555: ACCEL_D3D_IX16 (.554) removed: XP's index form always (vx_native3d_hw.c) */ /* .534: the T&L probe (a feature under test, default 0; vx_d3d_core.c): the HAL offers hardware T&L so a run shows which games choose it; DELETE ONCE THE GAMES ARE PROBED */} /* v10.477 */} /* .527: ACCEL_D3D_PFVV2 removed */
					{extern volatile unsigned long vx_d3d_ixpass; vx_d3d_ixpass = 1UL; /* .531: ACCEL_D3D_IX384 removed (384 vertices an indexed packet, always); .529: ACCEL_D3D_HWCULL removed */ /* .525: always on (ACCEL_D3D_IXPASS retired) */} /* .515: the engine's back-face cull for indexed packets; indexed lists passed through */
					{extern volatile unsigned long vx_d3d_infocache; vx_d3d_infocache = 1UL; /* .525: always on (ACCEL_D3D_INFOCACHE retired) */} /* .524: ACCEL_D3D_IXPF removed */
					{extern volatile uint32_t vx3dn_shadow; vx3dn_shadow = 1UL; /* .526: always on (ACCEL_D3D_SHADOW retired) */} /* .525: the state set-up's type-01 register shadow */
					{extern volatile uint32_t vx3dn_delta; vx3dn_delta = 1UL; /* .525: always on (ACCEL_D3D_DELTA retired) */} /* .524: state deltas in a batch (vx_native3d.h; the HAL marks, the VxD keeps) */
					{extern volatile unsigned long vx_d3d_v8; vx_d3d_v8 = 1UL; /* .524: always on (ACCEL_D3D_V8 retired) */} /* .523: indexed packets of 8-word vertices */ /* .522: the HAL's readiness check kept between calls; the pass-through's vertex prefetch */
					{extern volatile unsigned long vx_privlock; acc = 1UL; vx_privlock = acc ? 1UL : 0UL; vx_lock_init(0);} /* .515: the engine's own lock (0: the system critical section) */ /* v10.463: the HAL's surface lookup hint; v10.468: the not-yet-checked vertices prefetched */
					{extern volatile uint32_t vx3dn_texonly; vx3dn_texonly = 1UL; /* .524: always on (ACCEL_D3D_TEXONLY retired) */} /* v10.450: a packet that differs from the last only in its textures sends the texture units only */ /* v10.449: a batch's prepacked vertices copied once, into the command block, and checked there */
					{extern volatile uint32_t vx3dn_fastclock; vx3dn_fastclock = 1UL; /* .524: always on (ACCEL_D3D_FASTCLOCK retired) */} /* v10.448: the kick's time stamp from the VMM's time word */
					{extern volatile uint32_t vx3dn_xr; acc = 1UL; vx3dn_xr = acc ? 1UL : 0UL;} /* v10.402 */
					
					{extern volatile uint32_t vx3dn_xr_nt; acc = 1UL; vx3dn_xr_nt = acc ? 1UL : 0UL;} /* v10.411 */
					
					{extern volatile uint32_t vx3dn_fastblk; acc = 1UL; vx3dn_fastblk = acc ? 1UL : 0UL;} /* v10.412 */
					{extern volatile uint32_t vx3dn_prefetch; acc = 1UL; vx3dn_prefetch = acc ? 1UL : 0UL;} /* v10.412 */
					{extern volatile uint32_t vx3dn_wc_flush; acc = 1UL; vx3dn_wc_flush = acc ? 1UL : 0UL;} /* v10.390 */
					{extern volatile uint32_t vx3dn_ring_fence; acc = 1UL; vx3dn_ring_fence = acc ? 1UL : 0UL;} /* v10.383: the ring's fence; the ring off after its first hang */ /* v10.382: ring default 1 again, with the gap before each start written over (ACCEL_D3D_GAPWIPE, default 1) */ /* v10.374: the fetcher kept running as a ring (vx_native3d_vxd.c fx_start); v10.375: default 0 (GTA3 and Quake 2 drew broken with it); v10.378: 1 with XP's kick; v10.379: 0 again (GTA3: engine hangs) */
					{extern uint32_t vx3dn_stencil_zw; acc = 2UL; vx3dn_stencil_zw = acc>2 ? 2UL : acc;} /* v10.242-v10.245: the stencil plane as XP programs it (1); 2 with its read and write bits always on (v10.247: the default); 0 as v10.241 */
					{extern uint32_t vx3dn_strip_hw; acc = 1UL; vx3dn_strip_hw = acc ? 1UL : 0UL;} /* v10.236: strips drawn as strips (0: as their triangles) */
					{extern uint32_t vx3dn_vfast; acc = 1UL; vx3dn_vfast = acc ? 1UL : 0UL;} /* v10.317: the vertex words built by the plain loops (0: a vertex at a time) */
					{extern volatile uint32_t vx3dn_state_cache; acc = 1UL; vx3dn_state_cache = acc ? 1UL : 0UL;} /* v10.232 */
					{extern volatile uint32_t vx_mouse_fast; acc = 1UL; vx_mouse_fast = acc ? 1UL : 0UL;} /* v10.253 */
					{extern volatile uint32_t vx3dn_clear_async; acc = 1UL; vx3dn_clear_async = acc?1UL:0UL;} /* v10.282 */
					{extern volatile uint32_t vx3dn_fetch_kb; acc = 32UL; vx3dn_fetch_kb = acc;} /* v10.365: fetch group size (vx_native3d_vxd.c) */
					
					
					{extern volatile uint32_t vx3dn_cgroup; acc = 1UL; vx3dn_cgroup = acc?1UL:0UL;} /* v10.282 */
					{extern volatile uint32_t vx3dn_sink; acc = 1UL; vx3dn_sink = acc?1UL:0UL;} /* v10.282 */
					
					
					{extern volatile uint32_t vx3dn_async; acc = 1UL; vx3dn_async = acc ? 1UL : 0UL;} /* v10.231: fetched draws return without waiting */
					{extern uint32_t vx3dn_burst; acc = 1UL; vx3dn_burst = acc ? 1UL : 0UL;} /* v10.214: one engine burst per triangle-list packet; v10.216: default 1 (board, VXDX7 v2.38) */
					{extern volatile unsigned long vx3dn_recover; acc = 1UL; vx3dn_recover = acc ? 1UL : 0UL;} /* v10.210: reset a hung 3D engine and carry on (default 1) */
					{extern volatile unsigned long vx_d3d_log; acc = 0UL; RegReadConf(HKEY_LOCAL_MACHINE, reg_path, "ACCEL_D3D_LOG", &acc); vx_d3d_log = acc ? 1UL : 0UL; acc = 0UL; RegReadConf(HKEY_LOCAL_MACHINE, reg_path, "ACCEL_D3D_SAMPLE", &acc); if(acc) vx_d3d_log |= 2UL;} /* .511: ACCEL_D3D_SAMPLE (diagnostic, 0): the HAL samples the flipping thread (C:\VXD3D.LOG) */ /* v10.202: C:\VXD3D.LOG off unless set */
					{extern volatile unsigned long vx_lock_hal; acc = 1UL; vx_lock_hal = acc ? 1UL : 0UL;} /* v10.432 */
					{extern volatile unsigned long vx_heap_align; acc = 1UL; vx_heap_align = acc ? 1UL : 0UL;} /* v10.179: DirectDraw blocks in multiples of 256 bytes (the HAL asks through query 0xFFFFFE50) */
					{extern volatile uint32_t vx3dn_clear2d, vx3dn_clear2d_masked; acc = 3UL; vx3dn_clear2d = acc & 1UL; vx3dn_clear2d_masked = (acc >> 1) & 1UL;} /* v10.179: bit 0 = 2D-engine clears, bit 1 = also D24S8 Z-only / stencil-only clears as two ROP fills (default 3) */ /* v10.177: Direct3D clears on the 2D engine */
					{extern volatile unsigned long vx2d_keep, vx2d_keep_cfg; acc = 1; vx2d_keep = vx2d_keep_cfg = acc ? 1UL : 0UL;}
					{extern volatile unsigned long vx_accel_d3d; acc = 1UL; vx_accel_d3d = acc & 0xFFFFUL;} /* v10.165: default 1 (VXDX7 --draw passed on the board, v10.164); 0 off */ /* v10.149: advertise the experimental Direct3D HAL; v10.150: bit 0 on, bit 1 no GetDriverInfo (DX5-style HAL only) */ /* v10.144: kept engine context; v10.145: default 1 (VX2KEEP passed) */ /* v10.142: the DRV keeps requests the VxD would decline for their size */ /* v10.142: fills at least this many rows high go to the engine below ACCEL_FILL_MIN (0 = off) */
					acc = 256UL;    vx2d_copy_min = acc;
					acc = 32UL;     vx2d_line_min = acc; /* v10.90 */
					acc = 1;           vx2d_fline = acc;
					{ /* v10.507-v10.509: GDI options (vx855_2d.c vx2d_gdi): ACCEL_CPULINE (1) the driver's own solid lines, ACCEL_CPULINE_MAX (368; .510, VXGTUNE v1.3: the two .509 boards cross at about 362-365) / ACCEL_CPULINE_MAXY (112) the
					   * lengths from which a shallow / steep pen-copy segment goes to the engine instead, ACCEL_GDIFAST (1) no exclusion calls while the cursor is the hardware one */
						extern volatile unsigned long vx2d_gdi, vx2d_gdi_cfg, vx2d_linemax, vx2d_linemax_cfg, vx2d_linemaxy, vx2d_linemaxy_cfg; DWORD g = 0;
						acc = 1; if(acc) g |= 1;
						acc = 1; if(acc) g |= 2;
						vx2d_gdi_cfg = g; vx2d_gdi = g;
						acc = 368; if(acc > 4095) acc = 4095; vx2d_linemax_cfg = acc; vx2d_linemax = acc;
						acc = 112; if(acc > 4095) acc = 4095; vx2d_linemaxy_cfg = acc; vx2d_linemaxy = acc;
					}
					{ /* v10.95: asynchronous DirectDraw blits inside an exclusive mode. v10.103: on by default (VX2ASYNC v1.0 passed every check on the hardware with the v10.96 gate fix, 75% of the time with 1 ms of CPU work per copy); ACCEL_ASYNC=0 turns it off. */
						extern volatile unsigned long vx2d_async, vx2d_async_cfg;
						acc = 1; vx2d_async_cfg = acc & 1; vx2d_async = vx2d_async_cfg;
						{extern volatile unsigned long vx2d_async_present,vx2d_async_present_cfg; acc = 1; vx2d_async_present_cfg = acc & 1; vx2d_async_present = vx2d_async_present_cfg;} /* v10.424 */
					}
					{ /* v10.99: CPU stretch/conversion of StretchDIBits and DibToDevice sources into the 16bpp and 32bpp screen: the classes VX2STRX verified on the hardware (bit mask, 0 = off). v10.102: on by default = 32703 (0x7FBF: every class that passed and paid in VX2STRX v2.1b; classes 64 and 32768 are slower than DIBENG and stay off); the registry value overrides. */
						extern volatile unsigned long vx2d_str_mask, vx2d_str_cfg;
						acc = 0x187FBFUL; vx2d_str_cfg = acc & 0xFFFFFFUL; vx2d_str_mask = vx2d_str_cfg;
						{ /* v10.104: 1-bit, 4-bit and RLE sources into the 32bpp screen: the classes VX2STR2 verified (bit mask, 0 = off). On by default = 27 (1-bit and 4-bit, both factors >= 2 and one factor 1: pixels equal, 2x 12.5 -> 7.3 ms; the 1:1 classes and all RLE classes were slower than DIBENG and stay off); the registry value overrides. */
							extern volatile unsigned long vx2d_str_mask2, vx2d_str_cfg2;
							acc = 27; vx2d_str_cfg2 = acc & 0xFFFFFFUL; vx2d_str_mask2 = vx2d_str_cfg2;
						}
					}
					{ /* v10.105: hardware cursor (0 off, 1 the primary display's register set, 2 the second's): v10.105b: on by default = 1 (VX2CUR passed on the hardware with register set 1: 300 mouse moves 141.8 ms with the software cursor, 7.6 ms with the hardware cursor); ACCEL_HWCURSOR=0 turns it off */
						extern volatile unsigned long vx2d_hwcur, vx2d_hwcur_cfg;
						acc = 1; vx2d_hwcur_cfg = (acc > 2) ? 0 : acc; vx2d_hwcur = vx2d_hwcur_cfg;
						/* v10.329: diagnostics off unless asked: ACCEL_PROFILE (the 3D path's timing and counts), ACCEL_CURSOR_CHECK (the cursor's kept image and registers) */
						{extern volatile uint32_t vx_prof; DWORD pf = 0;
						 RegReadConf(HKEY_LOCAL_MACHINE, reg_path, "ACCEL_PROFILE", &pf); vx_prof = pf ? 1 : 0; /* .530: kept for development (the tools turn the profile on themselves); DELETE WHEN DEVELOPMENT IS FINISHED */}
					}
					{ /* v10.106: video overlay (V1), 0 off until VX2OVL has passed on the hardware: bits 0-1 register set, bit 2 RGB565 surfaces, bit 3 alternative width count, bit 4 colour key low bits replicated at 16bpp */
						extern volatile unsigned long vx2d_ovl, vx2d_ovl_cfg;
						acc = 961UL;
						{extern unsigned long vx_chip_id; if(vx_chip_id == 0x1122UL) acc = 5057UL;} /* .585 (board, VX2OVL v1.5): 961 | 4096 on the VX800: its 16-bit destination key is the pixel value itself (the VX855's widened forms, low bits zero or replicated, showed no picture there) */
						vx2d_ovl_cfg = (acc > 8191) ? 0 : acc; vx2d_ovl = vx2d_ovl_cfg;
					}
					{ /* v10.116: GDI drawing from system memory: ACCEL_HOST bits 1 mono SRCCOPY blits (host window), 2 mono blits whose ROP reads the screen (staging), 4 colour blits whose ROP reads the screen (staging),
					   * 8 text, 16 scan line spans whose ROP reads the screen; default 31, 0 turns all off. ACCEL_HOST_MIN (pixels, staging and spans) and ACCEL_MONO_MIN (pixels, mono SRCCOPY). */
						extern volatile unsigned long vx2d_host, vx2d_host_cfg, vx2d_host_min, vx2d_mono_min;
						{extern volatile unsigned long vx2d_op_wait,vx2d_op_wait_cfg,vx2d_dread,vx2d_dread_cfg; acc = 1; vx2d_op_wait_cfg = acc ? 1UL : 0UL; vx2d_op_wait = vx2d_op_wait_cfg; acc = 1; vx2d_dread_cfg = acc ? 1UL : 0UL; vx2d_dread = vx2d_dread_cfg;} /* v10.133 */
						{extern volatile unsigned long vx2d_span_wait,vx2d_span_wait_cfg,vx2d_str_fuse,vx2d_str_fuse_cfg; {extern volatile unsigned long vx2d_col8,vx2d_col8_cfg; acc = 1; /* v10.137: 1 = 8bpp colour-pattern bands on the engine */ vx2d_col8_cfg = acc ? 1UL : 0UL; vx2d_col8 = vx2d_col8_cfg;} acc = 5; /* v10.138: VX2TUNE soak, 540 random shape sets equal */ vx2d_span_wait_cfg = acc & 7UL; /* v10.134: bit 4 = the short wait for colour-pattern rectangles too */ vx2d_span_wait = vx2d_span_wait_cfg; acc = 1; vx2d_str_fuse_cfg = acc ? 1UL : 0UL; vx2d_str_fuse = vx2d_str_fuse_cfg;} /* v10.132 */
						acc = 30UL;       vx2d_host_cfg = acc & 63UL; /* v10.130: bit 32 = scan bands with write-only ROPs (COPYPEN shapes); v10.131: off by default (VX2WIDE: 0 differing pixels but 9.6 -> 31.8 ms at 16bpp; wide pens never arrive as bands) */ /* v10.117: mono SRCCOPY off (VX2GDI: no faster than DIBENG); v10.122: bit 8 text removed; v10.125: bit 8 = SaveScreenBitmap, on (VX2SAVE passed) */ vx2d_host = vx2d_host_cfg;
						acc = 256UL;  vx2d_host_min = acc;
						acc = 4096UL; vx2d_mono_min = acc;
					}
					{ /* v10.123: hardware gamma ramp (CR33 bit 7, the DAC as the table at 16/32 bpp); 0 turns it off (the ramp is then only stored, as before) */
						extern volatile unsigned long vx2d_gamma, vx2d_gamma_cfg;
						acc = 1; vx2d_gamma_cfg = acc & 1; vx2d_gamma = vx2d_gamma_cfg;
					}
					acc = 1;   vx2d_f16 = acc;
					acc = 1;    vx2d_f8 = acc; /* v10.76 */
					acc = 1; vx2d_fpat = acc;
					acc = 1; vx2d_fcol = acc; /* v10.77 */
					acc = 1;    vx2d_fclip = acc;
					acc = 1;   vx2d_fddraw = acc;
					{
						extern volatile unsigned long vx_flip_enable;
						acc = 1;        vx_flip_enable = acc;
						{extern volatile unsigned long vx_flip_async; acc = 1; vx_flip_async = acc ? 1UL : 0UL;} /* v10.384: the flip returns without waiting for the blank */
						{
							extern volatile unsigned long vx_scan_enable;
							acc = 1;   vx_scan_enable = acc; /* v10.73: 1 offers DirectDraw GetScanLine (default 0 until VX2AUTO has shown the counter is sane); v10.263: default 1 (VX2AUTO v1.1 on the board, v10.73b: SANE, raw = line << 16, past the last visible line only in the blank) */
						}
					}
				}
			}
		}
		
	}
}

/* generate all entry pro VDD function */
#define VDDFUNC(_fnname, _procname) void __declspec(naked) _procname ## _entry() { \
	_asm { push edi }; \
	_asm { push esi }; \
	_asm { push edx }; \
	_asm { push ecx }; /* extra_ecx */ \
	_asm { push eax }; /* extra_eax */ \
	_asm { push ebp }; /* state */ \
	_asm { push ebx }; /* vm */ \
	_asm { call _procname ## _proc }; \
	_asm { pop ebx }; \
	_asm { pop ebp }; \
	_asm { pop eax }; \
	_asm { pop ecx }; \
	_asm { pop edx }; \
	_asm { pop esi }; \
	_asm { pop edi }; \
	_asm { retn }; \
	}

#define VDDFUNCRET(_fnname, _procname) void __declspec(naked) _procname ## _entry() { \
	_asm { push edi }; \
	_asm { push esi }; \
	_asm { push edx }; \
	_asm { push ecx }; /* extra_ecx */ \
	_asm { push eax }; /* extra_eax */ \
	_asm { push ebp }; /* state */ \
	_asm { push ebx }; /* vm */ \
	_asm { call _procname ## _proc }; \
	_asm { sub eax, 1 }; /* set carry on zero */ \
	_asm { pop ebx }; \
	_asm { pop ebp }; \
	_asm { pop eax }; \
	_asm { pop ecx }; \
	_asm { pop edx }; \
	_asm { pop esi }; \
	_asm { pop edi }; \
	_asm { retn }; \
	}

#include "vxd_vdd_list.h"
#undef VDDFUNC
#undef VDDFUNCRET

#define VDDFUNC(id, procname) DispatchTable[id] = (DWORD)(procname ## _entry);
#define VDDFUNCRET VDDFUNC

/* init device and fill dispatch table */
BOOL Device_Init_proc(DWORD VM)
{
	if(device_ready) return TRUE;
	critical_section_enter();
	dbg_printf(dbg_Device_Init_proc);

	/* No control-block area is used by this driver. */
 	ThisVM = (DWORD)Get_Sys_VM_Handle();
 	DeviceCTX = _GetCurrentContext();



 /* Validate VDD registration before allocating buffers or scheduling a timer. */
 VDD_Get_Mini_Dispatch_Table();
 if(!DispatchTable || DispatchTableLength<0x31) {
  vx_status.failure=VX_NO_VDD_TABLE;
  critical_section_leave();
  return FALSE;
 }
	if(!VESA_init_hw()) {
  dbg_printf("VX85598 initialization stopped: stage=%ld failure=%ld\n",
   vx_status.stage,vx_status.failure);
  if(wram) {
   FBHDA_release_hw();
   _PageFree(wram,0);
   wram=NULL;
  }
  critical_section_leave();
  return FALSE;
 }

	Hook_V86_Int_Chain(0x10, ((DWORD)virtual_int_10h)+8);

	/* register miniVDD functions */
	if(DispatchTableLength >= 0x31)
	{
		#include "vxd_vdd_list.h"
	}

	configure_FBHDA();
	vx_lock_init(0); /* .515: also without the registry key (ACCEL_PRIVLOCK default 1) */
 device_ready=1;

	critical_section_leave();
	dbg_printf("Device_Init_proc DONE\n");
	/* v10.329: the VxD's memory log (VX_IOCTL_LOG) keeps what start-up wrote only with ACCEL_DEBUG_LOG = 1; otherwise it is emptied here and
	 * nothing more is written (a start-up that fails returns above with its log kept) */
	{extern void VX_log_enable(unsigned long on); DWORD lg = 0; RegReadConf(HKEY_LOCAL_MACHINE, reg_path, "ACCEL_DEBUG_LOG", &lg); VX_log_enable(lg);}
	dbg_printf("Address=0x%lX\n", port_info);
 return TRUE;
}
#undef VDDFUNC
#undef VDDNAKED

static DWORD io_open_cnt = 0;

/* process user space (PM32, RING-3) call */
#include "vx_native3d.h"
#include "vx_ring.h"
#include "vx_smp.h" /* v10.434 */
/* v10.432 (board, GTA3 v10.431: DrawPrimitives2 calls of 10 ms and more, the thread not running for most of them, up to 450 ms "after the
 * VxD's exit" in the HAL; 400-900 page-ins a second): the HAL's DLL (code and its shared data, about 1 MB) is kept in memory while a
 * Direct3D process holds it (VX_IOCTL_LOCKMEM). ACCEL_LOCK_HAL (default 1); 0 = never locked. vx_lock_stat: [0] pages locked now,
 * [1] their base, [2] lock requests, [3] refused or failed, [4] releases, [5] handles holding it. */
volatile unsigned long vx_lock_hal=1;uint32_t vx_lock_stat[6];
static DWORD lk_page,lk_n,lk_holder[64],lk_holders; /* v10.466: 64 holders (was 16) */
static void lk_release(void) {if(lk_n){DWORD i;for(i=0;i<lk_n;i++)_LinPageUnLock(lk_page+i,1,0);vx_lock_stat[4]++;}lk_n=0;lk_page=0;lk_holders=0;vx_lock_stat[0]=0;vx_lock_stat[1]=0;vx_lock_stat[5]=0;}
static void lk_drop(DWORD hdev) {DWORD k;for(k=0;k<lk_holders;k++)if(lk_holder[k]==hdev){lk_holder[k]=lk_holder[--lk_holders];break;}vx_lock_stat[5]=lk_holders;if(lk_n&&!lk_holders)lk_release();}
void VX_lock_request(DWORD op,DWORD a,DWORD n,DWORD hdev,DWORD *out) {
 DWORD page=a>>12,np=n?((a+n-1)>>12)-page+1:0,k,held=0;
 if(op==1){vx_lock_stat[2]++;
  if(!vx_lock_hal||a<0x80000000UL||a+n<a||a+n>0xC0000000UL||!np||np>2048)vx_lock_stat[3]++; /* .560: 2,048 pages (1,024: .559's image, 1,438 pages with the template table and the larger packets, was refused and not kept in memory) */
  else if(lk_n&&(lk_page!=page||lk_n!=np)&&!(lk_holders==1&&lk_holder[0]==hdev))vx_lock_stat[3]++; /* v10.466 (review): another range while
   another handle holds the lock is refused (it was released under them); the only holder may move it */
  else{if(lk_n&&(lk_page!=page||lk_n!=np))lk_release();
   /* v10.435: a page at a time, the ones done unlocked again if one fails: all or nothing whatever _LinPageLock does with a range it
    * cannot finish */
   if(!lk_n){DWORD i;for(i=0;i<np;i++)if(!_LinPageLock(page+i,1,0))break;
    if(i==np){lk_page=page;lk_n=np;vx_lock_stat[0]=np;vx_lock_stat[1]=a&~4095UL;}else{while(i--)_LinPageUnLock(page+i,1,0);vx_lock_stat[3]++;}}
   if(lk_n){for(k=0;k<lk_holders;k++)if(lk_holder[k]==hdev)held=1;if(!held){if(lk_holders<64)lk_holder[lk_holders++]=hdev;else vx_lock_stat[3]++;}vx_lock_stat[5]=lk_holders;}}}
 else if(op==0)lk_drop(hdev);
 out[0]=lk_n;out[1]=lk_page<<12;out[2]=lk_holders;out[3]=vx_lock_hal;
}
void VX_lock_close(DWORD hdev) {if(lk_holders)lk_drop(hdev);}
/* v10.434: the sampler sees whether the render thread is inside this call, while it is on. v10.439 (board, v10.438: "in the VxD" 87 ms of
 * a 132 ms gameplay frame, more than DrawPrimitives2 and Blt together): each thread inside has its own entry in the sampler's table
 * (VX_smp_enter / VX_smp_leave: depth and waits), nothing tied to which thread is the render thread; the render thread's time inside is
 * also measured (TSC). */
static DWORD __stdcall Device_IO_Control_body(DWORD vmhandle, struct DIOCParams *params);
DWORD __stdcall Device_IO_Control_proc(DWORD vmhandle, struct DIOCParams *params)
{
	extern uint32_t vx_smp_active(void);extern void VX_smp_enter(uint32_t *,uint32_t *);extern void VX_smp_leave(uint32_t,uint32_t,uint32_t);uint32_t s,t0,rc;
	if(!vx_smp_active())return Device_IO_Control_body(vmhandle,params);
	{extern volatile uint32_t vx_smp_ph;uint32_t o=vx_smp_ph,c=params->dwIoControlCode; /* .520: the sampler's phase for the call */
	 vx_smp_ph=c==VX_IOCTL_3DN_SUBMITN?VX_SMP_PH_SUBMIT:c==VX_IOCTL_3DN_SUBMIT?VX_SMP_PH_SINGLE:VX_SMP_PH_CALL;
	 VX_smp_enter(&s,&t0);rc=Device_IO_Control_body(vmhandle,params);VX_smp_leave(s,t0,c);vx_smp_ph=o;return rc;}
}
static DWORD __stdcall Device_IO_Control_body(DWORD vmhandle, struct DIOCParams *params)
{
	DWORD *inBuf  = (DWORD*)params->lpInBuffer;
	DWORD *outBuf = (DWORD*)params->lpOutBuffer;
	DWORD rc = 1;
 /* Diagnostics and an explicitly armed, fixed offscreen GPU test only. */
 if(params->dwIoControlCode!=VX_IOCTL_STATUS &&
    params->dwIoControlCode!=VX_IOCTL_LOG &&
    params->dwIoControlCode!=VX_IOCTL_3D_PROBE &&
    params->dwIoControlCode!=VX_IOCTL_3D_TEST &&
    params->dwIoControlCode!=VX_IOCTL_2D_CTRL &&
    params->dwIoControlCode!=VX_IOCTL_2D_SUBMIT &&
    params->dwIoControlCode!=VX_IOCTL_3DN_INFO &&
    params->dwIoControlCode!=VX_IOCTL_3DN_SUBMIT &&
    params->dwIoControlCode!=VX_IOCTL_3DN_SUBMITN && /* v10.275 */
    params->dwIoControlCode!=VX_IOCTL_LOCKMEM && /* v10.432 */
    params->dwIoControlCode!=VX_IOCTL_SAMPLE && /* v10.434 */
    params->dwIoControlCode!=VX_IOCTL_3DN_QUERY && /* v10.316 (board, v10.315: VXGL.LOG "the engine's count could not be set": the call was refused here with error 50, as VXRING's in v10.223) */
    params->dwIoControlCode!=VX_IOCTL_3DN_TEXDBG &&
    params->dwIoControlCode!=VX_IOCTL_2D_STATS &&
    params->dwIoControlCode!=VX_IOCTL_2D_ACCESS &&
    params->dwIoControlCode!=VX_IOCTL_FLIP &&
    params->dwIoControlCode!=VX_IOCTL_EDID &&
    params->dwIoControlCode!=VX_IOCTL_CRT &&
    params->dwIoControlCode!=VX_IOCTL_PROBE && /* v10.333 */
    params->dwIoControlCode!=VX_IOCTL_CACHE &&
    params->dwIoControlCode!=VX_IOCTL_CACHE_SET &&
    params->dwIoControlCode!=VX_IOCTL_DAC &&
    params->dwIoControlCode!=VX_IOCTL_OVL &&
    params->dwIoControlCode!=VX_IOCTL_RING && /* v10.224 (v10.223 left it out: VXRING got error 50) */
    params->dwIoControlCode!=VX_IOCTL_TAPE && /* v10.393 */
    params->dwIoControlCode!=DIOC_OPEN &&
    params->dwIoControlCode!=DIOC_CLOSEHANDLE) return 50;
	
	//Begin_Critical_Section(0);
	
	//dbg_printf("I%x\n", params->dwIoControlCode);
	
	switch(params->dwIoControlCode)
 {
  case VX_IOCTL_3DN_INFO:
   if(!outBuf||params->cbOutBuffer<sizeof(vx3dn_info)||!params->lpcbBytesReturned)return 87;
   VX3DN_info((vx3dn_info*)outBuf);*(DWORD*)params->lpcbBytesReturned=sizeof(vx3dn_info);return 0;
  case VX_IOCTL_3DN_SUBMIT:
   {extern void VX_smp_set_render(void);VX_smp_set_render();} /* v10.434; v10.437 */
   if(!inBuf||(params->cbInBuffer!=sizeof(vx3dn_packet)&&params->cbInBuffer!=VX3DN_PACKET_V1_BYTES)||!outBuf||params->cbOutBuffer<4||!params->lpcbBytesReturned)return 87;
   {extern void VX3DN_io_mark(uint32_t *o,int at_exit);uint32_t io[4];VX3DN_io_mark(io,0); /* v10.414 */
   if(params->cbInBuffer==sizeof(vx3dn_packet)) outBuf[0]=VX3DN_submit((const vx3dn_packet*)inBuf);
   else {static vx3dn_packet v1; vx_lock(); memset(&v1,0,sizeof(v1)); memcpy(&v1,inBuf,VX3DN_PACKET_V1_BYTES); outBuf[0]=VX3DN_submit(&v1); vx_unlock();} /* v10.169: the untextured packet of v10.149; v10.288 (review): the shared copy under the critical section (another thread's could tear it) */
   VX3DN_io_mark(io,1);if(params->cbOutBuffer>=20){outBuf[1]=io[0];outBuf[2]=io[1];outBuf[3]=io[2];outBuf[4]=io[3];*(DWORD*)params->lpcbBytesReturned=20;return 0;}}
   *(DWORD*)params->lpcbBytesReturned=4;return 0;
  case VX_IOCTL_3DN_SUBMITN: /* v10.275 */
   {extern void VX_smp_set_render(void);VX_smp_set_render();} /* v10.434; v10.437 */
   {extern uint32_t VX3DN_submit_batch(const vx3dn_packet *,uint32_t,uint32_t *);uint32_t n=params->cbInBuffer/sizeof(vx3dn_packet),done=0;
    if(!inBuf||!n||n>VX3DN_BATCH_MAX||params->cbInBuffer!=n*sizeof(vx3dn_packet)||!outBuf||params->cbOutBuffer<8||!params->lpcbBytesReturned)return 87;
    {extern void VX3DN_io_mark(uint32_t *o,int at_exit);uint32_t io[4];VX3DN_io_mark(io,0); /* v10.414 */
    outBuf[0]=VX3DN_submit_batch((const vx3dn_packet*)inBuf,n,&done);outBuf[1]=done;
    VX3DN_io_mark(io,1);if(params->cbOutBuffer>=24){outBuf[2]=io[0];outBuf[3]=io[1];outBuf[4]=io[2];outBuf[5]=io[3];*(DWORD*)params->lpcbBytesReturned=24;return 0;}}
    *(DWORD*)params->lpcbBytesReturned=8;return 0;}
  case VX_IOCTL_SAMPLE: /* v10.434 */
   if(!inBuf||!outBuf||!params->lpcbBytesReturned)return 87;
   {extern DWORD VX_smp_ioctl(const DWORD *,DWORD,DWORD *,DWORD,DWORD);*(DWORD*)params->lpcbBytesReturned=VX_smp_ioctl(inBuf,params->cbInBuffer,outBuf,params->cbOutBuffer,params->hDevice);}
   return 0;
  case VX_IOCTL_LOCKMEM: /* v10.432 */
   if(!inBuf||params->cbInBuffer<12||!outBuf||params->cbOutBuffer<16||!params->lpcbBytesReturned)return 87;
   VX_lock_request(inBuf[0],inBuf[1],inBuf[2],params->hDevice,outBuf);*(DWORD*)params->lpcbBytesReturned=16;return 0;
  case VX_IOCTL_3DN_QUERY: /* v10.315: occlusion count */
   if(!inBuf||params->cbInBuffer<4||!outBuf||params->cbOutBuffer<8||!params->lpcbBytesReturned)return 87;
   {uint32_t n=0;outBuf[0]=VX3DN_query(inBuf[0],params->cbInBuffer>=8?inBuf[1]:1,&n);outBuf[1]=n;} /* v10.322: the owner */
   *(DWORD*)params->lpcbBytesReturned=8;return 0;
  case VX_IOCTL_3DN_TEXDBG: /* v10.174 */
   if(!inBuf||params->cbInBuffer<4||!outBuf||params->cbOutBuffer<32||!params->lpcbBytesReturned)return 87;
   {const uint32_t q=inBuf[0]; /* v10.232: a debug switch can change what the emitter sends: the state cache starts over; v10.389: not for 0xBD (read every frame);
    * v10.476: nor for the other codes that only read or clear counters or turn the profile's clock on or off (BC's reads, BA/BB, B8/B9,
    * C8/C9, 6C-6E, 75): each tool's reading made the next packet send its whole state again, in the middle of what it measured */
    if(!(q==0xBC||q==0xBD||q==0xBA||q==0xBB||q==0xB8||q==0xB9||q==0xC8||q==0xC9||(q>=0x6C&&q<=0x6E)||q==0x75||
         q==0x76||q==0x77||q==0xA4||q==0xA8||q==0xA9||q==0xAC||q==0xAD||q==0xB0||q==0xB1||q==0xBE||q==0xBF||q==0xC0||q==0xC1||
         q==0x34||(q>=0x52&&q<=0x54)||(q>=0x64&&q<=0x68)||q==0x71||q==0x74||q==0x7A||q==0x7C||q==0xA3||q==0xA5||(q>=0xB5&&q<=0xB7)||
         q==0xCE||q==0xF3||q==0xF4||(q>=0xE0&&q<=0xEF)))vx3dn_forget();}
    /* v10.479 (review): and the last reads: 34 dither, 52-54 set-up, 64-68 latencies, 71 recoveries, 74 burst, 7A posted, 7C idle, A3 fetch,
     * A5 2D loss, B5-B7 stencil, CE / F3 / F4 switches, E0-EF the hung group */
    /* v10.477: the rest of the read and read-and-clear codes (76/77 the profile counters, A4 the fetcher's, A8/A9 the state cache's, AC/AD the
     * submits', B0/B1 the strips', BE/BF the frames', C0/C1 the batches'): VXD3DMON's A/B read 76 and B8 every 2 s, each period started with
     * the whole state sent again */
   /* .530: the switches and probes (00-08, 20-23, 30-33, 40-44, 48-4C, 50-51, 58-5F, 60-63, 71, 72-73, 78-79, 81-A0, A1-A2, A6-A7, AA-AB, AE-AF,
    * B2-B4, CA-CD, F0-F2) removed: their values fixed at the board's defaults; the reads and counters below stay */
    /* .530: 6C/6D (the profile on and off) and BC A2/A3 (the command words by part) are development hooks: DELETE WHEN DEVELOPMENT IS FINISHED */
   {extern volatile uint32_t vx_prof;uint32_t q=inBuf[0]; /* v10.329: the profile on (6C, or a request that clears one: the test programs and VXGL_LOG measure from there) or off (6D) */
    if(q==0x6C||q==0x68||q==0x77||q==0xA4||q==0xA9||q==0xAD||q==0xB0||q==0xC1||q==0xC9||q==0xB9)vx_prof=1;else if(q==0x6D)vx_prof=0;}
   {int k;for(k=0;k<8;k++)outBuf[k]=0;}
   if(inBuf[0]>=0x6C&&inBuf[0]<=0x6E){extern volatile uint32_t vx_prof;outBuf[0]=0x6E;outBuf[1]=vx_prof;} /* v10.329: 6E reads */
   if(inBuf[0]==0x75){extern unsigned long vx_chip_id,vx_chip_fb_cfg,vx_chip_mmio_cfg;extern int VX_chip_accel(void);outBuf[0]=0x75;outBuf[1]=DRV_VER_BUILD; /* v10.214: the VxD's build number */
    outBuf[2]=vx_chip_id;outBuf[3]=vx_chip_fb_cfg|(vx_chip_mmio_cfg<<8);outBuf[4]=(DWORD)VX_chip_accel();outBuf[5]=vx_status.bar0;outBuf[6]=vx_status.bar1;outBuf[7]=vx_status.stage|(vx_status.failure<<8);} /* .579: the device, its BARs' configuration offsets and bases, whether the engines are used, the bring-up stage and failure */
   if(inBuf[0]==0x7C){extern uint32_t vx3dn_idle_stable;outBuf[0]=0x7C;outBuf[1]=vx3dn_idle_stable;} /* v10.221: idle reads */
   if(inBuf[0]>=0x64&&inBuf[0]<=0x68){extern void VX3DN_lat(uint32_t,uint32_t *);VX3DN_lat(inBuf[0],(uint32_t*)outBuf);} /* v10.321: latency distributions (vx_native3d_vxd.c) */
   if(inBuf[0]==0x76||inBuf[0]==0x77){extern uint32_t vx3dn_prof[8];int k;outBuf[0]=inBuf[0];for(k=0;k<7;k++)outBuf[1+k]=vx3dn_prof[k];if(inBuf[0]==0x77)for(k=0;k<8;k++)vx3dn_prof[k]=0;} /* v10.220: profile */
   if(inBuf[0]==0xA3||inBuf[0]==0xA4){extern volatile uint32_t vx3dn_fetch,vx3dn_async;extern uint32_t vx3dn_fetch_stat[6];extern volatile unsigned long vx3dn_cmd_off;int k;
    outBuf[0]=vx3dn_fetch?0xA1:0xA2;for(k=0;k<5;k++)outBuf[1+k]=vx3dn_fetch_stat[k];outBuf[6]=vx3dn_cmd_off;outBuf[7]=(vx3dn_async?0x80000000UL:0)|(vx3dn_fetch_stat[5]&0x7FFFFFFFUL);if(inBuf[0]==0xA4)for(k=0;k<6;k++)vx3dn_fetch_stat[k]=0;} /* v10.231: [7] bit 31 async, the rest draws left running */ /* v10.228: fetch on/off, statistics, block offset */
   if(inBuf[0]==0xB0||inBuf[0]==0xB1){extern uint32_t vx3dn_strip_hw,vx3dn_strip_stat[3];int k;outBuf[0]=0xAE;outBuf[1]=vx3dn_strip_hw;for(k=0;k<3;k++)outBuf[2+k]=vx3dn_strip_stat[k];if(inBuf[0]==0xB0)for(k=0;k<3;k++)vx3dn_strip_stat[k]=0;} /* v10.236: AE/AF strips on/off; B0 reads and clears, B1 only reads: strip runs, strip vertices, list runs */
   if(inBuf[0]==0xBE||inBuf[0]==0xBF){extern uint32_t vx_frame_stat[16],vx_frame_worst[8][3];extern volatile unsigned long vx_flip_async;uint32_t i=params->cbInBuffer>=8?inBuf[1]:0,k; /* v10.384: frame timing; BF reads and clears */
    for(k=0;k<8;k++)outBuf[k]=0;outBuf[0]=0xBE;outBuf[1]=i;
    if(i<3)for(k=0;k<6&&i*6+k<16;k++)outBuf[2+k]=vx_frame_stat[i*6+k];
    else if(i<11){outBuf[2]=vx_frame_worst[i-3][0];outBuf[3]=vx_frame_worst[i-3][1];outBuf[4]=vx_frame_worst[i-3][2];}
    else if(i==11)outBuf[2]=vx_flip_async;
    if(inBuf[0]==0xBF){for(k=0;k<16;k++)vx_frame_stat[k]=0;for(k=0;k<8;k++)vx_frame_worst[k][0]=vx_frame_worst[k][1]=vx_frame_worst[k][2]=0;}}
   if(inBuf[0]==0xBD){extern uint32_t vx3dn_sv_worst[8][10],vx3dn_sv_count;uint32_t i=params->cbInBuffer>=8?inBuf[1]:0,k; /* v10.388: slow batch calls */
    for(k=0;k<8;k++)outBuf[k]=0;outBuf[0]=0xBD;outBuf[1]=i;
    if(i==8)outBuf[2]=vx3dn_sv_count;else if(i<8)for(k=0;k<6;k++)outBuf[2+k]=vx3dn_sv_worst[i][k];else if(i>=0x10&&i<0x18){outBuf[2]=vx3dn_sv_worst[i-0x10][6];outBuf[3]=vx3dn_sv_worst[i-0x10][7];outBuf[4]=vx3dn_sv_worst[i-0x10][8];outBuf[5]=vx3dn_sv_worst[i-0x10][9];}
    else if(i==0x81){DWORD di[32];outBuf[2]=Get_Thread_Exec_Time(Get_Cur_Thread_Handle());outBuf[3]=Get_System_Time();for(k=0;k<32;k++)di[k]=0; /* (16 dwords the DDK's DemandInfoStruc; room to spare) */vx_demand_info(di);outBuf[4]=di[6];outBuf[5]=di[7];} /* .525: [4] the VMM's page faults, [5] page-ins (the HAL reads them with the run time, every 16th call) */
    else if(i==0x82){extern uint32_t vx_lock_stat[6];for(k=0;k<6;k++)outBuf[2+k]=vx_lock_stat[k];} /* v10.433: the HAL lock (VX_IOCTL_LOCKMEM) */ /* v10.389: the caller's run time and the time (ms) */
    else if(i>=0x20&&i<0x30){extern uint32_t vx3dn_ss_worst[8][8];for(k=0;k<4;k++)outBuf[2+k]=vx3dn_ss_worst[(i-0x20)&7][(i>=0x28?4:0)+k];} /* v10.393: slow single submits */
    else if(i==0x30){extern uint32_t vx3dn_ss_count;outBuf[2]=vx3dn_ss_count;}
    else if(i==0x80){extern uint32_t vx3dn_ss_worst[8][8],vx3dn_ss_count;vx3dn_sv_count=0;for(k=0;k<80;k++)vx3dn_sv_worst[k/10][k%10]=0;vx3dn_ss_count=0;for(k=0;k<64;k++)vx3dn_ss_worst[k/8][k%8]=0;}}
   if(inBuf[0]==0xBC){extern uint32_t vx3dn_rhang[2][6],vx3dn_rhang_trace[64][10],vx3dn_rhang_words[1024],vx3dn_rhang_wn[4],vx3dn_rhang_ok,vx3dn_rf_stat[6];extern volatile uint32_t vx3dn_ring_fence;
    uint32_t i=params->cbInBuffer>=8?inBuf[1]:0,k; /* v10.383: the ring's first hang: 0-1 its header, 2-65 the groups kicked before it (oldest first), 66-73 the words of the group after the fence; 0x80 the fence counters */
    for(k=0;k<8;k++)outBuf[k]=0;outBuf[0]=0xBC|(vx3dn_rhang_ok<<8);outBuf[1]=i;
    if(i<2)for(k=0;k<6;k++)outBuf[2+k]=vx3dn_rhang[i][k];
    else if(i<66)for(k=0;k<6;k++)outBuf[2+k]=vx3dn_rhang_trace[i-2][k];
    else if(i<74)for(k=0;k<6;k++)outBuf[2+k]=vx3dn_rhang_words[(i-66)*6+k];
    else if(i==0x81)for(k=0;k<4;k++)outBuf[2+k]=vx3dn_rhang_wn[k]; /* v10.386: the two groups' words and starts */
    else if(i>=0x100&&i<0x140)for(k=0;k<4;k++)outBuf[2+k]=vx3dn_rhang_trace[i-0x100][6+k]; /* v10.386: the groups' first packets */
    else if(i>=0x200&&i<0x200+171)for(k=0;k<6;k++){uint32_t w=(i-0x200)*6+k;outBuf[2+k]=w<1024?vx3dn_rhang_words[w]:0;} /* v10.386: both groups' words (6 a read) */
    else if(i==0x80){for(k=0;k<4;k++)outBuf[2+k]=vx3dn_rf_stat[k];outBuf[6]=vx3dn_ring_fence;}
    else if(i==0x92||i==0x93){extern uint32_t vx3dn_xr_prof[12];for(k=0;k<6;k++)outBuf[2+k]=vx3dn_xr_prof[(i-0x92)*6+k];} /* v10.407 */
    else if(i==0xD6){extern uint32_t vx3dn_xr_unit[3];for(k=0;k<3;k++)outBuf[2+k]=vx3dn_xr_unit[k];} /* .504: the ring's cache work by unit (vx_ring.h); .512: D6 (it took 9B, the state-cache misses' first read: VXD3DMON showed the cache counts as miss percentages) */
    else if(i==0x95){extern uint32_t vx3dn_xr_stat2[6];for(k=0;k<6;k++)outBuf[2+k]=vx3dn_xr_stat2[k];} /* v10.410 */
    else if(i==0x96){extern uint32_t vx3dn_pf_stat[2];extern volatile uint32_t vx3dn_fastblk,vx3dn_prefetch;outBuf[2]=outBuf[3]=outBuf[4]=0;outBuf[5]=vx3dn_pf_stat[0];outBuf[6]=vx3dn_pf_stat[1];outBuf[7]=(vx3dn_fastblk<<1)|(vx3dn_prefetch<<2);} /* v10.412 */
    else if(i==0x97){extern uint32_t vx3dn_frontz_n;extern volatile uint32_t vx3dn_frontz;outBuf[2]=vx3dn_frontz;outBuf[3]=vx3dn_frontz_n;} /* v10.416 */
    else if(i==0x98||i==0x99){extern uint32_t vx3dn_fr_stat[8],vx_flip_ring_stat[4];extern volatile uint32_t vx3dn_flip_ring;if(i==0x98)for(k=0;k<6;k++)outBuf[2+k]=vx3dn_fr_stat[k];
     else{outBuf[2]=vx3dn_fr_stat[6];outBuf[3]=vx3dn_fr_stat[7];for(k=0;k<3;k++)outBuf[4+k]=vx_flip_ring_stat[k];outBuf[7]=vx3dn_flip_ring;}} /* v10.417 */
    else if(i==0x9A){extern uint32_t vx3dn_rc_stat[2],vx3dn_rt_stat[4];extern volatile uint32_t vx3dn_ringclear;outBuf[2]=vx3dn_rc_stat[0];outBuf[3]=vx3dn_rc_stat[1];for(k=0;k<4;k++)outBuf[4+k]=vx3dn_rt_stat[k];outBuf[1]|=vx3dn_ringclear<<8;} /* v10.418 */
    else if(i>=0x9B&&i<=0x9D){extern uint32_t vx3dn_miss_stat[18];for(k=0;k<6;k++)outBuf[2+k]=vx3dn_miss_stat[(i-0x9B)*6+k];} /* v10.429 */
    else if(i>=0x9E&&i<=0xA1){extern uint32_t vx3dn_wstat[24];for(k=0;k<6;k++)outBuf[2+k]=vx3dn_wstat[(i-0x9E)*6+k];} /* v10.440: the command words by part */
    else if(i==0xA2||i==0xA3){extern void vx3dn_wprof_set(int);vx3dn_wprof_set(i==0xA2);}
    else if(i==0xA4){extern uint32_t vx3dn_pre_stat[4];extern volatile unsigned long vx_d3d_feat_cfg;outBuf[2]=vx3dn_pre_stat[0];outBuf[3]=vx3dn_pre_stat[1];outBuf[4]=(vx_d3d_feat_cfg>>26)&1;outBuf[5]=vx3dn_pre_stat[2];outBuf[6]=vx3dn_pre_stat[3];outBuf[7]=0;} /* v10.441; [6] bytes copied in / 16 (v10.443); [5] v10.449: blocks left to the emitter (ACCEL_D3D_ONECOPY) */
    else if(i==0xA5){extern uint32_t vx3dn_pre_stat[4];for(k=0;k<4;k++)vx3dn_pre_stat[k]=0;} /* v10.443: cleared */
    else if((i>=0xD0&&i<=0xD5)||(i>=0xD7&&i<=0xDA)){extern uint32_t vx_csw[6],vx_csw_key[24],vx_csw_n[24]; /* .512: what ran while a thread waited for the system critical section; D5 clears; .513: D1-D4 and D7-DA (24 places) */
     if(i==0xD0)for(k=0;k<6;k++)outBuf[2+k]=vx_csw[k];
     else if(i!=0xD5){uint32_t b=(i<0xD5?i-0xD1:i-0xD7+4)*3;for(k=0;k<3;k++){outBuf[2+2*k]=vx_csw_key[b+k];outBuf[3+2*k]=vx_csw_n[b+k];}}
     else{for(k=0;k<6;k++)vx_csw[k]=0;for(k=0;k<24;k++){vx_csw_key[k]=0;vx_csw_n[k]=0;}}}
    else if(i==0xFA){extern uint32_t vx3dn_seq_stat[3];outBuf[2]=1;for(k=0;k<3;k++)outBuf[3+k]=vx3dn_seq_stat[k];} /* .528: the batch call's sequencer writes (.529: always skipped when unchanged; F8/F9 removed): [3] batch calls opened, [4] writes left out, [5] closes with nothing to put back */
    else if(i==0xF5||i==0xF7){extern uint32_t vx3dn_texrun_stat[2];outBuf[2]=vx3dn_texrun_stat[0];outBuf[3]=vx3dn_texrun_stat[1];if(i==0xF7){vx3dn_texrun_stat[0]=0;vx3dn_texrun_stat[1]=0;}} /* .528: texture runs: F5 reads [2] packets drawn with runs, [3] runs drawn (texture switches inside packets); F7 reads and clears */
    else if(i==0xEC||i==0xED||i==0xEE){extern uint32_t vx3dn_bad[12];if(i==0xEE)for(k=0;k<12;k++)vx3dn_bad[k]=0;else for(k=0;k<6;k++)outBuf[2+k]=vx3dn_bad[(i-0xEC)*6+k];} /* .527: the last packet refused (vx_native3d_vxd.c bad_note): EC [0]-[5], ED [6]-[11], EE clears (EC-EE were .526's FASTCHK, .525's IXCHK, both removed) */
    else if(i==0xE9||i==0xEA||i==0xEB){extern volatile uint32_t vx3dn_shadow;extern uint32_t vx3dn_sh_stat[2];outBuf[2]=vx3dn_shadow;outBuf[3]=vx3dn_sh_stat[0];outBuf[4]=vx3dn_sh_stat[1];} /* .525: ACCEL_D3D_SHADOW off / on / read: [3] set-ups with the shadow in use, [4] type-01 words left out */
    else if(i==0xE6||i==0xE7||i==0xE8){extern volatile uint32_t vx3dn_delta;extern uint32_t vx3dn_delta_stat[5];outBuf[2]=vx3dn_delta;for(k=0;k<5;k++)outBuf[3+k]=vx3dn_delta_stat[k];} /* .524: ACCEL_D3D_DELTA off / on / read: [3] packets with the HAL's delta word honoured, [4] ranges kept, [5] state bytes not read (16-byte units), [6] packets whose state's checks were not done again, [7] tagged packets copied whole */
    else if(i==0xE5){extern uint32_t vx3dn_v8_pk,vx3dn_v8_lay,vx3dn_v8_full,vx3dn_lay8;outBuf[2]=vx3dn_v8_pk;outBuf[3]=vx3dn_v8_lay;outBuf[4]=vx3dn_v8_full;outBuf[5]=vx3dn_lay8;} /* .523: 8-word packets drawn, their layout written, whole set-ups made for the layout, the engine's layout now */
    else if(i==0xDB||i==0xDC){extern uint32_t vx_lk[8];if(i==0xDB){outBuf[2]=vx_lk[0];outBuf[3]=vx_lk[1];outBuf[4]=vx_lk[2];outBuf[5]=vx_lk[3];outBuf[6]=vx_lk[4];outBuf[7]=vx_lk[5]|(vx_lk[7]<<30);}else{outBuf[2]=vx_lk[6];outBuf[3]=vx_lk[7];for(k=4;k<8;k++)outBuf[k]=0;for(k=0;k<7;k++)vx_lk[k]=0;}} /* .515: the engine lock: DB entries, waits, wait time, longest wait, waits 2 ms+, longest hold | mode << 30; DC holds 2 ms+ and the mode, then clears */
    else if(i==0xBA){extern uint32_t vx3dn_rp_hist[6];for(k=0;k<6;k++)outBuf[2+k]=vx3dn_rp_hist[k];} /* v10.467: the ring waits by length */
    else if(i==0xB9){extern volatile uint32_t vx3dn_ringprog;extern uint32_t vx3dn_rp_stat[2];outBuf[2]=vx3dn_ringprog;outBuf[3]=vx3dn_rp_stat[0];outBuf[4]=vx3dn_rp_stat[1];} /* v10.465: ACCEL_D3D_RINGPROG, waits past 100 ms on progress, the longest wait (ms) */
    else if(i==0xB5||i==0xB6||i==0xB7){extern volatile uint32_t vx3dn_ringblt;extern uint32_t vx3dn_rb_stat[8];outBuf[2]=vx3dn_ringblt;for(k=0;k<5;k++)outBuf[3+k]=vx3dn_rb_stat[k];} /* v10.453: ACCEL_D3D_RINGBLT off / on / read; copies in the ring, not taken, by the 2D engine, declined there, the 2D path's wait */
    else if(i==0xE0||i==0xE4){extern uint32_t vx3dn_ix16_stat[3]; /* .555: the index bursts' counts (heads, data words, indices): E4 reads, E0 clears; [2] = 3, the form (.554's mode 3) */
     if(i==0xE0)for(k=0;k<3;k++)vx3dn_ix16_stat[k]=0;
     outBuf[2]=3;for(k=0;k<3;k++)outBuf[3+k]=vx3dn_ix16_stat[k];}
    else if(i==0xF0){extern uint32_t vx3dn_ix_wait[2],vx3dn_va_nslots,vx3dn_reg_draws; /* .564: indexed packets drawn by index after a wait for the slots, of them still too large, the slots in use, draws through the registers. .566: F1 and F2 (.564-.565: three slots and every slot, for the tool VXSLOT) removed: the wait is confirmed on the board (.565: 9,487 packets, none through the registers) */
     outBuf[2]=vx3dn_ix_wait[0];outBuf[3]=vx3dn_ix_wait[1];outBuf[4]=vx3dn_va_nslots;outBuf[5]=vx3dn_reg_draws;}
    else if(i==0xC6||i==0xC7){extern uint32_t vx3dn_ix_stat[6],vx3dn_va_stat[4],vx3dn_va_off; /* v10.470: indexed packets: by index, expanded, vertices uploaded,
     * indices, slot refusals (in flight + own group), the area's offset; C7 clears */
     outBuf[2]=vx3dn_ix_stat[0];outBuf[3]=vx3dn_ix_stat[1];outBuf[4]=vx3dn_ix_stat[2];outBuf[5]=vx3dn_ix_stat[3];outBuf[6]=vx3dn_va_stat[1]+vx3dn_va_stat[2];outBuf[7]=vx3dn_va_off;
     if(i==0xC7){for(k=0;k<6;k++)vx3dn_ix_stat[k]=0;for(k=0;k<3;k++)vx3dn_va_stat[k]=0;}}
    else if(i==0xCA||i==0xCB){extern uint32_t vx3dn_rt_cause[8][2];for(k=0;k<6;k++)outBuf[2+k]=vx3dn_rt_cause[k][i==0xCB];} /* v10.473: the CPU-access retires by cause: counts, time (TSC / 256) */
    else if(i==0xCC||i==0xCD||i==0xCE){extern uint32_t vx3dn_ixp[8],vx3dn_rt_cause[8][2]; /* v10.473: indexed packets' time in the VxD; CE clears it */
     if(i==0xCC)for(k=0;k<6;k++)outBuf[2+k]=vx3dn_ixp[k];
     else if(i==0xCD){outBuf[2]=vx3dn_ixp[6];outBuf[3]=vx3dn_ixp[7];outBuf[4]=vx3dn_rt_cause[6][0];outBuf[5]=vx3dn_rt_cause[6][1];outBuf[6]=vx3dn_rt_cause[7][0];outBuf[7]=vx3dn_rt_cause[7][1];}
     else for(k=0;k<8;k++)vx3dn_ixp[k]=0;}
    else if(i==0xBB){extern uint32_t vx3dn_rprof[5];for(k=0;k<5;k++)outBuf[2+k]=vx3dn_rprof[k];} /* v10.468: the burst heads, bursts, runs, the time before the first burst, packets */
    else if(i==0xAE||i==0xAF){extern uint32_t vx3dn_vprof[8];outBuf[2]=vx3dn_vprof[0];outBuf[3]=vx3dn_vprof[1];outBuf[4]=vx3dn_vprof[2];outBuf[5]=vx3dn_vprof[3];outBuf[6]=vx3dn_vprof[5];outBuf[7]=vx3dn_vprof[4];if(i==0xAF){extern uint32_t vx3dn_rprof[5];for(k=0;k<8;k++)vx3dn_vprof[k]=0;for(k=0;k<5;k++)vx3dn_rprof[k]=0;}} /* v10.468: AF clears the run profile too */ /* v10.461: the vertex copy's parts: copies, checks, words, sampled second copies, the same calls' first copies, their words (AF clears) */
    else if(i==0xB8){extern uint32_t vx3dn_rb_stat[8],vx3dn_rt_stat[4];outBuf[2]=vx3dn_rb_stat[5];outBuf[3]=vx3dn_rb_stat[6];outBuf[4]=vx3dn_rt_stat[0];outBuf[5]=vx3dn_rt_stat[1];} /* v10.453: rectangles empty after the clip, the ring copies' time; all CPU-access retires and their time */
    else if(i==0xB2||i==0xB3||i==0xB4){extern volatile uint32_t vx3dn_texonly;extern uint32_t vx3dn_texonly_stat[2];outBuf[2]=vx3dn_texonly;outBuf[3]=vx3dn_texonly_stat[0];outBuf[4]=vx3dn_texonly_stat[1];} /* v10.450: ACCEL_D3D_TEXONLY off / on / read; packets sent textures-only, misses tested */
    else if(i==0x94){extern uint32_t vx3dn_xr_prof[12];for(k=0;k<12;k++)vx3dn_xr_prof[k]=0;}
    else if(i>=0x86&&i<0x92){extern uint32_t vx3dn_xr_stat[72];extern volatile uint32_t vx3dn_xr;for(k=0;k<6;k++)outBuf[2+k]=vx3dn_xr_stat[(i-0x86)*6+k];if(i==0x86)outBuf[1]=i|(vx3dn_xr<<8);} /* v10.402: XP's ring */
    else if(i==0x82){outBuf[2]=vx3dn_rf_stat[4];outBuf[3]=vx3dn_rf_stat[5];{extern uint32_t vx3dn_wc_stat[2];outBuf[6]=vx3dn_wc_stat[0];outBuf[7]=vx3dn_wc_stat[1];}}} /* v10.388: + program changes, waits for them */ /* v10.387: kicks written again, and those the fence then came for */
   if(inBuf[0]==0xBA||inBuf[0]==0xBB){extern uint32_t vx3dn_ring_stat[3],vx3dn_gap_stat[3];extern volatile uint32_t vx3dn_xr;int k;outBuf[0]=0xBA;outBuf[1]=vx3dn_xr;for(k=0;k<3;k++){outBuf[2+k]=vx3dn_ring_stat[k];outBuf[5+k]=vx3dn_gap_stat[k];}if(inBuf[0]==0xBB)for(k=0;k<3;k++){vx3dn_ring_stat[k]=0;vx3dn_gap_stat[k]=0;}} /* v10.382: [5..7] the gap wipes */ /* v10.374: the ring: on, starts, kicks, wraps */
   if(inBuf[0]==0xB8||inBuf[0]==0xB9){extern uint32_t vx3dn_bat_prof[4];int k;outBuf[0]=0xB8;for(k=0;k<4;k++)outBuf[1+k]=vx3dn_bat_prof[k];if(inBuf[0]==0xB9)for(k=0;k<4;k++)vx3dn_bat_prof[k]=0;} /* v10.370 (v10.369 used CA/CB, the constant-group switch) */
   if(inBuf[0]==0xC8||inBuf[0]==0xC9){extern uint32_t vx3dn_bprof[4],vx3dn_eprof[4];int k;outBuf[0]=0xC8;for(k=0;k<3;k++)outBuf[1+k]=vx3dn_bprof[k];for(k=0;k<4;k++)outBuf[4+k]=vx3dn_eprof[k];if(inBuf[0]==0xC9){for(k=0;k<4;k++){vx3dn_bprof[k]=0;vx3dn_eprof[k]=0;}}} /* v10.286 */
   if(inBuf[0]>=0xE0&&inBuf[0]<=0xEF){extern uint32_t vx3dn_hang[16][6],vx3dn_hang_n,vx3dn_hang_count,vx3dn_hang_status;uint32_t i=inBuf[0]-0xE0;int k; /* v10.291: packet i of the last group that did not finish */
    outBuf[0]=inBuf[0];outBuf[1]=vx3dn_hang_count;outBuf[2]=vx3dn_hang_status;outBuf[3]=vx3dn_hang_n;for(k=0;k<4;k++)outBuf[4+k]=i<vx3dn_hang_n?vx3dn_hang[i][k]:0;
    if(i<vx3dn_hang_n){outBuf[3]|=(vx3dn_hang[i][4]&0xFFFFUL)<<16;outBuf[7]=(vx3dn_hang[i][3]&0xFFFFUL)|(vx3dn_hang[i][5]<<16);}}
   if(inBuf[0]==0xC7){extern void VX3DN_cmd_probe(uint32_t *o);VX3DN_cmd_probe((uint32_t*)outBuf);outBuf[0]=0xC7;} /* v10.283: the command block's memory type and write speed */
   if(inBuf[0]==0xC0||inBuf[0]==0xC1){extern uint32_t vx3dn_batch_stat[4];int k;outBuf[0]=0xC0;for(k=0;k<4;k++)outBuf[1+k]=vx3dn_batch_stat[k];outBuf[5]=0;if(inBuf[0]==0xC1)for(k=0;k<4;k++)vx3dn_batch_stat[k]=0;} /* v10.275: batch calls, packets, starts, the most packets in a start; C0 reads, C1 reads and clears */
   if(inBuf[0]==0xAC||inBuf[0]==0xAD){extern uint32_t vx3dn_sub_stat[4];int k;outBuf[0]=0xAC;for(k=0;k<4;k++)outBuf[1+k]=vx3dn_sub_stat[k];if(inBuf[0]==0xAD)for(k=0;k<4;k++)vx3dn_sub_stat[k]=0;} /* v10.232 */
   if(inBuf[0]>=0xA8&&inBuf[0]<=0xAB){extern volatile uint32_t vx3dn_state_cache;extern uint32_t vx3dn_cache_stat[4];int k;outBuf[0]=0xA8;outBuf[1]=vx3dn_state_cache;for(k=0;k<4;k++)outBuf[2+k]=vx3dn_cache_stat[k];if(inBuf[0]==0xA9)for(k=0;k<4;k++)vx3dn_cache_stat[k]=0;} /* v10.232 */
   if(inBuf[0]==0xA5){extern void VX2D_loss_info(uint32_t *);outBuf[0]=0xA5;VX2D_loss_info((uint32_t*)outBuf+1);} /* v10.229: 2D state, native fault, LOST answers, loss generation, generation, failed recoveries */
   if(inBuf[0]==0x70){extern void VX2D_sync(void);VX2D_sync();} /* v10.281: a draw still in flight is retired first (a hung one is reset now), so the counts are the device's as of this call */
   if(inBuf[0]==0x70||inBuf[0]==0x71){extern uint32_t vx3dn_rec[5];int k;outBuf[0]=inBuf[0];for(k=0;k<5;k++)outBuf[1+k]=vx3dn_rec[k];outBuf[6]=0;} /* v10.210 (.530: the hang probe removed) */
   if(inBuf[0]>=0x50&&inBuf[0]<=0x54){int k;outBuf[0]=0x50;for(k=0;k<7;k++)outBuf[1+k]=vx3dn_set_last[k];}
   if(inBuf[0]>=0x30&&inBuf[0]<=0x34){outBuf[0]=0x30;outBuf[1]=vx3dn_dither_last[0];outBuf[2]=vx3dn_dither_last[1];outBuf[3]=vx3dn_dither_last[2];}
   *(DWORD*)params->lpcbBytesReturned=32;return 0;
  case VX_IOCTL_TAPE: /* v10.393: the ring tape (vx_ring.h) */
   {DWORD rc;if(!inBuf||params->cbInBuffer<4||!params->lpcbBytesReturned)return 87;
    rc=VX3DN_tape(inBuf[0],(const char*)inBuf+4,params->cbInBuffer-4,outBuf,params->cbOutBuffer);if(rc)return rc;
    *(DWORD*)params->lpcbBytesReturned=inBuf[0]==0?(params->cbOutBuffer<sizeof(vx_tape)?params->cbOutBuffer:sizeof(vx_tape)):0;return 0;}
  case VX_IOCTL_RING: /* v10.223: VXRING (vx_ring.h) */
   if(!inBuf||params->cbInBuffer!=sizeof(vx_ring_req)||!outBuf||params->cbOutBuffer<sizeof(vx_ring_reply)||!params->lpcbBytesReturned)return 87;
   VX3DN_ring((const vx_ring_req*)inBuf,(vx_ring_reply*)outBuf);*(DWORD*)params->lpcbBytesReturned=sizeof(vx_ring_reply);return 0;
  case VX_IOCTL_3D_PROBE:
  case VX_IOCTL_3D_TEST:
   vx3dn_forget(); /* v10.232: the 3D lab writes the engine's 3D registers */
   if(!outBuf || params->cbOutBuffer<sizeof(vx3d_report) ||
      !params->lpcbBytesReturned) return 87;
   if(params->dwIoControlCode==VX_IOCTL_3D_TEST &&
      (!inBuf || params->cbInBuffer!=sizeof(vx3d_request))) return 87;
   if(params->dwIoControlCode==VX_IOCTL_3D_TEST)
    rc=VX3D_test((const vx3d_request*)inBuf,(vx3d_report*)outBuf);
   else rc=VX3D_probe((vx3d_report*)outBuf);
   if(rc) return rc;
   *(DWORD*)params->lpcbBytesReturned=sizeof(vx3d_report);
   return 0;
  case VX_IOCTL_DAC:
   if(inBuf && params->cbInBuffer==sizeof(vx_dac_req) && outBuf && params->cbOutBuffer>=sizeof(vx_dac_req) && params->lpcbBytesReturned &&
      ((vx_dac_req*)inBuf)->abi==VX_DAC_ABI && ((vx_dac_req*)inBuf)->first==0xFFFFFFFDUL && !((vx_dac_req*)inBuf)->count) {
    /* v10.71: read request (VX2AUTO) */
    if(!VX_dac_read((vx_dac_req*)outBuf)) return 87;
    *(DWORD*)params->lpcbBytesReturned=sizeof(vx_dac_req);
    return 0;
   }
   if(!inBuf || params->cbInBuffer!=sizeof(vx_dac_req) || !outBuf || params->cbOutBuffer<4 || !params->lpcbBytesReturned) return 87;
   outBuf[0]=VX_dac((const vx_dac_req*)inBuf);
   *(DWORD*)params->lpcbBytesReturned=4;
   return 0;
  case VX_IOCTL_CACHE_SET:
   if(!inBuf || params->cbInBuffer!=4 || !outBuf || params->cbOutBuffer<4 || !params->lpcbBytesReturned) return 87;
   outBuf[0]=VX_cache_set(inBuf[0]);
   *(DWORD*)params->lpcbBytesReturned=4;
   return 0;
  case VX_IOCTL_CACHE:
   if(!outBuf || params->cbOutBuffer<sizeof(vx_cache_report) || !params->lpcbBytesReturned) return 87;
   VX_cache_report((vx_cache_report*)outBuf);
   *(DWORD*)params->lpcbBytesReturned=sizeof(vx_cache_report);
   return 0;
  case VX_IOCTL_PROBE: /* v10.333: VXPROBE */
   if(!inBuf || params->cbInBuffer<4 || !outBuf || params->cbOutBuffer<64*4 || !params->lpcbBytesReturned) return 87;
   VX_probe(inBuf, params->cbInBuffer>=8 ? 2 : 1, outBuf);
   *(DWORD*)params->lpcbBytesReturned=64*4;
   return 0;
  case VX_IOCTL_CRT:
   if(!outBuf || params->cbOutBuffer<sizeof(vx_crt_report) || !params->lpcbBytesReturned) return 87;
   VX_crt_dump(outBuf);
   outBuf[0]=VX_MAGIC;
   *(DWORD*)params->lpcbBytesReturned=sizeof(vx_crt_report);
   return 0;
  case VX_IOCTL_EDID:
   if(!outBuf || params->cbOutBuffer<sizeof(vx_edid_report) || !params->lpcbBytesReturned) return 87;
   VX_edid_report(outBuf, (params->lpInBuffer && params->cbInBuffer>=4 && (inBuf[0]&1)) ? 1 : 0);
   *(DWORD*)params->lpcbBytesReturned=sizeof(vx_edid_report);
   return 0;
  case VX_IOCTL_2D_ACCESS:
   if(!inBuf || (params->cbInBuffer!=4&&params->cbInBuffer!=12) || !outBuf || params->cbOutBuffer<4 || !params->lpcbBytesReturned) return 87;
   outBuf[0]=VX2D_access(inBuf[0]);
   /* v10.295: a Lock or a destroy of a depth surface names its memory: its Coarse Z records are no longer trusted */
   if(params->cbInBuffer==12&&inBuf[2]>inBuf[1]){vx_lock();vx3dn_forget();vx_unlock();}
   *(DWORD*)params->lpcbBytesReturned=4;
   return 0;
  case VX_IOCTL_2D_STATS:
   if(!outBuf || params->cbOutBuffer<sizeof(vx2d_stats) || !params->lpcbBytesReturned) return 87;
   VX2D_stats((vx2d_stats*)outBuf);
   *(DWORD*)params->lpcbBytesReturned=sizeof(vx2d_stats);
   return 0;
  case VX_IOCTL_2D_SUBMIT:
   if(!inBuf || params->cbInBuffer!=sizeof(vx2d_request) || !outBuf ||
      params->cbOutBuffer<4 || !params->lpcbBytesReturned) return 87;
   outBuf[0]=VX2D_submit((const vx2d_request*)inBuf);
   *(DWORD*)params->lpcbBytesReturned=4;
   return 0;
  case VX_IOCTL_OVL: /* v10.106 */
   if(!inBuf || params->cbInBuffer!=sizeof(vx_ovl_req) || !outBuf ||
      params->cbOutBuffer<4*VXO_REPLY_DWORDS || !params->lpcbBytesReturned) return 87;
   VX2D_ovl((const vx_ovl_req*)inBuf,(uint32_t*)outBuf);
   *(DWORD*)params->lpcbBytesReturned=4*VXO_REPLY_DWORDS;
   return 0;
  case VX_IOCTL_FLIP:
   if(!inBuf || params->cbInBuffer!=sizeof(vxf_request) || !outBuf ||
      params->cbOutBuffer<4*VXF_REPLY_DWORDS || !params->lpcbBytesReturned) return 87;
   VX_flip_control((const vxf_request*)inBuf,(uint32_t*)outBuf);
   *(DWORD*)params->lpcbBytesReturned=4*VXF_REPLY_DWORDS;
   return 0;
  case VX_IOCTL_2D_CTRL:
   if(!inBuf || params->cbInBuffer<4 || !outBuf || params->cbOutBuffer<16 || !params->lpcbBytesReturned) return 87;
   VX2D_control(inBuf[0],(uint32_t*)outBuf);
   *(DWORD*)params->lpcbBytesReturned=16;
   return 0;
  case VX_IOCTL_STATUS:
   if(!outBuf || params->cbOutBuffer<sizeof(vx_status_t) ||
      !params->lpcbBytesReturned) return 87;
   VX_snapshot((vx_status_t*)outBuf);
   *(DWORD*)params->lpcbBytesReturned=sizeof(vx_status_t);
   return 0;
  case VX_IOCTL_LOG:
   if(!outBuf || !params->cbOutBuffer || !params->lpcbBytesReturned) return 87;
   *(DWORD*)params->lpcbBytesReturned=VX_log_copy(outBuf,params->cbOutBuffer);
   return 0;
		/* DX */
		case DIOC_OPEN:
			dbg_printf("DIOC_OPEN\n");
			io_open_cnt++;
			rc = 0;
			break;
		case DIOC_CLOSEHANDLE:
			if(io_open_cnt) io_open_cnt--;
			VX_lock_close(params->hDevice); /* v10.432 */
			{extern void VX_smp_close(DWORD);VX_smp_close(params->hDevice);} /* v10.434 */
			VX_cache_restore(); /* v10.69: a VX2READ cache-type test never outlives its handle */
			rc = 0;
			break;
		/* .578: the FBHDA operations of the display driver's interface (OP_FBHDA_...) were cases here too: the list above lets none of them in (error 50) */
		default:
			dbg_printf("DeviceIOControl: Unknown: %d\n", params->dwIoControlCode);
			break;
	}
	
	//dbg_printf("IL\n");
	//End_Critical_Section();

	return rc;
}

void Device_Init_Complete(DWORD VM)
{
}

/* shutdown procedure */
void Device_Exit_proc(DWORD VM)
{
 async_blit_stop();
 VX2D_sync(); /* v10.144: a kept engine context is handed back before Windows leaves */
 VX2D_hwcur_hide(); /* v10.105: no hardware cursor over the text mode Windows leaves behind */
 VX2D_ovl_hide(); /* v10.106 */

	dbg_printf(dbg_destroy);
	
	// TODO: free memory
}

static DWORD __stdcall service_get_ver()
{
	return API_3DACCEL_VER;
}

static FBHDA_t *__stdcall service_setup()
{
	return FBHDA_setup();
}

static DWORD __stdcall service_overlay_setup(DWORD overlay, DWORD width, DWORD height, DWORD bpp)
{
	return FBHDA_overlay_setup(overlay, width, height, bpp);
}

static void __stdcall service_overlay_lock(DWORD left, DWORD top, DWORD right, DWORD bottom)
{
	FBHDA_overlay_lock(left, top, right, bottom);
}

static void __stdcall service_overlay_unlock(DWORD flags)
{
	FBHDA_overlay_unlock(flags);
}

DWORD service_table[SERVICE_TABLE_CNT] = {
	(DWORD)&service_get_ver, // 0
	(DWORD)&service_setup, // 1
	0, // 2
	0, // 3
	0, // 4
	0, // 5
	0, // 6
	0, // 7
	0, // 8
	(DWORD)&service_overlay_setup, // 9
	(DWORD)&service_overlay_lock, // 10
	(DWORD)&service_overlay_unlock, // 11
};

void __stdcall win32_destroy_process_proc(DWORD pid)
{
	dbg_printf("DESTROY_PROCESS: %lX\n", pid);
	
}
