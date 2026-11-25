/* VXDX8 v1.6 (driver .792): VXDX8 TEX: the texture formats alone (board, E791, 3DMark 2001 with the DirectX 8 interface on: some textures
 * wrong at a distance and right close up, some wholly wrong, while v1.5's one 32-bit texture passed). For X8R8G8B8, A8R8G8B8, R5G6B5,
 * A1R5G5B5, A4R4G4B4, DXT1, DXT3 and DXT5 a managed 64x64 texture whose every mip level is one colour of its own, drawn so that levels
 * 0 to 4 are each read; R5G6B5 and DXT1 again at 256x256 (levels 3 to 6) and from the default pool. Run with the interface off (the
 * reference) and on.
 * VXDX8 v1.5 (driver .791B): test 9 had a SKIP in its declaration, which Direct3D 8 allows in no fixed-function declaration (it refused the
 * shader before the driver was asked, with either vertex processing): the declaration is now the colour first, then the position, with
 * no gap, which is what that test is for (a stream whose elements are not in FVF order).
 * VXDX8 v1.4 (driver .790): after the texture test and at the end, the HAL's last events (texture surfaces announced or refused) and its
 * count of primitives left out with the reason.
 * VXDX8 v1.3 (driver .789): the texture loads (TEXBLT, QueryPhases 488-495) at the end of the log.
 * VXDX8 v1.2 (driver .786): the vertex source the driver last saw (QueryPhases 472-487) at the end of the log.
 * VXDX8 v1.1 (driver .784B): a line in the log before every Direct3D 8 call group ("at: ..."), so a run that stops shows the call it stopped in.
 * VXDX8 v1.0: the DirectX 8 driver interface (vx_d3d_dx8.inc), every path of it, with the pixels checked. Run it with the DirectX 8 DDI
 * off and on (VXREG DX8 OFF / VXREG DX8 ON): with it off Direct3D 8 translates everything to DirectX 7 tokens itself, so the same picture
 * both ways is the proof, and the HAL's counters (VX_D3D_QueryPhases words 561-600; .768: the patch's 526-565 were taken by .762-.767) show which of the layer's paths ran.
 *
 * It opens a 256x256 window and draws one coloured square per case into an 8x8 grid, twice: with hardware vertex processing (the
 * driver's T&L: buffers by handle, DRAWPRIMITIVE / DRAWINDEXEDPRIMITIVE) and with software vertex processing (Direct3D 8's own transform:
 * DRAWPRIMITIVE2 / DRAWINDEXEDPRIMITIVE2 byte offsets and CLIPPEDTRIANGLEFAN). Each square's centre is read back from the back buffer.
 *  1 DrawPrimitive from a vertex buffer: list, strip, fan              2 DrawIndexedPrimitive, 16-bit indices, a base vertex
 *  3 32-bit indices                                                    4 32-bit indices over 70004 vertices (gathered a primitive at a time)
 *  5 70000 triangles in one call (drawn in pieces)                     6 points, a line list, a line strip
 *  7 DrawPrimitiveUP, DrawIndexedPrimitiveUP                           8 a declaration over two streams, drawn plain and indexed
 *  9 a declaration with the colour first and a skip                   10 mip filter NONE / POINT / LINEAR (DirectX 8's filter values)
 * 11 MultiplyTransform                                                12 a square that leaves the screen (clipped)
 * No Direct3D 8 headers are needed: the interfaces are called through their vtables (d3d8.h's order). Log: VXDX8.LOG beside the program.
 * Copyright (c) 2026 360ALASKA. MIT; see LICENSE. */
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

typedef unsigned long U32;
#define VT(o,i) (((void**)(*(void***)(o)))[i])
typedef HRESULT (__stdcall *F0)(void*);
typedef HRESULT (__stdcall *F1)(void*,U32);
typedef HRESULT (__stdcall *F2)(void*,U32,U32);
typedef HRESULT (__stdcall *F2P)(void*,U32,void*);
typedef HRESULT (__stdcall *F3)(void*,U32,U32,U32);
typedef HRESULT (__stdcall *F3P)(void*,U32,U32,void*);
enum {D3D_REL=2,D3D_MODE=8,D3D_FMTCHECK=10,D3D_CAPS=13,D3D_CREATEDEV=15};
enum {DEV_PRESENT=15,DEV_BACKBUF=16,DEV_CREATETEX=20,DEV_CREATEVB=23,DEV_CREATEIB=24,DEV_BEGIN=34,DEV_END=35,DEV_CLEAR=36,DEV_SETTRANSFORM=37,
      DEV_MULTRANSFORM=39,DEV_SETRS=50,DEV_SETTEX=61,DEV_SETTSS=63,DEV_DRAW=70,DEV_DRAWIX=71,DEV_DRAWUP=72,DEV_DRAWIXUP=73,DEV_CREATEVS=75,
      DEV_SETVS=76,DEV_DELVS=78,DEV_SETSTREAM=83,DEV_SETINDICES=85};
enum {BUF_LOCK=11,BUF_UNLOCK=12,TEX_LOCK=16,TEX_UNLOCK=17,SURF_LOCK=9,SURF_UNLOCK=10};
typedef struct {U32 w,h,refresh,format;} MODE;
typedef struct {U32 bw,bh,bfmt,bcount,ms,swap;HWND wnd;U32 windowed,autoz,zfmt,flags,rate,interval;} PP;
typedef struct {long pitch;void *bits;} LR;
typedef struct {float x,y,z;U32 c;} V;
typedef struct {float x,y,z;U32 c;float u,v;} VTX;

