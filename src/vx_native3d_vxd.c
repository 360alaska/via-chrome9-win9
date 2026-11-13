/* Win98 VxD owner for native 3D requests. Copyright 2026, MIT. */
#include "winhack.h"
#include "vmm.h"
#include "vxd_lib.h"
#include "vx85598.h"
#include "3d_accel.h"
#include "vx_native3d.h"
#include <stddef.h>
#include "vx855_2d.h"
#include "vx_smp.h" /* .520: the sampler's phases (vx_smp_ph) */
volatile uint32_t vx_smp_ph; /* .520: the step the VxD is at (VX_SMP_PH_*), for the sampler */
#define SMP_PH(x) do{vx_smp_ph=(x);}while(0)
#define IO_IN8
#define IO_OUT8
#include "io32.h"
extern FBHDA_t *hda;
int VX2D_busy(void);
int VX2D_lost(void);
int VX3D_busy(void);
int VX3D_lab_busy(void);
void VX2D_sync(void);
void VX2D_device_lost(void);
void VX2D_3d_reset(void); /* v10.281 */
void vx_lock(void);void vx_unlock(void); /* .515 (vx855_2d.c) */
unsigned long VX_irq_save(void);
void VX_irq_restore(unsigned long);
#pragma aux VX_irq_save = "pushfd" "pop eax" "cli" value [eax] modify exact [eax];
#pragma aux VX_irq_restore = "push eax" "popfd" parm [eax] modify exact [];
static uint32_t mapped,running,fault,submitted,triangles,clears,last_status,last_result;
static uint32_t publication_word;
/* v10.218: why the last submit was refused as BUSY (nothing was sent to the engine), in last_result bits 8 and up: 1 = the 3D or 2D side
 * was running (bit 16 = 3D, bit 17 = 2D); 2 = SR1A bit 6/1 or SR4E bits 5:4 set (SR1A in bits 23:16, SR4E in 31:24); 3 = the engine clock
 * was not locked (SR3C in bits 23:16). */
static uint32_t busy_why;
/* v10.210: registry ACCEL_D3D_RECOVER (default 1). A draw that times out no longer fences 3D and 2D until reboot: the engine is reset
 * the way v9.40 recovered it on the board (SR4E bits 5:4 = 01 pulsed, then SR1A bit 6 pulsed if still busy, 5 ms each way), then given
 * 250 ms to read idle (0x400 & 1FF7 = 0). Idle: the draw is dropped (never replayed) and VX3DN_RECOVERED returned; the device stays
 * usable. Not idle: the old fence. vx3dn_rec[]: recoveries, failures, status before, status after, last step (1 SR4E, 2 SR4E + SR1A). */
volatile unsigned long vx3dn_recover=1;
uint32_t vx3dn_rec[5];
static void publish(void) {
 /* Drain CPU/WC framebuffer stores before GPU use and after CPU clears,
  * without requiring SSE/SFENCE in a 486-targeted VxD. */
 _asm {xor eax,eax
       xchg [publication_word],eax}
}
/* v10.390 (advice: the Linux VIA DRM flushes write combining and reads the ring's last word back before every kick; a locked
 * instruction drains WC buffers on Intel, the board's CPU is a VIA C7): before a fetcher start or kick, SFENCE when the CPU has SSE
 * (CPUID 1 EDX bit 25, looked up once) and a read of the last word written (ACCEL_D3D_WCFLUSH, default 1) */
unsigned long fx_cpuid_ok(void); /* EFLAGS.ID toggles */
#pragma aux fx_cpuid_ok = 0x9C 0x58 0x89 0xC1 0x35 0x00 0x00 0x20 0x00 0x50 0x9D 0x9C 0x58 0x51 0x9D 0x31 0xC8 value [eax] modify exact [eax ecx];
unsigned long fx_cpuid1_edx(void);
#pragma aux fx_cpuid1_edx = 0xB8 0x01 0x00 0x00 0x00 0x0F 0xA2 0x89 0xD0 value [eax] modify exact [eax ebx ecx edx];
void fx_sfence(void);
#pragma aux fx_sfence = 0x0F 0xAE 0xF8 modify exact [];
volatile uint32_t vx3dn_wc_flush=1;static int fx_sse=-1;uint32_t vx3dn_wc_stat[2]; /* flushes, with SFENCE */
static void fx_wc_flush(uint32_t last_off) {
 if(!vx3dn_wc_flush)return;
 if(fx_sse<0)fx_sse=fx_cpuid_ok()?(int)((fx_cpuid1_edx()>>25)&1):0;
 if(fx_sse){fx_sfence();vx3dn_wc_stat[1]++;}
 vx3dn_wc_stat[0]++;
 if(last_off>=4)(void)*(volatile uint32_t*)(vx_status.fb_linear+last_off-4);
}
int VESA_draw_needs_exclusion(unsigned long off,unsigned long bytes); /* vxd_vesa.c, v10.371 */
static vx3dn_packet request_copy;
/* v10.220: ACCEL_D3D_POSTED (default 1): engine writes are posted. Every write used to be followed by a read of 0x400 (status), about
 * 600 extra reads per 48-vertex packet. x86 uncached writes stay in order and the next read (the idle wait's) flushes them, as VIA's
 * DRM writes its PCI command stream. 0 = the read after every write, as before. TEXDBG 0x78 on, 0x79 off, 0x7A reads.
 * vx3dn_prof: draw packets, TSC cycles / 256 in VX3DN_submit for them, engine writes, status reads, other reads, clears, their cycles (TEXDBG 0x76 reads,
 * 0x77 reads and clears). */
volatile uint32_t vx3dn_posted=1;
uint32_t vx3dn_prof[8];
unsigned long VX_tsc(void);
#pragma aux VX_tsc = 0x0F 0x31 value [eax] modify exact [eax edx];
#include "vx_lat.h"
/* v10.329: the profile (the TSC readings, the latency distributions, the register-access counts) only while vx_prof is set: registry
 * ACCEL_PROFILE (default 0), TEXDBG 0x6C on / 0x6D off / 0x6E reads, and on at a request that clears a profile (the test programs and
 * VXGL_LOG clear before they measure). Off, a reading is 0 and nothing is added: the recovery and hang records stay as they were */
volatile uint32_t vx_prof;
static unsigned long PT(void) {return vx_prof?VX_tsc():0UL;}
#define LAT(l,v) do{if(vx_prof)vx_lat_add(l,v);}while(0)
/* v10.321: latency distributions (TSC cycles; TEXDBG 0x64-0x67 read one, 0x68 clears them): [0] the 3D critical section held (entry to
 * exit, every 3D call), [1] fetch_idle (the wait for the engine to read idle), [2] the wait for the draw before (fx_retire with a draw in
 * flight), [3] a whole draw submit or batch call */
vx_lat vx3dn_lat[4];
static uint32_t cs_t0,cs_depth;
/* TEXDBG 0x64-0x67: [0] the code, [1] samples, [2] p50, [3] p95, [4] p99, [5] max (TSC cycles), [6] sum (cycles / 256); 0x68 clears them */
void VX3DN_lat(uint32_t code,uint32_t *o) {
 uint32_t k,i;o[0]=code;
 if(code>=0x64&&code<0x68)vx_lat_out(&vx3dn_lat[code-0x64],o+1);
 else if(code==0x68)for(k=0;k<4;k++){vx3dn_lat[k].n=vx3dn_lat[k].sum=vx3dn_lat[k].max=0;for(i=0;i<VX_LAT_B;i++)vx3dn_lat[k].h[i]=0;}
}
unsigned long cs_tsc(void);
#pragma aux cs_tsc = 0x0F 0x31 value [eax] modify exact [eax edx];
static uint32_t cs_wt; /* v10.393: TSC / 256 waited for the critical section (the slow-call records) */
/* .512: the thread waiting in cs_enter for the system critical section (the sampler, while it is on, records what runs meanwhile: the
 * holder; board, GTA3 .511: clears waited 86-194 ms for it). */
volatile uint32_t vx_cs_waiter;
static void cs_enter(void) {uint32_t a=cs_tsc();vx_cs_waiter=(uint32_t)Get_Cur_Thread_Handle();vx_lock();vx_cs_waiter=0;cs_wt+=(cs_tsc()-a)>>8;if(!cs_depth++)cs_t0=PT();}
static void cs_leave(void) {if(cs_depth&&!--cs_depth)LAT(&vx3dn_lat[0],PT()-cs_t0);vx_unlock();}
static uint32_t read_mmio(void *ctx,uint32_t r) {(void)ctx;if(vx_prof){if(r==0x400)vx3dn_prof[3]++;else vx3dn_prof[4]++;}return *(volatile uint32_t*)(mapped+r);}
static void write_mmio(void *ctx,uint32_t r,uint32_t v) {(void)ctx;*(volatile uint32_t*)(mapped+r)=v;if(vx_prof)vx3dn_prof[2]++;if(!vx3dn_posted)(void)read_mmio(0,0x400);}
static uint32_t read_fb(void *ctx,uint32_t r) {(void)ctx;return *(volatile uint32_t*)(vx_status.fb_linear+r);}
static void write_fb(void *ctx,uint32_t r,uint32_t v) {(void)ctx;*(volatile uint32_t*)(vx_status.fb_linear+r)=v;}
static uint32_t ticks(void *ctx) {(void)ctx;return Get_System_Time();}
static unsigned char sr(unsigned char i) {outp(0x3C4,i);return inp(0x3C5);}
static void sw(unsigned char i,unsigned char v) {outp(0x3C4,i);outp(0x3C5,v);}
static void wait5(void) {uint32_t s=Get_System_Time();while((uint32_t)(Get_System_Time()-s)<5)(void)read_mmio(0,0x400);}
static void pulse(unsigned char i,unsigned char on,unsigned char clear) {
 unsigned long f;unsigned char old,v;
 f=VX_irq_save();old=inp(0x3C4);v=sr(i);sw(i,(unsigned char)((v&~clear)|on));outp(0x3C4,old);VX_irq_restore(f);wait5();
 f=VX_irq_save();old=inp(0x3C4);sw(i,(unsigned char)(v&~clear&~on));outp(0x3C4,old);VX_irq_restore(f);wait5();
}
/* called with SR10 unlocked (inside the submit's sequencer window); returns 1 when the engine reads idle again */
extern volatile uint32_t vx2d_reset_epoch;
/* v10.382 (board, VXRING v2.1 op 9): the fetcher's read pointer stays where its last words ended after the stop (68 = 0A0000). Writing
 * the pause (64/65) for a new start kicks it from there before 68 = 0A0001 moves it to the new start: the words after the old position
 * ran for about the time of the register writes in between (2-6 trap cells of 256 bytes; written after the stop or before it alike;
 * none when they were no-op blocks, none when the new start was the old position). fx_pend: the GPU offset where the words the fetcher
 * last ran end (0xFFFFFFFF not known: after a reset or VXRING) */
static uint32_t fx_pend=0xFFFFFFFFUL;uint32_t vx3dn_gap_stat[3]; /* starts with a gap written over, its words, starts with the old position not known */
static void fx_park_forget(void); /* v10.391 */
/* v10.402: XP's ring in GART-mapped system memory (the xr_ functions below). xr_state: -1 off for good (vx3dn_xr_stat[1]
 * says why), 0 not set up, 1 the page table set and the ring not running, 2 the ring running */
#define XR_PAGES 64UL
#define XR_BYTES (XR_PAGES*4096UL)
#define XR_IDX0 0x100UL
#define XR_GPU (XR_IDX0<<12)
#define XR_END (XR_GPU+XR_BYTES-0x10UL)
#define XR_WRAP (XR_BYTES-256UL) /* v10.404: the wrap point, a block before the ring's end (62/63) as XP's always is */
#define XR_ENTRIES 16384UL
volatile uint32_t vx3dn_xr=1;
volatile unsigned long vx3dn_gart_off;
volatile uint32_t vx3dn_xr_cr_pause; /* the command regulator's busy bit (0x400 bit 4) seen set with the fetcher at its pause (vx855_2d.c) */
uint32_t vx3dn_xr_stat[72]={1};
static int xr_state,xr_ran,xr_tested,xr_after_wrap,xr_wrapgrp,xr_lap;
/* v10.410: [0] wraps, [1] space checks, [2] waits they took, [3-5] 43C, 444, 400 when the last post-wrap kick was written again
 * (TEXDBG 0xBC 0x95) */
uint32_t vx3dn_xr_stat2[6];static uint32_t xr_lin,xr_next,xr_pend;
/* v10.407: where XP's ring spends the CPU's time (TSC / 256; TEXDBG 0xBC 0x92-0x93 reads, 0x94 clears): [0] opening groups (wraps,
 * starts), [1] the fence and the group's tail words, [2] the cache flush, [3] the 2D registers read again before a kick, [4] the kick's
 * register writes, [5] the recorder after a kick, [6] retire waits, [7] the 2D registers put back after one; counts: [8] 2D re-reads,
 * [9] put-backs, [10] kicks, [11] retires */
uint32_t vx3dn_xr_prof[12];
#define XP_ADD(i,t) (vx3dn_xr_prof[i]+=(PT()-(t)+128)>>8)
static int fx_in_ring(uint32_t a) {return xr_lin&&a>=XR_GPU&&a<XR_GPU+XR_BYTES;}
static uint32_t fx_rd(uint32_t a) {return fx_in_ring(a)?*(volatile uint32_t*)(xr_lin+a-XR_GPU):*(volatile uint32_t*)(vx_status.fb_linear+a);}
#define RESET_IDLE_MASK 0x101FF7UL /* v10.391 (review): the reset's idle check also waits for bit 20, which fetch_idle and XP's wait (0x101FF3) test */
static int engine_reset(void) {
 uint32_t st,s;
 fx_pend=0xFFFFFFFFUL;fx_park_forget();
 vx2d_reset_epoch++; /* v10.252: the cursor is programmed again at its next show */
 vx3dn_rec[2]=read_mmio(0,0x400);vx3dn_rec[4]=1;
 if(vx3dn_rec[2]==0xFFFFFFFFUL)return 0;
 /* v10.292 (board, VXDX7 v2.74 at 16 and 32 bpp): SR4E bit 4 alone brought the engine back to idle, but the next textured draw hung
  * again, every time (6 of 6, with bits 5:4 or bit 4 twice too); SR4E bit 4 then SR1A bit 6 drew it right (2 of 2). The engine left
  * like that also hung VXTEX's first 12 groups after VXDX7 ran. Both now, always (.530: the TEXDBG F0-F2 forms removed) */
 pulse(0x4E,0x10,0x30);
 pulse(0x1A,0x40,0);vx3dn_rec[4]=2;
 s=Get_System_Time();
 do st=read_mmio(0,0x400); while((st&RESET_IDLE_MASK)&&st!=0xFFFFFFFFUL&&(uint32_t)(Get_System_Time()-s)<250);
 vx3dn_rec[3]=st;
 vx2d_reset_epoch++; /* v10.326 (review): again after the pulses, so the cursor is programmed after the last of them */
 VX2D_3d_reset(); /* v10.281: the 2D side sets the engine up again before its next operation */
 return !(st&RESET_IDLE_MASK)&&st!=0xFFFFFFFFUL;
}
void VX3DN_info(vx3dn_info *i) {
 uint32_t limit;
 memset(i,0,sizeof(*i));i->cb=sizeof(*i);i->abi=VX3DN_ABI;
 i->generation=vx_status.set_successes;i->fault=fault;
 i->submitted=submitted;i->triangles=triangles;i->clears=clears;i->last_status=last_status;i->last_result=last_result;
 if(!hda) return;
 i->width=hda->width;i->height=hda->height;i->pitch=hda->pitch;i->bpp=hda->bpp;i->primary_bytes=hda->stride;
 limit=hda->vram_size_virt;if(limit>vx_status.fb_bytes)limit=vx_status.fb_bytes;
 if(limit>VX_FB_LIMIT)limit=VX_FB_LIMIT;
 if(hda->overlays_size<=limit) i->fb_bytes=limit-hda->overlays_size;
 {extern int VX_chip_accel(void);if(!VX_chip_accel())return;} /* .579: VX900: no 3D yet (ready 0: Direct3D is not offered, OpenGL draws in software) */
 i->ready=!fault&&!VX2D_lost()&&vx_status.stage==6&&!vx_status.failure&&!vx_status.vga_mode&&!vx_native_display_hold&&
  !hda->surface&&!hda->system_surface&&hda->vram_pm32&&
  (uint32_t)hda->vram_pm32==vx_status.fb_linear&&hda->pitch==vx_status.video_pitch&&
  hda->width==vx_status.width&&hda->height==vx_status.height&&hda->bpp==vx_status.bpp&&
  (i->bpp==16||i->bpp==32)&&i->fb_bytes>=i->primary_bytes;
}
/* v10.177: Direct3D clears go to the 2D engine as solid fills (XP's driver clears with 2D command F0002001 too). The engine works at the
 * desktop depth only, so a surface of the other depth is filled as that depth when its value allows: a 16-bit surface on the 32-bit
 * desktop as pixel pairs (even left and right edges), a 32-bit one on the 16-bit desktop when both halves are equal. Anything else, and
 * whatever the 2D side declines (ACCEL_FILL_MIN, busy), is cleared by the CPU as before. Registry ACCEL_3D_CLEAR = 0: CPU clears only.
 * The 2D wait reads the same status word (0x400 & 1FF7) as the 3D emitter, so a draw still running finishes before the fill starts. */
volatile unsigned long vx_heap_align=1;
volatile unsigned long vx_d3d_log=0; /* v10.202: registry ACCEL_D3D_LOG (default 0): the HAL's C:\VXD3D.LOG */
volatile uint32_t vx3dn_clear2d_masked=1; /* v10.179: registry ACCEL_3D_CLEAR bit 1 = 0 sends partial D24S8 clears to the CPU */
volatile uint32_t vx3dn_clear2d=1,vx3dn_clear2d_done,vx3dn_clear2d_cpu;
static uint32_t rgb565(uint32_t c) {return ((c>>8)&0xF800)|((c>>5)&0x7E0)|((c>>3)&31);}
/* v10.179: c = the word in the surface's format, m = the bits it replaces. A partial mask (D24S8 Z-only or stencil-only) is two engine
 * fills with ROPs that read the destination: D & ~m (ROP A0, P AND D), then D | c (ROP FA, P OR D). If the second is declined after the
 * first ran, the CPU clear finishes the job; its read-merge-write gives the same result either way. */
/* v10.282: ACCEL_3D_CLEAR_ASYNC (default 1): the last fill of a clear of an offscreen target is left running on the 2D engine (VX2D_ASYNC
 * with vx2d_own3d): the clear returns while the engine fills, and whatever touches that memory next waits for it (complete_pending: the
 * next 3D submit through VX2D_busy, a Lock through VX2D_access, every 2D call). The depth is cleared first and the target last (the
 * larger fill at 32 bpp), so most of the fill overlaps the application's work on the next frame (board, v10.280: a 634x455 clear of
 * colour and Z took 354 microseconds of the VxD's time, every frame). 0 = every fill waits, as before. */
volatile uint32_t vx3dn_clear_async=1;
extern volatile uint32_t vx2d_own3d;
int VX2D_pending(void);
uint32_t vx3dn_clear_async_n; /* clears left running */
static uint32_t fill_one(const vx3dn_rect *clip,const vx3dn_surface *s,uint32_t c,uint32_t rop,int async) {
 vx2d_request r;uint32_t two=s->format==VX3DN_RGB565||s->format==VX3DN_D16,bpp=hda?hda->bpp:0,l=(uint32_t)clip->left,rt=(uint32_t)clip->right,w=s->width;
 if(two&&bpp==32){if((l|rt)&1)return VX2D_SKIP;c=(c&0xFFFFUL)|(c<<16);l>>=1;rt>>=1;w>>=1;}
 else if(!two&&bpp==16){if((c>>16)!=(c&0xFFFFUL))return VX2D_SKIP;c&=0xFFFFUL;l<<=1;rt<<=1;w<<=1;}
 else if((two?16UL:32UL)!=bpp)return VX2D_SKIP;
 memset(&r,0,sizeof(r));r.cb=sizeof(r);r.abi=VX2D_ABI;r.op=VX2D_FILL;r.flags=VX2D_DDRAW|(async?VX2D_ASYNC:0);r.bpp=bpp;r.rop=rop;r.fg=c;
 r.dst.offset=s->offset;r.dst.pitch=s->pitch;r.dst.width=w;r.dst.height=s->height;
 r.dx=l;r.dy=(uint32_t)clip->top;r.width=rt-l;r.height=(uint32_t)clip->bottom-(uint32_t)clip->top;
 {uint32_t rc;vx2d_own3d=async?1UL:0UL;rc=(uint32_t)VX2D_submit(&r);vx2d_own3d=0;return rc;}
}
static uint32_t fill2d(const vx3dn_packet *p,const vx3dn_surface *s,uint32_t c,uint32_t m,int async) {
 uint32_t rc,two=s->format==VX3DN_RGB565||s->format==VX3DN_D16;
 if(two||m==0xFFFFFFFFUL)return fill_one(&p->clip,s,c,0xF0,async);
 if(!vx3dn_clear2d_masked)return VX2D_SKIP;
 rc=fill_one(&p->clip,s,~m,0xA0,0);if(rc!=VX2D_DONE)return rc;
 return fill_one(&p->clip,s,c&m,0xFA,async);
}
uint32_t vx3dn_plane_fill2d; /* v10.248: the stencil plane cleared by the 2D engine (count) */
static uint32_t submit_impl(const vx3dn_packet *p);
/* v10.228: ACCEL_D3D_FETCH (default 1): draw packets go to the engine through the command fetcher (cookbook 16.17) from the command
 * block in video memory (vx3dn_cmd_off, VX3DN_CMD_BYTES, reserved by vxd_main.c below the save block; 0 = none: register path).
 * 0 = every packet through the 0x43C/0x440 registers as before. TEXDBG 0xA1 on, 0xA2 off, 0xA3 reads, 0xA4 reads and clears
 * vx3dn_fetch_stat: packets fetched, words, packets sent through the registers instead (capture did not fit), fetches that did not
 * finish, the most words in one packet. */
volatile uint32_t vx3dn_fetch=1;
volatile unsigned long vx3dn_cmd_off;
uint32_t vx3dn_fetch_stat[6]; /* v10.231: [5] draws left running (async) */
#define FETCH_DECLINED 0xFFFFFFFFUL
#define FX_REBUILD 0xFFFFFFFEUL /* v10.281: fx_start found the draw before it hung and reset the engine: the group was built on its state and is built again */
static uint32_t fetch_draw(const vx3dn_packet *p,uint32_t *last,int async);
/* v10.231: ACCEL_D3D_ASYNC (default 1): a fetched draw returns as soon as the fetcher is started; the engine works while the HAL builds
 * the next packet. The draw still in flight is retired (idle wait, then the stop 68 = 0A0000) before anything else touches the engine,
 * the render target or the display: the next fetched draw just before its start, every other 3D submit, VX3DN_busy (the 2D engine
 * paths, Lock/Unlock and GetBltStatus through VX2D_access, flips, mode sets and VXRING all ask it), VX2D_sync (DOS boxes, exit).
 * Only for an offscreen target, and only when SR1A bit 3 and SR1E bit 0 were already set (the submit's close does not gate the
 * engine under the draw). A retire that does not see idle within 100 ms resets the engine as a failed draw does. 0 = every draw waits
 * for idle before it returns, as v10.228. TEXDBG 0xA6 on, 0xA7 off. */
volatile uint32_t vx3dn_async=1;
/* v10.232: ACCEL_D3D_STATE_CACHE (default 1): a fetched draw whose state (everything in the packet but the vertex count and the
 * per-vertex arrays) equals the last fetched draw's, with nothing in between that can change the engine's 3D state (vx3dn_state_gen:
 * vx3dn_forget, a register-path draw, an engine reset, VXRING, TEXDBG, the 3D lab), sends only the primitive set-up and its vertices,
 * as XP sends state only when it changes (VXRING 12/13: vertex blocks alone drew on the state already in the engine). 0 = the whole
 * state with every draw. TEXDBG 0xAA on, 0xAB off; 0xA8 reads, 0xA9 reads and clears vx3dn_cache_stat: draws sent without state,
 * then TSC cycles / 256 in the VxD's fetch path for building the packet, waiting for the draw before, and in all. */
volatile uint32_t vx3dn_state_cache=1;
uint32_t vx3dn_cache_stat[4];
extern uint32_t vx3dn_state_gen,vx3dn_vertices_only;
static vx3dn_packet fx_prev;static uint32_t fx_prev_ok,fx_prev_gen;
#define FXO(f) offsetof(vx3dn_packet,f)
static const uint32_t fx_ranges[7][2]=VX3DN_STATE_RANGES; /* v10.236: the runs are geometry, not state; v10.321: nor r1[]; v10.308: the stipple; .524: the table in vx_native3d.h (the HAL's state deltas) */
/* v10.236: the packet copied without the per-vertex arrays' unused tails (a 6-vertex packet was copied whole, 9140 bytes): every range
 * outside the arrays, then the first n entries of each, n = the vertex count read once and clamped (the copy's own count is that n) */
/* v10.236: rep movsd / repe cmpsd (the C loops were about six instructions a dword) */
void vx_movsd(void *d,const void *s,uint32_t n);
#pragma aux vx_movsd = "cld" "rep movsd" parm [edi] [esi] [ecx] modify exact [edi esi ecx];
int vx_cmpsd(const void *a,const void *b,uint32_t n);
#pragma aux vx_cmpsd = "cld" "xor eax,eax" "repe cmpsd" "setne al" parm [esi] [edi] [ecx] value [eax] modify exact [eax esi edi ecx];
static void copy_words(void *d,const void *s,uint32_t bytes) {vx_movsd(d,s,bytes>>2);}
void vx_stosd(void *d,uint32_t v,uint32_t n);
#pragma aux vx_stosd = "cld" "rep stosd" parm [edi] [eax] [ecx] modify exact [edi ecx];
/* v10.412 (board, v10.411: copying a batch's packets in 5.6 microseconds a packet, the HAL's packets no longer in the CPU's caches when
 * the VxD reads them): ACCEL_D3D_PREFETCH (default 1, needs SSE): while a batch's packet is built, the next packet's ranges that
 * copy_packet will read are asked for (PREFETCHT0, a hint: nothing is read or changed by it, and it never faults). 0 = none. */
volatile uint32_t vx3dn_prefetch=1;uint32_t vx3dn_pf_stat[2]; /* packets, lines */
void vx_prefetch(const void *a);
#pragma aux vx_prefetch = 0x0F 0x18 0x08 parm [eax] modify exact [];
static void pf_range(const void *a,uint32_t bytes) {
 uint32_t q=(uint32_t)a&~63UL,e=(uint32_t)a+bytes;
 vx3dn_pf_stat[1]+=(e-q+63)>>6;for(;q<e;q+=64)vx_prefetch((const void*)q);
}
uint32_t vx3dn_pre_stat[4]; /* v10.441: prepacked packets copied as they are, unpacked into the arrays; [2] v10.449: their vertices left to the
 * emitter (ACCEL_D3D_ONECOPY); [3] bytes copied in (16-byte units) */
static uint32_t fx_state_bytes;
/* v10.449 (ACCEL_D3D_ONECOPY, default 1; 0 = as before): in a batch through the fetcher (cp_defer), a prepacked packet's vertex block is not
 * copied into request_copy: the emitter copies the words from the caller's packet straight into the command block (rep movsd) and checks
 * them there, the words the engine will read, with the validation's tests (vx3dn_vsrc, vx3dn_vsrc_pkt; vx3dn_validate leaves those tests
 * to it). A word that fails refuses the packet before its group is started (fx_build puts the group back as it was), as the validation
 * would have. One copy of the vertices instead of two, and no separate pass over them. */
volatile uint32_t vx3dn_onecopy=1;static int cp_defer;
extern const uint32_t *vx3dn_vsrc;extern const vx3dn_packet *vx3dn_vsrc_pkt;
/* .524 (ACCEL_D3D_DELTA, vx_native3d.h): delta 1 = the caller's state-delta word is honoured (d holds the previous packet of the same
 * batch call, validated); cp_same = the ranges kept (bits 0-5 = ranges 1-6), cp_sbytes the state bytes read. vx3dn_delta_stat: [0]
 * packets with the word honoured, [1] ranges kept, [2] state bytes not read (16-byte units), [3] packets whose state's checks were not
 * done again, [4] tagged packets copied whole (the first of a call, alone, rebuilt) */
