/* VX85598 hardware guards and diagnostic buffer. Copyright (c) 2026, MIT. */
#include "winhack.h"
#include "vmm.h"
#include "vxd_lib.h"
#include "vesa.h"
#include "vx85598.h"
#define IO_IN32
#define IO_OUT32
#include "io32.h"

vx_status_t vx_status = {VX_MAGIC, sizeof(vx_status_t), VX_VERSION};
static char vx_log[VX_LOG_CAPACITY];
static unsigned long vx_log_used;

unsigned long VX_irq_save(void);
#pragma aux VX_irq_save = "pushfd" "pop eax" "cli" value [eax] modify exact [eax];
void VX_irq_restore(unsigned long flags);
#pragma aux VX_irq_restore = "push eax" "popfd" parm [eax] modify exact [];

volatile unsigned long vx_dbglog = 1; /* v10.329: on through start-up, then ACCEL_DEBUG_LOG (default 0) */
void VX_log_enable(unsigned long on)
{
 unsigned long flags = VX_irq_save();
 vx_dbglog = on ? 1 : 0;
 if(!on) vx_log_used = 0;
 VX_irq_restore(flags);
}
void VX_log_char(char c)
{
 unsigned long flags;
 if(!vx_dbglog) return;
 flags = VX_irq_save();
 /* Retain startup evidence; later bytes are counted rather than replacing it. */
 if(c=='\n' && (!vx_log_used || vx_log[vx_log_used-1]!='\r')) {
  if(vx_log_used < VX_LOG_CAPACITY-1) vx_log[vx_log_used++]='\r';
  else vx_status.log_dropped++;
 }
 if(vx_log_used < VX_LOG_CAPACITY-1) vx_log[vx_log_used++] = c;
 else vx_status.log_dropped++;
 VX_irq_restore(flags);
}

unsigned long VX_log_copy(void *out, unsigned long bytes)
{
 unsigned long flags, n;
 if(!out || !bytes) return 0;
 flags = VX_irq_save();
 n = vx_log_used;
 if(n >= bytes) n = bytes-1;
 memcpy(out, vx_log, n);
 ((char*)out)[n] = 0;
 VX_irq_restore(flags);
 return n+1;
}

void VX_snapshot(vx_status_t *out)
{
 unsigned long flags = VX_irq_save();
 vx_status.log_bytes = vx_log_used;
 memcpy(out, &vx_status, sizeof(vx_status));
 VX_irq_restore(flags);
}

unsigned long vx_vbe_total_bytes, vx_chip_fb_bytes, vx_map_bytes;
/* .579: one driver for the Chrome9 family of the VX chipsets. vx_chip_id the PCI device: 0x1122 VX800 / VX820 (Chrome9 HC3), 0x5122 VX855 /
 * VX875 (Chrome9 HCM), 0x7122 VX900 (Chrome9 HD). The frame buffer's and the register window's BARs by device (VIA's manuals, OpenChrome
 * via_ums.c): 5122 the frame buffer at configuration 10h, the registers at 14h; 1122 the frame buffer at 10h (a 64-bit BAR there takes
 * 14h too: the registers are then at 18h; a 32-bit one leaves them at 14h, as OpenChrome's region 1); 7122 the registers at 14h, the
 * frame buffer at 18h. vx_status.bar0 / bar1 hold the two bases with the type bits cleared, as every later check expects.
 * VX800's 2D engine and 3D engine take the VX855's register sequences (XP's miniport admits 1122 and 5122 to one branch; CE groups them
 * as H6). VX900's 2D register order and vertex input differ (XP's 7122 branch, CE's H5S2): VX_chip_accel() is 0 there: frame buffer,
 * modes, cursor and flips only, the engines not touched, until its own paths exist. */
