/* Native VX855/VX875 triangle submission. Copyright (c) 2026, MIT.
 * Register facts: VIA Chrome9 HCM OGPM part II, rev 1.0, July 29 2009.
 * MMIO/busy conventions cross-checked against OpenChrome via_regs.h,
 * via_exa.c and VIA's published via_chrome9 DRM patch. See SOURCES.md.
 * No CPU rasterizer: CPU writes background/depth/guards and fixed indexed
 * vertex input. Pixel images come from GPU target readback.
 */
#include "vx855_3d.h"
#include <string.h>
#include "vx855_fill.h"

static uint32_t rd(const vx3d_io *io, uint32_t reg) {
 return io->read_mmio(io->context,reg);
}
static void status(vx3d_report *r,uint32_t s) {
 r->status_after=s; r->status_or|=s;
 if(r->phase<VX3D_PHASES) r->phase_status[r->phase]|=s;
}
static void wr(const vx3d_io *io, vx3d_report *r, uint32_t reg, uint32_t val) {
 io->write_mmio(io->context,reg,val); r->writes++;
 /* Same-device read drains posted PCI writes. No write-combining mapping. */
 status(r,rd(io,0x400));
}
static void type(const vx3d_io *io, vx3d_report *r, uint32_t t) {
 r->parameter_type=t;
 wr(io,r,((r->path_flags&1) && (t==0x10 || t==0x11))?0x41C:0x43C,t<<16);
}
static void reg(const vx3d_io *io, vx3d_report *r, uint32_t a,uint32_t v) {
 uint32_t t=r->parameter_type;
 wr(io,r,((r->path_flags&1) && (t==0x10 || t==0x11))?0x420:0x440,
  (a<<24)|(v&0xFFFFFFUL));
}
static int idle(const vx3d_io *io, vx3d_report *r) {
 uint32_t start=io->milliseconds(io->context), n, stable=0, s;
 for(n=0;n<VX3D_POLL_LIMIT;n++) {
  s=rd(io,0x400); status(r,s); r->polls++;
  if(s==0xFFFFFFFFUL) {
   r->wait_failure_reason=3; r->wait_failure_status=s; return 0;
  }
  if(!(s&VX3D_BUSY_MASK)) { if(++stable==32) return 1; }
  else stable=0;
  if((uint32_t)(io->milliseconds(io->context)-start)>=VX3D_TIMEOUT_MS) {
   r->wait_failure_reason=2; r->wait_failure_status=s; return 0;
  }
 }
 r->wait_failure_reason=1; r->wait_failure_status=s;
 return 0;
}
static void profile(vx3d_report *r) {
 r->producer=VX3D_PRODUCER;
 if(r->probe_id) r->path_flags=1|16;
 if(r->probe_id==12) r->path_flags|=8|32;
 if(r->probe_id==13) r->path_flags|=8|32|64|2;
 if(r->probe_id==14) r->path_flags|=8|32|64|128|2;
 if(r->probe_id==15) r->path_flags|=8|32|64|128|256|2;
 if(r->probe_id>=VX3D_TRANSPORT_FIRST)
  r->path_flags=1|2|8|16|32|512|((r->probe_id==17)?1024:0);
 if(r->probe_id==VX3D_PIPELINE_PROBE) r->path_flags|=2048;
 if(r->probe_id==VX3D_DMA_PROBE) r->path_flags=(r->path_flags&0xFFFFFFF7U)|4096;
 if(r->probe_id==VX3D_SF_PROBE) r->path_flags=(r->path_flags&0xFFFFFFF7U)|4096|8192;
}
static int synchronize(const vx3d_io *io,vx3d_report *r,uint32_t mask) {
 if(r->path_flags&2) wr(io,r,0x6C,mask);
 return idle(io,r);
}
/* HCM T10 A03 reads a selected register into the four-DWORD writeback
 * window. VIA's original PCI-5122 DLL uses 041C/0420 and reads 0420/0424
 * (vendor VA 00112BB4..00112C1A). No memory destination, DMA or interrupt.
 * Readback layout is NOT considered proven until two distinct markers
 * match on the physical board. Preserve every raw state window as evidence. */
static int read_state_bank(const vx3d_io *io,vx3d_report *r,
 uint32_t address,uint32_t bank,uint32_t *out) {
 uint32_t i;
 wr(io,r,0x41C,0x00100000UL);
 /* HCM p62: IDs 0 CR, 1 FE, 2 PE, 3 RC, 4 PS, 5 XE, 6 BE.
  * Debug-pin routing remains disabled; no performance-counter ID used. */
 wr(io,r,0x420,0x02000000UL|bank);
 /* Hardware returns an aligned four-register group, each with its index
  * in the high byte. Observed directly in the 1.3e physical capture. */
 wr(io,r,0x420,0x03000000UL|(address&0xFFFFFCUL));
 if(!idle(io,r)) return 0;
 for(i=0;i<4;i++) out[i]=rd(io,0x420+i*4);
 return 1;
}
static int read_state(const vx3d_io *io,vx3d_report *r,uint32_t address,uint32_t *out) {
 return read_state_bank(io,r,address,0,out);
}
static int selected(uint32_t address,const uint32_t *words,uint32_t *value) {
 uint32_t raw=words[address&3];
 if((raw>>24)!=(address&255)) return 0;
 *value=raw&0xFFFFFFUL; return 1;
}
static uint32_t enables(const vx3d_report *r) {
 uint32_t v=0x40;
 if(r->path_flags&64) v|=0x100000;
 if(r->path_flags&128) v|=0x800;
 if(r->path_flags&256) v=(v&0xFFFFFFBFU)|4;
 return v;
}
static int pipeline_audit(const vx3d_io *io,vx3d_report *r) {
 uint32_t phase,bank,group,a,b,same;
 static const uint32_t address[2]={0x0100,0x0184};
 if(!(r->path_flags&2048)) return 1;
 r->pipeline_flags=1;
 for(phase=0;phase<2;phase++) {
  /* No vertices between these writes. First disable color writing and
   * change the legal solid-color field, then reissue the test's exact
   * enable/color state before any primitive. Nothing changes a GPU address,
   * queue bound, clock, interrupt route or shader instruction. */
  type(io,r,1); reg(io,r,1,phase?enables(r):0);
  reg(io,r,0x86,phase?0xE04020UL:0x112233UL);
  if(phase) r->pipeline_flags|=2;
  if(!idle(io,r)) return 0;
  for(bank=0;bank<VX3D_PIPELINE_BANKS;bank++)
   for(group=0;group<2;group++)
    if(!read_state_bank(io,r,address[group],bank,r->pipeline_raw[phase][bank][group])) return 0;
 }
 type(io,r,0x10); reg(io,r,2,0);
 if(!idle(io,r)) return 0;
 r->pipeline_flags|=4|8;
 /* The CR bank has physical aligned/tagged evidence. If it positively
  * contradicts either reissued rendering field, do not submit vertices.
  * Unknown layouts in any bank remain observations, not success/failure. */
 if((selected(0x0101,r->pipeline_raw[1][0][0],&a) && a!=enables(r)) ||
    (selected(0x0186,r->pipeline_raw[1][0][1],&b) && b!=0xE04020UL)) {
  r->pipeline_flags|=16; return -1;
 }
 for(bank=0;bank<VX3D_PIPELINE_BANKS;bank++) {
  for(group=0;group<2;group++) {
   uint32_t selected_address=group?0x0186:0x0101;
   uint32_t first=group?0x112233:0,second=group?0xE04020:enables(r);
   if(selected(selected_address,r->pipeline_raw[0][bank][group],&a) &&
      selected(selected_address,r->pipeline_raw[1][bank][group],&b) &&
      a==first && b==second) r->pipeline_match[group]|=1UL<<bank;
  }
  same=1;
  for(phase=0;phase<2;phase++) for(group=0;group<2;group++)
   if(memcmp(r->pipeline_raw[phase][bank][group],r->pipeline_raw[phase][0][group],4*sizeof(uint32_t))) same=0;
  if(same) r->pipeline_identical|=1UL<<bank;
 }
 return 1;
}
static void clear_general(const vx3d_io *io,vx3d_report *r) {
 static const uint32_t at[12]={1,2,3,4,5,8,9,10,11,12,13,14};
 uint32_t i;
 r->parameter_type=2; wr(io,r,0x43C,0xFE020000UL);
 reg(io,r,0,1); reg(io,r,0,0);
 for(i=0;i<12;i++) reg(io,r,at[i],0);
}
static int no_texture(const vx3d_io *io,vx3d_report *r) {
 uint32_t v0,v5;
 if(!(r->path_flags&16)) return 1;
 if(!read_state(io,r,0xFE0200,r->general_before) ||
    !read_state(io,r,0xFE0204,r->general_before+4)) return 0;
 /* HCM pp128-136. Texture count and user clipping are shared pipeline
  * state even in an untextured test. Never rely on BIOS leftovers.
  * Do not sweep reserved A06/A07/A0F or supply any texture addresses. */
 clear_general(io,r);
 if(!idle(io,r)) return 0;
 if(!read_state(io,r,0xFE0200,r->general_after) ||
    !read_state(io,r,0xFE0204,r->general_after+4)) return 0;
 /* Refuse vertices if supported readback positively shows an active
  * texture count or user clipping after the clear. Unknown layout stays
  * observational; it can never substitute for pixel/guard checks. */
 if((selected(0xFE0200,r->general_after,&v0) && (v0&0xF0)) ||
    (selected(0xFE0205,r->general_after+4,&v5) && (v5&0x8080))) {
  r->init_flags|=1024; return -1;
 }
 return 1;
}
static int fence_set(const vx3d_io *io,vx3d_report *r,uint32_t value) {
 /* Mode 0 records the ID only. It does not write memory, queue an IRQ,
  * wait for an interrupt, or change a render-buffer address (OGPM p70). */
 type(io,r,0x11); reg(io,r,0x06,value&0xFFFFFFUL);
 reg(io,r,0x07,0x100000UL|((value>>24)<<8));
 return idle(io,r);
}
static int acceptance_before(const vx3d_io *io,vx3d_report *r) {
 static const uint32_t addresses[VX3D_STATE_READS]={
  0xFE00,0x0101,0x0150,0x0152,0x0153,0x0400,0x0401,0x0402,0x0425,0x0428,
  0xFE0200,0xFE0205,0x0180,0x0183,0x011E,0x0440,0x0445,0x0448
 };
 uint32_t expected[VX3D_STATE_READS]={
  0x0E1004,0x40,VX3D_TARGET>>8,0x990008,0xC0F,5,0x020100,0x000503,0xF40000,0xF40000,
  0,0,64,64,0x800000,0x800000,0x3F,0x92003F
 };
 static const uint32_t kinds[3]={0x11,4,1};
 uint32_t id=r->probe_id>=VX3D_TRANSPORT_FIRST?VX3D_TRANSPORT_FIRST:r->probe_id;
 uint32_t i,a=0x1260A500UL|id,b=0x23605A00UL|id;
 uint32_t value[4],mask,ma,mb,v;
 int old;
 expected[1]=enables(r);
 if(r->path_flags&64) {expected[15]=0; expected[16]=0x42C2;}
 for(i=0;i<VX3D_STATE_READS;i++) {
  if(!read_state(io,r,addresses[i],r->state_readback[i])) return 0;
  /* A00 bit0 is a cache-clear control, not the active texture count.
   * The physical 1.3f capture returns it as one with count bits clear. */
  if(selected(addresses[i],r->state_readback[i],&v) &&
     (v&(i==10?0xF0UL:0xFFFFFFUL))==expected[i]) r->state_matches|=1UL<<i;
 }
 r->fence[0]=rd(io,0x494); /* exact 5122 vendor branch VA 00112182..001121D5 */
 if(!fence_set(io,r,a)) return 0;
 r->fence[1]=rd(io,0x494);
 if(!fence_set(io,r,b)) return 0;
 r->fence[2]=rd(io,0x494);
 if(r->fence[1]==a && r->fence[2]==b) r->acceptance|=1;
 for(i=0;i<3;i++) {
  mask=i?0xFFFFUL:0xFFFFFFUL;
  ma=(0x1357U+(i<<12)+id)&mask;
  mb=(0x6CA8U+(i<<12)+id)&mask;
  if(!read_state(io,r,(kinds[i]<<8)|0xAA,value)) return 0;
  r->marker[i][0]=value[2];
  old=selected((kinds[i]<<8)|0xAA,value,&v);
  type(io,r,kinds[i]); reg(io,r,0xAA,ma);
  if(!read_state(io,r,(kinds[i]<<8)|0xAA,value)) return 0;
  r->marker[i][1]=value[2];
  type(io,r,kinds[i]); reg(io,r,0xAA,mb);
  if(!read_state(io,r,(kinds[i]<<8)|0xAA,value)) return 0;
  r->marker[i][2]=value[2];
  if(old && (r->marker[i][1]>>24)==0xAA && (r->marker[i][2]>>24)==0xAA &&
     (r->marker[i][1]&mask)==ma && (r->marker[i][2]&mask)==mb) r->acceptance|=2UL<<i;
  /* v10.330 (review #117): the original value is put back whenever it was read, not only when both new markers read back as written */
  if(old) {
   type(io,r,kinds[i]); reg(io,r,0xAA,r->marker[i][0]&mask);
   if(!read_state(io,r,(kinds[i]<<8)|0xAA,value)) return 0;
   r->marker[i][3]=value[2];
  }
 }
 if(!read_state(io,r,0x011C,r->occlusion[0])) return 0;
 type(io,r,1); reg(io,r,0x1C,0); reg(io,r,0x1D,0);
 if(!read_state(io,r,0x011C,r->occlusion[1])) return 0;
 /* Marker registers are inert inspection flags; no vertices were submitted. */
 return idle(io,r);
}
static int acceptance_after(const vx3d_io *io,vx3d_report *r) {
 uint32_t lo,hi;
 if(!(r->path_flags&8)) return 1;
 r->phase=6; r->fence[5]=0x3460C300UL|
  (r->probe_id>=VX3D_TRANSPORT_FIRST?VX3D_TRANSPORT_FIRST:r->probe_id);
 if(!fence_set(io,r,r->fence[5])) return 0;
 r->fence[3]=rd(io,0x494);
 if(r->fence[3]==r->fence[5]) r->acceptance|=16;
 if((r->acceptance&1) && r->fence[0]!=0xFFFFFFFFUL) {
  if(!fence_set(io,r,r->fence[0])) return 0;
  r->fence[4]=rd(io,0x494);
  if(r->fence[4]==r->fence[0]) r->acceptance|=32;
 }
 if(!read_state(io,r,0x011C,r->occlusion[2])) return 0;
 if(selected(0x011C,r->occlusion[0],&lo) && selected(0x011D,r->occlusion[0],&hi)) {
  type(io,r,1); reg(io,r,0x1C,lo); reg(io,r,0x1D,hi&255);
  if(!idle(io,r)) return 0;
 }
 return 1;
}
static void clear(const vx3d_io *io) {
 uint32_t i;
 for(i=0;i<VX3D_ARENA_BYTES;i+=4)
  io->write_fb(io->context,VX3D_ARENA+i,VX3D_CANARY);
 for(i=0;i<VX3D_PIXELS*4;i+=4) {
  io->write_fb(io->context,VX3D_TARGET+i,VX3D_BACKGROUND);
  io->write_fb(io->context,VX3D_DEPTH+i,0xFFFFFFFFUL);
 }
 /* Drain CPU framebuffer writes before enabling GPU access. */
 (void)io->read_fb(io->context,VX3D_DEPTH+VX3D_PIXELS*4-4);
}
static void setup_command_path(const vx3d_io *io, vx3d_report *r) {
 /* Direct PCI/MMIO submission. No AGP, GART, DMA or command queue storage.
  * 10:74 and 10:7C explicitly disable both command queues (OGPM pp67-68).
  * FE configuration is a separate VX855-specific step after the 2D check. */
 type(io,r,0x10); reg(io,r,0x74,0); reg(io,r,0x7C,0);
 reg(io,r,0x00,0); reg(io,r,0x02,0); reg(io,r,0x07,0);
 reg(io,r,0x68,0x0A0000); /* documented threshold, no AGP trigger */
 type(io,r,0x11); reg(io,r,0x00,0x248); /* FIFO defaults, noncritical */
}
static void setup_global(const vx3d_io *io, vx3d_report *r) {
 /* The HCM manual requires FE in the draw sequence but omits its fields.
  * VIA's Linux H6S2 initialization supplies the configuration family.
  * Exact overrides/order below are independently checked by executing the
  * PCI 5122 branch of the user's vendor DLL; see references/VX855_FE.md.
  * Do not use the older UniChrome values, reset reserved FE registers, or
  * substitute the incompatible VX900 vertex shader/FVF path. */
 static const uint32_t words[16]={
  0x000E1004UL,0x08000249UL,0x0A0002C9UL,0x0B0002FBUL,
  0x0C000000UL,0x0D0002CBUL,0x0E000009UL,0x10000049UL,
  0x110002FFUL,0x12000008UL,0x130002DBUL,0x20000000UL,
  0x40008408UL,0x44000000UL,0x46000000UL,0x47000404UL
 };
 uint32_t i;
 type(io,r,0xFE);
 for(i=0;i<16;i++) wr(io,r,0x440,words[i]);
 r->init_flags|=512;
}
static void setup(const vx3d_io *io, vx3d_report *r) {
 type(io,r,0x01);
 reg(io,r,0x00,0); /* blending, fog, alpha, dither disabled */
 reg(io,r,0x01,enables(r)); /* explicit color/viewport/depth path */
 /* HCM T01A02: cache keeps clearing while bit1 is set. Invalidate once,
  * then release it with read-color caching still disabled. */
 reg(io,r,0x02,2); reg(io,r,0x02,0);
 reg(io,r,0x10,VX3D_DEPTH>>8);
 reg(io,r,0x11,(VX3D_SIDE*4/32)<<13); /* linear Z pitch, cache off */
 if(r->path_flags&256) reg(io,r,0x11,((VX3D_SIDE*4/32)<<13)|0x200);
 reg(io,r,0x12,4UL<<16); /* 32-bit fixed Z */
 reg(io,r,0x13,1UL<<16); /* LESS */
 if(r->path_flags&256) reg(io,r,0x13,7UL<<16); /* ALWAYS, independent Z writes */
 reg(io,r,0x15,0); reg(io,r,0x16,0); reg(io,r,0x17,0);
 reg(io,r,0x18,0); reg(io,r,0x19,0); reg(io,r,0x1A,0); reg(io,r,0x1B,0);
 /* Pixel near/far rejection precedes depth testing even with CL disabled.
  * Set float near=0 and far=1, rather than relying on BIOS leftovers. */
 reg(io,r,0x1E,0x800000); reg(io,r,0x1F,0); reg(io,r,0x20,0x3F00);
 reg(io,r,0x21,0); reg(io,r,0x22,0); /* no occlusion writeback address */
 reg(io,r,0x50,VX3D_TARGET>>8); /* 256-byte target base units */
 reg(io,r,0x51,0); /* linear, system local framebuffer */
 reg(io,r,0x52,(0x99UL<<16)|(VX3D_SIDE*4/32)); /* ARGB8888 */
 reg(io,r,0x53,0xC0F); /* COPY, write all four channels */
 reg(io,r,0x80,VX3D_SIDE); reg(io,r,0x81,VX3D_SIDE);
 reg(io,r,0x82,VX3D_SIDE); reg(io,r,0x83,VX3D_SIDE);
 reg(io,r,0x86,0xE04020); reg(io,r,0x87,0xFF); /* solid orange */
 reg(io,r,0x88,0); reg(io,r,0x89,VX3D_SIDE); reg(io,r,0x8A,0);
 type(io,r,0x04);
 reg(io,r,0x00,5); /* 5 dwords: x,y,z,w,ARGB */
 reg(io,r,0x01,0x020100); reg(io,r,0x02,0x000503);
 reg(io,r,0x10,0); /* no back-face color attributes */
 reg(io,r,0x20,0); /* full triangles, command vertices, no index fetch */
 reg(io,r,0x21,0); reg(io,r,0x22,0);
 reg(io,r,0x25,0xF40000); reg(io,r,0x26,0); /* vertex input mask */
 reg(io,r,0x27,0);
 reg(io,r,0x28,0xF40000); reg(io,r,0x29,0); /* parameter output mask */
 reg(io,r,0x2F,0x400000); /* default W=1 */
 /* OGPM pp165-166: keep screen coordinates within +/-2048 even though
  * clipping-coordinate transform and clipping engine are both disabled. */
 reg(io,r,0x50,0); reg(io,r,0x51,0); reg(io,r,0x52,0x0B45C5);
 if(r->path_flags&32) {
  /* HCM pp164-166: fully initialize the viewport and area threshold.
   * Screen-space path: identity. C2S paths: x=32*x+32, y=-32*y+32.
   * All inputs stay within the guarded 64x64 target; Z remains unchanged. */
  if(r->path_flags&64) {
   reg(io,r,0x40,0); reg(io,r,0x41,0); reg(io,r,0x42,0x4242);
   reg(io,r,0x43,0); reg(io,r,0x44,0); reg(io,r,0x45,0x42C2);
  } else {
   reg(io,r,0x40,0x800000); reg(io,r,0x41,0); reg(io,r,0x42,0x3F);
   reg(io,r,0x43,0x800000); reg(io,r,0x44,0); reg(io,r,0x45,0x3F);
  }
  reg(io,r,0x46,0x800000); reg(io,r,0x47,0); reg(io,r,0x48,0x92003F);
 }
}
static void vertex(const vx3d_io *io, vx3d_report *r,
 uint32_t x,uint32_t y,uint32_t z,uint32_t color) {
 wr(io,r,0x440,x); wr(io,r,0x440,y); wr(io,r,0x440,z);
 wr(io,r,0x440,0x3F800000UL); wr(io,r,0x440,color);
}
static int triangle(const vx3d_io *io, vx3d_report *r,
 uint32_t shading,uint32_t depth,uint32_t z,
 uint32_t ca,uint32_t cb,uint32_t cc) {
 r->phase=5;
 r->vertex_words=5; r->vertex_vertices=3;
 type(io,r,0x01); reg(io,r,0x01,0x40|depth);
 type(io,r,0x04); reg(io,r,0x23,shading|2); reg(io,r,0x24,3);
 type(io,r,0);
 vertex(io,r,0x41000000UL,0x41000000UL,z,ca); /* (8,8) */
 vertex(io,r,0x42600000UL,0x41000000UL,z,cb); /* (56,8) */
 vertex(io,r,0x41000000UL,0x42600000UL,z,cc); /* (8,56) */
 /* Vertex count generates PLEND (OGPM p61). No invented fire word. */
 return idle(io,r);
}
static uint32_t pixel(const vx3d_io *io,uint32_t x,uint32_t y) {
 return io->read_fb(io->context,(uint32_t)(VX3D_TARGET+(y*VX3D_SIDE+x)*4));
}
static int blit_test(const vx3d_io *io,vx3d_report *r) {
 /* VX855/H6 2D register map, OGPM part I pp74-80 and OpenChrome H6
  * solid fill. Save all changed non-command state. No source fetch, DMA,
  * quick-start, pattern fetch, tiling, or display address programming. */
 static const uint32_t at[7]={0x04,0x08,0x0C,0x10,0x14,0x48,0x58};
 uint32_t saved[7],i,x,y,c,expected;
 for(i=0;i<7;i++) saved[i]=rd(io,at[i]);
 wr(io,r,0x04,0x300); /* 32 bits per pixel */
 wr(io,r,0x14,VX3D_TARGET>>3);
 wr(io,r,0x08,(VX3D_SIDE*4/8)<<16);
 wr(io,r,0x10,(12UL<<16)|12);
 wr(io,r,0x0C,(15UL<<16)|15); /* 16 by 16 */
 wr(io,r,0x48,0); /* no keying or byte masking */
 wr(io,r,0x58,VX3D_BLIT_COLOR);
 wr(io,r,0x00,0xF0002001UL); /* COPY pattern, fixed color, BitBLT */
 if(!idle(io,r)) return 0;
 r->status_blit=r->status_after;
 r->blit_sample=pixel(io,16,16);
 for(y=0;y<VX3D_SIDE;y++) for(x=0;x<VX3D_SIDE;x++) {
  c=pixel(io,x,y);
  expected=(x>=12 && x<28 && y>=12 && y<28)?VX3D_BLIT_COLOR:VX3D_BACKGROUND;
  if(c!=expected) r->blit_errors++;
  if(c!=VX3D_BACKGROUND) r->blit_changed_pixels++;
 }
 /* None of these writes can fire: only register 0 is a command and our
  * quick-start bit is clear. Do not replay the old command on restore. */
 for(i=0;i<7;i++) wr(io,r,at[i],saved[i]);
 return 1;
}
static const uint32_t fixed_vertices[15]={
 0x41000000UL,0x41000000UL,0x3F000000UL,0x3F800000UL,0xFFE04020UL,
 0x42600000UL,0x41000000UL,0x3F000000UL,0x3F800000UL,0xFFE04020UL,
 0x41000000UL,0x42600000UL,0x3F000000UL,0x3F800000UL,0xFFE04020UL
};
static uint32_t guards(const vx3d_io *io,const vx3d_report *r) {
 uint32_t at,bad=0,slot,word,want;
 for(at=VX3D_ARENA;at<VX3D_ARENA+VX3D_ARENA_BYTES;at+=4) {
  if(at>=VX3D_TARGET && at<VX3D_TARGET+VX3D_PIXELS*4) continue;
  if(at>=VX3D_DEPTH && at<VX3D_DEPTH+VX3D_PIXELS*4) continue;
  if(r->sf_matrix_count && at>=VX3D_VERTICES &&
     at<VX3D_VERTICES+r->sf_matrix_count*VX3D_CASE_SOURCE_BYTES) {
   slot=(at-VX3D_VERTICES)/VX3D_CASE_SOURCE_BYTES;
   word=((at-VX3D_VERTICES)%VX3D_CASE_SOURCE_BYTES)/4;
   want=word<r->sf_matrix[slot].config[6]?r->sf_matrix[slot].input[word]:VX3D_CANARY;
   if(io->read_fb(io->context,at)!=want) bad++;
   continue;
  }
  if(r->vertex_stride && at>=VX3D_VERTICES && at<VX3D_VERTICES+60) {
   if(io->read_fb(io->context,at)!=fixed_vertices[(at-VX3D_VERTICES)/4]) bad++;
   continue;
  }
  if(io->read_fb(io->context,at)!=VX3D_CANARY) bad++;
 }
 return bad;
}
static uint32_t outside(const vx3d_io *io) {
 uint32_t x,y,bad=0;
 /* Check a safe margin around the triangle; don't assume an edge rule. */
 for(y=0;y<VX3D_SIDE;y++) for(x=0;x<VX3D_SIDE;x++)
  if(x<6 || y<6 || x+y>68)
   if(pixel(io,x,y)!=VX3D_BACKGROUND) bad++;
 return bad;
}
static uint32_t solid_check(const vx3d_io *io,uint32_t rgb) {
 uint32_t x,y,bad=outside(io);
 for(y=12;y<=40;y+=4) for(x=12;x<=40;x+=4)
  if(x+y<=56 && (pixel(io,x,y)&0xFFFFFFUL)!=rgb) bad++;
 return bad;
}
static uint32_t shade_check(const vx3d_io *io) {
 uint32_t x,y,c,rr,gg,bb,er,eg,eb,bad=outside(io);
 int dr,dg,db;
 /* Interior samples have a tolerance for subpixel origin and rounding.
  * This also rejects unchanged, solid-filled, wrong-order and blank output. */
 for(y=12;y<=36;y+=8) for(x=12;x<=36;x+=8) if(x+y<=52) {
  c=pixel(io,x,y); rr=(c>>16)&255; gg=(c>>8)&255; bb=c&255;
  er=(64-x-y)*255/48; eg=(x-8)*255/48; eb=(y-8)*255/48;
  dr=(int)rr-(int)er; dg=(int)gg-(int)eg; db=(int)bb-(int)eb;
  if(dr<-12 || dr>12 || dg<-12 || dg>12 || db<-12 || db>12) bad++;
 }
 return bad;
}
void vx3d_run(const vx3d_io *io, vx3d_report *r,uint32_t *backup) {
 uint32_t i,start=io->milliseconds(io->context), saved=0;
 int initialized;
 profile(r);
 r->target_offset=VX3D_TARGET; r->pitch=VX3D_SIDE*4;
 r->width=VX3D_SIDE; r->height=VX3D_SIDE;
 r->status_before=rd(io,0x400); r->stage=1;
 if(!idle(io,r)) { r->result=VX3D_BUSY; goto done; }
 for(i=0;i<VX3D_ARENA_BYTES;i+=4)
  backup[i/4]=io->read_fb(io->context,VX3D_ARENA+i);
 saved=1; clear(io); r->stage=2;
 setup_command_path(io,r);
 if(!idle(io,r)) {r->result=VX3D_TIMEOUT; goto fault;}
 r->status_setup=r->status_after; r->stage=7; r->phase=1;
 if(!blit_test(io,r)) goto timeout;
 r->checks_run|=1;
 r->guard_errors=guards(io,r);
 if(r->guard_errors) {r->result=VX3D_GUARD_FAILURE; goto capture;}
 if(r->blit_errors) {r->result=VX3D_BLIT_FAILURE; goto capture;}
 clear(io); r->phase=2; setup_global(io,r); r->stage=2;
 if(!idle(io,r)) {r->result=VX3D_TIMEOUT; goto fault;}
 r->phase=3; initialized=no_texture(io,r);
 if(!initialized) goto timeout;
 if(initialized<0) {r->result=VX3D_INIT_FAILURE; goto capture;}
 setup(io,r);
 if(!idle(io,r)) { r->result=VX3D_TIMEOUT; goto fault; }
 r->status_setup=r->status_after;
 if(!triangle(io,r,0,0,0x3F000000UL,0,0,0)) goto timeout;
 r->stage=3; r->solid_sample=pixel(io,16,16);
 r->status_solid=r->status_after;
 r->guard_errors=guards(io,r); r->solid_errors=solid_check(io,0xE04020);
 r->checks_run|=2;
 if(r->guard_errors) {r->result=VX3D_GUARD_FAILURE; goto capture;}
 if(r->solid_errors) {r->result=VX3D_SOLID_FAILURE; goto capture;}
 clear(io);
 if(!triangle(io,r,0x4000,0,0x3F000000UL,
  0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL)) goto timeout;
 r->stage=4; r->shade_sample=pixel(io,16,16);
 r->guard_errors=guards(io,r); r->shade_errors=shade_check(io);
 r->checks_run|=4;
 if(r->guard_errors) {r->result=VX3D_GUARD_FAILURE; goto capture;}
 if(r->shade_errors) {r->result=VX3D_SHADE_FAILURE; goto capture;}
 /* Save the shaded image before the depth test. */
 for(i=0;i<VX3D_PIXELS;i++) {
  r->image[i]=io->read_fb(io->context,VX3D_TARGET+i*4);
  if(r->image[i]!=VX3D_BACKGROUND) r->changed_pixels++;
 }
 clear(io);
 /* LESS+WRITE: near green, then far red must stay green. Finally closer
  * blue must replace it. This rejects an ignored Z buffer or a dead engine. */
 if(!triangle(io,r,0x1000,0xC,0x3E800000UL,
  0xFF00FF00UL,0xFF00FF00UL,0xFF00FF00UL)) goto timeout;
 if(!triangle(io,r,0x1000,0xC,0x3F400000UL,
  0xFFFF0000UL,0xFFFF0000UL,0xFFFF0000UL)) goto timeout;
 r->stage=5; r->depth_errors=solid_check(io,0x00FF00);
 if(!triangle(io,r,0x1000,0xC,0x3E000000UL,
  0xFF0000FFUL,0xFF0000FFUL,0xFF0000FFUL)) goto timeout;
 r->depth_sample=pixel(io,16,16); r->depth_errors+=solid_check(io,0x0000FF);
 r->guard_errors=guards(io,r); r->checks_run|=8;
 if(r->guard_errors) {r->result=VX3D_GUARD_FAILURE; goto capture;}
 if(r->depth_errors) {r->result=VX3D_DEPTH_FAILURE; goto capture;}
 r->result=VX3D_PASS; r->stage=6; goto restore;
capture:
 r->changed_pixels=0;
 for(i=0;i<VX3D_PIXELS;i++) {
  r->image[i]=io->read_fb(io->context,VX3D_TARGET+i*4);
  if(r->image[i]!=VX3D_BACKGROUND) r->changed_pixels++;
 }
restore:
 /* Only restore scratch memory after all submitted rendering is idle. */
 r->phase=7;
 if(r->stage!=7) {type(io,r,0x01); reg(io,r,0x01,0);}
 if(!idle(io,r)) goto timeout;
 if(saved) for(i=0;i<VX3D_ARENA_BYTES;i+=4)
  io->write_fb(io->context,VX3D_ARENA+i,backup[i/4]);
 if(saved) (void)io->read_fb(io->context,VX3D_ARENA+VX3D_ARENA_BYTES-4);
 goto done;
timeout:
 r->result=VX3D_TIMEOUT;
fault:
 /* Do not issue more commands or overwrite buffers owned by a hung GPU. */
 r->fault_latched=1;
done:
 r->elapsed_ms=(uint32_t)(io->milliseconds(io->context)-start);
}

/* Fixed independent probes. No caller-supplied commands or GPU addresses. */
static uint32_t primitive_check(const vx3d_io *io,int point) {
 uint32_t x,y,c,count=0,bad=0,hit;
 for(y=0;y<VX3D_SIDE;y++) for(x=0;x<VX3D_SIDE;x++) {
  c=pixel(io,x,y);
  if(c==VX3D_BACKGROUND) continue;
  count++;
  if((c&0xFFFFFFUL)!=0xE04020UL) bad++;
  if(point) {if(x<26 || x>37 || y<26 || y>37) bad++;}
  else if(x<6 || x>58 || y<6 || y>10) bad++;
 }
 if(point) {
  if(count!=64 || (pixel(io,32,32)&0xFFFFFFUL)!=0xE04020UL) bad++;
 } else {
  if(count<48 || count>49) bad++;
  for(x=12;x<=48;x+=6) {
   hit=0;
   for(y=7;y<=9;y++) if((pixel(io,x,y)&0xFFFFFFUL)==0xE04020UL) hit=1;
   if(!hit) bad++;
  }
 }
 return bad;
}
static void probe_submit(const vx3d_io *io,vx3d_report *r) {
 uint32_t id=r->probe_id,words=5,n=3,mode=2,shading=0;
 uint32_t mask=0xF40000UL,i,j,index,value,vertices[15];
 memcpy(vertices,fixed_vertices,sizeof(vertices));
 if(r->path_flags&64) {
  /* (-.75,+.75), (+.75,+.75), (-.75,-.75), Z=.5/W=1.
   * The explicit viewport produces the same (8,8),(56,8),(8,56). */
  vertices[0]=0xBF400000UL; vertices[1]=0x3F400000UL;
  vertices[5]=0x3F400000UL; vertices[6]=0x3F400000UL;
  vertices[10]=0xBF400000UL; vertices[11]=0xBF400000UL;
 }
 if(id==2) {words=4; mask=0xF00000UL;}
 if(id==3) {words=2; mask=0xC00000UL;}
 if(id==4) shading=0x1000; /* flat vertex a */
 if(id==5) {
  shading=0x4000;
  vertices[4]=0xFFFF0000UL; vertices[9]=0xFF00FF00UL; vertices[14]=0xFF0000FFUL;
 }
 if(id==8) {n=1; mode=0; vertices[0]=0x42000000UL; vertices[1]=0x42000000UL;}
 if(id==9) {n=2; mode=1;}
 r->vertex_words=words; r->vertex_vertices=n;
 type(io,r,0x04);
 reg(io,r,0x00,words);
 reg(io,r,0x01,words==2?0x000100UL:0x020100UL);
 reg(io,r,0x02,words==5?0x000503UL:0x000003UL);
 reg(io,r,0x25,mask); reg(io,r,0x28,mask);
 reg(io,r,0x2F,0x400000); /* defaults: Z=0, W=1 */
 if(id==8) {
  /* OGPM p162: s[8].15 is the high 24 bits of IEEE float. */
  reg(io,r,0x2A,0x410000); reg(io,r,0x2B,0x410000); /* 8x8 */
 }
 if(id==10 || id==11) {
  /* OGPM pp154,163: fixed local VB and bounded 16-bit indices.
   * Test both stride interpretations; all possible vertex reads stay
   * inside the initialized 256-byte local source reservation. */
  r->vertex_stride=id==10?20:5;
  for(i=0;i<15;i++) io->write_fb(io->context,VX3D_VERTICES+i*4,vertices[i]);
  (void)io->read_fb(io->context,VX3D_VERTICES+56);
  reg(io,r,0x20,1); reg(io,r,0x21,VX3D_VERTICES&0xFFFFFFUL);
  reg(io,r,0x22,(r->vertex_stride<<8)|(VX3D_VERTICES>>24));
  reg(io,r,0x2C,2); reg(io,r,0x2D,0); reg(io,r,0x2E,0);
 }
 reg(io,r,0x23,shading|mode|(id==9?0x8000UL:0));
 reg(io,r,0x24,n);
 type(io,r,0);
 if(id==10 || id==11) {
  wr(io,r,0x440,0x00010000UL); wr(io,r,0x440,0x00000002UL);
  return;
 }
 for(i=0;i<n;i++) {
  index=(id==6 && i)?3-i:i; /* reverse b/c with culling disabled */
  for(j=0;j<words;j++) {
   value=vertices[index*5+j];
   if(id==7) {io->write_mmio(io->context,0x440,value); r->writes++;}
   else wr(io,r,0x440,value);
  }
 }
 if(id==7) status(r,rd(io,0x400));
}
static uint32_t depth_only_check(const vx3d_io *io,vx3d_report *r) {
 uint32_t x,y,v,bad=0;
 for(y=0;y<VX3D_SIDE;y++) for(x=0;x<VX3D_SIDE;x++) {
  v=io->read_fb(io->context,(uint32_t)(VX3D_DEPTH+(y*VX3D_SIDE+x)*4));
  if(v!=0xFFFFFFFFUL) r->depth_changed_pixels++;
  if((x<6 || y<6 || x+y>68) && v!=0xFFFFFFFFUL) bad++;
  if(pixel(io,x,y)!=VX3D_BACKGROUND) bad++; /* color writes are disabled */
 }
 for(y=12;y<=40;y+=4) for(x=12;x<=40;x+=4) if(x+y<=56) {
  v=io->read_fb(io->context,(uint32_t)(VX3D_DEPTH+(y*VX3D_SIDE+x)*4));
  if(v<0x7FFFFFF0UL || v>0x80000010UL) bad++;
 }
 if(r->depth_changed_pixels<1000 || r->depth_changed_pixels>1300) bad++;
 r->depth_sample=io->read_fb(io->context,VX3D_DEPTH+(16*VX3D_SIDE+16)*4);
 return bad;
}
void vx3d_run_probe(const vx3d_io *io,vx3d_report *r,uint32_t *backup) {
 uint32_t i,start=io->milliseconds(io->context),saved=0;
 int initialized;
 profile(r);
 r->target_offset=VX3D_TARGET; r->pitch=VX3D_SIDE*4;
 r->width=VX3D_SIDE; r->height=VX3D_SIDE;
 r->status_before=rd(io,0x400); r->stage=1;
 if(!idle(io,r)) {r->result=VX3D_BUSY; goto done;}
 for(i=0;i<VX3D_ARENA_BYTES;i+=4) backup[i/4]=io->read_fb(io->context,VX3D_ARENA+i);
 saved=1; clear(io); r->stage=2; setup_command_path(io,r);
 if(!idle(io,r)) goto timeout;
 r->stage=7; r->phase=1;
 if(!blit_test(io,r)) goto timeout;
 r->checks_run|=1; r->guard_errors=guards(io,r);
 if(r->guard_errors) {r->result=VX3D_GUARD_FAILURE; goto capture;}
 if(r->blit_errors) {r->result=VX3D_BLIT_FAILURE; goto capture;}
 clear(io); r->stage=2; r->phase=2; setup_global(io,r);
 if(!idle(io,r)) goto timeout;
 r->phase=3; initialized=no_texture(io,r);
 if(!initialized) goto timeout;
 if(initialized<0) {r->result=VX3D_INIT_FAILURE; goto capture;}
 setup(io,r);
 if(!idle(io,r)) goto timeout;
 r->status_setup=r->status_after;
 if(r->path_flags&8) {r->phase=4; if(!acceptance_before(io,r)) goto timeout;}
 r->phase=5;
 if((r->path_flags&2) && !synchronize(io,r,0x00400000UL)) goto timeout;
 probe_submit(io,r);
 if(!synchronize(io,r,0x00800000UL)) goto timeout;
 r->stage=3; r->status_solid=r->status_after;
 /* Check pixels/guards only after the post-draw diagnostics too. A fence
  * observation can never bypass the final framebuffer validation. */
 if(!acceptance_after(io,r)) goto timeout;
 r->solid_sample=pixel(io,16,16);
 r->guard_errors=guards(io,r);
 if(r->path_flags&256) {
  r->checks_run|=64; r->depth_errors=depth_only_check(io,r);
  r->result=r->depth_errors?VX3D_DEPTH_FAILURE:VX3D_PASS;
 } else if(r->probe_id==5) {
  r->checks_run|=4; r->shade_sample=pixel(io,16,16);
  r->shade_errors=shade_check(io);
  r->result=r->shade_errors?VX3D_SHADE_FAILURE:VX3D_PASS;
 } else {
  if(r->probe_id==8) {r->checks_run|=16; r->solid_sample=pixel(io,32,32); r->solid_errors=primitive_check(io,1);}
  else if(r->probe_id==9) {r->checks_run|=32; r->solid_sample=pixel(io,16,8); r->solid_errors=primitive_check(io,0);}
  else {r->checks_run|=2; r->solid_errors=solid_check(io,0xE04020);}
  r->result=r->solid_errors?VX3D_SOLID_FAILURE:VX3D_PASS;
 }
 if(r->guard_errors) r->result=VX3D_GUARD_FAILURE;
 if(r->result==VX3D_PASS) r->stage=6;
capture:
 for(i=0;i<VX3D_PIXELS;i++) {
  r->image[i]=io->read_fb(io->context,VX3D_TARGET+i*4);
  if(r->image[i]!=VX3D_BACKGROUND) r->changed_pixels++;
 }
 r->phase=7;
 if(r->stage!=7) {type(io,r,0x01); reg(io,r,0x01,0);}
 if(!idle(io,r)) goto timeout;
 if(saved) for(i=0;i<VX3D_ARENA_BYTES;i+=4) io->write_fb(io->context,VX3D_ARENA+i,backup[i/4]);
 if(saved) (void)io->read_fb(io->context,VX3D_ARENA+VX3D_ARENA_BYTES-4);
 goto done;
timeout:
 r->result=VX3D_TIMEOUT; r->fault_latched=1;
done:
 r->elapsed_ms=(uint32_t)(io->milliseconds(io->context)-start);
}

