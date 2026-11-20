/* VXDOSD v1.0: the DOS half of the full-screen DOS session tester (VXDOS.EXE is the Windows half). Run it from a full-screen MS-DOS
 * Prompt (type VXDOSD and Enter, even if the screen is black or garbled), and once from plain DOS ("Restart in MS-DOS mode") for the
 * reference picture of the BIOS's own text mode. It never needs to be seen: it beeps, and the questions take Y or N (30 s, then "no answer").
 *  A. the display as found: the BIOS's mode, the standard VGA registers, the chip's extended sequencer and CRTC registers (SR00-FF,
 *     CR00-FF), the text in the first row; one beep, then: can you read the text on the screen? (Y / N)
 *  B. the BIOS sets text mode 3 (INT 10h AX=0003), the same dump; two beeps: can you read it now?
 *  C. the BIOS sets 640 x 480 graphics (mode 12h) with black and white stripes, the same dump; three beeps: do you see stripes?
 *     then text mode 3 again.
 * One long beep at the end. Writes VXDOSD.TXT (inside Windows) or VXDOSR.TXT (plain DOS) beside itself.
 * Copyright (c) 2026 360ALASKA. MIT; see LICENSE.
 * Build (Open Watcom): wcl -q -bt=dos -ml -0 -os -l=dos -fe=VXDOSD.EXE vxdosd.c */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <conio.h>
