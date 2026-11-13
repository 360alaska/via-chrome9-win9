/* VXGL host test (v10.239): the ICD's Mesa side (vxgl_mesa.c, vxgl_state.c, vxgl_tris.c) and the portable core built on Linux, with
 * video memory as an array, DirectDraw replaced by fixed surfaces in it and the VxD replaced by a model of the engine: a Direct3D-rule
 * rasterizer of the draw and clear packets the core sends (pixel centres on integers, top-left fill, Gouraud colours, vertex fog from
 * the specular alpha, D3DCMP depth/alpha/stencil, D3DBLEND factors). Each scene of src/vx_gl_scenes.h is drawn twice: through the
 * engine path, and with every primitive through Mesa's software rasterizer (the ICD's fallback, writing the same memory through the
 * span functions); the two images are compared. This checks the OpenGL -> core translation (coordinates, y flip, culling, depth
 * scale, colours, fog, blending, stencil, scissor), not the engine itself (the core's packets were proven on the board with D3D). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <sys/mman.h> /* v10.319: a guard page (audit M1 check) */
#include "../vxgl.h"
#include "context.h"
#include "macros.h"
#include "GL/gl.h"
#include "../vxgl_dxtn.h" /* v10.307: the sampler's DXT decode */

#define W 256
#define H 256
static uint8_t vram[0x4000000]; /* v10.342: 64 MB (GLREPLAY's Quake 2 traces) */
static unsigned host_w=W,host_h=H;static uint32_t host_tex_base=0x600000; /* v10.342: GLREPLAY's window and texture area */
HANDLE vxgl_vxd=INVALID_HANDLE_VALUE;LPDIRECTDRAW7 vxgl_dd;DWORD vxgl_vram_linear,vxgl_vram_size=sizeof(vram);int vxgl_hw_enabled=1;
int vxgl_sse_ok=0;void vxgl_sse_install(void){} /* v10.359: the SSE kernels are Watcom inline assembly: run by tests/sse_emu.py on the built DLL */
static int verbose,packets,draw_packets,clear_packets,host_cbits=32,host_zbits=24;
static vxgl_fb thefb;
static vxgl_ctx *cur;

double vxgl_prof_us[16];unsigned long vxgl_prof_n[16];double vxgl_prof_texels[2];unsigned long vxgl_cycles(void){return 0;}int vxgl_tsc_on;double vxgl_tsc[8];unsigned long vxgl_tsc_n[8];int vxgl_prof_on(void){return 0;}void vxgl_prof_swap(void){}
void vxgl_log(const char *fmt,...) {va_list a;if(!verbose)return;va_start(a,fmt);vprintf(fmt,a);va_end(a);}
BOOL GetClientRect(HWND w,RECT *r) {(void)w;r->left=0;r->top=0;r->right=(LONG)host_w;r->bottom=(LONG)host_h;return 1;}
static DWORD host_tid=1; /* v10.259: the calling thread as VXGL sees it */
DWORD GetCurrentThreadId(void) {return host_tid;}
DWORD GetCurrentProcessId(void) {return 0x1234;} /* v10.322 */
DWORD GetTickCount(void) {return 0;}
void SetLastError(DWORD e) {(void)e;}
BOOL DeviceIoControl(HANDLE h,DWORD c,void *i,DWORD n,void *o,DWORD z,DWORD *g,void *ov) {(void)h;(void)c;(void)i;(void)n;(void)o;(void)z;(void)g;(void)ov;return 0;}
int vxgl_pf_info(int pf,int *db,int *zb,int *sb,int *cb) {(void)pf;*db=1;*zb=host_zbits;*sb=host_zbits==24?8:0;*cb=host_cbits;return 1;}
vxgl_fb *vxgl_fb_for(HDC h) {(void)h;return &thefb;}
static uint32_t host_gen; /* v10.288: the VxD's generation (a mode change moves it on) */
static int host_remake;   /* v10.288: the next validate makes the surfaces again, at the same size and offsets (after a mode change) */
int vxgl_fb_validate(vxgl_ctx *c,vxgl_fb *fb) {
 (void)c;
 if(fb->w==host_w&&fb->h==host_h&&!host_remake)return 1;
 host_remake=0;fb->w=host_w;fb->h=host_h;fb->made++;
 fb->sback.offset=0x100000;fb->sback.pitch=host_w*(host_cbits/8);fb->sback.width=host_w;fb->sback.height=host_h;fb->sback.format=host_cbits==16?VX3DN_RGB565:VX3DN_XRGB8888;
 fb->sdepth.offset=host_w>W?0x400000:0x300000;fb->sdepth.pitch=host_w*(host_zbits==16?2:4);fb->sdepth.width=host_w;fb->sdepth.height=host_h;fb->sdepth.format=host_zbits==16?VX3DN_D16:VX3DN_D24S8;
 fb->pback=vram+fb->sback.offset;fb->pdepth=vram+fb->sdepth.offset;fb->back=(LPDIRECTDRAWSURFACE7)1;
 if(host_zbits==24){fb->soff=VX3DN_SPLANE_OFF(&fb->sdepth);fb->spitch=VX3DN_SPLANE_PITCH(host_w);fb->pstencil=vram+fb->soff;} /* v10.245: the stencil plane */
 return 1;
}
void vxgl_fb_destroy(vxgl_fb *fb) {(void)fb;} /* static */
int host_map_fail; /* v10.329: the next n video memory mappings fail (the review's injection) */
static vxgl_texmem *host_mapped[32]; /* v10.326: the renderbuffer mappings (vxgl_fbo.c frb_cpu) not yet let go */
static int host_lock_depth;static unsigned long host_unlock_outside; /* v10.326: a renderbuffer's mapping let go outside the engine lock */
void vxgl_engine_lock(void) {host_lock_depth++;}
void vxgl_engine_unlock(void) {host_lock_depth--;}
static uint32_t host_occl; /* v10.315: the model's occlusion count: pixels past the Z and stencil tests (VX3DN_query) */
unsigned long host_queries;
/* v10.322: the VxD's count, one owner's at a time (VX3DN_query: 0 begin, BUSY for another owner; 1 end; 2 put aside; 3 taken back; 4 given up) */
static uint32_t hq_owner,hq_acc,hq_aside;
int vxgl_vxd_query(uint32_t op,uint32_t owner,uint32_t *count) {
 *count=0;if(op>4||!owner)return 0;vxgl_batch_flush();host_queries++;
 if(op==0){if(hq_owner&&hq_owner!=owner)return 0;hq_owner=owner;host_occl=0;hq_acc=0;hq_aside=0;return 1;}
 if(hq_owner!=owner)return op!=1;
 if(op==1){*count=hq_acc+(hq_aside?0:host_occl);hq_owner=0;return 1;}
 if(op==2){if(!hq_aside){hq_acc+=host_occl;hq_aside=1;}return 1;}
 if(op==3){if(hq_aside){host_occl=0;hq_aside=0;}return 1;}
 hq_owner=0;return 1;
}
int vxgl_vxd_info(vx3dn_info *i) {memset(i,0,sizeof(*i));i->generation=host_gen;return 0;}
unsigned long host_z_written;
static int q2_begin(int);static void q2_swap(void);static void q2_end(void); /* v10.296: swrast's depth writes reported to the VxD (Coarse Z) */
int host_z_fail; /* v10.301: notices that fail (the review's fault injection) */
int vxgl_z_written(vxgl_fb *fb) {(void)fb;if(host_z_fail){host_z_fail--;return 0;}host_z_written++;return 1;}
int vxgl_z_written_at(const vx3dn_surface *d) {(void)d;host_z_written++;return 1;} /* v10.321 */
/* v10.267: the ICD's kept locks, modelled: an unlock at the end of swrast's batch keeps them (hold), the next lock reuses them, settle
 * lets them go. host_real_locks counts the locks DirectDraw would see; the core's submit checks nothing is held (host_held_submits) */
static unsigned long host_real_locks,host_held_submits;
unsigned long host_engine_waits;static uint32_t host_call,host_done_call;
int vxgl_engine_wait(void) {vxgl_batch_flush();host_engine_waits++;host_done_call=host_call;vxgl_seq_idle();return 1;}
int vxgl_fb_lock(vxgl_fb *fb) {if(fb->locked)return fb->locked++,1;if(vxgl_batch_flush()&&fb->hold)fb->hold=0;fb->locked++;if(fb->hold){fb->hold=0;return 1;}host_real_locks++;return 1;} /* v10.275: as the ICD's: the queue first */
void vxgl_fb_unlock(vxgl_fb *fb) {if(fb->locked&&!--fb->locked)fb->hold=1;}
void vxgl_fb_settle(vxgl_fb *fb) {vxgl_batch_flush();if(fb&&!fb->locked)fb->hold=0;}
static void fb_settle_only(vxgl_fb *fb) {if(fb&&!fb->locked)fb->hold=0;}
int vxgl_swap_blit(vxgl_fb *fb) {(void)fb;return 1;}
/* v10.268: the engine's texture copies in this model's video memory, 0x600000 up (the space is used again when none is left alive).
 * v10.287: a freed copy's memory is overwritten (another surface there: a draw that still samples it shows magenta); host_tex_fail: no
 * memory for a copy (host_tex_allocs counts the attempts); host_tex_lose: the live copies lost (a mode change; surf[1] holds the losses a
 * copy has seen), and a restore puts a copy elsewhere, its old place overwritten */
static uint32_t host_tex_top=0x600000,host_tex_live,host_tex_fail,host_tex_allocs,host_tex_lose;
unsigned long vxgl_texmem_freed;
/* v10.307: DXT copies as the HAL keeps them: block rows (8 bytes a block for DXT1, 16 for DXT3 / DXT5) at least 32 bytes apart */
static int host_dxt(uint32_t f) {return f>=VX3DN_TEX_DXT1&&f<=VX3DN_TEX_DXT5;}
static uint32_t host_packed(uint32_t f,uint32_t w) {return host_dxt(f)?((w+3)/4)*(f==VX3DN_TEX_DXT1?8U:16U):w*(f==VX3DN_TEX_RGB565?2U:4U);} /* v10.330 (review #31): blocks rounded up (6 wide is 2) */
static uint32_t host_rows(uint32_t f,uint32_t h) {return host_dxt(f)?(h+3)/4:h;}
static int host_tex_place(vxgl_texmem *m) {
 GLuint l;uint32_t o;
 if(!host_tex_live)host_tex_top=host_tex_base;
 o=host_tex_top;
 if(m->block){o=(o+255)&~255U;for(l=0;l<m->levels;l++)m->off[l]=o+m->rel[l];o+=m->block;} /* v10.317: a block copy, laid out by vxgl_texmem_layout */
 else for(l=0;l<m->levels;l++){uint32_t lw=m->w>>l,lh=m->h>>l,pt;if(!lw)lw=1;if(!lh)lh=1;pt=host_packed(m->format,lw);if(pt<32)pt=32;if(!host_dxt(m->format))pt=(pt+31)&~31U;o=(o+255)&~255U;m->off[l]=o;m->pitch[l]=pt;o+=pt*host_rows(m->format,lh);}
 if(o>sizeof(vram))return 0;
 host_tex_top=o;return 1;
}
/* where each copy really is (DirectDraw's Lock finds it there whatever offsets the ICD keeps): a restore moves it */
static struct {const void *m;uint32_t off[VXGL_TEXLEVELS];} host_tex_real[256];static size_t host_tex_id; /* v10.366: keyed by the copy's own id (surf[0]), not where the ICD keeps it (its ring moves copies) */
static void host_tex_where(const vxgl_texmem *m,int gone) {
 int k,f=-1;for(k=0;k<256;k++){if(host_tex_real[k].m==m->surf[0]){f=k;break;}if(f<0&&!host_tex_real[k].m)f=k;}
 if(f<0)return;if(gone){if(host_tex_real[f].m==m->surf[0])host_tex_real[f].m=0;return;}
 host_tex_real[f].m=m->surf[0];memcpy(host_tex_real[f].off,m->off,sizeof(m->off));
}
static void host_tex_poison(const vxgl_texmem *m) {
 GLuint l;
 if(m->block){uint32_t i;for(i=0;i<m->block/4;i++)((uint32_t*)(vram+m->off[0]))[i]=0xFFFF00FFUL;return;} /* v10.317 */
 for(l=0;l<m->levels;l++){uint32_t lh=m->h>>l,i,n;if(!lh)lh=1;n=m->pitch[l]*host_rows(m->format,lh)/4;for(i=0;i<n;i++)((uint32_t*)(vram+m->off[l]))[i]=0xFFFF00FFUL;}
}
int vxgl_texmem_alloc(vxgl_texmem *m) {
 host_tex_allocs++;
 if(host_tex_fail||!host_tex_place(m))return 0;
 host_tex_live++;m->surf[0]=(void*)++host_tex_id;m->surf[1]=(void*)(size_t)(host_tex_lose+1);host_tex_where(m,0);return 1;
}
void vxgl_texmem_free(vxgl_texmem *m) {if(m->surf[0]){vxgl_texmem_freed++;host_tex_poison(m);host_tex_where(m,1);if(host_tex_live)host_tex_live--;}memset(m,0,sizeof(*m));}
static GLubyte *host_lock_at(vxgl_texmem *m,GLuint l,uint32_t *pitch);
GLubyte *vxgl_texmem_lock(vxgl_texmem *m,GLuint l,uint32_t *pitch) {host_done_call=host_call;vxgl_seq_idle();return host_lock_at(m,l,pitch);} /* v10.366: the HAL's Lock waits for the engine */
static GLubyte *host_lock_at(vxgl_texmem *m,GLuint l,uint32_t *pitch) {
 int k;*pitch=m->pitch[l];
 if(host_dxt(m->format)){uint32_t lw=m->w>>l,lh=m->h>>l;if(!lw)lw=1;if(!lh)lh=1;*pitch=host_packed(m->format,lw)*host_rows(m->format,lh);} /* the HAL's linear size */for(k=0;k<256;k++)if(host_tex_real[k].m==m->surf[0])return vram+host_tex_real[k].off[l];
 return vram+m->off[l];
}
/* v10.366: the engine as the VxD runs it: the packets of the last call that took any still drawing (one fetch group in flight) until the
 * next such call or a wait; a copy written without a lock (vxgl_texmem_direct) while a queued or drawing packet samples it is counted */
static struct {uint32_t off[3],call;} hp[1024];static uint32_t hp_in,hp_out;unsigned long host_direct_bad,host_direct_writes;
static void hp_add(const vx3dn_packet *p) {uint32_t k=hp_in++%1024,u;hp[k].call=0;for(u=0;u<3;u++)hp[k].off[u]=0xFFFFFFFFUL;
 if(p->op==VX3DN_DRAW&&p->tex.enable){hp[k].off[0]=p->tex.surface.offset;for(u=1;u<p->stages.units&&u<3;u++)hp[k].off[u]=p->stages.unit[u-1].surface.offset;}}
static void hp_ran(void) {if(hp_out!=hp_in)hp[hp_out++%1024].call=host_call;}
GLubyte *vxgl_texmem_direct(vxgl_texmem *m,GLuint l,uint32_t *pitch) {
 uint32_t k,u;
 if(m->block||host_dxt(m->format)||l>=m->levels||!m->surf[0]||getenv("VXGL_NODIRECT"))return 0;
 host_direct_writes++;
 for(k=hp_in>1024?hp_in-1024:0;k<hp_in;k++){uint32_t j=k%1024;
  if(hp[j].call&&hp[j].call<=host_done_call)continue;
  for(u=0;u<3;u++)if(hp[j].off[u]==m->off[0]){host_direct_bad++;printf("  direct write: texture at %08X still read by packet %u (%s)\n",m->off[0],k,hp[j].call?"drawing":"queued");break;}
  if(u<3)break;}
 return host_lock_at(m,l,pitch);
}
GLubyte *vxgl_texmem_map(vxgl_texmem *m,GLuint l,uint32_t *pitch) {
 int k;if(l>=m->levels||!m->surf[0])return 0;
 if(host_map_fail>0){host_map_fail--;return 0;}
 for(k=0;k<32;k++)if(!host_mapped[k]){host_mapped[k]=m;break;}
 return vxgl_texmem_lock(m,l,pitch);
} /* v10.321 */
void vxgl_texmem_unlock(vxgl_texmem *m,GLuint l) { /* v10.307: the HAL spreads a DXT level's packed block rows to the sampler's pitch */
 {int k;for(k=0;k<32;k++)if(host_mapped[k]==m){host_mapped[k]=0;if(host_lock_depth<=0)host_unlock_outside++;break;}}
 if(l<m->levels&&host_dxt(m->format)){uint32_t lw=m->w>>l,lh=m->h>>l,a,b,n,r,o2;GLubyte *p;int k;if(!lw)lw=1;if(!lh)lh=1;
  a=host_packed(m->format,lw);b=m->pitch[l];n=host_rows(m->format,lh);o2=m->off[l];for(k=0;k<256;k++)if(host_tex_real[k].m==m->surf[0])o2=host_tex_real[k].off[l];
  p=vram+o2;if(a<b)for(r=n;r-->1;){memmove(p+r*b,p+r*a,a);memset(p+r*b+a,0,b-a);}
  if(a<b)memset(p+a,0,b-a);}
}
int vxgl_texmem_lost(vxgl_texmem *m) {return m->surf[0]&&(size_t)m->surf[1]!=(size_t)host_tex_lose+1;}
int vxgl_texmem_restore(vxgl_texmem *m) {
 vxgl_texmem old=*m;
 if(!m->surf[0])return 0;
 m->surf[1]=(void*)(size_t)(host_tex_lose+1);
 if(!host_tex_place(m)){*m=old;vxgl_texmem_free(m);return 0;}
 host_tex_where(m,0);host_tex_poison(&old);return 1;
}
static int host_front_fail; /* v10.261: no front storage at all (no surface, no system memory) */
int vxgl_front_load(vxgl_fb *fb) {if(host_front_fail){fb->pfront=0;fb->fstate|=1;memset(&fb->sfront,0,sizeof(fb->sfront));fb->front=0;return 0;}fb->pfront=vram+(host_w>W?0xC00000:0x500000);fb->fstate|=1;
 fb->front=(LPDIRECTDRAWSURFACE7)1;fb->sfront=fb->sback;fb->sfront.offset=0x500000;return 1;} /* v10.249: its own memory, as the ICD's front surface; v10.317: where the engine draws into it */
void vxgl_front_present(vxgl_fb *fb) {fb->fstate&=~2;}

/* ------------------------------------------------------------------ the engine model */
static float fl(uint32_t b) {float f;memcpy(&f,&b,4);return f;}
static int cmpf(uint32_t func,double a,double b) {
 switch(func){case 1:return 0;case 2:return a<b;case 3:return a==b;case 4:return a<=b;case 5:return a>b;case 6:return a!=b;case 7:return a>=b;default:return 1;}
}
static double factor(uint32_t f,const double *s,const double *d,int k,uint32_t kc) {
 double kk=k==0?((kc>>16)&255)/255.0:k==1?((kc>>8)&255)/255.0:(kc&255)/255.0; /* v10.304: the side's constant (14-17) */
 switch(f&31) {
  case 14:case 16:return kk;case 15:case 17:return 1-kk;
  case 1:return 0;case 2:return 1;case 3:return s[k];case 4:return 1-s[k];case 5:return s[3];case 6:return 1-s[3];
  case 7:return d[3];case 8:return 1-d[3];case 9:return d[k];case 10:return 1-d[k];
  case 11:return k==3?1:(s[3]<1-d[3]?s[3]:1-d[3]);
 }
 return 1;
}
static uint32_t sop(uint32_t op,uint32_t v,uint32_t ref) {
 switch(op){case 0:return v;case 1:return 0;case 2:return ref;case 3:return v<255?v+1:255;case 4:return v?v-1:0;case 5:return ~v&255;case 6:return (v+1)&255;case 7:return (v-1)&255;}
 return v;
}
typedef struct {double x,y,z,c[4],s[4],w,uv[3][2],q,r[3];} mv; /* v10.268: rhw and each unit's coordinates; v10.295: q, unit 0's third (cube r, projective q) */
static int host_flat_strip_fail; /* v10.481 */
static void get(const vx3dn_packet *p,uint32_t i,mv *v) {
 uint32_t c,s;
 if(p->pre.on){const uint32_t vs=VX3DN_PRE_STRIDE(p),*w=&p->pre.w[i*vs]; /* .523: 8 words a vertex in the 8-word form */ /* v10.501: the prepacked form (XP's 10 words: x y z rhw, colour, specular, s t of set 0, s t of set 1) and the indexed packets' vertices */
  c=w[4];s=w[5];v->x=fl(w[0]);v->y=fl(w[1]);v->z=fl(w[2]);v->w=fl(w[3]);
  v->c[0]=((c>>16)&255)/255.0;v->c[1]=((c>>8)&255)/255.0;v->c[2]=(c&255)/255.0;v->c[3]=(c>>24)/255.0;
  v->s[0]=((s>>16)&255)/255.0;v->s[1]=((s>>8)&255)/255.0;v->s[2]=(s&255)/255.0;v->s[3]=(s>>24)/255.0;
  if(p->tex.enable){uint32_t u,n=p->stages.units?p->stages.units:1;
   for(u=0;u<n&&u<3;u++){int set1=u&&p->stages.set[u];v->uv[u][0]=fl(w[set1&&vs==10?8:6]);v->uv[u][1]=fl(w[set1&&vs==10?9:7]);v->r[u]=1.0;}
   v->q=1.0;}
  return;}
 c=p->vertices[i].color;s=p->shader.spec[i];
 v->x=fl(p->vertices[i].x);v->y=fl(p->vertices[i].y);v->z=fl(p->vertices[i].z);
 v->c[0]=((c>>16)&255)/255.0;v->c[1]=((c>>8)&255)/255.0;v->c[2]=(c&255)/255.0;v->c[3]=(c>>24)/255.0;
 v->s[0]=((s>>16)&255)/255.0;v->s[1]=((s>>8)&255)/255.0;v->s[2]=(s&255)/255.0;v->s[3]=(s>>24)/255.0;
 v->w=fl(p->vertices[i].rhw);
 if(p->tex.enable){uint32_t u,n=p->stages.units?p->stages.units:1;
  for(u=0;u<n&&u<3;u++){int set1=u&&p->stages.set[u];v->uv[u][0]=fl(set1?p->stages.uv1[i][0]:p->tex.uv[i][0]);v->uv[u][1]=fl(set1?p->stages.uv1[i][1]:p->tex.uv[i][1]);}
  v->q=p->stages.layout?fl(p->stages.q[i]):1.0;
  for(u=0;u<n&&u<3;u++){int set1=u&&p->stages.set[u];v->r[u]=set1?((p->stages.s3&2)?fl(p->stages.r1[i]):1.0):((p->stages.layout||(p->stages.s3&1))?fl(p->stages.q[i]):1.0);}} /* v10.321 */
}
/* v10.268: texturing as Direct3D's texture stages define it (the stage state VXGL gave the core: cur->core.state.tss), sampled from the
 * packet's surfaces with its filters, address modes and mip chain. The core's composition of the stages into the engine's program is
 * not modelled (the board runs it); VXGL's side is: the stage state, the copies in video memory, the levels, the coordinates. */
static uint32_t g_border; /* v10.304: the sampled unit's border colour (address code 0) */
static void texel(const vx3dn_surface *s,uint32_t off,uint32_t w,uint32_t h,int x,int y,uint32_t au,uint32_t av,double *o) {
 uint32_t v,pitch=host_packed(s->format,w);if(pitch<32)pitch=32;if(!host_dxt(s->format))pitch=(pitch+31)&~31U; /* v10.321: as the VxD's tex_pitch */
 if((!au&&(x<0||x>=(int)w))||(!av&&(y<0||y>=(int)h))){v=g_border;o[0]=((v>>16)&255)/255.0;o[1]=((v>>8)&255)/255.0;o[2]=(v&255)/255.0;o[3]=(v>>24)/255.0;return;}
 if(au==1)x=x<0?0:x>=(int)w?(int)w-1:x;else if(au==3){int m=((x%(2*(int)w))+2*(int)w)%(2*(int)w);x=m<(int)w?m:2*(int)w-1-m;}else x=((x%(int)w)+(int)w)%(int)w;
 if(av==1)y=y<0?0:y>=(int)h?(int)h-1:y;else if(av==3){int m=((y%(2*(int)h))+2*(int)h)%(2*(int)h);y=m<(int)h?m:2*(int)h-1-m;}else y=((y%(int)h)+(int)h)%(int)h;
 if(host_dxt(s->format)){ /* v10.307: the sampler's DXT: DXT1's three-colour colour 3 transparent black, DXT3 / DXT5 four colours */
  uint32_t bs=s->format==VX3DN_TEX_DXT1?8:16;const GLubyte *b=vram+off+(uint32_t)(y/4)*pitch+(uint32_t)(x/4)*bs;GLchan c[4];
  if(s->format==VX3DN_TEX_DXT1)dxtn_colour(b,0,x,y,c,0);
  else{int n=(y&3)*4+(x&3);dxtn_colour(b+8,1,x,y,c,1);
   if(s->format==VX3DN_TEX_DXT3)c[3]=(GLchan)(((b[n/2]>>((n&1)*4))&15)*17);
   else{int bit=n*3,by=2+bit/8,code=((b[by]|((by+1<8)?(b[by+1]<<8):0))>>(bit&7))&7;c[3]=(GLchan)dxtn_alpha(b,code);}}
  o[0]=c[0]/255.0;o[1]=c[1]/255.0;o[2]=c[2]/255.0;o[3]=c[3]/255.0;return;}
 if(s->format==VX3DN_TEX_RGB565){v=*(const uint16_t*)(vram+off+(uint32_t)y*pitch+(uint32_t)x*2); /* v10.366: the sampler's R5G6B5, bits widened */
  o[0]=(((v>>11)&31)*255/31)/255.0;o[1]=(((v>>5)&63)*255/63)/255.0;o[2]=((v&31)*255/31)/255.0;o[3]=1.0;return;}
 v=*(const uint32_t*)(vram+off+(uint32_t)y*pitch+(uint32_t)x*4);
 o[0]=((v>>16)&255)/255.0;o[1]=((v>>8)&255)/255.0;o[2]=(v&255)/255.0;o[3]=s->format==VX3DN_TEX_XRGB8888?1.0:(v>>24)/255.0;
}
static void sample_level(const vx3dn_surface *s,uint32_t off,uint32_t lv,int lin,uint32_t au,uint32_t av,double u,double v,double *o) {
 uint32_t w=s->width>>lv,h=s->height>>lv;int k;
 if(!w)w=1;if(!h)h=1;
 if(!lin){texel(s,off,w,h,(int)floor(u*w),(int)floor(v*h),au,av,o);return;}
 {double x=u*w-0.5,y=v*h-0.5,fx=x-floor(x),fy=y-floor(y),a[4],b[4],c[4],d[4];int x0=(int)floor(x),y0=(int)floor(y);
  texel(s,off,w,h,x0,y0,au,av,a);texel(s,off,w,h,x0+1,y0,au,av,b);texel(s,off,w,h,x0,y0+1,au,av,c);texel(s,off,w,h,x0+1,y0+1,au,av,d);
  for(k=0;k<4;k++)o[k]=(a[k]*(1-fx)+b[k]*fx)*(1-fy)+(c[k]*(1-fx)+d[k]*fx)*fy;}
}
/* v10.295: a cube map (layout 1): the face and its s, t from the direction (GL's table, which Direct3D shares), sampled flat.
 * v10.317: with its mip chain: each face's levels at the level table's offsets from that face's base (HCM p116 base mode 1); the LOD as
 * swrast takes it (from s and t as they are, the face's size) */
/* v10.321: on unit 1 or 2 too (its surface, filter, faces and chain) */
static void sample_cube(const vx3dn_packet *p,uint32_t unit,double s,double t,double r,double lambda,double *o) {
 double as=fabs(s),at=fabs(t),ar=fabs(r),sc,tc,ma,u,v;int f;vx3dn_surface fs=unit?p->stages.unit[unit-1].surface:p->tex.surface;uint32_t fl=unit?p->stages.unit[unit-1].filter:p->tex.filter;
 const uint32_t *face=unit?p->stages.uface[unit-1]:p->stages.face,*ltab=unit?p->stages.ulevel[unit-1]:p->mip.level;
 uint32_t levels=unit?p->stages.umip[unit-1]&255:p->mip.levels,mf=unit?(p->stages.umip[unit-1]>>8)&3:p->mip.filter,bias=unit?p->stages.ubias[unit-1]:p->mip.bias;
 int mag_lin=fl==VX3DN_FILTER_LINEAR||fl==VX3DN_FILTER_MAG_LINEAR,min_lin=fl==VX3DN_FILTER_LINEAR||fl==VX3DN_FILTER_MIN_LINEAR;
 if(as>=at&&as>=ar){ma=as;if(s>0){f=0;sc=-r;tc=-t;}else{f=1;sc=r;tc=-t;}}
 else if(at>=ar){ma=at;if(t>0){f=2;sc=s;tc=r;}else{f=3;sc=s;tc=-r;}}
 else {ma=ar;if(r>0){f=4;sc=s;tc=-t;}else{f=5;sc=-s;tc=-t;}}
 if(ma==0)ma=1;
 fs.offset=face[f];u=(sc/ma+1)/2;v=(tc/ma+1)/2;
 if(levels>1&&mf){double b=(double)(int8_t)(uint8_t)bias/2.0,l=lambda+b;uint32_t n=levels-1;if(n>8)n=8;
  if(l>0){
   #define COFF(k) ((k)&&(k)<levels?fs.offset+(ltab[(k)-1]<<8):fs.offset)
   if(mf==1){uint32_t k=(uint32_t)floor(l+0.5);if(k>n)k=n;sample_level(&fs,COFF(k),k,min_lin,1,1,u,v,o);return;}
   {uint32_t k=(uint32_t)floor(l),k1;double fr=l-floor(l),a[4],b2[4];int c;if(k>=n){sample_level(&fs,COFF(n),n,min_lin,1,1,u,v,o);return;}k1=k+1;
    sample_level(&fs,COFF(k),k,min_lin,1,1,u,v,a);sample_level(&fs,COFF(k1),k1,min_lin,1,1,u,v,b2);for(c=0;c<4;c++)o[c]=a[c]*(1-fr)+b2[c]*fr;return;}
   #undef COFF
  }
  sample_level(&fs,fs.offset,0,mag_lin,1,1,u,v,o);return;}
 sample_level(&fs,fs.offset,0,lambda>0?min_lin:mag_lin,1,1,u,v,o); /* the engine clamps a cube's faces to their edges */
}
/* v10.317: a 3D texture (layout 3): slice k at the base + k x the slice step (face[1]), the R axis wrapped as the S and T ones are (face[2])
 * and filtered as they are: the two nearest slices blended for a linear filter */
