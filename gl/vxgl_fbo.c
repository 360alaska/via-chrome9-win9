/* VXGL v10.321: framebuffer objects (EXT_framebuffer_object) on the engine, ACCEL_OPENGL_RTT (default 1; 0: Mesa's own renderbuffers and
 * texture wrappers, swrast draws into them as before).
 *
 * A texture attachment that is level L of a 2D texture whose image is RGB is drawn in the texture's engine copy (vxgl_tex_rtt: the copy's level,
 * X8R8G8B8, the engine's X8R8G8B8 target); a colour renderbuffer of an RGB format and a depth renderbuffer (any DEPTH_COMPONENT format, kept
 * as D16) are surfaces in video memory. These are our renderbuffers (vxgl_frb): the engine draws into them when the framebuffer object's one
 * drawn colour buffer is one and its depth buffer (if any) is one, with no stencil attachment (vxgl_fbo_target, from vxgl_state.c); swrast
 * reads and writes them through the CPU under a DirectDraw lock (the functions below; the locks are let go at the end of swrast's work and
 * before the engine draws). Rows are not flipped: row 0 is GL's row 0 (a texture's first row).
 *
 * A texture's image (Mesa's, in system memory) is older than its copy once anything drew into the copy: vxgl_tex.c reads it back before
 * Mesa uses it. Anything else (another format, a cube map face, a 3D slice, a copy that cannot be made) keeps Mesa's own wrapper, drawn by
 * swrast into the image, and the copy is made again from the image afterwards (finish_render_texture, vxgl_fbo_span_finish). */
#include "vxgl.h"
#include "context.h"
#include "fbobject.h"
#include "framebuffer.h"
#include "renderbuffer.h"
#include "texrender.h"
#include "imports.h"

int vxgl_gl_rtt=1;
#define FRB_CLASS 0x56584642U /* 'VXFB' in ClassID: one of ours */
enum {FRB_SOFT=0,FRB_TEX=1,FRB_COLOR=2,FRB_DEPTH=3};
typedef struct vxgl_frb {
 struct gl_renderbuffer Base;
 int kind;                        /* FRB_SOFT: Mesa's storage in system memory (another format, or no video memory) */
 struct gl_texture_object *tex;   /* FRB_TEX: the texture and level (the attachment holds the texture) */
 GLuint level;
 vxgl_texmem m;                   /* FRB_COLOR, FRB_DEPTH: one level, a block of video memory */
 vxgl_texmem *lm;GLuint ll;       /* while locked: the memory and level locked */
 GLubyte *cpu;uint32_t pitch;     /* while locked: the CPU address of row 0 and the row pitch */
 int written;                     /* swrast wrote it since the last vxgl_fbo_span_finish */
 int zdirty;                      /* a depth renderbuffer swrast wrote that the VxD has not been told of (Coarse Z) */
} vxgl_frb;
#define FRB(r) ((vxgl_frb*)(r))
static int is_frb(const struct gl_renderbuffer *r) {return r&&r->ClassID==FRB_CLASS;}

/* ---------------------------------------------------------------- CPU access */
#define FRB_LOCKS 16
static vxgl_frb *held[FRB_LOCKS];static int nheld;
/* v10.326 (review): under the engine lock (recursive), which a thread drawing with swrast holds from vxgl_span_start to vxgl_span_finish:
 * vxgl_hw_flush let the held mappings go before it took the lock, so another thread's flush unlocked a renderbuffer its owner was
 * still writing through */
