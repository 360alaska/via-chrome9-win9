/* Host model of the lab request for testing VX2D8's logic: a 640x480x32 surface, an engine whose depth code map is configurable. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "vx85598.h"
#include "vx855_3d.h"
static uint8_t mem[2560*480];
static uint32_t pre(uint32_t x,uint32_t y) {
 uint32_t gx,gy,v;
 if(x>=4&&x<20&&y>=4&&y<20) {gx=x-4;gy=y-4;v=0xFF203060UL;
  if((gx>=3&&gx<=5&&gy>=3&&gy<=13)||(gy>=3&&gy<=5&&gx>=3&&gx<=12)||(gy>=7&&gy<=9&&gx>=3&&gx<=9)) v=0xFFFFFFFFUL;
  if(gx<2&&gy<2) v=0xFFFF0000UL; if(gx>13&&gy<2) v=0xFF00FF00UL; if(gx<2&&gy>13) v=0xFF0000FFUL; if(gx>13&&gy>13) v=0xFFFFFF00UL; return v;}
 return 0xFF101820UL|(((x*3+y*2)&0x1F)<<16)|(((x+y*3)&0x1F)<<8);
}
static int bug; /* 1: 8bpp destination key compares nothing; 2: 8bpp copies ignore the direction flags; 3: pitch unit wrong at 8bpp */
static int depth_of(uint32_t code) {
 const char *e=getenv("D8CODE"); uint32_t c8=e?strtoul(e,0,16):0;
 if(code==0x300) return 4; if(code==0x100) return 2; if(code==c8) return 1; return 0;
}
static uint32_t gp(uint32_t a,int d) {uint32_t v=0;int k; for(k=d-1;k>=0;k--) v=(v<<8)|mem[a+k]; return v;}
static void pp(uint32_t a,int d,uint32_t v) {int k; for(k=0;k<d;k++) {mem[a+k]=(uint8_t)v; v>>=8;}}
int engine_call(const vx3d_request *q,vx3d_report *r) {
 uint32_t reg[0x80]; uint32_t x,y,i; int d,w,h,cmd,rop,decx,decy; uint32_t pitch,db,sb,dx,dy,sx,sy,mask,cm;
 const char *b=getenv("BUG"); bug=b?atoi(b):0;
 memset(r,0,sizeof(*r)); r->magic=VX3D_MAGIC; r->cb=sizeof(*r); r->version=VX3D_VERSION; r->build=VX3D_BUILD; r->producer=VX3D_PRODUCER;
 memset(mem,0,sizeof(mem));
 for(y=0;y<64;y++) for(x=0;x<64;x++) {uint32_t v=pre(x,y),a=(320+y)*2560+(448+x)*4; memcpy(mem+a,&v,4); memcpy(&r->sf_x_raw[0][0]+y*64+x,&v,4);}
 memset(reg,0,sizeof(reg));
 reg[0x04>>2]=q->lab_mode; reg[0x08>>2]=(320UL<<16)|320; reg[0x0C>>2]=((q->lab_h-1)<<16)|(q->lab_w-1);
 reg[0x4C>>2]=q->lab_fg; reg[0x50>>2]=q->lab_bg; reg[0x58>>2]=q->lab_fg; reg[0x5C>>2]=q->lab_bg;
 for(i=0;i<q->lab_nregs;i++) reg[q->lab_reg[i][0]>>2]=q->lab_reg[i][1];
 d=depth_of(reg[1]);
 if(!d) {r->result=VX3D_PASS; goto out;}
 cmd=(int)q->lab_cmd; rop=(cmd>>24)&0xFF; w=(reg[3]&0xFFFF)+1; h=(reg[3]>>16)+1;
 pitch=((reg[2]>>16)&0x7FF)*8; if(bug==3&&d==1) pitch=pitch*2;
 db=reg[0x14>>2]*8; sb=reg[0x1C>>2]*8; dx=reg[4]&0xFFFF; dy=reg[4]>>16; sx=reg[6]&0xFFFF; sy=reg[6]>>16;
 mask=d==1?0xFF:d==2?0xFFFF:0xFFFFFFFFUL; cm=d==4?0xFFFFFF:mask; decx=!!(cmd&0x8000); decy=!!(cmd&0x4000);
 if(bug==2&&d==1) {decx=0;decy=0;}
 for(int j=0;j<h;j++) for(int k=0;k<w;k++) {
  int xi=decx?w-1-k:k, yi=decy?h-1-j:j; /* positions given are the far corner when decrementing: caller adjusted */
  int px=(int)dx+(decx?-k:k)-0, py=(int)dy+(decy?-j:j); (void)xi;(void)yi;
  uint32_t da=db+py*pitch+px*d,dv,p;
  if(cmd&0x1000) {int cl=reg[0x40>>2]&0xFFFF,ct=reg[0x40>>2]>>16,cr=reg[0x44>>2]&0xFFFF,cb2=reg[0x44>>2]>>16; if(px<cl||px>cr||py<ct||py>cb2) continue;}
  dv=gp(da,d);
  if(rop==0xCC) {
   int spx=(int)sx+(decx?-k:k),spy=(int)sy+(decy?-j:j); uint32_t s=gp(sb+spy*pitch+spx*d,d);
   if((reg[0x48>>2]&0x4000)&&(s&cm)==(reg[0x50>>2]&cm)) continue;
   if((reg[0x48>>2]&0x8000)&&!(bug==1&&d==1)&&(dv&cm)!=(reg[0x4C>>2]&cm)) continue;
   p=s;
  } else {uint32_t f=reg[0x58>>2]&mask; p=rop==0?0:rop==0xFF?mask:rop==0x55?(~dv)&mask:rop==0x5A?(f^dv)&mask:f;}
  pp(da,d,p);
 }
out:
 for(y=0;y<64;y++) for(x=0;x<64;x++) memcpy(&r->image[y*64+x],mem+(320+y)*2560+(448+x)*4,4);
 r->result=VX3D_PASS; return 1;
}
