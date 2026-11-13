/*
 * Mesa 3-D graphics library
 * Version:  6.5
 *
 * Copyright (C) 1999-2006  Brian Paul   All Rights Reserved.
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



#include "glheader.h"
#include "colormac.h"
#include "light.h"
#include "macros.h"
#include "imports.h"
#include "simple_list.h"
#include "mtypes.h"

#include "math/m_translate.h"

#include "t_context.h"
#include "t_pipeline.h"

/* VX855 v10.484: the lighting's normalizations (local viewer, half vectors) by vx_inv_sqrtf (SSE with ACCEL_OPENGL_RSQRT) */
#undef INV_SQRTF
#define INV_SQRTF(X) vx_inv_sqrtf(X)

#define LIGHT_TWOSIDE       0x1
#define LIGHT_MATERIAL      0x2
#define MAX_LIGHT_FUNC      0x4

typedef void (*light_func)( GLcontext *ctx,
			    struct vertex_buffer *VB,
			    struct tnl_pipeline_stage *stage,
			    GLvector4f *input );

/**
 * Information for updating current material attributes from vertex color,
 * for GL_COLOR_MATERIAL.
 */
struct material_cursor {
   const GLfloat *ptr;    /* points to src vertex color (in VB array) */
   GLuint stride;         /* stride to next vertex color (bytes) */
   GLfloat *current;      /* points to material attribute to update */
   GLuint size;           /* vertex/color size: 1, 2, 3 or 4 */
};

/**
 * Data private to this pipeline stage.
 */
struct light_stage_data {
   GLvector4f Input;
   GLvector4f LitColor[2];
   GLvector4f LitSecondary[2];
   GLvector4f LitIndex[2];
   light_func *light_func_tab;

   struct material_cursor mat[MAT_ATTRIB_MAX];
   GLuint mat_count;
   GLuint mat_bitmask;
};


#define LIGHT_STAGE_DATA(stage) ((struct light_stage_data *)(stage->privatePtr))



/**
 * In the case of colormaterial, the effected material attributes
 * should already have been bound to point to the incoming color data,
 * prior to running the pipeline.
 * This function copies the vertex's color to the material attributes
 * which are tracking glColor.
 * It's called per-vertex in the lighting loop.
 */
static void
update_materials(GLcontext *ctx, struct light_stage_data *store)
{
   GLuint i;

   for (i = 0 ; i < store->mat_count ; i++) {
      /* update the material */
      COPY_CLEAN_4V(store->mat[i].current, store->mat[i].size, store->mat[i].ptr);
      /* increment src vertex color pointer */
      STRIDE_F(store->mat[i].ptr, store->mat[i].stride);
   }
      
   /* recompute derived light/material values */
   _mesa_update_material( ctx, store->mat_bitmask );
   /* XXX we should only call this if we're tracking/changing the specular
    * exponent.
    */
   _mesa_validate_all_lighting_tables( ctx );
}


/**
 * Prepare things prior to running the lighting stage.
 * Return number of material attributes which will track vertex color.
 */
static GLuint
prepare_materials(GLcontext *ctx,
                  struct vertex_buffer *VB, struct light_stage_data *store)
{
   GLuint i;
   
   store->mat_count = 0;
   store->mat_bitmask = 0;

   /* Examine the ColorMaterialBitmask to determine which materials
    * track vertex color.  Override the material attribute's pointer
    * with the color pointer for each one.
    */
   if (ctx->Light.ColorMaterialEnabled) {
      const GLuint bitmask = ctx->Light.ColorMaterialBitmask;
      for (i = 0 ; i < MAT_ATTRIB_MAX ; i++)
	 if (bitmask & (1<<i))
	    VB->AttribPtr[_TNL_ATTRIB_MAT_FRONT_AMBIENT + i] = VB->ColorPtr[0];
   }

