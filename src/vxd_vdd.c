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



#include "winhack.h"
#include "vmm.h"

#include "vxd_vdd.h"
#include "3d_accel.h"

#include "vxd_lib.h"


#include "vxd.h"

#include "3d_accel.h"

/* code and data is same segment */
#include "code32.h"

#include "vxd_strings.h"

extern FBHDA_t *hda;
extern DWORD ThisVM;

extern DWORD is_qemu;

static DWORD hda_pm16 = 0;
static DWORD mouse_pm16 = 0;


extern WORD vesa_version;
#include "vx85598.h"

BOOL mode_changing = FALSE;
BOOL virtual_int10_mode_set = FALSE;

static void VDD_Register_Extra_Screen_Selector(DWORD selector)
{
	static DWORD sSelector;
	sSelector = selector;
	
	_asm mov eax, [sSelector]
	_asm push eax
	VxDCall(VDD, Register_Extra_Screen_Selector);
	_asm pop eax
}

DWORD map_pm16(DWORD vm, DWORD linear, DWORD size)
{
	DWORD hi  = 0;
	DWORD low = 0;
	DWORD selector=0;
	if(!linear || !size || size>0xFFFFF000UL || linear>0xFFFFFFFFUL-size) return 0;
	
	dbg_printf(dbg_map_pm16, vm, linear, size);
	
	/* With page granularity the inclusive limit is pages-1. */
	_BuildDescriptorDWORDs(linear, RoundToPages(size)-1, 0x92, 0x80, 0, &hi, &low);
	
	dbg_printf(dbg_map_pm16_qw, hi, low);
	
	//_Allocate_LDT_Selector(vm, hi, low, 1, 0, &selector, NULL);
	_Allocate_GDT_Selector(hi, low, 0, &selector, NULL);
	if(!selector || selector>0xFFFFUL) return 0;
	VDD_Register_Extra_Screen_Selector(selector);
	
	dbg_printf(dbg_map_pm16_sel, selector);
	
	return selector << 16;
}

void update_pm16(DWORD vm, DWORD oldmap, DWORD linear, DWORD size)
{
	WORD selector = oldmap >> 16;
	DWORD hi  = 0;
	DWORD low = 0;
	
	dbg_printf("update_pm16: %ld\n", linear);
	
	if(!selector || !linear || !size || size>0xFFFFF000UL || linear>0xFFFFFFFFUL-size) return;
	_BuildDescriptorDWORDs(linear, RoundToPages(size)-1, 0x92, 0x80, 0, &hi, &low);
	_SetDescriptor(selector, vm, hi, low, 0);
}

/*
You can implement all the VESA support entirely in your mini-VDD. Doing so will
cause VESA applications to run more efficiently since all of the VESA support is
done at Ring 0. You can expect a 20% speed increase by implementing VESA 
unctionality in your mini-VDD over using a real mode VESA driver. 
Following are the VESA functions you can implement in your mini-VDD: 
	CHECK_HIRES_MODE
	CHECK_SCREEN_SWITCH_OK
	GET_BANK_SIZE 
	GET_CURRENT_BANK_READ
	GET_CURRENT_BANK_WRITE
	GET_TOTAL_VRAM_SIZE
	POST_HIRES_SAVE_RESTORE
	PRE_HIRES_SAVE_RESTORE
	SET_BANK
	SET_HIRES_MODE
	VESA_CALL_POST_PROCESSING
	VESA_SUPPORT
*/


