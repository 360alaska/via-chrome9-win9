#ifndef __DRVLIB_H__INCLUDED__
#define __DRVLIB_H__INCLUDED__

void __far *drv_malloc(DWORD dwSize, DWORD __far *lpLinear);
BOOL drv_is_p2();

#endif /* __DRVLIB_H__INCLUDED__ */
