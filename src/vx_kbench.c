/* VXKBENCH v1.9 (driver .751): the position kernel cold with its outputs 0-12 bytes off a 16-byte line. VXKBENCH v1.8 (driver .750): sweep, a kernel's place within a 64-byte line. VXKBENCH v1.7 (.749B, the tool only): the position kernels beside five cut-down copies each (divide, clip-array stores, record stores).
 * VXKBENCH v1.6 (driver .749): the back-face test and the serial position kernel gone with the driver's (T748); big_test, the position
 * kernels over 200,000 cold places. VXKBENCH v1.4 (driver .747): the outside review's two hand-written tnk_splight (C7 and Nano orders; src/vx_tnl_cand.asm) beside the
 * driver's; tn_draw's back-face test three ways (tri_test). VXKBENCH v1.3 (driver .746): the driver's own kernels only (the winners of T745 are in the driver; the candidate module is deleted), the
 * serial tnk_pos_s and tnk_light_h beside them, two cases more (one light with normalized normals, two lights without). VXKBENCH v1.2 (driver .745): one frame a case for every row (v1.1 rebuilt it per row with new random normals and positions, so the
 * output check marked every row that reads them). VXKBENCH v1.1 (driver .744B): the outside reviews' candidate kernels as rows (src/vx_tnl_cand.asm: 26 that passed the kernel test in place
 * of the production kernel), three position cases (64 vertices, 4, the viewport flags on), fog and two-light cases, the spot / specular
 * kernel; every row's output checked against the driver's kernel; fastest first with the difference in percent; VXKBENCH.CSV; a
 * serializing CPUID before each RDTSC. VXKBENCH v1.0 (driver .744): the driver's T&L kernels timed on this machine's processor. T742 (boards): the same position kernel in two
 * instruction orders ran 9% faster on the C7 and 19% slower on the Nano, so which order is best is a question for the processor, not
 * for a reading of the code. Each kernel variant linked here (the bytes of src/vx_tnl_sse.asm, the ones VXDDRAW.DLL runs) is called on
 * a fixed, typical frame many times; the cycle counter (RDTSC) is read around runs of calls, 41 runs a case, and the least and the
 * middle run are reported as cycles a vertex. Variants of one job are checked to give the same output words, then ranked.
 * Nothing touches the screen or the driver's state (the VxD is only asked whether SSE2 may be used). A few seconds. Log: VXKBENCH.LOG.
 * Only the position job's variants are checked for the same output (the light jobs' variants are given different inputs by design).
 * A new variant: add its public name to tools/tnl_asm.py's list and a row to the table below. */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#define VX_TNK_FRAME_ONLY
#include "vx_d3d_tnl_soa.inc"
extern void __cdecl tnk_pos(tnk_frame *f);
extern void __cdecl tnk_pos_o(tnk_frame *f);
extern void __cdecl tnk_light(tnk_frame *f);
extern void __cdecl tnk_light_k(tnk_frame *f);
extern void __cdecl tnk_plight(tnk_frame *f);
static FILE *lf;
static void say(const char *f,...) {static char b[1024];va_list a;va_start(a,f);_vsnprintf(b,sizeof(b)-1,f,a);b[sizeof(b)-1]=0;va_end(a);fputs(b,stdout);if(lf){fputs(b,lf);fflush(lf);}}
static void tsc(DWORD *lo_hi);
#pragma aux tsc = 0x33 0xC0 0x0F 0xA2 0x0F 0x31 0x89 0x06 0x89 0x56 0x04 parm [esi] modify exact [eax ebx ecx edx]; /* v1.1: cpuid 0 (serializing: the Nano runs out of order), rdtsc; mov [esi],eax; mov [esi+4],edx */
static DWORD cpu1(void);
#pragma aux cpu1 = 0xB8 0x01 0x00 0x00 0x00 0x0F 0xA2 value [eax] modify exact [eax ebx ecx edx];
static void cpu0(DWORD *v);
#pragma aux cpu0 = 0x33 0xC0 0x0F 0xA2 0x89 0x1E 0x89 0x56 0x04 0x89 0x4E 0x08 parm [esi] modify exact [eax ebx ecx edx]; /* cpuid 0; ebx, edx, ecx to v */
static double now(void) {DWORD t[2];tsc(t);return (double)t[1]*4294967296.0+(double)t[0];}
#define NV 64
static uint8_t fmem[2][sizeof(tnk_frame)+16];static tnk_frame *F,*F0;
static float sx[NV+8],sy[NV+8],sz[NV+8],cx[NV+8],cy[NV+8],cz[NV+8],cw[NV+8],rw[NV+8];static uint8_t cc[NV+8],vcb[NV+8],rec[(NV+8)*40];
static void splat(float *o,float v) {o[0]=o[1]=o[2]=o[3]=v;}
static DWORD seed=12345;static DWORD rnd(void) {seed=seed*1664525UL+1013904223UL;return seed;}
static float frnd(float lo,float hi) {return lo+(hi-lo)*(float)(rnd()>>8)/16777216.0f;}
/* the frame every case starts from: a perspective view of vertices in front of the eye, all inside the screen; a rotation for world x view
 * and for the normals; one directional light; a point light at the eye's side */