static void sample_volume(const vx3dn_packet *p,uint32_t unit,double s,double t,double r,double lambda,double *o) {
 const vx3dn_surface *ts=unit?&p->stages.unit[unit-1].surface:&p->tex.surface;const uint32_t *face=unit?p->stages.uface[unit-1]:p->stages.face;
 uint32_t fl=unit?p->stages.unit[unit-1].filter:p->tex.filter,d=face[0],ar=face[2],au=unit?p->stages.unit[unit-1].address_u:p->tex.address_u,av=unit?p->stages.unit[unit-1].address_v:p->tex.address_v;
 int lin,k0,k1,c;double rz,fr,a[4],b[4];
 lin=lambda>0?(fl==VX3DN_FILTER_LINEAR||fl==VX3DN_FILTER_MIN_LINEAR):(fl==VX3DN_FILTER_LINEAR||fl==VX3DN_FILTER_MAG_LINEAR);
 if(!d)d=1;
 #define WRAPK(k) (ar==1?((k)<0?0:(k)>=(int)d?(int)d-1:(k)):ar==3?(((((k)%(2*(int)d))+2*(int)d)%(2*(int)d))<(int)d?((((k)%(2*(int)d))+2*(int)d)%(2*(int)d)):2*(int)d-1-((((k)%(2*(int)d))+2*(int)d)%(2*(int)d))):((((k)%(int)d)+(int)d)%(int)d))
 g_border=p->stages.border[unit];
 if(!lin){int k=(int)floor(r*d);if(!ar&&(k<0||k>=(int)d)){uint32_t v=p->stages.border[unit];o[0]=((v>>16)&255)/255.0;o[1]=((v>>8)&255)/255.0;o[2]=(v&255)/255.0;o[3]=(v>>24)/255.0;return;}
  k=WRAPK(k);sample_level(ts,ts->offset+(uint32_t)k*face[1],0,0,au,av,s,t,o);return;}
 rz=r*d-0.5;k0=(int)floor(rz);fr=rz-k0;k1=k0+1;
 /* v10.330 (review #32): the border address on R filtered linearly: a sample with a slice outside is the border colour (the engine's rule
  * for a filtered border sample, HCM p126); both slices were wrapped */
 if(!ar&&(k0<0||k1>=(int)d)){uint32_t v=p->stages.border[unit];o[0]=((v>>16)&255)/255.0;o[1]=((v>>8)&255)/255.0;o[2]=(v&255)/255.0;o[3]=(v>>24)/255.0;return;}
 k0=WRAPK(k0);k1=WRAPK(k1);
 #undef WRAPK
 sample_level(ts,ts->offset+(uint32_t)k0*face[1],0,1,au,av,s,t,a);
 sample_level(ts,ts->offset+(uint32_t)k1*face[1],0,1,au,av,s,t,b);
 for(c=0;c<4;c++)o[c]=a[c]*(1-fr)+b[c]*fr;
}
/* unit u's texel at u,v; lambda (log2 of texels per pixel, level 0) for unit 0's mip chain */
static void sample(const vx3dn_packet *p,uint32_t unit,double u,double v,double lambda,double *o) {
 const vx3dn_surface *s=unit?&p->stages.unit[unit-1].surface:&p->tex.surface;uint32_t f=unit?p->stages.unit[unit-1].filter:p->tex.filter;
 uint32_t au=unit?p->stages.unit[unit-1].address_u:p->tex.address_u,av=unit?p->stages.unit[unit-1].address_v:p->tex.address_v;
 int mag_lin=f==VX3DN_FILTER_LINEAR||f==VX3DN_FILTER_MAG_LINEAR,min_lin=f==VX3DN_FILTER_LINEAR||f==VX3DN_FILTER_MIN_LINEAR;
 uint32_t levels=unit?p->stages.umip[unit-1]&255:p->mip.levels,mf=unit?(p->stages.umip[unit-1]>>8)&3:p->mip.filter,bias=unit?p->stages.ubias[unit-1]:p->mip.bias;
 const uint32_t *ltab=unit?p->stages.ulevel[unit-1]:p->mip.level;
 g_border=p->stages.border[unit];
 if(levels>1&&mf){double b=(double)(int8_t)(uint8_t)bias/2.0,l=lambda+b;
  if(l>0){uint32_t n;
   /* v10.269: as the board: the chain runs down to 1x1 whatever the packet's level count, a level past it at offset 0 (level 0's);
    * v10.304: the VxD sets the maximum level to the chain's last, 8 at most: the LOD stops there */
   n=levels-1;if(n>8)n=8;
   #define LOFF(k) ((k)&&(k)<levels?s->offset+(ltab[(k)-1]<<8):s->offset)
   if(mf==1){uint32_t k=(uint32_t)floor(l+0.5);if(k>n)k=n;sample_level(s,LOFF(k),k,min_lin,au,av,u,v,o);return;}
   {uint32_t k=(uint32_t)floor(l),k1;double fr=l-floor(l),a[4],b2[4];int c;if(k>=n){sample_level(s,LOFF(n),n,min_lin,au,av,u,v,o);return;}k1=k+1;
    sample_level(s,LOFF(k),k,min_lin,au,av,u,v,a);sample_level(s,LOFF(k1),k1,min_lin,au,av,u,v,b2);for(c=0;c<4;c++)o[c]=a[c]*(1-fr)+b2[c]*fr;return;}
   #undef LOFF
  }
  sample_level(s,s->offset,0,mag_lin,au,av,u,v,o);return;}
 sample_level(s,s->offset,0,lambda>0?min_lin:mag_lin,au,av,u,v,o);
}
static double clamp1(double v){return v<0?0:v>1?1:v;}
static void stage_arg(uint32_t a,const double *diff,const double *curr,const double *tex,const double *spec,double *o) {
 const uint32_t *rs=cur->core.state.rs;double t[4];int k;
 switch(a&15){case 0:memcpy(t,diff,sizeof(t));break;case 1:memcpy(t,curr,sizeof(t));break;case 2:memcpy(t,tex,sizeof(t));break;
  case 3:t[0]=((rs[60]>>16)&255)/255.0;t[1]=((rs[60]>>8)&255)/255.0;t[2]=(rs[60]&255)/255.0;t[3]=(rs[60]>>24)/255.0;break;default:memcpy(t,spec,sizeof(t));}
 if(a&0x20)for(k=0;k<3;k++)t[k]=t[3];
 if(a&0x10)for(k=0;k<4;k++)t[k]=1-t[k];
 memcpy(o,t,sizeof(t));
}
static double stage_op(uint32_t op,double a,double b,double at,double ad,double af,double ac) {
 switch(op){case 2:return a;case 3:return b;case 4:return a*b;case 5:return clamp1(2*a*b);case 6:return clamp1(4*a*b);case 7:return clamp1(a+b);
  case 8:return clamp1(a+b-0.5);case 9:return clamp1(2*(a+b-0.5));case 10:return clamp1(a-b);case 11:return clamp1(a+b-a*b);
  case 12:return a*ad+b*(1-ad);case 13:return a*at+b*(1-at);case 14:return a*af+b*(1-af);case 16:return a*ac+b*(1-ac);}
 return a;
}
static double g_q; /* v10.295: unit 0's interpolated third coordinate */
static double g_r[3]; /* v10.321: each unit's (its set's third coordinate) */
static void textured(const vx3dn_packet *p,const double *uv,const double *lambda,double *col,const double *spec) {
 const uint32_t (*tss)[VX_D3D_TSS]=cur->core.state.tss;double diff[4],curr[4],tex[4],a1[4],a2[4],tf[4],o[4];uint32_t s,n=p->stages.units?p->stages.units:1;int k;
 memcpy(diff,col,sizeof(diff));memcpy(curr,col,sizeof(curr));
 tf[3]=(cur->core.state.rs[60]>>24)/255.0;
 for(s=0;s<VX_D3D_STAGES;s++) {
  const uint32_t *t=tss[s];
  if(t[1]==1)break;
  if(!p->tex.enable){ /* v10.321: no texture (a program on the colours and TFACTOR): the core reads TEXTURE as the diffuse colour, and a
   * TEXTURE first argument ends the cascade (compose_core, compose_multi) */
   if((!s&&t[2]==2)||(s&&(t[2]&15)==2&&t[1]!=3))break;
   memcpy(tex,diff,sizeof(tex));}
  else if(s<n){uint32_t ly=s?p->stages.ulayout[s-1]:p->stages.layout;double r3=s?g_r[s]:g_q;
   if(ly==VX3DN_LAYOUT_CUBE)sample_cube(p,s,uv[s*2],uv[s*2+1],r3,lambda[s],tex);else if(ly==VX3DN_LAYOUT_VOLUME)sample_volume(p,s,uv[s*2],uv[s*2+1],r3,lambda[s],tex);
   else sample(p,s,uv[s*2],uv[s*2+1],lambda[s],tex);}else memset(tex,0,sizeof(tex));
  stage_arg(t[2],diff,curr,tex,spec,a1);stage_arg(t[3],diff,curr,tex,spec,a2);
  if(t[1]==24&&t[VX_D3D_TSS_DOT3RGB]==1){double d=4*((a1[0]-.5)*(a2[0]-.5)+(a1[1]-.5)*(a2[1]-.5)+(a1[2]-.5)*(a2[2]-.5));d=clamp1(d*(1<<t[VX_D3D_TSS_CSCALE]));o[0]=o[1]=o[2]=d; /* v10.317: DOT3_RGB */
   if(t[4]!=1){double a0[4];stage_arg(t[5],diff,curr,tex,spec,a1);stage_arg(t[6],diff,curr,tex,spec,a2);if(t[4]==26)stage_arg(t[VX_D3D_TSS_AARG0],diff,curr,tex,spec,a0);
    o[3]=clamp1((t[4]==26?a0[3]*a1[3]+(1-a0[3])*a2[3]:stage_op(t[4],a1[3],a2[3],tex[3],diff[3],tf[3],curr[3]))*(1<<t[VX_D3D_TSS_ASCALE]));}else o[3]=curr[3];}
  else if(t[1]==24){double d=4*((a1[0]-.5)*(a2[0]-.5)+(a1[1]-.5)*(a2[1]-.5)+(a1[2]-.5)*(a2[2]-.5));d=clamp1(d*(1<<t[VX_D3D_TSS_CSCALE]));o[0]=o[1]=o[2]=o[3]=d;}
  else {double a0[4];
   if(t[1]==26)stage_arg(t[VX_D3D_TSS_CARG0],diff,curr,tex,spec,a0); /* v10.304: LERP */
   for(k=0;k<3;k++)o[k]=clamp1((t[1]==26?a0[k]*a1[k]+(1-a0[k])*a2[k]:stage_op(t[1],a1[k],a2[k],tex[3],diff[3],tf[3],curr[3]))*(1<<t[VX_D3D_TSS_CSCALE]));
   if(t[4]!=1){stage_arg(t[5],diff,curr,tex,spec,a1);stage_arg(t[6],diff,curr,tex,spec,a2);if(t[4]==26)stage_arg(t[VX_D3D_TSS_AARG0],diff,curr,tex,spec,a0);
    o[3]=clamp1((t[4]==26?a0[3]*a1[3]+(1-a0[3])*a2[3]:stage_op(t[4],a1[3],a2[3],tex[3],diff[3],tf[3],curr[3]))*(1<<t[VX_D3D_TSS_ASCALE]));}else o[3]=curr[3];}
  memcpy(curr,o,sizeof(curr));
 }
 memcpy(col,curr,sizeof(curr));
}
static void pixel(const vx3dn_packet *p,int x,int y,double z,const double *col,const double *spec) {
 const uint32_t *rs=cur->core.state.rs;double c[4],d[4];int k;uint8_t *t,*zp=0,*sp=0;uint32_t zv=0,st=0,zs=0;int zok=1,sok=1;
 if(x<p->clip.left||x>=p->clip.right||y<p->clip.top||y>=p->clip.bottom)return;
 if(x<0||y<0||x>=(int)p->target.width||y>=(int)p->target.height)return;
 if(p->op==VX3DN_DRAW&&p->stipple.on&&!((p->stipple.rows[y&31]>>(x&31))&1))return; /* v10.308: the engine's stipple (target row y, column bit x) */
 for(k=0;k<4;k++)c[k]=col[k];
 if(p->raster.alpha_func&&!cmpf(p->raster.alpha_func,floor(c[3]*255+0.5),(double)p->raster.alpha_ref))return;
 if(p->depth.format) {
  zp=vram+p->depth.offset+y*p->depth.pitch+x*(p->depth.format==VX3DN_D16?2:4);
  if(p->depth.format==VX3DN_D16){zv=(uint32_t)(z*65535+0.5);zs=*(uint16_t*)zp;}else{zv=(uint32_t)(z*16777215+0.5);zs=*(uint32_t*)zp>>8;
   sp=vram+VX3DN_SPLANE_OFF(&p->depth)+VX3DN_SPLANE_AT(VX3DN_SPLANE_PITCH(p->depth.width),x,y);st=*sp;} /* v10.245: stencil from its plane; v10.246 tiled */
  if(p->z_enable)zok=cmpf(p->z_func,(double)zv,(double)zs);
 }
 if(p->ext.stencil) {
  uint32_t ref=(p->ext.stencil_w1>>16)&255,mask=(p->ext.stencil_w1>>8)&255,wm=p->ext.stencil_w1&255,func=((p->ext.stencil_w2>>16)&7)+1,
   fail=(p->ext.stencil_w2>>6)&7,zfail=(p->ext.stencil_w2>>3)&7,pass=p->ext.stencil_w2&7,nv;
  sok=cmpf(func,(double)(ref&mask),(double)(st&mask));
  nv=!sok?sop(fail,st,ref):!zok?sop(zfail,st,ref):sop(pass,st,ref);
  st=(st&~wm)|(nv&wm);if(sp)*sp=(uint8_t)st;
 }
 if(!sok||!zok)return;
 if(p->op==VX3DN_DRAW)host_occl++; /* v10.315 */
 if(zp&&p->z_enable&&p->z_write){if(p->depth.format==VX3DN_D16)*(uint16_t*)zp=(uint16_t)zv;else *(uint32_t*)zp=(zv<<8)|(*(uint32_t*)zp&255);}
 if(rs[29])for(k=0;k<3;k++){c[k]+=spec[k];if(c[k]>1)c[k]=1;}
 if(rs[28]){double f=spec[3],fc[3];fc[0]=((rs[34]>>16)&255)/255.0;fc[1]=((rs[34]>>8)&255)/255.0;fc[2]=(rs[34]&255)/255.0;for(k=0;k<3;k++)c[k]=f*c[k]+(1-f)*fc[k];}
 t=vram+p->target.offset+y*p->target.pitch+x*(p->target.format==VX3DN_RGB565?2:4);
 if(p->target.format==VX3DN_RGB565){uint16_t v=*(uint16_t*)t;d[0]=((v>>11)&31)/31.0;d[1]=((v>>5)&63)/63.0;d[2]=(v&31)/31.0;}
 else {uint32_t v=*(uint32_t*)t;d[0]=((v>>16)&255)/255.0;d[1]=((v>>8)&255)/255.0;d[2]=(v&255)/255.0;}
 d[3]=1;
 if(p->blend){double s2[4];uint32_t bs=p->raster.blend_src?p->raster.blend_src:5,bd=p->raster.blend_dst?p->raster.blend_dst:6,op=(bs>>5)&7;
  for(k=0;k<4;k++)s2[k]=c[k];
  for(k=0;k<3;k++){double a=s2[k]*factor(bs,s2,d,k,bs>>8),b=d[k]*factor(bd,s2,d,k,bd>>8); /* v10.304: the equation */
   c[k]=op==VX3DN_BLENDOP_SUB?a-b:op==VX3DN_BLENDOP_REVSUB?b-a:op==VX3DN_BLENDOP_MIN?fmin(s2[k],d[k]):op==VX3DN_BLENDOP_MAX?fmax(s2[k],d[k]):a+b;
   if(c[k]>1)c[k]=1;if(c[k]<0)c[k]=0;}}
 {uint32_t sv,dv,m=p->write_mask,keep=0,out;
  if(p->target.format==VX3DN_RGB565){
   if(m&0x100){uint32_t r8=(uint32_t)(c[0]*255+0.5),g8=(uint32_t)(c[1]*255+0.5),b8=(uint32_t)(c[2]*255+0.5);sv=((r8>>3)<<11)|((g8>>2)<<5)|(b8>>3);} /* v10.295: a logic op on the 8-bit source's top bits (swrast's order) */
   else sv=((uint32_t)(c[0]*31+0.5)<<11)|((uint32_t)(c[1]*63+0.5)<<5)|(uint32_t)(c[2]*31+0.5);
   dv=*(uint16_t*)t;
   if(!(m&1))keep|=0xF800;if(!(m&2))keep|=0x7E0;if(!(m&4))keep|=0x1F;}
  else {sv=((uint32_t)(c[0]*255+0.5)<<16)|((uint32_t)(c[1]*255+0.5)<<8)|(uint32_t)(c[2]*255+0.5);dv=*(uint32_t*)t&0xFFFFFF;
   if(!(m&1))keep|=0xFF0000;if(!(m&2))keep|=0xFF00;if(!(m&4))keep|=0xFF;}
  out=sv;
  if(m&0x100){uint32_t tt=(m>>4)&15,r=0; /* v10.295: the logic op, a truth table of source 1100 and destination 1010 */
   if(tt&8)r|=sv&dv;if(tt&4)r|=sv&~dv;if(tt&2)r|=~sv&dv;if(tt&1)r|=~sv&~dv;out=r;}
  out=(out&~keep)|(dv&keep);
  if(p->target.format==VX3DN_RGB565)*(uint16_t*)t=(uint16_t)out;else *(uint32_t*)t=out&0xFFFFFF;}
}
static void triangle(const vx3dn_packet *p,const mv *a,const mv *b,const mv *c) {
 double area=(b->x-a->x)*(c->y-a->y)-(b->y-a->y)*(c->x-a->x);int x0,x1,y0,y1,x,y,k;
 if(area==0)return;
 if(area<0){const mv *t=b;b=c;c=t;area=-area;}
 /* v10.330 (review #33): a vertex that is not finite draws nothing, and the box is the part on the target (a huge triangle ran for ever) */
 if(!isfinite(a->x)||!isfinite(a->y)||!isfinite(b->x)||!isfinite(b->y)||!isfinite(c->x)||!isfinite(c->y)||!isfinite(area))return;
 {double fx0=floor(fmin(a->x,fmin(b->x,c->x))),fx1=ceil(fmax(a->x,fmax(b->x,c->x))),fy0=floor(fmin(a->y,fmin(b->y,c->y))),fy1=ceil(fmax(a->y,fmax(b->y,c->y)));
  if(fx0<0)fx0=0;if(fy0<0)fy0=0;if(fx1>(double)p->target.width)fx1=p->target.width;if(fy1>(double)p->target.height)fy1=p->target.height;
  if(fx0>fx1||fy0>fy1)return;x0=(int)fx0;x1=(int)fx1;y0=(int)fy0;y1=(int)fy1;}
 for(y=y0;y<=y1;y++)for(x=x0;x<=x1;x++) {
  /* edge functions with the top-left rule (y down, clockwise after the swap) */
  double px=x,py=y,w0=(c->x-b->x)*(py-b->y)-(c->y-b->y)*(px-b->x),w1=(a->x-c->x)*(py-c->y)-(a->y-c->y)*(px-c->x),w2=(b->x-a->x)*(py-a->y)-(b->y-a->y)*(px-a->x);
  const mv *e[3][2];double w[3];int ok=1;
  e[0][0]=b;e[0][1]=c;e[1][0]=c;e[1][1]=a;e[2][0]=a;e[2][1]=b;w[0]=w0;w[1]=w1;w[2]=w2;
  for(k=0;k<3;k++){double dx=e[k][1]->x-e[k][0]->x,dy=e[k][1]->y-e[k][0]->y;int topleft=(dy==0&&dx>0)||dy<0;
   if(w[k]<0||(w[k]==0&&!topleft)){ok=0;break;}}
  if(!ok)continue;
  {double l0=w0/area,l1=w1/area,l2=w2/area,col[4],sp[4],z=l0*a->z+l1*b->z+l2*c->z;
   for(k=0;k<4;k++){col[k]=l0*a->c[k]+l1*b->c[k]+l2*c->c[k];sp[k]=l0*a->s[k]+l1*b->s[k]+l2*c->s[k];}
   if(p->tex.enable){double uv[6],q[3][3][2],lam[3]={0,0,0};int u,dd,n=p->stages.units?(int)p->stages.units:1;
    for(dd=0;dd<3;dd++){double px2=x+(dd==1),py2=y+(dd==2),m0=((c->x-b->x)*(py2-b->y)-(c->y-b->y)*(px2-b->x))/area,m1=((a->x-c->x)*(py2-c->y)-(a->y-c->y)*(px2-c->x))/area,m2=1-m0-m1;
     double den=m0*a->w+m1*b->w+m2*c->w;
     for(u=0;u<n&&u<3;u++){double su=(m0*a->w*a->uv[u][0]+m1*b->w*b->uv[u][0]+m2*c->w*c->uv[u][0])/den,sv=(m0*a->w*a->uv[u][1]+m1*b->w*b->uv[u][1]+m2*c->w*c->uv[u][1])/den;
      if(!u&&p->stages.layout){double sq=(m0*a->w*a->q+m1*b->w*b->q+m2*c->w*c->q)/den; /* v10.295: cube r, or projective q (s/q, t/q) */
       if(p->stages.layout==2){if(sq!=0){su/=sq;sv/=sq;}}else if(!dd)g_q=sq;}
      if(!dd&&u){uint32_t ly=p->stages.ulayout[u-1];if(ly)g_r[u]=(m0*a->w*a->r[u]+m1*b->w*b->r[u]+m2*c->w*c->r[u])/den;} /* v10.321 */
      if(!dd){uv[u*2]=su;uv[u*2+1]=sv;}
      {const vx3dn_surface *us=u?&p->stages.unit[u-1].surface:&p->tex.surface;q[u][dd][0]=su*us->width;q[u][dd][1]=sv*us->height;}}}
    for(u=0;u<n&&u<3;u++){double dxu=q[u][1][0]-q[u][0][0],dxv=q[u][1][1]-q[u][0][1],dyu=q[u][2][0]-q[u][0][0],dyv=q[u][2][1]-q[u][0][1],r=fmax(sqrt(dxu*dxu+dxv*dxv),sqrt(dyu*dyu+dyv*dyv));lam[u]=r>0?log2(r):-100;}
    textured(p,uv,lam,col,sp);}
   else if(p->shader.count)textured(p,0,0,col,sp); /* v10.321: an untextured program */
   pixel(p,x,y,z,col,sp);}
 }
}
static void line(const vx3dn_packet *p,const mv *a,const mv *b) {
 double dx=b->x-a->x,dy=b->y-a->y;int n,i,k,xm=fabs(dx)>=fabs(dy);
 n=(int)floor((xm?fabs(dx):fabs(dy))+0.5); /* v10.330 (review #36): both majors rounded (the x major's length was truncated) */
 if(!n)return;
 for(i=0;i<n;i++){double t=(double)i/n,col[4],sp[4];int x=(int)floor(a->x+dx*t+0.5),y=(int)floor(a->y+dy*t+0.5);
  for(k=0;k<4;k++){col[k]=a->c[k]+(b->c[k]-a->c[k])*t;sp[k]=a->s[k]+(b->s[k]-a->s[k])*t;}
  pixel(p,x,y,a->z+(b->z-a->z)*t,col,sp);}
}
static void model(const vx3dn_packet *p) {
#ifdef HOST_PROFILE
 if(p->op!=VX3DN_CLEAR)return; /* v10.348: the driver's own cost measured (callgrind), not the model's rasterizer */
#endif
 uint32_t i,r,base=0;mv v[3];
 if(p->op==VX3DN_CLEAR) {
  int x,y;uint32_t f=p->clear_flags;
  clear_packets++;
  for(y=p->clip.top;y<p->clip.bottom;y++)for(x=p->clip.left;x<p->clip.right;x++) {
   if(f&1){uint8_t *t=vram+p->target.offset+y*p->target.pitch+x*(p->target.format==VX3DN_RGB565?2:4);uint32_t c=p->clear_color;
    if(p->target.format==VX3DN_RGB565)*(uint16_t*)t=(uint16_t)((((c>>19)&31)<<11)|(((c>>10)&63)<<5)|((c>>3)&31));else *(uint32_t*)t=c&0xFFFFFF;}
   if(f&6){uint8_t *zp=vram+p->depth.offset+y*p->depth.pitch+x*(p->depth.format==VX3DN_D16?2:4);
    if(p->depth.format==VX3DN_D16){if(f&2)*(uint16_t*)zp=(uint16_t)(p->clear_depth>>16);}
    else {uint32_t z=*(uint32_t*)zp;if(f&2)z=(z&255)|((p->clear_depth>>8)<<8);*(uint32_t*)zp=z;
     if(f&4)vram[VX3DN_SPLANE_OFF(&p->depth)+VX3DN_SPLANE_AT(VX3DN_SPLANE_PITCH(p->depth.width),x,y)]=(uint8_t)VX3DN_CLEAR_SVAL(f);}}
  }
  return;
 }
 draw_packets++;if(p->clear_flags&VX3DN_DRAW_CLEAR)clear_packets++; /* v10.357: the core's clear rectangle (ACCEL_3D_CLEAR3D, v10.354) is the clear */
 if(p->ext.prim==1){for(i=0;i+1<p->vertex_count;i+=2){get(p,i,&v[0]);get(p,i+1,&v[1]);line(p,&v[0],&v[1]);}return;}
 /* v10.481: ext.dither bit 1, the engine's flat shading (XP T48): the colour and the secondary colour's RGB of the first vertex; the fog
  * factor (secondary alpha) each vertex's own (the GL meaning; the board run of VXFLAT checks the engine does the same) */
 #define HFLAT() do{if(p->ext.dither&2){int k_;for(k_=0;k_<4;k_++){v[1].c[k_]=v[2].c[k_]=v[0].c[k_];}for(k_=0;k_<3;k_++){v[1].s[k_]=v[2].s[k_]=v[0].s[k_];}}}while(0)
 if(p->pre.on==2){const uint16_t *ix=VX3DN_CIDX(p);uint32_t ni=p->bursts.b[0]&0xFFFF; /* v10.501: an indexed packet: triangles by its packet-local indices */
  for(i=0;i+2<ni;i+=3){get(p,ix[i],&v[0]);get(p,ix[i+1],&v[1]);get(p,ix[i+2],&v[2]);HFLAT();triangle(p,&v[0],&v[1],&v[2]);}return;}
 if(!p->bursts.n){for(i=0;i+2<p->vertex_count;i+=3){get(p,i,&v[0]);get(p,i+1,&v[1]);get(p,i+2,&v[2]);HFLAT();triangle(p,&v[0],&v[1],&v[2]);}return;}
 for(r=0;r<p->bursts.n;r++) {
  uint32_t topo=p->bursts.b[r]>>16,cnt=p->bursts.b[r]&0xFFFF;
  if(topo==VX3DN_TOPO_LIST)for(i=0;i+2<cnt;i+=3){get(p,base+i,&v[0]);get(p,base+i+1,&v[1]);get(p,base+i+2,&v[2]);HFLAT();triangle(p,&v[0],&v[1],&v[2]);}
  else for(i=0;i+2<cnt;i++){get(p,base+i,&v[0]);get(p,base+i+1,&v[1]);get(p,base+i+2,&v[2]);HFLAT();triangle(p,&v[0],&v[1],&v[2]);} /* v10.482: a flat strip in the model: each triangle its first strip vertex's colours (the engine's rule for strips is not proven: VXFLAT tries both) */
  base+=cnt;
 }
}
static uint32_t host_draw(const vx3dn_packet *p);
static unsigned host_batches,host_batched;
/* v10.275: the queue's send: each packet drawn by the model now, as the engine would draw it when the ICD sends the queue; swrast must not
 * hold the surfaces then (the ICD's locks would not wait for these packets) */
/* the model takes the render and stage state from the context (cur->core.state), not from the packet: each queued packet keeps a copy of
 * the state it was made with, put back while the model draws it */
static vx_d3d_state hq_state[VX3DN_BATCH_MAX];static unsigned hq_in,hq_out;
static int host_lose; /* v10.279: the next send is answered VX3DN_LOST with nothing taken (the engine lost under a queued group) */
uint32_t vxgl_batch_send(const vx3dn_packet *p,uint32_t n,uint32_t *done) {
 uint32_t k,rc;static vx_d3d_state now;host_batches++;
 if(host_lose){uint32_t j;host_lose=0;hq_out+=n;*done=0;for(j=0;j<n;j++){hp_ran();hp[(hp_out-1)%1024].call=1;}return VX3DN_LOST;}
 if(!vxgl_seq_ring)host_done_call=host_call;host_call++; /* v10.366: this call retires the one before (v10.378: not with the VxD's ring) */
 now=cur->core.state;
 for(k=0;k<n;k++){
  if(thefb.locked||thefb.hold)host_held_submits++;
  cur->core.state=hq_state[hq_out++%VX3DN_BATCH_MAX];hp_ran();rc=host_draw(p+k);
  if(rc){cur->core.state=now;*done=k;return rc;}host_batched++;}
 cur->core.state=now;*done=n;return 0;
}
uint32_t vxgl_submit(void *ctx,const vx3dn_packet *p) {
 if(ctx){vxgl_fb *f=((vxgl_ctx*)ctx)->fb;fb_settle_only(f);if(f&&(f->locked||f->hold))host_held_submits++;} /* v10.267: as the ICD's */
 if(ctx&&((vxgl_ctx*)ctx)->batch_lost!=vxgl_batch_lost){((vxgl_ctx*)ctx)->batch_lost=vxgl_batch_lost;return VX3DN_LOST;} /* v10.279: as the ICD's */
 hq_state[hq_in++%VX3DN_BATCH_MAX]=cur->core.state; /* v10.275: in before the add (a full queue is sent by it) */
 hp_add(p); /* v10.366 */
 if(vxgl_batch_add(p))return 0;
 hq_in--;
 vxgl_batch_flush_why(1);
 {uint32_t rc;if(!vxgl_seq_ring)host_done_call=host_call;host_call++;hp_ran();rc=host_draw(p);vxgl_seq_alone(rc);return rc;}
}
/* v10.293: VXGL_REGDUMP: every draw packet also run through the emitter (vx3dn_execute) with a recording io; for each (type, register) the
 * phase that last wrote it and its value, and which registers each phase wrote at all (phase 1 VXTEX's cases, phase 2 VXSTEN's) */
static int rd_on,rd_phase;static uint32_t rd_type,rd_data;static unsigned char rd_w[3][256][256];static uint32_t rd_v[3][256][256];
static uint32_t rd_read(void *c,uint32_t r){(void)c;(void)r;return 0;}
static void rd_write(void *c,uint32_t r,uint32_t v){(void)c;
 if(r==0x43C){rd_data=(v>>24)!=0||((v>>16)&0xFF)==0xFE;rd_type=(v>>16)&0xFF;return;}
 if(r==0x440&&!rd_data){uint32_t g=v>>24;rd_w[rd_phase][rd_type][g]=1;rd_v[rd_phase][rd_type][g]=v&0xFFFFFF;}}
static uint32_t rd_ms(void *c){(void)c;return 0;}
static uint32_t rd_fb(void *c,uint32_t r){(void)c;(void)r;return 0;}
static void rd_wfb(void *c,uint32_t r,uint32_t v){(void)c;(void)r;(void)v;}
static void rd_packet(const vx3dn_packet *p){vx3dn_io io;uint32_t last=0;if(!rd_on||p->op!=VX3DN_DRAW)return;memset(&io,0,sizeof(io));io.read=rd_read;io.write=rd_write;io.read_fb=rd_fb;io.write_fb=rd_wfb;io.milliseconds=rd_ms;
 rd_data=1;(void)vx3dn_execute(p,&io,&last);}
static uint32_t pk_hash=2166136261U,pre_refused; /* v10.500; v10.501 */
static int host_nodraw; /* v10.482: VXGL_BENCH: the packets validated, not rasterized */
static uint32_t host_draw(const vx3dn_packet *p) {
 vx3dn_info i;uint32_t rc;
 rd_packet(p);
 memset(&i,0,sizeof(i));i.cb=sizeof(i);i.abi=VX3DN_ABI;i.ready=1;i.generation=host_gen;i.fb_bytes=sizeof(vram);i.primary_bytes=0x100000;i.width=W;i.height=H;i.pitch=W*4;i.bpp=32;
 rc=vx3dn_validate(p,&i);
 packets++;
 if(rc){printf("  packet refused by the VxD's validation: %u (op %u, %u vertices)\n",rc,p->op,p->vertex_count);{uint32_t k_;printf("  bursts %u prim %u dither %u pre %u:",p->bursts.n,p->ext.prim,p->ext.dither,p->pre.on);for(k_=0;k_<p->bursts.n;k_++)printf(" %lx",(unsigned long)p->bursts.b[k_]);printf("\n");}return rc;}
 if(getenv("VXGL_PKDUMP")&&p->op==VX3DN_DRAW){uint32_t v;printf("draw packet %u vertices, prim %u, bursts %u:",p->vertex_count,p->ext.prim,p->bursts.n);for(v=0;v<p->vertex_count&&v<64;v++){float x,y;memcpy(&x,&p->vertices[v].x,4);memcpy(&y,&p->vertices[v].y,4);printf(" (%.3f,%.3f)",x,y);}printf("\n");}
 if(p->op==VX3DN_DRAW){uint32_t v,k_; /* v10.500: a hash of every draw packet's vertices and bursts (VXGL_BENCH prints it: a switch's packets the same) */
  #define PH(x) do{const uint8_t *b_=(const uint8_t*)&(x);size_t n_;for(n_=0;n_<sizeof(x);n_++)pk_hash=(pk_hash^b_[n_])*16777619U;}while(0)
  PH(p->vertex_count);PH(p->bursts.n);for(k_=0;k_<p->bursts.n;k_++)PH(p->bursts.b[k_]);PH(p->pre.on);
  if(p->pre.on)for(v=0;v<p->vertex_count*VX3DN_PRE_STRIDE(p);v++)PH(p->pre.w[v]);
  else for(v=0;v<p->vertex_count;v++){PH(p->vertices[v]);PH(p->tex.uv[v]);PH(p->stages.uv1[v]);PH(p->shader.spec[v]);PH(p->stages.q[v]);PH(p->stages.r1[v]);}
  #undef PH
 }
 if(p->op==VX3DN_DRAW&&p->pre.on&&!host_nodraw){vx3dn_io io;uint32_t last=0,rc2,od=rd_data,op=rd_phase; /* v10.501: a prepacked or indexed packet also through the VxD's emitter (registers to the stubs): its form refused there would be lost on the board */
  memset(&io,0,sizeof(io));io.read=rd_read;io.write=rd_write;io.read_fb=rd_fb;io.write_fb=rd_wfb;io.milliseconds=rd_ms;rd_phase=0;
  rc2=vx3dn_execute(p,&io,&last);rd_data=od;rd_phase=op;
  if(rc2==VX3DN_BAD_PACKET){printf("  prepacked packet refused by the VxD's emitter (%u vertices, pre %u)\n",p->vertex_count,p->pre.on);pre_refused++;}}
 if(!host_nodraw)model(p);return 0;
}

/* ------------------------------------------------------------------ the test */
#include "../../src/vx_gl_scenes.h"
#include "../../src/vx_qscenes.h" /* v10.500 */
#define GXW 320
#define GXH 240
#define GXVW W
#define GXVH H
static int glx_notex;
#define GLX_NOTEX glx_notex
#include "../../src/vx_glix_scenes.h" /* .573 */
extern void APIENTRY glPointParameterfv(GLenum,const GLfloat*); /* v10.300: the attenuated points scene */
static uint8_t img[2][W*H*4];
extern int vxgl_destroy_context(vxgl_ctx *c);
extern void APIENTRY glStencilFuncSeparate(GLenum,GLenum,GLint,GLuint);
extern void APIENTRY glStencilOpSeparate(GLenum,GLenum,GLenum,GLenum);
/* v10.249: checks the scenes cannot make (both paths would agree on a wrong answer): the front and back buffers kept apart, alpha
 * read as 1.0 from an X8R8G8B8 surface, and front/back stencil state set apart drawn by swrast, each with the engine on and off */
