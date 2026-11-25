/* VXKBENCH v2.0 (driver .768, R1: the T&L reworked): the driver's position kernels are tnq_gpos (staged places, with clip codes) and tnq_pos (a vertex
 * range read in place into records, no codes): timed beside the two kernels they replace (tnk_pos and tnk_pos_o of .746-.767, kept for this tool
 * only: src/vx_kbench_old.asm), each on the frame its build of the driver makes (R1: the window folded into the matrix and 1/w by estimate and
 * two Newton steps; .767: clip coordinates and a divide), the same picture; tnq_lit4 (the lights in the mesh's own space) beside tnk_light;
 * cold, the staged kernels, the driver's gather with tnq_gpos, and tnq_pos straight from the vertices. The cut-down copies of the old kernels
 * (v1.7) and the outputs moved off a 16-byte line (v1.9) are gone: T749B closed the first, and R1's kernels store the clip arrays with aligned
 * operands. The frames are src/vx_kbench_frame.inc (tests/test_kbench_frame.c runs every row on the host).
 * VXKBENCH v1.9 (driver .751): the position kernel cold with its outputs 0-12 bytes off a 16-byte line. VXKBENCH v1.8 (driver .750): sweep, a kernel's place within a 64-byte line. VXKBENCH v1.7 (.749B, the tool only): the position kernels beside five cut-down copies each (divide, clip-array stores, record stores).
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
extern void __cdecl tnq_gpos(tnk_frame *f);
extern void __cdecl tnq_pos(tnk_frame *f);
extern void __cdecl tnq_lit4(tnk_frame *f);
extern void __cdecl tnk_old_pos(tnk_frame *f);   /* VXKBENCH only (src/vx_kbench_old.asm): .746-.767's tnk_pos, the C7 family's */
extern void __cdecl tnk_old_pos_o(tnk_frame *f); /* and tnk_pos_o, every other processor's */
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
#include "vx_kbench_frame.inc" /* v2.0: NV, the arrays, base_frame, set_case */
static uint8_t fmem[2][sizeof(tnk_frame)+16];static tnk_frame *F,*F0;
extern void __cdecl tnk_splight(tnk_frame *f);
typedef void (__cdecl *kfn)(tnk_frame *);
enum {FAM_POS,FAM_RANGE,FAM_LIGHT,FAM_LIGHT_K,FAM_PLIGHT,FAM_SPLIGHT,FAM_N};
/* old: the row runs on the frame .767 made (set_case's old); only: the cases the kernel serves, by case number's bit (0: all of its family's).
 * The first row of a family that serves a case is the driver's kernel for it. */