static void base_frame(tnk_frame *f) {
 static const float expc[7]={1.0f,0.6931471805599453f,0.2402265069591007f,0.0555041086648216f,0.0096181291076285f,0.0013333558146428f,0.0001540353039338f};
 static const float wv[16]={0.8f,0.0f,0.6f,0, 0.0f,1.0f,0.0f,0, -0.6f,0.0f,0.8f,0, 0.1f,-0.2f,5.0f,1};
 float m[16];DWORD k,i,q;
 memset(f,0,sizeof(*f));
 splat(f->kone,1.0f);splat(f->k255,255.0f);f->kr255=1.0f/255.0f;splat(f->khalf,0.5f);splat(f->kl2e,1.4426950408889634f);splat(f->kneg80,-80.0f);
 for(k=0;k<7;k++)splat(f->kexpc[k],expc[k]);
 for(k=0;k<4;k++){((DWORD*)f->kinf)[k]=0x7F800000UL;((DWORD*)f->ksign)[k]=0x80000000UL;}
 splat(f->klog[0],1.0f/3);splat(f->klog[1],1.0f/5);splat(f->klog[2],1.0f/7);splat(f->klog[3],1.0f/9);splat(f->ksqrt2,1.4142135623730951f);
 for(k=0;k<4;k++){f->kff[k]=0xFFUL;f->kmant[k]=0x007FFFFFUL;f->k127[k]=127;f->kmask24[k]=0x00FFFFFFUL;f->k7f[k]=0x7F;f->kvc[0][k]=1;f->kvc[1][k]=2;f->kvc[2][k]=4;f->kvc[3][k]=8;
  f->kb[0][k]=1;f->kb[1][k]=2;f->kb[2][k]=8;f->kb[3][k]=16;f->kb[4][k]=32;f->kb[5][k]=64;}
 /* clip = eye x projection (w = z, x and y scaled by 1.5, z to 0..w for near 1, far 100) */
 for(k=0;k<4;k++){m[k*4]=wv[k*4]*1.5f;m[k*4+1]=wv[k*4+1]*1.5f;m[k*4+2]=wv[k*4+2]*(100.0f/99.0f)-(k==3?100.0f/99.0f:0.0f);m[k*4+3]=wv[k*4+2];}
 for(k=0;k<16;k++){splat(f->m[k],m[k]);splat(f->e[k],wv[k]);}
 for(k=0;k<3;k++)for(i=0;i<3;i++)splat(f->nm[k*3+i],wv[k*4+i]);
 splat(f->vp[0],320.0f);splat(f->vp[1],320.0f);splat(f->vp[2],240.0f);splat(f->vp[3],-240.0f);splat(f->vp[4],0.0f);splat(f->vp[5],1.0f);splat(f->vp[6],0.0f);
 f->lit=1;f->haveN=1;f->nnorm=0;f->nl=1;f->fogmode=0;f->stride=32;f->first=0;f->n=NV;
 splat(f->ld[0][0],0.3f);splat(f->ld[0][1],0.5f);splat(f->ld[0][2],-0.81f);splat(f->lc[0][0],0.7f);splat(f->lc[0][1],0.6f);splat(f->lc[0][2],0.5f);
 splat(f->ld[1][0],-0.6f);splat(f->ld[1][1],0.2f);splat(f->ld[1][2],-0.77f);splat(f->lc[1][0],0.2f);splat(f->lc[1][1],0.3f);splat(f->lc[1][2],0.4f);
 for(i=0;i<4;i++){splat(f->base[i],i==3?1.0f:0.1f);splat(f->lbase[i],i==3?1.0f:0.1f);}
 f->sx=sx;f->sy=sy;f->sz=sz;f->cx=cx;f->cy=cy;f->cz=cz;f->cw=cw;f->rw=rw;f->cc=cc;f->rec=rec;f->vc=0;
 for(k=0;k<8;k++){for(i=0;i<3;i++)splat(f->pp[k][i],i==2?4.0f+(float)k:(float)k-3.0f);
  splat(f->pr[k][0],20.0f);splat(f->pr[k][1],1.0f);splat(f->pr[k][2],0.1f);splat(f->pr[k][3],0.01f);
  for(q=0;q<4;q++)for(i=0;i<3;i++){f->pdc[k][i][q]=0.5f;f->pac[k][i][q]=0.05f;}}
 f->pamb=255;f->pskip=0;f->npl=0;
 for(k=0;k<4;k++){f->gx[k]=frnd(-1,1);f->gy[k]=frnd(-1,1);f->gz[k]=frnd(-1,1);f->gnx[k]=frnd(-1,1);f->gny[k]=frnd(-1,1);f->gnz[k]=frnd(-1,1);f->gdc[k]=0xFFFFFFFFUL;f->gsc[k]=0;}
}
extern void __cdecl tnk_splight(tnk_frame *f);
typedef void (__cdecl *kfn)(tnk_frame *);
enum {FAM_POS,FAM_LIGHT,FAM_LIGHT_K,FAM_PLIGHT,FAM_SPLIGHT,FAM_N};
typedef struct {int fam;const char *name;kfn fn;} variant;
extern void __cdecl tnk_light_h(tnk_frame *f);
/* v1.3 (.746): the driver's kernels only. The outside candidates of v1.1-v1.2 are gone: the winners of T745 are the driver's kernels now */
static const variant V[]={
 {FAM_POS,"tnk_pos (C7 family: pair, divgap)",tnk_pos},{FAM_POS,"tnk_pos_o (others: pair, imask)",tnk_pos_o},
 {FAM_LIGHT,"tnk_light (pack4)",tnk_light},{FAM_LIGHT,"tnk_light_h (.742; Nano: 2 lights or fog)",tnk_light_h},{FAM_LIGHT_K,"tnk_light_k (pack4)",tnk_light_k},
 {FAM_PLIGHT,"tnk_plight",tnk_plight},{FAM_SPLIGHT,"tnk_splight (hand-written, .748)",tnk_splight},
};
#define NVAR (sizeof(V)/sizeof(V[0]))
/* the cases: cs 0 positions, 64 vertices inside the screen; 9 the same, 4 vertices (the call's fixed part); 10 as 0 with the guard band's
 * viewport flags on; 1 one directional light; 2 two, normals normalized; 11 one with exponential fog; 3 / 12 / 13 the same three with the
 * normals given (after a point-light kernel); 4 one point light in range; 5 one out of range; 6 four in range; 7 four, two out of range;
 * 14 two point lights, one a spot light; 15 the same with specular lighting (power 20) */
