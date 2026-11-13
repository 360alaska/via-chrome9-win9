/**************************************************************************
 * 
 * Copyright 2003 Tungsten Graphics, Inc., Cedar Park, Texas.
 * All Rights Reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT.
 * IN NO EVENT SHALL TUNGSTEN GRAPHICS AND/OR ITS SUPPLIERS BE LIABLE FOR
 * ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 **************************************************************************/

#include "glheader.h"
#include "context.h"
#include "state.h"
#include "api_validate.h"
#include "api_noop.h"
#include "dispatch.h"

#include "vbo_context.h"

extern int _vbo_vx855_fastindex; /* VX855 v10.501 (vbo_rebase.c) */
/* VX855 .506 (ACCEL_OPENGL_MINMAX2; Fable's: host bench, Quake 3's batches: the index scan 5% of the driver's time): the scan by SSE2
 * (the ICD sets _vbo_vx855_minmax2 when the processor has it), four 32-bit or eight 16-bit indices a step: the indices biased by the sign
 * bit (an unsigned order as a signed one), the lane minimums and maximums kept (32-bit: pcmpgtd and a masked blend; 16-bit: pminsw,
 * pmaxsw), the lanes reduced in C, the indices past the last whole step one at a time. The same minimum and maximum. */
int _vbo_vx855_minmax2 = 0;
unsigned long _vbo_vx855_minmax2_n;
#if defined(__WATCOMC__) && defined(__386__)
static void vx_mm32(const GLuint *p, GLuint nblk, GLuint *out);
#pragma aux vx_mm32 = "pcmpeqd xmm7,xmm7" "pslld xmm7,31" \
 "movdqu xmm0,[esi]" "pxor xmm0,xmm7" "movdqa xmm1,xmm0" "add esi,16" "dec ecx" "jz M32E" \
 "M32L: movdqu xmm2,[esi]" "pxor xmm2,xmm7" \
 "movdqa xmm3,xmm0" "pcmpgtd xmm3,xmm2" "movdqa xmm4,xmm0" "pxor xmm4,xmm2" "pand xmm4,xmm3" "pxor xmm0,xmm4" \
 "movdqa xmm3,xmm2" "pcmpgtd xmm3,xmm1" "movdqa xmm4,xmm1" "pxor xmm4,xmm2" "pand xmm4,xmm3" "pxor xmm1,xmm4" \
 "add esi,16" "dec ecx" "jnz M32L" \
 "M32E: pxor xmm0,xmm7" "pxor xmm1,xmm7" "movdqu [edi],xmm0" "movdqu [edi+16],xmm1" \
 parm [esi] [ecx] [edi] modify exact [esi ecx];
static void vx_mm16(const GLushort *p, GLuint nblk, GLushort *out);
#pragma aux vx_mm16 = "pcmpeqw xmm7,xmm7" "psllw xmm7,15" \
 "movdqu xmm0,[esi]" "pxor xmm0,xmm7" "movdqa xmm1,xmm0" "add esi,16" "dec ecx" "jz M16E" \
 "M16L: movdqu xmm2,[esi]" "pxor xmm2,xmm7" "pminsw xmm0,xmm2" "pmaxsw xmm1,xmm2" "add esi,16" "dec ecx" "jnz M16L" \
 "M16E: pxor xmm0,xmm7" "pxor xmm1,xmm7" "movdqu [edi],xmm0" "movdqu [edi+16],xmm1" \
 parm [esi] [ecx] [edi] modify exact [esi ecx];
