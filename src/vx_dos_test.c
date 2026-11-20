/* VXDOS v1.1 (driver .673: the BIOS's misc output kept at boot, and how often the driver set its clock select for a DOS session). VXDOS v1.0: the full-screen DOS session tester (Windows half; VXDOSD.EXE is the DOS half). For a display that shows black or garbage
 * when an MS-DOS Prompt goes full screen (Alt+Enter), as on a laptop whose panel the chip's second display unit (IGA2) drives.
 * 1. The desktop: the driver's state (mode, timing, the last mode sets) and every sequencer and CRTC register (SR00-FF, CR00-FF).
 * 2. It opens an MS-DOS Prompt in its own folder and watches the driver's switch to the DOS session and back. Two seconds into the
 *    full-screen session the driver reads the registers again (what the hardware really holds; the DOS side may be shown the VDD's copy).
 *    In the session: Alt+Enter, type VXDOSD and Enter (blind if need be), answer its beeps with Y / N, type EXIT and Enter.
 * 3. The desktop again, the registers that differ from step 1, and four questions about what was seen.
 * Then VXDOSD.TXT (and VXDOSR.TXT, VXDOSD from plain DOS, if it is there) are added. Log: VXDOS.TXT beside the program.
 * Works with any driver build from .600 on (only reads; nothing is changed). Copyright (c) 2026 360ALASKA. MIT; see LICENSE. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "vx85598.h"

static FILE *lf;static HANDLE dev=INVALID_HANDLE_VALUE;static char dir[MAX_PATH];
static void say(const char *f,...) {static char b[2048];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);if(lf){fputs(b,lf);fflush(lf);}}
static int status(vx_status_t *s) {DWORD got=0;memset(s,0,sizeof(*s));return DeviceIoControl(dev,VX_IOCTL_STATUS,NULL,0,s,sizeof(*s),&got,NULL)&&got>=sizeof(*s);}
static int probe(DWORD sub,DWORD *o) {DWORD in[2],got=0;in[0]=sub;in[1]=0;memset(o,0,256);return DeviceIoControl(dev,VX_IOCTL_PROBE,in,4,o,256,&got,NULL)&&o[0]==VX_MAGIC&&o[1]==sub;}
static int crt(vx_crt_report *c) {DWORD got=0;memset(c,0,sizeof(*c));return DeviceIoControl(dev,VX_IOCTL_CRT,NULL,0,c,sizeof(*c),&got,NULL)&&got==sizeof(*c);}
static void crt_say(const vx_crt_report *c) {int i;
 say("   misc output %02lX, CRTC at %04lX, SR10 was %02lX\n",c->misc,c->crtc_base,c->sr10_found);
 for(i=0;i<256;i++){if(!(i&15))say("      SR%02X:",i);say(" %02lX",c->sr[i]&255);if((i&15)==15)say("\n");}
 for(i=0;i<256;i++){if(!(i&15))say("      CR%02X:",i);say(" %02lX",c->cr[i]&255);if((i&15)==15)say("\n");}}
static void crt_diff(const char *a,const vx_crt_report *x,const char *b,const vx_crt_report *y) {int i,n=0;
 say("   registers that differ (%s -> %s):",a,b);
 for(i=0;i<256;i++)if((x->sr[i]^y->sr[i])&255){say("%s SR%02X %02lX->%02lX",n%8?"":"\n     ",i,x->sr[i]&255,y->sr[i]&255);n++;}
 for(i=0;i<256;i++)if((x->cr[i]^y->cr[i])&255){say("%s CR%02X %02lX->%02lX",n%8?"":"\n     ",i,x->cr[i]&255,y->cr[i]&255);n++;}
 say("%s\n",n?"":" none");}
static void state(const char *when,vx_crt_report *c) {
 vx_status_t s;DWORD o[64];int k;
 say("== %s\n",when);
 if(status(&s))say("   driver: stage %lu, failure %lu, mode %lux%lux%lu (BIOS mode %lX), VGA mode (a DOS session or text mode owns the display) %lu, mode sets %lu of %lu, last BIOS AX %04lX\n",
  s.stage,s.failure,s.width,s.height,s.bpp,s.active_mode,s.vga_mode,s.set_successes,s.set_attempts,s.last_bios_ax);
 else say("   driver status: no answer\n");
 if(probe(1,o)){say("   timing: own %lu, fixed %lu, %lu Hz; the driver's mode %lu, VGA mode %lu; desktop %lux%lux%lu\n",o[15],o[16],o[17],o[24],o[25],o[29],o[30],o[31]);
  if(o[39]&0x100)say("   the BIOS's misc output at boot %02lX (clock select %lu); the driver set clock select 3 for a DOS session %lu time(s) (.673)\n",o[39]&255,(o[39]>>2)&3,o[39]>>16);
  else say("   the BIOS's misc output at boot: not kept (a driver before .673, or .673-.692 loaded dynamically: it missed it)\n");}
 if(probe(4,o)){say("   the last mode sets (%lu in all), 12 words each, oldest first:\n",o[2]);for(k=0;k<(int)o[63]&&k<5;k++){int j;say("     ");for(j=0;j<12;j++)say(" %08lX",o[3+k*12+j]);say("\n");}}
 if(crt(c))crt_say(c);else say("   register dump: no answer\n");
}
static int yes(const char *q) {int r=MessageBox(NULL,q,"VXDOS",MB_YESNO|MB_ICONQUESTION|MB_TOPMOST|MB_SETFOREGROUND);say("   %s %s\n",q,r==IDYES?"YES":"NO");return r==IDYES;}
static void add_file(const char *name) {char p[MAX_PATH];FILE *f;char b[512];
 sprintf(p,"%s%s",dir,name);f=fopen(p,"r");if(!f){say("== %s: not there\n",name);return;}
 say("== %s\n",name);while(fgets(b,sizeof(b),f))say("%s",b);fclose(f);}
static void pump(void) {MSG m;while(PeekMessage(&m,NULL,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessage(&m);}}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cl,int sh) {
 char p[MAX_PATH],cmd[MAX_PATH],*s;vx_crt_report before,during,after;vx_status_t st;STARTUPINFO si;PROCESS_INFORMATION pi;
 DWORD t0,tsw=0,last=0,now,o[8],in,got;int dumped=0,switches=0,ended=0,ok;
 (void)hi;(void)hp;(void)cl;(void)sh;
 GetModuleFileName(NULL,dir,sizeof(dir));s=strrchr(dir,'\\');if(s)s[1]=0;else dir[0]=0;
 sprintf(p,"%sVXDOS.TXT",dir);lf=fopen(p,"w");
 sprintf(p,"%sVXDOSD.TXT",dir);DeleteFile(p);
 say("VXDOS v1.1: the full-screen DOS session (Alt+Enter)\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE){say("the driver (VX85598) does not answer: is the VIA Chrome9 driver installed?\n");MessageBox(NULL,"The VIA Chrome9 driver does not answer. Nothing was tested.","VXDOS",MB_OK|MB_ICONSTOP);if(lf)fclose(lf);return 1;}
 in=0x75;memset(o,0,sizeof(o));got=0;DeviceIoControl(dev,0x85500022UL,&in,4,o,32,&got,0);say("the driver's build: .%lu, the device 1106:%04lX\n",o[1],o[2]);
 state("1. the desktop, before",&before);
 if(MessageBox(NULL,"VXDOS will open an MS-DOS Prompt. In it:\n\n"
  "1. Press Alt+Enter (full screen). Wait 5 seconds, even if the screen is black or garbled.\n"
  "2. Type VXDOSD and press Enter, even if you cannot see anything.\n"
  "3. It beeps. After each beep (1, then 2, then 3 beeps) press Y if you can see what it asks about, N if not.\n"
  "    1 beep: can you read text?  2 beeps: can you read text?  3 beeps: black and white stripes?\n"
  "4. After the long beep type EXIT and press Enter.\n\nThen answer four questions here. Remember what the screen showed after Alt+Enter.",
  "VXDOS",MB_OKCANCEL|MB_ICONINFORMATION|MB_TOPMOST)!=IDOK){say("cancelled\n");if(lf)fclose(lf);return 0;}
 if(!GetEnvironmentVariable("COMSPEC",cmd,sizeof(cmd)))strcpy(cmd,"COMMAND.COM");
 memset(&si,0,sizeof(si));si.cb=sizeof(si);memset(&pi,0,sizeof(pi));
 if(!CreateProcess(NULL,cmd,NULL,NULL,FALSE,CREATE_NEW_CONSOLE,NULL,dir,&si,&pi)){say("the MS-DOS Prompt (%s) could not be started: error %lu\n",cmd,GetLastError());if(lf)fclose(lf);return 1;}
 say("== 2. the MS-DOS Prompt (%s) opened; the driver's switches:\n",cmd);
 t0=GetTickCount();
 while((now=GetTickCount())-t0<900000UL){
  pump();
  if(status(&st)){
   if(st.vga_mode!=last){say("   %6.1f s: VGA mode %lu -> %lu (%s)\n",(now-t0)/1000.0,last,st.vga_mode,st.vga_mode?"the DOS session took the display":"back to the desktop");last=st.vga_mode;tsw=now;switches++;}
   if(last&&!dumped&&now-tsw>=2000UL){dumped=1;state("2a. two seconds into the full-screen session (read by the driver)",&during);}}
  if(!ended&&WaitForSingleObject(pi.hProcess,0)==WAIT_OBJECT_0){ended=1;say("   %6.1f s: the MS-DOS Prompt ended\n",(now-t0)/1000.0);}
  if(ended&&!last&&now-tsw>=3000UL)break;
  Sleep(100);}
 if(!ended)say("   still open after 15 minutes: stopped watching\n");
 if(pi.hProcess)CloseHandle(pi.hProcess);if(pi.hThread)CloseHandle(pi.hThread);
 Sleep(1000);
 state("3. the desktop, after",&after);
 if(dumped){crt_diff("desktop before",&before,"in the session",&during);crt_diff("in the session",&during,"desktop after",&after);}
 else say("   the driver never saw the display go to the DOS session for 2 s (%d switches)\n",switches);
 crt_diff("desktop before",&before,"desktop after",&after);
 say("== 4. what was seen\n");
 ok=0;
 if(yes("After Alt+Enter (before typing VXDOSD), was the screen all black?"))ok|=1;
 if(yes("Was it garbled (wrong characters, colours, lines or a picture that did not belong)?"))ok|=2;
 if(yes("Could you read the DOS prompt normally?"))ok|=4;
 if(yes("Is the Windows desktop right again now?"))ok|=8;
 add_file("VXDOSD.TXT");add_file("VXDOSR.TXT");
 say("done\n");if(lf)fclose(lf);
 sprintf(p,"Done. Send the file VXDOS.TXT from\n%s",dir);MessageBox(NULL,p,"VXDOS",MB_OK|MB_ICONINFORMATION);
 return 0;
}
