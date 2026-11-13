/* Win98 GUI test/log reader, Copyright (c) 2026. MIT; see LICENSE. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "vx85598.h"

static HANDLE report=INVALID_HANDLE_VALUE;
static char report_name[MAX_PATH];
static vx_status_t status;
static int connected, test_pass, test_fail, phase;
static const char *failure_names[]={"OK","no unique VX855","multiple/no active VGA",
 "PCI decode/BAR invalid","video ROM invalid","V86 BIOS buffer failed",
 "VBE controller failed","baseline modes unavailable","bad mode table",
 "mode storage failed","framebuffer mapping failed","shadow allocation failed",
 "HDA initialization failed","mode rejected","BIOS mode set failed",
 "Win16 selector allocation failed","mini-VDD dispatch table unavailable",
 "loaded after VMM initialization; restart required"};

static void write_text(const char *s)
{
 DWORD n;
 if(report!=INVALID_HANDLE_VALUE) WriteFile(report,s,(DWORD)strlen(s),&n,NULL);
}

static void open_report(void)
{
 unsigned int i; char base[MAX_PATH], *last;
 GetModuleFileName(NULL,base,sizeof(base));
 last=strrchr(base,'\\'); if(last) last[1]=0; else strcpy(base,"C:\\");
 for(i=0; i<1000; i++) {
  wsprintf(report_name,"%sVX9T%03u.TXT",base,i);
  report=CreateFile(report_name,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(report!=INVALID_HANDLE_VALUE) return;
  if(GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS) break;
 }
 for(i=0; i<1000; i++) {
  wsprintf(report_name,"C:\\VX9T%03u.TXT",i);
  report=CreateFile(report_name,GENERIC_WRITE,FILE_SHARE_READ,NULL,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,NULL);
  if(report!=INVALID_HANDLE_VALUE) return;
  if(GetLastError()!=ERROR_FILE_EXISTS && GetLastError()!=ERROR_ALREADY_EXISTS) break;
 }
 strcpy(report_name,"Unable to create a report file");
}

static DWORD open_error, status_error, status_bytes, probe_error, probe_bytes;
static int opened, status_ok, probe_ok;
static const char *connection_reason;

static void read_status(void)
{
 HANDLE h;
 connected=opened=status_ok=probe_ok=0;
 open_error=status_error=status_bytes=probe_error=probe_bytes=0;
 memset(&status,0,sizeof(status));
 connection_reason="open failed";
 SetLastError(0);
 h=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(h==INVALID_HANDLE_VALUE) { open_error=GetLastError(); return; }
 opened=1;
 SetLastError(0);
 status_ok=DeviceIoControl(h,VX_IOCTL_STATUS,NULL,0,&status,sizeof(status),&status_bytes,NULL);
 if(!status_ok) status_error=GetLastError();
 if(!status_ok) connection_reason="status request failed";
 else if(status_bytes!=sizeof(status)) connection_reason="reply length mismatch";
 else if(status.magic!=VX_MAGIC) connection_reason="reply magic mismatch";
 else if(status.cb!=sizeof(status)) connection_reason="reply structure mismatch";
 else if(status.version!=VX_VERSION) connection_reason="reply version mismatch";
 else { connected=1; connection_reason="connected"; }
 /* Same read-only status request with a deliberately short output buffer.
  * The installed 1.2 handler must reject this with ERROR_INVALID_PARAMETER
  * (87). This tests dispatch without changing driver state. */
 if(!connected) {
  SetLastError(0);
  probe_ok=DeviceIoControl(h,VX_IOCTL_STATUS,NULL,0,NULL,0,&probe_bytes,NULL);
  if(!probe_ok) probe_error=GetLastError();
 }
 CloseHandle(h);
}

