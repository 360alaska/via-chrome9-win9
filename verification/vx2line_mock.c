/* Host model for VX2LINE: a scratch surface with an engine that runs the Bresenham recurrence from its registers (e >= 0 steps the minor axis), and a "DIB engine" memory bitmap whose
 * tie rule is chosen by the environment: DIBMODE=0 tie steps (A) everywhere, 1 no step (B) everywhere, 2 A for X-major lines and B for Y-major lines, 3 A when Y decreases and B when it increases (what the Wyse showed). DIBLAST=1 makes Polyline draw its last
 * point. DIBR2BUG=<r2> makes that R2 code draw as a plain copy. LINEHANG=<rop hex> hangs the engine on that ROP. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "vx85598.h"
#include "vx855_3d.h"
#define NPX 64
static uint8_t mem[4096*4];
static int dbits;
static uint8_t bm[NPX*NPX*4];
static int depth_of(uint32_t c) {return c==0x300?4:c==0x100?2:c==0?1:0;}
static uint32_t gp(const uint8_t *m,uint32_t a,int d) {uint32_t v=0; int k; for(k=d-1;k>=0;k--) v=(v<<8)|m[a+k]; return v;}
static void pp(uint8_t *m,uint32_t a,int d,uint32_t v) {int k; for(k=0;k<d;k++) {m[a+k]=(uint8_t)v; v>>=8;}}
static uint32_t r3(uint32_t rop,uint32_t P,uint32_t S,uint32_t D) {uint32_t r=0; int i; for(i=0;i<8;i++) if((rop>>i)&1) r|=((i&4)?P:~P)&((i&2)?S:~S)&((i&1)?D:~D); return r;}
static int sx14(uint32_t v) {v&=0x3FFF; return (v&0x2000)?(int)v-0x4000:(int)v;}
int engine_call(const vx3d_request *q,vx3d_report *r) {
 uint32_t reg[0x80],i,mask,cmd,rop,x0,y0,n,P; int d,k,e,minor=0,step_neg,step_pos,decx,decy,ym,skip,clip,hr=-1; const char *env;
 env=getenv("LINEHANG"); if(env) sscanf(env,"%x",(unsigned*)&hr);
 memset(r,0,sizeof(*r)); r->magic=VX3D_MAGIC; r->cb=sizeof(*r); r->version=VX3D_VERSION; r->build=VX3D_BUILD; r->producer=VX3D_PRODUCER;
 for(i=0;i<4096;i++) memcpy(mem+i*4,&q->lab_fg,4);
 memset(reg,0,sizeof(reg)); for(i=0;i<q->lab_nregs;i++) reg[q->lab_reg[i][0]>>2]=q->lab_reg[i][1];
 d=depth_of(q->lab_mode); mask=d==1?0xFF:d==2?0xFFFF:0xFFFFFFFFUL; cmd=q->lab_cmd; rop=cmd>>24;
 if((q->lab_flags&~32768UL)!=(128UL|16UL)||(cmd&0xFFF)!=0x005) {r->result=VX3D_BAD_REQUEST; return 1;}
 if(hr==(int)rop) {r->sf_x_info[0][1]=1; r->sf_x_info[0][2]=1; r->result=VX3D_PASS; return 1;}
 decx=(cmd&0x8000)!=0; decy=(cmd&0x4000)!=0; ym=(cmd&0x200000)!=0; skip=(cmd&0x100000)!=0; clip=(cmd&0x1000)!=0;
 x0=reg[0x10>>2]&0xFFFF; y0=reg[0x10>>2]>>16; n=reg[0x0C>>2]&0xFFF; P=reg[0x4C>>2]&mask;
 e=sx14(reg[0x2C>>2]); step_neg=sx14(reg[0x18>>2]&0xFFFF); step_pos=(int)(reg[0x18>>2]>>16);
 for(k=0;k<=(int)n;k++) {
  int x,y;
  if(!(skip&&k==(int)n)) {
   if(ym) {x=(int)x0+(decx?-minor:minor); y=(int)y0+(decy?-k:k);} else {x=(int)x0+(decx?-k:k); y=(int)y0+(decy?-minor:minor);}
   if(x>=0&&x<(int)(256/d)&&y>=0&&y<64) {
    int ok=1;
    if(clip) {int cl=reg[0x40>>2]&0xFFFF,ct=reg[0x40>>2]>>16,cr=reg[0x44>>2]&0xFFFF,cb=reg[0x44>>2]>>16; if(x<cl||x>cr||y<ct||y>cb) ok=0;}
    if(ok) {uint32_t a=(uint32_t)y*256+(uint32_t)x*d; uint32_t D=gp(mem,a,d); pp(mem,a,d,r3(rop,P,0,D)&mask);}
   }
  }
  if(e>=0) {minor++; e+=step_neg;} else e+=step_pos;
 }
 for(i=0;i<4096;i++) memcpy(&r->image[i],mem+i*4,4);
 r->result=VX3D_PASS; return 1;
}
/* the DIB engine */
static int dibmode,diblast,dibbug;
int dib_begin(int bits) {const char *e; dbits=bits; e=getenv("DIBMODE"); dibmode=e?atoi(e):0; e=getenv("DIBLAST"); diblast=e?atoi(e):0; e=getenv("DIBR2BUG"); dibbug=e?atoi(e):0; return 1;}
void dib_end(void) {}
static int dpxm(void) {return dbits/8;}
void dib_fill(uint32_t pixel) {int i,d=dpxm(); for(i=0;i<NPX*NPX;i++) pp(bm,(uint32_t)i*d,d,pixel);}
static const uint8_t r2rop[17]={0,0x00,0x05,0x0A,0x0F,0x50,0x55,0x5A,0x5F,0xA0,0xA5,0xAA,0xAF,0xF0,0xF5,0xFA,0xFF};
static uint32_t penpix(void) {return dpxm()==1?0xFF:dpxm()==2?0xFFFF:0x00FFFFFF;}
static void plot(int x,int y,int r2,const int *clip) {
 int d=dpxm(); uint32_t mask=d==1?0xFF:d==2?0xFFFF:0xFFFFFFFFUL,D,a;
 if(x<0||x>=NPX||y<0||y>=NPX) return;
 if(clip&&(x<clip[0]||x>=clip[2]||y<clip[1]||y>=clip[3])) return;
 a=(uint32_t)(y*NPX+x)*d; D=gp(bm,a,d); pp(bm,a,d,r3(r2==dibbug?0xF0:r2rop[r2],penpix(),0,D)&mask);
}
static void seg(int x0,int y0,int x1,int y1,int r2,const int *clip,int last) {
 int dx=x1-x0,dy=y1-y0,adx=abs(dx),ady=abs(dy),ym=ady>adx,n=ym?ady:adx,m=ym?adx:ady,sx=dx<0?-1:1,sy=dy<0?-1:1,k,e,minor=0,oct=(dx<0?1:0)|(dy<0?2:0)|(ym?4:0);
 int tiestep=dibmode==0?1:dibmode==1?0:dibmode==3?(dy<0):(ym?0:1); (void)oct;
 e=2*m-n-(tiestep?0:1);
 for(k=0;k<(last?n+1:n);k++) {
  int x,y; if(ym) {x=x0+sx*minor; y=y0+sy*k;} else {x=x0+sx*k; y=y0+sy*minor;}
  plot(x,y,r2,clip);
  if(e>=0) {minor++; e+=2*(m-n);} else e+=2*m;
 }
}
void dib_draw(int npts,const int *xy,int r2,const int *clip) {
 int i; for(i=0;i+1<npts;i++) seg(xy[i*2],xy[i*2+1],xy[i*2+2],xy[i*2+3],r2,clip,(i==npts-2)&&diblast&&npts>2);
}
void dib_read(unsigned char *img) {memcpy(img,bm,(size_t)(NPX*NPX*dpxm()));}