#define VX_MM2 1
#elif defined(__GNUC__) && defined(__SSE2__)
#include <emmintrin.h>
static void vx_mm32(const GLuint *p, GLuint nblk, GLuint *out)
{
   const __m128i b = _mm_slli_epi32(_mm_set1_epi32(-1), 31);
   __m128i mn = _mm_xor_si128(_mm_loadu_si128((const __m128i *) p), b), mx = mn, v, m;
   GLuint k;
   for (k = 1; k < nblk; k++) {
      v = _mm_xor_si128(_mm_loadu_si128((const __m128i *) (p + 4 * k)), b);
      m = _mm_cmpgt_epi32(mn, v); mn = _mm_xor_si128(mn, _mm_and_si128(_mm_xor_si128(mn, v), m));
      m = _mm_cmpgt_epi32(v, mx); mx = _mm_xor_si128(mx, _mm_and_si128(_mm_xor_si128(mx, v), m));
   }
   _mm_storeu_si128((__m128i *) out, _mm_xor_si128(mn, b));
   _mm_storeu_si128((__m128i *) (out + 4), _mm_xor_si128(mx, b));
}
static void vx_mm16(const GLushort *p, GLuint nblk, GLushort *out)
{
   const __m128i b = _mm_slli_epi16(_mm_set1_epi16(-1), 15);
   __m128i mn = _mm_xor_si128(_mm_loadu_si128((const __m128i *) p), b), mx = mn, v;
   GLuint k;
   for (k = 1; k < nblk; k++) {
      v = _mm_xor_si128(_mm_loadu_si128((const __m128i *) (p + 8 * k)), b);
      mn = _mm_min_epi16(mn, v); mx = _mm_max_epi16(mx, v);
   }
   _mm_storeu_si128((__m128i *) out, _mm_xor_si128(mn, b));
   _mm_storeu_si128((__m128i *) (out + 8), _mm_xor_si128(mx, b));
}
#define VX_MM2 1
#endif
/* Compute min and max elements for drawelements calls.
 */
static void get_minmax_index( GLuint count, GLuint type, 
			      const GLvoid *indices,
			      GLuint *min_index,
			      GLuint *max_index)
{
   GLint i;

   /* VX855 v10.501 (ACCEL_OPENGL_FASTINDEX): the indices two at a time, the larger of the pair against the maximum and the smaller against
    * the minimum (one and a half compares an index instead of two); the same minimum and maximum */
#ifdef VX_MM2
   if (_vbo_vx855_minmax2 && (type == GL_UNSIGNED_INT ? count >= 8 : type == GL_UNSIGNED_SHORT && count >= 16)) {
      GLuint mn, mx, k, n;
      if (type == GL_UNSIGNED_INT) {
         const GLuint *u = (const GLuint *) indices; GLuint o[8];
         n = count & ~3U; vx_mm32(u, n >> 2, o);
         mn = o[0]; mx = o[4];
         for (k = 1; k < 4; k++) { if (o[k] < mn) mn = o[k]; if (o[4 + k] > mx) mx = o[4 + k]; }
         for (k = n; k < count; k++) { if (u[k] < mn) mn = u[k]; if (u[k] > mx) mx = u[k]; }
      }
      else {
         const GLushort *u = (const GLushort *) indices; GLushort o[16];
         n = count & ~7U; vx_mm16(u, n >> 3, o);
         mn = o[0]; mx = o[8];
         for (k = 1; k < 8; k++) { if (o[k] < mn) mn = o[k]; if (o[8 + k] > mx) mx = o[8 + k]; }
         for (k = n; k < count; k++) { if (u[k] < mn) mn = u[k]; if (u[k] > mx) mx = u[k]; }
      }
      _vbo_vx855_minmax2_n++;
      *min_index = mn;
      *max_index = mx;
      return;
   }
#endif
   if (_vbo_vx855_fastindex && count > 1 && (type == GL_UNSIGNED_INT || type == GL_UNSIGNED_SHORT)) {
      GLuint mn, mx, k = 1;
      if (type == GL_UNSIGNED_INT) {
         const GLuint *u = (const GLuint *) indices;
         mn = mx = u[0];
         for (; k + 1 < count; k += 2) {
            GLuint a = u[k], b = u[k+1];
            if (a > b) { if (a > mx) mx = a; if (b < mn) mn = b; }
            else { if (b > mx) mx = b; if (a < mn) mn = a; }
         }
         if (k < count) { if (u[k] > mx) mx = u[k]; if (u[k] < mn) mn = u[k]; }
      }
      else {
         const GLushort *u = (const GLushort *) indices;
         mn = mx = u[0];
         for (; k + 1 < count; k += 2) {
            GLuint a = u[k], b = u[k+1];
            if (a > b) { if (a > mx) mx = a; if (b < mn) mn = b; }
            else { if (b > mx) mx = b; if (a < mn) mn = a; }
         }
         if (k < count) { if (u[k] > mx) mx = u[k]; if (u[k] < mn) mn = u[k]; }
      }
      *min_index = mn;
      *max_index = mx;
      return;
   }

   switch(type) {
   case GL_UNSIGNED_INT: {
      const GLuint *ui_indices = (const GLuint *)indices;
      GLuint max_ui = ui_indices[0];
      GLuint min_ui = ui_indices[0];
      for (i = 1; i < count; i++) {
	 if (ui_indices[i] > max_ui) max_ui = ui_indices[i];
	 if (ui_indices[i] < min_ui) min_ui = ui_indices[i];
      }
      *min_index = min_ui;
      *max_index = max_ui;
      break;
   }
   case GL_UNSIGNED_SHORT: {
      const GLushort *us_indices = (const GLushort *)indices;
      GLuint max_us = us_indices[0];
      GLuint min_us = us_indices[0];
      for (i = 1; i < count; i++) {
	 if (us_indices[i] > max_us) max_us = us_indices[i];
	 if (us_indices[i] < min_us) min_us = us_indices[i];
      }
      *min_index = min_us;
      *max_index = max_us;
      break;
   }
   case GL_UNSIGNED_BYTE: {
      const GLubyte *ub_indices = (const GLubyte *)indices;
      GLuint max_ub = ub_indices[0];
      GLuint min_ub = ub_indices[0];
      for (i = 1; i < count; i++) {
	 if (ub_indices[i] > max_ub) max_ub = ub_indices[i];
	 if (ub_indices[i] < min_ub) min_ub = ub_indices[i];
      }
      *min_index = min_ub;
      *max_index = max_ub;
      break;
   }
   default:
      assert(0);
      break;
   }
}


