/* Deterministic ASan/UBSan input stress. No OS, GPU or reference rasterizer. */
#include "vx_d3d_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t seed=0x8552006UL,issued;
static uint32_t rng(void) {seed^=seed<<13;seed^=seed>>17;seed^=seed<<5;return seed;}
/* v10.283 (review M1): the constant groups (vx3dn_cgroups) are only right while no write outside the state set-up's constant runs gives
 * one of their registers another value. Every packet that validates here (the DP2 core's, and packets with random texture, program,
 * stage, stencil, blend, fog, point and line state) is run through the emitter with a recording io: a write of a grouped register with
 * any other value fails the run. */
static uint32_t hdr,executed,checked,grouped_bad;
static int c1(uint32_t r,uint32_t *v) {
 if(r>=3&&r<=15){*v=0;return 1;} if(r==0x15){*v=0xFF;return 1;}
 if(r==0x19||r==0x1A||r==0x1E){*v=0x800000;return 1;} if(r==0x1B){*v=0x803FBF;return 1;} if(r==0x20){*v=0x3F00;return 1;}
 if(r==0x1C||r==0x1D||r==0x1F||r==0x21||r==0x22||r==0x27||(r>=0x2B&&r<=0x32)||(r>=0x38&&r<=0x3F)||r==0x51||r==0x84||r==0x88||r==0x8A){*v=0;return 1;}
 if(r==0x58||r==0x59||r==0x5A||r==0x60||r==0x61||r==0x62||r==0x68||r==0x69||r==0x6A){*v=0;return 1;}
 return 0;
}
static int c4(uint32_t r,uint32_t *v) {
 if((r>=5&&r<=0x10)||r==0x21||r==0x22||r==0x26||r==0x27||r==0x29||(r>=0x2C&&r<=0x2E)||r==0x50||r==0x51){*v=0;return 1;}
 if(r==0x2F){*v=0x582000;return 1;} if(r==0x52){*v=0x147FFF;return 1;}
 return 0;
}
static uint32_t io_read(void *c,uint32_t r) {(void)c;(void)r;return 0;}
static uint32_t io_rfb(void *c,uint32_t a) {(void)c;(void)a;return 0;}
static void io_wfb(void *c,uint32_t a,uint32_t v) {(void)c;(void)a;(void)v;}
static uint32_t io_ms(void *c) {(void)c;return 0;}
static void io_write(void *c,uint32_t reg,uint32_t v) {
 uint32_t want,r=v>>24;(void)c;
 if(reg==0x43C){hdr=v;return;}
 if(hdr==0x00010000UL){checked++;if(c1(r,&want)&&(v&0xFFFFFFUL)!=want){if(grouped_bad++<8)printf("01/%02X written %06lX outside the constant run\n",(unsigned)r,(unsigned long)(v&0xFFFFFFUL));}}
 if(hdr==0x00040000UL){checked++;if(c4(r,&want)&&(v&0xFFFFFFUL)!=want){if(grouped_bad++<8)printf("04/%02X written %06lX outside the constant run\n",(unsigned)r,(unsigned long)(v&0xFFFFFFUL));}}
}
static void run_emitter(const vx3dn_packet *p) {
 vx3dn_io io;uint32_t last=0;
 io.context=0;io.read=io_read;io.write=io_write;io.read_fb=io_rfb;io.write_fb=io_wfb;io.milliseconds=io_ms;
 hdr=0;executed++;(void)vx3dn_execute(p,&io,&last);
}
static vx3dn_packet keep;static int have_keep;
static uint32_t submit(void *unused,const vx3dn_packet *p) {
 vx3dn_info i;uint32_t rc;(void)unused;memset(&i,0,sizeof(i));i.ready=1;i.generation=7;
 i.width=64;i.height=64;i.pitch=256;i.bpp=32;i.primary_bytes=16384;i.fb_bytes=16777216;
 /* Rejecting a driver-generated packet is still safe and turns the
  * context lost; invalid input never reaches actual MMIO in this test. */
 issued++;rc=vx3dn_validate(p,&i);
 if(!rc&&p->op==VX3DN_DRAW){run_emitter(p);if(!have_keep){vx_d3d_packet_copy(&keep,p);have_keep=1;}}
 return rc;
}
int main(void) {
 static vx_d3d_context c;
 uint8_t buffer[256],vertex[160];uint32_t k,j,n,error;vx_d3d_input in;vx_d3d_services svc;
 vx3dn_surface target={0,256,64,64,1};
 memset(&svc,0,sizeof(svc));svc.submit=submit;
 for(k=0;k<100000;k++) {
  vx_d3d_init(&c,&target,0,7);c.state.rs[22]=1;
  for(j=0;j<sizeof(buffer);j++)buffer[j]=(uint8_t)rng();
  for(j=0;j<sizeof(vertex);j++)vertex[j]=(uint8_t)rng();
  n=rng()%sizeof(buffer);
  /* Also exercise recognizable structures, small counts and odd lengths. */
  if(k&1) {static const uint8_t op[]={8,18,19,20,21,22,25,26,28,29,32,39,41,42};buffer[0]=op[rng()%14];buffer[2]=(uint8_t)(rng()%4);buffer[3]=0;}
  if(!(k&7)) {
   static const uint32_t good[]={0x41000000,0x41000000,0x3F000000,0x3F800000,0xFFFF0000,
    0x42600000,0x41000000,0x3F000000,0x3F800000,0xFF00FF00,
    0x41000000,0x42600000,0x3F000000,0x3F800000,0xFF0000FF};
   memcpy(vertex,good,sizeof(good));buffer[0]=18;buffer[2]=1;buffer[3]=0;buffer[4]=buffer[5]=0;n=6;
   if(!(k&15))vertex[rng()%sizeof(good)]=(uint8_t)rng();
  }
  memset(&in,0,sizeof(in));in.commands=buffer;in.command_bytes=n;in.vertices=vertex;
  in.vertex_bytes=sizeof(vertex);in.vertex_count=8;in.vertex_stride=20;in.fvf=0x44;
  (void)vx_d3d_dp2(&c,&in,&svc,&error);assert(error<=n);
 }
 /* packets with random state of every kind the emitter has: those the validation takes go through the emitter */
 if(have_keep) {
  static vx3dn_packet q;vx3dn_info i;uint32_t m,ok=0;
  memset(&i,0,sizeof(i));i.ready=1;i.generation=7;i.width=64;i.height=64;i.pitch=256;i.bpp=32;i.primary_bytes=16384;i.fb_bytes=16777216;
  for(k=0;k<200000;k++) {
   memcpy(&q,&keep,sizeof(q));m=rng();
   if(m&1){q.tex.enable=1;q.tex.surface.offset=0x100000;q.tex.surface.width=q.tex.surface.height=8u<<(rng()%3);q.tex.surface.pitch=q.tex.surface.width*4;q.tex.surface.format=rng()%8;q.tex.filter=rng()%5;q.tex.address_u=rng()%4;q.tex.address_v=rng()%4;}
   if(m&2){q.shader.count=1+rng()%3;for(j=0;j<12;j++)q.shader.words[j]=rng();q.shader.kflags=rng()%4;}
   if(m&4){q.stages.units=rng()%4;q.stages.layout=rng()%3;q.stages.ps=rng()%2;q.stages.ntau=rng()%3;q.stages.c22=rng()%2;q.stages.set[1]=rng()%2;
    q.stages.unit[0].surface=q.tex.surface;q.stages.unit[0].surface.offset=0x120000;q.stages.unit[1].surface=q.stages.unit[0].surface;}
   if(m&8){q.ext.prim=rng()%3;}
   if(m&16){q.ext.stencil=rng()%2;q.depth.offset=0x200000;q.depth.pitch=256;q.depth.width=64;q.depth.height=64;q.depth.format=(rng()&1)?VX3DN_D24S8:VX3DN_D16;q.z_enable=rng()%2;q.z_write=rng()%2;q.z_func=1+rng()%8;}
   if(m&32){q.blend=rng()%2;q.raster.blend_src=rng()%12;q.raster.blend_dst=rng()%12;q.raster.alpha_func=rng()%9;q.raster.alpha_ref=rng()%256;}
   if(m&64){q.fog.enable=rng()%2;for(j=0;j<9;j++)q.fog.r[j]=rng()&0xFFFFFF;}
   if(m&128){q.ext.dither=rng()%2;q.target.format=(rng()&1)?VX3DN_RGB565:VX3DN_XRGB8888;q.mip.levels=1+rng()%3;}
   if(!vx3dn_validate(&q,&i)){run_emitter(&q);ok++;}
  }
  printf("%lu random-state packets validated and run through the emitter\n",(unsigned long)ok);
 }
 printf("constant groups: %lu packets through the emitter, %lu type 1/4 writes checked, %lu grouped registers written with another value\n",(unsigned long)executed,(unsigned long)checked,(unsigned long)grouped_bad);
 if(grouped_bad)return 1;
 printf("100000 deterministic ASan/UBSan DP2 inputs completed; %lu packet attempts, no hardware execution.\n",(unsigned long)issued);
 return 0;
}
