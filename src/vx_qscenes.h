/* v10.500: two scenes in the way Quake 2 and Quake 3 draw (VXFLAT v2.8 and the host bench, gl/host/vxgl_host.c VXGL_BENCH 7 and 8).
 *
 * qs_q2: a room of textured polygons drawn as Quake 2's world: glBegin(GL_POLYGON), glTexCoord2f and glVertex3fv a vertex, the polygons
 * sorted by texture (a glBindTexture each), then the lightmap pass (blend ZERO, SRC_COLOR, depth LEQUAL, no depth writes) sorted by
 * lightmap; a lightmap block's rows replaced by glTexSubImage2D each frame (a dynamic light); a model as strips and fans with glColor3f,
 * glTexCoord2f and glVertex3fv a vertex.
 * qs_q3: Quake 3's batches: vertex arrays (position 4 floats a vertex, texture coordinates, colours GL_UNSIGNED_BYTE), glLockArraysEXT, the
 * batch drawn twice with glDrawElements(GL_TRIANGLES, GL_UNSIGNED_INT) (the base texture with the vertex colours, then the lightmap pass
 * blended DST_COLOR, ZERO with the colour array off), glUnlockArraysEXT; half of the batches a part of one large array (the indices not
 * starting at 0).
 * The caller sets the viewport to QS_W x QS_H; qs_init makes the textures (once a context). MIT, 2026. */
#ifndef VX_QSCENES_H
#define VX_QSCENES_H
#ifndef QS_API
#define QS_API APIENTRY
#endif
#define QS_W 320
#define QS_H 240
#define QS_NTEX 8
#define QS_NLM 4
typedef void (QS_API *qs_lock_fn)(GLint,GLsizei);
typedef void (QS_API *qs_unlock_fn)(void);
static GLuint qs_tex[QS_NTEX],qs_lm[QS_NLM],qs_skin;static int qs_ready;
static qs_lock_fn qs_lock;static qs_unlock_fn qs_unlock;
static GLubyte qs_img[64*64*4];
static void qs_pattern(int seed,int w,int h,int lm) {int x,y;
 for(y=0;y<h;y++)for(x=0;x<w;x++){GLubyte *p=qs_img+(y*w+x)*4;int v=((x*7+seed*13)^(y*5+seed*3))&63;
  if(lm){int l=96+((x*x+y*3+seed*29)%160);p[0]=(GLubyte)l;p[1]=(GLubyte)(l-(seed*9)%40);p[2]=(GLubyte)(l-(x%32));p[3]=255;}
  else{p[0]=(GLubyte)(64+v*2+seed*11);p[1]=(GLubyte)(40+((x+y+seed*7)&127));p[2]=(GLubyte)(90+((x^y)&63)+seed*5);p[3]=255;}}}
static void qs_mk(GLuint t,int seed,int lm) {
 glBindTexture(GL_TEXTURE_2D,t);qs_pattern(seed,64,64,lm);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
 glTexImage2D(GL_TEXTURE_2D,0,lm?GL_RGB:GL_RGBA,64,64,0,GL_RGBA,GL_UNSIGNED_BYTE,qs_img);}
static void qs_init(qs_lock_fn lk,qs_unlock_fn ul) {int i;
 qs_lock=lk;qs_unlock=ul;if(qs_ready)return;
 glGenTextures(QS_NTEX,qs_tex);glGenTextures(QS_NLM,qs_lm);glGenTextures(1,&qs_skin);
 for(i=0;i<QS_NTEX;i++)qs_mk(qs_tex[i],i,0);for(i=0;i<QS_NLM;i++)qs_mk(qs_lm[i],i,1);qs_mk(qs_skin,11,0);
 glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);qs_ready=1;}
