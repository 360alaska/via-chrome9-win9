/* VXGAME v1.2 (driver v10.361; v1.1 v10.359, v1.0 v10.358): game-like OpenGL workloads on the VX855 ICD, timed and compared under each driver setting that trades
 * exactness for speed, in one run with no registry change and no restart.
 *
 * "VXGAME" runs itself once per configuration as a child process with the settings in its environment (VXGL.DLL reads every
 * ACCEL_OPENGL_* key from an environment variable of the same name first, v10.358; VXGL_HW = 0 draws in software). Each child draws five
 * scenes, a fixed frame of each read back for the comparison, then the scene animated for N frames (default 150, "VXGAME n") and timed.
 * The software child's frames are the reference: each configuration is judged by how many pixels differ from it (a channel more than 16
 * apart; 17 is two 5-bit steps at 16 bpp, the engine's truncation against swrast's rounding).
 *
 * Scenes:
 *  world      Quake 2 style: a corridor of quads with a texture and a GL_CLAMP lightmap on a second unit (coordinates in 0..1), a sky with
 *             GL_CLAMP, glBegin/glEnd, depth cleared every frame (gl_ztrick 0), polygons cut at the window's edges
 *  particles  the world plus 1500 smooth points of size 3 in one glBegin, with the alpha test and blending (Quake 2's particles)
 *  q3arrays   Quake 3 style: vertex arrays with glLockArraysEXT and glDrawElements, a lightmap pass blended GL_DST_COLOR / GL_ZERO with
 *             depth LEQUAL, a pixel format with 24-bit depth and 8-bit stencil when there is one, a gamma ramp set and put back
 *  hud        2D: 600 alpha-tested font quads from 4 textures with a bind every few quads
 *  bigtex     1024 x 1024 textures with full mip chains, minified (long chains: ACCEL_OPENGL_LONGMIP)
 *
 * Configurations: exact (the defaults), SNAP=0, FASTPOINTS=1, strict (v1.2: LONGMIP=0, EXACT_GL.REG; v1.0 approx: CLAMP=1, LONGMIP=1),
 * gaming (GAMING.REG: FASTPOINTS=1), vsync (swap interval 1: frame rate capped at the refresh), software (the reference), noguard (v1.2:
 * ACCEL_3D_GUARD, on by default since v10.361, turned off in the VxD for that child through its control 0xFFFFFE30 and put back after;
 * run no other OpenGL program meanwhile), nosse (v1.1: ACCEL_OPENGL_SSE = 0, Mesa's C transform and clip test: its frames a second against exact's are what the SSE
 * kernels give; the software reference runs without them too). Every configuration but vsync runs with ACCEL_OPENGL_SWAPINTERVAL = 0.
 * Results: VXGAME.TXT (a table: frames a second and pixels off per scene and configuration; BLANK when a frame has nothing but the clear
 * colour, the symptom Quake 3 showed). */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <GL/gl.h>

#define W 320
#define H 240
#define NSCENE 5
#define NCFG 9
static const char *scene_name[NSCENE]={"world","particles","q3arrays","hud","bigtex"};
static const struct {const char *name,*env;} cfg[NCFG]={
 {"exact","ACCEL_OPENGL_SWAPINTERVAL=0"},
 {"snap0","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SNAP=0"},
 {"fastpts","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_FASTPOINTS=1"},
 {"strict","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_LONGMIP=0"}, /* v1.2: EXACT_GL.REG's LONGMIP 0 (the default is 1 since v10.361) */
 {"gaming","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_FASTPOINTS=1"}, /* v1.2: GAMING.REG (v10.361) */
 {"vsync","ACCEL_OPENGL_SWAPINTERVAL=1"},
 {"software","ACCEL_OPENGL_SWAPINTERVAL=0;VXGL_HW=0;ACCEL_OPENGL_SSE=0"},
 {"noguard","ACCEL_OPENGL_SWAPINTERVAL=0"}, /* v1.2: ACCEL_3D_GUARD turned off in the VxD for this child (control 0xFFFFFE30), then put back */
 {"nosse","ACCEL_OPENGL_SWAPINTERVAL=0;ACCEL_OPENGL_SSE=0"}};
