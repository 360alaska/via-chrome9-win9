/* VXGL.DLL: Mesa's render stage -> the core (v10.239). Built like the Mesa unichrome driver (via_tris.c): tnl's t_vertex emits
 * the transformed vertex in the core's FVF layout (x y z rhw, diffuse BGRA, specular BGR + fog factor, texture coordinates),
 * t_dd_tritmp.h makes the triangle, line and point functions for two-sided lighting, polygon offset, unfilled polygons and flat
 * shading (the provoking vertex's colour copied), and each primitive goes to the core by its vertex index (vx_d3d_gl_prim). A
 * fallback (vxgl.h bits) hands the pipeline to swrast through swrast_setup, as the DRI drivers do. */
#include "vxgl.h"
#include "macros.h"
#include "colormac.h"
#include "enums.h"
#include "swrast/swrast.h"
#include "swrast_setup/swrast_setup.h"
#include "tnl/t_context.h"
#include "tnl/t_pipeline.h"
#include "tnl/t_vertex.h"
#include "math/m_vector.h"

/* v10.350: a colour byte from a float, (int) truncation as C's, on the float's encoding (Watcom's (int) calls __CHP, which sets the FPU's
 * rounding mode and back: half of Quake 2's particle frames with snapw); below 0 0, above 255 255, NaN 0 */
static GLubyte ub_trunc(GLfloat x) {
 union {GLfloat f;GLuint u;} v;GLuint e;v.f=x;
 if(v.u&0x80000000U)return 0; /* negative (and -0, -NaN) */
 e=(v.u>>23)&255U;if(e<127U)return 0;if(e>=135U)return (GLubyte)(e==255U&&(v.u&0x7FFFFFU)?0:255); /* < 1; >= 256 (NaN 0) */
 return (GLubyte)(((v.u&0x7FFFFFU)|0x800000U)>>(150U-e));
}
typedef union {
 struct {GLfloat x,y,z,w;GLuint color,spec;GLfloat u0,v0,u1,v1;} v;
 GLfloat f[16];GLuint ui[16];GLubyte ub4[16][4];
} vxglVertex,*vxglVertexPtr;

/* v10.481 (board, Lattice's profile: hw_prim 4.1% of the drawing thread, three divisions a triangle): a vertex's number by the store's
 * reciprocal: the offset times 1 / size plus 1.5 x 2^23 leaves the rounded number in the float's low mantissa bits (exact: the offset is a
 * multiple of the size below 2^22, the product within 2^-23 of a whole number); outside that range the division as before */
static uint32_t vert_no(const vxgl_ctx *c,const vxglVertex *v) {
 uint32_t d=(uint32_t)((const GLubyte*)v-c->verts);
 if(d<0x400000UL&&c->vs_rcp>0.0F){union {GLfloat f;uint32_t u;} x;x.f=(GLfloat)(int32_t)d*c->vs_rcp+12582912.0F;return x.u-0x4B400000UL;}
 return (uint32_t)(((const GLubyte*)v-c->verts)/c->vertex_size);
}
static void hw_prim(vxgl_ctx *c,const vxglVertex *const *v,GLuint n) {
 uint32_t idx[3],k,rc;
 for(k=0;k<n;k++)idx[k]=vert_no(c,v[k]);
 {VXGL_PROF_BEGIN;rc=vx_d3d_gl_prim(&c->core,&c->sv,idx,n,c->join);VXGL_PROF_END(2);}
 c->stats[n==3?0:n==2?1:2]++;
 if(rc){static int logged;if(logged++<8)vxgl_log("primitive refused: %08lX\n",(unsigned long)rc);}
}
/* v10.263: culling both faces drops every filled triangle (lines and points stay); with two-sided stencil each triangle takes its face's
 * state. The face as t_dd_tritmp.h finds it: cc = ex*fy - ey*fx on the y-flipped surface, back = (cc > 0) ^ _FrontBit. Lines and points
 * are front facing (GL). */
static void aa_triangle(vxgl_ctx *c,const vxglVertex *v0,const vxglVertex *v1,const vxglVertex *v2); /* v10.321 (below) */
static void vxgl_draw_triangle(vxgl_ctx *c,vxglVertexPtr v0,vxglVertexPtr v1,vxglVertexPtr v2) {
 const vxglVertex *v[3];
 if(c->cull_all)return;
 if(c->twoside_stencil){GLfloat ex=v0->v.x-v2->v.x,ey=v0->v.y-v2->v.y,fx=v1->v.x-v2->v.x,fy=v1->v.y-v2->v.y,cc=ex*fy-ey*fx;
  vxgl_stencil_face(c,((vxgl_flip(c)?cc>0:cc<0)?1:0)^(c->gl.Polygon._FrontBit?1:0));} /* v10.321: a framebuffer object's surface is not flipped */
 if(c->aa_tri){aa_triangle(c,v0,v1,v2);return;} /* v10.321 */
 v[0]=v0;v[1]=v1;v[2]=v2;hw_prim(c,v,3);
}
/* v10.295: wide lines and large or textured points as two triangles (ACCEL_OPENGL_WIDE; GL's non-antialiased rules: a line offset by
 * half its width along the minor axis, ends square to the major one; a point an s x s square of pixels, centred on the pixel holding it
 * for an odd size and on the nearest pixel corner for an even one). The four vertices are copies of the primitive's (colours, texture
 * coordinates) in a buffer of their own, named to the core for the two triangles and then the vertex buffer again. */
int vxgl_gl_wide=1; /* ACCEL_OPENGL_WIDE */
/* v10.301 (review: a diagonal wide line is a rectangle a pixel, each switching the core's vertex buffer): the quads of one line or point
 * gather in a buffer of their own (64 at most) and go to the core together, the vertex buffer named once, then the window's again */
#define QUADS_MAX 64
static GLubyte quad_buf[QUADS_MAX*4*64];static GLuint quad_n;
/* v10.330 (review #150): the core checks every vertex index against the store's real size (vx_d3d_gl_count); the window's store is tnl's
 * vertex buffer: (MaxArrayLockSize + 12) vertices of the largest layout (_tnl_init_vertices below), clipped vertices included */
static uint32_t verts_cap(vxgl_ctx *c) {GLcontext *ctx=&c->gl;TNLcontext *tnl=TNL_CONTEXT(ctx);uint32_t vs=c->vertex_size?c->vertex_size:1;return (uint32_t)((ctx->Const.MaxArrayLockSize+12)*tnl->clipspace.max_vertex_size/vs);}
static void quad_flush(vxgl_ctx *c) {
 uint32_t cm=c->core.state.rs[22],rc=0;GLuint q,n=c->vertex_size;
 if(!quad_n)return;
 vx_d3d_gl_begin(quad_buf,n,c->fvf);vx_d3d_gl_count(quad_n*4);
 {VXGL_PROF_BEGIN;
 for(q=0;q<quad_n&&!rc;q++) {
  uint32_t t0[3],t1[3],*t[2];int j;GLuint o=q*4;t0[0]=o;t0[1]=o+1;t0[2]=o+2;t1[0]=o+2;t1[1]=o+1;t1[2]=o+3;t[0]=t0;t[1]=t1;
  /* lines and points are not culled (GL): each triangle wound the way the core's CULLMODE keeps (2 culls area > 0, 3 area < 0) */
  for(j=0;j<2&&cm!=1;j++){const GLfloat *p0=(GLfloat*)(quad_buf+t[j][0]*n),*p1=(GLfloat*)(quad_buf+t[j][1]*n),*p2=(GLfloat*)(quad_buf+t[j][2]*n);
   GLfloat ar=(p1[0]-p0[0])*(p2[1]-p0[1])-(p1[1]-p0[1])*(p2[0]-p0[0]);
   if((cm==2&&ar>0)||(cm==3&&ar<0)){uint32_t x=t[j][1];t[j][1]=t[j][2];t[j][2]=x;}}
  /* v10.350: the two triangles as one strip when they are o o+1 o+2 and o+2 o+1 o+3 (the strip o o+1 o+2 o+3) or both turned (the
   * strip o o+2 o+1 o+3: o o+2 o+1, then o+1 o+2 o+3): one call, the second triangle one vertex */
  if(vxgl_gl_strips&&((t0[1]==o+1&&t1[1]==o+1)||(t0[1]==o+2&&t1[1]==o+3))){uint32_t s4[4];s4[0]=o;s4[1]=t0[1];s4[2]=t0[2];s4[3]=o+3;
   rc=vx_d3d_gl_strip(&c->core,&c->sv,s4,4,0);}
  else{rc=vx_d3d_gl_prim(&c->core,&c->sv,t0,3,0);if(!rc)rc=vx_d3d_gl_prim(&c->core,&c->sv,t1,3,0);}
  c->stats[0]+=2;
 }
 VXGL_PROF_END(2);}
 vx_d3d_gl_begin(c->verts,c->vertex_size,c->fvf);vx_d3d_gl_count(verts_cap(c));
 quad_n=0;
 if(rc){static int logged;if(logged++<8)vxgl_log("wide primitive refused: %08lX\n",(unsigned long)rc);}
}
static void hw_quad(vxgl_ctx *c,const vxglVertex *a,const vxglVertex *b,const vxglVertex *cc,const vxglVertex *d,const GLfloat *xy) {
 GLuint k,n=c->vertex_size;const vxglVertex *src[4];
 if(n>64)return;
 if(quad_n==QUADS_MAX)quad_flush(c);
 src[0]=a;src[1]=b;src[2]=cc;src[3]=d;
 for(k=0;k<4;k++){GLfloat *f=(GLfloat*)(quad_buf+(quad_n*4+k)*n);memcpy(f,src[k],n);f[0]=xy[k*2];f[1]=xy[k*2+1];}
 quad_n++;
}
/* v10.296: the vertex a fraction t of the way from a to b, for a wide line's run: x y z linear, the colours byte by byte (clamped),
 * rhw linear and the texture coordinates perspective-correct (s rhw linear, as swrast steps them along a line) */
static void lerp_vertex(const vxgl_ctx *c,const vxglVertex *a,const vxglVertex *b,GLfloat t,vxglVertex *o) {
 GLuint k,n=c->vertex_size/4;GLfloat wa=a->f[3],wb=b->f[3],w=wa+t*(wb-wa),iw=w>0.0F?1.0F/w:0.0F; /* v10.301: one reciprocal */
 for(k=0;k<n&&k<16;k++) {
  if(k==c->coloroffset||(c->specoffset&&k==c->specoffset)){int j;for(j=0;j<4;j++){GLfloat x=(GLfloat)a->ub4[k][j]+t*((GLfloat)b->ub4[k][j]-(GLfloat)a->ub4[k][j])+0.5F;
   o->ub4[k][j]=ub_trunc(x);}} /* v10.350: as (x<0?0:x>255?255:(int)x) */
  else if(k<3)o->f[k]=a->f[k]+t*(b->f[k]-a->f[k]);
  else if(k==3)o->f[3]=w;
  else o->f[k]=w>0.0F?(a->f[k]*wa+t*(b->f[k]*wb-a->f[k]*wa))*iw:a->f[k]+t*(b->f[k]-a->f[k]);
 }
}
/* v10.296 (review: a parallelogram around the endpoints is not swrast's coverage; 1361 pixels off for width-7 lines with fractional
 * ends): a wide line as swrast draws it (s_linetemp.h, s_lines.c draw_wide_line): the thin line's pixels from the truncated window
 * endpoints by Bresenham (the last one left out), x major when |dx| > |dy|, each pixel repeated across the width (start width / 2 below
 * it, or width / 2 - 1 for an even width). Pixels in a row of the major axis with the same minor coordinate make one rectangle with
 * its edges on half pixels (exactly those pixel centres), its vertices the line's attributes at pixel index i (i / n of the way). */
/* v10.300: one run of a line's pixels a..b (indices along the major axis) at minor coordinate mi, as a rectangle (see wide_line) */
static void line_run(vxgl_ctx *c,const vxglVertex *v0,const vxglVertex *v1,GLint a,GLint b,GLint mi,GLint n,int xm,GLint x0,GLint y0,GLint xs,GLint ys,GLint start,GLint width) {
 static vxglVertex ea,eb;GLfloat xy[8],ta=((GLfloat)a-0.5F)/(GLfloat)n,tb=((GLfloat)b+0.5F)/(GLfloat)n,wa=v0->f[3],wb=v1->f[3];
 /* v10.301 (review: a textured width-7 line with rhw 1/1000 and 1 at its ends drew nothing): the first and last runs' outer edges are
  * half a pixel past the ends, where rhw extrapolated can reach 0 or below and the core drops the triangles; such an edge takes the end's
  * own values (the half pixel's attributes as the end's) */
 if(ta<0.0F&&wa+ta*(wb-wa)<=wa*0.5F)ta=0.0F;
 if(tb>1.0F&&wa+tb*(wb-wa)<=wb*0.5F)tb=1.0F;
 lerp_vertex(c,v0,v1,ta,&ea);lerp_vertex(c,v0,v1,tb,&eb);
 if(xm){GLfloat xa=(GLfloat)(x0+a*xs)-0.5F*(GLfloat)xs,xb=(GLfloat)(x0+b*xs)+0.5F*(GLfloat)xs;
  GLfloat yt=vxgl_sy(c,(GLfloat)(mi-start+width)),yb=vxgl_sy(c,(GLfloat)(mi-start)); /* v10.321: the rows' edges through vxgl_sy (the window's flip or none) */
  xy[0]=xa;xy[1]=yt;xy[2]=xa;xy[3]=yb;xy[4]=xb;xy[5]=yt;xy[6]=xb;xy[7]=yb;}
 else {GLfloat ya=vxgl_sy(c,(GLfloat)(y0+a*ys)+0.5F-0.5F*(GLfloat)ys),yb=vxgl_sy(c,(GLfloat)(y0+b*ys)+0.5F+0.5F*(GLfloat)ys);
  GLfloat xl=(GLfloat)(mi-start)-0.5F,xr=(GLfloat)(mi-start+width-1)+0.5F;
  xy[0]=xl;xy[1]=ya;xy[2]=xr;xy[3]=ya;xy[4]=xl;xy[5]=yb;xy[6]=xr;xy[7]=yb;}
 hw_quad(c,&ea,&ea,&eb,&eb,xy);
}
/* v10.296 (review: a parallelogram around the endpoints is not swrast's coverage; 1361 pixels off for width-7 lines with fractional
 * ends): a line as swrast draws it (s_linetemp.h, s_lines.c draw_wide_line): the thin line's pixels from the truncated window
 * endpoints by Bresenham (the last one left out), x major when |dx| > |dy|, each pixel repeated across the width (start width / 2 below
 * it, or width / 2 - 1 for an even width). Pixels in a row of the major axis with the same minor coordinate make one rectangle with
 * its edges on half pixels (exactly those pixel centres), its vertices the line's attributes at pixel index i (i / n of the way).
 * v10.300: any width (textured one-pixel lines too), and line stipple as swrast's (s_lines.c compute_stipple_mask: pixel i of the thin
 * line drawn when bit (counter / factor) & 15 of the pattern is set, the counter counting every pixel; tnl resets it) */
static void wide_line(vxgl_ctx *c,const vxglVertex *v0,const vxglVertex *v1,GLint width) {
 GLfloat wx0=v0->v.x+0.5F,wy0=vxgl_wy(c,v0->v.y),wx1=v1->v.x+0.5F,wy1=vxgl_wy(c,v1->v.y);
 GLint x0,y0,x1,y1,dx,dy,xs,ys,n,start,i,a,err,inc,dec,xm,mi;int rv=1;
 if(IS_INF_OR_NAN(wx0+wy0+wx1+wy1)||wx0<-1e6F||wx0>1e6F||wy0<-1e6F||wy0>1e6F||wx1<-1e6F||wx1>1e6F||wy1<-1e6F||wy1>1e6F)return;
 if(width<1)width=1;
 x0=(GLint)wx0;y0=(GLint)wy0;x1=(GLint)wx1;y1=(GLint)wy1;dx=x1-x0;dy=y1-y0;
 if(!dx&&!dy)return;
 xs=dx<0?-1:1;if(dx<0)dx=-dx;ys=dy<0?-1:1;if(dy<0)dy=-dy;
 n=dx>dy?dx:dy;xm=dx>dy;start=(width&1)?width/2:width/2-1;
 if(xm){inc=dy+dy;err=inc-dx;dec=err-dx;mi=y0;}else{inc=dx+dx;err=inc-dy;dec=err-dy;mi=x0;}
 for(a=0,i=0;i<n;i++) {
  int last=i==n-1,step=err>=0,v=1; /* the minor coordinate moves after pixel i when step */
  if(c->stip_on){v=(int)((c->stip_pattern>>((c->stip_count/c->stip_factor)&15U))&1U);c->stip_count++;}
  if(i==a)rv=v;else if(v!=rv){if(rv)line_run(c,v0,v1,a,i-1,mi,n,xm,x0,y0,xs,ys,start,width);a=i;rv=v;}
  if(step)err+=dec;else err+=inc;
  if(step||last){if(rv)line_run(c,v0,v1,a,i,mi,n,xm,x0,y0,xs,ys,start,width);a=i+1;if(step)mi+=xm?ys:xs;}
 }
 quad_flush(c);
}
/* v10.321: antialiased points, lines and polygons on the engine (ACCEL_OPENGL_SMOOTH = 0: software), pixel for pixel as swrast draws them:
 * its pixels (s_pointtemp.h's box and circle, s_aaline.c's scan along the line, s_aatritemp.h's scan from the long edge while the
 * coverage stays above 0) and its coverage of each (the point's 1 - (d^2 - rmin^2) / (rmax^2 - rmin^2); the 16 samples of the line's
 * rectangle and the triangle, the same tables and the same shortcut when the first 4 are inside), which s_span.c multiplies into the
 * fragment's alpha after texturing and fog (and after the alpha test: state.c sends that to software, and a texture stage that does not
 * carry the vertex alpha through). Each run of a row's pixels with one coverage is a rectangle (its edges on half pixels, around exactly
 * those pixel centres, as the wide lines' runs) whose corners carry the primitive's attributes there: the point's own; the line's at
 * the pixel's corner (s_aalinetemp.h solves its planes at (ix, iy)), a fraction of v0..v1; the triangle's planes at the pixel centre
 * (barycentric, extrapolated, rhw and s * rhw linear) - and the alpha times the coverage. Culled as swrast does (the area). */
