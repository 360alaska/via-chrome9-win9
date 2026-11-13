/* VXPROBE v1.5 (driver v10.339; v1.5: bios and modeset list every mode set a change made (one change made about three); v1.4: bios, the timing-block test at 1024x768 or less (#172); v1.3: ovkey reports the VxD's refusal reason and sends twenty updates in a row; v1.2: stages checks that an unread argument changes nothing; v1.1: no command runs all six, the log next to the program): probes for the open review items that need the board or applications. One command a run; each
 * appends to VXPROBE.TXT next to the program. Every display mode it sets is temporary (not saved) and put back at the end.
 *   VXPROBE cursor   review #80:  resolution changes (the DRV's ReEnable) and a DOS box round trip; the cursor state before and after
 *                    each, and what you see (one pointer, a second or stuck pointer, or none).
 *   VXPROBE modeset  reviews #107 and #172: a mode above 1024x768 three ways: as the driver sets it; with the driver's own timing made
 *                    to fail (what the driver reports then, and what the screen shows); and failing again with the BIOS timing call
 *                    made through the V86 wrapper (does the BIOS see the timing block through Exec_VxD_Int?). Each: the BIOS answers,
 *                    the scanned size from the CRTC, the measured refresh, what you saw.
 *   VXPROBE edid     review #198: the EDID at boot, the modes it hid, and a bus scan now (a "late" EDID).
 *   VXPROBE ovkey    review #157: the overlay destination keys the HAL accepts or refuses (its own cases), then the keys your video
 *                    players hand the HAL while you play something.
 *   VXPROBE stages   review #196: stage 0 with no texture bound, every COLOROP / ALPHAOP / argument combination, drawn by the HAL and by
 *                    Microsoft's RGB device; the pixels that differ.
 *   VXPROBE fpcache  review #22: ARB fragment programs, one / two / four alternating and one with a changing local parameter; the
 *                    frame time and the translation memo's hits, misses and cost. MIT, 2026. */
#define COBJMACROS
#define INITGUID
#define DIRECTDRAW_VERSION 0x0700
#define DIRECT3D_VERSION 0x0700
#include <windows.h>
#include <tlhelp32.h>
#include <ddraw.h>
#include <d3d.h>
#include <GL/gl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include "vx85598.h"

static FILE *lf;
static HANDLE dev=INVALID_HANDLE_VALUE;
static void out(const char *fmt,...) {
 char t[1400]; va_list ap; va_start(ap,fmt); _vsnprintf(t,sizeof(t)-1,fmt,ap); va_end(ap); t[sizeof(t)-1]=0;
 fputs(t,stdout); if(lf) {fputs(t,lf); fflush(lf);}
}
static int probe(DWORD sub,DWORD arg,DWORD *o) {
 DWORD in[2],got=0; in[0]=sub; in[1]=arg; memset(o,0,64*4);
 return dev!=INVALID_HANDLE_VALUE&&DeviceIoControl(dev,VX_IOCTL_PROBE,in,8,o,64*4,&got,NULL)&&got==64*4&&o[0]==VX_MAGIC;
}
static int ctl(DWORD code,DWORD *o) {DWORD got=0; memset(o,0,16); return dev!=INVALID_HANDLE_VALUE&&DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)&&got==16;}
static int ask(const char *q) {return MessageBoxA(NULL,q,"VXPROBE",MB_YESNOCANCEL|MB_ICONQUESTION|MB_TOPMOST|MB_SETFOREGROUND);}

/* ---- the display mode ---- */
static DEVMODE start_mode;
static int have_mode(DWORD w,DWORD h,DWORD bpp,DEVMODE *found) {
 DEVMODE m; DWORD i;
 for(i=0;;i++) {memset(&m,0,sizeof(m)); m.dmSize=sizeof(m); if(!EnumDisplaySettings(NULL,i,&m)) break;
  if(m.dmPelsWidth==w&&m.dmPelsHeight==h&&m.dmBitsPerPel==bpp) {if(found) *found=m; return 1;}}
 return 0;
}
static LONG set_mode(DWORD w,DWORD h,DWORD bpp) {
 DEVMODE m; memset(&m,0,sizeof(m)); m.dmSize=sizeof(m); m.dmPelsWidth=w; m.dmPelsHeight=h; m.dmBitsPerPel=bpp; m.dmFields=DM_PELSWIDTH|DM_PELSHEIGHT|DM_BITSPERPEL;
 return ChangeDisplaySettings(&m,0); /* dynamic, not saved */
}
static const char *cds_name(LONG r) {
 switch(r) {case DISP_CHANGE_SUCCESSFUL: return "DISP_CHANGE_SUCCESSFUL"; case DISP_CHANGE_RESTART: return "DISP_CHANGE_RESTART"; case DISP_CHANGE_BADMODE: return "DISP_CHANGE_BADMODE";
  case DISP_CHANGE_FAILED: return "DISP_CHANGE_FAILED"; default: return "other";}
}
static void pump(DWORD ms) {DWORD t=GetTickCount(); MSG m; while(GetTickCount()-t<ms) {while(PeekMessage(&m,NULL,0,0,PM_REMOVE)) {TranslateMessage(&m); DispatchMessage(&m);} Sleep(10);}}

/* ---- the driver's state lines ---- */
static void state_line(const char *when) {
 DWORD o[64],c[4];
 if(!probe(1,0,o)) {out("   %s: the VxD does not answer VX_IOCTL_PROBE (older than v10.333)\n",when); return;}
 out("   %s: %lu x %lu x %lu; cursor: hardware %lu (shape ok %lu), software drawn now %lu, software draws %lu, visible %lu at (%ld,%ld), shape generation %lu",when,
  o[29],o[30],o[31],o[19],o[20],o[18],o[26],o[21],(long)o[22],(long)o[23],o[27]);
 if(ctl(0xFFFFFE16UL,c)) out("; unit: moves %lu, shapes %lu, hides %lu, on %lu",c[0],c[1],c[2],c[3]);
 out("\n");
}
static void set_record(const char *when) {
 DWORD o[64];
 if(!probe(1,0,o)) return;
 out("   %s: mode set #%lu %lux%lux%lu (rate asked %lu..%lu): timing block %lu Hz%s; BIOS AX %s%04lX%s",when,o[2],o[3],o[4],o[5],o[6],o[7],o[8],
  o[8]?"":" (none: the BIOS default timing)",o[9]==0xFFFFFFFFUL?"":"",o[9]==0xFFFFFFFFUL?0UL:o[9],o[9]==0xFFFFFFFFUL?" (no call with the timing block)":o[11]?" through vesa_bios_V86":" through vesa_bios (Exec_VxD_Int)");
 if(o[10]!=0xFFFFFFFFUL) out(", then the plain set AX %04lX",o[10]);
 out("; set %s; driver timing %lu (0 untouched, 1 reprogrammed, 2 failed) at %lu Hz\n",o[12]?"counted a success":"failed",o[13],o[14]);
}
/* v1.5: every mode set since the count 'from' (the VxD keeps the last five) */
static DWORD set_count(void) {DWORD o[64]; return probe(1,0,o)?o[2]:0;}
static void set_history(DWORD from) {
 DWORD o[64],m,i,n0;
 if(!probe(4,0,o)) {out("   (no mode-set history: VxD older than v10.339)\n"); return;}
 m=o[63]; n0=o[2]-m+1;
 if(o[2]-from>m) out("   (%lu earlier sets not kept)\n",o[2]-from-m);
 for(i=0;i<m;i++) {const DWORD *e=o+3+i*12; if(n0+i<=from) continue;
  out("   set #%lu: %lux%lux%lu, rate asked %lu..%lu, timing block %lu Hz, BIOS AX %s",n0+i,e[0],e[1],e[2],e[3],e[4],e[5],e[6]==0xFFFFFFFFUL?"(no block call)":"");
  if(e[6]!=0xFFFFFFFFUL) out("%04lX through %s",e[6],e[8]?"vesa_bios_V86":"vesa_bios (Exec_VxD_Int)");
  if(e[7]!=0xFFFFFFFFUL) out(", then the plain set %04lX",e[7]);
  out(", %s, driver timing %lu\n",e[9]?"success":"FAILED",e[10]);}
}
static void crtc_size(DWORD *hd,DWORD *vd) {
 static vx_crt_report c; DWORD got=0; *hd=*vd=0;
 if(DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,&c,sizeof(c),&got,NULL)&&got==sizeof(c)) {
  DWORD cr7=c.cr[7]&255,cr35=c.cr[0x35]&255;
  *hd=((c.cr[1]&255)+1)*8; *vd=((c.cr[0x12]&255)|(((cr7>>1)&1)<<8)|(((cr7>>6)&1)<<9)|(((cr35>>2)&1)<<10))+1;
 }
}
static double refresh(void) {
 DWORD o[64]; double ms;
 if(!probe(2,0,o)||o[2]<3||!o[5]||!o[4]) return 0;
 ms=(double)o[3]*(double)o[5]/(double)o[4]; /* TSC ticks between the first and last retrace, at the TSC rate over the whole poll */
 return ms>0?(o[2]-1)*1000.0/ms:0;
}