   /* Now, for each material attribute that's tracking vertex color, save
    * some values (ptr, stride, size, current) that we'll need in
    * update_materials(), above, that'll actually copy the vertex color to
    * the material attribute(s).
    */
   for (i = _TNL_FIRST_MAT; i <= _TNL_LAST_MAT; i++) {
      if (VB->AttribPtr[i]->stride) {
	 const GLuint j = store->mat_count++;
	 const GLuint attr = i - _TNL_ATTRIB_MAT_FRONT_AMBIENT;
	 store->mat[j].ptr    = VB->AttribPtr[i]->start;
	 store->mat[j].stride = VB->AttribPtr[i]->stride;
	 store->mat[j].size   = VB->AttribPtr[i]->size;
	 store->mat[j].current = ctx->Light.Material.Attrib[attr];
	 store->mat_bitmask |= (1<<attr);
      }
   }

   /* FIXME: Is this already done?
    */
   _mesa_update_material( ctx, ~0 );
   _mesa_validate_all_lighting_tables( ctx );

   return store->mat_count;
}

/* Tables for all the shading functions.
 */
static light_func _tnl_light_tab[MAX_LIGHT_FUNC];
static light_func _tnl_light_fast_tab[MAX_LIGHT_FUNC];
static light_func _tnl_light_fast_single_tab[MAX_LIGHT_FUNC];
static light_func _tnl_light_spec_tab[MAX_LIGHT_FUNC];
static light_func _tnl_light_ci_tab[MAX_LIGHT_FUNC];

#define TAG(x)           x
#define IDX              (0)
#include "t_vb_lighttmp.h"

#define TAG(x)           x##_twoside
#define IDX              (LIGHT_TWOSIDE)
#include "t_vb_lighttmp.h"

#define TAG(x)           x##_material
#define IDX              (LIGHT_MATERIAL)
#include "t_vb_lighttmp.h"

#define TAG(x)           x##_twoside_material
#define IDX              (LIGHT_TWOSIDE|LIGHT_MATERIAL)
#include "t_vb_lighttmp.h"


/* VX855 v10.485 (board, Lattice v10.484: Mesa's general lighting 18.5% of the samples, spread over its whole loop: the list of lights,
 * the flag tests, its float compares through the FPU's status word): light_rgba's case (RGBA, no separate specular colour, one side) in a
 * loop of its own: the compares on the floats' bits (n.VP < 0: negative and not -0; attenuation < 0.001; n.h > 0), the same arithmetic in
 * the same order as light_rgba. v10.486 (board, Lattice v10.485: light_rgba 9.9%, the v10.485 loop not taken: it took one positional
 * light without a spotlight only): every light of light_rgba's case, directional, positional and spotlights, local viewer or not, colour
 * material or not. ACCEL_OPENGL_LIGHTFAST 0 (the ICD): light_rgba as before. _tnl_vx855_light_stat (VXGL.LOG): [0] runs of this loop,
 * [1] of light_rgba, [2] of light_rgba_spec (separate specular colour), [3] of Mesa's fast functions (no positional light, no local
 * viewer), [4] two-sided runs, [5] colour index runs; [6] vertices lit by this loop, [7] by the others. */