void vxgl_fbo_settle(void) {
 int k;
 vxgl_engine_lock();
 for(k=0;k<nheld;k++){vxgl_frb *f=held[k];if(f->cpu&&f->lm)vxgl_texmem_unlock(f->lm,f->ll);f->cpu=0;f->lm=0;}
 nheld=0;
 vxgl_engine_unlock();
}
static int frb_alloc_mem(vxgl_frb *f,GLuint w,GLuint h) {
 memset(&f->m,0,sizeof(f->m));f->m.w=w;f->m.h=h;f->m.levels=1;f->m.format=VX3DN_TEX_XRGB8888;f->m.depth=1;f->m.block=vxgl_texmem_layout(&f->m);
 /* a depth buffer: room after it for the engine's Coarse Z storage (as the HAL's D16 surfaces, VX3DN_Z16_BLOCK) */
 if(f->kind==FRB_DEPTH){uint32_t zb=VX3DN_Z16_BLOCK(f->m.pitch[0],w,h);if(zb>f->m.block)f->m.block=zb;}
 if(vxgl_texmem_alloc(&f->m))return 1;
 memset(&f->m,0,sizeof(f->m));return 0;
}
static vxgl_texmem *frb_mem(vxgl_ctx *c,vxgl_frb *f,int make,GLuint *l) {
 vxgl_texmem *m=0;
 if(f->kind==FRB_TEX){if(!vxgl_tex_rtt(c,f->tex,f->level,make,&m,l))return 0;return m;}
 if(f->kind!=FRB_COLOR&&f->kind!=FRB_DEPTH)return 0;
 m=&f->m;*l=0;
 if(m->levels&&vxgl_texmem_lost(m)) { /* a mode change: the memory back, or new memory (its contents gone either way) */
  vxgl_engine_lock();vxgl_hw_flush_all();vxgl_batch_flush();vxgl_engine_unlock();
  if(!vxgl_texmem_restore(m))memset(m,0,sizeof(*m));}
 if(!m->levels&&(!make||!frb_alloc_mem(f,f->Base.Width,f->Base.Height)))return 0;
 return m;
}
static GLubyte *frb_cpu_(GLcontext *ctx,vxgl_frb *f);
/* v10.329 (review: one failed mapping read a red pixel as 0,0,0 with no error; writes went nowhere, unsaid): a failed mapping is tried
 * once more (a surface lost meanwhile is restored by frb_mem); one that still fails is GL_OUT_OF_MEMORY on the context: Mesa's span
 * callbacks have no result, so the reads give zeros and the writes are dropped, but the application is told */
static GLubyte *frb_cpu(GLcontext *ctx,vxgl_frb *f) {
 GLubyte *p=frb_cpu_(ctx,f);
 if(!p&&!(p=frb_cpu_(ctx,f))){static int n;_mesa_error(ctx,GL_OUT_OF_MEMORY,"renderbuffer not mapped");if(n++<8)vxgl_log("framebuffer object: a renderbuffer could not be mapped (GL_OUT_OF_MEMORY)\n");}
 return p;
}
static GLubyte *frb_cpu_(GLcontext *ctx,vxgl_frb *f) {
 vxgl_texmem *m;GLuint l=0;uint32_t pitch=0;GLubyte *p;
 if(f->cpu)return f->cpu;
 if(nheld>=FRB_LOCKS||!(m=frb_mem(VXGL_CTX(ctx),f,1,&l)))return 0;
 /* the engine's draws into it go first (the lock then waits for them); with another of ours already locked they were sent then (a flush here
  * would let that one go: a lock and a flush for every row swrast alternates between depth and colour) */
 if(!nheld){vxgl_engine_lock();vxgl_hw_flush_all();vxgl_batch_flush();vxgl_engine_unlock();}
 if(f->cpu)return f->cpu; /* (locked meanwhile) */
 if(!(p=vxgl_texmem_map(m,l,&pitch)))return 0;
 f->cpu=p;f->pitch=pitch;f->lm=m;f->ll=l;held[nheld++]=f;
 return p;
}
static int in_rb(const struct gl_renderbuffer *rb,GLint x,GLint y) {return x>=0&&y>=0&&(GLuint)x<rb->Width&&(GLuint)y<rb->Height;}
static void *no_pointer(GLcontext *ctx,struct gl_renderbuffer *rb,GLint x,GLint y) {(void)ctx;(void)rb;(void)x;(void)y;return 0;}