static FILE *logf;static void *d3d,*dev;static U32 bbfmt,caps[64];static int fails,skipped,cell_n,px,py,dx8_hal,sw;static float cx0,cy0,cx1,cy1;
static void say(const char *f,...) {static char b[2048];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);if(logf){fputs(b,logf);fflush(logf);}}
#define NX(x) ((float)(x)/128.0f-1.0f)
#define NY(y) (1.0f-(float)(y)/128.0f)
#define BG 0x00000040UL
static void rs(U32 s,U32 v) {((F2)VT(dev,DEV_SETRS))(dev,s,v);}
static void tss(U32 st,U32 s,U32 v) {((F3)VT(dev,DEV_SETTSS))(dev,st,s,v);}
static void begin(void) {((F0)VT(dev,DEV_BEGIN))(dev);}
static void end(void) {((F0)VT(dev,DEV_END))(dev);}
static HRESULT draw(U32 t,U32 start,U32 n) {return ((F3)VT(dev,DEV_DRAW))(dev,t,start,n);}
static HRESULT drawix(U32 t,U32 mn,U32 nv,U32 si,U32 n) {return ((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32))VT(dev,DEV_DRAWIX))(dev,t,mn,nv,si,n);}
static void stream(U32 s,void *vb,U32 stride) {((F3)VT(dev,DEV_SETSTREAM))(dev,s,(U32)vb,stride);}
static void indices(void *ib,U32 base) {((F2)VT(dev,DEV_SETINDICES))(dev,(U32)ib,base);}
static void shader(U32 h) {((F1)VT(dev,DEV_SETVS))(dev,h);}
static void rel(void *o) {if(o)((F0)VT(o,D3D_REL))(o);}
/* pool: 0 default (the driver's memory when it allocates buffers), 1 managed, 2 system memory */
static void at(const char *w) {say("  at: %s\n",w);}
static DWORD (WINAPI *qev)(DWORD*,DWORD);static DWORD (WINAPI *qst)(void*);
/* the HAL's event ring (callback, result, reason, three details) and its left-out count */
static void hal_dump(const char *when) {DWORD e[1+16*6],k,n;struct {DWORD cb,version,enabled,creates,dp2,legacy,rs,last_cb,last_hr,last_op,last_off,packets,dropped,drop_why,lost_why,lost_value,lost_count,build,batched,recovered,drop_state[8];} st;
 memset(&st,0,sizeof(st));st.cb=sizeof(st);
 if(qst&&qst(&st))say("  HAL %s: primitives left out %lu (reason bits %08lX; states %08lX %08lX %08lX %08lX), last result %08lX at opcode %lu, context lost %lu (why %lX)\n",when,(U32)st.dropped,(U32)st.drop_why,(U32)st.drop_state[0],(U32)st.drop_state[1],(U32)st.drop_state[2],(U32)st.drop_state[3],(U32)st.last_hr,(U32)st.last_op,(U32)st.lost_count,(U32)st.lost_why);
 n=qev?qev(e,1+16*6):0;if(n<1)return;say("  HAL events (%lu in all; the last %lu: callback result reason a b c):\n",(U32)e[0],(U32)((n-1)/6));
 for(k=0;k+6<=n-1;k+=6)say("    %lu %08lX %lX %08lX %08lX %08lX\n",(U32)e[1+k],(U32)e[2+k],(U32)e[3+k],(U32)e[4+k],(U32)e[5+k],(U32)e[6+k]);}
static void *mkvb(U32 bytes,U32 fvf,U32 pool) {void *vb=0;HRESULT hr;at("CreateVertexBuffer");hr=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,void**))VT(dev,DEV_CREATEVB))(dev,bytes,8 /* WRITEONLY */,fvf,pool,&vb);
 if(hr||!vb){say("  CreateVertexBuffer(%lu bytes, pool %lu): %08lX\n",bytes,pool,(U32)hr);return 0;}return vb;}
static void *mkib(U32 bytes,U32 fmt) {void *ib=0;HRESULT hr;at("CreateIndexBuffer");hr=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,void**))VT(dev,DEV_CREATEIB))(dev,bytes,8,fmt,0,&ib);
 if(hr||!ib){say("  CreateIndexBuffer(%lu bytes, format %lu): %08lX\n",bytes,fmt,(U32)hr);return 0;}return ib;}
static void *lock(void *b) {unsigned char *p=0;at("Lock");if(((HRESULT(__stdcall*)(void*,U32,U32,unsigned char**,U32))VT(b,BUF_LOCK))(b,0,0,&p,0))return 0;return p;}
static void unlock(void *b) {at("Unlock");((F0)VT(b,BUF_UNLOCK))(b);at("unlocked");}
/* the back buffer's pixel as X8R8G8B8 (R5G6B5 widened); 0xFFFFFFFF when the lock fails */
static U32 bb_pixel(int x,int y) {
 void *bb=0;LR lr;U32 v=0xFFFFFFFFUL;
 if(((HRESULT(__stdcall*)(void*,U32,U32,void**))VT(dev,DEV_BACKBUF))(dev,0,0,&bb)||!bb)return v;
 if(!((HRESULT(__stdcall*)(void*,LR*,void*,U32))VT(bb,SURF_LOCK))(bb,&lr,0,0x10)) {
  unsigned char *row=(unsigned char*)lr.bits+y*lr.pitch;
  if(bbfmt==23){unsigned short p=((unsigned short*)row)[x];v=((U32)(p>>11)*255/31)<<16|((U32)((p>>5)&63)*255/63)<<8|((U32)(p&31)*255/31);}
  else v=((U32*)row)[x]&0xFFFFFFUL;
  ((F0)VT(bb,SURF_UNLOCK))(bb);
 }
 rel(bb);return v;
}
static int near_rgb(U32 a,U32 b) {int k;for(k=0;k<24;k+=8){int d=(int)((a>>k)&255)-(int)((b>>k)&255);if(d<-14||d>14)return 0;}return 1;}
/* the next grid cell: its square (inset 4 pixels) and its centre */
static void cell(void) {int c=cell_n++&63,x=(c&7)*32,y=(c>>3)*32;cx0=(float)(x+4);cy0=(float)(y+4);cx1=(float)(x+28);cy1=(float)(y+28);px=x+16;py=y+16;}
static void expect_at(const char *what,int x,int y,U32 want) {U32 got=bb_pixel(x,y);int ok=got!=0xFFFFFFFFUL&&near_rgb(got,want);if(!ok)fails++;say("  %-58s %06lX (want %06lX) %s\n",what,got,want,ok?"ok":"WRONG");}
static void expect(const char *what,U32 want) {expect_at(what,px,py,want);}
/* a thin mark (a point, a line): the colour anywhere in the 3x3 pixels around x, y */
static void expect_near(const char *what,int x,int y,U32 want) {int i,j,ok=0;U32 got=0;for(j=-1;j<=1&&!ok;j++)for(i=-1;i<=1&&!ok;i++){got=bb_pixel(x+i,y+j);if(got!=0xFFFFFFFFUL&&near_rgb(got,want))ok=1;}
 if(!ok)fails++;say("  %-58s %06lX (want %06lX) %s\n",what,got,want,ok?"ok":"WRONG");}
