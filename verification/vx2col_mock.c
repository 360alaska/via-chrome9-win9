/* Host model of the lab request for testing VX2COL's logic: a 640x480x32 surface and an engine whose colour pattern layout is configurable.
 * Environment: COLLAY="t,mx,my,px0,py0" layout, COLOFF=1|-1 sign of the offset bits, COLANCHOR=0 screen|1 rectangle, COLBUG=1 stale pattern cache, 2 block ignored at 8bpp,
 * 3 alignment needed (blocks not on 256 bytes unreadable), 4 clip ignored, 5 mono fill broken after a colour fill. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "vx85598.h"
#include "vx855_3d.h"
static uint8_t mem[2560*480];
static uint8_t scratch[4096+2048];
static uint8_t cache[1024]; static int cache_valid;
static int after_colour;
static uint32_t pre(uint32_t x,uint32_t y) {
 uint32_t gx,gy,v;
 if(x>=4&&x<20&&y>=4&&y<20) {gx=x-4;gy=y-4;v=0xFF203060UL;
  if((gx>=3&&gx<=5&&gy>=3&&gy<=13)||(gy>=3&&gy<=5&&gx>=3&&gx<=12)||(gy>=7&&gy<=9&&gx>=3&&gx<=9)) v=0xFFFFFFFFUL;
  if(gx<2&&gy<2) v=0xFFFF0000UL; if(gx>13&&gy<2) v=0xFF00FF00UL; if(gx<2&&gy>13) v=0xFF0000FFUL; if(gx>13&&gy>13) v=0xFFFFFF00UL; return v;}
 return 0xFF101820UL|(((x*3+y*2)&0x1F)<<16)|(((x+y*3)&0x1F)<<8);
}
static int depth_of(uint32_t code) {if(code==0x300) return 4; if(code==0x100) return 2; if(code==0x000) return 1; return 0;}
static uint32_t gp(uint32_t a,int d) {uint32_t v=0;int k; for(k=d-1;k>=0;k--) v=(v<<8)|mem[a+k]; return v;}
static void pp(uint32_t a,int d,uint32_t v) {int k; for(k=0;k<d;k++) {mem[a+k]=(uint8_t)v; v>>=8;}}
int engine_call(const vx3d_request *q,vx3d_report *r) {
 uint32_t reg[0x80]; uint32_t x,y,i; int d,w,h,cmd,rop,t=0,mx=0,my=0,px0=0,py0=0,bug,sgn=1,anch=0; uint32_t pitch,db,dx,dy,mask;
 const char *e;
 e=getenv("COLLAY"); if(e) sscanf(e,"%d,%d,%d,%d,%d",&t,&mx,&my,&px0,&py0);
 e=getenv("COLOFF"); if(e) sgn=atoi(e); e=getenv("COLANCHOR"); if(e) anch=atoi(e); e=getenv("COLBUG"); bug=e?atoi(e):0;
 memset(r,0,sizeof(*r)); r->magic=VX3D_MAGIC; r->cb=sizeof(*r); r->version=VX3D_VERSION; r->build=VX3D_BUILD; r->producer=VX3D_PRODUCER;
 memset(mem,0,sizeof(mem));
 for(y=0;y<64;y++) for(x=0;x<64;x++) {uint32_t v=pre(x,y),a=(320+y)*2560+(448+x)*4; memcpy(mem+a,&v,4); memcpy(&r->sf_x_raw[0][0]+y*64+x,&v,4);}
 if(q->lab_flags&1024) {
  int fresh=!cache_valid||bug!=1;
  memset(scratch,0,sizeof(scratch)); for(i=0;i<q->lab_ndata;i++) memcpy(scratch+i*4,&q->lab_data[i],4);
  if(fresh) {memcpy(cache,scratch,sizeof(cache)); cache_valid=1;}
 }
 memset(reg,0,sizeof(reg));
 reg[0x04>>2]=q->lab_mode; reg[0x08>>2]=(320UL<<16)|320; reg[0x0C>>2]=((q->lab_h-1)<<16)|(q->lab_w-1);
 reg[0x4C>>2]=q->lab_fg; reg[0x58>>2]=q->lab_fg; reg[0x5C>>2]=q->lab_bg;
 for(i=0;i<q->lab_nregs;i++) reg[q->lab_reg[i][0]>>2]=q->lab_reg[i][1];
 d=depth_of(reg[1]);
 cmd=(int)q->lab_cmd; rop=(cmd>>24)&0xFF; w=(reg[3]&0xFFFF)+1; h=(reg[3]>>16)+1;
 pitch=((reg[2]>>16)&0x7FF)*8; db=reg[0x14>>2]*8; dx=reg[4]&0xFFFF; dy=reg[4]>>16; mask=d==1?0xFF:d==2?0xFFFF:0xFFFFFFFFUL;
 if(d&&(cmd&0xA01)==0x001&&!(cmd&0x2000)) { /* colour pattern from video memory */
  uint32_t addr=(reg[0x20>>2]&0x03FFFFFFUL)*8, off=addr-(uint32_t)VX3D_TARGET; int xo=(reg[0x20>>2]>>26)&7, yo=(reg[0x20>>2]>>29)&7;
  for(int j=0;j<h;j++) for(int k=0;k<w;k++) {
   int px=(int)dx+k, py=(int)dy+j; uint32_t da=db+py*pitch+px*d,dv,p; int u,v,s,ax,ay,kk;
   if(cmd&0x1000&&bug!=4) {int cl=reg[0x40>>2]&0xFFFF,ct=reg[0x40>>2]>>16,cr=reg[0x44>>2]&0xFFFF,cb2=reg[0x44>>2]>>16; if(px<cl||px>cr||py<ct||py>cb2) continue;}
   ax=anch?k:px; ay=anch?j:py;
   u=(ax+px0+sgn*xo)&7; v=(ay+py0+sgn*yo)&7; if(mx) u=7-u; if(my) v=7-v; if(t) {s=u;u=v;v=s;}
   kk=v*8+u;
   if(bug==3&&(off&255)) p=0; else if(bug==2&&d==1) p=0x77;
   else {const uint8_t *src=cache+off+kk*d; uint32_t val=0; int b; for(b=d-1;b>=0;b--) val=(val<<8)|src[b]; p=val;}
   dv=gp(da,d); if(rop==0x5A) p=(dv^p)&mask;
   pp(da,d,p);
  }
  after_colour=1;
 } else if(d&&(cmd&0xA00)==0xA00) { /* monochrome pattern, all ones: solid foreground */
  for(int j=0;j<h;j++) for(int k=0;k<w;k++) {int px=(int)dx+k,py=(int)dy+j; uint32_t da=db+py*pitch+px*d; pp(da,d,(bug==5&&after_colour&&k==3)?0:(reg[0x58>>2]&mask));}
 }
 for(y=0;y<64;y++) for(x=0;x<64;x++) memcpy(&r->image[y*64+x],mem+(320+y)*2560+(448+x)*4,4);
 r->result=VX3D_PASS; return 1;
}
