/*
 * Mesa 3-D graphics library
 * Version:  6.1
 *
 * Copyright (C) 1999-2004  Brian Paul   All Rights Reserved.
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
 */

/* Author:
 *    Keith Whitwell <keith@tungstengraphics.com>
 */

#include "glheader.h"
#include "context.h"
#include "imports.h"
#include "mtypes.h"
#include "macros.h"
#include "light.h"
#include "state.h"

#include "vbo_context.h"


/*
 * After playback, copy everything but the position from the
 * last vertex to the saved state
 */
static void _playback_copy_to_current( GLcontext *ctx,
				       const struct vbo_save_vertex_list *node )
{
   struct vbo_context *vbo = vbo_context(ctx);
   GLfloat vertex[VBO_ATTRIB_MAX * 4], *data = vertex;
   GLuint i, offset;

   if (node->count)
      offset = (node->buffer_offset + 
		(node->count-1) * node->vertex_size * sizeof(GLfloat));
   else
      offset = node->buffer_offset;

   ctx->Driver.GetBufferSubData( ctx, 0, offset, 
				 node->vertex_size * sizeof(GLfloat), 
				 data, node->vertex_store->bufferobj );

   data += node->attrsz[0]; /* skip vertex position */

   for (i = VBO_ATTRIB_POS+1 ; i < VBO_ATTRIB_MAX ; i++) {
      if (node->attrsz[i]) {
	 GLfloat *current = (GLfloat *)vbo->currval[i].Ptr;

	 COPY_CLEAN_4V(current, 
		       node->attrsz[i], 
		       data);

	 vbo->currval[i].Size = node->attrsz[i];

	 data += node->attrsz[i];

	 if (i >= VBO_ATTRIB_FIRST_MATERIAL &&
	     i <= VBO_ATTRIB_LAST_MATERIAL)
	    ctx->NewState |= _NEW_LIGHT;
      }
   }

   /* Colormaterial -- this kindof sucks.
    */
   if (ctx->Light.ColorMaterialEnabled) {
      _mesa_update_color_material(ctx, ctx->Current.Attrib[VBO_ATTRIB_COLOR0]);
   }

   /* CurrentExecPrimitive
    */
   if (node->prim_count) {
      const struct _mesa_prim *prim = &node->prim[node->prim_count - 1];
      if (prim->end)
	 ctx->Driver.CurrentExecPrimitive = PRIM_OUTSIDE_BEGIN_END;
      else
	 ctx->Driver.CurrentExecPrimitive = prim->mode;
   }
}



/* Treat the vertex storage as a VBO, define vertex arrays pointing
 * into it:
 */
int _vbo_vx855_listbind = 1; /* VX855 v10.495: ACCEL_OPENGL_LISTBIND */
unsigned long _vbo_vx855_listbind_n; /* VX855 v10.495: list draws bound by the short path */
static void vbo_bind_vertex_list( GLcontext *ctx,
                                   const struct vbo_save_vertex_list *node )
{
   struct vbo_context *vbo = vbo_context(ctx);
   struct vbo_save_context *save = &vbo->save;
   struct gl_client_array *arrays = save->arrays;
   GLuint data = node->buffer_offset;
   const GLuint *map;
   GLuint attr;

   /* Install the default (ie Current) attributes first, then overlay
    * all active ones.
    */
   GLuint over = 0;

   switch (get_program_mode(ctx)) {
   case VP_NONE:
      /* VX855 v10.495 (ACCEL_OPENGL_LISTBIND; board, Lattice v10.493: vbo_bind_vertex_list 0.7% of the samples, its 32 arrays copied a
       * draw): the arrays as the last list left them; only the slots it overlaid, and a slot whose current value's size changed since (the
       * one field of the current-value arrays that changes: vbo_exec_copy_to_current, _playback_copy_to_current), copied again. The arrays
       * the same as the copies below would make. */
      if (_vbo_vx855_listbind && save->vx855_bound) {
	 GLuint a;
	 for (a = 0; a < VERT_ATTRIB_MAX; a++) {
	    const struct gl_client_array *cv = a < 16 ? &vbo->legacy_currval[a] : a < 16 + MAT_ATTRIB_MAX ? &vbo->mat_currval[a - 16] : &vbo->legacy_currval[a - 16 - MAT_ATTRIB_MAX];
	    if (((save->vx855_over >> a) & 1) || arrays[a].Size != cv->Size)
	       arrays[a] = *cv;
	 }
	 _vbo_vx855_listbind_n++;
	 map = vbo->map_vp_none;
	 break;
      }
      memcpy(arrays,      vbo->legacy_currval, 16 * sizeof(arrays[0]));
      memcpy(arrays + 16, vbo->mat_currval,    MAT_ATTRIB_MAX * sizeof(arrays[0]));
      /* VX855 v10.490 (host check, a list recompiled 48 times: a read of a freed buffer object in bind_inputs): map_vp_none sends
       * attributes 28-31 to 0-3, so a node's position, weight, normal or colour overlays them below, and nothing reset them: a colour
       * of an earlier node (its vertex store freed with its list) stayed in 31 for a node without colours. Reset as the others. */
      memcpy(arrays + 16 + MAT_ATTRIB_MAX, vbo->legacy_currval, (VERT_ATTRIB_MAX - 16 - MAT_ATTRIB_MAX) * sizeof(arrays[0]));
      map = vbo->map_vp_none;
      break;
   case VP_NV:
   case VP_ARB:
      /* The aliasing of attributes for NV vertex programs has already
       * occurred.  NV vertex programs cannot access material values,
       * nor attributes greater than VERT_ATTRIB_TEX7.  
       */
      memcpy(arrays,      vbo->legacy_currval,  16 * sizeof(arrays[0]));
      memcpy(arrays + 16, vbo->generic_currval, 16 * sizeof(arrays[0]));
      map = vbo->map_vp_arb;
      break;
   }
   save->vx855_bound = map == vbo->map_vp_none; /* VX855 v10.495 */

   /* VX855 v10.493: the offset steps by the attribute the data is (src: was attr, so with a material a vertex the next one read the first's
    * place: a material is attribute 32 + m in the vertex, its tnl input 16 + m); the tnl inputs only (VERT_ATTRIB_MAX: was every vbo attribute,
    * map_vp_none's unset entries naming the position); without a program 28-31 not overlaid (they alias 0-3, past the vertex's last one: the
    * defaults installed above, unused by the fixed function) */
   for (attr = 0; attr < VERT_ATTRIB_MAX; attr++) {
      GLuint src = map[attr];

      if (map == vbo->map_vp_none && attr >= 16 + MAT_ATTRIB_MAX)
	 break;
      if (node->attrsz[src]) {
	 arrays[attr].Ptr = (const GLubyte *)data;
	 arrays[attr].Size = node->attrsz[src];
	 arrays[attr].StrideB = node->vertex_size * sizeof(GLfloat);
	 arrays[attr].Stride = node->vertex_size * sizeof(GLfloat);
	 arrays[attr].Type = GL_FLOAT;
	 arrays[attr].Enabled = 1;
	 arrays[attr].BufferObj = node->vertex_store->bufferobj;
	 arrays[attr]._MaxElement = node->count; /* ??? */
	 over |= 1U << attr; /* VX855 v10.495 */
	 
	 assert(arrays[attr].BufferObj->Name);

	 data += node->attrsz[src] * sizeof(GLfloat);
      }
   }
   save->vx855_over = over; /* VX855 v10.495 */
}