volatile uint32_t vx3dn_delta=1;uint32_t vx3dn_delta_stat[5];static uint32_t cp_same;
static void copy_packet_d(vx3dn_packet *d,const vx3dn_packet *s,int delta) {
 uint32_t k,n=s->vertex_count,sb=0,same=0,cd;const uint32_t po=s->pre.on,lim=po==2?VX3DN_IX_VERTICES:VX3DN_MAX_VERTICES; /* .529: pre.on read once: an
  * indexed packet's limit (384) only for the copy that goes the indexed way */
 vx3dn_vsrc_pkt=0;vx3dn_vsrc=0; /* v10.449 */
 if(n>lim)n=lim+1; /* still refused by the validation; only lim entries are copied */
 copy_words(d,s,fx_ranges[0][1]);sb=fx_ranges[0][1]; /* the header: always */
 cd=d->clear_depth; /* read once, from the copy */
 if(d->op==VX3DN_DRAW&&VX3DN_IS_SAME(cd)){d->clear_depth=0;if(delta&&vx3dn_delta)same=cd&VX3DN_SAME_MASK;else vx3dn_delta_stat[4]++;}
 for(k=1;k<7;k++)if(!((same>>(k-1))&1)){copy_words((char*)d+fx_ranges[k][0],(const char*)s+fx_ranges[k][0],fx_ranges[k][1]-fx_ranges[k][0]);sb+=fx_ranges[k][1]-fx_ranges[k][0];}
  else vx3dn_delta_stat[1]++;
 if(same){vx3dn_delta_stat[0]++;if(!fx_state_bytes)for(k=0;k<7;k++)fx_state_bytes+=fx_ranges[k][1]-fx_ranges[k][0];vx3dn_delta_stat[2]+=(fx_state_bytes-sb+15)>>4;}
 cp_same=same;
 d->vertex_count=n;if(n>lim)n=lim;
 /* v10.315: the per-vertex arrays only where the validation and the emitter read them (their tests, on the state just copied): the texture
  * coordinates of a textured draw, the specular of an XP program, the second set of more than one unit, q of a projected layout. An
  * untextured draw copied 2.4 KB of zeros a packet of 100 vertices. */
 /* .532 (review of .530, B8): the bursts first, read once from the caller: the indexed copy below takes its stride and index count from
  * this copy, as the validation and the emitter do (they were read twice: a word changed between the reads left a copy of one layout
  * validated as the other, with words of an earlier packet in it) */
 {uint32_t nb=s->bursts.n;d->bursts.n=nb;if(nb>VX3DN_MAX_BURSTS)nb=VX3DN_MAX_BURSTS; /* v10.236 (a count above 32 is refused by the validation) */
  copy_words(d->bursts.b,s->bursts.b,nb*4);}
 d->pre.on=0;
 if(po==2){ /* v10.470: an indexed packet: its vertices and indices copied (vx3dn_validate tests the copy; the emitter uploads it) */
  uint32_t ni=d->bursts.n?d->bursts.b[0]&0xFFFFUL:0;const uint32_t vb=d->bursts.n&&(d->bursts.b[0]&VX3DN_IX_V8)?32UL:40UL; /* .524: an 8-word packet's 32 bytes a vertex (was 40 for both: a
   * fifth more read than sent; the stride read once here, the validation and the emitter go by the copy's word, so a word changed meanwhile
   * only leaves this copy's own words there) */
  if(ni>VX3DN_MAX_INDICES)ni=VX3DN_MAX_INDICES;
  d->pre.on=2;
  copy_words(d->pre.w,s->pre.w,n*vb);copy_words(d->vertices,s->vertices,(ni*2+3)&~3UL);
  vx3dn_pre_stat[3]+=(sb+n*vb+ni*2+15)>>4;goto runs;}
 if(po){ /* v10.441: prepacked vertices: the 10-word form copied when this emitter sends it, else unpacked into the arrays */
  extern int vx3dn_pre_ok(const vx3dn_packet *);
  if(vx3dn_pre_ok(d)){d->pre.on=1;vx3dn_pre_stat[0]++;
   if(cp_defer&&vx3dn_onecopy&&n<=VX3DN_MAX_VERTICES){vx3dn_vsrc=s->pre.w;vx3dn_vsrc_pkt=d;vx3dn_pre_stat[2]++;} /* v10.449: the emitter copies and checks them */
   else copy_words(d->pre.w,s->pre.w,n*40);}
  else{uint32_t v;const uint32_t *w=s->pre.w;vx3dn_pre_stat[1]++;
   for(v=0;v<n;v++,w+=10){d->vertices[v].x=w[0];d->vertices[v].y=w[1];d->vertices[v].z=w[2];d->vertices[v].rhw=w[3];d->vertices[v].color=w[4];
    d->shader.spec[v]=w[5];d->tex.uv[v][0]=w[6];d->tex.uv[v][1]=w[7];d->stages.uv1[v][0]=w[8];d->stages.uv1[v][1]=w[9];d->stages.q[v]=0;d->stages.r1[v]=0;}}
  vx3dn_pre_stat[3]+=(sb+(vx3dn_vsrc_pkt==d?0:n*40)+15)>>4;
  goto runs;}
 copy_words(d->vertices,s->vertices,n*sizeof(vx3dn_vertex));
 vx3dn_pre_stat[3]+=(sb+n*(20+(d->tex.enable?8:0)+(d->shader.count?4:0)+(d->tex.enable&&d->stages.units>1?8:0))+15)>>4; /* v10.443 (q, r1 not counted) */
 if(d->tex.enable)copy_words(d->tex.uv,s->tex.uv,n*8);
 if(d->shader.count)copy_words(d->shader.spec,s->shader.spec,n*4);
 if(d->tex.enable&&d->stages.units>1)copy_words(d->stages.uv1,s->stages.uv1,n*8);
 if(d->stages.layout||(d->stages.s3&1))copy_words(d->stages.q,s->stages.q,n*4);
 if(d->stages.s3&2)copy_words(d->stages.r1,s->stages.r1,n*4); /* v10.321: TextureB's third coordinate */
runs: /* .532: the bursts were copied above */
 n=s->runs.n;d->runs.n=n;if(n>VX3DN_TEXRUNS_MAX)n=VX3DN_TEXRUNS_MAX; /* .528: the texture runs (the count read once; above the most, refused by the validation) */
 if(n)copy_words(d->runs.r,s->runs.r,n*sizeof(vx3dn_texrun));
}
static void copy_packet(vx3dn_packet *d,const vx3dn_packet *s) {copy_packet_d(d,s,0);}
static void fx_prefetch_packet(const vx3dn_packet *s) { /* v10.412: what copy_packet reads (the flags read here are only hints) */
 uint32_t k,n=s->vertex_count;
 if(n>VX3DN_MAX_VERTICES)n=VX3DN_MAX_VERTICES;
 vx3dn_pf_stat[0]++;
 for(k=0;k<7;k++)pf_range((const char*)s+fx_ranges[k][0],fx_ranges[k][1]-fx_ranges[k][0]);
 if(s->pre.on){uint32_t vb=s->bursts.n&&(s->bursts.b[0]&VX3DN_IX_V8)?32UL:40UL; /* .532 (review of .530, M1): an 8-word packet's 32 bytes a vertex, as copy_packet_d reads */
  if(s->vertex_count<=VX3DN_IX_VERTICES)n=s->vertex_count;pf_range(s->pre.w,n*vb);k=s->bursts.n;if(k>VX3DN_MAX_BURSTS)k=VX3DN_MAX_BURSTS;pf_range(&s->bursts,4+k*4);return;} /* v10.441 */
 pf_range(s->vertices,n*sizeof(vx3dn_vertex));
 if(s->tex.enable)pf_range(s->tex.uv,n*8);
 if(s->shader.count)pf_range(s->shader.spec,n*4);
 if(s->tex.enable&&s->stages.units>1)pf_range(s->stages.uv1,n*8);
 if(s->stages.layout||(s->stages.s3&1))pf_range(s->stages.q,n*4);
 if(s->stages.s3&2)pf_range(s->stages.r1,n*4);
 k=s->bursts.n;if(k>VX3DN_MAX_BURSTS)k=VX3DN_MAX_BURSTS;pf_range(&s->bursts,4+k*4);
}
static int fx_same_state(const vx3dn_packet *p) {
 uint32_t k;
 if(!vx3dn_state_cache||!fx_prev_ok||fx_prev_gen!=vx3dn_state_gen)return 0;
 for(k=0;k<7;k++) /* v10.234: dword compare (the library memcmp went byte by byte); v10.236: repe cmpsd */
  if(vx_cmpsd((const char*)p+fx_ranges[k][0],(const char*)&fx_prev+fx_ranges[k][0],(fx_ranges[k][1]-fx_ranges[k][0])>>2))return 0;
 return 1;
}
/* v10.429 (diagnostic, for a per-group state cache): what differed from the last packet when the whole-state cache missed, every 8th miss:
 * [mask] counts by which groups differed (bit 0 T textures: the units' surfaces, filters, addressing, mip chains, cube faces, borders;
 * bit 1 G target, depth, clip; bit 2 R raster: Z, blend, alpha test, write mask, fog, stencil, dither, stipple; bit 3 P program and
 * layout: shader words, the texture-unit headers, units, coordinate sets); [16] the state was forgotten (an engine event, a clear in the
 * ring, a Lock: everything goes again); [17] misses sampled. TEXDBG BC 9B-9D (VXD3DMON). */
uint32_t vx3dn_miss_stat[18],vx3dn_miss_n;
#define FXF(a) {offsetof(vx3dn_packet,a),sizeof(((vx3dn_packet*)0)->a)}
static const uint16_t fx_cls_t[][2]={FXF(tex.surface),FXF(tex.filter),FXF(tex.address_u),FXF(tex.address_v),FXF(tex.mode),FXF(mip),FXF(stages.unit),FXF(stages.face),
 FXF(stages.umip),FXF(stages.ubias),FXF(stages.ulevel),FXF(stages.border),FXF(stages.uface)};
static const uint16_t fx_cls_g[][2]={FXF(generation),FXF(target),FXF(depth),FXF(clip),FXF(op)};
static const uint16_t fx_cls_r[][2]={FXF(z_enable),FXF(z_write),FXF(z_func),FXF(blend),FXF(write_mask),FXF(raster),FXF(fog),FXF(ext),FXF(stipple)};
static const uint16_t fx_cls_p[][2]={FXF(tex.enable),FXF(shader.count),FXF(shader.words),FXF(shader.konst),FXF(shader.kflags),FXF(shader.key),FXF(stages.units),FXF(stages.set),
 FXF(stages.c22),FXF(stages.ps),FXF(stages.h90),FXF(stages.h91),FXF(stages.h93),FXF(stages.h95),FXF(stages.ntau),FXF(stages.tau),FXF(stages.nkc),FXF(stages.kc_reg),FXF(stages.kc),
 FXF(stages.layout),FXF(stages.ulayout),FXF(stages.s3)};
static int fx_cls_diff(const vx3dn_packet *p,const uint16_t (*f)[2],uint32_t n) {
 uint32_t k;for(k=0;k<n;k++)if(vx_cmpsd((const char*)p+f[k][0],(const char*)&fx_prev+f[k][0],f[k][1]>>2))return 1;return 0;}
static void fx_classify(const vx3dn_packet *p) {
 uint32_t m;vx3dn_miss_stat[17]++;
 if(!fx_prev_ok||fx_prev_gen!=vx3dn_state_gen){vx3dn_miss_stat[16]++;return;}
 m=(fx_cls_diff(p,fx_cls_t,sizeof(fx_cls_t)/4)?1:0)|(fx_cls_diff(p,fx_cls_g,sizeof(fx_cls_g)/4)?2:0)|(fx_cls_diff(p,fx_cls_r,sizeof(fx_cls_r)/4)?4:0)|(fx_cls_diff(p,fx_cls_p,sizeof(fx_cls_p)/4)?8:0);
 vx3dn_miss_stat[m]++;
}
/* v10.450 (ACCEL_D3D_TEXONLY, default 1; 0 = as before): a state-cache miss whose packet differs from the last packet only in its textures
 * (the fields of fx_cls_t: the units' surfaces, filters, addressing, mip chains, cube faces, borders; board, GTA3 v10.449: 60% of the
 * misses) sends the texture cache strobe and the texture units' words only (vx3dn_tex_only): setup()'s other words, the program and the
 * layout depend on none of those fields, and the engine holds the last packet's. The compare covers every byte the whole-state cache
 * compares (fx_ranges) but the texture fields' (fx_rest, worked out once). vx3dn_texonly_stat: [0] packets sent so, [1] misses seen. */
volatile uint32_t vx3dn_texonly=1;uint32_t vx3dn_texonly_stat[2];
extern uint32_t vx3dn_tex_only;
static uint16_t fx_rest[48][2];static int fx_rest_n=-1;
static void fx_rest_make(void) {
 uint32_t r,k,n=sizeof(fx_cls_t)/4;fx_rest_n=0;
 for(r=0;r<7;r++){uint32_t cur=fx_ranges[r][0],end=fx_ranges[r][1];
  for(;;){uint32_t best=end,bend=end;
   for(k=0;k<n;k++){uint32_t a=fx_cls_t[k][0],b=a+fx_cls_t[k][1];if(b>cur&&a<best){best=a<cur?cur:a;bend=b;}} /* the next texture field at or after cur */
   if(best>end)best=end;
   if(best>cur&&fx_rest_n<48){fx_rest[fx_rest_n][0]=(uint16_t)cur;fx_rest[fx_rest_n][1]=(uint16_t)(best-cur);fx_rest_n++;}
   if(best>=end)break;
   cur=bend>end?end:bend;if(cur>=end)break;}}
}
static int fx_tex_only(const vx3dn_packet *p) {
 int k;
 if(!vx3dn_texonly||!vx3dn_state_cache||!fx_prev_ok||fx_prev_gen!=vx3dn_state_gen)return 0;
 if(p->cb!=sizeof(*p)||p->op!=VX3DN_DRAW||!p->tex.enable)return 0;
 if(fx_rest_n<0)fx_rest_make();
 vx3dn_texonly_stat[1]++;
 for(k=0;k<fx_rest_n;k++)if(vx_cmpsd((const char*)p+fx_rest[k][0],(const char*)&fx_prev+fx_rest[k][0],fx_rest[k][1]>>2))return 0;
 return 1;
}
static uint32_t fx_inflight,fx_t0; /* v10.285: fx_t0 = when the draw in flight was started */
/* v10.448 (ACCEL_D3D_FASTCLOCK, default 1; 0 = Get_System_Time as before): the kick's time stamp (the recorder's entry and fx_t0, the in-flight
 * group's start for the 100 ms limits) read from the VMM's own system-time word (Get_System_Time_Address, the address taken once), not
 * through Get_System_Time, which on Windows 98 also reads the timer chip for the time since the last tick (port I/O, microseconds). The
 * word is the time at the last tick: at most a tick early, so a limit counted from it lasts at most a tick longer. */
volatile uint32_t vx3dn_fastclock=1;static volatile DWORD *xr_stp;
static uint32_t xr_now(void) {if(vx3dn_fastclock){if(!xr_stp)xr_stp=Get_System_Time_Address();if(xr_stp)return *xr_stp;}return Get_System_Time();}
/* v10.291 (board, v10.290 at 16 bpp: VXSTEN's run lost about 32 stencil draws with 2 groups not finished, and VXDX7's draw after the
 * hang recovery hung too): what each packet of a group was, kept for the group in flight, and for the last group that did not finish
 * (vx3dn_hang, TEXDBG 0xE0 + n reads packet n). Per packet: [0] op | target format << 8 | depth format << 16 | vertices (255 at most)
 * << 24; [1] z enable | z write << 1 | z func << 4 | blend << 8 | textured << 9 | units << 10 | primitive << 12 | dither << 16 |
 * stencil << 17 | alpha func << 20 | program instructions << 24; [2] stencil word 1; [3] stencil word 2; [4] write mask; [5] its words */
static uint32_t fx_sum[32][6],fx_nsum,fx_fly[32][6],fx_nfly; /* v10.365: 32 (a batch's packets) */
uint32_t vx3dn_hang[16][6],vx3dn_hang_n,vx3dn_hang_count,vx3dn_hang_status;
static void fx_note(const vx3dn_packet *p,uint32_t words) {
 uint32_t *d;if(fx_nsum>=32)return;d=fx_sum[fx_nsum++];
 d[0]=(p->op&0xFF)|((p->target.format&0xFF)<<8)|((p->depth.format&0xFF)<<16)|((p->vertex_count>255?255:p->vertex_count)<<24);
 d[1]=(p->z_enable&1)|((p->z_write&1)<<1)|((p->z_func&15)<<4)|((p->blend&1)<<8)|((p->tex.enable&1)<<9)|((p->stages.units&3)<<10)|((p->ext.prim&15)<<12)|
  ((p->ext.dither&1)<<16)|((p->ext.stencil&3)<<17)|((p->raster.alpha_func&15)<<20)|((p->shader.count&0xFF)<<24);
 d[2]=p->ext.stencil_w1;d[3]=p->ext.stencil_w2;d[4]=p->write_mask;d[5]=words;
}
static void hang_note(uint32_t st,uint32_t (*from)[6],uint32_t n) {
 uint32_t k,j;vx3dn_hang_count++;vx3dn_hang_status=st;vx3dn_hang_n=n>16?16:n;
 for(k=0;k<vx3dn_hang_n;k++)for(j=0;j<6;j++)vx3dn_hang[k][j]=from[k][j];
}
static int fetch_idle(uint32_t *last);
/* v10.406: groups go to XP's ring in system memory (xr_ below, ACCEL_D3D_XPRING); without it, or after a hang in it, each group is
 * fetched one-shot from the command block in video memory. The ring kept in video memory (v10.374-v10.405) and its workarounds are
 * gone: it never ran GTA3 without hanging. vx3dn_gap_wipe (ACCEL_D3D_GAPWIPE, default 1): before a one-shot start the words between
 * the fetcher's last position and the start are written over with no-ops (a start runs from the old position first, VXRING 43). */
volatile uint32_t vx3dn_gap_wipe=1;
static int fx_ring,fx_gring;uint32_t vx3dn_ring_stat[3]; /* XP's ring: starts, kicks, wraps */
/* v10.383 (board, v10.382 ring on: 85 groups never idle in 98 s; advice: the first hang matters, the later ones may follow from it):
 * the ring's fence and its recorder. Each ring group ends with a 1-pixel 2D fill (VXRING: 2D fills in the ring draw, mixed with 3D) into
 * the command block's last 256 bytes (kept out of the ring), colour = the group's sequence number (16 bits): the last group the
 * fetcher consumed. fx_retire in the ring waits for the engine idle and the fence at the last group kicked (ACCEL_D3D_RINGFENCE, default
 * 1); the 2D registers the fences write are put back after each stop. The last 64 groups kicked are kept; at the first hang the
 * status, register 440, the fence, the groups and the first 48 words of the group after the fence are kept (TEXDBG 0xBC). After a
 * hang in the ring the ring is off until restart: the one-shot path goes on. */
#define FX_BLOCK_BYTES (VX3DN_CMD_BYTES-256UL)
#define FX_FENCE_OFF (vx3dn_cmd_off+FX_BLOCK_BYTES)
volatile uint32_t vx3dn_ring_fence=1;
uint32_t vx3dn_rf_stat[6]; /* v10.387: [4] kicks written again, [5] of them the fence then came */ /* fences put; retires with the engine idle before the fence came (then waited for); hangs in the ring; ring turned off after a hang */
static int fx_ring_dead,fx_rfenced;uint32_t fx_seq; /* .532: public (the emulator's B1 test reads it) */static uint32_t fx_rkicks,fx_rreason,fx_2d[14],fx_trace[64][10],fx_tn,fx_gtgt,fx_gdep;
static const uint32_t fx_2d_reg[14]={0x04,0x08,0x14,0x1C,0x30,0x48,0x58,0x5C,0x20,0x4C,0x0C,0x10,0x40,0x18}; /* v10.453: + 18, the source position of the ring's copies (ACCEL_D3D_RINGBLT) */
uint32_t vx3dn_rhang[2][6],vx3dn_rhang_trace[64][10],vx3dn_rhang_words[1024],vx3dn_rhang_wn[4],vx3dn_rhang_ok; /* v10.386: [0-511] the group after the fence, [512-1023] the fence's group; wn: their words and starts */
static uint32_t fx_fence_read(void) {uint32_t a=vx_status.fb_linear+FX_FENCE_OFF;return hda&&hda->bpp==16?*(volatile uint16_t*)a:(*(volatile uint32_t*)a&0xFFFFUL);}
static void fx_2d_back(void) {uint32_t k;if(!fx_rfenced)return;for(k=0;k<14;k++)write_mmio(0,fx_2d_reg[k],fx_2d[k]);fx_rfenced=0;}
static int fx_2d_dirty;
static void fx_2d_restore(void) {uint32_t k;if(!fx_rfenced)return;for(k=0;k<14;k++)write_mmio(0,fx_2d_reg[k],fx_2d[k]);fx_2d_dirty=1;}
static void fx_park_forget(void) {fx_ring=0;fx_2d_dirty=0;if(xr_state==2)xr_state=1;}
#include "vx_ring.h"
uint32_t vx3dn_xr_unit[VX_XR_UNIT_N]; /* .504: the ring's cache work by unit (vx_ring.h) */
/* v10.393: the ring tape (vx_ring.h): the hung ring's words and groups, kept at the first hang for VXRING's replay (op 11) */
static vx_tape tape;
/* v10.394: what the driver did around the ring, the last 128 events (kept in the tape at the first hang): 1 kick (fence number), 2 ring
 * start, 3 park, 4 kick from a park, 5 VX3DN_busy, 6 VX3DN_poll that retired, 7 single submit (op), 8 batch (packets), 9 stop, 10 fence
 * late, 11 the fences' 2D registers put back, 12 flip (vx855_flip.c), 13 2D submit (op, vx855_2d.c), 14 cursor move, 15 Lock or CPU
 * access (VX2D_access), 16 the engine idle wait done (polls / 16) */
static uint32_t ev_ring[128][2],ev_n;
void VX3DN_ev(uint32_t type,uint32_t arg) {uint32_t k=ev_n++&127;ev_ring[k][0]=cs_tsc();ev_ring[k][1]=type|(arg<<8);}
static void tape_capture(uint32_t st,uint32_t f) {
 uint32_t n=fx_rkicks<64?fx_rkicks:64,k,first,end,s0,j;
 if(n>fx_tn)n=fx_tn;if(!n||!vx_status.fb_linear)return;
 end=fx_trace[(fx_tn-1)&63][2]+0x10;
 for(k=0;k<n;k++){uint32_t *e=fx_trace[(fx_tn-n+k)&63];if(end-e[1]<=VX_TAPE_WORDS*4UL&&(fx_in_ring(e[1])?fx_in_ring(end-4):e[1]>=vx3dn_cmd_off+256UL))break;} /* the earliest group that fits */
 if(k==n)return;first=fx_tn-n+k;n-=k;s0=fx_trace[first&63][1];
 memset(&tape,0,sizeof(tape));tape.magic=VX_TAPE_MAGIC;tape.version=1;tape.captured=1;tape.cmd_off=vx3dn_cmd_off;tape.block_end=vx3dn_cmd_off+FX_BLOCK_BYTES;
 if(fx_in_ring(s0)){tape.cmd_off=XR_GPU;tape.block_end=XR_GPU+XR_BYTES;} /* v10.402: XP's ring (flags 8): its GPU addresses */
 tape.start=s0;tape.words=(end-s0)/4;tape.ngroups=n;tape.bpp=hda?hda->bpp:0;tape.status=st;tape.r440=read_mmio(0,0x440);tape.fence=f;tape.boot_ms=Get_System_Time();
 tape.ring_kicks=fx_rkicks;tape.flags=(k?1UL:0)|(fx_rkicks>n?2UL:0)|(fx_rfenced?4UL:0)|(fx_in_ring(s0)?8UL:0);tape.fence_off=FX_FENCE_OFF; /* 1: not from the first group kept, 2: not from the ring's first group, 4: fenced */
 for(j=0;j<n;j++){uint32_t *e=fx_trace[(first+j)&63];tape.group[j][0]=e[1];tape.group[j][1]=e[2];tape.group[j][2]=e[3];tape.group[j][3]=e[0]&0xFFFFUL;}
 for(j=0;j<tape.words;j++)tape.w[j]=fx_rd(s0+j*4);
 if(hda){tape.mode_w=hda->width;tape.mode_h=hda->height;tape.mode_pitch=hda->pitch;}
 {uint32_t m=ev_n<128?ev_n:128;tape.nev=m;tape.ev_tsc_hang=cs_tsc();for(j=0;j<m;j++){tape.ev[j][0]=ev_ring[(ev_n-m+j)&127][0];tape.ev[j][1]=ev_ring[(ev_n-m+j)&127][1];}}
}
uint32_t VX3DN_tape(uint32_t op,const void *in,uint32_t in_bytes,void *out,uint32_t out_bytes) { /* 0 read (out: vx_tape), 1 put one back */
 if(op==0){uint32_t n=out_bytes<sizeof(tape)?out_bytes:sizeof(tape);if(!out||n<(uint32_t)((char*)&tape.nev-(char*)&tape))return 87;cs_enter();memcpy(out,&tape,n);cs_leave();return 0;} /* v10.396: an older program's shorter tape too */
 if(op==1){const vx_tape *t=(const vx_tape*)in;uint32_t j;
  if(!t||in_bytes<(uint32_t)((const char*)&t->nev-(const char*)t)||t->magic!=VX_TAPE_MAGIC||t->cmd_off!=vx3dn_cmd_off||!vx3dn_cmd_off||t->words>VX_TAPE_WORDS||!t->ngroups||t->ngroups>VX_TAPE_GROUPS||
     t->start<vx3dn_cmd_off+256UL||t->start+t->words*4>vx3dn_cmd_off+FX_BLOCK_BYTES)return 13;
  for(j=0;j<t->ngroups;j++)if(t->group[j][0]<t->start||t->group[j][1]+0x10>t->start+t->words*4||t->group[j][1]<t->group[j][0])return 13;
  cs_enter();memset(&tape,0,sizeof(tape));memcpy(&tape,t,in_bytes<sizeof(tape)?in_bytes:sizeof(tape));tape.captured=2;cs_leave();return 0;}
 return 1;
}
static void fx_ring_hang(uint32_t st) {
 uint32_t k,f=fx_rfenced?fx_fence_read():0xFFFFFFFFUL;
 vx3dn_rf_stat[2]++;
 if(!vx3dn_rhang_ok){uint32_t n=fx_tn<64?fx_tn:64,first=fx_tn-n,start=0xFFFFFFFFUL;
  vx3dn_rhang_ok=1;
  vx3dn_rhang[0][0]=st;vx3dn_rhang[0][1]=read_mmio(0,0x440);vx3dn_rhang[0][2]=f;vx3dn_rhang[0][3]=fx_seq&0xFFFFUL;vx3dn_rhang[0][4]=Get_System_Time()-fx_t0;vx3dn_rhang[0][5]=fx_rkicks;
  vx3dn_rhang[1][0]=vx3dn_ring_stat[0];vx3dn_rhang[1][1]=vx3dn_ring_stat[2];vx3dn_rhang[1][2]=Get_System_Time();vx3dn_rhang[1][3]=fx_rreason;vx3dn_rhang[1][4]=vx3dn_rf_stat[1];vx3dn_rhang[1][5]=fx_tn;
  memset(vx3dn_rhang_trace,0,sizeof(vx3dn_rhang_trace));memset(vx3dn_rhang_words,0,sizeof(vx3dn_rhang_words));memset(vx3dn_rhang_wn,0,sizeof(vx3dn_rhang_wn));
  {uint32_t hw=0,ps=0xFFFFFFFFUL,pw=0;
  for(k=0;k<n;k++){uint32_t j,*e=fx_trace[(first+k)&63];for(j=0;j<10;j++)vx3dn_rhang_trace[k][j]=e[j];
   if(f!=0xFFFFFFFFUL&&(e[0]&0xFFFFUL)==((f+1)&0xFFFFUL)){start=e[1];hw=e[3];}
   if(f!=0xFFFFFFFFUL&&(e[0]&0xFFFFUL)==(f&0xFFFFUL)){ps=e[1];pw=e[3];}}
  if(start==0xFFFFFFFFUL&&n){start=fx_trace[(fx_tn-1)&63][1];hw=fx_trace[(fx_tn-1)&63][3];}
  if(hw>512)hw=512;if(pw>512)pw=512;
  for(k=0;k<hw;k++)if(fx_in_ring(start)?fx_in_ring(start+k*4):start+k*4+4<=FX_FENCE_OFF)vx3dn_rhang_words[k]=fx_rd(start+k*4);
  for(k=0;k<pw;k++)if(ps!=0xFFFFFFFFUL&&(fx_in_ring(ps)?fx_in_ring(ps+k*4):ps+k*4+4<=FX_FENCE_OFF))vx3dn_rhang_words[512+k]=fx_rd(ps+k*4);
  vx3dn_rhang_wn[0]=hw;vx3dn_rhang_wn[1]=start;vx3dn_rhang_wn[2]=pw;vx3dn_rhang_wn[3]=ps;}
  tape_capture(st,f);} /* v10.393 */
 if(!fx_ring_dead){fx_ring_dead=1;vx3dn_rf_stat[3]++;}
}
/* v10.402 (the VRAM ring's hang never reproduced and the engine reset does not bring the chip all the way back; XP runs its ring in
 * system memory, CLXP3D: register 60 bit 0 set, 43C/444 live, the page table in video memory): XP's ring interface, rebuilt from
 * s3ginv.dll and S3gIGPm.sys. ACCEL_D3D_XPRING (default 1); 0 = the ring in video memory as before.
 * - The page table (S3gIGPm.sys 0x320A0, 0x31E60): 64 KB at vx3dn_gart_off in video memory (vxd_main.c), SR6A = offset >> 12, SR6B =
 *   offset >> 20, SR6C bit 0 = offset >> 28, entries page frame | 0x01000000, unused entries 0x80000000, SR6F |= 0x80 after the entries
 *   change. Entries 0x100-0x13F: 64 fixed pages of system memory (256 KB ring at GPU address 0x100000); 0xFF and 0x140-0x143 a guard
 *   page of no-op blocks.
 * - The start (s3ginv 0xBB370): with the fetcher left paused in the ring (444 bit 31) its read position (43C) gets XP's batch end and
 *   CCC00001/DDD00001 pairs to 256 bytes, run with the 0x200 pause and waited for; then the start block (batch end, CCC00002/DDD00002
 *   pairs to 256 bytes) at the ring's start, 60 = start | 1, 61, 62/63 = the ring's end - 0x10, 64/65 = the block's end - 0x10, 66/67 =
 *   FFFFF0/FF, 68 = 0A0001. The first start checks the fetcher reached its pause in the ring (43C); if not, the ring goes back to video memory.
 * - The kick (0xBC6CE): batch end, CCC00004 padding to 256 bytes, 0x41C, 64/65 = the end - 0x10.
 * - The wrap (0xBBC10): batch end and CCC00003 to the wrap point a block before the ring's end, 66/67 = that point - 0x10, 64/65 = the
 *   ring's start - 0x10.
 * - The ring is not stopped between groups, nor at a wrap: only before the fetcher is driven otherwise (one-shot, register path, queries,
 *   VXRING, a mode set) and after a hang (68 = 0A0000, as XP's engine initialisation 0x110EA0); the next group starts it again as above.
 * - The CPU's writes reach memory before each register write that lets the fetcher read them (CLFLUSH, else WBINVD).
 * vx3dn_xr_stat (TEXDBG 0xBC 0x86-0x91): [0] state + 1, [1] why off (VX_XR_OFF_*, vx_ring.h: 1 no table block, 2 allocation, 3 page
 * entries, 4 no pause in the ring at the first start, 5 the translation-cache flush failed), [2] starts, [3] of them from a paused fetcher (43C), [4] those not idle after it, [5] kicks, [6] wraps,
 * [7] stops, [8] the fetcher found stopped (444 bit 31 clear) with nothing in flight, [9] table or SR6A-6C written again, [10] not
 * written since .504 (it held CLFLUSH lines, WBINVD calls or MOVNTI words by mode: now vx3dn_xr_unit, vx_ring.h), [11] flush form (1 CLFLUSH + MFENCE, 2 WBINVD, 3 CLFLUSH) | line size << 8, [12] waits done with 0x400
 * bit 4 set at the pause, [13] hangs, [14] 43C, [15] 444 at the last hang, [16] SR68 | SR6A << 8 | SR6B << 16 | SR6C << 24 before,
 * [17] SR6D | SR6E << 8 | SR6F << 16 before, [18] SR6A | SR6B << 8 | SR6C << 16 | SR6F << 24 after, [19] polls for SR6F bit 7 to clear
 * (0xFFFFFFFF: it stayed set), [20] 400, [21] 43C, [22] 444 at the first start's pause, [23] the table's offset, [24] 440, [25] 45C
 * (pause read back), [26] 464 (start read back) at the first start's pause, [27] 440, [28] 45C at the last hang, [29] where its group
 * ended; [30-59] the first start's forms, 5 each: 400 | form << 28, 43C, 444, 45C, 464; [60] the form kept (0xFF none), [61] forms
 * tried; [62-67] 43C twice, 444, 45C, 464, 400 before the last first kick after a wrap; [68] such groups done without a restart,
 * [69] their kicks written again, [70] those never done, [71] done after the kick was written again. [8] counts 444 bit 31 read clear
 * with nothing in flight (v10.405: no longer a restart, XP has none). 43C is read until two reads agree (VIA's Linux driver: a single read is sometimes wrong). A first start
 * that does not reach its pause in the ring is tried once more with SR6C bit 7 (the table's write protection) set after the entries. */