int vxgl_gl_smooth=1;
typedef void (*aa_attr_fn)(vxgl_ctx *c,const void *d,GLfloat gx,GLfloat gy,vxglVertex *o); /* the vertex at GL window position (gx, gy) */
typedef struct {aa_attr_fn f;const void *d;GLint iy,x0,x1;GLfloat cov;int open;} aa_rows;
static void aa_run(vxgl_ctx *c,aa_rows *r) {
 static vxglVertex q[4];GLfloat xy[8],l=(GLfloat)r->x0-0.5F,rt=(GLfloat)r->x1+0.5F,t=vxgl_sy(c,(GLfloat)(r->iy+1)),b=vxgl_sy(c,(GLfloat)r->iy);int k; /* v10.321: through vxgl_sy */
 if(!r->open)return;
 r->open=0;
 r->f(c,r->d,(GLfloat)r->x0,(GLfloat)(r->iy+1),&q[0]);r->f(c,r->d,(GLfloat)(r->x1+1),(GLfloat)(r->iy+1),&q[1]); /* top: GL's y + 1 */
 r->f(c,r->d,(GLfloat)r->x0,(GLfloat)r->iy,&q[2]);r->f(c,r->d,(GLfloat)(r->x1+1),(GLfloat)r->iy,&q[3]);
 if(r->cov<1.0F)for(k=0;k<4;k++){GLubyte *a=&q[k].ub4[c->coloroffset][3];*a=ub_trunc((GLfloat)*a*r->cov);} /* (GLubyte) CLAMP(a * coverage); v10.350 without __CHP */
 xy[0]=l;xy[1]=t;xy[2]=rt;xy[3]=t;xy[4]=l;xy[5]=b;xy[6]=rt;xy[7]=b;
 hw_quad(c,&q[0],&q[1],&q[2],&q[3],xy);
}
/* pixel (ix, iy) with coverage cov: joined to the open run when next to it in the row with the same coverage */
static void aa_pixel(vxgl_ctx *c,aa_rows *r,GLint ix,GLint iy,GLfloat cov) {
 if(r->open&&iy==r->iy&&cov==r->cov){if(ix==r->x1+1){r->x1=ix;return;}if(ix==r->x0-1){r->x0=ix;return;}}
 aa_run(c,r);r->open=1;r->iy=iy;r->x0=r->x1=ix;r->cov=cov;
}

/* v10.353: swrast's alpha test (s_alpha.c) on a fragment's alpha byte: the reference as CLAMPED_FLOAT_TO_UBYTE */
static int alpha_pass(const GLcontext *ctx,GLubyte a) {
 GLubyte r;CLAMPED_FLOAT_TO_UBYTE(r,ctx->Color.AlphaRef);
 switch(ctx->Color.AlphaFunc){case GL_LESS:return a<r;case GL_LEQUAL:return a<=r;case GL_GEQUAL:return a>=r;case GL_GREATER:return a>r;
  case GL_NOTEQUAL:return a!=r;case GL_EQUAL:return a==r;case GL_ALWAYS:return 1;default:return 0;}
}
/* points (s_pointtemp.h, SMOOTH): every corner the point's vertex, its alpha faded as an attenuated point's is with multisampling on */
static void aa_point_attr(vxgl_ctx *c,const void *d,GLfloat gx,GLfloat gy,vxglVertex *o) {(void)gx;(void)gy;memcpy(o,d,c->vertex_size>sizeof(*o)?sizeof(*o):c->vertex_size);}
static void aa_point(vxgl_ctx *c,const vxglVertex *v0) {
 GLcontext *ctx=&c->gl;GLfloat wx=v0->v.x+0.5F,wy=vxgl_wy(c,v0->v.y),size=ctx->Point._Size,atten=1.0F,radius,rmin,rmax,rmin2,rmax2,cscale;
 GLint x,y,xmin,xmax,ymin,ymax;static vxglVertex base;aa_rows r;
 if(IS_INF_OR_NAN(wx+wy)||wx<-1e6F||wx>1e6F||wy<-1e6F||wy>1e6F)return;
 if(c->point_atten){struct vertex_buffer *VB=&TNL_CONTEXT(ctx)->vb;const GLvector4f *ps=VB->AttribPtr[_TNL_ATTRIB_POINTSIZE];GLuint idx=(GLuint)(((const GLubyte*)v0-c->verts)/c->vertex_size);
  GLfloat s=size;if(ps&&ps->data&&idx<VB->Count)s=*(const GLfloat*)((const GLubyte*)ps->data+idx*ps->stride);
  size=CLAMP(s,ctx->Point.MinSize,ctx->Point.MaxSize);
  if(ctx->Multisample.Enabled&&s<ctx->Point.Threshold){GLfloat f=s/ctx->Point.Threshold;atten=f*f;}}
 size=CLAMP(size,ctx->Const.MinPointSizeAA,ctx->Const.MaxPointSizeAA);
 radius=0.5F*size;rmin=radius-0.7071F;rmax=radius+0.7071F;rmin2=MAX2(0.0F,rmin*rmin);rmax2=rmax*rmax;cscale=1.0F/(rmax2-rmin2);
 xmin=(GLint)(wx-radius);xmax=(GLint)(wx+radius);ymin=(GLint)(wy-radius);ymax=(GLint)(wy+radius);
 memcpy(&base,v0,c->vertex_size>sizeof(base)?sizeof(base):c->vertex_size);
 if(atten<1.0F)base.ub4[c->coloroffset][3]=(GLubyte)((GLfloat)base.ub4[c->coloroffset][3]*atten); /* (GLchan) (alpha * alphaAtten) */
 if(c->aa_atest&&!alpha_pass(ctx,base.ub4[c->coloroffset][3]))return; /* v10.353: swrast tests the point's alpha before its coverage */
 r.f=aa_point_attr;r.d=&base;r.open=0;
 for(y=ymin;y<=ymax;y++)for(x=xmin;x<=xmax;x++) {
  const GLfloat dx=(GLfloat)x-wx+0.5F,dy=(GLfloat)y-wy+0.5F,dist2=dx*dx+dy*dy;
  if(dist2<rmax2)aa_pixel(c,&r,x,y,dist2>=rmin2?1.0F-(dist2-rmin2)*cscale:1.0F);
 }
 aa_run(c,&r);quad_flush(c);
}
/* lines (s_aaline.c): the rectangle's corners, its edges, the 4 x 4 samples (the corners first) */
typedef struct {const vxglVertex *v0,*v1;GLfloat x0,y0,dx,dy,len,wa,wb,qx[4],qy[4],ex[4],ey[4];} aa_lineinfo;
static GLfloat aa_line_cov(const aa_lineinfo *l,GLint winx,GLint winy) {
 static GLfloat samples[16][2];static int have;const GLfloat x=(GLfloat)winx,y=(GLfloat)winy;GLint stop=4,i,k;GLfloat inside=16.0F;
 if(!have){GLint xs,ys,j;i=4;for(xs=0;xs<4;xs++)for(ys=0;ys<4;ys++){j=(xs==0&&ys==0)?0:(xs==3&&ys==0)?1:(xs==0&&ys==3)?2:(xs==3&&ys==3)?3:i++;
   samples[j][0]=(GLfloat)xs*0.25F+0.125F;samples[j][1]=(GLfloat)ys*0.25F+0.125F;}have=1;} /* make_sample_table(4, 4) */
 for(i=0;i<stop;i++) {
  const GLfloat sx=x+samples[i][0],sy=y+samples[i][1];int out=0;
  for(k=0;k<4;k++){GLfloat cr=l->ex[k]*(sy-l->qy[k])-l->ey[k]*(sx-l->qx[k]);if(cr==0.0F)cr=l->ex[k]+l->ey[k];if(cr<0.0F)out=1;}
  if(out){inside-=1.0F;stop=16;}
 }
 return stop==4?1.0F:inside*(1.0F/16.0F);
}
/* the line's attributes at the pixel's corner: the corner (gx, gy) of the rectangle around pixel centres takes the planes at (gx, gy)
 * - (0.5, 0.5), so each centre gets them at its pixel's (ix, iy) */
static void aa_line_attr(vxgl_ctx *c,const void *d,GLfloat gx,GLfloat gy,vxglVertex *o) {
 const aa_lineinfo *l=(const aa_lineinfo*)d;GLfloat t=((gx-0.5F-l->x0)*l->dx+(gy-0.5F-l->y0)*l->dy)/(l->len*l->len);
 if(t<0.0F&&l->wa+t*(l->wb-l->wa)<=l->wa*0.5F)t=0.0F; /* (as line_run: an extrapolated rhw at 0 or below) */
 if(t>1.0F&&l->wa+t*(l->wb-l->wa)<=l->wb*0.5F)t=1.0F;
 lerp_vertex(c,l->v0,l->v1,t,o);
}
static void aa_segment(vxgl_ctx *c,aa_lineinfo *l,GLfloat hw,GLfloat t0,GLfloat t1,aa_rows *r) {
 const GLfloat absDx=l->dx<0.0F?-l->dx:l->dx,absDy=l->dy<0.0F?-l->dy:l->dy,x0=l->x0+t0*l->dx,y0=l->y0+t0*l->dy,x1=l->x0+t1*l->dx,y1=l->y0+t1*l->dy;
 const GLfloat xAdj=l->dx/l->len*hw,yAdj=l->dy/l->len*hw;int k;
 l->qx[0]=x0-yAdj;l->qy[0]=y0+xAdj;l->qx[1]=x0+yAdj;l->qy[1]=y0-xAdj;l->qx[2]=x1+yAdj;l->qy[2]=y1-xAdj;l->qx[3]=x1-yAdj;l->qy[3]=y1+xAdj;
 for(k=0;k<4;k++){l->ex[k]=l->qx[(k+1)&3]-l->qx[k];l->ey[k]=l->qy[(k+1)&3]-l->qy[k];}
 if(absDx>absDy) {
  GLfloat dydx=l->dy/l->dx,xLeft,xRight,yBot,yTop;GLint ix,ixRight;
  if(x0<x1){xLeft=x0-hw;xRight=x1+hw;if(l->dy>=0.0F){yBot=y0-3.0F*hw;yTop=y0+hw;}else{yBot=y0-hw;yTop=y0+3.0F*hw;}}
  else {xLeft=x1-hw;xRight=x0+hw;if(l->dy<=0.0F){yBot=y1-3.0F*hw;yTop=y1+hw;}else{yBot=y1-hw;yTop=y1+3.0F*hw;}}
  ixRight=(GLint)(xRight+1.0F);
  for(ix=(GLint)xLeft;ix<ixRight;ix++){const GLint iyBot=(GLint)yBot,iyTop=(GLint)(yTop+1.0F);GLint iy;
   for(iy=iyBot;iy<iyTop;iy++){GLfloat cv=aa_line_cov(l,ix,iy);if(cv!=0.0F)aa_pixel(c,r,ix,iy,cv);}
   yBot+=dydx;yTop+=dydx;}
 }
 else {
  GLfloat dxdy=l->dx/l->dy,yBot,yTop,xLeft,xRight;GLint iy,iyTop;
  if(y0<y1){yBot=y0-hw;yTop=y1+hw;if(l->dx>=0.0F){xLeft=x0-3.0F*hw;xRight=x0+hw;}else{xLeft=x0-hw;xRight=x0+3.0F*hw;}}
  else {yBot=y1-hw;yTop=y0+hw;if(l->dx<=0.0F){xLeft=x1-3.0F*hw;xRight=x1+hw;}else{xLeft=x1-hw;xRight=x1+3.0F*hw;}}
  iyTop=(GLint)(yTop+1.0F);
  for(iy=(GLint)yBot;iy<iyTop;iy++){const GLint ixLeft=(GLint)xLeft,ixRight=(GLint)(xRight+1.0F);GLint ix;
   for(ix=ixLeft;ix<ixRight;ix++){GLfloat cv=aa_line_cov(l,ix,iy);if(cv!=0.0F)aa_pixel(c,r,ix,iy,cv);}
   xLeft+=dxdy;xRight+=dxdy;}
 }
}
static void aa_line(vxgl_ctx *c,const vxglVertex *v0,const vxglVertex *v1) {
 GLcontext *ctx=&c->gl;GLfloat hw=0.5F*ctx->Line._Width;static aa_lineinfo l;aa_rows r;
 l.v0=v0;l.v1=v1;l.x0=v0->v.x+0.5F;l.y0=vxgl_wy(c,v0->v.y);l.dx=v1->v.x-v0->v.x;l.dy=vxgl_wy(c,v1->v.y)-l.y0;l.wa=v0->f[3];l.wb=v1->f[3];
 if(IS_INF_OR_NAN(l.x0+l.y0+l.dx+l.dy)||l.x0<-1e6F||l.x0>1e6F||l.y0<-1e6F||l.y0>1e6F||l.dx<-1e6F||l.dx>1e6F||l.dy<-1e6F||l.dy>1e6F)return;
 l.len=(GLfloat)sqrt((double)(l.dx*l.dx+l.dy*l.dy));
 if(l.len==0.0F)return;
 if(c->aa_atest&&!alpha_pass(ctx,v0->ub4[c->coloroffset][3]))return; /* v10.353: both ends' alpha the same (run_pipeline): the line's own, tested as swrast does */
 r.f=aa_line_attr;r.d=&l;r.open=0;
 if(c->stip_on) { /* s_aalinetemp.h: a segment from each run of set pattern bits, one bit per whole unit of length (tEnd the run's last bit's start) */
  GLint iLen=(GLint)l.len,i;int in=0;GLfloat ts=0.0F,te=0.0F;
  /* v10.334 (audit F04): a run ends at the end of its last set bit's unit ((i + 1) / len) and is closed at the first clear bit; it
   * ended at its last bit's start and stayed open after a one-bit run, so pattern 0x0001 drew the whole line. swrast (s_aalinetemp.h)
   * is changed the same way */
  for(i=0;i<iLen;i++) {
   GLuint bit=(c->stip_count/c->stip_factor)&15U;
   if((1U<<bit)&c->stip_pattern){if(!in){in=1;ts=(GLfloat)i/l.len;}te=(GLfloat)(i+1)/l.len;}
   else if(in){aa_segment(c,&l,hw,ts,te,&r);in=0;}
   c->stip_count++;
  }
  if(in)aa_segment(c,&l,hw,ts,1.0F,&r);
 }
 else aa_segment(c,&l,hw,0.0F,1.0F,&r);
 aa_run(c,&r);quad_flush(c);
}
/* triangles (s_aatriangle.c, s_aatritemp.h) */
typedef struct {const vxglVertex *v[3];GLfloat P[3][2],area2;} aa_triinfo;
static GLfloat aa_tri_cov(const GLfloat *v0,const GLfloat *v1,const GLfloat *v2,GLint winx,GLint winy) {
#define POS(a,b) (0.5F+(GLfloat)(a)*4.0F+(GLfloat)(b))/16.0F
 static const GLfloat samples[16][2]={{POS(0,2),POS(0,0)},{POS(3,3),POS(0,2)},{POS(0,0),POS(3,1)},{POS(3,1),POS(3,3)},{POS(1,1),POS(0,1)},{POS(2,0),POS(0,3)},
  {POS(0,3),POS(1,3)},{POS(1,2),POS(1,0)},{POS(2,3),POS(1,2)},{POS(3,2),POS(1,1)},{POS(0,1),POS(2,2)},{POS(1,0),POS(2,1)},{POS(2,1),POS(2,3)},{POS(3,0),POS(2,0)},
  {POS(1,3),POS(3,0)},{POS(2,2),POS(3,2)}};
#undef POS
 const GLfloat x=(GLfloat)winx,y=(GLfloat)winy,dx0=v1[0]-v0[0],dy0=v1[1]-v0[1],dx1=v2[0]-v1[0],dy1=v2[1]-v1[1],dx2=v0[0]-v2[0],dy2=v0[1]-v2[1];
 GLint stop=4,i;GLfloat inside=16.0F;
 for(i=0;i<stop;i++) {
  const GLfloat sx=x+samples[i][0],sy=y+samples[i][1],fx0=sx-v0[0],fy0=sy-v0[1],fx1=sx-v1[0],fy1=sy-v1[1],fx2=sx-v2[0],fy2=sy-v2[1];
  GLfloat cross0=dx0*fy0-dy0*fx0,cross1=dx1*fy1-dy1*fx1,cross2=dx2*fy2-dy2*fx2;
  if(cross0==0.0F)cross0=dx0+dy0;if(cross1==0.0F)cross1=dx1+dy1;if(cross2==0.0F)cross2=dx2+dy2;
  if(cross0<0.0F||cross1<0.0F||cross2<0.0F){inside-=1.0F;stop=16;}
 }
 return stop==4?1.0F:inside*(1.0F/16.0F);
}
/* the triangle's planes at window position (x, y) (barycentric, extrapolated outside it): x y z linear, the colours byte by byte
 * (clamped), rhw linear and the texture coordinates perspective-correct (s rhw linear) */
