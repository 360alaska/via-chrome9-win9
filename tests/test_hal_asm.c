/* test_hal_asm.c (.545): the HAL's span kernels (tests/hal_kernels.s, the same bytes as src/vx_hal_sse.asm) against the C reference of
 * src/vx_d3d_span.inc (vxs_check_c, vxs_conv_c), word for word, on random spans: positions inside, on and past every edge, the guard band,
 * -0 and +0, z at 0, 1 and past it, rhw 0, negative, denormal, infinite and NaN, coordinates not finite, every layout (colour, specular
 * modes, one or two sets, one or two words a set), strides 16-64, 8- and 10-word records, spans of 0-40 vertices; vxs_max16 on 0-1,799
 * indices at odd and even addresses.
 * 32-bit, freestanding: tests/run.py builds it with clang -m32 and runs it; exit 0 when every result matches. */
typedef unsigned int uint32_t;typedef unsigned char uint8_t;typedef unsigned short uint16_t;typedef unsigned int size_t;
#define offsetof(T,f) ((size_t)&((T*)0)->f)
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
#include "hal_kernels_c.inc"
extern uint32_t vxs_check_asm(const vxs_frame *f) __asm__("_vxs_check");
extern void vxs_conv_asm(const vxs_frame *f) __asm__("_vxs_conv");
extern uint32_t vxs_max16_asm(const uint8_t *p,uint32_t n) __asm__("_vxs_max16");
static uint8_t ib[1800*2+32];
static uint32_t seed=545;
static uint32_t rnd(void){seed=seed*1664525u+1013904223u;return seed>>3;}
static long sys_write(int fd,const char *s,unsigned n){long r;__asm__ volatile("int $0x80":"=a"(r):"a"(4),"b"(fd),"c"(s),"d"(n));return r;}
static void sys_exit(int c){__asm__ volatile("int $0x80"::"a"(1),"b"(c));}
static void out(const char *s){unsigned n=0;while(s[n])n++;sys_write(1,s,n);}
static void outn(unsigned v){char b[12];int i=0;if(!v)b[i++]='0';while(v){b[i++]=(char)('0'+v%10);v/=10;}while(i){char c[2];c[0]=b[--i];c[1]=0;out(c);}}
static uint8_t vb[48*64+64];static uint32_t wa[48*10+16],wb[48*10+16];
static void k4(uint32_t *k,uint32_t v){k[0]=k[1]=k[2]=k[3]=v;}
static uint32_t pick(uint32_t lo,uint32_t hi){ /* a word near the edges and specials */
 static const uint32_t sp[]={0,0x80000000u,0x3F800000u,0x3F800001u,0xBF800000u,0x7F800000u,0xFF800000u,0x7FC00000u,0xFFC00000u,0x00000001u,0x80000001u,0x7F7FFFFFu,0xBF000000u,0xBF000001u,0xC0000000u};
 switch(rnd()%6){case 0:return sp[rnd()%15];case 1:return lo+(rnd()%3)-1;case 2:return hi+(rnd()%3)-1;case 3:return rnd()*8;default:return lo+(uint32_t)(((unsigned long long)rnd()*(hi-lo))>>29);}}