/* A bounded, fixed command list in LOCAL framebuffer memory. The header
 * layouts and 60..68 launch sequence are checked against original VIA Linux
 * macros and the user's exact s3ginv.dll BC770..BCA94 CPU routine. This is
 * the AGP command fetcher, with HAGPBst location 00 (HCM p63); there are no
 * system-memory, AGP-aperture, GART, branch, IRQ or user-supplied addresses. */
typedef struct dma_builder {
 const vx3d_io *real;
 vx3d_report *report;
 uint32_t *words,count,error;
} dma_builder;
static void dma_word(dma_builder *b,uint32_t word) {
 if(b->count>=VX3D_DMA_WORDS) {b->error=1; return;}
 b->words[b->count++]=word;
}
static void dma_align(dma_builder *b) {
 while((b->count&3) && !b->error) dma_word(b,0xCC000000UL);
}
static void dma_header(dma_builder *b,uint32_t at,uint32_t value) {
 dma_align(b);
 if(at==0x43C && !value) {
  dma_word(b,0xFE03043CUL);
  dma_word(b,b->report->vertex_words*b->report->vertex_vertices);
  dma_word(b,0); dma_word(b,0xCC000000UL);
 } else {
  dma_word(b,0xFE020000UL|(at+4)); dma_word(b,at);
  dma_word(b,value); dma_word(b,0xCC000000UL);
 }
}
static uint32_t dma_read_mmio(void *context,uint32_t at) {
 dma_builder *b=(dma_builder*)context;
 return b->real->read_mmio(b->real->context,at);
}
static void dma_write_mmio(void *context,uint32_t at,uint32_t value) {
 dma_builder *b=(dma_builder*)context;
 if(at==0x43C || at==0x41C) dma_header(b,at,value);
 else if(at==0x440 || at==0x420) dma_word(b,value);
 else b->error=1;
}
static uint32_t dma_no_fb_read(void *context,uint32_t at) {
 dma_builder *b=(dma_builder*)context;
 (void)at; b->error=1; return 0;
}
static void dma_no_fb_write(void *context,uint32_t at,uint32_t value) {
 dma_builder *b=(dma_builder*)context;
 (void)at; (void)value; b->error=1;
}
static uint32_t dma_milliseconds(void *context) {
 dma_builder *b=(dma_builder*)context;
 return b->real->milliseconds(b->real->context);
}
static void dma_wait(dma_builder *b,uint32_t mask) {
 dma_align(b);
 dma_word(b,0xFE000000UL); dma_word(b,1); dma_word(b,0);
 dma_word(b,0xCC000000UL); dma_word(b,0x6C); dma_word(b,mask);
}
static int dma_build(const vx3d_io *io,vx3d_report *r,uint32_t which) {
 dma_builder b; vx3d_io record;
 uint32_t writes=r->writes;
 b.real=io; b.report=r; b.words=r->dma_program[which]; b.count=0; b.error=0;
 record=*io; record.context=&b; record.read_mmio=dma_read_mmio;
 record.write_mmio=dma_write_mmio;
 record.read_fb=dma_no_fb_read; record.write_fb=dma_no_fb_write;
 record.milliseconds=dma_milliseconds;
 r->dma_expected[which]=r->queue_fence_original^(which?0x2D430013UL:0x1C320013UL);
 if(r->dma_expected[which]==0xFFFFFFFFUL) r->dma_expected[which]^=0x01000000UL;
 if(which) {
  /* These exact existing state and vertex encoders now write packets to
   * the list; they do not send rendering parameters/vertices to MMIO. */
  clear_general(&record,r); setup(&record,r);
  dma_wait(&b,0x00400000UL); probe_submit(&record,r);
  dma_wait(&b,0x00800000UL);
 }
 dma_header(&b,0x41C,0x00110000UL);
 dma_word(&b,0x06000000UL|(r->dma_expected[which]&0xFFFFFFUL));
 dma_word(&b,0x07100000UL|((r->dma_expected[which]>>24)<<8));
 if(which && r->probe_id==VX3D_SF_PROBE) {
  /* Either separately checked T11 destination may supply the post-draw
   * fence. Both carry the same ID; the controls use distinct IDs. */
  dma_header(&b,0x43C,0x00110000UL);
  dma_word(&b,0x06000000UL|(r->dma_expected[which]&0xFFFFFFUL));
  dma_word(&b,0x07100000UL|((r->dma_expected[which]>>24)<<8));
 }
 /* Match VIA's dummy tail, with H6S2's conservative 128-byte alignment.
  * The completion fence is before this tail, not in the last fetch block. */
 dma_header(&b,0x41C,0x00300000UL);
 do {
  dma_word(&b,r->probe_id==VX3D_SF_PROBE?0xCCC00002UL:0xCCCCCCC0UL);
  dma_word(&b,r->probe_id==VX3D_SF_PROBE?0xDDD00002UL:0xDDD00000UL);
 } while((b.count&(r->probe_id==VX3D_SF_PROBE?63:31)) && !b.error);
 r->writes=writes; /* Recorded packet stores are not physical MMIO writes. */
 if(b.error) return 0;
 r->dma_count[which]=b.count;
 r->dma_stop[which]=VX3D_QUEUE+b.count*4-4;
 r->dma_flags|=which?8:2;
 r->dma_phase=which?5:2;
 return 1;
}
static int dma_clear_cycle(const vx3d_io *io,vx3d_report *r,uint32_t which) {
 uint32_t id=r->queue_fence_original^(0x3500D400UL|which);
 type(io,r,0x10); reg(io,r,0x68,0x0A0008); /* documented clear, no trigger */
 if(!idle(io,r) || !fence_set(io,r,id)) return 0;
 r->dma_park_fence[which]=rd(io,0x494);
 if(r->dma_park_fence[which]!=id) return -1;
 if(!fence_set(io,r,r->queue_fence_original)) return 0;
 if(rd(io,0x494)!=r->queue_fence_original) return -1;
 return 1;
}
static void dma_check_memory(const vx3d_io *io,vx3d_report *r,uint32_t which) {
 uint32_t at,index,want,got;
 for(at=VX3D_QUEUE_REGION;at<VX3D_QUEUE_REGION_END;at+=4) {
  want=VX3D_CANARY;
  if(at>=VX3D_QUEUE && at<VX3D_QUEUE+VX3D_QUEUE_BYTES) {
   index=(at-(uint32_t)VX3D_QUEUE)/4;
   want=index<r->dma_count[which]?r->dma_program[which][index]:0xCC000000UL;
  }
  got=io->read_fb(io->context,at);
  if(got!=want) {
   if(at>=VX3D_QUEUE && at<VX3D_QUEUE+VX3D_QUEUE_BYTES) r->dma_source_errors++;
   else r->dma_guard_errors++;
   r->guard_errors++;
  }
 }
 if(!r->dma_source_errors) r->dma_flags|=128;
 if(!r->dma_guard_errors) r->dma_flags|=256;
}
/* 1 done, 0 actual wait failure, -1 fetch not confirmed, -2 park not
 * confirmed, -3 source/guard error, -4 encoder overflow. */
static int dma_run_buffer(const vx3d_io *io,vx3d_report *r,uint32_t which) {
 uint32_t i;
 int state;
 if(!dma_build(io,r,which)) return -4;
 for(i=0;i<VX3D_QUEUE_BYTES/4;i++)
  io->write_fb(io->context,VX3D_QUEUE+i*4,i<r->dma_count[which]?r->dma_program[which][i]:0xCC000000UL);
 (void)io->read_fb(io->context,VX3D_QUEUE+VX3D_QUEUE_BYTES-4);
 r->dma_cursor[which][0]=rd(io,0x43C); r->dma_status[which][0]=rd(io,0x444);
 type(io,r,0x10);
 reg(io,r,0x60,VX3D_QUEUE); reg(io,r,0x61,0);
 reg(io,r,0x62,VX3D_QUEUE+VX3D_QUEUE_BYTES); reg(io,r,0x63,0);
 reg(io,r,0x64,r->dma_stop[which]&0xFFFFFFUL); reg(io,r,0x65,0x200);
 reg(io,r,0x68,0x0A0001);
 r->dma_phase=which?6:3;
 if(!idle(io,r)) return 0;
 r->dma_cursor[which][1]=rd(io,0x43C); r->dma_status[which][1]=rd(io,0x444);
 r->dma_observed[which]=rd(io,0x494);
 if(r->dma_observed[which]!=r->dma_expected[which]) return -1;
 r->dma_flags|=which?16:4;
 state=dma_clear_cycle(io,r,which);
 if(!state) return 0;
 if(state!=1) return -2;
 r->dma_flags|=which?64:32;
 r->dma_phase=which?7:4;
 dma_check_memory(io,r,which);
 if(r->dma_source_errors || r->dma_guard_errors) return -3;
 return 1;
}

/* Focused transport comparison. Same guarded triangle and complete state;
 * only A74 bit0 differs. This is the PCI virtual queue used by OpenChrome,
 * not an AGP/DMA command list or caller-supplied command buffer. */
static int queue_read(const vx3d_io *io,vx3d_report *r,uint32_t *words) {
 return read_state(io,r,0x1070,words) && read_state(io,r,0x1074,words+4);
}
static int queue_match(const uint32_t *words,const uint32_t *values) {
 uint32_t i,v;
 for(i=0;i<5;i++)
  if(!selected(0x1070+i,words+(i&4),&v) || v!=values[i]) return 0;
 return 1;
}
static int queue_snapshot(const vx3d_io *io,vx3d_report *r) {
 uint32_t i,v,other;
 r->queue_phase=1;
 if(!queue_read(io,r,r->queue_original) ||
    !read_state(io,r,0x107C,r->queue_other_original)) return 0;
 r->queue_flags|=VX3D_Q_SNAPSHOT|VX3D_Q_WO_OBSERVATION;
 for(i=0;i<5;i++) if(!selected(0x1070+i,r->queue_original+(i&4),&v)) {
  r->queue_flags|=VX3D_Q_BAD_READBACK;
 }
 if(!selected(0x107C,r->queue_other_original,&other)) {
  r->queue_flags|=VX3D_Q_BAD_READBACK;
 }
 /* These controls are WO in OGPM. Even plausible tagged values cannot
  * establish the inherited enable bits. Initialize our own OFF baseline
  * before touching any framebuffer bytes; never replay this snapshot. */
 r->queue_fence_original=rd(io,0x494);
 return 1;
}
static int queue_claim(const vx3d_io *io,vx3d_report *r) {
 uint32_t id=r->queue_fence_original^0x4570A510UL;
 if(r->queue_fence_original==0xFFFFFFFFUL) return -1;
 if(!idle(io,r)) return 0;
 r->queue_flags|=VX3D_Q_OWNED;
 type(io,r,0x10); reg(io,r,0x74,0x301000); reg(io,r,0x7C,0);
 reg(io,r,0,0); reg(io,r,2,0); reg(io,r,7,0); reg(io,r,0x68,0x0A0000);
 type(io,r,0x11); reg(io,r,0,0x248);
 if(!idle(io,r) || !fence_set(io,r,id)) return 0;
 r->queue_claim_fence=rd(io,0x494);
 if(r->queue_claim_fence!=id) return -1;
 if(!fence_set(io,r,r->queue_fence_original)) return 0;
 r->queue_fence_restored=rd(io,0x494);
 if(r->queue_fence_restored!=r->queue_fence_original) return -1;
 return 1;
}
static int queue_changed(const uint32_t *before,const uint32_t *after) {
 uint32_t i;
 for(i=0;i<5;i++) if(before[i]!=after[i]) return 1;
 return 0;
}
static void queue_observe(vx3d_report *r,const uint32_t *expected) {
 if(!queue_match(r->queue_active,expected)) {
  r->queue_flags|=VX3D_Q_CONFIG_MISMATCH;
 } else if(queue_changed(r->queue_original,r->queue_active)) {
  /* Positive calibration only: a supported bank must change AND match
   * every requested field before a later contradiction can veto cleanup. */
  r->queue_flags|=VX3D_Q_READBACK_RESPONDED;
 }
}
static int queue_program(const vx3d_io *io,vx3d_report *r) {
 uint32_t expected[5];
 r->queue_phase=2;
 r->queue_start=VX3D_QUEUE; r->queue_end=VX3D_QUEUE+VX3D_QUEUE_BYTES;
 r->queue_length=VX3D_QUEUE_BYTES/8;
 r->queue_storage_bytes=VX3D_QUEUE_STORAGE;
 r->queue_control=(r->path_flags&1024)?0x74301001UL:0x74301000UL;
 expected[0]=r->queue_start; expected[1]=r->queue_end;
 expected[2]=0; expected[3]=r->queue_length; expected[4]=r->queue_control&0xFFFFFFUL;
 /* OpenChrome viaEnablePCIeVQ order and /8 length formula. End is exclusive.
  * The entire /16-manual interpretation is also backed and guarded. */
 type(io,r,0x10); reg(io,r,0x72,0);
 reg(io,r,0x70,r->queue_start); reg(io,r,0x71,r->queue_end);
 reg(io,r,0x73,r->queue_length); reg(io,r,0x74,r->queue_control&0xFFFFFFUL);
 reg(io,r,0,0);
 if(r->path_flags&1024) r->queue_flags|=VX3D_Q_ENABLED;
 if(!idle(io,r) || !queue_read(io,r,r->queue_active)) return 0;
 queue_observe(r,expected);
 return 1;
}
static int queue_restore(const vx3d_io *io,vx3d_report *r) {
 uint32_t id=r->queue_fence_original^0x4560D310UL,v;
 r->queue_phase=4;
 /* The driver owns the CR. Leave an initialized OFF baseline, as
  * OpenChrome does; the WO snapshots are not restorable hardware state.
  * Drain, post the documented disable, wait for both engines, then use a
  * changing, memory-free CR fence before restoring CPU-owned scratch. */
 if(!idle(io,r)) return 0;
 type(io,r,0x10); reg(io,r,0x74,0x301000); reg(io,r,0x7C,0);
 if(!synchronize(io,r,0x00400000UL) ||
    !synchronize(io,r,0x00800000UL) || !fence_set(io,r,id)) return 0;
 r->queue_disable_fence=rd(io,0x494);
 if(r->queue_disable_fence!=id) return -1;
 if(!fence_set(io,r,r->queue_fence_original)) return 0;
 r->queue_fence_restored=rd(io,0x494);
 if(r->queue_fence_restored!=r->queue_fence_original) return -1;
 if(!queue_read(io,r,r->queue_disabled)) return 0;
 if((r->queue_flags&VX3D_Q_READBACK_RESPONDED) &&
    (!selected(0x1074,r->queue_disabled+4,&v) || v!=0x301000)) {
  r->queue_flags|=VX3D_Q_RESTORE_MISMATCH; return -1;
 }
 r->queue_flags|=VX3D_Q_DISABLED;
 memcpy(r->queue_restored,r->queue_disabled,sizeof(r->queue_restored));
 r->queue_flags|=VX3D_Q_RESTORED; r->queue_phase=5;
 return 1;
}
static void queue_check_memory(const vx3d_io *io,vx3d_report *r) {
 uint32_t at,v;
 for(at=VX3D_QUEUE_REGION;at<VX3D_QUEUE_REGION_END;at+=4) {
  v=io->read_fb(io->context,at);
  if(at>=VX3D_QUEUE && at<VX3D_QUEUE+VX3D_QUEUE_BYTES) {
   if(v) r->queue_changed_words++;
  } else if(v!=VX3D_CANARY) r->queue_guard_errors++;
 }
 r->guard_errors+=r->queue_guard_errors;
}
void vx3d_run_transport(const vx3d_io *io,vx3d_report *r,
 uint32_t *backup,uint32_t *queue_backup) {
 uint32_t i,at,start=io->milliseconds(io->context),saved=0;
 int state;
 profile(r);
 r->target_offset=VX3D_TARGET; r->pitch=VX3D_SIDE*4;
 r->width=VX3D_SIDE; r->height=VX3D_SIDE;
 r->status_before=rd(io,0x400); r->stage=1;
 if(!idle(io,r)) {r->result=VX3D_BUSY; goto done;}
 state=queue_snapshot(io,r);
 if(!state) goto timeout;
 state=queue_claim(io,r);
 if(!state) goto timeout;
 if(state<0) goto unconfirmed;
 if(r->path_flags&4096) {
  r->dma_phase=1; r->dma_base=VX3D_QUEUE;
  r->dma_limit=VX3D_QUEUE+VX3D_QUEUE_BYTES;
  type(io,r,0x10); reg(io,r,0x69,0); /* no command-buffer branching */
  state=dma_clear_cycle(io,r,0);
  if(!state) goto timeout;
  if(state<0) goto unconfirmed;
  r->dma_flags|=1;
 }
 for(i=0;i<VX3D_ARENA_BYTES;i+=4) backup[i/4]=io->read_fb(io->context,VX3D_ARENA+i);
 for(i=0;i<VX3D_QUEUE_SPAN;i+=4) queue_backup[i/4]=io->read_fb(io->context,VX3D_QUEUE_REGION+i);
 saved=1; clear(io);
 for(at=VX3D_QUEUE_REGION;at<VX3D_QUEUE_REGION_END;at+=4)
  io->write_fb(io->context,at,(at>=VX3D_QUEUE && at<VX3D_QUEUE+VX3D_QUEUE_BYTES)?0:VX3D_CANARY);
 (void)io->read_fb(io->context,VX3D_QUEUE_REGION_END-4);
 r->stage=2; state=queue_program(io,r);
 if(!state) goto timeout;
 r->stage=7; r->phase=1;
 if(!blit_test(io,r)) goto timeout;
 r->checks_run|=1; r->guard_errors=guards(io,r);
 if(r->guard_errors) {r->result=VX3D_GUARD_FAILURE; goto capture;}
 if(r->blit_errors) {r->result=VX3D_BLIT_FAILURE; goto capture;}
 clear(io); r->stage=2; r->phase=2; setup_global(io,r);
 if(!idle(io,r)) goto timeout;
 r->phase=3; state=no_texture(io,r);
 if(!state) goto timeout;
 if(state<0) {r->result=VX3D_INIT_FAILURE; goto capture;}
 if(r->path_flags&4096) {
  for(i=0;i<2;i++) {
   r->phase=i?5:4;
   state=dma_run_buffer(io,r,i);
   if(!state) goto timeout;
   if(state==-1) goto fetch_unconfirmed;
   if(state==-2) goto unconfirmed;
   if(state==-3) {r->result=VX3D_GUARD_FAILURE; goto capture;}
   if(state==-4) {r->result=VX3D_INIT_FAILURE; goto capture;}
  }
  goto rendered;
 }
 setup(io,r);
 if(!idle(io,r)) goto timeout;
 state=pipeline_audit(io,r);
 if(!state) goto timeout;
 if(state<0) {r->result=VX3D_INIT_FAILURE; goto capture;}
 r->status_setup=r->status_after; r->phase=4;
 if(!acceptance_before(io,r)) goto timeout;
 r->phase=5;
 if(!synchronize(io,r,0x00400000UL)) goto timeout;
 probe_submit(io,r);
 if(!synchronize(io,r,0x00800000UL)) goto timeout;
 r->stage=3; r->status_solid=r->status_after;
 if(!acceptance_after(io,r)) goto timeout;
 rendered:
 r->stage=3; r->status_solid=r->status_after;
 r->solid_sample=pixel(io,16,16); r->solid_errors=solid_check(io,0xE04020);
 r->checks_run|=2; r->guard_errors=guards(io,r);
 r->result=r->solid_errors?VX3D_SOLID_FAILURE:VX3D_PASS;
 if(r->guard_errors) r->result=VX3D_GUARD_FAILURE;
 capture:
 r->queue_phase=3;
 /* Capture and validate the target BEFORE transport/register/memory restore. */
 for(i=0;i<VX3D_PIXELS;i++) {
  r->image[i]=io->read_fb(io->context,VX3D_TARGET+i*4);
  if(r->image[i]!=VX3D_BACKGROUND) r->changed_pixels++;
 }
 if((r->checks_run&2) && (r->changed_pixels<1000 || r->changed_pixels>1300)) {
  r->solid_errors++;
  if(r->result==VX3D_PASS) r->result=VX3D_SOLID_FAILURE;
 }
 r->phase=7;
 if(!idle(io,r)) goto timeout;
 if(r->stage!=7) {type(io,r,1); reg(io,r,1,0);}
 if(!idle(io,r)) goto timeout;
 if(r->queue_flags&VX3D_Q_OWNED) {
  state=queue_restore(io,r);
  if(!state) goto timeout;
  if(state<0) goto unconfirmed;
 }
 if(!(r->path_flags&4096)) queue_check_memory(io,r);
 if(r->guard_errors) r->result=VX3D_GUARD_FAILURE;
 if(saved) {
  for(i=0;i<VX3D_ARENA_BYTES;i+=4) io->write_fb(io->context,VX3D_ARENA+i,backup[i/4]);
  for(i=0;i<VX3D_QUEUE_SPAN;i+=4) io->write_fb(io->context,VX3D_QUEUE_REGION+i,queue_backup[i/4]);
  (void)io->read_fb(io->context,VX3D_QUEUE_REGION_END-4);
  r->queue_flags|=VX3D_Q_MEMORY_RESTORED; r->queue_phase=6;
  if(r->path_flags&4096) r->dma_phase=8;
 }
 if(r->result==VX3D_PASS) r->stage=6;
 goto done;
 timeout:
 /* A hung/unconfirmed queue retains its backing. Never overwrite it or
  * issue another command; the wrapper keeps clocks on and latches failure. */
 r->result=VX3D_TIMEOUT; r->fault_latched=1;
 goto done;
 unconfirmed:
 r->queue_flags|=VX3D_Q_COMPLETION_UNCONFIRMED;
 r->result=VX3D_CONTROL_UNCONFIRMED; r->fault_latched=1;
 goto done;
 fetch_unconfirmed:
 r->result=VX3D_FETCH_UNCONFIRMED; r->fault_latched=1;
 done:
 r->elapsed_ms=(uint32_t)(io->milliseconds(io->context)-start);
}

/* v6 SF ring. All addresses are driver-owned: SF pages 0x220..0x22f map
 * pinned VMM pages 1..16; every other RTSF entry maps owned dummy storage.
 * No live ring byte is modified after publication. The next pause exposes
 * work already outside the preceding pause's conservative prefetch margin. */
#define SF_RING_BASE 0x00220000UL
#define SF_CONTROL0 0x400UL
#define SF_CONTROL1 0x800UL
#define SF_DRAW 0xC00UL
#define SF_BRANCH_DRAW 0x1C00UL
#define SF_ALT_DRAW 0x3000UL
#define SF_POINT_OFF 0x1400UL
#define SF_POINT_ON 0x2400UL
#define SF_MATRIX_BASE 0x4000UL
#define SF_MATRIX_STRIDE 0x600UL
#define SF_BRANCH_VERTEX 0x9000UL
/* A local CMDQ backing separate from the SF command-ring address and its
 * local dummy shadow. This 64 KiB is already saved in queue_backup. */
#define SF_AGP_QUEUE (VX3D_QUEUE+VX3D_QUEUE_BYTES)
static const uint32_t sf_gti_regs[5]={0x6A,0x6B,0x6C,0x6F,0x7B};
static uint32_t sf_dummy(uint32_t word) {
 static const uint32_t header[4]={0xFE020420UL,0x41C,0x00300000UL,0xCC000000UL};
 word&=63;
 if(word<4) return header[word];
 return (word&1)?0xDDD00002UL:0xCCC00002UL;
}
static uint32_t sf_ring_word(const vx3d_report *r,uint32_t word) {
 uint32_t base,i;
 base=SF_CONTROL0/4;
 if(word>=base && word-base<r->dma_count[0]) return r->dma_program[0][word-base];
 base=SF_CONTROL1/4;
 if(word>=base && word-base<64) return r->sf_control43c[word-base];
 if(r->sf_matrix_count) {
  for(i=0;i<r->sf_matrix_count;i++) {
   base=(SF_MATRIX_BASE+i*SF_MATRIX_STRIDE)/4;
   if(word>=base && word-base<r->sf_matrix[i].count)
    return r->sf_matrix[i].program[word-base];
  }
  /* Do not expose any previous-version draw on the way to the matrix. */
  return sf_dummy(word);
 }
 base=SF_DRAW/4;
 if(word>=base && word-base<r->dma_count[1]) return r->dma_program[1][word-base];
 base=SF_BRANCH_DRAW/4;
 if(word>=base && word-base<r->branch_control_count) return r->branch_control[word-base];
 base=SF_ALT_DRAW/4;
 if(word>=base && word-base<r->sf_alt_count) return r->sf_alt_program[word-base];
 base=SF_POINT_OFF/4;
 if(word>=base && word-base<r->sf_point_count[0]) return r->sf_point_program[0][word-base];
 base=SF_POINT_ON/4;
 if(word>=base && word-base<r->sf_point_count[1]) return r->sf_point_program[1][word-base];
 base=SF_BRANCH_VERTEX/4;
 if(word>=base && word-base<16) return r->branch_vertices[word-base];
 return sf_dummy(word);
}
static uint32_t sf_pte(const vx3d_report *r,uint32_t entry) {
 uint32_t first=SF_RING_BASE>>12;
 if(entry>=first && entry-first<16)
  return (r->sf_host_pte[entry-first+1]>>12)&0x3FFFFFFFUL;
 return (r->sf_host_pte[18]>>12)&0x3FFFFFFFUL;
}
static void sf_snapshot(const vx3d_io *io,vx3d_report *r,uint32_t *out) {
 out[0]=rd(io,0x400); status(r,out[0]);
 out[1]=rd(io,0x43C); out[2]=rd(io,0x444);
}
static void sf_seq_snapshot(const vx3d_sf_io *sf,uint32_t *out) {
 uint32_t i;
 for(i=0;i<5;i++) out[i]=sf->read_seq(sf->context,sf_gti_regs[i]);
}
static const uint32_t sf_2d_regs[7]={0x04,0x08,0x0C,0x10,0x14,0x48,0x58};
static void sf_branch_pad(dma_builder *b) {
 dma_header(b,0x41C,0x00300000UL);
 do {dma_word(b,0xCCC00002UL); dma_word(b,0xDDD00002UL);}
 while((b->count&63) && !b->error);
}
static int sf_build_control(vx3d_report *r,uint32_t which) {
 uint32_t *p=which?r->sf_control43c:r->dma_program[0];
 uint32_t n=0,id=r->sf_expected[which+1],i;
 /* FE82 raw-T11 branches stalled the physical reader in both v6.3 runs.
  * They are not the XP FE83 vertex path. Keep both preliminary controls
  * inline: independently measure a fetched 2D fill and a changing fence.
  * Header0 counts address/value pairs, not DWORDs. No branch is armed. */
 p[n++]=0xFE000000UL; p[n++]=which?12:9;
 p[n++]=0; p[n++]=0xCC000000UL;
 p[n++]=0x04; p[n++]=0x300;
 p[n++]=0x14; p[n++]=VX3D_TARGET>>3;
 p[n++]=0x08; p[n++]=(VX3D_SIDE*4/8)<<16;
 p[n++]=0x10; p[n++]=0x000C000CUL;
 p[n++]=0x0C; p[n++]=0x000F000FUL;
 p[n++]=0x48; p[n++]=0;
 p[n++]=0x58; p[n++]=r->sf_2d_color[which];
 p[n++]=0x00; p[n++]=0xF0002001UL;
 p[n++]=0x6C; p[n++]=0x00400000UL;
 if(which) {
  p[n++]=0x43C; p[n++]=0x00110000UL;
  p[n++]=0x440; p[n++]=0x06000000UL|(id&0xFFFFFFUL);
  p[n++]=0x440; p[n++]=0x07100000UL|((id>>24)<<8);
 }
 while(n&3) p[n++]=0xCC000000UL;
 if(!which) {
  p[n++]=0xFE020420UL; p[n++]=0x41C;
  p[n++]=0x00110000UL; p[n++]=0xCC000000UL;
  p[n++]=0x06000000UL|(id&0xFFFFFFUL);
  p[n++]=0x07100000UL|((id>>24)<<8);
  while(n&3) p[n++]=0xCC000000UL;
 }
 p[n++]=0xFE020420UL; p[n++]=0x41C;
 p[n++]=0x00300000UL; p[n++]=0xCC000000UL;
 for(i=0;n<64;i++) p[n++]=(i&1)?0xDDD00002UL:0xCCC00002UL;
 if(!which) r->dma_count[0]=n;
 r->sf_transfer_flags|=which?4:2;
 return n==64;
}
static int sf_observe_control(const vx3d_io *io,vx3d_report *r,uint32_t which) {
 uint32_t x,y,want,got;
 r->sf_2d_observed[which]=rd(io,0x58);
 for(y=0;y<VX3D_SIDE;y++) for(x=0;x<VX3D_SIDE;x++) {
  got=pixel(io,x,y);
  want=(x>=12 && x<28 && y>=12 && y<28)?r->sf_2d_color[which]:VX3D_BACKGROUND;
  if(got!=want) r->sf_2d_errors[which]++;
  if(got!=VX3D_BACKGROUND) r->sf_2d_changed[which]++;
 }
 r->sf_transfer_flags|=which?16:8;
 if(r->sf_2d_observed[which]==r->sf_2d_color[which] &&
    r->sf_2d_before[which]!=r->sf_2d_color[which]) r->sf_transfer_flags|=which?64:32;
 if(!r->sf_2d_errors[which] && r->sf_2d_changed[which]==256) {
  r->sf_transfer_flags|=which?256:128; return 1;
 }
 return 0;
}
static int sf_build_inline_vertices(const vx3d_io *io,vx3d_report *r,uint32_t retry) {
 dma_builder b; uint32_t i;
 uint32_t fence=retry?r->sf_branch_expected:r->sf_expected[3];
 uint32_t offset=retry?SF_BRANCH_DRAW:SF_DRAW;
 b.real=io; b.report=r; b.words=retry?r->branch_control:r->dma_program[1];
 b.count=0; b.error=0;
 /* Both OFF/ON draws inherit the same complete CPU-programmed state.
  * Only fence and trailing latch IDs differ; state and vertices match. */
 dma_header(&b,0x43C,0x00040000UL);
 dma_word(&b,0xAA000000UL|r->sf_state_marker[2]);
 dma_wait(&b,0x00400000UL);
 r->vertex_words=5; r->vertex_vertices=3;
 dma_header(&b,0x43C,0);
 for(i=0;i<15;i++) dma_word(&b,fixed_vertices[i]);
 dma_wait(&b,0x00800000UL);
 /* A trailing 2D-register latch proves that the reader dispatched a
  * command after the vertex block. It never writes the render target. */
 dma_align(&b); dma_word(&b,0xFE000000UL); dma_word(&b,1);
 dma_word(&b,0); dma_word(&b,0xCC000000UL);
 dma_word(&b,0x58); dma_word(&b,r->sf_draw_marker_expected+retry);
 dma_header(&b,0x43C,0x00110000UL);
 dma_word(&b,0x06000000UL|(fence&0xFFFFFFUL));
 dma_word(&b,0x07100000UL|((fence>>24)<<8));
 sf_branch_pad(&b);
 if(b.error || offset+b.count*4+0x200>=(retry?SF_ALT_DRAW:SF_BRANCH_DRAW)) return 0;
 if(retry) {
  r->branch_control_count=b.count; r->sf_branch_flags|=1;
  r->sf_branch_pause=SF_RING_BASE+offset+b.count*4-16;
  r->sf_queue_compare_flags|=1;
 } else r->dma_count[1]=b.count;
 return 1;
}
static int sf_build_point(const vx3d_io *io,vx3d_report *r,uint32_t which) {
 dma_builder b; uint32_t offset=which?SF_POINT_ON:SF_POINT_OFF;
 b.real=io; b.report=r; b.words=r->sf_point_program[which]; b.count=0; b.error=0;
 dma_wait(&b,0x00400000UL); dma_align(&b);
 dma_word(&b,0xFE03043CUL); dma_word(&b,5);
 dma_word(&b,0); dma_word(&b,0xCC000000UL);
 /* One point centered at (20,20), default width/height 16 pixels.
  * The same x,y,z,w,ARGB layout and target format/pitch are retained. */
 dma_word(&b,0x41A00000UL); dma_word(&b,0x41A00000UL);
 dma_word(&b,0x3F000000UL); dma_word(&b,0x3F800000UL);
 dma_word(&b,0xFFE04020UL); dma_wait(&b,0x00800000UL);
 dma_align(&b); dma_word(&b,0xFE000000UL); dma_word(&b,1);
 dma_word(&b,0); dma_word(&b,0xCC000000UL);
 dma_word(&b,0x58); dma_word(&b,r->sf_draw_marker_expected+2+which);
 sf_branch_pad(&b);
 if(b.error || b.count!=64) return 0;
 r->sf_point_count[which]=b.count; r->sf_point_flags[which]|=1;
 r->sf_point_pause[which]=SF_RING_BASE+offset+b.count*4-16;
 return 1;
}
/* Path 0 inline, 1 CPU repeated port, 2 CPU sequential ports, 3 indexed.
 * Geometry 0 original triangle, 1 normalized viewport, 2 skew, 3 square. */
