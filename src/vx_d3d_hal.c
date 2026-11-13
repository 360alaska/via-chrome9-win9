/* Experimental Win98 Direct3D7 raster DDI adapter, M2 on v10.148.
 * Default source build keeps advertisement off. See docs/DDI_REFERENCES.md.
 * Source is independent of VMHAL9x's Mesa/software renderer. MIT, 2026. */
#define COBJMACROS
#define DIRECTDRAW_VERSION 0x0700
#define DIRECT3D_VERSION 0x0700
#include "vx_d3d_hal.h"
#define VXD32
#include "ddhal32/vmdahal32.h"
#include "vx85598.h"
#include "version.h"
#include <string.h>
#include <stddef.h>
#define CONTEXTS 8
#define TABLES 32
#define SURFACE_BLOCKS 32
#define BLOCK_SURFACES 128
/* v10.182: see follow() for the reason codes */
static volatile DWORD lost_why,lost_value,lost_count;
static void note_lost(DWORD why,DWORD value){lost_why=why;lost_value=value;lost_count++;}
typedef struct context_slot {uint32_t handle,owner,local,target_key,depth_key;vx_d3d_context core;uint32_t rt_prev[64],rt_ok,rt_tex;} context_slot; /* v10.456: rt_tex = the bound render target is a texture (ACCEL_D3D_RTTEX) */ /* v10.428: rt_prev = the runtime's state array at the end of the last call */
typedef struct surface_slot {uint32_t handle,native_key;vx3dn_surface desc;} surface_slot;
typedef struct surface_table {uint32_t owner,local,blocks;uint8_t block[SURFACE_BLOCKS];} surface_table;
static VMDAHAL_t *shared_hal;
static context_slot contexts[CONTEXTS];static volatile DWORD hold_pid,hold_tid;static DWORD hold_why[8]; /* .550 (VX_D3D_hold_sync, below) */
/* Shared DLL data must not contain process-private heap pointers. Tables grow
 * exponentially from a shared block pool; exhaustion is reported as OOM.
 * Each DDLocal has its own handle namespace, including sparse handle values. */
static surface_slot surface_pool[SURFACE_BLOCKS][BLOCK_SURFACES];
static surface_table tables[TABLES];
static uint32_t used_blocks;
static uint32_t serial=1;
/* v10.319 (audit H2): the callback lock is its holder's token (0 free). It lives in the DLL's shared data: a process that died holding it (a
 * crash in a callback) left it taken for good, every later callback of every process answering WASSTILLDRAWING. A process's token is new at
 * each DLL attach (Windows 98 gives a restarted program the ID of the one that ended, v10.230: a process ID alone would make the new one
 * wait on the dead one's lock), kept in a table by process ID. A caller that finds the lock held by a token whose process has ended (the
 * token no longer in the table, or its process no longer running) takes it over (compare-and-swap: one taker) and gives back what the dead
 * process held (reclaim_dead; the core's batch and template start over at every DrawPrimitives2 call anyway). The same token (another
 * thread of this process) waits as before. A process that detaches holding it (a thread killed in a callback) lets it go. Token 1: no table
 * room, never taken over. tok_pid, tok_val and callback_owner are not static: the linked checks find them (verification/d3d_callbacks.py). */
#define TOKENS 64
volatile LONG callback_owner,tok_pid[TOKENS],tok_val[TOKENS];
static volatile LONG tok_next=1;
static uint32_t cb_takeovers;
long vx_cas(volatile LONG *p,long nv,long old); /* *p = nv if *p == old; the old *p */
#pragma aux vx_cas = "lock cmpxchg [edx],ecx" parm [edx] [ecx] [eax] value [eax] modify exact [eax];
static int owner_alive(uint32_t pid);
static void reclaim_dead(void);
static LONG tok_new(void) {LONG o,n;do{o=tok_next;n=o+1;if(n<2)n=2;}while(vx_cas(&tok_next,n,o)!=o);return n;}
/* 1: this process ID already had a token (an ended process's, whose ID Windows 98 gave again: it attached and never detached) */
static int attach_token(LONG me) {
 LONG k,o;
 for(k=0;k<TOKENS;k++)if(tok_pid[k]==me){tok_val[k]=tok_new();return 1;} /* an ended process's ID given again: a new token */
 for(k=0;k<TOKENS;k++)if(!tok_pid[k]&&vx_cas(&tok_pid[k],me,0)==0){tok_val[k]=tok_new();return 0;}
 for(k=0;k<TOKENS;k++){o=tok_pid[k];if(o&&!owner_alive((uint32_t)o)&&vx_cas(&tok_pid[k],me,o)==o){tok_val[k]=tok_new();return 0;}}
 return 0;
}
static void reclaim_pid(uint32_t pid);
static int cb_enter(void);
static void cb_leave(void);
void vxr_detach(void); /* .518 (vx_d3d_rec.inc) */
void VX_D3D_attach(int attach) {
 LONG me=(LONG)GetCurrentProcessId(),k;
 if(!attach) {
  vxr_detach(); /* .518 (audit): a recording of this process given up */
  if(vx_d3d_held()&&hold_pid==(DWORD)me){vx_d3d_hold_drop();hold_why[6]++;} /* .550: its held packets (a draw of an ending process) */
  for(k=0;k<TOKENS;k++)if(tok_pid[k]==me){if(tok_val[k]&&vx_cas(&callback_owner,0,tok_val[k])==tok_val[k])cb_takeovers++;tok_val[k]=0;tok_pid[k]=0;}
  return;
 }
 {int stale=attach_token(me);
 /* v10.330 (review #190): a process attaching has no contexts or surface tables yet; any under its process ID are an ended process's whose
  * ID Windows 98 gave again (owner_alive could not tell them apart). They go now, under the callback lock (one try: not held, they stay as
  * before and the DestroyDDLocal / DestroyAll callbacks remain the usual way out) */
 /* v10.470 (board, VXFETCH v2.1: its LoadLibrary of VXDDRAW.DLL inside a Direct3D process deleted that process's live contexts and surface
  * tables, and every draw after it was refused): a process can use the HAL (DirectDraw maps it in the shared arena) without this DLL ever
  * being attached in it, so contexts under its ID are not proof of an ended process. Only when the ID already had a token (an ended
  * process attached under it and never detached) are they reclaimed. */
 if(stale&&cb_enter()){reclaim_pid((uint32_t)me);cb_leave();}}
}
static LONG my_token(void) {LONG me=(LONG)GetCurrentProcessId(),k;for(k=0;k<TOKENS;k++)if(tok_pid[k]==me&&tok_val[k])return tok_val[k];return 1;}
static int token_alive(LONG t) {LONG k;if(t==1)return 1;for(k=0;k<TOKENS;k++)if(tok_val[k]==t)return owner_alive((uint32_t)tok_pid[k]);return 0;}
static void forget_drain(void);
static int cb_enter(void) {
 LONG t=my_token(),o=vx_cas(&callback_owner,t,0);
 if(!o){forget_drain();return 1;}
 if(o!=t&&!token_alive(o)&&vx_cas(&callback_owner,t,o)==o){cb_takeovers++;reclaim_dead();forget_drain();return 1;}
 return 0;
}
static void cb_leave(void) {InterlockedExchange(&callback_owner,0);}
/* v10.466 (review): the runtime's D3DParseUnknownCommand callback is an address in the asking process; the DLL's data is shared by
 * every process, so it is kept per process (16 slots by process id; a slot of a process that ended is taken over) */
typedef HRESULT (WINAPI *parse_fn)(LPVOID,LPVOID *);
#define PU_SLOTS 16
static parse_fn pu_fn[PU_SLOTS];static DWORD pu_pid[PU_SLOTS];
static parse_fn parse_unknown_get(void) {DWORD pid=GetCurrentProcessId();int i;for(i=0;i<PU_SLOTS;i++)if(pu_pid[i]==pid)return pu_fn[i];return 0;}
static int owner_alive(uint32_t pid);
static void parse_unknown_set(parse_fn f) {
 DWORD pid=GetCurrentProcessId();int i,fr=-1;
 for(i=0;i<PU_SLOTS;i++){if(pu_pid[i]==pid){pu_fn[i]=f;return;}if(fr<0&&!pu_pid[i])fr=i;}
 if(fr<0)for(i=0;i<PU_SLOTS;i++)if(!owner_alive(pu_pid[i])){fr=i;break;}
 if(fr<0)fr=(int)(pid%PU_SLOTS); /* every slot's process still runs: the oldest guess is replaced (that process parses no unknown command) */
 pu_fn[fr]=f;pu_pid[fr]=pid;
}
static vx_ddi_global global_caps;
static DDSURFACEDESC tex_formats[13]; /* v10.169, six since v10.172, ten since v10.199 (L8, DXT1, DXT3, DXT5), 13 since v10.208 (bump) */
static volatile DWORD trace_inits,trace_mode,trace_count,trace_ring[32][4];
static volatile DWORD rs_count,rs_ring[64][2],draw_count,draw_shade[8]; /* v10.162: legacy render states as received and the shade mode at each legacy draw */
DWORD WINAPI VX_D3D_QueryStates(DWORD *out,DWORD n) {
 DWORD i,k=0,c=rs_count,d=draw_count;
 if(!out||n<2+8+128)return 0;
 out[k++]=c;out[k++]=d;for(i=0;i<8;i++)out[k++]=draw_shade[i];
 for(i=0;i<64;i++){out[k++]=rs_ring[i][0];out[k++]=rs_ring[i][1];}
 return k;
}
static volatile DWORD trace_reply[8],trace_probe[12]; /* v10.156: the last positive GetDriverInfo reply (GUID Data1 + first 7 DWORDs copied) and a refused RenderPrimitive buffer pointer, dumped */
static void probe_words(volatile DWORD *o,const void *p,int n) {int k;for(k=0;k<n;k++)o[k]=0xDEADDEADUL;if(p&&!IsBadReadPtr(p,n*4))for(k=0;k<n;k++)o[k]=((const DWORD*)p)[k];}
static void probe_buffer(DWORD why,const void *p) {
 const DWORD *w=(const DWORD*)p;
 if(trace_probe[0])return;
 trace_probe[0]=why;trace_probe[1]=(DWORD)p;probe_words(trace_probe+2,p,4);
 if(p&&!IsBadReadPtr(p,8)){probe_words(trace_probe+6,(const void*)w[0],2);probe_words(trace_probe+8,(const void*)w[1],4);}
}
static vx_ddi_callbacks callbacks;
static vx_ddi_callbacks2 callbacks2;
static vx_ddi_callbacks3 callbacks3;
static vx_d3d_diag diagnostic;
static void record(uint32_t callback,HRESULT hr) {
 if(diagnostic.last_hr&&!FAILED(hr))return; /* v10.154: a failure stays visible (it was overwritten by the next successful callback) */
 diagnostic.last_callback=callback;diagnostic.last_hr=(uint32_t)hr;
}
/* v10.185: the last 16 HAL events, read by VXDX7 after an application has gone: every ContextCreate, and every failing CreateSurfaceEx,
 * DrawPrimitives2 and ValidateTextureStageState, with the reason. Kept across HAL rebuilds (mode changes), as the diagnostics now are.
 * Record: callback (1 ContextCreate, 2 DrawPrimitives2, 5 Validate, 6 CreateSurfaceEx), result, reason, three details. */
static volatile DWORD ev_count,ev_ring[16][6];
/* v10.191: every event is also appended to C:\VXD3D.LOG and flushed at once (WinTune locks the board before VXDX7 can read the log).
 * Events are rare (context creation and failures only), so the file write stays out of the per-frame path. */
