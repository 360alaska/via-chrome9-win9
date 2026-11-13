/* XPDUMP: read-only register dump of the VX855 display state under Windows XP (or any NT-family Windows with inpout32.dll).
   Usage:  XPDUMP <label>      e.g.  XPDUMP 1024x768-E2014H
   Needs inpout32.dll in the same folder (or on the PATH) and an Administrator account.
   Reads VGA sequencer/CRTC registers through the standard ports 3C4/3C5, 3D4/3D5 and talks to the VT1632 and the monitor EDID over the
   bit-banged DVI I2C bus (SR31). Writes only index registers, SR10 bit 0 (restored) and SR31 (restored). Output is appended to XPDUMP.TXT. */
#include <windows.h>
#include <stdio.h>
#include <string.h>
typedef short (__stdcall *Inp32_t)(short);
typedef void (__stdcall *Out32_t)(short,short);
static Inp32_t pInp; static Out32_t pOut;
static unsigned char inb(unsigned port) {return (unsigned char)(pInp((short)port)&0xFF);}
static void outb(unsigned port,unsigned v) {pOut((short)port,(short)(v&0xFF));}
static FILE *out;
static unsigned char sr_get(unsigned char i) {unsigned char o=inb(0x3C4),v; outb(0x3C4,i); v=inb(0x3C5); outb(0x3C4,o); return v;}
static void sr_mask(unsigned char i,unsigned char mask,unsigned char val) {unsigned char o=inb(0x3C4),v; outb(0x3C4,i); v=inb(0x3C5); outb(0x3C5,(unsigned char)((v&~mask)|(val&mask))); outb(0x3C4,o);}
static unsigned cr_base;
static unsigned char cr_get(unsigned char i) {unsigned char o=inb(cr_base),v; outb(cr_base,i); v=inb(cr_base+1); outb(cr_base,o); return v;}
/* I2C on bus 2 (SR31): enable 0x01, SCL write 0x20, SDA write 0x10, SCL read 0x08, SDA read 0x04 */
static void i2delay(void) {int i; for(i=0;i<6;i++) (void)inb(0x80);}
static void i2set(int scl,int sda) {sr_mask(0x31,0x31,(unsigned char)(0x01|(scl?0x20:0)|(sda?0x10:0)));}
static int i2sda(void) {unsigned char v; sr_mask(0x31,0x01,0x01); v=sr_get(0x31); return (v&0x04)?1:0;}
static int i2scl(void) {unsigned char v; v=sr_get(0x31); return (v&0x08)?1:0;}
static void i2start(void) {i2set(1,1); i2delay(); i2set(1,0); i2delay(); i2set(0,0); i2delay();}
static void i2stop(void) {i2set(0,0); i2delay(); i2set(1,0); i2delay(); i2set(1,1); i2delay();}
static int i2put(unsigned char b) {int i,ack; for(i=7;i>=0;i--) {i2set(0,(b>>i)&1); i2delay(); i2set(1,(b>>i)&1); i2delay(); i2set(0,(b>>i)&1); i2delay();} i2set(0,1); i2delay(); i2set(1,1); i2delay(); ack=i2sda()?0:1; i2set(0,1); i2delay(); return ack;}
static unsigned char i2get(int ack) {int i; unsigned char v=0; i2set(0,1); for(i=0;i<8;i++) {i2delay(); i2set(1,1); i2delay(); v=(unsigned char)((v<<1)|i2sda()); i2set(0,1);} i2set(0,ack?0:1); i2delay(); i2set(1,ack?0:1); i2delay(); i2set(0,ack?0:1); i2delay(); i2set(0,1); return v;}
static int i2probe(unsigned char a7) {int ack; i2start(); ack=i2put((unsigned char)(a7<<1)); i2stop(); return ack;}
static void table(const char *name,unsigned char (*get)(unsigned char)) {
 unsigned i,k; fprintf(out,"%s:\n",name);
 for(i=0;i<256;i+=16) {fprintf(out,"  %02X:",i); for(k=0;k<16;k++) fprintf(out," %02X",get((unsigned char)(i+k))); fprintf(out,"\n");}
}
static unsigned char cr_get_w(unsigned char i) {return cr_get(i);}
static unsigned char sr_get_w(unsigned char i) {return sr_get(i);}
int main(int argc,char **argv) {
 HMODULE h; SYSTEMTIME t; unsigned char unlock,misc,s31,e[128],tmp[16]; unsigned k,a; int ok;
 const char *label=argc>1?argv[1]:"(no label)";
 h=LoadLibrary("inpout32.dll");
 if(!h) {printf("inpout32.dll not found. Put it next to XPDUMP.EXE (and run as Administrator).\n"); return 1;}
 pInp=(Inp32_t)GetProcAddress(h,"Inp32"); pOut=(Out32_t)GetProcAddress(h,"Out32");
 if(!pInp||!pOut) {printf("inpout32.dll does not export Inp32/Out32.\n"); return 1;}
 out=fopen("XPDUMP.TXT","a");
 if(!out) {printf("Cannot write XPDUMP.TXT here.\n"); return 1;}
 GetLocalTime(&t);
 fprintf(out,"\n==== XPDUMP v1 label '%s'  %04u-%02u-%02u %02u:%02u:%02u ====\n",label,t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
 unlock=sr_get(0x10); sr_mask(0x10,0x01,0x01);
 misc=inb(0x3CC); cr_base=(misc&1)?0x3D4:0x3B4;
 fprintf(out,"misc output %02X (CRTC at %03X); SR10 was %02X; SR00=%02X (expect 03)\n",misc,cr_base,unlock,sr_get(0x00));
 {
  unsigned pdm=sr_get(0x44)|((sr_get(0x45)&3)<<8),pdr=(sr_get(0x45)>>2)&7,pdn=sr_get(0x46)&0x7F,pdz=((sr_get(0x45)>>7)&1)|((sr_get(0x46)>>6)&2);
  unsigned sdm=sr_get(0x4A)|((sr_get(0x4B)&3)<<8),sdr=(sr_get(0x4B)>>2)&7,sdn=sr_get(0x4C)&0x7F,sdz=((sr_get(0x4B)>>7)&1)|((sr_get(0x4C)>>6)&2);
  fprintf(out,"primary PLL SR44-46: dm %u dn %u dr %u dtz %u = %lu kHz;  secondary PLL SR4A-4C: dm %u dn %u dr %u dtz %u = %lu kHz\n",pdm,pdn,pdr,pdz,pdn?14318UL*pdm/((unsigned long)pdn<<pdr):0UL,sdm,sdn,sdr,sdz,sdn?14318UL*sdm/((unsigned long)sdn<<sdr):0UL);
  fprintf(out,"analog source IGA%u (SR16[6]); DVP1 pad SR1E[5:4]=%u; DVP1 source IGA%u (CR9B[4]); CR9B %02X (adjust %u, sync bits %u); SR65 %02X; SR1B %02X SR2D %02X\n",((sr_get(0x16)>>6)&1)+1,(sr_get(0x1E)>>4)&3,((cr_get(0x9B)>>4)&1)+1,cr_get(0x9B),cr_get(0x9B)&15,(cr_get(0x9B)>>5)&3,sr_get(0x65),sr_get(0x1B),sr_get(0x2D));
  fprintf(out,"IGA2: CR6A %02X CR6B %02X CR6C %02X CR79 %02X; CR97 %02X CR99 %02X CRD2 %02X; FIFO CR68 %02X CR92 %02X CR94 %02X CR95 %02X\n",cr_get(0x6A),cr_get(0x6B),cr_get(0x6C),cr_get(0x79),cr_get(0x97),cr_get(0x99),cr_get(0xD2),cr_get(0x68),cr_get(0x92),cr_get(0x94),cr_get(0x95));
 }
 table("SR00..FF",sr_get_w); table("CR00..FF",cr_get_w);
 s31=sr_get(0x31);
 i2set(1,1); i2delay(); i2delay();
 ok=(i2scl()&&i2sda());
 fprintf(out,"I2C bus 2 (SR31) idle lines SCL/SDA ok: %d\n",ok);
 if(ok) {
  fprintf(out,"devices answering:");
  for(a=0x08;a<=0x77;a++) if(i2probe((unsigned char)a)) fprintf(out," %02X",a);
  fprintf(out,"\n");
  if(i2probe(0x08)) {
   i2start(); i2put(0x10); i2put(0x00); i2start(); i2put(0x11); for(k=0;k<16;k++) tmp[k]=i2get(k<15); i2stop();
   fprintf(out,"VT1632 regs 00..0F:"); for(k=0;k<16;k++) fprintf(out," %02X",tmp[k]); fprintf(out,"   (reads of 0B..0F are not reliable)\n");
  } else fprintf(out,"VT1632 (7-bit 08) did not answer\n");
  if(i2probe(0x50)) {
   i2start(); if(i2put(0xA0)&&i2put(0x00)) {i2start(); if(i2put(0xA1)) {for(k=0;k<128;k++) e[k]=i2get(k<127); fprintf(out,"EDID:\n"); for(k=0;k<128;k+=16) {unsigned j; fprintf(out,"  %02X:",k); for(j=0;j<16;j++) fprintf(out," %02X",e[k+j]); fprintf(out,"\n");}}} i2stop();
  }
 }
 sr_mask(0x31,0xFF,s31); sr_mask(0x10,0x01,(unsigned char)(unlock&1));
 fclose(out);
 printf("Done. Appended to XPDUMP.TXT (label '%s').\n",label);
 return 0;
}
