/* VXIDLE v1.0 (driver .589): how soon after a 2D command is written the engine's status register shows it busy. The driver takes 32
 * idle status reads in a row after a command as "finished"; if the engine sometimes shows busy later than that, a wait ends while
 * the command is still to run, and whatever the driver or a program does next with that memory is overwritten by it (VX800 board:
 * a cleared front buffer that kept the window's picture, about one try in 20). The VxD runs 200 copies of 256 x 256 inside its
 * staging block (nothing on the screen), ten times, and counts the reads before the first busy one. Log: VXIDLE.LOG. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
static FILE *lf;
static void say(const char *f,...) {static char b[1024];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);if(lf){fputs(b,lf);fflush(lf);}}
static HANDLE dev;
static int ctl(DWORD v,DWORD *o) {DWORD got=0;o[0]=o[1]=o[2]=o[3]=0;return DeviceIoControl(dev,0x85500012UL,&v,4,o,16,&got,NULL)&&got==16;}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 DWORD o[4],a[4],b[4],c[4],tot[9]={0,0,0,0,0,0,0,0,0},never=0,maxfirst=0,maxdone=0,mindone=0xFFFFFFFFUL,runs=0;int r,k,late;(void)hi;(void)hp;(void)cmd;(void)show;
 lf=fopen("VXIDLE.LOG","w");
 say("VXIDLE v1.0 (driver .589): status reads between a 2D command and the engine showing busy\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE){say("FAILED: the VxD is not open\n");if(lf)fclose(lf);return 2;}
 for(r=0;r<10;r++){
  if(!ctl(0xFFFFFE40UL,o)||o[0]==0xFFFFFFFFUL){say("run %d: not run (the 2D engine is off or busy, the desktop is not 16 or 32 bits, or the VxD is older than .589)\n",r+1);continue;}
  ctl(0xFFFFFE41UL,a);ctl(0xFFFFFE42UL,b);ctl(0xFFFFFE43UL,c);
  say("run %2d: %lu commands; reads before busy: 0: %lu, 1: %lu, 2-3: %lu, 4-7: %lu, 8-15: %lu, 16-31: %lu, 32-63: %lu, 64-127: %lu, 128+: %lu; never busy: %lu; most reads before busy %lu; busy for %lu to %lu reads\n",
   r+1,o[0],a[0],a[1],a[2],a[3],b[0],b[1],b[2],b[3],c[0],o[1],o[2],c[1],o[3]);
  for(k=0;k<4;k++){tot[k]+=a[k];tot[4+k]+=b[k];}tot[8]+=c[0];never+=o[1];if(o[2]>maxfirst)maxfirst=o[2];if(o[3]>maxdone)maxdone=o[3];if(c[1]<mindone)mindone=c[1];runs++;
  Sleep(50);}
 if(!runs){say("NOT RUN\n");if(lf)fclose(lf);return 1;}
 late=(int)(tot[6]+tot[7]+tot[8]+never);
 say("in all: 0: %lu, 1: %lu, 2-3: %lu, 4-7: %lu, 8-15: %lu, 16-31: %lu, 32-63: %lu, 64-127: %lu, 128+: %lu, never: %lu; most reads before busy %lu; a copy keeps the engine busy for %lu to %lu reads\n",
  tot[0],tot[1],tot[2],tot[3],tot[4],tot[5],tot[6],tot[7],tot[8],never,maxfirst,mindone,maxdone);
 say("%s\n",late?"LATE: the engine showed busy only after 32 or more idle reads (or never) in some commands: the driver's wait of 32 idle reads can end before the command has run":"IN TIME: the engine showed busy within 32 reads every time: the driver's wait does not end early for this reason");
 if(lf)fclose(lf);CloseHandle(dev);return late?2:0;
}
