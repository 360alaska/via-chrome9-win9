/* Q2MENU v1.4 (driver v10.342): Quake 2 3.14's OpenGL calls (ref_gl.dll of March 1998, read from its code), replayed to find why the
 * world goes black after the menu opens. Each case is a new window and context: 40 normal frames, 40 frames with one part of the menu
 * (or all of it), 40 normal frames again; the world's colour is read before and after. A case whose world changed is the culprit.
 * Normal frame, as ref_gl: R_BeginFrame (2D mode, depth clear), R_SetupGL (viewport, frustum, cull, depth test), the world (each
 * surface's texture with GL_REPLACE, then the lightmaps: depth mask off, blend ZERO / SRC_COLOR, the dynamic lightmap rewritten with
 * glTexSubImage2D), the particles (attenuated smooth points), 2D mode again, the HUD (a picture with alpha), SwapBuffers.
 * Texture names as Quake's: lightmaps from 1024 (1024 = the dynamic one), the scrap 1152, images from 1153; no glGenTextures.
 * Output: Q2MENU.TXT next to the program. MIT, 2026. */
#ifdef Q2MENU_HOST /* the host test (gl/host/vxgl_host.c): the same frames against Mesa with the engine model and without it */
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#define _vsnprintf vsnprintf /* host C library; the Win98 branch keeps Watcom's name */
#include "GL/gl.h"
#include "GL/glext.h"
#define WINAPI
#define q(n) n
#define qglEnable glEnable
#define qglDisable glDisable
#define qglAlphaFunc glAlphaFunc
#define qglCullFace glCullFace
#define qglClearColor glClearColor
#define qglColor4f glColor4f
#define qglColor3f glColor3f
#define qglColor4ubv glColor4ubv
#define qglPolygonMode glPolygonMode
#define qglShadeModel glShadeModel
#define qglTexParameterf glTexParameterf
#define qglBlendFunc glBlendFunc
#define qglTexEnvf glTexEnvf
#define qglViewport glViewport
#define qglMatrixMode glMatrixMode
#define qglLoadIdentity glLoadIdentity
#define qglOrtho glOrtho
#define qglFrustum glFrustum
#define qglRotatef glRotatef
#define qglTranslatef glTranslatef
#define qglGetFloatv glGetFloatv
#define qglClear glClear
#define qglDepthFunc glDepthFunc
#define qglDepthRange glDepthRange
#define qglDepthMask glDepthMask
#define qglBindTexture glBindTexture
#define qglTexImage2D glTexImage2D
#define qglTexSubImage2D glTexSubImage2D
#define qglBegin glBegin
#define qglEnd glEnd
#define qglTexCoord2f glTexCoord2f
#define qglVertex2f glVertex2f
#define qglVertex3fv glVertex3fv
#define qglPointSize glPointSize
#define qglReadPixels glReadPixels
#define qglFinish glFinish
#define qglGetError glGetError
#define qglDrawBuffer glDrawBuffer
#define qglPushMatrix glPushMatrix
#define qglPopMatrix glPopMatrix
typedef void (*pp_t)(GLenum,GLfloat);typedef void (*ppv_t)(GLenum,const GLfloat*);static pp_t qglPointParameterfEXT;static ppv_t qglPointParameterfvEXT;
#else
#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

typedef unsigned int GLenum;typedef unsigned int GLuint;typedef int GLint;typedef int GLsizei;typedef float GLfloat;typedef double GLdouble;typedef unsigned char GLubyte;typedef unsigned int GLbitfield;typedef void GLvoid;typedef float GLclampf;typedef double GLclampd;typedef unsigned char GLboolean;
#define GL_TEXTURE_2D 0x0DE1
#define GL_BLEND 0x0BE2
#define GL_ALPHA_TEST 0x0BC0
#define GL_DEPTH_TEST 0x0B71
#define GL_CULL_FACE 0x0B44
#define GL_POINT_SMOOTH 0x0B10
#define GL_GREATER 0x0204
#define GL_LEQUAL 0x0203
#define GL_EQUAL 0x0202
#define GL_FRONT 0x0404
#define GL_BACK 0x0405
#define GL_FRONT_AND_BACK 0x0408
#define GL_FILL 0x1B02
#define GL_FLAT 0x1D00
#define GL_SMOOTH 0x1D01
#define GL_TRIANGLE_FAN 6
#define GL_SRC_ALPHA 0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303
#define GL_ZERO 0
#define GL_SRC_COLOR 0x0300
#define GL_TEXTURE_ENV 0x2300
#define GL_TEXTURE_ENV_MODE 0x2200
#define GL_REPLACE 0x1E01
#define GL_MODULATE 0x2100
#define GL_TEXTURE_MIN_FILTER 0x2801
#define GL_TEXTURE_MAG_FILTER 0x2800
#define GL_LINEAR 0x2601
#define GL_LINEAR_MIPMAP_NEAREST 0x2701
#define GL_RGBA 0x1908
#define GL_UNSIGNED_BYTE 0x1401
#define GL_PROJECTION 0x1701
#define GL_MODELVIEW 0x1700
#define GL_QUADS 7
#define GL_POLYGON 9
#define GL_POINTS 0
#define GL_DEPTH_BUFFER_BIT 0x100
#define GL_COLOR_BUFFER_BIT 0x4000
#define GL_POINT_SIZE_MIN_EXT 0x8126
#define GL_POINT_SIZE_MAX_EXT 0x8127
#define GL_DISTANCE_ATTENUATION_EXT 0x8129
#define GL_VENDOR 0x1F00
#define GL_RENDERER 0x1F01
#define GL_VERSION 0x1F02
#define GL_MODELVIEW_MATRIX 0x0BA6