static void save_report(HWND hwnd, const char *label)
{
 char line[300], log[VX_LOG_CAPACITY];
 HDC dc; HANDLE h; DWORD n=0;
 OSVERSIONINFO os;
 read_status();
 memset(&os,0,sizeof(os)); os.dwOSVersionInfoSize=sizeof(os); GetVersionEx(&os);
 dc=GetDC(hwnd);
 wsprintf(line,"\r\nVX98TEST 1.2a diagnostic -- %s -- tick %lu\r\nWindows=%lu.%lu build=%lu platform=%lu\r\nDesktop=%dx%d bpp=%d GDI_test_pass=%d GDI_test_fail=%d\r\n",
  label,GetTickCount(),os.dwMajorVersion,os.dwMinorVersion,os.dwBuildNumber,os.dwPlatformId,
  GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),GetDeviceCaps(dc,BITSPIXEL)*GetDeviceCaps(dc,PLANES),test_pass,test_fail);
 ReleaseDC(hwnd,dc); write_text(line);
 wsprintf(line,"Open_ok=%d Open_error=%lu Status_ok=%d Status_error=%lu Returned_bytes=%lu Expected_bytes=%lu\r\n",
 opened,open_error,status_ok,status_error,status_bytes,(DWORD)sizeof(status)); write_text(line);
 wsprintf(line,"Reply_magic=%08lX Expected_magic=%08lX Reply_size=%lu Reply_version=%08lX Expected_version=%08lX\r\n",
 status.magic,(DWORD)VX_MAGIC,status.cb,status.version,(DWORD)VX_VERSION); write_text(line);
 if(opened && !connected) {
  wsprintf(line,"Short_buffer_probe_ok=%d error=%lu bytes=%lu (expected failure/error 87)\r\n",probe_ok,probe_error,probe_bytes); write_text(line);
 }
 if(!connected) {
  wsprintf(line,"Diagnostic connection: %s. Display-driver loading is not determined by this message.\r\n",connection_reason); write_text(line);
 } else {
  const char *reason=status.failure<sizeof(failure_names)/sizeof(failure_names[0]) ? failure_names[status.failure] : "unknown";
  wsprintf(line,"Driver_stage=%lu failure=%lu (%s) PCI=%08lX command=%04lX BAR0=%08lX BAR1=%08lX ROM_FNV=%08lX\r\n",
   status.stage,status.failure,reason,status.pci_address,status.pci_command,status.bar0,status.bar1,status.rom_fingerprint); write_text(line);
  wsprintf(line,"VBE=%04lX modes=%lu BIOS_buffer=%08lX framebuffer_phys=%08lX framebuffer_linear=%08lX bytes=%lu\r\n",
   status.vbe_version,status.mode_count,status.bios_buffer,status.fb_phys,status.fb_linear,status.fb_bytes); write_text(line);
  wsprintf(line,"Active_mode=%04lX %lux%lux%lu shadow_pitch=%lu video_pitch=%lu shadow=%08lX bytes=%lu VGA=%lu\r\n",
   status.active_mode,status.width,status.height,status.bpp,status.shadow_pitch,status.video_pitch,status.shadow_linear,status.shadow_bytes,status.vga_mode); write_text(line);
  wsprintf(line,"Mode_attempts=%lu mode_successes=%lu last_BIOS_AX=%04lX physical_blits=%lu log_bytes=%lu dropped=%lu\r\n",
   status.set_attempts,status.set_successes,status.last_bios_ax,status.draws,status.log_bytes,status.log_dropped); write_text(line);
  h=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
  if(h!=INVALID_HANDLE_VALUE) {
   memset(log,0,sizeof(log));
   if(DeviceIoControl(h,VX_IOCTL_LOG,NULL,0,log,sizeof(log),&n,NULL) && n && n<=sizeof(log)) {
    log[sizeof(log)-1]=0;
    write_text("--- kernel startup log ---\r\n"); write_text(log); write_text("\r\n--- end startup log ---\r\n");
   }
   CloseHandle(h);
  }
 }
 write_text("Visual result must be reported separately; GDI readback does not prove physical scanout.\r\n");
 if(report!=INVALID_HANDLE_VALUE) FlushFileBuffers(report);
}

static void gdi_test(HWND hwnd)
{
 HDC dst=GetDC(hwnd), src=CreateCompatibleDC(dst);
 HBITMAP bm=CreateCompatibleBitmap(dst,32,32), old=NULL;
 HBRUSH br=CreateSolidBrush(RGB(255,0,0));
 RECT r={0,0,32,32};
 test_pass=0; test_fail=0;
 if(src && bm && br) {
  old=SelectObject(src,bm);
  if(FillRect(src,&r,br)) test_pass++; else test_fail++;
  if(GetPixel(src,8,8)==RGB(255,0,0)) test_pass++; else test_fail++;
  if(BitBlt(dst,20,175,32,32,src,0,0,SRCCOPY)) test_pass++; else test_fail++;
  if(GetPixel(dst,24,179)==RGB(255,0,0)) test_pass++; else test_fail++;
  if(BitBlt(dst,20,175,32,32,src,0,0,SRCINVERT) && GetPixel(dst,24,179)==RGB(0,0,0)) test_pass++; else test_fail++;
  if(BitBlt(dst,20,175,32,32,src,0,0,SRCINVERT) && GetPixel(dst,24,179)==RGB(255,0,0)) test_pass++; else test_fail++;
 } else test_fail++;
 if(old) SelectObject(src,old);
 if(br) DeleteObject(br); if(bm) DeleteObject(bm); if(src) DeleteDC(src); ReleaseDC(hwnd,dst);
}

