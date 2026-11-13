/* Win98 VxD native-engine test endpoint. Copyright (c) 2026, MIT. */
#include "winhack.h"
#include "vmm.h"
#include "vxd_lib.h"
#include "vx85598.h"
#include "vx855_3d.h"
#define IO_IN8
#define IO_OUT8
#define IO_IN32
#define IO_OUT32
#include "io32.h"

void vx_lock(void);void vx_unlock(void); /* .515 (vx855_2d.c) */
unsigned long VX_irq_save(void);
void VX_irq_restore(unsigned long flags);
#pragma aux VX_irq_save = "pushfd" "pop eax" "cli" value [eax] modify exact [eax];
#pragma aux VX_irq_restore = "push eax" "popfd" parm [eax] modify exact [];

static const unsigned char seq_index[VX3D_SEQ_COUNT]={0x10,0x1A,0x1E,0x2D,0x3F,0x4E,0x3C};
static unsigned char seq_read(unsigned char index) {
 outp(0x3C4,index); return inp(0x3C5);
}
static void seq_write(unsigned char index,unsigned char value) {
 outp(0x3C4,index); outp(0x3C5,value);
}
static void seq_snapshot(uint32_t *values) {
 unsigned int i;
 for(i=0;i<VX3D_SEQ_COUNT;i++) values[i]=seq_read(seq_index[i]);
}
static int enable_engine(vx3d_report *r) {
 unsigned long flags=VX_irq_save();
 unsigned char old_index=inp(0x3C4),unlock=seq_read(0x10);
 unsigned long start;
 seq_write(0x10,(unsigned char)(unlock|1));
 seq_snapshot(r->seq_before); r->seq_before[0]=unlock;
 r->init_flags=1;
 if((r->seq_before[1]&0x42) || (r->seq_before[5]&0x30)) {
  /* Never deassert a BIOS-held reset as part of an offscreen test. */
  r->init_flags|=32|4;
  memcpy(r->seq_after,r->seq_before,sizeof(r->seq_before));
  seq_write(0x10,unlock); outp(0x3C4,old_index);
  VX_irq_restore(flags); return 0;
 }
 /* VX855 OGPM part I: SR1A[3] enables extended memory/MMIO access;
  * SR1E[0] enables ROC ECK; SR2D[1:0]=10 keeps ECK PLL powered;
  * SR3F[7:2]=101010 keeps CR/3D/2D clocks on for this explicit test.
  * Preserve all other fields, clock frequencies and display clocks. */
 r->init_flags|=64;
 seq_write(0x1A,(unsigned char)(r->seq_before[1]|0x08));
 seq_write(0x1E,(unsigned char)(r->seq_before[2]|0x01));
 /* v9.4: SR2D/SR3F are left exactly as the display driver set them (FF/FF).
  * v9.4 rewrote SR3F FF->AB and SR2D FF->FE, which may gate engine clocks. */
 seq_snapshot(r->seq_active);
 outp(0x3C4,old_index); VX_irq_restore(flags);
 if(!(r->seq_active[1]&8) || !(r->seq_active[2]&1) ||
    0) {
  r->init_flags|=16; return 0;
 }
 /* The ECK PLL may have been powered off by the BIOS. Poll its documented
  * lock detector with interrupts enabled and both a time and count limit. */
 start=Get_System_Time();
 for(r->pll_polls=0;r->pll_polls<65536UL;r->pll_polls++) {
  flags=VX_irq_save(); old_index=inp(0x3C4);
  r->seq_active[6]=seq_read(0x3C);
  outp(0x3C4,old_index); VX_irq_restore(flags);
  if(r->seq_active[6]&0x10) {r->init_flags|=2; return 1;}
  if((uint32_t)(Get_System_Time()-start)>=VX3D_TIMEOUT_MS) break;
 }
 r->init_flags|=256; return 0;
}
static void restore_engine(vx3d_report *r) {
 unsigned long flags=VX_irq_save();
 unsigned char old_index=inp(0x3C4);
 /* Return to the saved settings only after the GPU is confirmed idle. */
 seq_write(0x3F,(unsigned char)r->seq_before[4]);
 seq_write(0x2D,(unsigned char)r->seq_before[3]);
 seq_write(0x1E,(unsigned char)r->seq_before[2]);
 seq_write(0x1A,(unsigned char)r->seq_before[1]);
 seq_snapshot(r->seq_after);
 seq_write(0x10,(unsigned char)r->seq_before[0]);
 r->seq_after[0]=seq_read(0x10);
 outp(0x3C4,old_index); VX_irq_restore(flags); r->init_flags|=4;
 if(((r->seq_after[0]^r->seq_before[0])&1) ||
    ((r->seq_after[1]^r->seq_before[1])&8) ||
    ((r->seq_after[2]^r->seq_before[2])&1) ||
    ((r->seq_after[3]^r->seq_before[3])&3) ||
    ((r->seq_after[4]^r->seq_before[4])&0xFC)) {
  r->init_flags|=128; r->result=VX3D_INIT_FAILURE;
 }
}

