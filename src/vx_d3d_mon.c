/* VXD3DMON v10.32 (driver .580: the build number). VXD3DMON v10.31 (driver .579: the build number). VXD3DMON v10.30 (driver .578: the build number). VXD3DMON v10.29 (driver .577: the build number). VXD3DMON v10.28 (driver .576: the build number). VXD3DMON v10.27 (driver .575: the build number). VXD3DMON v10.26 (driver .574: the build number). VXD3DMON v10.25 (driver .573: the build number). VXD3DMON v10.24 (driver .572: as v10.21; v10.22-v10.23's line for .570-.571's presents in the ring is gone with them). VXD3DMON v10.21 (driver .569: the wording of the .568 line). VXD3DMON v10.20 (driver .568: culled runs with a failing vertex, QueryPhases 354-355). VXD3DMON v10.19 (driver .567: culled lists and fans passed through, QueryPhases 350-353). VXD3DMON v10.18 (driver .566: indexed packets drawn after a wait for a slot, TEXDBG BC F0). VXD3DMON v10.17 (driver .564: v10.14-v10.16's lines for the template table removed with it; an indexed packet's 512 vertices). VXD3DMON v10.13 (driver .558: the test switch is gone; its word is no longer printed). VXD3DMON v10.12 (driver .557: the runs drawn by the vertices they use, QueryPhases 346-349). VXD3DMON v10.11 (driver .556: the lists taken whole, QueryPhases 344-345). VXD3DMON v10.10 (driver .555: the strips and fans passed through, QueryPhases 336-343). VXD3DMON v10.9 (driver .552: the answers kept for a state, QueryPhases 332-335; the 95th percentile frame time). VXD3DMON v10.8 (driver .551: ACCEL_D3D_HOLD removed, the hold line says so). VXD3DMON v10.7 (driver .550: the held queue's counts, QueryPhases 316-331). VXD3DMON v10.6 (driver .549: SAMPLE turns the VxD's word profiler off too). VXD3DMON v10.5 (driver .549: the VxD's own time by function and line, from VXVXD.MAP). VXD3DMON v10.4 (driver .548: sparse-run returns, the one-test checks, the T&L index snapshot, QueryPhases 312-315). VXD3DMON v10.3 (driver .547: the spans not checked again, QueryPhases 311). VXD3DMON v10.2 (driver .546: the indexed loop's off-screen drops and stops, QueryPhases 307-308). VXD3DMON v10.1 (driver .545: the pass-through span kernels line, QueryPhases 302-306). VXD3DMON v10.0 (driver .542: the HAL's 40 hottest 64-byte lines, each by its offset in VXDDRAW.DLL and its function + offset (to read
 * against the disassembly); the line report also for a program that never flips (VXTLP GRID draws to a window: all its samples). VXD3DMON v9.9 (driver .536: the driver's T&L, ACCEL_D3D_TNL: its counts, QueryPhases 290-301). VXD3DMON v9.8 (driver .534: SAMPLE, the sampler alone (the HAL's and the VxD's own timing off, so the shares are not inflated by their
 * clock reads); the render thread's ring-3 samples by 64-byte line: the HAL's by function from VXDDRAW.MAP beside the program, D3D8.DLL's by
 * function from its entry table (vx_d3d8_fns.h, the board's 4.09.00.0904), the rest by module and offset; the game's hottest pages from the
 * module that is an .EXE (v9.1-9.7 took Toolhelp's first module, which on Windows 98 need not be the game: D3D8.DLL's line came twice);
 * the T&L probe's counts (ACCEL_D3D_TNL, QueryPhases 270-289)). v9.7 (driver .533: the in-run A/B removed with VX_D3D_Switch; .531's kept-answers line removed). v9.6 (driver .531: IX384 always on, so no switch is left and the A/B is off; the rhw-0 primitives drawn with rhw 1 counted). v9.5 (driver .529: the A/B over ACCEL_D3D_IX384 (384 or 192 vertices an indexed packet); TEXRUNS and SEQSKIP always on, the engine's cull removed: their lines without the switch). v9.4 (driver .528: lighter while it watches: Windows' timer left at its own rate (timeBeginPeriod(1) was for the time-out sampler, removed in .524), the game's modules taken twice (when it is found and 15 s later) instead of every 10 s, the HAL's counters read every 250 ms instead of 50). v9.3 (driver .528: the A/B over TEXRUNS, SEQSKIP and HWCULL; the sequencer writes the batch calls left out; texture runs counted: the HAL's runs added and the template changes that still cut a packet, the VxD's packets drawn with runs and texture switches). v9.2 (driver .527: the A/B over HWCULL alone (VRECIPE, FASTMATCH, FASTVV, PFVV2, FASTCHK and TEXSHADOW removed, TEXHINT always on); D3D8.DLL picked by its full path (the system copy, not D3D8PROF's proxy), the game by its own executable; a module whose name another shares shown with its path. v9.1 driver .526: the A/B over VRECIPE, FASTMATCH, FASTVV, TEXHINT, PFVV2, HWCULL, FASTCHK, TEXSHADOW (SHADOW always on, IXCHK removed); the batch call's parts in the sampler's phases; the hottest pages of D3D8.DLL and GTA3.EXE; the texture shadow's and the screen's lines. v9.0 driver .525: the A/B over VRECIPE, FASTMATCH, FASTVV, TEXHINT, PFVV2, HWCULL and the new SHADOW and IXCHK (IXPASS, INFOCACHE, DELTA always on); the shadow's and the checked copies' lines; page faults and page-ins in the call-time table and for the calls of 10 ms and more. v8.9 driver .524: the A/B over the switches still undecided (PREPACK, INDEXED, TEXONLY, RINGBLT, RTNOWAIT, FASTCLOCK, ONECOPY, V8 always on; PAIRLIST, IXPF removed), + ACCEL_D3D_DELTA and its line; SMPCLK=PIT removed. v8.8 driver .523: the 8-word packets' line, ACCEL_D3D_V8 in the A/B. v8.7 driver .522: the readiness answers kept (ACCEL_D3D_INFOCACHE) and the pass-through's prefetch (ACCEL_D3D_IXPF), both in the A/B. v8.6 driver .521: the sampler on the RTC's interrupt (IRQ 8, 1024 a second) unless SMPCLK=PIT; the clock named. v8.5 driver .520: the sampler time-weighted (each sample as the time since the last), the gaps between its ticks by where they landed, the render thread inside the VxD by the VxD's phase. v8.4 driver .517: REC=n records n frames of DrawPrimitives2 (C:\VXD3D.REC, for the host replay) RECAT=s seconds after the start (60). v8.3 driver .515: the engine lock line (ACCEL_PRIVLOCK), the HAL's engine-cull and pass-through counts. v8.2 driver .513: the 16-bit holder by CS:IP block, named when it is the display driver's or DIBENG's. v8.1 driver .512: the critical-section holder while a 3D call waits; the state-cache misses read from their own codes again. v8.0 driver .504: the ring's off reason with its number, the reasons from vx_ring.h, the cache work by unit; v7.9, driver v10.474: the Blts by signature with their callers, why indexed-list triangles missed the loop, the CPU blits; v7.8, v10.473: triangles by draw command and path, the VxD's indexed packet time, retires by cause, Blts by kind; v7.7, v10.472: the indexed list loop's line; v7.6, v10.470: indexed packets (the line and the A/B); v7.5, v10.468: the burst-head line, PFVV2 in the A/B; v7.4, v10.467: PFVV out of the A/B, the ring waits by length; v7.3, v10.465: the ring-wait line; v7.2, v10.464: PFVV in the A/B; v7.1, v10.463: TEXHINT; v7.0, v10.462: a template build's parts; PFVERT gone from the A/B (10 settings again); v6.9: driver v10.461: ACCEL_D3D_PFVERT in the A/B (11 settings, a 22 s turn); the vertex copy's parts, its first copy against the same words copied again from the cache; v6.8: driver v10.455: ACCEL_D3D_FASTVV in the A/B (10 settings, a 20 s turn) and its count; v6.7: driver v10.454: AB=<switch> (AB=RINGBLT, AB=TEXONLY, ...): one switch off and on, 2 settings, a turn 4 s; Flip and Lock ms a frame in the A/B; v6.6: driver v10.453: ACCEL_D3D_RINGBLT in the all-switch A/B (9 settings, an 18 s turn), the render-target copies line, the HAL's token loop by part; v6.5: driver v10.452: the two-switch A/B pairs removed (AB=ALL measures them all); v6.4: driver v10.451: AB=ALL, the default: every speed switch in one run, each off in turn against all on; v6.3: driver v10.450: AB=TX (TEXONLY / ONECOPY); the textures-only set-ups line; v6.2: driver v10.449: AB=OC (ONECOPY / FASTCLOCK) for AB=QU (QDIRECT and FUSEDVAL removed); the copies line; -1 for a slow call's thread time not read; v6.1: driver v10.448: AB=CL (FASTCLOCK / PAIRLIST), paired differences in each turn, the VxD's time a packet per setting; v6.0: driver v10.447: the A/B pair chosen (AB=VF, AB=QU, AB=PL), medians of the periods, the VxD's copy-in and validation a packet; v5.9: driver v10.446: an A/B test of ACCEL_D3D_VRECIPE and ACCEL_D3D_FASTMATCH inside one run (NOAB: off); v5.8: driver v10.445: the batch sends apart in the fast path's slot opening; the VRECIPE and FASTMATCH switches; v5.7: driver v10.444: the fast path's parts, sampled; v5.6: driver v10.443: the HAL's batches built in the queue slot and the bytes copied into the queue; the VxD's bytes copied in and the prepacked blocks checked as copied; v5.5: driver v10.442: the pairs ACCEL_D3D_PAIRLIST kept in the list; v5.4: driver v10.441: the prepacked vertices (ACCEL_D3D_PREPACK) counted in the HAL and the VxD; triangles a frame by frame class; v5.3: driver v10.440: the HAL's geometry counters (source vertices, fast and general paths, strips, vertices written) and the VxD's command words by part, with the set-ups that repeat the last packet's; v5.2: driver v10.439: the render thread's VxD time measured; the stretches it did not run, by length and its last VxD call; v5.1: driver v10.438: the 16-bit and V86 samples by code selector; v5.0: driver v10.434: where the frame time went, by the VxD's sampler; the render thread's code by module; gameplay median and percentiles; v4.9: driver v10.433: the VxD's HAL lock read after the game ends; v4.8: driver v10.432: the HAL's DLL locked in memory, its pages and holders; v4.7: v4.6 printed the per-second line without its fps value, every field after it shifted; the gameplay fps as frames over time; v4.6: fps each second and the gameplay seconds' fps; v4.5 driver v10.431: DrawPrimitives2 in exclusive phases, calls by duration with the thread's run time, bad-vertex reasons; v4.4 driver v10.430: lines for removed experiments gone; v4.3 driver v10.429: what differed when the VxD state cache missed; v4.2 driver v10.418: clears in the ring, what retires the ring; v4.1 driver v10.417: flips in the ring; v4.0 driver v10.416: old-front waits left out for depth-only work; v3.9 driver v10.414: where a slow DrawPrimitives2 call's submit time went; v3.8 driver v10.412: the vertex runs written straight into the ring, the next packet prefetched; v3.7 driver v10.411: the MOVNTI ring writer; v3.6 driver v10.410: wraps without a wait, the space checks; v3.5 driver v10.408: the snooped ring's flush form; v3.4 driver v10.407: the XP ring's time a packet, Direct3D at the end, Locks of other processes; v3.3 driver v10.406: the old ring's lines removed; v3.2 driver v10.405: the first groups after a wrap; v3.1: VXDDRAW.DLL loaded from the system folder by its full path; v3.0 driver v10.403: the XP ring's first-start forms; v2.9 v10.402: XP's ring in system memory; v2.8 v10.400: the first-start probe; v2.7 v10.394: the slow single submits; v2.6 v10.393: the ring tape saved; v2.5 v10.392: the kick spacing; a game that ended before the start is not followed; v2.4 v10.391: the slow calls' longest commands; v2.3 v10.390: the write-combining flushes; v2.2 v10.389: stops when the game ends, thread run times; v2.1 v10.388: the VxD slow batch calls, program changes; v2.0 v10.387: the slow DrawPrimitives2 calls and Blts, kicks written again; v1.9 v10.386: the ring hang groups in full, DrawPrimitives2 calls a frame; v1.8 v10.385: each frame by HAL callback; v1.7 v10.384: the time between flips; v1.6 v10.383: the ring fence counters and its first hang; v1.5 v10.382: the fetcher starts with the gap written over; v1.4 v10.374; v1.3 v10.370; v1.2 v10.369 sent TEXDBG CB, which turned the VxD's constant groups off: wrong code; v1.1 v10.367; v1.0 v10.358): watches the Direct3D HAL while a game runs and records every primitive the HAL leaves out (state the
 * engine cannot draw: missing geometry in the game), every context marked lost and every engine recovery, with the time, the reason bits
 * (vx_d3d_unsupported_why) and the stage state at the last one; once a second a line of the rates (DrawPrimitives2 calls, engine packets).
 *
 * Start it, then the game (VXDDRAW.DLL's data is shared across processes: the counters are the game's). "VXD3DMON n" stops after n
 * seconds (default 600); a key in its console stops it at once. Results: VXD3DMON.TXT. Read-only: nothing is changed in the driver. */
#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include "vx_d3d_diag.h"
#include "vx_ring.h"
#include "vx_smp.h"
#include <stdlib.h>
#include <string.h>
/* v1.1: the VxD's own counters for the time watched (TEXDBG, as VXGL.LOG reads them; each read clears them): packets, microseconds a
 * packet in the VxD and its parts, the fetcher's building and waiting, the emitter's parts. The game's packets are all that runs then. */