typedef struct {int fam,cs,per;const char *what;} kcase;
static const kcase C[]={
 {FAM_POS,0,NV,"positions and clip codes, 64 vertices a call"},{FAM_POS,9,4,"positions and clip codes, 4 vertices a call"},{FAM_POS,10,NV,"positions, 64 vertices, the guard band's viewport flags on"},
 {FAM_LIGHT,1,4,"one directional light"},{FAM_LIGHT,16,4,"one directional light, normals normalized"},{FAM_LIGHT,17,4,"two directional lights"},{FAM_LIGHT,2,4,"two directional lights, normals normalized"},{FAM_LIGHT,11,4,"one directional light, exponential fog"},
 {FAM_LIGHT_K,3,4,"one directional light, normals given"},{FAM_LIGHT_K,12,4,"two directional lights, normals given"},{FAM_LIGHT_K,13,4,"one directional light, normals given, exponential fog"},
 {FAM_PLIGHT,4,4,"one point light in range"},{FAM_PLIGHT,5,4,"one point light out of range"},{FAM_PLIGHT,6,4,"four point lights in range"},{FAM_PLIGHT,7,4,"four point lights, two out of range"},
 {FAM_SPLIGHT,14,4,"two point lights, one a spot light"},{FAM_SPLIGHT,15,4,"two point lights, one a spot light, specular (power 20)"}};