extern uint32_t sv_wait_pub;static uint32_t sv_wait;
unsigned long fx_cpuid1_ebx(void);
#pragma aux fx_cpuid1_ebx = 0xB8 0x01 0x00 0x00 0x00 0x0F 0xA2 0x89 0xD8 value [eax] modify exact [eax ebx ecx edx];
void xr_clflush(unsigned long a);
#pragma aux xr_clflush = 0x0F 0xAE 0x38 parm [eax] modify exact [];
void xr_mfence(void);
#pragma aux xr_mfence = 0x0F 0xAE 0xF0 modify exact [];
void xr_wbinvd(void);
#pragma aux xr_wbinvd = 0x0F 0x09 modify exact [];
static uint32_t xr_alloc,xr_pte[XR_PAGES+1],xr_ls=64;static int xr_flushmode;
/* v10.466 (review): xr_rp_ok says whether two reads agreed; after 8 that did not, the value is not taken as the fetcher's position */
static uint32_t xr_rp_ok(int *ok) {uint32_t a=read_mmio(0,0x43C),b,n;*ok=0;for(n=0;n<8;n++){b=read_mmio(0,0x43C);if(a==b){*ok=1;break;}a=b;}return a;}
static uint32_t xr_rp(void) {int ok;return xr_rp_ok(&ok);}
static void xr_off(uint32_t why) {xr_state=-1;vx3dn_xr_stat[0]=0;vx3dn_xr_stat[1]=why;}
/* The ring's entries keep bit 25 (snoop) clear: the engine reads memory, not the CPU's caches (board, v10.408). */
/* v10.411 (board, v10.410: the flush 7.5 of 60 microseconds a packet, 15.4 a kick): ACCEL_D3D_XPRING_NT (default 1, needs SSE2): a group
 * is built in a cached staging buffer (xr_stage) and copied into the ring with MOVNTI (non-temporal stores, past the CPU's caches), then
 * one SFENCE before the kick's register writes; no ring line is written both ways (the wrap padding, start block and restart pad keep
 * their stores and the flush, in lines of their own). 0 = groups written in the ring and flushed (CLFLUSH) as in v10.410. */
volatile uint32_t vx3dn_xr_nt=1;
static int xr_nt_ok=-1; /* -1 not known, 0 no SSE2, 1 */
static uint32_t xr_stage[80UL*1024UL/4UL+64UL];
void xr_nt_copy(void *d,const void *s,uint32_t n);
#pragma aux xr_nt_copy = 0x8B 0x06 0x0F 0xC3 0x07 0x83 0xC6 0x04 0x83 0xC7 0x04 0x49 0x75 0xF2 parm [edi] [esi] [ecx] modify exact [eax esi edi ecx];
/* v10.412: the copy four words a step (MOVNTI [edi + 4k]) */
void xr_nt4(void *d,const void *s,uint32_t n4);
#pragma aux xr_nt4 = 0x8B 0x06 0x0F 0xC3 0x07 0x8B 0x46 0x04 0x0F 0xC3 0x47 0x04 0x8B 0x46 0x08 0x0F 0xC3 0x47 0x08 0x8B 0x46 0x0C 0x0F 0xC3 0x47 0x0C \
 0x83 0xC6 0x10 0x83 0xC7 0x10 0x49 0x75 0xDD parm [edi] [esi] [ecx] modify exact [eax esi edi ecx];
static void xr_copy(uint32_t *d,const uint32_t *s,uint32_t n) {
 if(n>=4){xr_nt4(d,s,n>>2);d+=n&~3UL;s+=n&~3UL;n&=3;}
 if(n)xr_nt_copy(d,s,n);
}
static int xr_use_nt(void) {
 if(xr_nt_ok<0)xr_nt_ok=fx_cpuid_ok()&&((fx_cpuid1_edx()>>26)&1)?1:0;
 return vx3dn_xr_nt&&xr_nt_ok;
}
static void xr_flush(uint32_t lo,uint32_t hi) { /* ring offsets lo..hi out of the CPU's caches */
 if(!xr_flushmode){unsigned long d=fx_cpuid_ok()?fx_cpuid1_edx():0;
  if(d&(1UL<<19)){xr_ls=((fx_cpuid1_ebx()>>8)&0xFFUL)*8UL;if(xr_ls<32||xr_ls>256||(xr_ls&(xr_ls-1)))xr_ls=32;xr_flushmode=(d&(1UL<<26))?1:3;}else xr_flushmode=2;
  vx3dn_xr_stat[11]=(uint32_t)xr_flushmode|(xr_ls<<8);}
 if(xr_flushmode==2){unsigned long f=VX_irq_save();xr_wbinvd();VX_irq_restore(f);vx3dn_xr_unit[VX_XR_UNIT_WBINVD]++;return;}
 {uint32_t a=(xr_lin+lo)&~(xr_ls-1),e=xr_lin+hi;for(;a<e;a+=xr_ls){xr_clflush(a);vx3dn_xr_unit[VX_XR_UNIT_LINES]++;}}
 if(xr_flushmode==1)xr_mfence();else publish();
}
/* v10.403 (board, v10.402: both first starts stayed idle, 43C 0, 444 000801FF): the first start tries the forms below in turn until
 * one reaches its pause in the ring, and keeps it; xr_variant bits: 1 entries without bit 24 (the 3D lab's form), 2 SR6C bit 7 set
 * (the lab's), 4 the command regulator's virtual queue on in XP's AGP form (s3ginv 0x110EF0: 70-73 a 64 KB queue in video memory above
 * the table, 74 = 101005), 8 the lab's regulator claim (type 10: 74 = 301000, 7C, 00, 02, 07 = 0, 68 = 0A0000; type 11: 00 = 248).
 * Every start is preceded by XP's engine initialisation writes (0x110E86: 68 = 0A0000, then 02 = 0). */
#define XR_VQ_BYTES 0x10000UL
static const unsigned char xr_forms[6]={0,4,0x0B,7,1,2};static uint32_t xr_variant,xr_try;
static uint32_t xr_entry(uint32_t k) {return ((xr_pte[k]>>12)&0xFFFFFUL)|((xr_variant&1)?0:0x01000000UL);}
static void xr_cr(uint32_t type,const uint32_t *w,uint32_t n) {uint32_t k;write_mmio(0,0x41C,type<<16);for(k=0;k<n;k++)write_mmio(0,0x420,w[k]);}
static void xr_vq(int on) {
 uint32_t a=vx3dn_gart_off+0x10000UL,e=a+XR_VQ_BYTES-1,w[6];
 if(on){w[0]=0x70000000UL|(a&0xFFFFFFUL);w[1]=0x71000000UL|(e&0xFFFFFFUL);w[2]=0x72000000UL|(a>>24)|((e>>24)<<8);w[3]=0x73000000UL|((XR_VQ_BYTES-0x400UL)>>4);w[4]=0x74101005UL;w[5]=0;xr_cr(0x10,w,6);}
 else {w[0]=0x74301000UL;w[1]=0;xr_cr(0x10,w,2);}
}
static void xr_claim(void) {static const uint32_t a[6]={0x74301000UL,0x7C000000UL,0,0x02000000UL,0x07000000UL,0x680A0000UL},b[1]={0x00000248UL};xr_cr(0x10,a,6);xr_cr(0x11,b,1);}
static void xr_table(void) { /* SR6C bit 7 (the table's write protection, VIA's Linux driver clears it while it writes entries) clear first */
 volatile uint32_t *t=(volatile uint32_t*)(vx_status.fb_linear+vx3dn_gart_off);uint32_t k;
 {unsigned long f=VX_irq_save();unsigned char old=inp(0x3C4),unlock=sr(0x10),v;sw(0x10,(unsigned char)(unlock|1));v=sr(0x6C);if(v&0x80)sw(0x6C,(unsigned char)(v&0x7F));sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);}
 for(k=0;k<XR_ENTRIES;k++)t[k]=0x80000000UL;
 t[XR_IDX0-1]=xr_entry(XR_PAGES);for(k=0;k<XR_PAGES;k++)t[XR_IDX0+k]=xr_entry(k);for(k=0;k<4;k++)t[XR_IDX0+XR_PAGES+k]=xr_entry(XR_PAGES);
 publish();(void)t[XR_ENTRIES-1];
}
static int xr_table_ok(void) {volatile uint32_t *t=(volatile uint32_t*)(vx_status.fb_linear+vx3dn_gart_off);return t[XR_IDX0]==xr_entry(0)&&t[XR_IDX0+XR_PAGES-1]==xr_entry(XR_PAGES-1);}
static int xr_srs_ok(void) {
 unsigned long f=VX_irq_save();unsigned char old=inp(0x3C4),a,b,c;
 a=sr(0x6A);b=sr(0x6B);c=sr(0x6C);outp(0x3C4,old);VX_irq_restore(f);
 return a==((vx3dn_gart_off>>12)&0xFF)&&b==((vx3dn_gart_off>>20)&0xFF)&&(c&1)==((vx3dn_gart_off>>28)&1);
}
/* v10.426 (review M08 / A1): 1 when the TLB flush finished (SR6F bit 7 cleared within 10 ms), 0 when it timed out: the fetcher could read
 * through stale translations, so the callers do not start (or keep) the ring then (ring off, reason 5: the one-shot path) */
static int xr_srs(void) { /* S3gIGPm.sys 0x32217-0x3230E, then the TLB flush of 0x32006 */
 unsigned long f=VX_irq_save();unsigned char old=inp(0x3C4),unlock=sr(0x10),v;uint32_t t,n=0,o=vx3dn_gart_off;
 sw(0x10,(unsigned char)(unlock|1));
 sw(0x6A,(unsigned char)((o>>12)&0xFF));sw(0x6B,(unsigned char)((o>>20)&0xFF));v=sr(0x6C);sw(0x6C,(unsigned char)((v&0x7E)|((o>>28)&1)|((xr_variant&2)?0x80:0)));
 v=sr(0x6F);sw(0x6F,(unsigned char)(v|0x80));
 sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);
 t=Get_System_Time();
 for(;;){f=VX_irq_save();old=inp(0x3C4);v=sr(0x6F);outp(0x3C4,old);VX_irq_restore(f);n++;
  if(!(v&0x80)){vx3dn_xr_stat[19]=n;break;}
  if((uint32_t)(Get_System_Time()-t)>=10){vx3dn_xr_stat[19]=0xFFFFFFFFUL;break;}}
 f=VX_irq_save();old=inp(0x3C4);vx3dn_xr_stat[18]=(uint32_t)sr(0x6A)|((uint32_t)sr(0x6B)<<8)|((uint32_t)sr(0x6C)<<16)|((uint32_t)sr(0x6F)<<24);outp(0x3C4,old);VX_irq_restore(f);
 return !(v&0x80);
}
static void xr_noops(uint32_t from,uint32_t to) { /* 256-byte no-op blocks (the kick's padding form) */
 volatile uint32_t *w=(volatile uint32_t*)xr_lin;uint32_t p,k;
 for(p=from;p<to;p+=256){w[p/4]=0xFE020420UL;w[p/4+1]=0x41C;w[p/4+2]=0x00300000UL;w[p/4+3]=0xCC000000UL;for(k=4;k<64;k++)w[p/4+k]=0xCCC00004UL;}
}
static int xr_setup(void) {
 uint32_t k,j;unsigned long phys=0;
 if(xr_state)return xr_state>0;
 vx3dn_xr_stat[23]=vx3dn_gart_off;
 if(!vx3dn_gart_off||!vx3dn_cmd_off||!vx_status.fb_linear||(vx3dn_gart_off&0xFFFUL)){xr_off(VX_XR_OFF_NOTABLE);return 0;}
 xr_alloc=_PageAllocate(XR_PAGES+1,PG_SYS,0,0,0x100,0xFFFFF,&phys,PAGEFIXED|PAGEUSEALIGN);
 if(!xr_alloc||xr_alloc==0xFFFFFFFFUL||(xr_alloc&4095UL)){xr_alloc=0;xr_off(VX_XR_OFF_ALLOC);return 0;}
 _CopyPageTable(xr_alloc>>12,XR_PAGES+1,(DWORD*)xr_pte,0);
 for(k=0;k<=XR_PAGES;k++){if(!(xr_pte[k]&1)||(xr_pte[k]&0xFFFFF000UL)<0x100000UL)break;for(j=0;j<k;j++)if(!((xr_pte[k]^xr_pte[j])&0xFFFFF000UL))break;if(j<k)break;}
 if(k<=XR_PAGES){_PageFree((void*)xr_alloc,0);xr_alloc=0;xr_off(VX_XR_OFF_PAGES);return 0;}
 xr_lin=xr_alloc;xr_noops(0,XR_BYTES+4096UL);xr_flush(0,XR_BYTES+4096UL);
 {unsigned long f=VX_irq_save();unsigned char old=inp(0x3C4);
  vx3dn_xr_stat[16]=(uint32_t)sr(0x68)|((uint32_t)sr(0x6A)<<8)|((uint32_t)sr(0x6B)<<16)|((uint32_t)sr(0x6C)<<24);vx3dn_xr_stat[17]=(uint32_t)sr(0x6D)|((uint32_t)sr(0x6E)<<8)|((uint32_t)sr(0x6F)<<16);
  outp(0x3C4,old);VX_irq_restore(f);}
 xr_table();if(!xr_srs()){xr_off(VX_XR_OFF_TLBFLUSH);return 0;} /* v10.426 (review M08): the pages stay allocated (the table points at them) */
 xr_state=1;vx3dn_xr_stat[0]=2;return 1;
}
/* the fetcher at its pause with every engine idle: 1, 2 the same with 0x400 bit 4 still set (444 bit 31: paused), 0 not yet, -1 gone */
static int xr_quiet(uint32_t *st) {
 uint32_t s=read_mmio(0,0x400);*st=s;
 if(s==0xFFFFFFFFUL)return -1;
 if(s&0x101FE3UL)return 0;
 if(!(s&0x10UL))return 1;
 return (read_mmio(0,0x444)&0x80000000UL)?2:0;
}
/* v10.465 (review): ACCEL_D3D_RINGPROG (default 1; 0 = as before): a wait gave up 100 ms after it began, however much fetched work
 * the ring held (groups are kicked without waiting): a busy engine was taken for hung, the ring turned off and the engine reset. Now
 * it gives up after 100 ms with neither the fetcher's position (43C) nor the fence moving, and after 2 s in all. vx3dn_rp_stat: [0]
 * waits that went past 100 ms on progress, [1] the longest wait (ms). */
volatile uint32_t vx3dn_ringprog=1;uint32_t vx3dn_rp_stat[2],vx3dn_rp_hist[6]; /* v10.467: the waits by length: 0-1, 2-10, 11-50, 51-100, 101-500, 501+ ms */
static int xr_wait_b(uint32_t *st);
static int xr_wait(uint32_t *st) {uint32_t o=vx_smp_ph;int r;vx_smp_ph=VX_SMP_PH_WAIT;r=xr_wait_b(st);vx_smp_ph=o;return r;} /* .520 */
static int xr_wait_b(uint32_t *st) { /* the last group kicked done: the fence at it (or no fence) and the fetcher paused */
 uint32_t t0=Get_System_Time(),want=fx_seq&0xFFFFUL,n,stable=0,t2=VX_tsc(),dw,rk=0,tp=t0,lrp=0,lfe=0,ext=0;int q,r=0;
 if(vx3dn_ringprog){lrp=xr_rp();lfe=fx_rfenced?fx_fence_read():0;}
 for(n=0;;n++){q=xr_quiet(st);if(q<0)break;
  if(q>0&&(!fx_rfenced||fx_fence_read()==want)){if(++stable>=2){if(q==2){vx3dn_xr_stat[12]++;vx3dn_xr_cr_pause=1;}r=1;break;}}else stable=0;
  /* v10.405: a group after a wrap whose fence does not come with every engine idle: its kick written again once (the count tells) */
  if(xr_wrapgrp&&!rk&&q>0&&fx_rfenced&&(uint32_t)(Get_System_Time()-t0)>=3){uint32_t pz=xr_pend-0x10;rk=1;vx3dn_xr_stat[69]++;
   vx3dn_xr_stat2[3]=xr_rp();vx3dn_xr_stat2[4]=read_mmio(0,0x444);vx3dn_xr_stat2[5]=read_mmio(0,0x400);
   write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x64000000UL|(pz&0xFFFFFFUL));write_mmio(0,0x420,0x65000000UL|(pz>>24));}
  if(!(n&15)){uint32_t now=Get_System_Time();
   if(!vx3dn_ringprog){if((uint32_t)(now-t0)>=100)break;}
   else if((uint32_t)(now-tp)>=100){uint32_t rp=xr_rp(),fe=fx_rfenced?fx_fence_read():0;
    if((uint32_t)(now-t0)<2000UL&&(rp!=lrp||fe!=lfe)){if(!ext){ext=1;vx3dn_rp_stat[0]++;}lrp=rp;lfe=fe;tp=now;}else break;}}}
 {uint32_t ms=Get_System_Time()-t0;if(ms>vx3dn_rp_stat[1])vx3dn_rp_stat[1]=ms;vx3dn_rp_hist[ms<=1?0:ms<=10?1:ms<=50?2:ms<=100?3:ms<=500?4:5]++;}
 if(xr_wrapgrp){xr_wrapgrp=0;if(r)vx3dn_xr_stat[rk?71:68]++;else vx3dn_xr_stat[70]++;}
 dw=(VX_tsc()-t2)>>8;sv_wait+=dw;sv_wait_pub+=dw;
 return r;
}
static void xr_stop(void) {
 write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x680A0000UL);
 if(xr_state==2)xr_state=1;fx_ring=0;
 if(!fx_2d_dirty)fx_2d_back();fx_rfenced=0;fx_2d_dirty=0;vx3dn_xr_stat[7]++;VX3DN_ev(9,1);
}
static void xr_reset_engine(void) {
 unsigned long f;unsigned char old,unlock;int ok;
 f=VX_irq_save();old=inp(0x3C4);unlock=sr(0x10);sw(0x10,(unsigned char)(unlock|1));outp(0x3C4,old);VX_irq_restore(f);
 ok=vx3dn_recover&&engine_reset();
 f=VX_irq_save();old=inp(0x3C4);sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);
 if(ok){vx3dn_rec[0]++;vx3dn_forget();}else{vx3dn_rec[1]++;fault=1;VX2D_device_lost();}
}
static void fr_gone(void); /* .532 (B1, below) */
static int xr_start(void) { /* s3ginv 0xBB370 */
 volatile uint32_t *w=(volatile uint32_t*)xr_lin;uint32_t p,pause,k,st=0;unsigned long f;
 fr_gone(); /* .532 (review of .530, B1): a ring flip queued before this start whose fence never came is lost, not run (the ring is rebuilt
  * from its start, its words discarded; newer fences would pass its number) */
 if(!xr_srs_ok()||!xr_table_ok()){xr_table();vx3dn_xr_stat[9]++;
  if(!xr_srs()){write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x680A0000UL);fx_ring=0;fx_rfenced=0;fx_pend=0xFFFFFFFFUL;xr_off(VX_XR_OFF_TLBFLUSH);return 0;}} /* v10.426 (review M08): as a failed start */
 if(xr_ran&&(read_mmio(0,0x444)&0x80000000UL)){int rok;uint32_t r=xr_rp_ok(&rok); /* 0xBB46B: the fetcher left paused in the ring */
  /* .514 (audit of .509): the position taken only from two reads that agree, inside the ring with 512 bytes after it, compared before any
   * sum (an all-ones read aligned to FFFFFFFC wrapped r + 512 to 1FC and passed: the first padding store went 0xFFEFFFFC past the
   * mapping); otherwise the fetcher is stopped and the engine reset before the fresh start writes the ring */
  if(!rok||r==0xFFFFFFFFUL||r<XR_GPU||r>XR_GPU+XR_BYTES-512UL){vx3dn_xr_stat[4]++;write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x680A0000UL);xr_reset_engine();if(fault)return 0;}
  else{uint32_t a=(r&~3UL)-XR_GPU;p=a;
   while(p&15){w[p/4]=0xCC000000UL;p+=4;}
   w[p/4]=0xFE020420UL;w[p/4+1]=0x41C;w[p/4+2]=0x00300000UL;w[p/4+3]=0xCC000000UL;p+=16;
   while(p&0xFFUL){w[p/4]=0xCCC00001UL;w[p/4+1]=0xDDD00001UL;p+=8;}
   xr_flush(a,p);pause=XR_GPU+p-0x10;
   f=VX_irq_save();
   write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x64000000UL|(pause&0xFFFFFFUL));write_mmio(0,0x420,0x65000000UL|(pause>>24)|0x200UL);
   VX_irq_restore(f);
   vx3dn_xr_stat[3]++;
   if(!fetch_idle(&st)){vx3dn_xr_stat[4]++;write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x680A0000UL);xr_reset_engine();if(fault)return 0;}}}
 w[0]=0xFE020420UL;w[1]=0x41C;w[2]=0x00300000UL;w[3]=0xCC000000UL;for(k=4;k<64;k+=2){w[k]=0xCCC00002UL;w[k+1]=0xDDD00002UL;}
 xr_flush(0,256);pause=XR_GPU+256UL-0x10;
 fx_rfenced=vx3dn_ring_fence&&hda&&(hda->bpp==16||hda->bpp==32);fx_rkicks=0;fx_rreason=0;
 if(fx_rfenced){uint32_t a=vx_status.fb_linear+FX_FENCE_OFF;for(k=0;k<14;k++)fx_2d[k]=read_mmio(0,fx_2d_reg[k]); /* the 2D registers the fences change */
  if(hda->bpp==16)*(volatile uint16_t*)a=(uint16_t)fx_seq;else *(volatile uint32_t*)a=fx_seq&0xFFFFUL;publish();(void)*(volatile uint32_t*)a;}
 f=VX_irq_save();
 write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x680A0000UL);write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x02000000UL); /* XP's engine initialisation (0x110E86) */
 if(xr_variant&4)xr_vq(1);
 write_mmio(0,0x41C,0x00100000UL);
 write_mmio(0,0x420,0x60000000UL|((XR_GPU|1UL)&0xFFFFFFUL));write_mmio(0,0x420,0x61000000UL|(XR_GPU>>24));
 write_mmio(0,0x420,0x62000000UL|(XR_END&0xFFFFFFUL));write_mmio(0,0x420,0x63000000UL|(XR_END>>24));
 write_mmio(0,0x420,0x64000000UL|(pause&0xFFFFFFUL));write_mmio(0,0x420,0x65000000UL|(pause>>24));
 write_mmio(0,0x420,0x66FFFFF0UL);write_mmio(0,0x420,0x670000FFUL);write_mmio(0,0x420,0x680A0001UL);
 VX_irq_restore(f);
 xr_next=256;xr_pend=pause+0x10;xr_state=2;xr_ran=1;fx_ring=1;fx_2d_dirty=0;vx3dn_xr_stat[0]=3;vx3dn_xr_stat[2]++;
 VX3DN_ev(2,XR_GPU>>8);vx3dn_ring_stat[0]++;
 if(!xr_tested){ /* the first start: the fetcher must reach its pause in the ring (43C there); else GART fetches do not work here */
  uint32_t t0=Get_System_Time(),c=0,q=0,s=0;int ok=0;
  for(;;){int z=xr_quiet(&s);c=xr_rp();q=read_mmio(0,0x444);
   {int rok;c=xr_rp_ok(&rok);if(z>0&&rok&&c>=XR_GPU&&c<=XR_GPU+256UL){ok=1;break;}} /* .514 (audit): a stable position in the first block, where the published pause is (any position in the ring was taken) */
   if(z<0||(uint32_t)(Get_System_Time()-t0)>=100)break;}
  vx3dn_xr_stat[20]=s;vx3dn_xr_stat[21]=c;vx3dn_xr_stat[22]=q;vx3dn_xr_stat[24]=read_mmio(0,0x440);vx3dn_xr_stat[25]=read_mmio(0,0x45C);vx3dn_xr_stat[26]=read_mmio(0,0x464);xr_tested=1;
  if(xr_try<6){uint32_t *r=&vx3dn_xr_stat[30+5*xr_try];r[0]=s|(xr_variant<<28);r[1]=c;r[2]=q;r[3]=vx3dn_xr_stat[25];r[4]=vx3dn_xr_stat[26];vx3dn_xr_stat[61]=xr_try+1;}
  if(!ok){write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x680A0000UL);fx_ring=0;fx_rfenced=0;xr_state=1;
   if(s&0x101FF3UL)xr_reset_engine();
   if(xr_variant&4)xr_vq(0);
   fx_pend=0xFFFFFFFFUL;
   if(++xr_try<6&&!fault){xr_variant=xr_forms[xr_try];xr_table();if(!xr_srs()){xr_variant=0;vx3dn_xr_stat[60]=0xFF;xr_off(VX_XR_OFF_TLBFLUSH);return 0;}if(xr_variant&8)xr_claim(); /* v10.426 (review M08) */xr_tested=0;xr_ran=0;return xr_start();} /* the next form */
   xr_variant=0;xr_table();xr_srs();vx3dn_xr_stat[60]=0xFF;xr_off(VX_XR_OFF_NOPAUSE);return 0;}
  vx3dn_xr_stat[60]=xr_variant;}
 return 1;
}
/* fx_retire_x for XP's ring: the wait, no stop unless asked */
static int xr_retire(int reset,int stop) {
 uint32_t st=0,t;int idle;
 xr_lap=0; /* every group kicked is done (or the ring stops): nothing of the last lap is still read */
 if(!fx_inflight){if(stop)xr_stop();return 1;}
 fx_inflight=0;
 t=PT();idle=xr_wait(&st);LAT(&vx3dn_lat[2],PT()-t);XP_ADD(6,t);vx3dn_xr_prof[11]++;
 if(!idle){
  vx3dn_fetch_stat[3]++;last_status=st;hang_note(st,fx_fly,fx_nfly);vx3dn_xr_stat[13]++;vx3dn_xr_stat[14]=xr_rp();vx3dn_xr_stat[15]=read_mmio(0,0x444);vx3dn_xr_stat[27]=read_mmio(0,0x440);vx3dn_xr_stat[28]=read_mmio(0,0x45C);vx3dn_xr_stat[29]=xr_pend;
  fx_ring_hang(st);
  write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x680A0000UL);xr_state=1;fx_ring=0;
  if(reset){xr_reset_engine();if(!fault)fx_2d_back();}
  fx_rfenced=0;fx_2d_dirty=0;return 0;}
 if(fx_rfenced){t=PT();fx_2d_restore();XP_ADD(7,t);vx3dn_xr_prof[9]++;}
 if(stop)xr_stop();
 last_status=st;return 1;
}
static int fx_retire_x(int reset,int stop);
static int fx_retire(int reset) {return fx_retire_x(reset,0);}
static int fx_retire_stop(int reset) {return fx_retire_x(reset,1);}
static int fx_retire_x(int reset,int stop) { /* XP's ring: xr_retire; a one-shot group: idle, then the stop */
 uint32_t st=0,t;int idle;
 if(xr_state==2)return xr_retire(reset,stop); /* v10.402 */
 if(!fx_inflight)return 1;
 fx_inflight=0;
 t=PT();idle=fetch_idle(&st);LAT(&vx3dn_lat[2],PT()-t); /* v10.321 */
 write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x680A0000UL);
 if(!idle) {
  vx3dn_fetch_stat[3]++;last_status=st;hang_note(st,fx_fly,fx_nfly); /* v10.291 */
  if(reset) {
   unsigned long f;unsigned char old,unlock;int ok;
   f=VX_irq_save();old=inp(0x3C4);unlock=sr(0x10);sw(0x10,(unsigned char)(unlock|1));outp(0x3C4,old);VX_irq_restore(f);
   ok=vx3dn_recover&&engine_reset();
   f=VX_irq_save();old=inp(0x3C4);sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);
   if(ok){vx3dn_rec[0]++;vx3dn_forget();}else{vx3dn_rec[1]++;fault=1;VX2D_device_lost();}
  }
  return 0;
 }
 last_status=st;return 1;
}
/* v10.232: and the state cache starts over: whoever asks may change memory the engine reads (a texture by Lock or Blt; the texture
 * cache is only known to be refreshed when the sampler state is written again) */
uint32_t vx3dn_rt_stat[4]; /* v10.418: retires of work in flight: [0] for a CPU access (VX3DN_busy: Lock, 2D, flip), [1] their time, [2] by a
 * submit outside the ring (a clear by the 2D engine), [3] their time (TSC / 256) */
/* v10.426 (review M12): a 3D draw still in flight is retired, the state cache kept (nothing in flight: nothing done); for the declined GDI
 * requests the display driver then draws with DIBENG */
/* v10.473: the retires for a CPU access by their caller (board, GTA3 v10.472: 5,430 in 5,257 frames, 2.2 ms each): vx3dn_rt_why, set by the
 * caller just before (0 when none did): 1 a DirectDraw blit (the 2D path's check, or one it declined), 2 a GDI or other 2D request, 3 a
 * Lock, Unlock or software blit (VX2D_access), 4 a flip, 5 VX2D_sync (an asynchronous blit completed: a flip's, a clear's, a query's), 6
 * the old 3D path or a mode change; read and cleared by every VX3DN_busy and VX3DN_retire_inflight. vx3dn_rt_cause[cause]: the retires and
 * their time (TSC / 256). TEXDBG BC CA reads causes 0-5's counts, CB their times. */
volatile uint32_t vx3dn_rt_why;uint32_t vx3dn_rt_cause[8][2];
static void rt_note(uint32_t t) {uint32_t w=vx3dn_rt_why<8?vx3dn_rt_why:0;vx3dn_rt_stat[0]++;vx3dn_rt_stat[1]+=(PT()-t+128)>>8;vx3dn_rt_cause[w][0]++;vx3dn_rt_cause[w][1]+=(PT()-t+128)>>8;}
int VX3DN_retire_inflight(void) {if(fx_inflight){uint32_t t=PT();VX3DN_ev(5,0);(void)fx_retire(1);rt_note(t);}vx3dn_rt_why=0;return running||fault;}
int VX3DN_busy(void) {if(fx_inflight){uint32_t t=PT();VX3DN_ev(5,0);(void)fx_retire(1);rt_note(t);}vx3dn_rt_why=0;vx3dn_forget();return running||fault;}
/* .575 (peer review of .572, finding 10): what VX3DN_busy answers when nothing is in flight, without its retire and without starting the
 * state cache over (VX2D_access's status query after VX3DN_poll: it touched no register) */
int VX3DN_stopped(void) {return running||fault;}
void VX3DN_ring_stop(void) {cs_enter();(void)fx_retire_stop(1);cs_leave();} /* v10.391: before a mode set */
/* v10.285: the status query's look at a draw in flight: 1 while the engine still works on it (and for less than 100 ms since its start),
 * else it is retired as VX3DN_busy would (a hung one reset after its 100 ms) and 0; the state cache is left alone when nothing changed */