static void aa_tri_attr(vxgl_ctx *c,const void *d,GLfloat x,GLfloat y,vxglVertex *o) {
 const aa_triinfo *t=(const aa_triinfo*)d;const vxglVertex *const *v=t->v;const GLfloat (*P)[2]=t->P;GLfloat b[3],w,iw,wmin;GLuint k,j,n=c->vertex_size/4;
 b[0]=((P[1][0]-x)*(P[2][1]-y)-(P[1][1]-y)*(P[2][0]-x))/t->area2;b[1]=((P[2][0]-x)*(P[0][1]-y)-(P[2][1]-y)*(P[0][0]-x))/t->area2;b[2]=1.0F-b[0]-b[1];
 w=b[0]*v[0]->f[3]+b[1]*v[1]->f[3]+b[2]*v[2]->f[3];wmin=MIN2(v[0]->f[3],MIN2(v[1]->f[3],v[2]->f[3]));
 if(w<wmin*0.25F)w=wmin*0.25F; /* rhw reaching 0 past a far edge (half a pixel out): kept above it (the core drops rhw at 0 or below) */
 iw=w>0.0F?1.0F/w:0.0F;
 for(k=0;k<n&&k<16;k++) {
  if(k==c->coloroffset||(c->specoffset&&k==c->specoffset)){for(j=0;j<4;j++){GLfloat s=b[0]*(GLfloat)v[0]->ub4[k][j]+b[1]*(GLfloat)v[1]->ub4[k][j]+b[2]*(GLfloat)v[2]->ub4[k][j]+0.5F;
   o->ub4[k][j]=(GLubyte)(s<0.0F?0:s>255.0F?255:(int)s);}}
  else if(k<3)o->f[k]=b[0]*v[0]->f[k]+b[1]*v[1]->f[k]+b[2]*v[2]->f[k];
  else if(k==3)o->f[3]=w;
  else o->f[k]=w>0.0F?(b[0]*v[0]->f[k]*v[0]->f[3]+b[1]*v[1]->f[k]*v[1]->f[3]+b[2]*v[2]->f[k]*v[2]->f[3])*iw:b[0]*v[0]->f[k]+b[1]*v[1]->f[k]+b[2]*v[2]->f[k];
 }
}
static void aa_triangle(vxgl_ctx *c,const vxglVertex *v0,const vxglVertex *v1,const vxglVertex *v2) {
 static aa_triinfo ti;GLcontext *ctx=&c->gl;GLfloat ar,yMin,yMax,majDx,majDy;const GLfloat *pMin,*pMid,*pMax;int k,iMin,iMid,iMax;GLint iyMin,iyMax,iy;
 uint32_t cm=c->core.state.rs[22];aa_rows r;
 /* culled by the core's CULLMODE (2 culls area > 0, 3 area < 0, on the engine's y-down surface), which matches GL's choice */
 ar=(v1->v.x-v0->v.x)*(v2->v.y-v0->v.y)-(v1->v.y-v0->v.y)*(v2->v.x-v0->v.x);
 if(ar==0.0F||IS_INF_OR_NAN(ar)||(cm==2&&ar>0.0F)||(cm==3&&ar<0.0F))return;
 ti.v[0]=v0;ti.v[1]=v1;ti.v[2]=v2;
 for(k=0;k<3;k++){ti.P[k][0]=ti.v[k]->v.x+0.5F;ti.P[k][1]=vxgl_wy(c,ti.v[k]->v.y);if(ti.P[k][0]<-1e6F||ti.P[k][0]>1e6F||ti.P[k][1]<-1e6F||ti.P[k][1]>1e6F)return;}
 ti.area2=(ti.P[1][0]-ti.P[0][0])*(ti.P[2][1]-ti.P[0][1])-(ti.P[1][1]-ti.P[0][1])*(ti.P[2][0]-ti.P[0][0]);
 if(ti.area2==0.0F||IS_INF_OR_NAN(ti.area2))return;
 { /* bottom to top (s_aatritemp.h) */
  GLfloat y0=ti.P[0][1],y1=ti.P[1][1],y2=ti.P[2][1];
  if(y0<=y1){if(y1<=y2){iMin=0;iMid=1;iMax=2;}else if(y2<=y0){iMin=2;iMid=0;iMax=1;}else{iMin=0;iMid=2;iMax=1;}}
  else {if(y0<=y2){iMin=1;iMid=0;iMax=2;}else if(y2<=y1){iMin=2;iMid=1;iMax=0;}else{iMin=1;iMid=2;iMax=0;}}
 }
 pMin=ti.P[iMin];pMid=ti.P[iMid];pMax=ti.P[iMax];majDx=pMax[0]-pMin[0];majDy=pMax[1]-pMin[1];
 {const GLfloat botDx=pMid[0]-pMin[0],botDy=pMid[1]-pMin[1],area=majDx*botDy-botDx*majDy;if(area==0.0F||IS_INF_OR_NAN(area))return;ar=area;}
 r.f=aa_tri_attr;r.d=&ti;r.open=0;
 yMin=pMin[1];yMax=pMax[1];iyMin=(GLint)yMin;iyMax=(GLint)yMax+1;
 if(ar<0.0F) { /* the long edge on the left: scan left to right */
  const GLfloat dxdy=majDx/majDy,xAdj=dxdy<0.0F?-dxdy:0.0F;GLfloat x=pMin[0]-(yMin-(GLfloat)iyMin)*dxdy;
  for(iy=iyMin;iy<iyMax;iy++,x+=dxdy) {
   GLint ix,startX=(GLint)(x-xAdj);GLfloat cv=0.0F;
   while(startX<MAX_WIDTH){cv=aa_tri_cov(pMin,pMid,pMax,startX,iy);if(cv>0.0F)break;startX++;}
   for(ix=startX;cv>0.0F;){aa_pixel(c,&r,ix,iy,cv);ix++;cv=aa_tri_cov(pMin,pMid,pMax,ix,iy);}
  }
 }
 else { /* scan right to left */
  const GLfloat dxdy=majDx/majDy,xAdj=dxdy>0.0F?dxdy:0.0F;GLfloat x=pMin[0]-(yMin-(GLfloat)iyMin)*dxdy;
  for(iy=iyMin;iy<iyMax;iy++,x+=dxdy) {
   GLint ix,startX=(GLint)(x+xAdj);GLfloat cv=0.0F;
   if(startX>=ctx->DrawBuffer->_Xmax)startX=ctx->DrawBuffer->_Xmax-1;
   while(startX>0){cv=aa_tri_cov(pMin,pMax,pMid,startX,iy);if(cv>0.0F)break;startX--;}
   for(ix=startX;cv>0.0F;){aa_pixel(c,&r,ix,iy,cv);ix--;cv=aa_tri_cov(pMin,pMax,pMid,ix,iy);}
  }
 }
 aa_run(c,&r);quad_flush(c);
}
static void vxgl_draw_line(vxgl_ctx *c,vxglVertexPtr v0,vxglVertexPtr v1) {
 const vxglVertex *v[2];if(c->twoside_stencil)vxgl_stencil_face(c,0);
 if(c->aa_line){aa_line(c,v0,v1);return;} /* v10.321 */
 /* v10.300: stippled lines and textured one-pixel lines as runs too (ACCEL_OPENGL_LINES); plain one-pixel lines are the engine's own */
 if(c->wide_line>1.0F||c->stip_on||(c->tex_units&&vxgl_gl_lines)){wide_line(c,v0,v1,c->wide_line>1.0F?(GLint)c->wide_line:1);return;}
 v[0]=v0;v[1]=v1;hw_prim(c,v,2);
}
int vxgl_gl_lines=1,vxgl_gl_points=1; /* v10.300: ACCEL_OPENGL_LINES, ACCEL_OPENGL_POINTS */
static void vxgl_draw_point(vxgl_ctx *c,vxglVertexPtr v0) {
 const vxglVertex *v[1];GLcontext *ctx=&c->gl;if(c->twoside_stencil)vxgl_stencil_face(c,0);
 if(c->aa_point){aa_point(c,v0);return;} /* v10.321 */
 if(c->wide_point>1.0F||(c->wide_point>0.0F&&c->tex_units)||c->point_atten||c->point_sprite){
  /* v10.296 (host model: a size 9 point at a whole y came out a row low): the pixels as swrast picks them (s_pointtemp.h, not smooth),
   * worked out in GL's window coordinates (y up; the engine's y is h - 0.5 - y): odd sizes truncate x - r and x + r, even ones start
   * at (int)(x + 0.5) - r. The square's edges are on half pixels around exactly those pixel centres.
   * v10.300: an attenuated point's size is its vertex's (Mesa's point attenuation stage), clamped to GL_POINT_SIZE_MIN/MAX and the
   * implementation's range; a point sprite's units with GL_COORD_REPLACE get s = 0.5 + (x - wx) / size and t = 0.5 -+ (y - wy) / size
   * across it (UPPER_LEFT or LOWER_LEFT origin), set at the square's corners (linear across it, as swrast's per pixel) */
  GLfloat size=ctx->Point._Size,wx=v0->v.x+0.5F,wy=vxgl_wy(c,v0->v.y),xy[8],l,rt,t,b;GLint sz,r,x0,x1,y0,y1;
  if(IS_INF_OR_NAN(wx+wy)||wx<-1e6F||wx>1e6F||wy<-1e6F||wy>1e6F)return;
  if(c->point_atten){struct vertex_buffer *VB=&TNL_CONTEXT(ctx)->vb;const GLvector4f *ps=VB->AttribPtr[_TNL_ATTRIB_POINTSIZE];GLuint idx=(GLuint)(((const GLubyte*)v0-c->verts)/c->vertex_size);
   if(ps&&ps->data&&idx<VB->Count)size=*(const GLfloat*)((const GLubyte*)ps->data+idx*ps->stride);
   size=CLAMP(size,ctx->Point.MinSize,ctx->Point.MaxSize);}
  size=CLAMP(size,ctx->Const.MinPointSize,ctx->Const.MaxPointSize);
  sz=(GLint)(size+0.5F);if(sz<1)sz=1;r=sz/2;
  if(sz&1){x0=(GLint)(wx-(GLfloat)r);x1=(GLint)(wx+(GLfloat)r);y0=(GLint)(wy-(GLfloat)r);y1=(GLint)(wy+(GLfloat)r);}
  else {x0=(GLint)(wx+0.5F)-r;x1=x0+sz-1;y0=(GLint)(wy+0.5F)-r;y1=y0+sz-1;}
  l=(GLfloat)x0-0.5F;rt=(GLfloat)x1+0.5F;t=vxgl_sy(c,(GLfloat)(y1+1));b=vxgl_sy(c,(GLfloat)y0); /* v10.321: through vxgl_sy */
  xy[0]=l;xy[1]=t;xy[2]=rt;xy[3]=t;xy[4]=l;xy[5]=b;xy[6]=rt;xy[7]=b;
  if(c->point_sprite&&c->tex_units){static vxglVertex q[4];GLuint k,st,o,base=(c->specoffset?c->specoffset:c->coloroffset)+1,n=c->vertex_size;
   GLfloat gx[4],gy[4]; /* the corners in GL's window coordinates: top left, top right, bottom left, bottom right */
   gx[0]=gx[2]=(GLfloat)x0;gx[1]=gx[3]=(GLfloat)(x1+1);gy[0]=gy[1]=(GLfloat)(y1+1);gy[2]=gy[3]=(GLfloat)y0;
   for(k=0;k<4;k++){memcpy(&q[k],v0,n>sizeof(q[k])?sizeof(q[k]):n);
    /* v10.330 (review #38): each set at its own offset (choose_vertex_state: three floats for set 0 with a cube, volume or projective
     * layout and for set 1 with tex_set3, two otherwise); base + 2 st overwrote set 0's r or q with set 1's s, t */
    for(st=0,o=base;st<(GLuint)c->tex_sets;o+=((!st&&c->tex_layout)||(st==1&&c->tex_set3))?3U:2U,st++){GLuint u=(GLuint)c->tex_unit_of[st];GLfloat sv,tv;if(!ctx->Point.CoordReplace[u]||o+1>=16||(o+2)*4>n)continue;
     sv=0.5F+(gx[k]-wx)/size;tv=ctx->Point.SpriteOrigin==GL_LOWER_LEFT?0.5F+(gy[k]-wy)/size:0.5F-(gy[k]-wy)/size;
     sv+=1.0F/8192.0F;tv+=1.0F/8192.0F; /* a centre at exactly 0 (a point on a half pixel) stays inside: interpolated it came out a hair below and wrapped (host model: the far row) */
     q[k].f[o]=sv;q[k].f[o+1]=tv;}}
   hw_quad(c,&q[0],&q[1],&q[2],&q[3],xy);quad_flush(c);return;}
  hw_quad(c,v0,v0,v0,v0,xy);quad_flush(c);return;}
 v[0]=v0;hw_prim(c,v,1);
}

/* swrast for the odd primitive while the rest goes to the engine (not used yet: whole-pipeline fallbacks only) */
#define LINE_FALLBACK (0)
#define POINT_FALLBACK (0)
#define TRI_FALLBACK (0)
#define ANY_FALLBACK_FLAGS (POINT_FALLBACK|LINE_FALLBACK|TRI_FALLBACK)
#define ANY_RASTER_FLAGS (DD_TRI_LIGHT_TWOSIDE|DD_TRI_OFFSET|DD_TRI_UNFILLED)

#define TRI(a,b,c_) do{vxgl_draw_triangle(vctx,a,b,c_);}while(0)
#define QUAD(a,b,c_,d) do{vxgl_draw_triangle(vctx,b,c_,d);vxgl_draw_triangle(vctx,a,b,d);}while(0)
#define LINE(v0,v1) do{vxgl_draw_line(vctx,v0,v1);}while(0)
#define POINT(v0) do{vxgl_draw_point(vctx,v0);}while(0)

#define VXGL_OFFSET_BIT 0x01
#define VXGL_TWOSIDE_BIT 0x02
#define VXGL_UNFILLED_BIT 0x04
#define VXGL_FLAT_BIT 0x08
#define VXGL_MAX_TRIFUNC 0x10
static struct {tnl_points_func points;tnl_line_func line;tnl_triangle_func triangle;tnl_quad_func quad;} rast_tab[VXGL_MAX_TRIFUNC];

#define DO_FALLBACK 0
#define DO_OFFSET   (IND & VXGL_OFFSET_BIT)
#define DO_UNFILLED (IND & VXGL_UNFILLED_BIT)
#define DO_TWOSIDE  (IND & VXGL_TWOSIDE_BIT)
#define DO_FLAT     (IND & VXGL_FLAT_BIT)
#define DO_TRI 1
#define DO_QUAD 1
#define DO_LINE 1
#define DO_POINTS 1
#define DO_FULL_QUAD 1
#define HAVE_RGBA 1
#define HAVE_SPEC 1
#define HAVE_BACK_COLORS 0
#define HAVE_HW_FLATSHADE 0
#define VERTEX vxglVertex
#define TAB rast_tab
#define DEPTH_SCALE vctx->offset_scale
#define UNFILLED_TRI unfilled_tri
#define UNFILLED_QUAD unfilled_quad
#define VERT_X(_v) _v->v.x
#define VERT_Y(_v) _v->v.y
#define VERT_Z(_v) _v->v.z
/* t_dd_tritmp's facing = AREA_IS_CCW(cc) ^ _FrontBit, 1 = back. On the y-flipped surface a GL counter-clockwise (front) triangle
 * has cc < 0, so this must be false for it: (a > 0), as in the other y-flipping drivers (unichrome) (host test scene 10) */
#define AREA_IS_CCW(a) (vctx->vp_flip?(a>0):(a<0)) /* v10.321: a framebuffer object's surface is not flipped (vp_flip: the viewport the vertices were made with) */
#define GET_VERTEX(e) ((vxglVertex*)(vctx->verts+(e)*vctx->vertex_size))
#define VERT_SET_RGBA(v,c_) do{GLubyte *col=(GLubyte*)&((v)->ui[coloroffset]);UNCLAMPED_FLOAT_TO_UBYTE(col[2],(c_)[0]);UNCLAMPED_FLOAT_TO_UBYTE(col[1],(c_)[1]);UNCLAMPED_FLOAT_TO_UBYTE(col[0],(c_)[2]);UNCLAMPED_FLOAT_TO_UBYTE(col[3],(c_)[3]);}while(0)
#define VERT_COPY_RGBA(v0,v1) v0->ui[coloroffset]=v1->ui[coloroffset]
#define VERT_SET_SPEC(v,c_) do{if(specoffset){GLubyte *col=(GLubyte*)&((v)->ui[specoffset]);UNCLAMPED_FLOAT_TO_UBYTE(col[2],(c_)[0]);UNCLAMPED_FLOAT_TO_UBYTE(col[1],(c_)[1]);UNCLAMPED_FLOAT_TO_UBYTE(col[0],(c_)[2]);}}while(0)
#define VERT_COPY_SPEC(v0,v1) do{if(specoffset){v0->ub4[specoffset][0]=v1->ub4[specoffset][0];v0->ub4[specoffset][1]=v1->ub4[specoffset][1];v0->ub4[specoffset][2]=v1->ub4[specoffset][2];}}while(0)
#define VERT_SAVE_RGBA(idx) color[idx]=v[idx]->ui[coloroffset]
#define VERT_RESTORE_RGBA(idx) v[idx]->ui[coloroffset]=color[idx]
#define VERT_SAVE_SPEC(idx) if(specoffset)spec[idx]=v[idx]->ui[specoffset]
#define VERT_RESTORE_SPEC(idx) if(specoffset)v[idx]->ui[specoffset]=spec[idx]
#define LOCAL_VARS(n) vxgl_ctx *vctx=VXGL_CTX(ctx);GLuint color[n],spec[n];GLuint coloroffset=vctx->coloroffset;GLuint specoffset=vctx->specoffset;(void)color;(void)spec;(void)coloroffset;(void)specoffset;