static void *host_gp(const char *n){return (void*)_glapi_get_proc_address(n);} /* v10.266: VXTEX's entry points */
/* v10.488: the SSE pass check's geometry (VXFLAT-like strips), compiled into a list or drawn as it is */
static void sp_geom(int cs) {
 int q,k;
      if(cs==2)glColor3f(0.8f,0.6f,0.3f);
      for(q=0;q<(cs==2?6:2);q++){glBegin(GL_TRIANGLE_STRIP);for(k=0;k<40;k++){
        if(cs==1||cs==4)glColor4f(0.3f+(k%5)*0.1f,0.6f,0.3f+(k%3)*0.2f,(k&1)?1.0f:0.5f);
        glNormal3f(0.6f*(float)sin(k*0.4+q),0.5f*(float)cos(k*0.3),cs==3?2.5f:1.0f);glTexCoord2f((k%4)*0.25f,(k&1)?0.75f:0.25f);
        if(cs==2)glVertex3f(-25.0f+(float)(k/2)*5.9f+q*1.3f+((q&1)&&(k&1)?3.0f:0.0f),((k&1)?9.5f:1.25f)+q*10.3f-(q==5?14.0f:0.0f),(float)(k%9)/10.0f-0.4f);
        else if(cs==5)glVertex3f(5000.0f+(float)(k/2)*3.1f,(k&1)?50.5f:8.25f,(float)(k%9)/10.0f-0.4f);
        else if(cs==6&&q==1&&k==9)glVertex3f(30.0f,30.0f,40.0f); /* the light's position: d2 0, Mesa's sqrt path */
        else glVertex3f(2.0f+(float)(k/2)*3.1f+q*0.7f,((k&1)?50.5f:8.25f)-q*4.0f,(float)(k%9)/10.0f-0.4f);}glEnd();}
}
static int checks(void) {
 int pass,fails=0;
 for(pass=0;pass<2;pass++) {
  vxgl_ctx *c;GLubyte px[4],st=0;const char *who=pass?"engine off":"engine on";
  memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
  vxgl_hw_enabled=pass==0;c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1)){printf("checks: no context\n");return 1;}
  cur=c;reset();
  if(pass==0){ /* v10.301 (review): a failed depth-write notice is sent again before the next engine draw, not lost */
   unsigned long w0; /* v10.308: swrast chosen by a fragment program (sw_on): the engine stipples polygons now */
   glEnable(GL_DEPTH_TEST);scene_fp=0;sw_on();host_z_fail=1;w0=host_z_written;
   glBegin(GL_TRIANGLES);glVertex3f(10,10,0.5f);glVertex3f(100,10,0.5f);glVertex3f(50,90,0.5f);glEnd();glFlush(); /* swrast writes depth: the notice fails */
   if(host_z_written!=w0||!thefb.zdirty){printf("check FAIL: the failed depth-write notice was not kept (zdirty %d)\n",thefb.zdirty);fails++;}
   { /* v10.342 (review): the notice failing several times: each depth-tested draw goes to software until it goes through */
    GLuint sw0=c->stats[3],hw0=c->stats[0];int k;
    sw_off();host_z_fail=1000;
    for(k=0;k<3;k++){glBegin(GL_TRIANGLES);glVertex3f(10,10,0);glVertex3f(100,10,0);glVertex3f(50,90,0);glEnd();glFlush();}
    if(c->stats[0]!=hw0||c->stats[3]!=sw0+3||!thefb.zdirty){printf("check FAIL: depth-tested draws while the notice kept failing: %u by the engine (want 0), %u in software (want 3)\n",c->stats[0]-hw0,c->stats[3]-sw0);fails++;}
    hw0=c->stats[0];glDisable(GL_DEPTH_TEST);glBegin(GL_TRIANGLES);glVertex3f(10,10,0);glVertex3f(100,10,0);glVertex3f(50,90,0);glEnd();glFlush();glEnable(GL_DEPTH_TEST); /* no depth test: the engine */
    if(c->stats[0]!=hw0+1){printf("check FAIL: a draw without the depth test went to software while the notice failed\n");fails++;}
    host_z_fail=0;w0=host_z_written;}
   glBegin(GL_TRIANGLES);glVertex3f(10,10,0);glVertex3f(100,10,0);glVertex3f(50,90,0);glEnd();glFlush(); /* the engine draws: told first */
   if(host_z_written!=w0+1||thefb.zdirty){printf("check FAIL: the depth-write notice was not sent before the engine's draw\n");fails++;}
   glDisable(GL_DEPTH_TEST);host_z_fail=0;}
  if(pass==0){ /* v10.302 (review): glFinish waits for the engine without locking the surfaces (the depth lock dropped Coarse Z's records) */
   unsigned long l0,e0;glBegin(GL_TRIANGLES);glVertex2f(10,10);glVertex2f(100,10);glVertex2f(50,90);glEnd();l0=host_real_locks;e0=host_engine_waits;glFinish();
   if(host_real_locks!=l0||host_engine_waits!=e0+1){printf("check FAIL: glFinish locked the surfaces (%lu locks, %lu waits)\n",host_real_locks-l0,host_engine_waits-e0);fails++;}}
  if(pass==0){ /* v10.302 (review): other alpha factors (glBlendFuncSeparate) on a target without alpha stay on the engine */
   void (APIENTRY *bfs)(GLenum,GLenum,GLenum,GLenum)=(void (APIENTRY *)(GLenum,GLenum,GLenum,GLenum))host_gp("glBlendFuncSeparateEXT");
   if(bfs){glEnable(GL_BLEND);bfs(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_ONE,GL_ZERO);glBegin(GL_TRIANGLES);glVertex2f(10,10);glVertex2f(100,10);glVertex2f(50,90);glEnd();glFlush();
    if(c->fallback&VXGL_FB_BLEND){printf("check FAIL: separate alpha factors went to software with no alpha bits\n");fails++;}
    bfs(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA,GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDisable(GL_BLEND);glFinish();}
   else {printf("check FAIL: no glBlendFuncSeparateEXT\n");fails++;}}
  if(pass==0&&scene_fbo_gen){ /* v10.321: render to texture on the engine; the image read back for glGetTexImage and before a glTexSubImage2D;
   ACCEL_OPENGL_RTT = 0 draws it in software; a mode change while the copy holds the drawing loses it without harm */
   int r;for(r=0;r<2;r++){GLuint fb,t;static GLubyte img2[16*16*4],sub[4*4*4];int i,okd,oks;vxgl_gl_rtt=!r;
    t=rtt_tex(16,16,GL_RGB);scene_fbo_gen(1,&fb);scene_fbo_bind(GL_FRAMEBUFFER_EXT,fb);scene_fbo_tex2d(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,t,0);
    rtt_view(16,16);glColor3f(1,0,0);glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(16,0);glVertex2f(16,16);glVertex2f(0,16);glEnd();glFlush();
    if(c->rtt!=!r||(!r&&c->fallback)){printf("check FAIL: ACCEL_OPENGL_RTT %d: the engine %s the framebuffer object (fallback %X)\n",!r,c->rtt?"drew":"did not draw",c->fallback);fails++;}
    for(i=0;i<16*4;i++)sub[i]=(GLubyte)((i&3)==1?255:(i&3)==3?255:0); /* green */
    glBindTexture(GL_TEXTURE_2D,t);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexSubImage2D(GL_TEXTURE_2D,0,4,4,4,4,GL_RGBA,GL_UNSIGNED_BYTE,sub);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,img2);
    okd=img2[0]==255&&img2[1]==0&&img2[2]==0;oks=img2[(5*16+5)*4]==0&&img2[(5*16+5)*4+1]==255;
    if(!okd||!oks){printf("check FAIL: ACCEL_OPENGL_RTT %d: texel 0,0 %02X%02X%02X (want red, the drawing), 5,5 %02X%02X%02X (want green, the TexSubImage)\n",!r,img2[0],img2[1],img2[2],img2[84],img2[85],img2[86]);fails++;}
    if(!r){glColor3f(0,0,1);glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(16,0);glVertex2f(16,16);glVertex2f(0,16);glEnd();glFlush();host_tex_lose++;c->new_state|=~0U;
     glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,img2);glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(16,0);glVertex2f(16,16);glVertex2f(0,16);glEnd();glFinish();}
    scene_fbo_bind(GL_FRAMEBUFFER_EXT,0);scene_fbo_del(1,&fb);glDeleteTextures(1,&t);rtt_view(W,H);glColor3f(1,1,1);}
   vxgl_gl_rtt=1;}
  if(pass==0&&scene_fbo_gen){ /* v10.321 (review): drawing into level 1 of a texture sampled at level 0 (its copy made again for the sampling):
   drawn into the copy as it is now, or by software; and a framebuffer object the engine cannot draw leaves the window's surfaces on the core */
   static GLubyte red[64*64*4],green[32*32*4],got[32*32*4];GLuint t,t2,fb,fb2,rb;int i;
   for(i=0;i<64*64;i++){red[i*4]=255;red[i*4+1]=0;red[i*4+2]=0;red[i*4+3]=255;}for(i=0;i<32*32;i++){green[i*4]=0;green[i*4+1]=255;green[i*4+2]=0;green[i*4+3]=255;}
   glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,red);glTexImage2D(GL_TEXTURE_2D,1,GL_RGB,32,32,0,GL_RGBA,GL_UNSIGNED_BYTE,green);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,0);
   scene_fbo_gen(1,&fb);scene_fbo_bind(GL_FRAMEBUFFER_EXT,fb);scene_fbo_tex2d(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,t,1);
   rtt_view(32,32);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);quad_tc(0,0,32,32,0,0,1,1);glDisable(GL_TEXTURE_2D);glFlush();
   glGetTexImage(GL_TEXTURE_2D,1,GL_RGBA,GL_UNSIGNED_BYTE,got);
   if(got[0]!=255||got[1]||got[(31*32+31)*4]!=255){printf("check FAIL: level 1 drawn from level 0 of the same texture: %02X%02X%02X\n",got[0],got[1],got[2]);fails++;}
   /* A: RGB + D16, drawn; B: an RGBA texture (software); then the window */
   t2=rtt_tex(32,32,GL_RGBA);scene_rb_gen(1,&rb);scene_rb_bind(GL_RENDERBUFFER_EXT,rb);scene_rb_storage(GL_RENDERBUFFER_EXT,GL_DEPTH_COMPONENT16,32,32);
   scene_fbo_rb(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,rb);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);quad_tc(0,0,32,32,0,0,1,1);glFlush();
   scene_fbo_gen(1,&fb2);scene_fbo_bind(GL_FRAMEBUFFER_EXT,fb2);scene_fbo_tex2d(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,t2,0);quad_tc(0,0,32,32,0,0,1,1);glFlush();
   scene_fbo_bind(GL_FRAMEBUFFER_EXT,0);glEnable(GL_DEPTH_TEST);quad_tc(0,0,8,8,0,0,1,1);glFlush();glDisable(GL_DEPTH_TEST);
   if(memcmp(&c->core.depth,&thefb.sdepth,sizeof(vx3dn_surface))||memcmp(&c->core.target,&thefb.sback,sizeof(vx3dn_surface))){printf("check FAIL: the window drew with a framebuffer object's surfaces on the core\n");fails++;}
   scene_fbo_del(1,&fb2);scene_fbo_del(1,&fb);scene_rb_del(1,&rb);glDeleteTextures(1,&t2);glDeleteTextures(1,&t);rtt_view(W,H);}
  if(pass==0) { /* v10.323: a triangle strip and a quad strip of 40 vertices each go to the engine as one strip run (one vertex a triangle);
   ACCEL_OPENGL_STRIPS 0 sends the triangles one by one as before; the pixels are the same either way */
   extern uint32_t vx_d3d_strip_vertices;static GLubyte img_a[64*64*4],img_b[64*64*4];int r,q;
   for(q=0;q<2;q++)for(r=0;r<2;r++){int d0,k;uint32_t s0;vxgl_gl_strips=!r;glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glFinish();d0=draw_packets;s0=vx_d3d_strip_vertices;
    glBegin(q?GL_QUAD_STRIP:GL_TRIANGLE_STRIP);for(k=0;k<40;k++){glColor3f((k%7)/7.0f,(k%5)/5.0f,(k%3)/3.0f);glVertex2f(2.0f+(float)(k/2)*3.1f,(k&1)?50.5f:8.25f);}glEnd();glFinish();
    glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,r?img_b:img_a);
    if(!r&&(draw_packets-d0!=1||vx_d3d_strip_vertices-s0<36)){printf("check FAIL: a %s strip of 40 vertices: %d packets, %u vertices added to the run (want 1, 36 or more: the first triangle may build the template)\n",q?"quad":"triangle",draw_packets-d0,(unsigned)(vx_d3d_strip_vertices-s0));fails++;}
    if(r&&memcmp(img_a,img_b,sizeof(img_a))){printf("check FAIL: a %s strip drew other pixels than the triangle-by-triangle path\n",q?"quad":"triangle");fails++;}}
   vxgl_gl_strips=1;
   /* v10.480: flat shading: a strip whose vertices share one colour goes to the engine as one strip run, the same pixels as Mesa's flat
    template (ACCEL_OPENGL_FLATSTRIP 0); a strip whose colours differ stays with the template (counted) */
   glShadeModel(GL_FLAT);
   for(q=0;q<2;q++)for(int dif=0;dif<2;dif++)for(r=0;r<2;r++){int d0,k;uint32_t s0,f0=vxgl_flat_stat[0],f2=vxgl_flat_stat[2],f4=vxgl_flat_stat[4];vxgl_gl_flatstrips=!r;vxgl_gl_flatbatch=!r; /* v10.481: r 1 = v10.479's template for every flat strip */glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glFinish();d0=draw_packets;s0=vx_d3d_strip_vertices;
    glBegin(q?GL_QUAD_STRIP:GL_TRIANGLE_STRIP);glColor3f(0.25f,0.75f,0.5f);for(k=0;k<40;k++){if(dif)glColor3f((k%7)/7.0f,(k%5)/5.0f,(k%3)/3.0f);glVertex2f(2.0f+(float)(k/2)*3.1f,(k&1)?50.5f:8.25f);}glEnd();glFinish();
    glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,r?img_b:img_a);
    if(!r&&!dif&&(draw_packets-d0!=1||vx_d3d_strip_vertices-s0<36||vxgl_flat_stat[0]!=f0+1)){printf("check FAIL: a flat %s strip of one colour: %d packets, %u strip vertices, %u sent whole (want 1, 36 or more, 1)\n",q?"quad":"triangle",draw_packets-d0,(unsigned)(vx_d3d_strip_vertices-s0),(unsigned)(vxgl_flat_stat[0]-f0));fails++;}
    if(!r&&dif&&(vxgl_flat_stat[2]!=f2+1||vxgl_flat_stat[0]!=f0||vxgl_flat_stat[4]!=f4+1)){printf("check FAIL: a flat %s strip of differing colours not sent as independent triangles (v10.481)\n",q?"quad":"triangle");fails++;}
    if(r&&memcmp(img_a,img_b,sizeof(img_a))){printf("check FAIL: a flat %s strip (%s) drew other pixels than Mesa's flat template\n",q?"quad":"triangle",dif?"colours differ":"one colour");fails++;}}
   vxgl_gl_flatstrips=1;vxgl_gl_flatbatch=1;
   /* v10.481: lit, fogged flat strips with a secondary colour (Lattice's kind): the batched path, the template, and the hardwired emit against
    Mesa's generic one draw the same pixels */
   {extern int _tnl_vx855_emit;extern void _tnl_vx855_reemit(GLcontext *);static GLubyte img_c[64*64*4];static const GLfloat ld[4]={0.3f,0.5f,1.0f,0.0f},fc[4]={0.2f,0.3f,0.4f,1};int m;
    glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glLightfv(GL_LIGHT0,GL_POSITION,ld);glEnable(GL_COLOR_MATERIAL);glEnable(GL_FOG);glFogi(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_START,-1);glFogf(GL_FOG_END,1);glFogfv(GL_FOG_COLOR,fc);
    glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SEPARATE_SPECULAR_COLOR);{static const GLfloat sp[4]={0.6f,0.6f,0.6f,1};glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,sp);glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,8);glLightfv(GL_LIGHT0,GL_SPECULAR,sp);}
    for(m=0;m<3;m++){int k;uint32_t f4=vxgl_flat_stat[4];vxgl_gl_flatbatch=m!=1;_tnl_vx855_emit=m!=2;_tnl_vx855_reemit(&cur->gl);
     glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glColor3f(0.8f,0.6f,0.3f);
     glBegin(GL_TRIANGLE_STRIP);for(k=0;k<40;k++){glNormal3f(0.6f*(float)sin(k*0.4),0.5f*(float)cos(k*0.3),1.0f);glVertex3f(2.0f+(float)(k/2)*3.1f,(k&1)?50.5f:8.25f,(float)(k%9)/10.0f-0.4f);}glEnd();glFinish();
     glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,m==0?img_a:m==1?img_b:img_c);
     {extern void _tnl_generic_emit(GLcontext *,GLuint,GLubyte *);void *em=(void*)TNL_CONTEXT(&cur->gl)->clipspace.emit;if((m!=2)!=(em!=(void*)_tnl_generic_emit)){printf("check FAIL: the lit, fogged strip's emit (setting %d): %s\n",m,em==(void*)_tnl_generic_emit?"Mesa's generic":"another");fails++;}}
     if(m!=1&&vxgl_flat_stat[4]!=f4+1){printf("check FAIL: a lit, fogged flat strip not batched (setting %d)\n",m);fails++;}}
    if(memcmp(img_a,img_b,sizeof(img_a))){printf("check FAIL: a lit, fogged flat strip: the batched path drew other pixels than the template\n");fails++;}
    if(memcmp(img_a,img_c,sizeof(img_a))){printf("check FAIL: a lit, fogged flat strip: the hardwired emit drew other pixels than Mesa's generic one\n");fails++;}
    {int nz=0,i;for(i=0;i<64*64;i++)if(img_a[i*4]|img_a[i*4+1]|img_a[i*4+2])nz++;if(nz<500){printf("check FAIL: the lit, fogged strip drew %d pixels\n",nz);fails++;}}
    /* v10.481: the engine's flat shading (ACCEL_OPENGL_HWFLAT): the same pixels from triangles turned to start at their provoking vertex */
    {int k;static GLubyte img_d[64*64*4];uint32_t f4=vxgl_flat_stat[4];vx_d3d_gl_hwflat=1;vxgl_gl_flatbatch=1;vxgl_gl_hwflatstrip=0;cur->new_state|=VXGL_STATE_MASK;
     glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glColor3f(0.8f,0.6f,0.3f);
     glBegin(GL_TRIANGLE_STRIP);for(k=0;k<40;k++){glNormal3f(0.6f*(float)sin(k*0.4),0.5f*(float)cos(k*0.3),1.0f);glVertex3f(2.0f+(float)(k/2)*3.1f,(k&1)?50.5f:8.25f,(float)(k%9)/10.0f-0.4f);}glEnd();glFinish();
     glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,img_d);
     if(vxgl_flat_stat[4]!=f4+1||cur->core.state.rs[9]!=1){printf("check FAIL: the engine's flat shading not used (batched %u, SHADEMODE %u)\n",(unsigned)(vxgl_flat_stat[4]-f4),(unsigned)cur->core.state.rs[9]);fails++;}
     if(memcmp(img_a,img_d,sizeof(img_a))){int i,d=0;for(i=0;i<64*64*4;i++)if(img_a[i]!=img_d[i])d++;printf("check FAIL: a lit, fogged flat strip on the engine's flat shading: %d bytes differ from the template\n",d);fails++;}
     /* v10.482: ACCEL_OPENGL_HWFLATSTRIP 1 (the model's rule for flat strips: each triangle its first strip vertex): one strip, the same pixels */
     {uint32_t d0=draw_packets,sv0=vx_d3d_strip_vertices;vxgl_gl_hwflatstrip=1;glClear(GL_COLOR_BUFFER_BIT);
      glBegin(GL_TRIANGLE_STRIP);for(k=0;k<40;k++){glNormal3f(0.6f*(float)sin(k*0.4),0.5f*(float)cos(k*0.3),1.0f);glVertex3f(2.0f+(float)(k/2)*3.1f,(k&1)?50.5f:8.25f,(float)(k%9)/10.0f-0.4f);}glEnd();glFinish();
      glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,img_d);vxgl_gl_hwflatstrip=1;
      if(vx_d3d_strip_vertices-sv0<30||draw_packets-d0>2){printf("check FAIL: a flat strip on the engine's strips: %u strip vertices, %d packets\n",(unsigned)(vx_d3d_strip_vertices-sv0),draw_packets-d0);fails++;}
      if(memcmp(img_a,img_d,sizeof(img_a))){int i,d=0;for(i=0;i<64*64*4;i++)if(img_a[i]!=img_d[i])d++;printf("check FAIL: a lit, fogged flat strip as the engine's strip (rule 1): %d bytes differ from the template\n",d);fails++;}}
     cur->new_state|=VXGL_STATE_MASK;}
    /* v10.483: a lit, fogged flat quad strip as the engine's strip (rule 1, pairs swapped): one strip, the template's pixels */
    {int k,r;static GLubyte img_q[2][64*64*4];
     for(r=0;r<2;r++){uint32_t sv0=vx_d3d_strip_vertices,f4=vxgl_flat_stat[4];vx_d3d_gl_hwflat=!r;vxgl_gl_flatbatch=!r;vxgl_gl_flatstrips=!r;vxgl_gl_hwflatstrip=!r;cur->new_state|=VXGL_STATE_MASK;
      glClear(GL_COLOR_BUFFER_BIT);glBegin(GL_QUAD_STRIP);for(k=0;k<40;k++){glNormal3f(0.6f*(float)sin(k*0.4),0.5f*(float)cos(k*0.3),1.0f);glVertex3f(2.0f+(float)(k/2)*3.1f,(k&1)?50.5f:8.25f,(float)(k%9)/10.0f-0.4f);}glEnd();glFinish();
      glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,img_q[r]);
      if(!r&&(vx_d3d_strip_vertices-sv0<30||vxgl_flat_stat[4]!=f4+1)){printf("check FAIL: a lit flat quad strip on the engine's strips: %u strip vertices, %u batched\n",(unsigned)(vx_d3d_strip_vertices-sv0),(unsigned)(vxgl_flat_stat[4]-f4));fails++;}}
     if(memcmp(img_q[0],img_q[1],sizeof(img_q[0]))){int i,d=0;for(i=0;i<64*64*4;i++)if(img_q[0][i]!=img_q[1][i])d++;printf("check FAIL: a lit, fogged flat quad strip as the engine's strip (rule 1): %d bytes differ from the template\n",d);fails++;}
     vx_d3d_gl_hwflat=1;vxgl_gl_flatbatch=1;vxgl_gl_flatstrips=1;vxgl_gl_hwflatstrip=1;cur->new_state|=VXGL_STATE_MASK;}
    /* v10.484: ACCEL_OPENGL_RSQRT: a positional light with a local viewer and attenuation (Mesa's general lighting), the SSE reciprocal
     square roots against the x87's: each channel within 1 */
    {extern int _mesa_vx855_rsqrt;static GLubyte img_r[2][64*64*4];static const GLfloat pos[4]={30.0f,30.0f,40.0f,1.0f};int k,r,i,d=0,mx=0;
     glLightfv(GL_LIGHT0,GL_POSITION,pos);glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.01f);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,1);glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SINGLE_COLOR);
     glFogi(GL_FOG_MODE,GL_EXP);glFogf(GL_FOG_DENSITY,0.5f);
     for(r=0;r<2;r++){_mesa_vx855_rsqrt=r;glClear(GL_COLOR_BUFFER_BIT);glColor3f(0.8f,0.6f,0.3f);
      glBegin(GL_TRIANGLE_STRIP);for(k=0;k<40;k++){glNormal3f(0.6f*(float)sin(k*0.4),0.5f*(float)cos(k*0.3),1.0f);glVertex3f(2.0f+(float)(k/2)*3.1f,(k&1)?50.5f:8.25f,(float)(k%9)/10.0f-0.4f);}glEnd();glFinish();
      glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,img_r[r]);}
     _mesa_vx855_rsqrt=0;glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.0f);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,0);glFogi(GL_FOG_MODE,GL_LINEAR);
     for(i=0;i<64*64*4;i++){int e=abs(img_r[0][i]-img_r[1][i]);if(e){d++;if(e>mx)mx=e;}}
     {int nz=0;for(i=0;i<64*64;i++)if(img_r[1][i*4]|img_r[1][i*4+1]|img_r[1][i*4+2])nz++;
      printf("rsqrt check: a positional light, local viewer, attenuation: %d of %d lit pixels' channels differ (at most %d)%s\n",d,nz,mx,mx>1||nz<500?"  FAIL":"");if(mx>1||nz<500)fails++;}}
    /* v10.485: ACCEL_OPENGL_LIGHTFAST: one positional light (local viewer, attenuation, colour material, exponential fog) in the straight loop
     against light_rgba: each channel within 1; and ACCEL_OPENGL_CLIPSTRIP: lit flat strips (and smooth ones) crossing the window's edges,
     split (runs inside as strips, the clipped polygons gathered) against Mesa's clipping table: the same pixels */
    {extern int _mesa_vx855_lightfast,_tnl_vx855_clipstrip;extern unsigned long _tnl_vx855_clipstrip_stat[5];static GLubyte img_l[2][64*64*4];
     static const GLfloat pos[4]={30.0f,30.0f,40.0f,1.0f};int k,r,i,d=0,mx=0,sm;
     glLightfv(GL_LIGHT0,GL_POSITION,pos);glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.01f);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,1);glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SINGLE_COLOR);
     glFogi(GL_FOG_MODE,GL_EXP);glFogf(GL_FOG_DENSITY,0.5f);
     {int lc;extern unsigned long _tnl_vx855_light_stat[8];
     extern int vxgl_ssepass_on;int sp_was=vxgl_ssepass_on;vxgl_ssepass_on=0; /* v10.489: the loop itself (the pass would take these draws) */
     for(lc=0;lc<3;lc++){unsigned long l0=_tnl_vx855_light_stat[0];d=0;mx=0;
      /* v10.486: 0 one positional light; 1 and a directional light and a spotlight; 2 the same without the local viewer */
      if(lc==1){static const GLfloat dir1[4]={-0.4f,0.3f,1.0f,0.0f},pos2[4]={20.0f,40.0f,30.0f,1.0f},sd[3]={0.2f,-0.3f,-1.0f},c1[4]={0.5f,0.4f,0.3f,1},c2[4]={0.3f,0.6f,0.5f,1};
       glEnable(GL_LIGHT1);glLightfv(GL_LIGHT1,GL_POSITION,dir1);glLightfv(GL_LIGHT1,GL_DIFFUSE,c1);glLightfv(GL_LIGHT1,GL_SPECULAR,c1);
       glEnable(GL_LIGHT2);glLightfv(GL_LIGHT2,GL_POSITION,pos2);glLightfv(GL_LIGHT2,GL_DIFFUSE,c2);glLightfv(GL_LIGHT2,GL_SPECULAR,c2);glLightfv(GL_LIGHT2,GL_SPOT_DIRECTION,sd);glLightf(GL_LIGHT2,GL_SPOT_CUTOFF,40.0f);glLightf(GL_LIGHT2,GL_SPOT_EXPONENT,6.0f);}
      if(lc==2)glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,0);
     for(r=0;r<2;r++){_mesa_vx855_lightfast=!r;glClear(GL_COLOR_BUFFER_BIT);
      glBegin(GL_TRIANGLE_STRIP);for(k=0;k<40;k++){glColor3f(0.3f+(k%5)*0.1f,0.6f,0.3f+(k%3)*0.2f);glNormal3f(0.6f*(float)sin(k*0.4),0.5f*(float)cos(k*0.3),1.0f);glVertex3f(2.0f+(float)(k/2)*3.1f,(k&1)?50.5f:8.25f,(float)(k%9)/10.0f-0.4f);}glEnd();glFinish();
      glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,img_l[r]);}
     _mesa_vx855_lightfast=1;
     for(i=0;i<64*64*4;i++){int e=abs(img_l[0][i]-img_l[1][i]);if(e){d++;if(e>mx)mx=e;}}
     {int nz=0;for(i=0;i<64*64;i++)if(img_l[0][i*4]|img_l[0][i*4+1]|img_l[0][i*4+2])nz++;
      printf("lightfast check (%s): %d of %d lit pixels' channels differ (at most %d), %lu runs of the loop%s\n",lc==0?"one positional light, local viewer, attenuation, colour material":lc==1?"and a directional light and a spotlight":"the same, no local viewer",
       d,nz,mx,_tnl_vx855_light_stat[0]-l0,mx>1||nz<500||_tnl_vx855_light_stat[0]==l0?"  FAIL":"");if(mx>1||nz<500||_tnl_vx855_light_stat[0]==l0)fails++;}}
     glDisable(GL_LIGHT1);glDisable(GL_LIGHT2);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,1);vxgl_ssepass_on=sp_was;}
     for(sm=0;sm<2;sm++){unsigned long st0[5];int q;glShadeModel(sm?GL_SMOOTH:GL_FLAT);cur->new_state|=VXGL_STATE_MASK;
      for(r=0;r<2;r++){uint32_t f6=vxgl_flat_stat[6];_tnl_vx855_clipstrip=!r;for(q=0;q<5;q++)st0[q]=_tnl_vx855_clipstrip_stat[q];glClear(GL_COLOR_BUFFER_BIT);
       for(q=0;q<6;q++){glBegin(GL_TRIANGLE_STRIP);for(k=0;k<40;k++){glColor3f(0.3f+(k%5)*0.1f,0.6f,0.3f+(k%3)*0.2f);glNormal3f(0.6f*(float)sin(k*0.4+q),0.5f*(float)cos(k*0.3),1.0f);
        glVertex3f(-25.0f+(float)(k/2)*5.9f+q*1.3f+((q&1)&&(k&1)?3.0f:0.0f),((k&1)?9.5f:1.25f)+q*10.3f-(q==5?14.0f:0.0f),(float)(k%9)/10.0f-0.4f);}glEnd();}
       glFinish();glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,img_l[r]);
       if(!r&&(_tnl_vx855_clipstrip_stat[0]-st0[0]<6||_tnl_vx855_clipstrip_stat[1]-st0[1]<6||_tnl_vx855_clipstrip_stat[4]-st0[4]<6)){printf("check FAIL: clipped %s strips not split: %lu split, %lu runs, %lu clipped\n",sm?"smooth":"flat",
        _tnl_vx855_clipstrip_stat[0]-st0[0],_tnl_vx855_clipstrip_stat[1]-st0[1],_tnl_vx855_clipstrip_stat[4]-st0[4]);fails++;}
       if(!r)printf("clipstrip check (%s): %lu strips split, %lu runs as strips (%lu triangles), %lu alone, %lu clipped, %u clipped polygons gathered\n",sm?"smooth":"flat",_tnl_vx855_clipstrip_stat[0]-st0[0],_tnl_vx855_clipstrip_stat[1]-st0[1],
        _tnl_vx855_clipstrip_stat[2]-st0[2],_tnl_vx855_clipstrip_stat[3]-st0[3],_tnl_vx855_clipstrip_stat[4]-st0[4],(unsigned)(vxgl_flat_stat[6]-f6));
       if(!r&&(_tnl_vx855_clipstrip_stat[3]-st0[3]<1||(!sm&&vxgl_flat_stat[6]-f6<6))){printf("check FAIL: clipped strips: a triangle alone or the gathered polygons not exercised\n");fails++;}}
      _tnl_vx855_clipstrip=1;
      if(memcmp(img_l[0],img_l[1],sizeof(img_l[0]))){d=0;for(i=0;i<64*64*4;i++)if(img_l[0][i]!=img_l[1][i])d++;printf("check FAIL: clipped %s strips split: %d bytes differ from Mesa's clipping table\n",sm?"smooth":"flat",d);fails++;}}
     glShadeModel(GL_FLAT);cur->new_state|=VXGL_STATE_MASK;
     glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.0f);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,0);glFogi(GL_FOG_MODE,GL_LINEAR);}
    /* v10.487: ACCEL_OPENGL_SSEPASS: a display list's draw in one SSE pass (vxgl_ssepass.c) against the pipeline's stages, scene by scene:
     the pass must be taken (its count advances), the pixels the same within 1 in a channel (the SSE steps round each operation; on the
     host Mesa's own float arithmetic is SSE too, so the unlit scene must match exactly) */
    {extern int vxgl_ssepass_on;extern unsigned long vxgl_ssepass_stat[20];static GLubyte img_s[3][64*64*4];
     extern int vxgl_ssepass_box,vxgl_ssepass_shine,vxgl_ssepass_pack,vxgl_ssepass_cache,vxgl_ssepass_mask,vxgl_gl_flatnocopy,vxgl_ssepass_tight,_tnl_vx855_fastbind;extern uint32_t vx_d3d_gl_cullrun2;unsigned long n_keep=0;extern int vxgl_ssepass_fuse,vxgl_ssepass_accept,vxgl_ssepass_box2,vxgl_ssepass_partial,_vbo_vx855_listbind,vxgl_ssepass_immbox,vxgl_ssepass_snap2,vxgl_ssepass_outfast,vxgl_ssepass_early;extern unsigned long _vbo_vx855_listbind_n,vxgl_ssepass_imm_stat[3];unsigned long n_acc=0,n_fz=0,n_lb=0,n_ib0=vxgl_ssepass_imm_stat[1];extern uint32_t vx_d3d_gl_cullrun,vx_d3d_ps_used,vx_d3d_cullrun_tris;
     unsigned long n_box=0,n_ps=0,n_cr=0,n_sh=0,n_psf=0;int cu; /* v10.490: the pass's new parts (box, shine, pack, snap, the culling run) against v10.489's, exactly, with culling off, back, front */static GLubyte tex[2*2*4]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
     static const GLfloat pos[4]={30.0f,30.0f,40.0f,1.0f},dir1[4]={-0.4f,0.3f,1.0f,0.0f},c1[4]={0.5f,0.4f,0.3f,1};GLuint list=glGenLists(1),tx=0;int cs,k,r,i,q;
     static const char *const cn[8]={"lit (positional light, local viewer, attenuation, specular), fogged EXP, textured","unlit, colours per vertex, fogged LINEAR, textured",
      "lit with colour material (one colour in the list), fogged EXP2, strips crossing the window's edges","lit, GL_NORMALIZE, a directional light added, no local viewer, no fog",
      "lit, colours per vertex (alpha too) with colour material (a material a group)","lit, one colour, no fog, no texture, the whole list outside",
      "lit, a vertex at the light's position (its group of four by the scalar reference)","lit, a spotlight and a positional light (v10.488)"};
     glShadeModel(GL_SMOOTH);glGenTextures(1,&tx);glBindTexture(GL_TEXTURE_2D,tx);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,tex);
     glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
     for(cu=0;cu<5;cu++){glShadeModel(cu>=3?GL_FLAT:GL_SMOOTH); /* v10.490 (board, VXFLAT v2.0): 3, 4 flat shading (the engine's flat strips from a copy) */
      if(cu&&cu!=3){glEnable(GL_CULL_FACE);glCullFace(cu==1?GL_BACK:GL_FRONT);}else glDisable(GL_CULL_FACE);
     for(cs=0;cs<8;cs++){int im;for(im=0;im<2;im++){int lit=cs!=1,d=0,mx=0,nz=0,want_pass=cs!=1; /* .506: scene 5 lit (the pass takes lit draws only); scene 1 unlit: not the pass's */unsigned long p0,r0=vxgl_ssepass_stat[2];
      if(!im){glNewList(list,GL_COMPILE);sp_geom(cs);glEndList();}
      if(lit){glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glLightfv(GL_LIGHT0,GL_POSITION,pos);glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,cs==3?0.0f:0.01f);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,cs!=3);
       {static const GLfloat sp[4]={0.6f,0.6f,0.6f,1},df[4]={0.8f,0.6f,0.3f,1};glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,sp);glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,8);glLightfv(GL_LIGHT0,GL_SPECULAR,sp);glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE,df);}
       if(cs==2||cs==4)glEnable(GL_COLOR_MATERIAL);else glDisable(GL_COLOR_MATERIAL);
       if(cs==7){static const GLfloat p2[4]={20.0f,40.0f,30.0f,1.0f},sd[3]={0.2f,-0.3f,-1.0f},c2[4]={0.3f,0.6f,0.5f,1};glEnable(GL_LIGHT2);glLightfv(GL_LIGHT2,GL_POSITION,p2);glLightfv(GL_LIGHT2,GL_DIFFUSE,c2);
        glLightfv(GL_LIGHT2,GL_SPECULAR,c2);glLightfv(GL_LIGHT2,GL_SPOT_DIRECTION,sd);glLightf(GL_LIGHT2,GL_SPOT_CUTOFF,40.0f);glLightf(GL_LIGHT2,GL_SPOT_EXPONENT,6.0f);}else glDisable(GL_LIGHT2);
       if(cs==3){glEnable(GL_LIGHT1);glLightfv(GL_LIGHT1,GL_POSITION,dir1);glLightfv(GL_LIGHT1,GL_DIFFUSE,c1);glLightfv(GL_LIGHT1,GL_SPECULAR,c1);glEnable(GL_NORMALIZE);}else{glDisable(GL_LIGHT1);glDisable(GL_NORMALIZE);}}
      else{glDisable(GL_LIGHTING);glDisable(GL_COLOR_MATERIAL);glColor3f(0.8f,0.6f,0.3f);}
      if(cs==3||cs==5||cs==6)glDisable(GL_FOG);else{glEnable(GL_FOG);glFogi(GL_FOG_MODE,cs==1?GL_LINEAR:cs==2?GL_EXP2:GL_EXP);glFogf(GL_FOG_DENSITY,0.5f);glFogf(GL_FOG_START,-1);glFogf(GL_FOG_END,1);}
      if(cs==5)glDisable(GL_TEXTURE_2D);else glEnable(GL_TEXTURE_2D);
      for(r=0;r<3;r++){unsigned long i0=vxgl_ssepass_stat[8],b0=vxgl_ssepass_stat[10],h0=vxgl_ssepass_stat[13];uint32_t u0=vx_d3d_ps_used,c0=vx_d3d_cullrun_tris;int nw=r!=2;
       vxgl_ssepass_box=vxgl_ssepass_shine=vxgl_ssepass_pack=vxgl_ssepass_cache=vxgl_ssepass_mask=vxgl_gl_flatnocopy=vxgl_ssepass_tight=_tnl_vx855_fastbind=nw;vx_d3d_gl_cullrun=vx_d3d_gl_cullrun2=(uint32_t)nw;vxgl_ssepass_fuse=vxgl_ssepass_accept=vxgl_ssepass_box2=vxgl_ssepass_partial=_vbo_vx855_listbind=vxgl_ssepass_immbox=vxgl_ssepass_snap2=vxgl_ssepass_outfast=vxgl_ssepass_early=nw; /* v10.495; v10.496; v10.497 */
       {unsigned long k0=vxgl_ssepass_stat[15],a0=vxgl_ssepass_stat[16],f0=vxgl_ssepass_stat[17],l0=_vbo_vx855_listbind_n;
       vxgl_ssepass_on=r==1?0:5;p0=vxgl_ssepass_stat[0];glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);if(im)sp_geom(cs);else glCallList(list);glFinish();glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,img_s[r]);
       if(!r){n_keep+=vxgl_ssepass_stat[15]-k0;n_acc+=vxgl_ssepass_stat[16]-a0;n_fz+=vxgl_ssepass_stat[17]-f0;n_lb+=_vbo_vx855_listbind_n-l0;}}
       vxgl_ssepass_box=vxgl_ssepass_shine=vxgl_ssepass_pack=vxgl_ssepass_cache=vxgl_ssepass_mask=vxgl_gl_flatnocopy=vxgl_ssepass_tight=_tnl_vx855_fastbind=1;vx_d3d_gl_cullrun=vx_d3d_gl_cullrun2=1;vxgl_ssepass_fuse=vxgl_ssepass_accept=vxgl_ssepass_box2=vxgl_ssepass_partial=_vbo_vx855_listbind=vxgl_ssepass_immbox=vxgl_ssepass_snap2=vxgl_ssepass_outfast=vxgl_ssepass_early=1; /* the defaults (v10.492: SSESNAP 0) */
       if(!r){n_box+=vxgl_ssepass_stat[10]-b0;n_sh+=vxgl_ssepass_stat[13]-h0;n_ps+=vx_d3d_ps_used-u0;n_cr+=vx_d3d_cullrun_tris-c0;if(cu>=3)n_psf+=vx_d3d_ps_used-u0;}
       if(!r&&im&&want_pass&&cs!=5&&vxgl_ssepass_stat[8]==i0){printf("check FAIL: ssepass scene %d drawn without a list: not by the pass (reason bits %lX)\n",cs,vxgl_ssepass_stat[6]);fails++;}
       if(!r&&want_pass&&vxgl_ssepass_stat[0]==p0){printf("check FAIL: ssepass scene %d (%s): the pass not taken (reason bits %lX)\n",cs,cn[cs],vxgl_ssepass_stat[6]);fails++;}
       if(!r&&!want_pass&&vxgl_ssepass_stat[0]!=p0){printf("check FAIL: ssepass scene %d (%s): the pass taken\n",cs,cn[cs]);fails++;}
       if(!r&&cs==5&&!(vxgl_ssepass_stat[0]!=p0&&vxgl_ssepass_stat[5])){printf("check FAIL: ssepass scene %d: the whole list outside not counted\n",cs);fails++;}
       if(!r&&cs==6&&vxgl_ssepass_stat[2]==r0){printf("check FAIL: ssepass scene %d: no group by the reference\n",cs);fails++;}}
      vxgl_ssepass_on=0;
      for(i=0;i<64*64*4;i++){int e=abs(img_s[0][i]-img_s[1][i]);if(e){d++;if(e>mx)mx=e;}}
      for(i=0;i<64*64;i++)if(img_s[1][i*4]|img_s[1][i*4+1]|img_s[1][i*4+2])nz++;
      {int bad=mx>(lit?1:0)||((cu==0||cu==3)&&cs!=5&&nz<300)||(cs==5&&nz),d2=0;
       for(i=0;i<64*64*4;i++)if(img_s[0][i]!=img_s[2][i])d2++; /* v10.490: the new parts off: v10.489's pass, the same bytes */
       if(d2)bad=1;
       printf("ssepass check, scene %d (%s), %s, culling %s: %d of %d drawn pixels' channels differ (at most %d), %lu groups by the reference; %d bytes apart from the pass without v10.490-v10.495's parts%s\n",
        cs,cn[cs],im?"no list":"a list",cu==0?"off":cu==1?"back":cu==2?"front":cu==3?"off, flat":"front, flat",d,nz,mx,vxgl_ssepass_stat[2]-r0,d2,bad?"  FAIL":"");if(bad)fails++;}}}}
     glDisable(GL_CULL_FACE);
     glShadeModel(GL_SMOOTH);
     printf("ssepass v10.490 parts: %lu draws whole outside by the box, %lu vertices taken snapped by the strip path (%lu with flat shading), %lu triangles by the culling run, %lu specular lanes by SSE; v10.492: %lu lit set-ups kept\n",n_box,n_ps,n_psf,n_cr,n_sh,n_keep);
     if(!n_box||!n_ps||!n_psf||!n_cr||!n_sh||!n_keep){printf("check FAIL: ssepass v10.490: a part not used\n");fails++;}
     printf("ssepass v10.495 parts: %lu groups by the fused kernels, %lu list draws inside every plane by the box, %lu list draws bound by the short path\n",n_fz,n_acc,n_lb);
     if(!n_fz||!n_acc||!n_lb){printf("check FAIL: ssepass v10.495: a part not used\n");fails++;}
     printf("ssepass v10.496: %lu draws not from a list inside every plane by their own box\n",vxgl_ssepass_imm_stat[1]-n_ib0);
     if(vxgl_ssepass_imm_stat[1]==n_ib0){printf("check FAIL: ssepass v10.496: no draw not from a list found inside by its box\n");fails++;}
     { /* v10.497: SSEEARLY (a directional light, no local viewer: Mesa's fast lighting, declined before the copy) and SSEOUTFAST (a lit draw whole
        outside with no box: the test in the registers), each against its switch off, exactly; the counts advance */
      static GLubyte img_e[2][64*64*4];static const GLfloat dl[4]={0.3f,0.5f,1.0f,0.0f};int pe,de=0,j2;unsigned long e0=vxgl_ssepass_imm_stat[2],o0,o1,ne=0,no=0;
      glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glDisable(GL_LIGHT1);glDisable(GL_LIGHT2);glLightfv(GL_LIGHT0,GL_POSITION,dl);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,0);
      glDisable(GL_COLOR_MATERIAL);glDisable(GL_FOG);glDisable(GL_TEXTURE_2D);
      for(pe=0;pe<2;pe++){vxgl_ssepass_early=!pe;vxgl_ssepass_on=5;glClear(GL_COLOR_BUFFER_BIT);
       for(q=0;q<3;q++){glBegin(GL_TRIANGLE_STRIP);for(k=0;k<40;k++){glNormal3f(0.6f*(float)sin(k*0.4+q),0.5f*(float)cos(k*0.3),1.0f);glVertex3f(2.0f+(float)(k/2)*3.1f+q*0.7f,((k&1)?50.5f:8.25f)-q*4.0f,(float)(k%9)/10.0f-0.4f);}glEnd();}
       glFinish();glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,img_e[pe]);if(!pe)ne=vxgl_ssepass_imm_stat[2]-e0;}
      for(i=0;i<64*64*4;i++)if(img_e[0][i]!=img_e[1][i])de++;
      glLightfv(GL_LIGHT0,GL_POSITION,pos);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,1);vxgl_ssepass_immbox=0;
      for(j2=0;j2<2;j2++){vxgl_ssepass_outfast=!j2;o0=vxgl_ssepass_stat[5];
       glBegin(GL_TRIANGLE_STRIP);for(k=0;k<40;k++){glNormal3f(0,0,1);glVertex3f(5000.0f+(float)(k/2)*3.1f,(k&1)?50.5f:8.25f,0.1f);}glEnd();glFinish();
       o1=vxgl_ssepass_stat[5];if(o1!=o0)no++;}
      vxgl_ssepass_immbox=vxgl_ssepass_outfast=vxgl_ssepass_early=1;vxgl_ssepass_on=0;glDisable(GL_LIGHTING);
      printf("ssepass v10.497: %lu draws declined before the copy, %d bytes apart from the copy first; a draw whole outside found with and without SSEOUTFAST: %lu of 2\n",ne,de,no);
      if(ne<1||de||no!=2){printf("check FAIL: ssepass v10.497 SSEEARLY / SSEOUTFAST\n");fails++;}}
     { /* v10.495 (SSEPARTIAL): two lists of one colour each with colour material drawn in turn (each set-up made for the tracked material only),
        against the same with the part off (every set-up whole), exactly */
      static GLubyte img_p[2][64*64*4];GLuint lp=glGenLists(3);int pr,j,dp=0;unsigned long q0=vxgl_ssepass_stat[18],nq=0;
      for(j=0;j<3;j++){glNewList(lp+j,GL_COMPILE);if(j==2)glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,30.0f);glColor3f(j==1?0.2f:0.8f,j==1?0.7f:0.6f,j==1?0.9f:0.3f); /* the third: the shininess too */
       for(q=0;q<3;q++){glBegin(GL_TRIANGLE_STRIP);for(k=0;k<40;k++){glNormal3f(0.6f*(float)sin(k*0.4+q),0.5f*(float)cos(k*0.3),1.0f);
        glVertex3f(2.0f+(float)(k/2)*3.1f+q*0.7f+j*1.5f,((k&1)?50.5f:8.25f)-q*4.0f-j*14.0f,(float)(k%9)/10.0f-0.4f);}glEnd();}glEndList();}
      glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glDisable(GL_LIGHT1);glDisable(GL_LIGHT2);glLightfv(GL_LIGHT0,GL_POSITION,pos);glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.0f);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,1);
      glEnable(GL_COLOR_MATERIAL);glDisable(GL_FOG);glDisable(GL_TEXTURE_2D);glDisable(GL_NORMALIZE);
      for(pr=0;pr<2;pr++){vxgl_ssepass_partial=!pr;vxgl_ssepass_on=5;glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,8.0f);glClear(GL_COLOR_BUFFER_BIT);for(j=0;j<9;j++)glCallList(lp+j%3);glFinish();glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,img_p[pr]);
       if(!pr)nq=vxgl_ssepass_stat[18]-q0;}
      vxgl_ssepass_partial=1;vxgl_ssepass_on=0;glDisable(GL_COLOR_MATERIAL);glDisable(GL_LIGHTING);glDeleteLists(lp,3);glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,8.0f);
      for(i=0;i<64*64*4;i++)if(img_p[0][i]!=img_p[1][i])dp++;
      printf("ssepass v10.495 partial set-up check: %lu set-ups for the material only, %d bytes apart from whole set-ups\n",nq,dp);
      if(nq<6||dp){printf("check FAIL: ssepass v10.495 partial set-ups\n");fails++;}}
     { /* the kernels against snapw and UNCLAMPED_FLOAT_TO_UBYTE, bit for bit: grid points and half points with their neighbours, both signs, the limits */
      extern void vxgl_ssepass_ktest(GLuint which,const GLuint *in,GLuint *out,GLuint n,const GLfloat *tab);extern GLuint vx_d3d_snapw(GLuint w,int up);
      static GLuint in[70000],out[70000];int nin=0,w,bk=0,bu=0,ns;unsigned long seed=12345;
      #define FB(f) (*(const GLuint*)&(f))
      for(i=0;i<4000;i++){float f;int sg;for(sg=0;sg<2;sg++){GLuint b;
        f=(float)i/16.0F;b=FB(f)|(sg?0x80000000U:0);in[nin++]=b;in[nin++]=b+1;if(b&0x7FFFFFFF)in[nin++]=b-1;
        f=((float)i+0.5F)/2048.0F;b=FB(f)|(sg?0x80000000U:0);in[nin++]=b;in[nin++]=b+1;if(b&0x7FFFFFFF)in[nin++]=b-1;}}
      for(i=0;i<8000;i++){float f;seed=seed*1103515245UL+12345UL;f=(float)((seed>>8)&0xFFFFF)/256.0F-2100.0F;in[nin++]=FB(f);}
      {static const GLuint sp[]={0,0x80000000U,0x7F800000U,0xFF800000U,0x7FC00000U,0x00000001U,0x44FF0000U,0x44FF0001U,0x44FEFFFFU,0xC4FF0000U,0x3F7F0000U,0x3F7EFFFFU,0x3F800000U,0x3F800001U,0xBF800000U};
       for(w=0;w<(int)(sizeof(sp)/sizeof(sp[0]));w++)in[nin++]=sp[w];}
      ns=nin;for(w=0;w<2;w++){vxgl_ssepass_ktest((GLuint)w,in,out,(GLuint)nin,0);for(i=0;i<nin;i++)if(out[i]!=vx_d3d_snapw(in[i],w)){if(bk<5)printf("check FAIL: snap kernel %d: %08X gives %08X, snapw %08X\n",w,in[i],out[i],vx_d3d_snapw(in[i],w));bk++;}}
      nin=0;for(i=0;i<20000;i++){float f;seed=seed*1103515245UL+12345UL;f=(float)((seed>>8)&0xFFFFF)/524288.0F-0.25F;in[nin++]=FB(f);}
      for(i=0;i<300;i++){in[nin++]=0x3F7F0000U+(GLuint)i-150U;in[nin++]=0x3F800000U+(GLuint)i-150U;}
      {static const GLuint sp[]={0,0x80000000U,0x7F800000U,0xFF800000U,0x7FC00000U,0xFFC00000U,0x00000001U,0x80000001U};for(w=0;w<(int)(sizeof(sp)/sizeof(sp[0]));w++)in[nin++]=sp[w];}
      vxgl_ssepass_ktest(2,in,out,(GLuint)nin,0);
      for(i=0;i<nin;i++){GLubyte b;float f=*(const float*)&in[i];UNCLAMPED_FLOAT_TO_UBYTE(b,f);if(out[i]!=b){if(bu<5)printf("check FAIL: byte kernel: %08X gives %u, Mesa %u\n",in[i],out[i],b);bu++;}}
      #undef FB
      printf("ssepass kernels: snap %d values each way, %d apart; bytes %d values, %d apart%s\n",ns,bk,nin,bu,bk||bu?"  FAIL":"");if(bk||bu)fails++;}
     { /* v10.493: two materials a vertex (ambient red, diffuse green; a white light along z, the normal along z): the colour 0.5 + 0.2 0.5 red, 0.5 green from a
        * list and from glBegin (Mesa's offset step took the first material's place for the second: diffuse red too) */
      static const GLfloat am[4]={0.5f,0,0,1},df[4]={0,0.5f,0,1},wh[4]={1,1,1,1},bl[4]={0,0,0,1},dz[4]={0,0,1,0};GLint vpt[4];int im2,k2;GLubyte px2[4];
      glDisable(GL_FOG);glDisable(GL_TEXTURE_2D);glDisable(GL_CULL_FACE);glDisable(GL_COLOR_MATERIAL);glDisable(GL_LIGHT1);glDisable(GL_LIGHT2);glDisable(GL_NORMALIZE);glShadeModel(GL_SMOOTH);
      glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,0);glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.0f);
      glLightfv(GL_LIGHT0,GL_AMBIENT,wh);glLightfv(GL_LIGHT0,GL_DIFFUSE,wh);glLightfv(GL_LIGHT0,GL_SPECULAR,bl);
      glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();glLightfv(GL_LIGHT0,GL_POSITION,dz);
      glGetIntegerv(GL_VIEWPORT,vpt);
      for(im2=0;im2<2;im2++){
       if(!im2)glNewList(list,GL_COMPILE);
       glBegin(GL_TRIANGLES);for(k2=0;k2<3;k2++){glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,am);glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,df);glNormal3f(0,0,1);
        glVertex3f(k2==1?0.9f:-0.9f,k2==2?0.9f:-0.9f,0);}glEnd();
       if(!im2){glEndList();}
       glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);if(!im2)glCallList(list);else{glBegin(GL_TRIANGLES);for(k2=0;k2<3;k2++){glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,am);glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,df);
        glNormal3f(0,0,1);glVertex3f(k2==1?0.9f:-0.9f,k2==2?0.9f:-0.9f,0);}glEnd();}
       glFinish();glReadPixels(vpt[0]+vpt[2]/2-vpt[2]/8,vpt[1]+vpt[3]/2-vpt[3]/8,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px2);
       {int bad=abs(px2[0]-153)>8||abs(px2[1]-128)>8||px2[2]>8; /* within 8: a 16-bit frame's 5 and 6 bits (156 129); the bug gave red 255, green 0 */printf("materials a vertex check (%s): %u %u %u (want 153 128 0)%s\n",im2?"glBegin":"a list",px2[0],px2[1],px2[2],bad?"  FAIL":"");if(bad)fails++;}}
      glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(GL_MODELVIEW);glPopMatrix();
      {static const GLfloat d8[4]={0.8f,0.8f,0.8f,1},a2[4]={0.2f,0.2f,0.2f,1};glMaterialfv(GL_FRONT_AND_BACK,GL_DIFFUSE,d8);glMaterialfv(GL_FRONT_AND_BACK,GL_AMBIENT,a2);glLightfv(GL_LIGHT0,GL_AMBIENT,bl);}
      glDisable(GL_LIGHTING);}
     glDeleteLists(list,1);glDeleteTextures(1,&tx);glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHT1);glDisable(GL_LIGHT2);glDisable(GL_NORMALIZE);glShadeModel(GL_FLAT);glLightModeli(GL_LIGHT_MODEL_LOCAL_VIEWER,0);glFogi(GL_FOG_MODE,GL_LINEAR);
     glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.0f);{static const GLfloat ld[4]={0.3f,0.5f,1.0f,0.0f};glLightfv(GL_LIGHT0,GL_POSITION,ld);}cur->new_state|=VXGL_STATE_MASK;}
    vxgl_gl_flatbatch=1;_tnl_vx855_emit=1;_tnl_vx855_reemit(&cur->gl);glDisable(GL_FOG);glDisable(GL_LIGHTING);glDisable(GL_COLOR_MATERIAL);glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SINGLE_COLOR);}
   /* v10.483: flat triangles, fans, polygons, quads (and the same by glDrawElements, and a triangle the clipper cuts) whose colours differ, on
    the engine's flat shading as triangle lists each turned to start at its provoking vertex: the template's pixels */
   {static GLubyte img_p[2][64*64*4];static const char *nm[7]={"triangles","a fan","a polygon","quads","triangles by glDrawElements","a clipped triangle","a quad strip"};int t;
    for(t=0;t<7;t++){int r;uint32_t s6=0;
     for(r=0;r<2;r++){int k;uint32_t f6=vxgl_flat_stat[6];vx_d3d_gl_hwflat=!r;vxgl_gl_flatbatch=!r;vxgl_gl_flatstrips=!r;vxgl_gl_hwflatstrip=!r;cur->new_state|=VXGL_STATE_MASK;
      glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
      #define PC(k) glColor3f(((k)%7)/7.0f,((k)%5)/5.0f,((k)%3)/3.0f)
      if(t==0){glBegin(GL_TRIANGLES);for(k=0;k<18;k++){PC(k);glVertex2f(3.0f+(float)(k/3)*9.5f+((k%3)==1?8.0f:0.0f),(k%3)==2?55.5f:(k&1)?6.25f:12.5f);}glEnd();}
      else if(t==1){glBegin(GL_TRIANGLE_FAN);PC(0);glVertex2f(32.3f,30.7f);for(k=0;k<9;k++){PC(k+1);glVertex2f(32.3f+27.0f*(float)cos(k*0.7),30.7f+26.0f*(float)sin(k*0.7));}glEnd();}
      else if(t==2){glBegin(GL_POLYGON);for(k=0;k<8;k++){PC(k+3);glVertex2f(32.1f+28.0f*(float)cos(k*0.785+0.2),31.9f+27.0f*(float)sin(k*0.785+0.2));}glEnd();}
      else if(t==3){glBegin(GL_QUADS);for(k=0;k<12;k++){int q=k/4,c=k%4;PC(k);glVertex2f(2.5f+q*20.0f+(c==1||c==2?17.0f:0.0f)+(c==2?1.5f:0.0f),c>=2?58.2f:4.4f+q*2.0f);}glEnd();}
      else if(t==4){static GLfloat vv[12*2];static GLfloat cc[12*3];static const GLuint ix[12]={0,1,2,3,4,5,6,7,8,9,10,11};static const GLuint ord[12]={2,0,1,5,3,4,8,6,7,11,9,10};
       for(k=0;k<12;k++){vv[k*2]=4.0f+(float)(k/3)*14.0f+((k%3)==1?12.0f:0.0f);vv[k*2+1]=(k%3)==2?60.0f:5.5f;cc[k*3]=(k%7)/7.0f;cc[k*3+1]=(k%5)/5.0f;cc[k*3+2]=(k%3)/3.0f;}(void)ix;
       glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_COLOR_ARRAY);glVertexPointer(2,GL_FLOAT,0,vv);glColorPointer(3,GL_FLOAT,0,cc);glDrawElements(GL_TRIANGLES,12,GL_UNSIGNED_INT,ord);
       glDisableClientState(GL_VERTEX_ARRAY);glDisableClientState(GL_COLOR_ARRAY);}
      else if(t==5){glBegin(GL_TRIANGLES);PC(1);glVertex2f(-30.0f,10.0f);PC(2);glVertex2f(60.0f,4.0f);PC(3);glVertex2f(40.0f,90.0f);PC(4);glVertex2f(5.0f,40.0f);PC(5);glVertex2f(30.0f,-20.0f);PC(6);glVertex2f(55.0f,50.0f);glEnd();}
      else {glBegin(GL_QUAD_STRIP);for(k=0;k<40;k++){PC(k);glVertex2f(2.0f+(float)(k/2)*3.1f,(k&1)?50.5f:8.25f);}glEnd();}
      #undef PC
      glFinish();glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,img_p[r]);if(!r)s6=vxgl_flat_stat[6]-f6;}
     if(t!=6&&!s6){printf("check FAIL: flat %s with differing colours not on the engine's flat shading\n",nm[t]);fails++;}
     {int nz=0,i;for(i=0;i<64*64;i++)if(img_p[1][i*4]|img_p[1][i*4+1]|img_p[1][i*4+2])nz++;if(nz<300){printf("check FAIL: flat %s drew %d pixels\n",nm[t],nz);fails++;}}
     if(memcmp(img_p[0],img_p[1],sizeof(img_p[0]))){int i,d=0;for(i=0;i<64*64*4;i++)if(img_p[0][i]!=img_p[1][i])d++;printf("check FAIL: flat %s on the engine's flat shading: %d bytes differ from the template\n",nm[t],d);fails++;}}
    vx_d3d_gl_hwflat=1;vxgl_gl_flatbatch=1;vxgl_gl_flatstrips=1;vxgl_gl_hwflatstrip=1;cur->new_state|=VXGL_STATE_MASK;}
   glShadeModel(GL_SMOOTH);}
  if(pass==0&&scene_glsl_create){ /* v10.322 (review): shaders attached to programs still there when the context is deleted (below): the
   programs give their shaders back before the shaders are freed (Mesa freed them in hash order: a program after its shader read it) */
   static const char *src="void main(){gl_FragColor=vec4(1.0,0.0,0.0,1.0);}";GLuint sh,pr,sh2,pr2;
   sh=scene_glsl_create(GL_FRAGMENT_SHADER);scene_glsl_src(sh,1,&src,0);scene_glsl_compile(sh);sh2=scene_glsl_create(GL_FRAGMENT_SHADER);scene_glsl_src(sh2,1,&src,0);scene_glsl_compile(sh2);
   pr=scene_glsl_prog();scene_glsl_attach(pr,sh);scene_glsl_attach(pr,sh2);scene_glsl_link(pr);pr2=scene_glsl_prog();scene_glsl_attach(pr2,sh);scene_glsl_link(pr2);scene_glsl_use(pr);
   glBegin(GL_TRIANGLES);glVertex2f(10,10);glVertex2f(20,10);glVertex2f(15,20);glEnd();glFinish();scene_glsl_use(0);(void)pr2;}
  if(pass==0){ /* v10.330 (review #37, #15, #12, #11): checks against Mesa's answers */
   GLubyte q[4];GLuint tx;static GLubyte l0[2*1*4]={255,0,0,255,0,255,0,255},l1[4]={0,0,255,255};
   #define QUAD_TC() do{glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(10,10);glTexCoord2f(1,0);glVertex2f(30,10);glTexCoord2f(1,1);glVertex2f(30,30);glTexCoord2f(0,1);glVertex2f(10,30);glEnd();glFinish();glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(20,20,1,1,GL_RGBA,GL_UNSIGNED_BYTE,q);}while(0)
   /* #37: no colour written with GL_MIN kept from the blend: the red background stays */
   if(scene_blend_eq){glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glColorMask(0,0,0,0);glEnable(GL_BLEND);scene_blend_eq(GL_MIN);glColor3f(0,0,0);
    glBegin(GL_QUADS);glVertex2f(10,10);glVertex2f(30,10);glVertex2f(30,30);glVertex2f(10,30);glEnd();glFinish();
    glColorMask(1,1,1,1);glDisable(GL_BLEND);scene_blend_eq(GL_FUNC_ADD);glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(20,20,1,1,GL_RGBA,GL_UNSIGNED_BYTE,q);glClearColor(0,0,0,1);
    if(q[0]<250||q[1]>4||q[2]>4){printf("check FAIL: colour mask all off with GL_MIN drew %02X%02X%02X (want the red background)\n",q[0],q[1],q[2]);fails++;}}
   /* #15: MIN_LOD = MAX_LOD = 0.25 with GL_LINEAR magnification and NEAREST_MIPMAP_NEAREST: swrast magnifies below 0.5 (red and green
    * blended); #12: both 1e30: the last level (blue), no integer overflow */
   glGenTextures(1,&tx);glBindTexture(GL_TEXTURE_2D,tx);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
   glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,1,0,GL_RGBA,GL_UNSIGNED_BYTE,l0);glTexImage2D(GL_TEXTURE_2D,1,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,l1);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);
   glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
   glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glEnable(GL_TEXTURE_2D);
   glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,0.25F);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_LOD,0.25F);QUAD_TC();
   if(q[0]<60||q[0]>200||q[1]<60||q[1]>200||q[2]>40){printf("check FAIL: LOD clamped to 0.25 (linear magnification, nearest mipmaps) drew %02X%02X%02X (want red and green blended, as swrast)\n",q[0],q[1],q[2]);fails++;}
   glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_LOD,1e30F);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,1e30F);QUAD_TC();
   if(q[2]<250||q[0]>4||q[1]>4){printf("check FAIL: LOD clamped to 1e30 drew %02X%02X%02X (want the last level, blue)\n",q[0],q[1],q[2]);fails++;}
   glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,-1000.0F);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_LOD,1000.0F);
   /* #11: one texture on units 0 and 1, unit 0's colour table red, unit 1's white; unit 0 REPLACE, unit 1 MODULATE: red */
   if(scene_ctab&&scene_active_tex&&scene_mtc2){static GLubyte red[256*4],white[256*4];int k;for(k=0;k<256;k++){red[k*4]=255;red[k*4+1]=0;red[k*4+2]=0;red[k*4+3]=255;white[k*4]=white[k*4+1]=white[k*4+2]=white[k*4+3]=255;}
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    scene_active_tex(GL_TEXTURE0_ARB);scene_ctab(GL_TEXTURE_COLOR_TABLE_SGI,GL_RGBA,256,GL_RGBA,GL_UNSIGNED_BYTE,red);glEnable(GL_TEXTURE_COLOR_TABLE_SGI);
    scene_active_tex(GL_TEXTURE1_ARB);glBindTexture(GL_TEXTURE_2D,tx);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    scene_ctab(GL_TEXTURE_COLOR_TABLE_SGI,GL_RGBA,256,GL_RGBA,GL_UNSIGNED_BYTE,white);glEnable(GL_TEXTURE_COLOR_TABLE_SGI);
    glBegin(GL_QUADS);scene_mtc2(GL_TEXTURE0_ARB,0,0);scene_mtc2(GL_TEXTURE1_ARB,0,0);glVertex2f(10,10);scene_mtc2(GL_TEXTURE0_ARB,1,0);scene_mtc2(GL_TEXTURE1_ARB,1,0);glVertex2f(30,10);
    scene_mtc2(GL_TEXTURE0_ARB,1,1);scene_mtc2(GL_TEXTURE1_ARB,1,1);glVertex2f(30,30);scene_mtc2(GL_TEXTURE0_ARB,0,1);scene_mtc2(GL_TEXTURE1_ARB,0,1);glVertex2f(10,30);glEnd();glFinish();
    glReadPixels(20,20,1,1,GL_RGBA,GL_UNSIGNED_BYTE,q);
    glDisable(GL_TEXTURE_COLOR_TABLE_SGI);glDisable(GL_TEXTURE_2D);scene_active_tex(GL_TEXTURE0_ARB);glDisable(GL_TEXTURE_COLOR_TABLE_SGI);
    if(q[0]<250||q[1]>4||q[2]>4){printf("check FAIL: one texture on two units with different colour tables drew %02X%02X%02X (want red, as swrast)\n",q[0],q[1],q[2]);fails++;}}
   glDisable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glDeleteTextures(1,&tx);glColor3f(1,1,1);
   #undef QUAD_TC
  }
  if(pass==0&&scene_gen_prog){ /* v10.322 (review): a fragment program's small coefficient is kept (MUL by 1e-7, then by 1e7: red, not black) */
   static const char fp7[]="!!ARBfp1.0\nTEMP r;\nMUL r, fragment.color, {1e-7,1e-7,1e-7,1};\nMUL result.color, r, {1e7,1e7,1e7,1};\nEND\n";
   static const char fpc[]="!!ARBfp1.0\nTEMP r;\nMUL r, fragment.color, 0.1;\nMAD r, r, 3, fragment.color;\nMAD result.color, fragment.color, -1.3, r;\nEND\n";
   /* v10.326 (review): an exact small difference (1 - 0.99999994 = 2^-24, no rounding) then scaled by 1e7: 0.596 red, not black */
   static const char fpx[]="!!ARBfp1.0\nTEMP r;\nSUB r, {1,1,1,1}, {0.99999994,0.99999994,0.99999994,0};\nMUL r, r, {1e7,0,0,1};\nMUL result.color, fragment.color, r;\nEND\n";
   /* v10.328 (review): float rounding swrast computes too, scaled up (0.1 added ten times = 1.0000001, minus 1, times 1e6: 0.119, 30 red);
    * and two clamps 0.0001 apart kept apart (clamp(2c + 0.0001) - clamp(2c), times 1000, at c = 0.25: 26 red) */
   static const char fpr[]="!!ARBfp1.0\nTEMP r;\nMOV r, {0,0,0,0};\nADD r, r, 0.1;\nADD r, r, 0.1;\nADD r, r, 0.1;\nADD r, r, 0.1;\nADD r, r, 0.1;\nADD r, r, 0.1;\nADD r, r, 0.1;\nADD r, r, 0.1;\nADD r, r, 0.1;\nADD r, r, 0.1;\nSUB r, r, 1;\nMUL r, r, {1000000,0,0,0};\nMUL result.color, fragment.color, r;\nEND\n";
   static const char fps2[]="!!ARBfp1.0\nTEMP a,b;\nMUL_SAT a, fragment.color, 2;\nMAD_SAT b, fragment.color, 2, 0.0001;\nSUB a, b, a;\nMUL result.color, a, {1000,0,0,1};\nEND\n";
   /* v10.329 (review): a clamp 1e-5 over 1 kept (MUL_SAT by 1.000005, minus the colour, times 1e5: black), and an overflow (1e30 * 1e30: 255 red) */
   static const char fpu[]="!!ARBfp1.0\nTEMP a;\nMUL_SAT a, fragment.color, 1.000005;\nSUB a, a, fragment.color;\nMUL result.color, a, {100000,0,0,1};\nEND\n";
   static const char fpo[]="!!ARBfp1.0\nTEMP a;\nMUL a, {1e30,0,0,1}, {1e30,0,0,1};\nMUL result.color, fragment.color, a;\nEND\n";
   /* v10.334 (audit F01): FLR and FRC each component its own: FLR (0.25, 1.5, 0.75, 1.25) is green (it was black: X for all four);
    * FRC (1.25, 0.5, 2.75, 1) is 40 80 BF */
   static const char fpf[]="!!ARBfp1.0\nTEMP r;\nFLR r, {0.25,1.5,0.75,1.25};\nMOV result.color, r;\nEND\n";
   static const char fpq[]="!!ARBfp1.0\nTEMP r;\nFRC r, {1.25,0.5,2.75,1};\nMOV result.color, r;\nEND\n";
   static const char *const fps[9]={fp7,fpc,fpx,fpr,fps2,fpu,fpo,fpf,fpq};
   GLuint id[9];GLubyte q[4];int k;scene_gen_prog(9,id);
   for(k=0;k<9;k++){scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,id[k]);scene_prog_str(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(fps[k]),fps[k]);
    glEnable(GL_FRAGMENT_PROGRAM_ARB);glColor3f(k==4?0.25F:1.0F,0,0);glBegin(GL_QUADS);glVertex2f(10,10);glVertex2f(30,10);glVertex2f(30,30);glVertex2f(10,30);glEnd();glFinish();glDisable(GL_FRAGMENT_PROGRAM_ARB);
    glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(20,20,1,1,GL_RGBA,GL_UNSIGNED_BYTE,q);
    if(k==0&&(q[0]<240||q[1]>16||q[2]>16)){printf("check FAIL: a fragment program scaled by 1e-7 then 1e7 drew %02X%02X%02X (want red)\n",q[0],q[1],q[2]);fails++;}
    if(k==1&&(q[0]>16||q[1]>16||q[2]>16)){printf("check FAIL: a fragment program whose terms cancel (0.3c + c - 1.3c) drew %02X%02X%02X (want black)\n",q[0],q[1],q[2]);fails++;}
    if(k==1&&(c->fallback&VXGL_FB_PROGRAM)){printf("check FAIL: cancelling terms left a float residue (the program went to software)\n");fails++;}
    if(k==2&&(q[0]<140||q[0]>165||q[1]>16||q[2]>16)){printf("check FAIL: an exact small difference scaled by 1e7 drew %02X%02X%02X (want 98 or so red, as swrast)\n",q[0],q[1],q[2]);fails++;}
    if(k==2&&(c->fallback&VXGL_FB_PROGRAM)){printf("check FAIL: the exact small difference went to software\n");fails++;}
    if(k==3&&(q[0]<26||q[0]>34||q[1]>4||q[2]>4)){printf("check FAIL: 0.1 added ten times, minus 1, times 1e6 drew %02X%02X%02X (want 1E red, as swrast)\n",q[0],q[1],q[2]);fails++;}
    if(k==4&&(q[0]<22||q[0]>30||q[1]>4||q[2]>4)){printf("check FAIL: clamp(2c + 0.0001) - clamp(2c), times 1000, drew %02X%02X%02X at c = 0.25 (want 1A red, as swrast)\n",q[0],q[1],q[2]);fails++;}
    if(k==5&&(q[0]>4||q[1]>4||q[2]>4)){printf("check FAIL: MUL_SAT by 1.000005, minus the colour, times 1e5 drew %02X%02X%02X (want black, as swrast)\n",q[0],q[1],q[2]);fails++;}
    if(k==6&&(q[0]<250||q[1]>4||q[2]>4)){printf("check FAIL: 1e30 * 1e30 drew %02X%02X%02X (want FF red, as swrast)\n",q[0],q[1],q[2]);fails++;}
    if(k==7&&(q[0]>4||q[1]<250||q[2]>4)){printf("check FAIL: FLR (0.25, 1.5, 0.75, 1.25) drew %02X%02X%02X (want 00FF00, each component its own)\n",q[0],q[1],q[2]);fails++;}
    if(k==8&&(q[0]<60||q[0]>68||q[1]<124||q[1]>132||q[2]<187||q[2]>195)){printf("check FAIL: FRC (1.25, 0.5, 2.75, 1) drew %02X%02X%02X (want 4080BF)\n",q[0],q[1],q[2]);fails++;}
    if(k>=7&&(c->fallback&VXGL_FB_PROGRAM)){printf("check FAIL: FLR / FRC of constants went to software\n");fails++;}
    if(k>=3)printf("  fragment program %d: %02X%02X%02X on %s\n",k,q[0],q[1],q[2],c->fallback&VXGL_FB_PROGRAM?"software":"the engine");}
   { /* v10.335 (review #22): two programs drawn alternately are translated once each, then found in the memo */
    extern uint32_t __stdcall VXGL_FpStats(uint32_t *o,uint32_t on);uint32_t st[8];int r;
    VXGL_FpStats(st,2);glEnable(GL_FRAGMENT_PROGRAM_ARB);
    for(r=0;r<8;r++){scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,id[7+(r&1)]);glBegin(GL_QUADS);glVertex2f(10,10);glVertex2f(30,10);glVertex2f(30,30);glVertex2f(10,30);glEnd();}
    glFinish();glDisable(GL_FRAGMENT_PROGRAM_ARB);VXGL_FpStats(st,0);
    printf("  two programs alternating, 8 draws: %u translations asked, %u memo hits, %u misses\n",(unsigned)st[0],(unsigned)st[1],(unsigned)st[2]);
    if(st[0]<4||st[2]>2){printf("check FAIL: two programs alternating were translated %u times (want 2 at most: the memo keeps both)\n",(unsigned)st[2]);fails++;}}
   { /* v10.337 (board, VXPROBE fpcache: an LRP by a program.local parameter took 4.1 s to translate and then went to software) */
    extern uint32_t __stdcall VXGL_FpStats(uint32_t *o,uint32_t on);uint32_t st[8];GLuint pl;
    static const char fpl[]="!!ARBfp1.0\nPARAM c = program.local[0];\nLRP result.color, c, fragment.color, {0.2,0.4,0.6,1};\nEND\n";
    static const char fpl2[]="!!ARBfp1.0\nPARAM c = program.local[0];\nTEMP t;\nMUL t, fragment.color, c;\nMAD t, t, fragment.color, c.wzyx;\nLRP result.color, c, t, {0.2,0.4,0.6,1};\nEND\n";
    const char *const ps[2]={fpl,fpl2};int j;
    for(j=0;j<2;j++){scene_gen_prog(1,&pl);scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,pl);scene_prog_str(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(ps[j]),ps[j]);
    glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,0,0.3F,0.5F,0.7F,0.5F);
    VXGL_FpStats(st,2);glEnable(GL_FRAGMENT_PROGRAM_ARB);glColor3f(1,0.5F,0.25F);
    glBegin(GL_QUADS);glVertex2f(10,10);glVertex2f(30,10);glVertex2f(30,30);glVertex2f(10,30);glEnd();glFinish();glDisable(GL_FRAGMENT_PROGRAM_ARB);VXGL_FpStats(st,0);
    printf("  LRP by a local parameter (%d): %u misses, %u to software, %u microseconds translating\n",j,(unsigned)st[2],(unsigned)st[3],(unsigned)st[4]);
    if(st[4]>300000U){printf("check FAIL: a fragment program took %u microseconds to translate (want the search bounded: 300 ms at most)\n",(unsigned)st[4]);fails++;}
    /* another parameter value: the program ran out of budget once, so it is not searched again */
    glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,0,0.6F,0.1F,0.9F,0.5F);VXGL_FpStats(st,2);glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glBegin(GL_QUADS);glVertex2f(10,10);glVertex2f(30,10);glVertex2f(30,30);glVertex2f(10,30);glEnd();glFinish();glDisable(GL_FRAGMENT_PROGRAM_ARB);VXGL_FpStats(st,0);
    printf("  the same with another value: %u microseconds translating, %u to software\n",(unsigned)st[4],(unsigned)st[3]);
    if(st[3]!=1||st[4]>5000U){printf("check FAIL: a program that ran out of the search budget was searched again for a new parameter value (%u microseconds)\n",(unsigned)st[4]);fails++;}}}
   { /* v10.340: VXPROBE fpcache's program: LRP by a program.local parameter between the texture and the colour (Direct3D's LERP with
      * TFACTOR as its first argument computes it) */
    extern uint32_t __stdcall VXGL_FpStats(uint32_t *o,uint32_t on);uint32_t st[8];GLuint pl,tx;static GLubyte tb[4*4*4];int i;GLubyte q[4];
    static const char fpt[]="!!ARBfp1.0\nPARAM c = program.local[0];\nTEMP t;\nTEX t, fragment.texcoord[0], texture[0], 2D;\nLRP result.color, c, t, fragment.color;\nEND\n";
    for(i=0;i<16;i++){tb[i*4]=200;tb[i*4+1]=100;tb[i*4+2]=40;tb[i*4+3]=255;}
    glGenTextures(1,&tx);glBindTexture(GL_TEXTURE_2D,tx);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,tb);glEnable(GL_TEXTURE_2D);
    scene_gen_prog(1,&pl);scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,pl);scene_prog_str(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(fpt),fpt);
    glProgramLocalParameter4fARB(GL_FRAGMENT_PROGRAM_ARB,0,0.25F,0.5F,0.75F,0.5F);
    VXGL_FpStats(st,2);glEnable(GL_FRAGMENT_PROGRAM_ARB);glColor4f(0.0F,0.4F,1.0F,1.0F);
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(10,10);glTexCoord2f(1,0);glVertex2f(30,10);glTexCoord2f(1,1);glVertex2f(30,30);glTexCoord2f(0,1);glVertex2f(10,30);glEnd();glFinish();
    glDisable(GL_FRAGMENT_PROGRAM_ARB);glDisable(GL_TEXTURE_2D);VXGL_FpStats(st,0);
    glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(20,20,1,1,GL_RGBA,GL_UNSIGNED_BYTE,q);
    printf("  LRP by a local parameter between the texture and the colour: %02X%02X%02X on %s (%u us)\n",q[0],q[1],q[2],st[3]?"software":"the engine",(unsigned)st[4]);
    if(st[3]||q[0]<0x2E||q[0]>0x36||q[1]<0x61||q[1]>0x69||q[2]<0x56||q[2]>0x62){printf("check FAIL: LRP by a local parameter between the texture and the colour: %02X%02X%02X on %s (want 32655E, the engine's LERP by TFACTOR)\n",q[0],q[1],q[2],st[3]?"software":"the engine");fails++;}
    glDeleteTextures(1,&tx);}
   scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,0);}
  if(pass==0){ /* v10.334 (audit F04): a smooth line stippled 0x0001 (factor 1) draws one unit in sixteen (it drew the whole line) */
   GLubyte row[96*4];int lit=0,x;
   glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glEnable(GL_LINE_SMOOTH);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
   glEnable(GL_LINE_STIPPLE);glLineStipple(1,0x0001);glColor3f(1,0,0);glBegin(GL_LINES);glVertex2f(10.5F,40.5F);glVertex2f(74.5F,40.5F);glEnd();glFinish();
   glDisable(GL_LINE_STIPPLE);glDisable(GL_BLEND);glDisable(GL_LINE_SMOOTH);
   glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,40,96,1,GL_RGBA,GL_UNSIGNED_BYTE,row);
   for(x=0;x<96;x++)if(row[x*4]>64)lit++;
   printf("  smooth line stippled 0x0001 over 64 units: %d pixels lit\n",lit);
   if(lit<2||lit>12){printf("check FAIL: a smooth line stippled 0x0001 lit %d pixels (want one unit in sixteen: 4 dashes)\n",lit);fails++;}}
  if(pass==0&&scene_fbo_gen){ /* v10.329 (review): a mapping that fails once is tried again; one that fails twice is GL_OUT_OF_MEMORY, and
   the texture's drawn level stays marked, so a later read gets what was drawn (both read 0,0,40 for good in v10.327) */
   extern int host_map_fail;int fl;
   for(fl=1;fl<=2;fl++){GLuint fb,t,rb;static GLubyte g[16*16*4];GLenum e;GLubyte px[4];
    t=rtt_tex(16,16,GL_RGB);scene_fbo_gen(1,&fb);scene_fbo_bind(GL_FRAMEBUFFER_EXT,fb);rtt_view(16,16);
    scene_fbo_tex2d(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,t,0);glColor3f(1,0,0);glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(16,0);glVertex2f(16,16);glVertex2f(0,16);glEnd();
    scene_fbo_bind(GL_FRAMEBUFFER_EXT,0);glBindTexture(GL_TEXTURE_2D,t);while(glGetError()!=GL_NO_ERROR);
    memset(g,0,sizeof(g));glPixelStorei(GL_PACK_ALIGNMENT,1);host_map_fail=fl;glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,g);e=glGetError();host_map_fail=0;
    if(fl==1&&(e!=GL_NO_ERROR||g[0]!=255||g[2]!=0)){printf("check FAIL: a texture read back after one failed mapping: %02X%02X%02X, error %04X (want red, no error)\n",g[0],g[1],g[2],(unsigned)e);fails++;}
    if(fl==2){if(e!=GL_OUT_OF_MEMORY){printf("check FAIL: a texture read back that could not map said error %04X (want GL_OUT_OF_MEMORY)\n",(unsigned)e);fails++;}
     memset(g,0,sizeof(g));glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,g);
     if(g[0]!=255||g[2]!=0){printf("check FAIL: the read after a failed one got %02X%02X%02X (want red: the level stays marked)\n",g[0],g[1],g[2]);fails++;}}
    /* a colour renderbuffer read through swrast's span functions */
    scene_rb_gen(1,&rb);scene_rb_bind(GL_RENDERBUFFER_EXT,rb);scene_rb_storage(GL_RENDERBUFFER_EXT,GL_RGB8,16,16);scene_fbo_bind(GL_FRAMEBUFFER_EXT,fb);
    scene_fbo_rb(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_RENDERBUFFER_EXT,rb);glClearColor(1,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glFinish();while(glGetError()!=GL_NO_ERROR);
    host_map_fail=fl;memset(px,0,4);glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);e=glGetError();host_map_fail=0;
    if(fl==1&&(e!=GL_NO_ERROR||px[0]!=255||px[2]!=0)){printf("check FAIL: a renderbuffer read after one failed mapping: %02X%02X%02X, error %04X (want red, no error)\n",px[0],px[1],px[2],(unsigned)e);fails++;}
    if(fl==2&&e!=GL_OUT_OF_MEMORY){printf("check FAIL: a renderbuffer read that could not map: %02X%02X%02X, error %04X (want GL_OUT_OF_MEMORY)\n",px[0],px[1],px[2],(unsigned)e);fails++;}
    scene_fbo_bind(GL_FRAMEBUFFER_EXT,0);scene_fbo_del(1,&fb);scene_rb_del(1,&rb);glDeleteTextures(1,&t);rtt_view(W,H);glClearColor(0,0,0,1);glColor3f(1,1,1);}}
  if(pass==0&&scene_fbo_gen){ /* v10.322 (board, v10.321 scene 56): glCopyTexSubImage2D right after switching the attachment reads the new one
   (Mesa validated the read framebuffer only for the transfer state: the copy read the texture attached before), engine and software */
   int r;for(r=0;r<2;r++){GLuint fb,ta,tc,td;static GLubyte got2[16*16*4];vxgl_gl_rtt=!r;
    ta=rtt_tex(16,16,GL_RGB);tc=rtt_tex(16,16,GL_RGB);td=rtt_tex(16,16,GL_RGB);scene_fbo_gen(1,&fb);scene_fbo_bind(GL_FRAMEBUFFER_EXT,fb);rtt_view(16,16);
    scene_fbo_tex2d(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,ta,0);glColor3f(1,0,0);glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(16,0);glVertex2f(16,16);glVertex2f(0,16);glEnd();
    scene_fbo_tex2d(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,tc,0);glColor3f(0,0,1);glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(16,0);glVertex2f(16,16);glVertex2f(0,16);glEnd();
    scene_fbo_tex2d(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,ta,0);
    glBindTexture(GL_TEXTURE_2D,td);glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,0,0,16,16);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,got2);
    if(got2[0]!=255||got2[2]!=0||got2[(15*16+15)*4]!=255){printf("check FAIL: ACCEL_OPENGL_RTT %d: a copy after switching the attachment read %02X%02X%02X (want red, the texture attached now)\n",!r,got2[0],got2[1],got2[2]);fails++;}
    scene_fbo_bind(GL_FRAMEBUFFER_EXT,0);scene_fbo_del(1,&fb);glDeleteTextures(1,&ta);glDeleteTextures(1,&tc);glDeleteTextures(1,&td);rtt_view(W,H);glColor3f(1,1,1);}
   vxgl_gl_rtt=1;}
  glClearColor(1,0,0,1);glDrawBuffer(GL_BACK);glClear(GL_COLOR_BUFFER_BIT);
  glDrawBuffer(GL_FRONT);glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);glDrawBuffer(GL_BACK);glFinish();
  glPixelStorei(GL_PACK_ALIGNMENT,1);
  glReadBuffer(GL_BACK);glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  if(px[0]<200||px[2]>50){printf("check FAIL (%s): back after a front clear %02X%02X%02X, want red\n",who,px[0],px[1],px[2]);fails++;}
  glReadBuffer(GL_FRONT);glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  if(px[2]<200||px[0]>50){printf("check FAIL (%s): front %02X%02X%02X, want blue\n",who,px[0],px[1],px[2]);fails++;}
  { /* v10.317: a shaded, blended triangle drawn into the front only (ACCEL_OPENGL_FRONT): by the engine with it on, the same picture as swrast's,
     * the back left alone */
   static GLubyte fimg[2][W*H*4];GLuint sw0=c->stats[3],hw0=c->stats[0];
   glDrawBuffer(GL_FRONT);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
   glBegin(GL_TRIANGLES);glColor4f(1,0,0,0.8f);glVertex2f(20,20);glColor4f(0,1,0,0.5f);glVertex2f(230,40);glColor4f(1,1,1,1);glVertex2f(120,240);glEnd();
   glDisable(GL_BLEND);glColor4f(1,1,1,1);glDrawBuffer(GL_BACK);glFinish();
   if(pass==0&&(c->stats[3]!=sw0||c->stats[0]!=hw0+1)){printf("check FAIL: the front triangle was not drawn by the engine (software %u, engine %u)\n",c->stats[3]-sw0,c->stats[0]-hw0);fails++;}
   glReadBuffer(GL_FRONT);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,fimg[pass]);
   glReadBuffer(GL_BACK);glReadPixels(120,100,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
   if(px[0]<200||px[1]>50||px[2]>50){printf("check FAIL (%s): back under the front triangle %02X%02X%02X, want red\n",who,px[0],px[1],px[2]);fails++;}
   if(pass==1){int i,ch,d=0;for(i=0;i<W*H;i++)for(ch=0;ch<3;ch++){int a=fimg[0][i*4+ch],b=fimg[1][i*4+ch];if(a-b>16||b-a>16){d++;break;}}
    if(d>W*H/200){printf("check FAIL: the front triangle, engine against software: %d pixels differ\n",d);fails++;}}}
  { /* v10.340: a shaded, blended triangle drawn into GL_FRONT_AND_BACK: by the engine (twice), both buffers as swrast draws them */
   static GLubyte bimg[2][2][W*H*4];GLuint sw0=c->stats[3];int b2;
   glDrawBuffer(GL_FRONT_AND_BACK);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
   glBegin(GL_TRIANGLES);glColor4f(0,0,1,0.6f);glVertex2f(40,200);glColor4f(1,1,0,0.4f);glVertex2f(240,220);glColor4f(0,1,1,1);glVertex2f(60,10);glEnd();
   glDisable(GL_BLEND);glColor4f(1,1,1,1);glDrawBuffer(GL_BACK);glFinish();
   if(pass==0&&c->stats[3]!=sw0){printf("check FAIL: the GL_FRONT_AND_BACK triangle went to software\n");fails++;}
   glReadBuffer(GL_FRONT);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,bimg[pass][0]);glReadBuffer(GL_BACK);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,bimg[pass][1]);
   if(pass==1)for(b2=0;b2<2;b2++){int i,ch,d=0;for(i=0;i<W*H;i++)for(ch=0;ch<3;ch++){int x=bimg[0][b2][i*4+ch],y=bimg[1][b2][i*4+ch];if(x-y>16||y-x>16){d++;break;}}
    printf("  GL_FRONT_AND_BACK triangle, %s: %d pixels differ from software\n",b2?"back":"front",d);
    if(d>W*H/200){printf("check FAIL: the GL_FRONT_AND_BACK triangle's %s buffer, engine against software: %d pixels differ\n",b2?"back":"front",d);fails++;}}}
  if(host_zbits){ /* Quake 2 3.14's state (GL_SetDefaultState with point parameters) and its world: texture with GL_REPLACE, then the lightmap
     * pass (depth GL_EQUAL, no depth writes, glBlendFunc(GL_ZERO,GL_SRC_COLOR)), then attenuated smooth particles; engine against software */
   static GLubyte qimg[2][W*H*4];static GLubyte tx[16*16*4],lm[16*16*4];GLuint tex[2];int i;GLuint qsw0=c->stats[3],qhw0=c->stats[0];GLfloat att[3]={0.01f,0.0f,0.01f};
   for(i=0;i<256;i++){tx[i*4]=(GLubyte)(120+(i&7)*10);tx[i*4+1]=(GLubyte)(100+((i>>4)&7)*12);tx[i*4+2]=(GLubyte)(80+(i%5)*20);tx[i*4+3]=255;
    {int r=40+(i&15)*12,g=30+(i>>4)*10,b=20+((i*7)&15)*8,mx=r>g?r:g;if(b>mx)mx=b;lm[i*4]=(GLubyte)r;lm[i*4+1]=(GLubyte)g;lm[i*4+2]=(GLubyte)b;lm[i*4+3]=(GLubyte)mx;}}
   glGenTextures(2,tex);
   glBindTexture(GL_TEXTURE_2D,tex[0]);glTexImage2D(GL_TEXTURE_2D,0,3,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,tx);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
   glBindTexture(GL_TEXTURE_2D,tex[1]);glTexImage2D(GL_TEXTURE_2D,0,3,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,lm);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
   glClearColor(1,0,0.5f,0.5f);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
   glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.666f);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glColor4f(1,1,1,1);glShadeModel(GL_FLAT);
   glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
   glEnable(GL_POINT_SMOOTH);glPointParameterfEXT(GL_POINT_SIZE_MIN_EXT,2);glPointParameterfEXT(GL_POINT_SIZE_MAX_EXT,40);glPointParameterfvEXT(GL_DISTANCE_ATTENUATION_EXT,att);
   glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);
   glBindTexture(GL_TEXTURE_2D,tex[0]);glBegin(GL_POLYGON);glTexCoord2f(0,0);glVertex3f(10,10,0.3f);glTexCoord2f(4,0);glVertex3f(250,20,0.5f);glTexCoord2f(4,4);glVertex3f(240,240,0.6f);glTexCoord2f(0,4);glVertex3f(20,230,0.4f);glEnd();
   glDepthMask(GL_FALSE);glDepthFunc(GL_EQUAL);glEnable(GL_BLEND);glBlendFunc(GL_ZERO,GL_SRC_COLOR);
   glBindTexture(GL_TEXTURE_2D,tex[1]);glBegin(GL_POLYGON);glTexCoord2f(0,0);glVertex3f(10,10,0.3f);glTexCoord2f(1,0);glVertex3f(250,20,0.5f);glTexCoord2f(1,1);glVertex3f(240,240,0.6f);glTexCoord2f(0,1);glVertex3f(20,230,0.4f);glEnd();
   glDisable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthFunc(GL_LEQUAL);
   glDepthMask(GL_FALSE);glEnable(GL_BLEND);glDisable(GL_TEXTURE_2D);glPointSize(40);
   glBegin(GL_POINTS);for(i=0;i<20;i++){glColor4f(1,0.8f,0.2f,0.6f);glVertex3f(30.0f+i*10,50.0f+(i%5)*30,0.1f);}glEnd();
   glDisable(GL_BLEND);glColor4f(1,1,1,1);glDepthMask(GL_TRUE);glEnable(GL_TEXTURE_2D);glFinish();
   { /* dynamic lightmaps: Quake 2 rewrites parts of a lightmap block with glTexSubImage2D between draws (a light on, then the block's
      * own values back), many frames; the last frame against software */
    int f,k;static GLubyte sub[8*8*4];
    for(f=0;f<40;f++){int on=f&1;
     for(k=0;k<64;k++){sub[k*4]=(GLubyte)(on?255:lm[(k/8*16+k%8)*4]);sub[k*4+1]=(GLubyte)(on?200:lm[(k/8*16+k%8)*4+1]);sub[k*4+2]=(GLubyte)(on?100:lm[(k/8*16+k%8)*4+2]);sub[k*4+3]=255;}
     glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glDisable(GL_BLEND);glDepthMask(GL_TRUE);glDepthFunc(GL_LEQUAL);glEnable(GL_TEXTURE_2D);
     glBindTexture(GL_TEXTURE_2D,tex[0]);glBegin(GL_POLYGON);glTexCoord2f(0,0);glVertex3f(10,10,0.3f);glTexCoord2f(4,0);glVertex3f(250,20,0.5f);glTexCoord2f(4,4);glVertex3f(240,240,0.6f);glTexCoord2f(0,4);glVertex3f(20,230,0.4f);glEnd();
     glBindTexture(GL_TEXTURE_2D,tex[1]);glTexSubImage2D(GL_TEXTURE_2D,0,(f%3)*4,(f%2)*8,8,8,GL_RGBA,GL_UNSIGNED_BYTE,sub);
     glDepthMask(GL_FALSE);glDepthFunc(GL_EQUAL);glEnable(GL_BLEND);glBlendFunc(GL_ZERO,GL_SRC_COLOR);
     glBegin(GL_POLYGON);glTexCoord2f(0,0);glVertex3f(10,10,0.3f);glTexCoord2f(1,0);glVertex3f(250,20,0.5f);glTexCoord2f(1,1);glVertex3f(240,240,0.6f);glTexCoord2f(0,1);glVertex3f(20,230,0.4f);glEnd();
     glDisable(GL_TEXTURE_2D);glPointSize(40);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glDepthFunc(GL_LEQUAL);
     glBegin(GL_POINTS);for(k=0;k<5;k++){glColor4f(1,0.8f,0.2f,0.6f);glVertex3f(30.0f+k*40,50.0f+f,0.1f);}glEnd();glColor4f(1,1,1,1);glEnable(GL_TEXTURE_2D);glDisable(GL_BLEND);
     if(f==20){ /* the menu: Draw_FadeScreen (2D: no depth test), then Quake's colour back to white */
      glDisable(GL_DEPTH_TEST);glEnable(GL_BLEND);glDisable(GL_TEXTURE_2D);glColor4f(0,0,0,0.8f);
      glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(W,0);glVertex2f(W,H);glVertex2f(0,H);glEnd();
      glColor4f(1,1,1,1);glEnable(GL_TEXTURE_2D);glDisable(GL_BLEND);glEnable(GL_DEPTH_TEST);}}
    glDepthMask(GL_TRUE);glFinish();}
   glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,qimg[pass]);
   /* v10.353: the points (untextured, smooth, the alpha test on) on the engine too (aa_atest) */
   if(pass==0&&(c->stats[3]-qsw0!=0||c->stats[0]-qhw0<82)){printf("check FAIL: Quake 2 3.14 scene (point smoothing and the alpha test on): software %u draws (want 0), engine %u (want the 82 polygons and the points)\n",c->stats[3]-qsw0,c->stats[0]-qhw0);fails++;}
   glDisable(GL_POINT_SMOOTH);att[0]=1;att[2]=0;glPointParameterfvEXT(GL_DISTANCE_ATTENUATION_EXT,att);glPointSize(1);glDisable(GL_ALPHA_TEST);glDisable(GL_DEPTH_TEST);glShadeModel(GL_SMOOTH);
   glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glDisable(GL_TEXTURE_2D);glDeleteTextures(2,tex);
   if(pass==1){int ch,d=0;long sa=0,sb=0;for(i=0;i<W*H;i++){for(ch=0;ch<3;ch++){sa+=qimg[0][i*4+ch];sb+=qimg[1][i*4+ch];}for(ch=0;ch<3;ch++){int a=qimg[0][i*4+ch],b=qimg[1][i*4+ch];if(a-b>16||b-a>16){d++;break;}}}
    printf("  Quake 2 3.14 scene: %d pixels differ, mean engine %.1f software %.1f\n",d,sa/(3.0*W*H),sb/(3.0*W*H));
    if(d>W*H/200){printf("check FAIL: the Quake 2 3.14 scene, engine against software: %d pixels differ\n",d);fails++;}}}
  glReadBuffer(GL_BACK);
  if(host_cbits==32){glClearColor(0,0,0,0.25f);glClear(GL_COLOR_BUFFER_BIT);glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
   if(px[3]!=255){printf("check FAIL (%s): alpha %u from an X8R8G8B8 buffer, want 255\n",who,px[3]);fails++;}}
  if(host_zbits==24){
   glClearStencil(0);glClear(GL_STENCIL_BUFFER_BIT);glEnable(GL_STENCIL_TEST);
   glStencilFuncSeparate(GL_FRONT,GL_ALWAYS,1,0xFF);glStencilFuncSeparate(GL_BACK,GL_ALWAYS,3,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
   {GLuint sw0=c->stats[3],hw0=c->stats[0];
   glBegin(GL_TRIANGLES);glVertex2f(20,30);glVertex2f(60,200);glVertex2f(110,40);glEnd(); /* clockwise: a back face */
   glBegin(GL_TRIANGLES);glVertex2f(150,30);glVertex2f(240,40);glVertex2f(190,200);glEnd(); /* counter-clockwise: a front face */
   glDisable(GL_STENCIL_TEST);glFinish();glReadPixels(63,90,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&st);
   if(st!=3){printf("check FAIL (%s): back-face stencil %u, want 3 (the back state)\n",who,st);fails++;}
   glReadPixels(190,80,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&st);
   if(st!=1){printf("check FAIL (%s): front-face stencil %u, want 1 (the front state)\n",who,st);fails++;}
   /* v10.263: two-sided stencil is drawn by the engine (it was swrast) */
   if(pass==0&&(c->stats[3]!=sw0||c->stats[0]<hw0+2)){printf("check FAIL (%s): two-sided stencil not drawn by the engine (software %u, engine %u)\n",who,c->stats[3]-sw0,c->stats[0]-hw0);fails++;}}
  }
  if(pass==0){ /* v10.319 (audit M1): an RGB image of exactly its size, uploaded with a swizzle (to RGBA and to BGR-ordered storage): ASan
    * catches a read past its last pixel */
   /* Mesa is built without ASan here: the image ends at a page with no access, so a read past it faults */
   GLuint id;GLubyte *pg=(GLubyte*)mmap(0,8192,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0),*rgb,back[16];int i;
   if(pg==(GLubyte*)MAP_FAILED){printf("check FAIL: no guard page\n");return 1;}
   mprotect(pg+4096,4096,PROT_NONE);rgb=pg+4096-12;for(i=0;i<12;i++)rgb[i]=(GLubyte)(i*20+5);
   glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
   glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,2,2,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);
   glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_BGR,GL_UNSIGNED_BYTE,rgb);glPixelStorei(GL_PACK_ALIGNMENT,1);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,back);
   if(back[12]!=rgb[11]||back[14]!=rgb[9]||back[15]!=255){printf("check FAIL: BGR upload read back %02X%02X%02X%02X, want %02X%02X%02XFF\n",back[12],back[13],back[14],back[15],rgb[11],rgb[10],rgb[9]);fails++;}
   glPixelStorei(GL_UNPACK_ALIGNMENT,4);munmap(pg,8192);glDeleteTextures(1,&id);}
  if(pass==0){ /* v10.319 (audit G2, G4): DXT1 from a 4x4 block half red, half green (one colour came out of the encoder), and the same block
    * at SKIP_ROWS 2 of a taller image (the fast path read from the image's start) */
   static GLubyte img[12*6*3];GLuint id;GLubyte back[16*4];int x,y,bad=0,run;
   for(run=0;run<2;run++){
    memset(img,0,sizeof(img));
    for(y=0;y<4;y++)for(x=0;x<4;x++){GLubyte *q=run?img+((y+2)*4+x)*3:img+(y*4+x)*3;q[0]=x<2?255:0;q[1]=x<2?0:255;q[2]=0;}
    glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    if(run)glPixelStorei(GL_UNPACK_SKIP_ROWS,2); /* rows still 12 bytes apart: the fast path's stride check passes */
    glTexImage2D(GL_TEXTURE_2D,0,GL_COMPRESSED_RGB_S3TC_DXT1_EXT,4,4,0,GL_RGB,GL_UNSIGNED_BYTE,img);
    glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
    glPixelStorei(GL_PACK_ALIGNMENT,1);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,back);
    for(y=0;y<4;y++)for(x=0;x<4;x++){const GLubyte *q=back+(y*4+x)*4;if(x<2?(q[0]<200||q[1]>50):(q[1]<200||q[0]>50))bad++;}
    if(bad){printf("check FAIL: DXT1 red/green block%s: %d of 16 texels wrong (first %02X%02X%02X)\n",run?" with skips":"",bad,back[0],back[1],back[2]);fails++;bad=0;}
    glDeleteTextures(1,&id);}}
  if(pass==0){ /* v10.319 (audit G3), v10.321: VERTEX_PROGRAM_POINT_SIZE without a vertex program changes nothing (GL takes glPointSize): the
    * engine draws the point and a triangle (the vertex program's own sizes are scene 49) */
   GLuint sw0=c->stats[3];glEnable(GL_VERTEX_PROGRAM_POINT_SIZE_ARB);glPointSize(5);glBegin(GL_POINTS);glVertex2f(30,30);glEnd();
   glBegin(GL_TRIANGLES);glVertex2f(10,10);glVertex2f(60,10);glVertex2f(30,50);glEnd();glFinish();glDisable(GL_VERTEX_PROGRAM_POINT_SIZE_ARB);glPointSize(1);
   if(c->stats[3]!=sw0){printf("check FAIL: a point or triangle with VERTEX_PROGRAM_POINT_SIZE on (no vertex program) went to software\n");fails++;}}
  if(pass==0){ /* v10.321: antialiasing on the engine unless the alpha test is on (swrast applies the coverage after it) or a texture stage
    * replaces the alpha (REPLACE with an RGBA texture); MODULATE keeps it (scene 50 draws them) */
   GLuint sw0=c->stats[3],hw0=c->stats[0],t=scene_tex(0),n;glEnable(GL_LINE_SMOOTH);
   glBegin(GL_LINES);glVertex2f(10,10);glVertex2f(60,30);glEnd();glFinish();
   if(c->stats[3]!=sw0||c->stats[0]==hw0){printf("check FAIL: a smooth line went to software\n");fails++;}
   /* v10.353: untextured with the alpha test: on the engine, the test made per line (aa_atest); textured: software */
   glEnable(GL_ALPHA_TEST);sw0=c->stats[3];glBegin(GL_LINES);glVertex2f(10,10);glVertex2f(60,30);glEnd();glFinish();
   if(c->stats[3]!=sw0){printf("check FAIL: an untextured smooth line with the alpha test went to software\n");fails++;}
   glBindTexture(GL_TEXTURE_2D,t);glEnable(GL_TEXTURE_2D);sw0=c->stats[3];glBegin(GL_LINES);glTexCoord2f(0,0);glVertex2f(10,10);glTexCoord2f(1,1);glVertex2f(60,30);glEnd();glFinish();glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);
   if(c->stats[3]==sw0){printf("check FAIL: a textured smooth line with the alpha test drew on the engine\n");fails++;}
   glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);sw0=c->stats[3];glBegin(GL_LINES);glTexCoord2f(0,0);glVertex2f(10,10);glVertex2f(60,30);glEnd();glFinish();
   if(c->stats[3]==sw0){printf("check FAIL: a smooth line with an RGBA texture REPLACE drew on the engine\n");fails++;}
   glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);sw0=c->stats[3];n=c->stats[0];glBegin(GL_LINES);glTexCoord2f(0,0);glVertex2f(10,10);glVertex2f(60,30);glEnd();glFinish();
   if(c->stats[3]!=sw0||c->stats[0]==n){printf("check FAIL: a smooth line with a texture MODULATE went to software\n");fails++;}
   glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&t);glDisable(GL_LINE_SMOOTH);}
  if(pass==0&&vxgl_gl_units3){ /* v10.319 (audit G1): three units, the third's s, t as unit 0's but its r not, the same texture matrix mixing r
    * into s: not unit 0's coordinates, so software (it drew with unit 0's) */
   static const GLfloat m[16]={1,0,0,0, 0,1,0,0, 1,0,1,0, 0,0,0,1};static GLubyte t4[4*4]={255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};
   GLuint id,sw0=c->stats[3],hw0=c->stats[0];int u;glGenTextures(1,&id);
   for(u=0;u<3;u++){glActiveTextureARB(GL_TEXTURE0_ARB+u);glBindTexture(GL_TEXTURE_2D,id);if(!u)glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,t4);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glEnable(GL_TEXTURE_2D);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glMatrixMode(GL_TEXTURE);if(u==1)glLoadIdentity();else glLoadMatrixf(m);glMatrixMode(GL_MODELVIEW);}
   glBegin(GL_TRIANGLES);
   for(u=0;u<3;u++){static const GLfloat v[3][2]={{10,10},{100,10},{50,90}};glMultiTexCoord3fARB(GL_TEXTURE0_ARB,u*0.3f,0.2f,0);glMultiTexCoord2fARB(GL_TEXTURE1_ARB,0.5f,0.5f);
    glMultiTexCoord3fARB(GL_TEXTURE2_ARB,u*0.3f,0.2f,0.5f);glVertex2fv(v[u]);}
   glEnd();glFinish();
   if(c->stats[3]==sw0){printf("check FAIL: a third unit whose r differs drew on the engine with unit 0's coordinates (engine %u)\n",c->stats[0]-hw0);fails++;}
   for(u=2;u>=0;u--){glActiveTextureARB(GL_TEXTURE0_ARB+u);glMatrixMode(GL_TEXTURE);glLoadIdentity();glMatrixMode(GL_MODELVIEW);glDisable(GL_TEXTURE_2D);}
   glDeleteTextures(1,&id);}
  { /* v10.263: culling both faces: filled polygons dropped, lines drawn, all on the engine (it was swrast) */
   GLuint sw0=c->stats[3];
   glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glEnable(GL_CULL_FACE);glCullFace(GL_FRONT_AND_BACK);glColor3f(1,1,1);
   glBegin(GL_TRIANGLES);glVertex2f(10,10);glVertex2f(100,10);glVertex2f(10,100);glVertex2f(10,10);glVertex2f(10,100);glVertex2f(100,10);glEnd();
   glBegin(GL_LINES);glVertex2f(5,150.5f);glVertex2f(200,150.5f);glEnd();
   glDisable(GL_CULL_FACE);glFinish();glReadPixels(30,30,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
   if(px[0]>50){printf("check FAIL (%s): a triangle was drawn with both faces culled (%02X)\n",who,px[0]);fails++;}
   glReadPixels(100,150,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
   if(px[0]<200){printf("check FAIL (%s): the line was not drawn with both faces culled (%02X)\n",who,px[0]);fails++;}
   if(pass==0&&c->stats[3]!=sw0){printf("check FAIL (%s): culling both faces fell back to software (%u primitives)\n",who,c->stats[3]-sw0);fails++;}
  }
  { /* v10.279 (review): an occlusion query counts the engine's draws too (they go to software while it counts): a 100x100 quad */
   void (APIENTRY *gen)(GLsizei,GLuint*)=(void (APIENTRY *)(GLsizei,GLuint*))host_gp("glGenQueriesARB");
   void (APIENTRY *begin)(GLenum,GLuint)=(void (APIENTRY *)(GLenum,GLuint))host_gp("glBeginQueryARB");
   void (APIENTRY *end)(GLenum)=(void (APIENTRY *)(GLenum))host_gp("glEndQueryARB");
   void (APIENTRY *get)(GLuint,GLenum,GLuint*)=(void (APIENTRY *)(GLuint,GLenum,GLuint*))host_gp("glGetQueryObjectuivARB");
   GLuint q=0,n=0,hw0=c->stats[0],sw0=c->stats[3];unsigned long q0=host_queries;
   if(!gen||!begin||!end||!get){printf("check FAIL (%s): no occlusion query entry points\n",who);fails++;}
   else {glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);gen(1,&q);begin(GL_SAMPLES_PASSED_ARB,q);
    glBegin(GL_QUADS);glVertex2f(20,20);glVertex2f(120,20);glVertex2f(120,120);glVertex2f(20,120);glEnd();end(GL_SAMPLES_PASSED_ARB);
    get(q,GL_QUERY_RESULT_ARB,&n);
    if(n!=10000){printf("check FAIL (%s): occlusion query %u samples for a 100x100 quad, want 10000\n",who,n);fails++;}
    /* v10.315: the query counted by the engine (VX3DN_query): the quad drawn by it, the count set and read through the VxD */
    if(pass==0&&(c->stats[3]!=sw0||host_queries!=q0+2)){printf("check FAIL (%s): the occlusion query was not counted by the engine (software %u, VxD calls %lu)\n",who,c->stats[3]-sw0,host_queries-q0);fails++;}
    /* v10.315: half of a far quad behind a near one: 5000 */
    glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);
    glBegin(GL_QUADS);glVertex3f(20,20,0.5f);glVertex3f(70,20,0.5f);glVertex3f(70,120,0.5f);glVertex3f(20,120,0.5f);glEnd();
    begin(GL_SAMPLES_PASSED_ARB,q);glBegin(GL_QUADS);glVertex3f(20,20,-0.5f);glVertex3f(120,20,-0.5f);glVertex3f(120,120,-0.5f);glVertex3f(20,120,-0.5f);glEnd();end(GL_SAMPLES_PASSED_ARB);
    get(q,GL_QUERY_RESULT_ARB,&n);glDisable(GL_DEPTH_TEST);
    if(n!=5000){printf("check FAIL (%s): occlusion query %u samples for a quad half behind another, want 5000\n",who,n);fails++;}
    glBegin(GL_TRIANGLES);glVertex2f(10,10);glVertex2f(50,10);glVertex2f(10,50);glEnd();glFinish();
    if(pass==0&&c->stats[0]==hw0){printf("check FAIL (%s): after the query the engine draws nothing\n",who);fails++;}}
  }
  if(pass==0) { /* v10.279 (review): a queued group answered VX3DN_LOST: the context's core hears of it at its next packet */
   unsigned long lost0=vxgl_batch_lost;
   glBegin(GL_TRIANGLES);glVertex2f(10,10);glVertex2f(50,10);glVertex2f(10,50);glEnd();host_lose=1;glFlush();
   if(vxgl_batch_lost!=lost0+1){printf("check FAIL: a lost queued group not counted\n");fails++;}
   glBegin(GL_TRIANGLES);glVertex2f(60,10);glVertex2f(90,10);glVertex2f(60,50);glEnd();glFlush();
   if(!c->core.lost){printf("check FAIL: the context's core was not marked lost after its queued packets found the engine lost\n");fails++;}
   host_lose=0;
  }
  if(pass==0) { /* v10.287 (review): textures. A deleted texture's copy freed while the core's batch still held a draw made with it;
                  a lost copy restored elsewhere and sampled at its old place; a copy that found no memory tried again at every state change */
   GLuint t[3],hw0,a0,i;GLubyte red[16*4],green[16*4];
   for(i=0;i<16;i++){red[i*4]=255;red[i*4+1]=0;red[i*4+2]=0;red[i*4+3]=255;green[i*4]=0;green[i*4+1]=255;green[i*4+2]=0;green[i*4+3]=255;}
   reset();glPixelStorei(GL_UNPACK_ALIGNMENT,1);glGenTextures(3,t);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
#define HQUAD(x0,y0,x1,y1) do{glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(x0,y0);glTexCoord2f(1,0);glVertex2f(x1,y0);glTexCoord2f(1,1);glVertex2f(x1,y1);glTexCoord2f(0,1);glVertex2f(x0,y1);glEnd();}while(0)
#define HTEX(n,px) do{glBindTexture(GL_TEXTURE_2D,n);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);\
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,px);}while(0)
   HTEX(t[0],red);glEnable(GL_TEXTURE_2D);
   hw0=c->stats[0];HQUAD(20,20,60,60);glDeleteTextures(1,&t[0]);glDisable(GL_TEXTURE_2D);glFinish();
   glReadPixels(40,40,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
   if(c->stats[0]==hw0){printf("check FAIL: the textured quad was not drawn by the engine\n");fails++;}
   else if(px[0]<200||px[1]>50||px[2]>50){printf("check FAIL: a texture deleted with its draw in the core's batch: %02X%02X%02X, want red (its freed memory sampled)\n",px[0],px[1],px[2]);fails++;}
   HTEX(t[1],green);glEnable(GL_TEXTURE_2D);HQUAD(20,20,60,60);glFinish();
   host_tex_lose++;c->new_state|=~0U; /* a mode change: the copy lost, the state translated again (core_targets after the buffers are made again) */
   glClear(GL_COLOR_BUFFER_BIT);HQUAD(20,20,60,60);glFinish();glReadPixels(40,40,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
   if(px[0]>50||px[1]<200||px[2]>50){printf("check FAIL: a restored texture copy drawn %02X%02X%02X, want green (its old offsets sampled)\n",px[0],px[1],px[2]);fails++;}
   host_tex_fail=1;HTEX(t[2],red);a0=host_tex_allocs;
   for(i=0;i<4;i++){if(i&1)glEnable(GL_BLEND);else glDisable(GL_BLEND);glBlendFunc(GL_ONE,GL_ZERO);HQUAD(20,20,60,60);}
   glDisable(GL_BLEND);glFinish();glReadPixels(40,40,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
   if(host_tex_allocs-a0!=1){printf("check FAIL: a texture copy with no memory for it tried %u times over 4 state changes, want 1\n",host_tex_allocs-a0);fails++;}
   if(px[0]<200||px[1]>50){printf("check FAIL: a texture without a copy drawn %02X%02X%02X, want red (software)\n",px[0],px[1],px[2]);fails++;}
   host_tex_fail=0;glBindTexture(GL_TEXTURE_2D,0);glDeleteTextures(1,&t[1]);glBindTexture(GL_TEXTURE_2D,t[2]);a0=host_tex_allocs;hw0=c->stats[0];
   glClear(GL_COLOR_BUFFER_BIT);HQUAD(20,20,60,60);glFinish();glReadPixels(40,40,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
   if(host_tex_allocs-a0!=1||c->stats[0]==hw0){printf("check FAIL: after memory was given back the copy was tried %u times, the engine drew %u triangles; want 1, some\n",host_tex_allocs-a0,c->stats[0]-hw0);fails++;}
   if(px[0]<200||px[1]>50){printf("check FAIL: the texture's copy made after memory was given back drawn %02X%02X%02X, want red\n",px[0],px[1],px[2]);fails++;}
   glDeleteTextures(1,&t[2]);glDisable(GL_TEXTURE_2D);
   { /* v10.288 (review): a mode change, the surfaces made again at the same size and offsets: the core follows (it kept the old
       generation, and the VxD refused the frame STALE until the swap after) */
    GLuint hw1=c->stats[0];
    host_gen++;host_remake=1;glViewport(0,0,W,H); /* the viewport call validates the buffers (resize), as a swap or the next frame does */
    glClear(GL_COLOR_BUFFER_BIT);glColor3f(0,0,1);glBegin(GL_TRIANGLES);glVertex2f(10,10);glVertex2f(100,10);glVertex2f(10,100);glEnd();glFinish();
    glReadPixels(30,30,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
    if(px[2]<200||c->stats[0]==hw1||c->core.lost){printf("check FAIL: after the surfaces were made again in place the triangle read %02X%02X%02X (engine %u, lost %u), want blue\n",px[0],px[1],px[2],c->stats[0]-hw1,c->core.lost);fails++;}
    glColor3f(1,1,1);
   }
#undef HQUAD
#undef HTEX
  }
  if(glGetError()){printf("check FAIL (%s): GL error\n",who);fails++;}
  /* v10.259: a context current in thread 1 cannot be made current in thread 2 */
  host_tid=2;if(vxgl_make_current(c,(HDC)1)){printf("check FAIL (%s): a context current in another thread was made current\n",who);fails++;}host_tid=1;
  host_tid=3;vxgl_thread_gone();host_tid=1; /* another thread ends: the context stays current here */
  {GET_CURRENT_CONTEXT(g);if(g!=&c->gl||c->owner!=1){printf("check FAIL (%s): another thread's end released this thread's context\n",who);fails++;}}
  vxgl_make_current(0,0);
  if(c->owner){printf("check FAIL (%s): released context still owned by thread %u\n",who,(unsigned)c->owner);fails++;}
  vxgl_destroy_context(c);vxgl_fb_release(&thefb);
 }
 { /* v10.259: a context without its swsetup / tnl parts (the 3D Pipes fault) is not torn down again; v10.261: it is detached (not current,
    no owner), quarantined (MakeCurrent refuses it) and the delete reports it was not freed */
  vxgl_ctx *c=vxgl_create_context(1,(HDC)1,0);void *ss,*tn;
  memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
  if(!c||!vxgl_make_current(c,(HDC)1)){printf("checks: no context\n");return fails+1;}
  ss=c->gl.swsetup_context;tn=c->gl.swtnl_context;c->gl.swsetup_context=0;c->gl.swtnl_context=0;
  if(vxgl_destroy_context(c)){printf("check FAIL: a broken context reported freed\n");fails++;} /* faulted in _tnl_free_vertices before v10.259 */
  {GET_CURRENT_CONTEXT(g);if(g||c->owner||c->magic!=VXGL_BROKEN){printf("check FAIL: a broken context left current (%p), owned (%u) or not quarantined\n",(void*)g,(unsigned)c->owner);fails++;}}
  if(vxgl_make_current(c,(HDC)1)){printf("check FAIL: a quarantined context was made current\n");fails++;}
  c->gl.swsetup_context=ss;c->gl.swtnl_context=tn;c->magic=VXGL_MAGIC;
  if(!vxgl_destroy_context(c)){printf("check FAIL: a whole context reported not freed\n");fails++;}
  vxgl_fb_release(&thefb);
 }
 { /* v10.261 (review): with no front storage at all a front clear is lost; it must not come out in the back buffer (it did) */
  vxgl_ctx *c=vxgl_create_context(1,(HDC)1,0);GLubyte px[4];
  memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;host_front_fail=1;
  if(!c||!vxgl_make_current(c,(HDC)1)){printf("checks: no context\n");return fails+1;}
  cur=c;reset();
  glClearColor(1,0,0,1);glDrawBuffer(GL_BACK);glClear(GL_COLOR_BUFFER_BIT);
  glDrawBuffer(GL_FRONT);glClearColor(0,0,1,1);glClear(GL_COLOR_BUFFER_BIT);glDrawBuffer(GL_BACK);glFinish();
  glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glReadPixels(5,5,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);
  if(px[0]<200||px[2]>50){printf("check FAIL: front clear without front storage reached the back buffer %02X%02X%02X, want red\n",px[0],px[1],px[2]);fails++;}
  host_front_fail=0;vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);
 }
 { /* v10.261 (review): a refused switch leaves the old context current and owned by this thread; another thread still cannot bind it */
  vxgl_ctx *a=vxgl_create_context(1,(HDC)1,0),*b=vxgl_create_context(2,(HDC)1,0);
  memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
  if(!a||!b||!vxgl_make_current(a,(HDC)1)){printf("checks: no context\n");return fails+1;}
  if(vxgl_make_current(b,(HDC)1)){printf("check FAIL: a context of another pixel format was bound\n");fails++;} /* pixel format 2 on a format-1 window */
  {GET_CURRENT_CONTEXT(g);if(g!=&a->gl||a->owner!=1){printf("check FAIL: after a refused switch the old context is not current and owned (%p, %u)\n",(void*)g,(unsigned)a->owner);fails++;}}
  host_tid=2;if(vxgl_make_current(a,(HDC)1)){printf("check FAIL: another thread bound the old context after a refused switch\n");fails++;}host_tid=1;
  vxgl_make_current(0,0);
  if(a->owner){printf("check FAIL: the released context still owned\n");fails++;}
  vxgl_destroy_context(a);vxgl_destroy_context(b);vxgl_fb_release(&thefb);
 }
 { /* v10.322 (review): occlusion queries of two contexts: A's count leaves out B's drawing while B is current, B's query (A's counting)
     counts in software, each gets its own samples */
  void (APIENTRY *gen)(GLsizei,GLuint*)=(void (APIENTRY *)(GLsizei,GLuint*))host_gp("glGenQueriesARB");
  void (APIENTRY *begin)(GLenum,GLuint)=(void (APIENTRY *)(GLenum,GLuint))host_gp("glBeginQueryARB");
  void (APIENTRY *end)(GLenum)=(void (APIENTRY *)(GLenum))host_gp("glEndQueryARB");
  void (APIENTRY *get)(GLuint,GLenum,GLuint*)=(void (APIENTRY *)(GLuint,GLenum,GLuint*))host_gp("glGetQueryObjectuivARB");
  vxgl_ctx *a,*b;GLuint qa=0,qb=0,na=0,nb=0;int hwa=0,hwb=0;
  memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;vxgl_hw_enabled=1;
  a=vxgl_create_context(1,(HDC)1,0);b=vxgl_create_context(1,(HDC)1,0);
  if(!a||!b||!vxgl_make_current(a,(HDC)1)||!gen){printf("checks: no context\n");return fails+1;}
  cur=a;reset();glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glBegin(GL_TRIANGLES);glVertex2f(1,1);glVertex2f(2,1);glVertex2f(1,2);glEnd();glFinish();gen(1,&qa);begin(GL_SAMPLES_PASSED_ARB,qa);hwa=a->query_hw;
  glBegin(GL_QUADS);glVertex2f(10,10);glVertex2f(20,10);glVertex2f(20,20);glVertex2f(10,20);glEnd();
  vxgl_make_current(b,(HDC)1);cur=b;reset();
  glBegin(GL_QUADS);glVertex2f(40,40);glVertex2f(60,40);glVertex2f(60,60);glVertex2f(40,60);glEnd();
  gen(1,&qb);begin(GL_SAMPLES_PASSED_ARB,qb);hwb=b->query_hw;
  glBegin(GL_QUADS);glVertex2f(70,70);glVertex2f(100,70);glVertex2f(100,100);glVertex2f(70,100);glEnd();end(GL_SAMPLES_PASSED_ARB);get(qb,GL_QUERY_RESULT_ARB,&nb);
  vxgl_make_current(a,(HDC)1);cur=a;
  glBegin(GL_QUADS);glVertex2f(10,30);glVertex2f(20,30);glVertex2f(20,40);glVertex2f(10,40);glEnd();end(GL_SAMPLES_PASSED_ARB);get(qa,GL_QUERY_RESULT_ARB,&na);
  if(!hwa||hwb||na!=200||nb!=900){printf("check FAIL: two contexts' occlusion queries: A on the engine %d (want 1) %u samples (want 200), B on the engine %d (want 0) %u samples (want 900)\n",hwa,na,hwb,nb);fails++;}
  vxgl_make_current(0,0);vxgl_destroy_context(a);vxgl_destroy_context(b);vxgl_fb_release(&thefb);
 }
 if(host_zbits==24) { /* v10.264: VXSTEN's cases (src/vx_sten_test.c) against their worked-out answers, the engine and the software path */
  extern int vxsten_host(FILE*,int,void*,void*,void*),vxsten_host_compare(FILE*);int pass,f;
  for(pass=0;pass<2;pass++) {
   vxgl_ctx *c;
   memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
   vxgl_hw_enabled=pass==0;c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1)){printf("checks: no context\n");return fails+1;}
   cur=c;
   f=vxsten_host(stdout,pass?2:0,(void*)glStencilFuncSeparate,(void*)glStencilOpSeparate,strstr((const char*)glGetString(GL_EXTENSIONS),"GL_EXT_stencil_two_side")?(void*)_glapi_get_proc_address("glActiveStencilFaceEXT"):(void*)0);
   if(f){printf("check FAIL (%s): VXSTEN cases %d failed\n",pass?"engine off":"engine on",f);fails+=f;}
   if(!pass)printf("VXSTEN on the engine: %lu triangles, %lu lines, %lu points by the engine, %lu fallbacks\n",(unsigned long)c->stats[0],(unsigned long)c->stats[1],(unsigned long)c->stats[2],(unsigned long)c->stats[6]);
   if(!pass&&c->stats[6]){printf("check FAIL: VXSTEN went to software %lu times with the engine on\n",(unsigned long)c->stats[6]);fails++;}
   vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);
  }
  f=vxsten_host_compare(stdout);if(f){printf("check FAIL: VXSTEN engine against software differs in %d cases\n",f);fails+=f;}
  vxgl_hw_enabled=1;
 }
 { /* v10.266: VXTEX's cases (src/vx_tex_test.c) against their worked-out answers, the engine and the software path */
  extern int vxtex_host(FILE*,int,void *(*)(const char*)),vxtex_host_compare(FILE*);int pass,f;FILE *tl=verbose?stdout:fopen("/dev/null","w");
  for(pass=0;pass<2;pass++) {
   vxgl_ctx *c;
   memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
   vxgl_hw_enabled=pass==0;c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1)){printf("checks: no context\n");return fails+1;}
   cur=c;
   host_real_locks=0;host_held_submits=0;host_batches=host_batched=0;
   {int t16=vxgl_gl_tex16;vxgl_gl_tex16=0;f=vxtex_host(tl,pass?2:0,host_gp);vxgl_gl_tex16=t16;} /* v10.366: VXTEX v1.8 runs with VXGL_TEX16=0 (its answers are 8 bits a channel) */
   if(!pass)printf("VXTEX on the engine: %u packets sent through the queue in %u sends\n",host_batched,host_batches); /* v10.275 */
   printf("VXTEX %s: %lu DirectDraw locks for the whole run (swrast's kept locks), %lu engine packets sent while swrast held the surfaces\n",pass?"engine off":"engine on",host_real_locks,host_held_submits);
   if(!pass){extern uint32_t vx_d3d_dropped;printf("VXTEX on the engine: %lu triangles by the engine, %lu primitives by software, %lu fallbacks, %lu primitives the core left out\n",
    (unsigned long)c->stats[0],(unsigned long)c->stats[3],(unsigned long)c->stats[6],(unsigned long)vx_d3d_dropped);}
   if(host_held_submits){printf("check FAIL: engine packets while swrast held the surfaces\n");fails++;}
   if(host_unlock_outside){printf("check FAIL: %lu renderbuffer mappings let go outside the engine lock\n",host_unlock_outside);fails++;}
   if(f){printf("check FAIL (%s): VXTEX cases %d failed\n",pass?"engine off":"engine on",f);fails+=f;}
   vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);
  }
  f=vxtex_host_compare(stdout);if(f){printf("check FAIL: VXTEX engine against software differs in %d cases\n",f);fails+=f;}
  if(tl!=stdout)fclose(tl);
  vxgl_hw_enabled=1;
 }
 { /* v10.342: Q2MENU's cases (src/q2menu.c: Quake 2 3.14's calls, the menu's parts) on the engine and in software */
  extern int q2menu_host(FILE*,int (*)(int),void (*)(void),void (*)(void),void *(*)(const char*));int f=q2menu_host(stdout,q2_begin,q2_swap,q2_end,host_gp);
  if(f){printf("check FAIL: Q2MENU %d cases\n",f);fails+=f;}vxgl_hw_enabled=1;
 }
 printf("checks (front/back buffers, X8R8G8B8 alpha, separate stencil): %d failed\n",fails);
 return fails;
}
static vxgl_ctx *q2c;
static void *host_gp(const char *n);
/* v10.342: GLREPLAY's context: the recorded window's size, the engine on or off */
static int gr_begin(void *u,int engine,unsigned w,unsigned h) {(void)u;host_w=w;host_h=h;host_tex_base=0x1800000;memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
 vxgl_hw_enabled=engine;q2c=vxgl_create_context(1,(HDC)1,0);if(!q2c||!vxgl_make_current(q2c,(HDC)1))return 0;cur=q2c;return 1;}
