/* VXGL v10.268: OpenGL textures on the engine. Mesa keeps every texture (swrast samples it when a draw goes to software); a texture an
 * engine draw uses gets a copy in video memory in a format the engine samples (A8R8G8B8, or X8R8G8B8 for RGB), made through DirectDraw
 * as the HAL makes Direct3D's (vxgl_texmem_*, vxgl_icd.c; the host test has its own), and the units' texture environments become
 * Direct3D texture stage state for the core (src/vx_d3d_core.c), which composes the engine's pixel program from it as for Direct3D.
 * The core finds the texture through the services (resolve: a handle -> the surface; mips: the levels' offsets).
 *
 * On the engine: 1D and 2D textures, power-of-two sizes up to 2048, no border; base formats RGBA RGB LUMINANCE LUMINANCE_ALPHA ALPHA
 * INTENSITY and paletted (the palette applied when the copy is made, made again when a palette changes); REPLACE MODULATE DECAL ADD; the
 * filters, mipmaps on the first unit (BaseLevel .. _MaxLevel, nine levels at most), REPEAT CLAMP CLAMP_TO_EDGE MIRRORED_REPEAT, LOD bias;
 * two units. Everything else goes to software as before: BLEND and COMBINE environments, 3D and cube textures, CLAMP_TO_BORDER, min/max
 * LOD clamps, depth textures and shadow compare, texture colour tables, mipmaps on the second unit, more than two units, textured lines
 * and points, projective coordinates (q other than 1).
 *
 * v10.317: the copy is made from a plan (vxgl_plan, prepare): which level it starts at and how many it has (the LOD clamps), its size and
 * padding (a non-power-of-two size or a rectangle texture: the last texel repeated out to the next power of two; CLAMP_TO_BORDER filtered
 * linearly or on a padded side: a ring of the border colour around it), and what each texel becomes (a compressed format the sampler has
 * not: decoded; a depth texture: its depth as its DEPTH_TEXTURE_MODE; the unit's texture colour table applied). A padded stage takes its
 * coordinates scaled (and offset past the ring) by the vxgl_texxf stage (vxgl_tris.c). Cube maps keep their mip chain (each face a block
 * of video memory with the same layout), 3D textures are the engine's volumes (one block of slices). */
#include "vxgl.h"
#include "colormac.h"
#include "context.h"
#include "colortab.h"
#include "pixel.h"
#include "image.h"
#include "texobj.h"
#include "texstore.h"
#include "texformat.h"
#include "teximage.h"
#include "mipmap.h"
#include "swrast/swrast.h"

/* the engine's copies, found by handle (1-based); the handle goes in the core's D3DTSS_TEXTUREMAP */
#define VXGL_MAX_TEX 4096
typedef struct vxgl_tex {
 GLuint handle;
 vxgl_texmem m;              /* the video memory copy (m.levels 0: none) */
 GLuint levels,w,h,format,base;
 int dirty;                  /* Mesa's texture changed since the copy */
 GLuint pal_gen;             /* paletted: the palette generation the copy was made with */
 GLboolean pal_shared;       /* paletted: whether the shared palette was on then */
 GLboolean failed;           /* the copy could not be made at this size: software until the texture changes */
 vxgl_texmem cf[5];          /* v10.295: a cube map's faces -X +Y -Y +Z -Z (m holds +X), one level each; v10.317: or their chains */
 int cube;                   /* v10.295: the copy is a cube map's six faces */
 GLuint fail_levels,fail_format; /* v10.287: what could not be made; fail_mem: for want of video memory (fail_freed: vxgl_texmem_freed */
 GLboolean fail_mem;          /* then), tried again when some is given back; otherwise only when the texture or what it needs changes */
 unsigned long fail_freed;
 GLuint sw,sh,sd;            /* v10.317: the size of the level the copy starts at (sd: a 3D texture's depth, else 1) */
 int padS,padT;              /* v10.317: 0 none, 1 the last texel repeated to the copy's size, 2 a ring of the border colour around the texels */
 GLuint ring;                /* v10.317: a ring copy's border colour (A R G B) */
 GLenum dmode;               /* v10.317: a depth texture's DEPTH_TEXTURE_MODE (0: not one) */
 GLuint ctab;                /* v10.317: the texture colour table the copy was made with (its checksum; 0: none) */
 int vol;                    /* v10.317: a 3D texture (m is one block of sd slices) */
 uint32_t vnew;              /* v10.321: the copy's levels drawn into (a framebuffer object, vxgl_fbo.c) since Mesa's images were read back from them */
 GLubyte *tabcopy;GLuint tabn;GLenum tabfmt; /* v10.330 (review #10): the colour table the copy was made with, entry for entry (the checksum alone could collide) */
 const void *pal_src;        /* v10.330 (review #18): the palette the copy was made with (a context's shared palette, or the texture's own) */
 vxgl_texmem spare[3];       /* v10.366: earlier copies of the same shape (ACCEL_OPENGL_TEXRING), each free again once vxgl_seq_done reaches */
 uint32_t spare_need[3];     /* its spare_need (the packets handed over when it stopped being the copy drawn with) */
 int no_ring;                /* v10.366: drawn into as a framebuffer object's texture (vxgl_fbo.c keeps &m): one copy only */
} vxgl_tex;
static vxgl_tex *tex_tab[VXGL_MAX_TEX+1];
static GLuint pal_gen=1;      /* the shared palette's changes */
static GLuint tex_next=1;
/* .509: ACCEL_OPENGL_CLAMP (GL_CLAMP filtered linearly on the engine as CLAMP_TO_EDGE, inexact) removed: it gained nothing on the board (v10.358) and cost pixels */
int vxgl_gl_combine=1,vxgl_gl_cube=1,vxgl_gl_projected=1; /* v10.295 */
int vxgl_gl_blendenv=1,vxgl_gl_qvertex=1; /* v10.300: ACCEL_OPENGL_BLENDENV, ACCEL_OPENGL_QVERTEX */
int vxgl_gl_lodclamp=1; /* v10.315: ACCEL_OPENGL_LODCLAMP */
int vxgl_gl_units3=1,vxgl_gl_s3tc=1; /* v10.307: ACCEL_OPENGL_UNITS3, ACCEL_OPENGL_S3TC. v10.308: three units default 0 (board, v10.307: the engine drew
 * scene 37's two three-unit quads black; VXDX7 v2.77 finds which part); 1 puts them on the engine. v10.313: default 1 again (the VxD
 * declares a third coordinate set, without which unit 2 read (0, 0), and names unit 2's set in FE/01) */
int vxgl_gl_longmip=1 /* v10.357: exact by default; v10.361: 1 again (a 512 or larger chain on the engine, its last levels as level 8, not all in software) */,vxgl_gl_unitmip=1,vxgl_gl_border=1,vxgl_gl_scale=1,vxgl_gl_lerp=1; /* v10.304: ACCEL_OPENGL_LONGMIP, _UNITMIP, _BORDER, _SCALE, _LERP */
int vxgl_gl_npotwrap=1,vxgl_gl_unit3d=1; /* v10.321: ACCEL_OPENGL_NPOTWRAP, ACCEL_OPENGL_UNIT3D */
int vxgl_gl_npot=1,vxgl_gl_fxt1=1,vxgl_gl_depthtex=1,vxgl_gl_ctable=1,vxgl_gl_dot3rgb=1,vxgl_gl_cubemip=1,vxgl_gl_front=1,vxgl_gl_tex3d=1; /* v10.317 (vxgl.h) */
static int lerp_here; /* v10.304: this unit is the only one (the core's LERP is a stage-0 program) */
/* v10.366: ACCEL_OPENGL_TEX16 (default 1): a texture whose base format is RGB (no alpha: Quake 2's walls and lightmaps) is copied as R5G6B5,
 * the sampler's 16-bit format (proven through Direct3D, v10.173): half the memory the engine reads for each texel and half the bytes an
 * upload writes; each channel rounded to its 5 or 6 bits (as a 16-bit Direct3D texture or a 16-bit desktop shows it). 0: X8R8G8B8 as before.
 * Not a cube map, a 3D texture, a padded or converted copy, nor a framebuffer object's (X8R8G8B8, the engine's target format) */
int vxgl_gl_tex16=1;
/* v10.366: ACCEL_OPENGL_TEXRING (default 3, at most 3; 0 = off): a texture written again (glTexSubImage2D / glTexImage2D of the same shape:
 * Quake 2's lightmaps, several times a frame) gets up to that many more copies in video memory, used in turn. The new texels go into a copy
 * no queued or drawing packet samples (vxgl_seq_done), written without DirectDraw's Lock (vxgl_texmem_direct), so the upload no longer
 * sends the queue and waits for the engine to finish everything (the 4.4 ms flush at each upload in VXGL.LOG). When every copy is still
 * read the queue is sent and the oldest tried again; still read: the wait as before. 2D textures of at most 256 KB a copy, not DXT,
 * not a framebuffer object's */
int vxgl_gl_texring=3;
unsigned long vxgl_ring_stat[4]; /* v10.366: uploads written into a free copy, after sending the queue, with a new copy, waited as before */
static void spare_free(vxgl_tex *x) {int k;for(k=0;k<3;k++)if(x->spare[k].levels)vxgl_texmem_free(&x->spare[k]);memset(x->spare,0,sizeof(x->spare));} /* (the caller sent what reads them) */

static vxgl_tex *tex_of(struct gl_texture_object *t,int make) {
 vxgl_tex *x=(vxgl_tex*)t->DriverData;GLuint h,i;
 if(x||!make)return x;
 for(i=0,h=tex_next;i<VXGL_MAX_TEX;i++,h=h%VXGL_MAX_TEX+1)if(!tex_tab[h])break;
 if(i==VXGL_MAX_TEX)return 0;
 x=(vxgl_tex*)_mesa_calloc(sizeof(*x));if(!x)return 0;
 x->handle=h;x->dirty=1;tex_tab[h]=x;tex_next=h%VXGL_MAX_TEX+1;t->DriverData=x;
 return x;
}
static void tex_free(struct gl_texture_object *t) {
 vxgl_tex *x=(vxgl_tex*)t->DriverData;
 if(!x)return;
 if(x->tabcopy){_mesa_free(x->tabcopy);x->tabcopy=0;x->tabn=0;}
 /* v10.275: the queued packets drawn before its memory goes. v10.287 (review): the core's batch first (its draws were made with this
  * copy's offsets, and the queue only gets them when it is sent), whichever context drew it (the texture may be shared), all under the
  * engine lock; the HAL's DestroySurface waits for the engine before the memory goes back */
 if(x->m.levels){int f;vxgl_engine_lock();vxgl_hw_flush_all();vxgl_batch_flush();vxgl_texmem_free(&x->m);for(f=0;f<5;f++)if(x->cf[f].levels)vxgl_texmem_free(&x->cf[f]);spare_free(x);vxgl_engine_unlock();}
 tex_tab[x->handle]=0;_mesa_free(x);t->DriverData=0;
}
static void tex_dirty(struct gl_texture_object *t) {vxgl_tex *x;if(t&&(x=(vxgl_tex*)t->DriverData)!=0){x->dirty=1;x->failed=GL_FALSE;}}
/* v10.321: render to texture (vxgl_fbo.c). The engine (and swrast, through our renderbuffer) draws into the copy's level: Mesa's image is then
 * older than the copy (vnew) and is read back from it before anything reads or changes the image (a TexImage / TexSubImage / CopyTex, its
 * parameters, glGetTexImage, glGenerateMipmapEXT, swrast sampling it, a copy made again from the image). A copy lost meanwhile (a mode
 * change) has lost what was drawn: the image stays as it was.
 * v10.329 (review: one failed mapping left the image stale for good, both reads after it 0,0,40 instead of red): a level's mark (vnew)
 * is cleared only when it was read back, or cannot be (the copy lost, the image reshaped); a mapping that fails is tried once more;
 * 0 is returned while a level stays unread, and the callers that change or read the image stop there with GL_OUT_OF_MEMORY rather
 * than work on the stale image (which the next copy would also be made from). */
