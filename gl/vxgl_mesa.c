/* VXGL.DLL: the Mesa side (v10.239): contexts, the window's framebuffer and renderbuffers in video memory, clears, flushes, the swap
 * and the software rasterizer's span functions (y flipped: Mesa's row 0 is the bottom, the surface's the top). */
#include "vxgl.h"
#include "context.h"
#include "framebuffer.h"
#include "renderbuffer.h"
#include "buffers.h"
#include "extensions.h"
#include "imports.h"
#include "colormac.h"
#include "matrix.h"
#include "state.h"
#include "texformat.h"
#include "teximage.h"
#include "texobj.h"
#include "drivers/common/driverfuncs.h"
#include "swrast/swrast.h"
#include "swrast_setup/swrast_setup.h"
#include "vbo/vbo.h"
#include "tnl/tnl.h"
#include "tnl/t_context.h"
#include "tnl/t_pipeline.h"
#include "glapi/dispatch.h"
#include "glapi/glapi.h"
#include "glapi.h"
#include "math/m_matrix.h"

/* ---------------------------------------------------------------- span functions (spantmp2.h, depthtmp.h, stenciltmp.h) */
#define DBG 0
#define Y_FLIP(_y) (height-(_y)-1)
#define HW_LOCK()
#define HW_UNLOCK()
#define HW_CLIPLOOP() do{int minx=0,miny=0,maxx=(int)width,maxy=(int)height;
#define HW_ENDCLIPLOOP() }while(0)
#undef LOCAL_VARS
/* v10.261 (review): a double-buffered window's front renderbuffer never uses the back buffer's memory. Its storage is the front surface
 * or, when that cannot be made or locked, system memory (vxgl_icd.c); with neither (no memory at all) its spans are clipped away
 * (width 0): front drawing is lost, the back buffer is not overwritten (a front clear came out in the back) */
#define LOCAL_VARS vxgl_rb *vrb=(vxgl_rb*)rb;GLuint pitch=vrb->fb->sback.pitch,height=vrb->fb->h,width=(vrb->front&&!vrb->fb->pfront)?0:vrb->fb->w;GLint p=0;char *buf=(char*)(vrb->front?vrb->fb->pfront:vrb->fb->pback);(void)p;

/* v10.309 (review): 565 reads expand each channel from a table instead of a division a channel (the values v*255/31 and v*255/63) */
static const GLubyte vxgl_exp5[32]={0,8,16,24,32,41,49,57,65,74,82,90,98,106,115,123,131,139,148,156,164,172,180,189,197,205,213,222,230,238,246,255};
static const GLubyte vxgl_exp6[64]={0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60,64,68,72,76,80,85,89,93,97,101,105,109,113,117,121,125,129,133,137,141,145,149,153,157,161,165,170,174,178,182,186,190,194,198,202,206,210,214,218,222,226,230,234,238,242,246,250,255};
#define SPANTMP_EXP5(v) vxgl_exp5[v]
#define SPANTMP_EXP6(v) vxgl_exp6[v]
#define GET_PTR(_x,_y) (buf+(_x)*2+(_y)*pitch)
#define SPANTMP_PIXEL_FMT GL_RGB
#define SPANTMP_PIXEL_TYPE GL_UNSIGNED_SHORT_5_6_5
#define TAG(x) vxgl##x##_565
#define TAG2(x,y) vxgl##x##_565##y
#include "common/spantmp2.h"

#define GET_PTR(_x,_y) (buf+(_x)*4+(_y)*pitch)
#define SPANTMP_PIXEL_FMT GL_BGRA
#define SPANTMP_OPAQUE 1 /* v10.249: X8R8G8B8, no alpha bits: alpha reads 1.0 */
#define SPANTMP_PIXEL_TYPE GL_UNSIGNED_INT_8_8_8_8_REV
#define TAG(x) vxgl##x##_8888
#define TAG2(x,y) vxgl##x##_8888##y
#include "common/spantmp2.h"

#define LOCAL_DEPTH_VARS vxgl_rb *vrb=(vxgl_rb*)rb;GLuint depth_pitch=vrb->fb->sdepth.pitch,height=vrb->fb->h,width=vrb->fb->w;char *buf=(char*)vrb->fb->pdepth
#define LOCAL_STENCIL_VARS LOCAL_DEPTH_VARS
#define WRITE_DEPTH(_x,_y,d) *(GLushort*)(buf+(_x)*2+(_y)*depth_pitch)=(GLushort)(d);
#define READ_DEPTH(d,_x,_y) d=*(volatile GLushort*)(buf+(_x)*2+(_y)*depth_pitch);
#define TAG(x) vxgl##x##_z16
#include "common/depthtmp.h"

/* v10.302 (review): written without reading the word first (a read of video memory a pixel): the low byte is not used (the stencil is in its own plane) */
#define WRITE_DEPTH(_x,_y,d) *(GLuint*)(buf+(_x)*4+(_y)*depth_pitch)=(GLuint)(d)<<8;
#define READ_DEPTH(d,_x,_y) d=(*(volatile GLuint*)(buf+(_x)*4+(_y)*depth_pitch))>>8;
#define TAG(x) vxgl##x##_z24_s8
#include "common/depthtmp.h"

/* v10.245: stencil in its own plane, a byte a pixel (the engine's layout; the Z words' low byte is not used); v10.246: 8x8 tiles */
#undef LOCAL_STENCIL_VARS
#define LOCAL_STENCIL_VARS vxgl_rb *vrb=(vxgl_rb*)rb;GLuint stencil_pitch=vrb->fb->spitch,height=vrb->fb->h,width=vrb->fb->w;GLubyte *buf=vrb->fb->pstencil
#define WRITE_STENCIL(_x,_y,d) buf[VX3DN_SPLANE_AT(stencil_pitch,_x,_y)]=(GLubyte)(d);
#define READ_STENCIL(d,_x,_y) d=((volatile GLubyte*)buf)[VX3DN_SPLANE_AT(stencil_pitch,_x,_y)];
#define TAG(x) vxgl##x##_z24_s8
#include "common/stenciltmp.h"

/* v10.249: a double-buffered window's front buffer is its own surface, filled from the window the first time after a swap that it is
 * drawn or read, and shown in the window at glFlush / glFinish when it was drawn (it shared the back buffer's memory: a clear of the back
 * then of the front read back as the front's) */
int vxgl_ftrace; /* .591: VXGL_FTRACE: the front buffer's steps in VXGL.LOG */
static int front_drawn(GLcontext *ctx) {return ctx->Visual.doubleBufferMode&&ctx->DrawBuffer&&(ctx->DrawBuffer->_ColorDrawBufferMask[0]&BUFFER_BIT_FRONT_LEFT);}
static int front_read(GLcontext *ctx) {return ctx->Visual.doubleBufferMode&&ctx->ReadBuffer&&ctx->ReadBuffer->_ColorReadBufferIndex==BUFFER_FRONT_LEFT;}
void vxgl_span_start(GLcontext *ctx) {
 vxgl_ctx *c=VXGL_CTX(ctx);
 vxgl_engine_lock(); /* v10.321 (review): held to vxgl_span_finish: another thread's flush lets our renderbuffers' locks go (vxgl_fbo_settle) */
 vxgl_hw_flush(c);
 vxgl_tex_sync_units(ctx); /* v10.321: a texture swrast samples read back where the engine drew into its copy */
 {extern int vxgl_ftrace;if(vxgl_ftrace&&c->fb&&(front_drawn(ctx)||front_read(ctx)))vxgl_log("ftrace: span start: drawn %d, read %d, fstate %d, locked %d, hold %d, held %d, front_hw %d, target %lX (front %lX, back %lX)\n",front_drawn(ctx),front_read(ctx),c->fb->fstate,c->fb->locked,c->fb->hold,c->fb->held,c->front_hw,(unsigned long)c->core.target.offset,(unsigned long)c->fb->sfront.offset,(unsigned long)c->fb->sback.offset);}
 if(c->fb&&!c->fb->locked&&(front_drawn(ctx)||front_read(ctx)))vxgl_front_load(c->fb);
 else if(c->fb&&c->fb->locked&&!(c->fb->fstate&1)&&(front_drawn(ctx)||front_read(ctx))){static int l;if(l++<24)vxgl_log("front buffer: not loaded before this use (the surfaces are locked, %d)\n",c->fb->locked);} /* .590 */
 if(c->fb)vxgl_fb_lock(c->fb);
}
void vxgl_span_finish(GLcontext *ctx) {
 vxgl_ctx *c=VXGL_CTX(ctx);
 _swrast_flush(ctx);
 vxgl_fbo_span_finish(ctx); /* v10.321 */
 if(c->fb&&c->fb->zdirty&&vxgl_z_written(c->fb))c->fb->zdirty=0; /* v10.296; v10.301: cleared only once the VxD was told */
 if(c->fb)vxgl_fb_unlock(c->fb);
 if(c->fb&&front_drawn(ctx))c->fb->fstate|=3; /* .590: drawn into: loaded too (below, vxgl_front_drawn) */
 vxgl_engine_unlock();
}

/* ---------------------------------------------------------------- renderbuffers */
/* v10.296 (board, v10.295, VXGL "depth by software, tested by the engine": the engine drew over what swrast had written): the VxD trusts
 * a depth surface's Coarse Z records until something else writes the depth; swrast writes it through these, and vxgl_span_finish tells
 * the VxD (VX_IOCTL_2D_ACCESS naming the surface, as the HAL's Lock), which drops them */
