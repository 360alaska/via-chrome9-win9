/* BIOSRD: read-only dump of PCI config, CMOS, VGA SR/CR, RAM seen by Win98.
   Writes BIOSRD[_label].TXT next to the EXE. Run once per BIOS setting. */
#include <windows.h>
#include <conio.h>
#include <stdio.h>
#include <string.h>
#include "vx85598.h"

static FILE *out;
static unsigned long pci_rd(int bus,int dev,int fn,int reg)
{
 outpd(0xCF8,0x80000000UL|((unsigned long)bus<<16)|((unsigned long)dev<<11)|((unsigned long)fn<<8)|(reg&0xFC));
 return inpd(0xCFC);
}
static unsigned cmos_rd(int idx)
{
 if(idx<0x80){outp(0x70,idx);return inp(0x71);}
 outp(0x72,idx&0x7F);return inp(0x73);
}
static void hexrow(const char *tag,int base,const unsigned char *b,int n)
{
 int i; fprintf(out,"%s %02X:",tag,base);
 for(i=0;i<n;i++) fprintf(out," %02X",b[i]);
 fprintf(out,"\n");
}
static void dump_vxd(void)
{
 HANDLE h; vx_status_t s; DWORD got=0;
 h=CreateFile("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(h==INVALID_HANDLE_VALUE){fprintf(out,"[VXD] not open (error %lu)\n",GetLastError());return;}
 memset(&s,0,sizeof(s));
 if(!DeviceIoControl(h,VX_IOCTL_STATUS,NULL,0,&s,sizeof(s),&got,NULL)||s.magic!=VX_MAGIC){
  fprintf(out,"[VXD] status ioctl failed or version mismatch\n");CloseHandle(h);return;}
 fprintf(out,"[VXD] version %08lX stage %lu failure %lu pci %08lX cmd %08lX bar0 %08lX bar1 %08lX\n",
  s.version,s.stage,s.failure,s.pci_address,s.pci_command,s.bar0,s.bar1);
 fprintf(out,"[VXD] fb_phys %08lX fb_bytes %lu (clamped by VX_FB_LIMIT) fb_linear %08lX\n",s.fb_phys,s.fb_bytes,s.fb_linear);
 fprintf(out,"[VXD] vbe_version %04lX modes %lu active %lu %lux%lux%lu video_pitch %lu\n",
  s.vbe_version,s.mode_count,s.active_mode,s.width,s.height,s.bpp,s.video_pitch);
 CloseHandle(h);
}
int main(int argc,char **argv)
{
 char name[MAX_PATH],*last; int bus=0,dev,fn,i,found=0;
 unsigned char cfg[256]; unsigned char b[16];
 MEMORYSTATUS ms; unsigned long v,a0,vga_dev=0xFF,vga_fn=0;
 GetModuleFileName(NULL,name,sizeof(name));
 last=strrchr(name,'\\'); if(last) last[1]=0; else name[0]=0;
 strcat(name,argc>1?"BIOSRD_":"BIOSRD");
 if(argc>1) strncat(name,argv[1],16);
 strcat(name,".TXT");
 out=fopen(name,"w"); if(!out){printf("cannot write %s\n",name);return 1;}
 fprintf(out,"BIOSRD v1 label=%s\n",argc>1?argv[1]:"(none)");
 ms.dwLength=sizeof(ms); GlobalMemoryStatus(&ms);
 fprintf(out,"[MEM] Win98 total physical %lu bytes (%lu KB), avail %lu KB, pagefile total %lu KB\n",
  (unsigned long)ms.dwTotalPhys,(unsigned long)(ms.dwTotalPhys>>10),(unsigned long)(ms.dwAvailPhys>>10),(unsigned long)(ms.dwTotalPageFile>>10));
 dump_vxd();
 fprintf(out,"\n[PCI] bus 0 scan; full 256-byte config of each device\n");
 for(dev=0;dev<32;dev++) for(fn=0;fn<8;fn++){
  v=pci_rd(bus,dev,fn,0); if((v&0xFFFF)==0xFFFF||v==0) continue;
  found++;
  for(i=0;i<256;i+=4){unsigned long w=pci_rd(bus,dev,fn,i);cfg[i]=(unsigned char)w;cfg[i+1]=(unsigned char)(w>>8);cfg[i+2]=(unsigned char)(w>>16);cfg[i+3]=(unsigned char)(w>>24);}
  fprintf(out,"\n[PCI] %02X:%02X.%X id %04lX:%04lX class %02X%02X%02X\n",bus,dev,fn,v&0xFFFF,v>>16,cfg[0x0B],cfg[0x0A],cfg[0x09]);
  for(i=0;i<256;i+=16){char tag[24];sprintf(tag,"PCI %02X.%X",dev,fn);hexrow(tag,i,cfg+i,16);}
  if(cfg[0x0B]==3&&vga_dev==0xFF){vga_dev=dev;vga_fn=fn;}
  if(dev==0&&fn==3){
   int n=(cfg[0xA1]>>4)&7;
   fprintf(out,"[DECODE] D0F3 reg A1=%02X -> size code n=%d; inferred size = 4MB<<n = %d MB (mapping inferred from BIOS code, not confirmed)\n",cfg[0xA1],n,4<<n);
   fprintf(out,"[DECODE] D0F3 reg 90=%02X (bits2:0=%d, BIOS puts (n+3)<<4 into CR3D)\n",cfg[0x90],cfg[0x90]&7);
  }
 }
 fprintf(out,"\n[PCI] %d devices on bus 0\n",found);
 fprintf(out,"\n[CMOS] 00-FF (read-only; 80-FF via ports 72/73 if supported)\n");
 for(i=0;i<256;i+=16){int j;for(j=0;j<16;j++)b[j]=(unsigned char)cmos_rd(i+j);hexrow("CMOS",i,b,16);}
 outp(0x70,0x0D);
 fprintf(out,"\n[VGA] SR00-7F and CR30-7F (read-only)\n");
 {
  unsigned char sr_save=(unsigned char)inp(0x3C4),cr_save=(unsigned char)inp(0x3D4);
  for(i=0;i<0x80;i+=16){int j;for(j=0;j<16;j++){outp(0x3C4,i+j);b[j]=(unsigned char)inp(0x3C5);}hexrow("SR",i,b,16);}
  outp(0x3C4,0x39);fprintf(out,"[DECODE] SR39=%02X\n",inp(0x3C5));
  outp(0x3C4,0x68);fprintf(out,"[DECODE] SR68=%02X\n",inp(0x3C5));
  for(i=0x30;i<0x80;i+=16){int j;for(j=0;j<16;j++){outp(0x3D4,i+j);b[j]=(unsigned char)inp(0x3D5);}hexrow("CR",i,b,16);}
  outp(0x3C4,sr_save);outp(0x3D4,cr_save);
 }
 fclose(out);
 printf("Wrote %s\n",name);
 return 0;
}
