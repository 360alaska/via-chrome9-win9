/* VXRING v2.8 (driver v10.398: VXRING pair; v2.7 v10.397: VXRING bisect; v2.6 v10.396: the replay log written through at each step, its parts on their own; v2.5 v10.395: the replay in the game's mode, with disk or memory load; v2.4 v10.394: the live replays; v2.3 v10.393: VXRING replay; v2.2 v10.383; v2.1 v10.381; v2.0 v10.380; v1.9 v10.379; v1.8 v10.377; v1.7 v10.376; v1.6 v10.375; v1.5 v10.373; v1.4 v10.227): the VX855 command-fetcher probe (cookbook 16.17). MIT, 2026.
 * Usage:  VXRING          read only: sequencer registers, MMIO 0x400-0x5FC, the GPU page table's location and entries
 *         VXRING fill     also the fetch tests: one 2D fill of a 64x32 offscreen rectangle sent through the fetcher from a buffer in
 *                         video memory, in XP's one-shot form (register 60 bit 0 clear, then set) and XP's ring form (bit 0 set, then clear)
 *         VXRING 3d       the 3D tests: 9 15 17 16 12 13 5 10 11 6 14 (run it first after a reboot)
 *         VXRING append   v1.5: the ring append tests 18 19 20 21 22 (a second group kicked while the fetcher runs the first)
 *         VXRING kick     v1.6: the kick onto an idle ring, tests 23 24 25 26
 *         VXRING long     v1.7: long groups, tests 27 28 29 30
 *         VXRING xp       v1.8: XP's kick, tests 31 32 33
 *         VXRING stale    v1.9: stale words past the pause, tests 34 35 36
 *         VXRING backlog  v2.0: 32 groups kicked back to back, tests 37 38 39 40
 *         VXRING restart  v2.1: the stop and restart, tests 41-48
 *         VXRING wrap     v2.2: the start order and the wrap without a stop, tests 49-52
 *         VXRING pair a b file...  v2.8: for each tape file: group b after a reset, then group a and at once group b, 6 times each
 *         VXRING bisect [file]  v2.7: the pair test: which group's run leaves the engine so that the next ring's first group hangs
 *         VXRING replay [file] [game|quiet|disk|mem]  v2.6: the log written through at each step; game: only the game's-mode
 *                         parts; quiet / disk / mem: only that one. v2.3: the ring tape the VxD kept at GTA3's first ring hang (saved as RINGTAPE.BIN; with none kept this
 *                         boot, RINGTAPE.BIN or the file named is put back) replayed 50 times in each of 15 forms (v2.4: 6 live forms): the kicks' spacing, the
 *                         status bit 0x10 wait, one kick, no fences, and the engine idle before each kick (the control)
 *         VXRING n        only test n (1..52) after the read-only part
 * v1.2 (board, v1.1: the one-shot fill from video memory with register 60 bit 0 clear drew; bit 0 set did nothing; the ring with bit 0
 * clear stayed busy, 400 bit 4): 5 a 3D triangle fetched one-shot, 6 the same triangle through the register path (the two checksums
 * must match), 7 the ring start block alone (bit 0 clear), 8 the ring with the kick and 68 written again.
 * v1.3 (board, v1.2: 1 and 8 filled, 7 idle, 5 drew nothing where 6 drew the 766-pixel triangle): the 3D triangle in other block forms:
 * 9 XP's forms (FE02 register blocks, FE03 data blocks), 10 the same with the type 0x11 block kept, 11 FE03 for the vertices only,
 * 12 the state through the registers and only the vertex blocks fetched (XP's forms), 13 the same in the v1.2 form, 14 test 6 again.
 * Each 3D test draws into its own 32-row band of a 64x256 target, so a fetch that skips the target state cannot pass on a band an
 * earlier test set up; pixels changed outside the band are counted. The fetched buffer's first words are logged.
 * v1.4 (board, v1.3: 9 10 11 and 5 all drew the register path's triangle, in their own bands; 12 and 13 drew nothing; the run's
 * starting MMIO 420-430 differed from v1.2's, where 5 drew nothing): 0x41C = 00100000 is written again just before the start words
 * (16 = 12 without it, as v1.3); 15 the command-regulator blocks (types 0x10, 0x11) through the registers and the rest fetched;
 * 17 = 9 again after 15. Each test logs the register-path draws since the VxD loaded. Target 64x512, 16 bands.
 * v1.5: 18-22 the ring append (op 5): the ring started, group A kicked, group B kicked into the band below: 18 A one fill, B a fill at
 * once; 19 A 200 fills, B at once (while A runs); 20 as 19, B after A is drawn; 21 A 200 fills, B the 3D triangle at once; 22 A one fill,
 * B the 3D triangle after A. Logged: the fetch registers before kick B and when both bands were drawn, the time to both drawn.
 * v1.6 (board, v10.374: the ring kept running drew GTA3 and Quake 2 broken; v1.5 had waited for the pixels, not the status): 23-26 the
 * kick onto a ring idle at its pause (A one fill, drawn first): 23 B the 3D triangle, status reads logged; 24 the same, then the VxD's wait
 * (8 idle reads) and the stop at once; 25 B 200 fills (the last in its own colour), logged; 26 the same with the VxD's wait and stop.
 * v1.7 (board, v1.6: the status reads busy at the first read after a kick and the VxD's wait drew the triangle whole; a group of 200 fills
 * lost its last fill, in the ring with or without the stop): 27 B 40 3D triangles in one kick, the last in its own colour; 28 the same with
 * a kick every 8; 29 the 40 triangles in the one-shot form (no ring); 30 the 200 fills in the one-shot form.
 * v1.8 (board, v1.7: 40 triangles drew whole in every form, but register 440 (vertices drawn) rose 360 for 28's five kicks against 120
 * for one kick: each kick with 68 written again starts the fetch over from the ring's start; XP's kick (s3ginv 0xBC6EC) writes 0x41C, 64,
 * 65 only): 31 = 27 with XP's kick, 32 = 28 with XP's kick, 33 = 18 (a fill after a fill) with XP's kick. The 440 rise is logged.
 * v1.9 (board, v10.378 with the ring: 179 engine recoveries in GTA3): does the fetcher read past the pause before the next group is
 * written? Band C (below B) is the trap: 2D fills in its own colour are written after A's pause before A's kick; B is written over them
 * after it. Any change in band C means words read before they were rewritten were run. 34 A a fill (drawn first), B a fill; 35 A a fill
 * (drawn first), B the 3D triangle; 36 A 200 fills, B the 3D triangle at once. XP's kick. The largest 43C read is logged.
 * v2.0 (board, v1.9: no stale words run; 43C always 0; 2048 pixels changed outside the bands in 35/36): the backlog (op 8): 32 groups,
 * each a 2D fill of its own 8x8 cell in its own colour, kicked back to back. 37 every group with a fence (0x494), waited for in full
 * (control); 38 the VxD's rule (8 idle reads, then the stop at once); 39 the fence rule (0x494 = the last fence, then idle, then the
 * stop); 40 the VxD's rule without fences. The first 48 reads after the last kick of 400 41C 418 43C 440 494 are logged, and pixels
 * changed outside a test's bands by place and colour.
 * v2.1 (board, v2.0: no group lost under any finish rule; the fence and 400 bit 17 / 41C bit 31 never moved; tests 35 and 36 changed the band
 * C of the test before them in its trap colour: words left past one run's last pause were run in the next, after the stop and a new start):
 * the stop and restart (op 9). The buffer is filled with no-op blocks first. Run 1: 8 groups (band A's first-row cells) kicked and drawn;
 * the trap: 32 fills of band C's 8x8 cells, one per 256 bytes, past run 1's last pause; the stop (68 = 0A0000, as the VxD); band A painted
 * back; run 2 started and group B (band B) kicked. 41 trap before the stop, run 2 at the buffer's middle; 42 the trap written after the
 * stop; 43 the trap written over with no-op blocks before the stop; 44 run 2 at the buffer's start (one test after another); 45 run 2 at
 * run 1's last pause (the VxD); 46 as 41 with B the 3D triangle; 47 no run 2 (the stop alone); 48 B in the one-shot form at the middle (the
 * ring-off path). Logged: which trap cells and which of band A's cells changed after the stop, after the new start, at the end.
 * v2.2 (board, v2.1: a start's 64/65 write kicks the fetcher from where its last words ended until 68 moves it; nothing ran when the new
 * start was the old position or the words there were no-ops): 49 = 41 with run 2's start written 60-63, 66, 67, 68 and only then 64/65;
 * 50-52 the wrap without a stop (XP s3ginv 0xBBD96): A two blocks before the buffer's end, the trap in the last block, B at the buffer's
 * start; 66/67 = A's pause, then 50: 64/65 = start - 16 and a kick to B's pause; 51: 64/65 = B's pause at once; 52: as 50 without the
 * kick. Then D (band A, another colour) is kicked after B as usual. Logged: A, B, D drawn, the trap, the status reads after the wrap.
 * Writes VXRING.TXT. A test that does not finish within 200 ms is stopped and the engine reset; if the reset fails the 3D and 2D engines
 * stay off until the next reboot (the desktop keeps working without acceleration). */