static const uint32_t sf_tri[6][6]={
  {0x40800000UL,0x41400000UL,0x42700000UL,0x41A00000UL,0x41A00000UL,0x42700000UL},
  {0xC1000000UL,0x41200000UL,0x42200000UL,0x41A00000UL,0x41A00000UL,0x42700000UL},
  {0x41200000UL,0xC1000000UL,0x42700000UL,0x41A00000UL,0x41A00000UL,0x42700000UL},
  {0x41200000UL,0x41200000UL,0x42900000UL,0x41C00000UL,0x41A00000UL,0x42600000UL},
  {0xC1000000UL,0x41200000UL,0x42200000UL,0x41200000UL,0xC1000000UL,0x42200000UL},
  {0x41000000UL,0x41200000UL,0x42600000UL,0x41A00000UL,0x41F00000UL,0x42900000UL}};
static uint32_t sf_case_vertex(const vx3d_case_report *c,uint32_t v,uint32_t a) {
 static const uint32_t xy[8]={0x41000000UL,0x41000000UL,0x42600000UL,0x41000000UL,
  0x41000000UL,0x42600000UL,0x42600000UL,0x42600000UL};
 static const uint32_t skew[6]={0x41400000UL,0x41200000UL,0x42580000UL,0x41A00000UL,
  0x41A00000UL,0x42580000UL};
 if((c->config[9]&2) && v && v<3) v=3-v;
 if((c->config[9]&4) && v>=2) v=5-v;
 if(a<2) {
  if((c->mode&15)==0 && c->vertices==1) return 0x41A00000UL;
  if(c->config[4]>=16 && c->config[4]<22) return sf_tri[c->config[4]-16][v*2+a];
  if((c->mode&15)==4) {
   /* Rectangle. v6.14 (two repeated diagonal pairs, four vertices) and
    * v6.15 (one diagonal pair, two vertices) both stalled the engine.
    * Three vertices use the triangle positions TL,TR,BL (8,8),(56,8),
    * (8,56): the documented vertex cycles are 3 for triangles and 2 for
    * lines, and none is documented for rectangles. Two vertices stay a
    * diagonal pair; four are the four distinct corners. */
   if(c->vertices==2) return v?0x42600000UL:0x41000000UL;
   return xy[v*2+a];
  }
  if(c->config[4]==4) {
   static const uint32_t clip[6]={0xC1000000UL,0x41200000UL,0x428C0000UL,0x41A00000UL,0x41A00000UL,0x428C0000UL};
   return clip[v*2+a];
  }
  if(c->config[4]==6) {
   static const uint32_t r5[6]={0x41800000UL,0x42400000UL,0x42400000UL,0x42400000UL,0x41800000UL,0x41800000UL};
   return r5[v*2+a];
  }
  if(c->config[4]==1) return (a==0?v==1:v==2)?0x3F000000UL:0xBF000000UL;
  if(c->config[4]==2) return skew[v*2+a];
  return xy[v*2+a];
 }
 if(a==4 && c->config[4]==5) return v==0?0xFFFF0000UL:v==1?0xFF00FF00UL:0xFF0000FFUL;
 return a==2?0x3F000000UL:a==3?0x3F800000UL:a==4?0xFFE04020UL:0;
}
/* v9.1 rasterizer variants (bit mask per case; control+triangle pairs share one):
 * 1 Z test+write on, 2 guard band/color+scissor windows at XP/full range,
 * 4 RT0 setting 4 = XP 010C00, 16 texture-0 state zeroed (type 02 subtype 0),
 * 32 pulse the two stuck cache-clear strobes (01h/29h bit 15, FE02/00 bit 0),
 * 8 Enable Setting 2 = XP CE1540 (caches, perspective correction). */
/* v9.10: XP-captured native triangle (XPRING 2.2). Versus XP's own point draw the
 * triangle differs in only: 04/23 = 00CE02 (point 001E00) and a 786432.0 bias on
 * X and Y offsets (04/41=400000, 04/42=00493F, 04/44=400000, 04/45=00493F).
 * 64 = that bias plus XP clamp 04/52=147FFF, 128 = mode word 00CE02,
 * 256 = other captured state (04/00 A80005, 04/28 FD0000, 04/2F 582000, 04/48 3F,
 *       01/01 CE1540, 01/53 010C0F, 01/1E 1F 20 82 83, shader 9B and word 00F04005),
 * 512 = captured FE context (840F, 46=5, 47=404) and FE02/00 = 1,
 * 1024 = keep the old 04/52 clamp with the bias (tests the clamp warning). */
static uint32_t sf_variant_mask[32]={0,64|128,0,64|128,0,64|128,64,64|128,0,64|128|256,0,64|128,64,64|128,0, 0,
64|128,64|128,64|128,64|128,64|128,64|128,64|128,64|128,64|128,64|128,0,0,64|128,64|128,64|128,0};
/* v9.7 flat-fill routine, cases 16-25 (cases 0-15 unchanged): a CPU edge walk
 * (vx855_fill.h) emits one type-0 point per run of identical rows, 04/2A =
 * width and 04/2B = height, through the CPU register path while the ring is
 * paused. 26 and 27 are point controls. */
static const uint32_t sf_en2_extra[32]={0};
static const uint32_t sf_rev[32]={0,0,0,1};
static const uint32_t sf_pw[32]={
 0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,
 0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000};
static const uint32_t sf_ph[32]={
 0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,
 0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000,0x418000};
/* Non-negative half-integer value twice/2 as IEEE-754 bits (no FPU). */
static uint32_t sf_half_bits(uint32_t twice) {
 uint32_t p=0;
 if(!twice) return 0;
 while((twice>>(p+1))!=0) p++;
 return ((p+126UL)<<23)|((twice<<(23-p))&0x7FFFFFUL);
}
static uint32_t sf_scaled_bits(uint32_t n,uint32_t k) { /* float bits of n/2^k */
 uint32_t p=0;
 if(!n) return 0;
 while((n>>(p+1))!=0) p++;
 return ((p+127UL-k)<<23)|((n<<(23-p))&0x7FFFFFUL);
}
/* Fill jobs. Cases 16-25 fill the 64x64 scratch target; cases 28-30 fill the
 * live desktop framebuffer (640x480x32, pitch 2560, offset 0) inside a small
 * window that is saved first, verified, and restored. kind 1 triangle,
 * 2 rectangle; geometry g[0..5] (triangle x0 y0 x1 y1 x2 y2, rectangle x0 y0
 * x1 y1); *color indexes sf_fill_float/sf_fill_pix; win = {x,y,w,h} of the
 * desktop window to save/verify/restore, w==0 for the scratch target. */
#define SF_DESK_W 640
#define SF_DESK_H 480
#define SF_DESK_PITCH 2560
#define SF_DESK_MAX 11520UL
static const uint32_t sf_fill_float[4][4]={
 {0,0x3F800000UL,0x3F800000UL,0x3F800000UL},          /* cyan  R,G,B,A */
 {0x3F800000UL,0,0,0x3F800000UL},                      /* red           */
 {0,0x3F800000UL,0,0x3F800000UL},                      /* green         */
 {0,0,0x3F800000UL,0x3F800000UL}};                     /* blue          */
static const uint32_t sf_fill_pix[4]={0xFF00FFFFUL,0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL};
static uint32_t sf_desk_save[SF_DESK_MAX];
/* v9.31: native 3D pixel-pipeline proofs on a prefilled desktop window.
 * Source colour is (0.5,0.25,0.75) with the test's alpha. Alpha test
 * reference scaling and compare modes, plus four more blend equations. */
typedef struct sf_3d_t {uint32_t alpha,es1,es3,a33,b34,b35,b36,b37,colour_off,model,aq;} sf_3d_t;
#define SF2I(p) ((p)==31?10:(p)==27?11:(p)-16)
#define SF3_NONE 1
#define SF3_SOLID 2
#define SF3_SA_ISA 3
#define SF3_ADD11 4
#define SF3_ONE_ISA 5
#define SF3_SA_ONE 6
#define SF3_ZERO_SC 7
#define SF3_UNKNOWN 8
#define AT(mode,ref) (((uint32_t)(mode)<<12)|(ref))
static const sf_3d_t sf_3d_tests[10]={
 {0x3E800000UL,0x10000,0,AT(7,0x200),0,0,0,0,0,SF3_SOLID,64},    /* 16 alpha test ALWAYS (control), a=0.25: solid */
 {0x3F400000UL,0x10000,0,AT(4,0x200),0,0,0,0,0,SF3_SOLID,192},   /* 17 GREATER ref 0x200, a=0.75: solid */
 {0x3F000000UL,0x10000,0,AT(4,0x300),0,0,0,0,0,SF3_UNKNOWN,128}, /* 18 GREATER ref 0x300, a=0.5: scaling probe (nothing if 1.0=0x400) */
 {0x3E800000UL,0x10000,0,AT(1,0x200),0,0,0,0,0,SF3_SOLID,64},    /* 19 LESS ref 0x200, a=0.25: solid */
 {0x3F400000UL,0x10000,0,AT(0,0x200),0,0,0,0,0,SF3_NONE,192},    /* 20 alpha test NEVER (control), a=0.75: nothing */
 {0x3F400000UL,0x10000,0,AT(5,0x200),0,0,0,0,0,SF3_SOLID,192},   /* 21 NOTEQUAL ref 0x200, a=0.75: solid */
 {0x3F800000UL,0x10,0,0,0x150,0x154,0,0,0,SF3_ADD11,256},        /* 22 blend ONE/ONE */
 {0x3F000000UL,0x10,0,0,0x150,0x148,0,0,0,SF3_ONE_ISA,128},      /* 23 blend ONE/INVSRCALPHA a=0.5 */
 {0x3F000000UL,0x10,0,0,0x020,0x154,0,0,0,SF3_SA_ONE,128},       /* 24 blend SRCALPHA/ONE a=0.5 */
 {0x3F800000UL,0x10,0,0,0x050,0x100,0,0,0,SF3_ZERO_SC,256}};     /* 25 blend ZERO/SRCCOLOR (dest*src) */
static uint32_t sf_3d_expect(const sf_3d_t *t,uint32_t d) {
 uint32_t chn,out=0,aq=t->aq;
 if(t->model==SF3_NONE || t->model==SF3_UNKNOWN) return d;
 for(chn=0;chn<3;chn++) {
  uint32_t sh=chn*8,dc=(d>>sh)&0xFF,sc=(chn==2)?128:(chn==1)?64:191,v;
  switch(t->model) {
  case SF3_SOLID: v=sc; break;
  case SF3_SA_ISA: v=(sc*aq+dc*(256-aq))>>8; break;
  case SF3_ADD11: v=sc+dc; break;
  case SF3_ONE_ISA: v=sc+((dc*(256-aq))>>8); break;
  case SF3_SA_ONE: v=((sc*aq)>>8)+dc; break;
  default: v=(dc*sc)>>8; break;
  }
  if(v>255) v=255;
  out|=v<<sh;
 }
 return out;
}
/* v9.33: depth test proofs. Two opaque rectangles, A then B, overlapping.
 * A=(456,328)-(488,360), B=(472,344)-(504,376). Z buffer: 32-bit fixed, pitch
 * 256 bytes, based so desktop pixel (448,320) maps to VX3D_DEPTH. */
typedef struct sf_z_t {uint32_t mode,clear,za,zb,zwrite;} sf_z_t;
static const sf_z_t sf_z_tests[10]={
 {1,0xFFFFFFFFUL,0x40000000UL,0xC0000000UL,1},  /* 16 LESS A=.25 B=.75: A wins overlap */
 {4,0x00000000UL,0x40000000UL,0xC0000000UL,1},  /* 17 GREATER clear 0, A=.25 B=.75: B wins */
 {3,0xFFFFFFFFUL,0x80000000UL,0x80000000UL,1},  /* 18 mode3 (LEQUAL) equal Z: B wins */
 {3,0xFFFFFFFFUL,0x40000000UL,0xC0000000UL,1},  /* 19 mode3, B>A: A wins */
 {5,0xFFFFFFFFUL,0x80000000UL,0x80000000UL,1},  /* 20 mode5 (NOTEQUAL) equal Z: A wins */
 {5,0xFFFFFFFFUL,0x40000000UL,0xC0000000UL,1},  /* 21 mode5, B>A: B wins */
 {6,0x00000000UL,0x80000000UL,0x80000000UL,1},  /* 22 mode6 (GEQUAL) equal Z: B wins */
 {6,0x00000000UL,0x40000000UL,0xC0000000UL,1},  /* 23 mode6, B>A: B wins */
 {0,0xFFFFFFFFUL,0x40000000UL,0xC0000000UL,1},  /* 24 NEVER: nothing drawn */
 {1,0xFFFFFFFFUL,0x40000000UL,0xC0000000UL,0}}; /* 25 LESS, Z write off: B wins */
/* v9.53 (variant 15): the depth modes the first suite did not cover (EQUAL, ALWAYS) and strict-vs-equal edge cases. */
static const sf_z_t sf_z_tests2[10]={
 {2,0x80000000UL,0x80000000UL,0x80000000UL,1},  /* 16 EQUAL clear=.5, A=B=.5: both pass, B wins */
 {2,0x80000000UL,0x80000000UL,0xC0000000UL,1},  /* 17 EQUAL clear=.5, A=.5 B=.75: A wins, B rejected */
 {2,0xFFFFFFFFUL,0x40000000UL,0xC0000000UL,1},  /* 18 EQUAL clear=1: nothing drawn */
 {7,0xFFFFFFFFUL,0x40000000UL,0xC0000000UL,1},  /* 19 ALWAYS: B wins */
 {7,0xFFFFFFFFUL,0xC0000000UL,0x40000000UL,1},  /* 20 ALWAYS, B nearer: B wins */
 {7,0xFFFFFFFFUL,0x40000000UL,0xC0000000UL,0},  /* 21 ALWAYS, Z write off: B wins, Z stays clear */
 {7,0x00000000UL,0x40000000UL,0xC0000000UL,1},  /* 22 ALWAYS clear=0: B wins */
 {1,0xFFFFFFFFUL,0x80000000UL,0x80000000UL,1},  /* 23 LESS equal Z: A wins (strict) */
 {4,0x00000000UL,0x80000000UL,0x80000000UL,1},  /* 24 GREATER equal Z: A wins (strict) */
 {3,0xFFFFFFFFUL,0x80000000UL,0x80000000UL,0}}; /* 25 LEQUAL equal Z, Z write off: B wins */
#define SF_ZT(i) (vx3d_x_select==15?&sf_z_tests2[(i)]:&sf_z_tests[(i)])
/* v9.54 (variant 17): stencil proofs. Z buffer format 110 (24-bit Z in 31:8, stencil in 7:0), Z test LESS.
 * Two rectangle passes (A then B, as the depth suite), each with its own stencil state. Pass fields:
 * {test mode, ref, op mask, write mask, fail op, zfail op, pass op, vertex Z (32-bit fixed)}. Both faces get identical state. */
typedef struct sf_st_t {uint32_t s0; uint32_t p[2][8];} sf_st_t;
#define ZQ 0x40000000UL
#define ZH 0x80000000UL
#define ZT 0xC0000000UL
static const sf_st_t sf_st_tests[10]={
 {0x00,{{7,0x7F,0xFF,0xFF,0,0,2,ZH},{0,0,0xFF,0xFF,0,0,0,ZQ}}},     /* 16 ALWAYS, REPLACE ref 7F: A stencil 7F                    */
 {0x00,{{7,0x55,0xFF,0xFF,0,0,2,ZH},{2,0x55,0xFF,0xFF,0,0,6,ZQ}}},  /* 17 B EQUAL 55: only overlap passes, INCR -> 56            */
 {0xFF,{{7,0,0xFF,0xFF,0,0,3,ZH},{7,0,0xFF,0xFF,0,0,6,ZQ}}},        /* 18 INCRSAT keeps FF; INCR wraps FF->00                    */
 {0x00,{{7,0,0xFF,0xFF,0,0,4,ZH},{7,0,0xFF,0xFF,0,0,7,ZQ}}},        /* 19 DECRSAT keeps 00; DECR wraps 00->FF                    */
 {0x0F,{{7,0,0xFF,0xFF,0,0,5,ZH},{0,0,0xFF,0xFF,0,0,0,ZQ}}},        /* 20 INVERT 0F -> F0                                        */
 {0x3C,{{7,0,0xFF,0xFF,0,0,1,ZH},{4,1,0xFF,0xFF,0,0,2,ZQ}}},        /* 21 ZERO; B GREATER(ref>s) ref 1: only overlap passes      */
 {0x00,{{0,0x33,0xFF,0xFF,2,0,0,ZH},{0,0,0xFF,0xFF,0,0,0,ZQ}}},     /* 22 NEVER with fail-op REPLACE 33: A not drawn, s=33       */
 {0x00,{{7,0x10,0xFF,0xFF,0,0,2,ZH},{7,0x20,0xFF,0xFF,0,2,5,ZT}}},  /* 23 Z-fail op REPLACE in overlap, INVERT elsewhere         */
 {0xF0,{{2,0x00,0x0F,0xFF,0,0,5,ZH},{0,0,0xFF,0xFF,0,0,0,ZQ}}},     /* 24 op mask 0F: (0&0F)==(F0&0F) passes, INVERT -> 0F       */
 {0x00,{{7,0xFF,0xFF,0x0F,0,0,2,ZH},{0,0,0xFF,0xFF,0,0,0,ZQ}}}};    /* 25 write mask 0F: REPLACE FF writes only low nibble       */
static int sf_st_cmp(uint32_t m,uint32_t a,uint32_t b) { /* a = ref&opmask, b = stencil&opmask; pass if a <m> b */
 switch(m) {case 0: return 0; case 1: return a<b; case 2: return a==b; case 3: return a<=b; case 4: return a>b;
  case 5: return a!=b; case 6: return a>=b; default: return 1;}
}
static uint32_t sf_st_op(uint32_t op,uint32_t s,uint32_t ref) {
 switch(op) {case 0: return s; case 1: return 0; case 2: return ref; case 3: return s<255?s+1:255; case 4: return s>0?s-1:0;
  case 5: return (~s)&0xFF; case 6: return (s+1)&0xFF; default: return (s-1)&0xFF;}
}
static uint32_t sf_st_model(const sf_st_t *t,int32_t sx,int32_t sy,uint32_t orig,uint32_t *zout) {
 uint32_t z=0xFFFFFF00UL,s=t->s0,col=orig,pi;
 for(pi=0;pi<2;pi++) {
  static const int32_t rc[2][4]={{456,328,488,360},{472,344,504,376}};
  const uint32_t *q=t->p[pi];
  if(sx>=rc[pi][0] && sx<rc[pi][2] && sy>=rc[pi][1] && sy<rc[pi][3]) {
   uint32_t stp=sf_st_cmp(q[0],q[1]&q[2],s&q[2]),zp=(q[7]&0xFFFFFF00UL)<(z&0xFFFFFF00UL),ns;
   if(!stp) ns=sf_st_op(q[4],s,q[1]);
   else if(!zp) ns=sf_st_op(q[5],s,q[1]);
   else ns=sf_st_op(q[6],s,q[1]);
   s=(s&~q[3])|(ns&q[3]);
   if(stp&&zp) {col=pi?0x0000FFUL:0xFF0000UL; z=q[7]&0xFFFFFF00UL;}
  }
 }
 if(zout) *zout=z|s;
 return col;
}
/* v9.53 (variant 16): Gouraud triangle (8,8) red, (56,8) green, (8,56) blue into RT0 (RT-relative coords). */
typedef struct sf_gr_t {uint32_t fmt,mode53,rev;} sf_gr_t;
static const sf_gr_t sf_gr_tests[10]={
 {0x99,0x000C0FUL,0},{0x89,0x010C0FUL,0},{0x89,0x000C0FUL,0},{0x8B,0x010C0FUL,0},{0x8A,0x010C0FUL,0},
 {0x99,0x000C0FUL,1},{0x89,0x010C0FUL,1},{0x8B,0x000C0FUL,0},{0x8A,0x000C0FUL,0},{0x88,0x010C0FUL,0}};
/* v9.55 (variant 18): fog measurement. Triangle (8,8) z=.25, (56,8) z=.75, (8,56) z=.5 into the 32bpp desktop target, constant shader colour
 * C, fog colour F; HCM 01/00 bit 8 enables fog for RT0; 01/70 mode (010 linear, bit4 Z/W, bit3 equation), 01/71..72 fog colour (11-bit fixed, 1.0=0x400),
 * 01/73 start and 01/77 end as 15-bit floats (IEEE bits 30:16), 01/75-76 mantissa/exponent of 1/(end-start). */
typedef struct sf_fog_t {uint32_t mode70,en,start_m,end_m,c[3],f[3],report_only;} sf_fog_t; /* colours in 1/1000 */
static const sf_fog_t sf_fog_tests[10]={
 {0x12,1,250,750,{1000,0,0},{0,0,1000},0},   /* 16 linear Z, eq0, start .25 end .75, red over blue fog   */
 {0x1A,1,250,750,{1000,0,0},{0,0,1000},0},   /* 17 same, equation 1                                       */
 {0x12,1,0,1000,{1000,0,0},{0,0,1000},0},    /* 18 start 0 end 1                                          */
 {0x12,1,500,1000,{1000,0,0},{0,0,1000},0},  /* 19 start .5 end 1 (f clamps to 1 for z<.5)                */
 {0x12,0,250,750,{1000,0,0},{0,0,1000},0},   /* 20 fog disabled (control): plain red                      */
 {0x00,1,250,750,{1000,0,0},{0,0,1000},1},   /* 21 local fog mode, no fog attribute (exploratory)         */
 {0x02,1,250,750,{1000,0,0},{0,0,1000},1},   /* 22 linear from W (W=1) (exploratory)                      */
 {0x12,1,250,750,{1000,0,0},{0,1000,0},0},   /* 23 green fog colour                                       */
 {0x12,1,0,500,{0,0,0},{1000,500,250},0},    /* 24 black geometry, fog (1,.5,.25), z>=.5 fully fogged: colour-field mapping probe */
 {0x12,1,0,500,{1000,0,0},{0,0,1000},0}};    /* 25 start 0 end .5: clamp at z>.5                          */
static uint32_t sf_fog_f(uint32_t m1000) { /* 0..1000 -> float bits */
 return m1000==0?0UL:m1000==1000?0x3F800000UL:m1000==500?0x3F000000UL:m1000==250?0x3E800000UL:m1000==750?0x3F400000UL:0x3F000000UL;
}
static uint32_t sf_fog_h(uint32_t m1000) { /* 0..1000 -> 15-bit float (IEEE bits 30:16) */
 return sf_fog_f(m1000)>>16;
}
static int sf_z_pass(uint32_t mode,uint32_t zn,uint32_t zd) {
 switch(mode) {
 case 0: return 0; case 1: return zn<zd; case 2: return zn==zd; case 3: return zn<=zd;
 case 4: return zn>zd; case 5: return zn!=zd; case 6: return zn>=zd; default: return 1;
 }
}
/* Model: colour and final Z at a window pixel. */
static uint32_t sf_z_model(const sf_z_t *t,int32_t sx,int32_t sy,uint32_t orig,uint32_t *zout) {
 uint32_t z=t->clear,col=orig;
 if(sx>=456 && sx<488 && sy>=328 && sy<360 && sf_z_pass(t->mode,t->za,z)) {col=0xFF0000UL; if(t->zwrite) z=t->za;}
 if(sx>=472 && sx<504 && sy>=344 && sy<376 && sf_z_pass(t->mode,t->zb,z)) {col=0x0000FFUL; if(t->zwrite) z=t->zb;}
 if(zout) *zout=z;
 return col;
}
/* v9.34: texture sampling proofs. A 64x64 texture at VX3D_DEPTH is sampled by
 * the pixel shader (TAU word 001101B0 + MOV from temp 1) with the texture
 * coordinate replaced by the screen position: s=x/2^W, t=y/2^H. */
typedef struct sf_tx_t {uint32_t fmt,filter,wexp,hexp,addr,blend;} sf_tx_t;
#define TXF_ARGB 0
#define TXF_L8 1
#define TXF_ABGR 2
#define TXF_BGRA 3
#define TXF_ARGBA 4
static sf_tx_t sf_tx_tests[10]={
 {TXF_ARGB,0,6,6,2,0},   /* 16 nearest, 1:1, repeat */
 {TXF_ARGB,0,7,7,2,0},   /* 17 nearest, 2x magnified, repeat */
 {TXF_ARGB,1,6,6,2,0},   /* 18 bilinear 1:1 */
 {TXF_ARGB,1,7,7,2,0},   /* 19 bilinear 2x magnified */
 {TXF_ARGB,0,4,4,1,0},   /* 20 nearest, clamp (far outside 0..1) */
 {TXF_ARGB,0,6,6,3,0},   /* 21 nearest, mirror */
 {TXF_L8,0,6,6,2,0},     /* 22 L8 format */
 {TXF_ABGR,0,6,6,2,0},   /* 23 ABGR 32bpp format */
 {TXF_BGRA,0,6,6,2,0},   /* 24 BGRA 32bpp format */
 {TXF_ARGBA,0,6,6,2,1}}; /* 25 texture alpha + SRCALPHA/INVSRCALPHA blend */
static uint32_t sf_tx_uniform=1; /* v9.35: uniform texture until the draw completes reliably */
static uint32_t sf_tx_texel(uint32_t fmt,uint32_t u,uint32_t v) {
 uint32_t r=(u*4)&0xFF,g=(v*4)&0xFF,b=0x80,a=0xFF,l;
 if(sf_tx_uniform && fmt!=TXF_L8) return fmt==TXF_ARGBA?0x80336699UL:0xFF336699UL;
 if(fmt==TXF_ARGBA) a=(u*4+3)&0xFF;
 if(fmt==TXF_L8) {l=(u*3+v*2)&0xFF; return 0xFF000000UL|(l<<16)|(l<<8)|l;}
 return (a<<24)|(r<<16)|(g<<8)|b;
}
static uint32_t sf_tx_word(uint32_t fmt,uint32_t wi) {
 uint32_t u,v,r,g,b,a,c,k,w;
 if(fmt==TXF_L8) {
  u=(wi%16)*4; v=wi/16; w=0;
  for(k=0;k<4;k++) w|=(((u+k)*3+v*2)&0xFF)<<(k*8);
  return w;
 }
 u=wi%64; v=wi/64; c=sf_tx_texel(fmt==TXF_ARGBA?TXF_ARGBA:TXF_ARGB,u,v);
 a=c>>24;r=(c>>16)&0xFF;g=(c>>8)&0xFF;b=c&0xFF;
 if(fmt==TXF_ABGR) return (a<<24)|(b<<16)|(g<<8)|r;
 if(fmt==TXF_BGRA) return (b<<24)|(g<<16)|(r<<8)|a;
 return c;
}
static int32_t sf_tx_addr(int32_t i,uint32_t mode) {
 if(mode==1) return i<0?0:i>63?63:i;
 if(mode==3) {i&=127; return i<64?i:127-i;}
 return i&63;
}
static uint32_t sf_tx_model(const sf_tx_t *t,int32_t sx,int32_t sy,int32_t ax,int32_t ay) {
 int32_t uq=((4*sx+ax)*64)>>t->wexp,vq=((4*sy+ay)*64)>>t->hexp;
 if(t->filter==0) {
  return sf_tx_texel(t->fmt,(uint32_t)sf_tx_addr(uq>>2,t->addr),(uint32_t)sf_tx_addr(vq>>2,t->addr));
 } else {
  int32_t pu=uq-2,pv=vq-2,i0=pu>>2,j0=pv>>2,fu=pu&3,fv=pv&3,k; uint32_t out=0; /* v10.330 (review #179): unsigned: v << 24 with v >= 128 overflowed an int32_t */
  uint32_t c[4],sh;
  c[0]=sf_tx_texel(t->fmt,(uint32_t)sf_tx_addr(i0,t->addr),(uint32_t)sf_tx_addr(j0,t->addr));
  c[1]=sf_tx_texel(t->fmt,(uint32_t)sf_tx_addr(i0+1,t->addr),(uint32_t)sf_tx_addr(j0,t->addr));
  c[2]=sf_tx_texel(t->fmt,(uint32_t)sf_tx_addr(i0,t->addr),(uint32_t)sf_tx_addr(j0+1,t->addr));
  c[3]=sf_tx_texel(t->fmt,(uint32_t)sf_tx_addr(i0+1,t->addr),(uint32_t)sf_tx_addr(j0+1,t->addr));
  for(k=0;k<4;k++) {
   int32_t top,bot,v;
   sh=(uint32_t)(k*8);
   top=(int32_t)((c[0]>>sh)&0xFF)*(4-fu)+(int32_t)((c[1]>>sh)&0xFF)*fu;
   bot=(int32_t)((c[2]>>sh)&0xFF)*(4-fu)+(int32_t)((c[3]>>sh)&0xFF)*fu;
   v=(top*(4-fv)+bot*fv+8)/16;
   out|=(uint32_t)v<<sh;
  }
  return out;
 }
}
/* v9.81: rotated texture sampling. Selectors 24/25/26 = rotate the 64x64 window at (448,320) by 90/180/270 degrees clockwise
 * by giving the rectangle rotated texture coordinates (affine map of the screen position), sampled from the video-memory texture. */
static int sf_rotsel(void) {return (vx3d_x_select>=24 && vx3d_x_select<=26)?(int)(vx3d_x_select-23):0;}
static void sf_rot_map(int rot,int32_t x,int32_t y,int32_t *u,int32_t *v) {
 int32_t dx=x-448,dy=y-320;
 if(rot==1) {*u=448+dy; *v=320+63-dx;}
 else if(rot==2) {*u=448+63-dx; *v=320+63-dy;}
 else {*u=448+63-dy; *v=320+dx;}
}
/* Apply the destination blend model (SRCALPHA/INVSRCALPHA) to the sampled texel. */
static uint32_t sf_tx_final(const sf_tx_t *t,uint32_t tex,uint32_t dst) {
 uint32_t a,k,sh,out=0xFF000000UL,v;
 if(!t->blend) return tex;
 a=tex>>24;
 for(k=0;k<3;k++) {
  sh=k*8;
  v=((((tex>>sh)&0xFF)*a)+(((dst>>sh)&0xFF)*(255-a))+127)/255;
  out|=v<<sh;
 }
 return out;
}
static uint32_t sf_tx_modelr(const sf_tx_t *t,int32_t sx,int32_t sy,int32_t ax,int32_t ay) {
 int rot=sf_rotsel(); int32_t u,v;
 if(rot) {sf_rot_map(rot,sx,sy,&u,&v); return sf_tx_model(t,u,v,ax,ay);}
 return sf_tx_model(t,sx,sy,ax,ay);
}
static void sf_state_dump(const vx3d_io *io,vx3d_report *r);
static uint32_t sf_tx_diff(uint32_t a,uint32_t b) {
 uint32_t k,m=0,x,y;
 for(k=0;k<3;k++) {x=(a>>(k*8))&0xFF;y=(b>>(k*8))&0xFF;x=x>y?x-y:y-x;if(x>m)m=x;}
 return m;
}
/* v9.50: 16bpp render-target proofs (variant 10 reuses case slots 16..25).
 * RT0 is based at desktop pixel (448,320) with the desktop pitch and a 16bpp
 * format code (01/52 bits 23:16 = 10001xxx). A rectangle (8,8)-(56,56) in RT
 * coordinates covers 16bpp bytes 16..111 of each row, inside the left half of
 * the saved 64x64 32bpp window; the right half is a guard. */
typedef struct sf_rt_t {uint32_t fmt,blend,dst; uint32_t fb[4]; uint32_t m[4];} sf_rt_t;
#define F_0   0x00000000UL
#define F_02  0x3E4CCCCDUL
#define F_025 0x3E800000UL
#define F_05  0x3F000000UL
#define F_06  0x3F19999AUL
#define F_075 0x3F400000UL
#define F_09  0x3F666666UL
#define F_1   0x3F800000UL
static const sf_rt_t sf_rt_tests[10]={
 {0x89,0,0xA5A5,{F_05,F_025,F_075,F_1},{500,250,750,1000}},   /* 16 RGB565 flat (.5,.25,.75)           */
 {0x88,0,0xA5A5,{F_05,F_025,F_075,F_1},{500,250,750,1000}},   /* 17 RGB555 flat                        */
 {0x8A,0,0xA5A5,{F_05,F_025,F_075,F_1},{500,250,750,1000}},   /* 18 ARGB1555 flat, A=1                 */
 {0x8B,0,0xA5A5,{F_05,F_025,F_075,F_1},{500,250,750,1000}},   /* 19 ARGB4444 flat, A=1                 */
 {0x89,0,0xA5A5,{F_02,F_06,F_09,F_1},{200,600,900,1000}},     /* 20 RGB565 flat (.2,.6,.9)             */
 {0x89,0,0xA5A5,{F_05,F_05,F_05,F_1},{500,500,500,1000}},     /* 21 RGB565 mid grey (rounding probe)   */
 {0x89,1,0x07E0,{F_05,F_025,F_075,F_05},{500,250,750,500}},   /* 22 RGB565 SRCALPHA/INVSRCALPHA a=.5   */
 {0x8B,1,0xF0F0,{F_05,F_025,F_075,F_05},{500,250,750,500}},   /* 23 ARGB4444 blend a=.5                */
 {0x8A,0,0xA5A5,{F_05,F_025,F_075,F_0},{500,250,750,0}},      /* 24 ARGB1555 flat, A=0                 */
 {0x8D,0,0xA5A5,{F_05,F_025,F_075,F_1},{500,250,750,1000}}};  /* 25 format 0x8D: plain RGB565, no swap */
/* v10.11: 3D reach. Ten video-memory byte offsets (all 256-byte aligned, 4 MiB .. last 160 KB of the 512 MiB window) where the render target,
 * depth buffer or texture of a proven case is relocated. Case 16+k uses entry k. */
uint32_t vx3d_x_reach;
static uint32_t sf_hi; /* base of the relocated depth buffer / texture for the case being run, 0 = normal */
/* v10.330 (review #119): the 16 KB a reach case writes at its high offset (depth buffer or texture) is kept before it is written and put
 * back after the case's measure (only after a confirmed stop: see vx3d_run_sf) */
static uint32_t sf_hi_save[4096],sf_hi_at;
static void sf_hi_keep(const vx3d_io *io,uint32_t at) {uint32_t i; for(i=0;i<4096;i++) sf_hi_save[i]=io->read_fb(io->context,at+i*4); sf_hi_at=at;}
static void sf_hi_restore(const vx3d_io *io) {uint32_t i; if(!sf_hi_at) return; for(i=0;i<4096;i++) io->write_fb(io->context,sf_hi_at+i*4,sf_hi_save[i]); (void)io->read_fb(io->context,sf_hi_at+4092); sf_hi_at=0;}
static const uint32_t sf_reach_off[10]={0x00400000UL,0x00800000UL,0x01000000UL,0x02000000UL,0x04000000UL,0x08000000UL,0x10000000UL,0x18000000UL,0x1C000000UL,0x1FF00000UL};
#define SF_DEPTHB (sf_hi?sf_hi:VX3D_DEPTH)
static int sf_fill_job(uint32_t i,int32_t *g,uint32_t *color,int32_t *win) {
 win[0]=win[1]=win[2]=win[3]=0;
 if(i>=16 && i<=25 && (vx3d_x_select==19||vx3d_x_select==20||vx3d_x_select==22||vx3d_x_select==23)) return 0;
 if(i>=16 && i<=25 && vx3d_x_select==27) { /* v10.06: alpha test / blend suite re-enabled (kind 6) */
  g[0]=456;g[1]=328;g[2]=504;g[3]=376;g[4]=(int32_t)(i-16);g[5]=0;*color=1;
  win[0]=448;win[1]=320;win[2]=64;win[3]=64;
  return 6;
 }
 if(i>=16 && i<=25 && (vx3d_x_select==15||vx3d_x_select==17||vx3d_x_select==28)) {
  g[0]=g[1]=g[2]=g[3]=g[5]=0;g[4]=(int32_t)(i-16);*color=1;
  win[0]=448;win[1]=320;win[2]=64;win[3]=64;
  return 7;
 }
 if(i>=16 && i<=25 && vx3d_x_select==16 && vx3d_x_reach==1) { /* v10.11: Gouraud ARGB8888 triangle with the render target at a high offset */
  uint32_t off=sf_reach_off[i-16];
  g[0]=8;g[1]=8;g[2]=56;g[3]=8;g[4]=0;g[5]=0;*color=1;
  win[1]=(int32_t)(off/SF_DESK_PITCH); win[0]=(int32_t)((off%SF_DESK_PITCH)/4UL); win[2]=64; win[3]=64;
  return 10;
 }
 if(i>=16 && i<=25 && (vx3d_x_select==16||vx3d_x_select==18)) {
  const sf_gr_t *q=(vx3d_x_select==18)?&sf_gr_tests[0]:&sf_gr_tests[i-16];
  g[0]=8;g[1]=8;g[2]=q->rev?8:56;g[3]=q->rev?56:8;g[4]=(int32_t)(i-16);g[5]=0;*color=1;
  win[0]=448;win[1]=320;win[2]=64;win[3]=64;
  return 10;
 }
 if(i>=16 && i<=25 && vx3d_x_select>=10 && vx3d_x_select<=14) {
  g[0]=8;g[1]=8;g[2]=56;g[3]=56;g[4]=(int32_t)(i-16);g[5]=0;*color=1;
  win[0]=448;win[1]=320;win[2]=64;win[3]=64;
  return 9;
 }
 if(i>=16 && i<=25) {
  g[0]=448;g[1]=320;g[2]=512;g[3]=384;g[4]=(int32_t)(i-16);g[5]=0;*color=1;
  win[0]=448;win[1]=320;win[2]=64;win[3]=64;
  return 8;
 }
 if(i>=22 && i<=25) {
  /* v9.21: 2D engine proof on the desktop. Window (448,320) 64x64. */
  g[0]=(int32_t)SF2I(i);g[1]=g[2]=g[3]=g[4]=g[5]=0;*color=0;
  win[0]=448;win[1]=320;win[2]=64;win[3]=64;
  return 5;
 }
 if(i==28) {g[0]=520;g[1]=40;g[2]=600;g[3]=100;g[4]=g[5]=0;*color=1;
  win[0]=512;win[1]=32;win[2]=96;win[3]=76; return 4;}
 if(i==29) {g[0]=520;g[1]=120;g[2]=620;g[3]=180;g[4]=520;g[5]=200;*color=2;
  win[0]=512;win[1]=112;win[2]=120;win[3]=96; return 3;}
 if(i==30) {g[0]=587;g[1]=433;g[2]=633;g[3]=475;g[4]=g[5]=0;*color=3;
  win[0]=584;win[1]=432;win[2]=56;win[3]=48; return 4;}
 return 0;
}
typedef struct sf_fill_ctx {const vx3d_io *io; vx3d_report *r; uint32_t spans,pixels;} sf_fill_ctx;
static void sf_fill_emit(void *ctx,int32_t xs,int32_t xe,int32_t y,int32_t h) {
 sf_fill_ctx *f=(sf_fill_ctx*)ctx; const vx3d_io *io=f->io; vx3d_report *r=f->r;
 int32_t w=xe-xs;
 type(io,r,4);
 reg(io,r,0x2A,sf_half_bits((uint32_t)(2*w))>>8);
 reg(io,r,0x2B,sf_half_bits((uint32_t)(2*h))>>8);
 reg(io,r,0x24,1);
 type(io,r,0);
 wr(io,r,0x440,sf_half_bits((uint32_t)(2*xs+w)));
 wr(io,r,0x440,sf_half_bits((uint32_t)(2*y+h)));
 wr(io,r,0x440,0x3F000000UL); wr(io,r,0x440,0x3F800000UL); wr(io,r,0x440,0xFFE04020UL);
 f->spans++; f->pixels+=(uint32_t)(w*h);
 (void)idle(io,r);
}