static const char *env_keys[]={"ACCEL_OPENGL_SWAPINTERVAL","ACCEL_OPENGL_SNAP","ACCEL_OPENGL_FASTPOINTS","ACCEL_OPENGL_LONGMIP","VXGL_HW","ACCEL_OPENGL_SSE",0};

#ifndef GL_TEXTURE0_ARB
#define GL_TEXTURE0_ARB 0x84C0
#define GL_TEXTURE1_ARB 0x84C1
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
typedef void (APIENTRY *act_fn)(GLenum);typedef void (APIENTRY *mtc_fn)(GLenum,GLfloat,GLfloat);
typedef void (APIENTRY *lock_fn)(GLint,GLsizei);typedef void (APIENTRY *unlock_fn)(void);typedef BOOL (APIENTRY *swi_fn)(int);
static act_fn act,cact;static mtc_fn mtc;static lock_fn lockarr;static unlock_fn unlockarr;
static FILE *logf;static HDC dc;static HWND win;
static LRESULT CALLBACK proc(HWND h,UINT m,WPARAM w,LPARAM l){return DefWindowProcA(h,m,w,l);}
static double now_ms(void){static LARGE_INTEGER f;LARGE_INTEGER t;if(!f.QuadPart)QueryPerformanceFrequency(&f);QueryPerformanceCounter(&t);return (double)t.QuadPart*1000.0/(double)f.QuadPart;}
static unsigned rnd_s=12345;static float frand(void){rnd_s=rnd_s*1103515245u+12345u;return (float)((rnd_s>>8)&0xFFFF)/65535.0F;}

/* ------------------------------------------------------------------ textures */
static GLuint tex_wall,tex_light,tex_sky,tex_font[4],tex_big[2];
static void make_tex(GLuint *t,int w,int h,int kind,GLenum wrap,int mip) {
 static GLubyte buf[1024*1024*4];int x,y,l=0;
 glGenTextures(1,t);glBindTexture(GL_TEXTURE_2D,*t);
 for(;;){
  for(y=0;y<h;y++)for(x=0;x<w;x++){GLubyte *p=buf+(y*w+x)*4;
   switch(kind){
   case 0:p[0]=(GLubyte)(90+((x^y)&8)*8+l*20);p[1]=(GLubyte)(70+(x*5)%60);p[2]=(GLubyte)(50+(y*3)%50);p[3]=255;break; /* wall */
   case 1:{int v=(int)(255.0*(0.35+0.65*sin(x*0.4)*sin(y*0.4)*sin(x*0.4)*sin(y*0.4)));p[0]=p[1]=p[2]=(GLubyte)v;p[3]=255;}break; /* lightmap */
   case 2:p[0]=(GLubyte)(40+y*4);p[1]=(GLubyte)(60+y*3);p[2]=(GLubyte)(150+x%100);p[3]=255;break; /* sky */
   case 3:{int on=((x/2+y/3)&3)!=0&&x%8<6&&y%8<7;p[0]=255;p[1]=(GLubyte)(200+l);p[2]=(GLubyte)(80*kind);p[3]=on?255:0;}break; /* font */
   default:p[0]=(GLubyte)(l*25);p[1]=(GLubyte)((x/(w/8+1))*30);p[2]=(GLubyte)((y/(h/8+1))*30);p[3]=255;break; /* big: each level its own colour */
   }}
  glTexImage2D(GL_TEXTURE_2D,l,GL_RGBA,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,buf);
  if(!mip||(w==1&&h==1))break;
  if(w>1)w/=2;if(h>1)h/=2;l++;}
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,wrap);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,wrap);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,mip?GL_LINEAR_MIPMAP_NEAREST:GL_LINEAR);
}
static void make_textures(void) {
 make_tex(&tex_wall,64,64,0,GL_REPEAT,1);make_tex(&tex_light,16,16,1,GL_CLAMP,0);make_tex(&tex_sky,64,64,2,GL_CLAMP,0);
 {int k;for(k=0;k<4;k++)make_tex(&tex_font[k],64,64,3,GL_CLAMP_TO_EDGE,0);}
 make_tex(&tex_big[0],1024,1024,4,GL_REPEAT,1);make_tex(&tex_big[1],512,512,4,GL_REPEAT,1);
}