static int tex_sync_(GLcontext *ctx,struct gl_texture_object *t,vxgl_tex *x) {
 GLuint l;GLbitfield ts;struct gl_pixelstore_attrib pk;uint32_t left;
 vxgl_fbo_settle();
 vxgl_engine_lock();vxgl_hw_flush_all();vxgl_batch_flush(); /* what the engine was sent is in the copy before it is read */
 left=x->vnew;
 if(x->m.levels&&!vxgl_texmem_lost(&x->m)&&!x->cube&&!x->vol)for(l=0;l<x->m.levels&&l<32;l++)if(x->vnew&(1U<<l)) {
  struct gl_texture_image *ti=t->Image[0][x->base+l];uint32_t pitch=0,lw=x->m.w>>l,lh=x->m.h>>l;GLubyte *p;
  if(!lw)lw=1;if(!lh)lh=1;
  if(!ti||!ti->Data||!ti->TexFormat||!ti->TexFormat->StoreImage||ti->IsCompressed||ti->Width!=lw||ti->Height!=lh){left&=~(1U<<l);continue;} /* nothing to read it into */
  if(!(p=vxgl_texmem_map(&x->m,l,&pitch))&&!(p=vxgl_texmem_map(&x->m,l,&pitch)))continue; /* stays marked */
  /* the copy's X8R8G8B8 words as B G R X bytes, stored by the image's own format (no pixel transfer: the texels as they are) */
  pk=ctx->DefaultPacking;pk.Alignment=4;pk.RowLength=(GLint)(pitch/4);pk.SkipPixels=pk.SkipRows=pk.SkipImages=0;pk.SwapBytes=GL_FALSE;pk.Invert=GL_FALSE;
  ts=ctx->_ImageTransferState;ctx->_ImageTransferState=0;
  if(!ti->TexFormat->StoreImage(ctx,2,ti->_BaseFormat,ti->TexFormat,ti->Data,0,0,0,(GLint)(ti->RowStride*ti->TexFormat->TexelBytes),ti->ImageOffsets,
     (GLint)lw,(GLint)lh,1,GL_BGRA,GL_UNSIGNED_BYTE,p,&pk)){static int n;if(n++<8)vxgl_log("texture %u level %u: not read back from the engine's copy\n",t->Name,x->base+l);}
  else left&=~(1U<<l);
  ctx->_ImageTransferState=ts;
  vxgl_texmem_unlock(&x->m,l);
 }
 if(!x->m.levels||vxgl_texmem_lost(&x->m)||x->cube||x->vol)left=0; /* no copy to read from (lost: what was drawn is gone) */
 x->vnew=left;
 vxgl_engine_unlock();
 if(left){static int n;if(n++<8)vxgl_log("texture %u: levels %08lX not read back yet (kept for the next try)\n",t->Name,(unsigned long)left);}
 return !left;
}
int vxgl_tex_sync(GLcontext *ctx,struct gl_texture_object *t) {vxgl_tex *x;if(t&&(x=(vxgl_tex*)t->DriverData)!=0&&x->vnew)return tex_sync_(ctx,t,x);return 1;}
/* v10.329: a level replaced whole (TexImage) needs nothing read back */
static void tex_replaced(struct gl_texture_object *t,GLint level) {vxgl_tex *x=t?(vxgl_tex*)t->DriverData:0;if(x&&level>=(GLint)x->base&&level-(GLint)x->base<32)x->vnew&=~(1U<<(level-x->base));}
void vxgl_tex_drop_drawn(struct gl_texture_object *t) {vxgl_tex *x=t?(vxgl_tex*)t->DriverData:0;if(x)x->vnew=0;}
void vxgl_tex_sync_units(GLcontext *ctx) {
 GLuint u;
 for(u=0;u<ctx->Const.MaxTextureUnits;u++)if(ctx->Texture.Unit[u]._ReallyEnabled&&!vxgl_tex_sync(ctx,ctx->Texture.Unit[u]._Current))
  _mesa_error(ctx,GL_OUT_OF_MEMORY,"a texture swrast samples could not be read back from video memory"); /* v10.329: said, not silent */
}
void vxgl_tex_drawn(struct gl_texture_object *t,GLuint level) {
 vxgl_tex *x=t?(vxgl_tex*)t->DriverData:0;
 if(x&&x->m.levels&&level>=x->base&&level<x->base+x->m.levels&&level-x->base<32)x->vnew|=1U<<(level-x->base);
}
void vxgl_tex_changed(struct gl_texture_object *t) {tex_dirty(t);}
#define SYNC(t) vxgl_tex_sync(ctx,t)
/* v10.329: an operation that reads or changes part of the image stops when it could not be read back */
#define SYNC_OR_FAIL(t) do{if(!vxgl_tex_sync(ctx,t)){_mesa_error(ctx,GL_OUT_OF_MEMORY,"the texture could not be read back from video memory");return;}}while(0)

/* ---------------------------------------------------------------- Mesa's driver functions: the store as before, then the copy is stale */
static void tex_image1d(GLcontext *ctx,GLenum target,GLint level,GLint ifmt,GLint w,GLint border,GLenum fmt,GLenum type,const GLvoid *px,
 const struct gl_pixelstore_attrib *pk,struct gl_texture_object *t,struct gl_texture_image *ti)
 {(void)SYNC(t);tex_replaced(t,level);_mesa_store_teximage1d(ctx,target,level,ifmt,w,border,fmt,type,px,pk,t,ti);tex_dirty(t);}
static void tex_image2d(GLcontext *ctx,GLenum target,GLint level,GLint ifmt,GLint w,GLint h,GLint border,GLenum fmt,GLenum type,const GLvoid *px,
 const struct gl_pixelstore_attrib *pk,struct gl_texture_object *t,struct gl_texture_image *ti)
 {(void)SYNC(t);tex_replaced(t,level);_mesa_store_teximage2d(ctx,target,level,ifmt,w,h,border,fmt,type,px,pk,t,ti);tex_dirty(t);}
static void tex_sub1d(GLcontext *ctx,GLenum target,GLint level,GLint x,GLint w,GLenum fmt,GLenum type,const GLvoid *px,
 const struct gl_pixelstore_attrib *pk,struct gl_texture_object *t,struct gl_texture_image *ti)
 {SYNC_OR_FAIL(t);_mesa_store_texsubimage1d(ctx,target,level,x,w,fmt,type,px,pk,t,ti);tex_dirty(t);}
static void tex_sub2d(GLcontext *ctx,GLenum target,GLint level,GLint x,GLint y,GLint w,GLint h,GLenum fmt,GLenum type,const GLvoid *px,
 const struct gl_pixelstore_attrib *pk,struct gl_texture_object *t,struct gl_texture_image *ti)
 {SYNC_OR_FAIL(t);_mesa_store_texsubimage2d(ctx,target,level,x,y,w,h,fmt,type,px,pk,t,ti);tex_dirty(t);}
/* v10.317: 3D textures (the engine's volumes) */
static void tex_image3d(GLcontext *ctx,GLenum target,GLint level,GLint ifmt,GLint w,GLint h,GLint d,GLint border,GLenum fmt,GLenum type,const GLvoid *px,
 const struct gl_pixelstore_attrib *pk,struct gl_texture_object *t,struct gl_texture_image *ti)
 {(void)SYNC(t);tex_replaced(t,level);_mesa_store_teximage3d(ctx,target,level,ifmt,w,h,d,border,fmt,type,px,pk,t,ti);tex_dirty(t);}
static void tex_sub3d(GLcontext *ctx,GLenum target,GLint level,GLint x,GLint y,GLint z,GLint w,GLint h,GLint d,GLenum fmt,GLenum type,const GLvoid *px,
 const struct gl_pixelstore_attrib *pk,struct gl_texture_object *t,struct gl_texture_image *ti)
 {SYNC_OR_FAIL(t);_mesa_store_texsubimage3d(ctx,target,level,x,y,z,w,h,d,fmt,type,px,pk,t,ti);tex_dirty(t);}
/* v10.317: glCopyTexImage / glCopyTexSubImage (Mesa's: they store through the TexImage / TexSubImage functions above, which mark the copy
 * stale; listed here so that nothing else is needed) */
/* v10.307: glCompressedTexImage2D / SubImage2D (S3TC) mark the copy stale as the others do */
static void tex_cimage2d(GLcontext *ctx,GLenum target,GLint level,GLint ifmt,GLsizei w,GLsizei h,GLint border,GLsizei size,const GLvoid *data,
 struct gl_texture_object *t,struct gl_texture_image *ti)
 {(void)SYNC(t);tex_replaced(t,level);_mesa_store_compressed_teximage2d(ctx,target,level,ifmt,w,h,border,size,data,t,ti);tex_dirty(t);}
static void tex_csub2d(GLcontext *ctx,GLenum target,GLint level,GLint x,GLint y,GLsizei w,GLint h,GLenum fmt,GLsizei size,const GLvoid *data,
 struct gl_texture_object *t,struct gl_texture_image *ti)
 {SYNC_OR_FAIL(t);_mesa_store_compressed_texsubimage2d(ctx,target,level,x,y,w,h,fmt,size,data,t,ti);tex_dirty(t);}
static void tex_param(GLcontext *ctx,GLenum target,struct gl_texture_object *t,GLenum pname,const GLfloat *params)
 {(void)target;(void)params;if(pname==GL_TEXTURE_BASE_LEVEL||pname==GL_TEXTURE_MAX_LEVEL||pname==GL_GENERATE_MIPMAP_SGIS){(void)SYNC(t);tex_dirty(t);}} /* (a failed read back stays marked: the copy is made again only after it) */
static void tex_palette(GLcontext *ctx,struct gl_texture_object *t) {(void)ctx;if(t)tex_dirty(t);else pal_gen++;}
static void tex_delete(GLcontext *ctx,struct gl_texture_object *t) {tex_free(t);_mesa_delete_texture_object(ctx,t);}
/* v10.321: glGetTexImage and glGenerateMipmapEXT read Mesa's images: read back first; the generated levels make the copy stale */
static void tex_get(GLcontext *ctx,GLenum target,GLint level,GLenum fmt,GLenum type,GLvoid *px,struct gl_texture_object *t,struct gl_texture_image *ti)
 {SYNC_OR_FAIL(t);_mesa_get_teximage(ctx,target,level,fmt,type,px,t,ti);}
static void tex_genmip(GLcontext *ctx,GLenum target,const struct gl_texture_unit *u,struct gl_texture_object *t)
 {SYNC_OR_FAIL(t);_mesa_generate_mipmap(ctx,target,u,t);tex_dirty(t);}
void vxgl_tex_functions(struct dd_function_table *f) {
 f->TexImage1D=tex_image1d;f->TexImage2D=tex_image2d;f->TexSubImage1D=tex_sub1d;f->TexSubImage2D=tex_sub2d;
 f->TexImage3D=tex_image3d;f->TexSubImage3D=tex_sub3d; /* v10.317 */
 f->TexParameter=tex_param;f->UpdateTexturePalette=tex_palette;f->DeleteTexture=tex_delete;
 f->CompressedTexImage2D=tex_cimage2d;f->CompressedTexSubImage2D=tex_csub2d; /* v10.307 */
 f->GetTexImage=tex_get;f->GenerateMipmap=tex_genmip; /* v10.321 */
}

/* ---------------------------------------------------------------- the core's services */
int vxgl_tex_resolve(void *ctx,uint32_t handle,vx3dn_surface *s) {
 vxgl_tex *x;(void)ctx;
 if(!handle||handle>VXGL_MAX_TEX||!(x=tex_tab[handle])||!x->m.levels)return 0;
 s->offset=x->m.off[0];s->pitch=x->m.pitch[0];s->width=x->w;s->height=x->h;s->format=x->format;
 return 1;
}
uint32_t vxgl_tex_mips(void *ctx,uint32_t handle,uint32_t *off,uint32_t max) {
 vxgl_tex *x;uint32_t l,n;(void)ctx;
 if(!handle||handle>VXGL_MAX_TEX||!(x=tex_tab[handle])||!x->m.levels)return 0;
 n=x->m.levels<max?x->m.levels:max;for(l=0;l<n;l++)off[l]=x->m.off[l];
 return n;
}

/* v10.295: a cube map's six faces (+X -X +Y -Y +Z -Z, GL's order and the packet's), 0 for a flat texture; v10.317: | VX_D3D_FACES_SHARED
 * when the faces are blocks of one layout (their mip chains at the same offsets from each face's level 0) */
uint32_t vxgl_tex_faces(void *ctx,uint32_t handle,uint32_t *off) {
 vxgl_tex *x;int f,shared;(void)ctx;
 if(!handle||handle>VXGL_MAX_TEX||!(x=tex_tab[handle])||!x->m.levels||!x->cube)return 0;
 off[0]=x->m.off[0];shared=x->m.block!=0;
 for(f=0;f<5;f++){if(!x->cf[f].levels)return 0xFFFFFFFFUL;off[f+1]=x->cf[f].off[0];if(!x->cf[f].block||x->cf[f].levels!=x->m.levels)shared=0;}
 return shared?(6UL|VX_D3D_FACES_SHARED):6UL;
}
/* v10.317: a 3D texture's depth and slice step (the core's volume service) */
uint32_t vxgl_tex_volume(void *ctx,uint32_t handle,uint32_t *out) {
 vxgl_tex *x;(void)ctx;
 if(!handle||handle>VXGL_MAX_TEX||!(x=tex_tab[handle])||!x->m.levels||!x->vol||!x->m.block)return 0;
 out[0]=x->m.depth;out[1]=x->m.slice;return 1;
}

/* ---------------------------------------------------------------- the copy */
static int pow2(GLuint v){return v&&!(v&(v-1));}
static GLuint pow2up(GLuint v){GLuint p=1;while(p<v)p<<=1;return p;}
static int mipmapped(GLenum f){return f!=GL_NEAREST&&f!=GL_LINEAR;}
/* v10.317: what a copy is made as (prepare): the level it starts at and its count, its size and padding, the texel conversions */
typedef struct {
 GLuint levels,fmt,w,h,base,sw,sh,sd;  /* w h: the copy's (powers of two); sw sh sd: the base level's */
 int padS,padT,cube,vol,mm;            /* mm: 1 always minified (the minification filter throughout), 2 always magnified */
 int npot;                             /* v10.321: a copy of the texture's own size, a side not a power of two (the sampler's own NPOT mode) */
 GLuint ring,ctab;GLenum dmode;const struct gl_color_table *tab;
 GLuint border;                        /* v10.330: the sampler's border colour (A R G B): a depth texture's converted by its depth mode */
 int xf;GLfloat sa,sb,ta,tb;           /* the coordinates the stage takes: s' = sa s + sb q, t' = ta t + tb q */
 int cx;                               /* v10.357: GL_CLAMP filtered linearly on a ring copy, exact while the draw's s (bit 0) / t (bit 1) are in 0..1 */
} vxgl_plan;
/* v10.317: a block copy's layout: level l at rel[l] (256-byte aligned) with rows of the sampler's pitch (A8R8G8B8: width x 4, 32 bytes at
 * least; v10.321: rounded up to 32 bytes, as for an NPOT width); a 3D texture's slices follow level 0, each pitch x height bytes (as the
 * engine steps a DXT chain's levels: packed) */
