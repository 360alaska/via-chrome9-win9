/* v10.279 (review): the VxD's cursor converters (src/vxd_mouse_conv.h) against payloads sized as the DRV copies them (dibcall.c
 * mouse_cursor_size: AND mask cbWidth x cy, XOR mask CURSOR_XOR_ROW x cy), every supported depth, every size 1..32 x 1..32, each payload
 * in a buffer of exactly that size (ASan finds a read past it), and every converted pixel checked against the source pixel it came from. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef uint8_t BYTE;typedef uint16_t WORD;typedef uint32_t DWORD;typedef int32_t LONG;typedef int BOOL;
#define TRUE 1
#define FALSE 0
#include "cursor.h"
typedef union {DWORD dw;struct {BYTE b,g,r,x;} c;} pixel_32_t;
#define CURSOR_DMAX 32
static struct {pixel_32_t palette[256];struct {struct {LONG cursor_w,cursor_h;} s;} regs;struct {DWORD andmask[CURSOR_DMAX*CURSOR_DMAX],xormask[CURSOR_DMAX*CURSOR_DMAX];} cursor;} wram_s,*wram=&wram_s; /* the parts the converters use */
#include "vxd_mouse_conv.h"
static DWORD drv_xor_row(DWORD bp,DWORD cx) {return bp==15?cx*2:(bp*cx+7)/8;} /* dibcall.c's rule (the source check below holds it there) */
static DWORD want(int bp,const BYTE *row,int x) {
 DWORD v,r,g,b;
 switch(bp) {
  case 8:return wram->palette[row[x]].dw;
  case 15:v=row[2*x]|row[2*x+1]<<8;r=(v>>10)&31;g=(v>>5)&31;b=v&31;return ((r<<3|r>>2)<<16)|((g<<3|g>>2)<<8)|(b<<3|b>>2);
  case 16:v=row[2*x]|row[2*x+1]<<8;r=(v>>11)&31;g=(v>>5)&63;b=v&31;return ((r<<3|r>>2)<<16)|((g<<2|g>>4)<<8)|(b<<3|b>>2);
  case 24:return row[3*x]|row[3*x+1]<<8|(DWORD)row[3*x+2]<<16;
  default:return row[4*x]|row[4*x+1]<<8|(DWORD)row[4*x+2]<<16|(DWORD)row[4*x+3]<<24;
 }
}
int main(void) {
 static const int bps[]={8,15,16,24,32};int k,cx,cy,x,y,fails=0;long n=0;static DWORD dst[CURSOR_DMAX*CURSOR_DMAX];
 for(k=0;k<256;k++)wram->palette[k].dw=0x010203u*k^0x5A0000u;
 for(k=0;k<5;k++)for(cx=1;cx<=32;cx++)for(cy=1;cy<=32;cy++) {
  CURSORSHAPE cur;DWORD rb=drv_xor_row(bps[k],cx);BYTE *src=malloc(rb*cy);
  if(rb!=CURSOR_XOR_ROW(bps[k],cx)){printf("FAIL bpp %d cx %d: the DRV copies rows of %lu bytes, the VxD reads %lu\n",bps[k],cx,(unsigned long)rb,(unsigned long)CURSOR_XOR_ROW(bps[k],cx));fails++;}
  for(x=0;x<(int)(rb*cy);x++)src[x]=(BYTE)(x*37+cx*5+cy);
  memset(&cur,0,sizeof(cur));cur.cx=(short)cx;cur.cy=(short)cy;cur.cbWidth=(short)((cx+15)/16*2);cur.BitsPixel=(BYTE)bps[k];cur.Planes=1;
  memset(dst,0,sizeof(dst));expandmask(&cur,CURSOR_XOR_ROW(cur.BitsPixel,cur.cx),src,dst,bps[k]);n++;
  for(y=0;y<cy;y++)for(x=0;x<cx;x++)if(dst[y*CURSOR_DMAX+x]!=want(bps[k],src+rb*y,x)){if(fails++<8)printf("FAIL bpp %d %dx%d pixel %d,%d: %08lX, want %08lX\n",bps[k],cx,cy,x,y,(unsigned long)dst[y*CURSOR_DMAX+x],(unsigned long)want(bps[k],src+rb*y,x));}
  free(src);
 }
 (void)cursor_is_empty;
 printf("%ld cursor shapes converted from exactly sized payloads: %d failed\n",n,fails);
 return fails!=0;
}
