/* VXSTEN v2.0 (driver .533: the Coarse Z probe and the probe variants of the removed switch words gone). VXSTEN v1.1 (driver v10.266): an OpenGL stencil test with the answers worked out in advance. Every case draws in a 256x256 window
 * (pixel coordinates: glOrtho(0,256,0,256,-1,1), larger z nearer) and reads the stencil and colour back with glReadPixels at points
 * and regions whose value is known exactly (cell centres and whole rectangles on integer edges, so no rasterization rule decides).
 * Cases: clears (and a masked clear), the eight stencil operations on four starting values, the fail / depth-fail / pass paths, the
 * eight compare functions against three references, value and write masks, two-sided stencil (glStencilFuncSeparate/OpSeparate and
 * EXT_stencil_two_side, both windings, glFrontFace(GL_CW), a line after a back face), culling both faces (triangles dropped, a line
 * and a point drawn), and shadow volumes (z-pass and z-fail, two passes and one two-sided pass, the floor between, in front of and
 * behind the box). Run three times: the VX855 ICD with the engine, the ICD with the engine off (ACCEL_OPENGL_HW = 0 for that context,
 * put back afterwards), and Microsoft's software OpenGL as a reference (it has no two-sided stencil; its failures are not counted).
 * Then the engine's stencil and colour images are compared with the engine-off ones, case by case, and a two-sided shadow volume pass
 * is timed. "VXSTEN n" times n passes (default 200). Log: VXSTEN.TXT; an engine case that differs from the engine-off one writes
 * VXSTEN_nn_HW.BMP and VXSTEN_nn_SW.BMP (stencil values as grey). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <GL/gl.h>

#define W 256
#define H 256
#ifndef GL_INCR_WRAP
#define GL_INCR_WRAP 0x8507
#define GL_DECR_WRAP 0x8508
#endif
#define GL_STENCIL_TEST_TWO_SIDE_EXT 0x8910
typedef void (APIENTRY *sfs_fn)(GLenum,GLenum,GLint,GLuint);
typedef void (APIENTRY *sos_fn)(GLenum,GLenum,GLenum,GLenum);
typedef void (APIENTRY *asf_fn)(GLenum);
static sfs_fn pFuncSep;static sos_fn pOpSep;static asf_fn pActiveFace;static int have_wrap;
static FILE *logf;
static int which,failures,case_fail,frames=200;
static HDC gdc;
#define NC 16
static unsigned char simg[3][NC][W*H],cimg[3][NC][W*H*4];static int shave[3][NC];
static double tms[3];
/* lines and points are drawn by the chip's rule with the engine and Mesa's without: their pixels are judged by the cases' own checks
 * (want_st, want_col) and left out of the engine against engine-off image comparison. lined[k]: case k drew the line at y 200.5 and
 * the point at 50.5,230.5 */
static int curk,lined[NC];
static int raster_only(int k,int i){int x=i%W,y=i/W;return lined[k]&&((y>=199&&y<=201)||(x>=48&&x<=52&&y>=228&&y<=232));}
static void differ(int k,int a,int b,int *ds,int *dc,int *at) {
 int i;*ds=*dc=0;*at=-1;
 for(i=0;i<W*H;i++){int c,o=0;if(raster_only(k,i))continue;
  if(simg[a][k][i]!=simg[b][k][i]){if(*at<0)*at=i;(*ds)++;}
  for(c=0;c<3;c++){int p=cimg[a][k][i*4+c],q=cimg[b][k][i*4+c];if(p-q>16||q-p>16)o=1;}
  if(o){if(*at<0)*at=i;(*dc)++;}}
}

#ifdef VXSTEN_HOST /* the host test (gl/host/vxgl_host.c) runs the cases against Mesa with the engine model and without it */
#define SwapBuffers(d) ((void)(d))
DWORD GetTickCount(void);
#else
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcA(h,m,w,l);}
#endif
static const char *opname(GLenum o){switch(o){case GL_KEEP:return "KEEP";case GL_ZERO:return "ZERO";case GL_REPLACE:return "REPLACE";case GL_INCR:return "INCR";case GL_DECR:return "DECR";case GL_INVERT:return "INVERT";case GL_INCR_WRAP:return "INCR_WRAP";case GL_DECR_WRAP:return "DECR_WRAP";}return "?";}
static const char *fnname(GLenum f){static const char *n[8]={"NEVER","LESS","EQUAL","LEQUAL","GREATER","NOTEQUAL","GEQUAL","ALWAYS"};return f>=GL_NEVER&&f<=GL_ALWAYS?n[f-GL_NEVER]:"?";}

/* ------------------------------------------------------------------ helpers */
/* v1.4 (v10.292): the fresh-context probe's variants. probe_sub: only cases 0, 1, 2 and 7, no probe; probe_clear: depth and stencil
 * cleared by an engine draw (depth ALWAYS, stencil REPLACE 0, colour masked) instead of glClear; probe_warm: one depth- and stencil-tested
 * draw over the window after each clear */