uint32_t vxgl_texmem_layout(vxgl_texmem *m) {
 GLuint l;uint32_t o=0;
 if(!m->depth)m->depth=1;
 for(l=0;l<m->levels&&l<VXGL_TEXLEVELS;l++){uint32_t lw=m->w>>l,lh=m->h>>l,pt;if(!lw)lw=1;if(!lh)lh=1;pt=(lw*(m->format==VX3DN_TEX_RGB565?2U:4U)+31)&~31U; /* v10.366: R5G6B5 2 bytes */
  o=(o+255)&~255U;m->rel[l]=o;m->pitch[l]=pt;
  if(!l){m->slice=pt*lh;o+=m->slice*m->depth;}else o+=pt*lh;}
 return (o+255)&~255U;
}
/* the base format the environment sees: a paletted texture's is its palette's; v10.317: a depth texture's its DEPTH_TEXTURE_MODE */
static GLenum base_format(GLcontext *ctx,struct gl_texture_object *t,const struct gl_texture_image *ti) {
 if(ti->_BaseFormat==GL_DEPTH_COMPONENT||ti->_BaseFormat==GL_DEPTH_STENCIL_EXT)return t->DepthMode;
 if(ti->_BaseFormat!=GL_COLOR_INDEX)return ti->_BaseFormat;
 return ctx->Texture.SharedPalette?ctx->Texture.Palette._BaseFormat:t->Palette._BaseFormat;
}
static int upload_(vxgl_ctx *c,struct gl_texture_object *t,vxgl_tex *x,const vxgl_plan *pl);
static void table_keep(vxgl_tex *x,const struct gl_color_table *ct); /* v10.330 */
/* v10.289: timed for VXGL.LOG */
static int upload(vxgl_ctx *c,struct gl_texture_object *t,vxgl_tex *x,const vxgl_plan *pl) {int r;VXGL_PROF_BEGIN;r=upload_(c,t,x,pl);VXGL_PROF_END(4);return r;}
/* one level of the copy from Mesa's image; v10.295: a cube map's face too */
static int fill_copy(const struct gl_texture_image *li,GLubyte *p,uint32_t pitch,uint32_t lw,uint32_t lh,GLuint fmt);
static int fill_conv(const struct gl_texture_image *li,GLuint k,GLubyte *p,uint32_t pitch,uint32_t cw,uint32_t chh,const vxgl_plan *pl);
static int plain(const vxgl_plan *pl) {return !pl->padS&&!pl->padT&&!pl->tab&&!pl->dmode;}
static int fill_direct; /* v10.366: the copy is one no packet reads (the ring): written without a lock */
static int fill_level(vxgl_tex *x,vxgl_texmem *m,GLuint l,const struct gl_texture_image *li,uint32_t lw,uint32_t lh,const vxgl_plan *pl,const struct gl_texture_object *t) {
 uint32_t pitch=0;GLubyte *p=0;GLuint levels=pl->levels,fmt=pl->fmt;int locked=0;
 /* v10.317: a padded level's image is the source's size (sw x sh), the copy's the power of two around it */
 GLuint iw=pl->padS==3?lw/2:plain(pl)?lw:(l?lw:pl->sw),ih=pl->padT==3?lh/2:plain(pl)?lh:(l?lh:pl->sh); /* v10.353: a mirrored side's source half the copy's */
 if(!li||(!li->FetchTexelc&&!pl->dmode)||(pl->dmode&&!li->FetchTexelf)||li->Width!=iw||li->Height!=ih||(pl->vol&&li->Depth!=pl->sd)){static int n;if(n++<16)vxgl_log("texture %u level %u missing\n",t->Name,l);
  x->failed=GL_TRUE;x->fail_levels=levels;x->fail_format=fmt;x->fail_mem=GL_FALSE;return 0;} /* v10.287: until the texture changes */
 {VXGL_PROF_BEGIN;if(fill_direct)p=vxgl_texmem_direct(m,l,&pitch);if(!p){p=vxgl_texmem_lock(m,l,&pitch);locked=1;}VXGL_PROF_END(10);}
 if(!p){x->failed=GL_TRUE;x->fail_levels=levels;x->fail_format=fmt;x->fail_mem=GL_TRUE;x->fail_freed=vxgl_texmem_freed;return 0;} /* v10.288: kept as a failed allocation (it was tried at every state change) */
 /* v10.302 (review): Mesa's RGBA, RGB and ARGB8888 images read directly (R G B A bytes, R G B bytes, or the copy's own layout) instead of a
  * FetchTexelc call a texel, the most of a state translation's time in VXGL.LOG (a glTexSubImage'd texture is copied whole each change).
  * Mesa keeps its own formats so that its software triangles keep their fast textured paths. */
 {VXGL_PROF_BEGIN;int r=1;
  if(pl->vol){GLuint k;for(k=0;k<pl->sd&&r;k++)r=fill_conv(li,k,p+k*m->slice,pitch,lw,lh,pl);} /* v10.317: the slices, one after another */
  else if(plain(pl))r=fill_copy(li,p,pitch,lw,lh,fmt);
  else r=fill_conv(li,0,p,pitch,lw,lh,pl);
  VXGL_PROF_END(11);
  if(locked){VXGL_PROF_BEGIN;vxgl_texmem_unlock(m,l);VXGL_PROF_END(12);}
  return r;}
}
/* v10.366: an R5G6B5 copy (ACCEL_OPENGL_TEX16): each channel rounded to its bits (v x 31 / 255 and v x 63 / 255 to the nearest, by table);
 * Mesa's RGB, RGBA, ARGB8888 and RGB565 images read directly, any other by FetchTexelc */
static uint16_t t565r[256],t565g[256],t565b[256];
static void t565_init(void) {int v;if(t565g[255])return;for(v=0;v<256;v++){t565r[v]=(uint16_t)(((v*249+1014)>>11)<<11);t565g[v]=(uint16_t)(((v*253+505)>>10)<<5);t565b[v]=(uint16_t)((v*249+1014)>>11);}}
static int fill_565(const struct gl_texture_image *li,GLubyte *p,uint32_t pitch,uint32_t lw,uint32_t lh) {
 uint32_t i,j;t565_init();
 if(pitch<lw*2)return 0;
 if(li->Data&&li->TexFormat&&li->RowStride>=lw&&!li->IsCompressed) {int mf=li->TexFormat->MesaFormat;
  if(mf==MESA_FORMAT_RGB565){for(j=0;j<lh;j++)memcpy(p+j*pitch,(const GLubyte*)li->Data+(size_t)j*li->RowStride*2,lw*2);vxgl_prof_texels[0]+=(double)lw*lh;return 1;}
  if(mf==MESA_FORMAT_RGB||mf==MESA_FORMAT_RGBA||mf==MESA_FORMAT_ARGB8888){int n=mf==MESA_FORMAT_RGB?3:4,ri=mf==MESA_FORMAT_ARGB8888?2:0,bi=mf==MESA_FORMAT_ARGB8888?0:2;
   for(j=0;j<lh;j++){uint16_t *d=(uint16_t*)(p+j*pitch);const GLubyte *sp=(const GLubyte*)li->Data+(size_t)j*li->RowStride*n;
    if(n==3)for(i=0;i<lw;i++,sp+=3)d[i]=(uint16_t)(t565r[sp[0]]|t565g[sp[1]]|t565b[sp[2]]);
    else for(i=0;i<lw;i++,sp+=4)d[i]=(uint16_t)(t565r[sp[ri]]|t565g[sp[1]]|t565b[sp[bi]]);}
   vxgl_prof_texels[0]+=(double)lw*lh;return 1;}}
 if(!li->FetchTexelc)return 0;
 for(j=0;j<lh;j++){uint16_t *d=(uint16_t*)(p+j*pitch);
  for(i=0;i<lw;i++){GLchan tx[4];li->FetchTexelc(li,(GLint)i,(GLint)j,0,tx);d[i]=(uint16_t)(t565r[tx[0]]|t565g[tx[1]]|t565b[tx[2]]);}}
 vxgl_prof_texels[1]+=(double)lw*lh;
 return 1;
}
/* v10.303: the copy itself (fill_level), timed for VXGL.LOG */
static int fill_copy(const struct gl_texture_image *li,GLubyte *p,uint32_t pitch,uint32_t lw,uint32_t lh,GLuint fmt) {
 uint32_t i,j;
 /* v10.307: a DXT copy takes Mesa's blocks as they are: packed block rows (the HAL spreads narrow ones to the sampler's 32 bytes) */
 if(fmt>=VX3DN_TEX_DXT1&&fmt<=VX3DN_TEX_DXT5){uint32_t bs=fmt==VX3DN_TEX_DXT1?8:16,row=(lw<4?1:lw/4)*bs,rows=lh<4?1:lh/4;
  if(!li->Data||pitch<row*rows)return 0;
  memcpy(p,li->Data,(size_t)row*rows);vxgl_prof_texels[0]+=(double)lw*lh;return 1;}
 if(fmt==VX3DN_TEX_RGB565)return fill_565(li,p,pitch,lw,lh); /* v10.366 */
 if(li->Data&&li->TexFormat&&li->RowStride>=lw&&!li->IsCompressed) {int mf=li->TexFormat->MesaFormat;
  if(mf==MESA_FORMAT_ARGB8888){for(j=0;j<lh;j++)memcpy(p+j*pitch,(const GLubyte*)li->Data+(size_t)j*li->RowStride*4,lw*4);vxgl_prof_texels[0]+=(double)lw*lh;return 1;}
  if(mf==MESA_FORMAT_RGBA||mf==MESA_FORMAT_RGB){int n=mf==MESA_FORMAT_RGBA?4:3;
   for(j=0;j<lh;j++){uint32_t *d=(uint32_t*)(p+j*pitch);const GLubyte *sp=(const GLubyte*)li->Data+(size_t)j*li->RowStride*n;
    if(n==4)for(i=0;i<lw;i++,sp+=4){uint32_t v=*(const uint32_t*)sp;d[i]=(v&0xFF00FF00UL)|((v&0xFFUL)<<16)|((v>>16)&0xFFUL);} /* v10.309 (review): R G B A as one dword, R and B swapped */
    else for(i=0;i<lw;i++,sp+=3)d[i]=0xFF000000UL|((uint32_t)sp[0]<<16)|((uint32_t)sp[1]<<8)|sp[2];}
   vxgl_prof_texels[0]+=(double)lw*lh;return 1;}}
 for(j=0;j<lh;j++){uint32_t *d=(uint32_t*)(p+j*pitch);
  for(i=0;i<lw;i++){GLchan tx[4];li->FetchTexelc(li,(GLint)i,(GLint)j,0,tx);d[i]=((uint32_t)tx[3]<<24)|((uint32_t)tx[0]<<16)|((uint32_t)tx[1]<<8)|tx[2];}}
 vxgl_prof_texels[1]+=(double)lw*lh;
 return 1;
}
/* v10.317: row j of slice k of the source image (its width) as the copy's A R G B words: a depth texture's depth as its DEPTH_TEXTURE_MODE
 * (as swrast's sample_depth_texture with no compare: the depth converted to 8 bits), the unit's texture colour table applied to each texel
 * (as swrast applies it to the sampled texel: the same for nearest filters, the only ones it is taken with) */
static GLchan conv_c[2048][4];static uint32_t conv_src[2048],conv_row[2048];
static void src_row(const struct gl_texture_image *li,GLuint j,GLuint k,GLuint n,uint32_t *out,const vxgl_plan *pl) {
 GLuint i;
 if(pl->dmode){for(i=0;i<n;i++){GLfloat d=0.0F;GLchan v;li->FetchTexelf(li,(GLint)i,(GLint)j,(GLint)k,&d);CLAMPED_FLOAT_TO_CHAN(v,d);
   out[i]=pl->dmode==GL_ALPHA?((uint32_t)v<<24):pl->dmode==GL_INTENSITY?((uint32_t)v*0x01010101UL):(0xFF000000UL|((uint32_t)v*0x010101UL));}
  vxgl_prof_texels[1]+=(double)n;return;}
 for(i=0;i<n;i++)li->FetchTexelc(li,(GLint)i,(GLint)j,(GLint)k,conv_c[i]);
 if(pl->tab)_mesa_lookup_rgba_ubyte(pl->tab,n,conv_c);
 for(i=0;i<n;i++)out[i]=((uint32_t)conv_c[i][3]<<24)|((uint32_t)conv_c[i][0]<<16)|((uint32_t)conv_c[i][1]<<8)|conv_c[i][2];
 vxgl_prof_texels[1]+=(double)n;
}
/* v10.317: copy texel i of a padded side of n source texels comes from source texel (the result), or is the border colour (-1) */
static GLint pad_map(int pad,GLuint i,GLuint n) {
 if(pad==3)return i<n?(GLint)(n-1-i):(GLint)(i-n); /* v10.353: mirrored */
 if(pad==2){GLint s=(GLint)i-1;return (s<0||s>=(GLint)n)?-1:s;}
 if(pad==1&&i>=n)return (GLint)n-1;
 return (GLint)i;
}
/* v10.317: one level (or slice) of a padded or converted copy, cw x chh texels (the source is the image's own size) */
static int fill_conv(const struct gl_texture_image *li,GLuint k,GLubyte *p,uint32_t pitch,uint32_t cw,uint32_t chh,const vxgl_plan *pl) {
 GLuint sw=li->Width,sh=li->Height,jd,id;GLint last=-2;
 if(sw>2048||cw>2048||pitch<cw*4)return 0;
 for(jd=0;jd<chh;jd++){GLint js=pad_map(pl->padT,jd,sh);uint32_t *d=(uint32_t*)(p+jd*pitch);
  if(js<0){for(id=0;id<cw;id++)d[id]=pl->ring;last=-2;continue;}
  if(js!=last){src_row(li,(GLuint)js,k,sw,conv_src,pl);last=js;
   for(id=0;id<cw;id++){GLint is=pad_map(pl->padS,id,sw);conv_row[id]=is<0?pl->ring:conv_src[is];}}
  memcpy(d,conv_row,cw*4);}
 return 1;
}
static void free_copy(vxgl_tex *x) {int f;spare_free(x);vxgl_texmem_free(&x->m);for(f=0;f<5;f++)if(x->cf[f].levels)vxgl_texmem_free(&x->cf[f]);memset(x->cf,0,sizeof(x->cf));}
/* v10.317: the memory the plan needs: a block for a cube map with mipmaps and for a 3D texture, the HAL's chain otherwise */
static void texmem_shape(vxgl_texmem *m,const vxgl_plan *pl) {
 memset(m,0,sizeof(*m));m->w=pl->w;m->h=pl->h;m->levels=pl->levels;m->format=pl->fmt;
 if((pl->cube&&pl->levels>1)||pl->vol||pl->npot){m->depth=pl->vol?pl->sd:1;m->block=vxgl_texmem_layout(m);} /* v10.321: an NPOT copy as a block (its pitch ours) */
}
static int shape_same(const vxgl_tex *x,const vxgl_plan *pl) {
 return x->w==pl->w&&x->h==pl->h&&x->levels==pl->levels&&x->format==pl->fmt&&x->cube==pl->cube&&x->vol==pl->vol&&
  (x->m.block!=0)==((pl->cube&&pl->levels>1)||pl->vol||pl->npot)&&(!pl->vol||x->m.depth==pl->sd);
}
/* v10.366: ACCEL_OPENGL_TEXRING: the copy to write this upload into without waiting, made the texture's copy (m) in place of the one the
 * queued and drawing packets read, which goes into the ring until they are finished (the packets handed over now: the core's batch is sent
 * into the queue first, made with the old copy's offsets). 0: none (the caller sends and waits as before) */