static void gr_swap(void *u) {extern int vxgl_swap(vxgl_ctx*,HDC);(void)u;vxgl_swap(q2c,(HDC)1);}
static void gr_end(void *u) {(void)u;vxgl_make_current(0,0);vxgl_destroy_context(q2c);vxgl_fb_release(&thefb);q2c=0;}
/* v10.346: the window's depth (and stencil, colour) clear drawn by the engine (ACCEL_OPENGL_ZCLEAR3D, default 1) against the 2D fill
 * (= 0) and swrast: an app's hostile state at glClear (alpha test NEVER, depth func NEVER, blend, two texture units, a clip plane, culling,
 * lines, a small viewport and depth range, a matrix pushed) must not change what is cleared; the scissor and the write masks must; the
 * app's state, matrices, attribute stack and glGetError come back as they were */
/* v10.353: features moved to the engine, each against swrast: a fragment program's KIL as the alpha test, a program's fog option in a mode
 * GL_FOG is not set to, smooth points with the alpha test, MIRROR_CLAMP_TO_EDGE (magnified and mipmapped); the engine run must draw with no
 * software primitive */
static GLubyte ft_img[2][W*H*4];
/* v10.358: ACCEL_3D_GUARD: polygons crossing the window's left and top edges (Mesa clips them there: vertices at the engine's -0.5) drawn
 * through the fast path with the switch on (the VxD's check and the core's), against swrast; and FASTPOINTS draws one quad a point */
