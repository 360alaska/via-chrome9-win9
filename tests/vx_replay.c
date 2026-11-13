/* .517: replays a DrawPrimitives2 recording (C:\VXD3D.REC, VXD3DMON REC=n; the recorder: src/vx_d3d_rec.inc) through the core and the
 * emitter on the host: each call from the context state it had on the board, the HAL's services answered from the recording, the packets
 * validated and emitted into a counting writer. Prints per frame and in all: DrawPrimitives2 calls, packets (indexed, list), triangles,
 * the emitter's words (by register type), the core's time and the emitter's time; and any place where the replay asked the services for
 * something the recording does not have (it then answers "no").
 *   vx_replay FILE [-r N] [-s name=value ...] [-q]
 * -r N: the whole recording N times (the times are the best run's); -s: a core switch (feat, prepack, vrecipe, fastmatch,
 * fastvv, indexed, (ixv: .533, 384 always; hwcull removed), ixpass, pfvv2, ckey) instead of the recorded one; -q: totals only.
 * Built: gcc -O2 -Isrc tests/vx_replay.c src/vx_d3d_core.c src/vx_native3d_hw.c -o tests/vx_replay */
#define _POSIX_C_SOURCE 199309L
#include "vx_d3d_core.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
enum {VXR_CALL=1,VXR_BLOB=2,VXR_SVC=3,VXR_FRAME=4,VXR_END=5,VXR_TNL=6}; /* .536: VXR_TNL the context's T&L state at the start */
static const uint8_t *tnl_p;static uint32_t tnl_n;extern uint32_t vx_d3d_tnl_set(const vx_d3d_context *,const void *,uint32_t);
enum {VXS_RESOLVE=1,VXS_BIND=2,VXS_UNKNOWN=3,VXS_COLORKEY=4,VXS_MIPS=5,VXS_FACES=6,VXS_TARGET=7,VXS_TARGETDONE=8,VXS_SUBMIT=9,VXS_SUBMITN=10};
extern uint32_t vx_d3d_tnl,vx_d3d_texruns,vx_d3d_tr[3],vx_d3d_feat,vx_d3d_prepack,vx_d3d_indexed,vx_d3d_ixpass,vx_d3d_ckey_on,vx_d3d_v8,vx_d3d_delta;extern volatile uint32_t vx3dn_shadow;extern uint32_t vx3dn_sh_stat[2]; /* .527: vrecipe, fastmatch, fastvv, pfvv2, texshadow, fastchk removed */
extern uint32_t vx3dn_va_base,vx3dn_va_bytes;
typedef struct {uint32_t type,bytes;const uint8_t *p;} rec_t;
static uint8_t *file;static long fsize;static rec_t *recs;static uint32_t nrec;
static const uint8_t *blob_p[1024];static uint32_t blob_n[1024];
static uint32_t cur; /* the next record a service answer is looked for at */
#define CALL_FIXED (56+2*sizeof(vx3dn_surface)+sizeof(vx_d3d_state)) /* a call record before its commands */
static uint32_t bad_blob;
static uint32_t miss[16],misses;
static vx3dn_info info;static uint32_t fails,first_fail[4],overridden;
static uint32_t u32(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static double now_us(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec*1e6+t.tv_nsec/1e3;}
/* the next service answer of this type, skipping the submits' (the replay makes its own); 0 when the call's answers do not have it */
static const uint8_t *answer(uint32_t type) {
 uint32_t k;
 for(k=cur;k<nrec&&recs[k].type!=VXR_CALL&&recs[k].type!=VXR_FRAME;k++){
  uint32_t t;if(recs[k].type!=VXR_SVC)continue;t=u32(recs[k].p);
  if(t==VXS_SUBMIT||t==VXS_SUBMITN)continue;
  if(t==type){cur=k+1;return recs[k].p+4;}
  break;}
 miss[type&15]++;misses++;return 0;
}
static int fake_tex; /* .536: -t, a texture the recording never resolved (a draw it did not make: a recording made with the T&L probe of .534/.535) answered with a 256x256 X8R8G8B8 texture */
static int s_resolve(void *x,uint32_t h,vx3dn_surface *s){const uint8_t *a=answer(VXS_RESOLVE);(void)x;
 if(!a||u32(a)!=h){if(fake_tex&&h){s->offset=0x800000UL+(h&255)*0x40000UL;s->pitch=1024;s->width=256;s->height=256;s->format=VX3DN_ARGB8888;return 1;}return 0;}memcpy(s,a+8,sizeof(*s));return (int)u32(a+4);}
static int s_target(void *x,uint32_t h,vx3dn_surface *s){const uint8_t *a=answer(VXS_TARGET);(void)x;if(!a||u32(a)!=h)return 0;memcpy(s,a+8,sizeof(*s));return (int)u32(a+4);}
static void s_target_done(void *x){(void)x;(void)answer(VXS_TARGETDONE);}
static uint32_t s_bind(void *x,vx_d3d_input *in){const uint8_t *a=answer(VXS_BIND);uint32_t b;(void)x;if(!a)return 0x80004005UL;
 b=u32(a+4);in->vertex_bytes=u32(a+8);in->vertex_count=u32(a+12);in->vertex_stride=u32(a+16);in->fvf=u32(a+20);in->flags=u32(a+24);
 if(b==0xFFFFFFFFUL)in->vertices=0;else if(b<1024&&blob_p[b]&&blob_n[b]>=in->vertex_bytes)in->vertices=blob_p[b];else{bad_blob++;in->vertices=0;in->vertex_bytes=0;in->vertex_count=0;} /* .518: a blob defined before, long enough */
 return u32(a);}
static const uint8_t *cmd0;
static uint32_t s_unknown(void *x,const uint8_t *cmd,const uint8_t **next){const uint8_t *a=answer(VXS_UNKNOWN);(void)x;if(!a)return 0x80004005UL;
 if(next&&u32(a+8)!=0xFFFFFFFFUL)*next=cmd+u32(a+8);return u32(a);}
static int s_colorkey(void *x,uint32_t h,uint32_t *v){const uint8_t *a=answer(VXS_COLORKEY);(void)x;if(!a||u32(a)!=h)return 0;if(v)*v=u32(a+8);return (int)u32(a+4);}
static uint32_t s_mips(void *x,uint32_t h,uint32_t *o,uint32_t m){const uint8_t *a=answer(VXS_MIPS);uint32_t k;(void)x;if(!a||u32(a)!=h)return 0;for(k=0;k<m&&k<16;k++)o[k]=u32(a+12+4*k);return u32(a+8);}
static uint32_t s_faces(void *x,uint32_t h,uint32_t *o){const uint8_t *a=answer(VXS_FACES);uint32_t k;(void)x;if(!a||u32(a)!=h)return 0;for(k=0;k<6;k++)o[k]=u32(a+8+4*k);return u32(a+4);}
/* the emitter's side: validated, emitted into a counting writer */
static uint64_t words,wtype[256],packets,ipackets,tris,bad,whash;static uint32_t wsel;static double t_emit; /* .522: whash, FNV-1a of every register write and frame-buffer word the emitter made (the last repeat's) */
#define WH(a) (whash=(whash^(uint64_t)(a))*1099511628211ULL)
static uint32_t io_read(void *x,uint32_t r){(void)x;(void)r;return 0;}
/* .523: what the engine would fetch for each index drawn from the vertex area: the area's words kept (io_wfb), the type-4 registers 20
 * 21 22 24 followed, and for each index word of a burst the 8 words at the area offset + index * stride (04/22 bits 8-15) hashed with
 * the burst's mode: the same for the 10-word and the 8-word forms when both give the engine the same vertices (fhash, fidx) */
static uint32_t *area;static uint32_t r4[64],ix_left,lay_bad,lay_n,ix_over;
/* .555: the indices by triangle, a triangle with no area left out (the area as fast_triangle works it out, from the x and y the engine
 * reads): the same whether a path drops such a triangle on the CPU or sends it (nzhash, nz_n triangles; nz_zero the ones left out) */
static uint64_t nzhash=14695981039346656037ULL,nz_t[3];static uint32_t nz_k,nz_n,nz_zero;static float nz_x[3],nz_y[3];
static void nz_index(uint64_t h,uint32_t x,uint32_t y){float fx,fy;memcpy(&fx,&x,4);memcpy(&fy,&y,4);nz_t[nz_k]=h;nz_x[nz_k]=fx;nz_y[nz_k]=fy;
 if(++nz_k==3){double a=((double)nz_x[1]-nz_x[0])*((double)nz_y[2]-nz_y[0])-((double)nz_y[1]-nz_y[0])*((double)nz_x[2]-nz_x[0]);nz_k=0;
  if(a==0)nz_zero++;else{int i;nz_n++;for(i=0;i<3;i++)nzhash=(nzhash^nz_t[i])*1099511628211ULL;}}}
 /* lay_bad: index bursts whose stride and layout (04/00) disagree */static uint64_t fhash=14695981039346656037ULL,fidx;
#define FH(a) (fhash=(fhash^(uint64_t)(a))*1099511628211ULL)
static uint64_t ihash=14695981039346656037ULL,dcur; /* .528: each index drawn (and each inline vertex) folded with the hash of the type-01 and
 * texture registers its draw ran with (dcur): unlike dhash it does not depend on where packets and bursts split, so packets merged by
 * texture runs must give the same value */
#define IHH(a) (ihash=(ihash^(uint64_t)(a))*1099511628211ULL)
static uint64_t f2hash=14695981039346656037ULL; /* .528: fhash by index: each index's burst mode and its 8 words (fhash folds the mode once a burst) */
#define F2H(a) (f2hash=(f2hash^(uint64_t)(a))*1099511628211ULL)
static void fetch_note(uint32_t r,uint32_t v){
 if(r==0x43C){if(((v>>16)&255)!=0)ix_left=0;return;}
 if(r!=0x440)return;
 if(wsel==4){uint32_t g=v>>24;if(g<64)r4[g]=v&0xFFFFFFUL;if(g==0x24&&(r4[0x20]==3||r4[0x20]==1)){uint32_t st=(r4[0x22]>>8)&0xFFFF;nz_k=0;ix_left=v&0xFFFFFFUL;FH(r4[0x23]);lay_n++;
  if(!((st==32&&r4[0]==0xA80008UL&&r4[3]==0x000908UL&&r4[0x25]==0xF6C000UL)||(st==40&&r4[0]==0xA8000AUL)))lay_bad++;}return;}
 if(wsel==0&&ix_left){uint32_t base=r4[0x21]|((r4[0x22]&255)<<24),st=(r4[0x22]>>8)&0xFFFF,k,h,nh=r4[0x20]==1?2:1; /* .554: 04/20 = 1: two 16-bit indices a word, the low half first */
  for(h=0;h<nh&&ix_left;h++){uint32_t i=nh==2?(v>>(16*h))&0xFFFFUL:v,a=base+i*st;ix_left--;fidx++;IHH(dcur); /* .528 */
   if(i>r4[0x2C])ix_over++; /* .554: an index past 04/2C */
   F2H(r4[0x23]);if(area&&a>=0x4000000UL&&a+32<=0x4100000UL){uint64_t h=(14695981039346656037ULL^r4[0x23])*1099511628211ULL;const uint32_t *w=area+(a-0x4000000UL)/4;
    for(k=0;k<8;k++){FH(w[k]);F2H(w[k]);h=(h^w[k])*1099511628211ULL;}h=(h^dcur)*1099511628211ULL;nz_index(h,w[0],w[1]);}
   else{FH(0xDEADBEEFUL);F2H(0xDEADBEEFUL);nz_index(0xDEADBEEFULL,0,0);}}}
}
/* .525: the engine's type-01 registers as the words leave them (register writes after a 43C of mode 0, type 1), hashed at every draw (each
 * index or vertex burst: 04/24): the same with the set-up's shadow on and off when every value a draw runs with is the same (dhash) */
static uint32_t wmode,r1v[256],r1k[256],whdr,r2v[64][64],r2k[64][64]; /* .526: and the texture units' registers (43C = mode << 24 | 00020000, mode 0-2 a unit's texture, 20-22 its sampler) */static uint64_t dhash=14695981039346656037ULL,ndraw;
#define DHH(a) (dhash=(dhash^(uint64_t)(a))*1099511628211ULL)
static void io_write(void *x,uint32_t r,uint32_t v){(void)x;words++;WH(r);WH(v);if(r==0x43C){wsel=(v>>16)&255;wmode=v>>24;}wtype[wsel]++;fetch_note(r,v);
 if(r==0x43C)whdr=v;
 if(r==0x440&&wmode==0&&wsel==1){r1v[v>>24]=v&0xFFFFFFUL;r1k[v>>24]=1;}
 if(r==0x440&&wsel==2&&((whdr>>24)<3||((whdr>>24)>=0x20&&(whdr>>24)<0x23))&&(v>>24)<64){uint32_t m=(whdr>>24)&0x3F;r2v[m][v>>24]=v&0xFFFFFFUL;r2k[m][v>>24]=1;}
 if(r==0x440&&wmode==0&&wsel==4&&(v>>24)==0x24){uint32_t k;uint64_t d=14695981039346656037ULL;ndraw++;for(k=0;k<256;k++){DHH(r1k[k]);DHH(r1v[k]);d=(d^r1k[k])*1099511628211ULL;d=(d^r1v[k])*1099511628211ULL;}
  for(k=0;k<64*64;k++){DHH(r2k[k/64][k%64]);DHH(r2v[k/64][k%64]);d=(d^r2k[k/64][k%64])*1099511628211ULL;d=(d^r2v[k/64][k%64])*1099511628211ULL;}
  dcur=d;if(r4[0x20]!=3&&r4[0x20]!=1)for(k=0;k<(v&0xFFFFFFUL);k++)IHH(dcur);}} /* .528: an inline burst: each of its vertices */
static uint32_t io_rfb(void *x,uint32_t r){(void)x;(void)r;return 0;}
static void io_wfb(void *x,uint32_t r,uint32_t v){(void)x;words++;WH(r|0x80000000UL);WH(v);if(area&&r>=0x4000000UL&&r<0x4100000UL)area[(r-0x4000000UL)/4]=v;}
static uint32_t io_ms(void *x){(void)x;return 0;}
static vx3dn_io io={0,io_read,io_write,io_rfb,io_wfb,io_ms};
static int emit=1;
static uint32_t one(const vx3dn_packet *p) { /* .524: p validated (one_copy) */
 uint32_t last=0,k,t=0;double a=now_us();
 if(p->op==VX3DN_DRAW){
  if(p->pre.on==2&&p->bursts.n==1)t=(p->bursts.b[0]&0xFFFF)/3,ipackets++;
  else if(p->bursts.n)for(k=0;k<p->bursts.n;k++){uint32_t c=p->bursts.b[k]&0xFFFF;t+=(p->bursts.b[k]>>16)==1?(c>=2?c-2:0):c/3;}
  else t=p->vertex_count/3;}
 tris+=t;packets++;
 {extern volatile uint32_t vx3dn_shadow;extern uint32_t vx3dn_sh_ok;vx3dn_sh_ok=vx3dn_shadow&&packets>1?1UL:0UL;} /* .525: as the VxD, once the engine has had a whole set-up */
 if(emit)(void)vx3dn_execute(p,&io,&last);
 t_emit+=now_us()-a;return 0;
}
/* .524: the VxD's copy-in with the state deltas (vx_native3d_vxd.c copy_packet_d, batch_impl): within a batch call, the ranges a packet
 * marks the same are kept from the copy of the packet before (the one validated), the rest copied; the delta word cleared in the copy;
 * every range marked and the header the same as the last validated one: only the geometry's checks (vx3dn_validate_geo). dl_lie counts
 * ranges marked the same that were not (the copy would then draw with the packet before's state); dl_kept ranges kept, dl_geo packets
 * validated by the geometry alone, dl_tag tagged packets */
static vx3dn_packet cp;static uint32_t cp_hdr[27];static uint64_t dl_lie,dl_kept,dl_geo,dl_tag,dl_bytes;
static const uint32_t dl_r[7][2]=VX3DN_STATE_RANGES;
static uint32_t one_copy(const vx3dn_packet *p,int delta) {
 static unsigned char keep[1024];uint32_t k,o,same=0,cd=p->clear_depth,rc;int geo;
 if(p->op==VX3DN_DRAW&&VX3DN_IS_SAME(cd)){dl_tag++;if(delta)same=cd&VX3DN_SAME_MASK;}
 for(o=0,k=1;k<7;k++)if((same>>(k-1))&1){memcpy(keep+o,(char*)&cp+dl_r[k][0],dl_r[k][1]-dl_r[k][0]);o+=dl_r[k][1]-dl_r[k][0];}
 memcpy(&cp,p,sizeof(cp));if(p->op==VX3DN_DRAW&&VX3DN_IS_SAME(cd))cp.clear_depth=0;
 for(o=0,k=1;k<7;k++)if((same>>(k-1))&1){uint32_t b=dl_r[k][1]-dl_r[k][0];if(memcmp((char*)&cp+dl_r[k][0],keep+o,b))dl_lie++;memcpy((char*)&cp+dl_r[k][0],keep+o,b);o+=b;dl_kept++;dl_bytes+=b;}
 geo=same==VX3DN_SAME_MASK&&!memcmp(&cp,cp_hdr,sizeof(cp_hdr));if(geo)dl_geo++;
 rc=geo?vx3dn_validate_geo(&cp,&info):vx3dn_validate(&cp,&info);
 if(!rc&&getenv("VX_DUMP")){static FILE *df;static uint32_t dn; /* .552: the validated packets, whole, for tests/test_ixcheck.py (the first 400) */
  if(!df){uint32_t h[2]={sizeof(info),sizeof(cp)};FILE *o;char nm[512];df=fopen(getenv("VX_DUMP"),"wb");if(df){fwrite(h,4,2,df);fwrite(&info,sizeof(info),1,df);}
   snprintf(nm,sizeof(nm),"%s.off",getenv("VX_DUMP"));o=fopen(nm,"w");if(o){fprintf(o,"%u %u %u %u\n",(unsigned)offsetof(vx3dn_packet,vertex_count),(unsigned)offsetof(vx3dn_packet,vertices),(unsigned)offsetof(vx3dn_packet,pre),(unsigned)offsetof(vx3dn_packet,bursts)+4);fclose(o);}}
  if(df&&dn<400){fwrite(&cp,sizeof(cp),1,df);dn++;fflush(df);}}
 if(!rc&&!geo)memcpy(cp_hdr,&cp,sizeof(cp_hdr));
 if(geo&&vx3dn_validate(&cp,&info)!=rc){fprintf(stderr,"the geometry's checks and the whole validation disagree\n");exit(3);}
 return rc;
}
static uint32_t s_submit(void *x,const vx3dn_packet *p){(void)x;if(one_copy(p,0)){bad++;return 0;}return one(&cp);}
static uint32_t s_submit_n(void *x,const vx3dn_packet *p,uint32_t n,uint32_t *done){uint32_t k;int ok=0;(void)x;
 for(k=0;k<n;k++){if(one_copy(p+k,ok)){bad++;ok=0;continue;}ok=1;one(&cp);}if(done)*done=n;return 0;}
static int parse(void) {
 long o=0;uint32_t cap=1<<16;
 if(fsize<20+64||u32(file)!=0x31525856UL||u32(file+4)!=1){fprintf(stderr,"not a VXD3D.REC recording (version 1)\n");return 0;}
 if(u32(file+8)!=sizeof(vx_d3d_context)||u32(file+12)!=sizeof(vx_d3d_state)||u32(file+16)!=sizeof(vx3dn_surface)){
  fprintf(stderr,"the recording's structures (context %u, state %u, surface %u) are not this core's (%u, %u, %u)\n",u32(file+8),u32(file+12),u32(file+16),
   (unsigned)sizeof(vx_d3d_context),(unsigned)sizeof(vx_d3d_state),(unsigned)sizeof(vx3dn_surface));return 0;}
 o=20+64+sizeof(vx_d3d_context);if(o>fsize){fprintf(stderr,"the recording ends inside its header\n");return 0;}recs=malloc(cap*sizeof(rec_t));
 /* .518 (audit): every record's fixed and variable lengths checked before its payload is used; a record cut off by the end of the file
  * ends the recording (a capture cut short keeps its whole frames); a complete record that is malformed is an error */
 while(o+8<=fsize){rec_t r;uint32_t need=0;r.type=u32(file+o);r.bytes=u32(file+o+4);r.p=file+o+8;if((uint64_t)o+8+r.bytes>(uint64_t)fsize)break;
  if(r.type==VXR_END)break;
  if(r.type==VXR_CALL){need=CALL_FIXED;if(r.bytes>=need)need+=u32(r.p+12);}
  else if(r.type==VXR_BLOB){need=8;if(r.bytes>=8)need+=u32(r.p+4);}
  else if(r.type==VXR_SVC){static const uint32_t sl[11]={0,4+4*(2+sizeof(vx3dn_surface)/4),4+32,4+12,4+12,4+76,4+32,4+4*(2+sizeof(vx3dn_surface)/4),4,8,16};
   uint32_t ty=r.bytes>=4?u32(r.p):0;if(!ty||ty>10){fprintf(stderr,"record %u: unknown service answer %u\n",nrec,ty);return 0;}need=sl[ty];}
  else if(r.type==VXR_TNL){tnl_p=r.p;tnl_n=r.bytes;o+=8+r.bytes;continue;} /* .536 */
  else if(r.type!=VXR_FRAME){fprintf(stderr,"record %u: unknown type %u\n",nrec,r.type);return 0;}
  if(r.bytes<need){fprintf(stderr,"record %u (type %u): %u bytes, it needs %u\n",nrec,r.type,r.bytes,need);return 0;}
  if(r.type==VXR_BLOB){uint32_t id=u32(r.p);if(id>=1024||blob_p[id]){fprintf(stderr,"record %u: blob %u out of range or defined twice\n",nrec,id);return 0;}blob_p[id]=r.p+8;blob_n[id]=u32(r.p+4);}
  if(nrec==cap){cap*=2;recs=realloc(recs,cap*sizeof(rec_t));}
  recs[nrec++]=r;o+=8+r.bytes;}
 while(nrec&&recs[nrec-1].type!=VXR_FRAME)nrec--; /* a frame cut short (the buffer full) left out */
 return 1;
}
typedef struct {const char *name;uint32_t *v;int idx;} sw_t;
int main(int argc,char **argv) {
 static vx_d3d_context ctx;static uint32_t rstates[256];
 vx_d3d_services sv;uint32_t sw[16],rep=1,r,k,quiet=0,frames=0,calls=0;int a;
 double best=0,best_emit=0;uint64_t fw=0,fp=0,fi=0,ft=0;
 extern uint32_t vx_d3d_cp[6];static uint32_t force_cull;
 sw_t sws[]={{"feat",&vx_d3d_feat,0},{"prepack",&vx_d3d_prepack,1},
  {"indexed",&vx_d3d_indexed,6},{"ixpass",&vx_d3d_ixpass,8},{"ckey",&vx_d3d_ckey_on,13},{"v8",&vx_d3d_v8,-1},{"delta",&vx_d3d_delta,-1},{"shadow",(uint32_t*)&vx3dn_shadow,-1},{"texruns",&vx_d3d_texruns,-1},{"tnl",&vx_d3d_tnl,14},{"hold",&vx_d3d_hold_on,-1},}; /* .572: cullpass gone (always on; -s ixpass=0 gives the loop) */ /* .555: pass=0, the strips and fans by the old path (a feature under test) */ /* .550: hold=1 keeps the queue across calls (sent at each frame record, as Flip does) */ /* .535: the T&L probe (a recording made with it) */ /* .528: texruns (1 unless -s texruns=0, as the HAL sets it) */ /* .527: texshadow, fastchk, vrecipe, fastmatch, fastvv, pfvv2 removed */ /* .526 */ /* .525: shadow */ /* .523: v8; .524: delta */ /* .522: ixpf not in the recording (-1: the build's default) */
 FILE *f;
 if(argc<2){fprintf(stderr,"vx_replay FILE [-r N] [-s name=value ...] [-q] [-n]\n");return 2;}
 f=fopen(argv[1],"rb");if(!f){perror(argv[1]);return 2;}
 fseek(f,0,SEEK_END);fsize=ftell(f);fseek(f,0,SEEK_SET);file=malloc(fsize);if(fread(file,1,fsize,f)!=(size_t)fsize){fclose(f);return 2;}fclose(f);
 if(!parse())return 2;
 memcpy(sw,file+20,64);vx_d3d_texruns=1; /* .528: as the HAL sets it (the recording's word 10 was PFVV2's before .528) */
 for(k=0;k<sizeof(sws)/sizeof(sws[0]);k++)if(sws[k].idx>=0)*sws[k].v=sw[sws[k].idx];
 for(a=2;a<argc;a++){
  if(!strcmp(argv[a],"-r")&&a+1<argc)rep=(uint32_t)atoi(argv[++a]);
  else if(!strcmp(argv[a],"-q"))quiet=1;
  else if(!strcmp(argv[a],"-n"))emit=0;
  else if(!strcmp(argv[a],"-t"))fake_tex=1; /* .536 */
  else if(!strcmp(argv[a],"-c")&&a+1<argc)force_cull=(uint32_t)atoi(argv[++a]); /* .567: every call drawn with this cull mode (2 clockwise, 3 counter-clockwise), to test the culled paths on a recording made with none */
  else if(!strcmp(argv[a],"-s")&&a+1<argc){char *e=strchr(argv[++a],'=');int hit=0;if(e){*e=0;for(k=0;k<sizeof(sws)/sizeof(sws[0]);k++)if(!strcmp(sws[k].name,argv[a])){*sws[k].v=(uint32_t)strtoul(e+1,0,0);hit=1;}}
   if(!hit){fprintf(stderr,"unknown switch %s\n",argv[a]);return 2;}overridden=1;}}
 printf("recording: %ld bytes, %u records; switches:",fsize,nrec);for(k=0;k<sizeof(sws)/sizeof(sws[0]);k++)printf(" %s %u",sws[k].name,*sws[k].v);printf("\n");
 vx3dn_va_base=0x4000000UL;vx3dn_va_bytes=0x100000UL;area=calloc(0x100000/4,4); /* .523 */ /* the vertex area, as the VxD's: indexed packets drawn by index */
 memset(&info,0,sizeof(info));info.cb=sizeof(info);info.abi=0x10000;info.ready=1;info.fb_bytes=0x10000000UL;
 memset(&sv,0,sizeof(sv));sv.submit=s_submit;sv.submit_n=s_submit_n;sv.resolve=s_resolve;sv.unknown=s_unknown;sv.bind_vertices=s_bind;sv.colorkey=s_colorkey;
 sv.mips=s_mips;sv.faces=s_faces;
 if(sw[11]){sv.target=s_target;sv.target_done=s_target_done;} /* .518 (audit): as the HAL had them: ACCEL_D3D_RTTEX off leaves them unset (render targets resolved) */
 for(r=0;r<rep;r++){
  double t_core=0,t0;uint64_t pw=0,pp=0,pi=0,pt=0;uint32_t fcalls=0;
  memcpy(&ctx,file+20+64,sizeof(ctx));if(tnl_p&&!vx_d3d_tnl_set(&ctx,tnl_p,tnl_n))fprintf(stderr,"the T&L state record (6) is %u bytes, not this build's: left out (a recording before .540 kept a pointer in it)\n",tnl_n); /* .536; .540: reported */
  words=packets=ipackets=tris=bad=whash=0;whash=14695981039346656037ULL;t_emit=0;frames=0;calls=0;misses=0;fails=0;bad_blob=0;memset(first_fail,0,sizeof(first_fail));memset(wtype,0,sizeof(wtype));memset(miss,0,sizeof(miss));
  info.generation=ctx.generation;
  for(k=0;k<nrec;k++){
   if(recs[k].type==VXR_FRAME){(void)vx_d3d_hold_send(&sv);frames++; /* .550: the flip sends what is held */
    if(!quiet&&r+1==rep)printf("frame %u: %u calls, %llu packets (%llu indexed), %llu triangles, %llu words\n",frames,fcalls,
     (unsigned long long)(packets-pp),(unsigned long long)(ipackets-pi),(unsigned long long)(tris-pt),(unsigned long long)(words-pw));
    pw=words;pp=packets;pi=ipackets;pt=tris;fcalls=0;continue;}
   if(recs[k].type!=VXR_CALL)continue;
   {const uint8_t *p=recs[k].p,*q;vx_d3d_input in;uint32_t err=0,b,ss=sizeof(vx3dn_surface);double e0;
    memset(&in,0,sizeof(in));in.flags=u32(p);in.fvf=u32(p+4);in.vertex_stride=u32(p+8);in.command_bytes=u32(p+12);in.render_state_count=u32(p+16)>256?256:u32(p+16);
    b=u32(p+20);if(b<1024&&blob_p[b]&&blob_n[b]>=u32(p+24)){in.vertices=blob_p[b];in.vertex_bytes=u32(p+24);in.vertex_count=u32(p+28);}else if(b!=0xFFFFFFFFUL)bad_blob++;
    ctx.generation=u32(p+32);ctx.lost=u32(p+36);ctx.recording=u32(p+40);q=p+48;
    memcpy(&ctx.target,q,ss);q+=ss;memcpy(&ctx.depth,q,ss);q+=ss;memcpy(&ctx.state,q,sizeof(vx_d3d_state));q+=sizeof(vx_d3d_state);
    ctx.target_handle=u32(q);ctx.depth_handle=u32(q+4);q+=8;if(force_cull)ctx.state.rs[22]=force_cull;
    in.commands=q;cmd0=q;in.render_states=rstates;info.generation=ctx.generation;
    cur=k+1;e0=t_emit;t0=now_us();
    {uint32_t rc=vx_d3d_dp2(&ctx,&in,&sv,&err);if(rc){fails++;if(!first_fail[0]){first_fail[0]=frames+1;first_fail[1]=calls+1;first_fail[2]=rc;first_fail[3]=err;}}} /* .518 (audit): a failed call reported */
    t_core+=now_us()-t0-(t_emit-e0);calls++;fcalls++;}
  }
  if(!r||t_core<best){best=t_core;best_emit=t_emit;}
  fw=words;fp=packets;fi=ipackets;ft=tris;
 }
 if(!frames)frames=1;
 printf("%u frames, %u DrawPrimitives2 calls: %.1f packets a frame (%.1f indexed), %.0f triangles a frame (%.1f a packet), %.0f words a packet; %llu packets refused by the validation\n",
  frames,calls,(double)fp/frames,(double)fi/frames,(double)ft/frames,fp?(double)ft/fp:0.0,fp?(double)fw/fp:0.0,(unsigned long long)bad);
 printf("host time a frame (best of %u): the core %.3f ms (%.2f us a call, %.0f ns a triangle), the emitter %.3f ms (%.2f us a packet)\n",rep,
  best/1000.0/frames,calls?best/calls:0.0,ft?best*1000.0/ft:0.0,best_emit/1000.0/frames,fp?best_emit/fp:0.0);
 printf("words by register type a packet:");for(k=0;k<256;k++)if(wtype[k])printf(" %02X %.1f",k,(double)wtype[k]/(fp?fp:1));printf("\n");
 {extern uint32_t vx_d3d_cq_calls,vx_d3d_hold_stat[6];printf("VxD batch calls %.1f a frame (all repeats; .550 hold: %u calls ended holding, %u later sends)\n",(double)vx_d3d_cq_calls/frames/(rep?rep:1),vx_d3d_hold_stat[0],vx_d3d_hold_stat[2]);}
 printf("emitted words hash %016llX (every register write and vertex-area word, in order)\n",(unsigned long long)whash); /* .522 */
 printf("fetched vertices hash %016llX over %llu indices (the 8 words the engine reads at each index drawn from the vertex area, with the burst modes)\n",(unsigned long long)fhash,(unsigned long long)fidx); /* .523 */
 printf("fetched vertices hash by index %016llX (the same, each index with its burst's mode: independent of where bursts split)\n",(unsigned long long)f2hash); /* .528 */
 printf("triangles with area, by index and state: hash %016llX over %u (%u with no area left out of the hash)\n",(unsigned long long)nzhash,nz_n,nz_zero); /* .555 */
 {extern uint32_t vx_d3d_ixs[4],vx_d3d_fan[3];printf("indexed strips as lists: %u (%u triangles, %u of them an index twice, left out), %u by the old path; fans passed through: %u (%u triangles), %u by the loop\n",vx_d3d_ixs[0],vx_d3d_ixs[1],vx_d3d_ixs[3],vx_d3d_ixs[2],vx_d3d_fan[0],vx_d3d_fan[1],vx_d3d_fan[2]);} /* .555 */
 {extern uint32_t vx_d3d_tokrun[2],vx_d3d_px[6],vx_d3d_ix[4];printf("tokens taken whole: %u (%u triangles); spans: %u triangles, %u vertices in %u runs; vertices converted in all %u\n",vx_d3d_tokrun[0],vx_d3d_tokrun[1],vx_d3d_px[0],vx_d3d_px[1],vx_d3d_px[2],vx_d3d_ix[1]);} /* .556 */
 {extern uint32_t vx_d3d_ref[4];printf("runs by the vertices used: %u (%u triangles, %u vertices), %u ended by a failing vertex\n",vx_d3d_ref[0],vx_d3d_ref[1],vx_d3d_ref[2],vx_d3d_ref[3]);} /* .557 */
 printf("culled pass-through (forced cull %u): %u runs filtered, %u triangles culled in them and in fans, %u runs with nothing kept, %u by the vertices used, %u with a failing vertex in the span, %u ended by a failing vertex\n",force_cull,vx_d3d_cp[0],vx_d3d_cp[1],vx_d3d_cp[2],vx_d3d_cp[3],vx_d3d_cp[4],vx_d3d_cp[5]); /* .567 */
 printf("index bursts %u, of them with a layout (04/00) that does not match their stride: %u\n",lay_n,lay_bad);
 printf("index bursts: %u heads, %u index data words, %u indices; %u indices past 04/2C\n",vx3dn_ix16_stat[0],vx3dn_ix16_stat[1],vx3dn_ix16_stat[2],ix_over); /* .554 */
 printf("draws %llu, the type-01 and texture registers they ran with hashed %016llX (the set-up's shadow %u: %u words left out)\n",(unsigned long long)ndraw,(unsigned long long)dhash,vx3dn_shadow,vx3dn_sh_stat[1]);
 {extern uint32_t vx3dn_texrun_stat[2];printf("index-state hash %016llX (each index drawn with the registers its draw ran with; texruns %u: %u runs added in the HAL, %u template changes with other state different, %u packets cut with every run used; the emitter drew %u packets with runs, %u texture switches)\n",
  (unsigned long long)ihash,vx_d3d_texruns,vx_d3d_tr[0],vx_d3d_tr[1],vx_d3d_tr[2],vx3dn_texrun_stat[0],vx3dn_texrun_stat[1]);} /* .528 */
 printf("state deltas (delta %u): tagged packets %llu, ranges kept %llu (%llu state bytes not copied), ranges marked the same that were not %llu, packets validated by their geometry alone %llu\n",vx_d3d_delta,(unsigned long long)dl_tag,(unsigned long long)dl_kept,(unsigned long long)dl_bytes,(unsigned long long)dl_lie,(unsigned long long)dl_geo);
 if(misses){printf("answers the recording did not have: %u (",misses);for(k=1;k<16;k++)if(miss[k])printf(" type %u: %u",k,miss[k]);printf(" )\n");}
 if(bad_blob)printf("vertex buffers the recording did not have (or too short): %u\n",bad_blob);
 if(fails)printf("DrawPrimitives2 calls that failed: %u, the first: frame %u, call %u, result %08X, error offset %u\n",fails,first_fail[0],first_fail[1],first_fail[2],first_fail[3]);
 /* .518 (audit): a strict replay (the recorded switches) fails when it diverged; with -s the service sequence may differ by design */
 if(misses||bad_blob||fails||bad){printf("%s\n",overridden?"diverged (switches changed with -s: may be expected)":"DIVERGED from the recording");return overridden?0:1;}
 return 0;
}
