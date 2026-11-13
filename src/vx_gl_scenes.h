/* VXGL's scenes (v10.239): the same GL calls for the ICD, Microsoft's software OpenGL (VXGL.EXE) and the host test (gl/host). W and H
 * are the window size; the speed scene's frame count is the caller's. */
#include <math.h>
/* ------------------------------------------------------------------ scenes (the same GL calls for both renderers) */
static void reset(void) {
 glViewport(0,0,W,H);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,0,H,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 glDisable(GL_DEPTH_TEST);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_ALPHA_TEST);
 glDisable(GL_STENCIL_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_POLYGON_OFFSET_FILL);glDisable(GL_LINE_STIPPLE);glDisable(GL_POLYGON_STIPPLE);glDisable(GL_TEXTURE_2D);
 glShadeModel(GL_SMOOTH);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);glFrontFace(GL_CCW);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glLineWidth(1);glPointSize(1);
 glColorMask(1,1,1,1);glClearColor(0.1f,0.2f,0.3f,1);glClearDepth(1);glClearStencil(0);glDisable(GL_COLOR_LOGIC_OP);
 /* VXGL v1.3: no dithering in the comparisons: the engine dithers at 16 bpp (as Microsoft's renderer does) and swrast does not, so the
  * blending scene differed by 1% in dither noise alone (board, v10.241). Programs keep OpenGL's default (on). */
 glDisable(GL_DITHER);
 glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
}
static void tri(float x0,float y0,float x1,float y1,float x2,float y2,unsigned c0,unsigned c1,unsigned c2) {
 glBegin(GL_TRIANGLES);
 glColor3ub((GLubyte)(c0>>16),(GLubyte)(c0>>8),(GLubyte)c0);glVertex2f(x0,y0);
 glColor3ub((GLubyte)(c1>>16),(GLubyte)(c1>>8),(GLubyte)c1);glVertex2f(x1,y1);
 glColor3ub((GLubyte)(c2>>16),(GLubyte)(c2>>8),(GLubyte)c2);glVertex2f(x2,y2);
 glEnd();
}
static void s_clear(void) {reset();}
static void s_smooth(void) {
 int i;reset();
 tri(10,10,120,20,40,120,0xFF0000,0x00FF00,0x0000FF);
 glBegin(GL_QUADS);glColor3f(1,1,0);glVertex2f(140,10);glColor3f(0,1,1);glVertex2f(246,10);glColor3f(1,0,1);glVertex2f(246,110);glColor3f(1,1,1);glVertex2f(140,110);glEnd();
 glBegin(GL_TRIANGLE_STRIP);for(i=0;i<=10;i++){glColor3f(i/10.f,0.5f,1-i/10.f);glVertex2f(10+i*23.f,140);glColor3f(0.2f,i/10.f,0.4f);glVertex2f(10+i*23.f,180);}glEnd();
 glBegin(GL_TRIANGLE_FAN);glColor3f(1,1,1);glVertex2f(128,220);for(i=0;i<=12;i++){float a=i*3.14159f/6;glColor3f(0.5f+0.5f*(float)cos(a),0.5f,0.5f+0.5f*(float)sin(a));glVertex2f(128+30*(float)cos(a),220+30*(float)sin(a));}glEnd();
}
static void s_depth(void) {
 reset();glEnable(GL_DEPTH_TEST);
 glBegin(GL_TRIANGLES);
 glColor3f(1,0,0);glVertex3f(20,20,0.5f);glVertex3f(230,40,0.5f);glVertex3f(60,230,0.5f);
 glColor3f(0,1,0);glVertex3f(230,20,-0.8f);glVertex3f(200,230,0.9f);glVertex3f(20,120,0.0f);
 glEnd();
 glDepthFunc(GL_GREATER);glColor3f(0,0,1);glBegin(GL_QUADS);glVertex3f(100,100,0.95f);glVertex3f(150,100,0.95f);glVertex3f(150,150,0.95f);glVertex3f(100,150,0.95f);glEnd();
}
static void s_blend(void) {
 reset();tri(10,10,240,10,128,240,0xFF0000,0x00FF00,0x0000FF);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 glBegin(GL_QUADS);glColor4f(1,1,1,0.5f);glVertex2f(40,40);glVertex2f(200,40);glColor4f(0,0,0,0.25f);glVertex2f(200,120);glVertex2f(40,120);glEnd();
 glBlendFunc(GL_ONE,GL_ONE);glBegin(GL_QUADS);glColor3f(0.3f,0.1f,0.1f);glVertex2f(60,140);glVertex2f(220,140);glVertex2f(220,220);glVertex2f(60,220);glEnd();
}
static void s_cull(void) {
 reset();glEnable(GL_CULL_FACE);glCullFace(GL_BACK);
 tri(10,10,110,10,60,110,0xFF0000,0xFF0000,0xFF0000);   /* CCW: front, kept */
 tri(140,10,190,110,240,10,0x00FF00,0x00FF00,0x00FF00); /* CW: back, culled */
 glFrontFace(GL_CW);
 tri(10,140,110,140,60,240,0x0000FF,0x0000FF,0x0000FF); /* CCW now back: culled */
 tri(140,140,190,240,240,140,0xFFFF00,0xFFFF00,0xFFFF00); /* CW now front: kept */
}
static void s_flat(void) {
 int i;reset();glShadeModel(GL_FLAT);
 glBegin(GL_TRIANGLE_STRIP);for(i=0;i<=8;i++){glColor3f(i/8.f,1-i/8.f,0.5f);glVertex2f(10+i*29.f,30);glColor3f(0.5f,i/8.f,1-i/8.f);glVertex2f(10+i*29.f,110);}glEnd();
 tri(20,140,230,150,120,240,0xFF0000,0x00FF00,0x0000FF);
}
static void sphere(int slices,int stacks) {
 int i,j;
 for(i=0;i<stacks;i++){float t0=3.14159f*i/stacks-1.5708f,t1=3.14159f*(i+1)/stacks-1.5708f;
  glBegin(GL_QUAD_STRIP);
  for(j=0;j<=slices;j++){float p=6.28318f*j/slices;float x0=(float)(cos(t0)*cos(p)),y0=(float)sin(t0),z0=(float)(cos(t0)*sin(p)),x1=(float)(cos(t1)*cos(p)),y1=(float)sin(t1),z1=(float)(cos(t1)*sin(p));
   glNormal3f(x1,y1,z1);glVertex3f(x1,y1,z1);glNormal3f(x0,y0,z0);glVertex3f(x0,y0,z0);}
  glEnd();}
}
static void persp(void) {glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-1,1,-1,1,2,10);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glTranslatef(0,0,-5);}
static void s_light(void) {
 float pos[4]={2,3,4,0},dif[4]={1,0.8f,0.6f,1},amb[4]={0.1f,0.1f,0.2f,1},spec[4]={1,1,1,1};
 reset();persp();glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);
 glLightfv(GL_LIGHT0,GL_POSITION,pos);glLightfv(GL_LIGHT0,GL_DIFFUSE,dif);glLightfv(GL_LIGHT0,GL_AMBIENT,amb);glLightfv(GL_LIGHT0,GL_SPECULAR,spec);
 glMaterialfv(GL_FRONT,GL_SPECULAR,spec);glMaterialf(GL_FRONT,GL_SHININESS,20);
 glRotatef(30,1,0.5f,0);sphere(24,16);
}
static void s_fog(void) {
 float fc[4]={0.6f,0.6f,0.7f,1};int i;
 reset();persp();glEnable(GL_DEPTH_TEST);glEnable(GL_FOG);glFogi(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_START,3);glFogf(GL_FOG_END,9);glFogfv(GL_FOG_COLOR,fc);
 for(i=0;i<5;i++){glPushMatrix();glTranslatef(-1.5f+i*0.75f,0,-i*1.0f);glColor3f(1,0.2f*i,0.1f);sphere(12,8);glPopMatrix();}
}
static void s_lines(void) {
 int i;reset();
 glBegin(GL_LINES);for(i=0;i<16;i++){glColor3f(1,i/16.f,0);glVertex2f(10.5f,10.5f+i*7);glColor3f(0,i/16.f,1);glVertex2f(120.5f,20.5f+i*5);}glEnd();
 glBegin(GL_LINE_LOOP);glColor3f(1,1,1);glVertex2f(140.5f,10.5f);glVertex2f(245.5f,10.5f);glVertex2f(245.5f,120.5f);glVertex2f(140.5f,120.5f);glEnd();
 glBegin(GL_POINTS);for(i=0;i<64;i++){glColor3f(i/64.f,1,1-i/64.f);glVertex2f(10.5f+(i%16)*14,140.5f+(i/16)*20);}glEnd();
}
static void s_scissor(void) {
 reset();glEnable(GL_SCISSOR_TEST);glScissor(40,60,150,100);
 glClearColor(0.8f,0.1f,0.1f,1);glClear(GL_COLOR_BUFFER_BIT);
 tri(0,0,256,0,128,256,0x00FF00,0x0000FF,0xFFFFFF);
}
static void s_unfilled(void) {
 reset();glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);tri(20,20,230,40,100,230,0xFFFFFF,0xFF0000,0x00FF00);
 glPolygonMode(GL_FRONT,GL_FILL);glPolygonMode(GL_BACK,GL_POINT);tri(40,60,80,120,120,70,0xFFFF00,0xFFFF00,0xFFFF00);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
}
static void s_offset(void) {
 reset();glEnable(GL_DEPTH_TEST);glColor3f(0,0,1);
 glBegin(GL_QUADS);glVertex3f(60,60,0);glVertex3f(190,60,0);glVertex3f(190,190,0);glVertex3f(60,190,0);glEnd();
 glEnable(GL_POLYGON_OFFSET_FILL);glPolygonOffset(-1,-1);glColor3f(1,1,0);
 glBegin(GL_TRIANGLES);glVertex3f(80,80,0);glVertex3f(170,80,0);glVertex3f(125,170,0);glEnd();
 glPolygonOffset(0,-2);glColor3f(0,1,0);
 glBegin(GL_TRIANGLES);glVertex3f(20,200,0.5f);glVertex3f(120,210,-0.5f);glVertex3f(60,250,0.2f);glEnd();
 glDisable(GL_POLYGON_OFFSET_FILL);glColor3f(1,0,1);
 glBegin(GL_TRIANGLES);glVertex3f(20,200,0.5f);glVertex3f(120,210,-0.5f);glVertex3f(60,250,0.2f);glEnd();
}
/* VXGL v1.3: where the stencil scene goes wrong on the board (v10.241: the engine drew nothing of its quad where swrast had written stencil 1
 * with the colour mask off, depth test off). These split it: who writes the stencil, depth test off or on; and depth written by one side,
 * tested by the other (textured surfaces are drawn by swrast until the engine textures, so games mix the two every frame). */
/* (glOrtho(..., -1, 1): z 0.5 is nearer than z 0, window depth 0.25 against 0.5) */
static void s_depth_sw(void) { /* swrast writes depth (colour mask off), the engine tests against it: the triangle's area stays clear */
 reset();glEnable(GL_DEPTH_TEST);
 glColorMask(0,0,0,0);glBegin(GL_TRIANGLES);glVertex3f(40,40,0.5f);glVertex3f(220,60,0.5f);glVertex3f(128,220,0.5f);glEnd();glColorMask(1,1,1,1);
 glBegin(GL_QUADS);glColor3f(1,0.5f,0);glVertex3f(0,0,0);glVertex3f(256,0,0);glColor3f(0,0.5f,1);glVertex3f(256,256,0);glVertex3f(0,256,0);glEnd();
}
/* v10.295: logic ops are drawn by the engine now: swrast was chosen with a polygon stipple of all ones. v10.308: the engine stipples
 * polygons now: swrast is chosen with an ARB fragment program that passes the colour through (fragment programs stay in software);
 * Microsoft's 1.1 has none (everything is software there) */
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F /* v10.317: GL 1.2 (Microsoft's GL/gl.h is 1.1) */
#endif
#ifndef GL_FRAGMENT_PROGRAM_ARB
#define GL_FRAGMENT_PROGRAM_ARB 0x8804
#define GL_PROGRAM_FORMAT_ASCII_ARB 0x8875
#endif
typedef void (APIENTRY *gen_prog_fn)(GLsizei,GLuint*);typedef void (APIENTRY *bind_prog_fn)(GLenum,GLuint);typedef void (APIENTRY *prog_str_fn)(GLenum,GLenum,GLsizei,const GLvoid*);
static gen_prog_fn scene_gen_prog;static bind_prog_fn scene_bind_prog;static prog_str_fn scene_prog_str;static GLuint scene_fp;
/* v4.4: GL_ATI_fragment_shader's entry points (s_atishader) */
typedef GLuint (APIENTRY *ati_gen_fn)(GLuint);typedef void (APIENTRY *ati_bind_fn)(GLuint);typedef void (APIENTRY *ati_be_fn)(void);typedef void (APIENTRY *ati_smap_fn)(GLuint,GLuint,GLenum);
typedef void (APIENTRY *ati_cop2_fn)(GLenum,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint);typedef void (APIENTRY *ati_cop3_fn)(GLenum,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint);
typedef void (APIENTRY *ati_aop2_fn)(GLenum,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint,GLuint);typedef void (APIENTRY *ati_const_fn)(GLuint,const GLfloat*);
static ati_gen_fn scene_ati_gen;static ati_bind_fn scene_ati_bind;static ati_be_fn scene_ati_begin,scene_ati_end;static ati_smap_fn scene_ati_smap;static ati_cop2_fn scene_ati_cop2;static ati_cop3_fn scene_ati_cop3;static ati_aop2_fn scene_ati_aop2;static ati_const_fn scene_ati_const;
/* v4.4: GLSL's entry points (s_glsl) */
typedef GLuint (APIENTRY *glsl_create_fn)(GLenum);typedef GLuint (APIENTRY *glsl_prog_fn)(void);typedef void (APIENTRY *glsl_src_fn)(GLuint,GLsizei,const char**,const GLint*);typedef void (APIENTRY *glsl_one_fn)(GLuint);
typedef void (APIENTRY *glsl_attach_fn)(GLuint,GLuint);typedef GLint (APIENTRY *glsl_loc_fn)(GLuint,const char*);typedef void (APIENTRY *glsl_u1i_fn)(GLint,GLint);typedef void (APIENTRY *glsl_u4f_fn)(GLint,GLfloat,GLfloat,GLfloat,GLfloat);
static glsl_create_fn scene_glsl_create;static glsl_prog_fn scene_glsl_prog;static glsl_src_fn scene_glsl_src;static glsl_one_fn scene_glsl_compile,scene_glsl_link,scene_glsl_use;static glsl_attach_fn scene_glsl_attach;static glsl_loc_fn scene_glsl_loc;static glsl_u1i_fn scene_glsl_u1i;static glsl_u4f_fn scene_glsl_u4f;
static void sw_on(void) {
 /* v10.321: fragment programs that a stage cascade can draw go to the engine; KIL (never true here: colours are not negative) keeps this one in software */
 static const char fp[]="!!ARBfp1.0\nKIL fragment.color;\nMOV result.color, fragment.color;\nEND\n";
 if(!scene_gen_prog||!scene_bind_prog||!scene_prog_str)return;
 if(!scene_fp){scene_gen_prog(1,&scene_fp);scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,scene_fp);scene_prog_str(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)(sizeof(fp)-1),fp);}
 scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,scene_fp);glEnable(GL_FRAGMENT_PROGRAM_ARB);}
static void sw_off(void) {if(scene_gen_prog)glDisable(GL_FRAGMENT_PROGRAM_ARB);}
static void s_depth_hw(void) { /* the engine writes depth, swrast (a stipple of all ones) tests against it: the grey triangle stays */
 reset();glEnable(GL_DEPTH_TEST);
 glBegin(GL_TRIANGLES);glColor3f(0.75f,0.75f,0.75f);glVertex3f(40,40,0.5f);glColor3f(0.25f,0.25f,0.25f);glVertex3f(220,60,0.5f);glColor3f(0.5f,0.5f,0.5f);glVertex3f(128,220,0.5f);glEnd();
 sw_on();
 glBegin(GL_QUADS);glColor3f(1,0.5f,0);glVertex3f(0,0,0);glVertex3f(256,0,0);glColor3f(0,0.5f,1);glVertex3f(256,256,0);glVertex3f(0,256,0);glEnd();
 sw_off();
}
/* multipass at equal depth (Quake 3 draws a surface's later passes with GL_EQUAL): a pass by the engine, then the same triangle by swrast
 * with GL_EQUAL, and the other way round. Every pixel of the second pass shows only if both sides turn a vertex's z into the same
 * stored value. */
