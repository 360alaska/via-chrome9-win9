/* .544: four-wide transform and eligible lighting run in vx_d3d_tnl_soa.inc.
 * The scalar route handles clipped edges and state outside the grouped path.
 * tn_ix checks positions before it maps finished records to packet slots.
 * .536: the driver's own transform and lighting (included by vx_d3d_core.c before dp2_body). ACCEL_D3D_TNL (VxD key, default 0; the
 * feature under test, .534's probe grown up): the HAL offers hardware T&L (vx_d3d_hal.c: D3DDEVCAPS_HWTRANSFORMANDLIGHT, 8 lights,
 * VertexProcessingCaps 3F), so Direct3D 7's T&L HAL device and Direct3D 8's hardware vertex processing send the T&L commands and
 * untransformed vertices (DirectX 7 DDI), and this file does the vertex work:
 *  - the state: SETTRANSFORM (1 world, 2 view, 3 projection, 16-23 the texture matrices), SETMATERIAL, SETLIGHT (enable, disable, a
 *    D3DLIGHT7), ZRANGE, kept per context (tnl_of); CREATELIGHT and SETCLIPPLANE read past (no user clip planes are offered);
 *  - a draw of untransformed vertices (FVF position XYZ; XYZB* and points, lines and the _IMM commands are left out and counted): its
 *    triangles' vertices transformed into a scratch vertex buffer of pretransformed vertices (XYZRHW, DIFFUSE, SPECULAR, one or two
 *    coordinate sets), appended within the call, and the draw sent on as an indexed triangle list (26) into that buffer, through the same
 *    path as every pretransformed draw (templates, indexed packets, the VxD);
 *  - per vertex: position by world x view x projection; lighting (LIGHTING with normals: global ambient, emissive, directional and point
 *    lights with range and attenuation, spot lights (.645: the cone) and specular lighting (.645: SPECULARENABLE, LOCALVIEWER), material sources from COLORVERTEX and the
 *    *MATERIALSOURCE states; no normals: ambient and emissive only, as Direct3D does); vertex fog (FOGVERTEXMODE
 *    linear, exp, exp2 by eye depth or by range with RANGEFOGENABLE) into the specular alpha; the coordinate sets per stage: TEXCOORDINDEX's
 *    set or its generation (camera-space normal, position, reflection vector), then TEXTURETRANSFORMFLAGS's count with the stage's texture
 *    matrix ((u, v, 1, 0) for a set read from the vertex, (x, y, z, 1) for a generated one; PROJECTED divides by the last);
 *  - clipping: every triangle is tested against the six planes in clip space (near z >= 0, far z <= w, and since .543 x and y against
 *    w); one across them is clipped there (its new vertices interpolated before the divide and appended to the scratch buffer), one wholly
 *    outside a plane is dropped; a draw whose clipped output would not fit clips at near and far only, the rest by the core's screen
 *    clipper as for pretransformed draws;
 *  - for the draw, each active stage's TEXCOORDINDEX is set to its own output set and TEXTURETRANSFORMFLAGS to 0 (the pretransformed path
 *    reads them), and put back after it.
 * Counts (vx_d3d_tn, QueryPhases 290-301): [0] draws transformed, [1] their triangles, [2] vertices transformed, [3] triangles clipped,
 * [4] triangles dropped wholly outside, [5] vertices made by clipping, [6] scratch buffer restarts (the batch sent first), [7] draws left
 * out (unsupported command or FVF, a vertex past the buffer, too many triangles), [8] lit vertices, [9] fogged vertices, [10] vertices
 * with a generated or transformed coordinate set, [11] state not done (.646: a light with every one of the 64 slots in use and enabled, a stage past the second's coordinates;
 * .645: spot lights and specular lighting are done). */
uint32_t vx_d3d_tn[12];
/* .667: the states not done by cause, counted where each is found (a set-up kept for later draws is not counted again): [0] a light with no
 * slot (64 lit at once), [1] a switched-off light's definition dropped from a full park (tnl_park), [2] a
 * third stage asking for a third coordinate set (it gets the second), [3] TEXTURETRANSFORMFLAGS this does not do */
uint32_t vx_d3d_tn_nd[4];
typedef struct {uint32_t type,on,have;float dif[4],spe[4],amb[4],pos[3],dir[3],range,falloff,a0,a1,a2,theta,phi;} tnl_light;
/* .540 (review): the state holds no pointer (32-bit floats and words only: a recording's record 6 has one layout on Windows 98 and on a
 * 64-bit host; .536 kept the context's address in it and a host replay dropped it as the wrong size). The context's key is kept beside
 * it; vx_d3d_init gives a new context fresh state (vx_d3d_tnl_reset: a context made again at the same address inherited the old
 * matrices and lights), and the table holds more contexts than the HAL's 8 (a ninth evicted the first). */
/* .646 (board, 3DMark 2001: every scene dark): a light's index is any number the application picks (Direct3D 7 and 8 keep lights in a
 * sparse table); .536-.645 kept indices 0-15 only and dropped the rest (counted in [11]), so such a program's lights never lit anything.
 * The lights are now slots keyed by their index (lidx), TNL_LIGHTS of them a context; the first 16 slots and the fields before them
 * keep the old layout, so a recording's state record (6) from before .646 still loads (vx_d3d_tnl_set). */
#define TNL_LIGHTS 64
#define TNL_OLD_SIZE (sizeof(float)*(16*3+8*16+17+2)+16*sizeof(tnl_light))
typedef struct {float world[16],view[16],proj[16],tex[8][16],mat[17],zmin,zmax;tnl_light light[TNL_LIGHTS];uint32_t lidx[TNL_LIGHTS],nslot,full;} tnl_state;
#define TNL_CTX 16
static tnl_state tnl_tab[TNL_CTX];static const void *tnl_key[TNL_CTX];static uint32_t tnl_next;
static void tnl_ident(float *m) {uint32_t k;for(k=0;k<16;k++)m[k]=(k%5)?0.0f:1.0f;}
/* .647 (board, GTA3 on .646 with ACCEL_D3D_TNL 1 and 0 in the same pair of boots: Direct3D's T&L 5% faster per triangle; GTA3 sends
 * about 320 draws a frame of 42 triangles each, and every draw rebuilt the whole set-up: a 1.4 KB clear, two matrix products, the normal
 * matrix with nine divides, the lights in eye space with a square root each, the material products, and the group kernels' frame). The
 * set-up is kept between draws: tn_key holds what it was built from (the context's T&L state, the vertex format and stride, the
 * viewport, the render states and the first three stages' states it reads); a T&L command other than a world matrix forgets it, and a
 * world matrix only marks the matrices to be made again (the lights are in eye space: the view's, not the world's). tn_same: 0 built
 * whole, 1 the matrices made again, 2 kept as it was. vx_d3d_tn_setup counts the three (QueryPhases 390-392). */
uint32_t vx_d3d_tn_keep_on=1,vx_d3d_tn_plg; /* 0 in host tests that compare kept and rebuilt set-ups */
extern uint32_t vx_d3d_gband,vx_d3d_gbs[2],vx_d3d_vcd[3]; /* .698: ACCEL_D3D_GUARDBAND (the VxD's bit 22; 0 on the host unless a test sets it) */
static float tn_gxl,tn_gxr,tn_gyt,tn_gyb; /* .698: the guard band's edges for TN_OK (tn_prepare_full) */
/* .701: tn_gb, the guard band for this draw's set-up (tn_prepare_full): ACCEL_D3D_GUARDBAND. .701-.702: only where the records went into
 * indexed packets (VX3DN_IX_GUARD); .703: in every form (plain packets carry VX3DN_EXT_GUARD). T700, board B: GRID 6i (a
 * projected set: records of 11 words, never prepacked) went to the per-triangle path, whose packets cannot carry the band, so every
 * triangle with a corner past the viewport was clipped on the screen by the general path: 97.0 ms a frame against 88.6 without the band */
static uint32_t tn_gb;
static uint8_t tn_vc[16384+8]; /* .698: by place, the viewport's sides a vertex is beyond (tn_soa_pos; the guard band only; TN_MAX places) */
/* .698: a T&L record's x or y inside the clip (the bits' order, as before) or, with the guard band, inside its edges (as floats) */
#define TN_GX(w,f,lo,hi,gl,gh) (GB_IN(w,lo,hi)||(tn_gb&&(f)>=(gl)&&(f)<=(gh))) /* .699: inside the clip first (one unsigned compare, as before); the band's float test only outside it */
static uint32_t tn_world_dirty,tn_same,tn_soa_stale;static int tn_soa_kept_valid; /* .648: here, cleared by a full tn_prepare */uint32_t vx_d3d_tn_setup[3]; /* tn_soa_stale: the matrices made again and not yet in the kernels' frame (a draw can be prepared, refused for room, and prepared again before tn_soa_prepare runs) */
#define TN_KRS 17
static const uint16_t tn_krs[TN_KRS]={137,139,141,143,145,146,147,148,29,142,28,35,140,48,36,37,38};
static struct {const void *t;uint32_t fvf,stride,rs[TN_KRS],tss[3][3],ns;int32_t vp[4];int valid;} tn_key;
static void tnl_park_purge(const tnl_state *t);
static void tnl_fresh(tnl_state *t) {uint32_t k;tn_key.valid=0;tnl_park_purge(t);memset(t,0,sizeof(*t));tnl_ident(t->world);tnl_ident(t->view);tnl_ident(t->proj);for(k=0;k<8;k++)tnl_ident(t->tex[k]);t->zmin=0.0f;t->zmax=1.0f;}
static tnl_state *tnl_of(const vx_d3d_context *c) {
 uint32_t k;tnl_state *t;
 for(k=0;k<TNL_CTX;k++)if(tnl_key[k]==c)return &tnl_tab[k];
 for(k=0;k<TNL_CTX&&tnl_key[k];k++){}
 if(k==TNL_CTX){k=tnl_next;tnl_next=(tnl_next+1)%TNL_CTX;}
 t=&tnl_tab[k];tnl_key[k]=c;tnl_fresh(t);return t;
}
static void tnl_blk_forget(const vx_d3d_context *c);
void vx_d3d_tnl_reset(const vx_d3d_context *c) {uint32_t k;for(k=0;k<TNL_CTX;k++)if(tnl_key[k]==c)tnl_fresh(&tnl_tab[k]);tnl_blk_forget(c);} /* .663: and its state blocks' T&L commands */
/* the recorder (vx_d3d_rec.inc) keeps a context's T&L state with a recording; the replay puts it back (1: done, 0: the wrong size) */
uint32_t vx_d3d_tnl_get(const vx_d3d_context *c,void *o,uint32_t n) {if(!o)return sizeof(tnl_state);if(n<sizeof(tnl_state))return 0;memcpy(o,tnl_of(c),sizeof(tnl_state));return sizeof(tnl_state);}
uint32_t vx_d3d_tnl_set(const vx_d3d_context *c,const void *s,uint32_t n) {tnl_state *t=tnl_of(c);uint32_t k;tn_key.valid=0;
 if(n==sizeof(*t)){tnl_park_purge(t);memcpy(t,s,n);return 1;}
 if(n!=TNL_OLD_SIZE)return 0;
 tnl_fresh(t);memcpy(t,s,n);for(k=0;k<16;k++)t->lidx[k]=k;t->nslot=16;return 1;} /* .646: a recording from before .646: indices 0-15 in slots 0-15 */
/* .646: the slot of light index i (add: made when it is not there; a slot of a disabled light is taken when all are used).
 * .667 (review of .667, finding 3: the slot's old light was cleared, so a program that set more than 64 light indices over time and
 * switched an earlier one on again got a light with no definition): a disabled light whose slot is taken is parked (tnl_park, by
 * context and index) and comes back with its definition when it is used again. The parked copy is a copy: what the slot holds wins
 * while the index has a slot, and parking the same index again overwrites it (a retried command, tnl_undo, parks the same thing again).
 * Only a full park (1,024 indices put aside, every context together) loses one, the longest parked, counted in vx_d3d_tn_nd[1]. */
#define TNL_PARK 1024
static struct {const tnl_state *t;uint32_t idx,age;tnl_light l;} tnl_park[TNL_PARK];static uint32_t tnl_park_age;
static int tnl_park_find(const tnl_state *t,uint32_t i) {int k;for(k=0;k<TNL_PARK;k++)if(tnl_park[k].t==t&&tnl_park[k].idx==i)return k;return -1;}
static void tnl_park_put(const tnl_state *t,uint32_t i,const tnl_light *l) {
 int k=tnl_park_find(t,i),j;
 if(k<0){for(j=0;j<TNL_PARK&&tnl_park[j].t;j++){}k=j;
  if(k==TNL_PARK){k=0;for(j=1;j<TNL_PARK;j++)if(tnl_park[j].age<tnl_park[k].age)k=j;vx_d3d_tn_nd[1]++;}}
 tnl_park[k].t=t;tnl_park[k].idx=i;tnl_park[k].age=++tnl_park_age;tnl_park[k].l=*l;
}
static void tnl_park_purge(const tnl_state *t) {int k;for(k=0;k<TNL_PARK;k++)if(tnl_park[k].t==t)memset(&tnl_park[k],0,sizeof(tnl_park[k]));}
/* the slot light i gets (-1: none); add: make it when the index is new. tnl_light_k answers the same without changing anything. */
static int tnl_light_k(const tnl_state *t,uint32_t i,int add) {
 uint32_t k;
 for(k=0;k<t->nslot;k++)if(t->lidx[k]==i)return (int)k;
 if(!add&&tnl_park_find(t,i)<0)return -1;
 if(t->nslot<TNL_LIGHTS)return (int)t->nslot;
 for(k=0;k<TNL_LIGHTS&&t->light[k].on;k++){}
 return k==TNL_LIGHTS?-1:(int)k;
}
static tnl_light *tnl_light_slot(tnl_state *t,uint32_t i,int add) {
 int q=tnl_light_k(t,i,add),pk;uint32_t k;
 if(q<0)return 0;k=(uint32_t)q;
 if(k<t->nslot&&t->lidx[k]==i)return &t->light[k];
 pk=tnl_park_find(t,i);
 if(pk>=0){
  /* Exchange the definitions in place. Parking first could evict pk itself. */
  tnl_light restored=tnl_park[pk].l;
  if(k<t->nslot){
   tnl_park[pk].idx=t->lidx[k];tnl_park[pk].l=t->light[k];
   tnl_park[pk].age=++tnl_park_age;t->full++;
  }else{tnl_park[pk].t=0;t->nslot++;}
  t->light[k]=restored;
 }else{
  if(k<t->nslot){tnl_park_put(t,t->lidx[k],&t->light[k]);t->full++;}
  else t->nslot++;
  memset(&t->light[k],0,sizeof(tnl_light));
 }
 t->lidx[k]=i;return &t->light[k];
}
/* .543: reads and writes through the core's unaligned types and vx_fu (Watcom compiled each 4-byte memcpy as REP MOVSD and REP MOVSB with
 * their set-up: the GRID scene's tn_pos read x, y and z that way) */
static float tf(const uint8_t *p) {return *(const vx_f32a*)p;}
static uint32_t tn_bits(float x) {vx_fu u;u.f=x;return u.n;}
static float tn_flt(uint32_t n) {vx_fu u;u.n=n;return u.f;}
/* .663 (audit 03: a busy send retried from the first packet not taken drew it with the material set after it): what a T&L command
 * overwrites while packets of the call are queued or batched is kept first (tnl_undo, a stack in command order), and a failed call puts
 * back what the commands after its error offset changed: the runtime's retry runs them again from there. The core makes room (it sends the
 * queue when the log cannot take a command); vx_d3d_tnl_undo_stat: [0] entries put back, [1] sends for a full log */
#define TNL_UNDO 64
typedef struct {uint32_t pos,off,len;tnl_state *t;uint8_t d[sizeof(tnl_light)];} tnl_undo_e;
static tnl_undo_e tnl_undo[TNL_UNDO];static uint32_t tnl_undo_n,tnl_undo_on,tnl_undo_pos;
uint32_t vx_d3d_tnl_undo_stat[2];
static void tnl_log(tnl_state *t,const void *p,uint32_t len) {
 tnl_undo_e *e;if(!tnl_undo_on||tnl_undo_n>=TNL_UNDO||len>sizeof(e->d))return;
 e=&tnl_undo[tnl_undo_n++];e->pos=tnl_undo_pos;e->t=t;e->off=(uint32_t)((const uint8_t*)p-(const uint8_t*)t);e->len=len;memcpy(e->d,p,len);
}
static uint32_t tnl_need(uint32_t op,uint32_t n) {return op==34?n*4:op==36?n:(op==33||op==32)?1:0;}
static void tnl_undo_to(uint32_t pos) {
 while(tnl_undo_n&&tnl_undo[tnl_undo_n-1].pos>pos){tnl_undo_e *e=&tnl_undo[--tnl_undo_n];memcpy((uint8_t*)e->t+e->off,e->d,e->len);vx_d3d_tnl_undo_stat[0]++;tn_key.valid=0;}
 tnl_undo_n=0;
}
/* .663 (audit 16, 17): a state block's T&L commands (material, transforms, lights, depth range) recorded, applied when the block is applied
 * and filled with the values in force when it is captured. Before .663 they were read past while recording.
 * .667 (review of .667, findings 1 and 2: .663-.667 kept the commands as they came in a 1,024-byte log, eight logs for every context
 * together: a block past 1,024 bytes (one SETTRANSFORM of 15 matrices) lost the commands after it, and a ninth block begun anywhere
 * took the oldest log, so applying that block left its T&L state as it was): a block now keeps its final state, as the runtime's own
 * state blocks do: each matrix (world, view, projection, the eight texture matrices) with a bit saying it was set, the material, the
 * depth range, and each light index it touched (its definition and/or its enable) in a list from a pool. Setting a thing twice keeps
 * the last value, so a block's size is bounded by what it touches, not by how many commands set it. The table holds TNL_BLKS blocks
 * keyed by context and handle (16 contexts of the HAL's 8 blocks each: a context never has more than VX_D3D_BLOCKS live, so none is
 * given up while it is live); vx_d3d_tnl_blk_over counts what could still not be kept (the table or the light pool full). */
