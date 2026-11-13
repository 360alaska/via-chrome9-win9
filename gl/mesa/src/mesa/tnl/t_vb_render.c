/*
 * Mesa 3-D graphics library
 * Version:  6.5
 *
 * Copyright (C) 1999-2005  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */


/*
 * Render whole vertex buffers, including projection of vertices from
 * clip space and clipping of primitives.
 *
 * This file makes calls to project vertices and to the point, line
 * and triangle rasterizers via the function pointers:
 *
 *    context->Driver.Render.*
 *
 */


#include "glheader.h"
#include "context.h"
#include "enums.h"
#include "macros.h"
#include "imports.h"
#include "mtypes.h"

#include "t_pipeline.h"

/* VX855 v10.484: [flat][GL primitive][calls, vertices, calls through the clipping table] */
unsigned long _tnl_vx855_prims[2][GL_POLYGON+1][3];




/**********************************************************************/
/*                        Clip single primitives                      */
/**********************************************************************/


#define W(i) coord[i][3]
#define Z(i) coord[i][2]
#define Y(i) coord[i][1]
#define X(i) coord[i][0]
#define SIZE 4
#define TAG(x) x##_4
#include "t_vb_cliptmp.h"



/**********************************************************************/
/*              Clip and render whole begin/end objects               */
/**********************************************************************/

#define NEED_EDGEFLAG_SETUP (ctx->Polygon.FrontMode != GL_FILL || ctx->Polygon.BackMode != GL_FILL)
#define EDGEFLAG_GET(idx) VB->EdgeFlag[idx]
#define EDGEFLAG_SET(idx, val) VB->EdgeFlag[idx] = val


/* This does NOT include the CLIP_USER_BIT! */
#define CLIPMASK (CLIP_FRUSTUM_BITS | CLIP_CULL_BIT)


/* Vertices, with the possibility of clipping.
 */
#define RENDER_POINTS( start, count ) \
   tnl->Driver.Render.Points( ctx, start, count )

#define RENDER_LINE( v1, v2 )			\
do {						\
   GLubyte c1 = mask[v1], c2 = mask[v2];	\
   GLubyte ormask = c1|c2;			\
   if (!ormask)					\
      LineFunc( ctx, v1, v2 );			\
   else if (!(c1 & c2 & CLIPMASK))			\
      clip_line_4( ctx, v1, v2, ormask );	\
} while (0)

#define RENDER_TRI( v1, v2, v3 )			\
do {							\
   GLubyte c1 = mask[v1], c2 = mask[v2], c3 = mask[v3];	\
   GLubyte ormask = c1|c2|c3;				\
   if (!ormask)						\
      TriangleFunc( ctx, v1, v2, v3 );			\
   else if (!(c1 & c2 & c3 & CLIPMASK)) 			\
      clip_tri_4( ctx, v1, v2, v3, ormask );    	\
} while (0)

#define RENDER_QUAD( v1, v2, v3, v4 )			\
do {							\
   GLubyte c1 = mask[v1], c2 = mask[v2];		\
   GLubyte c3 = mask[v3], c4 = mask[v4];		\
   GLubyte ormask = c1|c2|c3|c4;			\
   if (!ormask)						\
      QuadFunc( ctx, v1, v2, v3, v4 );			\
   else if (!(c1 & c2 & c3 & c4 & CLIPMASK)) 		\
      clip_quad_4( ctx, v1, v2, v3, v4, ormask );	\
} while (0)


#define LOCAL_VARS						\
   TNLcontext *tnl = TNL_CONTEXT(ctx);				\
   struct vertex_buffer *VB = &tnl->vb;				\
   const GLuint * const elt = VB->Elts;				\
   const GLubyte *mask = VB->ClipMask;				\
   const GLuint sz = VB->ClipPtr->size;				\
   const tnl_line_func LineFunc = tnl->Driver.Render.Line;		\
   const tnl_triangle_func TriangleFunc = tnl->Driver.Render.Triangle;	\
   const tnl_quad_func QuadFunc = tnl->Driver.Render.Quad;		\
   const GLboolean stipple = ctx->Line.StippleFlag;		\
   (void) (LineFunc && TriangleFunc && QuadFunc);		\
   (void) elt; (void) mask; (void) sz; (void) stipple;

