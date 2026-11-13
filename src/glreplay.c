/* GLREPLAY v1.1 (driver v10.342): plays back a GLTRACE.BIN (src/gltrace.c) through the OpenGL driver, once with the engine and once in
 * software (VXGL_HW per context), and finds where the two pictures part: the first recorded frame that differs, then within that frame
 * the first draw (glEnd) after which they differ, with the calls before it. Win98 EXE (GLREPLAY.EXE, a window the size of the recorded
 * one) and the host model (gl/host/vxgl_host.c with GLREPLAY=<file>). Output: GLREPLAY.TXT next to the EXE (stdout on the host), and the
 * first differing frame from both runs as BMP files. MIT, 2026. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef GR_HOST
#include "GL/gl.h"
#define GT_API
#else
#include <windows.h>
typedef unsigned int GLenum;typedef unsigned char GLboolean;typedef unsigned int GLbitfield;typedef void GLvoid;typedef signed char GLbyte;typedef short GLshort;
typedef int GLint;typedef unsigned char GLubyte;typedef unsigned short GLushort;typedef unsigned int GLuint;typedef int GLsizei;typedef float GLfloat;typedef float GLclampf;
typedef double GLdouble;typedef double GLclampd;
#define GT_API WINAPI
#endif
static unsigned char *body;static unsigned long blen,bpos,bcap;static unsigned char *scratch;
static unsigned long rd_i(void) {unsigned long v=0;if(bpos+4<=blen){memcpy(&v,body+bpos,4);}bpos+=4;return v;}
static float rd_f(void) {float v=0;if(bpos+4<=blen)memcpy(&v,body+bpos,4);bpos+=4;return v;}
static double rd_d(void) {double v=0;if(bpos+8<=blen)memcpy(&v,body+bpos,8);bpos+=8;return v;}
static void *rd_blob(void) {
 unsigned long n=rd_i();void *p;
 if(n==0xFFFFFFFEUL)return 0;
 if(n==0xFFFFFFFFUL||bpos+n>blen)return scratch;
 p=body+bpos;bpos+=n;return p;
}
static void rd_skipgen(void) {}
#define GT_REPLAY
#include "gltrace_gen.h"
#define GT_SWAP (GT_NFUNCS+0)
#define GT_CREATE (GT_NFUNCS+1)
#define GT_CURRENT (GT_NFUNCS+2)
#define GT_DELETE (GT_NFUNCS+3)
#define GT_NOTE (GT_NFUNCS+4)

typedef struct gr_io {
 void *(*gp)(const char*);                 /* GL entry points (core and extensions) */
 int (*begin)(void *u,int engine,unsigned w,unsigned h); /* a context of that size, current; engine on or off */
 void (*swap)(void *u);
 void (*end)(void *u);
 void (*out)(void *u,const char *line);
 void *u;
} gr_io;
#define GR_MAXF 1500
#define GR_MAXD 4096 /* draws checked in one frame */
#define DTW 32
#define DTH 24
#define TW 64
#define TH 48
typedef struct gr_run {
 unsigned nframes,w,h;                     /* recorded frames (Scroll Lock on) */
 float mean[GR_MAXF][3];
 unsigned char thumb[GR_MAXF][TW*TH*3];    /* each frame averaged down to 64x48 */
 unsigned ndraws;unsigned char *dthumb;    /* the checked frame: a thumbnail after each draw */
 unsigned long *dcall;                     /* the record number of each draw's glEnd */
 unsigned char *frame;                     /* the checked frame, whole (RGBA) */
} gr_run;
static void *fp[GT_NFUNCS];
typedef void (GT_API *rp_t)(GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,GLvoid*);typedef void (GT_API *rb_t)(GLenum);typedef void (GT_API *fi_t)(void);
static void thumbn(const unsigned char *px,unsigned w,unsigned h,unsigned char *t,float *mean,unsigned TW_,unsigned TH_);
static void thumb(const unsigned char *px,unsigned w,unsigned h,unsigned char *t,float *mean) {thumbn(px,w,h,t,mean,TW,TH);}
static void thumbn(const unsigned char *px,unsigned w,unsigned h,unsigned char *t,float *mean,unsigned TW_,unsigned TH_) {
 unsigned x,y,i,j;double s[3]={0,0,0};
 for(j=0;j<TH_;j++)for(i=0;i<TW_;i++){unsigned long a[3]={0,0,0},n=0;unsigned x0=i*w/TW_,x1=(i+1)*w/TW_,y0=j*h/TH_,y1=(j+1)*h/TH_;
  for(y=y0;y<y1;y++)for(x=x0;x<x1;x++){const unsigned char *p=px+((h-1-y)*w+x)*4;a[0]+=p[0];a[1]+=p[1];a[2]+=p[2];n++;}
  if(!n)n=1;t[(j*TW_+i)*3]=(unsigned char)(a[0]/n);t[(j*TW_+i)*3+1]=(unsigned char)(a[1]/n);t[(j*TW_+i)*3+2]=(unsigned char)(a[2]/n);s[0]+=a[0]/n;s[1]+=a[1]/n;s[2]+=a[2]/n;}
 if(mean){mean[0]=(float)(s[0]/(TW_*TH_));mean[1]=(float)(s[1]/(TW_*TH_));mean[2]=(float)(s[2]/(TW_*TH_));}
}
static unsigned char *px;static int noread,only_frame=-1; /* only_frame: read back that recorded frame only (-race) */
static void readback(unsigned w,unsigned h) {
 int i=-1,k;for(k=0;k<GT_NFUNCS;k++)if(!strcmp(gt_fns[k].name,"glReadPixels"))i=k;
 {int rb=-1;for(k=0;k<GT_NFUNCS;k++)if(!strcmp(gt_fns[k].name,"glReadBuffer"))rb=k;if(rb>=0&&fp[rb])((rb_t)fp[rb])(0x0405);}
 if(i>=0&&fp[i])((rp_t)fp[i])(0,0,(GLsizei)w,(GLsizei)h,0x1908,0x1401,px);
}
/* one run: every record played; at each recorded swap the back buffer read; in frame check_frame (-1: none) also after each draw */
static int run(const char *path,gr_io *io,int engine,int check_frame,gr_run *r,char *hist,unsigned long *hist_at) {
 FILE *f=fopen(path,"rb");unsigned char h8[8];unsigned long rec=0;int ctx=0,id,k;unsigned w=640,hgt=480,on=0;int fidx=0;int id_end=-1;
 unsigned long hn=0;
 if(!f)return -1;
 if(fread(h8,1,8,f)!=8||memcmp(h8,"GLTRACE1",8)){fclose(f);return -2;}
 for(k=0;k<GT_NFUNCS;k++){fp[k]=io->gp(gt_fns[k].name);if(!strcmp(gt_fns[k].name,"glEnd"))id_end=k;}
 r->nframes=0;r->ndraws=0;
 for(;;){
  unsigned short hd[2];unsigned long n=0; /* (host: a long is 8 bytes; 4 read) */
  if(fread(hd,1,4,f)!=4||fread(&n,1,4,f)!=4)break;
  if(n>bcap){bcap=n+65536;body=(unsigned char*)realloc(body,bcap);if(!body)break;}
  if(fread(body,1,n,f)!=n)break;
  blen=n;bpos=0;id=hd[0];rec++;
  if(id<GT_NFUNCS){
   if(!ctx)continue;
   if(gt_fns[id].cls=='U'||gt_fns[id].cls=='Q')continue;
   gt_call(id,fp);
   if(hist&&on&&fidx==check_frame&&gt_fns[id].cls!='G'){ /* the calls since the last draw, for the report */
    char t[160];int l=sprintf(t,"%s",gt_fns[id].name);unsigned long save=bpos,nv=0;bpos=0;
    while(bpos+4<=blen&&nv<6){unsigned long v=rd_i();l+=sprintf(t+l," %lX",v);nv++;}bpos=save;
    if(hn+(unsigned long)l+2<60000){memcpy(hist+hn,t,(size_t)l);hn+=(unsigned long)l;hist[hn++]='\n';hist[hn]=0;}}
   if(on&&fidx==check_frame&&id==id_end&&r->dthumb&&r->ndraws<GR_MAXD){
    fi_t fin=(fi_t)io->gp("glFinish");if(fin)fin();readback(w,hgt);thumbn(px,w,hgt,r->dthumb+(size_t)r->ndraws*DTW*DTH*3,0,DTW,DTH);r->dcall[r->ndraws]=rec;
    if(hist_at)hist_at[r->ndraws]=hn;r->ndraws++;}
  } else if(id==GT_CREATE||id==GT_CURRENT){
   unsigned long c=rd_i(),cw=rd_i(),ch=rd_i();
   if(!ctx&&cw&&ch&&(id==GT_CREATE||c)){w=(unsigned)cw;hgt=(unsigned)ch;r->w=w;r->h=hgt;if(!io->begin(io->u,engine,w,hgt)){fclose(f);return -3;}ctx=1;
    for(k=0;k<GT_NFUNCS;k++)fp[k]=io->gp(gt_fns[k].name);}
  } else if(id==GT_DELETE){
   if(ctx){io->end(io->u);ctx=0;}
  } else if(id==GT_SWAP){
   unsigned long fr=rd_i(),o=rd_i();(void)fr;
   if(!ctx)continue;
   if(o&&r->nframes<GR_MAXF&&!noread&&(only_frame<0||fidx==only_frame)){readback(w,hgt);thumb(px,w,hgt,r->thumb[r->nframes],r->mean[r->nframes]);
    if((fidx==check_frame||fidx==only_frame)&&r->frame)memcpy(r->frame,px,(size_t)w*hgt*4);r->nframes++;fidx++;}
   else if(o)fidx++;
   io->swap(io->u);
   on=(unsigned)o;
   if(check_frame>=0&&fidx>check_frame)break;
  } else if(id==GT_NOTE){
   if(engine&&check_frame<0){char t[300];sprintf(t,"trace note: %.250s",(char*)body);io->out(io->u,t);}
  }
 }
 if(ctx)io->end(io->u);
 fclose(f);return 0;
}
static int tdiffn(const unsigned char *a,const unsigned char *b,int *worst,int cnt) {int i,n=0,wv=0;for(i=0;i<cnt;i++){int d=a[i]-b[i];if(d<0)d=-d;if(d>wv)wv=d;if(d>24)n++;}if(worst)*worst=wv;return n;}
static int count_frames(const char *path) {FILE *f=fopen(path,"rb");unsigned char h8[8];unsigned short hd[2];unsigned long n,v[2];int k=0;if(!f)return 0;fread(h8,1,8,f);
 while(fread(hd,1,4,f)==4&&((n=0),fread(&n,1,4,f)==4)){if(hd[0]==GT_SWAP&&n>=8){fread(v,1,8,f);if(v[1])k++;fseek(f,(long)n-8,SEEK_CUR);}else fseek(f,(long)n,SEEK_CUR);}fclose(f);return k;}