static void eq_tri(float dx,float z) {glBegin(GL_TRIANGLES);glVertex3f(20+dx,30,z);glVertex3f(110+dx,50,z*0.5f);glVertex3f(60+dx,220,-z);glEnd();}
static void s_depth_equal(void) {
 reset();glEnable(GL_DEPTH_TEST);
 glColor3f(0.3f,0.3f,0.3f);eq_tri(0,0.6f);                                  /* engine */
 glDepthFunc(GL_EQUAL);sw_on();glColor3f(1,0,0);eq_tri(0,0.6f);sw_off(); /* swrast */
 glDepthFunc(GL_LESS);sw_on();glColor3f(0.3f,0.3f,0.3f);eq_tri(120,0.6f);sw_off(); /* swrast */
 glDepthFunc(GL_EQUAL);glColor3f(0,1,0);eq_tri(120,0.6f);                   /* engine */
 glDepthFunc(GL_LESS);
}
/* VXGL v1.6: the stencil runs as the reviews of v1.4 asked (board: the scene failed until the engine had written stencil once, and every
 * scene used the same triangle and value). Two triangles apart, two values: software writes 3 in the left one, the engine 1 in the right
 * one. After each software write the stencil is read back (glReadPixels GL_STENCIL_INDEX, the CPU's view) and counted (st_count[]).
 * Depth test off throughout: with Z off and stencil on the engine gets the same registers as Z on, ALWAYS, no Z writes. */
static void tri_l(void) {glBegin(GL_TRIANGLES);glVertex2f(20,30);glVertex2f(110,40);glVertex2f(60,200);glEnd();}
static void tri_r(void) {glBegin(GL_TRIANGLES);glVertex2f(140,40);glVertex2f(240,60);glVertex2f(190,220);glEnd();}
static void quad_full(float r,float g,float b) {glColor3f(r,g,b);glBegin(GL_QUADS);glVertex2f(0,0);glVertex2f(256,0);glVertex2f(256,256);glVertex2f(0,256);glEnd();}
static long st_count[8][4]; /* per stencil scene: pixels with stencil 0, 1, 3, other, read back after the software write */
static long st_zchg[8]; /* v1.7: per stencil scene, after its last draw: pixels whose depth is no longer the cleared 1.0 (no stencil draw writes depth) */
static void st_depth(int slot) {
 static GLuint zb[W*H];int i;GLint bits=0;long n=0;
 glGetIntegerv(GL_DEPTH_BITS,&bits);if(bits<1){st_zchg[slot]=-1;return;}
 glPixelStorei(GL_PACK_ALIGNMENT,4);glReadPixels(0,0,W,H,GL_DEPTH_COMPONENT,GL_UNSIGNED_INT,zb);
 for(i=0;i<W*H;i++)if((zb[i]>>(bits>16?8:16))!=(bits>16?0xFFFFFFUL:0xFFFFUL))n++;
 st_zchg[slot]=n;
}
static void st_read(int slot) {
 static unsigned char sb[W*H];int i;GLint bits=0;
 glGetIntegerv(GL_STENCIL_BITS,&bits);if(bits<1){for(i=0;i<4;i++)st_count[slot][i]=-1;return;} /* no stencil buffer (the host's 16-bit run) */
 glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,sb);
 for(i=0;i<4;i++)st_count[slot][i]=0;
 for(i=0;i<W*H;i++)st_count[slot][sb[i]==0?0:sb[i]==1?1:sb[i]==3?2:3]++;
}
static void st_soft(int left,int slot) { /* software writes 3 (colour mask off: swrast) */
 glEnable(GL_STENCIL_TEST);glStencilFunc(GL_ALWAYS,3,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
 glColorMask(0,0,0,0);if(left)tri_l();else tri_r();glColorMask(1,1,1,1);
 st_read(slot);
}
static void st_test(int ref,float r,float g,float b) {glStencilFunc(GL_EQUAL,ref,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);quad_full(r,g,b);}
static void s_stA(void) {reset();st_soft(1,0);st_test(3,1,0.5f,0);st_depth(0);}                              /* first stencil of the run */
static void s_stB(void) {reset();glEnable(GL_STENCIL_TEST);glStencilFunc(GL_ALWAYS,1,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
 glColor3f(0.25f,0.25f,0.25f);tri_r();st_test(1,0,0.5f,1);st_depth(1);}                                      /* the engine writes 1, right */
static void s_stC(void) {reset();st_soft(1,2);st_test(3,1,0.5f,0);st_depth(2);}                              /* software 3 left, after B */
static void s_stD(void) {reset();st_soft(1,3);st_test(1,0,1,0);st_depth(3);}                                 /* the same, EQUAL 1: B's triangle if stale */
static void s_stE(void) {reset();st_soft(0,4);st_test(3,1,0.5f,0);st_depth(4);}                              /* software 3 where B wrote 1 */
static void s_alphatest(void) {
 int i;reset();glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.5f);
 glBegin(GL_QUADS);for(i=0;i<8;i++){glColor4f(1,1-i/8.f,0,i/7.f);glVertex2f(10+i*30.f,20);glVertex2f(35+i*30.f,20);glVertex2f(35+i*30.f,230);glVertex2f(10+i*30.f,230);}glEnd();
}
static void s_stipple(void) { /* stippled and wide lines (swrast until v10.299) */
 int i;reset();glEnable(GL_LINE_STIPPLE);glLineStipple(2,0xF0F0);
 glBegin(GL_LINES);for(i=0;i<10;i++){glColor3f(1,1,i/10.f);glVertex2f(10.5f,10.5f+i*12);glVertex2f(245.5f,30.5f+i*12);}glEnd();
 glDisable(GL_LINE_STIPPLE);glLineWidth(3);glBegin(GL_LINES);glColor3f(0,1,0);glVertex2f(20,150);glVertex2f(230,240);glEnd();glLineWidth(1);
 tri(30,160,120,170,60,240,0xFF00FF,0x00FFFF,0xFFFF00);
}
static void s_pixels(void) {
 static unsigned char px[64*64*4];int i;reset();
 for(i=0;i<64*64;i++){px[i*4]=(unsigned char)(i*3);px[i*4+1]=(unsigned char)(i>>4);px[i*4+2]=(unsigned char)(255-(i&255));px[i*4+3]=255;}
 glRasterPos2i(20,20);glDrawPixels(64,64,GL_RGBA,GL_UNSIGNED_BYTE,px);
 tri(100,20,240,30,160,200,0xFF0000,0x00FF00,0x0000FF);
 glRasterPos2i(20,120);glCopyPixels(100,20,100,100,GL_COLOR);
}
/* v10.295: drawn by the engine from v10.295: the sixteen logic ops over a gradient, the colour masks, wide lines and large points, and
 * textured points and wide lines (each against the ICD without the engine) */
/* a background of flat bands (a logic op turns the one-step differences of interpolated colours into many) */
#define E5(v) ((GLubyte)(((v)<<3)|((v)>>2))) /* colours a 565 buffer holds exactly (rounded or truncated alike) */
#define E6(v) ((GLubyte)(((v)<<2)|((v)>>4)))
static void grad(void) {int k;glBegin(GL_QUADS);for(k=0;k<8;k++){float y=(float)(k*H/8);glColor3ub(E5(k*4),E6(62-k*7),E5(11+k*2));
 glVertex2f(0,y);glVertex2f((float)W,y);glVertex2f((float)W,y+(float)(H/8));glVertex2f(0,y+(float)(H/8));}glEnd();}
static void s_logicop(void) {
 int k;reset();grad();glEnable(GL_COLOR_LOGIC_OP);
 for(k=0;k<16;k++){float x=(float)(8+(k%4)*60),y=(float)(8+(k/4)*60);glLogicOp(GL_CLEAR+k);
  glBegin(GL_QUADS);glColor3ub(E5(30),E6(22),E5(7));glVertex2f(x,y);glVertex2f(x+40,y);glVertex2f(x+40,y+20);glVertex2f(x,y+20);
  glColor3ub(E5(1),E6(41),E5(24));glVertex2f(x,y+20);glVertex2f(x+40,y+20);glVertex2f(x+40,y+40);glVertex2f(x,y+40);glEnd();}
 glDisable(GL_COLOR_LOGIC_OP);
}
static void s_colormask(void) {
 int k;reset();grad();
 for(k=0;k<7;k++){float x=(float)(8+(k%4)*60),y=(float)(8+(k/4)*80);glColorMask((k+1)&1,((k+1)>>1)&1,((k+1)>>2)&1,1);
  glBegin(GL_TRIANGLES);glColor3f(1,1,1);glVertex2f(x,y);glColor3f(0.5f,0.8f,0.2f);glVertex2f(x+48,y+8);glColor3f(0.2f,0.4f,0.9f);glVertex2f(x+20,y+60);glEnd();}
 glColorMask(1,1,1,1);
}
static void s_wide(void) {
 int i;reset();
 for(i=0;i<6;i++){glLineWidth((float)(i+2));glBegin(GL_LINES);glColor3f(1,i/6.f,0);glVertex2f(10.5f,10.5f+i*18);glColor3f(0,1,i/6.f);glVertex2f(240.5f,30.5f+i*18);
  glVertex2f(20.5f+i*30,130.5f);glVertex2f(30.5f+i*30,240.5f);glEnd();}
 glLineWidth(1);
 for(i=0;i<8;i++){glPointSize((float)(i+2));glBegin(GL_POINTS);glColor3f(i/8.f,1,1-i/8.f);glVertex2f(30.5f+i*26,120.25f);glVertex2f(31+i*26,105);glEnd();}
 glPointSize(1);
}
static void s_texwide(void) {
 static GLubyte tx[8*8*4];GLuint t=0;int i;reset();
 for(i=0;i<64;i++){tx[i*4]=(GLubyte)((i&7)*32);tx[i*4+1]=(GLubyte)((i>>3)*32);tx[i*4+2]=(GLubyte)(((i^(i>>3))&1)*255);tx[i*4+3]=255;}
 glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,tx);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 for(i=0;i<4;i++){glLineWidth((float)(i*2+3));glBegin(GL_LINES);glTexCoord2f(0.06f,0.06f+i*0.25f);glVertex2f(10.5f,20.5f+i*40);glTexCoord2f(0.94f,0.06f+i*0.25f);glVertex2f(240.5f,40.5f+i*40);glEnd();}
 glLineWidth(1);
 for(i=0;i<16;i++){glPointSize((float)(1+i%6));glBegin(GL_POINTS);glTexCoord2f((i&3)*0.25f+0.1f,(i>>2)*0.25f+0.1f);glVertex2f(20.5f+i*14,200.5f);glEnd();}
 glPointSize(1);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&t);
}
static void s_speed(void) {
 float pos[4]={2,3,4,0};
 glViewport(0,0,W,H);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 persp();glEnable(GL_DEPTH_TEST);glEnable(GL_LIGHTING);glEnable(GL_LIGHT0);glLightfv(GL_LIGHT0,GL_POSITION,pos);
 glRotatef(20,0,1,0);sphere(64,48); /* 6144 quads: 12288 triangles */
 glDisable(GL_LIGHTING);
}
typedef void (*scene_fn)(void);
/* info 1: shown, not judged (the multipass scene: whether the engine and swrast store the same depth for the same vertex z is measured,
 * not required: each interpolates and rounds its own way, and once textures are on the engine the two rarely meet in one surface) */
/* v10.300: GL_BLEND with a black and a white environment colour, for each base format (the engine's MODULATE by the complement and
 * ADDSMOOTH); a grey one stays in software and is drawn too */