static void skip(const char *what,const char *why) {skipped++;cell_n++;say("  %-58s not run: %s\n",what,why);}
static void vset(V *v,float x,float y,U32 c) {v->x=NX(x);v->y=NY(y);v->z=0.5f;v->c=0xFF000000UL|c;}
/* the cell's square: 6 vertices of a list, or its 4 corners in strip or fan order */
static void q_list(V *v,U32 c) {vset(v,cx0,cy0,c);vset(v+1,cx1,cy0,c);vset(v+2,cx0,cy1,c);vset(v+3,cx1,cy0,c);vset(v+4,cx1,cy1,c);vset(v+5,cx0,cy1,c);}
static void q_strip(V *v,U32 c) {vset(v,cx0,cy0,c);vset(v+1,cx1,cy0,c);vset(v+2,cx0,cy1,c);vset(v+3,cx1,cy1,c);}
static void q_fan(V *v,U32 c) {vset(v,cx0,cy0,c);vset(v+1,cx1,cy0,c);vset(v+2,cx1,cy1,c);vset(v+3,cx0,cy1,c);}
static const float ident[16]={1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
static void setup(void) {
 ((F2P)VT(dev,DEV_SETTRANSFORM))(dev,256,(void*)ident);((F2P)VT(dev,DEV_SETTRANSFORM))(dev,2,(void*)ident);((F2P)VT(dev,DEV_SETTRANSFORM))(dev,3,(void*)ident);
 rs(137,0);rs(22,1);rs(7,0);rs(9,2); /* LIGHTING off, CULLMODE none, ZENABLE off, SHADEMODE gouraud */
 tss(0,1,3);tss(0,3,0);tss(0,4,3);tss(0,6,0); /* COLOROP, ALPHAOP SELECTARG2; the second arguments DIFFUSE: no texture bound */
 ((F2)VT(dev,DEV_SETTEX))(dev,0,0);
}

static void t_plain(void) {
 void *vb=mkvb(14*sizeof(V),0x42,0);V *v;int p[3][2];
 if(!vb){fails++;cell_n+=3;return;}
 v=lock(vb);if(!v){say("  the vertex buffer could not be locked\n");fails++;rel(vb);cell_n+=3;return;}
 cell();p[0][0]=px;p[0][1]=py;q_list(v,0xFF0000);cell();p[1][0]=px;p[1][1]=py;q_strip(v+6,0x00FF00);cell();p[2][0]=px;p[2][1]=py;q_fan(v+10,0xFFFF00);
 unlock(vb);stream(0,vb,sizeof(V));shader(0x42);
 begin();draw(4,0,2);draw(5,6,2);draw(6,10,2);end();
 expect_at("1 DrawPrimitive, triangle list",p[0][0],p[0][1],0xFF0000);expect_at("1 DrawPrimitive, triangle strip from vertex 6",p[1][0],p[1][1],0x00FF00);
 expect_at("1 DrawPrimitive, triangle fan from vertex 10",p[2][0],p[2][1],0xFFFF00);
 expect_at("1 the cell's corner stays background",p[0][0]-15,p[0][1]-15,BG);
 stream(0,0,0);rel(vb);
}
/* three squares as corners in fan order; the indices of a list, a strip and a fan, 16 or 32 bits wide; squares 2 and 3 through the base vertex */
static void t_indexed(int wide) {
 void *vb=mkvb(12*sizeof(V),0x42,0),*ib=mkib(wide?14*4:14*2,wide?102:101);V *v;unsigned char *ix;int p[3][2],k;
 static const unsigned short ord[14]={0,1,2,0,2,3, 0,1,3,2, 0,1,2,3};char name[80];
 if(!vb||!ib){fails++;rel(vb);rel(ib);cell_n+=3;return;}
 v=lock(vb);ix=lock(ib);if(!v||!ix){say("  a buffer could not be locked\n");fails++;rel(vb);rel(ib);cell_n+=3;return;}
 cell();p[0][0]=px;p[0][1]=py;q_fan(v,0xFF00FF);cell();p[1][0]=px;p[1][1]=py;q_fan(v+4,0x00FFFF);cell();p[2][0]=px;p[2][1]=py;q_fan(v+8,0xFF8000);
 for(k=0;k<14;k++){if(wide)((U32*)ix)[k]=ord[k];else ((unsigned short*)ix)[k]=ord[k];}
 unlock(vb);unlock(ib);stream(0,vb,sizeof(V));shader(0x42);
 begin();indices(ib,0);drawix(4,0,4,0,2);indices(ib,4);drawix(5,0,4,6,2);indices(ib,8);drawix(6,0,4,10,2);end();
 sprintf(name,"%d DrawIndexedPrimitive %s, list",wide?3:2,wide?"32-bit":"16-bit");expect_at(name,p[0][0],p[0][1],0xFF00FF);
 sprintf(name,"%d DrawIndexedPrimitive %s, strip, base vertex 4",wide?3:2,wide?"32-bit":"16-bit");expect_at(name,p[1][0],p[1][1],0x00FFFF);
 sprintf(name,"%d DrawIndexedPrimitive %s, fan, base vertex 8",wide?3:2,wide?"32-bit":"16-bit");expect_at(name,p[2][0],p[2][1],0xFF8000);
 indices(0,0);stream(0,0,0);rel(ib);rel(vb);
}
static void t_wide(void) {
 const U32 nv=70004UL;void *vb,*ib;V *v;U32 *ix;int p[2][2];
 static const U32 ord[12]={0,1,2,0,2,3, 70000UL,70001UL,70002UL,70000UL,70002UL,70003UL};
 if(caps[46]<nv){skip("4 32-bit indices over 70004 vertices","MaxVertexIndex is below it");cell_n++;return;}
 vb=mkvb(nv*sizeof(V),0x42,2);ib=mkib(12*4,102);
 if(!vb||!ib){fails++;rel(vb);rel(ib);cell_n+=2;return;}
 v=lock(vb);ix=lock(ib);if(!v||!ix){fails++;rel(vb);rel(ib);cell_n+=2;return;}
 memset(v,0,nv*sizeof(V));cell();p[0][0]=px;p[0][1]=py;q_fan(v,0x80FF00);cell();p[1][0]=px;p[1][1]=py;q_fan(v+70000UL,0x0080FF);memcpy(ix,ord,sizeof(ord));
 unlock(vb);unlock(ib);stream(0,vb,sizeof(V));shader(0x42);indices(ib,0);
 begin();{HRESULT hr=drawix(4,0,nv,0,4);if(hr)say("  DrawIndexedPrimitive: %08lX\n",(U32)hr);}end();
 expect_at("4 32-bit indices over 70004 vertices, vertices 0-3",p[0][0],p[0][1],0x80FF00);expect_at("4 the same call, vertices 70000-70003",p[1][0],p[1][1],0x0080FF);
 indices(0,0);stream(0,0,0);rel(ib);rel(vb);
}
static void t_many(void) {
 const U32 nt=70000UL;void *vb;V *v;int p[2][2];
 if(caps[45]<nt){skip("5 70000 triangles in one call","MaxPrimitiveCount is below it");cell_n++;return;}
 vb=mkvb(nt*3*sizeof(V),0x42,2);if(!vb){fails++;cell_n+=2;return;}
 v=lock(vb);if(!v){fails++;rel(vb);cell_n+=2;return;}
 memset(v,0,nt*3*sizeof(V));cell();p[0][0]=px;p[0][1]=py;q_list(v,0xFFFFFF);cell();p[1][0]=px;p[1][1]=py;q_list(v+(nt-2)*3,0x808080);
 unlock(vb);stream(0,vb,sizeof(V));shader(0x42);
 begin();{HRESULT hr=draw(4,0,nt);if(hr)say("  DrawPrimitive: %08lX\n",(U32)hr);}end();
 expect_at("5 70000 triangles in one call, the first two",p[0][0],p[0][1],0xFFFFFF);expect_at("5 the same call, the last two",p[1][0],p[1][1],0x808080);
 stream(0,0,0);rel(vb);
}
static void t_thin(void) {
 void *vb=mkvb(9*sizeof(V),0x42,0);V *v;int p[3][2];
 if(!vb){fails++;cell_n+=3;return;}
 v=lock(vb);if(!v){fails++;rel(vb);cell_n+=3;return;}
 cell();p[0][0]=px;p[0][1]=py;vset(v,(float)px,(float)py,0xFFFFFF);vset(v+1,(float)(px-6),(float)py,0xFFFFFF);vset(v+2,(float)(px+6),(float)py,0xFFFFFF);
 cell();p[1][0]=px;p[1][1]=py;vset(v+3,cx0,(float)py,0xFF0000);vset(v+4,cx1,(float)py,0xFF0000);
 cell();p[2][0]=px;p[2][1]=py;vset(v+5,cx0,(float)py,0x00FF00);vset(v+6,(float)px,(float)py,0x00FF00);vset(v+7,cx1,(float)py,0x00FF00);
 unlock(vb);stream(0,vb,sizeof(V));shader(0x42);
 begin();draw(1,0,3);draw(2,3,1);draw(3,5,2);end();
 expect_near("6 point list, the middle point",p[0][0],p[0][1],0xFFFFFF);expect_near("6 point list, the third point",p[0][0]+6,p[0][1],0xFFFFFF);
 expect_near("6 line list",p[1][0],p[1][1],0xFF0000);expect_near("6 line strip, first segment",p[2][0]-6,p[2][1],0x00FF00);expect_near("6 line strip, second segment",p[2][0]+6,p[2][1],0x00FF00);
 stream(0,0,0);rel(vb);
}
static void t_up(void) {
 V a[4],b[4];static const unsigned short ix[6]={0,1,2,0,2,3};int p[2][2];HRESULT h1,h2;
 cell();p[0][0]=px;p[0][1]=py;q_strip(a,0xC000C0);cell();p[1][0]=px;p[1][1]=py;q_fan(b,0x00C0C0);shader(0x42);
 begin();h1=((HRESULT(__stdcall*)(void*,U32,U32,const void*,U32))VT(dev,DEV_DRAWUP))(dev,5,2,a,sizeof(V));
 h2=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,const void*,U32,const void*,U32))VT(dev,DEV_DRAWIXUP))(dev,4,0,4,2,ix,101,b,sizeof(V));end();
 if(h1||h2)say("  DrawPrimitiveUP %08lX, DrawIndexedPrimitiveUP %08lX\n",(U32)h1,(U32)h2);
 expect_at("7 DrawPrimitiveUP, strip",p[0][0],p[0][1],0xC000C0);expect_at("7 DrawIndexedPrimitiveUP, list",p[1][0],p[1][1],0x00C0C0);
}
/* positions in stream 0, colours in stream 1 */
static void t_streams(void) {
 static const U32 decl[]={0x20000000UL,0x40020000UL,0x20000001UL,0x40040005UL,0xFFFFFFFFUL};
 static const unsigned short ord[6]={0,1,2,0,2,3};
 void *vp,*vc,*ib;float *f;U32 *c,h=0,k;unsigned short *ix;HRESULT hr;V q[8];int p[2][2];
 if(caps[47]<2){skip("8 a declaration over two streams","MaxStreams is 1 (the DirectX 7 DDI)");cell_n++;return;}
 hr=((HRESULT(__stdcall*)(void*,const U32*,const U32*,U32*,U32))VT(dev,DEV_CREATEVS))(dev,decl,0,&h,sw?0x10:0); /* D3DUSAGE_SOFTWAREPROCESSING on the software device */
 if(hr){say("  CreateVertexShader (two streams): %08lX\n",(U32)hr);fails++;cell_n+=2;return;}
 vp=mkvb(8*12,0,0);vc=mkvb(8*4,0,0);ib=mkib(12,101);
 if(!vp||!vc||!ib){fails++;rel(vp);rel(vc);rel(ib);cell_n+=2;return;}
 f=lock(vp);c=lock(vc);ix=lock(ib);if(!f||!c||!ix){fails++;rel(vp);rel(vc);rel(ib);cell_n+=2;return;}
 cell();p[0][0]=px;p[0][1]=py;q_fan(q,0xFFC000);cell();p[1][0]=px;p[1][1]=py;q_fan(q+4,0x00C0FF);
 for(k=0;k<8;k++){f[k*3]=q[k].x;f[k*3+1]=q[k].y;f[k*3+2]=q[k].z;c[k]=q[k].c;}memcpy(ix,ord,sizeof(ord));
 unlock(vp);unlock(vc);unlock(ib);
 stream(0,vp,12);stream(1,vc,4);shader(h);
 begin();{HRESULT a=draw(6,0,2),b;indices(ib,4);b=drawix(4,0,4,0,2);if(a||b)say("  DrawPrimitive %08lX, DrawIndexedPrimitive %08lX\n",(U32)a,(U32)b);}end();
 expect_at("8 two streams (positions, colours), fan",p[0][0],p[0][1],0xFFC000);expect_at("8 two streams, indexed list, base vertex 4",p[1][0],p[1][1],0x00C0FF);
 shader(0x42);indices(0,0);stream(0,0,0);stream(1,0,0);((F1)VT(dev,DEV_DELVS))(dev,h);rel(ib);rel(vc);rel(vp);
}
/* one stream: the colour, four bytes skipped, then the position */
static void t_order(void) {
 static const U32 decl[]={0x20000000UL,0x40040005UL,0x40020000UL,0xFFFFFFFFUL}; /* stream 0: DIFFUSE as D3DCOLOR, then POSITION as FLOAT3 */
 typedef struct {U32 c;float x,y,z;} W;void *vb;W *w;U32 h=0,k;HRESULT hr;V q[4];
 hr=((HRESULT(__stdcall*)(void*,const U32*,const U32*,U32*,U32))VT(dev,DEV_CREATEVS))(dev,decl,0,&h,sw?0x10:0);
 if(hr){say("  9 CreateVertexShader (colour, then position): %08lX\n",(U32)hr);skip("9 a declaration with the colour first","Direct3D 8 refuses the declaration itself, before the driver is asked (board, E791B: 8876086C on the VX855 and VX900, with hardware and with software vertex processing, with a SKIP and without)");return;} /* next build: not a driver fault, so not counted */
 vb=mkvb(4*sizeof(W),0,0);if(!vb){fails++;cell_n++;return;}
 w=lock(vb);if(!w){fails++;rel(vb);cell_n++;return;}
 cell();q_strip(q,0x8000FF);for(k=0;k<4;k++){w[k].c=q[k].c;w[k].x=q[k].x;w[k].y=q[k].y;w[k].z=q[k].z;}
 unlock(vb);stream(0,vb,sizeof(W));shader(h);
 begin();{HRESULT a=draw(5,0,2);if(a)say("  DrawPrimitive %08lX\n",(U32)a);}end();
 expect("9 one stream: colour, then position",0x8000FF);
 shader(0x42);stream(0,0,0);((F1)VT(dev,DEV_DELVS))(dev,h);rel(vb);
}
/* a 64x64 texture: level 0 red, level 1 green, the smaller ones blue. A 12 pixel square reads level 2, a 28 pixel one level 1, when the
 * mip filter is on; level 0 (red) when it is NONE */
