/* VXTEX v1.3 (driver v10.274): an OpenGL texture test with the answers worked out in advance, as VXSTEN for the stencil. Every case
 * draws in a 256x256 window (pixel coordinates: glOrtho(0,256,0,256,-1,1)) and reads colours back at pixels whose texture coordinate
 * is known exactly: texel centres, the middle of magnified texels, the inside of a bilinear cell, so no rasterization rule decides.
 * Cases: texel upload (nearest, 1:1 and magnified), internal formats (RGBA8 RGB8 RGB5 RGBA4 L8 L8A8 A8 I8), source formats and unpack
 * state (BGRA, RGB, luminance, 5_6_5, 4_4_4_4, 8_8_8_8_REV, row length and skips), the texture environments (REPLACE MODULATE DECAL
 * BLEND ADD on every base format, colour and alpha), wrap modes (REPEAT CLAMP CLAMP_TO_EDGE MIRRORED_REPEAT), bilinear filtering,
 * mipmap selection (sizes, base and max level, min LOD, LOD bias, between two levels), incomplete textures, generated mipmaps,
 * TexSubImage / CopyTexImage / CopyTexSubImage, texture objects, multitexture, env combine and dot3, paletted textures (own and the
 * shared palette, as Quake 2), texgen and the texture matrix, cube maps and 3D textures, alpha-tested cut-outs and alpha blending,
 * perspective correction; and three timings (a bind and a tiny triangle, as glean's texBindPerf; textured fill; small textured quads
 * each in its own glBegin/glEnd with a bind every 8, as Quake 2 draws). Runs: the VX855 ICD with the engine, the ICD with the engine
 * off (ACCEL_OPENGL_HW = 0 for that context, put back afterwards), and Microsoft's software OpenGL as a reference (OpenGL 1.1: most
 * cases are skipped there; its failures are not counted). Then the engine's images are compared with the engine-off ones. Log
 * VXTEX.TXT; a case that differs writes VXTEX_nn_HW.BMP and VXTEX_nn_SW.BMP. "VXTEX n" scales the timings (default 1). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <GL/gl.h>

#define W 256
#define H 256
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#ifndef GL_MIRRORED_REPEAT
#define GL_MIRRORED_REPEAT 0x8370
#endif
#ifndef GL_TEXTURE_BASE_LEVEL
#define GL_TEXTURE_MIN_LOD 0x813A
#define GL_TEXTURE_MAX_LOD 0x813B
#define GL_TEXTURE_BASE_LEVEL 0x813C
#define GL_TEXTURE_MAX_LEVEL 0x813D
#endif
#ifndef GL_TEXTURE_FILTER_CONTROL
#define GL_TEXTURE_FILTER_CONTROL 0x8500
#define GL_TEXTURE_LOD_BIAS 0x8501
#endif
#ifndef GL_GENERATE_MIPMAP
#define GL_GENERATE_MIPMAP 0x8191
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0 0x84C0
#define GL_TEXTURE1 0x84C1
#define GL_MAX_TEXTURE_UNITS 0x84E2
#endif
#ifndef GL_COMBINE
#define GL_COMBINE 0x8570
#define GL_COMBINE_RGB 0x8571
#define GL_COMBINE_ALPHA 0x8572
#define GL_RGB_SCALE 0x8573
#define GL_ADD_SIGNED 0x8574
#define GL_INTERPOLATE 0x8575
#define GL_CONSTANT 0x8576
#define GL_PRIMARY_COLOR 0x8577
#define GL_PREVIOUS 0x8578
#define GL_SOURCE0_RGB 0x8580
#define GL_SOURCE1_RGB 0x8581
#define GL_SOURCE2_RGB 0x8582
#define GL_SOURCE0_ALPHA 0x8588
#define GL_SOURCE1_ALPHA 0x8589
#define GL_OPERAND0_RGB 0x8590
#define GL_OPERAND1_RGB 0x8591
#define GL_OPERAND2_RGB 0x8592
#define GL_OPERAND0_ALPHA 0x8598
#define GL_OPERAND1_ALPHA 0x8599
#define GL_SUBTRACT 0x84E7
#endif
#ifndef GL_DOT3_RGB
#define GL_DOT3_RGB 0x86AE
#endif
#ifndef GL_COLOR_INDEX8_EXT
#define GL_COLOR_INDEX8_EXT 0x80E5
#endif
#ifndef GL_SHARED_TEXTURE_PALETTE_EXT
#define GL_SHARED_TEXTURE_PALETTE_EXT 0x81FB
#endif
#ifndef GL_TEXTURE_CUBE_MAP
#define GL_TEXTURE_CUBE_MAP 0x8513
#define GL_TEXTURE_CUBE_MAP_POSITIVE_X 0x8515
#endif
#ifndef GL_TEXTURE_3D
#define GL_TEXTURE_3D 0x806F
#define GL_TEXTURE_WRAP_R 0x8072
#endif
#ifndef GL_BGRA
#define GL_BGRA 0x80E1
#endif
#ifndef GL_UNSIGNED_SHORT_5_6_5
#define GL_UNSIGNED_SHORT_5_6_5 0x8363
#define GL_UNSIGNED_SHORT_4_4_4_4 0x8033
#define GL_UNSIGNED_INT_8_8_8_8_REV 0x8367
#endif
#ifndef GL_PERSPECTIVE_CORRECTION_HINT
#define GL_PERSPECTIVE_CORRECTION_HINT 0x0C50
#endif

typedef void (APIENTRY *at_fn)(GLenum);
typedef void (APIENTRY *mtc_fn)(GLenum,GLfloat,GLfloat);
typedef void (APIENTRY *ti3_fn)(GLenum,GLint,GLint,GLsizei,GLsizei,GLsizei,GLint,GLenum,GLenum,const GLvoid*);
typedef void (APIENTRY *ct_fn)(GLenum,GLenum,GLsizei,GLenum,GLenum,const GLvoid*);
static at_fn pActiveTexture;static mtc_fn pMultiTexCoord2f;static ti3_fn pTexImage3D;static ct_fn pColorTable;
static int gl12,gl13,gl14,has_genmip,has_lodbias,has_add,has_combine,has_dot3,has_cube,has_mirror,has_pal,has_shpal,has_bgra,max_units;
static void *(*getproc)(const char*);
static FILE *logf;
static int which,failures,case_fail,case_skip,scale=1,fbtol=1,cmp_tol=16;
static HDC gdc;
#define NC 24
static unsigned char cimg[3][NC][W*H*4];static int shave[3][NC];
static double tms[3][3];

#ifdef VXTEX_HOST /* the host test (gl/host/vxgl_host.c) runs the cases against Mesa with the engine model and without it */
#define SwapBuffers(d) ((void)(d))
DWORD GetTickCount(void);
static double now_ms(void){return (double)GetTickCount();}
#else
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcA(h,m,w,l);}
static void *wgl_proc(const char *n){return (void*)wglGetProcAddress(n);}
static double now_ms(void){LARGE_INTEGER f,c;if(QueryPerformanceFrequency(&f)&&f.QuadPart){QueryPerformanceCounter(&c);return (double)c.QuadPart*1000.0/(double)f.QuadPart;}return (double)GetTickCount();}
#endif

/* ------------------------------------------------------------------ helpers */
static void bad(const char *fmt,...) {va_list a;case_fail++;fprintf(logf,"     FAIL ");va_start(a,fmt);vfprintf(logf,fmt,a);va_end(a);fprintf(logf,"\n");}
static void skip(const char *what) {case_skip++;fprintf(logf,"     (%s not offered: skipped)\n",what);}
static int clampi(int v){return v<0?0:v>255?255:v;}
static int f2b(double v){return clampi((int)(v*255.0+0.5));}
/* the colour at x,y within tol (plus the framebuffer's own rounding) of r,g,b; a channel of -1 is not checked */
static void want_rgb(int x,int y,int r,int g,int b,int tol,const char *what) {
 GLubyte p[4];int t=tol+fbtol;
 glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);
 if((r>=0&&abs(p[0]-r)>t)||(g>=0&&abs(p[1]-g)>t)||(b>=0&&abs(p[2]-b)>t))
  bad("%s: colour at %d,%d = %02X%02X%02X, want %02X%02X%02X (within %d)",what,x,y,p[0],p[1],p[2],clampi(r),clampi(g),clampi(b),t);
}
static void reset(void) {
 int u;
 glViewport(0,0,W,H);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,0,H,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 for(u=max_units>1?1:0;u>=0;u--) {
  if(pActiveTexture)pActiveTexture(GL_TEXTURE0+u);
  glMatrixMode(GL_TEXTURE);glLoadIdentity();glMatrixMode(GL_MODELVIEW);
  glDisable(GL_TEXTURE_2D);glDisable(GL_TEXTURE_1D);if(gl12)glDisable(GL_TEXTURE_3D);if(has_cube)glDisable(GL_TEXTURE_CUBE_MAP);
  glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);
  glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
  if(has_lodbias)glTexEnvf(GL_TEXTURE_FILTER_CONTROL,GL_TEXTURE_LOD_BIAS,0.0f);
  glBindTexture(GL_TEXTURE_2D,0);
 }
 if(has_shpal)glDisable(GL_SHARED_TEXTURE_PALETTE_EXT);
 glDisable(GL_DEPTH_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDisable(GL_DITHER);glDisable(GL_ALPHA_TEST);
 glDisable(GL_LIGHTING);glDisable(GL_FOG);glShadeModel(GL_SMOOTH);glColorMask(1,1,1,1);
 glPixelStorei(GL_UNPACK_ALIGNMENT,1);glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
 glPixelStorei(GL_PACK_ALIGNMENT,1);glReadBuffer(GL_BACK);
 glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 glColor4f(1,1,1,1);glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_NICEST);
}
/* a new texture object with nearest filtering and repeat, bound to unit 0's 2D target */
static GLuint newtex(void) {
 GLuint t=0;glGenTextures(1,&t);glBindTexture(GL_TEXTURE_2D,t);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
 return t;
}
/* a quad on integer edges (pixels x0..x1-1, y0..y1-1) with texture coordinates s0..s1, t0..t1 */
static void tquad(int x0,int y0,int x1,int y1,float s0,float t0,float s1,float t1) {
 glBegin(GL_QUADS);
 glTexCoord2f(s0,t0);glVertex2f((float)x0,(float)y0);glTexCoord2f(s1,t0);glVertex2f((float)x1,(float)y0);
 glTexCoord2f(s1,t1);glVertex2f((float)x1,(float)y1);glTexCoord2f(s0,t1);glVertex2f((float)x0,(float)y1);
 glEnd();
}
static void rectc(int x0,int y0,int x1,int y1) {glBegin(GL_QUADS);glVertex2f((float)x0,(float)y0);glVertex2f((float)x1,(float)y0);glVertex2f((float)x1,(float)y1);glVertex2f((float)x0,(float)y1);glEnd();}
static void deltex(GLuint t){glDeleteTextures(1,&t);}