#define TAG(x) clip_##x##_verts
#define INIT(x) tnl->Driver.Render.PrimitiveNotify( ctx, x )
#define RESET_STIPPLE if (stipple) tnl->Driver.Render.ResetLineStipple( ctx )
#define PRESERVE_VB_DEFS
#include "t_vb_rendertmp.h"



/* Elts, with the possibility of clipping.
 */
#undef ELT
#undef TAG
#define ELT(x) elt[x]
#define TAG(x) clip_##x##_elts
#include "t_vb_rendertmp.h"

/* TODO: do this for all primitives, verts and elts:
 */
static void clip_elt_triangles( GLcontext *ctx,
				GLuint start,
				GLuint count,
				GLuint flags )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   tnl_render_func render_tris = tnl->Driver.Render.PrimTabElts[GL_TRIANGLES];
   struct vertex_buffer *VB = &tnl->vb;
   const GLuint * const elt = VB->Elts;
   GLubyte *mask = VB->ClipMask;
   GLuint last = count-2;
   GLuint j;
   (void) flags;

   tnl->Driver.Render.PrimitiveNotify( ctx, GL_TRIANGLES );

   for (j=start; j < last; j+=3 ) {
      GLubyte c1 = mask[elt[j]];
      GLubyte c2 = mask[elt[j+1]];
      GLubyte c3 = mask[elt[j+2]];
      GLubyte ormask = c1|c2|c3;
      if (ormask) {
	 if (start < j)
	    render_tris( ctx, start, j, 0 );
	 if (!(c1&c2&c3&CLIPMASK))
	    clip_tri_4( ctx, elt[j], elt[j+1], elt[j+2], ormask );
	 start = j+3;
      }
   }

   if (start < j)
      render_tris( ctx, start, j, 0 );
}

/* VX855 v10.485 (board, Lattice v10.484: 720,084 of 1,301,826 flat strips had a vertex outside, and Mesa's clipping table drew each of
 * them a triangle at a time): a triangle strip with a vertex outside, split. Each run of triangles none of whose vertices is outside,
 * from a triangle at an even place in the strip (the winding and the strip's parity as in the whole strip), goes to the driver's own
 * strip function; a triangle inside at an odd place before a run starts goes alone (Render.Triangle); every other triangle as Mesa's
 * clipping table: dropped when all its vertices are outside one plane, else clipped (clip_tri_4). The driver may gather the clipped
 * polygons while _tnl_vx855_clipwin is set: _tnl_vx855_clipflush draws them before anything else is drawn and at the end. Filled
 * polygons only (no edge flags); ACCEL_OPENGL_CLIPSTRIP 0 (the ICD): Mesa's clipping table as before. _tnl_vx855_clipstrip_stat:
 * [0] strips split, [1] runs sent as strips, [2] their triangles, [3] triangles inside sent alone, [4] triangles clipped. */
int _tnl_vx855_clipstrip = 1;
int _tnl_vx855_clipwin;
void (*_tnl_vx855_clipflush)(GLcontext *);
unsigned long _tnl_vx855_clipstrip_stat[5];
static void vx855_clip_strip( GLcontext *ctx, GLuint start, GLuint count, GLuint flags )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   const GLuint * const elt = VB->Elts;
   const GLubyte *mask = VB->ClipMask;
   tnl_render_func strip = (elt ? tnl->Driver.Render.PrimTabElts : tnl->Driver.Render.PrimTabVerts)[GL_TRIANGLE_STRIP];
   const tnl_triangle_func TriangleFunc = tnl->Driver.Render.Triangle;
   void (*flush)(GLcontext *) = _tnl_vx855_clipflush;
   unsigned long *st = _tnl_vx855_clipstrip_stat;
   GLuint j, parity = 0, run = 0, open = 0;