#ifndef GL_POINT_SPRITE_ARB
#define GL_POINT_SPRITE_ARB 0x8861
#define GL_COORD_REPLACE_ARB 0x8862
#endif
#ifndef GL_POINT_SIZE_MIN_ARB
#define GL_POINT_SIZE_MIN_ARB 0x8126
#define GL_POINT_SIZE_MAX_ARB 0x8127
#define GL_POINT_DISTANCE_ATTENUATION_ARB 0x8129
#endif
typedef void (APIENTRY *point_param_fn)(GLenum,const GLfloat*);
static point_param_fn scene_point_param; /* glPointParameterfvARB, set by the program (none: the attenuated part is left out) */
static GLuint scene_tex(int fmt) { /* a 16x16 texture of steps in each channel, in a format (0 RGBA, 1 LUMINANCE, 2 INTENSITY, 3 ALPHA, 4 LUMINANCE_ALPHA) */
 static GLubyte t[16*16*4];GLuint id=0;int i;static const GLenum f[5]={GL_RGBA,GL_LUMINANCE,GL_INTENSITY,GL_ALPHA,GL_LUMINANCE_ALPHA},src[5]={GL_RGBA,GL_LUMINANCE,GL_LUMINANCE,GL_ALPHA,GL_LUMINANCE_ALPHA};
 for(i=0;i<256;i++){t[i*4]=(GLubyte)((i&15)*17);t[i*4+1]=(GLubyte)((i>>4)*17);t[i*4+2]=(GLubyte)(255-(i&15)*17);t[i*4+3]=(GLubyte)(((i>>4)&15)*17);}
 if(fmt==4)for(i=0;i<256;i++){t[i*2]=(GLubyte)((i&15)*17);t[i*2+1]=(GLubyte)((i>>4)*17);}
 else if(fmt)for(i=0;i<256;i++)t[i]=(GLubyte)((i&15)*17^((i>>4)*8));
 glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,f[fmt],16,16,0,src[fmt],GL_UNSIGNED_BYTE,t);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
 return id;
}
static void s_blendenv(void) {
 static const GLfloat ec[3][4]={{0,0,0,0},{1,1,1,1},{0.5f,0.25f,0.75f,0.5f}};int f,k;GLuint id;
 reset();grad();glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_BLEND);
 for(f=0;f<5;f++){id=scene_tex(f);
  for(k=0;k<3;k++){float x=(float)(8+k*80),y=(float)(8+f*48);glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_COLOR,ec[k]);
   glBegin(GL_QUADS);glColor4ub(E5(28),E6(40),E5(6),200);glTexCoord2f(0,0);glVertex2f(x,y);glTexCoord2f(1,0);glVertex2f(x+64,y);glTexCoord2f(1,1);glVertex2f(x+64,y+40);glTexCoord2f(0,1);glVertex2f(x,y+40);glEnd();}
  glDeleteTextures(1,&id);}
 glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glDisable(GL_TEXTURE_2D);
}
/* v10.300: q in the vertices (glTexCoord4f), no texgen or texture matrix: drawn projective by the engine */
static void s_qvertex(void) {
 GLuint id;int i;reset();id=scene_tex(0);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 for(i=0;i<3;i++){float x=(float)(10+i*82),q=1.0f+i*0.75f;glBegin(GL_QUADS);glTexCoord4f(0,0,0,1);glVertex2f(x,20);glTexCoord4f(q,0,0,q);glVertex2f(x+70,20);
  glTexCoord4f(q,q,0,q);glVertex2f(x+70,230);glTexCoord4f(0,1,0,1);glVertex2f(x,230);glEnd();}
 glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&id);
}
/* v10.300: one-pixel textured lines in every direction, fractional ends, and a textured line strip */
static void s_texlines(void) {
 GLuint id;int i;reset();id=scene_tex(0);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glColor3f(1,1,1);
 /* v10.301 (review): wide textured lines whose ends are 1 and 1000 deep (rhw 1000 times apart), both ways: the far end's outer half pixel
  * extrapolated rhw below 0 and the whole line was left out */
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-1,1,-1,1,1,2000);glMatrixMode(GL_MODELVIEW);glLoadIdentity();glLineWidth(7);
 glBegin(GL_LINES);glTexCoord2f(0,0);glVertex3f(-0.9f,0.6f,-1.01f);glTexCoord2f(1,1);glVertex3f(400,560,-1000);
 glTexCoord2f(1,1);glVertex3f(400,-480,-1000);glTexCoord2f(0,0);glVertex3f(-0.9f,-0.5f,-1.01f);glEnd();glLineWidth(1);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,0,H,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 glBegin(GL_LINES);for(i=0;i<24;i++){float a=(float)i*0.2618f,cs=(float)cos(a),sn=(float)sin(a);
  glTexCoord2f(0.03f,(float)i/24);glVertex2f(128.3f+cs*12,128.6f+sn*12);glTexCoord2f(0.97f,(float)i/24+0.02f);glVertex2f(128.3f+cs*120,128.6f+sn*118);}glEnd();
 glBegin(GL_LINE_STRIP);for(i=0;i<9;i++){glTexCoord2f((float)i/8,0.5f);glVertex2f(10.25f+i*29.7f,i&1?20.75f:6.1f);}glEnd();
 glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&id);
}
/* v10.300: line stipple on the engine: factors 1 and 3, lines (the pattern restarts each), a strip and a loop (it runs on), wide */
static void s_linestip(void) {
 int i;reset();glEnable(GL_LINE_STIPPLE);
 glLineStipple(1,0x3F07);glBegin(GL_LINES);for(i=0;i<8;i++){glColor3f(1,i/8.f,0);glVertex2f(10.5f,10.5f+i*10);glVertex2f(245.5f,20.5f+i*12);}glEnd();
 glLineStipple(3,0xA5C3);glBegin(GL_LINE_STRIP);for(i=0;i<10;i++){glColor3f(0,1,i/10.f);glVertex2f(10.5f+i*26,i&1?120.5f:150.25f);}glEnd();
 glLineStipple(2,0x00FF);glBegin(GL_LINE_LOOP);glColor3f(1,1,1);glVertex2f(30.5f,170.5f);glVertex2f(120.3f,175.6f);glVertex2f(100.5f,245.5f);glVertex2f(25.5f,230.7f);glEnd();
 glLineWidth(4);glLineStipple(1,0xF0F0);glBegin(GL_LINES);glColor3f(1,0,1);glVertex2f(140.5f,170.5f);glVertex2f(245.5f,240.5f);glVertex2f(150.5f,240.5f);glVertex2f(250.5f,180.5f);glEnd();
 glLineWidth(1);glDisable(GL_LINE_STIPPLE);
}
/* v10.300: point sprites (GL_COORD_REPLACE, the texture across each point) and distance-attenuated points */
static void s_sprites(void) {
 GLuint id;int i;reset();id=scene_tex(0);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 glEnable(GL_POINT_SPRITE_ARB);glTexEnvi(GL_POINT_SPRITE_ARB,GL_COORD_REPLACE_ARB,GL_TRUE);
 for(i=0;i<8;i++){glPointSize((float)(4+i*3));glBegin(GL_POINTS);glVertex2f(16.5f+i*30,30.5f);glVertex2f(17+i*30,70);glEnd();}
 glTexEnvi(GL_POINT_SPRITE_ARB,GL_COORD_REPLACE_ARB,GL_FALSE);glDisable(GL_POINT_SPRITE_ARB);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&id);
 if(scene_point_param){static const GLfloat att[3]={0,0.5f,0.05f},mx[1]={24},mn[1]={1};
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-1,1,-1,1,1,20);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  scene_point_param(GL_POINT_DISTANCE_ATTENUATION_ARB,att);scene_point_param(GL_POINT_SIZE_MAX_ARB,mx);scene_point_param(GL_POINT_SIZE_MIN_ARB,mn);glPointSize(20);
  glBegin(GL_POINTS);for(i=0;i<12;i++){glColor3f(1,i/12.f,0.3f);glVertex3f(-0.8f+i*0.13f,-0.2f-i*0.02f,-1.5f-i*0.9f);}glEnd();
  {static const GLfloat one[3]={1,0,0};scene_point_param(GL_POINT_DISTANCE_ATTENUATION_ARB,one);}glPointSize(1);
  glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,0,H,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();}
}
/* v10.304 (VXGL v3.4): what v10.304 put on the engine. The extension entry points are the program's (none: that part is left out). */
#ifndef GL_FUNC_ADD
#define GL_FUNC_ADD 0x8006
#define GL_MIN 0x8007
#define GL_MAX 0x8008
#define GL_FUNC_SUBTRACT 0x800A
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#define GL_CONSTANT_COLOR 0x8001
#define GL_ONE_MINUS_CONSTANT_COLOR 0x8002
#define GL_CONSTANT_ALPHA 0x8003
#define GL_ONE_MINUS_CONSTANT_ALPHA 0x8004
#endif
#ifndef GL_COMBINE_ARB
#define GL_COMBINE_ARB 0x8570
#define GL_COMBINE_RGB_ARB 0x8571
#define GL_COMBINE_ALPHA_ARB 0x8572
#define GL_SOURCE0_RGB_ARB 0x8580
#define GL_SOURCE1_RGB_ARB 0x8581
#define GL_SOURCE2_RGB_ARB 0x8582
#define GL_OPERAND0_RGB_ARB 0x8590
#define GL_OPERAND1_RGB_ARB 0x8591
#define GL_OPERAND2_RGB_ARB 0x8592
#define GL_RGB_SCALE_ARB 0x8573
#define GL_ADD_SIGNED_ARB 0x8574
#define GL_INTERPOLATE_ARB 0x8575
#define GL_SUBTRACT_ARB 0x84E7
#define GL_CONSTANT_ARB 0x8576
#define GL_PRIMARY_COLOR_ARB 0x8577
#define GL_PREVIOUS_ARB 0x8578
#endif
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif
#ifndef GL_SAMPLES_PASSED_ARB
#define GL_SAMPLES_PASSED_ARB 0x8914
#define GL_QUERY_RESULT_ARB 0x8866
#endif
#ifndef GL_TEXTURE_MIN_LOD
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#endif
#ifndef GL_TEXTURE_MAX_LEVEL
#define GL_TEXTURE_MAX_LEVEL 0x813D
#endif
#ifndef GL_TEXTURE0_ARB
#define GL_TEXTURE0_ARB 0x84C0
#define GL_TEXTURE1_ARB 0x84C1
#endif
#ifndef GL_TEXTURE2_ARB
#define GL_TEXTURE2_ARB 0x84C2
#endif
#ifndef GL_COMPRESSED_RGB_S3TC_DXT1_EXT
#define GL_COMPRESSED_RGB_S3TC_DXT1_EXT 0x83F0
#define GL_COMPRESSED_RGBA_S3TC_DXT1_EXT 0x83F1
#define GL_COMPRESSED_RGBA_S3TC_DXT3_EXT 0x83F2
#define GL_COMPRESSED_RGBA_S3TC_DXT5_EXT 0x83F3
#endif
typedef void (APIENTRY *blend_eq_fn)(GLenum);
typedef void (APIENTRY *blend_col_fn)(GLclampf,GLclampf,GLclampf,GLclampf);
typedef void (APIENTRY *active_tex_fn)(GLenum);
typedef void (APIENTRY *mtc2_fn)(GLenum,GLfloat,GLfloat);typedef void (APIENTRY *mtc3_fn)(GLenum,GLfloat,GLfloat,GLfloat);static mtc3_fn scene_mtc3; /* v4.4 */
typedef void (APIENTRY *ctex_fn)(GLenum,GLint,GLenum,GLsizei,GLsizei,GLint,GLsizei,const GLvoid*); /* v10.307: glCompressedTexImage2DARB */
static ctex_fn scene_ctex;
static blend_eq_fn scene_blend_eq;static blend_col_fn scene_blend_col;static active_tex_fn scene_active_tex;static mtc2_fn scene_mtc2;static int scene_combine;
static void quad_tc(float x,float y,float w,float h,float s0,float t0,float s1,float t1) {
 glBegin(GL_QUADS);glTexCoord2f(s0,t0);glVertex2f(x,y);glTexCoord2f(s1,t0);glVertex2f(x+w,y);glTexCoord2f(s1,t1);glVertex2f(x+w,y+h);glTexCoord2f(s0,t1);glVertex2f(x,y+h);glEnd();
}
/* blend equations over a gradient (SUBTRACT, REVERSE_SUBTRACT, MIN, MAX), and the constant colour's four factors */
static void s_blendop(void) {
 static const GLenum eq[4]={GL_FUNC_SUBTRACT,GL_FUNC_REVERSE_SUBTRACT,GL_MIN,GL_MAX};
 static const GLenum fs[4]={GL_CONSTANT_COLOR,GL_ONE_MINUS_CONSTANT_COLOR,GL_CONSTANT_ALPHA,GL_ONE_MINUS_CONSTANT_ALPHA},fd[4]={GL_ONE_MINUS_CONSTANT_ALPHA,GL_CONSTANT_ALPHA,GL_ONE_MINUS_CONSTANT_COLOR,GL_CONSTANT_COLOR};int k;
 reset();grad();glEnable(GL_BLEND);
 if(scene_blend_eq)for(k=0;k<4;k++){float x=(float)(8+k*62);scene_blend_eq(eq[k]);glBlendFunc(GL_ONE,k&1?GL_SRC_ALPHA:GL_ONE);
  glBegin(GL_QUADS);glColor4f(0.25f+k*0.15f,0.6f-k*0.1f,0.3f,0.5f);glVertex2f(x,10);glColor4f(0.9f,0.2f,0.5f,0.75f);glVertex2f(x+54,10);glVertex2f(x+54,120);glColor4f(0.1f,0.8f,0.4f,0.25f);glVertex2f(x,120);glEnd();}
 if(scene_blend_eq)scene_blend_eq(GL_FUNC_ADD);
 if(scene_blend_col){scene_blend_col(0.25f,0.5f,0.75f,0.6f);
  for(k=0;k<4;k++){float x=(float)(8+k*62);glBlendFunc(fs[k],fd[k]);
   glBegin(GL_QUADS);glColor4f(1,0.5f,0.1f,0.7f);glVertex2f(x,136);glColor4f(0.2f,0.9f,1,0.3f);glVertex2f(x+54,136);glVertex2f(x+54,246);glColor4f(0.6f,0.2f,0.8f,1);glVertex2f(x,246);glEnd();}}
 glBlendFunc(GL_ONE,GL_ZERO);glDisable(GL_BLEND);
}
/* GL_COMBINE scaled (REPLACE x2, ADD x2, SUBTRACT x4, ADD_SIGNED x4) and INTERPOLATE by a colour (the constant, the texture) */
static void s_combscale(void) {
 GLuint id;int k;static const GLfloat cc[4]={0.3f,0.7f,0.45f,1};
 reset();grad();if(!scene_combine)return;
 id=scene_tex(0);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE_ARB);glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_COLOR,cc);
 glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA_ARB,GL_REPLACE);
 for(k=0;k<6;k++){static const GLenum mode[6]={GL_REPLACE,GL_ADD,GL_SUBTRACT_ARB,GL_ADD_SIGNED_ARB,GL_INTERPOLATE_ARB,GL_INTERPOLATE_ARB};static const GLfloat sc[6]={2,2,4,4,1,2};
  float x=(float)(8+(k%3)*82),y=(float)(8+(k/3)*124);
  glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB_ARB,mode[k]);glTexEnvf(GL_TEXTURE_ENV,GL_RGB_SCALE_ARB,sc[k]);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB_ARB,k==5?GL_PRIMARY_COLOR_ARB:GL_TEXTURE);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB_ARB,GL_SRC_COLOR);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB_ARB,k==5?GL_CONSTANT_ARB:GL_PRIMARY_COLOR_ARB);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB_ARB,GL_SRC_COLOR);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE2_RGB_ARB,k==5?GL_TEXTURE:GL_CONSTANT_ARB);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND2_RGB_ARB,k==4?GL_ONE_MINUS_SRC_COLOR:GL_SRC_COLOR);
  glColor4f(0.2f+k*0.05f,0.25f,0.4f-k*0.04f,1);quad_tc(x,y,64,112,0,0,1,1);} /* 4 x 7 pixels a texel: no texel edge inside a pixel */
 glTexEnvf(GL_TEXTURE_ENV,GL_RGB_SCALE_ARB,1);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&id);
}
/* CLAMP_TO_BORDER with nearest filters (and one linear quad, which stays in software), coordinates -0.5..1.5 (4 pixels a texel) */
static void s_border(void) {
 GLuint id;static const GLfloat bc[4]={1,0.5f,0,1},bc2[4]={0,0.3f,0.9f,0.5f};
 reset();id=scene_tex(0);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
 glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,bc);quad_tc(0,0,128,128,-0.5f,-0.5f,1.5f,1.5f);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,bc2);quad_tc(128,0,128,128,-0.5f,-0.5f,1.5f,1.5f);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);quad_tc(64,128,128,128,-0.25f,-0.25f,1.25f,1.25f);
 glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&id);
}
/* a texture whose levels each have their own colour, from size n down to 1x1 (maxl: GL_TEXTURE_MAX_LEVEL, -1 = the whole chain) */
static GLuint scene_levels(int n,int maxl) {
 static GLubyte t[512*512*4];GLuint id=0;int l=0,s,i;
 glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
 for(s=n;s>=1;s>>=1,l++){GLubyte r=(GLubyte)((l*97+40)&255),g=(GLubyte)((l*53+200)&255),b=(GLubyte)((l*151+10)&255);
  for(i=0;i<s*s;i++){int x=i%s,y=i/s,c=((x>>2)^(y>>2))&1;t[i*4]=c?r:(GLubyte)(r/2);t[i*4+1]=g;t[i*4+2]=c?b:(GLubyte)(255-b);t[i*4+3]=255;}
  glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA,s,s,0,GL_RGBA,GL_UNSIGNED_BYTE,t);}
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 if(maxl>=0)glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,maxl);
 return id;
}
/* a 512x512 chain (10 levels) at LODs 1, 2, 4, 6, 8, and a 64x64 chain cut at MAX_LEVEL 2 drawn small */
static void s_longmip(void) {
 GLuint a,b;static const float sz[5]={256,128,32,8,2};int k;
 reset();glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 a=scene_levels(512,-1);
 for(k=0;k<5;k++){float s=sz[k]*0.5f;quad_tc(k?(float)(140+k*20):4.0f,k?40.0f:4.0f,s,s,0,0,1,1);}
 b=scene_levels(64,2);quad_tc(150,150,16,16,0,0,1,1);quad_tc(180,150,4,4,0,0,1,1);quad_tc(200,150,64,64,0,0,1,1);
 glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&a);glDeleteTextures(1,&b);
}
/* unit 1 mipmapped (MODULATE) over a plain unit 0 at LODs 0, 1, 3 */
static void s_unitmip(void) {
 GLuint a,b;static const float sz[3]={64,32,8};int k;
 reset();if(!scene_active_tex||!scene_mtc2)return;
 scene_active_tex(GL_TEXTURE0_ARB);a=scene_tex(0);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 scene_active_tex(GL_TEXTURE1_ARB);b=scene_levels(64,-1);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
 for(k=0;k<3;k++){float x=(float)(10+k*80),y=40,s=sz[k];
  glBegin(GL_QUADS);scene_mtc2(GL_TEXTURE0_ARB,0,0);scene_mtc2(GL_TEXTURE1_ARB,0,0);glVertex2f(x,y);scene_mtc2(GL_TEXTURE0_ARB,1,0);scene_mtc2(GL_TEXTURE1_ARB,1,0);glVertex2f(x+s,y);
  scene_mtc2(GL_TEXTURE0_ARB,1,1);scene_mtc2(GL_TEXTURE1_ARB,1,1);glVertex2f(x+s,y+s);scene_mtc2(GL_TEXTURE0_ARB,0,1);scene_mtc2(GL_TEXTURE1_ARB,0,1);glVertex2f(x,y+s);glEnd();}
 glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&b);scene_active_tex(GL_TEXTURE0_ARB);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&a);
}
/* v10.307: three units (the core's three stages, two coordinate sets): unit 0 REPLACE, unit 1 MODULATE, unit 2 MODULATE; unit 2's coordinates
 * those of unit 0, then of unit 1 (which differ from unit 0's), then their own (software for that draw); 4 pixels a texel */