/**
REGISTER_DISPLAY_DRIVER (Function 0) 
Call With
EBX: Contains the Windows VM handle. 
EBP: Contains the Windows VM's Client Registers. 
All other registers are via agreement between the display driver and the mini-VDD. 
Return Values
Whatever is agreed upon by the display driver and mini-VDD. 
Remarks
This function is called in response to a display driver call to the Main VDD function VDD_REGISTER_DISPLAY_DRIVER_INFO. 
See also VDD_REGISTER_DISPLAY_DRIVER_INFO. 

**/
VDDPROC(REGISTER_DISPLAY_DRIVER, register_display_driver)
{
	dbg_printf("register: ebx = %ld, ecx = %ld, VM = %lX\n", state->Client_EBX, state->Client_ECX, ThisVM);
	state->Client_EAX=0;
	state->Client_EDX=0;
	state->Client_ECX=0;
	state->Client_ESI=0;
	if(!hda || !hda->vram_pm32 || !hda->vram_size_virt) { VDD_NC; return; }
	
	if(hda->vram_pm16 == 0)
	{
		hda->vram_pm16 = map_pm16(state->Client_EBX, (DWORD)hda->vram_pm32, hda->vram_size_virt);
	}
	
	if(hda_pm16 == 0)
	{
		hda_pm16 = map_pm16(state->Client_EBX, (DWORD)hda, sizeof(FBHDA_t));
	}
	if(!hda->vram_pm16 || !hda_pm16) {
  vx_status.failure=VX_NO_SELECTOR;
  VDD_NC;
  return;
 }
	
	if(mouse_pm16 == 0)
	{
		mouse_pm16 = map_pm16(state->Client_EBX, (DWORD)mouse_buffer(), MOUSE_BUFFER_SIZE);
	}
	/* .737 (review audit #48, checked and not taken): a 0 mouse selector is a valid answer: the display driver asks for the buffer
	 * itself and turns the VxD cursor off when there is none (init.c), so failing the registration here would lose the whole driver */
	
	/* RETURN:
		EAX -> VXD_VM (thisVM)
		EDX -> fbhda
		ECX -> mouse_buffer
		ESI -> fbhda linear
	*/
	state->Client_EAX = ThisVM;
	state->Client_EDX = hda_pm16;
	state->Client_ECX = mouse_pm16;
	state->Client_ESI = (DWORD)hda;
	
	VDD_CY;
}

/**
GET_CHIP_ID (Function 42) 
Call With
EBX: Contains the VM handle (always the Windows VM). 
EBP: Points to the Windows VM's Client Registers. 
Return Values
Save everything that you use. EAX contains the ChipID. 
Remarks
Several mini-VDD's support multiple chipsets that utilize different display drivers.
For example, ATI's mini-VDD supports the VGA Wonder which uses SUPERVGA.DRV, the Mach8
which uses ATIM8.DRV, the Mach32 which uses ATIM32.DRV, and the Mach64 which uses ATIM64.DRV.
Therefore, the detection for each of these chipsets is included in the ATI mini-VDD.
GET_CHIP_ID is the only way the Main VDD Plug & Play support code can differentiate between
display cards that use the same mini-VDD. 

The Main VDD calls the function MiniVDD_Dynamic_Init when the mini-VDD is loaded.
If MiniVDD_Dynamic_Init returns with the carry flag clear (indicating success),
the Main VDD calls GET_CHIP_ID as a second check to make sure that the user has
not changed the video card since the last time Windows was run. The Main VDD compares
the value returned by GET_CHIP_ID with the value stored in the registry and if they
are different, it reports the error to the Plug & Play subsystem. 

If the mini-VDD fails to detect one of the cards it supports, MiniVDD_Dynamic_Init
returns with the carry flag set (indicating failure) and the Windows 95's Plug & Play
code loads the standard VGA driver. 

If the value returned by GET_CHIP_ID is different than the value stored in the registry,
or if MiniVDD_Dynamic_Init returns failure, the system displays an error message to
the user concerning the problem with the display settings and allows the user to run
hardware detection to re-detect the video card. 

**/
VDDPROC(GET_CHIP_ID, get_chip_id)
{


	if(VESA_valid())
	{
		state->Client_EAX = vesa_version;
		VDD_CY;
	}
	else
	{
		state->Client_EAX = 0;
		VDD_NC;
	}
}

/**
CHECK_SCREEN_SWITCH_OK (Function 43) 
Call With
EAX: Contains -1 if running in a known VESA mode. 
EBX: Contains the VM handle (always the Windows VM). 
ECX: Contains the video mode number (if known). 
EBP: Points to the Windows VM's Client Registers. 
Return Values
CY indicates that the hi-res application may not be switched away from. NC indicates
that it is safe to switch away from the hi-res application.
Remarks
The Main VDD calls this routine whenever a user presses ALT-ENTER or ALT-TAB to
switch away from a full-screen MS-DOS prompt. The mini-VDD should determine if
it knows how to restore this mode. If it is a VESA mode or standard VGA mode,
the Main VDD knows how to restore it and unless the mini-VDD has special
considerations, it should return NC. Otherwise, it should return CY causing the
system to beep to alert the user that the hi-res VM cannot be switched away from.
This notification is not given if a user presses CTRL-ALT-DEL to terminate a full-screen
application. In this case, no save of the screen is attempted and it will be impossible
to restore the screen if the user tries to switch back. In this case, if the user tries
to switch back, the system terminates the application.

**/
BOOL VESA_check_switch(DWORD mode);

