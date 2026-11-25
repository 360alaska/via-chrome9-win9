/* test_tnq_asm.c (R1, the T&L reworked): the tnq_ kernels of the board (tests/tnl_kernels.s, the same bytes as src/vx_tnl_sse.asm) against
 * the host's intrinsics (part 3 of the kernels in src/vx_d3d_tnl_soa.inc), word for word, on random frames; and what the driver relies
 * on: tnq_gpos's codes are the folded planes' and its viewport flags the viewport's; a vertex has the same four words from tnq_pos and
 * from tnq_gpos; tnq_pos's inside answer is the test of the words; 1/w is exactly 1 for w = 1; tnq_bbxy's bounds and tnq_vc's codes
 * are the records'; a vertex has the same colour words from tnq_lit and tnq_lit4; tnq_cull keeps exactly what vx_area_cull and the
 * viewport codes keep, in order, and marks their corners; tnq_pack, tnq_ixmap and tnq_ixadd copy what they should and nothing past
 * it; tnq_mm4 is the row sums. 32-bit, freestanding (no libc), as tests/test_tnl_asm.c:
 *   python3 tools/tnl_asm.py
 *   sed -n '/TNK KERNELS BEGIN/,/TNK KERNELS END/p' src/vx_d3d_tnl_soa.inc > tests/tnl_kernels_c.inc
 *   clang -m32 --target=i686-linux-gnu -O2 -msse2 -nostdlib -static -ffreestanding -fno-stack-protector -fno-pic -no-pie \
 *     -Wno-gnu-folding-constant -Wa,--noexecstack tests/test_tnq_asm.c tests/tnl_kernels.s -o tests/test_tnq_asm && tests/test_tnq_asm */
typedef unsigned int uint32_t;typedef unsigned char uint8_t;typedef unsigned short uint16_t;typedef unsigned int size_t;
#define offsetof(T,f) ((size_t)&((T*)0)->f)
static void *memset(void *d,int c,size_t n){uint8_t *p=d;while(n--)*p++=(uint8_t)c;return d;}
void *memcpy(void *d,const void *s,size_t n){uint8_t *p=d;const uint8_t *q=s;while(n--)*p++=*q++;return d;}
#include "tnl_kernels_c.inc"
/* the board's */
/* R1: the reworked kernels (compiled by clang from part 3 of the C, tools/tnl_asm.py) */
extern void tnq_pos_asm(tnk_frame *f) __asm__("_tnq_pos");
extern void tnq_gpos_asm(tnk_frame *f) __asm__("_tnq_gpos");
extern void tnq_vcodes_asm(tnk_frame *f) __asm__("_tnq_vcodes");
extern void tnq_bbxy_asm(tnk_frame *f) __asm__("_tnq_bbxy");
extern void tnq_vc_asm(tnk_frame *f) __asm__("_tnq_vc");
extern void tnq_lit_asm(tnk_frame *f) __asm__("_tnq_lit");
extern void tnq_lit4_asm(tnk_frame *f) __asm__("_tnq_lit4");
extern void tnq_attr_asm(tnk_frame *f) __asm__("_tnq_attr");
extern void tnq_cull_asm(tnk_frame *f) __asm__("_tnq_cull");
extern void tnq_ixadd_asm(uint16_t *dst,const uint8_t *src,uint32_t n,uint32_t d) __asm__("_tnq_ixadd");
extern void tnq_ixmap_asm(uint16_t *dst,const uint8_t *src,uint32_t n,const uint16_t *slot,uint32_t bias) __asm__("_tnq_ixmap");
extern uint32_t tnq_pack_asm(uint32_t *dst,const uint8_t *rec,uint32_t rst,const uint8_t *nd,uint32_t nv,uint32_t form,uint16_t *slot,uint32_t b0) __asm__("_tnq_pack");
extern void tnq_mm4_asm(float *o,const float *a,const float *b) __asm__("_tnq_mm4");