/* Just translate the arrayobj into a sane layout.
 */
static void bind_array_obj( GLcontext *ctx )
{
   struct vbo_context *vbo = vbo_context(ctx);
   struct vbo_exec_context *exec = &vbo->exec;
   GLuint i;

   /* TODO: Fix the ArrayObj struct to keep legacy arrays in an array
    * rather than as individual named arrays.  Then this function can
    * go away.
    */
   exec->array.legacy_array[VERT_ATTRIB_POS] = &ctx->Array.ArrayObj->Vertex;
   exec->array.legacy_array[VERT_ATTRIB_WEIGHT] = &vbo->legacy_currval[VERT_ATTRIB_WEIGHT];
   exec->array.legacy_array[VERT_ATTRIB_NORMAL] = &ctx->Array.ArrayObj->Normal;
   exec->array.legacy_array[VERT_ATTRIB_COLOR0] = &ctx->Array.ArrayObj->Color;
   exec->array.legacy_array[VERT_ATTRIB_COLOR1] = &ctx->Array.ArrayObj->SecondaryColor;
   exec->array.legacy_array[VERT_ATTRIB_FOG] = &ctx->Array.ArrayObj->FogCoord;
   exec->array.legacy_array[VERT_ATTRIB_COLOR_INDEX] = &ctx->Array.ArrayObj->Index;
   exec->array.legacy_array[VERT_ATTRIB_EDGEFLAG] = &ctx->Array.ArrayObj->EdgeFlag;

   for (i = 0; i < 8; i++)
      exec->array.legacy_array[VERT_ATTRIB_TEX0 + i] = &ctx->Array.ArrayObj->TexCoord[i];

   for (i = 0; i < VERT_ATTRIB_MAX; i++)
      exec->array.generic_array[i] = &ctx->Array.ArrayObj->VertexAttrib[i];
   
   exec->array.array_obj = ctx->Array.ArrayObj->Name;
}