/* ---- v9.22 2D engine proof, round two ---------------------------------
 * Round one (v9.21) proved: PATCOPY/BLACKNESS/WHITENESS/DSTINVERT/PATINVERT,
 * SRCCOPY no overlap, SRCCOPY overlap with decrement (far-corner start),
 * clip window (inclusive limits). The mono pattern read: rows 0..3 are
 * pat0 bytes MSB first, rows 4..7 pat1 bytes MSB first, bit 0 = leftmost,
 * origin = the operation's destination start. */
typedef struct sf_2d_op {
 uint32_t cmd,color,bg; int32_t dx,dy,w,h,sx,sy; uint32_t flags; uint32_t pat0,pat1;
 int32_t offx,offy,key;
} sf_2d_op;
#define OP_COPY 1
#define OP_MONO 8
#define OP_TRANS 16
#define OP_RAM 32
#define OP_LINE 64
#define OP_SKEY 128
#define OP_DKEY 256
#define OP_NOLAST 512
#define OP_FBPAT 1024
#define OP_MSRC 2048
#define OP_HOSTM 4096
#define OP_HOSTC 8192
#define OP_ROT 16384
#define OP_TEXT 32768
#define SF_2D_MROW 52
#define SF_2D_PATROW 60
#define SF_KEY 0xFF112233UL
uint32_t vx3d_x_select;
static const sf_2d_op sf_2d_ops[26]={
 {0xF0000A01UL,0xFFFF00FFUL,0xFF008000UL,4,4,40,24,0,0,OP_MONO,0x01020408UL,0x10204080UL,0,0,0},     /* 16 mono pattern, opaque */
 {0xF0010A01UL,0xFFFF00FFUL,0xFF008000UL,4,4,40,24,0,0,OP_MONO|OP_TRANS,0x01020408UL,0x10204080UL,0,0,0}, /* 17 mono pattern, transparent bg */
 {0xF0400801UL,0,0,5,6,40,24,0,0,OP_RAM,0,0,0,0,0},                                                   /* 18 colour pattern RAM, dest (5,6) */
 {0xF0000A01UL,0xFFFF00FFUL,0xFF008000UL,4,4,40,24,0,0,OP_MONO,0x01020408UL,0x10204080UL,3,2,0},       /* 19 mono pattern, offset X3 Y2 */
 {0xF0002005UL,0xFFFFFF00UL,0,4,6,0,0,50,20,OP_LINE,0xFFFFFFFFUL,0,0,0,0},                               /* 20 line shallow, style mask all ones */
 {0xF0002805UL,0xFFFFFF00UL,0,6,4,0,0,20,50,OP_LINE,0xFFFFFFFFUL,0,0,0,0},                               /* 21 line steep, +bit11 register pattern */
 {0xF0002A05UL,0xFFFFFF00UL,0,50,4,0,0,10,44,OP_LINE,0xFFFFFFFFUL,0xFFFFFFFFUL,0,0,0},                   /* 22 line X decrement, +bit11+bit9 mono */
 {0xF0102005UL,0xFFFFFF00UL,0,30,4,0,0,30,55,OP_LINE|OP_NOLAST,0xFFFFFFFFUL,0,0,0,0},                    /* 23 vertical, last pixel disabled (bit20) */
 {0xCC000001UL,0,0,32,36,24,16,0,0,OP_COPY|OP_SKEY,0,0,0,0,0},                                          /* 24 SRCCOPY with source colour key */
 {0xF0000001UL,0,0,4,4,40,24,0,0,OP_FBPAT,0,0,0,0,0},                                                  /* 25 colour pattern from frame buffer */
 {0xCC000001UL,0,0,4,4,24,16,32,36,OP_COPY|OP_DKEY,0,0,0,0,0},                                      /* 31 (LAST) SRCCOPY with destination colour key */
 {0xCC000101UL,0xFFFF8000UL,0xFF004080UL,4,4,32,8,0,0,OP_MSRC,0,0,0,0,0},                                /* x1 mono source expansion (bit 8) */
 {0xCC000141UL,0xFFFF8000UL,0xFF004080UL,4,4,32,8,0,0,OP_MSRC|OP_HOSTM,0,0,0,0,0},                      /* 12 host mono, port 0x100 constant */
 {0xCC0A0141UL,0xFFFF8000UL,0xFF004080UL,4,4,32,8,0,0,OP_MSRC|OP_HOSTM,0,0,0,0,0},                      /* 13 host mono, dword aligned */
 {0xCC000141UL,0xFFFF8000UL,0xFF004080UL,4,4,32,8,0,0,OP_MSRC|OP_HOSTM,0,0,0,0,1},                      /* 14 host mono, incrementing ports */
 {0xCC000041UL,0,0,4,4,32,8,0,0,OP_HOSTC,0,0,0,0,0},                                                    /* 15 host colour copy */
 {0xCC000541UL,0xFFFF8000UL,0xFF004080UL,4,4,32,8,0,0,OP_MSRC|OP_HOSTM|OP_TRANS,0,0,0,0,0},             /* 16 host mono, source transparency bit 10 */
 {0xCC000142UL,0xFFFF8000UL,0xFF004080UL,4,4,32,8,0,0,OP_MSRC|OP_HOSTM,0,0,0,0,0},
 {0xCC000009UL,0,0,4,4,24,16,32,36,OP_COPY|OP_ROT,0,0,1,0,0},                                      /* 18 bitblt+rotate 90 */
 {0xCC000009UL,0,0,4,4,24,16,32,36,OP_COPY|OP_ROT,0,0,2,0,0},                                      /* 19 rotate 180 */
 {0xCC000009UL,0,0,4,4,24,16,32,36,OP_COPY|OP_ROT,0,0,3,0,0},
 {0xCC002041UL,0,0,4,4,32,8,0,0,OP_HOSTC,0,0,0,0,0},                                                  /* 21 XP host colour blit, constant window address */
 {0xCC002041UL,0,0,4,4,32,8,0,0,OP_HOSTC,0,0,0,0,1},                                                  /* 22 XP host colour blit, incrementing window addresses */
 {0xCC0A0141UL,0xFFFF8000UL,0xFF004080UL,4,4,32,8,0,0,OP_MSRC|OP_HOSTM,0,0,0,0,1},
 {0xF0002001UL,0xFFC08000UL,0,4,4,24,16,0,0,OP_DKEY,0,0,0,0,0},                                      /* 24 fixed-colour fill with destination colour key */
 {0xCC021542UL,0xFFFF8000UL,0xFF004080UL,4,4,32,8,12,6,OP_MSRC|OP_HOSTM|OP_TRANS|OP_TEXT,27,9,0,0,1}};  /* 25 XP host text with clip window, transparent mono */                  /* 23 XP host mono blit (dword aligned), incrementing window addresses */                     /* 17 host text command 2 */
#define SF_2D_WX 448
#define SF_2D_WY 320
static uint32_t sf_ram_pat(uint32_t i) {
 return 0xFF000000UL|(((i*37)&0xFF)<<16)|(((i*91+13)&0xFF)<<8)|((i*53+7)&0xFF);
}
static uint32_t sf_2d_pre(int32_t x,int32_t y) {
 if(y>=SF_2D_MROW && y<SF_2D_MROW+8 && x>=0 && x<64) {
  uint32_t r=(uint32_t)(y-SF_2D_MROW);
  return x==0?((0xAAUL<<24)|(0x0FUL<<16)|((0x80UL>>r)<<8)|(1UL<<r)):0;
 }
 if(y==SF_2D_PATROW && x>=0 && x<64) return sf_ram_pat((uint32_t)x);
 if(x>=4 && x<12 && y>=4 && y<12) return SF_KEY;
 return 0xFF000000UL|(((uint32_t)(x*5+y*3)&0xFF)<<16)|(((uint32_t)(x*7+y*11)&0xFF)<<8)|((uint32_t)(x*13+y*17+x*y)&0xFF);
}
static uint32_t sf_rop3(uint32_t rop,uint32_t d,uint32_t sr,uint32_t p) {
 uint32_t k,out=0;
 for(k=0;k<32;k++) {
  uint32_t idx=(((p>>k)&1)<<2)|(((sr>>k)&1)<<1)|((d>>k)&1);
  if((rop>>idx)&1) out|=1UL<<k;
 }
 return out;
}
/* Ideal Bresenham from (x1,y1) to (x2,y2) inclusive: hit test. */
static int sf_line_hit(int32_t x1,int32_t y1,int32_t x2,int32_t y2,int32_t px,int32_t py) {
 int32_t dx=x2>x1?x2-x1:x1-x2,dy=y2>y1?y2-y1:y1-y2,sx=x2>x1?1:-1,sy=y2>y1?1:-1;
 int32_t n=dx>dy?dx:dy,m=dx>dy?dy:dx,e=2*m-n,i,x=x1,y=y1;
 for(i=0;i<=n;i++) {
  if(x==px && y==py) return 1;
  if(dx>=dy) {x+=sx; if(e>=0){y+=sy;e+=2*(m-n);} else e+=2*m;}
  else {y+=sy; if(e>=0){x+=sx;e+=2*(m-n);} else e+=2*m;}
 }
 return 0;
}
static uint32_t sf_2d_expect(const sf_2d_op *o,int32_t x,int32_t y) {
 uint32_t d=sf_2d_pre(x,y),rop=o->cmd>>24;
 int32_t rx=x-o->dx,ry=y-o->dy;
 if(o->flags&OP_LINE) {
  if((o->flags&OP_NOLAST) && x==o->sx && y==o->sy) return d;
  return sf_line_hit(o->dx,o->dy,o->sx,o->sy,x,y)?o->color:d;
 }
 if(rx<0||ry<0||rx>=o->w||ry>=o->h) return d;
 if(o->flags&OP_COPY) {
  uint32_t src=sf_2d_pre(o->sx+rx,o->sy+ry);
  if((o->flags&OP_SKEY) && src==SF_KEY) return d;
  if((o->flags&OP_DKEY) && d!=SF_KEY) return d;
  return sf_rop3(rop,d,src,0);
 }
 if(o->flags&OP_MONO) {
  uint32_t row=(uint32_t)((ry+o->offy)&7),col=(uint32_t)(rx&7); /* v9.23: X offset ignored for register patterns */
  uint32_t byte=((row<4?o->pat0:o->pat1)>>((3-(row&3))*8))&0xFF;
  if((byte>>col)&1) return o->color;
  return (o->flags&OP_TRANS)?d:o->bg;
 }
 if(o->flags&OP_HOSTC) return sf_ram_pat((uint32_t)(ry*32+rx));
 if(o->flags&OP_MSRC) {
  uint32_t bx=(uint32_t)rx,by=(uint32_t)ry,dw=(0xAAUL<<24)|(0x0FUL<<16)|((0x80UL>>by)<<8)|(1UL<<by);
  uint32_t byte=bx<32?((dw>>((bx>>3)*8))&0xFF):0;
  if((o->flags&OP_TEXT) && (x<o->sx||x>(int32_t)o->pat0||y<o->sy||y>(int32_t)o->pat1)) return d;
  if((byte>>(7-(bx&7)))&1) return o->color; /* v9.59: hardware mono bitmaps are MSB-first (bit 7 = leftmost) */
  return (o->flags&OP_TRANS)?d:o->bg;
 }
 if(o->flags&OP_FBPAT) return sf_ram_pat((uint32_t)((ry&7)*8+(rx&7)));
 if(o->flags&OP_RAM) return sf_ram_pat((uint32_t)((ry&7)*8+((rx+o->dx)&7)));
 if(o->flags&OP_DKEY) return d==SF_KEY?o->color:d;
 return sf_rop3(rop,d,0,o->color);
}
static const vx3d_sf_io *sf_seqio;
/* v10.330 (review #115): 1 when the engine finished (or the 2D reset brought it back idle), 0 when it is still busy: then the saved
 * registers are not written back into it */
static int sf_2d_run(const vx3d_io *io,vx3d_report *r,const sf_2d_op *o,uint32_t ri2) {
 static const uint32_t at[19]={0x04,0x08,0x0C,0x10,0x14,0x18,0x1C,0x20,0x24,0x28,0x2C,0x48,0x4C,0x50,0x58,0x5C,0x30,0x40,0x44};
 uint32_t saved[19],i,cmd=o->cmd;
 for(i=0;i<19;i++) saved[i]=rd(io,at[i]);
 wr(io,r,0x04,0x300UL|((o->flags&OP_ROT)?(uint32_t)o->offx:0UL));
 wr(io,r,0x08,(o->flags&(OP_HOSTM|OP_HOSTC))?(320UL<<16):((320UL<<16)|320UL)); /* v9.58: XP leaves the source pitch 0 for host-fed blits */
 wr(io,r,0x14,0); wr(io,r,0x1C,0);
 wr(io,r,0x30,0);
 wr(io,r,0x48,0);
 wr(io,r,0x58,o->color); wr(io,r,0x5C,o->bg);
 if(o->flags&OP_MSRC) {wr(io,r,0x4C,o->color); wr(io,r,0x50,o->bg); if(!(o->flags&OP_HOSTM)) wr(io,r,0x1C,((uint32_t)(SF_2D_WY+SF_2D_MROW)*SF_DESK_PITCH+(uint32_t)SF_2D_WX*4)>>3);}
 if(o->flags&OP_TEXT) {wr(io,r,0x24,0); wr(io,r,0x28,0); wr(io,r,0x40,((uint32_t)(SF_2D_WY+o->sy)<<16)|(uint32_t)(SF_2D_WX+o->sx)); wr(io,r,0x44,((uint32_t)(SF_2D_WY+(int32_t)o->pat1)<<16)|(uint32_t)(SF_2D_WX+(int32_t)o->pat0));}
 else {wr(io,r,0x24,o->pat0); wr(io,r,0x28,o->pat1);}
 wr(io,r,0x20,((uint32_t)o->offy<<29)|((uint32_t)o->offx<<26)|((o->flags&OP_FBPAT)?(((uint32_t)(SF_2D_WY+SF_2D_PATROW)*SF_DESK_PITCH+(uint32_t)SF_2D_WX*4)>>3):0));
 if(o->flags&OP_RAM) for(i=0;i<64;i++) wr(io,r,0x100+i*4,sf_ram_pat(i));
 if(o->flags&OP_SKEY) {wr(io,r,0x50,SF_KEY); wr(io,r,0x48,0x4000UL);}
 if(o->flags&OP_DKEY) {wr(io,r,0x4C,SF_KEY); wr(io,r,0x48,0x8000UL);}
 if(o->flags&OP_LINE) {
  int32_t ddx=o->sx-o->dx,ddy=o->sy-o->dy,adx,ady,n,m;
  adx=ddx<0?-ddx:ddx; ady=ddy<0?-ddy:ddy;
  if(ddx<0) cmd|=0x8000UL;
  if(ddy<0) cmd|=0x4000UL;
  if(ady>adx) {cmd|=0x200000UL; n=ady; m=adx;} else {n=adx; m=ady;}
  wr(io,r,0x4C,o->color);
  wr(io,r,0x18,((uint32_t)(2*m)<<16)|((uint32_t)(2*(m-n))&0x3FFFUL));
  wr(io,r,0x2C,(uint32_t)(2*m-n)&0x3FFFUL);
  wr(io,r,0x0C,(uint32_t)n&0xFFFUL);
  wr(io,r,0x10,((uint32_t)(SF_2D_WY+o->dy)<<16)|(uint32_t)(SF_2D_WX+o->dx));
 } else {
  wr(io,r,0x0C,((uint32_t)(o->h-1)<<16)|(uint32_t)(o->w-1));
  if(o->flags&OP_MSRC) wr(io,r,0x18,0);
  else if(o->flags&OP_COPY) {
   wr(io,r,0x18,((uint32_t)(SF_2D_WY+o->sy)<<16)|(uint32_t)(SF_2D_WX+o->sx));
  }
  wr(io,r,0x10,((uint32_t)(SF_2D_WY+o->dy)<<16)|(uint32_t)(SF_2D_WX+o->dx));
 }
 wr(io,r,0x00,cmd);
 if(o->flags&(OP_HOSTM|OP_HOSTC)) {
  uint32_t n=(o->flags&OP_HOSTC)?256:8,q,by;
  for(q=0;q<n;q++) {
   uint32_t dv;
   if(o->flags&OP_HOSTC) dv=sf_ram_pat(q);
   else {by=q&7; dv=(0xAAUL<<24)|(0x0FUL<<16)|((0x80UL>>by)<<8)|(1UL<<by);}
   wr(io,r,0x200000UL+((o->key==1)?q*4UL:0UL),dv); /* v9.57: HCM 2D Host BitBLT Space is MB1+0x200000..0x3FFFFF, not pattern RAM 0x100 */
  }
 }
 if(!idle(io,r)) {
  /* v9.26: HCM SR4E bits 3:2 = 01 is a 2D engine reset. Try it so one hung
   * command does not end the whole proof run. */
  uint32_t v,k,ms;
  r->sf_raw[13][ri2*2]=1;
  if(!sf_seqio) return 0;
  v=sf_seqio->read_seq(sf_seqio->context,0x4E);
  sf_seqio->write_seq(sf_seqio->context,0x4E,(v&~0x0CUL)|0x04UL);
  ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
  sf_seqio->write_seq(sf_seqio->context,0x4E,v&~0x0CUL);
  ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
  wr(io,r,0x04,0x300);
  if(idle(io,r)) {r->sf_raw[13][ri2*2+1]=1; r->wait_failure_reason=0; r->wait_failure_status=0;}
  else return 0;
 }
 for(i=0;i<19;i++) wr(io,r,at[i],saved[i]);
 return 1;
}
/* v9.19: the desktop retarget writes 01/88 and 01/89. The scratch target had
 * 01/88=0 and 01/89=64. Variants isolate which one moves the triangle tie rule. */