int VX3DN_poll(void) {
 uint32_t st;
 if(!fx_inflight) return 0;
 if(xr_state==2){int q=xr_quiet(&st);if(q>=0&&!(q>0&&(!fx_rfenced||fx_fence_read()==(fx_seq&0xFFFFUL)))&&(uint32_t)(xr_now()-fx_t0)<100UL)return 1;} /* v10.402; v10.449: xr_now */
 else {
 st=read_mmio(0,0x400);
 if(st!=0xFFFFFFFFUL&&(st&0x101FF3UL)&&(uint32_t)(xr_now()-fx_t0)<100UL) return 1;}
 VX3DN_ev(6,0);if(!fx_retire(1)) vx3dn_forget();
 return 0;
}
/* v10.236: the triangles in a draw packet (a strip run of n vertices holds n - 2) */
static uint32_t packet_triangles(const vx3dn_packet *p) {
 uint32_t k,t=0;
 if(p->cb!=sizeof(*p)||!p->bursts.n||p->bursts.n>VX3DN_MAX_BURSTS)return p->vertex_count/3;
 for(k=0;k<p->bursts.n;k++){uint32_t c=p->bursts.b[k]&0xFFFFUL,tp=p->bursts.b[k]>>16;t+=tp==VX3DN_TOPO_STRIP?(c>=2?c-2:0):c/3;} /* v10.470: an indexed run's count is indices */
 return t;
}
uint32_t vx3dn_reg_draws; /* v10.227: register-path draws since the VxD loaded (VXRING) */
static uint32_t q_before_draw(void);
static uint32_t q_before_clear(void);
extern volatile uint32_t vx_flip_old_front;void VX_flip_front_wait(uint32_t off); /* v10.384 (vx855_flip.c) */
uint32_t vx3dn_ss_worst[8][8],vx3dn_ss_count; /* v10.393: VX3DN_submit calls of 10 ms and more, the 8 longest: whole, critical-section wait,
 * engine wait (TSC / 256), op | clear flags << 8, triangles, thread run ms, wall ms, target offset */
static uint32_t submit_x(const vx3dn_packet *p);
static uint32_t packet_triangles(const vx3dn_packet *p);
extern uint32_t sv_wait_pub;
/* v10.470 (ACCEL_D3D_INDEXED): the vertex area of indexed packets: VX3DN_VA_SLOTS slots of one packet's vertices (192 x 40 bytes) in video
 * memory below the page table block (vxd_main.c reserves it: vx3dn_va_off, 0 = none, every indexed packet expanded inline). The slots are
 * taken in turn; each keeps the group that read it: in XP's ring the fence sequence its group will be kicked with (fx_seq + 1), else the
 * one-shot group's number. A slot is given again only once that group is done: nothing in flight, the ring's fence past it, or another
 * one-shot group started since (one-shot groups run one at a time); the group being built never gets its own slot back. Without a free
 * slot the packet is drawn by its indices expanded inline (the emitter), never waited for. vx3dn_va_stat: [0] slots given, [1] refused
 * (still read by a group in flight), [2] refused (the group being built), [3] the slots. */
#define VX3DN_VA_SLOT 7680UL
#define VX3DN_VA_SLOTS 136UL
uint32_t vx3dn_va_off,vx3dn_va_stat[4],vx3dn_va_nslots=VX3DN_VA_SLOTS; /* (the slots used: the linked checks lower it) */static uint32_t va_tag[VX3DN_VA_SLOTS],va_next,va_gid=1,va_started;
extern uint32_t (*vx3dn_varena)(uint32_t);extern void (*vx3dn_vwrite)(uint32_t,const uint32_t *,uint32_t);
extern void (*vx3dn_write_n)(const uint32_t *w,uint32_t n);
#define VA_RING 0x80000000UL
/* .529: an indexed packet of more than 192 vertices takes two slots side by side (384 x 40 bytes); both must be free, the run of slots
 * starts again at slot 0 when it would pass the last */
static uint32_t va_busy(uint32_t t,uint32_t cur) { /* 1 refused for the group being built, 2 for a group in flight, else 0 */
 if(!t||!cur)return 0;
 if(t==cur)return 1;
 if(fx_inflight){
  if(t&VA_RING){if(fx_gring==2&&!(fx_rfenced&&(int16_t)(uint16_t)(fx_fence_read()-(t&0xFFFFUL))>=0))return 2;}
  else if(fx_gring!=2&&t==va_started)return 2;}
 return 0;
}
static uint32_t va_alloc(uint32_t bytes) {
 uint32_t k=va_next,cur,m,j,b;
 if(!vx3dn_va_off||!bytes)return 0xFFFFFFFFUL;
 m=(bytes+VX3DN_VA_SLOT-1)/VX3DN_VA_SLOT;
 if(!vx3dn_va_nslots||vx3dn_va_nslots>VX3DN_VA_SLOTS)vx3dn_va_nslots=VX3DN_VA_SLOTS;vx3dn_va_stat[3]=vx3dn_va_nslots;
 if(m>3||m>vx3dn_va_nslots)return 0xFFFFFFFFUL; /* .559: three (512 vertices: 512 x 40 bytes = 2.7 slots; .529-.558 two for 384) */
 if(k+m>vx3dn_va_nslots)k=va_next=0;
 /* the register path (no capture: vx3dn_write_n unset) draws synchronously after every group was retired: every slot is free and this one
  * is done when the draw returns */
 cur=!vx3dn_write_n?0:fx_gring==2?VA_RING|((fx_seq+1)&0xFFFFUL):(va_gid&0x7FFFFFFFUL);
 for(j=0;j<m;j++)if((b=va_busy(va_tag[k+j],cur))!=0){vx3dn_va_stat[b==1?2:1]++;return 0xFFFFFFFFUL;}
 for(j=0;j<m;j++)va_tag[k+j]=cur;
 va_next=k+m>=vx3dn_va_nslots?0:k+m;vx3dn_va_stat[0]++;
 return vx3dn_va_off+k*VX3DN_VA_SLOT;
}
static void va_write(uint32_t off,const uint32_t *w,uint32_t n) {
 uint32_t bytes=n*4;if(!vx_status.fb_linear||off>vx_status.fb_bytes||bytes>vx_status.fb_bytes-off)return;
 vx_movsd((void*)(vx_status.fb_linear+off),w,n);
}
static void va_hooks(void) {if(!vx3dn_varena){vx3dn_varena=va_alloc;vx3dn_vwrite=va_write;}}
uint32_t VX3DN_submit(const vx3dn_packet *p) {
 uint32_t t=VX_tsc(),c0=cs_wt,w0,rc,d,k,lo=0;void *th=Get_Cur_Thread_Handle();uint32_t x0=vx3dn_fastclock?0xFFFFFFFFUL:Get_Thread_Exec_Time(th),m0=xr_now(); /* v10.449: no thread time with FASTCLOCK (another timer-chip read) */
 VX3DN_ev(7,p?p->op:0xFF);w0=sv_wait_pub;va_hooks(); /* v10.470 */
 rc=submit_x(p);d=(VX_tsc()-t)>>8;
 if(d>=39062UL){vx3dn_ss_count++;for(k=1;k<8;k++)if(vx3dn_ss_worst[k][0]<vx3dn_ss_worst[lo][0])lo=k;
  if(d>vx3dn_ss_worst[lo][0]){vx3dn_ss_worst[lo][0]=d;vx3dn_ss_worst[lo][1]=cs_wt-c0;vx3dn_ss_worst[lo][2]=sv_wait_pub-w0;
   vx3dn_ss_worst[lo][3]=p?(p->op|(p->clear_flags&0xFFFFFFUL)<<8):0;vx3dn_ss_worst[lo][4]=p&&p->op==VX3DN_DRAW?packet_triangles(p):0;
   vx3dn_ss_worst[lo][5]=x0==0xFFFFFFFFUL?0xFFFFFFFFUL:Get_Thread_Exec_Time(th)-x0;vx3dn_ss_worst[lo][6]=xr_now()-m0;vx3dn_ss_worst[lo][7]=p?p->target.offset:0;}}
 return rc;
}
/* v10.416 (board, GTA3 v10.414: 185 single submits of 14.6 ms each were depth-only clears of the back buffer waiting for the flip away
 * from it; 315 waits, 2902 ms in a session): ACCEL_D3D_FRONTZ (default 1): only a packet that writes the target's colour waits for the old
 * front to leave the screen; a depth/stencil-only clear, or a draw with no colour channel written, goes at once (it touches the depth
 * surface, the stencil plane and Coarse Z storage, never the shown pixels). 0 = every packet on the old front waits, as v10.384-v10.415. */
volatile uint32_t vx3dn_frontz=1;uint32_t vx3dn_frontz_n; /* waits not taken */
static int front_wait_needed(const vx3dn_packet *p) {
 if(vx_flip_old_front==0xFFFFFFFFUL||p->target.offset!=vx_flip_old_front)return 0;
 if(vx3dn_frontz&&(p->op==VX3DN_CLEAR?!(p->clear_flags&VX3DN_CLEAR_TARGET):(p->op==VX3DN_DRAW&&!(p->write_mask&15UL)))){vx3dn_frontz_n++;return 0;}
 {extern int vx_flip_ring_pend;extern uint32_t vx3dn_fr_stat[8]; /* v10.417: after a ring flip a fetched draw runs after the flip in the ring */
  if(vx_flip_ring_pend&&(p->op==VX3DN_DRAW||p->op==VX3DN_CLEAR)&&xr_state==2&&vx3dn_fetch&&vx3dn_cmd_off&&!fault){vx3dn_fr_stat[6]++;return 0;}}
 /* v10.418: a clear too: in the ring it runs after the flip; a clear that is not is done after a retire of the ring (the flip with it) */
 return 1;
}
/* .527: the last packet refused as BAD_PACKET, for the HAL's event log on both paths (it had the VxD's reason only for single submits,
 * and not even there which check refused) and for VXD3DMON: TEXDBG BC EC reads [0]-[5], ED [6]-[11], EE clears. [0] refusals counted;
 * [1] path << 24 (1 single submit, 2 batch) | where: 1 << 16 | the source line in vx_native3d_hw.c (vx3dn_bad_line: the validation or
 * the emitter), or 2 << 16 | the line in this file; [2] the packet's index in the batch, [3] the batch's packets; [4] op << 24 | texture
 * on << 20 | texture format << 12 | depth format << 4 | target format; [5] vertex count | pre.on << 16 | bursts << 20 | validated by its
 * geometry alone (state deltas, .524) << 31; [6] the state ranges the HAL marked the same (cp_same); [7] the target's offset, [8] its
 * width << 16 | height, [9] the texture's offset, [10] [11] the first vertex's x and y words. Written only on a refusal. */
uint32_t vx3dn_bad[12];extern uint32_t vx3dn_bad_line;static uint32_t bad_vline;
static void bad_note(uint32_t path,uint32_t k,uint32_t n,const vx3dn_packet *q,uint32_t geo) {
 const uint32_t *w;
 vx3dn_bad[0]++;vx3dn_bad[1]=path<<24|(bad_vline?0x20000UL|(bad_vline&0xFFFFUL):0x10000UL|(vx3dn_bad_line&0xFFFFUL));vx3dn_bad[2]=k;vx3dn_bad[3]=n;
 if(!q){uint32_t j;for(j=4;j<12;j++)vx3dn_bad[j]=0;return;}
 vx3dn_bad[4]=q->op<<24|(q->tex.enable?1UL:0)<<20|(q->tex.surface.format&255)<<12|(q->depth.format&255)<<4|(q->target.format&15);
 vx3dn_bad[5]=(q->vertex_count&0xFFFFUL)|(q->pre.on&15)<<16|(q->bursts.n&255)<<20|(geo?0x80000000UL:0);
 vx3dn_bad[6]=cp_same;vx3dn_bad[7]=q->target.offset;vx3dn_bad[8]=q->target.width<<16|(q->target.height&0xFFFFUL);
 vx3dn_bad[9]=q->tex.enable?q->tex.surface.offset:0;
 w=q==vx3dn_vsrc_pkt&&vx3dn_vsrc?vx3dn_vsrc:q->pre.w; /* the words the validation tested (v10.449: the caller's, copied by the emitter) */
 if(q->pre.on){vx3dn_bad[10]=w[0];vx3dn_bad[11]=w[1];}else{vx3dn_bad[10]=q->vertices[0].x;vx3dn_bad[11]=q->vertices[0].y;}
}
static uint32_t submit_x(const vx3dn_packet *p) {
 uint32_t t0,rc;
 if(p&&front_wait_needed(p))VX_flip_front_wait(p->target.offset); /* v10.384: the old front still shown; v10.416: only for colour */
 if((rc=(p&&(p->op==VX3DN_CLEAR||(p->op==VX3DN_DRAW&&(p->clear_flags&VX3DN_DRAW_CLEAR))))?q_before_clear():q_before_draw())!=0)return rc; /* v10.456 (XP RE, finding B): a clear is never counted into an occlusion query */
 t0=PT();rc=submit_impl(p);
 LAT(&vx3dn_lat[3],PT()-t0); /* v10.321 */
 if(p&&p->op==VX3DN_DRAW){vx3dn_prof[0]++;vx3dn_prof[1]+=(PT()-t0+128)>>8;}else if(p&&p->op==VX3DN_CLEAR){vx3dn_prof[5]++;vx3dn_prof[6]+=(PT()-t0+128)>>8;}
 return rc;
}
/* v10.232: vx3dn_sub_stat: TSC cycles / 256 of draw submits in their parts: entry checks, copy, validation and mapping; the sequencer
 * open and clock check; the draw (fetch or registers); the close (TEXDBG 0xAC reads, 0xAD reads and clears) */
uint32_t vx3dn_sub_stat[4];static uint32_t sub_t[4];
static uint32_t ring_clear(const vx3dn_packet *p);
static uint32_t submit_impl(const vx3dn_packet *p) {
 vx3dn_info i;vx3dn_io io;uint32_t rc,base;unsigned long f;unsigned char old,unlock,a,b,locked;
 int excluded=0,opened=0;
 sub_t[0]=PT();sub_t[1]=sub_t[2]=sub_t[3]=0;
 cs_enter();
 if(p&&p->op==VX3DN_CLEAR){uint32_t r=ring_clear(p);if(r!=0xFFFFFFFFUL){cs_leave();return r;}} /* v10.418 */
 if(!(p&&p->op==VX3DN_DRAW&&vx3dn_fetch&&vx3dn_cmd_off)){uint32_t t=PT();if(fx_inflight){(void)fx_retire(1);vx3dn_rt_stat[2]++;vx3dn_rt_stat[3]+=(PT()-t+128)>>8;}else(void)fx_retire(1);} /* v10.231: a fetched draw retires the one before at its start */
 /* VX3D_busy includes this module (VX3DN_busy, which would retire the draw in flight). Check before setting our own latch. */
 {int b3=VX3D_lab_busy()||(int)fault,b2=b3?0:VX2D_busy();
  if(b3||b2) {rc=(fault||VX2D_lost())?VX3DN_LOST:VX3DN_BUSY;if(rc==VX3DN_BUSY)last_result=VX3DN_BUSY|1UL<<8|(b3?1UL:2UL)<<16;cs_leave();return rc;}}
 if(p&&p->op==VX3DN_CLEAR&&vx3dn_clear2d) {
  uint32_t k,kk,r2,did=0,zdone=0;int async;VX3DN_info(&i);copy_packet(&request_copy,p);p=&request_copy; /* v10.309 (review): the state ranges, not 9.5 KB */
  rc=vx3dn_validate(p,&i);if(rc){last_result=rc;cs_leave();return rc;}
  if(p->clip.right>p->clip.left&&p->clip.bottom>p->clip.top) for(kk=0;kk<2;kk++) {
   uint32_t bits,cv,cm;
   k=kk?0:1; /* v10.282: the depth first, the target last */
   bits=k?(VX3DN_CLEAR_Z|VX3DN_CLEAR_STENCIL):VX3DN_CLEAR_TARGET;
   if(!(request_copy.clear_flags&bits))continue;
   if(k)vx3dn_depth_word(p,&cv,&cm);else{cv=p->target.format==VX3DN_RGB565?rgb565(p->clear_color):p->clear_color;cm=0xFFFFFFFFUL;}
   /* v10.282: the target's fill may be left running when nothing comes after it (the depth done by the 2D engine or not asked, no plane) */
   async=!k&&vx3dn_clear_async&&p->target.offset&&!zdone&&!(request_copy.clear_flags&(VX3DN_CLEAR_Z|VX3DN_CLEAR_STENCIL));
   r2=cm?fill2d(p,k?&p->depth:&p->target,cv,cm,async):VX2D_DONE;
   if(r2==VX2D_DONE){if(k&&(request_copy.clear_flags&VX3DN_CLEAR_STENCIL))zdone=1;
    request_copy.clear_flags&=~bits;vx3dn_clear2d_done++;did=1;}
   else if(r2!=VX2D_SKIP){fault=1;last_result=VX3DN_LOST;cs_leave();return VX3DN_LOST;} /* the command fired: never replay it */
  }
  /* v10.248: a stencil clear of the whole surface fills the plane with the 2D engine too (XP clears it with an 8bpp fill, T52), as a
   * 32bpp surface of pitch / 4 pixels by the rows rounded to 8: the tiling does not matter when every byte gets the value. A partial
   * clear is CPU writes over the tiles (vx3dn_plane_clear). */
  if(zdone&&p->depth.format==VX3DN_D24S8&&p->clip.left<=0&&p->clip.top<=0&&(uint32_t)p->clip.right>=p->depth.width&&(uint32_t)p->clip.bottom>=p->depth.height) {
   vx3dn_rect q;vx3dn_surface ps;uint32_t v=VX3DN_CLEAR_SVAL(p->clear_flags)&255UL;
   /* v10.249: the plane as a surface of the desktop's depth (fill_one converts nothing then), the value in every byte */
   uint32_t bpp=hda?hda->bpp:0,r3;
   ps.offset=VX3DN_SPLANE_OFF(&p->depth);ps.pitch=VX3DN_SPLANE_PITCH(p->depth.width);ps.height=VX3DN_SPLANE_ROWS(p->depth.height);
   ps.format=bpp==16?VX3DN_RGB565:VX3DN_XRGB8888;ps.width=bpp==16?ps.pitch>>1:ps.pitch>>2;v|=v<<8;if(bpp!=16)v|=v<<16;
   q.left=0;q.top=0;q.right=(int32_t)ps.width;q.bottom=(int32_t)ps.height;
   r3=(bpp==16||bpp==32)?fill_one(&q,&ps,v,0xF0,0):VX2D_SKIP;
   if(r3==VX2D_DONE){zdone=0;did=1;vx3dn_plane_fill2d++;}
   else if(r3!=VX2D_SKIP){fault=1;last_result=VX3DN_LOST;cs_leave();return VX3DN_LOST;} /* v10.249: the command fired: never replay it (as the fills above) */
  }
  if(did){if(VX2D_pending())vx3dn_clear_async_n++;else VX2D_sync();vx3dn_forget();} /* v10.282: a fill left running is completed by whoever comes next */
  if(zdone){vx3dn_io pio;vx3dn_packet *q=&request_copy;uint32_t f=q->clear_flags;memset(&pio,0,sizeof(pio));pio.read_fb=read_fb;pio.write_fb=write_fb;
   q->clear_flags|=VX3DN_CLEAR_STENCIL;vx3dn_plane_clear(q,&pio);q->clear_flags=f;publish();} /* v10.391 (review): the CPU's stencil writes drained before the return */ /* v10.245: the stencil plane after a 2D Z/stencil fill */ /* hand the kept 2D context back: the sequencer is locked again when a 3D submit returns; v10.232: the next draw sends its whole state */
  if(!VX3DN_CLEAR_WHAT(request_copy.clear_flags)){submitted++;clears++;last_result=VX3DN_OK;cs_leave();return VX3DN_OK;}
  if(!(request_copy.clear_flags&VX3DN_CLEAR_STENCIL))request_copy.clear_flags&=7UL; /* the stencil value goes with its flag */
  vx3dn_clear2d_cpu++;
 }
 running=1;VX3DN_info(&i);
 if(!p) {rc=VX3DN_BAD_PACKET;bad_vline=__LINE__;goto done;}
 if(p!=&request_copy)copy_packet(&request_copy,p);p=&request_copy;
 rc=vx3dn_validate(p,&i);if(rc){bad_vline=0;goto done;} /* .527: bad_vline cleared: the line is vx_native3d_hw.c's */
 base=vx_status.bar1&0xFFFFFFF0UL;
 if((vx_status.bar1&15)||base<0x100000UL||base>0xFFC00000UL) {rc=VX3DN_LOST;goto done;}
 if(!mapped) mapped=_MapPhysToLinear(base,0x400000UL,0);
 if(!mapped||mapped==0xFFFFFFFFUL) {mapped=0;rc=VX3DN_LOST;goto done;}
 sub_t[1]=PT();
 f=VX_irq_save();old=inp(0x3C4);unlock=sr(0x10);sw(0x10,(unsigned char)(unlock|1));
 a=sr(0x1A);b=sr(0x1E);
 if((a&0x42)||(sr(0x4E)&0x30)) {busy_why=2UL<<8|(uint32_t)a<<16|(uint32_t)sr(0x4E)<<24;sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);rc=VX3DN_BUSY;goto done;}
 sw(0x1A,(unsigned char)(a|8));sw(0x1E,(unsigned char)(b|1));
 opened=1;outp(0x3C4,old);VX_irq_restore(f);
 /* The clock must already be locked by the working display/2D path. */
 f=VX_irq_save();old=inp(0x3C4);locked=sr(0x3C);outp(0x3C4,old);VX_irq_restore(f);
 if(!(locked&0x10)) {busy_why=3UL<<8|(uint32_t)locked<<16;rc=VX3DN_BUSY;goto finish;}
 if(VESA_draw_needs_exclusion(p->target.offset,p->target.pitch*p->target.height)) {FBHDA_access_begin(0);excluded=1;} /* v10.371: only when the cursor may be on it (was: offset 0) */
 io.context=0;io.read=read_mmio;io.write=write_mmio;io.read_fb=read_fb;io.write_fb=write_fb;io.milliseconds=ticks;
 sub_t[2]=PT();
 publish();rc=FETCH_DECLINED;
 if(p->op==VX3DN_DRAW&&vx3dn_fetch&&vx3dn_cmd_off)rc=fetch_draw(p,&last_status,vx3dn_async&&!excluded&&(a&8)&&(b&1)); /* v10.228: through the command fetcher */
 if(rc==FETCH_DECLINED){if(!fx_retire_stop(0))rc=VX3DN_LOST;else{if(p->op==VX3DN_DRAW){vx3dn_reg_draws++;vx3dn_forget();}rc=vx3dn_execute(p,&io,&last_status);}}
 if(rc==VX3DN_LOST) {
  /* Never replay a partially submitted primitive or race a hung engine
   * with DIBENG, cursor pixels, mode changes or a CPU fallback. */
  if(fault) ; /* v10.281: fx_start's reset already failed and lost the device */
  else if(vx3dn_recover&&engine_reset()){vx3dn_rec[0]++;vx3dn_forget();rc=VX3DN_RECOVERED;submitted++;}
  else{vx3dn_rec[1]++;fault=1;VX2D_device_lost();}
 } else if(rc==VX3DN_RECOVERED) submitted++; /* v10.465: the ring's retire reset the engine already */
 else if(!rc) {publish();submitted++;if(p->op==VX3DN_DRAW)triangles+=packet_triangles(p);else clears++;}
 sub_t[3]=PT();
finish:
 /* Relock sequencer access even on failure. Do not gate the engine's
  * clocks, restore 3D registers or redraw the cursor while it is lost. */
 if(opened) {
  f=VX_irq_save();old=inp(0x3C4);
  if(!fault){sw(0x1E,b);sw(0x1A,a);}
  sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);
 }
 if(excluded&&!fault){if(fx_inflight)(void)fx_retire(1);if(!fault)FBHDA_access_end(0);} /* v10.466: nothing of ours still drawing under the cursor */
done:
 if(rc==VX3DN_BAD_PACKET){bad_note(1,0,1,p,0);bad_vline=0;} /* .527 */
 {extern uint32_t vx3dn_stage;last_result=rc==VX3DN_LOST?rc|(vx3dn_stage&255)<<8:rc==VX3DN_BUSY?rc|busy_why:rc;busy_why=0;} /* v10.192: + which wait timed out; v10.218: + why busy */
 running=0;cs_leave();
 if(sub_t[3]&&p&&p->op==VX3DN_DRAW){uint32_t t=PT();vx3dn_sub_stat[0]+=(sub_t[1]-sub_t[0]+128)>>8;vx3dn_sub_stat[1]+=(sub_t[2]-sub_t[1]+128)>>8;
  vx3dn_sub_stat[2]+=(sub_t[3]-sub_t[2]+128)>>8;vx3dn_sub_stat[3]+=(t-sub_t[3]+128)>>8;}
 return rc;
}
/* ---- v10.223: VXRING, the command-fetcher probe (vx_ring.h, cookbook 16.17). Test code: nothing in the draw path uses it. ---- */

static const unsigned char ring_sr_idx[16]={0x68,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F,0x39,0x3F,0x1A,0x1E,0x4E,0x10,0x3C,0x1B,0x2D};
static uint32_t ring_wordp,ring_count,ring_2d[13];static volatile uint32_t *ring_cpu;
static void ring_put(uint32_t w) {ring_cpu[ring_wordp++]=w;ring_count++;}
static void ring_pre(vx_ring_reply *r,uint32_t k,uint32_t w) {write_mmio(0,0x420,w);if(k<10)r->prec[k]=w;}
static int ring_map(void) {
 uint32_t base=vx_status.bar1&0xFFFFFFF0UL;
 if((vx_status.bar1&15)||base<0x100000UL||base>0xFFC00000UL)return 0;
 if(!mapped)mapped=_MapPhysToLinear(base,0x400000UL,0);
 if(!mapped||mapped==0xFFFFFFFFUL){mapped=0;return 0;}
 return 1;
}
static uint32_t ring_table_entry(vx_ring_reply *r,uint32_t page) {
 uint32_t at=r->table_offset+page*4;
 if(!r->table_readable||at+4>vx_status.fb_bytes)return 0xEEEEEEEEUL;
 return *(volatile uint32_t*)(vx_status.fb_linear+at);
}
/* the 2D solid fill as a fetched 2D packet: FE000000 n 00000000 CC000000, then n (register, value) pairs (cookbook 16.2) */
static void ring_fill_packet(const vx_ring_reply *r,const vx_ring_req *q) {
 uint32_t depth=r->bpp==16?0x100UL:0x300UL,mask=r->bpp==16?0xFFFFUL:0xFFFFFFFFUL;
 static const uint32_t reg[13]={0x04,0x08,0x14,0x1C,0x30,0x48,0x58,0x5C,0x20,0x4C,0x0C,0x10,0x00};
 uint32_t val[13],k;
 val[0]=depth;val[1]=(q->tgt_pitch>>3)<<16;val[2]=r->tgt_offset>>3;val[3]=0;val[4]=0;val[5]=0;val[6]=q->color&mask;val[7]=0;val[8]=0;
 val[9]=q->color&mask;val[10]=((q->height-1)<<16)|(q->width-1);val[11]=0;val[12]=0xF0002001UL;
 ring_put(0xFE000000UL);ring_put(13);ring_put(0);ring_put(0xCC000000UL);
 for(k=0;k<13;k++){ring_put(reg[k]);ring_put(val[k]);}
}
static uint32_t ring_gpu(const vx_ring_reply *r) {return r->buf_offset+ring_wordp*4;}
/* XP's batch end: CC000000 to 16 bytes, FE020420 0000041C 00300000 CC000000, then fill words up to the mask */
static void ring_pad(uint32_t mask,uint32_t w1,uint32_t w2) {
 while(ring_wordp&3)ring_put(0xCC000000UL);
 ring_put(0xFE020420UL);ring_put(0x41C);ring_put(0x00300000UL);ring_put(0xCC000000UL);
 while((ring_wordp*4)&mask){ring_put(w1);if(w2&&((ring_wordp*4)&mask))ring_put(w2);}
}
/* v1.2: the 3D emitter's register writes captured as fetched blocks (cookbook 16.2). Types 0x10 and 0x11 configure the command
 * regulator itself (setup() writes 68 = 0A0000 there, which would switch the fetcher off in the middle of its own buffer): left out.
 * v1.3 forms (request flags): none: every block FE03043C n header CC000000 (v1.2, drew nothing on the board);
 * 0x10: XP's two forms: a data header (type byte 03, or 0 = vertices) FE03043C n header CC000000 + n words, a register header
 *   FE020440 0000043C header CC000000 + its words; 0x20: FE03 only for the vertex header 0, FE02 for the rest;
 * 0x40: the type 0x11 block kept (as FE02); 0x80: everything before the vertex loop (vx3dn_stage < 2) written through the
 *   registers as test 6 does, only the vertex blocks fetched. Every block padded to 16 bytes with CC000000. Status reads see idle.
 * v1.4: 0x100: the type 0x10 and 0x11 blocks written through the registers during the capture (not left out). */
static uint32_t cap_hdr,cap_cnt,cap_err,cap_blocks,cap_max,cap_skip,cap_form,cap_flags;
extern uint32_t vx3dn_stage;
/* v10.282: the emitter's direct sink (vx_native3d_hw.c reg/dat): while a block is open for it (vx3dn_wform), the emitter stores the words
 * at vx3dn_wp; cap_sync takes them into ring_wordp, ring_count and cap_cnt before anything here looks at those. */
extern uint32_t *vx3dn_wp,*vx3dn_wend,vx3dn_wform,vx3dn_cgroups;
static void cap_sync(void) {
 if(vx3dn_wform){
  uint32_t *base=(uint32_t*)ring_cpu+ring_wordp,n;
  /* v10.283 (review H2): every path that moves ring_wordp puts vx3dn_wp back (or closes the sink first), so the pointer is never below
   * the count; checked anyway: a pointer out of [base, end] refuses the packet (cap_err 1) instead of moving the count by it */
  if(vx3dn_wp<base||vx3dn_wp>vx3dn_wend){cap_err|=1;vx3dn_wform=0;vx3dn_wend=0;return;}
  n=(uint32_t)(vx3dn_wp-base);ring_wordp+=n;ring_count+=n;cap_cnt+=n;
 }
}
static void cap_sink_off(void) {cap_sync();vx3dn_wform=0;vx3dn_wend=0;} /* v10.287: wend 0 = no room (the inline REG/DAT test) */
volatile uint32_t vx3dn_sink=1; /* v10.282: ACCEL_D3D_SINK (default 1); 0 = every word through cap_write as before (the same words) */
static void cap_sink_on(void) {if(!vx3dn_sink)return;vx3dn_wp=(uint32_t*)ring_cpu+ring_wordp;vx3dn_wend=(uint32_t*)ring_cpu+(cap_max>7?cap_max-7:0);vx3dn_wform=cap_form;}
static void cap_close(void) {cap_sink_off();if(cap_form==3)ring_cpu[cap_hdr]=cap_cnt;if(cap_form)while(ring_wordp&3)ring_put(0xCC000000UL);cap_form=0;}
static int cap_data(uint32_t v) {if(!(cap_flags&0x30))return 1;if(cap_flags&0x20)return v==0;return v==0||((v>>16)&0xFF)==3;}
static int cap_direct(void) {return (cap_flags&0x80)&&vx3dn_stage<2;}
volatile uint32_t vx3dn_cgroup=1; /* v10.282: ACCEL_D3D_CGROUPS (default 1; 0 = the constant runs sent with every set-up) */
static uint32_t sd_gen=0xFFFFFFFFUL,sd_primed;
/* v10.412 (board, v10.411: a packet with its state set-up opens about 40 blocks, each through cap_sync, cap_close, cap_sink_off, cap_sync,
 * four ring_put, cap_data and cap_sink_on: about 180 instructions a block, more than the set-up's own words): cap_write's 0x43C path
 * in one pass with the positions in locals. The same steps in the same order, the same words. ACCEL_D3D_FASTBLK 0 = the steps as before. */