static unsigned long mmio_linear;
static uint32_t arena_backup[VX3D_ARENA_BYTES/4];
static uint32_t queue_backup[VX3D_QUEUE_SPAN/4];
static uint32_t sf_table_backup[VX3D_SF_TABLE_SPAN/4];
static unsigned long sf_allocation;
static uint32_t fault_latched, running;
static uint32_t read_mmio(void *unused,uint32_t at) {
 (void)unused; return *(volatile uint32_t *)(mmio_linear+at);
}
static void write_mmio(void *unused,uint32_t at,uint32_t value) {
 (void)unused; *(volatile uint32_t *)(mmio_linear+at)=value;
}
static uint32_t read_fb(void *unused,uint32_t at) {
 (void)unused; if(at>vx_status.fb_bytes || vx_status.fb_bytes-at<4) return 0xBAD0BAD0UL; /* v9.91: never fault outside the mapped video memory */
 return *(volatile uint32_t *)(vx_status.fb_linear+at);
}
static void write_fb(void *unused,uint32_t at,uint32_t value) {
 (void)unused; if(at>vx_status.fb_bytes || vx_status.fb_bytes-at<4) return;
 *(volatile uint32_t *)(vx_status.fb_linear+at)=value;
}
static uint32_t milliseconds(void *unused) {
 (void)unused; return Get_System_Time();
}
static uint32_t sf_read_host(void *context,uint32_t at) {
 return *(volatile uint32_t *)((unsigned long)context+at);
}
static void sf_write_host(void *context,uint32_t at,uint32_t value) {
 *(volatile uint32_t *)((unsigned long)context+at)=value;
}
static uint32_t sf_read_seq(void *unused,uint32_t index) {
 unsigned long flags=VX_irq_save();
 unsigned char old=inp(0x3C4),value;
 (void)unused; value=seq_read((unsigned char)index);
 outp(0x3C4,old); VX_irq_restore(flags); return value;
}
static void sf_write_seq(void *unused,uint32_t index,uint32_t value) {
 unsigned long flags=VX_irq_save();
 unsigned char old=inp(0x3C4);
 (void)unused; seq_write((unsigned char)index,(unsigned char)value);
 outp(0x3C4,old); VX_irq_restore(flags);
}
static void VX_cache_publish(void);
#pragma aux VX_cache_publish = "wbinvd" modify exact [];
static void sf_publish(void *unused) {
 unsigned long flags=VX_irq_save();
 (void)unused; VX_cache_publish(); VX_irq_restore(flags);
}
static int sf_allocate(vx3d_report *r) {
 uint32_t i,j;
 unsigned long physical=0;
 if(sf_allocation) return 0;
 sf_allocation=_PageAllocate(VX3D_SF_PAGES,PG_SYS,0,0,0x100,0xFFFFF,
  &physical,PAGEFIXED|PAGEUSEALIGN);
 if(!sf_allocation || sf_allocation==0xFFFFFFFFUL) {sf_allocation=0; return 0;}
 r->sf_flags|=1; r->sf_host_linear=sf_allocation; r->sf_host_bytes=VX3D_SF_BYTES;
 if(sf_allocation&4095) goto bad;
 /* The output entries, not a guessed allocator physical return value,
  * identify each fixed page. Validate all entries before touching GTI. */
 _CopyPageTable(sf_allocation>>12,VX3D_SF_PAGES,(DWORD*)r->sf_host_pte,0);
 for(i=0;i<VX3D_SF_PAGES;i++) {
  if((r->sf_host_pte[i]&3)!=3 || (r->sf_host_pte[i]&0xFFFFF000UL)<0x100000UL) goto bad;
  for(j=0;j<i;j++) if(!((r->sf_host_pte[i]^r->sf_host_pte[j])&0xFFFFF000UL)) goto bad;
 }
 r->sf_flags|=2; return 1;
bad:
 _PageFree((void*)sf_allocation,0); sf_allocation=0; r->sf_flags|=0x20000;
 return 0;
}
static uint32_t sf_pci_read(unsigned long offset) {
 unsigned long flags=VX_irq_save(),saved=inpd(0xCF8),value;
 outpd(0xCF8,vx_status.pci_address|offset);
 value=inpd(0xCFC);
 outpd(0xCF8,saved); VX_irq_restore(flags); return value;
}
static void diagnostic_snapshot(uint32_t *out) {
 out[0]=read_mmio(NULL,0x400); out[1]=read_mmio(NULL,0x43C);
 out[2]=read_mmio(NULL,0x444); out[3]=read_mmio(NULL,0x494);
}
static unsigned long prepare(vx3d_report *r,uint32_t probe) {
 unsigned long base;
 uint32_t reserved_end=probe>=VX3D_TRANSPORT_FIRST?VX3D_QUEUE_REGION_END:VX3D_ARENA+VX3D_ARENA_BYTES;
 if(probe==VX3D_SF_PROBE) reserved_end=VX3D_SF_TABLE_END;
 memset(r,0,sizeof(*r));
 r->magic=VX3D_MAGIC; r->cb=sizeof(*r); r->version=VX3D_VERSION;
 r->capabilities=1; r->fault_latched=fault_latched;
 r->build=VX3D_BUILD; r->producer=VX3D_PRODUCER;
 if(fault_latched) { r->result=VX3D_DISABLED; return 0; }
 if(vx_status.stage!=6 || vx_status.failure || vx_status.vga_mode ||
  !vx_status.fb_linear || vx_status.fb_bytes<reserved_end ||
  !vx_status.height || vx_status.video_pitch>VX3D_ARENA/vx_status.height ||
  Get_Cur_VM_Handle()!=Get_Sys_VM_Handle()) {
  r->result=VX3D_NOT_READY; return 0;
 }
 if(!VESA_native_span(&r->display_begin,&r->display_end) ||
    (r->display_begin<reserved_end &&
     r->display_end>VX3D_ARENA)) {
  r->result=VX3D_NOT_READY; return 0;
 }
 if(probe==VX3D_SF_PROBE) {
  r->diagnostic_pci[0]=sf_pci_read(0x10);
  {extern unsigned long vx_chip_mmio_cfg;r->diagnostic_pci[1]=sf_pci_read(vx_chip_mmio_cfg?vx_chip_mmio_cfg:0x14);} /* .579: the register window's BAR of this device */
  r->diagnostic_pci[2]=sf_pci_read(0x18);
  r->diagnostic_pci[3]=sf_pci_read(4);
  r->sf_pci_command=r->diagnostic_pci[3]&0xFFFF;
  if((r->diagnostic_pci[1]&0xFFFFFFF0UL)!=(vx_status.bar1&0xFFFFFFF0UL)) {
   r->result=VX3D_BAD_BAR; return 0;
  }
  /* The pinned-RAM reader is a PCI bus master. A framebuffer-only mode
   * working does not establish this prerequisite. Do not silently alter
   * PCI configuration; report the current command and stop before DMA. */
  if((r->sf_pci_command&6)!=6) {r->result=VX3D_NOT_READY; return 0;}
 }
 base=vx_status.bar1&0xFFFFFFF0UL;
 if((vx_status.bar1&15)!=0 || base<0x100000UL || base>0xFFFFEFFFUL) {
  r->result=VX3D_BAD_BAR; return 0;
 }
 if(!mmio_linear) {
  mmio_linear=_MapPhysToLinear(base,0x400000UL,0);
  if(!mmio_linear || mmio_linear==0xFFFFFFFFUL) {
   mmio_linear=0; r->result=VX3D_NO_MMIO; return 0;
  }
 }
 r->status_before=read_mmio(NULL,0x400);
 if(probe==VX3D_SF_PROBE) diagnostic_snapshot(r->diagnostic_mmio[0]);
 r->status_after=r->status_before;
 if(r->status_before==0xFFFFFFFFUL || (r->status_before&VX3D_BUSY_MASK)) {
  r->result=VX3D_BUSY; return 0;
 }
 r->result=VX3D_NOT_RUN;
 return 1;
}
int VX2D_busy(void);
int VX3DN_busy(void);
extern volatile uint32_t vx3dn_rt_why; /* v10.473 */
int VX3D_busy(void) {if(running||fault_latched){vx3dn_rt_why=0;return 1;}return VX3DN_busy();} /* v10.476: the retire cause's tag cleared on the early answer too (it was left for the next retire) */
int VX3D_lab_busy(void) {return running || fault_latched;} /* v10.231: without VX3DN_busy's retire (the native submit's own entry) */
unsigned long VX3D_probe(vx3d_report *r) {
 prepare(r,VX3D_TRANSPORT_FIRST); return 0;
}
static uint32_t recover_info[4];
/* v9.39: try to bring a stalled 3D engine back inside the same boot:
 * HCM SR4E bits 5:4 = 01 is a 3D engine reset. Only if the engine then reports
 * idle is the fault latch cleared and the pinned test memory released. */
