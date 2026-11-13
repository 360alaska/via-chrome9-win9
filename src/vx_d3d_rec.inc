/* .517: the DrawPrimitives2 recorder (included by vx_d3d_hal.c before dp2_impl). VXD3DMON REC=n (VX_D3D_Record) asks for n frames; the
 * game's process starts at its next flip and records every DrawPrimitives2 call of the first context it sees, at the core's boundary:
 * the call's input and the context's state before it, then every answer the HAL's services give the core during it (texture handles
 * resolved, vertex buffers bound with their bytes, unknown tokens skipped, colour keys, mip chains, cube faces, render targets, the
 * submits' results), and a mark at each flip. After n flips the recording goes to C:\VXD3D.REC. The host replays it through the same core
 * and emitter (tests/vx_replay.c): the HAL's and the emitter's work for GTA3's real frames, without the board.
 * The file: a header (VXR_MAGIC, version, the context's, state's and surface's sizes, the HAL's switches, the whole context), then
 * records of [type][payload bytes][payload]. Vertex buffers are kept once by address, length and contents (a blob), referred to by number.
 * VX_D3D_Record(n): n > 0 asks for n frames (at most 64), 0 cancels a request not started, 0xFFFFFFFF reads; returns state | frames << 8 |
 * KB << 16 (state: 0 idle, 1 asked, 2 recording, 3 written, 4 written but cut short (the buffer full), 5 no memory, 6 the file not
 * written). The globals are in the shared data (VXD3DMON asks from its own process); the buffer is the recording process's own. */
#define VXR_MAGIC 0x31525856UL /* "VXR1" */
#define VXR_VERSION 1
enum {VXR_CALL=1,VXR_BLOB=2,VXR_SVC=3,VXR_FRAME=4,VXR_END=5};
enum {VXS_RESOLVE=1,VXS_BIND=2,VXS_UNKNOWN=3,VXS_COLORKEY=4,VXS_MIPS=5,VXS_FACES=6,VXS_TARGET=7,VXS_TARGETDONE=8,VXS_SUBMIT=9,VXS_SUBMITN=10};
#define VXR_BYTES (40UL<<20)
#define VXR_BLOBS 1024
static volatile uint32_t vxr_req,vxr_state,vxr_pid,vxr_frames,vxr_kb,vxr_want,vxr_cancel;
static uint8_t *vxr_buf;static uint32_t vxr_n,vxr_full,vxr_ctx,vxr_nblob;
static uint32_t vxr_bptr[VXR_BLOBS],vxr_blen[VXR_BLOBS],vxr_bsum[VXR_BLOBS];
static vx_d3d_services vxr_real;static const uint8_t *vxr_cmd0;
/* .518 (audit): a recording whose process ended (it exited before its last flip, another process keeping the HAL loaded) is given up:
 * at that process's detach (vxr_detach), or at the next request when its owner is gone; 0 while it records asks its process to drop it
 * (at its next call or flip) */
