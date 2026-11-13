/* Shared pixel checks: Win98 VXFEAT and the bundled-Mesa OSMesa host test.
 * The includer supplies Bounds/query types and reset/quad/pixel/check/say/proc.
 * All drawing uses the existing 256x128 test coordinate system. */
static void bounds_split(Bounds bounds) {
 reset();glDisable(GL_DEPTH_TEST);glClearDepth(.25);glClear(GL_DEPTH_BUFFER_BIT);
 glEnable(GL_SCISSOR_TEST);glScissor(64,0,64,128);glClearDepth(.9);
 glClear(GL_DEPTH_BUFFER_BIT);glDisable(GL_SCISSOR_TEST);
 bounds(.75,1);glEnable(DEPTH_BOUNDS);glColor3f(1,0,0);
}
static void bounds_checks(Bounds bounds) {
 GLfloat z=0;GLdouble range[2];GLubyte st=0;GLuint tex=0,q=0,n=0;
 GLubyte red[16]={255,0,0,255,255,0,0,255,255,0,0,255,255,0,0,255};
 GLubyte bits=255;GLint stencil_bits=0;
 GenObj gen=(GenObj)proc("glGenQueriesARB");
 BeginQ begin=(BeginQ)proc("glBeginQueryARB");
 EndQ end=(EndQ)proc("glEndQueryARB");
 GetQ get=(GetQ)proc("glGetQueryObjectuivARB");
 DelObj del=(DelObj)proc("glDeleteQueriesARB");

 reset();bounds(.75,1);glEnable(DEPTH_BOUNDS);
 glPushAttrib(GL_ENABLE_BIT);glDisable(DEPTH_BOUNDS);glPopAttrib();
 check("bounds enable attribute restore",glIsEnabled(DEPTH_BOUNDS));
 glPushAttrib(GL_DEPTH_BUFFER_BIT);bounds(0,.1);glDisable(DEPTH_BOUNDS);glPopAttrib();
 glGetDoublev(0x8891,range);
 check("bounds depth attribute restore",glIsEnabled(DEPTH_BOUNDS)&&range[0]==.75&&range[1]==1);
 glDisable(DEPTH_BOUNDS);glClearDepth(.25);glClear(GL_DEPTH_BUFFER_BIT);
 glDepthFunc(GL_ALWAYS);glDepthMask(0);bounds(.75,1);glEnable(DEPTH_BOUNDS);
 glColor3f(1,0,0);quad(16,16,112,112,-.8F); /* incoming Z=.9, stored Z=.25 */
 check("depth bounds rejects pixels using stored depth",pixel(64,64,0,0,255));
 glDisable(DEPTH_BOUNDS);glColor3f(0,1,0);quad(16,16,112,112,0);
 check("draw resumes after bounds disabled",pixel(64,64,0,255,0));
 glEnable(DEPTH_BOUNDS);glDepthMask(1);glClearDepth(.9);glClear(GL_DEPTH_BUFFER_BIT);
 glReadPixels(64,64,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&z);
 check("glClear ignores depth bounds",z>.89F&&z<.91F);
 check("glClear preserves bounds enable",glIsEnabled(DEPTH_BOUNDS));

 reset();glDisable(GL_DEPTH_TEST);glClearDepth(.9);glClear(GL_DEPTH_BUFFER_BIT);
 bounds(.75,1);glEnable(DEPTH_BOUNDS);glColor3f(1,0,0);quad(16,16,112,112,.8F);
 check("bounds accepts stored depth despite incoming depth outside range",pixel(64,64,255,0,0));

 bounds_split(bounds);quad(16,16,112,112,0);
 check("bounds with depth test disabled rejects left half",pixel(32,64,0,0,255));
 check("bounds mixed span keeps passing right half",pixel(96,64,255,0,0));

 bounds_split(bounds);glPointSize(1);glBegin(GL_POINTS);
 glVertex2f(32.5F,64.5F);glVertex2f(96.5F,64.5F);glEnd();
 check("bounds point XY span rejects left point",pixel(32,64,0,0,255));
 check("bounds point XY span accepts right point",pixel(96,64,255,0,0));

 bounds_split(bounds);glLineWidth(1);glBegin(GL_LINES);
 glVertex2f(16.5F,64.5F);glVertex2f(112.5F,64.5F);glEnd();
 check("bounds line rejects left portion",pixel(32,64,0,0,255));
 check("bounds line accepts right portion",pixel(96,64,255,0,0));

 reset();glClearDepth(.25);glClearStencil(0x31);glStencilMask(255);
 glClear(GL_DEPTH_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);glDepthFunc(GL_ALWAYS);
 glEnable(GL_STENCIL_TEST);glStencilFunc(GL_ALWAYS,0x72,255);
 glStencilOp(GL_REPLACE,GL_REPLACE,GL_REPLACE);
 bounds(.75,1);glEnable(DEPTH_BOUNDS);glColor3f(1,0,0);quad(16,16,112,112,0);
 glReadPixels(64,64,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&z);
 check("bounds rejected fragment leaves depth unchanged",z>.24F&&z<.26F);
 glGetIntegerv(GL_STENCIL_BITS,&stencil_bits);
 if(stencil_bits){glReadPixels(64,64,1,1,GL_STENCIL_INDEX,GL_UNSIGNED_BYTE,&st);
  check("bounds rejected fragment leaves stencil unchanged",st==0x31);}

 reset();glDisable(GL_DEPTH_TEST);glClearDepth(.25);glClear(GL_DEPTH_BUFFER_BIT);
 glGenTextures(1,&tex);glBindTexture(GL_TEXTURE_2D,tex);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
 glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
 glTexImage2D(GL_TEXTURE_2D,0,GL_RGB,2,2,0,GL_RGBA,GL_UNSIGNED_BYTE,red);
 glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
 glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_FASTEST);glEnable(GL_TEXTURE_2D);
 bounds(.75,1);glEnable(DEPTH_BOUNDS);quad(16,16,112,112,0);
 check("bounds rejects nearest textured triangle fast path",pixel(64,64,0,0,255));
 glDisable(GL_TEXTURE_2D);glDeleteTextures(1,&tex);glHint(GL_PERSPECTIVE_CORRECTION_HINT,GL_DONT_CARE);

 reset();glDisable(GL_DEPTH_TEST);glClearDepth(.25);glClear(GL_DEPTH_BUFFER_BIT);
 bounds(.75,1);glEnable(DEPTH_BOUNDS);glRasterPos2i(48,48);
 glDrawPixels(2,2,GL_RGBA,GL_UNSIGNED_BYTE,red);
 check("bounds rejects DrawPixels fast path",pixel(49,49,0,0,255));

 reset();glDisable(GL_DEPTH_TEST);glClearDepth(.25);glClear(GL_DEPTH_BUFFER_BIT);
 glColor3f(1,0,0);quad(16,16,32,32,0);glRasterPos2i(64,64);
 bounds(.75,1);glEnable(DEPTH_BOUNDS);glCopyPixels(16,16,16,16,GL_COLOR);
 check("bounds rejects CopyPixels fast path",pixel(72,72,0,0,255));

 reset();glDisable(GL_DEPTH_TEST);glClearDepth(.25);glClear(GL_DEPTH_BUFFER_BIT);
 glColor3f(1,0,0);glRasterPos2i(64,64);bounds(.75,1);glEnable(DEPTH_BOUNDS);
 glBitmap(8,1,0,0,0,0,&bits);
 check("bounds rejects bitmap pixels",pixel(68,64,0,0,255));

 if(gen&&begin&&end&&get&&del){
  reset();glClearDepth(.25);glClear(GL_DEPTH_BUFFER_BIT);glDepthMask(0);glDepthFunc(GL_LESS);
  glColorMask(0,0,0,0);bounds(.75,1);glEnable(DEPTH_BOUNDS);gen(1,&q);
  begin(SAMPLES_PASSED,q);quad(16,16,112,112,.8F);end(SAMPLES_PASSED);get(q,QUERY_RESULT,&n);
  say("  samples rejected by bounds: %lu",(unsigned long)n);
  check("bounds query-only triangle rejects samples",n==0);
  bounds(.2,.3);begin(SAMPLES_PASSED,q);quad(16,16,112,112,.8F);end(SAMPLES_PASSED);get(q,QUERY_RESULT,&n);
  check("bounds query-only triangle accepts samples",n>0&&n!=0xFFFFFFFFU);del(1,&q);
 }

 reset();glDisable(GL_DEPTH_TEST);glClearDepth(1);glClear(GL_DEPTH_BUFFER_BIT);
 bounds(0,1);glEnable(DEPTH_BOUNDS);glColor3f(1,0,0);quad(16,16,112,112,0);
 check("bounds full range includes stored far endpoint",pixel(64,64,255,0,0));
 glDisable(DEPTH_BOUNDS);glStencilMask(255);glDepthMask(1);glColorMask(1,1,1,1);
 check("bounds tests GL error",glGetError()==GL_NO_ERROR);
}
