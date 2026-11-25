/* test_kbench_frame.c (.768): VXKBENCH v2.0's rows run once on the host with the board's kernel bytes (tests/tnl_kernels.s, and
 * tests/kbench_old.s: .767's two position kernels, which the tool keeps for comparison), on the tool's own frames
 * (src/vx_kbench_frame.inc). The tool cannot be run here (Win32, the VxD), so this is what shows that no row faults (the tnq_ kernels
 * use aligned operands) and that rows timed against each other do the same job:
 *  - the staged position rows (tnq_gpos on R1's frame, tnk_pos and tnk_pos_o on .767's): every vertex inside the screen with code 0, and
 *    the same window position (x and y within 1/64 of a pixel, z and 1/w within 2 parts in a million: the formulas differ in rounding);
 *  - tnq_pos over the same positions as 32-byte vertices: the words tnq_gpos wrote, exactly, and "every vertex inside" (qacc 0);
 *  - the guard-band case sets the viewport flags' array without touching it (vacc only), on both frames;
 *  - tnq_lit4 and tnk_light light the same lanes to the same colour bytes within 1, for each of the tool's four directional cases.
 * 32-bit, freestanding (no libc), as tests/test_tnq_asm.c:
 *   clang -m32 --target=i686-linux-gnu -O2 -msse2 -nostdlib -static -ffreestanding -fno-stack-protector -fno-pic -no-pie \
 *     -Wno-gnu-folding-constant -Wa,--noexecstack -Isrc tests/test_kbench_frame.c tests/tnl_kernels.s tests/kbench_old.s -o tests/test_kbench_frame */
