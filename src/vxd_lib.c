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

/* 32 bit RING-0 system calls and CRT function */

#include "winhack.h"
#include "vmm.h"
#include "vxd.h"
#include "vpicd.h"


#include "vxd_vdd.h"

#include "vxd_lib.h"

#include "code32.h"

#include "vxd_strings.h"

/* some CRT */
/* v10.309 (review): the dword runs as rep stosd / rep movsd (the mode set's clear, the software cursor's saves) */
void vxl_stosd(void *d, unsigned int v, unsigned int n);
#pragma aux vxl_stosd = "cld" "rep stosd" parm [edi] [eax] [ecx] modify exact [edi ecx];
void vxl_movsd(void *d, const void *s, unsigned int n);
#pragma aux vxl_movsd = "cld" "rep movsd" parm [edi] [esi] [ecx] modify exact [edi esi ecx];
void memset(void *dst, int c, unsigned int size)
{
	unsigned int par;
	unsigned int i;
	unsigned int dw_count;
	unsigned int dw_size;
	
	c &= 0xFF;
	par = (unsigned int)c * 0x01010101U; /* v10.330 (review #143): no signed shift into bit 31 (c << 24 for c >= 128) */
	dw_count = size >> 2;
	dw_size = size & 0xFFFFFFFCUL;
	
	if(dw_count) vxl_stosd(dst, par, dw_count);
	
	for(i = dw_size; i < size; i++)
	{
		((unsigned char*)dst)[i] = c; 
	}
}

void *memcpy(void *dst, const void *src, unsigned int size)
{
	unsigned int dw_count;
	unsigned int dw_size;
	unsigned int i;
	
	dw_count = size >> 2;
	dw_size = size & 0xFFFFFFFCUL;
	
	if(dw_count) vxl_movsd(dst, src, dw_count);
	
	for(i = dw_size; i < size; i++)
	{
		((unsigned char*)dst)[i] = ((unsigned char*)src)[i]; 
	}
	
	return dst;
}

int memcmp(const void *ptr1, const void *ptr2, unsigned int num)
{
	const unsigned char *p1 = (const unsigned char *)ptr1;
	const unsigned char *p2 = (const unsigned char *)ptr2;

	if(num == 0) return 0;

	while(num > 1 && *p1 == *p2)
	{
		p1++;
		p2++;
		num--;
	}

	return *p1 - *p2;
}

void *memmove(void *destination, const void *source, unsigned int num)
{
	unsigned char *dst = (unsigned char *)destination;
	const unsigned char *src = (const unsigned char *)source;
	
	if(src > dst)
	{
		while(num--)
		{
			*dst++ = *src++;
		}
	}
	else if(src < dst)
	{
		dst += num;
		src += num;
		while(num--)
		{
			*--dst = *--src;
		}
	}
	return destination;
}

unsigned int strlen(const char *s)
{
	const char *ptr = s;
	while(*ptr != '\0') ptr++;
	return ptr - s;
}

char *strcpy(char *dst, const char *src)
{	
	char *pdst = dst;
	
	while(*src != '\0')
	{
		*pdst = *src;
		src++;
		pdst++;
	}
	
	*pdst = '\0';
	
	return dst;
}

char *strcat(char *dst, const char *src)
{	
	unsigned int i = strlen(dst);
	strcpy(dst+i, src);
	return dst;
}

/* temp for reading registry */
static union
{
	DWORD dw;
	char str[64];
} RegReadConfBuf = {0};

/**
 * Read integer value from registry
 *
 **/ 