static uint32_t seed=12345;
static uint32_t rnd(void){seed=seed*1664525u+1013904223u;return seed;}
static float frnd(float lo,float hi){return lo+(hi-lo)*(float)(rnd()>>8)/16777216.0f;}
static float fbits(uint32_t n){union{float f;uint32_t n;}u;u.n=n;return u.f;}
static void splat(float *o,float v){o[0]=o[1]=o[2]=o[3]=v;}
static void putf(uint8_t *p,float v){memcpy(p,&v,4);}
static uint32_t getu(const uint8_t *p){uint32_t v;memcpy(&v,p,4);return v;}
static float getf(const uint8_t *p){float v;memcpy(&v,p,4);return v;}
static int nonfin(float v){union{float f;uint32_t n;}u;u.f=v;return (u.n&0x7F800000u)==0x7F800000u;}
static long sys_write(int fd,const char *s,unsigned n){long r;__asm__ volatile("int $0x80":"=a"(r):"a"(4),"b"(fd),"c"(s),"d"(n));return r;}
static void out(const char *s){unsigned n=0;while(s[n])n++;sys_write(1,s,n);}
static void outn(unsigned v){char b[12];int i=0;if(!v)b[i++]='0';while(v){b[i++]=(char)('0'+v%10);v/=10;}while(i){char c[2];c[0]=b[--i];c[1]=0;out(c);}}