/* colour: X8R8G8B8 words; swrast's values are R G B A bytes (alpha read as 255: the attachment is RGB) */
#define CPIX(f,x,y) ((uint32_t*)((f)->cpu+(GLuint)(y)*(f)->pitch)+(x))
static uint32_t xrgb(const GLubyte *v) {return 0xFF000000UL|((uint32_t)v[0]<<16)|((uint32_t)v[1]<<8)|v[2];}
static void rgba_of(uint32_t w,GLubyte *d) {d[0]=(GLubyte)(w>>16);d[1]=(GLubyte)(w>>8);d[2]=(GLubyte)w;d[3]=255;}
static void c_get_row(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,GLint x,GLint y,void *values) {
 vxgl_frb *f=FRB(rb);GLubyte *d=(GLubyte*)values;GLuint i;
 if(!frb_cpu(ctx,f)){memset(values,0,n*4);return;}
 for(i=0;i<n;i++,d+=4){GLint xx=x+(GLint)i;rgba_of(in_rb(rb,xx,y)?*CPIX(f,xx,y):0,d);}
}
static void c_get_values(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,const GLint x[],const GLint y[],void *values) {
 vxgl_frb *f=FRB(rb);GLubyte *d=(GLubyte*)values;GLuint i;
 if(!frb_cpu(ctx,f)){memset(values,0,n*4);return;}
 for(i=0;i<n;i++,d+=4)rgba_of(in_rb(rb,x[i],y[i])?*CPIX(f,x[i],y[i]):0,d);
}
static void c_put_row(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,GLint x,GLint y,const void *values,const GLubyte *mask) {
 vxgl_frb *f=FRB(rb);const GLubyte *s=(const GLubyte*)values;GLuint i;
 if(!frb_cpu(ctx,f))return;
 f->written=1;
 for(i=0;i<n;i++,s+=4){GLint xx=x+(GLint)i;if((!mask||mask[i])&&in_rb(rb,xx,y))*CPIX(f,xx,y)=xrgb(s);}
}
static void c_put_row_rgb(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,GLint x,GLint y,const void *values,const GLubyte *mask) {
 vxgl_frb *f=FRB(rb);const GLubyte *s=(const GLubyte*)values;GLuint i;
 if(!frb_cpu(ctx,f))return;
 f->written=1;
 for(i=0;i<n;i++,s+=3){GLint xx=x+(GLint)i;if((!mask||mask[i])&&in_rb(rb,xx,y))*CPIX(f,xx,y)=xrgb(s);}
}
static void c_put_mono_row(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,GLint x,GLint y,const void *value,const GLubyte *mask) {
 vxgl_frb *f=FRB(rb);uint32_t w=xrgb((const GLubyte*)value);GLuint i;
 if(!frb_cpu(ctx,f))return;
 f->written=1;
 for(i=0;i<n;i++){GLint xx=x+(GLint)i;if((!mask||mask[i])&&in_rb(rb,xx,y))*CPIX(f,xx,y)=w;}
}
static void c_put_values(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,const GLint x[],const GLint y[],const void *values,const GLubyte *mask) {
 vxgl_frb *f=FRB(rb);const GLubyte *s=(const GLubyte*)values;GLuint i;
 if(!frb_cpu(ctx,f))return;
 f->written=1;
 for(i=0;i<n;i++,s+=4)if((!mask||mask[i])&&in_rb(rb,x[i],y[i]))*CPIX(f,x[i],y[i])=xrgb(s);
}
static void c_put_mono_values(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,const GLint x[],const GLint y[],const void *value,const GLubyte *mask) {
 vxgl_frb *f=FRB(rb);uint32_t w=xrgb((const GLubyte*)value);GLuint i;
 if(!frb_cpu(ctx,f))return;
 f->written=1;
 for(i=0;i<n;i++)if((!mask||mask[i])&&in_rb(rb,x[i],y[i]))*CPIX(f,x[i],y[i])=w;
}
static void color_funcs(struct gl_renderbuffer *rb) {
 rb->GetPointer=no_pointer;rb->GetRow=c_get_row;rb->GetValues=c_get_values;rb->PutRow=c_put_row;rb->PutRowRGB=c_put_row_rgb;
 rb->PutMonoRow=c_put_mono_row;rb->PutValues=c_put_values;rb->PutMonoValues=c_put_mono_values;
}
/* depth: 16-bit values (DataType GL_UNSIGNED_SHORT, as the engine's D16 keeps them) */
#define ZPIX(f,x,y) ((GLushort*)((f)->cpu+(GLuint)(y)*(f)->pitch)+(x))
static void z_get_row(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,GLint x,GLint y,void *values) {
 vxgl_frb *f=FRB(rb);GLushort *d=(GLushort*)values;GLuint i;
 if(!frb_cpu(ctx,f)){memset(values,0,n*2);return;}
 for(i=0;i<n;i++){GLint xx=x+(GLint)i;d[i]=in_rb(rb,xx,y)?*ZPIX(f,xx,y):0;}
}
static void z_get_values(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,const GLint x[],const GLint y[],void *values) {
 vxgl_frb *f=FRB(rb);GLushort *d=(GLushort*)values;GLuint i;
 if(!frb_cpu(ctx,f)){memset(values,0,n*2);return;}
 for(i=0;i<n;i++)d[i]=in_rb(rb,x[i],y[i])?*ZPIX(f,x[i],y[i]):0;
}
static void z_put_row(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,GLint x,GLint y,const void *values,const GLubyte *mask) {
 vxgl_frb *f=FRB(rb);const GLushort *s=(const GLushort*)values;GLuint i;
 if(!frb_cpu(ctx,f))return;
 f->written=f->zdirty=1;
 for(i=0;i<n;i++){GLint xx=x+(GLint)i;if((!mask||mask[i])&&in_rb(rb,xx,y))*ZPIX(f,xx,y)=s[i];}
}
static void z_put_mono_row(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,GLint x,GLint y,const void *value,const GLubyte *mask) {
 vxgl_frb *f=FRB(rb);GLushort v=*(const GLushort*)value;GLuint i;
 if(!frb_cpu(ctx,f))return;
 f->written=f->zdirty=1;
 for(i=0;i<n;i++){GLint xx=x+(GLint)i;if((!mask||mask[i])&&in_rb(rb,xx,y))*ZPIX(f,xx,y)=v;}
}
static void z_put_values(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,const GLint x[],const GLint y[],const void *values,const GLubyte *mask) {
 vxgl_frb *f=FRB(rb);const GLushort *s=(const GLushort*)values;GLuint i;
 if(!frb_cpu(ctx,f))return;
 f->written=f->zdirty=1;
 for(i=0;i<n;i++)if((!mask||mask[i])&&in_rb(rb,x[i],y[i]))*ZPIX(f,x[i],y[i])=s[i];
}
static void z_put_mono_values(GLcontext *ctx,struct gl_renderbuffer *rb,GLuint n,const GLint x[],const GLint y[],const void *value,const GLubyte *mask) {
 vxgl_frb *f=FRB(rb);GLushort v=*(const GLushort*)value;GLuint i;
 if(!frb_cpu(ctx,f))return;
 f->written=f->zdirty=1;
 for(i=0;i<n;i++)if((!mask||mask[i])&&in_rb(rb,x[i],y[i]))*ZPIX(f,x[i],y[i])=v;
}
static void depth_funcs(struct gl_renderbuffer *rb) {
 rb->GetPointer=no_pointer;rb->GetRow=z_get_row;rb->GetValues=z_get_values;rb->PutRow=z_put_row;rb->PutRowRGB=0;
 rb->PutMonoRow=z_put_mono_row;rb->PutValues=z_put_values;rb->PutMonoValues=z_put_mono_values;
}

