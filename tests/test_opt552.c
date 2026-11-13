/* Internal numerical regression: the scalar fog optimization must preserve
 * every output bit, including guards for non-finite input. No hardware I/O. */
#include <stdio.h>
#include "../src/vx_d3d_core.c"

static float original_exp(float x) {
 double y,f,r;int i;
 if(!(x>-80.0f))return 0.0f;if(!(x<0.0f))return 1.0f;
 y=(double)x*1.4426950408889634;i=(int)y;if((double)i>y)i--;f=y-i;
 r=1.0+f*(0.6931471805599453+f*(0.2402265069591007+f*(0.0555041086648216+f*(0.0096181291076285+f*(0.0013333558146428+f*0.0001540353039338)))));
 while(i<0){r*=0.5;i++;}
 return (float)r;
}

int main(void) {
 uint32_t k,seed=552,bad=0;float x;
 for(k=0;k<200000;k++) {
  seed=seed*1664525U+1013904223U;
  x=k<100000?-80.0f*(float)k/100000.0f:tn_flt(seed);
  if(tn_bits(tn_exp(x))!=tn_bits(original_exp(x)))bad++;
 }
 printf("scalar fog: 200000 inputs, %u bit differences\n",bad);
 /* A template ID reused after reset must not reuse its old clip bounds.
  * Simulates generation wrap without billions of draws. FVF changes inside
  * one ID must also refresh source offsets. */
 tc.id=552;tc.lay.fvf=0x144;tc.lay.coff=16;tc.bxl=0;vxs_tc();
 template_reset();tc.id=552;tc.bxl=0x3f800000U;vxs_tc();
 if(vxs.xlo!=tc.bxl)bad++;
 tc.lay.fvf=0x184;tc.lay.coff=0;tc.lay.soff=16;vxs_tc();
 if(vxs.coff||vxs.soff!=16)bad++;
 puts("span constants: explicit template reset and in-template FVF change checked");
 return bad!=0;
}