static void s_units3(void) {
 GLuint a,b,d;int k;
 reset();if(!scene_active_tex||!scene_mtc2)return;
 scene_active_tex(GL_TEXTURE0_ARB);a=scene_tex(0);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 scene_active_tex(GL_TEXTURE1_ARB);b=scene_tex(1);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
 scene_active_tex(GL_TEXTURE2_ARB);d=scene_tex(4);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
 for(k=0;k<3;k++){float x=(float)(8+k*82),y=40,s=64;static const float c[4][2]={{0,0},{1,0},{1,1},{0,1}};int v;
  glBegin(GL_QUADS);
  for(v=0;v<4;v++){float u0=c[v][0],v0=c[v][1],u1=c[v][0]*0.5f+0.25f,v1=c[v][1]*0.5f;
   scene_mtc2(GL_TEXTURE0_ARB,u0,v0);scene_mtc2(GL_TEXTURE1_ARB,u1,v1);
   if(k==0)scene_mtc2(GL_TEXTURE2_ARB,u0,v0);else if(k==1)scene_mtc2(GL_TEXTURE2_ARB,u1,v1);else scene_mtc2(GL_TEXTURE2_ARB,1-u0,v0);
   glVertex2f(x+c[v][0]*s,y+c[v][1]*s);}
  glEnd();glFlush();} /* each quad its own batch: the last one goes to software alone */
 /* v10.309 (board, v10.307: the engine drew the first two quads black, drawing, not skipping): which unit reads nothing. Solid 4x4 textures
  * on all three units, then each unit's own texture with solid ones on the other two (unit 2 on set 0 throughout) */
 {static GLubyte sol[3][64];GLuint s3[3];int j,u;static const GLubyte col[3][4]={{0xC0,0x80,0x40,0xFF},{0x80,0x80,0x80,0xFF},{0x80,0x80,0x80,0xFF}};
  for(u=0;u<3;u++){for(j=0;j<16;j++)memcpy(sol[u]+j*4,col[u],4);
   scene_active_tex(GL_TEXTURE0_ARB+u);glGenTextures(1,&s3[u]);glBindTexture(GL_TEXTURE_2D,s3[u]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,sol[u]);}
  for(k=0;k<4;k++){float x=(float)(8+k*62),y=150,s=48;static const float c[4][2]={{0,0},{1,0},{1,1},{0,1}};int v;
   for(u=0;u<3;u++){scene_active_tex(GL_TEXTURE0_ARB+u);glBindTexture(GL_TEXTURE_2D,(k==u+1)?(u==0?a:u==1?b:d):s3[u]);}
   glBegin(GL_QUADS);
   for(v=0;v<4;v++){scene_mtc2(GL_TEXTURE0_ARB,c[v][0],c[v][1]);scene_mtc2(GL_TEXTURE1_ARB,c[v][0]*0.5f+0.25f,c[v][1]*0.5f);scene_mtc2(GL_TEXTURE2_ARB,c[v][0],c[v][1]);glVertex2f(x+c[v][0]*s,y+c[v][1]*s);}
   glEnd();glFlush();}
  scene_active_tex(GL_TEXTURE0_ARB);glBindTexture(GL_TEXTURE_2D,a);scene_active_tex(GL_TEXTURE1_ARB);glBindTexture(GL_TEXTURE_2D,b);scene_active_tex(GL_TEXTURE2_ARB);glBindTexture(GL_TEXTURE_2D,d);
  glDeleteTextures(3,s3);}
 scene_active_tex(GL_TEXTURE2_ARB);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&d);
 scene_active_tex(GL_TEXTURE1_ARB);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&b);
 scene_active_tex(GL_TEXTURE0_ARB);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&a);
}
/* v10.309: scene "three texture units"'s quads for VXGL's per-quad report: x, y, size, what it shows */
static const struct {int x,y,s;const char *what;} units3_quads[7]={{8,40,64,"own textures, unit 2 on set 0"},{90,40,64,"own textures, unit 2 on set 1"},
 {172,40,64,"unit 2 on its own coordinates (software)"},{8,150,48,"solid 4x4 on all three"},{70,150,48,"unit 0's texture, solid 1 and 2"},
 {132,150,48,"unit 1's texture, solid 0 and 2"},{194,150,48,"unit 2's texture, solid 0 and 1"}};
/* v10.307: S3TC: an RGBA image stored as RGB_DXT1, RGBA_DXT1, RGBA_DXT3 and RGBA_DXT5 (the ICD's encoder), blended by its alpha over the
 * gradient; blocks given with glCompressedTexImage2D (a three-colour DXT1 block with transparent texels, and DXT5's six-value alpha form);
 * a DXT1 chain 32x32 to 1x1 drawn at LODs 0-3 (block rows narrower than 32 bytes). 4 pixels a texel. */
static void s_s3tc(void) {
 static const GLenum f[4]={GL_COMPRESSED_RGB_S3TC_DXT1_EXT,GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,GL_COMPRESSED_RGBA_S3TC_DXT3_EXT,GL_COMPRESSED_RGBA_S3TC_DXT5_EXT};
 static GLubyte t[32*32*4];GLuint id;int k,i;
 reset();grad();glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 for(i=0;i<256;i++){int x=i&15,y=i>>4;t[i*4]=(GLubyte)(x*17);t[i*4+1]=(GLubyte)(y*17);t[i*4+2]=(GLubyte)(255-((x+y)*8));t[i*4+3]=(GLubyte)((x<4&&y<4)?0:(x*16+8));}
 for(k=0;k<4;k++){glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,0,f[k],16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,t);quad_tc((float)(k*64),0,64,64,0,0,1,1);glDeleteTextures(1,&id);}
 if(scene_ctex){ /* 8x8 = 2x2 blocks each */
  static const GLubyte d1[32]={0x00,0xF8,0x1F,0x00,0xE4,0xE4,0xE4,0xE4, 0x1F,0x00,0x00,0xF8,0x1B,0x1B,0x1B,0x1B, /* c0 > c1, then c0 <= c1: colour 3 transparent */
   0xE0,0x07,0xE0,0x07,0xFF,0x00,0xFF,0x00, 0x00,0x00,0xFF,0xFF,0x55,0xAA,0x55,0xAA};
  static const GLubyte d5[64]={0x20,0xE0,0x88,0x46,0x2C,0x88,0x46,0x2C, 0x00,0xF8,0x1F,0x00,0x00,0x00,0x00,0x00, /* a0 <= a1: the six-value form with 0 and 255 */
   0xF0,0x10,0x88,0x46,0x2C,0x88,0x46,0x2C, 0xE0,0x07,0x1F,0x00,0x55,0x55,0x55,0x55,
   0x80,0x80,0x00,0x00,0x00,0x00,0x00,0x00, 0xFF,0xFF,0x00,0x00,0xAA,0xAA,0xAA,0xAA,
   0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF, 0x1F,0x00,0xE0,0x07,0x00,0x55,0xAA,0xFF};
  glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  scene_ctex(GL_TEXTURE_2D,0,GL_COMPRESSED_RGBA_S3TC_DXT1_EXT,8,8,0,32,d1);quad_tc(0,72,64,64,0,0,1,1);
  scene_ctex(GL_TEXTURE_2D,0,GL_COMPRESSED_RGB_S3TC_DXT1_EXT,8,8,0,32,d1);quad_tc(64,72,64,64,0,0,1,1);
  scene_ctex(GL_TEXTURE_2D,0,GL_COMPRESSED_RGBA_S3TC_DXT5_EXT,8,8,0,64,d5);quad_tc(128,72,64,64,0,0,1,1);
  glDeleteTextures(1,&id);}
 glDisable(GL_BLEND);
 {int s,l=0;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
  for(s=32;s>=1;s>>=1,l++){for(i=0;i<s*s;i++){int x=i%s,y=i/s;t[i*4]=(GLubyte)((l*97+40)&255);t[i*4+1]=(GLubyte)(((x>>1)^(y>>1))&1?220:60);t[i*4+2]=(GLubyte)((l*151+10)&255);t[i*4+3]=255;}
   glTexImage2D(GL_TEXTURE_2D,l,GL_COMPRESSED_RGB_S3TC_DXT1_EXT,s,s,0,GL_RGBA,GL_UNSIGNED_BYTE,t);}
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  quad_tc(0,150,128,128,0,0,1,1);quad_tc(136,150,32,32,0,0,1,1);quad_tc(176,150,16,16,0,0,1,1);quad_tc(200,150,8,8,0,0,1,1);
  glDeleteTextures(1,&id);}
 glDisable(GL_TEXTURE_2D);
}
/* v10.308: polygon stipple on the engine: an asymmetric 32x32 pattern over shaded triangles at odd offsets (the pattern is the window's,
 * not the triangle's), a strip and a fan; then, with the stipple still on, an outlined quad and a line strip (not stippled: software) */
static void s_pstipple(void) {
 static GLubyte pat[128];int r,x,k;
 for(r=0;r<32;r++)for(x=0;x<32;x++){int on=((x+r*3)%7<3)||x==r||(r<4&&x<8);if(on)pat[r*4+x/8]|=(GLubyte)(0x80>>(x&7));}
 reset();grad();glPolygonStipple(pat);glEnable(GL_POLYGON_STIPPLE);
 for(k=0;k<3;k++){float x0=(float)(13+k*77),y0=(float)(9+k*5);glBegin(GL_TRIANGLES);glColor3f(1,0.2f*k,0);glVertex2f(x0,y0);glColor3f(0,1,0.3f*k);glVertex2f(x0+70,y0+20);glColor3f(0.2f,0.2f,1);glVertex2f(x0+20,y0+90);glEnd();}
 glBegin(GL_TRIANGLE_STRIP);for(k=0;k<6;k++){glColor3f(k/6.f,1,1-k/6.f);glVertex2f(10.0f+k*40,130.0f+(k&1)*35);}glEnd();
 glBegin(GL_TRIANGLE_FAN);glColor3f(1,1,1);glVertex2f(200,215);for(k=0;k<7;k++){float a=(float)k*0.5f;glColor3f(k/7.f,0.5f,1-k/7.f);glVertex2f(200+(float)cos(a)*40,215+(float)sin(a)*35);}glEnd();
 glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);glBegin(GL_QUADS);glColor3f(1,1,0);glVertex2f(20.5f,180.5f);glVertex2f(120.5f,180.5f);glVertex2f(120.5f,240.5f);glVertex2f(20.5f,240.5f);glEnd();
 glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
 glBegin(GL_LINE_STRIP);glColor3f(1,0,1);for(k=0;k<5;k++)glVertex2f(130.5f+k*12,175.5f+(k&1)*20);glEnd();
 glDisable(GL_POLYGON_STIPPLE);
}
/* v10.315: LOD clamps on the engine: a 32x32 chain, one colour a level and a checker inside each, drawn at LODs 0-3 with MAX_LOD 1 (levels
 * 2 and 3 not reached), with MIN_LOD -2 (no change) and, last, MAX_LOD 2.5 (between levels: software) */
static void s_lodclamp(void) {
 static GLubyte t[32*32*4];GLuint id;int s,l,i,k;
 reset();glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
 for(s=32,l=0;s>=1;s>>=1,l++){for(i=0;i<s*s;i++){int x=i%s,y=i/s,on=((x>>1)^(y>>1))&1;t[i*4]=(GLubyte)((l*97+40)&255);t[i*4+1]=(GLubyte)(on?220:60);t[i*4+2]=(GLubyte)((l*151+10)&255);t[i*4+3]=255;}
  glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA,s,s,0,GL_RGBA,GL_UNSIGNED_BYTE,t);}
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 for(k=0;k<3;k++){float y=(float)(8+k*80);
  if(k==0){glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_LOD,1.0f);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,-1000.0f);}
  if(k==1){glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_LOD,1.0f);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,-2.0f);}
  if(k==2){glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_LOD,2.5f);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,-1000.0f);}
  quad_tc(0,y,64,64,0,0,1,1);quad_tc(72,y,32,32,0,0,1,1);quad_tc(112,y,16,16,0,0,1,1);quad_tc(136,y,8,8,0,0,1,1);quad_tc(152,y,4,4,0,0,1,1);}
 glDeleteTextures(1,&id);glDisable(GL_TEXTURE_2D);
}
/* ------------------------------------------------------------------ v10.317 scenes */
#ifndef GL_TEXTURE_RECTANGLE_ARB
#define GL_TEXTURE_RECTANGLE_ARB 0x84F5
#endif
#ifndef GL_COMPRESSED_RGB_FXT1_3DFX
#define GL_COMPRESSED_RGB_FXT1_3DFX 0x86B0
#define GL_COMPRESSED_RGBA_FXT1_3DFX 0x86B1
#endif
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D 0x806F
#endif
#ifndef GL_TEXTURE_WRAP_R
#define GL_TEXTURE_WRAP_R 0x8072
#endif
#ifndef GL_DEPTH_TEXTURE_MODE
#define GL_DEPTH_TEXTURE_MODE 0x884B
#endif
#ifndef GL_TEXTURE_COLOR_TABLE_SGI
#define GL_TEXTURE_COLOR_TABLE_SGI 0x80BC
#endif
#ifndef GL_DOT3_RGB_ARB
#define GL_DOT3_RGB_ARB 0x86AE
#define GL_DOT3_RGBA_ARB 0x86AF
#endif
#ifndef GL_SOURCE0_ALPHA_ARB
#define GL_SOURCE0_ALPHA_ARB 0x8588
#define GL_SOURCE1_ALPHA_ARB 0x8589
#define GL_OPERAND0_ALPHA_ARB 0x8598
#define GL_OPERAND1_ALPHA_ARB 0x8599
#endif
#ifndef GL_TEXTURE_CUBE_MAP_ARB
#define GL_TEXTURE_CUBE_MAP_ARB 0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB 0x8515
#endif
typedef void (APIENTRY *tex3d_fn)(GLenum,GLint,GLint,GLsizei,GLsizei,GLsizei,GLint,GLenum,GLenum,const GLvoid*); /* glTexImage3D */
typedef void (APIENTRY *ctab_fn)(GLenum,GLenum,GLsizei,GLenum,GLenum,const GLvoid*); /* glColorTable(SGI) */
static tex3d_fn scene_tex3d;static ctab_fn scene_ctab;
static int scene_has(const char *x) {const char *e=(const char*)glGetString(GL_EXTENSIONS);return e&&strstr(e,x)!=0;}
/* a w x h RGBA image of steps every texel (neighbours differ in every channel), on target (filters nearest, edges clamped) */
static GLuint scene_img(GLenum target,int w,int h,GLenum ifmt) {
 static GLubyte t[64*64*4];GLuint id=0;int x,y;
 for(y=0;y<h;y++)for(x=0;x<w;x++){GLubyte *p=t+(y*w+x)*4;p[0]=(GLubyte)((x*41+y*7)&255);p[1]=(GLubyte)((y*53+x*13+60)&255);p[2]=(GLubyte)(((x^y)*29+120)&255);p[3]=(GLubyte)(((x+y)*23+90)&255);}
 glGenTextures(1,&id);glBindTexture(target,id);glTexParameteri(target,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(target,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexParameteri(target,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(target,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glPixelStorei(GL_UNPACK_ALIGNMENT,1);
 if(target==GL_TEXTURE_1D)glTexImage1D(target,0,ifmt,w,0,GL_RGBA,GL_UNSIGNED_BYTE,t);else glTexImage2D(target,0,ifmt,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,t);
 glPixelStorei(GL_UNPACK_ALIGNMENT,4);
 return id;
}
/* v10.317: non-power-of-two textures (24x20, nearest at 4 pixels a texel; linear past the edges), a 24x16 one repeating in T, a 1D one of 12,
 * and rectangle textures (30x18: coordinates in texels, nearest at 3 pixels a texel; linear past the edges): padded copies on the engine */
static void s_npot(void) {
 GLuint id;
 reset();grad();glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 glEnable(GL_TEXTURE_2D);id=scene_img(GL_TEXTURE_2D,24,20,GL_RGBA);
 quad_tc(8,8,96,80,0,0,1,1);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);quad_tc(8,96,96,80,-0.25f,-0.25f,1.25f,1.25f);
 glDeleteTextures(1,&id);
 id=scene_img(GL_TEXTURE_2D,24,16,GL_RGB);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);quad_tc(150,8,96,64,0,0,1,2);glDeleteTextures(1,&id);
 glDisable(GL_TEXTURE_2D);
 glEnable(GL_TEXTURE_1D);id=scene_img(GL_TEXTURE_1D,12,1,GL_RGBA);quad_tc(150,80,96,16,0,0,1,1);glDeleteTextures(1,&id);glDisable(GL_TEXTURE_1D);
 if(scene_has("GL_ARB_texture_rectangle")||scene_has("GL_NV_texture_rectangle")){
  glEnable(GL_TEXTURE_RECTANGLE_ARB);id=scene_img(GL_TEXTURE_RECTANGLE_ARB,30,18,GL_RGBA);
  quad_tc(8,184,90,54,0,0,30,18);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_MAG_FILTER,GL_LINEAR);quad_tc(150,120,96,64,-3,-3,33,21);
  glDeleteTextures(1,&id);glDisable(GL_TEXTURE_RECTANGLE_ARB);}
}
/* v10.317: CLAMP_TO_BORDER filtered linearly: a 16x16 texture at 4 pixels a texel from -0.5 to 1.5, a 12x10 one past its edges, a 20x14
 * rectangle texture (nearest), and border in S with repeat in T: a ring of the border colour around each copy */
static void s_border2(void) {
 GLuint id;static const GLfloat bc[4]={1,0.5f,0,1},bc2[4]={0,0.3f,0.9f,0.5f};
 reset();grad();glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glEnable(GL_TEXTURE_2D);
 id=scene_img(GL_TEXTURE_2D,16,16,GL_RGBA);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
 glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,bc);quad_tc(0,0,128,128,-0.5f,-0.5f,1.5f,1.5f);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,bc2);quad_tc(136,136,112,112,-0.25f,-0.25f,1.5f,1.5f);
 glDeleteTextures(1,&id);
 id=scene_img(GL_TEXTURE_2D,12,10,GL_RGBA);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
 glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,bc2);quad_tc(136,0,112,112,-0.25f,-0.25f,1.25f,1.25f);
 glDeleteTextures(1,&id);glDisable(GL_TEXTURE_2D);
 if(scene_has("GL_ARB_texture_rectangle")||scene_has("GL_NV_texture_rectangle")){
  glEnable(GL_TEXTURE_RECTANGLE_ARB);id=scene_img(GL_TEXTURE_RECTANGLE_ARB,20,14,GL_RGBA);
  glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_BORDER);glTexParameteri(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_BORDER);
  glTexParameterfv(GL_TEXTURE_RECTANGLE_ARB,GL_TEXTURE_BORDER_COLOR,bc);quad_tc(0,136,112,112,-4,-4,24,24);
  glDeleteTextures(1,&id);glDisable(GL_TEXTURE_RECTANGLE_ARB);}
}
/* v10.317: FXT1 (Mesa's encoder; decoded copies on the engine), RGB and RGBA, blended by alpha over the gradient, and an RGB_DXT1 texture under
 * GL_COMBINE (decoded: the sampler's three-colour blocks read colour 3 as transparent, OpenGL's as black) */