static const GLenum hwPrim[GL_POLYGON+2]={GL_POINTS,GL_LINES,GL_LINES,GL_LINES,GL_TRIANGLES,GL_TRIANGLES,GL_TRIANGLES,GL_TRIANGLES,GL_TRIANGLES,GL_TRIANGLES,GL_POLYGON+1};
#define RASTERIZE(x) vxgl_raster_primitive(ctx,x,hwPrim[x])
#define RENDER_PRIMITIVE vctx->render_prim
static void vxgl_raster_primitive(GLcontext *ctx,GLenum glprim,GLenum hwprim) {VXGL_CTX(ctx)->render_prim=glprim;VXGL_CTX(ctx)->hw_prim=hwprim;}
#define TAG(x) x
#define IND VXGL_FLAT_BIT
#include "tnl_dd/t_dd_unfilled.h"
#undef IND
#undef RASTERIZE

#define RASTERIZE(x)
#define IND (0)
#define TAG(x) x
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_OFFSET_BIT)
#define TAG(x) x##_offset
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_TWOSIDE_BIT)
#define TAG(x) x##_twoside
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_TWOSIDE_BIT|VXGL_OFFSET_BIT)
#define TAG(x) x##_twoside_offset
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_UNFILLED_BIT)
#define TAG(x) x##_unfilled
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_OFFSET_BIT|VXGL_UNFILLED_BIT)
#define TAG(x) x##_offset_unfilled
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_TWOSIDE_BIT|VXGL_UNFILLED_BIT)
#define TAG(x) x##_twoside_unfilled
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_TWOSIDE_BIT|VXGL_OFFSET_BIT|VXGL_UNFILLED_BIT)
#define TAG(x) x##_twoside_offset_unfilled
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_FLAT_BIT)
#define TAG(x) x##_flat
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_FLAT_BIT|VXGL_OFFSET_BIT)
#define TAG(x) x##_flat_offset
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_FLAT_BIT|VXGL_TWOSIDE_BIT)
#define TAG(x) x##_flat_twoside
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_FLAT_BIT|VXGL_TWOSIDE_BIT|VXGL_OFFSET_BIT)
#define TAG(x) x##_flat_twoside_offset
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_FLAT_BIT|VXGL_UNFILLED_BIT)
#define TAG(x) x##_flat_unfilled
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_FLAT_BIT|VXGL_OFFSET_BIT|VXGL_UNFILLED_BIT)
#define TAG(x) x##_flat_offset_unfilled
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_FLAT_BIT|VXGL_TWOSIDE_BIT|VXGL_UNFILLED_BIT)
#define TAG(x) x##_flat_twoside_unfilled
#include "tnl_dd/t_dd_tritmp.h"
#define IND (VXGL_FLAT_BIT|VXGL_TWOSIDE_BIT|VXGL_OFFSET_BIT|VXGL_UNFILLED_BIT)
#define TAG(x) x##_flat_twoside_offset_unfilled
#include "tnl_dd/t_dd_tritmp.h"

static void init_rast_tab(void) {
 init();init_offset();init_twoside();init_twoside_offset();init_unfilled();init_offset_unfilled();init_twoside_unfilled();init_twoside_offset_unfilled();
 init_flat();init_flat_offset();init_flat_twoside();init_flat_twoside_offset();init_flat_unfilled();init_flat_offset_unfilled();init_flat_twoside_unfilled();
 init_flat_twoside_offset_unfilled();
}

/* the plain path: points, lines and triangles of the vertex buffer straight to the core (t_vb_rendertmp.h) */
#undef LOCAL_VARS
#define IND 0
#define V(x) ((vxglVertex*)(vertptr+(x)*vertsize))
#define RENDER_POINTS(start,count) for(;start<count;start++)POINT(V(ELT(start)));
#define RENDER_LINE(v0,v1) LINE(V(v0),V(v1))
#define RENDER_TRI(v0,v1,v2) TRI(V(v0),V(v1),V(v2))
#define RENDER_QUAD(v0,v1,v2,v3) QUAD(V(v0),V(v1),V(v2),V(v3))
#define INIT(x) vxgl_raster_primitive(ctx,x,hwPrim[x])
#define LOCAL_VARS vxgl_ctx *vctx=VXGL_CTX(ctx);GLubyte *vertptr=vctx->verts;const GLuint vertsize=vctx->vertex_size;const GLuint *const elt=TNL_CONTEXT(ctx)->vb.Elts;(void)elt;
#define RESET_STIPPLE vctx->stip_count=0 /* v10.300: where tnl resets swrast's counter */
#define RESET_OCCLUSION
#define PRESERVE_VB_DEFS
#define ELT(x) x
#define TAG(x) vxgl_##x##_verts
#include "tnl/t_vb_rendertmp.h"
#undef ELT
#undef TAG
#define TAG(x) vxgl_##x##_elts
#define ELT(x) elt[x]
#include "tnl/t_vb_rendertmp.h"
#undef ELT
#undef TAG
#undef NEED_EDGEFLAG_SETUP
#undef EDGEFLAG_GET
#undef EDGEFLAG_SET
#undef RESET_OCCLUSION

/* v10.323: triangle and quad strips of the plain path go to the core as strips (vx_d3d_gl_strip, ACCEL_OPENGL_STRIPS): one vertex a triangle in
 * one run, the same triangles as Mesa's (a quad strip pair-swapped: v1 v0 v3 v2 ..., each quad's diagonal as RENDER_QUAD's). Two-sided
 * stencil, smoothing and culling both faces keep the triangle-by-triangle path. */
int vxgl_gl_strips=1;
int vxgl_gl_fan=1; /* .505: ACCEL_OPENGL_GLFAN (vx_d3d_gl_fan: a polygon's, fan's or quads' strips in one core call) */
int vxgl_gl_snap=1; /* v10.324: ACCEL_OPENGL_SNAP */
#define STRIP_CHUNK 64
static void (*plain_strip[2][2])(GLcontext *,GLuint,GLuint,GLuint); /* [elts][quad] Mesa's template functions */
/* a strip of n vertex numbers to the core, in chunks of STRIP_CHUNK overlapping by two, the triangle parity carried on (quad: a quad strip's
 * pairs, parity fixed) */
static void strip_send_t(vxgl_ctx *c,const uint32_t *v,GLuint n,int quad);
static void strip_send(vxgl_ctx *c,const uint32_t *v,GLuint n,int quad) {VXT_BEGIN;strip_send_t(c,v,n,quad);VXT_END(2);} /* .506 */
static void strip_send_t(vxgl_ctx *c,const uint32_t *v,GLuint n,int quad) {
 uint32_t s[STRIP_CHUNK],rc;GLuint i,k,m;
 for(i=0;i+2<n;i+=STRIP_CHUNK-2) {
  m=n-i<STRIP_CHUNK?n-i:STRIP_CHUNK;
  for(k=0;k<m;k++)s[k]=v[i+k];
  {VXGL_PROF_BEGIN;rc=vx_d3d_gl_strip(&c->core,&c->sv,s,m,quad?1:(int)(i&1));VXGL_PROF_END(2);}
  c->stats[0]+=m-2;
  if(rc){static int logged;if(logged++<8)vxgl_log("strip refused: %08lX\n",(unsigned long)rc);}
  if(m<STRIP_CHUNK)break;
 }
}
#define POLY_MAX 512
static uint32_t strip_v[POLY_MAX];
static void strip_run(GLcontext *ctx,GLuint start,GLuint count,GLuint flags,int elts,int quad) {
 vxgl_ctx *c=VXGL_CTX(ctx);const GLuint *elt=TNL_CONTEXT(ctx)->vb.Elts;GLuint n,i,k,m;
 if(!vxgl_gl_strips||c->twoside_stencil||c->aa_tri||c->cull_all){plain_strip[elts][quad](ctx,start,count,flags);return;}
 vxgl_raster_primitive(ctx,quad?GL_QUAD_STRIP:GL_TRIANGLE_STRIP,GL_TRIANGLES);
 n=count>start?count-start:0;if(quad)n&=~1U;
 if(n<(quad?4U:3U))return;
 /* chunks of POLY_MAX vertices overlapping by two (one pair of a quad strip); POLY_MAX - 2 is even: the parity goes on */
 for(i=0;i+2<n;i+=POLY_MAX-2) {
  m=n-i<POLY_MAX?n-i:POLY_MAX;
  for(k=0;k<m;k++){GLuint j=i+(quad?(k^1):k);strip_v[k]=elts?elt[start+j]:start+j;}
  strip_send(c,strip_v,m,quad);
  if(m<POLY_MAX)break;
 }
}
/* v10.348: a filled polygon (GL_POLYGON, a polygon the clipper cut), a triangle fan and quads to the core as short strips: the fan's triangles exactly
 * (0 k k+1), three to a strip, k k+1 0 k+2 k+3 (the strip's first triangle k k+1 0 and its third 0 k+2 k+3 as they are, its second swapped
 * by the strip's parity to 0 k+1 k+2), each wound as the fan. Was a triangle at a time through the core's general path; Quake 2 draws its
 * world this way. Flat shading (Quake 2's default) only when every vertex has the first one's colours (then flat is smooth); otherwise,
 * and with two-sided stencil, smoothing or both faces culled, Mesa's template as before. ACCEL_OPENGL_STRIPS = 0: off with the strips. */
static void (*plain_poly[2][3])(GLcontext *,GLuint,GLuint,GLuint),(*flat_poly[2][3])(GLcontext *,GLuint,GLuint,GLuint); /* [elts][polygon, fan, quads] */
static int poly_same_colour(vxgl_ctx *c,const GLuint *v,GLuint n) {
 const GLuint *p0=(const GLuint*)(c->verts+v[0]*c->vertex_size);GLuint co=c->coloroffset,so=c->specoffset,i;
 for(i=1;i<n;i++){const GLuint *p=(const GLuint*)(c->verts+v[i]*c->vertex_size);if(p[co]!=p0[co]||(so&&p[so]!=p0[so]))return 0;}
 return 1;
}
static int fan_strips(vxgl_ctx *c,const GLuint *v,GLuint n) {
 GLuint k;uint32_t s[5];
 if(!vxgl_gl_strips||c->twoside_stencil||c->aa_tri||c->cull_all||n<3)return 0;
 if(c->render_index&&(c->render_index!=VXGL_FLAT_BIT||!poly_same_colour(c,v,n)))return 0;
 if(vxgl_gl_fan){uint32_t rc;VXGL_PROF_BEGIN;VXT_BEGIN;rc=vx_d3d_gl_fan(&c->core,&c->sv,(const uint32_t*)v,n,0);VXT_END(2);VXGL_PROF_END(2);c->stats[0]+=n-2;
  if(rc){static int logged;if(logged++<8)vxgl_log("strip refused: %08lX\n",(unsigned long)rc);}return 1;} /* .505 (ACCEL_OPENGL_GLFAN) */
 for(k=1;k+1<n;k+=3){GLuint m=0;s[m++]=v[k];s[m++]=v[k+1];s[m++]=v[0];if(k+2<n){s[m++]=v[k+2];if(k+3<n)s[m++]=v[k+3];}strip_send(c,s,m,0);}
 return 1;
}
static GLuint poly_idx[POLY_MAX];
/* mode 0 a polygon, 1 a triangle fan, 2 quads: each quad a b c d as the strip a b d c, Mesa's two triangles b c d and a b d (QUAD above) */
static int quads_strips(vxgl_ctx *c,const GLuint *v,GLuint n) {
 GLuint q;uint32_t s[4];
 if(!vxgl_gl_strips||c->twoside_stencil||c->aa_tri||c->cull_all)return 0;
 if(c->render_index)for(q=0;q+3<n;q+=4)if(c->render_index!=VXGL_FLAT_BIT||!poly_same_colour(c,v+q,4))return 0;
 if(vxgl_gl_fan&&n>=4){uint32_t rc;VXGL_PROF_BEGIN;VXT_BEGIN;rc=vx_d3d_gl_fan(&c->core,&c->sv,(const uint32_t*)v,n,1);VXT_END(2);VXGL_PROF_END(2);c->stats[0]+=(n/4)*2;
  if(rc){static int logged;if(logged++<8)vxgl_log("strip refused: %08lX\n",(unsigned long)rc);}return 1;} /* .505 */
 for(q=0;q+3<n;q+=4){s[0]=v[q];s[1]=v[q+1];s[2]=v[q+3];s[3]=v[q+2];strip_send(c,s,4,0);}
 return 1;
}
static int hf_prims(GLcontext *ctx,GLuint start,GLuint count,int elts,int mode);static int hf_ok(vxgl_ctx *c);
static int pb_poly(GLcontext *ctx,GLuint start,GLuint count);static void pb_flush(vxgl_ctx *c);extern int _tnl_vx855_clipwin;static GLuint pb_nt;
static void poly_run(GLcontext *ctx,GLuint start,GLuint count,GLuint flags,int elts,int mode) {
 vxgl_ctx *c=VXGL_CTX(ctx);const GLuint *elt=TNL_CONTEXT(ctx)->vb.Elts;GLuint n=count>start?count-start:0,i;
 /* v10.485: a clipped polygon of a split strip, flat on the engine's flat shading: gathered (pb_poly); anything else draws what is gathered first */
 if(_tnl_vx855_clipwin&&mode==0&&elts&&c->render_index==VXGL_FLAT_BIT&&hf_ok(c)&&pb_poly(ctx,start,count))return;
 if(pb_nt)pb_flush(c);
 if(n>=3&&n<=POLY_MAX){for(i=0;i<n;i++)poly_idx[i]=elts?elt[start+i]:start+i;
  vxgl_raster_primitive(ctx,mode==2?GL_QUADS:mode?GL_TRIANGLE_FAN:GL_POLYGON,GL_TRIANGLES);if(mode==2?quads_strips(c,poly_idx,n):fan_strips(c,poly_idx,n))return;}
 if(c->render_index==VXGL_FLAT_BIT&&hf_prims(ctx,start,count,elts,mode))return; /* v10.483: their colours differ: the engine's flat shading */
 (c->render_index?flat_poly:plain_poly)[elts][mode](ctx,start,count,flags);
}
static void poly_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {poly_run(ctx,start,count,flags,0,0);}
static void poly_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {poly_run(ctx,start,count,flags,1,0);}
static void fan_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {poly_run(ctx,start,count,flags,0,1);}
static void fan_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {poly_run(ctx,start,count,flags,1,1);}
static void quads_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {poly_run(ctx,start,count,flags,0,2);}
static void quads_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {poly_run(ctx,start,count,flags,1,2);}
static tnl_render_func flat_tab_verts[GL_POLYGON+2],flat_tab_elts[GL_POLYGON+2]; /* v10.348: Mesa's tables, the polygon ours (flat shading) */
static void strip_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {strip_run(ctx,start,count,flags,0,0);}
static void quadstrip_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {strip_run(ctx,start,count,flags,0,1);}
static void strip_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {strip_run(ctx,start,count,flags,1,0);}
static void quadstrip_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {strip_run(ctx,start,count,flags,1,1);}
/* v10.480: flat-shaded triangle and quad strips (board: Lattice, lit and flat, 1.8 fps; another session's review: each such strip went through
 * Mesa's flat template a triangle at a time, copying the provoking vertex's colour onto the other two and back). When every vertex of the
 * strip has the first one's colours (primary, and secondary with its fog byte), flat is smooth: the strip goes to the core as one, as the
 * smooth path's (strip_run). Otherwise, and with two-sided stencil, smoothing or both faces culled, Mesa's flat template as before.
 * ACCEL_OPENGL_FLATSTRIP = 0: always the template. vxgl_flat_stat: [0] strips sent whole, [1] their triangles, [2] strips left to the
 * template (colours differ), [3] their triangles (VXGL_FlatStats; the context's end in the log). */
int vxgl_gl_flatstrips=1;uint32_t vxgl_flat_stat[10]; /* v10.484: [8] of [4] the engine's strips (HWFLATSTRIP), [9] their triangles; v10.483: [6] other flat primitives (lists, fans, polygons, quads) on the engine's flat shading, [7] their triangles */
/* v10.481 (board, Lattice v10.480: 1,057,235 flat strips left to the template against 2,033 sent whole: its lit colours differ along every
 * strip; the template and its calls 8% of the drawing thread, the core's triangle-at-a-time work about 21%): a flat strip whose colours
 * differ goes to the core as independent triangles from a buffer of their own, each triangle's three vertices copies of the strip's with
 * the provoking vertex's colours (primary; secondary red, green and blue with each vertex's own fog byte, as the template's VERT_COPY_SPEC,
 * only when there is a secondary colour array) in the template's order: a triangle strip's triangle j-2+parity, j-1-parity, j; a quad
 * strip's quad j-1, j-3, j-2, j as the two triangles j-3 j-2 j and j-1 j-3 j, coloured from j. 64 triangles a call (vx_d3d_gl_tris);
 * the store named again after. ACCEL_OPENGL_FLATBATCH = 0: the template as v10.480. [4] strips batched, [5] their triangles. */