/* ================= cursor (review #80) ================= */
static LRESULT CALLBACK white_proc(HWND w,UINT m,WPARAM a,LPARAM b) {
 if(m==WM_ERASEBKGND) {RECT r; GetClientRect(w,&r); FillRect((HDC)a,&r,(HBRUSH)GetStockObject(WHITE_BRUSH)); return 1;}
 if(m==WM_DISPLAYCHANGE) {MoveWindow(w,0,0,LOWORD(b),HIWORD(b),TRUE); return 0;}
 return DefWindowProc(w,m,a,b);
}
static HWND white_window(void) {
 WNDCLASSA wc; memset(&wc,0,sizeof(wc)); wc.lpfnWndProc=white_proc; wc.hInstance=GetModuleHandleA(0); wc.lpszClassName="VXPROBEW"; wc.hCursor=LoadCursor(NULL,IDC_ARROW); RegisterClassA(&wc);
 return CreateWindowExA(WS_EX_TOPMOST,"VXPROBEW","VXPROBE",WS_POPUP|WS_VISIBLE,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),NULL,NULL,wc.hInstance,NULL);
}
static int cur_bad;
static void cursor_check(HWND w,const char *what) {
 char q[400]; int a;
 MoveWindow(w,0,0,GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),TRUE); SetForegroundWindow(w);
 SetCursorPos(GetSystemMetrics(SM_CXSCREEN)/2,GetSystemMetrics(SM_CYSCREEN)/2); pump(400);
 state_line("after");
 sprintf(q,"%s\n\nMove the mouse around the white screen for a few seconds (also near the edges).\n\nYes = exactly one pointer, nothing left behind\nNo = two pointers, or a stuck / left-over pointer image\nCancel = no pointer at all",what);
 a=ask(q);
 out("   you saw: %s\n",a==IDYES?"one pointer, nothing left behind":a==IDNO?"TWO POINTERS OR A LEFT-OVER IMAGE":"NO POINTER");
 if(a!=IDYES) cur_bad++;
}
static void cmd_cursor(void) {
 static const DWORD res[4][2]={{640,480},{800,600},{1024,768},{0,0}};
 HWND w; int i; LONG r; char what[160]; DWORD bpp=start_mode.dmBitsPerPel;
 out("== cursor (review #80): resolution changes at %lu bpp through the DRV's ReEnable, then a DOS box round trip\n",bpp);
 w=white_window(); pump(300);
 state_line("start");
 cursor_check(w,"Start: your current mode.");
 for(i=0;i<4;i++) {
  DWORD x=res[i][0]?res[i][0]:start_mode.dmPelsWidth,y=res[i][1]?res[i][1]:start_mode.dmPelsHeight;
  if(!have_mode(x,y,bpp,NULL)) {out("   %lux%lux%lu: not offered\n",x,y,bpp); continue;}
  state_line("before");
  r=set_mode(x,y,bpp); out("   ChangeDisplaySettings(%lux%lux%lu) = %s\n",x,y,bpp,cds_name(r)); pump(800);
  sprintf(what,"Mode %lu x %lu x %lu (%s).",x,y,bpp,i==3?"back to where you started":"resolution change");
  cursor_check(w,what);
 }
 DestroyWindow(w); pump(200);
 if(ask("Last step: a DOS box round trip.\n\nPress Yes, then open an MS-DOS Prompt, press Alt+Enter (full screen) and Alt+Enter again (window), type EXIT.\nVXPROBE waits 30 seconds, then asks about the pointer.\n\nNo or Cancel skips this step.")==IDYES) {
  state_line("before the DOS box"); pump(30000);
  w=white_window(); pump(300); cursor_check(w,"After the DOS box round trip."); DestroyWindow(w);
 } else out("   DOS box step skipped\n");
 out("   cursor: %s\n",cur_bad?"PROBLEM SEEN (see the lines above)":"one pointer every time");
}