#include <windows.h>
#include <ddraw.h>
#include <stdio.h>
#include <string.h>
#include "vx_ring.h"
#include <io.h>
static FILE *logf;
static const char *srn[16]={"68","6A","6B","6C","6D","6E","6F","39","3F","1A","1E","4E","10","3C","1B","2D"};
static void show(const vx_ring_req *q,const vx_ring_reply *r) {
 unsigned k;
 fprintf(logf,"   result %lu; 3D register-path draws since the VxD loaded: %lu\n",(unsigned long)r->result,(unsigned long)r->reg_draws);
 fprintf(logf,"   SR");for(k=0;k<16;k++)fprintf(logf," %s=%02lX",srn[k],(unsigned long)r->sr[k]);fprintf(logf,"\n");
 fprintf(logf,"   desktop %lu bpp pitch %lu; video memory mapped %lu KB; page table at offset %08lX (%s)\n",(unsigned long)r->bpp,(unsigned long)r->pitch,
  (unsigned long)(r->fb_bytes>>10),(unsigned long)r->table_offset,r->table_readable?"readable":"outside the mapped video memory");
 fprintf(logf,"   table entries 0-7:");for(k=0;k<8;k++)fprintf(logf," %08lX",(unsigned long)r->table_first[k]);fprintf(logf,"\n");
 fprintf(logf,"   buffer at offset %08lX (entries %08lX .. %08lX), target at offset %08lX (entry %08lX)\n",(unsigned long)r->buf_offset,(unsigned long)r->table_buf[0],
  (unsigned long)r->table_buf[1],(unsigned long)r->tgt_offset,(unsigned long)r->table_tgt);
 if(!q->op){fprintf(logf,"   MMIO 400-5FC:\n");for(k=0;k<128;k+=8){unsigned j;fprintf(logf,"     %03X:",0x400+k*4);for(j=0;j<8;j++)fprintf(logf," %08lX",(unsigned long)r->mmio[k+j]);fprintf(logf,"\n");}return;}
 fprintf(logf,"   before: 400=%08lX 43C=%08lX 440=%08lX 444=%08lX 45C=%08lX 494=%08lX\n",(unsigned long)r->mmio[0],(unsigned long)r->mmio[0xF],(unsigned long)r->mmio[0x10],
  (unsigned long)r->mmio[0x11],(unsigned long)r->mmio[0x17],(unsigned long)r->mmio[0x25]);
 fprintf(logf,"   buffer GPU %08lX-%08lX, pause %08lX, %lu words; written to 420:",(unsigned long)r->gpu_start,(unsigned long)r->gpu_end,(unsigned long)r->gpu_pause,(unsigned long)r->words);
 for(k=0;k<10;k++)if(r->prec[k])fprintf(logf," %08lX",(unsigned long)r->prec[k]);fprintf(logf,"\n");
 for(k=0;k<r->samples;k++)fprintf(logf,"   at %3lu ms: 400=%08lX 43C=%08lX 444=%08lX\n",(unsigned long)r->sample_ms[k],(unsigned long)r->sample_400[k],(unsigned long)r->sample_43c[k],(unsigned long)r->sample_444[k]);
 fprintf(logf,"   %s after %lu ms (%lu status reads)%s\n",r->idle?"idle":"NOT idle",(unsigned long)r->elapsed_ms,(unsigned long)r->polls,
  r->reset_tried?(r->reset_ok?"; engine reset worked":"; engine reset FAILED: engines off until reboot"):"");
 fprintf(logf,"   after the stop: 400=%08lX 43C=%08lX 440=%08lX 444=%08lX 45C=%08lX 494=%08lX\n",(unsigned long)r->after[0],(unsigned long)r->after[1],(unsigned long)r->after[2],
  (unsigned long)r->after[3],(unsigned long)r->after[4],(unsigned long)r->after[5]);
 fprintf(logf,"   target %lux%lu: %lu pixels have the fill colour, %lu changed, first pixel %08lX: %s\n",(unsigned long)q->width,(unsigned long)q->height,(unsigned long)r->pixels_set,
  (unsigned long)r->pixels_changed,(unsigned long)r->first_pixel,(q->op==1||q->op==2)?(r->pixels_set==q->width*q->height?"FILLED BY THE FETCHED COMMAND":r->pixels_changed?"partly changed":"unchanged"):"see the checksum");
 fprintf(logf,"   checksum %08lX; capture error %lX, %lu blocks\n",(unsigned long)r->checksum,(unsigned long)r->capture_error,(unsigned long)r->capture_blocks);
 if(q->op>=5)fprintf(logf,"   register 440 (vertices drawn) rose by %lu (%lu before, %lu after)%s\n",(unsigned long)(r->after[2]-r->mmio[0x10]),(unsigned long)r->mmio[0x10],(unsigned long)r->after[2],
  (q->flags&0x4000)?"; 40 triangles drawn once each = 120":"");
 if(q->op==8){unsigned k2;
  fprintf(logf,"   backlog: %lu groups kicked; %lu cells drawn whole%s; first missing %ld; finish: %s\n",(unsigned long)r->bl_groups,(unsigned long)r->bl_cells,
   r->bl_cells==r->bl_groups?" (ALL)":" (SOME LOST)",r->bl_first_missing==0xFFFFFFFFUL?-1L:(long)r->bl_first_missing,
   (q->flags&0x1000)?"the VxD's rule":(q->flags&0x4000000)?"the fence rule":"waited for every cell");
  if(q->flags&0x2000000)fprintf(logf,"   fences: last %08lX; 494 reached it at read %ld\n",(unsigned long)r->bl_last_fence,r->bl_fence_reads==0xFFFFFFFFUL?-1L:(long)r->bl_fence_reads);
  if(q->flags&0x4001000)fprintf(logf,"   the rule decided idle after %ld reads, then stopped the fetcher\n",r->bl_decide_reads==0xFFFFFFFFUL?-1L:(long)r->bl_decide_reads);
  fprintf(logf,"   reads after the last kick (x16 cycles: 400 [bit17] 41C [bit31] 418 43C 440 494):\n");
  for(k2=0;k2<48;k2++)fprintf(logf,"     %4lu: %08lX [%lu] %08lX [%lu] %08lX %08lX %08lX %08lX\n",(unsigned long)r->bl_t[k2],(unsigned long)r->bl_s[k2][0],(unsigned long)((r->bl_s[k2][0]>>17)&1),
   (unsigned long)r->bl_s[k2][1],(unsigned long)(r->bl_s[k2][1]>>31),(unsigned long)r->bl_s[k2][2],(unsigned long)r->bl_s[k2][3],(unsigned long)r->bl_s[k2][4],(unsigned long)r->bl_s[k2][5]);}
 if(q->op==10){
  fprintf(logf,"   wrap: A at the buffer's end drawn: %s (pause %08lX); trap after it at %08lX; B at the buffer's start, pause %08lX; 66/67 = A's pause, %s\n",r->rs_a_run1?"yes":"NO",
   (unsigned long)r->pause_a,(unsigned long)r->rs_trap_gpu,(unsigned long)r->pause_b,(q->flags&1)?"64/65 = B's pause":(q->flags&2)?"64/65 = start - 16, no kick after":"64/65 = start - 16, then a kick to B's pause");
  {unsigned k2;fprintf(logf,"   first status reads after the wrap (value @ x16 cycles):");for(k2=0;k2<32;k2++)fprintf(logf,"%s%08lX@%lu",k2%6?" ":"\n     ",(unsigned long)r->k_400[k2],(unsigned long)r->k_t[k2]);fprintf(logf,"\n");}
  fprintf(logf,"   B: %lu pixels set (%s); idle after B: %s; trap cell %s; D kicked after B as usual: %s\n",(unsigned long)r->b_set,r->b_set==q->width*q->height?"WHOLE":r->b_set?"PART":"NOT DRAWN",
   r->rs_start_idle?"yes":"NO",(r->rs_c_end&1)?"RUN (the fetcher went on past A's pause)":"untouched",r->rs_a_end?"DRAWN (the ring goes on after the wrap)":"NOT drawn");}
 if(q->op==9){static const char *where[4]={"the buffer's middle","the buffer's start","run 1's last pause","the buffer's middle"};
  int wi=(q->flags&4)?1:(q->flags&0x10)?2:0;
  fprintf(logf,"   run 1: 8 groups kicked; band A cells drawn %02lX (FF = all); last pause %08lX; 440 before the stop %lu\n",(unsigned long)r->rs_a_run1,(unsigned long)r->pause_a,(unsigned long)r->rs_440[0]);
  fprintf(logf,"   trap: %lu fills (%lu words) from %08lX, written %s%s\n",(unsigned long)r->rs_trap_cells,(unsigned long)r->rs_trap_words,(unsigned long)r->rs_trap_gpu,
   (q->flags&1)?"after the stop":"before the stop",(q->flags&2)?", then written over with no-op blocks before the stop":"");
  fprintf(logf,"   the stop: %s; band C cells changed after it: %08lX\n",r->rs_stop_idle?"idle":"NOT idle",(unsigned long)r->rs_c_stop);
  if(q->flags&0x20)fprintf(logf,"   no restart\n");
  else {unsigned k2;
   if(q->flags&0x80)fprintf(logf,"   run 2's start written 60-63, 66, 67, 68, then 64/65\n");
  fprintf(logf,"   run 2 at %08lX (%s), %s; after the new start, before B: %s, band C cells %08lX, band A cells %02lX, 440 %lu\n",(unsigned long)r->rs_new_start,where[wi],
    (q->flags&0x40)?"B in the one-shot form":(q->flags&8)?"ring, B the 3D triangle":"ring, B a fill",r->rs_start_idle?"idle":"NOT idle",(unsigned long)r->rs_c_start,(unsigned long)r->rs_a_start,(unsigned long)r->rs_440[1]);
   fprintf(logf,"   first status reads after the new start (value @ x16 cycles):");for(k2=0;k2<32;k2++)fprintf(logf,"%s%08lX@%lu",k2%6?" ":"\n     ",(unsigned long)r->k_400[k2],(unsigned long)r->k_t[k2]);fprintf(logf,"\n");
   fprintf(logf,"   band B: %lu pixels set (%s), %lu changed\n",(unsigned long)r->b_set,(q->flags&8)?(r->b_set==766?"WHOLE":"CUT SHORT OR MISSING"):(r->b_set==q->width*q->height?"WHOLE":"CUT SHORT OR MISSING"),(unsigned long)r->b_changed);}
  fprintf(logf,"   at the end: band C cells %08lX, band A cells %02lX, 440 %lu: %s\n",(unsigned long)r->rs_c_end,(unsigned long)r->rs_a_end,(unsigned long)r->rs_440[2],
   (r->rs_c_end||r->rs_a_end)?"OLD WORDS WERE RUN":"no old words run");}
 if(q->op>=5&&(q->flags&0x1000000))fprintf(logf,"   stale words: %lu written after A's pause before its kick; band C (below B): %lu pixels changed: %s; largest 43C read %08lX\n",
  (unsigned long)r->poison_words,(unsigned long)r->c_changed,r->c_changed?"STALE WORDS WERE RUN (the fetcher read past the pause)":"untouched",(unsigned long)r->r43c_max);
 if(q->op>=5&&q->op<=8){unsigned k2;
  fprintf(logf,"   append: group A %lu fill(s), pause %08lX; group B pause %08lX; band A %s when B was kicked\n",(unsigned long)r->a_fills,(unsigned long)r->pause_a,(unsigned long)r->pause_b,r->a_before_b?"already drawn":"NOT yet drawn (B kicked mid-fetch)");
  fprintf(logf,"   before kick B:      400 43C 440 444 45C 494 =");for(k2=0;k2<6;k2++)fprintf(logf," %08lX",(unsigned long)r->mid[k2]);fprintf(logf,"\n");
  if(r->done_ms!=0xFFFFFFFFUL){fprintf(logf,"   both drawn after %lu ms: 400 43C 440 444 45C 494 =",(unsigned long)r->done_ms);for(k2=0;k2<6;k2++)fprintf(logf," %08lX",(unsigned long)r->done[k2]);fprintf(logf,"\n");}
  else fprintf(logf,"   both bands NOT drawn within 200 ms\n");
  fprintf(logf,"   band B: %lu pixels set, %lu changed, checksum %08lX: %s\n",(unsigned long)r->b_set,(unsigned long)r->b_changed,(unsigned long)r->b_checksum,
   r->b_set?(r->pixels_set==q->width*q->height?"BOTH GROUPS DRAWN":"B drawn, A not whole"):"B NOT drawn");
 if(q->op>=6&&q->op<=8){unsigned k2;
  fprintf(logf,"   kick onto the idle ring: first busy status at read %ld (%lu x16 cycles), idle again at read %ld (%lu x16 cycles); %s\n",
   r->k_reads_busy==0xFFFFFFFFUL?-1L:(long)r->k_reads_busy,(unsigned long)r->k_t_busy,r->k_reads_idle==0xFFFFFFFFUL?-1L:(long)r->k_reads_idle,(unsigned long)r->k_t_idle,
   (q->flags&0x1000)?"then the VxD's wait and the stop":"observed only");
  if(q->flags&0x1000)fprintf(logf,"   the VxD's wait (8 idle reads in a row) ended after %lu reads, then the fetcher was stopped\n",(unsigned long)r->k_wait_reads);
  fprintf(logf,"   first status reads (value @ x16 cycles):");for(k2=0;k2<32;k2++)fprintf(logf,"%s%08lX@%lu",k2%6?" ":"\n     ",(unsigned long)r->k_400[k2],(unsigned long)r->k_t[k2]);fprintf(logf,"\n");
  fprintf(logf,"   B's %s: %s\n",(q->flags&0x4800)?"(last) triangle (766 pixels when whole)":"last fill (2048 pixels when whole)",
   ((q->flags&0x4800)?r->b_set==766:r->b_set==q->width*q->height)?"WHOLE":"CUT SHORT OR MISSING");}
}
}
static HANDLE vxd;
/* v2.6: the log written through to the disk at each step (a crash keeps what came before it) */
static void logsync(void) {fflush(logf);_commit(fileno(logf));}
/* the target surface's bytes, read under a lock */
static int snap(LPDIRECTDRAWSURFACE st,unsigned char *to,DWORD bytes) {
 DDSURFACEDESC s;memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
 if(!st||FAILED(IDirectDrawSurface_Lock(st,0,&s,DDLOCK_WAIT,0)))return 0;
 memcpy(to,s.lpSurface,bytes);IDirectDrawSurface_Unlock(st,0);return 1;
}
static int ring(vx_ring_req *q,vx_ring_reply *r) {
 DWORD got=0;memset(r,0,sizeof(*r));
 if(!DeviceIoControl(vxd,VX_IOCTL_RING,q,sizeof(*q),r,sizeof(*r),&got,0)||got!=sizeof(*r)){fprintf(logf,"   the VxD does not take VX_IOCTL_RING (error %lu)\n",(unsigned long)GetLastError());return 0;}
 return 1;
}
/* v2.3: the replay (op 11) */
static int tape_io(DWORD op,vx_tape *t) {DWORD got=0;static unsigned char in[4+sizeof(vx_tape)];
 if(op==0)return DeviceIoControl(vxd,VX_IOCTL_TAPE,&op,4,t,sizeof(*t),&got,0)&&got==sizeof(*t);
 memcpy(in,&op,4);memcpy(in+4,t,sizeof(*t));return DeviceIoControl(vxd,VX_IOCTL_TAPE,in,sizeof(in),0,0,&got,0);}