/* ------------------------------------------------------------------ cases */
/* 8x8 texels, each its own colour */
static void code8(GLubyte *p) {int i,j;for(j=0;j<8;j++)for(i=0;i<8;i++){GLubyte *q=p+(j*8+i)*4;q[0]=(GLubyte)(i*32+16);q[1]=(GLubyte)(j*32+16);q[2]=(GLubyte)((i^j)*16+8);q[3]=255;}}
static void c_upload(void) {
 GLubyte img[8*8*4];GLuint t;int i,j;char m[64];
 code8(img);t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,img);
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 tquad(8,8,16,16,0,0,1,1);tquad(40,8,72,40,0,0,1,1);
 for(j=0;j<8;j++)for(i=0;i<8;i++){const GLubyte *q=img+(j*8+i)*4;
  sprintf(m,"texel %d,%d 1:1",i,j);want_rgb(8+i,8+j,q[0],q[1],q[2],0,m);
  sprintf(m,"texel %d,%d magnified 4x",i,j);want_rgb(40+4*i+2,8+4*j+2,q[0],q[1],q[2],0,m);}
 deltex(t);
}
/* the 2x2 source: texel (0,0) (1,0) (0,1) (1,1) */
static const GLubyte src4[16]={200,100,50,255, 30,220,140,128, 90,60,250,64, 255,255,255,0};
static void c_formats(void) {
 static const struct {GLenum f;const char *n;int kind,tol;} fm[8]={{GL_RGBA8,"RGBA8",0,0},{GL_RGB8,"RGB8",1,0},{GL_RGB5,"RGB5",1,9},{GL_RGBA4,"RGBA4",0,18},
  {GL_LUMINANCE8,"LUMINANCE8",2,0},{GL_LUMINANCE8_ALPHA8,"LUMINANCE8_ALPHA8",3,0},{GL_ALPHA8,"ALPHA8",4,0},{GL_INTENSITY8,"INTENSITY8",5,0}};
 int k,i,pass;char m[96];
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glColor4ub(255,255,255,255);
 for(pass=0;pass<2;pass++){ /* 0 the colour; 1 the colour times the alpha (blend SRC_ALPHA, ZERO over black) */
  if(pass){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ZERO);}
  for(k=0;k<8;k++){GLuint t=newtex();int x0=8+k*30,y0=pass?60:8;
   glTexImage2D(GL_TEXTURE_2D,0,fm[k].f,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,src4);tquad(x0,y0,x0+16,y0+16,0,0,1,1);
   for(i=0;i<4;i++){const GLubyte *q=src4+i*4;int c[3],a,ch,px=x0+(i&1)*8+4,py=y0+(i>>1)*8+4;
    switch(fm[k].kind){case 0:c[0]=q[0];c[1]=q[1];c[2]=q[2];a=q[3];break;case 1:c[0]=q[0];c[1]=q[1];c[2]=q[2];a=255;break;
     case 2:c[0]=c[1]=c[2]=q[0];a=255;break;case 3:c[0]=c[1]=c[2]=q[0];a=q[3];break;case 4:c[0]=c[1]=c[2]=255;a=q[3];break;default:c[0]=c[1]=c[2]=a=q[0];}
    if(pass)for(ch=0;ch<3;ch++)c[ch]=(c[ch]*a+127)/255;
    sprintf(m,"%s texel %d%s",fm[k].n,i,pass?" (colour x alpha)":"");want_rgb(px,py,c[0],c[1],c[2],fm[k].tol*(pass?2:1)+(pass?2:0),m);}
   deltex(t);}
 }
}
static void c_sources(void) {
 GLubyte bgra[16],rgb[12],lum[4],big[4*4*4];GLushort s565[4],s4444[4];GLuint t;int i;char m[64];
 static const int want565[4][3]={{255,0,0},{0,255,0},{0,0,255},{132,130,132}};
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 for(i=0;i<4;i++){bgra[i*4]=src4[i*4+2];bgra[i*4+1]=src4[i*4+1];bgra[i*4+2]=src4[i*4];bgra[i*4+3]=src4[i*4+3];rgb[i*3]=src4[i*4];rgb[i*3+1]=src4[i*4+1];rgb[i*3+2]=src4[i*4+2];lum[i]=src4[i*4];}
 /* RGB, 2 texels a row: 6 bytes, unpack alignment 1 */
 t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,2,2,0,GL_RGB,GL_UNSIGNED_BYTE,rgb);tquad(8,8,24,24,0,0,1,1);
 for(i=0;i<4;i++){sprintf(m,"GL_RGB source texel %d",i);want_rgb(8+(i&1)*8+4,8+(i>>1)*8+4,src4[i*4],src4[i*4+1],src4[i*4+2],0,m);}deltex(t);
 /* luminance source into RGB8 */
 t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,2,2,0,GL_LUMINANCE,GL_UNSIGNED_BYTE,lum);tquad(40,8,56,24,0,0,1,1);
 for(i=0;i<4;i++){sprintf(m,"GL_LUMINANCE source texel %d",i);want_rgb(40+(i&1)*8+4,8+(i>>1)*8+4,lum[i],lum[i],lum[i],0,m);}deltex(t);
 if(has_bgra){t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_BGRA,GL_UNSIGNED_BYTE,bgra);tquad(72,8,88,24,0,0,1,1);
  for(i=0;i<4;i++){sprintf(m,"GL_BGRA source texel %d",i);want_rgb(72+(i&1)*8+4,8+(i>>1)*8+4,src4[i*4],src4[i*4+1],src4[i*4+2],0,m);}deltex(t);}
 else skip("GL_EXT_bgra");
 if(gl12) {
  s565[0]=0xF800;s565[1]=0x07E0;s565[2]=0x001F;s565[3]=0x8410;
  t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,2,2,0,GL_RGB,GL_UNSIGNED_SHORT_5_6_5,s565);tquad(104,8,120,24,0,0,1,1);
  for(i=0;i<4;i++){sprintf(m,"GL_UNSIGNED_SHORT_5_6_5 texel %d",i);want_rgb(104+(i&1)*8+4,8+(i>>1)*8+4,want565[i][0],want565[i][1],want565[i][2],2,m);}deltex(t);
  s4444[0]=0xF00F;s4444[1]=0x0F0F;s4444[2]=0x00FF;s4444[3]=0x888F;
  t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_SHORT_4_4_4_4,s4444);tquad(136,8,152,24,0,0,1,1);
  want_rgb(140,12,255,0,0,1,"GL_UNSIGNED_SHORT_4_4_4_4 texel 0");want_rgb(148,12,0,255,0,1,"GL_UNSIGNED_SHORT_4_4_4_4 texel 1");
  want_rgb(140,20,0,0,255,1,"GL_UNSIGNED_SHORT_4_4_4_4 texel 2");want_rgb(148,20,136,136,136,1,"GL_UNSIGNED_SHORT_4_4_4_4 texel 3");deltex(t);
  t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_BGRA,GL_UNSIGNED_INT_8_8_8_8_REV,bgra);tquad(168,8,184,24,0,0,1,1);
  for(i=0;i<4;i++){sprintf(m,"GL_BGRA 8_8_8_8_REV texel %d",i);want_rgb(168+(i&1)*8+4,8+(i>>1)*8+4,src4[i*4],src4[i*4+1],src4[i*4+2],0,m);}deltex(t);
 } else skip("OpenGL 1.2 packed pixel types");
 /* unpack state: a 2x2 piece of a 4x4 image (row length 4, skip 1 pixel and 1 row) */
 for(i=0;i<16;i++){big[i*4]=(GLubyte)(i*16);big[i*4+1]=(GLubyte)(255-i*16);big[i*4+2]=(GLubyte)(i*5);big[i*4+3]=255;}
 glPixelStorei(GL_UNPACK_ROW_LENGTH,4);glPixelStorei(GL_UNPACK_SKIP_PIXELS,1);glPixelStorei(GL_UNPACK_SKIP_ROWS,1);
 t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,big);
 glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);
 tquad(8,40,24,56,0,0,1,1);
 for(i=0;i<4;i++){int s=(1+(i>>1))*4+1+(i&1);sprintf(m,"row length 4, skip 1,1: texel %d (source %d)",i,s);want_rgb(8+(i&1)*8+4,40+(i>>1)*8+4,big[s*4],big[s*4+1],big[s*4+2],0,m);}
 deltex(t);
}
/* the texture environments: fragment colour Cf, environment colour Cc, one texel per base format */
static void env_expect(GLenum mode,int kind,const double *cf,const double *ct,double at,const double *cc,double *o) {
 /* kind: 0 RGBA, 1 RGB, 2 L, 3 LA, 4 A, 5 I (ct holds L or I in all three) */
 int c;double a=cf[3];
 for(c=0;c<3;c++)o[c]=cf[c];
 switch(mode) {
 case GL_REPLACE:
  if(kind==4){a=at;break;}
  for(c=0;c<3;c++)o[c]=ct[c];
  a=kind==0||kind==3?at:kind==5?ct[0]:cf[3];break;
 case GL_MODULATE:
  if(kind!=4)for(c=0;c<3;c++)o[c]=cf[c]*ct[c];
  a=kind==0||kind==3||kind==4?cf[3]*at:kind==5?cf[3]*ct[0]:cf[3];break;
 case GL_DECAL: /* RGB and RGBA only */
  for(c=0;c<3;c++)o[c]=kind==1?ct[c]:cf[c]*(1-at)+ct[c]*at;a=cf[3];break;
 case GL_BLEND:
  if(kind!=4)for(c=0;c<3;c++)o[c]=cf[c]*(1-ct[c])+cc[c]*ct[c];
  a=kind==0||kind==3||kind==4?cf[3]*at:kind==5?cf[3]*(1-ct[0])+cc[3]*ct[0]:cf[3];break;
 default: /* GL_ADD */
  if(kind!=4)for(c=0;c<3;c++)o[c]=cf[c]+ct[c];
  a=kind==0||kind==3||kind==4?cf[3]*at:kind==5?cf[3]+ct[0]:cf[3];break;
 }
 for(c=0;c<3;c++)o[c]=o[c]>1?1:o[c];o[3]=a>1?1:a;
}
static void c_env(void) {
 static const GLenum modes[5]={GL_REPLACE,GL_MODULATE,GL_DECAL,GL_BLEND,GL_ADD};static const char *mn[5]={"REPLACE","MODULATE","DECAL","BLEND","ADD"};
 static const GLenum fmts[6]={GL_RGBA8,GL_RGB8,GL_LUMINANCE8,GL_LUMINANCE8_ALPHA8,GL_ALPHA8,GL_INTENSITY8};static const char *fn[6]={"RGBA","RGB","L","LA","A","I"};
 static const GLubyte tex[4]={100,200,30,180};static const GLfloat ccf[4]={40/255.f,200/255.f,120/255.f,64/255.f};
 double cf[4]={160/255.,80/255.,240/255.,128/255.},cc[4],ct[3],o[4];int m,f,c,pass;char msg[96];
 for(c=0;c<4;c++)cc[c]=ccf[c];
 glEnable(GL_TEXTURE_2D);glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_COLOR,ccf);glColor4ub(160,80,240,128);
 for(pass=0;pass<2;pass++) {
  if(pass){glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ZERO);}else glDisable(GL_BLEND);
  for(f=0;f<6;f++){GLuint t=newtex();glTexImage2D(GL_TEXTURE_2D,0,fmts[f],1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,tex);
   for(m=0;m<5;m++){int x0=8+m*24,y0=8+f*20+pass*124;
    if(m==2&&f>1)continue; /* DECAL is undefined for these formats */
    if(m==4&&!has_add){if(f==0&&pass==0)skip("GL_EXT_texture_env_add");continue;}
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,(GLint)modes[m]);tquad(x0,y0,x0+16,y0+16,0,0,1,1);
    if(f<2){ct[0]=tex[0]/255.;ct[1]=tex[1]/255.;ct[2]=tex[2]/255.;}else ct[0]=ct[1]=ct[2]=tex[0]/255.;
    env_expect(modes[m],f,cf,ct,tex[3]/255.,cc,o);
    if(pass)for(c=0;c<3;c++)o[c]*=o[3];
    sprintf(msg,"%s on %s%s",mn[m],fn[f],pass?" (colour x alpha)":"");want_rgb(x0+8,y0+8,f2b(o[0]),f2b(o[1]),f2b(o[2]),pass?5:3,msg);}
   deltex(t);}
 }
}
/* 4x4 texels: red 40 + 60 u, green 40 + 60 v */
static void code4(GLubyte *p) {int u,v;for(v=0;v<4;v++)for(u=0;u<4;u++){GLubyte *q=p+(v*4+u)*4;q[0]=(GLubyte)(40+60*u);q[1]=(GLubyte)(40+60*v);q[2]=128;q[3]=255;}}
static int wrap_texel(GLenum mode,int p) {
 int m;
 if(mode==GL_REPEAT)return ((p%4)+4)%4;
 if(mode==GL_MIRRORED_REPEAT){m=((p%8)+8)%8;return m<4?m:7-m;}
 return p<0?0:p>3?3:p; /* CLAMP (nearest) and CLAMP_TO_EDGE */
}
static void c_wrap(void) {
 static const GLenum modes[4]={GL_REPEAT,GL_CLAMP,GL_CLAMP_TO_EDGE,GL_MIRRORED_REPEAT};static const char *mn[4]={"REPEAT","CLAMP","CLAMP_TO_EDGE","MIRRORED_REPEAT"};
 GLubyte img[64];int k,bx,by;char m[96];
 code4(img);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 for(k=0;k<4;k++){GLuint t;int x0=8+(k&1)*120,y0=8+(k>>1)*120;
  if(k==2&&!gl12){skip("GL_CLAMP_TO_EDGE (OpenGL 1.2)");continue;}
  if(k==3&&!has_mirror){skip("GL_MIRRORED_REPEAT");continue;}
  t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,img);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,(GLint)modes[k]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,(GLint)modes[k]);
  tquad(x0,y0,x0+96,y0+96,-1,-1,2,2); /* 12 texel steps of 8 pixels: step b is texel position b - 4 */
  for(by=0;by<12;by++)for(bx=0;bx<12;bx++){int u=wrap_texel(modes[k],bx-4),v=wrap_texel(modes[k],by-4);
   sprintf(m,"%s at texel position %d,%d",mn[k],bx-4,by-4);want_rgb(x0+bx*8+4,y0+by*8+4,40+60*u,40+60*v,128,0,m);}
  deltex(t);}
}
static void c_bilinear(void) {
 static const GLubyte tex[16]={0,0,0,255, 255,0,0,255, 0,255,0,255, 255,255,255,255};
 static const int at[6]={16,24,31,32,40,47};GLubyte img[256];GLuint t;int i,j,c;char m[96];
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 t=newtex();glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,tex);tquad(8,8,72,72,0,0,1,1);
 for(j=0;j<6;j++)for(i=0;i<6;i++){double u=(at[i]+0.5)/32.0-0.5,v=(at[j]+0.5)/32.0-0.5,w[4],o[3];
  w[0]=(1-u)*(1-v);w[1]=u*(1-v);w[2]=(1-u)*v;w[3]=u*v;
  for(c=0;c<3;c++)o[c]=(w[0]*tex[c]+w[1]*tex[4+c]+w[2]*tex[8+c]+w[3]*tex[12+c])/255.0;
  sprintf(m,"bilinear at %.3f,%.3f",u,v);want_rgb(8+at[i],8+at[j],f2b(o[0]),f2b(o[1]),f2b(o[2]),10,m);} /* v1.2: the chip's weights are 1/32 steps rounded to nearest (v1.1 probe): up to 255/64 a level along each axis */
 deltex(t);
 /* linear filtering 1:1 on texel centres: the texels themselves */
 code8(img);t=newtex();glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,img);tquad(100,8,108,16,0,0,1,1);
 for(j=1;j<7;j++)for(i=1;i<7;i++){const GLubyte *q=img+(j*8+i)*4;sprintf(m,"linear 1:1 texel %d,%d",i,j);want_rgb(100+i,8+j,q[0],q[1],q[2],1,m);}
 deltex(t);
}
/* mipmaps: 32x32 down to 1x1, level n one solid colour */
static const GLubyte lvcol[6][3]={{255,0,0},{0,255,0},{0,0,255},{255,255,0},{255,0,255},{0,255,255}};
static GLuint miptex(GLenum minf) {
 GLubyte img[32*32*4];int l,s,i;GLuint t=newtex();
 for(l=0,s=32;l<6;l++,s>>=1){for(i=0;i<s*s;i++){img[i*4]=lvcol[l][0];img[i*4+1]=lvcol[l][1];img[i*4+2]=lvcol[l][2];img[i*4+3]=255;}
  glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA8,s,s,0,GL_RGBA,GL_UNSIGNED_BYTE,img);}
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,(GLint)minf);return t;
}
static void want_level(int x,int y,int l,const char *what) {char m[96];sprintf(m,"%s: level %d",what,l);want_rgb(x,y,lvcol[l][0],lvcol[l][1],lvcol[l][2],0,m);}
static void c_mipmap(void) {
 GLuint t;int l,s,x;char m[64];
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 t=miptex(GL_NEAREST_MIPMAP_NEAREST);
 for(l=0,s=32,x=8;l<6;l++,x+=s+8,s>>=1){tquad(x,8,x+s,8+s,0,0,1,1);sprintf(m,"a %dx%d quad",s,s);want_level(x+s/2,8+s/2,l,m);}
 if(gl12) {
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,2);tquad(8,60,40,92,0,0,1,1);want_level(24,76,2,"base level 2, 32x32 (magnified)");
  tquad(50,60,52,62,0,0,1,1);want_level(50,60,4,"base level 2, 2x2");
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_BASE_LEVEL,0);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,1);
  tquad(60,60,64,64,0,0,1,1);want_level(62,62,1,"max level 1, 4x4");
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAX_LEVEL,1000);glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,2.0f);
  tquad(80,60,112,92,0,0,1,1);want_level(96,76,2,"min LOD 2, 32x32");
  glTexParameterf(GL_TEXTURE_2D,GL_TEXTURE_MIN_LOD,-1000.0f);
 } else skip("base and max level, LOD clamps (OpenGL 1.2)");
 if(has_lodbias){glTexEnvf(GL_TEXTURE_FILTER_CONTROL,GL_TEXTURE_LOD_BIAS,1.0f);tquad(120,60,152,92,0,0,1,1);want_level(136,76,1,"LOD bias 1, 32x32");
  glTexEnvf(GL_TEXTURE_FILTER_CONTROL,GL_TEXTURE_LOD_BIAS,0.0f);}
 else skip("GL_EXT_texture_lod_bias");
 deltex(t);
 /* between two levels: 32 pixels for s 0..1 and t 0..1.414 (one texel step and 1.414 per pixel: LOD 0.5): half red, half green */
 t=miptex(GL_LINEAR_MIPMAP_LINEAR);tquad(8,110,40,142,0,0,1,1.41421356f);want_rgb(24,126,128,128,0,16,"LINEAR_MIPMAP_LINEAR at LOD 0.5"); /* v1.1: within 1/16 of a level (OpenGL lets the scale factor be approximated; the chip: 0.47) */deltex(t);
 /* NEAREST_MIPMAP_LINEAR between levels 1 and 2 at LOD 1.5 */
 t=miptex(GL_NEAREST_MIPMAP_LINEAR);tquad(50,110,66,126,0,0,1,1.41421356f);want_rgb(58,118,0,128,128,16,"NEAREST_MIPMAP_LINEAR at LOD 1.5");deltex(t);
}
static void c_incomplete(void) {
 GLubyte img[16];GLuint t;int i;
 for(i=0;i<4;i++){img[i*4]=255;img[i*4+1]=128;img[i*4+2]=0;img[i*4+3]=255;}
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glColor3ub(0,0,255);
 t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,img);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);tquad(8,8,24,24,0,0,1,1);
 want_rgb(16,16,0,0,255,0,"mipmapped filter, level 1 missing: as if texturing were off");
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);tquad(40,8,56,24,0,0,1,1);want_rgb(48,16,255,128,0,0,"the same texture with GL_LINEAR");
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);glTexImage2D(GL_TEXTURE_2D,1,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,img);
 tquad(72,8,88,24,0,0,1,1);want_rgb(80,16,0,0,255,0,"level 1 of the wrong size: as if texturing were off");
 glTexImage2D(GL_TEXTURE_2D,1,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,img);tquad(104,8,120,24,0,0,1,1);want_rgb(112,16,255,128,0,0,"level 1 made right");
 deltex(t);
 t=newtex();tquad(136,8,152,24,0,0,1,1);want_rgb(144,16,0,0,255,0,"a texture object without an image: as if texturing were off");deltex(t);
}
static void c_genmip(void) {
 GLubyte img[64];GLuint t;int i;
 if(!has_genmip){skip("GL_SGIS_generate_mipmap");return;}
 for(i=0;i<16;i++){int left=(i%4)<2;img[i*4]=(GLubyte)(left?255:0);img[i*4+1]=0;img[i*4+2]=(GLubyte)(left?0:255);img[i*4+3]=255;}
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 t=newtex();glTexParameteri(GL_TEXTURE_2D,GL_GENERATE_MIPMAP,GL_TRUE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST_MIPMAP_NEAREST);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,img);
 tquad(8,8,10,10,0,0,1,1);want_rgb(8,8,255,0,0,0,"generated level 1, left texel");want_rgb(9,8,0,0,255,0,"generated level 1, right texel");
 tquad(20,8,21,9,0,0,1,1);want_rgb(20,8,128,0,128,2,"generated level 2 (the average)");
 for(i=0;i<16;i++){img[i*4]=0;img[i*4+1]=255;img[i*4+2]=0;}
 glTexSubImage2D(GL_TEXTURE_2D,0,0,0,4,4,GL_RGBA,GL_UNSIGNED_BYTE,img);
 tquad(30,8,31,9,0,0,1,1);want_rgb(30,8,0,255,0,2,"level 2 generated again after TexSubImage on level 0");
 deltex(t);
}
static void c_subcopy(void) {
 GLubyte img[8*8*4],g[4*4*4];GLuint t;int i;
 for(i=0;i<64;i++){img[i*4]=255;img[i*4+1]=0;img[i*4+2]=0;img[i*4+3]=255;}for(i=0;i<16;i++){g[i*4]=0;g[i*4+1]=255;g[i*4+2]=0;g[i*4+3]=255;}
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,img);glTexSubImage2D(GL_TEXTURE_2D,0,2,2,4,4,GL_RGBA,GL_UNSIGNED_BYTE,g);
 tquad(8,8,16,16,0,0,1,1);want_rgb(8+1,8+1,255,0,0,0,"TexSubImage: outside");want_rgb(8+2,8+2,0,255,0,0,"TexSubImage: its first texel");
 want_rgb(8+5,8+5,0,255,0,0,"TexSubImage: its last texel");want_rgb(8+6,8+5,255,0,0,0,"TexSubImage: right of it");deltex(t);
 /* the framebuffer: an 8x8 block, left half yellow, right half cyan, and a white 2x2 */
 glDisable(GL_TEXTURE_2D);glColor3ub(255,255,0);rectc(200,200,204,208);glColor3ub(0,255,255);rectc(204,200,208,208);glColor3ub(255,255,255);rectc(220,220,222,222);
 glEnable(GL_TEXTURE_2D);glColor3ub(255,255,255);
 t=newtex();glCopyTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,200,200,8,8,0);tquad(100,100,108,108,0,0,1,1);
 want_rgb(101,103,255,255,0,0,"CopyTexImage2D: left half");want_rgb(106,103,0,255,255,0,"CopyTexImage2D: right half");
 glCopyTexSubImage2D(GL_TEXTURE_2D,0,6,6,220,220,2,2);tquad(120,100,128,108,0,0,1,1);
 want_rgb(126,106,255,255,255,0,"CopyTexSubImage2D: the copied texel");want_rgb(125,106,0,255,255,0,"CopyTexSubImage2D: beside it");
 deltex(t);
}
static void c_objects(void) {
 GLuint t[16];GLubyte c[16][4];GLint wrap=0;int k,n;char m[64];
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);glColor3ub(0,0,255);
 glGenTextures(16,t);
 for(k=0;k<16;k++){c[k][0]=(GLubyte)(k*16);c[k][1]=(GLubyte)(255-k*16);c[k][2]=(GLubyte)((k*37)&255);c[k][3]=255;
  glBindTexture(GL_TEXTURE_2D,t[k]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,k&1?GL_CLAMP:GL_REPEAT);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,c[k]);}
 for(n=0;n<16;n++){k=(n*7)%16;glBindTexture(GL_TEXTURE_2D,t[k]);tquad(8+(n%8)*28,8+(n/8)*28,24+(n%8)*28,24+(n/8)*28,0,0,1,1);}
 for(n=0;n<16;n++){k=(n*7)%16;sprintf(m,"texture object %d (draw %d)",k,n);want_rgb(16+(n%8)*28,16+(n/8)*28,c[k][0],c[k][1],c[k][2],0,m);}
 for(k=0;k<16;k++){glBindTexture(GL_TEXTURE_2D,t[k]);glGetTexParameteriv(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,&wrap);
  if(wrap!=(k&1?GL_CLAMP:GL_REPEAT))bad("texture object %d: its wrap mode %04X not kept",k,(unsigned)wrap);}
 for(k=0;k<16;k+=2)glDeleteTextures(1,&t[k]);
 for(k=0;k<16;k++)if((glIsTexture(t[k])!=0)!=(k&1))bad("glIsTexture(%u) = %d after %s",t[k],(int)glIsTexture(t[k]),k&1?"keeping it":"deleting it");
 glBindTexture(GL_TEXTURE_2D,t[0]);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);tquad(8,80,24,96,0,0,1,1);
 want_rgb(16,88,0,0,255,0,"a deleted name bound again (a new texture without an image)");
 glBindTexture(GL_TEXTURE_2D,t[1]);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,c[9]);tquad(40,80,56,96,0,0,1,1);
 want_rgb(48,88,c[9][0],c[9][1],c[9][2],0,"a texture's image replaced");
 glDeleteTextures(1,&t[0]);for(k=1;k<16;k+=2)glDeleteTextures(1,&t[k]);
}
static void c_multi(void) {
 static const GLubyte a[4]={200,100,50,255},b[8]={128,255,64,255, 255,0,255,255};GLuint ta,tb;GLint units=0;
 if(!pActiveTexture||!pMultiTexCoord2f||max_units<2){skip("GL_ARB_multitexture with two units");return;}
 glGetIntegerv(GL_MAX_TEXTURE_UNITS,&units);if(units<2)bad("GL_MAX_TEXTURE_UNITS %d",(int)units);
 pActiveTexture(GL_TEXTURE0);ta=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,a);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 pActiveTexture(GL_TEXTURE1);tb=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,1,0,GL_RGBA,GL_UNSIGNED_BYTE,b);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