static GLubyte gd_img[2][W*H*4];
/* v10.366: ACCEL_OPENGL_TEXRING and TEX16: Quake 2's dynamic lightmap, one GL_RGB texture written again (glTexSubImage2D) many times a frame,
 * each time drawn into its own part of the screen before the next write: every part must show its own write's colour (a copy written
 * while a queued or drawing packet reads it shows the later one). Against software; the ring's uploads counted; the model's in-flight
 * packets (the last call's) checked at each direct write (host_direct_bad) */
static uint8_t rc_img[2][W*H*4];
static int ring_check1(void);
/* v10.378: the texture ring checked with the VxD's ring off (a call retires the group before it) and on (only engine waits finish packets) */
/* v10.500: Quake 2's and Quake 3's scenes (src/vx_qscenes.h), three frames each: every new switch on (the default) against all off,
 * the same draw packets word for word (pk_hash), the pictures the same, and the engine's against software; and each part used */
static int qscene_check(void) {
 /* v10.501: runs 0 v10.500's parts and UBCOLOR on, 1 all off, 2 software, 3 as 0 with PREPACK, 4 as 3 with INDEXED: 0 and 1 the same draw
  * packets (the arrays form), every hardware picture the same, none refused by the VxD's emitter, each part used where it should be */
 extern int vxgl_gl_trilist,_vbo_vx855_fastvtx,_tnl_vx855_cvacache,vxgl_gl_storesnap,vxgl_gl_ubcolor;extern uint32_t vx_d3d_prepack,vx_d3d_indexed;
 extern uint32_t __stdcall VXGL_Stats505(uint32_t*,uint32_t);extern int vxgl_ssepass_sse2,vxgl_ssepass_defer;int k8=vx_d3d_gl_gllist,k9=vx_d3d_gl_glwords,k10=vxgl_ssepass_sse2,k11=vxgl_ssepass_defer;
 extern uint32_t vx_d3d_gw_fixed;extern int vxgl_gl_fan,vxgl_ssepass_range;uint32_t k12=vx_d3d_gw_fixed;int k13=vxgl_gl_fan,k14=vxgl_ssepass_range; /* .505: run 5 their off too */ /* .503: run 5 as 3 with GLLIST, GLWORDS, SSE2, SSEDEFER off */
 extern int vxgl_ssepass_on;int k7=vxgl_ssepass_on; /* v10.502: the ICD's default pass (1 | 4: lit draws, lists and not; board, v10.501: the pass converted every byte colour at its entry) */
 int k0=vxgl_gl_trilist,k1=_vbo_vx855_fastvtx,k2=_tnl_vx855_cvacache,k3=vxgl_gl_storesnap,k4=vxgl_gl_ubcolor,sc,run,f,fails=0,i,d;uint32_t k5=vx_d3d_prepack,k6=vx_d3d_indexed,hs[6],st[6][20],pr[6];
 static GLubyte qi[6][W*H*4];
 for(sc=0;sc<2;sc++){
  for(run=-1;run<6;run++){vxgl_ctx *c;int on=run!=1&&run!=2,n3=run!=1&&run!=5; /* run -1: a frame not compared (the first engine clear after the other checks carries their unused vertex words) */
   vxgl_gl_trilist=_vbo_vx855_fastvtx=_tnl_vx855_cvacache=vxgl_gl_storesnap=vxgl_gl_ubcolor=on;vx_d3d_prepack=run>=3;vx_d3d_indexed=run==4;vxgl_ssepass_on=5;
   vx_d3d_gl_gllist=vx_d3d_gl_glwords=vx_d3d_gw_fixed=(uint32_t)n3;vxgl_ssepass_sse2=vxgl_ssepass_defer=vxgl_gl_fan=vxgl_ssepass_range=n3;
   memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;vxgl_hw_enabled=run!=2;
   c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1)){printf("check FAIL: qscene context\n");return 1;}cur=c;
   qs_ready=0;qs_init((qs_lock_fn)host_gp("glLockArraysEXT"),(qs_unlock_fn)host_gp("glUnlockArraysEXT"));VXGL_Stats505(0,1);pk_hash=2166136261U;pre_refused=0;
   for(f=0;f<3;f++){glViewport(0,0,W,H);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);if(sc)qs_q3(f);else qs_q2(f);
    if(f==2&&run>=0){glFinish();glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,qi[run]);}
    {extern int vxgl_swap(vxgl_ctx*,HDC);vxgl_swap(c,(HDC)1);}}
   glFinish();if(run>=0){hs[run]=pk_hash;pr[run]=pre_refused;VXGL_Stats505(st[run],0);}
   vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);}
  if(hs[0]!=hs[1]){printf("check FAIL: qscene %s: the draw packets differ with the v10.500-501 switches on (%08X) and off (%08X)\n",sc?"q3":"q2",hs[0],hs[1]);fails++;}
  if(hs[3]!=hs[5]){printf("check FAIL: qscene %s: the draw packets differ with the .503 switches on (%08X) and off (%08X)\n",sc?"q3":"q2",hs[3],hs[5]);fails++;}
  if((sc&&!st[0][14])||!st[3][13]||st[5][13]||st[5][14]||!st[3][18]||st[5][18]||(!sc&&!st[0][19])||st[5][19]){printf("check FAIL: qscene %s: .503-505 counts: run triangles %u, recipe vertices %u, fixed %u, one-call polygons %u; off %u %u %u %u\n",sc?"q3":"q2",st[0][14],st[3][13],st[3][18],st[0][19],st[5][13],st[5][14],st[5][18],st[5][19]);fails++;}
  for(run=1;run<6;run++)if(run!=2&&memcmp(qi[0],qi[run],sizeof(qi[0]))){int q,nd=0,fx=-1,fy=0,lx=0,ly=0;for(q=0;q<W*H;q++)if(memcmp(qi[0]+4*q,qi[run]+4*q,4)){if(fx<0){fx=q%W;fy=q/W;}lx=q%W;ly=q/W;nd++;}printf("check FAIL: qscene %s: run %d's picture differs from run 0's (%d pixels, first %d,%d: %02X%02X%02X against %02X%02X%02X; last %d,%d)\n",sc?"q3":"q2",run,nd,fx,fy,qi[0][4*(fy*W+fx)],qi[0][4*(fy*W+fx)+1],qi[0][4*(fy*W+fx)+2],qi[run][4*(fy*W+fx)],qi[run][4*(fy*W+fx)+1],qi[run][4*(fy*W+fx)+2],lx,ly);fails++;}
  for(run=0;run<6;run++)if(pr[run]){printf("check FAIL: qscene %s: run %d: %u packets refused by the VxD's emitter\n",sc?"q3":"q2",run,pr[run]);fails++;}
  for(i=0,d=0;i<W*H;i++){int ch,o=0;for(ch=0;ch<3;ch++){int a=qi[0][i*4+ch],b=qi[2][i*4+ch];if(a-b>16||b-a>16)o=1;}d+=o;}
  if(d>W*H/100){printf("check FAIL: qscene %s: %d pixels off software\n",sc?"q3":"q2",d);fails++;}
  if(!st[0][5]||(sc&&(!st[0][0]||!st[0][3]||!st[0][9]||st[0][10]))||st[1][0]||st[1][3]||st[1][5]||st[1][9]||st[0][11]||!st[3][11]||st[3][12]||(sc&&!st[4][12])){
   printf("check FAIL: qscene %s: counts on %u %u %u %u, off %u %u %u %u, prepacked %u %u, indexed %u %u\n",sc?"q3":"q2",st[0][0],st[0][3],st[0][5],st[0][9],st[1][0],st[1][3],st[1][5],st[1][9],st[0][11],st[3][11],st[3][12],st[4][12]);fails++;}
  printf("qscene check %s: packets %08X both ways, %d pixels off software; trilist runs %u (%u triangles), transforms kept %u made %u, grid words %u draws (%u vertices), byte colours %u draws (%u converted), prepacked packets %u, indexed triangles %u; %s\n",
   sc?"q3":"q2",hs[0],d,st[0][0],st[0][1],st[0][3],st[0][4],st[0][5],st[0][6],st[0][9],st[0][10],st[3][11],st[4][12],fails?"FAIL":"ok");
 }
 vxgl_gl_trilist=k0;_vbo_vx855_fastvtx=k1;_tnl_vx855_cvacache=k2;vxgl_gl_storesnap=k3;vxgl_gl_ubcolor=k4;vx_d3d_prepack=k5;vx_d3d_indexed=k6;vxgl_ssepass_on=k7;
 vx_d3d_gl_gllist=(uint32_t)k8;vx_d3d_gl_glwords=(uint32_t)k9;vxgl_ssepass_sse2=k10;vxgl_ssepass_defer=k11;vx_d3d_gw_fixed=k12;vxgl_gl_fan=k13;vxgl_ssepass_range=k14;
 return fails;
}
/* .503 (ACCEL_OPENGL_SSEDEFER, ACCEL_OPENGL_SSE2): lit, smooth, culled geometry drawn with the pass's lighting deferred and not, the colour
 * words by SSE2 and not: strips (a quarter turned away) immediate and from a display list, GL_TRIANGLES from glDrawElements, fans; back faces
 * culled with counter-clockwise fronts and front faces with clockwise, a window and a framebuffer-like flip both ways by the frame; three
 * frames each: the same draw packets word for word, the same picture, groups left unlit in the deferred runs */