static void t_mip(void) {
 void *tex=0;U32 fmt=22,lv,k;HRESULT hr;VTX q[4];int p[4][2];static const U32 col[7]={0xFF0000,0x00FF00,0x0000FF,0x0000FF,0x0000FF,0x0000FF,0x0000FF};
 static const U32 mode[4]={0,1,2,1};static const float size[4]={12,12,12,28};static const U32 want[4]={0xFF0000,0x0000FF,0x0000FF,0x00FF00};
 static const char *nm[4]={"10 mip filter NONE: level 0","10 mip filter POINT, 12 pixels: level 2","10 mip filter LINEAR, 12 pixels: level 2","10 mip filter POINT, 28 pixels: level 1"};
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32))VT(d3d,D3D_FMTCHECK))(d3d,0,1,bbfmt,0,3,22))fmt=23;
 hal_dump("before the texture test");say("  texture format %lu\n",fmt);
 hr=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,DEV_CREATETEX))(dev,64,64,7,0,fmt,1,&tex);
 if(hr||!tex){say("  CreateTexture 64x64, 7 levels, format %lu: %08lX\n",fmt,(U32)hr);fails++;cell_n+=4;return;}
 for(lv=0;lv<7;lv++){LR lr;U32 s=64>>lv,x,y;
  if(((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(tex,TEX_LOCK))(tex,lv,&lr,0,0)){say("  LockRect level %lu failed\n",lv);fails++;rel(tex);cell_n+=4;return;}
  for(y=0;y<s;y++)for(x=0;x<s;x++){if(fmt==22)((U32*)((char*)lr.bits+y*lr.pitch))[x]=0xFF000000UL|col[lv];
   else ((unsigned short*)((char*)lr.bits+y*lr.pitch))[x]=(unsigned short)(((col[lv]>>19)&31)<<11|((col[lv]>>10)&63)<<5|((col[lv]>>3)&31));}
  ((F1)VT(tex,TEX_UNLOCK))(tex,lv);}
 ((F2)VT(dev,DEV_SETTEX))(dev,0,(U32)tex);tss(0,1,2);tss(0,2,2);tss(0,16,1);tss(0,17,1);shader(0x142); /* COLOROP SELECTARG1 TEXTURE; MAG, MIN POINT */
 for(k=0;k<4;k++){float x0,y0,x1,y1;cell();p[k][0]=px;p[k][1]=py;x0=(float)px-size[k]/2;y0=(float)py-size[k]/2;x1=x0+size[k];y1=y0+size[k];
  q[0].x=NX(x0);q[0].y=NY(y0);q[0].u=0;q[0].v=0;q[1].x=NX(x1);q[1].y=NY(y0);q[1].u=1;q[1].v=0;q[2].x=NX(x0);q[2].y=NY(y1);q[2].u=0;q[2].v=1;q[3].x=NX(x1);q[3].y=NY(y1);q[3].u=1;q[3].v=1;
  {int j;for(j=0;j<4;j++){q[j].z=0.5f;q[j].c=0xFFFFFFFFUL;}}
  tss(0,18,mode[k]);
  begin();((HRESULT(__stdcall*)(void*,U32,U32,const void*,U32))VT(dev,DEV_DRAWUP))(dev,5,2,q,sizeof(VTX));end();
  expect(nm[k],want[k]);}
 hal_dump("after the texture test");
 tss(0,18,0);tss(0,1,3);((F2)VT(dev,DEV_SETTEX))(dev,0,0);shader(0x42);rel(tex);
}
/* v1.6: one texture: every level a colour of its own (lvcol), read at the levels the quad sizes pick */
static int tex_only;
static const U32 lvcol[10]={0xFF0000,0x00FF00,0x0000FF,0xFFFF00,0xFF00FF,0x00FFFF,0xFFFFFF,0x808080,0x808080,0x808080};
static void fill_level(U32 fmt,void *bits,long pitch,U32 w,U32 h,U32 c) {
 const unsigned short c565=(unsigned short)(((c>>19)&31)<<11|((c>>10)&63)<<5|((c>>3)&31));U32 x,y;
 if(fmt==0x31545844UL||fmt==0x33545844UL||fmt==0x35545844UL){const U32 bw=w<4?1:w/4,bh=h<4?1:h/4,bs=fmt==0x31545844UL?8:16;
  for(y=0;y<bh;y++)for(x=0;x<bw;x++){unsigned char *b=(unsigned char*)bits+y*(pitch?(U32)pitch:bw*bs)+x*bs;
   if(bs==16){if(fmt==0x33545844UL)memset(b,0xFF,8);else{b[0]=0xFF;b[1]=0xFF;memset(b+2,0,6);}b+=8;}
   b[0]=(unsigned char)c565;b[1]=(unsigned char)(c565>>8);b[2]=b[0];b[3]=b[1];b[4]=b[5]=b[6]=b[7]=0;}
  return;}
 for(y=0;y<h;y++)for(x=0;x<w;x++){char *r=(char*)bits+y*pitch;
  if(fmt==21||fmt==22)((U32*)r)[x]=0xFF000000UL|c;
  else if(fmt==23)((unsigned short*)r)[x]=c565;
  else if(fmt==25)((unsigned short*)r)[x]=(unsigned short)(0x8000|((c>>19)&31)<<10|((c>>11)&31)<<5|((c>>3)&31));
  else ((unsigned short*)r)[x]=(unsigned short)(0xF000|((c>>20)&15)<<8|((c>>12)&15)<<4|((c>>4)&15));}
}
static void t_one_format(const char *name,U32 fmt,U32 size,U32 pool) {
 void *tex=0;U32 lv,k,levels=0,first;HRESULT hr;VTX q[4];char what[96];static const float qs[4]={28,12,6,3};
 for(lv=size;lv;lv>>=1)levels++;
 first=size==64?1:3; /* the level a 28 pixel quad reads: 64 / 28 -> 1, 256 / 28 -> 3 */
 if(((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32))VT(d3d,D3D_FMTCHECK))(d3d,0,1,bbfmt,0,3,fmt)){sprintf(what,"13 %s %lux%lu pool %lu",name,size,size,pool);skip(what,"Direct3D 8 does not offer the format");cell_n+=4;return;}
 hr=((HRESULT(__stdcall*)(void*,U32,U32,U32,U32,U32,U32,void**))VT(dev,DEV_CREATETEX))(dev,size,size,levels,0,fmt,pool,&tex);
 if(hr||!tex){say("  13 %s %lux%lu pool %lu: CreateTexture %08lX WRONG\n",name,size,size,pool,(U32)hr);fails++;cell_n+=5;return;}
 for(lv=0;lv<levels;lv++){LR lr;U32 d=size>>lv;
  if(((HRESULT(__stdcall*)(void*,U32,LR*,void*,U32))VT(tex,TEX_LOCK))(tex,lv,&lr,0,0)){say("  13 %s: LockRect level %lu failed WRONG\n",name,lv);fails++;rel(tex);cell_n+=5;return;}
  fill_level(fmt,lr.bits,lr.pitch,d,d,lvcol[lv]);
  ((F1)VT(tex,TEX_UNLOCK))(tex,lv);}
 ((F2)VT(dev,DEV_SETTEX))(dev,0,(U32)tex);tss(0,1,2);tss(0,2,2);tss(0,16,1);tss(0,17,1);shader(0x142);
 for(k=0;k<5;k++){const float sz=k?qs[k-1]:28.0f;float x0,y0,x1,y1;int j;const U32 want=k?lvcol[first+k-1]:lvcol[0];
  cell();x0=(float)px-sz/2;y0=(float)py-sz/2;x1=x0+sz;y1=y0+sz;
  q[0].x=NX(x0);q[0].y=NY(y0);q[0].u=0;q[0].v=0;q[1].x=NX(x1);q[1].y=NY(y0);q[1].u=1;q[1].v=0;q[2].x=NX(x0);q[2].y=NY(y1);q[2].u=0;q[2].v=1;q[3].x=NX(x1);q[3].y=NY(y1);q[3].u=1;q[3].v=1;
  for(j=0;j<4;j++){q[j].z=0.5f;q[j].c=0xFFFFFFFFUL;}
  tss(0,18,k?1:0); /* MIPFILTER: NONE for level 0, POINT for the others */
  begin();((HRESULT(__stdcall*)(void*,U32,U32,const void*,U32))VT(dev,DEV_DRAWUP))(dev,5,2,q,sizeof(VTX));end();
  if(k)sprintf(what,"13 %s %lux%lu pool %lu: level %lu",name,size,size,pool,first+k-1);else sprintf(what,"13 %s %lux%lu pool %lu: level 0",name,size,size,pool);
  expect(what,want);}
 tss(0,18,0);tss(0,1,3);((F2)VT(dev,DEV_SETTEX))(dev,0,0);shader(0x42);rel(tex);
}
static void t_formats(void) {
 t_one_format("X8R8G8B8",22,64,1);t_one_format("A8R8G8B8",21,64,1);t_one_format("R5G6B5",23,64,1);t_one_format("A1R5G5B5",25,64,1);t_one_format("A4R4G4B4",26,64,1);
 t_one_format("DXT1",0x31545844UL,64,1);t_one_format("DXT3",0x33545844UL,64,1);t_one_format("DXT5",0x35545844UL,64,1);
 t_one_format("R5G6B5",23,256,1);t_one_format("DXT1",0x31545844UL,256,1);
 t_one_format("R5G6B5",23,64,0);t_one_format("DXT1",0x31545844UL,64,0);
 hal_dump("after the texture formats");
}
static void t_multiply(void) {
 V q[4];float m[16];int p0x,p0y;
 if((cell_n&7)==7)cell_n++;
 cell();p0x=px;p0y=py;q_strip(q,0xFFFF80);cell(); /* the square is the first cell's; the matrix moves it one cell to the right (32 pixels = 0.25) */
 memcpy(m,ident,sizeof(m));m[12]=0.25f;
 ((F2P)VT(dev,DEV_SETTRANSFORM))(dev,256,(void*)ident);((F2P)VT(dev,DEV_MULTRANSFORM))(dev,256,(void*)m);shader(0x42);
 begin();((HRESULT(__stdcall*)(void*,U32,U32,const void*,U32))VT(dev,DEV_DRAWUP))(dev,5,2,q,sizeof(V));end();
 ((F2P)VT(dev,DEV_SETTRANSFORM))(dev,256,(void*)ident);
 expect("11 MultiplyTransform: the square one cell to the right",0xFFFF80);expect_at("11 and not where its vertices are",p0x,p0y,BG);
}
static void t_clip(void) {
 V q[4],l[2];int lx,ly;HRESULT h1,h2;const float past=sw?100000.0f:300.0f; /* the software device: far past any guard band, so Direct3D 8's clipper has to cut it */
 while((cell_n&7)!=7)cell_n++; /* the last column */
 cell();cx1=cx0+past;q_strip(q,0x40FF40);shader(0x42);
 begin();h1=((HRESULT(__stdcall*)(void*,U32,U32,const void*,U32))VT(dev,DEV_DRAWUP))(dev,5,2,q,sizeof(V));end();
 expect("12 a square that leaves the screen on the right",0x40FF40);
 while((cell_n&7)!=7)cell_n++;
 cell();lx=px;ly=py;vset(l,cx0,(float)py,0xFFFFFF);vset(l+1,cx0+past,(float)py,0xFFFFFF);
 begin();h2=((HRESULT(__stdcall*)(void*,U32,U32,const void*,U32))VT(dev,DEV_DRAWUP))(dev,2,1,l,sizeof(V));end();
 if(h1||h2)say("  DrawPrimitiveUP: square %08lX, line %08lX\n",(U32)h1,(U32)h2);
 expect_near("12 a line that leaves the screen on the right",lx,ly,0xFFFFFF);
}