static void qs_view(int f) {
 glMatrixMode(GL_PROJECTION);glLoadIdentity();glFrustum(-0.8,0.8,-0.6,0.6,1,200);
 glMatrixMode(GL_MODELVIEW);glLoadIdentity();glRotatef(-90,1,0,0);glRotatef(90,0,0,1); /* Quake's axes: x forward, z up */
 glRotatef(-(float)(f%7)*0.5f,0,0,1);glTranslatef(-8.0f-(float)(f%5)*0.25f,0,-2.0f);
 glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_TRUE);glEnable(GL_TEXTURE_2D);glShadeModel(GL_SMOOTH);
 glDisable(GL_BLEND);glDisable(GL_LIGHTING);glDisable(GL_FOG);glDisable(GL_CULL_FACE);glColor4f(1,1,1,1);}
/* the room: a floor and a ceiling (16 x 8 cells) and two side walls (16 x 4), x forward from 4 to 132, y -32 to 32, z -16 to 16; a cell
 * a polygon of 4 or 5 vertices (every third cell a midpoint on its first edge, as Quake's split faces) */
#define QS_NSURF (16*8*2+16*4*2)
static GLfloat qs_sv[QS_NSURF][5][3];static GLfloat qs_st[QS_NSURF][5][2],qs_ls[QS_NSURF][5][2];static int qs_sn[QS_NSURF],qs_stex[QS_NSURF],qs_slm[QS_NSURF],qs_built;
static void qs_cell(int n,int i,int j,int kind) {float a0=4.0f+(float)i*8.0f,a1=a0+8.0f;float b0,b1;int k,m=0;GLfloat c[4][3];
 if(kind<2){b0=-32.0f+(float)j*8.0f;b1=b0+8.0f;{float z=kind?16.0f:-16.0f;c[0][0]=a0;c[0][1]=b0;c[0][2]=z;c[1][0]=a1;c[1][1]=b0;c[1][2]=z;c[2][0]=a1;c[2][1]=b1;c[2][2]=z;c[3][0]=a0;c[3][1]=b1;c[3][2]=z;}}
 else{float y=kind==2?-32.0f:32.0f;b0=-16.0f+(float)j*8.0f;b1=b0+8.0f;c[0][0]=a0;c[0][1]=y;c[0][2]=b0;c[1][0]=a1;c[1][1]=y;c[1][2]=b0;c[2][0]=a1;c[2][1]=y;c[2][2]=b1;c[3][0]=a0;c[3][1]=y;c[3][2]=b1;}
 for(k=0;k<4;k++){qs_sv[n][m][0]=c[k][0];qs_sv[n][m][1]=c[k][1];qs_sv[n][m][2]=c[k][2];m++;
  if(k==0&&(n%3)==0){qs_sv[n][m][0]=(c[0][0]+c[1][0])*0.5f;qs_sv[n][m][1]=(c[0][1]+c[1][1])*0.5f;qs_sv[n][m][2]=(c[0][2]+c[1][2])*0.5f;m++;}}
 qs_sn[n]=m;qs_stex[n]=(i/2+j*3+kind)%QS_NTEX;qs_slm[n]=(i/4+kind)%QS_NLM;
 for(k=0;k<m;k++){float u=kind<2?qs_sv[n][k][0]:qs_sv[n][k][0],v=kind<2?qs_sv[n][k][1]:qs_sv[n][k][2];
  qs_st[n][k][0]=u/32.0f;qs_st[n][k][1]=v/32.0f;qs_ls[n][k][0]=(u-4.0f)/128.0f+(float)(j&1)*0.25f;qs_ls[n][k][1]=(v+32.0f)/128.0f;}}
static void qs_build(void) {int n=0,i,j,kd;if(qs_built)return;
 for(kd=0;kd<2;kd++)for(i=0;i<16;i++)for(j=0;j<8;j++)qs_cell(n++,i,j,kd);
 for(kd=2;kd<4;kd++)for(i=0;i<16;i++)for(j=0;j<4;j++)qs_cell(n++,i,j,kd);
 qs_built=1;}