#define NCASE (sizeof(C)/sizeof(C[0]))
static void set_case(tnk_frame *f,int cs) {
 DWORD k,i,q;base_frame(f);
 if(cs==9)f->n=4;
 if(cs==10)f->vc=vcb;
 if(cs==2||cs==12){f->nl=2;f->nnorm=cs==2;}
 if(cs==16)f->nnorm=1;
 if(cs==17)f->nl=2; /* v1.3: 16 and 17 take case 2 apart (the Nano's choice between tnk_light and tnk_light_h) */
 if(cs==11||cs==13){f->fogmode=1;splat(f->fog[0],0.05f);}
 if(cs==3||cs==12||cs==13){f->nready=1;for(k=0;k<4;k++){f->nv[0][k]=0.3f;f->nv[1][k]=0.4f;f->nv[2][k]=-0.86f;}}
 if(cs>=4&&cs<=7){f->nl=0;f->npl=cs>=6?4:1;if(cs==5)splat(f->pr[0][0],0.5f);if(cs==7){splat(f->pr[1][0],0.5f);splat(f->pr[3][0],0.5f);}}
 if(cs==14||cs==15){f->nl=1;f->npl=2;f->nspot=1;f->pspot[1]=1;f->pmode[1]=1;
  splat(f->psd[1][0],0.0f);splat(f->psd[1][1],0.0f);splat(f->psd[1][2],1.0f);splat(f->pcone[1][0],0.2f);splat(f->pcone[1][1],0.8f);splat(f->pcone[1][2],1.0f);
  if(cs==15){f->spec=1;f->lview=0;splat(f->spow,20.0f);f->spmode=3;f->spint=20;splat(f->spcut,0.0f);
   for(k=0;k<16;k++)for(i=0;i<3;i++)for(q=0;q<4;q++)f->sdc[k][i][q]=0.3f;}}
}
static int cmpd(const void *a,const void *b) {double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y;}
/* a run: calls times the kernel, the flags a kernel sets put back before each call (the same three stores for every variant) */
static double run(kfn fn,DWORD calls) {
 const DWORD nr=F0->nready,er=F0->eready,sd=F0->sdone;double t0,t1;DWORD k;
 t0=now();for(k=0;k<calls;k++){F->nready=nr;F->eready=er;F->sdone=sd;fn(F);}t1=now();
 return t1-t0;
}
static void __cdecl nothing(tnk_frame *f) {(void)f;}
static DWORD sum(const void *p,DWORD n) {const uint8_t *q=p;DWORD s=0,k;for(k=0;k<n;k++)s=s*31+q[k];return s;}
/* v1.6 (.749): the position kernels as the driver feeds them: 200,000 places walked once a run in calls of 256 (the driver's TN_CH) from one
 * staging chunk, so every output (four clip arrays, the w array, the codes, 6.4 MB of records) written cold, as in a frame of
 * 200,000 vertices. T748: a kernel 20-29% faster in the in-cache bench made the frame 2% slower; this case shows what the memory costs. */
/* v1.8 (.750): a kernel's place within a 64-byte line. T749, VX900: tnk_pos_o took 49% longer in the frame after other code moved it from
 * 37 to 11 within its line. Each kernel's bytes (position-independent) are copied to all 64 places in executable memory and timed there. */