static void zw_row(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,GLint x,GLint y,const void *v,const GLubyte *m) {vxgl_rb *r=(vxgl_rb*)rb;r->fb->zdirty=1;r->put_row(ctx,rb,n,x,y,v,m);}
static void zw_mono(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,GLint x,GLint y,const void *v,const GLubyte *m) {vxgl_rb *r=(vxgl_rb*)rb;r->fb->zdirty=1;r->put_mono_row(ctx,rb,n,x,y,v,m);}
static void zw_values(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,const GLint x[],const GLint y[],const void *v,const GLubyte *m) {vxgl_rb *r=(vxgl_rb*)rb;r->fb->zdirty=1;r->put_values(ctx,rb,n,x,y,v,m);}
static void z_watch(vxgl_rb *r) {
 r->put_row=r->Base.PutRow;r->put_mono_row=r->Base.PutMonoRow;r->put_values=r->Base.PutValues;
 if(r->put_row)r->Base.PutRow=zw_row;if(r->put_mono_row)r->Base.PutMonoRow=zw_mono;if(r->put_values)r->Base.PutValues=zw_values;
}
static void rb_delete(struct gl_renderbuffer *rb) {_mesa_free(rb);}
static GLboolean rb_storage(GLcontext *ctx,struct gl_renderbuffer *rb,GLenum ifmt,GLuint w,GLuint h) {(void)ctx;(void)ifmt;rb->Width=w;rb->Height=h;return GL_TRUE;}
static vxgl_rb *rb_new(vxgl_fb *fb,int kind,int bpp) {
 vxgl_rb *r=(vxgl_rb*)_mesa_calloc(sizeof(vxgl_rb));
 if(!r)return 0;
 _mesa_init_renderbuffer(&r->Base,0);
 r->fb=fb;r->depth=kind;r->Base.Delete=rb_delete;r->Base.AllocStorage=rb_storage;
 if(kind==0) {
  r->Base._BaseFormat=GL_RGBA;r->Base.InternalFormat=GL_RGBA;r->Base.DataType=GL_UNSIGNED_BYTE;
  if(bpp==16){r->Base.RedBits=5;r->Base.GreenBits=6;r->Base.BlueBits=5;vxglInitPointers_565(&r->Base);}
  else {r->Base.RedBits=8;r->Base.GreenBits=8;r->Base.BlueBits=8;vxglInitPointers_8888(&r->Base);}
 } else if(kind==16) {
  r->Base._BaseFormat=GL_DEPTH_COMPONENT;r->Base.InternalFormat=GL_DEPTH_COMPONENT16;r->Base.DataType=GL_UNSIGNED_INT;r->Base.DepthBits=16; /* as the DRI drivers: depthtmp.h passes GLuint values */
  vxglInitDepthPointers_z16(&r->Base);z_watch(r);
 } else if(kind==24) {
  r->Base._BaseFormat=GL_DEPTH_COMPONENT;r->Base.InternalFormat=GL_DEPTH_COMPONENT24;r->Base.DataType=GL_UNSIGNED_INT;r->Base.DepthBits=24;
  vxglInitDepthPointers_z24_s8(&r->Base);z_watch(r);
 } else {
  r->Base._BaseFormat=GL_STENCIL_INDEX;r->Base.InternalFormat=GL_STENCIL_INDEX8_EXT;r->Base.DataType=GL_UNSIGNED_BYTE;r->Base.StencilBits=8;
  vxglInitStencilPointers_z24_s8(&r->Base);
 }
 return r;
}
/* v10.239: the framebuffer's life. _mesa_initialize_framebuffer gives it one reference, the window's; each context it is bound to holds
 * another. When the window is gone (vxgl_icd.c finds it dead) its reference is dropped, and the last one runs fb_delete: Mesa's
 * renderbuffers (the accumulation buffer's system memory too), the DirectDraw surfaces and the structure are freed. A context left
 * bound to a destroyed window keeps the buffers alive until it is deleted or bound elsewhere, instead of pointing at freed memory. */
static void fb_delete(struct gl_framebuffer *b) {_mesa_free_framebuffer_data(b);vxgl_fb_destroy((vxgl_fb*)b);}
void vxgl_fb_release(vxgl_fb *fb) {
 if(fb->attached){struct gl_framebuffer *p=&fb->Base;_mesa_unreference_framebuffer(&p);}
 else vxgl_fb_destroy(fb);
}
/* the first time a window is made current: Mesa's framebuffer and our renderbuffers on it */
/* v10.329 (review): a renderbuffer that could not be made gives back the ones made before it and the framebuffer's mutex
 * (_mesa_free_framebuffer_data, with the window's reference counted off as the last unreference would), so the next MakeCurrent starts
 * again from nothing and the window's release frees only the structure; the accumulation buffer missing is a failure too */
static int fb_attach_fail(vxgl_fb *fb) {fb->Base.RefCount=0;_mesa_free_framebuffer_data(&fb->Base);return 0;}
static int fb_attach(vxgl_ctx *c,vxgl_fb *fb) {
 const GLvisual *v=&c->gl.Visual;vxgl_rb *r;int db,zb,sb,cb;
 if(fb->attached)return 1;
 vxgl_pf_info(fb->pf,&db,&zb,&sb,&cb);if(fb->cb)cb=fb->cb;
 _mesa_initialize_framebuffer(&fb->Base,v);fb->Base.Delete=fb_delete;
 r=rb_new(fb,0,cb);if(!r)return fb_attach_fail(fb);r->front=db;_mesa_add_renderbuffer(&fb->Base,BUFFER_FRONT_LEFT,&r->Base); /* single-buffered: the front is the one surface; v10.249: double-buffered, its own */
 if(db){r=rb_new(fb,0,cb);if(!r)return fb_attach_fail(fb);_mesa_add_renderbuffer(&fb->Base,BUFFER_BACK_LEFT,&r->Base);}
 if(zb){r=rb_new(fb,zb,cb);if(!r)return fb_attach_fail(fb);_mesa_add_renderbuffer(&fb->Base,BUFFER_DEPTH,&r->Base);}
 if(sb){r=rb_new(fb,8,cb);if(!r)return fb_attach_fail(fb);_mesa_add_renderbuffer(&fb->Base,BUFFER_STENCIL,&r->Base);}
 _mesa_add_soft_renderbuffers(&fb->Base,GL_FALSE,GL_FALSE,GL_FALSE,v->accumRedBits>0,GL_FALSE,GL_FALSE); /* accumulation in system memory */
 if(v->accumRedBits>0&&!fb->Base.Attachment[BUFFER_ACCUM].Renderbuffer)return fb_attach_fail(fb);
 fb->attached=1;return 1;
}
/* the core's view of the buffers, after they were made or the mode changed */
static void core_targets(vxgl_ctx *c) {
 vx3dn_info i;
 vxgl_engine_lock();
 vxgl_hw_flush(c);
 vxgl_vxd_info(&i); /* opens the VxD if a failed call closed it */
 c->core.target=c->fb?c->fb->sback:c->core.target;c->core.depth=c->fb?c->fb->sdepth:c->core.depth;
 c->core.generation=i.generation;c->core.lost=0;if(c->fb)c->fb_made=c->fb->made; /* v10.288 */
 /* swrast draws everything without a back buffer the engine can use, or without a depth surface when the format has depth (the depth
  * buffer is in system memory then: vxgl_icd.c fb_make_surfaces) */
 if(!c->fb||!c->fb->sback.format||(c->gl.Visual.depthBits&&!c->fb->sdepth.format))vxgl_fallback(c,VXGL_FB_NOHW,GL_TRUE);else vxgl_fallback(c,VXGL_FB_NOHW,GL_FALSE);
 vx_d3d_gl_state();
 c->new_state|=~0U;
 vxgl_engine_unlock();
}
static void resize(vxgl_ctx *c) {
 GLuint w0=c->fb->w,h0=c->fb->h;
 if(!vxgl_fb_validate(c,c->fb)){vxgl_log("buffers for %ux%u not made\n",c->fb->w,c->fb->h);}
 if(c->fb->w!=c->fb->Base.Width||c->fb->h!=c->fb->Base.Height)_mesa_resize_framebuffer(&c->gl,&c->fb->Base,c->fb->w,c->fb->h);
 /* v10.288 (review): also when the surfaces were made again at the same size and offset (after a mode change the heap often gives
  * the same place): the core kept the old generation and the old depth surface, and the VxD refused the frame (STALE) */
 if(w0!=c->fb->w||h0!=c->fb->h||(!c->rtt&&c->core.target.offset!=(c->front_hw?c->fb->sfront.offset:c->fb->sback.offset))||c->core.lost||c->fb_made!=c->fb->made)core_targets(c); /* v10.321: not while a framebuffer object is the target */
 /* v10.317 (review): the target is the front surface while the engine draws the front (front_hw): not a change */
 if(h0!=c->fb->h&&vxgl_flip(c))vxgl_calc_viewport(&c->gl); /* the y flip is from the window's height: an application that does not call glViewport
                                               again after a resize keeps its viewport, at the bottom of the window */
}