static void defer_draw(int f,int cw) {
 static const GLfloat pos[4]={160.0f,120.0f,60.0f,1.0f},sp[4]={0.5f,0.5f,0.5f,1},fc[4]={0.2f,0.25f,0.35f,1};static GLfloat va[64*2*3],na[64*2*3];static GLuint el[63*6];static GLuint list;int s,k,n=0;
 glViewport(0,0,W,H);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glShadeModel(GL_SMOOTH);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-1,1,-0.75,0.75,1,10);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 glTranslatef(0,0,-2);glScalef(1.0f/80.0f,1.0f/80.0f,1);glTranslatef(-160,-120,0);glTranslatef((float)(f%5),(float)(f%3),0);
 glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glLightfv(GL_LIGHT0,GL_POSITION,pos);glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.002f);glLightfv(GL_LIGHT0,GL_SPECULAR,sp);
 glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,sp);glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,20);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);glEnable(GL_COLOR_MATERIAL);
 glEnable(GL_FOG);glFogi(GL_FOG_MODE,GL_EXP);glFogf(GL_FOG_DENSITY,0.35f);glFogfv(GL_FOG_COLOR,fc);
 glEnable(GL_CULL_FACE);glCullFace(cw?GL_FRONT:GL_BACK);glFrontFace(cw?GL_CW:GL_CCW);
 for(s=0;s<16;s++){float y0=10.0f+s*4.6f,y1=y0+4.0f;glColor3f(0.3f+0.7f*(float)((s*37)%11)/10.0f,0.5f,0.6f);glBegin(GL_TRIANGLE_STRIP);
  for(k=0;k<64;k++){float x=8.0f+k*(304.0f/63.0f),ya=y0,yb=y1;if((((k/16)+s)&3)==0){ya=y1;yb=y0;}
   glNormal3f(0.6f*(float)sin(k*0.35+s*0.5),0.6f*(float)cos(k*0.2+f*0.1),1.0f);glVertex2f(x,ya);glNormal3f(0.6f*(float)sin(k*0.35+s*0.5+0.4),0.6f*(float)cos(k*0.2+f*0.1+0.3),1.0f);glVertex2f(x,yb);}
  glEnd();}
 if(!list){list=glGenLists(1);glNewList(list,GL_COMPILE); /* strips from a list: rows 16-31 */
  for(s=16;s<32;s++){float y0=10.0f+s*4.6f,y1=y0+4.0f;glColor3f(0.4f,0.3f+0.7f*(float)((s*29)%7)/6.0f,0.5f);glBegin(GL_TRIANGLE_STRIP);
   for(k=0;k<64;k++){float x=8.0f+k*(304.0f/63.0f),ya=y0,yb=y1;if((((k/8)+s)&3)==1){ya=y1;yb=y0;}glNormal3f(0.5f*(float)sin(k*0.3),0.4f,1.0f);glVertex2f(x,ya);glNormal3f(0.5f*(float)sin(k*0.3+0.2),0.4f,1.0f);glVertex2f(x,yb);}
   glEnd();}
  glEndList();}
 glCallList(list);
 for(k=0;k<64;k++){float x=8.0f+k*(304.0f/63.0f),y0=10.0f+32*4.6f,y1=y0+8.0f;va[k*6]=x;va[k*6+1]=y0;va[k*6+2]=0;va[k*6+3]=x;va[k*6+4]=y1;va[k*6+5]=0; /* GL_TRIANGLES by elements: every third quad turned away */
  na[k*6]=0.3f*(float)sin(k*0.4+f*0.2);na[k*6+1]=0.2f;na[k*6+2]=1;na[k*6+3]=0.3f*(float)sin(k*0.4+0.3);na[k*6+4]=0.25f;na[k*6+5]=1;}
 for(k=0;k<63;k++){GLuint a=2*k,b=2*k+2,c=2*k+1,d=2*k+3;if(k%3==1){el[n++]=a;el[n++]=c;el[n++]=b;el[n++]=b;el[n++]=c;el[n++]=d;}else{el[n++]=a;el[n++]=b;el[n++]=c;el[n++]=b;el[n++]=d;el[n++]=c;}}
 glColor3f(0.8f,0.6f,0.3f);glVertexPointer(3,GL_FLOAT,0,va);glNormalPointer(GL_FLOAT,0,na);glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_NORMAL_ARRAY);
 glDrawElements(GL_TRIANGLES,n,GL_UNSIGNED_INT,el);glDisableClientState(GL_VERTEX_ARRAY);glDisableClientState(GL_NORMAL_ARRAY);
 for(s=0;s<6;s++){int i;float cx=30.0f+s*50.0f,cy=215.0f;glColor3f(0.2f,0.7f,0.4f);glBegin(GL_TRIANGLE_FAN);glNormal3f(0,0,1);glVertex2f(cx,cy); /* fans: odd ones wound the other way */
  for(i=0;i<=12;i++){double a=(s&1?-1:1)*6.2831853*i/12;glNormal3f(0.3f*(float)cos(a),0.3f*(float)sin(a),1);glVertex2f(cx+18.0f*(float)cos(a),cy+12.0f*(float)sin(a));}glEnd();}
 glDisable(GL_CULL_FACE);glFrontFace(GL_CCW);glDisable(GL_FOG);glDisable(GL_LIGHTING);glDisable(GL_COLOR_MATERIAL);
}
static int defer_check(void) {
 extern int vxgl_ssepass_on,vxgl_ssepass_defer,vxgl_ssepass_sse2;extern uint32_t vxgl_ssepass_defer_stat[3];
 int k0=vxgl_ssepass_on,k1=vxgl_ssepass_defer,k2=vxgl_ssepass_sse2,cw,run,f,fails=0;uint32_t hs[3],lit[3],unlit[3];static GLubyte di[3][W*H*4];
 for(cw=0;cw<2;cw++){
  for(run=-1;run<3;run++){vxgl_ctx *c; /* 0 deferred, SSE2; 1 neither; 2 deferred, no SSE2; -1 not compared (the first engine clear after the other checks, as qscene_check's) */
   vxgl_ssepass_on=5;vxgl_ssepass_defer=run!=1;vxgl_ssepass_sse2=run<=0;{extern int vxgl_ssepass_range;vxgl_ssepass_range=run!=1;} /* .505: SSERANGE off in run 1 */
   memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;vxgl_hw_enabled=1;
   c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1)){printf("check FAIL: defer context\n");return 1;}cur=c;
   memset(vxgl_ssepass_defer_stat,0,sizeof(uint32_t)*3);pk_hash=2166136261U;
   for(f=0;f<3;f++){defer_draw(f,cw);if(f==2&&run>=0){glFinish();glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,di[run]);}
    {extern int vxgl_swap(vxgl_ctx*,HDC);vxgl_swap(c,(HDC)1);}}
   glFinish();if(run<0){vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);continue;}hs[run]=pk_hash;lit[run]=vxgl_ssepass_defer_stat[1];unlit[run]=vxgl_ssepass_defer_stat[2];
   vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);}
  if(hs[0]!=hs[1]||hs[2]!=hs[1]){printf("check FAIL: defer %s: the draw packets differ (deferred %08X, not %08X, without SSE2 %08X)\n",cw?"cw":"ccw",hs[0],hs[1],hs[2]);fails++;}
  if(memcmp(di[0],di[1],sizeof(di[0]))||memcmp(di[2],di[1],sizeof(di[0]))){printf("check FAIL: defer %s: the pictures differ\n",cw?"cw":"ccw");fails++;}
  if(!unlit[0]||!lit[0]||unlit[1]||lit[1]){printf("check FAIL: defer %s: groups lit %u unlit %u (off: %u %u)\n",cw?"cw":"ccw",lit[0],unlit[0],lit[1],unlit[1]);fails++;}
  printf("defer check %s: packets %08X all three ways, groups lit %u, left unlit %u; %s\n",cw?"cw":"ccw",hs[1],lit[0],unlit[0],fails?"FAIL":"ok");}
 vxgl_ssepass_on=k0;vxgl_ssepass_defer=k1;vxgl_ssepass_sse2=k2;{extern int vxgl_ssepass_range;vxgl_ssepass_range=1;}
 return fails;
}
static int ring_check(void) {int f=0;vxgl_seq_ring=0;f+=ring_check1();vxgl_seq_ring=1;f+=ring_check1();vxgl_seq_ring=0;return f;}
static int ring_check1(void) {
 extern int vxgl_gl_texring,vxgl_gl_tex16;int run,fails=0,i,f,keep=vxgl_gl_texring;unsigned long st[4],bad0=host_direct_bad,dw0=host_direct_writes;
 for(run=0;run<2;run++){vxgl_ctx *c;GLuint tx;static GLubyte px[16*16*3];
  memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
  vxgl_hw_enabled=run==0;memset(vxgl_ring_stat,0,sizeof(vxgl_ring_stat));
  c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1)){printf("ring check: no context\n");return 1;}
  cur=c;glViewport(0,0,W,H);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,0,H,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  glGenTextures(1,&tx);glBindTexture(GL_TEXTURE_2D,tx);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  memset(px,0,sizeof(px));glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,16,16,0,GL_RGB,GL_UNSIGNED_BYTE,px);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
  for(f=0;f<3;f++){
   glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
   for(i=0;i<24;i++){int k;float x=(float)((i%6)*(W/6)),y=(float)((i/6)*(H/4));
    for(k=0;k<16*16;k++){px[k*3]=(GLubyte)(40*i+f*7);px[k*3+1]=(GLubyte)(255-9*i);px[k*3+2]=(GLubyte)(f*80+i);}
    glTexSubImage2D(GL_TEXTURE_2D,0,0,0,16,16,GL_RGB,GL_UNSIGNED_BYTE,px);
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(x+2,y+2);glTexCoord2f(1,0);glVertex2f(x+W/6-2,y+2);glTexCoord2f(1,1);glVertex2f(x+W/6-2,y+H/4-2);glTexCoord2f(0,1);glVertex2f(x+2,y+H/4-2);glEnd();}
   if(f<2)vxgl_swap(c,(HDC)1);}
  glFinish();if(!run)memcpy(st,vxgl_ring_stat,sizeof(st));
  glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,rc_img[run]);
  glDeleteTextures(1,&tx);vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);}
 vxgl_hw_enabled=1;vxgl_gl_texring=keep;
 {int d=0,ch,tol=host_cbits==16?9:vxgl_gl_tex16?5:1; /* a 16-bit target rounds again (one 5-bit step 8) */for(i=0;i<W*H;i++)for(ch=0;ch<3;ch++){int a=rc_img[0][i*4+ch],b=rc_img[1][i*4+ch];if(a-b>tol||b-a>tol){d++;break;}}
  int want=vxgl_gl_texring>0&&!vxgl_seq_ring;unsigned long used=st[0]+st[1]+st[2]; /* v10.378: with the VxD's ring, copies come free only at engine waits */
  printf("ring check (TEXRING %d, TEX16 %d, VxD ring %d): 72 uploads, %lu into a free copy, %lu after sending the queue, %lu into a new copy, %lu waited; %lu direct writes, %lu while read; %d pixels differ from software%s\n",
   vxgl_gl_texring,vxgl_gl_tex16,vxgl_seq_ring,st[0],st[1],st[2],st[3],host_direct_writes-dw0,host_direct_bad-bad0,d,(d||host_direct_bad!=bad0||(want&&used<60))?"  FAIL":"");
  if(d||host_direct_bad!=bad0||(want&&used<60))fails++;}
 return fails;
}
static int guard_check(void) {
 extern uint32_t vx3dn_guard,vx_d3d_feat;int run,fails=0,i;unsigned long tris[2]={0,0},pk[2]={0,0};uint32_t f0=vx_d3d_feat;uint32_t g0=vx3dn_guard; /* v10.361: the guard is on by default: both put back as they were */
 for(run=0;run<2;run++){vxgl_ctx *c;
  memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
  vxgl_hw_enabled=run==0;vx3dn_guard=1;vx_d3d_feat=f0|VX_F_GUARD;
  c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1)){printf("guard check: no context\n");return 1;}
  cur=c;packets=draw_packets=clear_packets=0;
  glViewport(0,0,W,H);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,0,H,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  glClearColor(0.1f,0.2f,0.3f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glBegin(GL_TRIANGLES);
  for(i=0;i<12;i++){float y=(float)(i*20);glColor3f(1,0.1f*i,0);glVertex2f(-30,y);glColor3f(0,1,0.05f*i);glVertex2f(60,y+5);glColor3f(0.2f,0.2f,1);glVertex2f(-10,y+18);}
  for(i=0;i<12;i++){float x=(float)(i*20);glColor3f(1,1,0.08f*i);glVertex2f(x,H+30);glColor3f(0,0.5f,1);glVertex2f(x+18,H-60);glColor3f(1,0,1);glVertex2f(x+6,H+10);}
  glEnd();
  glBegin(GL_QUADS);glColor3f(0.8f,0.8f,0.8f);glVertex2f(-20,-20);glVertex2f(40,-20);glVertex2f(40,40);glVertex2f(-20,40);glEnd();
  glFinish();tris[run]=c->stats[0];pk[run]=c->stats[3];
  glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,gd_img[run]);
  vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);}
 vx3dn_guard=g0;vx_d3d_feat=f0;vxgl_hw_enabled=1;
 {int d=0,ch;for(i=0;i<W*H;i++)for(ch=0;ch<3;ch++){int a=gd_img[0][i*4+ch],b=gd_img[1][i*4+ch];if(a-b>16||b-a>16){d++;break;}}
  printf("guard check: edge polygons with ACCEL_3D_GUARD, %lu triangles by the engine, %lu software primitives; %d pixels differ from software%s\n",tris[0],pk[0],d,(d>W*H/200||pk[0])?"  FAIL":"");
  if(d>W*H/200||pk[0])fails++;}
 return fails;
}
/* v10.360: polygons through the near plane and the window's edges under a perspective camera (Quake's: 90 degrees, near 4, far 4096):
 * a room of GL_POLYGON faces and pillars seen from 48 places, some inside a near-plane's distance of a wall, the engine against swrast.
 * A view with many pixels off is drawn again one polygon at a time to name the polygons that differ. */