#define TNL_BLKS (TNL_CTX*VX_D3D_BLOCKS)
#define TNL_BLK_POOL 1024
#define TNL_BLK_NONE 0xFFFFFFFFUL
typedef struct {const void *c;uint32_t handle,mmask,has_mat,has_z,first;float m[11][16],mat[17],z[2];} tnl_blkst;
typedef struct {uint32_t next,idx,flags;uint8_t d[104];} tnl_blkl; /* flags: 1 the enable set, 2 its value (on), 4 the definition set (d) */
static tnl_blkst tnl_blks[TNL_BLKS];static tnl_blkl tnl_blkp[TNL_BLK_POOL];static uint32_t tnl_blkp_free=TNL_BLK_NONE,tnl_blkp_init;
uint32_t vx_d3d_tnl_blk_over;
static void tnl_blkp_setup(void) {uint32_t k;if(tnl_blkp_init)return;for(k=0;k<TNL_BLK_POOL;k++)tnl_blkp[k].next=k+1<TNL_BLK_POOL?k+1:TNL_BLK_NONE;tnl_blkp_free=0;tnl_blkp_init=1;}
static void tnl_blk_clear(tnl_blkst *b) { /* its lights back to the pool, the block empty */
 uint32_t k=b->c?b->first:TNL_BLK_NONE; /* a slot never used holds first 0, not a list */
 while(k!=TNL_BLK_NONE&&k<TNL_BLK_POOL){uint32_t nx=tnl_blkp[k].next;tnl_blkp[k].next=tnl_blkp_free;tnl_blkp_free=k;k=nx;}
 memset(b,0,sizeof(*b));b->first=TNL_BLK_NONE;
}
static tnl_blkst *tnl_blk_find(const vx_d3d_context *c,uint32_t h) {int k;for(k=0;k<TNL_BLKS;k++)if(tnl_blks[k].c==c&&tnl_blks[k].handle==h&&h)return &tnl_blks[k];return 0;}
static void tnl_blk_begin(const vx_d3d_context *c,uint32_t h) {
 tnl_blkst *b=tnl_blk_find(c,h);int k;
 tnl_blkp_setup();
 if(!b)for(k=0;k<TNL_BLKS;k++)if(!tnl_blks[k].c){b=&tnl_blks[k];break;}
 if(!b){vx_d3d_tnl_blk_over++;return;} /* more than 16 contexts' blocks: this one keeps no T&L state (counted) */
 tnl_blk_clear(b);b->c=c;b->handle=h;
}
static void tnl_blk_forget(const vx_d3d_context *c) {int k;tnl_blkp_setup();for(k=0;k<TNL_BLKS;k++)if(tnl_blks[k].c==c)tnl_blk_clear(&tnl_blks[k]);}
static void tnl_blk_drop(const vx_d3d_context *c,uint32_t h) {tnl_blkst *b=tnl_blk_find(c,h);if(b)tnl_blk_clear(b);}
static int tnl_blk_mi(uint32_t ty) {return ty==1?0:ty==2?1:ty==3?2:ty>=16&&ty<24?(int)(ty-13):-1;} /* the matrix's place in m */
static tnl_blkl *tnl_blk_light(tnl_blkst *b,uint32_t idx) {
 uint32_t k;
 for(k=b->first;k!=TNL_BLK_NONE&&k<TNL_BLK_POOL;k=tnl_blkp[k].next)if(tnl_blkp[k].idx==idx)return &tnl_blkp[k];
 if(tnl_blkp_free==TNL_BLK_NONE){vx_d3d_tnl_blk_over++;return 0;} /* the pool full (1,024 light indices in live blocks): this one not kept, counted */
 k=tnl_blkp_free;tnl_blkp_free=tnl_blkp[k].next;memset(&tnl_blkp[k],0,sizeof(tnl_blkp[k]));tnl_blkp[k].idx=idx;tnl_blkp[k].next=b->first;b->first=k;return &tnl_blkp[k];
}
static void tnl_blk_add(const vx_d3d_context *c,uint32_t h,uint32_t op,const uint8_t *p,uint32_t n,uint32_t len) {
 tnl_blkst *b=tnl_blk_find(c,h);uint32_t j,k;(void)len;
 if(!b||op==35||op==44)return; /* CREATELIGHT and SETCLIPPLANE change nothing here */
 if(op==36)for(j=0;j<n;j++){int mi=tnl_blk_mi(u32(p+j*68));if(mi>=0){b->mmask|=1UL<<mi;memcpy(b->m[mi],p+j*68+4,64);}}
 else if(op==33&&n){memcpy(b->mat,p+(n-1)*68,68);b->has_mat=1;}
 else if(op==32&&n){memcpy(b->z,p+(n-1)*8,8);b->has_z=1;}
 else if(op==34)for(j=0,k=0;j<n;j++){uint32_t i=u32(p+k),ty=u32(p+k+4);tnl_blkl *l=tnl_blk_light(b,i);k+=8;
  if(ty==0||ty==1){if(l)l->flags=(l->flags&~2UL)|1UL|(ty==0?2UL:0UL);}
  else if(ty==2){if(l){memcpy(l->d,p+k,104);l->flags|=4;}k+=104;}}
}
static void tnl_command(vx_d3d_context *c,uint32_t op,const uint8_t *p,uint32_t n);
static void tnl_blk_apply(vx_d3d_context *c,uint32_t h) { /* as the commands that set the same final state: matrices, material, depth range, each light's definition then its enable */
 tnl_blkst *b=tnl_blk_find(c,h);uint8_t buf[112];uint32_t k;int mi;
 static const uint32_t ty[11]={1,2,3,16,17,18,19,20,21,22,23};
 if(!b)return;
 for(mi=0;mi<11;mi++)if(b->mmask&(1UL<<mi)){*(vx_u32a*)buf=ty[mi];memcpy(buf+4,b->m[mi],64);tnl_command(c,36,buf,1);}
 if(b->has_mat){memcpy(buf,b->mat,68);tnl_command(c,33,buf,1);}
 if(b->has_z){memcpy(buf,b->z,8);tnl_command(c,32,buf,1);}
 for(k=b->first;k!=TNL_BLK_NONE&&k<TNL_BLK_POOL;k=tnl_blkp[k].next){const tnl_blkl *l=&tnl_blkp[k];
  if(l->flags&4){*(vx_u32a*)buf=l->idx;*(vx_u32a*)(buf+4)=2;memcpy(buf+8,l->d,104);tnl_command(c,34,buf,1);}
  if(l->flags&1){*(vx_u32a*)buf=l->idx;*(vx_u32a*)(buf+4)=(l->flags&2)?0UL:1UL;tnl_command(c,34,buf,1);}}
}
/* Capture reads a light without taking a working slot away from another light. */
static const tnl_light *tnl_light_read(const tnl_state *t,uint32_t idx) {
 uint32_t k;int parked;
 for(k=0;k<t->nslot;k++)if(t->lidx[k]==idx)return &t->light[k];
 parked=tnl_park_find(t,idx);
 return parked>=0?&tnl_park[parked].l:0;
}
static void tnl_blk_capture(vx_d3d_context *c,uint32_t h) { /* the recorded fields take the values now in force */
 tnl_blkst *b=tnl_blk_find(c,h);tnl_state *t=tnl_of(c);uint32_t k;int mi;
 if(!b)return;
 for(mi=0;mi<11;mi++)if(b->mmask&(1UL<<mi)){const float *m=mi==0?t->world:mi==1?t->view:mi==2?t->proj:t->tex[mi-3];memcpy(b->m[mi],m,64);}
 if(b->has_mat)memcpy(b->mat,t->mat,68);
 if(b->has_z){b->z[0]=t->zmin;b->z[1]=t->zmax;}
 for(k=b->first;k!=TNL_BLK_NONE&&k<TNL_BLK_POOL;k=tnl_blkp[k].next){tnl_blkl *e=&tnl_blkp[k];const tnl_light *l=tnl_light_read(t,e->idx);
  if(e->flags&1)e->flags=(e->flags&~2UL)|(l&&l->on?2UL:0UL);
  if((e->flags&4)&&l&&l->have){uint8_t *d=e->d;uint32_t r;*(vx_u32a*)d=l->type;for(r=0;r<4;r++){*(vx_f32a*)(d+4+r*4)=l->dif[r];*(vx_f32a*)(d+20+r*4)=l->spe[r];*(vx_f32a*)(d+36+r*4)=l->amb[r];}
   for(r=0;r<3;r++){*(vx_f32a*)(d+52+r*4)=l->pos[r];*(vx_f32a*)(d+64+r*4)=l->dir[r];}
   *(vx_f32a*)(d+76)=l->range;*(vx_f32a*)(d+80)=l->falloff;*(vx_f32a*)(d+84)=l->a0;*(vx_f32a*)(d+88)=l->a1;*(vx_f32a*)(d+92)=l->a2;*(vx_f32a*)(d+96)=l->theta;*(vx_f32a*)(d+100)=l->phi;}}
}
/* a T&L command into the state (the lengths were checked by the caller) */
static void tnl_command(vx_d3d_context *c,uint32_t op,const uint8_t *p,uint32_t n) {
 tnl_state *t=tnl_of(c);uint32_t j,k;
 {int world=op==36&&n>0;for(j=0;world&&j<n;j++)if(u32(p+j*68)!=1)world=0; /* .647: a world matrix alone keeps the set-up but its matrices */
  if(world)tn_world_dirty=1;else tn_key.valid=0;}
 if(op==36)for(j=0;j<n;j++){uint32_t ty=u32(p+j*68);float *m=ty==1?t->world:ty==2?t->view:ty==3?t->proj:ty>=16&&ty<24?t->tex[ty-16]:0;
  if(m){tnl_log(t,m,64);for(k=0;k<16;k++)m[k]=tf(p+j*68+4+k*4);}}
 else if(op==33&&n){tnl_log(t,t->mat,sizeof(t->mat));for(k=0;k<17;k++)t->mat[k]=tf(p+(n-1)*68+k*4);}
 else if(op==34)for(j=0,k=0;j<n;j++){uint32_t i=u32(p+k),ty=u32(p+k+4);tnl_light *l;k+=8;
  if(tnl_undo_on){int q=tnl_light_k(t,i,ty==0||ty==2);tnl_log(t,&t->nslot,4);tnl_log(t,&t->full,4);if(q>=0){tnl_log(t,&t->lidx[q],4);tnl_log(t,&t->light[q],sizeof(tnl_light));}}
  l=tnl_light_slot(t,i,ty==0||ty==2); /* .646: any index (a disable of a light never set needs no slot) */
  if(!l){if(ty==2)k+=104;if(ty!=1){vx_d3d_tn[11]++;vx_d3d_tn_nd[0]++;}continue;}
  if(ty==0)l->on=1;else if(ty==1)l->on=0;
  else if(ty==2){uint32_t q;l->type=u32(p+k);for(q=0;q<4;q++){l->dif[q]=tf(p+k+4+q*4);l->spe[q]=tf(p+k+20+q*4);l->amb[q]=tf(p+k+36+q*4);}
   for(q=0;q<3;q++){l->pos[q]=tf(p+k+52+q*4);l->dir[q]=tf(p+k+64+q*4);}
   l->range=tf(p+k+76);l->falloff=tf(p+k+80);l->a0=tf(p+k+84);l->a1=tf(p+k+88);l->a2=tf(p+k+92);l->theta=tf(p+k+96);l->phi=tf(p+k+100);l->have=1;k+=104;}}
 else if(op==32&&n){tnl_log(t,&t->zmin,8);t->zmin=tf(p+(n-1)*8);t->zmax=tf(p+(n-1)*8+4);}
}
/* the square root without the C library (the host builds link none): a seed from the float's bits, three Newton steps (1e-7) */
static float tn_sqrt(float x) {float y;uint32_t i;if(!(x>0.0f))return 0.0f;i=0x1FBD1DF5UL+(tn_bits(x)>>1);y=tn_flt(i);y=0.5f*(y+x/y);y=0.5f*(y+x/y);y=0.5f*(y+x/y);return y;}
static void mmul(float *o,const float *a,const float *b) {uint32_t i,j;for(i=0;i<4;i++)for(j=0;j<4;j++)o[i*4+j]=a[i*4]*b[j]+a[i*4+1]*b[4+j]+a[i*4+2]*b[8+j]+a[i*4+3]*b[12+j];}
/* e^x for x <= 0 (the fog), within 1e-6: 2^(x log2 e) by its integer part and a polynomial for the rest */
/* .540 (review): a NaN failed both tests and reached the conversion to int (the x87 gives 0x80000000: a loop of 2^31 halvings, a hang):
 * only -80 < x < 0 goes on, so i is -116..-1 */
static float tn_exp(float x) {
 double y,f,r;int i;
 if(!(x>-80.0f))return 0.0f;if(!(x<0.0f))return 1.0f;
 y=(double)x*1.4426950408889634;i=(int)y;if((double)i>y)i--;f=y-i; /* 0 <= f < 1 */
 r=1.0+f*(0.6931471805599453+f*(0.2402265069591007+f*(0.0555041086648216+f*(0.0096181291076285+f*(0.0013333558146428+f*0.0001540353039338)))));
 /* .552: i is -116..-1. 2^i is an exactly representable normal float;
  * scaling this double by it is the same exact binary scaling as the old
  * loop of up to 116 halvings. Reuse the grouped fog kernel's exponent
  * construction without changing the polynomial or its rounding. */
 return (float)(r*(double)tn_flt((uint32_t)(i+127)<<23));
}
/* .645: cos(a/2) for a spot light's cone angles (0 to pi: the half angle is at most pi/2), a series in double without the C library */
static float tn_cos_half(float a) {
 double x=(double)a*0.5,x2,r;
 if(!(x>0.0))return 1.0f;if(x>1.5707963267948966)x=1.5707963267948966;
 x2=x*x;r=1.0-x2*(1.0/2-x2*(1.0/24-x2*(1.0/720-x2*(1.0/40320-x2*(1.0/3628800-x2*(1.0/479001600))))));
 return (float)r;
}
/* .645: ln x for a finite x > 0 (a float's exponent, then 2 atanh((m - 1) / (m + 1)) for the mantissa in [0.707, 1.414]: 1e-10) */
static double tn_log(float x) {
 uint32_t b=tn_bits(x);int e=(int)((b>>23)&255)-127;double m,z,z2;
 if(e<=-127)return -88.0; /* zero and denormals: a power of them is 0 */
 m=(double)tn_flt((b&0x7FFFFFUL)|0x3F800000UL);if(m>1.4142135623730951){m*=0.5;e++;}
 z=(m-1.0)/(m+1.0);z2=z*z;
 return (double)e*0.6931471805599453+2.0*z*(1.0+z2*(1.0/3+z2*(1.0/5+z2*(1.0/7+z2*(1.0/9+z2*(1.0/11))))));
}
/* .645: x^p for the specular power and a spot light's falloff (x > 0 only: the callers skip the rest; p >= 0; x >= 1 gives 1) */
static float tn_pow(float x,float p) {
 double y;
 if(!(x>0.0f))return p==0.0f?1.0f:0.0f;
 if(p==1.0f)return x;
 if(!(p>0.0f)||x>=1.0f)return 1.0f; /* a power of 0 is 1; no light's term is above 1 */
 /* .661 (board, VXTLP 6k on .660: vertices finished one at a time 8% of the T&L, a log and an exp in double for each light): a whole power
  * 2-255 by squaring in float, the group kernels' steps in their order (tnk_pow mode 3), so both give the same bits */
 if(p>=2.0f&&p<=255.0f&&(float)(int)p==p){uint32_t n=(uint32_t)(int)p;float b=x,r;while(!(n&1)){b*=b;n>>=1;}r=b;n>>=1;while(n){b*=b;r*=(n&1)?b:1.0f;n>>=1;}return r;}
 y=(double)p*tn_log(x);
 return tn_exp((float)y);
}
/* the scratch buffer: TN_MAX pretransformed vertices (at most 40 bytes), their clip-space positions and clip codes */
#ifndef TN_MAX /* .718: a host test builds a small scratch (tests/run.py: the deferred side planes taken on every clipped scene) */
#define TN_MAX 16384 /* the HAL image stays under the VxD lock's 2048 pages (ACCEL_LOCK_HAL; 1028 at .663) */
#endif
#define TN_TRIS 8192 /* triangles in; .540: out at most 3 a triangle in (two planes cut it to a pentagon): tn_tok holds TN_OUT (.539 held
 * TN_TRIS and dropped the rest of a draw whose clipping made more) */
#define TN_OUT (TN_TRIS*3)
static uint8_t tn_buf[(TN_MAX+8)*44],tn_cc[TN_MAX+8];static uint16_t tn_tok[3+TN_OUT*3]; /* .544: eight places of slack: the group kernels write whole groups of four */

/* .707 (a review of .703: three byte-at-a-time passes over a draw's places after the SIMD kernels): the clip codes and viewport codes
 * read four places a word. tn_w4 reads four bytes (any alignment: the x86 allows it); tn_hasb, some byte of w equals b (the exact
 * zero-byte test on w ^ b x 0x01010101: it can mark the wrong byte, so a hit is checked byte by byte); tn_nz4, how many of w's bytes
 * are not 0; tn_orand, the OR and the AND of n bytes from p. */
#ifdef __WATCOMC__
static uint32_t tn_w4(const uint8_t *p) {return *(const uint32_t*)(const void*)p;} /* the board: one load (Watcom made the 4-byte memcpy a rep movsb) */
#else
static uint32_t tn_w4(const uint8_t *p) {uint32_t w;memcpy(&w,p,4);return w;}
#endif
static int tn_hasb(uint32_t w,uint32_t b4) {w^=b4;return ((w-0x01010101UL)&~w&0x80808080UL)!=0;}
static uint32_t tn_nz4(uint32_t w) {uint32_t x=(((w&0x7F7F7F7FUL)+0x7F7F7F7FUL)|w)&0x80808080UL;return (uint32_t)((x>>7)*0x01010101UL)>>24;} /* the product held to 32 bits (unsigned long is 64 on the host) */
static void tn_orand(const uint8_t *p,uint32_t n,uint32_t *por,uint32_t *pand) {
 uint32_t o=0,a=0xFFFFFFFFUL,k=0,w;
 for(;k+4<=n;k+=4){w=tn_w4(p+k);o|=w;a&=w;}
 o|=o>>16;o|=o>>8;a&=a>>16;a&=a>>8;o&=0xFF;a&=0xFF;
 for(;k<n;k++){o|=p[k];a&=p[k];}
 *por=o;*pand=a;
}
/* .540: tn_ref, the vertices a draw's triangles use (stamped with tn_epoch: only those are transformed, not every vertex between the
 * lowest and highest index); tn_uvq, a projected set's u, v and q before the divide (clipping interpolates them, then divides) */