static void vbo_save_loopback_vertex_list( GLcontext *ctx,
					   const struct vbo_save_vertex_list *list )
{
   const char *buffer = ctx->Driver.MapBuffer(ctx, 
					      GL_ARRAY_BUFFER_ARB, 
					      GL_READ_ONLY, /* ? */
					       list->vertex_store->bufferobj);

   vbo_loopback_vertex_list( ctx,
			     (const GLfloat *)(buffer + list->buffer_offset),
			     list->attrsz,
			     list->prim,
			     list->prim_count,
			     list->wrap_count,
			     list->vertex_size);

   ctx->Driver.UnmapBuffer(ctx, GL_ARRAY_BUFFER_ARB, 
			   list->vertex_store->bufferobj);
}


/**
 * Execute the buffer and save copied verts.
 */
void vbo_save_playback_vertex_list( GLcontext *ctx, void *data )
{
   const struct vbo_save_vertex_list *node = (const struct vbo_save_vertex_list *) data;
   struct vbo_save_context *save = &vbo_context(ctx)->save;

   FLUSH_CURRENT(ctx, 0);

   if (node->prim_count > 0 && node->count > 0) {

      if (ctx->Driver.CurrentExecPrimitive != PRIM_OUTSIDE_BEGIN_END &&
	  node->prim[0].begin) {

	 /* Degenerate case: list is called inside begin/end pair and
	  * includes operations such as glBegin or glDrawArrays.
	  */
	 if (0)
	    _mesa_printf("displaylist recursive begin");

	 vbo_save_loopback_vertex_list( ctx, node );
	 return;
      }
      else if (save->replay_flags) {
	 /* Various degnerate cases: translate into immediate mode
	  * calls rather than trying to execute in place.
	  */
	 vbo_save_loopback_vertex_list( ctx, node );
	 return;
      }
      
      if (ctx->NewState)
	 _mesa_update_state( ctx );

      /* XXX also need to check if shader enabled, but invalid */
      if ((ctx->VertexProgram.Enabled && !ctx->VertexProgram._Enabled) ||
          (ctx->FragmentProgram.Enabled && !ctx->FragmentProgram._Enabled)) {
         _mesa_error(ctx, GL_INVALID_OPERATION,
                     "glBegin (invalid vertex/fragment program)");
         return;
      }

      vbo_bind_vertex_list( ctx, node );

      save->vx855_node = node; /* VX855 v10.487: the ICD's SSE pass (vxgl_ssepass.c) takes this draw from the node's SoA copy */
      vbo_context(ctx)->draw_prims( ctx, 
				    save->inputs, 
				    node->prim, 
				    node->prim_count,
				    NULL,
				    0,	/* Node is a VBO, so this is ok */
				    node->count - 1);
      save->vx855_node = NULL;
   }

   /* Copy to current?
    */
   _playback_copy_to_current( ctx, node );
}