static int probe_sub,probe_clear,probe_warm;
static void rect(int x0,int y0,int x1,int y1,float z,int cw);
static void reset(void) {
 glViewport(0,0,W,H);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,0,H,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_DITHER);glDisable(GL_ALPHA_TEST);
 if(pActiveFace){glDisable(GL_STENCIL_TEST_TWO_SIDE_EXT);pActiveFace(GL_FRONT);}
 glFrontFace(GL_CCW);glCullFace(GL_BACK);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);glColorMask(1,1,1,1);
 glStencilMask(0xFF);glStencilFunc(GL_ALWAYS,0,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
 if(pFuncSep){pFuncSep(GL_FRONT_AND_BACK,GL_ALWAYS,0,0xFF);pOpSep(GL_FRONT_AND_BACK,GL_KEEP,GL_KEEP,GL_KEEP);}
 glClearColor(0,0,0,1);glClearDepth(1.0);glClearStencil(0);glClear(GL_COLOR_BUFFER_BIT|(probe_clear?0:GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT));
 glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);glColor3f(1,1,1);
 if(probe_clear||probe_warm) {
  glEnable(GL_DEPTH_TEST);glEnable(GL_STENCIL_TEST);glColorMask(0,0,0,0);
  if(probe_clear){glDepthFunc(GL_ALWAYS);glStencilFunc(GL_ALWAYS,0,0xFF);glStencilOp(GL_REPLACE,GL_REPLACE,GL_REPLACE);rect(0,0,W,H,-0.99999f,0);}
  if(probe_warm){glDepthFunc(GL_LESS);glDepthMask(GL_FALSE);glStencilFunc(GL_EQUAL,0x55,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);rect(0,0,W,H,0.5f,0);glDepthMask(GL_TRUE);}
  glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glColorMask(1,1,1,1);glDepthFunc(GL_LESS);glStencilFunc(GL_ALWAYS,0,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);
 }
}
/* a rectangle on integer edges (pixels x0..x1-1, y0..y1-1), counter-clockwise (front facing) unless cw */
static void rect(int x0,int y0,int x1,int y1,float z,int cw) {
 glBegin(GL_QUADS);
 if(!cw){glVertex3f((float)x0,(float)y0,z);glVertex3f((float)x1,(float)y0,z);glVertex3f((float)x1,(float)y1,z);glVertex3f((float)x0,(float)y1,z);}
 else {glVertex3f((float)x0,(float)y0,z);glVertex3f((float)x0,(float)y1,z);glVertex3f((float)x1,(float)y1,z);glVertex3f((float)x1,(float)y0,z);}
 glEnd();
}
static unsigned st_at(int x,int y) {GLubyte v=0;glReadPixels(x,y,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&v);return v;}
static void col_at(int x,int y,GLubyte *p) {glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);}
static void bad(const char *fmt,...) {va_list a;case_fail++;fprintf(logf,"     FAIL ");va_start(a,fmt);vfprintf(logf,fmt,a);va_end(a);fprintf(logf,"\n");}
static void want_st(int x,int y,unsigned want,const char *what) {unsigned v=st_at(x,y);if(v!=want)bad("%s: stencil at %d,%d = %02X, want %02X",what,x,y,v,want);}
/* colour: 'G' green (passed), 'K' black (not drawn), 'W' white, 'S' the shadow grey, 'F' the floor grey */
static void want_col(int x,int y,char want,const char *what) {
 GLubyte p[4];int ok;col_at(x,y,p);
 if(want=='G')ok=p[1]>200&&p[0]<60&&p[2]<60;else if(want=='K')ok=p[0]<40&&p[1]<40&&p[2]<40;else if(want=='W')ok=p[0]>200&&p[1]>200&&p[2]>200;
 else if(want=='S')ok=p[0]>20&&p[0]<90&&p[1]>20&&p[1]<90;else ok=p[0]>150&&p[0]<220;
 if(!ok)bad("%s: colour at %d,%d = %02X%02X%02X, want %s",what,x,y,p[0],p[1],p[2],want=='G'?"green":want=='K'?"black":want=='W'?"white":want=='S'?"shadow":"floor");
}
static unsigned op_apply(GLenum op,unsigned s,unsigned ref,unsigned wm) {
 unsigned n;
 switch(op){case GL_KEEP:n=s;break;case GL_ZERO:n=0;break;case GL_REPLACE:n=ref;break;case GL_INCR:n=s<255?s+1:255;break;case GL_DECR:n=s?s-1:0;break;
  case GL_INVERT:n=~s;break;case GL_INCR_WRAP:n=s+1;break;case GL_DECR_WRAP:n=s-1;break;default:n=s;}
 return ((n&wm)|(s&~wm))&255;
}
static int fn_pass(GLenum f,unsigned ref,unsigned s,unsigned m) {
 ref&=m;s&=m;
 switch(f){case GL_NEVER:return 0;case GL_LESS:return ref<s;case GL_LEQUAL:return ref<=s;case GL_GREATER:return ref>s;case GL_GEQUAL:return ref>=s;case GL_EQUAL:return ref==s;case GL_NOTEQUAL:return ref!=s;}
 return 1;
}