unsigned long _tnl_vx855_light_stat[8];
static INLINE GLint vxbits(GLfloat f) {union {GLfloat f; GLint i;} u; u.f = f; return u.i;}
static void light_vx855( GLcontext *ctx, struct vertex_buffer *VB, struct tnl_pipeline_stage *stage, GLvector4f *input, int material )
{
   struct light_stage_data *store = LIGHT_STAGE_DATA(stage);
   const GLuint vstride = input->stride;
   const GLfloat *vertex = (GLfloat *) input->data;
   const GLuint nstride = VB->AttribPtr[_TNL_ATTRIB_NORMAL]->stride;
   const GLfloat *normal = (GLfloat *) VB->AttribPtr[_TNL_ATTRIB_NORMAL]->data;
   GLfloat (*Fcolor)[4] = (GLfloat (*)[4]) store->LitColor[0].data;
   const GLuint nr = VB->Count;
   const GLboolean local = ctx->Light.Model.LocalViewer;
   const GLint thr = vxbits(1e-3F);
   GLfloat sumA = ctx->Light.Material.Attrib[MAT_ATTRIB_FRONT_DIFFUSE][3];
   GLuint j;

   VB->ColorPtr[0] = &store->LitColor[0];
   store->LitColor[0].stride = 16;
   store->LitColor[1].stride = 16;

   for (j = 0; j < nr; j++, STRIDE_F(vertex,vstride), STRIDE_F(normal,nstride)) {
      const GLfloat *base;
      GLfloat s0, s1, s2;
      struct gl_light *light;

      if (material) {
         update_materials( ctx, store );
         sumA = ctx->Light.Material.Attrib[MAT_ATTRIB_FRONT_DIFFUSE][3];
      }
      base = ctx->Light._BaseColor[0];
      s0 = base[0]; s1 = base[1]; s2 = base[2];

      foreach (light, &ctx->Light.EnabledList) {
         GLfloat VP[3], attenuation, n_dot_VP, n_dot_h, c0, c1, c2;
         const GLfloat *amb, *dif, *h;

         if (!(light->_Flags & LIGHT_POSITIONAL)) {
            COPY_3V(VP, light->_VP_inf_norm);
            attenuation = light->_VP_inf_spot_attenuation;
         }
         else {
            GLfloat d;
            SUB_3V(VP, light->_Position, vertex);
            if (_mesa_vx855_rsqrt) {
               GLfloat d2 = (GLfloat) LEN_SQUARED_3FV( VP );
               if (d2 > 1e-12F) {
                  GLfloat invd = vx_inv_sqrtf(d2);
                  d = d2 * invd;
                  SELF_SCALE_SCALAR_3V(VP, invd);
               }
               else
                  d = (GLfloat) SQRTF(d2);
            }
            else {
               d = (GLfloat) LEN_3FV( VP );
               if (d > 1e-6) {
                  GLfloat invd = 1.0F / d;
                  SELF_SCALE_SCALAR_3V(VP, invd);
               }
            }
            if (light->LinearAttenuation == 0.0F && light->QuadraticAttenuation == 0.0F)
               attenuation = light->ConstantAttenuation == 1.0F ? 1.0F : 1.0F / light->ConstantAttenuation;
            else
               attenuation = vx_recipf(light->ConstantAttenuation + d * (light->LinearAttenuation + d * light->QuadraticAttenuation));
            if (light->_Flags & LIGHT_SPOT) {
               GLfloat PV_dot_dir = - DOT3(VP, light->_NormDirection);
               if (PV_dot_dir < light->_CosCutoff)
                  continue;
               else {
                  GLdouble x = PV_dot_dir * (EXP_TABLE_SIZE-1);
                  GLint k = vx_itruncd(x);
                  GLfloat spot = (GLfloat) (light->_SpotExpTable[k][0] + (x-k)*light->_SpotExpTable[k][1]);
                  attenuation *= spot;
               }
            }
         }
         if (vxbits(attenuation) < thr)
            continue;

         amb = light->_MatAmbient[0];
         n_dot_VP = DOT3( normal, VP );
         if ((GLuint) vxbits(n_dot_VP) > 0x80000000U) {
            s0 += attenuation * amb[0]; s1 += attenuation * amb[1]; s2 += attenuation * amb[2];
            continue;
         }
         dif = light->_MatDiffuse[0];
         c0 = amb[0]; c1 = amb[1]; c2 = amb[2];
         c0 += n_dot_VP * dif[0]; c1 += n_dot_VP * dif[1]; c2 += n_dot_VP * dif[2];

         if (local) {
            GLfloat v[3];
            COPY_3V(v, vertex);
            NORMALIZE_3FV(v);
            SUB_3V(VP, VP, v);
            NORMALIZE_3FV(VP);
            h = VP;
         }
         else if (light->_Flags & LIGHT_POSITIONAL) {
            ACC_3V(VP, ctx->_EyeZDir);
            NORMALIZE_3FV(VP);
            h = VP;
         }
         else
            h = light->_h_inf_norm;
         n_dot_h = 1.0F * DOT3(normal, h);
         if (vxbits(n_dot_h) > 0) {
            GLfloat spec_coef;
            const GLfloat *spc = light->_MatSpecular[0];
            GET_SHINE_TAB_ENTRY( ctx->_ShineTable[0], n_dot_h, spec_coef );
            c0 += spec_coef * spc[0]; c1 += spec_coef * spc[1]; c2 += spec_coef * spc[2];
         }
         s0 += attenuation * c0; s1 += attenuation * c1; s2 += attenuation * c2;
      }
      Fcolor[j][0] = s0; Fcolor[j][1] = s1; Fcolor[j][2] = s2;
      Fcolor[j][3] = sumA;
   }
}

