/* VX2EDID v1.0: shows what the driver learned about the monitor (EDID over DDC) and which video modes it found.
 * Read-only: it only asks the driver for its report. Log: VX2EDID.TXT next to this program. Needs VX85598.VXD v10.37 or newer. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include "vx85598.h"

static FILE *lf;
static void out(const char *fmt,...) {
 char t[600]; va_list ap; va_start(ap,fmt); _vsnprintf(t,sizeof(t)-1,fmt,ap); va_end(ap); t[sizeof(t)-1]=0;
 fputs(t,stdout); if(lf) fputs(t,lf);
}
static const char *verdict(unsigned v) {
 static const char *n[11]={"base mode (always offered)","ADDED (monitor can show it)","BIOS mode query failed","not a linear direct-colour graphics mode","not 16 or 32 bits per pixel","size outside 640x480..1920x1200","layout, pitch or memory check failed","same size and depth already present","monitor does not list this size","no EDID, larger than 1280x1024","mode table full"};
 return v<11?n[v]:"?";
}
int main(void) {
 HANDLE d; vx_edid_report r; DWORD got=0; const unsigned char *e; unsigned i,x;
 lf=fopen("VX2EDID.TXT","a");
 out("\n==== VX2EDID v1.0 ====\n");
 d=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(d==INVALID_HANDLE_VALUE) {out("Cannot open the VX85598 driver.\n"); if(lf) fclose(lf); return 1;}
 memset(&r,0,sizeof(r));
 {DWORD q=1; if(!DeviceIoControl(d,VX_IOCTL_EDID,&q,4,&r,sizeof(r),&got,NULL)||got!=sizeof(r)||r.magic!=VX_MAGIC) {
  out("Driver gave no EDID report (need VX85598.VXD v10.37 or newer).\n"); CloseHandle(d); if(lf) fclose(lf); return 1;
 }}
 CloseHandle(d);
 {
  static const char *bn[3]={"bus 2 (SR31, DVI DDC)","bus 1 (SR26, VGA DDC)","bus 3 (SR2C)"};
  unsigned b,k;
  out("BIOS VBE/DDC (INT 10h 4F15h) at driver start: %s\n",r.reserved==0xFFFF?"not needed (the bus read worked)":(r.reserved==0x4F?"returned an EDID":"no EDID"));
  if(r.reserved!=0xFFFF) out("  BIOS answer AX=%04lX (004F = success; 014F = failed; 024F = not supported; 034F = not valid in this mode)\n",(unsigned long)r.reserved);
  out("DDC bus scan taken now:\n");
  for(b=0;b<3;b++) {
   const unsigned long *q=(const unsigned long*)(r.scan+12*b); /* same order as the names: bus 2, bus 1, bus 3 */
   out("  %s: lines idle %s (%lu)%s",bn[b],q[0]==3?"high/high":"NOT both high",q[0],q[0]==3?"":" - a line is held low, nothing can be read");
   if(q[0]==3) {
    out("; devices answering: %lu",q[1]); for(k=0;k<q[1]&&k<8;k++) out(" %02X",(unsigned)q[2+k]);
    out("; EDID address %s; EDID read now %s",q[10]?"answers":"silent",q[11]?"OK":"failed");
   }
   out("\n");
  }
 }
 e=(const unsigned char*)r.edid;
 if(r.edid_valid==2) out("NOTE: the EDID below was readable now but not when the driver started, so the driver is not using it yet.\n");
 if(!r.edid_valid) out("EDID: none could be read on any DDC bus (bus 2 = SR31, bus 1 = SR26, bus 3 = SR2C).\n  Without an EDID modes up to 1280x1024 are offered\n  (registry EDID_MODES=2 offers every BIOS mode up to 1920x1200).\n");
 else {
  unsigned dd; char name[16]="";
  out("EDID read OK via %s %lu. Manufacturer %c%c%c, product %04X, EDID %u.%u, %s input.\n",r.edid_bus==4?"the BIOS (VBE/DDC), code":"DDC bus",(unsigned long)r.edid_bus,
   '@'+((e[8]>>2)&31),'@'+(((e[8]&3)<<3)|(e[9]>>5)),'@'+(e[9]&31),e[10]|(e[11]<<8),e[18],e[19],(e[20]&0x80)?"DIGITAL":"ANALOG");
  for(dd=54;dd<=108;dd+=18) {
   if(e[dd]||e[dd+1]) {
    unsigned ha=e[dd+2]|((e[dd+4]&0xF0)<<4), va=e[dd+5]|((e[dd+7]&0xF0)<<4), pc=e[dd]|(e[dd+1]<<8);
    out("  detailed timing: %ux%u at %u.%02u MHz%s\n",ha,va,pc/100,pc%100,dd==54?" (preferred/native)":"");
   } else if(e[dd+3]==0xFC) {
    memcpy(name,e+dd+5,13); name[13]=0; for(i=0;i<13;i++) if(name[i]=='\n') name[i]=0;
    out("  monitor name: %s\n",name);
   } else if(e[dd+3]==0xFD) out("  range limits: vertical %u-%u Hz, horizontal %u-%u kHz, max pixel clock %u MHz\n",e[dd+5],e[dd+6],e[dd+7],e[dd+8],e[dd+9]*10);
  }
  out("  established timings bytes: %02X %02X %02X\n  standard timings:",e[35],e[36],e[37]);
  for(i=0;i<8;i++) if(!(e[38+2*i]==1&&e[39+2*i]==1)) {x=(e[38+2*i]+31)*8; out(" %ux?(%d Hz)",x,60+(e[39+2*i]&63));}
  out("\n  raw EDID:\n");
  for(i=0;i<128;i+=16) {unsigned k; out("    %02X:",i); for(k=0;k<16;k++) out(" %02X",e[i+k]); out("\n");}
 }
 out("\nBIOS video modes seen (%lu):\n",(unsigned long)r.bios_count);
 for(i=0;i<r.bios_count && i<64;i++) {
  unsigned long a=r.bios[i*3],b=r.bios[i*3+1],c=r.bios[i*3+2];
  out("  mode %03lX  %4lux%-4lu %2lu bpp  attr %04lX model %lu  -> %s\n",a&0xFFFF,b&0xFFFF,b>>16,c&0xFF,c>>16,(c>>8)&0xFF,verdict((unsigned)(a>>16)));
 }
 out("\nDriver mode table: %lu modes (%lu base, extras %s).\n",(unsigned long)r.total_modes,(unsigned long)r.base_modes,r.extras_on?"on":"off by the EDID_MODES registry switch");
 out("Done. Send me VX2EDID.TXT.\n");
 if(lf) fclose(lf);
 return 0;
}