/* ------------------------------------------------------------------ scenes */
static void persp(float fov) {double t=tan(fov*3.14159/360.0)*0.5;glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-t*4.0/3.0,t*4.0/3.0,-t,t,0.5,200.0);glMatrixMode(GL_MODELVIEW);glLoadIdentity();}
static void reset_state(void) {
 glViewport(0,0,W,H);glDisable(GL_BLEND);glDisable(GL_ALPHA_TEST);glDisable(GL_POINT_SMOOTH);glDepthMask(GL_TRUE);glDepthFunc(GL_LEQUAL);glEnable(GL_DEPTH_TEST);
 glDisable(GL_CULL_FACE);glShadeModel(GL_SMOOTH);glDisable(GL_DITHER);glColor4f(1,1,1,1);
 if(act){act(GL_TEXTURE1_ARB);glDisable(GL_TEXTURE_2D);act(GL_TEXTURE0_ARB);}
 glEnable(GL_TEXTURE_2D);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
}
/* the corridor: walls, floor and ceiling of quads, the camera moving down it (frame f) */
static void world(int f) {
 int i,k;float z0=-(float)(f%40)*0.25F;
 glClearColor(0.05F,0.05F,0.1F,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 persp(90);glRotatef((float)(f%360)*0.3F,0,1,0);glTranslatef(0,-0.5F,z0);
 /* sky: one GL_CLAMP quad far away, coordinates in 0..1 */
 glDepthMask(GL_FALSE);glBindTexture(GL_TEXTURE_2D,tex_sky);
 glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex3f(-150,-20,-180);glTexCoord2f(1,0);glVertex3f(150,-20,-180);glTexCoord2f(1,1);glVertex3f(150,120,-180);glTexCoord2f(0,1);glVertex3f(-150,120,-180);glEnd();
 glDepthMask(GL_TRUE);
 glBindTexture(GL_TEXTURE_2D,tex_wall);
 if(act&&mtc){act(GL_TEXTURE1_ARB);glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,tex_light);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);act(GL_TEXTURE0_ARB);}
 for(i=0;i<40;i++)for(k=0;k<4;k++){float z=-(float)i*2.0F,x0=k==0?-2.0F:k==1?2.0F:-2.0F,x1=k==0?-2.0F:k==1?2.0F:2.0F,y=k==2?0.0F:k==3?3.0F:0.0F;int s;
  glBegin(GL_QUADS);
  for(s=0;s<4;s++){float u=(s==1||s==2)?1.0F:0.0F,v=(s>=2)?1.0F:0.0F,px,py,pz=z-v*2.0F;
   if(k<2){px=x0;py=u*3.0F;}else{px=x0+u*(x1-x0);py=y;}
   glTexCoord2f(u*2.0F,v*2.0F);if(mtc)mtc(GL_TEXTURE1_ARB,u,v);glVertex3f(px,py,pz);}
  glEnd();}
 if(act){act(GL_TEXTURE1_ARB);glDisable(GL_TEXTURE_2D);act(GL_TEXTURE0_ARB);}
}
static void particles(int f) {
 int i;world(f);rnd_s=777u+(unsigned)f*7u;
 glDisable(GL_TEXTURE_2D);glEnable(GL_POINT_SMOOTH);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.1F);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
 glDepthMask(GL_FALSE);
 /* v1.2: one glBegin at one size, as Quake 2 3.20 draws its particles (v1.0 set the size and began a primitive for each point: 1500
  * pipeline runs a frame, the cost of the calls rather than of the points) */
 glPointSize(3.0F);glBegin(GL_POINTS);for(i=0;i<1500;i++){glColor4f(1.0F,0.4F+0.5F*frand(),0.1F,0.3F+0.7F*frand());glVertex3f(-1.8F+3.6F*frand(),0.2F+2.6F*frand(),-1.0F-30.0F*frand());}glEnd();
 glDepthMask(GL_TRUE);glDisable(GL_POINT_SMOOTH);glDisable(GL_ALPHA_TEST);glDisable(GL_BLEND);glEnable(GL_TEXTURE_2D);glColor4f(1,1,1,1);
}
/* Quake 3 style: arrays, glLockArraysEXT, glDrawElements; base pass, then the lightmap pass multiplied in (GL_DST_COLOR, GL_ZERO) */
#define Q3N 33
static GLfloat q3v[Q3N*Q3N*3],q3t[Q3N*Q3N*2],q3l[Q3N*Q3N*2];static GLubyte q3c[Q3N*Q3N*4];static GLushort q3i[(Q3N-1)*(Q3N-1)*6];
static void q3arrays(int f) {
 int x,y,n=0;float ph=(float)f*0.05F;
 for(y=0;y<Q3N;y++)for(x=0;x<Q3N;x++){int k=y*Q3N+x;q3v[k*3]=(float)x-16.0F;q3v[k*3+1]=(float)(sin(x*0.5+ph)+cos(y*0.4+ph))*0.7F-2.0F;q3v[k*3+2]=-(float)y-2.0F;
  q3t[k*2]=(float)x*0.25F;q3t[k*2+1]=(float)y*0.25F;q3l[k*2]=(float)x/(Q3N-1);q3l[k*2+1]=(float)y/(Q3N-1);
  q3c[k*4]=(GLubyte)(150+x*3);q3c[k*4+1]=(GLubyte)(150+y*3);q3c[k*4+2]=200;q3c[k*4+3]=255;}
 for(y=0;y<Q3N-1;y++)for(x=0;x<Q3N-1;x++){GLushort a=(GLushort)(y*Q3N+x);q3i[n++]=a;q3i[n++]=(GLushort)(a+1);q3i[n++]=(GLushort)(a+Q3N);q3i[n++]=(GLushort)(a+1);q3i[n++]=(GLushort)(a+Q3N+1);q3i[n++]=(GLushort)(a+Q3N);}
 glClearColor(0.0F,0.0F,0.0F,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);
 persp(75);glRotatef(20,1,0,0);glRotatef((float)(f%90)-45.0F,0,1,0);
 glEnableClientState(GL_VERTEX_ARRAY);glVertexPointer(3,GL_FLOAT,0,q3v);
 glEnableClientState(GL_TEXTURE_COORD_ARRAY);glTexCoordPointer(2,GL_FLOAT,0,q3t);
 glEnableClientState(GL_COLOR_ARRAY);glColorPointer(4,GL_UNSIGNED_BYTE,0,q3c);
 if(lockarr)lockarr(0,Q3N*Q3N);
 glBindTexture(GL_TEXTURE_2D,tex_wall);glDrawElements(GL_TRIANGLES,n,GL_UNSIGNED_SHORT,q3i);
 glDisableClientState(GL_COLOR_ARRAY);glColor4f(1,1,1,1);glTexCoordPointer(2,GL_FLOAT,0,q3l);
 glEnable(GL_BLEND);glBlendFunc(GL_DST_COLOR,GL_ZERO);glDepthMask(GL_FALSE);glDepthFunc(GL_EQUAL);
 glBindTexture(GL_TEXTURE_2D,tex_light);glDrawElements(GL_TRIANGLES,n,GL_UNSIGNED_SHORT,q3i);
 glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glDisable(GL_BLEND);
 if(unlockarr)unlockarr();
 glDisableClientState(GL_VERTEX_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);
}
static void hud(int f) {
 int i;glClearColor(0.2F,0.25F,0.3F,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,W,0,H,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 glDisable(GL_DEPTH_TEST);glEnable(GL_ALPHA_TEST);glAlphaFunc(GL_GREATER,0.5F);
 for(i=0;i<600;i++){float x=(float)((i*13+f)%(W-8)),y=(float)((i*7)%(H-8));if(i%5==0)glBindTexture(GL_TEXTURE_2D,tex_font[(i/5)&3]);
  glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex2f(x,y);glTexCoord2f(0.125F,0);glVertex2f(x+8,y);glTexCoord2f(0.125F,0.125F);glVertex2f(x+8,y+8);glTexCoord2f(0,0.125F);glVertex2f(x,y+8);glEnd();}
 glDisable(GL_ALPHA_TEST);glEnable(GL_DEPTH_TEST);
}
static void bigtex(int f) {
 int i;glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);persp(60);
 glTranslatef(0,0,-3.0F-(float)(f%50)*0.3F);
 for(i=0;i<2;i++){glBindTexture(GL_TEXTURE_2D,tex_big[i]);glBegin(GL_QUADS);
  glTexCoord2f(0,0);glVertex3f(-4.0F+i*4.0F,-1,0);glTexCoord2f(24,0);glVertex3f(i*4.0F,-1,0);glTexCoord2f(24,24);glVertex3f(i*4.0F,-1,-60);glTexCoord2f(0,24);glVertex3f(-4.0F+i*4.0F,-1,-60);glEnd();}
}
static void draw_scene(int s,int f){reset_state();switch(s){case 0:world(f);break;case 1:particles(f);break;case 2:q3arrays(f);break;case 3:hud(f);break;default:bigtex(f);break;}}

