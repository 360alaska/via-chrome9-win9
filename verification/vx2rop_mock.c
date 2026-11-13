/* Host model of the lab request for VX2ROP: a 16-row scratch surface and an engine doing exact ROP3. Environment: ROPBUG=<kind>:<rop> makes that ROP wrong
 * (result = destination), ROPHANG=<kind>:<rop> makes the engine hang there. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "vx85598.h"
#include "vx855_3d.h"
static uint8_t mem[4096*4];
static int depth_of(uint32_t c) {return c==0x300?4:c==0x100?2:c==0?1:0;}
static uint32_t gp(uint32_t a,int d) {uint32_t v=0; int k; for(k=d-1;k>=0;k--) v=(v<<8)|mem[a+k]; return v;}
static void pp(uint32_t a,int d,uint32_t v) {int k; for(k=0;k<d;k++) {mem[a+k]=(uint8_t)v; v>>=8;}}
static uint32_t r3(uint32_t rop,uint32_t P,uint32_t S,uint32_t D) {uint32_t r=0; int i; for(i=0;i<8;i++) if((rop>>i)&1) r|=((i&4)?P:~P)&((i&2)?S:~S)&((i&1)?D:~D); return r;}
int engine_call(const vx3d_request *q,vx3d_report *r) {
 uint32_t reg[0x80],i,mask; int d,w,h,j,k,kind; uint32_t cmd,rop,dx,dy,sx,sy,pitch; const char *e; int bk=-1,br=-1,hk=-1,hr=-1;
 e=getenv("ROPBUG"); if(e) sscanf(e,"%d:%x",&bk,&br); e=getenv("ROPHANG"); if(e) sscanf(e,"%d:%x",&hk,&hr);
 memset(r,0,sizeof(*r)); r->magic=VX3D_MAGIC; r->cb=sizeof(*r); r->version=VX3D_VERSION; r->build=VX3D_BUILD; r->producer=VX3D_PRODUCER;
 for(i=0;i<4096;i++) {uint32_t v=0xFF000000UL; memcpy(mem+i*4,&v,4);}
 for(i=0;i<q->lab_ndata;i++) memcpy(mem+i*4,&q->lab_data[i],4);
 memset(reg,0,sizeof(reg)); for(i=0;i<q->lab_nregs;i++) reg[q->lab_reg[i][0]>>2]=q->lab_reg[i][1];
 d=depth_of(q->lab_mode); mask=d==1?0xFF:d==2?0xFFFF:0xFFFFFFFFUL; cmd=q->lab_cmd; rop=cmd>>24;
 w=(int)q->lab_w; h=(int)q->lab_h; dx=reg[4]&0xFFFF; dy=reg[4]>>16; sx=reg[6]&0xFFFF; sy=reg[6]>>16; pitch=((reg[2]&0x7FF)*8);
 if(cmd&0x2000) kind=1; else if((cmd&0xA00)==0xA00) kind=(cmd&0x10000)?5:4; else if(reg[0x20>>2]&0x3FFFFFF) kind=3; else kind=2;
 if(hk==kind&&hr==(int)rop) {r->sf_x_info[0][1]=1; r->sf_x_info[0][2]=1; r->result=VX3D_PASS; return 1;}
 for(j=0;j<h;j++) for(k=0;k<w;k++) {
  uint32_t px=dx+k,py=dy+j,da=py*pitch+px*d,D=gp(da,d),S=0,P=reg[0x58>>2]&mask,res;
  if(kind==1||kind==2) S=gp((sy+j)*pitch+(sx+k)*d,d);
  if(kind==3) {int kk=(py&7)*8+(px&7); uint32_t off=((reg[0x20>>2]&0x3FFFFFF)*8-(uint32_t)VX3D_TARGET); P=gp(off+kk*d,d);}
  if(kind>=4) {uint32_t row=(uint32_t)j&7,bits=((row<4?reg[0x24>>2]:reg[0x28>>2])>>((row&3)*8))&0xFF; int bit=(bits>>(7-(px&7)))&1; if(kind==5&&!bit) continue; P=bit?(reg[0x58>>2]&mask):(reg[0x5C>>2]&mask);}
  res=r3(rop,P,S,D)&mask; if(bk==kind&&br==(int)rop) res=D;
  pp(da,d,res);
 }
 for(i=0;i<4096;i++) memcpy(&r->image[i],mem+i*4,4);
 r->result=VX3D_PASS; return 1;
}