static void paint(HWND hwnd)
{
 static const COLORREF colors[]={RGB(0,0,0),RGB(255,0,0),RGB(0,255,0),RGB(0,0,255),RGB(255,255,0),RGB(0,255,255),RGB(255,0,255),RGB(255,255,255)};
 PAINTSTRUCT ps; HDC dc=BeginPaint(hwnd,&ps); RECT client,r;
 char text[260]; int i,width;
 GetClientRect(hwnd,&client); width=client.right-40;
 SetBkMode(dc,TRANSPARENT); SetTextColor(dc,RGB(0,0,0));
 TextOut(dc,20,10,"VX85598 Windows 98 display test",31);
 TextOut(dc,20,32,"Bars: black, red, green, blue, yellow, cyan, magenta, white",59);
 for(i=0; i<8; i++) {
  HBRUSH br=CreateSolidBrush(colors[i]);
  r.left=20+i*width/8; r.right=20+(i+1)*width/8; r.top=58; r.bottom=118;
  FillRect(dc,&r,br); DeleteObject(br);
 }
 for(i=0; i<width; i++) {
  HPEN pen=CreatePen(PS_SOLID,1,RGB(i*255/width,i*255/width,i*255/width));
  HGDIOBJ old=SelectObject(dc,pen); MoveToEx(dc,20+i,128,NULL); LineTo(dc,20+i,158); SelectObject(dc,old); DeleteObject(pen);
 }
 wsprintf(text,"GDI checks passed: %d   failed: %d",test_pass,test_fail);
 TextOut(dc,65,180,text,(int)strlen(text));
 TextOut(dc,20,222,"Move this window and check that bars, text and mouse redraw correctly.",70);
 wsprintf(text,"Desktop: %dx%d. VX85598 device: %s.",GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),connection_reason);
 TextOut(dc,20,246,text,(int)strlen(text));
 TextOut(dc,20,270,report_name,(int)strlen(report_name));
 EndPaint(hwnd,&ps);
}

static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
 switch(msg) {
  case WM_CREATE:
   open_report(); save_report(hwnd,"opened");
   CreateWindow("BUTTON","Run GDI tests",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,20,305,130,28,hwnd,(HMENU)1,((LPCREATESTRUCT)lp)->hInstance,NULL);
   CreateWindow("BUTTON","Save log",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,165,305,100,28,hwnd,(HMENU)2,((LPCREATESTRUCT)lp)->hInstance,NULL);
   CreateWindow("BUTTON","Close",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,280,305,90,28,hwnd,(HMENU)3,((LPCREATESTRUCT)lp)->hInstance,NULL);
   SetTimer(hwnd,1,500,NULL); return 0;
  case WM_PAINT: paint(hwnd); return 0;
  case WM_TIMER:
   if(!phase) { phase=1; gdi_test(hwnd); InvalidateRect(hwnd,NULL,FALSE); }
   else { KillTimer(hwnd,1); save_report(hwnd,"after drawing"); }
   return 0;
  case WM_COMMAND:
   if(LOWORD(wp)==1) { gdi_test(hwnd); read_status(); InvalidateRect(hwnd,NULL,FALSE); save_report(hwnd,"GDI test"); }
   if(LOWORD(wp)==2) { save_report(hwnd,"manual save"); MessageBox(hwnd,report_name,"Log location",MB_OK); }
   if(LOWORD(wp)==3) DestroyWindow(hwnd);
   return 0;
  case WM_DESTROY:
   KillTimer(hwnd,1); save_report(hwnd,"closed");
   if(report!=INVALID_HANDLE_VALUE) CloseHandle(report);
   PostQuitMessage(0); return 0;
 }
 return DefWindowProc(hwnd,msg,wp,lp);
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE prev, LPSTR cmd, int show)
{
 WNDCLASS wc; HWND hwnd; MSG msg;
 (void)prev; (void)cmd;
 memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=window_proc; wc.hInstance=inst;
 wc.hCursor=LoadCursor(NULL,IDC_ARROW); wc.hbrBackground=(HBRUSH)(COLOR_BTNFACE+1); wc.lpszClassName="VX98TEST";
 if(!RegisterClass(&wc)) return 1;
 hwnd=CreateWindow("VX98TEST","VX85598 diagnostic 1.2a",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,20,20,590,375,NULL,NULL,inst,NULL);
 if(!hwnd) return 2;
 ShowWindow(hwnd,show); UpdateWindow(hwnd);
 while(GetMessage(&msg,NULL,0,0)>0) { TranslateMessage(&msg); DispatchMessage(&msg); }
 return (int)msg.wParam;
}