#define MQ(x0,y0,s1) do{glBegin(GL_QUADS);pMultiTexCoord2f(GL_TEXTURE0,0.5f,0.5f);pMultiTexCoord2f(GL_TEXTURE1,s1,0.5f);glVertex2f((float)(x0),(float)(y0));\
  glVertex2f((float)(x0)+16,(float)(y0));glVertex2f((float)(x0)+16,(float)(y0)+16);glVertex2f((float)(x0),(float)(y0)+16);glEnd();}while(0)
 MQ(8,8,0.25f);want_rgb(16,16,(200*128+127)/255,100,(50*64+127)/255,2,"unit 0 REPLACE, unit 1 MODULATE (unit 1's left texel)");
 MQ(40,8,0.75f);want_rgb(48,16,200,0,50,2,"unit 1's coordinates pick its right texel");
 if(has_add){glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_ADD);MQ(72,8,0.25f);want_rgb(80,16,255,255,114,2,"unit 1 ADD");}
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR_MIPMAP_LINEAR);MQ(104,8,0.25f);want_rgb(112,16,200,100,50,1,"unit 1 incomplete: unit 0 alone");
 glDisable(GL_TEXTURE_2D);MQ(136,8,0.25f);want_rgb(144,16,200,100,50,1,"unit 1 disabled");