/* ---------------------------------------------------------------- driver functions */
static const GLubyte *get_string(GLcontext *ctx,GLenum name) {
 /* the renderer says when the engine is not drawing at all (VXGL.EXE logs it): no usable buffers in video memory, or ACCEL_OPENGL_HW = 0 */
 if(name==GL_RENDERER) {
  const vxgl_ctx *c=VXGL_CTX(ctx);
  if(c->fallback&VXGL_FB_REGISTRY)return (const GLubyte*)"VIA Chrome9 VXGL " VXGL_VERSION " (software: ACCEL_OPENGL_HW = 0)";
  if(c->fallback&VXGL_FB_NOHW)return (const GLubyte*)"VIA Chrome9 VXGL " VXGL_VERSION " (software: no video memory buffers)";
  return (const GLubyte*)"VIA Chrome9 VXGL " VXGL_VERSION;
 }
 if(name==GL_VENDOR)return (const GLubyte*)"VX855 native driver";
 return 0;
}
static void get_buffer_size(GLframebuffer *b,GLuint *w,GLuint *h) {
 vxgl_fb *fb=VXGL_FB(b);RECT r;
 if(fb->hwnd&&GetClientRect(fb->hwnd,&r)){*w=r.right-r.left;*h=r.bottom-r.top;}else{*w=fb->w;*h=fb->h;}
 if(!*w)*w=1;if(!*h)*h=1;
}
static void resize_buffers(GLcontext *ctx,GLframebuffer *b,GLuint w,GLuint h) {
 vxgl_ctx *c=VXGL_CTX(ctx);(void)w;(void)h;
 if(c->fb&&&c->fb->Base==b)resize(c);
 else _mesa_resize_framebuffer(ctx,b,w,h);
}
static void viewport(GLcontext *ctx,GLint x,GLint y,GLsizei w,GLsizei h) {
 vxgl_ctx *c=VXGL_CTX(ctx);(void)x;(void)y;(void)w;(void)h;
 if(c->fb)resize(c);
 vxgl_calc_viewport(ctx);
}
static void depth_range(GLcontext *ctx,GLclampd n,GLclampd f) {(void)n;(void)f;vxgl_calc_viewport(ctx);}
static void update_state(GLcontext *ctx,GLuint new_state) {
 vxgl_ctx *c=VXGL_CTX(ctx);
 _swrast_InvalidateState(ctx,new_state);
 _swsetup_InvalidateState(ctx,new_state);
 _vbo_InvalidateState(ctx,new_state);
 _tnl_InvalidateState(ctx,new_state);
 c->new_state|=new_state&VXGL_STATE_MASK;
}
void vxgl_hw_flush(vxgl_ctx *c) {
 uint32_t rc;
 if(!vx_d3d_cut_tag)vx_d3d_cut_tag=5; /* v10.363: the reason, when the caller named none */
 vxgl_engine_lock();
 vxgl_fbo_settle(); /* v10.321: our renderbuffers' CPU locks before the engine; v10.326 (review): under the lock */
 if(vxgl_batch_ctx)c=vxgl_batch_ctx; /* v10.288: the batch holds that context's draws: sent with its state and services (its window's kept locks let go) */
 if(!c){vxgl_engine_unlock();return;}
 {VXGL_PROF_BEGIN;
 if(c->xlate_old){static vx_d3d_state now;now=c->core.state;c->core.state=*c->xlate_old;rc=vx_d3d_gl_flush(&c->core,&c->sv);c->core.state=now;} /* v10.268 */
 else rc=vx_d3d_gl_flush(&c->core,&c->sv);
 VXGL_PROF_END(3);}
 vxgl_engine_unlock();
 if(rc){static int n;if(n++<64)vxgl_log("flush: %08lX\n",(unsigned long)rc);}
}
/* v10.287: the core's batch is one for all contexts (vx_d3d_core.c bq); the context whose draws it holds sends it. Before memory a
 * queued draw may read or write is given back (a texture's copy, a window's surfaces), whichever context drew last. */
vxgl_ctx *vxgl_batch_ctx;
void vxgl_hw_flush_all(void) {vxgl_hw_flush(0);}
static void present_single(vxgl_ctx *c) {
 if(c->fb&&(!c->gl.Visual.doubleBufferMode||(c->fb->fstate&2)))vxgl_fb_settle(c->fb); /* v10.267 */
 /* v10.302 (review): and a single-buffered window's surfaces checked as a swap does (lost after a mode change, or a resized window):
  * they were made again only by glViewport, and an application that does not call it again stayed with lost surfaces */
 if(c->fb&&!c->gl.Visual.doubleBufferMode){vxgl_hw_flush(c);vxgl_swap_blit(c->fb);resize(c);}
 if(c->fb&&c->gl.Visual.doubleBufferMode&&(c->fb->fstate&2))vxgl_front_present(c->fb); /* v10.249 */
}
/* v10.315: GL_ARB_occlusion_query on the engine (ACCEL_OPENGL_QUERY). The VxD sets the engine's count of pixels passing the Z and stencil
 * tests to 0 when a query begins and reads it when it ends (VX3DN_query, HCM p83-84); what goes to software meanwhile swrast counts into
 * the same object. v10.322 (review: the count was the chip's: another context's drawing was counted, its query set the count to 0): the
 * count is one context's (query_owner): while it counts, another context's query counts in software (the VxD answers BUSY); this
 * context's count is put aside while it is not current (MakeCurrent: op 2, 3) and while other threads draw (the VxD itself) */
static uint32_t query_token(vxgl_ctx *c) {
 static uint32_t serial;
 if(!c->query_owner){uint32_t t=(uint32_t)GetCurrentProcessId()^((++serial)*0x9E3779B1UL);c->query_owner=t?t:1;}
 return c->query_owner;
}
static void begin_query(GLcontext *ctx,GLenum target,struct gl_query_object *q) {
 vxgl_ctx *c=VXGL_CTX(ctx);uint32_t n=0;(void)q;
 c->query_hw=0;
 if(target!=GL_SAMPLES_PASSED_ARB||!vxgl_gl_query||(c->fallback&(VXGL_FB_REGISTRY|VXGL_FB_NOHW)))return;
 vxgl_hw_flush(c);vxgl_batch_flush();
 vxgl_engine_lock();c->query_hw=vxgl_vxd_query(0,query_token(c),&n);vxgl_engine_unlock();
 if(!c->query_hw){static int k;if(k++<8)vxgl_log("occlusion query: the engine's count could not be set (software counts)\n");}
 c->new_state|=VXGL_STATE_MASK; /* the query fallback is decided again */
}
static void end_query(GLcontext *ctx,GLenum target,struct gl_query_object *q) {
 vxgl_ctx *c=VXGL_CTX(ctx);
 if(target==GL_SAMPLES_PASSED_ARB&&c->query_hw){uint32_t n=0;int ok;
  vxgl_hw_flush(c);vxgl_batch_flush();
  vxgl_engine_lock();ok=vxgl_vxd_query(1,c->query_owner,&n);vxgl_engine_unlock();
  if(ok)q->Result+=n;else{static int k;
   /* v10.354: an unknown count must not make a visible object disappear. */
   q->Result=0xFFFFFFFFU;_mesa_error(ctx,GL_OUT_OF_MEMORY,"occlusion query result unavailable");
   if(k++<8)vxgl_log("occlusion query: count unavailable; conservative visible result, GL_OUT_OF_MEMORY\n");}
  c->query_hw=0;c->new_state|=VXGL_STATE_MASK;}
 q->Ready=GL_TRUE;
}
static void flush(GLcontext *ctx) {vxgl_ctx *c=VXGL_CTX(ctx);vx_d3d_cut_tag=7;vxgl_hw_flush(c);vxgl_batch_flush();present_single(c);} /* v10.275: glFlush sends the queue */
static void finish(GLcontext *ctx) {
 vxgl_ctx *c=VXGL_CTX(ctx);
 vx_d3d_cut_tag=7;vxgl_hw_flush(c);vxgl_batch_flush();present_single(c);
 /* v10.302 (review): a plain engine wait; the lock of the back and depth surfaces it was (a write lock) also made the VxD drop the
  * depth surface's Coarse Z records at every glFinish. The lock only when the VxD cannot be reached. */
 if(c->fb){vxgl_fb_settle(c->fb);if(!vxgl_engine_wait()){vxgl_fb_lock(c->fb);vxgl_fb_unlock(c->fb);vxgl_fb_settle(c->fb);}}
}
/* glClear: colour, depth and stencil through the VxD when the masks allow, the rest by swrast */
/* v10.345: registry ACCEL_OPENGL_ZCLEAR3D. v10.346: default 1 (board: fixes Quake 2 with gl_ztrick 0, whose world froze after the first
 * frame: a depth clear written behind the 3D engine's back, by a 2D-engine fill or the CPU, is not seen by the engine's next draws).
 * The window's depth clear (and stencil, colour of the same glClear) is drawn by the 3D engine itself: one quad over the whole buffer
 * at the clear depth, depth test ALWAYS. glClear's rules: the scissor and the write masks apply; nothing else does (no alpha / depth /
 * stencil test of the app's, blend, logic op, fog, textures, programs, clip planes, stipple, offset, culling). All state is pushed and
 * popped; GL errors are left as they were. Returns the buffer bits it cleared (0: none, the old path does it). = 0: the 2D fill as before */