static void init_lighting_tables( void )
{
   static int done;

   if (!done) {
      init_light_tab();
      init_light_tab_twoside();
      init_light_tab_material();
      init_light_tab_twoside_material();
      done = 1;
   }
}


static GLboolean run_lighting( GLcontext *ctx, 
			       struct tnl_pipeline_stage *stage )
{
   struct light_stage_data *store = LIGHT_STAGE_DATA(stage);
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct vertex_buffer *VB = &tnl->vb;
   GLvector4f *input = ctx->_NeedEyeCoords ? VB->EyePtr : VB->ObjPtr;
   GLuint idx;

   if (!ctx->Light.Enabled || ctx->VertexProgram._Current)
      return GL_TRUE;

   /* Make sure we can talk about position x,y and z:
    */
   if (input->size <= 2 && input == VB->ObjPtr) {

      _math_trans_4f( store->Input.data,
		      VB->ObjPtr->data,
		      VB->ObjPtr->stride,
		      GL_FLOAT,
		      VB->ObjPtr->size,
		      0,
		      VB->Count );

      if (input->size <= 2) {
	 /* Clean z.
	  */
	 _mesa_vector4f_clean_elem(&store->Input, VB->Count, 2);
      }
	 
      if (input->size <= 1) {
	 /* Clean y.
	  */
	 _mesa_vector4f_clean_elem(&store->Input, VB->Count, 1);
      }

      input = &store->Input;
   }
   
   idx = 0;

   if (prepare_materials( ctx, VB, store ))
      idx |= LIGHT_MATERIAL;

   if (ctx->Light.Model.TwoSide)
      idx |= LIGHT_TWOSIDE;

   /* The individual functions know about replaying side-effects
    * vs. full re-execution. 
    */
   { /* VX855 v10.485, v10.486 */
      unsigned long *ls = _tnl_vx855_light_stat;
      if (store->light_func_tab == _tnl_light_tab && !(idx & LIGHT_TWOSIDE) && _mesa_vx855_lightfast) {
         light_vx855( ctx, VB, stage, input, (idx & LIGHT_MATERIAL) != 0 );
         ls[0]++; ls[6] += VB->Count;
      }
      else {
         store->light_func_tab[idx]( ctx, VB, stage, input );
         ls[store->light_func_tab == _tnl_light_tab ? 1 : store->light_func_tab == _tnl_light_spec_tab ? 2 : store->light_func_tab == _tnl_light_ci_tab ? 5 : 3]++;
         if (idx & LIGHT_TWOSIDE) ls[4]++;
         ls[7] += VB->Count;
      }
   }