#define N 64
static tnk_frame fa __attribute__((aligned(16))),fb __attribute__((aligned(16)));
#define A16 __attribute__((aligned(16))) /* R1: the staged and clip arrays are on 16 bytes, as the board's (vx_tnl_data.asm): the tnq_ kernels use aligned operands */
static float sx[N+8] A16,sy[N+8] A16,sz[N+8] A16,cx[2][N+8] A16,cy[2][N+8] A16,cz[2][N+8] A16,cw[2][N+8] A16,rw[2][N+8] A16;static uint8_t cc[2][N+8],vc[2][N+8],rec[2][(N+8)*40];
static void consts(tnk_frame *f){
 static const float expc[7]={1.0f,0.6931471805599453f,0.2402265069591007f,0.0555041086648216f,0.0096181291076285f,0.0013333558146428f,0.0001540353039338f};uint32_t k;
 splat(f->kone,1.0f);splat(f->k255,255.0f);splat(f->khalf,0.5f);splat(f->kl2e,1.4426950408889634f);splat(f->kneg80,-80.0f);
 for(k=0;k<7;k++)splat(f->kexpc[k],expc[k]);splat(f->kinf,fbits(0x7F800000u));splat(f->ksign,fbits(0x80000000u));
 splat(f->klog[0],1.0f/3);splat(f->klog[1],1.0f/5);splat(f->klog[2],1.0f/7);splat(f->klog[3],1.0f/9);splat(f->ksqrt2,1.4142135623730951f);
 for(k=0;k<4;k++){f->kff[k]=0xFFu;f->kmant[k]=0x007FFFFFu;f->k127[k]=127;f->kmask24[k]=0x00FFFFFFu;f->k7f[k]=0x7F;f->kvc[0][k]=1;f->kvc[1][k]=2;f->kvc[2][k]=4;f->kvc[3][k]=8;f->kb[0][k]=1;f->kb[1][k]=2;f->kb[2][k]=8;f->kb[3][k]=16;f->kb[4][k]=32;f->kb[5][k]=64;}
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
  memset(cc,0,sizeof(cc));memset(rec,0,sizeof(rec));memset(vc,0xEE,sizeof(vc));fa.vc=(t&1)?vc[0]:0; /* .702: the past-a-side flags (vacc) every second case */
  for(k=0;k<4;k++)fa.vacc[k]=(t&2)?0xFFFFFFFFu:0; /* (a lane already set stays set) */
  fb=fa;fb.cc=cc[1];fb.rec=rec[1];fb.cx=cx[1];fb.cy=cy[1];fb.cz=cz[1];fb.cw=cw[1];fb.rw=rw[1];fb.vc=(t&1)?vc[1]:0;
  /* R1: tnq_gpos in the folded space (the side planes |X| <= gxw W and |Y| <= gyw W, far Z <= zk W, the record's 1/w times qws) and tnq_vcodes */
  splat(fa.qtwo,2.0f);splat(fa.q15,1.5f);splat(fa.qws,(t%7)==3?0.25f:1.0f);
  splat(fa.qg[0],frnd(0.5f,3.0f));splat(fa.qg[1],frnd(0.5f,3.0f));splat(fa.qg[2],frnd(0.3f,1.0f));splat(fa.qg[3],frnd(0.5f,1.5f));splat(fa.qv[0],fa.qg[0][0]*0.5f);splat(fa.qv[1],fa.qg[1][0]*0.5f);
  fb=fa;fb.cc=cc[1];fb.rec=rec[1];fb.cx=cx[1];fb.cy=cy[1];fb.cz=cz[1];fb.cw=cw[1];fb.rw=rw[1];fb.vc=(t&1)?vc[1]:0;
  tnq_gpos(&fa);tnq_gpos_asm(&fb);cases++;
  if(cmp(fa.vacc,fb.vacc,16)){out("gpos: vacc differs, case ");outn(t);out("\n");bad++;}
  {uint32_t want[4]={0,0,0,0};for(i=fa.first;i<fa.first+fa.n;i++){const float hx=cw[0][i]*fa.qv[0][0],hy=cw[0][i]*fa.qv[1][0],ax=cx[0][i]<0?-cx[0][i]:cx[0][i],ay=cy[0][i]<0?-cy[0][i]:cy[0][i];
    if(hx<ax||hy<ay)want[(i-fa.first)&3]=0xFFFFFFFFu;}
   for(k=0;k<4;k++){if((t&2))want[k]=0xFFFFFFFFu;if(!(t&1))want[k]=(t&2)?0xFFFFFFFFu:0;}
   if(cmp(want,fa.vacc,16)){out("gpos: vacc wrong, case ");outn(t);out("\n");bad++;}}
  for(i=0;i<N+8;i++)if(vc[0][i]!=0xEE||vc[1][i]!=0xEE){out("gpos: wrote viewport codes (tnq_vcodes does), case ");outn(t);out("\n");bad++;break;}
  if(t&1){fb.vc=vc[1];tnq_vcodes(&fa);tnq_vcodes_asm(&fb); /* the codes over the same places */
   if(cmp(vc[0],vc[1],sizeof(vc[0]))){out("vcodes: differ, case ");outn(t);out("\n");bad++;}
   for(i=0;i<N+8;i++){const int in=i>=(int)fa.first&&i<(int)(fa.first+fa.n);uint8_t e=0xEE; /* against the scalar planes, and nothing outside the places */
    if(in){const float hx=cw[0][i]*fa.qv[0][0],hy=cw[0][i]*fa.qv[1][0];e=(uint8_t)((cx[0][i]<0.0f-hx?1:0)|(hx<cx[0][i]?2:0)|(cy[0][i]<0.0f-hy?4:0)|(hy<cy[0][i]?8:0));}
    if(vc[0][i]!=e){out("vcodes: wrong, case ");outn(t);out(" place ");outn(i);out("\n");bad++;break;}}}
  fa.vc=fb.vc=0;
  if(cmp(cx[0],cx[1],sizeof(cx[0]))||cmp(cy[0],cy[1],sizeof(cy[0]))||cmp(cz[0],cz[1],sizeof(cz[0]))||cmp(cw[0],cw[1],sizeof(cw[0]))){out("gpos: clip arrays differ, case ");outn(t);out("\n");bad++;}
  if(cmp(cc[0],cc[1],sizeof(cc[0]))){out("gpos: codes differ, case ");outn(t);out("\n");bad++;}
  if(cmp(rec[0],rec[1],sizeof(rec[0]))){out("gpos: records differ, case ");outn(t);out("\n");bad++;}
  for(i=fa.first;i<fa.first+fa.n;i++){const float X=cx[0][i],Y=cy[0][i],Z=cz[0][i],W=cw[0][i],gw=W*fa.qg[0][0],gh=W*fa.qg[1][0],fz=W*fa.qg[2][0];const uint8_t *r=rec[0]+i*stride;uint8_t e; /* the codes against the scalar planes */
   e=(uint8_t)(((Z<0.0f||W<=0.0f)?1:0)|(Z>fz?2:0)|(X<0.0f-gw?8:0)|(X>gw?16:0)|(Y<0.0f-gh?32:0)|(Y>gh?64:0));
   if(nonfin(X)||nonfin(Y)||nonfin(Z)||nonfin(W)||nonfin(getf(r+12)))e=0x7F;
   if(cc[0][i]!=e){out("gpos: code wrong, case ");outn(t);out(" place ");outn(i);out("\n");bad++;break;}
   if(!(W>0.0f)&&e!=0x7F&&getu(r+12)!=0){out("gpos: 1/w not 0 behind the eye, case ");outn(t);out("\n");bad++;break;}
   if(e!=0x7F&&getf(r+8)>fa.qg[3][0]){out("gpos: depth past the range's end, case ");outn(t);out("\n");bad++;break;}}
  /* R1: the range kernels. Each against the board's bytes; and what the driver relies on: tnq_pos's words are tnq_gpos's for the same
   * vertex, its inside answer is the words' test, tnq_lit's words are tnq_lit4's, tnq_cull keeps what vx_area_cull keeps. */
  {static uint8_t src[(N+12)*48+64],rr[2][(N+12)*40+64],rg[(N+12)*40+64],mk[N+16],vcb[2][N+16],nd2[2][N+24],tr2[2][N+24];static uint16_t tri[2][N*3+8],slot[2][N+8],ixo[2][N*3+8];static uint32_t pk[2][(N+8)*10+8];
   static const uint32_t sts[5]={12,16,24,36,40};const uint32_t ss=sts[t%5],ds=(t&1)?40:32,qn=4+4*(t%15),tame=(t%3)==0;uint32_t q,xyo=0,zwo=0;
   for(i=0;i<sizeof(src);i++)src[i]=(uint8_t)rnd();
   for(i=0;i<N+8;i++){uint8_t *p=src+i*ss;putf(p,frnd(-30,30));putf(p+4,frnd(-30,30));putf(p+8,frnd(-30,30));
    if(!tame&&(rnd()&63)==0)putf(p,fbits(0x7FC00000u));if(!tame&&(rnd()&63)==1)putf(p+4,fbits(0x7F800000u));}
   if(tame){float m[16];for(k=0;k<16;k++)m[k]=0.0f;m[0]=frnd(2,9);m[5]=frnd(2,7);m[10]=0.004f;m[14]=0.2f;m[15]=1.0f;m[3]=frnd(-0.002f,0.002f);m[11]=frnd(0,0.01f);if(t%9==0){m[3]=m[11]=0.0f;}for(k=0;k<16;k++)splat(fa.m[k],m[k]);splat(fa.vp[4],0.1f);}
   for(k=0;k<4;k++){const uint32_t y=k&1;union{float f;uint32_t n;}l0,h0,l1,h1;l0.f=0.0f;h0.f=y?480.0f:640.0f;l1.f=0.0f;h1.f=1.0f;if(y){l1.n=1;h1.n=0x7F7FFFFFu;}
    fa.qlo[0][k]=l0.n+0x80000000u;fa.qlim[0][k]=(h0.n-l0.n)^0x80000000u;fa.qlo[1][k]=l1.n+0x80000000u;fa.qlim[1][k]=(h1.n-l1.n)^0x80000000u;fa.qacc[k]=0;fa.qk12[k]=1u<<(k&1);fa.qk48[k]=4u<<(k&1);}
   for(i=0;i<sizeof(rr[0]);i++)rr[0][i]=rr[1][i]=rg[i]=(uint8_t)rnd();
   fa.qsrc=src;fa.qdst=rr[0];fa.qn=qn;fa.qsstride=ss;fa.qdstride=ds;fb=fa;fb.qdst=rr[1];
   tnq_pos(&fa);tnq_pos_asm(&fb);cases++;
   if(cmp(rr[0],rr[1],sizeof(rr[0]))||cmp(fa.qacc,fb.qacc,16)){out("pos: records or the inside answer differ, case ");outn(t);out("\n");bad++;}
   for(i=0;i<qn;i++){const uint8_t *r=rr[0]+i*ds;const uint32_t bx=getu(r),by=getu(r+4),bz=getu(r+8),bw=getu(r+12);
    if(bx>0x44200000u||by>0x43F00000u)xyo=1;if(bz>0x3F800000u||bw-1u>0x7F7FFFFEu)zwo=1;} /* x in 0 .. 640, y in 0 .. 480, z in 0 .. 1, 1/w above 0 and finite, by the bits */
   {const uint16_t *a=(const uint16_t*)(const void*)fa.qacc;const uint32_t gx=a[0]|a[1]|a[2]|a[3],gz=a[4]|a[5]|a[6]|a[7];
    if((gx!=0)!=(xyo!=0)||(gz!=0)!=(zwo!=0)){out("pos: the inside answer is not the words' test, case ");outn(t);out("\n");bad++;}
    if(tame&&t%9==0&&(gz||fa.qacc[0]!=fa.qacc[0])){out("pos: tame case outside\n");bad++;}}
   /* the same vertices through tnq_gpos: the same four words wherever w is above 0 (the staged path's depth is held to the range's end: put out of reach here) */
   for(i=0;i<qn;i++){sx[i]=getf(src+i*ss);sy[i]=getf(src+i*ss+4);sz[i]=getf(src+i*ss+8);}
   {static tnk_frame fg __attribute__((aligned(16)));fg=fa;fg.first=0;fg.n=qn;fg.rec=rg;fg.stride=ds;fg.vc=0;splat(fg.qws,1.0f);splat(fg.qg[3],3.0e38f);fg.cc=cc[0];fg.cx=cx[0];fg.cy=cy[0];fg.cz=cz[0];fg.cw=cw[0];
    tnq_gpos(&fg);
    for(i=0;i<qn;i++)if(cw[0][i]>0.0f&&cc[0][i]!=0x7F&&cmp(rg+i*ds,rr[0]+i*ds,16)){out("pos: a vertex's words are not tnq_gpos's, case ");outn(t);out(" vertex ");outn(i);out("\n");bad++;break;}
    memcpy(cx[1],cx[0],sizeof(cx[0]));memcpy(cy[1],cy[0],sizeof(cy[0]));memcpy(cz[1],cz[0],sizeof(cz[0]));memcpy(cw[1],cw[0],sizeof(cw[0]));memcpy(cc[1],cc[0],sizeof(cc[0]));}
   if(tame&&t%9==0)for(i=0;i<qn;i++)if(getu(rr[0]+i*ds+12)!=0x3F800000u){out("pos: 1/w not exactly 1 for w = 1, case ");outn(t);out("\n");bad++;break;}
   /* the bounds and the viewport codes of those records */
   splat(fa.qmn,3.0e38f);splat(fa.qmx,-3.0e38f);for(k=0;k<4;k++){fa.qvlo[k]=(k&1)?frnd(0,200):frnd(0,300);fa.qvhi[k]=(k&1)?frnd(200,480):frnd(300,640);}fa.qvlo[2]=fa.qvlo[0];fa.qvlo[3]=fa.qvlo[1];fa.qvhi[2]=fa.qvhi[0];fa.qvhi[3]=fa.qvhi[1];
   fa.qdst=rr[0];fa.qvc=vcb[0];fb=fa;fb.qvc=vcb[1];memset(vcb,0xEE,sizeof(vcb));
   tnq_bbxy(&fa);tnq_bbxy_asm(&fb);cases++;
   if(cmp(fa.qmn,fb.qmn,16)||cmp(fa.qmx,fb.qmx,16)||cmp(fa.qacc,fb.qacc,16)){out("bbxy: differs, case ");outn(t);out("\n");bad++;}
   {float lo[2]={3.0e38f,3.0e38f},hi[2]={-3.0e38f,-3.0e38f};uint32_t nf=0;
    for(i=0;i<qn;i++)for(k=0;k<2;k++){const float v=getf(rr[0]+i*ds+k*4);if(nonfin(v)){nf=1;continue;}if(v<lo[k])lo[k]=v;if(v>hi[k])hi[k]=v;}
    for(k=0;k<2;k++){const float l=fa.qmn[k]<fa.qmn[k+2]?fa.qmn[k]:fa.qmn[k+2],h=fa.qmx[k]>fa.qmx[k+2]?fa.qmx[k]:fa.qmx[k+2];
     if(!nf&&(l!=lo[k]||h!=hi[k])){out("bbxy: bounds wrong, case ");outn(t);out("\n");bad++;break;}}
    if((nf!=0)!=((fa.qacc[0]|fa.qacc[1]|fa.qacc[2]|fa.qacc[3])!=0)){out("bbxy: the no-number answer wrong, case ");outn(t);out("\n");bad++;}}
   tnq_vc(&fa);tnq_vc_asm(&fb);cases++;
   if(cmp(vcb[0],vcb[1],qn)||cmp(fa.qacc,fb.qacc,16)){out("vc: differs, case ");outn(t);out("\n");bad++;}
   for(i=0;i<qn;i++){const float x=getf(rr[0]+i*ds),y=getf(rr[0]+i*ds+4);const uint8_t e=(uint8_t)((x<fa.qvlo[0]?1:0)|(y<fa.qvlo[1]?2:0)|(fa.qvhi[0]<x?4:0)|(fa.qvhi[1]<y?8:0));
    if(vcb[0][i]!=e){out("vc: code wrong, case ");outn(t);out("\n");bad++;break;}}
   for(i=qn+2;i<N+16;i++)if(vcb[0][i]!=0xEE){out("vc: wrote past its two spare bytes, case ");outn(t);out("\n");bad++;break;}
   /* lighting: 0-8 lights, unit length or not, the vertex colour as the material or not, marks or none */
   fa.qnl=t%9;fa.qnnorm=(t>>1)&1;fa.qvcol=(t>>2)&1;fa.qinorm=ss>=24?12:0;fa.qidif=ss>=36?24:(ss>=16?12:8);
   for(k=0;k<8;k++)for(i=0;i<3;i++){splat(fa.ql[k][i],frnd(-1,1));splat(fa.qc[k][i],frnd(0,300));}
   for(i=0;i<3;i++){splat(fa.qbase[i],frnd(-20,280));splat(fa.qv0[i],frnd(0,0.6f));}
   for(k=0;k<4;k++){fa.qalpha[k]=rnd()&0xFF;fa.qspec[k]=rnd();}fa.qspec[1]=fa.qspec[2]=fa.qspec[3]=fa.qspec[0];
   for(i=0;i<N+16;i++)mk[i]=(uint8_t)((rnd()&3)==0);for(i=0;i<8&&(t&8);i++)mk[i]=0;mk[4]=(t&8)?0:1;
   if(ss>=24)for(i=0;i<N+8;i++){uint8_t *p=src+i*ss+12;putf(p,frnd(-1,1));putf(p+4,frnd(-1,1));putf(p+8,frnd(-1,1));if((rnd()&31)==0){putf(p,0.0f);putf(p+4,0.0f);putf(p+8,0.0f);}if((rnd()&15)==1){putf(p,0.0f);putf(p+4,0.0f);putf(p+8,1.0f);}}
   fa.qmark=(t&16)?mk:0;fa.qdst=rr[0];fb=fa;fb.qdst=rr[1];
   tnq_lit(&fa);tnq_lit_asm(&fb);cases++;
   if(cmp(rr[0],rr[1],sizeof(rr[0]))){out("lit: records differ, case ");outn(t);out(" (lights ");outn(fa.qnl);out(")\n");bad++;}
   q=qn>=8?4:0; /* the group compared with tnq_lit4 */
   for(k=0;k<4;k++){fa.gnx[k]=getf(src+(q+k)*ss+fa.qinorm);fa.gny[k]=getf(src+(q+k)*ss+fa.qinorm+4);fa.gnz[k]=getf(src+(q+k)*ss+fa.qinorm+8);fa.gdc[k]=getu(src+(q+k)*ss+fa.qidif);}
   if(!fa.qnl)for(k=0;k<4;k++)fa.gnx[k]=fa.gny[k]=fa.gnz[k]=0.0f;
   fb=fa;tnq_lit4(&fa);tnq_lit4_asm(&fb);cases++;
   if(cmp(fa.od,fb.od,16)){out("lit4: differs, case ");outn(t);out("\n");bad++;}
   if(!(t&16)||mk[q]|mk[q+1]|mk[q+2]|mk[q+3])for(k=0;k<4;k++)if(fa.od[k]!=getu(rr[0]+(q+k)*ds+16)||getu(rr[0]+(q+k)*ds+20)!=fa.qspec[0]){out("lit: a vertex's words are not tnq_lit4's, case ");outn(t);out("\n");bad++;break;}
   /* the copied attributes: any count, 0-2 sets (3 with 48-byte records: not here), coordinates that are no number */
   fa.qadm=t%3;fa.qado=ss>=16?12:4;fa.qadc=rnd();fa.qasm=(t/3)%4;fa.qasc=rnd();fa.qaso=ss>=24?20:8;fa.qans=ds==40?(t/12)%3:(t/12)%2;
   for(k=0;k<4;k++){fa.qato[k]=((rnd()&7)==0)?0xFFFFFFFFu:(ss>=24?(k?16u:8u):(ss>=16?8u:4u));fa.qatv[k]=(rnd()&3)!=0;fa.qatd[k]=6+2*k;}
   if((t&3)==1){fa.qato[0]=ss>=24?16:4;fa.qato[1]=ss>=24?8:0;fa.qatv[0]=fa.qatv[1]=1;} /* the loops for one and two whole sets */
   for(i=0;i<N;i++)if((rnd()&15)==0)putf(src+i*ss+(rnd()%(ss/4))*4,fbits((rnd()&1)?0x7F800000u:0xFFC00001u));
   fa.qn=1+t%61;fa.qmark=(t&32)?mk:0;fb=fa;fb.qdst=rr[1];
   tnq_attr(&fa);tnq_attr_asm(&fb);cases++;
   if(cmp(rr[0],rr[1],sizeof(rr[0]))){out("attr: records differ, case ");outn(t);out("\n");bad++;}
   for(i=0;i<fa.qn&&fa.qans;i++)if(!(t&32)||mk[i]){const uint32_t u=getu(rr[0]+i*ds+24),v=getu(rr[0]+i*ds+28);if((u&0x7F800000u)==0x7F800000u||(v&0x7F800000u)==0x7F800000u){out("attr: a coordinate that is no number, case ");outn(t);out("\n");bad++;break;}}
   /* the cull: window positions on a small grid (areas of both signs and zero), 0 none / 2 / 3, viewport codes or none, in place */
   {const uint32_t lo=(t%5)*3,nvv=3+t%40,nt=1+t%N,cm=(t%3)==0?0:2+((t>>1)&1);uint32_t kept=0;
    for(i=0;i<N+12;i++){putf(rr[0]+i*ds,(float)(rnd()%7));putf(rr[0]+i*ds+4,(float)(rnd()%7));if((rnd()&63)==0)putf(rr[0]+i*ds,fbits(0x7FC00000u));}
    for(i=0;i<nt*3;i++)tri[0][i]=tri[1][i]=(uint16_t)(lo+rnd()%nvv);
    for(i=0;i<N+16;i++){vcb[0][i]=(uint8_t)((rnd()&3)?0:(1u<<(rnd()&3)));}
    memset(nd2,0,sizeof(nd2));memset(tr2,0,sizeof(tr2));
    fa.qtidx=(const uint8_t*)tri[0];fa.qtout=tri[0];fa.qtn=nt;fa.qtist=6;fa.qtrst=ds;fa.qtcm=cm;fa.qtrec=rr[0]-lo*ds;fa.qtvc=(t&2)?vcb[0]-lo:0;fa.qtnd=nd2[0]-lo;fa.qttrash=tr2[0]-lo;fa.qtkept=77;
    fb=fa;fb.qtidx=(const uint8_t*)tri[1];fb.qtout=tri[1];fb.qtnd=nd2[1]-lo;fb.qttrash=tr2[1]-lo;
    {static uint16_t t0[N*3+8];static uint8_t want[N+24];uint32_t w=0;memcpy(t0,tri[0],sizeof(t0));memset(want,0,sizeof(want));
     for(i=0;i<nt;i++){const uint32_t a=t0[i*3],b=t0[i*3+1],c=t0[i*3+2];int keep=1;
      if((t&2)&&(vcb[0][a-lo]&vcb[0][b-lo]&vcb[0][c-lo]))keep=0;
      if(cm){const double x0=getf(rr[0]+(a-lo)*ds),y0=getf(rr[0]+(a-lo)*ds+4),x1=getf(rr[0]+(b-lo)*ds),y1=getf(rr[0]+(b-lo)*ds+4),x2=getf(rr[0]+(c-lo)*ds),y2=getf(rr[0]+(c-lo)*ds+4);
       const double ar=(x1-x0)*(y2-y0)-(y1-y0)*(x2-x0);if(ar==0.0)keep=0;else if(ar!=ar);else if(ar>0.0?cm==2:cm==3)keep=0;}
      if(keep){t0[w*3]=(uint16_t)a;t0[w*3+1]=(uint16_t)b;t0[w*3+2]=(uint16_t)c;w++;want[a-lo]=want[b-lo]=want[c-lo]=1;}}
     kept=w;
     tnq_cull(&fa);tnq_cull_asm(&fb);cases++;
     if(fa.qtkept!=fb.qtkept||cmp(tri[0],tri[1],fa.qtkept*6)||cmp(nd2[0],nd2[1],sizeof(nd2[0]))){out("cull: differs, case ");outn(t);out("\n");bad++;}
     if(fa.qtkept!=kept||cmp(tri[0],t0,kept*6)||cmp(nd2[0],want,sizeof(want))){out("cull: not vx_area_cull's answers, case ");outn(t);out("\n");bad++;}}
    /* the packet copies of the kept triangles: marked records in order and the indices through the table; a block and one addition */
    {const uint32_t form=(t>>3)&3;uint32_t n0,n1;const uint32_t b0=t%400;
     memset(pk,0x5A,sizeof(pk));memset(slot,0x33,sizeof(slot));
     n0=tnq_pack(pk[0],rr[0],ds,(t&1)?nd2[0]:0,nvv,form,(t&4)?slot[0]:0,b0);n1=tnq_pack_asm(pk[1],rr[0],ds,(t&1)?nd2[0]:0,nvv,form,(t&4)?slot[1]:0,b0);cases++;
     if(n0!=n1||cmp(pk[0],pk[1],sizeof(pk[0]))||cmp(slot[0],slot[1],sizeof(slot[0]))){out("pack: differs, case ");outn(t);out("\n");bad++;}
     {uint32_t w=0;const uint32_t vw=(form&1)?10:8;for(i=0;i<nvv;i++)if(!(t&1)||nd2[0][i]){const uint8_t *r=rr[0]+i*ds;
       if(cmp(pk[0]+w*vw,r,32)||((form&1)&&cmp(pk[0]+w*vw+8,r+((form&2)?32:24),8))||((t&4)&&slot[0][i]!=(uint16_t)(b0+w))){out("pack: a record's words or slot wrong, case ");outn(t);out("\n");bad++;break;}w++;}
      if(w!=n0||pk[0][w*vw]!=0x5A5A5A5Au){out("pack: count wrong or wrote past, case ");outn(t);out("\n");bad++;}}
     if((t&5)==5){memset(ixo,0x77,sizeof(ixo));tnq_ixmap(ixo[0],(const uint8_t*)tri[0],kept*3,slot[0],lo);tnq_ixmap_asm(ixo[1],(const uint8_t*)tri[0],kept*3,slot[0],lo);cases++;
      if(cmp(ixo[0],ixo[1],sizeof(ixo[0]))){out("ixmap: differs, case ");outn(t);out("\n");bad++;}
      for(i=0;i<kept*3;i++)if(ixo[0][i]!=slot[0][tri[0][i]-lo]){out("ixmap: wrong, case ");outn(t);out("\n");bad++;break;}}
     memset(ixo,0x77,sizeof(ixo));tnq_ixadd(ixo[0],(const uint8_t*)tri[0],kept*3,b0-lo);tnq_ixadd_asm(ixo[1],(const uint8_t*)tri[0],kept*3,b0-lo);cases++;
     if(cmp(ixo[0],ixo[1],sizeof(ixo[0]))){out("ixadd: differs, case ");outn(t);out("\n");bad++;}
     for(i=0;i<kept*3;i++)if(ixo[0][i]!=(uint16_t)(tri[0][i]+b0-lo)){out("ixadd: wrong, case ");outn(t);out("\n");bad++;break;}
     if(ixo[0][kept*3]!=0x7777){out("ixadd: wrote past, case ");outn(t);out("\n");bad++;}}}
   /* the matrix product */
   {static float ma[20],mb[20],mo[2][20];for(i=0;i<20;i++){ma[i]=frnd(-4,4);mb[i]=frnd(-4,4);mo[0][i]=mo[1][i]=7.0f;}
    tnq_mm4(mo[0]+1,ma+2,mb+3);tnq_mm4_asm(mo[1]+1,ma+2,mb+3);cases++;
    if(cmp(mo[0],mo[1],sizeof(mo[0]))||mo[0][0]!=7.0f||mo[0][17]!=7.0f){out("mm4: differs or wrote outside, case ");outn(t);out("\n");bad++;}
    for(i=0;i<4;i++)for(k=0;k<4;k++){const float *a=ma+2+i*4,*b=mb+3;const float e=((a[0]*b[k]+a[1]*b[4+k])+a[2]*b[8+k])+a[3]*b[12+k];if(mo[0][1+i*4+k]!=e){out("mm4: not the row sums, case ");outn(t);out("\n");bad++;i=4;break;}}}
   splat(fa.vp[4],frnd(0,0.5f));}
 }
 out("tnq kernels against the host's: ");outn(cases);out(" cases, ");outn(bad);out(bad?" FAIL\n":" differ: PASS\n");
 __asm__ volatile("int $0x80"::"a"(1),"b"(bad?1:0));
}