static int cl_only,cl_n;
#define CL_ON() (cl_only<0||cl_only==cl_n)
static void clip_scene(float ex,float ey,float ez) {
 int f,gx,gy,i;cl_n=0;
 for(f=0;f<6;f++)for(gy=0;gy<4;gy++)for(gx=0;gx<4;gx++,cl_n++){float a0=-256.0f+128.0f*gx,a1=a0+128.0f,b0=-64.0f+64.0f*gy,b1=b0+64.0f,q[4][3];int k;
  float c0=-256.0f+128.0f*gy,c1=c0+128.0f;if(!CL_ON())continue;
  for(k=0;k<4;k++){float a=(k==1||k==2)?a1:a0,b=(k>=2)?b1:b0,cc=(k>=2)?c1:c0;
   switch(f){case 0:q[k][0]=-256;q[k][1]=a;q[k][2]=b;break;case 1:q[k][0]=256;q[k][1]=a;q[k][2]=b;break;
    case 2:q[k][0]=a;q[k][1]=-256;q[k][2]=b;break;case 3:q[k][0]=a;q[k][1]=256;q[k][2]=b;break;
    case 4:q[k][0]=a;q[k][1]=cc;q[k][2]=-64;break;default:q[k][0]=a;q[k][1]=cc;q[k][2]=192;break;}}
  glBegin(GL_POLYGON);for(k=0;k<4;k++){glColor3f(0.2f+0.13f*f,0.25f*gx+0.1f*(k&1),0.25f*gy+0.1f*(k>>1));glVertex3fv(q[k]);}glEnd();}
 for(i=0;i<5;i++){float px=-200.0f+100.0f*i,py=(i&1)?60.0f:-60.0f,r=24.0f;int s;
  for(s=0;s<4;s++,cl_n++){float x0,y0,x1,y1;if(!CL_ON())continue;
   if(s==0){x0=px-r;y0=py-r;x1=px-r;y1=py+r;}else if(s==1){x0=px-r;y0=py+r;x1=px+r;y1=py+r;}else if(s==2){x0=px+r;y0=py+r;x1=px+r;y1=py-r;}else{x0=px+r;y0=py-r;x1=px-r;y1=py-r;}
   glBegin(GL_POLYGON);glColor3f(0.9f,0.3f*s,0.2f*i);glVertex3f(x0,y0,-64);glVertex3f(x1,y1,-64);glColor3f(0.3f,0.9f,0.1f*s);glVertex3f(x1,y1,192);glVertex3f(x0,y0,192);glEnd();}}
 for(i=0;i<8;i++,cl_n++){float an=(float)i*0.785398f;if(!CL_ON())continue;
  glBegin(GL_TRIANGLES);glColor3f(1,1,0);glVertex3f(ex+300.0f*cosf(an),ey+300.0f*sinf(an),ez-20);
  glColor3f(0,1,1);glVertex3f(ex-5.0f*cosf(an),ey-5.0f*sinf(an),ez-20);glColor3f(1,0,1);glVertex3f(ex+300.0f*cosf(an+0.5f),ey+300.0f*sinf(an+0.5f),ez-20);glEnd();}
}
static int clip_view(float ex,float ey,float ez,float yaw,float pitch,unsigned long *swp) {
 int run,i,ch,d=0;
 for(run=0;run<2;run++){vxgl_ctx *c;
  memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;vxgl_hw_enabled=run==0;
  c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1)){printf("clip check: no context\n");return -1;}
  cur=c;glViewport(0,0,W,H);glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-4,4,-4,4,4,4096);
  glMatrixMode(GL_MODELVIEW);glLoadIdentity();glRotatef(-90,1,0,0);glRotatef(90,0,0,1); /* Quake's axes: x forward, z up */
  glRotatef(-pitch,0,1,0);glRotatef(-yaw,0,0,1);glTranslatef(-ex,-ey,-ez);
  glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glShadeModel(GL_SMOOTH);
  clip_scene(ex,ey,ez);
  glFinish();if(!run&&swp)*swp+=c->stats[3];
  glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,gd_img[run]);
  vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);}
 vxgl_hw_enabled=1;
 for(i=0;i<W*H;i++)for(ch=0;ch<3;ch++){int a=gd_img[0][i*4+ch],b=gd_img[1][i*4+ch];if(a-b>16||b-a>16){d++;break;}}
 return d;
}
static int clip_check(void) {
 int v,fails=0,worst=0,worstv=-1,i,total=0;unsigned long sw_prims=0;unsigned rs=359;
 for(v=0;v<48;v++){float ex,ey,ez,yaw,pitch;int d;
  rs=rs*1103515245u+12345u;ex=-240.0f+480.0f*(float)((rs>>8)&0xFFFF)/65535.0f;
  rs=rs*1103515245u+12345u;ey=-240.0f+480.0f*(float)((rs>>8)&0xFFFF)/65535.0f;
  rs=rs*1103515245u+12345u;ez=-60.0f+240.0f*(float)((rs>>8)&0xFFFF)/65535.0f;
  if(v%3==0){ex=v&8?253.0f:-253.0f;} /* 3 units from a wall: inside the near distance */
  if(v%5==0){ez=v&16?189.0f:-61.0f;}
  rs=rs*1103515245u+12345u;yaw=360.0f*(float)((rs>>8)&0xFFFF)/65535.0f;
  rs=rs*1103515245u+12345u;pitch=-80.0f+160.0f*(float)((rs>>8)&0xFFFF)/65535.0f;
  if(getenv("VXGL_CLIPVIEW")&&atoi(getenv("VXGL_CLIPVIEW"))!=v)continue;
  cl_only=getenv("VXGL_CLIPPOLY")?atoi(getenv("VXGL_CLIPPOLY")):-1;d=clip_view(ex,ey,ez,yaw,pitch,&sw_prims);if(d<0)return 1;
  if(getenv("VXGL_CLIPPOLY")){char nm[64];FILE *o;int j,y;printf("view %d polygon %d: %d pixels differ\n",v,cl_only,d);for(j=0;j<2;j++){sprintf(nm,"build/hostgl/cp_%s.ppm",j?"sw":"hw");o=fopen(nm,"wb");
    if(o){fprintf(o,"P6 %d %d 255\n",W,H);for(y=H-1;y>=0;y--)for(i=0;i<W;i++)fwrite(gd_img[j]+(y*W+i)*4,1,3,o);fclose(o);}}continue;}
  total+=d;if(d>worst){worst=d;worstv=v;}
  if(d>W*H/100){int n=cl_n,k;fails++;printf("clip check FAIL: view %d (eye %.1f %.1f %.1f, yaw %.1f, pitch %.1f): %d pixels differ from software\n",v,ex,ey,ez,yaw,pitch,d);
   if(getenv("VXGL_DUMP")){char nm[64];FILE *o;int j,y;for(j=0;j<2;j++){sprintf(nm,"build/hostgl/clip_%02d_%s.ppm",v,j?"sw":"hw");o=fopen(nm,"wb");
    if(o){fprintf(o,"P6 %d %d 255\n",W,H);for(y=H-1;y>=0;y--)for(i=0;i<W;i++)fwrite(gd_img[j]+(y*W+i)*4,1,3,o);fclose(o);}}}
   for(k=0;k<n;k++){int dk;cl_only=k;dk=clip_view(ex,ey,ez,yaw,pitch,0);if(dk>16)printf("  polygon %d alone: %d pixels differ\n",k,dk);}
   cl_only=-1;}}
 printf("clip check: 48 perspective views through the near plane and the edges: %d pixels off in all (worst %d, view %d), %lu software primitives%s\n",total,worst,worstv,sw_prims,fails?"  FAIL":"");
 return fails;
}
static int feat_check(void) {
 int fails=0,k,run;static const char *nm[7]={"KIL as the alpha test","fog option EXP2 with GL_FOG off (LINEAR set)","smooth points with the alpha test","MIRROR_CLAMP_TO_EDGE magnified","MIRROR_CLAMP_TO_EDGE mipmapped",
  "flat fans and polygons (VXFLAT's scene)","flat triangles (VXFLAT's scene)"};
 for(k=0;k<7;k++){unsigned long sw0=0,sw1=0;
  if(k<2&&!scene_gen_prog)continue;
  for(run=0;run<2;run++){vxgl_ctx *c;GLuint tx=0,pr=0;int i,j;static GLubyte t16[16*16*4],t8[4][8*8*4];
   memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
   vxgl_hw_enabled=run==0;c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1)){printf("feature check: no context\n");return 1;}
   cur=c;glViewport(0,0,W,H);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,0,H,-1,200);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
   glClearColor(0.6f,0.1f,0.1f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
   for(i=0;i<256;i++){t16[i*4]=(GLubyte)(40+(i&15)*12);t16[i*4+1]=(GLubyte)(200-(i>>4)*10);t16[i*4+2]=(GLubyte)((i*37)&255);t16[i*4+3]=(GLubyte)((i&15)*17);}
   sw0=c->stats[3];
   if(k==0){static const char fp[]="!!ARBfp1.0\nTEMP t,q;\nTEX t, fragment.texcoord[0], texture[0], 2D;\nSUB q, t.wwww, 0.5;\nKIL q;\nMOV result.color, t;\nEND\n";
    glGenTextures(1,&tx);glBindTexture(GL_TEXTURE_2D,tx);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,t16);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glEnable(GL_TEXTURE_2D);
    scene_gen_prog(1,&pr);scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,pr);scene_prog_str(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(fp),fp);glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glEnable(GL_DEPTH_TEST);glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex3f(8,8,-5);glTexCoord2f(1,0);glVertex3f(W-8,8,-5);glTexCoord2f(1,1);glVertex3f(W-8,H-8,-5);glTexCoord2f(0,1);glVertex3f(8,H-8,-5);glEnd();
    /* the depth the kept fragments wrote: a green quad behind is hidden there only */
    glDisable(GL_FRAGMENT_PROGRAM_ARB);glDisable(GL_TEXTURE_2D);glColor3f(0,1,0);
    glBegin(GL_QUADS);glVertex3f(0,0,-10);glVertex3f(W,0,-10);glVertex3f(W,H,-10);glVertex3f(0,H,-10);glEnd();}
   if(k==1){static const char fp[]="!!ARBfp1.0\nOPTION ARB_fog_exp2;\nMOV result.color, fragment.color;\nEND\n";
    GLfloat fc[4]={0.1f,0.2f,0.9f,1};
    scene_gen_prog(1,&pr);scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,pr);scene_prog_str(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(fp),fp);glEnable(GL_FRAGMENT_PROGRAM_ARB);
    glDisable(GL_FOG);glFogi(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_DENSITY,0.012f);glFogf(GL_FOG_START,0);glFogf(GL_FOG_END,300);glFogfv(GL_FOG_COLOR,fc);
    for(j=0;j<4;j++){GLfloat z=-(10.0f+j*40.0f);glColor3f(1,0.8f,0.2f);
     glBegin(GL_TRIANGLES);glVertex3f(10+j*30.0f,10,z);glVertex3f(40+j*30.0f,10,z);glVertex3f(25+j*30.0f,60,z);glEnd();}}
   if(k==2){glEnable(GL_POINT_SMOOTH);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GEQUAL,0.5f);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    for(j=0;j<12;j++){glPointSize(j&1?9.0f:13.0f);glBegin(GL_POINTS);glColor4f(0.2f,0.9f,0.3f,j%3?0.8f:0.3f);glVertex2f(12.0f+j*10.5f,20.0f+(j%4)*25.3f);glEnd();}}
   if(k>=3){int l;
    for(l=0;l<4;l++){int n=8>>l;for(i=0;i<n*n;i++){t8[l][i*4]=(GLubyte)(l*60+(i%n)*25);t8[l][i*4+1]=(GLubyte)((i/n)*30);t8[l][i*4+2]=(GLubyte)(200-l*40);t8[l][i*4+3]=255;}}
    glGenTextures(1,&tx);glBindTexture(GL_TEXTURE_2D,tx);for(l=0;l<4;l++)glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA,8>>l,8>>l,0,GL_RGBA,GL_UNSIGNED_BYTE,t8[l]);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_MIRROR_CLAMP_TO_EDGE_EXT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_MIRROR_CLAMP_TO_EDGE_EXT);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,k==3?GL_LINEAR:GL_LINEAR_MIPMAP_NEAREST);
    glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
    if(k==3){glBegin(GL_QUADS);glTexCoord2f(-2,-2);glVertex2f(4,4);glTexCoord2f(2,-2);glVertex2f(W-4,4);glTexCoord2f(2,2);glVertex2f(W-4,H-4);glTexCoord2f(-2,2);glVertex2f(4,H-4);glEnd();}
    else for(j=0;j<4;j++){GLfloat s0=(GLfloat)(4+j*24),sz=(GLfloat)(6+j*5);glBegin(GL_QUADS);glTexCoord2f(-2,-2);glVertex2f(s0,10);glTexCoord2f(2,-2);glVertex2f(s0+sz,10);glTexCoord2f(2,2);glVertex2f(s0+sz,10+sz);glTexCoord2f(-2,2);glVertex2f(s0,10+sz);glEnd();}}
   if(k>=5){int s,e,q; /* v10.484: VXFLAT v1.3's fans and polygons scene (290 pixels off software on the board, the template too) and its triangles */
    glShadeModel(GL_FLAT);
    for(s=0;s<40;s++){float y0=10.0f+s*5.6f;
     #define VC(a) glColor3f((float)((s*7+(a)*3)%11)/10.0f,(float)((s*5+(a)*7)%7)/6.0f,(float)((s*3+(a)*5)%5)/4.0f)
     if(k==6){glBegin(GL_TRIANGLES);for(q=0;q<50;q++){float x0=4.0f+q*4.9f,x1=x0+4.9f;VC(q);glVertex2f(x0,y0);VC(q+1);glVertex2f(x1,y0);VC(q+40);glVertex2f(x0,y0+4);VC(q+21);glVertex2f(x1,y0);VC(q+13);glVertex2f(x1,y0+4);VC(q+5);glVertex2f(x0,y0+4);}glEnd();continue;}
     for(e=0;e<7;e++){float cx=20.0f+e*34.0f,cy=y0+2.0f;int i;
      if(s&1){glBegin(GL_TRIANGLE_FAN);VC(e);glVertex2f(cx,cy);for(i=0;i<15;i++){double a=0.2+6.2831853*i/16;VC(e*16+i+1);glVertex2f(cx+(float)(cos(a)*16.5),cy+(float)(sin(a)*1.95));}glEnd();}
      else{glBegin(GL_POLYGON);for(i=0;i<16;i++){double a=0.1+6.2831853*i/16;VC(e*16+i);glVertex2f(cx+(float)(cos(a)*16.5),cy+(float)(sin(a)*1.95));}glEnd();}}
     #undef VC
    }
    glShadeModel(GL_SMOOTH);}
   glFinish();sw1=c->stats[3];
   if(run==0&&sw1!=sw0){printf("check FAIL: feature %s: %lu draws in software on the engine run\n",nm[k],sw1-sw0);fails++;}
   glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,ft_img[run]);
   if(glGetError()){printf("check FAIL: feature %s: GL error\n",nm[k]);fails++;}
   if(scene_gen_prog)scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,0);
   vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);}
  {int i,d=0,ch;for(i=0;i<W*H;i++)for(ch=0;ch<3;ch++){int a=ft_img[0][i*4+ch],b=ft_img[1][i*4+ch];if(a-b>16||b-a>16){d++;break;}}
   if(k>=5){int x,y,edge=0;for(y=0;y<H;y++)for(x=0;x<W;x++){int i=y*W+x,dd=0,nb=0,m;static const int n[4][2]={{1,0},{-1,0},{0,1},{0,-1}};
     for(ch=0;ch<3;ch++){int a=ft_img[0][i*4+ch],b=ft_img[1][i*4+ch];if(a-b>16||b-a>16)dd=1;}
     if(!dd)continue;for(m=0;m<4;m++){int xx=x+n[m][0],yy=y+n[m][1];if(xx>=0&&yy>=0&&xx<W&&yy<H&&memcmp(&ft_img[1][(yy*W+xx)*4],&ft_img[1][i*4],3))nb=1;}if(nb)edge++;}
    /* a pixel off only where software's frame has another colour next to it: the two rasterizers give a pixel on a shared edge to different triangles */
    printf("feature check: %s: %d of the pixels that differ are on an edge between two colours in software's frame%s\n",nm[k],edge,edge!=d||(k==6&&d)?"  FAIL":"");if(edge!=d||(k==6&&d))fails++;}
   printf("feature check: %-44s %d pixels differ from software%s\n",nm[k],d,d>W*H/200?"  FAIL":"");if(d>W*H/200)fails++;}}
 return fails;
}
static GLubyte zc_img[3][W*H*4];
static int zclear_check(int mode) { /* v10.362: mode 1 the depth quad and the colour's 2D fill (the default), 2 colour in the quad too */
 extern int vxgl_gl_zclear3d;int run,fails=0,keep=vxgl_gl_zclear3d,i,j;unsigned long hw_tris[3]={0,0,0},hw_clears[3]={0,0,0};
 for(run=0;run<3;run++){ /* 0 zclear3d, 1 the 2D fill, 2 software */
  vxgl_ctx *c;GLuint tx;GLint vp[4],dfn,mm,at,asd,afn;GLfloat cc[4];GLenum e;static const GLubyte t4[4*4]={255,255,255,255,255,255,255,255,255,255,255,255,255,255,255,255};
  GLdouble pl[4]={1,0,0,-1000};int sten=host_zbits==24;
  memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
  vxgl_hw_enabled=run!=2;vxgl_gl_zclear3d=run==0?mode:0;c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1)){printf("zclear check: no context\n");return 1;}
  cur=c;packets=draw_packets=clear_packets=0;
  glViewport(0,0,W,H);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,0,H,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  glClearColor(1,0,0,1);glClearDepth(1);glClearStencil(0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|(sten?GL_STENCIL_BUFFER_BIT:0));
  glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glColor3f(0,1,0); /* green at z 0.5 over everything */
  glBegin(GL_QUADS);glVertex3f(0,0,0);glVertex3f(W,0,0);glVertex3f(W,H,0);glVertex3f(0,H,0);glEnd();
  /* hostile state */
  glGenTextures(1,&tx);glBindTexture(GL_TEXTURE_2D,tx);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,t4);glEnable(GL_TEXTURE_2D);
  if(scene_active_tex){scene_active_tex(GL_TEXTURE1_ARB);glBindTexture(GL_TEXTURE_2D,tx);glEnable(GL_TEXTURE_2D);}
  glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_NEVER,0);glDepthFunc(GL_NEVER);glEnable(GL_BLEND);glBlendFunc(GL_ZERO,GL_ZERO);
  glClipPlane(GL_CLIP_PLANE0,pl);glEnable(GL_CLIP_PLANE0);glEnable(GL_CULL_FACE);glCullFace(GL_FRONT_AND_BACK);glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
  glViewport(3,4,5,6);glDepthRange(0.2,0.3);glMatrixMode(GL_MODELVIEW);glPushMatrix();glTranslatef(5,5,5);glMatrixMode(GL_TEXTURE);glColor4f(0.25f,0.5f,0.75f,1);
  if(sten){glEnable(GL_STENCIL_TEST);glStencilFunc(GL_NEVER,7,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);glStencilMask(0x0F);}
  glShadeModel(GL_FLAT);glEnable(GL_COLOR_SUM_EXT); /* v10.348: the colour sum and the fragment program are not in Mesa's enable group */
  if(scene_gen_prog){static const char fp[]="!!ARBfp1.0\nMOV result.color, fragment.color;\nEND\n";GLuint pr;scene_gen_prog(1,&pr);scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,pr);
   scene_prog_str(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(fp),fp);glEnable(GL_FRAGMENT_PROGRAM_ARB);}
  glEnable(GL_SCISSOR_TEST);glScissor(W/4,H/4,W/2,H/2);glClearColor(0,0,1,1);glClearStencil(0x35);
  glClear(GL_DEPTH_BUFFER_BIT|(sten?GL_STENCIL_BUFFER_BIT:0)); /* depth (and stencil) only, in the scissor */
  if(c->stats[0])hw_tris[run]=c->stats[0];hw_clears[run]=clear_packets;
  e=glGetError();if(e){printf("check FAIL: zclear %d: glClear left GL error %04X\n",run,e);fails++;}
  glGetIntegerv(GL_VIEWPORT,vp);glGetIntegerv(GL_DEPTH_FUNC,&dfn);glGetIntegerv(GL_MATRIX_MODE,&mm);glGetIntegerv(GL_ACTIVE_TEXTURE_ARB,&at);glGetIntegerv(GL_ATTRIB_STACK_DEPTH,&asd);
  glGetIntegerv(GL_ALPHA_TEST_FUNC,&afn);glGetFloatv(GL_CURRENT_COLOR,cc);
  if(vp[0]!=3||vp[1]!=4||vp[2]!=5||vp[3]!=6||dfn!=GL_NEVER||mm!=GL_TEXTURE||(scene_active_tex&&at!=GL_TEXTURE1_ARB)||asd!=0||afn!=GL_NEVER||cc[0]!=0.25f||cc[2]!=0.75f||
     !glIsEnabled(GL_CLIP_PLANE0)||!glIsEnabled(GL_TEXTURE_2D)||!glIsEnabled(GL_BLEND)||!glIsEnabled(GL_COLOR_SUM_EXT)||(scene_gen_prog&&!glIsEnabled(GL_FRAGMENT_PROGRAM_ARB))||!glIsEnabled(GL_CULL_FACE)||
     !glIsEnabled(GL_ALPHA_TEST)||(sten&&!glIsEnabled(GL_STENCIL_TEST))||!glIsEnabled(GL_DEPTH_TEST)||!glIsEnabled(GL_SCISSOR_TEST)||({GLint sm,pm[2];GLboolean m4[4];glGetIntegerv(GL_SHADE_MODEL,&sm);glGetIntegerv(GL_POLYGON_MODE,pm);glGetBooleanv(GL_COLOR_WRITEMASK,m4);sm!=GL_FLAT||pm[0]!=GL_LINE||pm[1]!=GL_LINE||!m4[0]||!m4[3];})){
   printf("check FAIL: zclear %d: the app's state changed by glClear (viewport %d,%d,%d,%d depth func %X matrix mode %X active %X stack %d)\n",run,vp[0],vp[1],vp[2],vp[3],dfn,mm,at,asd);fails++;}
  /* back to plain state: blue at z 0.9 with LESS: drawn only where the depth was cleared (the scissor); stencil EQUAL 0x05 (the 0x35 clear through write mask 0x0F) */
  glMatrixMode(GL_MODELVIEW);glPopMatrix();glViewport(0,0,W,H);glDepthRange(0,1);glDisable(GL_SCISSOR_TEST);
  if(scene_active_tex){glDisable(GL_TEXTURE_2D);scene_active_tex(GL_TEXTURE0_ARB);}glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glDisable(GL_CLIP_PLANE0);glDisable(GL_CULL_FACE);
  glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glDepthFunc(GL_LESS);glColor3f(0,0,1);glShadeModel(GL_SMOOTH);glDisable(GL_COLOR_SUM_EXT);if(scene_gen_prog)glDisable(GL_FRAGMENT_PROGRAM_ARB);
  if(sten){glStencilFunc(GL_EQUAL,0x05,0xFF);glStencilMask(0xFF);}
  glBegin(GL_QUADS);glVertex3f(0,0,-0.8f);glVertex3f(W,0,-0.8f);glVertex3f(W,H,-0.8f);glVertex3f(0,H,-0.8f);glEnd(); /* ortho near -1 far 1: z -0.8 is window 0.9 */
  if(sten)glDisable(GL_STENCIL_TEST);
  /* colour and depth together, a clear colour through the engine's quad: a scissored yellow */
  glEnable(GL_SCISSOR_TEST);glScissor(0,0,W/8,H/8);glClearColor(1,1,0,1);glDepthFunc(GL_NEVER);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);glDepthFunc(GL_LESS);
  glFinish();glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,zc_img[run]);
  vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);
 }
 vxgl_gl_zclear3d=keep;
 { /* the picture: yellow lower-left corner, blue in the middle (the scissor), green elsewhere; all three runs alike */
  static const struct {int x,y;GLubyte r,g,b;} want[]={{W/16,H/16,255,255,0},{W/2,H/2,0,0,255},{W/4+1,H/4+1,0,0,255},{W/4-2,H/2,0,255,0},{W-2,H-2,0,255,0},{3*W/4+1,H/2,0,255,0}};
  for(run=0;run<3;run++)for(j=0;j<(int)(sizeof(want)/sizeof(want[0]));j++){const GLubyte *q=zc_img[run]+(want[j].y*W+want[j].x)*4;
   if(abs(q[0]-want[j].r)>8||abs(q[1]-want[j].g)>8||abs(q[2]-want[j].b)>8){printf("check FAIL: zclear %d (%s): %d,%d is %02X%02X%02X, want %02X%02X%02X\n",run,run==0?"engine quad":run==1?"2D fill":"software",want[j].x,want[j].y,q[0],q[1],q[2],want[j].r,want[j].g,want[j].b);fails++;}}
  for(i=0,j=0;i<W*H*4;i++)if(abs(zc_img[0][i]-zc_img[2][i])>16)j++;
  if(j){printf("check FAIL: zclear: the engine quad's picture differs from software in %d bytes\n",j);fails++;}
  printf("zclear check (ZCLEAR3D %d): engine quad %lu triangles %lu engine clears (2D fill path: %lu clears); %s\n",mode,hw_tris[0],hw_clears[0],hw_clears[1],fails?"FAIL":"ok");
  /* the first two glClears (colour + depth + stencil, then depth + stencil in the scissor) are engine quads (2 triangles each, plus the green quad's 2), none a 2D fill */
  /* v10.362: mode 1 leaves the first clear's colour to the 2D fill (one clear packet) */
  if(hw_clears[0]!=(mode==1?1UL:0UL)||hw_tris[0]!=6||hw_clears[1]!=2){printf("check FAIL: zclear: %lu 2D fills and %lu triangles with the engine quad (want %d and 6), %lu fills without (want 2)\n",hw_clears[0],hw_tris[0],mode==1?1:0,hw_clears[1]);fails++;}
 }
 return fails;
}
static void gr_out(void *u,const char *l) {(void)u;puts(l);fflush(stdout);}
static int q2_begin(int pass) {memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
 vxgl_hw_enabled=pass==0;q2c=vxgl_create_context(1,(HDC)1,0);if(!q2c||!vxgl_make_current(q2c,(HDC)1))return 0;cur=q2c;return 1;}