BOOL RegReadConf(UINT root, const char *path, const char *name, DWORD *out)
{
	/* v10.319 (audit V2): the length and type the registry returns are honoured: a REG_DWORD of exactly 4 bytes; a REG_SZ read as decimal
	 * digits after leading spaces, within the returned length, at least one digit, no overflow (a 4-byte REG_BINARY as a DWORD, as before);
	 * anything else (a value of another length or type,
	 * an empty or non-numeric string) leaves *out alone and returns FALSE, so the caller keeps its default (a short binary value took the
	 * previous value's bytes, a string of 64 digits or spaces was read past the buffer) */
	DWORD hKey;
	DWORD type;
	DWORD cbData = sizeof(RegReadConfBuf.str);
	BOOL rv = FALSE;
	
	if(!out) return FALSE;
	if(_RegOpenKey(root, (char*)path, &hKey) == ERROR_SUCCESS)
	{
		if(_RegQueryValueEx(hKey, (char*)name, 0, &type, (BYTE*)RegReadConfBuf.str, &cbData) == ERROR_SUCCESS && cbData <= sizeof(RegReadConfBuf.str))
		{
			if(type == REG_SZ)
			{
				DWORD cdw = 0, pos = 0, start, digit;
				while(pos < cbData && RegReadConfBuf.str[pos] == ' ') pos++;
				start = pos;
				while(pos < cbData && RegReadConfBuf.str[pos] >= '0' && RegReadConfBuf.str[pos] <= '9')
				{
					digit = (DWORD)(RegReadConfBuf.str[pos] - '0');
					if(cdw > (0xFFFFFFFFUL - digit) / 10UL) break;
					cdw = cdw * 10UL + digit;
					pos++;
				}
				/* v10.330 (review #59): only spaces may follow the digits (then the end of the value or its NUL): "1x" or an overflowing
				 * number is refused (the caller keeps its default) instead of read as its leading digits */
				{DWORD end = pos; while(end < cbData && RegReadConfBuf.str[end] == ' ') end++;
				if(pos > start && (end >= cbData || RegReadConfBuf.str[end] == 0))
				{
					*out = cdw;
					rv = TRUE;
				}}
			}
			else if((type == 4 /* REG_DWORD */ || type == REG_BINARY) && cbData == sizeof(DWORD)) /* a 4-byte binary value as before (.reg hex:) */
			{
				*out = RegReadConfBuf.dw;
				rv = TRUE;
			}
		}
		
		_RegCloseKey(hKey);
	}
	
	return rv;
}

/**
 * VMM calls wrapers
 **/
DWORD Get_VMM_Version()
{
	WORD ver = 0;
	
	_asm push ecx
	_asm push eax
	_asm xor eax, eax
	VMMCall(Get_VMM_Version);
	_asm mov [ver],ax
	_asm pop eax /* v10.330 (review #58): popped in the reverse order of the pushes (EAX and ECX were swapped) */
	_asm pop ecx
	
	return ver;
}

void *Get_Cur_VM_Handle()
{
	void *handle = 0;

	_asm push ebx
	VMMCall(Get_Cur_VM_Handle);
	_asm mov [handle],ebx
	_asm pop ebx

	return handle;
}

/* v10.322: the calling thread's handle (ring-0 thread control block; VMM Get_Cur_Thread_Handle returns it in EDI) */
void *Get_Cur_Thread_Handle()
{
	void *handle = 0;

	_asm push edi
	VMMCall(Get_Cur_Thread_Handle);
	_asm mov [handle],edi
	_asm pop edi

	return handle;
}

/* v10.389: the milliseconds a thread has run (VMM _GetThreadExecTime, C convention: the thread handle on the stack) */
unsigned long Get_Thread_Exec_Time(void *thread)
{
	unsigned long ms = 0;
	_asm push [thread]
	VMMCall(_GetThreadExecTime);
	_asm add esp,4
	_asm mov [ms],eax
	return ms;
}

void *Get_Sys_VM_Handle()
{
 void *handle=0;
 _asm push ebx
 VMMCall(Get_Sys_VM_Handle);
 _asm mov [handle],ebx
 _asm pop ebx
 return handle;
}

DWORD VMM_GetSystemInitState()
{
 DWORD state=0;
 VMMCall(VMM_GetSystemInitState);
 _asm mov [state],eax
 return state;
}

volatile void __cdecl Begin_Critical_Section(ULONG Flags)
{
	_asm push ecx
	_asm mov ecx, [Flags]
	VMMCall(Begin_Critical_Section);
	_asm pop ecx
}