typedef struct {int fam;const char *name;kfn fn;int old;DWORD only;} variant;
extern void __cdecl tnk_light_h(tnk_frame *f);
#define QL_CASES ((1UL<<1)|(1UL<<16)|(1UL<<17)|(1UL<<2)) /* tnq_lit's lighting: directional lights alone, no fog (tn_soa_prepare_b) */
static const variant V[]={
 {FAM_POS,"tnq_gpos (R1: 1/w by estimate)",tnq_gpos,0,0},{FAM_POS,"tnk_pos (.767, C7 family: divide)",tnk_old_pos,1,0},{FAM_POS,"tnk_pos_o (.767, others: divide)",tnk_old_pos_o,1,0},
 {FAM_RANGE,"tnq_pos (R1: a range in place)",tnq_pos,0,0},
 {FAM_LIGHT,"tnq_lit4 (R1: mesh-space lights)",tnq_lit4,0,QL_CASES},{FAM_LIGHT,"tnk_light (pack4)",tnk_light,0,0},{FAM_LIGHT,"tnk_light_h (.742; Nano: normalized)",tnk_light_h,0,0},{FAM_LIGHT_K,"tnk_light_k (pack4)",tnk_light_k,0,0},
 {FAM_PLIGHT,"tnk_plight",tnk_plight,0,0},{FAM_SPLIGHT,"tnk_splight (hand-written, .748)",tnk_splight,0,0},
};
#define NVAR (sizeof(V)/sizeof(V[0]))
typedef struct {int fam,cs,per;const char *what;} kcase;
static const kcase C[]={
 {FAM_POS,0,NV,"positions and clip codes, 64 staged vertices a call"},{FAM_POS,9,4,"positions and clip codes, 4 staged vertices a call"},{FAM_POS,10,NV,"positions, 64 staged vertices, the guard band's viewport flags on"},
 {FAM_RANGE,18,NV,"positions of a vertex range, 64 vertices a call read in place (no clip codes; compare with the staged rows above)"},
 {FAM_LIGHT,1,4,"one directional light"},{FAM_LIGHT,16,4,"one directional light, normals normalized"},{FAM_LIGHT,17,4,"two directional lights"},{FAM_LIGHT,2,4,"two directional lights, normals normalized"},{FAM_LIGHT,11,4,"one directional light, exponential fog"},
 {FAM_LIGHT_K,3,4,"one directional light, normals given"},{FAM_LIGHT_K,12,4,"two directional lights, normals given"},{FAM_LIGHT_K,13,4,"one directional light, normals given, exponential fog"},
 {FAM_PLIGHT,4,4,"one point light in range"},{FAM_PLIGHT,5,4,"one point light out of range"},{FAM_PLIGHT,6,4,"four point lights in range"},{FAM_PLIGHT,7,4,"four point lights, two out of range"},
 {FAM_SPLIGHT,14,4,"two point lights, one a spot light"},{FAM_SPLIGHT,15,4,"two point lights, one a spot light, specular (power 20)"}};
#define NCASE (sizeof(C)/sizeof(C[0]))
static int serves(const variant *v,const kcase *c) {return v->fam==c->fam&&(!v->only||(c->cs<32&&(v->only&(1UL<<c->cs))));}
static int cmpd(const void *a,const void *b) {double x=*(const double*)a,y=*(const double*)b;return x<y?-1:x>y;}
/* a run: calls times the kernel, the flags a kernel sets put back before each call (the same three stores for every variant) */
static double run(kfn fn,DWORD calls) {
 const DWORD nr=F0->nready,er=F0->eready,sd=F0->sdone;double t0,t1;DWORD k;
 t0=now();for(k=0;k<calls;k++){F->nready=nr;F->eready=er;F->sdone=sd;fn(F);}t1=now();
 return t1-t0;
}
static void __cdecl nothing(tnk_frame *f) {(void)f;}
static DWORD sum(const void *p,DWORD n) {const uint8_t *q=p;DWORD s=0,k;for(k=0;k<n;k++)s=s*31+q[k];return s;}
/* v1.8 (.750): a kernel's place within a 64-byte line. T749, VX900: tnk_pos_o took 49% longer in the frame after other code moved it from
 * 37 to 11 within its line. Each kernel's bytes (position-independent) are copied to all 64 places in executable memory and timed there. */