static void recover_engine(void) {
 unsigned long flags; unsigned char old,unlock,sr; uint32_t st,start;
 recover_info[0]=read_mmio(NULL,0x400); recover_info[1]=0; recover_info[2]=0; recover_info[3]=1;
 flags=VX_irq_save(); old=inp(0x3C4); unlock=seq_read(0x10); seq_write(0x10,(unsigned char)(unlock|1));
 sr=seq_read(0x4E); seq_write(0x4E,(unsigned char)((sr&~0x30)|0x10));
 VX_irq_restore(flags);
 start=Get_System_Time(); while((uint32_t)(Get_System_Time()-start)<5) (void)read_mmio(NULL,0x400);
 flags=VX_irq_save(); seq_write(0x4E,(unsigned char)(sr&~0x30)); VX_irq_restore(flags);
 start=Get_System_Time(); while((uint32_t)(Get_System_Time()-start)<5) (void)read_mmio(NULL,0x400);
 st=read_mmio(NULL,0x400);
 if(st&VX3D_BUSY_MASK) {
  /* v9.40: still busy after the engine reset: pulse SR1A bit 6, the 3D ECK software reset. */
  unsigned char s1a;
  flags=VX_irq_save(); s1a=seq_read(0x1A); seq_write(0x1A,(unsigned char)(s1a|0x40)); VX_irq_restore(flags);
  start=Get_System_Time(); while((uint32_t)(Get_System_Time()-start)<5) (void)read_mmio(NULL,0x400);
  flags=VX_irq_save(); seq_write(0x1A,(unsigned char)(s1a&~0x40)); VX_irq_restore(flags);
  start=Get_System_Time(); while((uint32_t)(Get_System_Time()-start)<5) (void)read_mmio(NULL,0x400);
  recover_info[3]=2;
 }
 flags=VX_irq_save(); seq_write(0x10,unlock); outp(0x3C4,old); VX_irq_restore(flags);
 start=Get_System_Time();
 do {st=read_mmio(NULL,0x400);} while((st&VX3D_BUSY_MASK) && st!=0xFFFFFFFFUL && (uint32_t)(Get_System_Time()-start)<250);
 recover_info[1]=st;
 if(!(st&VX3D_BUSY_MASK) && st!=0xFFFFFFFFUL) {
  recover_info[2]=1; fault_latched=0;
  if(sf_allocation) {_PageFree((void*)sf_allocation,0); sf_allocation=0;}
 }
}

/* v10.12: read-only display routing dump (lab flag 1048576). No video memory is touched, so it works with any BIOS frame-buffer size.
 * image[0]=1 (format), [1]=misc output 3CC, [2]=CRTC base port, [3]=SR10 as found, [4]=VX855 PCI id dword, [5]=video pitch, [6]=height, [7]=bpp,
 * [8]=display_begin, [9]=display_end, [10]=fb_bytes mapped, [11]=chip fb bytes (host bridge), [12]=VBE total bytes, [13]=map bytes asked
 * image[16+i]=SR i, image[272+i]=CR i, image[528+i]=PCI config dword i (0..63)
 * I2C buses b=0..2 (SR26, SR31, SR2C) at image[600+200*b]: [0]=idle lines (bit0 SCL, bit1 SDA), [1]=device count, [2..33]=7-bit addresses that ACKed,
 *  [34]=EDID bytes read, [35]=EDID status (0 ok, 1 no ack at 50, 2 no ack at 51, 3 data nack), [40..71]=EDID 128 bytes as dwords,
 *  [80]=VT1632 (7-bit 0x08 = 8-bit 0x10) ack, [81..96]=its regs 0..15, [100]=SiI164 (0x38) ack, [101..108]=regs 0..7 */
static void rt_delay(void) {unsigned int i; for(i=0;i<6;i++) (void)inp(0x80);}
static unsigned char rt_sr_get(unsigned char i) {unsigned long f=VX_irq_save(); unsigned char o=inp(0x3C4),v; outp(0x3C4,i); v=inp(0x3C5); outp(0x3C4,o); VX_irq_restore(f); return v;}
static void rt_sr_mask(unsigned char i,unsigned char mask,unsigned char val) {unsigned long f=VX_irq_save(); unsigned char o=inp(0x3C4),v; outp(0x3C4,i); v=inp(0x3C5); outp(0x3C5,(unsigned char)((v&~mask)|(val&mask))); outp(0x3C4,o); VX_irq_restore(f);}
static unsigned char rt_cr_get(unsigned short base,unsigned char i) {unsigned long f=VX_irq_save(); unsigned char o=inp(base),v; outp(base,i); v=inp((unsigned short)(base+1)); outp(base,o); VX_irq_restore(f); return v;}
static unsigned char rt_reg,rt_en,rt_mask;
static void rt_set(int scl,int sda) {rt_sr_mask(rt_reg,rt_mask,(unsigned char)(rt_en|(scl?0x20:0)|(sda?0x10:0)));}
static unsigned char rt_get(void) { /* bit0 SCL, bit1 SDA */
 unsigned char v;
 if(rt_reg!=0x2C) rt_sr_mask(rt_reg,0x01,0x01);
 v=rt_sr_get(rt_reg);
 return (unsigned char)(((v&0x08)?1:0)|((v&0x04)?2:0));
}
static void rt_start(void) {rt_set(1,1); rt_delay(); rt_set(1,0); rt_delay(); rt_set(0,0); rt_delay();}
static void rt_stop(void) {rt_set(0,0); rt_delay(); rt_set(1,0); rt_delay(); rt_set(1,1); rt_delay();}
static int rt_put_byte(unsigned char b) { /* returns 1 if ACKed */
 int i,ack;
 for(i=7;i>=0;i--) {rt_set(0,(b>>i)&1); rt_delay(); rt_set(1,(b>>i)&1); rt_delay(); rt_set(0,(b>>i)&1); rt_delay();}
 rt_set(0,1); rt_delay(); rt_set(1,1); rt_delay(); ack=(rt_get()&2)?0:1; rt_set(0,1); rt_delay();
 return ack;
}
static unsigned char rt_get_byte(int ack) {
 int i; unsigned char v=0;
 rt_set(0,1);
 for(i=0;i<8;i++) {rt_delay(); rt_set(1,1); rt_delay(); v=(unsigned char)((v<<1)|((rt_get()&2)?1:0)); rt_set(0,1);}
 rt_set(0,ack?0:1); rt_delay(); rt_set(1,ack?0:1); rt_delay(); rt_set(0,ack?0:1); rt_delay(); rt_set(0,1);
 return v;
}
static int rt_probe(unsigned char a7) {int ack; rt_start(); ack=rt_put_byte((unsigned char)(a7<<1)); rt_stop(); return ack;}
static int rt_read_regs(unsigned char a7,unsigned char first,unsigned int n,uint32_t *out) { /* write index, restart, read n bytes */
 unsigned int i;
 rt_start();
 if(!rt_put_byte((unsigned char)(a7<<1))) {rt_stop(); return 0;}
 rt_put_byte(first);
 rt_start();
 if(!rt_put_byte((unsigned char)((a7<<1)|1))) {rt_stop(); return 0;}
 for(i=0;i<n;i++) out[i]=rt_get_byte(i+1<n);
 rt_stop(); return 1;
}
static void vx_route_dump(vx3d_report *r) {
 static const unsigned char regs[3]={0x26,0x31,0x2C}, ens[3]={0x01,0x01,0xC0}, masks[3]={0x31,0x31,0xF0};
 unsigned long f; unsigned char unlock,misc; unsigned short base; unsigned int i,b,a,ndev; uint32_t *im=r->image,tmp[128];
 unsigned char saved[3];
 f=VX_irq_save(); unlock=rt_sr_get(0x10); VX_irq_restore(f);
 rt_sr_mask(0x10,0x01,0x01);
 misc=inp(0x3CC); base=(misc&1)?0x3D4:0x3B4;
 im[0]=1; im[1]=misc; im[2]=base; im[3]=unlock; im[4]=sf_pci_read(0);
 im[5]=vx_status.video_pitch; im[6]=vx_status.height; im[7]=vx_status.bpp;
 im[8]=r->display_begin; im[9]=r->display_end; im[10]=vx_status.fb_bytes; im[11]=vx_chip_fb_bytes; im[12]=vx_vbe_total_bytes; im[13]=vx_map_bytes;
 {unsigned char s5a=rt_sr_get(0x5A); rt_sr_mask(0x5A,1,1); im[14]=rt_sr_get(0x12); im[15]=rt_sr_get(0x13); rt_sr_mask(0x5A,1,(unsigned char)(s5a&1));}
 for(i=0;i<256;i++) im[16+i]=rt_sr_get((unsigned char)i);
 for(i=0;i<256;i++) im[272+i]=rt_cr_get(base,(unsigned char)i);
 for(i=0;i<64;i++) im[528+i]=sf_pci_read(i*4UL);
 for(b=0;b<3;b++) saved[b]=rt_sr_get(regs[b]);
 for(b=0;b<3;b++) {
  uint32_t *o=im+600+200*b;
  rt_reg=regs[b]; rt_en=ens[b]; rt_mask=masks[b];
  rt_set(1,1); rt_delay(); rt_delay();
  o[0]=rt_get(); ndev=0;
  if(o[0]==3) {
   for(a=0x08;a<=0x77;a++) if(rt_probe((unsigned char)a) && ndev<32) o[2+ndev++]=a;
   o[1]=ndev;
   o[35]=1;
   if(rt_probe(0x50)) {
    o[35]=2;
    rt_start();
    if(rt_put_byte(0xA0) && rt_put_byte(0x00)) {
     unsigned int k;
     rt_start();
     if(rt_put_byte(0xA1)) {
      for(k=0;k<128;k++) ((unsigned char*)tmp)[k]=rt_get_byte(k<127);
      o[34]=128; o[35]=0; for(k=0;k<32;k++) o[40+k]=tmp[k];
     }
    }
    rt_stop();
   }
   if(rt_read_regs(0x08,0,16,tmp)) {o[80]=1; for(i=0;i<16;i++) o[81+i]=tmp[i];}
   if(rt_read_regs(0x38,0,8,tmp)) {o[100]=1; for(i=0;i<8;i++) o[101+i]=tmp[i];}
  } else o[1]=0xFFFF;
  rt_set(1,1);
  rt_sr_mask(regs[b],0xFF,saved[b]);
 }
 rt_sr_mask(0x10,0x01,(unsigned char)(unlock&1));
 r->result=VX3D_PASS;
}