#undef MQ
 deltex(tb);pActiveTexture(GL_TEXTURE0);deltex(ta);
}
static void c_combine(void) {
 static const GLubyte tx[4]={200,64,128,96};static const GLfloat cc[4]={128/255.f,32/255.f,240/255.f,64/255.f};
 double t[4]={200/255.,64/255.,128/255.,96/255.},p[4]={40/255.,180/255.,90/255.,200/255.},k[4]={128/255.,32/255.,240/255.,64/255.},o[3];
 GLuint tex;int c,x=8;
 if(!has_combine){skip("GL_ARB_texture_env_combine");return;}
 glEnable(GL_TEXTURE_2D);tex=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,tx);
 glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_COMBINE);glTexEnvfv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_COLOR,cc);glColor4ub(40,180,90,200);
 glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_RGB,GL_TEXTURE);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_RGB,GL_PRIMARY_COLOR);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE2_RGB,GL_CONSTANT);
 glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_RGB,GL_SRC_COLOR);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND2_RGB,GL_SRC_ALPHA);
 glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_REPLACE);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE0_ALPHA,GL_TEXTURE);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_ALPHA,GL_SRC_ALPHA);
#define CQ(mode,what,expr) do{glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,mode);tquad(x,8,x+16,24,0,0,1,1);for(c=0;c<3;c++){double v=(expr);o[c]=v<0?0:v>1?1:v;}\
  want_rgb(x+8,16,f2b(o[0]),f2b(o[1]),f2b(o[2]),4,what);x+=24;}while(0)
 CQ(GL_REPLACE,"combine REPLACE texture",t[c]);
 CQ(GL_MODULATE,"combine MODULATE texture, primary",t[c]*p[c]);
 CQ(GL_ADD,"combine ADD",t[c]+p[c]);
 CQ(GL_ADD_SIGNED,"combine ADD_SIGNED",t[c]+p[c]-0.5);
 CQ(GL_INTERPOLATE,"combine INTERPOLATE by the constant's alpha",t[c]*k[3]+p[c]*(1-k[3]));
 CQ(GL_SUBTRACT,"combine SUBTRACT",t[c]-p[c]);
 glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_ONE_MINUS_SRC_COLOR);CQ(GL_REPLACE,"combine REPLACE, operand ONE_MINUS_SRC_COLOR",1-t[c]);
 glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND0_RGB,GL_SRC_COLOR);glTexEnvf(GL_TEXTURE_ENV,GL_RGB_SCALE,2.0f);CQ(GL_MODULATE,"combine MODULATE, RGB_SCALE 2",2*t[c]*p[c]);
 glTexEnvf(GL_TEXTURE_ENV,GL_RGB_SCALE,1.0f);
 if(has_dot3){double d=4*((t[0]-.5)*(p[0]-.5)+(t[1]-.5)*(p[1]-.5)+(t[2]-.5)*(p[2]-.5));CQ(GL_DOT3_RGB,"combine DOT3_RGB",d);}else skip("GL_ARB_texture_env_dot3");
 /* alpha: MODULATE texture alpha by the primary's, seen as colour x alpha */
 glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_ALPHA,GL_MODULATE);glTexEnvi(GL_TEXTURE_ENV,GL_SOURCE1_ALPHA,GL_PRIMARY_COLOR);glTexEnvi(GL_TEXTURE_ENV,GL_OPERAND1_ALPHA,GL_SRC_ALPHA);
 glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ZERO);x=8;
 {double a=t[3]*p[3];glTexEnvi(GL_TEXTURE_ENV,GL_COMBINE_RGB,GL_REPLACE);tquad(x,40,x+16,56,0,0,1,1);want_rgb(x+8,48,f2b(t[0]*a),f2b(t[1]*a),f2b(t[2]*a),5,"combine alpha MODULATE (colour x alpha)");}