#define FN(r,n,a) typedef r (WINAPI *n##_t) a; static n##_t q##n;
FN(void,glEnable,(GLenum)) FN(void,glDisable,(GLenum)) FN(void,glAlphaFunc,(GLenum,GLclampf)) FN(void,glCullFace,(GLenum)) FN(void,glClearColor,(GLclampf,GLclampf,GLclampf,GLclampf))
FN(void,glColor4f,(GLfloat,GLfloat,GLfloat,GLfloat)) FN(void,glColor3f,(GLfloat,GLfloat,GLfloat)) FN(void,glColor4ubv,(const GLubyte*)) FN(void,glPolygonMode,(GLenum,GLenum)) FN(void,glShadeModel,(GLenum))
FN(void,glTexParameterf,(GLenum,GLenum,GLfloat)) FN(void,glBlendFunc,(GLenum,GLenum)) FN(void,glTexEnvf,(GLenum,GLenum,GLfloat)) FN(void,glViewport,(GLint,GLint,GLsizei,GLsizei))
FN(void,glMatrixMode,(GLenum)) FN(void,glLoadIdentity,(void)) FN(void,glOrtho,(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble)) FN(void,glFrustum,(GLdouble,GLdouble,GLdouble,GLdouble,GLdouble,GLdouble))
FN(void,glRotatef,(GLfloat,GLfloat,GLfloat,GLfloat)) FN(void,glTranslatef,(GLfloat,GLfloat,GLfloat)) FN(void,glGetFloatv,(GLenum,GLfloat*)) FN(void,glClear,(GLbitfield)) FN(void,glDepthFunc,(GLenum))
FN(void,glDepthRange,(GLclampd,GLclampd)) FN(void,glDepthMask,(GLboolean)) FN(void,glBindTexture,(GLenum,GLuint)) FN(void,glTexImage2D,(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const GLvoid*))
FN(void,glTexSubImage2D,(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const GLvoid*)) FN(void,glBegin,(GLenum)) FN(void,glEnd,(void)) FN(void,glTexCoord2f,(GLfloat,GLfloat))
FN(void,glVertex2f,(GLfloat,GLfloat)) FN(void,glVertex3fv,(const GLfloat*)) FN(void,glPointSize,(GLfloat)) FN(void,glReadPixels,(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,GLvoid*))
FN(void,glFinish,(void)) FN(GLenum,glGetError,(void)) FN(const GLubyte*,glGetString,(GLenum)) FN(void,glDrawBuffer,(GLenum)) FN(void,glPushMatrix,(void)) FN(void,glPopMatrix,(void))
FN(HGLRC,wglCreateContext,(HDC)) FN(BOOL,wglMakeCurrent,(HDC,HGLRC)) FN(BOOL,wglDeleteContext,(HGLRC)) FN(PROC,wglGetProcAddress,(LPCSTR)) FN(BOOL,wglSwapBuffers,(HDC))
typedef void (WINAPI *pp_t)(GLenum,GLfloat);typedef void (WINAPI *ppv_t)(GLenum,const GLfloat*);static pp_t qglPointParameterfEXT;static ppv_t qglPointParameterfvEXT;