/* ------------------------------------------------------------------ cases */
static void c_clear(void) {
 static const unsigned v[3]={0,0x5A,0xFF};int k;char t[64];
 for(k=0;k<3;k++){glClearStencil(v[k]);glClear(GL_STENCIL_BUFFER_BIT);sprintf(t,"clear %02X",v[k]);want_st(5,5,v[k],t);want_st(128,128,v[k],t);want_st(250,250,v[k],t);}
 glStencilMask(0x0F);glClearStencil(0x00);glClear(GL_STENCIL_BUFFER_BIT);glStencilMask(0xFF); /* the masked clear: FF -> F0 */
 want_st(5,5,0xF0,"clear 00 with mask 0F over FF");want_st(200,100,0xF0,"clear 00 with mask 0F over FF");
}
static const GLenum ops[8]={GL_KEEP,GL_ZERO,GL_REPLACE,GL_INCR,GL_DECR,GL_INVERT,GL_INCR_WRAP,GL_DECR_WRAP};
static void c_ops(void) {
 static const unsigned init[4]={0,1,0x80,0xFF};int r,i;char t[64];
 glEnable(GL_STENCIL_TEST);glColorMask(0,0,0,0);
 for(r=0;r<4;r++){glStencilFunc(GL_ALWAYS,init[r],0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);rect(0,4+r*31,W,4+r*31+28,0,0);} /* each row's starting value */
 for(r=0;r<4;r++)for(i=0;i<8;i++){if(!have_wrap&&i>=6)continue;glStencilFunc(GL_ALWAYS,0x37,0xFF);glStencilOp(GL_KEEP,GL_KEEP,ops[i]);rect(4+i*31,4+r*31,4+i*31+28,4+r*31+28,0,0);}
 glColorMask(1,1,1,1);
 for(r=0;r<4;r++)for(i=0;i<8;i++){if(!have_wrap&&i>=6)continue;sprintf(t,"%s on %02X (ref 37)",opname(ops[i]),init[r]);want_st(18+i*31,18+r*31,op_apply(ops[i],init[r],0x37,0xFF),t);}
}
static int paths_quiet,paths_draw_clear,paths_retire;
static void c_paths(void) {
 if(paths_draw_clear){glEnable(GL_STENCIL_TEST);glColorMask(0,0,0,0);glStencilFunc(GL_ALWAYS,0x10,0xFF);glStencilOp(GL_REPLACE,GL_REPLACE,GL_REPLACE);rect(0,0,W,H,0,0);glColorMask(1,1,1,1);}
 else {glClearStencil(0x10);glClear(GL_STENCIL_BUFFER_BIT);glEnable(GL_STENCIL_TEST);}
 /* stencil fail: NEVER, fail op INCR */
 glColorMask(0,0,0,0);glStencilFunc(GL_NEVER,0,0xFF);glStencilOp(GL_INCR,GL_KEEP,GL_KEEP);rect(10,10,60,60,0,0);
 /* depth fail: a near rectangle writes depth, then a far one with zfail INCR over the left half of it and beyond */
 glEnable(GL_DEPTH_TEST);glStencilFunc(GL_ALWAYS,0,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);rect(100,10,160,60,0.5f,0);
 if(paths_retire){glFinish();(void)st_at(0,0);} /* probe: the engine finished and the planes read between the near and the far rectangle */
 glStencilOp(GL_KEEP,GL_INCR,GL_DECR);rect(70,10,130,60,-0.5f,0); /* 70..99 depth passes (DECR), 100..129 fails (INCR) */
 glColorMask(1,1,1,1);
 if(!paths_quiet){want_st(35,35,0x11,"stencil fail (NEVER, INCR)");want_st(115,35,0x11,"depth fail (INCR)");want_st(85,35,0x0F,"depth pass (DECR)");want_st(145,35,0x10,"untouched");}
 /* depth test off: every fragment passes the depth test */
 glDisable(GL_DEPTH_TEST);glStencilOp(GL_KEEP,GL_INCR,GL_ZERO);rect(100,100,160,160,-0.5f,0);if(!paths_quiet)want_st(130,130,0x00,"depth test off: the pass op");
}
static void c_funcs(void) {
 static const GLenum fs[8]={GL_NEVER,GL_LESS,GL_LEQUAL,GL_GREATER,GL_GEQUAL,GL_EQUAL,GL_NOTEQUAL,GL_ALWAYS};
 static const unsigned refs[3]={0x30,0x40,0x50};int r,i;char t[64];
 glClearStencil(0x40);glClear(GL_STENCIL_BUFFER_BIT);glEnable(GL_STENCIL_TEST);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);glColor3f(0,1,0);
 for(r=0;r<3;r++)for(i=0;i<8;i++){glStencilFunc(fs[i],refs[r],0xFF);rect(4+i*31,4+r*31,4+i*31+28,4+r*31+28,0,0);}
 for(r=0;r<3;r++)for(i=0;i<8;i++){sprintf(t,"%s ref %02X against 40",fnname(fs[i]),refs[r]);want_col(18+i*31,18+r*31,fn_pass(fs[i],refs[r],0x40,0xFF)?'G':'K',t);want_st(18+i*31,18+r*31,0x40,t);}
}
static void c_masks(void) {
 glEnable(GL_STENCIL_TEST);glColor3f(0,1,0);
 glClearStencil(0x45);glClear(GL_STENCIL_BUFFER_BIT);
 glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);glStencilFunc(GL_EQUAL,0x4A,0xF0);rect(10,10,60,60,0,0);glStencilFunc(GL_EQUAL,0x4A,0xFF);rect(70,10,120,60,0,0);
 want_col(35,35,'G',"EQUAL 4A mask F0 against 45 (passes)");want_col(95,35,'K',"EQUAL 4A mask FF against 45 (fails)");
 glClearStencil(0xA5);glClear(GL_STENCIL_BUFFER_BIT);glColorMask(0,0,0,0);glStencilFunc(GL_ALWAYS,0x3C,0xFF);
 glStencilMask(0x0F);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);rect(10,80,60,130,0,0);
 glStencilMask(0xF0);glStencilOp(GL_KEEP,GL_KEEP,GL_INVERT);rect(70,80,120,130,0,0);
 glStencilMask(0x0F);glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);rect(130,80,180,130,0,0);rect(130,80,180,130,0,0); /* A5 -> A6 -> A7 */
 glStencilMask(0xFF);glColorMask(1,1,1,1);
 want_st(35,105,op_apply(GL_REPLACE,0xA5,0x3C,0x0F),"REPLACE 3C write mask 0F on A5");want_st(95,105,op_apply(GL_INVERT,0xA5,0x3C,0xF0),"INVERT write mask F0 on A5");
 want_st(155,105,0xA7,"INCR twice write mask 0F on A5");want_st(200,105,0xA5,"untouched");
}
static void tri_ccw(int x0,int y0) {glBegin(GL_TRIANGLES);glVertex2f((float)x0,(float)y0);glVertex2f((float)x0+90,(float)y0);glVertex2f((float)x0,(float)y0+90);glEnd();}
static void tri_cw(int x0,int y0) {glBegin(GL_TRIANGLES);glVertex2f((float)x0,(float)y0);glVertex2f((float)x0,(float)y0+90);glVertex2f((float)x0+90,(float)y0);glEnd();}
static void two_sided_draw(int ext) {
 glClearStencil(5);glClear(GL_STENCIL_BUFFER_BIT);glEnable(GL_STENCIL_TEST);glColorMask(0,0,0,0);
 if(ext){glEnable(GL_STENCIL_TEST_TWO_SIDE_EXT);pActiveFace(GL_BACK);glStencilFunc(GL_ALWAYS,3,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);
  pActiveFace(GL_FRONT);glStencilFunc(GL_ALWAYS,1,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);}
 else {pFuncSep(GL_FRONT,GL_ALWAYS,1,0xFF);pFuncSep(GL_BACK,GL_ALWAYS,3,0xFF);pOpSep(GL_FRONT,GL_KEEP,GL_KEEP,GL_REPLACE);pOpSep(GL_BACK,GL_KEEP,GL_KEEP,GL_INCR);}
 tri_ccw(10,10);tri_cw(140,10);
 lined[curk]=1;glBegin(GL_LINES);glVertex2f(10,200.5f);glVertex2f(240,200.5f);glEnd(); /* a line after a back face: the front state */
 glColorMask(1,1,1,1);
}
static void c_twoside(void) {
 const char *how;int pass;
 for(pass=0;pass<2;pass++) {
  if(pass==0&&!pFuncSep){fprintf(logf,"     (glStencilFuncSeparate not offered: skipped)\n");continue;}
  if(pass==1&&!pActiveFace){fprintf(logf,"     (EXT_stencil_two_side not offered: skipped)\n");continue;}
  how=pass?"EXT_stencil_two_side":"StencilFuncSeparate";
  reset();two_sided_draw(pass);
  {char t[80];sprintf(t,"%s: front face REPLACE 1",how);want_st(30,30,1,t);sprintf(t,"%s: back face INCR from 5",how);want_st(160,30,6,t);
   sprintf(t,"%s: a line after a back face (front state)",how);want_st(100,200,1,t);sprintf(t,"%s: untouched",how);want_st(100,150,5,t);}
  reset();glFrontFace(GL_CW);two_sided_draw(pass);
  {char t[80];sprintf(t,"%s, glFrontFace(GL_CW): the CCW triangle is a back face",how);want_st(30,30,6,t);sprintf(t,"%s, glFrontFace(GL_CW): the CW triangle is a front face",how);want_st(160,30,1,t);}
 }
}
static void c_cull(void) {
 glEnable(GL_STENCIL_TEST);glStencilFunc(GL_ALWAYS,7,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_REPLACE);
 glEnable(GL_CULL_FACE);glCullFace(GL_FRONT_AND_BACK);
 tri_ccw(10,10);tri_cw(140,10);rect(10,120,100,160,0,0);
 lined[curk]=1;glBegin(GL_LINES);glVertex2f(10,200.5f);glVertex2f(240,200.5f);glEnd();
 glBegin(GL_POINTS);glVertex2f(50.5f,230.5f);glEnd();
 glDisable(GL_CULL_FACE);
 want_st(30,30,0,"front triangle, both faces culled");want_st(160,30,0,"back triangle, both faces culled");want_st(50,140,0,"rectangle, both faces culled");
 want_col(30,30,'K',"front triangle, both faces culled");want_col(50,140,'K',"rectangle, both faces culled");
 want_st(100,200,7,"line, both faces culled");want_col(100,200,'W',"line, both faces culled");want_st(50,230,7,"point, both faces culled");
}
/* shadow volumes: a floor at depth zf, a box from z +0.5 (front face, counter-clockwise) to -0.5 (back face, clockwise) over the
 * footprint 80..175 x 80..175; the shadowed floor (inside the box) gets stencil 1, darkened where it is not 0 */