volatile uint32_t vx3dn_fastblk=1;
static void cap_block(uint32_t v) {
 volatile uint32_t *rc=ring_cpu;uint32_t wp=ring_wordp,n=0,skip;
 if(vx3dn_wform){uint32_t *base=(uint32_t*)rc+wp; /* cap_sync */
  if(vx3dn_wp<base||vx3dn_wp>vx3dn_wend)cap_err|=1;else{uint32_t m=(uint32_t)(vx3dn_wp-base);wp+=m;n=m;cap_cnt+=m;}}
 vx3dn_wform=0;vx3dn_wend=0; /* cap_close */
 if(cap_form==3)rc[cap_hdr]=cap_cnt;
 if(cap_form)while(wp&3){rc[wp++]=0xCC000000UL;n++;}
 cap_form=0;
 skip=v==0x00100000UL||(v==0x00110000UL&&!(cap_flags&0x40));cap_skip=skip;
 if(skip||wp+8>cap_max){ring_wordp=wp;ring_count+=n;
  if(skip&&(cap_flags&0x100)){cap_skip=2;write_mmio(0,0x43C,v);}
  else if(!skip)cap_err|=1;
  return;}
 if(!(cap_flags&0x30)||((cap_flags&0x20)?v==0:(v==0||((v>>16)&0xFF)==3))){rc[wp]=0xFE03043CUL;cap_hdr=wp+1;rc[wp+1]=0;rc[wp+2]=v;rc[wp+3]=0xCC000000UL;cap_form=3;}
 else{rc[wp]=0xFE020440UL;rc[wp+1]=0x43C;rc[wp+2]=v;rc[wp+3]=0xCC000000UL;cap_form=2;}
 wp+=4;ring_wordp=wp;ring_count+=n+4;cap_cnt=0;cap_blocks++;
 if(vx3dn_sink){vx3dn_wp=(uint32_t*)rc+wp;vx3dn_wend=(uint32_t*)rc+(cap_max>7?cap_max-7:0);vx3dn_wform=cap_form;} /* cap_sink_on */
}
static void cap_write(void *ctx,uint32_t reg,uint32_t v) {
 (void)ctx;
 if(reg==0x43C&&vx3dn_fastblk&&!(cap_flags&0x80)){cap_block(v);return;}
 cap_sync();
 if(cap_direct()){vx3dn_wform=0;write_mmio(0,reg,v);return;}
 if(reg==0x43C){cap_close();cap_skip=v==0x00100000UL||(v==0x00110000UL&&!(cap_flags&0x40));
  if(cap_skip&&(cap_flags&0x100)){cap_skip=2;write_mmio(0,reg,v);return;}
  if(cap_skip)return;
  if(ring_wordp+8>cap_max){cap_err|=1;return;}
  if(cap_data(v)){ring_put(0xFE03043CUL);cap_hdr=ring_wordp;ring_put(0);ring_put(v);ring_put(0xCC000000UL);cap_form=3;}
  else{ring_put(0xFE020440UL);ring_put(0x43C);ring_put(v);ring_put(0xCC000000UL);cap_form=2;}
  cap_cnt=0;cap_blocks++;
  cap_sink_on();} /* v10.282 */
 else if(reg==0x440){if(cap_skip==2){write_mmio(0,reg,v);return;}if(cap_skip)return;
  if(!cap_form){cap_err|=2;return;}if(ring_wordp+8>cap_max){cap_err|=1;return;}
  if(cap_form==2&&((v>>24)==0xFEUL||(v>>24)==0xCCUL))cap_err|=8; /* would end the FE02 block */
  ring_put(v);cap_cnt++;
  if(vx3dn_wform)vx3dn_wp=(uint32_t*)ring_cpu+ring_wordp;}
 else cap_err|=4;
}
static uint32_t cap_read(void *ctx,uint32_t reg) {(void)ctx;return cap_direct()?read_mmio(0,reg):0;}
/* v10.232: a vertex's words in one call (vx3dn_write_n): the checks once, then a plain copy into the block */
extern void (*vx3dn_write_n)(const uint32_t *w,uint32_t n);
static void cap_write_n(const uint32_t *w,uint32_t n) {
 uint32_t j;volatile uint32_t *d;
 cap_sync();
 if(cap_skip)return;
 if(!cap_form){cap_err|=2;return;}
 if(ring_wordp+n+8>cap_max){cap_err|=1;return;}
 if(cap_form==2)for(j=0;j<n;j++)if((w[j]>>24)==0xFEUL||(w[j]>>24)==0xCCUL)cap_err|=8;
 d=ring_cpu+ring_wordp;vx_movsd((void*)d,w,n);
 ring_wordp+=n;ring_count+=n;cap_cnt+=n;
 if(vx3dn_wform)vx3dn_wp=(uint32_t*)ring_cpu+ring_wordp;
}
/* v10.238: room for n vertex words in the open data block, the vertices then built straight into the command block; NULL (the caller
 * sends a vertex at a time through cap_write_n) unless a data block is open with room */
extern uint32_t *(*vx3dn_reserve)(uint32_t n);
static uint32_t *cap_reserve(uint32_t n) {
 uint32_t *d;
 cap_sync();
 if(cap_skip||cap_form!=3||ring_wordp+n+8>cap_max)return 0;
 d=(uint32_t*)(ring_cpu+ring_wordp);ring_wordp+=n;ring_count+=n;cap_cnt+=n;
 if(vx3dn_wform)vx3dn_wp=(uint32_t*)ring_cpu+ring_wordp;
 return d;
}
/* v10.228: one draw packet through the fetcher, in the form VXRING v1.4 test 15 drew on the board (v10.227): the command-regulator
 * blocks (types 0x10/0x11) through the registers, everything else captured into the command block in XP's forms (FE02 register
 * blocks, FE03 data blocks), XP's batch end, then the one-shot start (0x41C = 00100000 again first, 60-65 with 65 | 0x200, 68 =
 * 0A0001), the wait for idle (0x400 & 101FF3 = 0, ACCEL_D3D_IDLE_READS reads in a row, 100 ms), the stop (68 = 0A0000). Packets
 * start 256-aligned and move through the block. FETCH_DECLINED: nothing was sent (a capture that does not fit): the caller uses the
 * register path. */
/* v10.365: ACCEL_D3D_FETCH_KB (VxD registry, default 32; 16 to 80): the most a fetch group holds. A group is started when another packet
 * might not fit; the next group is built in the block while it runs, and a group that would pass the block's end starts again at its
 * beginning: the group in flight then began after 256 KB - 2 x the size, so the new one cannot reach it while the size is at most a
 * third of the block (80 KB). A batch call
 * (VXGL's queue) returns once its last group is started: with larger groups more of the engine's drawing runs while the application goes on.
 * Board, v10.364: Quake 2's batches of about 17 packets went as 2.6 packets a start, the CPU waiting in the VxD for each group before. */
volatile uint32_t vx3dn_fetch_kb=32;
#define FETCH_MAX_BYTES ((vx3dn_fetch_kb<16UL?16UL:vx3dn_fetch_kb>80UL?80UL:vx3dn_fetch_kb&~3UL)*1024UL)
static uint32_t fx_next;
static int fetch_idle_impl(uint32_t *last) {
 uint32_t n,t0=Get_System_Time(),stable=0;extern uint32_t vx3dn_idle_stable;uint32_t need=vx3dn_idle_stable<1?1:vx3dn_idle_stable;
 for(n=0;n<1000000UL;n++) {
  *last=read_mmio(0,0x400);
  if(*last==0xFFFFFFFFUL)return 0;
  if(!(*last&0x101FF3UL)||(xr_state==2&&(*last&0x101FF3UL)==0x10UL&&(read_mmio(0,0x444)&0x80000000UL))){if(++stable>=need)return 1;}else stable=0; /* v10.402: XP's ring paused with bit 4 set */
  if(!(n&15)&&(uint32_t)(Get_System_Time()-t0)>=100)break;
 }
 return 0;
}
/* v10.388 (board, v10.387: a DrawPrimitives2 call spent 594 ms in one VxD call): VX3DN_submit_batch calls of 10 ms and more, the 8
 * longest (TSC / 256): the whole, the engine waits in it (fetch_idle), packets, triangles, fetch starts, engine resets, late fences and
 * kicks written again in it (TEXDBG 0xBD, index 0-8; 0xBD with index 0x80 clears) */
uint32_t vx3dn_sv_worst[8][10],vx3dn_sv_count; /* v10.389: [8] the calling thread's run time in it (ms), [9] the wall time (ms) */static uint32_t sv_wait;
uint32_t sv_wait_pub; /* v10.393: every engine wait (the single-submit record) */
int VX_smp_wait_begin(void);void VX_smp_wait_end(int);
uint32_t vx_smp_active(void);
static int fetch_idle(uint32_t *last) {uint32_t t=PT(),t2=VX_tsc(),dw,o=vx_smp_ph;int r,w=VX_smp_wait_begin();vx_smp_ph=VX_SMP_PH_WAIT;r=fetch_idle_impl(last);vx_smp_ph=o;VX_smp_wait_end(w); /* v10.434; v10.439: this thread's own entry */LAT(&vx3dn_lat[1],PT()-t);dw=(VX_tsc()-t2)>>8;sv_wait+=dw;sv_wait_pub+=dw;return r;} /* v10.321 */
/* v10.275: a fetch is a group of packets: their words one after another in the block, XP's batch end once, one start (VX3DN_submit_batch,
 * VX_IOCTL_3DN_SUBMITN). fx_open_group starts a group at fx_next, fx_build adds a packet, fx_start sends the group. A single draw is a
 * group of one, as before. */
uint32_t vx3dn_batch_stat[4]; /* v10.275: batch calls, packets in them, fetch starts for them, the most packets in one start */
/* v10.369: a batch's parts not in the fetcher's other counters (TSC cycles / 256; TEXDBG 0xB8 reads, 0xB9 reads and clears): [0] copying the
 * packets in, [1] their validation, [2] the wait for a synchronous last group (a batch with a draw on the primary), [3] fetch starts */
uint32_t vx3dn_bat_prof[4];
static uint32_t fx_gstart,fx_gpackets;
#define FX_PACKET_WORDS 3200UL /* room kept for one more packet in a group (the most seen: 2248 words) */
#define FX_RUN_WORDS 100UL /* .528: and for each texture run (the vertex-fetch registers set back, the strobe, a unit's words: about 85) */
/* v10.402: a group in XP's ring (xr_ above): the wrap when the next group might not fit, the ring started when it is not running */
/* v10.404 (board, v10.403: the fetcher read stopped (444 bit 31 clear) after every one of 4855 wraps, each then a new start): the wrap
 * point was the ring's end (62/63); XP's is always before it. Now the last block is left out: the padding runs to XR_WRAP and 66/67 =
 * XR_WRAP - 0x10 (the same point every time, so a later lap never meets an older one) */
static void xr_wrap(void) { /* s3ginv 0xBBC10 */
 volatile uint32_t *w=(volatile uint32_t*)xr_lin;uint32_t p=xr_next,a=xr_next,pause=XR_GPU-0x10;unsigned long f;
 while(p&15){w[p/4]=0xCC000000UL;p+=4;}
 w[p/4]=0xFE020420UL;w[p/4+1]=0x41C;w[p/4+2]=0x00300000UL;w[p/4+3]=0xCC000000UL;p+=16;
 if(p<XR_WRAP){vx_stosd((void*)(w+p/4),0xCCC00003UL,(XR_WRAP-p)/4);p=XR_WRAP;} /* v10.412: rep stosd, the same words */
 xr_flush(a,XR_WRAP);
 f=VX_irq_save();
 write_mmio(0,0x41C,0x00100000UL);
 write_mmio(0,0x420,0x66000000UL|((XR_GPU+XR_WRAP-0x10UL)&0xFFFFFFUL));write_mmio(0,0x420,0x67000000UL|((XR_GPU+XR_WRAP-0x10UL)>>24));
 write_mmio(0,0x420,0x64000000UL|(pause&0xFFFFFFUL));write_mmio(0,0x420,0x65000000UL|(pause>>24));
 VX_irq_restore(f);
 xr_pend=XR_GPU;xr_next=0;vx3dn_xr_stat[6]++;vx3dn_ring_stat[2]++;xr_after_wrap=1;
}
static int xr_open_x(void);
static int xr_open(void) {uint32_t t=PT();int r=xr_open_x();XP_ADD(0,t);return r;}
static int xr_open_x(void) { /* 1: the group opened in the ring, 0: XP's ring cannot run (off for good) */
 /* v10.410 (XP's space check, s3ginv 0xBBF3D-0xBC16B: the writes kept clear of 43C; v10.405-v10.409 waited for every group before a
  * wrap, 5.3 microseconds a packet): the wrap is written at once (its padding lies past every word kicked). After it, while groups
  * of the last lap may still be read (xr_lap, until a retire), each new group's room, with 1 KB for the fetcher's read-ahead, is
  * checked against the fetcher's read position (43C, two reads agreeing): the engine is waited for only when it is still there or
  * the position is not in the ring. */
 if(xr_state==2&&xr_next+FETCH_MAX_BYTES+512UL>XR_WRAP){xr_wrap();xr_lap=fx_inflight;vx3dn_xr_stat2[0]++;}
 if(xr_state==2&&xr_lap&&fx_inflight){int rok;uint32_t rp=xr_rp_ok(&rok),off=rp-XR_GPU;vx3dn_xr_stat2[1]++;
  if(rok&&rp>=XR_GPU&&rp<=XR_GPU+XR_BYTES&&off<=xr_next)xr_lap=0; /* the fetcher is in this lap, behind the writes */
  else if(!rok||rp<XR_GPU||rp>XR_GPU+XR_BYTES||xr_next+FETCH_MAX_BYTES+512UL+0x400UL>off){vx3dn_xr_stat2[2]++;if(!fx_retire(1))return (fx_ring_dead||fault)?0:xr_open_x();}} /* v10.465 (review): a ring that hung in this wait is not started again (the group goes one-shot) */
 /* v10.405 (XP binary audit: XP never restarts on 444 bit 31; board, v10.404: it read clear after every wrap): counted only */
 /* v10.408: no longer read every group (opening groups took 5.3 microseconds a packet) */
 if(xr_state==1){if(fx_inflight)(void)fx_retire_stop(1);if(!xr_start())return 0;}
 if(xr_state!=2)return 0;
 fx_gstart=XR_GPU+xr_next;
 ring_cpu=xr_use_nt()?(volatile uint32_t*)xr_stage:(volatile uint32_t*)(xr_lin+xr_next);ring_wordp=0;ring_count=0;fx_gpackets=0;fx_nsum=0; /* v10.411: staged for MOVNTI */
 cap_max=FETCH_MAX_BYTES/4-128;fx_gring=2;
 return 1;
}
static void fx_open_group(void) {
 int ring=vx3dn_xr&&!fx_ring_dead; /* v10.383: not after a hang in the ring */
 if(ring&&xr_state>=0&&xr_setup()&&xr_open())return; /* v10.402: XP's ring */
 if(xr_state==2)(void)fx_retire_stop(1); /* XP's ring left for a one-shot group */
 if(fx_next+FETCH_MAX_BYTES>FX_BLOCK_BYTES)fx_next=0; /* v10.383: the block's last 256 bytes hold the fence */
 fx_gstart=vx3dn_cmd_off+fx_next;
 ring_cpu=(volatile uint32_t*)(vx_status.fb_linear+fx_gstart);ring_wordp=0;ring_count=0;fx_gpackets=0;fx_nsum=0; /* v10.291 */
 cap_max=FETCH_MAX_BYTES/4-16;fx_gring=0;
 if(!++va_gid||va_gid>0x7FFFFFFFUL)va_gid=1; /* v10.470: the one-shot group's number (vertex area slots) */
}
static void fx_batch_end(void) {
 while(ring_wordp&3)ring_put(0xCC000000UL);
 ring_put(0xFE020420UL);ring_put(0x41C);ring_put(0x00300000UL);ring_put(0xCC000000UL);
}
/* one packet's words at the group's end: 0, the emitter's refusal, or FETCH_DECLINED (it did not fit); on either the group is as before */
/* v10.286: fx_build's parts (TSC cycles / 256): [0] the state-cache compare, [1] the emitter, [2] keeping the state for the next compare,
 * [3] packets; TEXDBG 0xC8 reads them and the emitter's (vx3dn_eprof), 0xC9 reads and clears */
uint32_t vx3dn_bprof[4];
/* v10.473: indexed packets' time in the VxD, TSC cycles / 256 (vx3dn_ixp, vx_native3d_hw.c): [0] packets copied and validated in a batch,
 * [1] their copy-in, [2] their validation, [3] the emitter of those counted in [7] (the state, texture and program set-up, the vertices and indices), [4] the
 * vertex area's slot and the vertices written into it, [5] the index bursts, [6] the registers set back after, [7] packets drawn by index
 * with the profile on (vx3dn_tsc). TEXDBG BC CC reads [0..5], CD [6..7] and the retire causes 6 and 7, CE clears. */
extern uint32_t vx3dn_ixp[8];
static uint32_t tsc_fn(void) {return VX_tsc();}
static void movsd_fn(uint32_t *d,const uint32_t *s,uint32_t n) {vx_movsd(d,s,n);} /* v10.449: the emitter's block copy */
extern void (*vx3dn_copyw)(uint32_t *,const uint32_t *,uint32_t);
extern uint32_t (*vx3dn_tsc)(void),vx3dn_eprof[4];
/* v10.440: the command words by part (TEXDBG BC 9E-A1 read 6 words each, A2 clears and turns it on, A3 turns it off): [0] packets,
 * [1] those with their state set-up, [2] words in all, [3] state set-up, [4] texture set-up, [5] program set-up, [6] vertex words,
 * [7] the rest of the vertex part (burst heads, block headers, padding), [8] words before the set-up, [9] vertices, [10] triangles,
 * [11] strip vertices; a part the same words as that part of the last packet that had it: [12]/[13] set-ups and their words, [14]/[15]
 * texture set-ups, [16]/[17] program set-ups; [18] packets compared (the ring staged in cached memory: the words read back), [19]
 * packets with texture words, [20] with program words. */
uint32_t vx3dn_wstat[24];volatile uint32_t vx3dn_wprof;
static uint32_t wp_mk[4],wp_h[3],wp_n[3],wp_ok[3],wp_g0[4];
extern void (*vx3dn_mark)(uint32_t k);extern uint32_t vx3dn_geo[4];
static uint32_t wp_pos(void) {return ring_wordp+(vx3dn_wform&&vx3dn_wp>=(uint32_t*)ring_cpu+ring_wordp?(uint32_t)(vx3dn_wp-((uint32_t*)ring_cpu+ring_wordp)):0);}
static void wp_mark(uint32_t k) {if(k<4)wp_mk[k]=wp_pos();}
static void wp_part(uint32_t k,uint32_t a,uint32_t b,int readable) {
 uint32_t n=b-a,h=0x811C9DC5UL,j;
 vx3dn_wstat[3+k]+=n;if(!n||!readable)return;
 for(j=a;j<b;j++)h=((h<<5)|(h>>27))^ring_cpu[j];
 if(wp_ok[k]&&wp_n[k]==n&&wp_h[k]==h){vx3dn_wstat[12+2*k]++;vx3dn_wstat[13+2*k]+=n;}
 wp_ok[k]=1;wp_n[k]=n;wp_h[k]=h;
}
static void wp_packet(uint32_t at,const uint32_t *g0) {
 uint32_t total=ring_wordp-at,vstart=at,vw=vx3dn_geo[0]-g0[0];int readable=ring_cpu==(volatile uint32_t*)xr_stage;
 vx3dn_wstat[0]++;vx3dn_wstat[2]+=total;
 if(wp_mk[0]!=0xFFFFFFFFUL&&wp_mk[3]!=0xFFFFFFFFUL&&wp_mk[0]>=at&&wp_mk[3]<=ring_wordp) {
  vx3dn_wstat[1]++;vx3dn_wstat[8]+=wp_mk[0]-at;
  wp_part(0,wp_mk[0],wp_mk[1],readable);wp_part(1,wp_mk[1],wp_mk[2],readable);wp_part(2,wp_mk[2],wp_mk[3],readable);
  if(wp_mk[2]>wp_mk[1])vx3dn_wstat[19]++;if(wp_mk[3]>wp_mk[2])vx3dn_wstat[20]++;if(readable)vx3dn_wstat[18]++;
  vstart=wp_mk[3];}
 vx3dn_wstat[6]+=vw;{uint32_t vp=ring_wordp-vstart;vx3dn_wstat[7]+=vp>vw?vp-vw:0;}
 vx3dn_wstat[9]+=vx3dn_geo[1]-g0[1];vx3dn_wstat[10]+=vx3dn_geo[2]-g0[2];vx3dn_wstat[11]+=vx3dn_geo[3]-g0[3];
}
void vx3dn_wprof_set(int on) {uint32_t k;if(on){for(k=0;k<24;k++)vx3dn_wstat[k]=0;for(k=0;k<3;k++)wp_ok[k]=0;}vx3dn_wprof=on?1:0;}
volatile uint32_t vx3dn_ringcap=1;
static uint32_t fx_build_b(const vx3dn_packet *p,uint32_t *last);
static uint32_t fx_build(const vx3dn_packet *p,uint32_t *last) {uint32_t o=vx_smp_ph,r;vx_smp_ph=VX_SMP_PH_BUILD;r=fx_build_b(p,last);vx_smp_ph=o;return r;} /* .520 */
static uint32_t fx_build_b(const vx3dn_packet *p,uint32_t *last) {
 vx3dn_io io;uint32_t rc,at=ring_wordp,t0=PT(),tb;int same;
 extern uint32_t vx3dn_ix_stat[6];const uint32_t ix1=vx3dn_ix_stat[1],ix2=vx3dn_ix_stat[2],ix3=vx3dn_ix_stat[3]; /* .564: a build that is not kept leaves the indexed counts as they were (an expansion that did not fit was counted as a packet drawn expanded) */
 cap_hdr=0;cap_err=0;cap_blocks=0;cap_skip=0;cap_form=0;
 vx3dn_wform=0;vx3dn_wend=0;vx3dn_cgroups=0; /* v10.282 */
 /* v10.287: the constant groups need only that a whole state set-up went to the engine in this state generation (the engine holds what
  * it was sent; vx3dn_state_gen counts everything that could change that), not the diff table */
 if(sd_gen!=vx3dn_state_gen){sd_gen=vx3dn_state_gen;sd_primed=0;} /* the engine's state is not known: start over */
 if(sd_primed&&vx3dn_cgroup)vx3dn_cgroups=3;
 {extern volatile uint32_t vx3dn_shadow;extern uint32_t vx3dn_sh_ok;vx3dn_sh_ok=sd_primed&&vx3dn_shadow?1UL:0UL;} /* .525: the set-up's register shadow, only while primed in this generation */
 cap_flags=(fx_inflight||ring_wordp||(fx_gring==2&&vx3dn_ringcap))?0x10:0x110; /* v10.466 (review): ACCEL_D3D_RINGCAP (default 1; 0 = as before): in XP's ring the
  * first packet's command-regulator blocks go into the group's words as every later packet's do, not to 43C/440 directly while the ring
  * holds its paused fetcher (the group then ran only by the v10.382 quirk, and a lost kick only came back by the hang path). v10.231: no register writes under a draw in flight; v10.275: nor after another packet of the group (they would come before its words run) */
 io.context=0;io.read=cap_read;io.write=cap_write;io.read_fb=read_fb;io.write_fb=write_fb;io.milliseconds=ticks;
 tb=PT();same=fx_same_state(p);vx3dn_vertices_only=(uint32_t)same;if(!same&&vx3dn_state_cache&&!(++vx3dn_miss_n&7))fx_classify(p); /* v10.429 */vx3dn_tex_only=!same&&fx_tex_only(p)?1UL:0; /* v10.450 */vx3dn_write_n=cap_write_n;vx3dn_reserve=cap_reserve;vx3dn_copyw=movsd_fn;
 {uint32_t t1=PT();vx3dn_bprof[0]+=(t1-tb+128)>>8;tb=t1;}
 vx3dn_tsc=vx_prof?tsc_fn:0;{uint32_t e0=vx3dn_eprof[0]+vx3dn_eprof[1]+vx3dn_eprof[2];const uint32_t i7=vx3dn_ixp[7];
 if(vx3dn_wprof){uint32_t k;for(k=0;k<4;k++){wp_mk[k]=0xFFFFFFFFUL;wp_g0[k]=vx3dn_geo[k];}vx3dn_mark=wp_mark;} /* v10.440 */
 rc=vx3dn_execute(p,&io,last);cap_close();vx3dn_mark=0;{extern uint32_t vx3dn_sh_ok;vx3dn_sh_ok=0;}if(same&&!vx3dn_vertices_only)same=0;vx3dn_vertices_only=0;if(vx3dn_tex_only&&!rc)vx3dn_texonly_stat[0]++;vx3dn_tex_only=0; /* v10.450 */ /* v10.296: the emitter sent the whole state (Coarse Z) */vx3dn_write_n=0;vx3dn_reserve=0;vx3dn_copyw=0;vx3dn_cgroups=0;vx3dn_tsc=0;
 {uint32_t t1=PT(),d=(t1-tb+128)>>8,e=vx3dn_eprof[0]+vx3dn_eprof[1]+vx3dn_eprof[2]-e0;vx3dn_bprof[1]+=d;vx3dn_eprof[3]+=d>e?d-e:0;tb=t1;if(p->pre.on==2&&vx3dn_ixp[7]!=i7)vx3dn_ixp[3]+=d;}} /* v10.473; v10.476: only the packets whose parts were timed ([7]: drawn from a slot with the profile on), so the set-up is their emitter less their parts (an expanded packet counted in [3] but not in [4..6] made it wrong) */
 if(rc||cap_err){ring_wordp=at;vx3dn_forget();vx3dn_ix_stat[1]=ix1;vx3dn_ix_stat[2]=ix2;vx3dn_ix_stat[3]=ix3;return rc?rc:FETCH_DECLINED;}
 /* v10.391 (review): an empty draw (clip of no area: vx3dn_execute returns before the state set-up) sends nothing: the state it
  * carried is not the engine's, so it neither primes the constant groups nor becomes the state the next draw is compared with */
 if(!same&&ring_wordp!=at&&sd_gen==vx3dn_state_gen)sd_primed=1;
 if(same)vx3dn_cache_stat[0]++;
 else if(ring_wordp!=at){uint32_t k;for(k=0;k<7;k++)copy_words((char*)&fx_prev+fx_ranges[k][0],(const char*)p+fx_ranges[k][0],fx_ranges[k][1]-fx_ranges[k][0]);fx_prev_gen=vx3dn_state_gen;fx_prev_ok=1;} /* v10.237: only the compared ranges; v10.232: the state the engine will hold */
 if(p->runs.n&&ring_wordp!=at&&fx_prev_ok){const vx3dn_texrun *r=&p->runs.r[p->runs.n-1]; /* .528: after texture runs the engine holds the last run's texture */
  fx_prev.tex.surface=r->surface;fx_prev.tex.filter=r->filter;fx_prev.tex.address_u=r->address_u;fx_prev.tex.address_v=r->address_v;fx_prev.mip=r->mip;}
 {uint32_t t1=PT();vx3dn_bprof[2]+=(t1-tb+128)>>8;vx3dn_bprof[3]++;}
 if(!fx_gpackets){fx_gtgt=p->target.offset;fx_gdep=(p->z_enable||p->z_write)?p->depth.offset:0xFFFFFFFFUL;} /* v10.386 */
 fx_gpackets++;vx3dn_cache_stat[1]+=(PT()-t0+128)>>8;fx_note(p,ring_wordp-at); /* v10.291 */
 if(vx3dn_wprof)wp_packet(at,wp_g0); /* v10.440 */
 return 0;
}
/* v10.382: before a start at s, the words between where the fetcher's last ones ended and s (at a wrap: the block's end) are written over
 * with no-op words as the board ran them (VXRING 43): the ring's kick padding blocks after a ring, the one-shot padding pairs after a
 * one-shot group; at most 8 KB (the writes from 64/65 to 68 run with interrupts off, about 1 microsecond: 1.5 KB at most on the board) */
static uint32_t fx_wipe_gap(uint32_t s) { /* v10.383: 0 nothing to write over, 1 written over, 2 the old position not known, 3 off */
 uint32_t p=fx_pend,blk=vx3dn_cmd_off,end=vx3dn_cmd_off+FX_BLOCK_BYTES,to;volatile uint32_t *w;
 if(p==0xFFFFFFFFUL||p<blk||p>=end){vx3dn_gap_stat[2]++;return 2;}
 if(!vx3dn_gap_wipe)return 3;
 to=p<=s?s:end;if(to-p>8192UL)to=p+8192UL;
 p=(p+7)&~7UL;if(to<=p)return 0;w=(volatile uint32_t*)(vx_status.fb_linear+p);vx3dn_gap_stat[0]++;
 for(;p+8<=to;p+=8){*w++=0xCCCCCCC0UL;*w++=0xDDD00000UL;vx3dn_gap_stat[1]+=2;}
 publish();fx_wc_flush(p);return 1;
}
/* v10.383: the fence: a 1x1 2D solid fill at FX_FENCE_OFF in the colour seq (as VXRING's fill packet) */
static void fx_fence_put(uint32_t seq) {
 uint32_t depth=hda->bpp==16?0x100UL:0x300UL,k;
 static const uint32_t reg[13]={0x04,0x08,0x14,0x1C,0x30,0x48,0x58,0x5C,0x20,0x4C,0x0C,0x10,0x00};uint32_t val[13];
 val[0]=depth;val[1]=(256UL>>3)<<16;val[2]=FX_FENCE_OFF>>3;val[3]=0;val[4]=0;val[5]=0;val[6]=seq&0xFFFFUL;val[7]=0;val[8]=0;
 val[9]=seq&0xFFFFUL;val[10]=0;val[11]=0;val[12]=0xF0002001UL;
 while(ring_wordp&3)ring_put(0xCC000000UL);
 ring_put(0xFE000000UL);ring_put(13);ring_put(0);ring_put(0xCC000000UL);
 for(k=0;k<13;k++){ring_put(reg[k]);ring_put(val[k]);}
 vx3dn_rf_stat[0]++;
}
static uint32_t xr_tick;