#include "vx_tnl_sizes.inc"
static uint8_t *swx;
static double sweep_one(kfn at,int cs,int per) {
 double r[9],e[9];DWORD j;const DWORD calls=per>=NV?100:1000;
 set_case(F0,cs);
 for(j=0;j<9;j++){*F=*F0;e[j]=run(nothing,calls);*F=*F0;r[j]=run(at,calls);}
 qsort(r,9,sizeof(double),cmpd);qsort(e,9,sizeof(double),cmpd);return (r[0]-e[0])/((double)calls*per);
}
static void sweep_print(FILE *csv,const char *nm,const char *how,const double *t,DWORD pin) {
 DWORD o,bo=0,wo=0;for(o=1;o<64;o++){if(t[o]<t[bo])bo=o;if(t[o]>t[wo])wo=o;}
 say("   %-12s %s: the driver's place %2lu: %6.1f; the best place %2lu: %6.1f (%+.1f%%); the worst %2lu: %6.1f (%+.1f%%)\n",nm,how,pin,t[pin],bo,t[bo],(t[bo]-t[pin])*100.0/t[pin],wo,t[wo],(t[wo]-t[pin])*100.0/t[pin]);
 for(o=0;o<64;o++){if(!(o&15))say("      %2lu:",o);say(" %5.1f",t[o]);if((o&15)==15)say("\n");}
 if(csv){fprintf(csv,"\"place sweep %s\",\"%s\"",nm,how);for(o=0;o<64;o++)fprintf(csv,",%.1f",t[o]);fprintf(csv,"\n");}
}
static void sweep(FILE *csv) {
 static const struct {const char *nm;kfn fn;DWORD sz,pin;int cs,per;} K[5]={{"tnk_pos",tnk_pos,KSZ_tnk_pos,KPIN_tnk_pos,0,NV},{"tnk_pos_o",tnk_pos_o,KSZ_tnk_pos_o,KPIN_tnk_pos_o,0,NV},
  {"tnk_light",tnk_light,KSZ_tnk_light,KPIN_tnk_light,1,4},{"tnk_light_k",tnk_light_k,KSZ_tnk_light_k,KPIN_tnk_light_k,3,4},{"tnk_plight",tnk_plight,KSZ_tnk_plight,KPIN_tnk_plight,6,4}};
 DWORD k,o;double t[64];
 swx=(uint8_t*)VirtualAlloc(NULL,65536,MEM_COMMIT,PAGE_EXECUTE_READWRITE);
 if(!swx){say("\nthe place sweep: no executable memory\n");return;}
 say("\na kernel's place within a 64-byte line (its first byte's address mod 64): cycles a vertex in cache at each of the 64 places\n");
 SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
 for(k=0;k<5;k++){for(o=0;o<64;o++){uint8_t *at=swx+4096+o;memset(swx+4096,0xCC,8192);memcpy(at,(const void*)K[k].fn,K[k].sz);FlushInstructionCache(GetCurrentProcess(),swx,65536);t[o]=sweep_one((kfn)(void*)at,K[k].cs,K[k].per);}
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);sweep_print(csv,K[k].nm,"in cache",t,K[k].pin);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);}
 SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
}
#define BIGN 200000UL
/* v1.7 (.749B): where the position kernel's cycles go. Each kernel beside five cut-down copies of itself (src/vx_kbench_abl.asm, made by
 * tools/kb_ablate.py; WRONG RESULTS ON PURPOSE, this tool only): d the divide made a multiply; a the four clip-array stores gone; r the
 * four record stores gone; ar both; dar all three. Timed in cache (64 vertices) and cold (200,000 places). A part's share is at most
 * the whole kernel's time less the copy's without it: on an in-order processor removing one part can uncover another's wait. */