volatile void __cdecl End_Critical_Section()
{
	VMMCall(End_Critical_Section);
}

ULONG __cdecl Create_Semaphore(ULONG TokenCount)
{
	ULONG SemHandle = 0;

	_asm push eax
	_asm push ecx
	_asm mov ecx, [TokenCount]
	VMMCall(Create_Semaphore);
	_asm {
		jc create_sem_error
		mov [SemHandle], eax
		create_sem_error:
		pop ecx
		pop eax
	}
	
	return SemHandle;
}

void __cdecl Destroy_Semaphore(ULONG SemHandle)
{
	_asm push eax
	_asm mov eax, [SemHandle]
	VMMCall(Destroy_Semaphore);
	_asm pop eax
}

void __cdecl Wait_Semaphore(ULONG semHandle, ULONG flags)
{
	_asm push eax
	_asm push ecx
	_asm mov eax, [semHandle]
	_asm mov ecx, [flags]
	VMMCall(Wait_Semaphore);
	_asm pop ecx
	_asm pop eax
}

void __cdecl Signal_Semaphore(ULONG SemHandle)
{
	_asm push eax
	_asm mov eax, [SemHandle]
	VMMCall(Signal_Semaphore);
	_asm pop eax
}

void __cdecl Resume_VM(ULONG VM)
{
	_asm push ebx
	_asm mov ebx, [VM]
	VMMCall(Resume_VM);
	_asm pop ebx	
}

void Release_Time_Slice()
{
	VMMCall(Release_Time_Slice);
}

void __cdecl *Map_Flat(BYTE SegOffset, BYTE OffOffset)
{
	void *result = NULL;
	
	_asm
	{
		push eax
		xor eax, eax
		mov al, [OffOffset]
		mov ah, [SegOffset]
	}
	VMMCall(Map_Flat);
	_asm
	{
		mov [result],eax
		pop eax
	}
	
	return result;
}

void __cdecl Install_IO_Handler(DWORD port, DWORD callback)
{
/*
 * esi <- IOCallback
 * edx <- I/O port numbers
 */
	_asm
	{
		push esi
		push edx
		mov esi, [callback]
		mov edx, [port]
	}
	VMMCall(Install_IO_Handler);
	_asm
	{
		pop edx
		pop esi
	}
}

/* .525: the VMM's paging counters (_GetDemandPageInfo fills a DemandInfoStruc: [6] DIPage_Faults, [7] DIPage_Ins, [8] DIPage_Outs,
 * [9] DIPage_Discards); d holds at least 16 dwords (the caller gives 32) */
void __cdecl vx_demand_info(DWORD *d)
{
	DWORD *p = d;
	_asm
	{
		push edx
		push ecx
		push 0 ; flags - must be zero
		push [p]
	}
	VMMCall(_GetDemandPageInfo);
	_asm
	{
		add esp, 8
		pop ecx
		pop edx
	}
}

DWORD __cdecl _GetFreePageCount(DWORD *pLockablePages)
{
	DWORD LockablePages = 0;
	DWORD FreePages = 0;
	_asm
	{
		push edx
		push ecx
		push 0 ; flags - must be zero
	}
	VMMCall(_GetFreePageCount);
	_asm
	{
		mov [LockablePages], edx
		mov [FreePages], eax
		pop eax
		pop ecx
		pop edx
	}
	
	if(LockablePages != NULL)
	{
		*pLockablePages = LockablePages;
	}
	
	return FreePages;
}

static void __declspec(naked) __cdecl _BuildDescriptorDWORDs_(ULONG DESCBase, ULONG DESCLimit, ULONG DESCType, ULONG DESCSize, ULONG flags)
{
	VMMJmp(_BuildDescriptorDWORDs);
}