/* ================= modeset (reviews #107, #172) ================= */
static void cmd_modeset(void) {
 static const DWORD big[][2]={{1280,1024},{1152,864},{1280,960},{1280,768},{1280,720},{1360,768},{1440,900},{1600,1200},{1680,1050},{1920,1080}};
 DWORD bpp=start_mode.dmBitsPerPel,x=0,y=0,k,o[64],hd,vd; int run,a; LONG r; char q[400]; double hz;
 out("== modeset (reviews #107, #172): a mode above 1024x768, three ways\n");
 hz=refresh(); crtc_size(&hd,&vd); out("   now: %lux%lu scanned %lux%lu, %.1f Hz measured\n",start_mode.dmPelsWidth,start_mode.dmPelsHeight,hd,vd,hz);
 if(probe(1,0,o)) out("   driver timing switch (CUSTOM_TIMING) %lu\n",o[15]);
 for(k=0;k<sizeof(big)/sizeof(big[0]);k++) if(have_mode(big[k][0],big[k][1],bpp,NULL)) {x=big[k][0]; y=big[k][1]; break;}
 if(!x) {out("   no mode above 1024x768 is offered at %lu bpp (no EDID: NOEDIDHIGHRES = 1 offers them): nothing to test\n",bpp); return;}
 for(run=0;run<3;run++) {
  static const char *name[3]={"A: as the driver sets it","B: the driver's own timing made to fail (review #107)","C: failing again, the BIOS timing call through vesa_bios_V86 (review #172)"};
  out("-- %s, %lux%lux%lu\n",name[run],x,y,bpp);
  if(start_mode.dmPelsWidth==x&&start_mode.dmPelsHeight==y) {r=set_mode(1024,768,bpp); out("   first 1024x768 (the test mode is the current one): %s\n",cds_name(r)); pump(1500);}
  if(run) {if(!probe(3,run==1?1UL:3UL,o)) {out("   arming failed\n"); continue;} out("   armed %lu\n",o[2]);}
  {DWORD before=set_count();
  r=set_mode(x,y,bpp); probe(3,0,o); /* disarmed: the switches hold for every set until here */
  out("   ChangeDisplaySettings = %s\n",cds_name(r)); pump(1500);
  set_history(before);}
  crtc_size(&hd,&vd); hz=refresh();
  out("   scanned (CRTC display end) %lux%lu for a %lux%lu desktop; %.1f Hz measured\n",hd,vd,x,y,hz);
  pump(4000);
  r=ChangeDisplaySettings(NULL,0); out("   back to the start mode: %s\n",cds_name(r)); pump(1500);
  sprintf(q,"Run %c (%lu x %lu), the 5 seconds just past:\n\nYes = the whole desktop was on the screen\nNo = only part of it (top-left), or it was cut or wrong\nCancel = no picture / the monitor said out of range",'A'+run,x,y);
  a=ask(q); out("   you saw: %s\n",a==IDYES?"the whole desktop":a==IDNO?"ONLY PART / WRONG":"NO PICTURE");
 }
 set_record("after the restore");
}

/* ================= bios (review #172), v1.4: at a mode up to 1024x768, no NOEDIDHIGHRES needed ================= */
/* the scanned totals from the CRTC: horizontal (CR00 + 5) x 8, vertical CR06 | CR07 bit 0 << 8 | bit 5 << 9 | CR35 bit 0 << 10, + 2 */
static void crtc_totals(DWORD *ht,DWORD *vt) {
 static vx_crt_report c; DWORD got=0; *ht=*vt=0;
 if(DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,&c,sizeof(c),&got,NULL)&&got==sizeof(c)) {
  DWORD cr7=c.cr[7]&255;
  *ht=((c.cr[0]&255)+5)*8; *vt=((c.cr[6]&255)|((cr7&1)<<8)|(((cr7>>5)&1)<<9)|((c.cr[0x35]&1)<<10))+2;
 }
}
static void cmd_bios(void) {
 DWORD w=start_mode.dmPelsWidth,h=start_mode.dmPelsHeight,bpp=start_mode.dmBitsPerPel,o[64],ht,vt,ht0,vt0; int run; LONG r; double hz;
 DWORD aw=w==800?640:800,ah=w==800?480:600;
 out("== bios (review #172): the mode set's timing block (GTF 60 Hz) at %lux%lu, through vesa_bios (Exec_VxD_Int) and through vesa_bios_V86\n",w,h);
 if(w>1024||h>768) {out("   the desktop is above 1024x768: the driver's own timing runs there; use 1024x768 or less\n"); return;}
 if(!have_mode(aw,ah,bpp,NULL)) {out("   %lux%lux%lu is not offered (needed to set the mode again)\n",aw,ah,bpp); return;}
 crtc_totals(&ht0,&vt0); hz=refresh();
 out("   now (the BIOS's own timing): totals %lu x %lu, %.2f Hz measured\n",ht0,vt0,hz);
 for(run=0;run<2;run++) {
  out("-- %s\n",run?"B: the timing block through vesa_bios_V86 (the client ES set)":"A: the timing block through vesa_bios (Exec_VxD_Int), as the driver does");
  r=set_mode(aw,ah,bpp); pump(800);
  if(!probe(3,run?6UL:4UL,o)) {out("   arming failed (VxD older than v10.338?)\n"); break;}
  {DWORD before=set_count();
  r=set_mode(w,h,bpp); probe(3,0,o); out("   ChangeDisplaySettings(%lux%lu) = %s\n",w,h,cds_name(r)); pump(1500);
  set_history(before);}
  crtc_totals(&ht,&vt); hz=refresh();
  out("   totals %lu x %lu, %.2f Hz measured: %s\n",ht,vt,hz,(ht==ht0&&vt==vt0)?"the BIOS's own timing (the block was not used)":"CHANGED: the BIOS used the timing block");
  r=set_mode(aw,ah,bpp); pump(800); r=set_mode(w,h,bpp); pump(1200); /* back to the BIOS's own timing */
 }
 crtc_totals(&ht,&vt); out("   after: totals %lu x %lu%s\n",ht,vt,(ht==ht0&&vt==vt0)?" (as at the start)":" (NOT as at the start)");
}

/* ================= edid (review #198) ================= */
static void cmd_edid(void) {
 static vx_edid_report e; DWORD got=0,in,i,hid=0,o[64]; HKEY k; DWORD v,t,n;
 out("== edid (review #198)\n");
 in=0;
 if(!DeviceIoControl(dev,VX_IOCTL_EDID,&in,4,&e,sizeof(e),&got,NULL)||got!=sizeof(e)) {out("   EDID report failed\n"); return;}
 out("   at boot: EDID %s (bus %lu); modes: %lu base, %lu offered, extras %lu; BIOS modes %lu\n",e.edid_valid==1?"valid":e.edid_valid==2?"not read at boot, read later":"NOT READ",
  e.edid_bus,e.base_modes,e.total_modes,e.extras_on,e.bios_count);
 for(i=0;i<e.bios_count&&i<64;i++) if(((e.bios[i*3]>>16)&255)==9) {if(!hid++) out("   hidden at boot for want of an EDID:"); out(" %lux%lux%lu",e.bios[i*3+1]&0xFFFF,e.bios[i*3+1]>>16,e.bios[i*3+2]&255);}
 if(hid) out("\n"); else out("   no mode was hidden for want of an EDID\n");
 if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Software\\VX85598",0,KEY_READ,&k)==ERROR_SUCCESS) {
  n=4; v=0; out("   NOEDIDHIGHRES %s",RegQueryValueExA(k,"NOEDIDHIGHRES",NULL,&t,(BYTE*)&v,&n)==ERROR_SUCCESS?(v?"1":"0"):"not set (0)");
  n=4; v=0; out(", EDID_MODES %s\n",RegQueryValueExA(k,"EDID_MODES",NULL,&t,(BYTE*)&v,&n)==ERROR_SUCCESS?(v==2?"2":v?"1":"0"):"not set (1)");
  RegCloseKey(k);
 }
 if(probe(1,0,o)) {out("   the driver's mode table (%lu):",o[38]); for(i=0;i<o[38];i++) out(" %lux%lux%lu",o[40+i]&0xFFF,(o[40+i]>>12)&0xFFF,o[40+i]>>24); out("\n");}
 in=1; got=0;
 if(!DeviceIoControl(dev,VX_IOCTL_EDID,&in,4,&e,sizeof(e),&got,NULL)||got!=sizeof(e)) {out("   bus scan failed\n"); return;}
 for(i=0;i<3;i++) {const DWORD *q=(const DWORD*)e.scan+12*i; out("   bus %lu now: lines %lu, devices %lu, EDID address %s, EDID read %s\n",i,q[0],q[1],q[10]?"answered":"silent",q[11]?"VALID":"no");}
 out("   now: EDID %s\n",e.edid_valid==1?"valid since boot":e.edid_valid==2?"valid only now (a LATE EDID: the modes hidden at boot stay hidden until a restart)":"still not read");
}