static uint32_t copy_bytes(const vxgl_texmem *m) {GLuint l;uint32_t b=0;for(l=0;l<m->levels;l++){uint32_t lh=m->h>>l;if(!lh)lh=1;b+=m->pitch[l]*lh;}return b;}
static int ring_free_at(const vxgl_tex *x,int k) {return x->spare[k].levels&&(int32_t)(vxgl_seq_done-x->spare_need[k])>=0&&!vxgl_texmem_lost((vxgl_texmem*)&x->spare[k]);}
static int ring_take(vxgl_tex *x,const vxgl_plan *pl) {
 int k,n=vxgl_gl_texring>3?3:vxgl_gl_texring,pick=-1,old=-1;uint32_t need;vxgl_texmem tmp;
 if(n<=0||x->no_ring||x->vnew||pl->cube||pl->vol||x->m.block||(pl->fmt>=VX3DN_TEX_DXT1&&pl->fmt<=VX3DN_TEX_DXT5)||copy_bytes(&x->m)>256UL*1024UL)return 0;
 vxgl_hw_flush_all(); /* the draws made with the copy so far into the queue (under the state they were made with: c->xlate_old) */
 need=vxgl_seq_q;
 for(k=0;k<n;k++)if(ring_free_at(x,k)){if(pick<0||(int32_t)(x->spare_need[pick]-x->spare_need[k])>0)pick=k;}
 if(pick>=0)vxgl_ring_stat[0]++;
 else for(k=0;k<n;k++)if(!x->spare[k].levels){vxgl_texmem *m=&x->spare[k];unsigned long fr=vxgl_texmem_freed;
  texmem_shape(m,pl);if(vxgl_texmem_alloc(m)){pick=k;vxgl_ring_stat[2]++;}else{memset(m,0,sizeof(*m));vxgl_texmem_freed=fr;}break;} /* new memory: nothing reads it */
 if(pick<0){vxgl_batch_flush(); /* every copy still read: the queue sent (no wait); the call retires what the one before left drawing */
  for(k=0;k<n;k++)if(x->spare[k].levels&&(old<0||(int32_t)(x->spare_need[old]-x->spare_need[k])>0))old=k;
  if(old>=0&&ring_free_at(x,old)){pick=old;vxgl_ring_stat[1]++;}}
 if(pick<0){vxgl_ring_stat[3]++;return 0;}
 tmp=x->m;x->m=x->spare[pick];x->spare[pick]=tmp;x->spare_need[pick]=need;
 return 1;
}
static int upload_(vxgl_ctx *c,struct gl_texture_object *t,vxgl_tex *x,const vxgl_plan *pl) {
 GLuint w=pl->w,h=pl->h,l,levels=pl->levels,fmt=pl->fmt;int cube=pl->cube,nf=cube?6:1,f,fresh=0;
 if(x->vnew&&!tex_sync_(&c->gl,t,x))return 0; /* v10.321: the copy is made from the images: what was drawn into it read back first; v10.329: not made from images still stale (software this time, tried again at the next) */
 if(x->m.levels&&!shape_same(x,pl)){vxgl_hw_flush_all();vxgl_batch_flush();free_copy(x);}
 if(!x->m.levels) {
  int ok;fresh=1;
  texmem_shape(&x->m,pl);memset(x->cf,0,sizeof(x->cf));
  {VXGL_PROF_BEGIN;ok=vxgl_texmem_alloc(&x->m);
  for(f=1;ok&&f<nf;f++){vxgl_texmem *m=&x->cf[f-1];texmem_shape(m,pl);if(!vxgl_texmem_alloc(m)){memset(m,0,sizeof(*m));ok=0;}} /* v10.295: the other five faces */
  VXGL_PROF_END(8);}
  /* v10.296 (review): the faces made before the one that failed are given back without counting as video memory freed (as alloc_undo
   * does for one copy): two cube maps that do not fit made each other try again at every bind, three allocations a draw */
  if(!ok){unsigned long fr=vxgl_texmem_freed;if(x->m.levels)free_copy(x);vxgl_texmem_freed=fr;memset(&x->m,0,sizeof(x->m));
   /* v10.287 (review): the failure is kept (prepare) until the texture, what it needs, or the video memory free changes; it was tried
    * again at every state change (dirty stayed set, which passed over the guard): a CreateSurface each time */
   x->failed=GL_TRUE;x->fail_levels=levels;x->fail_format=fmt;x->fail_mem=GL_TRUE;x->fail_freed=vxgl_texmem_freed;
   {static int n;if(n++<8)vxgl_log("texture %u %ux%u, %u levels%s: no video memory copy (software)\n",t->Name,w,h,levels,cube?", cube":pl->vol?", 3D":"");}return 0;}
 }
 x->w=w;x->h=h;x->levels=levels;x->format=fmt;x->base=pl->base;x->cube=cube;x->vol=pl->vol;
 x->sw=pl->sw;x->sh=pl->sh;x->sd=pl->sd;x->padS=pl->padS;x->padT=pl->padT;x->ring=pl->ring;x->dmode=pl->dmode;x->ctab=pl->ctab;
 fill_direct=0;
 if(!fresh){VXGL_PROF_BEGIN;vx_d3d_cut_tag=9;if(ring_take(x,pl)){fill_direct=1;c->tex_changed=1;}else{vxgl_hw_flush_all();vxgl_batch_flush();}VXGL_PROF_END(9);} /* v10.366: or a copy nothing reads (ring_take) */ /* the batch may still sample the old copy; the lock waits for the engine; v10.275: and the queue; v10.302 (review): not for a copy made just now (nothing drawn can read it; freed memory was sent before it was given back) */
 for(f=0;f<nf;f++)for(l=0;l<levels;l++){uint32_t lw=w>>l,lh=h>>l;
  if(!lw)lw=1;if(!lh)lh=1;
  if(!fill_level(x,f?&x->cf[f-1]:&x->m,l,t->Image[f][pl->base+l],lw,lh,pl,t)){fill_direct=0;return 0;}}
 fill_direct=0;
 x->dirty=0;x->failed=GL_FALSE;x->pal_gen=pal_gen;x->pal_shared=c->gl.Texture.SharedPalette;x->pal_src=c->gl.Texture.SharedPalette?(const void*)&c->gl.Texture.Palette:(const void*)&t->Palette;table_keep(x,pl->tab);
 c->tex_changed=1;
 return 1;
}
/* the unit's texture as an engine copy: its handle, or 0 (software) */
#define PREPSW do{static int n_;if(n_++<32)vxgl_log("texture copy to software (vxgl_tex.c line %d)\n",__LINE__);return 0;}while(0)
/* v10.353: MIRROR_CLAMP_EXT and MIRROR_CLAMP_TO_EDGE_EXT (ATI's MIRROR_CLAMP / MIRROR_ONCE the same enums) from a copy twice as wide on that
 * side, the mirror image before the texels (copy texel i: source n - 1 - i below n, i - n from n), clamped to its edge, the coordinate
 * s' = (s + 1) / 2: |s| clamped to 1 is what the copy gives (the mirror at 0 is the seam, the edges at -1 and 1 the copy's), linear in s
 * (perspective and filters as they are; each level the source's level doubled, the LOD the same). A power-of-two side, 1024 at most, not
 * a rectangle, cube map or volume. MIRROR_CLAMP_EXT filtered linearly takes in the border at |s| = 1: as GL_CLAMP (ACCEL_OPENGL_CLAMP).
 * ACCEL_OPENGL_MIRRORCLAMP = 0: software */
int vxgl_gl_mirrorclamp=1;
static int wrap_mc(GLenum w) {return vxgl_gl_mirrorclamp&&(w==GL_MIRROR_CLAMP_EXT||w==GL_MIRROR_CLAMP_TO_EDGE_EXT);}
static int wrap_ok(GLenum w) {return w==GL_REPEAT||w==GL_MIRRORED_REPEAT||w==GL_CLAMP||w==GL_CLAMP_TO_EDGE||w==GL_CLAMP_TO_BORDER||wrap_mc(w);}
static int wrap_clamps(GLenum w) {return w==GL_CLAMP||w==GL_CLAMP_TO_EDGE||w==GL_CLAMP_TO_BORDER;}
static int linear_used(const struct gl_texture_object *t,int mm) {
 int mag=t->MagFilter==GL_LINEAR,min=t->MinFilter==GL_LINEAR||t->MinFilter==GL_LINEAR_MIPMAP_NEAREST||t->MinFilter==GL_LINEAR_MIPMAP_LINEAR;
 return mm==1?min:mm==2?mag:(mag||min);
}
/* v10.317: the LOD clamps (GL 1.2: the LOD clamped to [MIN_LOD, MAX_LOD] before the choice of magnification and of a level; swrast's
 * nearest_mipmap_level and linear_mipmap_level) as the copy's first level (*first, from BaseLevel), its last (*lastl, of last) and the filter
 * used throughout (*mm: 1 always minified: MIN_LOD above 0; 2 always magnified: MAX_LOD at or below 0). A clamp between levels with a
 * filter that blends levels stays in software (0). */
/* v10.330 (review #15): swrast's minification starts above c, 0.5 for a GL_LINEAR magnification with a NEAREST_MIPMAP minification
 * (s_texfilter.c compute_min_mag_ranges), else 0; a clamped range is magnified up to c and minified above it, and one that starts in
 * (0, c] and goes past c is left to software (the engine changes filter at 0). (review #12): a NaN range is software, and a LOD at or
 * past the last level is that level, found in float before any conversion to an integer (1e30 overflowed the int). */