static uint32_t tn_ref[TN_MAX],tn_epoch;static uint16_t tn_map[TN_MAX];static float tn_uvq[TN_MAX][2][3];static uint32_t tn_proj[2];
static uint32_t tn_P,tn_ostride,tn_sets,tn_fvf,tn_stage_set[8]; /* .646: each stage's output set */
/* .656: tn_qs, the sets whose records carry q (u, v, q undivided: the HAL divides per pixel); tn_toff, each set's first word in a record */
static uint32_t tn_qs,tn_r3,tn_toff[2]; /* .663 (audit 06): tn_r3, a set whose records carry its third coordinate (r: a cube map's direction, a
 * volume's depth), not divided (the stage keeps TTF 0) */
/* the call's own: the untransformed input (bound once) and the frame of the per-draw set-up */
typedef struct {float wvp[16],wv[16],nrm[9],amb[4],mdif[4],mamb[4],memi[4],ld[16][3],lpos[16][3],fs,fe,fd;uint32_t lit,fog,fmode,rfog,nl,li[16],ldir[16];
 uint32_t gen[2],idx[2],ttf[2],ins,inormal,idif,ispe,itex[8],itn[8],dsrc,asrc,esrc,mat_sources,nnorm;
 /* .542: nrm16, the normal matrix as tn_x4's 4x4 (rows padded, no translation); mconst: no material colour from the
  * vertex, so base4 (emissive + global ambient x ambient + every directional light's ambient x ambient, alpha the diffuse's) and per
  * light ld4 (diffuse x diffuse) and la4 (ambient x ambient) are the draw's: a lit vertex is base4 + the lights' terms (b g r a order) */
 uint32_t mconst,pointlight,gen_normal,gen_eye,fog_clip_w,lit_valid,lit_key[5],lit_color,nrm_need;float nrm16[16],base4[4],ld4[16][4],la4[16][4];
 /* .645 (review of .644): spot lights (the cone: a direction in eye space, cos(theta/2), cos(phi/2), the falloff) and specular lighting
  * (SPECULARENABLE with a material or vertex specular colour: the material's, its power, local viewer). Before, a spot light was a point
  * light and the specular highlight was left out, while the HAL's caps (POSITIONALLIGHTS, LOCALVIEWER) say the driver does both. */
 uint32_t spec_on,ssrc,localview,spotmask;float mspec[4],mpow,spcut; /* .697: spcut, below */ /* spotmask: bit k set, light k of the draw is a spot light (its cone in tn_spot, below) */
 /* .542: the viewport and depth range as scale and offset (tn_pos: x = vx0 + cx rhw vxs, y = vy0 + cy rhw vys, z = z0 + cz rhw zs) */
 float vx0,vxs,vy0,vys,z0,zs;
 tnl_state *ts; /* .543: the context's T&L state, found once a draw (tn_fin_to looked it up in the table for every vertex) */
} tn_setup;
static float tn_sdir[16][3],tn_sct[16],tn_scp[16],tn_sfall[16]; /* .645: the cones of the draw's spot lights (valid where spotmask has the bit; not cleared a draw) */
static int tn_inf(float x) {return (tn_bits(x)&0x7F800000UL)==0x7F800000UL;} /* .538: NaN or infinity */
/* .644 (board, .643: the grid scene 37.3 to 45.9 ms a frame, tn_col 3% of GTA3's frame): tn_col divided four times a colour, and .643
 * called it for every vertex whose colour is a material source; a divide is about 40 cycles on the C7. The 256 quotients are made once
 * (the same divide, so the same bits) and read from a table. */
static float tn_c255[256];static int tn_c255_ok;
static void tn_col(uint32_t c,float *o) {
 if(!tn_c255_ok){uint32_t k;for(k=0;k<256;k++){volatile float r=1.0f/255.0f;tn_c255[k]=(float)k*r;} /* .694: k x (1/255), as tnk_cv's multiply (one rounding: the product of an 8-bit integer and a float is exact before it) */tn_c255_ok=1;}
 o[0]=tn_c255[(c>>16)&255];o[1]=tn_c255[(c>>8)&255];o[2]=tn_c255[c&255];o[3]=tn_c255[c>>24];}
/* .542: SSE kernels; .543: the driver's T&L is SSE only. The HAL offers hardware T&L (ACCEL_D3D_TNL) only when the processor has SSE,
 * SSE2 and FXSR and the system saves the XMM registers (CR4 OSFXSR, the VxD's control 0xFFFFFE34, as VXGL.DLL checks): otherwise the
 * runtime's own T&L, as with the key off (the x87 copies of these steps removed). Three kernels, the same operations in the same order on
 * the board (Watcom #pragma aux) and on the host (SSE2 intrinsics), so the host's tests check the board's arithmetic:
 *  tn_x4  o[0..3] = v0 m[0..3] + v1 m[4..7] + v2 m[8..11] + m[12..15] (a position by a row-vector matrix: clip space, eye space, the
 *         normal by the padded normal matrix); every load and store unaligned (movups);
 *  tn_ma4 a[0..3] += s v[0..3];
 *  tn_pk4 four channels times k (255 for 0..1, 1 for 0..255), rounded (cvtps2dq, nearest), saturated to bytes (packssdw, packuswb: a
 *         NaN gives 0x80000000, so 0), lane 0 the lowest byte: b g r a in gives 0xAARRGGBB. */
static const float tn_255[4]={255.0f,255.0f,255.0f,255.0f},tn_one[4]={1.0f,1.0f,1.0f,1.0f};
#ifdef __WATCOMC__
/* the kernels as bytes (the HAL is built for the 486/x87 instruction set, where Watcom's assembler refuses SSE mnemonics): assembled
 * from movss/shufps/movups/mulps/addps (tn_x4: xmm0-3, tn_ma4: xmm0-2) and movups/mulps/cvtps2dq/packssdw/packuswb/movd (tn_pk4) by
 * Watcom with -6, the same registers as the parm lists */
static void tn_x4(const float *m,const float *v,float *o);
#pragma aux tn_x4 = 0xF3 0x0F 0x10 0x02 0x0F 0xC6 0xC0 0x00 0xF3 0x0F 0x10 0x4A 0x04 0x0F 0xC6 0xC9 0x00 0xF3 0x0F 0x10 0x52 0x08 0x0F 0xC6 0xD2 0x00 0x0F 0x10 0x18 0x0F 0x59 0xC3 0x0F 0x10 0x58 0x10 0x0F 0x59 0xCB 0x0F 0x58 0xC1 0x0F 0x10 0x58 0x20 0x0F 0x59 0xD3 0x0F 0x58 0xC2 0x0F 0x10 0x58 0x30 0x0F 0x58 0xC3 0x0F 0x11 0x01 parm [eax] [edx] [ecx] modify exact [];
static void tn_ma4(float *a,const float *s,const float *v);
#pragma aux tn_ma4 = 0xF3 0x0F 0x10 0x0A 0x0F 0xC6 0xC9 0x00 0x0F 0x10 0x01 0x0F 0x59 0xC1 0x0F 0x10 0x10 0x0F 0x58 0xD0 0x0F 0x11 0x10 parm [eax] [edx] [ecx] modify exact [];
static uint32_t tn_pk4(const float *bgra,const float *k255);
#pragma aux tn_pk4 = 0x0F 0x10 0x00 0x0F 0x10 0x0A 0x0F 0x59 0xC1 0x66 0x0F 0x5B 0xC0 0x66 0x0F 0x6B 0xC0 0x66 0x0F 0x67 0xC0 0x66 0x0F 0x7E 0xC0 parm [eax] [edx] value [eax] modify exact [eax];
static void tn_pf(uint32_t at);
#pragma aux tn_pf = 0x0F 0x18 0x08 parm [eax] modify exact []; /* .681: prefetcht0 [eax] (a hint: never faults, changes no register) */
#else
#include <emmintrin.h>
static void tn_x4(const float *m,const float *v,float *o) {__m128 a=_mm_set1_ps(v[0]),b=_mm_set1_ps(v[1]),d=_mm_set1_ps(v[2]);
 a=_mm_mul_ps(a,_mm_loadu_ps(m));b=_mm_mul_ps(b,_mm_loadu_ps(m+4));a=_mm_add_ps(a,b);d=_mm_mul_ps(d,_mm_loadu_ps(m+8));a=_mm_add_ps(a,d);
 a=_mm_add_ps(a,_mm_loadu_ps(m+12));_mm_storeu_ps(o,a);}
static void tn_ma4(float *a,const float *s,const float *v) {_mm_storeu_ps(a,_mm_add_ps(_mm_loadu_ps(a),_mm_mul_ps(_mm_loadu_ps(v),_mm_set1_ps(*s))));}
static uint32_t tn_pk4(const float *bgra,const float *k255) {__m128i i=_mm_cvtps_epi32(_mm_mul_ps(_mm_loadu_ps(bgra),_mm_loadu_ps(k255)));
 i=_mm_packs_epi32(i,i);i=_mm_packus_epi16(i,i);return (uint32_t)_mm_cvtsi128_si32(i);}
static void tn_pf(uintptr_t at) {_mm_prefetch((const char*)at,_MM_HINT_T0);}
#endif
/* the draw's set-up: the matrices, the lights in eye space, the input FVF's layout, the stages' coordinate sets */
/* .647: the parts of the set-up made from the world matrix (tn_prepare; again on their own when only it changed) */
static void tn_nrm(tn_setup *s);
static void tn_mats(const tnl_state *t,tn_setup *s) {
 mmul(s->wv,t->world,t->view);mmul(s->wvp,s->wv,t->proj);
 /* .698 (ACCEL_D3D_GUARDBAND, vx_d3d_gband): the clip space's x and y halved (exact: a power of two) and the viewport's scales doubled
  * (tn_prepare_full): the positions are the same, but the side planes x = +-w, y = +-w now lie at twice the viewport, so the codes, the
  * per-triangle path and the clipper (tn_lerp holds a new vertex inside vx0 +- vxs: now the guard band) clip the sides to the guard band,
  * and the engine clips the rest in screen space (VXCLIP v1.1). Near and far are unchanged (z and w are not scaled) */
 if(tn_gb){uint32_t k;for(k=0;k<16;k+=4){s->wvp[k]*=0.5f;s->wvp[k+1]*=0.5f;}} /* .701: tn_gb (tn_prepare_full decides it after these, then halves them itself) */
 if(s->nrm_need)tn_nrm(s); /* .678: only when a normal is read (set by tn_prepare_full; the kept path keeps it) */
}
/* .678: the normal matrix (9 cofactors, 9 divides, nrm16) was made at every world change, unlit draws included (GTA3's 2D and
 * unlit geometry): it is made only when a normal is read, lighting or a texture set generated from the camera-space normal, with a
 * normal in the vertex (tn_normal's callers all test inormal). The same operations as before when made. */
static void tn_nrm(tn_setup *s) {
 float a[16],det;uint32_t k;
 /* the normal matrix: the inverse transpose of wv's 3x3 (a, cofactors) */
 a[0]=s->wv[5]*s->wv[10]-s->wv[6]*s->wv[9];a[1]=s->wv[6]*s->wv[8]-s->wv[4]*s->wv[10];a[2]=s->wv[4]*s->wv[9]-s->wv[5]*s->wv[8];
 a[3]=s->wv[2]*s->wv[9]-s->wv[1]*s->wv[10];a[4]=s->wv[0]*s->wv[10]-s->wv[2]*s->wv[8];a[5]=s->wv[1]*s->wv[8]-s->wv[0]*s->wv[9];
 a[6]=s->wv[1]*s->wv[6]-s->wv[2]*s->wv[5];a[7]=s->wv[2]*s->wv[4]-s->wv[0]*s->wv[6];a[8]=s->wv[0]*s->wv[5]-s->wv[1]*s->wv[4];
 det=s->wv[0]*a[0]+s->wv[1]*a[1]+s->wv[2]*a[2];if(det==0.0f)det=1.0f;
 for(k=0;k<9;k++)s->nrm[k]=a[k]/det; /* nrm[i*3+j] = cofactor C(i,j) / det: the inverse transpose. n'(j) = sum over i of n(i) nrm[i*3+j]
  * (row vectors, as positions); .540 (review): .536 summed along the rows (n x the inverse: a rotation turned the normals the wrong way) */
 for(k=0;k<16;k++)s->nrm16[k]=0.0f;for(k=0;k<3;k++){s->nrm16[k*4]=s->nrm[k*3];s->nrm16[k*4+1]=s->nrm[k*3+1];s->nrm16[k*4+2]=s->nrm[k*3+2];}
}
/* A standard perspective projection can leave clip W equal to eye Z. Check the matrix columns bit for bit, including signs of zero,
 * before using the existing transform for depth fog. */
static void tn_fogw(tn_setup *s) {
 uint32_t k;s->fog_clip_w=0;
 if(s->fog&&!s->rfog&&!s->pointlight&&!s->gen_eye&&!s->spec_on){
  s->fog_clip_w=1;
  for(k=0;k<16;k+=4)if(tn_bits(s->wvp[k+3])!=tn_bits(s->wv[k+2]))s->fog_clip_w=0;
 }
}
/* .651 (board, VXTLP 6e on .650: the driver's T&L frame 80 ms against the runtime's 62, the HAL's "primitives into packets" 56 ms a frame
 * against 13 with the runtime's vertices): the T&L's time by part while VXD3DMON profiles (vx_d3d_phclock set), exclusive, in TSC cycles
 * (QueryPhases 395-401 in units of 256): 0 set-up, 1 positions and clip codes (phase T, with the gather), 2 groups of four (phase L),
 * 3 the rest of tn_draw (the pass over the triangles, the scratch buffer), 4 clipping, 5 vertices finished one at a time, 6 the packets
 * (tn_ix: the indices and the vertex words into the indexed packet). .652 (a review of .651): part 1 is tnk_pos alone; the gather of the
 * positions from the vertex buffer and the clip codes' reduction before it are in part 3. Part 2 is phase L's gather and records, part 7
 * its kernels (tnk_cv, tnk_plight, tnk_light, tnk_spec). */
uint32_t vx_d3d_tnp[8];static uint32_t tnp_cur=8,tnp_t0,tnp_frac[8];
static uint32_t tnp_set(uint32_t p) {uint32_t o=tnp_cur;
 if(vx_d3d_phclock){uint32_t t=vx_d3d_phclock();if(o<8){uint32_t d=t-tnp_t0+tnp_frac[o];vx_d3d_tnp[o]+=d>>8;tnp_frac[o]=d&255;}tnp_t0=t;}
 tnp_cur=p;return o;}