/* ================= ovkey (review #157) ================= */
static DWORD *ovk,*ovk_on;
#include "vx855_2d.h"
/* v1.3: the VxD's last overlay refusal reason (VXO_QUERY), after each case */
static DWORD ov_reason(void) {vx_ovl_req q; DWORD r[VXO_REPLY_DWORDS],got=0; memset(&q,0,sizeof(q)); q.cb=sizeof(q); q.abi=VXO_ABI; q.cmd=VXO_QUERY; memset(r,0,sizeof(r));
 return dev!=INVALID_HANDLE_VALUE&&DeviceIoControl(dev,VX_IOCTL_OVL,&q,sizeof(q),r,sizeof(r),&got,NULL)?r[VXO_R_REASON]:0xFFFFFFFFUL;}
static DWORD ov_last_seen;
static void ov_note(void) {DWORD x=ov_reason(); if(x!=ov_last_seen) out("      (the VxD's last refusal reason is now %lu%s)\n",x,x==10?": the previous overlay command was not loaded yet":""); ov_last_seen=x;}
static DWORD pid_seen[64]; static char pid_name[64][40]; static int pid_n; static volatile int poll_stop;
static DWORD WINAPI pid_poll(LPVOID p) {
 (void)p;
 while(!poll_stop) {
  HANDLE s=CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0); PROCESSENTRY32 e; int i;
  if(s!=INVALID_HANDLE_VALUE) {e.dwSize=sizeof(e);
   if(Process32First(s,&e)) do {for(i=0;i<pid_n;i++) if(pid_seen[i]==e.th32ProcessID) break;
    if(i==pid_n&&pid_n<64) {const char *b=strrchr(e.szExeFile,'\\'); pid_seen[pid_n]=e.th32ProcessID; strncpy(pid_name[pid_n],b?b+1:e.szExeFile,39); pid_name[pid_n][39]=0; pid_n++;}}
   while(Process32Next(s,&e));
   CloseHandle(s);}
  Sleep(500);
 }
 return 0;
}
static const char *pname(DWORD pid) {int i; for(i=0;i<pid_n;i++) if(pid_seen[i]==pid) return pid_name[i]; return pid==GetCurrentProcessId()?"VXPROBE.EXE":"?";}
static void ovk_dump(DWORD from) {
 static const char *kind[5]={"?","UpdateOverlay KEYDESTOVERRIDE","UpdateOverlay KEYDEST (surface key)","SetColorKey DESTOVERLAY","UpdateOverlay source key"};
 DWORD n=ovk[0],i;
 if(n-from>16) {out("   (%lu earlier entries were overwritten)\n",n-from-16); from=n-16;}
 for(i=from;i<n;i++) {const DWORD *e=ovk+1+(i%16)*6;
  out("   #%lu %s: flags %08lX key %08lX..%08lX%s, HAL answer %08lX, %s\n",i+1,kind[e[0]<5?e[0]:0],e[1],e[2],e[3],e[2]!=e[3]?" (A RANGE)":"",e[4],pname(e[5]));}
}
static void cmd_ovkey(void) {
 HRESULT (WINAPI *create)(GUID *,void **,REFIID,IUnknown *); HMODULE ddl,hal; IDirectDraw7 *dd=0; IDirectDrawSurface7 *pri=0,*ov=0;
 DDSURFACEDESC2 s; DDOVERLAYFX fx; DDCOLORKEY ck; RECT sr,dr; HRESULT hr; DWORD start,key; int c; HANDLE th; DWORD tid;
 static const GUID iid_dd7={0x15E65EC0UL,0x3B9C,0x11D2,{0xB9,0x2F,0,0x60,0x97,0x97,0xEA,0x5B}};
 out("== ovkey (review #157)\n");
 ddl=LoadLibraryA("DDRAW.DLL"); create=ddl?(HRESULT (WINAPI *)(GUID *,void **,REFIID,IUnknown *))GetProcAddress(ddl,"DirectDrawCreateEx"):0;
 if(!create||FAILED(create(NULL,(void**)&dd,&iid_dd7,NULL))) {out("   DirectDrawCreateEx failed\n"); return;}
 hal=GetModuleHandleA("VXDDRAW.DLL"); if(!hal) hal=LoadLibraryA("VXDDRAW.DLL");
 ovk=hal?(DWORD*)GetProcAddress(hal,"VXOVKEY"):0; ovk_on=hal?(DWORD*)GetProcAddress(hal,"VXOVKEYON"):0;
 if(!ovk||!ovk_on) {out("   VXDDRAW.DLL has no VXOVKEY record (older than v10.333)\n"); IDirectDraw7_Release(dd); return;}
 *ovk_on=1; start=ovk[0];
 IDirectDraw7_SetCooperativeLevel(dd,NULL,DDSCL_NORMAL);
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS; s.ddsCaps.dwCaps=DDSCAPS_PRIMARYSURFACE;
 if(FAILED(hr=IDirectDraw7_CreateSurface(dd,&s,&pri,NULL))) out("   primary: %08lX\n",hr);
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT|DDSD_PIXELFORMAT; s.dwWidth=64; s.dwHeight=64;
 s.ddsCaps.dwCaps=DDSCAPS_OVERLAY|DDSCAPS_VIDEOMEMORY; s.ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT); s.ddpfPixelFormat.dwFlags=DDPF_FOURCC; s.ddpfPixelFormat.dwFourCC=MAKEFOURCC('Y','U','Y','2');
 if(pri&&FAILED(hr=IDirectDraw7_CreateSurface(dd,&s,&ov,NULL))) out("   YUY2 overlay: %08lX\n",hr);
 if(pri&&ov) {
  key=start_mode.dmBitsPerPel==16?0xF81FUL:0xFF00FFUL; SetRect(&sr,0,0,64,64); SetRect(&dr,32,32,160,160);
  memset(&fx,0,sizeof(fx)); fx.dwSize=sizeof(fx);
  fx.dckDestColorkey.dwColorSpaceLowValue=fx.dckDestColorkey.dwColorSpaceHighValue=key;
  out("   1 KEYDESTOVERRIDE one key: %08lX\n",IDirectDrawSurface7_UpdateOverlay(ov,&sr,pri,&dr,DDOVER_SHOW|DDOVER_KEYDESTOVERRIDE,&fx));
  ov_note();
  fx.dckDestColorkey.dwColorSpaceHighValue=key^0x0101UL;
  out("   2 KEYDESTOVERRIDE a range (low != high): %08lX\n",IDirectDrawSurface7_UpdateOverlay(ov,&sr,pri,&dr,DDOVER_SHOW|DDOVER_KEYDESTOVERRIDE,&fx));
  ov_note();
  fx.dckDestColorkey.dwColorSpaceLowValue=fx.dckDestColorkey.dwColorSpaceHighValue=0;
  out("   3 KEYDESTOVERRIDE 0..0: %08lX\n",IDirectDrawSurface7_UpdateOverlay(ov,&sr,pri,&dr,DDOVER_SHOW|DDOVER_KEYDESTOVERRIDE,&fx));
  ov_note();
  ck.dwColorSpaceLowValue=ck.dwColorSpaceHighValue=key;
  out("   4 SetColorKey(primary, DESTOVERLAY, one key): %08lX",IDirectDrawSurface7_SetColorKey(pri,DDCKEY_DESTOVERLAY,&ck));
  out("; UpdateOverlay KEYDEST: %08lX\n",IDirectDrawSurface7_UpdateOverlay(ov,&sr,pri,&dr,DDOVER_SHOW|DDOVER_KEYDEST,NULL));
  ov_note();
  ck.dwColorSpaceHighValue=key^0x0101UL;
  out("   5 SetColorKey(primary, DESTOVERLAY, a range): %08lX",IDirectDrawSurface7_SetColorKey(pri,DDCKEY_DESTOVERLAY|DDCKEY_COLORSPACE,&ck));
  out("; UpdateOverlay KEYDEST: %08lX\n",IDirectDrawSurface7_UpdateOverlay(ov,&sr,pri,&dr,DDOVER_SHOW|DDOVER_KEYDEST,NULL));
  ov_note();
  {int k,ref=0; HRESULT h; /* v1.3: twenty updates in a row, as a player moving its window sends them */
   fx.dckDestColorkey.dwColorSpaceLowValue=fx.dckDestColorkey.dwColorSpaceHighValue=key;
   for(k=0;k<20;k++) {SetRect(&dr,32+k,32,160+k,160); h=IDirectDrawSurface7_UpdateOverlay(ov,&sr,pri,&dr,DDOVER_SHOW|DDOVER_KEYDESTOVERRIDE,&fx); if(FAILED(h)) {ref++; out("   6 update %d of 20 in a row refused: %08lX, VxD reason %lu\n",k+1,h,ov_reason());}}
   out("   6 twenty updates in a row: %d refused\n",ref);}
  IDirectDrawSurface7_UpdateOverlay(ov,NULL,pri,NULL,DDOVER_HIDE,NULL);
 }
 out("   what the HAL was handed:\n"); ovk_dump(start); start=ovk[0];
 if(ov) IDirectDrawSurface7_Release(ov); if(pri) IDirectDrawSurface7_Release(pri);
 poll_stop=0; th=CreateThread(NULL,0,pid_poll,NULL,0,&tid);
 c=ask("Now play a few seconds of video in each player you use (Windows Media Player, a DVD player, ActiveMovie, a game's movies), one at a time, and close each.\n\nKeep VXPROBE open meanwhile. Press Yes when done (No or Cancel skips).");
 poll_stop=1; if(th) {WaitForSingleObject(th,3000); CloseHandle(th);}
 if(c==IDYES) {out("   your players handed the HAL (%lu):\n",ovk[0]-start); if(ovk[0]==start) out("   nothing: no overlay with a key was used\n"); else ovk_dump(start);}
 else out("   the player step was skipped\n");
 *ovk_on=0;
 IDirectDraw7_Release(dd);
}