static int lod_plan(const struct gl_texture_object *t,int mip,GLint last,GLint *first,GLint *lastl,int *mm) {
 GLfloat lo=t->MinLod,hi=t->MaxLod,c;int nearest=t->MinFilter==GL_NEAREST_MIPMAP_NEAREST||t->MinFilter==GL_LINEAR_MIPMAP_NEAREST;
 *first=0;*lastl=last;*mm=0;
 if(lo==-1000.0F&&hi==1000.0F)return 1;
 if(!vxgl_gl_lodclamp||lo!=lo||hi!=hi||lo>hi)return 0;
 c=t->MagFilter==GL_LINEAR&&(t->MinFilter==GL_NEAREST_MIPMAP_NEAREST||t->MinFilter==GL_NEAREST_MIPMAP_LINEAR)?0.5F:0.0F;
 if(hi<=c){*mm=2;*lastl=0;return 1;} /* always magnified: the base level */
 if(lo>0.0F&&lo<=c)return 0; /* magnified from lo to c, minified above: swrast's threshold, not the engine's */
 if(lo>0.0F)*mm=1; /* always minified */
 if(!mip)return 1;
 if(lo>0.0F){GLint k;
  if(lo>=(GLfloat)last)k=last;
  else if(nearest)k=lo<=0.5F?0:(GLint)(lo+0.5F); /* the level a LOD of MIN_LOD picks: none nearer is reached */
  else {if(lo!=(GLfloat)(GLint)lo)return 0;k=(GLint)lo;}
  *first=k>last?last:k;}
 if(hi<(GLfloat)last){GLint k;
  if(nearest)k=hi<=0.5F?0:(GLint)(hi+0.5F); /* the farthest level a LOD of MAX_LOD picks */
  else {if(hi!=(GLfloat)(GLint)hi)return 0;k=(GLint)hi;}
  if(k<*lastl)*lastl=k;}
 if(*lastl<*first)*lastl=*first;
 return 1;
}
/* v10.317: a texture colour table's checksum (its size, format and entries): a copy made with another is made again */
static GLuint table_bytes(const struct gl_color_table *ct) {GLint comps=_mesa_components_in_format(ct->_BaseFormat);return ct->TableUB?ct->Size*(GLuint)(comps>0?comps:0):0;}
/* v10.330 (review #10): the copy's table is the plan's entry for entry (none: none) */
static int table_same(const vxgl_tex *x,const struct gl_color_table *ct) {
 GLuint n;if(!ct)return x->tabn==0&&!x->tabcopy;
 n=table_bytes(ct);return x->tabcopy&&x->tabn==n&&x->tabfmt==ct->_BaseFormat&&(!n||!memcmp(x->tabcopy,ct->TableUB,n));
}
static void table_keep(vxgl_tex *x,const struct gl_color_table *ct) {
 GLuint n=ct?table_bytes(ct):0;
 if(x->tabcopy&&(!ct||x->tabn!=n)){_mesa_free(x->tabcopy);x->tabcopy=0;x->tabn=0;}
 if(!ct)return;
 if(!x->tabcopy)x->tabcopy=(GLubyte*)_mesa_malloc(n?n:1);
 if(x->tabcopy){if(n)memcpy(x->tabcopy,ct->TableUB,n);x->tabn=n;x->tabfmt=ct->_BaseFormat;}
}
static GLuint table_sum(const struct gl_color_table *ct) {
 GLint comps=_mesa_components_in_format(ct->_BaseFormat);GLuint s=ct->Size*2654435761U+(GLuint)ct->_BaseFormat,i,n=ct->Size*(GLuint)(comps>0?comps:0);
 if(ct->TableUB)for(i=0;i<n;i++)s=s*31U+ct->TableUB[i];
 return s|1U;
}
/* v10.321: each component of a 256-entry table a line through its ends within half a step (an exact lookup of a filtered texel) */
static int table_affine(const struct gl_color_table *ct) {
 GLint comps=_mesa_components_in_format(ct->_BaseFormat),k,i;
 if(ct->Size!=256||!ct->TableUB||comps<1||comps>4)return 0;
 for(k=0;k<comps;k++){GLfloat a=((GLfloat)ct->TableUB[255*comps+k]-(GLfloat)ct->TableUB[k])/255.0F,b=(GLfloat)ct->TableUB[k];
  for(i=0;i<256;i++){GLfloat d=(GLfloat)ct->TableUB[i*comps+k]-(a*(GLfloat)i+b);if(d>0.51F||d<-0.51F)return 0;}}
 return 1;
}
static GLuint prepare(vxgl_ctx *c,int unit,int stage,GLenum *base,vxgl_plan *pl) {
 GLcontext *ctx=&c->gl;struct gl_texture_unit *u=&ctx->Texture.Unit[unit];struct gl_texture_object *t=u->_Current;
 const struct gl_texture_image *ti;vxgl_tex *x;GLuint levels,fmt;GLbitfield en=u->_ReallyEnabled;
 int cube=(en&TEXTURE_CUBE_BIT)!=0,rect=(en&TEXTURE_RECT_BIT)!=0,vol=(en&TEXTURE_3D_BIT)!=0,one=(en&TEXTURE_1D_BIT)!=0,depth,mip,ringed=0;
 GLint first=0,lastl=0,last;
 memset(pl,0,sizeof(*pl));
 if(!t||!(en&(TEXTURE_1D_BIT|TEXTURE_2D_BIT|TEXTURE_CUBE_BIT|TEXTURE_RECT_BIT|TEXTURE_3D_BIT))||!t->Complete)PREPSW;
 if((rect&&!vxgl_gl_npot)||(vol&&!vxgl_gl_tex3d))PREPSW;
 if(t->BaseLevel<0||t->BaseLevel>=MAX_TEXTURE_LEVELS||!(ti=t->Image[0][t->BaseLevel]))PREPSW;
 if(ti->Border||!ti->Width||!ti->Height||ti->Width>2048||ti->Height>2048)PREPSW;
 depth=ti->_BaseFormat==GL_DEPTH_COMPONENT||ti->_BaseFormat==GL_DEPTH_STENCIL_EXT;
 if(!wrap_ok(t->WrapS)||(!one&&!wrap_ok(t->WrapT))||(vol&&!wrap_ok(t->WrapR)))PREPSW;
 if((wrap_mc(t->WrapS)||(!one&&wrap_mc(t->WrapT))||(vol&&wrap_mc(t->WrapR)))&&(rect||cube||vol))PREPSW; /* v10.353 */ /* v10.309 (review): MIRROR_CLAMP_EXT, MIRROR_CLAMP_TO_EDGE_EXT, ATI's MIRROR_ONCE drew as REPEAT */
 /* v10.317: a depth texture without a compare (swrast's sample_depth_texture: the base level, the magnification filter throughout, the
  * depth as luminance, intensity or alpha; ACCEL_OPENGL_DEPTHTEX); a compare stays in software (and a compare on other textures is ignored
  * by swrast, as here) */
 if(depth){if(!vxgl_gl_depthtex||cube||vol||t->CompareFlag||t->CompareMode!=GL_NONE)PREPSW;
  if(t->DepthMode!=GL_LUMINANCE&&t->DepthMode!=GL_INTENSITY&&t->DepthMode!=GL_ALPHA)PREPSW;pl->dmode=t->DepthMode;}
 mip=mipmapped(t->MinFilter)&&!depth;
 /* v10.317: the texture colour table (SGI_texture_color_table) at the copy: swrast applies it to the sampled texel, which with nearest
  * filters is one texel of one level (ACCEL_OPENGL_CTABLE) */
 /* v10.321: with other filters too when the table is affine in each of its components and 256 long (index = the texel's byte): a filtered
  * value's lookup then equals the filtered lookups (filtering is a weighted mean, weights summing to 1), as swrast looks up after filtering */
 if(u->ColorTableEnabled){
  if(!vxgl_gl_ctable||!u->ColorTable.Size||!u->ColorTable.TableUB)PREPSW;
  if((t->MagFilter!=GL_NEAREST||(t->MinFilter!=GL_NEAREST&&t->MinFilter!=GL_NEAREST_MIPMAP_NEAREST))&&!table_affine(&u->ColorTable))PREPSW;
  pl->tab=&u->ColorTable;pl->ctab=table_sum(&u->ColorTable);}
 /* v10.315 / v10.317: LOD clamps (lod_plan); a depth texture has no levels (swrast: the base level whatever the LOD) */
 last=t->_MaxLevel-t->BaseLevel;if(last<0)last=0;
 if(!depth&&!lod_plan(t,mip,last,&first,&lastl,&pl->mm))PREPSW;
 if(!mip){first=0;lastl=0;}
 pl->base=(GLuint)(t->BaseLevel+first);
 if(pl->base>=MAX_TEXTURE_LEVELS||!(ti=t->Image[0][pl->base]))PREPSW;
 pl->sw=ti->Width;pl->sh=ti->Height;pl->sd=vol?(GLuint)ti->Depth:1;
 /* v10.295: a cube map on the only unit (the core's T150 path): six square faces of one size; v10.317: with its mip chain (ACCEL_OPENGL_CUBEMIP:
  * each face a block of one layout, the engine's base mode 1 taking the level offsets from each face's base); a face's compressed image decoded */
 if(cube){int f;GLint l;if(!vxgl_gl_cube||(stage>0&&(!vxgl_gl_unit3d||stage>1))||(mip&&!vxgl_gl_cubemip)||ti->Width!=ti->Height||!pow2(ti->Width))PREPSW; /* v10.321: stage 1 too */
  for(f=1;f<6;f++)for(l=first;l<=lastl;l++){const struct gl_texture_image *fi=t->Image[f][t->BaseLevel+l],*f0=t->Image[0][t->BaseLevel+l];
   if(!fi||!f0||fi->Border||fi->Width!=f0->Width||fi->Height!=f0->Height||fi->_BaseFormat!=ti->_BaseFormat)PREPSW;}}
 /* v10.317: a 3D texture on the only unit (the engine's volume, layout 3): power-of-two sides, no mip chain (the engine has none for volumes),
  * depth up to 512 (ACCEL_OPENGL_TEX3D) */
 /* v10.321: stage 1 too (ACCEL_OPENGL_UNIT3D); a mipmapped filter on a chain of one level (MAX_LEVEL at the base) is that level's (swrast
  * clamps the LOD to it). The depth is never above 256: Mesa's MAX_3D_TEXTURE_LEVELS (9) */
 if(vol&&((stage>0&&(!vxgl_gl_unit3d||stage>1))||(mip&&lastl>first)||!pow2(pl->sw)||!pow2(pl->sh)||!pow2(pl->sd)))PREPSW;
 /* v10.317: the copy's size and padding. A side that is not a power of two (or a rectangle texture's: its coordinates in texels) is drawn from
  * a power-of-two copy: with GL_CLAMP or CLAMP_TO_EDGE its last texel repeated to the copy's edge, with CLAMP_TO_BORDER a ring of the border
  * colour around the texels (the engine clamping to it); CLAMP_TO_BORDER filtered linearly gets the ring on a power-of-two side too (the
  * engine's border colour takes a whole filtered sample, HCM p126). No mip chain (its levels are not halves of the copy's), REPEAT and
  * MIRRORED_REPEAT only on sides that need nothing (ACCEL_OPENGL_NPOT). */
 {int needS=!pow2(pl->sw)||rect,needT=!one&&(!pow2(pl->sh)||rect);
  int bS=t->WrapS==GL_CLAMP_TO_BORDER,bT=!one&&t->WrapT==GL_CLAMP_TO_BORDER,lin=linear_used(t,pl->mm);
  /* v10.357: GL_CLAMP filtered linearly, exact (ACCEL_OPENGL_CLAMP = 0, the default): GL clamps s to 0..1 and the filter then takes in the
   * border colour at the edges (half of it at s = 0 and 1). A ring of the border colour around the texels, clamped to the ring, is the same
   * filter for every s in 0..1 (the engine at s' = (s x sw + 1) / w blends the ring and the edge texel by the same weights); below 0 or above
   * 1 GL stays at half the border where the ring goes on to all of it, so a draw whose coordinates leave 0..1 on that side goes to software
   * (vxgl_tex_vertices_ok, per draw). Not a rectangle, cube map or volume, nor mipmapped (a ring copy has no chain) */
  int gS=lin&&t->WrapS==GL_CLAMP&&!rect&&!cube&&!vol,gT=lin&&!one&&t->WrapT==GL_CLAMP&&!rect&&!cube&&!vol;
  if(gS||gT){pl->cx=(gS?1:0)|(gT?2:0);bS|=gS;bT|=gT;}
  pl->padS=!pow2(pl->sw)?1:0;pl->padT=!one&&!pow2(pl->sh)?1:0;
  /* v10.321: a side not a power of two that repeats or mirrors, or a mip chain on one: the copy at the texture's own size, its levels
   * halved and rounded down as GL's (the sampler's NPOT mode, 02/22 bit 15 clear, HCM p118; level l = HTXSnL0W >> l), rows aligned to
   * 32 bytes (ACCEL_OPENGL_NPOTWRAP). Not a cube map or a volume (bit 15 set), nor a border linearly filtered */
  if((pl->padS||pl->padT)&&!rect&&!cube&&!vol&&vxgl_gl_npot&&vxgl_gl_npotwrap&&
     (mip||(pl->padS&&!wrap_clamps(t->WrapS))||(pl->padT&&!wrap_clamps(t->WrapT)))&&!((bS||bT)&&lin)){pl->padS=pl->padT=0;pl->npot=1;needS=needT=0;}
  if(bS&&(pl->padS||lin||rect))pl->padS=2;
  if(bT&&(pl->padT||lin||rect))pl->padT=2;
  if(pl->padS||pl->padT||rect){
   if(!vxgl_gl_npot||mip||cube||vol)PREPSW;
   if((needS||pl->padS)&&!wrap_clamps(t->WrapS))PREPSW;
   if((needT||pl->padT)&&!wrap_clamps(t->WrapT))PREPSW;}
  ringed=pl->padS==2||pl->padT==2;
  pl->w=pl->padS?pow2up(pl->sw+(pl->padS==2?2:0)):pl->sw;pl->h=pl->padT?pow2up(pl->sh+(pl->padT==2?2:0)):pl->sh;
  if(pl->w>2048||pl->h>2048)PREPSW;
  if(ringed){GLchan *b=t->_BorderChan;pl->ring=((GLuint)b[3]<<24)|((GLuint)b[0]<<16)|((GLuint)b[1]<<8)|b[2];}
  pl->xf=pl->padS||pl->padT||rect;
  /* s' = (s x sw + ring) / w; a rectangle texture's s is in texels already */
  pl->sa=(rect?1.0F:(GLfloat)pl->sw)/(GLfloat)pl->w;pl->sb=(pl->padS==2?1.0F:0.0F)/(GLfloat)pl->w;
  pl->ta=one?1.0F:(rect?1.0F:(GLfloat)pl->sh)/(GLfloat)pl->h;pl->tb=one?0.0F:(pl->padT==2?1.0F:0.0F)/(GLfloat)pl->h;}
 /* v10.353: a mirror-clamped side (wrap_mc): the mirrored copy twice as wide, s' = (s + 1) / 2 */
 {int mS=wrap_mc(t->WrapS),mT=!one&&wrap_mc(t->WrapT);
  if(mS){if(pl->padS||pl->npot||!pow2(pl->sw)||pl->sw>1024)PREPSW;pl->padS=3;pl->w=pl->sw*2;pl->xf=1;pl->sa=0.5F;pl->sb=0.5F;}
  if(mT){if(pl->padT||pl->npot||!pow2(pl->sh)||pl->sh>1024)PREPSW;pl->padT=3;pl->h=pl->sh*2;pl->xf=1;pl->ta=0.5F;pl->tb=0.5F;}}
 /* v10.304: CLAMP_TO_BORDER on the engine (its address code 0 and border colour, HCM pp. 125-126) with NEAREST filters: the manual
  * says a filtered sample with any border texel in it is the border colour, where OpenGL blends; v10.317: linear filters take the ring above,
  * so only a mipmapped texture filtered linearly with a border stays in software */
 if(((t->WrapS==GL_CLAMP_TO_BORDER&&pl->padS!=2)||(!one&&t->WrapT==GL_CLAMP_TO_BORDER&&pl->padT!=2)||(vol&&t->WrapR==GL_CLAMP_TO_BORDER))&&
    (!vxgl_gl_border||!(vx_d3d_feat&VX_F_BORDER)||linear_used(t,pl->mm)))PREPSW;
 /* GL_CLAMP filtered linearly takes in the border colour at the edges; the engine clamps to the edge texel (as the cards of the time
  * did, and games expect): such textures go to software, which does it exactly */
 if(((t->WrapS==GL_CLAMP&&!(pl->cx&1))||t->WrapS==GL_MIRROR_CLAMP_EXT||(!one&&((t->WrapT==GL_CLAMP&&!(pl->cx&2))||t->WrapT==GL_MIRROR_CLAMP_EXT))||(vol&&t->WrapR==GL_CLAMP))&&linear_used(t,pl->mm))PREPSW; /* v10.357: the ring copy (pl->cx) is exact */ /* v10.353: MIRROR_CLAMP_EXT as GL_CLAMP */
 /* v10.330 (review #17): a depth texture with a colour table goes to software (the depth mode's conversion returned before the table).
  * (review #13): a border sampled through a colour table too: the copy's texels are looked up, the sampler's border colour and the ring
  * were not. (review #14): a depth texture's border is its red as the depth, converted by the depth mode as its texels are (swrast) */
 {int bord=t->WrapS==GL_CLAMP_TO_BORDER||(!one&&t->WrapT==GL_CLAMP_TO_BORDER)||(vol&&t->WrapR==GL_CLAMP_TO_BORDER);GLubyte b[4];
  UNCLAMPED_FLOAT_TO_UBYTE(b[0],t->BorderColor[0]);UNCLAMPED_FLOAT_TO_UBYTE(b[1],t->BorderColor[1]);UNCLAMPED_FLOAT_TO_UBYTE(b[2],t->BorderColor[2]);UNCLAMPED_FLOAT_TO_UBYTE(b[3],t->BorderColor[3]);
  pl->border=((GLuint)b[3]<<24)|((GLuint)b[0]<<16)|((GLuint)b[1]<<8)|b[2];
  if(depth&&pl->tab)PREPSW;
  if((bord||ringed)&&pl->tab)PREPSW; /* v10.357: a GL_CLAMP ring too */
  if(depth){GLuint d=b[0];pl->border=pl->dmode==GL_ALPHA?d<<24:pl->dmode==GL_INTENSITY?d*0x01010101U:0xFF000000U|d*0x00010101U;if(ringed)pl->ring=pl->border;}}
 *base=base_format(ctx,t,ti);
 switch(*base){case GL_RGB:fmt=VX3DN_TEX_XRGB8888;break;case GL_RGBA:case GL_LUMINANCE:case GL_LUMINANCE_ALPHA:case GL_ALPHA:case GL_INTENSITY:fmt=VX3DN_TEX_ARGB8888;break;default:PREPSW;}
 /* v10.307: S3TC images go to the engine as DXT (the sampler's own formats, proven through Direct3D in v10.199-v10.200) at a quarter or an
  * eighth of the memory, their blocks copied as they are. RGB_DXT1's three-colour blocks read colour 3 as transparent on the sampler where
  * OpenGL makes it opaque black: only the environments that never read the texture's alpha of an RGB texture take it (not GL_COMBINE).
  * v10.317: every other compressed image (FXT1, RGB_DXT1 under GL_COMBINE, a padded or converted copy, a cube's face, ACCEL_OPENGL_S3TC = 0)
  * is decoded into an A8R8G8B8 / X8R8G8B8 copy by Mesa's own texel fetch, as swrast reads it */
 if(vxgl_gl_s3tc&&!cube&&!vol&&plain(pl)&&!pl->npot&&ti->TexFormat&&ti->IsCompressed){int mf=ti->TexFormat->MesaFormat; /* (v10.321: DXT sets the power-of-two mode) */
  if(mf==MESA_FORMAT_RGBA_DXT1)fmt=VX3DN_TEX_DXT1;else if(mf==MESA_FORMAT_RGBA_DXT3)fmt=VX3DN_TEX_DXT3;else if(mf==MESA_FORMAT_RGBA_DXT5)fmt=VX3DN_TEX_DXT5;
  else if(mf==MESA_FORMAT_RGB_DXT1&&ctx->Texture.Unit[unit].EnvMode!=GL_COMBINE)fmt=VX3DN_TEX_DXT1;}
 if(ti->IsCompressed&&fmt<VX3DN_TEX_DXT1&&!ti->FetchTexelc)PREPSW; /* nothing to decode it with */
 if(fmt==VX3DN_TEX_XRGB8888&&vxgl_gl_tex16&&!cube&&!vol&&plain(pl)&&!ti->IsCompressed)fmt=VX3DN_TEX_RGB565; /* v10.366: ACCEL_OPENGL_TEX16 */
 levels=1;
 if(mip){GLuint full=1,d=ti->Width>ti->Height?ti->Width:ti->Height;
  if(stage>0&&!vxgl_gl_unitmip)PREPSW; /* the core's units 1 and 2 have no mip chain; v10.304: they do (ACCEL_OPENGL_UNITMIP) */
  while(d>1){d>>=1;full++;}
  levels=(GLuint)(lastl-first+1);
  /* v10.269: the engine takes the whole chain down to 1x1, its offsets from the core's table of eight (nine levels); a level past the
   * ones given reads offset 0, level 0 (board, VXTEX v1.0 case 6: MAX_LEVEL 1 drew level 0 at LOD 3). A shorter chain (MAX_LEVEL below
   * the last level) or a longer one (above 256) is drawn by software.
   * v10.304: the VxD now sets the sampler's maximum level (30 bits 11:6) to the chain's last, as GL clamps the LOD to MAX_LEVEL: a
   * shorter chain is drawn on the engine (ACCEL_OPENGL_LONGMIP). A longer one too, the engine's last level being 8 (block 10030000
   * holds levels 1-8): a 512 texture's 1x1 level is not reached (its 2x2 is), the only difference from software. */
  if(levels<1||levels>full||levels>VXGL_TEXLEVELS||(!vxgl_gl_longmip&&levels>9))PREPSW;}
 pl->levels=levels;pl->fmt=fmt;pl->cube=cube;pl->vol=vol;
 if(!(x=tex_of(t,1)))PREPSW;
 /* v10.330 (review #18): and the context whose shared palette made it (a texture shared by two contexts with their own shared palettes) */
 if(ti->_BaseFormat==GL_COLOR_INDEX&&(x->pal_gen!=pal_gen||x->pal_shared!=ctx->Texture.SharedPalette||
    x->pal_src!=(ctx->Texture.SharedPalette?(const void*)&ctx->Texture.Palette:(const void*)&t->Palette)))x->dirty=1;
 /* v10.287 (review): a lost copy (a mode change) is restored and each level's offset and pitch read again (DirectDraw may put it
  * elsewhere: the engine sampled the old offsets); restored or not (then freed, made again below) it is uploaded again, which
  * makes the core build its draws again (tex_changed). The batch's draws with the old offsets are sent first. */
 {int lost=x->m.levels&&vxgl_texmem_lost(&x->m),f;for(f=0;f<5&&x->m.levels;f++)if(x->cf[f].levels&&vxgl_texmem_lost(&x->cf[f]))lost=1;
  if(lost){int bad;x->vnew=0; /* v10.321: what was drawn into it is gone */vxgl_hw_flush_all();vxgl_batch_flush();spare_free(x); /* v10.366: made again when needed */vxgl_texmem_restore(&x->m);for(f=0;f<5;f++)if(x->cf[f].levels)vxgl_texmem_restore(&x->cf[f]);x->dirty=1;x->failed=GL_FALSE;
   bad=!x->m.levels;for(f=0;f<5;f++)if(x->cube&&!x->cf[f].levels)bad=1;
   if(bad){if(x->m.levels)vxgl_texmem_free(&x->m);for(f=0;f<5;f++)if(x->cf[f].levels)vxgl_texmem_free(&x->cf[f]);memset(&x->m,0,sizeof(x->m));memset(x->cf,0,sizeof(x->cf));}}} /* v10.295: a face that could not be restored: all made again */
 if(x->m.levels&&x->cube!=cube)x->dirty=1; /* v10.295 */
 if(x->failed&&x->fail_levels==levels&&x->fail_format==fmt&&(!x->fail_mem||x->fail_freed==vxgl_texmem_freed))PREPSW;
 /* v10.317: made again when anything in the plan differs from the copy's */
 if((x->dirty||!x->m.levels||!shape_same(x,pl)||x->base!=pl->base||x->sw!=pl->sw||x->sh!=pl->sh||x->sd!=pl->sd||x->padS!=pl->padS||x->padT!=pl->padT||
     (ringed&&x->ring!=pl->ring)||x->dmode!=pl->dmode||x->ctab!=pl->ctab||!table_same(x,pl->tab))&&!upload(c,t,x,pl))PREPSW;
 return x->handle;
}