#define VXE(x) (elt ? elt[x] : (x))
#define VXFLUSH() do { if (flush) flush( ctx ); } while (0)
   st[0]++;
   _tnl_vx855_clipwin = 1;
   tnl->Driver.Render.PrimitiveNotify( ctx, GL_TRIANGLE_STRIP );
   for (j = start + 2; j < count; j++, parity ^= 1) {
      GLuint e2 = VXE(j-2+parity), e1 = VXE(j-1-parity), e0 = VXE(j);
      GLubyte c1 = mask[e2], c2 = mask[e1], c3 = mask[e0];
      if (!(c1 | c2 | c3)) {
	 if (open)
	    continue;
	 if (!parity) {
	    open = 1;
	    run = j - 2;
	    continue;
	 }
	 VXFLUSH();
	 TriangleFunc( ctx, e2, e1, e0 );
	 st[3]++;
	 continue;
      }
      if (open) {
	 VXFLUSH();
	 strip( ctx, run, j, flags );
	 st[1]++; st[2] += j - run - 2;
	 open = 0;
	 tnl->Driver.Render.PrimitiveNotify( ctx, GL_TRIANGLE_STRIP );
      }
      if (!(c1 & c2 & c3 & CLIPMASK)) {
	 clip_tri_4( ctx, e2, e1, e0, c1 | c2 | c3 );
	 st[4]++;
      }
   }
   if (open) {
      VXFLUSH();
      strip( ctx, run, count, flags );
      st[1]++; st[2] += count - run - 2;
   }
   VXFLUSH();
   _tnl_vx855_clipwin = 0;
#undef VXE
#undef VXFLUSH
}

/**********************************************************************/
/*                  Render whole begin/end objects                    */
/**********************************************************************/

#define NEED_EDGEFLAG_SETUP (ctx->Polygon.FrontMode != GL_FILL || ctx->Polygon.BackMode != GL_FILL)
#define EDGEFLAG_GET(idx) VB->EdgeFlag[idx]
#define EDGEFLAG_SET(idx, val) VB->EdgeFlag[idx] = val


/* Vertices, no clipping.
 */
#define RENDER_POINTS( start, count ) \
   tnl->Driver.Render.Points( ctx, start, count )

#define RENDER_LINE( v1, v2 ) \
   LineFunc( ctx, v1, v2 )

#define RENDER_TRI( v1, v2, v3 ) \
   TriangleFunc( ctx, v1, v2, v3 )

#define RENDER_QUAD( v1, v2, v3, v4 ) \
   QuadFunc( ctx, v1, v2, v3, v4 )

#define TAG(x) _tnl_##x##_verts

#define LOCAL_VARS						\
   TNLcontext *tnl = TNL_CONTEXT(ctx);				\
   struct vertex_buffer *VB = &tnl->vb;				\
   const GLuint * const elt = VB->Elts;				\
   const tnl_line_func LineFunc = tnl->Driver.Render.Line;		\
   const tnl_triangle_func TriangleFunc = tnl->Driver.Render.Triangle;	\
   const tnl_quad_func QuadFunc = tnl->Driver.Render.Quad;		\
   const GLboolean stipple = ctx->Line.StippleFlag;		\
   (void) (LineFunc && TriangleFunc && QuadFunc);		\
   (void) elt; (void) stipple

#define RESET_STIPPLE if (stipple) tnl->Driver.Render.ResetLineStipple( ctx )
#define INIT(x) tnl->Driver.Render.PrimitiveNotify( ctx, x )
#define RENDER_TAB_QUALIFIER
#define PRESERVE_VB_DEFS
#include "t_vb_rendertmp.h"


/* Elts, no clipping.
 */
#undef ELT
#define TAG(x) _tnl_##x##_elts
#define ELT(x) elt[x]
#include "t_vb_rendertmp.h"


/**********************************************************************/
/*              Helper functions for drivers                  */
/**********************************************************************/

void _tnl_RenderClippedPolygon( GLcontext *ctx, const GLuint *elts, GLuint n )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   GLuint *tmp = VB->Elts;

   VB->Elts = (GLuint *)elts;
   tnl->Driver.Render.PrimTabElts[GL_POLYGON]( ctx, 0, n, PRIM_BEGIN|PRIM_END );
   VB->Elts = tmp;
}

void _tnl_RenderClippedLine( GLcontext *ctx, GLuint ii, GLuint jj )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   tnl->Driver.Render.Line( ctx, ii, jj );
}



/**********************************************************************/
/*              Clip and render whole vertex buffers                  */
/**********************************************************************/


