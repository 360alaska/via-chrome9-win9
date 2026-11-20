/* test_tnl_asm.c (.544): the board's T&L group kernels (tests/tnl_kernels.s, the same bytes as src/vx_tnl_sse.asm) against the host's
 * intrinsics (the kernel part of src/vx_d3d_tnl_soa.inc), word for word, on random frames: positions near and across every clip plane,
 * (.697: tnk_splight's specular sums, tnk_cv's kept products) non-finite inputs, lit with 0-8 directional lights, normalized or not, fog off / linear / exp / exp2 / range, unlit.
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
extern void tnk_plight_asm(tnk_frame *f) __asm__("_tnk_plight"); /* .647 */
extern void tnk_spec_asm(tnk_frame *f) __asm__("_tnk_spec"); /* .651: compiled by clang from the C */
extern void tnk_cv_asm(tnk_frame *f,float (*b)[4]) __asm__("_tnk_cv"); /* .652 */
extern void tnk_splight_asm(tnk_frame *f) __asm__("_tnk_splight"); /* .653 */
extern void tnk_vcodes_asm(tnk_frame *f) __asm__("_tnk_vcodes"); /* .702 */

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
static float sx[N+8],sy[N+8],sz[N+8],cx[2][N+8],cy[2][N+8],cz[2][N+8],cw[2][N+8],rw[2][N+8];static uint8_t cc[2][N+8],vc[2][N+8],rec[2][(N+8)*40];
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
  tnk_pos(&fa);tnk_pos_asm(&fb);cases++;
  if(cmp(fa.vacc,fb.vacc,16)){out("pos: vacc differs, case ");outn(t);out("\n");bad++;}
  {uint32_t want[4]={0,0,0,0};for(i=fa.first;i<fa.first+fa.n;i++){const float h=cw[0][i]*0.5f,ax=cx[0][i]<0?-cx[0][i]:cx[0][i],ay=cy[0][i]<0?-cy[0][i]:cy[0][i];
    if(h<ax||h<ay)want[(i-fa.first)&3]=0xFFFFFFFFu;}
   for(k=0;k<4;k++){if((t&2))want[k]=0xFFFFFFFFu;if(!(t&1))want[k]=(t&2)?0xFFFFFFFFu:0;}
   if(cmp(want,fa.vacc,16)){out("pos: vacc wrong, case ");outn(t);out("\n");bad++;}}
  for(i=0;i<N+8;i++)if(vc[0][i]!=0xEE||vc[1][i]!=0xEE){out("pos: wrote viewport codes (.702: tnk_vcodes does), case ");outn(t);out("\n");bad++;break;}
  if(t&1){fb.vc=vc[1];tnk_vcodes(&fa);tnk_vcodes_asm(&fb); /* .702: the codes over the same places */
   if(cmp(vc[0],vc[1],sizeof(vc[0]))){out("vcodes: differ, case ");outn(t);out("\n");bad++;}
   for(i=0;i<N+8;i++){const int in=i>=(int)fa.first&&i<(int)(fa.first+fa.n);uint8_t e=0xEE; /* against the scalar planes, and nothing outside the places */
    if(in){const float h=cw[0][i]*0.5f;e=(uint8_t)((cx[0][i]<-h?1:0)|(cx[0][i]>h?2:0)|(cy[0][i]<-h?4:0)|(cy[0][i]>h?8:0));}
    if(vc[0][i]!=e){out("vcodes: wrong, case ");outn(t);out(" place ");outn(i);out("\n");bad++;break;}}}
  fa.vc=fb.vc=0;
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
  /* .647: the point lights on the same four lanes: 0-8 lights, near and past the range, attenuation above and at or below 0, a light on a vertex (d 0) */
  {uint32_t q;fa.npl=t%9;
   for(k=0;k<8;k++){for(i=0;i<3;i++){splat(fa.pp[k][i],frnd(-60,60));}
    splat(fa.pr[k][0],(t&32)?frnd(1,200):1000.0f);splat(fa.pr[k][1],(rnd()&7)?frnd(0.2f,2):((rnd()&1)?0.0f:-0.5f));splat(fa.pr[k][2],(rnd()&1)?frnd(0,0.2f):0.0f);splat(fa.pr[k][3],(rnd()&1)?frnd(0,0.01f):0.0f);
    for(q=0;q<4;q++)for(i=0;i<3;i++){fa.pdc[k][i][q]=frnd(0,1.2f);fa.pac[k][i][q]=frnd(0,0.5f);}}
   if(t&64){fa.pp[0][0][1]=fa.gx[1];fa.pp[0][1][1]=fa.gy[1];fa.pp[0][2][1]=fa.gz[1];splat(fa.e[0],1);splat(fa.e[1],0);splat(fa.e[2],0);splat(fa.e[4],0);splat(fa.e[5],1);splat(fa.e[6],0);
    splat(fa.e[8],0);splat(fa.e[9],0);splat(fa.e[10],1);splat(fa.e[12],0);splat(fa.e[13],0);splat(fa.e[14],0);}
   for(q=0;q<4;q++)for(i=0;i<4;i++)fa.base[i][q]=frnd(-0.2f,1.3f);
   fb=fa;fb.cc=cc[1];fb.rec=rec[1];
   tnk_plight(&fa);tnk_plight_asm(&fb);cases++;
   if(cmp(fa.base,fb.base,sizeof(fa.base))){out("plight: colours differ, case ");outn(t);out(" (point lights ");outn(fa.npl);out(" normals ");outn(fa.haveN);out(")\n");bad++;}
   fb.cc=fa.cc;fb.rec=fa.rec;memset(fa.scratch,0,sizeof(fa.scratch));memset(fb.scratch,0,sizeof(fb.scratch)); /* .647 (board): nothing else in the frame may change */
   if(cmp(&fa,&fb,sizeof(fa))){out("plight: the frame differs beyond the colours, case ");outn(t);out("\n");bad++;}
   /* .653: the same lights, some of them spot lights: directions, cos(phi/2) below or equal to cos(theta/2), falloff 1, 0, 0.5, 2.5 */
   {static const float fo[4]={1.0f,0.0f,0.5f,2.5f};fa.nspot=0;
    for(k=0;k<8;k++){float d[3],l;fa.pspot[k]=(rnd()&3)!=0;fa.nspot+=fa.pspot[k];
     for(i=0;i<3;i++)d[i]=frnd(-1,1);l=d[0]*d[0]+d[1]*d[1]+d[2]*d[2];if(l<0.01f){d[2]=1;l=1;}
     {float r=1.0f/_mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(l)));for(i=0;i<3;i++)splat(fa.psd[k][i],d[i]*r);}
     {float ct=frnd(0.0f,0.99f),cp=(rnd()&3)?frnd(-0.5f,ct):ct,fl=fo[rnd()&3];splat(fa.pcone[k][0],cp);splat(fa.pcone[k][1],ct);splat(fa.pcone[k][2],fl);fa.pmode[k]=fl==1.0f?1:!(fl>0.0f)?2:0;}}
    for(q=0;q<4;q++)for(i=0;i<4;i++)fa.base[i][q]=frnd(-0.2f,1.3f);
    fa.spec=(t>>5)&1; /* .659: with specular lighting it keeps the lights for tnk_spec (.697: makes the specular sums) */
    fa.pamb=rnd()&0xFF; /* .697: point lights whose ambient adds (the rest skipped) */
    splat(fa.spcut,(t&8)?frnd(0,0.8f):0.0f); /* .697: the specular cutoff */
    {uint32_t q2,c2;for(k=0;k<16;k++)for(c2=0;c2<3;c2++)for(q2=0;q2<4;q2++)fa.sdc[k][c2][q2]=frnd(0,1.1f);}fa.lview=(t>>3)&1;
    {static const float pw2[4]={1.0f,0.0f,0.5f,20.0f};float p2=pw2[(t>>1)&3];splat(fa.spow,p2);fa.spmode=p2==1.0f?1:!(p2>0.0f)?2:0;if(p2==20.0f&&(t&4)){fa.spmode=3;fa.spint=20;}}
    fb=fa;fb.cc=cc[1];fb.rec=rec[1];
    tnk_splight(&fa);tnk_splight_asm(&fb);cases++;
    fb.cc=fa.cc;fb.rec=fa.rec;
    if(cmp(&fa,&fb,sizeof(fa))){out("splight: the frame differs, case ");outn(t);out(" (point lights ");outn(fa.npl);out(" spot ");outn(fa.nspot);out(")\n");bad++;}}}
  /* .651: the specular word on the same lanes: 0-8 directional and 0-8 point lights, a power of 1, 0, below 1 and up to 80, LOCALVIEWER or
   * not, normals normalized or not, the alpha of the word kept */
  {uint32_t q;static const float pw[6]={1.0f,0.0f,0.5f,10.0f,33.0f,80.0f};float p=pw[t%6];
   fa.nl=(t/3)%9;fa.npl=(t/7)%9;fa.spec=1;fa.lview=(t>>3)&1;fa.nnorm=(t>>4)&1;splat(fa.spow,p);fa.spmode=p==1.0f?1:!(p>0.0f)?2:0;
   if(!fa.spmode&&p>=2.0f&&(float)(int)p==p&&((t>>7)&1)){fa.spmode=3;fa.spint=(uint32_t)(int)p;} /* .659: an integer power by squaring */
   splat(fa.spcut,(t&16)?frnd(0,0.8f):0.0f);
   fa.sdone=(t>>6)&1; /* .697: the sums made by tnk_splight (sacc, random here) or the directional lights worked out here */
   for(k=0;k<3;k++)for(q=0;q<4;q++)fa.sacc[k][q]=frnd(0,2.0f);
   for(k=0;k<16;k++)for(i=0;i<3;i++)for(q=0;q<4;q++)fa.sdc[k][i][q]=frnd(0,1.1f);
   for(k=0;k<4;k++)fa.os[k]=rnd();
   fb=fa;fb.cc=cc[1];fb.rec=rec[1];
   tnk_spec(&fa);tnk_spec_asm(&fb);cases++;
   fb.cc=fa.cc;fb.rec=fa.rec;
   if(cmp(&fa,&fb,sizeof(fa))){out("spec: the frame differs, case ");outn(t);out(" (lights ");outn(fa.nl);out("+");outn(fa.npl);out(" power case ");outn(t%6);out(")\n");bad++;}}
  /* .652: the colour from the vertices, four lanes: 0-16 lights mixed directional and point, every source combination, specular on or off */
  {uint32_t q,c;static float ba[4][4],bb[4][4];
   fa.cvn=t%17;fa.spec=(t>>2)&1;fa.cvfirst=(t>>4)&1;fa.cvvary=(t/5)&15;fa.pamb=rnd()&0xFF; /* .697 */fa.dsrc=t%3;fa.asrc=(t/3)%3;fa.esrc=(t/9)%3;fa.ssrc=(t/27)%3;fa.cvconv=(t/81)&15; /* .678: every set of converted words */
   {uint32_t nd=0,np=0;for(k=0;k<16;k++){fa.cvdir[k]=(rnd()>>7)&1;if(fa.cvdir[k]?nd>=8:np>=8)fa.cvdir[k]^=1;if(fa.cvdir[k])nd++;else np++;}} /* .697: at most 8 of each (the set-up's group path's limit; more wrote past pdc, pac and sdc) */
   for(k=0;k<16;k++){for(i=0;i<3;i++)for(c=0;c<3;c++)splat(fa.cvl[k][i][c],frnd(0,1.2f));}
   for(k=0;k<4;k++)for(c=0;c<4;c++)splat(fa.cvm[k][c],frnd(0,1.1f));for(c=0;c<3;c++)splat(fa.cvamb[c],frnd(0,0.5f));
   for(k=0;k<4;k++){fa.gdc[k]=rnd();fa.gsc[k]=rnd();}
   for(q=0;q<4;q++)for(c=0;c<4;c++)ba[q][c]=bb[q][c]=frnd(-1,1);
   fb=fa;fb.cc=cc[1];fb.rec=rec[1];
   tnk_cv(&fa,ba);tnk_cv_asm(&fb,bb);cases++;
   fb.cc=fa.cc;fb.rec=fa.rec;
   if(cmp(&fa,&fb,sizeof(fa))||cmp(ba,bb,sizeof(ba))){out("cv: the frame differs, case ");outn(t);out("\n");bad++;}}
 }
 out("tnl asm kernels against the host's: ");outn(cases);out(" cases, ");outn(bad);out(bad?" FAIL\n":" differ: PASS\n");
 __asm__ volatile("int $0x80"::"a"(1),"b"(bad?1:0));
}