/* v10.321: the copy level a framebuffer object's texture attachment is drawn into (vxgl_fbo.c): level `level` of a 2D texture whose image is
 * RGB (the copy X8R8G8B8, the engine's X8R8G8B8 target), uncompressed, no border, up to 2048. A copy that holds the level plain (no padding,
 * no conversion) at the image's size is used as it is; otherwise a copy of that level alone is made from Mesa's image (its own size: an NPOT
 * side is the sampler's NPOT layout, rows 32-byte aligned, the engine drawing it as any surface). Sampling the texture later in another
 * plan makes the copy again from the image, read back first (upload_). */
int vxgl_tex_rtt(vxgl_ctx *c,struct gl_texture_object *t,GLuint level,int make,vxgl_texmem **mp,GLuint *lp) {
 const struct gl_texture_image *ti;vxgl_tex *x;vxgl_plan pl;
 if(!t||t->Target!=GL_TEXTURE_2D||level>=MAX_TEXTURE_LEVELS||!(ti=t->Image[0][level]))return 0;
 if(ti->Border||ti->IsCompressed||ti->_BaseFormat!=GL_RGB||!ti->Width||!ti->Height||ti->Width>2048||ti->Height>2048)return 0;
 if(!(x=tex_of(t,1)))return 0;
 if(!x->no_ring){int k,any=0;for(k=0;k<3;k++)if(x->spare[k].levels)any=1;x->no_ring=1;if(any){vxgl_hw_flush_all();vxgl_batch_flush();spare_free(x);}} /* v10.366: vxgl_fbo.c keeps &x->m */
 if(!make){if(!x->m.levels||x->dirty||vxgl_texmem_lost(&x->m))return 0;}
 else if(x->m.levels&&vxgl_texmem_lost(&x->m)){int f;x->vnew=0;vxgl_hw_flush_all();vxgl_batch_flush();free_copy(x);memset(&x->m,0,sizeof(x->m));for(f=0;f<5;f++)memset(&x->cf[f],0,sizeof(x->cf[f]));x->dirty=1;} /* made again below */
 if(x->m.levels&&!x->dirty&&!x->cube&&!x->vol&&x->format==VX3DN_TEX_XRGB8888&&!x->padS&&!x->padT&&!x->dmode&&!x->ctab&&level>=x->base&&level<x->base+x->m.levels) {
  GLuint l=level-x->base,lw=x->m.w>>l,lh=x->m.h>>l;if(!lw)lw=1;if(!lh)lh=1;
  if(lw==ti->Width&&lh==ti->Height){*mp=&x->m;*lp=l;return 1;}}
 if(!make)return 0;
 if(x->failed&&x->fail_mem&&x->fail_levels==1&&x->fail_format==VX3DN_TEX_XRGB8888&&x->fail_freed==vxgl_texmem_freed)return 0; /* (no video memory since) */
 memset(&pl,0,sizeof(pl));pl.levels=1;pl.fmt=VX3DN_TEX_XRGB8888;pl.w=ti->Width;pl.h=ti->Height;pl.base=level;pl.sw=ti->Width;pl.sh=ti->Height;pl.sd=1;
 pl.npot=!pow2(ti->Width)||!pow2(ti->Height);
 if(!upload(c,t,x,&pl))return 0;
 *mp=&x->m;*lp=0;return 1;
}
/* ---------------------------------------------------------------- the texture environment as stage state */
#define TOP_DISABLE 1
#define TOP_SEL1 2
#define TOP_MODULATE 4
#define TOP_ADD 7
#define TOP_BLENDTEXALPHA 13
#define TA_DIFFUSE 0
#define TA_CURRENT 1
#define TA_TEXTURE 2
static void ops(uint32_t *s,uint32_t cop,uint32_t c1,uint32_t c2,uint32_t aop,uint32_t a1,uint32_t a2) {s[1]=cop;s[2]=c1;s[3]=c2;s[4]=aop;s[5]=a1;s[6]=a2;}
/* REPLACE MODULATE DECAL ADD for the base format; 0 = not on the engine */
/* v10.300: 0 for a colour of 0 in every channel asked, 1 for 1 in every one, -1 otherwise */
static int env_level(const GLfloat *c,int n) {int i,z=1,o=1;for(i=0;i<n;i++){if(c[i]!=0.0F)z=0;if(c[i]!=1.0F)o=0;}return z?0:o?1:-1;}
static uint32_t argb_of(const GLfloat *c);
static int env(uint32_t *s,GLenum mode,GLenum base,const GLfloat *ec,uint32_t *tf,int *tf_used) {
 int ta=base==GL_RGBA||base==GL_LUMINANCE_ALPHA||base==GL_ALPHA||base==GL_INTENSITY,tc=base!=GL_ALPHA;
 switch(mode) {
 /* v10.300: GL_BLEND (C = Cf (1 - Ct) + Cc Ct) when the environment colour is black or white in RGB (ACCEL_OPENGL_BLENDENV): black is
  * MODULATE by the complement of the texture, white is ADDSMOOTH (Ct + Cf - Ct Cf); an intensity's alpha the same by Ac; the engine has
  * no per-channel interpolation for another colour (software). Alpha as GL's table: At Af, or Af for RGB and LUMINANCE. */
 case GL_BLEND:{int lc=env_level(ec,3),la=env_level(ec+3,1);uint32_t cop,aop=TOP_SEL1,a1=TA_CURRENT;
  if(!vxgl_gl_blendenv)return 0;
  if(base==GL_ALPHA){ops(s,TOP_SEL1,TA_CURRENT,TA_CURRENT,TOP_MODULATE,TA_TEXTURE,TA_CURRENT);return 1;}
  /* v10.304: another colour as the core's LERP on the only unit (ACCEL_OPENGL_LERP): Ct x Cc + (1 - Ct) x Cf, Cc as TFACTOR; an
   * intensity's alpha the same by Ac */
  if(lc<0||(base==GL_INTENSITY&&la<0)){uint32_t col=argb_of(ec);
   if(!vxgl_gl_lerp||!lerp_here||(*tf_used&&*tf!=col))return 0;
   *tf=col;*tf_used=1;
   if(lc<0){ops(s,26,3,TA_CURRENT,TOP_SEL1,TA_CURRENT,TA_CURRENT);s[VX_D3D_TSS_CARG0]=TA_TEXTURE;}
   else ops(s,lc?11:TOP_MODULATE,lc?TA_TEXTURE:(TA_TEXTURE|0x10),TA_CURRENT,TOP_SEL1,TA_CURRENT,TA_CURRENT);
   if(base==GL_INTENSITY){if(la<0){s[4]=26;s[5]=3;s[6]=TA_CURRENT;s[VX_D3D_TSS_AARG0]=TA_TEXTURE;}else{s[4]=la?11:TOP_MODULATE;s[5]=la?TA_TEXTURE:(TA_TEXTURE|0x10);s[6]=TA_CURRENT;}}
   else if(base==GL_RGBA||base==GL_LUMINANCE_ALPHA){s[4]=TOP_MODULATE;s[5]=TA_TEXTURE;s[6]=TA_CURRENT;}
   return 1;}
  cop=lc?11:TOP_MODULATE; /* D3DTOP_ADDSMOOTH, or MODULATE with the texture complemented */
  if(base==GL_INTENSITY){if(la<0)return 0;aop=la?11:TOP_MODULATE;a1=la?TA_TEXTURE:(TA_TEXTURE|0x10);}
  else if(base==GL_RGBA||base==GL_LUMINANCE_ALPHA){aop=TOP_MODULATE;a1=TA_TEXTURE;}
  ops(s,cop,lc?TA_TEXTURE:(TA_TEXTURE|0x10),TA_CURRENT,aop,a1,TA_CURRENT);return 1;}
 case GL_REPLACE:ops(s,TOP_SEL1,tc?TA_TEXTURE:TA_CURRENT,TA_CURRENT,TOP_SEL1,ta?TA_TEXTURE:TA_CURRENT,TA_CURRENT);return 1;
 case GL_MODULATE:ops(s,tc?TOP_MODULATE:TOP_SEL1,tc?TA_TEXTURE:TA_CURRENT,TA_CURRENT,ta?TOP_MODULATE:TOP_SEL1,ta?TA_TEXTURE:TA_CURRENT,TA_CURRENT);return 1;
 case GL_DECAL:
  if(base==GL_RGB){ops(s,TOP_SEL1,TA_TEXTURE,TA_CURRENT,TOP_SEL1,TA_CURRENT,TA_CURRENT);return 1;}
  if(base==GL_RGBA){ops(s,TOP_BLENDTEXALPHA,TA_TEXTURE,TA_CURRENT,TOP_SEL1,TA_CURRENT,TA_CURRENT);return 1;}
  return 0; /* undefined for the others: as swrast does */
 case GL_ADD:
  ops(s,tc?TOP_ADD:TOP_SEL1,tc?TA_TEXTURE:TA_CURRENT,TA_CURRENT,base==GL_INTENSITY?TOP_ADD:ta?TOP_MODULATE:TOP_SEL1,ta?TA_TEXTURE:TA_CURRENT,TA_CURRENT);return 1;
 }
 return 0;
}
static uint32_t fbits(GLfloat f){uint32_t b;memcpy(&b,&f,4);return b;}
static uint32_t argb_of(const GLfloat *c) {GLubyte b[4];UNCLAMPED_FLOAT_TO_UBYTE(b[0],c[0]);UNCLAMPED_FLOAT_TO_UBYTE(b[1],c[1]);UNCLAMPED_FLOAT_TO_UBYTE(b[2],c[2]);UNCLAMPED_FLOAT_TO_UBYTE(b[3],c[3]);return ((uint32_t)b[3]<<24)|((uint32_t)b[0]<<16)|((uint32_t)b[1]<<8)|b[2];}
static uint32_t address(GLenum w) {
 switch(w){case GL_REPEAT:return 1;case GL_MIRRORED_REPEAT:return 2;case GL_CLAMP:case GL_CLAMP_TO_EDGE:return 3;case GL_CLAMP_TO_BORDER:return 4;}
 return 1;
}
/* v10.295: GL_COMBINE (ARB_texture_env_combine, ARB_texture_env_dot3) as Direct3D stage ops: a source and operand -> an argument
 * (TEXTURE this unit's, PRIMARY_COLOR DIFFUSE, PREVIOUS CURRENT, CONSTANT TFACTOR: one colour for all units, as in Direct3D; COMPLEMENT
 * for ONE_MINUS, ALPHAREPLICATE for an alpha operand of the colour combiner) */
