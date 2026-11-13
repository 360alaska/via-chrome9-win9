/* VX855 monitor detection (v10.37): reads the monitor EDID over DDC (I2C bit-banged through the SR registers), at driver start.
 * The same bus layout and bit-bang sequence as the routing dump that read the Dell/MStar EDIDs in the lab.
 * Buses (display numbering): 2 = SR31 (DVI/DDC), 1 = SR26 (VGA DDC), 3 = SR2C. The first bus that returns a valid EDID wins.
 * Only used to decide which extra video modes to offer; nothing here changes the display. */
#include "winhack.h"
#include "vmm.h"
#include "vxd_lib.h"
#include "vx85598.h"
#include "3d_accel.h"
#define IO_IN8
#define IO_OUT8
#include "io32.h"

unsigned long VX_irq_save(void);
void VX_irq_restore(unsigned long flags);
#pragma aux VX_irq_save = "pushfd" "pop eax" "cli" value [eax] modify exact [eax];
#pragma aux VX_irq_restore = "push eax" "popfd" parm [eax] modify exact [];

unsigned char vx_edid[128];
int vx_edid_ok;
int vx_edid_bus;

static unsigned char d_reg, d_en, d_mask;
static void d_delay(void) {unsigned i; for(i=0;i<6;i++) (void)inp(0x80);}
static unsigned char d_sr_get(unsigned char i) {
 unsigned long f=VX_irq_save(); unsigned char o=inp(0x3C4),v;
 outp(0x3C4,i); v=inp(0x3C5); outp(0x3C4,o); VX_irq_restore(f); return v;
}
static void d_sr_mask(unsigned char i,unsigned char mask,unsigned char val) {
 unsigned long f=VX_irq_save(); unsigned char o=inp(0x3C4),v;
 outp(0x3C4,i); v=inp(0x3C5); outp(0x3C5,(unsigned char)((v&~mask)|(val&mask))); outp(0x3C4,o); VX_irq_restore(f);
}
static void d_set(int scl,int sda) {d_sr_mask(d_reg,d_mask,(unsigned char)(d_en|(scl?0x20:0)|(sda?0x10:0)));}
static unsigned char d_get(void) { /* bit0 SCL, bit1 SDA */
 unsigned char v;
 if(d_reg!=0x2C) d_sr_mask(d_reg,0x01,0x01);
 v=d_sr_get(d_reg);
 return (unsigned char)(((v&0x08)?1:0)|((v&0x04)?2:0));
}
static void d_start(void) {d_set(1,1); d_delay(); d_set(1,0); d_delay(); d_set(0,0); d_delay();}
static void d_stop(void) {d_set(0,0); d_delay(); d_set(1,0); d_delay(); d_set(1,1); d_delay();}
static int d_put(unsigned char b) { /* 1 = ACK */
 int i,ack;
 for(i=7;i>=0;i--) {d_set(0,(b>>i)&1); d_delay(); d_set(1,(b>>i)&1); d_delay(); d_set(0,(b>>i)&1); d_delay();}
 d_set(0,1); d_delay(); d_set(1,1); d_delay(); ack=(d_get()&2)?0:1; d_set(0,1); d_delay();
 return ack;
}
static unsigned char d_getb(int ack) {
 int i; unsigned char v=0;
 d_set(0,1);
 for(i=0;i<8;i++) {d_delay(); d_set(1,1); d_delay(); v=(unsigned char)((v<<1)|((d_get()&2)?1:0)); d_set(0,1);}
 d_set(0,ack?0:1); d_delay(); d_set(1,ack?0:1); d_delay(); d_set(0,ack?0:1); d_delay(); d_set(0,1);
 return v;
}
static int d_read(unsigned char *e) {
 unsigned int k,sum=0; int ok=0;
 d_set(1,1); d_delay(); d_delay();
 if(d_get()!=3) return 0;                 /* a line is stuck low: nothing usable here */
 d_start();
 if(d_put(0xA0) && d_put(0x00)) {
  d_start();
  if(d_put(0xA1)) {for(k=0;k<128;k++) e[k]=d_getb(k<127); ok=1;}
 }
 d_stop(); d_set(1,1);
 if(!ok) return 0;
 for(k=0;k<128;k++) sum+=e[k];
 if((sum&255)||e[0]||e[1]!=0xFF||e[2]!=0xFF||e[7]) return 0;
 return 1;
}