static vx_tape t;
static int replay_load(const char *file) {
 unsigned j;FILE *tf;
 memset(&t,0,sizeof(t));
 if(!tape_io(0,&t)){fprintf(logf,"   the VxD does not take VX_IOCTL_TAPE (error %lu)\n",(unsigned long)GetLastError());return 0;}
 if(t.captured==1){if((tf=fopen("RINGTAPE.BIN","wb"))!=NULL){fwrite(&t,sizeof(t),1,tf);fclose(tf);fprintf(logf,"   the tape kept at this boot's first ring hang saved as RINGTAPE.BIN\n");}}
 else {if((tf=fopen(file,"rb"))==NULL||fread(&t,1,sizeof(t),tf)<(size_t)((char*)&t.nev-(char*)&t)){ /* v2.5: an older, shorter tape too */fprintf(logf,"   no tape kept this boot (no ring hang yet) and %s not readable\n",file);if(tf)fclose(tf);return 0;}fclose(tf);
  if(!tape_io(1,&t)){fprintf(logf,"   %s refused by the VxD (error %lu: another command-block offset, or not a tape)\n",file,(unsigned long)GetLastError());return 0;}
  fprintf(logf,"   %s put back (no tape kept this boot)\n",file);}
 fprintf(logf,"   tape: %lu words from %08lX, %lu groups (%lu kicked in that ring), %lu bpp, at the hang status %08lX 440 %lu fence %04lX, %lu ms after boot; %s%s%s\n",(unsigned long)t.words,(unsigned long)t.start,
  (unsigned long)t.ngroups,(unsigned long)t.ring_kicks,(unsigned long)t.bpp,(unsigned long)t.status,(unsigned long)t.r440,(unsigned long)t.fence,(unsigned long)t.boot_ms,
  (t.flags&4)?"fenced":"no fences",(t.flags&2)?"; not from the ring's first group (the engine's state before it not known)":"",(t.flags&1)?"; groups left out (too long)":"");
 for(j=0;j<t.ngroups;j++)fprintf(logf,"     group %2u: fence %04lX, %08lX-%08lX, %lu words\n",j,(unsigned long)t.group[j][3],(unsigned long)t.group[j][0],(unsigned long)t.group[j][1]+0x10,(unsigned long)t.group[j][2]);
 if(t.nev){static const char *en[18]={"?","kick (fence number)","ring start","park","kick from a park","VX3DN_busy (a draw in flight)","VX3DN_poll retired","single submit (op)","batch (packets)",
  "ring stop","fence late","2D registers put back","flip (offset / 4 KB)","2D submit (op, 0x100 DirectDraw)","cursor move","Lock or CPU access","engine idle","first-start probe (at / 256)"};
  fprintf(logf,"   the driver's last %lu events before the hang (microseconds before it, at 1000 TSC cycles a microsecond):\n",(unsigned long)t.nev);
  for(j=0;j<t.nev;j++){DWORD ty=t.ev[j][1]&255,ar=t.ev[j][1]>>8;fprintf(logf,"     -%9.1f  %s %lX\n",(double)(t.ev_tsc_hang-t.ev[j][0])/1000.0,ty<18?en[ty]:"?",(unsigned long)ar);}}
 fprintf(logf,"   (the replay draws over whatever is at the game's surfaces' offsets: the screen may show garbage until it is repainted)\n");logsync();
 return 1;
}
/* v2.5 (v10.395): the load threads: unbuffered reads of a 16 MB file (disk traffic and its interrupts), or 4 MB memory copies */
static volatile int load_stop;static volatile DWORD load_n;
static DWORD WINAPI disk_load(LPVOID f) {HANDLE h;static char *buf;DWORD got;
 h=CreateFileA((const char*)f,GENERIC_READ,0,0,OPEN_EXISTING,FILE_FLAG_NO_BUFFERING,0);if(h==INVALID_HANDLE_VALUE)return 0;
 if(!buf)buf=(char*)VirtualAlloc(0,65536,MEM_COMMIT,PAGE_READWRITE);if(!buf){CloseHandle(h);return 0;}
 while(!load_stop){if(!ReadFile(h,buf,65536,&got,0)||got<65536)SetFilePointer(h,0,0,FILE_BEGIN);load_n++;}
 CloseHandle(h);return 0;}