void __cdecl _BuildDescriptorDWORDs(ULONG DESCBase, ULONG DESCLimit, ULONG DESCType, ULONG DESCSize, ULONG flags, DWORD *outDescHigh, DWORD *outDescLow)
{
	DWORD High = 0;
	DWORD Low = 0;
	
	_BuildDescriptorDWORDs_(DESCBase, DESCLimit, DESCType, DESCSize, flags);
	_asm
	{
		mov [High], edx
		mov [Low],  eax
	}
	
	*outDescHigh = High;
	*outDescLow  = Low;
}

static void __declspec(naked) __cdecl _Allocate_LDT_Selector_(ULONG vm, ULONG DescHigh, ULONG DescLow, ULONG Count, ULONG flags)
{
	VMMJmp(_Allocate_LDT_Selector);
}

void __cdecl _Allocate_LDT_Selector(ULONG vm, ULONG DescHigh, ULONG DescLow, ULONG Count, ULONG flags, DWORD *outFirstSelector, DWORD *outSelectorTable)
{
	DWORD firstSelector = 0;
	DWORD selectorTable = 0;
	
	_Allocate_LDT_Selector_(vm, DescHigh, DescLow, Count, flags);
	_asm
	{
		mov [firstSelector], eax
		mov [selectorTable], edx
	}
	
	if(outFirstSelector != NULL)
	{
		*outFirstSelector = firstSelector;
	}
	
	if(outSelectorTable != NULL)
	{
		*outSelectorTable = selectorTable;
	}
}

static void __declspec(naked) __cdecl _Allocate_GDT_Selector_(ULONG DescHigh, ULONG DescLow, ULONG flags)
{
	VMMJmp(_Allocate_GDT_Selector);
}

void __cdecl _Allocate_GDT_Selector(ULONG DescHigh, ULONG DescLow, ULONG flags, DWORD *outFirstSelector, DWORD *outSelectorTable)
{
	DWORD firstSelector = 0;
	DWORD selectorTable = 0;
	
	_Allocate_GDT_Selector_(DescHigh, DescLow, flags);
	_asm
	{
		mov [firstSelector], eax
		mov [selectorTable], edx
	}
	
	if(outFirstSelector != NULL)
	{
		*outFirstSelector = firstSelector;
	}
	
	if(outSelectorTable != NULL)
	{
		*outSelectorTable = selectorTable;
	}
}

ULONG __declspec(naked) __cdecl _SetDescriptor(ULONG selector, ULONG vm, DWORD DescDWORD1, DWORD DescDWORD2, ULONG flags)
{
	VMMJmp(_SetDescriptor);
}

static ULONG __declspec(naked) __cdecl _PageAllocate_call(ULONG nPages, ULONG pType, ULONG VM, ULONG AlignMask, ULONG minPhys, ULONG maxPhys, ULONG *PhysAddr, ULONG flags)
{
	VMMJmp(_PageAllocate);
}

ULONG __cdecl _PageAllocate(ULONG nPages, ULONG pType, ULONG VM, ULONG AlignMask, ULONG minPhys, ULONG maxPhys, ULONG *PhysAddr, ULONG flags)
{
	ULONG r = _PageAllocate_call(nPages, pType, VM, AlignMask, minPhys, maxPhys, PhysAddr, flags);
	
	if(r == 0)
	{
		dbg_printf("_PageAllocate FAIL - pages: %ld\n", nPages);
	}
	
	return r;
}

ULONG __declspec(naked) __cdecl _PageFree(PVOID hMem, DWORD flags)
{
	VMMJmp(_PageFree);
}

ULONG __declspec(naked) __cdecl _CopyPageTable(ULONG LinPgNum, ULONG nPages, DWORD *PageBuf, ULONG flags)
{
	VMMJmp(_CopyPageTable);
}

ULONG __declspec(naked) __cdecl _LinPageLock(ULONG page, ULONG npages, ULONG flags)
{
	VMMJmp(_LinPageLock);
}

ULONG __declspec(naked) __cdecl _LinPageUnLock(ULONG page, ULONG npages, ULONG flags)
{
	VMMJmp(_LinPageUnLock);
}