/* ---------------------------------------------------------------- our renderbuffers */
static void frb_free_storage(vxgl_frb *f) {
 vxgl_fbo_settle();
 if(f->m.levels){vxgl_engine_lock();vxgl_hw_flush_all();vxgl_batch_flush();vxgl_texmem_free(&f->m);vxgl_engine_unlock();} /* the draws into it sent before it goes */
 memset(&f->m,0,sizeof(f->m));
 if(f->Base.Data){_mesa_free(f->Base.Data);f->Base.Data=0;}
 f->kind=FRB_SOFT;f->tex=0;f->zdirty=0;f->written=0;
}
static void frb_delete(struct gl_renderbuffer *rb) {frb_free_storage(FRB(rb));_mesa_free(rb);}
static GLboolean frb_storage(GLcontext *ctx,struct gl_renderbuffer *rb,GLenum ifmt,GLuint w,GLuint h);
static vxgl_frb *frb_alloc(GLuint name) {
 vxgl_frb *f=(vxgl_frb*)_mesa_calloc(sizeof(vxgl_frb));
 if(!f)return 0;
 _mesa_init_renderbuffer(&f->Base,name);
 f->Base.ClassID=FRB_CLASS;f->Base.Delete=frb_delete;f->Base.AllocStorage=frb_storage;
 return f;
}
static int engine_ok(vxgl_ctx *c) {return vxgl_gl_rtt&&vxgl_hw_enabled&&!(c->fallback&VXGL_FB_REGISTRY);}
/* glRenderbufferStorageEXT: an RGB colour format or a depth format in video memory (one level of the texture copies' block layout: rows of
 * the width x 4 bytes rounded up to 32, D16 using the first half of each), anything else (or no video memory) in Mesa's system memory */