static int volume_pass(int zfail,int onepass) {
 glColorMask(0,0,0,0);glDepthMask(GL_FALSE);glEnable(GL_DEPTH_TEST);glEnable(GL_STENCIL_TEST);glStencilFunc(GL_ALWAYS,0,0xFF);
 if(onepass) {
  if(!pFuncSep)return 0;
  glDisable(GL_CULL_FACE);
  if(!zfail){pOpSep(GL_FRONT,GL_KEEP,GL_KEEP,have_wrap?GL_INCR_WRAP:GL_INCR);pOpSep(GL_BACK,GL_KEEP,GL_KEEP,have_wrap?GL_DECR_WRAP:GL_DECR);}
  else {pOpSep(GL_BACK,GL_KEEP,have_wrap?GL_INCR_WRAP:GL_INCR,GL_KEEP);pOpSep(GL_FRONT,GL_KEEP,have_wrap?GL_DECR_WRAP:GL_DECR,GL_KEEP);}
  rect(80,80,176,176,0.5f,0);rect(80,80,176,176,-0.5f,1);
 } else {
  glEnable(GL_CULL_FACE);
  if(!zfail){glCullFace(GL_BACK);glStencilOp(GL_KEEP,GL_KEEP,GL_INCR);rect(80,80,176,176,0.5f,0);rect(80,80,176,176,-0.5f,1);
   glCullFace(GL_FRONT);glStencilOp(GL_KEEP,GL_KEEP,GL_DECR);rect(80,80,176,176,0.5f,0);rect(80,80,176,176,-0.5f,1);}
  else {glCullFace(GL_FRONT);glStencilOp(GL_KEEP,GL_INCR,GL_KEEP);rect(80,80,176,176,0.5f,0);rect(80,80,176,176,-0.5f,1);
   glCullFace(GL_BACK);glStencilOp(GL_KEEP,GL_DECR,GL_KEEP);rect(80,80,176,176,0.5f,0);rect(80,80,176,176,-0.5f,1);}
  glDisable(GL_CULL_FACE);
 }
 glColorMask(1,1,1,1);glDepthMask(GL_TRUE);
 return 1;
}
static void shadow_scene(int zfail,int onepass,float zfloor) {
 glEnable(GL_DEPTH_TEST);glColor3f(0.7f,0.7f,0.7f);rect(0,0,W,H,zfloor,0); /* the floor */
 if(!volume_pass(zfail,onepass))return;
 glDisable(GL_DEPTH_TEST);glStencilFunc(GL_NOTEQUAL,0,0xFF);glStencilOp(GL_KEEP,GL_KEEP,GL_KEEP);glColor3f(0.2f,0.2f,0.2f);rect(0,0,W,H,0,0);
 glDisable(GL_STENCIL_TEST);
}
static void c_shadow(void) {
 static const float zf[3]={0.0f,0.8f,-0.8f};static const char *zn[3]={"floor inside the box","floor in front of the box","floor behind the box"};
 int zfail,one,f;char t[96];
 for(f=0;f<3;f++)for(zfail=0;zfail<2;zfail++)for(one=0;one<2;one++) {
  long inside=0,outside=0;int x,y;static unsigned char sb[W*H];
  if(one&&!pFuncSep)continue;
  reset();shadow_scene(zfail,one,zf[f]);
  glReadPixels(0,0,W,H,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,sb);
  for(y=0;y<H;y++)for(x=0;x<W;x++){int in=x>=80&&x<176&&y>=80&&y<176;unsigned want=(in&&f==0)?1:0;if(sb[y*W+x]!=want){if(in)inside++;else outside++;}}
  sprintf(t,"%s, %s, %s",zfail?"z-fail":"z-pass",one?"one two-sided pass":"two passes",zn[f]);
  if(inside||outside)bad("%s: %ld pixels wrong inside the footprint, %ld outside (want stencil %d inside, 0 outside; the centre holds %02X)",t,inside,outside,f==0?1:0,sb[128*W+128]);
  want_col(128,128,f==0?'S':'F',t);want_col(20,20,'F',t);
  SwapBuffers(gdc);
 }
 reset();shadow_scene(0,pFuncSep?1:0,0.0f); /* the image kept for the comparison */
}
static void c_speed(void) {
 DWORD t0;int k;
 if(!pFuncSep){fprintf(logf,"     (no two-sided stencil: not timed)\n");return;}
 glEnable(GL_DEPTH_TEST);glColor3f(0.7f,0.7f,0.7f);rect(0,0,W,H,0,0);
 glFinish();t0=GetTickCount();
 for(k=0;k<frames;k++){glClear(GL_STENCIL_BUFFER_BIT);volume_pass(0,1);}
 glFinish();tms[which]=(double)(GetTickCount()-t0)/frames;
 fprintf(logf,"     %d two-sided z-pass volume passes: %.2f ms each\n",frames,tms[which]);
 want_st(128,128,1,"after the timed passes");
}
static struct {const char *name;void (*f)(void);} cases[]={
 {"clears and a masked clear",c_clear},{"the eight operations on 00 01 80 FF",c_ops},{"stencil fail, depth fail, depth pass",c_paths},
 {"the eight functions against 40",c_funcs},{"value and write masks",c_masks},{"two-sided stencil",c_twoside},{"culling both faces",c_cull},
 {"shadow volumes",c_shadow},{"speed: two-sided volume passes",c_speed}};