#endif
static FILE *lf;
static void out(const char *f,...) {char t[1024];va_list a;va_start(a,f);_vsnprintf(t,sizeof(t)-1,f,a);va_end(a);t[sizeof(t)-1]=0;fputs(t,stdout);if(lf){fputs(t,lf);fflush(lf);}}
#ifndef Q2MENU_HOST
static HMODULE gl;
static int load(void) {
 gl=LoadLibraryA("opengl32");if(!gl)return 0;
#define L(n) if(!(q##n=(n##_t)GetProcAddress(gl,#n))){out("missing %s\n",#n);return 0;}
 L(glEnable)L(glDisable)L(glAlphaFunc)L(glCullFace)L(glClearColor)L(glColor4f)L(glColor3f)L(glColor4ubv)L(glPolygonMode)L(glShadeModel)L(glTexParameterf)L(glBlendFunc)L(glTexEnvf)
 L(glViewport)L(glMatrixMode)L(glLoadIdentity)L(glOrtho)L(glFrustum)L(glRotatef)L(glTranslatef)L(glGetFloatv)L(glClear)L(glDepthFunc)L(glDepthRange)L(glDepthMask)L(glBindTexture)
 L(glTexImage2D)L(glTexSubImage2D)L(glBegin)L(glEnd)L(glTexCoord2f)L(glVertex2f)L(glVertex3fv)L(glPointSize)L(glReadPixels)L(glFinish)L(glGetError)L(glGetString)L(glDrawBuffer)L(glPushMatrix)L(glPopMatrix)
 L(wglCreateContext)L(wglMakeCurrent)L(wglDeleteContext)L(wglGetProcAddress)L(wglSwapBuffers)
 return 1;
}

#endif
#define W 640
#define H 480
#define TEX_LM 1024
#define TEX_SCRAP 1152
#define TEX_IMG 1153
static GLubyte wall[64*64*4],lmap[128*128*4],dyn[16*16*4],hud[32*32*4],pic[128*64*4],scrap[256*256*4],tmp[64*64*4];
static int cur_texenv,cur_tex;
static void GL_TexEnv(int m) {if(m!=cur_texenv){qglTexEnvf(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,(GLfloat)m);cur_texenv=m;}}
static void GL_Bind(int t) {if(t!=cur_tex){cur_tex=t;qglBindTexture(GL_TEXTURE_2D,(GLuint)t);}}
/* GL_Upload32 with mipmaps (GL_MipMap: box filter), internal format 3 (solid) or 4 (alpha) */
static void upload(int name,const GLubyte *d,int w,int h,int comp,int mip) {
 int l=0;GL_Bind(name);qglTexImage2D(GL_TEXTURE_2D,0,comp,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,d);
 if(mip){const GLubyte *s=d;while(w>1||h>1){int x,y,nw=w>1?w/2:1,nh=h>1?h/2:1;static GLubyte m[64*64*4];
   for(y=0;y<nh;y++)for(x=0;x<nw;x++){int k;for(k=0;k<4;k++)m[(y*nw+x)*4+k]=(GLubyte)((s[((y*2)*w+x*2)*4+k]+s[((y*2)*w+x*2+(w>1))*4+k]+s[((y*2+(h>1))*w+x*2)*4+k]+s[((y*2+(h>1))*w+x*2+(w>1))*4+k])/4);}
   memcpy(tmp,m,nw*nh*4);s=tmp;w=nw;h=nh;l++;qglTexImage2D(GL_TEXTURE_2D,l,comp,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,s);}
  qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_NEAREST);qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);}
 else {qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);}
}
static void make_data(void) {
 int x,y;
 for(y=0;y<64;y++)for(x=0;x<64;x++){GLubyte *p=wall+(y*64+x)*4;int b=((x>>3)^(y>>3))&1;p[0]=(GLubyte)(b?200:150);p[1]=(GLubyte)(b?180:120);p[2]=(GLubyte)(b?140:90);p[3]=255;}
 for(y=0;y<128;y++)for(x=0;x<128;x++){GLubyte *p=lmap+(y*128+x)*4;int r=150+(x&31),g=140+(y&31),b=120,mx=r>g?r:g;p[0]=(GLubyte)r;p[1]=(GLubyte)g;p[2]=(GLubyte)b;p[3]=(GLubyte)mx;}
 for(y=0;y<32;y++)for(x=0;x<32;x++){GLubyte *p=hud+(y*32+x)*4;p[0]=255;p[1]=(GLubyte)(x*8);p[2]=0;p[3]=(GLubyte)((x>4&&x<28&&y>4&&y<28)?255:0);}
 for(y=0;y<64;y++)for(x=0;x<128;x++){GLubyte *p=pic+(y*128+x)*4;p[0]=(GLubyte)(x*2);p[1]=200;p[2]=(GLubyte)(y*4);p[3]=(GLubyte)((x+y)&8?255:0);}
 memset(scrap,0,sizeof(scrap));
}
/* GL_SetDefaultState */
static void default_state(void) {
 GLfloat att[3]={0.01f,0.0f,0.01f};
 qglClearColor(1,0,0.5f,0.5f);qglCullFace(GL_FRONT);qglEnable(GL_TEXTURE_2D);qglEnable(GL_ALPHA_TEST);qglAlphaFunc(GL_GREATER,0.666f);
 qglDisable(GL_DEPTH_TEST);qglDisable(GL_CULL_FACE);qglDisable(GL_BLEND);qglColor4f(1,1,1,1);qglPolygonMode(GL_FRONT_AND_BACK,GL_FILL);qglShadeModel(GL_FLAT);
 qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_NEAREST);qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 qglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);cur_texenv=-1;GL_TexEnv(GL_REPLACE);
 if(qglPointParameterfEXT){qglEnable(GL_POINT_SMOOTH);qglPointParameterfEXT(GL_POINT_SIZE_MIN_EXT,2);qglPointParameterfEXT(GL_POINT_SIZE_MAX_EXT,40);qglPointParameterfvEXT(GL_DISTANCE_ATTENUATION_EXT,att);}
}
static void set2d(void) { /* R_SetGL2D */
 qglViewport(0,0,W,H);qglMatrixMode(GL_PROJECTION);qglLoadIdentity();qglOrtho(0,W,H,0,-99999,99999);qglMatrixMode(GL_MODELVIEW);qglLoadIdentity();
 qglDisable(GL_DEPTH_TEST);qglDisable(GL_CULL_FACE);qglDisable(GL_BLEND);qglEnable(GL_ALPHA_TEST);qglColor4f(1,1,1,1);
}
/* the world: four walls of a room seen from its middle, in Quake's axes (x forward, y left, z up) */
static GLfloat wv[4][4][3];static const GLfloat st[4][2]={{0,0},{4,0},{4,4},{0,4}};
static void make_world(void) {
 static const GLfloat c[8][3]={{200,-300,-100},{200,300,-100},{200,300,200},{200,-300,200},{-200,-300,-100},{-200,300,-100},{-200,300,200},{-200,-300,200}};
 static const int f[4][4]={{0,1,2,3},{1,5,6,2},{4,0,3,7},{0,4,5,1}}; /* front, left, right, floor */
 int i,k;for(i=0;i<4;i++)for(k=0;k<4;k++)memcpy(wv[i][k],c[f[i][k]],sizeof(wv[i][k]));
}
static void poly(int i,int lm) {int k;qglBegin(GL_POLYGON);for(k=0;k<4;k++){if(lm)qglTexCoord2f(st[k][0]/4.0f,st[k][1]/4.0f);else qglTexCoord2f(st[k][0],st[k][1]);qglVertex3fv(wv[i][k]);}qglEnd();}
static int frame_no,pic_loaded;
/* 8192: the world as Quake's frame sends it: each wall cut into 20 x 20 polygons, each its own glBegin/glEnd (about 1600 draws a pass),
 * so the engine is still working when the next frame's depth clear comes */