#pragma aux mon_tsc = 0x0F 0x31 value [eax] modify [edx];
unsigned long mon_tsc(void);
static HANDLE vxd=INVALID_HANDLE_VALUE;
static int texdbg(DWORD code,DWORD *o) {DWORD in=code,got=0;memset(o,0,32);return vxd!=INVALID_HANDLE_VALUE&&DeviceIoControl(vxd,0x85500022UL,&in,4,o,32,&got,NULL);}
/* v1.6 (v10.383): TEXDBG 0xBC with an index: the ring's fence counters and its first hang */
static int texdbg2(DWORD code,DWORD i,DWORD *o) {DWORD in[2],got=0;in[0]=code;in[1]=i;memset(o,0,32);return vxd!=INVALID_HANDLE_VALUE&&DeviceIoControl(vxd,0x85500022UL,in,8,o,32,&got,NULL);}
static void ring_hang(FILE *f) {
 static const char *why[4]={"at the old position","with the gap written over","with the old position not known","with the gap left (ACCEL_D3D_GAPWIPE 0)"};
 DWORD o[8],h0[6],h1[6],k,j,n;
 if(!texdbg2(0xBC,0x80,o)||(o[0]&0xFF)!=0xBC)return;
 fprintf(f,"VxD ring fences %s: %lu put; %lu waits where the engine read idle before the fence came; %lu hangs in the ring; ring %s\n",o[6]?"on":"off (ACCEL_D3D_RINGFENCE 0)",o[2],o[3],o[4],o[5]?"turned OFF after its first hang":"still on");
 {DWORD r[8];if(texdbg2(0xBC,0x82,r)&&(r[0]&0xFF)==0xBC){

  fprintf(f,"VxD write-combining flushes before a fetch: %lu (%lu with SFENCE)\n",r[6],r[7]);

  {DWORD x[72],q[8],j;int ok=1;static const char *why[]=VX_XR_OFF_NAMES; /* v8.0: the VxD's list (vx_ring.h) */
   typedef char why_count_matches_vx_ring_h[sizeof(why)/sizeof(why[0])==VX_XR_OFF_N?1:-1];
   DWORD on=0;for(j=0;j<12&&ok;j++){ok=texdbg2(0xBC,0x86+j,q)&&(q[1]&0xFF)==0x86+j;if(ok){memcpy(x+6*j,q+2,24);if(!j)on=(q[1]>>8)&1;}}
   if(ok){static const char *st[4]={"OFF","not set up","set up, not running","running"};
    {char rs[160];rs[0]=0;if(!x[0])sprintf(rs,", reason %lu: %s",x[1],x[1]<VX_XR_OFF_N?why[x[1]]:"unknown to this VXD3DMON"); /* v8.0: the number with the text */
    fprintf(f,"VxD XP ring in system memory (ACCEL_D3D_XPRING %lu): %s%s; page table at video offset %08lX\n",on,st[x[0]&3],rs,x[23]);}
    fprintf(f,"   starts %lu (%lu from a paused fetcher, %lu not idle after it), kicks %lu, wraps %lu, stops %lu, 444 bit 31 clear while idle %lu, table/SR6A-6C written again %lu, hangs %lu (43C %08lX 444 %08lX)\n",x[2],x[3],x[4],x[5],x[6],x[7],x[8],x[9],x[13],x[14],x[15]);
    {DWORD u[8];int un=texdbg2(0xBC,0xD6,u)&&(u[1]&0xFF)==0xD6; /* v8.1 (.512): D6 */ /* v8.0 (driver .504): the cache work by unit (vx_ring.h); an older VxD: its one slot by the last mode */
     if(un)fprintf(f,"   cache work (last form %s, line %lu): CLFLUSH lines %lu, WBINVD calls %lu, MOVNTI words copied %lu; waits done with 0x400 bit 4 set at the pause: %lu\n",
      x[11]==5?"MOVNTI copy from staging":(x[11]&3)==1?"CLFLUSH+MFENCE":(x[11]&3)==2?"WBINVD":(x[11]&3)==3?"CLFLUSH":"not used",x[11]>>8,u[2+VX_XR_UNIT_LINES],u[2+VX_XR_UNIT_WBINVD],u[2+VX_XR_UNIT_NTWORDS],x[12]);
     else fprintf(f,"   cache flush %s, line %lu, %lu %s (an older VxD: one count, its unit the last form's); waits done with 0x400 bit 4 set at the pause: %lu\n",
      x[11]==5?"none (MOVNTI copy from staging)":(x[11]&3)==1?"CLFLUSH+MFENCE":(x[11]&3)==2?"WBINVD":(x[11]&3)==3?"CLFLUSH":"not used",x[11]>>8,x[10],
      x[11]==5?"words copied":(x[11]&3)==2?"WBINVD calls":(x[11]&3)?"cache lines flushed":"(unused)",x[12]);}
    fprintf(f,"   SR68 %02lX SR6A %02lX SR6B %02lX SR6C %02lX SR6D %02lX SR6E %02lX SR6F %02lX before; SR6A %02lX SR6B %02lX SR6C %02lX SR6F %02lX after (SR6F bit 7 %s)\n",
     x[16]&0xFF,(x[16]>>8)&0xFF,(x[16]>>16)&0xFF,x[16]>>24,x[17]&0xFF,(x[17]>>8)&0xFF,(x[17]>>16)&0xFF,x[18]&0xFF,(x[18]>>8)&0xFF,(x[18]>>16)&0xFF,x[18]>>24,x[19]==0xFFFFFFFFUL?"stayed set":x[19]?"cleared":"not polled");
    fprintf(f,"   first start at its pause: 400 %08lX 43C %08lX 444 %08lX 440 %08lX 45C %08lX 464 %08lX\n",x[20],x[21],x[22],x[24],x[25],x[26]);
    if(x[13])fprintf(f,"   last hang: 440 %08lX 45C %08lX, its group ended at %08lX\n",x[27],x[28],x[29]);
    {DWORD w2[8];if(texdbg2(0xBC,0x95,w2))fprintf(f,"   wraps written without a wait: %lu; space checks after them %lu, waits they took %lu; when the last post-wrap kick was written again: 43C %08lX 444 %08lX 400 %08lX\n",w2[2],w2[3],w2[4],w2[5],w2[6],w2[7]);} /* v3.6 */
    {DWORD w3[8];if(texdbg2(0xBC,0x96,w3))fprintf(f,"   one-pass block switch (ACCEL_D3D_FASTBLK) %lu; next packet prefetched (ACCEL_D3D_PREFETCH %lu): %lu packets, %lu lines\n",(w3[7]>>1)&1,(w3[7]>>2)&1,w3[5],w3[6]);} /* v3.8 */
    if(x[6])fprintf(f,"   first groups after a wrap: %lu done without a restart, %lu done after their kick was written again (%lu kicks again), %lu never done; before the last one's kick: 43C %08lX %08lX 444 %08lX 45C %08lX 464 %08lX 400 %08lX\n",x[68],x[71],x[69],x[70],x[62],x[63],x[64],x[65],x[66],x[67]);
    {static const char *fm[6]={"XP's (bit 24, SR6C bit 7 clear)","+ virtual queue (AGP form)","lab's (no bit 24, SR6C bit 7, regulator claim)","no bit 24, SR6C bit 7, virtual queue","no bit 24","SR6C bit 7"};DWORD t;
     fprintf(f,"   first-start forms tried %lu, kept: %s\n",x[61],x[60]==0xFF?"none":x[60]==0?fm[0]:x[60]==4?fm[1]:x[60]==0x0B?fm[2]:x[60]==7?fm[3]:x[60]==1?fm[4]:x[60]==2?fm[5]:"?");
     for(t=0;t<x[61]&&t<6;t++)fprintf(f,"     form %lu %-48s 400 %08lX 43C %08lX 444 %08lX 45C %08lX 464 %08lX\n",t,fm[t],x[30+5*t]&0x0FFFFFFFUL,x[31+5*t],x[32+5*t],x[33+5*t],x[34+5*t]);}}}

  }} /* v2.0; v2.1 */
 if(!(o[0]>>8))return;
 texdbg2(0xBC,0,o);for(k=0;k<6;k++)h0[k]=o[2+k];texdbg2(0xBC,1,o);for(k=0;k<6;k++)h1[k]=o[2+k];
 fprintf(f,"FIRST RING HANG (%.2f s after boot): status %08lX, register 440 %lu, fence %04lX, last group kicked %04lX (%s), %lu ms after the last kick, %lu groups kicked since the ring's start\n",
  h1[2]/1000.0,h0[0],h0[1],h0[2]&0xFFFF,h0[3],h0[2]==0xFFFFFFFFUL?"no fence":(h0[2]&0xFFFF)==h0[3]?"the fence reached it: the engine still busy":"groups not consumed",h0[4],h0[5]);
 fprintf(f,"   that ring started %s; %lu ring starts and %lu wraps before it; %lu fence waits before it; %lu groups kicked since boot\n",why[h1[3]&3],h1[0],h1[1],h1[4],h1[5]);
 fprintf(f,"   the groups before it, oldest first (seq; S first after a start, Y synchronous, F fenced; packets, words, vertices, start, pause, ms, 440 at its kick):\n");
 n=h1[5]<64?h1[5]:64;
 for(k=0;k<n;k++){DWORD e[6],x[4];texdbg2(0xBC,2+k,o);for(j=0;j<6;j++)e[j]=o[2+j];texdbg2(0xBC,0x100+k,o);for(j=0;j<4;j++)x[j]=o[2+j];
  fprintf(f,"     %04lX %c%c%c %3lu %5lu %5lu %08lX %08lX %5lu %lu\n",e[0]&0xFFFF,(e[0]>>16)&1?'S':'-',(e[0]>>19)&1?'Y':'-',(e[0]>>20)&1?'F':'-',e[0]>>24,e[3],e[4]&0xFFFF,e[1],e[2],e[4]>>16,e[5]);
  /* v1.9: the first packet (vx_native3d_vxd.c fx_note) */
  fprintf(f,"          first packet: op %lu, target fmt %lu at %08lX, depth fmt %lu at %08lX, %lu vertices, z en %lu wr %lu func %lu, blend %lu, textured %lu, units %lu, primitive %lu, dither %lu, stencil %lu, alpha func %lu, program %lu\n",
   x[0]&0xFF,(x[0]>>8)&0xFF,x[2],(x[0]>>16)&0xFF,x[3],x[0]>>24,x[1]&1,(x[1]>>1)&1,(x[1]>>4)&15,(x[1]>>8)&1,(x[1]>>9)&1,(x[1]>>10)&3,(x[1]>>12)&15,(x[1]>>16)&1,(x[1]>>17)&3,(x[1]>>20)&15,x[1]>>24);}
 {DWORD wn[4],w[1026],b;texdbg2(0xBC,0x81,o);for(j=0;j<4;j++)wn[j]=o[2+j];
  for(b=0;b<171;b++){texdbg2(0xBC,0x200+b,o);for(j=0;j<6;j++)w[b*6+j]=o[2+j];}
  for(b=0;b<2;b++){DWORD m=wn[b*2],*v=w+b*512;
   fprintf(f,"   %s (%lu words from %08lX):\n",b?"the fence's group (the last one consumed)":"the group after the fence (not consumed)",m,wn[b*2+1]);
   for(k=0;k<m;k++)fprintf(f,"%s%08lX%s",k%8?" ":"     ",v[k],(k%8==7||k+1==m)?"\n":"");}}
}
static double mon_mhz(void) {LARGE_INTEGER f,a,b;unsigned long c0,c1;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&a);c0=mon_tsc();
 do QueryPerformanceCounter(&b);while((b.QuadPart-a.QuadPart)*1000<f.QuadPart*50);c1=mon_tsc();return (double)(c1-c0)*f.QuadPart/1e6/(double)(b.QuadPart-a.QuadPart);}
/* v1.7 (v10.384): the time between flips (frames) and what the flips waited for */
static void frame_times(FILE *f,double mhz) {
 DWORD o[8],st[18],k,j;memset(st,0,sizeof(st));
 for(k=0;k<3;k++){if(!texdbg2(0xBE,k,o)||o[0]!=0xBE)return;for(j=0;j<6&&k*6+j<16;j++)st[k*6+j]=o[2+j];}
 texdbg2(0xBE,11,o);
 if(!st[0]){fprintf(f,"Frames: no flips counted\n");return;}
 fprintf(f,"Frames: %lu flips (FLIP_ASYNC %s; %lu returned before the blank), %.1f ms between flips on average, the longest %lu ms\n",st[0],o[2]?"on":"off",st[14],st[0]>1?(double)st[15]/(st[0]-1):0.0,st[9]);
 fprintf(f,"   time between flips: under 20 ms %lu, 20-35 %lu, 35-50 %lu, 50-75 %lu, 75-100 %lu, 100 ms and more %lu\n",st[1],st[2],st[3],st[4],st[5],st[6]);
 fprintf(f,"   in each flip: the wait for the engine %.2f ms on average (longest %.1f), the wait for the blank or the flip before %.2f ms (longest %.1f)\n",
  st[7]*256.0/mhz/1000.0/st[0],st[10]*256.0/mhz/1000.0,st[8]*256.0/mhz/1000.0/st[0],st[11]*256.0/mhz/1000.0);
 fprintf(f,"   draws, Locks and Blts that waited for the old front: %lu, %.2f ms in all\n",st[12],st[13]*256.0/mhz/1000.0);
 {DWORD w4[8];if(texdbg2(0xBC,0x97,w4))fprintf(f,"   depth-only draws and clears on the old front that went without the wait (ACCEL_D3D_FRONTZ %lu): %lu\n",w4[2],w4[3]);} /* v4.0 */
 {DWORD a[8],b[8];if(texdbg2(0xBC,0x98,a)&&texdbg2(0xBC,0x99,b)){
  fprintf(f,"   flips in the ring (ACCEL_FLIP_RING %lu): %lu, declined %lu (last reason %lu: 1 ring not running, 2 address or engine busy, 3 clocks off, 4 no room, 5 kick); CR32 bit 0 set %lu, cleared %lu; CPU flips after ring flips %lu\n",b[7],a[2],a[3],a[7],b[4],b[5],b[6]);
  fprintf(f,"   CPU waits for a ring flip to run: %lu, %.2f ms in all, the longest %.2f ms, ran out (100 ms) %lu; draws on the old front sent without a wait %lu\n",a[4],a[5]*256.0/mhz/1000.0,b[3]*256.0/mhz/1000.0,a[6],b[2]);}} /* v4.1 */
 {DWORD c[8];if(texdbg2(0xBC,0x9A,c))fprintf(f,"   clears in the ring (ACCEL_3D_RINGCLEAR %lu): %lu, declined %lu; retires of work in flight: for a CPU access (Lock, 2D, flip) %lu, %.1f ms in all; by a clear or submit outside the ring %lu, %.1f ms in all\n",
  (c[1]>>8)&1,c[2],c[3],c[4],c[5]*256.0/mhz/1000.0,c[6],c[7]*256.0/mhz/1000.0);} /* v4.2 */
 fprintf(f,"   the longest frames (ms between flips; the flip's engine wait, blank wait in ms):");
 for(k=0;k<8;k++){texdbg2(0xBE,3+k,o);if(o[2])fprintf(f," %lu (%.1f, %.1f)",o[2],o[3]*256.0/mhz/1000.0,o[4]*256.0/mhz/1000.0);}
 fprintf(f,"\n");
}
/* v1.8 (v10.385): where each frame's time went, by HAL callback (VXDDRAW.DLL VX_DD_QueryFrames) */
static DWORD (WINAPI *hal_frames)(DWORD *,DWORD);static DWORD (WINAPI *hal_blts)(DWORD *,DWORD),(WINAPI *hal_bsig)(DWORD *,DWORD); /* v7.8; v7.9: the Blts by signature */
static void slow_report(FILE *f,double cp);
static void hal_frame_report(FILE *f) {
 static const char *cat[6]={"DrawPrimitives2","Lock","Unlock","Blt","surface create/destroy","Flip"};
 DWORD o[116],k,j;double cp;
 if(!hal_frames||hal_frames(o,116)!=116){fprintf(f,"Frame parts: not available\n");return;}
 cp=o[0]/256.0;if(cp<=0)cp=1000000/256.0;
 slow_report(f,cp);
 fprintf(f,"Frame parts (HAL callbacks, %lu frames, TSC %lu cycles per ms):\n",o[1],o[0]);
 for(j=0;j<2;j++){DWORD *v=o+(j?10:3),n=j?o[2]:o[1];double whole=v[6]/cp,in=0;
  if(!n){fprintf(f,"   %s: none\n",j?"frames of 75 ms and more":"all frames");continue;}
  fprintf(f,"   %s: %lu, %.1f ms each:",j?"frames of 75 ms and more":"all frames",n,whole/n);
  for(k=0;k<6;k++){in+=v[k]/cp;fprintf(f," %s %.1f",cat[k],v[k]/cp/n);}
  fprintf(f,"; outside the driver's callbacks %.1f ms; the flipping thread ran %.1f ms\n",(whole-in)/n,(double)o[j?115:114]/n);}
 fprintf(f,"   the 8 longest frames (ms: whole; DrawPrimitives2, Lock, Unlock, Blt, create/destroy, Flip, outside; Locks, Unlocks, creates, DrawPrimitives2 calls; ms the flipping thread ran):\n");
 for(k=0;k<8;k++){DWORD *w=o+17+k*12;double in=0;if(!w[0])continue;
  fprintf(f,"     %.1f;",w[0]/cp);for(j=0;j<6;j++){in+=w[1+j]/cp;fprintf(f," %.1f",w[1+j]/cp);}
  fprintf(f," %.1f; %lu %lu %lu %lu; %lu\n",w[0]/cp-in,w[7],w[8],w[9],w[10],w[11]);}
 if(o[1])fprintf(f,"   DrawPrimitives2 calls: %.1f a frame on average\n",(double)o[113]/o[1]);
 {DWORD x2[120];if(hal_frames(x2,120)==119)fprintf(f,"   Lock/Unlock/create calls from other processes than the flipping one (not in the counts above): %lu, %lu, %lu\n",x2[116],x2[117],x2[118]);} /* v3.4 */
}
/* v4.6: frames a second (the HAL's flip count, read here once a second: nothing more in the game). The seconds with draws are kept by
 * fps, apart for those with 1000 engine packets a second or more (gameplay; menus and loading screens flip fast with few packets) */
static DWORD fs_n[2][7];static double fs_sum[2],fs_frames[2],fs_time[2];static DWORD fs_secs[2];
static double gs_fps[4096];static DWORD gs_n; /* v5.0: the gameplay seconds' fps, for the median and the 10th percentile */
static void fps_sec(double fps,DWORD packets,DWORD frames,DWORD ms) {int c=packets>=1000;if(c&&gs_n<4096)gs_fps[gs_n++]=fps;fs_frames[c]+=frames;fs_time[c]+=ms/1000.0;{int b=fps<5?0:fps<10?1:fps<15?2:fps<20?3:fps<30?4:fps<60?5:6;fs_n[c][b]++;fs_sum[c]+=fps;fs_secs[c]++;}}
static int dcmp(const void *a,const void *b) {double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y?1:0;}
static void fps_report(FILE *f) {
static const char *bn[7]={"under 5","5-10","10-15","15-20","20-30","30-60","60+"};int c,b;
 if(gs_n){qsort(gs_fps,gs_n,sizeof(double),dcmp);fprintf(f,"Gameplay seconds: median %.1f fps, 10th percentile %.1f fps, the slowest %.1f, the fastest %.1f (%lu seconds)\n",gs_fps[gs_n/2],gs_fps[gs_n/10],gs_fps[0],gs_fps[gs_n-1],gs_n);}
 for(c=1;c>=0;c--){if(!fs_secs[c])continue;fprintf(f,"Seconds %s: %lu, %.0f frames in %.1f s = %.1f fps; by fps:",c?"with 1000 engine packets/s or more (gameplay)":"with fewer (menus, loading)",fs_secs[c],fs_frames[c],fs_time[c],fs_time[c]>0?fs_frames[c]/fs_time[c]:0.0);
  for(b=0;b<7;b++)if(fs_n[c][b])fprintf(f," %s %lu;",bn[b],fs_n[c][b]);fprintf(f,"\n");}}