unsigned long vx_chip_id, vx_chip_fb_cfg, vx_chip_mmio_cfg;
int VX_chip_accel(void) {return vx_chip_id==0x1122UL||vx_chip_id==0x5122UL;}
/* the video overlay: the VX855's and the VX800's (.583: one code path, the VX800's HQV register map and 16-bit key form branched in vx855_2d.c); the VX900's is its own, not driven yet */
int VX_chip_video(void) {return vx_chip_id==0x5122UL||vx_chip_id==0x1122UL;}
static unsigned long pci_read(unsigned long address, unsigned long offset);
/* a memory BAR below 4 GB: its base, or 0 (an I/O BAR, a reserved type, a 64-bit one with its upper half set, or nothing assigned) */
static unsigned long bar_mem(unsigned long address, unsigned long offset) {
 unsigned long raw=pci_read(address,offset),type=(raw>>1)&3UL;
 if((raw&1UL)||type==1UL||type==3UL) return 0;
 if(type==2UL&&pci_read(address,offset+4)) return 0;
 return raw&0xFFFFFFF0UL;
}
static unsigned long pci_read(unsigned long address, unsigned long offset);
unsigned long VX_pci_read(unsigned long address, unsigned long offset) {return pci_read(address,offset);}
static unsigned long pci_read(unsigned long address, unsigned long offset)
{
 unsigned long flags = VX_irq_save();
 unsigned long saved = inpd(0xCF8);
 unsigned long value;
 outpd(0xCF8, address | offset);
 value = inpd(0xCFC);
 outpd(0xCF8, saved);
 VX_irq_restore(flags);
 return value;
}

int VX_check_hardware(void)
{
 unsigned long bus, dev, fun, targets=0, active_vga=0, target=0;
 unsigned long id, address, command, classreg, raw;
 unsigned long i, len, pcir, fingerprint=2166136261UL;
 unsigned char checksum=0;
 const unsigned char *rom;
 vx_status.stage = 1;
 dbg_printf("VX85598 1.3d: hardware identification (multi-probe native tests)\n");
 for(bus=0; bus<256; bus++) for(dev=0; dev<32; dev++) {
  unsigned long maxfun=1;
  for(fun=0; fun<maxfun; fun++) {
   address=0x80000000UL | (bus<<16) | (dev<<11) | (fun<<8);
   id=pci_read(address,0);
   if((id & 0xFFFF)==0xFFFF || !(id & 0xFFFF)) continue;
   if(!fun && (pci_read(address,0x0C) & 0x00800000UL)) maxfun=8;
   classreg=pci_read(address,8);
   if((classreg>>16)!=0x0300) continue;
   command=pci_read(address,4) & 0xFFFF;
   if(command & 1) active_vga++;
   if(id==0x51221106UL||id==0x11221106UL||id==0x71221106UL) { targets++; target=address; vx_chip_id=id>>16; } /* .579 */
  }
 }
 if(targets!=1) { vx_status.failure=VX_NO_DEVICE; return 0; }
 if(active_vga!=1) { vx_status.failure=VX_MULTIPLE_VGA; return 0; }
 vx_status.pci_address=target;
 vx_status.pci_command=pci_read(target,4) & 0xFFFF;
 /* .579: the BARs by device (above) */
 vx_chip_fb_cfg=vx_chip_id==0x7122UL?0x18UL:0x10UL;
 vx_chip_mmio_cfg=(vx_chip_id==0x1122UL&&((pci_read(target,0x10)>>1)&3UL)==2UL)?0x18UL:0x14UL;
 vx_status.bar0=bar_mem(target,vx_chip_fb_cfg);
 vx_status.bar1=bar_mem(target,vx_chip_mmio_cfg);
 raw=vx_status.bar0;
 if((vx_status.pci_command & 3)!=3 || !raw || !vx_status.bar1 ||
    (raw & 0xFFFFFFF0UL)<0x100000UL ||
    (raw & 0xFFFFFFF0UL)>0xFFFFFFFFUL-VX_FB_LIMIT) {
  vx_status.failure=VX_BAD_PCI; return 0;
 }
 vx_status.fb_phys=raw & 0xFFFFFFF0UL;
 rom=(const unsigned char*)_MapPhysToLinear(0xC0000UL,0x10000UL,0);
 if(!rom || (unsigned long)rom==0xFFFFFFFFUL) {
  vx_status.failure=VX_BAD_ROM; return 0;
 }
 len=(unsigned long)rom[2]*512;
 pcir=(unsigned long)rom[0x18] | ((unsigned long)rom[0x19]<<8);
 if(rom[0]!=0x55 || rom[1]!=0xAA || len<0x20 || len>0x10000UL ||
    pcir>len-0x16 || memcmp(rom+pcir,"PCIR",4) ||
    rom[pcir+4]!=0x06 || rom[pcir+5]!=0x11 ||
    (vx_chip_id==0x5122UL && (rom[pcir+6]!=0x22 || rom[pcir+7]!=0x51)) || rom[pcir+0x14]!=0) { /* .579: a VIA x86 video ROM; its device number must be the VX855's there as before (the other boards' ROMs have not been read yet: theirs is only reported, TEXDBG 75) */
  vx_status.failure=VX_BAD_ROM; return 0;
 }
 for(i=0; i<len; i++) {
  checksum+=rom[i]; fingerprint=(fingerprint ^ rom[i])*16777619UL;
 }
 if(checksum) { vx_status.failure=VX_BAD_ROM; return 0; }
 vx_status.rom_fingerprint=fingerprint;
 vx_status.stage=2;
 dbg_printf("VX85598 PCI=%lX command=%lX BAR0=%lX BAR1=%lX ROM=%lX\n",
  target,vx_status.pci_command,raw,vx_status.bar1,fingerprint);
 return 1;
}