static void s_fxt1(void) {
 static GLubyte t[16*16*4];GLuint id;int i,k;
 reset();grad();glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 for(i=0;i<256;i++){int x=i&15,y=i>>4;t[i*4]=(GLubyte)(x*17);t[i*4+1]=(GLubyte)(y*17);t[i*4+2]=(GLubyte)(255-((x+y)*8));t[i*4+3]=(GLubyte)((x<4&&y<4)?0:(x*16+8));}
 if(scene_has("GL_3DFX_texture_compression_FXT1"))for(k=0;k<2;k++){glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,0,k?GL_COMPRESSED_RGBA_FXT1_3DFX:GL_COMPRESSED_RGB_FXT1_3DFX,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,t);quad_tc((float)(8+k*72),8,64,64,0,0,1,1);glDeleteTextures(1,&id);}
 glDisable(GL_BLEND);
 if(scene_has("GL_EXT_texture_compression_s3tc")&&scene_combine){glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,0,GL_COMPRESSED_RGB_S3TC_DXT1_EXT,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,t);
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE_ARB);glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB_ARB,GL_MODULATE);
  glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB_ARB,GL_TEXTURE);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB_ARB,GL_PRIMARY_COLOR_ARB);
  glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB_ARB,GL_SRC_COLOR);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB_ARB,GL_SRC_COLOR);
  glColor3f(0.9f,0.7f,1.0f);quad_tc(152,8,64,64,0,0,1,1);glColor3f(1,1,1);
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glDeleteTextures(1,&id);}
 glDisable(GL_TEXTURE_2D);
}
/* v10.317: texture colour tables (SGI_texture_color_table: RGBA and LUMINANCE tables on nearest textures, applied when the copy is made) and
 * DEPTH_COMPONENT textures without a compare (LUMINANCE, INTENSITY and ALPHA modes, nearest and linear: the depth as a colour) */
static void s_ctable(void) {
 static GLubyte lut[256*4];static GLfloat dz[16*16];GLuint id;int i;
 reset();grad();glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 if(scene_ctab&&scene_has("GL_SGI_texture_color_table")){
  for(i=0;i<256;i++){lut[i*4]=(GLubyte)(255-i);lut[i*4+1]=(GLubyte)(i/2+64);lut[i*4+2]=(GLubyte)((i*3)&255);lut[i*4+3]=(GLubyte)(255-i/3);}
  /* clamped to the edge: swrast's fast REPEAT triangles (s_triangle.c) leave the table out */
  id=scene_tex(0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
  scene_ctab(GL_TEXTURE_COLOR_TABLE_SGI,GL_RGBA,256,GL_RGBA,GL_UNSIGNED_BYTE,lut);glEnable(GL_TEXTURE_COLOR_TABLE_SGI);
  quad_tc(8,8,64,64,0,0,1,1);
  for(i=0;i<256;i++)lut[i]=(GLubyte)((i*7)&255);
  scene_ctab(GL_TEXTURE_COLOR_TABLE_SGI,GL_LUMINANCE,256,GL_LUMINANCE,GL_UNSIGNED_BYTE,lut);quad_tc(80,8,64,64,0,0,1,1);
  /* v10.321: filtered linearly: an affine table on the engine (each component a line: 255 - i, i / 2 + 64, i, 200 - i / 4), then one that
   * is not (software) */
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  for(i=0;i<256;i++){lut[i*4]=(GLubyte)(255-i);lut[i*4+1]=(GLubyte)(i/2+64);lut[i*4+2]=(GLubyte)i;lut[i*4+3]=(GLubyte)(200-i/4);}
  scene_ctab(GL_TEXTURE_COLOR_TABLE_SGI,GL_RGBA,256,GL_RGBA,GL_UNSIGNED_BYTE,lut);quad_tc(152,8,64,64,0,0,1,1);
  for(i=0;i<256;i++)lut[i*4+2]=(GLubyte)((i*3)&255);
  scene_ctab(GL_TEXTURE_COLOR_TABLE_SGI,GL_RGBA,256,GL_RGBA,GL_UNSIGNED_BYTE,lut);quad_tc(100,168,64,64,0,0,1,1);
  glDisable(GL_TEXTURE_COLOR_TABLE_SGI);glDeleteTextures(1,&id);}
 if(scene_has("GL_ARB_depth_texture")){static const GLenum mode[3]={GL_LUMINANCE,GL_INTENSITY,GL_ALPHA};int k;
  for(i=0;i<256;i++){int x=i&15,y=i>>4;dz[i]=(float)((x*13+y*9)%64)/63.0f;}
  glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexImage2D(GL_TEXTURE_2D,0,GL_DEPTH_COMPONENT,16,16,0,GL_DEPTH_COMPONENT,GL_FLOAT,dz);
  glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
  for(k=0;k<3;k++){glTexParameteri(GL_TEXTURE_2D,GL_DEPTH_TEXTURE_MODE,mode[k]);quad_tc((float)(8+k*80),88,64,64,0,0,1,1);}
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_DEPTH_TEXTURE_MODE,GL_INTENSITY);
  /* 76 wide, not 80: no pixel centre lands on a texel centre, where swrast's FRAC(u+1.0F) rounds a u just below 0 to a whole weight on the wrapped texel */
  quad_tc(8,168,76,76,0,0,1,1);
  glDisable(GL_BLEND);glDeleteTextures(1,&id);}
 glDisable(GL_TEXTURE_2D);
}
/* v10.317: the LOD clamps the engine now draws: MIN_LOD 1 and 1.3 (with MAX_LOD 2.6) on NEAREST_MIPMAP_NEAREST, MIN_LOD 2 on LINEAR_MIPMAP_LINEAR,
 * MAX_LOD -0.5 (always magnified), on the 32x32 chain at LODs -1 to 3 */
static void s_lodclamp2(void) {
 static GLubyte t[32*32*4];GLuint id;int s,l,i,k;
 reset();glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
 for(s=32,l=0;s>=1;s>>=1,l++){for(i=0;i<s*s;i++){int x=i%s,y=i/s,on=((x>>1)^(y>>1))&1;t[i*4]=(GLubyte)((l*97+40)&255);t[i*4+1]=(GLubyte)(on?220:60);t[i*4+2]=(GLubyte)((l*151+10)&255);t[i*4+3]=255;}
  glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA,s,s,0,GL_RGBA,GL_UNSIGNED_BYTE,t);}
 for(k=0;k<4;k++){float y=(float)(4+k*64);
  if(k==0){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
   glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,1.0f);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_LOD,1000.0f);}
  if(k==1){glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,1.3f);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_LOD,2.6f);}
  if(k==2){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
   glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,2.0f);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_LOD,1000.0f);}
  if(k==3){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,-1000.0f);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAX_LOD,-0.5f);}
  quad_tc(0,y,64,64,0,0,1,1);quad_tc(72,y,32,32,0,0,1,1);quad_tc(112,y,16,16,0,0,1,1);quad_tc(136,y,8,8,0,0,1,1);quad_tc(152,y,4,4,0,0,1,1);}
 glDeleteTextures(1,&id);glDisable(GL_TEXTURE_2D);
}
/* v10.317: GL_DOT3_RGB (the dot product into colour only, alpha the alpha combiner's), blended by that alpha over the gradient: one unit with
 * its alpha MODULATE and REPLACE (the primary colour's), scaled by 2, then on unit 1 over unit 0 (alpha REPLACE PREVIOUS and MODULATE) */
static void dot3_unit(GLenum a0,GLenum s0,GLenum s1,GLenum amode,GLenum as0,GLenum as1,float scale) {
 glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE_ARB);glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB_ARB,a0);
 glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB_ARB,s0);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB_ARB,s1);
 glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB_ARB,GL_SRC_COLOR);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB_ARB,GL_SRC_COLOR);
 glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA_ARB,amode);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA_ARB,as0);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_ALPHA_ARB,as1);
 glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA_ARB,GL_SRC_ALPHA);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_ALPHA_ARB,GL_SRC_ALPHA);glTexEnvf(GL_TEXTURE_ENV,GL_RGB_SCALE_ARB,scale);
}
static void s_dot3rgb(void) {
 GLuint a,b;
 reset();grad();if(!scene_combine||!scene_has("GL_ARB_texture_env_dot3"))return;
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 a=scene_tex(0);glEnable(GL_TEXTURE_2D);
 dot3_unit(GL_DOT3_RGB_ARB,GL_TEXTURE,GL_PRIMARY_COLOR_ARB,GL_MODULATE,GL_TEXTURE,GL_PRIMARY_COLOR_ARB,1);glColor4f(0.8f,0.3f,0.6f,0.8f);quad_tc(8,8,64,112,0,0,1,1);
 dot3_unit(GL_DOT3_RGB_ARB,GL_TEXTURE,GL_PRIMARY_COLOR_ARB,GL_REPLACE,GL_PRIMARY_COLOR_ARB,GL_TEXTURE,1);glColor4f(0.2f,0.9f,0.5f,0.6f);quad_tc(88,8,64,112,0,0,1,1);
 dot3_unit(GL_DOT3_RGB_ARB,GL_TEXTURE,GL_PRIMARY_COLOR_ARB,GL_REPLACE,GL_TEXTURE,GL_TEXTURE,2);glColor4f(0.7f,0.6f,0.2f,1);quad_tc(168,8,64,112,0,0,1,1);
 glTexEnvf(GL_TEXTURE_ENV,GL_RGB_SCALE_ARB,1);
 if(scene_active_tex&&scene_mtc2){int k;
  for(k=0;k<2;k++){float x=(float)(8+k*120),y=136;
   scene_active_tex(GL_TEXTURE0_ARB);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,k?GL_MODULATE:GL_REPLACE);
   scene_active_tex(GL_TEXTURE1_ARB);b=scene_tex(1);glEnable(GL_TEXTURE_2D);
   dot3_unit(GL_DOT3_RGB_ARB,GL_PREVIOUS_ARB,GL_TEXTURE,k?GL_MODULATE:GL_REPLACE,GL_PREVIOUS_ARB,GL_TEXTURE,1);
   glColor4f(0.9f,0.4f,0.7f,0.7f);
   glBegin(GL_QUADS);scene_mtc2(GL_TEXTURE0_ARB,0,0);scene_mtc2(GL_TEXTURE1_ARB,0,0);glVertex2f(x,y);scene_mtc2(GL_TEXTURE0_ARB,1,0);scene_mtc2(GL_TEXTURE1_ARB,1,0);glVertex2f(x+112,y);
   scene_mtc2(GL_TEXTURE0_ARB,1,1);scene_mtc2(GL_TEXTURE1_ARB,1,1);glVertex2f(x+112,y+112);scene_mtc2(GL_TEXTURE0_ARB,0,1);scene_mtc2(GL_TEXTURE1_ARB,0,1);glVertex2f(x,y+112);glEnd();
   glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&b);scene_active_tex(GL_TEXTURE0_ARB);}}
 glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&a);glDisable(GL_BLEND);glColor4f(1,1,1,1);
}
/* v10.317: a cube map with its mip chain (32x32 faces, a colour a face and level, a checker in each), its +Z and -Z faces drawn at LODs -1 to 3
 * (r = +-0.5, s and t -0.5 to 0.5: the face's coordinates step as s and t do, as swrast takes the LOD), nearest and linear between levels */
static void cube_quad(float x,float y,float w,float r) {
 glBegin(GL_QUADS);glTexCoord3f(-0.5f,-0.5f,r);glVertex2f(x,y);glTexCoord3f(0.5f,-0.5f,r);glVertex2f(x+w,y);glTexCoord3f(0.5f,0.5f,r);glVertex2f(x+w,y+w);glTexCoord3f(-0.5f,0.5f,r);glVertex2f(x,y+w);glEnd();
}
static void s_cubemip(void) {
 static GLubyte t[32*32*4];GLuint id;int f,s,l,i,k;
 reset();if(!scene_has("GL_ARB_texture_cube_map"))return;
 glGenTextures(1,&id);glBindTexture(GL_TEXTURE_CUBE_MAP_ARB,id);
 for(f=0;f<6;f++)for(s=32,l=0;s>=1;s>>=1,l++){for(i=0;i<s*s;i++){int x=i%s,y=i/s,on=((x>>1)^(y>>1))&1;t[i*4]=(GLubyte)((l*97+f*31+40)&255);t[i*4+1]=(GLubyte)(on?220:60);t[i*4+2]=(GLubyte)((l*151+f*57+10)&255);t[i*4+3]=255;}
  glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB+f,l,GL_RGBA,s,s,0,GL_RGBA,GL_UNSIGNED_BYTE,t);}
 glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE); /* the engine clamps a face at its edges */
 glEnable(GL_TEXTURE_CUBE_MAP_ARB);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 for(k=0;k<3;k++){float y=(float)(8+k*80),r=k==1?-0.5f:0.5f;
  if(k==2){glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB,GL_TEXTURE_MAG_FILTER,GL_LINEAR);}
  cube_quad(0,y,64,r);cube_quad(72,y,32,r);cube_quad(112,y,16,r);cube_quad(136,y,8,r);cube_quad(152,y,4,r);}
 glDisable(GL_TEXTURE_CUBE_MAP_ARB);glDeleteTextures(1,&id);
}
/* v10.317: a 3D texture (16x16x8, a colour a slice and steps inside): each slice drawn at its middle r (nearest, 3 pixels a texel), r past
 * the end with REPEAT, and r across a quad with linear filters (two slices blended) */
static void s_tex3d(void) {
 static GLubyte t[16*16*8*4];GLuint id;int x,y,z,k;
 reset();grad();if(!scene_tex3d)return;
 for(z=0;z<8;z++)for(y=0;y<16;y++)for(x=0;x<16;x++){GLubyte *p=t+((z*16+y)*16+x)*4;p[0]=(GLubyte)((z*31+x*9)&255);p[1]=(GLubyte)((y*15+z*7+40)&255);p[2]=(GLubyte)((z*53+(x^y)*5+20)&255);p[3]=255;}
 glGenTextures(1,&id);glBindTexture(GL_TEXTURE_3D,id);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_REPEAT);
 glPixelStorei(GL_UNPACK_ALIGNMENT,1);scene_tex3d(GL_TEXTURE_3D,0,GL_RGBA,16,16,8,0,GL_RGBA,GL_UNSIGNED_BYTE,t);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
 glEnable(GL_TEXTURE_3D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 for(k=0;k<9;k++){float x0=(float)(4+(k%5)*50),y0=(float)(4+(k/5)*52),r=k<8?((float)k+0.5f)/8.0f:1.0f+2.5f/8.0f;
  glBegin(GL_QUADS);glTexCoord3f(0,0,r);glVertex2f(x0,y0);glTexCoord3f(1,0,r);glVertex2f(x0+48,y0);glTexCoord3f(1,1,r);glVertex2f(x0+48,y0+48);glTexCoord3f(0,1,r);glVertex2f(x0,y0+48);glEnd();}
 glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glBegin(GL_QUADS);glTexCoord3f(0,0,0.0625f);glVertex2f(4,112);glTexCoord3f(1,0,0.9375f);glVertex2f(244,112);glTexCoord3f(1,1,0.9375f);glVertex2f(244,248);glTexCoord3f(0,1,0.0625f);glVertex2f(4,248);glEnd();
 glDisable(GL_TEXTURE_3D);glDeleteTextures(1,&id);
}
/* v10.321: a vertex program's point size (VERTEX_PROGRAM_POINT_SIZE): sizes 1 to 12 across the window (result.pointsize from x), colours
 * from the vertices, then the same with a texture (MODULATE) */