/* v10.13: IGA2 -> DVP1 test with automatic restore. lab_flags 2097152 = apply variant lab_sy (1..4), pattern base lab_sx (0 = automatic);
 * lab_flags 4194304 = restore. IGA1 and the analog output are never touched.
 * Apply reply: image[0]=status (1 ok, 2 memory too small, 3 memory check failed, 4 bad variant), image[1]=pattern base, image[2]=usable memory used for the choice,
 * image[16+k]=CR list readback after apply, image[64+k]=SR list readback, image[100+k]=same lists as found before. */
static const unsigned char i2_crl[35]={0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57,0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F,0x62,0x63,0x64,0x65,0x66,0x67,0x6A,0x6B,0x6C,0x79,0x97,0x99,0x9B,0xA3,0xD2,0x68,0x92,0x94,0x95};
static const unsigned char i2_srl[8]={0x16,0x1B,0x1E,0x2D,0x4A,0x4B,0x4C,0x65};
static unsigned char i2_crv[35],i2_srv[8],i2_sr10,i2_have;
static void rt_cr_mask(unsigned short base,unsigned char i,unsigned char mask,unsigned char val) {
 unsigned long f=VX_irq_save(); unsigned char o=inp(base),v; outp(base,i); v=inp((unsigned short)(base+1));
 outp((unsigned short)(base+1),(unsigned char)((v&~mask)|(val&mask))); outp(base,o); VX_irq_restore(f);
}
static unsigned char i2_sr31,i2_txv[16]; static int i2_txok;
static int rt_write_reg(unsigned char a7,unsigned char reg,unsigned char val) {
 int ok; rt_start(); ok=rt_put_byte((unsigned char)(a7<<1)); if(ok) ok=rt_put_byte(reg); if(ok) ok=rt_put_byte(val); rt_stop(); return ok;
}
static void vx_iga2_restore(unsigned short base) {
 unsigned int k;
 if(!i2_have) return;
 rt_sr_mask(0x10,0x01,0x01);
 rt_cr_mask(base,0x9B,0xFF,i2_crv[28]);
 rt_cr_mask(base,0x6A,0xFF,i2_crv[22]);
 rt_cr_mask(base,0x6B,0xFF,i2_crv[23]);
 for(k=0;k<35;k++) rt_cr_mask(base,i2_crl[k],0xFF,i2_crv[k]);
 for(k=0;k<8;k++) rt_sr_mask(i2_srl[k],0xFF,i2_srv[k]);
 rt_sr_mask(0x40,0x04,0x04); rt_sr_mask(0x40,0x04,0x00);
 if(i2_txok) {
  rt_reg=0x31; rt_en=0x01; rt_mask=0x31; rt_set(1,1);
  rt_write_reg(0x08,0x0C,(unsigned char)(i2_txv[0x0C]==0xFF?0x00:i2_txv[0x0C])); rt_write_reg(0x08,0x0A,(unsigned char)(i2_txv[0x0A]==0xFF?0x00:i2_txv[0x0A])); rt_write_reg(0x08,0x09,i2_txv[0x09]); rt_write_reg(0x08,0x08,i2_txv[0x08]);
  rt_set(1,1); i2_txok=0;
 }
 rt_sr_mask(0x31,0xFF,i2_sr31); /* v10.330 (review #200): SR31 (set up for the transmitter's I2C bus) put back whether or not the transmitter answered */
 rt_sr_mask(0x10,0x01,(unsigned char)(i2_sr10&1));
 i2_have=0;
}
typedef struct {unsigned short hd,hss,hse,ht,vd,vss,vse,vt,dm; unsigned char dn,dr,hneg,vneg;} iga2_mode;
static const iga2_mode i2_modes[4]={
 {1280,1390,1430,1650,720,725,730,750,83,2,3,0,0},    /* 1: 1280x720 60 Hz, 74.25 MHz, positive syncs */
 {1920,2008,2052,2200,1080,1084,1089,1125,83,2,2,0,0}, /* 2: 1920x1080 60 Hz, 148.5 MHz, positive syncs */
 {1280,1390,1430,1650,720,725,730,750,83,2,3,0,0},    /* 3: as 1 */
 {1280,1390,1430,1650,720,725,730,750,83,2,3,0,0}     /* 4: as 1 */
};
static iga2_mode i2_nm; /* 5: native mode taken from the monitor EDID */
static int rt_read_edid(unsigned char *e) {
 unsigned int k,sum=0; int ok=0; unsigned char s31=rt_sr_get(0x31);
 rt_reg=0x31; rt_en=0x01; rt_mask=0x31; rt_set(1,1); rt_delay(); rt_delay();
 rt_start();
 if(rt_put_byte(0xA0) && rt_put_byte(0x00)) {
  rt_start();
  if(rt_put_byte(0xA1)) {for(k=0;k<128;k++) e[k]=rt_get_byte(k<127); ok=1;}
 }
 rt_stop(); rt_set(1,1); rt_sr_mask(0x31,0xFF,s31);
 if(!ok) return 0;
 for(k=0;k<128;k++) sum+=e[k];
 if((sum&255)||e[0]||e[1]!=0xFF||e[7]) return 0;
 return 1;
}
/* PLL solver as OpenChrome ViaComputeProDotClock (clk in kHz); fills dm/dn/dr */
static int i2_pll_solve(unsigned long clk,iga2_mode *m) {
 unsigned long fout=clk,dr=0,dn,dm,maxdn,best=0xFFFFFFFFUL,bdm=0,bdn=0,bdr=0,f,err;
 if(!clk) return 0;
 while(fout*(1UL<<dr)<300000UL && dr<8) dr++;
 if(dr>=8) return 0;
 maxdn=clk<30000UL?8:(clk<45000UL?7:(clk<170000UL?6:5));
 for(dn=2;dn<maxdn;dn++) for(dm=2;dm<1024;dm++) {
  f=(14318UL*dm)/(dn<<dr); err=(f>fout?f-fout:fout-f); if(err>=fout) continue; err=err*10000UL/fout;
  if(err<best && err<50UL) {best=err;bdm=dm;bdn=dn;bdr=dr;}
 }
 if(best==0xFFFFFFFFUL) return 0;
 m->dm=(unsigned short)bdm; m->dn=(unsigned char)bdn; m->dr=(unsigned char)bdr;
 return 1;
}
/* extra standard modes (VESA DMT) that the Dell E2210 lists in its EDID; variants 6..14 */
typedef struct {unsigned long clk; unsigned short hd,hss,hse,ht,vd,vss,vse,vt; unsigned char hneg,vneg;} iga2_ext;
static const iga2_ext i2_ext[9]={
 {25175UL,640,656,752,800,480,490,492,525,1,1},       /* 6: 640x480 60 Hz */
 {40000UL,800,840,968,1056,600,601,605,628,0,0},      /* 7: 800x600 60 Hz */
 {65000UL,1024,1048,1184,1344,768,771,777,806,1,1},   /* 8: 1024x768 60 Hz */
 {108000UL,1280,1328,1440,1688,1024,1025,1028,1066,0,0}, /* 9: 1280x1024 60 Hz */
 {78750UL,1024,1040,1136,1312,768,769,772,800,0,0},   /* 10: 1024x768 75 Hz */
 {135000UL,1280,1296,1440,1688,1024,1025,1028,1066,0,0}, /* 11: 1280x1024 75 Hz */
 {49500UL,800,816,896,1056,600,601,604,625,0,0},      /* 12: 800x600 75 Hz */
 {28322UL,720,738,846,900,400,413,415,449,1,0},       /* 13: 720x400 70 Hz */
 {108000UL,1152,1216,1344,1600,864,865,868,900,0,0}   /* 14: 1152x864 75 Hz */
};
static iga2_mode i2_xm;
/* first detailed timing descriptor -> mode, PLL solved as in OpenChrome ViaComputeProDotClock; im[190..] receives what was found */
static int i2_native(const unsigned char *e,iga2_mode *m,uint32_t *im) {
 const unsigned char *d=e+54; unsigned long clk=(unsigned long)(d[0]|(d[1]<<8))*10UL,fout,dr=0,dn,dm,maxdn,best=0xFFFFFFFFUL,bdm=0,bdn=0,bdr=0,f,err;
 unsigned long hact,hbl,vact,vbl,hso,hsw,vso,vsw;
 if(!clk||(d[17]&0x80)) return 0;
 hact=d[2]|((unsigned long)(d[4]&0xF0)<<4); hbl=d[3]|((unsigned long)(d[4]&0x0F)<<8);
 vact=d[5]|((unsigned long)(d[7]&0xF0)<<4); vbl=d[6]|((unsigned long)(d[7]&0x0F)<<8);
 hso=d[8]|((unsigned long)(d[11]&0xC0)<<2); hsw=d[9]|((unsigned long)(d[11]&0x30)<<4);
 vso=((d[10]&0xF0)>>4)|((unsigned long)(d[11]&0x0C)<<2); vsw=(d[10]&0x0F)|((unsigned long)(d[11]&0x03)<<4);
 if(clk>165000UL||hact+hbl>4096UL||hact>2048UL||vact+vbl>2048UL||!hact||!vact) return 0;
 fout=clk;
 while(fout*(1UL<<dr)<300000UL && dr<8) dr++;
 if(dr>=8) return 0;
 maxdn=clk<30000UL?8:(clk<45000UL?7:(clk<170000UL?6:5));
 for(dn=2;dn<maxdn;dn++) for(dm=2;dm<1024;dm++) {
  f=(14318UL*dm)/(dn<<dr); err=(f>fout?f-fout:fout-f); if(err>=fout) continue; err=err*10000UL/fout;
  if(err<best && err<50UL) {best=err;bdm=dm;bdn=dn;bdr=dr;}
 }
 if(best==0xFFFFFFFFUL) return 0;
 m->hd=(unsigned short)hact; m->hss=(unsigned short)(hact+hso); m->hse=(unsigned short)(hact+hso+hsw); m->ht=(unsigned short)(hact+hbl);
 m->vd=(unsigned short)vact; m->vss=(unsigned short)(vact+vso); m->vse=(unsigned short)(vact+vso+vsw); m->vt=(unsigned short)(vact+vbl);
 m->dm=(unsigned short)bdm; m->dn=(unsigned char)bdn; m->dr=(unsigned char)bdr;
 m->hneg=(unsigned char)((d[17]&0x18)==0x18 ? ((d[17]&0x02)?0:1) : 0); m->vneg=(unsigned char)((d[17]&0x18)==0x18 ? ((d[17]&0x04)?0:1) : 0);
 im[190]=1; im[191]=clk; im[192]=m->hd; im[193]=m->hss; im[194]=m->hse; im[195]=m->ht; im[196]=m->vd; im[197]=m->vss; im[198]=m->vse; im[199]=m->vt;
 im[200]=m->hneg|(m->vneg<<1); im[201]=bdm; im[202]=bdn; im[203]=bdr; im[204]=14318UL*bdm/(bdn<<bdr);
 return 1;
}
static void i2_timing(unsigned short cb,const iga2_mode *m) {
 unsigned long t;
 t=m->ht-1UL; rt_cr_mask(cb,0x50,0xFF,(unsigned char)t); rt_cr_mask(cb,0x55,0x0F,(unsigned char)(t>>8));
 t=m->hd-1UL; rt_cr_mask(cb,0x51,0xFF,(unsigned char)t); rt_cr_mask(cb,0x55,0x70,(unsigned char)(t>>4));
 t=m->hd-1UL; rt_cr_mask(cb,0x52,0xFF,(unsigned char)t); rt_cr_mask(cb,0x54,0x07,(unsigned char)(t>>8));
 t=m->ht-1UL; rt_cr_mask(cb,0x53,0xFF,(unsigned char)t); rt_cr_mask(cb,0x54,0x38,(unsigned char)(t>>5)); rt_cr_mask(cb,0x5D,0x40,(unsigned char)(t>>5));
 t=m->hss; rt_cr_mask(cb,0x56,0xFF,(unsigned char)t); rt_cr_mask(cb,0x54,0xC0,(unsigned char)(t>>2)); rt_cr_mask(cb,0x5C,0x80,(unsigned char)(t>>3)); rt_cr_mask(cb,0x5D,0x80,(unsigned char)(t>>4));
 t=m->hse-1UL; rt_cr_mask(cb,0x57,0xFF,(unsigned char)t); rt_cr_mask(cb,0x5C,0x40,(unsigned char)(t>>2));
 t=m->vt-1UL; rt_cr_mask(cb,0x58,0xFF,(unsigned char)t); rt_cr_mask(cb,0x5D,0x07,(unsigned char)(t>>8));
 t=m->vd-1UL; rt_cr_mask(cb,0x59,0xFF,(unsigned char)t); rt_cr_mask(cb,0x5D,0x38,(unsigned char)(t>>5));
 t=m->vd-1UL; rt_cr_mask(cb,0x5A,0xFF,(unsigned char)t); rt_cr_mask(cb,0x5C,0x07,(unsigned char)(t>>8));
 t=m->vt-1UL; rt_cr_mask(cb,0x5B,0xFF,(unsigned char)t); rt_cr_mask(cb,0x5C,0x38,(unsigned char)(t>>5));
 t=m->vss; rt_cr_mask(cb,0x5E,0xFF,(unsigned char)t); rt_cr_mask(cb,0x5F,0xE0,(unsigned char)(t>>3));
 t=m->vse-1UL; rt_cr_mask(cb,0x5F,0x1F,(unsigned char)t);
}
static unsigned char i2_pat_var; static unsigned long i2_pat_base;
/* tune bits (lab_dx): 3:0 DVP1 adjust (CR9B[3:0]); 5:4 DVP1 clock drive, 7:6 data drive (SR65); 8 VT1632 EDGE, 9 DSEL, 10 BSEL; 11 OpenChrome VX855 IGA2 FIFO values;
 * 12 negative HSYNC, 13 negative VSYNC (CR9B[5], [6]); 14 PLL dtz = 3; 17:18 PLL multiplier-1; 27:29 forced PLL dn (0 = auto); 19:22 VT1632 reg 0C filter (0 = 00, n = 01|(n-1)<<1); 23:26 VT1632 reg 0A deskew (0 = 00, n = 10|(n-1)<<5) (dm and dn both times k, same frequency); 15 light update (only the output-side items, needs a previous full apply); 16 keep the picture already in memory */