static uint32_t xr_kick(int async,uint32_t *last) { /* s3ginv 0xBC60A-0xBC74F */
 uint32_t k,seq,start=fx_gstart,pause,rc,t=PT();unsigned long f;
 seq=fx_seq+1;if(fx_rfenced)fx_fence_put(seq);
 fx_batch_end();for(k=0;k<4;k++)ring_put(0xCCC00004UL);while((ring_wordp*4)&0xFF)ring_put(0xCCC00004UL);
 pause=start+ring_wordp*4-0x10;
 XP_ADD(1,t);t=PT();
 if(ring_cpu==(volatile uint32_t*)xr_stage){if(ring_wordp)xr_copy((uint32_t*)(xr_lin+start-XR_GPU),xr_stage,ring_wordp);fx_sfence();vx3dn_xr_stat[11]=5;vx3dn_xr_unit[VX_XR_UNIT_NTWORDS]+=ring_wordp;} /* v10.411 */
 else xr_flush(start-XR_GPU,pause+0x10-XR_GPU);
 XP_ADD(2,t);t=PT();
 if(fx_rfenced&&fx_2d_dirty){for(k=0;k<14;k++)fx_2d[k]=read_mmio(0,fx_2d_reg[k]);vx3dn_xr_prof[8]++;} /* the 2D registers as they are now are what the next fence puts back */
 fx_2d_dirty=0;
 XP_ADD(3,t);t=PT();
 VX3DN_ev(1,seq&0xFFFFUL);
 if(xr_after_wrap){uint32_t *r=&vx3dn_xr_stat[62];xr_after_wrap=0;xr_wrapgrp=1; /* v10.405: the fetcher before the first kick after a wrap (the last one's) */
  r[0]=read_mmio(0,0x43C);r[1]=read_mmio(0,0x43C);r[2]=read_mmio(0,0x444);r[3]=read_mmio(0,0x45C);r[4]=read_mmio(0,0x464);r[5]=read_mmio(0,0x400);}
 f=VX_irq_save();
 write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x64000000UL|(pause&0xFFFFFFUL));write_mmio(0,0x420,0x65000000UL|(pause>>24));
 VX_irq_restore(f);
 XP_ADD(4,t);t=PT();vx3dn_xr_prof[10]++;
 xr_pend=pause+0x10;vx3dn_xr_stat[5]++;vx3dn_ring_stat[1]++;
 {uint32_t *e=fx_trace[fx_tn&63],v=0,k2;fx_seq=seq;xr_tick=xr_now(); /* the recorder, as the ring in video memory; v10.448: xr_now */
  for(k2=0;k2<fx_nsum;k2++)v+=fx_sum[k2][0]>>24;
  e[0]=(seq&0xFFFFUL)|((fx_rkicks?0UL:1UL)<<16)|((async?0UL:1UL)<<19)|((uint32_t)fx_rfenced<<20)|((fx_gpackets>255?255UL:fx_gpackets)<<24);
  e[1]=start;e[2]=pause;e[3]=ring_wordp;e[4]=(v&0xFFFFUL)|((xr_tick&0xFFFFUL)<<16);e[5]=0; /* v10.408: one clock call and no register read a kick (the recorder took 5.6 microseconds a packet) */
  e[6]=fx_nsum?fx_sum[0][0]:0;e[7]=fx_nsum?fx_sum[0][1]:0;e[8]=fx_gtgt;e[9]=fx_gdep;fx_tn++;fx_rkicks++;}
 {uint32_t k2,j;fx_inflight=1;fx_t0=xr_tick;fx_nfly=fx_nsum;for(k2=0;k2<fx_nsum;k2++)for(j=0;j<6;j++)fx_fly[k2][j]=fx_sum[k2][j];}
 rc=VX3DN_OK;XP_ADD(5,t);
 xr_next=(xr_next+ring_wordp*4+255)&~255UL;
 if(async)vx3dn_fetch_stat[5]++;
 else {uint32_t t2=PT();rc=xr_retire(1,0)?VX3DN_OK:fault?VX3DN_LOST:VX3DN_RECOVERED;vx3dn_bat_prof[2]+=(PT()-t2+128)>>8;*last=last_status;} /* v10.465 (review): xr_retire reset the engine for a hung group (or lost the device); RECOVERED, not LOST, so the caller does not reset it a second time */
 vx3dn_bat_prof[3]++;vx3dn_fetch_stat[0]+=fx_gpackets;vx3dn_fetch_stat[1]+=ring_wordp;if(ring_wordp>vx3dn_fetch_stat[4])vx3dn_fetch_stat[4]=ring_wordp;
 if(fx_gpackets>vx3dn_batch_stat[3])vx3dn_batch_stat[3]=fx_gpackets;
 if(rc)vx3dn_forget();
 fx_gpackets=0;
 return rc;
}
static uint32_t fx_start_b(int async,uint32_t *last);
static uint32_t fx_start(int async,uint32_t *last) {uint32_t o=vx_smp_ph,r;vx_smp_ph=VX_SMP_PH_KICK;r=fx_start_b(async,last);vx_smp_ph=o;return r;} /* .520 */
static uint32_t fx_start_b(int async,uint32_t *last) {
 uint32_t rc,start=fx_gstart,end=vx3dn_cmd_off+FX_BLOCK_BYTES,pause,t1;
 if(fx_gring==2)return xr_kick(async,last); /* v10.402 */
 fx_batch_end();
 while((ring_wordp*4)&0x1F){ring_put(0xCCCCCCC0UL);ring_put(0xDDD00000UL);}
 pause=start+ring_wordp*4-4;
 publish(); /* the captured words out of the CPU's write-combining buffers */
 fx_wc_flush(pause+4); /* v10.390 */
 t1=PT();
 /* v10.281 (board, VXDX7 --texture): the draw before is retired here; when it hung, this group was lost with it (the caller reset the
  * engine and the packets it had counted as taken were never drawn: the draw after VXDX7's hang test read black, the hang itself was
  * only seen one draw late). The engine is reset here and the caller builds the group again on the reset engine (FX_REBUILD): the
  * group was built for the engine state the hung draw left (its command-regulator blocks left out, the state cache). A reset that
  * fails loses the device here (fault: the caller does not reset again). */
 if(!fx_retire_stop(0)) {
  vx3dn_forget();
  if(vx3dn_recover&&engine_reset()){vx3dn_rec[0]++;vx3dn_forget();return FX_REBUILD;}
  vx3dn_rec[1]++;fault=1;VX2D_device_lost();return VX3DN_LOST;
 }
 vx3dn_cache_stat[2]+=(PT()-t1+128)>>8;
 fx_wipe_gap(start); /* v10.382 */
 {unsigned long f=VX_irq_save();
 write_mmio(0,0x41C,0x00100000UL);
 write_mmio(0,0x420,0x60000000UL|(start&0xFFFFFFUL));write_mmio(0,0x420,0x61000000UL|(start>>24));
 write_mmio(0,0x420,0x62000000UL|(end&0xFFFFFFUL));write_mmio(0,0x420,0x63000000UL|(end>>24));
 write_mmio(0,0x420,0x64000000UL|(pause&0xFFFFFFUL));write_mmio(0,0x420,0x65000000UL|(pause>>24)|0x200UL);
 write_mmio(0,0x420,0x680A0001UL);
 VX_irq_restore(f);}
 fx_pend=pause+4;
 if(async){uint32_t k,j;fx_inflight=1;va_started=va_gid&0x7FFFFFFFUL;fx_t0=xr_now();rc=VX3DN_OK;vx3dn_fetch_stat[5]++;fx_nfly=fx_nsum;for(k=0;k<fx_nsum;k++)for(j=0;j<6;j++)fx_fly[k][j]=fx_sum[k][j];}
 else {uint32_t t2=PT();
  rc=fetch_idle(last)?VX3DN_OK:VX3DN_LOST;vx3dn_bat_prof[2]+=(PT()-t2+128)>>8;
  write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x680A0000UL);
  if(rc){vx3dn_fetch_stat[3]++;hang_note(*last,fx_sum,fx_nsum);}}
 vx3dn_bat_prof[3]++; vx3dn_fetch_stat[0]+=fx_gpackets;vx3dn_fetch_stat[1]+=ring_wordp;if(ring_wordp>vx3dn_fetch_stat[4])vx3dn_fetch_stat[4]=ring_wordp;
 if(fx_gpackets>vx3dn_batch_stat[3])vx3dn_batch_stat[3]=fx_gpackets;
 if(rc)vx3dn_forget();
 fx_next=(fx_next+ring_wordp*4+255)&~255UL;fx_gpackets=0;
 return rc;
}
/* .564: an indexed packet that found no free slot in the vertex area is drawn with its indices expanded inline; one of more than some
 * 790 indices is then more words than a fetch group holds, even alone, and went through the registers after the ring was stopped
 * (correct, and many times slower). The slots were busy only because the groups that read them are in flight: the work in flight is
 * waited for, every slot is free, and the packet is built again by index. vx3dn_ix_wait: [0] packets drawn so, [1] of them still too
 * large after it (through the registers as before). */
uint32_t vx3dn_ix_wait[2];
static uint32_t fx_build(const vx3dn_packet *p,uint32_t *last);
static uint32_t fx_ix_again(const vx3dn_packet *p,uint32_t *last) {
 uint32_t rc;
 if(!fx_retire(0))return VX3DN_LOST;
 vx3dn_ix_wait[0]++;fx_open_group();rc=fx_build(p,last);if(rc==FETCH_DECLINED)vx3dn_ix_wait[1]++;
 return rc;
}
static uint32_t fetch_draw(const vx3dn_packet *p,uint32_t *last,int async) {
 uint32_t rc,t0=PT();
 if(!fx_inflight&&!fetch_idle(last))return VX3DN_LOST; /* what the register path's first wait does (with a draw in flight: before the start) */
 fx_open_group();
 rc=fx_build(p,last);
 if(rc==FETCH_DECLINED&&p->pre.on==2&&fx_inflight)rc=fx_ix_again(p,last); /* .564 */
 if(rc==FETCH_DECLINED){vx3dn_fetch_stat[2]++;return rc;}
 if(rc)return rc; /* the emitter's own refusals (its waits read idle here) */
 rc=fx_start(async,last);
 if(rc==FX_REBUILD) { /* v10.281: the draw before hung; the engine was reset: this draw again, whole, on the reset engine */
  fx_open_group();rc=fx_build(p,last);
  if(rc==FETCH_DECLINED){vx3dn_fetch_stat[2]++;return rc;}
  if(rc)return rc;
  rc=fx_start(async,last);if(rc==FX_REBUILD)rc=VX3DN_LOST; /* nothing is in flight now */
 }
 vx3dn_cache_stat[3]+=(PT()-t0+128)>>8;
 return rc;
}
/* v10.275: several draw packets in one call (VX_IOCTL_3DN_SUBMITN): one entry, one sequencer open and close, the packets' words built
 * one after another into fetch groups (FX_PACKET_WORDS kept free for the next one; a group that is full is started and the next one
 * is built while it runs), one start a group. Offscreen draw packets only (VXGL queues nothing else). *done = the packets taken (a
 * refused one stops the batch: the ones before it are sent, its result is returned). Without the fetcher every packet goes alone. */
static uint32_t fx_vhdr[27]; /* .524: the header (range 0) of the last packet validated in a batch call */
typedef char fx_vhdr_size_check[sizeof(fx_vhdr)==offsetof(vx3dn_packet,vertex_count)?1:-1]; /* .524: the header compare covers range 0 exactly */
/* .528 (.529: always; TEXDBG BC FA reads the counts): the batch call's open and close of the sequencer (SR10 bit 0, SR1A bit 3,
 * SR1E bit 0: the 3D engine ungated) write a register only when it does not already hold the value, and the clock check (SR3C) shares the
 * open's interrupts-off section; with all three already set (the common case: async draws depend on it) the close does nothing. Every
 * VGA sequencer access is two port I/Os (the open and close made up to 27, about 23 us a call with the dispatch, cookbook 31.3).
 * vx3dn_seq_stat: [0] batch calls opened, [1] writes left out at the open, [2] closes with nothing to put back */
uint32_t vx3dn_seq_stat[3];
static const vx3dn_packet *bad_src; /* .527 */
static uint32_t batch_impl(const vx3dn_packet *ps,uint32_t n,uint32_t *done) {
 vx3dn_info i;vx3dn_io io;uint32_t rc=VX3DN_OK,r2,base,k,gfirst=0,gdone=0,gsub=0,gtri=0,cp_k=0xFFFFFFFFUL; /* .524: cp_k = the packet request_copy holds, validated */unsigned long f;unsigned char old,unlock,a,b,locked;int opened=0,async,grp=0,excluded=0;
 *done=0;sub_t[0]=PT();sub_t[1]=sub_t[2]=sub_t[3]=0;SMP_PH(VX_SMP_PH_B_ENTER); /* .526 */
 cs_enter();
 {int b3=VX3D_lab_busy()||(int)fault,b2=b3?0:VX2D_busy();
  if(b3||b2) {rc=(fault||VX2D_lost())?VX3DN_LOST:VX3DN_BUSY;if(rc==VX3DN_BUSY)last_result=VX3DN_BUSY|1UL<<8|(b3?1UL:2UL)<<16;cs_leave();return rc;}}
 running=1;VX3DN_info(&i);
 base=vx_status.bar1&0xFFFFFFF0UL;
 if((vx_status.bar1&15)||base<0x100000UL||base>0xFFC00000UL) {rc=VX3DN_LOST;goto done;}
 if(!mapped) mapped=_MapPhysToLinear(base,0x400000UL,0);
 if(!mapped||mapped==0xFFFFFFFFUL) {mapped=0;rc=VX3DN_LOST;goto done;}
 sub_t[1]=PT();SMP_PH(VX_SMP_PH_B_OPEN); /* .526 */
 f=VX_irq_save();old=inp(0x3C4);unlock=sr(0x10);vx3dn_seq_stat[0]++;
 if(!(unlock&1))sw(0x10,(unsigned char)(unlock|1));else vx3dn_seq_stat[1]++; /* .528: a write of the value the register holds is left out */
 a=sr(0x1A);b=sr(0x1E);
 if((a&0x42)||(sr(0x4E)&0x30)) {busy_why=2UL<<8|(uint32_t)a<<16|(uint32_t)sr(0x4E)<<24;if(!(unlock&1))sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);rc=VX3DN_BUSY;goto done;}
 if(!(a&8))sw(0x1A,(unsigned char)(a|8));else vx3dn_seq_stat[1]++;
 if(!(b&1))sw(0x1E,(unsigned char)(b|1));else vx3dn_seq_stat[1]++;
 opened=1;
 locked=sr(0x3C);outp(0x3C4,old);VX_irq_restore(f); /* .528: the clock check in the same section */
 if(!(locked&0x10)) {busy_why=3UL<<8|(uint32_t)locked<<16;rc=VX3DN_BUSY;goto finish;}
 async=vx3dn_async&&(a&8)&&(b&1);
 io.context=0;io.read=read_mmio;io.write=write_mmio;io.read_fb=read_fb;io.write_fb=write_fb;io.milliseconds=ticks;
 sub_t[2]=PT();
 publish();
 if(!fx_inflight&&!fetch_idle(&last_status)){rc=VX3DN_LOST;goto lost;}
 vx3dn_batch_stat[0]++;SMP_PH(VX_SMP_PH_B_LOOP); /* .526 */
 /* v10.281: a start that finds the draw before hung (FX_REBUILD: the engine was reset) builds its group again from the group's first
  * packet: gfirst, and what was counted for the group (gdone, gsub, gtri) taken back */
 k=0;
again:
 for(;k<n;k++) {
  const vx3dn_packet *p=(const vx3dn_packet*)((const char*)ps+k*sizeof(vx3dn_packet));
  if(p->cb!=sizeof(*p)||p->op!=VX3DN_DRAW){rc=VX3DN_BAD_PACKET;bad_vline=__LINE__;bad_src=p;break;} /* .527: bad_src, the caller's packet (not copied) */
  {uint32_t t0=PT(),t1;int geo;SMP_PH(VX_SMP_PH_COPY);cp_defer=1;copy_packet_d(&request_copy,p,k&&cp_k==k-1);cp_defer=0;t1=PT();vx3dn_bat_prof[0]+=(t1-t0+128)>>8;
   SMP_PH(VX_SMP_PH_VALID);cp_k=0xFFFFFFFFUL;if(request_copy.cb!=sizeof(request_copy)||request_copy.op!=VX3DN_DRAW){rc=VX3DN_BAD_PACKET;bad_vline=__LINE__;SMP_PH(VX_SMP_PH_B_LOOP);break;} /* .524: cb and op checked again on the copy (the check above reads the caller's memory: a packet changed meanwhile could bring a V1 or clear packet into the batch path) */ /* .524: the state deltas (copy_packet_d); every range kept and the header unchanged: the state's checks not again */
   geo=cp_same==VX3DN_SAME_MASK&&!vx_cmpsd(&request_copy,fx_vhdr,sizeof(fx_vhdr)>>2);if(geo)vx3dn_delta_stat[3]++;
   rc=geo?vx3dn_validate_geo(&request_copy,&i):vx3dn_validate(&request_copy,&i);if(!rc){cp_k=k;if(!geo)copy_words(fx_vhdr,&request_copy,sizeof(fx_vhdr));}SMP_PH(VX_SMP_PH_B_LOOP);{uint32_t t2=PT();vx3dn_bat_prof[1]+=(t2-t1+128)>>8;
   if(request_copy.pre.on==2){vx3dn_ixp[0]++;vx3dn_ixp[1]+=(t1-t0+128)>>8;vx3dn_ixp[2]+=(t2-t1+128)>>8;}}}if(rc)break; /* v10.473 */
  if(vx3dn_prefetch&&k+1<n){if(fx_sse<0)fx_sse=fx_cpuid_ok()?(int)((fx_cpuid1_edx()>>25)&1):0;if(fx_sse)fx_prefetch_packet((const vx3dn_packet*)((const char*)p+sizeof(vx3dn_packet)));} /* v10.412 */
  /* v10.368 (audit N8): a draw on the primary's memory (every other frame of a flip chain) is taken too: the cursor is excluded from
   * the first such packet to the end of the batch, and the batch's last group is then started synchronously (as a single draw there:
   * the cursor is drawn again only once the engine is done). It was refused (BAD_PACKET; the HAL sent the primary's packets alone). */
  if(!excluded&&VESA_draw_needs_exclusion(request_copy.target.offset,request_copy.target.pitch*request_copy.target.height)){FBHDA_access_begin(0);excluded=1;} /* v10.371: only when the cursor may be on it (was: offset 0) */
  if(!grp){fx_open_group();grp=1;gfirst=k;gdone=*done;gsub=submitted;gtri=triangles;}
  else if(fx_gpackets&&ring_wordp+FX_PACKET_WORDS+request_copy.runs.n*FX_RUN_WORDS>cap_max){vx3dn_batch_stat[2]++;rc=fx_start(1,&last_status);grp=0;
   if(rc==FX_REBUILD){k=gfirst;*done=gdone;submitted=gsub;triangles=gtri;rc=VX3DN_OK;goto again;}
   if(rc)break;fx_open_group();grp=1;gfirst=k;gdone=*done;gsub=submitted;gtri=triangles;}
  rc=fx_build(&request_copy,&last_status);
  if(rc==FETCH_DECLINED&&fx_gpackets){vx3dn_batch_stat[2]++;rc=fx_start(1,&last_status);grp=0;
   if(rc==FX_REBUILD){k=gfirst;*done=gdone;submitted=gsub;triangles=gtri;rc=VX3DN_OK;goto again;}
   if(rc)break;fx_open_group();grp=1;gfirst=k;gdone=*done;gsub=submitted;gtri=triangles;rc=fx_build(&request_copy,&last_status);}
  if(rc==FETCH_DECLINED&&request_copy.pre.on==2&&fx_inflight){rc=fx_ix_again(&request_copy,&last_status);grp=rc==VX3DN_OK;if(grp){gfirst=k;gdone=*done;gsub=submitted;gtri=triangles;}} /* .564: by index once the slots are free (fx_ix_again) */
  if(rc==FETCH_DECLINED) { /* too many words for a fetch even alone (the group is empty): through the registers, as a single draw */
   vx3dn_fetch_stat[2]++;grp=0;
   if(!fx_retire_stop(0))rc=VX3DN_LOST;else{vx3dn_reg_draws++;vx3dn_forget();rc=vx3dn_execute(&request_copy,&io,&last_status);}
  }
  if(rc)break;
  submitted++;triangles+=packet_triangles(&request_copy);(*done)++;vx3dn_batch_stat[1]++;
 }
 SMP_PH(VX_SMP_PH_B_CLOSE); /* .526 (the last group's start sets its own phase) */
 if(grp&&fx_gpackets){vx3dn_batch_stat[2]++;r2=fx_start(async&&!excluded,&last_status);
  if(r2==FX_REBUILD){grp=0;k=gfirst;*done=gdone;submitted=gsub;triangles=gtri;rc=VX3DN_OK;goto again;}
  if(r2==VX3DN_LOST||!rc)rc=r2;}
 vx3dn_cache_stat[3]+=(PT()-sub_t[2]+128)>>8; /* v10.276: the fetcher log's "in all" for batches too */
lost:
 if(rc==VX3DN_LOST) {
  /* as a single draw: never replay, never race a hung engine */
  if(fault) ; /* v10.281: fx_start's reset already failed and lost the device */
  else if(vx3dn_recover&&engine_reset()){vx3dn_rec[0]++;vx3dn_forget();rc=VX3DN_RECOVERED;}
  else{vx3dn_rec[1]++;fault=1;VX2D_device_lost();}
 } else publish();
 sub_t[3]=PT();
finish:
 if(opened&&(!(a&8)||!(b&1)||!(unlock&1))) { /* .528: nothing to put back when every register already held what the open wrote */
  f=VX_irq_save();old=inp(0x3C4);
  if(!fault){if(!(b&1))sw(0x1E,b);if(!(a&8))sw(0x1A,a);}
  if(!(unlock&1))sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);
 } else if(opened)vx3dn_seq_stat[2]++;
 if(excluded&&!fault){if(fx_inflight)(void)fx_retire(1);if(!fault)FBHDA_access_end(0);} /* v10.368: as a single draw on the primary. v10.466 (review): a group started
  * asynchronously within the batch (a group split) is finished before the cursor is drawn again: a batch that ended early (a bad
  * packet after a split) left one running */
done:
 if(rc==VX3DN_BAD_PACKET){const vx3dn_packet *q=bad_src?bad_src:&request_copy; /* .527: the packet refused, with its index */
  bad_note(2,k,n,q,!bad_src&&cp_same==VX3DN_SAME_MASK&&!vx_cmpsd(&request_copy,fx_vhdr,sizeof(fx_vhdr)>>2));bad_vline=0;bad_src=0;}
 {extern uint32_t vx3dn_stage;last_result=rc==VX3DN_LOST?rc|(vx3dn_stage&255)<<8:rc==VX3DN_BUSY?rc|busy_why:rc;busy_why=0;}
 running=0;cs_leave();
 if(sub_t[3]){uint32_t t=PT();vx3dn_sub_stat[0]+=(sub_t[1]-sub_t[0]+128)>>8;vx3dn_sub_stat[1]+=(sub_t[2]-sub_t[1]+128)>>8;
  vx3dn_sub_stat[2]+=(sub_t[3]-sub_t[2]+128)>>8;vx3dn_sub_stat[3]+=(t-sub_t[3]+128)>>8;}
 return rc;
}
/* v10.283: why does building a packet cost about 57 ns a command word on the board when the VxD's instructions for it halved (v10.282:
 * 30.6 -> 25.3 microseconds a packet, 438 words)? The command block is the top of the video memory the driver found (256 KB below the
 * save block); the write-combining range may cover less (the MTRR set for the size VBE reported, or the BIOS's). TEXDBG 0xC7: [1] the
 * block's offset, [2] its physical address, [3] its MTRR memory type, [4] the frame buffer start's, [5] the block's page-table entry,
 * [6] TSC cycles for 16 KB of CPU dword writes into the block (after the draw in flight is retired; nothing reads the block then),
 * [7] the same for 16 KB of dword writes to a cached buffer (the CPU's own speed). */
unsigned long VX_cache_type_at(unsigned long phys);
static uint32_t probe_buf[4096];
void VX3DN_cmd_probe(uint32_t *o) {
 uint32_t k,t0,phys,e=0;volatile uint32_t *d;
 o[1]=vx3dn_cmd_off;phys=vx_status.fb_phys+vx3dn_cmd_off;o[2]=vx3dn_cmd_off?phys:0;
 o[3]=vx3dn_cmd_off?VX_cache_type_at(phys):0xFFUL;o[4]=VX_cache_type_at(vx_status.fb_phys);
 if(!vx3dn_cmd_off||!vx_status.fb_linear){o[5]=o[6]=o[7]=0;return;}
 cs_enter();
 if(!fx_retire_stop(1)){o[5]=o[6]=o[7]=0;cs_leave();return;} /* v10.330 (review #159): no write into the command block unless the draw before was retired idle */
 _CopyPageTable((vx_status.fb_linear+vx3dn_cmd_off)>>12,1,(DWORD*)&e,0);o[5]=e&0xFFFUL;
 d=(volatile uint32_t*)(vx_status.fb_linear+vx3dn_cmd_off);
 t0=VX_tsc();for(k=0;k<4096;k++)d[k]=0xCC000000UL;(void)d[4095];o[6]=VX_tsc()-t0;
 t0=VX_tsc();for(k=0;k<4096;k++)((volatile uint32_t*)probe_buf)[k]=k;o[7]=VX_tsc()-t0;
 vx3dn_forget();
 cs_leave();
}
static uint32_t batch_entry(const vx3dn_packet *ps,uint32_t n,uint32_t *done);
/* v10.414 (board, v10.413: 300-475 DrawPrimitives2 calls a session of 10 ms and more, about 23 ms of each in its 1-3 submit calls, while
 * no VxD batch or single submit took 10 ms): a submit IOCTL's entry and exit as the VxD saw them, for the HAL's slow-call records:
 * o[0] TSC at entry, o[1] TSC at exit, o[2] the calling thread's run time at entry, o[3] at exit (ms, Get_Thread_Exec_Time) */
void VX3DN_io_mark(uint32_t *o,int at_exit) {
 void *th=Get_Cur_Thread_Handle();
 if(!at_exit){o[0]=VX_tsc();o[2]=Get_Thread_Exec_Time(th);}else{o[1]=VX_tsc();o[3]=Get_Thread_Exec_Time(th);}
}
uint32_t VX3DN_submit_batch(const vx3dn_packet *ps,uint32_t n,uint32_t *done) { /* v10.388: the slow calls kept */
 uint32_t t=VX_tsc(),w0,b0=vx3dn_batch_stat[2],r0=vx3dn_rec[0],l0=vx3dn_rf_stat[1],k0=vx3dn_rf_stat[4],rc,d,k,lo=0;
 void *th=Get_Cur_Thread_Handle();uint32_t x0=vx3dn_fastclock?0xFFFFFFFFUL:Get_Thread_Exec_Time(th),m0=xr_now(); /* v10.449 */
 VX3DN_ev(8,n);sv_wait=0;w0=sv_wait;rc=batch_entry(ps,n,done);d=(VX_tsc()-t)>>8;
 if(d>=39062UL){ /* 10 ms at the board's 1000 MHz */vx3dn_sv_count++;for(k=1;k<8;k++)if(vx3dn_sv_worst[k][0]<vx3dn_sv_worst[lo][0])lo=k;
  if(d>vx3dn_sv_worst[lo][0]){uint32_t tri=0,j;for(j=0;j<*done&&j<n;j++)tri+=packet_triangles((const vx3dn_packet*)((const char*)ps+j*sizeof(vx3dn_packet)));
   vx3dn_sv_worst[lo][0]=d;vx3dn_sv_worst[lo][1]=sv_wait-w0;vx3dn_sv_worst[lo][2]=*done;vx3dn_sv_worst[lo][3]=tri;vx3dn_sv_worst[lo][4]=vx3dn_batch_stat[2]-b0;
   vx3dn_sv_worst[lo][5]=vx3dn_rec[0]-r0;vx3dn_sv_worst[lo][6]=vx3dn_rf_stat[1]-l0;vx3dn_sv_worst[lo][7]=vx3dn_rf_stat[4]-k0;
   vx3dn_sv_worst[lo][8]=x0==0xFFFFFFFFUL?0xFFFFFFFFUL:Get_Thread_Exec_Time(th)-x0;vx3dn_sv_worst[lo][9]=xr_now()-m0;}}
 return rc;
}
static uint32_t batch_entry(const vx3dn_packet *ps,uint32_t n,uint32_t *done) {
 uint32_t t0=PT(),rc,k,has_clear=0;
 *done=0;
 if(!ps||!n){bad_vline=__LINE__;bad_note(2,0,n,0,0);bad_vline=0;return VX3DN_BAD_PACKET;}
 va_hooks(); /* v10.470 */
 for(k=0;k<n;k++){
  if(ps[k].op==VX3DN_CLEAR||(ps[k].clear_flags&VX3DN_DRAW_CLEAR))has_clear=1; /* v10.456: a clear in a batch too */
  if(front_wait_needed(&ps[k]))VX_flip_front_wait(ps[k].target.offset);} /* v10.384; v10.416: only for colour */
 if(!vx3dn_fetch||!vx3dn_cmd_off||has_clear) { /* internal clears also suspend the occlusion count */
  for(k=0;k<n;k++){rc=VX3DN_submit((const vx3dn_packet*)((const char*)ps+k*sizeof(vx3dn_packet)));if(rc&&rc!=VX3DN_RECOVERED)return rc;(*done)++;if(rc)return rc;}
  return VX3DN_OK;
 }
 if((rc=q_before_draw())!=0)return rc; /* v10.322 */
 rc=batch_impl(ps,n,done);
 LAT(&vx3dn_lat[3],PT()-t0); /* v10.321 */
 vx3dn_prof[0]+=*done;vx3dn_prof[1]+=(PT()-t0+128)>>8;
 return rc;
}
/* v10.315: GL_ARB_occlusion_query on the engine (HCM p83-84). 01/1C (low 24 bits) and 01/1D (high 8) are the count of pixels that passed
 * the Z and stencil tests; writing them sets it. 01/21 (low 24 bits) and 01/22 (bits 7:0 high, bits 23:22 = 00 the local frame buffer) are
 * an address in 4-byte units: an address that is not 0 makes the engine write the count there. op 0: the count set to 0 (a query begins);
 * op 1: the count written to the command block's last 16 bytes (nothing in flight uses them: the draw before is retired first) and read
 * back. Outside any packet: the in-flight group retired, the sequencer opened as for a draw, the engine idle before and after. */