static void qs_q2(int f) {int t,n,k,s,r;
 qs_build();qs_view(f);
 /* the dynamic light: 16 rows of a lightmap replaced (Quake 2's LM_UploadBlock of a block whose surfaces a light touches) */
 qs_pattern(f%17,64,16,1);glBindTexture(GL_TEXTURE_2D,qs_lm[f%QS_NLM]);glTexSubImage2D(GL_TEXTURE_2D,0,0,(f*16)%64,64,16,GL_RGBA,GL_UNSIGNED_BYTE,qs_img);
 for(t=0;t<QS_NTEX;t++){glBindTexture(GL_TEXTURE_2D,qs_tex[t]);
  for(n=0;n<QS_NSURF;n++)if(qs_stex[n]==t){glBegin(GL_POLYGON);for(k=0;k<qs_sn[n];k++){glTexCoord2f(qs_st[n][k][0],qs_st[n][k][1]);glVertex3fv(qs_sv[n][k]);}glEnd();}}
 glEnable(GL_BLEND);glBlendFunc(GL_ZERO,GL_SRC_COLOR);glDepthMask(GL_FALSE);
 for(t=0;t<QS_NLM;t++){glBindTexture(GL_TEXTURE_2D,qs_lm[t]);
  for(n=0;n<QS_NSURF;n++)if(qs_slm[n]==t){glBegin(GL_POLYGON);for(k=0;k<qs_sn[n];k++){glTexCoord2f(qs_ls[n][k][0],qs_ls[n][k][1]);glVertex3fv(qs_sv[n][k]);}glEnd();}}
 glDisable(GL_BLEND);glDepthMask(GL_TRUE);
 /* the model: 12 strips of 20 and 8 fans of 10, a colour a vertex (Quake 2's shadelight), x 40 to 56 in the middle of the room */
 glBindTexture(GL_TEXTURE_2D,qs_skin);
 for(s=0;s<12;s++){glBegin(GL_TRIANGLE_STRIP);
  for(k=0;k<20;k++){float a=(float)k*0.33f,zz=-10.0f+(float)s*1.5f+(float)(k&1)*1.5f;GLfloat v[3];v[0]=48.0f+4.0f*(float)cos(a+f*0.05);v[1]=4.0f*(float)sin(a+f*0.05);v[2]=zz;
   glColor3f(0.5f+0.4f*(float)sin(a),0.6f,0.5f+0.3f*(float)cos(a+s));glTexCoord2f((float)k/19.0f,(float)s/12.0f+(float)(k&1)/12.0f);glVertex3fv(v);}
  glEnd();}
 for(r=0;r<8;r++){glBegin(GL_TRIANGLE_FAN);
  for(k=0;k<10;k++){GLfloat v[3];float a=(float)k*0.7f;v[0]=48.0f+(k?2.5f*(float)cos(a):0.0f)+(float)(r&1)*2.0f;v[1]=(k?2.5f*(float)sin(a):0.0f)-6.0f+(float)r*1.5f;v[2]=9.0f+(float)(r&3);
   glColor3f(0.7f,0.5f+0.05f*(float)k,0.4f);glTexCoord2f(0.5f+0.4f*(float)cos(a),0.5f+0.4f*(float)sin(a));glVertex3fv(v);}
  glEnd();}
 glColor3f(1,1,1);}
/* Quake 3's batches: a 9 x 9 grid of vertices (128 triangles) a batch, 24 batches; batches 12-23 drawn from one array of 12 grids (their
 * indices start at 81 x b) */
