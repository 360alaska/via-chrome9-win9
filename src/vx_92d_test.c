/* VX92D v1.4 (driver .603: the quick idle check's counts are gone again with the VxD's counters: 0 of 88,550 fell through on the VX900). VX92D v1.3 (driver .602: step 7, the status register at rest and after a register write, a register read, a sequencer read and a flush, and how often the driver's quick idle check fell through to the full wait since Windows started: run it after other drawing tools for a count that means something). VX92D v1.2 (driver .601: the 2D engine and the 3D engine reported apart). VX92D v1.1 (driver .600: the VxD puts the sequencer's engine enables back as found; the tool is unchanged but for the build it asks for). VX92D v1.0 (driver .599): the VX900's 2D engine, before the driver uses it. Which register layout the engine decodes (the M1 layout
 * of the VX800 / VX855, which VIA's manual and OpenChrome give the VX900 too, or the older layout VIA's XP miniport writes for 7122),
 * whether a source-keyed copy needs 4000h or 6000h in the key control (48h), and how the status register (400h) behaves around a
 * command. It runs on any of the three chips, with the 2D engine in use or not: on a VX800 / VX855 it is the control (M1 must pass,
 * the older layout must not). The VxD draws in the first 16 rows of the screen, saved first and put back after; the timing step
 * inverts the whole screen twice (a short flash). Steps: 1 the M1 fill; 2 keyed copy with 4000h; 3 keyed copy with 6000h; 4 the
 * status around two whole-screen fills; 5 the older layout's fill; 6 the M1 fill again (the engine still answers after step 5).
 * Steps 2-4 use the M1 layout and are skipped when step 1 did not draw. Log: VX92D.LOG next to the program. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
static FILE *lf;static int wrong;
static void say(const char *f,...) {static char b[1400];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);if(lf){fputs(b,lf);fflush(lf);}}
static HANDLE dev;
static int ctl(DWORD v,DWORD *o) {DWORD got=0;o[0]=o[1]=o[2]=o[3]=0;return DeviceIoControl(dev,0x85500012UL,&v,4,o,16,&got,NULL)&&got==16;}
static int step(DWORD k,DWORD *r) {int i;for(i=0;i<16;i++)r[i]=0;if(!ctl(0xFFFFFE70UL+k,r)||r[0]==0xFFFFFFFFUL)return 0;ctl(0xFFFFFE7CUL,r+4);ctl(0xFFFFFE7DUL,r+8);ctl(0xFFFFFE7EUL,r+12);return 1;}
static const char *why(DWORD c) {return c==1?"no usable desktop (8, 16 or 32 bits, at least 64 x 16)":c==2?"the display is held or the 3D engine is busy":c==3?"the registers could not be opened (the sequencer's engine enables: see SR1A / SR1E / SR4E below)":c==4?"the engine was not idle before the command":"?";}
static void reads(char *t,DWORD v) {if(v==0xFFFFFFFFUL)strcpy(t,"never");else if(v==0xFFFFFFFEUL)strcpy(t,"NOT IN 400000 READS");else sprintf(t,"%lu",v);}
/* a fill step: 1 the rectangle is exactly there, 0 not, -1 not run */
static int fill(const char *name,DWORD k,DWORD *r) {
 char a[32],b[32];
 if(!step(k,r)){say("%s: no answer (the VxD is older than .600)\n",name);return -1;}
 if(r[0]&&r[0]!=5){say("%s: not run: %s; SR1A %02lX SR1E %02lX SR4E %02lX SR3C %02lX\n",name,why(r[0]),r[15]&255,(r[15]>>8)&255,(r[15]>>16)&255,r[15]>>24);return -1;}
 reads(a,r[6]);reads(b,r[3]);
 say("%s: %lu of %lu pixels of the rectangle written, %lu other values in it, %lu pixels changed outside it",name,r[1],r[11],r[12],r[2]);
 if(r[8]!=0xFFFFFFFFUL)say("; everything changed lies in (%lu,%lu)-(%lu,%lu)",r[8]&0xFFFF,r[8]>>16,r[9]&0xFFFF,r[9]>>16);else say("; nothing changed");
 say("; status before %08lX, first after %08lX; reads before the 2D busy bit: %s; reads until idle: %s\n",r[4],r[5],a,b);
 if(r[0]==5)say("   the command did not finish; the engine's reset %s\n",r[7]==1?"worked":"did NOT work");
 return r[0]==0&&r[1]==r[11]&&!r[2]&&!r[12];
}
static int key(const char *name,DWORD k,DWORD *r) {
 if(!step(k,r)||(r[0]&&r[0]!=5)){say("%s: not run%s%s\n",name,r[0]?": ":"",r[0]?why(r[0]):"");return -1;}
 say("%s: of 32 pixels that are not the key: %lu copied, %lu left; of 32 that are the key: %lu copied, %lu left; %lu other; ",name,r[1],r[4],r[2],r[5],r[6]);
 if(r[1]==32&&r[5]==32){say("the key colour is skipped (a transparent copy)\n");return 1;}
 if(r[2]==32&&r[4]==32){say("ONLY the key colour is copied (the opposite sense)\n");return 2;}
 if(r[1]==32&&r[2]==32){say("the key is ignored (a plain copy)\n");return 3;}
 say("no clear sense\n");return 0;
}
int WINAPI WinMain(HINSTANCE hi,HINSTANCE hp,LPSTR cmd,int show) {
 DWORD r[16],in,o[8],got=0,chip=0;char path[MAX_PATH],*s,a[32],b[32],c[32],d[32];int m1,old,m1b,k4=-1,k6=-1;HANDLE t;(void)hi;(void)hp;(void)cmd;(void)show;
 GetModuleFileNameA(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VX92D.LOG");lf=fopen(path,"w");
 say("VX92D v1.4 (driver .603): the 2D engine's register layout, key polarity and status, probed in the first 16 rows of the screen\n");
 dev=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(dev==INVALID_HANDLE_VALUE){say("FAILED: the VxD is not open\n");if(lf)fclose(lf);return 2;}
 t=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 in=0x75;memset(o,0,sizeof(o));if(t!=INVALID_HANDLE_VALUE){DeviceIoControl(t,0x85500022UL,&in,4,o,32,&got,0);CloseHandle(t);}
 chip=o[2];say("the VxD's build: .%lu; the device 1106:%04lX (%s); the driver's 2D engine %s, 3D engine %s\n",o[1],chip,chip==0x7122?"VX900":chip==0x5122?"VX855":chip==0x1122?"VX800":"?",(o[4]&2)?"used":"not used",(o[4]&1)?"used":"not used");
 if(o[1]<600){say("FAILED: the VxD is older than .600 (the probe is not in it)\n");if(lf)fclose(lf);return 2;}
 m1=fill("1 M1 layout fill",0,r);
 if(m1>=0)say("   the screen %lu bits, pitch %lu, the test area %lu x %lu; SR1A %02lX SR1E %02lX SR4E %02lX SR3C %02lX\n",r[14]&255,r[14]>>8,r[13]&0xFFFF,r[13]>>16,r[15]&255,(r[15]>>8)&255,(r[15]>>16)&255,r[15]>>24);
 if(m1==1){
  k4=key("2 keyed copy, 48h = 4000h",3,r);
  k6=key("3 keyed copy, 48h = 6000h",4,r);
  if(step(5,r)&&(!r[0]||r[0]==5)){
   reads(a,r[1]);reads(b,r[2]);reads(c,r[3]);reads(d,r[4]);
   say("4 two whole-screen inverts: status before %08lX, first after %08lX; fill 1: reads before the 2D busy bit %s, until idle %s; fill 2: %s, %s; the first rows afterwards %s\n",r[5],r[6],a,b,c,d,r[8]?"as before":"NOT as before");
   if(r[0]==5){say("   a fill did not finish; the engine's reset %s\n",r[7]==1?"worked":"did NOT work");wrong++;}
   if(!r[8])wrong++;
  }else say("4 two whole-screen inverts: not run%s%s\n",r[0]?": ":"",r[0]?why(r[0]):"");
 }else say("2-4: skipped (the M1 fill did not draw its rectangle)\n");
 old=fill("5 older layout fill (XP's 7122 sequence)",1,r);
 m1b=fill("6 M1 layout fill again",0,r);
 if(o[1]>=602&&step(6,r)&&!r[0]){
  static const char *const what[5]={"nothing done","one engine register written (5Ch)","one engine register read (04h)","a sequencer register read (SR1A)","the driver's flush alone"};int k;
  say("7 the status register, 16 tries each (not idle at the first read; the most reads until idle; that first status's low word):\n");
  for(k=0;k<5;k++)say("     after %-36s %2lu of 16, %2lu reads, %04lX\n",what[k],r[1+k]&255,(r[1+k]>>8)&255,r[1+k]>>16);
  say("     the status at rest: %08lX\n",r[6]);
 }else say("7 the status register: not run%s%s\n",r[0]&&r[0]!=0xFFFFFFFFUL?": ":"",r[0]&&r[0]!=0xFFFFFFFFUL?why(r[0]):"");
 say("\n");
 if(m1<0||old<0){say("NOT RUN: see above\n");if(lf)fclose(lf);CloseHandle(dev);return 1;}
 if(m1==1&&old==0)say("LAYOUT: M1 (the VX800 / VX855 register layout draws; the older layout does not)\n");
 else if(m1==0&&old==1)say("LAYOUT: the OLDER layout draws and M1 does not: the 2D code needs a second register table for this chip\n");
 else if(m1==1&&old==1)say("LAYOUT: BOTH layouts draw the rectangle\n");
 else say("LAYOUT: NEITHER layout drew the rectangle\n");
 if(m1==1){
  if(k4==1&&k6==1)say("KEY: 4000h and 6000h both skip the key colour: no chip-specific value needed\n");
  else if(k4==1)say("KEY: 4000h skips the key colour, as on the VX855; 6000h does not: keep 4000h\n");
  else if(k6==1)say("KEY: this chip needs 6000h (bit 13) for a transparent copy; 4000h alone does not skip the key colour\n");
  else say("KEY: neither value gave a transparent copy\n");
 }
 if(m1==1&&m1b!=1){say("WRONG: the M1 fill no longer draws after the older layout's writes\n");wrong++;}
 if(chip!=0x7122&&!(m1==1&&old==0&&k4==1)){say("WRONG: on a VX800 / VX855 the M1 fill must draw, the older layout must not, and 4000h must skip the key colour\n");wrong++;}
 say("%s\n",wrong?"FAILED: see WRONG above":"DONE");
 if(lf)fclose(lf);CloseHandle(dev);return wrong?2:0;
}