#include <dos.h>
#include <i86.h>
static FILE *lf;
static void say(const char *f,...) {va_list a; if(!lf) return; va_start(a,f); vfprintf(lf,f,a); va_end(a); fflush(lf);}
static unsigned long ticks(void) {unsigned long far *t=(unsigned long far *)MK_FP(0x40,0x6C); unsigned long a; _disable(); a=*t; _enable(); return a;}
static void wait_ticks(unsigned long n) {unsigned long t=ticks(); while(ticks()-t<n) ;}
static void tone(unsigned hz,unsigned long n) {unsigned d=(unsigned)(1193182UL/hz); outp(0x43,0xB6); outp(0x42,d&255); outp(0x42,d>>8); outp(0x61,inp(0x61)|3); wait_ticks(n); outp(0x61,inp(0x61)&0xFC);}
static void beeps(int n) {int k; for(k=0;k<n;k++) {tone(880,4); wait_ticks(4);}}
static int key_there(void) {union REGPACK p; memset(&p,0,sizeof(p)); p.h.ah=1; intr(0x16,&p); return !(p.w.flags&0x40);} /* ZF clear: a key waits */
static int key_get(void) {union REGS r; r.h.ah=0; int86(0x16,&r,&r); return r.h.al;}
static void kflush(void) {while(key_there()) key_get();}
/* Y / N with a 30 s limit */
static int ask(const char *q) {
 unsigned long t=ticks(); int a=0;
 kflush(); say("   %s ",q);
 while(ticks()-t<546UL) if(key_there()) {a=key_get()|32; if(a=='y'||a=='n') break; a=0;}
 say("%s\n",a=='y'?"YES":a=='n'?"NO":"no answer (30 s)"); return a;
}
static unsigned crtc(void) {return (inp(0x3CC)&1)?0x3D4:0x3B4;}
static unsigned char rd(unsigned p,unsigned char i) {unsigned char o=(unsigned char)inp(p),v; outp(p,i); v=(unsigned char)inp(p+1); outp(p,o); return v;}
static unsigned char ar(unsigned char i) {unsigned char v; _disable(); inp(crtc()+6); outp(0x3C0,(unsigned char)(i|0x20)); v=(unsigned char)inp(0x3C1); inp(crtc()+6); outp(0x3C0,0x20); _enable(); return v;}
static void grid(const char *name,unsigned p,int n) {int i; for(i=0;i<n;i++) {if(!(i&15)) say("      %s%02X:",name,i); say(" %02X",rd(p,(unsigned char)i)); if((i&15)==15||i==n-1) say("\n");}}
static void dump(const char *when) {
 union REGS r; unsigned char sr10,far *bda=(unsigned char far *)MK_FP(0x40,0); char row[81]; int i; unsigned char far *t=(unsigned char far *)MK_FP(0xB800,0);
 say("== %s\n",when);
 r.h.ah=0x0F; int86(0x10,&r,&r); say("   INT 10h AH=0F: mode %02X, %u columns, page %u; BIOS data: mode %02X, columns %u, rows %u, CRTC port %04X\n",r.h.al,r.h.ah,r.h.bh,bda[0x49],*(unsigned far *)(bda+0x4A),bda[0x84]+1,*(unsigned far *)(bda+0x63));
 r.x.ax=0x4F03; int86(0x10,&r,&r); say("   INT 10h AX=4F03 (VESA mode): AX %04X, BX %04X\n",r.x.ax,r.x.bx);
 say("   misc output %02X, input status 1 %02X, feature %02X, DAC mask %02X\n",inp(0x3CC),inp(crtc()+6),inp(0x3CA),inp(0x3C6));
 say("   graphics controller:"); for(i=0;i<9;i++) say(" %02X",rd(0x3CE,(unsigned char)i)); say("\n");
 say("   attribute controller:"); for(i=0;i<21;i++) say(" %02X",ar((unsigned char)i)); say("\n");
 sr10=rd(0x3C4,0x10); outp(0x3C4,0x10); outp(0x3C5,(unsigned char)(sr10|1)); /* the extended registers unlocked while read */
 say("   sequencer (SR10 was %02X):\n",sr10); grid("SR",0x3C4,256);
 say("   CRTC at %04X:\n",crtc()); grid("CR",crtc(),256);
 outp(0x3C4,0x10); outp(0x3C5,sr10);
 for(i=0;i<80;i++) {unsigned char c=t[2*i]; row[i]=(char)(c>=32&&c<127?c:'.');} row[80]=0;
 say("   text row 0 at B800: \"%s\", attribute %02X\n",row,t[1]);
}
static void text_mark(const char *s) {unsigned char far *t=(unsigned char far *)MK_FP(0xB800,0); int i,k,n=(int)strlen(s); for(k=8;k<16;k++) for(i=0;i<80;i++) {t[(k*80+i)*2]=(unsigned char)(i<n?s[i]:' '); t[(k*80+i)*2+1]=k&1?0x1F:0x4E;}}
static void stripes(void) {unsigned char far *v=(unsigned char far *)MK_FP(0xA000,0); unsigned i; outp(0x3C4,2); outp(0x3C5,15); outp(0x3CE,8); outp(0x3CF,0xFF); for(i=0;i<38400U;i++) v[i]=(unsigned char)(((i/80)/40)&1?0xFF:0x00);}
static void setmode(unsigned ax) {union REGS r; r.x.ax=ax; int86(0x10,&r,&r);}
int main(int argc,char **argv) {
 union REGS r; int win; char path[128],*s;
 r.x.ax=0x1600; int86(0x2F,&r,&r); win=r.h.al!=0&&r.h.al!=0x80;
 strncpy(path,argv[0],sizeof(path)-16); path[sizeof(path)-16]=0; s=strrchr(path,'\\'); if(s) s[1]=0; else path[0]=0; strcat(path,win?"VXDOSD.TXT":"VXDOSR.TXT");
 lf=fopen(path,"w");
 say("VXDOSD v1.0: %s\n",win?"inside Windows (a DOS session)":"plain DOS (no Windows): the reference");
 if(win) say("   Windows %u.%02u\n",r.h.al,r.h.ah);
 (void)argc;
 dump("A. the display as found"); beeps(1);
 ask("A. could you read the text on the screen when VXDOSD started?");
 setmode(0x0003); text_mark("VXDOSD step B: the BIOS set text mode 3. If you can read this, press Y, else N."); dump("B. after the BIOS's text mode 3"); beeps(2);
 ask("B. after two beeps: could you read the text?");
 setmode(0x0012); stripes(); dump("C. after the BIOS's 640 x 480 graphics (mode 12h), stripes drawn"); beeps(3);
 ask("C. after three beeps: black and white stripes?");
 setmode(0x0003); text_mark("VXDOSD is done. Type EXIT and press Enter to go back to Windows."); dump("D. text mode 3 again (the end)");
 tone(660,18);
 say("done\n"); if(lf) fclose(lf);
 printf("VXDOSD done: results in %s\n",path);
 return 0;
}