#include "vx_tnl_sizes.inc"
static uint8_t *swx;
static double sweep_one(kfn at,int cs,int per) {
 double r[9],e[9];DWORD j;const DWORD calls=per>=NV?100:1000;
 set_case(F0,cs,0);
 for(j=0;j<9;j++){*F=*F0;e[j]=run(nothing,calls);*F=*F0;r[j]=run(at,calls);}
 qsort(r,9,sizeof(double),cmpd);qsort(e,9,sizeof(double),cmpd);return (r[0]-e[0])/((double)calls*per);
}
static void sweep_print(FILE *csv,const char *nm,const char *how,const double *t,DWORD pin) {
 DWORD o,bo=0,wo=0;for(o=1;o<64;o++){if(t[o]<t[bo])bo=o;if(t[o]>t[wo])wo=o;}
 say("   %-12s %s: the driver's place %2lu: %6.1f; the best place %2lu: %6.1f (%+.1f%%); the worst %2lu: %6.1f (%+.1f%%)\n",nm,how,pin,t[pin],bo,t[bo],(t[bo]-t[pin])*100.0/t[pin],wo,t[wo],(t[wo]-t[pin])*100.0/t[pin]);
 for(o=0;o<64;o++){if(!(o&15))say("      %2lu:",o);say(" %5.1f",t[o]);if((o&15)==15)say("\n");}
 if(csv){fprintf(csv,"\"place sweep %s\",\"%s\"",nm,how);for(o=0;o<64;o++)fprintf(csv,",%.1f",t[o]);fprintf(csv,"\n");}
}
#define NSWEEP 6
static void sweep(FILE *csv) {
 static const struct {const char *nm;kfn fn;DWORD sz,pin;int cs,per;} K[NSWEEP]={{"tnq_gpos",tnq_gpos,KSZ_tnq_gpos,KPIN_tnq_gpos,0,NV},{"tnq_pos",tnq_pos,KSZ_tnq_pos,KPIN_tnq_pos,18,NV},{"tnq_lit4",tnq_lit4,KSZ_tnq_lit4,KPIN_tnq_lit4,1,4},
  {"tnk_light",tnk_light,KSZ_tnk_light,KPIN_tnk_light,1,4},{"tnk_light_k",tnk_light_k,KSZ_tnk_light_k,KPIN_tnk_light_k,3,4},{"tnk_plight",tnk_plight,KSZ_tnk_plight,KPIN_tnk_plight,6,4}};
 DWORD k,o;double t[64];
 swx=(uint8_t*)VirtualAlloc(NULL,65536,MEM_COMMIT,PAGE_EXECUTE_READWRITE);
 if(!swx){say("\nthe place sweep: no executable memory\n");return;}
 say("\na kernel's place within a 64-byte line (its first byte's address mod 64): cycles a vertex in cache at each of the 64 places\n");
 SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
 for(k=0;k<NSWEEP;k++){for(o=0;o<64;o++){uint8_t *at=swx+4096+o;memset(swx+4096,0xCC,8192);memcpy(at,(const void*)K[k].fn,K[k].sz);FlushInstructionCache(GetCurrentProcess(),swx,65536);t[o]=sweep_one((kfn)(void*)at,K[k].cs,K[k].per);}
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);sweep_print(csv,K[k].nm,"in cache",t,K[k].pin);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);}
 SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
}
#define BIGN 200000UL
#define BIGC 256UL /* the driver's call size (TN_CH) */
/* v2.0 (.768): the position kernels as a frame feeds them: 200,000 places walked once a run, so every output (the clip arrays, the codes,
 * 6.4 MB of records) is written cold and, for the last two rows, 6.4 MB of 32-byte vertices are read cold. Rows: the driver's staged
 * kernel and the two it replaced, 256 places a call from one staging chunk (v1.6's case); the driver's gather (three floats a vertex into
 * the staging lanes, as tn_draw's loop does) with tnq_gpos; tnq_pos straight from the vertices (R1's range path: no gather, no clip
 * arrays, no codes). T748: a kernel 20-29% faster in the in-cache bench made the frame 2% slower: this is what the memory costs. */
