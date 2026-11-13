/* test_tnl_asm.c (.544): the board's T&L group kernels (tests/tnl_kernels.s, the same bytes as src/vx_tnl_sse.asm) against the host's
 * intrinsics (the kernel part of src/vx_d3d_tnl_soa.inc), word for word, on random frames: positions near and across every clip plane,
 * non-finite inputs, lit with 0-8 directional lights, normalized or not, fog off / linear / exp / exp2 / range, unlit.
 * 32-bit, freestanding (no libc): tests/run.py builds it with clang -m32 and runs it; exit 0 when every word matches.
 * Built: python3 tools/tnl_asm.py; then
 *   sed -n '/TNK KERNELS BEGIN/,/TNK KERNELS END/p' src/vx_d3d_tnl_soa.inc > tests/tnl_kernels_c.inc
 *   clang -m32 --target=i686-linux-gnu -O2 -msse2 -nostdlib -static -ffreestanding -fno-stack-protector -fno-pic -no-pie \
 *     tests/test_tnl_asm.c tests/tnl_kernels.s -o tests/test_tnl_asm && tests/test_tnl_asm */
typedef unsigned int uint32_t;typedef unsigned char uint8_t;typedef unsigned short uint16_t;typedef unsigned int size_t;
#define offsetof(T,f) ((size_t)&((T*)0)->f)
static void *memset(void *d,int c,size_t n){uint8_t *p=d;while(n--)*p++=(uint8_t)c;return d;}
void *memcpy(void *d,const void *s,size_t n){uint8_t *p=d;const uint8_t *q=s;while(n--)*p++=*q++;return d;}
#include "tnl_kernels_c.inc"
/* the board's */
extern void tnk_pos_asm(tnk_frame *f) __asm__("_tnk_pos");
extern void tnk_light_asm(tnk_frame *f) __asm__("_tnk_light");

static uint32_t seed=12345;
static uint32_t rnd(void){seed=seed*1664525u+1013904223u;return seed;}
static float frnd(float lo,float hi){return lo+(hi-lo)*(float)(rnd()>>8)/16777216.0f;}
static float fbits(uint32_t n){union{float f;uint32_t n;}u;u.n=n;return u.f;}
static void splat(float *o,float v){o[0]=o[1]=o[2]=o[3]=v;}
static long sys_write(int fd,const char *s,unsigned n){long r;__asm__ volatile("int $0x80":"=a"(r):"a"(4),"b"(fd),"c"(s),"d"(n));return r;}
static void out(const char *s){unsigned n=0;while(s[n])n++;sys_write(1,s,n);}
static void outn(unsigned v){char b[12];int i=0;if(!v)b[i++]='0';while(v){b[i++]=(char)('0'+v%10);v/=10;}while(i){char c[2];c[0]=b[--i];c[1]=0;out(c);}}