static void recalculate_input_bindings( GLcontext *ctx )
{
   struct vbo_context *vbo = vbo_context(ctx);
   struct vbo_exec_context *exec = &vbo->exec;
   const struct gl_client_array **inputs = &exec->array.inputs[0];
   GLuint i;

   exec->array.program_mode = get_program_mode(ctx);
   exec->array.enabled_flags = ctx->Array.ArrayObj->_Enabled;

   switch (exec->array.program_mode) {
   case VP_NONE:
      /* When no vertex program is active, we put the material values
       * into the generic slots.  This is the only situation where
       * material values are available as per-vertex attributes.
       */
      for (i = 0; i <= VERT_ATTRIB_TEX7; i++) {
	 if (exec->array.legacy_array[i]->Enabled)
	    inputs[i] = exec->array.legacy_array[i];
	 else
	    inputs[i] = &vbo->legacy_currval[i];
      }

      for (i = 0; i < MAT_ATTRIB_MAX; i++) {
	 inputs[VERT_ATTRIB_GENERIC0 + i] = &vbo->mat_currval[i];
      }

      /* Could use just about anything, just to fill in the empty
       * slots:
       */
      for (i = MAT_ATTRIB_MAX; i < VERT_ATTRIB_MAX - VERT_ATTRIB_GENERIC0; i++)
	 inputs[VERT_ATTRIB_GENERIC0 + i] = &vbo->generic_currval[i];

      break;
   case VP_NV:
      /* NV_vertex_program - attribute arrays alias and override
       * conventional, legacy arrays.  No materials, and the generic
       * slots are vacant.
       */
      for (i = 0; i <= VERT_ATTRIB_TEX7; i++) {
	 if (exec->array.generic_array[i]->Enabled)
	    inputs[i] = exec->array.generic_array[i];
	 else if (exec->array.legacy_array[i]->Enabled)
	    inputs[i] = exec->array.legacy_array[i];
	 else
	    inputs[i] = &vbo->legacy_currval[i];
      }

      /* Could use just about anything, just to fill in the empty
       * slots:
       */
      for (i = VERT_ATTRIB_GENERIC0; i < VERT_ATTRIB_MAX; i++)
	 inputs[i] = &vbo->generic_currval[i - VERT_ATTRIB_GENERIC0];

      break;
   case VP_ARB:
      /* ARB_vertex_program - Only the attribute zero (position) array
       * aliases and overrides the legacy position array.  
       *
       * Otherwise, legacy attributes available in the legacy slots,
       * generic attributes in the generic slots and materials are not
       * available as per-vertex attributes.
       */
      if (exec->array.generic_array[0]->Enabled)
	 inputs[0] = exec->array.generic_array[0];
      else if (exec->array.legacy_array[0]->Enabled)
	 inputs[0] = exec->array.legacy_array[0];
      else
	 inputs[0] = &vbo->legacy_currval[0];


      for (i = 1; i <= VERT_ATTRIB_TEX7; i++) {
	 if (exec->array.legacy_array[i]->Enabled)
	    inputs[i] = exec->array.legacy_array[i];
	 else
	    inputs[i] = &vbo->legacy_currval[i];
      }

      for (i = 0; i < 16; i++) {
	 if (exec->array.generic_array[i]->Enabled)
	    inputs[VERT_ATTRIB_GENERIC0 + i] = exec->array.generic_array[i];
	 else
	    inputs[VERT_ATTRIB_GENERIC0 + i] = &vbo->generic_currval[i];
      }
      break;
   }
}

static void bind_arrays( GLcontext *ctx )
{
#if 0
   if (ctx->Array.ArrayObj.Name != exec->array.array_obj) {
      bind_array_obj(ctx);
      recalculate_input_bindings(ctx);
   }
   else if (exec->array.program_mode != get_program_mode(ctx) ||
	    exec->array.enabled_flags != ctx->Array.ArrayObj->_Enabled) {
      
      recalculate_input_bindings(ctx);
   }
#else
   bind_array_obj(ctx);
   recalculate_input_bindings(ctx);
#endif
}



/* VX855 .506: the host's check of the SSE2 scan against the C scans: random index arrays of every length 1-80 and a few long ones, values
 * across the whole range (0, 0x7FFF/0x8000 and 0x7FFFFFFF/0x80000000 edges, 0xFFFF, 0xFFFFFFFF), both types: the number of mismatches */
