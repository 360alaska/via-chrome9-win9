/* v10.288 (review): a D24S8 Z surface's block (Z words, the gap, the stencil plane) is a multiple of 256 bytes, so the surface DirectDraw
 * packs after it keeps the sampler's 256-byte alignment, and the plane (VX3DN_SPLANE_OFF .. VX3DN_SPLANE_END) and (v10.294) the Coarse Z storage after it fit inside the block */
#include <stdio.h>
#include <stdint.h>
#include "vx_native3d.h"
int main(void) {
 uint32_t w,h,bad=0,n=0;
 for(w=1;w<=2048;w+=(w<64?1:7))for(h=1;h<=2048;h+=(h<64?1:5)) {
  vx3dn_surface s;uint32_t pitch=w*4,blk=VX3DN_SPLANE_BLOCK(pitch,w,h);
  s.offset=0x100000;s.pitch=pitch;s.width=w;s.height=h;s.format=VX3DN_D24S8;n++;
  if((blk&255)||VX3DN_SPLANE_END(&s)>s.offset+blk||VX3DN_CZ_END(&s)>s.offset+blk||VX3DN_CZ_OFF(&s)<VX3DN_SPLANE_END(&s)||(VX3DN_SPLANE_OFF(&s)&255)){if(bad++<8)printf("FAIL %ux%u: block %u, plane %lu..%lu\n",w,h,blk,(unsigned long)(VX3DN_SPLANE_OFF(&s)-s.offset),(unsigned long)(VX3DN_SPLANE_END(&s)-s.offset));}
 }
 printf("%u D24S8 sizes: %u blocks not a multiple of 256 or too small for the plane\n",n,bad);
 {uint32_t b2=0,n2=0; /* v10.295: Z16 blocks held the Coarse Z storage after the Z words. .767: the Z words alone (VX3DN_CZ_FORMAT is D24S8's
  * only: a Z16 draw writes 01/2A 0 and leaves Coarse Z off). .768: this half still asked for the storage and failed on every size (it runs
  * only with tests/run.py --asan). The rule now: a multiple of 256, never smaller than the Z words, never a whole 256 more than they need,
  * and no user of the VX3DN_CZ_ bounds takes a Z16 surface (they all ask VX3DN_CZ_FORMAT first) */
 for(w=1;w<=2048;w+=(w<64?1:7))for(h=1;h<=2048;h+=(h<64?1:5)) {
  uint32_t pitch=((w*2+31)&~31UL),blk=VX3DN_Z16_BLOCK(pitch,w,h);
  n2++;
  if((blk&255)||blk<pitch*h||blk>=pitch*h+256UL){if(b2++<8)printf("FAIL Z16 %ux%u: block %u\n",w,h,blk);}
 }
 if(VX3DN_CZ_FORMAT(VX3DN_D16)){printf("FAIL: VX3DN_CZ_FORMAT takes Z16 (its block holds no Coarse Z storage)\n");b2++;}
 if(!VX3DN_CZ_FORMAT(VX3DN_D24S8)){printf("FAIL: VX3DN_CZ_FORMAT does not take D24S8\n");b2++;}
 printf("%u Z16 sizes: %u blocks not a multiple of 256, smaller than the Z words or larger than they need\n",n2,b2);bad+=b2;}
 return bad!=0;
}