extern void __cdecl tnk_abl_c7_d(tnk_frame *f);extern void __cdecl tnk_abl_c7_a(tnk_frame *f);extern void __cdecl tnk_abl_c7_r(tnk_frame *f);extern void __cdecl tnk_abl_c7_ar(tnk_frame *f);extern void __cdecl tnk_abl_c7_dar(tnk_frame *f);
extern void __cdecl tnk_abl_o_d(tnk_frame *f);extern void __cdecl tnk_abl_o_a(tnk_frame *f);extern void __cdecl tnk_abl_o_r(tnk_frame *f);extern void __cdecl tnk_abl_o_ar(tnk_frame *f);extern void __cdecl tnk_abl_o_dar(tnk_frame *f);
static void big_test(FILE *csv,int c7) {
 static const char *const vn[6]={"whole","no divide (a multiply)","no clip-array stores","no record stores","no stores at all","no divide, no stores"};
 static const char *const kn[2]={"tnk_pos (C7 family: pair, divgap)","tnk_pos_o (others: pair, imask)"};
 float *a=(float*)VirtualAlloc(NULL,(BIGN+64)*(8*4+1+32)+65536,MEM_COMMIT,PAGE_READWRITE);DWORD k,v,i;static float st[3*264];
 kfn fn[2][6];double hot[2][6],cold[2][6];
 fn[0][0]=tnk_pos;fn[0][1]=tnk_abl_c7_d;fn[0][2]=tnk_abl_c7_a;fn[0][3]=tnk_abl_c7_r;fn[0][4]=tnk_abl_c7_ar;fn[0][5]=tnk_abl_c7_dar;
 fn[1][0]=tnk_pos_o;fn[1][1]=tnk_abl_o_d;fn[1][2]=tnk_abl_o_a;fn[1][3]=tnk_abl_o_r;fn[1][4]=tnk_abl_o_ar;fn[1][5]=tnk_abl_o_dar;
 if(!a){say("\nthe cut-down kernels: no memory\n");return;}
 SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
 set_case(F0,0); /* in cache: 64 vertices a call, as the first case above */
 for(k=0;k<2;k++)for(v=0;v<6;v++){double r[41],e[41];DWORD j;
  for(j=0;j<41;j++){*F=*F0;e[j]=run(nothing,200);*F=*F0;r[j]=run(fn[k][v],200);}
  qsort(r,41,sizeof(double),cmpd);qsort(e,41,sizeof(double),cmpd);hot[k][v]=(r[0]-e[0])/(200.0*NV);}
 set_case(F0,0); /* cold: the driver's call size from one staging chunk, every output place written once a run */
 {float *p=a;p+=3*(BIGN+64);F0->sx=st;F0->sy=st+264;F0->sz=st+528;F0->n=256;F0->cx=p;p+=BIGN+64;F0->cy=p;p+=BIGN+64;F0->cz=p;p+=BIGN+64;F0->cw=p;p+=BIGN+64;F0->rw=p;p+=BIGN+64;
  F0->cc=(uint8_t*)p;F0->rec=(uint8_t*)p+((BIGN+64+15)&~15UL);}
 for(i=0;i<264;i++){st[i]=sx[i&63];st[264+i]=sy[i&63];st[528+i]=sz[i&63];}
 for(k=0;k<2;k++)for(v=0;v<6;v++){double r[7];DWORD j;
  for(j=0;j<7;j++){double t0;*F=*F0;t0=now();for(i=0;i+256<=BIGN;i+=256){F->first=i;fn[k][v](F);}r[j]=now()-t0;}
  qsort(r,7,sizeof(double),cmpd);cold[k][v]=r[0]/(double)(BIGN/256*256);}
 SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
 say("\nwhere the position kernel's cycles go: cycles a vertex, in cache (64 vertices a call) and cold (200,000 places a run, 256 a call,\nthe clip arrays and 6.4 MB of records written once); the cut-down copies give wrong results on purpose\n");
 for(k=0;k<2;k++){say("   %s%s\n",kn[k],(c7?k==0:k==1)?"  <- the driver's":"");
  for(v=0;v<6;v++){say("      %-26s in cache %7.1f",vn[v],hot[k][v]);if(v)say(" (%+6.1f)",hot[k][v]-hot[k][0]);else say("         ");say("   cold %7.1f",cold[k][v]);if(v)say(" (%+6.1f)",cold[k][v]-cold[k][0]);say("\n");
   if(csv)fprintf(csv,"\"%s\",\"%s\",%.2f,%.2f,cut\n",kn[k],vn[v],hot[k][v],cold[k][v]);}}
 say("   cold less in cache, whole kernel: what the memory costs. The row without stores still reads the staging chunk only.\n");
 if(swx){double t[64];DWORD o;const kfn dk=c7?tnk_pos:tnk_pos_o;const DWORD sz=c7?KSZ_tnk_pos:KSZ_tnk_pos_o,pin=c7?KPIN_tnk_pos:KPIN_tnk_pos_o;
  SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
  for(o=0;o<64;o++){uint8_t *at=swx+4096+o;double r[3];DWORD j;memset(swx+4096,0xCC,8192);memcpy(at,(const void*)dk,sz);FlushInstructionCache(GetCurrentProcess(),swx,65536);
   for(j=0;j<3;j++){double t0;*F=*F0;t0=now();for(i=0;i+256<=BIGN;i+=256){F->first=i;((kfn)(void*)at)(F);}r[j]=now()-t0;}
   qsort(r,3,sizeof(double),cmpd);t[o]=r[0]/(double)(BIGN/256*256);}
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
  say("\nthe driver's position kernel at each place, cold (200,000 places a run):\n");sweep_print(csv,c7?"tnk_pos":"tnk_pos_o","cold",t,pin);}
 /* v1.9 (.751): the outputs' alignment. T748 -> T749 (VX900): the clip arrays and the records went from 16-byte aligned to 8 off and
  * tnk_pos_o's time in the frame from 1250 to 1860 ms. The driver's kernel cold with every output array 0, 4, 8 and 12 bytes off a 16-byte line. */
 {const kfn dk=c7?tnk_pos:tnk_pos_o;DWORD off;float *bx=F0->cx,*by=F0->cy,*bz=F0->cz,*bw=F0->cw,*br=F0->rw;uint8_t *bc=F0->cc,*bb=F0->rec;
  say("\nthe driver's position kernel cold, its output arrays (clip x, y, z, w, 1/w, records) moved off a 16-byte line: cycles a vertex\n");
  SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
  for(off=0;off<16;off+=4){double r[5];DWORD j;
   F0->cx=(float*)(void*)((uint8_t*)bx+off);F0->cy=(float*)(void*)((uint8_t*)by+off);F0->cz=(float*)(void*)((uint8_t*)bz+off);F0->cw=(float*)(void*)((uint8_t*)bw+off);F0->rw=(float*)(void*)((uint8_t*)br+off);F0->cc=bc;F0->rec=bb+off;
   for(j=0;j<5;j++){double t0;*F=*F0;t0=now();for(i=0;i+256<=BIGN-256;i+=256){F->first=i;dk(F);}r[j]=now()-t0;}
   qsort(r,5,sizeof(double),cmpd);say("   %2lu bytes off: %7.1f\n",off,r[0]/(double)((BIGN-256)/256*256));
   if(csv)fprintf(csv,"\"position kernel cold, outputs off a 16-byte line\",\"%lu bytes\",%.2f,%.2f,same\n",off,r[0]/(double)((BIGN-256)/256*256),0.0);}
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);}
 VirtualFree(a,0,MEM_RELEASE);
}
static DWORD sig_of(void) {
 return sum(F->od,16)^sum(F->os,16)^sum(F->base,sizeof(F->base))^sum(F->sacc,sizeof(F->sacc))^sum(F->vacc,16)^sum(cc,sizeof(cc))^sum(cx,sizeof(cx))^sum(cy,sizeof(cy))^sum(cz,sizeof(cz))^sum(cw,sizeof(cw))^sum(rw,sizeof(rw))^sum(rec,sizeof(rec));
}
static double R_best[NVAR],R_mid[NVAR];static DWORD R_sig[NVAR];static int R_ord[NVAR];
static int cmpo(const void *a,const void *b) {double x=R_best[*(const int*)a],y=R_best[*(const int*)b];return x<y?-1:x>y;}
int main(int argc,char **argv) {
 DWORD v[3],a,k,c,in=0xFFFFFE34UL,x[4]={0,0,0,0},got=0;HANDLE h;char ven[13];FILE *csv;int c7;
 lf=fopen("VXKBENCH.LOG","w");
 say("VXKBENCH v1.9 (driver .751): the driver's T&L kernels timed; each kernel at all 64 places within a line; the position kernel cut down, in cache and cold, on this processor\n");
 cpu0(v);memcpy(ven,&v[0],4);memcpy(ven+4,&v[1],4);memcpy(ven+8,&v[2],4);ven[12]=0;a=cpu1();
 c7=v[0]==0x746E6543UL&&((a>>8)&15)==6&&((a>>4)&15)<15&&!((a>>16)&15);
 say("processor: %s, family %lu, model %lu, stepping %lu (CPUID 1 EAX %08lX): the driver runs %s on it\n",ven,(a>>8)&15,((a>>4)&15)|((a>>12)&0xF0),a&15,a,c7?"tnk_pos and tnk_light":"tnk_pos_o, and tnk_light for one light without fog (tnk_light_h otherwise)");
 if(argc>1&&(argv[1][0]=='C'||argv[1][0]=='c')){ /* v1.3: VXKBENCH CPU: VXOOO.TXT made when the processor is outside the C7 family (QUICK.BAT runs the position kernel A/B only there) */
  remove("VXOOO.TXT");if(!c7){FILE *o=fopen("VXOOO.TXT","w");if(o){fputs("tnk_pos_o\n",o);fclose(o);}}if(lf)fclose(lf);remove("VXKBENCH.LOG");return 0;}
 h=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(h==INVALID_HANDLE_VALUE||!DeviceIoControl(h,0x85500012UL,&in,4,x,16,&got,NULL)||got!=16||x[1]!=0x31455353UL||(x[2]&0x07000000UL)!=0x07000000UL||!(x[0]&0x200UL)){
  say("FAILED: the driver's VxD is not there, or the processor or the system does not allow SSE2: nothing timed\n");if(lf)fclose(lf);return 2;}
 CloseHandle(h);
 F=(tnk_frame*)(void*)(fmem[0]+((16-((size_t)fmem[0]&15))&15));F0=(tnk_frame*)(void*)(fmem[1]+((16-((size_t)fmem[1]&15))&15));
 for(k=0;k<NV+8;k++){sx[k]=frnd(-1.5f,1.5f);sy[k]=frnd(-1.5f,1.5f);sz[k]=frnd(-1.5f,1.5f);}
 csv=fopen("VXKBENCH.CSV","w");if(csv)fprintf(csv,"processor,%s,%08lX\ncase,kernel,least,middle,output\n",ven,a);
 say("cycles a vertex: the least of 41 runs (the middle run), the call's own cost taken off; fastest first; the percentage is against the\ndriver's kernel on this processor (below 0: faster). A candidate whose output differs from the driver's is marked and not ranked.\n");
 for(c=0;c<NCASE;c++){
  const DWORD per=(DWORD)C[c].per,calls=per>=NV?200:2000;int n=0,ref=-1;DWORD j,i;double rb;
  set_case(F0,C[c].cs); /* v1.2: once a case, so every row gets the same frame (v1.1 built it per row: the random normals and positions differed, and the rows that read them were marked OUTPUT DIFFERS wrongly) */
  SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
  for(k=0;k<NVAR;k++){double r[41],e[41];
   if(V[k].fam!=C[c].fam)continue;
   memset(cx,0,sizeof(cx));memset(cy,0,sizeof(cy));memset(cz,0,sizeof(cz));memset(cw,0,sizeof(cw));memset(rw,0,sizeof(rw));memset(cc,0,sizeof(cc));memset(rec,0,sizeof(rec));memset(vcb,0,sizeof(vcb));
   *F=*F0;V[k].fn(F);R_sig[k]=sig_of(); /* once: the output's check, and the cache warm */
   for(j=0;j<41;j++){*F=*F0;e[j]=run(nothing,calls);*F=*F0;r[j]=run(V[k].fn,calls);}
   qsort(r,41,sizeof(double),cmpd);qsort(e,41,sizeof(double),cmpd);
   R_best[k]=(r[0]-e[0])/((double)calls*per);R_mid[k]=(r[20]-e[20])/((double)calls*per);R_ord[n++]=(int)k;
   if(ref<0&&(C[c].fam!=FAM_POS||(c7?V[k].fn==tnk_pos:V[k].fn==tnk_pos_o)))ref=(int)k;
   Sleep(0);}
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
  qsort(R_ord,n,sizeof(int),cmpo);rb=R_best[ref];
  say("\n%s:\n",C[c].what);
  for(i=0;i<(DWORD)n;i++){int q=R_ord[i];const int same=R_sig[q]==R_sig[ref];
   say("   %-34s %8.1f (%8.1f)  %+6.1f%%%s%s\n",V[q].name,R_best[q],R_mid[q],rb>0?(R_best[q]-rb)*100.0/rb:0.0,q==ref?"  <- the driver's":"",same?"":"   OUTPUT DIFFERS: not a candidate");
   if(csv)fprintf(csv,"\"%s\",\"%s\",%.2f,%.2f,%s\n",C[c].what,V[q].name,R_best[q],R_mid[q],same?"same":"DIFFERS");}
 }
 sweep(csv);
 big_test(csv,c7);
 if(csv)fclose(csv);
 say("\ndone. Send VXKBENCH.LOG (and VXKBENCH.CSV).\n");if(lf)fclose(lf);return 0;
}