static GLboolean frb_storage(GLcontext *ctx,struct gl_renderbuffer *rb,GLenum ifmt,GLuint w,GLuint h) {
 vxgl_frb *f=FRB(rb);int kind=0;vxgl_ctx *c=VXGL_CTX(ctx);
 frb_free_storage(f);
 /* .518 (audit): the format chosen from the request and the configuration only (ACCEL_OPENGL's render-to-texture switch, the registry
  * fallback): a depth request is 16 bits and an RGB one RGB8 whether or not video memory or the engine is there; without them the same
  * format in system memory (Mesa's), not the 24 or 32 bits Mesa would give (EXT_framebuffer_object: the choice must not depend on other state) */
 if(vxgl_gl_rtt&&!(c->fallback&VXGL_FB_REGISTRY)&&w&&h&&w<=2048&&h<=2048)switch(ifmt) {
  case GL_DEPTH_COMPONENT:case GL_DEPTH_COMPONENT16:case GL_DEPTH_COMPONENT24:case GL_DEPTH_COMPONENT32:kind=FRB_DEPTH;break;
  case 3:case GL_RGB:case GL_R3_G3_B2:case GL_RGB4:case GL_RGB5:case GL_RGB8:case GL_RGB10:case GL_RGB12:case GL_RGB16:kind=FRB_COLOR;break;
 }
 if(kind) {
  f->kind=kind;
  if(engine_ok(c)&&frb_alloc_mem(f,w,h)) {
   rb->Width=w;rb->Height=h;rb->Data=0;
   rb->RedBits=rb->GreenBits=rb->BlueBits=rb->AlphaBits=rb->IndexBits=rb->DepthBits=rb->StencilBits=0;
   if(kind==FRB_DEPTH){rb->_ActualFormat=GL_DEPTH_COMPONENT16;rb->_BaseFormat=GL_DEPTH_COMPONENT;rb->DataType=GL_UNSIGNED_SHORT;rb->DepthBits=16;depth_funcs(rb);}
   else {rb->_ActualFormat=GL_RGB8;rb->_BaseFormat=GL_RGB;rb->DataType=GL_UNSIGNED_BYTE;rb->RedBits=rb->GreenBits=rb->BlueBits=8;color_funcs(rb);}
   return GL_TRUE;
  }
  {static int n;if(n++<8)vxgl_log("renderbuffer %ux%u (%04X): no video memory or engine, system memory (software) in the same format\n",w,h,(unsigned)ifmt);}
  f->kind=FRB_SOFT;
  return _mesa_soft_renderbuffer_storage(ctx,rb,kind==FRB_DEPTH?GL_DEPTH_COMPONENT16:GL_RGB8,w,h);
 }
 f->kind=FRB_SOFT;
 return _mesa_soft_renderbuffer_storage(ctx,rb,ifmt,w,h);
}
static struct gl_renderbuffer *new_renderbuffer(GLcontext *ctx,GLuint name) {vxgl_frb *f=frb_alloc(name);(void)ctx;return f?&f->Base:0;}
/* glFramebufferTexture2DEXT (and a bind of a framebuffer object with texture attachments): ours when the engine can draw the level, else Mesa's
 * wrapper (Mesa's image read back first: the wrapper draws into it) */