static void big_test(FILE *csv,int c7) {
 static const char *const rn[5]={"tnq_gpos, staged (R1)","tnk_pos, staged (.767, C7 family)","tnk_pos_o, staged (.767, others)","the gather + tnq_gpos (R1, a draw the range path leaves)","tnq_pos from the vertices (R1, the range path)"};
 const DWORD fl=(BIGN+64)*4; /* a float array's bytes: a multiple of 16 */
 uint8_t *a=(uint8_t*)VirtualAlloc(NULL,5*fl+(BIGN+64)+16+(BIGN+64)*32+65536,MEM_COMMIT,PAGE_READWRITE),*vb=(uint8_t*)VirtualAlloc(NULL,(BIGN+64)*32+4096,MEM_COMMIT,PAGE_READWRITE);
 float *st;DWORD k,i,q;double cold[5];uint8_t *recb;
 if(!a||!vb){say("\nthe cold runs: no memory\n");if(a)VirtualFree(a,0,MEM_RELEASE);if(vb)VirtualFree(vb,0,MEM_RELEASE);return;}
 st=(float*)(void*)a; /* the staging chunk: three lanes of 264 (1,056 bytes each: on 16), at the block's start (a page) */
 for(i=0;i<264;i++){st[i]=sx[i&63];st[264+i]=sy[i&63];st[528+i]=sz[i&63];}
 for(i=0;i<BIGN+64;i++){float *v=(float*)(void*)(vb+i*32);v[0]=sx[i&63];v[1]=sy[i&63];v[2]=sz[i&63];v[3]=1.0f;v[4]=0.0f;v[5]=0.0f;v[6]=0.0f;v[7]=0.0f;}
 SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
 for(k=0;k<5;k++){double r[7];DWORD j;const int old=k==1||k==2;const kfn fn=k==1?tnk_old_pos:k==2?tnk_old_pos_o:tnq_gpos;
  set_case(F0,0,old);
  {uint8_t *p=a+4096;F0->sx=st;F0->sy=st+264;F0->sz=st+528;F0->n=BIGC;F0->cx=(float*)(void*)p;p+=fl;F0->cy=(float*)(void*)p;p+=fl;F0->cz=(float*)(void*)p;p+=fl;F0->cw=(float*)(void*)p;p+=fl;F0->rw=(float*)(void*)p;p+=fl;
   F0->cc=p;p+=(BIGN+64+15)&~15UL;F0->rec=p;recb=p;}
  F0->qsstride=32;F0->qdstride=32;F0->qn=BIGC;
  for(j=0;j<7;j++){double t0;*F=*F0;t0=now();
   if(k<3)for(i=0;i+BIGC<=BIGN;i+=BIGC){F->first=i;fn(F);}
   else if(k==3)for(i=0;i+BIGC<=BIGN;i+=BIGC){const float *v=(const float*)(const void*)(vb+i*32);for(q=0;q<BIGC;q++,v+=8){st[q]=v[0];st[264+q]=v[1];st[528+q]=v[2];}F->first=i;tnq_gpos(F);}
   else for(i=0;i+BIGC<=BIGN;i+=BIGC){F->qsrc=vb+i*32;F->qdst=recb+i*32;tnq_pos(F);}
   r[j]=now()-t0;}
  qsort(r,7,sizeof(double),cmpd);cold[k]=r[0]/(double)(BIGN/BIGC*BIGC);}
 SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
 say("\nthe position kernels cold: cycles a vertex over 200,000 places a run, 256 a call (the clip arrays and 6.4 MB of records written once;\nthe last two rows also read 6.4 MB of 32-byte vertices once)\n");
 for(k=0;k<5;k++){say("   %-58s %7.1f",rn[k],cold[k]);if(k)say("  (%+6.1f%% against the first row)",(cold[k]-cold[0])*100.0/cold[0]);say("%s\n",k==(c7?1:2)?"  <- what .767 ran on this processor":"");
  if(csv)fprintf(csv,"\"position kernels cold\",\"%s\",%.2f,%.2f,cold\n",rn[k],cold[k],0.0);}
 say("   the fourth row against the fifth: a draw's positions through the gather and the staged kernel, and through the range path.\n");
 if(swx){double t[64];DWORD o;
  set_case(F0,0,0);
  {uint8_t *p=a+4096;F0->sx=st;F0->sy=st+264;F0->sz=st+528;F0->n=BIGC;F0->cx=(float*)(void*)p;p+=fl;F0->cy=(float*)(void*)p;p+=fl;F0->cz=(float*)(void*)p;p+=fl;F0->cw=(float*)(void*)p;p+=fl;F0->rw=(float*)(void*)p;p+=fl;F0->cc=p;p+=(BIGN+64+15)&~15UL;F0->rec=p;}
  for(i=0;i<264;i++){st[i]=sx[i&63];st[264+i]=sy[i&63];st[528+i]=sz[i&63];}
  SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
  for(o=0;o<64;o++){uint8_t *at=swx+4096+o;double r[3];DWORD j;memset(swx+4096,0xCC,8192);memcpy(at,(const void*)tnq_gpos,KSZ_tnq_gpos);FlushInstructionCache(GetCurrentProcess(),swx,65536);
   for(j=0;j<3;j++){double t0;*F=*F0;t0=now();for(i=0;i+BIGC<=BIGN;i+=BIGC){F->first=i;((kfn)(void*)at)(F);}r[j]=now()-t0;}
   qsort(r,3,sizeof(double),cmpd);t[o]=r[0]/(double)(BIGN/BIGC*BIGC);}
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
  say("\nthe driver's staged position kernel at each place, cold (200,000 places a run):\n");sweep_print(csv,"tnq_gpos","cold",t,KPIN_tnq_gpos);}
 VirtualFree(a,0,MEM_RELEASE);VirtualFree(vb,0,MEM_RELEASE);
}
static DWORD sig_of(void) {
 return sum(F->od,16)^sum(F->os,16)^sum(F->base,sizeof(F->base))^sum(F->sacc,sizeof(F->sacc))^sum(F->vacc,16)^sum(cc,NV+8)^sum(cx,(NV+8)*4)^sum(cy,(NV+8)*4)^sum(cz,(NV+8)*4)^sum(cw,(NV+8)*4)^sum(rw,(NV+8)*4)^sum(rec,KB_REC);
}
static double R_best[NVAR],R_mid[NVAR];static DWORD R_sig[NVAR];static int R_ord[NVAR];
static int cmpo(const void *a,const void *b) {double x=R_best[*(const int*)a],y=R_best[*(const int*)b];return x<y?-1:x>y;}
int main(int argc,char **argv) {
 DWORD v[3],a,k,c,in=0xFFFFFE34UL,x[4]={0,0,0,0},got=0;HANDLE h;char ven[13];FILE *csv;int c7;uint8_t *blk;
 lf=fopen("VXKBENCH.LOG","w");
 say("VXKBENCH v2.0 (driver .768): the driver's T&L kernels timed on this processor, the reworked position and light kernels beside the ones they replace; each kernel at all 64 places within a line; the position kernels cold\n");
 cpu0(v);memcpy(ven,&v[0],4);memcpy(ven+4,&v[1],4);memcpy(ven+8,&v[2],4);ven[12]=0;a=cpu1();
 c7=v[0]==0x746E6543UL&&((a>>8)&15)==6&&((a>>4)&15)<15&&!((a>>16)&15);
 say("processor: %s, family %lu, model %lu, stepping %lu (CPUID 1 EAX %08lX): the driver runs tnq_gpos and tnq_pos on it (every processor), tnq_lit for directional lights alone, and otherwise %s\n",ven,(a>>8)&15,((a>>4)&15)|((a>>12)&0xF0),a&15,a,c7?"tnk_light":"tnk_light (tnk_light_h when the normals are normalized)");
 if(argc>1&&(argv[1][0]=='C'||argv[1][0]=='c')){ /* v1.3: VXKBENCH CPU: VXOOO.TXT made when the processor is outside the C7 family */
  remove("VXOOO.TXT");if(!c7){FILE *o=fopen("VXOOO.TXT","w");if(o){fputs("not the C7 family\n",o);fclose(o);}}if(lf)fclose(lf);remove("VXKBENCH.LOG");return 0;}
 h=CreateFileA("\\\\.\\VX85598",0,0,NULL,OPEN_EXISTING,0,NULL);
 if(h==INVALID_HANDLE_VALUE||!DeviceIoControl(h,0x85500012UL,&in,4,x,16,&got,NULL)||got!=16||x[1]!=0x31455353UL||(x[2]&0x07000000UL)!=0x07000000UL||!(x[0]&0x200UL)){
  say("FAILED: the driver's VxD is not there, or the processor or the system does not allow SSE2: nothing timed\n");if(lf)fclose(lf);return 2;}
 CloseHandle(h);
 F=(tnk_frame*)(void*)(fmem[0]+((16-((size_t)fmem[0]&15))&15));F0=(tnk_frame*)(void*)(fmem[1]+((16-((size_t)fmem[1]&15))&15));
 blk=(uint8_t*)VirtualAlloc(NULL,KB_BLOCK,MEM_COMMIT,PAGE_READWRITE); /* v2.0: a page: the arrays on 64-byte lines */
 if(!blk){say("FAILED: no memory\n");if(lf)fclose(lf);return 2;}
 kb_arrays(blk);
 csv=fopen("VXKBENCH.CSV","w");if(csv)fprintf(csv,"processor,%s,%08lX\ncase,kernel,least,middle,output\n",ven,a);
 say("cycles a vertex: the least of 41 runs (the middle run), the call's own cost taken off; fastest first; the percentage is against the\ndriver's kernel (below 0: faster). A row of another build's kernel runs on that build's frame (the same picture by another formula) and is not\ncompared word for word; any other row whose output differs from the driver's is marked.\n");
 for(c=0;c<NCASE;c++){
  const DWORD per=(DWORD)C[c].per,calls=per>=NV?200:2000;int n=0,ref=-1;DWORD j,i;double rb;
  SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS);SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_HIGHEST);
  for(k=0;k<NVAR;k++){double r[41],e[41];
   if(!serves(&V[k],&C[c]))continue;
   set_case(F0,C[c].cs,V[k].old); /* v2.0: a row's own frame; the same numbers in it for every row (base_frame) */
   memset(cx,0,(NV+8)*4);memset(cy,0,(NV+8)*4);memset(cz,0,(NV+8)*4);memset(cw,0,(NV+8)*4);memset(rw,0,(NV+8)*4);memset(cc,0,NV+8);memset(rec,0,KB_REC);memset(vcb,0,NV+8);
   *F=*F0;V[k].fn(F);R_sig[k]=sig_of(); /* once: the output's check, and the cache warm */
   for(j=0;j<41;j++){*F=*F0;e[j]=run(nothing,calls);*F=*F0;r[j]=run(V[k].fn,calls);}
   qsort(r,41,sizeof(double),cmpd);qsort(e,41,sizeof(double),cmpd);
   R_best[k]=(r[0]-e[0])/((double)calls*per);R_mid[k]=(r[20]-e[20])/((double)calls*per);R_ord[n++]=(int)k;
   if(ref<0)ref=(int)k;
   Sleep(0);}
  SetThreadPriority(GetCurrentThread(),THREAD_PRIORITY_NORMAL);SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS);
  if(ref<0)continue;
  qsort(R_ord,n,sizeof(int),cmpo);rb=R_best[ref];
  say("\n%s:\n",C[c].what);
  for(i=0;i<(DWORD)n;i++){int q=R_ord[i];const int other=V[q].old!=V[ref].old||(V[q].fn==tnq_lit4)!=(V[ref].fn==tnq_lit4),same=other||R_sig[q]==R_sig[ref];
   say("   %-36s %8.1f (%8.1f)  %+6.1f%%%s%s\n",V[q].name,R_best[q],R_mid[q],rb>0?(R_best[q]-rb)*100.0/rb:0.0,q==ref?"  <- the driver's":"",same?"":"   OUTPUT DIFFERS: not a candidate");
   if(csv)fprintf(csv,"\"%s\",\"%s\",%.2f,%.2f,%s\n",C[c].what,V[q].name,R_best[q],R_mid[q],other?"other formula":same?"same":"DIFFERS");}
 }
 sweep(csv);
 big_test(csv,c7);
 if(csv)fclose(csv);
 say("\ndone. Send VXKBENCH.LOG (and VXKBENCH.CSV).\n");if(lf)fclose(lf);return 0;
}