int _vbo_vx855_minmax_check(void)
{
   static GLuint u[4096]; static GLushort w[4096];
   GLuint seed = 12345, n, t, k, bad = 0, a1, b1, a2, b2;
   int keep2 = _vbo_vx855_minmax2, keepf = _vbo_vx855_fastindex;
   for (t = 0; t < 3000; t++) {
      n = t < 2400 ? 1 + t % 80 : 1 + (seed >> 8) % 4096;
      for (k = 0; k < n; k++) {
         seed = seed * 1103515245U + 12345U;
         switch ((seed >> 16) % 6) {
         case 0: u[k] = seed; break;
         case 1: u[k] = (seed >> 3) & 0xFFFF; break;
         case 2: u[k] = 0x7FFFFFFFU + ((seed >> 20) & 3) - 1; break;
         case 3: u[k] = (seed >> 25) & 1 ? 0xFFFFFFFFU : 0; break;
         case 4: u[k] = 0x7FFF + ((seed >> 20) & 3) - 1; break;
         default: u[k] = (seed >> 10) & 0xFFFF0000U; break;
         }
         w[k] = (GLushort) (u[k] ^ (u[k] >> 16));
      }
      _vbo_vx855_minmax2 = 1; get_minmax_index(n, GL_UNSIGNED_INT, u, &a1, &b1);
      _vbo_vx855_minmax2 = 0; _vbo_vx855_fastindex = 0; get_minmax_index(n, GL_UNSIGNED_INT, u, &a2, &b2); _vbo_vx855_fastindex = keepf;
      if (a1 != a2 || b1 != b2) bad++;
      _vbo_vx855_minmax2 = 1; get_minmax_index(n, GL_UNSIGNED_SHORT, w, &a1, &b1);
      _vbo_vx855_minmax2 = 0; _vbo_vx855_fastindex = 0; get_minmax_index(n, GL_UNSIGNED_SHORT, w, &a2, &b2); _vbo_vx855_fastindex = keepf;
      if (a1 != a2 || b1 != b2) bad++;
   }
   _vbo_vx855_minmax2 = keep2;
   return (int) bad;
}

/***********************************************************************
 * API functions.
 */

/* VX855 v10.500: set while glDrawArrays or glDrawElements draws from client arrays (not a buffer object, not glBegin/glEnd, not a list):
 * the only draws whose positions tnl's locked-array transform cache may keep (t_vb_vertex.c) */
int _vbo_vx855_arraydraw;

static void GLAPIENTRY
vbo_exec_DrawArrays(GLenum mode, GLint start, GLsizei count)
{
   GET_CURRENT_CONTEXT(ctx);
   struct vbo_context *vbo = vbo_context(ctx);
   struct vbo_exec_context *exec = &vbo->exec;
   struct _mesa_prim prim[1];

   if (!_mesa_validate_DrawArrays( ctx, mode, start, count ))
      return;

   FLUSH_CURRENT( ctx, 0 );

   if (ctx->NewState)
      _mesa_update_state( ctx );
      
   if (!vbo_validate_shaders(ctx)) {
      _mesa_error(ctx, GL_INVALID_OPERATION, "glDrawArrays(bad shader)");
      return;
   }

   bind_arrays( ctx );

   prim[0].begin = 1;
   prim[0].end = 1;
   prim[0].weak = 0;
   prim[0].pad = 0;
   prim[0].mode = mode;
   prim[0].start = start;
   prim[0].count = count;
   prim[0].indexed = 0;

   _vbo_vx855_arraydraw = !ctx->Array.ArrayObj->Vertex.BufferObj->Name; /* VX855 v10.500 */
   vbo->draw_prims( ctx, exec->array.inputs, prim, 1, NULL, start, start + count - 1 );
   _vbo_vx855_arraydraw = 0;
}