int vxgl_gl_flatbatch=1;
#define FB_TRIS 64
static GLubyte fb_buf[FB_TRIS*3*64];static GLuint fb_n;
static void quad_flush(vxgl_ctx *c);
static void fb_send(vxgl_ctx *c) {
 uint32_t rc;
 if(!fb_n)return;
 vx_d3d_gl_begin(fb_buf,c->vertex_size,c->fvf);vx_d3d_gl_count(fb_n*3);
 {VXGL_PROF_BEGIN;rc=vx_d3d_gl_tris(&c->core,&c->sv,0,fb_n);VXGL_PROF_END(2);}
 c->stats[0]+=fb_n;fb_n=0;
 vx_d3d_gl_begin(c->verts,c->vertex_size,c->fvf);vx_d3d_gl_count(verts_cap(c));
 if(rc){static int logged;if(logged++<8)vxgl_log("flat triangles refused: %08lX\n",(unsigned long)rc);}
}
static void fb_tri(vxgl_ctx *c,GLuint a,GLuint b,GLuint p,int spec) {
 GLuint n=c->vertex_size,co=c->coloroffset,so=c->specoffset;GLubyte *d=fb_buf+fb_n*3*n;const GLubyte *pv=c->verts+p*n;
 memcpy(d,c->verts+a*n,n);memcpy(d+n,c->verts+b*n,n);memcpy(d+2*n,pv,n);
 ((GLuint*)d)[co]=((const GLuint*)pv)[co];((GLuint*)(d+n))[co]=((const GLuint*)pv)[co];
 if(spec){GLuint m=((const GLuint*)pv)[so]&0x00FFFFFFU;((GLuint*)d)[so]=(((GLuint*)d)[so]&0xFF000000U)|m;((GLuint*)(d+n))[so]=(((GLuint*)(d+n))[so]&0xFF000000U)|m;}
 if(++fb_n==FB_TRIS)fb_send(c);
}
static void flat_batch(GLcontext *ctx,GLuint start,GLuint count,int elts,int quad,GLuint tris) {
 vxgl_ctx *c=VXGL_CTX(ctx);const GLuint *elt=TNL_CONTEXT(ctx)->vb.Elts;GLuint j,parity=0;int spec=c->specoffset&&TNL_CONTEXT(ctx)->vb.SecondaryColorPtr[0];
 #define FE(x) (elts?elt[x]:(x))
 quad_flush(c);vx_d3d_gl_unjoin();
 vxgl_raster_primitive(ctx,quad?GL_QUAD_STRIP:GL_TRIANGLE_STRIP,GL_TRIANGLES);
 if(vx_d3d_gl_hwflat) { /* v10.481: the engine's flat shading (ACCEL_OPENGL_HWFLAT): each triangle turned to start at its provoking vertex
   (p a b: the same winding), from the store itself, no copies */
  static uint32_t ix[FB_TRIS*3];GLuint m=0;uint32_t rc=0;
  #define FH(a,b,p) do{ix[m*3]=(p);ix[m*3+1]=(a);ix[m*3+2]=(b);if(++m==FB_TRIS){VXGL_PROF_BEGIN;rc|=vx_d3d_gl_tris(&c->core,&c->sv,ix,m);VXGL_PROF_END(2);c->stats[0]+=m;m=0;}}while(0)
  if(!quad)for(j=start+2;j<count;j++,parity^=1)FH(FE(j-2+parity),FE(j-1-parity),FE(j));
  else for(j=start+3;j<count;j+=2){FH(FE(j-3),FE(j-2),FE(j));FH(FE(j-1),FE(j-3),FE(j));}
  if(m){VXGL_PROF_BEGIN;rc|=vx_d3d_gl_tris(&c->core,&c->sv,ix,m);VXGL_PROF_END(2);c->stats[0]+=m;}
  #undef FH
  if(rc){static int logged;if(logged++<8)vxgl_log("flat triangles refused: %08lX\n",(unsigned long)rc);}
 } else {
 if(!quad)for(j=start+2;j<count;j++,parity^=1)fb_tri(c,FE(j-2+parity),FE(j-1-parity),FE(j),spec);
 else for(j=start+3;j<count;j+=2){fb_tri(c,FE(j-3),FE(j-2),FE(j),spec);fb_tri(c,FE(j-1),FE(j-3),FE(j),spec);}
 fb_send(c);}
 #undef FE
 vxgl_flat_stat[4]++;vxgl_flat_stat[5]+=tris;
}
static void (*flat_strip_mesa[2][2])(GLcontext *,GLuint,GLuint,GLuint); /* [elts][quad] Mesa's flat template */
/* v10.482 (board, VXFLAT v1.1: the engine's flat shading drew the template's pixels in every scene, lit and fogged too, from triangle lists):
 * ACCEL_OPENGL_HWFLATSTRIP (0 by default; VXFLAT tries both) sends a flat triangle strip whose colours differ as one strip on the engine's
 * flat shading, its colours laid out for the engine's rule for strips, still unproven: 2 = each triangle takes its last strip vertex's
 * colours (GL's own rule: the strip goes as it is, no copy); 1 = its first (each vertex given the colours of the vertex two on, in a copy).
 * The core turns any triangle its strip run does not take to start at that vertex (vx_d3d_gl_flatpv). */
int vxgl_gl_hwflatstrip=1; /* v10.483: rule 1 by default (board) */
int vxgl_gl_flatnocopy=1; /* v10.492: ACCEL_OPENGL_FLATNOCOPY */
static void flat_hwstrip(GLcontext *ctx,GLuint start,GLuint count,int elts,int quad) {
 vxgl_ctx *c=VXGL_CTX(ctx);const GLuint *elt=TNL_CONTEXT(ctx)->vb.Elts;
 if(vxgl_gl_hwflatstrip==2&&!quad){vx_d3d_gl_flatpv=2;strip_run(ctx,start,count,0,elts,0);vx_d3d_gl_flatpv=0;return;}
 if(vxgl_gl_flatnocopy){int spec=c->specoffset&&TNL_CONTEXT(ctx)->vb.SecondaryColorPtr[0]; /* v10.492: rule 1 without the copy: the core takes each vertex's
  * colours from the vertex two on (vx_d3d_gl_flatpv 3, a quad strip 4), the same words the copy below held */
  quad_flush(c);vx_d3d_gl_unjoin();vx_d3d_gl_flatspec=spec?1U:0U;vx_d3d_gl_flatpv=quad?4U:3U;strip_run(ctx,start,count,0,elts,quad);vx_d3d_gl_flatpv=0;vx_d3d_gl_flatspec=0;return;}
 /* rule 1 (board, VXFLAT v1.2: the engine's flat strip triangle takes its first strip vertex's colours): the strip in a copy, each vertex
  * given the colours of the vertex that colours its triangle in GL: a triangle strip's vertex k those of k + 2; v10.483: a quad strip goes
  * pair-swapped (v1 v0 v3 v2 ..., as strip_run sends it) and both vertices of a pair take the colours of the quad's last vertex (GL's
  * provoking vertex for quads), the vertex two pairs on in the swapped order's first place */
 {GLuint n=count-start,vs=c->vertex_size,co=c->coloroffset,so=c->specoffset,cap=((GLuint)(sizeof(fb_buf)/vs)<POLY_MAX?(GLuint)(sizeof(fb_buf)/vs):POLY_MAX)&~1U,i,k,m;int spec=so&&TNL_CONTEXT(ctx)->vb.SecondaryColorPtr[0];
  static uint32_t hs_xy[2*POLY_MAX];uint32_t ps_n=0,ps_mode=0;const uint8_t *ps_base=0;const uint32_t *ps_xy=vx_d3d_gl_presnap_get(&ps_n,&ps_mode,&ps_base); /* v10.490 */
  #define HS_V(x) (c->verts+(elts?elt[start+(x)]:start+(x))*vs)
  if(quad)n&=~1U;
  quad_flush(c);vx_d3d_gl_unjoin();vxgl_raster_primitive(ctx,quad?GL_QUAD_STRIP:GL_TRIANGLE_STRIP,GL_TRIANGLES);
  for(i=0;i+2<n;i+=cap-2){ /* chunks overlapping by two, an even step: the parity (and a quad strip's pairs) go on */
   m=n-i<cap?n-i:cap;
   for(k=0;k<m;k++)memcpy(fb_buf+k*vs,HS_V(i+(quad?(k^1):k)),vs);
   for(k=0;k+2<m;k++){const GLuint *q=(const GLuint*)HS_V(i+(quad?((k&~1U)+3):k+2));GLuint *d=(GLuint*)(fb_buf+k*vs);
    d[co]=q[co];if(spec)d[so]=(d[so]&0xFF000000U)|(q[so]&0x00FFFFFFU);}
   for(k=0;k<m;k++)strip_v[k]=k;
   /* v10.490 (board, VXFLAT v2.0: no snapped vertex taken in any scene: the strip went to the core from this copy, not the store the pass's
    * words name): the pass's x y words go with the copy, vertex for vertex (a vertex past them, a clipper's: none for the chunk) */
   if(ps_xy&&ps_base==c->verts){GLuint ok=1,ix;for(k=0;k<m&&ok;k++){ix=elts?elt[start+i+(quad?(k^1):k)]:start+i+(quad?(k^1):k);if(ix<ps_n){hs_xy[2*k]=ps_xy[2*ix];hs_xy[2*k+1]=ps_xy[2*ix+1];}else ok=0;}
    vx_d3d_gl_presnap(ok?hs_xy:0,m,ps_mode,fb_buf);}
   vx_d3d_gl_begin(fb_buf,vs,c->fvf);vx_d3d_gl_count(m);vx_d3d_gl_flatpv=1;strip_send(c,strip_v,m,quad);vx_d3d_gl_flatpv=0;
   if(ps_xy&&ps_base==c->verts)vx_d3d_gl_presnap(ps_xy,ps_n,ps_mode,ps_base);
   vx_d3d_gl_begin(c->verts,vs,c->fvf);vx_d3d_gl_count(verts_cap(c));
   if(m<cap)break;}
  #undef HS_V
 }
}
static void flat_strip_run(GLcontext *ctx,GLuint start,GLuint count,GLuint flags,int elts,int quad) {
 vxgl_ctx *c=VXGL_CTX(ctx);const GLuint *elt=TNL_CONTEXT(ctx)->vb.Elts;GLuint n=count>start?count-start:0,i,tris;
 if(quad)n&=~1U;
 tris=quad?(n>=4?n-2:0):(n>=3?n-2:0);
 if(tris&&vxgl_gl_flatstrips&&vxgl_gl_strips&&!c->twoside_stencil&&!c->aa_tri&&!c->cull_all) {
  const GLuint *p0=(const GLuint*)(c->verts+(elts?elt[start]:start)*c->vertex_size);GLuint co=c->coloroffset,so=c->specoffset;
  for(i=1;i<n;i++){const GLuint *p=(const GLuint*)(c->verts+(elts?elt[start+i]:start+i)*c->vertex_size);if(p[co]!=p0[co]||(so&&p[so]!=p0[so]))break;}
  if(i==n){vxgl_flat_stat[0]++;vxgl_flat_stat[1]+=tris;strip_run(ctx,start,count,flags,elts,quad);return;}
 }
 if(tris){vxgl_flat_stat[2]++;vxgl_flat_stat[3]+=tris;}
 if(tris&&!(quad&&vxgl_gl_hwflatstrip==2)&&vx_d3d_gl_hwflat&&vxgl_gl_hwflatstrip&&vxgl_gl_strips&&!c->twoside_stencil&&!c->aa_tri&&!c->cull_all&&c->vertex_size<=64&&c->core.state.rs[9]==1){flat_hwstrip(ctx,start,count,elts,quad);vxgl_flat_stat[4]++;vxgl_flat_stat[5]+=tris;vxgl_flat_stat[8]++;vxgl_flat_stat[9]+=tris;return;} /* v10.482; v10.483: quad strips too */
 if(tris&&vxgl_gl_flatbatch&&vxgl_gl_strips&&!c->twoside_stencil&&!c->aa_tri&&!c->cull_all&&c->vertex_size<=64){flat_batch(ctx,start,count,elts,quad,tris);return;} /* v10.481 */
 flat_strip_mesa[elts][quad](ctx,start,count,flags);
}
/* v10.483: flat triangles, fans, polygons and quads whose colours differ, on the engine's flat shading (each triangle the colours of its
 * first vertex, board-proven for lists): straight from the store as triangle lists, each triangle turned to start at its GL provoking
 * vertex (the same winding), in Mesa's template's order: triangles j-2 j-1 j (provoking j); a fan start j-1 j (j); a polygon j-1 j start
 * (start: a polygon's first vertex, GL's rule, the clipper's too); a quad a b c d as b c d and a b d (d). No colour copies, no template. */
static uint32_t hf_ix[FB_TRIS*3];static GLuint hf_m;static uint32_t hf_rc;
static int hf_ok(vxgl_ctx *c) {return vx_d3d_gl_hwflat&&vxgl_gl_flatbatch&&c->core.state.rs[9]==1&&c->render_index==VXGL_FLAT_BIT&&!c->twoside_stencil&&!c->aa_tri&&!c->cull_all;}
static void hf_flush(vxgl_ctx *c) {if(hf_m){VXGL_PROF_BEGIN;hf_rc|=vx_d3d_gl_tris(&c->core,&c->sv,hf_ix,hf_m);VXGL_PROF_END(2);c->stats[0]+=hf_m;vxgl_flat_stat[7]+=hf_m;hf_m=0;}}
static void hf_add(vxgl_ctx *c,GLuint p,GLuint a,GLuint b) {hf_ix[hf_m*3]=p;hf_ix[hf_m*3+1]=a;hf_ix[hf_m*3+2]=b;if(++hf_m==FB_TRIS)hf_flush(c);}
static void hf_end(vxgl_ctx *c) {hf_flush(c);if(hf_rc){static int logged;if(logged++<8)vxgl_log("flat triangles refused: %08lX\n",(unsigned long)hf_rc);}hf_rc=0;vxgl_flat_stat[6]++;}
/* mode 0 a polygon, 1 a fan, 2 quads, 3 triangles */
static int hf_prims(GLcontext *ctx,GLuint start,GLuint count,int elts,int mode) {
 vxgl_ctx *c=VXGL_CTX(ctx);const GLuint *elt=TNL_CONTEXT(ctx)->vb.Elts;GLuint j;
 #define HE(x) (elts?elt[x]:(x))
 if(!hf_ok(c)||count<start+3)return 0;
 quad_flush(c);vx_d3d_gl_unjoin();vxgl_raster_primitive(ctx,mode==3?GL_TRIANGLES:mode==2?GL_QUADS:mode?GL_TRIANGLE_FAN:GL_POLYGON,GL_TRIANGLES);
 if(mode==3)for(j=start+2;j<count;j+=3)hf_add(c,HE(j),HE(j-2),HE(j-1));
 else if(mode==2)for(j=start+3;j<count;j+=4){hf_add(c,HE(j),HE(j-2),HE(j-1));hf_add(c,HE(j),HE(j-3),HE(j-2));}
 else if(mode==1)for(j=start+2;j<count;j++)hf_add(c,HE(j),HE(start),HE(j-1));
 else for(j=start+2;j<count;j++)hf_add(c,HE(start),HE(j-1),HE(j));
 #undef HE
 hf_end(c);return 1;
}
/* v10.485: the clipper's flat polygons of a split strip (t_vb_render.c vx855_clip_strip, _tnl_vx855_clipwin) gathered: each polygon's vertices
 * copied out of the store (the clipper writes the next polygon's new vertices over this one's) with its triangles (start j-1 j, as hf_prims),
 * and all of them sent in one call when anything else is to be drawn (_tnl_vx855_clipflush, poly_run) or the buffer is full. */
#define PB_V 192
static GLubyte pb_buf[PB_V*64];static uint32_t pb_ix[FB_TRIS*3];static GLuint pb_nv;
static void pb_flush(vxgl_ctx *c) {
 uint32_t rc;
 if(!pb_nt)return;
 vx_d3d_gl_begin(pb_buf,c->vertex_size,c->fvf);vx_d3d_gl_count(pb_nv);
 {VXGL_PROF_BEGIN;rc=vx_d3d_gl_tris(&c->core,&c->sv,pb_ix,pb_nt);VXGL_PROF_END(2);}
 c->stats[0]+=pb_nt;pb_nt=pb_nv=0;
 vx_d3d_gl_begin(c->verts,c->vertex_size,c->fvf);vx_d3d_gl_count(verts_cap(c));
 if(rc){static int logged;if(logged++<8)vxgl_log("clipped flat polygons refused: %08lX\n",(unsigned long)rc);}
}
void vxgl_clipflush(GLcontext *ctx) {pb_flush(VXGL_CTX(ctx));}
static int pb_poly(GLcontext *ctx,GLuint start,GLuint count) {
 vxgl_ctx *c=VXGL_CTX(ctx);const GLuint *elt=TNL_CONTEXT(ctx)->vb.Elts;GLuint n=count>start?count-start:0,vs=c->vertex_size,i,b;
 if(n<3||n>PB_V||n-2>FB_TRIS||vs>64)return 0;
 if(pb_nv+n>PB_V||pb_nt+n-2>FB_TRIS)pb_flush(c);
 quad_flush(c);vx_d3d_gl_unjoin();vxgl_raster_primitive(ctx,GL_POLYGON,GL_TRIANGLES);
 b=pb_nv;for(i=0;i<n;i++)memcpy(pb_buf+(b+i)*vs,c->verts+elt[start+i]*vs,vs);
 for(i=2;i<n;i++){pb_ix[pb_nt*3]=b;pb_ix[pb_nt*3+1]=b+i-1;pb_ix[pb_nt*3+2]=b+i;pb_nt++;}
 pb_nv+=n;vxgl_flat_stat[6]++;vxgl_flat_stat[7]+=n-2;return 1;
}
static void (*flat_tris_mesa[2])(GLcontext *,GLuint,GLuint,GLuint);
static void flat_tris_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {if(!hf_prims(ctx,start,count,0,3))flat_tris_mesa[0](ctx,start,count,flags);}
static void flat_tris_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {if(!hf_prims(ctx,start,count,1,3))flat_tris_mesa[1](ctx,start,count,flags);}
static void flat_strip_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {flat_strip_run(ctx,start,count,flags,0,0);}
static void flat_quadstrip_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {flat_strip_run(ctx,start,count,flags,0,1);}
static void flat_strip_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {flat_strip_run(ctx,start,count,flags,1,0);}
static void flat_quadstrip_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {flat_strip_run(ctx,start,count,flags,1,1);}
uint32_t __stdcall VXGL_FlatStats(uint32_t *o,uint32_t clear) {int k;if(o)for(k=0;k<4;k++)o[k]=vxgl_flat_stat[k];if(clear)for(k=0;k<10;k++)vxgl_flat_stat[k]=0;return (uint32_t)vxgl_gl_flatstrips;}
/* v10.481: the six counts ([4] strips batched, [5] their triangles: of [2] and [3]); returns ACCEL_OPENGL_FLATSTRIP | FLATBATCH << 1 | EMIT << 2 */
extern int _tnl_vx855_emit;
/* v10.485: VXGL_ClipStats, the split strips' counts (t_vb_render.c: [0] strips split, [1] runs as strips, [2] their triangles, [3] triangles
 * inside alone, [4] triangles clipped); returns ACCEL_OPENGL_CLIPSTRIP, CLIPINT, LIGHTFAST and RSQRT as bits 0-3 */