/* v5.0 (v10.434): the VxD's sampler (vx_smp.h): started with the watch, stopped and read at the end; the game's modules taken with
 * Toolhelp when the game is found and 15 s later (v9.4; every 10 s before) to name the sampled pages */
typedef struct {DWORD dwSize,th32ModuleID,th32ProcessID,GlblcntUsage,ProccntUsage;BYTE *modBaseAddr;DWORD modBaseSize;HMODULE hModule;char szModule[256];char szExePath[260];} vx_me32;
static struct {DWORD base,size;char name[32],path[260];} mods[160];static int nmods; /* v9.2 (.527): path, the module's full path (two modules can share a name: D3D8PROF's D3D8.DLL proxy and the system's) */
static void mod_snap(DWORD pid) {
 HMODULE k=GetModuleHandleA("KERNEL32.DLL");HANDLE (WINAPI *snap)(DWORD,DWORD);BOOL (WINAPI *first)(HANDLE,vx_me32 *),(WINAPI *next)(HANDLE,vx_me32 *);HANDLE s;vx_me32 e;
 if(!k)return;snap=(HANDLE (WINAPI *)(DWORD,DWORD))GetProcAddress(k,"CreateToolhelp32Snapshot");first=(BOOL (WINAPI *)(HANDLE,vx_me32 *))GetProcAddress(k,"Module32First");next=(BOOL (WINAPI *)(HANDLE,vx_me32 *))GetProcAddress(k,"Module32Next");
 if(!snap||!first||!next)return;s=snap(8,pid);if(s==INVALID_HANDLE_VALUE)return; /* TH32CS_SNAPMODULE */
 memset(&e,0,sizeof(e));e.dwSize=sizeof(e);
 if(first(s,&e))do{int j,have=0;for(j=0;j<nmods;j++)if(mods[j].base==(DWORD)e.modBaseAddr)have=1;
  if(!have&&nmods<160){mods[nmods].base=(DWORD)e.modBaseAddr;mods[nmods].size=e.modBaseSize;strncpy(mods[nmods].name,e.szModule,31);mods[nmods].name[31]=0;strncpy(mods[nmods].path,e.szExePath,259);mods[nmods].path[259]=0;nmods++;}
  e.dwSize=sizeof(e);}while(next(s,&e));
 CloseHandle(s);
}
static int mod_of(DWORD a) {int j;for(j=0;j<nmods;j++)if(a-mods[j].base<mods[j].size)return j;return -1;}
static vx_smp smpr;static int smp_ok; /* v8.9: SMPCLK=PIT removed (the driver's RTC clock alone) */
static void smp_start(HMODULE m) {
 DWORD in[4],o[2],got=0,pe;const BYTE *b=(const BYTE*)m;
 if(vxd==INVALID_HANDLE_VALUE||!b)return;pe=*(const DWORD*)(b+0x3C);
 in[0]=1;in[1]=(DWORD)b;in[2]=*(const DWORD*)(b+pe+24+56);in[3]=1;smp_ok=DeviceIoControl(vxd,VX_IOCTL_SAMPLE,in,16,o,8,&got,0)&&got==8&&o[0]==VX_SMP_VERSION&&o[1];
}
static void smp_stop_read(void) {DWORD in[3]={0,0,0},got=0;if(!smp_ok)return;smp_ok=DeviceIoControl(vxd,VX_IOCTL_SAMPLE,in,12,&smpr,sizeof(smpr),&got,0)&&got==sizeof(smpr)&&smpr.version==VX_SMP_VERSION;}
static void smp_report(FILE *f) {
 static const char *cn[VX_SMP_CATS]={"render thread in the VxD waiting for the engine","render thread in the VxD otherwise","render thread in VXDDRAW.DLL (the HAL)",
  "render thread in other 32-bit code (game, runtime)","render thread in 16-bit or V86 code","other threads, 32-bit code","other threads, 16-bit or V86 code","no render thread yet"};
 static const char *gn[3]={"gameplay frames under 75 ms","gameplay frames of 75 ms and more","other frames (fewer than 50 draw packets: menus, loading)"};
 DWORD g,k,tot,eng,run;double each,secs;
 if(!smp_ok){fprintf(f,"Sampler: not available (an older driver, or IRQ 8 / the RTC's periodic interrupt taken)\n");return;}
 secs=(smpr.stop_ms-smpr.start_ms)/1000.0;
 fprintf(f,"Where the frame time went (the VxD's sampler on %s: %lu samples in %.1f s, one every %.2f ms; %lu ms late (interrupts off); the sampler's own time %.1f ms):\n",
  smpr.clk==1?"the RTC's interrupt (IRQ 8)":"the VMM's time-outs (the counted split is not reliable: VXSMPCAL v1.0)",smpr.samples,secs,smpr.samples?secs*1000.0/smpr.samples:0.0,smpr.late_ms,smpr.tick_cycles*256.0/1000000.0);
 for(g=0;g<3;g++){if(!smpr.frames[g])continue;tot=0;eng=0;for(k=0;k<VX_SMP_CATS;k++){tot+=smpr.cat[g][k];eng+=smpr.eng[g][k];}if(!tot)continue;
  each=(double)smpr.frame_ms[g]/smpr.frames[g];run=smpr.cat[g][1]+smpr.cat[g][2]+smpr.cat[g][3]+smpr.cat[g][4];
  fprintf(f,"   %s: %lu, %.1f ms each; the engine busy %.0f%% of it; the render thread running %.0f%% (in the VxD waiting for the engine %.0f%%), other threads %.0f%%\n",gn[g],smpr.frames[g],each,
   100.0*eng/tot,100.0*run/tot,100.0*smpr.cat[g][0]/tot,100.0*(smpr.cat[g][5]+smpr.cat[g][6])/tot);
  {DWORD fo[116];double cpms=hal_frames&&hal_frames(fo,116)==116&&fo[0]?fo[0]:1000000.0;
   fprintf(f,"      its time inside the VxD, measured on the way in and out: %.1f ms a frame (%.0f%%; sampled %.0f%%)\n",smpr.vxd_cyc[g]*256.0/cpms/smpr.frames[g],100.0*smpr.vxd_cyc[g]*256.0/cpms/smpr.frames[g]/each,100.0*(smpr.cat[g][0]+smpr.cat[g][1])/tot);} /* v5.2 */
  fprintf(f,"      triangles sent: %.0f a frame (%.1f microseconds of the frame each)\n",(double)smpr.tri[g]/smpr.frames[g],smpr.tri[g]?1000.0*smpr.frame_ms[g]/smpr.tri[g]:0.0); /* v5.4 */
  if(smpr.run_n[g])fprintf(f,"      its flipping thread's own run time: %.1f ms a frame (%.0f%% of the frame; sampled as running %.0f%%: the difference is time it was charged while blocked or idle)\n",
   (double)smpr.run_ms[g]/smpr.run_n[g],100.0*smpr.run_ms[g]/smpr.run_n[g]/each,100.0*run/tot);
  for(k=0;k<VX_SMP_CATS;k++)if(smpr.cat[g][k])fprintf(f,"      %-52s %6.1f ms a frame (%4.1f%%), the engine busy in %3.0f%% of it\n",cn[k],each*smpr.cat[g][k]/tot,100.0*smpr.cat[g][k]/tot,100.0*smpr.eng[g][k]/smpr.cat[g][k]);
  {double tt=0,kh=smpr.tsc_khz?smpr.tsc_khz:1000000.0;for(k=0;k<VX_SMP_CATS;k++)tt+=smpr.tw[g][k]; /* v8.5 (.520): each sample as the time since the one before */
   if(tt>0){fprintf(f,"      time-weighted (v8.5: each sample stands for the time since the last; %.1f ms a frame sampled this way):",tt*1024.0/kh/smpr.frames[g]);
    for(k=0;k<VX_SMP_CATS;k++)if(smpr.tw[g][k])fprintf(f," %s %.1f ms (%.1f%%);",k==0?"VxD waiting":k==1?"VxD otherwise":k==2?"HAL":k==3?"other 32-bit":k==4?"16-bit":k==5?"other threads 32-bit":k==6?"other threads 16-bit":"no render thread",
     smpr.tw[g][k]*1024.0/kh/smpr.frames[g],100.0*smpr.tw[g][k]/tt);
    fprintf(f,"\n");}}}
 { /* v8.5 (.520): the gaps between the sampler's ticks by where the tick landed, and the VxD's phases */
  static const char *ln[4]={"render thread, ring 3 (HAL, game, runtime, 16-bit)","render thread inside the VxD","other threads","no render thread yet"},
   *bn[8]={"<0.5","0.5-0.9","0.9-1.1","1.1-1.5","1.5-2.5","2.5-5","5-20","20+"},
   *pn[14]={"phase not set","other calls (Lock, flip, query, 2D, status)","submit: the call's dispatch, outside the batch (.526)","copy-in","validation","building the words","starting a group (flush or copy, fence, kick)","engine and ring waits","single submits and clears","VXSMPCAL's spin","batch entry: lock, busy checks, mapping","batch: sequencer opened, status, fetcher idle","batch: between packets (cursor, groups, counts)","batch: after the packets (status, sequencer closed, cursor, lock)"}; /* v9.1 (.526): 10-13 the batch's parts */
  double kh=smpr.tsc_khz?smpr.tsc_khz:1000000.0,all=smpr.tw_total?smpr.tw_total:1;DWORD l,b2,n;
  fprintf(f,"   the sampler's ticks (v8.5): TSC %.1f MHz; the gap before each tick by where it landed (ms: count), and the time those gaps stand for:\n",kh/1000.0);
  for(l=0;l<4;l++){n=0;for(b2=0;b2<8;b2++)n+=smpr.gap_n[l][b2];if(!n)continue;
   fprintf(f,"      %-52s %7lu ticks, %.3f ms each on average, %.1f%% of the time:",ln[l],n,smpr.gap_t[l]*1024.0/kh/n,100.0*smpr.gap_t[l]/all);
   for(b2=0;b2<8;b2++)if(smpr.gap_n[l][b2])fprintf(f," %s %lu;",bn[b2],smpr.gap_n[l][b2]);fprintf(f,"\n");}
  n=0;for(l=0;l<16;l++)n+=smpr.ph_n[l];
  if(n){double pt=0;for(l=0;l<16;l++)pt+=smpr.ph_t[l];
   fprintf(f,"   the render thread inside the VxD by the VxD's phase (samples counted / time-weighted):");
   for(l=0;l<16;l++)if(smpr.ph_n[l])fprintf(f," %s %.1f%% / %.1f%%;",l<14?pn[l]:"?",100.0*smpr.ph_n[l]/n,pt>0?100.0*smpr.ph_t[l]/pt:0.0);fprintf(f,"\n");}}
 /* the render thread's 32-bit code by module (gameplay frames) */
 {static double mn[161][2];static DWORD on[161][2];int j;DWORD c,t2[2]={0,0};
  memset(mn,0,sizeof(mn));for(g=0;g<2;g++)for(k=0;k<VX_SMP_CATS;k++)t2[g]+=smpr.cat[g][k];
  for(k=0;k<VX_SMP_PAGES;k++)if(smpr.page[k]){j=mod_of(smpr.page[k]<<12);if(j<0)j=160;for(g=0;g<2;g++)mn[j][g]+=smpr.pn[k][g];}
  for(g=0;g<2;g++)mn[160][g]+=smpr.page_other[g];
  fprintf(f,"   the render thread's 32-bit code in gameplay frames, by module (%% of all the time of frames under 75 ms / of 75 ms and more):\n");
  for(c=0;c<12;c++){int best=-1;for(j=0;j<161;j++)if((mn[j][0]+mn[j][1])>0&&(best<0||mn[j][0]+mn[j][1]>mn[best][0]+mn[best][1]))best=j;if(best<0)break;
   {int d=0;if(best<160)for(j=0;j<nmods;j++)if(j!=best&&!_stricmp(mods[j].name,mods[best].name))d=1; /* v9.2: a shared name is followed by the full path */
   fprintf(f,"      %-24s %5.1f%% / %5.1f%%%s%s\n",best==160?"(outside the modules seen)":mods[best].name,t2[0]?100.0*mn[best][0]/t2[0]:0.0,t2[1]?100.0*mn[best][1]/t2[1]:0.0,d?"  ":"",d?mods[best].path:"");}mn[best][0]=mn[best][1]=-1;}
  { /* v9.1 (.526): the hottest pages of D3D8.DLL and GTA3.EXE (the render thread, gameplay frames), as offsets from the module's base:
     with the DLL itself they name the routines (is Direct3D 8's vertex processing its SSE code or the plain x87 code on this CPU?) */
   /* v9.2 (.527): Direct3D 8 picked by its full path, the system directory's D3D8.DLL (.526 took the last module named D3D8.DLL: D3D8PROF's
    * proxy in the game's folder); the game by the process's own executable (Toolhelp lists it first), whatever its name */
   char sysd[260],want[2][260];int w;UINT sl=GetSystemDirectoryA(sysd,sizeof(sysd));
   if(sl&&sl<sizeof(sysd)-10)sprintf(want[0],"%s\\D3D8.DLL",sysd);else strcpy(want[0],"C:\\WINDOWS\\SYSTEM\\D3D8.DLL");
   want[1][0]=0;for(j=0;j<nmods;j++){size_t l=strlen(mods[j].path);if(l>4&&!_stricmp(mods[j].path+l-4,".EXE")){strcpy(want[1],mods[j].path);break;}} /* v9.8: the game, the module that is an .EXE */
   for(w=0;w<2;w++){int mj=-1;DWORD hot[16],hn[16],nh=0,tot=t2[0]+t2[1];for(j=0;j<nmods;j++)if(want[w][0]&&!_stricmp(mods[j].path,want[w]))mj=j;
    if(mj<0){if(!w)fprintf(f,"   %s: not among the game's modules\n",want[0]);continue;}if(!tot)continue;
    for(k=0;k<VX_SMP_PAGES;k++)if(smpr.page[k]&&mod_of(smpr.page[k]<<12)==mj){DWORD n=smpr.pn[k][0]+smpr.pn[k][1],x;
     if(nh<16){hot[nh]=smpr.page[k];hn[nh]=n;nh++;}else{DWORD lo=0;for(x=1;x<16;x++)if(hn[x]<hn[lo])lo=x;if(n>hn[lo]){hot[lo]=smpr.page[k];hn[lo]=n;}}}
    if(!nh)continue;
    fprintf(f,"   %s's hottest pages (offset from its base %08lX, %% of all the time of gameplay frames):",mods[mj].path,mods[mj].base);
    {DWORD x,y;for(x=0;x<nh;x++)for(y=x+1;y<nh;y++)if(hn[y]>hn[x]){DWORD t=hn[x];hn[x]=hn[y];hn[y]=t;t=hot[x];hot[x]=hot[y];hot[y]=t;}
     for(x=0;x<nh;x++)fprintf(f," +%05lX %.1f%%;",(hot[x]<<12)-mods[mj].base,100.0*hn[x]/tot);}fprintf(f,"\n");}}
  memset(on,0,sizeof(on));for(k=0;k<VX_SMP_OPAGES;k++)if(smpr.opage[k]){j=mod_of(smpr.opage[k]<<12);if(j<0)j=160;for(g=0;g<2;g++)on[j][g]+=smpr.opn[k][g];}
  fprintf(f,"   other threads in gameplay frames, shared-arena modules (%% as above); in a process's own memory %.1f%% / %.1f%%:\n",t2[0]?100.0*smpr.opage_other[0]/t2[0]:0.0,t2[1]?100.0*smpr.opage_other[1]/t2[1]:0.0);
  for(c=0;c<6;c++){int best=-1;for(j=0;j<161;j++)if(on[j][0]+on[j][1]&&on[j][0]+on[j][1]!=0xFFFFFFFEUL&&(best<0||on[j][0]+on[j][1]>on[best][0]+on[best][1]))best=j;if(best<0)break;
   fprintf(f,"      %-24s %5.1f%% / %5.1f%%\n",best==160?"(outside the modules seen)":mods[best].name,t2[0]?100.0*on[best][0]/t2[0]:0.0,t2[1]?100.0*on[best][1]/t2[1]:0.0);on[best][0]=on[best][1]=0xFFFFFFFFUL;}}
 {int k2;DWORD any=smpr.v86n[0]+smpr.v86n[1]+smpr.cs16_other;for(k2=0;k2<16;k2++)any+=smpr.cs16n[k2][0]+smpr.cs16n[k2][1];
  if(any){fprintf(f,"   16-bit and V86 samples (all frames; ms, render thread / other threads): V86 %lu / %lu;",smpr.v86n[0],smpr.v86n[1]);
   for(k2=0;k2<16;k2++)if(smpr.cs16[k2])fprintf(f," CS %04lX %lu / %lu;",smpr.cs16[k2],smpr.cs16n[k2][0],smpr.cs16n[k2][1]);
   fprintf(f," other selectors %lu\n",smpr.cs16_other);}} /* v5.1 */
 {static const char *bb[8]={"1","2-5","6-10","11-15","16-20","21-50","51-200","201+"};int k2; /* v5.2 */
  fprintf(f,"   stretches with the render thread not running (all frames): %.1f s in all, %.1f s of it V86 code; by length (ms):",smpr.blk_ms/1000.0,smpr.blk_v86ms/1000.0);
  for(k2=0;k2<8;k2++)fprintf(f," %s %lu;",bb[k2],smpr.blk_hist[k2]);fprintf(f,"\n      by its last VxD call before them:");
  for(k2=0;k2<8;k2++)if(smpr.blk_code_n[k2])fprintf(f," %08lX %lu (%.1f s);",smpr.blk_code[k2],smpr.blk_code_n[k2],smpr.blk_code_ms[k2]/1000.0);fprintf(f,"\n");
  if(smpr.tt_full)fprintf(f,"   (calls the sampler's thread table could not hold: %lu)\n",smpr.tt_full);}
 /* gameplay frame times */
 {DWORD n=0,s=0,p[4]={0,0,0,0},want[4],i;double ms=0;
  for(i=0;i<VX_SMP_HIST;i++){n+=smpr.hist[i];ms+=(double)i*smpr.hist[i];}
  if(n){want[0]=n/2;want[1]=n*9/10;want[2]=n*99/100;want[3]=n*95/100;for(k=0;k<4;k++){s=0;for(i=0;i<VX_SMP_HIST;i++){s+=smpr.hist[i];if(s>want[k]){p[k]=i;break;}}}
   fprintf(f,"   gameplay frame times (%lu frames, %.1f ms on average = %.1f fps): median %lu ms (%.1f fps), 90th percentile %lu ms (%.1f fps), 95th percentile %lu ms (%.1f fps), 99th percentile %lu ms (%.1f fps)\n",
    n,ms/n,ms?1000.0*n/ms:0.0,p[0],p[0]?1000.0/p[0]:0.0,p[1],p[1]?1000.0/p[1]:0.0,p[3],p[3]?1000.0/p[3]:0.0,p[2],p[2]?1000.0/p[2]:0.0);}} /* v10.9: the 95th percentile */
}
/* v9.8 (.534): the render thread's ring-3 samples in gameplay frames by 64-byte line (vx_smp.line, ln). VXDDRAW.DLL's lines named by
 * function from VXDDRAW.MAP (the build's linker map, beside this program: its _DriverInit@4 checked against the loaded DLL's DriverInit
 * export, so a map of another build is not used); D3D8.DLL's (the system folder's) by the function entries in vx_d3d8_fns.h when the file is
 * that build (PE time and image size); every other line by its module and offset. Percentages of all the time of gameplay frames. */