/* ------------------------------------------------------------------ the child: one configuration */
static int child(int ci,int frames) {
 WNDCLASSA wc;HGLRC rc;PIXELFORMATDESCRIPTOR p;int pf,s,i,best=0,score=-1,n;RECT r;static GLubyte img[W*H*4];char fn[64];
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="VXGAME";wc.style=CS_OWNDC;RegisterClassA(&wc);
 r.left=0;r.top=0;r.right=W;r.bottom=H;AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE);
 win=CreateWindowA("VXGAME",cfg[ci].name,WS_OVERLAPPEDWINDOW|WS_VISIBLE,60,60,r.right-r.left,r.bottom-r.top,0,0,wc.hInstance,0);
 if(!win){fprintf(logf,"%s: no window\n",cfg[ci].name);return 1;}
 dc=GetDC(win);n=DescribePixelFormat(dc,1,sizeof(p),&p);
 for(i=1;i<=n;i++){int sc;DescribePixelFormat(dc,i,sizeof(p),&p);
  if(!(p.dwFlags&PFD_SUPPORT_OPENGL)||!(p.dwFlags&PFD_DOUBLEBUFFER)||(p.dwFlags&PFD_GENERIC_FORMAT)||p.cDepthBits<16)continue;
  sc=(p.cStencilBits>=8?100:0)+(p.cDepthBits>=24?10:0); /* Quake 3 asks for 24-bit depth and 8-bit stencil */
  if(sc>score){score=sc;best=i;}}
 if(!best){fprintf(logf,"%s: no ICD pixel format\n",cfg[ci].name);return 1;}
 DescribePixelFormat(dc,best,sizeof(p),&p);if(!SetPixelFormat(dc,best,&p)){fprintf(logf,"%s: SetPixelFormat failed\n",cfg[ci].name);return 1;}
 rc=wglCreateContext(dc);if(!rc||!wglMakeCurrent(dc,rc)){fprintf(logf,"%s: no context\n",cfg[ci].name);return 1;}
 act=(act_fn)wglGetProcAddress("glActiveTextureARB");mtc=(mtc_fn)wglGetProcAddress("glMultiTexCoord2fARB");
 lockarr=(lock_fn)wglGetProcAddress("glLockArraysEXT");unlockarr=(unlock_fn)wglGetProcAddress("glUnlockArraysEXT");
 fprintf(logf,"%s: %s, format %d (depth %d stencil %d), %s; arrays lock %s\n",cfg[ci].name,(const char*)glGetString(GL_RENDERER),best,p.cDepthBits,p.cStencilBits,
  (const char*)glGetString(GL_VERSION),lockarr?"yes":"no");
 make_textures();
 if(ci==0){WORD ramp[3][256],keep[3][256];int k; /* Quake 3 sets the gamma ramp: set a brighter one and put the old one back */
  if(GetDeviceGammaRamp(dc,keep)){for(k=0;k<256;k++){unsigned v=(unsigned)(sqrt(k/255.0)*65535.0);ramp[0][k]=ramp[1][k]=ramp[2][k]=(WORD)v;}
   fprintf(logf,"%s: gamma ramp set %s, put back %s\n",cfg[ci].name,SetDeviceGammaRamp(dc,ramp)?"ok":"FAILED",SetDeviceGammaRamp(dc,keep)?"ok":"FAILED");}
  else fprintf(logf,"%s: GetDeviceGammaRamp failed\n",cfg[ci].name);}
 for(s=0;s<NSCENE;s++){double t0,t1;int f,nf=ci==6?(frames<20?frames:20):frames,blank=1;FILE *o;
  draw_scene(s,0);glFinish();glReadBuffer(GL_BACK);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,W,H,GL_RGBA,GL_UNSIGNED_BYTE,img);
  for(i=1;i<W*H&&blank;i++)if(memcmp(img+i*4,img,3))blank=0;
  sprintf(fn,"VXGAME%d%d.RAW",ci,s);o=fopen(fn,"wb");if(o){fwrite(img,1,sizeof(img),o);fclose(o);}
  SwapBuffers(dc);
  t0=now_ms();for(f=1;f<=nf;f++){MSG m;draw_scene(s,f);SwapBuffers(dc);while(PeekMessageA(&m,0,0,0,PM_REMOVE))DispatchMessageA(&m);}
  glFinish();t1=now_ms();
  fprintf(logf,"R %d %d %.2f %d %lu\n",ci,s,nf*1000.0/(t1-t0),blank,(unsigned long)glGetError());fflush(logf);}
 wglMakeCurrent(0,0);wglDeleteContext(rc);ReleaseDC(win,dc);DestroyWindow(win);
 return 0;
}