#define NCASES ((int)(sizeof(cases)/sizeof(cases[0])))

/* v1.1: case 2 on the board (v10.264) had 90 stencil pixels in small tile-shaped patches of the depth-fail area holding 01 where 11
 * belongs. The probe runs case 2 again, after different things, and counts its wrong pixels against the worked-out image: which of them
 * hold what the case before it left there, 00, the clear value, or something else, and in which 8x8 cells. Variants: 0 and 1 after
 * case 1 as in the run (twice: repeatable?); 2 after case 2 itself; 3 after case 1 with the 10 clear drawn by the engine instead of
 * glClear (the 2D fill); 4 after case 1, the engine finished and the planes read between the near and the far rectangle; 5 after case 1
 * with no read of its image (the run reads every case's image). */
static unsigned paths_want(int x,int y) {
 if(x>=100&&x<160&&y>=100&&y<160)return 0x00;
 if(y>=10&&y<60){if(x>=10&&x<60)return 0x11;if(x>=70&&x<100)return 0x0F;if(x>=100&&x<130)return 0x11;}
 return 0x10;
}
static unsigned char pre_img[W*H],got_img[W*H],cell[(W/8)*(H/8)];static int probe_wrong;
/* v1.2 (v10.290; board, v10.289 at 16 bpp: the run's case 2 drew 01 over most of the far rectangle, in whole 8x8 tiles, while the v1.1
 * probe's variants drew it right): variants 6 to 10 go between case 1 and case 2 as the run does (both images read, a swap, 150 ms and
 * the messages), 7 without the swap, 8 without the 150 ms, 9 with the VxD's constant groups off (TEXDBG CB), 10 with its asynchronous
 * clears off (CD); each run three times. The VxD's settings are put back after. */
#ifndef VXSTEN_HOST
static HANDLE vxd=INVALID_HANDLE_VALUE;
static int texdbg(DWORD code,DWORD *o) {DWORD in=code,got=0;memset(o,0,32);return vxd!=INVALID_HANDLE_VALUE&&DeviceIoControl(vxd,0x85500022UL,&in,4,o,32,&got,0)&&got==32;}
#else
static int texdbg(DWORD code,DWORD *o) {(void)code;memset(o,0,32);return 0;}
#endif
/* v1.3 (v10.291; board, v10.290: case 1 lost about 32 draws while two of the VxD's fetch groups did not finish): after each case of the
 * engine run, the groups that did not finish during it and the packets of the last one (TEXDBG 0xE0 + n) */