#define TNP(p) (vx_d3d_phclock?tnp_set(p):tnp_cur)
#define TNP_BACK(o) do{if(vx_d3d_phclock)(void)tnp_set(o);else tnp_cur=(o);}while(0)
static int tn_prepare_full(vx_d3d_context *c,const vx_d3d_input *src,tn_setup *s);
static int tn_prepare_b(vx_d3d_context *c,const vx_d3d_input *src,tn_setup *s);
static int tn_prepare(vx_d3d_context *c,const vx_d3d_input *src,tn_setup *s) {uint32_t o=TNP(0);int r=tn_prepare_b(c,src,s);TNP_BACK(o);return r;}
static int tn_prepare_b(vx_d3d_context *c,const vx_d3d_input *src,tn_setup *s) {
 tnl_state *t=tnl_of(c);const vx_d3d_state *st=&c->state;uint32_t k,n0;int eq;
 int32_t vp[4];
 vp[0]=st->viewport.left;vp[1]=st->viewport.top;vp[2]=st->viewport.right;vp[3]=st->viewport.bottom;
 eq=vx_d3d_tn_keep_on&&tn_key.valid&&tn_key.t==(const void*)t&&tn_key.fvf==src->fvf&&tn_key.stride==src->vertex_stride;
 for(k=0;eq&&k<4;k++)eq=tn_key.vp[k]==vp[k];
 for(k=0;eq&&k<TN_KRS;k++)eq=tn_key.rs[k]==st->rs[tn_krs[k]];
 for(k=0;eq&&k<3;k++)eq=tn_key.tss[k][0]==st->tss[k][1]&&tn_key.tss[k][1]==st->tss[k][11]&&tn_key.tss[k][2]==st->tss[k][24];
 if(eq){
  s->lit_valid=0;vx_d3d_tn[11]+=tn_key.ns; /* the states not done count every draw, as before */
  if(!tn_world_dirty){tn_same=2;vx_d3d_tn_setup[2]++;return 1;}
  tn_world_dirty=0;tn_mats(t,s);tn_fogw(s);tn_same=1;tn_soa_stale=1;vx_d3d_tn_setup[1]++;return 1;}
 n0=vx_d3d_tn[11];tn_key.valid=0;tn_world_dirty=0;tn_same=0;
 tn_soa_kept_valid=0; /* .648 (board, 3DMark 2001 on .647: 1,993,559 primitives left out for vertices with a NaN rhw): the group frame belongs to the
  * set-up it was made from. A draw prepared in full and refused for room (tn_draw 2, the record size changed) is prepared again after the
  * flush and found kept: the frame of the draw before (its record size, matrices and lights) must not be taken for it */
 if(!tn_prepare_full(c,src,s))return 0;
 tn_key.t=(const void*)t;tn_key.fvf=src->fvf;tn_key.stride=src->vertex_stride;for(k=0;k<4;k++)tn_key.vp[k]=vp[k];
 for(k=0;k<TN_KRS;k++)tn_key.rs[k]=st->rs[tn_krs[k]];
 for(k=0;k<3;k++){tn_key.tss[k][0]=st->tss[k][1];tn_key.tss[k][1]=st->tss[k][11];tn_key.tss[k][2]=st->tss[k][24];}
 tn_key.ns=vx_d3d_tn[11]-n0;tn_key.valid=1;vx_d3d_tn_setup[0]++;
 return 1;
}
static int tn_prepare_full(vx_d3d_context *c,const vx_d3d_input *src,tn_setup *s) {
 tnl_state *t=tnl_of(c);vx_d3d_state *st=&c->state;uint32_t f=src->fvf,off=12,k,n,i;
 memset(s,0,sizeof(*s));
 if((f&0xE)<2||(f&0xE)==4)return 0;
 if((f&0xE)>4)off+=(((f&0xE)-4)>>1)*4; /* .640: XYZB1..XYZB5: the weights passed over (no vertex blending is offered; before: the draw left out) */
 if(f&0x10){s->inormal=off;off+=12;}else s->inormal=0xFFFFFFFFUL;
 if(f&0x20)off+=4; /* PSIZE */
 if(f&0x40){s->idif=off;off+=4;}else s->idif=0xFFFFFFFFUL;
 if(f&0x80){s->ispe=off;off+=4;}else s->ispe=0xFFFFFFFFUL;
 n=(f>>8)&15;if(n>8)return 0;
 for(k=0;k<n;k++){uint32_t fm=(f>>(16+k*2))&3;s->itex[k]=off;s->itn[k]=fm==0?2:fm==1?3:fm==2?4:1;off+=s->itn[k]*4;}
 s->ins=n;if(src->vertex_stride<off)return 0;
 tn_gb=0; /* .701: decided at the end, once the records' form is known */
 tn_mats(t,s); /* .647: the matrices (tn_mats) */
 s->nnorm=st->rs[143]!=0; /* .540: NORMALIZENORMALS: the transformed normal is made unit length only when it is on, as Direct3D does */
 s->lit=st->rs[137]!=0;
 s->dsrc=st->rs[145];s->esrc=st->rs[148];s->asrc=st->rs[147];
 for(k=0;k<4;k++){s->mdif[k]=t->mat[k];s->mamb[k]=t->mat[4+k];s->memi[k]=t->mat[12+k];}
 tn_col(st->rs[139],s->amb);
 if(s->lit){
  for(i=0;i<t->nslot&&s->nl<16;i++){tnl_light *l=&t->light[i];if(!l->on||!l->have)continue; /* .646: the slots; at most 16 lit (the HAL offers 8) */
   if(l->type==3){float x=-l->dir[0],y=-l->dir[1],z=-l->dir[2],e[3],m; /* to the light, in eye space by the view's 3x3 */
    e[0]=x*t->view[0]+y*t->view[4]+z*t->view[8];e[1]=x*t->view[1]+y*t->view[5]+z*t->view[9];e[2]=x*t->view[2]+y*t->view[6]+z*t->view[10];
    m=tn_sqrt(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]);if(m>0){e[0]/=m;e[1]/=m;e[2]/=m;}
    s->ld[s->nl][0]=e[0];s->ld[s->nl][1]=e[1];s->ld[s->nl][2]=e[2];s->ldir[s->nl]=1;}
   else{const float *v=t->view;
    s->lpos[s->nl][0]=l->pos[0]*v[0]+l->pos[1]*v[4]+l->pos[2]*v[8]+v[12];s->lpos[s->nl][1]=l->pos[0]*v[1]+l->pos[1]*v[5]+l->pos[2]*v[9]+v[13];
    s->lpos[s->nl][2]=l->pos[0]*v[2]+l->pos[1]*v[6]+l->pos[2]*v[10]+v[14];s->ldir[s->nl]=0;s->pointlight=1;
    if(l->type==2){float e0=l->dir[0]*v[0]+l->dir[1]*v[4]+l->dir[2]*v[8],e1=l->dir[0]*v[1]+l->dir[1]*v[5]+l->dir[2]*v[9],e2=l->dir[0]*v[2]+l->dir[1]*v[6]+l->dir[2]*v[10],m=tn_sqrt(e0*e0+e1*e1+e2*e2); /* the cone's axis in eye space */
     if(m>0){e0/=m;e1/=m;e2/=m;}
     s->spotmask|=1UL<<s->nl;tn_sdir[s->nl][0]=e0;tn_sdir[s->nl][1]=e1;tn_sdir[s->nl][2]=e2;tn_sct[s->nl]=tn_cos_half(l->theta);tn_scp[s->nl]=tn_cos_half(l->phi);tn_sfall[s->nl]=l->falloff;}}
   s->li[s->nl++]=i;}}
 /* .542: the per-draw parts (see tn_setup) */
 s->ts=t;
 {const vx3dn_rect *vp=&st->viewport;s->vxs=(float)(vp->right-vp->left)*0.5f;s->vx0=(float)vp->left+s->vxs;s->vys=-(float)(vp->bottom-vp->top)*0.5f;s->vy0=(float)vp->top-s->vys;
  s->z0=t->zmin;s->zs=t->zmax-t->zmin;}
 {
  const uint32_t vd=st->rs[141]&&s->idif!=0xFFFFFFFFUL?1:0;
  const uint32_t vs=st->rs[141]&&s->ispe!=0xFFFFFFFFUL?2:0;
  if(s->dsrc!=vd&&s->dsrc!=vs)s->dsrc=0;
  if(s->asrc!=vd&&s->asrc!=vs)s->asrc=0;
  if(s->esrc!=vd&&s->esrc!=vs)s->esrc=0;
  /* .645: specular lighting is done when SPECULARENABLE is on, with lights, normals and a material specular colour that is not black (or the
   * vertex's, SPECULARMATERIALSOURCE); LOCALVIEWER decides the viewer's direction */
  {uint32_t q;for(q=0;q<4;q++)s->mspec[q]=t->mat[8+q];s->mpow=t->mat[16];s->localview=st->rs[142]!=0;
   s->ssrc=st->rs[146];if(s->ssrc!=vd&&s->ssrc!=vs)s->ssrc=0;
   s->spec_on=s->lit&&st->rs[29]&&s->nl&&s->inormal!=0xFFFFFFFFUL&&(s->ssrc||s->mspec[0]!=0.0f||s->mspec[1]!=0.0f||s->mspec[2]!=0.0f);
   if(!s->spec_on)s->ssrc=0;
   /* .697 (board, 3DMark 2001's High Polygon Count, 8 lights, N/A on .694: five 2.6 s frames in the test's 12 s): spcut, the N.H at or below
    * which a light's specular term is under half a step of the colour's 255 shared by all the lights (att x light specular x material
    * specular x (N.H) ^ power, the attenuation at most 1 / a0): such terms are left out, and with them the power and the sum (for a power
    * of 20 and eight lights of colour 1 at most, N.H at or below 0.66: most of a mesh, away from each highlight). The output word stays
    * within one step of the full sum (each term left out is under 1/510 of a step). 0 (nothing left out) when a light's attenuation is not
    * bounded (a0 at or below 0, or a1 or a2 below 0) or the power is 0 or less */
   s->spcut=0.0f;
   if(s->spec_on&&s->mpow>0.0f){float K=0.0f;int bounded=1;uint32_t j,c2;
    for(j=0;j<s->nl;j++){const tnl_light *l=&t->light[s->li[j]];float sm=0.0f,am=1.0f;
     for(c2=0;c2<3;c2++){float v=l->spe[c2]*(s->ssrc?1.0f:s->mspec[c2]);if(v<0.0f)v=-v;if(v>sm)sm=v;} /* a colour from the vertex is at most 1 */
     if(!s->ldir[j]){if(!(l->a0>0.0f)||l->a1<0.0f||l->a2<0.0f){bounded=0;break;}am=1.0f/l->a0;}
     if(sm*am>K)K=sm*am;}
    if(bounded&&K>0.0f){const float x=1.0f/(510.0f*(float)s->nl*K);s->spcut=x>=1.0f?1.0f:tn_pow(x,1.0f/s->mpow);}}}
  s->mat_sources=s->dsrc|s->asrc|s->esrc|s->ssrc;
  s->mconst=!s->mat_sources;
 }
 if(s->lit&&s->mconst){static const uint32_t bgr[3]={2,1,0};
  for(k=0;k<3;k++)s->base4[k]=s->memi[bgr[k]]+s->amb[bgr[k]]*s->mamb[bgr[k]];s->base4[3]=s->mdif[3];
  for(i=0;i<s->nl;i++){const tnl_light *l=&t->light[s->li[i]];
   for(k=0;k<3;k++){s->ld4[i][k]=l->dif[bgr[k]]*s->mdif[bgr[k]];s->la4[i][k]=l->amb[bgr[k]]*s->mamb[bgr[k]];}s->ld4[i][3]=s->la4[i][3]=0.0f;
   if(s->ldir[i])for(k=0;k<3;k++)s->base4[k]+=s->la4[i][k];}}
 s->fog=st->rs[28]&&!st->rs[35]&&st->rs[140];s->fmode=st->rs[140];s->rfog=st->rs[48]!=0;
 s->fs=tf((const uint8_t*)&st->rs[36]);s->fe=tf((const uint8_t*)&st->rs[37]);s->fd=tf((const uint8_t*)&st->rs[38]);
 /* the output sets: one a stage up to the first disabled one, at most two */
 /* .646 (board, 3DMark 2001: 453,331 draws with state not done): .536-.645 gave stage 2 stage 1's output set whatever it asked for (a
  * third stage reading set 0, as a base, light map and detail texture do, got the light map's coordinates). Stages 0 and 1 keep their
  * own sets as before (the records' layout is unchanged); stage 2 now takes the set of the stage that asks for the same coordinates
  * (TEXCOORDINDEX with its generation, and TEXTURETRANSFORMFLAGS); a third different one still gets the second set (the HAL offers two
  * sets), counted. */
 tn_sets=0;for(k=0;k<2;k++){if(st->tss[k][1]==1)break;s->gen[k]=st->tss[k][11]>>16;s->idx[k]=st->tss[k][11]&0xFFFF;s->ttf[k]=st->tss[k][24];tn_sets++;}
 if(!tn_sets){tn_sets=1;s->gen[0]=0;s->idx[0]=0;s->ttf[0]=0;}
 /* .658 (board, VXTLP 6i on .653-.657: every third grid, stage 1 reading set 1 of a one-set vertex, drawn one flat colour; Direct3D's own T&L
  * drew them with set 0's coordinates): a set the vertex lacks is read as set 0 (.653-.657 wrote 0, 0); none at all is still 0, 0 */
 for(k=0;k<tn_sets;k++)if(s->idx[k]>=s->ins&&s->ins)s->idx[k]=0;
 for(k=0;k<8;k++)tn_stage_set[k]=k<tn_sets?k:tn_sets-1;
 if(tn_sets>1&&st->tss[2][1]!=1){
  /* .663 (audit 08): and, when stage 2 transforms its coordinates, by the same matrix (each stage has its own) */
  const int m0=!(st->tss[2][24]&0xFF)||!memcmp(t->tex[2],t->tex[0],sizeof(t->tex[2])),m1=!(st->tss[2][24]&0xFF)||!memcmp(t->tex[2],t->tex[1],sizeof(t->tex[2]));
  if(st->tss[2][11]==st->tss[0][11]&&st->tss[2][24]==st->tss[0][24]&&m0)tn_stage_set[2]=0;
  else if(!(st->tss[2][11]==st->tss[1][11]&&st->tss[2][24]==st->tss[1][24]&&m1)){vx_d3d_tn[11]++;vx_d3d_tn_nd[2]++;}}
 /* .540 (review): TEXTURETRANSFORMFLAGS' count is 0-4 (D3DTTFF_COUNT1-4) with PROJECTED (256); any other value is not a transform this
 * does (counted, the set passed through): .536 indexed o4[count - 1] with the count as given */
 for(k=0;k<tn_sets;k++){
  uint32_t cnt=s->ttf[k]&0xFF;
  if(cnt>4||(s->ttf[k]&~0x1FFUL)){vx_d3d_tn[11]++;vx_d3d_tn_nd[3]++;s->ttf[k]=0;}
  tn_proj[k]=(s->ttf[k]&256)&&(s->ttf[k]&0xFF)>=2;
  if(s->gen[k]==1||s->gen[k]==3)s->gen_normal=1;
  if(s->gen[k]==2||s->gen[k]==3)s->gen_eye=1;
 }
 tn_fogw(s); /* .647 */
 s->nrm_need=s->inormal!=0xFFFFFFFFUL&&(s->lit||s->gen_normal);if(s->nrm_need)tn_nrm(s); /* .678: tn_mats above ran with it 0 (memset) */
 /* .656 (board, VXTLP 6i on .653-.655: the driver's T&L divided a projected set by q at each vertex, which is exact only along the
  * triangle's edges): a set projected with COUNT3 (TTF 103) keeps u, v and q in its record (the FVF's TEXCOORDSIZE3) and its stage keeps
  * TTF 103, so the HAL divides per pixel (unit 0: layout 2; unit 1 on its own set: ulayout PROJ). One such set a draw (a record at most
  * 44 bytes), no third stage (the VxD's three-unit layout has no third coordinate); otherwise the divide at the vertex, as before */
 tn_qs=0;
 if((vx_d3d_feat&VX_F_PROJECTED)&&st->tss[2][1]==1)for(k=0;k<tn_sets&&!tn_qs;k++)
  if(tn_proj[k]&&(s->ttf[k]&0x1FFUL)==0x103UL&&(k==0?(tn_sets==1&&st->tss[1][1]==1)||(vx_d3d_feat&VX_F_UNIT3D):(vx_d3d_feat&VX_F_UNIT3D)!=0))tn_qs=1UL<<k;
 /* .663 (audit 06): a three-component set (one read with three or four, generated, or transformed to three without PROJECTED) keeps its
  * third coordinate in the record as a q set does (the same layout and limits), undivided: a cube map's r (the core's cube layout reads
  * it there) was written 1.0 */
 tn_r3=0;
 if(!tn_qs&&st->tss[2][1]==1)for(k=0;k<tn_sets&&!tn_r3;k++){uint32_t cnt=s->ttf[k]&0xFF,d;
  if(s->gen[k]&&!cnt)d=3;else if(cnt)d=tn_proj[k]?0:cnt;else d=s->idx[k]<s->ins?s->itn[s->idx[k]]:0;
  if(d>=3&&(k==0?(tn_sets==1&&st->tss[1][1]==1)||(vx_d3d_feat&VX_F_UNIT3D):(vx_d3d_feat&VX_F_UNIT3D)!=0))tn_r3=1UL<<k;}
 tn_toff[0]=6;tn_toff[1]=((tn_qs|tn_r3)&1)?9:8;
 /* .540 (review): XYZRHW | DIFFUSE | SPECULAR (C4) and the set count: .536 put the count over 1C4, which already says one set, so two
 * sets read as three (FVF 3C4, 48 bytes, against the records' 40: the core refused every two-stage draw, DDERR_INVALIDPARAMS) */
 tn_fvf=0xC4|(tn_sets<<8)|(((tn_qs|tn_r3)&1)?0x10000UL:0)|(((tn_qs|tn_r3)&2)?0x40000UL:0);tn_ostride=24+tn_sets*8+((tn_qs|tn_r3)?4:0); /* .656: TEXCOORDSIZE3 of the q set (.663: or the r set) */
 /* .698: the guard band: clip x and y halved (exact: a power of two) and the viewport's scales doubled, so the side planes lie at twice
  * the viewport; its edges for TN_OK a pixel wider, inside the VxD's +-4096. .701: only for records the indexed packets take (tn_gb) */
 tn_gb=vx_d3d_gband!=0&&!(tn_qs|tn_r3); /* .703: every form again: the plain packets carry the band too (VX3DN_EXT_GUARD; .701-.702 left it off for these).
  * .721 (board, VXTLP 6Q on .720, VX855): except a set with a third coordinate (q kept, or r): the engine draws the S, T, R layouts wrong
  * with vertices past the viewport (6P NC, a COUNT3 PROJECTED floor across the sides, 35% of its pixels off, the far squares as stripes;
  * 48% across the sides alone, 0% across the near plane alone; the same floor clipped to the viewport: right; strips or lists: no
  * difference). Dragothic's water (3DMark 2001) is such a set: replayed with the band, 3,366 of its 3,414 vertices lay past the viewport.
  * These records' sides are clipped to the viewport, as the runtime does. */
 if(tn_gb)vx_d3d_gbs[tn_sets<=2?0:1]++; /* [1]: records only plain packets take (three sets; .721: a third coordinate takes no band) */
 if(tn_gb){for(k=0;k<16;k+=4){s->wvp[k]*=0.5f;s->wvp[k+1]*=0.5f;}
  s->vxs*=2.0f;s->vys*=2.0f;
  tn_gxl=s->vx0-s->vxs-1.0f;tn_gxr=s->vx0+s->vxs+1.0f;tn_gyt=s->vy0+s->vys-1.0f;tn_gyb=s->vy0-s->vys+1.0f;
  if(tn_gxl<-4096.0f)tn_gxl=-4096.0f;if(tn_gxr>4095.0f)tn_gxr=4095.0f;if(tn_gyt<-4096.0f)tn_gyt=-4096.0f;if(tn_gyb>4095.0f)tn_gyb=4095.0f;} /* .702: +4095 (the engine's guard window is signed 13-bit: -4096 .. +4095) */
 return 1;
}
/* .541: a vertex in two steps. tn_pos (every vertex a draw's triangles use, in tn_draw): the clip-space position and code (tn_cv, tn_cc)
 * and its source (tn_sp). tn_fin (once, when the vertex is first needed: entering a packet in tn_ix, an end of a clipped edge, a
 * triangle on the general path): lighting, fog, the coordinate sets, the divide and the viewport, the record into tn_buf. A vertex only
 * dropped triangles use is never lit (GTA3: 2.2 of 15.8 million triangles dropped wholly outside). tn_s and tn_c: the draw's set-up. */