#undef CQ
 deltex(tex);
}
static void c_palette(void) {
 GLubyte pal[256*4],pal2[256*4],idx[16];GLuint t,t2;int i;char m[64];
 if(!has_pal||!pColorTable){skip("GL_EXT_paletted_texture");return;}
 for(i=0;i<256;i++){pal[i*4]=(GLubyte)i;pal[i*4+1]=(GLubyte)(255-i);pal[i*4+2]=(GLubyte)((i*5)&255);pal[i*4+3]=255;
  pal2[i*4]=(GLubyte)((i*3)&255);pal2[i*4+1]=(GLubyte)((i*7)&255);pal2[i*4+2]=(GLubyte)(255-i);pal2[i*4+3]=255;}
 for(i=0;i<16;i++)idx[i]=(GLubyte)(i*13+7);
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 t=newtex();pColorTable(GL_TEXTURE_2D,GL_RGBA,256,GL_RGBA,GL_UNSIGNED_BYTE,pal);glTexImage2D(GL_TEXTURE_2D,0,GL_COLOR_INDEX8_EXT,4,4,0,GL_COLOR_INDEX,GL_UNSIGNED_BYTE,idx);
 tquad(8,8,40,40,0,0,1,1);
 for(i=0;i<16;i++){int j=idx[i];sprintf(m,"the texture's palette, index %d",j);want_rgb(8+(i%4)*8+4,8+(i/4)*8+4,pal[j*4],pal[j*4+1],pal[j*4+2],0,m);}
 if(has_shpal) {
  pColorTable(GL_SHARED_TEXTURE_PALETTE_EXT,GL_RGBA,256,GL_RGBA,GL_UNSIGNED_BYTE,pal2);glEnable(GL_SHARED_TEXTURE_PALETTE_EXT);
  t2=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_COLOR_INDEX8_EXT,4,4,0,GL_COLOR_INDEX,GL_UNSIGNED_BYTE,idx);tquad(56,8,88,40,0,0,1,1);
  glBindTexture(GL_TEXTURE_2D,t);tquad(104,8,136,40,0,0,1,1);
  for(i=0;i<16;i++){int j=idx[i];sprintf(m,"the shared palette, index %d",j);want_rgb(56+(i%4)*8+4,8+(i/4)*8+4,pal2[j*4],pal2[j*4+1],pal2[j*4+2],0,m);
   sprintf(m,"the shared palette over the texture's own, index %d",j);want_rgb(104+(i%4)*8+4,8+(i/4)*8+4,pal2[j*4],pal2[j*4+1],pal2[j*4+2],0,m);}
  pColorTable(GL_SHARED_TEXTURE_PALETTE_EXT,GL_RGBA,256,GL_RGBA,GL_UNSIGNED_BYTE,pal);glBindTexture(GL_TEXTURE_2D,t2);tquad(152,8,184,40,0,0,1,1);
  for(i=0;i<16;i++){int j=idx[i];sprintf(m,"the shared palette changed, index %d",j);want_rgb(152+(i%4)*8+4,8+(i/4)*8+4,pal[j*4],pal[j*4+1],pal[j*4+2],0,m);}
  glDisable(GL_SHARED_TEXTURE_PALETTE_EXT);deltex(t2);
 } else skip("GL_EXT_shared_texture_palette");
 deltex(t);
}
static void c_texgen(void) {
 GLubyte img[64];GLuint t;int bx,by;char m[96];GLfloat sp[4],tp[4];
 code4(img);glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,img);
 /* OBJECT_LINEAR: s = (x - 8) / 64, t = (y - 8) / 64 */
 sp[0]=1/64.f;sp[1]=0;sp[2]=0;sp[3]=-8/64.f;tp[0]=0;tp[1]=1/64.f;tp[2]=0;tp[3]=-8/64.f;
 glTexGeni(GL_S,GL_TEXTURE_GEN_MODE,GL_OBJECT_LINEAR);glTexGeni(GL_T,GL_TEXTURE_GEN_MODE,GL_OBJECT_LINEAR);glTexGenfv(GL_S,GL_OBJECT_PLANE,sp);glTexGenfv(GL_T,GL_OBJECT_PLANE,tp);
 glEnable(GL_TEXTURE_GEN_S);glEnable(GL_TEXTURE_GEN_T);rectc(8,8,72,72);
 for(by=0;by<4;by++)for(bx=0;bx<4;bx++){sprintf(m,"OBJECT_LINEAR texgen, texel %d,%d",bx,by);want_rgb(8+bx*16+8,8+by*16+8,40+60*bx,40+60*by,128,0,m);}
 /* EYE_LINEAR with the identity modelview: the same planes; at x 136 */
 sp[3]=-136/64.f;glTexGeni(GL_S,GL_TEXTURE_GEN_MODE,GL_EYE_LINEAR);glTexGeni(GL_T,GL_TEXTURE_GEN_MODE,GL_EYE_LINEAR);glTexGenfv(GL_S,GL_EYE_PLANE,sp);glTexGenfv(GL_T,GL_EYE_PLANE,tp);
 rectc(136,8,200,72);
 for(by=0;by<4;by++)for(bx=0;bx<4;bx++){sprintf(m,"EYE_LINEAR texgen, texel %d,%d",bx,by);want_rgb(136+bx*16+8,8+by*16+8,40+60*bx,40+60*by,128,0,m);}
 glDisable(GL_TEXTURE_GEN_S);glDisable(GL_TEXTURE_GEN_T);
 /* the texture matrix: s + 0.25 (one texel on), then s and t times 2 */
 glMatrixMode(GL_TEXTURE);glLoadIdentity();glTranslatef(0.25f,0,0);glMatrixMode(GL_MODELVIEW);tquad(8,100,72,164,0,0,1,1);
 for(by=0;by<4;by++)for(bx=0;bx<4;bx++){sprintf(m,"texture matrix s + 0.25, texel %d,%d",(bx+1)%4,by);want_rgb(8+bx*16+8,100+by*16+8,40+60*((bx+1)%4),40+60*by,128,0,m);}
 glMatrixMode(GL_TEXTURE);glLoadIdentity();glScalef(2,2,1);glMatrixMode(GL_MODELVIEW);tquad(136,100,200,164,0,0,1,1);
 for(by=0;by<8;by++)for(bx=0;bx<8;bx++){sprintf(m,"texture matrix x2, texel %d,%d",bx%4,by%4);want_rgb(136+bx*8+4,100+by*8+4,40+60*(bx%4),40+60*(by%4),128,0,m);}
 glMatrixMode(GL_TEXTURE);glLoadIdentity();glMatrixMode(GL_MODELVIEW);
 deltex(t);
}
static void c_cube3d(void) {
 static const GLubyte fc[6][4]={{255,0,0,255},{0,255,0,255},{0,0,255,255},{255,255,0,255},{255,0,255,255},{0,255,255,255}};
 static const GLfloat dir[7][3]={{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1},{0.9f,0.3f,-0.2f}};
 static const char *fname[6]={"+X","-X","+Y","-Y","+Z","-Z"};
 GLubyte face[2*2*4],vol[2*2*4*4];GLuint t;int f,i,k;char m[64];
 glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 if(has_cube) {
  glGenTextures(1,&t);glBindTexture(GL_TEXTURE_CUBE_MAP,t);
  glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_CUBE_MAP,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  for(f=0;f<6;f++){for(i=0;i<4;i++)memcpy(face+i*4,fc[f],4);glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X+f,0,GL_RGBA8,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,face);}
  glEnable(GL_TEXTURE_CUBE_MAP);
  for(i=0;i<7;i++){int x0=8+i*24;glBegin(GL_QUADS);glTexCoord3fv(dir[i]);glVertex2f((float)x0,8);glVertex2f((float)x0+16,8);glVertex2f((float)x0+16,24);glVertex2f((float)x0,24);glEnd();
   f=i<6?i:0;sprintf(m,"cube map: direction %s%s",i<6?fname[f]:"0.9,0.3,-0.2 (",i<6?" face":"+X face)");want_rgb(x0+8,16,fc[f][0],fc[f][1],fc[f][2],0,m);}
  glDisable(GL_TEXTURE_CUBE_MAP);glBindTexture(GL_TEXTURE_CUBE_MAP,0);glDeleteTextures(1,&t);
 } else skip("GL_ARB_texture_cube_map");
 if(gl12&&pTexImage3D) {
  for(k=0;k<4;k++)for(i=0;i<4;i++)memcpy(vol+(k*4+i)*4,fc[k],4);
  glGenTextures(1,&t);glBindTexture(GL_TEXTURE_3D,t);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_3D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
  pTexImage3D(GL_TEXTURE_3D,0,GL_RGBA8,2,2,4,0,GL_RGBA,GL_UNSIGNED_BYTE,vol);glEnable(GL_TEXTURE_3D);
  for(k=0;k<4;k++){int x0=8+k*24;glBegin(GL_QUADS);glTexCoord3f(0.5f,0.5f,(k+0.5f)/4);glVertex2f((float)x0,40);glVertex2f((float)x0+16,40);glVertex2f((float)x0+16,56);glVertex2f((float)x0,56);glEnd();
   sprintf(m,"3D texture: slice %d",k);want_rgb(x0+8,48,fc[k][0],fc[k][1],fc[k][2],0,m);}
  glDisable(GL_TEXTURE_3D);glBindTexture(GL_TEXTURE_3D,0);glDeleteTextures(1,&t);
 } else skip("3D textures (OpenGL 1.2)");
}
static void c_alpha(void) {
 GLubyte img[64],half[4]={255,255,255,128};GLuint t;int u,v,pass;char m[80];
 for(v=0;v<4;v++)for(u=0;u<4;u++){GLubyte *q=img+(v*4+u)*4;q[0]=q[1]=q[2]=255;q[3]=(GLubyte)(((u+v)&1)?255:0);}
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.5f);
 for(pass=0;pass<2;pass++){int x0=8+pass*48;
  t=newtex();if(pass){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);}
  glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,4,4,0,GL_RGBA,GL_UNSIGNED_BYTE,img);tquad(x0,8,x0+32,40,0,0,1,1);
  for(v=0;v<4;v++)for(u=0;u<4;u++){int on=(u+v)&1;sprintf(m,"alpha test GREATER 0.5, %s, texel %d,%d %s",pass?"linear":"nearest",u,v,on?"opaque":"clear");
   want_rgb(x0+u*8+4,8+v*8+4,on?255:0,on?255:0,on?255:0,0,m);}
  deltex(t);}
 glDisable(GL_ALPHA_TEST);
 /* blending over red: alpha 128 */
 glDisable(GL_TEXTURE_2D);glColor3ub(255,0,0);rectc(120,8,152,40);glEnable(GL_TEXTURE_2D);glColor3ub(255,255,255);
 t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,half);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 tquad(120,8,152,40,0,0,1,1);want_rgb(136,24,255,128,128,2,"texture alpha 128 blended over red");glDisable(GL_BLEND);deltex(t);
}
static void c_perspective(void) {
 static const GLubyte tex[8]={255,0,0,255, 0,255,0,255};GLuint t;
 /* the right edge 3 times as far (w 3): s at a fraction f across the screen is (f/3) / ((1-f) + f/3); s = 0.5 at f = 0.75 */
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);t=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,2,1,0,GL_RGBA,GL_UNSIGNED_BYTE,tex);
 glBegin(GL_QUADS);
 glTexCoord2f(0,0);glVertex4f(28,8,0,1);glTexCoord2f(1,0);glVertex4f(228*3,8*3,0,3);glTexCoord2f(1,1);glVertex4f(228*3,40*3,0,3);glTexCoord2f(0,1);glVertex4f(28,40,0,1);
 glEnd();
 want_rgb(28+120,24,255,0,0,0,"perspective: 60% across (affine would be the second texel)");
 want_rgb(28+140,24,255,0,0,0,"perspective: 70% across");
 want_rgb(28+160,24,0,255,0,0,"perspective: 80% across");
 want_rgb(28+60,24,255,0,0,0,"perspective: 30% across");
 deltex(t);
}
/* timings: they are reported, not judged */
static void c_speed_bind(void) {
 GLubyte a[64*4],b[64*4];GLuint t[2];int i,n=2000*scale;double t0,t1;
 for(i=0;i<64;i++){a[i*4]=200;a[i*4+1]=100;a[i*4+2]=50;a[i*4+3]=255;b[i*4]=20;b[i*4+1]=200;b[i*4+2]=150;b[i*4+3]=255;}
 glEnable(GL_TEXTURE_2D);t[0]=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,a);t[1]=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,8,8,0,GL_RGBA,GL_UNSIGNED_BYTE,b);
 glFinish();t0=now_ms();
 for(i=0;i<n;i++){float x=(float)(8+(i*37)%200),y=(float)(8+(i*91)%200);glBindTexture(GL_TEXTURE_2D,t[i&1]);
  glBegin(GL_TRIANGLES);glTexCoord2f(0,0);glVertex2f(x,y);glTexCoord2f(1,0);glVertex2f(x+3,y);glTexCoord2f(0,1);glVertex2f(x,y+3);glEnd();}
 glFinish();t1=now_ms();tms[which][0]=(t1-t0)*1000.0/n;
 fprintf(logf,"     a bind and a tiny triangle: %.2f microseconds (%d)\n",tms[which][0],n);
 glDeleteTextures(2,t);
}
static void c_speed_fill(void) {
 GLubyte img[64*64*4];GLuint t;int i,n=20*scale;double t0,t1;
 for(i=0;i<64*64;i++){img[i*4]=(GLubyte)(i*7);img[i*4+1]=(GLubyte)(i>>4);img[i*4+2]=(GLubyte)(i*3);img[i*4+3]=255;}
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);t=newtex();
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,img);
 glFinish();t0=now_ms();
 for(i=0;i<n;i++)tquad(0,0,W,H,0,0,4,4);
 glFinish();t1=now_ms();tms[which][1]=t1>t0?(double)W*H*n/((t1-t0)*1000.0):0;
 fprintf(logf,"     textured fill (bilinear, modulate): %.2f million pixels a second (%d full-window quads)\n",tms[which][1],n);
 deltex(t);
}
static void c_speed_quads(void) {
 GLubyte img[16*16*4];GLuint t[8];int i,k,f,n=10*scale,q=0;double t0,t1;
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
 for(k=0;k<8;k++){for(i=0;i<256;i++){img[i*4]=(GLubyte)(k*30+i);img[i*4+1]=(GLubyte)(i);img[i*4+2]=(GLubyte)(255-k*20);img[i*4+3]=255;}
  t[k]=newtex();glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,16,16,0,GL_RGBA,GL_UNSIGNED_BYTE,img);}
 glFinish();t0=now_ms();
 for(f=0;f<n;f++)for(i=0;i<800;i++){int x=(i%32)*8,y=(i/32)*8;if(!(i&7))glBindTexture(GL_TEXTURE_2D,t[(i>>3)&7]);tquad(x,y,x+8,y+8,0,0,1,1);q++;}
 glFinish();t1=now_ms();tms[which][2]=(t1-t0)*1000.0/q;
 fprintf(logf,"     small textured quads, each its own glBegin/glEnd, a bind every 8: %.2f microseconds a quad (%d)\n",tms[which][2],q);
 glDeleteTextures(8,t);
}