#ifndef GL_VERTEX_PROGRAM_ARB
#define GL_VERTEX_PROGRAM_ARB 0x8620
#endif
#ifndef GL_VERTEX_PROGRAM_POINT_SIZE_ARB
#define GL_VERTEX_PROGRAM_POINT_SIZE_ARB 0x8642
#endif
static void s_vpsize(void) {
 static const char vp[]="!!ARBvp1.0\nPARAM mvp[4]={state.matrix.mvp};\nDP4 result.position.x, mvp[0], vertex.position;\nDP4 result.position.y, mvp[1], vertex.position;\n"
  "DP4 result.position.z, mvp[2], vertex.position;\nDP4 result.position.w, mvp[3], vertex.position;\nMOV result.color, vertex.color;\nMOV result.texcoord[0], vertex.texcoord[0];\n"
  "MAD result.pointsize.x, vertex.position.xxxx, 0.045, 1.0;\nEND\n";
 GLuint id=0;int i,pass; /* made in each context (the host test and VXGL make one a run) */
 reset();grad();if(!scene_gen_prog||!scene_bind_prog||!scene_prog_str||!scene_has("GL_ARB_vertex_program"))return;
 scene_gen_prog(1,&id);scene_bind_prog(GL_VERTEX_PROGRAM_ARB,id);scene_prog_str(GL_VERTEX_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)(sizeof(vp)-1),vp);glEnable(GL_VERTEX_PROGRAM_ARB);glEnable(GL_VERTEX_PROGRAM_POINT_SIZE_ARB);
 for(pass=0;pass<2;pass++){GLuint t=0;
  if(pass){t=scene_tex(0);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);}
  glBegin(GL_POINTS);
  for(i=0;i<12;i++){float x=10.0f+i*20.0f,y=pass?170.0f:60.0f;glColor3f(1.0f-i/12.0f,0.3f+i*0.05f,0.5f);glTexCoord2f(i/12.0f,pass?0.7f:0.2f);glVertex2f(x,y);glVertex2f(x+3.5f,y+40.5f);}
  glEnd();
  if(pass){glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&t);}}
 glDisable(GL_VERTEX_PROGRAM_POINT_SIZE_ARB);glDisable(GL_VERTEX_PROGRAM_ARB);scene_bind_prog(GL_VERTEX_PROGRAM_ARB,0);
}
/* v4.4: antialiased points, lines and polygons (blended over the gradient: the coverage shows) */
static void s_aa(void) {
 int i;GLuint t;reset();grad();glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 glEnable(GL_POINT_SMOOTH);
 for(i=0;i<8;i++){static const float sz[8]={1.0f,1.5f,2.7f,4.0f,6.3f,9.5f,12.0f,15.0f};glPointSize(sz[i]);glBegin(GL_POINTS);glColor4f(1,1-i/8.0f,i/8.0f,1);
  glVertex2f(12.3f+i*30,240.6f);glColor4f(0.2f,1,0.4f,0.7f);glVertex2f(14.0f+i*30,218.0f);glEnd();}
 glPointSize(1);glDisable(GL_POINT_SMOOTH);
 glEnable(GL_LINE_SMOOTH);
 for(i=0;i<10;i++){float a=i*0.31f,w=1.0f+(i%4)*1.1f;glLineWidth(w);glBegin(GL_LINES);glColor4f(1,0.9f,0.2f,1);glVertex2f(60.2f,140.4f);
  glColor4f(0.3f,0.6f,1,0.8f);glVertex2f(60.2f+50*(float)cos(a),140.4f+50*(float)sin(a));glEnd();}
 glLineWidth(2.0f);glEnable(GL_LINE_STIPPLE);glLineStipple(2,0x0F33);glBegin(GL_LINE_STRIP);glColor3f(1,1,1);glVertex2f(10.5f,95.3f);glVertex2f(120.7f,105.1f);glVertex2f(125.2f,190.8f);glEnd();
 glDisable(GL_LINE_STIPPLE);glLineWidth(1);
 t=scene_tex(0);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
 glLineWidth(3.0f);glBegin(GL_LINES);glColor3f(1,1,1);glTexCoord2f(0.05f,0.1f);glVertex2f(8.4f,80.2f);glTexCoord2f(0.95f,0.8f);glVertex2f(122.8f,62.6f);glEnd();glLineWidth(1);
 glEnable(GL_POINT_SMOOTH);glPointSize(7.5f);glBegin(GL_POINTS);glTexCoord2f(0.3f,0.6f);glVertex2f(20.6f,40.3f);glTexCoord2f(0.8f,0.2f);glVertex2f(44.1f,40.8f);glEnd();glPointSize(1);glDisable(GL_POINT_SMOOTH);
 glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&t);glDisable(GL_LINE_SMOOTH);
 glEnable(GL_POLYGON_SMOOTH);
 glBegin(GL_TRIANGLES);glColor4f(1,0.2f,0.2f,1);glVertex2f(140.3f,120.6f);glColor4f(0.2f,1,0.2f,1);glVertex2f(250.1f,140.2f);glColor4f(0.2f,0.2f,1,0.9f);glVertex2f(170.8f,200.9f);
 glColor4f(1,1,0,1);glVertex2f(140.5f,20.5f);glVertex2f(250.5f,30.2f);glVertex2f(141.0f,24.0f); /* a sliver */
 glColor4f(0,1,1,1);glVertex2f(200.2f,60.4f);glVertex2f(201.9f,60.9f);glVertex2f(200.8f,62.3f); /* a small one */
 glColor4f(1,0.5f,0,1);glVertex2f(150.0f,50.0f);glVertex2f(190.0f,50.0f);glVertex2f(150.0f,100.0f); /* whole-pixel edges */
 glEnd();
 glEnable(GL_CULL_FACE);glBegin(GL_TRIANGLES);glColor3f(1,1,1);glVertex2f(200,80);glVertex2f(240,110);glVertex2f(250,80);glColor3f(1,0,1);glVertex2f(205,75);glVertex2f(250,75);glVertex2f(235,55);glEnd();glDisable(GL_CULL_FACE);
 glDisable(GL_POLYGON_SMOOTH);glDisable(GL_BLEND);glBlendFunc(GL_ONE,GL_ZERO);
}
/* v4.4: fragment programs that stages compute (each quad its own program) */
static void s_fprog(void) {
 static const char *const prog[]={
  "!!ARBfp1.0\nTEMP t;\nTEX t, fragment.texcoord[0], texture[0], 2D;\nMUL result.color, t, fragment.color;\nEND\n",
  "!!ARBfp1.0\nTEMP a,b;\nTEX a, fragment.texcoord[0], texture[0], 2D;\nTEX b, fragment.texcoord[1], texture[1], 2D;\nMAD result.color, a, {0.5,0.25,0.75,1}, b;\nEND\n",
  "!!ARBfp1.0\nTEMP a,b;\nTEX a, fragment.texcoord[0], texture[0], 2D;\nTEX b, fragment.texcoord[1], texture[1], 2D;\nLRP result.color, fragment.color.w, a, b;\nEND\n",
  "!!ARBfp1.0\nPARAM one={1,1,1,1};\nTEMP n,l;\nTEX n, fragment.texcoord[0], texture[0], 2D;\nMAD n, n, 2, -1;\nMAD l, fragment.color, 2, -1;\nDP3_SAT result.color.xyz, n, l;\nMOV result.color.w, one;\nEND\n",
  "!!ARBfp1.0\nMUL result.color, fragment.color, {0.5,1,0.25,1};\nEND\n",
  "!!ARBfp1.0\nADD result.color, fragment.color, fragment.color;\nEND\n",
  "!!ARBfp1.0\nOPTION ARB_fog_linear;\nTEMP t;\nTEX t, fragment.texcoord[0], texture[0], 2D;\nMUL result.color, t, fragment.color;\nEND\n",
  "!!ARBfp1.0\nTEMP a,b,r;\nTEX a, fragment.texcoord[0], texture[0], 2D;\nTEX b, fragment.texcoord[1], texture[1], 2D;\nSUB_SAT r, a, fragment.color;\nADD result.color, r, b;\nEND\n",
  "!!ARBfp1.0\nTEMP a,b;\nTEX a, fragment.texcoord[0], texture[0], 2D;\nTEX b, fragment.texcoord[1], texture[1], 2D;\nMUL a, a, fragment.color;\nMUL result.color.xyz, a, b;\nMOV result.color.w, a.w;\nEND\n",
  "!!ARBfp1.0\nTEMP t;\nTEX t, fragment.texcoord[0], texture[0], 2D;\nKIL t;\nMUL result.color, t, fragment.color;\nEND\n"};
 GLuint ids[10],a,b;int k,v;
 reset();grad();if(!scene_gen_prog||!scene_bind_prog||!scene_prog_str||!scene_active_tex||!scene_mtc2||!scene_has("GL_ARB_fragment_program"))return;
 scene_active_tex(GL_TEXTURE1_ARB);b=scene_tex(4);scene_active_tex(GL_TEXTURE0_ARB);a=scene_tex(0);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 glFogi(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_START,0);glFogf(GL_FOG_END,1);{float fc[4]={0.2f,0.9f,0.3f,1};glFogfv(GL_FOG_COLOR,fc);}
 scene_gen_prog(10,ids);
 for(k=0;k<10;k++) {
  static const float c[4][2]={{0,0},{1,0},{1,1},{0,1}};float x=(float)(6+(k%5)*50),y=(float)(30+(k/5)*110),w=44,h=90;
  scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,ids[k]);scene_prog_str(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(prog[k]),prog[k]);
  glEnable(GL_FRAGMENT_PROGRAM_ARB);if(k==6)glEnable(GL_FOG);
  glBegin(GL_QUADS);
  for(v=0;v<4;v++){glColor4f(0.3f+0.6f*c[v][0],0.9f-0.5f*c[v][1],0.4f+0.5f*c[v][1],0.35f+0.6f*c[v][0]);scene_mtc2(GL_TEXTURE0_ARB,c[v][0]*1.5f,c[v][1]);scene_mtc2(GL_TEXTURE1_ARB,c[v][1],c[v][0]*0.75f);
   glVertex3f(x+c[v][0]*w,y+c[v][1]*h,-0.9f+1.8f*c[v][1]);}
  glEnd();glFlush();glDisable(GL_FOG);glDisable(GL_FRAGMENT_PROGRAM_ARB);
 }
 /* fixed-function fog with the NICEST hint (v10.321: the engine's factors, swrast's coordinates) */
 glHint(GL_FOG_HINT,GL_NICEST);glEnable(GL_FOG);glDisable(GL_TEXTURE_2D);glBegin(GL_QUADS);glColor3f(0.9f,0.2f,0.6f);glVertex3f(6,8,-0.9f);glVertex3f(250,8,-0.9f);glVertex3f(250,24,0.9f);glVertex3f(6,24,0.9f);glEnd();
 glDisable(GL_FOG);glHint(GL_FOG_HINT,GL_DONT_CARE);
 scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,0);glDisable(GL_BLEND);glBlendFunc(GL_ONE,GL_ZERO);
 scene_active_tex(GL_TEXTURE1_ARB);glDeleteTextures(1,&b);scene_active_tex(GL_TEXTURE0_ARB);glDeleteTextures(1,&a);
}
/* v4.4: GL_ATI_fragment_shader as stages: texel x colour blended with a second texel by the colour's alpha; a saturated, doubled difference
 * from a constant */