static int csrc(GLenum src,GLenum operand,int alpha,int unit,uint32_t *a,int *need_tf) {
 uint32_t r;
 switch(src){case GL_TEXTURE:r=TA_TEXTURE;break;case GL_PRIMARY_COLOR:r=TA_DIFFUSE;break;case GL_PREVIOUS:r=TA_CURRENT;break;case GL_CONSTANT:r=3;*need_tf=1;break;
  default:if(src==GL_TEXTURE0+(GLenum)unit){r=TA_TEXTURE;break;}return 0;} /* crossbar: only the unit's own texture */
 if(!alpha)switch(operand){case GL_SRC_COLOR:break;case GL_ONE_MINUS_SRC_COLOR:r|=0x10;break;case GL_SRC_ALPHA:r|=0x20;break;case GL_ONE_MINUS_SRC_ALPHA:r|=0x30;break;default:return 0;}
 else switch(operand){case GL_SRC_ALPHA:break;case GL_ONE_MINUS_SRC_ALPHA:r|=0x10;break;default:return 0;}
 *a=r;return 1;
}
/* the combine function and its scale -> the op and its two arguments; 0 = not on the engine (ADD, SUBTRACT, INTERPOLATE and DOT3 scaled,
 * ADD_SIGNED by 4, INTERPOLATE by anything but an alpha, DOT3_RGB with its own alpha) */
/* v10.304: the scale of the others as the stage's result scale (*sc: 1 = x2, 2 = x4; ACCEL_OPENGL_SCALE), ADD_SIGNED by 4 as ADD_SIGNED
 * scaled by 4, and INTERPOLATE by a colour as the core's LERP on the only unit (ACCEL_OPENGL_LERP; *x0 = its interpolant) */
static int cop(GLenum mode,GLuint shift,int alpha,const uint32_t *a,uint32_t *op,uint32_t *x1,uint32_t *x2,uint32_t *x0,uint32_t *sc) {
 *sc=0;
 if(shift&&!vxgl_gl_scale&&mode!=GL_MODULATE&&!(mode==GL_ADD_SIGNED&&shift==1))return 0;
 switch(mode) {
 case GL_REPLACE:*sc=shift;*op=TOP_SEL1;*x1=a[0];*x2=TA_CURRENT;return 1;
 case GL_MODULATE:*op=shift==0?TOP_MODULATE:shift==1?5:6;*x1=a[0];*x2=a[1];return 1;
 case GL_ADD:*sc=shift;*op=TOP_ADD;*x1=a[0];*x2=a[1];return 1;
 case GL_ADD_SIGNED:if(shift==2){*op=8;*sc=2;}else *op=shift?9:8;*x1=a[0];*x2=a[1];return 1;
 case GL_SUBTRACT:*sc=shift;*op=10;*x1=a[0];*x2=a[1];return 1;
 case GL_INTERPOLATE:{uint32_t f=a[2]&15,comp=a[2]&0x10;
  if(!alpha&&!(a[2]&0x20)){ /* by a colour: arg0 x arg2 + arg1 x (1 - arg2) */
   if(!vxgl_gl_lerp||!lerp_here)return 0;
   *op=26;*x0=a[2];*x1=a[0];*x2=a[1];*sc=shift;return 1;}
  *sc=shift;
  switch(f){case TA_DIFFUSE:*op=12;break;case TA_TEXTURE:*op=13;break;case 3:*op=14;break;case TA_CURRENT:*op=16;break;default:return 0;}
  if(comp){*x1=a[1];*x2=a[0];}else{*x1=a[0];*x2=a[1];} /* BLEND..ALPHA: arg1 x alpha + arg2 x (1 - alpha) */
  return 1;}
 case GL_DOT3_RGBA:case GL_DOT3_RGBA_EXT:if(alpha)return 0;*sc=shift;*op=24;*x1=a[0];*x2=a[1];return 1;
 /* v10.317: DOT3_RGB (the core's DOTPRODUCT3 into colour only, VX_D3D_TSS_DOT3RGB; ACCEL_OPENGL_DOT3RGB) */
 case GL_DOT3_RGB:case GL_DOT3_RGB_EXT:if(alpha||!vxgl_gl_dot3rgb)return 0;*sc=shift;*op=24;*x1=a[0];*x2=a[1];return 1;
 }
 return 0;
}
static int combine(uint32_t *s,const struct gl_texture_unit *u,int unit,uint32_t *tf,int *tf_used) {
 const struct gl_tex_env_combine_state *k=u->_CurrentCombine;uint32_t a[3]={TA_CURRENT,TA_CURRENT,TA_CURRENT},b[3]={TA_CURRENT,TA_CURRENT,TA_CURRENT};int i,need=0;
 int rgba=k&&(k->ModeRGB==GL_DOT3_RGBA||k->ModeRGB==GL_DOT3_RGBA_EXT);
 if(!vxgl_gl_combine||!k)return 0;
 for(i=0;i<(int)k->_NumArgsRGB&&i<3;i++)if(!csrc(k->SourceRGB[i],k->OperandRGB[i],0,unit,&a[i],&need))return 0;
 if(!cop(k->ModeRGB,k->ScaleShiftRGB,0,a,&s[1],&s[2],&s[3],&s[VX_D3D_TSS_CARG0],&s[VX_D3D_TSS_CSCALE]))return 0;
 if(s[1]==24&&rgba){s[4]=TOP_SEL1;s[5]=TA_CURRENT;s[6]=TA_CURRENT;s[VX_D3D_TSS_ASCALE]=s[VX_D3D_TSS_CSCALE];} /* DOT3_RGBA writes the dot product to alpha too (Direct3D's DOTPRODUCT3) */
 else {if(s[1]==24)s[VX_D3D_TSS_DOT3RGB]=1; /* v10.317: DOT3_RGB: the alpha is the alpha combiner's */
  for(i=0;i<(int)k->_NumArgsA&&i<3;i++)if(!csrc(k->SourceA[i],k->OperandA[i],1,unit,&b[i],&need))return 0;
  if(!cop(k->ModeA,k->ScaleShiftA,1,b,&s[4],&s[5],&s[6],&s[VX_D3D_TSS_AARG0],&s[VX_D3D_TSS_ASCALE]))return 0;}
 if(need){uint32_t col=argb_of(u->EnvColor);if(*tf_used&&*tf!=col)return 0;*tf=col;*tf_used=1;}
 return 1;
}
/* v10.321: the fragment program's stage ops (vxgl_fp.c) over the units' stages, and its TFACTOR; the stage after them disabled */
static void fp_stages(vxgl_ctx *c) {
 uint32_t (*tss)[VX_D3D_TSS]=c->core.state.tss;int s;
 for(s=0;s<c->fp_nst&&s<VX_D3D_STAGES;s++){const uint32_t *o=c->fp_ops[s];
  tss[s][1]=o[0];tss[s][2]=o[1];tss[s][3]=o[2];tss[s][VX_D3D_TSS_CARG0]=o[3];tss[s][VX_D3D_TSS_CSCALE]=o[4];
  tss[s][4]=o[5];tss[s][5]=o[6];tss[s][6]=o[7];tss[s][VX_D3D_TSS_AARG0]=o[8];tss[s][VX_D3D_TSS_ASCALE]=o[9];
  tss[s][VX_D3D_TSS_DOT3RGB]=o[0]==24?1U:0U;}
 if(s<VX_D3D_STAGES){tss[s][1]=TOP_DISABLE;tss[s][4]=TOP_DISABLE;}
 if(c->fp_tf_used)c->core.state.rs[60]=c->fp_tf;
}
/* v10.295: VXGL.LOG names the line that sent a texture state to software (the first 32) */
#define TEXSW do{static int n_;if(n_++<32)vxgl_log("texture state to software (vxgl_tex.c line %d)\n",__LINE__);return 0;}while(0)
/* the units' state into the core's stages: 1 the engine draws it (c->tex_units stages, maybe 0), 0 software */
/* v10.330 (review #11): one texture on two units whose copies would differ (another colour table, depth mode, level range, padding): the
 * copy and its handle are the texture's, so the second unit's plan made it again under the first's stage; such a draw is software */