/* ------------------------------------------------------------------ the parent: every configuration, then the table */
/* the VxD's ACCEL_3D_GUARD switch (v10.358): 1 on, 0 off; VXGL.DLL reads it when a process loads it */
/* v1.2: on = 2 reads the setting (1 on, 0 off, -1 no answer); the guard row turns it off for its child (noguard: the default is on since
 * v10.361) and puts back what it was */
static int vxd_guard(int on) {
 HANDLE h=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);DWORD in=on==2?0xFFFFFE32UL:on?0xFFFFFE31UL:0xFFFFFE30UL,o[4]={0,0,0,0},got=0;int ok;
 if(on==2){if(h==INVALID_HANDLE_VALUE)return -1;ok=DeviceIoControl(h,0x85500012UL,&in,4,o,16,&got,NULL)&&o[1]==0x47524431UL;CloseHandle(h);return ok?(int)o[0]:-1;}
 if(h==INVALID_HANDLE_VALUE)return 0;
 ok=DeviceIoControl(h,0x85500012UL,&in,4,o,16,&got,NULL)&&o[1]==0x47524431UL&&o[0]==(DWORD)on;CloseHandle(h);return ok;
}
int main(int argc,char **argv) {
 int guard_was=1;int frames=150,ci,s;double fps[NCFG][NSCENE];int blank[NCFG][NSCENE],have[NCFG][NSCENE];long off[NCFG][NSCENE];char self[MAX_PATH],line[256];
 if(argc>2&&!strcmp(argv[1],"/child")){ci=atoi(argv[2]);if(argc>3)frames=atoi(argv[3]);logf=fopen("VXGAME.LOG","a");if(!logf)return 1;s=child(ci,frames);fclose(logf);return s;}
 if(argc>1)frames=atoi(argv[1]);if(frames<10)frames=10;
 remove("VXGAME.LOG");GetModuleFileNameA(0,self,sizeof(self));
 for(ci=0;ci<NCFG;ci++){char cmd[MAX_PATH+40],env[256],*e;STARTUPINFOA si;PROCESS_INFORMATION pi;int k;
  for(k=0;env_keys[k];k++)SetEnvironmentVariableA(env_keys[k],NULL);
  strcpy(env,cfg[ci].env);for(e=strtok(env,";");e;e=strtok(0,";")){char *q=strchr(e,'=');if(q){*q=0;SetEnvironmentVariableA(e,q+1);}}
  if(ci==7&&(guard_was=vxd_guard(2))<0){continue;} /* (the VxD did not answer: no noguard row) */
  if(ci==7)vxd_guard(0);
  sprintf(cmd,"\"%s\" /child %d %d",self,ci,frames);memset(&si,0,sizeof(si));si.cb=sizeof(si);
  if(CreateProcessA(0,cmd,0,0,FALSE,0,0,0,&si,&pi)){WaitForSingleObject(pi.hProcess,INFINITE);CloseHandle(pi.hProcess);CloseHandle(pi.hThread);}
  if(ci==7)vxd_guard(guard_was);}
 {int k;for(k=0;env_keys[k];k++)SetEnvironmentVariableA(env_keys[k],NULL);}
 memset(have,0,sizeof(have));
 {FILE *l=fopen("VXGAME.LOG","r");if(l){while(fgets(line,sizeof(line),l)){int a,b,bl;unsigned long er;double v;if(sscanf(line,"R %d %d %lf %d %lu",&a,&b,&v,&bl,&er)==5&&a>=0&&a<NCFG&&b>=0&&b<NSCENE){fps[a][b]=v;blank[a][b]=bl;have[a][b]=1;}}fclose(l);}}
 for(ci=0;ci<NCFG;ci++)for(s=0;s<NSCENE;s++){char fa[32],fb[32];FILE *a,*b;static GLubyte ia[W*H*4],ib[W*H*4];long d=-1;
  sprintf(fa,"VXGAME%d%d.RAW",ci,s);sprintf(fb,"VXGAME6%d.RAW",s);a=fopen(fa,"rb");b=fopen(fb,"rb");
  if(a&&b&&fread(ia,1,sizeof(ia),a)==sizeof(ia)&&fread(ib,1,sizeof(ib),b)==sizeof(ib)){int i,ch;d=0;for(i=0;i<W*H;i++)for(ch=0;ch<3;ch++)if(abs(ia[i*4+ch]-ib[i*4+ch])>16){d++;break;}}
  if(a)fclose(a);if(b)fclose(b);off[ci][s]=d;}
 for(ci=0;ci<NCFG;ci++)for(s=0;s<NSCENE;s++){char fn[32];sprintf(fn,"VXGAME%d%d.RAW",ci,s);remove(fn);}
 logf=fopen("VXGAME.TXT","w");if(!logf)return 1;
 fprintf(logf,"VXGAME v1.2 (driver v10.361): %dx%d, %d frames a scene (software 20); frames a second / pixels off the software frame (%d pixels)\n",W,H,frames,W*H);
 {HANDLE h=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);DWORD in=0xFFFFFE34UL,o[4]={0,0,0,0},got=0; /* the VxD's SSE report, as VXGL.DLL reads it */
  if(h!=INVALID_HANDLE_VALUE&&DeviceIoControl(h,0x85500012UL,&in,4,o,16,&got,NULL)&&o[1]==0x31455353UL)
   fprintf(logf,"SSE: processor %s, FXSR %s, system saves XMM registers (CR4 OSFXSR) %s: the SSE transform is %s in every row but nosse and software\n",
    o[2]&0x02000000UL?"yes":"no",o[2]&0x01000000UL?"yes":"no",o[0]&0x200UL?"yes":"no",(o[2]&0x03000000UL)==0x03000000UL&&(o[0]&0x200UL)?"ON":"OFF (all rows the same code)");
  else fprintf(logf,"SSE: the VxD gave no report (older VxD): the SSE transform is off\n");
  if(h!=INVALID_HANDLE_VALUE)CloseHandle(h);}
 fprintf(logf,"%-10s","");for(s=0;s<NSCENE;s++)fprintf(logf,"%18s",scene_name[s]);fprintf(logf,"\n");
 for(ci=0;ci<NCFG;ci++){fprintf(logf,"%-10s",cfg[ci].name);
  for(s=0;s<NSCENE;s++){char c[40];if(!have[ci][s])strcpy(c,"-");else sprintf(c,"%.1f/%ld%s",fps[ci][s],off[ci][s],blank[ci][s]?" BLANK":"");fprintf(logf,"%18s",c);}
  fprintf(logf,"\n");}
 fprintf(logf,"\nsettings:\n");for(ci=0;ci<NCFG;ci++)fprintf(logf,"  %-9s %s\n",cfg[ci].name,cfg[ci].env);
 {FILE *l=fopen("VXGAME.LOG","r");fprintf(logf,"\nchildren:\n");if(l){while(fgets(line,sizeof(line),l))if(line[0]!='R'||line[1]!=' ')fputs(line,logf);fclose(l);}}
 fclose(logf);
 MessageBoxA(0,"VXGAME done: results in VXGAME.TXT","VXGAME",MB_OK);
 return 0;
}