static void sf_dvar(uint32_t ci,uint32_t *a,uint32_t *b) {
 static const uint32_t w89[10]={0x280,0x200,0x300,0x400,0x100,0x1E0,0x280,0x280,0x280,0x280};
 *a=0x1ED0UL;*b=0x800UL;
 if(ci>=16 && ci<=25) {*b=w89[ci-16]; if(ci==24) *a=0;}
}
/* v10.330 (review #116): 0 when the engine did not go idle between the set-up steps (nothing more is written then) */
static int sf_fill_run(const vx3d_io *io,vx3d_report *r,vx3d_case_report *c,
 int kind,const int32_t *g,uint32_t color,const int32_t *win) {
 sf_fill_ctx f; uint32_t a,x,y,start=io->milliseconds(io->context);
 int32_t cw=(int32_t)VX3D_SIDE,ch=(int32_t)VX3D_SIDE;
 sf_hi=(vx3d_x_reach>=2UL && vx3d_x_reach<=3UL && (uint32_t)(c-r->sf_matrix)>=16UL && (uint32_t)(c-r->sf_matrix)<=25UL)?sf_reach_off[(uint32_t)(c-r->sf_matrix)-16UL]:0UL;
 sf_hi_at=0; if(sf_hi) sf_hi_keep(io,sf_hi);
 f.io=io; f.r=r; f.spans=0; f.pixels=0;
 if(win[2]) {
  if(kind>=5 && kind<=8) {
   /* v9.21: 2D engine proof. Prefill the window with a known pattern, run
    * one 2D engine operation, leave verification to the measure pass. */
   uint32_t yy,xx;
   for(yy=0;yy<(uint32_t)win[3];yy++) for(xx=0;xx<(uint32_t)win[2];xx++)
    sf_desk_save[yy*(uint32_t)win[2]+xx]=io->read_fb(io->context,(uint32_t)(win[1]+(int32_t)yy)*SF_DESK_PITCH+(uint32_t)(win[0]+(int32_t)xx)*4);
   for(yy=0;yy<(uint32_t)win[3];yy++) for(xx=0;xx<(uint32_t)win[2];xx++)
    io->write_fb(io->context,(uint32_t)(win[1]+(int32_t)yy)*SF_DESK_PITCH+(uint32_t)(win[0]+(int32_t)xx)*4,sf_2d_pre((int32_t)xx,(int32_t)yy));
   for(yy=0;yy<8;yy++) for(xx=0;xx<8;xx++) r->sf_raw[12][yy*8+xx]=io->read_fb(io->context,yy*SF_DESK_PITCH+xx*4);
   (void)io->read_fb(io->context,(uint32_t)(win[1]+win[3]-1)*SF_DESK_PITCH);
   if(kind==5) {
    if(!sf_2d_run(io,r,&sf_2d_ops[g[0]],(uint32_t)g[0])) return 0;
    c->fill_rects=1; c->fill_ms=io->milliseconds(io->context)-start;
    return 1;
   }
  }
  /* Save the desktop window before anything can touch it. */
  if(kind<5||kind>8) for(y=0;y<(uint32_t)win[3];y++) for(x=0;x<(uint32_t)win[2];x++)
   sf_desk_save[y*(uint32_t)win[2]+x]=io->read_fb(io->context,
    (uint32_t)(win[1]+(int32_t)y)*SF_DESK_PITCH+(uint32_t)(win[0]+(int32_t)x)*4);
  /* Retarget RT0 to the desktop (linear, offset 0, ARGB8888, pitch 2560)
   * and open the clip windows to the XP full range used by variant 2. */
  type(io,r,1);
  if(kind==10) {
   /* v9.53: Gouraud proof. Keep the scratch-target state of proven case 9 (clip, bias, 01/88..8A from the case setup); only
    * retarget RT0 base/format and select the dither mode. */
   const sf_gr_t *q=&sf_gr_tests[vx3d_x_select==18?0:g[4]];
   uint32_t yy,xx;
   reg(io,r,0x50,(((uint32_t)win[1]*SF_DESK_PITCH+(uint32_t)win[0]*4UL)>>8)); reg(io,r,0x51,0);
   reg(io,r,0x52,(q->fmt<<16)|(SF_DESK_PITCH/32));
   reg(io,r,0x53,q->mode53);
   for(yy=0;yy<64;yy++) for(xx=0;xx<64;xx++)
    io->write_fb(io->context,(uint32_t)(win[1]+(int32_t)yy)*SF_DESK_PITCH+(uint32_t)(win[0]+(int32_t)xx)*4,0xA5A5A5A5UL);
   (void)io->read_fb(io->context,(uint32_t)(win[1]+63)*SF_DESK_PITCH);
  } else if(kind==9) {
   uint32_t yy,xx,d=sf_rt_tests[g[4]].dst;
   reg(io,r,0x50,(((uint32_t)win[1]*SF_DESK_PITCH+(uint32_t)win[0]*4UL)>>8)); reg(io,r,0x51,0);
   reg(io,r,0x52,((uint32_t)sf_rt_tests[g[4]].fmt<<16)|(SF_DESK_PITCH/32));
   /* v9.51: dither control search. 01/53 bits 16:15 = dither mode (00 table*max, 10 round*max, 01 table, 11 round),
    * 11:8 = ROP (XP writes 0x010C00); 01/01 bit 0 = dither enable for RT0. */
   if(vx3d_x_select==11) reg(io,r,0x53,0x010C0FUL);
   if(vx3d_x_select==12) {reg(io,r,0x53,0x000C0FUL); reg(io,r,0x01,c->state[1]|1UL);}
   if(vx3d_x_select==13) {reg(io,r,0x53,0x010C0FUL); reg(io,r,0x01,c->state[1]|1UL);}
   if(vx3d_x_select==14) reg(io,r,0x53,0x008C0FUL);
   for(yy=0;yy<64;yy++) for(xx=0;xx<64;xx++)
    io->write_fb(io->context,(uint32_t)(win[1]+(int32_t)yy)*SF_DESK_PITCH+(uint32_t)(win[0]+(int32_t)xx)*4,xx<32?((d<<16)|d):0xA5A5A5A5UL);
   (void)io->read_fb(io->context,(uint32_t)(win[1]+63)*SF_DESK_PITCH);
  } else {
  reg(io,r,0x50,0); reg(io,r,0x51,0);
  reg(io,r,0x52,(0x99UL<<16)|(SF_DESK_PITCH/32));
  }
  if(kind!=10) {
  reg(io,r,0x80,0x000800UL); reg(io,r,0x81,0x000800UL);
  reg(io,r,0x82,0x000800UL); reg(io,r,0x83,0x000800UL);
  {uint32_t v88,v89; sf_dvar((uint32_t)(c-r->sf_matrix),&v88,&v89);
   if(kind==9) {v88=0x1ED0UL;v89=0x800UL;}
   reg(io,r,0x88,v88); reg(io,r,0x89,v89); reg(io,r,0x8A,0);}
  }
  cw=SF_DESK_W; ch=SF_DESK_H;
 }
 /* constant bank 0: float R,G,B,A (cyan 0,1,1,1 is what the shader was proven with) */
 wr(io,r,0x43C,0x22030000UL);
 for(a=0;a<4;a++) wr(io,r,0x440,(kind==9)?sf_rt_tests[g[4]].fb[a]:(kind==10&&vx3d_x_select==18)?(a==3?0x3F800000UL:sf_fog_f(sf_fog_tests[g[4]].c[a])):(kind==6)?(a==0?0x3F000000UL:a==1?0x3E800000UL:a==2?0x3F400000UL:sf_3d_tests[g[4]].alpha):sf_fill_float[color][a]);
 if(!idle(io,r)) return 0;

 if(kind==8) {
  const sf_tx_t *tt=&sf_tx_tests[g[4]];
  static const uint32_t fmtcode[5]={0x980000UL,0x130000UL,0xB80000UL,0xE80000UL,0x980000UL};
  static const uint32_t fe0[15]={0x10,0x10,0x12,0x12,0x16,0x1A,0x12,0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10};
  uint32_t ti,k,fl,mp;
  /* v9.45: V0 = XP's captured shader/texture bundle + screen-coordinate path + non-uniform texture (the proof run).
   * V1..V5 = same bundle but with ONE piece reverted to our old value (bisect, uniform texture).
   * V6 = full XP vertex path (explicit UVs, output mask), uniform texture. */
  uint32_t sel=sf_rotsel()?0UL:vx3d_x_select;
  int rot=sf_rotsel();
  uint32_t xs=(sel==0)?14:0;
  uint32_t tcache=0x100UL;
  uint32_t xpx=1UL;
  uint32_t xpfull=(sel==0)?1UL:0UL;
  int32_t vx[6];
  sf_tx_uniform=(sel!=0&&sel!=6)?1UL:0UL; /* sel 7: old TAU + XP ALU; sel 8: XP TAU + old ALU (uniform, screen path) */
  for(ti=0;ti<64*64;ti++) {
   if(tt->fmt==TXF_L8 && ti>=1024) break;
   io->write_fb(io->context,SF_DEPTHB+ti*4,sf_tx_word(tt->fmt,ti));
  }
  (void)io->read_fb(io->context,SF_DEPTHB+(tt->fmt==TXF_L8?1024:4096)*4-4);
  /* general texture state: one texture, clear the texture cache */
  wr(io,r,0x43C,0xFE020000UL);
  {uint32_t fv=fe0[xs<15?xs:0];
   if(xpfull) reg(io,r,0x00,0x11UL); else {reg(io,r,0x00,fv|1UL); reg(io,r,0x00,fv);}}
  reg(io,r,0x01,xpfull?0x21UL:0x000001UL);
  if(xpfull) reg(io,r,0x08,0x101UL);
  reg(io,r,0x04,(xs==14||xpfull)?0UL:((tt->wexp<<20)|(tt->hexp<<16)|1UL)); /* V14: no screen-coordinate replacement, explicit vertex UVs */
  /* texture 0 attributes */
  wr(io,r,0x43C,0x00020000UL);
  reg(io,r,0x00,SF_DEPTHB>>8);
  reg(io,r,0x18,(xpx&&sel!=3)?0x1100UL:0x100UL);
  reg(io,r,0x20,(64UL<<12)|64UL);
  reg(io,r,0x21,1UL);
  reg(io,r,0x22,(xpx&&sel!=3)?0x66UL:0x8066UL);
  reg(io,r,0x30,fmtcode[tt->fmt]|((xpx&&sel!=3&&(tt->fmt==TXF_ARGB||tt->fmt==TXF_ARGBA))?0x10000UL:0UL) /* 0x990000 = A8R8G8B8 with alpha (XP), 0x980000 = no alpha: case 25 needs the alpha form */);
  /* sample stage 0 */
  fl=tt->filter?((1UL<<13)|(1UL<<10)|(1UL<<7)|(1UL<<4)):0UL;
  mp=tt->addr|(tt->addr<<3);
  wr(io,r,0x43C,0x20020000UL);
  if(xpx&&sel!=4) {fl|=0x10000UL; mp|=0x80UL;}
  reg(io,r,0x30,0); reg(io,r,0x31,fl); reg(io,r,0x32,mp);
  reg(io,r,0x35,0); reg(io,r,0x36,0);
  if(xpx&&sel!=5) {wr(io,r,0x43C,0x10030000UL); for(k=0;k<8;k++) wr(io,r,0x440,0);}
  {uint32_t *q=&r->sf_raw[g[4]][48];
   q[0]=0x001101B0UL;q[1]=0x01C00530UL;q[2]=0x301C0053UL;q[3]=0x00F00105UL;q[4]=0x000D0820UL;
   q[5]=((xs==8)?0UL:(xs==9)?0x8000UL:0x18000UL)|((xs==1||(xs>=3&&xs<=9))?0x700000UL:(xs==11)?0x800000UL:0UL);q[6]=0x10001UL;q[7]=1UL|((xs==12)?0x100UL:0UL)|((xs==10)?0x2000000UL:0UL); /* A93 | A95<<8 | A9A<<16 */q[8]=vx3d_x_select|(1UL<<8)|((uint32_t)fe0[xs<15?xs:0]<<16); /* 7:0 variant, 15:8 A91, 23:16 FE00 fetch word */
   q[9]=SF_DEPTHB>>8;q[10]=0x100UL;q[11]=(64UL<<12)|64UL;q[12]=0x8066UL;
   q[13]=fmtcode[tt->fmt];q[14]=fl;q[15]=mp;}
  if(!idle(io,r)) return 0;
  /* pixel shader: TAU sample from t0 -> temp1, ALU move temp1 -> output */
  type(io,r,1);
  reg(io,r,0x01,c->state[1]&~0x400UL);
  reg(io,r,0x90,((xs==8)?0UL:(xs==9)?0x8000UL:0x18000UL)|((xs==1||(xs>=3&&xs<=9))?0x700000UL:(xs==11)?0x800000UL:0UL)); reg(io,r,0x91,1UL); /* A91 bit0 = t0 feeds the TAU */ reg(io,r,0x92,0x10001UL);
  reg(io,r,0x93,1); for(k=0x94;k<=0x9A;k++) reg(io,r,k,(k==0x95&&xs==12)?1UL:(k==0x9A&&xs==10)?0x200UL:0UL);
  if(xpx&&sel!=1) reg(io,r,0x9B,0xFFFFFEUL);
  wr(io,r,0x43C,0x21030000UL); wr(io,r,0x440,(xpx&&sel!=2&&sel!=7)?0x301001B0UL:0x001101B0UL);
  wr(io,r,0x43C,0x20030000UL);
  wr(io,r,0x440,0x01C00530UL); wr(io,r,0x440,0x301C0053UL);
  wr(io,r,0x440,(xpx&&sel!=2&&sel!=8)?0x00F20005UL:0x00F00105UL); wr(io,r,0x440,0x000D0820UL);
  type(io,r,1);
  reg(io,r,0x01,c->state[1]|tcache);
  if(tt->blend) {
   reg(io,r,0x00,0x10); reg(io,r,0x34,0x020); reg(io,r,0x35,0x148);
   reg(io,r,0x36,0); reg(io,r,0x37,0);
  }
  if(!idle(io,r)) return 0;
  if(sel==0 && g[4]==0) sf_state_dump(io,r); /* v9.43: read back the latched texture/PS/enable state before the draw */
  vx[0]=g[0];vx[1]=g[1];vx[2]=g[2];vx[3]=g[1];vx[4]=g[0];vx[5]=g[3];
  if(xs==6) {vx[5]=g[1]+8;}
  if(xs==7) {
   int32_t tv[12]; uint32_t nv=6;
   tv[0]=g[0];tv[1]=g[1];tv[2]=g[2];tv[3]=g[1];tv[4]=g[0];tv[5]=g[3];
   tv[6]=g[2];tv[7]=g[1];tv[8]=g[2];tv[9]=g[3];tv[10]=g[0];tv[11]=g[3];
   type(io,r,4); reg(io,r,0x23,0xCE02UL); reg(io,r,0x24,nv);
   type(io,r,0);
   for(k=0;k<nv;k++) {
    wr(io,r,0x440,sf_half_bits((uint32_t)(2*tv[k*2])));
    wr(io,r,0x440,sf_half_bits((uint32_t)(2*tv[k*2+1])));
    wr(io,r,0x440,0x3F000000UL); wr(io,r,0x440,0x3F800000UL); wr(io,r,0x440,0xFFE04020UL);
   }
   if(!idle(io,r)) return 0;
   type(io,r,4); reg(io,r,0x23,c->mode); reg(io,r,0x24,3);
  } else {
  type(io,r,4);
  if(xs==14) {
   /* flexible vertex format: X Y Z W Cd S(A) T(A), 7 words per vertex */
   reg(io,r,0x00,7UL); reg(io,r,0x01,0x020100UL); reg(io,r,0x02,0x080503UL); reg(io,r,0x03,0x09UL);
   reg(io,r,0x25,0xF4C000UL);
   if(xpfull) reg(io,r,0x28,0xFDC000UL);
  }
  reg(io,r,0x23,c->mode); reg(io,r,0x24,3);
  type(io,r,0);
  for(k=0;k<3;k++) {
   wr(io,r,0x440,sf_half_bits((uint32_t)(2*vx[k*2])));
   wr(io,r,0x440,sf_half_bits((uint32_t)(2*vx[k*2+1])));
   wr(io,r,0x440,0x3F000000UL); wr(io,r,0x440,0x3F800000UL); wr(io,r,0x440,0xFFE04020UL);
   if(xs==14) {
    {int32_t uu=vx[k*2],vv=vx[k*2+1]; if(rot) sf_rot_map(rot,uu,vv,&uu,&vv); wr(io,r,0x440,sf_scaled_bits((uint32_t)(uu>0?uu:0),tt->wexp)); wr(io,r,0x440,sf_scaled_bits((uint32_t)(vv>0?vv:0),tt->hexp));} /* UV = absolute screen position / 2^exp, as the screen path computes it */
   }
  }
  if(!idle(io,r)) return 0;
  if(xs==14) {
   type(io,r,4);
   reg(io,r,0x00,c->config[0]|c->state[24]); reg(io,r,0x01,c->config[10]); reg(io,r,0x02,c->config[11]);
   reg(io,r,0x25,c->config[1]);
   reg(io,r,0x28,c->state[2]);
  }
  }
  /* restore: blend off, constant shader, no textures */
  type(io,r,1);
  reg(io,r,0x00,0); reg(io,r,0x34,0); reg(io,r,0x35,0); reg(io,r,0x36,0); reg(io,r,0x37,0);
  reg(io,r,0x01,c->state[1]&~0x400UL);
  for(k=0;k<11;k++) reg(io,r,0x90+k,c->shader[2+k]);
  if(xpx) reg(io,r,0x9B,0UL);
  wr(io,r,0x43C,0x20030000UL);
  for(k=0;k<4;k++) wr(io,r,0x440,c->shader[13+k]);
  type(io,r,1); reg(io,r,0x01,c->state[1]);
  wr(io,r,0x43C,0xFE020000UL);
  reg(io,r,0x01,0); reg(io,r,0x04,0); reg(io,r,0x00,0x01); reg(io,r,0x00,0);
  if(!idle(io,r)) return 0;
  c->fill_rects=1; c->fill_ms=io->milliseconds(io->context)-start;
  return 1;
 }

 if(kind==7) {
  const sf_z_t *zt=SF_ZT(g[4]);
  const sf_st_t *st=(vx3d_x_select==17)?&sf_st_tests[g[4]]:0;
  uint32_t zi,pi;
  static const int32_t rc[2][4]={{456,328,488,360},{472,344,504,376}};
  uint32_t zv[2],cv[2][3];
  zv[0]=st?st->p[0][7]:zt->za; zv[1]=st?st->p[1][7]:zt->zb;
  cv[0][0]=0x3F800000UL;cv[0][1]=0;cv[0][2]=0;  /* A red */
  cv[1][0]=0;cv[1][1]=0;cv[1][2]=0x3F800000UL;  /* B blue */
  for(zi=0;zi<64*64;zi++) io->write_fb(io->context,SF_DEPTHB+zi*4,st?(0xFFFFFF00UL|st->s0):zt->clear);
  (void)io->read_fb(io->context,SF_DEPTHB+64*64*4-4);
  type(io,r,1);
  reg(io,r,0x10,(SF_DEPTHB-(320UL*256+448UL*4))>>8);
  reg(io,r,0x11,(256UL/32)<<13);
  reg(io,r,0x12,(st?6UL:4UL)<<16);
  reg(io,r,0x13,(st?1UL:zt->mode)<<16);
  reg(io,r,0x01,c->state[1]|(st?0x1CUL:(zt->zwrite?0xCUL:0x8UL)));
  if(!idle(io,r)) return 0;
  for(pi=0;pi<2;pi++) {
   int32_t vx[6]; uint32_t k,zf;
   /* convert fixed Z to float bits for the vertex: .25/.5/.75 */
   zf=(zv[pi]==0x40000000UL)?0x3E800000UL:(zv[pi]==0x80000000UL)?0x3F000000UL:0x3F400000UL;
   if(st) {
    const uint32_t *q=st->p[pi];
    uint32_t w1=(q[1]<<16)|(q[2]<<8)|q[3],w2=(q[0]<<16)|(q[4]<<6)|(q[5]<<3)|q[6];
    type(io,r,1);
    reg(io,r,0x23,w1); reg(io,r,0x24,w2); reg(io,r,0x25,w1); reg(io,r,0x26,w2);
    if(!idle(io,r)) return 0;
   }
   wr(io,r,0x43C,0x22030000UL);
   wr(io,r,0x440,cv[pi][0]); wr(io,r,0x440,cv[pi][1]); wr(io,r,0x440,cv[pi][2]); wr(io,r,0x440,0x3F800000UL);
   if(!idle(io,r)) return 0;
   vx[0]=rc[pi][0];vx[1]=rc[pi][1];vx[2]=rc[pi][2];vx[3]=rc[pi][1];vx[4]=rc[pi][0];vx[5]=rc[pi][3];
   type(io,r,4); reg(io,r,0x23,c->mode); reg(io,r,0x24,3);
   type(io,r,0);
   for(k=0;k<3;k++) {
    wr(io,r,0x440,sf_half_bits((uint32_t)(2*vx[k*2])));
    wr(io,r,0x440,sf_half_bits((uint32_t)(2*vx[k*2+1])));
    wr(io,r,0x440,zf); wr(io,r,0x440,0x3F800000UL); wr(io,r,0x440,0xFFE04020UL);
   }
   if(!idle(io,r)) return 0;
  }
  type(io,r,1); reg(io,r,0x01,c->state[1]); reg(io,r,0x13,0x10000UL);
  if(st) {reg(io,r,0x23,0); reg(io,r,0x24,0); reg(io,r,0x25,0); reg(io,r,0x26,0); reg(io,r,0x12,0);}
  if(!idle(io,r)) return 0;
  c->fill_rects=2; c->fill_ms=io->milliseconds(io->context)-start;
  return 1;
 }
 if(kind>=3) {
  /* v9.17: one native hardware primitive into the desktop target. The
   * case setup already issued bias, clamp and mode (CE02 triangle, CE04 rect). */
  int32_t vx[6]; uint32_t k;
  if(kind==3) {for(k=0;k<6;k++) vx[k]=g[k];}
  else if(kind==10) {
   if(vx3d_x_select!=18 && sf_gr_tests[g[4]].rev) {vx[0]=8;vx[1]=8;vx[2]=8;vx[3]=56;vx[4]=56;vx[5]=8;}
   else {vx[0]=8;vx[1]=8;vx[2]=56;vx[3]=8;vx[4]=8;vx[5]=56;}
  }
  else {vx[0]=g[0];vx[1]=g[1];vx[2]=g[2];vx[3]=g[1];vx[4]=g[0];vx[5]=g[3];}
  if(kind==10 && vx3d_x_select==18) {
   const sf_fog_t *ft=&sf_fog_tests[g[4]];
   uint32_t fx[3],k2,dm=(ft->end_m>ft->start_m)?(ft->end_m-ft->start_m):1,oo,e127,mant=0;
   for(k2=0;k2<3;k2++) fx[k2]=(ft->f[k2]*0x400UL)/1000UL;
   oo=(1000000UL/dm); /* 1/(end-start) x1000: 1000 -> exponent 127, 2000 -> 128, 4000 -> 129 */
   e127=oo>=4000?129UL:oo>=2000?128UL:127UL;
   type(io,r,1);
   reg(io,r,0x00,ft->en?0x100UL:0UL);
   reg(io,r,0x70,ft->mode70); reg(io,r,0x71,(fx[1]<<12)|fx[2]); reg(io,r,0x72,fx[0]);
   reg(io,r,0x73,sf_fog_h(ft->start_m)); reg(io,r,0x75,mant); reg(io,r,0x76,e127); reg(io,r,0x77,sf_fog_h(ft->end_m));
   if(!idle(io,r)) return 0;
  }
  if(kind==9 && sf_rt_tests[g[4]].blend) {
   type(io,r,1);
   reg(io,r,0x00,0x10UL); reg(io,r,0x02,0);
   reg(io,r,0x33,0); reg(io,r,0x34,0x020UL); reg(io,r,0x35,0x148UL);
   reg(io,r,0x36,0); reg(io,r,0x37,0);
   if(!idle(io,r)) return 0;
  }
  if(kind==6) {
   const sf_3d_t *t3=&sf_3d_tests[g[4]];
   type(io,r,1);
   reg(io,r,0x00,t3->es1); reg(io,r,0x02,t3->es3);
   reg(io,r,0x33,t3->a33); reg(io,r,0x34,t3->b34); reg(io,r,0x35,t3->b35);
   reg(io,r,0x36,t3->b36); reg(io,r,0x37,t3->b37);
   if(t3->colour_off) reg(io,r,0x01,c->state[1]&~0x40UL);
   if(!idle(io,r)) return 0;
  }
  type(io,r,4); reg(io,r,0x23,c->mode); reg(io,r,0x24,3);
  type(io,r,0);
  for(k=0;k<3;k++) {
   wr(io,r,0x440,sf_half_bits((uint32_t)(2*vx[k*2])));
   wr(io,r,0x440,sf_half_bits((uint32_t)(2*vx[k*2+1])));
   wr(io,r,0x440,(kind==10&&vx3d_x_select==18)?(((uint32_t)vx[k*2]==8&&(uint32_t)vx[k*2+1]==8)?0x3E800000UL:((uint32_t)vx[k*2]==56)?0x3F400000UL:0x3F000000UL):0x3F000000UL); wr(io,r,0x440,0x3F800000UL);
   if(kind==10 && vx3d_x_select==18) wr(io,r,0x440,0xFFE04020UL);
   else if(kind==10) {
    /* colour follows position: TL red, TR green, BL blue */
    uint32_t px=(uint32_t)vx[k*2],py=(uint32_t)vx[k*2+1];
    wr(io,r,0x440,(px==8&&py==8)?0xFFFF0000UL:(px==56)?0xFF00FF00UL:0xFF0000FFUL);
   } else wr(io,r,0x440,0xFFE04020UL);
  }
  if(!idle(io,r)) return 0; f.spans=1; f.pixels=0;
  if(kind==9||kind==10) {
   type(io,r,1);
   if(kind==10 && vx3d_x_select==18) {reg(io,r,0x70,0); reg(io,r,0x71,0); reg(io,r,0x72,0);}
   reg(io,r,0x00,0); reg(io,r,0x02,0); reg(io,r,0x33,0);
   reg(io,r,0x34,0); reg(io,r,0x35,0); reg(io,r,0x36,0); reg(io,r,0x37,0);
   reg(io,r,0x01,c->state[1]);
   reg(io,r,0x53,0xC0FUL);
   reg(io,r,0x53,0xC0FUL);
   reg(io,r,0x50,0); reg(io,r,0x51,0); reg(io,r,0x52,(0x99UL<<16)|(SF_DESK_PITCH/32));
   if(!idle(io,r)) return 0;
  }
  if(kind==6) {
   type(io,r,1);
   reg(io,r,0x00,0); reg(io,r,0x02,0); reg(io,r,0x33,0);
   reg(io,r,0x34,0); reg(io,r,0x35,0); reg(io,r,0x36,0); reg(io,r,0x37,0);
   reg(io,r,0x01,c->state[1]);
   if(!idle(io,r)) return 0;
  }
 } else if(kind==1) vx_fill_triangle(g[0],g[1],g[2],g[3],g[4],g[5],cw,ch,sf_fill_emit,&f);
 else vx_fill_rect(g[0],g[1],g[2],g[3],cw,ch,sf_fill_emit,&f);
 r->sf_fill_spans+=f.spans; r->sf_fill_pixels+=f.pixels;
 c->fill_rects=f.spans; c->fill_ms=io->milliseconds(io->context)-start;
 return 1;
}
static int sf_build_matrix(const vx3d_io *io,vx3d_report *r) {
 static const uint32_t modes[32]={0,0x4F02,0,0x4F02,0,0x4F02,
  0xCE01,0x4F02,0,0x4F02,0,0x4F02,0xCE04,0x4F02,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
 static const uint32_t profiles[32]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
 static const uint32_t dependencies[32]={0xFFFFFFFFUL,0,0,2,0,4,0,6,0,8,0,10,0,0,0xFFFFFFFFUL,
  0,15,15,15,15,15,15,15,15,15,15,0,0,27,27,27,0};
 static const uint32_t context_fe[23]={
  0x000E1004UL,0x01000000UL,0x08000000UL,0x0A0002C9UL,0x0B0002FBUL,0x0C000000UL,0x0D0002CBUL,0x0E000000UL,0x10000000UL,0x11000000UL,0x12000000UL,0x130002DBUL,0x14000000UL,0x15000000UL,0x16000000UL,0x17000000UL,0x18000000UL,0x19000000UL,0x20000000UL,0x40008000UL,0x44000000UL,0x46000000UL,0x47000000UL
 };
 uint32_t i,v,a,n,begin,fe_profile; dma_builder b; vx3d_case_report *c;
 for(i=16;i<=25;i++) sf_variant_mask[i]=(vx3d_x_select==16)?(64|128|256):(64|128); /* v9.53: V16 Gouraud cases use the vertex-colour state of proven case 9 */
 r->sf_matrix_count=VX3D_MATRIX_CASES; r->sf_matrix_last=0xFFFFFFFFUL;
 for(i=0;i<VX3D_PIXELS;i++) r->image[i]=VX3D_BACKGROUND;
 for(i=0;i<VX3D_MATRIX_CASES;i++) {
  c=&r->sf_matrix[i]; c->mode=modes[i];
  /* Case 12 is a three-vertex rectangle. Case 13 is a plain triangle that
   * only runs if case 12 settled. Three-vertex lists have always
   * completed; the rectangle vertex contract is unknown. */
  c->vertices=(i==14)?1:(i==6?2:3);c->layout=((i>=16&&i<=25)||(i>=28&&i<=30))?1:0;
  c->config[0]=5; n=5; c->config[1]=0xF40000;
  c->config[2]=0;c->config[3]=0;c->config[4]=i==1||i==3?16:i==5?17:i==7?18:i==9?5:i==11?19:i==12?20:i==13?21:0;
  c->config[5]=VX3D_VERTICES+i*VX3D_CASE_SOURCE_BYTES;
  c->config[6]=c->vertices*n;c->config[7]=c->config[6];
  c->config[8]=dependencies[i];c->config[9]=1|(profiles[i]<<8)|(sf_rev[i]?2:0);
  c->config[10]=0x020100;c->config[11]=0x503;
  c->state[0]=0x8408;c->state[1]=0x440;
  c->state[2]=c->config[1];c->state[3]=0x800000;c->state[4]=0;
  c->state[5]=0x3F00;c->state[6]=0x10000;c->state[7]=0;
  c->state[8]=c->state[11]=c->state[14]=0x800000;
  c->state[10]=c->state[13]=0x3F;c->state[16]=0x92003F;
  c->state[17]=0;c->state[18]=VX3D_SIDE;c->state[19]=0;
  c->state[20]=0;c->state[21]=0;c->state[22]=0x0B45C5;
  c->state[23]=0x400000;c->state[24]=0;
  c->shader[0]=1;c->shader[1]=0xFF00FFFFUL;
  c->shader[2]=0x58000;c->shader[4]=1;
  c->shader[13]=0x01C00530UL;c->shader[14]=0x301C0053UL;
  c->shader[15]=0x00F08005UL;c->shader[16]=0x000D0820UL;
  c->shader[17]=0;c->shader[18]=0x3F800000UL;
  c->shader[19]=0x3F800000UL;c->shader[20]=0x3F800000UL;
  c->state[1]|=sf_en2_extra[i];
  if(sf_variant_mask[i]&1) c->state[1]|=0xC;
 if(sf_variant_mask[i]&8) c->state[1]=0xCE1540UL|0x400;
 if(sf_variant_mask[i]&2) {c->state[17]=0x1ED0;c->state[18]=0x800;}
 if(i==11) {
  c->shader[1]=0xFFFF0000UL;c->shader[17]=0x3F800000UL;c->shader[18]=0;c->shader[19]=0;c->shader[20]=0x3F800000UL;
 }
 if(sf_variant_mask[i]&64) {
  c->state[9]=0x400000;c->state[10]=0x493F;c->state[12]=0x400000;c->state[13]=0x493F;
  if(!(sf_variant_mask[i]&1024)) c->state[22]=0x147FFF;
 }
 if(sf_variant_mask[i]&128) c->mode=0xCE02;
 if(i==28||i==30||(i>=16&&i<=25)) c->mode=((vx3d_x_select==16||vx3d_x_select==18)&&i>=16&&i<=25)?0xCE02:0xCE04;
 if(sf_variant_mask[i]&256) {
  c->shader[1]=0xFFE04020UL; /* captured program 00F04005 outputs the vertex colour */
  c->state[24]=0xA80000;c->state[2]=0xFD0000;c->state[23]=0x582000;c->state[16]=0x3F;
  c->state[1]=0xCE1540UL;c->shader[15]=0x00F04005UL;
 }
 if(sf_variant_mask[i]&512) {
  c->fe_override[0]=4;c->fe_override[1]=0x4000840FUL;c->fe_override[2]=0x44000000UL;
  c->fe_override[3]=0x46000005UL;c->fe_override[4]=0x47000404UL;c->state[0]=0x840F;
 }
 fe_profile=profiles[i];
  if(fe_profile==1 || fe_profile==3) {
   c->fe_override[++c->fe_override[0]]=0x40008000UL;
   c->state[0]=0x8000;
  }
  if(fe_profile==2 || fe_profile==3)
   c->fe_override[++c->fe_override[0]]=0x47000000UL;
  if(fe_profile==4) {
   c->fe_override[++c->fe_override[0]]=0x01000000UL;
   for(a=0x14;a<=0x19;a++)c->fe_override[++c->fe_override[0]]=a<<24;
  }
  if(fe_profile==5) {
   c->fe_override[0]=23;c->state[0]=0x8000;
   for(a=0;a<23;a++)c->fe_override[a+1]=context_fe[a];
  }
  c->first=i; c->address=SF_RING_BASE+SF_MATRIX_BASE+i*SF_MATRIX_STRIDE;
  c->latch_expected=0xFF6CDE00UL+i;
  for(v=0;v<c->vertices;v++) for(a=0;a<n;a++) c->input[v*n+a]=sf_case_vertex(c,v,a);
  b.real=io; b.report=r; b.words=r->sf_alt_program; b.count=0; b.error=0;
  dma_wait(&b,0x00400000UL);
  if(c->config[9]&1) {
   /* Exact inspected XP helper prefix at VA 1145E0..1146C9. */
   dma_header(&b,0x43C,0x00040000UL);
   dma_word(&b,0x23000000UL|c->mode); dma_word(&b,0x24000000UL|c->vertices);
  }
  if(c->layout==0 || c->layout==3) {
   dma_align(&b); dma_word(&b,0xFE03043CUL); dma_word(&b,c->config[7]);
   dma_word(&b,0); dma_word(&b,0xCC000000UL);
   if(c->layout==3) {
    if(c->config[3]==16) {
     dma_word(&b,0x00010000UL); dma_word(&b,2); /* unused upper index is safe 0 */
    } else for(v=0;v<c->vertices;v++) dma_word(&b,v);
   } else for(v=0;v<c->config[6];v++) dma_word(&b,c->input[v]);
  }
  /* CPU-path packets contain only synchronization and the trailing latch.
   * Their vertex data is sent directly before releasing this pause. */
  dma_wait(&b,0x00800000UL);
  dma_align(&b); dma_word(&b,0xFE000000UL); dma_word(&b,1);
  dma_word(&b,0); dma_word(&b,0xCC000000UL);
  dma_word(&b,0x58); dma_word(&b,c->latch_expected); sf_branch_pad(&b);
  begin=SF_MATRIX_BASE+i*SF_MATRIX_STRIDE;
  if(b.error || b.count>128 || c->config[6]>24 ||
     begin+b.count*4+0x200>=begin+SF_MATRIX_STRIDE ||
     begin+0x400>=VX3D_SF_RING_BYTES ||
     c->config[5]+VX3D_CASE_SOURCE_BYTES>VX3D_ARENA+VX3D_ARENA_BYTES) return 0;
  c->count=b.count; memcpy(c->program,b.words,b.count*4);
  c->pause=c->address+b.count*4-16; c->flags=1;
 }
 /* Publish immutable local VB slots while no command reader is armed.
  * Sources and their padding are checked by guards(), never exempted. */
 for(i=0;i<VX3D_MATRIX_CASES;i++) {
  c=&r->sf_matrix[i];
  for(v=0;v<c->config[6];v++) io->write_fb(io->context,c->config[5]+v*4,c->input[v]);
 }
 (void)io->read_fb(io->context,VX3D_VERTICES+VX3D_MATRIX_CASES*VX3D_CASE_SOURCE_BYTES-4);
 return 1;
}
static void sf_matrix_clear(const vx3d_io *io) {
 uint32_t i;
 /* Preserve source slots and guard history across cases. */
 for(i=0;i<VX3D_PIXELS*4;i+=4) {
  io->write_fb(io->context,VX3D_TARGET+i,VX3D_BACKGROUND);
  io->write_fb(io->context,VX3D_DEPTH+i,0xFFFFFFFFUL);
 }
 (void)io->read_fb(io->context,VX3D_DEPTH+VX3D_PIXELS*4-4);
}
static void sf_xp_state(const vx3d_io *io,vx3d_report *r) {
 /* v9.1: XP writes every documented 01h/04h state register; v9 left ~200
  * unwritten. Neutral zeros, except the non-zero XP shadow defaults
  * (VERIFICATION/XP_CONTEXT_DEFAULTS.json). Case values written later win. */
 uint32_t a;
 type(io,r,1);
 reg(io,r,0,0x00000FUL);
 for(a=0x03;a<=0x0F;a++) reg(io,r,a,0);
 reg(io,r,0x14,0xFFFFFFUL); reg(io,r,0x15,0x0000FFUL);
 reg(io,r,0x19,0x800000UL); reg(io,r,0x1A,0x800000UL); reg(io,r,0x1B,0x803FBFUL);
 reg(io,r,0x1C,0); reg(io,r,0x1D,0);
 for(a=0x23;a<=0x3F;a++) reg(io,r,a,0);
 for(a=0x58;a<=0x6B;a++) { if((a&7)<4) reg(io,r,a,(a&7)==3?0x010C0FUL:0); }
 for(a=0x70;a<=0x78;a++) reg(io,r,a,0);
 reg(io,r,0x84,0); reg(io,r,0x85,0);
 type(io,r,4);
 for(a=0x03;a<=0x0F;a++) reg(io,r,a,0);
 reg(io,r,0x2C,0); reg(io,r,0x2D,0); reg(io,r,0x2E,0);
}
static void sf_state_dump(const vx3d_io *io,vx3d_report *r) {
 /* HCM 10h/02h selects the stage ID, 10h/03h the register group; four
  * tagged words come back at 420..42C (see read_state_bank). */
 static const uint32_t space[VX3D_DUMP_SPACES]={0x000100,0x000400,0xFE0200,0x00FE00,0x000200,0x200200};
 uint32_t id,s,g;
 r->dump_flags|=1;
 for(id=0;id<VX3D_DUMP_IDS;id++) for(s=0;s<VX3D_DUMP_SPACES;s++) for(g=0;g<64;g++)
  if(!read_state_bank(io,r,space[s]|(g<<2),id,r->dump_words[id][s][g])) {
   r->dump_flags|=2; goto done;
  }
 r->dump_flags|=4;
done:
 type(io,r,0x10); reg(io,r,0x02,0); (void)idle(io,r);
}
static int sf_matrix_setup(const vx3d_io *io,vx3d_report *r,uint32_t which) {
 vx3d_case_report *c=&r->sf_matrix[which]; uint32_t a;
 setup_global(io,r);
 if(c->fe_override[0]) {
  type(io,r,0xFE);
  for(a=1;a<=c->fe_override[0];a++)wr(io,r,0x440,c->fe_override[a]);
 }
 clear_general(io,r); setup(io,r);
 sf_xp_state(io,r);
 if(sf_variant_mask[which]&16) {
  type(io,r,2); for(a=0;a<=0x13;a++) reg(io,r,a,0);
 }
 if(sf_variant_mask[which]&32) {
  type(io,r,1); reg(io,r,0x29,0x008000UL); if(!idle(io,r)) return 0; reg(io,r,0x29,0); if(!idle(io,r)) return 0; /* v10.330 (review #116) */
  wr(io,r,0x43C,0xFE020000UL); reg(io,r,0,1); if(!idle(io,r)) return 0; reg(io,r,0,0); if(!idle(io,r)) return 0;
 }
 type(io,r,1); reg(io,r,1,c->state[1]&~0x400UL);
 reg(io,r,0x88,c->state[17]);reg(io,r,0x89,c->state[18]);reg(io,r,0x8A,c->state[19]);
 if(sf_variant_mask[which]&2) {
  reg(io,r,0x80,0x000800UL);reg(io,r,0x81,0x000800UL);
  reg(io,r,0x82,0x000800UL);reg(io,r,0x83,0x000800UL);
 }
 if(sf_variant_mask[which]&4) reg(io,r,0x53,0x010C00UL);
 type(io,r,4);
 reg(io,r,0,c->config[0]|c->state[24]);reg(io,r,1,c->config[10]);reg(io,r,2,c->config[11]);
 reg(io,r,0x25,c->config[1]);reg(io,r,0x26,0);
 reg(io,r,0x28,c->state[2]);reg(io,r,0x29,0);reg(io,r,0x2F,c->state[23]);
 reg(io,r,0x50,c->state[20]);reg(io,r,0x51,c->state[21]);reg(io,r,0x52,c->state[22]);
 reg(io,r,0x2A,sf_pw[which]);reg(io,r,0x2B,sf_ph[which]);
 for(a=0;a<9;a++) reg(io,r,0x40+a,c->state[8+a]);
 reg(io,r,0x20,c->state[7]);
 if(c->layout==3) {
  reg(io,r,0x21,c->config[5]&0xFFFFFFUL);
  reg(io,r,0x22,(c->config[2]<<8)|(c->config[5]>>24));
  reg(io,r,0x2C,c->vertices-1);reg(io,r,0x2D,0);reg(io,r,0x2E,0);
 }
 if(c->shader[0]) {
  /* Program a complete one-ALU/no-TAU shader while PS is disabled.
   * These subtype payloads are raw DWORDs, not subaddress/value words. */
  type(io,r,1);
  for(a=0;a<11;a++)reg(io,r,0x90+a,c->shader[2+a]);
  wr(io,r,0x43C,0x22030000UL);
  for(a=0;a<4;a++)wr(io,r,0x440,c->shader[17+a]);
  wr(io,r,0x43C,0x20030000UL);
  for(a=0;a<4;a++)wr(io,r,0x440,c->shader[13+a]);
 }
 if(sf_variant_mask[which]&256) {
  type(io,r,1);
  reg(io,r,0x1E,0);reg(io,r,0x1F,0);reg(io,r,0x20,0x7FFF);reg(io,r,0x29,0);
  reg(io,r,0x82,0x40);reg(io,r,0x83,0x40);reg(io,r,0x53,0x010C0FUL);
  if(c->shader[0]) reg(io,r,0x9B,0xFFFFFFUL);
 }
 if(sf_variant_mask[which]&512) {wr(io,r,0x43C,0xFE020000UL);reg(io,r,0,1);}
 type(io,r,1);reg(io,r,1,c->state[1]);
 type(io,r,4);reg(io,r,0x23,c->mode);reg(io,r,0x24,c->vertices);
 c->flags|=2;r->init_flags|=2048;
 r->vertex_words=c->config[0];r->vertex_vertices=c->vertices;
 r->vertex_stride=c->config[2];
 return idle(io,r);
}
static int sf_matrix_cpu(const vx3d_io *io,vx3d_report *r,vx3d_case_report *c) {
 uint32_t i,color; int32_t g[6],win[4]; int kind=sf_fill_job(c->first,g,&color,win);
 if(kind) return sf_fill_run(io,r,c,kind,g,color,win);
 type(io,r,0);
 for(i=0;i<c->config[6];i++) wr(io,r,c->layout==2?0x440+i*4:0x440,c->input[i]);
 return 1;
}
static int sf_marker_snapshot(const vx3d_io *io,vx3d_report *r,uint32_t phase) {
 uint32_t bank,value;
 for(bank=0;bank<2;bank++) {
  if(!read_state_bank(io,r,0x04AA,bank,r->sf_state_raw[phase][bank])) return 0;
  value=r->sf_state_raw[phase][bank][2];
  if(!bank) r->sf_state_marker[phase*2+1]=value&0xFFFFUL;
  if((value>>24)==0xAA && (value&0xFFFFUL)==r->sf_state_marker[phase*2])
   r->sf_state_flags|=1UL<<(2+phase*2+bank);
 }
 type(io,r,0x10); reg(io,r,0x02,0); /* restore CR readback selection */
 if(!idle(io,r)) return 0;
 r->sf_state_flags|=phase?512:256;
 return 1;
}
static int sf_preload_state(const vx3d_io *io,vx3d_report *r) {
 clear_general(io,r); setup(io,r);
 type(io,r,4); reg(io,r,0x23,2); reg(io,r,0x24,3);
 reg(io,r,0xAA,r->sf_state_marker[0]);
 r->sf_state_flags|=1|128; r->init_flags|=2048;
 if(!idle(io,r)) return 0;
 r->sf_cpu_state_status=rd(io,0x400); r->sf_state_flags|=2;
 if(!read_state(io,r,0x0400,r->sf_state_fvf) ||
    !read_state(io,r,0x0150,r->sf_state_target)) return 0;
 return sf_marker_snapshot(io,r,0);
}
static int sf_enable_agp_queue(const vx3d_io *io,vx3d_report *r) {
 /* Original XP DLL VA 110EF0..111043: end=base+bytes-1,
  * length=(bytes-0x400)>>4, and AGP source selected by 74101005.
  * Apply only while paused/idle; all reachable local bytes are owned. */
 if(!idle(io,r)) return 0;
 r->sf_queue_compare_control=0x74101005UL;
 r->sf_queue_compare_start=SF_AGP_QUEUE;
 r->sf_queue_compare_end=SF_AGP_QUEUE+VX3D_QUEUE_BYTES-1;
 r->sf_queue_compare_length=(VX3D_QUEUE_BYTES-0x400UL)>>4;
 r->sf_queue_compare_status[0]=rd(io,0x400);
 type(io,r,0x10);
 reg(io,r,0x70,r->sf_queue_compare_start&0xFFFFFFUL);
 reg(io,r,0x71,r->sf_queue_compare_end&0xFFFFFFUL);
 reg(io,r,0x72,(r->sf_queue_compare_start>>24)|((r->sf_queue_compare_end>>24)<<8));
 reg(io,r,0x73,r->sf_queue_compare_length);
 r->sf_queue_compare_flags|=2; r->queue_flags|=VX3D_Q_ENABLED;
 reg(io,r,0x74,r->sf_queue_compare_control&0xFFFFFFUL); reg(io,r,0,0);
 if(!idle(io,r)) return 0;
 r->sf_queue_compare_status[1]=rd(io,0x400); r->sf_queue_compare_flags|=4;
 return 1;
}
static void sf_h0_write(void *context,uint32_t at,uint32_t value) {
 dma_builder *b=(dma_builder*)context;
 if(at>0x6C && at!=0x41C && at!=0x420 && at!=0x43C && at!=0x440) {
  b->error=1; return;
 }
 dma_word(b,at); dma_word(b,value);
}
static int sf_build_alternate(const vx3d_io *io,vx3d_report *r) {
 dma_builder b; vx3d_io record; uint32_t writes=r->writes;
 b.real=io; b.report=r; b.words=r->sf_alt_program; b.count=0; b.error=0;
 record=*io; record.context=&b; record.read_mmio=dma_read_mmio;
 record.write_mmio=sf_h0_write; record.read_fb=dma_no_fb_read;
 record.write_fb=dma_no_fb_write; record.milliseconds=dma_milliseconds;
 r->sf_alt_expected=r->queue_fence_original^0x4F250019UL;
 if(r->sf_alt_expected==0xFFFFFFFFUL) r->sf_alt_expected^=0x01000000UL;
 dma_word(&b,0xFE000000UL); dma_word(&b,0); dma_word(&b,0); dma_word(&b,0xCC000000UL);
 /* Clear only the owned target, on the GPU, after the first draw settles.
  * This makes the two triangle measurements independent while the ring
  * bytes remain immutable. No quick-start, source fetch, or desktop write. */
 wr(&record,r,0x6C,0x00800000UL);
 wr(&record,r,0x04,0x300); wr(&record,r,0x14,VX3D_TARGET>>3);
 wr(&record,r,0x08,(VX3D_SIDE*4/8)<<16);
 wr(&record,r,0x10,0); wr(&record,r,0x0C,0x003F003FUL);
 wr(&record,r,0x48,0); wr(&record,r,0x58,VX3D_BACKGROUND);
 wr(&record,r,0x00,0xF0002001UL); wr(&record,r,0x6C,0x00400000UL);
 clear_general(&record,r); setup(&record,r); probe_submit(&record,r);
 wr(&record,r,0x6C,0x00800000UL);
 fence_set(&record,r,r->sf_alt_expected);
 r->sf_alt_program[1]=(b.count-4)/2;
 dma_align(&b); dma_header(&b,0x41C,0x00300000UL);
 do {dma_word(&b,0xCCC00002UL); dma_word(&b,0xDDD00002UL);}
 while((b.count&63) && !b.error);
 r->writes=writes;
 if(b.error) return 0;
 r->sf_alt_count=b.count; r->sf_alt_flags|=1;
 r->sf_alt_pause=SF_RING_BASE+SF_ALT_DRAW+b.count*4-16;
 return 1;
}
static int sf_cpu_pair(const vx3d_io *io,vx3d_report *r,uint32_t *expected,
 uint32_t *observed,uint32_t *restored,uint32_t *flags,uint32_t salt) {
 uint32_t i,port,id;
 if(r->cpu_original==0xFFFFFFFFUL) return -1;
 for(i=0;i<2;i++) {
  port=i?0x43C:0x41C; id=r->cpu_original^(i?0x643CA002UL:0x61C0A001UL)^salt;
  if(id==0xFFFFFFFFUL) id^=0x01000000UL;
  expected[i]=id;
  wr(io,r,port,0x00110000UL);
  wr(io,r,port+4,0x06000000UL|(id&0xFFFFFFUL));
  wr(io,r,port+4,0x07100000UL|((id>>24)<<8));
  if(!idle(io,r)) return 0;
  observed[i]=rd(io,0x494);
  if(observed[i]==id) *flags|=1UL<<i;
 }
 if(!(*flags&3)) return -1;
 port=(*flags&1)?0x41C:0x43C;
 wr(io,r,port,0x00110000UL);
 wr(io,r,port+4,0x06000000UL|(r->cpu_original&0xFFFFFFUL));
 wr(io,r,port+4,0x07100000UL|((r->cpu_original>>24)<<8));
 if(!idle(io,r)) return 0;
 *restored=rd(io,0x494);
 if(*restored!=r->cpu_original) return -1;
 *flags|=4;
 return 1;
}
static int sf_cpu_controls(const vx3d_io *io,vx3d_report *r) {
 uint32_t row,port,flags,i;
 int state;
 r->cpu_selected=0xFFFFFFFFUL;
 r->cpu_original=rd(io,0x494);
 state=sf_cpu_pair(io,r,r->cpu_expected,r->cpu_observed,&r->cpu_restored,
  &r->cpu_flags,0);
 if(!state) return 0;
 if(state==1) {
  r->cpu_selected=0;
  if(!(r->cpu_flags&1)) r->path_flags&=~1UL;
  return 1;
 }
 if(r->cpu_original==0xFFFFFFFFUL) return -1;
 /* A failed bare fence is not proof that an uninitialized CR is broken.
  * Try the exact VX855 path in OpenChrome viaInitPCIe()/viaDisableVQ():
  * no trigger, queue allocation, translation change or error-bit clear. */
 for(row=0;row<2;row++) {
  if(!idle(io,r)) return 0;
  r->cpu_retry_flags[row]=1;
  if(!row) {
   wr(io,r,0x41C,0x00100000UL);
   wr(io,r,0x420,0x680A0000UL);
   wr(io,r,0x420,0x02000000UL);
   wr(io,r,0x420,0x74301000UL);
  } else {
   /* Earlier v6 initialized these controls before its successful CPU
    * fences. Try both documented CPU ports; 43C also wakes 3D clocks.
    * A74 keeps the OpenChrome disabled thresholds, never the old zero. */
   for(i=0;i<2;i++) {
    port=i?0x43C:0x41C;
    wr(io,r,port,0x00100000UL);
    wr(io,r,port+4,0x74301000UL); wr(io,r,port+4,0x7C000000UL);
    wr(io,r,port+4,0); wr(io,r,port+4,0x02000000UL);
    wr(io,r,port+4,0x07000000UL); wr(io,r,port+4,0x680A0000UL);
    wr(io,r,port,0x00110000UL); wr(io,r,port+4,0x00000248UL);
   }
  }
  if(!idle(io,r)) return 0;
  flags=0;
  state=sf_cpu_pair(io,r,r->cpu_retry_expected[row],r->cpu_retry_observed[row],
   &r->cpu_retry_restored[row],&flags,(row+1)*0x01010100UL);
  r->cpu_retry_flags[row]|=flags<<1;
  r->cpu_retry_mmio[row][0]=rd(io,0x400);
  r->cpu_retry_mmio[row][1]=rd(io,0x43C);
  r->cpu_retry_mmio[row][2]=rd(io,0x444);
  r->cpu_retry_mmio[row][3]=rd(io,0x494);
  if(!state) return 0;
  if(state==1) {
   r->cpu_selected=row+1;
   if(!(flags&1)) r->path_flags&=~1UL;
   return 1;
  }
 }
 return -1;
}
static int sf_flush_idle(const vx3d_io *io,const vx3d_sf_io *sf,vx3d_report *r) {
 uint32_t i,start=io->milliseconds(io->context);
 for(i=0;i<VX3D_POLL_LIMIT;i++) {
  r->sf_flush_polls++;
  if(!(sf->read_seq(sf->context,0x6F)&0x80)) return 1;
  if((uint32_t)(io->milliseconds(io->context)-start)>=VX3D_TIMEOUT_MS) break;
 }
 r->sf_failure_phase=r->sf_phase;
 return 0;
}
static int sf_flush(const vx3d_io *io,const vx3d_sf_io *sf,vx3d_report *r) {
 uint32_t value;
 if(!sf_flush_idle(io,sf,r)) return 0;
 value=sf->read_seq(sf->context,0x6F);
 sf->write_seq(sf->context,0x6F,value|0x80);
 return sf_flush_idle(io,sf,r);
}
/* 1=PAUSE plus expected fence; -1=clean PAUSE but missing fence;
 * 0=busy/unreadable timeout; -2=reader state/address not confirmed.
 * At PAUSE the CR busy bit may remain set; every other engine must settle. */
static int sf_wait(const vx3d_io *io,vx3d_report *r,uint32_t row,int retire) {
 uint32_t i,start=io->milliseconds(io->context),stable=0,s=0,f=0,id=0,cursor=0;
 uint32_t mask=retire?VX3D_BUSY_MASK:(VX3D_BUSY_MASK&~0x10UL);
 int ready=0;
 uint32_t pause=row<5?r->sf_pause[row]:row==5?r->sf_alt_pause:
  row==6?r->sf_branch_pause:row<9?r->sf_point_pause[row-7]:r->sf_matrix[row-9].pause;
 uint32_t expected=row<5?r->sf_expected[row]:row==5?r->sf_alt_expected:
  row==6?r->sf_branch_expected:0;
 for(i=0;i<VX3D_POLL_LIMIT;i++) {
  s=rd(io,0x400); status(r,s); r->polls++;
  f=rd(io,0x444); id=rd(io,0x494); cursor=rd(io,0x43C);
  if(s==0xFFFFFFFFUL || f==0xFFFFFFFFUL || id==0xFFFFFFFFUL || cursor==0xFFFFFFFFUL) {
   r->wait_failure_reason=3; r->wait_failure_status=s; break;
  }
  ready=!(s&mask) && (retire?!(f&0x80000000UL):!!(f&0x80000000UL));
  if(!retire && (cursor<pause || cursor>=SF_RING_BASE+VX3D_SF_RING_BYTES)) ready=0;
  /* PAUSE/quiet state is independent of optional T11 fence acceptance.
   * After it settles, each work phase checks register/target side effects. */
  if(ready) {
   if(++stable==32) break;
  } else stable=0;
  if((uint32_t)(io->milliseconds(io->context)-start)>=VX3D_TIMEOUT_MS) break;
 }
 sf_snapshot(io,r,row<5?r->sf_after[row]:row==5?r->sf_alt_after:
  row==6?r->sf_branch_after:row<9?r->sf_point_after[row-7]:r->sf_matrix[row-9].after);
 if(row<5) r->sf_observed[row]=id;
 else if(row==5) r->sf_alt_observed=id;
 else if(row==6) r->sf_branch_observed=id;
 if(stable==32) return (retire || row==0 || id==expected)?1:-1;
 r->sf_failure_phase=r->sf_phase;
 if(r->wait_failure_reason==3 || (s&mask)) {
  if(!r->wait_failure_reason) r->wait_failure_reason=i==VX3D_POLL_LIMIT?1:2;
  r->wait_failure_status=s; return 0;
 }
 /* v10.330 (review #114): ready at the time limit with fewer than 32 settled reads is not a confirmed pause: -2 (unconfirmed), not
  * -1 (which means a settled pause whose fence id differed, and lets the run go on) */
 return -2;
}
static void sf_pause_update(const vx3d_io *io,vx3d_report *r,uint32_t address,uint32_t id) {
 type(io,r,0x10); reg(io,r,0x64,address&0xFFFFFFUL);
 reg(io,r,0x65,(address>>24)|id);
}
static int sf_check(const vx3d_io *io,const vx3d_sf_io *sf,vx3d_report *r) {
 uint32_t at,want,got;
 sf->publish(sf->context);
 for(at=0;at<VX3D_SF_BYTES;at+=4) {
  if(at>=4096 && at<4096+VX3D_SF_RING_BYTES) want=sf_ring_word(r,(at-4096)/4);
  else if(at>=18*4096UL) want=sf_dummy(at/4);
  else want=VX3D_CANARY;
  got=sf->read_host(sf->context,at);
  if(got!=want) {
   if(at>=4096 && at<4096+VX3D_SF_RING_BYTES) r->sf_source_errors++;
   else r->sf_guard_errors++;
  }
 }
 for(at=VX3D_SF_TABLE_BEGIN;at<VX3D_SF_TABLE_END;at+=4) {
  want=at>=VX3D_SF_GART && at<VX3D_SF_GART+VX3D_SF_TABLE_BYTES?
   sf_pte(r,(at-(uint32_t)VX3D_SF_GART)/4):VX3D_CANARY;
  if(io->read_fb(io->context,at)!=want) {
   if(at>=VX3D_SF_GART && at<VX3D_SF_GART+VX3D_SF_TABLE_BYTES) r->sf_table_errors++;
   else r->sf_guard_errors++;
  }
 }
 /* The same numeric address in local VRAM contains dummy commands only.
  * Its fence-free shadow makes an SL/SF alias unable to fake acceptance. */
 for(at=VX3D_QUEUE_REGION;at<VX3D_QUEUE_REGION_END;at+=4) {
  /* The queue is allowed to overwrite its separately owned 64 KiB.
   * Never exempt its neighboring guards or the local ring shadow. */
  if((r->sf_queue_compare_flags&2) && at>=SF_AGP_QUEUE &&
     at<SF_AGP_QUEUE+VX3D_QUEUE_BYTES) {
   if(io->read_fb(io->context,at)!=VX3D_CANARY) r->queue_changed_words++;
   continue;
  }
  want=at>=VX3D_QUEUE && at<VX3D_QUEUE+VX3D_QUEUE_BYTES?
   sf_dummy((at-(uint32_t)VX3D_QUEUE)/4):VX3D_CANARY;
  if(io->read_fb(io->context,at)!=want) r->sf_guard_errors++;
 }
 r->sf_flags|=(r->sf_flags&16384)?0x180000UL:0x200000UL;
 r->guard_errors+=r->sf_source_errors+r->sf_guard_errors+r->sf_table_errors;
 return !r->guard_errors;
}
static void sf_measure_draw(const vx3d_io *io,vx3d_report *r) {
 uint32_t i;
 /* Read-only target evidence is useful even on an unconfirmed pause.
  * The caller marks that case observational and cannot return PASS. */
 r->solid_sample=pixel(io,16,16); r->solid_errors=solid_check(io,0xE04020);
 r->changed_pixels=0;
 for(i=0;i<VX3D_PIXELS;i++) {
  r->image[i]=io->read_fb(io->context,VX3D_TARGET+i*4);
  if(r->image[i]!=VX3D_BACKGROUND) r->changed_pixels++;
 }
 if(r->changed_pixels<1000 || r->changed_pixels>1300) r->solid_errors++;
 r->guard_errors+=guards(io,r);
}
static int sf_run_point(const vx3d_io *io,vx3d_report *r,uint32_t which) {
 uint32_t x,y,got,want; int state;
 clear(io);
 if(!sf_preload_state(io,r)) return 0;
 type(io,r,4); reg(io,r,0x23,0); reg(io,r,0x24,1);
 /* OGPM pp162: point width/height use IEEE float truncated to s[8].15.
  * FVF has no point-size attribute, so both defaults are used directly. */
 reg(io,r,0x2A,0x418000UL); reg(io,r,0x2B,0x418000UL);
 r->sf_point_flags[which]|=32;
 if(!idle(io,r)) return 0;
 r->sf_phase=13+which; r->phase=5;
 sf_snapshot(io,r,r->sf_point_before[which]);
 sf_pause_update(io,r,r->sf_point_pause[which],0); r->sf_point_flags[which]|=2;
 state=sf_wait(io,r,7+which,0);
 r->sf_point_sample[which]=pixel(io,16,16);
 for(y=0;y<VX3D_SIDE;y++) for(x=0;x<VX3D_SIDE;x++) {
  got=pixel(io,x,y);
  want=(x>=12 && x<28 && y>=12 && y<28)?0xFFE04020UL:VX3D_BACKGROUND;
  if(got!=want) r->sf_point_errors[which]++;
  if(got!=VX3D_BACKGROUND) r->sf_point_changed[which]++;
 }
 r->guard_errors+=guards(io,r);
 if(!state || state==-2) {r->sf_point_flags[which]|=16; return state;}
 r->sf_point_flags[which]|=4;
 r->sf_point_latch[which]=rd(io,0x58);
 if(r->sf_point_latch[which]==r->sf_draw_marker_expected+2+which) r->sf_point_flags[which]|=64;
 if(!r->sf_point_errors[which] && r->sf_point_changed[which]==256 && !r->guard_errors)
  r->sf_point_flags[which]|=8;
 return 1;
}
/* Brute-force reference: pixel centre inside the triangle with top-left ties. */
/* v9.17: exact model of the hardware coverage rule found from the v9.15/v9.16
 * maps: sample point = integer (x,y); left/top edges count, right/bottom do not. */
static int sf_ref_tl(const int32_t *g,int32_t x,int32_t y) {
 int32_t px[3],py[3],area,t,i,e,dx,dy; 
 px[0]=g[0];py[0]=g[1];px[1]=g[2];py[1]=g[3];px[2]=g[4];py[2]=g[5];
 area=(px[1]-px[0])*(py[2]-py[0])-(py[1]-py[0])*(px[2]-px[0]);
 if(!area) return 0;
 if(area<0){t=px[1];px[1]=px[2];px[2]=t;t=py[1];py[1]=py[2];py[2]=t;}
 for(i=0;i<3;i++) {
  int32_t n=(i+1)%3;
  dx=px[n]-px[i];dy=py[n]-py[i];
  e=dx*(y-py[i])-dy*(x-px[i]);
  if(e>0) continue;
  if(e==0 && ((dy==0 && dx>0) || dy<0)) continue;
  return 0;
 }
 return 1;
}
static int sf_ref_tri(const int32_t *g,int32_t x,int32_t y) {
 int32_t X[3],Y[3],t,e,b,ar,v,dx,dy; 
 X[0]=2*g[0];Y[0]=2*g[1];X[1]=2*g[2];Y[1]=2*g[3];X[2]=2*g[4];Y[2]=2*g[5];
 ar=(X[1]-X[0])*(Y[2]-Y[0])-(Y[1]-Y[0])*(X[2]-X[0]);
 if(!ar) return 0;
 if(ar<0) {t=X[1];X[1]=X[2];X[2]=t;t=Y[1];Y[1]=Y[2];Y[2]=t;}
 for(e=0;e<3;e++) {
  b=(e+1)%3; dx=X[b]-X[e]; dy=Y[b]-Y[e];
  v=dx*(2*y+1-Y[e])-dy*(2*x+1-X[e]);
  if(v<0) return 0;
  if(v==0 && !((dy==0 && dx>0) || dy<0)) return 0;
 }
 return 1;
}
/* v10.08: distinct-value counter. The old code looped u<nuniq over an 8-entry stack array while nuniq kept counting
 * past 8, reading beyond the stack and page-faulting (Windows fatal exception 0E in VX85598 at sf_gr_measure). */
static uint32_t sf_uniq[4096];
static void sf_uniq_add(uint32_t *n,uint32_t v) {
 uint32_t u;
 for(u=0;u<*n;u++) if(sf_uniq[u]==v) return;
 if(*n<4096UL) sf_uniq[(*n)++]=v;
}
/* v9.50: decode a 16bpp pixel to components (order R,G,B,A) per RT format code. */
static void sf_rt_decode(uint32_t fmt,uint32_t px,uint32_t *c,uint32_t *nb) {
 switch(fmt) {
 case 0x88: nb[0]=nb[1]=nb[2]=5;nb[3]=0; c[0]=(px>>10)&31;c[1]=(px>>5)&31;c[2]=px&31;c[3]=0; break;
 case 0x8A: nb[0]=nb[1]=nb[2]=5;nb[3]=1; c[0]=(px>>10)&31;c[1]=(px>>5)&31;c[2]=px&31;c[3]=(px>>15)&1; break;
 case 0x8B: nb[0]=nb[1]=nb[2]=nb[3]=4; c[0]=(px>>8)&15;c[1]=(px>>4)&15;c[2]=px&15;c[3]=(px>>12)&15; break;
 /* v10.09: 0x8D is RGB565 pixels (no red/blue swap), decoded by the default case; it also enables 2x2 ordered dither */
 default:   nb[0]=5;nb[1]=6;nb[2]=5;nb[3]=0; c[0]=(px>>11)&31;c[1]=(px>>5)&63;c[2]=px&31;c[3]=0; break;
 }
}
static uint32_t sf_rt_expand(uint32_t v,uint32_t n) {return n==0?0:n==1?(v?255UL:0UL):(n==4?v*17UL:n==5?(v<<3)|(v>>2):(v<<2)|(v>>4));}
/* Exact analysis of a 16bpp render-target case; restores the desktop window. */
static int sf_rt_measure(const vx3d_io *io,vx3d_report *r,uint32_t which,vx3d_case_report *c,const int32_t *g,const int32_t *win) {
 const sf_rt_t *t=&sf_rt_tests[g[4]];
 uint32_t oy,ox,k,nuniq=0,uniq[4]={0,0,0,0},nflat=0,nA=0,nB=0,nC=0,nbad=0,outchg=0,guardchg=0,first=0,havefirst=0,ndist=0;
 uint32_t cand[3][4],ex[4],nb[4],dc[4],dd[4];
 uint32_t *raw=r->sf_raw[g[4]];
 uint32_t cmp[4];
 sf_rt_decode(t->fmt,0,cmp,nb);
 for(k=0;k<4;k++) {
  uint32_t max=(1UL<<nb[k])-1,v8=(t->m[k]*255UL+500UL)/1000UL;
  cand[0][k]=nb[k]?(t->m[k]*max)/1000UL:0;           /* A: trunc(f*max)           */
  cand[1][k]=nb[k]?(t->m[k]*max+500UL)/1000UL:0;     /* B: round(f*max)           */
  cand[2][k]=nb[k]?(nb[k]==1?(v8>=128?1UL:0UL):(v8>>(8-nb[k]))):0; /* C: 8-bit then >> */
 }
 if(t->blend) {
  sf_rt_decode(t->fmt,t->dst,dc,nb);
  for(k=0;k<3;k++) {
   uint32_t s8=(t->m[k]*255UL+500UL)/1000UL,d8=sf_rt_expand(dc[k],nb[k]),a=(t->m[3]*256UL)/1000UL,o8;
   o8=(s8*a+d8*(256-a))>>8;
   ex[k]=nb[k]==1?(o8>=128):(o8>>(8-nb[k]));
   cand[0][k]=ex[k]; cand[1][k]=nb[k]?((o8*((1UL<<nb[k])-1)+127UL)/255UL):0; cand[2][k]=ex[k];
  }
 }
 for(oy=0;oy<64;oy++) for(ox=0;ox<64;ox++) {
  uint32_t dw=io->read_fb(io->context,(uint32_t)(win[1]+(int32_t)oy)*SF_DESK_PITCH+(uint32_t)(win[0]+(int32_t)ox)*4);
  if(ox>=32) {
   if(dw!=0xA5A5A5A5UL) {guardchg++;c->flags|=128;}
   continue;
  }
  {
   uint32_t h;
   for(h=0;h<2;h++) {
    uint32_t px=h?(dw>>16):(dw&0xFFFFUL),x=ox*2+h,inside=(int32_t)x>=g[0] && (int32_t)x<g[2] && (int32_t)oy>=g[1] && (int32_t)oy<g[3];
    if(oy==20 && x>=8 && x<24) {if(h) raw[(x-8)/2]|=(px<<16); else raw[(x-8)/2]=px;}
    if(px!=t->dst) {c->changed++;c->flags|=128;}
    if(!inside) {
     if(px!=t->dst) {
      uint32_t kk=r->sf_miss[which][0];
      outchg++;c->errors++;
      if(kk<32) r->sf_miss[which][1+kk]=(oy<<16)|x|0x80000000UL;
      r->sf_miss[which][0]=kk+1;
     }
     continue;
    }
    if(!havefirst) {first=px;havefirst=1;c->sample=px;}
    if(px==first) nflat++;
    {uint32_t u,found=0;
     for(u=0;u<nuniq;u++) if(uniq[u]==px) found=1;
     if(!found && nuniq<4) uniq[nuniq++]=px;
     sf_uniq_add(&ndist,px);} /* v10.330 (review #180): raw[14] the full distinct count, uniq[] the first four */
    {
     uint32_t cc[4],okA=1,okB=1,okC=1,okT=1;
     sf_rt_decode(t->fmt,px,cc,nb);
     for(k=0;k<(t->blend?3UL:4UL);k++) {
      uint32_t d;
      if(!nb[k]) continue;
      d=cc[k]>cand[0][k]?cc[k]-cand[0][k]:cand[0][k]-cc[k]; if(d) okA=0;
      d=cc[k]>cand[1][k]?cc[k]-cand[1][k]:cand[1][k]-cc[k]; if(d) okB=0;
      d=cc[k]>cand[2][k]?cc[k]-cand[2][k]:cand[2][k]-cc[k]; if(d) okC=0;
      /* v10.330 (review #202): one step of tolerance, none for a one-bit channel (ARGB1555's alpha: one step is every wrong value) */
      d=cc[k]>cand[1][k]?cc[k]-cand[1][k]:cand[1][k]-cc[k]; if(d>(nb[k]==1?0UL:1UL)) okT=0;
      d=cc[k]>cand[0][k]?cc[k]-cand[0][k]:cand[0][k]-cc[k]; if(d>(nb[k]==1?0UL:1UL)) okT=0;
     }
     if(t->fmt==0x8D && vx3d_x_select==10) {
      /* v10.09: format 0x8D = RGB565 with 2x2 ordered dither, proved from the V10 table-mode pixels (79F7 8217 / 8238 7A17).
       * out = floor(f*max + T/2^(8-bits)), T = {{0,4},{6,2}}[y&1][x&1] in 8-bit LSB units; exact match required. */
      static const uint32_t bay[2][2]={{0,4},{6,2}};
      uint32_t tt=bay[oy&1][x&1];
      okT=1;
      for(k=0;k<3;k++) {
       uint32_t mx=(1UL<<nb[k])-1,e=(t->m[k]*mx+(tt*1000UL)/(1UL<<(8-nb[k])))/1000UL;
       if(cc[k]!=e) okT=0;
      }
     }
     if(okA) nA++; if(okB) nB++; if(okC) nC++;
     if(okT) c->orange++; else {
      uint32_t kk=r->sf_miss[which][0];
      nbad++;c->errors++;
      if(kk<32) r->sf_miss[which][1+kk]=(oy<<16)|x;
      r->sf_miss[which][0]=kk+1;
     }
    }
   }
  }
 }
 /* raw layout: [0..7] row 20 pixel pairs x=8..23; [8] fmt reg; [9] colour millis RGB(10 bits each)|A; [10] dest pixel;
  * [11] cand A px (R,G,B,A packed in comps); [12] cand B; [13] cand C; [14] unique inside values; [15] pixels equal to first;
  * [16] match A count; [17] match B; [18] match C; [19] first inside pixel; [20..23] unique values; [24] outside-rect changed;
  * [25] guard (right half) changed dwords; [26] tolerance failures; [27] inside pixels expected (48*48). */
 raw[8]=t->fmt; raw[9]=t->m[0]|(t->m[1]<<10)|(t->m[2]<<20); raw[10]=t->dst;
 for(k=0;k<3;k++) raw[11+k]=(cand[k][0]<<24)|(cand[k][1]<<16)|(cand[k][2]<<8)|cand[k][3];
 raw[14]=ndist; raw[15]=nflat; raw[16]=nA; raw[17]=nB; raw[18]=nC; raw[19]=first;
 for(k=0;k<4;k++) raw[20+k]=uniq[k];
 raw[24]=outchg; raw[25]=guardchg; raw[26]=nbad; raw[27]=48UL*48UL;
 (void)ex;(void)dd;
 for(oy=0;oy<64;oy++) for(ox=0;ox<64;ox++)
  io->write_fb(io->context,(uint32_t)(win[1]+(int32_t)oy)*SF_DESK_PITCH+(uint32_t)(win[0]+(int32_t)ox)*4,sf_desk_save[oy*64+ox]);
 (void)io->read_fb(io->context,(uint32_t)(win[1]+63)*SF_DESK_PITCH);
 if(guardchg) c->errors++;
 r->sf_matrix_observed|=1UL<<which;r->guard_errors+=guards(io,r);
 return !c->errors && !r->guard_errors;
}
/* v9.53: Gouraud proof analysis. Interior pixels (all barycentric weights > 0.05) must match the interpolated colour within the format
 * step (+3); pixels clearly outside (weight < -0.05) must be untouched. Records probes and error statistics. */
static int sf_gr_measure(const vx3d_io *io,vx3d_report *r,uint32_t which,vx3d_case_report *c,const int32_t *g,const int32_t *win) {
 const sf_gr_t *q=&sf_gr_tests[g[4]];
 uint32_t oy,ox,k,checked=0,okc=0,outbad=0,maxe=0,sume=0,nuniq=0;
 uint32_t *raw=r->sf_raw[g[4]];
 uint32_t nb[4],cc[4],step;
 sf_rt_decode(q->fmt,0,cc,nb);
 for(oy=0;oy<64;oy++) for(ox=0;ox<64;ox++) {
  uint32_t px,got,exp8[3],st,h;
  int32_t wg1000,wb1000,wr1000,mn;
  uint32_t dw=io->read_fb(io->context,(uint32_t)(win[1]+(int32_t)oy)*SF_DESK_PITCH+(uint32_t)(win[0]+(int32_t)ox)*4);
  if(q->fmt==0x99) {px=ox;got=dw;}
  else {
   if(ox>=32) {if(dw!=0xA5A5A5A5UL) {c->errors++;c->flags|=128;} continue;}
   for(h=0;h<2;h++) { /* handled below via loop on pixel x */ }
   px=0;got=dw;
  }
  if(q->fmt==0x99) {
   int32_t x=(int32_t)ox;
   wg1000=((x-8)*1000)/48; wb1000=(((int32_t)oy-8)*1000)/48; wr1000=1000-wg1000-wb1000;
   mn=wg1000<wb1000?wg1000:wb1000; if(wr1000<mn) mn=wr1000;
   if(got!=0xA5A5A5A5UL) {c->changed++;c->flags|=128;}
   if(mn<-50) {if(got!=0xA5A5A5A5UL) outbad++; continue;}
   if(mn<=50) continue;
   exp8[0]=(uint32_t)(wr1000*255/1000);exp8[1]=(uint32_t)(wg1000*255/1000);exp8[2]=(uint32_t)(wb1000*255/1000);
   {uint32_t e,m=0,gg[3];
    gg[0]=(got>>16)&0xFF;gg[1]=(got>>8)&0xFF;gg[2]=got&0xFF;
    for(k=0;k<3;k++) {e=gg[k]>exp8[k]?gg[k]-exp8[k]:exp8[k]-gg[k]; if(e>m) m=e; sume+=e;}
    checked++; if(m<=4) {okc++;c->orange++;}
    else {uint32_t kk=r->sf_miss[which][0]; c->errors++; if(kk<32) r->sf_miss[which][1+kk]=(oy<<16)|ox; r->sf_miss[which][0]=kk+1;}
    if(m>maxe) maxe=m;
   }
   if(!c->sample) c->sample=got;
   if((ox==12&&oy==12)||(ox==44&&oy==12)||(ox==12&&oy==44)||(ox==24&&oy==24)) {
    uint32_t pi=(ox==12&&oy==12)?0:(ox==44)?1:(oy==44)?2:3;
    raw[8+pi]=got; raw[12+pi]=(exp8[0]<<16)|(exp8[1]<<8)|exp8[2];
   }
   sf_uniq_add(&nuniq,got);
   continue;
  }
  for(h=0;h<2;h++) {
   int32_t x=(int32_t)(ox*2+h);
   uint32_t pxv=h?(dw>>16):(dw&0xFFFFUL),gg[4],e,m=0,i2;
   wg1000=((x-8)*1000)/48; wb1000=(((int32_t)oy-8)*1000)/48; wr1000=1000-wg1000-wb1000;
   mn=wg1000<wb1000?wg1000:wb1000; if(wr1000<mn) mn=wr1000;
   if(pxv!=0xA5A5) {c->changed++;c->flags|=128;}
   if(mn<-50) {if(pxv!=0xA5A5) outbad++; continue;}
   if(mn<=50) continue;
   sf_rt_decode(q->fmt,pxv,gg,nb);
   exp8[0]=(uint32_t)(wr1000*255/1000);exp8[1]=(uint32_t)(wg1000*255/1000);exp8[2]=(uint32_t)(wb1000*255/1000);
   step=1UL<<(8-nb[0]); if(nb[1]==6) st=4; else st=step;
   for(i2=0;i2<3;i2++) {
    uint32_t g8=sf_rt_expand(gg[i2],nb[i2]),stp=(i2==1)?st:step,lim=stp+3;
    e=g8>exp8[i2]?g8-exp8[i2]:exp8[i2]-g8; if(e>m && e>lim) m=e; sume+=e; if(e>maxe) maxe=e;
   }
   /* v10.330 (review #207): an empty if/else on the dither bit was here: both modes are checked against the same tolerance */
   checked++; if(!m) {okc++;c->orange++;}
   else {uint32_t kk=r->sf_miss[which][0]; c->errors++; if(kk<32) r->sf_miss[which][1+kk]=(oy<<16)|(uint32_t)x; r->sf_miss[which][0]=kk+1;}
   if(!c->sample) c->sample=pxv;
   if(((uint32_t)x==12&&oy==12)||((uint32_t)x==44&&oy==12)||((uint32_t)x==12&&oy==44)||((uint32_t)x==24&&oy==24)) {
    uint32_t pi=((uint32_t)x==12&&oy==12)?0:((uint32_t)x==44)?1:((uint32_t)x==12&&oy==44)?2:3;
    raw[8+pi]=pxv; raw[12+pi]=(exp8[0]<<16)|(exp8[1]<<8)|exp8[2];
   }
   sf_uniq_add(&nuniq,pxv);
  }
 }
 /* raw: [0] fmt code [1] 01/53 value [2] interior pixels checked [3] within tolerance [4] outside pixels changed [5] max error
  * [6] mean error x100 [7] distinct interior values (>8 counted); [8..11] got at (12,12),(44,12),(12,44),(24,24); [12..15] expected 0x00RRGGBB */
 raw[0]=q->fmt; raw[1]=q->mode53; raw[2]=checked; raw[3]=okc; raw[4]=outbad; raw[5]=maxe; raw[6]=checked?(sume*100UL)/(checked*3UL):0; raw[7]=nuniq;
 if(outbad) c->errors++;
 for(oy=0;oy<64;oy++) for(ox=0;ox<64;ox++)
  io->write_fb(io->context,(uint32_t)(win[1]+(int32_t)oy)*SF_DESK_PITCH+(uint32_t)(win[0]+(int32_t)ox)*4,sf_desk_save[oy*64+ox]);
 (void)io->read_fb(io->context,(uint32_t)(win[1]+63)*SF_DESK_PITCH);
 if(!checked) c->errors++;
 r->sf_matrix_observed|=1UL<<which;r->guard_errors+=guards(io,r);
 return !c->errors && !r->guard_errors;
}
/* v9.55: fog analysis. raw: [0] 01/70 value [1] fog enable [2] interior pixels checked [3] within tolerance (+-4) [4] outside pixels changed
 * [5] max error [6] mean error x100 [7] distinct interior values; [8..11] got at (12,12),(44,12),(12,44),(24,24); [12..15] expected 0x00RRGGBB;
 * [16..18] fog colour mapping: got at (50,10) (z~.7); [19] got at (10,50). */
static int sf_fog_measure(const vx3d_io *io,vx3d_report *r,uint32_t which,vx3d_case_report *c,const int32_t *g,const int32_t *win) {
 const sf_fog_t *ft=&sf_fog_tests[g[4]];
 uint32_t oy,ox,k,checked=0,okc=0,outbad=0,maxe=0,sume=0,nuniq=0;
 uint32_t *raw=r->sf_raw[g[4]];
 for(oy=0;oy<64;oy++) for(ox=0;ox<64;ox++) {
  int32_t x=(int32_t)ox,y=(int32_t)oy,wg1000,wb1000,wr1000,mn,z1000,f1000;
  uint32_t got=io->read_fb(io->context,(uint32_t)(win[1]+y)*SF_DESK_PITCH+(uint32_t)(win[0]+x)*4);
  uint32_t exp8[3],gg[3],m=0,e;
  wg1000=((x-8)*1000)/48; wb1000=((y-8)*1000)/48; wr1000=1000-wg1000-wb1000;
  mn=wg1000<wb1000?wg1000:wb1000; if(wr1000<mn) mn=wr1000;
  if(got!=0xA5A5A5A5UL) {c->changed++;c->flags|=128;}
  if(ox==10&&oy==50) raw[19]=got;
  if(ox==50&&oy==10) raw[16]=got;
  if(mn<-50) {if(got!=0xA5A5A5A5UL) outbad++; continue;}
  if(mn<=50) continue;
  z1000=250+(500*wg1000)/1000+(250*wb1000)/1000;
  if(ft->en && ft->mode70&2) {
   int32_t den=(int32_t)ft->end_m-(int32_t)ft->start_m;
   f1000=(((int32_t)ft->end_m-z1000)*1000)/den; if(f1000<0) f1000=0; if(f1000>1000) f1000=1000;
   if(ft->mode70&8) f1000=1000-f1000;
   if(!(ft->mode70&0x10)) f1000=0; /* W-based: report only */
  } else f1000=1000;
  for(k=0;k<3;k++) {
   uint32_t cv=(ft->c[k]*f1000+ft->f[k]*(1000-f1000))/1000;
   exp8[k]=(cv*255UL)/1000UL;
  }
  gg[0]=(got>>16)&0xFF;gg[1]=(got>>8)&0xFF;gg[2]=got&0xFF;
  for(k=0;k<3;k++) {e=gg[k]>exp8[k]?gg[k]-exp8[k]:exp8[k]-gg[k]; if(e>m) m=e; sume+=e;}
  checked++; if(m>maxe) maxe=m;
  if(m<=6) {okc++;c->orange++;} /* v10.08: tolerance +-6; hardware deviates by up to 5 (mean ~1) from the integer model (fog start .5/end 1) */
  else if(!ft->report_only) {uint32_t kk=r->sf_miss[which][0]; c->errors++; if(kk<32) r->sf_miss[which][1+kk]=((uint32_t)y<<16)|(uint32_t)x; r->sf_miss[which][0]=kk+1;}
  if(!c->sample) c->sample=got;
  if((x==12&&y==12)||(x==44&&y==12)||(x==12&&y==44)||(x==24&&y==24)) {
   uint32_t pi=(x==12&&y==12)?0:(x==44)?1:(y==44)?2:3;
   raw[8+pi]=got; raw[12+pi]=(exp8[0]<<16)|(exp8[1]<<8)|exp8[2];
  }
  sf_uniq_add(&nuniq,got);
 }
 raw[0]=ft->mode70; raw[1]=ft->en; raw[2]=checked; raw[3]=okc; raw[4]=outbad; raw[5]=maxe; raw[6]=checked?(sume*100UL)/(checked*3UL):0; raw[7]=nuniq;
 if(outbad) c->errors++;
 for(oy=0;oy<64;oy++) for(ox=0;ox<64;ox++)
  io->write_fb(io->context,(uint32_t)(win[1]+(int32_t)oy)*SF_DESK_PITCH+(uint32_t)(win[0]+(int32_t)ox)*4,sf_desk_save[oy*64+ox]);
 (void)io->read_fb(io->context,(uint32_t)(win[1]+63)*SF_DESK_PITCH);
 if(!checked) c->errors++;
 r->sf_matrix_observed|=1UL<<which;r->guard_errors+=guards(io,r);
 return !c->errors && !r->guard_errors;
}
static int sf_matrix_measure(const vx3d_io *io,vx3d_report *r,uint32_t which) {
 static const int32_t triangles[3][6]={{8,8,56,8,8,56},{16,48,48,48,16,16},{12,10,54,20,20,54}};
 uint32_t x,y,got,want,slot,primitive,one_point,shape,edge,inside,outside_pixel;
 uint32_t low,high;int32_t dx,dy,e,margin,orientation;const int32_t *t;
 vx3d_case_report *c=&r->sf_matrix[which];
 sf_hi=(vx3d_x_reach>=2UL && vx3d_x_reach<=3UL && which>=16UL && which<=25UL)?sf_reach_off[which-16UL]:0UL;
 {
  int32_t g[6],win[4]; uint32_t color; int kind=sf_fill_job(which,g,&color,win);
  if(kind) {
   /* Exact per-pixel check against a brute-force reference. Scratch
    * cases cover the whole 64x64 target; desktop cases check only their
    * saved window (expected = fill colour where covered and on screen,
    * else the saved original pixel), then restore it. */
   uint32_t col=sf_fill_pix[color],ox,oy,ww,wh,orig;
   if(win[2] && kind==9) return sf_rt_measure(io,r,which,c,g,win);
   if(win[2] && kind==10 && vx3d_x_select==18) return sf_fog_measure(io,r,which,c,g,win);
   if(win[2] && kind==10) return sf_gr_measure(io,r,which,c,g,win);
   if(win[2]) {
    ww=(uint32_t)win[2]; wh=(uint32_t)win[3];
    for(oy=0;oy<wh;oy++) for(ox=0;ox<ww;ox++) {
     int32_t px=win[0]+(int32_t)ox,py=win[1]+(int32_t)oy;
     got=io->read_fb(io->context,(uint32_t)py*SF_DESK_PITCH+(uint32_t)px*4);
     orig=sf_desk_save[oy*ww+ox];
     if(kind==8) {
      const sf_tx_t *tt=&sf_tx_tests[g[4]];
      int32_t sx=win[0]+(int32_t)ox,sy=win[1]+(int32_t)oy;
      uint32_t wv,dd,tol=tt->filter?2:(tt->blend?3:1);
      static int32_t bax,bay;
      static uint32_t bsc,sc00,sc22,nsamp;
      orig=sf_2d_pre((int32_t)ox,(int32_t)oy);
      if(ox==0 && oy==0) {
       int32_t ax,ay; uint32_t px2,py2,sc;
       bsc=0;bax=0;bay=0;sc00=0;sc22=0;nsamp=0;
       {static const int8_t gx[9]={0,-2,0,2,-2,2,-2,0,2},gy[9]={0,-2,-2,-2,0,0,2,2,2};
        uint32_t gi;
        for(gi=0;gi<9;gi++) { /* v9.48: (0,0) first so ties keep zero offset */
         ax=gx[gi]; ay=gy[gi];
         sc=0;nsamp=0;
         for(py2=1;py2<wh;py2+=3) for(px2=1;px2<ww;px2+=3) {
          uint32_t gt=io->read_fb(io->context,(uint32_t)(win[1]+(int32_t)py2)*SF_DESK_PITCH+(uint32_t)(win[0]+(int32_t)px2)*4);
          uint32_t md=sf_tx_final(tt,sf_tx_modelr(tt,win[0]+(int32_t)px2,win[1]+(int32_t)py2,ax,ay),sf_2d_pre((int32_t)px2,(int32_t)py2));
          nsamp++;
          if(sf_tx_diff(gt,md)<=tol) sc++;
         }
         if(ax==0&&ay==0) sc00=sc;
         if(ax==2&&ay==2) sc22=sc;
         if(sc>bsc) {bsc=sc;bax=ax;bay=ay;}
        }}
       r->sf_raw[g[4]][16]=(uint32_t)(bax&0xFF)|((uint32_t)(bay&0xFF)<<8);
       r->sf_raw[g[4]][17]=bsc; r->sf_raw[g[4]][18]=sc00; r->sf_raw[g[4]][19]=sc22; r->sf_raw[g[4]][20]=nsamp;
      }
      want=sf_tx_final(tt,sf_tx_modelr(tt,sx,sy,bax,bay),orig);
      dd=sf_tx_diff(got,want);
      inside=1;
      if((got&0xFFFFFFUL)!=(orig&0xFFFFFFUL)) {c->changed++;c->flags|=128;}
      {uint32_t m0=sf_tx_final(tt,sf_tx_modelr(tt,sx,sy,0,0),orig);
       uint32_t cl=((got&0xFFFFFFUL)==(orig&0xFFFFFFUL))?0:(sf_tx_diff(got,m0)<=tol)?1:3;
       r->sf_cls[g[4]][(oy*64+ox)/16]|=cl<<(((oy*64+ox)%16)*2);}
      if(dd<=tol) c->orange++;
      if(oy==20 && ox>=20 && ox<28) {
       r->sf_raw[g[4]][ox-20]=got; r->sf_raw[g[4]][8+ox-20]=want;
       r->sf_raw[g[4]][24+ox-20]=sf_tx_final(tt,sf_tx_modelr(tt,sx,sy,0,0),orig);
      }
      if(oy==0 && ox<8) {r->sf_raw[g[4]][32+ox]=got; r->sf_raw[g[4]][40+ox]=want;} /* v9.48: top row (seam) instead of row 40 */
      wv=dd; (void)wv;
      if(dd>tol) {
       uint32_t k=r->sf_miss[which][0];
       c->errors++;
       if(k<32) r->sf_miss[which][1+k]=((uint32_t)sy<<16)|(uint32_t)sx;
       r->sf_miss[which][0]=k+1;
      }
      if(!c->sample) c->sample=got;
      continue;
     }
     if(kind==7) {
      const sf_z_t *zt=SF_ZT(g[4]);
      int32_t sx=win[0]+(int32_t)ox,sy=win[1]+(int32_t)oy;
      uint32_t wv,dd,gg,k3,zexp,zgot;
      orig=sf_2d_pre((int32_t)ox,(int32_t)oy);
      want=(vx3d_x_select==17)?sf_st_model(&sf_st_tests[g[4]],sx,sy,orig,&zexp):sf_z_model(zt,sx,sy,orig,&zexp);
      zgot=io->read_fb(io->context,SF_DEPTHB+(oy*64+ox)*4);
      inside=(want!=orig);
      dd=0; for(k3=0;k3<3;k3++) {gg=((got>>(k3*8))&0xFF); wv=((want>>(k3*8))&0xFF); wv=gg>wv?gg-wv:wv-gg; if(wv>dd) dd=wv;}
      if((got&0xFFFFFFUL)!=(orig&0xFFFFFFUL)) {c->changed++;c->flags|=128;}
      if(inside && dd<=1) c->orange++;
      if((sx==480&&sy==352)||(sx==460&&sy==332)||(sx==500&&sy==372)) {
       uint32_t n=(sx==480)?0:(sx==460)?1:2;
       r->sf_raw[g[4]][n]=zgot; r->sf_raw[g[4]][3+n]=got; r->sf_raw[g[4]][6+n]=want; r->sf_raw[g[4]][9+n]=zexp;
      }
      if(dd>1 || zgot!=zexp) {
       uint32_t k=r->sf_miss[which][0];
       c->errors++;
       if(k<32) r->sf_miss[which][1+k]=((uint32_t)sy<<16)|(uint32_t)sx|(inside?0UL:0x80000000UL);
       r->sf_miss[which][0]=k+1;
      }
      if(inside && !c->sample) c->sample=got;
      continue;
     }
     if(kind==6) {
      const sf_3d_t *t3=&sf_3d_tests[g[4]];
      int32_t sx=win[0]+(int32_t)ox,sy=win[1]+(int32_t)oy;
      uint32_t wv,dd,gg,k3;
      orig=sf_2d_pre((int32_t)ox,(int32_t)oy);
      inside=sx>=g[0] && sx<g[2] && sy>=g[1] && sy<g[3];
      want=inside?sf_3d_expect(t3,orig):orig;
      dd=0; for(k3=0;k3<3;k3++) {gg=((got>>(k3*8))&0xFF); wv=((want>>(k3*8))&0xFF); wv=gg>wv?gg-wv:wv-gg; if(wv>dd) dd=wv;}
      if((got&0xFFFFFFUL)!=(orig&0xFFFFFFUL)) {c->changed++;c->flags|=128;}
      if(inside && dd<=3) c->orange++;
      if(oy==8 && ox>=8 && ox<16) {r->sf_raw[g[4]][ox-8]=got; r->sf_raw[g[4]][8+ox-8]=orig; r->sf_raw[g[4]][16+ox-8]=want;}
      if(dd>3 && !(inside && t3->model==SF3_UNKNOWN)) { /* v10.330 (review #205): an unknown model's inside pixels are observed, not scored */
       uint32_t k=r->sf_miss[which][0];
       c->errors++;
       if(k<32) r->sf_miss[which][1+k]=((uint32_t)sy<<16)|(uint32_t)sx|(inside?0UL:0x80000000UL);
       r->sf_miss[which][0]=k+1;
      }
      if(inside && !c->sample) c->sample=got;
      continue;
     }
     if(kind==5) {
      orig=sf_2d_pre((int32_t)ox,(int32_t)oy);
      want=sf_2d_expect(&sf_2d_ops[g[0]],(int32_t)ox,(int32_t)oy);
      inside=(want!=orig);
      if((which==31||which==27||(which>=16 && which<=25)) && ox>=4 && ox<12 && oy>=4 && oy<12) r->sf_raw[SF2I(which)][(oy-4)*8+(ox-4)]=got;
      if(which==31||which==27||(which>=16 && which<=25)) {
       const sf_2d_op *po=&sf_2d_ops[g[0]];
       uint32_t cl=(got==orig)?0:(got==po->color)?1:(got==po->bg && po->bg)?2:3;
       r->sf_cls[SF2I(which)][(oy*64+ox)/16]|=cl<<(((oy*64+ox)%16)*2);
      }
      if(got!=orig) {c->changed++;c->flags|=128;}
      if(got==want && inside) c->orange++;
     } else {
     if(px<SF_DESK_W && py<SF_DESK_H) {
      if(kind==2||kind==4) inside=px>=g[0] && px<g[2] && py>=g[1] && py<g[3];
      else if(kind==3) inside=sf_ref_tl(g,px,py);
      else inside=sf_ref_tri(g,px,py);
     } else inside=0;
     want=inside?col:orig;
     if(got!=orig) {c->changed++;c->flags|=128;}
     if(got==col && inside) c->orange++;
     }
     if(got!=want) {
      uint32_t k=r->sf_miss[which][0];
      c->errors++;
      if(k<32) r->sf_miss[which][1+k]=((uint32_t)py<<16)|(uint32_t)px|(inside?0UL:0x80000000UL);
      r->sf_miss[which][0]=k+1;
     }
     if(kind==5) {if(got!=orig && !c->sample) c->sample=got;}
     else if(inside && !c->sample) c->sample=got;
    }
    /* put the desktop back exactly as it was */
    for(oy=0;oy<wh;oy++) for(ox=0;ox<ww;ox++)
     io->write_fb(io->context,(uint32_t)(win[1]+(int32_t)oy)*SF_DESK_PITCH+
      (uint32_t)(win[0]+(int32_t)ox)*4,sf_desk_save[oy*ww+ox]);
    (void)io->read_fb(io->context,(uint32_t)(win[1]+win[3]-1)*SF_DESK_PITCH);
    r->sf_matrix_observed|=1UL<<which;r->guard_errors+=guards(io,r);
    return !c->errors && !r->guard_errors;
   }
   for(y=0;y<VX3D_SIDE;y++) for(x=0;x<VX3D_SIDE;x++) {
    got=pixel(io,x,y);
    if(kind==2) inside=(int32_t)x>=g[0] && (int32_t)x<g[2] && (int32_t)y>=g[1] && (int32_t)y<g[3];
    else inside=sf_ref_tri(g,(int32_t)x,(int32_t)y);
    want=inside?col:VX3D_BACKGROUND;
    if(got!=VX3D_BACKGROUND) {c->changed++;c->flags|=128;}
    if(got==col) c->orange++;
    if(got!=want) c->errors++;
   }
   c->sample=pixel(io,which==22?30:12,which==22?26:10);
   r->sf_matrix_observed|=1UL<<which;r->guard_errors+=guards(io,r);
   return !c->errors && !r->guard_errors;
  }
 }
 primitive=c->mode&15;one_point=primitive==0 && c->vertices==1;shape=c->config[4];
 t=triangles[shape<3?shape:0];
 orientation=(t[2]-t[0])*(t[5]-t[1])-(t[3]-t[1])*(t[4]-t[0]);
 orientation=orientation<0?-1:1;
 c->sample=pixel(io,one_point?16:shape==1?24:shape==2?24:16,
  one_point?16:shape==1?32:shape==2?24:16);
 for(y=0;y<VX3D_SIDE;y++) for(x=0;x<VX3D_SIDE;x++) {
  got=pixel(io,x,y);want=VX3D_BACKGROUND;
  if(which<16 && got!=VX3D_BACKGROUND) r->sf_cover[which][(y*64+x)/32]|=1UL<<((y*64+x)%32);
  if(which==9) r->sf_gouraud[y*64+x]=got;
  if(got!=VX3D_BACKGROUND) {c->changed++;c->flags|=128;}
  if(got==c->shader[1])c->orange++;
  if(primitive==0) {
   if(one_point) {if(x>=12 && x<28 && y>=12 && y<28)want=c->shader[1];}
   else if((x<16 && y<16)||(x>=48 && y<16)||(x<16 && y>=48))want=c->shader[1];
   if(got!=want)c->errors++;
  } else {
   inside=1;outside_pixel=0;
   if(shape==3 || primitive==4) {
    inside=x>=10 && x<=54 && y>=10 && y<=54;
    outside_pixel=x<7 || y<7 || x>57 || y>57;
   } else for(edge=0;edge<3;edge++) {
    uint32_t next=(edge+1)%3;
    dx=t[next*2]-t[edge*2];dy=t[next*2+1]-t[edge*2+1];
    e=(dx*((int32_t)y-t[edge*2+1])-dy*((int32_t)x-t[edge*2]))*orientation;
    margin=(dx<0?-dx:dx)+(dy<0?-dy:dy);
    if(e<=2*margin)inside=0;
    if(e<-margin)outside_pixel=1;
   }
   if(got!=VX3D_BACKGROUND && got!=c->shader[1])c->errors++;
   if(inside && got!=c->shader[1])c->errors++;
   if(outside_pixel && got!=VX3D_BACKGROUND)c->errors++;
  }
  /* All cases remain visible on a 4x4 actual-read contact sheet until
   * success. A passed triangle is then captured at full resolution. */
  if(which<16) {
   slot=((which/4)*16+y*16/64)*64+(which%4)*16+x*16/64;
   if(got!=VX3D_BACKGROUND || r->image[slot]==VX3D_BACKGROUND)r->image[slot]=got;
  }
 }
 if(primitive==2 || primitive==4) {
  low=shape==1?450:shape==2?780:shape==3?2200:1000;
  high=shape==1?560:shape==2?980:shape==3?2450:1300;
  if(c->orange<low || c->orange>high || c->orange!=c->changed)c->errors++;
 } else if(c->changed!=(one_point?256:768))c->errors++;
 r->sf_matrix_observed|=1UL<<which;r->guard_errors+=guards(io,r);
 return !c->errors && !r->guard_errors;
}

/* v9.27: isolated 2D variants. They run after the main matrix, cleanup and
 * restoration are complete, so a hang cannot stop any other test. */
/* v10.330 (review #115, #125): 0 when a 2D operation left the engine busy after its reset (the rest are not run and the desktop window is
 * not written: the caller reports a timeout); nothing is saved or written when the selector has no 2D extras */
static int sf_2d_extra(const vx3d_io *io,vx3d_report *r) {
 static const uint8_t list_rot[3]={18,19,20};
 static const uint8_t list_host[6]={12,13,14,15,16,17};
 static const uint8_t list_h19[4]={21,22,13,23};
 static const uint8_t list_h20[5]={8,10,24,25,11};
 static const uint8_t list_h21[8]={0,1,2,3,4,5,6,7}; /* v10.06: mono/colour patterns and four lines (max 8 per request: sf_x_* arrays) */
 static const uint8_t list_h22[4]={8,10,25,9}; /* v10.06: proven key copies, clipped text, frame-buffer pattern */
 const uint8_t *list=vx3d_x_select&&vx3d_x_select<7?&list_host[vx3d_x_select-1]:list_rot;
 uint32_t n=0; /* v9.35: host-data extras removed; v9.57: selector 19 runs the host-data window tests */
 uint32_t k,xx,yy,sw=r->wait_failure_reason,ss=r->wait_failure_status;
 if(vx3d_x_select==19) {list=list_h19; n=4;}
 if(vx3d_x_select==20) {list=list_h20; n=5;}
 if(vx3d_x_select==22) {list=list_h21; n=8;} /* 21 is the 2D lab, so the lists use 22 and 23 */
 if(vx3d_x_select==23) {list=list_h22; n=4;}
 if(!n) return 1;
 for(yy=0;yy<64;yy++) for(xx=0;xx<64;xx++)
  sf_desk_save[yy*64+xx]=io->read_fb(io->context,(uint32_t)(SF_2D_WY+(int32_t)yy)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+(int32_t)xx)*4);
 for(k=0;k<n;k++) {
  const sf_2d_op *o=&sf_2d_ops[list[k]];
  uint32_t changed=0,match=0,errs=0,expected=0;
  for(yy=0;yy<64;yy++) for(xx=0;xx<64;xx++)
   io->write_fb(io->context,(uint32_t)(SF_2D_WY+(int32_t)yy)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+(int32_t)xx)*4,sf_2d_pre((int32_t)xx,(int32_t)yy));
  (void)io->read_fb(io->context,(uint32_t)(SF_2D_WY+63)*SF_DESK_PITCH);
  r->wait_failure_reason=0; r->wait_failure_status=0;
  if(!sf_2d_run(io,r,o,list[k])) {r->sf_x_info[k][0]=list[k]; r->sf_x_info[k][1]=1; r->sf_x_info[k][2]=0; return 0;}
  for(yy=0;yy<64;yy++) for(xx=0;xx<64;xx++) {
   uint32_t got=io->read_fb(io->context,(uint32_t)(SF_2D_WY+(int32_t)yy)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+(int32_t)xx)*4);
   uint32_t orig=sf_2d_pre((int32_t)xx,(int32_t)yy),want=sf_2d_expect(o,(int32_t)xx,(int32_t)yy);
   if(got!=orig) changed++;
   if(want!=orig) expected++;
   if(got==want && want!=orig) match++;
   if(got!=want) errs++;
   if(xx>=4 && xx<36 && yy>=4 && yy<20) {
    uint32_t idx=(yy-4)*32+(xx-4),cl=(got==orig)?0:(got==o->color)?1:(got==o->bg && o->bg)?2:3;
    r->sf_x_raw[k][idx]=got;
    r->sf_x_cls[k][idx/16]|=cl<<((idx%16)*2);
   }
  }
  r->sf_x_info[k][0]=list[k]; r->sf_x_info[k][1]=r->sf_raw[13][list[k]*2]; r->sf_x_info[k][2]=r->sf_raw[13][list[k]*2+1];
  r->sf_x_info[k][3]=changed; r->sf_x_info[k][4]=match; r->sf_x_info[k][5]=errs; r->sf_x_info[k][6]=expected; r->sf_x_info[k][7]=o->cmd;
 }
 for(yy=0;yy<64;yy++) for(xx=0;xx<64;xx++)
  io->write_fb(io->context,(uint32_t)(SF_2D_WY+(int32_t)yy)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+(int32_t)xx)*4,sf_desk_save[yy*64+xx]);
 (void)io->read_fb(io->context,(uint32_t)(SF_2D_WY+63)*SF_DESK_PITCH);
 r->wait_failure_reason=sw; r->wait_failure_status=ss;
 return 1;
}
void vx3d_run_sf(const vx3d_io *io,const vx3d_sf_io *sf,vx3d_report *r,
 uint32_t *backup,uint32_t *queue_backup,uint32_t *table_backup) {
 uint32_t i,at,row,start=io->milliseconds(io->context),saved=0,table_saved=0;
 uint32_t words,vertices,stride,original_result,control_pass=0,launched=0;
 int state,extra_ok;
 sf_seqio=sf;
 profile(r); r->stage=1; r->sf_phase=1;
 r->target_offset=VX3D_TARGET; r->pitch=VX3D_SIDE*4; r->width=VX3D_SIDE; r->height=VX3D_SIDE;
 r->sf_gart_offset=VX3D_SF_GART; r->sf_gart_bytes=VX3D_SF_TABLE_BYTES;
 r->sf_location_word=0x60000001UL|SF_RING_BASE;
 r->dma_base=SF_RING_BASE; r->dma_limit=SF_RING_BASE+VX3D_SF_RING_BYTES;
 /* Preserve translation observations even when the initial CPU controls
  * fail. Those failures never justify blindly clearing inherited settings. */
 sf_seq_snapshot(sf,r->sf_gti_before); r->sf_flags|=4;
 r->sf_memory[0]=sf->read_seq(sf->context,0x68);
 r->sf_memory[1]=sf->read_seq(sf->context,0x6D);
 r->sf_memory[2]=sf->read_seq(sf->context,0x6E);
 if(!idle(io,r)) {r->result=VX3D_BUSY; goto done;}
 state=sf_cpu_controls(io,r);
 if(!state) goto timeout;
 if(state<0) {
  /* No fetch or mapping is armed. Stop before creating fake draw results.
   * There are no GPU references to the newly allocated host pages. */
  r->result=VX3D_CONTROL_UNCONFIRMED; r->cpu_flags|=8;
  r->sf_flags|=VX3D_SF_SAFE_RELEASE; goto done;
 }
 if(!queue_snapshot(io,r)) goto timeout;
 state=queue_claim(io,r);
 if(!state) goto timeout;
 if(state<0) goto control_failure;
 type(io,r,0x10); reg(io,r,0x69,0);
 state=dma_clear_cycle(io,r,0);
 if(!state) goto timeout;
 if(state<0) goto control_failure;
 r->dma_flags|=1;
 for(i=0;i<VX3D_ARENA_BYTES;i+=4) backup[i/4]=io->read_fb(io->context,VX3D_ARENA+i);
 for(i=0;i<VX3D_QUEUE_SPAN;i+=4) queue_backup[i/4]=io->read_fb(io->context,VX3D_QUEUE_REGION+i);
 saved=1; clear(io);
 for(at=VX3D_QUEUE_REGION;at<VX3D_QUEUE_REGION_END;at+=4)
  io->write_fb(io->context,at,at>=VX3D_QUEUE && at<VX3D_QUEUE+VX3D_QUEUE_BYTES?
   sf_dummy((at-(uint32_t)VX3D_QUEUE)/4):VX3D_CANARY);
 if(!queue_program(io,r)) goto timeout;
 r->phase=1;
 if(!blit_test(io,r)) goto timeout;
 r->checks_run|=1; r->guard_errors=guards(io,r);
 if(r->guard_errors || r->blit_errors) {
  r->result=r->guard_errors?VX3D_GUARD_FAILURE:VX3D_BLIT_FAILURE; goto cleanup;
 }
 clear(io); r->stage=2; r->phase=2; setup_global(io,r);
 if(!idle(io,r)) goto timeout;
 r->cpu_after_fe[0]=r->cpu_original^0x6FE0A003UL;
 if(r->cpu_after_fe[0]==0xFFFFFFFFUL) r->cpu_after_fe[0]^=0x01000000UL;
 if(!fence_set(io,r,r->cpu_after_fe[0])) goto timeout;
 r->cpu_after_fe[1]=rd(io,0x494);
 if(r->cpu_after_fe[1]!=r->cpu_after_fe[0]) {r->result=VX3D_CONTROL_UNCONFIRMED; goto cleanup;}
 r->cpu_flags|=16;
 if(!fence_set(io,r,r->cpu_original)) goto timeout;
 if(rd(io,0x494)!=r->cpu_original) {r->result=VX3D_CONTROL_UNCONFIRMED; goto cleanup;}
 r->cpu_flags|=32;
 r->phase=3; state=no_texture(io,r);
 if(!state) goto timeout;
 if(state<0) {r->result=VX3D_INIT_FAILURE; goto cleanup;}
 if(!dma_build(io,r,0) || !dma_build(io,r,1) || r->dma_count[0]!=64 ||
    r->dma_count[1]*4+SF_DRAW+0x200>=SF_BRANCH_DRAW) {
  r->result=VX3D_INIT_FAILURE; goto cleanup;
 }
 words=r->vertex_words; vertices=r->vertex_vertices; stride=r->vertex_stride;
 r->vertex_words=0; r->vertex_vertices=0; r->vertex_stride=0;
 r->sf_expected[1]=r->dma_expected[0];
 r->sf_expected[2]=r->queue_fence_original^0x3E540017UL;
 if(r->sf_expected[2]==0xFFFFFFFFUL) r->sf_expected[2]^=0x01000000UL;
 r->sf_expected[3]=r->dma_expected[1];
 r->sf_branch_expected=r->queue_fence_original^0x5B830015UL;
 if(r->sf_branch_expected==0xFFFFFFFFUL) r->sf_branch_expected^=0x01000000UL;
 r->sf_2d_color[0]=0xFF6AC15AUL; r->sf_2d_color[1]=0xFF39B7D2UL;
 r->sf_state_marker[0]=0xD701; r->sf_state_marker[2]=0xD702;
 r->sf_draw_marker_expected=0xFF6CD771UL;
 for(i=0;i<7;i++) r->sf_2d_saved[i]=rd(io,sf_2d_regs[i]);
 r->sf_transfer_flags|=1;
 if(!sf_build_control(r,0) || !sf_build_control(r,1)) {r->result=VX3D_INIT_FAILURE; goto cleanup;}
 if(r->probe_id==VX3D_SF_PROBE) {
  if(!sf_build_matrix(io,r)) {r->result=VX3D_INIT_FAILURE; goto cleanup;}
  r->dma_count[1]=0;
 } else if(!sf_build_inline_vertices(io,r,0) || !sf_build_inline_vertices(io,r,1) ||
    !sf_build_point(io,r,0) || !sf_build_point(io,r,1) || !sf_build_alternate(io,r) ||
    r->sf_alt_pause>=SF_RING_BASE+0x3C00UL) {r->result=VX3D_INIT_FAILURE; goto cleanup;}
 /* Both encoders populate metadata while generating bytes. Keep actual
  * submission fields zero until the first triangle is exposed to the GPU. */
 r->vertex_words=0; r->vertex_vertices=0; r->vertex_stride=0;
 r->sf_pause[0]=SF_RING_BASE+0xF0;
 r->sf_pause[1]=SF_RING_BASE+SF_CONTROL0+r->dma_count[0]*4-16;
 r->sf_pause[2]=SF_RING_BASE+SF_CONTROL1+64*4-16;
 if(!r->sf_matrix_count) r->sf_pause[3]=SF_RING_BASE+SF_DRAW+r->dma_count[1]*4-16;
 r->dma_stop[0]=r->sf_pause[1]; r->dma_stop[1]=r->sf_pause[3];
 /* Fill the entire reachable host allocation, including two guard pages
  * and the harmless page backing every otherwise-unmapped SF entry. */
 for(at=0;at<VX3D_SF_BYTES;at+=4) {
  if(at>=4096 && at<4096+VX3D_SF_RING_BYTES) i=sf_ring_word(r,(at-4096)/4);
  else if(at>=18*4096UL) i=sf_dummy(at/4);
  else i=VX3D_CANARY;
  sf->write_host(sf->context,at,i);
 }
 sf->publish(sf->context); r->sf_flags|=0x40;
 r->sf_phase=2;
 r->sf_memory[0]=sf->read_seq(sf->context,0x68);
 r->sf_memory[1]=sf->read_seq(sf->context,0x6D);
 r->sf_memory[2]=sf->read_seq(sf->context,0x6E);
 sf_seq_snapshot(sf,r->sf_gti_before); r->sf_flags|=4;
 if(!sf_flush_idle(io,sf,r)) goto mapping_failure;
 for(i=0;i<VX3D_SF_TABLE_SPAN;i+=4) table_backup[i/4]=io->read_fb(io->context,VX3D_SF_TABLE_BEGIN+i);
 table_saved=1; r->sf_flags|=8;
 sf->write_seq(sf->context,0x6C,r->sf_gti_before[2]&0x7FU);
 for(at=VX3D_SF_TABLE_BEGIN;at<VX3D_SF_TABLE_END;at+=4)
  io->write_fb(io->context,at,at>=VX3D_SF_GART && at<VX3D_SF_GART+VX3D_SF_TABLE_BYTES?
   sf_pte(r,(at-(uint32_t)VX3D_SF_GART)/4):VX3D_CANARY);
 (void)io->read_fb(io->context,VX3D_SF_TABLE_END-4);
 sf->publish(sf->context); r->sf_flags|=16;
 /* VX855 manual documents SR6A/B/C. SR7B is reserved on this chip:
  * sample and preserve it. All 64 KiB entries are safe even when the
  * implemented table window is smaller; our first ring index is 0x220. */
 sf->write_seq(sf->context,0x6A,(VX3D_SF_GART>>12)&255);
 sf->write_seq(sf->context,0x6B,(VX3D_SF_GART>>20)&255);
 sf->write_seq(sf->context,0x6C,(r->sf_gti_before[2]&0x7EU)|((VX3D_SF_GART>>28)&1));
 r->sf_flags|=32;
 sf->write_seq(sf->context,0x6C,(r->sf_gti_before[2]&0xFEU)|((VX3D_SF_GART>>28)&1)|0x80);
 r->sf_phase=3;
 if(!sf_flush(io,sf,r)) goto mapping_failure;
 sf_seq_snapshot(sf,r->sf_gti_active);
 if(r->sf_gti_active[0]!=((VX3D_SF_GART>>12)&255) ||
    r->sf_gti_active[1]!=((VX3D_SF_GART>>20)&255) ||
    (r->sf_gti_active[2]&0x81)!=(0x80|((VX3D_SF_GART>>28)&1))) goto mapping_failure;
 /* Verify CPU-visible PTEs/source/guards before permitting any reader. */
 if(!sf_check(io,sf,r)) {r->result=VX3D_GUARD_FAILURE; goto cleanup;}
 r->phase=4; r->sf_phase=4; sf_snapshot(io,r,r->sf_before[0]);
 type(io,r,0x10); reg(io,r,0x60,SF_RING_BASE|1); reg(io,r,0x61,0);
 reg(io,r,0x62,SF_RING_BASE+VX3D_SF_RING_BYTES); reg(io,r,0x63,0);
 reg(io,r,0x64,r->sf_pause[0]); reg(io,r,0x65,0);
 reg(io,r,0x66,0xFFFFF0); reg(io,r,0x67,0xFF);
 reg(io,r,0x68,0x0A0001); launched=1;
 state=sf_wait(io,r,0,0);
 if(!state) goto timeout;
 if(state<0) goto pause_failure;
 r->sf_flags|=128;
 for(row=1;row<=2;row++) {
  r->sf_phase=4+row; sf_snapshot(io,r,r->sf_before[row]);
  r->sf_2d_before[row-1]=rd(io,0x58);
  sf_pause_update(io,r,r->sf_pause[row],0);
  r->sf_flags|=row==1?256:1024;
  state=sf_wait(io,r,row,0);
  if(!state) goto timeout;
  if(state==-2) goto pause_failure;
  if(state==1) {r->sf_flags|=row==1?512:2048; control_pass=1;}
  (void)sf_observe_control(io,r,row-1);
  r->guard_errors+=guards(io,r);
  if(r->guard_errors) {r->result=VX3D_GUARD_FAILURE; goto cleanup;}
 }
 r->dma_observed[0]=r->sf_observed[1];
 if(r->sf_matrix_count) {
  r->result=VX3D_SOLID_FAILURE; r->stage=3;
  for(i=0;i<r->sf_matrix_count;i++) {
   vx3d_case_report *c=&r->sf_matrix[i]; int coverage;
   uint32_t dependency=c->config[8];
   /* Packets are immutable and ordered. Never jump across a skipped
    * packet: mark failed controls explicitly and still run the bounded
    * case, preserving what the hardware actually saw. */
   if(dependency!=0xFFFFFFFFUL && !(r->sf_matrix[dependency].flags&16))c->flags|=256;
   sf_matrix_clear(io);
   r->sf_phase=16+i; r->phase=5;
   if(!sf_matrix_setup(io,r,i)) goto timeout;
   if(i==VX3D_DUMP_CASE) sf_state_dump(io,r);
   sf_snapshot(io,r,c->before);
   c->flags|=4;r->sf_matrix_last=i;r->sf_flags|=4096;
   if((c->layout==1 || c->layout==2) && !sf_matrix_cpu(io,r,c)) {c->flags|=64; goto timeout;} /* v10.330 (review #116) */
   sf_pause_update(io,r,c->pause,0);
   /* v10.330 (review #110): the measure restores the desktop window (and the case's high-offset memory, #119): only once the wait
    * confirmed the engine stopped; a timeout or an unconfirmed pause writes nothing more */
   state=sf_wait(io,r,9+i,0);
   if(!state) {c->flags|=64; goto timeout;}
   if(state==-2) {c->flags|=64; goto pause_failure;}
   coverage=sf_matrix_measure(io,r,i); sf_hi_restore(io);
   c->flags|=8; c->latch_observed=rd(io,0x58);
   if(c->latch_observed==c->latch_expected) c->flags|=32;
   if(coverage) c->flags|=16;
   if(i==16 && coverage) {
    uint32_t px;
    for(px=0;px<VX3D_PIXELS;px++) r->image[px]=io->read_fb(io->context,VX3D_TARGET+px*4);
   }
   if((c->mode&15)==2) {
    r->checks_run|=2;
    if(coverage || !r->sf_matrix_triangle_pass) {
     r->solid_errors=c->errors; r->solid_sample=c->sample; r->changed_pixels=c->changed;
    }
    if(coverage) {
     uint32_t px;
     r->sf_matrix_triangle_pass|=1UL<<i;r->result=VX3D_PASS;
     for(px=0;px<VX3D_PIXELS;px++)r->image[px]=io->read_fb(io->context,VX3D_TARGET+px*4);
    }
   } else if((c->mode&15)==0)r->checks_run|=16;
   else if((c->mode&15)==1)r->checks_run|=32;
   if(r->guard_errors) {r->result=VX3D_GUARD_FAILURE; break;}
   if(i==0 && !coverage) break; /* failed point control: do not draw triangle */
  }
  goto retire;
 }
 /* A missed inline fence with a clean PAUSE does not veto rendering.
  * All vertices are inline. No FE83 or branch-enable write is sent. */
 if(!control_pass) r->branch_flags|=16;
 if(r->sf_flags&(512|2048)) r->dma_flags|=4;
 /* Both controls may have filled this target. Clear their colors only
  * after the ring is confirmed paused and the 2D/3D engines are idle.
  * CPU never writes either unique control color or any triangle color. */
 clear(io);
 if(!sf_preload_state(io,r)) goto timeout;
 r->sf_phase=7; r->phase=5;
 sf_snapshot(io,r,r->sf_before[3]);
 r->vertex_words=words; r->vertex_vertices=vertices; r->vertex_stride=stride;
 sf_pause_update(io,r,r->sf_pause[3],0); r->sf_flags|=4096;
 state=sf_wait(io,r,3,0); r->dma_observed[1]=r->sf_observed[3];
 sf_measure_draw(io,r);
 r->sf_primary[0]=r->changed_pixels; r->sf_primary[1]=r->solid_errors;
 r->sf_primary[2]=r->solid_sample;
 if(!state) {r->sf_draw_observation|=1; goto timeout;}
 if(state==-2) {r->sf_draw_observation|=1; goto pause_failure;}
 r->sf_draw_marker_observed=rd(io,0x58);
 if(r->sf_draw_marker_observed==r->sf_draw_marker_expected) r->sf_state_flags|=64;
 if(!sf_marker_snapshot(io,r,1)) goto timeout;
 if(state==1) {r->sf_flags|=8192; r->dma_flags|=16;}
 r->stage=3; r->checks_run|=2;
 r->result=r->solid_errors?VX3D_SOLID_FAILURE:VX3D_PASS;
 if(state!=1) r->sf_transfer_flags|=1024;
 if(r->guard_errors) r->result=VX3D_GUARD_FAILURE;
 if(!r->guard_errors) {
  state=sf_run_point(io,r,0);
  if(!state) goto timeout;
  if(state==-2) goto pause_failure;
  if(r->guard_errors) r->result=VX3D_GUARD_FAILURE;
 }
 if(r->result==VX3D_SOLID_FAILURE) {
  /* A settled OFF miss permits the same inline draw with XP's AGP CMDQ.
   * No FE83/branch unit is used. CPU state/target/vertex values match. */
  clear(io); r->sf_phase=12; sf_snapshot(io,r,r->sf_branch_before);
  if(!sf_enable_agp_queue(io,r) || !sf_preload_state(io,r)) goto timeout;
  r->sf_queue_compare_flags|=8;
  sf_pause_update(io,r,r->sf_branch_pause,0); r->sf_branch_flags|=2;
  state=sf_wait(io,r,6,0);
  sf_measure_draw(io,r);
  r->sf_branch_changed=r->changed_pixels; r->sf_branch_errors=r->solid_errors;
  r->sf_branch_sample=r->solid_sample;
  if(!state) {r->sf_draw_observation|=2; r->sf_branch_flags|=32; goto timeout;}
  if(state==-2) {r->sf_draw_observation|=2; r->sf_branch_flags|=32; goto pause_failure;}
  r->sf_branch_flags|=4;
  r->sf_queue_compare_latch=rd(io,0x58);
  if(r->sf_queue_compare_latch==r->sf_draw_marker_expected+1) r->sf_queue_compare_flags|=16;
  if(state==1) r->sf_branch_flags|=8;
  r->result=r->solid_errors?VX3D_SOLID_FAILURE:VX3D_PASS;
  if(r->result==VX3D_PASS) r->sf_branch_flags|=16;
  if(r->guard_errors) r->result=VX3D_GUARD_FAILURE;
  if(!r->guard_errors) {
   state=sf_run_point(io,r,1);
   if(!state) goto timeout;
   if(state==-2) goto pause_failure;
   if(r->guard_errors) r->result=VX3D_GUARD_FAILURE;
  }
 }
 if(r->result==VX3D_SOLID_FAILURE) {
  /* A failed Header2/Header3 draw must not hide a working Header0 path.
   * The second packet was published before launch, in the same mapping. */
  r->sf_phase=11; sf_snapshot(io,r,r->sf_alt_before);
  sf_pause_update(io,r,r->sf_alt_pause,0); r->sf_alt_flags|=2;
  state=sf_wait(io,r,5,0);
  sf_measure_draw(io,r);
  r->sf_alt_changed=r->changed_pixels; r->sf_alt_errors=r->solid_errors;
  r->sf_alt_sample=r->solid_sample;
  if(!state) {r->sf_draw_observation|=4; goto timeout;}
  if(state==-2) {r->sf_draw_observation|=4; goto pause_failure;}
  r->sf_alt_flags|=4;
  if(state==1) r->sf_alt_flags|=8;
  r->result=r->solid_errors?VX3D_SOLID_FAILURE:VX3D_PASS;
  if(r->result==VX3D_PASS) r->sf_alt_flags|=16;
  if(r->guard_errors) r->result=VX3D_GUARD_FAILURE;
 }
retire:
 r->sf_phase=8;
 if(r->sf_matrix_count) r->sf_pause[4]=SF_RING_BASE+
  (r->sf_matrix_last<r->sf_matrix_count?SF_MATRIX_BASE+r->sf_matrix_last*SF_MATRIX_STRIDE+0x400UL:0xB80UL);
 else r->sf_pause[4]=SF_RING_BASE+((r->sf_alt_flags&2)?0x3C00UL:
  (r->sf_point_flags[1]&2)?0x2800UL:(r->sf_branch_flags&2)?0x2000UL:
  (r->sf_point_flags[0]&2)?0x1800UL:(r->sf_flags&4096)?0xF00UL:0xB80UL);
 /* Retire before either unsubmitted draw; a STOP is not execution proof. */
 sf_snapshot(io,r,r->sf_before[4]); sf_pause_update(io,r,r->sf_pause[4],0x200);
 state=sf_wait(io,r,4,1);
 if(!state) goto timeout;
 if(state<0) goto control_failure;
 state=dma_clear_cycle(io,r,1);
 if(!state) goto timeout;
 if(state<0) goto control_failure;
 if(r->sf_matrix_count)r->guard_errors+=guards(io,r);
 r->sf_flags|=16384; launched=0;
cleanup:
 original_result=r->result;
 if(launched) goto held;
 if(!idle(io,r)) goto timeout;
 if(r->branch_flags&2) {
  type(io,r,0x10); reg(io,r,0x69,0); r->branch_flags|=8;
 }
 if(r->sf_transfer_flags&1) {
  for(i=0;i<7;i++) wr(io,r,sf_2d_regs[i],r->sf_2d_saved[i]);
  for(i=0;i<7;i++) r->sf_2d_restored[i]=rd(io,sf_2d_regs[i]);
  r->sf_transfer_flags|=512;
 }
 if(r->queue_flags&VX3D_Q_OWNED) {
  state=queue_restore(io,r);
  if(!state) goto timeout;
  if(state<0) goto control_failure;
 }
 if(table_saved) {
  if(r->sf_flags&32) (void)sf_check(io,sf,r);
  r->sf_phase=9;
  if(!sf_flush_idle(io,sf,r)) goto mapping_failure;
  sf->write_seq(sf->context,0x6C,sf->read_seq(sf->context,0x6C)&0x7FU);
  for(i=0;i<VX3D_SF_TABLE_SPAN;i+=4) io->write_fb(io->context,VX3D_SF_TABLE_BEGIN+i,table_backup[i/4]);
  (void)io->read_fb(io->context,VX3D_SF_TABLE_END-4); sf->publish(sf->context);
  sf->write_seq(sf->context,0x6A,r->sf_gti_before[0]);
  sf->write_seq(sf->context,0x6B,r->sf_gti_before[1]);
  sf->write_seq(sf->context,0x6C,r->sf_gti_before[2]&0x7FU);
  if(!sf_flush(io,sf,r)) goto mapping_failure;
  sf->write_seq(sf->context,0x6C,r->sf_gti_before[2]);
  sf_seq_snapshot(sf,r->sf_gti_after);
  if(r->sf_gti_after[0]!=r->sf_gti_before[0] || r->sf_gti_after[1]!=r->sf_gti_before[1] ||
     ((r->sf_gti_after[2]^r->sf_gti_before[2])&0x81) ||
     ((r->sf_gti_after[3]^r->sf_gti_before[3])&0x7F)) goto mapping_failure;
  r->sf_flags|=32768;
 }
 if(saved) {
  for(i=0;i<VX3D_ARENA_BYTES;i+=4) io->write_fb(io->context,VX3D_ARENA+i,backup[i/4]);
  for(i=0;i<VX3D_QUEUE_SPAN;i+=4) io->write_fb(io->context,VX3D_QUEUE_REGION+i,queue_backup[i/4]);
  (void)io->read_fb(io->context,VX3D_QUEUE_REGION_END-4);
  r->queue_flags|=VX3D_Q_MEMORY_RESTORED;
 }
 extra_ok=r->sf_matrix_count?sf_2d_extra(io,r):1;
 r->sf_flags|=VX3D_SF_SAFE_RELEASE; r->sf_phase=10;
 r->result=r->guard_errors?VX3D_GUARD_FAILURE:original_result;
 if(!extra_ok) r->result=VX3D_TIMEOUT; /* v10.330 (review #115): a 2D extra left the 2D engine busy (the 3D ring had stopped: the pages may go) */
 if(r->result==VX3D_PASS) r->stage=6;
 goto done;
mapping_failure:
 r->result=VX3D_SF_MAPPING_FAILURE; goto held;
pause_failure:
 r->result=VX3D_SF_PAUSE_UNCONFIRMED; goto held;
control_failure:
 r->result=VX3D_CONTROL_UNCONFIRMED; goto held;
timeout:
 r->result=VX3D_TIMEOUT;
held:
 r->sf_flags|=0x40000; r->fault_latched=1; r->sf_failure_phase=r->sf_phase;
done:
 r->elapsed_ms=io->milliseconds(io->context)-start;
}