uint32_t __stdcall VXGL_ClipStats(uint32_t *o,uint32_t clear) {
 extern unsigned long _tnl_vx855_clipstrip_stat[5];extern int _tnl_vx855_clipstrip,_mesa_vx855_lightfast,_mesa_vx855_rsqrt;int k;
 if(o)for(k=0;k<5;k++)o[k]=(uint32_t)_tnl_vx855_clipstrip_stat[k];if(clear)for(k=0;k<5;k++)_tnl_vx855_clipstrip_stat[k]=0;
 return (uint32_t)(_tnl_vx855_clipstrip!=0)|(uint32_t)(_mesa_vx855_lightfast!=0)<<2|(uint32_t)(_mesa_vx855_rsqrt!=0)<<3;
}
/* v10.484: VXGL_FlatStats10, the ten counts ([8] of the strips whose colours differ, those sent as the engine's strips, [9] their triangles; the
 * rest of [4] batched as triangles); returns as VXGL_FlatStats8 */
uint32_t __stdcall VXGL_FlatStats10(uint32_t *o,uint32_t clear) {int k;if(o)for(k=0;k<10;k++)o[k]=vxgl_flat_stat[k];if(clear)for(k=0;k<10;k++)vxgl_flat_stat[k]=0;return (uint32_t)vxgl_gl_flatstrips|(uint32_t)vxgl_gl_flatbatch<<1|(uint32_t)(_tnl_vx855_emit!=0)<<2|(uint32_t)(vx_d3d_gl_hwflat!=0)<<3|(uint32_t)vxgl_gl_hwflatstrip<<4;}
/* v10.483: VXGL_FlatStats8, the eight counts ([6] other flat primitives on the engine's flat shading, [7] their triangles) */
uint32_t __stdcall VXGL_FlatStats8(uint32_t *o,uint32_t clear) {int k;if(o)for(k=0;k<8;k++)o[k]=vxgl_flat_stat[k];if(clear)for(k=0;k<10;k++)vxgl_flat_stat[k]=0;return (uint32_t)vxgl_gl_flatstrips|(uint32_t)vxgl_gl_flatbatch<<1|(uint32_t)(_tnl_vx855_emit!=0)<<2|(uint32_t)(vx_d3d_gl_hwflat!=0)<<3|(uint32_t)vxgl_gl_hwflatstrip<<4;}
uint32_t __stdcall VXGL_FlatStats6(uint32_t *o,uint32_t clear) {int k;if(o)for(k=0;k<6;k++)o[k]=vxgl_flat_stat[k];if(clear)for(k=0;k<10;k++)vxgl_flat_stat[k]=0;return (uint32_t)vxgl_gl_flatstrips|(uint32_t)vxgl_gl_flatbatch<<1|(uint32_t)(_tnl_vx855_emit!=0)<<2;}
/* v10.500 (ACCEL_OPENGL_TRILIST; host bench, Quake 3's batches: GL_TRIANGLES from glDrawElements a triangle at a time through Mesa's template,
 * vxgl_draw_triangle, hw_prim's vertex numbers from pointers and vx_d3d_gl_prim): the plain path's triangle runs (a run of a draw, or of a
 * draw with clipping between the triangles the clipper takes: clip_elt_triangles) gathered 128 triangles at a time, their store numbers
 * straight from the elements, to vx_d3d_gl_trilist, which draws each as vx_d3d_gl_prim did. Two-sided stencil, smoothing and both faces
 * culled keep the template (vxgl_draw_triangle's own cases). 0: the template for every run. vxgl_trilist_stat: [0] runs, [1] triangles,
 * [2] runs left to the template */
int vxgl_gl_trilist=1;uint32_t vxgl_trilist_stat[4];
static void (*plain_tris[2])(GLcontext *,GLuint,GLuint,GLuint); /* [elts] Mesa's template functions */
#define TRI_CHUNK 640 /* .573 (was 128): a packet's indices, so a run goes to the core whole and its vertices are converted once (gl_ix_emit) */
static uint32_t tri_ix[TRI_CHUNK*3];
static void tris_run_t(GLcontext *ctx,GLuint start,GLuint count,GLuint flags,int elts);
static void tris_run(GLcontext *ctx,GLuint start,GLuint count,GLuint flags,int elts) {VXT_BEGIN;tris_run_t(ctx,start,count,flags,elts);VXT_END(1);} /* .506 */
static void tris_run_t(GLcontext *ctx,GLuint start,GLuint count,GLuint flags,int elts) {
 vxgl_ctx *c=VXGL_CTX(ctx);const GLuint *elt=TNL_CONTEXT(ctx)->vb.Elts;GLuint j=start+2,m;uint32_t bad,rc;
 if(!vxgl_gl_trilist||c->twoside_stencil||c->aa_tri||c->cull_all){vxgl_trilist_stat[2]++;plain_tris[elts](ctx,start,count,flags);return;}
 vxgl_raster_primitive(ctx,GL_TRIANGLES,GL_TRIANGLES); /* the template's INIT(GL_TRIANGLES) */
 vxgl_trilist_stat[0]++;vx_d3d_gl_runstart(); /* .503 */
 while(j<count){
  m=0;
  if(elts)for(;j<count&&m<TRI_CHUNK;j+=3,m++){tri_ix[3*m]=elt[j-2];tri_ix[3*m+1]=elt[j-1];tri_ix[3*m+2]=elt[j];}
  else for(;j<count&&m<TRI_CHUNK;j+=3,m++){tri_ix[3*m]=j-2;tri_ix[3*m+1]=j-1;tri_ix[3*m+2]=j;}
  {VXGL_PROF_BEGIN;rc=vx_d3d_gl_trilist(&c->core,&c->sv,tri_ix,m,c->join,&bad);VXGL_PROF_END(2);}
  c->stats[0]+=m;vxgl_trilist_stat[1]+=m;
  if(bad){static int logged;if(logged<8){logged++;vxgl_log("primitive refused: %08lX (%lu of a run)\n",(unsigned long)rc,(unsigned long)bad);}}
 }
}
static void tris_verts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {tris_run(ctx,start,count,flags,0);}
static void tris_elts(GLcontext *ctx,GLuint start,GLuint count,GLuint flags) {tris_run(ctx,start,count,flags,1);}
/* v10.500: VXFLAT's counts of this build's parts: [0] triangle runs by TRILIST, [1] their triangles, [2] runs left to the template, [3] draws
 * that took the locked-array transform results (CVACACHE), [4] draws that made them, [5] draws with the store's grid words (STORESNAP), [6]
 * their vertices, [7] times glapi went to per-thread dispatch, [8] times it came back (GLAPIONE); cleared when clear is set. Returns the
 * switches: 1 TRILIST, 2 CVACACHE, 4 FASTVTX, 8 STORESNAP, 16 GLAPIONE */
extern unsigned long _tnl_vx855_cva_stat[2],_glapi_vx855_switches[2];extern int _tnl_vx855_cvacache,_vbo_vx855_fastvtx,vxgl_gl_glapione,vxgl_gl_storesnap;extern uint32_t vxgl_storesnap_stat[2];
/* v10.501: VXGL_Stats500's nine and [9] draws whose colours went as bytes (UBCOLOR), [10] of them converted after all, [11] packets sent
 * prepacked (PREPACK), [12] triangles in indexed packets (INDEXED); switches 32 UBCOLOR, 64 PREPACK, 128 INDEXED besides VXGL_Stats500's */
extern unsigned long _tnl_vx855_ub_stat[2];extern uint32_t vx_d3d_pre_stat[2],vx_d3d_ix[4],vx_d3d_prepack,vx_d3d_indexed;extern int vxgl_gl_ubcolor;
uint32_t __stdcall VXGL_Stats500(uint32_t *o,uint32_t clear);
uint32_t __stdcall VXGL_Stats501(uint32_t *o,uint32_t clear) {
 uint32_t sw=VXGL_Stats500(o,0);
 if(o){o[9]=(uint32_t)_tnl_vx855_ub_stat[0];o[10]=(uint32_t)_tnl_vx855_ub_stat[1];o[11]=vx_d3d_pre_stat[0];o[12]=vx_d3d_ix[0];}
 if(clear){VXGL_Stats500(0,1);_tnl_vx855_ub_stat[0]=_tnl_vx855_ub_stat[1]=0;vx_d3d_pre_stat[0]=vx_d3d_pre_stat[1]=0;vx_d3d_ix[0]=0;}
 return sw|(vxgl_gl_ubcolor?32U:0U)|(vx_d3d_prepack?64U:0U)|(vx_d3d_indexed?128U:0U);
}
/* .503: VXGL_Stats501's thirteen and [13] vertices written from the recipe (GLWORDS), [14] triangles through the triangle-list run's own
 * checks (GLLIST), [15] draws whose lighting waited for the triangles (SSEDEFER), [16] their groups lit, [17] left unlit; switches 256 GLLIST,
 * 512 GLWORDS, 1024 SSE2, 2048 SSEDEFER besides VXGL_Stats501's */
extern uint32_t vxgl_ssepass_defer_stat[3];extern int vxgl_ssepass_sse2,vxgl_ssepass_defer;
uint32_t __stdcall VXGL_Stats503(uint32_t *o,uint32_t clear) {
 uint32_t sw=VXGL_Stats501(o,0);
 if(o){o[13]=vx_d3d_gw[0];o[14]=vx_d3d_gw[2];o[15]=vxgl_ssepass_defer_stat[0];o[16]=vxgl_ssepass_defer_stat[1];o[17]=vxgl_ssepass_defer_stat[2];}
 if(clear){VXGL_Stats501(0,1);vx_d3d_gw[0]=vx_d3d_gw[2]=0;vxgl_ssepass_defer_stat[0]=vxgl_ssepass_defer_stat[1]=vxgl_ssepass_defer_stat[2]=0;}
 return sw|(vx_d3d_gl_gllist?256U:0U)|(vx_d3d_gl_glwords?512U:0U)|(vxgl_ssepass_sse2?1024U:0U)|(vxgl_ssepass_defer?2048U:0U);
}
/* .505: VXGL_Stats503's eighteen and [18] vertices by the fixed layouts (GLWORDS 2), [19] polygons, fans and quads in one call (GLFAN);
 * switches 4096 GLWORDS 2, 8192 GLFAN, 16384 SSERANGE besides VXGL_Stats503's */
extern uint32_t vx_d3d_gw_fixed;extern int vxgl_ssepass_range;
uint32_t __stdcall VXGL_Stats505(uint32_t *o,uint32_t clear) {
 uint32_t sw=VXGL_Stats503(o,0);
 if(o){o[18]=vx_d3d_gw[1];o[19]=vx_d3d_gw[3];}
 if(clear){VXGL_Stats503(0,1);vx_d3d_gw[1]=vx_d3d_gw[3]=0;}
 return sw|(vx_d3d_gw_fixed&&vx_d3d_gl_glwords?4096U:0U)|(vxgl_gl_fan?8192U:0U)|(vxgl_ssepass_range?16384U:0U);
}
/* .506: VXGL_Stats505's twenty and [20..26] the CPU's cycles / 256 in the ICD's parts (ACCEL_OPENGL_TSCPROF: vxgl.h's VXT list), [27]
 * triangles whose decisions came from the last draw's plan (PLAN), [28] index scans by SSE2 (MINMAX2), [29] 0 (.506's indexed polygons, removed in .509);
 * switches 32768 PLAN, 65536 MINMAX2, 131072 TSCPROF besides VXGL_Stats505's */
extern int _vbo_vx855_minmax2;extern unsigned long _vbo_vx855_minmax2_n;
uint32_t __stdcall VXGL_Stats506(uint32_t *o,uint32_t clear) {
 uint32_t sw=VXGL_Stats505(o,0);int k;
 if(o){for(k=0;k<7;k++)o[20+k]=(uint32_t)(vxgl_tsc[k]/256.0);o[27]=vx_d3d_plan_stat[0];o[28]=(uint32_t)_vbo_vx855_minmax2_n;o[29]=0;} /* .509: [29] (polygons into an indexed packet) gone with INDEXED */
 if(clear){VXGL_Stats505(0,1);for(k=0;k<8;k++){vxgl_tsc[k]=0;vxgl_tsc_n[k]=0;}vx_d3d_plan_stat[0]=vx_d3d_plan_stat[1]=vx_d3d_plan_stat[2]=0;_vbo_vx855_minmax2_n=0;}
 return sw|(vx_d3d_gl_plan?32768U:0U)|(_vbo_vx855_minmax2?65536U:0U)|(vxgl_tsc_on?131072U:0U);
}
uint32_t __stdcall VXGL_Stats500(uint32_t *o,uint32_t clear) {
 if(o){o[0]=vxgl_trilist_stat[0];o[1]=vxgl_trilist_stat[1];o[2]=vxgl_trilist_stat[2];o[3]=(uint32_t)_tnl_vx855_cva_stat[0];o[4]=(uint32_t)_tnl_vx855_cva_stat[1];
  o[5]=vxgl_storesnap_stat[0];o[6]=vxgl_storesnap_stat[1];o[7]=(uint32_t)_glapi_vx855_switches[0];o[8]=(uint32_t)_glapi_vx855_switches[1];}
 if(clear){int k;for(k=0;k<4;k++)vxgl_trilist_stat[k]=0;_tnl_vx855_cva_stat[0]=_tnl_vx855_cva_stat[1]=0;vxgl_storesnap_stat[0]=vxgl_storesnap_stat[1]=0;}
 return (vxgl_gl_trilist?1U:0U)|(_tnl_vx855_cvacache?2U:0U)|(_vbo_vx855_fastvtx?4U:0U)|(vxgl_gl_storesnap?8U:0U)|(vxgl_gl_glapione?16U:0U);
}
static void strips_install(void) {
 plain_tris[0]=vxgl_render_tab_verts[GL_TRIANGLES];plain_tris[1]=vxgl_render_tab_elts[GL_TRIANGLES]; /* v10.500 */
 vxgl_render_tab_verts[GL_TRIANGLES]=tris_verts;vxgl_render_tab_elts[GL_TRIANGLES]=tris_elts;
 {extern void (*_tnl_vx855_clipflush)(GLcontext *);_tnl_vx855_clipflush=vxgl_clipflush;} /* v10.485 */
 plain_strip[0][0]=vxgl_render_tab_verts[GL_TRIANGLE_STRIP];plain_strip[0][1]=vxgl_render_tab_verts[GL_QUAD_STRIP];
 plain_strip[1][0]=vxgl_render_tab_elts[GL_TRIANGLE_STRIP];plain_strip[1][1]=vxgl_render_tab_elts[GL_QUAD_STRIP];
 vxgl_render_tab_verts[GL_TRIANGLE_STRIP]=strip_verts;vxgl_render_tab_verts[GL_QUAD_STRIP]=quadstrip_verts;
 vxgl_render_tab_elts[GL_TRIANGLE_STRIP]=strip_elts;vxgl_render_tab_elts[GL_QUAD_STRIP]=quadstrip_elts;
 memcpy(flat_tab_verts,_tnl_render_tab_verts,sizeof(flat_tab_verts));memcpy(flat_tab_elts,_tnl_render_tab_elts,sizeof(flat_tab_elts)); /* v10.348 */
 flat_tris_mesa[0]=flat_tab_verts[GL_TRIANGLES];flat_tris_mesa[1]=flat_tab_elts[GL_TRIANGLES];flat_tab_verts[GL_TRIANGLES]=flat_tris_verts;flat_tab_elts[GL_TRIANGLES]=flat_tris_elts; /* v10.483 */
 flat_strip_mesa[0][0]=flat_tab_verts[GL_TRIANGLE_STRIP];flat_strip_mesa[0][1]=flat_tab_verts[GL_QUAD_STRIP];flat_strip_mesa[1][0]=flat_tab_elts[GL_TRIANGLE_STRIP];flat_strip_mesa[1][1]=flat_tab_elts[GL_QUAD_STRIP]; /* v10.480 */
 flat_tab_verts[GL_TRIANGLE_STRIP]=flat_strip_verts;flat_tab_verts[GL_QUAD_STRIP]=flat_quadstrip_verts;flat_tab_elts[GL_TRIANGLE_STRIP]=flat_strip_elts;flat_tab_elts[GL_QUAD_STRIP]=flat_quadstrip_elts;
 plain_poly[0][0]=vxgl_render_tab_verts[GL_POLYGON];plain_poly[1][0]=vxgl_render_tab_elts[GL_POLYGON];plain_poly[0][1]=vxgl_render_tab_verts[GL_TRIANGLE_FAN];plain_poly[1][1]=vxgl_render_tab_elts[GL_TRIANGLE_FAN];
 flat_poly[0][0]=flat_tab_verts[GL_POLYGON];flat_poly[1][0]=flat_tab_elts[GL_POLYGON];flat_poly[0][1]=flat_tab_verts[GL_TRIANGLE_FAN];flat_poly[1][1]=flat_tab_elts[GL_TRIANGLE_FAN];
 vxgl_render_tab_verts[GL_POLYGON]=flat_tab_verts[GL_POLYGON]=poly_verts;vxgl_render_tab_elts[GL_POLYGON]=flat_tab_elts[GL_POLYGON]=poly_elts;
 vxgl_render_tab_verts[GL_TRIANGLE_FAN]=flat_tab_verts[GL_TRIANGLE_FAN]=fan_verts;vxgl_render_tab_elts[GL_TRIANGLE_FAN]=flat_tab_elts[GL_TRIANGLE_FAN]=fan_elts;
 plain_poly[0][2]=vxgl_render_tab_verts[GL_QUADS];plain_poly[1][2]=vxgl_render_tab_elts[GL_QUADS];flat_poly[0][2]=flat_tab_verts[GL_QUADS];flat_poly[1][2]=flat_tab_elts[GL_QUADS];
 vxgl_render_tab_verts[GL_QUADS]=flat_tab_verts[GL_QUADS]=quads_verts;vxgl_render_tab_elts[GL_QUADS]=flat_tab_elts[GL_QUADS]=quads_elts;
}