   VB->AttribPtr[_TNL_ATTRIB_COLOR0] = VB->ColorPtr[0];
   VB->AttribPtr[_TNL_ATTRIB_COLOR1] = VB->SecondaryColorPtr[0];
   VB->AttribPtr[_TNL_ATTRIB_COLOR_INDEX] = VB->IndexPtr[0];

   return GL_TRUE;
}


/* Called in place of do_lighting when the light table may have changed.
 */
static void validate_lighting( GLcontext *ctx,
					struct tnl_pipeline_stage *stage )
{
   light_func *tab;

   if (!ctx->Light.Enabled || ctx->VertexProgram._Current)
      return;

   if (ctx->Visual.rgbMode) {
      if (ctx->Light._NeedVertices) {
	 if (ctx->Light.Model.ColorControl == GL_SEPARATE_SPECULAR_COLOR)
	    tab = _tnl_light_spec_tab;
	 else
	    tab = _tnl_light_tab;
      }
      else {
	 if (ctx->Light.EnabledList.next == ctx->Light.EnabledList.prev)
	    tab = _tnl_light_fast_single_tab;
	 else
	    tab = _tnl_light_fast_tab;
      }
   }
   else
      tab = _tnl_light_ci_tab;


   LIGHT_STAGE_DATA(stage)->light_func_tab = tab;

   /* This and the above should only be done on _NEW_LIGHT:
    */
   TNL_CONTEXT(ctx)->Driver.NotifyMaterialChange( ctx );
}



/* Called the first time stage->run is called.  In effect, don't
 * allocate data until the first time the stage is run.
 */
static GLboolean init_lighting( GLcontext *ctx,
				struct tnl_pipeline_stage *stage )
{
   TNLcontext *tnl = TNL_CONTEXT(ctx);
   struct light_stage_data *store;
   GLuint size = tnl->vb.Size;

   stage->privatePtr = MALLOC(sizeof(*store));
   store = LIGHT_STAGE_DATA(stage);
   if (!store)
      return GL_FALSE;

   /* Do onetime init.
    */
   init_lighting_tables();

   _mesa_vector4f_alloc( &store->Input, 0, size, 32 );
   _mesa_vector4f_alloc( &store->LitColor[0], 0, size, 32 );
   _mesa_vector4f_alloc( &store->LitColor[1], 0, size, 32 );
   _mesa_vector4f_alloc( &store->LitSecondary[0], 0, size, 32 );
   _mesa_vector4f_alloc( &store->LitSecondary[1], 0, size, 32 );
   _mesa_vector4f_alloc( &store->LitIndex[0], 0, size, 32 );
   _mesa_vector4f_alloc( &store->LitIndex[1], 0, size, 32 );

   store->LitColor[0].size = 4;
   store->LitColor[1].size = 4;
   store->LitSecondary[0].size = 3;
   store->LitSecondary[1].size = 3;

   store->LitIndex[0].size = 1;
   store->LitIndex[0].stride = sizeof(GLfloat);
   store->LitIndex[1].size = 1;
   store->LitIndex[1].stride = sizeof(GLfloat);

   return GL_TRUE;
}




static void dtr( struct tnl_pipeline_stage *stage )
{
   struct light_stage_data *store = LIGHT_STAGE_DATA(stage);

   if (store) {
      _mesa_vector4f_free( &store->Input );
      _mesa_vector4f_free( &store->LitColor[0] );
      _mesa_vector4f_free( &store->LitColor[1] );
      _mesa_vector4f_free( &store->LitSecondary[0] );
      _mesa_vector4f_free( &store->LitSecondary[1] );
      _mesa_vector4f_free( &store->LitIndex[0] );
      _mesa_vector4f_free( &store->LitIndex[1] );
      FREE( store );
      stage->privatePtr = NULL;
   }
}

const struct tnl_pipeline_stage _tnl_lighting_stage =
{
   "lighting",			/* name */
   NULL,			/* private_data */
   init_lighting,
   dtr,				/* destroy */
   validate_lighting,
   run_lighting
};