#define N 64
static tnk_frame fa __attribute__((aligned(16))),fb __attribute__((aligned(16)));
static float sx[N+8],sy[N+8],sz[N+8],cx[2][N+8],cy[2][N+8],cz[2][N+8],cw[2][N+8],rw[2][N+8];static uint8_t cc[2][N+8],rec[2][(N+8)*40];
static void consts(tnk_frame *f){
 static const float expc[7]={1.0f,0.6931471805599453f,0.2402265069591007f,0.0555041086648216f,0.0096181291076285f,0.0013333558146428f,0.0001540353039338f};uint32_t k;
 splat(f->kone,1.0f);splat(f->k255,255.0f);splat(f->khalf,0.5f);splat(f->kl2e,1.4426950408889634f);splat(f->kneg80,-80.0f);
 for(k=0;k<7;k++)splat(f->kexpc[k],expc[k]);splat(f->kinf,fbits(0x7F800000u));splat(f->ksign,fbits(0x80000000u));
 for(k=0;k<4;k++){f->k127[k]=127;f->kmask24[k]=0x00FFFFFFu;f->k7f[k]=0x7F;f->kb[0][k]=1;f->kb[1][k]=2;f->kb[2][k]=8;f->kb[3][k]=16;f->kb[4][k]=32;f->kb[5][k]=64;}
}
static int cmp(const void *a,const void *b,unsigned n){const uint8_t *p=a,*q=b;unsigned i;for(i=0;i<n;i++)if(p[i]!=q[i])return 1;return 0;}
__attribute__((force_align_arg_pointer)) void _start(void){
 unsigned t,bad=0,cases=0;
 for(t=0;t<400;t++){
  uint32_t k,i,stride=(t&1)?40:32;
  memset(&fa,0,sizeof(fa));consts(&fa);
  /* a perspective-like matrix with noise, in splats */
  {float m[16];for(k=0;k<16;k++)m[k]=frnd(-1.0f,1.0f);m[11]=frnd(0.5f,2.0f);m[15]=frnd(-2.0f,2.0f);for(k=0;k<16;k++)splat(fa.m[k],m[k]);
   for(k=0;k<16;k++)splat(fa.e[k],frnd(-2.0f,2.0f));for(k=0;k<9;k++)splat(fa.nm[k],frnd(-1.5f,1.5f));}
  splat(fa.vp[0],320.0f);splat(fa.vp[1],320.0f);splat(fa.vp[2],240.0f);splat(fa.vp[3],-240.0f);splat(fa.vp[4],frnd(0,0.5f));splat(fa.vp[5],frnd(0.2f,1.0f));splat(fa.vp[6],0.0f);
  fa.lit=(t%3)!=0;fa.haveN=(t%5)!=0;fa.nnorm=(t>>1)&1;fa.nl=t%9;fa.fogmode=t%4;fa.rfog=(t>>2)&1;
  for(k=0;k<8;k++){for(i=0;i<3;i++){splat(fa.ld[k][i],frnd(-1,1));splat(fa.lc[k][i],frnd(0,1.2f));}splat(fa.lc[k][3],0);}
  for(k=0;k<4;k++)splat(fa.base[k],frnd(-0.2f,1.3f));
  splat(fa.fog[0],frnd(0.001f,0.1f));splat(fa.fog[1],frnd(20,200));splat(fa.fog[2],frnd(0.002f,0.1f));
  fa.stride=stride;fa.first=(t*4)%16;fa.n=((N-fa.first)/4)*4;
  fa.sx=sx;fa.sy=sy;fa.sz=sz;fa.cc=cc[0];fa.rec=rec[0];fa.cx=cx[0];fa.cy=cy[0];fa.cz=cz[0];fa.cw=cw[0];fa.rw=rw[0];
  for(i=0;i<N+8;i++){sx[i]=frnd(-50,50);sy[i]=frnd(-50,50);sz[i]=frnd(-50,50);
   if((rnd()&31)==0)sx[i]=fbits(0x7FC00000u);if((rnd()&31)==1)sy[i]=fbits(0x7F800000u);if((rnd()&63)==2)sz[i]=fbits(0x00000001u);}
  memset(cc,0,sizeof(cc));memset(rec,0,sizeof(rec));
  fb=fa;fb.cc=cc[1];fb.rec=rec[1];fb.cx=cx[1];fb.cy=cy[1];fb.cz=cz[1];fb.cw=cw[1];fb.rw=rw[1];
  tnk_pos(&fa);tnk_pos_asm(&fb);cases++;
  if(cmp(cx[0],cx[1],sizeof(cx[0]))||cmp(cy[0],cy[1],sizeof(cy[0]))||cmp(cz[0],cz[1],sizeof(cz[0]))||cmp(cw[0],cw[1],sizeof(cw[0]))||cmp(rw[0],rw[1],sizeof(rw[0]))){out("pos: clip arrays differ, case ");outn(t);out("\n");bad++;}
  if(cmp(cc[0],cc[1],sizeof(cc[0]))){out("pos: codes differ, case ");outn(t);out("\n");bad++;}
  if(cmp(rec[0],rec[1],sizeof(rec[0]))){out("pos: records differ, case ");outn(t);out("\n");bad++;}
  /* phase L on four lanes */
  for(k=0;k<4;k++){fa.gx[k]=frnd(-50,50);fa.gy[k]=frnd(-50,50);fa.gz[k]=frnd(-50,50);fa.gnx[k]=frnd(-1,1);fa.gny[k]=frnd(-1,1);fa.gnz[k]=frnd(-1,1);fa.gdc[k]=rnd();fa.gsc[k]=rnd();
   if((rnd()&15)==0){fa.gnx[k]=fa.gny[k]=fa.gnz[k]=0.0f;}if((rnd()&15)==1)fa.gz[k]=fbits(0x7FC00000u);}
  /* Exercise reuse with prefilled diffuse words and ready depth for depth fog. */
  fa.eye_z_ready=fa.fogmode&&!fa.rfog&&(t&8);
  fa.reuse_diffuse=fa.lit&&(t&16);
  for(k=0;k<4;k++)fa.od[k]=rnd();
  fb=fa;fb.cc=cc[1];fb.rec=rec[1];
  tnk_light(&fa);tnk_light_asm(&fb);cases++;
  if(cmp(fa.od,fb.od,16)||cmp(fa.os,fb.os,16)){out("light: words differ, case ");outn(t);out(" (lit ");outn(fa.lit);out(" lights ");outn(fa.nl);out(" fog ");outn(fa.fogmode);out(")\n");bad++;}
 }
 out("tnl asm kernels against the host's: ");outn(cases);out(" cases, ");outn(bad);out(bad?" FAIL\n":" differ: PASS\n");
 __asm__ volatile("int $0x80"::"a"(1),"b"(bad?1:0));
}