uint32_t vx_d3d_tn_one;
static tn_setup tn_s;static vx_d3d_context *tn_c;static const uint8_t *tn_sp[TN_MAX+8];static uint8_t tn_done[TN_MAX+8];static uint32_t tn_lazy,tn_lz0;
/* .544: tn_pos (the one-vertex clip-space position, code and position words) is the group kernel tnk_pos (vx_d3d_tnl_soa.inc) */
static void tn_normal(const tn_setup *s,const uint8_t *v,float *nn);static void tn_texsets(uint32_t o,const uint8_t *v,const float *e,const float *nn,int haveN,uint32_t *r); /* .653: for the group path */
#include "vx_d3d_tnl_soa.inc" /* .544: the group kernels, the frame, phases T and L */
static void tn_normal(const tn_setup *s,const uint8_t *v,float *nn) {
 tn_x4(s->nrm16,(const float*)(const void*)(v+s->inormal),nn);
 if(s->nnorm){
  float l=tn_sqrt(nn[0]*nn[0]+nn[1]*nn[1]+nn[2]*nn[2]);
  if(l>0){nn[0]/=l;nn[1]/=l;nn[2]/=l;}
 }
}
static uint32_t tn_light_color(const tn_setup *s,const float *e,const float *nn,int haveN,uint32_t dc,uint32_t sc,uint32_t *specw) {
 float col[4],vb[4],vld[16][4],vla[16][4],vd[4],vs[4],spc[4]={0,0,0,0},V[3]={0,0,-1.0f};
 const tnl_state *t=s->ts;
 const float *base=s->base4,*ms=s->mspec;
 const float (*ld)[4]=s->ld4,(*la)[4]=s->la4;
 static const uint32_t bgr[3]={2,1,0};
 uint32_t k;
  if(!s->mconst){
   const float *md=s->dsrc==1?vd:s->dsrc==2?vs:s->mdif;
   const float *ma=s->asrc==1?vd:s->asrc==2?vs:s->mamb;
   const float *me=s->esrc==1?vd:s->esrc==2?vs:s->memi;
   uint32_t i;
   if(s->mat_sources&1)tn_col(dc,vd);
   if(s->mat_sources&2)tn_col(sc,vs);
   if(s->ssrc)ms=s->ssrc==1?vd:vs; /* .645: the specular material colour from the vertex */
   for(k=0;k<3;k++)vb[k]=me[bgr[k]]+s->amb[bgr[k]]*ma[bgr[k]];vb[3]=md[3];
   for(i=0;i<s->nl;i++){const tnl_light *l=&t->light[s->li[i]];
    for(k=0;k<3;k++){vld[i][k]=l->dif[bgr[k]]*md[bgr[k]];vla[i][k]=l->amb[bgr[k]]*ma[bgr[k]];}vld[i][3]=vla[i][3]=0.0f;
    if(s->ldir[i])for(k=0;k<3;k++)vb[k]+=vla[i][k];}
   base=vb;ld=(const float (*)[4])vld;la=(const float (*)[4])vla;}
  col[0]=base[0];col[1]=base[1];col[2]=base[2];col[3]=base[3];
  if(s->spec_on&&s->localview){float l=tn_sqrt(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]);if(l>0){V[0]=-e[0]/l;V[1]=-e[1]/l;V[2]=-e[2]/l;}} /* to the eye, from the vertex */
  for(k=0;k<s->nl;k++){float ndl,att=1.0f;const float *L;float Lp[3];
   if(s->ldir[k])L=s->ld[k];
   else{const tnl_light *l=&t->light[s->li[k]];float dd,q;Lp[0]=s->lpos[k][0]-e[0];Lp[1]=s->lpos[k][1]-e[1];Lp[2]=s->lpos[k][2]-e[2];dd=tn_sqrt(Lp[0]*Lp[0]+Lp[1]*Lp[1]+Lp[2]*Lp[2]);
    if(dd>l->range)continue;if(dd>0){Lp[0]/=dd;Lp[1]/=dd;Lp[2]/=dd;}q=l->a0+l->a1*dd+l->a2*dd*dd;att=q>0?1.0f/q:1.0f;
    if(s->spotmask&(1UL<<k)){ /* .645: the cone: rho = cos of the angle between its axis and the way from the light to the vertex; 1 inside theta, 0 outside phi, between them ((rho - cos(phi/2)) / (cos(theta/2) - cos(phi/2))) ^ falloff */
     float rho=-(Lp[0]*tn_sdir[k][0]+Lp[1]*tn_sdir[k][1]+Lp[2]*tn_sdir[k][2]);
     if(rho<=tn_scp[k])continue;
     if(rho<tn_sct[k]){float f=(rho-tn_scp[k])/(tn_sct[k]-tn_scp[k]);att*=tn_pow(f,tn_sfall[k]);}}
    L=Lp;tn_ma4(col,&att,la[k]);}
   if(!haveN)continue;
   ndl=nn[0]*L[0]+nn[1]*L[1]+nn[2]*L[2];if(ndl<=0)continue;
   if(s->spec_on){ /* .645: the half vector of the light and the viewer: ms x ls x att x (N.H) ^ power */
    float H[3],hl=0,ndh;uint32_t c;
    H[0]=L[0]+V[0];H[1]=L[1]+V[1];H[2]=L[2]+V[2];hl=tn_sqrt(H[0]*H[0]+H[1]*H[1]+H[2]*H[2]);
    if(hl>0){ndh=(nn[0]*H[0]+nn[1]*H[1]+nn[2]*H[2])/hl;
     if(ndh>s->spcut){const float *ls=t->light[s->li[k]].spe;float f=att*tn_pow(ndh,s->mpow); /* .697: spcut (0 or more) */
      for(c=0;c<3;c++)spc[c]+=f*ls[bgr[c]]*ms[bgr[c]];}}}
   ndl*=att;tn_ma4(col,&ndl,ld[k]);}
 if(s->spec_on)*specw=tn_pk4(spc,tn_255)&0x00FFFFFFUL;
 return tn_pk4(col,tn_255);
}

/* Without point lights, the diffuse result depends only on the normal and
 * material inputs. Reuse the last result within this draw, where the matrices,
 * light state and material are fixed. Fog still uses each vertex's depth. */
static uint32_t tn_diffuse(tn_setup *s,const uint8_t *v,const float *e,float *nn,int haveN,uint32_t dc,uint32_t sc,uint32_t *specw) {
 uint32_t nx=0,ny=0,nz=0,dc_key,sc_key,lit;
 if(s->pointlight||s->spec_on){ /* .645: and with specular lighting: it depends on the vertex's place too (the viewer's direction) */
  if(haveN&&!s->gen_normal)tn_normal(s,v,nn);
  return tn_light_color(s,e,nn,haveN,dc,sc,specw);
 }
 if(haveN&&s->nl){
  const uint8_t *p=v+s->inormal;
  nx=u32(p);ny=u32(p+4);nz=u32(p+8);
 }
 dc_key=(s->mat_sources&1)?dc:0;sc_key=(s->mat_sources&2)?sc:0;
 if(s->lit_valid&&s->lit_key[0]==nx&&s->lit_key[1]==ny&&s->lit_key[2]==nz&&
    s->lit_key[3]==dc_key&&s->lit_key[4]==sc_key)return s->lit_color;
 if(haveN&&s->nl&&!s->gen_normal)tn_normal(s,v,nn);
 lit=tn_light_color(s,e,nn,haveN,dc,sc,specw);
 s->lit_key[0]=nx;s->lit_key[1]=ny;s->lit_key[2]=nz;s->lit_key[3]=dc_key;s->lit_key[4]=sc_key;
 s->lit_color=lit;s->lit_valid=1;
 return lit;
}

/* Finish the attributes once, on first use by a packet or by the clipper. */
/* .653: the output coordinate sets of one vertex (place o, source v; e its eye-space position, nn its normal when a set is generated
 * from them): the one-vertex code's, also called by the group path for draws with generated or transformed sets */
static void tn_texsets(uint32_t o,const uint8_t *v,const float *e,const float *nn,int haveN,uint32_t *r) {
 const tn_setup *s=&tn_s;const tnl_state *t=s->ts;uint32_t k;
 for(k=0;k<tn_sets;k++){float in4[4],u=0,vv=0;uint32_t g=s->gen[k],cnt=s->ttf[k]&0xFF;
  if(!g&&!cnt){
   uint32_t u_bits=0,v_bits=0;
   if(s->idx[k]<s->ins){
    const uint32_t idx=s->idx[k];
    const uint8_t *q=v+s->itex[idx];
    u_bits=u32(q);if(s->itn[idx]>1)v_bits=u32(q+4);
    if(tn_r3&(1UL<<k)){uint32_t w=u32(q+8);r[tn_toff[k]+2]=(w&0x7FFFFFFFUL)>=0x7F800000UL?0:w;} /* .663: r (tn_r3 asks three or four) */
   }
   r[tn_toff[k]]=(u_bits&0x7FFFFFFFUL)>=0x7F800000UL?0:u_bits;
   r[tn_toff[k]+1]=(v_bits&0x7FFFFFFFUL)>=0x7F800000UL?0:v_bits;
   continue;
  }
  if(g==1&&haveN){in4[0]=nn[0];in4[1]=nn[1];in4[2]=nn[2];in4[3]=1;}
  else if(g==2){in4[0]=e[0];in4[1]=e[1];in4[2]=e[2];in4[3]=1;}
  else if(g==3){float E[3],l=tn_sqrt(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]),ne;
   if(tn_c->state.rs[142]&&l>0){E[0]=-e[0]/l;E[1]=-e[1]/l;E[2]=-e[2]/l;}else{E[0]=0;E[1]=0;E[2]=-1;}
   ne=nn[0]*E[0]+nn[1]*E[1]+nn[2]*E[2];in4[0]=2*ne*nn[0]-E[0];in4[1]=2*ne*nn[1]-E[1];in4[2]=2*ne*nn[2]-E[2];in4[3]=1;}
  /* .663 (audit 07): the vertex's coordinates as many as the FVF gives, the first one missing 1 and the rest 0 ((u, v) is (u, v, 1, 0) as
   * before; a third coordinate was read as 1) */
  else if(s->idx[k]<s->ins){const uint8_t *q=v+s->itex[s->idx[k]];uint32_t nc=s->itn[s->idx[k]],j;for(j=0;j<4;j++)in4[j]=j<nc?tf(q+j*4):j==nc?1.0f:0.0f;g=0;}
  else{in4[0]=0;in4[1]=0;in4[2]=1;in4[3]=0;}
  if(g||cnt)vx_d3d_tn[10]++;
  if(cnt){const float *M=t->tex[k];float o4[4];uint32_t j;for(j=0;j<4;j++)o4[j]=in4[0]*M[j]+in4[1]*M[4+j]+in4[2]*M[8+j]+in4[3]*M[12+j];
   u=o4[0];vv=cnt>1?o4[1]:0;
   if(tn_proj[k]){float qq=o4[cnt-1];tn_uvq[o][k][0]=u;tn_uvq[o][k][1]=vv;tn_uvq[o][k][2]=qq;
    if(tn_qs&(1UL<<k)){r[tn_toff[k]+2]=tn_inf(qq)?0x3F800000UL:tn_bits(qq);} /* .656: q kept, the HAL divides; a q not finite as 1 (the VxD refuses it) */
    else if(qq!=0.0f){u/=qq;vv/=qq;}}} /* cnt 2-4 here */
  else{u=in4[0];vv=in4[1];}
  if(tn_r3&(1UL<<k)){float rr=cnt?in4[0]*t->tex[k][2]+in4[1]*t->tex[k][6]+in4[2]*t->tex[k][10]+in4[3]*t->tex[k][14]:in4[2];r[tn_toff[k]+2]=tn_inf(rr)?0:tn_bits(rr);} /* .663: r */
  if(tn_inf(u))u=0;if(tn_inf(vv))vv=0; /* .540 (review): the VxD refuses a non-finite coordinate (vx_native3d_hw.c); the core does not check them */
  r[tn_toff[k]]=tn_bits(u);r[tn_toff[k]+1]=tn_bits(vv);}
}
static void tn_attributes_b(uint32_t o);
static void tn_attributes(uint32_t o) {uint32_t q=TNP(5);tn_attributes_b(o);TNP_BACK(q);}
static void tn_attributes_b(uint32_t o) {
 tn_setup *s=&tn_s;
 const uint8_t *v=tn_sp[o];
 const tnl_state *t=s->ts;
 float e[4]={0,0,0,0},nn[4]={0,0,0,0};
 uint32_t k,dc,sc,s32=0xFF000000UL;
 uint32_t *r=(uint32_t*)(void*)(tn_buf+o*tn_ostride);
 const int haveN=s->inormal!=0xFFFFFFFFUL;
 tn_done[o]=1;vx_d3d_tn_one++; /* .643: vertices finished one at a time (QueryPhases 389) */
 if(s->fog_clip_w)e[2]=tn_cw[o];
 else if(s->fog||s->pointlight||s->gen_eye||s->spec_on)tn_x4(s->wv,(const float*)(const void*)v,e);
 if(haveN&&s->gen_normal)tn_normal(s,v,nn);
 dc=s->idif!=0xFFFFFFFFUL?u32(v+s->idif):0xFFFFFFFFUL;
 sc=s->ispe!=0xFFFFFFFFUL?u32(v+s->ispe):0;
 if(s->fog){
  float d=s->rfog?tn_sqrt(e[0]*e[0]+e[1]*e[1]+e[2]*e[2]):(e[2]<0?-e[2]:e[2]);
  float sp[4]={0,0,0,1.0f};
  vx_d3d_tn[9]++;
  if(s->fmode==3)sp[3]=s->fe>s->fs?(s->fe-d)/(s->fe-s->fs):1.0f;
  else if(s->fmode==1)sp[3]=tn_exp(-s->fd*d);
  else{float u=s->fd*d;sp[3]=tn_exp(-u*u);}
  s32=tn_pk4(sp,tn_255);
 }
 if(s->lit){uint32_t sw=0;vx_d3d_tn[8]++;r[4]=tn_diffuse(s,v,e,nn,haveN,dc,sc,&sw);s32|=sw;} /* .645: sw is 0 unless specular lighting is on */
 else{r[4]=dc;s32|=sc&0x00FFFFFFUL;}
 r[5]=s32;
 tn_texsets(o,v,e,nn,haveN,r);
}
static void tn_fin(uint32_t o) {if(!tn_done[o])tn_attributes(o);}
/* Keep the normal float interpolation. Opposite finite endpoints can
 * overflow the float difference even though the interpolant is finite. */
static float tn_lerp_float(float a,float b,float t) {
 float value=a+(b-a)*t;
 if(tn_inf(value)&&!tn_inf(a)&&!tn_inf(b)&&!tn_inf(t))
  value=(float)((double)a+((double)b-(double)a)*(double)t);
 return value;
}
/* .718 (outside review, reproduced on the host with .716's core): tn_lm, the planes the clip in progress clips (its masked codes). When scratch
 * room is short the triangle and line paths clip near and far only (mask 07) and leave the side planes to the per-primitive path; tn_lerp
 * then held a near-plane vertex beyond a side inside the viewport, x and y moved to the edge while its depth, rhw, coordinates and colour
 * stayed the off-screen point's, and the later clipper took that as an intersection (a corner 32 pixels off, s 19 texels off on a
 * 256-wide texture: a reflection-mapped triangle across the near plane and a screen edge). Now x is held only when the x planes are clipped
 * here, y only when the y planes are (the hold is for 1/w's rounding at an edge just clipped); depth is always held (near and far are). */
static uint32_t tn_lm=0x7F;uint32_t vx_d3d_tn_dfr[4]; /* .718: [0] draws whose triangles clipped near and far only (the side planes deferred), [1] their lines' too, [2] new vertices beyond an x edge there (.716 moved them), [3] beyond a y edge */
/* a vertex made by clipping, between scratch vertices a and b at t (clip space, before the divide) */
static uint32_t tn_lerp(vx_d3d_context *c,uint32_t a,uint32_t b,float tt,uint32_t pl) {
 uint32_t o=tn_P++,k;const uint8_t *A=tn_buf+a*tn_ostride,*B=tn_buf+b*tn_ostride;uint8_t *w=tn_buf+o*tn_ostride;float p[4],rw,sx,sy,sz,l0,h0;
 (void)c;
 p[0]=tn_lerp_float(tn_cx[a],tn_cx[b],tt);
 p[1]=tn_lerp_float(tn_cy[a],tn_cy[b],tt);
 p[2]=tn_lerp_float(tn_cz[a],tn_cz[b],tt);
 p[3]=tn_lerp_float(tn_cw[a],tn_cw[b],tt);
 /* on the plane exactly (rounding put the vertex a little outside it); .543: the six planes */
 if(pl==0)p[2]=0.0f;else if(pl==1)p[2]=p[3];else if(pl==2)p[0]=-p[3];else if(pl==3)p[0]=p[3];else if(pl==4)p[1]=-p[3];else p[1]=p[3];
 tn_cx[o]=p[0];tn_cy[o]=p[1];tn_cz[o]=p[2];tn_cw[o]=p[3];rw=p[3]>0?1.0f/p[3]:0.0f;tn_cc[o]=0;
 /* .543: as tn_pos, then held inside the viewport and the depth range (1/w's rounding put a vertex on an edge just past it: the packet's
  * check would have sent its triangle to the per-triangle path) */
 sx=tn_s.vx0+p[0]*rw*tn_s.vxs;sy=tn_s.vy0+p[1]*rw*tn_s.vys;sz=tn_s.z0+p[2]*rw*tn_s.zs;
 l0=tn_s.vx0-tn_s.vxs;h0=tn_s.vx0+tn_s.vxs;if(tn_lm&(8|16)){if(sx<l0)sx=l0;if(sx>h0)sx=h0;}else if(sx<l0||sx>h0)vx_d3d_tn_dfr[2]++; /* .718: held only when the x planes are clipped here */
 l0=tn_s.vy0+tn_s.vys;h0=tn_s.vy0-tn_s.vys;if(tn_lm&(32|64)){if(sy<l0)sy=l0;if(sy>h0)sy=h0;}else if(sy<l0||sy>h0)vx_d3d_tn_dfr[3]++;
 l0=tn_s.zs<0?tn_s.z0+tn_s.zs:tn_s.z0;h0=tn_s.zs<0?tn_s.z0:tn_s.z0+tn_s.zs;if(sz<l0)sz=l0;if(sz>h0)sz=h0;
 if(tn_bits(sx)==0x80000000UL)sx=0.0f;if(tn_bits(sy)==0x80000000UL)sy=0.0f;if(tn_bits(sz)==0x80000000UL)sz=0.0f;
 *(vx_f32a*)w=sx;*(vx_f32a*)(w+4)=sy;*(vx_f32a*)(w+8)=sz;*(vx_f32a*)(w+12)=rw;
 for(k=16;k<24;k+=4){float f4[4];uint32_t j;for(j=0;j<4;j++){float x=(float)A[k+j],y=(float)B[k+j];f4[j]=x+(y-x)*tt;}*(vx_u32a*)(w+k)=tn_pk4(f4,tn_one);} /* the colour bytes (.543: tn_pk4, as 0..255) */
 for(k=24;k<tn_ostride;k+=4){
  float v=tn_lerp_float(tf(A+k),tf(B+k),tt);
  *(vx_f32a*)(w+k)=tn_inf(v)?0.0f:v;
 }
 /* .540 (review): a projected set: u, v and q interpolated, then divided (the quotients interpolated were wrong across the cut) */
 for(k=0;k<tn_sets;k++)if(tn_proj[k]&&!(tn_qs&(1UL<<k))){float u,v,q;uint32_t j;for(j=0;j<3;j++)tn_uvq[o][k][j]=tn_lerp_float(tn_uvq[a][k][j],tn_uvq[b][k][j],tt); /* .656: a set with q in its record: the words above (u, v, q interpolated) are already right */
  u=tn_uvq[o][k][0];v=tn_uvq[o][k][1];q=tn_uvq[o][k][2];if(q!=0.0f){u/=q;v/=q;}if(tn_inf(u))u=0;if(tn_inf(v))v=0;*(vx_f32a*)(w+tn_toff[k]*4)=u;*(vx_f32a*)(w+tn_toff[k]*4+4)=v;}
tn_done[o]=1;vx_d3d_tn[5]++;return o;
}
/* a triangle across planes it crosses (u: its vertices' codes together; .543: the side planes as well as near and far): clipped
 * (Sutherland-Hodgman, each plane in clip space; a vertex made by clipping lies inside every plane its ends are), the fan's triangles into
 * the token. At most 9 vertices (3 + one a plane), 7 triangles. */
