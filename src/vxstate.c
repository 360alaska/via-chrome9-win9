/* VXSTATE v1.1 (driver v10.332; v1.1: the MMIO dwords 0x000-0x3FC and 0x1200-0x12FC with a v10.332 VxD): read-only snapshot of the driver in whatever state it is in now (run it while the screen is wrong,
 * before anything else, before a restart). It draws nothing, does not move the pointer, does not touch the 3D engine or the cursor:
 * - the driver status, the 2D engine's state (GDI binding, recoveries, loss), the hardware cursor's counters and image block;
 * - the display unit's registers 0x200-0x2FC (the hardware cursor at 0x2D0-0x2E0 and 0x2E4-0x2FC, the overlay) as they read now;
 * - the 3D engine's state (VX_IOCTL_3DN_INFO), the last group that did not finish, the loss record;
 * - every sequencer and CRTC register (the display start address and pitch among them);
 * - the screen as GDI reads it, in VXSTATE.BMP: when the band is in that picture it is in video memory; when it is only on the monitor,
 *   the display unit shows something other than the frame buffer;
 * - last, the engine recovery counts (TEXDBG 70, which first retires a draw still in flight).
 * Writes VXSTATE.TXT (appended) and VXSTATE.BMP next to the program. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "vx85598.h"
#include "vx_native3d.h"

static FILE *lf;
static void out(const char *fmt,...) {
 char t[1200]; va_list ap; va_start(ap,fmt); _vsnprintf(t,sizeof(t)-1,fmt,ap); va_end(ap); t[sizeof(t)-1]=0;
 fputs(t,stdout); if(lf) {fputs(t,lf); fflush(lf);}
}
static HANDLE dev;
static int ctl(DWORD code,DWORD *o) {DWORD got=0; memset(o,0,16); return DeviceIoControl(dev,VX_IOCTL_2D_CTRL,&code,4,o,16,&got,NULL)&&got==16;}
static int texdbg(DWORD code,DWORD *o) {DWORD got=0; memset(o,0,32); return DeviceIoControl(dev,VX_IOCTL_3DN_TEXDBG,&code,4,o,32,&got,NULL)&&got==32;}
static void shot(void) {
 HDC s=GetDC(NULL),m; HBITMAP b,old; BITMAPINFO bi; void *bits=NULL; int w=GetSystemMetrics(SM_CXSCREEN),h=GetSystemMetrics(SM_CYSCREEN); FILE *f; BITMAPFILEHEADER fh; DWORD sz;
 if(!s) {out("screen: no screen DC\n"); return;}
 memset(&bi,0,sizeof(bi)); bi.bmiHeader.biSize=sizeof(bi.bmiHeader); bi.bmiHeader.biWidth=w; bi.bmiHeader.biHeight=h; bi.bmiHeader.biPlanes=1; bi.bmiHeader.biBitCount=24; bi.bmiHeader.biCompression=BI_RGB;
 m=CreateCompatibleDC(s); b=CreateDIBSection(s,&bi,DIB_RGB_COLORS,&bits,NULL,0);
 if(!m||!b||!bits) {out("screen: no memory for the picture\n"); if(b) DeleteObject(b); if(m) DeleteDC(m); ReleaseDC(NULL,s); return;}
 old=(HBITMAP)SelectObject(m,b);
 if(!BitBlt(m,0,0,w,h,s,0,0,SRCCOPY)) out("screen: BitBlt from the screen failed (error %lu)\n",GetLastError());
 GdiFlush(); SelectObject(m,old);
 sz=(DWORD)(((w*3+3)&~3)*h);
 memset(&fh,0,sizeof(fh)); fh.bfType=0x4D42; fh.bfOffBits=sizeof(fh)+sizeof(bi.bmiHeader); fh.bfSize=fh.bfOffBits+sz;
 f=fopen("VXSTATE.BMP","wb"); if(f) {fwrite(&fh,sizeof(fh),1,f); fwrite(&bi.bmiHeader,sizeof(bi.bmiHeader),1,f); fwrite(bits,1,sz,f); fclose(f); out("screen: %d x %d as GDI reads it in VXSTATE.BMP\n",w,h);}
 else out("screen: VXSTATE.BMP not written\n");
 DeleteObject(b); DeleteDC(m); ReleaseDC(NULL,s);
}
int main(void) {
 DWORD got,o[8],i,k; vx_status_t st; vx3dn_info in; static vx_crt_report crt; SYSTEMTIME t;
 lf=fopen("VXSTATE.TXT","a"); GetLocalTime(&t);
 out("\n==== VXSTATE v1.1 (read-only) %04u-%02u-%02u %02u:%02u:%02u ====\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
 {HDC s=GetDC(NULL); out("Windows: %d x %d, %d bpp\n",GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),s?GetDeviceCaps(s,BITSPIXEL):0); if(s) ReleaseDC(NULL,s);}
 dev=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE) {out("Cannot open the VX85598 driver.\n"); shot(); if(lf) fclose(lf); return 1;}
 memset(&st,0,sizeof(st)); got=0;
 if(DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,&st,sizeof(st),&got,NULL)) {
  out("status: version %08lX stage %lu failure %lu; %lu x %lu x %lu, pitch %lu; mode sets %lu of %lu; vga_mode %lu; fb %08lX (%lu MB) linear %08lX\n",
   (unsigned long)st.version,(unsigned long)st.stage,(unsigned long)st.failure,(unsigned long)st.width,(unsigned long)st.height,(unsigned long)st.bpp,(unsigned long)st.video_pitch,
   (unsigned long)st.set_successes,(unsigned long)st.set_attempts,(unsigned long)st.vga_mode,(unsigned long)st.fb_phys,(unsigned long)(st.fb_bytes>>20),(unsigned long)st.fb_linear);
 } else out("status: request failed (%lu)\n",GetLastError());
 if(ctl(0xFFFFFFFEUL,o)) out("2D: ops %lu, handed back %lu, state %lu, flags %08lX\n",o[0],o[1],o[2],o[3]);
 if(ctl(0xFFFFFE10UL,o)) out("2D GDI binding: %lu (1 bound, 2 enabled word reads 0, 4 BUSY), GDI requests while disabled %lu, state %lu\n",o[0],o[1],o[2]);
 if(ctl(0xFFFFFE11UL,o)) out("2D recoveries %lu, retries %lu, state %lu, GDI fenced %lu\n",o[0],o[1],o[2],o[3]);
 if(ctl(0xFFFFFE1CUL,o)) out("2D binding moves %lu, maps %lu, map failures %lu\n",o[0],o[1],o[2]);
 if(ctl(0xFFFFFE12UL,o)) out("2D async: started %lu, waits %lu, pending %lu, switches %lu\n",o[0],o[1],o[2],o[3]);
 if(ctl(0xFFFFFE15UL,o)) out("hardware cursor: mode %lu, shown %lu times, shapes to software %lu, image block at %08lX\n",o[0],o[1],o[2],o[3]);
 if(ctl(0xFFFFFE16UL,o)) out("hardware cursor: moves %lu, shapes %lu, hides %lu, on %lu\n",o[0],o[1],o[2],o[3]);
 if(ctl(0xFFFFFE1EUL,o)) out("cursor resets seen %lu (engine resets), shapes %lu\n",o[0],o[1]);
 if(ctl(0xFFFFFE1DUL,o)&&(o[0]&0x400)) {
  out("cursor check on: flags %08lX, image dwords differing %lu (first %08lX), registers read other than written %06lX of %lu\n",o[0],o[1],o[2],o[3]&0xFFFFFFUL,o[3]>>24);
  for(i=0;i<(o[3]>>24)&&i<16;i++) {DWORD r[4]; if(ctl(0xFFFFFE70UL+i,r)) out("   reg %03lX written %08lX reads %08lX\n",r[0],r[1],r[2]);}
 } else out("cursor check off (ACCEL_CURSOR_CHECK 0): the written values are not kept; the registers as they read are below\n");
 out("display registers 200-2FC:\n");
 for(k=0;k<16;k++) if(ctl(0xFFFFFE60UL+k,o)) out("  %03lX: %08lX %08lX %08lX %08lX\n",0x200+k*16,o[0],o[1],o[2],o[3]);
 {DWORD base,b;int ok=1; /* v1.1 */
  for(b=0;b<2&&ok;b++){out("MMIO %04lX-%04lX:\n",b?0x1200UL:0UL,b?0x12FCUL:0x3FCUL);
   for(k=0;k<64;k++){base=(b?0x1200UL:0UL)+k*16; if(!ctl((b?0xFFFFFEC0UL:0xFFFFFE80UL)+k,o)||(k==0&&!b&&o[0]==0&&o[1]==0&&o[2]==0&&o[3]==0&&0)){ok=0;break;} out("  %04lX: %08lX %08lX %08lX %08lX\n",base,o[0],o[1],o[2],o[3]);}}
  if(!ok) out("MMIO dump: the VxD does not answer (older than v10.332)\n");}
 memset(&in,0,sizeof(in)); got=0;
 if(DeviceIoControl(dev,VX_IOCTL_3DN_INFO,NULL,0,&in,sizeof(in),&got,NULL)&&got>=sizeof(in))
  out("3D: ready %lu, generation %lu, fault %lu, submitted %lu, triangles %lu, clears %lu, last status %08lX, last result %lu\n",
   (unsigned long)in.ready,(unsigned long)in.generation,(unsigned long)in.fault,(unsigned long)in.submitted,(unsigned long)in.triangles,(unsigned long)in.clears,(unsigned long)in.last_status,(unsigned long)in.last_result);
 if(texdbg(0xA5,o)&&o[0]==0xA5) out("loss: 2D state %lu, native fault %lu, LOST answers %lu, lost at generation %lu (now %lu), failed recoveries %lu\n",o[1],o[2],o[3],o[4],o[5],o[6]);
 if(texdbg(0xE0,o)&&o[0]==0xE0) out("groups that did not finish: %lu; the last: status %08lX, %lu packets\n",o[1],o[2],o[3]&0xFFFF);
 memset(&crt,0,sizeof(crt)); got=0;
 if(DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,&crt,sizeof(crt),&got,NULL)&&got==sizeof(crt)) {
  #define CR(n) ((unsigned long)crt.cr[(n)]&0xFF)
  out("display start (CR0C/0D/34/48) %02lX %02lX %02lX %02lX, offset CR13 %02lX CR35 %02lX\n",CR(0x0C),CR(0x0D),CR(0x34),CR(0x48),CR(0x13),CR(0x35));
  out("SR00..FF:\n"); for(i=0;i<256;i+=16) {out("  %02lX:",i); for(k=0;k<16;k++) out(" %02lX",(unsigned long)crt.sr[i+k]&0xFF); out("\n");}
  out("CR00..FF:\n"); for(i=0;i<256;i+=16) {out("  %02lX:",i); for(k=0;k<16;k++) out(" %02lX",CR(i+k)); out("\n");}
 } else out("CRT dump: request failed\n");
 shot();
 if(texdbg(0x70,o)&&o[0]==0x70) out("3D engine recoveries %lu, failed %lu, status before the last reset %08lX, after %08lX, step %lu\n",o[1],o[2],o[3],o[4],o[5]);
 {static char log[VX_LOG_CAPACITY+1]; got=0; if(DeviceIoControl(dev,VX_IOCTL_LOG,NULL,0,log,VX_LOG_CAPACITY,&got,NULL)&&got>1) {log[got<VX_LOG_CAPACITY?got:VX_LOG_CAPACITY]=0; out("---- VxD log (%lu bytes) ----\n%s\n",got,log);} else out("VxD log: empty (ACCEL_DEBUG_LOG 0)\n");}
 CloseHandle(dev);
 out("Done. Send VXSTATE.TXT and VXSTATE.BMP.\n");
 if(lf) fclose(lf);
 return 0;
}