int vxgl_gl_zclear3d=1;
static GLbitfield zclear3d(GLcontext *ctx,vxgl_ctx *c,GLbitfield mask,GLbitfield hwcol) {
 struct _glapi_table *d=ctx->Exec;GLframebuffer *fb=ctx->DrawBuffer;GLfloat z;GLenum err;GLuint i;GLbitfield done=0;
 int dz,st,col;
 if(!vxgl_gl_zclear3d||!d||!c->fb||fb!=&c->fb->Base||c->rtt||(c->fallback&(VXGL_FB_NOHW|VXGL_FB_REGISTRY|VXGL_FB_DRAWBUFFER|VXGL_FB_RENDERMODE))||!c->core.depth.format)return 0; /* the app's own state's fallbacks are not the quad's */
 dz=(mask&BUFFER_BIT_DEPTH)&&ctx->Depth.Mask&&ctx->Visual.depthBits;
 st=(mask&BUFFER_BIT_STENCIL)&&c->core.depth.format==VX3DN_D24S8&&ctx->Visual.stencilBits&&(ctx->Stencil.WriteMask[0]&0xFF);
 if(!dz&&!st)return 0; /* a colour-only clear: the 2D fill (unchanged) */
 /* the colour of the same clear too (no 2D fill after the quad); only all channels, one buffer (as the 2D path) */
 /* v10.362: ACCEL_OPENGL_ZCLEAR3D 1 (the default) leaves the colour to the 2D fill (the quad depth-only: WinTune's OpenGL fill test 104
  * MPixels/s with the colour in the quad, 182 with neither in it); 2 draws it in the quad as v10.346-v10.361 */
 col=vxgl_gl_zclear3d>=2&&hwcol&&(mask&hwcol)&&ctx->Color.ColorMask[0]&&ctx->Color.ColorMask[1]&&ctx->Color.ColorMask[2]&&ctx->Color.ColorMask[3]&&fb->_NumColorDrawBuffers[0]==1;
 if(ctx->AttribStackDepth>=MAX_ATTRIB_STACK_DEPTH||ctx->ProjectionMatrixStack.Depth+1>=ctx->ProjectionMatrixStack.MaxDepth||
    ctx->ModelviewMatrixStack.Depth+1>=ctx->ModelviewMatrixStack.MaxDepth||ctx->Query.CurrentOcclusionObject||ctx->Shader.CurrentProgram)return 0;
 /* v10.348: only the enables pushed (and the stencil group when the stencil is cleared); the few values changed besides are kept here and
  * set back (GL_ALL_ATTRIB_BITS' pop set every texture object's parameters and every light again: about 180,000 instructions a clear on
  * the host). The fragment program's and the colour sum's enables are not in Mesa's enable group: set back here too. */
 {GLenum dfunc=ctx->Depth.Func,pfront=ctx->Polygon.FrontMode,pback=ctx->Polygon.BackMode,mmode=ctx->Transform.MatrixMode;GLuint unit=ctx->Texture.CurrentUnit;
  GLint vx=ctx->Viewport.X,vy=ctx->Viewport.Y;GLsizei vw=ctx->Viewport.Width,vh=ctx->Viewport.Height;GLfloat vn=ctx->Viewport.Near,vf=ctx->Viewport.Far;
  GLboolean cm[4],fp=ctx->FragmentProgram.Enabled,csum=ctx->Fog.ColorSumEnabled,dbounds=ctx->Depth.BoundsTest;GLfloat cur[4];
  cm[0]=ctx->Color.ColorMask[0];cm[1]=ctx->Color.ColorMask[1];cm[2]=ctx->Color.ColorMask[2];cm[3]=ctx->Color.ColorMask[3];
  COPY_4V(cur,ctx->Current.Attrib[VERT_ATTRIB_COLOR0]);
  err=ctx->ErrorValue;z=(GLfloat)(ctx->Depth.Clear*2.0-1.0);if(z>1.0F)z=1.0F;if(z<-1.0F)z=-1.0F;
  CALL_PushAttrib(d,(GL_ENABLE_BIT|(st?GL_STENCIL_BUFFER_BIT:0)));
  CALL_MatrixMode(d,(GL_PROJECTION));CALL_PushMatrix(d,());CALL_LoadIdentity(d,());
  CALL_MatrixMode(d,(GL_MODELVIEW));CALL_PushMatrix(d,());CALL_LoadIdentity(d,());
  if(ctx->Color.AlphaEnabled)CALL_Disable(d,(GL_ALPHA_TEST));if(ctx->Polygon.CullFlag)CALL_Disable(d,(GL_CULL_FACE));if(ctx->Fog.Enabled)CALL_Disable(d,(GL_FOG));
  if(ctx->Depth.BoundsTest)CALL_Disable(d,(GL_DEPTH_BOUNDS_TEST_EXT)); /* glClear ignores the test */
  if(ctx->Light.Enabled)CALL_Disable(d,(GL_LIGHTING));if(ctx->Color.BlendEnabled)CALL_Disable(d,(GL_BLEND));if(ctx->Polygon.StippleFlag)CALL_Disable(d,(GL_POLYGON_STIPPLE));
  if(ctx->Polygon.OffsetFill)CALL_Disable(d,(GL_POLYGON_OFFSET_FILL));if(ctx->Polygon.SmoothFlag)CALL_Disable(d,(GL_POLYGON_SMOOTH));if(ctx->Color.ColorLogicOpEnabled)CALL_Disable(d,(GL_COLOR_LOGIC_OP));
  for(i=0;i<ctx->Const.MaxClipPlanes;i++)if(ctx->Transform.ClipPlanesEnabled&(1U<<i))CALL_Disable(d,(GL_CLIP_PLANE0+i));
  for(i=0;i<ctx->Const.MaxTextureUnits;i++)if(ctx->Texture.Unit[i].Enabled){CALL_ActiveTextureARB(d,(GL_TEXTURE0_ARB+i));
   CALL_Disable(d,(GL_TEXTURE_1D));CALL_Disable(d,(GL_TEXTURE_2D));if(ctx->Extensions.EXT_texture3D)CALL_Disable(d,(GL_TEXTURE_3D));
   if(ctx->Extensions.ARB_texture_cube_map)CALL_Disable(d,(GL_TEXTURE_CUBE_MAP_ARB));if(ctx->Extensions.NV_texture_rectangle)CALL_Disable(d,(GL_TEXTURE_RECTANGLE_NV));}
  if(fp)CALL_Disable(d,(GL_FRAGMENT_PROGRAM_ARB));
  if(ctx->VertexProgram.Enabled)CALL_Disable(d,(GL_VERTEX_PROGRAM_ARB));
  if(ctx->ATIFragmentShader.Enabled)CALL_Disable(d,(GL_FRAGMENT_SHADER_ATI));
  if(csum)CALL_Disable(d,(GL_COLOR_SUM_EXT));
  if(ctx->Stencil.TestTwoSide)CALL_Disable(d,(GL_STENCIL_TEST_TWO_SIDE_EXT));
  if(pfront!=GL_FILL||pback!=GL_FILL)CALL_PolygonMode(d,(GL_FRONT_AND_BACK,GL_FILL));
  CALL_Viewport(d,(0,0,(GLsizei)fb->Width,(GLsizei)fb->Height));CALL_DepthRange(d,(0.0,1.0));
  if(dz){CALL_Enable(d,(GL_DEPTH_TEST));CALL_DepthFunc(d,(GL_ALWAYS));}else CALL_Disable(d,(GL_DEPTH_TEST)); /* depth writes: the app's mask (on, when dz) */
  if(st){CALL_Enable(d,(GL_STENCIL_TEST));CALL_StencilFunc(d,(GL_ALWAYS,ctx->Stencil.Clear,0xFF));CALL_StencilOp(d,(GL_REPLACE,GL_REPLACE,GL_REPLACE));} /* the app's write mask */
  else CALL_Disable(d,(GL_STENCIL_TEST));
  if(!col)CALL_ColorMask(d,(GL_FALSE,GL_FALSE,GL_FALSE,GL_FALSE)); /* no colour: the engine's depth-only mode; with colour, the app's dither as glClear's */
  else CALL_Color4f(d,(ctx->Color.ClearColor[0],ctx->Color.ClearColor[1],ctx->Color.ClearColor[2],ctx->Color.ClearColor[3]));
  CALL_Begin(d,(GL_QUADS));CALL_Vertex3f(d,(-1.0F,-1.0F,z));CALL_Vertex3f(d,(1.0F,-1.0F,z));CALL_Vertex3f(d,(1.0F,1.0F,z));CALL_Vertex3f(d,(-1.0F,1.0F,z));CALL_End(d,());
  CALL_MatrixMode(d,(GL_MODELVIEW));CALL_PopMatrix(d,());CALL_MatrixMode(d,(GL_PROJECTION));CALL_PopMatrix(d,());
  CALL_PopAttrib(d,());
  CALL_MatrixMode(d,(mmode));CALL_ActiveTextureARB(d,(GL_TEXTURE0_ARB+unit));
  CALL_Viewport(d,(vx,vy,vw,vh));CALL_DepthRange(d,(vn,vf));
  if(dz)CALL_DepthFunc(d,(dfunc));
  if(pfront!=GL_FILL||pback!=GL_FILL){CALL_PolygonMode(d,(GL_FRONT,pfront));CALL_PolygonMode(d,(GL_BACK,pback));}
  if(!col)CALL_ColorMask(d,(cm[0],cm[1],cm[2],cm[3]));else CALL_Color4fv(d,(cur));
  if(fp)CALL_Enable(d,(GL_FRAGMENT_PROGRAM_ARB));if(csum)CALL_Enable(d,(GL_COLOR_SUM_EXT));
  if(dbounds)CALL_Enable(d,(GL_DEPTH_BOUNDS_TEST_EXT));
  ctx->ErrorValue=err; /* (none expected; an app's glGetError sees only its own) */
 }
 if(dz)done|=BUFFER_BIT_DEPTH;if(st)done|=BUFFER_BIT_STENCIL;if(col)done|=mask&hwcol;
 return done;
}
static void clear_(GLcontext *ctx,GLbitfield mask);
static void clear(GLcontext *ctx,GLbitfield mask) {VXGL_PROF_BEGIN;clear_(ctx,mask);VXGL_PROF_END(6);} /* v10.347: timed for VXGL.LOG */
static void clear_(GLcontext *ctx,GLbitfield mask) {
 vxgl_ctx *c=VXGL_CTX(ctx);GLframebuffer *fb=ctx->DrawBuffer;GLuint flags=0,col;vx3dn_rect r;GLubyte cc[4];
 GLbitfield hw;
 r.left=r.top=r.right=r.bottom=0;
 hw=ctx->Visual.doubleBufferMode?(c->front_hw?BUFFER_BIT_FRONT_LEFT:BUFFER_BIT_BACK_LEFT):(BUFFER_BIT_FRONT_LEFT|BUFFER_BIT_BACK_LEFT);
 {GLbitfield z3=zclear3d(ctx,c,mask,(c->fallback&(VXGL_FB_NOHW|VXGL_FB_REGISTRY))?0:hw); /* v10.346: default (the 3D engine) */
  if(z3){mask&=~z3;
   if(vxgl_framelog&&c->fl_len<(int)sizeof(c->fl_buf)-40){c->fl_len+=_snprintf(c->fl_buf+c->fl_len,sizeof(c->fl_buf)-1-c->fl_len," zclear3d %lX",(unsigned long)z3);c->fl_after_clear=1;}
   if(!mask)return;}}
 vxgl_engine_lock();
 if(c->new_state)vxgl_translate_state(c);
 /* v10.249: the engine clears the surface it draws (the back); a double-buffered front by swrast. v10.317: or the front, when the engine
  * draws into it (vxgl_state.c: filled from the window first) */
 /* (review) the same test as run_pipeline's: a front released after a failed lock (fsoft) is not cleared by the engine */
 if(c->front_hw&&c->fb){if(!(c->fb->fstate&1)){vxgl_hw_flush(c);vxgl_front_load(c->fb);}
  if(!c->fb->front||c->fb->fsoft||!c->fb->sfront.format||memcmp(&c->core.target,&c->fb->sfront,sizeof(vx3dn_surface))){c->new_state|=VXGL_STATE_MASK;vxgl_translate_state(c);}}
 if(c->rtt&&c->fb&&fb!=&c->fb->Base)hw=fb->_ColorDrawBufferMask[0]; /* v10.321: a framebuffer object the engine draws into (its one colour attachment) */
 if(c->fb&&(fb==&c->fb->Base||c->rtt)&&!(c->fallback&(VXGL_FB_NOHW|VXGL_FB_REGISTRY))) {
  int32_t th=(int32_t)c->core.target.height,tw=(int32_t)c->core.target.width;
  if((mask&hw)&&ctx->Color.ColorMask[0]&&ctx->Color.ColorMask[1]&&ctx->Color.ColorMask[2]&&ctx->Color.ColorMask[3]&&fb->_NumColorDrawBuffers[0]==1){
   flags|=1;mask&=~hw;}
  if((mask&BUFFER_BIT_DEPTH)&&ctx->Depth.Mask&&c->core.depth.format){flags|=2;mask&=~BUFFER_BIT_DEPTH;}
  if((mask&BUFFER_BIT_STENCIL)&&(ctx->Stencil.WriteMask[0]&0xFF)==0xFF&&c->core.depth.format==VX3DN_D24S8){flags|=4;mask&=~BUFFER_BIT_STENCIL;}
  if(flags) {
   UNCLAMPED_FLOAT_TO_UBYTE(cc[0],ctx->Color.ClearColor[0]);UNCLAMPED_FLOAT_TO_UBYTE(cc[1],ctx->Color.ClearColor[1]);
   UNCLAMPED_FLOAT_TO_UBYTE(cc[2],ctx->Color.ClearColor[2]);UNCLAMPED_FLOAT_TO_UBYTE(cc[3],ctx->Color.ClearColor[3]);
   col=((GLuint)cc[3]<<24)|((GLuint)cc[0]<<16)|((GLuint)cc[1]<<8)|cc[2];
   r.left=fb->_Xmin;r.right=fb->_Xmax;
   if(c->rtt){r.top=fb->_Ymin;r.bottom=fb->_Ymax;}else{r.top=th-fb->_Ymax;r.bottom=th-fb->_Ymin;} /* v10.321: a framebuffer object's row 0 is GL's */
   if(r.top<0)r.top=0;if(r.bottom>th)r.bottom=th;if(r.right>tw)r.right=tw;if(r.left<0)r.left=0;
   if(vx_d3d_gl_clear(&c->core,&c->sv,flags,col,ctx->Depth.Clear,(GLuint)ctx->Stencil.Clear,&r)==0){c->stats[5]++;if((flags&1)&&c->front_hw)c->fb->fstate|=3;if((flags&1)&&c->rtt)vxgl_fbo_drawn(c);} /* v10.317: shown at glFlush / glFinish */
   else {{static int n;if(n++<16)vxgl_log("hardware clear refused\n");}mask|=((flags&1)?hw&fb->_ColorDrawBufferMask[0]:0)|((flags&2)?BUFFER_BIT_DEPTH:0)|((flags&4)?BUFFER_BIT_STENCIL:0);}
  }
 }
 {extern int vxgl_ftrace;if(vxgl_ftrace&&c->fb)vxgl_log("ftrace: clear: engine flags %lX, software mask %lX, engine's colour buffer %s, front_hw %d, fstate %d, target %lX (front %lX, back %lX), fallback %lX\n",(unsigned long)flags,(unsigned long)mask,hw==BUFFER_BIT_FRONT_LEFT?"front":hw==BUFFER_BIT_BACK_LEFT?"back":"both",c->front_hw,c->fb->fstate,(unsigned long)c->core.target.offset,(unsigned long)c->fb->sfront.offset,(unsigned long)c->fb->sback.offset,(unsigned long)c->fallback);}
 if(mask)_swrast_Clear(ctx,mask);
 if(vxgl_framelog&&c->fl_len<(int)sizeof(c->fl_buf)-120){ /* v10.343: what this clear did (engine flags, area, depth value, the core's answer; what software cleared) */
  c->fl_len+=_snprintf(c->fl_buf+c->fl_len,sizeof(c->fl_buf)-1-c->fl_len," clear[hw %lX %ld,%ld-%ld,%ld z%.3f stats5 %lu sw %lX dmask %d zsurf %lX/%lu]",(unsigned long)flags,(long)r.left,(long)r.top,(long)r.right,(long)r.bottom,
   (double)ctx->Depth.Clear,(unsigned long)c->stats[5],(unsigned long)mask,(int)ctx->Depth.Mask,(unsigned long)c->core.depth.offset,(unsigned long)c->core.depth.format);
  c->fl_after_clear=1;}
 vxgl_engine_unlock();
}