/* ---- v9.62 2D lab -----------------------------------------------------
 * One operator-defined operation on the 64x64 desktop window. Defaults for
 * the common registers are programmed first, then the operator's own
 * register list (which may override any of them), then the command word,
 * then optional host data into the 0x200000 window. */
static uint32_t lab_pre(uint32_t x,uint32_t y) {
 uint32_t gx,gy,v,k;
 if(x>=4 && x<20 && y>=4 && y<20) {
  gx=x-4; gy=y-4; v=0xFF203060UL;
  if((gx>=3&&gx<=5&&gy>=3&&gy<=13)||(gy>=3&&gy<=5&&gx>=3&&gx<=12)||(gy>=7&&gy<=9&&gx>=3&&gx<=9)) v=0xFFFFFFFFUL;
  k=0;
  if(gx<2&&gy<2) v=0xFFFF0000UL;
  if(gx>13&&gy<2) v=0xFF00FF00UL;
  if(gx<2&&gy>13) v=0xFF0000FFUL;
  if(gx>13&&gy>13) v=0xFFFFFF00UL;
  (void)k; return v;
 }
 return 0xFF101820UL|(((x*3+y*2)&0x1F)<<16)|(((x+y*3)&0x1F)<<8);
}

/* v10.330 (review #122, #129, #130, #171): set when a lab copy or operation left the engine busy even after the 2D reset; the lab then
 * writes nothing more into that memory or those registers and reports VX3D_TIMEOUT (which latches the lab's fault) */
