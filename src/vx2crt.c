/* VX2CRT v1.0: read-only. Shows what the display timing registers (IGA1 CRTC) are set to right now, decoded with the OpenChrome register map,
 * next to what Windows thinks the screen is. Use it at a good mode and at a bad one (for example 1024x768 and 1280x1024) and send VX2CRT.TXT.
 * No video memory and no 3D engine are touched. Needs VX85598.VXD v10.43 or newer. Log: VX2CRT.TXT next to this program. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "vx85598.h"

static FILE *lf;
static vx_crt_report rep;
static void out(const char *fmt,...) {
 char t[800]; va_list ap; va_start(ap,fmt); _vsnprintf(t,sizeof(t)-1,fmt,ap); va_end(ap); t[sizeof(t)-1]=0;
 fputs(t,stdout); if(lf) fputs(t,lf);
}
int main(void) {
 HANDLE d; DWORD got=0, got2=0; unsigned long *C,*S; unsigned i,k; vx_status_t st;
 unsigned long htot,hdisp,hbs,hss,vtot,vdisp,vss,vbs,off,dm,dr,dn,clk;
 lf=fopen("VX2CRT.TXT","a");
 out("\n==== VX2CRT v1.0 ====\n");
 out("Windows says the screen is %d x %d, %d bits per pixel.\n",GetSystemMetrics(SM_CXSCREEN),GetSystemMetrics(SM_CYSCREEN),(int)GetDeviceCaps(GetDC(NULL),BITSPIXEL));
 d=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(d==INVALID_HANDLE_VALUE) {out("Cannot open the VX85598 driver.\n"); if(lf) fclose(lf); return 1;}
 memset(&rep,0,sizeof(rep));
 if(!DeviceIoControl(d,VX_IOCTL_CRT,NULL,0,&rep,sizeof(rep),&got,NULL)) {out("Driver request failed: Windows error %lu (need VX85598.VXD v10.43 or newer).\n",GetLastError()); CloseHandle(d); if(lf) fclose(lf); return 1;}
 memset(&st,0,sizeof(st));
 DeviceIoControl(d,VX_IOCTL_STATUS,NULL,0,&st,sizeof(st),&got2,NULL);
 CloseHandle(d);
 if(got!=sizeof(rep)||rep.magic!=VX_MAGIC) {out("Driver reply not usable. Use VX85598.VXD from the same package.\n"); if(lf) fclose(lf); return 1;}
 S=(unsigned long*)0; (void)S;
 #define SR(n) ((unsigned long)rep.sr[(n)]&0xFF)
 #define CR(n) ((unsigned long)rep.cr[(n)]&0xFF)
 out("Driver says: %lu x %lu, %lu bpp, pitch %lu bytes\n",(unsigned long)st.width,(unsigned long)st.height,(unsigned long)st.bpp,(unsigned long)st.video_pitch);
 htot=((CR(0x00)|(((CR(0x36)>>3)&1)<<8))+5)*8;
 hdisp=(CR(0x01)+1)*8; hbs=(CR(0x02)+1)*8;
 hss=(CR(0x04)|(((CR(0x33)>>4)&1)<<8))*8;
 vtot=(CR(0x06)|((CR(0x07)&1)<<8)|(((CR(0x07)>>5)&1)<<9)|((CR(0x35)&1)<<10))+2;
 vdisp=(CR(0x12)|(((CR(0x07)>>1)&1)<<8)|(((CR(0x07)>>6)&1)<<9)|(((CR(0x35)>>2)&1)<<10))+1;
 vss=CR(0x10)|(((CR(0x07)>>2)&1)<<8)|(((CR(0x07)>>7)&1)<<9)|(((CR(0x35)>>1)&1)<<10);
 vbs=(CR(0x15)|(((CR(0x07)>>3)&1)<<8)|(((CR(0x09)>>5)&1)<<9)|(((CR(0x35)>>3)&1)<<10))+1;
 off=CR(0x13)|(((CR(0x35)>>5)&7)<<8);
 dm=SR(0x44)|((SR(0x45)&3)<<8); dr=(SR(0x45)>>2)&7; dn=SR(0x46)&0x7F;
 clk=dn?(14318UL*dm/(dn<<dr)):0;   /* kHz */
 out("IGA1 timing as programmed (OpenChrome register map):\n");
 out("  horizontal (pixels): total %lu, displayed %lu, blank start %lu, sync start %lu\n",htot,hdisp,hbs,hss);
 out("  vertical:   total %lu, displayed %lu, blank start %lu, sync start %lu\n",vtot,vdisp,vbs,vss);
 out("  line offset register %lu = %lu bytes per line (driver pitch %lu)\n",off,off*8,(unsigned long)st.video_pitch);
 out("  dot clock %lu.%03lu MHz",clk/1000,clk%1000);
 if(htot&&vtot&&clk) out(" -> %lu.%02lu kHz line rate, %lu.%02lu Hz refresh",clk/htot,(clk*100/htot)%100,(clk*1000/(htot*vtot)),((clk*100000/(htot*vtot))%100));
 out("\n");
 out("  CR07 %02lX  CR09 %02lX  CR33 %02lX  CR35 %02lX  CR36 %02lX\n",CR(0x07),CR(0x09),CR(0x33),CR(0x35),CR(0x36));
 out("  fetch/FIFO: SR1C %02lX SR1D %02lX SR16 %02lX SR17 %02lX SR18 %02lX  CR1A %02lX CR51 %02lX CR55 %02lX\n",SR(0x1C),SR(0x1D),SR(0x16),SR(0x17),SR(0x18),CR(0x1A),CR(0x51),CR(0x55));
 out("  IGA2: CR6A %02lX CR6B %02lX CR79 %02lX   scaling-related: CR9F %02lX CRA0 %02lX CRA1 %02lX\n",CR(0x6A),CR(0x6B),CR(0x79),CR(0x9F),CR(0xA0),CR(0xA1));
 if(vdisp!=(unsigned long)st.height) out("\nNOTE: the card is scanning %lu lines but the driver thinks the desktop is %lu lines tall.\n",vdisp,(unsigned long)st.height);
 if(hdisp!=(unsigned long)st.width) out("NOTE: the card is scanning %lu pixels per line but the driver thinks the desktop is %lu wide.\n",hdisp,(unsigned long)st.width);
 out("\nSR00..FF:\n");
 for(i=0;i<256;i+=16) {out("  %02X:",i); for(k=0;k<16;k++) out(" %02lX",SR(i+k)); out("\n");}
 out("CR00..FF:\n");
 for(i=0;i<256;i+=16) {out("  %02X:",i); for(k=0;k<16;k++) out(" %02lX",CR(i+k)); out("\n");}
 out("Done. Send me VX2CRT.TXT.\n");
 if(lf) fclose(lf);
 (void)C;
 return 0;
}
