/* host test shim (gl/host): the few Win32 names VXGL's Mesa side uses, for building it on Linux */
#ifndef VXGL_HOST_WINDOWS_H
#define VXGL_HOST_WINDOWS_H
#include <stdint.h>
typedef void *HANDLE,*HWND,*HDC,*HMODULE,*HINSTANCE,*LPVOID;
typedef uint32_t DWORD;typedef int BOOL;typedef uint16_t WORD;typedef uint8_t BYTE;typedef int32_t LONG; /* v10.330 (review #35): Win32's 32 bits (long is 64 on LP64) */
typedef struct {LONG left,top,right,bottom;} RECT;
typedef struct {LONG x,y;} POINT;
#define INVALID_HANDLE_VALUE ((HANDLE)(intptr_t)-1)
#define TRUE 1
#define FALSE 0
#define WINAPI
#define APIENTRY
BOOL GetClientRect(HWND,RECT*);
BOOL DeviceIoControl(HANDLE,DWORD,void*,DWORD,void*,DWORD,DWORD*,void*);
#define ERROR_BUSY 170L
#define ERROR_INVALID_HANDLE 6L
#define ERROR_NOT_ENOUGH_MEMORY 8L
#define ERROR_INVALID_PIXEL_FORMAT 2000L
DWORD GetCurrentThreadId(void);
DWORD GetCurrentProcessId(void); /* v10.322 */
DWORD GetTickCount(void);
void SetLastError(DWORD);
#include <time.h>
typedef union {struct {DWORD LowPart;LONG HighPart;} u;long long QuadPart;} LARGE_INTEGER; /* v10.333 (vxgl_fp.c's VXPROBE timing) */
static inline BOOL QueryPerformanceFrequency(LARGE_INTEGER *f){f->QuadPart=1000000000LL;return 1;}
static inline BOOL QueryPerformanceCounter(LARGE_INTEGER *c){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);c->QuadPart=(long long)t.tv_sec*1000000000LL+t.tv_nsec;return 1;}
#define __stdcall
#endif
#ifndef _snprintf
#define _snprintf snprintf
#endif
