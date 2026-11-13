/* VX9PROBE v1.1 (the VGA status port sampled: do vertical retrace and display enable ever change; which display channel drives the output; PCI programming interface bytes). v1.0: what this board's BIOS gives a DOS program in the places Windows 98's core touches first
 * (keyboard controller, A20, interrupt controllers, timer, clock, PCI BIOS, VGA, ACPI / MP tables).
 * Run from plain DOS ("Command prompt only"). Writes VX9PROBE.TXT beside itself. Compare a board that starts Windows with one that does not. */
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>
static FILE *lf;
static void say(const char *f,...) {va_list a; va_start(a,f); vprintf(f,a); va_end(a); if(lf) {va_start(a,f); vfprintf(lf,f,a); va_end(a); fflush(lf);} }
static unsigned long ticks(void) {unsigned long far *t=(unsigned long far *)MK_FP(0x40,0x6C); unsigned long a; _disable(); a=*t; _enable(); return a;}
static void io_wait(void) {inp(0x80);}
/* keyboard controller */
static long kbc_wait_in(void) {long n; for(n=0;n<200000L;n++) {if(!(inp(0x64)&2)) return n; io_wait();} return -1;}
static long kbc_wait_out(void) {long n; for(n=0;n<200000L;n++) {if(inp(0x64)&1) return n; io_wait();} return -1;}
static void kbc_flush(void) {int k; for(k=0;k<32&&(inp(0x64)&1);k++) {inp(0x60); io_wait();}}
static int kbc_cmd(unsigned char c,long *w1,long *w2) {int v; *w1=kbc_wait_in(); if(*w1<0) {*w2=-1; return -1;} outp(0x64,c); *w2=kbc_wait_out(); if(*w2<0) return -2; v=inp(0x60); return v;}
static int a20_on(void) {unsigned char far *lo=(unsigned char far *)MK_FP(0,0x500),far *hi=(unsigned char far *)MK_FP(0xFFFF,0x510); unsigned char s,r; int on; _disable(); s=*lo; *lo=(unsigned char)~*hi; r=*hi; on=(r!=*lo); *lo=s; _enable(); return on;}
static unsigned char cmos(unsigned char i) {unsigned char v; _disable(); outp(0x70,i|0x80); io_wait(); v=(unsigned char)inp(0x71); outp(0x70,0x0D); io_wait(); inp(0x71); _enable(); return v;}
static unsigned char vga_r(unsigned p,unsigned char i) {outp(p,i); return (unsigned char)inp(p+1);}
static void find_sig(const char *sig,int n,unsigned seg0,unsigned long len,const char *name,int step) {
 unsigned long o; unsigned char far *p; int found=0;
 for(o=0;o+n<=len;o+=step) {p=(unsigned char far *)MK_FP(seg0+(unsigned)(o>>4),(unsigned)(o&15)); if(!_fmemcmp(p,sig,n)) {int k; say("   %s at %04X:%04X:",name,seg0+(unsigned)(o>>4),(unsigned)(o&15)); for(k=0;k<24;k++) say(" %02X",p[k]); say("\n"); found=1; break;}}
 if(!found) say("   %s: not found in %04X:0 + %lu bytes\n",name,seg0,len);
}
int main(void) {
 union REGS r; struct SREGS s; long w1,w2; int v,cb,k; unsigned long t0,t1; unsigned ebda;
 lf=fopen("VX9PROBE.TXT","w");
 say("VX9PROBE v1.1: the legacy hardware as a DOS program sees it\n");
 r.h.ah=0x30; int86(0x21,&r,&r); say("DOS version %u.%u\n",r.h.al,r.h.ah);
 say("BIOS data: equipment word %04X, keyboard flags %02X %02X %02X, EBDA segment %04X, base memory %u K\n",*(unsigned far *)MK_FP(0x40,0x10),*(unsigned char far *)MK_FP(0x40,0x17),*(unsigned char far *)MK_FP(0x40,0x18),*(unsigned char far *)MK_FP(0x40,0x96),ebda=*(unsigned far *)MK_FP(0x40,0x0E),*(unsigned far *)MK_FP(0x40,0x13));
 {char d[9]; _fmemcpy(d,MK_FP(0xF000,0xFFF5),8); d[8]=0; say("BIOS date at F000:FFF5 \"%s\", model byte %02X\n",d,*(unsigned char far *)MK_FP(0xF000,0xFFFE));}

 say("== keyboard controller (ports 60/64)\n");
 say("   status port 64 = %02X (FF usually means no controller answers)\n",inp(0x64));
 kbc_flush();
 cb=kbc_cmd(0x20,&w1,&w2); say("   command 20 (read command byte): %s%02X, waited %ld / %ld polls\n",cb<0?"NO ANSWER ":"",cb<0?0:cb,w1,w2);
 v=kbc_cmd(0xD0,&w1,&w2); say("   command D0 (read output port): %s%02X (bit 1 = A20), waited %ld / %ld polls\n",v<0?"NO ANSWER ":"",v<0?0:v,w1,w2);
 v=kbc_cmd(0xAB,&w1,&w2); say("   command AB (keyboard interface test): %s%02X (00 = good), waited %ld / %ld polls\n",v<0?"NO ANSWER ":"",v<0?0:v,w1,w2);
 v=kbc_cmd(0xAA,&w1,&w2); say("   command AA (self test): %s%02X (55 = good), waited %ld / %ld polls\n",v<0?"NO ANSWER ":"",v<0?0:v,w1,w2);
 if(cb>=0) {if(kbc_wait_in()>=0) {outp(0x64,0x60); if(kbc_wait_in()>=0) outp(0x60,(unsigned char)cb);} }
 if(kbc_wait_in()>=0) outp(0x64,0xAE);
 k=kbc_cmd(0x20,&w1,&w2); say("   command byte after the tests and putting it back: %s%02X\n",k<0?"NO ANSWER ":"",k<0?0:k);
 /* keyboard itself: echo */
 kbc_flush(); if(kbc_wait_in()>=0) {outp(0x60,0xEE); w2=kbc_wait_out(); if(w2>=0) say("   keyboard echo (EE to port 60): %02X (EE = a keyboard answered), waited %ld polls\n",inp(0x60),w2); else say("   keyboard echo (EE to port 60): NO ANSWER\n");}
 kbc_flush();

 say("== A20\n");
 say("   A20 line now: %s; port 92 = %02X\n",a20_on()?"on":"off",inp(0x92));
 r.x.ax=0x2402; int86(0x15,&r,&r); say("   INT 15 AX=2402 (A20 state): carry %u, AH %02X, AL %02X\n",r.x.cflag&1,r.h.ah,r.h.al);
 r.x.ax=0x2403; int86(0x15,&r,&r); say("   INT 15 AX=2403 (A20 support): carry %u, AH %02X, BX %04X (bit 0 keyboard controller, bit 1 port 92)\n",r.x.cflag&1,r.h.ah,r.x.bx);

 say("== interrupt controllers\n");
 say("   masks: master %02X, slave %02X\n",inp(0x21),inp(0xA1));
 _disable(); outp(0x20,0x0A); io_wait(); v=inp(0x20); outp(0xA0,0x0A); io_wait(); k=inp(0xA0); _enable(); say("   requests pending (IRR): master %02X, slave %02X\n",v,k);
 _disable(); outp(0x20,0x0B); io_wait(); v=inp(0x20); outp(0xA0,0x0B); io_wait(); k=inp(0xA0); outp(0x20,0x0A); outp(0xA0,0x0A); _enable(); say("   in service (ISR): master %02X, slave %02X\n",v,k);
 say("   edge/level registers 4D0 = %02X, 4D1 = %02X\n",inp(0x4D0),inp(0x4D1));
 {unsigned long far *iv=(unsigned long far *)MK_FP(0,0); say("   vectors: 08 %08lX, 09 %08lX, 0B %08lX, 70 %08lX, 74 %08lX, 76 %08lX\n",iv[8],iv[9],iv[0xB],iv[0x70],iv[0x74],iv[0x76]);}

 say("== timer\n");
 _disable(); outp(0x43,0xE2); io_wait(); v=inp(0x40); _enable(); say("   channel 0 status (read-back): %02X (mode bits 3-1: 3 = square wave, 2 = rate generator)\n",v);
 {unsigned a,b; _disable(); outp(0x43,0); a=inp(0x40); a|=inp(0x40)<<8; _enable(); for(k=0;k<200;k++) io_wait(); _disable(); outp(0x43,0); b=inp(0x40); b|=inp(0x40)<<8; _enable(); say("   channel 0 count: %04X then %04X after 200 port-80 reads\n",a,b);}
 t0=ticks(); {unsigned long n=0; while(ticks()==t0&&n<4000000UL) n++; t0=ticks(); n=0; while(ticks()<t0+18&&n<40000000UL) n++; t1=ticks(); say("   18 BIOS ticks (about 1 second) took %lu loop passes; ticks moved %lu (0 = IRQ 0 is not arriving)\n",n,t1-t0);}
 say("   port 61 = %02X\n",inp(0x61));

 say("== clock (CMOS)\n");
 say("   status A %02X, B %02X, C %02X, D %02X; shutdown byte 0F = %02X; time %02X:%02X:%02X\n",cmos(0x0A),cmos(0x0B),cmos(0x0C),cmos(0x0D),cmos(0x0F),cmos(4),cmos(2),cmos(0));

 say("== memory as the BIOS reports it\n");
 r.h.ah=0x88; int86(0x15,&r,&r); say("   INT 15 AH=88: %u K above 1 MB (carry %u)\n",r.x.ax,r.x.cflag&1);
 r.x.ax=0xE801; int86(0x15,&r,&r); say("   INT 15 AX=E801: carry %u, AX %04X K below 16 MB, BX %04X x 64 K above; CX %04X, DX %04X\n",r.x.cflag&1,r.x.ax,r.x.bx,r.x.cx,r.x.dx);
 r.x.ax=0x4300; int86(0x2F,&r,&r); say("   XMS driver installed: %s\n",r.h.al==0x80?"yes":"no");

 say("== PCI BIOS and the devices Windows meets first\n");
 r.x.ax=0xB101; int86(0x1A,&r,&r); say("   INT 1A AX=B101: carry %u, AH %02X, version %X.%02X, mechanisms %02X, last bus %u\n",r.x.cflag&1,r.h.ah,r.h.bh,r.h.bl,r.h.al,r.h.cl);
 {unsigned dev,fn; for(dev=0;dev<32;dev++) for(fn=0;fn<8;fn++) {unsigned ven,did,cls,irq,hdr;
   r.x.ax=0xB109; r.x.bx=(dev<<3)|fn; r.x.di=0; int86(0x1A,&r,&r); ven=r.x.cx; if((r.x.cflag&1)||ven==0xFFFF) {if(!fn) break; continue;}
   r.x.ax=0xB109; r.x.bx=(dev<<3)|fn; r.x.di=2; int86(0x1A,&r,&r); did=r.x.cx;
   r.x.ax=0xB109; r.x.bx=(dev<<3)|fn; r.x.di=0x0A; int86(0x1A,&r,&r); cls=r.x.cx;
   r.x.ax=0xB109; r.x.bx=(dev<<3)|fn; r.x.di=0x3C; int86(0x1A,&r,&r); irq=r.x.cx;
   r.x.ax=0xB108; r.x.bx=(dev<<3)|fn; r.x.di=0x0E; int86(0x1A,&r,&r); hdr=r.h.cl;
   {unsigned pi; r.x.ax=0xB108; r.x.bx=(dev<<3)|fn; r.x.di=9; int86(0x1A,&r,&r); pi=r.h.cl;
   say("   00:%02X.%u %04X:%04X class %04X programming interface %02X, interrupt line %02X pin %02X\n",dev,fn,ven,did,cls,pi,irq&255,irq>>8);}
   if(!fn&&!(hdr&0x80)) break;}}

 say("== VGA\n");
 r.x.ax=0x0F00; int86(0x10,&r,&r); say("   mode %02X, %u columns, page %u\n",r.h.al,r.h.ah,r.h.bh);
 r.x.ax=0x1A00; int86(0x10,&r,&r); say("   INT 10 AX=1A00: AL %02X, display codes %02X %02X (08 = VGA colour)\n",r.h.al,r.h.bl,r.h.bh);
 say("   misc output %02X, sequencer 01 %02X 04 %02X, CRTC 07 (overflow) %02X, 09 %02X, 11 %02X, 17 %02X, 18 (line compare) %02X\n",inp(0x3CC),vga_r(0x3C4,1),vga_r(0x3C4,4),vga_r(0x3D4,7),vga_r(0x3D4,9),vga_r(0x3D4,0x11),vga_r(0x3D4,0x17),vga_r(0x3D4,0x18));
 say("   extended sequencer (unlocked with 10 = 01): "); outp(0x3C4,0x10); outp(0x3C5,1); {static const unsigned char e[]={0x10,0x11,0x12,0x13,0x15,0x16,0x17,0x18,0x1A,0x1B,0x1E,0x2D,0x30,0x39,0x3F}; for(k=0;k<(int)sizeof(e);k++) say("%02X=%02X ",e[k],vga_r(0x3C4,e[k]));} say("\n");
 /* v1.1: Windows' display device and the VGA driver wait for vertical retrace by polling port 3DA; a wait that never ends is a freeze */
 {unsigned long n=0,vt=0,dt=0,v1=0,d1=0; unsigned char p,q; unsigned long e; e=ticks(); while(ticks()==e); e=ticks()+9; p=(unsigned char)inp(0x3DA);
  while(ticks()<e) {q=(unsigned char)inp(0x3DA); n++; if((q^p)&8) vt++; if((q^p)&1) dt++; if(q&8) v1++; if(q&1) d1++; p=q;}
  say("   port 3DA over half a second: %lu reads; vertical retrace bit changed %lu times (about 60 expected), set in %lu reads; display enable bit changed %lu times (thousands expected), set in %lu reads; last value %02X\n",n,vt,v1,dt,d1,p);
  if(!vt) say("   >>> vertical retrace never changed: a program that waits for it by polling 3DA waits for ever\n");
  if(!dt) say("   >>> display enable never changed\n");
  say("   input status 0 (3C2) %02X, feature control (3CA) %02X\n",inp(0x3C2),inp(0x3CA));}
 say("   CRTC 00-18:"); for(k=0;k<=0x18;k++) say(" %02X",vga_r(0x3D4,(unsigned char)k)); say("\n");
 say("   CRTC extended:"); {static const unsigned char e[]={0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x3B,0x3C,0x3D,0x3E,0x3F,0x47,0x6A,0x6B,0x6C,0x79,0x88,0x8A,0x91,0x96,0x97,0x98,0x99,0x9B,0xD2,0xF3,0xF6,0xF9,0xFB}; for(k=0;k<(int)sizeof(e);k++) say(" %02X=%02X",e[k],vga_r(0x3D4,e[k]));} say("\n");
 say("   sequencer 00-04:"); for(k=0;k<=4;k++) say(" %02X",vga_r(0x3C4,(unsigned char)k)); say("; extended:"); {static const unsigned char e[]={0x12,0x13,0x14,0x1C,0x1D,0x2A,0x2B,0x2C,0x3A,0x3E,0x40,0x44,0x45,0x46,0x59,0x5A,0x5B,0x65}; for(k=0;k<(int)sizeof(e);k++) say(" %02X=%02X",e[k],vga_r(0x3C4,e[k]));} say("\n");
 r.x.ax=0x5F01; r.x.bx=0; int86(0x10,&r,&r); say("   VIA BIOS INT 10 AX=5F01 (chip): AX %04X BX %04X CX %04X DX %04X\n",r.x.ax,r.x.bx,r.x.cx,r.x.dx);
 r.x.ax=0x5F02; r.x.bx=0; r.x.cx=0; int86(0x10,&r,&r); say("   VIA BIOS INT 10 AX=5F02 (devices): AX %04X BX %04X CX %04X DX %04X\n",r.x.ax,r.x.bx,r.x.cx,r.x.dx);
 {static unsigned char vb[512]; memcpy(vb,"VBE2",4); segread(&s); s.es=FP_SEG((void far *)vb); r.x.di=FP_OFF((void far *)vb); r.x.ax=0x4F00; int86x(0x10,&r,&r,&s);
  if(r.x.ax==0x004F) {char far *oem=(char far *)MK_FP(*(unsigned *)(vb+8),*(unsigned *)(vb+6)); char o[40]; int i; for(i=0;i<39&&oem[i];i++) o[i]=oem[i]; o[i]=0; say("   VBE %u.%u, \"%s\", %u x 64 K of video memory\n",vb[5],vb[4],o,*(unsigned *)(vb+18));} else say("   VBE: AX %04X\n",r.x.ax);}

 say("== power management and tables\n");
 r.x.ax=0x5300; r.x.bx=0; int86(0x15,&r,&r); if(r.x.cflag&1) say("   APM: not there (AH %02X)\n",r.h.ah); else say("   APM %u.%u, flags %04X\n",r.h.ah,r.h.al,r.x.cx);
 find_sig("RSD PTR ",8,0xE000,0x20000UL,"ACPI RSDP (byte 15 = revision)",16);
 if(ebda) find_sig("RSD PTR ",8,ebda,1024UL,"ACPI RSDP in the EBDA",16);
 find_sig("_MP_",4,0xF000,0x10000UL,"MP floating pointer (multiprocessor / APIC table)",16);
 if(ebda) find_sig("_MP_",4,ebda,1024UL,"MP floating pointer in the EBDA",16);
 find_sig("$PnP",4,0xF000,0x10000UL,"PnP BIOS",16);
 find_sig("_SM_",4,0xF000,0x10000UL,"SMBIOS",16);
 say("done\n"); if(lf) fclose(lf); return 0;
}