/* v1.1 probes (reported, not judged): the board's v10.268 engine read every bilinear weight 1/64 of a texel high at the case's sample
 * points (all on the 1/64 grid) and its mip level about 0.03 low. The probes measure both off that grid: a black-white texel pair,
 * filtered linearly, at several scales and texture sizes and along each axis (the weight seen against the exact one), and the level
 * a LINEAR_MIPMAP_LINEAR quad samples at LODs from 0.25 to 1.75. */
static void probe_bilinear(int n,int q,int vert) {
 GLubyte tex[16*3],row[256*4];GLuint t;int i,b=n/2-1,cnt=0,shown=0;double sum=0,mn=9,mx=-9;
 for(i=0;i<n;i++){GLubyte v=(GLubyte)(i<n/2?0:255);tex[i*3]=tex[i*3+1]=tex[i*3+2]=v;}
 reset();glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);t=newtex();
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 if(gl12){glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
 if(!vert){glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,n,1,0,GL_RGB,GL_UNSIGNED_BYTE,tex);tquad(0,100,q,104,0,0,1,1);glReadPixels(0,101,q,1,GL_RGBA,GL_UNSIGNED_BYTE,row);}
 else {glTexImage2D(GL_TEXTURE_2D,0,GL_RGB8,1,n,0,GL_RGB,GL_UNSIGNED_BYTE,tex);tquad(100,0,104,q,0,0,1,1);for(i=0;i<q;i++)glReadPixels(101,i,1,1,GL_RGBA,GL_UNSIGNED_BYTE,row+i*4);}
 fprintf(logf,"     %s, %d texels over %d pixels (%.4f texel a pixel): ",vert?"t":"s",n,q,(double)n/q);
 for(i=0;i<q;i++){double u=(i+0.5)*n/q-0.5,f=u-b,m=row[i*4]/255.0;if(f<0||f>=1)continue;cnt++;sum+=m-f;if(m-f<mn)mn=m-f;if(m-f>mx)mx=m-f;
  if(shown<6&&(cnt%3==1)){fprintf(logf,"%.4f>%.4f ",f,m);shown++;}}
 fprintf(logf,"| %d samples, read minus exact: mean %+.4f, min %+.4f, max %+.4f (1/64 = 0.0156)\n",cnt,cnt?sum/cnt:0,mn,mx);
 deltex(t);
}
static void probe_lod(void) {
 GLuint t;int k;GLubyte p[4];double sc;
 reset();glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);t=miptex(GL_LINEAR_MIPMAP_LINEAR);
 fprintf(logf,"     LINEAR_MIPMAP_LINEAR, LOD exact > read:");
 sc=1.0;
 for(k=1;k<8;k++){double lam=k*0.25,r;int x=8+(k-1)*24;sc*=1.18920711500272; /* 2 to the 1/4 */
  tquad(x,140,x+16,156,0,0,0.5f,(float)(0.5*sc));glReadPixels(x+8,148,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);
  r=lam<1?p[1]/255.0:1+p[2]/255.0;fprintf(logf," %.2f>%.3f",lam,r);}
 fprintf(logf,"\n");
 deltex(t);
}
static void probes(void) {
 fprintf(logf,"   probes (reported, not judged)\n");
 probe_bilinear(2,64,0);probe_bilinear(2,48,0);probe_bilinear(2,40,0);probe_bilinear(2,200,0);probe_bilinear(16,256,0);probe_bilinear(16,96,0);
 probe_bilinear(2,64,1);probe_bilinear(2,40,1);
 probe_lod();
}

#ifndef VXTEX_HOST
static HANDLE vxd=INVALID_HANDLE_VALUE;
/* v1.6 (v10.295): the VxD's Coarse Z (on by default, XP's clear) against the same draws with it off (TEXDBG D4/D5; reported, not judged):
 * 20 full-window textured quads in front-to-back order (each hidden by the first: what Coarse Z can reject) and back to front (each in
 * front of the last: its records are updated), timed and read back, the two images compared pixel for pixel */