static void render_texture(GLcontext *ctx,struct gl_framebuffer *fb,struct gl_renderbuffer_attachment *att) {
 vxgl_ctx *c=VXGL_CTX(ctx);struct gl_texture_object *t=att->Texture;vxgl_texmem *m;GLuint l;vxgl_frb *f=0;
 int ok=engine_ok(c)&&t&&att->CubeMapFace==0&&att->Zoffset==0&&vxgl_tex_rtt(c,t,att->TextureLevel,1,&m,&l);
 if(ok) {
  if(is_frb(att->Renderbuffer))f=FRB(att->Renderbuffer);
  else if((f=frb_alloc(0))!=0){if(att->Renderbuffer)_mesa_reference_renderbuffer(&att->Renderbuffer,NULL);_mesa_reference_renderbuffer(&att->Renderbuffer,&f->Base);}
  if(f) {
   const struct gl_texture_image *ti=t->Image[0][att->TextureLevel];
   vxgl_fbo_settle();f->kind=FRB_TEX;f->tex=t;f->level=att->TextureLevel;f->Base.AllocStorage=0;
   f->Base.Width=ti->Width;f->Base.Height=ti->Height;f->Base.InternalFormat=ti->InternalFormat;f->Base._ActualFormat=GL_RGB8;f->Base._BaseFormat=GL_RGB;
   f->Base.DataType=GL_UNSIGNED_BYTE;f->Base.RedBits=f->Base.GreenBits=f->Base.BlueBits=8;f->Base.AlphaBits=f->Base.IndexBits=f->Base.DepthBits=f->Base.StencilBits=0;
   color_funcs(&f->Base);
   return;
  }
 }
 if(is_frb(att->Renderbuffer)){vxgl_fbo_settle();_mesa_reference_renderbuffer(&att->Renderbuffer,NULL);}
 if(!vxgl_tex_sync(ctx,t)){_mesa_error(ctx,GL_OUT_OF_MEMORY,"the texture could not be read back from video memory");vxgl_tex_drop_drawn(t);} /* v10.329: swrast draws into the image from here: what the copy held is given up, said */
 _mesa_render_texture(ctx,fb,att);
}
/* the end of rendering into a texture (an unbind, a detach): what Mesa's wrapper drew into the image makes the copy stale */
static void finish_render_texture(GLcontext *ctx,struct gl_renderbuffer_attachment *att) {(void)ctx;if(att->Texture&&!is_frb(att->Renderbuffer))vxgl_tex_changed(att->Texture);}
void vxgl_fbo_functions(struct dd_function_table *f) {
 f->NewRenderbuffer=new_renderbuffer;f->RenderTexture=render_texture;f->FinishRenderTexture=finish_render_texture;
}