uint32_t vx3dn_query_stat[4]; /* resets, reads, reads with no answer, the last count */
static uint32_t q_hw(uint32_t op,uint32_t *count) {
 uint32_t rc=VX3DN_OK,base,st=0,s;unsigned long f;unsigned char old,unlock,a,b,locked;int opened=0;
 *count=0;
 if(op>1)return VX3DN_BAD_PACKET;
 cs_enter();
 if(!vx3dn_cmd_off){cs_leave();return VX3DN_UNSUPPORTED;}
 (void)fx_retire_stop(1); /* v10.391: the query's register writes with the fetcher stopped */
 {int b3=VX3D_lab_busy()||(int)fault,b2=b3?0:VX2D_busy();
  if(b3||b2){rc=(fault||VX2D_lost())?VX3DN_LOST:VX3DN_BUSY;cs_leave();return rc;}}
 running=1;
 base=vx_status.bar1&0xFFFFFFF0UL;
 if((vx_status.bar1&15)||base<0x100000UL||base>0xFFC00000UL){rc=VX3DN_LOST;goto done;}
 if(!mapped)mapped=_MapPhysToLinear(base,0x400000UL,0);
 if(!mapped||mapped==0xFFFFFFFFUL){mapped=0;rc=VX3DN_LOST;goto done;}
 f=VX_irq_save();old=inp(0x3C4);unlock=sr(0x10);sw(0x10,(unsigned char)(unlock|1));
 a=sr(0x1A);b=sr(0x1E);
 if((a&0x42)||(sr(0x4E)&0x30)){sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);rc=VX3DN_BUSY;goto done;}
 sw(0x1A,(unsigned char)(a|8));sw(0x1E,(unsigned char)(b|1));
 opened=1;outp(0x3C4,old);VX_irq_restore(f);
 f=VX_irq_save();old=inp(0x3C4);locked=sr(0x3C);outp(0x3C4,old);VX_irq_restore(f);
 if(!(locked&0x10)){rc=VX3DN_BUSY;goto finish;}
 if(!fetch_idle(&st)){rc=VX3DN_LOST;goto finish;}
 s=vx3dn_cmd_off+VX3DN_CMD_BYTES-16UL;
 {extern void vx3dn_sh_forget(void);vx3dn_sh_forget();} /* .525: type-01 registers written here: the set-up's shadow starts over */
 write_mmio(0,0x43C,0x00010000UL);
 if(!op){write_mmio(0,0x440,0x1C000000UL);write_mmio(0,0x440,0x1D000000UL);vx3dn_query_stat[0]++;}
 else {
  uint32_t v=0xFFFFFFFFUL,t0;
  write_fb(0,s,0xFFFFFFFFUL);(void)read_fb(0,s); /* a marker no count reaches; the read drains the write-combining buffer */
  write_mmio(0,0x440,0x21000000UL|((s>>2)&0xFFFFFFUL));write_mmio(0,0x440,0x22000000UL|((s>>26)&0xFFUL));
  if(!fetch_idle(&st)){rc=VX3DN_LOST;goto finish;}
  t0=Get_System_Time();
  do v=read_fb(0,s);while(v==0xFFFFFFFFUL&&(uint32_t)(Get_System_Time()-t0)<5UL);
  write_mmio(0,0x440,0x21000000UL);write_mmio(0,0x440,0x22000000UL);
  vx3dn_query_stat[1]++;
  if(v==0xFFFFFFFFUL){vx3dn_query_stat[2]++;rc=VX3DN_UNSUPPORTED;}else{*count=v;vx3dn_query_stat[3]=v;}
 }
finish:
 if(opened){f=VX_irq_save();old=inp(0x3C4);if(!fault){sw(0x1E,b);sw(0x1A,a);}sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);}
done:
 running=0;cs_leave();
 return rc;
}
/* v10.322 (review: another context's drawing was counted into a query, and its query set the one count to 0 under the first): the count is
 * one context's at a time. VX3DN_query(op, owner): 0 a query begins (the count set to 0; BUSY while another owner's query counts: that one
 * counts in software), 1 it ends (its samples: what was put aside plus the count), 2 its context stops being current (the count put aside),
 * 3 current again (the count set to 0, adding from there), 4 its context is deleted (given up). A draw from another thread while a query
 * counts (another context, another process, Direct3D) puts the count aside first; the owner's next draw sets it to 0 again, so the others'
 * samples are not in it. q_away: the owner's context is not current (op 2): its own thread's draws are not its own then either. */
static uint32_t q_owner,q_acc,q_aside,q_away,q_bad;static void *q_thread;
uint32_t vx3dn_query_switch[2]; /* the times the count was put aside for another thread's draw; begins refused (another owner's query) */
/* q_bad: a put-aside or take-back failed: the count is not the owner's alone, its end answers UNSUPPORTED (not a wrong number). BUSY (the
 * 2D side running) is answered to the draw's caller, which sends it again (nothing was sent), as the draw's own BUSY */
static uint32_t q_put_aside(void) {uint32_t v=0,rc;if(q_aside)return VX3DN_OK;rc=q_hw(1,&v);if(rc==VX3DN_BUSY)return rc;if(rc){q_bad=1;return VX3DN_OK;}q_acc+=v;q_aside=1;return VX3DN_OK;}
static uint32_t q_take_back(void) {uint32_t v,rc;if(!q_aside)return VX3DN_OK;rc=q_hw(0,&v);if(rc==VX3DN_BUSY)return rc;if(rc){q_bad=1;return VX3DN_OK;}q_aside=0;return VX3DN_OK;}
static uint32_t q_before_clear(void) {uint32_t rc=VX3DN_OK;
 cs_enter();if(q_owner)rc=q_put_aside();cs_leave();return rc;}
/* before a draw (VX3DN_submit, VX3DN_submit_batch): another thread's puts the count aside; the owner's takes it back */
static uint32_t q_before_draw(void) {
 uint32_t rc=VX3DN_OK;
 if(!q_owner)return rc;
 cs_enter();
 if(q_owner){void *t=Get_Cur_Thread_Handle();
  if(t!=q_thread||q_away){if(!q_aside){rc=q_put_aside();if(!rc)vx3dn_query_switch[0]++;}}
  else rc=q_take_back();}
 cs_leave();
 return rc;
}
uint32_t VX3DN_query(uint32_t op,uint32_t owner,uint32_t *count) {
 uint32_t rc=VX3DN_OK,v=0;
 *count=0;
 if(op>4||!owner)return VX3DN_BAD_PACKET;
 cs_enter();
 if(op==0){
  if(q_owner&&q_owner!=owner){vx3dn_query_switch[1]++;rc=VX3DN_BUSY;}
  else if(!(rc=q_hw(0,&v))){q_owner=owner;q_thread=Get_Cur_Thread_Handle();q_acc=0;q_aside=0;q_away=0;q_bad=0;}
 }
 else if(q_owner!=owner)rc=op==1?VX3DN_UNSUPPORTED:VX3DN_OK;
 else if(op==1){
  if(!q_aside&&!q_bad)rc=q_hw(1,&v);
  if(q_bad)rc=VX3DN_UNSUPPORTED;else if(!rc)*count=q_acc+v;
  q_owner=0;
 }
 else if(op==2){q_away=1;rc=q_put_aside();}
 else if(op==3){q_away=0;q_thread=Get_Cur_Thread_Handle();rc=q_take_back();}
 else q_owner=0; /* 4 */
 cs_leave();
 return rc;
}
static uint32_t f2u(float f) {union {float f;uint32_t u;} x;x.f=f;return x.u;}
static void ring_triangle(vx3dn_packet *p,const vx_ring_reply *r,const vx_ring_req *q,const vx3dn_info *i) {
 static const float vx[3]={4.0f,60.0f,10.0f},vy[3]={2.0f,8.0f,30.0f};uint32_t k;
 memset(p,0,sizeof(*p));p->cb=sizeof(*p);p->abi=VX3DN_ABI;p->arm=VX3DN_ARM;p->op=VX3DN_DRAW;p->generation=i->generation;
 p->target.offset=r->tgt_offset;p->target.pitch=q->tgt_pitch;p->target.width=q->width;p->target.height=q->height;p->target.format=r->bpp==16?VX3DN_RGB565:VX3DN_XRGB8888;
 p->clip.right=(int32_t)q->width;p->clip.bottom=(int32_t)q->height;p->z_func=8;p->write_mask=15;p->vertex_count=3;
 for(k=0;k<3;k++){p->vertices[k].x=f2u(vx[k]);p->vertices[k].y=f2u(vy[k]);p->vertices[k].z=f2u(0.5f);p->vertices[k].rhw=f2u(1.0f);p->vertices[k].color=0xFF000000UL|q->color;}
}
/* v1.5 (v10.373): op 5, the ring append. Can a second group be appended while the fetcher still runs the first (XP's kick: the new
 * words after the old, 0x41C, the pause registers 64/65 moved on, 68 = 0A0001 as VXRING test 8 needed), and what do the registers say
 * when everything kicked is done (in the ring form 0x400 stays busy at the pause, v10.225)? Band A (the request's rectangle) gets
 * group A, flags bits 16-23 copies of the 2D fill; band B (the rectangle below it) group B: the fill in the colour's complement less
 * one, or (flag 0x800) the 3D triangle captured in XP's forms. Flag 0x400: kick B only once band A is drawn. */
/* v10.406: VXRING ops 5-12 (the ring in video memory: appends, backlog, restart, wrap, the tape's replays) are gone with that ring */
uint32_t VX3DN_ring(const vx_ring_req *q,vx_ring_reply *r) {
 unsigned long f;unsigned char old,unlock,a=0,b=0;uint32_t k,t0,n,i,st,stable=0,lin_end;int opened=0;
 memset(r,0,sizeof(*r));r->cb=sizeof(*r);r->abi=VX_RING_ABI;
 if(!q||q->cb!=sizeof(*q)||q->abi!=VX_RING_ABI||q->op>4||!hda){r->result=1;return 1;}
 cs_enter();
 if((vx3dn_rt_why=6,VX3D_busy())||VX2D_busy()||fault||!ring_map()){cs_leave();r->result=2;return 2;} /* v10.476: the retire's cause */
 if(q->op)(void)fx_retire_stop(1); /* v10.391: a parked ring stopped before a test drives the fetcher */
 r->bpp=hda->bpp;r->pitch=hda->pitch;r->fb_bytes=vx_status.fb_bytes;
 f=VX_irq_save();old=inp(0x3C4);unlock=sr(0x10);sw(0x10,(unsigned char)(unlock|1));
 for(k=0;k<16;k++)r->sr[k]=sr(ring_sr_idx[k]);
 r->reg_draws=vx3dn_reg_draws;
 if(q->op){a=sr(0x1A);b=sr(0x1E);sw(0x1A,(unsigned char)(a|8));sw(0x1E,(unsigned char)(b|1));opened=1;}
 outp(0x3C4,old);VX_irq_restore(f);
 r->table_offset=((r->sr[3]&1)<<28)|(r->sr[2]<<20)|(r->sr[1]<<12);
 r->table_readable=r->table_offset+0x10000UL<=vx_status.fb_bytes;
 for(k=0;k<8;k++)r->table_first[k]=ring_table_entry(r,k);
 for(k=0;k<128;k++)r->mmio[k]=read_mmio(0,0x400+k*4);
 lin_end=vx_status.fb_linear+vx_status.fb_bytes;
 if(q->buf_linear>=vx_status.fb_linear&&q->buf_linear<lin_end){r->buf_offset=q->buf_linear-vx_status.fb_linear;r->table_buf[0]=ring_table_entry(r,r->buf_offset>>12);
  r->table_buf[1]=ring_table_entry(r,(r->buf_offset+(q->buf_bytes?q->buf_bytes-1:0))>>12);}
 if(q->tgt_linear>=vx_status.fb_linear&&q->tgt_linear<lin_end){r->tgt_offset=q->tgt_linear-vx_status.fb_linear;r->table_tgt=ring_table_entry(r,r->tgt_offset>>12);}
 if(!q->op)goto out;
 /* checks: both areas inside video memory, the buffer 256-aligned with room, the target inside its surface, not the primary */
 {uint32_t bpx=r->bpp/8,start=(r->buf_offset+255)&~255UL;
  /* v10.288 (review): the ends by subtraction, the pitch bounded (pitch x height and linear + bytes wrapped at 32 bits and passed: the
   * CPU then filled outside video memory) */
  if(!r->buf_offset||!r->tgt_offset||q->buf_bytes<8192||q->buf_bytes>lin_end-q->buf_linear||!q->width||!q->height||q->width>256||q->height>256||
     !q->tgt_pitch||q->tgt_pitch>0x10000UL||q->tgt_pitch<q->width*bpx||r->tgt_offset>vx_status.fb_bytes||q->tgt_pitch*q->height*(q->op>=5?((q->flags&0x1000000)||q->op>=9?3:2):1)>vx_status.fb_bytes-r->tgt_offset||
     (r->tgt_offset&15)||(q->tgt_pitch&7)||(r->bpp!=16&&r->bpp!=32)){r->result=1;goto out;}
  r->words=0;ring_cpu=(volatile uint32_t*)(vx_status.fb_linear+start);r->buf_offset=start;
  r->gpu_start=start;r->gpu_end=r->buf_offset+((q->buf_bytes-(start-(q->buf_linear-vx_status.fb_linear)))&~255UL);
  /* target: every pixel set to the inverted colour first */
  for(i=0;i<q->height*(q->op>=5?((q->flags&0x1000000)||q->op>=9?3:2):1);i++)for(k=0;k<q->width;k++){uint32_t at=vx_status.fb_linear+r->tgt_offset+i*q->tgt_pitch+k*bpx; /* v1.5: op 5's two bands */
   if(bpx==4)*(volatile uint32_t*)at=~q->color;else *(volatile uint16_t*)at=(uint16_t)~q->color;}}
 {static const uint32_t r2d[13]={0x04,0x08,0x14,0x1C,0x30,0x48,0x58,0x5C,0x20,0x4C,0x0C,0x10,0x40};for(k=0;k<13;k++)ring_2d[k]=read_mmio(0,r2d[k]);}
 ring_wordp=0;ring_count=0;publish();
 write_mmio(0,0x41C,0x00100000UL);
 if(q->op==4) { /* the reference: the same triangle through the register path */
  static vx3dn_packet tp;vx3dn_info ii;vx3dn_io io;uint32_t last=0,rc;
  VX3DN_info(&ii);ring_triangle(&tp,r,q,&ii);rc=vx3dn_validate(&tp,&ii);
  if(rc){r->capture_error=0x100|rc;r->result=1;goto check;}
  io.context=0;io.read=read_mmio;io.write=write_mmio;io.read_fb=read_fb;io.write_fb=write_fb;io.milliseconds=ticks;publish();
  vx3dn_reg_draws++;rc=vx3dn_execute(&tp,&io,&last);r->idle=rc==0;if(rc){r->capture_error=0x200|rc;r->result=3;r->reset_tried=1;r->reset_ok=engine_reset();if(!r->reset_ok){fault=1;VX2D_device_lost();}}
  goto check;
 }
 if(q->op==3) { /* the triangle's register writes captured into the buffer, fetched one-shot */
  static vx3dn_packet tp;vx3dn_info ii;vx3dn_io io;uint32_t last=0,rc;
  VX3DN_info(&ii);ring_triangle(&tp,r,q,&ii);rc=vx3dn_validate(&tp,&ii);
  if(rc){r->capture_error=0x100|rc;r->result=1;goto check;}
  cap_hdr=0;cap_err=0;cap_blocks=0;cap_skip=0;cap_form=0;cap_flags=q->flags;vx3dn_wform=0;vx3dn_cgroups=0;cap_max=(r->gpu_end-r->gpu_start)/4-16;
  io.context=0;io.read=cap_read;io.write=cap_write;io.read_fb=read_fb;io.write_fb=write_fb;io.milliseconds=ticks;
  rc=vx3dn_execute(&tp,&io,&last);cap_close();r->capture_error=cap_err|(rc?0x200|rc:0);r->capture_blocks=cap_blocks;
  if(r->capture_error){r->result=1;goto check;}
 } else if(q->op==1) { /* one-shot (s3ginv 0xBC770): packet, pad to 32 bytes, pause = end - 4, 65 | 0x200 */
  ring_fill_packet(r,q);}
 if(q->op==1||q->op==3) {
  while(ring_wordp&3)ring_put(0xCC000000UL);
  ring_put(0xFE020420UL);ring_put(0x41C);ring_put(0x00300000UL);ring_put(0xCC000000UL);
  while((ring_wordp*4)&0x1F){ring_put(0xCCCCCCC0UL);ring_put(0xDDD00000UL);}
  r->gpu_pause=ring_gpu(r)-4;publish();
  t0=Get_System_Time();while((read_mmio(0,0x400)&0x101FF3UL)&&(uint32_t)(Get_System_Time()-t0)<50); /* v1.3: fully idle (op 3 flag 0x80 writes registers first) */
  if(!(q->flags&0x200))write_mmio(0,0x41C,0x00100000UL); /* v1.4: the header again (3D register writes may have changed the latch) */
  ring_pre(r,0,0x60000000UL|(r->gpu_start&0xFFFFFFUL)|((q->flags&2)?1UL:0));ring_pre(r,1,0x61000000UL|(r->gpu_start>>24));
  ring_pre(r,2,0x62000000UL|(r->gpu_end&0xFFFFFFUL));ring_pre(r,3,0x63000000UL|(r->gpu_end>>24));
  ring_pre(r,4,0x64000000UL|(r->gpu_pause&0xFFFFFFUL));ring_pre(r,5,0x65000000UL|(r->gpu_pause>>24)|0x200UL);ring_pre(r,6,0x680A0001UL);
 } else { /* ring (s3ginv 0xBB370 restart path, then one kick 0xBC5xx) */
  ring_pad(0xFF,0xCCC00002UL,0xDDD00002UL);r->gpu_pause=ring_gpu(r)-0x10;publish();
  if(!(q->flags&0x200))write_mmio(0,0x41C,0x00100000UL);
  ring_pre(r,0,0x60000000UL|(r->gpu_start&0xFFFFFFUL)|((q->flags&1)?1UL:0));ring_pre(r,1,0x61000000UL|(r->gpu_start>>24));
  ring_pre(r,2,0x62000000UL|(r->gpu_end&0xFFFFFFUL));ring_pre(r,3,0x63000000UL|(r->gpu_end>>24));
  ring_pre(r,4,0x64000000UL|(r->gpu_pause&0xFFFFFFUL));ring_pre(r,5,0x65000000UL|(r->gpu_pause>>24));
  ring_pre(r,6,0x66FFFFF0UL);ring_pre(r,7,0x670000FFUL);ring_pre(r,8,0x680A0001UL);
  /* the kick: the packet after the start block, the kick padding to 256 bytes, the new pause */
  if(!(q->flags&4)){
   ring_fill_packet(r,q);ring_pad(0xFF,0xCCC00004UL,0);r->gpu_pause=ring_gpu(r)-0x10;publish();
   write_mmio(0,0x41C,0x00100000UL);ring_pre(r,9,0x64000000UL|(r->gpu_pause&0xFFFFFFUL));write_mmio(0,0x420,0x65000000UL|(r->gpu_pause>>24));
   if(q->flags&8)write_mmio(0,0x420,0x680A0001UL);}
 }
 r->words=ring_count;for(k=0;k<VX_RING_STREAM&&k<ring_wordp;k++)r->stream[k]=ring_cpu[k]; /* v1.3: the buffer as written */
 /* wait: up to 200 ms for 0x400 & 101FF3 = 0 eight reads in a row; samples at about 0, 1, 2, 5, 10, 20, 50, 100 ms */
 {static const uint32_t at_ms[VX_RING_SAMPLES]={0,1,2,5,10,20,50,100};
  t0=Get_System_Time();n=0;
  for(;;) {
   uint32_t ms=(uint32_t)(Get_System_Time()-t0);
   st=read_mmio(0,0x400);r->polls++;
   if(n<VX_RING_SAMPLES&&ms>=at_ms[n]){r->sample_ms[n]=ms;r->sample_400[n]=st;r->sample_43c[n]=read_mmio(0,0x43C);r->sample_444[n]=read_mmio(0,0x444);n++;r->samples=n;}
   if(!(st&0x101FF3UL)){if(++stable>=8){r->idle=1;break;}}else stable=0;
   if(ms>=200||st==0xFFFFFFFFUL)break;
  }
  r->elapsed_ms=(uint32_t)(Get_System_Time()-t0);}
 /* stop the fetcher; reset the engine if it did not finish */
 write_mmio(0,0x41C,0x00100000UL);write_mmio(0,0x420,0x680A0000UL);
 if(!r->idle){r->reset_tried=1;r->reset_ok=engine_reset();r->result=3;if(!r->reset_ok){fault=1;VX2D_device_lost();}}
 r->after[0]=read_mmio(0,0x400);r->after[1]=read_mmio(0,0x43C);r->after[2]=read_mmio(0,0x440);r->after[3]=read_mmio(0,0x444);r->after[4]=read_mmio(0,0x45C);r->after[5]=read_mmio(0,0x494);
check:
 vx3dn_forget();
 if(r->idle||r->reset_ok){static const uint32_t r2d[13]={0x04,0x08,0x14,0x1C,0x30,0x48,0x58,0x5C,0x20,0x4C,0x0C,0x10,0x40};for(k=0;k<13;k++)write_mmio(0,r2d[k],ring_2d[k]);} /* the 2D registers the fill used, as they were */
 {uint32_t bpx=r->bpp/8,mask=r->bpp==16?0xFFFFUL:0xFFFFFFFFUL;
  for(i=0;i<q->height;i++)for(k=0;k<q->width;k++){uint32_t at=vx_status.fb_linear+r->tgt_offset+i*q->tgt_pitch+k*bpx,v=bpx==4?*(volatile uint32_t*)at:*(volatile uint16_t*)at;
   if(!i&&!k)r->first_pixel=v;if((v&mask)==(q->color&mask))r->pixels_set++;if((v&mask)!=((~q->color)&mask))r->pixels_changed++;r->checksum+=(v&mask)*(i*q->width+k+1);}}
out:
 if(opened&&!fault){f=VX_irq_save();old=inp(0x3C4);sw(0x1E,b);sw(0x1A,a);outp(0x3C4,old);VX_irq_restore(f);}
 f=VX_irq_save();old=inp(0x3C4);sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);
 if(q&&q->op)fx_pend=0xFFFFFFFFUL; /* v10.382: the fetcher last ran this test's buffer */
 cs_leave();return r->result;
}
/* v10.417: XP's flip in the ring (s3ginv 0xB6B50, the 1106:5122 path of 0xB7990: the engine's own flip). After the frame's draws the ring
 * gets: a 2D block FE000000 1 0 CC000000 with 2D register 6C = 1 (the engine waits for IGA1's vertical blank; 2 is IGA2's), then
 * FE020420 0000041C 00110000 CC000000 and the command regulator's 10 = 10000000 | address & FFFFF8, 11 = 11000000 | address >> 24,
 * 12 = 12000008, CC000099 (XP's words); CR32 bit 0 set once before (XP 0xB7079). Everything after it in the ring runs once the new
 * address is in, so the next frame's draws need no wait for the old front, and the CPU does not wait for the blank or for the engine
 * at the flip: it waits only when it touches the old front itself (the flip's fence) or flips again before this one ran.
 * ACCEL_FLIP_RING (default 1); 0 = the CPU writes the CRTC as before. vx3dn_fr_stat: [0] ring flips, [1] declined, [2] CPU waits for
 * a ring flip, [3] their time (TSC / 256), [4] waits that ran out (100 ms), [5] the last reason declined, [6] draws on the old front
 * sent without a wait, [7] the longest wait (TSC / 256). */
volatile uint32_t vx3dn_flip_ring=1;uint32_t vx3dn_fr_stat[8];int vx3dn_fr_pend;
uint32_t fr_seq; /* .532: public (the emulator's B1 test) */static int fr_dead,fr_lost;
/* .532 (review of .530, B1): a ring flip whose fence never came before the ring stopped (a hang's retire, a reset, a fresh start) did not
 * set the scanout address: VX3DN_ring_flip_done answered "done" for a stopped ring, so the flip code let writers into the old front while
 * the CRTC still showed it, and nothing wrote the new address. fr_gone (the ring not running, or about to start again) decides from the
 * fence itself; a flip it finds unrun is lost: VX3DN_ring_flip_lost tells the flip code once, which writes the address by the CPU and keeps
 * the old front protected until that is taken at a blank (vx855_flip.c fl_ring_lost). */
static void fr_gone(void) {if(vx3dn_fr_pend&&((fx_fence_read()-fr_seq)&0x8000UL))fr_lost=1;vx3dn_fr_pend=0;}
int VX3DN_ring_flip_lost(void) {int r=fr_lost;fr_lost=0;return r;}
static uint32_t xr_kick(int async,uint32_t *last);
int VX3DN_ring_flip_ok(void) {return vx3dn_flip_ring&&!fr_dead&&!fault&&xr_state==2&&fx_rfenced&&!fx_ring_dead&&vx3dn_xr&&vx3dn_fetch&&vx3dn_cmd_off;}
int VX3DN_ring_flip(uint32_t off) { /* 1: queued in the ring (the caller has CR32 bit 0 set); 0: not (the CPU flips) */
 unsigned long f;unsigned char old,unlock,a,b;uint32_t last,why=0;int ok=0;
 cs_enter();
 if(!VX3DN_ring_flip_ok())why=1;
 else if((off&7)||VX3D_lab_busy()||VX2D_busy())why=2;
 else{f=VX_irq_save();old=inp(0x3C4);unlock=sr(0x10);sw(0x10,(unsigned char)(unlock|1));a=sr(0x1A);b=sr(0x1E);sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(f);
  if(!(a&8)||!(b&1))why=3; /* the engine's clocks are not left on: no asynchronous group */
  else if(!xr_open())why=4;}
 if(!why){
  ring_put(0xFE000000UL);ring_put(1);ring_put(0);ring_put(0xCC000000UL);ring_put(0x6C);ring_put(1);
  while(ring_wordp&3)ring_put(0xCC000000UL);
  ring_put(0xFE020420UL);ring_put(0x41C);ring_put(0x00110000UL);ring_put(0xCC000000UL);
  ring_put(0x10000000UL|(off&0xFFFFF8UL));ring_put(0x11000000UL|(off>>24));ring_put(0x12000008UL);ring_put(0xCC000099UL);
  if(xr_kick(1,&last)==VX3DN_OK){fr_seq=fx_seq;vx3dn_fr_pend=1;ok=1;vx3dn_fr_stat[0]++;VX3DN_ev(20,off>>12);}else why=5;
 }
 if(!ok){vx3dn_fr_stat[1]++;vx3dn_fr_stat[5]=why;}
 cs_leave();return ok;
}
int VX3DN_ring_flip_done(void) { /* the last ring flip has run (its fence came), or the ring is gone */
 if(!vx3dn_fr_pend)return 1;
 if(xr_state!=2||!fx_rfenced){fr_gone();return 1;} /* .532 (B1): the ring stopped: run or lost, by the fence (VX3DN_ring_flip_lost) */
 if(((fx_fence_read()-fr_seq)&0x8000UL)==0){vx3dn_fr_pend=0;return 1;}
 return 0;
}
void VX3DN_ring_flip_wait(void) {
 uint32_t t=VX_tsc(),t0=Get_System_Time(),n,d;
 int w;
 if(VX3DN_ring_flip_done())return;
 vx3dn_fr_stat[2]++;w=VX_smp_wait_begin(); /* v10.437: a wait for the sampler; v10.439: this thread's entry */
 for(n=0;!VX3DN_ring_flip_done();n++)
  if(!(n&63)&&(uint32_t)(Get_System_Time()-t0)>=100UL){vx3dn_fr_stat[4]++;fr_dead=1;fr_lost=1;cs_enter();(void)fx_retire(1);cs_leave();vx3dn_fr_pend=0;break;} /* .532 (B1): lost (fr_lost) */ /* a flip that never ran: the ring's hang handling, and no more ring flips this session */
 VX_smp_wait_end(w);
 d=(VX_tsc()-t)>>8;vx3dn_fr_stat[3]+=d;if(d>vx3dn_fr_stat[7])vx3dn_fr_stat[7]=d;
}
/* v10.418 (board, GTA3 v10.417: 1394 flips in the ring, no old-front waits; but each frame's depth clear, a 2D-engine fill, retired the
 * ring first, waiting for the flip's blank: 379 clears of 14.5 ms, retires 33.7 microseconds a packet): ACCEL_3D_RINGCLEAR (default 1):
 * with XP's ring running, a clear of a whole surface from its corner (depth and/or colour, no stencil, no masked word) is the same 2D solid
 * fill written into the ring (the fill of VXRING's test and of the ring's fence: 2D registers 04 08 14 1C 30 48 58 5C 20 4C 0C 10 00), so it
 * runs in order after the draws and the flip before it, and nothing waits. The depth first, the colour last, as the 2D path. Anything else
 * (stencil, a partial word, a rectangle not at 0,0) goes the usual way. vx3dn_rc_stat: [0] clears in the ring, [1] declined. */
volatile uint32_t vx3dn_ringclear=1;uint32_t vx3dn_rc_stat[2];
static int rc_prep(const vx3dn_surface *s,uint32_t c,uint32_t right,uint32_t bottom,uint32_t bpp,uint32_t *o) {
 uint32_t two=s->format==VX3DN_RGB565||s->format==VX3DN_D16,w=right;
 if(two&&bpp==32){if(w&1)return 0;c=(c&0xFFFFUL)|(c<<16);w>>=1;}
 else if(!two&&bpp==16){if((c>>16)!=(c&0xFFFFUL))return 0;c&=0xFFFFUL;w<<=1;}
 else if((two?16UL:32UL)!=bpp)return 0;
 if(!w||!bottom||w>4096||bottom>4096||(s->offset&7)||(s->pitch&7))return 0;
 o[0]=s->offset;o[1]=s->pitch;o[2]=w;o[3]=bottom;o[4]=bpp==16?(c&0xFFFFUL):c;return 1;
}
static void rc_put(const uint32_t *o,uint32_t bpp) {
 static const uint32_t reg[13]={0x04,0x08,0x14,0x1C,0x30,0x48,0x58,0x5C,0x20,0x4C,0x0C,0x10,0x00};uint32_t val[13],k;
 val[0]=bpp==16?0x100UL:0x300UL;val[1]=(o[1]>>3)<<16;val[2]=o[0]>>3;val[3]=0;val[4]=0;val[5]=0;val[6]=o[4];val[7]=0;val[8]=0;
 val[9]=o[4];val[10]=((o[3]-1)<<16)|(o[2]-1);val[11]=0;val[12]=0xF0002001UL;
 while(ring_wordp&3)ring_put(0xCC000000UL);
 ring_put(0xFE000000UL);ring_put(13);ring_put(0);ring_put(0xCC000000UL);
 for(k=0;k<13;k++){ring_put(reg[k]);ring_put(val[k]);}
}
/* v10.453 (board, GTA3 v10.451 with Trails: about one back-buffer-to-texture copy a frame (Direct3D 8's CopyRects, 640x480), each through
 * the 2D engine after a retire of the whole ring: 4,130 retires "for a CPU access" in 4,227 frames, 2.1 ms each, the Blt 2.3 ms a frame):
 * ACCEL_D3D_RINGBLT (default 1; 0 = as before): with XP's ring running, such a copy (VX2D_RT: SRCCOPY, same format, the screen's depth,
 * distinct surfaces, clipped to its rectangles) is the same 2D screen-to-screen copy written into the ring as a group of its own (2D
 * registers 04 08 14 1C 30 48 58 5C 20 18 0C 10, then the command CC000001; the ring's fence after it), so it runs in order after the
 * draws that made the back buffer and before the draws that sample the texture, and nothing waits. A later CPU access retires the ring as
 * before (the fence). The state cache starts over (the texture's cached texels). Not taken: the ring off or dead, another ROP or format, a
 * key, overlapping surfaces, a surface the software cursor may be on, the 3D lab busy: the usual 2D path.
 * vx3dn_rb_stat: [0] copies in the ring, [1] not taken, [2] render-target copies by the 2D engine after the wait for the 3D work, [3] those
 * the 2D path declined (the HAL copies them on the CPU), [4] TSC / 256 of that wait (vx855_2d.c), [5] rectangles empty after the clip,
 * [6] TSC / 256 of the ring copies themselves. TEXDBG BC B5 / B6 / B7: off, on, read. */