static DWORD WINAPI mem_load(LPVOID f) {static char *a,*b;(void)f;
 if(!a){a=(char*)VirtualAlloc(0,4<<20,MEM_COMMIT,PAGE_READWRITE);b=(char*)VirtualAlloc(0,4<<20,MEM_COMMIT,PAGE_READWRITE);}if(!a||!b)return 0;
 while(!load_stop){memcpy(a,b,4<<20);memcpy(b,a,4<<20);load_n++;}return 0;}
static void replay_form(DWORD flags,const char *what,int split,DWORD blin,DWORD bbytes,DWORD tlin,DWORD tpitch) {
 static vx_ring_reply r;vx_ring_req q;unsigned j,n,calls=split?50:1;DWORD runs=0,hangs=0,first=0xFFFFFFFFUL,st=0,r440=0,fn=0,msmax=0,groups=0,cyc=0,bits[2]={0,0};
 for(n=0;n<calls;n++){
  memset(&q,0,sizeof(q));q.cb=sizeof(q);q.abi=VX_RING_ABI;q.op=11;q.flags=flags|0x80000000UL|((split?1UL:50UL)<<16); /* v2.6: from the first group that sends the whole state */
  q.buf_linear=blin;q.buf_bytes=bbytes;q.tgt_linear=tlin;q.tgt_pitch=tpitch;q.width=64;q.height=32;q.color=0x0000FF00UL;
  if(split){fprintf(logf,"%s%u",n?" ":"   ",n);if(n==calls-1||n%25==24)fprintf(logf,"\n");logsync();}
  if(!ring(&q,&r))return;
  if(r.rp_hangs&&split){fprintf(logf,"[HUNG st %08lX 440 %lu fence %04lX]",(unsigned long)r.rp_status,(unsigned long)r.rp_440,(unsigned long)r.rp_fence);logsync();}
  if(r.result==1){fprintf(logf,"== %s: refused (no tape, or the command block moved)\n",what);return;}
  if(r.rp_hangs&&first==0xFFFFFFFFUL){first=runs+r.rp_first;st=r.rp_status;r440=r.rp_440;fn=r.rp_fence;}
  for(j=0;j<50;j++)if(r.rp_bits[j>>5]>>(j&31)&1){DWORD at=runs+j;if(at<64)bits[at>>5]|=1UL<<(at&31);}
  runs+=r.rp_runs;hangs+=r.rp_hangs;if(r.rp_ms_max>msmax)msmax=r.rp_ms_max;groups=r.rp_groups;cyc=r.rp_cyc_us;
  if(r.result==3&&!r.reset_ok){fprintf(logf,"== %s: the engine did not come back: stopped\n",what);return;}
  if(split&&hangs>=3){fprintf(logf,"\n   3 hangs: the rest of this form left out\n");break;} /* v2.6: each hang holds the system for about 100 ms and a reset */
 }
 fprintf(logf,"== %s: %lu runs, %lu HUNG%s",what,(unsigned long)runs,(unsigned long)hangs,hangs?"":" (none)");
 if(hangs){fprintf(logf,"; first at run %lu: status %08lX, 440 %lu, fence %04lX; hung runs:",(unsigned long)first,(unsigned long)st,(unsigned long)r440,(unsigned long)fn);for(j=0;j<64&&j<runs;j++)if(bits[j>>5]>>(j&31)&1)fprintf(logf," %u",j);}
 fprintf(logf,"; longest to idle %lu ms; %lu groups a run; %lu cycles/us\n",(unsigned long)msmax,(unsigned long)(groups&0xFFFF),(unsigned long)cyc);
 if(groups>>16)fprintf(logf,"   (from tape group %lu, the first that sends the whole state)\n",(unsigned long)(groups>>16));logsync();
}
/* v2.7 (v10.397): VXRING bisect, the pair test (op 12): which group's run leaves the engine so that a later group hangs */
static DWORD pr_blin,pr_bbytes,pr_tlin,pr_tpitch;
static int pair(unsigned a0,unsigned a1,unsigned b0,unsigned b1,DWORD mode,unsigned n,const char *what) {
 static vx_ring_reply r;vx_ring_req q;unsigned k,hb=0,ha=0,runs=0;DWORD st=0,fv=0,ast=0,afv=0;
 for(k=0;k<n&&hb<3&&ha<3;k++){
  memset(&q,0,sizeof(q));q.cb=sizeof(q);q.abi=VX_RING_ABI;q.op=12;q.flags=1UL<<16;q.a0=a0;q.a1=a1;q.b0=b0;q.b1=b1;q.mode=mode;
  q.buf_linear=pr_blin;q.buf_bytes=pr_bbytes;q.tgt_linear=pr_tlin;q.tgt_pitch=pr_tpitch;q.width=64;q.height=32;q.color=0x0000FF00UL;
  if(!ring(&q,&r))return -1;
  if(r.result==1){fprintf(logf,"   %s: refused\n",what);logsync();return -1;}
  runs++;if(r.rp_hangs){if(!hb){st=r.rp_status;fv=r.rp_fence;}hb++;}if(r.rp_fence_hist[0]){if(!ha){ast=r.rp_fence_hist[1];afv=r.rp_fence_hist[2];}ha++;}
  if(r.result==3&&!r.reset_ok){fprintf(logf,"   %s: the engine did not come back: stopped\n",what);logsync();return -1;}
 }
 fprintf(logf,"   %-44s %u runs: B hung %u%s",what,runs,hb,hb?"":"   ");
 if(hb)fprintf(logf," (status %08lX, fence %04lX)",(unsigned long)st,(unsigned long)fv);
 if(ha)fprintf(logf,"; A ITSELF hung %u (status %08lX, fence %04lX)",ha,(unsigned long)ast,(unsigned long)afv);
 fprintf(logf,"\n");logsync();return (int)hb;
}
static void bisect(const char *file) {
 unsigned g,full[64],nf=0,last,f0;char w[80];
 if(!replay_load(file))return;
 for(g=0;g<t.ngroups;g++){DWORD a=(t.group[g][0]-t.start)/4;if(a+3<t.words&&t.w[a]==0xFE020440UL&&t.w[a+2]==0x00FE0000UL)full[nf++]=g;}
 if(!nf){fprintf(logf,"   no group of the tape sends the whole state\n");return;}
 last=t.ngroups-1;f0=full[0];
 fprintf(logf,"   groups that send the whole state:");for(g=0;g<nf;g++)fprintf(logf," %u",full[g]);fprintf(logf,"\n-- A = a range run as its own ring (its own start, kicks, wait, stop), then at once B; 6 times each (3 hangs end a test)\n");logsync();
 sprintf(w,"control: B = %u alone, an engine reset before",f0);pair(f0,f0,f0,f0,3,6,w);
 sprintf(w,"B = %u alone, no reset (after the last test)",f0);pair(f0,f0,f0,f0,2,6,w);
 sprintf(w,"A = %u-%u, B = %u",f0,last,f0);pair(f0,last,f0,f0,1,6,w);
 fprintf(logf,"-- the end of A: A = k-%u, B = %u (reset before A)\n",last,f0);logsync();
 for(g=0;g<nf;g++){sprintf(w,"A = %u-%u, B = %u",full[g],last,f0);pair(full[g],last,f0,f0,1,6,w);}
 fprintf(logf,"-- one group before: A = k alone, B = %u (reset before A)\n",f0);logsync();
 for(g=0;g<nf;g++){sprintf(w,"A = %u, B = %u",full[g],f0);pair(full[g],full[g],f0,f0,1,6,w);}
 fprintf(logf,"-- which B: A = %u-%u, B = k alone (reset before A)\n",f0,last);logsync();
 for(g=0;g<nf;g++){sprintf(w,"A = %u-%u, B = %u",f0,last,full[g]);pair(f0,last,full[g],full[g],1,6,w);}
}
/* v2.8 (v10.398): VXRING pair a b file...: for each tape file (put back in turn), B = group b after an engine reset (control), then
 * A = group a, then at once B = group b (6 times each): the tape variants made from one tape, each changing one thing */