/* ================= stages (review #196) ================= */
typedef struct {float x,y,z,rhw;DWORD c,s;} tlv;
static const GUID iid_dd7b={0x15E65EC0UL,0x3B9C,0x11D2,{0xB9,0x2F,0,0x60,0x97,0x97,0xEA,0x5B}};
static const GUID iid_d3d7={0xF5049E77UL,0x4861,0x11D2,{0xA4,0x07,0x00,0xA0,0xC9,0x06,0x29,0xA8}};
static const GUID iid_hal={0x84E63DE0UL,0x46AA,0x11CF,{0x81,0x6F,0x00,0x00,0xC0,0x20,0x15,0x6E}};
static const GUID iid_rgb={0xA4665C60UL,0x2673,0x11CF,{0xA3,0x1A,0x00,0xAA,0x00,0xB9,0x33,0x56}};
static DWORD chan(DWORD v,DWORD m) {DWORD s=0,b=0,x; if(!m) return 0; while(!(m&1)) {m>>=1; s++;} while(m&(1UL<<b)) b++; x=(v>>s)&m; return b>=8?(x>>(b-8)):((x<<(8-b))|(x>>(2*b>8?2*b-8:0)));}
static DWORD read_px(IDirectDrawSurface7 *t,int x,int y) {
 DDSURFACEDESC2 s; DWORD v=0; const DDPIXELFORMAT *f=&s.ddpfPixelFormat;
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s);
 if(FAILED(IDirectDrawSurface7_Lock(t,NULL,&s,DDLOCK_WAIT|DDLOCK_READONLY,NULL))) return 0xFFFFFFFFUL;
 memcpy(&v,(BYTE*)s.lpSurface+y*s.lPitch+x*(f->dwRGBBitCount/8),f->dwRGBBitCount/8);
 IDirectDrawSurface7_Unlock(t,NULL);
 return (chan(v,f->dwRBitMask)<<16)|(chan(v,f->dwGBitMask)<<8)|chan(v,f->dwBBitMask);
}
static int near_px(DWORD a,DWORD b,int tol) {int k; for(k=0;k<24;k+=8) {int d=(int)((a>>k)&255)-(int)((b>>k)&255); if(d<-tol||d>tol) return 0;} return 1;}
static DWORD st_draw(IDirect3DDevice7 *d,IDirectDrawSurface7 *t,const DWORD *ts0,int st1,int blend) {
 tlv v[4]; int k;
 static const float px[4]={0,64,0,64},py[4]={0,0,64,64};
 for(k=0;k<4;k++) {v[k].x=px[k]; v[k].y=py[k]; v[k].z=0.5f; v[k].rhw=1; v[k].c=0xC0204080UL; v[k].s=0;}
 IDirect3DDevice7_Clear(d,0,NULL,D3DCLEAR_TARGET,0xFF808080UL,1,0);
 IDirect3DDevice7_SetTexture(d,0,NULL); IDirect3DDevice7_SetTexture(d,1,NULL);
 IDirect3DDevice7_SetRenderState(d,D3DRENDERSTATE_TEXTUREFACTOR,0x40C08020UL);
 IDirect3DDevice7_SetRenderState(d,D3DRENDERSTATE_ALPHABLENDENABLE,blend);
 IDirect3DDevice7_SetRenderState(d,D3DRENDERSTATE_SRCBLEND,D3DBLEND_SRCALPHA); IDirect3DDevice7_SetRenderState(d,D3DRENDERSTATE_DESTBLEND,D3DBLEND_INVSRCALPHA);
 IDirect3DDevice7_SetTextureStageState(d,0,D3DTSS_COLOROP,ts0[0]); IDirect3DDevice7_SetTextureStageState(d,0,D3DTSS_COLORARG1,ts0[1]); IDirect3DDevice7_SetTextureStageState(d,0,D3DTSS_COLORARG2,ts0[2]);
 IDirect3DDevice7_SetTextureStageState(d,0,D3DTSS_ALPHAOP,ts0[3]); IDirect3DDevice7_SetTextureStageState(d,0,D3DTSS_ALPHAARG1,ts0[4]); IDirect3DDevice7_SetTextureStageState(d,0,D3DTSS_ALPHAARG2,D3DTA_DIFFUSE);
 if(st1) {IDirect3DDevice7_SetTextureStageState(d,1,D3DTSS_COLOROP,D3DTOP_MODULATE); IDirect3DDevice7_SetTextureStageState(d,1,D3DTSS_COLORARG1,D3DTA_CURRENT); IDirect3DDevice7_SetTextureStageState(d,1,D3DTSS_COLORARG2,D3DTA_TFACTOR);
  IDirect3DDevice7_SetTextureStageState(d,1,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1); IDirect3DDevice7_SetTextureStageState(d,1,D3DTSS_ALPHAARG1,D3DTA_CURRENT);}
 else {IDirect3DDevice7_SetTextureStageState(d,1,D3DTSS_COLOROP,D3DTOP_DISABLE); IDirect3DDevice7_SetTextureStageState(d,1,D3DTSS_ALPHAOP,D3DTOP_DISABLE);}
 if(FAILED(IDirect3DDevice7_BeginScene(d))) return 0xFFFFFFFEUL;
 IDirect3DDevice7_DrawPrimitive(d,D3DPT_TRIANGLESTRIP,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_SPECULAR,v,4,0);
 IDirect3DDevice7_EndScene(d);
 return read_px(t,32,32);
}
static void cmd_stages(void) {
 static const DWORD cop[5]={D3DTOP_SELECTARG1,D3DTOP_SELECTARG2,D3DTOP_MODULATE,D3DTOP_ADD,D3DTOP_DISABLE},aop[3]={D3DTOP_SELECTARG1,D3DTOP_SELECTARG2,D3DTOP_MODULATE};
 static const char *copn[5]={"SELECTARG1","SELECTARG2","MODULATE","ADD","DISABLE"},*aopn[3]={"SELECTARG1","SELECTARG2","MODULATE"};
 static const DWORD a1[2]={D3DTA_TEXTURE,D3DTA_TFACTOR},a2[2]={D3DTA_DIFFUSE,D3DTA_TEXTURE}; static const char *a1n[2]={"TEXTURE","TFACTOR"},*a2n[2]={"DIFFUSE","TEXTURE"};
 HRESULT (WINAPI *create)(GUID *,void **,REFIID,IUnknown *); HMODULE ddl; IDirectDraw7 *dd=0; IDirect3D7 *d3=0; IDirectDrawSurface7 *th=0,*tr=0; IDirect3DDevice7 *dh=0,*dr=0;
 DDSURFACEDESC2 s; D3DVIEWPORT7 vp; int ci,c1,c2,ai,aa,st1,bl,tol,n=0,bad=0,blank=0,rgb_diffuse; HRESULT hr; static DWORD hres[5][2][2][3][2][2][2];
 out("== stages (review #196): stage 0 with no texture bound, the HAL against Microsoft's RGB device\n");
 ddl=LoadLibraryA("DDRAW.DLL"); create=ddl?(HRESULT (WINAPI *)(GUID *,void **,REFIID,IUnknown *))GetProcAddress(ddl,"DirectDrawCreateEx"):0;
 if(!create||FAILED(create(NULL,(void**)&dd,&iid_dd7b,NULL))) {out("   DirectDrawCreateEx failed\n"); return;}
 IDirectDraw7_SetCooperativeLevel(dd,NULL,DDSCL_NORMAL);
 if(FAILED(hr=IDirectDraw7_QueryInterface(dd,&iid_d3d7,(void**)&d3))) {out("   IDirect3D7: %08lX\n",hr); goto done;}
 memset(&s,0,sizeof(s)); s.dwSize=sizeof(s); s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT; s.dwWidth=64; s.dwHeight=64;
 s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE|DDSCAPS_VIDEOMEMORY;
 if(FAILED(hr=IDirectDraw7_CreateSurface(dd,&s,&th,NULL))) {out("   HAL target: %08lX\n",hr); goto done;}
 s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_3DDEVICE|DDSCAPS_SYSTEMMEMORY;
 if(FAILED(hr=IDirectDraw7_CreateSurface(dd,&s,&tr,NULL))) {out("   RGB target: %08lX\n",hr); goto done;}
 if(FAILED(hr=IDirect3D7_CreateDevice(d3,&iid_hal,th,&dh))) {out("   HAL device: %08lX\n",hr); goto done;}
 if(FAILED(hr=IDirect3D7_CreateDevice(d3,&iid_rgb,tr,&dr))) {out("   RGB device: %08lX\n",hr); goto done;}
 memset(&vp,0,sizeof(vp)); vp.dwWidth=64; vp.dwHeight=64; vp.dvMaxZ=1; IDirect3DDevice7_SetViewport(dh,&vp); IDirect3DDevice7_SetViewport(dr,&vp);
 {IDirect3DDevice7 *dv[2]; int k; dv[0]=dh; dv[1]=dr; for(k=0;k<2;k++) {IDirect3DDevice7_SetRenderState(dv[k],D3DRENDERSTATE_ZENABLE,FALSE); IDirect3DDevice7_SetRenderState(dv[k],D3DRENDERSTATE_LIGHTING,FALSE);
  IDirect3DDevice7_SetRenderState(dv[k],D3DRENDERSTATE_CULLMODE,D3DCULL_NONE); IDirect3DDevice7_SetRenderState(dv[k],D3DRENDERSTATE_DITHERENABLE,FALSE); IDirect3DDevice7_SetRenderState(dv[k],D3DRENDERSTATE_SPECULARENABLE,FALSE);}}
 tol=start_mode.dmBitsPerPel<=16?10:4;
 out("   diffuse C0204080, TFACTOR 40C08020, background 808080; blend 0 = the colour, 1 = SRCALPHA blend (shows the alpha); tolerance %d a channel\n",tol);
 /* v1.2: the HAL's results kept (hres), so an argument the operation does not read can be checked not to change them (review #196: the
  * RGB device draws the diffuse colour whenever no texture is bound, whatever the stages say, so it is no reference for this) */
 memset(hres,0xFF,sizeof(hres)); rgb_diffuse=1;
 for(ci=0;ci<5;ci++) for(c1=0;c1<2;c1++) for(c2=0;c2<2;c2++) for(ai=0;ai<3;ai++) for(aa=0;aa<2;aa++) for(st1=0;st1<2;st1++) for(bl=0;bl<2;bl++) {
  DWORD ts[5],ph,pr;
  ts[0]=cop[ci]; ts[1]=a1[c1]; ts[2]=a2[c2]; ts[3]=ci==4?D3DTOP_DISABLE:aop[ai]; ts[4]=a1[aa];
  if(ci==4&&(c1||c2||ai||aa)) continue; /* DISABLE: the arguments do not matter */
  ph=st_draw(dh,th,ts,st1,bl); pr=st_draw(dr,tr,ts,st1,bl); n++; hres[ci][c1][c2][ai][aa][st1][bl]=ph;
  if(!near_px(pr,bl?0x374F7FUL:0x204080UL,tol)) rgb_diffuse=0;
  if(near_px(ph,0x808080UL,tol)&&!near_px(pr,0x808080UL,tol)) blank++;
  if(!near_px(ph,pr,tol)) {bad++; if(bad<=8) out("   DIFFERS: COLOROP %s ARG1 %s ARG2 %s, ALPHAOP %s ARG1 %s, stage 1 %s, blend %d: HAL %06lX, RGB device %06lX%s\n",copn[ci],a1n[c1],a2n[c2],ci==4?"DISABLE":aopn[ai],a1n[aa],
   st1?"MODULATE TFACTOR":"off",bl,ph,pr,near_px(ph,0x808080UL,tol)?" (HAL: nothing drawn)":"");}
 }
 out("   stages: %d cases, %d differ from the RGB device (%d where the HAL drew nothing; the first 8 listed)\n",n,bad,blank);
 if(rgb_diffuse) out("   the RGB device drew the diffuse colour in every case: with no texture bound it leaves the stages out, so it is no reference here\n");
 {int un=0,ub=0; /* an unread argument must not change the result: SELECTARG1 does not read ARG2, SELECTARG2 not ARG1; the alpha the same */
  for(ai=0;ai<3;ai++) for(aa=0;aa<2;aa++) for(st1=0;st1<2;st1++) for(bl=0;bl<2;bl++) {
   DWORD x,y;
   for(c1=0;c1<2;c1++) {x=hres[0][c1][0][ai][aa][st1][bl]; y=hres[0][c1][1][ai][aa][st1][bl]; un++;
    if(!near_px(x,y,tol)) {ub++; out("   UNREAD ARG2 CHANGES IT: COLOROP SELECTARG1 ARG1 %s, ALPHAOP %s ARG1 %s, stage 1 %s, blend %d: ARG2 DIFFUSE %06lX, ARG2 TEXTURE %06lX\n",a1n[c1],aopn[ai],a1n[aa],st1?"MODULATE TFACTOR":"off",bl,x,y);}}
   for(c2=0;c2<2;c2++) {x=hres[1][0][c2][ai][aa][st1][bl]; y=hres[1][1][c2][ai][aa][st1][bl]; un++;
    if(!near_px(x,y,tol)) {ub++; out("   UNREAD ARG1 CHANGES IT: COLOROP SELECTARG2 ARG2 %s, ALPHAOP %s ARG1 %s, stage 1 %s, blend %d: ARG1 TEXTURE %06lX, ARG1 TFACTOR %06lX\n",a2n[c2],aopn[ai],a1n[aa],st1?"MODULATE TFACTOR":"off",bl,x,y);}}
  }
  for(ci=0;ci<4;ci++) for(c1=0;c1<2;c1++) for(c2=0;c2<2;c2++) for(st1=0;st1<2;st1++) for(bl=0;bl<2;bl++) {
   DWORD x=hres[ci][c1][c2][1][0][st1][bl],y=hres[ci][c1][c2][1][1][st1][bl]; un++; /* ALPHAOP SELECTARG2: ALPHAARG1 unread */
   if(!near_px(x,y,tol)) {ub++; out("   UNREAD ALPHAARG1 CHANGES IT: COLOROP %s ARG1 %s ARG2 %s, ALPHAOP SELECTARG2, stage 1 %s, blend %d: ALPHAARG1 TEXTURE %06lX, TFACTOR %06lX\n",copn[ci],a1n[c1],a2n[c2],st1?"MODULATE TFACTOR":"off",bl,x,y);}
  }
  out("   unread arguments (review #196): %d pairs checked, %d changed the HAL's result%s\n",un,ub,ub?" (WRONG)":"");}
done:
 if(dh) IDirect3DDevice7_Release(dh); if(dr) IDirect3DDevice7_Release(dr); if(th) IDirectDrawSurface7_Release(th); if(tr) IDirectDrawSurface7_Release(tr);
 if(d3) IDirect3D7_Release(d3); if(dd) IDirectDraw7_Release(dd);
}