void VX_edid_probe(void) {
 static const unsigned char regs[3]={0x31,0x26,0x2C}, ens[3]={0x01,0x01,0xC0}, masks[3]={0x31,0x31,0xF0}, num[3]={2,1,3};
 unsigned char unlock,saved,tmp[128]; unsigned int b,t;
 vx_edid_ok=0; vx_edid_bus=0;
 unlock=d_sr_get(0x10);
 d_sr_mask(0x10,0x01,0x01);
 for(b=0;b<3 && !vx_edid_ok;b++) {
  d_reg=regs[b]; d_en=ens[b]; d_mask=masks[b];
  saved=d_sr_get(d_reg);
  for(t=0;t<3 && !vx_edid_ok;t++) {
   if(d_read(tmp)) {memcpy(vx_edid,tmp,128); vx_edid_ok=1; vx_edid_bus=num[b];}
  }
  d_sr_mask(d_reg,0xFF,saved);
 }
 d_sr_mask(0x10,0x01,(unsigned char)(unlock&1));
 dbg_printf("VX85598 EDID ok=%ld bus=%ld\n",(DWORD)vx_edid_ok,(DWORD)vx_edid_bus);
}

/* Does the monitor list this resolution (established, standard or detailed timings), or do its range limits cover it at 60 Hz? */
int VX_edid_mode_ok(unsigned w,unsigned h) {
 static const unsigned short e1[8][2]={{720,400},{720,400},{640,480},{640,480},{640,480},{640,480},{800,600},{800,600}};
 static const unsigned short e2[8][2]={{800,600},{800,600},{832,624},{1024,768},{1024,768},{1024,768},{1024,768},{1280,1024}};
 const unsigned char *e=vx_edid; unsigned i,d;
 if(!vx_edid_ok) return 0;
 for(i=0;i<8;i++) {
  if((e[35]&(0x80>>i)) && e1[i][0]==w && e1[i][1]==h) return 1;
  if((e[36]&(0x80>>i)) && e2[i][0]==w && e2[i][1]==h) return 1;
 }
 if((e[37]&0x80) && w==1152 && h==870) return 1;
 for(i=0;i<8;i++) {
  unsigned a=e[38+2*i], b=e[39+2*i], x, y;
  if(a==1 && b==1) continue;
  x=(a+31)*8;
  switch(b>>6) {case 0: y=(e[18]==1 && e[19]<3)?x:x*10/16; break; case 1: y=x*3/4; break; case 2: y=x*4/5; break; default: y=x*9/16;}
  if(x==w && (y>h?y-h:h-y)<=4) return 1;
 }
 for(d=54;d<=108;d+=18) {
  if(e[d] || e[d+1]) {
   unsigned ha=e[d+2]|((unsigned)(e[d+4]&0xF0)<<4), va=e[d+5]|((unsigned)(e[d+7]&0xF0)<<4);
   if(ha==w && va==h) return 1;
  }
 }
 if(e[54] || e[55]) {   /* an unlisted size larger than the native panel is never offered */
  unsigned nw=e[56]|((unsigned)(e[58]&0xF0)<<4), nh=e[59]|((unsigned)(e[61]&0xF0)<<4);
  if(w>nw || h>nh) return 0;
 }
 for(d=54;d<=108;d+=18) {
  if(!e[d] && !e[d+1] && !e[d+2] && e[d+3]==0xFD) {   /* monitor range limits: assume 60 Hz and typical blanking */
   unsigned vmin=e[d+5], vmax=e[d+6], hmin=e[d+7], hmax=e[d+8], pmax=(unsigned)e[d+9]*10u;
   unsigned vt=h+h/25+8, hf=(60u*vt+500u)/1000u, ht=w+w/4, pix=(hf*ht)/1000u;
   if(60<vmin || 60>vmax || hf<hmin || hf>hmax) return 0;
   if(e[d+9]!=0xFF && e[d+9]!=0 && pix>pmax) return 0;
   return 1;
  }
 }
 return 0;
}