static void q2_swap(void) {extern int vxgl_swap(vxgl_ctx*,HDC);vxgl_swap(q2c,(HDC)1);}
static void q2_end(void) {vxgl_make_current(0,0);vxgl_destroy_context(q2c);vxgl_fb_release(&thefb);q2c=0;}
int main(int argc,char **argv) {
 int k,fails=0,only=-1;
 scene_point_param=glPointParameterfv;
 scene_blend_eq=glBlendEquation;scene_blend_col=glBlendColor;scene_active_tex=glActiveTextureARB;scene_mtc2=glMultiTexCoord2fARB;scene_mtc3=glMultiTexCoord3fARB;scene_combine=1; /* v10.304 */
 scene_ctex=(ctex_fn)glCompressedTexImage2DARB; /* v10.307 */
 scene_tex3d=(tex3d_fn)glTexImage3D;scene_ctab=(ctab_fn)glColorTable; /* v10.317 */
 scene_gen_prog=glGenProgramsARB;scene_bind_prog=glBindProgramARB;scene_prog_str=(prog_str_fn)glProgramStringARB;scene_fp=0; /* v10.308: sw_on */
 scene_ati_gen=glGenFragmentShadersATI;scene_ati_bind=glBindFragmentShaderATI;scene_ati_begin=glBeginFragmentShaderATI;scene_ati_end=glEndFragmentShaderATI;scene_ati_smap=glSampleMapATI; /* v10.321 */
 scene_glsl_create=glCreateShader;scene_glsl_prog=glCreateProgram;scene_glsl_src=(glsl_src_fn)glShaderSource;scene_glsl_compile=glCompileShader;scene_glsl_link=glLinkProgram;scene_glsl_use=glUseProgram;scene_glsl_attach=glAttachShader;scene_glsl_loc=glGetUniformLocation;scene_glsl_u1i=glUniform1i;scene_glsl_u4f=glUniform4f;
 scene_fbo_gen=(fbo_gen_fn)host_gp("glGenFramebuffersEXT");scene_fbo_bind=(fbo_bind_fn)host_gp("glBindFramebufferEXT");scene_fbo_tex2d=(fbo_tex2d_fn)host_gp("glFramebufferTexture2DEXT"); /* v10.321 */
 scene_rb_gen=(fbo_gen_fn)host_gp("glGenRenderbuffersEXT");scene_rb_bind=(fbo_bind_fn)host_gp("glBindRenderbufferEXT");scene_rb_storage=(rb_storage_fn)host_gp("glRenderbufferStorageEXT");
 scene_fbo_rb=(fbo_rb_fn)host_gp("glFramebufferRenderbufferEXT");scene_fbo_status=(fbo_status_fn)host_gp("glCheckFramebufferStatusEXT");scene_fbo_del=(fbo_del_fn)host_gp("glDeleteFramebuffersEXT");
 scene_rb_del=(fbo_del_fn)host_gp("glDeleteRenderbuffersEXT");scene_genmip=(genmip_fn)host_gp("glGenerateMipmapEXT");
 scene_ati_cop2=glColorFragmentOp2ATI;scene_ati_cop3=glColorFragmentOp3ATI;scene_ati_aop2=glAlphaFragmentOp2ATI;scene_ati_const=glSetFragmentShaderConstantATI;
 if(argc>1&&!strcmp(argv[1],"-v"))verbose=1;
 if(getenv("VXGL_FRAMELOG"))vxgl_framelog=1; /* v10.343 */
 {extern int vxgl_gl_zclear3d;const char *e=getenv("VXGL_ZCLEAR3D");if(e)vxgl_gl_zclear3d=atoi(e);} /* v10.346: default 1; 0 the 2D fill; v10.362: 2 the colour in the quad too */
 if(getenv("VXGL_NOCOLOR"))vxgl_gl_nocolor=atoi(getenv("VXGL_NOCOLOR"))!=0; /* v10.346 */
 if(getenv("GLREPLAY")){extern int gr_check(const char*,void*,const char*);struct {void *(*gp)(const char*);int (*b)(void*,int,unsigned,unsigned);void (*s)(void*);void (*e)(void*);void (*o)(void*,const char*);void *u;} io;
  if(getenv("VXGL_CBITS"))host_cbits=atoi(getenv("VXGL_CBITS"));if(getenv("VXGL_ZBITS"))host_zbits=atoi(getenv("VXGL_ZBITS"));
  io.gp=host_gp;io.b=gr_begin;io.s=gr_swap;io.e=gr_end;io.o=gr_out;io.u=0;return gr_check(getenv("GLREPLAY"),&io,getenv("GLREPLAY_BMP"));}
 if(argc>2)only=atoi(argv[2]);
 if(getenv("VXGL_GUARD")){extern uint32_t vx3dn_guard,vx_d3d_feat;vx3dn_guard=atoi(getenv("VXGL_GUARD"))!=0;if(vx3dn_guard)vx_d3d_feat|=VX_F_GUARD;else vx_d3d_feat&=~VX_F_GUARD;}
 if(getenv("VXGL_STRIPS")){extern int vxgl_gl_strips;vxgl_gl_strips=atoi(getenv("VXGL_STRIPS"));}
 if(getenv("VXGL_SNAP")){extern int vxgl_gl_snap;vxgl_gl_snap=atoi(getenv("VXGL_SNAP"));}
 vx_d3d_gl_hwflat=1; /* v10.482: as the ICD's default (ACCEL_OPENGL_HWFLAT 1) */
 if(getenv("VXGL_FLATBATCH"))vxgl_gl_flatbatch=atoi(getenv("VXGL_FLATBATCH"));if(getenv("VXGL_HWFLAT"))vx_d3d_gl_hwflat=atoi(getenv("VXGL_HWFLAT"));
 if(getenv("VXGL_EMIT")){extern int _tnl_vx855_emit;_tnl_vx855_emit=atoi(getenv("VXGL_EMIT"));}if(getenv("VXGL_SSEPASS")){extern int vxgl_ssepass_on;vxgl_ssepass_on=atoi(getenv("VXGL_SSEPASS"));} /* v10.488: the scenes with the pass */if(getenv("VXGL_CLIPSTRIP")){extern int _tnl_vx855_clipstrip;_tnl_vx855_clipstrip=atoi(getenv("VXGL_CLIPSTRIP"));}if(getenv("VXGL_LIGHTFAST")){extern int _mesa_vx855_lightfast;_mesa_vx855_lightfast=atoi(getenv("VXGL_LIGHTFAST"));} /* v10.485 */if(getenv("VXGL_RSQRT")){extern int _mesa_vx855_rsqrt;_mesa_vx855_rsqrt=atoi(getenv("VXGL_RSQRT"));} /* v10.484 */ /* v10.482: the bench's settings */
 {extern int vxgl_gl_trilist,_vbo_vx855_fastvtx,_tnl_vx855_cvacache; /* v10.500 */
  if(getenv("VXGL_TRILIST"))vxgl_gl_trilist=atoi(getenv("VXGL_TRILIST"));if(getenv("VXGL_FASTVTX"))_vbo_vx855_fastvtx=atoi(getenv("VXGL_FASTVTX"));if(getenv("VXGL_CVACACHE"))_tnl_vx855_cvacache=atoi(getenv("VXGL_CVACACHE"));{extern int vxgl_gl_storesnap,vxgl_gl_glapione,vxgl_gl_ubcolor;extern uint32_t vx_d3d_prepack,vx_d3d_indexed;if(getenv("VXGL_STORESNAP"))vxgl_gl_storesnap=atoi(getenv("VXGL_STORESNAP"));
   vx_d3d_prepack=getenv("VXGL_PREPACK")?atoi(getenv("VXGL_PREPACK")):1;vx_d3d_indexed=getenv("VXGL_INDEXED")?atoi(getenv("VXGL_INDEXED")):1; /* .573: on, as the ICD */if(getenv("VXGL_UBCOLOR"))vxgl_gl_ubcolor=atoi(getenv("VXGL_UBCOLOR"));{extern int _vbo_vx855_fastindex;if(getenv("VXGL_FASTINDEX"))_vbo_vx855_fastindex=atoi(getenv("VXGL_FASTINDEX"));}
   if(getenv("VXGL_GLLIST"))vx_d3d_gl_gllist=atoi(getenv("VXGL_GLLIST"));if(getenv("VXGL_GLWORDS")){extern uint32_t vx_d3d_gw_fixed;int gw=atoi(getenv("VXGL_GLWORDS"));vx_d3d_gl_glwords=gw!=0;vx_d3d_gw_fixed=gw>=2;}if(getenv("VXGL_PLAN"))vx_d3d_gl_plan=atoi(getenv("VXGL_PLAN"));{extern int _vbo_vx855_minmax2;_vbo_vx855_minmax2=getenv("VXGL_MINMAX2")?atoi(getenv("VXGL_MINMAX2")):1;}{extern int vxgl_gl_fan,vxgl_ssepass_range;if(getenv("VXGL_GLFAN"))vxgl_gl_fan=atoi(getenv("VXGL_GLFAN"));if(getenv("VXGL_SSERANGE"))vxgl_ssepass_range=atoi(getenv("VXGL_SSERANGE"));}{extern int vxgl_ssepass_sse2,vxgl_ssepass_defer;vxgl_ssepass_sse2=getenv("VXGL_SSE2")?atoi(getenv("VXGL_SSE2")):1;if(getenv("VXGL_SSEDEFER"))vxgl_ssepass_defer=atoi(getenv("VXGL_SSEDEFER"));}}} /* .503 */ /* v10.501: the ICD's defaults (PREPACK 1, INDEXED 0) */
 if(getenv("VXGL_TEX16"))vxgl_gl_tex16=atoi(getenv("VXGL_TEX16"));if(getenv("VXGL_TEXRING"))vxgl_gl_texring=atoi(getenv("VXGL_TEXRING")); /* v10.366 */
 if(getenv("VXGL_CLIPONLY")){int f=clip_check();printf("failures=%d\n",f);return f!=0;} /* v10.360: the perspective clip views alone */
 if(getenv("VXGL_CBITS"))host_cbits=atoi(getenv("VXGL_CBITS"));
 vxgl_gl_units3=getenv("VXGL_UNITS3")?atoi(getenv("VXGL_UNITS3")):1; /* v10.308: the ICD defaults to 0; the model keeps the engine path covered */
 if(getenv("VXGL_ZBITS"))host_zbits=atoi(getenv("VXGL_ZBITS"));
 if(getenv("VXGL_BATCH"))vxgl_batch_max=(DWORD)atoi(getenv("VXGL_BATCH")); /* v10.275: ACCEL_OPENGL_BATCH */
 if(getenv("VXGL_GLIX")) { /* .573: VXGLIX's sequence on the host (src/vx_glix_scenes.h): one context, each scene's frame with by index off, then on */
  extern uint32_t vx_d3d_indexed,vx_d3d_gl_ix[6],vx_d3d_gl_ixw[12];vxgl_ctx *c;int sc,f,on,k,bad=0;static GLubyte gi[2][W*H*4];
  memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;vxgl_hw_enabled=getenv("VXGL_GLIX_HW")?atoi(getenv("VXGL_GLIX_HW")):1;
  c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1))return 1;cur=c;
  qs_init((qs_lock_fn)host_gp("glLockArraysEXT"),(qs_unlock_fn)host_gp("glUnlockArraysEXT"));
  if(getenv("VXGL_GLIX_SC")){glx_notex=getenv("VXGL_GLIX_NOTEX")!=0;int n=getenv("VXGL_GLIX_N")?atoi(getenv("VXGL_GLIX_N")):20;const char *sq=getenv("VXGL_GLIX_SEQ");sc=atoi(getenv("VXGL_GLIX_SC")); /* for callgrind: one scene, n frames, by index as VXGL_GLIX_SEQ's first digit; not rasterized */
   host_nodraw=1;vx_d3d_indexed=(uint32_t)(!sq||sq[0]=='1');for(f=0;f<n;f++){glx_scene(sc,f);glFinish();}printf("glix scene %d, %d frames, by index %u: %u packets\n",sc,n,vx_d3d_indexed,packets);return 0;}
  for(sc=0;sc<6;sc++)for(f=0;f<2;f++){uint32_t b[6],w[12];int d=0;
   glx_scene(sc,f);glFinish(); /* once unjudged: the Quake 3 scene's arrays are left as this frame's (its first batches read what the last frame's later ones wrote) */
   for(on=0;on<2;on++){const char *sq=getenv("VXGL_GLIX_SEQ");glFinish();vx_d3d_indexed=(uint32_t)(sq?sq[on]=='1':on);memcpy(b,vx_d3d_gl_ix,sizeof(b));memcpy(w,vx_d3d_gl_ixw,sizeof(w));
    glx_scene(sc,f);glFinish();glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,gi[on]);
    {extern int vxgl_swap(vxgl_ctx*,HDC);vxgl_swap(c,(HDC)1);}}
   for(k=0;k<W*H;k++)if(memcmp(gi[0]+4*k,gi[1]+4*k,3))d++;
   printf("glix scene %d frame %d: by index %u primitives, %u triangles, %u culled, %u vertices, %u declined (%u for a vertex); why:",sc,f,vx_d3d_gl_ix[0]-b[0],vx_d3d_gl_ix[1]-b[1],vx_d3d_gl_ix[2]-b[2],vx_d3d_gl_ix[3]-b[3],vx_d3d_gl_ix[4]-b[4],vx_d3d_gl_ix[5]-b[5]);
   for(k=0;k<12;k++)printf(" %u",vx_d3d_gl_ixw[k]-w[k]);printf("; %d pixels differ%s\n",d,d||(sc!=5&&vx_d3d_gl_ix[0]==b[0])?": FAIL":"");if(d||(sc!=5&&vx_d3d_gl_ix[0]==b[0]))bad++;}
  printf("glix failures=%d\n",bad);return bad!=0;}
 if(getenv("VXGL_BENCH")) { /* v10.482: VXFLAT's lit scenes on the host (for callgrind): VXGL_BENCH = scene (4 lit, normals; 5 lit, fog; 6 a positional light, EXP fog), frames */
  vxgl_ctx *c;int sc=atoi(getenv("VXGL_BENCH")),nf=getenv("VXGL_BENCH_N")?atoi(getenv("VXGL_BENCH_N")):5,f,s,k;
  memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=32;vxgl_hw_enabled=getenv("VXGL_BENCH_HW")?atoi(getenv("VXGL_BENCH_HW")):1;host_nodraw=getenv("VXGL_BENCH_IMG")?0:1; /* v10.500: VXGL_BENCH_IMG: the last frame drawn by the model and written out; VXGL_BENCH_HW 0: software */
  if(getenv("VXGL_BENCH_CORE")){extern uint32_t vx_d3d_prepack,vx_d3d_indexed;int m=atoi(getenv("VXGL_BENCH_CORE"));vx_d3d_prepack=m&1;vx_d3d_indexed=(m>>1)&1;/* .527: bits 2-3 (VRECIPE, FASTMATCH) removed; .524: bit 4 (PAIRLIST) removed */}
  c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1))return 1;cur=c;
  if(sc==7||sc==8){ /* v10.500: Quake 2's world and Quake 3's batches (src/vx_qscenes.h) */
   qs_init((qs_lock_fn)host_gp("glLockArraysEXT"),(qs_unlock_fn)host_gp("glUnlockArraysEXT"));
   for(f=0;f<nf;f++){glViewport(0,0,W,H);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);if(sc==7)qs_q2(f);else qs_q3(f);
    if(f==nf-1&&getenv("VXGL_BENCH_IMG")){FILE *o=fopen(getenv("VXGL_BENCH_IMG"),"wb");int y,x;glFinish();glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,img[0]);
     if(o){fprintf(o,"P6 %d %d 255\n",W,H);for(y=H-1;y>=0;y--)for(x=0;x<W;x++)fwrite(img[0]+(y*W+x)*4,1,3,o);fclose(o);}}
    {extern int vxgl_swap(vxgl_ctx*,HDC);vxgl_swap(c,(HDC)1);}}
   glFinish();printf("bench scene %d, %d frames: %u packets, %u triangles, packet hash %08X (recipe vertices %u, fixed %u)\n",sc,nf,packets,c->stats[0],pk_hash,vx_d3d_gw[0],vx_d3d_gw[1]);
   {extern uint32_t vx_d3d_cut_hist[16],vx_d3d_ix[4];int k_;printf("cuts before a plain packet:");for(k_=1;k_<7;k_++)printf(" %u",vx_d3d_cut_hist[k_]);printf("; before an indexed one:");for(k_=1;k_<7;k_++)printf(" %u",vx_d3d_cut_hist[8+k_]);printf(" (1 template, 2 kind, 3 room, 4 runs, 5 the other form, 6 indices); indexed: %u triangles, %u vertices, %u packets\n",vx_d3d_ix[0],vx_d3d_ix[1],vx_d3d_ix[2]);{extern uint32_t vx_d3d_gl_ix[6];printf("by index: %u primitives, %u triangles in, %u culled, %u vertices; %u declined, %u of them for a vertex\n",vx_d3d_gl_ix[0],vx_d3d_gl_ix[1],vx_d3d_gl_ix[2],vx_d3d_gl_ix[3],vx_d3d_gl_ix[4],vx_d3d_gl_ix[5]);{extern uint32_t vx_d3d_gl_ixw[12];int z;{extern uint32_t vx_d3d_gl_ixr[4];printf("runs: %u all new, %u all there, %u some of each, %u by the table; ",vx_d3d_gl_ixr[0],vx_d3d_gl_ixr[1],vx_d3d_gl_ixr[2],vx_d3d_gl_ixr[3]);}printf("declined why:");for(z=0;z<12;z++)printf(" %u",vx_d3d_gl_ixw[z]);printf("\n");}}}return 0;}
  for(f=0;f<nf;f++){static const GLfloat dir[4]={0.3f,0.5f,1.0f,0.0f},fc[4]={0.2f,0.25f,0.35f,1},sp[4]={0.5f,0.5f,0.5f,1};
   glViewport(0,0,W,H);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);glShadeModel(GL_FLAT);
   glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-1,1,-0.75,0.75,1,10);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
   glTranslatef(0,0,-2);glScalef(1.0f/80.0f,1.0f/80.0f,1);glTranslatef(-160,-120,0);
   glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glLightfv(GL_LIGHT0,GL_POSITION,dir);glColorMaterial(GL_FRONT_AND_BACK,GL_AMBIENT_AND_DIFFUSE);glEnable(GL_COLOR_MATERIAL);
   if(sc==9||sc==10){glShadeModel(GL_SMOOTH);glEnable(GL_CULL_FACE);glCullFace(sc==9?GL_BACK:GL_FRONT);glFrontFace(sc==9?GL_CCW:GL_CW);} /* .503: SSEDEFER's scenes: smooth, half the strips facing away (9 back faces culled, 10 front faces with clockwise fronts) */
   if(sc==6||sc==9||sc==10){static const GLfloat pos[4]={160.0f,120.0f,60.0f,1.0f}; /* v10.483: Lattice's kind: a positional light (light_rgba), specular, EXP fog */
    glLightfv(GL_LIGHT0,GL_POSITION,pos);glLightf(GL_LIGHT0,GL_LINEAR_ATTENUATION,0.002f);glLightfv(GL_LIGHT0,GL_SPECULAR,sp);glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,sp);glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,20);
    glEnable(GL_FOG);glFogi(GL_FOG_MODE,GL_EXP);glFogf(GL_FOG_DENSITY,0.35f);glFogfv(GL_FOG_COLOR,fc);}
   if(sc==5){glEnable(GL_FOG);glFogi(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_START,1.5f);glFogf(GL_FOG_END,3.5f);glFogfv(GL_FOG_COLOR,fc);glLightfv(GL_LIGHT0,GL_SPECULAR,sp);glMaterialfv(GL_FRONT_AND_BACK,GL_SPECULAR,sp);glMaterialf(GL_FRONT_AND_BACK,GL_SHININESS,10);glLightModeli(GL_LIGHT_MODEL_COLOR_CONTROL,GL_SEPARATE_SPECULAR_COLOR);}
   for(s=0;s<48;s++){float y0=10.0f+s*4.6f,y1=y0+4.0f;glColor3f(0.3f+0.7f*(float)((s*37)%11)/10.0f,0.5f,0.6f);glBegin(GL_TRIANGLE_STRIP);
    for(k=0;k<64;k++){float x=8.0f+k*(304.0f/63.0f),ya=y0,yb=y1;if(sc>=9&&(((s+k/16)&1)!=0)){ya=y1;yb=y0;} /* .503: a quarter strip in four facing away */
     glNormal3f(0.6f*(float)sin(k*0.35+s*0.5),0.6f*(float)cos(k*0.2+f*0.1),1.0f);glVertex2f(x,ya);glNormal3f(0.6f*(float)sin(k*0.35+s*0.5+0.4),0.6f*(float)cos(k*0.2+f*0.1+0.3),1.0f);glVertex2f(x,yb);}
    glEnd();}
   glFinish();}
  {extern uint32_t vxgl_ssepass_defer_stat[3];printf("bench scene %d, %d frames: %u packets, flat batched %u, packet hash %08X (deferred %u: groups lit %u, unlit %u)\n",sc,nf,packets,(unsigned)vxgl_flat_stat[4],pk_hash,
   (unsigned)vxgl_ssepass_defer_stat[0],(unsigned)vxgl_ssepass_defer_stat[1],(unsigned)vxgl_ssepass_defer_stat[2]);}return 0;} /* .503: the hash; SSEDEFER's counts */
 if(getenv("VXGL_REGDUMP")) { /* v10.293: the registers VXTEX's draws write that no VXSTEN draw writes (they keep VXTEX's values through VXSTEN) */
  extern int vxtex_host(FILE*,int,void *(*)(const char*)),vxsten_host(FILE*,int,void*,void*,void*);FILE *nul=fopen("/dev/null","w");int ph,t,g,n=0;
  rd_on=1;
  for(ph=1;ph<=2;ph++){vxgl_ctx *c;memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;vxgl_hw_enabled=1;
   c=vxgl_create_context(1,(HDC)1,0);if(!c||!vxgl_make_current(c,(HDC)1))return 1;cur=c;rd_phase=ph;
   if(ph==1)vxtex_host(nul,0,host_gp);else vxsten_host(nul,0,(void*)glStencilFuncSeparate,(void*)glStencilOpSeparate,0);
   glFinish();vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);}
  for(t=0;t<256;t++)for(g=0;g<256;g++)if(rd_w[1][t][g]&&!rd_w[2][t][g]){printf("type %02X reg %02X: VXTEX last %06X, never written by VXSTEN\n",t,g,rd_v[1][t][g]);n++;}
  for(t=0;t<256;t++)for(g=0;g<256;g++)if(rd_w[2][t][g]&&!rd_w[1][t][g])printf("type %02X reg %02X: VXSTEN only (last %06X)\n",t,g,rd_v[2][t][g]);
  printf("%d registers VXTEX writes and VXSTEN never does\n",n);return 0;
 }
 printf("VXGL host test: %dx%d, colour %d, depth %d\n",W,H,host_cbits,host_zbits);
 for(k=0;k<NS;k++) {
  int pass,i,d=0,fx=-1,fy=-1,n=k==NS-1?2:1,f,lines=0;
  if(only>=0&&k!=only)continue;
  for(pass=0;pass<2;pass++) {
   vxgl_ctx *c;
   memset(vram,0x5A,sizeof(vram));memset(&thefb,0,sizeof(thefb));thefb.pf=1;thefb.hwnd=(HWND)1;thefb.cb=host_cbits;
   vxgl_hw_enabled=pass==0;
   c=vxgl_create_context(1,(HDC)1,0);if(!c){printf("context failed\n");return 1;}scene_fp=0; /* v10.308: a program per context */
   cur=c;packets=draw_packets=clear_packets=0;
   if(!vxgl_make_current(c,(HDC)1)){printf("make current failed\n");return 1;}
   if(k==0&&pass==0){const char *e=(const char*)glGetString(GL_EXTENSIONS);printf("GL_VERSION %s, GL_EXTENSIONS %u characters\n",(const char*)glGetString(GL_VERSION),e?(unsigned)strlen(e):0);if(verbose)printf("%s\n",e);}
   for(f=0;f<n;f++)scenes[k].f();
   glFinish();glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,img[pass]);
   if(glGetError())printf("  GL error\n");
   if(pass==0)printf("%2d %-38s engine path: %d packets (%d draws, %d clears), %u triangles, %u lines, %u points; fallbacks %u",k,scenes[k].name,packets,draw_packets,clear_packets,
    c->stats[0],c->stats[1],c->stats[2],c->stats[6]);
   if(pass==0)lines=c->stats[1]!=0;
   vxgl_make_current(0,0);vxgl_destroy_context(c);vxgl_fb_release(&thefb);
  }
  for(i=0;i<W*H;i++){int ch,o=0;for(ch=0;ch<3;ch++){int a=img[0][i*4+ch],b=img[1][i*4+ch];if(a-b>16||b-a>16)o=1;}if(o){if(!d){fx=i%W;fy=i/W;}d++;}}
  printf("; %d pixels differ (%.2f%%)",d,100.0*d/(W*H));
  if(d){const uint8_t *a=img[0]+(fy*W+fx)*4,*b=img[1]+(fy*W+fx)*4;printf(" first at %d,%d: engine %02X%02X%02X software %02X%02X%02X",fx,fy,a[0],a[1],a[2],b[0],b[1],b[2]);}
  /* the model's lines are a plain DDA, not the chip's rule (which VXGL.EXE measures on the board): a scene with engine lines is shown, not judged */
  if(lines&&d>W*H/200)printf("  (engine lines: the model's DDA, not judged)");
  else if(scenes[k].info&&d>W*H/200)printf("  (shown, not judged: the model's depth rounding)");
  else if(d>W*H/200){printf("  FAIL");fails++;
   {char nm[512];FILE *o;int j;
    for(j=0;j<2;j++){sprintf(nm,"build/hostgl/vxgl_%02d_%s.ppm",k,j?"sw":"hw");o=fopen(nm,"wb");
     if(o){int y;fprintf(o,"P6 %d %d 255\n",W,H);for(y=H-1;y>=0;y--)for(i=0;i<W;i++)fwrite(img[j]+(y*W+i)*4,1,3,o);fclose(o);}}}}
  if(getenv("VXGL_DUMP")){char nm[512];FILE *o;int j; /* v10.296: both images of every scene */
   for(j=0;j<2;j++){sprintf(nm,"build/hostgl/vxgl_%02d_%s.ppm",k,j?"sw":"hw");o=fopen(nm,"wb");
    if(o){int y;fprintf(o,"P6 %d %d 255\n",W,H);for(y=H-1;y>=0;y--)for(i=0;i<W;i++)fwrite(img[j]+(y*W+i)*4,1,3,o);fclose(o);}}}
  printf("\n");
 }
 if(only<0)fails+=checks();
 if(only<0){fails+=zclear_check(1);fails+=zclear_check(2);} /* v10.346; v10.362: both ZCLEAR3D modes */
 if(only<0)fails+=feat_check(); /* v10.353 */
 if(only<0)fails+=guard_check(); /* v10.358 */
 if(only<0)fails+=ring_check(); /* v10.366 */
 if(only<0)fails+=qscene_check(); /* v10.500 */
 if(only<0)fails+=defer_check(); /* .503 */
 if(only<0){extern int _vbo_vx855_minmax_check(void);int b=_vbo_vx855_minmax_check();printf("minmax check: %d mismatches of the SSE2 index scan against the C scans; %s\n",b,b?"FAIL":"ok");fails+=b!=0;} /* .506 */
 if(only<0)fails+=clip_check(); /* v10.360 */
 printf("failures=%d\n",fails);
 return fails!=0;
}
void vxgl_zlock(vxgl_ctx *c,int after) {(void)c;(void)after;} /* v10.344: the board-only experiment */