/* ================= fpcache (review #22) ================= */
#define GL_FRAGMENT_PROGRAM_ARB 0x8804
#define GL_PROGRAM_FORMAT_ASCII_ARB 0x8875
#define GL_PROGRAM_ERROR_POSITION_ARB 0x864B
typedef void (APIENTRY *gen_fn)(GLsizei,GLuint *); typedef void (APIENTRY *bind_fn)(GLenum,GLuint); typedef void (APIENTRY *str_fn)(GLenum,GLenum,GLsizei,const void *);
typedef void (APIENTRY *loc_fn)(GLenum,GLuint,GLfloat,GLfloat,GLfloat,GLfloat);
static LRESULT CALLBACK gl_proc(HWND w,UINT m,WPARAM a,LPARAM b) {return DefWindowProc(w,m,a,b);}
static void cmd_fpcache(void) {
 static const char *src[4]={
  "!!ARBfp1.0\nMOV result.color, fragment.color;\nEND\n",
  "!!ARBfp1.0\nTEMP t;\nTEX t, fragment.texcoord[0], texture[0], 2D;\nMUL result.color, t, fragment.color;\nEND\n",
  "!!ARBfp1.0\nTEMP t;\nTEX t, fragment.texcoord[0], texture[0], 2D;\nADD result.color, t, fragment.color;\nEND\n",
  "!!ARBfp1.0\nPARAM c = program.local[0];\nTEMP t;\nTEX t, fragment.texcoord[0], texture[0], 2D;\nLRP result.color, c, t, fragment.color;\nEND\n"};
 static const char *sc[5]={"one program","two programs alternating","four programs alternating","one program, its local parameter changed every quad","no program (fixed function, the reference)"};
 WNDCLASSA wc; HWND w; HDC dc; HGLRC rc; PIXELFORMATDESCRIPTOR pfd; int pf,k,f,q; GLuint prog[4],tex; gen_fn gen; bind_fn bind; str_fn str; loc_fn loc;
 uint32_t (__stdcall *stats)(uint32_t *,uint32_t); uint32_t a[8],b[8]; DWORD t0,t1; const char *ext; static BYTE texel[8*8*4];
 out("== fpcache (review #22): 200 frames of 64 quads each, glFinish every frame\n");
 memset(&wc,0,sizeof(wc)); wc.style=CS_OWNDC; wc.lpfnWndProc=gl_proc; wc.hInstance=GetModuleHandleA(0); wc.lpszClassName="VXPROBEGL"; RegisterClassA(&wc);
 w=CreateWindowA("VXPROBEGL","VXPROBE fpcache",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,256+8,256+28,NULL,NULL,wc.hInstance,NULL); dc=GetDC(w);
 memset(&pfd,0,sizeof(pfd)); pfd.nSize=sizeof(pfd); pfd.nVersion=1; pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER; pfd.iPixelType=PFD_TYPE_RGBA; pfd.cColorBits=(BYTE)start_mode.dmBitsPerPel; pfd.cDepthBits=16;
 pf=ChoosePixelFormat(dc,&pfd); if(!pf||!SetPixelFormat(dc,pf,&pfd)||!(rc=wglCreateContext(dc))||!wglMakeCurrent(dc,rc)) {out("   no OpenGL context\n"); DestroyWindow(w); return;}
 out("   GL_RENDERER %s, GL_VERSION %s\n",(const char*)glGetString(GL_RENDERER),(const char*)glGetString(GL_VERSION));
 ext=(const char*)glGetString(GL_EXTENSIONS);
 gen=(gen_fn)wglGetProcAddress("glGenProgramsARB"); bind=(bind_fn)wglGetProcAddress("glBindProgramARB"); str=(str_fn)wglGetProcAddress("glProgramStringARB"); loc=(loc_fn)wglGetProcAddress("glProgramLocalParameter4fARB");
 stats=(uint32_t (__stdcall *)(uint32_t *,uint32_t))GetProcAddress(GetModuleHandleA("VXGL.DLL"),"VXGL_FpStats");
 if(!ext||!strstr(ext,"GL_ARB_fragment_program")||!gen||!bind||!str||!loc) {out("   GL_ARB_fragment_program not offered\n"); goto fin;}
 if(!stats) out("   VXGL.DLL has no VXGL_FpStats (older than v10.333): frame times only\n");
 for(k=0;k<8*8*4;k++) texel[k]=(BYTE)((k&4)?200:60);
 glGenTextures(1,&tex); glBindTexture(GL_TEXTURE_2D,tex); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST); glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,texel); glEnable(GL_TEXTURE_2D);
 gen(4,prog);
 for(k=0;k<4;k++) {GLint pos=-1; bind(GL_FRAGMENT_PROGRAM_ARB,prog[k]); str(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(src[k]),src[k]); glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB,&pos);
  if(pos!=-1) out("   program %d: error at %d\n",k,pos);}
 glViewport(0,0,256,256); glMatrixMode(GL_PROJECTION); glLoadIdentity(); glOrtho(0,256,0,256,-1,1); glMatrixMode(GL_MODELVIEW); glLoadIdentity();
 for(q=0;q<5;q++) {
  if(stats) stats(a,2);
  if(q<4) glEnable(GL_FRAGMENT_PROGRAM_ARB); else glDisable(GL_FRAGMENT_PROGRAM_ARB);
  bind(GL_FRAGMENT_PROGRAM_ARB,prog[q==3?3:1]);
  t0=GetTickCount();
  for(f=0;f<200;f++) {
   glClearColor(0.1f,0.1f,0.2f,1); glClear(GL_COLOR_BUFFER_BIT);
   for(k=0;k<64;k++) {float x=(float)((k&7)*32),y=(float)((k>>3)*32);
    if(q==1) bind(GL_FRAGMENT_PROGRAM_ARB,prog[1+(k&1)]); else if(q==2) bind(GL_FRAGMENT_PROGRAM_ARB,prog[k&3]); else if(q==3) loc(GL_FRAGMENT_PROGRAM_ARB,0,(k&7)/7.0f,0.5f,0.5f,0.5f);
    glBegin(GL_QUADS); glColor4f(0.8f,0.4f,0.2f,1); glTexCoord2f(0,0); glVertex2f(x,y); glTexCoord2f(1,0); glVertex2f(x+30,y); glTexCoord2f(1,1); glVertex2f(x+30,y+30); glTexCoord2f(0,1); glVertex2f(x,y+30); glEnd();}
   glFinish(); SwapBuffers(dc);
  }
  t1=GetTickCount();
  out("   %s: %.2f ms a frame",sc[q],(t1-t0)/200.0);
  if(stats) {stats(b,0); out("; translations asked %lu, memo hits %lu, misses %lu, to software %lu; misses cost %lu us (%.1f us each, longest %lu us), hits %lu us",
   (unsigned long)b[0],(unsigned long)b[1],(unsigned long)b[2],(unsigned long)b[3],(unsigned long)b[4],b[2]?(double)b[4]/b[2]:0.0,(unsigned long)b[5],(unsigned long)b[6]);}
  out("\n");
 }
 glDisable(GL_FRAGMENT_PROGRAM_ARB);