/* Runtime bus scan for diagnosis (VX2EDID asks for it): per bus 12 DWORDs: [0] idle lines (3 = both high), [1] device count, [2..9] 7-bit addresses that
 * answered, [10] 0xA0 (EDID) answered, [11] 1 = a valid EDID was read now. A valid EDID found here is kept as the "late" EDID. */
unsigned char vx_edid_late[128];
int vx_edid_late_ok, vx_edid_late_bus;
void VX_edid_scan(DWORD *o) {
 static const unsigned char regs[3]={0x31,0x26,0x2C}, ens[3]={0x01,0x01,0xC0}, masks[3]={0x31,0x31,0xF0}, num[3]={2,1,3};
 unsigned char unlock,saved,tmp[128]; unsigned int b,a,n; DWORD *q;
 unlock=d_sr_get(0x10);
 d_sr_mask(0x10,0x01,0x01);
 for(b=0;b<3;b++) {
  q=o+12*b; for(a=0;a<12;a++) q[a]=0;
  d_reg=regs[b]; d_en=ens[b]; d_mask=masks[b];
  saved=d_sr_get(d_reg);
  d_set(1,1); d_delay(); d_delay();
  q[0]=d_get();
  if(q[0]==3) {
   n=0;
   for(a=0x08;a<=0x77;a++) {int ack; d_start(); ack=d_put((unsigned char)(a<<1)); d_stop(); if(ack && n<8) q[2+n++]=a;}
   q[1]=n;
   d_start(); q[10]=d_put(0xA0); d_stop();
   if(d_read(tmp)) {q[11]=1; if(!vx_edid_late_ok) {memcpy(vx_edid_late,tmp,128); vx_edid_late_ok=1; vx_edid_late_bus=num[b];}}
  }
  d_set(1,1);
  d_sr_mask(d_reg,0xFF,saved);
 }
 d_sr_mask(0x10,0x01,(unsigned char)(unlock&1));
}

/* Take an EDID block obtained some other way (the BIOS VBE/DDC call). bus 4 = BIOS. */
int VX_edid_set(const unsigned char *e,int bus) {
 unsigned k,sum=0;
 for(k=0;k<128;k++) sum+=e[k];
 if((sum&255)||e[0]||e[1]!=0xFF||e[2]!=0xFF||e[7]) return 0;
 memcpy(vx_edid,e,128); vx_edid_ok=1; vx_edid_bus=bus;
 return 1;
}

/* Read-only register dump for VX2CRT: SR00..FF and CR00..FF. SR10 bit 0 is set while reading (extended registers need it) and restored. */
void VX_crt_dump(DWORD *out) {
 unsigned long f; unsigned char unlock,misc; unsigned short base; unsigned i;
 unlock=d_sr_get(0x10);
 d_sr_mask(0x10,0x01,0x01);
 misc=inp(0x3CC); base=(misc&1)?0x3D4:0x3B4;
 out[1]=misc; out[2]=base; out[3]=unlock;
 for(i=0;i<256;i++) out[4+i]=d_sr_get((unsigned char)i);
 for(i=0;i<256;i++) {
  unsigned char o,v;
  f=VX_irq_save(); o=inp(base); outp(base,(unsigned char)i); v=inp((unsigned short)(base+1)); outp(base,o); VX_irq_restore(f);
  out[260+i]=v;
 }
 d_sr_mask(0x10,0x01,(unsigned char)(unlock&1));
}