static float tn_pd(uint32_t pl,uint32_t i) {return pl==0?tn_cz[i]:pl==1?tn_cw[i]-tn_cz[i]:pl==2?tn_cx[i]+tn_cw[i]:pl==3?tn_cw[i]-tn_cx[i]:pl==4?tn_cy[i]+tn_cw[i]:tn_cw[i]-tn_cy[i];}
/* Finite clip coordinates can overflow the float plane distance or
 * the distance difference. Rebuild the affected crossing in double. */
static double tn_pd_wide(uint32_t pl,uint32_t i) {
 const double w=tn_cw[i];
 switch(pl){
 case 0:return tn_cz[i];
 case 1:return w-tn_cz[i];
 case 2:return w+tn_cx[i];
 case 3:return w-tn_cx[i];
 case 4:return w+tn_cy[i];
 default:return w-tn_cy[i];
 }
}
static float tn_clip_fraction(uint32_t pl,uint32_t i,uint32_t j,float di,float dj) {
 const float span=di-dj;
 if(tn_inf(span)){
  const double a=tn_pd_wide(pl,i),b=tn_pd_wide(pl,j);
  return (float)(a/(a-b));
 }
 return di/span;
}
/* .652 (a review of .651): a triangle the clipper gives up on (more than two crossings of one plane, which only rounding at extreme
 * coordinates makes, or more than nine vertices) is left out; it was counted as clipped and nowhere as left out (QueryPhases 413) */
uint32_t vx_d3d_tn_cdrop;
/* .663 (audit 13): FILLMODE point or wireframe draws only the edges D3DTRIFLAG_EDGEENABLE1..3 enable (INDEXEDTRIANGLELIST's flags); the
 * list handed on by tn_draw has no flags, so each output triangle's are kept here (tn_tef, its bits as prim_edges takes them) while
 * tn_efon: the source triangle's for one kept whole, and for a clipped one's fan the parts of its enabled edges (an edge along a clipping
 * plane or a diagonal of the fan is not one) */
static uint8_t tn_tef[TN_OUT];static uint32_t tn_efon,tn_clip_ef;
static uint32_t tn_clip_b(vx_d3d_context *c,uint32_t a,uint32_t b,uint32_t d,uint16_t *ix,uint32_t m,uint32_t u);
static uint32_t tn_clip(vx_d3d_context *c,uint32_t a,uint32_t b,uint32_t d,uint16_t *ix,uint32_t m,uint32_t u) {uint32_t o=TNP(4),r=tn_clip_b(c,a,b,d,ix,m,u);TNP_BACK(o);return r;}
static uint32_t tn_clip_b(vx_d3d_context *c,uint32_t a,uint32_t b,uint32_t d,uint16_t *ix,uint32_t m,uint32_t u) {
 static const uint8_t bit[6]={1,2,8,16,32,64};
 uint32_t pa[9],pb[9],na=3,pl,k;uint8_t ea[9],eb[9]; /* .663: ea[k], the edge from pa[k] to the next: original edge 0..2, or 3 along a plane */
 pa[0]=a;pa[1]=b;pa[2]=d;ea[0]=0;ea[1]=1;ea[2]=2;tn_lm=u; /* .718 */
 for(pl=0;pl<6;pl++)if(u&bit[pl]){
  uint32_t i=pa[0],nb=0,crossings=0;
  float di=tn_pd(pl,i);
  for(k=0;k<na;k++){
   const uint32_t j=pa[k+1<na?k+1:0];
   const float dj=tn_pd(pl,j);
   if(di>=0){if(nb==9){vx_d3d_tn_cdrop++;return m;}eb[nb]=(uint8_t)(di==0&&dj<0?3:ea[k]);pb[nb++]=i;}
   if((di>0&&dj<0)||(di<0&&dj>0)){
    /* Two crossings per plane bound scratch growth and token expansion.
     * Stop even if extreme coordinates violate convexity after rounding. */
    if(nb==9||++crossings>2){vx_d3d_tn_cdrop++;return m;}
    tn_fin(i);tn_fin(j);
    eb[nb]=(uint8_t)(di>0?3:ea[k]); /* going out: the next edge runs along the plane; coming in: it is the rest of this one */
    pb[nb++]=tn_lerp(c,i,j,tn_clip_fraction(pl,i,j,di,dj),pl);
   }
   i=j;di=dj;
  }
  if(nb<3)return m;
  for(k=0;k<nb;k++){pa[k]=pb[k];ea[k]=eb[k];}
  na=nb;
 }
 /* .663 (audit 11): flat shading takes each triangle's first vertex's colours, and a vertex made by clipping has interpolated ones: the
  * vertices made here take the first vertex's diffuse and specular colours (the specular alpha, the fog factor, stays interpolated), and
  * the fan starts at the first vertex, or at a vertex made here when the first was clipped away (the polygon is convex) */
 if(FLATSW(c)){uint32_t st=na,q;tn_fin(a);
  {const uint8_t *s0=tn_buf+a*tn_ostride;const uint32_t d0=*(const vx_u32a*)(s0+16),p0=*(const vx_u32a*)(s0+20);
   for(k=0;k<na;k++){if(pa[k]==a){if(st==na||pa[st]!=a)st=k;continue;}
    if(pa[k]==b||pa[k]==d)continue;
    {uint8_t *w=tn_buf+pa[k]*tn_ostride;*(vx_u32a*)(w+16)=d0;*(vx_u32a*)(w+20)=(p0&0x00FFFFFFUL)|(*(vx_u32a*)(w+20)&0xFF000000UL);}
    if(st==na)st=k;}}
  if(st<na&&st){uint32_t r[9];uint8_t re[9];for(q=0;q<na;q++){r[q]=pa[(st+q)%na];re[q]=ea[(st+q)%na];}for(q=0;q<na;q++){pa[q]=r[q];ea[q]=re[q];}}}
 for(k=1;k+1<na;k++,m++,ix+=3){
  ix[0]=(uint16_t)pa[0];ix[1]=(uint16_t)pa[k];ix[2]=(uint16_t)pa[k+1];
  if(tn_efon&&m<TN_OUT){
#define TN_EON(e) ((e)<3&&((tn_clip_ef>>(e))&1))
   tn_tef[m]=(uint8_t)((k==1&&TN_EON(ea[0])?1:0)|(TN_EON(ea[k])?2:0)|(k+2==na&&TN_EON(ea[na-1])?4:0));
#undef TN_EON
  }
 }
 return m;
}

/* 9 bits for the packet slot, 7 for the stamp. The table is invalidated
 * on scratch restart and whenever ix_new changes the packet mapping. */
#define TN_SLOT_STEP 512
#if VX3DN_IX_VERTICES > TN_SLOT_STEP
#error T&L packet slots do not fit
#endif
static uint16_t tn_slot[TN_MAX];
static uint32_t tn_slot_tag,tn_slot_g,tn_slot_hi; /* .661: tn_slot_hi: one past the highest place written since the last clear (TN_PUT): the clear at the stamp's wrap covers only those (callgrind: the whole table's memset was 3.6% of a dense grid) */
static void tn_slot_reset(void) {
 tn_slot_g=ix_g;
 tn_slot_tag+=TN_SLOT_STEP;
 if(tn_slot_tag==0x10000UL){
  memset(tn_slot,0,tn_slot_hi*sizeof(tn_slot[0]));tn_slot_hi=0;
  tn_slot_tag=TN_SLOT_STEP;
 }
}
/* a draw command of untransformed vertices: its triangles' vertices transformed into the scratch buffer and the command rewritten as an
 * indexed triangle list into it (tn_tok: header, base 0, indices). 1: done (*op, *n, *p set to the list), 0: the draw left out (counted),
 * 2: the scratch buffer is full (the caller sends the batch and calls again with tn_P 0). */
uint32_t vx_d3d_tn_snap[2];
static uint16_t tn_need[TN_MAX+8];
uint32_t vx_d3d_tn_pre[2]; /* .675: the corners of clipped triangles lit in groups (draws, corners); .676: always, with two lights or more or specular lighting (ACCEL_D3D_PRECLIP removed) */
/* .676: ACCEL_D3D_PRECULL and ACCEL_D3D_IXSKIP (features under work, VXDDRAW.DLL reads them at its start); vx_d3d_tn_pcull: [0] draws
 * culled before lighting, [1] triangles culled there, [2] triangles the packets took without their back-face test (tn_ix; .677: the screen and depth test always made) */
uint32_t vx_d3d_tn_pcull[3]; /* .682: ACCEL_D3D_PRECULL, _IXSKIP, _PCLGATE and _SPECSE removed, all on (T681 GRID) */
uint32_t vx_d3d_tn_fastix=1,vx_d3d_tn_fastix_stat[4]; /* .680: tn_ix's block path (ACCEL_D3D_TNFASTIX) */
/* .681: the dense draw's position gather (tn_draw: a range taken in order) asks for the source vertex eight vertices ahead (prefetcht0).
 * T676's HarborBench pursuit profile: that loop's 64-byte line alone 2.8% of the frame, about 80 cycles a vertex (the application's vertex
 * buffer read cold, each miss waited for in turn). .682: ACCEL_D3D_TNPF removed, always on (T681 GRID: off, 6j 1.2% slower). */ /* .677: the .676 PRECLIP gate (ACCEL_D3D_PCLGATE) and the specular reuse (ACCEL_D3D_SPECSE) switchable */
static uint8_t tn_ok[TN_MAX+8]; /* .676: a place of a kept, wholly inside triangle of a draw culled before lighting (tn_ix: its tests done) */
static uint8_t tn_nd[TN_MAX+8];
/* .640 (outside review of .637, reproduced on the host: with the driver's T&L offered, an untransformed point and an untransformed line
 * each answered DD_OK and drew nothing; tn_draw took triangles only): points and lines of untransformed vertices. 1 POINTS {count,
 * start} x n; 2 INDEXEDLINELIST {a, b} x n; 15 LINELIST start; 16 LINESTRIP start; 17 INDEXEDLINESTRIP base, n + 1 indices; 27
 * INDEXEDLINELIST2 base, {a, b} x n. The vertices they use are transformed and lit as a triangle's are; a point outside any plane is
 * left out; a line is cut at the planes it crosses (the end outside moved onto the plane, tn_lerp, as the triangle clipper's vertices).
 * The command is rewritten for the scratch buffer: points as runs {count, start}, lines as INDEXEDLINELIST2 with base 0.
 * 24 LINELIST_IMM carries its vertices in the command (the runtime's own clipped output: transformed) and is not taken here.
 * vx_d3d_tn_pl: [0] point draws, [1] points kept, [2] line draws, [3] lines kept, [4] lines cut, [5] points and lines outside. */
uint32_t vx_d3d_tn_dense[4],vx_d3d_tn_dense_on=1; /* .642; _on: 0 in host tests that compare the two ways */
uint32_t vx_d3d_tn_pl[6],vx_d3d_tn_split[2]; /* tn_split: [0] draws taken in pieces, [1] pieces */
#define TN_PIECE 2340 /* triangles a piece: 7 places each under TN_MAX */
#define TN_PIECE_L 4096 /* lines a piece: 2 ends and 2 cuts each */
static uint32_t tn_split,tn_k0,tn_more;
static uint32_t tn_pl_index(uint32_t o,const uint8_t *d,uint32_t k) { /* the k-th vertex index of a line command (two a line), from the buffer's start */
 const uint32_t j=k>>1,e=k&1;
 switch(o){
 case 2:return u16(d+j*4+e*2);
 case 15:return (uint32_t)u16(d)+j*2+e;
 case 16:return (uint32_t)u16(d)+j+e;
 case 17:return (uint32_t)u16(d)+u16(d+2+(j+e)*2);
 default:return (uint32_t)u16(d)+u16(d+2+j*4+e*2); /* 27 */
 }
}
static int tn_draw_pl(vx_d3d_context *c,const vx_d3d_input *src,uint32_t *op,uint32_t *n,const uint8_t **p) {
 static const uint8_t bit[6]={1,2,8,16,32,64};
 const uint32_t o=*op,cnt=*n,per=o==1?1U:2U,k0=tn_split?tn_k0:0,lim=tn_split?(per==1?(uint32_t)TN_MAX:(uint32_t)TN_PIECE_L):0xFFFFFFFFUL;const uint8_t *data=*p;uint16_t *st;
 uint32_t np=0,tot=0,ni,lo=0xFFFFFFFFUL,hi=0,i,j,k,nv,need,used=0,P0,m=0,mask=0x7F,wide;
 if(!cnt){vx_d3d_tn[7]++;return 0;}
 /* the primitives from the k0-th, at most lim (a piece: tn_split, as the triangles'); tot: the command's */
 if(o==1){for(j=0;j<cnt;j++){const uint32_t rc=u16(data+j*4),rs=u16(data+j*4+2);uint32_t a=tot<k0?k0-tot:0,e;
   tot+=rc;if(a>=rc)continue;e=rc;if(np+(e-a)>lim)e=a+(lim-np);if(e<=a)continue;
   np+=e-a;if(rs+a<lo)lo=rs+a;if(rs+e-1>hi)hi=rs+e-1;}
  if(!tn_split&&np>TN_MAX)return 3;}
 else {tot=cnt;if(k0>=tot){vx_d3d_tn[7]++;return 0;}np=tot-k0;if(np>lim)np=lim;
  if(!tn_split&&np>TN_PIECE_L)return 3;
  for(k=0;k<np*2;k++){const uint32_t a=tn_pl_index(o,data,k0*2+k);if(a<lo)lo=a;if(a>hi)hi=a;}}
 ni=np*per;
 if(!np||hi<lo){vx_d3d_tn[7]++;return 0;}
 nv=hi-lo+1;wide=nv>TN_MAX;need=(nv<ni&&!wide?nv:ni)+(per==2?np*2:0); /* the vertices used, then at most two made by the near and far planes a line (the side planes when the room allows, below) */
 if(!src->vertices||!src->vertex_stride||hi>=src->vertex_bytes/src->vertex_stride||need>TN_MAX){vx_d3d_tn[7]++;return 0;}
 {uint32_t was=tn_ostride;if(!tn_prepare(c,src,&tn_s)){vx_d3d_tn[7]++;return 0;}tn_c=c;
  if(tn_P&&tn_ostride!=was)return 2;}
 if(tn_P+need>TN_MAX)return 2;
 (void)tn_soa_prepare();
 st=tn_tok+3+(TN_OUT*3-ni); /* the source indices (less lo) at the token's tail: the output, never longer a primitive, is written from its head */
 /* .663 (audit 15): a wide draw (more than TN_MAX vertices between its lowest and highest index) can span more than 16 bits: its
  * indices are read again in the loop below, in the same order, and kept 32-bit (a point at lo + 65536 was read as the one at lo) */
 if(!wide){
  if(o==1){uint32_t seen=0;k=0;for(j=0;j<cnt&&k<np;j++){const uint32_t rc=u16(data+j*4),rs=u16(data+j*4+2);for(i=0;i<rc&&k<np;i++,seen++)if(seen>=k0)st[k++]=(uint16_t)(rs+i-lo);}}
  else for(k=0;k<ni;k++)st[k]=(uint16_t)(tn_pl_index(o,data,k0*2+k)-lo);
 }
 if(++tn_epoch==0){memset(tn_ref,0,sizeof(tn_ref));tn_epoch=1;}
 P0=tn_P;
 if(!P0)tn_slot_reset();
 {uint32_t wj=0,wi=0,wseen=0; /* the wide points' run, place in it, points seen */
 for(i=0;i<ni;i++){uint32_t qq;
  if(!wide)qq=st[i];
  else if(o!=1)qq=tn_pl_index(o,data,k0*2+i)-lo;
  else for(;;){const uint32_t rc=u16(data+wj*4);if(wi>=rc){wj++;wi=0;continue;}if(wseen++>=k0){qq=u16(data+wj*4+2)+wi-lo;wi++;break;}wi++;}
  if(wide||tn_ref[qq]!=tn_epoch){const uint8_t *v=src->vertices+(lo+qq)*src->vertex_stride;const uint32_t pl=P0+used,ln=used&(TN_CH-1);
   if(!wide){tn_ref[qq]=tn_epoch;tn_map[qq]=(uint16_t)pl;}tn_sp[pl]=v;tn_sx[ln]=tf(v);tn_sy[ln]=tf(v+4);tn_sz[ln]=tf(v+8);used++;
   if(ln==TN_CH-1)tn_soa_pos(pl+1-TN_CH,TN_CH);
   st[i]=(uint16_t)pl;
  }
  else st[i]=tn_map[qq];
 }}
 if(tn_split){tn_k0+=np;tn_more=tn_k0<tot;vx_d3d_tn_split[1]++;}
 if(used&(TN_CH-1))tn_soa_pos(P0+(used&~(TN_CH-1)),used&(TN_CH-1));
 tn_vdone(P0,used); /* .702 */
 vx_d3d_tn[0]++;vx_d3d_tn[2]+=used;tn_P+=used;tn_lz0=P0;vx_d3d_tn_pl[per==1?0:2]++;
 for(i=P0;i<P0+used;i++)if(!(tn_cc[i]&0x7F))tn_fin(i); /* every vertex inside finished now: the point and line paths read the records as they are */
 if(per==1){uint16_t *out=tn_tok+3; /* runs of kept points whose places follow each other */
  for(i=0;i<np;i++){const uint32_t x=st[i];
   if(tn_cc[x]&0x7F){vx_d3d_tn_pl[5]++;continue;}
   if(m&&(uint32_t)out[(m-1)*2+1]+out[(m-1)*2]==x&&out[(m-1)*2]<0xFFFFU)out[(m-1)*2]++;
   else {out[m*2]=1;out[m*2+1]=(uint16_t)x;m++;}
   vx_d3d_tn_pl[1]++;}
  tn_lazy=0;*op=1;*n=m;*p=(const uint8_t*)(tn_tok+3);
  return 1;}
 /* the side planes when every crossing line's six possible cuts fit; else near and far only (the room for those was checked above) and the
  * rest to the per-primitive path, which clips a line on the screen */
 {uint32_t nc=0;for(i=0;i<np;i++){const uint32_t a=st[i*2],b=st[i*2+1];if(((tn_cc[a]|tn_cc[b])&0x7F)&&!(tn_cc[a]&tn_cc[b]&0x7F))nc++;}
  if(tn_P+nc*6>TN_MAX){mask=0x07;vx_d3d_tn_dfr[1]++;}}
 {uint16_t *out=tn_tok+3;
  for(i=0;i<np;i++){uint32_t a=st[i*2],b=st[i*2+1];const uint32_t all=(tn_cc[a]|tn_cc[b])&0x7F,u=all&mask;int gone=0,cut=0;
   if((tn_cc[a]&tn_cc[b]&0x7F)||(all&4)){vx_d3d_tn_pl[5]++;continue;} /* both ends outside one plane, or a position not finite */
   if(u){uint32_t pl;
    if(tn_cc[a]&0x7F)tn_fin(a);if(tn_cc[b]&0x7F)tn_fin(b); /* an end outside: finished too, the cut interpolates finished records */
    for(pl=0;pl<6&&!gone;pl++)if(u&bit[pl]){const float da=tn_pd(pl,a),db=tn_pd(pl,b);
     if(da<0&&db<0)gone=1;
     else if(da<0||db<0){if(tn_P>=TN_MAX){gone=1;break;}
      tn_lm=u; /* .718 */if(da<0)a=tn_lerp(c,a,b,tn_clip_fraction(pl,a,b,da,db),pl);else b=tn_lerp(c,a,b,tn_clip_fraction(pl,a,b,da,db),pl);cut=1;}}
    if(gone){vx_d3d_tn_pl[5]++;continue;}}
   else if(all){tn_fin(a);tn_fin(b);} /* the side planes left to the per-primitive path: its ends finished as they are */
   if(cut)vx_d3d_tn_pl[4]++;
   out[m*2]=(uint16_t)a;out[m*2+1]=(uint16_t)b;m++;vx_d3d_tn_pl[3]++;}}
 tn_lazy=0;tn_tok[0]=27;tn_tok[1]=(uint16_t)m;tn_tok[2]=0;
 *op=27;*n=m;*p=(const uint8_t*)(tn_tok+2);
 return 1;
}
/* .676: cull before lighting: back faces culled (D3DCULL_CW or CCW) and the switch on */
static int tn_pcull_on(const vx_d3d_context *c) {const uint32_t cm=c->state.rs[22];return cm==2||cm==3;} /* .682: ACCEL_D3D_PRECULL removed, always on (T681 GRID 6l: off, through .680's engine cull, 32% slower) */
/* .714: TN_OK's test on one record r (a pointer variable: each use reads it, no multiply): window x and y inside the clip or the guard
 * band, 0 <= z <= 1, 1/w positive and finite. A macro: Watcom called a static function for it (T714: driver T&L 1.5% slower) */