fin:
 wglMakeCurrent(NULL,NULL); wglDeleteContext(rc); ReleaseDC(w,dc); DestroyWindow(w);
}

static void run_one(const char *c) {
 if(!strcmp(c,"cursor")) cmd_cursor();
 else if(!strcmp(c,"bios")) cmd_bios();
 else if(!strcmp(c,"modeset")) cmd_modeset();
 else if(!strcmp(c,"edid")) cmd_edid();
 else if(!strcmp(c,"ovkey")) cmd_ovkey();
 else if(!strcmp(c,"stages")) cmd_stages();
 else cmd_fpcache();
}
/* v1.1: no command (started from Explorer, v1.0 printed its usage and closed) runs all six, the ones that need nothing from you first;
 * the log is next to the program whatever the current folder is, and a message box says when it is done */
int main(int argc,char **argv) {
 static const char *all[7]={"edid","stages","fpcache","ovkey","cursor","modeset","bios"};
 SYSTEMTIME t; const char *c=argc>1?argv[1]:"all"; char path[MAX_PATH],*e; int k;
 if(strcmp(c,"all")&&strcmp(c,"bios")&&strcmp(c,"cursor")&&strcmp(c,"modeset")&&strcmp(c,"edid")&&strcmp(c,"ovkey")&&strcmp(c,"stages")&&strcmp(c,"fpcache")) {
  MessageBoxA(NULL,"VXPROBE [cursor | modeset | bios | edid | ovkey | stages | fpcache]\nNo command runs all seven. Results: VXPROBE.TXT next to the program.","VXPROBE",MB_OK|MB_TOPMOST); return 1;
 }
 GetModuleFileNameA(NULL,path,sizeof(path)); e=strrchr(path,'\\'); if(e) strcpy(e+1,"VXPROBE.TXT"); else strcpy(path,"VXPROBE.TXT");
 lf=fopen(path,"a"); GetLocalTime(&t);
 memset(&start_mode,0,sizeof(start_mode)); start_mode.dmSize=sizeof(start_mode); EnumDisplaySettings(NULL,ENUM_CURRENT_SETTINGS,&start_mode);
 out("\n==== VXPROBE v1.5 %s %04u-%02u-%02u %02u:%02u:%02u; %lux%lux%lu ====\n",c,t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,start_mode.dmPelsWidth,start_mode.dmPelsHeight,start_mode.dmBitsPerPel);
 dev=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) out("Cannot open the VX85598 driver: cursor, modeset and edid need it.\n");
 if(!lf) MessageBoxA(NULL,"VXPROBE cannot write VXPROBE.TXT next to the program (a read-only folder?): the results go to the console only.","VXPROBE",MB_OK|MB_TOPMOST);
 if(!strcmp(c,"all")) {for(k=0;k<7;k++) if(dev!=INVALID_HANDLE_VALUE||(k>=1&&k<=3)) run_one(all[k]);}
 else if(dev!=INVALID_HANDLE_VALUE||!strcmp(c,"stages")||!strcmp(c,"fpcache")||!strcmp(c,"ovkey")) run_one(c);
 if(dev!=INVALID_HANDLE_VALUE) CloseHandle(dev);
 out("==== done ====\n");
 if(lf) fclose(lf);
 MessageBoxA(NULL,lf?"VXPROBE is done: the results are in VXPROBE.TXT next to the program.":"VXPROBE is done.","VXPROBE",MB_OK|MB_TOPMOST);
 return 0;
}