volatile uint32_t vx3dn_ringblt=1;uint32_t vx3dn_rb_stat[8];
int VX3DN_ring_copy(const vx2d_request *r) { /* 1: done in the ring; 0: not taken (the usual path follows); -1: the device lost */
 uint32_t bpp=hda?hda->bpp:0,last,dx=r->dx,dy=r->dy,sx=r->sx,sy=r->sy,w=r->width,h=r->height,t=PT(),k;unsigned long fl;unsigned char old,unlock,a,b;
 static const uint32_t reg[13]={0x04,0x08,0x14,0x1C,0x30,0x48,0x58,0x5C,0x20,0x18,0x0C,0x10,0x00};uint32_t val[13];
 if(!vx3dn_ringblt||fault||running||xr_state!=2||!fx_rfenced||fx_ring_dead||!vx3dn_xr||!vx3dn_fetch||!vx3dn_cmd_off||(bpp!=16&&bpp!=32)||r->bpp!=bpp)goto no;
 if(r->op!=VX2D_COPY||r->rop!=0xCC||(r->flags&~(VX2D_DDRAW|VX2D_CLIP|VX2D_RT))||r->src.offset==r->dst.offset)goto no;
 if((r->src.offset&7)||(r->dst.offset&7)||(r->src.pitch&7)||(r->dst.pitch&7))goto no;
 if(r->flags&VX2D_CLIP){uint32_t l=r->left>dx?r->left:dx,tp=r->top>dy?r->top:dy,rt=r->right<dx+w?r->right:dx+w,bt=r->bottom<dy+h?r->bottom:dy+h;
  if(l>=rt||tp>=bt){vx3dn_rb_stat[5]++;return 1;} /* nothing to copy */
  sx+=l-dx;sy+=tp-dy;dx=l;dy=tp;w=rt-l;h=bt-tp;}
 if(!w||!h||w>4096||h>4096||dx>4095||dy>4095||sx>4095||sy>4095)goto no;
 if(VESA_draw_needs_exclusion(r->src.offset,r->src.pitch*r->src.height)||VESA_draw_needs_exclusion(r->dst.offset,r->dst.pitch*r->dst.height))goto no;
 cs_enter();
 if(VX3D_lab_busy()){cs_leave();goto no;}
 fl=VX_irq_save();old=inp(0x3C4);unlock=sr(0x10);sw(0x10,(unsigned char)(unlock|1));a=sr(0x1A);b=sr(0x1E);sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(fl);
 if(!(a&8)||!(b&1)||!xr_open()){cs_leave();goto no;}
 val[0]=bpp==16?0x100UL:0x300UL;val[1]=((r->dst.pitch>>3)<<16)|(r->src.pitch>>3);val[2]=r->dst.offset>>3;val[3]=r->src.offset>>3;
 val[4]=0;val[5]=0;val[6]=0;val[7]=0;val[8]=0;val[9]=(sy<<16)|sx;val[10]=((h-1)<<16)|(w-1);val[11]=(dy<<16)|dx;val[12]=0xCC000001UL;
 while(ring_wordp&3)ring_put(0xCC000000UL);
 ring_put(0xFE000000UL);ring_put(13);ring_put(0);ring_put(0xCC000000UL);
 for(k=0;k<13;k++){ring_put(reg[k]);ring_put(val[k]);}
 if(xr_kick(1,&last)!=VX3DN_OK){last_result=VX3DN_LOST;cs_leave();return -1;}
 vx3dn_rb_stat[0]++;vx3dn_rb_stat[6]+=(PT()-t+128)>>8;vx3dn_forget();VX3DN_ev(22,w|(h<<16));
 cs_leave();return 1;
 no:vx3dn_rb_stat[1]++;return 0;
}
static uint32_t ring_clear(const vx3dn_packet *p) { /* VX3DN_OK: done in the ring; 0xFFFFFFFF: not taken (the usual path follows) */
 vx3dn_info i;vx3dn_packet *q=&request_copy;uint32_t f,bpp=hda?hda->bpp:0,oz[5],oc[5],hz=0,hc=0,cv,cm,last;unsigned long fl;unsigned char old,unlock,a,b;
 if(!vx3dn_ringclear||!vx3dn_clear2d||fault||xr_state!=2||!fx_rfenced||fx_ring_dead||!vx3dn_xr||!vx3dn_fetch||!vx3dn_cmd_off||(bpp!=16&&bpp!=32))return 0xFFFFFFFFUL;
 f=VX3DN_CLEAR_WHAT(p->clear_flags);
 if(!f||(f&~(VX3DN_CLEAR_TARGET|VX3DN_CLEAR_Z)))return 0xFFFFFFFFUL;
 copy_packet(q,p);VX3DN_info(&i);
 if(vx3dn_validate(q,&i)||q->clip.left!=0||q->clip.top!=0||q->clip.right<=0||q->clip.bottom<=0)goto no;
 if(f&VX3DN_CLEAR_Z){vx3dn_depth_word(q,&cv,&cm);if(!(cm==0xFFFFFFFFUL||(q->depth.format==VX3DN_D16&&cm==0xFFFFUL))||!rc_prep(&q->depth,cv,(uint32_t)q->clip.right,(uint32_t)q->clip.bottom,bpp,oz))goto no;hz=1;}
 if(f&VX3DN_CLEAR_TARGET){cv=q->target.format==VX3DN_RGB565?rgb565(q->clear_color):q->clear_color;if(!rc_prep(&q->target,cv,(uint32_t)q->clip.right,(uint32_t)q->clip.bottom,bpp,oc))goto no;hc=1;}
 if(VX3D_lab_busy()||VX2D_busy())goto no;
 fl=VX_irq_save();old=inp(0x3C4);unlock=sr(0x10);sw(0x10,(unsigned char)(unlock|1));a=sr(0x1A);b=sr(0x1E);sw(0x10,unlock);outp(0x3C4,old);VX_irq_restore(fl);
 if(!(a&8)||!(b&1)||!xr_open())goto no;
 if(hz)rc_put(oz,bpp);
 if(hc)rc_put(oc,bpp);
 if(xr_kick(1,&last)!=VX3DN_OK){last_result=VX3DN_LOST;return VX3DN_LOST;}
 vx3dn_rc_stat[0]++;vx3dn_clear2d_done++;submitted++;clears++;last_result=VX3DN_OK;vx3dn_forget();VX3DN_ev(21,f);
 return VX3DN_OK;
 no:vx3dn_rc_stat[1]++;return 0xFFFFFFFFUL;
}

/* v10.434: the sampling profiler (vx_smp.h). Asynchronous time-outs of 1 ms re-armed from their own callback while it is on; the
 * render thread is the last thread that sent draw packets (VX_smp_render, set by the submit IOCTLs); vx_smp_io_thread is the thread
 * inside Device_IO_Control (v10.439: each thread inside has an entry, smp_tt, with its engine waits: fetch_idle, the flip's waits). A frame's samples
 * wait in smp_fc / smp_fe / smp_fpg until its flip (VX_smp_frame) adds them to its class. The starting handle's close stops it. */
#include "vx_smp.h"
static vx_smp smp;static volatile uint32_t smp_on,smp_owner;
volatile uint32_t vx_smp_render; /* the last thread that sent draw packets */
/* v10.439: the threads inside Device_IO_Control: depth (calls inside), wait (engine waits inside); an entry is freed when its thread leaves
 * its last call. smp_vxd_cyc: this frame's render-thread time inside (TSC / 256, outermost calls), added to its class at the flip. */
static struct {uint32_t thr,depth,wait;} smp_tt[16];static uint32_t smp_vxd_cyc,smp_tt_full;
static int smp_tslot(uint32_t thr,int add) {int k,f=-1;for(k=0;k<16;k++){if(smp_tt[k].thr==thr)return k;if(f<0&&!smp_tt[k].thr)f=k;}
 if(!add||f<0){if(add)smp_tt_full++;return -1;}smp_tt[f].thr=thr;smp_tt[f].depth=0;smp_tt[f].wait=0;return f;}
void VX_smp_enter(uint32_t *slot,uint32_t *t0) {uint32_t thr=(uint32_t)Get_Cur_Thread_Handle();unsigned long f=VX_irq_save();int k=smp_tslot(thr,1);if(k>=0)smp_tt[k].depth++;VX_irq_restore(f);*slot=k<0?0xFFFFFFFFUL:(uint32_t)k;*t0=VX_tsc();}
static uint32_t smp_last_code,smp_blk,smp_blk_v86,smp_blk_code;
void VX_smp_leave(uint32_t slot,uint32_t t0,uint32_t code) {uint32_t d=VX_tsc()-t0;unsigned long f=VX_irq_save();
 if(slot<16&&smp_tt[slot].thr){if(smp_tt[slot].depth)smp_tt[slot].depth--;
  if(!smp_tt[slot].depth){if(smp_tt[slot].thr==vx_smp_render){smp_vxd_cyc+=(d+128)>>8;smp_last_code=code;}smp_tt[slot].thr=0;smp_tt[slot].wait=0;}}
 VX_irq_restore(f);}
int VX_smp_wait_begin(void) {int k;unsigned long f;if(!smp_on)return -1;f=VX_irq_save();k=smp_tslot((uint32_t)Get_Cur_Thread_Handle(),0);if(k>=0)smp_tt[k].wait++;VX_irq_restore(f);return k;}
void VX_smp_wait_end(int k) {unsigned long f;if(k<0)return;f=VX_irq_save();if(smp_tt[k].wait)smp_tt[k].wait--;VX_irq_restore(f);}
void VX_smp_set_render(void) {vx_smp_render=(uint32_t)Get_Cur_Thread_Handle();} /* the submit IOCTLs */
static uint32_t smp_fc[VX_SMP_CATS],smp_fe[VX_SMP_CATS],smp_fw[VX_SMP_CATS],smp_prev,smp_gb[7],smp_lastpk; /* .520: smp_fw the frame's time-weighted
 * samples (TSC / 1024), smp_prev the last tick's TSC, smp_gb the gap buckets' bounds in TSC / 1024 */static uint16_t smp_fpg[2048];static uint32_t smp_fpgn;static uint16_t smp_fln[2048];static uint32_t smp_flnn;static uint16_t smp_fvl[1024];static uint32_t smp_fvln; /* .549: the frame's VxD lines */ /* .534: the frame's 64-byte lines (vx_smp.line) */
uint32_t vx_smp_active(void) {return smp_on;}
static uint32_t smp_slot(unsigned long *keys,uint32_t n,uint32_t key) { /* open addressing; key 0 is never a page here (EIP >> 12 of code) */
 uint32_t h=(key*2654435761UL)&(n-1),k;
 for(k=0;k<16;k++){uint32_t i=(h+k)&(n-1);if(keys[i]==key)return i;if(!keys[i]){keys[i]=key;return i;}}
 return 0xFFFFUL;
}
static uint32_t smp_client(uint32_t *eip,uint32_t *cs,uint32_t *efl) { /* the current thread's ring-3 registers (Client_Reg_Struc: EIP +24h, CS +28h, EFLAGS +2Ch) */
 uint32_t vm=(uint32_t)Get_Cur_VM_Handle(),cp;
 if(vm<0xC0000000UL)return 0;cp=*(volatile uint32_t*)(vm+8);if(cp<0xC0000000UL||cp>0xFFFFF000UL)return 0;
 *eip=*(volatile uint32_t*)(cp+0x24);*cs=*(volatile uint16_t*)(cp+0x28);*efl=*(volatile uint32_t*)(cp+0x2C);return 1;
}
/* .512: what ran while a thread waited in cs_enter (sampled; VXD3DMON, TEXDBG BC D0-D4): [0] samples, [1] a thread in this VxD's calls,
 * [2] V86 code, [3] 16-bit protected-mode code, [4] 32-bit code, [5] no client registers; the 24 commonest places (key: V86 0x40000000 | CS,
 * 16-bit 0x80000000 | CS << 8 | IP >> 8 (.513: the block, to name the code from its module's map), 32-bit EIP >> 12) with their counts */
uint32_t vx_csw[6],vx_csw_key[24],vx_csw_n[24]; /* .513: 24 places; 16-bit ones by CS and 256-byte block of IP */
static void csw_note(uint32_t thr) {
 uint32_t eip=0,cs=0,efl=0,key,k;int t=smp_tslot(thr,0);
 vx_csw[0]++;
 if(t>=0&&smp_tt[t].depth){vx_csw[1]++;return;}
 if(!smp_client(&eip,&cs,&efl)){vx_csw[5]++;return;}
 if(efl&0x20000UL){vx_csw[2]++;key=0x40000000UL|cs;}else if(cs!=smp.flat_cs){vx_csw[3]++;key=0x80000000UL|((cs&0xFFFFUL)<<8)|((eip&0xFFFFUL)>>8);}else{vx_csw[4]++;key=eip>>12;}
 for(k=0;k<24;k++)if(vx_csw_key[k]==key&&vx_csw_n[k]){vx_csw_n[k]++;return;}
 for(k=0;k<24;k++)if(!vx_csw_n[k]){vx_csw_key[k]=key;vx_csw_n[k]=1;return;}
}
/* .521: the RTC's periodic interrupt as the sampler's clock (VXSMPCAL v1.0 on .520: the VMM's time-outs came every 4.5-5 ms on the
 * desktop and the samples did not follow the time the code spent; a hardware interrupt is taken wherever the processor is). IRQ 8
 * virtualized (VPICD) while the sampler runs on it; register A's rate 6 (1024 a second), register B's PIE set; each interrupt reads
 * register C (the RTC's acknowledgement), takes the sample, and ends with the physical EOI; at the stop A and B as they were and the IRQ
 * back to VPICD's default behaviour. Not available (no sampling): IRQ 8 already virtualized, or B's PIE already set. */
#include "vpicd.h"
static uint32_t rtc_h;static unsigned char rtc_a0,rtc_b0;static VPICD_IRQ_Descriptor rtc_vid;static volatile uint32_t smp_clk;
static uint32_t vp_virt(VPICD_IRQ_Descriptor *d) {uint32_t h=0;_asm{push edi
 mov edi,[d]}
 VxDCall(VPICD,Virtualize_IRQ)
 _asm{jc vp_fail
 mov [h],eax
 vp_fail: pop edi}
 return h;}
static void vp_eoi(uint32_t h) {_asm{mov eax,[h]}VxDCall(VPICD,Phys_EOI)}
static void vp_unmask(uint32_t h) {_asm{mov eax,[h]}VxDCall(VPICD,Physically_Unmask)}
static void vp_mask(uint32_t h) {_asm{mov eax,[h]}VxDCall(VPICD,Physically_Mask)}
static void vp_default(uint32_t h) {_asm{mov eax,[h]}VxDCall(VPICD,Force_Default_Behavior)}
static unsigned char cmos_r(unsigned char i) {outp(0x70,i);return (unsigned char)inp(0x71);}
static void cmos_w(unsigned char i,unsigned char v) {outp(0x70,i);outp(0x71,v);}
void __stdcall smp_tick(DWORD late,DWORD ref);
static uint32_t smp_esp(void);
#pragma aux smp_esp = "mov eax,esp" value [eax] modify exact [eax]; /* .549: the sampler's own stack pointer (the interrupted frame is above it) */
void __stdcall rtc_irq(DWORD h) {(void)cmos_r(0x0C);if(smp_on&&smp_clk==1)smp_tick(0,0);vp_eoi(h);}
void __declspec(naked) rtc_hw(void) {_asm {pushad
 push eax
 call rtc_irq
 popad
 clc
 retn}}
static int rtc_start(void) { /* with interrupts off */
 if(!rtc_h){rtc_vid.IRQ_Number=8;rtc_vid.Options=0;rtc_vid.Hw_Int_Proc=(DWORD)rtc_hw;rtc_vid.Virt_Int_Proc=0;rtc_vid.EOI_Proc=0;rtc_vid.Mask_Change_Proc=0;
  rtc_vid.IRET_Proc=0;rtc_vid.IRET_Time_Out=500;rtc_vid.Hw_Int_Ref=0;rtc_h=vp_virt(&rtc_vid);if(!rtc_h)return 0;}
 rtc_a0=cmos_r(0x0A);rtc_b0=cmos_r(0x0B);
 if(rtc_b0&0x40){vp_default(rtc_h);rtc_h=0;return 0;} /* someone's periodic interrupt already */
 cmos_w(0x0A,(unsigned char)((rtc_a0&0xF0)|6));cmos_w(0x0B,(unsigned char)(rtc_b0|0x40));(void)cmos_r(0x0C);vp_unmask(rtc_h);return 1;
}
static void rtc_stop(void) { /* with interrupts off */
 if(!rtc_h)return;
 cmos_w(0x0B,rtc_b0);cmos_w(0x0A,rtc_a0);(void)cmos_r(0x0C);vp_mask(rtc_h);vp_default(rtc_h);rtc_h=0;
}
void __stdcall smp_tick(DWORD late,DWORD ref) {
 uint32_t t0=VX_tsc(),c,thr,eip=0,cs=0,efl=0,eng=0,w=1+late,pg=0xFFFFUL,dt;(void)ref;
 if(!smp_on)return;
 dt=(t0-smp_prev)>>10;smp_prev=t0; /* .520: the time this sample stands for */
 smp.samples++;smp.late_ms+=late;
 if(mapped){uint32_t s=*(volatile uint32_t*)(mapped+0x400);eng=s!=0xFFFFFFFFUL&&(s&0x1FF7UL);}
 thr=(uint32_t)Get_Cur_Thread_Handle();
 if(vx_cs_waiter&&thr!=vx_cs_waiter)csw_note(thr); /* .512 */
 if(vx_smp_render){ /* v10.439: a stretch with the render thread not running, ended by its next sample */
  if(thr!=vx_smp_render){if(!smp_blk)smp_blk_code=smp_last_code;smp_blk+=w;}
  else if(smp_blk){uint32_t b=smp_blk<=1?0:smp_blk<=5?1:smp_blk<=10?2:smp_blk<=15?3:smp_blk<=20?4:smp_blk<=50?5:smp_blk<=200?6:7,k;
   smp.blk_hist[b]++;smp.blk_ms+=smp_blk;smp.blk_v86ms+=smp_blk_v86;
   for(k=0;k<8;k++)if(smp.blk_code[k]==smp_blk_code||!smp.blk_code_n[k]){smp.blk_code[k]=smp_blk_code;smp.blk_code_n[k]++;smp.blk_code_ms[k]+=smp_blk;break;}
   smp_blk=0;smp_blk_v86=0;}}
 if(!vx_smp_render)c=VX_SMP_NONE;
 else if(thr==vx_smp_render){
  {int k=smp_tslot(thr,0);if(k>=0&&smp_tt[k].depth)c=smp_tt[k].wait?VX_SMP_RWAIT:VX_SMP_RVXD;else c=0xFF;}
  if(c==VX_SMP_RVXD&&smp_fvln<1024){uint32_t vm=(uint32_t)Get_Cur_VM_Handle(),cp=vm>=0xC0000000UL?*(volatile uint32_t*)(vm+8):0,sp=smp_esp(),a,e=0; /* .549 */
   if(cp>sp&&cp-sp<0x4000UL){for(a=sp;a+12<=cp;a+=4){const uint32_t x=*(volatile uint32_t*)a,y=*(volatile uint32_t*)(a+4),z=*(volatile uint32_t*)(a+8);
     if(x>=0xC0000000UL&&y==0x28UL&&(z&0xFFC08028UL)==0&&(z&0x202UL)==0x202UL){e=x;break;}}}
   if(!e)smp.vline_miss++;
   else if(e-(smp.vxd_ref-0x60000UL)<0x80000UL)smp_fvl[smp_fvln++]=(uint16_t)smp_slot(smp.vline,VX_SMP_VLINES,e>>6);
   else smp.vline_vmm++;}
  if(c==0xFF){if(!smp_client(&eip,&cs,&efl)||(efl&0x20000UL)||cs!=smp.flat_cs)c=VX_SMP_R16;
  else if(eip-smp.hal_base<smp.hal_size)c=VX_SMP_RHAL;
  else{c=VX_SMP_RCODE;pg=smp_slot(smp.page,VX_SMP_PAGES,eip>>12);}
  if((c==VX_SMP_RHAL||c==VX_SMP_RCODE)&&smp_flnn<2048)smp_fln[smp_flnn++]=(uint16_t)smp_slot(smp.line,VX_SMP_LINES,eip>>6);}} /* .534: FFFF no slot */
 else{
  if(!smp_client(&eip,&cs,&efl)||(efl&0x20000UL)||cs!=smp.flat_cs)c=VX_SMP_O16;
  else{c=VX_SMP_OCODE;if(eip>=0x80000000UL){pg=smp_slot(smp.opage,VX_SMP_OPAGES,eip>>12);if(pg!=0xFFFFUL)pg|=0x8000UL;}else pg=0xFFFEUL;}}
 if(c==VX_SMP_O16&&smp_blk&&cs&&(efl&0x20000UL))smp_blk_v86+=w; /* v10.439 */
 if((c==VX_SMP_R16||c==VX_SMP_O16)&&cs){uint32_t o=c==VX_SMP_O16; /* v10.438: which 16-bit code (or V86) */
  if(efl&0x20000UL)smp.v86n[o]+=w;else{uint32_t k;for(k=0;k<16;k++)if(smp.cs16[k]==cs||!smp.cs16[k]){smp.cs16[k]=cs;smp.cs16n[k][o]+=w;break;}if(k==16)smp.cs16_other+=w;}}
 smp_fc[c]+=w;if(eng)smp_fe[c]+=w;
 { /* .520: time-weighted; the gap before this tick by where it landed; the VxD's phase */
  uint32_t l=c==VX_SMP_RWAIT||c==VX_SMP_RVXD?1:c==VX_SMP_RHAL||c==VX_SMP_RCODE||c==VX_SMP_R16?0:c==VX_SMP_NONE?3:2,b=0;
  while(b<7&&dt>=smp_gb[b])b++;
  smp_fw[c]+=dt;smp.tw_total+=dt;smp.n_all[c]++;smp.tw_all[c]+=dt;smp.gap_n[l][b]++;smp.gap_t[l]+=dt;
  if(l==1){uint32_t ph=vx_smp_ph&15;smp.ph_n[ph]++;smp.ph_t[ph]+=dt;}}
 if((c==VX_SMP_RCODE||c==VX_SMP_OCODE)&&smp_fpgn<2048)smp_fpg[smp_fpgn++]=(uint16_t)(c==VX_SMP_OCODE&&pg==0xFFFFUL?0xFFFDUL:pg); /* FFFF: the render thread's table full; FFFD: the other table full; FFFE: another process's private page */
 smp.tick_cycles+=(VX_tsc()-t0+128)>>8;
}
static uint32_t smp_run_last,smp_run_thr;uint32_t vx_smp_pkbias; /* 0; the emulator's checks add packets here */
static uint32_t smp_lasttri,smp_tri;
void VX_smp_frame(uint32_t iv) { /* a flip: the frame since the last one ended (iv ms; 0: the first) */
 uint32_t k,g,pk,thr,run,rd=0xFFFFFFFFUL;unsigned long f;
 if(!smp_on)return;
 thr=(uint32_t)Get_Cur_Thread_Handle();run=Get_Thread_Exec_Time((void*)thr);
 if(thr==smp_run_thr&&run>=smp_run_last)rd=run-smp_run_last;smp_run_thr=thr;smp_run_last=run;
 f=VX_irq_save();pk=submitted+vx_smp_pkbias-smp_lastpk;smp_lastpk=submitted+vx_smp_pkbias;smp.frames_seen++;
 {extern uint32_t vx3dn_geo[4];uint32_t t=vx3dn_geo[2]-smp_lasttri;smp_lasttri=vx3dn_geo[2];smp_tri=t;} /* v10.441: the frame's triangles */
 g=pk>=smp.pk_threshold?(iv>=75?1:0):2;
 if(iv){for(k=0;k<VX_SMP_CATS;k++){smp.cat[g][k]+=smp_fc[k];smp.eng[g][k]+=smp_fe[k];}
  for(k=0;k<VX_SMP_CATS;k++)smp.tw[g][k]+=smp_fw[k];
  smp.frames[g]++;smp.frame_ms[g]+=iv;smp.tri[g]+=smp_tri;smp.vxd_cyc[g]+=smp_vxd_cyc;if(rd!=0xFFFFFFFFUL){smp.run_ms[g]+=rd;smp.run_n[g]++;}if(g<2)smp.hist[iv<1000?iv:1000]++;
  if(g<2)for(k=0;k<smp_fpgn;k++){uint32_t p=smp_fpg[k];
   if(p==0xFFFFUL)smp.page_other[g]++;else if(p==0xFFFDUL||p==0xFFFEUL)smp.opage_other[g]++;else if(p&0x8000UL)smp.opn[p&0x7FFFUL][g]++;else smp.pn[p][g]++;}
  if(g<2)for(k=0;k<smp_flnn;k++){uint32_t l=smp_fln[k];if(l==0xFFFFUL)smp.line_other++;else smp.ln[l]++;}
  if(g<2)for(k=0;k<smp_fvln;k++){uint32_t l=smp_fvl[k];if(l==0xFFFFUL)smp.vline_other++;else smp.vln[l]++;}}
 for(k=0;k<VX_SMP_CATS;k++){smp_fc[k]=0;smp_fe[k]=0;smp_fw[k]=0;}smp_fpgn=0;smp_flnn=0;smp_fvln=0;smp_vxd_cyc=0;
 VX_irq_restore(f);
}
/* v10.436: the time-out's handle is only taken and cancelled with interrupts off (a tick in between would have fired it: a dead handle,
 * which the VMM may have given to another time-out) */
static void smp_stop(void) {unsigned long f=VX_irq_save();smp_on=0;smp.on=0;if(smp_clk==1)rtc_stop();smp_clk=0;VX_irq_restore(f);smp.stop_ms=Get_System_Time();}
DWORD VX_smp_ioctl(const DWORD *in,DWORD nin,DWORD *out,DWORD nout,DWORD hdev) { /* bytes returned */
 if(nin<4)return 0;
 if(in[0]==1&&nin>=12){uint32_t eip=0,cs=0,efl=0;
  if(smp_on)smp_stop();
  memset(&smp,0,sizeof(smp));smp.version=VX_SMP_VERSION;smp.vxd_ref=(uint32_t)smp_tick;smp_fvln=0; /* .549 */smp.hal_base=in[1];smp.hal_size=in[2];smp.pk_threshold=50;
  if(smp_client(&eip,&cs,&efl))smp.flat_cs=cs; /* the caller is a Win32 program: its CS is the flat code selector */
  {uint32_t k;for(k=0;k<VX_SMP_CATS;k++){smp_fc[k]=0;smp_fe[k]=0;smp_fw[k]=0;}}
  { /* .520: the TSC's rate over 8 ms of the system clock, and the gap buckets' bounds (us) in TSC / 1024 */
   static const uint32_t us[7]={500,900,1100,1500,2500,5000,20000};uint32_t m=Get_System_Time(),a,b,k,khz;
   while(Get_System_Time()==m);a=VX_tsc();m=Get_System_Time();while((uint32_t)(Get_System_Time()-m)<8);b=VX_tsc();
   khz=(b-a)/((uint32_t)(Get_System_Time()-m));if(khz<10000)khz=1000000UL;smp.tsc_khz=khz;
   for(k=0;k<7;k++)smp_gb[k]=us[k]*(khz>>10)/1000UL;} /* no 64-bit arithmetic in the VxD */smp_fpgn=0;smp_flnn=0;smp_fvln=0;smp_lastpk=submitted;{extern uint32_t vx3dn_geo[4];smp_lasttri=vx3dn_geo[2];}smp_run_thr=0;smp_vxd_cyc=0;smp_tt_full=0;smp_blk=0;smp_blk_v86=0;smp_last_code=0;
  smp.start_ms=Get_System_Time();smp_owner=hdev;{unsigned long f=VX_irq_save();smp_prev=VX_tsc();smp_clk=0;
   if(rtc_start()){smp_clk=1;smp.clk=1;smp_on=1;smp.on=1;} /* .521: the RTC; .524: the only clock (the VMM's time-outs, biased in-game, removed;
    * in[3] ignored): with IRQ 8 or the RTC's periodic interrupt taken the sampler does not start */
   VX_irq_restore(f);}} /* v10.436: armed with interrupts off */
 else if(in[0]==0){if(smp_on)smp_stop();}
 else if(in[0]==3&&nin>=12&&nout>=4){ /* .520: VXSMPCAL's spin inside the VxD (vx_smp.h) */
  uint32_t want=in[1]>200000UL?200000UL:in[1],kind=in[2],khz=smp.tsc_khz?smp.tsc_khz:1000000UL,o=vx_smp_ph,t=VX_tsc(),n,k;
  unsigned long f=0;
  n=want*(khz/1000UL);
  VX_smp_set_render();vx_smp_ph=VX_SMP_PH_CAL;
  if(kind==2)f=VX_irq_save();
  if(kind&&xr_use_nt()){ /* the ring's pattern: MOVNTI a staging block over a buffer, then CLFLUSH its lines, as a group's copy and flush */
   uint32_t *d=probe_buf;while(VX_tsc()-t<n){xr_copy(d,xr_stage,1024);xr_mfence();for(k=0;k<4096;k+=64)xr_clflush((unsigned long)d+k);}}
  else while(VX_tsc()-t<n){volatile uint32_t x=0;for(k=0;k<64;k++)x+=k;}
  if(kind==2)VX_irq_restore(f);
  vx_smp_ph=o;out[0]=VX_tsc()-t;return 4;}
 smp.tt_full=smp_tt_full;
 if(nout>=sizeof(smp)){memcpy(out,&smp,sizeof(smp));return sizeof(smp);}
 if(nout>=8){out[0]=smp.version;out[1]=smp.on;return 8;}
 return 0;
}
void VX_smp_close(DWORD hdev) {if(smp_on&&hdev==smp_owner)smp_stop();}
