/* VXGL's packet queue (v10.275). Offscreen draw packets are kept here and sent to the VxD several at a time (VX_IOCTL_3DN_SUBMITN): one
 * call, one sequencer open and close and one fetch start for the group instead of one each per packet (on the board each packet paid
 * about 3 microseconds for the call, 9 for the open and close and 13 for the wait before its start). Registry ACCEL_OPENGL_BATCH (default
 * 16, at most VX3DN_BATCH_MAX): the packets a call; 0 or 1 = every packet alone as before.
 * The queue is sent before anything can see or change what its packets use: a clear or any other packet that goes alone, swrast's
 * access (vxgl_fb_lock), the kept locks let go (vxgl_fb_settle: swaps, blits, the front buffer, glFinish, a context switch, a thread
 * leaving, the surfaces made again), glFlush, a texture written or freed, and the surfaces freed. Shared by the ICD (vxgl_icd.c sends
 * through the VxD) and the host test (gl/host/vxgl_host.c draws the packets in its model, at the time they are sent). */
#include "vxgl.h"
#include "vx_d3d_core.h"
DWORD vxgl_batch_max=32; /* v10.315: 32 (the VxD's most a call; board, v10.313: 1274 of 1941 calls were sent for a full queue of 16) */
static vx3dn_packet q[VX3DN_BATCH_MAX];
static uint32_t qn;
unsigned long vxgl_batch_calls,vxgl_batch_packets,vxgl_batch_flushes[4]; /* calls sent, packets in them; flushes for: a full queue, a lone packet, the others */
/* v10.279 (review): a queued packet was answered VX3DN_OK when it was queued, so a lost engine found when the queue went was only logged:
 * the context never marked its core lost (a packet sent alone does: vx_d3d_submit_result), so its surfaces were not made again. Each loss
 * counts here; vxgl_submit answers the next packet of every context that has not heard of it yet VX3DN_LOST. (A queue still refused as
 * busy after the send's half second of retries is dropped, as a packet sent alone is: the flush points promise the packets are done.) */
unsigned long vxgl_batch_lost;
/* v10.366: how far the packets have got, for the texture copies written while the engine draws (vxgl_tex.c ACCEL_OPENGL_TEXRING): q the
 * packets handed over (queued, or sent alone), sent those given to the VxD (or dropped), done those the engine has finished. A VxD call
 * that takes a packet first retires the fetch group still running (vx_native3d_vxd.c fx_start / fx_retire: one group in flight at most),
 * so every packet given before that call is finished; a wait for the engine (vxgl_seq_idle) finishes all that were given. */
/* v10.378: with the VxD's ring on (ACCEL_D3D_RING, TEXDBG 0xBA at startup) a VxD call no longer retires the groups before it (they stay
 * queued in the engine's ring): only a wait for the engine (vxgl_seq_idle) or a recovery finishes packets */
uint32_t vxgl_seq_q,vxgl_seq_sent,vxgl_seq_done;int vxgl_seq_ring;
void vxgl_seq_idle(void) {vxgl_seq_done=vxgl_seq_sent;}
void vxgl_seq_alone(uint32_t rc) {uint32_t b=vxgl_seq_sent;vxgl_seq_q++;vxgl_seq_sent++;if(rc==VX3DN_OK&&!vxgl_seq_ring)vxgl_seq_done=b;else if(rc==VX3DN_RECOVERED)vxgl_seq_done=vxgl_seq_sent;}
int vxgl_batch_add(const vx3dn_packet *p) {
 uint32_t m=vxgl_batch_max>VX3DN_BATCH_SEND?VX3DN_BATCH_SEND:vxgl_batch_max;int full; /* .560: VX3DN_BATCH_SEND (a call's bytes) */
 if(m<2||p->op!=VX3DN_DRAW||(p->clear_flags&VX3DN_DRAW_CLEAR)||!p->target.offset||p->vertex_count>VX3DN_MAX_VERTICES)return 0;
 /* v10.330 (review #25): the room checked, the packet queued and a full queue sent under one hold of the engine lock (recursive): the
  * send ran after the lock was let go, so another thread could queue between (the callers hold it already: vxgl_hw_flush's core flush) */
 vxgl_engine_lock();
 if(qn>=VX3DN_BATCH_SEND)vxgl_batch_flush_why(0);
 vx_d3d_packet_copy(&q[qn],p);qn++;vxgl_seq_q++;full=qn>=m;
 if(full)vxgl_batch_flush_why(0);
 vxgl_engine_unlock();
 return 1;
}
/* the packets that went: 0 when the queue was empty */
uint32_t vxgl_batch_flush_why(int why) {
 uint32_t off=0,done,rc,sent=0;
 int zero=0,again=0; /* v10.426; .532: again (below) */
 vxgl_engine_lock(); /* v10.330 (review #25): the count read under the lock too */
 if(!qn){vxgl_engine_unlock();return 0;}
 if(qn&&why>=0&&why<4)vxgl_batch_flushes[why]++;
 while(off<qn) {
  uint32_t before=vxgl_seq_sent;
  done=0;rc=vxgl_batch_send(q+off,qn-off,&done);
  vxgl_batch_calls++;vxgl_batch_packets+=done;sent+=done;
  if(done>qn-off)done=qn-off;
  vxgl_seq_sent+=done;if(done&&!vxgl_seq_ring)vxgl_seq_done=before;if(rc==VX3DN_RECOVERED)vxgl_seq_done=vxgl_seq_sent; /* v10.366 */
  off+=done;
  if(rc==VX3DN_OK){if(done){zero=0;continue;}if(++zero<3)continue; /* v10.426 (review, side note to #82): OK with nothing taken is sent again (twice), then logged and dropped as BUSY */
   rc=VX3DN_BUSY;}
  {static int n;if(n++<64)vxgl_log("batch: %lu packets, %lu taken, result %lu\n",(unsigned long)(qn-off+done),(unsigned long)done,(unsigned long)rc);}
  if(rc==VX3DN_LOST)vxgl_batch_lost++;
  if(rc==VX3DN_LOST||rc==VX3DN_BUSY)break; /* the engine is gone (or still taken after the send's retries): the rest cannot be drawn */
  if(rc==VX3DN_RECOVERED){if(!done&&again++)break;continue;} /* the engine was reset under the group: the rest goes on; .532 (review of .530, B2): with none taken the
    * VxD reset the engine before issuing any of them (a taken packet is counted: done is what was issued), so they are sent once more */
  if(off<qn){off++;vxgl_seq_sent++;} /* the refused packet is dropped, as a refused single one */
 }
 if(off<qn)vxgl_seq_sent+=qn-off; /* v10.366: dropped (the engine lost or still taken): never drawn */
 qn=0;
 vxgl_engine_unlock();
 return sent;
}
uint32_t vxgl_batch_flush(void) {return vxgl_batch_flush_why(2);}