#define TN_OKR(r) (TN_GX(u32(r),tf(r),tc.bxl,tc.bxr,tn_gxl,tn_gxr)&&TN_GX(u32((r)+4),tf((r)+4),tc.byt,tc.byb,tn_gyt,tn_gyb)&&u32((r)+8)<=0x3F800000UL&&u32((r)+12)-1UL<0x7F7FFFFFUL)
static int tn_draw(vx_d3d_context *c,const vx_d3d_input *src,uint32_t *op,uint32_t *n,const uint8_t **p) {
 uint32_t o=*op,cnt=*n,base=0,lo=0xFFFFFFFFUL,hi=0,i,m=0,nv,need,mask,used=0,P0,k0=0,wide;uint16_t dlv=0;const uint8_t *data=*p;uint16_t *tri;int soa,pcull=0;
 if(o==1||o==2||o==15||o==16||o==17||o==27)return tn_draw_pl(c,src,op,n,p); /* .640 */
 if(o!=18&&o!=19&&o!=20&&o!=21&&o!=22&&o!=26&&o!=3){vx_d3d_tn[7]++;return 0;}
 if(o!=3){base=u16(data);data+=2;}
 tn_efon=o==3&&c->state.rs[8]!=3; /* .663 (audit 13) */
 /* .640: a draw too large for the scratch buffer is taken in pieces (tn_split, set by the caller on the answer 3: the command comes again
  * from its tn_k0-th triangle until tn_more is 0). Before, the answer was 0 and the caller left the draw out: one command of more than
  * 8192 triangles, or of fewer whose vertices and clip room passed 16384 places (3000 triangles of a list), drew nothing. A piece of
  * TN_PIECE triangles always fits (3 + 4 places a triangle). */
 if(!cnt){vx_d3d_tn[7]++;return 0;}
 if(tn_split){if(tn_k0>=cnt){vx_d3d_tn[7]++;return 0;}cnt-=tn_k0;if(cnt>TN_PIECE)cnt=TN_PIECE;k0=tn_k0;}
 else if(cnt>TN_TRIS)return 3;
 /* Stage the source-relative triples at the tail of the output token.
  * Reserve the space before it for all potential expansion. */
 tri=tn_tok+3+(TN_OUT-cnt)*3;
 if(o==26){const uint32_t ni=cnt*3;uint32_t a; /* .548 (.547 review): INDEXEDTRIANGLELIST2's indices are back to back: copied once
  * into the snapshot with the bounds in the same pass (tri_fill widened them to 32 bits in tri3, then they were read back and narrowed);
  * the same triples. vx_d3d_tn_snap: [0] tokens, [1] indices (QueryPhases 314-315) */
  {uint32_t mm[2];(void)a;(void)i;VXS_CMM16(data+k0*3*2,tri,ni,mm);lo=mm[0];hi=mm[1];} /* .645: eight at a time (was a loop of two compares an index) */
  vx_d3d_tn_snap[0]++;vx_d3d_tn_snap[1]+=ni;}
 else for(i=0;i<cnt;i+=VX_TRI_CHUNK){
  const uint32_t chunk=cnt-i<VX_TRI_CHUNK?cnt-i:VX_TRI_CHUNK;
  uint32_t j;
  tri_fill(o,data,0,k0+i,chunk);
  for(j=0;j<chunk*3;j++){
   const uint32_t idx=tri3[j];
   if(idx<lo)lo=idx;
   if(idx>hi)hi=idx;
   tri[i*3+j]=(uint16_t)idx;
  }
 }
 nv=hi-lo+1;hi+=base; /* the staged indices and lo are still relative to base */
 wide=nv>TN_MAX; /* .640: indices across more vertices than tn_ref and tn_map hold: each index takes a place of its own (before: the draw left out) */
 need=(nv<3*cnt&&!wide?nv:3*cnt)+cnt*4; /* the vertices used (at most 3 a triangle, at most the span), then each clipped triangle makes at most four */
 if(!src->vertices||!src->vertex_stride||hi>=src->vertex_bytes/src->vertex_stride){vx_d3d_tn[7]++;return 0;}
 if(need>TN_MAX){if(tn_split){vx_d3d_tn[7]++;return 0;}return 3;}
 {uint32_t was=tn_ostride;if(!tn_prepare(c,src,&tn_s)){vx_d3d_tn[7]++;return 0;}tn_c=c;
  if(tn_P&&tn_ostride!=was)return 2;} /* one record size in the buffer: a state command between draws changed the stages' sets */
 if(tn_P+need>TN_MAX)return 2;
 soa=tn_soa_prepare();
 if(!soa){uint32_t b;for(b=0;b<8;b++)if(tn_soa_why&(1UL<<b))vx_d3d_tn_why[b]++;vx_d3d_tn_why[8]++;vx_d3d_tn_why[9]+=nv<3*cnt?nv:3*cnt;} /* .652 */
 /* .540 (review): only the vertices the triangles use are transformed (stamped). .541: and they take the scratch buffer's next places in
  * the order the triangles first use them (tn_map: source index less lo to place), so a draw's vertices are dense there. .544: gathered
  * into the staging arrays (tn_sx tn_sy tn_sz, a chunk of TN_CH places), phase T over each chunk in groups of four */
 /* .642 (board, VXTLP WHY on .640: the driver's T&L 1.2 to 1.8 times the runtime's time in every case, the same ratio from 8 to 4,608
  * triangles a call, so the loss is per index and per vertex; Direct3D's own pipeline transforms the draw's vertex range in order and
  * looks at triangles only when a vertex is outside). A draw whose indices span no more vertices than it has indices (.642: than it has
  * triangles) takes its range in order: place = P0 + index - lo, no stamp and no map per index. The clip codes
  * of the range are ORed and ANDed: every vertex outside one plane, the draw is dropped; every vertex inside, the indices are passed on
  * with one subtraction each and the range is lit as a whole, with no pass over the triangles; else the triangles are looked at as
  * before. A sparse draw (a few triangles out of a large buffer) keeps the gather below, which transforms only what is used.
  * vx_d3d_tn_dense: [0] draws taken in order, [1] of them wholly inside, [2] wholly outside one plane, [3] their vertices. */
 /* .643 (board, GTA3 on .642: only 32,641 of 440,245 draws passed .642's test, no more vertices than triangles; its meshes carry about 1.5
  * vertices a triangle, each face's corners with their own normals and coordinates): the test is the range no longer than the index
  * list (nv <= 3 x cnt), so a mesh of unshared corners is taken in order too. The log's vertices a triangle (transformed vertices over
  * triangles) shows whether ranges with unused vertices are being transformed: 1.62 on .641, 1.52 on .642. */
 if(vx_d3d_tn_dense_on&&!wide&&nv<=cnt*3){uint32_t q,oo=0,oa=0x7F;const uint32_t st=src->vertex_stride;const uint8_t *v=src->vertices+(base+lo)*st;uint16_t dl;
  P0=tn_P;if(!P0)tn_slot_reset();
  {const uintptr_t ah=(uintptr_t)st*8U; /* .681: the prefetch eight vertices ahead */
   for(q=0;q<nv;q++,v+=st){const uint32_t pl=P0+q,ln=q&(TN_CH-1);tn_pf((uintptr_t)v+ah);tn_sp[pl]=v;tn_sx[ln]=tf(v);tn_sy[ln]=tf(v+4);tn_sz[ln]=tf(v+8);if(ln==TN_CH-1)tn_soa_pos(pl+1-TN_CH,TN_CH);}}
  if(nv&(TN_CH-1))tn_soa_pos(P0+(nv&~(TN_CH-1)),nv&(TN_CH-1));
  tn_vdone(P0,nv); /* .702 */
  used=nv;{uint32_t o4,a4;tn_orand(tn_cc+P0,nv,&o4,&a4);oo|=o4;oa&=a4;} /* .707: four places a word (tn_orand; .711: always, board T710 A-B-A-B:
   * 0.3-1.0% faster in every GRID scene than .706's byte loops) */
  if(tn_vany&&!oa){uint32_t va=15;for(q=0;q<nv&&va;q++)va&=tn_vc[P0+q];if(va)oa=0x80;} /* .698: every vertex beyond one side of the viewport (inside the guard band): dropped as before */
  if(tn_split){tn_k0+=cnt;tn_more=tn_k0<*n;vx_d3d_tn_split[1]++;}
  vx_d3d_tn[0]++;vx_d3d_tn[1]+=cnt;vx_d3d_tn[2]+=used;tn_P+=used;tn_lz0=P0;vx_d3d_tn_dense[0]++;vx_d3d_tn_dense[3]+=used;
  dl=(uint16_t)(P0-lo);
  if(oa){vx_d3d_tn_dense[2]++;vx_d3d_tn[4]+=cnt;m=0;goto tn_out;} /* every vertex outside one plane (or none finite) */
  /* .703 (T702, board B: GRID 6k, 30 tori with eight lights, CULL_NONE, 196.8 ms with the guard band against 186.9 without in four
   * alternating runs): a torus reaching past the viewport but inside the
   * band counts as wholly inside (its codes are the band's), so it was lit whole here, its off-screen vertices too; without the band
   * its triangles past the viewport were dropped before lighting. With a vertex past the viewport (tn_vany), the triangle pass below
   * drops what lies wholly beyond a side (the viewport codes) and lights only the vertices of the rest.
   * The board's T702 profile of 6k puts 26% of the T&L in the packets (indices, vertex words), and the VxD copies and checks every
   * word again, so a triangle wholly off the screen costs its packet words as well as its lighting. Only where it pays: an eighth of
   * the range or more past the viewport (the host, where a packet costs little, ran 3% more instructions through the pass than it
   * saved on a scene with a few grids across the edges, 0.5% fewer with half of them off the screen); vx_d3d_vcd[2] counts these draws. */
  if(!oo&&!(pcull=tn_pcull_on(c))&&!(tn_vany&&tn_voff*8>=nv&&++vx_d3d_vcd[2])){
   m=cnt;vx_d3d_tn_dense[1]++;memset(tn_ok+P0,0,nv); /* .677: no marks left from an earlier draw in these places (tn_ix would skip a culled draw's test for them) */
   /* .652 (a review of .651): tn_soa_pos cleared tn_done for every place of the range and no code here is 0x7F (oo is 0), and no
    * clipping ran: none is finished yet, so the list is the range in order, without the test */
   if(soa){for(q=0;q<nv;q++)tn_need[q]=(uint16_t)(P0+q);tn_soa_finish(tn_need,nv);}
   /* .645: the indices are not written again: the command handed on is the staged list where it lies, its base word the range's
    * place less its first index (the core adds the base to every index). Only a range placed below its first index (a base below 0)
    * is still rewritten. */
   if(tn_efon)for(q=0;q<cnt;q++)tn_tef[q]=(uint8_t)((u16(data+(k0+q)*8+6)>>8)&7); /* .663: the same order */
   if(P0>=lo){tri[-1]=(uint16_t)(P0-lo);tn_lazy=1;*op=26;*n=m;*p=(const uint8_t*)(tri-1);return 1;}
   {uint16_t *ix=tn_tok+3;const uint32_t ni=cnt*3;for(i=0;i<ni;i++)ix[i]=(uint16_t)(tri[i]+dl);}
   goto tn_out;}
  dlv=dl; /* .645: added as each triangle is read below (was a pass over the list) */
  goto tn_tris;
 }
 if(++tn_epoch==0){memset(tn_ref,0,sizeof(tn_ref));tn_epoch=1;}
 P0=tn_P;
 if(!P0)tn_slot_reset();
 for(i=0;i<cnt*3;i++){const uint32_t qq=tri[i]-lo;
  if(wide||tn_ref[qq]!=tn_epoch){const uint8_t *v=src->vertices+(base+tri[i])*src->vertex_stride;const uint32_t pl=P0+used,ln=used&(TN_CH-1);
   if(!wide){tn_ref[qq]=tn_epoch;tn_map[qq]=(uint16_t)pl;}tn_sp[pl]=v;tn_sx[ln]=tf(v);tn_sy[ln]=tf(v+4);tn_sz[ln]=tf(v+8);used++;
   if(ln==TN_CH-1)tn_soa_pos(pl+1-TN_CH,TN_CH);
   tri[i]=(uint16_t)pl;
  }
  else tri[i]=tn_map[qq];
 }
 if(tn_split){tn_k0+=cnt;tn_more=tn_k0<*n;vx_d3d_tn_split[1]++;}
 if(used&(TN_CH-1))tn_soa_pos(P0+(used&~(TN_CH-1)),used&(TN_CH-1));
 tn_vdone(P0,used); /* .702 */
 vx_d3d_tn[0]++;vx_d3d_tn[1]+=cnt;vx_d3d_tn[2]+=used;tn_P+=used;tn_lz0=P0;
tn_tris:
 /* .543: the side planes clipped when the room allows (each triangle clipped there makes at most 12 vertices and 7 triangles); else near
  * and far only, as before (the room for that was checked above), the rest to the per-triangle path */
 mask=0x7F;
 /* For small draws the worst-case reservation already covers every
  * triangle. Count crossings only when that bound will not fit. */
 if(tn_P+cnt*12>TN_MAX||cnt*7>TN_OUT){
  uint32_t nc=0;
  for(i=0;i<cnt;i++){const uint32_t x=(uint16_t)(tri[i*3]+dlv),y=(uint16_t)(tri[i*3+1]+dlv),z=(uint16_t)(tri[i*3+2]+dlv),u=tn_cc[x]|tn_cc[y]|tn_cc[z];
   if(u&&!(tn_cc[x]&tn_cc[y]&tn_cc[z]))nc++;}
  if(tn_P+nc*12>TN_MAX||cnt+nc*6>TN_OUT){mask=0x07;vx_d3d_tn_dfr[0]++;}}
 /* the triangles: kept whole (their places marked for phase L), dropped, or clipped (their vertices finished one by one first, as the
  * clipper interpolates finished records) */
 if(soa)memset(tn_nd+P0,0,used);
 pcull=tn_pcull_on(c);memset(tn_ok+P0,0,used);
 /* .675 (board, VXTLP 6K on .668: the one-vertex code's lighting, tn_light_color and tn_sqrt, 6% of the HAL with one directional and seven
  * point lights and specular lighting: the corners of the triangles to be clipped were lit one at a time, as the clipper interpolates
  * finished records): those corners are found first, with the same tests as the loop below, and lit by the group kernels before any is
  * clipped (tn_clip then finds them finished). Their records are the group path's, as the kept triangles' (within the last bit of a colour
  * byte of the one-vertex code's).
  * .676 (board, .675 A/B in GRID: 6h -15%, 6k -5%, 6i -3%, 6e -0.7%; with one directional light 6 +2.5% and 6j +0.9%, the pass costing
  * more than one-vertex lighting of a cheap light saves; HarborBench within 0.5%): only with two lights or more or specular lighting.
  * vx_d3d_tn_pre: [0] draws, [1] corners lit this way */
 if(soa&&(tnk.spec||tnk.nl+tnk.npl>=2)){ /* .682: ACCEL_D3D_PCLGATE removed, the gate always on (T681 GRID: off, 6b 3% slower) */uint32_t nc=0;
  for(i=0;i<cnt;i++){const uint32_t x=(uint16_t)(tri[i*3]+dlv),y=(uint16_t)(tri[i*3+1]+dlv),z=(uint16_t)(tri[i*3+2]+dlv),u=(tn_cc[x]|tn_cc[y]|tn_cc[z])&mask;
   if(!u||(tn_cc[x]&tn_cc[y]&tn_cc[z]&0x7F)||(u&4))continue;
   if(!tn_nd[x]){tn_nd[x]=1;tn_need[nc++]=(uint16_t)x;}
   if(!tn_nd[y]){tn_nd[y]=1;tn_need[nc++]=(uint16_t)y;}
   if(!tn_nd[z]){tn_nd[z]=1;tn_need[nc++]=(uint16_t)z;}}
  if(nc){uint32_t w=0;for(i=0;i<nc;i++)if(!tn_done[tn_need[i]])tn_need[w++]=tn_need[i];
   if(w){tn_soa_finish(tn_need,w);vx_d3d_tn_pre[0]++;vx_d3d_tn_pre[1]+=w;}}}
 {uint16_t *ix=tn_tok+3;uint32_t nneed=0;
  /* Each source triple is loaded before its output is written. Six
   * extra triples per crossing fit ahead of the unread tail. For the
   * near/far fallback, TN_OUT already reserves two extras per input. */
  for(i=0;i<cnt;i++){const uint32_t x=(uint16_t)(tri[i*3]+dlv),y=(uint16_t)(tri[i*3+1]+dlv),z=(uint16_t)(tri[i*3+2]+dlv),u=(tn_cc[x]|tn_cc[y]|tn_cc[z])&mask;
   if(tn_efon)tn_clip_ef=(u16(data+(k0+i)*8+6)>>8)&7; /* .663 */
   /* .676: culled before lighting (tn_pcull_on): a wholly inside triangle by the area of its records' window positions, as tn_ix tests
    * it, so its vertices are lit only if a kept triangle uses them; its places marked for tn_ix (tn_ok: tests done) */
   if(pcull&&!(tn_cc[x]|tn_cc[y]|tn_cc[z])){const float *f0=(const float*)(const void*)(tn_buf+x*tn_ostride),*f1=(const float*)(const void*)(tn_buf+y*tn_ostride),*f2=(const float*)(const void*)(tn_buf+z*tn_ostride);
    const double area=((double)f1[0]-f0[0])*((double)f2[1]-f0[1])-((double)f1[1]-f0[1])*((double)f2[0]-f0[0]);const uint32_t cm=c->state.rs[22];
    if(vx_area_cull(area,cm)){vx_d3d_tn_pcull[1]++;continue;}
    tn_ok[x]=1;tn_ok[y]=1;tn_ok[z]=1;} /* .682: ACCEL_D3D_IXSKIP removed, always on */
   if(!u&&tn_vany&&(tn_vc[x]&tn_vc[y]&tn_vc[z])){vx_d3d_tn[4]++;continue;} /* .698: wholly beyond one side of the viewport, inside the guard band: dropped as before */
   if(!u){if(m>=TN_OUT){vx_d3d_tn[7]++;break;}if(tn_efon)tn_tef[m]=(uint8_t)tn_clip_ef;ix[m*3]=(uint16_t)x;ix[m*3+1]=(uint16_t)y;ix[m*3+2]=(uint16_t)z;m++;
    if(soa){
     if(!tn_nd[x]){tn_nd[x]=1;tn_need[nneed++]=(uint16_t)x;}
     if(!tn_nd[y]){tn_nd[y]=1;tn_need[nneed++]=(uint16_t)y;}
     if(!tn_nd[z]){tn_nd[z]=1;tn_need[nneed++]=(uint16_t)z;}
    }}
   else if((tn_cc[x]&tn_cc[y]&tn_cc[z]&0x7F)||(u&4))vx_d3d_tn[4]++; /* .538: 4, a position not finite; .543: wholly outside any plane */
   else{vx_d3d_tn[3]++;m=tn_clip(c,x,y,z,ix+m*3,m,u);}}
  if(tn_P>P0+used)memset(tn_ok+P0+used,0,tn_P-(P0+used)); /* .676: the clipper's places: tested in tn_ix */
  if(pcull)vx_d3d_tn_pcull[0]++;
  /* phase L: the kept triangles' vertices not yet finished (a clipped edge's end may be) */
  if(soa){
   uint32_t w=0;
   for(i=0;i<nneed;i++)if(!tn_done[tn_need[i]])tn_need[w++]=tn_need[i];
   tn_soa_finish(tn_need,w);
  }
 }
tn_out:
 /* A fully rejected draw owns no records needed by DP2. */
 tn_lazy=m!=0;
 tn_tok[0]=26;tn_tok[1]=(uint16_t)m;tn_tok[2]=0;
 *op=26;*n=m;*p=(const uint8_t*)(tn_tok+2);
 return 1;
}
/* .541: every vertex of the draw finished (the paths that read the scratch records as they are: the per-triangle path, a draw not on
 * the indexed path) */