/* v10.202: only with HKLM\Software\VX85598 ACCEL_D3D_LOG = 1 (read through the VxD at DriverInit); the event ring is always kept */
static DWORD event_log_file;
DWORD vx_d3d_sample_on; /* .511: ACCEL_D3D_SAMPLE (vx_ddraw.c samples the flipping thread) */
static void event_file(DWORD n,DWORD cb,DWORD hr,DWORD why,DWORD a,DWORD b,DWORD c) {
 char line[160];DWORD w=0;int k;HANDLE f;
 if(!event_log_file)return;
 f=CreateFileA("C:\\VXD3D.LOG",GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,0,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
 if(f==INVALID_HANDLE_VALUE)return;
 k=wsprintfA(line,"#%lu t=%lu pid=%08lX cb=%lu hr=%08lX why=%lX %08lX %08lX %08lX\r\n",n,GetTickCount(),GetCurrentProcessId(),cb,hr,why,a,b,c);
 SetFilePointer(f,0,0,FILE_END);if(k>0)WriteFile(f,line,(DWORD)k,&w,0);FlushFileBuffers(f);CloseHandle(f);
}
static void event(DWORD cb,DWORD hr,DWORD why,DWORD a,DWORD b,DWORD c) {volatile DWORD *e=ev_ring[ev_count%16];e[0]=cb;e[1]=hr;e[2]=why;e[3]=a;e[4]=b;e[5]=c;ev_count++;event_file(ev_count,cb,hr,why,a,b,c);}
void VX_D3D_log(DWORD cb,DWORD hr,DWORD why,DWORD a,DWORD b,DWORD c) {event(cb,hr,why,a,b,c);} /* v10.195: the 2D callbacks' entries */
/* v10.420 (diagnostic, VXTCBENCH 1.10: a frame drawn with Z writes off after a batch flushed inside a Lock while another surface was
 * locked): with ACCEL_D3D_LOG the first 3000 lines of C:\VXD3D.LOG (numbered from 100001) also trace each DrawPrimitives2 call (cb 90 at
 * entry: why = ZENABLE | ZWRITEENABLE << 8 | ZFUNC << 16 of the context, then command bytes, flags, call number; cb 92 at exit: hr, the
 * same Z states, packets sent with Z write off, with Z write on, error offset), each Z render-state command (cb 91: why = state, then
 * value, command offset) and each Lock, Unlock and Blt (cb 93 / 94 / 95 from VXDDRAW: hr, why = callback's return, then destination
 * offset, flags or source offset, caps). Ring-only events (VXDX7) are not touched. */
extern void (*vx_d3d_rs_hook)(uint32_t id,uint32_t value,uint32_t at);
/* v10.427 (board: two Direct3D programs before VXTCBENCH in the same Windows session used the 3000 lines up, the DLL's data being shared):
 * each process its own budgets (8 at a time; a ninth takes a slot over) */
static DWORD trp_pid[8],trp_n[8],trp_n2[8];static uint32_t tr_zw[2];
static int trp(void) {DWORD p=GetCurrentProcessId();int k,f=-1;for(k=0;k<8;k++){if(trp_pid[k]==p)return k;if(f<0&&!trp_pid[k])f=k;}
 if(f<0)f=(int)(p&7);trp_pid[f]=p;trp_n[f]=0;trp_n2[f]=0;return f;}
#define tr_n (trp_n[trp()])
void VX_D3D_trace(DWORD cb,DWORD hr,DWORD why,DWORD a,DWORD b,DWORD c) {int k;if(!event_log_file)return;k=trp();if(trp_n[k]>=3000)return;trp_n[k]++;event_file(100000UL+trp_n[k],cb,hr,why,a,b,c);}
/* v10.422 (diagnostic, WinTune GL after the D3D test: bands, a lower score): Blts to the primary (cb 96/97 from VXDDRAW), their own 600
 * lines so a long Direct3D trace does not use them up */
void VX_D3D_trace2(DWORD cb,DWORD hr,DWORD why,DWORD a,DWORD b,DWORD c) {int k;if(!event_log_file)return;k=trp();if(trp_n2[k]>=600)return;trp_n2[k]++;event_file(200000UL+trp_n2[k],cb,hr,why,a,b,c);}
DWORD VX_D3D_trace_on(void) {return event_log_file&&trp_n2[trp()]<600;}
static void tr_rs(uint32_t id,uint32_t value,uint32_t at) {if(id==7||id==14||id==23)VX_D3D_trace(91,0,id,value,at,0);}
static void tr_packets(const vx3dn_packet *p,uint32_t n) {uint32_t k;if(!event_log_file||tr_n>=3000)return;for(k=0;k<n;k++)if(p[k].op==VX3DN_DRAW)tr_zw[p[k].z_write&1]++;}
DWORD WINAPI VX_D3D_QueryEvents(DWORD *out,DWORD n) {
 DWORD c,m,k,j,tries=0;
 /* v10.330 (review #163): copied again while an event was added during the copy (a record half old, half new), at most four times */
 do{c=ev_count;m=c>16?16:c;
  if(!out||n<1+m*6)return 0;
  out[0]=c;for(k=0;k<m;k++)for(j=0;j<6;j++)out[1+k*6+j]=ev_ring[(c-m+k)%16][j];
 }while(ev_count!=c&&++tries<4);
 return 1+m*6;
}
DWORD WINAPI VX_D3D_QueryStats(vx_d3d_diag *p) {
 if(!p||p->cb!=sizeof(*p)||!cb_enter())return 0;
 *p=diagnostic;p->dropped=vx_d3d_dropped;p->drop_why=vx_d3d_drop_why;p->lost_why=lost_why;p->lost_value=lost_value;p->lost_count=lost_count;memcpy(p->drop_state,vx_d3d_drop_state,sizeof(p->drop_state));p->build=DRV_VER_BUILD;p->batched=vx_d3d_batched;p->recovered=vx_d3d_recovered;cb_leave();return 1;
}
/* Save x87 registers/status/control in a STACK-LOCAL buffer, never shared
 * writable DLL data. The callback's implementation may do CPU clipping. */
static void save_fp(void *p);
static void restore_fp(void *p);
#pragma aux save_fp = "fnsave [edi]" parm [edi] modify exact [];
#pragma aux restore_fp = "frstor [edi]" parm [edi] modify exact [];
/* v10.234: the DirectDraw HAL's per-process handle (vx_ddraw.c ioctl: one slot per process, opened again after a failed call) instead
 * of CreateFile, DeviceIoControl and CloseHandle for every packet (VXDISCO: about 32 us a packet outside the VxD) */
int VX_DD_call(DWORD code,const void *in,DWORD n,void *out,DWORD z);
static int call(uint32_t code,const void *in,uint32_t n,void *out,uint32_t z) {return VX_DD_call(code,in,n,out,z);}
static context_slot *find(uint32_t h) {
 uint32_t j,owner=GetCurrentProcessId();
 for(j=0;j<CONTEXTS;j++)if(contexts[j].handle==h&&contexts[j].owner==owner)return &contexts[j];
 return 0;
}
static surface_slot *table_at(surface_table *t,uint32_t i) {
 return &surface_pool[t->block[i/BLOCK_SURFACES]][i%BLOCK_SURFACES];
}
/* .532 (review of .530, B7): it grew only by doubling, so a table of 16 blocks with 1 of the 32 free could not grow, and one past 16 never
 * could, though one block's 128 slots hold any one registration (NODES 96). Now: as many free blocks as there are, up to doubling; one is
 * enough to succeed. Both callers loop on the slots they still need. */
static int table_grow(surface_table *t) {
 uint32_t n=t->blocks?t->blocks:1,j,k=0;uint8_t selected[SURFACE_BLOCKS];
 if(n>SURFACE_BLOCKS-t->blocks)n=SURFACE_BLOCKS-t->blocks;
 for(j=0;j<SURFACE_BLOCKS&&k<n;j++)if(!(used_blocks&(1UL<<j)))selected[k++]=(uint8_t)j;
 if(!k)return 0;n=k;
 for(j=0;j<n;j++) {
  k=selected[j];used_blocks|=1UL<<k;t->block[t->blocks++]=(uint8_t)k;
  memset(surface_pool[k],0,sizeof(surface_pool[k]));
 }
 return 1;
}
static void table_release(surface_table *t) {
 uint32_t j;for(j=0;j<t->blocks;j++) {
  memset(surface_pool[t->block[j]],0,sizeof(surface_pool[0]));used_blocks&=~(1UL<<t->block[j]);
 }
 memset(t,0,sizeof(*t));
}
/* v10.183: the surface pool, the tables and the context slots live in the DLL's shared data. A process that ends without DirectDraw's
 * clean-up (a crash; DDHELP does not always call DestroyDDLocal/DestroyAll for it) kept its blocks for good, so after a few such exits
 * CreateSurfaceEx ran out (DDERR_OUTOFMEMORY, "Execute failed in SetRenderState" in a DirectX 5 sample). When something runs short,
 * everything owned by a process that no longer runs is given back. */
static int owner_alive(uint32_t pid) {
 HANDLE h;DWORD code=0;
 if(!pid||pid==GetCurrentProcessId())return 1;
 h=OpenProcess(PROCESS_QUERY_INFORMATION,FALSE,pid);if(!h)return 0;
 if(!GetExitCodeProcess(h,&code))code=STILL_ACTIVE;CloseHandle(h);
 return code==STILL_ACTIVE;
}
static uint32_t reclaimed;
static void reclaim_pid(uint32_t pid) {
 uint32_t j;
 for(j=0;j<TABLES;j++)if(tables[j].local&&tables[j].owner==pid){table_release(&tables[j]);reclaimed++;}
 for(j=0;j<CONTEXTS;j++)if(contexts[j].handle&&contexts[j].owner==pid){memset(&contexts[j],0,sizeof(contexts[j]));reclaimed++;}
}
static void reclaim_dead(void) {
 uint32_t j;
 for(j=0;j<TABLES;j++)if(tables[j].local&&!owner_alive(tables[j].owner)){table_release(&tables[j]);reclaimed++;}
 for(j=0;j<CONTEXTS;j++)if(contexts[j].handle&&!owner_alive(contexts[j].owner)){memset(&contexts[j],0,sizeof(contexts[j]));reclaimed++;}
}
static surface_table *table_find(uint32_t owner,uint32_t local,int create) {
 uint32_t j,pass;surface_table *free_slot=0;
 for(j=0;j<TABLES;j++)if(tables[j].local==local&&tables[j].owner==owner)return &tables[j];
 if(!create)return 0;
 for(pass=0;pass<2;pass++) {
  free_slot=0;for(j=0;j<TABLES;j++)if(!tables[j].local){free_slot=&tables[j];break;}
  if(free_slot&&table_grow(free_slot)){free_slot->owner=owner;free_slot->local=local;return free_slot;}
  if(!pass)reclaim_dead();
 }
 return 0;
}
/* v10.463: ACCEL_D3D_TEXHINT (.527: always on, the registry value retired; its VX_D3D_Switch id 5 gone in .530): the slot a handle was last found in, by the handle's low 10 bits. VXD3DMON v7.0 (GTA3, about
 * 2,000 surfaces): a template build's textures resolved took 7.6 of its 16.3 microseconds, mostly this walk over the table (resolve, faces
 * and mips each walk it again). A hint is taken only when its slot is one of t's (its block is in t's list) and holds the handle; a table
 * holds a handle in one slot at most (CreateSurfaceEx finds the old slot first), so the result is the walk's. */
#define SF_HINTS 4096 /* .535: was 1024 indexed by the handle's low bits: GTA3's streamed textures' handles collided (surface_find 1.4% of its gameplay frames on .534) */
#define SF_HIX(h) (((h)*2654435761UL)>>20) /* 12 bits */
static struct {surface_table *t;uint32_t h;surface_slot *s;} sf_hint[SF_HINTS];
extern uint32_t vx_d3d_lost_rc,vx_d3d_lost_n; /* .539 (vx_d3d_core.c) */
extern uint32_t vx_d3d_ixpass,vx_d3d_v8,vx_d3d_v8_n,vx_d3d_delta,vx_d3d_dl[3]; /* .515 (vx_d3d_core.c); .524: IXPF removed */
uint32_t vx_d3d_texhint,vx_d3d_rtnowait; /* v10.477: ACCEL_D3D_RTNOWAIT (vx_ddraw.c tex_blit) */
static surface_slot *surface_find(surface_table *t,uint32_t h) {
 uint32_t j,n;surface_slot *s;if(!t||!h)return 0;
 if(vx_d3d_texhint){uint32_t i=SF_HIX(h),o;s=sf_hint[i].s;o=(uint32_t)((char*)s-(char*)&surface_pool[0][0]);
  if(sf_hint[i].t==t&&sf_hint[i].h==h&&o<sizeof(surface_pool)&&!(o%sizeof(surface_slot))&&s->handle==h){
   o/=sizeof(surface_slot)*BLOCK_SURFACES;for(j=0;j<t->blocks;j++)if(t->block[j]==o)return s;}}
 /* v10.319 (audit, optional A): each block's slots walked in place (table_at's block lookup and division once a block, not twice a slot) */
 for(j=0;j<t->blocks;j++){s=surface_pool[t->block[j]];for(n=0;n<BLOCK_SURFACES;n++,s++)if(s->handle==h){
  if(vx_d3d_texhint){uint32_t i=SF_HIX(h);sf_hint[i].s=s;sf_hint[i].h=h;sf_hint[i].t=t;}return s;}}
 return 0;
}
/* .538 (board, GTA3 with ACCEL_D3D_TNL on .537: the context lost, reason 20, after 40 s of gameplay, then SURFACELOST on every call and
 * two frames flipped over and over): a context is lost only when the surface object it draws to (its target or depth surface) is
 * replaced, not when another surface takes the same handle number. Direct3D 8's hardware vertex processing gives handles that can equal
 * the render target's while the target lives on; the context draws to its own copy of the target's description (core.target), so a
 * slot reused for another surface changes nothing it draws to. A target destroyed (DestroySurface) or made again with another layout
 * (CreateSurfaceEx, reason 21) still loses it. why: 1 system-memory surface, 2 a texture without memory yet, 3 video memory, 4 destroyed;
 * the reason's value: why << 28 | the handle */
static void invalidate_surface(surface_table *t,surface_slot *s,uint32_t why) {
 uint32_t j;
 for(j=0;j<CONTEXTS;j++) {
  context_slot *c=&contexts[j];
  if(c->handle&&c->owner==t->owner&&c->local==t->local&&s->native_key&&
    (c->target_key==s->native_key||c->depth_key==s->native_key)){c->core.lost=1;note_lost(0x20,why<<28|(s->handle&0x0FFFFFFFUL));}
 }
}
int VX_D3D_format(const DDPIXELFORMAT *f,uint32_t *format) {
 if(f->dwSize!=sizeof(*f))return 0;
 if(f->dwFlags==DDPF_RGB&&f->dwRGBBitCount==32&&f->dwRBitMask==0xFF0000&&f->dwGBitMask==0xFF00&&f->dwBBitMask==255&&!f->dwRGBAlphaBitMask){*format=VX3DN_XRGB8888;return 1;}
 if(f->dwFlags==DDPF_RGB&&f->dwRGBBitCount==16&&f->dwRBitMask==0xF800&&f->dwGBitMask==0x7E0&&f->dwBBitMask==31&&!f->dwRGBAlphaBitMask){*format=VX3DN_RGB565;return 1;}
 if(f->dwFlags==DDPF_ZBUFFER&&f->dwZBufferBitDepth==32&&f->dwZBitMask==0xFFFFFFFFUL&&!f->dwStencilBitMask){*format=VX3DN_D32;return 1;}
 /* v10.177: Z16 and D24S8 (XP's set) */
 if(f->dwFlags==DDPF_ZBUFFER&&f->dwZBufferBitDepth==16&&f->dwZBitMask==0xFFFFUL&&!f->dwStencilBitMask){*format=VX3DN_D16;return 1;}
 if(f->dwFlags==(DDPF_ZBUFFER|DDPF_STENCILBUFFER)&&f->dwZBufferBitDepth==32&&f->dwZBitMask==0xFFFFFF00UL&&f->dwStencilBitMask==0xFFUL){*format=VX3DN_D24S8;return 1;}
 /* v10.169: A8R8G8B8, a texture format only (an XRGB8888 texture is format 1 here; VX_D3D_surface names it VX3DN_TEX_XRGB8888) */
 if(f->dwFlags==(DDPF_RGB|DDPF_ALPHAPIXELS)&&f->dwRGBBitCount==32&&f->dwRBitMask==0xFF0000&&f->dwGBitMask==0xFF00&&f->dwBBitMask==255&&f->dwRGBAlphaBitMask==0xFF000000UL){*format=VX3DN_TEX_ARGB8888;return 1;}
 /* v10.172: 16-bit texture formats (texture use only): A1R5G5B5, A4R4G4B4, X1R5G5B5 */
 if(f->dwFlags==(DDPF_RGB|DDPF_ALPHAPIXELS)&&f->dwRGBBitCount==16&&f->dwRBitMask==0x7C00&&f->dwGBitMask==0x3E0&&f->dwBBitMask==31&&f->dwRGBAlphaBitMask==0x8000){*format=VX_RAW_1555;return 1;}
 if(f->dwFlags==(DDPF_RGB|DDPF_ALPHAPIXELS)&&f->dwRGBBitCount==16&&f->dwRBitMask==0xF00&&f->dwGBitMask==0xF0&&f->dwBBitMask==15&&f->dwRGBAlphaBitMask==0xF000){*format=VX_RAW_4444;return 1;}
 if(f->dwFlags==DDPF_RGB&&f->dwRGBBitCount==16&&f->dwRBitMask==0x7C00&&f->dwGBitMask==0x3E0&&f->dwBBitMask==31&&!f->dwRGBAlphaBitMask){*format=VX_RAW_555;return 1;}
 /* v10.199: L8 (DDPF_LUMINANCE, 8 bits) and DXT1 / DXT3 / DXT5 (XP's set, CLXP3D T31, T33-T35) */
 if(f->dwFlags==DDPF_LUMINANCE&&f->dwRGBBitCount==8&&f->dwRBitMask==0xFF){*format=VX_RAW_L8;return 1;}
 if(f->dwFlags==DDPF_FOURCC&&f->dwFourCC==0x31545844UL){*format=VX_RAW_DXT1;return 1;}
 if(f->dwFlags==DDPF_FOURCC&&f->dwFourCC==0x33545844UL){*format=VX_RAW_DXT3;return 1;}
 if(f->dwFlags==DDPF_FOURCC&&f->dwFourCC==0x35545844UL){*format=VX_RAW_DXT5;return 1;}
 /* v10.208: du/dv bump formats (CLXP3D T146-T148): the masks are dwBumpDuBitMask / DvBitMask / LuminanceBitMask in the RGB fields */
 if(f->dwFlags==DDPF_BUMPDUDV&&f->dwRGBBitCount==16&&f->dwRBitMask==0xFF&&f->dwGBitMask==0xFF00UL&&!f->dwBBitMask){*format=VX_RAW_V8U8;return 1;}
 if(f->dwFlags==(DDPF_BUMPDUDV|DDPF_BUMPLUMINANCE)&&f->dwRGBBitCount==16&&f->dwRBitMask==0x1F&&f->dwGBitMask==0x3E0&&f->dwBBitMask==0xFC00UL){*format=VX_RAW_L6V5U5;return 1;}
 if(f->dwFlags==(DDPF_BUMPDUDV|DDPF_BUMPLUMINANCE)&&f->dwRGBBitCount==32&&f->dwRBitMask==0xFF&&f->dwGBitMask==0xFF00UL&&f->dwBBitMask==0xFF0000UL){*format=VX_RAW_X8L8V8U8;return 1;}
 return 0;
}
/* v10.172: VX_D3D_format code -> texture format; 0 when it is not a texture format */
uint32_t VX_D3D_texture_format(uint32_t f) {
 switch(f){case VX3DN_XRGB8888:return VX3DN_TEX_XRGB8888;case VX3DN_TEX_ARGB8888:return VX3DN_TEX_ARGB8888;case VX3DN_RGB565:return VX3DN_TEX_RGB565;
  case VX_RAW_1555:return VX3DN_TEX_ARGB1555;case VX_RAW_4444:return VX3DN_TEX_ARGB4444;case VX_RAW_555:return VX3DN_TEX_XRGB1555;
  case VX_RAW_L8:return VX3DN_TEX_L8;case VX_RAW_DXT1:return VX3DN_TEX_DXT1;case VX_RAW_DXT3:return VX3DN_TEX_DXT3;case VX_RAW_DXT5:return VX3DN_TEX_DXT5;
  case VX_RAW_V8U8:return VX3DN_TEX_V8U8;case VX_RAW_L6V5U5:return VX3DN_TEX_L6V5U5;case VX_RAW_X8L8V8U8:return VX3DN_TEX_X8L8V8U8;}
 return 0;
}
uint32_t VX_D3D_format_bytes(uint32_t f) {return f==VX_RAW_L8?1:(f==VX3DN_RGB565||f==VX3DN_D16||f==VX_RAW_1555||f==VX_RAW_4444||f==VX_RAW_555||f==VX_RAW_V8U8||f==VX_RAW_L6V5U5)?2:4;}
/* v10.199: a texture's row (of texels, or of 4x4 blocks for DXT: 8 bytes a block for DXT1, 16 for DXT3/5) and number of rows.
 * v10.200: the sampler steps rows at least 32 bytes apart (board, v10.199: an 8x8 L8 texture read black, an 8x8 DXT1 lost its second block
 * row and read the bytes at offset 32 instead): VX_D3D_texture_pitch is the row the hardware reads, VX_D3D_texture_packed the row as
 * written (a DXT surface's linear layout). */
uint32_t VX_D3D_texture_packed(uint32_t f,uint32_t w) {return VX_RAW_IS_DXT(f)?(w<4?1:w/4)*(f==VX_RAW_DXT1?8UL:16UL):w*VX_D3D_format_bytes(f);}
uint32_t VX_D3D_texture_pitch(uint32_t f,uint32_t w) {uint32_t r=VX_D3D_texture_packed(f,w);return r<32?32:r;}
uint32_t VX_D3D_texture_rows(uint32_t f,uint32_t h) {return VX_RAW_IS_DXT(f)?(h<4?1:h/4):h;}
/* v10.169: a texture the hardware samples: power-of-two sides, rows of exactly width x bytes (the sampler takes no pitch). v10.199: up to
 * 2048 (XP's limit, CLXP3D T126-T128; ring 0 always took 2048). */
static int pow2(uint32_t v) {return v&&!(v&(v-1));}
int VX_D3D_texture_ok(uint32_t f,uint32_t w,uint32_t h) {return VX_D3D_texture_format(f)&&pow2(w)&&pow2(h)&&w<=2048&&h<=2048;}
/* v10.185: why the last VX_D3D_surface call said no (HAL event log): 1 no HAL/object/INVALID, 2 not local video memory, 3 pixel format,
 * 4 memory outside the frame buffer or pitch <= 0, 5 texture caps/size/pitch, 6 texture placement, 7 target format, 8 target geometry */
uint32_t vx_surface_why;
/* v10.456 (3DMark2001 SE: CreateTexture of a render target failed, D3DERR_NOTAVAILABLE; cookbook 24.8 P0): ACCEL_D3D_RTTEX (default 1): a
 * texture that is also a render target (DDSCAPS_TEXTURE | DDSCAPS_3DDEVICE), X8R8G8B8 or R5G6B5 (v10.459: and A8R8G8B8), one level, power-of-two sides up to 2048,
 * local video memory. Its rows are the texture's (width x bytes, at least 32: always a multiple of 32 for a power of two, so the target's
 * 32-byte pitch rule holds too) on a 256-byte base. It is resolved by role: a texture for sampling, the target format for drawing. */
uint32_t vx_d3d_rttex,vx_d3d_mipcap; /* the VxD's ACCEL_D3D_RTTEX, ACCEL_D3D_MIPCAP (0 with an older VxD) */
int VX_D3D_rt_texture(const DDRAWI_DDRAWSURFACE_LCL *s) {
 uint32_t f;const DDPIXELFORMAT *pf;DWORD c;
 if(!vx_d3d_rttex||!s||!s->lpGbl)return 0;c=s->ddsCaps.dwCaps;
 if((c&(DDSCAPS_TEXTURE|DDSCAPS_3DDEVICE))!=(DDSCAPS_TEXTURE|DDSCAPS_3DDEVICE)||(c&(DDSCAPS_ZBUFFER|DDSCAPS_PRIMARYSURFACE|DDSCAPS_MIPMAP|DDSCAPS_SYSTEMMEMORY|DDSCAPS_NONLOCALVIDMEM|DDSCAPS_OVERLAY|DDSCAPS_FLIP)))return 0;
 if(s->lpSurfMore&&!IsBadReadPtr(s->lpSurfMore,sizeof(*s->lpSurfMore))&&(s->lpSurfMore->ddsCapsEx.dwCaps2&DDSCAPS2_CUBEMAP))return 0;
 pf=(s->dwFlags&DDRAWISURF_HASPIXELFORMAT)?&s->lpGbl->ddpfSurface:(shared_hal?&shared_hal->ddpf:0);
 return pf&&VX_D3D_format(pf,&f)&&(f==VX3DN_XRGB8888||f==VX3DN_RGB565||f==VX3DN_TEX_ARGB8888); /* v10.459: A8R8G8B8 (3DMark2001 SE) */
}
int VX_D3D_surface(DDRAWI_DDRAWSURFACE_LCL *s,vx3dn_surface *d) {
 DDRAWI_DDRAWSURFACE_GBL *g;uint32_t f,limit,bytes;const DDPIXELFORMAT *pf;
 vx_surface_why=1;if(!shared_hal||!s||!s->lpGbl||s->dwFlags&DDRAWISURF_INVALID)return 0;
 vx_surface_why=2;if(!(s->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY)||s->ddsCaps.dwCaps&(DDSCAPS_NONLOCALVIDMEM|DDSCAPS_SYSTEMMEMORY|DDSCAPS_OVERLAY))return 0;
 g=s->lpGbl;pf=(s->dwFlags&DDRAWISURF_HASPIXELFORMAT)?&g->ddpfSurface:&shared_hal->ddpf;
 vx_surface_why=3;if(!VX_D3D_format(pf,&f))return 0;
 vx_surface_why=4;if(g->fpVidMem<shared_hal->vramLinear||g->lPitch<=0)return 0;
 d->offset=g->fpVidMem-shared_hal->vramLinear;d->pitch=(uint32_t)g->lPitch;d->width=g->wWidth;d->height=g->wHeight;
 limit=shared_hal->vramSize;if(limit>VX_FB_LIMIT)limit=VX_FB_LIMIT;
 if(s->ddsCaps.dwCaps&DDSCAPS_TEXTURE) {
  vx_surface_why=5;
  /* v10.199: a DXT surface's lPitch is its linear size (16.10, as XP's driver reports it); the packet gets the block-row pitch */
  if((s->ddsCaps.dwCaps&(DDSCAPS_ZBUFFER|DDSCAPS_PRIMARYSURFACE))||((s->ddsCaps.dwCaps&DDSCAPS_3DDEVICE)&&!VX_D3D_rt_texture(s))||!VX_D3D_texture_ok(f,d->width,d->height))return 0; /* v10.456: a render-target texture */
  if(VX_RAW_IS_DXT(f)){if(d->pitch<VX_D3D_texture_packed(f,d->width)*VX_D3D_texture_rows(f,d->height))return 0;d->pitch=VX_D3D_texture_pitch(f,d->width);}
  else if(d->pitch!=VX_D3D_texture_pitch(f,d->width))return 0;
  d->format=VX_D3D_texture_format(f);
  vx_surface_why=6;if(!(!(d->offset&255)&&d->offset<limit&&VX_D3D_texture_rows(f,d->height)<=(limit-d->offset)/d->pitch))return 0;
  vx_surface_why=0;return 1;
 }
 vx_surface_why=7;if(f!=VX3DN_XRGB8888&&f!=VX3DN_RGB565&&!VX3DN_IS_DEPTH(f))return 0;
 d->format=f;bytes=VX_D3D_format_bytes(f);
 vx_surface_why=8;
 if(!(d->width&&d->height&&d->width<=2048&&d->height<=2048&&d->pitch<=0x7FE0UL&&!(d->offset&255)&&!(d->pitch&31)&&
  d->width<=d->pitch/bytes&&d->offset<limit&&d->height<=(limit-d->offset)/d->pitch))return 0;
 if(VX3DN_CZ_FORMAT(f)&&VX3DN_CZ_END(d)>limit)return 0; /* v10.245: and its stencil plane; v10.294: and the Coarse Z storage after it; v10.295: Z16's too */
 vx_surface_why=0;return 1;
}
/* v10.456: a surface as a render target: a render-target texture in its target format (the texture's rows and base), anything else as
 * VX_D3D_surface. Returns 2 for a texture, 1 otherwise, 0 when it cannot be drawn into. */
int VX_D3D_surface_target(DDRAWI_DDRAWSURFACE_LCL *s,vx3dn_surface *d) {
 if(!VX_D3D_surface(s,d))return 0;
 if(!(s->ddsCaps.dwCaps&DDSCAPS_TEXTURE))return 1;
 if(!VX_D3D_rt_texture(s)){vx_surface_why=7;return 0;}
 d->format=d->format==VX3DN_TEX_XRGB8888?VX3DN_XRGB8888:d->format==VX3DN_TEX_ARGB8888?VX3DN_ARGB8888:d->format==VX3DN_TEX_RGB565?VX3DN_RGB565:0; /* v10.460: A8R8G8B8 drawn as the A8R8G8B8 target (the engine writes alpha) */
 vx_surface_why=8;if(!d->format||(d->pitch&31)||d->pitch>0x7FE0UL||d->width>d->pitch/VX_D3D_format_bytes(d->format))return 0;
 vx_surface_why=0;return 2;
}
/* v10.456: before a render-target texture stops being the target (the draws into it are sent): wait until the engine has finished them and
 * have the VxD send its whole state again (texture set-up included) with the next draw, so nothing samples the texture before its pixels are
 * written; the OpenGL ICD's proven order for its render-to-texture (vxgl_state.c rtt_done). A Lock waits the same way (VXLock). */
/* v10.479 (review): the wait no longer clears rt_tex: the caller sets it once the change is made, so a change refused after the wait (a
 * bad depth handle, targets that do not fit) leaves the texture the target and still marked, and the next change waits again. Returns 0 when
 * the wait reported the device lost (the VxD's VX2D_LOST, or the call failed), 1 otherwise. */
uint32_t vx_d3d_rt_waits;
static int rt_leave(context_slot *c,const vx3dn_surface *old) {
 DWORD in[3],rc=3;if(!c->rt_tex)return 1;vx_d3d_rt_waits++; /* 3 = VX2D_LOST */
 in[0]=0;in[1]=old->offset;in[2]=old->offset+old->pitch*old->height;
 if(!call(0x85500017UL,in,12,&rc,4))rc=3;
 return rc!=3;
}
/* Match the native owner's usable pool and primary geometry at binding time.
 * vramSize also includes reservations which DirectDraw cannot allocate. */
static int native_surface(const vx3dn_surface *s,const vx3dn_info *i) {
 if(!s->pitch||s->offset>=i->fb_bytes||s->height>(i->fb_bytes-s->offset)/s->pitch)return 0;
 if(s->offset<i->primary_bytes&&
    (VX3DN_IS_DEPTH(s->format)||s->offset||s->pitch!=i->pitch||s->width!=i->width||s->height!=i->height||
     s->format!=(i->bpp==16?VX3DN_RGB565:VX3DN_XRGB8888)))return 0;
 return 1;
}
/* v10.192: a packet the VxD refused (WinTune: the first triangle came back lost and the display froze) goes to the event log with what
 * it held: callback 7 = result | VxD fault << 8 | VxD last result << 16; a = op << 24 | texture on << 20 | texture format << 12 |
 * depth format << 4 | target format; b = texture size; c = VxD last status. Callback 8 = vertex count; target, depth, texture offsets.
 * Callback 9 = first vertex x, y, z bits; target size (w << 16 | h). Only the first 4 refusals of a process are written. */
static uint32_t refusals,refusal_pid;
/* v10.196: callback 11 = a fogged packet reached the VxD: why = fog mode register (70) | program count << 8; a, b, c = the three vertices'
 * specular (fog factor in alpha). Written for the first fogged packet of a process and again whenever a lower fog alpha turns up (8 at most). */
static uint32_t fog_pid,fog_logged,fog_min;
/* .527: what the HAL sent of a refused packet (callbacks 8-10, as v10.192 wrote them for single submits only) and the VxD's own record of
 * the refusal (vx_native3d_vxd.c bad_note, TEXDBG BC EC/ED): callback 13 = result; why = path << 24 | where (1 << 16 | the line in
 * vx_native3d_hw.c, 2 << 16 | the line in vx_native3d_vxd.c); a = index in the batch << 16 | the batch's packets; b = vertex count |
 * pre.on << 16 | bursts << 20 | geometry-only validation << 31; c = the state ranges marked the same. Callback 14 = the VxD's copy:
 * why = op and formats as callback 7's a; a = target offset; b = target size; c = texture offset. Both paths now write all of them. */
static void refusal_events(const vx3dn_packet *p,uint32_t rc) {
 DWORD in[2],o[8],v[12];int k;
 event(8,0,p->vertex_count,p->target.offset,p->depth.offset,p->tex.enable?p->tex.surface.offset:0);
 event(9,0,p->target.width<<16|(p->target.height&0xFFFF),p->pre.on?p->pre.w[0]:p->vertices[0].x,p->pre.on?p->pre.w[1]:p->vertices[0].y,p->pre.on?p->pre.w[2]:p->vertices[0].z);
 /* callback 10: pixel-program instruction count; z_enable | z_write << 1 | z_func << 4 | blend << 8 | write_mask << 12; blend src | dst << 8 |
  * alpha func << 16 | alpha ref << 24; clip right << 16 | bottom */
 event(10,0,p->shader.count,(p->z_enable&1)|(p->z_write&1)<<1|(p->z_func&15)<<4|(p->blend&1)<<8|(p->write_mask&15)<<12,
  (p->raster.blend_src&255)|(p->raster.blend_dst&255)<<8|(p->raster.alpha_func&255)<<16|(p->raster.alpha_ref&255)<<24,(uint32_t)p->clip.right<<16|((uint32_t)p->clip.bottom&0xFFFF));
 if((rc&0x7F)!=VX3DN_BAD_PACKET)return; /* 13 and 14 only for a packet the VxD refused */
 for(k=0;k<2;k++){in[0]=0xBC;in[1]=0xEC+k;memset(o,0,sizeof(o));if(!call(VX_IOCTL_3DN_TEXDBG,in,8,o,32)||(o[0]&0xFF)!=0xBC)return;memcpy(v+6*k,o+2,24);}
 event(13,rc,v[1],v[2]<<16|(v[3]&0xFFFF),v[5],v[6]);
 event(14,0,v[4],v[7],v[8],v[9]);
}
static void fog_event(const vx3dn_packet *p) {
 uint32_t k,lo=255;
 if(!p->fog.enable||p->op!=VX3DN_DRAW)return;
 if(fog_pid!=GetCurrentProcessId()){fog_pid=GetCurrentProcessId();fog_logged=0;fog_min=256;}
 for(k=0;k<3&&k<p->vertex_count;k++)if((p->shader.spec[k]>>24)<lo)lo=p->shader.spec[k]>>24;
 if(fog_logged>=8||lo>=fog_min)return;
 fog_min=lo;fog_logged++;
 event(11,0,(p->fog.r[0]&255)|p->shader.count<<8,p->shader.spec[0],p->shader.spec[1],p->shader.spec[2]);
}
/* v10.217: the distinct draw states the HAL sent (first 32, kept across processes in the shared DLL): run an application, then VXDX7
 * --draws prints them. Record = 40 words: hash, packets, triangles, process; op, prim, target format, depth format; tex enable, format,
 * size (w << 16 | h), filter | address_u << 8 | address_v << 16; z enable | write << 4 | func << 8, blend | src << 8 | dst << 16,
 * alpha func | ref << 8, write mask | dither << 4 | stencil << 8; fog enable | 70 << 8, shader count, kflags, units | layout << 8 | ps << 16;
 * the first vertex's colour, specular, u, v, x, y, z, rhw; diffuse min (per channel, AARRGGBB), diffuse max, specular max; program words
 * 0..7. */
#define DRAWREC 32
static uint32_t draw_rec[DRAWREC][40],draw_recs;
/* v10.321: the records are kept only while capture is on: ACCEL_D3D_LOG = 1, or from the first VX_D3D_QueryDraws call of any process (VXDX7
 * --draws, VXDISCO) until the display driver is reloaded. Off, a packet costs no signature, hash or colour scan. */
static volatile uint32_t draw_capture;
/* v10.236: the triangles in a draw packet (a strip run of n vertices holds n - 2) */
static uint32_t packet_triangles(const vx3dn_packet *p) {
 uint32_t k,t=0;
 if(p->cb!=sizeof(*p)||!p->bursts.n||p->bursts.n>VX3DN_MAX_BURSTS)return p->vertex_count/3;
 for(k=0;k<p->bursts.n;k++){uint32_t c=p->bursts.b[k]&0xFFFFUL,tp=p->bursts.b[k]>>16;t+=tp==VX3DN_TOPO_STRIP?(c>=2?c-2:0):c/3;} /* v10.470: an indexed run counts indices */
 return t;
}
static void draw_record(const vx3dn_packet *p) {
 uint32_t h=2166136261UL,k,*r=0,v;const uint32_t *w;
 uint32_t sig[24];
 sig[0]=p->op;sig[1]=p->ext.prim;sig[2]=p->target.format;sig[3]=p->depth.format;sig[4]=p->tex.enable;sig[5]=p->tex.surface.format;
 sig[6]=p->tex.surface.width<<16|(p->tex.surface.height&0xFFFF);sig[7]=p->tex.filter|p->tex.address_u<<8|p->tex.address_v<<16;
 sig[8]=p->z_enable|p->z_write<<4|p->z_func<<8;sig[9]=p->blend|p->raster.blend_src<<8|p->raster.blend_dst<<16;sig[10]=p->raster.alpha_func|p->raster.alpha_ref<<8;
 sig[11]=p->write_mask|p->ext.dither<<4|p->ext.stencil<<8;sig[12]=p->fog.enable|(p->fog.r[0]&255)<<8;sig[13]=p->shader.count;sig[14]=p->shader.kflags;
 sig[15]=p->stages.units|p->stages.layout<<8|p->stages.ps<<16;
 for(k=0;k<8;k++)sig[16+k]=p->shader.words[k];
 for(k=0;k<24;k++){h^=sig[k];h*=16777619UL;}
 for(k=0;k<draw_recs;k++)if(draw_rec[k][0]==h){r=draw_rec[k];break;}
 if(!r){if(draw_recs>=DRAWREC)return;r=draw_rec[draw_recs++];memset(r,0,40*4);r[0]=h;r[3]=GetCurrentProcessId();
  for(k=0;k<12;k++)r[4+k]=sig[k];r[16]=sig[12];r[17]=sig[13];r[18]=sig[14];r[19]=sig[15];
  if(p->vertex_count){const vx3dn_vertex *a=&p->vertices[0];r[20]=a->color;r[21]=p->shader.spec[0];r[22]=p->tex.uv[0][0];r[23]=p->tex.uv[0][1];r[24]=a->x;r[25]=a->y;r[26]=a->z;r[27]=a->rhw;}
  r[28]=0xFFFFFFFFUL;for(k=0;k<8;k++)r[32+k]=sig[16+k];}
 r[1]++;r[2]+=p->op==VX3DN_DRAW?packet_triangles(p):0;
 if(r[1]>8)return; /* v10.222: the colour range from a state's first 64 packets (the scan cost a twentieth of the HAL's time); v10.236: 8 (192-vertex packets) */
 for(v=0;v<p->vertex_count;v++){uint32_t c=p->vertices[v].color,s=p->shader.spec[v],sh;
  for(sh=0;sh<32;sh+=8){uint32_t m=0xFFUL<<sh;if((c&m)<(r[28]&m))r[28]=(r[28]&~m)|(c&m);if((c&m)>(r[29]&m))r[29]=(r[29]&~m)|(c&m);if((s&m)>(r[30]&m))r[30]=(r[30]&~m)|(s&m);}}
 (void)w;
}
DWORD WINAPI VX_D3D_QueryDraws(DWORD *out,DWORD n) {
 DWORD k,m,reset=n&0x80000000UL,off=n&0x40000000UL;n&=0x3FFFFFFFUL;if(!out||n<1)return 0;
 draw_capture=!off; /* v10.321: a reader turns capture on; n | 0x40000000 turns it off after this read */
 if(!cb_enter())return 0;
 m=draw_recs;if(1+m*40>n)m=(n-1)/40;out[0]=draw_recs;for(k=0;k<m*40;k++)out[1+k]=((uint32_t*)draw_rec)[k];
 if(reset){draw_recs=0;memset(draw_rec,0,sizeof(draw_rec));}
 cb_leave();return 1+m*40;
}
/* v10.218: VX3DN_BUSY means the VxD sent nothing to the engine (the 3D or 2D side was running, or the sequencer was not in a drawable
 * state), so the same packet is sent again after a short wait, for up to 500 ms (600 attempts). It went back to the core, which marks the context lost
 * when an earlier packet of the same DrawPrimitives2 call has been drawn: one busy moment lost the device for good. Callback 12 = a packet
 * that had to wait: why = attempts, a = milliseconds, b = the final result, c = the VxD's result for the first refusal (busy reason, vx_native3d_vxd.c);
 * the first 4 of a process are logged. A wait that runs out is not repeated until a packet goes through. */
static uint32_t busy_pid,busy_logged,busy_stuck;
/* .522 (board, GTA3 .521 with the RTC sampler: 23% of the render thread's VxD time in calls other than draw submits; one of them every
 * DrawPrimitives2 call, VX_IOCTL_3DN_INFO, only to see the VxD ready and the mode generation unchanged): ACCEL_D3D_INFOCACHE (default 1,
 * the VxD's feature word bit 17; VX_D3D_Switch 11 until .530): the answer kept and used again while it said ready, for the context's generation, and
 * is under 100 ms old; asked again after any refused submit (not busy, not recovered), at a HAL rebuild (a mode change) and otherwise
 * every 100 ms. The VxD still checks every packet's generation and readiness (vx3dn_validate: STALE, LOST), and a refused packet marks
 * the context lost as before (vx_d3d_submit_result), so a change between two answers loses at most the call it happens in.
 * ic_stat: [0] answers used again, [1] VxD calls made. */
uint32_t vx_d3d_infocache=1,ic_stat[2];static vx3dn_info ic_info;static uint32_t ic_ok,ic_t;
static void ic_drop(void) {ic_ok=0;}
/* v10.220: where the time goes (TSC cycles / 256; shared DLL data): [0] DrawPrimitives2 calls, [1] cycles in them, [2] packet submissions,
 * [3] cycles in the submit IOCTLs, [4] triangles in draw packets. VX_D3D_QueryProfile(out, n): n | 0x80000000 also clears. */
unsigned long VX_hal_tsc(void);
#pragma aux VX_hal_tsc = 0x0F 0x31 value [eax] modify exact [eax edx];
static uint32_t hal_prof[8];
#include "vx_lat.h"
/* v10.321: distributions (shared DLL data, every process): [0] a DrawPrimitives2 call, [1] a submit IOCTL (TSC cycles), [2] the busy
 * retries of a submit that had to wait (attempts, not cycles). VX_D3D_QueryLatency(out, n): six words each (samples, p50, p95, p99, max,
 * sum / 256), n | 0x80000000 also clears. */
static vx_lat hal_lat[3];
/* v10.329: the HAL's profile only after a tool asked for one (a query that clears: VXDISCO, VXDX7), shared DLL data; off it reads no TSC */
static volatile uint32_t hal_prof_on;
static uint32_t HPT(void) {return hal_prof_on?VX_hal_tsc():0UL;}
#define HLAT(l,v) do{if(hal_prof_on)vx_lat_add(l,v);}while(0)
DWORD WINAPI VX_D3D_QueryLatency(DWORD *out,DWORD n) {
 DWORD k,i,reset=n&0x80000000UL,stop=n&0x40000000UL;n&=0x3FFFFFFFUL;if(!out||n<18)return 0;if(stop)hal_prof_on=0;else if(reset)hal_prof_on=1; /* v10.367 (audit): n | 0x40000000 turns the profile off (it stayed on after the first clearing query) */
 for(k=0;k<3;k++)vx_lat_out(&hal_lat[k],(uint32_t*)out+k*6);
 if(reset)for(k=0;k<3;k++){hal_lat[k].n=hal_lat[k].sum=hal_lat[k].max=0;for(i=0;i<VX_LAT_B;i++)hal_lat[k].h[i]=0;}
 return 18;
}
DWORD WINAPI VX_D3D_QueryProfile(DWORD *out,DWORD n) {
 DWORD k,reset=n&0x80000000UL,stop=n&0x40000000UL;n&=0x3FFFFFFFUL;if(!out||!n)return 0;if(stop)hal_prof_on=0;else if(reset)hal_prof_on=1;if(n>8)n=8; /* v10.367 */
 for(k=0;k<n;k++)out[k]=hal_prof[k];
 if(reset)for(k=0;k<8;k++)hal_prof[k]=0;
 return n;
}
static uint32_t submit_one(const vx3dn_packet *p);
/* v10.368 (audit N9): the first retry of a busy answer waits in ring 0 (VX_IOCTL_2D_ACCESS action 0: the asynchronous blit and the draw in
 * flight are finished there) instead of giving up the time slice; later ones as before (Sleep 0 seven times, then Sleep 1 a try) */
#ifndef VX_IOCTL_2D_ACCESS
#define VX_IOCTL_2D_ACCESS 0x85500017UL
#endif
uint32_t vx_d3d_busy_waits;
/* v10.387 (board, GTA3 v10.386: a frame spent 855 ms in 31 DrawPrimitives2 calls, about 28 ms a call): the slow calls. While a
 * DrawPrimitives2 call runs, its VxD calls' time, the busy answers it retried, their time and why the VxD said busy (VX3DN_info
 * last_result at the first) are summed; a call of 10 ms or more is kept (the 8 longest, VX_D3D_QuerySlow). TSC cycles / 256. */
static uint32_t sl_on,sl_ioctl,sl_tries,sl_busy_t,sl_why,sl_calls,sl_sleeps;
static volatile uint32_t sl_worst[8][8],sl_count,sl_total;
static volatile uint32_t sl_ops[8][6]; /* v10.390: each kept call's 3 longest opcodes: op | count << 8, TSC / 256 */
/* v10.414: where a slow call's submit time went (the VxD stamps each submit's entry and exit, TSC and the thread's run time): [0] before the
 * VxD's entry, [1] inside, [2] after its exit, TSC / 256; [3] the thread's run time inside (ms); [4] between one submit's exit and the next's
 * entry in the same call (TSC / 256) and [5] the thread's run time then (ms); [6] the longest single before-entry, [7] after-exit */
static uint32_t sl_io[8],sl_px,sl_pxe,sl_pv;static volatile uint32_t sl_iow[8][8];
static void sl_io_add(uint32_t s0,uint32_t s1,const uint32_t *o) { /* o: entry TSC, exit TSC, run ms at entry, at exit (0: an older VxD) */
 uint32_t a,b,c;if(!sl_on||!o[0]||!o[1])return;
 a=(o[0]-s0)>>8;b=(o[1]-o[0])>>8;c=(s1-o[1])>>8;sl_io[0]+=a;sl_io[1]+=b;sl_io[2]+=c;sl_io[3]+=o[3]-o[2];
 if(sl_pv){sl_io[4]+=(o[0]-sl_px)>>8;sl_io[5]+=o[2]-sl_pxe;}
 if(a>sl_io[6])sl_io[6]=a;if(c>sl_io[7])sl_io[7]=c;sl_px=o[1];sl_pxe=o[3];sl_pv=1;
}
DWORD WINAPI VX_D3D_QuerySlowIo(DWORD *out,DWORD n) { /* v10.414: [0..63] sl_io of each call VX_D3D_QuerySlow keeps, in its order */
 DWORD k,j;if(!out||(n&0x7FFFFFFFUL)<64)return 0;for(k=0;k<8;k++)for(j=0;j<8;j++)out[k*8+j]=sl_iow[k][j];
 if(n&0x80000000UL)for(k=0;k<8;k++)for(j=0;j<8;j++)sl_iow[k][j]=0;
 return 64;
}
static uint32_t hal_clock(void);
/* v10.431: DrawPrimitives2's time in exclusive phases (vx_d3d_core.h VX_PH_*), on while a tool asked for it (VX_D3D_QueryPhases with
 * n | 0x80000000 clears and turns it on, n | 0x40000000 turns it off; off, nothing is read). Per phase: the sum, the switches into it, the
 * calls that spent time in it, the most one call spent there and those calls by their time in it (under 10, 10-50, 50-100, 100-500,
 * 500-1000, 1000+ microseconds); per call: the calls by their whole time with their time and the thread's run time (ms, the VxD's
 * _GetThreadExecTime read at the call's start and end, charged to VX_PH_PROF); the submit calls' time inside the VxD and the thread's run
 * time there; the bad-vertex checks by reason. Shared DLL data, under the callback lock. */
static volatile uint32_t ph_on;
static uint32_t ph_sum[VX_PH_N],ph_cnt[VX_PH_N],ph_max[VX_PH_N],ph_calls[VX_PH_N],ph_h[VX_PH_N][6];
static uint32_t pc_n,pc_h[6],pc_wall[6],pc_run[6],pc_runok[6],pv_n,pv_in,pv_run;
DWORD VX_dd_cpms(void);
static uint32_t ph_bucket(uint32_t cyc,uint32_t cpus) {uint32_t us=cyc/cpus;return us<10?0:us<50?1:us<100?2:us<500?3:us<1000?4:5;}
/* .525: with the run time, the VMM's page faults and page-ins (TEXDBG BD 81 [4], [5]; 0 from an older VxD): per bucket of the calls' whole
 * time (pc_pf, pc_pi), and the sampled calls of 10 ms or more apart (pc_sl: [0] calls, [1] their ms, [2] ms the thread ran, [3] page
 * faults, [4] page-ins): were the long calls waiting on the disk? */
static uint32_t ph_pf[2],pc_pf[6],pc_pi[6],pc_sl[5];
static uint32_t ph_run_ms(int at) {DWORD in[2],o[8];in[0]=0xBD;in[1]=0x81;o[0]=0;o[2]=0;o[4]=0;o[5]=0;if(!call(VX_IOCTL_3DN_TEXDBG,in,8,o,32)||o[0]!=0xBD)return 0xFFFFFFFFUL;
 if(at)ph_pf[0]=o[4]-ph_pf[0],ph_pf[1]=o[5]-ph_pf[1];else ph_pf[0]=o[4],ph_pf[1]=o[5];return o[2];}
static void ph_vxd(const uint32_t *o) {if(vx_d3d_phclock&&o[0]&&o[1]){pv_n++;pv_in+=(o[1]-o[0]+128)>>8;if(o[3]>=o[2])pv_run+=o[3]-o[2];}} /* entry TSC, exit TSC, run ms at entry, at exit */
static void ph_account(uint32_t run0,uint32_t run1) {
 uint32_t cpus=VX_dd_cpms()/1000UL,k,wall=0,b;if(!cpus)cpus=1000;
 for(k=0;k<VX_PH_N;k++){uint32_t d=vx_d3d_ph_call[k];wall+=d;ph_cnt[k]+=vx_d3d_ph_cnt[k];vx_d3d_ph_cnt[k]=0;
  if(d){ph_sum[k]+=(d+128)>>8;ph_calls[k]++;if((d>>8)>ph_max[k])ph_max[k]=d>>8;ph_h[k][ph_bucket(d,cpus)]++;}}
 b=ph_bucket(wall,cpus);pc_n++;pc_h[b]++;pc_wall[b]+=(wall+128)>>8;
 if(run0!=0xFFFFFFFFUL&&run1!=0xFFFFFFFFUL&&run1>=run0){pc_run[b]+=run1-run0;pc_runok[b]++;pc_pf[b]+=ph_pf[0];pc_pi[b]+=ph_pf[1]; /* .525 */
  if(wall/cpus>=10000UL){pc_sl[0]++;pc_sl[1]+=wall/cpus/1000UL;pc_sl[2]+=run1-run0;pc_sl[3]+=ph_pf[0];pc_sl[4]+=ph_pf[1];}}
}
/* .527: ids 0 (VRECIPE), 1 (FASTMATCH), 2 (FASTVV) and 6 (PFVV2) removed: they answer 0xFFFFFFFF as unknown ids. .528: 15 ACCEL_D3D_TEXRUNS. */
/* .533: VX_D3D_Switch (v10.446-.532: the HAL's switches for an A/B inside one run) removed: no switch is left; a feature under test
 * brings its own back */
DWORD WINAPI VX_D3D_QueryPhases(DWORD *out,DWORD n) {
 DWORD k,i,reset=n&0x80000000UL,stop=n&0x40000000UL,w[366];extern DWORD vx_lock_result[4];extern DWORD vx_flip_seen[7];extern DWORD vx_init_seen[3];
 /* .572: bit 29 (the test switch of .555-.557, .559-.563 and .567-.569) does nothing: no switch is left; word 343 reads 0. 350-355: the culled pass-through's counters (vx_d3d_core.c vx_d3d_cp) */
 n&=0x00FFFFFFUL;if(!out||!n)return 0; /* .561: the count is the low 24 bits (.559-.560 kept flag bits 24-27 in it: a count of 1 with bit 24 set was answered with every word, over the caller's one-word buffer) */
 w[0]=0x431;w[1]=VX_dd_cpms()/1000UL;w[2]=pc_n;w[270]=vx_d3d_tnl;for(k=0;k<18;k++)w[271+k]=vx_d3d_tlp[k];w[289]=0; /* .534: the T&L probe (vx_d3d_core.c) */{extern uint32_t vx_d3d_tn[12];for(k=0;k<12;k++)w[290+k]=vx_d3d_tn[k];} /* .536: the driver's T&L (vx_d3d_tnl.inc) */
 for(k=0;k<6;k++){w[3+k]=pc_h[k];w[9+k]=pc_wall[k];w[15+k]=pc_run[k];w[21+k]=pc_runok[k];}
 for(k=0;k<4;k++)w[27+k]=vx_d3d_bad_why[k];
 w[31]=pv_n;w[32]=pv_in;w[33]=pv_run;
 for(k=0;k<VX_PH_N;k++){DWORD *q=w+34+k*11;q[0]=ph_sum[k];q[1]=ph_cnt[k];q[2]=ph_calls[k];q[3]=ph_max[k];for(i=0;i<6;i++)q[4+i]=ph_h[k][i];q[10]=0;}
 for(k=0;k<4;k++)w[144+k]=vx_lock_result[k]; /* v10.432 */
 for(k=0;k<24;k++)w[148+k]=vx_d3d_geo[k]; /* v10.440: the geometry counters (vx_d3d_core.c) */
 w[172]=vx_d3d_pre_stat[0];w[173]=vx_d3d_pre_stat[1];w[174]=vx_d3d_prepack;w[175]=0; /* .524: PAIRLIST removed */ /* v10.441; v10.442: bit 1 ACCEL_D3D_PAIRLIST, the pairs kept in the list */
 w[176]=vv_fast_n;w[177]=vx_d3d_tr[0];w[178]=vx_d3d_tr[1]; /* .528: 177 texture runs added, 178 template changes with other state different (a new packet); .527: 176 vertices passed by the indexed path's one-test check (vv_good) */w[179]=vx_d3d_ix[0]; /* v10.470: 179 the triangles in indexed packets */ /* v10.449: 176-177 and bit 0 were ACCEL_D3D_QDIRECT's (removed) */ /* v10.443: batches built in the queue slot, bytes copied into the queue / 16 */
 for(k=0;k<20;k++)w[180+k]=vx_d3d_ft[k]; /* v10.444: the fast path's parts, sampled */
 for(k=0;k<12;k++)w[200+k]=vx_d3d_ft[20+k]; /* v10.453: the token loop around it (vx_d3d_ft[20..31]) */
 for(k=0;k<8;k++)w[212+k]=vx_d3d_tprof[k]; /* v10.462: a template build's parts */
 for(k=0;k<16;k++)w[220+k]=vx_d3d_tk[k]; /* v10.473: the triangles by draw command and path (vx_d3d_core.c) */
 {extern uint32_t vx_d3d_px[6],vx_d3d_ixpass;for(k=0;k<6;k++)w[236+k]=vx_d3d_px[k];w[242]=vx_d3d_rhw0_prims;w[243]=vx_d3d_ixpass;} /* .531: 242 the primitives drawn with rhw 1 (all their vertices rhw 0); .529: 242 (HWCULL) 0 */ /* .515: the pass-through's counts, ACCEL_D3D_HWCULL, ACCEL_D3D_IXPASS */
 {w[244]=vx_d3d_infocache;w[245]=vx_d3d_delta;w[246]=ic_stat[0];w[247]=ic_stat[1];w[248]=vx_d3d_dl[0]; /* .524: 245 ACCEL_D3D_DELTA, 248 packets marked (were IXPF's) */ w[249]=vx_d3d_v8;w[250]=vx_d3d_v8_n;if(reset){ic_stat[0]=ic_stat[1]=0;vx_d3d_v8_n=0;vx_d3d_dl[0]=vx_d3d_dl[1]=vx_d3d_dl[2]=0;}} /* .522 */
 w[251]=vx_d3d_dl[1];w[252]=vx_d3d_dl[2]; /* .524: ranges marked the same, packets with every range the same */
 for(k=0;k<6;k++){w[253+k]=pc_pf[k];w[259+k]=pc_pi[k];}for(k=0;k<5;k++)w[265+k]=pc_sl[k]; /* .525: page faults, page-ins by call time (the sampled calls); 265-269 the sampled calls of 10 ms+: count, ms, ms ran, faults, page-ins */
 {extern uint32_t vx_d3d_sse2,vx_d3d_vxs[4];w[302]=vx_d3d_sse2;for(k=0;k<4;k++)w[303+k]=vx_d3d_vxs[k];} /* .545: 302 SSE2 on, 303-306 the pass-through's span kernels */
 {extern uint32_t vx_d3d_off[4];for(k=0;k<4;k++)w[307+k]=vx_d3d_off[k];} /* .546: 307 off-screen triangles dropped early, 308 indexed-loop stops, 309 tokens proven inside at once, 310 not */ {extern uint32_t vx_d3d_vxs_skip;w[311]=vx_d3d_vxs_skip;} /* .547: spans not checked again inside a proven range */
 {extern uint32_t vx_d3d_vvq,vx_d3d_px_back,vx_d3d_tn_snap[2];w[312]=vx_d3d_vvq;w[313]=vx_d3d_px_back;w[314]=vx_d3d_tn_snap[0];w[315]=vx_d3d_tn_snap[1];} /* .548 */
 for(k=0;k<6;k++)w[316+k]=vx_d3d_hold_stat[k];for(k=0;k<8;k++)w[322+k]=hold_why[k];w[330]=vx_d3d_hold_on;w[331]=vx_d3d_held(); /* .550: 316-321 the held queue (vx_d3d_core.c), 322-329 the sends' causes (VX_D3D_hold_sync), 330 ACCEL_D3D_HOLD, 331 packets held now */
 {extern uint32_t vx_d3d_ep[4];for(k=0;k<4;k++)w[332+k]=vx_d3d_ep[k];} /* .552: 332-335 the answers kept for a state (vx_d3d_core.c, ep): composes done, used again; support answers worked out, used again. A caller that asks for 332 words gets the 332 it knows */
 {extern uint32_t vx_d3d_ixs[4],vx_d3d_fan[3];for(k=0;k<4;k++)w[336+k]=vx_d3d_ixs[k];for(k=0;k<3;k++)w[340+k]=vx_d3d_fan[k];{extern uint32_t vx_d3d_cp[6];w[354]=vx_d3d_cp[4];w[355]=vx_d3d_cp[5]; /* .568: 354-355 runs filtered whose span holds a failing vertex, runs a failing vertex ended */w[343]=0;w[350]=vx_d3d_cp[0];w[351]=vx_d3d_cp[1];w[352]=vx_d3d_cp[2];w[353]=vx_d3d_cp[3];}} /* .555: 336-339 indexed strips as lists (strips, their triangles, strips by the old path, triangles with an index twice left out), 340-342 fans (passed through, their triangles, by the loop), 343 the switch */
 {extern uint32_t vx_d3d_tokrun[2];w[344]=vx_d3d_tokrun[0];w[345]=vx_d3d_tokrun[1];} /* .556: 344-345 proven tokens taken as one run, their triangles */
 {extern uint32_t vx_d3d_ref[4];for(k=0;k<4;k++)w[346+k]=vx_d3d_ref[k];} /* .557: 346-349 runs drawn by the vertices they use, their triangles, vertices, runs ended by a failing vertex */
 for(k=0;k<7;k++)w[356+k]=vx_flip_seen[k]; /* .575: 356-361 the Flip callback's requests (vx_ddraw.c vx_flip_seen: VXFLIPF); .576: 362 refused */
 for(k=0;k<3;k++)w[363+k]=vx_init_seen[k]; /* .581: 363-365 DriverInit (vx_ddraw.c vx_init_seen: VXREG) */
 if(n>366)n=366;for(k=0;k<n;k++)out[k]=w[k]; /* .534: 270-289 the T&L probe; .536: 290-301 the driver's T&L */ /* .533: 270-273 (.531's kept answers) removed with them */
 if(stop){ph_on=0;vx_d3d_geo_on=0;vx_d3d_ftprof=0;}else if(reset){ph_on=1;vx_d3d_geo_on=1;vx_d3d_ftprof=1;vv_fast_n=0;for(k=0;k<32;k++)vx_d3d_ft[k]=0;for(k=0;k<8;k++)vx_d3d_tprof[k]=0;for(k=0;k<24;k++)vx_d3d_geo[k]=0;for(k=0;k<16;k++)vx_d3d_tk[k]=0;{extern uint32_t vx_d3d_px[6];for(k=0;k<6;k++)vx_d3d_px[k]=0;}vx_d3d_pre_stat[0]=vx_d3d_pre_stat[1]=0;
  for(k=0;k<VX_PH_N;k++){ph_sum[k]=ph_cnt[k]=ph_max[k]=ph_calls[k]=0;for(i=0;i<6;i++)ph_h[k][i]=0;}
  for(k=0;k<6;k++)pc_h[k]=pc_wall[k]=pc_run[k]=pc_runok[k]=pc_pf[k]=pc_pi[k]=0;for(k=0;k<5;k++)pc_sl[k]=0;pc_n=pv_n=pv_in=pv_run=0;for(k=0;k<4;k++)vx_d3d_bad_why[k]=0;vx_d3d_rhw0_prims=0;}
 return n;
}
static void busy_wait(uint32_t tries) {
 if(tries==1){uint32_t a=0,r=0;vx_d3d_busy_waits++;(void)call(VX_IOCTL_2D_ACCESS,&a,4,&r,4);return;}
 if(sl_on&&tries>=8)sl_sleeps++;
 Sleep(tries<8?0:1);
}
static void sl_busy_why(void) {if(sl_on&&!sl_why){vx3dn_info i;memset(&i,0,sizeof(i));if(call(VX_IOCTL_3DN_INFO,0,0,&i,sizeof(i)))sl_why=i.last_result|0x80000000UL;}}
static uint32_t hal_clock(void) {return (uint32_t)VX_hal_tsc();}
DWORD WINAPI VX_D3D_QuerySlowOps(DWORD *out,DWORD n) { /* v10.390: [0..47] the 3 longest DrawPrimitives2 opcodes of each call VX_D3D_QuerySlow keeps, in its order */
 DWORD k,j;if(!out||(n&0x7FFFFFFFUL)<48)return 0;for(k=0;k<8;k++)for(j=0;j<6;j++)out[k*6+j]=sl_ops[k][j];
 if(n&0x80000000UL)for(k=0;k<8;k++)for(j=0;j<6;j++)sl_ops[k][j]=0;
 return 48;
}
DWORD WINAPI VX_D3D_QuerySlow(DWORD *out,DWORD n) { /* [0] calls of 10 ms and more, [1] their time, [2..65] the 8 longest: time, cb_enter wait, VxD calls'
 * time, VxD calls, busy retries, their time, Sleep(1)s, why busy (bit 31 set when known); n | 0x80000000 clears */
 DWORD k,j;if(!out||(n&0x7FFFFFFFUL)<66)return 0;
 out[0]=sl_count;out[1]=sl_total;for(k=0;k<8;k++)for(j=0;j<8;j++)out[2+k*8+j]=sl_worst[k][j];
 if(n&0x80000000UL){sl_count=0;sl_total=0;for(k=0;k<8;k++)for(j=0;j<8;j++)sl_worst[k][j]=0;for(k=0;k<8;k++)for(j=0;j<6;j++)sl_ops[k][j]=0;for(k=0;k<8;k++)for(j=0;j<8;j++)sl_iow[k][j]=0;}
 return 66;
}
static uint32_t native_submit(void *ctx,const vx3dn_packet *p) {
 (void)ctx;tr_packets(p,1); /* v10.420 */if(draw_capture||event_log_file)draw_record(p); /* v10.217; v10.321: only while capture is on */
 return submit_one(p);
}
static uint32_t submit_one(const vx3dn_packet *p) {
 uint32_t rc=VX3DN_LOST,ok,tries=0,start=0,why0=0;
 for(;;) {
  {uint32_t t0=HPT(),d,s0=VX_hal_tsc(),o5[5]={VX3DN_LOST,0,0,0,0};
   {uint32_t ph=VX_PH(VX_PH_VXD);ok=call(VX_IOCTL_3DN_SUBMIT,p,sizeof(*p),o5,20);VX_PH_BACK(ph);ph_vxd(o5+1);}rc=o5[0];
   if(sl_on){uint32_t s1=VX_hal_tsc();sl_ioctl+=(s1-s0)>>8;sl_calls++;sl_io_add(s0,s1,o5+1);}
   d=HPT()-t0;hal_prof[2]++;hal_prof[3]+=(d+128)>>8;HLAT(&hal_lat[1],d);}
  if(!ok||rc!=VX3DN_BUSY||busy_stuck)break;
  if(sl_on)sl_tries++;
  {uint32_t ph=VX_PH(VX_PH_WAIT);
  if(!tries){vx3dn_info i;memset(&i,0,sizeof(i));(void)call(VX_IOCTL_3DN_INFO,0,0,&i,sizeof(i));why0=i.last_result;start=GetTickCount();if(sl_on&&!sl_why)sl_why=why0|0x80000000UL;}
  if(GetTickCount()-start>500||tries>=600){busy_stuck=1;VX_PH_BACK(ph);break;}
  tries++;busy_wait(tries);VX_PH_BACK(ph);}
 }
 if(ok&&(!rc||rc==VX3DN_RECOVERED))busy_stuck=0; /* v10.221 (review): a recovered draw reached the engine too */
 if(!ok||(rc&&rc!=VX3DN_RECOVERED&&rc!=VX3DN_BUSY))ic_drop(); /* .522 */
 if(hal_prof_on&&ok&&!rc&&p->op==VX3DN_DRAW)hal_prof[4]+=packet_triangles(p);
 if(tries)HLAT(&hal_lat[2],tries); /* v10.321 */
 if(tries) {
  if(busy_pid!=GetCurrentProcessId()){busy_pid=GetCurrentProcessId();busy_logged=0;}
  if(busy_logged++<4)event(12,0,tries,GetTickCount()-start,ok?rc:0xFFFFFFFFUL,why0);
 }
 if(ok&&!rc){diagnostic.native_packets++;fog_event(p);return 0;}
 if(!ok)rc=VX3DN_LOST|0x80; /* the DeviceIoControl itself failed */
 if(refusal_pid!=GetCurrentProcessId()){refusal_pid=GetCurrentProcessId();refusals=0;}
 if(refusals++<4) {
  vx3dn_info i;memset(&i,0,sizeof(i));(void)call(VX_IOCTL_3DN_INFO,0,0,&i,sizeof(i));
  event(7,rc,rc|i.fault<<8|i.last_result<<16,p->op<<24|(p->tex.enable?1UL:0)<<20|(p->tex.surface.format&255)<<12|(p->depth.format&255)<<4|(p->target.format&15),
   p->tex.surface.width<<16|(p->tex.surface.height&0xFFFF),i.last_status);
  refusal_events(p,rc); /* .527 */
 }
 return rc&0x7F;
}
/* v10.276: a DrawPrimitives2 call's queued draw packets in one VxD call (VX_IOCTL_3DN_SUBMITN); busy is tried again as for one packet. A VxD
 * without the call (the DeviceIoControl fails): the packets one at a time. */
static int submitn_missing,submitn_worked,submitn_fails;
static uint32_t native_submit_n(void *ctx,const vx3dn_packet *p,uint32_t n,uint32_t *done) {
 uint32_t out[6],ok,tries=0,start=0,k,rc;
 *done=0;
 if(!submitn_missing) {
  if(draw_capture||event_log_file)for(k=0;k<n;k++)draw_record(p+k); /* v10.321: only while capture is on */
  tr_packets(p,n); /* v10.420 */
  for(;;) {
   {uint32_t t0=HPT(),d,s0=VX_hal_tsc();out[0]=VX3DN_LOST;out[1]=0;out[2]=out[3]=0;
    {uint32_t ph=VX_PH(VX_PH_VXD);ok=call(VX_IOCTL_3DN_SUBMITN,p,n*sizeof(*p),out,24);VX_PH_BACK(ph);ph_vxd(out+2);}
    if(sl_on){uint32_t s1=VX_hal_tsc();sl_ioctl+=(s1-s0)>>8;sl_calls++;sl_io_add(s0,s1,out+2);}
    d=HPT()-t0;hal_prof[2]++;hal_prof[3]+=(d+128)>>8;HLAT(&hal_lat[1],d);}
   if(!ok)break;
   if(out[0]!=VX3DN_BUSY||out[1]||busy_stuck)break;
   if(sl_on){sl_tries++;sl_busy_why();}
   {uint32_t ph=VX_PH(VX_PH_WAIT);
   if(!tries)start=GetTickCount();
   if(GetTickCount()-start>500||tries>=600){busy_stuck=1;VX_PH_BACK(ph);break;}
   tries++;busy_wait(tries);VX_PH_BACK(ph);}
  }
  if(tries)HLAT(&hal_lat[2],tries); /* v10.321 */
  if(ok) {
   submitn_worked=1;
   *done=out[1];if(*done>n)*done=n;
   for(k=0;k<*done;k++){diagnostic.native_packets++;if(hal_prof_on)hal_prof[4]+=packet_triangles(p+k);}
   if(!out[0]||out[0]==VX3DN_RECOVERED)busy_stuck=0;
   if(out[0]&&out[0]!=VX3DN_RECOVERED&&out[0]!=VX3DN_BUSY)ic_drop(); /* .522 */
   if(out[0]&&out[0]!=VX3DN_RECOVERED&&out[0]!=VX3DN_BUSY&&*done<n){
    if(refusal_pid!=GetCurrentProcessId()){refusal_pid=GetCurrentProcessId();refusals=0;}
    if(refusals++<4){event(7,out[0],out[0]|*done<<8,p[*done].op<<24|(p[*done].tex.enable?1UL:0)<<20|(p[*done].tex.surface.format&255)<<12|(p[*done].depth.format&255)<<4|(p[*done].target.format&15),n,0);
     if(out[0]==VX3DN_BAD_PACKET)refusal_events(p+*done,out[0]);}} /* .527: the packet and the VxD's reason on the batch path too */
   return out[0];
  }
  /* v10.367 (audit): a failed call means a VxD without it only when it never worked in this DLL and failed three times; once it has worked a
   * failure (a stale handle, a transient error) sends these packets one at a time and tries the batch again next time */
  if(!submitn_worked&&++submitn_fails>=3)submitn_missing=1;
  for(k=0;k<n;k++){rc=submit_one(p+k);if(rc&&rc!=VX3DN_RECOVERED)return rc;(*done)++;if(rc)return rc;} /* v10.321: recorded above, not twice */
  return VX3DN_OK;
 }
 for(k=0;k<n;k++){rc=native_submit(ctx,p+k);if(rc&&rc!=VX3DN_RECOVERED)return rc;(*done)++;if(rc)return rc;}
 return VX3DN_OK;
}
/* .550: the packets the core holds across DrawPrimitives2 calls (vx_d3d_core.c, cq_hold) are sent before a callback that reads, writes or
 * moves what they draw. hold_pid the process that queued them (only its handle can send them; the DLL's data is shared), hold_tid the
 * thread inside a DrawPrimitives2 call (a nested callback: the core sends its queue itself before such a command). The causes counted in
 * hold_why: [0] Lock, [1] Blt, [2] Flip, [3] DestroySurface, [4] SetEntries, [5] the context destroyed, [6] another process's packets dropped
 * (that process gone), [7] waits for the callback lock that timed out */
static uint32_t hold_noop(void *ctx,const vx3dn_packet *p) {(void)ctx;(void)p;return VX3DN_LOST;}
static uint32_t hold_send_here(void) {vx_d3d_services s;uint32_t fp[27],rc;memset(&s,0,sizeof(s));s.submit=hold_noop;s.submit_n=native_submit_n;
 save_fp(fp);rc=vx_d3d_hold_send(&s);restore_fp(fp);return rc;}
/* .575 (peer review of .572, finding 4): returns 1 when the callback lock could not be had in time and packets are still held: the
 * caller's Lock, Blt or Flip (why 0..2) then answers DDERR_WASSTILLDRAWING and touches nothing (it went on, and read, overwrote or showed
 * a surface the held draws had not reached: the runtime waits and asks again, or tells the application). A call that cannot be refused
 * (DestroySurface, SetEntries, a context going) waits 2 s for the lock before it goes on as it did. */
int VX_D3D_hold_sync(DWORD why) {
 DWORD me,t0;const DWORD lim=why<=2?100UL:2000UL;
 if(!vx_d3d_held())return 0;
 me=GetCurrentProcessId();if(hold_pid!=me||hold_tid==GetCurrentThreadId())return 0;
 for(t0=GetTickCount();!cb_enter();){if(GetTickCount()-t0>lim){hold_why[7]++;return vx_d3d_held()&&hold_pid==me;}Sleep(0);} /* another thread of this process in a callback */
 if(vx_d3d_held()&&hold_pid==me){if(why<8)hold_why[why]++;vx_d3d_hold_stat[4]++;(void)hold_send_here();}
 cb_leave();
 return 0;
}
/* v10.166: a flip moves surface memory between the surfaces of a chain (DirectDraw swaps their video memory), so a cached offset would draw into
 * what is now the front buffer. Re-read the local object's memory at each use; size, pitch and format must not have changed. */
/* v10.182: why a context was marked lost (HAL diagnostics lost_why / lost_value; VXDX7 prints them). Low byte: 1 VxD not ready, 2 mode
 * generation changed, 3 already lost; follow(): 0x10 no object, 0x11 object INVALID flag, 0x12 memory below the frame buffer, 0x13 not
 * 256-byte aligned, 0x14 beyond the usable frame buffer; bit 8 = it was the depth surface. lost_value = the offset (or fpVidMem). */
static int follow(uint32_t key,vx3dn_surface *d) {
 /* v10.167: only the memory address is read. After a flip the back buffer holds the primary's memory description, whose size fields
  * DirectDraw leaves empty (VXDX7 --flip, v10.166: every second frame was drawn into the buffer just shown); size, pitch and format are
  * the chain's and stay as bound. */
 DDRAWI_DDRAWSURFACE_LCL *s=(DDRAWI_DDRAWSURFACE_LCL *)key;uint32_t off,limit;
 if(!d->format)return 1;
 if(!key||!shared_hal||!s->lpGbl){note_lost(0x10,key);return 0;}
 if(s->dwFlags&DDRAWISURF_INVALID){note_lost(0x11,s->lpGbl->fpVidMem);return 0;}
 if(s->lpGbl->fpVidMem<shared_hal->vramLinear){note_lost(0x12,s->lpGbl->fpVidMem);return 0;}
 off=s->lpGbl->fpVidMem-shared_hal->vramLinear;limit=shared_hal->vramSize;if(limit>VX_FB_LIMIT)limit=VX_FB_LIMIT;
 if(off&255){note_lost(0x13,off);return 0;}
 if(off>=limit||!d->pitch||d->height>(limit-off)/d->pitch){note_lost(0x14,off);return 0;}
 d->offset=off;return 1;
}
/* v10.168: a CreateSurfaceEx handle names memory, not a DirectDraw object: on a flip DirectDraw 7 swaps the chain's handles together with their
 * memory and the runtime then sends D3DDP2OP_SETRENDERTARGET with the back buffer's new handle (VXDX7 --flip, v10.166/167: following the
 * handle's object drew every second frame into the buffer being shown). Only targets bound by object (ContextCreate, no handle yet) follow it. */
static void follow_targets(context_slot *c) {
 if(c->core.lost)return;
 if(!c->core.target_handle&&!follow(c->target_key,&c->core.target)){c->core.lost=1;return;}
 if(!c->core.depth_handle&&!follow(c->depth_key,&c->core.depth)){lost_why|=0x100;c->core.lost=1;}
}
/* v10.185: a texture is re-read from its surface object at each use. Its memory can arrive after CreateSurfaceEx (DDSCAPS_ALLOCONLOAD, the
 * DirectX 3/5 IDirect3DTexture::Load path: the DirectX 5 SDK's Tunnel) and the recorded description was then empty or stale. */
static int resolve(void *ctx,uint32_t h,vx3dn_surface *out) {
 context_slot *c=ctx;surface_slot *s=surface_find(table_find(c->owner,c->local,0),h);vx3dn_surface d;
 if(!s)return 0;
 if(!s->desc.format||VX3DN_IS_TEX(s->desc.format)) { /* v10.215: every texture format (L8, DXT and bump were kept from their first use) */
  DDRAWI_DDRAWSURFACE_LCL *l=(DDRAWI_DDRAWSURFACE_LCL *)s->native_key;
  if(!l||IsBadReadPtr(l,sizeof(*l))||!l->lpGbl||IsBadReadPtr(l->lpGbl,sizeof(*l->lpGbl))||!VX_D3D_surface(l,&d))return 0;
  s->desc=d;
 }
 *out=s->desc;return 1;
}
static void sync_target_keys(context_slot *c) {
 surface_table *t=table_find(c->owner,c->local,0);surface_slot *s;
 if(!c->core.target_handle)return;
 s=surface_find(t,c->core.target_handle);c->target_key=s?s->native_key:0;
 s=surface_find(t,c->core.depth_handle);c->depth_key=s?s->native_key:0;
}
static uint32_t unknown(void *ctx,const uint8_t *at,const uint8_t **next) {
 parse_fn f=parse_unknown_get();uint32_t r;(void)ctx;if(!f)return VX_D3D_UNPARSED;
 r=(uint32_t)f((LPVOID)at,(LPVOID*)next);
 return r;
}
static uint32_t surface_bytes(DDRAWI_DDRAWSURFACE_LCL *s) {
 uint32_t pitch,h;if(!s||!s->lpGbl)return 0;
 /* Execute buffers use dwLinearSize; ordinary surfaces use pitch*height.
  * Reject overflow before validating command/vertex subranges. */
 if(s->ddsCaps.dwCaps&DDSCAPS_EXECUTEBUFFER)return s->lpGbl->dwLinearSize;
 if(s->lpGbl->lPitch<=0)return 0;pitch=(uint32_t)s->lpGbl->lPitch;h=s->lpGbl->wHeight;
 if(h>0xFFFFFFFFUL/pitch)return 0;return h*pitch;
}
/* v10.153: the runtime passes DirectDraw LOCAL objects to ContextCreate only for a DirectX 7 driver (one that offered CreateSurfaceEx);
 * an older driver gets the DirectDraw global object and IDirectDrawSurface interface pointers (DDRAWI_DDRAWSURFACE_INT). */
static volatile DWORD dx7_handles;
/* v10.155: which form arrives is checked per pointer (the board's execute/TL buffers failed as interfaces): a local object's lpSurfMore
 * starts with a small dwSize, an interface's first word is its vtable, whose first entry is a code address. */
static int is_lcl(const DDRAWI_DDRAWSURFACE_LCL *l) {
 const DDRAWI_DDRAWSURFACE_MORE *m;
 if(!l||IsBadReadPtr(l,sizeof(*l))||!l->lpGbl||IsBadReadPtr(l->lpGbl,sizeof(*l->lpGbl)))return 0;
 m=l->lpSurfMore;
 return !m||(!IsBadReadPtr(m,4)&&m->dwSize>=16&&m->dwSize<=1024); /* an interface's vtable is never NULL */
}
static DDRAWI_DDRAWSURFACE_LCL *surface_lcl(void *p) {
 LPDDRAWI_DDRAWSURFACE_INT i=(LPDDRAWI_DDRAWSURFACE_INT)p;
 if(!p)return 0;
 if(is_lcl((DDRAWI_DDRAWSURFACE_LCL *)p))return (DDRAWI_DDRAWSURFACE_LCL *)p;
 if(!IsBadReadPtr(i,sizeof(*i))&&is_lcl(i->lpLcl))return i->lpLcl;
 return dx7_handles?(DDRAWI_DDRAWSURFACE_LCL *)p:0;
}
/* v10.185: every ContextCreate goes to the event log: reason 0 created, 1 no DirectDraw local object, 2 render target not usable (detail c
 * bits 15:8 = VX_D3D_surface's reason), 3 render target is a depth surface, 4 Z buffer not usable, 5 Z buffer not a depth format, 6 VxD not
 * ready, 7 target/Z pair invalid, 8 target outside the native pool, 9 Z outside it, 10 no free context. a = target caps, b = target size. */
static DWORD create_impl(vx_ddi_create *p) {
 vx3dn_surface t,z;vx3dn_info info={0};uint32_t j,why=0,sw=0;int rtk=0;context_slot *s;DDRAWI_DDRAWSURFACE_LCL *tl=surface_lcl(p->lpDDSLcl),*zl=surface_lcl(p->lpDDSZLcl);
 p->dwhContext=0;p->ddrval=DDERR_INVALIDPARAMS;
 memset(&z,0,sizeof(z));memset(&t,0,sizeof(t));
 if(!p->lpDDLcl)why=1;
 else if(!(rtk=VX_D3D_surface_target(tl,&t))){why=2;sw=vx_surface_why;} /* v10.456: a render-target texture as its target format */
 else if(VX3DN_IS_DEPTH(t.format))why=3;
 else if(p->lpDDSZLcl&&!VX_D3D_surface(zl,&z)){why=4;sw=vx_surface_why;}
 else if(p->lpDDSZLcl&&!VX3DN_IS_DEPTH(z.format))why=5;
 else if(!call(VX_IOCTL_3DN_INFO,0,0,&info,sizeof(info))||!info.ready){why=6;p->ddrval=DDERR_SURFACELOST;}
 else if(!vx_d3d_targets_valid(&t,&z))why=7;
 else if(!native_surface(&t,&info))why=8;
 else if(z.format&&!native_surface(&z,&info))why=9;
 if(!why) {
  for(j=0;j<CONTEXTS;j++)if(!contexts[j].handle)break;
  if(j==CONTEXTS){reclaim_dead();for(j=0;j<CONTEXTS;j++)if(!contexts[j].handle)break;} /* v10.183 */
  if(j==CONTEXTS){why=10;p->ddrval=0x201;}
 }
 event(1,why?(DWORD)p->ddrval:0,why,tl?tl->ddsCaps.dwCaps:0,(tl&&tl->lpGbl)?((DWORD)tl->lpGbl->wWidth<<16|tl->lpGbl->wHeight):0,t.format|sw<<8|z.format<<16);
 if(why)return DDHAL_DRIVER_HANDLED;
 s=&contexts[j];memset(s,0,sizeof(*s));s->owner=GetCurrentProcessId();s->local=(uint32_t)p->lpDDLcl;
 s->target_key=(uint32_t)tl;s->depth_key=(uint32_t)zl;s->rt_tex=rtk==2;
 if(++serial==0)serial=1;s->handle=(serial<<4)|(j+1);
 if(!s->handle)s->handle=j+1;
 vx_d3d_init(&s->core,&t,z.format?&z:0,info.generation);
 p->dwhContext=s->handle;p->ddrval=DD_OK;return DDHAL_DRIVER_HANDLED;
}
DWORD WINAPI VX_D3D_Create(vx_ddi_create *p) {
 DWORD rc;uint32_t fp[27];if(!p)return DDHAL_DRIVER_NOTHANDLED;
 if(!cb_enter()){p->dwhContext=0;p->ddrval=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 save_fp(fp);rc=create_impl(p);restore_fp(fp);diagnostic.creates++;record(1,p->ddrval);cb_leave();
 {extern void VX_DD_lock_image(void);VX_DD_lock_image();} /* v10.432 */
 return rc;
}
DWORD WINAPI VX_D3D_Destroy(vx_ddi_destroy *p) {
 context_slot *s;if(!p)return DDHAL_DRIVER_NOTHANDLED;
 if(!cb_enter()){p->ddrval=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 s=find(p->dwhContext);if(s&&vx_d3d_hold_is(&s->core)){hold_why[5]++;if(hold_pid==GetCurrentProcessId())(void)hold_send_here();else vx_d3d_hold_drop();} /* .550 */
 if(s){memset(s,0,sizeof(*s));p->ddrval=DD_OK;}else p->ddrval=VX_D3D_CONTEXT_BAD;
 cb_leave();return DDHAL_DRIVER_HANDLED;
}
DWORD WINAPI VX_D3D_DestroyAll(vx_ddi_destroy_all *p) {
 uint32_t j;if(!p)return DDHAL_DRIVER_NOTHANDLED;
 if(!cb_enter()){p->ddrval=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 for(j=0;j<CONTEXTS;j++)if(contexts[j].owner==p->dwPID){if(vx_d3d_hold_is(&contexts[j].core)){hold_why[5]++;if(hold_pid==GetCurrentProcessId())(void)hold_send_here();else vx_d3d_hold_drop();} /* .550 */
  memset(&contexts[j],0,sizeof(contexts[j]));}
 for(j=0;j<TABLES;j++)if(tables[j].local&&tables[j].owner==p->dwPID)table_release(&tables[j]);
 p->ddrval=DD_OK;cb_leave();return DDHAL_DRIVER_HANDLED;
}
typedef struct dp2_source {context_slot *slot;const vx_ddi_dp2 *args;const vx3dn_info *info;uint32_t stride,rt_kind,rt_lost;} dp2_source; /* v10.479: rt_kind = the colour surface dp2_target last resolved is a texture; rt_lost = its wait reported the device lost */
/* v10.199: a texture's mip chain: level 0 and each attached DDSCAPS_MIPMAP level, read from the surface objects at each use; each level
 * must be the same format at half the size. Returns the number of levels with each level's frame-buffer offset in off[]. */
static uint32_t dp2_mips(void *ctx,uint32_t h,uint32_t *off,uint32_t max) {
 dp2_source *src=ctx;surface_slot *s=surface_find(table_find(src->slot->owner,src->slot->local,0),h);DDRAWI_DDRAWSURFACE_LCL *l;vx3dn_surface d0,d;uint32_t n=0;
 if(!s||!max)return 0;l=(DDRAWI_DDRAWSURFACE_LCL *)s->native_key;
 if(!l||IsBadReadPtr(l,sizeof(*l))||!l->lpGbl||IsBadReadPtr(l->lpGbl,sizeof(*l->lpGbl))||!VX_D3D_surface(l,&d0))return 0;
 off[n++]=d0.offset;d=d0;
 while(n<max) {
  LPATTACHLIST a=l->lpAttachList;DDRAWI_DDRAWSURFACE_LCL *next=0;vx3dn_surface e;uint32_t w=d.width>1?d.width/2:1,hh=d.height>1?d.height/2:1;
  {int guard=0;while(a&&!IsBadReadPtr(a,sizeof(*a))&&guard++<64){if(a->lpAttached&&!IsBadReadPtr(a->lpAttached,sizeof(*a->lpAttached))&&(a->lpAttached->ddsCaps.dwCaps&DDSCAPS_MIPMAP)){next=a->lpAttached;break;}a=a->lpLink;}} /* v10.367 (audit): bounded as dp2_faces' walk */
  if(!next||!next->lpGbl||IsBadReadPtr(next->lpGbl,sizeof(*next->lpGbl))||!VX_D3D_surface(next,&e)||e.format!=d0.format||e.width!=w||e.height!=hh)break;
  off[n++]=e.offset;d=e;l=next;
 }
 return n;
}
/* v10.214: a cube map's faces (T150): the root (+X) and the other five faces attached to it, each the root's format and size; mip
 * sublevels are skipped. Returns 6 with the offsets in +X -X +Y -Y +Z -Z order, 0 for a texture that is not a cube map, 0xFFFFFFFF for a
 * cube map whose faces cannot be sampled together. */
static uint32_t dp2_faces(void *ctx,uint32_t h,uint32_t *off) {
 dp2_source *src=ctx;surface_slot *s=surface_find(table_find(src->slot->owner,src->slot->local,0),h);DDRAWI_DDRAWSURFACE_LCL *l,*f[6];vx3dn_surface d0,e;uint32_t k,found=0;
 if(!s)return 0;l=(DDRAWI_DDRAWSURFACE_LCL *)s->native_key;
 if(!l||IsBadReadPtr(l,sizeof(*l))||!l->lpSurfMore||IsBadReadPtr(l->lpSurfMore,sizeof(*l->lpSurfMore))||!(l->lpSurfMore->ddsCapsEx.dwCaps2&DDSCAPS2_CUBEMAP))return 0;
 if(!l->lpGbl||IsBadReadPtr(l->lpGbl,sizeof(*l->lpGbl))||!VX_D3D_surface(l,&d0))return 0xFFFFFFFFUL;
 for(k=0;k<6;k++)f[k]=0;
 {DDRAWI_DDRAWSURFACE_LCL *c=l;LPATTACHLIST a=l->lpAttachList;uint32_t guard=0;
  for(;;) {
   if(c&&c->lpSurfMore&&!IsBadReadPtr(c->lpSurfMore,sizeof(*c->lpSurfMore))) {
    DWORD c2=c->lpSurfMore->ddsCapsEx.dwCaps2;
    if((c2&DDSCAPS2_CUBEMAP)&&!(c2&DDSCAPS2_MIPMAPSUBLEVEL))for(k=0;k<6;k++)if((c2&(0x400UL<<k))&&!f[k]){f[k]=c;found++;}
   }
   if(!a||IsBadReadPtr(a,sizeof(*a))||++guard>64)break;
   c=a->lpAttached&&!IsBadReadPtr(a->lpAttached,sizeof(*a->lpAttached))?a->lpAttached:0;a=a->lpLink;
  }}
 if(found!=6)return 0xFFFFFFFFUL;
 for(k=0;k<6;k++){if(!f[k]->lpGbl||IsBadReadPtr(f[k]->lpGbl,sizeof(*f[k]->lpGbl))||!VX_D3D_surface(f[k],&e)||e.format!=d0.format||e.width!=d0.width||e.height!=d0.height||e.pitch!=d0.pitch)return 0xFFFFFFFFUL;off[k]=e.offset;}
 return 6;
}
/* v10.197: a texture's source colour key (SetColorKey DDCKEY_SRCBLT), read from its surface object at each use */
static int dp2_colorkey(void *ctx,uint32_t h,uint32_t *key) {
 dp2_source *src=ctx;surface_slot *s=surface_find(table_find(src->slot->owner,src->slot->local,0),h);DDRAWI_DDRAWSURFACE_LCL *l;
 if(!s)return 0;l=(DDRAWI_DDRAWSURFACE_LCL *)s->native_key;
 if(!l||IsBadReadPtr(l,sizeof(*l))||!(l->dwFlags&DDRAWISURF_HASCKEYSRCBLT))return 0;
 *key=l->ddckCKSrcBlt.dwColorSpaceLowValue;return 1;
}
static int dp2_resolve(void *ctx,uint32_t h,vx3dn_surface *out) {
 dp2_source *s=ctx;
 return resolve(s->slot,h,out)&&native_surface(out,s->info);
}
/* v10.456: DP2 SETRENDERTARGET's colour surface (the core has sent every draw before it): a render-target texture in its target format;
 * a texture that was the target is finished first (rt_leave) */
static int dp2_target(void *ctx,uint32_t h,vx3dn_surface *out) {
 dp2_source *src=ctx;context_slot *c=src->slot;surface_slot *s=surface_find(table_find(c->owner,c->local,0),h);DDRAWI_DDRAWSURFACE_LCL *l;int k;
 src->rt_kind=0;
 if(!rt_leave(c,&c->core.target)){src->rt_lost=1;return 0;} /* v10.479: the wait reported the device lost: the call answers SURFACELOST */
 if(!s)return 0;l=(DDRAWI_DDRAWSURFACE_LCL *)s->native_key;
 if(!l||IsBadReadPtr(l,sizeof(*l))||!l->lpGbl||IsBadReadPtr(l->lpGbl,sizeof(*l->lpGbl))||!(l->ddsCaps.dwCaps&DDSCAPS_TEXTURE))return dp2_resolve(ctx,h,out);
 k=VX_D3D_surface_target(l,out);if(k!=2||!native_surface(out,src->info))return 0;
 src->rt_kind=1;return 1;
}
/* v10.479: the core made the change dp2_target resolved: only now does the texture mark follow it */
static void dp2_target_done(void *ctx) {dp2_source *src=ctx;src->slot->rt_tex=src->rt_kind;}
static uint32_t bind_vertices(void *ctx,vx_d3d_input *in) {
 dp2_source *source=ctx;const vx_ddi_dp2 *p=source->args;
 uint32_t stride=source->stride,bytes,base,avail=0;
 /* .538: with ACCEL_D3D_TNL a vertex buffer is bound whole below, so its dwVertexLength may be 0 (not checked against it) */
 if(!stride||(!p->dwVertexLength&&!(vx_d3d_tnl&&!(p->dwFlags&1)))||p->dwVertexLength>0xFFFFFFFFUL/stride)return VX_D3D_INVALID;
 bytes=p->dwVertexLength*stride;
 if(p->dwFlags&1)base=(uint32_t)p->lpVertices;
 else {
  avail=surface_bytes(p->lpDDVertex);
  if(!p->lpDDVertex||!p->lpDDVertex->lpGbl||p->dwVertexOffset>avail||bytes>avail-p->dwVertexOffset)return VX_D3D_INVALID;
  base=(uint32_t)p->lpDDVertex->lpGbl->fpVidMem;
 }
 if(!base||base>0xFFFFFFFFUL-p->dwVertexOffset||base+p->dwVertexOffset>0xFFFFFFFFUL-bytes)return VX_D3D_INVALID;
 /* .538: every vertex format, not only untransformed (board, GTA3 on .537: DrawPrimitives2 answered DDERR_INVALIDPARAMS, 80070057,
  * through gameplay; Direct3D 8's hardware vertex processing can index past dwVertexLength for pretransformed vertices too; VXTLP 4c).
  * .536 (ACCEL_D3D_TNL): untransformed vertices from a vertex buffer: the whole buffer past the offset. Direct3D 8's hardware vertex
  * processing on this DirectX 7 DDI indexes past dwVertexLength (GTA3: draws from vertex dwVertexLength on, appended to the buffer after
  * the length was set); the buffer's own size bounds every index */
 if(vx_d3d_tnl&&!(p->dwFlags&1)){uint32_t all=(avail-p->dwVertexOffset)/stride;if(!all)return VX_D3D_INVALID;if(all>=p->dwVertexLength){bytes=all*stride;if(base+p->dwVertexOffset>0xFFFFFFFFUL-bytes)return VX_D3D_INVALID; /* .540 (review): the end checked for the whole span */
  in->vertices=(const uint8_t*)(base+p->dwVertexOffset);in->vertex_bytes=bytes;in->vertex_count=all;in->vertex_stride=stride;in->fvf=p->dwVertexType;return 0;}}
 in->vertices=(const uint8_t*)(base+p->dwVertexOffset);in->vertex_bytes=bytes;
 in->vertex_count=p->dwVertexLength;in->vertex_stride=stride;in->fvf=p->dwVertexType;
 return 0;
}
/* v10.428 (board, VXTCBENCH 1.10 with the v10.427 trace: after every check (Lock A, Lock B while A is held) the runtime's state array said
 * ZWRITEENABLE 1 while no DrawPrimitives2 call had carried it: the SetRenderState that followed the objects' EndScene was recorded and
 * never sent; the next frame's background went out with Z writes off). At the end of a successful call every command the runtime has
 * recorded has been parsed, so the runtime's array (lpdwRStates) and the context agree, except for a command that never came. The
 * pixel-pipeline states below are taken from the array when they disagree and the array's value changed since the end of the last call
 * (a runtime that does not keep the array changes nothing then). Not for execute buffers (the HAL keeps the array itself then).
 * ACCEL_D3D_RSSYNC (default 1); cb 99 in C:\VXD3D.LOG (ACCEL_D3D_LOG): the state, the context's value, the array's. */
static const unsigned char rs_sync_ids[]={7,8,9,14,15,19,20,22,23,24,25,26,27,29,52,53,54,55,56,57,58,59,60};
static void rs_resync(context_slot *c,const vx_ddi_dp2 *p) {
 const DWORD *r=(const DWORD*)p->lpdwRStates;unsigned k;
 if(!(vx_d3d_feat&VX_F_RSSYNC)||p->ddrval!=DD_OK||(p->dwFlags&2)||!r||IsBadReadPtr(r,64*4)||c->core.recording)return;
 for(k=0;k<sizeof(rs_sync_ids);k++){unsigned id=rs_sync_ids[k];DWORD v=r[id];
  if(c->rt_ok&&v!=c->rt_prev[id]&&c->core.state.rs[id]!=v){VX_D3D_trace(99,0,id,c->core.state.rs[id],v,0);vx_d3d_state_adopt(&c->core,id,v);}
  c->rt_prev[id]=v;}
 c->rt_ok=1;
}
#include "vx_d3d_rec.inc" /* .517: the DrawPrimitives2 recorder */
static DWORD dp2_impl(vx_ddi_dp2 *p,uint32_t stride) {
 context_slot *c=find(p->dwhContext);vx_d3d_input in;vx_d3d_services sv;vx3dn_info info={0};dp2_source source;
 uint32_t command_size,error=0;
 p->dwErrorOffset=p->dwCommandOffset;p->ddrval=DDERR_INVALIDPARAMS;
 if(!c){p->ddrval=VX_D3D_CONTEXT_BAD;return DDHAL_DRIVER_HANDLED;}
 if(vx_d3d_infocache&&ic_ok&&ic_info.generation==c->core.generation&&GetTickCount()-ic_t<100UL){info=ic_info;ic_stat[0]++;} /* .522 */
 else{uint32_t ph=VX_PH(VX_PH_VXD),ok=call(VX_IOCTL_3DN_INFO,0,0,&info,sizeof(info));VX_PH_BACK(ph);if(!ok)info.ready=0; /* v10.431: a VxD call, timed as one */
  ic_stat[1]++;ic_info=info;ic_t=GetTickCount();ic_ok=ok&&info.ready;}
 if(!info.ready||info.generation!=c->core.generation){ic_drop();if(!c->core.lost)note_lost(0x22,info.generation);c->core.lost=1;p->ddrval=DDERR_SURFACELOST;return DDHAL_DRIVER_HANDLED;}
 command_size=surface_bytes(p->lpDDCommands);
 if(!p->lpDDCommands||!p->lpDDCommands->lpGbl||p->dwCommandOffset>command_size||p->dwCommandLength>command_size-p->dwCommandOffset)return DDHAL_DRIVER_HANDLED;
 if(!p->lpDDCommands->lpGbl->fpVidMem)return DDHAL_DRIVER_HANDLED;
 if((uint32_t)p->lpDDCommands->lpGbl->fpVidMem>0xFFFFFFFFUL-p->dwCommandOffset||
    (uint32_t)p->lpDDCommands->lpGbl->fpVidMem+p->dwCommandOffset>0xFFFFFFFFUL-p->dwCommandLength)return DDHAL_DRIVER_HANDLED;
 memset(&in,0,sizeof(in));in.commands=(uint8_t*)p->lpDDCommands->lpGbl->fpVidMem+p->dwCommandOffset;
 in.command_bytes=p->dwCommandLength;
 in.flags=p->dwFlags;in.render_states=(uint32_t*)p->lpdwRStates;in.render_state_count=256;
 in.fvf=p->dwVertexType;in.vertex_stride=stride; /* v10.189: inline-vertex commands (TRIANGLEFAN_IMM) need them without a vertex buffer */
 follow_targets(c); /* v10.166 */
 source.slot=c;source.args=p;source.info=&info;source.stride=stride;
 sv.context=&source;sv.submit=native_submit;sv.resolve=dp2_resolve;sv.unknown=unknown;sv.bind_vertices=bind_vertices;sv.colorkey=dp2_colorkey;sv.mips=dp2_mips;sv.faces=dp2_faces;sv.submit_n=native_submit_n; /* v10.276 */
 sv.target=vx_d3d_rttex?dp2_target:0;sv.target_done=vx_d3d_rttex?dp2_target_done:0;source.rt_kind=source.rt_lost=0; /* v10.456; v10.479: target_done */
 sv.volume=0; /* v10.317: volume textures are the ICD's (Direct3D 7 has none) */
 if(vxr_state)(void)vxr_call(c,&in,&sv); /* .517: the recorder (VXD3DMON REC=n) */
 {uint32_t th=c->core.target_handle,to=c->core.target.offset;
 #define TRZ(c) ((c)->core.state.rs[7]&255|((c)->core.state.rs[14]&255)<<8|((c)->core.state.rs[23]&255)<<16)
 /* v10.421: the last detail is the runtime's render-state array (lpdwRStates) for the same three states, 0xFFFFFFFF when unreadable */
 if(event_log_file&&tr_n<3000){const DWORD *r=(const DWORD*)p->lpdwRStates;DWORD rz=r&&!IsBadReadPtr(r,24*4)?(r[7]&255)|(r[14]&255)<<8|(r[23]&255)<<16:0xFFFFFFFFUL;
  tr_zw[0]=tr_zw[1]=0;vx_d3d_rs_hook=tr_rs;VX_D3D_trace(90,0,TRZ(c),p->dwCommandLength,p->dwFlags,rz);} /* v10.420 */
 {uint32_t ln=vx_d3d_lost_n; /* .539: a submit failure that lost the context (vx_d3d_submit_result) counted: reason 26, the opcode in bits 16-23 */
 p->ddrval=(HRESULT)vx_d3d_dp2(&c->core,&in,&sv,&error);p->dwErrorOffset=p->dwCommandOffset+error;
 if(vx_d3d_lost_n!=ln)note_lost(0x26,(vx_d3d_lost_rc&0x8000FFFFUL)|((error<in.command_bytes?(uint32_t)in.commands[error]:0)&255)<<16);}
 if(source.rt_lost&&p->ddrval!=DD_OK)p->ddrval=DDERR_SURFACELOST; /* v10.479: dp2_target's wait reported the device lost */
 rs_resync(c,p); /* v10.428 */
 if(vx_d3d_rs_hook){vx_d3d_rs_hook=0;VX_D3D_trace(92,(DWORD)p->ddrval,TRZ(c),tr_zw[0],tr_zw[1],error);}
 if(c->core.target_handle!=th||c->core.target.offset!=to){draw_shade[4]++;draw_shade[5]=c->core.target_handle;draw_shade[6]=c->core.target.offset;} /* v10.168: render-target changes (VXDX7 --flip) */
 draw_shade[7]=c->core.target.offset;}
 sync_target_keys(c);
 diagnostic.last_offset=p->dwErrorOffset;
 diagnostic.last_opcode=error<in.command_bytes?in.commands[error]:0;
 return DDHAL_DRIVER_HANDLED;
}
/* v10.192: a lost context fails every later batch the same way; log the first 8 failures per process, then only a changed result */
static uint32_t dp2_fail_pid,dp2_fail_count,dp2_fail_last;
static int dp2_events(HRESULT hr) {
 if(dp2_fail_pid!=GetCurrentProcessId()){dp2_fail_pid=GetCurrentProcessId();dp2_fail_count=0;dp2_fail_last=0;}
 if(dp2_fail_count<8||(uint32_t)hr!=dp2_fail_last){dp2_fail_count++;dp2_fail_last=(uint32_t)hr;return 1;}
 return 0;
}
void VX_frame_add(DWORD cat,DWORD cycles); /* v10.385 (vx_ddraw.c): the frame's time by callback */
static uint32_t ph_r0; /* v10.431: the thread's run time at the call's start */
static volatile DWORD dp2_pid; /* v10.389: the process of the last DrawPrimitives2 call (VXD3DMON stops when it ends) */
DWORD WINAPI VX_D3D_LastPid(void) {return dp2_pid;}
DWORD WINAPI VX_D3D_DP2(vx_ddi_dp2 *p) {
 DWORD rc,stride,ft=VX_hal_tsc();uint32_t fp[27];if(!p)return DDHAL_DRIVER_NOTHANDLED;stride=p->dwVertexSize;
 dp2_pid=GetCurrentProcessId();
 if(!cb_enter()){VX_D3D_trace(89,(DWORD)callback_owner,0,p->dwCommandLength,p->dwFlags,(DWORD)my_token()); /* v10.421: a call turned away (callback lock held: holder, bytes, flags, this process's token) */
  p->ddrval=DDERR_WASSTILLDRAWING;p->dwErrorOffset=p->dwCommandOffset;VX_frame_add(0,VX_hal_tsc()-ft);return DDHAL_DRIVER_HANDLED;}
 {uint32_t ce=VX_hal_tsc()-ft;sl_on=1;sl_ioctl=0;sl_tries=0;sl_why=0;sl_calls=0;sl_sleeps=0;sl_busy_t=ce;{int q;for(q=0;q<8;q++)sl_io[q]=0;}sl_pv=0; /* v10.387: sl_busy_t holds the cb_enter wait until the end */}
 {uint32_t ce=VX_hal_tsc()-ft;vx_d3d_phclock=ph_on?hal_clock:0;if(vx_d3d_phclock){vx_d3d_ph_begin(VX_PH_PROF);ph_r0=(pc_n&15)?0xFFFFFFFFUL:ph_run_ms(0);VX_PH(VX_PH_HAL);vx_d3d_ph_call[VX_PH_ADMIT]=ce;}} /* v10.431 */
 vx_d3d_clock=hal_clock;vx_d3d_ck_begin(); /* v10.390 */
 if(vx_d3d_held()&&hold_pid!=dp2_pid&&!owner_alive(hold_pid)){vx_d3d_hold_drop();hold_why[6]++;} /* .550: a gone process's held packets */
 hold_tid=GetCurrentThreadId(); /* .550 */
 {uint32_t t0=HPT(),d;save_fp(fp);rc=dp2_impl(p,stride);restore_fp(fp);vx_d3d_ck_end();d=HPT()-t0;diagnostic.dp2++;hal_prof[0]++;hal_prof[1]+=(d+128)>>8;HLAT(&hal_lat[0],d);}
 /* v10.189: D3DERR_COMMAND_UNPARSED is the protocol, not a failure: D3DIM.DLL passes a DirectX 3/5 execute buffer straight to DrawPrimitives2
  * and handles the instructions it gets back at dwErrorOffset (STATELIGHT, PROCESSVERTICES, EXIT) itself */
 if(p->ddrval!=(HRESULT)VX_D3D_UNPARSED){record(2,p->ddrval);
  if((FAILED(p->ddrval)||p->ddrval==VX_D3D_CONTEXT_BAD)&&dp2_events(p->ddrval))event(2,(DWORD)p->ddrval,diagnostic.last_opcode,p->dwErrorOffset,p->dwCommandLength,lost_why);} /* v10.185 */
 {uint32_t d=VX_hal_tsc()-ft,d8=d>>8,k,lo=0; /* v10.387: a slow call kept */
  sl_on=0;
  if(d>=10000000UL){sl_count++;sl_total+=d8;for(k=1;k<8;k++)if(sl_worst[k][0]<sl_worst[lo][0])lo=k;
   if(d8>sl_worst[lo][0]){sl_worst[lo][0]=d8;sl_worst[lo][1]=sl_busy_t>>8;sl_worst[lo][2]=sl_ioctl;sl_worst[lo][3]=sl_calls;sl_worst[lo][4]=sl_tries;sl_worst[lo][5]=0;sl_worst[lo][6]=sl_sleeps;sl_worst[lo][7]=sl_why;{int q;for(q=0;q<8;q++)sl_iow[lo][q]=sl_io[q];}
    {uint32_t j,q,u[3]={64,64,64};for(q=0;q<3;q++){uint32_t best=64;for(j=0;j<64;j++)if(j!=u[0]&&j!=u[1]&&vx_d3d_op_t[j]&&(best==64||vx_d3d_op_t[j]>vx_d3d_op_t[best]))best=j;u[q]=best;
     sl_ops[lo][q*2]=best==64?0xFFFFFFFFUL:best|vx_d3d_op_n[best]<<8;sl_ops[lo][q*2+1]=best==64?0:vx_d3d_op_t[best]>>8;}}}}}
 hold_tid=0;if(vx_d3d_held())hold_pid=dp2_pid; /* .550 */
 if(vx_d3d_phclock){uint32_t r1;VX_PH(VX_PH_PROF);r1=ph_r0==0xFFFFFFFFUL?0xFFFFFFFFUL:ph_run_ms(1);vx_d3d_ph_end(); /* .524: the thread's run time read for every 16th call only (two VxD calls a call were about 13 ms of a slow frame while VXD3DMON watched) */vx_d3d_phclock=0;ph_account(ph_r0,r1);} /* v10.431 */
 cb_leave();VX_frame_add(0,VX_hal_tsc()-ft);return rc;
}
DWORD WINAPI VX_D3D_Validate(vx_ddi_validate *p) {
 context_slot *c;vx3dn_info info={0};uint32_t fp[27];if(!p)return DDHAL_DRIVER_NOTHANDLED;p->dwNumPasses=0;
 if(!cb_enter()){p->ddrval=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 save_fp(fp);
 c=find(p->dwhContext);p->ddrval=VX_D3D_CONTEXT_BAD;
 if(c) {
  if(c->core.lost||!call(VX_IOCTL_3DN_INFO,0,0,&info,sizeof(info))||!info.ready||info.generation!=c->core.generation) {
   if(!c->core.lost)note_lost(0x23,info.generation);c->core.lost=1;p->ddrval=DDERR_SURFACELOST;
  } else {
   p->ddrval=(HRESULT)vx_d3d_texture_status(&c->core);
   if(!p->ddrval&&!vx_d3d_supported(&c->core))p->ddrval=VX_D3D_CONFLICTINGSTATE;
   /* v10.215: what the draw would leave out with these textures (three coordinate sets, a cube map with another unit) */
   if(!p->ddrval){dp2_source src;vx_d3d_services sv;memset(&src,0,sizeof(src));memset(&sv,0,sizeof(sv));src.slot=c;src.info=&info;sv.context=&src;sv.faces=dp2_faces;sv.resolve=dp2_resolve;sv.mips=dp2_mips;sv.colorkey=dp2_colorkey;
    p->ddrval=(HRESULT)vx_d3d_draw_status(&c->core,&sv);}
   if(!p->ddrval)p->dwNumPasses=1;
  }
 }
 if(p->ddrval)event(5,(DWORD)p->ddrval,c?vx_d3d_unsupported_why(&c->core):0,c?c->core.state.tss[0][0]:0,c?c->core.state.tss[0][1]:0,c?c->core.state.tss[0][4]:0); /* v10.185 */
 restore_fp(fp);cb_leave();return DDHAL_DRIVER_HANDLED;
}
/* v10.215: 96 (was 64): a 2048 cube map with every mip level is 6 x 12 = 72 surfaces; the frame stays under 3 KB */
#define NODES 96
static DWORD WINAPI create_surface_ex(LPDDHAL_CREATESURFACEEXDATA p) {
 DDRAWI_DDRAWSURFACE_LCL *nodes[NODES],*root;vx3dn_surface desc[NODES];uint32_t handles[NODES];
 uint32_t j,k,n=1,needed=0,free_count,owner=GetCurrentProcessId();surface_table *t;surface_slot *s;
 LPATTACHLIST a;
 if(!p)return DDHAL_DRIVER_NOTHANDLED;p->ddRVal=DDERR_INVALIDPARAMS;
 root=p->lpDDSLcl;
 if(!p->lpDDLcl||!root||!root->lpGbl||!root->lpSurfMore||!root->lpSurfMore->dwSurfaceHandle)return DDHAL_DRIVER_HANDLED;
 if(!cb_enter()){p->ddRVal=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 t=table_find(owner,(uint32_t)p->lpDDLcl,0);
 /* .536 (board, GTA3 with ACCEL_D3D_TNL: the context lost, reason 20, as gameplay began, then every DrawPrimitives2 call SURFACELOST and
  * the same frame shown over and over): a vertex buffer (an execute buffer) is not a texture or a target. Direct3D 8's hardware vertex
  * processing creates and releases them through CreateSurfaceEx with handles numbered apart from the textures' and targets', so a vertex
  * buffer's handle found the render target's slot and invalidated the context. This DirectX 7 DDI never names a vertex buffer by handle
  * (DrawPrimitives2 passes lpDDVertex): they are left out of the table. */
 if(root->ddsCaps.dwCaps&DDSCAPS_EXECUTEBUFFER){p->ddRVal=DD_OK;goto surface_done;}
 /* System-memory formats belong to the runtime/reference rasterizer. Do not
  * fail them. A zero fpVidMem is a disassociation only for system memory;
  * a video surface with fpVidMem==0 is not a destruction notification. */
 if(root->ddsCaps.dwCaps&DDSCAPS_SYSTEMMEMORY) {
  s=surface_find(t,root->lpSurfMore->dwSurfaceHandle);
  if(s){invalidate_surface(t,s,1);memset(s,0,sizeof(*s));}
  p->ddRVal=DD_OK;goto surface_done;
 }
 /* v10.185: a texture without memory yet (DDSCAPS_ALLOCONLOAD: it is allocated by IDirect3DTexture::Load) is recorded now and described
  * at its first use (resolve). It was skipped, so its handle never resolved and every draw with it failed. */
 if(!root->lpGbl->fpVidMem) {
  if(!(root->ddsCaps.dwCaps&DDSCAPS_TEXTURE)){p->ddRVal=DD_OK;goto surface_done;}
  if(!t)t=table_find(owner,(uint32_t)p->lpDDLcl,1);
  if(!t){p->ddRVal=DDERR_OUTOFMEMORY;event(6,p->ddRVal,0x15,root->ddsCaps.dwCaps,used_blocks,0);goto surface_done;}
  s=surface_find(t,root->lpSurfMore->dwSurfaceHandle);
  if(!s)for(k=0;k<t->blocks*BLOCK_SURFACES;k++)if(!table_at(t,k)->handle){s=table_at(t,k);break;}
  if(!s){if(!table_grow(t)){reclaim_dead();if(!table_grow(t)){p->ddRVal=DDERR_OUTOFMEMORY;event(6,p->ddRVal,0x16,root->ddsCaps.dwCaps,used_blocks,t->blocks);goto surface_done;}}
   for(k=0;k<t->blocks*BLOCK_SURFACES;k++)if(!table_at(t,k)->handle){s=table_at(t,k);break;}}
  if(!s){p->ddRVal=DDERR_OUTOFMEMORY;goto surface_done;}
  if(s->handle&&s->native_key!=(uint32_t)root)invalidate_surface(t,s,2);
  s->handle=root->lpSurfMore->dwSurfaceHandle;s->native_key=(uint32_t)root;memset(&s->desc,0,sizeof(s->desc));
  p->ddRVal=DD_OK;goto surface_done;
 }
 /* Walk the supported video flipping/depth attachment graph once. Cycles
  * are normal. Mipmaps, cube maps and stereo are not advertised or accepted.
  * Validate and reserve the entire group before changing existing mappings. */
 nodes[0]=root;
 for(j=0;j<n;j++) {
  uint32_t links=0;DDRAWI_DDRAWSURFACE_LCL *v=nodes[j];
  if(!v->lpSurfMore||!v->lpSurfMore->dwSurfaceHandle||!VX_D3D_surface(v,&desc[j])||
     ((v->ddsCaps.dwCaps&DDSCAPS_MIPMAP)&&!(v->ddsCaps.dwCaps&DDSCAPS_TEXTURE))|| /* v10.200: texture mip levels are taken */
     (v->lpSurfMore->ddsCapsEx.dwCaps2&DDSCAPS2_STEREOSURFACELEFT)||((v->lpSurfMore->ddsCapsEx.dwCaps2&DDSCAPS2_CUBEMAP)&&(!(v->ddsCaps.dwCaps&DDSCAPS_TEXTURE)||!(vx_d3d_feat&VX_F_CUBE)))) { /* v10.214: cube-map textures are taken */
   p->ddRVal=DDERR_UNSUPPORTED;
   event(6,p->ddRVal,(v->lpSurfMore&&v->lpSurfMore->dwSurfaceHandle&&!((v->ddsCaps.dwCaps&DDSCAPS_MIPMAP)&&!(v->ddsCaps.dwCaps&DDSCAPS_TEXTURE)))?vx_surface_why:0x10,v->ddsCaps.dwCaps,
    v->lpGbl?((DWORD)v->lpGbl->wWidth<<16|v->lpGbl->wHeight):0,v->lpGbl?(DWORD)v->lpGbl->lPitch:0);
   goto surface_done;
  }
  handles[j]=v->lpSurfMore->dwSurfaceHandle;
  for(k=0;k<j;k++)if(handles[k]==handles[j]){p->ddRVal=DDERR_INVALIDPARAMS;event(6,p->ddRVal,0x11,handles[j],j,k);goto surface_done;}
  for(a=v->lpAttachList;a;a=a->lpLink) {
   if(++links>64){p->ddRVal=DDERR_INVALIDPARAMS;event(6,p->ddRVal,0x12,v->ddsCaps.dwCaps,links,0);goto surface_done;}
   if(!a->lpAttached||!(a->lpAttached->ddsCaps.dwCaps&DDSCAPS_VIDEOMEMORY))continue;
   for(k=0;k<n;k++)if(nodes[k]==a->lpAttached)break;
   if(k==n){if(n==NODES){p->ddRVal=DDERR_OUTOFMEMORY;event(6,p->ddRVal,0x13,root->ddsCaps.dwCaps,n,0);goto surface_done;}nodes[n++]=a->lpAttached;}
  }
 }
 if(!t)t=table_find(owner,(uint32_t)p->lpDDLcl,1);
 if(!t){p->ddRVal=DDERR_OUTOFMEMORY;event(6,p->ddRVal,0x14,root->ddsCaps.dwCaps,used_blocks,0);goto surface_done;}
 for(j=0;j<n;j++)if(!surface_find(t,handles[j]))needed++;
 free_count=0;for(j=0;j<t->blocks*BLOCK_SURFACES;j++)if(!table_at(t,j)->handle)free_count++;
 while(free_count<needed) {
  uint32_t before=t->blocks;
  if(!table_grow(t)){reclaim_dead();if(!table_grow(t)){p->ddRVal=DDERR_OUTOFMEMORY;event(6,p->ddRVal,0x16,root->ddsCaps.dwCaps,used_blocks,t->blocks);goto surface_done;}}
  free_count+=(t->blocks-before)*BLOCK_SURFACES;
 }
 for(j=0;j<n;j++) {
  s=surface_find(t,handles[j]);
  if(!s)for(k=0;k<t->blocks*BLOCK_SURFACES;k++)if(!table_at(t,k)->handle){s=table_at(t,k);break;}
  if(s->handle&&(s->native_key!=(uint32_t)nodes[j]||memcmp(&s->desc,&desc[j],sizeof(desc[j]))))invalidate_surface(t,s,3);
  /* ContextCreate can precede the first CreateSurfaceEx. In that case
   * no previous table entry exists to compare with the live descriptor. */
  for(k=0;k<CONTEXTS;k++) {
   context_slot *c=&contexts[k];
   if(c->handle&&c->owner==owner&&c->local==t->local&&
     ((c->target_key==(uint32_t)nodes[j]&&memcmp(&c->core.target,&desc[j],sizeof(desc[j])))||
      (c->depth_key==(uint32_t)nodes[j]&&memcmp(&c->core.depth,&desc[j],sizeof(desc[j]))))){c->core.lost=1;note_lost(0x21,(DWORD)nodes[j]);}
  }
  s->handle=handles[j];s->native_key=(uint32_t)nodes[j];s->desc=desc[j];
 }
 p->ddRVal=DD_OK;
surface_done:
 record(6,p->ddRVal); /* v10.183: CreateSurfaceEx failures show in the HAL diagnostics (callback 6) */
 cb_leave();return DDHAL_DRIVER_HANDLED;
}
/* v10.151: DirectX 7 drops the whole HAL (DDCAPS_NOHARDWARE) when Miscellaneous2Callbacks offers CreateSurfaceEx without GetDriverState
 * (board log v10.150: DDHAL_SetInfo accepted, the runtime stopped after this reply). No D3DDEVINFO_* statistics are kept: report that. */
static DWORD WINAPI get_driver_state(LPDDHAL_GETDRIVERSTATEDATA p) {
 if(!p)return DDHAL_DRIVER_NOTHANDLED;
 if(p->lpdwStates&&p->dwLength&&!IsBadWritePtr(p->lpdwStates,p->dwLength))memset(p->lpdwStates,0,p->dwLength);
 p->ddRVal=DDERR_UNSUPPORTED;return DDHAL_DRIVER_HANDLED;
}
static DWORD WINAPI destroy_local(LPDDHAL_DESTROYDDLOCALDATA p) {
 uint32_t j,owner=GetCurrentProcessId();surface_table *t;if(!p)return DDHAL_DRIVER_NOTHANDLED;
 if(!cb_enter()){p->ddRVal=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 t=table_find(owner,(uint32_t)p->pDDLcl,0);if(t)table_release(t);
 for(j=0;j<CONTEXTS;j++)if(contexts[j].local==(uint32_t)p->pDDLcl&&contexts[j].owner==owner){if(vx_d3d_hold_is(&contexts[j].core)){hold_why[5]++;if(hold_pid==owner)(void)hold_send_here();else vx_d3d_hold_drop();} /* .550 */
  memset(&contexts[j],0,sizeof(contexts[j]));}
 p->ddRVal=DD_OK;cb_leave();return DDHAL_DRIVER_HANDLED;
}
/* v10.466 (review): DestroySurface answered DDERR_WASSTILLDRAWING when the callback lock was held, and DirectDraw freed the surface
 * anyway: its slot kept a freed object as native_key (read again at the next resolve). The lock is now waited for (another process's
 * callback: up to 200 ms; this process's: up to 20 ms, it may be this very thread's), then the surface is queued and forgotten under the
 * lock before any callback's own work (cb_enter drains the queue). WASSTILLDRAWING only with the queue full (32). */
#define FQ 32
volatile LONG fq_pid[FQ],fq_ptr[FQ];uint32_t fq_queued,fq_full; /* not static: the linked checks read them */
static void forget_locked(DDRAWI_DDRAWSURFACE_LCL *s,uint32_t owner);
static void forget_drain(void) {
 uint32_t k;for(k=0;k<FQ;k++){LONG pp=fq_ptr[k],pid=fq_pid[k];if(pp&&pid){forget_locked((DDRAWI_DDRAWSURFACE_LCL*)pp,(uint32_t)pid);fq_ptr[k]=0;InterlockedExchange(&fq_pid[k],0);}}
}
int VX_D3D_forget_surface(DDRAWI_DDRAWSURFACE_LCL *s) {
 uint32_t k,n;LONG me=(LONG)GetCurrentProcessId(),t=my_token();
 for(n=0;;n++){LONG o;
  if(cb_enter()){forget_locked(s,(uint32_t)me);cb_leave();return 1;}
  o=callback_owner;if(n>=(o==t?20U:200U))break;Sleep(1);}
 for(k=0;k<FQ;k++)if(vx_cas(&fq_pid[k],me,0)==0){fq_ptr[k]=(LONG)s;fq_queued++;return 1;}
 fq_full++;return 0;
}
static void forget_locked(DDRAWI_DDRAWSURFACE_LCL *s,uint32_t owner) {
 uint32_t i,j;surface_table *t;surface_slot *v;
 /* ContextCreate can precede a DX6 CreateSurfaceEx notification. Its
  * direct surface identities must be invalidated even without a handle map. */
 for(j=0;j<CONTEXTS;j++)if(contexts[j].owner==owner&&contexts[j].handle&&
    (contexts[j].target_key==(uint32_t)s||contexts[j].depth_key==(uint32_t)s)){contexts[j].core.lost=1;note_lost(0x24,(DWORD)s);}
 for(i=0;i<TABLES;i++)if(tables[i].owner==owner&&tables[i].local) {
  t=&tables[i];
  for(j=0;j<t->blocks*BLOCK_SURFACES;j++) {
   v=table_at(t,j);
   if(v->handle&&v->native_key==(uint32_t)s){invalidate_surface(t,v,4);memset(v,0,sizeof(*v));}
  }
 }
}
static DWORD WINAPI setrt(vx_ddi_setrt *p) {
 context_slot *c;vx3dn_surface t,z;vx3dn_info info={0};DDRAWI_DDRAWSURFACE_LCL *tl=0,*zl=0;int rtk=0;if(!p)return DDHAL_DRIVER_NOTHANDLED;
 if(!cb_enter()){p->ddrval=DDERR_WASSTILLDRAWING;return DDHAL_DRIVER_HANDLED;}
 memset(&z,0,sizeof(z));c=find(p->dwhContext);p->ddrval=c?DDERR_INVALIDPARAMS:VX_D3D_CONTEXT_BAD;
 if(c&&(c->core.lost||!call(VX_IOCTL_3DN_INFO,0,0,&info,sizeof(info))||!info.ready||info.generation!=c->core.generation)) {
  if(!c->core.lost)note_lost(0x25,info.generation);c->core.lost=1;p->ddrval=DDERR_SURFACELOST;
 } else if(c&&(tl=surface_lcl(p->lpDDS))!=0&&(rtk=VX_D3D_surface_target(tl,&t))!=0&&(!p->lpDDSZ||((zl=surface_lcl(p->lpDDSZ))!=0&&VX_D3D_surface(zl,&z)))&&
           vx_d3d_targets_valid(&t,&z)&&native_surface(&t,&info)&&(!z.format||native_surface(&z,&info))) {
  if(!rt_leave(c,&c->core.target))p->ddrval=DDERR_SURFACELOST; /* v10.479 (review): the wait's lost device answered DD_OK; the target stays */
  else {c->rt_tex=rtk==2; /* v10.456 */
  c->core.target=t;c->core.depth=z;c->core.target_handle=0;c->core.depth_handle=0;
  c->target_key=(uint32_t)tl;c->depth_key=(uint32_t)zl;p->ddrval=DD_OK;} /* v10.215: interface or local objects, as ContextCreate */
 }
 cb_leave();return DDHAL_DRIVER_HANDLED;
}
static DWORD copy_info(LPDDHAL_GETDRIVERINFODATA p,const void *data,uint32_t bytes) {
 uint32_t n=p->dwExpectedSize<bytes?p->dwExpectedSize:bytes;
 if(n&&!p->lpvData){p->ddRVal=DDERR_INVALIDPARAMS;return DDHAL_DRIVER_HANDLED;}
 {uint32_t k;uint8_t *o=(uint8_t*)p->lpvData;const uint8_t *i=(const uint8_t*)data;for(k=0;k<n;k++)o[k]=i[k];} /* v10.158: plain loop, no C library call in the runtime's context */
 p->dwActualSize=n;p->ddRVal=DD_OK;return DDHAL_DRIVER_HANDLED;
}
#include "vx_d3d_legacy.inc"
static int guid(const GUID *a,uint32_t d1,uint16_t d2,uint16_t d3,const uint8_t *d4) {return a->Data1==d1&&a->Data2==d2&&a->Data3==d3&&!memcmp(a->Data4,d4,8);}
static DWORD get_driver_info(LPDDHAL_GETDRIVERINFODATA p) {
 static const uint8_t a[8]={0xA9,0xB6,0,0xAA,0,0xC0,0x99,0x3E},b[8]={0x89,0xAB,0,0xA0,0xC9,5,0x41,0x29};
 static const uint8_t z[8]={0x9B,0x1B,0,0xAA,0,0xBB,0xB8,0xAE},u[8]={0x8C,0xE1,0,0xA0,0xC9,6,0x29,0xA8};
 static const uint8_t m[8]={0xB6,0x40,0,0xAA,0,0xA1,0xF9,0x6A},two[8]={0x88,0x9D,0,0xAA,0,0xBB,0xB7,0x6A};
 if(!p)return DDHAL_DRIVER_NOTHANDLED;p->dwActualSize=0;p->ddRVal=DDERR_CURRENTLYNOTAVAIL;
 if(guid(&p->guidInfo,0xDDF41230UL,0xEC0A,0x11D0,a))return copy_info(p,&callbacks3,sizeof(callbacks3));
 if(guid(&p->guidInfo,0x0BA584E1UL,0x70B6,0x11D0,two))return copy_info(p,&callbacks2,sizeof(callbacks2));
 if(guid(&p->guidInfo,0x7DE41F80UL,0x9D93,0x11D0,b)) {
  vx_ddi_extended e;memset(&e,0,sizeof(e));e.dwSize=sizeof(e);e.dwMaxAnisotropy=16; /* v10.199: XP's, CLXP3D T47 */
  /* v10.169: one texture stage: DISABLE, SELECTARG1, SELECTARG2, MODULATE (drawn as the texel until a modulate program is proven), one coordinate set */
  e.dwTextureOpCaps=(D3DTEXOPCAPS_DISABLE|D3DTEXOPCAPS_SELECTARG1|D3DTEXOPCAPS_SELECTARG2|D3DTEXOPCAPS_MODULATE|D3DTEXOPCAPS_MODULATE2X|D3DTEXOPCAPS_MODULATE4X|D3DTEXOPCAPS_ADD|D3DTEXOPCAPS_ADDSIGNED|D3DTEXOPCAPS_BLENDDIFFUSEALPHA|D3DTEXOPCAPS_BLENDTEXTUREALPHA|
   D3DTEXOPCAPS_ADDSIGNED2X|D3DTEXOPCAPS_SUBTRACT|D3DTEXOPCAPS_ADDSMOOTH|D3DTEXOPCAPS_BLENDFACTORALPHA|D3DTEXOPCAPS_BLENDCURRENTALPHA|
   D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR|D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA|
   D3DTEXOPCAPS_BLENDTEXTUREALPHAPM|D3DTEXOPCAPS_PREMODULATE|D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR|D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA|D3DTEXOPCAPS_DOTPRODUCT3|D3DTEXOPCAPS_BUMPENVMAP|D3DTEXOPCAPS_BUMPENVMAPLUMINANCE); /* v10.208: 0xFFFFFF (bump); v10.206: 0x9FFFFF; v10.198: + the ops vx_d3d_compose builds (0x6BFFF) */ /* v10.181: 0x18FF, the ops vx_d3d_program takes; v10.172-180 sent 0xCFF (0x400 is ADDSMOOTH, never taken, and BLENDTEXTUREALPHA 0x1000 was missing) */e.wMaxTextureBlendStages=(trace_mode&2)?1:3;e.wMaxSimultaneousTextures=(trace_mode&2)?0:3;e.dwFVFCaps=(trace_mode&2)?0:2;
  /* v10.212: switched-off features leave the caps */
  if(!(vx_d3d_feat&VX_F_NEWOPS))e.dwTextureOpCaps&=~(DWORD)(D3DTEXOPCAPS_BLENDTEXTUREALPHAPM|D3DTEXOPCAPS_PREMODULATE|D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR|D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA|D3DTEXOPCAPS_DOTPRODUCT3);
  if(!(vx_d3d_feat&VX_F_BUMP)||!(vx_d3d_feat&VX_F_STAGES))e.dwTextureOpCaps&=~(DWORD)(D3DTEXOPCAPS_BUMPENVMAP|D3DTEXOPCAPS_BUMPENVMAPLUMINANCE);
  if(!(vx_d3d_feat&VX_F_STAGES)){e.wMaxTextureBlendStages=1;e.wMaxSimultaneousTextures=1;e.dwFVFCaps=1;} /* v10.206: three stages and textures, two coordinate sets */
  e.dwStencilCaps=0xFF; /* v10.201: KEEP ZERO REPLACE INCRSAT DECRSAT INVERT INCR DECR on D24S8 (cookbook 6.5) */
  {e.dwMinTextureWidth=e.dwMinTextureHeight=1;e.dwMaxTextureWidth=e.dwMaxTextureHeight=2048;e.dwMaxTextureRepeat=2048;e.dwMaxTextureAspectRatio=2048; /* v10.199: 2048 */}
  /* No pure-device, hardware VB/IB or shader caps; hardware T&L only for the probe (.534, ACCEL_D3D_TNL): eight lights, no user clip
   * planes, no vertex blending; D3DVTXPCAPS_TEXGEN, MATERIALSOURCE7, VERTEXFOG, DIRECTIONALLIGHTS, POSITIONALLIGHTS, LOCALVIEWER (0x3F) */
  if(vx_d3d_tnl){e.dwMaxActiveLights=8;e.dwVertexProcessingCaps=0x3FUL;}
  if(p->dwExpectedSize==36)e.dwSize=36;else if(p->dwExpectedSize==84)e.dwSize=84;
  return copy_info(p,&e,e.dwSize);
 }
 /* v10.214: GUID_DDMoreSurfaceCaps: DDSCAPS2_CUBEMAP and all six faces, no extra restriction on any heap (DirectDraw creates no cube
  * surface without it) */
 {static const uint8_t ms[8]={0x88,0x0B,0x00,0xC0,0x4F,0xD9,0x30,0xC5};
  if((vx_d3d_feat&VX_F_CUBE)&&guid(&p->guidInfo,0x3B8A0466UL,0xF269,0x11D1,ms)) {
   uint8_t d[4+12+24*8];uint32_t n=p->dwExpectedSize;memset(d,0,sizeof(d));
   if(n<40||n>sizeof(d)||(n-16)%24)return DDHAL_DRIVER_HANDLED;
   ((DWORD*)d)[0]=n;((DWORD*)d)[1]=DDSCAPS2_CUBEMAP|0xFC00UL;
   return copy_info(p,d,n);
  }}
 if(guid(&p->guidInfo,0x93869880UL,0x36CF,0x11D1,z)) {
  /* v10.177: Z16, D24S8, D32 in XP's order */
  struct {DWORD count;DDPIXELFORMAT f[3];} d;memset(&d,0,sizeof(d));d.count=3;
  d.f[0].dwSize=sizeof(DDPIXELFORMAT);d.f[0].dwFlags=DDPF_ZBUFFER;d.f[0].dwZBufferBitDepth=16;d.f[0].dwZBitMask=0xFFFFUL;
  d.f[1].dwSize=sizeof(DDPIXELFORMAT);d.f[1].dwFlags=DDPF_ZBUFFER|DDPF_STENCILBUFFER;d.f[1].dwZBufferBitDepth=32;d.f[1].dwStencilBitDepth=8;d.f[1].dwZBitMask=0xFFFFFF00UL;d.f[1].dwStencilBitMask=0xFFUL;
  d.f[2].dwSize=sizeof(DDPIXELFORMAT);d.f[2].dwFlags=DDPF_ZBUFFER;d.f[2].dwZBufferBitDepth=32;d.f[2].dwZBitMask=0xFFFFFFFFUL;
  return copy_info(p,&d,sizeof(d));
 }
 if(guid(&p->guidInfo,0x406B2F00UL,0x3E5A,0x11D1,m)) {
  DDHAL_DDMISCELLANEOUS2CALLBACKS c;dx7_handles=1;memset(&c,0,sizeof(c));c.dwSize=sizeof(c);
  c.dwFlags=DDHAL_MISC2CB32_CREATESURFACEEX|DDHAL_MISC2CB32_GETDRIVERSTATE|DDHAL_MISC2CB32_DESTROYDDLOCAL;c.CreateSurfaceEx=create_surface_ex;c.GetDriverState=get_driver_state;c.DestroyDDLocal=destroy_local;
  return copy_info(p,&c,sizeof(c));
 }
 if(guid(&p->guidInfo,0x2E04FFA0UL,0x98E4,0x11D1,u)) {
  /* This special notification carries the CALLBACK ADDRESS in lpvData,
   * not a readable buffer containing another pointer. Never dereference
   * the runtime's function entry as if it were an output structure. */
  if(p->lpvData){parse_unknown_set((parse_fn)p->lpvData);p->ddRVal=DD_OK;}
  else p->ddRVal=DDERR_INVALIDPARAMS;
  return DDHAL_DRIVER_HANDLED;
 }
 /* GetDriverInfo2 aliases DDStereoMode. This DX7 milestone advertises
  * neither GetDriverInfo2 nor stereo and does not guess at their payload. */
 return DDHAL_DRIVER_HANDLED;
}
DWORD WINAPI VX_D3D_GetDriverInfo(LPDDHAL_GETDRIVERINFODATA p) {
 DWORD r,j;
 r=get_driver_info(p);
 if(p&&p->ddRVal==DD_OK&&p->dwActualSize&&p->lpvData){trace_reply[0]=p->guidInfo.Data1;probe_words(trace_reply+1,p->lpvData,p->dwActualSize>=28?7:p->dwActualSize/4);}
 if(p){j=trace_count&31;trace_ring[j][0]=p->guidInfo.Data1;trace_ring[j][1]=p->dwExpectedSize;trace_ring[j][2]=p->dwActualSize;trace_ring[j][3]=(DWORD)p->ddRVal;trace_count++;}
 return r;
}
/* v10.159: execute/command buffers in system memory, as JHRobotics' vmhal9x (a working Win98 D3D HAL on the same vmdisp9x base) does:
 * the runtime allocates them (PLEASEALLOC_USERMEM) and frees them; lock hands back that memory. Wired for ACCEL_D3D 3 (v10.165). */
#ifndef DDHAL_PLEASEALLOC_USERMEM
#define DDHAL_PLEASEALLOC_USERMEM 0x00000004UL
#endif
/* v10.149: the HAL is advertised when built with --d3d-experimental, or when HKLM\Software\VX85598 ACCEL_D3D (DWORD) is nonzero. Default 0 until
 * the board has run VX3DM1 and VXDX7; the native endpoint (VX3DM1) exists either way. */
/* v10.150: what DirectDraw asked GetDriverInfo, read back by VXDX7 through VX_D3D_QueryTrace. Shared DLL data: every process sees one copy. */
/* v10.174: the last D3DTSS_ADDRESS, ADDRESSU, ADDRESSV values stage 0 received in DrawPrimitives2, and a mask of which arrived */
/* v10.175: then out[4] = records in the ring (max 16), out[5..] pairs (call << 16 | state, value), oldest first */
DWORD WINAPI VX_D3D_QueryTSS(DWORD *out,DWORD n) {
 DWORD k,c=vx_d3d_addr_count,m=c>16?16:c,first=c-m;if(!out||n<4)return 0;for(k=0;k<4;k++)out[k]=vx_d3d_tss_seen[k];
 if(n<5+2*m)return 4;out[4]=m;for(k=0;k<m;k++){out[5+k*2]=vx_d3d_addr_ring[(first+k)%16][0];out[6+k*2]=vx_d3d_addr_ring[(first+k)%16][1];}
 return 5+2*m;
}
DWORD WINAPI VX_D3D_QueryTrace(DWORD *out,DWORD n) {
 DWORD i,k=0,c=trace_count;
 if(!out||n<28)return 0;
 out[k++]=0x00020000UL;out[k++]=trace_inits;out[k++]=trace_mode;out[k++]=c;
 out[k++]=shared_hal?shared_hal->vxBuilds:0;out[k++]=shared_hal?shared_hal->vxSetInfo:0;out[k++]=shared_hal?shared_hal->vxBuiltCaps:0;out[k++]=shared_hal?shared_hal->vxBuiltSurfCaps:0;
 for(i=0;i<8;i++)out[k++]=trace_reply[i];
 for(i=0;i<12;i++)out[k++]=trace_probe[i];
 for(i=0;i<32&&i<c&&k+4<=n;i++){DWORD j=(c>32?c-32+i:i)&31;out[k++]=trace_ring[j][0];out[k++]=trace_ring[j][1];out[k++]=trace_ring[j][2];out[k++]=trace_ring[j][3];}
 return k;
}
extern DWORD vx_heap_round;
static DWORD d3d_registry(void) { /* the VxD reads the registry (ACCEL_D3D) and answers control query 0xFFFFFE50; this shared DLL imports only kernel32/user32/gdi32 */
 HANDLE h=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL); DWORD code=0xFFFFFE50UL,o[4],got=0; DWORD on=0;
 vx_d3d_hold_on=1; /* .551: the queue held across DrawPrimitives2 calls, always (ACCEL_D3D_HOLD removed: GTA3 on .550, median 93 -> 76 ms) */
 if(h==INVALID_HANDLE_VALUE) return 0;
 memset(o,0,sizeof(o));
 if(DeviceIoControl(h,0x85500012UL,&code,4,o,16,&got,NULL)&&got==16&&o[1]==0x56584433UL) {on=o[0];if(o[2]&0x100UL)vx_heap_round=o[2]&1;if(o[2]&0x400UL){event_log_file=(o[2]>>1)&1;vx_d3d_ckey_on=(o[2]>>2)&1;vx_d3d_sample_on=(o[2]>>13)&1;} /* .511: bit 13 ACCEL_D3D_SAMPLE */if(o[2]&0x800UL){vx_d3d_rttex=(o[2]>>3)&1;vx_d3d_mipcap=(o[2]>>4)&1;vx_d3d_texhint=(o[2]>>5)&1;{extern uint32_t vx_d3d_texruns;vx_d3d_texruns=1;} /* .528; .529: always *//* .531: bit 6 (ACCEL_D3D_IX384) retired: 384 always */vx_d3d_indexed=(o[2]>>7)&1;vx_d3d_rtnowait=(o[2]>>12)&1;{extern uint32_t vx_d3d_ixpass;vx_d3d_ixpass=(o[2]>>16)&1;{vx_d3d_infocache=(o[2]>>17)&1;vx_d3d_delta=(o[2]>>20)&1;vx_d3d_tnl=(o[2]>>21)&1; /* .534: bit 21 ACCEL_D3D_TNL */
  /* .543: the driver's T&L is SSE only (vx_d3d_tnl.inc): offered only when the processor has FXSR, SSE and SSE2 (CPUID 1 EDX bits 24-26)
   * and the system saves the XMM registers (CR4 OSFXSR, bit 9): the VxD's control 0xFFFFFE34, as VXGL.DLL reads it */
  /* .545: the same answer sets vx_d3d_sse2 (the indexed pass-through's span kernels, vx_d3d_span.inc), with or without the T&L */
  {extern uint32_t vx_d3d_sse2;DWORD q=0xFFFFFE34UL,x[4]={0,0,0,0},g2=0;
   vx_d3d_sse2=DeviceIoControl(h,0x85500012UL,&q,4,x,16,&g2,NULL)&&g2==16&&x[1]==0x31455353UL&&(x[2]&0x07000000UL)==0x07000000UL&&(x[0]&0x200UL);
   if(!vx_d3d_sse2)vx_d3d_tnl=0;}
 {extern uint32_t vx_d3d_v8;vx_d3d_v8=(o[2]>>19)&1;}} /* .523: bit 19 ACCEL_D3D_V8 */}} /* .522: bits 17 ACCEL_D3D_INFOCACHE, 18 ACCEL_D3D_IXPF */ /* v10.456; .515: bits 14-15 ACCEL_D3D_HWCULL, 16 ACCEL_D3D_IXPASS */if(o[3]&0x80000000UL){vx_d3d_feat=o[3]&0x7FFFFFFFUL;vx_d3d_prepack=(o[3]>>26)&1;}} /* .527: bits 28-30 (FASTVV, VRECIPE, FASTMATCH) removed */ /* v10.454: bit 28 ACCEL_D3D_FASTVV */ /* v10.445: bits 29 ACCEL_D3D_VRECIPE, 30 ACCEL_D3D_FASTMATCH */ /* v10.443: bit 28 ACCEL_D3D_QDIRECT */ /* v10.442: bit 27 ACCEL_D3D_PAIRLIST */ /* v10.441: bit 26 ACCEL_D3D_PREPACK */ /* o[1] tag 'VXD3' guards against an older VxD; o[2] (v10.179) ACCEL_HEAP_ALIGN */
 CloseHandle(h); return on;
}
void VX_D3D_init(void *ctx) {
 D3DPRIMCAPS *p;vx_ddi_desc *h;
 ic_drop(); /* .522: a HAL rebuild (a mode change): the readiness answer asked again */
 shared_hal=ctx;
 /* v10.185: kept across HAL rebuilds (each mode change rebuilds the HAL): what an application did before a mode switch stays readable */
 if(diagnostic.cb!=sizeof(diagnostic)||diagnostic.version!=0x0002000BUL){memset(&diagnostic,0,sizeof(diagnostic));diagnostic.cb=sizeof(diagnostic);diagnostic.version=0x0002000BUL;}
 /* v10.60's Win16 bootstrap reads these existing capability slots. */
 memset(&shared_hal->d3dhal_flags,0,sizeof(shared_hal->d3dhal_flags));
 memset(&global_caps,0,sizeof(global_caps));global_caps.dwSize=sizeof(global_caps);h=&global_caps.hwCaps;
 h->dwSize=sizeof(*h);h->dwFlags=D3DDD_COLORMODEL|D3DDD_DEVCAPS|D3DDD_BCLIPPING|D3DDD_LINECAPS|D3DDD_TRICAPS|D3DDD_DEVICERENDERBITDEPTH|D3DDD_DEVICEZBUFFERBITDEPTH|D3DDD_MAXVERTEXCOUNT;
 h->dcmColorModel=D3DCOLOR_RGB;h->dwDevCaps=D3DDEVCAPS_EXECUTESYSTEMMEMORY|D3DDEVCAPS_DRAWPRIMTLVERTEX|D3DDEVCAPS_FLOATTLVERTEX|D3DDEVCAPS_TLVERTEXSYSTEMMEMORY|D3DDEVCAPS_DRAWPRIMITIVES2|D3DDEVCAPS_HWRASTERIZATION;
 h->bClipping=FALSE;h->dpcLineCaps.dwSize=sizeof(D3DPRIMCAPS);p=&h->dpcTriCaps;p->dwSize=sizeof(*p);
 p->dwMiscCaps=D3DPMISCCAPS_MASKZ|D3DPMISCCAPS_CULLNONE|D3DPMISCCAPS_CULLCW|D3DPMISCCAPS_CULLCCW; /* v10.178: + MASKZ (ZWRITEENABLE works) */
 /* v10.178 (WinTune 98 "Could not find any compatible devices"): the raster caps were empty. Z test and subpixel accuracy are what the
  * hardware does (VXDX7 --draw and --raster: Z with every ZFUNC, float vertices); DX5-style device checks look for D3DPRASTERCAPS_ZTEST. */
 (void)d3d_registry(); /* v10.215: the feature switches before the caps that depend on them (the DITHER cap was set from the default) */
 if(vx_d3d_tnl)h->dwDevCaps|=0x10000UL; /* .534: D3DDEVCAPS_HWTRANSFORMANDLIGHT for the probe (vx_d3d_core.c); nothing is transformed */
 p->dwRasterCaps=D3DPRASTERCAPS_ZTEST|D3DPRASTERCAPS_SUBPIXEL|0x80UL|0x100UL|((vx_d3d_feat&VX_F_DITHER)?0x1UL:0)|0x4000UL; /* v10.196: + FOGVERTEX, FOGTABLE (fog, v10.195); v10.201: + DITHER, ZBIAS */
 p->dwZCmpCaps=255;p->dwSrcBlendCaps=0x1FFF;p->dwDestBlendCaps=0x3FF;p->dwAlphaCmpCaps=255; /* v10.177: every blend factor (SRCALPHASAT/BOTH* as source only), alpha test */
 p->dwShadeCaps=D3DPSHADECAPS_COLORFLATRGB|D3DPSHADECAPS_COLORGOURAUDRGB|D3DPSHADECAPS_SPECULARFLATRGB|D3DPSHADECAPS_SPECULARGOURAUDRGB|D3DPSHADECAPS_ALPHAFLATBLEND|D3DPSHADECAPS_ALPHAGOURAUDBLEND|0x80000UL; /* v10.196: + FOGGOURAUD. v10.178: + specular (XP's specular programs, v10.172) */
 h->dwDeviceRenderBitDepth=DDBD_16|DDBD_32;h->dwDeviceZBufferBitDepth=DDBD_16|DDBD_32;h->dwMaxVertexCount=4096;
 /* v10.188: was 65535. D3DIM.DLL 4.07.00.0700 (the DirectX 3/5/6 interfaces) sizes its TL vertex buffer at every Execute to at least
  * dwMaxVertexCount (56675380: max(count, hwCaps.dwMaxVertexCount), capped at 4096 only when the count is above it) plus 1 KB: 65535 asked
  * for 65568 vertices, more than a vertex buffer holds, the creation failed and every DirectX 3/5 Execute returned E_OUTOFMEMORY (Tunnel).
  * 4096 is the runtime's own cap. */
 memset(&callbacks,0,sizeof(callbacks));callbacks.dwSize=sizeof(callbacks);callbacks.ContextCreate=VX_D3D_Create;callbacks.ContextDestroy=VX_D3D_Destroy;callbacks.ContextDestroyAll=VX_D3D_DestroyAll;
 callbacks.RenderState=VX_D3D_RenderState;callbacks.RenderPrimitive=VX_D3D_RenderPrimitive;callbacks.GetState=VX_D3D_GetState;callbacks.SceneCapture=VX_D3D_Scene;
 /* v10.164: DDRAW.DLL 4.09.00.0904 (BAABB95A) accepts DrawOnePrimitive, DrawOneIndexedPrimitive and DrawPrimitives only all together or none:
  * the DrawPrimitives2 HAL offers none of them (it had the first two, which dropped the whole HAL). */
 memset(&callbacks2,0,sizeof(callbacks2));callbacks2.dwSize=sizeof(callbacks2);callbacks2.dwFlags=3;callbacks2.SetRenderTarget=setrt;
 callbacks2.Clear=VX_D3D_Clear;
 memset(&callbacks3,0,sizeof(callbacks3));callbacks3.dwSize=sizeof(callbacks3);callbacks3.dwFlags=4|8;callbacks3.ValidateTextureStageState=VX_D3D_Validate;callbacks3.DrawPrimitives2=VX_D3D_DP2;
 {DWORD mode=d3d_registry()&1; /* ACCEL_D3D: 1 (default) the DirectX 6/7 HAL (DrawPrimitives2, CreateSurfaceEx), 0 off. v10.430: the DirectX
  * 5-style HAL (3: execute buffers, no GetDriverInfo), never drawn on the board, is gone; 3 now means 1. */
 memset(&shared_hal->d3dhal_exebuffcallbacks,0,sizeof(shared_hal->d3dhal_exebuffcallbacks));
 {
  global_caps.hwCaps.dwDevCaps|=D3DDEVCAPS_DRAWPRIMITIVES2EX; /* v10.164: DDRAW.DLL (BAABB726) drops a HAL that offers CreateSurfaceEx without D3DDEVCAPS_DRAWPRIMITIVES2EX */
  /* v10.169: textures in video memory, A8R8G8B8 and X8R8G8B8, one stage */
  /* v10.172: the order and set of Windows XP's driver (CLXP3D): 32-bit with and without alpha, then the 16-bit formats */
  memset(tex_formats,0,sizeof(tex_formats));
  {static const DWORD fm[13][6]={{DDPF_RGB|DDPF_ALPHAPIXELS,32,0xFF0000UL,0xFF00UL,0xFFUL,0xFF000000UL},{DDPF_RGB,32,0xFF0000UL,0xFF00UL,0xFFUL,0},
    {DDPF_RGB,16,0xF800UL,0x7E0UL,0x1FUL,0},{DDPF_RGB|DDPF_ALPHAPIXELS,16,0x7C00UL,0x3E0UL,0x1FUL,0x8000UL},{DDPF_RGB|DDPF_ALPHAPIXELS,16,0xF00UL,0xF0UL,0xFUL,0xF000UL},
    {DDPF_RGB,16,0x7C00UL,0x3E0UL,0x1FUL,0},{DDPF_LUMINANCE,8,0xFFUL,0,0,0},{DDPF_FOURCC,0,0,0,0,0},{DDPF_FOURCC,0,0,0,0,0},{DDPF_FOURCC,0,0,0,0,0},
    {DDPF_BUMPDUDV,16,0xFFUL,0xFF00UL,0,0},{DDPF_BUMPDUDV|DDPF_BUMPLUMINANCE,16,0x1FUL,0x3E0UL,0xFC00UL,0},{DDPF_BUMPDUDV|DDPF_BUMPLUMINANCE,32,0xFFUL,0xFF00UL,0xFF0000UL,0}}; /* v10.208: V8U8, L6V5U5, X8L8V8U8 */
   static const DWORD fcc[13]={0,0,0,0,0,0,0,0x31545844UL,0x33545844UL,0x35545844UL,0,0,0};int k; /* v10.199: + L8, DXT1, DXT3, DXT5 */
   for(k=0;k<13;k++){DDSURFACEDESC *t=&tex_formats[k];t->dwSize=sizeof(*t);t->dwFlags=DDSD_CAPS|DDSD_PIXELFORMAT;t->ddsCaps.dwCaps=DDSCAPS_TEXTURE;
    t->ddpfPixelFormat.dwSize=sizeof(DDPIXELFORMAT);t->ddpfPixelFormat.dwFlags=fm[k][0];t->ddpfPixelFormat.dwRGBBitCount=fm[k][1];
    t->ddpfPixelFormat.dwRBitMask=fm[k][2];t->ddpfPixelFormat.dwGBitMask=fm[k][3];t->ddpfPixelFormat.dwBBitMask=fm[k][4];t->ddpfPixelFormat.dwRGBAlphaBitMask=fm[k][5];t->ddpfPixelFormat.dwFourCC=fcc[k];}}
  global_caps.dwNumTextureFormats=(vx_d3d_feat&VX_F_BUMP)&&(vx_d3d_feat&VX_F_STAGES)?13:10;global_caps.lpTextureFormats=tex_formats; /* v10.212: the bump formats are the last three */
  global_caps.hwCaps.dwDevCaps|=D3DDEVCAPS_TEXTUREVIDEOMEMORY;
  p->dwTextureCaps=1|2|4|((vx_d3d_feat&VX_F_PROJECTED)?0x400:0)|((vx_d3d_feat&VX_F_CUBE)?0x800:0); /* v10.214: + CUBEMAP */
  /* v10.456 (cookbook 24.8; Velocity9x: 3DMark2001 refused its first mipmapped texture without it): ACCEL_D3D_MIPCAP (default 1) adds
   * D3DPTEXTURECAPS_MIPMAP (0x4000, DirectX 8's name for what the filter caps and DDSCAPS_MIPMAP already offer; a cube map's levels are
   * not sampled here, so no MIPCUBEMAP). VXRTT reports what Direct3D 8 makes of it. */
  if(vx_d3d_mipcap)p->dwTextureCaps|=0x4000UL;                       /* D3DPTEXTURECAPS_PERSPECTIVE (v10.172: W carries rhw) | POW2 | ALPHA | PROJECTED (v10.208, T151) */
  p->dwTextureFilterCaps=1|2|0x100|0x200|0x1000000UL|0x2000000UL; /* NEAREST LINEAR MINFPOINT MINFLINEAR MAGFPOINT MAGFLINEAR */
  /* v10.199: + MIPNEAREST MIPLINEAR LINEARMIPNEAREST LINEARMIPLINEAR, MINFANISOTROPIC, MIPFPOINT MIPFLINEAR (CLXP3D T21-T23, T47);
   * raster + MIPMAPLODBIAS, ANISOTROPY */
  p->dwTextureFilterCaps|=4|8|0x10|0x20|0x400|0x10000UL|0x20000UL;p->dwRasterCaps|=0x2000UL|0x20000UL;
  /* v10.178: legacy TEXTUREMAPBLEND modes (the DirectX 6+ runtime maps them to stage 0, all of which run): DECAL MODULATE DECALALPHA
   * MODULATEALPHA COPY ADD. DX5-style applications check these before they accept a device. */
  p->dwTextureBlendCaps=D3DPTBLENDCAPS_DECAL|D3DPTBLENDCAPS_MODULATE|D3DPTBLENDCAPS_DECALALPHA|D3DPTBLENDCAPS_MODULATEALPHA|D3DPTBLENDCAPS_COPY|D3DPTBLENDCAPS_ADD;
  p->dwTextureAddressCaps=1|2|4|0x10|((vx_d3d_feat&VX_F_BORDER)?8:0); /* WRAP MIRROR CLAMP INDEPENDENTUV; v10.304: BORDER */
 } /* v10.164: DDRAW.DLL (BAABB726) drops a HAL that offers CreateSurfaceEx without D3DDEVCAPS_DRAWPRIMITIVES2EX */
 {global_caps.hwCaps.dpcLineCaps=global_caps.hwCaps.dpcTriCaps;global_caps.hwCaps.dpcLineCaps.dwMiscCaps&=D3DPMISCCAPS_MASKZ;} /* v10.201: lines and points draw with every triangle state (CLXP3D T62-T64) */
 trace_inits++; trace_mode=mode; dx7_handles=0; memset((void*)trace_probe,0,sizeof(trace_probe)); rs_count=0; draw_count=0; shared_hal->vxD3DMode=mode;
if(mode&1) {
 diagnostic.enabled=1;
 shared_hal->d3dhal_flags.ddscaps=DDSCAPS_3DDEVICE|DDSCAPS_ZBUFFER|DDSCAPS_TEXTURE|DDSCAPS_MIPMAP; /* v10.169; v10.199: + MIPMAP */
 shared_hal->d3dhal_flags.caps2=DDCAPS2_CANRENDERWINDOWED;
 shared_hal->d3dhal_flags.zcaps=DDBD_16|DDBD_32; /* v10.177 */
 shared_hal->d3dhal_global=(DWORD)&global_caps;shared_hal->d3dhal_callbacks=(DWORD)&callbacks;
 shared_hal->cb32.GetDriverInfo=VX_D3D_GetDriverInfo;
} else {
 shared_hal->d3dhal_global=0;shared_hal->d3dhal_callbacks=0;
 }}
}
/* ABI facts checked against the pinned DDK declarations during development. */
const DWORD VXDD3DABI[]={sizeof(vx_ddi_create),sizeof(vx_ddi_dp2),offsetof(vx_ddi_dp2,ddrval),offsetof(vx_ddi_dp2,dwErrorOffset),
 sizeof(vx_ddi_callbacks),sizeof(vx_ddi_callbacks2),sizeof(vx_ddi_callbacks3),sizeof(vx_ddi_global),sizeof(vx_ddi_extended),sizeof(vx3dn_packet),sizeof(vx3dn_info),
 offsetof(DDRAWI_DDRAWSURFACE_MORE,dwSurfaceHandle),offsetof(DDRAWI_DDRAWSURFACE_MORE,ddsCapsEx),
 offsetof(DDHAL_CREATESURFACEEXDATA,ddRVal),offsetof(DDHAL_DESTROYDDLOCALDATA,ddRVal),(DWORD)&global_caps};