VDDPROCRET(CHECK_SCREEN_SWITCH_OK, check_screen_switch_ok)
{
	if(VESA_check_switch(extra_ecx))
	{
		dbg_printf("CHECK_SCREEN_SWITCH_OK: OK\n");
		return 1; /* NC */
	}

	dbg_printf("CHECK_SCREEN_SWITCH_OK: DENIED\n");
	return 0; /* CY */
}

VDDPROC(GET_VDD_BANK, get_vdd_bank)
{
	state->Client_EDX = state->Client_ECX;
}

VDDPROC(SET_VDD_BANK, set_vdd_bank)
{

}

VDDPROC(RESET_BANK, reset_bank)
{

}

/**
GET_TOTAL_VRAM_SIZE (Function 36) 
Call With
EBX: Contains the VM handle (always the currently executing VM). 
EBP: Contains the Windows VM's Client Registers. 
Return Values
Save everything that you use. CY returned means that mini-VDD handled the call.
ECX contains the total size of VRAM on the card. 
Remarks
Whenever the VDD saves a hi-res mode, it saves all of the card's video memory to
the swap file. This is because VESA applications have full access to the total memory
on the card, even if their visible screen size is less than the total VRAM size on the card.
Therefore, the Main VDD must know the total VRAM size. If the mini-VDD does not
handle this call, the Main VDD will do a time-consuming call to
VESA BIOS function 4F00h to obtain this information.
For performance reasons, you should implement this function. 

**/
VDDPROC(GET_TOTAL_VRAM_SIZE, get_total_vram_size)
{
	state->Client_ECX = hda->vram_size_virt;
	VDD_CY;
}

VDDPROC(PRE_HIRES_TO_VGA, pre_hires_to_vga)
{
	dbg_printf("PRE_HIRES_TO_VGA\n");
	mode_changing = TRUE;
	if(is_qemu)
	{
		Disable_Global_Trapping(0x1CE);
		Disable_Global_Trapping(0x1CF);
	}
	VDD_NC;
}

VDDPROC(POST_HIRES_TO_VGA, post_hires_to_vga)
{
	dbg_printf("POST_HIRES_TO_VGA\n");
	mode_changing = FALSE;
	{extern void VESA_dos_restore(void); VESA_dos_restore();} /* .673: as RESTORE_REGISTERS (whichever the VDD calls after its own restore; it writes only a wrong clock select) */
	if(is_qemu)
	{
		Enable_Global_Trapping(0x1CE);
		Enable_Global_Trapping(0x1CF);
	}
	VDD_NC;
}

VDDPROC(PRE_VGA_TO_HIRES, pre_vga_to_hires)
{
	dbg_printf("PRE_VGA_TO_HIRES\n");
	mode_changing = TRUE;
	VDD_NC;
}

VDDPROC(POST_VGA_TO_HIRES, post_vga_to_hires)
{
	dbg_printf("POST_VGA_TO_HIRES\n");
	mode_changing = FALSE;
	VDD_NC;
}

/**
Entry:
	EAX contains the mode that we're setting to (in AL).
	EBX contains the VM handle doing the mode set.
	EBP --> VM's Client Registers.
Exit:
	Preserve everything that's used.

This routine is called whenever a VM (including the Windows VM) does an
INT 10H, function 0 mode set call.  The MiniVDD uses this routine to
do anything necessary to implement the mode set.  In the case of the XGA,
where the ROM BIOS itself doesn't take the screen out of HiRes mode (if it's
currently in an XGA HiRes mode), we take care of restoring the screen to
VGA mode.  This routine is called regardless of whether the VM is windowed
or full-screen.  We therefore must be careful not to take the screen out
of XGA HiRes mode if this is a windowed VM!

Do nothing for the Windows VM:

**/

VDDPROC(SAVE_REGISTERS, save_registers)
{
	//dbg_printf("SAVE_REGISTERS\n");
	// NOP
}

VDDPROC(RESTORE_REGISTERS, restore_registers)
{
	//dbg_printf("RESTORE_REGISTERS\n");
	extern void VESA_dos_restore(void);
	if(vm != ThisVM) VESA_dos_restore(); /* .673: a DOS session given the display: the BIOS's clock select (vxd_vesa.c) */
}

VDDPROC(VIRTUALIZE_CRTC_IN, virtualize_crtc_in)
{
	//dbg_printf("VIRTUALIZE_CRTC_IN: port: %X, value: %X\n", extra_edx, extra_eax);
	// NOP
}

VDDPROC(VIRTUALIZE_CRTC_OUT, virtualize_crtc_out)
{
	//dbg_printf("VIRTUALIZE_CRTC_OUT: port: %X, value: %X\n", extra_edx, extra_eax);
}