ULONG __declspec(naked) __cdecl _MapPhysToLinear(ULONG PhysAddr, ULONG nBytes, ULONG flags)
{
	VMMJmp(_MapPhysToLinear);
}

ULONG __declspec(naked) __cdecl _SelectorMapFlat(ULONG VMHandle, ULONG Selector, ULONG flags) /* v10.273 */
{
	VMMJmp(_SelectorMapFlat);
}

ULONG __declspec(naked) __cdecl GetNulPageHandle()
{
	VMMJmp(_MapPhysToLinear);
}

ULONG __declspec(naked) __cdecl _PhysIntoV86(ULONG PhysPage, ULONG VM, ULONG VMLinPgNum, ULONG nPages, ULONG flags)
{
	VMMJmp(_PhysIntoV86);
}

DWORD __declspec(naked) __cdecl _RegOpenKey(DWORD hKey, char *lpszSubKey, DWORD *lphKey)
{
	VMMJmp(_RegOpenKey);
}

DWORD __declspec(naked) __cdecl _RegCloseKey(DWORD hKey)
{
	VMMJmp(_RegCloseKey);
}

DWORD __declspec(naked) __cdecl _RegQueryValueEx(DWORD hKey, char *lpszValueName, DWORD *lpdwReserved, DWORD *lpdwType, BYTE *lpbData, DWORD *lpcbData)
{
	VMMJmp(_RegQueryValueEx);
}

DWORD __declspec(naked) __cdecl _PageModifyPermissions(ULONG page, ULONG npages, ULONG permand, ULONG permor)
{
	VMMJmp(_PageModifyPermissions);
}

DWORD __declspec(naked) __cdecl _PageReserve(ULONG page, ULONG npages, ULONG flags)
{
	VMMJmp(_PageReserve);
}

DWORD __declspec(naked) __cdecl _PageCommit(ULONG page, ULONG npages, ULONG hpd, ULONG pagerdata, ULONG flags)
{
	VMMJmp(_PageCommit);
}

DWORD __declspec(naked) __cdecl _PageReAllocate(ULONG hMem, ULONG nPages, ULONG flags)
{
	VMMJmp(_PageReAllocate);
}

DWORD __declspec(naked) __cdecl _PageCommitPhys(ULONG page, ULONG npages, ULONG physpg, ULONG flags)
{
	VMMJmp(_PageCommitPhys);
}

DWORD __declspec(naked) __cdecl _PageCommitContig(ULONG page, ULONG npages, ULONG flags, ULONG alignmask, ULONG minphys, ULONG maxphys)
{
	VMMJmp(_PageCommitContig);
}

DWORD __declspec(naked) __cdecl _PageDecommit(ULONG page, ULONG npages, ULONG flags)
{
	VMMJmp(_PageDecommit);
}

DWORD __declspec(naked) __cdecl _LinMapIntoV86(ULONG HLinPgNum, ULONG VM, ULONG VMLinPgNum, ULONG nPages, ULONG flags)
{
	VMMJmp(_LinMapIntoV86);
}

DWORD __declspec(naked) __cdecl _MapIntoV86(ULONG hMem, ULONG VM, ULONG VMLinPgNum, ULONG nPages, ULONG PageOff, ULONG flags)
{
	VMMJmp(_MapIntoV86);
}

DWORD __declspec(naked) __cdecl _GetFirstV86Page()
{
	VMMJmp(_GetFirstV86Page);
}

DWORD __declspec(naked) __cdecl _GetLastV86Page()
{
	VMMJmp(_GetLastV86Page);
}

DWORD __declspec(naked) __cdecl _SetLastV86Page(ULONG PgNum, ULONG flags)
{
	VMMJmp(_SetLastV86Page);
}

DWORD __declspec(naked) __cdecl _Allocate_Global_V86_Data_Area(ULONG nBytes, ULONG flags)
{
	VMMJmp(_Allocate_Global_V86_Data_Area);
}

void Enable_Global_Trapping(DWORD port)
{
	_asm push edx
	_asm mov edx, [port];
	VMMCall(Enable_Global_Trapping);
	_asm pop edx
}

