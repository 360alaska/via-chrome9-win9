/* The scenes of VXGLIX (.573's board tool for the OpenGL driver's primitives by index, removed in .574 with the switch), kept for the
 * host's copy of its sequence (gl/host/vxgl_host.c, VXGL_GLIX: by index off and on, the pictures the same): include after
 * vx_qscenes.h with GXW and GXH the scenes' size (320 x 240) and GXVW and GXVH the viewport's. */
#ifndef VX_GLIX_SCENES_H
#define VX_GLIX_SCENES_H
static void glx_ortho(void) {glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,GXW,0,GXH,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();}
static void glx_reset(void) {glDisable(GL_TEXTURE_2D);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_LIGHTING);glDisable(GL_FOG);glDepthMask(GL_TRUE);glDisable(GL_DEPTH_TEST);glShadeModel(GL_SMOOTH);
 glDisableClientState(GL_VERTEX_ARRAY);glDisableClientState(GL_COLOR_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);glColor4f(1,1,1,1);glFrontFace(GL_CCW);glCullFace(GL_BACK);}
/* every vertex with texture coordinates: the driver prepacks (and so indexes) textured draws only */
#define GLX_V(x,y) do{const GLfloat x_=(x),y_=(y);glTexCoord2f(x_*(1.0f/64.0f),y_*(1.0f/64.0f));glVertex2f(x_,y_);}while(0)
static void glx_col(int a,int b) {glColor3ub((GLubyte)(60+(a*37)%190),(GLubyte)(60+(b*53)%190),(GLubyte)(90+((a+b)*29)%160));}
#define GX 24
#define GY 18
static GLfloat gxy[(GX+1)*(GY+1)][2];static GLfloat gtc[(GX+1)*(GY+1)][2];static GLubyte gcol[(GX+1)*(GY+1)][4];static GLuint gix[GX*GY*6];
static void glx_scene(int sc,int f) {
 int s,k,i,j;
 glViewport(0,0,GXVW,GXVH);glClearColor(0.1f,0.0f,0.15f,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);glx_reset();
 if(sc==0){glEnable(GL_DEPTH_TEST);qs_q2(f);glx_reset();return;}
 if(sc==1){glEnable(GL_DEPTH_TEST);qs_q3(f);glx_reset();return;}
 glx_ortho();
 glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,qs_tex[sc%QS_NTEX]);
#ifdef GLX_NOTEX
 if(GLX_NOTEX)glDisable(GL_TEXTURE_2D); /* the host's count of the untextured form (.574) */
#endif
 if(sc==2||sc==5){
  if(sc==5)glShadeModel(GL_FLAT);else{glEnable(GL_CULL_FACE);glCullFace((f&1)?GL_FRONT:GL_BACK);}
  for(s=0;s<24;s++){float y0=6.0f+s*9.5f,y1=y0+8.5f;glBegin(GL_TRIANGLE_STRIP);
   for(k=0;k<48;k++){float x=6.0f+k*(308.0f/47.0f);int fold=sc==2&&(((k/12)+s)&3)==0;glx_col(s*3+k,k);GLX_V(x,fold?y1:y0);glx_col(s+k,s*5+k);GLX_V(x,fold?y0:y1);}
   glEnd();}
  return;}
 if(sc==3){
  for(s=0;s<60;s++){float cx=20.0f+(s%10)*31.0f,cy=20.0f+(s/10)*38.0f;int n=5+s%7;
   if(s%3==0){glBegin(GL_TRIANGLE_FAN);glx_col(s,1);GLX_V(cx,cy);for(i=0;i<=n;i++){glx_col(s,i+2);GLX_V(cx+13.0f*(float)cos(i*6.2832/n),cy+16.0f*(float)sin(i*6.2832/n));}glEnd();}
   else if(s%3==1){glBegin(GL_POLYGON);for(i=0;i<n;i++){glx_col(s+i,3);GLX_V(cx+13.0f*(float)cos(i*6.2832/n),cy+16.0f*(float)sin(i*6.2832/n));}glEnd();}
   else{glBegin(GL_QUADS);for(i=0;i<3;i++){float x=cx-13.0f+i*9.0f;glx_col(s,i);GLX_V(x,cy-14.0f);glx_col(s+1,i);GLX_V(x+8.0f,cy-14.0f);glx_col(s+2,i);GLX_V(x+8.0f,cy+14.0f);glx_col(s+3,i);GLX_V(x,cy+14.0f);}glEnd();}}
  glBegin(GL_QUAD_STRIP);for(k=0;k<30;k++){glx_col(k,7);GLX_V(8.0f+k*10.3f,4.0f);glx_col(k,9);GLX_V(8.0f+k*10.3f,12.0f);}glEnd();
  return;}
 /* 4: the grid from -60 to W + 60 and -40 to H + 40: the outer cells cross the window's edge or lie outside */
 for(j=0;j<=GY;j++)for(i=0;i<=GX;i++){k=j*(GX+1)+i;gxy[k][0]=-60.0f+i*(float)(GXW+120)/GX+(float)((i*7+j*3+f)%5);gxy[k][1]=-40.0f+j*(float)(GXH+80)/GY;
  gtc[k][0]=gxy[k][0]*(1.0f/64.0f);gtc[k][1]=gxy[k][1]*(1.0f/64.0f);gcol[k][0]=(GLubyte)(50+(i*31)%200);gcol[k][1]=(GLubyte)(50+(j*47)%200);gcol[k][2]=(GLubyte)(120+((i+j)*13)%130);gcol[k][3]=255;}
 k=0;for(j=0;j<GY;j++)for(i=0;i<GX;i++){GLuint a=(GLuint)(j*(GX+1)+i);int turn=((i/5)+j)%4==0; /* a quarter wound the other way */
  if(turn){gix[k++]=a;gix[k++]=a+GX+1;gix[k++]=a+1;gix[k++]=a+1;gix[k++]=a+GX+1;gix[k++]=a+GX+2;}
  else{gix[k++]=a;gix[k++]=a+1;gix[k++]=a+GX+1;gix[k++]=a+1;gix[k++]=a+GX+2;gix[k++]=a+GX+1;}}
 glEnable(GL_CULL_FACE);glCullFace((f&1)?GL_FRONT:GL_BACK);
 glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_COLOR_ARRAY);glVertexPointer(2,GL_FLOAT,0,gxy);glColorPointer(4,GL_UNSIGNED_BYTE,0,gcol);glEnableClientState(GL_TEXTURE_COORD_ARRAY);glTexCoordPointer(2,GL_FLOAT,0,gtc);
 if(qs_lock)qs_lock(0,(GX+1)*(GY+1));
 glDrawElements(GL_TRIANGLES,GX*GY*6,GL_UNSIGNED_INT,gix);
 if(qs_unlock)qs_unlock();
}
#endif