static const char *stat_name[40]={"calls","segments","DRAWPRIMITIVE","DRAWINDEXEDPRIMITIVE","DRAWPRIMITIVE2","DRAWINDEXEDPRIMITIVE2","CLIPPEDTRIANGLEFAN","rebased",
 "32-bit index draws","draws without a drawable shader","pixel shader commands","BUFFERBLT","MULTIPLYTRANSFORM","buffers known","buffer table full","bad buffers",
 "streams above 15","DirectX 7 contexts","commands for the runtime's parser","GetDriverInfo2","caps asked","formats asked","DXVERSION","GetDriverInfo2 not answered",
 "state-block commands","full-buffer flushes","context scans","patches","declarations not drawable","declarations drawn direct","runtime version","failed segments",
 "draws past the stream","gathered draws","gathered vertices","draws a primitive at a time","filter values translated","declarations gathered","filter fix-ups","busy answers taken up"};
static DWORD (WINAPI *qp)(DWORD*,DWORD);static DWORD ph0[620],ph1[620];static int nph;
static int phases(DWORD *o) {return qp&&qp(o,620)==620?620:0;} /* 620 words: a HAL with the layer (.768: word 560 the switch, 561-600 the counters) */

static int run(int software) {
 PP pp;HWND wnd;WNDCLASS wc;HRESULT hr;MSG msg;int f0=fails;
 sw=software;cell_n=0;
 memset(&wc,0,sizeof(wc));wc.lpfnWndProc=DefWindowProc;wc.hInstance=GetModuleHandle(0);wc.lpszClassName="VXDX8";wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);RegisterClass(&wc);
 wnd=CreateWindow("VXDX8",software?"VXDX8: software vertex processing":"VXDX8: hardware vertex processing",WS_OVERLAPPED|WS_CAPTION|WS_VISIBLE,40,40,264,284,0,0,wc.hInstance,0);
 memset(&pp,0,sizeof(pp));pp.bw=256;pp.bh=256;pp.bfmt=bbfmt;pp.bcount=1;pp.swap=3;pp.wnd=wnd;pp.windowed=1;pp.autoz=1;pp.zfmt=80;pp.flags=1;
 dev=0;hr=((HRESULT(__stdcall*)(void*,U32,U32,HWND,U32,PP*,void**))VT(d3d,D3D_CREATEDEV))(d3d,0,1,wnd,software?0x20:0x40,&pp,&dev);
 say("== %s vertex processing: CreateDevice %08lX\n",software?"software":"hardware",(U32)hr);
 if(hr||!dev){fails++;DestroyWindow(wnd);return 0;}
 at("Clear");((HRESULT(__stdcall*)(void*,U32,void*,U32,U32,float,U32))VT(dev,DEV_CLEAR))(dev,0,0,3,BG,1.0f,0);
 at("the first states");setup();
 if(tex_only){at("texture formats");t_formats();}else{
 at("plain draws");t_plain();at("indexed draws");t_indexed(0);if(caps[46]>0xFFFFUL)t_indexed(1);else {skip("3 32-bit indices","MaxVertexIndex is 65535");cell_n+=2;}
 at("wide range");t_wide();at("many");t_many();at("thin");t_thin();at("user-pointer draws");t_up();at("two streams");t_streams();at("order");t_order();at("mip");t_mip();at("multiply");t_multiply();at("clip");t_clip();
 }
 at("Present");((HRESULT(__stdcall*)(void*,void*,void*,HWND,void*))VT(dev,DEV_PRESENT))(dev,0,0,0,0);at("presented");
 while(PeekMessage(&msg,0,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessage(&msg);}
 Sleep(1500);
 rel(dev);dev=0;DestroyWindow(wnd);
 say("   %s vertex processing: %d wrong\n",software?"software":"hardware",fails-f0);
 return 1;
}
int main(void) {
 HMODULE d8;void *(__stdcall *create)(U32);MODE mode;HRESULT hr;char path[MAX_PATH],*s;int k;HANDLE vxd;
 GetModuleFileName(0,path,sizeof(path));s=strrchr(path,'\\');if(s)strcpy(s+1,"VXDX8.LOG");logf=fopen(path,"w");
 tex_only=strstr(GetCommandLineA()," TEX")!=0||strstr(GetCommandLineA()," tex")!=0;
 say("VXDX8 v1.6: the DirectX 8 driver interface%s\n",tex_only?" (TEX: the texture formats alone)":"");
 vxd=CreateFileA("\\\\.\\VX85598",0,0,0,OPEN_EXISTING,0,0);
 if(vxd!=INVALID_HANDLE_VALUE){DWORD in=0x75,o[8],got=0;memset(o,0,sizeof(o));DeviceIoControl(vxd,0x85500022UL,&in,4,o,32,&got,0);say("driver build .%lu, device 1106:%04lX\n",o[1],o[2]&0xFFFF);CloseHandle(vxd);}
 else say("the VIA Chrome9 driver does not answer (another display driver?): the pictures are still checked\n");
 {char dll[MAX_PATH];UINT n=GetSystemDirectoryA(dll,MAX_PATH-16);HMODULE h=0;
  if(n&&n<MAX_PATH-16){strcpy(dll+n,dll[n-1]=='\\'?"VXDDRAW.DLL":"\\VXDDRAW.DLL");h=LoadLibraryA(dll);}
  qp=h?(DWORD (WINAPI *)(DWORD*,DWORD))GetProcAddress(h,"VX_D3D_QueryPhases"):0;
  qev=h?(DWORD (WINAPI *)(DWORD*,DWORD))GetProcAddress(h,"VX_D3D_QueryEvents"):0;qst=h?(DWORD (WINAPI *)(void*))GetProcAddress(h,"VX_D3D_QueryStats"):0;}
 d8=LoadLibrary("d3d8.dll");create=d8?(void*(__stdcall*)(U32))GetProcAddress(d8,"Direct3DCreate8"):0;
 if(!create){say("d3d8.dll / Direct3DCreate8 not found (DirectX 8 or later is needed)\n");return 1;}
 d3d=create(220);if(!d3d)d3d=create(120);
 if(!d3d){say("Direct3DCreate8 failed\n");return 1;}
 nph=phases(ph0);dx8_hal=nph>560&&ph0[560];
 say("the HAL as built now: the DirectX 8 DDI is %s\n",!qp?"not known (VXDDRAW.DLL does not answer)":!nph?"not in this driver (Direct3D 8 translates to DirectX 7 tokens itself)":dx8_hal?"ON":"OFF (Direct3D 8 translates to DirectX 7 tokens itself)");
 memset(&mode,0,sizeof(mode));hr=((F2P)VT(d3d,D3D_MODE))(d3d,0,&mode);bbfmt=mode.format;
 say("display %lux%lu format %lu: %08lX\n",mode.w,mode.h,mode.format,(U32)hr);
 if(bbfmt!=22&&bbfmt!=23){say("the desktop must be 16 or 32 bits a pixel\n");return 1;}
 memset(caps,0,sizeof(caps));hr=((F3P)VT(d3d,D3D_CAPS))(d3d,0,1,caps);
 say("GetDeviceCaps: %08lX  DevCaps %08lX  MaxStreams %lu  MaxStreamStride %lu  MaxVertexIndex %lu  MaxPrimitiveCount %lu\n",(U32)hr,caps[7],caps[47],caps[48],caps[46],caps[45]);
 say("  vertex shader version %lX, pixel shader version %lX; TextureCaps %08lX, TextureFilterCaps %08lX, PresentationIntervals %08lX, PrimitiveMiscCaps %08lX\n",caps[49]&0xFFFF,caps[51]&0xFFFF,caps[15],caps[16],caps[5],caps[8]);
 if(dx8_hal&&caps[47]!=16){say("  WRONG: the DirectX 8 DDI is on, but Direct3D 8 reports MaxStreams %lu (16 with the DirectX 8 caps): it took the driver as a DirectX 7 one\n",caps[47]);fails++;}
 if(caps[7]&0x10000UL)run(0);else say("== no hardware vertex processing offered (the driver's T&L is off): only the software device is run\n");
 run(1);
 ((F0)VT(d3d,D3D_REL))(d3d);
 if(nph&&phases(ph1)==nph&&nph>600){DWORD *a=ph0+561,*b=ph1+561;
  say("== the vertex source the driver last saw (472-487):");for(k=0;k<16;k++)say(" %08lX",(U32)ph1[472+k]);say("\n");
  say("== texture loads (488-495): TEXBLT entries %lu, levels copied %lu, source not known %lu, destination not ours %lu, formats differ %lu, memory or rectangle bad %lu; system-memory textures kept %lu, table full %lu\n",(U32)ph1[488],(U32)ph1[489],(U32)ph1[490],(U32)ph1[491],(U32)ph1[492],(U32)ph1[493],(U32)ph1[494],(U32)ph1[495]);
  if(ph1[487]==2){say("   WRONG: the driver could not read a stream's memory (the segment was refused)\n");fails++;}
  say("== the layer's counters during this run (VX_D3D_QueryPhases 561-600)\n");
  for(k=0;k<40;k++)if(k!=13&&k!=30&&b[k]!=a[k])say("   %-34s %lu\n",stat_name[k],(U32)(b[k]-a[k]));
  if(dx8_hal&&!tex_only){
   #define NEED(i,what) do{int ok=b[i]!=a[i];if(!ok)fails++;say("   %-52s %s\n",what,ok?"ok":"WRONG: did not run");}while(0)
   #define NONE(i,what) do{int ok=b[i]==a[i];if(!ok)fails++;say("   %-52s %s\n",what,ok?"ok":"WRONG");}while(0)
   NEED(0,"DirectX 8 calls reached the layer");NEED(3,"DRAWINDEXEDPRIMITIVE");NEED(33,"gathered draws (the two-stream declaration)");
   NEED(35,"draws a primitive at a time (the wide range)");NEED(36,"filter values translated");
   say("   CLIPPEDTRIANGLEFAN %s, MULTIPLYTRANSFORM %s (either is Direct3D 8's choice: not an error)\n",b[6]!=a[6]?"seen":"not seen",b[12]!=a[12]?"seen":"not seen");
   NONE(31,"no failed segment");NONE(9,"no draw without a drawable shader");NONE(15,"no bad buffer");
   #undef NEED
   #undef NONE
  }
  else if(b[0]!=a[0]){say("   WRONG: the DirectX 8 DDI is off, yet DirectX 8 calls reached the layer\n");fails++;}}
 say("%s: %d wrong, %d not run\n",fails?"FAILED":"PASSED",fails,skipped);
 if(logf)fclose(logf);
 return fails!=0;
}