__attribute__((force_align_arg_pointer)) void _start(void){
 unsigned t,bad=0,cases=0,passed=0;
 for(t=0;t<20000;t++){
  vxs_frame f;uint32_t i,k,n=rnd()%41,st=16+4*(rnd()%13),ra,rb;
  for(i=0;i<sizeof(f)/4;i++)((uint32_t*)&f)[i]=0;
  k4(f.kbias,0x80000000u);k4(f.kgu,0xBF000000u^0x80000000u);k4(f.kz,0x3F800000u^0x80000000u);k4(f.kw,0x7F7FFFFFu^0x80000000u);k4(f.kone,1);k4(f.kneg0,0x80000000u);f.kneg0[3]=0;
  {uint32_t xl=(rnd()&1)?0:0x3F800000u+rnd()%0x01000000u,xh=0x43000000u+rnd()%0x02000000u,yl=(rnd()&1)?0:0x3F000000u+rnd()%0x01000000u,yh=0x43000000u+rnd()%0x02000000u,g=rnd()&1;
   f.xlo=xl;f.xrange=xh-xl;f.ylo=yl;f.yrange=yh-yl;f.xg=g&&!xl?0xFFFFFFFFu:0;f.yg=g&&!yl?0xFFFFFFFFu:0;
   k4(f.kxlo,f.xlo);k4(f.kxr,f.xrange^0x80000000u);k4(f.kylo,f.ylo);k4(f.kyr,f.yrange^0x80000000u);k4(f.kxg,f.xg);k4(f.kyg,f.yg);
   for(i=0;i<n;i++){uint8_t *p=vb+i*st;uint32_t w[16];
    w[0]=pick(xl,xh);w[1]=pick(yl,yh);w[2]=(rnd()%4)?(rnd()%0x3F800001u):pick(0,0x3F800000u);w[3]=(rnd()%8)?0x3E000000u+rnd()%0x02000000u:pick(0,0x7F7FFFFFu);
    if((t&3)==0&&(rnd()%3)==0){w[0]=xl+rnd()%(f.xrange+1);w[1]=yl+rnd()%(f.yrange+1);} /* mostly inside */
    for(k=4;k<16;k++)w[k]=(rnd()%5)?rnd()*8:pick(0,0x7F800000u);
    if((t&3)==1){w[0]=xl+rnd()%(f.xrange+1);w[1]=yl+rnd()%(f.yrange+1);w[2]=rnd()%0x3F800001u;w[3]=0x3E000000u+rnd()%0x02000000u;} /* all inside */
    for(k=0;k<st/4&&k<16;k++){p[k*4]=(uint8_t)w[k];p[k*4+1]=(uint8_t)(w[k]>>8);p[k*4+2]=(uint8_t)(w[k]>>16);p[k*4+3]=(uint8_t)(w[k]>>24);}}}
  f.vb=vb;f.stride=st;f.n=n;
  ra=vxs_check_c(&f);rb=vxs_check_asm(&f);cases++;
  /* the kernel's answer: the leading multiple of four, or the first failure in a full group; the C goes on vertex by vertex */
  {uint32_t want=ra;if(ra==n)want=n&~3u;else if((ra&~3u)+4>n)want=ra&~3u;if(rb!=want){out("check differs, case ");outn(t);out(": C ");outn(ra);out(" asm ");outn(rb);out(" n ");outn(n);out("\n");bad++;}}
  passed+=ra;
  {uint32_t slots=st/4;f.coff=(rnd()%3)?4*(4+rnd()%(slots>5?slots-5:1)):0;if(f.coff+4>st)f.coff=0;
   f.smode=rnd()%3;f.soff=4*(4+rnd()%(slots>5?slots-5:1));if(f.soff+4>st)f.smode=1;
   f.t0=(rnd()%4)?4*(4+rnd()%(slots>6?slots-6:1)):0;if(f.t0+8>st)f.t0=0;f.v0on=rnd()&1;
   f.two=rnd()&1;f.t1=(rnd()%3)?4*(4+rnd()%(slots>6?slots-6:1)):0;if(f.t1+8>st)f.t1=0;f.v1on=rnd()&1;
   f.wstride=(rnd()&1)?32:40;
   for(i=0;i<sizeof(wa)/4;i++)wa[i]=wb[i]=0xA5A5A5A5u;
   f.w=wa;vxs_conv_c(&f);f.w=wb;vxs_conv_asm(&f);cases++;
   for(i=0;i<sizeof(wa)/4;i++)if(wa[i]!=wb[i]){out("conv differs, case ");outn(t);out(" word ");outn(i);out("\n");bad++;break;}}
 }
 for(t=0;t<20000;t++){uint32_t n=rnd()%1800,o=rnd()&1,i,lim=(rnd()&1)?0x10000:(1+rnd()%4000),ra,rb; /* vxs_max16: odd addresses, every length */
  for(i=0;i<n;i++){uint32_t a=rnd()%lim;if((rnd()%97)==0)a=0xFFFF-(rnd()&3);ib[o+i*2]=(uint8_t)a;ib[o+i*2+1]=(uint8_t)(a>>8);}
  ra=vxs_max16_c(ib+o,n);rb=vxs_max16_asm(ib+o,n);cases++;if(ra!=rb){out("max16 differs, case ");outn(t);out("\n");bad++;}}
 out("hal span kernels against the C: ");outn(cases);out(" cases, ");outn(bad);out(" differ (");outn(passed);out(" vertices passed the check)\n");
 sys_exit(bad?1:0);
}