typedef unsigned int uint32_t;typedef unsigned char uint8_t;typedef unsigned short uint16_t;typedef unsigned int size_t;
#define offsetof(T,f) ((size_t)&((T*)0)->f)
void *memset(void *d,int c,size_t n){uint8_t *p=d;while(n--)*p++=(uint8_t)c;return d;}
void *memcpy(void *d,const void *s,size_t n){uint8_t *p=d;const uint8_t *q=s;while(n--)*p++=*q++;return d;}
#include "tnl_kernels_c.inc" /* the kernels' C, for its frame (the tool takes the frame alone from src/vx_d3d_tnl_soa.inc); the rows below are the board's bytes (_b) */
#include "../src/vx_kbench_frame.inc"
extern void tnq_gpos_b(tnk_frame *f) __asm__("_tnq_gpos");
extern void tnq_pos_b(tnk_frame *f) __asm__("_tnq_pos");
extern void tnq_lit4_b(tnk_frame *f) __asm__("_tnq_lit4");
extern void tnk_light_b(tnk_frame *f) __asm__("_tnk_light");
extern void tnk_old_pos_b(tnk_frame *f) __asm__("_tnk_old_pos");
extern void tnk_old_pos_o_b(tnk_frame *f) __asm__("_tnk_old_pos_o");
static long sys_write(int fd,const char *s,unsigned n){long r;__asm__ volatile("int $0x80":"=a"(r):"a"(4),"b"(fd),"c"(s),"d"(n));return r;}
static void sys_exit(int c){__asm__ volatile("int $0x80"::"a"(1),"b"(c));for(;;);}
static void out(const char *s){unsigned n=0;while(s[n])n++;sys_write(1,s,n);}
static void outn(unsigned v){char b[12];int i=0;if(!v)b[i++]='0';while(v){b[i++]=(char)('0'+v%10);v/=10;}while(i){char c[2];c[0]=b[--i];c[1]=0;out(c);}}
static uint8_t blk[KB_BLOCK+64] __attribute__((aligned(64)));
static tnk_frame F __attribute__((aligned(64)));
static float px[3][NV][4];static uint8_t code[3][NV];
static float fab(float x){return x<0?-x:x;}
__attribute__((force_align_arg_pointer)) void _start(void){
 unsigned bad=0,k,i,row;
 static void (*const posfn[3])(tnk_frame*)={tnq_gpos_b,tnk_old_pos_b,tnk_old_pos_o_b};static const char *const posnm[3]={"tnq_gpos","tnk_pos (.767)","tnk_pos_o (.767)"};
 static const int lc[4]={1,16,17,2};
 kb_arrays(blk);
 /* the staged position rows: cases 0 (64 a call), 9 (4 a call), 10 (the viewport flags on) */
 for(row=0;row<3;row++){
  set_case(&F,0,row!=0);memset(cc,0xEE,128);memset(rec,0,KB_REC);memset(vcb,0xEE,128);posfn[row](&F);
  for(i=0;i<NV;i++){const float *r=(const float*)(const void*)(rec+i*32);code[row][i]=cc[i];for(k=0;k<4;k++)px[row][i][k]=r[k];
   if(cc[i]){out(posnm[row]);out(": a vertex of the bench is not inside every plane, place ");outn(i);out("\n");bad++;break;}
   if(!(r[0]>=0.0f&&r[0]<=640.0f&&r[1]>=0.0f&&r[1]<=480.0f&&r[2]>=0.0f&&r[2]<=1.0f&&r[3]>0.0f)){out(posnm[row]);out(": a position outside the window, place ");outn(i);out("\n");bad++;break;}}
  for(i=NV;i<NV+8;i++)if(cc[i]!=0xEE){out(posnm[row]);out(": wrote codes past its places\n");bad++;break;}
  for(i=0;i<128;i++)if(vcb[i]!=0xEE){out(posnm[row]);out(": wrote viewport codes with none asked for\n");bad++;break;}
  if(row)for(i=0;i<NV;i++){const float *a=px[0][i],*b=px[row][i];
   if(fab(a[0]-b[0])>1.0f/64||fab(a[1]-b[1])>1.0f/64||fab(a[2]-b[2])>2.0e-6f||fab(a[3]-b[3])>2.0e-6f*fab(a[3])){out(posnm[row]);out(": not the picture tnq_gpos draws, place ");outn(i);out("\n");bad++;break;}}
  set_case(&F,9,row!=0);memset(rec,0,KB_REC);posfn[row](&F);
  for(i=0;i<4;i++){const float *r=(const float*)(const void*)(rec+i*32);if(r[0]!=px[row][i][0]||r[1]!=px[row][i][1]||r[2]!=px[row][i][2]||r[3]!=px[row][i][3]){out(posnm[row]);out(": 4 vertices a call differ from 64\n");bad++;break;}}
  for(i=4;i<8;i++){const uint32_t *r=(const uint32_t*)(const void*)(rec+i*32);if(r[0]|r[1]|r[2]|r[3]){out(posnm[row]);out(": 4 vertices a call wrote a fifth\n");bad++;break;}}
  set_case(&F,10,row!=0);memset(vcb,0xEE,128);posfn[row](&F);
  for(i=0;i<128;i++)if(vcb[i]!=0xEE){out(posnm[row]);out(": the flags case wrote the viewport codes (the codes' kernel does)\n");bad++;break;}
 }
 /* the range row: tnq_gpos's words for the same positions, and inside */
 set_case(&F,18,0);memset(rec,0,KB_REC);F.qacc[0]=F.qacc[1]=F.qacc[2]=F.qacc[3]=0;tnq_pos_b(&F);
 for(i=0;i<NV;i++){const float *r=(const float*)(const void*)(rec+i*32);
  if(r[0]!=px[0][i][0]||r[1]!=px[0][i][1]||r[2]!=px[0][i][2]||r[3]!=px[0][i][3]){out("tnq_pos: not tnq_gpos's words, place ");outn(i);out("\n");bad++;break;}}
 if(F.qacc[0]|F.qacc[1]|F.qacc[2]|F.qacc[3]){out("tnq_pos: the bench's range is not wholly inside by its test\n");bad++;}
 /* the directional light rows */
 for(k=0;k<4;k++){uint32_t a[4],b[4];unsigned j,q;
  set_case(&F,lc[k],0);tnq_lit4_b(&F);for(j=0;j<4;j++)a[j]=F.od[j];
  set_case(&F,lc[k],0);tnk_light_b(&F);for(j=0;j<4;j++)b[j]=F.od[j];
  for(j=0;j<4;j++)for(q=0;q<4;q++){const int d=(int)((a[j]>>(8*q))&255)-(int)((b[j]>>(8*q))&255);
   if(d<-1||d>1){out("tnq_lit4 and tnk_light: colours apart, light case ");outn((unsigned)lc[k]);out(" lane ");outn(j);out("\n");bad++;q=4;j=4;}}}
 out("VXKBENCH's rows on the host: ");outn(bad);out(" wrong\n");
 sys_exit(bad?1:0);
}