static int tdiff(const unsigned char *a,const unsigned char *b,int *worst) {int i,n=0,wv=0;for(i=0;i<TW*TH*3;i++){int d=a[i]-b[i];if(d<0)d=-d;if(d>wv)wv=d;if(d>24)n++;}if(worst)*worst=wv;return n;}
static void bmp(const char *name,const unsigned char *p,unsigned w,unsigned h) {
 FILE *f=fopen(name,"wb");unsigned long sz=54+(unsigned long)w*h*3;unsigned char hd[54];unsigned x,y;
 if(!f)return;memset(hd,0,54);hd[0]='B';hd[1]='M';memcpy(hd+2,&sz,4);hd[10]=54;hd[14]=40;memcpy(hd+18,&w,4);memcpy(hd+22,&h,4);hd[26]=1;hd[28]=24;fwrite(hd,1,54,f);
 for(y=0;y<h;y++)for(x=0;x<w;x++){const unsigned char *q=p+(y*w+x)*4;fputc(q[2],f);fputc(q[1],f);fputc(q[0],f);}fclose(f);
}
/* the whole check: both runs, the first differing frame, then the first differing draw in it */
int gr_check(const char *path,gr_io *io,const char *bmpdir) {
 static gr_run a,b;char t[400];unsigned i,n;int first=-1,k;
 scratch=(unsigned char*)calloc(1,16u<<20);px=(unsigned char*)malloc(2048u*2048u*4u);if(!scratch||!px)return 1;
 io->out(io->u,"run 1: engine on");if((k=run(path,io,1,-1,&a,0,0))!=0){sprintf(t,"cannot play %s (%d)",path,k);io->out(io->u,t);return 1;}
 #ifdef GR_PROFILE
 return 0; /* host profiling build (callgrind): the engine run only */
#endif
 io->out(io->u,"run 2: engine off (software)");run(path,io,0,-1,&b,0,0);
 n=a.nframes<b.nframes?a.nframes:b.nframes;
 sprintf(t,"%u recorded frames (%ux%u)",n,a.w,a.h);io->out(io->u,t);
 for(i=0;i<n;i++){int wv,d=tdiff(a.thumb[i],b.thumb[i],&wv);
  if(i<3||d||i+1==n||(i%50)==0){sprintf(t,"frame %4u: engine %5.1f %5.1f %5.1f, software %5.1f %5.1f %5.1f, %4d of %d blocks differ (most %d)",i,a.mean[i][0],a.mean[i][1],a.mean[i][2],b.mean[i][0],b.mean[i][1],b.mean[i][2],d,TW*TH,wv);io->out(io->u,t);}
  if(d>TW*TH/100&&first<0)first=(int)i;}
 if(getenv("GLREPLAY_FRAME"))first=atoi(getenv("GLREPLAY_FRAME")); /* a frame to check draw by draw anyway */
 if(first<0){io->out(io->u,"no frame differs: the engine draws this trace as software does");return 0;}
 sprintf(t,"first differing frame: %d; its draws checked one by one",first);io->out(io->u,t);
 { static char ha[60001],hb[60001];unsigned long *ata=(unsigned long*)calloc(GR_MAXD,4),*atb=(unsigned long*)calloc(GR_MAXD,4);unsigned d;int fd=-1;
  a.dthumb=(unsigned char*)malloc((size_t)GR_MAXD*DTW*DTH*3);b.dthumb=(unsigned char*)malloc((size_t)GR_MAXD*DTW*DTH*3);a.dcall=(unsigned long*)calloc(GR_MAXD,4);b.dcall=(unsigned long*)calloc(GR_MAXD,4);
  a.frame=(unsigned char*)malloc((size_t)a.w*a.h*4);b.frame=(unsigned char*)malloc((size_t)a.w*a.h*4);
  if(!a.dthumb||!b.dthumb||!ata||!atb||!a.frame||!b.frame){io->out(io->u,"out of memory for the draw check");return 1;}
  ha[0]=hb[0]=0;run(path,io,1,first,&a,ha,ata);run(path,io,0,first,&b,hb,atb);
  sprintf(t,"frame %d: %u draws (engine), %u (software)",first,a.ndraws,b.ndraws);io->out(io->u,t);
  for(d=0;d<a.ndraws&&d<b.ndraws;d++){int wv,x=tdiffn(a.dthumb+(size_t)d*DTW*DTH*3,b.dthumb+(size_t)d*DTW*DTH*3,&wv,DTW*DTH*3);if(x>DTW*DTH/100){fd=(int)d;break;}}
  if(fd<0)io->out(io->u,"no single draw makes the difference (it comes at the swap or later)");
  else {unsigned long s=fd?ata[fd-1]:0,e=ata[fd];char line[200];unsigned long p=s;int wv,x=tdiffn(a.dthumb+(size_t)fd*DTW*DTH*3,b.dthumb+(size_t)fd*DTW*DTH*3,&wv,DTW*DTH*3);
   sprintf(t,"first differing draw: %d of the frame (record %lu): %d blocks differ after it (most %d); the calls before it:",fd,a.dcall[fd],x,wv);io->out(io->u,t);
   while(p<e){unsigned long q=p;while(q<e&&ha[q]!='\n')q++;if(q-p<sizeof(line)){memcpy(line,ha+p,q-p);line[q-p]=0;io->out(io->u,line);}p=q+1;}
   if(fd>0){sprintf(t,"(draw %d before it matched)",fd-1);io->out(io->u,t);}}
  if(bmpdir){char nm[400];sprintf(nm,"%sGLR_ENG.BMP",bmpdir);bmp(nm,a.frame,a.w,a.h);sprintf(nm,"%sGLR_SW.BMP",bmpdir);bmp(nm,b.frame,a.w,a.h);io->out(io->u,"frame written: GLR_ENG.BMP (engine), GLR_SW.BMP (software)");}
 }
 return 0;
}