#ifndef GL_FRAGMENT_SHADER_ATI
#define GL_FRAGMENT_SHADER_ATI 0x8920
#define GL_REG_0_ATI 0x8921
#define GL_REG_1_ATI 0x8922
#define GL_CON_0_ATI 0x8941
#define GL_MOV_ATI 0x8961
#define GL_ADD_ATI 0x8963
#define GL_MUL_ATI 0x8964
#define GL_SUB_ATI 0x8965
#define GL_LERP_ATI 0x8969
#define GL_SWIZZLE_STR_ATI 0x8976
#define GL_2X_BIT_ATI 0x00000001
#define GL_SATURATE_BIT_ATI 0x00000040
#endif
#ifndef GL_PRIMARY_COLOR_EXT
#define GL_PRIMARY_COLOR_EXT 0x8577
#endif
static void s_atishader(void) {
 GLuint id,a,b;int k,v;static const float c[4][2]={{0,0},{1,0},{1,1},{0,1}},kc[4]={0.25f,0.5f,0.1f,0.3f};
 reset();grad();if(!scene_ati_gen||!scene_ati_bind||!scene_ati_begin||!scene_ati_end||!scene_ati_smap||!scene_ati_cop2||!scene_ati_cop3||!scene_ati_aop2||!scene_ati_const||!scene_active_tex||!scene_mtc2||!scene_has("GL_ATI_fragment_shader"))return;
 scene_active_tex(GL_TEXTURE1_ARB);b=scene_tex(4);glEnable(GL_TEXTURE_2D);scene_active_tex(GL_TEXTURE0_ARB);a=scene_tex(0);glEnable(GL_TEXTURE_2D);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 id=scene_ati_gen(2);
 scene_ati_bind(id);scene_ati_begin();
 scene_ati_smap(GL_REG_0_ATI,GL_TEXTURE0_ARB,GL_SWIZZLE_STR_ATI);scene_ati_smap(GL_REG_1_ATI,GL_TEXTURE1_ARB,GL_SWIZZLE_STR_ATI);
 scene_ati_cop2(GL_MUL_ATI,GL_REG_0_ATI,GL_NONE,GL_NONE,GL_REG_0_ATI,GL_NONE,GL_NONE,GL_PRIMARY_COLOR_EXT,GL_NONE,GL_NONE);
 scene_ati_aop2(GL_MUL_ATI,GL_REG_0_ATI,GL_NONE,GL_REG_0_ATI,GL_NONE,GL_NONE,GL_PRIMARY_COLOR_EXT,GL_NONE,GL_NONE);
 scene_ati_cop3(GL_LERP_ATI,GL_REG_0_ATI,GL_NONE,GL_NONE,GL_PRIMARY_COLOR_EXT,GL_ALPHA,GL_NONE,GL_REG_0_ATI,GL_NONE,GL_NONE,GL_REG_1_ATI,GL_NONE,GL_NONE);
 scene_ati_end();
 scene_ati_bind(id+1);scene_ati_begin();
 scene_ati_smap(GL_REG_0_ATI,GL_TEXTURE0_ARB,GL_SWIZZLE_STR_ATI);scene_ati_const(GL_CON_0_ATI,kc);
 scene_ati_cop2(GL_SUB_ATI,GL_REG_0_ATI,GL_NONE,GL_2X_BIT_ATI|GL_SATURATE_BIT_ATI,GL_REG_0_ATI,GL_NONE,GL_NONE,GL_CON_0_ATI,GL_NONE,GL_NONE);
 scene_ati_aop2(GL_MUL_ATI,GL_REG_0_ATI,GL_NONE,GL_PRIMARY_COLOR_EXT,GL_NONE,GL_NONE,GL_CON_0_ATI,GL_NONE,GL_NONE);
 scene_ati_end();
 for(k=0;k<2;k++) {
  float x=(float)(20+k*120),y=40,w=100,h=170;
  scene_ati_bind(id+k);glEnable(GL_FRAGMENT_SHADER_ATI);
  glBegin(GL_QUADS);
  for(v=0;v<4;v++){glColor4f(0.3f+0.6f*c[v][0],0.9f-0.5f*c[v][1],0.4f+0.5f*c[v][1],0.2f+0.7f*c[v][0]);scene_mtc2(GL_TEXTURE0_ARB,c[v][0]*2,c[v][1]);scene_mtc2(GL_TEXTURE1_ARB,c[v][1],c[v][0]);glVertex2f(x+c[v][0]*w,y+c[v][1]*h);}
  glEnd();glFlush();glDisable(GL_FRAGMENT_SHADER_ATI);
 }
 glDisable(GL_BLEND);glBlendFunc(GL_ONE,GL_ZERO);
 scene_active_tex(GL_TEXTURE1_ARB);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&b);scene_active_tex(GL_TEXTURE0_ARB);glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&a);
}
/* v4.4: GLSL fragment shaders (Mesa compiles them to the same instructions) as stages */
static void s_glsl(void) {
 static const char *const fs[]={
  "uniform sampler2D t0;\nvoid main(){gl_FragColor=texture2D(t0,gl_TexCoord[0].xy)*gl_Color;}\n",
  "uniform sampler2D t0,t1;uniform vec4 k;\nvoid main(){vec4 a=texture2D(t0,gl_TexCoord[0].xy);vec4 b=texture2D(t1,gl_TexCoord[1].xy);gl_FragColor=mix(b,a*k,gl_Color.a);}\n"};
 GLuint a,b;int k,v;static const float c[4][2]={{0,0},{1,0},{1,1},{0,1}};
 reset();grad();if(!scene_glsl_create||!scene_glsl_prog||!scene_glsl_src||!scene_glsl_compile||!scene_glsl_link||!scene_glsl_use||!scene_glsl_attach||!scene_glsl_loc||!scene_glsl_u1i||!scene_glsl_u4f||!scene_active_tex||!scene_mtc2)return;
 scene_active_tex(GL_TEXTURE1_ARB);b=scene_tex(4);scene_active_tex(GL_TEXTURE0_ARB);a=scene_tex(0);
 for(k=0;k<2;k++) {
  GLuint sh=scene_glsl_create(0x8B30),pr=scene_glsl_prog();float x=(float)(20+k*120),y=40,w=100,h=170; /* GL_FRAGMENT_SHADER */
  scene_glsl_src(sh,1,(const char**)&fs[k],0);scene_glsl_compile(sh);scene_glsl_attach(pr,sh);scene_glsl_link(pr);scene_glsl_use(pr);
  scene_glsl_u1i(scene_glsl_loc(pr,"t0"),0);if(k){scene_glsl_u1i(scene_glsl_loc(pr,"t1"),1);scene_glsl_u4f(scene_glsl_loc(pr,"k"),0.5f,0.75f,1.0f,1.0f);}
  glBegin(GL_QUADS);
  for(v=0;v<4;v++){glColor4f(0.3f+0.6f*c[v][0],0.9f-0.5f*c[v][1],0.4f+0.5f*c[v][1],0.2f+0.7f*c[v][0]);scene_mtc2(GL_TEXTURE0_ARB,c[v][0]*2,c[v][1]);scene_mtc2(GL_TEXTURE1_ARB,c[v][1],c[v][0]);glVertex2f(x+c[v][0]*w,y+c[v][1]*h);}
  glEnd();glFlush();scene_glsl_use(0);
 }
 scene_active_tex(GL_TEXTURE1_ARB);glDeleteTextures(1,&b);scene_active_tex(GL_TEXTURE0_ARB);glDeleteTextures(1,&a);
}
/* v4.4: non-power-of-two textures that repeat, mirror or have a mip chain (the sampler's own NPOT mode) */
#ifndef GL_MIRRORED_REPEAT
#define GL_MIRRORED_REPEAT 0x8370
#endif
#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif
static void s_npot2(void) {
 GLuint id;
 reset();grad();glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glEnable(GL_TEXTURE_2D);
 id=scene_img(GL_TEXTURE_2D,24,20,GL_RGBA);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
 quad_tc(8,8,110,100,-0.5f,-0.25f,2.5f,2.0f);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);quad_tc(130,8,110,100,-0.7f,-0.3f,1.6f,2.1f);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_MIRRORED_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);quad_tc(8,120,110,60,-1.0f,0,2.0f,1);
 glDeleteTextures(1,&id);
 /* a 40 x 24 chain (40 20 10 5 2 1 by 24 12 6 3 1 1) drawn shrinking */
 {static GLubyte t[40*24*4];int x,y;glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);
  for(y=0;y<24;y++)for(x=0;x<40;x++){GLubyte *p=t+(y*40+x)*4;p[0]=(GLubyte)(((x/3)&1)*200+40);p[1]=(GLubyte)(((y/2)&1)*180+50);p[2]=(GLubyte)(x*6);p[3]=255;}
  glTexParameteri(GL_TEXTURE_2D,GL_GENERATE_MIPMAP,GL_TRUE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,40,24,0,GL_RGBA,GL_UNSIGNED_BYTE,t);
  quad_tc(130,120,60,60,0,0,1,1);quad_tc(196,120,30,30,0,0,1,1);quad_tc(196,156,14,14,0,0,1,1);quad_tc(130,190,100,40,0,0,8,4);}
 glDeleteTextures(1,&id);glDisable(GL_TEXTURE_2D);
}
/* v4.4: a cube map or a 3D texture with a second unit, on unit 0 or unit 1 (the engine's coordinate sets in three dimensions) */
static GLuint unit3d_cube(int mip) {
 static GLubyte t[16*16*4];GLuint id;int f,s,l,i;
 glGenTextures(1,&id);glBindTexture(GL_TEXTURE_CUBE_MAP_ARB,id);
 for(f=0;f<6;f++)for(s=16,l=0;s>=1;s>>=1,l++){for(i=0;i<s*s;i++){int x=i%s,y=i/s,on=((x>>1)^(y>>1))&1;t[i*4]=(GLubyte)((f*41+l*70+30)&255);t[i*4+1]=(GLubyte)(on?230:50);t[i*4+2]=(GLubyte)((f*83+90)&255);t[i*4+3]=255;}
  glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X_ARB+f,l,GL_RGBA,s,s,0,GL_RGBA,GL_UNSIGNED_BYTE,t);if(!mip)break;}
 glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB,GL_TEXTURE_MIN_FILTER,mip?GL_NEAREST_MIPMAP_NEAREST:GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_CUBE_MAP_ARB,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 return id;
}
static GLuint unit3d_vol(void) {
 static GLubyte t[16*16*8*4];GLuint id;int x,y,z;
 for(z=0;z<8;z++)for(y=0;y<16;y++)for(x=0;x<16;x++){GLubyte *p=t+((z*16+y)*16+x)*4;p[0]=(GLubyte)((z*31+x*9)&255);p[1]=(GLubyte)((y*15+z*7+40)&255);p[2]=(GLubyte)((z*53+(x^y)*5+20)&255);p[3]=255;}
 glGenTextures(1,&id);glBindTexture(GL_TEXTURE_3D,id);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_T,GL_REPEAT);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_WRAP_R,GL_REPEAT);
 glPixelStorei(GL_UNPACK_ALIGNMENT,1);scene_tex3d(GL_TEXTURE_3D,0,GL_RGBA,16,16,8,0,GL_RGBA,GL_UNSIGNED_BYTE,t);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
 return id;
}
static void s_unit3d(void) {
 static const float c[4][2]={{0,0},{1,0},{1,1},{0,1}};GLuint flat,t3;int k,v,u3;
 reset();grad();if(!scene_active_tex||!scene_mtc2||!scene_mtc3||!scene_tex3d||!scene_has("GL_ARB_texture_cube_map"))return;
 for(k=0;k<6;k++) { /* 0, 1: cube on unit 1 (no chain, chain); 2: cube on unit 0; 3: volume on unit 1; 4: volume on unit 0; 5: cube on unit 1 under GL_COMBINE */
  float x=(float)(6+(k%3)*84),y=(float)(10+(k/3)*120),w=76,h=112;GLenum tg=k<3||k==5?GL_TEXTURE_CUBE_MAP_ARB:GL_TEXTURE_3D;
  u3=(k==2||k==4)?0:1;
  scene_active_tex(GL_TEXTURE0_ARB+(1-u3));flat=scene_tex(k==1?1:0);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,u3?GL_REPLACE:GL_MODULATE);
  scene_active_tex(GL_TEXTURE0_ARB+u3);t3=tg==GL_TEXTURE_3D?unit3d_vol():unit3d_cube(k==1);glEnable(tg);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,u3?GL_MODULATE:GL_REPLACE);
  if(k==5&&scene_combine){glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE_ARB);glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB_ARB,GL_ADD_SIGNED_ARB);
   glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB_ARB,GL_PREVIOUS_ARB);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB_ARB,GL_TEXTURE);}
  glBegin(GL_QUADS);
  for(v=0;v<4;v++){float s0=c[v][0],t0=c[v][1];
   scene_mtc2(GL_TEXTURE0_ARB+(1-u3),s0*1.37f,t0); /* (texel edges off the pixel centres) */
   if(tg==GL_TEXTURE_3D)scene_mtc3(GL_TEXTURE0_ARB+u3,s0*1.25f,t0,(t0*7.0f+0.5f)/8.0f);else scene_mtc3(GL_TEXTURE0_ARB+u3,s0-0.5f,t0-0.5f,k==1?0.45f:k==2?-0.7f:-0.6f+0.2f*s0);
   glVertex2f(x+c[v][0]*w,y+c[v][1]*h);}
  glEnd();glFlush();
  if(k==1){glBegin(GL_QUADS);for(v=0;v<4;v++){scene_mtc2(GL_TEXTURE0_ARB,c[v][0],c[v][1]);scene_mtc3(GL_TEXTURE1_ARB,c[v][0]-0.5f,c[v][1]-0.5f,0.45f);glVertex2f(x+10+c[v][0]*12,y+10+c[v][1]*12);}glEnd();glFlush();}
  glDisable(tg);glDeleteTextures(1,&t3);scene_active_tex(GL_TEXTURE0_ARB+(1-u3));glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&flat);
 }
 scene_active_tex(GL_TEXTURE1_ARB);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);scene_active_tex(GL_TEXTURE0_ARB);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
}
/* v4.4 (driver v10.321): render to texture through framebuffer objects (EXT_framebuffer_object). An RGB texture with a depth renderbuffer
 * drawn into (clear, depth test, culling, a texture, blending, a scissored clear) and read back (glReadPixels), then drawn 1:1 into the window;
 * an NPOT RGB texture (sampled padded: its copy made again from the image read back); an RGBA one (Mesa's wrapper, software, then sampled by the
 * engine); glCopyTexSubImage2D from the framebuffer object; glGenerateMipmapEXT after the drawing, minified to level 2; glGetTexImage drawn as
 * pixels; an RGB8 colour renderbuffer read back */