static GLboolean run_render( GLcontext *ctx,
			     struct tnl_pipeline_stage *stage )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   tnl_render_func *tab;
   GLint pass = 0;

   /* Allow the drivers to lock before projected verts are built so
    * that window coordinates are guarenteed not to change before
    * rendering.
    */
   ASSERT(tnl->Driver.Render.Start);

   tnl->Driver.Render.Start( ctx );

   ASSERT(tnl->Driver.Render.BuildVertices);
   ASSERT(tnl->Driver.Render.PrimitiveNotify);
   ASSERT(tnl->Driver.Render.Points);
   ASSERT(tnl->Driver.Render.Line);
   ASSERT(tnl->Driver.Render.Triangle);
   ASSERT(tnl->Driver.Render.Quad);
   ASSERT(tnl->Driver.Render.ResetLineStipple);
   ASSERT(tnl->Driver.Render.Interp);
   ASSERT(tnl->Driver.Render.CopyPV);
   ASSERT(tnl->Driver.Render.ClippedLine);
   ASSERT(tnl->Driver.Render.ClippedPolygon);
   ASSERT(tnl->Driver.Render.Finish);

   tnl->Driver.Render.BuildVertices( ctx, 0, VB->Count, ~0 );

   if (VB->ClipOrMask) {
      tab = VB->Elts ? clip_render_tab_elts : clip_render_tab_verts;
      clip_render_tab_elts[GL_TRIANGLES] = clip_elt_triangles;
   }
   else {
      tab = (VB->Elts ? 
	     tnl->Driver.Render.PrimTabElts : 
	     tnl->Driver.Render.PrimTabVerts);
   }

   do
   {
      GLuint i;

      for (i = 0 ; i < VB->PrimitiveCount ; i++)
      {
	 GLuint prim = _tnl_translate_prim(&VB->Primitive[i]);
	 GLuint start = VB->Primitive[i].start;
	 GLuint length = VB->Primitive[i].count;

	 assert((prim & PRIM_MODE_MASK) <= GL_POLYGON);

	 if (MESA_VERBOSE & VERBOSE_PRIMS) 
	    _mesa_debug(NULL, "MESA prim %s %d..%d\n", 
			_mesa_lookup_enum_by_nr(prim & PRIM_MODE_MASK), 
			start, start+length);

	 if (length) {
	    /* VXGL v10.348: a primitive none of whose vertices is outside (any clip bit) is drawn by the driver's own table, not the
	     * clipping one: the clipping table drew every primitive of a vertex buffer holding one clipped vertex a triangle at a time
	     * (Quake 2's world: most polygons of every buffer) */
	    tnl_render_func *t = tab;
	    if (VB->ClipOrMask) {
	       const GLubyte *cm = VB->ClipMask;GLubyte ormask = 0;GLuint j;
	       if (VB->Elts) {for (j = start ; j < start + length ; j++) ormask |= cm[VB->Elts[j]];}
	       else {for (j = start ; j < start + length ; j++) ormask |= cm[j];}
	       if (!ormask)
	          t = VB->Elts ? tnl->Driver.Render.PrimTabElts : tnl->Driver.Render.PrimTabVerts;
	    }
	    { /* VX855 v10.484: the primitives drawn, by kind, shading and path (the ICD's log at a context's end) */
	       unsigned long *ps = _tnl_vx855_prims[ctx->Light.ShadeModel == GL_FLAT][prim & PRIM_MODE_MASK];
	       ps[0]++; ps[1] += length; if (VB->ClipOrMask && t == tab) ps[2]++;
	    }
	    if (t == tab && VB->ClipOrMask && (prim & PRIM_MODE_MASK) == GL_TRIANGLE_STRIP && _tnl_vx855_clipstrip && length >= 3 &&
		ctx->Polygon.FrontMode == GL_FILL && ctx->Polygon.BackMode == GL_FILL)
	       vx855_clip_strip( ctx, start, start + length, prim ); /* VX855 v10.485 */
	    else
	    t[prim & PRIM_MODE_MASK]( ctx, start, start + length, prim );
	 }
      }
   } while (tnl->Driver.Render.Multipass &&
	    tnl->Driver.Render.Multipass( ctx, ++pass ));

   tnl->Driver.Render.Finish( ctx );

   return GL_FALSE;		/* finished the pipe */
}


/**********************************************************************/
/*                          Render pipeline stage                     */
/**********************************************************************/





const struct tnl_pipeline_stage _tnl_render_stage =
{
   "render",			/* name */
   NULL,			/* private data */
   NULL,			/* creator */
   NULL,			/* destructor */
   NULL,			/* validate */
   run_render			/* run */
};