static void pairs(unsigned a,unsigned b,int nf,char **files) {
 int i;char w[96];
 for(i=0;i<nf;i++){FILE *tf=fopen(files[i],"rb");size_t n;
  memset(&t,0,sizeof(t));
  if(!tf||(n=fread(&t,1,sizeof(t),tf))<(size_t)((char*)&t.nev-(char*)&t)){fprintf(logf,"-- %s: not readable\n",files[i]);if(tf)fclose(tf);continue;}
  fclose(tf);
  if(!tape_io(1,&t)){fprintf(logf,"-- %s: refused by the VxD (error %lu)\n",files[i],(unsigned long)GetLastError());continue;}
  fprintf(logf,"-- %s\n",files[i]);logsync();
  sprintf(w,"control: B = %u after a reset",b);pair(b,b,b,b,3,6,w);
  sprintf(w,"A = %u, then B = %u",a,b);pair(a,a,b,b,1,6,w);
 }
}
static LRESULT CALLBACK rp_proc(HWND w,UINT m,WPARAM a,LPARAM b) {return DefWindowProcA(w,m,a,b);}
static int rp_surfaces(LPDIRECTDRAW dd,LPDIRECTDRAWSURFACE *sb,LPDIRECTDRAWSURFACE *st,DWORD *blin,DWORD *bbytes,DWORD *tlin,DWORD *tpitch) {
 DDSURFACEDESC s;*blin=*tlin=0;
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;s.dwWidth=512;s.dwHeight=64;
 if(SUCCEEDED(IDirectDraw_CreateSurface(dd,&s,sb,0))){memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
  if(SUCCEEDED(IDirectDrawSurface_Lock(*sb,0,&s,DDLOCK_WAIT,0))){*blin=(DWORD)s.lpSurface;*bbytes=(DWORD)s.lPitch*s.dwHeight;IDirectDrawSurface_Unlock(*sb,0);}}
 memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;s.dwWidth=64;s.dwHeight=512;
 if(SUCCEEDED(IDirectDraw_CreateSurface(dd,&s,st,0))){memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
  if(SUCCEEDED(IDirectDrawSurface_Lock(*st,0,&s,DDLOCK_WAIT,0))){*tlin=(DWORD)s.lpSurface;*tpitch=(DWORD)s.lPitch;IDirectDrawSurface_Unlock(*st,0);}}
 return *blin&&*tlin;}