/* ---------------------------------------------------------------- contexts */
vxgl_ctx *vxgl_create_context(int pf,HDC hdc,vxgl_ctx *share) {
 vxgl_ctx *c;GLvisual *vis;struct dd_function_table f;int db,zb,sb,cb;TNLcontext *tnl;
 (void)hdc;
 if(!vxgl_pf_info(pf,&db,&zb,&sb,&cb))return 0;
 {vxgl_fb *f=hdc?vxgl_fb_for(hdc):0;if(f&&f->cb)cb=f->cb;} /* the window's colour depth (fixed at SetPixelFormat) */
 c=(vxgl_ctx*)_mesa_calloc(sizeof(vxgl_ctx));if(!c)return 0;
 c->batch_lost=vxgl_batch_lost; /* v10.279: a loss before it was made is not this context's */
 vis=_mesa_create_visual(GL_TRUE,db?GL_TRUE:GL_FALSE,GL_FALSE,cb==16?5:8,cb==16?6:8,cb==16?5:8,0,0,zb,sb,16,16,16,16,1);
 if(!vis){_mesa_free(c);return 0;}
 _mesa_init_driver_functions(&f);
 vxgl_tex_functions(&f); /* v10.268: texture uploads mark the engine's copy stale */
 vxgl_fbo_functions(&f); /* v10.321: framebuffer objects on the engine */
 f.GetString=get_string;f.UpdateState=update_state;f.GetBufferSize=get_buffer_size;f.ResizeBuffers=resize_buffers;
 f.Flush=flush;f.Finish=finish;f.Clear=clear;f.Viewport=viewport;f.DepthRange=depth_range;
 f.BeginQuery=begin_query;f.EndQuery=end_query; /* v10.315 */
 if(!_mesa_initialize_context(&c->gl,vis,share?&share->gl:0,&f,(void*)c)){_mesa_destroy_visual(vis);_mesa_free(c);return 0;}
 _mesa_destroy_visual(vis);
 {extern int vxgl_sse_ok;extern void vxgl_sse_install(void);if(vxgl_sse_ok)vxgl_sse_install();} /* v10.359: after Mesa's one-time table set-up */
 _mesa_enable_sw_extensions(&c->gl);
 _mesa_enable_1_3_extensions(&c->gl);
 _mesa_enable_1_4_extensions(&c->gl);
 _mesa_enable_1_5_extensions(&c->gl);
 _mesa_enable_2_0_extensions(&c->gl);
 _mesa_enable_2_1_extensions(&c->gl);
 c->gl.Extensions.EXT_texture_filter_anisotropic=GL_TRUE;
 c->gl.Const.MaxTextureMaxAnisotropy=16.0F;
 if(vxgl_gl_s3tc){c->gl.Extensions.EXT_texture_compression_s3tc=GL_TRUE;c->gl.Extensions.S3_s3tc=GL_TRUE;} /* v10.307: the built-in DXT codec (vxgl_dxtn.h), DXT copies on the engine */
 if(vxgl_gl_fxt1)c->gl.Extensions.TDFX_texture_compression_FXT1=GL_TRUE; /* v10.317: GL_3DFX_texture_compression_FXT1 (Mesa's codec; decoded copies on the engine) */
 /* v10.330 (review #21): the Mesa parts made before one that failed are torn down (in reverse order) before the context's data goes */
 {int st=0;
  if(_swrast_CreateContext(&c->gl)){st=1;if(_vbo_CreateContext(&c->gl)){st=2;if(_tnl_CreateContext(&c->gl)){st=3;if(_swsetup_CreateContext(&c->gl))st=4;}}}
  if(st<4){if(st>=3)_tnl_DestroyContext(&c->gl);if(st>=2)_vbo_DestroyContext(&c->gl);if(st>=1)_swrast_DestroyContext(&c->gl);
   _mesa_free_context_data(&c->gl);_mesa_free(c);return 0;}}
 {struct swrast_device_driver *sw=_swrast_GetDeviceDriverReference(&c->gl);sw->SpanRenderStart=vxgl_span_start;sw->SpanRenderFinish=vxgl_span_finish;}
 vxgl_init_pipeline(&c->gl); /* v10.295: Mesa's stages plus the projective coordinates (vxgl_tris.c) */
 tnl=TNL_CONTEXT(&c->gl);tnl->_DoVertexFog=GL_TRUE; /* fog factors per vertex, the engine's vertex fog (specular alpha) */
 c->pf=pf;
 _math_matrix_ctr(&c->ViewportMatrix);
 {vx3dn_surface none;memset(&none,0,sizeof(none));vx_d3d_init(&c->core,&none,&none,0);} /* the window's buffers come at MakeCurrent (core_targets) */
 c->sv.context=c;c->sv.submit=vxgl_submit;c->sv.resolve=vxgl_tex_resolve;c->sv.mips=vxgl_tex_mips;c->sv.faces=vxgl_tex_faces; /* v10.268: textures; v10.295: cube maps */
 c->sv.volume=vxgl_tex_volume; /* v10.317: 3D textures */
 memcpy(c->tss_default,c->core.state.tss,sizeof(c->tss_default));
 c->depth_scale=zb==16?1.0f/65535.0f:(zb?1.0f/16777215.0f:1.0f/65535.0f);
 vxgl_init_tri_funcs(&c->gl);
 if(!vxgl_hw_enabled)vxgl_fallback(c,VXGL_FB_REGISTRY,GL_TRUE);
 c->new_state=~0U;
 c->magic=VXGL_MAGIC;
 return c;
}
/* v10.500: the contexts current in some thread (each thread holds one at most: the threads holding one), kept under the engine lock */
int vxgl_gl_glapione=1;static int vxgl_owned;static unsigned long glapi_seen;
extern int _glapi_vx855_single(void),_glapi_vx855_threadsafe(void);extern unsigned long _glapi_vx855_switches[2];
static void own(vxgl_ctx *c,DWORD t) {if(!c->owner&&t)vxgl_owned++;else if(c->owner&&!t)vxgl_owned--;c->owner=t;}
int vxgl_destroy_context(vxgl_ctx *c) {
 GET_CURRENT_CONTEXT(cur);
 if(!c)return 0;
 /* v10.259: a context whose Mesa parts are gone (3D Pipes, board: _swsetup_DestroyContext ran on one with no swsetup and no tnl context
  * and faulted in _tnl_free_vertices) is not torn down a second time. v10.261 (review): it is also detached (no longer current in this
  * thread, no owner) and quarantined (VXGL_BROKEN: MakeCurrent refuses it), its memory left alone, and the caller told it was not freed */
 if(c->magic!=VXGL_MAGIC||!c->gl.swsetup_context||!c->gl.swtnl_context||!c->gl.swrast_context) {
  vxgl_log("DeleteContext: context %p not whole (magic %08lX, swsetup %p, tnl %p, swrast %p): detached and left alone\n",(void*)c,(unsigned long)c->magic,
   c->gl.swsetup_context,c->gl.swtnl_context,c->gl.swrast_context);
  if(cur==&c->gl)_mesa_make_current(0,0,0); /* only glapi's pointers change: the broken context is not read */
  /* v10.288 (review): its draws sent now through its core and services (not the Mesa parts), not left to whichever context comes next,
   * after textures they read may be gone; a context that is not ours (the magic) is not used: its batch is dropped */
  if(vxgl_batch_ctx==c){if(c->magic==VXGL_MAGIC)vxgl_hw_flush(c);else vx_d3d_gl_drop();vxgl_batch_ctx=0;}
  own(c,0);c->magic=VXGL_BROKEN;
  return 0;
 }
 vxgl_hw_flush(c);vxgl_fb_settle(c->fb); /* v10.267 */
 if(c->query_hw){uint32_t n;vxgl_batch_flush();vxgl_vxd_query(4,c->query_owner,&n);c->query_hw=0;} /* v10.322: a query still counting given up */
 if(vxgl_batch_ctx==c)vxgl_batch_ctx=0; /* v10.287: sent; the textures freed below must not flush through this context */
 if(cur==&c->gl)_mesa_make_current(0,0,0);
 vxgl_log("context %p: %lu triangles, %lu lines, %lu points by the engine; %lu primitives by software; %lu packets; %lu clears; %lu fallbacks; %lu swaps\n",
  (void*)c,(unsigned long)c->stats[0],(unsigned long)c->stats[1],(unsigned long)c->stats[2],(unsigned long)c->stats[3],(unsigned long)c->stats[4],
  (unsigned long)c->stats[5],(unsigned long)c->stats[6],(unsigned long)c->stats[7]);
 if(c->stats[3]){static const char *const bn[17]={"texture","engine state","logic op","colour mask","blend","stipple","draw buffer","render mode","no video memory buffers","programs",
   "ACCEL_OPENGL_HW 0","textured lines or points or q","occlusion query","polygon stipple with lines or points","points or lines condition","depth notice","depth bounds"};
  static const char *const wn[11]={"polygon stipple off","smooth points","smooth lines","smooth polygons","alpha test with smoothing","line stipple","attenuated points or sprites","wide line","point size","texture state","coverage alpha"};
  char l[900];int k,n=0;l[0]=0; /* v10.500 (Fable's list: every software draw a bug to find): the context's draws in software by fallback bit and by the state behind it */
  for(k=0;k<17;k++)if(c->fb_draws[k]&&n<(int)sizeof(l)-80)n+=_snprintf(l+n,sizeof(l)-1-n,"%s%s %lu",n?", ":"",bn[k],(unsigned long)c->fb_draws[k]);
  for(k=0;k<11;k++)if(c->fb_why_draws[k]&&n<(int)sizeof(l)-80)n+=_snprintf(l+n,sizeof(l)-1-n,"%s(%s) %lu",n?", ":"",wn[k],(unsigned long)c->fb_why_draws[k]);
  l[sizeof(l)-1]=0;vxgl_log("context %p: draws in software by reason: %s\n",(void*)c,l);}
 vxgl_log("flat-shaded strips (v10.480, ACCEL_OPENGL_FLATSTRIP %d, the process so far): %lu sent whole (%lu triangles), %lu with colours differing (%lu triangles)\n",
  vxgl_gl_flatstrips,(unsigned long)vxgl_flat_stat[0],(unsigned long)vxgl_flat_stat[1],(unsigned long)vxgl_flat_stat[2],(unsigned long)vxgl_flat_stat[3]);
 vxgl_log("  of the strips whose colours differ: %lu as the engine's strips (%lu triangles; v10.482, ACCEL_OPENGL_HWFLATSTRIP %d), %lu as independent triangles (%lu triangles; v10.481, ACCEL_OPENGL_FLATBATCH %d), the rest to the template\n",
  (unsigned long)vxgl_flat_stat[8],(unsigned long)vxgl_flat_stat[9],vxgl_gl_hwflatstrip,(unsigned long)(vxgl_flat_stat[4]-vxgl_flat_stat[8]),(unsigned long)(vxgl_flat_stat[5]-vxgl_flat_stat[9]),vxgl_gl_flatbatch);
 vxgl_log("other flat primitives whose colours differ on the engine's flat shading (v10.483, ACCEL_OPENGL_HWFLAT %d): %lu calls (lists, fans, polygons, quads), %lu triangles\n",
  vx_d3d_gl_hwflat,(unsigned long)vxgl_flat_stat[6],(unsigned long)vxgl_flat_stat[7]);
 {extern unsigned long _tnl_vx855_light_stat[8];extern int _mesa_vx855_lightfast;unsigned long *k=_tnl_vx855_light_stat; /* v10.486 */
  vxgl_log("lighting runs (ACCEL_OPENGL_LIGHTFAST %d): %lu by the VX855 loop (%lu vertices); others: %lu Mesa's general, %lu separate specular, %lu Mesa's fast (directional, no local viewer), %lu colour index (%lu vertices); %lu of them two-sided\n",
   _mesa_vx855_lightfast,k[0],k[6],k[1],k[2],k[3],k[5],k[7],k[4]);}
 {extern unsigned long vxgl_ssepass_stat[20];extern int vxgl_ssepass_on;unsigned long *k=vxgl_ssepass_stat; /* v10.487 */
  vxgl_log("the SSE pass (v10.487, ACCEL_OPENGL_SSEPASS: bits %d): %lu draws (%lu vertices, %lu lit), %lu groups of four by the reference, %lu whole outside; %lu list draws left to the pipeline (last reason bits %lX), %lu lists without an SoA copy; of the draws %lu not from a list (%lu vertices; v10.488)\n",
   vxgl_ssepass_on,k[0],k[1],k[7],k[2],k[5],k[3],k[6],k[4],k[8],k[9]);
  {extern int vxgl_ssepass_box,vxgl_ssepass_shine,vxgl_ssepass_pack;extern uint32_t vx_d3d_gl_cullrun,vx_d3d_ps_used,vx_d3d_cullrun_tris; /* v10.490 */
   vxgl_log("the SSE pass's parts (v10.490): %lu draws whole outside by the bounding box (ACCEL_OPENGL_SSEBOX %d); specular lanes %lu by SSE, %lu by pow (ACCEL_OPENGL_SSESHINE %d); "
    "colour bytes by SSE (ACCEL_OPENGL_SSEPACK %d); %lu vertices the strip path took snapped by the pass; %lu triangles by the strip path's culling run (ACCEL_OPENGL_CULLRUN %u)\n",
    k[10],vxgl_ssepass_box,k[13],k[14],vxgl_ssepass_shine,vxgl_ssepass_pack,(unsigned long)vx_d3d_ps_used,(unsigned long)vx_d3d_cullrun_tris,(unsigned)vx_d3d_gl_cullrun);
   {extern int vxgl_ssepass_cache,vxgl_ssepass_mask,vxgl_gl_flatnocopy; /* v10.492 */
    vxgl_log("the SSE pass's parts (v10.492): %lu lit draws whose lighting set-up was kept (ACCEL_OPENGL_SSECACHE %d); the lighting's lane compares by SSE (ACCEL_OPENGL_SSEMASK %d); flat strips without the copy (ACCEL_OPENGL_FLATNOCOPY %d)\n",
     k[15],vxgl_ssepass_cache,vxgl_ssepass_mask,vxgl_gl_flatnocopy);}
   {extern int vxgl_ssepass_tight,_tnl_vx855_fastbind;extern uint32_t vx_d3d_gl_cullrun2; /* v10.493 */
    vxgl_log("v10.493: ACCEL_OPENGL_SSETIGHT %d (clip masks from tables, coordinate sets as words), ACCEL_OPENGL_CULLRUN2 %u (the culling run's area from kept floats), ACCEL_OPENGL_FASTBIND %d (Mesa's float inputs bound in place)\n",
     vxgl_ssepass_tight,(unsigned)vx_d3d_gl_cullrun2,_tnl_vx855_fastbind);}
   {extern int vxgl_ssepass_fuse,vxgl_ssepass_accept,vxgl_ssepass_box2,vxgl_ssepass_partial,_vbo_vx855_listbind;extern unsigned long _vbo_vx855_listbind_n; /* v10.495 */
    vxgl_log("v10.495: %lu groups of four by the fused kernels (ACCEL_OPENGL_SSEFUSE %d); %lu list draws inside every plane by the box, no clip compares (ACCEL_OPENGL_SSEACCEPT %d); the box from its centre (ACCEL_OPENGL_SSEBOX2 %d); %lu lit set-ups for the material only (ACCEL_OPENGL_SSEPARTIAL %d); %lu list draws bound by the short path (ACCEL_OPENGL_LISTBIND %d)\n",
     k[17],vxgl_ssepass_fuse,k[16],vxgl_ssepass_accept,vxgl_ssepass_box2,k[18],vxgl_ssepass_partial,_vbo_vx855_listbind_n,_vbo_vx855_listbind);}
   {extern int vxgl_ssepass_immbox;extern unsigned long vxgl_ssepass_imm_stat[3]; /* v10.496 */
    vxgl_log("v10.496: draws not from a list by their own box (ACCEL_OPENGL_SSEIMMBOX %d): %lu whole outside, %lu inside every plane\n",vxgl_ssepass_immbox,vxgl_ssepass_imm_stat[0],vxgl_ssepass_imm_stat[1]);}
   {uint32_t o[13];extern uint32_t __stdcall VXGL_Stats501(uint32_t*,uint32_t);extern int _vbo_vx855_fastindex;uint32_t sw=VXGL_Stats501(o,0); /* v10.501 */
    vxgl_log(".501 (the process so far): %lu draws with byte colours, %lu converted after all (ACCEL_OPENGL_UBCOLOR %lu); %lu packets prepacked (ACCEL_OPENGL_PREPACK %lu); %lu triangles in indexed packets (ACCEL_OPENGL_INDEXED %lu); the index scan by pairs and kept rebase blocks (ACCEL_OPENGL_FASTINDEX %d)\n",
     (unsigned long)o[9],(unsigned long)o[10],(unsigned long)((sw>>5)&1),(unsigned long)o[11],(unsigned long)((sw>>6)&1),(unsigned long)o[12],(unsigned long)((sw>>7)&1),_vbo_vx855_fastindex);}
   {uint32_t o[9];extern uint32_t __stdcall VXGL_Stats500(uint32_t*,uint32_t);uint32_t sw=VXGL_Stats500(o,0); /* v10.500 */
    vxgl_log("v10.500 (the process so far): triangle runs in one call %lu (%lu triangles; %lu left to the template) (ACCEL_OPENGL_TRILIST %lu); locked-array transforms kept %lu, made %lu (ACCEL_OPENGL_CVACACHE %lu); the vertex copy by words (ACCEL_OPENGL_FASTVTX %lu); the store's grid words for %lu draws, %lu vertices (ACCEL_OPENGL_STORESNAP %lu); per-thread dispatch entered %lu times, left %lu (ACCEL_OPENGL_GLAPIONE %lu)\n",
     (unsigned long)o[0],(unsigned long)o[1],(unsigned long)o[2],(unsigned long)(sw&1),(unsigned long)o[3],(unsigned long)o[4],(unsigned long)((sw>>1)&1),(unsigned long)((sw>>2)&1),(unsigned long)o[5],(unsigned long)o[6],(unsigned long)((sw>>3)&1),(unsigned long)o[7],(unsigned long)o[8],(unsigned long)((sw>>4)&1));}
   {extern int vxgl_ssepass_snap2,vxgl_ssepass_outfast,vxgl_ssepass_early;extern unsigned long vxgl_ssepass_imm_stat[3]; /* v10.497 */
    vxgl_log("v10.497: grid words from the pass (ACCEL_OPENGL_SSESNAP2 %d; %lu vertices taken by the strip path in all); the whole-outside test in the registers (ACCEL_OPENGL_SSEOUTFAST %d); %lu draws not from a list declined before the copy (ACCEL_OPENGL_SSEEARLY %d)\n",
     vxgl_ssepass_snap2,(unsigned long)vx_d3d_ps_used,vxgl_ssepass_outfast,vxgl_ssepass_imm_stat[2],vxgl_ssepass_early);}}}
 {extern unsigned long _tnl_vx855_clipstrip_stat[5];extern int _tnl_vx855_clipstrip;unsigned long *k=_tnl_vx855_clipstrip_stat; /* v10.485 */
  vxgl_log("strips with a vertex outside, split (v10.485, ACCEL_OPENGL_CLIPSTRIP %d): %lu strips; %lu runs inside as strips (%lu triangles), %lu triangles inside alone, %lu triangles clipped\n",
   _tnl_vx855_clipstrip,k[0],k[1],k[2],k[3],k[4]);}
 { /* v10.484: Mesa's primitives drawn, the process so far, by kind and shading: calls, vertices, calls with a vertex outside (the clipping path) */
  extern unsigned long _tnl_vx855_prims[2][GL_POLYGON+1][3];static const char *pn[GL_POLYGON+1]={"points","lines","line loops","line strips","triangles","triangle strips","triangle fans","quads","quad strips","polygons"};int f,k;
  for(f=0;f<2;f++)for(k=0;k<=GL_POLYGON;k++)if(_tnl_vx855_prims[f][k][0])
   vxgl_log("primitives, %s %s: %lu calls, %lu vertices (%.1f a call), %lu clipped\n",f?"flat":"smooth",pn[k],_tnl_vx855_prims[f][k][0],_tnl_vx855_prims[f][k][1],(double)_tnl_vx855_prims[f][k][1]/(double)_tnl_vx855_prims[f][k][0],_tnl_vx855_prims[f][k][2]);}
 _swsetup_DestroyContext(&c->gl);
 if(c->proj_size){_mesa_vector4f_free(&c->proj_store);c->proj_size=0;} /* v10.295 */
 {int s;for(s=0;s<3;s++)if(c->xf_size[s]){_mesa_vector4f_free(&c->xf_store[s]);c->xf_size[s]=0;}} /* v10.317 */
 _tnl_DestroyContext(&c->gl);
 _vbo_DestroyContext(&c->gl);
 _swrast_DestroyContext(&c->gl);
 _mesa_free_context_data(&c->gl);
 _math_matrix_dtr(&c->ViewportMatrix);
 c->magic=0;own(c,0);
 if(c->ext)_mesa_free(c->ext); /* v10.263 */
 _mesa_free(c);
 return 1;
}
static int make_current(vxgl_ctx *c,HDC hdc);
/* v10.259: a thread that ends with a context current: the context is free for other threads again (DLL_THREAD_DETACH, vxgl_icd.c) */
void vxgl_thread_gone(void) {GET_CURRENT_CONTEXT(cur);if(cur&&VXGL_CTX(cur)->owner==GetCurrentThreadId()){ /* single-threaded glapi keeps one current context for the process: only the thread that bound it lets it go */vxgl_engine_lock();own(VXGL_CTX(cur),0);FLUSH_VERTICES(cur,0);vxgl_hw_flush(VXGL_CTX(cur));vxgl_fb_settle(VXGL_CTX(cur)->fb);_mesa_make_current(0,0,0);vxgl_engine_unlock();}}
int vxgl_make_current(vxgl_ctx *c,HDC hdc) {int r;vxgl_engine_lock();r=make_current(c,hdc);vxgl_engine_unlock();return r;}
static int make_current(vxgl_ctx *c,HDC hdc) {
 GLcontext *cur;vxgl_ctx *o;vxgl_fb *fb=0;DWORD tid=GetCurrentThreadId();
 /* Mesa 7.0.3 left this to the drivers: a second thread making a context current switches glapi to per-thread context and dispatch
  * (until then they are process globals, and one thread's MakeCurrent would change the other's). First, so that the current context
  * read below is this thread's. */
 /* v10.500 (ACCEL_OPENGL_GLAPIONE, glapi.c _glapi_vx855_single): per-thread dispatch left again when no other thread holds a context */
 if(vxgl_gl_glapione&&_glapi_vx855_threadsafe()){GLcontext *mine=(GLcontext*)_glapi_get_context();int m=mine&&VXGL_CTX(mine)->owner==tid;
  if(vxgl_owned==0||(vxgl_owned==1&&m)){if(_glapi_vx855_single()){static int logged;if(logged<8){logged++;vxgl_log("MakeCurrent: thread %lX alone holds a context again: GL calls through the global dispatch (left per-thread dispatch %lu times, entered %lu)\n",(unsigned long)tid,_glapi_vx855_switches[1],_glapi_vx855_switches[0]);}}}}
 _glapi_check_multithread();
 if(_glapi_vx855_threadsafe()&&glapi_seen!=_glapi_vx855_switches[0]){glapi_seen=_glapi_vx855_switches[0];if(glapi_seen<=8)vxgl_log("MakeCurrent: a second thread (%lX) made a context current: GL calls through per-thread dispatch (TlsGetValue) while two threads hold contexts\n",(unsigned long)tid);}
 cur=(GLcontext*)_glapi_get_context();o=cur?VXGL_CTX(cur):0;
 if(o&&o->owner!=tid)o=0; /* not bound by this thread (single-threaded glapi's process-wide context): not ours to flush or release */
 /* v10.261 (review): everything that can refuse the new binding is checked before the old one is touched; a refused switch leaves the
  * old context current and still owned by this thread (v10.259 cleared its owner first, so another thread could then bind it) */
 if(c) {
  if(c->magic!=VXGL_MAGIC){vxgl_log("MakeCurrent: context %p is not usable (magic %08lX)\n",(void*)c,(unsigned long)c->magic);SetLastError(ERROR_INVALID_HANDLE);return 0;}
  /* v10.259: a context current in another thread cannot be made current here (as wglMakeCurrent documents) */
  if(c->owner&&c->owner!=tid){vxgl_log("MakeCurrent: context %p is current in thread %lX\n",(void*)c,(unsigned long)c->owner);SetLastError(ERROR_BUSY);return 0;}
  fb=vxgl_fb_for(hdc);
  if(!fb||fb->pf!=c->pf){vxgl_log("MakeCurrent: window pixel format %d, context %d\n",fb?fb->pf:0,c->pf);SetLastError(ERROR_INVALID_PIXEL_FORMAT);return 0;}
  if(!fb_attach(c,fb)){vxgl_log("MakeCurrent: the window's buffers not made\n");SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}
 }
 /* the old binding's work first: Mesa's queued vertices (else they are drawn after the switch, into the new window) and the core's batch */
 if(o){FLUSH_VERTICES(cur,0);vxgl_hw_flush(o);vxgl_fb_settle(o->fb); /* v10.267: swrast's kept locks */if(!c||o!=c||o->hdc!=hdc)vx_d3d_gl_state();
  if(o!=c&&o->query_hw){uint32_t n;vxgl_batch_flush();vxgl_vxd_query(2,o->query_owner,&n);}} /* v10.322: its occlusion count put aside */
 if(!c){_mesa_make_current(0,0,0);if(o)own(o,0);return 1;}
 {HDC oh=c->hdc;vxgl_fb *of=c->fb;c->hdc=hdc;c->fb=fb;
 if(!vxgl_fb_validate(c,fb)){vxgl_log("MakeCurrent: no video memory buffers\n");
  /* .581 (VX800 board with DirectX 6.1: no DirectDraw 7, so no surfaces; VXGL.EXE and VXTEX.EXE then faulted in swrast, which read a depth
   * and a colour buffer that had no memory behind them): a window that has never had buffers cannot be drawn to; the binding is refused and the
   * old one stays. Buffers lost later (a mode change) keep the binding as before: they are made again at the next resize. */
  if(!fb->back){c->hdc=oh;c->fb=of;
   /* .587 (bug check): the old binding stays current, so what was put aside for the switch above is taken back: its occlusion count
    * (it stopped counting for good) */
   if(o&&o!=c&&o->query_hw){uint32_t n;vxgl_batch_flush();vxgl_vxd_query(3,o->query_owner,&n);}
   SetLastError(ERROR_NOT_ENOUGH_MEMORY);return 0;}}}
 _mesa_make_current(&c->gl,&fb->Base,&fb->Base);
 if(o&&o!=c)own(o,0); /* ownership moves with the binding itself */
 own(c,tid);
 if(o!=c&&c->query_hw){uint32_t n;vxgl_batch_flush();vxgl_vxd_query(3,c->query_owner,&n);} /* v10.322: its occlusion count taken back (counts again from here) */
 if(fb->w!=fb->Base.Width||fb->h!=fb->Base.Height)_mesa_resize_framebuffer(&c->gl,&fb->Base,fb->w,fb->h);
 core_targets(c);
 if(!c->gl.Viewport.Width)_mesa_set_viewport(&c->gl,0,0,fb->w,fb->h);
 else vxgl_calc_viewport(&c->gl); /* v10.309 (review): the y flip is this window's height (a context moved to a window of another height drew at the old one's) */
 return 1;
}
int vxgl_framelog; /* v10.343: registry VXGL_FRAMELOG (default 0): a VXGL.LOG line for each of the first 400 frames of a context */
static int vxgl_swap_t(vxgl_ctx *c,HDC hdc);
int vxgl_swap(vxgl_ctx *c,HDC hdc) {int r;VXT_BEGIN;r=vxgl_swap_t(c,hdc);VXT_END(6);return r;} /* .506 */
static int vxgl_swap_t(vxgl_ctx *c,HDC hdc) {
 vxgl_fb *fb=vxgl_fb_for(hdc);int ok;
 if(!fb)return 0;
 {extern int vxgl_ftrace;if(vxgl_ftrace)vxgl_log("ftrace: swap: fstate %d, locked %d, hold %d\n",fb->fstate,fb->locked,fb->hold);}
 if(vxgl_framelog&&c->fl_frames<400){GLuint *s=c->stats,*o=c->fl_s0;
  vxgl_log("frame %lu: tris %lu, packets %lu, engine clears %lu, software primitives %lu, fallbacks %lu, fallback now %04lX, zdirty %d;%s\n",(unsigned long)c->fl_frames,
   (unsigned long)(s[0]-o[0]),(unsigned long)(s[4]-o[4]),(unsigned long)(s[5]-o[5]),(unsigned long)(s[3]-o[3]),(unsigned long)(s[6]-o[6]),(unsigned long)c->fallback,c->fb?c->fb->zdirty:-1,c->fl_buf);
  memcpy(c->fl_s0,c->stats,sizeof(c->fl_s0));c->fl_len=0;c->fl_buf[0]=0;c->fl_frames++;}
 if(fb==c->fb)_mesa_notifySwapBuffers(&c->gl); /* Mesa's queued vertices drawn */
 vx_d3d_cut_tag=7;vxgl_hw_flush(c); /* the core's batch, whichever window it is for */
 vxgl_fb_settle(fb); /* v10.267 */
 c->stats[7]++;
 {VXGL_PROF_BEGIN;ok=vxgl_swap_blit(fb);VXGL_PROF_END(7);}vxgl_prof_swap(); /* v10.347: the swap's blit, for VXGL.LOG's frame-time report */
 fb->fstate=0; /* v10.249: the window now shows the back buffer; the front surface is filled again when next used */
 if(fb==c->fb)resize(c); /* also when the blit failed: lost surfaces are made again here */
 return ok;
}