DWORD WINAPI VX_D3D_Record(DWORD n) {
 if(vxr_state==2&&vxr_pid!=GetCurrentProcessId()&&!owner_alive(vxr_pid)){vxr_buf=0;vxr_state=0;vxr_req=0;} /* its memory went with it */
 if(n==0xFFFFFFFFUL)return vxr_state|(vxr_frames<<8)|(vxr_kb<<16);
 if(!n){if(vxr_state==1)vxr_state=0;else if(vxr_state==2)vxr_cancel=1;vxr_req=0;return vxr_state;}
 if(vxr_state==1||vxr_state==2)return vxr_state;
 vxr_cancel=0;
 vxr_want=n>64?64:n;vxr_frames=0;vxr_kb=0;vxr_req=1;vxr_state=1;return vxr_state;
}
static int vxr_room(uint32_t b) {if(vxr_full||vxr_n+b+64>VXR_BYTES){vxr_full=1;return 0;}return 1;}
static void vxr_put(const void *p,uint32_t b) {if(!vxr_room(b))return;memcpy(vxr_buf+vxr_n,p,b);vxr_n+=b;}
static void vxr_u32(uint32_t v) {vxr_put(&v,4);}
static void vxr_head(uint32_t type,uint32_t bytes) {if(!vxr_room(8+bytes))return;vxr_u32(type);vxr_u32(bytes);} /* a record that does not fit: none of it (vxr_full drops its payload too) */
static uint32_t vxr_sum(const uint8_t *p,uint32_t b) { /* FNV-1a over the dwords, then the tail */
 uint32_t h=2166136261UL,k;const uint32_t *w=(const uint32_t*)p;
 for(k=0;k<b/4;k++)h=(h^w[k])*16777619UL;for(k=b&~3UL;k<b;k++)h=(h^p[k])*16777619UL;return h;
}
/* a vertex buffer's bytes: the blob's number (written once), 0xFFFFFFFF none */
static uint32_t vxr_blob(const uint8_t *p,uint32_t b) {
 uint32_t k,s;
 if(!p||!b||IsBadReadPtr(p,b))return 0xFFFFFFFFUL;
 s=vxr_sum(p,b);
 for(k=0;k<vxr_nblob;k++)if(vxr_bptr[k]==(uint32_t)p&&vxr_blen[k]==b&&vxr_bsum[k]==s)return k;
 if(vxr_nblob>=VXR_BLOBS||!vxr_room(b+16)){vxr_full=1;return 0xFFFFFFFFUL;}
 k=vxr_nblob++;vxr_bptr[k]=(uint32_t)p;vxr_blen[k]=b;vxr_bsum[k]=s;
 vxr_head(VXR_BLOB,8+((b+3)&~3UL));vxr_u32(k);vxr_u32(b);vxr_put(p,b);if(b&3)vxr_put("\0\0\0",4-(b&3));
 return k;
}
static void vxr_svc(uint32_t type,uint32_t n,const uint32_t *w) {uint32_t k;vxr_head(VXR_SVC,4+n*4);vxr_u32(type);for(k=0;k<n;k++)vxr_u32(w[k]);}
static int vxr_resolve(void *x,uint32_t h,vx3dn_surface *s) {int r=vxr_real.resolve(x,h,s);uint32_t w[2+sizeof(vx3dn_surface)/4];w[0]=h;w[1]=(uint32_t)r;memcpy(w+2,s,sizeof(*s));vxr_svc(VXS_RESOLVE,2+sizeof(vx3dn_surface)/4,w);return r;}
static int vxr_target(void *x,uint32_t h,vx3dn_surface *s) {int r=vxr_real.target(x,h,s);uint32_t w[2+sizeof(vx3dn_surface)/4];w[0]=h;w[1]=(uint32_t)r;memcpy(w+2,s,sizeof(*s));vxr_svc(VXS_TARGET,2+sizeof(vx3dn_surface)/4,w);return r;}
static void vxr_target_done(void *x) {vxr_real.target_done(x);vxr_svc(VXS_TARGETDONE,0,0);}
static uint32_t vxr_bind(void *x,vx_d3d_input *in) {
 uint32_t r=vxr_real.bind_vertices(x,in),w[8];
 w[0]=r;w[1]=vxr_blob(in->vertices,in->vertex_bytes);w[2]=in->vertex_bytes;w[3]=in->vertex_count;w[4]=in->vertex_stride;w[5]=in->fvf;w[6]=in->flags;w[7]=(uint32_t)in->vertices;
 vxr_svc(VXS_BIND,8,w);return r;
}
static uint32_t vxr_unknown(void *x,const uint8_t *cmd,const uint8_t **next) {
 uint32_t r=vxr_real.unknown(x,cmd,next),w[3];w[0]=r;w[1]=(uint32_t)(cmd-vxr_cmd0);w[2]=next&&*next?(uint32_t)(*next-cmd):0xFFFFFFFFUL;vxr_svc(VXS_UNKNOWN,3,w);return r;
}
static int vxr_colorkey(void *x,uint32_t h,uint32_t *v) {int r=vxr_real.colorkey(x,h,v);uint32_t w[3];w[0]=h;w[1]=(uint32_t)r;w[2]=v?*v:0;vxr_svc(VXS_COLORKEY,3,w);return r;}
static uint32_t vxr_mips(void *x,uint32_t h,uint32_t *o,uint32_t m) {
 uint32_t r=vxr_real.mips(x,h,o,m),w[3+16],k,n=m>16?16:m;w[0]=h;w[1]=m;w[2]=r;for(k=0;k<16;k++)w[3+k]=o&&k<n?o[k]:0;vxr_svc(VXS_MIPS,19,w);return r;
}
static uint32_t vxr_faces(void *x,uint32_t h,uint32_t *o) {uint32_t r=vxr_real.faces(x,h,o),w[8],k;w[0]=h;w[1]=r;for(k=0;k<6;k++)w[2+k]=o?o[k]:0;vxr_svc(VXS_FACES,8,w);return r;}
static uint32_t vxr_submit(void *x,const vx3dn_packet *p) {uint32_t r=vxr_real.submit(x,p);vxr_svc(VXS_SUBMIT,1,&r);return r;}
static uint32_t vxr_submit_n(void *x,const vx3dn_packet *p,uint32_t n,uint32_t *done) {uint32_t r=vxr_real.submit_n(x,p,n,done),w[3];w[0]=n;w[1]=r;w[2]=done?*done:0;vxr_svc(VXS_SUBMITN,3,w);return r;}
/* the switches the core reads, for the replay to set the same */
static void vxr_switches(uint32_t *w) {
 w[0]=vx_d3d_feat;w[1]=vx_d3d_prepack;w[2]=0 /* .524: PAIRLIST removed */;w[3]=0;w[4]=0;w[5]=0 /* .527: VRECIPE, FASTMATCH, FASTVV removed */;w[6]=vx_d3d_indexed;
 w[7]=0 /* .529: HWCULL removed */;w[8]=vx_d3d_ixpass;w[9]=vx_d3d_texhint;w[10]=vx_d3d_texruns /* .528: TEXRUNS (.527: PFVV2, removed) */;w[11]=vx_d3d_rttex;w[12]=vx_d3d_mipcap;{extern uint32_t vx_d3d_ckey_on;w[13]=vx_d3d_ckey_on;}w[14]=vx_d3d_tnl; /* .535: the T&L probe */w[15]=0;
}
static void vxr_begin(context_slot *c) {
 uint32_t w[16];
 vxr_buf=(uint8_t*)VirtualAlloc(0,VXR_BYTES,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
 if(!vxr_buf){vxr_state=5;vxr_req=0;return;}
 vxr_n=0;vxr_full=0;vxr_nblob=0;vxr_ctx=c->handle;vxr_pid=GetCurrentProcessId();vxr_frames=0;
 vxr_u32(VXR_MAGIC);vxr_u32(VXR_VERSION);vxr_u32(sizeof(vx_d3d_context));vxr_u32(sizeof(vx_d3d_state));vxr_u32(sizeof(vx3dn_surface));
 vxr_switches(w);vxr_put(w,sizeof(w));vxr_put(&c->core,sizeof(c->core));
 {extern uint32_t vx_d3d_tnl_get(const vx_d3d_context *,void *,uint32_t);static uint8_t tb[8192];uint32_t n=vx_d3d_tnl_get(&c->core,tb,sizeof(tb)); /* .536: the T&L state (VXR_TNL, 6) */
  if(n){vxr_head(6,n);vxr_put(tb,n);}}
 vxr_state=2;vxr_req=0;
}
static void vxr_finish(uint32_t st) {
 HANDLE f;DWORD got=0;
 vxr_head(VXR_END,0);
 f=CreateFileA("C:\\VXD3D.REC",GENERIC_WRITE,0,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
 if(f==INVALID_HANDLE_VALUE||!WriteFile(f,vxr_buf,vxr_n,&got,0)||got!=vxr_n)st=6;
 if(f!=INVALID_HANDLE_VALUE)CloseHandle(f);
 vxr_kb=(vxr_n+1023)>>10;VirtualFree(vxr_buf,0,MEM_RELEASE);vxr_buf=0;vxr_state=st;
}
static int vxr_dropped(void) { /* the recording process: a cancel asked for, or its detach: the buffer freed, nothing written */
 if(vxr_state!=2||vxr_pid!=GetCurrentProcessId())return 0;
 if(vxr_buf){VirtualFree(vxr_buf,0,MEM_RELEASE);vxr_buf=0;}vxr_state=0;vxr_cancel=0;vxr_req=0;return 1;
}
void vxr_detach(void) {(void)vxr_dropped();}
/* at a flip (vx_ddraw.c VXFlip): a recording asked for begins at the next DrawPrimitives2; one going counts the frame */
void VX_rec_flip(void) {
 if(vxr_cancel&&vxr_dropped())return;
 if(vxr_state==1&&vxr_req)vxr_req=2; /* armed: the next DrawPrimitives2 call begins it, at a frame's start */
 else if(vxr_state==2&&vxr_pid==GetCurrentProcessId()&&vxr_buf){vxr_head(VXR_FRAME,0);vxr_frames++;if(vxr_full||vxr_frames>=vxr_want)vxr_finish(vxr_full?4:3);}
}
/* a frame's end marked by a program that does not flip (VXCULL's windowed device: its Presents are Blts) */
DWORD WINAPI VX_D3D_RecordFrame(void) {VX_rec_flip();return vxr_state|(vxr_frames<<8);}
/* the call's record, and the services wrapped (returns 1 when this call is recorded) */
static int vxr_call(context_slot *c,const vx_d3d_input *in,vx_d3d_services *sv) {
 uint32_t w[8];
 if(vxr_state==1&&vxr_req==2)vxr_begin(c);
 if(vxr_cancel&&vxr_dropped())return 0;
 if(vxr_state!=2||!vxr_buf||vxr_pid!=GetCurrentProcessId()||c->handle!=vxr_ctx)return 0;
 vxr_cmd0=in->commands;
 vxr_head(VXR_CALL,32+16+2*sizeof(vx3dn_surface)+sizeof(vx_d3d_state)+8+((in->command_bytes+3)&~3UL));
 w[0]=in->flags;w[1]=in->fvf;w[2]=in->vertex_stride;w[3]=in->command_bytes;w[4]=in->render_state_count;w[5]=vxr_blob(in->vertices,in->vertex_bytes);w[6]=in->vertex_bytes;w[7]=in->vertex_count;vxr_put(w,32);
 w[0]=c->core.generation;w[1]=c->core.lost;w[2]=c->core.recording;w[3]=0;vxr_put(w,16);
 vxr_put(&c->core.target,sizeof(vx3dn_surface));vxr_put(&c->core.depth,sizeof(vx3dn_surface));vxr_put(&c->core.state,sizeof(vx_d3d_state));
 w[0]=c->core.target_handle;w[1]=c->core.depth_handle;vxr_put(w,8);
 vxr_put(in->commands,in->command_bytes);if(in->command_bytes&3)vxr_put("\0\0\0",4-(in->command_bytes&3));
 vxr_real=*sv;
 if(sv->resolve)sv->resolve=vxr_resolve;if(sv->bind_vertices)sv->bind_vertices=vxr_bind;if(sv->unknown)sv->unknown=vxr_unknown;
 if(sv->colorkey)sv->colorkey=vxr_colorkey;if(sv->mips)sv->mips=vxr_mips;if(sv->faces)sv->faces=vxr_faces;
 if(sv->target)sv->target=vxr_target;if(sv->target_done)sv->target_done=vxr_target_done;if(sv->submit)sv->submit=vxr_submit;if(sv->submit_n)sv->submit_n=vxr_submit_n;
 return 1;
}