void Disable_Global_Trapping(DWORD port)
{
	_asm push edx
	_asm mov edx, [port];
	VMMCall(Disable_Global_Trapping);
	_asm pop edx
}

void Hook_V86_Int_Chain(DWORD int_num, DWORD HookProc)
{
	_asm push esi
	_asm mov eax, [int_num]
	_asm mov esi, [HookProc]
	VMMCall(Hook_V86_Int_Chain)
	_asm pop esi
}

DWORD Set_Async_Time_Out(DWORD delayms, DWORD refdata, void *callback)
{
	volatile DWORD handle = 0;
	
	_asm push esi
	_asm mov eax, [delayms]
	_asm mov edx, [refdata]
	_asm mov esi, [callback]
	VMMCall(Set_Async_Time_Out)
	_asm mov [handle], esi
	_asm pop esi
	
	return handle;
}

/*
	Async_Time_Out_Proc
		ecx = tardiness (number of extra milliseconds that have elapsed)
		edx = refdata
*/


DWORD Set_Global_Time_Out(DWORD delayms, DWORD refdata, void *callback)
{
	volatile DWORD handle = 0;
	
	_asm push esi
	_asm mov eax, [delayms]
	_asm mov edx, [refdata]
	_asm mov esi, [callback]
	VMMCall(Set_Global_Time_Out)
	_asm mov [handle], esi
	_asm pop esi
	
	return handle;
}

void Cancel_Time_Out(DWORD handle)
{
	_asm push esi
	_asm mov esi, [handle]
	VMMCall(Cancel_Time_Out)
	_asm pop esi
}

DWORD Get_System_Time()
{
	DWORD ticks = 0;
	/* A normal C function has saved registers above its return address.
	 * VMMJmp would return through that saved state, bypassing the epilogue.
	 * Call the service and let the compiler restore the C stack normally. */
	VMMCall(Get_System_Time);
	_asm mov [ticks], eax
	return ticks;
}

DWORD *Get_System_Time_Address()
{
	DWORD *retaddr = NULL;
	
	VMMCall(Get_System_Time_Address);
	_asm mov [retaddr], eax
	
	return retaddr; /* never reached */
}

void _EnterMustComplete()
{
	VMMCall(_EnterMustComplete);
}

void _LeaveMustComplete()
{
	VMMCall(_LeaveMustComplete);
}

BOOL Get_Crit_Section_Status(DWORD *out_vm, DWORD *out_claims)
{
	DWORD vm = 0;
	DWORD claims = 0;
	DWORD boost = 0;
	VMMCall(Get_Crit_Section_Status);
	_asm
	{
		mov [vm], ebx
		mov [claims], ecx
		jnc no_boost
		mov [boost], 1
		no_boost:
	}
	
	if(out_vm != NULL)
	{
		*out_vm = vm;
	}
	
	if(out_claims != NULL)
	{
		*out_claims = claims;
	}
	
	if(claims == 0 && boost == 0)
	{
		return FALSE;
	}
	
	return TRUE;
}

void critical_section_enter()
{
	Begin_Critical_Section(BLOCK_SVC_INTS);
	_EnterMustComplete();
}

void critical_section_leave()
{
	_LeaveMustComplete();
	End_Critical_Section();
}

PVOID __declspec(naked) __cdecl _ContextCreate()
{
	VMMJmp(_ContextCreate);
}

ULONG __declspec(naked) __cdecl _ContextDestroy(PVOID hcd)
{
	VMMJmp(_ContextDestroy);
}

PVOID __declspec(naked) __cdecl _ContextSwitch(PVOID hcd)
{
	VMMJmp(_ContextSwitch);
}

PVOID __declspec(naked) __cdecl _GetCurrentContext()
{
	VMMJmp(_GetCurrentContext);
}

ULONG __declspec(naked) __cdecl _PageAttach(ULONG page, PVOID hcontextsrc, ULONG npages)
{
	VMMJmp(_PageAttach);
}