static void poly_grid(int i,int lm) {
 int gx,gy,k;const int N=20;
 for(gy=0;gy<N;gy++)for(gx=0;gx<N;gx++){qglBegin(GL_POLYGON);
  for(k=0;k<4;k++){GLfloat u=(GLfloat)(gx+((k==1||k==2)?1:0))/N,v=(GLfloat)(gy+(k>=2?1:0))/N,p[3];int c;
   for(c=0;c<3;c++)p[c]=(1-u)*(1-v)*wv[i][0][c]+u*(1-v)*wv[i][1][c]+u*v*wv[i][2][c]+(1-u)*v*wv[i][3][c];
   if(lm)qglTexCoord2f(u,v);else qglTexCoord2f(u*4,v*4);qglVertex3fv(p);}
  qglEnd();}
}
static void frame(int menu) {
 int i;GLfloat m[16];
 /* R_BeginFrame: 2D, the draw buffer, R_Clear */
 set2d();qglDrawBuffer(GL_BACK);qglClear(GL_DEPTH_BUFFER_BIT);qglDepthFunc(GL_LEQUAL);qglDepthRange(0,1);
 /* R_SetupGL */
 qglViewport(0,0,W,H);qglMatrixMode(GL_PROJECTION);qglLoadIdentity();qglFrustum(-4*0.8284,4*0.8284,-4*0.6213,4*0.6213,4,4096);qglCullFace(GL_FRONT);
 qglMatrixMode(GL_MODELVIEW);qglLoadIdentity();qglRotatef(-90,1,0,0);qglRotatef(90,0,0,1);qglRotatef((GLfloat)((frame_no%20)-10)*0.5f,0,0,1);qglTranslatef((menu&2048)?(GLfloat)(frame_no*3):0.0f,0,-40); /* 2048: the camera steps back every frame: the world farther each time */
 qglGetFloatv(GL_MODELVIEW_MATRIX,m);qglEnable(GL_CULL_FACE);qglDisable(GL_BLEND);qglDisable(GL_ALPHA_TEST);qglEnable(GL_DEPTH_TEST);
 /* R_DrawWorld without multitexture: textures, then R_BlendLightmaps */
 qglColor3f(1,1,1);GL_TexEnv(GL_REPLACE);
 for(i=0;i<4;i++){GL_Bind(TEX_IMG+1+(i&1));if(menu&8192)poly_grid(i,0);else poly(i,0);}
 qglDepthMask(0);qglEnable(GL_BLEND);qglBlendFunc(GL_ZERO,GL_SRC_COLOR);
 GL_Bind(TEX_LM+1);for(i=0;i<3;i++){if(menu&8192)poly_grid(i,1);else poly(i,1);}
 { /* the dynamic lightmap: rebuilt and rewritten every frame (LM_UploadBlock dynamic) */
  int k;for(k=0;k<16*16;k++){dyn[k*4]=(GLubyte)(160+(frame_no&31));dyn[k*4+1]=150;dyn[k*4+2]=130;dyn[k*4+3]=160;}
  GL_Bind(TEX_LM);qglTexSubImage2D(GL_TEXTURE_2D,0,0,0,16,16,GL_RGBA,GL_UNSIGNED_BYTE,dyn);poly(3,1);}
 qglDisable(GL_BLEND);qglBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);qglDepthMask(1);
 /* R_DrawParticles with point parameters */
 if(qglPointParameterfEXT){static const GLfloat pv[5][3]={{150,-50,0},{150,0,20},{150,50,40},{120,-20,60},{100,30,10}};static const GLubyte pc[4]={255,200,40,160};
  qglDepthMask(0);qglEnable(GL_BLEND);qglDisable(GL_TEXTURE_2D);qglPointSize(40);qglBegin(GL_POINTS);for(i=0;i<5;i++){qglColor4ubv(pc);qglVertex3fv(pv[i]);}qglEnd();
  qglDisable(GL_BLEND);qglColor4f(1,1,1,1);qglDepthMask(1);qglEnable(GL_TEXTURE_2D);}
 /* the view weapon (R_DrawAliasModel with RF_DEPTHHACK): depth range squeezed, MODULATE, smooth vertex colours, a fan in eye space */
 {static const GLfloat wv2[4][3]={{1.5f,-2.6f,-5},{3.2f,-2.6f,-5},{3.2f,-1.4f,-6},{1.5f,-1.4f,-6}};
  qglPushMatrix();qglLoadIdentity();qglDepthRange(0,0.3);GL_TexEnv(GL_MODULATE);qglShadeModel(GL_SMOOTH);GL_Bind(TEX_IMG+1);
  qglBegin(GL_TRIANGLE_FAN);for(i=0;i<4;i++){qglColor4f(0.3f+i*0.15f,0.3f,0.25f,1);qglTexCoord2f((GLfloat)(i&1),(GLfloat)(i>>1));qglVertex3fv(wv2[i]);}qglEnd();
  GL_TexEnv(GL_REPLACE);qglShadeModel(GL_FLAT);qglDepthRange(0,1);qglPopMatrix();qglColor4f(1,1,1,1);}
 if(menu&128){ /* R_PolyBlend: the damage flash, a quad in front of the eye with no depth test */
  static const GLfloat bq[4][3]={{10,100,100},{10,-100,100},{10,-100,-100},{10,100,-100}};
  qglDisable(GL_ALPHA_TEST);qglEnable(GL_BLEND);qglDisable(GL_DEPTH_TEST);qglDisable(GL_TEXTURE_2D);qglLoadIdentity();qglRotatef(-90,1,0,0);qglRotatef(90,0,0,1);
  qglColor4f(1,0,0,0.3f);qglBegin(GL_QUADS);for(i=0;i<4;i++)qglVertex3fv(bq[i]);qglEnd();
  qglDisable(GL_BLEND);qglEnable(GL_TEXTURE_2D);qglEnable(GL_ALPHA_TEST);qglColor4f(1,1,1,1);}
 /* 2D: the HUD */
 set2d();GL_Bind(TEX_IMG);qglBegin(GL_QUADS);qglTexCoord2f(0,0);qglVertex2f(10,440);qglTexCoord2f(1,0);qglVertex2f(42,440);qglTexCoord2f(1,1);qglVertex2f(42,472);qglTexCoord2f(0,1);qglVertex2f(10,472);qglEnd();
 /* the menu's parts */
 if(menu&1){ /* Draw_FadeScreen */
  qglEnable(GL_BLEND);qglDisable(GL_TEXTURE_2D);qglColor4f(0,0,0,0.8f);qglBegin(GL_QUADS);qglVertex2f(0,0);qglVertex2f(W,0);qglVertex2f(W,H);qglVertex2f(0,H);qglEnd();
  qglColor4f(1,1,1,1);qglEnable(GL_TEXTURE_2D);qglDisable(GL_BLEND);}
 if(menu&(2|256)){ /* menu pictures loaded (the first menu frame): GL_LoadPic, no mipmaps, alpha */
  if(!pic_loaded||(menu&16)){pic_loaded=1;upload(TEX_IMG+10,pic,128,64,4,0);}}
 if(menu&4){ /* the scrap rewritten (Scrap_Upload: the whole block again, small pictures) */
  int k;for(k=0;k<64*64;k++){scrap[k*4]=200;scrap[k*4+1]=(GLubyte)k;scrap[k*4+2]=50;scrap[k*4+3]=255;}upload(TEX_SCRAP,scrap,256,256,4,0);}
 if(menu&256){ /* the console: its background picture over the top half, then rows of characters (conchars, alpha) */
  int r,cx;GL_Bind(TEX_IMG+10);qglBegin(GL_QUADS);qglTexCoord2f(0,0);qglVertex2f(0,-240);qglTexCoord2f(1,0);qglVertex2f(W,-240);qglTexCoord2f(1,1);qglVertex2f(W,240);qglTexCoord2f(0,1);qglVertex2f(0,240);qglEnd();
  GL_Bind(TEX_IMG);for(r=0;r<28;r++)for(cx=0;cx<40;cx++){GLfloat x=(GLfloat)(cx*8),y=(GLfloat)(r*8);qglBegin(GL_QUADS);qglTexCoord2f(0,0);qglVertex2f(x,y);qglTexCoord2f(0.25f,0);qglVertex2f(x+8,y);qglTexCoord2f(0.25f,0.25f);qglVertex2f(x+8,y+8);qglTexCoord2f(0,0.25f);qglVertex2f(x,y+8);qglEnd();}}
 if(menu&8){ /* Draw_Pic of the menu pictures and of the scrap */
  GL_Bind(TEX_IMG+10);qglBegin(GL_QUADS);qglTexCoord2f(0,0);qglVertex2f(200,150);qglTexCoord2f(1,0);qglVertex2f(456,150);qglTexCoord2f(1,1);qglVertex2f(456,278);qglTexCoord2f(0,1);qglVertex2f(200,278);qglEnd();
  GL_Bind(TEX_SCRAP);qglBegin(GL_QUADS);qglTexCoord2f(0,0);qglVertex2f(160,150);qglTexCoord2f(0.1f,0);qglVertex2f(182,150);qglTexCoord2f(0.1f,0.1f);qglVertex2f(182,179);qglTexCoord2f(0,0.1f);qglVertex2f(160,179);qglEnd();}
 if(!(menu&64)||(menu&2048))frame_no++; /* paused: the same frame again (Quake keeps the refdef) */
}
#ifndef Q2MENU_HOST
static LRESULT CALLBACK wp(HWND w,UINT m,WPARAM a,LPARAM b){return DefWindowProcA(w,m,a,b);}
static void pump(void){MSG m;while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}}
#endif
/* the world's mean colour: rows 100-380 (above the HUD), every 8th pixel; read from the back buffer after the frame */
static void world_colour(double *c) {
 static GLubyte row[W*4];int y,x;double s[3]={0,0,0};long n=0;
 qglFinish();
 for(y=100;y<380;y+=8){qglReadPixels(0,y,W,1,GL_RGBA,GL_UNSIGNED_BYTE,row);for(x=0;x<W;x+=8){s[0]+=row[x*4];s[1]+=row[x*4+1];s[2]+=row[x*4+2];n++;}}
 c[0]=s[0]/n;c[1]=s[1]/n;c[2]=s[2]/n;
}
#ifdef Q2MENU_HOST
/* the host's cases: begin makes a context (engine on or off), swap shows the back buffer, end destroys it; the world's colour after each case */
static double host_res[2][24][6];
static int host_case(int menu,int ci,int pass,int (*begin)(void),void (*swap)(void),void (*end)(void),void *(*gp)(const char*)) {
 int i;double a[3],b[3];
 if(!begin())return 1;
 qglPointParameterfEXT=(pp_t)gp("glPointParameterfEXT");qglPointParameterfvEXT=(ppv_t)gp("glPointParameterfvEXT");
 if(menu&32){qglPointParameterfEXT=0;qglPointParameterfvEXT=0;}
 cur_tex=-1;frame_no=0;pic_loaded=0;default_state();
 upload(TEX_IMG,hud,32,32,4,0);upload(TEX_IMG+1,wall,64,64,3,1);
 {int x;for(x=0;x<64*64*4;x+=4){GLubyte t2=wall[x];wall[x]=wall[x+2];wall[x+2]=t2;}upload(TEX_IMG+2,wall,64,64,3,1);for(x=0;x<64*64*4;x+=4){GLubyte t2=wall[x];wall[x]=wall[x+2];wall[x+2]=t2;}}
 {static GLubyte z[128*128*4];GL_Bind(TEX_LM);qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);qglTexImage2D(GL_TEXTURE_2D,0,3,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,z);}
 GL_Bind(TEX_LM+1);qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);qglTexImage2D(GL_TEXTURE_2D,0,3,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,lmap);
 for(i=0;i<8;i++){frame(((menu&512)?128:0)|(menu&(2048|8192)));if(i==7)world_colour(a);swap();}
 for(i=0;i<8;i++){frame((menu&511)|(menu&(2048|8192)));swap();}
 for(i=0;i<8;i++){frame(((menu&512)?128:0)|(menu&(2048|8192)));if(i==7)world_colour(b);swap();}
 for(i=0;i<3;i++){host_res[pass][ci][i]=a[i];host_res[pass][ci][3+i]=b[i];}
 end();
 return 0;
}
static int (*g_begin)(int);static int g_pass;static int bw(void){return g_begin(g_pass);}
int q2menu_host(FILE *f,int (*begin)(int),void (*swap)(void),void (*end)(void),void *(*gp)(const char*)) {
 static const int m[]={0,1|2|4|8,1,2,2|8,4,4|8,1|2|4|8|32,2|16,64,64|256,64|1|2|4|8,128,64|256|128,64|256|128|512,2048,2048|1|2|4|8,8192|2048,8192|2048|1|2|4|8};int k,pass,fails=0;const int NC=19;
 make_data();make_world();
 g_begin=begin;
 for(pass=0;pass<2;pass++){g_pass=pass;for(k=0;k<NC;k++)if(host_case(m[k],k,pass,bw,swap,end,gp))return 99;}
 for(k=0;k<NC;k++){double *e=host_res[0][k],*s0=host_res[1][k];int i,bad=0;
  for(i=0;i<6;i++)if(e[i]-s0[i]>8||s0[i]-e[i]>8)bad=1;
  for(i=0;i<3;i++)if(e[3+i]-e[i]>20||e[i]-e[3+i]>20)bad=1;
  fprintf(f,"  Q2MENU case %d: engine %5.1f %5.1f %5.1f -> %5.1f %5.1f %5.1f, software %5.1f %5.1f %5.1f -> %5.1f %5.1f %5.1f%s\n",k,e[0],e[1],e[2],e[3],e[4],e[5],s0[0],s0[1],s0[2],s0[3],s0[4],s0[5],bad?"  FAIL":"");
  fails+=bad;}
 return fails;
}
#else
/* Quake's mouse: in game the cursor is hidden, captured and clipped to the window (IN_ActivateMouse); the console or the menu gives it
 * back (IN_DeactivateMouse) */