static DWORD hang_seen;
static void hang_report(const char *when) {
 DWORD o[8],n,i;
 if(!texdbg(0xE0,o)||o[0]!=0xE0)return;
 if(o[1]==hang_seen)return;
 n=o[3]&0xFFFF;fprintf(logf,"     %s: %lu of the VxD's fetch groups did not finish (%lu so far); the last: status %08lX, %lu packets\n",when,(unsigned long)(o[1]-hang_seen),(unsigned long)o[1],(unsigned long)o[2],(unsigned long)n);
 hang_seen=o[1];
 for(i=0;i<n&&i<16;i++){DWORD a,b;if(!texdbg(0xE0+i,o))break;a=o[4];b=o[5];
  fprintf(logf,"       %2lu: op %lu target %02lX depth %02lX, %lu vertices; z %lu write %lu func %lu; blend %lu; textured %lu units %lu; prim %lu dither %lu stencil %lu (%06lX %04lX) alpha %lu; program %lu; write mask %04lX; %lu words\n",
   (unsigned long)i,(unsigned long)(a&255),(unsigned long)((a>>8)&255),(unsigned long)((a>>16)&255),(unsigned long)(a>>24),(unsigned long)(b&1),(unsigned long)((b>>1)&1),(unsigned long)((b>>4)&15),
   (unsigned long)((b>>8)&1),(unsigned long)((b>>9)&1),(unsigned long)((b>>10)&3),(unsigned long)((b>>12)&15),(unsigned long)((b>>16)&1),(unsigned long)((b>>17)&3),(unsigned long)o[6],(unsigned long)(o[7]&0xFFFF),
   (unsigned long)((b>>20)&15),(unsigned long)(b>>24),(unsigned long)(o[3]>>16),(unsigned long)(o[7]>>16));}
}
static void probe_paths(void) {
 static const char *vn[9]={"after case 1","after case 1","after case 2","after case 1, the 10 clear drawn by the engine","after case 1, finished and read between the rectangles",
  "after case 1, its image not read","after case 1 as the run: images read, swap, 150 ms","as 6 without the swap","as 6 without the 150 ms"}; /* v2.0 (.533): 9 and 10 (the VxD's switch words CA-CD, gone in .530) removed */
 int v,i,rep;
 fprintf(logf,"   probe of case 2 (the engine): wrong pixels against the worked-out image\n");probe_wrong=0;
 for(v=0;v<9;v++)for(rep=0;rep<(v>=6?3:1);rep++) {
  int wrong=0,left=0,zero=0,ten=0,other=0,cells=0,shown=0,cx,cy;
  reset();if(v==2)c_paths();else c_ops();glFinish();
  if(v!=5)glReadPixels(0,0,W,H,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,pre_img);else memset(pre_img,0xEE,sizeof(pre_img));
  if(v>=6){static unsigned char ci[W*H*4];glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,ci);
#ifndef VXSTEN_HOST
   if(v!=7)SwapBuffers(gdc);if(v!=8)Sleep(150);
   {MSG m;while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}}
#endif
  }
  reset();paths_quiet=1;paths_draw_clear=v==3;paths_retire=v==4;c_paths();paths_quiet=paths_draw_clear=paths_retire=0;glFinish();
  glReadPixels(0,0,W,H,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,got_img);
  memset(cell,0,sizeof(cell));
  for(i=0;i<W*H;i++){unsigned g=got_img[i],w=paths_want(i%W,i/W);if(g==w)continue;wrong++;
   if(v!=5&&g==pre_img[i])left++;else if(g==0)zero++;else if(g==0x10)ten++;else other++;
   if(!cell[(i/W/8)*(W/8)+(i%W)/8]++)cells++;}
  fprintf(logf,"     %2d %-54s %5d wrong: %d as the case before left them, %d 00, %d 10, %d other; %d 8x8 cells\n",v,vn[v],wrong,left,zero,ten,other,cells);
  if(v<6)probe_wrong+=wrong;
  for(cy=0;cy<H/8&&shown<(v>=6?3:12);cy++)for(cx=0;cx<W/8&&shown<(v>=6?3:12);cx++)if(cell[cy*(W/8)+cx]) {
   int x,y,fx=-1,fy=-1;for(y=cy*8;y<cy*8+8&&fx<0;y++)for(x=cx*8;x<cx*8+8;x++)if(got_img[y*W+x]!=paths_want(x,y)){fx=x;fy=y;break;}
   fprintf(logf,"         cell %3d,%3d: %2d wrong, first %d,%d: %02X, want %02X, the case before left %02X\n",cx*8,cy*8,cell[cy*(W/8)+cx],fx,fy,got_img[fy*W+fx],paths_want(fx,fy),pre_img[fy*W+fx]);shown++;}
 }
}
#ifdef VXSTEN_HOST
int vxsten_host(FILE *log,int slot,void *fs,void *os,void *af) {
 int k;logf=log;which=slot;failures=0;frames=20;
 pFuncSep=(sfs_fn)fs;pOpSep=(sos_fn)os;pActiveFace=(asf_fn)af;have_wrap=1;
 for(k=0;k<NCASES;k++){case_fail=0;curk=k;reset();cases[k].f();glFinish();if(glGetError()!=GL_NO_ERROR){case_fail++;fprintf(logf,"     FAIL a GL error\n");}
  glReadPixels(0,0,W,H,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,simg[slot][k]);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,cimg[slot][k]);
  if(case_fail){fprintf(logf,"VXSTEN %d %s: FAIL\n",k,cases[k].name);failures+=case_fail;}}
 if(slot==0){probe_paths();failures+=probe_wrong;}
 return failures;
}
int vxsten_host_compare(FILE *log) {
 int k,f=0;
 for(k=0;k<NCASES;k++){int ds,dc,at;differ(k,0,2,&ds,&dc,&at);
  if(ds||dc){fprintf(log,"VXSTEN %d %s: engine against software: stencil %d, colour %d pixels differ, first at %d,%d (stencil %u against %u)\n",k,cases[k].name,ds,dc,at%W,at/W,simg[0][k][at],simg[2][k][at]);f++;}}
 return f;
}
#else
/* ------------------------------------------------------------------ runs */