static void GLAPIENTRY
vbo_exec_DrawRangeElements(GLenum mode,
			   GLuint start, GLuint end,
			   GLsizei count, GLenum type, const GLvoid *indices)
{
   GET_CURRENT_CONTEXT(ctx);
   struct vbo_context *vbo = vbo_context(ctx);
   struct vbo_exec_context *exec = &vbo->exec;
   struct _mesa_index_buffer ib;
   struct _mesa_prim prim[1];

   if (!_mesa_validate_DrawRangeElements( ctx, mode, start, end, count, type, indices ))
      return;

   FLUSH_CURRENT( ctx, 0 );

   if (ctx->NewState)
      _mesa_update_state( ctx );

   if (!vbo_validate_shaders(ctx)) {
      _mesa_error(ctx, GL_INVALID_OPERATION, "glDrawRangeElements(bad shader)");
      return;
   }

   bind_arrays( ctx );

   ib.count = count;
   ib.type = type; 
   ib.obj = ctx->Array.ElementArrayBufferObj;
   ib.ptr = indices;

   prim[0].begin = 1;
   prim[0].end = 1;
   prim[0].weak = 0;
   prim[0].pad = 0;
   prim[0].mode = mode;
   prim[0].start = 0;
   prim[0].count = count;
   prim[0].indexed = 1;

   /* Need to give special consideration to rendering a range of
    * indices starting somewhere above zero.  Typically the
    * application is issuing multiple DrawRangeElements() to draw
    * successive primitives layed out linearly in the vertex arrays.
    * Unless the vertex arrays are all in a VBO (or locked as with
    * CVA), the OpenGL semantics imply that we need to re-read or
    * re-upload the vertex data on each draw call.  
    *
    * In the case of hardware tnl, we want to avoid starting the
    * upload at zero, as it will mean every draw call uploads an
    * increasing amount of not-used vertex data.  Worse - in the
    * software tnl module, all those vertices might be transformed and
    * lit but never rendered.
    *
    * If we just upload or transform the vertices in start..end,
    * however, the indices will be incorrect.
    *
    * At this level, we don't know exactly what the requirements of
    * the backend are going to be, though it will likely boil down to
    * either:
    *
    * 1) Do nothing, everything is in a VBO and is processed once
    *       only.
    *
    * 2) Adjust the indices and vertex arrays so that start becomes
    *    zero.
    *
    * Rather than doing anything here, I'll provide a helper function
    * for the latter case elsewhere.
    */

   _vbo_vx855_arraydraw = !ctx->Array.ArrayObj->Vertex.BufferObj->Name; /* VX855 v10.500 */
   vbo->draw_prims( ctx, exec->array.inputs, prim, 1, &ib, start, end );
   _vbo_vx855_arraydraw = 0;
}

static void GLAPIENTRY
vbo_exec_DrawElements(GLenum mode, GLsizei count, GLenum type, const GLvoid *indices)
{
   GET_CURRENT_CONTEXT(ctx);
   GLuint min_index = 0;
   GLuint max_index = 0;

   if (!_mesa_validate_DrawElements( ctx, mode, count, type, indices ))
      return;

   if (!vbo_validate_shaders(ctx)) {
      _mesa_error(ctx, GL_INVALID_OPERATION, "glDrawElements(bad shader)");
      return;
   }

   if (ctx->Array.ElementArrayBufferObj->Name) {
      const GLvoid *map = ctx->Driver.MapBuffer(ctx,
						 GL_ELEMENT_ARRAY_BUFFER_ARB,
						 GL_READ_ONLY,
						 ctx->Array.ElementArrayBufferObj);

      get_minmax_index(count, type, ADD_POINTERS(map, indices), &min_index, &max_index);

      ctx->Driver.UnmapBuffer(ctx,
			      GL_ELEMENT_ARRAY_BUFFER_ARB,
			      ctx->Array.ElementArrayBufferObj);
   }
   else {
      get_minmax_index(count, type, indices, &min_index, &max_index);
   }

   vbo_exec_DrawRangeElements(mode, min_index, max_index, count, type, indices);
}


/***********************************************************************
 * Initialization
 */




void vbo_exec_array_init( struct vbo_exec_context *exec )
{
#if 1
   exec->vtxfmt.DrawArrays = vbo_exec_DrawArrays;
   exec->vtxfmt.DrawElements = vbo_exec_DrawElements;
   exec->vtxfmt.DrawRangeElements = vbo_exec_DrawRangeElements;
#else
   exec->vtxfmt.DrawArrays = _mesa_noop_DrawArrays;
   exec->vtxfmt.DrawElements = _mesa_noop_DrawElements;
   exec->vtxfmt.DrawRangeElements = _mesa_noop_DrawRangeElements;
#endif
}


void vbo_exec_array_destroy( struct vbo_exec_context *exec )
{
   /* nothing to do */
}