int VX_mode_allowed(unsigned long id, unsigned long w,
 unsigned long h, unsigned long bpp)
{
 /* v10.37: any linear 16/32bpp mode the BIOS lists, within sane sizes (the EDID filter is applied by the caller) */
 (void)id;
 return (bpp==8 || bpp==16 || bpp==32) && w>=640 && w<=1920 && h>=480 && h<=1200 && (w&1)==0;
}

int VX_mode_layout(unsigned long version, unsigned long id, const void *info,
 unsigned long *pitch)
{
 const vesa_mode_info_t *m=(const vesa_mode_info_t*)info;
 unsigned long p, rs, rp, gs, gp, bs, bp;
 if(!VX_mode_allowed(id,m->XResolution,m->YResolution,m->BitsPerPixel) ||
    (m->ModeAttributes & 0x99)!=0x99 || (m->BitsPerPixel==8 ? m->MemoryModel!=4 : m->MemoryModel!=6) ||
    m->PhysBasePtr!=vx_status.fb_phys) return 0;
 p=m->BytesPerScanLine;
 rs=m->RedMaskSize; rp=m->RedFieldPosition;
 gs=m->GreenMaskSize; gp=m->GreenFieldPosition;
 bs=m->BlueMaskSize; bp=m->BlueFieldPosition;
 if(version>=0x300) {
  /* Linear layout is independent of the banked layout in VBE 3.0. */
  p=m->LinBytesPerScanLine;
  rs=m->LinRedMaskSize; rp=m->LinRedFieldPosition;
  gs=m->LinGreenMaskSize; gp=m->LinGreenFieldPosition;
  bs=m->LinBlueMaskSize; bp=m->LinBlueFieldPosition;
 }
 if(p<(unsigned long)m->XResolution*(m->BitsPerPixel/8) ||
    p>VX_FB_LIMIT/m->YResolution) return 0;
 if(m->BitsPerPixel==8) {*pitch=p; return 1;} /* v10.71: packed-pixel palette mode, no colour masks */
 if(m->BitsPerPixel==16) {
  if(rs!=5 || rp!=11 || gs!=6 || gp!=5 || bs!=5 || bp!=0) return 0;
 } else {
  if(rs!=8 || rp!=16 || gs!=8 || gp!=8 || bs!=8 || bp!=0) return 0;
 }
 *pitch=p;
 return 1;
}