static int choose_format(HDC dc,int generic) {
 PIXELFORMATDESCRIPTOR p;int n=DescribePixelFormat(dc,1,sizeof(p),&p),i,best=0,score=-1;
 for(i=1;i<=n;i++) {
  int s;DescribePixelFormat(dc,i,sizeof(p),&p);
  if(!(p.dwFlags&PFD_SUPPORT_OPENGL)||!(p.dwFlags&PFD_DRAW_TO_WINDOW)||!(p.dwFlags&PFD_DOUBLEBUFFER)||p.iPixelType!=PFD_TYPE_RGBA)continue;
  if(((p.dwFlags&PFD_GENERIC_FORMAT)!=0)!=generic)continue;
  if(generic&&(p.dwFlags&PFD_GENERIC_ACCELERATED))continue;
  if(p.cStencilBits<8||p.cDepthBits<16)continue;
  s=(p.cDepthBits>=24?10:0)+(p.cColorBits==GetDeviceCaps(dc,BITSPIXEL)?1000:0);
  if(s>score){score=s;best=i;}
 }
 if(best){DescribePixelFormat(dc,best,sizeof(p),&p);fprintf(logf,"   pixel format %d: colour %d, depth %d, stencil %d\n",best,p.cColorBits,p.cDepthBits,p.cStencilBits);}
 return best;
}
/* which: 0 the ICD (engine), 1 Microsoft's generic OpenGL, 2 the ICD with ACCEL_OPENGL_HW = 0 */
static void run(int w_) {
 WNDCLASSA wc;HWND w;HDC dc;HGLRC rc;int pf,k;RECT r;const char *ver,*ext;
 const char *name=w_==1?"Microsoft software OpenGL (reference, failures not counted)":w_==2?"VX855 ICD, engine off (ACCEL_OPENGL_HW = 0)":"VX855 ICD with the engine";
 which=w_;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="VXSTEN";wc.style=CS_OWNDC;RegisterClassA(&wc);
 r.left=0;r.top=0;r.right=W;r.bottom=H;AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
 w=CreateWindowA("VXSTEN",name,WS_OVERLAPPEDWINDOW|WS_VISIBLE,60,60,r.right-r.left,r.bottom-r.top,0,0,wc.hInstance,0);
 fprintf(logf,"== %s%s\n",name,probe_sub?(probe_clear?" (probe: a fresh context, depth and stencil cleared by an engine draw)":probe_warm?" (probe: a fresh context, a depth- and stencil-tested draw after each clear)":" (probe: a fresh context, as the run)"):"");
 if(!w){fprintf(logf,"   no window\n");if(which!=1)failures++;return;}
 dc=GetDC(w);gdc=dc;
 pf=choose_format(dc,which==1);
 if(!pf){fprintf(logf,"   no pixel format with an 8-bit stencil\n");if(which!=1)failures++;DestroyWindow(w);return;}
 {PIXELFORMATDESCRIPTOR p;DescribePixelFormat(dc,pf,sizeof(p),&p);if(!SetPixelFormat(dc,pf,&p)){fprintf(logf,"   SetPixelFormat failed\n");if(which!=1)failures++;DestroyWindow(w);return;}}
 rc=wglCreateContext(dc);if(!rc||!wglMakeCurrent(dc,rc)){fprintf(logf,"   no context\n");if(which!=1)failures++;DestroyWindow(w);return;}
 ver=(const char*)glGetString(GL_VERSION);ext=(const char*)glGetString(GL_EXTENSIONS);
 fprintf(logf,"   GL_RENDERER %s, GL_VERSION %s\n",(const char*)glGetString(GL_RENDERER),ver?ver:"?");
 pFuncSep=0;pOpSep=0;pActiveFace=0;
 if(ver&&ver[0]>='2'){pFuncSep=(sfs_fn)wglGetProcAddress("glStencilFuncSeparate");pOpSep=(sos_fn)wglGetProcAddress("glStencilOpSeparate");if(!pFuncSep||!pOpSep)pFuncSep=0,pOpSep=0;}
 if(ext&&strstr(ext,"GL_EXT_stencil_two_side"))pActiveFace=(asf_fn)wglGetProcAddress("glActiveStencilFaceEXT");
 have_wrap=(ver&&(ver[0]>'1'||(ver[0]=='1'&&ver[2]>='4')))||(ext&&strstr(ext,"GL_EXT_stencil_wrap"));
 fprintf(logf,"   separate stencil %s, EXT_stencil_two_side %s, wrap operations %s\n",pFuncSep?"yes":"no",pActiveFace?"yes":"no",have_wrap?"yes":"no");
 if(which==0){const char *rn=(const char*)glGetString(GL_RENDERER);if(!rn||strstr(rn,"(software")){fprintf(logf,"   the engine is not drawing: FAIL\n");failures++;}}
 if(which==2){const char *rn=(const char*)glGetString(GL_RENDERER);if(!rn||!strstr(rn,"(software")){fprintf(logf,"   the engine-off run is not in software (VXGL_HW = 0 not taken): FAIL\n");failures++;}} /* v10.319 (audit T2): the reference is checked */
 for(k=0;k<NCASES;k++) {
  MSG m;while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}
  if(probe_sub&&k!=0&&k!=1&&k!=2&&k!=7)continue;
  case_fail=0;curk=k;reset();cases[k].f();glFinish();
  if(glGetError()!=GL_NO_ERROR){case_fail++;fprintf(logf,"     FAIL a GL error\n");}
  glReadPixels(0,0,W,H,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,simg[which][k]);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,cimg[which][k]);shave[which][k]=1;
  fprintf(logf,"%2d %-40s %s\n",k,cases[k].name,case_fail?"FAIL":"pass");
  if(which==0)hang_report("during this case"); /* v1.3 */
  /* v1.5 (v10.292): a case of the engine run that failed is run twice more at once in the same context (reported): does it fail again? */
  if(which==0&&case_fail&&!probe_sub){int a,f=case_fail,f2;for(a=0;a<2;a++){int sv=failures;FILE *l=logf;case_fail=0;logf=fopen("NUL","w");if(!logf)logf=l;reset();cases[k].f();glFinish();
    if(logf!=l)fclose(logf);logf=l;f2=case_fail;failures=sv;fprintf(logf,"     again at once (%d): %s (%d checks wrong)\n",a+1,f2?"FAIL":"pass",f2);}case_fail=f;}
  if(case_fail&&which!=1)failures+=case_fail;
  SwapBuffers(dc);Sleep(150);
 }
 if(which==0&&!probe_sub){probe_paths();hang_report("during the probe");} /* v1.1; v1.3 */
 wglMakeCurrent(0,0);wglDeleteContext(rc);ReleaseDC(w,dc);DestroyWindow(w);
}
static void bmp(const char *name,const unsigned char *s) {
 FILE *f=fopen(name,"wb");int y,x;unsigned char h[54];
 if(!f)return;memset(h,0,54);h[0]='B';h[1]='M';*(DWORD*)(h+2)=54+W*H*3;*(DWORD*)(h+10)=54;*(DWORD*)(h+14)=40;*(LONG*)(h+18)=W;*(LONG*)(h+22)=H;*(WORD*)(h+26)=1;*(WORD*)(h+28)=24;
 fwrite(h,1,54,f);for(y=0;y<H;y++)for(x=0;x<W;x++){fputc(s[y*W+x],f);fputc(s[y*W+x],f);fputc(s[y*W+x],f);}
 fclose(f);
}
/* v10.310: the engine-off run is asked for by VXGL_HW = 0 in this process's environment (VXGL.DLL reads it at CreateContext), not by
 * ACCEL_OPENGL_HW in the registry: a run stopped during its engine-off part (a hang, a reboot) left the key at 0, and each later run saved
 * that 0 and put it back (board, v10.309: every OpenGL program drew in software) */