static void tn_fin_all(void) {uint32_t o;for(o=tn_lz0;o<tn_P;o++)tn_fin(o);tn_lazy=0;}
/* .541: the driver's T&L draw into the open indexed packet (the fused path: ix_list's work for the scratch buffer's triangles, without
 * ix_pass's span pass and without the source checks a vertex buffer needs). Each vertex is lit when it first enters a packet (tn_fin),
 * its words written there once (fast_vertex_pre_w, the same words as before); a vertex whose position is outside the guard band or
 * the depth range, or whose rhw is not positive and finite, stops it (return: that triangle and on go back to the caller's per-triangle
 * path, which clips it on the screen). The records are the T&L's own (finite coordinates, tn_vertex), so no vv_good. */
static uint32_t tn_ix_b(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint8_t *q,uint32_t ist,uint32_t base,uint32_t n,uint32_t *completed,uint32_t *rc);
static uint32_t tn_ix(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint8_t *q,uint32_t ist,uint32_t base,uint32_t n,uint32_t *completed,uint32_t *rc) {uint32_t o=TNP(6),r=tn_ix_b(c,in,sv,q,ist,base,n,completed,rc);TNP_BACK(o);return r;}
static uint32_t tn_ix_b(vx_d3d_context *c,const vx_d3d_input *in,const vx_d3d_services *sv,const uint8_t *q,uint32_t ist,uint32_t base,uint32_t n,uint32_t *completed,uint32_t *rc) {
 const uint8_t *tn_okp; /* .714: TN_OK's record */
 const uint8_t *const vb=in->vertices;const uint32_t st=in->vertex_stride,cull=c->state.rs[22];
 const int noarea=cull==1&&vx_d3d_ixpass;
 uint32_t k=0,drawn=0,culled=0,newv=0;int cur;
 /* Grouped lighting ran in tn_draw. Scalar attributes are finished when
  * a vertex first enters a packet. Plain packets can copy the record. */
 const uint32_t ten=tn_sets>1;
 /* .542: plain, the template takes the record's words as they are; otherwise (ZBIAS, snapping, another layout) fast_vertex_pre_w converts
  * them as before (.544 first copied the words in every case: ZBIAS lost) */
 const int two=tc.tex&&tc.units>1;
 const int plain=tc.lay.coff==16&&tc.lay.soff==20&&tc.prog>=0&&tc.tex&&tc.lay.t0==24&&tc.lay.n0>=2&&tc.zb==0&&!tri_snap&&
  (two?tn_sets==2&&tc.lay.t1==32&&tc.lay.n1>=2:tn_sets==1);
 *rc=0;
 cur=bq_on&&bq_tid==tc.id&&bq.pre.on==2&&ix_vb==vb&&ix_stride==st&&ix_fvf==in->fvf;
 if(cur&&tn_slot_g!=ix_g)tn_slot_reset();
 if(cur&&tn_gb)bq.bursts.b[0]|=VX3DN_IX_GUARD; /* .698: the packet may carry guard-band vertices (ix_open sets it on a new one below) */
 #define TN_OK(i) (tn_okp=tn_buf+(i)*tn_ostride,TN_OKR(tn_okp)) /* .714: the record's address made once (Watcom multiplied tn_ostride four times a test: board, T713 pursuit, tn_ix_b's hottest lines) */ /* (.680: defined before the block path) */
 /* .680 (PLAN_SPEED.md I, the first step of the kernels' direct output; ACCEL_D3D_TNFASTIX, a feature under work): a run of triangles
  * whose places span at most 3 a triangle and 32 more, every place of the span finished and inside the screen and the depth range (the
  * loop's TN_OK), goes in as a block: the span's records copied in place order to the packet's next slots, each index one addition
  * (place - lo + the first slot). No slot lookup, no per-triangle test: the loop below costs three lookups, three tests and the
  * vertex-by-vertex copies a triangle (6e: the packets were 36% of the T&L). Only where nothing is culled on the CPU (cull none, or
  * the engine's cull) and the template takes the record's words as they are (plain). A span that fails ends the block path; the loop
  * takes the rest as before. vx_d3d_tn_fastix_stat: [0] runs, [1] their triangles, [2] their vertices, [3] spans refused (sparse,
  * unfinished or outside) */
 while(vx_d3d_tn_fastix&&plain&&noarea&&k<n) {
  uint32_t lo,hi,m=0,j,rv,rt,nv,i;const uint8_t *p=q;uint16_t *ix;uint16_t d;
  rv=cur?VX3DN_IX_VERTICES-bq.vertex_count:VX3DN_IX_VERTICES;rt=cur?(VX3DN_MAX_INDICES-ix_ni)/3:VX3DN_MAX_INDICES/3;
  lo=hi=base+u16(p);
  for(j=k;j<n&&m<rt;j++,p+=ist){const uint32_t a0=base+u16(p),a1=base+u16(p+2),a2=base+u16(p+4);uint32_t l2=lo,h2=hi;
   if(a0<l2)l2=a0;if(a1<l2)l2=a1;if(a2<l2)l2=a2;if(a0>h2)h2=a0;if(a1>h2)h2=a1;if(a2>h2)h2=a2;
   if(h2-l2+1>rv)break;lo=l2;hi=h2;m++;}
  if(!m){if(cur&&ix_ni){const uint32_t g=ix_g;if(ix_open(c,in,sv,VX3DN_IX_VERTICES+1,1,completed,rc))break;if(ix_g!=g||tn_slot_g!=ix_g)tn_slot_reset();cur=1;if(tn_gb)bq.bursts.b[0]|=VX3DN_IX_GUARD;continue;}break;} /* no room: a new packet */
  nv=hi-lo+1;
  if(nv>3*m+32){vx_d3d_tn_fastix_stat[3]++;break;}
  for(i=lo;i<=hi;i++)if(!tn_done[i]||!TN_OK(i))break;
  if(i<=hi){vx_d3d_tn_fastix_stat[3]++;break;}
  if(!cur){const uint32_t g=ix_g;if(ix_open(c,in,sv,nv,m,completed,rc))break;if(ix_g!=g||tn_slot_g!=ix_g)tn_slot_reset();cur=1;if(tn_gb)bq.bursts.b[0]|=VX3DN_IX_GUARD;
   if(bq.vertex_count+nv>VX3DN_IX_VERTICES||ix_ni+3*m>VX3DN_MAX_INDICES)break;} /* (ix_open leaves that room) */
  {const uint32_t b0=bq.vertex_count,v8=(bq.bursts.b[0]&VX3DN_IX_V8)!=0;
   if(v8&&tn_ostride==32)memcpy(IXW(b0),tn_buf+lo*32,nv*32);
   else if(!v8&&ten&&tn_ostride==40)memcpy(IXW(b0),tn_buf+lo*40,nv*40);
   else for(i=0;i<nv;i++){const uint32_t *r_=(const uint32_t*)(const void*)(tn_buf+(lo+i)*tn_ostride);uint32_t *w_=IXW(b0+i);
    w_[0]=r_[0];w_[1]=r_[1];w_[2]=r_[2];w_[3]=r_[3];w_[4]=r_[4];w_[5]=r_[5];w_[6]=r_[6];w_[7]=r_[7];if(!v8){w_[8]=r_[6+2*ten];w_[9]=r_[7+2*ten];}}
   for(i=0;i<nv;i++)tn_slot[lo+i]=(uint16_t)(tn_slot_tag|(b0+i));
   if(hi>=tn_slot_hi)tn_slot_hi=hi+1;
   bq.vertex_count+=nv;newv+=nv;d=(uint16_t)(b0+base-lo);
   ix=VX3DN_IDX(&bq)+ix_ni;
   for(j=0,p=q;j<m;j++,p+=ist,ix+=3){ix[0]=(uint16_t)(u16(p)+d);ix[1]=(uint16_t)(u16(p+2)+d);ix[2]=(uint16_t)(u16(p+4)+d);}
   ix_ni+=3*m;bq.bursts.b[0]=(bq.bursts.b[0]&0xFFFF0000UL)|ix_ni;drawn+=m;k+=m;q+=m*ist;
   vx_d3d_tn_fastix_stat[0]++;vx_d3d_tn_fastix_stat[1]+=m;vx_d3d_tn_fastix_stat[2]+=nv;}
 }
 if(!*rc) /* (review of .680: a packet the block path could not open ends the call here, as the loop's own ix_open failure does) */
 for(;k<n;k++,q+=ist) {
  const uint32_t i0=base+u16(q),i1=base+u16(q+2),i2=base+u16(q+4);
  uint32_t l0=IX_NONE,l1=IX_NONE,l2=IX_NONE;uint16_t *ix;
  if(cur){
   const uint32_t a=tn_slot[i0]^tn_slot_tag,b=tn_slot[i1]^tn_slot_tag,d=tn_slot[i2]^tn_slot_tag;
   if(a<VX3DN_IX_VERTICES)l0=a;
   if(b<VX3DN_IX_VERTICES)l1=b;
   if(d<VX3DN_IX_VERTICES)l2=d;
  }
  /* .543's test, on the record (the words of a kept triangle are inside the viewport and the depth range when the side planes were
   * clipped; not when the room allowed near and far only, and a vertex made by clipping can have a 1/w of 0 or not finite): such a
   * triangle and the rest go back to the caller's per-triangle path, which clips them on the screen (tested before a packet is opened:
   * the per-triangle path then adds to the open one) */
  if((l0==IX_NONE&&!TN_OK(i0))||(l1==IX_NONE&&!TN_OK(i1))||(l2==IX_NONE&&!TN_OK(i2)))break; /* .677: always (.676 skipped it for a triangle culled before lighting: its window position and the clip codes are two calculations that need not agree at an edge) */
  if(tn_ok[i0]&tn_ok[i1]&tn_ok[i2])vx_d3d_tn_pcull[2]++; /* .676: a triangle culled before lighting and wholly inside: its back-face test is done (tn_draw) */
  else{
  if(!noarea){double area;const vx_f32a *const f0=(const vx_f32a *)(vb+i0*st),*const f1=(const vx_f32a *)(vb+i1*st),*const f2=(const vx_f32a *)(vb+i2*st);
   area=((double)f1[0]-f0[0])*((double)f2[1]-f0[1])-((double)f1[1]-f0[1])*((double)f2[0]-f0[0]);
   if(vx_area_cull(area,cull)){culled++;continue;}}
  else if(i0==i1||i1==i2||i0==i2){culled++;continue;}}
#undef TN_OK
  if(!cur||bq.vertex_count+3>VX3DN_IX_VERTICES||ix_ni+3>VX3DN_MAX_INDICES) {
   const uint32_t g=ix_g;
   if(ix_open(c,in,sv,3,1,completed,rc))break;
   if(tn_gb)bq.bursts.b[0]|=VX3DN_IX_GUARD; /* .698 */
   if(ix_g!=g||tn_slot_g!=ix_g){tn_slot_reset();l0=l1=l2=IX_NONE;}
   cur=1;}
#define TN_PUT(i,l) do{const uint8_t *p_=tn_buf+(i)*tn_ostride;const uint32_t *r_=(const uint32_t*)(const void*)p_;uint32_t *w_;l=bq.vertex_count++;tn_slot[i]=(uint16_t)(tn_slot_tag|l);if((i)>=tn_slot_hi)tn_slot_hi=(i)+1;if(!tn_done[i])tn_attributes(i);w_=IXW(l); \
 if(plain){w_[0]=r_[0];w_[1]=r_[1];w_[2]=r_[2];w_[3]=r_[3];w_[4]=r_[4];w_[5]=r_[5];w_[6]=r_[6];w_[7]=r_[7];if(!(bq.bursts.b[0]&VX3DN_IX_V8)){w_[8]=r_[6+2*ten];w_[9]=r_[7+2*ten];}} \
 else fast_vertex_pre_w(w_,p_,0xFFFFFFFFUL,p_);newv++;}while(0)
  if(l0==IX_NONE)TN_PUT(i0,l0);
  if(l1==IX_NONE)TN_PUT(i1,l1);
  if(l2==IX_NONE)TN_PUT(i2,l2);
#undef TN_PUT
  ix=VX3DN_IDX(&bq)+ix_ni;ix[0]=(uint16_t)l0;ix[1]=(uint16_t)l1;ix[2]=(uint16_t)l2;
  ix_ni+=3;bq.bursts.b[0]=(bq.bursts.b[0]&0xFFFF0000UL)|ix_ni;drawn++;
 }
 vx_d3d_batched+=drawn;vx_d3d_ix[0]+=drawn;vx_d3d_ix[1]+=newv;vx_d3d_tk[8]+=k;vx_d3d_tk[9]+=culled;
 if(vx_d3d_geo_on){vx_d3d_geo[6]+=drawn;vx_d3d_geo[3]+=culled;}
 return k;
}