#ifndef GR_HOST
/* ---- the Win98 EXE ---- */
static FILE *lf;static HMODULE gl;static HWND win;static HDC dc;static HGLRC rc;
typedef PROC (WINAPI *gpa_t)(LPCSTR);typedef HGLRC (WINAPI *cc_t)(HDC);typedef BOOL (WINAPI *mc_t)(HDC,HGLRC);typedef BOOL (WINAPI *dc_t)(HGLRC);typedef BOOL (WINAPI *sb_t)(HDC);
static gpa_t wgpa;static cc_t wcc;static mc_t wmc;static dc_t wdc;static sb_t wsb;
static void *w_gp(const char *n) {void *p=(void*)GetProcAddress(gl,n);if(!p&&wgpa&&rc)p=(void*)wgpa(n);return p;}
static LRESULT CALLBACK wp(HWND w,UINT m,WPARAM a,LPARAM b) {return DefWindowProcA(w,m,a,b);}
static void pump(void) {MSG m;while(PeekMessageA(&m,0,0,0,PM_REMOVE)){TranslateMessage(&m);DispatchMessageA(&m);}}
static int w_begin(void *u,int engine,unsigned w,unsigned h) {
 WNDCLASSA wc;RECT r;PIXELFORMATDESCRIPTOR pfd;int pf;(void)u;
 SetEnvironmentVariableA("VXGL_HW",engine?"1":"0");
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=wp;wc.hInstance=GetModuleHandleA(0);wc.lpszClassName="GLREPLAY";wc.hCursor=LoadCursor(0,IDC_ARROW);RegisterClassA(&wc);
 SetRect(&r,0,0,(int)w,(int)h);AdjustWindowRect(&r,WS_OVERLAPPED|WS_CAPTION,FALSE);
 win=CreateWindowExA(0,"GLREPLAY",engine?"GLREPLAY: engine":"GLREPLAY: software",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,20,20,r.right-r.left,r.bottom-r.top,0,0,wc.hInstance,0);
 dc=GetDC(win);memset(&pfd,0,sizeof(pfd));pfd.nSize=sizeof(pfd);pfd.nVersion=1;pfd.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER;pfd.iPixelType=PFD_TYPE_RGBA;pfd.cColorBits=24;pfd.cDepthBits=32;pfd.iLayerType=PFD_MAIN_PLANE;
 pf=ChoosePixelFormat(dc,&pfd);SetPixelFormat(dc,pf,&pfd);rc=wcc(dc);if(!rc||!wmc(dc,rc))return 0;pump();return 1;
}
static void w_swap(void *u) {(void)u;wsb(dc);pump();}
static void w_end(void *u) {(void)u;wmc(0,0);wdc(rc);rc=0;ReleaseDC(win,dc);DestroyWindow(win);pump();SetEnvironmentVariableA("VXGL_HW",0);}
static void w_out(void *u,const char *l) {(void)u;puts(l);if(lf){fputs(l,lf);fputs("\n",lf);fflush(lf);}}
int main(int argc,char **argv) {
 char dir[MAX_PATH],path[MAX_PATH],*e;gr_io io;SYSTEMTIME t;
 GetModuleFileNameA(0,dir,sizeof(dir));e=strrchr(dir,'\\');if(e)e[1]=0;else dir[0]=0;
 if(argc>1)strcpy(path,argv[1]);else sprintf(path,"%sGLTRACE.BIN",dir);
 {char lp[MAX_PATH];sprintf(lp,"%sGLREPLAY.TXT",dir);lf=fopen(lp,"a");}
 GetLocalTime(&t);if(lf)fprintf(lf,"\n==== GLREPLAY v1.1 %04u-%02u-%02u %02u:%02u:%02u: %s ====\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,path);
 gl=LoadLibraryA("opengl32");if(!gl){w_out(0,"opengl32 not loaded");return 1;}
 wgpa=(gpa_t)GetProcAddress(gl,"wglGetProcAddress");wcc=(cc_t)GetProcAddress(gl,"wglCreateContext");wmc=(mc_t)GetProcAddress(gl,"wglMakeCurrent");wdc=(dc_t)GetProcAddress(gl,"wglDeleteContext");wsb=(sb_t)GetProcAddress(gl,"wglSwapBuffers");
 io.gp=w_gp;io.begin=w_begin;io.swap=w_swap;io.end=w_end;io.out=w_out;io.u=0;
 if(argc>1&&!strcmp(argv[1],"-race")){static gr_run ra,rb;int last,wv,d;char t[300];scratch=(unsigned char*)calloc(1,16u<<20);px=(unsigned char*)malloc(2048u*2048u*4u);
  sprintf(path,"%sGLTRACE.BIN",dir);if(argc>2)strcpy(path,argv[2]);last=count_frames(path)-1;only_frame=last;
  ra.frame=(unsigned char*)malloc(2048u*2048u*4u);rb.frame=(unsigned char*)malloc(2048u*2048u*4u);
  sprintf(t,"-race: %d recorded frames, only the last read back (the engine is not made to finish between frames)",last+1);w_out(0,t);
  run(path,&io,1,-1,&ra,0,0);run(path,&io,0,-1,&rb,0,0);
  d=tdiff(ra.thumb[0],rb.thumb[0],&wv);
  sprintf(t,"last frame: engine %5.1f %5.1f %5.1f, software %5.1f %5.1f %5.1f, %d of %d blocks differ (most %d)",ra.mean[0][0],ra.mean[0][1],ra.mean[0][2],rb.mean[0][0],rb.mean[0][1],rb.mean[0][2],d,TW*TH,wv);w_out(0,t);
  {char nm[400];sprintf(nm,"%sGLR_ENG.BMP",dir);bmp(nm,ra.frame,ra.w,ra.h);sprintf(nm,"%sGLR_SW.BMP",dir);bmp(nm,rb.frame,ra.w,ra.h);w_out(0,"written: GLR_ENG.BMP (engine), GLR_SW.BMP (software)");}}
 else if(argc>1&&!strcmp(argv[1],"-play")){static gr_run pr;noread=1;scratch=(unsigned char*)calloc(1,16u<<20);px=(unsigned char*)malloc(2048u*2048u*4u);sprintf(path,"%sGLTRACE.BIN",dir);if(argc>2)strcpy(path,argv[2]);w_out(0,"-play: the engine run only");run(path,&io,1,-1,&pr,0,0);}
 else gr_check(path,&io,dir);
 if(lf)fclose(lf);
 MessageBoxA(0,"GLREPLAY is done: GLREPLAY.TXT is next to the program.","GLREPLAY",MB_OK|MB_TOPMOST);
 return 0;
}
#endif