static void render_clipped_poly(GLcontext *ctx,const GLuint *elts,GLuint n) {
 TNLcontext *tnl=TNL_CONTEXT(ctx);struct vertex_buffer *VB=&tnl->vb;GLuint prim=VXGL_CTX(ctx)->render_prim;GLuint *tmp=VB->Elts;
 vx_d3d_gl_unjoin(); /* v10.360: the clipper's new vertices reuse slots (vx_d3d_gl_unjoin) */
 VB->Elts=(GLuint*)elts;tnl->Driver.Render.PrimTabElts[GL_POLYGON](ctx,0,n,PRIM_BEGIN|PRIM_END);VB->Elts=tmp;
 if(prim!=GL_POLYGON&&prim!=GL_POLYGON+1)tnl->Driver.Render.PrimitiveNotify(ctx,prim);
}
static void render_clipped_line(GLcontext *ctx,GLuint ii,GLuint jj) {vx_d3d_gl_unjoin();TNL_CONTEXT(ctx)->Driver.Render.Line(ctx,ii,jj);}
static void clipped_line(GLcontext *ctx,GLuint ii,GLuint jj) {vx_d3d_gl_unjoin();line(ctx,ii,jj);} /* v10.360 */
static void fast_clipped_poly(GLcontext *ctx,const GLuint *elts,GLuint n) {
 vxgl_ctx *c=VXGL_CTX(ctx);GLuint i;vxglVertex *v0=(vxglVertex*)(c->verts+elts[0]*c->vertex_size);
 vx_d3d_gl_unjoin(); /* v10.360: the clipper's new vertices reuse slots: no join to the last clipped polygon's (vx_d3d_gl_unjoin) */
 if(fan_strips(c,elts,n))return; /* v10.348: the clipper's polygon as fan strips */
 for(i=2;i<n;i++)vxgl_draw_triangle(c,(vxglVertex*)(c->verts+elts[i-1]*c->vertex_size),(vxglVertex*)(c->verts+elts[i]*c->vertex_size),v0);
}

void vxgl_choose_render_state(GLcontext *ctx) {
 TNLcontext *tnl=TNL_CONTEXT(ctx);vxgl_ctx *c=VXGL_CTX(ctx);GLuint flags=ctx->_TriangleCaps,index=0;
 if(flags&ANY_RASTER_FLAGS) {
  if(ctx->Light.Enabled&&ctx->Light.Model.TwoSide)index|=VXGL_TWOSIDE_BIT;
  if(ctx->Polygon.FrontMode!=GL_FILL||ctx->Polygon.BackMode!=GL_FILL)index|=VXGL_UNFILLED_BIT;
  if(flags&DD_TRI_OFFSET)index|=VXGL_OFFSET_BIT;
 }
 if(ctx->Light.ShadeModel==GL_FLAT)index|=VXGL_FLAT_BIT;
 c->join=index==0; /* the vertices are not changed per triangle */
 if(c->render_index!=index) {
  c->render_index=index;
  tnl->Driver.Render.Points=rast_tab[index].points;tnl->Driver.Render.Line=rast_tab[index].line;
  tnl->Driver.Render.Triangle=rast_tab[index].triangle;tnl->Driver.Render.Quad=rast_tab[index].quad;
  if(index==0) {
   tnl->Driver.Render.PrimTabVerts=vxgl_render_tab_verts;tnl->Driver.Render.PrimTabElts=vxgl_render_tab_elts;
   tnl->Driver.Render.ClippedLine=clipped_line;tnl->Driver.Render.ClippedPolygon=fast_clipped_poly;
  } else if(index==VXGL_FLAT_BIT) { /* v10.348: flat shading's polygons as strips when their colours are one */
   tnl->Driver.Render.PrimTabVerts=flat_tab_verts;tnl->Driver.Render.PrimTabElts=flat_tab_elts;
   tnl->Driver.Render.ClippedLine=render_clipped_line;tnl->Driver.Render.ClippedPolygon=render_clipped_poly;
  } else {
   tnl->Driver.Render.PrimTabVerts=_tnl_render_tab_verts;tnl->Driver.Render.PrimTabElts=_tnl_render_tab_elts;
   tnl->Driver.Render.ClippedLine=render_clipped_line;tnl->Driver.Render.ClippedPolygon=render_clipped_poly;
  }
 }
}

/* the vertex layout: x y z rhw, diffuse, [specular + fog], [s t for units 0 and 1 later] */
#define EMIT_ATTR(ATTR,STYLE) do{c->attrs[c->attr_count].attrib=(ATTR);c->attrs[c->attr_count].format=(STYLE);c->attr_count++;}while(0)
#define EMIT_PADDING(N) do{c->attrs[c->attr_count].attrib=0;c->attrs[c->attr_count].format=EMIT_PAD;c->attrs[c->attr_count].offset=(N);c->attr_count++;}while(0)
void vxgl_choose_vertex_state(GLcontext *ctx) { /* v10.487: called by the SSE pass too, ahead of the render stage (the same map: _tnl_install_attrs changes nothing the second time) */
 vxgl_ctx *c=VXGL_CTX(ctx);TNLcontext *tnl=TNL_CONTEXT(ctx);DECLARE_RENDERINPUTS(bits);GLuint fvf=0x44,spec;
 RENDERINPUTS_COPY(bits,tnl->render_inputs_bitset);
 c->attr_count=0;
 EMIT_ATTR(_TNL_ATTRIB_POS,EMIT_4F_VIEWPORT);
 EMIT_ATTR(_TNL_ATTRIB_COLOR0,EMIT_4UB_4F_BGRA);c->coloroffset=4;c->specoffset=0;
 spec=RENDERINPUTS_TEST(bits,_TNL_ATTRIB_COLOR1)||RENDERINPUTS_TEST(bits,_TNL_ATTRIB_FOG);
 if(spec) {
  c->specoffset=5;fvf|=0x80;
  if(RENDERINPUTS_TEST(bits,_TNL_ATTRIB_COLOR1))EMIT_ATTR(_TNL_ATTRIB_COLOR1,EMIT_3UB_3F_BGR);else EMIT_PADDING(3);
  if(RENDERINPUTS_TEST(bits,_TNL_ATTRIB_FOG))EMIT_ATTR(_TNL_ATTRIB_FOG,EMIT_1UB_1F);else EMIT_PADDING(1);
 }
 {int s;for(s=0;s<c->tex_sets;s++)EMIT_ATTR(_TNL_ATTRIB_TEX0+c->tex_unit_of[s],((!s&&c->tex_layout)||(s==1&&c->tex_set3))?EMIT_3F:EMIT_2F);fvf|=(GLuint)c->tex_sets<<8; /* v10.307: tex_sets (a third stage shares one) */ /* v10.268: D3DFVF_TEXn, 2D sets */
  if(c->tex_units&&c->tex_layout)fvf|=1UL<<16; /* v10.295: D3DFVF_TEXCOORDSIZE3(0): a cube map's s t r, or projective s t q (vxgl_proj) */
  if(c->tex_units>1&&c->tex_set3)fvf|=1UL<<18;} /* v10.321: D3DFVF_TEXCOORDSIZE3(1): stage 1's cube map or volume */
 c->vertex_size=_tnl_install_attrs(ctx,c->attrs,c->attr_count,c->ViewportMatrix.m,0);c->vs_rcp=c->vertex_size?1.0F/(GLfloat)c->vertex_size:0.0F; /* v10.481 */
 c->fvf=fvf;
}

static void render_start(GLcontext *ctx) {
 vxgl_ctx *c=VXGL_CTX(ctx);TNLcontext *tnl=TNL_CONTEXT(ctx);struct vertex_buffer *VB=&tnl->vb;
 if(c->new_state)vxgl_translate_state(c);
 if(c->fallback){tnl->Driver.Render.Start(ctx);return;} /* swrast_setup took over (vxgl_fallback) */
 vxgl_choose_vertex_state(ctx);
 vxgl_choose_render_state(ctx);
 VB->AttribPtr[VERT_ATTRIB_POS]=VB->NdcPtr;
 vx_d3d_gl_begin(c->verts,c->vertex_size,c->fvf);vx_d3d_gl_count(verts_cap(c));
 { extern uint32_t vx_d3d_gl_snap; vx_d3d_gl_snap=vxgl_gl_snap?(vxgl_flip(c)?2U:1U):0U; } /* v10.324: triangles on swrast's 1/16 grid, y up with the window's flip */
 /* .506 (ACCEL_OPENGL_PLAN): the store's window vertices are the last draw's when the positions are the locked range's kept transform results
  * (the same clip coordinates), the viewport, the store, its layout and the vertex count the same: the core takes the last draw's triangle
  * decisions (vx_d3d_gl_plan_begin) */
 {extern int _tnl_vx855_cva_hit;static GLfloat vp[16];static const vxgl_ctx *pc;static const GLubyte *pv;static GLuint ps,pf,pn;
  int same=_tnl_vx855_cva_hit&&pc==c&&pv==c->verts&&ps==c->vertex_size&&pf==c->fvf&&pn==VB->Count&&!memcmp(vp,c->ViewportMatrix.m,sizeof(vp));
  memcpy(vp,c->ViewportMatrix.m,sizeof(vp));pc=c;pv=c->verts;ps=c->vertex_size;pf=c->fvf;pn=VB->Count;
  vx_d3d_gl_plan_begin(same);}
}
/* v10.500 (ACCEL_OPENGL_STORESNAP): after Mesa builds the store's vertices for a draw, their grid words (the snap the core makes for every
 * triangle vertex, two snapw a vertex, again for each triangle that shares it) four at a time (vxgl_ssepass_snapstore), given to the core for
 * this draw (vx_d3d_gl_presnap, as the SSE pass's: used by vertex number below the draw's count, so not for the clipper's new vertices); taken
 * back at the render's end. Not when the snap is off or something else's words are given. vxgl_storesnap_stat: [0] draws, [1] vertices */
int vxgl_gl_storesnap=1;uint32_t vxgl_storesnap_stat[2];static int ss_set;
extern GLuint *vxgl_ssepass_snapstore(const GLubyte *verts,GLuint vs,GLuint n,int up);
static void build_vertices(GLcontext *ctx,GLuint start,GLuint end,GLuint newinputs) {
 vxgl_ctx *c=VXGL_CTX(ctx);extern uint32_t vx_d3d_gl_snap;
 {VXT_BEGIN;_tnl_build_vertices(ctx,start,end,newinputs);VXT_END(4);} /* .506 */
 if(vxgl_gl_storesnap&&!start&&end&&vx_d3d_gl_snap&&!c->fallback){uint32_t pn,pm;const uint8_t *pb;
  if(!vx_d3d_gl_presnap_get(&pn,&pm,&pb)){GLuint *w=vxgl_ssepass_snapstore(c->verts,c->vertex_size,end,vx_d3d_gl_snap==2);
   if(w){vx_d3d_gl_presnap(w,end,vx_d3d_gl_snap,c->verts);ss_set=1;vxgl_storesnap_stat[0]++;vxgl_storesnap_stat[1]+=end;}}}
}
static void render_finish(GLcontext *ctx) {(void)ctx;if(ss_set){vx_d3d_gl_presnap(0,0,0,0);ss_set=0;}} /* the batch stays open (vxgl_hw_flush); v10.500: the store's grid words taken back */
static void render_primitive(GLcontext *ctx,GLenum prim) {vxgl_raster_primitive(ctx,prim,hwPrim[prim]);}
static void reset_line_stipple(GLcontext *ctx) {VXGL_CTX(ctx)->stip_count=0;} /* v10.300: swrast's _swrast_ResetLineStipple */

void vxgl_fallback(vxgl_ctx *c,GLuint bit,GLboolean on) {
 GLcontext *ctx=&c->gl;TNLcontext *tnl=TNL_CONTEXT(ctx);GLuint old=c->fallback;
 if(on) {
  c->fallback|=bit;
  if(!old){vxgl_hw_flush(c);_swsetup_Wakeup(ctx);c->render_index=~0U;c->stats[6]++;
   if(c->stats[6]<=32){ /* v10.336: the condition behind the bits (vxgl_state.c fall_why) */
    static const char *const wn[11]={"polygon stipple off (ACCEL_OPENGL_PSTIPPLE 0)","smooth points","smooth lines","smooth polygons","with the alpha test on (swrast tests before the coverage)",
     "line stipple","attenuated points or point sprites","a wide line","a point size other than 1","the texture state","the stage does not carry the coverage alpha"};
    char why[400];GLuint k;why[0]=0;
    for(k=0;k<11;k++)if(c->fall_why&(1U<<k)){if(why[0])strcat(why,", ");strcat(why,wn[k]);}
    vxgl_log("fallback on: %04lX%s%s%s%s\n",(unsigned long)c->fallback,why[0]?" (":"",why,why[0]?")":"",c->stats[6]==32?" (the first 32 of this context; the count at its end)":"");}}
 } else {
  c->fallback&=~bit;
  if(old&&!c->fallback) {
   _swrast_flush(ctx);
   tnl->Driver.Render.Start=render_start;tnl->Driver.Render.PrimitiveNotify=render_primitive;tnl->Driver.Render.Finish=render_finish;
   tnl->Driver.Render.BuildVertices=build_vertices;tnl->Driver.Render.CopyPV=_tnl_copy_pv;tnl->Driver.Render.Interp=_tnl_interp;
   tnl->Driver.Render.ResetLineStipple=reset_line_stipple;
   _tnl_invalidate_vertex_state(ctx,~0);_tnl_invalidate_vertices(ctx,~0);
   _tnl_install_attrs(ctx,c->attrs,c->attr_count,c->ViewportMatrix.m,0);
   c->render_index=~0U;
   if(c->stats[6]<=32)vxgl_log("fallback off\n");
  }
 }
}

/* v10.265: the state's fallback bits (mask) set to bits in one step. A context that stays in software keeps swrast_setup as it is: it was
 * switched off and on again at every state change with a texture bound (Quake 2: every glBindTexture), each time a swrast flush, the
 * vertex formats made again and swrast_setup woken. */
void vxgl_fallback_bits(vxgl_ctx *c,GLuint mask,GLuint bits) {
 GLuint now=(c->fallback&~mask)|bits;
 if(c->fallback&&now){c->fallback=now;return;}
 if(now)vxgl_fallback(c,bits,GL_TRUE);else vxgl_fallback(c,mask,GL_FALSE);
}

/* v10.501 (ACCEL_OPENGL_UBCOLOR, t_draw.c): whether a draw's byte colours may stay bytes: only the vertex emit reads them (no lighting, no
 * program, the engine drawing; the SSE pass takes lit draws only); run_pipeline converts them after all when this draw turns out to go to
 * software (its fallbacks are decided there), and the pass converts them before reading */
