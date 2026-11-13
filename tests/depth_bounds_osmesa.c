/* Executes the exact pixel suite shipped in VXFEAT against this project's
 * bundled Mesa. OSMesa supplies CPU buffers, not the VX855/Win98 HAL. */
#include <GL/osmesa.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#define DEPTH_BOUNDS 0x8890
#define SAMPLES_PASSED 0x8914
#define QUERY_RESULT 0x8866
typedef void (GLAPIENTRY *Bounds)(GLclampd,GLclampd);
typedef void (GLAPIENTRY *GenObj)(GLsizei,GLuint*);
typedef void (GLAPIENTRY *DelObj)(GLsizei,const GLuint*);
typedef void (GLAPIENTRY *BeginQ)(GLenum,GLuint);
typedef void (GLAPIENTRY *EndQ)(GLenum);
typedef void (GLAPIENTRY *GetQ)(GLuint,GLenum,GLuint*);
static int passed,failed;
static void say(const char *fmt,...){va_list a;va_start(a,fmt);vprintf(fmt,a);va_end(a);putchar('\n');}
static void check(const char *name,int ok){if(ok)passed++;else failed++;say("%s: %s",ok?"PASS":"FAIL",name);}
static OSMESAproc proc(const char *name){return OSMesaGetProcAddress(name);}
static void reset(void){
 glDisable(DEPTH_BOUNDS);glDisable(GL_TEXTURE_2D);glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);
 glDisable(GL_STENCIL_TEST);glDisable(GL_SCISSOR_TEST);glDisable(GL_LIGHTING);glDisable(GL_CULL_FACE);
 glColorMask(1,1,1,1);glDepthMask(1);glDepthFunc(GL_LESS);glEnable(GL_DEPTH_TEST);
 glViewport(0,0,256,128);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,256,0,128,-1,1);
 glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDrawBuffer(GL_FRONT);glReadBuffer(GL_FRONT);
 glClearColor(0,0,1,1);glClearDepth(1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 while(glGetError()!=GL_NO_ERROR);
}
static void quad(float l,float b,float r,float t,float z){glBegin(GL_QUADS);
 glTexCoord2f(0,0);glVertex3f(l,b,z);glTexCoord2f(1,0);glVertex3f(r,b,z);
 glTexCoord2f(1,1);glVertex3f(r,t,z);glTexCoord2f(0,1);glVertex3f(l,t,z);glEnd();}
static int pixel(int x,int y,int r,int g,int b){GLubyte p[3];glPixelStorei(GL_PACK_ALIGNMENT,1);
 glReadPixels(x,y,1,1,GL_RGB,GL_UNSIGNED_BYTE,p);say("  pixel %d,%d = %u,%u,%u",x,y,p[0],p[1],p[2]);
 return abs((int)p[0]-r)<=8&&abs((int)p[1]-g)<=8&&abs((int)p[2]-b)<=8;}
#include "../tools/vx_bounds_checks.h"
int main(void){static GLubyte buffer[256*128*4];int i;static const GLint depth[3]={16,24,32};
 for(i=0;i<3;i++){
  GLint actual=0;Bounds bounds;OSMesaContext ctx=OSMesaCreateContextExt(OSMESA_RGBA,depth[i],8,0,0);
  if(!ctx||!OSMesaMakeCurrent(ctx,buffer,GL_UNSIGNED_BYTE,256,128))return 2;
  glGetIntegerv(GL_DEPTH_BITS,&actual);say("-- bundled Mesa RGBA, requested D%d, actual D%d --",depth[i],actual);
  bounds=(Bounds)proc("glDepthBoundsEXT");check("depth bounds function resolves",bounds!=0);
  if(bounds)bounds_checks(bounds);OSMesaDestroyContext(ctx);
 }
 {OSMesaContext ctx=OSMesaCreateContextExt(OSMESA_RGBA,0,0,0,0);Bounds bounds;
  if(!ctx||!OSMesaMakeCurrent(ctx,buffer,GL_UNSIGNED_BYTE,256,128))return 2;
  reset();bounds=(Bounds)proc("glDepthBoundsEXT");bounds(0,0);glEnable(DEPTH_BOUNDS);
  glColor3f(1,0,0);quad(16,16,112,112,0);
  check("bounds ignored when framebuffer has no depth buffer",pixel(64,64,255,0,0));OSMesaDestroyContext(ctx);}
 say("%d passed, %d failed",passed,failed);return failed?1:0;
}