#define QS_GV 81
#define QS_GI (8*8*6)
#define QS_NB 24
static GLfloat qs_xyz[12*QS_GV][4],qs_tc[12*QS_GV][2],qs_lc[12*QS_GV][2];static GLubyte qs_rgba[12*QS_GV][4];static GLuint qs_ix[12][QS_GI];static int qs_q3built;
static void qs_q3build(void) {int b,i,j,n;if(qs_q3built)return;
 for(b=0;b<12;b++){float x0=10.0f+(float)(b%6)*20.0f,y0=b<6?-30.0f:2.0f;
  for(j=0;j<9;j++)for(i=0;i<9;i++){n=b*QS_GV+j*9+i;qs_xyz[n][0]=x0+(float)i*2.2f;qs_xyz[n][1]=y0+(float)j*3.3f;qs_xyz[n][2]=-14.0f+2.0f*(float)sin(i*0.6+j*0.4+b);qs_xyz[n][3]=1.0f;
   qs_tc[n][0]=(float)i/4.0f;qs_tc[n][1]=(float)j/4.0f;qs_lc[n][0]=((float)i+0.5f)/9.0f;qs_lc[n][1]=((float)j+0.5f)/9.0f;
   qs_rgba[n][0]=(GLubyte)(128+((i*29+b*7)&127));qs_rgba[n][1]=(GLubyte)(100+((j*41+i)&127));qs_rgba[n][2]=(GLubyte)(150+((i^j)&63));qs_rgba[n][3]=255;}
  n=0;for(j=0;j<8;j++)for(i=0;i<8;i++){GLuint a=(GLuint)(b*QS_GV+j*9+i);qs_ix[b][n++]=a;qs_ix[b][n++]=a+9;qs_ix[b][n++]=a+1;qs_ix[b][n++]=a+1;qs_ix[b][n++]=a+9;qs_ix[b][n++]=a+10;}}
 qs_q3built=1;}
static void qs_q3(int f) {int b;static GLuint lo[QS_GI];
 qs_q3build();qs_view(f);
 glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
 for(b=0;b<QS_NB;b++){int g=b%12,whole=b<12,base=whole?g*QS_GV:0,k;const GLuint *ix=qs_ix[g];GLsizei cnt=whole?QS_GV:12*QS_GV;
  /* batches 0-11: their own array (Quake 3's tess: the indices from 0); 12-23: the large array (indices from 81 x g) */
  if(whole){for(k=0;k<QS_GI;k++)lo[k]=qs_ix[g][k]-(GLuint)base;ix=lo;}
  else for(k=0;k<QS_GV;k++){int i=k%9,j=k/9;qs_xyz[g*QS_GV+k][2]=-14.0f+2.0f*(float)sin(i*0.6+j*0.4+g)+0.25f*(float)((b+f)%3);} /* the large array's grid moved between
   * locks, the same pointer, count and matrix as the batch before (a transform kept from another lock would be wrong) */
  glVertexPointer(3,GL_FLOAT,16,qs_xyz[base]);glColorPointer(4,GL_UNSIGNED_BYTE,0,qs_rgba[base]);glEnableClientState(GL_COLOR_ARRAY);
  glTexCoordPointer(2,GL_FLOAT,0,qs_tc[base]);
  if(qs_lock)qs_lock(0,cnt);
  glBindTexture(GL_TEXTURE_2D,qs_tex[(g+f/8)%QS_NTEX]);glDisable(GL_BLEND);glDepthMask(GL_TRUE);
  glPushMatrix();if(!whole)glTranslatef(0,0,24.0f); /* the second set on the ceiling */
  glDrawElements(GL_TRIANGLES,QS_GI,GL_UNSIGNED_INT,ix);
  if(b==5){glBegin(GL_QUADS);glTexCoord2f(0,0);glVertex3f(60,-4,-6);glTexCoord2f(1,0);glVertex3f(60,4,-6);glTexCoord2f(1,1);glVertex3f(60,4,2);glTexCoord2f(0,1);glVertex3f(60,-4,2);glEnd();} /* a glBegin draw inside the lock */
  glDisableClientState(GL_COLOR_ARRAY);glColor4f(1,1,1,1);glTexCoordPointer(2,GL_FLOAT,0,qs_lc[base]);
  glBindTexture(GL_TEXTURE_2D,qs_lm[g%QS_NLM]);glEnable(GL_BLEND);glBlendFunc(GL_DST_COLOR,GL_ZERO);glDepthMask(GL_FALSE);
  glDrawElements(GL_TRIANGLES,QS_GI,GL_UNSIGNED_INT,ix);
  glPopMatrix();
  if(qs_unlock)qs_unlock();}
 glDisableClientState(GL_VERTEX_ARRAY);glDisableClientState(GL_TEXTURE_COORD_ARRAY);glDisableClientState(GL_COLOR_ARRAY);
 glDisable(GL_BLEND);glDepthMask(GL_TRUE);}
#endif