static int vxd_texdbg(DWORD code,DWORD *o) {DWORD got=0;return vxd!=INVALID_HANDLE_VALUE&&DeviceIoControl(vxd,0x85500022UL,&code,4,o,32,&got,0)&&got==32;}
static void probe_coarse_z(void) {
 static GLubyte img[2][2][W*H*4],tex[64*64*4];DWORD o[8],sv[8];double t[2][2];int pass,ord,i,n=20,diff[2]={0,0};GLuint tx;
 if(!vxd_texdbg(0xCE,sv)||sv[0]!=0xCE){fprintf(logf,"     (the VxD has no TEXDBG CE: no Coarse Z probe)\n");return;}
 for(i=0;i<64*64;i++){tex[i*4]=(GLubyte)(i*5);tex[i*4+1]=(GLubyte)(i>>4);tex[i*4+2]=(GLubyte)(i*3);tex[i*4+3]=255;}
 reset();glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);tx=newtex();
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,tex);
 glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);glDepthMask(GL_TRUE);
 for(pass=0;pass<2;pass++){vxd_texdbg(pass?0xD4:0xD5,o);
  for(ord=0;ord<2;ord++){double t0;glClearDepth(1.0);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glFinish();t0=now_ms();
   for(i=0;i<n;i++){int k=ord?n-1-i:i;float z=0.9f-k*0.08f; /* depth (1 - z) / 2: quad 0 is the nearest */
    glColor3ub((GLubyte)(255-k*10),(GLubyte)(k*12),(GLubyte)(128+k*5));
    glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex3f(0,0,z);glTexCoord2f(4,0);glVertex3f((float)W,0,z);glTexCoord2f(4,4);glVertex3f((float)W,(float)H,z);glTexCoord2f(0,4);glVertex3f(0,(float)H,z);glEnd();}
   glFinish();t[pass][ord]=now_ms()-t0;glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,img[pass][ord]);}}
 vxd_texdbg(0xD4+(sv[4]>2?1:sv[4]),o);
 glDisable(GL_DEPTH_TEST);glColor3ub(255,255,255);deltex(tx);
 for(ord=0;ord<2;ord++)for(i=0;i<W*H;i++)if(memcmp(img[0][ord]+i*4,img[1][ord]+i*4,3))diff[ord]++;
 fprintf(logf,"     Coarse Z, 20 full-window textured quads: front to back %.2f ms on, %.2f off; back to front %.2f on, %.2f off; pixels differing on/off %d and %d%s\n",
  t[0][0],t[1][0],t[0][1],t[1][1],diff[0],diff[1],sv[4]==0?" (ACCEL_3D_COARSEZ 0: both off)":"");
 if(vxd_texdbg(0xCE,o)&&o[0]==0xCE)fprintf(logf,"     Coarse Z storage fills: 2D engine %lu, CPU %lu; records dropped by a depth clear %lu\n",(unsigned long)o[5],(unsigned long)o[6],(unsigned long)o[7]);
}
#endif

static const struct {const char *name;void (*f)(void);int timed;} cases[]={
 {"texel upload, nearest, 1:1 and magnified",c_upload,0},{"internal formats",c_formats,0},{"source formats and unpack state",c_sources,0},
 {"texture environments",c_env,0},{"wrap modes",c_wrap,0},{"bilinear filtering",c_bilinear,0},{"mipmap selection",c_mipmap,0},
 {"incomplete textures",c_incomplete,0},{"generated mipmaps",c_genmip,0},{"TexSubImage and CopyTexImage",c_subcopy,0},{"texture objects",c_objects,0},
 {"multitexture",c_multi,0},{"env combine and dot3",c_combine,0},{"paletted textures",c_palette,0},{"texgen and the texture matrix",c_texgen,0},
 {"cube maps and 3D textures",c_cube3d,0},{"alpha-tested cut-outs and blending",c_alpha,0},{"perspective correction",c_perspective,0},
 {"speed: a bind and a tiny triangle",c_speed_bind,1},{"speed: textured fill",c_speed_fill,1},{"speed: small textured quads",c_speed_quads,1}};
#define NCASES ((int)(sizeof(cases)/sizeof(cases[0])))

/* what the context offers */
static int has_ext(const char *ext,const char *n) {
 const char *p=ext;size_t l=strlen(n);
 if(!ext)return 0;
 while((p=strstr(p,n))!=0){if((p==ext||p[-1]==' ')&&(p[l]==' '||p[l]==0))return 1;p+=l;}
 return 0;
}
static void features(void) {
 const char *ver=(const char*)glGetString(GL_VERSION),*ext=(const char*)glGetString(GL_EXTENSIONS);int mj=ver?ver[0]-'0':1,mn=ver&&ver[1]=='.'?ver[2]-'0':0;GLint u=1;
 gl12=mj>1||mn>=2;gl13=mj>1||mn>=3;gl14=mj>1||mn>=4;
 has_genmip=gl14||has_ext(ext,"GL_SGIS_generate_mipmap");has_lodbias=gl14||has_ext(ext,"GL_EXT_texture_lod_bias");
 has_add=gl13||has_ext(ext,"GL_EXT_texture_env_add")||has_ext(ext,"GL_ARB_texture_env_add");
 has_combine=gl13||has_ext(ext,"GL_ARB_texture_env_combine")||has_ext(ext,"GL_EXT_texture_env_combine");
 has_dot3=gl13||has_ext(ext,"GL_ARB_texture_env_dot3");has_cube=gl13||has_ext(ext,"GL_ARB_texture_cube_map");
 has_mirror=gl14||has_ext(ext,"GL_ARB_texture_mirrored_repeat")||has_ext(ext,"GL_IBM_texture_mirrored_repeat");
 has_pal=has_ext(ext,"GL_EXT_paletted_texture");has_shpal=has_ext(ext,"GL_EXT_shared_texture_palette");has_bgra=gl12||has_ext(ext,"GL_EXT_bgra");
 pActiveTexture=(at_fn)getproc(gl13?"glActiveTexture":"glActiveTextureARB");if(!pActiveTexture)pActiveTexture=(at_fn)getproc("glActiveTextureARB");
 pMultiTexCoord2f=(mtc_fn)getproc(gl13?"glMultiTexCoord2f":"glMultiTexCoord2fARB");if(!pMultiTexCoord2f)pMultiTexCoord2f=(mtc_fn)getproc("glMultiTexCoord2fARB");
 if(!gl13&&!has_ext(ext,"GL_ARB_multitexture"))pActiveTexture=0,pMultiTexCoord2f=0;
 pTexImage3D=gl12?(ti3_fn)getproc("glTexImage3D"):0;if(gl12&&!pTexImage3D)pTexImage3D=(ti3_fn)getproc("glTexImage3DEXT");
 pColorTable=has_pal?(ct_fn)getproc("glColorTableEXT"):0;
 max_units=1;if(pActiveTexture){glGetIntegerv(GL_MAX_TEXTURE_UNITS,&u);max_units=u;}
 {GLint rb=8;glGetIntegerv(GL_RED_BITS,&rb);fbtol=rb>=8?1:9;if(which==0)cmp_tol=rb>=8?16:17;}
 while(glGetError()!=GL_NO_ERROR);
 fprintf(logf,"   GL_RENDERER %s, GL_VERSION %s; units %d; generate mipmap %s, LOD bias %s, env add %s, combine %s, dot3 %s, cube %s, mirror %s, palette %s, shared palette %s\n",
  (const char*)glGetString(GL_RENDERER),ver?ver:"?",max_units,has_genmip?"yes":"no",has_lodbias?"yes":"no",has_add?"yes":"no",has_combine?"yes":"no",has_dot3?"yes":"no",
  has_cube?"yes":"no",has_mirror?"yes":"no",has_pal?"yes":"no",has_shpal?"yes":"no");
}
static void run_cases(void) {
 int k;GLenum e;
 for(k=0;k<NCASES;k++) {
#ifndef VXTEX_HOST
  {MSG m;while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}}
#endif
  case_fail=0;case_skip=0;reset();
  if(!cases[k].timed)fprintf(logf,"%2d %s\n",k,cases[k].name);else fprintf(logf,"%2d %s\n",k,cases[k].name);
  cases[k].f();glFinish();
  while((e=glGetError())!=GL_NO_ERROR){case_fail++;fprintf(logf,"     FAIL GL error %04X\n",(unsigned)e);}
  if(!cases[k].timed){glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,cimg[which][k]);shave[which][k]=1;}
  fprintf(logf,"   %s\n",case_fail?"FAIL":case_skip?"pass (parts skipped)":"pass");
  if(case_fail&&which!=1)failures+=case_fail;
  SwapBuffers(gdc);
 }
}
static int compare(int k,int *at) {
 int i,c,d=0;*at=-1;
 /* v1.5 (v10.291): at 16 bpp two 5-bit steps (16 or 17 read back) are allowed. The board's probe (v1.4) showed the engine truncating to 565
  * with dither off, in every mode of 01/53, where swrast rounds to nearest: with a filtered value a little high (the engine's 1/32
  * weights) the two land up to two steps apart (VXTEX case 5: FFDADE against FFD2CD) */
 for(i=0;i<W*H;i++){int o=0;for(c=0;c<3;c++){int p=cimg[0][k][i*4+c],q=cimg[2][k][i*4+c];if(p-q>cmp_tol||q-p>cmp_tol)o=1;}if(o){if(*at<0)*at=i;d++;}}
 return d;
}

