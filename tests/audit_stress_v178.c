/* Structured state/geometry and clear stress of actual portable production C.
 * Host only. Sanitizers and exact clear-byte comparisons, no GPU rendering. */
#include "vx_d3d_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t seed=0x178855UL,issued,accepted,fb[0x40000/4];
static uint32_t rng(void){seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
static uint32_t submit(void *ctx,const vx3dn_packet *p) {
 vx3dn_info info;uint32_t rc;(void)ctx;memset(&info,0,sizeof(info));
 info.ready=1;info.generation=7;info.fb_bytes=sizeof(fb);info.primary_bytes=16384;
 info.width=64;info.height=64;info.pitch=256;info.bpp=32;
 issued++;rc=vx3dn_validate(p,&info);if(!rc)accepted++;return rc;
}
static int resolve(void *ctx,uint32_t h,vx3dn_surface *s) {
 (void)ctx;if(h!=5)return 0;
 s->offset=0x20000;s->width=8;s->height=8;s->pitch=32;s->format=VX3DN_TEX_ARGB8888;return 1;
}
static void put32(uint8_t *p,uint32_t v){memcpy(p,&v,4);}
static uint32_t fl(float f){uint32_t v;memcpy(&v,&f,4);return v;}
static uint32_t rd(void *ctx,uint32_t a){(void)ctx;(void)a;return 0;}
static void wr(void *ctx,uint32_t a,uint32_t b){(void)ctx;(void)a;(void)b;}
static uint32_t getfb(void *ctx,uint32_t a){(void)ctx;assert(!(a&3)&&a<sizeof(fb));return fb[a/4];}
static void putfb(void *ctx,uint32_t a,uint32_t b){(void)ctx;assert(!(a&3)&&a<sizeof(fb));fb[a/4]=b;}
static uint32_t now(void *ctx){(void)ctx;return 0;}
int main(void) {
 static vx_d3d_context c;vx3dn_surface target={0,256,64,64,1},depth={0x10000,256,64,64,3};
 vx_d3d_services svc;vx_d3d_input in;uint8_t vertices[5*160],cmd[256];
 uint32_t k,j,texcount,fvf,size,n,off,err,rc,bad,op,last,fmt;
 vx3dn_packet p;vx3dn_io io={0,rd,wr,getfb,putfb,now};
 memset(&svc,0,sizeof(svc));svc.submit=submit;svc.resolve=resolve;
 for(k=0;k<200000;k++) {
  vx_d3d_init(&c,&target,&depth,7);c.state.rs[22]=1;c.state.rs[9]=1+(rng()&1);
  c.depth.format=(k%3)==0?VX3DN_D16:(k%3)==1?VX3DN_D24S8:VX3DN_D32;
  texcount=rng()%9;fvf=4|(rng()&0xc0)|(texcount<<8);
  for(j=0;j<texcount;j++)fvf|=(rng()&3)<<(16+j*2);
  size=vx_d3d_vertex_size(fvf);assert(size&&size<=160);
  memset(vertices,0,sizeof(vertices));
  for(j=0;j<5;j++) {
   uint32_t b=j*size,a;
   put32(vertices+b,fl((float)(int32_t)(rng()%256)-96));
   put32(vertices+b+4,fl((float)(int32_t)(rng()%256)-96));
   put32(vertices+b+8,fl((float)(rng()%257)/128.0f-0.5f));
   put32(vertices+b+12,fl((float)(1+(rng()%32))/8.0f));
   for(a=16;a<size;a+=4)put32(vertices+b+a,(rng()&1)?fl((float)(rng()%1024)/256.0f):rng());
  }
  if(texcount){c.state.tss[0][0]=5;c.state.tss[0][11]=rng()%texcount;}
  c.state.tss[0][16]=1+(rng()&1);c.state.tss[0][17]=1+(rng()&1);
  c.state.tss[0][13]=1+rng()%3;c.state.tss[0][14]=1+rng()%3;
  c.state.rs[27]=rng()&1;c.state.rs[19]=1+rng()%13;c.state.rs[20]=1+rng()%10;
  c.state.rs[15]=rng()&1;c.state.rs[25]=1+rng()%8;c.state.rs[24]=rng()&255;
  memset(cmd,0,sizeof(cmd));off=0;
  cmd[off]=8;cmd[off+2]=1;put32(cmd+off+4,60);put32(cmd+off+8,rng());off+=12;
  op=(uint8_t[]){18,19,21,3,20,22,26}[rng()%7];cmd[off]=op;n=1+rng()%3;cmd[off+2]=(uint8_t)n;off+=4;
  if(op==18||op==19||op==21){cmd[off]=(uint8_t)(rng()%3);off+=2;}
  else {
   if(op!=3){cmd[off]=(uint8_t)(rng()%2);off+=2;}
   if(op==3||op==26){for(j=0;j<n;j++) {
    cmd[off]=(uint8_t)(rng()%6);cmd[off+2]=(uint8_t)(rng()%6);cmd[off+4]=(uint8_t)(rng()%6);off+=op==3?8:6;
   }} else for(j=0;j<n+2;j++){cmd[off]=(uint8_t)(rng()%6);off+=2;}
  }
  /* Truncated tokens, state bits and FVF payloads mixed with valid geometry. */
  bad=rng()%8;
  if(bad==0)off=rng()%(off+1);
  if(bad==1)cmd[rng()%off]^=(uint8_t)(1UL<<(rng()%8));
  if(bad==2)vertices[rng()%(5*size)]^=(uint8_t)(1UL<<(rng()%8));
  memset(&in,0,sizeof(in));in.commands=cmd;in.command_bytes=off;in.vertices=vertices;
  in.vertex_bytes=5*size;in.vertex_count=5;in.vertex_stride=size;in.fvf=fvf;
  rc=vx_d3d_dp2(&c,&in,&svc,&err);(void)rc;assert(err<=off);
 }
 printf("200000 structured DP2/FVF/state inputs: %lu packet attempts, %lu accepted by native validation.\n",(unsigned long)issued,(unsigned long)accepted);
 for(k=0;k<2000;k++) {
  uint32_t x,y,l=rng()%64,t=rng()%64,r=l+rng()%(65-l),b=t+rng()%(65-t),value=rng();
  uint32_t before[64*64],addr,got,want;unsigned char *bytes=(unsigned char *)fb;
  fmt=(k%3)==0?VX3DN_D16:(k%3)==1?VX3DN_D24S8:VX3DN_D32;
  memset(&p,0,sizeof(p));p.cb=sizeof(p);p.abi=VX3DN_ABI;p.arm=VX3DN_ARM;p.generation=7;
  p.op=VX3DN_CLEAR;p.target=target;p.depth=depth;p.depth.format=fmt;
  p.clip.left=l;p.clip.top=t;p.clip.right=r;p.clip.bottom=b;p.z_func=1;p.write_mask=15;p.clear_flags=2;p.clear_depth=value;
  for(j=0;j<sizeof(fb)/4;j++)fb[j]=0x91a73c55UL;
  for(y=0;y<64;y++)for(x=0;x<64;x++) {
   addr=0x10000+y*256+x*(fmt==VX3DN_D16?2:4);got=rng();
   if(fmt==VX3DN_D16){uint16_t s=(uint16_t)got;memcpy(bytes+addr,&s,2);before[y*64+x]=s;}
   else{memcpy(bytes+addr,&got,4);before[y*64+x]=got;}
  }
  assert(submit(0,&p)==0);assert(vx3dn_execute(&p,&io,&last)==0);
  for(y=0;y<64;y++)for(x=0;x<64;x++) {
   addr=0x10000+y*256+x*(fmt==VX3DN_D16?2:4);want=before[y*64+x];
   if(x>=l&&x<r&&y>=t&&y<b)want=fmt==VX3DN_D16?value>>16:fmt==VX3DN_D24S8?(value&0xffffff00)|(want&255):value;
   if(fmt==VX3DN_D16){uint16_t s;memcpy(&s,bytes+addr,2);got=s;}else memcpy(&got,bytes+addr,4);
   assert(got==want);
  }
  for(j=0;j<0x10000/4;j++)assert(fb[j]==0x91a73c55UL);
  for(j=0x14000/4;j<sizeof(fb)/4;j++)assert(fb[j]==0x91a73c55UL);
 }
 puts("2000 exact D16/D24S8/D32 rectangular clears: pixels, stencil and guard regions match.");
 return 0;
}
