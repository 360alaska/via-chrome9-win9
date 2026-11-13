#include <string.h>
#include "winhack.h"

#include "drvlib.h"
#include "dpmi.h"

#include <gdidefs.h>
#include <dibeng.h>
#include <valmode.h>
#include <minivdd.h>
#include "minidrv.h"

#pragma code_seg( _INIT )

void __far *drv_malloc(DWORD dwSize, DWORD __far *lpLinear)
{
	WORD    wSel;
	DWORD   dwLin;

	wSel = DPMI_AllocLDTDesc(1);
	if(!wSel)
		return NULL;

	dwLin = DPMI_AllocMemBlk(dwSize);
    
	if(dwLin)
	{
		DPMI_SetSegBase(wSel, dwLin);
		DPMI_SetSegLimit(wSel, dwSize - 1);
    
    *lpLinear = dwLin;
    
		return wSel :> 0;
	}

	DPMI_FreeLDTDesc(wSel); /* v10.139 (review F17): no selector leak when the memory block cannot be had */
	return NULL;
}

BOOL drv_is_p2()
{
	/* CPUID detection from:
		https://www.prowaretech.com/articles/current/assembly/x86/check-for-cpuid-support
	*/
	static DWORD rc = 0;
	_asm {
		push eax
		push ebx
		pushfd                    // push eflags on the stack
		pop eax                   // pop them into eax
		mov ebx, eax              // save to ebx for restoring afterwards
		xor eax, 0x200000         // toggle bit 21
		push eax                  // push the toggled eflags
		popfd                     // pop them back into eflags
		pushfd                    // push eflags
		pop eax                   // pop them back into eax
		push ebx                  // v10.330 (review #78): the original eflags back (the ID bit stayed toggled)
		popfd
		cmp eax, ebx              // see if bit 21 was reset
		jz non_support_cpuid
		push edx
		push ecx
		mov eax, 1
		db 0x0F                   // cpuid
		db 0xA2                   // (manual insert)
    and  edx, 0x1800000       // MMX + FXSR support
    cmp  edx, 0x1800000
    jnz  non_support_mmx
    pop  ecx
    pop  edx
    pop  ebx
    mov  eax, 1
    mov  [rc], eax
    pop  eax
    jmp done
    non_support_mmx:
    	pop ecx
    	pop edx
		non_support_cpuid:
			pop ebx
			pop eax
		done:
	};

	return (rc == 0) ? FALSE : TRUE;
}