#ifdef VXTEX_HOST
int vxtex_host(FILE *log,int slot,void *(*gp)(const char*)) {
 logf=log;which=slot;failures=0;scale=1;getproc=gp;
 features();run_cases();if(slot==0)probes();
 return failures;
}
int vxtex_host_compare(FILE *log) {
 int k,f=0,at;
 for(k=0;k<NCASES;k++){int d;if(cases[k].timed)continue;d=compare(k,&at);
  if(d){fprintf(log,"VXTEX %d %s: engine against software: %d pixels differ, first at %d,%d\n",k,cases[k].name,d,at%W,at/W);f++;}}
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
  if(p.cDepthBits<16)continue;
  s=(p.cColorBits==GetDeviceCaps(dc,BITSPIXEL)?1000:0)+(p.cDepthBits>=24?10:0);
  if(s>score){score=s;best=i;}
 }
 if(best){DescribePixelFormat(dc,best,sizeof(p),&p);fprintf(logf,"   pixel format %d: colour %d, depth %d\n",best,p.cColorBits,p.cDepthBits);}
 return best;
}
/* which: 0 the ICD (engine), 1 Microsoft's generic OpenGL, 2 the ICD with ACCEL_OPENGL_HW = 0 */
static void run(int w_) {
 WNDCLASSA wc;HWND w;HDC dc;HGLRC rc;int pf;RECT r;
 const char *name=w_==1?"Microsoft software OpenGL (reference, failures not counted)":w_==2?"VX855 ICD, engine off (ACCEL_OPENGL_HW = 0)":"VX855 ICD with the engine";
 which=w_;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="VXTEX";wc.style=CS_OWNDC;RegisterClassA(&wc);
 r.left=0;r.top=0;r.right=W;r.bottom=H;AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
 w=CreateWindowA("VXTEX",name,WS_OVERLAPPEDWINDOW|WS_VISIBLE,60,60,r.right-r.left,r.bottom-r.top,0,0,wc.hInstance,0);
 fprintf(logf,"== %s\n",name);
 if(!w){fprintf(logf,"   no window\n");if(which!=1)failures++;return;}
 dc=GetDC(w);gdc=dc;
 pf=choose_format(dc,which==1);
 if(!pf){fprintf(logf,"   no double-buffered pixel format with depth\n");if(which!=1)failures++;DestroyWindow(w);return;}
 {PIXELFORMATDESCRIPTOR p;DescribePixelFormat(dc,pf,sizeof(p),&p);if(!SetPixelFormat(dc,pf,&p)){fprintf(logf,"   SetPixelFormat failed\n");if(which!=1)failures++;DestroyWindow(w);return;}}
 rc=wglCreateContext(dc);if(!rc||!wglMakeCurrent(dc,rc)){fprintf(logf,"   no context\n");if(which!=1)failures++;DestroyWindow(w);return;}
 getproc=wgl_proc;features();
 if(which==0){const char *rn=(const char*)glGetString(GL_RENDERER);if(!rn||strstr(rn,"(software")){fprintf(logf,"   the engine is not drawing: FAIL\n");failures++;}}
 if(which==2){const char *rn=(const char*)glGetString(GL_RENDERER);if(!rn||!strstr(rn,"(software")){fprintf(logf,"   the engine-off run is not in software (VXGL_HW = 0 not taken): FAIL\n");failures++;}} /* v10.319 (audit T2): the reference is checked */
 run_cases();if(which==0){probes();probe_coarse_z();} /* v1.1; v1.6: Coarse Z */
 wglMakeCurrent(0,0);wglDeleteContext(rc);ReleaseDC(w,dc);DestroyWindow(w);
}
static void bmp(const char *name,const unsigned char *s) {
 FILE *f=fopen(name,"wb");int y,x;unsigned char h[54];
 if(!f)return;memset(h,0,54);h[0]='B';h[1]='M';*(DWORD*)(h+2)=54+W*H*3;*(DWORD*)(h+10)=54;*(DWORD*)(h+14)=40;*(LONG*)(h+18)=W;*(LONG*)(h+22)=H;*(WORD*)(h+26)=1;*(WORD*)(h+28)=24;
 fwrite(h,1,54,f);for(y=0;y<H;y++)for(x=0;x<W;x++){fputc(s[(y*W+x)*4+2],f);fputc(s[(y*W+x)*4+1],f);fputc(s[(y*W+x)*4],f);}
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
/* v1.3: the desktop's 2D drawing before and after each run. A popup window, 32 screen-to-screen copies of 256x256 and 32 fills of
 * 512x256 through GDI, timed; with the VxD's 2D counters around them (VX_IOCTL_2D_STATS 0x85500016), the display driver's binding
 * (VX_IOCTL_2D_CTRL 0x85500012, query FFFFFE10: bound, enabled word, BUSY flag) and the native 3D fault (TEXDBG A5). An operation the
 * engine did counts in "ops"; one the VxD declined in "fallbacks"; one the display driver kept for itself in neither. */
static void check_2d(const char *when) {
 WNDCLASSA wc;HWND w;HDC dc;DWORD s0[20],s1[20],b[4],l[8],m[4],got,in;LARGE_INTEGER f,t0,t1,t2;int k,ok;HBRUSH br,hb;RECT rr;double cu,fu;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="VXTEX2D";RegisterClassA(&wc);
 w=CreateWindowExA(WS_EX_TOPMOST,"VXTEX2D","VXTEX 2D",WS_POPUP|WS_VISIBLE,60,60,512,256,0,0,wc.hInstance,0);
 if(!w){fprintf(logf,"2D %s: no window\n",when);return;}
 UpdateWindow(w);dc=GetDC(w);
 hb=CreateHatchBrush(HS_DIAGCROSS,RGB(255,128,0));br=CreateSolidBrush(RGB(0,64,128));
 SetRect(&rr,0,0,512,256);FillRect(dc,&rr,hb);GdiFlush();
 memset(s0,0,sizeof(s0));memset(s1,0,sizeof(s1));memset(b,0,sizeof(b));memset(l,0,sizeof(l));memset(m,0,sizeof(m));
 ok=vxd!=INVALID_HANDLE_VALUE&&DeviceIoControl(vxd,0x85500016UL,0,0,s0,sizeof(s0),&got,0);
 QueryPerformanceFrequency(&f);QueryPerformanceCounter(&t0);
 for(k=0;k<32;k++){if(k&1)BitBlt(dc,0,0,256,256,dc,256,0,SRCCOPY);else BitBlt(dc,256,0,256,256,dc,0,0,SRCCOPY);}
 GdiFlush();QueryPerformanceCounter(&t1);
 {HGDIOBJ o=SelectObject(dc,br);for(k=0;k<32;k++)PatBlt(dc,0,0,512,256,(k&1)?PATCOPY:PATINVERT);SelectObject(dc,o);}
 GdiFlush();QueryPerformanceCounter(&t2);
 ok=ok&&DeviceIoControl(vxd,0x85500016UL,0,0,s1,sizeof(s1),&got,0);
 cu=(double)(t1.QuadPart-t0.QuadPart)*1e6/(double)f.QuadPart/32;fu=(double)(t2.QuadPart-t1.QuadPart)*1e6/(double)f.QuadPart/32;
 fprintf(logf,"2D %s: a 256x256 copy %.0f microseconds, a 512x256 fill %.0f",when,cu,fu);
 if(ok){
  in=0xFFFFFE10UL;(void)DeviceIoControl(vxd,0x85500012UL,&in,4,b,16,&got,0);
  in=0xA5;(void)DeviceIoControl(vxd,0x85500022UL,&in,4,l,32,&got,0);
  in=0xFFFFFE1CUL;(void)DeviceIoControl(vxd,0x85500012UL,&in,4,m,16,&got,0); /* v10.273: the binding's mapping */
  fprintf(logf,"; the engine did %lu of the 64, the VxD declined %lu; state %lu, timeouts %lu, resets %lu, last result %08lX;"
   " display driver bound %lu, enabled %s, BUSY %s, moved in memory %lu times (%lu mappings, %lu failed); 3D fault %lu\n",s1[4]-s0[4],s1[5]-s0[5],s1[2],s1[14],s1[15],s1[19],b[0]&1,(b[0]&2)?"no":"yes",(b[0]&4)?"set":"clear",m[0],m[1],m[2],l[0]==0xA5?l[2]:0);
 } else fprintf(logf,"; no VxD counters\n");
 DeleteObject(hb);DeleteObject(br);ReleaseDC(w,dc);DestroyWindow(w);
}
int main(int argc,char **argv) {
 int k;
 if(argc>1)scale=atoi(argv[1]);if(scale<1)scale=1;
 logf=fopen("VXTEX.TXT","w");if(!logf)return 1;setvbuf(logf,0,_IONBF,0);
 fprintf(logf,"VXTEX v1.8 (driver v10.366): OpenGL textures against worked-out answers, %dx%d\n",W,H);
 SetEnvironmentVariableA("VXGL_TEX16","0"); /* v1.8: the answers are 8 bits a channel; the ICD's R5G6B5 copies of RGB textures (ACCEL_OPENGL_TEX16, default 1) off for this run */
 hw_key_note(logf);
 {HDC d=GetDC(0);fprintf(logf,"desktop %dx%dx%d\n",GetDeviceCaps(d,HORZRES),GetDeviceCaps(d,VERTRES),GetDeviceCaps(d,BITSPIXEL));ReleaseDC(0,d);}
 vxd=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);
 check_2d("at the start");
 run(0);check_2d("after the engine run");
 {DWORD saved=0;int existed=0;if(hw_key(1,&saved,&existed)){run(2);hw_key(0,&saved,&existed);}else{fprintf(logf,"ACCEL_OPENGL_HW could not be set: no engine-off run\n");failures++;}}
 check_2d("after the engine-off run");
 run(1);check_2d("after Microsoft's OpenGL");
 fprintf(logf,"== the engine against the ICD without it (colour, every pixel, more than %d apart)\n",cmp_tol);
 for(k=0;k<NCASES;k++) {
  int d,at;
  if(cases[k].timed)continue;
  if(!shave[0][k]||!shave[2][k]){fprintf(logf,"%2d %-40s not drawn by both\n",k,cases[k].name);continue;}
  d=compare(k,&at);fprintf(logf,"%2d %-40s %5d pixels differ",k,cases[k].name,d);
  if(d){char nm[32];fprintf(logf,", first at %d,%d: %02X%02X%02X against %02X%02X%02X  FAIL",at%W,at/W,cimg[0][k][at*4],cimg[0][k][at*4+1],cimg[0][k][at*4+2],cimg[2][k][at*4],cimg[2][k][at*4+1],cimg[2][k][at*4+2]);
   failures++;sprintf(nm,"VXTEX_%02d_HW.BMP",k);bmp(nm,cimg[0][k]);sprintf(nm,"VXTEX_%02d_SW.BMP",k);bmp(nm,cimg[2][k]);}
  fprintf(logf,"\n");
 }
 fprintf(logf,"== timings: engine / engine off / Microsoft\n");
 fprintf(logf,"   a bind and a tiny triangle (microseconds):   %8.2f %8.2f %8.2f\n",tms[0][0],tms[2][0],tms[1][0]);
 fprintf(logf,"   textured fill (million pixels a second):     %8.2f %8.2f %8.2f\n",tms[0][1],tms[2][1],tms[1][1]);
 fprintf(logf,"   small textured quads (microseconds a quad):  %8.2f %8.2f %8.2f\n",tms[0][2],tms[2][2],tms[1][2]);
 fprintf(logf,"failures=%d\n",failures);fclose(logf);
 return failures;
}
#endif
