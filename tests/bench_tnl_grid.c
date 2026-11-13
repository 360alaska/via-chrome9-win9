/* Native GRID replay. Arguments are frames, vary inputs, indexed packets, and COLORVERTEX (0 uses constant material). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "vx_d3d_core.h"
extern uint32_t vx_d3d_indexed,vx_d3d_ixpass,vx_d3d_v8,vx_d3d_tn[12];
enum {GN=24,NV=(GN+1)*(GN+1), NT=GN*GN*2, ND=30};
typedef struct {float x,y,z,nx,ny,nz;uint32_t c;float u,v;} vert;
static vert v[NV];
static unsigned char cmd[ND][140+72+124+120+72+6+NT*6];
static uint32_t colorvertex=1;
static uint32_t lengths[ND],hash,packets,validation,invalid;
static vx3dn_info info={sizeof(info),VX3DN_ABI,1,7,64*1024*1024,256*1024,256,256,1024,32};
static void w32(unsigned char **p,uint32_t x){memcpy(*p,&x,4);*p+=4;}
static void w16(unsigned char **p,uint16_t x){memcpy(*p,&x,2);*p+=2;}
static void tok(unsigned char **p,uint32_t op,uint32_t n){w16(p,op);w16(p,n);}
static void words(unsigned char **p,const void *x,size_t n){memcpy(*p,x,n);*p+=n;}
static uint32_t bits(float f){uint32_t x;memcpy(&x,&f,4);return x;}
static uint32_t submit(void *ctx,const vx3dn_packet *p){
 (void)ctx;packets++;
 if(validation){
  const uint32_t b0=p->bursts.b[0];
  if(vx3dn_validate(p,&info))invalid++;
  if(p->pre.on==2){
   const uint16_t *ix=VX3DN_IDX(p);
   const unsigned stride=(b0&VX3DN_IX_V8)?8:10;
   for(unsigned j=0;j<(b0&0xFFFF);j++)
    for(unsigned i=0;i<stride;i++)hash=(hash^p->pre.w[ix[j]*stride+i])*16777619u;
  }else if(p->pre.on==1){
   for(unsigned j=0;j<p->vertex_count*10;j++)
    hash=(hash^p->pre.w[j])*16777619u;
  }else{
   const unsigned char *bytes=(const unsigned char*)p->vertices;
   for(size_t i=0;i<p->vertex_count*sizeof(p->vertices[0]);i++)
    hash=(hash^bytes[i])*16777619u;
  }
 }
 return 0;
}
static int resolve(void *ctx,uint32_t handle,vx3dn_surface *s){
 (void)ctx;
 if(handle!=7)return 0;
 *s=(vx3dn_surface){0x40000,32,8,8,VX3DN_TEX_XRGB8888};return 1;
}
static void init(int vary){
 static const float id[16]={1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1};
 const float pr[16]={1.5f,0,0,0,0,2,0,0,0,0,200.f/199,1,0,0,-200.f/199,0};
 const float mat[17]={1,1,1,1,.3f,.3f,.3f,1};
 float light[26]={0,1,1,1,1,0,0,0,0,.2f,.2f,.2f,1,0,0,0,.3f,-1,.2f,1000};
 const uint32_t light_type=3;
 uint32_t seed=541;
 memcpy(light,&light_type,4);
 for(unsigned j=0;j<=GN;j++)for(unsigned i=0;i<=GN;i++){
  v[j*(GN+1)+i]=(vert){(float)i-GN/2,0,(float)j,.3f,1,.1f,0xff806040,(float)i/GN,(float)j/GN};
  if(vary){v[j*(GN+1)+i].nx+=(float)i*.015f;v[j*(GN+1)+i].c+=j*1024+i;}
 }
 for(unsigned k=0;k<ND;k++){
  unsigned char *p=cmd[k];
  float w[16];
  const uint32_t rs[]={137,1,139,0xff404040,141,colorvertex,145,1,147,0,148,0,22,1,28,1,34,0xff6080a0,35,0,140,3,36,bits(10),37,bits(150)};
  memcpy(w,id,sizeof(w));
  seed=seed*1103515245u+12345u;w[12]=(float)((seed>>8)%600)/10-30;
  seed=seed*1103515245u+12345u;w[13]=-2-(float)((seed>>8)%60)/10;
  seed=seed*1103515245u+12345u;w[14]=(float)((seed>>8)%700)/10-10;
  if(!k){
   tok(&p,36,2);w32(&p,2);words(&p,id,sizeof(id));w32(&p,3);words(&p,pr,sizeof(pr));
   tok(&p,33,1);words(&p,mat,sizeof(mat));
   tok(&p,34,2);w32(&p,0);w32(&p,2);words(&p,light,sizeof(light));w32(&p,0);w32(&p,0);
   tok(&p,8,sizeof(rs)/8);words(&p,rs,sizeof(rs));
  }
  tok(&p,36,1);w32(&p,1);words(&p,w,sizeof(w));
  tok(&p,26,NT);w16(&p,0);
  for(unsigned j=0;j<GN;j++)for(unsigned i=0;i<GN;i++){
   uint16_t a=j*(GN+1)+i;
   w16(&p,a);w16(&p,a+GN+1);w16(&p,a+1);w16(&p,a+1);w16(&p,a+GN+1);w16(&p,a+GN+2);
  }
  lengths[k]=p-cmd[k];
 }
}
static void frame(vx_d3d_context *ctx,const vx_d3d_services *sv){
 for(unsigned k=0;k<ND;k++){
  uint32_t err;
  vx_d3d_input in={cmd[k],(const uint8_t*)v,lengths[k],sizeof(v),NV,sizeof(vert),0x152,0,NULL,0};
  uint32_t rc=vx_d3d_dp2(ctx,&in,sv,&err);
  if(rc){fprintf(stderr,"DP2 %u: %08x at %u\n",k,rc,err);exit(2);}
 }
}
static double now(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec*1e-9;}
int main(int argc,char **argv){
 vx3dn_surface target={0,1024,256,256,VX3DN_XRGB8888};
 static vx_d3d_context ctx;
 vx_d3d_services sv={0,submit,resolve};
 const unsigned reps=argc>1?(unsigned)strtoul(argv[1],NULL,10):500;
 const int vary=argc>2?atoi(argv[2]):0;
 if(!reps)return 2;
 colorvertex=argc>4?(uint32_t)atoi(argv[4]):1;
 init(vary);
 vx_d3d_init(&ctx,&target,NULL,7);
 ctx.state.tss[0][0]=7;ctx.state.tss[0][1]=4;ctx.state.tss[0][2]=2;ctx.state.tss[0][3]=0;
 ctx.state.tss[0][4]=2;ctx.state.tss[0][5]=0;
 ctx.state.tss[1][1]=1;ctx.state.tss[1][4]=1;
 vx_d3d_feat=0xffffffffu&~(1048576|2097152|4194304|8388608);
 vx_d3d_tnl=vx_d3d_indexed=vx_d3d_ixpass=vx_d3d_prepack=vx_d3d_v8=1;
 if(argc>3)vx_d3d_indexed=atoi(argv[3]);
 validation=1;hash=2166136261u;frame(&ctx,&sv);validation=0;
 printf("packets=%u hash=%08x refused=%u T&L=%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u/%u\n",packets,hash,invalid,
 vx_d3d_tn[0],vx_d3d_tn[1],vx_d3d_tn[2],vx_d3d_tn[3],vx_d3d_tn[4],vx_d3d_tn[5],vx_d3d_tn[6],vx_d3d_tn[7],
 vx_d3d_tn[8],vx_d3d_tn[9],vx_d3d_tn[10],vx_d3d_tn[11]);
 for(unsigned k=0;k<20;k++)frame(&ctx,&sv);
 double a=now();
 for(unsigned k=0;k<reps;k++)frame(&ctx,&sv);
 printf("ms/frame %.4f\n",1000*(now()-a)/reps);
 return invalid!=0||packets==0;
}