#include "vx_d3d8_fns.h"
static struct {DWORD a;char n[48];} *hsym;static int nhsym;static long hsym_adj;static char hsym_why[160];
static int hsym_cmp(const void *x,const void *y) {DWORD a=*(const DWORD*)x,b=*(const DWORD*)y;return a<b?-1:a>b?1:0;}
static void hal_map(HMODULE m) {
 char path[MAX_PATH],*q,l[512];FILE *mf;DWORD lo=0,hi=0,mapinit=0;int cap=0;
 GetModuleFileName(0,path,sizeof(path));q=strrchr(path,'\\');if(q)strcpy(q+1,"VXDDRAW.MAP");else strcpy(path,"VXDDRAW.MAP");
 mf=fopen(path,"r");if(!mf){sprintf(hsym_why,"%s not found: the HAL's lines by offset only",path);return;}
 while(fgets(l,sizeof(l),mf)){DWORD a,sz;char nm[128],cl[64];
  if(sscanf(l,"_TEXT %63s AUTO %lx %lx",cl,&a,&sz)==3&&!strcmp(cl,"CODE")){lo=a;hi=a+sz;continue;}
  if(strlen(l)>16&&sscanf(l,"%8lx",&a)==1&&l[8]&&strchr("s+* ",l[8])&&sscanf(l+9,"%127s",nm)==1){
   if(!strcmp(nm,"_DriverInit@4"))mapinit=a;
   if(lo&&a>=lo&&a<hi){if(nhsym>=cap){cap=cap?cap*2:1024;hsym=realloc(hsym,cap*sizeof(*hsym));if(!hsym){nhsym=0;break;}}
    hsym[nhsym].a=a;strncpy(hsym[nhsym].n,nm,47);hsym[nhsym].n[47]=0;nhsym++;}}}
 fclose(mf);
 {FARPROC di=GetProcAddress(m,"DriverInit");
  if(!mapinit||!di||!nhsym){sprintf(hsym_why,"%s does not name _DriverInit@4 or holds no code: not used",path);nhsym=0;return;}
  hsym_adj=(long)((DWORD)di-mapinit);}
 qsort(hsym,nhsym,sizeof(*hsym),hsym_cmp);
 sprintf(hsym_why,"%d functions from %s",nhsym,path);
}
static int hsym_of(DWORD addr) {int lo=0,hi=nhsym-1,r=-1;DWORD a=addr-(DWORD)hsym_adj;while(lo<=hi){int mid=(lo+hi)/2;if(hsym[mid].a<=a){r=mid;lo=mid+1;}else hi=mid-1;}return r;}
static int d3d8_ok;
static void d3d8_check(const char *path) {
 FILE *df=fopen(path,"rb");unsigned char h[1024];DWORD pe;d3d8_ok=0;if(!df)return;
 if(fread(h,1,sizeof(h),df)==sizeof(h)){pe=*(DWORD*)(h+0x3C);if(pe<sizeof(h)-120&&*(DWORD*)(h+pe+8)==VX_D3D8_TIME&&*(DWORD*)(h+pe+24+56)==VX_D3D8_SIZE)d3d8_ok=1;}
 fclose(df);
}
static int d3d8_fn(DWORD rva) {int lo=0,hi=VX_D3D8_NFN-1,r=-1;while(lo<=hi){int mid=(lo+hi)/2;if(vx_d3d8_fn[mid]<=rva){r=mid;lo=mid+1;}else hi=mid-1;}return r;}
static const char *d3d8_name(DWORD fn) { /* the entries the D3D8.DLL disassembly (November 2026) described */
 switch(fn){case 0xA1D30:return " (SIMD transform/state set-up)";case 0xAE080:return " (clip: interpolated vertex)";case 0xB1630:return " (perspective divide, viewport)";
  case 0xB22A0:return " (indexed geometry assembly)";case 0xB4F80:return " (holds the SSE2 colour packing at +B5036)";case 0xB6860:return " (holds the x87 matrix accumulation at +B70B8)";default:return "";}
}
typedef struct {DWORD key,n;} kn;
static int kn_cmp(const void *x,const void *y) {DWORD a=((const kn*)x)->n,b=((const kn*)y)->n;return a>b?-1:a<b?1:0;}
/* v10.5 (.549): the VxD's own time by function: the render thread's samples inside VX85598.VXD (vline, vln) named from VXVXD.MAP beside
 * this program (the build's VX85598_VXD.map: lines "0001:OOOOOOOO name" in the code object); the load base is smpr.vxd_ref less the map's
 * _smp_tick@8 offset */