#ifndef GL_FRAMEBUFFER_EXT
#define GL_FRAMEBUFFER_EXT 0x8D40
#define GL_RENDERBUFFER_EXT 0x8D41
#define GL_COLOR_ATTACHMENT0_EXT 0x8CE0
#define GL_DEPTH_ATTACHMENT_EXT 0x8D00
#define GL_FRAMEBUFFER_COMPLETE_EXT 0x8CD5
#endif
#ifndef GL_DEPTH_COMPONENT16
#define GL_DEPTH_COMPONENT16 0x81A5
#endif
typedef void (APIENTRY *fbo_gen_fn)(GLsizei,GLuint*);typedef void (APIENTRY *fbo_bind_fn)(GLenum,GLuint);typedef void (APIENTRY *fbo_tex2d_fn)(GLenum,GLenum,GLenum,GLuint,GLint);
typedef void (APIENTRY *rb_storage_fn)(GLenum,GLenum,GLsizei,GLsizei);typedef void (APIENTRY *fbo_rb_fn)(GLenum,GLenum,GLenum,GLuint);typedef GLenum (APIENTRY *fbo_status_fn)(GLenum);
typedef void (APIENTRY *fbo_del_fn)(GLsizei,const GLuint*);typedef void (APIENTRY *genmip_fn)(GLenum);
static fbo_gen_fn scene_fbo_gen,scene_rb_gen;static fbo_bind_fn scene_fbo_bind,scene_rb_bind;static fbo_tex2d_fn scene_fbo_tex2d;static rb_storage_fn scene_rb_storage;
static fbo_rb_fn scene_fbo_rb;static fbo_status_fn scene_fbo_status;static fbo_del_fn scene_fbo_del,scene_rb_del;static genmip_fn scene_genmip;
static void rtt_view(int w,int h) {glViewport(0,0,w,h);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,w,0,h,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();}
static GLuint rtt_tex(int w,int h,GLenum ifmt) {
 static GLubyte t[128*128*4];GLuint id=0;int x,y;
 for(y=0;y<h;y++)for(x=0;x<w;x++){GLubyte *p=t+(y*w+x)*4;p[0]=(GLubyte)(x*2);p[1]=(GLubyte)(y*2);p[2]=(GLubyte)(((x>>3)^(y>>3))&1?200:40);p[3]=(GLubyte)(128+x);}
 glGenTextures(1,&id);glBindTexture(GL_TEXTURE_2D,id);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glPixelStorei(GL_UNPACK_ALIGNMENT,1);glTexImage2D(GL_TEXTURE_2D,0,ifmt,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,t);glPixelStorei(GL_UNPACK_ALIGNMENT,4);
 return id;
}
static void rtt_draw_inside(int w,int h,GLuint t16,int depth) {
 float fw=(float)w,fh=(float)h;
 glClearColor(0.25f,0.1f,0.4f,1);glClear(GL_COLOR_BUFFER_BIT|(depth?GL_DEPTH_BUFFER_BIT:0));
 if(depth)glEnable(GL_DEPTH_TEST);
 glBegin(GL_TRIANGLES);
 glColor3f(1,0,0);glVertex3f(fw*0.05f,fh*0.1f,0.5f);glColor3f(1,1,0);glVertex3f(fw*0.9f,fh*0.2f,0.5f);glColor3f(1,0,1);glVertex3f(fw*0.3f,fh*0.95f,0.5f);
 glColor3f(0,1,0);glVertex3f(fw*0.95f,fh*0.05f,-0.8f);glColor3f(0,1,1);glVertex3f(fw*0.8f,fh*0.9f,0.9f);glColor3f(0,0.5f,1);glVertex3f(fw*0.1f,fh*0.6f,0.0f);
 glEnd();
 if(depth)glDisable(GL_DEPTH_TEST);
 glEnable(GL_CULL_FACE);glCullFace(GL_BACK);
 tri(fw*0.6f,fh*0.6f,fw*0.95f,fh*0.6f,fw*0.78f,fh*0.95f,0xFFFFFF,0x8080FF,0xFF8080); /* counter-clockwise: kept */
 tri(fw*0.05f,fh*0.62f,fw*0.2f,fh*0.95f,fw*0.35f,fh*0.62f,0x00FF00,0x00FF00,0x00FF00); /* clockwise: culled */
 glDisable(GL_CULL_FACE);
 glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,t16);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glColor3f(1,1,1);
 quad_tc(fw*0.5f,fh*0.125f,32,32,0,0,1,1);glDisable(GL_TEXTURE_2D);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glColor4f(1,1,1,0.5f);
 glBegin(GL_QUADS);glVertex2f(fw*0.1f,fh*0.4f);glVertex2f(fw*0.45f,fh*0.4f);glVertex2f(fw*0.45f,fh*0.55f);glVertex2f(fw*0.1f,fh*0.55f);glEnd();glDisable(GL_BLEND);
 glEnable(GL_SCISSOR_TEST);glScissor(w/8,h/16,w/4,h/8);glClearColor(0.9f,0.6f,0.1f,1);glClear(GL_COLOR_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);
 glClearColor(0.1f,0.2f,0.3f,1);
}
static void rtt_show(GLuint t,float x,float y,float w,float h) {
 glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,t);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);quad_tc(x,y,w,h,0,0,1,1);glDisable(GL_TEXTURE_2D);
}
static void s_rtt(void) {
 static GLubyte px[32*32*4],big[128*128*4];GLuint fb=0,fb2=0,rb=0,rbc=0,ta,tb,tc,td,t16;int ok;
 reset();grad();
 if(!scene_fbo_gen||!scene_fbo_bind||!scene_fbo_tex2d||!scene_rb_gen||!scene_rb_bind||!scene_rb_storage||!scene_fbo_rb||!scene_fbo_status||!scene_fbo_del||!scene_rb_del||!scene_genmip||
    !scene_has("GL_EXT_framebuffer_object"))return;
 t16=scene_tex(0);ta=rtt_tex(128,128,GL_RGB);
 scene_fbo_gen(1,&fb);scene_fbo_bind(GL_FRAMEBUFFER_EXT,fb);scene_fbo_tex2d(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,ta,0);
 scene_rb_gen(1,&rb);scene_rb_bind(GL_RENDERBUFFER_EXT,rb);scene_rb_storage(GL_RENDERBUFFER_EXT,GL_DEPTH_COMPONENT16,128,128);
 scene_fbo_rb(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,rb);
 ok=scene_fbo_status(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT;
 if(ok){rtt_view(128,128);rtt_draw_inside(128,128,t16,1);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(4,4,32,32,GL_RGBA,GL_UNSIGNED_BYTE,px);}
 scene_fbo_bind(GL_FRAMEBUFFER_EXT,0);rtt_view(W,H);
 if(ok){rtt_show(ta,8,8,128,128);glRasterPos2i(144,8);glPixelStorei(GL_UNPACK_ALIGNMENT,1);glDrawPixels(32,32,GL_RGBA,GL_UNSIGNED_BYTE,px);}
 /* an NPOT RGB texture, no depth */
 tb=rtt_tex(100,60,GL_RGB);scene_fbo_bind(GL_FRAMEBUFFER_EXT,fb);scene_fbo_rb(GL_FRAMEBUFFER_EXT,GL_DEPTH_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,0);
 scene_fbo_tex2d(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,tb,0);
 if(scene_fbo_status(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT){rtt_view(100,60);rtt_draw_inside(100,60,t16,0);}
 scene_fbo_bind(GL_FRAMEBUFFER_EXT,0);rtt_view(W,H);rtt_show(tb,144,48,100,60);
 /* an RGBA texture: Mesa's wrapper (software), sampled by the engine afterwards */
 tc=rtt_tex(64,64,GL_RGBA);scene_fbo_bind(GL_FRAMEBUFFER_EXT,fb);scene_fbo_tex2d(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,tc,0);
 if(scene_fbo_status(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT){rtt_view(64,64);rtt_draw_inside(64,64,t16,0);}
 scene_fbo_bind(GL_FRAMEBUFFER_EXT,0);rtt_view(W,H);rtt_show(tc,144,116,64,64);
 /* glCopyTexSubImage2D from the framebuffer object (ta attached again) */
 td=rtt_tex(32,32,GL_RGB);scene_fbo_bind(GL_FRAMEBUFFER_EXT,fb);scene_fbo_tex2d(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_TEXTURE_2D,ta,0);
 if(scene_fbo_status(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT){glBindTexture(GL_TEXTURE_2D,td);glCopyTexSubImage2D(GL_TEXTURE_2D,0,0,0,40,20,32,32);}
 scene_fbo_bind(GL_FRAMEBUFFER_EXT,0);rtt_view(W,H);rtt_show(td,8,144,64,64);
 /* glGenerateMipmapEXT after the drawing: a quarter size reads level 2 */
 glBindTexture(GL_TEXTURE_2D,ta);scene_genmip(GL_TEXTURE_2D);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);rtt_show(ta,80,144,32,32);
 /* glGetTexImage of level 0, drawn as pixels (a 48 x 48 part) */
 glBindTexture(GL_TEXTURE_2D,ta);glPixelStorei(GL_PACK_ALIGNMENT,1);glGetTexImage(GL_TEXTURE_2D,0,GL_RGBA,GL_UNSIGNED_BYTE,big);
 glPixelStorei(GL_UNPACK_ROW_LENGTH,128);glPixelStorei(GL_UNPACK_SKIP_PIXELS,60);glPixelStorei(GL_UNPACK_SKIP_ROWS,50);glRasterPos2i(120,190);glDrawPixels(48,48,GL_RGBA,GL_UNSIGNED_BYTE,big);
 glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
 /* an RGB8 colour renderbuffer, read back */
 scene_rb_gen(1,&rbc);scene_rb_bind(GL_RENDERBUFFER_EXT,rbc);scene_rb_storage(GL_RENDERBUFFER_EXT,GL_RGB8,64,64);
 scene_fbo_gen(1,&fb2);scene_fbo_bind(GL_FRAMEBUFFER_EXT,fb2);scene_fbo_rb(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_RENDERBUFFER_EXT,rbc);
 if(scene_fbo_status(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT){rtt_view(64,64);rtt_draw_inside(64,64,t16,0);glReadPixels(16,8,32,32,GL_RGBA,GL_UNSIGNED_BYTE,px);}
 scene_fbo_bind(GL_FRAMEBUFFER_EXT,0);rtt_view(W,H);glRasterPos2i(200,190);glDrawPixels(32,32,GL_RGBA,GL_UNSIGNED_BYTE,px);glPixelStorei(GL_UNPACK_ALIGNMENT,4);glPixelStorei(GL_PACK_ALIGNMENT,4);
 scene_fbo_del(1,&fb2);scene_fbo_del(1,&fb);scene_rb_del(1,&rbc);scene_rb_del(1,&rb);
 glDeleteTextures(1,&td);glDeleteTextures(1,&tc);glDeleteTextures(1,&tb);glDeleteTextures(1,&ta);glDeleteTextures(1,&t16);
 glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
}
/* v4.5 (v10.353): five cases that left software. Each needs the ICD's extensions (Microsoft's 1.1 draws the cleared window only). */
#ifndef GL_MIRROR_CLAMP_TO_EDGE_EXT
#define GL_MIRROR_CLAMP_TO_EDGE_EXT 0x8743
#endif
static void s16_tex(GLuint *t) { /* 16x16 RGBA, alpha 0..255 in steps of 17 across, nearest */
 static GLubyte t16[16*16*4];int i;
 for(i=0;i<256;i++){t16[i*4]=(GLubyte)(40+(i&15)*12);t16[i*4+1]=(GLubyte)(200-(i>>4)*10);t16[i*4+2]=(GLubyte)((i*37)&255);t16[i*4+3]=(GLubyte)((i&15)*17);}
 glGenTextures(1,t);glBindTexture(GL_TEXTURE_2D,*t);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,t16);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
}
static void fp_load(const char *fp) {
 GLuint id=0;scene_gen_prog(1,&id); /* its own name: scene_fp holds sw_on's program */
 scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,id);scene_prog_str(GL_FRAGMENT_PROGRAM_ARB,GL_PROGRAM_FORMAT_ASCII_ARB,(GLsizei)strlen(fp),fp);glEnable(GL_FRAGMENT_PROGRAM_ARB);
}
static void fp_off(void) {glDisable(GL_FRAGMENT_PROGRAM_ARB);scene_bind_prog(GL_FRAGMENT_PROGRAM_ARB,0);}
/* KIL of (texel alpha - 0.5): the engine's alpha test GEQUAL 128. Depth on: a green quad behind shows only where fragments were killed
 * (killed fragments write no depth) */
static void s_kil(void) {
 static const char fp[]="!!ARBfp1.0\nTEMP t,q;\nTEX t, fragment.texcoord[0], texture[0], 2D;\nSUB q, t.wwww, 0.5;\nKIL q;\nMOV result.color, t;\nEND\n";
 GLuint tx;
 reset();if(!scene_gen_prog||!scene_bind_prog||!scene_prog_str||!scene_has("GL_ARB_fragment_program"))return;
 s16_tex(&tx);glEnable(GL_TEXTURE_2D);fp_load(fp);glEnable(GL_DEPTH_TEST);
 glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex3f(8,8,-0.2f);glTexCoord2f(1,0);glVertex3f(W-8,8,-0.2f);glTexCoord2f(1,1);glVertex3f(W-8,H-8,-0.2f);glTexCoord2f(0,1);glVertex3f(8,H-8,-0.2f);glEnd();
 fp_off();glDisable(GL_TEXTURE_2D);glColor3f(0,1,0);
 glBegin(GL_QUADS);glVertex3f(0,0,-0.5f);glVertex3f(W,0,-0.5f);glVertex3f(W,H,-0.5f);glVertex3f(0,H,-0.5f);glEnd();
 glDeleteTextures(1,&tx);
}
/* OPTION ARB_fog_exp2 with GL_FOG off and GL_FOG_MODE LINEAR: the program's mode, at its distances */
static void s_fogopt(void) {
 static const char fp[]="!!ARBfp1.0\nOPTION ARB_fog_exp2;\nMOV result.color, fragment.color;\nEND\n";
 GLfloat fc[4]={0.1f,0.2f,0.9f,1};int j;
 reset();if(!scene_gen_prog||!scene_bind_prog||!scene_prog_str||!scene_has("GL_ARB_fragment_program"))return;
 fp_load(fp);glDisable(GL_FOG);glFogi(GL_FOG_MODE,GL_LINEAR);glFogf(GL_FOG_DENSITY,1.5f);glFogf(GL_FOG_START,0);glFogf(GL_FOG_END,1);glFogfv(GL_FOG_COLOR,fc);
 for(j=0;j<5;j++){GLfloat z=-(0.05f+j*0.2f),x=(GLfloat)(8+j*48);
  glBegin(GL_TRIANGLES);glColor3f(1,0.8f,0.2f);glVertex3f(x,10,z);glColor3f(0.9f,0.3f,0.5f);glVertex3f(x+44,10,z);glColor3f(0.4f,1,0.4f);glVertex3f(x+22,240,z);glEnd();}
 /* white bars, each at one distance (the engine's fog is per vertex, swrast's per pixel: across one primitive EXP2's curve and a straight
  * line between its ends part, as GL allows) */
 for(j=0;j<8;j++){GLfloat z=-(0.02f+j*0.13f),y=(GLfloat)(200+j*6);
  glBegin(GL_QUADS);glColor3f(1,1,1);glVertex3f(8,y,z);glVertex3f(248,y,z);glVertex3f(248,y+5,z);glVertex3f(8,y+5,z);glEnd();}
 fp_off();glFogi(GL_FOG_MODE,GL_EXP);glFogf(GL_FOG_DENSITY,1);
}
/* smooth points with the alpha test GEQUAL 0.5: alphas 0.8 pass, 0.3 fail; blended, two sizes, attenuation off */
static void s_aapoint_atest(void) {
 int j;
 reset();glEnable(GL_POINT_SMOOTH);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GEQUAL,0.5f);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 for(j=0;j<48;j++){glPointSize(j&1?9.0f:13.0f);glBegin(GL_POINTS);glColor4f(0.2f+(j%5)*0.15f,0.9f,0.3f,j%3?0.8f:0.3f);glVertex2f(14.0f+(j%8)*30.5f,16.0f+(j/8)*40.3f);glEnd();}
 /* a line of points in one draw */
 glPointSize(5);glBegin(GL_POINTS);for(j=0;j<40;j++){glColor4f(1,0.5f,0.1f,j&1?0.9f:0.2f);glVertex2f(8.25f+j*6,250.5f);}glEnd();
 glDisable(GL_POINT_SMOOTH);glDisable(GL_ALPHA_TEST);glAlphaFunc(GL_ALWAYS,0);glDisable(GL_BLEND);glBlendFunc(GL_ONE,GL_ZERO);glPointSize(1);
}
static void mc_tex(GLuint *tx,int mip) { /* 8x8 to 1x1, each level its own colours */
 static GLubyte t8[4][8*8*4];int l,i;
 for(l=0;l<4;l++){int n=8>>l;for(i=0;i<n*n;i++){t8[l][i*4]=(GLubyte)(l*60+(i%n)*25);t8[l][i*4+1]=(GLubyte)((i/n)*30);t8[l][i*4+2]=(GLubyte)(200-l*40);t8[l][i*4+3]=255;}}
 glGenTextures(1,tx);glBindTexture(GL_TEXTURE_2D,*tx);for(l=0;l<4;l++)glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA,8>>l,8>>l,0,GL_RGBA,GL_UNSIGNED_BYTE,t8[l]);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_MIRROR_CLAMP_TO_EDGE_EXT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_MIRROR_CLAMP_TO_EDGE_EXT);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,mip?GL_LINEAR_MIPMAP_NEAREST:GL_LINEAR);
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
}
static void mc_quad(float x,float y,float w,float h,float s) {
 glBegin(GL_QUADS);glTexCoord2f(-s,-s);glVertex2f(x,y);glTexCoord2f(s,-s);glVertex2f(x+w,y);glTexCoord2f(s,s);glVertex2f(x+w,y+h);glTexCoord2f(-s,s);glVertex2f(x,y+h);glEnd();
}
/* MIRROR_CLAMP_TO_EDGE magnified: s, t from -2 to 2 (the mirror in the middle, the edge texels beyond 1), then a perspective quad */
static void s_mclamp(void) {
 GLuint tx;
 reset();if(!scene_has("GL_EXT_texture_mirror_clamp")&&!scene_has("GL_ATI_texture_mirror_once"))return;
 mc_tex(&tx,0);mc_quad(4,4,160,160,2);
 glBegin(GL_QUADS);glTexCoord4f(-1.5f,-1.5f,0,1);glVertex2f(170,8);glTexCoord4f(1.5f,-1.5f,0,1);glVertex2f(250,8);glTexCoord4f(0.6f,0.6f,0,0.4f);glVertex2f(230,250);glTexCoord4f(-0.6f,0.6f,0,0.4f);glVertex2f(190,250);glEnd();
 glDisable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glDeleteTextures(1,&tx);
}
/* MIRROR_CLAMP_TO_EDGE mipmapped: the same chain minified at four sizes */
static void s_mclampmip(void) {
 GLuint tx;int j;
 reset();if(!scene_has("GL_EXT_texture_mirror_clamp")&&!scene_has("GL_ATI_texture_mirror_once"))return;
 mc_tex(&tx,1);
 for(j=0;j<6;j++){float sz=(float)(6+j*7);mc_quad((float)(4+j*42),10,sz,sz,2);mc_quad((float)(4+j*42),120,sz,sz*2,3);}
 glDisable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glDeleteTextures(1,&tx);
}
/* v4.6 (v10.357): GL_CLAMP filtered linearly (exact by default): coordinates in 0..1 on the engine (a ring of the border colour), a quad
 * whose coordinates leave 0..1 in software (GL stays at half the border there); NPOT and power-of-two, a 1D texture */
static void s_glclamp(void) {
 static GLubyte t[8*8*4],n[6*5*4];int i;GLuint a,b;GLfloat bc[4]={0.1f,0.2f,0.9f,1};
 reset();
 for(i=0;i<64;i++){t[i*4]=(GLubyte)((i%8)*32);t[i*4+1]=(GLubyte)((i/8)*32);t[i*4+2]=(GLubyte)(i&1?200:40);t[i*4+3]=255;}
 for(i=0;i<30;i++){n[i*4]=(GLubyte)(220-(i%6)*30);n[i*4+1]=(GLubyte)((i/6)*50);n[i*4+2]=90;n[i*4+3]=255;}
 glGenTextures(1,&a);glBindTexture(GL_TEXTURE_2D,a);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,t);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,bc);
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 quad_tc(8,8,112,112,0,0,1,1);           /* in 0..1: the engine */
 quad_tc(136,8,112,112,-0.5f,-0.5f,1.5f,1.5f); /* out of 0..1: software */
 glGenTextures(1,&b);glBindTexture(GL_TEXTURE_2D,b);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,6,5,0,GL_RGBA,GL_UNSIGNED_BYTE,n); /* NPOT */
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);glTexParameterfv(GL_TEXTURE_2D,GL_TEXTURE_BORDER_COLOR,bc);
 quad_tc(8,136,112,112,0,0,1,1);
 glDisable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glDeleteTextures(1,&a);glDeleteTextures(1,&b);
}
static const struct {const char *name;scene_fn f;int info;} scenes[]={
 {"clear",s_clear},{"smooth triangles, quads, strip, fan",s_smooth},{"depth test",s_depth},{"blending",s_blend},{"culling",s_cull},
 {"flat shading",s_flat},{"lighting",s_light},{"fog",s_fog},{"lines and points",s_lines},{"scissor",s_scissor},
 {"unfilled polygons (lines, points)",s_unfilled},{"polygon offset",s_offset},{"stencil A: software 3 left, EQUAL 3",s_stA},{"stencil B: engine 1 right, EQUAL 1",s_stB},{"stencil C: software 3 left, EQUAL 3",s_stC},
 {"stencil D: software 3 left, EQUAL 1",s_stD},{"stencil E: software 3 right, EQUAL 3",s_stE},
 {"depth by software, tested by the engine",s_depth_sw},{"depth by the engine, tested by software",s_depth_hw},
 {"multipass: GL_EQUAL, engine and software",s_depth_equal,1},{"alpha test",s_alphatest},{"line stipple and a wide line",s_stipple},
 {"DrawPixels, CopyPixels",s_pixels},{"logic ops",s_logicop},{"colour masks",s_colormask},{"wide lines and large points",s_wide},
 {"textured points and wide lines",s_texwide},{"texture environment BLEND",s_blendenv},{"q in the texture coordinates",s_qvertex},
 {"textured one-pixel lines",s_texlines},{"line stipple: lines, strip, loop, wide",s_linestip},{"point sprites, attenuated points",s_sprites},
 {"blend equations, constant colour",s_blendop},{"combine scaled, interpolate by a colour",s_combscale},{"texture border colour",s_border},
 {"512 mip chain, MAX_LEVEL 2",s_longmip},{"mipmaps on unit 1",s_unitmip},
 {"three texture units",s_units3},{"S3TC textures",s_s3tc},{"polygon stipple",s_pstipple},{"LOD clamps (MIN_LOD, MAX_LOD)",s_lodclamp},
 {"non-power-of-two, rectangle textures",s_npot},{"CLAMP_TO_BORDER filtered linearly",s_border2},{"FXT1, RGB_DXT1 under GL_COMBINE",s_fxt1},
 {"colour tables, DEPTH_COMPONENT textures",s_ctable},{"MIN_LOD above 0, MAX_LOD between levels",s_lodclamp2},{"GL_DOT3_RGB",s_dot3rgb},
 {"cube map mip chain",s_cubemip},{"3D texture",s_tex3d},{"point sizes from a vertex program",s_vpsize},{"antialiased points, lines, polygons",s_aa},{"fragment programs as stages",s_fprog},{"ATI fragment shaders as stages",s_atishader},{"GLSL fragment shaders as stages",s_glsl},{"NPOT: repeat, mirrored, mip chain",s_npot2},{"cube map, 3D texture with a second unit",s_unit3d},{"render to texture (framebuffer objects)",s_rtt},
 {"KIL as the alpha test",s_kil},{"fog option, GL_FOG off",s_fogopt},{"smooth points, alpha test",s_aapoint_atest},
 {"MIRROR_CLAMP_TO_EDGE magnified",s_mclamp},{"MIRROR_CLAMP_TO_EDGE mipmapped",s_mclampmip},
 {"GL_CLAMP filtered linearly",s_glclamp},
 {"speed: lit sphere, 12288 triangles",s_speed}};
#define NS ((int)(sizeof(scenes)/sizeof(scenes[0])))