static void vx_iga2_apply(vx3d_report *r,const vx3d_request *q) {
 unsigned long mem,base=0,i,x,y,sz,W,H,pitch,t=q->lab_dx; unsigned char misc=inp(0x3CC),var=(unsigned char)q->lab_sy; unsigned short cb=(misc&1)?0x3D4:0x3B4; unsigned int k;
 volatile uint32_t *p; static const unsigned long bars[8]={0xFFFFFFUL,0xFFFF00UL,0x00FFFFUL,0x00FF00UL,0xFF00FFUL,0xFF0000UL,0x0000FFUL,0x000000UL};
 uint32_t *im=r->image; uint32_t tmp[16]; const iga2_mode *m; int light=((t>>15)&1)&&i2_have; unsigned char ed[128];
 r->result=VX3D_PASS;
 if(var<1||var>14) {im[0]=4; return;}
 if(var==5) {
  if(!light && (!rt_read_edid(ed) || !i2_native(ed,&i2_nm,im))) {im[0]=5; return;}
  m=&i2_nm;
 } else if(var>=6) {
  const iga2_ext *x=&i2_ext[var-6];
  i2_xm.hd=x->hd; i2_xm.hss=x->hss; i2_xm.hse=x->hse; i2_xm.ht=x->ht; i2_xm.vd=x->vd; i2_xm.vss=x->vss; i2_xm.vse=x->vse; i2_xm.vt=x->vt; i2_xm.hneg=x->hneg; i2_xm.vneg=x->vneg;
  if(!i2_pll_solve(x->clk,&i2_xm)) {im[0]=5; return;}
  m=&i2_xm;
 } else m=&i2_modes[var-1];
 W=m->hd; H=m->vd; pitch=W*4UL; sz=pitch*H;
 if(!light) {
  mem=vx_status.fb_bytes;
  if(vx_chip_fb_bytes && vx_chip_fb_bytes<mem) mem=vx_chip_fb_bytes;
  if(vx_vbe_total_bytes && vx_vbe_total_bytes<mem) mem=vx_vbe_total_bytes;
  im[2]=mem; im[3]=sz;
  base=q->lab_sx?q->lab_sx:((mem-sz-0x100000UL)&~0xFFFUL);
  /* v10.330 (review #194): base + sz could wrap past 4 GB and pass; base checked against the usable size (mem) and the mapping, and 8-byte
   * aligned (the scan-out start is base >> 3) */
  if(mem<0x1000000UL || mem<sz+0x500000UL || base<0x400000UL || (base&7) || base>mem || sz>mem-base || base>vx_status.fb_bytes || sz>vx_status.fb_bytes-base) {im[0]=2; return;}
  p=(volatile uint32_t*)(vx_status.fb_linear+base);
  im[1]=base;
  if(!(((t>>16)&1) && i2_pat_var==var && i2_pat_base==base)) {
   p[0]=0x5A5AA5A5UL; p[sz/4-1]=0xA5A55A5AUL; p[(sz/8)]=0x13572468UL;
   if(p[0]!=0x5A5AA5A5UL||p[sz/4-1]!=0xA5A55A5AUL||p[sz/8]!=0x13572468UL) {im[0]=3; return;}
   for(y=0;y<H;y++) for(x=0;x<W;x++) {
    unsigned long c;
    if(y<H*2/3) c=bars[x*8/W];
    else if(y<H*5/6) {unsigned long g=x*255UL/(W-1); c=(g<<16)|(g<<8)|g;}
    else if(x<W/2) c=(x&1)?0xFFFFFFUL:0UL;
    else c=0x808080UL;
    if(x<16&&y<16) c=0xFF0000UL; else if(x>=W-16&&y<16) c=0x00FF00UL; else if(x<16&&y>=H-16) c=0x0000FFUL; else if(x>=W-16&&y>=H-16) c=0xFFFFFFUL;
    if(x==0||y==0||x==W-1||y==H-1) c=0xFFFFFFUL;
    p[y*W+x]=c;
   }
   (void)p[0]; i2_pat_var=var; i2_pat_base=base;
  }
  rt_sr_mask(0x10,0x01,0x01);
  if(!i2_have) {
   i2_sr10=rt_sr_get(0x10);
   for(k=0;k<35;k++) i2_crv[k]=rt_cr_get(cb,i2_crl[k]);
   for(k=0;k<8;k++) i2_srv[k]=rt_sr_get(i2_srl[k]);
   i2_sr31=rt_sr_get(0x31); i2_txok=0;
   i2_have=1;
  }
  for(k=0;k<35;k++) im[100+k]=i2_crv[k];
  for(k=0;k<8;k++) im[140+k]=i2_srv[k];
 } else rt_sr_mask(0x10,0x01,0x01);
 /* VT1632 transmitter (7-bit 0x08) on I2C bus 2 (SR31) */
 rt_reg=0x31; rt_en=0x01; rt_mask=0x31; rt_set(1,1); rt_delay(); rt_delay();
 im[149]=0;
 if(!light && rt_read_regs(0x08,0,16,tmp)) {
  im[149]|=1; i2_txok=1;
  for(k=0;k<16;k++) {im[170+k]=tmp[k]&0xFF; i2_txv[k]=(unsigned char)tmp[k];}
 }
 if(i2_txok) {
  unsigned char r08=(unsigned char)(0x31|(((t>>8)&1)?0x02:0)|(((t>>9)&1)?0x08:0)|(((t>>10)&1)?0x04:0));
  unsigned char r0a=0,r0c=0; unsigned long fv=(t>>19)&15UL,dv=(t>>23)&15UL;
  if(fv) r0c=(unsigned char)(0x01|(((fv-1)&7)<<1));   /* 0C: bit0 PLL filter enable, bits 3:1 filter setting (SiI164 compatible) */
  if(dv) r0a=(unsigned char)(0x10|(((dv-1)&7)<<5));   /* 0A: bit4 deskew enable, bits 7:5 deskew step */
  im[209]=r0a; im[210]=r0c;
  if(rt_write_reg(0x08,0x08,r08) && rt_write_reg(0x08,0x09,0x20) && rt_write_reg(0x08,0x0A,r0a) && rt_write_reg(0x08,0x0C,r0c)) im[149]|=2;
  if(rt_read_regs(0x08,0,16,tmp)) for(k=0;k<16;k++) im[150+k]=tmp[k]&0xFF;
 }
 rt_set(1,1);
 rt_sr_mask(0x1E,0x30,0x30);
 rt_sr_mask(0x65,0x0F,(unsigned char)(((t>>4)&3)<<2|((t>>6)&3)));
 if((t>>11)&1) {
  rt_cr_mask(cb,0x68,0xF0,0x80); rt_cr_mask(cb,0x94,0x80,0x80); rt_cr_mask(cb,0x95,0x80,0x00);
  rt_cr_mask(cb,0x68,0x0F,0x08); rt_cr_mask(cb,0x95,0x70,0x20);
  rt_cr_mask(cb,0x92,0x08,0x08); rt_cr_mask(cb,0x95,0x07,0x02);
  rt_cr_mask(cb,0x94,0x7F,0x08);
 } else {
  rt_cr_mask(cb,0x68,0xFF,i2_crv[31]); rt_cr_mask(cb,0x92,0xFF,i2_crv[32]); rt_cr_mask(cb,0x94,0xFF,i2_crv[33]); rt_cr_mask(cb,0x95,0xFF,i2_crv[34]);
 }
 if(!light) {
  rt_sr_mask(0x1B,0xC0,0xC0);
  rt_sr_mask(0x2D,0x0C,0x0C);
  /* secondary (IGA2) dot clock PLL SR4A..4C, then reset the PLL (SR40 bit 2) */
  {unsigned long kk=1UL+((t>>17)&3),pdm,pdn;
   while(kk>1 && (m->dm*kk>1023UL || m->dn*kk>127UL)) kk--;
   pdm=m->dm*kk; pdn=m->dn*kk; /* same output frequency, higher comparison divider (kk=1 is the OpenChrome choice) */
   {unsigned long fd=(t>>27)&7UL; /* forced dn (BIOS uses dn 5 at 25 MHz): dm recomputed for the same frequency */
    if(fd) {unsigned long fo=14318UL*m->dm/((unsigned long)m->dn<<m->dr); pdn=fd; pdm=(fo*(fd<<m->dr)+7159UL)/14318UL; if(pdm>1023UL) pdm=1023UL;}}
   im[205]=pdm; im[206]=pdn; im[207]=m->dr; im[208]=kk;
   rt_sr_mask(0x4A,0xFF,(unsigned char)(pdm&0xFF));
   rt_sr_mask(0x4B,0xFF,(unsigned char)(0x80|((pdm>>8)&3)|(m->dr<<2)));
   rt_sr_mask(0x4C,0xFF,(unsigned char)(pdn|(((t>>14)&1)?0x80:0)));}
  rt_sr_mask(0x40,0x04,0x04); rt_sr_mask(0x40,0x04,0x00);
  for(i=0;i<40000UL;i++) (void)inp(0x80);
  i2_timing(cb,m);
  rt_cr_mask(cb,0x62,0x01,0x00);
  {unsigned long off=pitch>>3,fc=pitch>>4;
   rt_cr_mask(cb,0x66,0xFF,(unsigned char)off);
   rt_cr_mask(cb,0x65,0xFF,(unsigned char)fc);
   rt_cr_mask(cb,0x67,0xEF,(unsigned char)(0xC0|(((fc>>8)&3)<<2)|((off>>8)&3)));}
  {unsigned long b8=base>>3;
   rt_cr_mask(cb,0x62,0xFE,(unsigned char)((b8&0x7F)<<1));
   rt_cr_mask(cb,0x63,0xFF,(unsigned char)((b8>>7)&0xFF));
   rt_cr_mask(cb,0x64,0xFF,(unsigned char)((b8>>15)&0xFF));
   rt_cr_mask(cb,0xA3,0x07,(unsigned char)((b8>>23)&7));}
  rt_cr_mask(cb,0x79,0x01,0x00);
  rt_cr_mask(cb,0x6C,0x0F,0x00);
  rt_cr_mask(cb,0x6B,0x36,0x00);
  rt_cr_mask(cb,0x6A,0xC1,0xC0);
  for(i=0;i<20000UL;i++) (void)inp(0x80);
 }
 rt_cr_mask(cb,0x9B,0x7F,(unsigned char)(0x10|(t&15)|((m->hneg^((t>>12)&1))?0x20:0)|((m->vneg^((t>>13)&1))?0x40:0)));
 for(k=0;k<35;k++) im[16+k]=rt_cr_get(cb,i2_crl[k]);
 for(k=0;k<8;k++) im[64+k]=rt_sr_get(i2_srl[k]);
 rt_sr_mask(0x10,0x01,(unsigned char)(i2_sr10&1));
 im[0]=1;
}
unsigned long VX3D_test(const vx3d_request *request,vx3d_report *r) {
 vx3d_io io;
 vx3d_sf_io sf;
 uint32_t final_status;
 int ready;
 if(request->cb!=sizeof(*request) || request->version!=VX3D_VERSION ||
    request->arm!=VX3D_ARM || (request->reserved&0xFF)>28 || request->probe>=VX3D_PROBES) return 87;
 vx_lock();
 recover_info[3]=0;
 if(fault_latched && !running && !VX2D_busy() && (request->reserved&0x100) && mmio_linear) recover_engine();
 ready=prepare(r,request->probe); r->probe_id=request->probe;
 r->sf_x_info[7][0]=recover_info[0]; r->sf_x_info[7][1]=recover_info[1]; r->sf_x_info[7][2]=recover_info[2]; r->sf_x_info[7][3]=recover_info[3];
 if(!ready) { vx_unlock(); return 0; }
 if(fault_latched || running || VX2D_busy() || (vx3dn_rt_why=6,VX3DN_busy())) {
  r->result=VX3D_DISABLED; vx_unlock(); return 0;
 }
 running=1;
 r->probe_id=request->probe;
 r->display_draws_before=vx_status.draws;
 r->display_skips_before=vx_native_display_skips;
 if(request->probe) {vx_native_display_hold=1; r->display_isolated=1;}
 if(request->probe==VX3D_SF_PROBE && (request->reserved&0xFF)==21 && (request->lab_flags&1048576UL)) {vx_route_dump(r); goto finish;}
 if(request->probe==VX3D_SF_PROBE && (request->reserved&0xFF)==21 && (request->lab_flags&2097152UL)) {vx_iga2_apply(r,request); goto finish;}
 if(request->probe==VX3D_SF_PROBE && (request->reserved&0xFF)==21 && (request->lab_flags&4194304UL)) {vx_iga2_restore((inp(0x3CC)&1)?0x3D4:0x3B4); r->result=VX3D_PASS; r->image[0]=1; goto finish;}
 if(!enable_engine(r)) {r->result=VX3D_INIT_FAILURE; goto finish;}
 if(request->probe==VX3D_SF_PROBE) diagnostic_snapshot(r->diagnostic_mmio[1]);
 io.context=NULL; io.read_mmio=read_mmio; io.write_mmio=write_mmio;
 io.read_fb=read_fb; io.write_fb=write_fb; io.milliseconds=milliseconds;
 if(request->probe==VX3D_SF_PROBE && (request->reserved&0xFF)==21) {
  sf.context=NULL; sf.read_host=sf_read_host; sf.write_host=sf_write_host;
  sf.read_seq=sf_read_seq; sf.write_seq=sf_write_seq; sf.publish=sf_publish;
  vx3d_run_lab(&io,&sf,r,request);
  if(request->lab_flags&524288UL) {r->sf_x_info[5][0]=vx_vbe_total_bytes; r->sf_x_info[5][1]=vx_chip_fb_bytes; r->sf_x_info[5][2]=vx_map_bytes; r->sf_x_info[5][3]=vx_status.fb_bytes;}
 } else if(request->probe==VX3D_SF_PROBE) {
  if(!sf_allocate(r)) {r->result=VX3D_SF_MAPPING_FAILURE; goto finish;}
  sf.context=(void*)sf_allocation;
  sf.read_host=sf_read_host; sf.write_host=sf_write_host;
  sf.read_seq=sf_read_seq; sf.write_seq=sf_write_seq; sf.publish=sf_publish;
  vx3d_x_select=request->reserved&0xFF;
  vx3d_x_reach=(request->reserved>>16)&0xFF;
  vx3d_run_sf(&io,&sf,r,arena_backup,queue_backup,sf_table_backup);
  if((r->sf_flags&VX3D_SF_SAFE_RELEASE) || !(r->sf_flags&0x40000)) {
   _PageFree((void*)sf_allocation,0); sf_allocation=0; r->sf_flags|=0x20000;
  }
 } else if(request->probe>=VX3D_TRANSPORT_FIRST) vx3d_run_transport(&io,r,arena_backup,queue_backup);
 else if(request->probe<2) vx3d_run(&io,r,arena_backup);
 else vx3d_run_probe(&io,r,arena_backup);
finish:
 if((r->init_flags&64) && !(r->init_flags&4)) {
  if((r->sf_flags&0x40000) ||
    ((request->probe!=VX3D_SF_PROBE || !(r->sf_flags&VX3D_SF_SAFE_RELEASE)) &&
    (r->result==VX3D_TIMEOUT || r->result==VX3D_BUSY ||
     r->result==VX3D_CONTROL_UNCONFIRMED ||
     r->result==VX3D_FETCH_UNCONFIRMED))) r->init_flags|=8;
  else {
   final_status=read_mmio(NULL,0x400);
   r->status_after=final_status; r->status_or|=final_status;
   if(final_status&VX3D_BUSY_MASK) {
    r->init_flags|=8;
    r->wait_failure_reason=4; r->wait_failure_status=final_status;
    if(r->result==VX3D_PASS) r->result=VX3D_TIMEOUT;
   } else restore_engine(r);
  }
 }
 r->display_draws_after=vx_status.draws;
 r->display_skips_after=vx_native_display_skips;
 vx_native_display_hold=0;
 /* Clean idle pixel failures can be followed by another fixed probe.
  * Timeout, bad guards, unreadable/busy status and control errors stop all. */
 /* v10.466 (review): a request refused before any work reached the engine (a bad lab register or size, the SF pages not had,
  * busy before a register was written) with the engine reading idle no longer latches the fault (it refused every 2D, flip, D3D and
  * GL request until a reboot or a lab recover) */
 {int benign=(r->result==VX3D_BAD_REQUEST||r->result==VX3D_SF_MAPPING_FAILURE||(r->result==VX3D_BUSY&&!r->writes))&&!(r->init_flags&8)&&
   mmio_linear&&!(read_mmio(NULL,0x400)&VX3D_BUSY_MASK);
 if(!benign&&r->result!=VX3D_PASS && r->result!=VX3D_SOLID_FAILURE &&
    r->result!=VX3D_SHADE_FAILURE && r->result!=VX3D_DEPTH_FAILURE)
  fault_latched=1;}
 if(r->init_flags&8) fault_latched=1;
 r->fault_latched=fault_latched; running=0;
 dbg_printf("VX85598 v9 probe=%ld result=%ld stage=%ld status=%lX/%lX writes=%ld polls=%ld guard=%ld solid=%ld shade=%ld depth=%ld\n",
  r->probe_id,r->result,r->stage,r->status_before,r->status_after,r->writes,r->polls,
  r->guard_errors,r->solid_errors,r->shade_errors,r->depth_errors);
 vx_unlock(); return 0;
}