static int plan_differs(const vxgl_plan *a,const vxgl_plan *b) {
 if(a->levels!=b->levels||a->fmt!=b->fmt||a->w!=b->w||a->h!=b->h||a->base!=b->base||a->sw!=b->sw||a->sh!=b->sh||a->sd!=b->sd||a->padS!=b->padS||a->padT!=b->padT||
    a->npot!=b->npot||a->ring!=b->ring||a->dmode!=b->dmode||a->ctab!=b->ctab||(a->tab!=0)!=(b->tab!=0))return 1;
 if(a->tab&&a->tab!=b->tab){GLuint n=table_bytes(a->tab);if(n!=table_bytes(b->tab)||a->tab->_BaseFormat!=b->tab->_BaseFormat||(n&&memcmp(a->tab->TableUB,b->tab->TableUB,n)))return 1;}
 return 0;
}
int vxgl_tex_state(vxgl_ctx *c) {
 GLcontext *ctx=&c->gl;uint32_t (*tss)[VX_D3D_TSS]=c->core.state.tss;int unit,stage=0,s,nunits;uint32_t tf=0xFFFFFFFFUL;int tf_used=0;
 struct gl_texture_object *st_t[3];vxgl_plan st_pl[3];
 c->tex_units=0;c->tex_sets=0;c->tex_layout=0;c->tex_vol=0;c->tex_set3=0;c->core.state.rs[60]=0xFFFFFFFFUL;
 c->tex_cx[0]=c->tex_cx[1]=c->tex_cx[2]=0;
 for(s=0;s<VX_D3D_STAGES;s++)memcpy(tss[s],c->tss_default[s],sizeof(tss[s]));
 for(s=0;s<3;s++)c->tex_xf[s].on=0;
 /* v10.321: a fragment program as stages (vxgl_fp.c); the units' textures laid out below as for the fixed function */
 c->fp_on=0;
 if(ctx->FragmentProgram._Current||ctx->ATIFragmentShader._Enabled) {
  unsigned pm=0;int flat=1,n=0;
  for(unit=0;unit<8;unit++)if(ctx->Texture._EnabledUnits&(1U<<unit)){const GLfloat *m=ctx->TextureMatrixStack[unit].Top->m;const struct gl_texture_unit *u=&ctx->Texture.Unit[unit];n++;
   if((u->TexGenEnabled&Q_BIT)||m[3]!=0.0F||m[7]!=0.0F||m[11]!=0.0F||m[15]!=1.0F||c->q_forced)pm|=1U<<unit;
   if(u->_ReallyEnabled&(TEXTURE_CUBE_BIT|TEXTURE_3D_BIT))flat=0;}
  if(pm||n>1)flat=0;
  if(!vxgl_fp_compile(c,pm,flat))TEXSW;
 }
 if(!ctx->Texture._EnabledUnits){if(c->fp_on)fp_stages(c);return 1;}
 /* v10.307: three units (the core's three stages, XP's T149). The vertex carries two coordinate sets (T149 sends two), so the third
  * stage reads the set of an earlier one whose coordinates it matches (vxgl_tex_vertices_ok); ACCEL_OPENGL_UNITS3 = 0: software */
 if(ctx->Texture._EnabledUnits&~7U)TEXSW;
 nunits=(ctx->Texture.Unit[0]._ReallyEnabled?1:0)+(ctx->Texture.Unit[1]._ReallyEnabled?1:0)+(ctx->Texture.Unit[2]._ReallyEnabled?1:0);
 if(nunits>2&&(!vxgl_gl_units3||ctx->Point.PointSprite))TEXSW;
 for(unit=0;unit<3;unit++) {
  struct gl_texture_unit *u=&ctx->Texture.Unit[unit];struct gl_texture_object *t=u->_Current;GLenum base=0;GLuint h;int cube,vol,proj;vxgl_plan pl;
  if(!u->_ReallyEnabled)continue;
  if(u->_ReallyEnabled&~(TEXTURE_1D_BIT|TEXTURE_2D_BIT|TEXTURE_CUBE_BIT|TEXTURE_RECT_BIT|TEXTURE_3D_BIT))TEXSW;
  cube=(u->_ReallyEnabled&TEXTURE_CUBE_BIT)!=0;vol=(u->_ReallyEnabled&TEXTURE_3D_BIT)!=0;
  /* v10.295: the core draws a cube map (v10.317: a volume) on the only unit; v10.321: with a second unit too, on either (ACCEL_OPENGL_UNIT3D;
   * the engine's coordinate sets A and B each two- or three-dimensional, HCM p132), not with three */
  if((cube||vol)&&nunits>1&&(!vxgl_gl_unit3d||nunits>2||!(vx_d3d_feat&VX_F_UNIT3D)))TEXSW;
  {const GLfloat *m=ctx->TextureMatrixStack[unit].Top->m; /* q from texgen or a texture matrix: projective */
   proj=(u->TexGenEnabled&Q_BIT)||m[3]!=0.0F||m[7]!=0.0F||m[11]!=0.0F||m[15]!=1.0F||c->q_forced;} /* v10.300: or q in the vertices */
  if(proj&&(cube||vol||nunits>1||!vxgl_gl_projected))TEXSW; /* v10.295: projective on the only flat unit (the core's T151 read) */
  if(!(h=prepare(c,unit,stage,&base,&pl)))TEXSW;
  for(s=0;s<stage;s++)if(st_t[s]==t&&plan_differs(&st_pl[s],&pl))TEXSW; /* v10.330 (review #11) */
  st_t[stage]=t;st_pl[stage]=pl;
  if(pl.xf&&nunits>2)TEXSW; /* v10.317: a padded copy's coordinates are its own; a third stage shares a set as it is */
  /* v10.300: a point sprite's coordinates across the point are drawn for 2D, unmipmapped units (swrast samples level 0 there) */
  if(ctx->Point.PointSprite&&ctx->Point.CoordReplace[unit]&&(cube||vol||proj||mipmapped(t->MinFilter)||pl.xf))TEXSW;
  lerp_here=nunits==1&&!proj&&!cube&&!vol; /* v10.304: the core's LERP is a one-stage program on the only flat unit */
  if(!c->fp_on){ /* (a program's ops: fp_stages) */
   if(u->EnvMode==GL_COMBINE){if(!combine(tss[stage],u,unit,&tf,&tf_used))TEXSW;}
   else if(!env(tss[stage],u->EnvMode,base,u->EnvColor,&tf,&tf_used))TEXSW;}
  tss[stage][0]=h;tss[stage][11]=stage<2?(uint32_t)stage:(uint32_t)c->tex_set2; /* v10.307: the third stage's set, as last matched */
  /* v10.317: a padded side clamps on the engine (its edge texels repeated, or the ring) */
  tss[stage][13]=pl.padS?3:address(t->WrapS);tss[stage][14]=(u->_ReallyEnabled&TEXTURE_1D_BIT)?1:pl.padT?3:address(t->WrapT);tss[stage][15]=pl.dmode?pl.border:argb_of(t->BorderColor); /* v10.304; v10.330 (review #14): a depth texture's converted */
  if(vol)tss[stage][25]=address(t->WrapR); /* v10.317: D3DTSS_ADDRESSW */
  tss[stage][16]=t->MagFilter==GL_LINEAR?2:1;
  tss[stage][17]=(t->MinFilter==GL_LINEAR||t->MinFilter==GL_LINEAR_MIPMAP_NEAREST||t->MinFilter==GL_LINEAR_MIPMAP_LINEAR)?2:1;
  tss[stage][18]=(!mipmapped(t->MinFilter)||pl.levels<2&&pl.mm==2)?1:(t->MinFilter==GL_NEAREST_MIPMAP_NEAREST||t->MinFilter==GL_LINEAR_MIPMAP_NEAREST)?2:3;
  /* v10.317: one filter throughout: a depth texture's magnification filter (swrast's sample_depth_texture), the minification filter when
   * MIN_LOD makes every pixel minified, the magnification filter when MAX_LOD makes every pixel magnified */
  if(pl.dmode||c->fp_ati){tss[stage][17]=tss[stage][16];tss[stage][18]=1;} /* v10.321: an ATI shader samples at lambda 0 (s_atifragshader.c) */
  else if(pl.mm==1)tss[stage][16]=tss[stage][17];
  else if(pl.mm==2){tss[stage][17]=tss[stage][16];tss[stage][18]=1;}
  tss[stage][19]=fbits(u->LodBias+t->LodBias);tss[stage][20]=0;tss[stage][21]=1;tss[stage][24]=proj?0x103UL:0; /* COUNT3 | PROJECTED */
  /* v10.354: native anisotropic minification on every unit. Depth/ATI/LOD-forced magnification stay on their exact filters. */
  if(t->MaxAnisotropy>1.0F&&!pl.dmode&&!c->fp_ati&&pl.mm!=2){GLuint an=(GLuint)t->MaxAnisotropy;if(an>16)an=16;if(an<1)an=1;
   tss[stage][17]=3;tss[stage][21]=an;}
  if(stage>0){if(cube||vol)c->tex_set3=1;} /* v10.321: stage 1's set (its own, set 1) carries r */
  else if(cube)c->tex_layout=1;else if(vol){c->tex_layout=3;c->tex_vol=1;}else if(proj)c->tex_layout=2;
  c->tex_xf[stage].on=pl.xf;c->tex_xf[stage].sa=pl.sa;c->tex_xf[stage].sb=pl.sb;c->tex_xf[stage].ta=pl.ta;c->tex_xf[stage].tb=pl.tb;
  c->tex_unit_of[stage]=unit;c->tex_cx[stage]=pl.cx;stage++;
 }
 if(stage<VX_D3D_STAGES){tss[stage][1]=TOP_DISABLE;tss[stage][4]=TOP_DISABLE;}
 if(tf_used)c->core.state.rs[60]=tf;
 c->tex_units=stage;c->tex_sets=stage>2?2:stage;
 if(c->fp_on)fp_stages(c);
 return 1;
}
/* before the pipeline: what only the vertices tell (v10.268): a textured line or point, or a texture coordinate with q other than 1 */
/* v10.300: the only unit's vertices (a 2D texture, no texgen or texture matrix projection) carry q other than 1: then they are drawn
 * projective as texgen's are (ACCEL_OPENGL_QVERTEX; the core's T151 read, vxgl_tris.c proj_run), instead of by software */
int vxgl_tex_q_varies(vxgl_ctx *c) {
 GLcontext *ctx=&c->gl;struct vertex_buffer *VB=&TNL_CONTEXT(ctx)->vb;GLuint e=ctx->Texture._EnabledUnits,u,n;const GLvector4f *v;const GLubyte *p;
 if(!vxgl_gl_qvertex||!vxgl_gl_projected||(e!=1&&e!=2))return 0;
 u=e==1?0:1;if(ctx->Texture.Unit[u]._ReallyEnabled&(TEXTURE_CUBE_BIT|TEXTURE_3D_BIT))return 0; /* v10.317: a volume's q stays (vxgl_tex_vertices_ok) */
 v=VB->AttribPtr[_TNL_ATTRIB_TEX0+u];if(!v||v->size<4||!v->data)return 0;
 for(n=0,p=(const GLubyte*)v->data;n<(v->stride?VB->Count:1);n++,p+=v->stride)if(((const GLfloat*)p)[3]!=1.0F)return 1;
 return 0;
}
static GLfloat tex_component(const GLfloat *p,GLuint size,GLuint m) {return m<size?p[m]:(m==3?1.0F:0.0F);} /* v10.319 */
/* v10.307: the set (0 or 1) whose s, t the third stage's match at every vertex, -1 for none: the third stage reads the coordinate set of the earlier stage whose s, t it matches at every vertex (none: software) */
int vxgl_tex_set2(vxgl_ctx *c) {
 GLcontext *ctx=&c->gl;struct vertex_buffer *VB=&TNL_CONTEXT(ctx)->vb;
 {const GLvector4f *v2=VB->AttribPtr[_TNL_ATTRIB_TEX0+c->tex_unit_of[2]];int k,set=-1;
  for(k=0;k<2&&set<0;k++){const GLvector4f *w=VB->AttribPtr[_TNL_ATTRIB_TEX0+c->tex_unit_of[k]];GLuint n,cnt;const GLubyte *p,*q;
   if(!v2||!w||!v2->data||!w->data)continue;
   /* these are the inputs (before texgen and the texture matrix): the two units must transform them alike */
   {const struct gl_texture_unit *a=&ctx->Texture.Unit[c->tex_unit_of[2]],*b=&ctx->Texture.Unit[c->tex_unit_of[k]];
    if(a->TexGenEnabled||b->TexGenEnabled||memcmp(ctx->TextureMatrixStack[c->tex_unit_of[2]].Top->m,ctx->TextureMatrixStack[c->tex_unit_of[k]].Top->m,16*sizeof(GLfloat)))continue;}
   if(v2->data==w->data&&v2->stride==w->stride&&v2->size==w->size){set=k;break;}
   cnt=(v2->stride||w->stride)?VB->Count:1;
   /* v10.319 (audit G1): all four components (missing ones as GL's 0, 0, 0, 1): the same matrix gives the same s, t only from the same
    * s, t, r, q (a matrix that mixes r into s was drawn with the wrong set) */
   for(n=0,p=(const GLubyte*)v2->data,q=(const GLubyte*)w->data;n<cnt;n++,p+=v2->stride,q+=w->stride){GLuint m;
    for(m=0;m<4;m++)if(tex_component((const GLfloat*)p,v2->size,m)!=tex_component((const GLfloat*)q,w->size,m))break;
    if(m!=4)break;}
   if(n==cnt)set=k;}
  return set;}
}
int vxgl_tex_vertices_ok(vxgl_ctx *c,int set2) {
 GLcontext *ctx=&c->gl;struct vertex_buffer *VB=&TNL_CONTEXT(ctx)->vb;GLuint i,s;
 if(!c->tex_units)return 1;
 /* v10.295: textured points and wide lines are two triangles each (vxgl_tris.c, ACCEL_OPENGL_WIDE); a textured line one pixel wide is not */
 for(i=0;i<VB->PrimitiveCount;i++){GLenum m=VB->Primitive[i].mode;
  if(m==GL_POINTS&&!vxgl_gl_wide)return 0;
  if((m==GL_LINES||m==GL_LINE_LOOP||m==GL_LINE_STRIP)&&c->wide_line<=1.0F&&!vxgl_gl_lines)return 0;} /* v10.300: one pixel wide as runs */
 if((ctx->Polygon.FrontMode==GL_LINE||ctx->Polygon.BackMode==GL_LINE)&&c->wide_line<=1.0F&&!vxgl_gl_lines)return 0;
 if((ctx->Polygon.FrontMode==GL_POINT||ctx->Polygon.BackMode==GL_POINT)&&!vxgl_gl_wide)return 0;
 if(c->tex_units>2&&set2!=c->tex_set2)return 0; /* v10.307: the third stage's coordinates match neither set (v10.309: set2, as run_pipeline scanned it) */
 for(s=0;s<(GLuint)c->tex_units;s++) {
  const GLvector4f *v=VB->AttribPtr[_TNL_ATTRIB_TEX0+c->tex_unit_of[s]];GLuint n;const GLubyte *p;
  if(!v||v->size<4||c->tex_layout==2)continue; /* v10.295: projective: q goes to the engine */
  for(n=0,p=(const GLubyte*)v->data;n<(v->stride?VB->Count:1);n++,p+=v->stride)if(((const GLfloat*)p)[3]!=1.0F)return 0;
 }
 /* v10.357: a GL_CLAMP ring copy (prepare, pl->cx) is exact only while the coordinates stay in 0..1 on that side: the vertices' own (no
  * texgen on the side, an identity texture matrix, not projective), each one checked. A polygon whose vertices are in 0..1 interpolates
  * inside it, and Mesa's clipping keeps that */
 for(s=0;s<(GLuint)c->tex_units;s++)if(c->tex_cx[s]) {
  GLuint u=(GLuint)c->tex_unit_of[s],k,n;const GLvector4f *v=VB->AttribPtr[_TNL_ATTRIB_TEX0+u];const GLubyte *p;
  static const GLfloat id[16]={1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
  const struct gl_texture_unit *tu=&ctx->Texture.Unit[u];
  if(c->tex_layout==2||((c->tex_cx[s]&1)&&(tu->TexGenEnabled&S_BIT))||((c->tex_cx[s]&2)&&(tu->TexGenEnabled&T_BIT)))return 0;
  if(memcmp(ctx->TextureMatrixStack[u].Top->m,id,sizeof(id)))return 0;
  if(!v)return 0;
  for(n=0,p=(const GLubyte*)v->data;n<(v->stride?VB->Count:1);n++,p+=v->stride)
   for(k=0;k<2;k++)if(c->tex_cx[s]&(1U<<k)){GLfloat f=k<v->size?((const GLfloat*)p)[k]:0.0F;if(!(f>=0.0F&&f<=1.0F))return 0;}
 }
 return 1;
}