static void replay(const char *file,int only,LPDIRECTDRAW dd,DWORD blin,DWORD bbytes,DWORD tlin,DWORD tpitch) { /* only: 0 all, 1 the game's mode parts, 2 no load, 3 disk, 4 memory */
 static const DWORD form[15]={0x4000000UL,0,20,100,500,2000,0x1000000UL,0x2000000UL,0x8000000UL,0x10000000UL,0x30000000UL,0x50000000UL,0x30000000UL|2000,0x50000000UL|2000,0x50000000UL|100};
 static const char *fname[15]={"the engine idle before each kick (control)","kicks back to back (as the driver)","20 us between kicks","100 us between kicks","500 us between kicks",
  "2000 us between kicks","status bit 0x10 clear before each kick (up to 2 ms)","one kick to the last pause (the tape as one group)","kicks back to back, the fences made no-ops",
  "live: each group written just before its kick (as the driver), the area as it was","live, the area zeroed first","live, the area pseudo-random first",
  "live, zeroed, 2000 us (the fetcher waits at its pause before the words are written)","live, pseudo-random, 2000 us","live, pseudo-random, 100 us"};

 unsigned k;HWND w=0;LPDIRECTDRAWSURFACE gb=0,gt=0;DWORD gw,gh,gbl=0,gbb=0,gtl=0,gtp=0;
 static const DWORD gform[4]={0x4000000UL,0,0x50000000UL|100,0x50000000UL};
 static const char *gname[4]={"engine idle before each kick (control)","kicks back to back","live, pseudo-random, 100 us","live, pseudo-random, back to back"};
 static const char *cname[3]={"in the game's mode","in the game's mode, disk load (unbuffered reads of VXRLOAD.BIN)","in the game's mode, memory load (4 MB copies)"};
 if(!replay_load(file))return;
 if(only<1)fprintf(logf,"-- on the desktop, each form 50 times, one call a run\n");
 if(only<1)for(k=0;k<15;k++){fprintf(logf,"   starting: %s\n",fname[k]);logsync();replay_form(form[k],fname[k],1,blin,bbytes,tlin,tpitch);}
 gw=t.mode_w?t.mode_w:640;gh=t.mode_h?t.mode_h:480;
 fprintf(logf,"-- setting the game's mode %lux%lux%lu\n",(unsigned long)gw,(unsigned long)gh,(unsigned long)(t.bpp?t.bpp:16));logsync();
 {WNDCLASSA c;memset(&c,0,sizeof(c));c.lpfnWndProc=rp_proc;c.hInstance=GetModuleHandleA(0);c.lpszClassName="VXRINGRP";RegisterClassA(&c);
  w=CreateWindowA("VXRINGRP","VXRING replay",WS_POPUP|WS_VISIBLE,0,0,gw,gh,0,0,c.hInstance,0);}
 if(!w||FAILED(IDirectDraw_SetCooperativeLevel(dd,w,DDSCL_EXCLUSIVE|DDSCL_FULLSCREEN))||FAILED(IDirectDraw_SetDisplayMode(dd,gw,gh,t.bpp?t.bpp:16))){
  fprintf(logf,"-- the game's mode %lux%lux%lu could not be set: the rest left out\n",(unsigned long)gw,(unsigned long)gh,(unsigned long)(t.bpp?t.bpp:16));if(w)DestroyWindow(w);return;}
 fprintf(logf,"   mode set\n");logsync();
 if(!rp_surfaces(dd,&gb,&gt,&gbl,&gbb,&gtl,&gtp)){fprintf(logf,"-- no surfaces in the game's mode\n");goto end;}
 fprintf(logf,"   surfaces: buffer %08lX, target %08lX pitch %lu\n",(unsigned long)gbl,(unsigned long)gtl,(unsigned long)gtp);logsync();
 if(only==0||only==1||only==3){fprintf(logf,"   writing VXRLOAD.BIN (16 MB)\n");logsync();}
 if(only==0||only==1||only==3){HANDLE h=CreateFileA("VXRLOAD.BIN",GENERIC_WRITE,0,0,CREATE_ALWAYS,0,0);static char z[65536];DWORD put,i;if(h!=INVALID_HANDLE_VALUE){for(i=0;i<256;i++)WriteFile(h,z,65536,&put,0);CloseHandle(h);}}
 for(k=0;k<3;k++){HANDLE th=0;DWORD tid,n0;unsigned f2;if(only>=2&&k!=only-2)continue;
  fprintf(logf,"-- %s (%lux%lu), each form 50 times one call a run\n",cname[k],(unsigned long)gw,(unsigned long)gh);logsync();
  load_stop=0;load_n=0;if(k==1)th=CreateThread(0,0,disk_load,(LPVOID)"VXRLOAD.BIN",0,&tid);if(k==2)th=CreateThread(0,0,mem_load,0,0,&tid);
  if(th)Sleep(200);n0=load_n;if(k){fprintf(logf,"   load thread %s\n",th?"running":"NOT started");logsync();}
  for(f2=0;f2<4;f2++){fprintf(logf,"   starting: %s\n",gname[f2]);logsync();replay_form(gform[f2],gname[f2],1,gbl,gbb,gtl,gtp);}
  load_stop=1;if(th){WaitForSingleObject(th,5000);CloseHandle(th);fprintf(logf,"   the load ran %lu loops during the forms\n",(unsigned long)(load_n-n0));}}
 DeleteFileA("VXRLOAD.BIN");
end:
 if(gt)IDirectDrawSurface_Release(gt);if(gb)IDirectDrawSurface_Release(gb);
 IDirectDraw_RestoreDisplayMode(dd);IDirectDraw_SetCooperativeLevel(dd,w,DDSCL_NORMAL);DestroyWindow(w);
}
#define TESTS 53
static const DWORD band[TESTS]={0,0,0,0,0,1,0,0,0,2,3,4,5,6,7,8,9,10,0,2,4,6,8,10,12,14,0,2,4,6,8,10,12,14,0,3,6,8,10,12,14,0,3,6,9,12,0,3,6,9,12,0,3};
int main(int argc,char **argv) {
 static const char *name[TESTS]={"read only","one-shot 2D fill, 60 bit 0 clear","one-shot 2D fill, 60 bit 0 set","ring 2D fill, 60 bit 0 set (as XP on 5122)","ring 2D fill, 60 bit 0 clear",
  "3D triangle fetched one-shot","3D triangle through the register path (reference for 5)","ring start block alone, 60 bit 0 clear","ring 2D fill, 60 bit 0 clear, 68 again after the kick",
  "3D fetched one-shot, XP's forms (FE02 register blocks, FE03 data blocks)","3D fetched, XP's forms, type 0x11 block kept","3D fetched, FE03 for the vertices only",
  "3D state through the registers, vertex blocks fetched (XP's forms)","3D state through the registers, vertex blocks fetched (v1.2 form)","3D triangle through the register path again",
  "3D fetched, types 0x10/0x11 through the registers first (XP's forms)","as 12 without the 0x41C write before the start words (v1.3's 12)","as 9, after 15","ring append: A one fill, B a fill at once","ring append: A 200 fills, B a fill at once (mid-fetch)","ring append: A 200 fills, B a fill after A is drawn",
  "ring append: A 200 fills, B the 3D triangle at once (mid-fetch)","ring append: A one fill, B the 3D triangle after A is drawn","kick onto the idle ring: B the 3D triangle, observed","kick onto the idle ring: B the 3D triangle, the VxD's wait and stop",
  "kick onto the idle ring: B 200 fills, observed","kick onto the idle ring: B 200 fills, the VxD's wait and stop",
  "long group: 40 3D triangles in one kick (the last in its own colour)","long group: 40 3D triangles, a kick every 8","long group: 40 3D triangles in the one-shot form (no ring)","long group: 200 fills in the one-shot form (no ring)",
  "XP's kick: 40 3D triangles in one kick","XP's kick: 40 3D triangles, a kick every 8","XP's kick: A one fill, B a fill",
  "stale words: A a fill (drawn first), B a fill","stale words: A a fill (drawn first), B the 3D triangle","stale words: A 200 fills, B the 3D triangle at once",
  "backlog: 32 groups with fences, waited for in full (control)","backlog: 32 groups with fences, the VxD's rule (8 idle reads, stop)","backlog: 32 groups with fences, the fence rule","backlog: 32 groups, the VxD's rule, no fences",
  "restart: trap before the stop, run 2 at the buffer's middle","restart: trap written after the stop, run 2 at the middle","restart: trap written over with no-op blocks before the stop, run 2 at the middle",
  "restart: trap before the stop, run 2 at the buffer's start","restart: trap before the stop, run 2 at run 1's last pause (the VxD)","restart: as 41, B the 3D triangle",
  "restart: trap before the stop, no run 2 (the stop alone)","restart: trap before the stop, B in the one-shot form at the middle",
  "restart: as 41, run 2's start with 68 before 64/65","wrap without a stop: 66/67 = A's pause, 64/65 = start - 16, then a kick to B","wrap without a stop: 66/67 = A's pause, 64/65 = B's pause at once",
  "wrap without a stop: 66/67 = A's pause, 64/65 = start - 16, no kick"};
 static const DWORD ops[TESTS]={0,1,1,2,2,3,4,2,2,3,3,3,3,3,4,3,3,3,5,5,5,5,5,6,6,6,6,6,6,7,7,6,6,5,6,6,5,8,8,8,8,9,9,9,9,9,9,9,9,9,10,10,10},flags[TESTS]={0,0,2,1,0,0,0,4,8,0x10,0x50,0x20,0x90,0x80,0,0x110,0x290,0x10,
  1UL<<16,200UL<<16,(200UL<<16)|0x400,(200UL<<16)|0x800,(1UL<<16)|0xC00,
  0x800,0x1800,200UL<<16,(200UL<<16)|0x1000,(40UL<<16)|0x4000,(40UL<<16)|0x6000,(40UL<<16)|0x4000,200UL<<16,(40UL<<16)|0xC000,(40UL<<16)|0xE000,(1UL<<16)|0x8000,
  (1UL<<16)|0x1008000UL,0x1008800UL,(200UL<<16)|0x1008800UL,0x2000000UL,0x2001000UL,0x6000000UL,0x1000UL,0,1,2,4,0x10,8,0x20,0x40,0x80,0,1,2},
  colors[TESTS]={0,0x0000FF00UL,0x00FF0000UL,0x000000FFUL,0x00FFFF00UL,0x0000FFFFUL,0x0000FFFFUL,0x00FF00FFUL,0x00808080UL,0x0000FFFFUL,0x0000FFFFUL,0x0000FFFFUL,0x0000FFFFUL,0x0000FFFFUL,0x0000FFFFUL,0x0000FFFFUL,0x0000FFFFUL,0x0000FFFFUL,0x00FF8000UL,0x0000FF80UL,0x008000FFUL,0x0000FFFFUL,0x0000FFFFUL,0x0000FFFFUL,0x0000FFFFUL,0x00FF8000UL,0x0000FF80UL,0x0000FFFFUL,0x0000FFFFUL,0x0000FFFFUL,0x00FF8000UL,0x0000FFFFUL,0x0000FFFFUL,0x00FF8000UL,0x00FF8000UL,0x0000FFFFUL,0x0000FFFFUL,0x00FFFFFFUL,0x00FFFFFFUL,0x00FFFFFFUL,0x00FFFFFFUL,0x00FF8000UL,0x0000FF80UL,0x008000FFUL,0x00FF8000UL,0x0000FF80UL,0x0000FFFFUL,0x008000FFUL,0x00FF8000UL,0x00FF8000UL,0x0000FF80UL,0x008000FFUL,0x00FF8000UL};
 static const int fill_set[]={1,5,6,7,8},set3d[]={9,15,17,16,12,13,5,10,11,6,14},setapp[]={18,19,20,21,22},setkick[]={23,24,25,26},setlong[]={27,28,29,30},setxp[]={31,32,33},setstale[]={34,35,36},setbl[]={37,38,39,40},setrs[]={41,42,43,44,45,46,47,48},setwr[]={49,50,51,52};
 static int order[TESTS+1];int norder=0;static unsigned char before[64*512*4],after[64*512*4];DWORD tbytes=0;
 DWORD sums[TESTS];int run[TESTS];
 HRESULT (WINAPI *create)(GUID *,LPDIRECTDRAW *,IUnknown *);HMODULE ddlib;LPDIRECTDRAW dd=0;LPDIRECTDRAWSURFACE sb=0,st=0;DDSURFACEDESC s;
 int first=0,last=0,k,n;DWORD blin=0,bbytes=0,tlin=0,tpitch=0;
 memset(run,0,sizeof(run));memset(sums,0,sizeof(sums));run[0]=1;order[norder++]=0;
 if(argc>1&&!strcmp(argv[1],"fill")){unsigned j;for(j=0;j<sizeof(fill_set)/sizeof(fill_set[0]);j++){run[fill_set[j]]=1;order[norder++]=fill_set[j];}}
 else if(argc>1&&!strcmp(argv[1],"wrap")){unsigned j;for(j=0;j<sizeof(setwr)/sizeof(setwr[0]);j++){run[setwr[j]]=1;order[norder++]=setwr[j];}}
 else if(argc>1&&!strcmp(argv[1],"restart")){unsigned j;for(j=0;j<sizeof(setrs)/sizeof(setrs[0]);j++){run[setrs[j]]=1;order[norder++]=setrs[j];}}
 else if(argc>1&&!strcmp(argv[1],"backlog")){unsigned j;for(j=0;j<sizeof(setbl)/sizeof(setbl[0]);j++){run[setbl[j]]=1;order[norder++]=setbl[j];}}
 else if(argc>1&&!strcmp(argv[1],"stale")){unsigned j;for(j=0;j<sizeof(setstale)/sizeof(setstale[0]);j++){run[setstale[j]]=1;order[norder++]=setstale[j];}}
 else if(argc>1&&!strcmp(argv[1],"xp")){unsigned j;for(j=0;j<sizeof(setxp)/sizeof(setxp[0]);j++){run[setxp[j]]=1;order[norder++]=setxp[j];}}
 else if(argc>1&&!strcmp(argv[1],"long")){unsigned j;for(j=0;j<sizeof(setlong)/sizeof(setlong[0]);j++){run[setlong[j]]=1;order[norder++]=setlong[j];}}
 else if(argc>1&&!strcmp(argv[1],"kick")){unsigned j;for(j=0;j<sizeof(setkick)/sizeof(setkick[0]);j++){run[setkick[j]]=1;order[norder++]=setkick[j];}}
 else if(argc>1&&!strcmp(argv[1],"append")){unsigned j;for(j=0;j<sizeof(setapp)/sizeof(setapp[0]);j++){run[setapp[j]]=1;order[norder++]=setapp[j];}}
 else if(argc>1&&!strcmp(argv[1],"3d")){unsigned j;for(j=0;j<sizeof(set3d)/sizeof(set3d[0]);j++){run[set3d[j]]=1;order[norder++]=set3d[j];}}
 else if(argc>1&&atoi(argv[1])>=1&&atoi(argv[1])<TESTS){run[atoi(argv[1])]=1;order[norder++]=atoi(argv[1]);}
 (void)first;(void)last;
 logf=fopen("VXRING.TXT","w");if(!logf)return 1;
 fprintf(logf,"VXRING v2.8 (driver v10.398): command-fetcher probe\n");logsync();
 vxd=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);
 if(vxd==INVALID_HANDLE_VALUE){fprintf(logf,"VX85598.VXD not reachable\n");fclose(logf);return 1;}
 ddlib=LoadLibraryA("DDRAW.DLL");create=ddlib?(HRESULT (WINAPI *)(GUID *,LPDIRECTDRAW *,IUnknown *))GetProcAddress(ddlib,"DirectDrawCreate"):0;
 if(create&&SUCCEEDED(create(0,&dd,0))&&SUCCEEDED(IDirectDraw_SetCooperativeLevel(dd,GetDesktopWindow(),DDSCL_NORMAL))) {
  memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;s.dwWidth=512;s.dwHeight=64;
  if(SUCCEEDED(IDirectDraw_CreateSurface(dd,&s,&sb,0))){memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
   if(SUCCEEDED(IDirectDrawSurface_Lock(sb,0,&s,DDLOCK_WAIT,0))){blin=(DWORD)s.lpSurface;bbytes=(DWORD)s.lPitch*s.dwHeight;IDirectDrawSurface_Unlock(sb,0);}}
  memset(&s,0,sizeof(s));s.dwSize=sizeof(s);s.dwFlags=DDSD_CAPS|DDSD_WIDTH|DDSD_HEIGHT;s.ddsCaps.dwCaps=DDSCAPS_OFFSCREENPLAIN|DDSCAPS_VIDEOMEMORY;s.dwWidth=64;s.dwHeight=512;
  if(SUCCEEDED(IDirectDraw_CreateSurface(dd,&s,&st,0))){memset(&s,0,sizeof(s));s.dwSize=sizeof(s);
   if(SUCCEEDED(IDirectDrawSurface_Lock(st,0,&s,DDLOCK_WAIT,0))){tlin=(DWORD)s.lpSurface;tpitch=(DWORD)s.lPitch;tbytes=tpitch*512;IDirectDrawSurface_Unlock(st,0);}}
 }
 fprintf(logf,"DirectDraw surfaces: buffer %08lX (%lu bytes), target %08lX pitch %lu (64x512, 32-row bands)\n",(unsigned long)blin,(unsigned long)bbytes,(unsigned long)tlin,(unsigned long)tpitch);
 if(tbytes>sizeof(before))tbytes=0;
 if(argc>4&&!strcmp(argv[1],"pair")){pr_blin=blin;pr_bbytes=bbytes;pr_tlin=tlin;pr_tpitch=tpitch;if(blin&&tlin)pairs((unsigned)atoi(argv[2]),(unsigned)atoi(argv[3]),argc-4,argv+4);norder=0;}
 if(argc>1&&!strcmp(argv[1],"bisect")){pr_blin=blin;pr_bbytes=bbytes;pr_tlin=tlin;pr_tpitch=tpitch;if(blin&&tlin)bisect(argc>2?argv[2]:"RINGTAPE.BIN");else fprintf(logf,"   no surfaces\n");norder=0;}
 if(argc>1&&!strcmp(argv[1],"replay")){if(blin&&tlin){int only=0;if(argc>3)only=!strcmp(argv[3],"game")?1:!strcmp(argv[3],"quiet")?2:!strcmp(argv[3],"disk")?3:!strcmp(argv[3],"mem")?4:0;replay(argc>2?argv[2]:"RINGTAPE.BIN",only,dd,blin,bbytes,tlin,tpitch);}else fprintf(logf,"   no surfaces\n");norder=0;}
 for(n=0;n<norder;n++) {
  vx_ring_req q;static vx_ring_reply r;DWORD bl=0;int snapped;
  k=order[n];
  memset(&q,0,sizeof(q));q.cb=sizeof(q);q.abi=VX_RING_ABI;q.op=ops[k];q.flags=flags[k];
  bl=tlin+band[k]*32*tpitch;
  q.buf_linear=blin;q.buf_bytes=bbytes;q.tgt_linear=bl;q.tgt_pitch=tpitch;q.width=64;q.height=32;q.color=colors[k];
  fprintf(logf,"== %d: %s\n",k,name[k]);
  if(k&&(!blin||!tlin)){fprintf(logf,"   no surfaces\n");break;}
  snapped=tbytes&&snap(st,before,tbytes);
  if(!ring(&q,&r))break;
  show(&q,&r);sums[k]=r.checksum;
  if(k&&snapped&&snap(st,after,tbytes)){DWORD o,out=0,bpx=r.bpp/8;if(!bpx)bpx=2;
   {DWORD r0=0xFFFFFFFFUL,r1=0,c0=0xFFFFFFFFUL,c1=0,fo=0xFFFFFFFFUL;
   for(o=0;o+bpx<=tbytes;o+=bpx){DWORD row=o/tpitch,col=(o%tpitch)/bpx;if(row>=band[k]*32&&row<band[k]*32+(ops[k]>=5?((flags[k]&0x1000000)||ops[k]>=9?96:64):32))continue;
    if(memcmp(before+o,after+o,bpx)){out++;if(fo==0xFFFFFFFFUL)fo=o;if(row<r0)r0=row;if(row>r1)r1=row;if(col<c0)c0=col;if(col>c1)c1=col;}}
   if(out){DWORD bv=0,av=0;memcpy(&bv,before+fo,bpx);memcpy(&av,after+fo,bpx);
    fprintf(logf,"   changed outside the bands: rows %lu-%lu, columns %lu-%lu (%s); first at row %lu column %lu: %08lX -> %08lX\n",(unsigned long)r0,(unsigned long)r1,(unsigned long)c0,(unsigned long)c1,
     (r1-r0+1)*(c1-c0+1)==out?"a whole rectangle":"scattered",(unsigned long)(fo/tpitch),(unsigned long)((fo%tpitch)/bpx),(unsigned long)bv,(unsigned long)av);}}
   fprintf(logf,"   band %lu (rows %lu-%lu); pixels changed outside the band: %lu\n",(unsigned long)band[k],(unsigned long)band[k]*32,(unsigned long)band[k]*32+31,(unsigned long)out);}
  if(q.op==3){DWORD j,m=r.words<VX_RING_STREAM?r.words:VX_RING_STREAM;fprintf(logf,"   buffer, first %lu of %lu words:\n",(unsigned long)m,(unsigned long)r.words);
   for(j=0;j<m;j++)fprintf(logf,"%s%08lX%s",j%8?" ":"     ",(unsigned long)r.stream[j],(j%8==7||j+1==m)?"\n":"");}
  fflush(logf);
  if(r.result==2||(r.result==3&&!r.reset_ok))break;
 }
 if(run[6]){static const int f3[]={5,9,10,11,12,13,14,15,16,17};unsigned j;fprintf(logf,"== 3D: register path (6) checksum %08lX\n",(unsigned long)sums[6]);
  for(j=0;j<sizeof(f3)/sizeof(f3[0]);j++)if(run[f3[j]])fprintf(logf,"   %2d %08lX %s\n",f3[j],(unsigned long)sums[f3[j]],sums[f3[j]]==sums[6]&&sums[6]?"MATCHES":"different");}
 if(st)IDirectDrawSurface_Release(st);if(sb)IDirectDrawSurface_Release(sb);if(dd)IDirectDraw_Release(dd);
 CloseHandle(vxd);fprintf(logf,"done\n");fclose(logf);printf("VXRING.TXT written.\n");return 0;
}