/* ---------------------------------------------------------------- the engine */
static int frb_surface(vxgl_ctx *c,vxgl_frb *f,int make,vx3dn_surface *s) {
 GLuint l=0,lw,lh;vxgl_texmem *m=frb_mem(c,f,make,&l);
 if(!m)return 0;
 lw=m->w>>l;lh=m->h>>l;if(!lw)lw=1;if(!lh)lh=1;
 s->offset=m->off[l];s->pitch=m->pitch[l];s->width=lw;s->height=lh;s->format=f->kind==FRB_DEPTH?VX3DN_D16:VX3DN_XRGB8888;
 return 1;
}
int vxgl_fbo_target(vxgl_ctx *c,struct gl_framebuffer *fb,int make,vx3dn_surface *t,vx3dn_surface *z) {
 struct gl_renderbuffer *cr,*dr,*sr;
 memset(t,0,sizeof(*t));memset(z,0,sizeof(*z));
 if(!vxgl_gl_rtt||!fb||!fb->Name||fb->_Status!=GL_FRAMEBUFFER_COMPLETE_EXT||fb->_NumColorDrawBuffers[0]!=1)return 0;
 cr=fb->_ColorDrawBuffers[0][0];dr=fb->Attachment[BUFFER_DEPTH].Renderbuffer;sr=fb->Attachment[BUFFER_STENCIL].Renderbuffer;
 if(!is_frb(cr)||sr||(dr&&!is_frb(dr)))return 0;
 if(!frb_surface(c,FRB(cr),make,t)||t->format!=VX3DN_XRGB8888)return 0;
 if(dr){if(!frb_surface(c,FRB(dr),make,z)||z->format!=VX3DN_D16||z->width<t->width||z->height<t->height)return 0;
  if(FRB(dr)->zdirty&&vxgl_z_written_at(z))FRB(dr)->zdirty=0; /* a notice that failed earlier goes before the engine tests it */
  if(FRB(dr)->zdirty)return 0;} /* v10.342 (review): failed again: the engine does not draw into it (software) until the VxD is told */
 return 1;
}
void vxgl_fbo_drawn(vxgl_ctx *c) {
 struct gl_framebuffer *fb=c->gl.DrawBuffer;struct gl_renderbuffer *r;
 if(!fb||!fb->Name||fb->_NumColorDrawBuffers[0]<1)return;
 r=fb->_ColorDrawBuffers[0][0];
 if(is_frb(r)&&FRB(r)->kind==FRB_TEX){vxgl_tex_drawn(FRB(r)->tex,FRB(r)->level);c->rtt_drawn=1;}
}
void vxgl_fbo_span_finish(GLcontext *ctx) {
 struct gl_framebuffer *fbs[2];int k;GLuint i;
 fbs[0]=ctx->DrawBuffer;fbs[1]=ctx->ReadBuffer;
 for(k=0;k<2;k++){struct gl_framebuffer *fb=fbs[k];
  if(!fb||!fb->Name||(k&&fb==fbs[0]))continue;
  for(i=0;i<BUFFER_COUNT;i++){struct gl_renderbuffer_attachment *a=&fb->Attachment[i];struct gl_renderbuffer *r=a->Renderbuffer;
   if(is_frb(r)){vxgl_frb *f=FRB(r);
    if(f->written){f->written=0;if(f->kind==FRB_TEX)vxgl_tex_drawn(f->tex,f->level);}
    if(f->zdirty&&f->kind==FRB_DEPTH&&f->m.levels){vx3dn_surface s;s.offset=f->m.off[0];s.pitch=f->m.pitch[0];s.width=f->m.w;s.height=f->m.h;s.format=VX3DN_D16;if(vxgl_z_written_at(&s))f->zdirty=0;}}
   else if(!k&&a->Type==GL_TEXTURE&&a->Texture)vxgl_tex_changed(a->Texture);}} /* Mesa's wrapper: swrast may have drawn into the image */
 vxgl_fbo_settle();
}
