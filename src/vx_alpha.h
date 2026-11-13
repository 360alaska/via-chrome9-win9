/* Constant source alpha over RGB565/XRGB8888; no framebuffer access or floating point. */
#ifndef VX_ALPHA_H
#define VX_ALPHA_H
#include <stdint.h>
static uint32_t vx_alpha_pixel(uint32_t s,uint32_t d,uint32_t a,uint32_t bpp) {
 uint32_t ia=255-a,r,g,b;
 if(bpp==16){r=(((s>>11)&31)*a+((d>>11)&31)*ia+127)/255;
  g=(((s>>5)&63)*a+((d>>5)&63)*ia+127)/255;b=((s&31)*a+(d&31)*ia+127)/255;return (r<<11)|(g<<5)|b;}
 r=(((s>>16)&255)*a+((d>>16)&255)*ia+127)/255;
 g=(((s>>8)&255)*a+((d>>8)&255)*ia+127)/255;b=((s&255)*a+(d&255)*ia+127)/255;
 return (d&0xFF000000UL)|(r<<16)|(g<<8)|b;
}
#endif