static int lab_hung;
/* v9.90: full-screen 640x480x32 copy with the 2D engine (src/dst are byte offsets in video memory, pitch 2560).
 * Returns 1 if the engine finished, 0 if it hung (then it is reset). */
static int lab_screen_copy(const vx3d_io *io,vx3d_report *r,uint32_t src,uint32_t dst) {
 wr(io,r,0x04,0x300); wr(io,r,0x08,0x01400140UL);
 wr(io,r,0x14,dst>>3); wr(io,r,0x1C,src>>3); wr(io,r,0x20,0); wr(io,r,0x24,0); wr(io,r,0x28,0);
 wr(io,r,0x2C,0); wr(io,r,0x30,0); wr(io,r,0x40,0); wr(io,r,0x44,0); wr(io,r,0x48,0);
 wr(io,r,0x4C,0); wr(io,r,0x50,0); wr(io,r,0x58,0); wr(io,r,0x5C,0);
 wr(io,r,0x0C,(479UL<<16)|639UL); wr(io,r,0x10,0); wr(io,r,0x18,0);
 wr(io,r,0x00,0xCC000001UL);
 if(!idle(io,r)) {
  if(sf_seqio) {
   uint32_t v=sf_seqio->read_seq(sf_seqio->context,0x4E),k,ms;
   sf_seqio->write_seq(sf_seqio->context,0x4E,(v&~0x0CUL)|0x04UL);
   ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
   sf_seqio->write_seq(sf_seqio->context,0x4E,v&~0x0CUL);
   ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
  }
  wr(io,r,0x04,0x300); wr(io,r,0x14,0); wr(io,r,0x1C,0);
  if(!idle(io,r)) lab_hung=1; /* v10.330 (review #122, #171): still busy after the reset */
  return 0;
 }
 wr(io,r,0x14,0); wr(io,r,0x1C,0);
 return 1;
}

/* v10.04: copy a w x h block of 32bpp pixels (pitch 2560 both sides) between two byte offsets in video memory with the 2D engine.
 * Returns 1 if the engine finished, 0 if it hung (it is then reset). Used by the video-memory reach test. */