typedef struct {DWORD off;char name[48];} vsym;
static int vsym_cmp(const void *a,const void *b) {DWORD x=((const vsym*)a)->off,y=((const vsym*)b)->off;return x<y?-1:x>y;}
static void vxd_line_report(FILE *f) {
 static vsym vs[4096];static struct {DWORD key,n;} fn[4096],ln[VX_SMP_VLINES];int nvs=0,nf=0,nl=0,k,j;DWORD tick=0xFFFFFFFFUL,base,tot=0,size=0;char path[MAX_PATH],*s,l[256];FILE *mf;
 if(!smp_ok)return;
 for(k=0;k<VX_SMP_VLINES;k++)tot+=smpr.vln[k];
 fprintf(f,"   the render thread inside VX85598.VXD by 64-byte line (.549): %lu samples in lines, %lu without a slot, %lu with no frame found, %lu in other ring-0 code (VMM, other VxDs)\n",tot,smpr.vline_other,smpr.vline_miss,smpr.vline_vmm);
 if(!tot)return;
 GetModuleFileNameA(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXVXD.MAP");
 mf=fopen(path,"r");
 if(mf){while(fgets(l,sizeof(l),mf)){unsigned seg;unsigned long off,sz;char nm[64],cls[32],grp[32];
   if(sscanf(l,"_TEXT %31s %31s %x:%lx %lx",cls,grp,&seg,&off,&sz)==5&&seg==1){size=(DWORD)(off+sz);continue;}
   {int at=0;if(sscanf(l,"%x:%lx%n",&seg,&off,&at)==2&&at>0){const char *q=l+at;if(*q=='+'||*q=='*'||*q=='s')q++;while(*q==' ')q++; /* the marker right after the offset, then the name */
    if(sscanf(q,"%63s",nm)!=1)seg=0;}else seg=0;}
   if(seg==1&&nvs<4096){vs[nvs].off=(DWORD)off;strncpy(vs[nvs].name,nm,47);vs[nvs].name[47]=0;if(!strcmp(nm,"_smp_tick@8"))tick=(DWORD)off;nvs++;}}
  fclose(mf);qsort(vs,nvs,sizeof(vs[0]),vsym_cmp);}
 if(tick==0xFFFFFFFFUL){fprintf(f,"      (VXVXD.MAP not beside VXD3DMON.EXE, or without _smp_tick@8: lines by address only)\n");base=0;}else base=smpr.vxd_ref-tick;
 for(k=0;k<VX_SMP_VLINES;k++){DWORD n=smpr.vln[k],a,key;int lo,hi,x;if(!smpr.vline[k]||!n)continue;a=smpr.vline[k]<<6;
  if(nl<VX_SMP_VLINES){ln[nl].key=a;ln[nl].n=n;nl++;}
  key=0xFFFFFFFFUL;if(base&&a-base<(size?size:0x100000UL)&&nvs){lo=0;hi=nvs-1;while(lo<hi){int mid=(lo+hi+1)/2;if(vs[mid].off<=a-base)lo=mid;else hi=mid-1;}if(vs[lo].off<=a-base)key=(DWORD)lo;}
  for(x=0;x<nf&&fn[x].key!=key;x++);if(x==nf){if(nf>=4096)continue;fn[nf].key=key;fn[nf].n=0;nf++;}fn[x].n+=n;}
 for(k=0;k<nf;k++)for(j=k+1;j<nf;j++)if(fn[j].n>fn[k].n){DWORD t1=fn[k].key,t2=fn[k].n;fn[k]=fn[j];fn[j].key=t1;fn[j].n=t2;}
 for(k=0;k<nl;k++)for(j=k+1;j<nl;j++)if(ln[j].n>ln[k].n){DWORD t1=ln[k].key,t2=ln[k].n;ln[k]=ln[j];ln[j].key=t1;ln[j].n=t2;}
 fprintf(f,"      by function (%% of the VxD's line samples):\n");
 for(k=0;k<nf&&k<40;k++)fprintf(f,"         %-34s %6.2f%%\n",fn[k].key==0xFFFFFFFFUL?"(outside the map)":vs[fn[k].key].name,100.0*fn[k].n/tot);
 fprintf(f,"      the hottest lines (offset in the code object; function + offset):\n");
 for(k=0;k<nl&&k<40;k++){DWORD a=ln[k].key,o=base?a-base:a;int lo=0,hi=nvs-1;
  if(base&&nvs&&o<(size?size:0x100000UL)){while(lo<hi){int mid=(lo+hi+1)/2;if(vs[mid].off<=o)lo=mid;else hi=mid-1;}fprintf(f,"         +%05lX  %s+%lX   %.2f%%\n",o,vs[lo].name,o-vs[lo].off,100.0*ln[k].n/tot);}
  else fprintf(f,"         %08lX   %.2f%%\n",a,100.0*ln[k].n/tot);}
}
static void line_report(FILE *f,HMODULE m) {
 static kn hal[4096],d8[4096],oth[VX_SMP_LINES],hl[VX_SMP_LINES];int nh=0,nd=0,no=0,nl=0,k,j,d8m=-1;DWORD tot=0,g,c,inh=0,ind=0,ino=0,lines=0;char sysd[260],dp[280];UINT sl;
 if(!smp_ok)return;
 for(g=0;g<2;g++)for(c=0;c<VX_SMP_CATS;c++)tot+=smpr.cat[g][c];
 if(!tot)for(k=0;k<VX_SMP_LINES;k++)tot+=smpr.ln[k]; /* v10.0: no frames counted (a program that never flips): every line sample */
 if(!tot)return;
 sl=GetSystemDirectoryA(sysd,sizeof(sysd));if(sl&&sl<sizeof(sysd)-10)sprintf(dp,"%s\\D3D8.DLL",sysd);else strcpy(dp,"C:\\WINDOWS\\SYSTEM\\D3D8.DLL");
 for(j=0;j<nmods;j++)if(!_stricmp(mods[j].path,dp))d8m=j;
 if(d8m>=0)d3d8_check(dp);
 hal_map(m);
 for(k=0;k<VX_SMP_LINES;k++){DWORD a,n=smpr.ln[k];int i,x;if(!smpr.line[k]||!n)continue;a=smpr.line[k]<<6;lines+=n;
  if(a-smpr.hal_base<smpr.hal_size){DWORD key=a;inh+=n;hl[nl].key=a;hl[nl].n=n;nl++; /* v10.0 */if(nhsym){i=hsym_of(a);key=i>=0?0x80000000UL|(DWORD)i:a;}
   for(x=0;x<nh&&hal[x].key!=key;x++);if(x==nh){if(nh>=4096)continue;hal[nh].key=key;hal[nh].n=0;nh++;}hal[x].n+=n;}
  else if(d8m>=0&&a-mods[d8m].base<mods[d8m].size){DWORD rva=a-mods[d8m].base,key=rva;ind+=n;if(d3d8_ok){i=d3d8_fn(rva);if(i>=0)key=0x80000000UL|vx_d3d8_fn[i];}
   for(x=0;x<nd&&d8[x].key!=key;x++);if(x==nd){if(nd>=4096)continue;d8[nd].key=key;d8[nd].n=0;nd++;}d8[x].n+=n;}
  else{ino+=n;oth[no].key=a;oth[no].n=n;no++;}}
 fprintf(f,"   the render thread's ring-3 code by 64-byte line (v9.8, .534; %% of all the time of gameplay frames): %lu samples in lines, %lu without a slot\n",lines,smpr.line_other);
 qsort(hal,nh,sizeof(kn),kn_cmp);qsort(d8,nd,sizeof(kn),kn_cmp);qsort(oth,no,sizeof(kn),kn_cmp);
 fprintf(f,"      VXDDRAW.DLL (the HAL), %.1f%% in all, by %s (%s):\n",100.0*inh/tot,nhsym?"function":"line",hsym_why);
 for(k=0;k<nh&&k<30;k++){DWORD key=hal[k].key;
  if(key&0x80000000UL)fprintf(f,"         %-34s %5.2f%%  (%4.1f%% of the HAL's)\n",hsym[key&0x7FFFFFFFUL].n,100.0*hal[k].n/tot,inh?100.0*hal[k].n/inh:0.0);
  else fprintf(f,"         +%05lX                             %5.2f%%\n",key-smpr.hal_base,100.0*hal[k].n/tot);}
 qsort(hl,nl,sizeof(kn),kn_cmp); /* v10.0: the hottest lines */
 fprintf(f,"      VXDDRAW.DLL's hottest 64-byte lines (the offset in the DLL; the function + offset by VXDDRAW.MAP):\n");
 for(k=0;k<nl&&k<40;k++){int i=nhsym?hsym_of(hl[k].key):-1;
  if(i>=0)fprintf(f,"         +%05lX  %s+%lX  %5.2f%%\n",hl[k].key-smpr.hal_base,hsym[i].n,hl[k].key-(DWORD)hsym_adj-hsym[i].a,100.0*hl[k].n/tot);
  else fprintf(f,"         +%05lX  %5.2f%%\n",hl[k].key-smpr.hal_base,100.0*hl[k].n/tot);}
 if(d8m>=0){fprintf(f,"      %s, %.1f%% in all, by %s:\n",dp,100.0*ind/tot,d3d8_ok?"function (the entry's offset from its base)":"line (not the 4.09.00.0904 build the entry table was made from)");
  for(k=0;k<nd&&k<30;k++){DWORD key=d8[k].key;fprintf(f,"         +%05lX%-46s %5.2f%%  (%4.1f%% of D3D8's)\n",key&0x7FFFFFFFUL,key&0x80000000UL?d3d8_name(key&0x7FFFFFFFUL):" (line)",100.0*d8[k].n/tot,ind?100.0*d8[k].n/ind:0.0);}}
 fprintf(f,"      other modules, %.1f%% in all, the hottest lines:\n",100.0*ino/tot);
 for(k=0;k<no&&k<20;k++){int mj=mod_of(oth[k].key);
  if(mj>=0)fprintf(f,"         %s+%05lX %5.2f%%\n",mods[mj].name,oth[k].key-mods[mj].base,100.0*oth[k].n/tot);else fprintf(f,"         %08lX %5.2f%%\n",oth[k].key,100.0*oth[k].n/tot);}
}
/* v4.5 (v10.431): DrawPrimitives2's time in exclusive phases (VXDDRAW.DLL VX_D3D_QueryPhases, turned on at the start) */
static DWORD (WINAPI *hal_phases)(DWORD *,DWORD);
/* v7.8 (driver v10.473): the triangles by draw command and path (QueryPhases 220-235), the VxD's time a packet for indexed packets (TEXDBG
 * BC CC/CD), the retires for a CPU access by cause (BC CA/CB/CD), the Blts by kind and destination (VX_DD_QueryBlts) */
static void v78_lines(FILE *f) {
 DWORD y[236],o[8],o2[8],o3[8];double cu=1000.0;int k;
 if(hal_phases&&hal_phases(y,236)==236&&y[0]==0x431) {
  double all=0;cu=y[1]?y[1]:1000.0;for(k=0;k<8;k++)all+=y[220+k];
  if(all>0){
   fprintf(f,"Triangles by draw command (v7.8): %.0f in all; indexed list (26) %.1f%%, indexed list (3) %.1f%%, list (18) %.1f%%, strip (19) %.1f%%, indexed strip (20) %.1f%%, fan (21) %.1f%%, indexed fan (22) %.1f%%, fan in the command (23) %.1f%%\n",
    all,100*y[226]/all,100*y[220]/all,100*y[221]/all,100*y[222]/all,100*y[223]/all,100*y[224]/all,100*y[225]/all,100*y[227]/all);
   fprintf(f,"   the indexed list loop %.1f%% (%lu; culled or no area %lu, %.1f%% of its triangles; stops %lu); indexed lists on the per-triangle path in its tokens %lu (v7.9: the template not on the fast path %lu, on it but not prepacked or batching off %lu, the rest stops); kept out of it: INDEXED off %lu, an index past the vertex data %lu\n",
    100*y[228]/all,y[228],y[229],y[228]?100.0*y[229]/y[228]:0.0,y[230],y[231],y[234],y[235],y[232],y[233]);}}
 if(texdbg2(0xBC,0xCC,o)&&(o[0]&0xFF)==0xBC&&o[1]==0xCC&&o[2]&&texdbg2(0xBC,0xCD,o2)) {
  double us=256.0/cu,n=o[2];
  fprintf(f,"VxD indexed packets (v7.8, microseconds a packet): copy-in %.1f, validation %.1f (%lu packets)",o[3]*us/n,o[4]*us/n,o[2]);
  if(o2[3]){double m=o2[3],e=o[5]*us/m,sl=o[6]*us/m,bu=o[7]*us/m,rg=o2[2]*us/m; /* v7.9: the emitter over the timed packets (v10.476) */
   fprintf(f,"; the emitter %.1f: the set-up before the vertices (state, texture, program) %.1f, the vertex area's slot and the vertices written %.1f, the index bursts %.1f, the registers set back %.1f (%lu packets drawn from a slot, timed)",e,e-sl-bu-rg,sl,bu,rg,o2[3]);}
  fprintf(f,"\n");}
 if(texdbg2(0xBC,0xCA,o)&&(o[0]&0xFF)==0xBC&&o[1]==0xCA&&texdbg2(0xBC,0xCB,o2)&&texdbg2(0xBC,0xCD,o3)) {
  static const char *cn[8]={"no cause given","a DirectDraw blit","a GDI or other 2D request","a Lock, Unlock or software blit","a flip","an asynchronous blit completed (VX2D_sync)","the old 3D path or a mode change","?"};
  DWORD c[8],t[8];double ms=256.0/cu/1000.0;for(k=0;k<6;k++){c[k]=o[2+k];t[k]=o2[2+k];}c[6]=o3[4];t[6]=o3[5];c[7]=o3[6];t[7]=o3[7];
  fprintf(f,"Retires of the 3D work in flight for a CPU access, by cause (v7.8):");
  for(k=0;k<8;k++)if(c[k])fprintf(f," %s %lu (%.1f ms in all, %.2f each);",cn[k],c[k],t[k]*ms,t[k]*ms/c[k]);
  fprintf(f,"\n");}
 if(hal_blts){DWORD b[69];memset(b,0,sizeof(b));if(hal_blts(b,69)>=53){ /* v7.9: words 53-68 0 from a HAL that gives 53 */
  static const char *kn[4]={"colour fills","depth fills","copies","other Blts"},*dn[4]={"the front","a back buffer or render target","a texture","another surface"};
  double cpm=b[0]?b[0]:1000000.0;int any=0;
  fprintf(f,"Blts by kind and destination (v7.8, the HAL):");
  for(k=0;k<16;k++)if(b[1+k]){any=1;fprintf(f," %s to %s %lu (%.2f ms each, %.0f pixels each, %lu finished on the CPU);",kn[k/4],dn[k%4],b[1+k],b[17+k]*256.0/cpm/b[1+k],b[33+k]*256.0/b[1+k],b[53+k]);}
  if(!any)fprintf(f," none");
  if(b[49]|b[50]|b[51]|b[52])fprintf(f," the copies' sources: the front %lu, a back buffer or render target %lu, a texture %lu, another surface %lu",b[49],b[50],b[51],b[52]);
  fprintf(f,"\n");}}
 if(hal_bsig){static DWORD g[226];if(hal_bsig(g,226)==226&&g[0]){DWORD i,j;double cpm=1000000.0;{DWORD b[53];if(hal_blts&&hal_blts(b,53)>=53&&b[0])cpm=b[0];}
  fprintf(f,"Blts by signature (v7.9, the HAL; %lu kinds, %lu calls of kinds past the eighth):\n",g[0],g[1]);
  for(i=0;i<g[0]&&i<8;i++){const DWORD *o=g+2+i*28;
   fprintf(f,"   %lu calls, %.2f ms each, %lu finished on the CPU, %lu left to DirectDraw: flags %08lX, destination caps %08lX %lux%lu at %08lX, source caps %08lX %lux%lu at %08lX%s; called from:",
    o[6],o[6]?o[7]*256.0/cpm/o[6]:0.0,o[8],o[9],o[0],o[1],o[3]>>16,o[3]&0xFFFF,o[26],o[2],o[4]>>16,o[4]&0xFFFF,o[27],o[5]?" (the same surface)":"");
   for(j=0;j<16&&o[10+j];j++){int m=mod_of(o[10+j]);if(m>=0)fprintf(f," %s+%lX",mods[m].name,o[10+j]-mods[m].base);else fprintf(f," %08lX",o[10+j]);}
   fprintf(f,"\n");}}}
}
/* v9.7 (.533): the in-run A/B (v5.9-v9.6: the HAL's VX_D3D_Switch ids and TEXDBG BC words, a setting each 2 s, medians and paired
 * differences) removed with VX_D3D_Switch: no switch is left to turn. A feature under test brings its own A/B back with its switch. */
static int sample_only; /* v9.8: SAMPLE */
/* v9.8 (.534): the T&L probe (ACCEL_D3D_TNL, QueryPhases 270-288; vx_d3d_core.c): which games chose hardware T&L when it was offered */
static void vxs_report(FILE *f) { /* v10.1 (.545): the indexed pass-through's span kernels (vx_d3d_span.inc, QueryPhases 302-306) */
 DWORD y[316];
 if(!hal_phases||hal_phases(y,307)!=307)return;
 fprintf(f,"Pass-through spans (.545; since the HAL was loaded, every process): SSE2 kernels %s; %lu spans checked, %lu vertices passed four at a time; %lu spans converted (%lu vertices)\n",
  y[302]?"on":"off (the C)",y[303],y[304],y[305],y[306]);
 if(hal_phases(y,311)==311)fprintf(f,"Indexed loop (.546): %lu tokens' vertex ranges proven inside at once (no vertex checked in the loop), %lu not; %lu triangles wholly off the screen dropped early; %lu stops for a vertex that failed (those triangles drawn one by one)\n",y[309],y[310],y[307],y[308]); /* v10.2 */
 if(hal_phases(y,312)==312)fprintf(f,"   (.547) pass-through spans inside a proven range, not checked again: %lu\n",y[311]); /* v10.3 */
 if(hal_phases(y,316)==316)fprintf(f,"   (.548) returns to the pass-through after a sparse run %lu; per-triangle path vertices passed by the one-test check %lu; driver T&L index snapshots %lu (%lu indices)\n",y[313],y[312],y[314],y[315]); /* v10.4 */
 {static DWORD z[332];if(hal_phases(z,332)==332)fprintf(f,"Draw packets held across DrawPrimitives2 calls (.550; always on from .551: %lu): %lu calls ended holding (%lu packets), %lu sends by a later call, %lu by a callback (Lock %lu, Blt %lu, Flip %lu, DestroySurface %lu, SetEntries %lu, context destroyed %lu); dropped: %lu refused or busy, %lu unsent (process gone %lu); lock waits timed out %lu\n",
  z[330],z[316],z[317],z[318],z[320],z[322],z[323],z[324],z[325],z[326],z[327],z[319],z[321],z[328],z[329]);} /* v10.7 */
 {static DWORD z[336];if(hal_phases(z,336)==336)fprintf(f,"Answers kept for a state (.552; since the HAL was loaded): the pixel program composed %lu times, used again %lu; a draw's support worked out %lu times, used again %lu\n",z[332],z[333],z[334],z[335]);
  else fprintf(f,"Answers kept for a state: not reported (a driver before .552)\n");} /* v10.9 */
 {static DWORD z[356];DWORD zn=hal_phases(z,356);if(zn>=344)fprintf(f,"Strips and fans passed through (.555; since the HAL was loaded): %lu indexed strips as lists (%lu triangles, %lu with an index twice left out), %lu by the old path; %lu fans passed through (%lu triangles), %lu by the loop\n",z[336],z[337],z[339],z[338],z[340],z[341],z[342]);
  if(zn>=346)fprintf(f,"Lists taken whole (.556): %lu tokens (%lu triangles)\n",z[344],z[345]);
  if(zn>=350)fprintf(f,"Runs drawn by the vertices they use (.557): %lu (%lu triangles, %lu vertices), %lu ended by a failing vertex\n",z[346],z[347],z[348],z[349]);
  if(zn>=354)fprintf(f,"Culled lists and fans passed through (.567; switch %lu): %lu runs filtered (%lu triangles culled in them and in fans, %lu runs with nothing kept, %lu by the vertices used)\n",z[343],z[350],z[351],z[352],z[353]);
  if(zn==356)fprintf(f,"   (.568) of them with a failing vertex in the span: %lu; ended by a triangle that uses a failing vertex: %lu\n",z[354],z[355]);} /* v10.10; v10.11; v10.12 */
}
static void tlp_report(FILE *f) { /* v9.9 (.536): the driver's T&L (ACCEL_D3D_TNL, QueryPhases 270-301) */
 DWORD y[302],k;
 if(!hal_phases||hal_phases(y,302)!=302)return;
 if(!y[270]){fprintf(f,"Driver T&L (ACCEL_D3D_TNL 0): off; hardware T&L not offered\n");return;}
 fprintf(f,"Driver T&L (ACCEL_D3D_TNL 1; since the HAL was loaded, every process): hardware T&L offered and done by the driver\n");
 fprintf(f,"   DrawPrimitives2 calls with untransformed vertices %lu, their draw commands %lu (%lu primitives)\n",y[271],y[272],y[288]);
 fprintf(f,"   T&L commands: SETTRANSFORM %lu, SETLIGHT %lu (%lu with light data), CREATELIGHT %lu, SETMATERIAL %lu, SETCLIPPLANE %lu\n",y[276],y[274],y[278],y[275],y[273],y[277]);
 fprintf(f,"   the untransformed calls' vertex formats:");for(k=0;k<4;k++)if(y[280+k*2])fprintf(f," FVF %03lX %lu calls;",y[279+k*2],y[280+k*2]);if(y[287])fprintf(f," others %lu calls",y[287]);fprintf(f,"\n");
 fprintf(f,"   transformed: %lu draws, %lu triangles, %lu vertices (%lu lit, %lu fogged, %lu with a generated or transformed coordinate set)\n",y[290],y[291],y[292],y[298],y[299],y[300]);
 fprintf(f,"   clipping: %lu triangles clipped at the near or far plane (%lu vertices made), %lu dropped wholly outside; %lu scratch-buffer restarts\n",y[293],y[295],y[294],y[296]);
 fprintf(f,"   left out: %lu draws (unsupported command or FVF, an index past the vertex buffer, too many triangles); state not done: %lu (spot as point, specular lighting, a light past 15, a third stage's coordinates)\n",y[297],y[301]);
}
static DWORD (WINAPI *hal_record)(DWORD),rec_n,rec_at=60,rec_asked,rec_hold; /* v8.4 (.517): REC=n frames recorded RECAT=s seconds after the start (C:\VXD3D.REC) */
static void phase_report(FILE *f) {
 static const char *nm[10]={"waiting for the callback lock","commands and states","state into the draw template","vertices (checks, fetch, clip)",
  "primitives into packets","buffers (queue copies, vertex buffer)","VxD calls","HAL waits (busy retries)","the HAL around the core","profiler's own calls"};
 static const char *bk[6]={"under 10 us","10-50","50-100","100-500","500-1000","1 ms+"};
 DWORD w[144],k,i,frames=0,fo[116];double cu,tot=0;
 if(!hal_phases||hal_phases(w,144)!=144||w[0]!=0x431){fprintf(f,"DrawPrimitives2 phases: not available\n");return;}
 if(hal_frames&&hal_frames(fo,116)==116)frames=fo[1];
 cu=w[1]?w[1]:1000;
 for(k=0;k<10;k++)tot+=w[34+k*11]*256.0/cu/1000.0;
 fprintf(f,"DrawPrimitives2 phases (exclusive; %lu calls, %lu frames, %.0f TSC cycles a microsecond): %.1f ms in all, %.2f ms a frame\n",w[2],frames,cu,tot,frames?tot/frames:0.0);
 fprintf(f,"   phase: ms in all, ms a frame, microseconds a call; calls with time in it, entries, the longest (us); those calls by their time in it (%s, %s, %s, %s, %s, %s)\n",bk[0],bk[1],bk[2],bk[3],bk[4],bk[5]);
 for(k=0;k<10;k++){DWORD *q=w+34+k*11;double ms=q[0]*256.0/cu/1000.0;
  fprintf(f,"   %-40s %9.1f %7.2f %7.1f; %lu, %lu, %.0f;",nm[k],ms,frames?ms/frames:0.0,w[2]?ms*1000.0/w[2]:0.0,q[2],q[1],q[3]*256.0/cu);
  for(i=0;i<6;i++)fprintf(f," %lu",q[4+i]);fprintf(f,"\n");}
 {DWORD y[270];int pg=hal_phases(y,270)==270; /* v9.0 (.525): the VMM's page faults and page-ins in the calls read */
 fprintf(f,"   calls by their whole time: count, ms in all; of the calls read (every 16th): ms the thread ran (calls)%s:\n",pg?", page faults, page-ins":"");
 for(i=0;i<6;i++)if(w[3+i]){fprintf(f,"     %-12s %8lu %10.1f %8lu (%lu)",bk[i],w[3+i],w[9+i]*256.0/cu/1000.0,w[15+i],w[21+i]);if(pg)fprintf(f,", %lu, %lu",y[253+i],y[259+i]);fprintf(f,"\n");}
 if(pg&&y[265])fprintf(f,"   the calls read that took 10 ms or more: %lu, %lu ms in all, the thread ran %lu ms of it; %lu page faults, %lu page-ins in them (a page-in is a wait on the disk)\n",y[265],y[266],y[267],y[268],y[269]);
 else if(pg)fprintf(f,"   the calls read that took 10 ms or more: none\n");}
 if(w[31])fprintf(f,"   submit calls: %lu, inside the VxD %.1f ms (%.1f us each), the thread ran %lu ms of it\n",w[31],w[32]*256.0/cu/1000.0,w[32]*256.0/cu/w[31],w[33]);
 {DWORD x[148];if(hal_phases(x,148)==148)fprintf(f,"   the HAL's DLL kept in memory (ACCEL_LOCK_HAL %lu): %lu pages at %08lX, %lu handles holding it\n",x[147],x[144],x[145],x[146]);}
 fprintf(f,"   bad-vertex checks (drop reason 2000): x, y or z not finite %lu; rhw not finite %lu; rhw 0 %lu; rhw < 0 %lu\n",w[27],w[28],w[29],w[30]);
 {DWORD y[244];if(hal_phases(y,244)==244)fprintf(f,"   primitives whose vertices all have rhw 0, drawn with rhw 1 (v9.6, .531: no texture, no table fog, no w-buffer): %lu\n",y[242]);}
 {DWORD g[172],*x=g+148;double t;
  if(hal_phases(g,172)==172&&x[0]){t=x[0];
   fprintf(f,"Geometry (v5.3, the HAL, all frames): %lu triangles in, %lu source vertices (an index's first use in a DrawPrimitives2 call: %.2f a triangle); %lu DrawPrimitives2 calls, %lu vertex buffers changed within one, %lu indices above 65535\n",x[0],x[1],x[1]/t,x[21],x[22],x[23]);
   fprintf(f,"   fast path: list triangles %lu (%.0f%%), strip starts %lu (%.0f%%), strips' next triangles %lu (%.0f%%), culled or no area %lu (%.0f%%), declined %lu (%.0f%%); general path %lu (%.0f%%: clipped %lu, culled %lu); fast path not tried %lu\n",
    x[6],100.0*x[6]/t,x[5],100.0*x[5]/t,x[4],100.0*x[4]/t,x[3],100.0*x[3]/t,x[2],100.0*x[2]/t,x[7],100.0*x[7]/t,x[8],x[9],x[10]);
   fprintf(f,"   vertices written into packets: fast path %lu + general path %lu = %lu (%.2f a triangle in, %.2f a source vertex)\n",x[11],x[12],x[11]+x[12],(x[11]+x[12])/t,x[1]?(double)(x[11]+x[12])/x[1]:0.0);
   fprintf(f,"   triangles sharing 0 / 1 / 2 / 3 indices with the triangle before them: %.0f%% / %.0f%% / %.0f%% / %.0f%%\n",100.0*x[13]/t,100.0*x[14]/t,100.0*x[15]/t,100.0*x[16]/t);
   fprintf(f,"   batches sent because the next primitive had: another state %lu, another kind %lu, no room for its vertices (192, or 384 by index) %lu, no room for another run %lu\n",x[17],x[18],x[19],x[20]);}
  {DWORD q[176],r[8];if(hal_phases(q,176)==176){int vok=texdbg2(0xBC,0xA4,r)&&(r[0]&0xFF)==0xBC;
   fprintf(f,"Prepacked vertices (v5.4, ACCEL_D3D_PREPACK %lu in the VxD, %lu in the HAL): the HAL's batches sent prepacked %lu, with the six arrays %lu; the VxD copied %lu as they came, unpacked %lu\n",
    vok?r[4]:0,q[174]&1,q[172],q[173],vok?r[2]:0,vok?r[3]:0);
   /* v8.9: the pairs-kept line removed with ACCEL_D3D_PAIRLIST (.524) */
   {DWORD q2[180];if(hal_phases(q2,180)==180){double nb=(double)q2[172]+q2[173];
    fprintf(f,"Copies (v6.2): the VxD's copy-in %.1f MB (%.0f bytes a batch); prepacked blocks copied once, into the command block (ACCEL_D3D_ONECOPY) %lu of %.0f\n",
     vok?r[6]*16.0/1048576.0:0.0,vok&&nb?r[6]*16.0/nb:0.0,vok?r[5]:0,nb);}}
   {DWORD r3[8],r4[8];if(texdbg2(0xBC,0xB7,r3)&&(r3[0]&0xFF)==0xBC&&texdbg2(0xBC,0xB8,r4)&&(r4[0]&0xFF)==0xBC){ /* v6.6 */
    fprintf(f,"Render target into texture copies (v6.6, ACCEL_D3D_RINGBLT %lu): in the ring %lu (%.1f microseconds each to write), not taken %lu, empty after the clip %lu; by the 2D engine after the wait for the 3D work %lu, declined there (the HAL's CPU copy) %lu; that wait %.1f ms in all\n",
     r3[2],r3[3],r3[3]?r4[3]*256.0/1000.0/r3[3]:0.0,r3[4],r4[2],r3[5],r3[6],r3[7]*256.0/1000000.0);
    fprintf(f,"   all retires of work in flight for a CPU access (Lock, 2D, flip): %lu, %.1f ms\n",r4[4],r4[5]*256.0/1000000.0);}}
   {DWORD r2[8];if(texdbg2(0xBC,0xB4,r2)&&(r2[0]&0xFF)==0xBC) /* v6.3 */
    fprintf(f,"Textures-only set-ups (v6.3, ACCEL_D3D_TEXONLY %lu): %lu packets sent the texture units only, of %lu state-cache misses tested\n",r2[2],r2[3],r2[4]);}
   {DWORD z[200],*t=z+180;if(hal_phases(z,200)==200&&t[0]){double n=t[0],cu=z[1]?z[1]:1000,ck=t[14]?(double)t[13]/t[14]:0;
    fprintf(f,"Fast path, sampled one triangle in 16 (v5.7): %lu triangles; cycles a triangle (%.0f a microsecond; each part holds one clock read of about %.0f cycles):\n",t[0],cu,ck);
    fprintf(f,"   fetch and range checks %.0f, area and cull (doubles) %.0f, strip and pair matching %.0f, vertex writes %.0f, opening the batch slot %.0f (v5.8: of it sending the batch before %.0f, for %.1f%% of triangles: %.0f a send; the opening alone %.0f), the rest %.0f; in all %.0f (%.2f microseconds; %.0f without the sends)\n",
     t[1]*16/n,t[2]*16/n,t[3]*16/n,t[4]*16/n,t[5]*16/n,t[19]*16/n,100.0*t[15]/n,t[15]?t[19]*16.0/t[15]:0.0,(t[5]>t[19]?t[5]-t[19]:0)*16/n,t[6]*16/n,(t[1]+t[2]+t[3]+t[4]+t[5]+t[6])*16/n,(t[1]+t[2]+t[3]+t[4]+t[5]+t[6])*16/n/cu,(t[1]+t[2]+t[3]+t[4]+t[5]+t[6]-t[19])*16/n);
    fprintf(f,"   declined %.1f%%, culled %.1f%%, strip next or start %.1f%%, list %.1f%%; the token loop's own time a triangle %.0f cycles (%lu sampled), of it the geometry counter's call %.0f\n",
     100.0*t[7]/n,100.0*t[8]/n,100.0*t[9]/n,100.0*t[18]/n,t[12]?t[10]*16.0/t[12]:0.0,t[12],t[12]?t[11]*16.0/t[12]:0.0);
    fprintf(f,"   vertex index checks before the triangles: %lu made, %lu skipped as checked earlier in the call\n",t[16],t[17]);
    {DWORD y[212];if(hal_phases(y,212)==212&&y[192]){double ls=y[192],fs=y[180]?y[180]:1,h=y[200]*16.0/ls,g=y[191]*16.0/ls,cl=y[201]*16.0/ls,rt=y[202]*16.0/ls, /* v6.6 */
      fp=(y[181]+y[182]+y[183]+y[184]+y[185]+y[186])*16.0/fs,tot=y[190]*16.0/ls,rd=y[206]/ls,cr=y[194]?(double)y[193]/y[194]:0;
     fprintf(f,"   the token loop, cycles a sampled triangle (v6.6): head (bad-vertex, edge and fast tests) %.0f, geometry counter %.0f, the call into the fast path %.0f, the fast path %.0f, the return %.0f; in all %.0f\n",h,g,cl,fp,rt,tot);
     fprintf(f,"   of it the profiler: %.1f clock reads a triangle at about %.0f cycles each (%.0f) and the geometry counter (%.0f); without them %.0f cycles a triangle\n",rd,cr,rd*cr,g,tot-rd*cr-g);
     if(y[205])fprintf(f,"   before the loop, every triangle: reading its indices %.0f, the vertex checks %.0f cycles a triangle (%lu triangles); vertices passed by the indexed path's one-test check: %lu\n",y[203]*16.0/y[205],y[204]*16.0/y[205],y[205],y[176]);}}}}}}}
 {DWORD y[212];if(hal_phases(y,212)==212&&y[208]) /* v7.7 (v10.472): the indexed list loop, apart from the per-triangle loop's samples */
   fprintf(f,"Indexed list loop (v7.7, v10.472): %lu triangles, %lu stops; %.0f cycles a triangle, of it %.0f packets begun (with the sends); %.2f vertices converted a triangle\n",y[208],y[209],y[207]*16.0/y[208],y[211]*16.0/y[208],(double)y[210]/y[208]);}
 {DWORD y[244];if(hal_phases(y,244)==244){ /* v8.3 (.515); v9.5: the engine's cull removed */
  fprintf(f,"Indexed pass-through (ACCEL_D3D_IXPASS %lu; 512 vertices a packet): %lu triangles passed through in %lu runs (%.2f vertices converted a triangle), %lu runs refused as sparse, %lu for a vertex, %lu packets begun",
   y[243],y[236],y[238],y[236]?(double)y[237]/y[236]:0.0,y[239],y[240],y[241]);
  fprintf(f,"\n");}}
 {DWORD y[253],c[8];if(hal_phases(y,253)==253){ /* v8.7 (.522); v8.8 (.523): the 8-word packets; v8.9 (.524): state deltas, IXPF removed */
  fprintf(f,"Readiness answers (ACCEL_D3D_INFOCACHE %lu): %lu DrawPrimitives2 calls used the kept answer, %lu asked the VxD\n",y[244],y[246],y[247]);
  fprintf(f,"State deltas (v8.9, ACCEL_D3D_DELTA %lu): the HAL marked %lu queued packets, %.2f of the 6 state ranges each the same as the packet before, %lu with every range the same",y[245],y[248],y[248]?(double)y[251]/y[248]:0.0,y[252]);
  if(texdbg2(0xBC,0xE8,c)&&(c[1]&0xFF)==0xE8)fprintf(f,"; the VxD honoured %lu (copied whole: %lu), kept %lu ranges, did not read %.1f MB of state, skipped the state's checks for %lu packets",c[3],c[7],c[4],c[5]*16.0/1048576.0,c[6]);
  fprintf(f,"\n");
  if(texdbg2(0xBC,0xFA,c)&&(c[1]&0xFF)==0xFA)fprintf(f,"Batch call sequencer (v9.3; always from .529): %lu calls opened, %lu writes left out at the open (%.2f a call), %lu closes with nothing to put back\n",c[3],c[4],c[3]?(double)c[4]/c[3]:0.0,c[5]);
  {DWORD y[180],t[8];if(hal_phases&&hal_phases(y,180)==180&&texdbg2(0xBC,0xF5,t)&&(t[0]&0xFF)==0xBC) /* v9.3 (.528) */
   fprintf(f,"Texture runs (v9.3; always from .529): the HAL added %lu runs (a texture change inside an open indexed packet), %lu template changes changed more than the texture (a new packet); the VxD drew %lu packets with runs, %lu texture switches inside them\n",
    y[177],y[178],t[2],t[3]);}
  if(texdbg2(0xBC,0xEB,c)&&(c[1]&0xFF)==0xEB)fprintf(f,"Set-up register shadow (v9.0, ACCEL_D3D_SHADOW %lu, always on from .526): %lu state set-ups with it in use, %lu type-01 words left out (%.1f a set-up)\n",c[2],c[3],c[4],c[3]?(double)c[4]/c[3]:0.0);
  {DWORD b0[8],b1[8];if(texdbg2(0xBC,0xEC,b0)&&texdbg2(0xBC,0xED,b1)&&(b0[0]&0xFF)==0xBC&&b0[2]) /* v9.2 (.527): the VxD's record of the last refused packet */
   fprintf(f,"Packets the VxD refused (v9.2): %lu; the last: %s, refused at %s line %lu, packet %lu of %lu, op %lu, textured %lu (format %lu), depth format %lu, target format %lu, %lu vertices, pre.on %lu, %lu bursts, %s, ranges marked the same %02lX, target %08lX %lux%lu, texture %08lX, first vertex %08lX %08lX\n",
    b0[2],(b0[3]>>24)==2?"batch":"single submit",((b0[3]>>16)&255)==2?"vx_native3d_vxd.c":"vx_native3d_hw.c",b0[3]&0xFFFF,b0[4]+1,b0[5],b0[6]>>24,(b0[6]>>20)&1,(b0[6]>>12)&255,(b0[6]>>4)&255,b0[6]&15,
    b0[7]&0xFFFF,(b0[7]>>16)&15,(b0[7]>>20)&255,(b0[7]>>31)?"validated by its geometry alone":"fully validated",b1[2],b1[3],b1[4]>>16,b1[4]&0xFFFF,b1[5],b1[6],b1[7]);}
  fprintf(f,"8-word vertices (ACCEL_D3D_V8 %lu): the HAL began %lu indexed packets in the 8-word form",y[249],y[250]);
  if(texdbg2(0xBC,0xE5,c)&&(c[1]&0xFF)==0xE5)fprintf(f,"; the VxD drew %lu from the vertex area, wrote the 8-word layout %lu times, made %lu whole set-ups to put the 10-word layout back",c[2],c[3],c[4]);
  fprintf(f,"\n");}}
 {DWORD y[220];if(hal_phases(y,220)==220&&y[212]){double n=y[212],mh=y[1]?y[1]:1000.0; /* v7.0 (v10.462): a template build's parts */
   fprintf(f,"Template builds (v7.0): %lu measured, %.1f microseconds each: composing the program %.1f, the textures resolved %.1f, state and texture fields %.1f, program and colour key %.1f, the rest %.1f; state the same as the last build's except texture handles %.0f%%, the same in everything %.0f%%\n",
    y[212],(y[213]+y[214]+y[215]+y[216]+y[217])*16.0/mh/n,y[213]*16.0/mh/n,y[214]*16.0/mh/n,y[215]*16.0/mh/n,y[216]*16.0/mh/n,y[217]*16.0/mh/n,100.0*y[218]/n,100.0*y[219]/n);}}
 v78_lines(f); /* v7.8 */
 {DWORD o2[2];hal_phases(o2,2|0x40000000UL);} /* off */
}
/* v5.3 (v10.440): the VxD's command words by part (TEXDBG BC 9E-A1; A2 at the start clears and turns it on, A3 off) */
static void words_report(FILE *f) {
 DWORD w[24],q[8],j;double P,T;
 for(j=0;j<4;j++){if(!texdbg2(0xBC,0x9E+j,q))return;memcpy(w+6*j,q+2,24);}
 texdbg2(0xBC,0xA3,q);
 if(!w[0]){fprintf(f,"Command words: none counted\n");return;}
 P=w[0];T=w[10]?w[10]:1;
 fprintf(f,"Command words (v5.3, the VxD): %lu packets (%lu with their state set-up), %lu words, %.0f a packet; %lu vertices (%.1f a packet, %.1f words each), %lu triangles (%.1f a packet), %lu vertices in strips\n",
  w[0],w[1],w[2],w[2]/P,w[9],w[9]/P,w[9]?(double)w[6]/w[9]:0.0,w[10],w[10]/P,w[11]);
 fprintf(f,"   a packet's words: state set-up %.1f, texture set-up %.1f, program set-up %.1f, vertex words %.1f, burst heads, block headers and padding %.1f, before the set-up %.1f\n",
  w[3]/P,w[4]/P,w[5]/P,w[6]/P,w[7]/P,w[8]/P);
 fprintf(f,"   a triangle's words: %.1f in all: vertex words %.1f, set-ups %.1f, the rest %.1f\n",w[2]/T,w[6]/T,(w[3]+w[4]+w[5])/T,(w[7]+w[8])/T);
 fprintf(f,"   the same words as the last packet's (of %lu packets read back): state set-up %lu of %lu (%lu words), texture set-up %lu of %lu (%lu words), program set-up %lu of %lu (%lu words)\n",
  w[18],w[12],w[1],w[13],w[14],w[19],w[15],w[16],w[20],w[17]);
}
/* v2.3 (v10.390): paging and CPU use each second (Windows 98's own counters, HKEY_DYN_DATA\PerfStats: VMM page-ins, page faults,
 * discards, KERNEL CPU use), put beside the seconds that had a frame of 75 ms and more; memory at the start and the least free */
static const char *perf_name[4]={"VMM\\cPageIns","VMM\\cPageFaults","VMM\\cDiscards","KERNEL\\CPUUsage"};static int perf_ok;
static int perf_key(const char *sub,DWORD *v) {HKEY k;DWORD j,t,sz;BYTE b[64];int ok=0;
 if(RegOpenKeyExA((HKEY)0x80000006UL,sub,0,KEY_READ,&k)!=ERROR_SUCCESS)return 0;
 for(j=0;j<4;j++){sz=sizeof(b);if(RegQueryValueExA(k,perf_name[j],0,&t,b,&sz)==ERROR_SUCCESS&&sz>=4){if(v)memcpy(&v[j],b,4);ok++;}else if(v)v[j]=0;}
 RegCloseKey(k);return ok;}
static DWORD pf_n[2],pf_sum[2][4];static DWORD mem_free0,mem_least=0xFFFFFFFFUL,mem_total,swap_most;
static void mem_note(void) {MEMORYSTATUS m;memset(&m,0,sizeof(m));m.dwLength=sizeof(m);GlobalMemoryStatus(&m);
 if(!mem_total){mem_total=m.dwTotalPhys;mem_free0=m.dwAvailPhys;}if(m.dwAvailPhys<mem_least)mem_least=m.dwAvailPhys;
 if(m.dwTotalPageFile-m.dwAvailPageFile>swap_most&&m.dwTotalPageFile>=m.dwAvailPageFile)swap_most=m.dwTotalPageFile-m.dwAvailPageFile;}
static void perf_report(FILE *f) {DWORD j;
 fprintf(f,"Memory: %lu KB physical, %lu KB free at the start, the least free %lu KB; swap file in use at most %lu KB\n",mem_total>>10,mem_free0>>10,mem_least==0xFFFFFFFFUL?0:mem_least>>10,swap_most>>10);
 if(!perf_ok){fprintf(f,"Paging counters: not available\n");return;}
 for(j=0;j<2;j++){DWORD n=pf_n[j];if(!n){fprintf(f,"   %s: none\n",j?"seconds with a frame of 75 ms and more":"the other seconds");continue;}
  fprintf(f,"   %s (%lu): %.1f page-ins/s, %.1f page faults/s, %.1f discards/s, CPU %.0f%%\n",j?"seconds with a frame of 75 ms and more":"the other seconds",n,
   (double)pf_sum[j][0]/n,(double)pf_sum[j][1]/n,(double)pf_sum[j][2]/n,(double)pf_sum[j][3]/n);}
}
/* v2.0 (v10.387): the slow DrawPrimitives2 calls and Blts (VXDDRAW.DLL VX_D3D_QuerySlow, VX_DD_QuerySlowBlt) */
static DWORD (WINAPI *hal_slowops)(DWORD *,DWORD); /* v2.3 */static DWORD (WINAPI *hal_slowio)(DWORD *,DWORD); /* v3.9 */
static const char *dp2_op(DWORD op) {static const char *n[45]={"before the first command","POINTS","INDEXEDLINELIST","INDEXEDTRIANGLELIST",0,0,0,0,"RENDERSTATE",0,0,0,0,0,0,"LINELIST","LINESTRIP","INDEXEDLINESTRIP","TRIANGLELIST","TRIANGLESTRIP","INDEXEDTRIANGLESTRIP","TRIANGLEFAN","INDEXEDTRIANGLEFAN","TRIANGLEFAN_IMM","LINELIST_IMM","TEXTURESTAGESTATE","INDEXEDTRIANGLELIST2","INDEXEDLINELIST2","VIEWPORTINFO","WINFO","SETPALETTE","UPDATEPALETTE","ZRANGE","SETMATERIAL","SETLIGHT","CREATELIGHT","SETTRANSFORM","EXT","TEXBLT","STATESET","SETPRIORITY","SETRENDERTARGET","CLEAR","SETTEXLOD","SETCLIPPLANE"};return op<45&&n[op]?n[op]:"op";}
static DWORD (WINAPI *hal_slow)(DWORD *,DWORD),(WINAPI *hal_slowblt)(DWORD *,DWORD);
static void slow_report(FILE *f,double cp) {
 DWORD o[66],k;
 {DWORD r[8],x[8],j;if(texdbg2(0xBD,8,r)&&r[0]==0xBD){fprintf(f,"VxD batch calls of 10 ms and more: %lu\n",r[2]);
  if(r[2])fprintf(f,"   the 8 longest (ms: whole, waiting for the engine in it, the calling thread ran in it (-1: not read, ACCEL_D3D_FASTCLOCK); packets, triangles, fetch starts, engine resets, late fences, kicks written again):\n");
  for(j=0;j<8;j++){texdbg2(0xBD,j,r);texdbg2(0xBD,0x10+j,x);if(!r[2])continue;
   fprintf(f,"     %.1f, %.1f, %ld; %lu, %lu, %lu, %lu, %lu, %lu\n",r[2]/cp,r[3]/cp,(long)x[4],r[4],r[5],r[6],r[7],x[2],x[3]);}}} /* v2.1 (v10.388) */
 {DWORD l[8],m[8];if(texdbg2(0xBC,0xDB,l)&&(l[1]&0xFF)==0xDB&&texdbg2(0xBC,0xDC,m)){DWORD mode=l[7]>>30; /* v8.3 (.515): the engine's lock (ACCEL_PRIVLOCK) */
  if(mode==1)fprintf(f,"Engine lock (ACCEL_PRIVLOCK 1, the VxD's own): %lu entries, %lu waited (%.1f ms in all, the longest %.1f ms, %lu of 2 ms and more); the longest hold %.1f ms, %lu holds of 2 ms and more\n",l[2],l[3],l[4]/cp,l[5]/cp,l[6],(l[7]&0x3FFFFFFFUL)/cp,m[2]);
  else fprintf(f,"Engine lock: the system critical section (ACCEL_PRIVLOCK 0 or no semaphore)\n");}}
 {DWORD c[8],q[8],j,k;if(texdbg2(0xBC,0xD0,c)&&(c[1]&0xFF)==0xD0&&c[2]){ /* v8.1 (.512): what ran while a 3D call waited for the system critical section */
  fprintf(f,"While a 3D call waited for the engine lock (the sampler, 1 ms): %lu samples: in this VxD's calls %lu, V86 code %lu, 16-bit code %lu, 32-bit code %lu, no client registers %lu\n",c[2],c[3],c[4],c[5],c[6],c[7]);
  {DWORD sel[16];HDC dc=GetDC(NULL);WORD two=2;int ns=ExtEscape(dc,0x8F74,2,(LPCSTR)&two,64,(LPSTR)sel)>0; /* v8.2 (.513): the display driver's and DIBENG's code selectors */
   static const char *sn[8]={"VX85598.DRV _TEXT (MoveCursor)","VX85598.DRV (Control)","DIBENG (BitBlt)","DIBENG (Output)","DIBENG (ExtTextOut)","DIBENG (MoveCursorExt)","DIBENG (StretchBlt)","DIBENG (Control)"};
   ReleaseDC(NULL,dc);
   if(ns){fprintf(f,"   code selectors:");for(k=0;k<8;k++)fprintf(f," %s %04lX:%04lX;",sn[k],sel[k]>>16,sel[k]&0xFFFF);fprintf(f,"\n");}
  for(j=0;j<8;j++)if(texdbg2(0xBC,j<4?0xD1+j:0xD7+j-4,q))for(k=0;k<3;k++){DWORD key=q[2+2*k],n=q[3+2*k],m;const char *who="";if(!n)continue;
   if(ns&&(key&0x80000000UL))for(m=0;m<8;m++)if((sel[m]>>16)==((key>>8)&0xFFFF)){who=sn[m];break;}
   if(key&0x40000000UL)fprintf(f,"   V86 CS %04lX: %lu\n",key&0xFFFF,n);
   else if(key&0x80000000UL)fprintf(f,"   16-bit %04lX:%04lX-%04lX%s%s: %lu\n",(key>>8)&0xFFFF,(key&0xFF)<<8,((key&0xFF)<<8)|0xFF,who[0]?" ":"",who,n);
   else{char name[MAX_PATH];MEMORY_BASIC_INFORMATION mi;name[0]=0;if(VirtualQuery((LPCVOID)(key<<12),&mi,sizeof(mi))&&mi.AllocationBase)GetModuleFileNameA((HMODULE)mi.AllocationBase,name,sizeof(name));
    fprintf(f,"   32-bit page %08lX%s%s: %lu\n",key<<12,name[0]?" ":"",name,n);}}}}}
 {DWORD r[8],x[8],j;if(texdbg2(0xBD,0x30,r)&&r[0]==0xBD){fprintf(f,"VxD single draw/clear calls of 10 ms and more: %lu\n",r[2]); /* v2.6 (v10.393) */
  if(r[2])fprintf(f,"   the 8 longest (ms: whole, waiting for the VxD's lock, waiting for the engine, the calling thread ran in it; op, clear flags, triangles, target):\n");
  for(j=0;j<8;j++){texdbg2(0xBD,0x20+j,r);texdbg2(0xBD,0x28+j,x);if(!r[2])continue;
   fprintf(f,"     %.1f, %.1f, %.1f, %ld; op %lu, flags %06lX, %lu, %08lX\n",r[2]/cp,r[3]/cp,r[4]/cp,(long)x[3],r[5]&255,r[5]>>8,x[2],x[5]);}}}
 if(hal_slow&&hal_slow(o,66)==66){
  fprintf(f,"DrawPrimitives2 calls of 10 ms and more: %lu, %.0f ms in all\n",o[0],o[1]/cp);
  if(o[0])fprintf(f,"   the 8 longest (ms: whole, waiting for the HAL lock, in VxD calls (calls), outside them; busy answers retried, Sleep(1)s, why busy):\n");
  for(k=0;k<8;k++){DWORD *w=o+2+k*8;if(!w[0])continue;
   fprintf(f,"     %.1f, %.1f, %.1f (%lu), %.1f; %lu, %lu, %s%08lX\n",w[0]/cp,w[1]/cp,w[2]/cp,w[3],(w[0]-w[1]-w[2])/cp,w[4],w[6],w[7]>>31?"":"(none) ",w[7]&0x7FFFFFFFUL);
   {DWORD so[48],q;if(hal_slowops&&hal_slowops(so,48)==48){fprintf(f,"        longest commands:");for(q=0;q<3;q++){DWORD op=so[k*6+q*2];if(op==0xFFFFFFFFUL)break;
     fprintf(f,"%s %s %lu (%lu x) %.1f ms",q?",":"",dp2_op(op&255),op&255,op>>8,so[k*6+q*2+1]/cp);}fprintf(f,"\n");}
   {DWORD si[64],*v;if(hal_slowio&&hal_slowio(si,64)==64){v=si+k*8;if(v[0]|v[1]|v[2])
     fprintf(f,"        its submits: before the VxD's entry %.1f ms (longest %.1f), inside %.1f ms (the thread ran %lu ms), after its exit %.1f ms (longest %.1f); between submits %.1f ms (the thread ran %lu ms)\n",
      v[0]/cp,v[6]/cp,v[1]/cp,v[3],v[2]/cp,v[7]/cp,v[4]/cp,v[5]);}}}}}
 if(hal_slowblt&&hal_slowblt(o,65)==65){
  fprintf(f,"Blts of 10 ms and more: %lu\n",o[0]);
  for(k=0;k<8;k++){DWORD *w=o+1+k*8;if(!w[0])continue;
   fprintf(f,"     %.1f ms: flags %08lX, dest caps %08lX %lux%lu, source caps %08lX %lux%lu, %s %08lX, NOTHANDLED at line %lu, CPU blits %lu\n",w[0]/cp,w[1],w[2],w[3]>>16,w[3]&0xFFFF,w[4],w[5]>>16,w[5]&0xFFFF,
    w[6]>>31?"handled":"not handled",w[6]&0x7FFFFFFFUL,w[7]>>16,w[7]&0xFFFF);}}
}
static void vxd_report(FILE *f) {
 DWORD o[8],s[8],x[8],k[8],e[8];double mhz=mon_mhz();
 if(vxd==INVALID_HANDLE_VALUE){fprintf(f,"VxD counters: VX85598 not opened\n");return;}
 if(texdbg(0x77,o)&&o[0]==0x77&&o[1]){
  fprintf(f,"VxD: %lu draw packets, %.1f microseconds each in VX3DN_submit (CPU %.0f MHz); %lu engine writes, %lu status reads a packet\n",
   o[1],o[2]*256.0/mhz/o[1],mhz,o[3]/o[1],o[4]/o[1]);
  if(texdbg(0xAD,s)&&s[0]==0xAC)fprintf(f,"VxD: a packet's parts: checks, copy, validation, mapping %.1f; open and clock %.1f; the draw %.1f; the close %.1f microseconds\n",
   s[1]*256.0/mhz/o[1],s[2]*256.0/mhz/o[1],s[3]*256.0/mhz/o[1],s[4]*256.0/mhz/o[1]);}
 else fprintf(f,"VxD: no draw packets counted\n");
 if(texdbg(0xA4,x)&&x[1]){fprintf(f,"VxD fetcher: %lu packets, %.0f command words each (most %lu), %lu through the registers, %lu not finished\n",x[1],(double)x[2]/x[1],x[5],x[3],x[4]);
  if(texdbg(0xA9,k))fprintf(f,"VxD fetcher: %lu without state; a packet: building its words %.1f, waiting for the draw before %.1f, in all %.1f microseconds\n",k[2],k[3]*256.0/mhz/x[1],k[4]*256.0/mhz/x[1],k[5]*256.0/mhz/x[1]);
  {DWORD m[18],q[8];int j,ok=1;for(j=0;j<3&&ok;j++){ok=texdbg2(0xBC,0x9B+j,q);if(ok)memcpy(m+6*j,q+2,24);} /* v10.429: what differed when the state cache missed */
   if(ok&&m[17]){static const char *nm[16]={"nothing listed","textures only","target only","textures + target","raster only","textures + raster","target + raster","textures + target + raster",
     "program only","textures + program","target + program","textures + target + program","raster + program","textures + raster + program","target + raster + program","all four"};
    fprintf(f,"VxD state cache misses, every 8th sampled (%lu): state forgotten %.0f%%",m[17],100.0*m[16]/m[17]);
    for(j=0;j<16;j++)if(m[j])fprintf(f,"; %s %.0f%%",nm[j],100.0*m[j]/m[17]);fprintf(f,"\n");}}
  if(texdbg(0xC9,e)&&e[0]==0xC8)fprintf(f,"VxD building (microseconds a packet): state-cache compare %.1f, emitter %.1f (state set-up %.1f, texture set-up %.1f, program %.1f, primitive set-up and vertices %.1f), keeping the state %.1f\n",
   e[1]*256.0/mhz/x[1],e[2]*256.0/mhz/x[1],e[4]*256.0/mhz/x[1],e[5]*256.0/mhz/x[1],e[6]*256.0/mhz/x[1],e[7]*256.0/mhz/x[1],e[3]*256.0/mhz/x[1]);
  {DWORD v[8];if(texdbg2(0xBC,0xAE,v)&&(v[0]&0xFF)==0xBC&&x[1]){ /* v6.9 (v10.461): the prepacked vertex copy (TSC / 16) */
    double cyc=v[2]*16.0,chk=v[3]*16.0,w=(double)v[4];
    fprintf(f,"VxD vertex copy (v6.9): %.1f microseconds a packet copying %.0f words (%.0f MB/s), %.1f checking them; sampled: the first copy %.0f MB/s, the same words again from the cache %.0f MB/s (%lu words)\n",
     cyc/mhz/x[1],w/x[1],cyc?w*4.0/(cyc/mhz):0.0,chk/mhz/x[1],v[6]?v[7]*4.0/(v[6]*16.0/mhz):0.0,v[5]?v[7]*4.0/(v[5]*16.0/mhz):0.0,v[7]);}}
  {DWORD v9[8];if(texdbg2(0xBC,0xF0,v9)&&(v9[0]&0xFF)==0xBC&&v9[1]==0xF0)fprintf(f,"VxD indexed packets with no free slot (.564): %lu drawn by index after a wait for the work in flight, %lu still too large and drawn through the registers\n",v9[2],v9[3]);} /* v10.18 */
  {DWORD v[8];if(texdbg2(0xBC,0xC6,v)&&(v[0]&0xFF)==0xBC&&v[1]==0xC6) /* v7.6 (v10.470): indexed packets */
    fprintf(f,"VxD indexed packets (ACCEL_D3D_INDEXED): %lu drawn by index, %lu expanded (no free slot), %lu vertices uploaded (%.1f a packet), %lu indices (%.1f a packet), %lu slot refusals; vertex area at %08lX\n",
     v[2],v[3],v[4],v[2]?(double)v[4]/v[2]:0.0,v[5],v[2]+v[3]?(double)v[5]/(v[2]+v[3]):0.0,v[6],v[7]);}
  {DWORD v[8],c[8];if(texdbg2(0xBC,0xBB,v)&&(v[0]&0xFF)==0xBC&&v[1]==0xBB&&v[6]&&texdbg2(0xBC,0xAE,c)&&(c[0]&0xFF)==0xBC){ /* v7.5 (v10.468): the primitive set-up and vertices by part (TSC / 16) */
    double n=v[6],hd=v[2]*16.0/mhz/n,pre=v[5]*16.0/mhz/n,cp=c[2]*16.0/mhz/n,ck=c[3]*16.0/mhz/n;
    fprintf(f,"VxD primitive set-up and vertices (v7.5, microseconds a packet): before the first burst %.1f, burst heads %.1f (%.1f bursts, %.1f runs a packet, %.2f us a head), the vertex copy %.1f, its checks %.1f\n",
     pre,hd,v[3]/n,v[4]/n,v[3]?v[2]*16.0/mhz/v[3]:0.0,cp,ck);}}
  {DWORD v[8];if(texdbg2(0xBC,0xB9,v)&&(v[0]&0xFF)==0xBC&&v[1]==0xB9) /* v7.3 (v10.465): the ring wait bounded by progress */
    fprintf(f,"VxD ring waits (ACCEL_D3D_RINGPROG %lu): past 100 ms with the engine still moving %lu, the longest %lu ms\n",v[2],v[3],v[4]);
   if(texdbg2(0xBC,0xBA,v)&&(v[0]&0xFF)==0xBC&&v[1]==0xBA) /* v7.4 (v10.467) */
    fprintf(f,"   by length (ms): 0-1 %lu; 2-10 %lu; 11-50 %lu; 51-100 %lu; 101-500 %lu; 501+ %lu\n",v[2],v[3],v[4],v[5],v[6],v[7]);}
  /* v1.2: the rest of "in all" */
  if(texdbg(0xB9,e)&&e[0]==0xB8)fprintf(f,"VxD batches (microseconds a packet): copying in %.1f, validation %.1f, waiting for a synchronous last group %.1f; %lu fetch starts (%.1f packets a start)\n",
   e[1]*256.0/mhz/x[1],e[2]*256.0/mhz/x[1],e[3]*256.0/mhz/x[1],e[4],e[4]?(double)x[1]/e[4]:0.0);
  /* v1.4: the ring (v10.374) */
  if(texdbg(0xBB,e)&&e[0]==0xBA){fprintf(f,"VxD ring %s: %lu starts, %lu groups kicked, %lu wraps that waited for the engine\n",e[1]?"on":"off (ACCEL_D3D_RING 0)",e[2],e[3],e[4]);
  fprintf(f,"VxD fetcher starts: %lu with the gap after the last words written over (%lu words), %lu with the old position not known\n",e[5],e[6],e[7]);}
  /* v3.4 (v10.407): where XP's ring spends the CPU's time */
  {DWORD a[8],b2[8];if(texdbg2(0xBC,0x92,a)&&texdbg2(0xBC,0x93,b2)){double pk=x[1]?(double)x[1]:1.0,c=256.0/mhz;
   fprintf(f,"VxD XP ring, microseconds a packet: opening groups %.1f, fence and tail words %.1f, cache flush or MOVNTI copy %.1f, 2D registers read again %.1f, kick writes %.1f, recorder %.1f, retire waits %.1f, 2D registers put back %.1f\n",
    a[2]*c/pk,a[3]*c/pk,a[4]*c/pk,a[5]*c/pk,a[6]*c/pk,a[7]*c/pk,b2[2]*c/pk,b2[3]*c/pk);
   fprintf(f,"   kicks %lu, retires %lu, 2D re-reads %lu, put-backs %lu; a kick: flush %.1f, 2D re-read %.1f, kick writes %.1f microseconds; a retire: wait %.1f microseconds\n",
    b2[6],b2[7],b2[4],b2[5],b2[6]?a[4]*c/b2[6]:0.0,b2[4]?a[5]*c/b2[4]:0.0,b2[6]?a[6]*c/b2[6]:0.0,b2[7]?b2[2]*c/b2[7]:0.0);}}}
 ring_hang(f);
 frame_times(f,mhz);
}

static const char *why_name[13]={"target/depth surfaces","fill or shade mode","Z state","cull mode","alpha test","blend pair","stencil",
 "colour write mask","viewport","texture stage state","texture not samplable","points/lines","cube map"};
static void why_text(uint32_t w,char *o,size_t n) {
 int k;o[0]=0;
 for(k=0;k<13;k++)if(w&(1UL<<k)){if(o[0])strncat(o,", ",n-strlen(o)-1);strncat(o,why_name[k],n-strlen(o)-1);}
 if(w&~0x1FFFUL){char t[24];sprintf(t,"%s other %08lX",o[0]?",":"",(unsigned long)(w&~0x1FFFUL));strncat(o,t,n-strlen(o)-1);}
 if(!o[0])strcpy(o,"none recorded");
}
int main(int argc,char **argv) {
 HMODULE m;DWORD (WINAPI *stats)(vx_d3d_diag *);vx_d3d_diag a,b;DWORD dp2_0=0;FILE *f;DWORD t0,last,secs=600;uint32_t drops_total=0;
 if(argc>1&&atol(argv[1])>0)secs=(DWORD)atol(argv[1]);
 vxd=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);
 {DWORD o[8];texdbg(0x77,o);texdbg(0xAD,o);texdbg(0xA4,o);texdbg(0xA9,o);texdbg(0xC9,o);texdbg(0xB9,o);texdbg(0xBB,o);texdbg(0xBF,o);texdbg2(0xBC,0x94,o);} /* v3.4: and the XP ring's time counters */ /* v1.7: and 0xBF; v1.1 (v1.3: and 0xB9): cleared: what follows is the time watched */
 f=fopen("VXD3DMON.TXT","w");if(!f)return 1;
 /* v3.1: the HAL's counters are in the copy of VXDDRAW.DLL the game loads, the one in the system folder: loaded from there by its full
  * path (LoadLibrary by name took a copy next to VXD3DMON.EXE first, whose counters are not the game's, and kept that file in use) */
 {char dll[MAX_PATH];UINT n=GetSystemDirectoryA(dll,MAX_PATH-16);m=0;
  if(n&&n<MAX_PATH-16){strcpy(dll+n,dll[n-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");m=LoadLibraryA(dll);}
  if(!m){fprintf(f,"%s not loaded\n",n?dll:"the system folder's VXDDRAW.DLL");fclose(f);printf("VXDDRAW.DLL not found in the system folder\n");return 1;}
  fprintf(f,"HAL counters from %s\n",dll);}
 stats=m?(DWORD (WINAPI *)(vx_d3d_diag *))GetProcAddress(m,"VX_D3D_QueryStats"):0;
 hal_frames=m?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(m,"VX_DD_QueryFrames"):0; /* v1.8 */
 hal_blts=m?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(m,"VX_DD_QueryBlts"):0;if(hal_blts){DWORD o[53];hal_blts(o,53|0x80000000UL);} /* v7.8 */
 hal_bsig=m?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(m,"VX_DD_QueryBltSigs"):0;if(hal_bsig){static DWORD o[226];hal_bsig(o,226|0x80000000UL);} /* v7.9 */
 if(hal_frames){DWORD o[116];hal_frames(o,116|0x80000000UL);}
 hal_slow=m?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(m,"VX_D3D_QuerySlow"):0;hal_slowops=m?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(m,"VX_D3D_QuerySlowOps"):0;hal_slowio=m?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(m,"VX_D3D_QuerySlowIo"):0;hal_slowblt=m?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(m,"VX_DD_QuerySlowBlt"):0;
 if(hal_slow){DWORD o[66];hal_slow(o,66|0x80000000UL);}
 hal_phases=m?(DWORD (WINAPI *)(DWORD *,DWORD))GetProcAddress(m,"VX_D3D_QueryPhases"):0;if(hal_phases){DWORD o[144];hal_phases(o,144|0x80000000UL);} /* v4.5 */
 hal_record=m?(DWORD (WINAPI *)(DWORD))GetProcAddress(m,"VX_D3D_Record"):0; /* v8.4 (.517) */
 {int a; /* v9.7: the A/B's options (NOAB, AB=) gone with it */
  for(a=1;a<argc;a++){if(!strncmp(argv[a],"REC=",4))rec_n=(DWORD)atoi(argv[a]+4);else if(!strncmp(argv[a],"RECAT=",6))rec_at=(DWORD)atoi(argv[a]+6);else if(!_stricmp(argv[a],"SAMPLE"))sample_only=1;}}
 {DWORD o[8];texdbg2(0xBC,0xA2,o);texdbg2(0xBC,0xA5,o);texdbg2(0xBC,0xAF,o);texdbg2(0xBC,0xC7,o);texdbg2(0xBC,0xCE,o);texdbg2(0xBC,0xD5,o);} /* v8.1: D5 the critical-section holder counts */ /* v7.8: CE the indexed packets' VxD time */ /* v6.9: the vertex copy's parts too */ /* v5.3: the VxD's command words counted from here; v5.6: the copy counters cleared */
 {DWORD o[8];texdbg2(0xBD,0x80,o);} /* v2.1: the VxD's slow calls cleared */if(hal_slowblt){DWORD o[66];hal_slowblt(o,65|0x80000000UL);}
 if(!stats){fprintf(f,"VXDDRAW.DLL or VX_D3D_QueryStats not found\n");fclose(f);printf("VXDDRAW.DLL not found\n");return 1;}
 memset(&a,0,sizeof(a));a.cb=sizeof(a);
 if(!stats(&a)){fprintf(f,"VX_D3D_QueryStats refused (structure size %u)\n",(unsigned)sizeof(a));fclose(f);return 1;}
 if(sample_only){DWORD o[8];texdbg(0x6D,o);texdbg2(0xBC,0xA3,o); /* v10.6 (.549 board log: wp_part 5.3% of the VxD in a SAMPLE run): the word profiler off too (0xA2 above turned it on) */if(hal_phases){DWORD o2[2];hal_phases(o2,2|0x40000000UL);}} /* v9.8: SAMPLE: the VxD's profile (vx_prof) and the HAL's phase clocks, geometry counters and fast-path samples off again */
 fprintf(f,"VXD3DMON v10.32: HAL build v10.%lu, Direct3D %s; watching for %lu s%s\n",(unsigned long)a.build,a.enabled?"on":"off",(unsigned long)secs,sample_only?"; SAMPLE: the sampler alone (the HAL's and the VxD's own timing off; their lines below are left out)":"");
 fprintf(f,"at start: %lu DrawPrimitives2 calls, %lu engine packets, %lu primitives left out, %lu lost, %lu recoveries\n\n",(unsigned long)a.dp2,
  (unsigned long)a.native_packets,(unsigned long)a.dropped,(unsigned long)a.lost_count,(unsigned long)a.recovered);
 printf("VXD3DMON: start the game now; it stops when the game ends, or press any key here (results in VXD3DMON.TXT)\n");
 perf_ok=perf_key("PerfStats\\StartStat",0)==4;mem_note();
 smp_start(m); /* v5.0 */
 t0=last=GetTickCount();dp2_0=a.dp2;b=a;
 {DWORD (WINAPI *lastpid)(void)=m?(DWORD (WINAPI *)(void))GetProcAddress(m,"VX_D3D_LastPid"):0;HANDLE game=0;DWORD gpid=0,why=0; /* v2.2: stops when the game ends */
 for(;;){DWORD now;
  if(lastpid&&!game&&b.dp2!=dp2_0){DWORD pid=lastpid();if(pid&&pid!=GetCurrentProcessId()){game=OpenProcess(SYNCHRONIZE,FALSE,pid);if(game){gpid=pid;mod_snap(pid);}}}
  {static DWORD ls,ln;if(gpid&&ln<2){if(!ls)ls=GetTickCount();else if(GetTickCount()-ls>15000){mod_snap(gpid);ln=2;}}} /* v9.4: once more 15 s after the first, then left alone */ /* v5.0 */ /* v2.5: only a game that drew after the start (a game that ended before is not followed) */
  if(game&&WaitForSingleObject(game,0)==WAIT_OBJECT_0){why=1;break;}
  Sleep(250);now=GetTickCount(); /* v9.4: 250 ms (50 before) */
  memset(&b,0,sizeof(b));b.cb=sizeof(b);if(!stats(&b))continue;
  if(b.dropped!=a.dropped){char w[256];uint32_t d=b.dropped-a.dropped;drops_total+=d;why_text(b.drop_why,w,sizeof(w));
   fprintf(f,"%7.2f s  %lu primitive(s) left out (%lu in all): %s (bits %04lX); texture status %lu, handle %lu, stage 0 texture %lu, COLOROP %lu ARG1 %lu ARG2 %lu, ALPHAOP %lu, stage 1 COLOROP %lu\n",
    (now-t0)/1000.0,(unsigned long)d,(unsigned long)b.dropped,w,(unsigned long)b.drop_why,(unsigned long)b.drop_state[0],(unsigned long)b.drop_state[1],(unsigned long)b.drop_state[2],
    (unsigned long)b.drop_state[3],(unsigned long)b.drop_state[4],(unsigned long)b.drop_state[5],(unsigned long)b.drop_state[6],(unsigned long)b.drop_state[7]);fflush(f);}
  if(b.lost_count!=a.lost_count){fprintf(f,"%7.2f s  context marked lost: reason %lu value %08lX (%lu in all)\n",(now-t0)/1000.0,(unsigned long)b.lost_why,(unsigned long)b.lost_value,(unsigned long)b.lost_count);fflush(f);}
  if(b.recovered!=a.recovered){fprintf(f,"%7.2f s  engine recovered from a hang (%lu in all)\n",(now-t0)/1000.0,(unsigned long)b.recovered);fflush(f);}
  if(now-last>=1000){static vx_d3d_diag p;static int have;static DWORD pv[4],lf,lfr;DWORD v[4],fo[116],nl=0,nf=0,d[4],j;double fps;int pg=perf_ok&&perf_key("PerfStats\\StatData",v)==4;
   mem_note();if(hal_frames&&hal_frames(fo,116)==116){nl=fo[2]-lf;lf=fo[2];nf=fo[1]-lfr;lfr=fo[1];}fps=nf*1000.0/(now-last);
   if(pg){for(j=0;j<3;j++)d[j]=v[j]-pv[j];d[3]=v[3];}else for(j=0;j<4;j++)d[j]=0;
   if(have&&pg&&b.dp2!=p.dp2){DWORD c=nl?1:0;pf_n[c]++;for(j=0;j<4;j++)pf_sum[c][j]+=d[j];}
   if(have&&(b.dp2!=p.dp2||b.native_packets!=p.native_packets)){fps_sec(fps,b.native_packets-p.native_packets,nf,now-last);fprintf(f,"%7.2f s  %.1f fps; %lu DrawPrimitives2 calls/s, %lu engine packets/s, %lu batched primitives/s, last HRESULT %08lX; %lu frames of 75 ms+; %lu page-ins, %lu page faults, CPU %lu%%\n",(now-t0)/1000.0,fps,
    (unsigned long)(b.dp2-p.dp2),(unsigned long)(b.native_packets-p.native_packets),(unsigned long)(b.batched-p.batched),(unsigned long)b.last_hr,nl,d[0],d[1],d[3]);}
   if(pg)memcpy(pv,v,sizeof(pv));p=b;have=1;last=now;fflush(f);
   if(hal_record&&rec_n&&!rec_asked&&game&&now-t0>=rec_at*1000UL){DWORD st;rec_hold=1;rec_asked=now;st=hal_record(rec_n); /* v8.4 */
    fprintf(f,"%7.2f s  recording %lu frames asked (C:\\VXD3D.REC): %s\n",(now-t0)/1000.0,rec_n,(st&255)==1?"armed, it starts at the next flip":"refused");if((st&255)!=1)rec_hold=0;}
   if(rec_hold){DWORD st=hal_record(0xFFFFFFFFUL);if((st&255)>=3||(st&255)==0||now-rec_asked>120000UL){rec_hold=0;
    fprintf(f,"%7.2f s  recording: %s, %lu frames, %lu KB\n",(now-t0)/1000.0,(st&255)==3?"written to C:\\VXD3D.REC":(st&255)==4?"written to C:\\VXD3D.REC, cut short (its 40 MB buffer full)":(st&255)==5?"no memory for the buffer":(st&255)==6?"the file could not be written":"not done (no flips?)",(st>>8)&255,st>>16);}}
   }
  a=b;
  if(kbhit()||now-t0>=secs*1000UL)break;
 }
 fprintf(f,"\nstopped after %.1f s%s: %lu primitive(s) left out while watching\n",(GetTickCount()-t0)/1000.0,why?" (the game ended)":"",(unsigned long)drops_total);
 {vx_d3d_diag z;memset(&z,0,sizeof(z));z.cb=sizeof(z);if(stats(&z))fprintf(f,"Direct3D %s at the end (the header's reading is from before the game started)\n",z.enabled?"on":"off");} /* v3.4 */
 {DWORD o[8];memset(o,0,sizeof(o));if(why==1)Sleep(500);if(texdbg2(0xBD,0x82,o)&&o[0]==0xBD)fprintf(f,"HAL lock in the VxD at the end%s: %lu pages locked, base %08lX, %lu handles holding it; %lu requests, %lu refused or failed, %lu releases\n",why==1?" (after the game ended)":" (the game still running)",o[2],o[3],o[7],o[4],o[5],o[6]);} /* v4.9 */
 smp_stop_read(); /* v5.0 */
 if(game)CloseHandle(game);(void)gpid;}
 perf_key("PerfStats\\StopStat",0);if(!sample_only)vxd_report(f);
 {static vx_tape t;DWORD op=0,got=0;FILE *tf; /* v2.6 (v10.393): the ring tape saved for VXRING replay */
  if(vxd!=INVALID_HANDLE_VALUE&&DeviceIoControl(vxd,VX_IOCTL_TAPE,&op,4,&t,sizeof(t),&got,0)&&got==sizeof(t)&&t.captured==1&&(tf=fopen("RINGTAPE.BIN","wb"))!=NULL){fwrite(&t,sizeof(t),1,tf);fclose(tf);
   fprintf(f,"Ring tape (the hung ring's %lu words, %lu groups) saved as RINGTAPE.BIN: run VXRING replay\n",(unsigned long)t.words,(unsigned long)t.ngroups);}}
fps_report(f);smp_report(f);line_report(f,m);vxd_line_report(f);hal_frame_report(f);vxs_report(f);tlp_report(f);if(!sample_only){phase_report(f);words_report(f);}perf_report(f);if(vxd!=INVALID_HANDLE_VALUE)CloseHandle(vxd);
 fclose(f);printf("done: %lu primitive(s) left out; see VXD3DMON.TXT\n",(unsigned long)drops_total);
 return 0;
}