static void mouse_game(HWND w) {RECT r;POINT c;GetWindowRect(w,&r);c.x=(r.left+r.right)/2;c.y=(r.top+r.bottom)/2;SetCursorPos(c.x,c.y);SetCapture(w);ClipCursor(&r);while(ShowCursor(FALSE)>=0);}
static void mouse_free(void) {ClipCursor(NULL);ReleaseCapture();while(ShowCursor(TRUE)<0);}
static int run(int menu,const char *name) {
 WNDCLASSA wc;HWND w;HDC dc;HGLRC rc;PIXELFORMATDESCRIPTOR pfd;int pf,i;double a[3],b[3],d[3];int bad;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="Q2MENU";wc.hCursor=LoadCursor(0,IDC_ARROW);RegisterClassA(&wc);
 {RECT r={0,0,W,H};AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION,FALSE);w=CreateWindowExA(0,"Q2MENU","Q2MENU",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,r.right-r.left,r.bottom-r.top,0,0,wc.hInstance,0);}
 dc=GetDC(w);memset(&pfd,0,sizeof(pfd));pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=24;pfd.cDepthBits=32;pfd.iLayerType=PFD_MAIN_PLANE;
 SetEnvironmentVariableA("VXGL_HW",(menu&4096)?"0":"1"); /* 4096: software (the reference) */
 pf=ChoosePixelFormat(dc,&pfd);SetPixelFormat(dc,pf,&pfd);rc=qwglCreateContext(dc);qwglMakeCurrent(dc,rc);SetEnvironmentVariableA("VXGL_HW",0);
 qglPointParameterfEXT=(pp_t)qwglGetProcAddress("glPointParameterfEXT");qglPointParameterfvEXT=(ppv_t)qwglGetProcAddress("glPointParameterfvEXT");
 if(menu&32){qglPointParameterfEXT=0;qglPointParameterfvEXT=0;}
 cur_tex=-1;frame_no=0;pic_loaded=0;default_state();
 upload(TEX_IMG,hud,32,32,4,0);upload(TEX_IMG+1,wall,64,64,3,1);
 {int x;for(x=0;x<64*64*4;x+=4){GLubyte t2=wall[x];wall[x]=wall[x+2];wall[x+2]=t2;}upload(TEX_IMG+2,wall,64,64,3,1);for(x=0;x<64*64*4;x+=4){GLubyte t2=wall[x];wall[x]=wall[x+2];wall[x+2]=t2;}}
 /* R_BeginBuildingLightmaps: the dynamic one (1024) made empty, then the static blocks with glTexImage2D */
 {static GLubyte z[128*128*4];GL_Bind(TEX_LM);qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);qglTexImage2D(GL_TEXTURE_2D,0,3,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,z);}
 GL_Bind(TEX_LM+1);qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);qglTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);qglTexImage2D(GL_TEXTURE_2D,0,3,128,128,0,GL_RGBA,GL_UNSIGNED_BYTE,lmap);
 if(menu&1024)mouse_game(w);
 for(i=0;i<40;i++){frame(((menu&512)?128:0)|(menu&(2048|8192)));if(i==39)world_colour(a);qwglSwapBuffers(dc);pump();}
 if(menu&1024)mouse_free();
 for(i=0;i<40;i++){frame((menu&511)|(menu&(2048|8192)));qwglSwapBuffers(dc);pump();}
 if(menu&1024)mouse_game(w);
 for(i=0;i<40;i++){frame(((menu&512)?128:0)|(menu&(2048|8192)));if(i==39)world_colour(b);qwglSwapBuffers(dc);pump();}
 if(menu&1024)mouse_free();
 for(i=0;i<3;i++)d[i]=b[i]-a[i];
 bad=d[0]<-20||d[1]<-20||d[2]<-20||d[0]>20||d[1]>20||d[2]>20;
 out("%-44s world before %5.1f %5.1f %5.1f, after %5.1f %5.1f %5.1f: %s (GL error %04X)\n",name,a[0],a[1],a[2],b[0],b[1],b[2],bad?"CHANGED":"same",qglGetError());
 qwglMakeCurrent(0,0);qwglDeleteContext(rc);ReleaseDC(w,dc);DestroyWindow(w);pump();
 return bad;
}
int main(void) {
 char path[MAX_PATH],*e;SYSTEMTIME t;
 GetModuleFileNameA(0,path,sizeof(path));e=strrchr(path,'\\');if(e)strcpy(e+1,"Q2MENU.TXT");else strcpy(path,"Q2MENU.TXT");
 lf=fopen(path,"a");GetLocalTime(&t);
 out("\n==== Q2MENU v1.4 %04u-%02u-%02u %02u:%02u:%02u ====\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond);
 if(!load()){out("opengl32 not loaded\n");if(lf)fclose(lf);return 1;}
 make_data();make_world();
 {static const struct {int m;const char *n;} c[]={
   {0,"0: no menu (the reference)"},
   {1|2|4|8,"1: the whole menu"},
   {1,"2: Draw_FadeScreen only"},
   {2,"3: a new picture texture loaded only"},
   {2|8,"4: picture loaded and drawn"},
   {4,"5: the scrap rewritten only"},
   {4|8,"6: scrap rewritten and drawn"},
   {1|2|4|8|32,"7: the whole menu, no point parameters"},
   {2|16,"8: a picture texture loaded every frame"},
   {64,"9: paused (the same frame again)"},
   {64|256,"10: paused with the console"},
   {64|1|2|4|8,"11: paused with the menu"},
   {128,"12: the damage flash every frame"},
   {64|256|128,"13: paused, console and damage flash"},
   {64|256|128|512,"14: as 13, then the flash again after"},
   {1024,"15: the mouse given back and taken again only"},
   {1024|64|256|128|512,"16: the console as in a game (mouse, pause, flash)"},
   {2048|4096,"17: camera stepping back, depth cleared each frame (SOFTWARE)"},
   {2048,"18: camera stepping back, depth cleared each frame (engine)"},
   {2048|1|2|4|8|4096,"19: as 17 with the menu (SOFTWARE)"},
   {2048|1|2|4|8,"20: as 18 with the menu (engine)"},
   {8192|2048|4096,"21: Quake's load (3200 draws a frame), camera moving (SOFTWARE)"},
   {8192|2048,"22: Quake's load (3200 draws a frame), camera moving (engine)"},
   {8192|2048|1|2|4|8|4096,"23: as 21 with the menu (SOFTWARE)"},
   {8192|2048|1|2|4|8,"24: as 22 with the menu (engine)"},
  };int k,n=0;
  for(k=0;k<(int)(sizeof(c)/sizeof(c[0]));k++){
   if(k==1){/* the renderer, once a context exists */}
   n+=run(c[k].m,c[k].n);
   if(k==0){HDC d=0;(void)d;}
  }
  out("%d of %d cases changed the world\n",n,(int)(sizeof(c)/sizeof(c[0])));}
 if(lf)fclose(lf);
 MessageBoxA(0,"Q2MENU is done: Q2MENU.TXT is next to the program.","Q2MENU",MB_OK|MB_TOPMOST);
 return 0;
}
#endif