static int hw_key(int set,DWORD *saved,int *existed) {(void)saved;(void)existed;if(set)return SetEnvironmentVariableA("VXGL_HW","0")!=0;SetEnvironmentVariableA("VXGL_HW",0);return 1;}
/* v10.310: a 0 left in the registry by an earlier version is reported (the engine is off for every OpenGL program until it is deleted) */
static void hw_key_note(FILE *l) {HKEY k;DWORD v=1,t=0,n=4;
 if(RegOpenKeyExA(HKEY_LOCAL_MACHINE,"Software\\VX85598",0,KEY_READ,&k)!=ERROR_SUCCESS)return;
 if(RegQueryValueExA(k,"ACCEL_OPENGL_HW",0,&t,(BYTE*)&v,&n)==ERROR_SUCCESS&&t==REG_DWORD&&!v)fprintf(l,"ACCEL_OPENGL_HW = 0 in HKLM\\Software\\VX85598: the engine is off for every OpenGL program until it is deleted\n");
 RegCloseKey(k);}
int main(int argc,char **argv) {
 int k;
 if(argc>1)frames=atoi(argv[1]);if(frames<1)frames=1;
 logf=fopen("VXSTEN.TXT","w");if(!logf)return 1;setvbuf(logf,0,_IONBF,0);
 vxd=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0); /* v1.2 */
 {DWORD o[8];if(texdbg(0xE0,o)&&o[0]==0xE0)hang_seen=o[1];} /* v1.3: what came before this run is not reported */
 fprintf(logf,"VXSTEN v2.0 (driver .533): OpenGL stencil against worked-out answers, %dx%d\n",W,H);
 hw_key_note(logf);
 {HDC d=GetDC(0);fprintf(logf,"desktop %dx%dx%d\n",GetDeviceCaps(d,HORZRES),GetDeviceCaps(d,VERTRES),GetDeviceCaps(d,BITSPIXEL));ReleaseDC(0,d);}
 /* v1.5 (v10.293; board, v1.4: the run's case 2 failed while nine fresh contexts after it passed): the process's first context is a
  * fresh-context probe (cases 0, 1, 2, 7, reported); the run's context is then the second */
 {int f0=failures;probe_sub=1;run(0);probe_sub=0;failures=f0;fprintf(logf,"   (the probe above was the process's first context: not counted)\n");}
 run(0);
 {DWORD saved=0;int existed=0;if(hw_key(1,&saved,&existed)){run(2);hw_key(0,&saved,&existed);}else{fprintf(logf,"ACCEL_OPENGL_HW could not be set: no engine-off run\n");failures++;}}
 run(1);
 fprintf(logf,"== the engine against the ICD without it (stencil and colour, every pixel)\n");
 for(k=0;k<NCASES;k++) {
  int ds,dc,at;
  if(!shave[0][k]||!shave[2][k]){fprintf(logf,"%2d %-40s not drawn by both\n",k,cases[k].name);continue;}
  differ(k,0,2,&ds,&dc,&at);
  fprintf(logf,"%2d %-40s stencil: %5d pixels differ, colour: %5d",k,cases[k].name,ds,dc);
  if(ds||dc)fprintf(logf,"; first difference at %d,%d: stencil %02X against %02X",at%W,at/W,simg[0][k][at],simg[2][k][at]);
  if(ds||dc){char nm[32];fprintf(logf,"  FAIL");failures++;sprintf(nm,"VXSTEN_%02d_HW.BMP",k);bmp(nm,simg[0][k]);sprintf(nm,"VXSTEN_%02d_SW.BMP",k);bmp(nm,simg[2][k]);}
  fprintf(logf,"\n");
 }
 /* v1.4: fresh contexts (the run's failures come early in its context; the probe's runs late in it all pass): three each of the run as it
  * is, with engine-drawn depth and stencil clears, and with a tested draw after each clear; reported, not counted */
 {int f0=failures,v,r;for(v=0;v<3;v++)for(r=0;r<3;r++){probe_sub=1;probe_clear=v==1;probe_warm=v==2;run(0);}probe_sub=probe_clear=probe_warm=0;failures=f0;}
 /* v2.0 (.533): the Coarse Z probe (TEXDBG D4/D5, v1.7-v1.9) removed: Coarse Z is gone from the driver */
 if(tms[0]>0&&tms[2]>0)fprintf(logf,"two-sided volume pass: engine %.2f ms, engine off %.2f ms (%.1fx)\n",tms[0],tms[2],tms[2]/tms[0]);
 fprintf(logf,"failures=%d\n",failures);fclose(logf);
 return failures;
}
#endif