int vxgl_gl_ubcolor=1;
extern int _tnl_vx855_ubcolor;extern int (*_tnl_vx855_ub_ok)(GLcontext*);extern void _tnl_vx855_ub_float(GLcontext*);
static int ub_ok(GLcontext *ctx) {
 vxgl_ctx *c=VXGL_CTX(ctx);
 return !ctx->Light.Enabled&&!ctx->VertexProgram._Current&&!ctx->VertexProgram._Enabled&&ctx->RenderMode==GL_RENDER&&!c->fallback;
}
static void run_pipeline_t(GLcontext *ctx);
static void run_pipeline(GLcontext *ctx) {VXT_BEGIN;run_pipeline_t(ctx);VXT_END(0);} /* .506 */
static void run_pipeline_t(GLcontext *ctx) {
 vxgl_ctx *c=VXGL_CTX(ctx);
 {extern int _tnl_vx855_cva_hit;_tnl_vx855_cva_hit=0;} /* .506: set by this run's vertex stage only */
 vxgl_engine_lock(); /* render start to finish: the core's vertex buffer (vx_d3d_gl_begin) and batch are this context's throughout */
 if(vxgl_batch_ctx!=c){vxgl_hw_flush(0);vxgl_batch_ctx=c;} /* v10.288 (review): another context's batch is sent by that context (the core's own flush would send it through this one: this context's window settled, not the one drawn into) */
 if(c->new_state)vxgl_translate_state(c);
 /* v10.317: the engine draws into the front surface (vxgl_state.c): filled from the window again after a swap; a surface made again or gone
  * since the state was translated is looked at again (the core's target is its offset) */
 if(c->front_hw&&c->fb){if(!(c->fb->fstate&1)){vxgl_hw_flush(c);vxgl_front_load(c->fb);}
  if(!c->fb->front||c->fb->fsoft||!c->fb->sfront.format||memcmp(&c->core.target,&c->fb->sfront,sizeof(vx3dn_surface))){c->new_state|=VXGL_STATE_MASK;vxgl_translate_state(c);}}
 if(c->fb&&c->fb->zdirty&&vxgl_z_written(c->fb))c->fb->zdirty=0; /* v10.301 (review): a notice that failed goes before the next engine draw */
 /* v10.342 (review): when it failed again the engine's Coarse Z records may still be the old depth: a draw that tests depth on the engine
  * stays in software until the notice goes through (the flag stays set; it is tried again at every draw) */
 {int bad=c->fb&&c->fb->zdirty&&!c->rtt&&c->gl.Depth.Test&&c->core.depth.format;
  if(bad!=((c->fallback&VXGL_FB_ZNOTICE)!=0))vxgl_fallback(c,VXGL_FB_ZNOTICE,bad?GL_TRUE:GL_FALSE);}
 {int q=vxgl_tex_q_varies(c);if(q!=c->q_forced){c->q_forced=q;vxgl_translate_state(c);}} /* v10.300: q in the vertices: projective */
 {int s2=-1; /* v10.309 (review): the third stage's set is scanned once a draw, here, and given to vxgl_tex_vertices_ok */
 if(c->tex_units>2){s2=vxgl_tex_set2(c);if(s2>=0&&s2!=c->tex_set2){c->tex_set2=s2;vxgl_translate_state(c);}} /* v10.307: the third stage's coordinate set */
 {int bad=c->tex_units&&!vxgl_tex_vertices_ok(c,s2); /* v10.268: known only from this draw's primitives and coordinates */
  if(bad!=((c->fallback&VXGL_FB_TEXVERT)!=0))vxgl_fallback(c,VXGL_FB_TEXVERT,bad?GL_TRUE:GL_FALSE);}}
 {int bad=0;GLuint i;struct vertex_buffer *VB=&TNL_CONTEXT(ctx)->vb; /* v10.308: polygon stipple takes no lines or points */
  if(c->pstip){if(ctx->Polygon.FrontMode!=GL_FILL||ctx->Polygon.BackMode!=GL_FILL)bad=1;
   for(i=0;i<VB->PrimitiveCount&&!bad;i++){GLenum m=VB->Primitive[i].mode;if(m==GL_POINTS||m==GL_LINES||m==GL_LINE_LOOP||m==GL_LINE_STRIP)bad=1;}}
  if(bad!=((c->fallback&VXGL_FB_PSTIPVERT)!=0))vxgl_fallback(c,VXGL_FB_PSTIPVERT,bad?GL_TRUE:GL_FALSE);}
 {int bad=0;GLuint i;struct vertex_buffer *VB=&TNL_CONTEXT(ctx)->vb; /* v10.341: points-only / lines-only conditions (vxgl_state.c): this draw's primitives */
  if(c->fall_pt||c->fall_ln){int pm=ctx->Polygon.FrontMode==GL_POINT||ctx->Polygon.BackMode==GL_POINT,lm=ctx->Polygon.FrontMode==GL_LINE||ctx->Polygon.BackMode==GL_LINE;
   for(i=0;i<VB->PrimitiveCount&&!bad;i++){GLenum m=VB->Primitive[i].mode;int pts=m==GL_POINTS,lns=m==GL_LINES||m==GL_LINE_LOOP||m==GL_LINE_STRIP,tri=!pts&&!lns;
    if((c->fall_pt&&(pts||(tri&&pm)))||(c->fall_ln&&(lns||(tri&&lm))))bad=1;}}
  /* v10.353: smooth points / lines tested here (c->aa_atest): the draw's points and lines alone (the engine's alpha test is off for them),
   * filled polygons, both ends of each line with one alpha (lighting off: the colours as given), one target */
  if(!bad&&c->aa_atest){int pl=0,other=0;
   if(ctx->Polygon.FrontMode!=GL_FILL||ctx->Polygon.BackMode!=GL_FILL||c->both)bad=1;
   for(i=0;i<VB->PrimitiveCount&&!bad;i++){GLenum m=VB->Primitive[i].mode;if(m==GL_POINTS||m==GL_LINES||m==GL_LINE_LOOP||m==GL_LINE_STRIP)pl=1;else other=1;}
   if(pl&&other)bad=1;
   if(!bad&&pl&&c->aa_line){const GLvector4f *col;_tnl_vx855_ub_float(ctx);col=VB->ColorPtr[0]; /* v10.501: the floats */
    if(ctx->Light.Enabled||!col)bad=1;
    else if(col->stride&&col->size>=4&&VB->Count>1){ /* (fewer than 4 components: the alpha is 1 throughout) */GLuint k;const GLubyte *p=(const GLubyte*)col->data;GLfloat a0=((const GLfloat*)p)[3];
     for(i=0;i<VB->PrimitiveCount&&!bad;i++){GLenum m=VB->Primitive[i].mode;if(m!=GL_LINES&&m!=GL_LINE_LOOP&&m!=GL_LINE_STRIP)continue;
      for(k=VB->Primitive[i].start;k<VB->Primitive[i].start+VB->Primitive[i].count&&k<VB->Count;k++)if(((const GLfloat*)(p+k*col->stride))[3]!=a0){bad=1;break;}}}}
   c->atest_off=!bad&&pl;}
  else c->atest_off=0;
  if(bad!=((c->fallback&VXGL_FB_PRIMSTATE)!=0))vxgl_fallback(c,VXGL_FB_PRIMSTATE,bad?GL_TRUE:GL_FALSE);}

 if(c->fallback||ctx->RenderMode!=GL_RENDER)_tnl_vx855_ub_float(ctx); /* v10.501: software reads the colours as floats */
 if(c->fallback){GLuint k;c->stats[3]++;for(k=0;k<17;k++)if(c->fallback&(1U<<k))c->fb_draws[k]++;for(k=0;k<11;k++)if(c->fall_why&(1U<<k))c->fb_why_draws[k]++;} /* v10.500: by bit and reason */
 if(vxgl_framelog&&c->fl_after_clear&&c->fl_len<(int)sizeof(c->fl_buf)-140){GLcontext *g=&c->gl; /* v10.343: the first draw after a clear: its depth state */

  c->fl_len+=_snprintf(c->fl_buf+c->fl_len,sizeof(c->fl_buf)-1-c->fl_len," draw[%s test %d func %X mask %d range %.3f-%.3f zsurf %lX/%lu tgt %lX]",c->fallback?"sw":"hw",(int)g->Depth.Test,(unsigned)g->Depth.Func,(int)g->Depth.Mask,
   (double)g->Viewport.Near,(double)g->Viewport.Far,(unsigned long)c->core.depth.offset,(unsigned long)c->core.depth.format,(unsigned long)c->core.target.offset);
  if(g->Depth.Test)c->fl_after_clear=0;}
 /* v10.321: tnl's fog values are blend factors for the engine (its vertex fog), but swrast takes them as fog coordinates and works the
  * factor out per pixel when a fragment program is current or the fog hint is NICEST (s_context.c _swrast_update_fog_hint; tnl's own
  * choice follows only the hint, t_context.c): fogged twice in software, and coordinates sent to the engine as factors under NICEST.
  * Coordinates when swrast draws and wants them, factors otherwise; set here, after every fallback of this draw is known. */
 TNL_CONTEXT(ctx)->_DoVertexFog=!(c->fallback&&(ctx->FragmentProgram._Current||ctx->Hint.Fog==GL_NICEST));
 {GLuint st0=c->stip_count;
  /* v10.353: a fragment program's fog option: tnl's fog stage runs only with GL_FOG and in its mode; for the engine's draw, GL_FOG on in the
   * option's mode meanwhile (no state is validated in between: the pipeline's stages read them as they run) */
  const struct gl_fragment_program *ffp=ctx->FragmentProgram._Current;GLboolean fog_en=ctx->Fog.Enabled;GLenum fog_mode=ctx->Fog.Mode;
  int fog_swap=!c->fallback&&ffp&&ffp->Base.Target==GL_FRAGMENT_PROGRAM_ARB&&ffp->FogOption!=GL_NONE&&(!fog_en||fog_mode!=ffp->FogOption);
  if(fog_swap){ctx->Fog.Enabled=GL_TRUE;ctx->Fog.Mode=ffp->FogOption;}
  if(c->atest_off&&!c->fallback){vxgl_hw_flush(c);c->core.state.rs[15]=0;vx_d3d_gl_state();} /* v10.353: tested per primitive (aa_point, aa_line) */
  {VXGL_PROF_BEGIN;_tnl_run_pipeline(ctx);VXGL_PROF_END(1);}
  if(c->atest_off&&!c->fallback){vxgl_hw_flush(c);c->core.state.rs[15]=ctx->Color.AlphaEnabled?1:0;vx_d3d_gl_state();}
  /* v10.340: GL_FRONT_AND_BACK: the same draw again into the front surface (the line stipple from where it began), then the back again
   * as the target for the next draw */
  if(c->both&&!c->fallback&&c->fb) {
   c->stip_count=st0;c->both_front=1;c->new_state|=VXGL_STATE_MASK;vxgl_translate_state(c);
   if(c->both&&c->front_hw&&!c->fallback){{VXGL_PROF_BEGIN;_tnl_run_pipeline(ctx);VXGL_PROF_END(1);}c->fb->fstate|=3;}
   c->both_front=0;c->new_state|=VXGL_STATE_MASK;vxgl_translate_state(c);
  }
  if(fog_swap){ctx->Fog.Enabled=fog_en;ctx->Fog.Mode=fog_mode;}}
 if(c->front_hw&&!c->fallback&&c->fb)c->fb->fstate|=3; /* v10.317: drawn into the front: shown at glFlush / glFinish */
 if(c->rtt&&!c->fallback)vxgl_fbo_drawn(c); /* v10.321: drawn into a framebuffer object's texture: its image is read back before Mesa uses it */
 vxgl_engine_unlock();
}

/* v10.295: projective texture coordinates for the engine: after Mesa's texgen and texture matrix stages, the only unit's (s, t, r, q)
 * become (s, t, q), which the vertex emit sends as the third coordinate (the core's COUNT3 | PROJECTED layout, XP's T151 read);
 * clipping interpolates them linearly, as projective coordinates are. Only while the engine draws (swrast takes Mesa's own). */
static GLboolean proj_run(GLcontext *ctx,struct tnl_pipeline_stage *st) {
 vxgl_ctx *c=VXGL_CTX(ctx);struct vertex_buffer *VB=&TNL_CONTEXT(ctx)->vb;GLvector4f *in;GLuint u,i,n;const GLubyte *p;GLfloat (*d)[4];
 (void)st;
 if(c->fallback||c->tex_layout!=2||!c->tex_units)return GL_TRUE;
 u=(GLuint)c->tex_unit_of[0];in=VB->AttribPtr[_TNL_ATTRIB_TEX0+u];n=VB->Count;
 if(!in||!n)return GL_TRUE;
 /* v10.296 (review): the new store is made before the old one goes, and kept only when it was made: on a failed allocation the draw is
  * left out with GL_OUT_OF_MEMORY (the old one stays) instead of written through a null pointer */
 if(c->proj_size<(int)n){GLvector4f t;GLuint want=n<VB->Size?VB->Size:n;
  _mesa_vector4f_alloc(&t,0,want,32);
  if(!t.storage){_mesa_error(ctx,GL_OUT_OF_MEMORY,"projective texture coordinates");return GL_FALSE;}
  if(c->proj_size)_mesa_vector4f_free(&c->proj_store);
  c->proj_store=t;c->proj_size=(int)want;}
 d=(GLfloat(*)[4])c->proj_store.start;p=(const GLubyte*)in->data;
 for(i=0;i<n;i++,p+=in->stride){const GLfloat *f=(const GLfloat*)p;d[i][0]=f[0];d[i][1]=in->size>1?f[1]:0.0F;d[i][2]=in->size>3?f[3]:1.0F;d[i][3]=1.0F;}
 c->proj_store.size=3;c->proj_store.count=n;c->proj_store.stride=4*sizeof(GLfloat);c->proj_store.flags|=VEC_SIZE_3;
 VB->AttribPtr[_TNL_ATTRIB_TEX0+u]=&c->proj_store;VB->TexCoordPtr[u]=&c->proj_store;
 return GL_TRUE;
}
static const struct tnl_pipeline_stage vxgl_proj_stage={"vxgl projective coordinates",NULL,NULL,NULL,NULL,proj_run};
/* v10.317: a padded copy's coordinates (vxgl_tex.c, prepare: a non-power-of-two side or a rectangle texture drawn from a power-of-two copy,
 * a ring of the border colour): s' = sa s + sb q, t' = ta t + tb q, after texgen and the texture matrix (q the fourth coordinate, 1 when there
 * is none), before the projective stage. Only while the engine draws: swrast samples Mesa's own image with the coordinates as they are. */
static GLboolean xf_run(GLcontext *ctx,struct tnl_pipeline_stage *st) {
 vxgl_ctx *c=VXGL_CTX(ctx);struct vertex_buffer *VB=&TNL_CONTEXT(ctx)->vb;GLuint s,i,n=VB->Count;
 (void)st;
 if(c->fallback||!c->tex_units||!n)return GL_TRUE;
 for(s=0;s<(GLuint)c->tex_units&&s<3;s++) {
  GLuint u=(GLuint)c->tex_unit_of[s];GLvector4f *in=VB->AttribPtr[_TNL_ATTRIB_TEX0+u];const GLubyte *p;GLfloat (*d)[4];GLuint sz;
  if(!c->tex_xf[s].on||!in||!in->data)continue;
  if(c->xf_size[s]<(int)n){GLvector4f t;GLuint want=n<VB->Size?VB->Size:n; /* made before the old one goes (as proj_run) */
   _mesa_vector4f_alloc(&t,0,want,32);
   if(!t.storage){_mesa_error(ctx,GL_OUT_OF_MEMORY,"texture coordinates");return GL_FALSE;}
   if(c->xf_size[s])_mesa_vector4f_free(&c->xf_store[s]);
   c->xf_store[s]=t;c->xf_size[s]=(int)want;}
  d=(GLfloat(*)[4])c->xf_store[s].start;p=(const GLubyte*)in->data;
  for(i=0;i<n;i++,p+=in->stride){const GLfloat *f=(const GLfloat*)p;GLfloat q=in->size>3?f[3]:1.0F,t=in->size>1?f[1]:0.0F;
   d[i][0]=c->tex_xf[s].sa*f[0]+c->tex_xf[s].sb*q;d[i][1]=c->tex_xf[s].ta*t+c->tex_xf[s].tb*q;d[i][2]=in->size>2?f[2]:0.0F;d[i][3]=q;}
  sz=in->size<2?2:in->size;
  c->xf_store[s].size=sz;c->xf_store[s].count=n;c->xf_store[s].stride=4*sizeof(GLfloat);
  c->xf_store[s].flags=(c->xf_store[s].flags&~VEC_SIZE_4)|(sz==2?VEC_SIZE_2:sz==3?VEC_SIZE_3:VEC_SIZE_4);
  VB->AttribPtr[_TNL_ATTRIB_TEX0+u]=&c->xf_store[s];VB->TexCoordPtr[u]=&c->xf_store[s];
 }
 return GL_TRUE;
}
static const struct tnl_pipeline_stage vxgl_texxf_stage={"vxgl padded texture coordinates",NULL,NULL,NULL,NULL,xf_run};
static const struct tnl_pipeline_stage *vxgl_pipeline[]={&vxgl_ssepass_stage, /* v10.487: a display list's draw in one SSE pass (vxgl_ssepass.c); GL_TRUE when it does not apply */
 &_tnl_vertex_transform_stage,&_tnl_normal_transform_stage,&_tnl_lighting_stage,
 &_tnl_fog_coordinate_stage,&_tnl_texgen_stage,&_tnl_texture_transform_stage,&_tnl_point_attenuation_stage,&_tnl_vertex_program_stage,
 &vxgl_texxf_stage,&vxgl_proj_stage,&_tnl_render_stage,NULL};
void vxgl_init_pipeline(GLcontext *ctx) {_tnl_destroy_pipeline(ctx);_tnl_install_pipeline(ctx,vxgl_pipeline);}
void vxgl_init_tri_funcs(GLcontext *ctx) {
 vxgl_ctx *c=VXGL_CTX(ctx);TNLcontext *tnl=TNL_CONTEXT(ctx);static int first=1;
 if(first){init_rast_tab();strips_install();first=0;}
 _tnl_vx855_ub_ok=ub_ok;_tnl_vx855_ubcolor=vxgl_gl_ubcolor; /* v10.501 */
 tnl->Driver.RunPipeline=run_pipeline;
 tnl->Driver.Render.Start=render_start;tnl->Driver.Render.Finish=render_finish;tnl->Driver.Render.PrimitiveNotify=render_primitive;
 tnl->Driver.Render.ResetLineStipple=reset_line_stipple;tnl->Driver.Render.BuildVertices=build_vertices;
 tnl->Driver.Render.CopyPV=_tnl_copy_pv;tnl->Driver.Render.Interp=_tnl_interp;
 _tnl_init_vertices(ctx,ctx->Const.MaxArrayLockSize+12,(6+2*ctx->Const.MaxTextureUnits)*sizeof(GLfloat));
 c->verts=(GLubyte*)tnl->clipspace.vertex_buf;
 c->render_index=~0U;
 c->attr_count=0;
 EMIT_ATTR(_TNL_ATTRIB_POS,EMIT_4F_VIEWPORT);EMIT_ATTR(_TNL_ATTRIB_COLOR0,EMIT_4UB_4F_BGRA);
 c->vertex_size=_tnl_install_attrs(ctx,c->attrs,c->attr_count,c->ViewportMatrix.m,0);c->vs_rcp=c->vertex_size?1.0F/(GLfloat)c->vertex_size:0.0F;c->fvf=0x44;c->coloroffset=4;
}