static int lab_blk_copy(const vx3d_io *io,vx3d_report *r,uint32_t src,uint32_t dst,uint32_t w,uint32_t h) {
 wr(io,r,0x04,0x300); wr(io,r,0x08,0x01400140UL);
 wr(io,r,0x14,dst>>3); wr(io,r,0x1C,src>>3); wr(io,r,0x20,0); wr(io,r,0x24,0); wr(io,r,0x28,0);
 wr(io,r,0x2C,0); wr(io,r,0x30,0); wr(io,r,0x40,0); wr(io,r,0x44,0); wr(io,r,0x48,0);
 wr(io,r,0x4C,0); wr(io,r,0x50,0); wr(io,r,0x58,0); wr(io,r,0x5C,0);
 wr(io,r,0x0C,((h-1)<<16)|(w-1)); wr(io,r,0x10,0); wr(io,r,0x18,0);
 wr(io,r,0x00,0xCC000001UL);
 if(!idle(io,r)) {
  if(sf_seqio) {
   uint32_t v=sf_seqio->read_seq(sf_seqio->context,0x4E),k,ms;
   sf_seqio->write_seq(sf_seqio->context,0x4E,(v&~0x0CUL)|0x04UL);
   ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
   sf_seqio->write_seq(sf_seqio->context,0x4E,v&~0x0CUL);
   ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
  }
  wr(io,r,0x04,0x300); wr(io,r,0x14,0); wr(io,r,0x1C,0);
  if(!idle(io,r)) lab_hung=1; /* v10.330 (review #122, #171): still busy after the reset */
  return 0;
 }
 wr(io,r,0x14,0); wr(io,r,0x1C,0);
 return 1;
}
static const uint16_t lab_scr_pt[8][2]={{32,32},{607,32},{32,447},{607,447},{110,200},{200,110},{200,240},{330,250}};
static const uint32_t lab_scr_val[8]={0xFFFF0000UL,0xFF00FF00UL,0xFF0000FFUL,0xFFFFFF00UL,0xFFFFFFFFUL,0xFFFFFFFFUL,0xFFFFFFFFUL,0xFF00FFFFUL};
static uint32_t lab_scr_pix(int x,int y) {
 if(x<64&&y<64) return 0xFFFF0000UL;
 if(x>=576&&y<64) return 0xFF00FF00UL;
 if(x<64&&y>=416) return 0xFF0000FFUL;
 if(x>=576&&y>=416) return 0xFFFFFF00UL;
 if(x>=320&&x<360&&y>=240&&y<280) return 0xFF00FFFFUL;
 if(x>=100&&x<120&&y>=100&&y<380) return 0xFFFFFFFFUL;
 if(x>=100&&x<300&&y>=100&&y<120) return 0xFFFFFFFFUL;
 if(x>=100&&x<240&&y>=230&&y<250) return 0xFFFFFFFFUL;
 if(x==0||y==0||x==639||y==479) return 0xFFFFFFFFUL;
 return 0xFF202020UL;
}
void vx3d_run_lab(const vx3d_io *io,const vx3d_sf_io *sf,vx3d_report *r,const vx3d_request *q) {
 static const uint32_t at[19]={0x04,0x08,0x0C,0x10,0x14,0x18,0x1C,0x20,0x24,0x28,0x2C,0x48,0x4C,0x50,0x58,0x5C,0x30,0x40,0x44};
 static uint32_t desk[4096]; /* v9.63: static, a 16 KB local overflowed the VxD stack */
 static uint32_t rotsave2[16];
 uint32_t saved[19],xsaved[VX3D_LAB_REGS],i,x,y,n,changed=0,hung=0,rec=0;
 uint32_t minx=64,miny=64,maxx=0,maxy=0;
 uint32_t w=q->lab_w?q->lab_w:1,h=q->lab_h?q->lab_h:1;
 sf_seqio=sf; lab_hung=0;
 n=q->lab_nregs>VX3D_LAB_REGS?VX3D_LAB_REGS:q->lab_nregs;
 for(i=0;i<n;i++) if(q->lab_reg[i][0]>=0x70 && (q->lab_reg[i][0]<0x100||q->lab_reg[i][0]>0x1FC) && (q->lab_reg[i][0]<0x1E00||q->lab_reg[i][0]>0x1FFC)) {r->result=VX3D_BAD_REQUEST; return;}
 for(i=0;i<n;i++) if(q->lab_reg[i][0]==0 || (q->lab_reg[i][0]&3)) {r->result=VX3D_BAD_REQUEST; return;}

 if(q->lab_flags&256) {
  /* v9.79 throughput bench: repeat the operator's operation lab_bg times (host data every time) and time it.
   * flag 512 = CPU baseline: rotate-copy lab_data (w x h) into the desktop window with CPU framebuffer writes instead. */
  uint32_t reps=q->lab_bg?q->lab_bg:1,rp,t0,t1,nd=q->lab_ndata>VX3D_LAB_DATA?VX3D_LAB_DATA:q->lab_ndata;
  /* v10.330 (review #111): the CPU baseline reads lab_data[sy*w + sx] and writes the box rotated at (dx, dy): w x h within lab_data and
   * the rotated box within the saved 64 x 64 window, or nothing is done (it read past the request and wrote outside the window) */
  if((q->lab_flags&512) && (w>64 || h>64 || w*h>VX3D_LAB_DATA || q->lab_dx>64-h || q->lab_dy>64-w)) {r->result=VX3D_BAD_REQUEST; return;}
  if(!idle(io,r)) {r->result=VX3D_BUSY; return;}
  for(i=0;i<19;i++) saved[i]=rd(io,at[i]);
  for(i=0;i<16;i++) rotsave2[i]=rd(io,0x1E00UL+i*0x20UL); /* v10.330 (review #128): window registers the operator's list may set */
  for(y=0;y<64;y++) for(x=0;x<64;x++) {
   desk[y*64+x]=io->read_fb(io->context,(uint32_t)(SF_2D_WY+y)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+x)*4);
   io->write_fb(io->context,(uint32_t)(SF_2D_WY+y)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+x)*4,0xFF000000UL);
  }
  (void)io->read_fb(io->context,(uint32_t)(SF_2D_WY+63)*SF_DESK_PITCH);
  wr(io,r,0x04,q->lab_mode);
  wr(io,r,0x08,320UL<<16);
  wr(io,r,0x14,0); wr(io,r,0x1C,0); wr(io,r,0x20,0); wr(io,r,0x24,0); wr(io,r,0x28,0);
  wr(io,r,0x2C,0); wr(io,r,0x30,0); wr(io,r,0x40,0); wr(io,r,0x44,0); wr(io,r,0x48,0);
  wr(io,r,0x4C,q->lab_fg); wr(io,r,0x50,q->lab_fg); wr(io,r,0x58,q->lab_fg); wr(io,r,0x5C,q->lab_fg);
  wr(io,r,0x0C,((h-1)<<16)|(w-1));
  wr(io,r,0x10,((uint32_t)(SF_2D_WY+q->lab_dy)<<16)|(uint32_t)(SF_2D_WX+q->lab_dx));
  wr(io,r,0x18,((uint32_t)(SF_2D_WY+q->lab_sy)<<16)|(uint32_t)(SF_2D_WX+q->lab_sx));
  for(i=0;i<n;i++) wr(io,r,q->lab_reg[i][0],q->lab_reg[i][1]);
  t0=io->milliseconds(io->context);
  for(rp=0;rp<reps && !hung;rp++) {
   if(q->lab_flags&512) {
    /* CPU baseline: 90 degree rotate of a w x h source into the box at (dx,dy) */
    uint32_t sy2,sx2;
    for(sy2=0;sy2<h;sy2++) for(sx2=0;sx2<w;sx2++)
     io->write_fb(io->context,(uint32_t)(SF_2D_WY+q->lab_dy+sx2)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+q->lab_dx+(h-1-sy2))*4,q->lab_data[sy2*w+sx2]);
   } else {
    wr(io,r,0x00,q->lab_cmd);
        for(i=0;i<nd;i++) io->write_mmio(io->context,0x200000UL,q->lab_data[i]); /* v9.80: no drain read per dword */
    if(!idle(io,r)) hung=1;
   }
  }
  t1=io->milliseconds(io->context);
  if(!(q->lab_flags&512)) (void)idle(io,r);
  (void)io->read_fb(io->context,(uint32_t)(SF_2D_WY+63)*SF_DESK_PITCH);
  if(hung && sf_seqio) {
   uint32_t v=sf_seqio->read_seq(sf_seqio->context,0x4E),k,ms;
   sf_seqio->write_seq(sf_seqio->context,0x4E,(v&~0x0CUL)|0x04UL);
   ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
   sf_seqio->write_seq(sf_seqio->context,0x4E,v&~0x0CUL);
   ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
   wr(io,r,0x04,0x300);
   if(idle(io,r)) rec=1;
  }
  for(y=0;y<64;y++) for(x=0;x<64;x++) {
   uint32_t got=io->read_fb(io->context,(uint32_t)(SF_2D_WY+y)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+x)*4);
   r->image[y*64+x]=got;
   if(got!=0xFF000000UL) changed++;
  }
  /* v10.330 (review #130): an engine still busy after the reset gets no more writes (window, registers); [0][4] is the repetitions
   * that finished, not the ones asked for */
  if(!(hung&&!rec)) {
   for(y=0;y<64;y++) for(x=0;x<64;x++)
    io->write_fb(io->context,(uint32_t)(SF_2D_WY+y)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+x)*4,desk[y*64+x]);
   (void)io->read_fb(io->context,(uint32_t)(SF_2D_WY+63)*SF_DESK_PITCH);
   for(i=0;i<16;i++) wr(io,r,0x1E00UL+i*0x20UL,0);
   for(i=0;i<16;i++) if(rotsave2[i]) wr(io,r,0x1E00UL+i*0x20UL,rotsave2[i]);
   for(i=0;i<19;i++) wr(io,r,at[i],saved[i]);
  }
  r->sf_x_info[0][0]=changed; r->sf_x_info[0][1]=hung; r->sf_x_info[0][2]=rec;
  r->sf_x_info[0][3]=t1-t0; r->sf_x_info[0][4]=hung?rp-1:rp;
  r->changed_pixels=changed;
  r->result=(hung&&!rec)?VX3D_TIMEOUT:VX3D_PASS;
  return;
 }


 if(q->lab_flags&524288UL) {
  /* v10.04 video-memory reach test. q->lab_sx = mapped video memory bytes (from the status ioctl).
   * Phase A (CPU): sentinel dwords at up to 16 offsets from 4 MiB up to the last page; written, read back, then the
   *   original contents are restored in reverse order. image[0]=probes valid, image[1]=probes OK;
   *   image[16+4k..19+4k] = offset, dword at off, dword at off+4, ok flag.
   * Phase B (2D engine): copy a 32x8 block (source 0x250000, pitch 2560) to a destination at each of up to 14 offsets;
   *   CPU verifies the destination; if it did not arrive, the six aliases dest & (2^b-1), b=22..27, are searched
   *   (their contents are saved and restored). image[96+8j..103+8j] = dest, state (0 full, 1 partial, 2 none, 3 hung),
   *   matched dwords of 256, first dword read, mask of wrap bits b (bit b set = a copy was found at dest&(2^b-1)), most dwords found at any alias, that alias address, 0.
   *   image[2]=destinations valid, image[3]=full matches, image[4]=1 if the engine hung. */
  static const uint32_t cpu_off[15]={0x400000UL,0x800000UL,0xC00000UL,0x1000000UL,0x2000000UL,0x4000000UL,0x6000000UL,0x8000000UL,0xA000000UL,0xC000000UL,0xE000000UL,0x10000000UL,0x14000000UL,0x18000000UL,0x1C000000UL};
  static const uint32_t dst_off[13]={0x400000UL,0x800000UL,0x1000000UL,0x2000000UL,0x4000000UL,0x8000000UL,0xA000000UL,0xC000000UL,0xE000000UL,0x10000000UL,0x14000000UL,0x18000000UL,0x1C000000UL};
  static uint32_t vr_save[18][2],vr_alias[6][256],vr_src[256],vr_dst[256]; /* v10.330 (review #112): the source and each destination block kept */
  uint32_t fb=q->lab_sx,off[18],dst[16],nprobe=0,ndst=0,k,j,b,y,x,ok=0,full=0,hang=0,g1,g2,e1,e2;
  r->result=VX3D_PASS;
  if(fb<0x500000UL) {r->image[0]=0; return;}
  for(k=0;k<15;k++) if(cpu_off[k]+16<=fb-0x1000UL) off[nprobe++]=cpu_off[k];
  off[nprobe++]=(fb-0x1000UL)&~7UL;
  for(k=0;k<13;k++) if(dst_off[k]+0x8000UL<=fb) dst[ndst++]=dst_off[k];
  dst[ndst++]=(fb-0x10000UL)&~7UL;
  for(k=0;k<nprobe;k++) {vr_save[k][0]=io->read_fb(io->context,off[k]); vr_save[k][1]=io->read_fb(io->context,off[k]+4);}
  for(k=0;k<nprobe;k++) {io->write_fb(io->context,off[k],0xC0DE0000UL|k); io->write_fb(io->context,off[k]+4,~(0xC0DE0000UL|k));}
  (void)io->read_fb(io->context,off[nprobe-1]+4);
  for(k=0;k<nprobe;k++) {
   e1=0xC0DE0000UL|k; e2=~e1;
   g1=io->read_fb(io->context,off[k]); g2=io->read_fb(io->context,off[k]+4);
   r->image[16+4*k]=off[k]; r->image[17+4*k]=g1; r->image[18+4*k]=g2; r->image[19+4*k]=(g1==e1&&g2==e2)?1:0;
   if(g1==e1&&g2==e2) ok++;
  }
  for(k=nprobe;k>0;k--) {io->write_fb(io->context,off[k-1],vr_save[k-1][0]); io->write_fb(io->context,off[k-1]+4,vr_save[k-1][1]);}
  (void)io->read_fb(io->context,off[0]);
  r->image[0]=nprobe; r->image[1]=ok;
  if(!idle(io,r)) {r->result=VX3D_BUSY; return;}
  for(i=0;i<19;i++) saved[i]=rd(io,at[i]);
  for(y=0;y<8;y++) for(x=0;x<32;x++) vr_src[y*32+x]=io->read_fb(io->context,0x250000UL+y*2560UL+x*4);
  for(j=0;j<ndst && !hang;j++) {
   uint32_t D=dst[j],matched=0,state,got0,amask=0,amatch=0,aaddr=0;
   for(y=0;y<8;y++) for(x=0;x<32;x++) vr_dst[y*32+x]=io->read_fb(io->context,D+y*2560UL+x*4);
   for(y=0;y<8;y++) for(x=0;x<32;x++) {
    io->write_fb(io->context,0x250000UL+y*2560UL+x*4,0x90000000UL+(j<<20)+(y<<8)+x);
    io->write_fb(io->context,D+y*2560UL+x*4,0x11111111UL);
   }
   for(b=22;b<=27;b++) {
    uint32_t A=D&((1UL<<b)-1UL);
    if(A!=D) for(y=0;y<8;y++) for(x=0;x<32;x++) vr_alias[b-22][y*32+x]=io->read_fb(io->context,A+y*2560UL+x*4);
   }
   (void)io->read_fb(io->context,D+7*2560UL);
   if(!lab_blk_copy(io,r,0x250000UL,D,32,8)) {hang=1;}
   got0=io->read_fb(io->context,D);
   for(y=0;y<8;y++) for(x=0;x<32;x++) if(io->read_fb(io->context,D+y*2560UL+x*4)==0x90000000UL+(j<<20)+(y<<8)+x) matched++;
   if(hang) state=3; else if(matched==256) {state=0; full++;} else if(matched) state=1; else state=2;
   if(matched<256) for(b=22;b<=27;b++) {
    uint32_t A=D&((1UL<<b)-1UL),m2=0;
    if(A==D) continue;
    for(y=0;y<8;y++) for(x=0;x<32;x++) if(io->read_fb(io->context,A+y*2560UL+x*4)==0x90000000UL+(j<<20)+(y<<8)+x) m2++;
    if(m2) {amask|=1UL<<b; if(m2>=amatch) {amatch=m2; aaddr=A;}}
   }
   /* v10.330 (review #171): an engine still busy after the reset may yet write there: nothing is put back then */
   if(!lab_hung) {
    for(b=27;b>=22;b--) {
     uint32_t A=D&((1UL<<b)-1UL);
     if(A!=D) for(y=0;y<8;y++) for(x=0;x<32;x++) io->write_fb(io->context,A+y*2560UL+x*4,vr_alias[b-22][y*32+x]);
    }
    for(y=0;y<8;y++) for(x=0;x<32;x++) io->write_fb(io->context,D+y*2560UL+x*4,vr_dst[y*32+x]);
   }
   (void)io->read_fb(io->context,0);
   r->image[96+8*j]=D; r->image[97+8*j]=state; r->image[98+8*j]=matched; r->image[99+8*j]=got0; r->image[100+8*j]=amask; r->image[101+8*j]=amatch; r->image[102+8*j]=aaddr;
  }
  if(!lab_hung) {
   for(y=0;y<8;y++) for(x=0;x<32;x++) io->write_fb(io->context,0x250000UL+y*2560UL+x*4,vr_src[y*32+x]);
   (void)io->read_fb(io->context,0x250000UL);
   for(i=0;i<19;i++) wr(io,r,at[i],saved[i]);
  }
  r->image[2]=ndst; r->image[3]=full; r->image[4]=hang;
  r->sf_x_info[0][0]=ok; r->sf_x_info[0][1]=hang; r->sf_x_info[0][2]=hang?(!lab_hung&&idle(io,r)?1:0):0;
  if(hang && !r->sf_x_info[0][2]) r->result=VX3D_TIMEOUT;
  return;
 }
 if(q->lab_flags&131072) {
  /* v9.99: scan the first 4 MB of video memory with the CPU for stale host-stream words (tag 81 or AA in the top byte, low 24 bits < 8).
   * image[0]=count of 81 words, image[1..24]=their byte addresses; image[32]=count of AA words, image[33..56]=addresses. */
  uint32_t a2,v2,n81=0,nAA=0;
  for(a2=0;a2<0x400000UL;a2+=4) {
   v2=io->read_fb(io->context,a2);
   if((v2&0xFFFFFFF8UL)==0x81000000UL) {if(n81<24) r->image[1+n81]=a2; n81++;}
   else if((v2&0xFFFFFFF8UL)==0xAA000000UL) {if(nAA<24) r->image[33+nAA]=a2; nAA++;}
  }
  r->image[0]=n81; r->image[32]=nAA;
  r->result=VX3D_PASS; return;
 }
 if(q->lab_flags&4096) {
  /* v9.90: CBU window on the VISIBLE 640x480x32 desktop. Steps: 2D copy the desktop to a backup surface (A, 4 MB),
   * paint an asymmetric test picture straight into the desktop with the CPU, program the operator's window
   * registers (control last), HOLD for lab_bg milliseconds so the operator can look at the monitor, read 8 sample
   * pixels with the CPU, 2D copy the desktop to a second surface (B, 6 MB) and read the same samples there,
   * disable the windows, read the samples again, then put the desktop back from A.
   * image[0..7] CPU reads while ON, [8..15] reads of copy B, [16..23] CPU reads after OFF, [24..31] expected. */
  uint32_t k,hold=q->lab_bg,t0,fl=0,baseA=q->lab_sx?q->lab_sx:0x400000UL,baseB=q->lab_sy?q->lab_sy:0x250000UL;
  static uint32_t rs[16],psave[6];
  static const uint32_t probe[6]={0x300000UL,0x400000UL,0x500000UL,0x600000UL,0x800000UL,0x1000000UL};
  /* v10.330 (review #113): A and B each hold a 640 x 480 x 32 copy: both past the visible desktop and apart from each other, 8-byte
   * aligned (the engine takes base >> 3), or nothing is done; the six probe dwords are kept and put back */
  if(baseA<0x12C000UL || baseB<0x12C000UL || baseA>0xFFFFFFFFUL-0x12C000UL || baseB>0xFFFFFFFFUL-0x12C000UL ||
     (baseA<baseB+0x12C000UL && baseB<baseA+0x12C000UL) || (baseA&7) || (baseB&7)) {r->result=VX3D_BAD_REQUEST; return;}
  if(hold>15000UL) hold=15000UL; /* v10.330 (review #120): the hold is a busy wait in the VxD: 15 s at most (VX2DLAB asks 8) */
  if(!idle(io,r)) {r->result=VX3D_BUSY; return;}
  for(i=0;i<19;i++) saved[i]=rd(io,at[i]);
  for(k=0;k<16;k++) rs[k]=rd(io,0x1E00UL+k*0x20UL);
  for(k=0;k<16;k++) wr(io,r,0x1E00UL+k*0x20UL,0);
  for(k=0;k<6;k++) psave[k]=io->read_fb(io->context,probe[k]);
  for(k=0;k<6;k++) io->write_fb(io->context,probe[k],0xA5000000UL|(probe[k]>>8));
  (void)io->read_fb(io->context,probe[5]);
  for(k=0;k<6;k++) r->image[40+k]=io->read_fb(io->context,probe[k]);
  if(!lab_screen_copy(io,r,0,baseA)) fl|=1;
  if(lab_hung) goto cbu_hung;
  for(y=0;y<480;y++) for(x=0;x<640;x++) io->write_fb(io->context,y*2560UL+x*4UL,lab_scr_pix((int)x,(int)y));
  (void)io->read_fb(io->context,479UL*2560UL+639UL*4UL);
  /* control: copy to B with the window OFF, read the samples (image[32..39]) */
  for(k=0;k<8;k++) io->write_fb(io->context,baseB+(uint32_t)lab_scr_pt[k][1]*2560UL+(uint32_t)lab_scr_pt[k][0]*4UL,0xDEAD0000UL|k);
  (void)io->read_fb(io->context,baseB);
  if(!lab_screen_copy(io,r,0,baseB)) fl|=8;
  if(lab_hung) goto cbu_hung;
  for(k=0;k<8;k++) r->image[32+k]=io->read_fb(io->context,baseB+(uint32_t)lab_scr_pt[k][1]*2560UL+(uint32_t)lab_scr_pt[k][0]*4UL);
  for(i=0;i<n;i++) wr(io,r,q->lab_reg[i][0],q->lab_reg[i][1]);
  for(i=0;i<n;i++) r->sf_x_cls[0][i]=rd(io,q->lab_reg[i][0]);
  t0=io->milliseconds(io->context);
  while((uint32_t)(io->milliseconds(io->context)-t0)<hold) {}
  for(k=0;k<8;k++) r->image[k]=io->read_fb(io->context,(uint32_t)lab_scr_pt[k][1]*2560UL+(uint32_t)lab_scr_pt[k][0]*4UL);
  for(k=0;k<8;k++) io->write_fb(io->context,baseB+(uint32_t)lab_scr_pt[k][1]*2560UL+(uint32_t)lab_scr_pt[k][0]*4UL,0xDEAD0000UL|k);
  (void)io->read_fb(io->context,baseB);
  if(!lab_screen_copy(io,r,0,baseB)) fl|=2;
  if(lab_hung) goto cbu_hung;
  for(k=0;k<8;k++) r->image[8+k]=io->read_fb(io->context,baseB+(uint32_t)lab_scr_pt[k][1]*2560UL+(uint32_t)lab_scr_pt[k][0]*4UL);
  for(k=0;k<16;k++) wr(io,r,0x1E00UL+k*0x20UL,0);
  for(k=0;k<8;k++) {r->image[16+k]=io->read_fb(io->context,(uint32_t)lab_scr_pt[k][1]*2560UL+(uint32_t)lab_scr_pt[k][0]*4UL); r->image[24+k]=lab_scr_val[k];}
  /* v10.330 (review #122): the desktop is copied back from A only when the backup copy to A finished (fl bit 4: not put back) */
  if(fl&1) fl|=16; else if(!lab_screen_copy(io,r,baseA,0)) fl|=4;
  if(lab_hung) goto cbu_hung;
  (void)io->read_fb(io->context,479UL*2560UL+639UL*4UL);
  for(k=0;k<8;k++) r->image[48+k]=io->read_fb(io->context,(uint32_t)lab_scr_pt[k][1]*2560UL+(uint32_t)lab_scr_pt[k][0]*4UL);
  for(k=0;k<6;k++) io->write_fb(io->context,probe[k],psave[k]);
  (void)io->read_fb(io->context,probe[5]);
  for(k=0;k<16;k++) if(rs[k]) wr(io,r,0x1E00UL+k*0x20UL,rs[k]);
  for(i=0;i<19;i++) wr(io,r,at[i],saved[i]);
  r->sf_x_info[0][0]=0; r->sf_x_info[0][1]=fl; r->sf_x_info[0][2]=0; r->sf_x_info[0][3]=0;
  r->sf_x_info[0][4]=rd(io,0x400);
  r->result=VX3D_PASS; return;
  /* v10.330 (review #122): a copy left the engine busy after its reset: nothing more is written; TIMEOUT latches the lab's fault */
 cbu_hung:
  r->sf_x_info[0][0]=0; r->sf_x_info[0][1]=fl|32; r->sf_x_info[0][2]=0; r->sf_x_info[0][3]=0;
  r->sf_x_info[0][4]=rd(io,0x400);
  r->result=VX3D_TIMEOUT; return;
 }
 if(q->lab_flags&8) {
  /* v9.64: CBU rotate-window test. CPU writes a 64x64 linear image into the
   * arena target while the operator's rotate window registers are active,
   * then the image is read back linearly (windows optionally disabled first). */
  static uint32_t rotsave[16];
  uint32_t k;
  for(k=0;k<16;k++) rotsave[k]=rd(io,0x1E00UL+k*0x20UL);
  for(y=0;y<64;y++) for(x=0;x<64;x++) {
   ((uint32_t*)r->sf_x_raw)[y*64+x]=lab_pre(x,y);
   io->write_fb(io->context,VX3D_TARGET+(y*64+x)*4,0xFF000000UL);
  }
  (void)io->read_fb(io->context,VX3D_TARGET);
  if(q->lab_flags&32) { /* v9.66: picture written first with the window off; reads then go through the window */
   for(y=0;y<64;y++) for(x=0;x<64;x++) io->write_fb(io->context,VX3D_TARGET+(y*64+x)*4,lab_pre(x,y));
   (void)io->read_fb(io->context,VX3D_TARGET);
  }
  for(i=0;i<n;i++) wr(io,r,q->lab_reg[i][0],q->lab_reg[i][1]);
  for(i=0;i<n;i++) r->sf_x_cls[0][i]=rd(io,q->lab_reg[i][0]); /* v9.65: register readback */
  if(!(q->lab_flags&32)) {
   for(y=0;y<64;y++) for(x=0;x<64;x++) io->write_fb(io->context,VX3D_TARGET+(y*64+x)*4,lab_pre(x,y));
   (void)io->read_fb(io->context,VX3D_TARGET);
  }
  if((q->lab_flags&4) && !(q->lab_flags&32)) for(k=0;k<16;k++) wr(io,r,0x1E00UL+k*0x20UL,0);
  (void)io->read_fb(io->context,VX3D_TARGET);
  for(y=0;y<64;y++) for(x=0;x<64;x++) {
   uint32_t got=io->read_fb(io->context,VX3D_TARGET+(y*64+x)*4);
   r->image[y*64+x]=got;
   if(got!=lab_pre(x,y)) {changed++; if(x<minx)minx=x; if(x>maxx)maxx=x; if(y<miny)miny=y; if(y>maxy)maxy=y;}
  }
  for(k=0;k<16;k++) wr(io,r,0x1E00UL+k*0x20UL,0);
  for(k=0;k<16;k++) if(rotsave[k]) wr(io,r,0x1E00UL+k*0x20UL,rotsave[k]);
  r->sf_x_info[0][0]=changed; r->sf_x_info[0][1]=0; r->sf_x_info[0][2]=0;
  r->sf_x_info[0][3]=(minx<<24)|(miny<<16)|(maxx<<8)|maxy;
  r->sf_x_info[0][4]=rd(io,0x400);
  r->changed_pixels=changed; r->result=VX3D_PASS; return;
 }
 if(!idle(io,r)) {r->result=VX3D_BUSY; return;}
 for(i=0;i<19;i++) saved[i]=rd(io,at[i]);
 for(i=0;i<n;i++) xsaved[i]=(q->lab_reg[i][0]<0x100)?rd(io,q->lab_reg[i][0]):0;
 for(i=0;i<16;i++) rotsave2[i]=rd(io,0x1E00UL+i*0x20UL);
 if(q->lab_flags&16) {for(y=0;y<64;y++) for(x=0;x<64;x++) io->write_fb(io->context,VX3D_TARGET+(y*64+x)*4,(q->lab_flags&64)?lab_pre(x,y):0xFF000000UL); (void)io->read_fb(io->context,VX3D_TARGET);}
 if(q->lab_flags&128) {for(y=0;y<64;y++) for(x=0;x<64;x++) io->write_fb(io->context,VX3D_TARGET+(y*64+x)*4,q->lab_fg); (void)io->read_fb(io->context,VX3D_TARGET);} /* v9.83: solid scratch source */
 if(q->lab_flags&32768) {
  /* v9.96: no history. Reset the 2D engine (as after a hang), then clear the whole 4 KB source area of the scratch surface. */
  if(sf_seqio) {
   uint32_t v=sf_seqio->read_seq(sf_seqio->context,0x4E),k,ms;
   sf_seqio->write_seq(sf_seqio->context,0x4E,(v&~0x0CUL)|0x04UL);
   ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
   sf_seqio->write_seq(sf_seqio->context,0x4E,v&~0x0CUL);
   ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
   wr(io,r,0x04,0x300); (void)idle(io,r);
  }
  for(i=0;i<1024;i++) io->write_fb(io->context,VX3D_TARGET+i*4,0);
  (void)io->read_fb(io->context,VX3D_TARGET+4092);
 }
 if(q->lab_flags&65536) {for(i=0;i<1024;i++) io->write_fb(io->context,VX3D_TARGET+i*4,0x3C000000UL|i); (void)io->read_fb(io->context,VX3D_TARGET+4092);} /* v9.97: every source dword unique (0x3C000000 | index) so the output bits identify the dword fetched */
 if((q->lab_flags&2048) && !(q->lab_flags&65536)) {for(i=0;i<8;i++) {io->write_fb(io->context,VX3D_TARGET+i*q->lab_sx,(q->lab_flags&16384)?q->lab_data[VX3D_LAB_DATA-1]:q->lab_data[2*i]); io->write_fb(io->context,VX3D_TARGET+i*q->lab_sx+4,(q->lab_flags&16384)?q->lab_data[VX3D_LAB_DATA-1]:q->lab_data[2*i+1]);} (void)io->read_fb(io->context,VX3D_TARGET);} /* v9.89: 8-row bitmap at a chosen byte stride (lab_sx) */
 if(q->lab_flags&1024) {uint32_t nn=q->lab_ndata>VX3D_LAB_DATA?VX3D_LAB_DATA:q->lab_ndata; for(i=0;i<nn;i++) io->write_fb(io->context,VX3D_TARGET+i*4,q->lab_data[i]); (void)io->read_fb(io->context,VX3D_TARGET);} /* v9.87: lab_data into the scratch surface (mono source bitmap) */
 for(y=0;y<64;y++) for(x=0;x<64;x++) {
  desk[y*64+x]=io->read_fb(io->context,(uint32_t)(SF_2D_WY+y)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+x)*4);
  io->write_fb(io->context,(uint32_t)(SF_2D_WY+y)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+x)*4,lab_pre(x,y));
  ((uint32_t*)r->sf_x_raw)[y*64+x]=lab_pre(x,y);
 }
 (void)io->read_fb(io->context,(uint32_t)(SF_2D_WY+63)*SF_DESK_PITCH);
 wr(io,r,0x04,q->lab_mode);
 wr(io,r,0x08,(q->lab_flags&1)?(320UL<<16):((320UL<<16)|320UL));
 wr(io,r,0x14,0); wr(io,r,0x1C,0); wr(io,r,0x20,0); wr(io,r,0x24,0); wr(io,r,0x28,0);
 wr(io,r,0x2C,0); wr(io,r,0x30,0); wr(io,r,0x40,0); wr(io,r,0x44,0); wr(io,r,0x48,0);
 wr(io,r,0x4C,q->lab_fg); wr(io,r,0x50,q->lab_bg); wr(io,r,0x58,q->lab_fg); wr(io,r,0x5C,q->lab_bg);
 wr(io,r,0x0C,((h-1)<<16)|(w-1));
 wr(io,r,0x10,((uint32_t)(SF_2D_WY+q->lab_dy)<<16)|(uint32_t)(SF_2D_WX+q->lab_dx));
 wr(io,r,0x18,((uint32_t)(SF_2D_WY+q->lab_sy)<<16)|(uint32_t)(SF_2D_WX+q->lab_sx));
 for(i=0;i<n;i++) wr(io,r,q->lab_reg[i][0],q->lab_reg[i][1]);
 for(i=0;i<n;i++) r->sf_x_cls[0][i]=rd(io,q->lab_reg[i][0]); /* v9.65: register readback */
 wr(io,r,0x00,q->lab_cmd);
 if(q->lab_flags&1) {
  uint32_t nd=q->lab_ndata>VX3D_LAB_DATA?VX3D_LAB_DATA:q->lab_ndata;
  uint32_t stp=(q->lab_flags&8192)?2UL:1UL; /* v9.94: flag 8192 = feed every second dword (lab_data holds 2 dwords per bitmap row) */
  if(q->lab_flags&262144) {
   /* v10.00: stepped feed. Status after the command (before any data), then after EACH host dword; stop feeding the moment the engine is idle. */
   uint32_t stt,fed=0,done=0;
   r->sf_x_cls[1][8]=rd(io,0x400);
   for(i=0;i<nd;i+=stp) {
    wr(io,r,0x200000UL+((q->lab_flags&2)?(i/stp)*4UL:0UL),q->lab_data[i]);
    stt=rd(io,0x400); if(fed<8) r->sf_x_cls[1][fed]=stt; fed++;
    if((stt&0x1FF7UL)==0) {done=1; break;}
   }
   r->sf_x_info[2][0]=fed; r->sf_x_info[2][1]=done;
  } else
  for(i=0;i<nd;i+=stp) wr(io,r,0x200000UL+((q->lab_flags&2)?(i/stp)*4UL:0UL),q->lab_data[i]);
 }
 if(!idle(io,r)) { /* v10.330 (review #129): a hang is a hang whether or not the sequencer reset is there to try */
  uint32_t v,k,ms;
  hung=1; r->sf_x_info[1][0]=r->wait_failure_status; r->sf_x_info[1][1]=r->wait_failure_reason; r->sf_x_info[1][2]=rd(io,0x400); r->sf_x_info[1][3]=rd(io,0x0C); r->sf_x_info[1][4]=rd(io,0x10); /* v9.89: engine state at the hang */
  if(sf_seqio) {
   v=sf_seqio->read_seq(sf_seqio->context,0x4E);
   sf_seqio->write_seq(sf_seqio->context,0x4E,(v&~0x0CUL)|0x04UL);
   ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
   sf_seqio->write_seq(sf_seqio->context,0x4E,v&~0x0CUL);
   ms=io->milliseconds(io->context); for(k=0;k<2000;k++) {(void)rd(io,0x400); if((uint32_t)(io->milliseconds(io->context)-ms)>=5) break;}
   wr(io,r,0x04,0x300);
   if(idle(io,r)) {rec=1; r->wait_failure_reason=0; r->wait_failure_status=0;}
  }
 }
 (void)io->read_fb(io->context,(uint32_t)(SF_2D_WY+63)*SF_DESK_PITCH);
 if((q->lab_flags&4) && !(hung&&!rec)) for(i=0;i<16;i++) wr(io,r,0x1E00UL+i*0x20UL,0);
 if((q->lab_flags&16) && !(q->lab_flags&64)) {
  /* v9.65: read the 64x64 scratch target (linear, pitch 64 pixels) instead of the desktop window */
  (void)io->read_fb(io->context,VX3D_TARGET);
  for(y=0;y<64;y++) for(x=0;x<64;x++) {
   uint32_t got=io->read_fb(io->context,VX3D_TARGET+(y*64+x)*4);
   r->image[y*64+x]=got;
   if(got!=0xFF000000UL) {changed++; if(x<minx)minx=x; if(x>maxx)maxx=x; if(y<miny)miny=y; if(y>maxy)maxy=y;}
  }
 } else
 for(y=0;y<64;y++) for(x=0;x<64;x++) {
  uint32_t got=io->read_fb(io->context,(uint32_t)(SF_2D_WY+y)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+x)*4);
  r->image[y*64+x]=got;
  if(got!=lab_pre(x,y)) {changed++; if(x<minx)minx=x; if(x>maxx)maxx=x; if(y<miny)miny=y; if(y>maxy)maxy=y;}
 }
 if(!(hung&&!rec)) { /* v10.330 (review #129): no writes into an engine still busy after the reset */
  for(i=0;i<16;i++) wr(io,r,0x1E00UL+i*0x20UL,0);
  for(i=0;i<16;i++) if(rotsave2[i]) wr(io,r,0x1E00UL+i*0x20UL,rotsave2[i]);
  for(y=0;y<64;y++) for(x=0;x<64;x++)
   io->write_fb(io->context,(uint32_t)(SF_2D_WY+y)*SF_DESK_PITCH+(uint32_t)(SF_2D_WX+x)*4,desk[y*64+x]);
  (void)io->read_fb(io->context,(uint32_t)(SF_2D_WY+63)*SF_DESK_PITCH);
  for(i=0;i<n;i++) if(q->lab_reg[i][0]<0x100) wr(io,r,q->lab_reg[i][0],xsaved[i]);
  for(i=0;i<19;i++) wr(io,r,at[i],saved[i]);
 }
 r->sf_x_info[0][0]=changed; r->sf_x_info[0][1]=hung; r->sf_x_info[0][2]=rec;
 r->sf_x_info[0][3]=(minx<<24)|(miny<<16)|(maxx<<8)|maxy;
 r->sf_x_info[0][4]=rd(io,0x400);
 r->changed_pixels=changed;
 r->result=(hung&&!rec)?VX3D_TIMEOUT:VX3D_PASS;
}
